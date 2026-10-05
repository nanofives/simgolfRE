"""Collect ghidra2src.py results at 100% into re/match/<prefix>_raw_NN.cpp files.

Each function keeps its own declarations inside `namespace f_<addr> { ... }` (two functions may declare the same
global with different access sizes, or the same callee through different wrappers); namespaces change symbol names,
not code. The merged file is recompiled and every annotation must still score 100%, else that function is left out.

    py -3.12 re/tools/g2s_collect.py --module jgld.dll --prefix jgld [--per-file 40]"""
import argparse, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402
import ghidra2src  # noqa: E402
from match_queue import annotated  # noqa: E402

INTRO = """// {module} functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (raw form: Ghidra's offsets,
// casts and FUN_/DAT_ names; in a /Od build this compiles to the same instructions as the original member
// accesses, so each // MATCH: below is a 100% instruction match). Names, types and layouts are still to be
// written: these are evidence of byte identity, not readable source. String literals are placeholders named by
// address (s_<addr>), never the game's text.
"""


def body_of(path):
    """Everything after the shared header: the function's declarations and definition."""
    t = path.read_text()
    t = t.split("\n", 1)[1]                       # FLAGS line
    t = t.replace(ghidra2src.HEADER, "")
    return t.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", required=True)
    ap.add_argument("--prefix", required=True)
    ap.add_argument("--per-file", type=int, default=40)
    a = ap.parse_args()
    d = ROOT / "log" / "g2s" / a.module
    done = annotated(a.module)
    base = match.image_base(a.module)
    ok = []
    for l in (d / "results.tsv").read_text().splitlines():
        addr, r = l.split("\t")[:2]
        va = int(addr, 16)
        key = match.tracker_key(a.module, va)
        if r == "100.0" and key not in done:
            src = d / f"{va:08x}.best.cpp"
            ok.append((va, src if src.exists() else d / f"{va:08x}.cpp"))
    ok.sort()
    existing = sorted((ROOT / "re" / "match").glob(f"{a.prefix}_raw_*.cpp"))
    n = len(existing) + 1
    flags = ghidra2src.FLAGS[a.module]
    total = 0
    for i in range(0, len(ok), a.per_file):
        chunk = ok[i:i + a.per_file]
        parts = [f"// FLAGS {a.module}: {flags}", INTRO.format(module=a.module), ghidra2src.HEADER]
        for va, src in chunk:
            b = body_of(src)
            b = re.sub(r"^\s*// MATCH: .*\n", "", b, flags=re.M)
            parts.append(f"namespace f_{va:08x} {{\n// MATCH: {a.module} 0x{va:08x} FUN_{va:08x}\n{b}\n}}\n")
        out = ROOT / "re" / "match" / f"{a.prefix}_raw_{n:02d}.cpp"
        out.write_text("\n".join(parts))
        # exact decorated names, then verify
        secs, syms = match.coff(match.compile_obj(out, flags))
        text = out.read_text()
        for va, _ in chunk:
            nm = next(s["name"] for s in syms if s["sec"] > 0 and s["type"] == 0x20 and f"FUN_{va:08x}@" in s["name"]
                      and f"f_{va:08x}" in s["name"])
            text = text.replace(f"// MATCH: {a.module} 0x{va:08x} FUN_{va:08x}\n", f"// MATCH: {a.module} 0x{va:08x} {nm}\n")
        out.write_text(text)
        res = match.compare(out, "", False, quiet=True)
        bad = [r for r in res if r[4] < 1]
        for r in bad:
            text = text.replace(f"// MATCH: {a.module} 0x{r[1]:08x} ", f"// NOT 100% when merged: {a.module} 0x{r[1]:08x} ")
        out.write_text(text)
        good = len(res) - len(bad)
        total += good
        print(f"{out.name}: {good}/{len(chunk)} at 100%")
        n += 1
    print(f"total {total}")


if __name__ == "__main__":
    main()
