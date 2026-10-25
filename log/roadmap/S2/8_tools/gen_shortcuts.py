"""S2.8: the shortcut text table of showShortcutsScreen as port code (model symbol + offset per string), from the C4 hook
source shim/src/re/c3es.cpp (kShortcutTop / kShortcutMid / kShortcutRest, 63 calls in order) and re/model/addresses.tsv.

usage: ~/bin/py -3.12 log/roadmap/S2/8_tools/gen_shortcuts.py   (prints the C++ table)
"""
import csv
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[4]


def symbols():
    rows = []
    with open(ROOT / "re" / "model" / "addresses.tsv", newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["module"] == "golf_clean.exe":
                rows.append((int(r["va"], 16), int(r["size"], 16), r["symbol"]))
    return rows


def sym(rows, va):
    for a, n, s in rows:
        if a <= va < a + max(n, 1):
            return s, va - a
    raise KeyError(hex(va))


def main():
    src = (ROOT / "shim" / "src" / "re" / "c3es.cpp").read_text()
    rows = symbols()
    out = []
    for m in re.finditer(r"\{(\d), 0x([0-9A-Fa-f]+), 0x([0-9A-Fa-f]+), 0x([0-9A-Fa-f]+), 0x([0-9A-Fa-f]+)\},?\s*//\s*(0x[0-9a-f]+)", src):
        widget, font, text, x, y, site = m.groups()
        font = int(font, 16)
        s, off = sym(rows, int(text, 16))
        title = 1 if font == 0x00821020 else 0
        out.append(f"    {{{widget}, {title}, GSO({s}, 0x{off:x}), 0x{int(x, 16):x}, 0x{int(y, 16):x}}},")
    print(len(out))
    print("\n".join(out))


if __name__ == "__main__":
    main()
