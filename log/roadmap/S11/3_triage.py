"""S11.3: triage of every open C2 row -> re/roadmap/S11_c2_triage.tsv, log/roadmap/S11/3_summary.tsv, 3_merge.md.

Offline and read-only (no tracker writes). Deterministic: rows sorted by (module, addr); caches under log/roadmap/S11/3_cache/
(delete a file or pass --refresh to recompute). Open row = scripts/re_classify.py cmd_status's logic: hooks.csv confidence C2,
status not in END_STATES, not in re/c3_hold.tsv.

    ~/bin/py -3.12 log/roadmap/S11/3_triage.py [--refresh]
"""
import csv, json, pathlib, re, sys
from collections import Counter, defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = ROOT / "re" / "roadmap" / "S11_c2_triage.tsv"
CACHE = ROOT / "log" / "roadmap" / "S11" / "3_cache"
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
import pointer_callers as pc  # noqa: E402
import endstate  # noqa: E402
import re_classify as rc  # noqa: E402

EXE = "golf_clean.exe"
BASE = 0x10000000
TAG = {"jgld.dll": "jgld", "sound.dll": "sound", "Terrain.dll": "Terrain"}
MODULE_LEVEL = {"jgld.dll": "gfx backend (PORT_PLAN.md module table: replaces jgld.dll)",
                "Terrain.dll": "terrain / TerrainApi (PORT_PLAN.md module table: replaces Terrain.dll)"}
AUDIO = {"chan": "audio_chan", "codec": "audio_codec", "crt": "audio_crt", "dev": "audio_dev", "ds": "audio_ds",
         "list": "audio_core", "mixer": "audio_mixer", "seq": "audio_seq", "sound": "audio_sound", "wave": "audio_wave"}


def n8(a):
    return f"{int(a, 16):08x}"


def cached(name, fn, refresh):
    CACHE.mkdir(parents=True, exist_ok=True)
    f = CACHE / name
    if f.exists() and not refresh:
        return json.loads(f.read_text())
    v = fn()
    f.write_text(json.dumps(v, sort_keys=True, indent=0))
    return v


def open_rows():
    rows = list(csv.DictReader(open(ROOT / "hooks.csv", newline="")))
    held = set()
    for h in csv.DictReader(open(ROOT / "re" / "c3_hold.tsv", encoding="utf-8"), delimiter="\t"):
        held.add(((h.get("module") or EXE).lower(), n8(h["addr"])))
    out = []
    for r in rows:
        mod = r.get("module") or EXE
        if r["confidence"] == "C2" and r["status"] not in rc.END_STATES and (mod.lower(), n8(r["addr"])) not in held:
            out.append(r)
    out.sort(key=lambda r: (r.get("module") or EXE, n8(r["addr"])))
    return out


def sizes(mod):
    m = pc.mod(mod)
    d = {}
    for r in csv.DictReader(open(ROOT / pc.FILES[mod]), delimiter="\t"):
        e = int(r["entry"], 16)
        e = e if mod == EXE or e >= BASE else e + m.base
        d[e] = int(r["size"])
    return d


def entry_points():
    """{(module, addr8)} the PE loader calls: AddressOfEntryPoint (+ TLS callbacks). endstate.py's dead checks do not
    cover these (U-2020)."""
    import pefile
    out = set()
    for mod in pc.FILES:
        pe = pefile.PE(str(ROOT / "original" / mod), fast_load=True)
        out.add((mod, f"{pe.OPTIONAL_HEADER.AddressOfEntryPoint:08x}" if mod != EXE
                 else f"{pe.OPTIONAL_HEADER.ImageBase + pe.OPTIONAL_HEADER.AddressOfEntryPoint:08x}"))
    return out


def endstate_row(mod, addr, callers):
    dead_ok, dchecks = endstate.run(mod, addr, "dead")
    triv_ok, tchecks = endstate.run(mod, addr, "trivial")
    fd = [f"{n}" + (f" ({d})" if d else "") for n, ok, d in dchecks if not ok]
    ft = [f"{n}" + (f" ({d})" if d else "") for n, ok, d in tchecks if not ok]
    if dead_ok and callers == "none":
        v = "dead"
    elif triv_ok:
        v = "trivial"
    else:
        v = "none"
    return {"verdict": v, "dead_ok": dead_ok, "trivial_ok": triv_ok, "dead_fail": fd, "trivial_fail": ft}


