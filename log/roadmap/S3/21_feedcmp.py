"""S3.21: compare the real GL's feedback window coordinates (21_glfeed/glfeed.exe) with GlRaster's own vertex positions.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21_feedcmp.py <corpus> <step> <feed.f32>
"""
import collections
import pathlib
import struct
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import importlib.util  # noqa: E402

_s = importlib.util.spec_from_file_location("t21", pathlib.Path(__file__).resolve().parent / "21_tris.py")
T = importlib.util.module_from_spec(_s)
_s.loader.exec_module(T)

POLY, PASS = 0x0703, 0x0700


def read_feed(path):
    b = pathlib.Path(path).read_bytes()
    n = int(struct.unpack_from("<f", b, 0)[0])
    f = np.frombuffer(b, "<f4", count=n, offset=4)
    polys = []
    i = 0
    while i < n:
        tok = int(f[i])
        if tok == POLY:
            k = int(f[i + 1])
            v = f[i + 2:i + 2 + 11 * k].reshape(k, 11)
            polys.append(v.copy())
            i += 2 + 11 * k
        elif tok == PASS:
            i += 2
        else:
            raise ValueError(f"token {tok} at {i}")
    return polys


def main():
    corpus, step, feed = sys.argv[1:4]
    polys = read_feed(feed)
    sizes = collections.Counter(len(p) for p in polys)
    print("polygons", len(polys), "sizes", dict(sizes))
    xs = np.concatenate([p[:, 0] for p in polys]).astype(np.float64)
    ys = np.concatenate([p[:, 1] for p in polys]).astype(np.float64)
    for name, a in (("x", xs), ("y", ys)):
        fr = a * 256.0 - np.floor(a * 256.0)
        print(name, "fraction of 256*v: zero", int((fr == 0).sum()), "of", len(a), " examples", a[:6].tolist())
        for bits in (8, 9, 10, 11, 12, 14, 16):
            q = a * (1 << bits)
            print(f"   on 2^-{bits} grid: {int((q == np.round(q)).sum())}")
    # the port's vertices
    setup = T.R.Setup(T.ROOT / "port" / "corpus" / "gl" / corpus)
    lib = T.L.load()
    post, got, ids, tris, ff = T.run_ids(lib, setup, step)
    px = tris[:, 0:6:2].ravel(); py = tris[:, 1:6:2].ravel()
    # nearest feedback vertex to each port vertex (by position), report the offset statistics
    fv = {}
    for x, y in zip(xs, ys):
        fv.setdefault((round(x, 1), round(y, 1)), []).append((x, y))
    dx = []
    for x, y in zip(px, py):
        c = fv.get((round(x, 1), round(y, 1)))
        if not c:
            continue
        bx, by = min(c, key=lambda q: abs(q[0] - x) + abs(q[1] - y))
        dx.append((x, y, bx - x, by - y))
    dx = np.array(dx)
    print("matched port vertices", len(dx), "of", len(px))
    cols = collections.defaultdict(list)
    for x, y, ddx, ddy in dx:
        cols[round(x, 3)].append(ddx)
    print("x column: feedback x - port x (unique values per column)")
    for c in sorted(cols)[:80]:
        u = sorted(set(np.round(cols[c], 7)))
        print(f"  {c:10.4f} n {len(cols[c]):4d} d {u[:4]}")


if __name__ == "__main__" and len(sys.argv) == 4:
    main()


def model(v, c, h):
    return np.round(256.0 * (c + (v - c) * (1.0 - 1.0 / (256.0 * h)))) / 256.0


def check(corpus, step, feed):
    polys = read_feed(feed)
    allv = np.concatenate(polys)
    print("feedback x range", float(allv[:, 0].min()), float(allv[:, 0].max()), "y range", float(allv[:, 1].min()), float(allv[:, 1].max()))
    setup = T.R.Setup(T.ROOT / "port" / "corpus" / "gl" / corpus)
    lib = T.L.load()
    post, got, ids, tris, ff = T.run_ids(lib, setup, step)
    # match port triangles to feedback polygons by their (model-rounded) vertex set within 2/256
    fx = {}
    for k, p in enumerate(polys):
        key = tuple(sorted((round(float(a) * 16), round(float(b) * 16)) for a, b in p[:, :2]))
        fx.setdefault(key, []).append(k)
    nm = 0; mism = collections.Counter(); tot = collections.Counter(); unmatched = 0
    for t in tris:
        xs, ys = t[0:6:2], t[1:6:2]
        mx, my = model(xs, 400.0, 400.0), model(ys, 300.0, 300.0)
        key = tuple(sorted((round(float(a) * 16), round(float(b) * 16)) for a, b in zip(mx, my)))
        ks = fx.get(key)
        if not ks:
            unmatched += 1
            continue
        p = polys[ks[0]]
        nm += 1
        for a, b, ox, oy in zip(mx, my, xs, ys):
            j = int(np.argmin(np.abs(p[:, 0] - a) + np.abs(p[:, 1] - b)))
            ex, ey = float(p[j, 0]) - a, float(p[j, 1]) - b
            tot["x"] += 1; tot["y"] += 1
            if ex != 0: mism[("x", round(ex * 256))] += 1
            if ey != 0: mism[("y", round(ey * 256))] += 1
    print("port triangles", len(tris), "matched", nm, "unmatched", unmatched)
    print("vertex mismatches (axis, steps of 1/256):", dict(mism), "of", dict(tot))


if __name__ == "__main__" and len(sys.argv) > 4 and sys.argv[4] == "check":
    check(*sys.argv[1:4])
