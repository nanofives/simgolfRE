"""SMSG-decomp sources -> re/match wrapper files (roadmap S10.1). One command per SMSG source file:

    ~/bin/py -3.12 re/tools/smsg_import.py file terrain/Terrain.dll/src/Tile.cpp [--evidence]
    ~/bin/py -3.12 re/tools/smsg_import.py project golf/jackal.lib/jackal.dsp [--evidence]   # every source of a .dsp
    ~/bin/py -3.12 re/tools/smsg_import.py inventory golf      # log/roadmap/S10/inventory_golf.tsv
    ~/bin/py -3.12 re/tools/smsg_import.py calibrate Terrain   # our 100% addresses against SMSG's result
    ~/bin/py -3.12 re/tools/smsg_import.py list                # projects and their sources

Paths are relative to third_party/smsg/SMSG-decomp. For one source file the tool
 1. reads FLAGS from its .dsp (`# ADD CPP` of the project plus the file's own `# ADD CPP` / `# SUBTRACT CPP`) and the
    include path (/I of the .dsp, then the INCLUDE that SMSG-decomp's build.bat sets for that project);
 2. writes an annotation-only wrapper whose `// COMPILE` line names the source (match.py compiles the source itself:
    under /ZI an #included file's __LINE__ is a constant, not the line variable), compiles it once with VC6 and takes one MATCH line per PDB procedure of
    that object file (third_party/smsg/release/<pdb>.pdb; their sources carry no addresses). The decorated name is the
    PDB public at that RVA when the obj defines it, else the obj function whose undecorated name equals the procedure's;
 3. scores every line with re/tools/match.py's comparison (no evidence) and splits them:
    re/match/smsg_<pdb>_<stem>.cpp   100% lines for addresses no other re/match/*.cpp file has (canonical; the match
                                     test tier checks it); addresses we already match at 100% are listed as comments;
    re/match/wip/smsg_<pdb>_<stem>.cpp  every other line with its score (and procedures with no obj symbol);
 4. writes log/roadmap/S10/runs/<pdb>_<stem>.tsv (one row per procedure) and appends a line to
    log/roadmap/S10/runs/summary.tsv.
`--evidence` then runs match.py on the canonical file and writes log/diff CSVs (all GREEN by construction; never pass
it for exploration: a RED CSV breaks the C4 gate of a row already at C4).
`--toolchain` lines: sound.dll's own sources were built by VC6 SP5 (smsg.toml profile vc6-sp5); match.py compiles with
the SP5 tree through the wrapper's `// TOOLCHAIN sp5` line (match.py's selector, roadmap S10.5).
"""
from __future__ import annotations

import argparse
import csv
import ctypes
import pathlib
import re
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402
import smsg_pdb  # noqa: E402

SMSG = ROOT / "third_party" / "smsg" / "SMSG-decomp"
SMSG_REL = "third_party/smsg/SMSG-decomp"
RUNS = ROOT / "log" / "roadmap" / "S10" / "runs"
DX = "tools/dxsdk8/include"
_S = SMSG_REL + "/"
GOLF_ENV = [DX, "$VC", _S + "golf/jpeg.lib/jpeg-6a", _S + "terrain/Terrain.dll/include", _S + "sound/sound.dll/include",
            _S + "sound/lpc10.lib/lpc10/lpc10-1.5/lpc55-C", _S + "jgl/jgld.dll/include", _S + "golf/binkw32.lib"]
