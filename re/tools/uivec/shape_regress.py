"""P41: compare one renderer change shape by shape. Collects every shape of the given types from every scene JSON under the
given roots and evaluates the old and the new distance function on a 4x grid over the shape's own extent.

py -3.12 re/tools/uivec/shape_regress.py <old sdf.py> <type,type> <root> [<root> ...]
Prints one line per file that holds a changed shape: shapes changed / shapes of those types, the largest area change in
px^2 (coverage, both ways) and the largest distance change inside the shape (what a bevel or a soft edge sees).
"""
import glob
import importlib.util
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sdf as NEW  # noqa: E402


def load_old(path):
    spec = importlib.util.spec_from_file_location("sdf_old", path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def walk(o, types, out):
    if isinstance(o, dict):
        if isinstance(o.get("t"), str) and o["t"] in types and ("pts" in o or "a" in o):
            out.append(o)
        for v in o.values():
            walk(v, types, out)
    elif isinstance(o, list):
        for v in o:
            walk(v, types, out)


def extent(S):
    pts = S["pts"] if S["t"] == "stroke" else [S["a"], S["b"]]
    p = np.array(pts, float)
    m = max(S.get("w", 1.0), S.get("hw", 0.0)) + 3.0
    return p[:, 0].min() - m, p[:, 1].min() - m, p[:, 0].max() + m, p[:, 1].max() + m


def main():
    old = load_old(sys.argv[1])
    types = set(sys.argv[2].split(","))
    files = []
    for root in sys.argv[3:]:
        files += glob.glob(os.path.join(root, "**", "*.json"), recursive=True)
    total = changed_files = changed_shapes = 0
    seen = {}
    for k, f in enumerate(sorted(files)):
        try:
            data = json.load(open(f, encoding="utf-8"))
        except Exception:
            continue
        shapes = []
        walk(data, types, shapes)
        if not shapes:
            continue
        n_ch, worst_a, worst_d = 0, 0.0, 0.0
        for S in shapes:
            key = json.dumps(S, sort_keys=True)
            if key not in seen:
                x0, y0, x1, y1 = extent(S)
                s = 4
                xs = np.arange(x0, x1, 1.0 / s) + 0.5 / s
                ys = np.arange(y0, y1, 1.0 / s) + 0.5 / s
                x, y = np.meshgrid(xs, ys)
                a, b = old.sd(S, x, y), NEW.sd(S, x, y)
                area = float(((a < 0) != (b < 0)).sum()) / (s * s)
                inside = (a < 0) & (b < 0)
                dd = float(np.abs(a - b)[inside].max()) if inside.any() else 0.0
                seen[key] = (area, dd)
            area, dd = seen[key]
            total += 1
            if area > 0.05 or dd > 0.05:
                n_ch += 1
                worst_a, worst_d = max(worst_a, area), max(worst_d, dd)
        if n_ch:
            changed_files += 1
            changed_shapes += n_ch
            print(f"{os.path.relpath(f)}: {n_ch}/{len(shapes)} changed, area {worst_a:.2f} px2, distance {worst_d:.2f} px")
    print(f"TOTAL {total} shapes in {len(files)} files; {changed_shapes} changed in {changed_files} files")


if __name__ == "__main__":
    main()