def pointer_row(mod, addr):
    m = pc.mod(mod)
    refs = pc.references(m, pc.norm(m, addr))
    inst = sorted({i for *_, ii in refs for i in ii})
    kinds = sorted({k for _, k, _, _ in refs})
    detail = "; ".join(f"{k} 0x{r:08x}" + (f" [{d}]" if k == "table" else "") for r, k, d, _ in refs[:3])
    return {"nrefs": len(refs), "kinds": kinds, "installers": [f"0x{i:08x} {m.entries.get(i, '?')}" for i in inst][:6],
            "n_installers": len(inst), "detail": detail}


def import_callers():
    import import_callers as ic
    out = {}
    for rva, name, slot, ss in ic.table():
        if rva is None:
            continue
        cs = sorted({e for *_, e, _ in ss if e is not None})
        out[f"{rva:08x}"] = [f"0x{c:08x}" for c in cs]
    return out


def match_index():
    """{(module, addr8): [(file, kind)]} from // MATCH lines; kind ours/smsg, dir main/wip. DLL VAs become RVAs."""
    idx = defaultdict(list)
    pat = re.compile(r"//\s*MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)")
    for sub, d in (("main", ROOT / "re" / "match"), ("wip", ROOT / "re" / "match" / "wip")):
        for f in sorted(d.glob("*.cpp")):
            kind = "smsg" if f.name.startswith("smsg_") else "ours"
            for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
                mm = pat.search(line)
                if mm:
                    mod, a = mm.group(1), int(mm.group(2), 16)
                    if mod != EXE and a >= BASE:
                        a -= BASE
                    idx[(mod, f"{a:08x}")].append([sub, kind, f.name])
    return idx


def _unused_match_csvs():
    """{(tag, addr8): True if every row of every csv is True}; tag '' for the exe (name not starting with a module tag)."""
    out = {}
    tags = {"jgld_": "jgld", "sound_": "sound", "Terrain_": "Terrain"}
    for f in sorted((ROOT / "log" / "diff").glob("*.match.csv")):
        a, _, rest = f.name.partition("_")
        if len(a) != 8:
            continue
        tag = next((t for p, t in tags.items() if rest.startswith(p)), "")
        try:
            rows = list(csv.DictReader(open(f, newline="")))
        except OSError:
            continue
        green = bool(rows) and all(r.get("match") == "True" for r in rows)
        k = (tag, a)
        out[k] = out.get(k, True) and green
    return out


