"""Phase 3.7a: turns a hook source (shim/src/re/*.cpp, the C3/C4 reimplementation) into port C++ text for the tick module.

The generated text is included inside `struct Tk` of port/src/sim/tick*.cpp (member functions), so every converted
function sees the context (`ctx`) and the frame block (`F`, a virtual address of the context's address space):

- every literal inside golf_clean.exe's .data (0x004c1000..) becomes `(GS(sym) + off)`: sym = the GameState object of
  re/model/addresses.tsv that holds it (GameState keeps the .data layout, so any address is base + offset);
- `CC(0x<code>, ...)` / `TC(0x<code>, self, ...)` become `cc("<hooks.csv name>", ...)` / `tc("<name>", self, ...)`:
  calls by registry name, dispatched by tick.cpp (port function, documented drop, or MissingResource);
- the explicit idiv blocks become `idiv64(edx, eax, d)` (DivideError where idiv raises #DE);
- `u8* F` parameters become `u32 F`; `static` / `inline` / `__declspec(noinline)` / `namespace {` lines are dropped;
- comments keep their text, with every original address written without its `0x` prefix as a citation:
  `ML+75c` = mainLoop + 0x75c, `<function>+off`, `<object>+off`; quoted game text in comments is replaced by <text>.
Literals left in the code range are reported on stderr (hand edits); so are literals in no object.
usage: py -3.12 log/p3/p37a_conv.py <src.cpp> <first line> <last line> [--hook] > out.inc
       (--hook: a per-batch hook source: typed function-pointer constants k<Name> become call macros by hooks.csv name,
        P() / reinterpret_cast<void*> / void* values become u32 virtual addresses; out-pointer arguments are hand edits)
       py -3.12 log/p3/p37a_conv.py --sym 0x<va> ...
"""
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
DATA_LO, DATA_HI = 0x004C1000, 0x004C1000 + 0x381174
ML_LO, ML_HI = 0x0040F5C0, 0x00421B30

ROWS = []
with open(ROOT / "re" / "model" / "addresses.tsv", encoding="utf8") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        if r["relation"] != "top" or r["module"] != "golf_clean.exe":
            continue
        try:
            ROWS.append((int(r["va"], 16), int(r["size"], 16), r["symbol"]))
        except ValueError:
            pass
ROWS.sort()
OBJ_NAMES = set()
for line in (ROOT / "port" / "include" / "gamestate_meta.inc").read_text().splitlines():
    m = re.match(r"SG_OBJECT\((\w+),", line)
    if m:
        OBJ_NAMES.add(m.group(1))
ROWS = [r for r in ROWS if r[2] in OBJ_NAMES]

NAMES = {}
for r in csv.DictReader(open(ROOT / "hooks.csv", encoding="utf8")):
    if r["module"] == "golf_clean.exe":
        NAMES[int(r["addr"], 16)] = r["name"]
FUNCS = []
for line in (ROOT / "re" / "functions_ghidra.tsv").read_text().splitlines()[1:]:
    c = line.split("\t")
    FUNCS.append((int(c[0], 16), int(c[2]), c[1]))
FUNCS.sort()


def obj(v):
    best = None
    for b, s, n in ROWS:
        if b <= v < b + s:
            return n, v - b
        if b <= v:
            best = (n, v - b)
    return best


def fname(v):
    if v in NAMES:
        return NAMES[v]
    for a, s, n in FUNCS:
        if a == v:
            return n
    return None


def cite(v):
    if ML_LO <= v < ML_HI:
        return "ML+0x%x" % (v - ML_LO)
    if 0x00400000 <= v < DATA_LO:
        n = fname(v)
        if n:
            return n
        for a, s, nm in FUNCS:
            if a <= v < a + max(s, 1):
                return "%s+0x%x" % (nm, v - a)
        return "code:%x" % (v - 0x00400000)
    if DATA_LO <= v < DATA_HI:
        o = obj(v)
        if o:
            return o[0] if o[1] == 0 else "%s+0x%x" % o
    return "%x" % v


HEX = re.compile(r"\b0x([0-9a-fA-F]{6,8})[uU]?\b")
WARN = []


