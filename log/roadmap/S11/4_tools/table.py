"""S11.4: the rename table, built from the S10 merge lists (the rows S10 deferred because port / shim / frida code uses the old
name) and checked against re/names: a row is kept only while its re/names row still carries the old name.

    ~/bin/py -3.12 log/roadmap/S11/4_tools/table.py [--root <repo>]   -> log/roadmap/S11/4_renames.tsv

Columns: module addr kind old new src names_file
kind: func | global.  src: 8_merge.2 | 8_merge.2b | 10_renames | 9_renames_sound.
"""
from __future__ import annotations

import csv
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
S10 = ROOT / "log" / "roadmap" / "S10"
OUT = HERE.parent / "4_renames.tsv"

# 8_merge.md section 2 / 2b: parsed from the markdown tables (P rows only; `(keep)` skipped)
ROW = re.compile(r"^\| *([\w.]+\.tsv) *\| *(0x[0-9a-f]{8}) *\| *([^|]+?) *\| *([^|]+?) *\| *(P[^|]*|-) *\|")


def names_index(root: pathlib.Path) -> dict[tuple[str, str], tuple[str, str]]:
    idx = {}
    for f in sorted((root / "re" / "names").glob("*.tsv")):
        for line in f.read_text(encoding="utf-8").splitlines():
            p = line.split("\t")
            if len(p) >= 3 and p[1].startswith("0x"):
                idx.setdefault((p[0], p[1].lower()), (p[2], f.name))
    return idx


def rows8() -> list[dict]:
    out, sec = [], None
    for line in (S10 / "8_merge.md").read_text(encoding="utf-8").splitlines():
        if line.startswith("## 2. "):
            sec = "8_merge.2"
        elif line.startswith("### 2b"):
            sec = "8_merge.2b"
        elif line.startswith("### 2c") or line.startswith("## 3"):
            sec = None
        m = ROW.match(line) if sec else None
        if not m or not m.group(5).startswith("P") or m.group(4) == "(keep)":
            continue
        f, a, old, new = m.group(1), m.group(2), m.group(3), m.group(4)
        out.append(dict(module="golf_clean.exe", addr=a, kind="global" if old.startswith("g_") else "func", old=old,
                        new=new, src=sec))
    return out


def rows10() -> list[dict]:
    out = []
    with open(S10 / "10_renames.tsv", encoding="utf-8") as fh:
        for r in csv.DictReader(fh, delimiter="\t"):
            if r["port"].startswith("P"):
                out.append(dict(module="golf_clean.exe", addr=r["addr"], kind="func", old=r["old"], new=r["new"], src="10_renames"))
    return out


def rows9() -> list[dict]:
    out = []
    with open(S10 / "9_renames_sound.tsv", encoding="utf-8") as fh:
        for r in csv.DictReader(fh, delimiter="\t"):
            out.append(dict(module="sound.dll", addr=r["addr"], kind="func", old=r["old"], new=r["new"], src="9_renames_sound"))
    return out


def main() -> None:
    root = pathlib.Path(sys.argv[sys.argv.index("--root") + 1]) if "--root" in sys.argv else ROOT
    idx = names_index(root)
    rows, skipped = [], []
    for r in rows8() + rows10() + rows9():
        cur = idx.get((r["module"], r["addr"]))
        if cur is None:
            raise SystemExit(f"no re/names row for {r}")
        if cur[0] == r["new"]:
            skipped.append((r, "already renamed"))
            continue
        if cur[0] != r["old"]:
            raise SystemExit(f"re/names has {cur[0]!r} for {r}")
        r["names_file"] = cur[1]
        rows.append(r)
    olds = [r["old"] for r in rows]
    news = [r["new"] for r in rows]
    assert len(set(olds)) == len(olds) and len(set(news)) == len(news)
    taken = {v[0] for v in idx.values()} - set(olds)
    clash = [n for n in news if n in taken]
    assert not clash, clash
    cols = ["module", "addr", "kind", "old", "new", "src", "names_file"]
    with open(OUT, "w", encoding="utf-8", newline="") as fh:
        fh.write("\t".join(cols) + "\n")
        for r in rows:
            fh.write("\t".join(r[c] for c in cols) + "\n")
    print(f"{len(rows)} renames -> {OUT}")
    for r, why in skipped:
        print(f"  skipped {r['module']} {r['addr']} {r['old']} -> {r['new']}: {why}")


if __name__ == "__main__":
    main()