def main():
    refresh = "--refresh" in sys.argv
    rows = open_rows()
    print(f"open rows: {len(rows)}")
    sz = {mod: sizes(mod) for mod in pc.FILES}
    scope = {r["addr"]: r for r in csv.DictReader(open(ROOT / "re" / "port" / "scope.tsv"), delimiter="\t")}
    s4 = {n8(r["rva"]): r for r in csv.DictReader(open(ROOT / "re" / "roadmap" / "S4_functions.tsv"), delimiter="\t")
          if r["rva"] not in ("rva", "-")}
    imp = cached("import_callers.json", import_callers, refresh)
    midx = match_index()
    es_cache = cached("endstate.json", dict, refresh)
    pt_cache = cached("pointer.json", dict, refresh)
    out, dirty = [], False
    eps = entry_points()
    for n, r in enumerate(rows):
        mod, a = r.get("module") or EXE, n8(r["addr"])
        key = f"{mod}|{a}"
        callers = (r.get("callers") or "none").strip() or "none"
        if key not in es_cache:
            es_cache[key] = endstate_row(mod, a, callers); dirty = True
        if key not in pt_cache:
            pt_cache[key] = pointer_row(mod, a); dirty = True
        if n % 200 == 0:
            print(f"  {n}/{len(rows)}")
        es, pt = dict(es_cache[key]), pt_cache[key]
        is_entry = (mod, a) in eps
        if is_entry and es["verdict"] == "dead":
            es["verdict"] = "none"
        m = pc.mod(mod)
        va = pc.norm(m, a)
        size = sz[mod].get(va, "")
        imp_callers = imp.get(a, []) if mod == "Terrain.dll" else []
        callers_all = callers if callers != "none" else ("imports:" + ";".join(imp_callers) if imp_callers else "none")
        if callers_all != "none":
            reach = "called"
        elif pt["n_installers"]:
            reach = "pointer-installed"
        elif pt["nrefs"]:
            reach = "pointer-no-installer"
        else:
            reach = "no-caller"
        if mod == EXE:
            sc = scope.get(a)
            port = (sc["port_module"] + (" straddler" if sc.get("straddler") == "1" else "")) if sc else "not-in-scope.tsv"
            port_src = "re/port/scope.tsv" + (f" ({sc['reach_reason']})" if sc else "")
        elif mod == "sound.dll":
            s = s4.get(a)
            fam = (s["family"].rstrip("~") if s else "")
            port = f"{AUDIO.get(fam, 'audio_?')} (family {fam}, S4 status {s['status']})" if s else "none-stated (not in S4_functions.tsv)"
            port_src = "re/roadmap/S4_functions.tsv"
        else:
            port, port_src = "none-stated", "function-level: no file; module-level: " + MODULE_LEVEL[mod]
        ms = midx.get((mod, a), [])
        main_ms = [x for x in ms if x[0] == "main"]
        wip_ms = [x for x in ms if x[0] == "wip"]
        tag = TAG.get(mod, "")
        if main_ms:
            match = "100% ours" if any(x[1] == "ours" for x in main_ms) else "100% smsg"
            mfile = ";".join(sorted({x[2] for x in main_ms}))
        elif wip_ms:
            match, mfile = "wip", ";".join(sorted({x[2] for x in wip_ms}))
        else:
            match, mfile = "none", ""
        notes = []
        if r["status"] != "transcribed":
            notes.append(f"hooks status {r['status']!r}: mark refuses non-'transcribed'")
        if es["verdict"] == "dead" and callers != "none":
            notes.append("dead check passes but hooks.csv records callers")
        if is_entry:
            notes.append("PE entry point (called by the loader): endstate.py dead checks pass but the row is not dead (U-2020)")
        if es["dead_ok"] and callers != "none":
            notes.append("endstate dead checks pass although hooks.csv callers non-empty")
        out.append({
            "module": mod, "addr": a, "name": r["name"], "size": size, "subsystem": r["subsystem"], "hooks_status": r["status"],
            "endstate": es["verdict"],
            "endstate_dead_fail": " | ".join(es["dead_fail"]), "endstate_trivial_fail": " | ".join(es["trivial_fail"]),
            "callers": callers, "import_callers": ";".join(imp_callers), "installers": "; ".join(pt["installers"]),
            "pointer_refs": f"{pt['nrefs']} ({','.join(pt['kinds'])})" if pt["nrefs"] else "0",
            "reach": reach, "port_module": port, "port_source": port_src,
            "match": match, "match_file": mfile, "notes": "; ".join(notes)})
    if dirty:
        (CACHE / "endstate.json").write_text(json.dumps(es_cache, sort_keys=True, indent=0))
        (CACHE / "pointer.json").write_text(json.dumps(pt_cache, sort_keys=True, indent=0))
    cols = list(out[0])
    with open(OUT, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols, delimiter="\t", lineterminator="\n")
        w.writeheader(); w.writerows(out)
    # summary
    sumf = ROOT / "log" / "roadmap" / "S11" / "3_summary.tsv"
    with open(sumf, "w", encoding="utf-8", newline="") as f:
        f.write("column\tmodule\tvalue\tcount\n")
        for col in ("endstate", "reach", "match", "hooks_status"):
            c = Counter((r["module"], r[col]) for r in out)
            for (mod, v), k in sorted(c.items()):
                f.write(f"{col}\t{mod}\t{v}\t{k}\n")
        c = Counter((r["module"], r["port_module"].split(" ")[0]) for r in out)
        for (mod, v), k in sorted(c.items()):
            f.write(f"port_module\t{mod}\t{v}\t{k}\n")
        c = Counter((r["module"], r["reach"], r["match"].split(" ")[0]) for r in out)
        for (mod, a_, b_), k in sorted(c.items()):
            f.write(f"reach+match\t{mod}\t{a_}+{b_}\t{k}\n")
    # mark commands
    mk = defaultdict(list)
    for r in out:
        if r["endstate"] in ("dead", "trivial") and r["hooks_status"] == "transcribed":
            mk[(r["module"], r["endstate"])].append(r["addr"])
    lines = []
    for (mod, st), addrs in sorted(mk.items()):
        modarg = "" if mod == EXE else f" --module {mod}"
        for i in range(0, len(addrs), 40):
            lines.append(f"~/bin/py -3.12 scripts/re_classify.py mark {' '.join(addrs[i:i + 40])} --status {st}{modarg}")
    (ROOT / "log" / "roadmap" / "S11" / "3_mark_commands.txt").write_text("\n".join(lines) + "\n")
    print(f"wrote {OUT} ({len(out)} rows); {len(lines)} mark command lines")


if __name__ == "__main__":
    main()