# .dsp -> (PDB key, INCLUDE that SMSG-decomp/build.bat sets before MSDEV builds it, their toolchain profile).
# build.bat: :terrain `set INCLUDE=%VC6%\\VC98\\INCLUDE;...MFC` (line 79); jgld `...;libpng;zlib` (85); lpc10 VC98 only
# (93); sound `DXSDK;VC98;MFC;DXSDK;I_LPC10` (96) under VC6SP5 (98); golf (103). MFC's include is absent from tools/vc6.
PROJECTS = {
    "terrain/Terrain.dll/Terrain.dsp": ("Terrain", ["$VC"], "8168"),
    "jgl/zlib.lib/zlib.dsp": ("jgld", ["$VC", _S + "jgl/libpng.lib/libpng", _S + "jgl/zlib.lib/zlib"], "8168"),
    "jgl/libpng.lib/libpng.dsp": ("jgld", ["$VC", _S + "jgl/libpng.lib/libpng", _S + "jgl/zlib.lib/zlib"], "8168"),
    "jgl/jgld.dll/jgld.dsp": ("jgld", ["$VC", _S + "jgl/libpng.lib/libpng", _S + "jgl/zlib.lib/zlib"], "8168"),
    "sound/lpc10.lib/lpc10.dsp": ("sound", ["$VC"], "8168"),
    "sound/sound.dll/sound.dsp": ("sound", [DX, "$VC", _S + "sound/lpc10.lib/lpc10/lpc10-1.5/lpc55-C"], "sp5"),
    "golf/golf.exe/golf.dsp": ("golf", GOLF_ENV, "8168"),
    "golf/jackal.lib/jackal.dsp": ("golf", GOLF_ENV, "8168"),
    "golf/jpeg.lib/jpeg.dsp": ("golf", GOLF_ENV, "8168"),
}
# cl options that do not change the code of a function, or that match.py sets itself (/c /Fo /Fd)
DROP = {"/nologo", "/c", "/Gm", "/YX", "/FD", "/FR", "/Fr"}
DROP_PREFIX = ("/W", "/Fp", "/Yu", "/Yc", "/Fo", "/Fd", "/FR", "/Fr")


def _tokens(s: str) -> list[str]:
    """cl tokens of a .dsp line; `/D "X"` and `/I "dir"` become one token each (`/DX`, `/Idir`)."""
    raw = [t.strip('"') for t in re.findall(r'"[^"]*"|\S+', s)]
    out, i = [], 0
    while i < len(raw):
        t = raw[i]
        if t in ("/D", "/U", "/I") and i + 1 < len(raw):
            t, i = t + raw[i + 1], i + 1
        out.append(t)
        i += 1
    return out


def parse_dsp(dsp: str) -> tuple[list[str], dict[str, list[tuple[str, list[str]]]]]:
    """(project cl tokens, {source path relative to SMSG-decomp: [('+'|'-', tokens), ...]}) of a one-config .dsp."""
    path = SMSG / dsp
    base: list[str] = []
    per: dict[str, list] = {}
    cur, target = None, False
    for ln in path.read_text(errors="replace").splitlines():
        if ln.startswith("# Begin Target"):
            target = True
        elif ln.startswith("SOURCE="):
            rel = (path.parent / ln[7:].strip().strip('"').replace("\\", "/")).resolve()
            cur = rel.relative_to(SMSG.resolve()).as_posix()
            per[cur] = []
        elif ln.startswith("# End Source File"):
            cur = None
        elif ln.startswith(("# ADD CPP ", "# SUBTRACT CPP ")):
            toks = _tokens(ln.split(" CPP ", 1)[1])
            if not target:
                base = toks
            elif cur is not None:
                per[cur].append(("-" if ln.startswith("# SUBTRACT") else "+", toks))
    return base, per


def source_config(dsp: str, src: str) -> tuple[str, list[str]]:
    """(cl flags for match.py, INCLUDE lines in search order) of one source of a project."""
    base, per = parse_dsp(dsp)
    toks = list(base)
    for op, ts in per.get(src, []):
        toks = toks + ts if op == "+" else [t for t in toks if t not in ts]
    flags = [t for t in toks if t not in DROP and not t.startswith(DROP_PREFIX) and not t.startswith("/I")]
    dspdir = (SMSG / dsp).parent
    incs = [(dspdir / t[2:].replace("\\", "/")).resolve().relative_to(ROOT.resolve()).as_posix()
            for t in toks if t.startswith("/I")]
    return " ".join(flags), incs + PROJECTS[dsp][1]


def sources(dsp: str) -> list[str]:
    return [s for s in parse_dsp(dsp)[1] if s.lower().endswith((".c", ".cpp"))]


def project_of(src: str) -> str:
    hits = [d for d in PROJECTS if src in parse_dsp(d)[1]]
    if len(hits) != 1:
        raise SystemExit(f"{src}: in {len(hits)} projects of PROJECTS ({hits})")
    return hits[0]


def pdb_procs(dsp: str, src: str) -> list[dict]:
    """PDB procedures of the object file built from `src` (a library's member objs are told apart by the library)."""
    pdb = smsg_pdb.load(PROJECTS[dsp][0])
    obj = pathlib.PurePosixPath(src).stem + ".obj"
    lib = pathlib.PurePosixPath(dsp).stem.lower() + ".lib" if dsp.split("/")[1].endswith(".lib") else None
    return [p for p in pdb.procs_of(obj)
            if (p["lib"].lower().endswith("\\" + lib) if lib else p["lib"] == p["module"])]


