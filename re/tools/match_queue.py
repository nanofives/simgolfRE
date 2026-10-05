"""Next functions to match, per module: game code only (no thunks, no Unwind@/Catch@ funclets, no FidDb library
code), not yet in re/match or re/match/wip, not in re/match/skip.tsv. Reached functions (coverage census) come
first unless --size-order; within a group, smallest first. Each line ends with a one-line disassembly.

    py -3.12 re/tools/match_queue.py                                   # golf_clean.exe, 10 candidates
    py -3.12 re/tools/match_queue.py --module jgld.dll --min 30 --max 120 --count 20
    py -3.12 re/tools/match_queue.py --module Terrain.dll --no-asm --count 100"""
import argparse, csv, glob, pathlib, re, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
from match import TSV, image_base, tracker_key  # noqa: E402

FUNCLETS = ("thunk_", "Unwind@", "Catch@", "Catch_All@", "FID_conflict:")


def not_game(name, size, code=b""):
    """Thunks, compiler EH funclets, ambiguous library matches and import stubs (`jmp dword ptr [IAT]`)."""
    return name.startswith(FUNCLETS) or size <= 5 or (size == 6 and code[:2] == bytes((0xFF, 0x25)))


def rows(path):
    return list(csv.DictReader(open(path), delimiter="\t"))


def annotated(module):
    """Tracker keys of every // MATCH: in re/match and re/match/wip for `module`."""
    out = set()
    for f in glob.glob(str(ROOT / "re" / "match" / "*.cpp")) + glob.glob(str(ROOT / "re" / "match" / "wip" / "*.cpp")):
        for m, a in re.findall(r"// MATCH: (\S+) 0x([0-9a-fA-F]+)", open(f).read()):
            if m == module:
                out.add(tracker_key(m, int(a, 16)))
    return out


def queue(module, lo=0, hi=1 << 30, size_order=False):
    lib = {tracker_key(module, int(r["entry"], 16)) for r in rows(ROOT / "re" / "fid" / f"{module}.fid.tsv")
           if r["unique"] == "1"}
    skip = {tracker_key(r["module"], int(r["addr"], 16)) for r in rows(ROOT / "re" / "match" / "skip.tsv")
            if r["module"] == module}
    reach = set()
    for f in glob.glob(str(ROOT / "re" / "coverage" / "*.tsv")):
        for r in rows(f):
            if r["module"] == module:
                reach.add(tracker_key(module, int(r["addr"], 16)))
    done = annotated(module)
    pe = pefile.PE(str(ROOT / "original" / module), fast_load=True)
    base = image_base(module)
    cands = []
    for r in rows(ROOT / "re" / TSV[module]):
        key, size = int(r["entry"], 16), int(r["size"])
        va = key if module == "golf_clean.exe" else key + base
        if not_game(r["name"], size, pe.get_data(va - base, 2)) or not lo <= size <= hi:
            continue
        if key in lib or key in skip or key in done:
            continue
        cands.append((0 if (key in reach and not size_order) else 1, size, key, r["name"], key in reach))
    return sorted(cands)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", default="golf_clean.exe", choices=sorted(TSV))
    ap.add_argument("--min", type=int, default=0)
    ap.add_argument("--max", type=int, default=1 << 30)
    ap.add_argument("--count", type=int, default=10)
    ap.add_argument("--size-order", action="store_true", help="ignore reach, smallest first")
    ap.add_argument("--no-asm", action="store_true")
    a = ap.parse_args()
    q = queue(a.module, a.min, a.max, a.size_order)
    print(f"# {a.module}: {len(q)} candidates in {a.min}..{a.max} bytes ({sum(c[4] for c in q)} reached)")
    pe = pefile.PE(str(ROOT / "original" / a.module), fast_load=True)
    base = image_base(a.module)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for _, size, key, name, reached in q[:a.count]:
        va = key if a.module == "golf_clean.exe" else key + base
        line = f"0x{va:08x}\t{size}\t{'R' if reached else '-'}\t{name}"
        if not a.no_asm:
            ins = md.disasm(pe.get_data(va - base, size), va)
            line += "\t" + " | ".join(f"{i.mnemonic} {i.op_str}".strip() for i in ins)
        print(line)


if __name__ == "__main__":
    main()
