"""S3.29 (U-1309): where GlRaster's one-level pixels of corpus passes lie inside their triangles (keep_float run): distance to the
nearest edge (px), the texture coordinates there (texel units) and their position in the texel grid, the rows from the triangle's top,
the triangle's size; plus the same statistics over all drawn pixels, for contrast.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/29_locate.py <corpus> [step ...] [--knob i=v,...]
"""
import ctypes as C
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glcorpus as G, glraster as R, lib as L  # noqa: E402

W, H = 800, 600
STEPS = ["sandbox", "zoom1", "z2_r1", "hole", "fuzz1_z0", "fuzz7_z1", "view3"]


def ch(v):
    return np.stack([((v >> s) & 0x1f).astype(np.int16) for s in (10, 5, 0)], axis=-1)


def run(lib, setup, step, knobs):
    dll = lib.dll
    R.bind(dll)
    dll.sg_raster_get_tri_ids.restype, dll.sg_raster_get_tri_ids.argtypes = C.c_size_t, [C.c_void_p, C.c_void_p, C.c_size_t]
    dll.sg_raster_get_triangles.restype, dll.sg_raster_get_triangles.argtypes = C.c_size_t, [C.c_void_p, C.c_void_p, C.c_size_t]
    d = setup.root / "snaps" / step
    meta = G.meta(d / "meta.txt")
    r = dll.sg_raster_new(W, H)
    try:
        for h, b in setup.textures.items():
            dll.sg_raster_texture(r, h.encode(), b, len(b))
        state = "\n".join(setup.lines[setup.ctx_line + 1:meta["stateIdx"]]).encode()
        dll.sg_raster_exec(r, state, len(state))
        vtx, nrm = (d / "vtx.bin").read_bytes(), (d / "nrm.bin").read_bytes()
        dll.sg_raster_arrays(r, vtx, len(vtx) // 12, nrm, len(nrm) // 12)
        pre = (d / "dib_pre.bin").read_bytes()
        post = np.frombuffer((d / "dib_post.bin").read_bytes(), dtype="<u2")
        dll.sg_raster_set_pixels(r, pre, W * H)
        dll.sg_raster_keep_float(r, 1)
        t = "".join(f"#tune {i} {v}\n" for i, v in knobs.items()).encode()
        if t:
            dll.sg_raster_exec(r, t, len(t))
        pas = (d / "pass.gl.txt").read_bytes()
        dll.sg_raster_exec(r, pas, len(pas))
        buf = C.create_string_buffer(W * H * 2)
        dll.sg_raster_get_pixels(r, buf, W * H * 2)
        got = np.frombuffer(buf.raw, dtype="<u2")
        ids = np.zeros(W * H, np.int32)
        dll.sg_raster_get_tri_ids(r, ids.ctypes.data, W * H)
        n = dll.sg_raster_get_triangles(r, None, 0)
        tri = np.zeros(max(n, 1), np.float64)
        dll.sg_raster_get_triangles(r, tri.ctypes.data, n)
        return post.reshape(H, W), got.reshape(H, W), ids.reshape(H, W), tri.reshape(-1, 12)
    finally:
        dll.sg_raster_free(r)


def edge(ax, ay, bx, by, px, py):
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax)


def info(T, x, y):
    xs, ys = T[0:6:2], T[1:6:2]
    ss, ts = T[6:12:2], T[7:12:2]
    px, py = x + 0.5, y + 0.5
    ar = edge(xs[0], ys[0], xs[1], ys[1], xs[2], ys[2])
    w = [edge(xs[1], ys[1], xs[2], ys[2], px, py), edge(xs[2], ys[2], xs[0], ys[0], px, py), edge(xs[0], ys[0], xs[1], ys[1], px, py)]
    lens = [np.hypot(xs[2] - xs[1], ys[2] - ys[1]), np.hypot(xs[0] - xs[2], ys[0] - ys[2]), np.hypot(xs[1] - xs[0], ys[1] - ys[0])]
    dist = min(abs(w[i]) / lens[i] for i in range(3) if lens[i] > 0)
    u = (w[0] * ss[0] + w[1] * ss[1] + w[2] * ss[2]) / ar * 64
    v = (w[0] * ts[0] + w[1] * ts[1] + w[2] * ts[2]) / ar * 64
    return dist, u, v, max(ys) - py, abs(ar) / 2


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    knobs = {}
    for a in sys.argv[1:]:
        if a.startswith("--knob="):
            knobs = {int(k): int(v) for k, v in (kv.split("=") for kv in a[7:].split(","))}
    corpus = args[0]
    steps = args[1:] or STEPS
    lib = L.load()
    setup = R.Setup(pathlib.Path.cwd() / "port" / "corpus" / "gl" / corpus)
    bad, allp = [], []
    for s in steps:
        post, got, ids, tri = run(lib, setup, s, knobs)
        d = np.abs(ch(post) - ch(got)).max(-1)
        ys, xs = np.nonzero(d == 1)
        for y, x in zip(ys, xs):
            ti = ids[y, x]
            if ti >= 0:
                bad.append(info(tri[ti], x, y))
        rng = np.random.default_rng(1)
        dy, dx = np.nonzero(ids >= 0)
        pick = rng.choice(len(dy), size=min(3000, len(dy)), replace=False)
        for k in pick:
            allp.append(info(tri[ids[dy[k], dx[k]]], dx[k], dy[k]))
    B, A = np.array(bad), np.array(allp)
    for name, X in (("one-level", B), ("all drawn (sample)", A)):
        dist, u, v, rows, area = X.T
        fu, fv = (u - 0.5) % 1.0, (v - 0.5) % 1.0
        print(f"{name}: n {len(X)}")
        print("   edge distance px quantiles", np.quantile(dist, [0.1, 0.25, 0.5, 0.75, 0.9]).round(3).tolist())
        print("   share within 0.5 / 1 / 2 px of an edge", [round(float((dist < q).mean()), 3) for q in (0.5, 1, 2)])
        print("   count within 0.005 px of an edge", int((dist < 0.005).sum()), " within 0.02", int((dist < 0.02).sum()))
        print("   u range", float(u.min()), float(u.max()), " v range", float(v.min()), float(v.max()))
        print("   u in [0,1) or (63,64]:", round(float(((u < 1) | (u > 63)).mean()), 3), " v:", round(float(((v < 1) | (v > 63)).mean()), 3))
        print("   frac(u - 0.5) within 0.01 of 0/1:", round(float(((fu < 0.01) | (fu > 0.99)).mean()), 3),
              " frac(v - 0.5):", round(float(((fv < 0.01) | (fv > 0.99)).mean()), 3))
        print("   rows from top quantiles", np.quantile(rows, [0.1, 0.5, 0.9]).round(1).tolist(), " area quantiles",
              np.quantile(area, [0.1, 0.5, 0.9]).round(1).tolist())


if __name__ == "__main__":
    main()