_UND = None


def undname(sym: str) -> str:
    """Name-only undecoration (dbghelp UNDNAME_NAME_ONLY) with `class ` / `struct ` / spaces dropped, so that it
    compares with a PDB procedure name; C symbols lose their `_` and stdcall `@N`."""
    global _UND
    if not sym.startswith("?"):
        return re.sub(r"@\d+$", "", sym[1:] if sym.startswith("_") else sym)
    if _UND is None:
        _UND = ctypes.windll.dbghelp.UnDecorateSymbolName
    buf = ctypes.create_string_buffer(2048)
    n = _UND(sym.encode("latin-1"), buf, 2048, 0x1000)
    return norm(buf.value[:n].decode("latin-1")) if n else sym


def norm(name: str) -> str:
    return re.sub(r"\b(class|struct|union|enum) ", "", name).replace(" ", "")


def stem_of(dsp: str, src: str) -> str:
    """File stem for re/match: PDB key + project + source stem (`Terrain_Tile`, `golf_jackal_sprite`)."""
    proj = pathlib.PurePosixPath(dsp).stem
    key = PROJECTS[dsp][0]
    mid = "" if proj.lower() == key.lower() or dsp.startswith(("golf/golf.exe", "jgl/jgld.dll", "sound/sound.dll")) \
        else proj.lower() + "_"
    return f"{key}_{mid}{re.sub(r'[^A-Za-z0-9_]', '_', pathlib.PurePosixPath(src).stem)}"


def header(dsp: str, src: str, depth: int) -> str:
    """Wrapper text before the MATCH lines (`depth` is unused since the wrapper names the source by `// COMPILE`)."""
    key, _, tool = PROJECTS[dsp]
    flags, incs = source_config(dsp, src)
    lines = [f"// SMSG-decomp source {SMSG_REL}/{src} (project {dsp}), generated by re/tools/smsg_import.py.",
             "// Their sources carry no addresses: each annotation below is a procedure of this object file in",
             f"// third_party/smsg/release/{key}.pdb, scored by re/tools/match.py against our anchored binary.",
             f"// FLAGS {smsg_pdb.OUR_MODULE[key]}: {flags}"]
    lines += [f"// INCLUDE {d}" for d in incs]
    if tool != "8168":
        lines.append(f"// TOOLCHAIN {tool}")
    if src.lower().endswith(".c"):
        lines.append("// LANG c")
    lines.append(f"// COMPILE {SMSG_REL}/{src}")
    return "\n".join(lines) + "\n\n"


def map_symbols(procs: list[dict], key: str, syms: list[dict]) -> list[tuple[dict, list[str], str]]:
    """[(proc, candidate decorated obj symbols, how)] with how = public | name | overload | nosym. PDB procedure names
    carry no parameter types, so overloads (ClassList<T>::fill_entry(LinkListEntry *) and the virtual fill_entry(T *))
    give several candidates: score_rows keeps the best-scoring one."""
    funcs = [s["name"] for s in syms if s["sec"] > 0 and s["type"] == 0x20]
    have = set(funcs)
    pubs = smsg_pdb.load(key).public_names()
    out, used, rest = {}, set(), []
    for i, p in enumerate(procs):
        hit = [n for n in pubs.get(p["rva"], []) if n in have and n not in used]
        named = [n for n in hit if undname(n) == norm(p["name"])]
        if hit:
            out[i] = ([(named or hit)[0]], "public")
            used.add(out[i][0][0])
        else:
            rest.append(i)
    by_name: dict[str, list[str]] = {}
    for n in funcs:
        if n not in used:
            by_name.setdefault(undname(n), []).append(n)
    groups: dict[str, list[int]] = {}
    for i in rest:
        groups.setdefault(norm(procs[i]["name"]), []).append(i)
    for nm, idx in groups.items():
        cands = by_name.get(nm, [])
        for i in idx:
            out[i] = (cands, "name" if len(cands) == 1 else "overload") if cands else ([], "nosym")
    return [(p, *out[i]) for i, p in enumerate(procs)]


