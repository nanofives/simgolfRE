"""Verifier step of a parallel C3 batch (CLAUDE.md, parallel C3): after `shim\\build.bat` (with the batch listed in
shim/re_batches.txt) and `re/frida/diff_hook.py <its registry keys>`, report each function of the batch and, with
--apply, promote the ones that are ready.

    py -3.12 re/tools/c3_verify.py c3b              # report: per address, every registry key's verdict and spread
    py -3.12 re/tools/c3_verify.py c3b --keys       # print the batch's registry keys (for diff_hook)
    py -3.12 re/tools/c3_verify.py c3b --apply      # insert the Purposes and promote the ready functions

Ready = every registry key of the address has a GREEN path-1 CSV and at least one key shows more than one distinct
result or a state change (a single-valued GREEN is not evidence). The Purpose comes from the `**Purpose:**` paragraph
of the address's section in log/c3/<ID>_purpose.md; the A/B facts (keys, vectors, distinct results, CSVs) are appended
by this tool from the CSVs, never taken from the agent's text."""
import argparse, collections, csv, importlib.util, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def keys_of(batch):
    spec = importlib.util.spec_from_file_location("frag", ROOT / "re" / "frida" / "registry.d" / f"{batch}.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m.HOOKS


def csv_facts(spec, key):
    sys.path.insert(0, str(ROOT / "re" / "frida"))
    from diff_hook import csv_path
    p = csv_path(spec, key, ROOT / "log" / "diff")
    if not p.exists():
        return None
    rows = list(csv.reader(p.open()))
    verdict = next((r[1] for r in rows if r and r[0] == "VERDICT"), "?")
    distinct = next((int(r[1]) for r in rows if r and r[0] == "distinct_results"), 0)
    head = rows[0]
    data = [r for r in rows[1:] if r and r[0] not in ("install_witness", "meta", "VERDICT", "distinct_results",
                                                      "state_regions")]
    changed = sum(1 for r in data if len(head) > 6 and r[6] == "True")
    return dict(path=p.relative_to(ROOT).as_posix(), verdict=verdict, vectors=len(data), distinct=distinct,
                changed=changed, stateful=len(head) > 4)


def purposes(batch):
    f = ROOT / "log" / "c3" / f"{batch}_purpose.md"
    out = {}
    if not f.exists():
        return out
    for sec in re.split(r"\n(?=## 0x)", f.read_text(encoding="utf-8")):
        m = re.match(r"## (0x[0-9a-fA-F]{8})", sec)
        p = re.search(r"\*\*Purpose:\*\*\s*(.+?)(?:\n\s*\n|\Z)", sec, re.S)
        if m and p:
            out[int(m.group(1), 16)] = " ".join(p.group(1).split())
    return out


# Functions held at C2 by a verifier's judgement that the gate cannot express (the A/B is GREEN but does not reach the
# behaviour). --apply never promotes them; remove a row once its evidence is redone.
HOLD = {(r["module"], int(r["addr"], 16)) for r in csv.DictReader(open(ROOT / "re" / "c3_hold.tsv"), delimiter="	")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("batch")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--keys", action="store_true")
    ap.add_argument("--callers", default="", help="addr=caller[;caller] for a function reached only through a stored "
                    "pointer (no call xref), passed to re_classify promote --callers; cite the store in the Purpose")
    ap.add_argument("--skip", default="", help="comma-separated addresses not to promote (e.g. only guard branches were exercised)")
    a = ap.parse_args()
    hooks = keys_of(a.batch)
    if a.keys:
        print(" ".join(hooks))
        return 0
    # keyed by (module, addr): DLL RVAs collide across jgld.dll / sound.dll / Terrain.dll
    by_addr = collections.defaultdict(list)
    for k, spec in hooks.items():
        by_addr[(spec.get("module") or "golf_clean.exe", spec["addr"])].append(k)
    rows = {((r["module"] or "golf_clean.exe"), r["addr"]): r for r in csv.DictReader(open(ROOT / "hooks.csv"))}
    purp = purposes(a.batch)
    ready = []
    for (mod, addr), ks in sorted(by_addr.items()):
        facts = {k: csv_facts(hooks[k], k) for k in ks}
        row = rows.get((mod, f"{addr:08x}"), {})
        ok = all(f and f["verdict"] == "GREEN" for f in facts.values())
        varied = any(f and (f["distinct"] > 1 or f["changed"]) for f in facts.values())
        status = "READY" if ok and varied and addr in purp else "GREEN-but-single-valued" if ok and not varied else \
            "no-purpose" if ok else "NOT-GREEN"
        if row.get("confidence") in ("C3", "C4"):
            status = "already " + row["confidence"]
        where = f"0x{addr:08x}" if mod == "golf_clean.exe" else f"{mod}+0x{addr:08x}"
        print(f"{where} {row.get('name', '?'):28} {status}")
        for k, f in facts.items():
            print(f"    {k:28} " + ("no CSV" if not f else f"{f['verdict']} {f['vectors']} vectors, {f['distinct']} distinct"
                                     + (f", {f['changed']} change state" if f["stateful"] else "")))
        skip = {int(s, 16) for s in a.skip.split(",") if s.strip()}
        if (mod, addr) in HOLD:                    # re/c3_hold.tsv: held by judgement, never promoted by --apply
            skip.add(addr)
        if status == "READY" and addr in skip:
            print("    held back by --skip or re/c3_hold.tsv")
        elif status == "READY":
            ready.append((mod, addr, row, facts))
    if not a.apply:
        return 0
    for mod, addr, row, facts in ready:
        note = ROOT / row["note"]
        t = note.read_text(encoding="utf-8")
        if "## Purpose" in t:
            print(f"0x{addr:08x}: note already has a Purpose; not touched")
            continue
        ab = "; ".join(f"{k} ({f['vectors']} vectors, {f['distinct']} distinct results"
                       + (f", {f['changed']} changing state" if f["stateful"] else "") + f", `{f['path']}`)"
                       for k, f in facts.items())
        text = f"{purp[addr]} Reimplemented in `shim/src/re/{a.batch}.cpp`; path-1 A/B GREEN: {ab}."
        note.write_text(t.replace("## Signature", "## Purpose\n" + text + "\n\n## Signature", 1), encoding="utf-8")
        extra = {int(k, 16): v for k, v in (c.split("=", 1) for c in a.callers.split(",") if c.strip())}
        cmd = [sys.executable, str(ROOT / "scripts" / "re_classify.py"), "promote", f"{addr:08x}", "--to", "C3",
               "--file", f"shim/src/re/{a.batch}.cpp", "--module", mod]
        if addr in extra:
            cmd += ["--callers", extra[addr]]
        if "--callers" not in cmd and mod == "Terrain.dll" and (row.get("callers") or "none") == "none":
            # an export called only from the exe: its callers come through the exe's import thunks
            r0 = subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "import_callers.py"), "--callers-arg",
                                 f"0x{addr:x}"], capture_output=True, text=True, cwd=ROOT)
            if r0.stdout.strip():
                cmd += ["--callers", r0.stdout.strip()]
                print(f"0x{addr:08x}: callers from the exe import table: {r0.stdout.strip()}")
        if "--callers" not in cmd and (row.get("callers") or "none") == "none":
            # reached only through a stored pointer (vtable slot, callback field): the installers are the callers
            r1 = subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "pointer_callers.py"), mod,
                                 "--callers-arg", f"0x{addr:x}"], capture_output=True, text=True, cwd=ROOT)
            if r1.stdout.strip():
                cmd += ["--callers", r1.stdout.strip()]
                print(f"0x{addr:08x}: callers from stored-pointer installers: {r1.stdout.strip()}")
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
        lines = [l for l in r.stdout.splitlines() if "PROMOTED" in l or "REFUSED" in l or "FAIL" in l]
        print("\n".join(lines))
        if r.returncode != 0:                      # refused: take the Purpose back out so the note stays C2-shaped
            note.write_text(t, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
