"""Print a sheet's pixel column or row: prof.py <sheet> col <x> <y0> <y1> | row <y> <x0> <x1>"""
import sys
import common as c
rel, kind, a, b, e = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
rgb, m, idx = c.sheet(rel)
for t in range(b, e):
    x, y = (a, t) if kind == "col" else (t, a)
    print(f"{x},{y}", tuple(int(v) for v in rgb[y, x]) if m[y, x] else "-")