def our_matches(smsg_before: str = "") -> dict[tuple[str, int], list[str]]:
    """(module, VA) -> re/match/*.cpp files (not wip) holding a MATCH line for it (all 100%). smsg_ files are left out
    unless `smsg_before` names one: then those whose file name sorts before it count too (smsg_dedupe.py's rule: the first
    smsg file by name holds an ICF-folded address, so a run needs no dedupe pass afterwards)."""
    idx: dict[tuple[str, int], list[str]] = {}
    for f in sorted((ROOT / "re" / "match").glob("*.cpp")):
        if f.name.startswith("smsg_") and not (smsg_before and f.name < smsg_before):
            continue
        for mod, addr, _ in re.findall(r"//\s*MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)\s+(\S+)", f.read_text(errors="replace")):
            idx.setdefault((mod, int(addr, 16)), []).append(f.name)
    return idx


def score_rows(src: str, dsp: str, wip: pathlib.Path) -> list[dict]:
    """Compile the wip wrapper once and score every PDB procedure of the object file (no evidence written)."""
    key = PROJECTS[dsp][0]
    mod, base = smsg_pdb.OUR_MODULE[key], smsg_pdb.IMAGE_BASE[key]
    text = wip.read_text()
    lang = "c" if src.lower().endswith(".c") else "c++"
    procs = pdb_procs(dsp, src)
    try:
        secs, syms = match.coff(match.compile_obj(match.compiled_file(wip, text), source_config(dsp, src)[0], lang,
                                                  match.include_path(text), match.file_toolchain(text)))
    except SystemExit as e:
        err = str(e).splitlines()
        msg = next((x for x in err if " error " in x), err[-1] if err else "compile failed")
        return [dict(p, va=base + p["rva"], symbol="", how="-", status="compile_error", score="",
                     note=msg.strip()[:200]) for p in procs]
    rows = []
    for p, cands, how in map_symbols(procs, key, syms):
        r = dict(p, va=base + p["rva"], symbol="", how=how, status="nosym", score="", note="")
        best = None
        for sym in cands:
            try:
                acc, a, b, _, same, table_bad, _ = match.score_function(mod, r["va"], secs, syms, sym)
            except SystemExit as e:
                best = best or (-1.0, sym, str(e).splitlines()[0][:200])
                continue
            note = f"{same}/{max(len(a), len(b))} ins" + (" switch table differs" if table_bad else "")
            if best is None or acc > best[0]:
                best = (acc, sym, note + (f" (best of {len(cands)} overloads)" if len(cands) > 1 else ""))
        if best is not None:
            acc, r["symbol"], r["note"] = best
            r["status"] = "error" if acc < 0 else "100" if acc == 1 else "miss"
            r["score"] = "" if acc < 0 else f"{100 * acc:.1f}"
        rows.append(r)
    return rows


def match_line(mod: str, r: dict) -> str:
    return f"// MATCH: {mod} 0x{r['va']:08x} {r['symbol']}"


def write_outputs(src: str, dsp: str, rows: list[dict], evidence: bool) -> dict:
    key = PROJECTS[dsp][0]
    mod = smsg_pdb.OUR_MODULE[key]
    stem = stem_of(dsp, src)
    canon = ROOT / "re" / "match" / f"smsg_{stem}.cpp"
    wip = ROOT / "re" / "match" / "wip" / f"smsg_{stem}.cpp"
    ours = our_matches(canon.name)
    if re.search(r"__declspec\s*\(\s*naked", (SMSG / src).read_text(errors="replace")):
        for r in rows:            # re/C4_PLAN.md: a naked asm transcription is not match evidence; sort out by hand
            if r["status"] == "100":
                r["status"], r["note"] = "naked_file", "source has __declspec(naked): " + r["note"]
    va100 = {r["va"] for r in rows if r["status"] == "100"}
    for r in rows:
        r["already"] = ",".join(ours.get((mod, r["va"]), []))
        if r["status"] != "100" and r["va"] in va100:     # ICF: another procedure at this address matched
            r["note"] = f"{r['status']} {r['score']} {r['note']}".strip()
            r["status"] = "folded"
    new, seen = [], set()
    for r in rows:                     # one MATCH per address: ICF-folded procedures share it
        if r["status"] == "100" and not r["already"]:
            if r["va"] in seen:
                r["already"] = "folded"
            else:
                seen.add(r["va"])
                new.append(r)
    calib = [r for r in rows if r["status"] == "100" and r["already"]]
    rest = [r for r in rows if r["status"] not in ("100", "folded")]
    if new:
        body = [match_line(mod, r) + f"   // {undname(r['symbol'])}" for r in new]
        body += [f"// ALSO 100%, already in {r['already']}: {mod} 0x{r['va']:08x} {r['symbol']}" for r in calib]
        canon.write_text(header(dsp, src, 2) + "\n".join(body) + "\n")
    elif canon.exists():
        canon.unlink()
    wbody = []
    for r in rest:
        if r["symbol"] and r["status"] != "error":
            wbody.append(match_line(mod, r) + f"   // {r['status']} {r['score']}% {r['note']} ({r['name']})")
        else:     # no score (error: e.g. no Ghidra size, so no function entry yet): a comment, never a MATCH line
            sym = f" symbol={r['symbol']}" if r["symbol"] else ""
            wbody.append(f"// SMSG-{r['status'].upper()}: {mod} 0x{r['va']:08x} {r['name']}{sym}  {r['note']}")
    if wbody:
        wip.write_text(header(dsp, src, 3) + "\n".join(wbody) + "\n")
    elif wip.exists():
        wip.unlink()
    verified = ""
    if new:
        res = match.compare(canon, "", False, quiet=True)
        verified = f"{sum(1 for x in res if x[4] == 1)}/{len(res)}"
        if evidence and all(x[4] == 1 for x in res):
            match.write_evidence(res)
    return {"canon": canon if new else None, "wip": wip if wbody else None, "new": len(new), "calib": len(calib),
            "verified": verified}


