"""Dump a jgld.dll vtable with each slot's thunk resolved to its body and named from re/names (S3.5 survey helper).

    ~/bin/py -3.12 log/roadmap/S3/5_tools/vt.py <vtable VA> <slots> [module]
"""
import csv
import glob
import pathlib
import struct
import sys

import pefile

ROOT = pathlib.Path(__file__).resolve().parents[4]


def main():
    vt = int(sys.argv[1], 16)
    n = int(sys.argv[2])
    mod = sys.argv[3] if len(sys.argv) > 3 else "jgld.dll"
    pe = pefile.PE(str(ROOT / "original" / mod), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    names = {}
    for f in glob.glob(str(ROOT / "re" / "names" / "*.tsv")):
        for row in csv.reader(open(f, encoding="utf-8"), delimiter="\t"):
            if len(row) > 2 and row[0] == mod:
                try:
                    names[int(row[1], 16)] = row[2]
                except ValueError:
                    pass

    def d(va):
        return struct.unpack_from("<I", img, va - base)[0]

    def resolve(a):
        if img[a - base] == 0xE9:
            return a + 5 + struct.unpack_from("<i", img, a - base + 1)[0]
        return a

    for i in range(n):
        t = d(vt + 4 * i)
        body = resolve(t)
        print(f"{i:2d} +0x{4 * i:02x} {t:#x} -> {body:#x} {names.get(body, names.get(t, '?'))}")


if __name__ == "__main__":
    main()
