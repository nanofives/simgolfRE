"""Print pixel rows / columns of a sheet as colours, for reading edges. Usage: rows.py <sheet> row|col <k> <a> <b>"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C
name, kind, k, a, b = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
rgb, m = C.sheet(name)
for t in range(a, b):
    p = rgb[k, t] if kind == "row" else rgb[t, k]
    mk = m[k, t] if kind == "row" else m[t, k]
    print(t, tuple(int(v) for v in p), "" if mk else "T")