def conv_code(code, lineno):
    def call(m):
        kind, v = m.group(1), int(m.group(2), 16)
        n = fname(v)
        if n is None:
            WARN.append("%d: call to unnamed 0x%x" % (lineno, v))
            n = "FUN_%08x" % v
        return '%s("%s"' % ("cc" if kind == "CC" else "tc", n)

    code = re.sub(r"\b(CC|TC)\(\s*0x([0-9a-fA-F]{6,8})[uU]?", call, code)
    code = re.sub(
        r"\{\s*long long _n = \(\(long long\)edx<<32\) \| \(u32\)eax; int _d=\((.*?)\); eax=\(int\)\(_n/_d\); edx=\(int\)\(_n%_d\);\s*\}",
        lambda m: "{ idiv64(edx, eax, (%s)); }" % m.group(1), code)

    def lit(m):
        v = int(m.group(1), 16)
        if v == 0x00400000:
            return "(1u << 22)"
        if DATA_LO <= v < DATA_HI:
            o = obj(v)
            if o is None:
                WARN.append("%d: data literal in no object 0x%x" % (lineno, v))
                return m.group(0)
            return "(GS(%s) + %d)" % o if o[1] else "GS(%s)" % o[0]
        if 0x00400000 <= v < 0x00900000:
            WARN.append("%d: literal 0x%x (%s) left" % (lineno, v, cite(v)))
            return "CODE_%x" % v
        return m.group(0)

    code = HEX.sub(lit, code)
    code = re.sub(r"\bu8\*\s*F\b", "u32 F", code)
    code = re.sub(r"\bconst u8\*\s*F\b", "u32 F", code)
    code = re.sub(r"^(\s*)(?:__declspec\(noinline\)\s*)?(?:static\s+)?(?:inline\s+)?(?=(?:void|int|u32|bool|short|u8|u16|s8|unsigned char|unsigned|signed char)\b[^;=]*\()", r"\1", code)
    return code


def conv_comment(c):
    c = re.sub(r'"[^"]*"', "<text>", c)
    return HEX.sub(lambda m: cite(int(m.group(1), 16)), c)


FASTCALL = set()


def hook_line(ln, lineno):
    """--hook mode: the idioms of the per-batch hook sources (typed function-pointer constants, P(), void* values)."""
    s = ln.strip()
    m = re.match(r"typedef\s+[^;]*\(\s*(__\w+)\s*\*\s*(\w+)\s*\)", s)
    if m:
        if m.group(1) == "__fastcall":
            FASTCALL.add(m.group(2))
        return "// (typedef dropped) " + s
    m = re.match(r"const\s+(\w+)\s+(k\w+)\s*=\s*reinterpret_cast<\s*\1\s*>\(\s*0x([0-9a-fA-F]+)u?\s*\)\s*;(.*)", s)
    if m:
        n = fname(int(m.group(3), 16)) or ("FUN_%08x" % int(m.group(3), 16))
        how = "tcx" if m.group(1) in FASTCALL else "cc"
        if n == "__itoa":  # TkBase::Itoa (the static CRT's itoa over the address space)
            return "#define %s(...) Itoa(__VA_ARGS__)" % m.group(2)
        if n == "Random::range":  # this = the game's generator g_rng in every hook source: TkBase::Rnd
            return "#define %s(self, edx, n) Rnd(n)" % m.group(2)
        return "#define %s(...) %s(\"%s\", ##__VA_ARGS__)%s" % (m.group(2), how, n, (" " + m.group(4).strip()) if m.group(4).strip() else "")
    if re.match(r"(SG_HOOK\(|\w+_t\s+\w+_orig;|#include|template <typename T> static inline T& G\()", s):
        return "// " + s
    ln = re.sub(r"\b__(cdecl|fastcall|stdcall)\s+", "", ln)
    T = r"(unsigned char|signed char|unsigned short|unsigned int|char|short|int|unsigned|u8|s8|u16|u32)"
    # typed host pointers into the image -> VP<T> (a typed virtual address: * and [] give Ref<T>, + scales like C)
    ln = re.sub(r"\breinterpret_cast<\s*(?:const\s+)?void\s*\*\s*>\(", "(u32)(", ln)
    ln = re.sub(r"\b(?:reinterpret|static)_cast<\s*(?:const\s+)?" + T + r"\s*\*\s*>\(", r"vp<\1>(", ln)
    ln = re.sub(r"\b(?:const\s+)?" + T + r"\s*\*\s*(?:const\s+)?(\w+)\s*=", r"VP<\1> \2 =", ln)
    # a pointer value as an integer: the address itself
    ln = re.sub(r"\breinterpret_cast<\s*(int|unsigned|unsigned int)\s*>\(", r"static_cast<\1>(", ln)
    # an uninitialised char pointer local (assigned a string address later)
    ln = re.sub(r"\b(?:const\s+)?char\s*\*\s*(\w+)\s*;", r"u32 \1;", ln)
    ln = re.sub(r"\bP\(", "(u32)(", ln)
    # BR(jcc address, cond): the coverage build's jcc address (a case marker BR(table + 4 * i, true) has no effect)
    ln = re.sub(r"\bBR\(\s*[^,()]*0x[0-9a-fA-F]+u?[^,]*,", "BR(0,", ln)
    ln = re.sub(r"\bG<void\*>", "G<u32>", ln)
    ln = re.sub(r"\bvoid\*\s*(\w*)", lambda mm: "u32 " + mm.group(1), ln)
    ln = re.sub(r"^(\s*)const (signed char|unsigned char|int|short|unsigned) (k\w+\[)", r"\1static constexpr \2 \3", ln)
    # a table of image addresses becomes a per-object array (its GS() values need the context)
    ln = re.sub(r"^(\s*)static constexpr unsigned (k\w+\[\d+\]\s*=\s*\{[^}]*0x)", r"\1const u32 \2", ln)
    return ln


