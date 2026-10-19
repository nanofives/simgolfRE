"""Phase 3.5 helper: turns a range of a hook source (shim/src/re/*.cpp) into port-style C++ text.

Comments are dropped (they cite original addresses), every literal in the exe data range is replaced by `GS(member) + off`
(member = the GameState object of re/model/addresses.tsv that contains it) and `G<T>(` / `ref<T>(` / `at<T>(` gain the `ctx`
argument. `--const=NAME=0xADDR` first replaces the identifier NAME by the literal (file-level constants of the hook source).
The result is a starting point that a person edits (callees, types); a literal that is in no object is left and reported.
usage: py -3.12 log/p3/p35_hook2port.py <file> <first line> <last line> [--const=NAME=0xADDR ...]
"""
import csv
import re
import sys

ROWS = []
with open(r"C:\Users\maria\Desktop\Proyectos\SimGolf\re\model\addresses.tsv", encoding="utf8") as f:
    for r in csv.reader(f, delimiter="\t"):
        if r[0] == "symbol" or len(r) < 13 or r[12] != "top":
            continue
        try:
            ROWS.append((int(r[2], 16), int(r[3], 16), r[0]))
        except ValueError:
            pass
ROWS.sort()


def lookup(v):
    best = None
    for b, s, n in ROWS:
        if b <= v < b + s:
            best = (n, v - b)
    return best


def conv(m):
    v = int(m.group(1), 16)
    if not (0x400000 <= v < 0x900000):
        return m.group(0)
    hit = lookup(v)
    if hit is None:
        sys.stderr.write("NO OBJECT for " + m.group(0) + "\n")
        return m.group(0)
    n, off = hit
    return "(GS(%s) + %d)" % (n, off) if off else "GS(%s)" % n


HEX = re.compile(r"\b0x([0-9a-fA-F]{6,8})[uU]?\b")
GCALL = re.compile(r"\b(?:G|ref|at)<([^>]+)>\(")
FCALL = re.compile(r"\bfld<([^>]+)>\(")


def main():
    args = [x for x in sys.argv[1:] if not x.startswith("--const=")]
    consts = dict(x[8:].split("=", 1) for x in sys.argv[1:] if x.startswith("--const="))
    path, a, b = args[0], int(args[1]), int(args[2])
    whole = open(path, encoding="utf8").read()
    # file-level constants that name an address of the data image (strings, tables): k<Name> = reinterpret_cast<..>(0x...)
    for mm in re.finditer(r"\b(k\w+)\s*=\s*reinterpret_cast<[^;]*?>\(\s*(0x[0-9A-Fa-f]{6,8})u?\s*\)", whole):
        if int(mm.group(2), 16) >= 0x4C1000 and mm.group(1) not in consts:
            consts[mm.group(1)] = mm.group(2)
    lines = whole.splitlines()[a - 1:b]
    out = []
    for ln in lines:
        i = ln.find("//")
        if i >= 0:
            ln = ln[:i]
        ln = ln.rstrip()
        if not ln.strip():
            continue
        for nm, lit in consts.items():
            ln = re.sub(r"\b" + nm + r"\b", lit, ln)
        ln = HEX.sub(conv, ln)
        ln = GCALL.sub(lambda mm: "G<" + mm.group(1) + ">(ctx, ", ln)
        ln = FCALL.sub(lambda mm: "fld<" + mm.group(1) + ">(ctx, ", ln)
        out.append(ln)
    print("\n".join(out))


main()
