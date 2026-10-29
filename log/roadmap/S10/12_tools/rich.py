"""S10.12: print the Rich header (prodid, build, count) of a PE: ~/bin/py -3.12 12_tools/rich.py <pe> ..."""
import pathlib, sys
import pefile

for p in sys.argv[1:]:
    pe = pefile.PE(p, fast_load=True)
    rh = pe.parse_rich_header()
    print(p)
    vals = rh["values"]
    for i in range(0, len(vals), 2):
        cid, cnt = vals[i], vals[i + 1]
        print(f"  prodid {cid >> 16:3d} build {cid & 0xffff:5d} count {cnt}")
