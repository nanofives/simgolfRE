"""S3.4 coverage: every loadFlic call of loadWorldAssets' tables (port/src/sim/menu_flics.inc, generated from c3fv.cpp) against
the recorded loads (port/corpus/flic/*/flic/loads.jsonl): a table entry is covered when a recorded loadWorldAssets load has
its path, frames, size and flag (the path resolved from the user's exe at the entry's string-pool offset).

    ~/bin/py -3.12 log/roadmap/S3/4_coverage.py
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
POOLS = {"f_strpool_4c95f4": 0x004C95F4, "f_strpool_4c8a28": 0x004C8A28}
WA_LO, WA_HI = 0x0043DBE0, 0x00442180


def main():
    import pefile
    pe = pefile.PE(str(ROOT / "original" / "golf_clean.exe"), fast_load=True)
    img, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase

    def s(va):
        o = va - base
        return img[o:img.index(b"\0", o)].decode("latin-1")
    rec = set()
    for f in sorted((ROOT / "port" / "corpus" / "flic").glob("*/flic/loads.jsonl")):
        for line in f.read_text().splitlines():
            x = json.loads(line)
            if WA_LO <= x["caller"] < WA_HI:
                a = x["args"]
                rec.add((x["path"], a[2], a[3] - (1 << 32) if a[3] & 0x80000000 else a[3], a[4]))
    text = (ROOT / "port" / "src" / "sim" / "menu_flics.inc").read_text()
    cur, rows, total, miss = None, [], 0, []
    per = {}
    for line in text.splitlines():
        m = re.match(r"const FlicCall (\w+)", line)
        if m:
            cur = m.group(1)
        for g in re.finditer(r"\{(0x[0-9a-f]+), GSO\((\w+), (0x[0-9a-f]+)\), (-?\d+), (-?\d+), (\d+)\}", line):
            _id, pool, off, fr, dl, fl = g.groups()
            key = (s(POOLS[pool] + int(off, 16)), int(fr), int(dl), int(fl))
            total += 1
            hit = key in rec
            per.setdefault(cur, [0, 0])
            per[cur][0] += hit
            per[cur][1] += 1
            if not hit:
                miss.append((cur, key))
    for t, (h, n) in per.items():
        print(f"{t:24s} {h:3d} / {n:3d}")
    print(f"total {total - len(miss)} / {total} table entries covered")
    for m in miss:
        print("MISSING", m)
    return 0 if not miss else 1


if __name__ == "__main__":
    sys.exit(main())
