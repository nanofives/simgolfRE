"""S3.6: jgld.dll's Sprite vtable (0x1011d380) slot by slot, each slot's incremental-link thunk followed to its body and
named from re/functions_ghidra_jgld.dll.tsv. Read-only over original/jgld.dll.

    ~/bin/py -3.12 log/roadmap/S3/6_vtable.py [vtable_rva_hex] [slots]
"""
import csv
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
import pefile  # noqa: E402

pe = pefile.PE(str(ROOT / "original" / "jgld.dll"), fast_load=True)
img = pe.get_memory_mapped_image()
names = {}
with open(ROOT / "re" / "functions_ghidra_jgld.dll.tsv", encoding="utf-8") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        names[int(r["entry"], 16)] = r["name"]
vt = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x11D380
n = int(sys.argv[2]) if len(sys.argv) > 2 else 48
for k in range(n):
    va = struct.unpack_from("<I", img, vt + 4 * k)[0]
    rva = va - 0x10000000
    tgt = rva
    if img[rva] == 0xE9:                         # jmp rel32 thunk
        tgt = rva + 5 + struct.unpack_from("<i", img, rva + 1)[0]
    print(f"{k:2d} +0x{4 * k:02x}  thunk 0x{rva:05x} -> 0x{tgt:05x} {names.get(tgt, '?')}")
