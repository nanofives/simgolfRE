"""Port survey 2: which golf_clean.exe hook reimplementations depend on the original process.

Parses the linked sources (shim/re_batches.txt batches plus the core golf_*.cpp), finds each SG_HOOK("golf_clean.exe", ...),
locates its detour definition, and classifies the detour's own body (helpers it calls are not followed, so a body that only
forwards to a helper reads as pure; the count is a lower bound for dependence):

  pure        no literal address, no *_orig call
  globals     reads/writes literal data addresses (>= 0x4ba000) only
  callees     calls original code (literal text address < 0x4ba000 or the *_orig pointer)
  both        globals and callees
  naked       __declspec(naked) body

    py -3.12 re/tools/port_survey_hooks.py [--out log/port_hooks.tsv]
"""
import argparse, collections, pathlib, re

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "shim" / "src" / "re"
HOOK = re.compile(r'SG_HOOK\(\s*"golf_clean\.exe"\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*([\w:]+)\s*,\s*([\w:]+)\s*,\s*([\w:]+)\s*\)')
LIT = re.compile(r'\b0x00[4-8][0-9a-fA-F]{5}\b')


def linked_files():
    files = []
    bl = ROOT / "shim" / "re_batches.txt"
    names = []
    if bl.exists():
        for line in bl.read_text().split():
            line = line.strip()
            if line and not line.startswith("#"):
                names.append(line)
    for n in names:
        p = SRC / (n if n.endswith(".cpp") else n + ".cpp")
        if p.exists():
            files.append(p)
    for p in sorted(SRC.glob("golf_*.cpp")):
        if p not in files:
            files.append(p)
    return [p for p in files if not re.search(r"(negctl|_cov|_dbl)", p.name)]


def body_of(text, detour):
    name = detour.split("::")[-1]
    for m in re.finditer(r'(?<![\w.>])' + re.escape(name) + r'\s*\(', text):
        # definition: after the closing paren comes '{' (not ';' and not an SG_HOOK argument list)
        i, depth = m.end(), 1
        while i < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
        j = i
        while j < len(text) and text[j] in " \t\r\n":
            j += 1
        if j < len(text) and text[j] == "{":
            k, d = j, 0
            while k < len(text):
                d += {"{": 1, "}": -1}.get(text[k], 0)
                k += 1
                if d == 0:
                    break
            return text[max(0, m.start() - 120):m.start()], text[j:k]
    return None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "log" / "port_hooks.tsv"))
    a = ap.parse_args()
    rows, cnt = [], collections.Counter()
    for p in linked_files():
        text = p.read_text(encoding="utf-8", errors="replace")
        for m in HOOK.finditer(text):
            addr, _, detour, orig = m.groups()
            head, body = body_of(text, detour)
            if body is None:
                kind, g, c = "nobody", 0, 0
            else:
                lits = {int(x, 16) for x in LIT.findall(body)}
                g = sum(1 for x in lits if x >= 0x4BA000)
                c = sum(1 for x in lits if x < 0x4BA000) + (1 if re.search(r'\b' + re.escape(orig.split("::")[-1]) + r'\b', body) else 0)
                if "naked" in (head or ""):
                    kind = "naked"
                elif g and c:
                    kind = "both"
                elif g:
                    kind = "globals"
                elif c:
                    kind = "callees"
                else:
                    kind = "pure"
            cnt[kind] += 1
            rows.append((addr, p.name, kind, g, c))
    pathlib.Path(a.out).parent.mkdir(exist_ok=True)
    with open(a.out, "w") as f:
        f.write("addr\tfile\tkind\tdata_lits\tcode_refs\n")
        for r in sorted(set(rows)):
            f.write("\t".join(map(str, r)) + "\n")
    uniq = len({r[0] for r in rows})
    print(f"linked source files: {len(linked_files())}; golf_clean.exe SG_HOOKs: {len(rows)} ({uniq} distinct addresses)")
    for k in ("pure", "globals", "callees", "both", "naked", "nobody"):
        print(f"  {k:8s} {cnt[k]}")


if __name__ == "__main__":
    main()