RUN_COLS = ["pdb", "source", "rva", "va", "size", "kind", "pdb_name", "symbol", "how", "status", "score", "note", "already"]
SUM_COLS = ["when", "pdb", "source", "procs", "mapped", "new_100", "already_100", "miss", "nosym", "folded", "error",
            "verified", "canonical", "wip", "seconds"]


def run_file(src: str, evidence: bool = False) -> dict:
    t0 = time.time()
    src = src.replace("\\", "/")
    dsp = project_of(src)
    key = PROJECTS[dsp][0]
    stem = stem_of(dsp, src)
    wip = ROOT / "re" / "match" / "wip" / f"smsg_{stem}.cpp"
    wip.write_text(header(dsp, src, 3))
    rows = score_rows(src, dsp, wip)
    out = write_outputs(src, dsp, rows, evidence)
    RUNS.mkdir(parents=True, exist_ok=True)
    with (RUNS / f"{stem}.tsv").open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(RUN_COLS)
        for r in rows:
            w.writerow([key, src, f"0x{r['rva']:x}", f"0x{r['va']:08x}", r["size"], r["kind"], r["name"], r["symbol"],
                        r["how"], r["status"], r["score"], r["note"], r["already"]])
    st = [r["status"] for r in rows]
    rel = lambda p: p.relative_to(ROOT).as_posix() if p else ""  # noqa: E731
    summ = {"when": time.strftime("%Y-%m-%d %H:%M"), "pdb": key, "source": src, "procs": len(rows),
            "mapped": sum(1 for r in rows if r["symbol"]), "new_100": out["new"], "already_100": out["calib"],
            "miss": st.count("miss"), "nosym": st.count("nosym"), "folded": st.count("folded"),
            "error": st.count("error") + st.count("compile_error"), "verified": out["verified"],
            "canonical": rel(out["canon"]), "wip": rel(out["wip"]), "seconds": f"{time.time() - t0:.1f}"}
    sp = RUNS / "summary.tsv"
    new_file = not sp.exists()
    with sp.open("a", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        if new_file:
            w.writerow(SUM_COLS)
        w.writerow([summ[c] for c in SUM_COLS])
    print("\t".join(f"{c}={summ[c]}" for c in SUM_COLS[1:]), flush=True)
    return summ


def run_rows(key: str) -> dict[tuple[int, str], dict]:
    """(VA, PDB procedure name) -> its run row (log/roadmap/S10/runs/<stem>.tsv) for one PDB key; (VA, "") -> the best
    row at that address (100 before folded before the rest)."""
    out: dict[tuple[int, str], dict] = {}
    rank = {"100": 0, "folded": 1}
    for f in sorted(RUNS.glob(f"{key}_*.tsv")):
        with f.open(encoding="utf-8") as fh:
            for r in csv.DictReader(fh, delimiter="\t"):
                va = int(r["va"], 16)
                out[(va, r["pdb_name"])] = r
                old = out.get((va, ""))
                if old is None or rank.get(r["status"], 2) < rank.get(old["status"], 2):
                    out[(va, "")] = r
    return out


def hooks_rows(mod: str) -> dict[int, dict]:
    """hooks.csv rows of one module by VA (DLL rows are keyed by RVA there)."""
    base = 0 if mod == "golf_clean.exe" else 0x10000000
    with (ROOT / "hooks.csv").open(encoding="utf-8") as fh:
        return {int(r["addr"], 16) + base: r for r in csv.DictReader(fh) if r["module"] == mod}


def source_map(key: str) -> dict[int, str]:
    out = {}
    for dsp in PROJECTS:
        if PROJECTS[dsp][0] == key:
            for s in sources(dsp):
                out.update({p["rva"]: s for p in pdb_procs(dsp, s)})
    return out


INV_COLS = ["rva", "va", "size", "kind", "pdb_name", "module", "lib", "smsg_source", "our_match", "hooks_name",
            "hooks_level", "c4_candidate", "smsg_status", "smsg_score", "smsg_symbol"]


def inventory(key: str) -> None:
    """log/roadmap/S10/inventory_<key>.tsv: every PDB procedure with our MATCH, hooks.csv level and the SMSG run result.
    c4_candidate = yes for a C3 row we do not match yet (a 100% match would lift it to C4)."""
    mod, base = smsg_pdb.OUR_MODULE[key], smsg_pdb.IMAGE_BASE[key]
    ours, hooks, runs, srcs = our_matches(), hooks_rows(mod), run_rows(key), source_map(key)
    path = ROOT / "log" / "roadmap" / "S10" / f"inventory_{key}.tsv"
    counts: dict[str, int] = {}
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(INV_COLS)
        for p in smsg_pdb.load(key).procs:
            if p["rva"] is None:
                continue
            va = base + p["rva"]
            h, r = hooks.get(va, {}), runs.get((va, p["name"]), {})
            om = ",".join(ours.get((mod, va), []))
            cand = "yes" if h.get("confidence") == "C3" and not om else "no"
            w.writerow([f"0x{p['rva']:x}", f"0x{va:08x}", p["size"], p["kind"], p["name"], p["module"], p["lib"],
                        srcs.get(p["rva"], ""), om, h.get("name", ""), h.get("confidence", ""), cand,
                        r.get("status", ""), r.get("score", ""), r.get("symbol", "")])
            for k in ("procs", "in_smsg_source" if p["rva"] in srcs else "lib_or_other", "our_match" if om else "",
                      "c4_candidate" if cand == "yes" else "", "smsg_" + r["status"] if r else ""):
                if k:
                    counts[k] = counts.get(k, 0) + 1
    print(path.relative_to(ROOT).as_posix(), counts)


def calibrate(key: str) -> None:
    """log/roadmap/S10/1_calibration_<key>.tsv: every address our re/match has at 100% against SMSG's result."""
    mod = smsg_pdb.OUR_MODULE[key]
    runs, procs = run_rows(key), {smsg_pdb.IMAGE_BASE[key] + p["rva"]: p for p in smsg_pdb.load(key).procs if p["rva"]}
    path = ROOT / "log" / "roadmap" / "S10" / f"1_calibration_{key}.tsv"
    counts: dict[str, int] = {}
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["va", "our_files", "pdb_name", "pdb_obj", "smsg_source", "smsg_status", "smsg_score", "smsg_symbol",
                    "note"])
        for (m, va), files in sorted(our_matches().items()):
            if m != mod:
                continue
            p, r = procs.get(va), runs.get((va, ""))
            st = r["status"] if r else ("no_pdb_proc" if not p else "not_in_their_sources")
            counts[st] = counts.get(st, 0) + 1
            w.writerow([f"0x{va:08x}", ",".join(files), p["name"] if p else "", p["lib"] if p else "",
                        r["source"] if r else "", st, r["score"] if r else "", r["symbol"] if r else "",
                        r["note"] if r else ""])
    print(path.relative_to(ROOT).as_posix(), counts)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["file", "project", "inventory", "calibrate", "list"])
    ap.add_argument("arg", nargs="?", default="")
    ap.add_argument("--evidence", action="store_true", help="write log/diff CSVs for the canonical (100%%) lines")
    a = ap.parse_args(argv)
    if a.cmd == "file":
        run_file(a.arg, a.evidence)
    elif a.cmd == "project":
        for s in sources(a.arg):
            run_file(s, a.evidence)
    elif a.cmd == "list":
        for d in PROJECTS:
            print(d, PROJECTS[d][0], *sources(d), sep="\n  ")
    elif a.cmd == "inventory":
        inventory(a.arg)
    else:
        calibrate(a.arg)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