def split(line):
    i = line.find("//")
    if i < 0:
        return line, ""
    return line[:i], line[i:]


def main():
    if sys.argv[1] == "--sym":  # print the GameState symbol + offset of data addresses
        for a in sys.argv[2:]:
            v = int(a, 16)
            print(a, "GS(%s) + 0x%x" % obj(v) if obj(v) else "?", cite(v))
        return
    hook = "--hook" in sys.argv
    args = [x for x in sys.argv[1:] if x != "--hook"]
    path, a, b = args[0], int(args[1]), int(args[2])
    whole = open(path, encoding="utf8").read().splitlines()
    lines = whole[a - 1:b]
    out = []
    pre = []
    if hook:
        # the whole file's typedefs first (which are fastcall), then the call macros the range uses but defines elsewhere
        for k, ln in enumerate(whole, 1):
            if ln.strip().startswith("typedef"):
                hook_line(split(ln)[0], k)
        inside = "\n".join(lines)
        for k, ln in enumerate(whole, 1):
            if a <= k <= b:
                continue
            m = re.match(r"\s*const\s+\w+\s+(k\w+)\s*=\s*reinterpret_cast<", ln)
            if m and re.search(r"\b%s\(" % m.group(1), inside) and not re.search(r"const\s+\w+\s+%s\s*=" % m.group(1), inside):
                pre.append(hook_line(split(ln)[0], k))
    for k, ln in enumerate(lines, a):
        s = ln.strip()
        if s in ("namespace {", "}  // namespace") or s.startswith("}  // namespace"):
            continue
        if hook and not s.startswith("//"):
            code0, com0 = split(ln)
            ln = hook_line(code0, k) + com0
        code, com = split(ln)
        code = conv_code(code, k)
        com = conv_comment(com)
        out.append((code + com).rstrip())
    if hook:  # the call macros are file-wide: undefine them at the end of the generated part
        names = sorted(set(re.findall(r"^#define (\w+)\(", "\n".join(out), re.M)))
        pre = sorted(set(pre))
        names = sorted(set(names) | set(re.findall(r"^#define (\w+)\(", "\n".join(pre), re.M)))
        out = ["#define BR(a, c) (c)"] + pre + out + ["#undef BR"] + ["#undef %s" % n for n in names]
    sys.stdout.write("\n".join(out) + "\n")
    for w in WARN:
        sys.stderr.write(w + "\n")


if __name__ == "__main__":
    main()
