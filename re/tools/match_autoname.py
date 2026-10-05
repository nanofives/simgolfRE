"""Find which compiled function is which original: compile a source (typically an explicit template
instantiation such as `template class std::list<Tile*>;`), compare EVERY function symbol in the obj with each
candidate address, and print the pairs that match at 100% (and the best partial per address).
Useful when the code comes from a library header (VC6 STL) and only the instantiation, not the member names,
is known.

    py -3.12 re/tools/match_autoname.py re/match/terrain_list.cpp --module Terrain.dll 0x1000ae60 0x1000aed0 ...
    py -3.12 re/tools/match_autoname.py src.cpp --module Terrain.dll --range 0x1000ae60 0x1000c560
Prints `// MATCH:` lines ready to paste for the 100% pairs."""
import argparse, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402
from match_queue import queue  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("--module", required=True, choices=sorted(match.TSV))
    ap.add_argument("--range", nargs=2, metavar=("LO", "HI"), help="every unmatched candidate in [LO, HI]")
    ap.add_argument("--flags", default="")
    ap.add_argument("--prefer", default="", help="regex: among names that all match at 100%, prefer these")
    ap.add_argument("addrs", nargs="*")
    a = ap.parse_args()
    src = pathlib.Path(a.src)
    text = src.read_text()
    flags = a.flags or match.file_flags(text, a.module) or match.DEFAULT_FLAGS
    lang = "c" if re.search(r"//\s*LANG\s+c\b", text) else "c++"
    secs, syms = match.coff(match.compile_obj(src, flags, lang))
    funcs = sorted({s["name"] for s in syms if s["sec"] > 0 and s["type"] == 0x20})
    base = match.image_base(a.module)
    addrs = [int(x, 16) for x in a.addrs]
    if a.range:
        lo, hi = (int(x, 16) for x in a.range)
        for _, _, key, _, _ in queue(a.module):
            va = key if a.module == "golf_clean.exe" else key + base
            if lo <= va <= hi:
                addrs.append(va)
    taken = set()
    for addr in sorted(set(addrs)):
        best, exact = (0.0, None), []
        for name in funcs:
            if name in taken:
                continue
            try:
                acc = match.score_function(a.module, addr, secs, syms, name)[0]
            except SystemExit:
                continue
            if acc == 1:
                exact.append(name)
            elif acc > best[0]:
                best = (acc, name)
        if exact:
            pref = [n for n in exact if a.prefer and re.search(a.prefer, n)]
            pick = (pref or exact)[0]
            taken.add(pick)
            note = f"   // {len(exact)} names compile identically: name not determined" if len(exact) > 1 else ""
            print(f"// MATCH: {a.module} 0x{addr:08x} {pick}{note}")
        else:
            print(f"#  0x{addr:08x} best {100 * best[0]:.1f}% {best[1]}")


if __name__ == "__main__":
    main()
