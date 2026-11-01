"""S3.21: the triangles under GlRaster's one-level pixels of a corpus pass (keep_float run).

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21_tris.py [corpus] step
Prints the vertex window coordinates (raw, before GlRaster's fixed-point setup), the 256 * x' value of each vertex under the
section-18 model (x' = c + (x - c) * (1 - 2^-8 / h)) and its distance from the nearest rounding tie, for the triangles holding
differing pixels, and the same statistic over all drawn triangles.
"""
import collections
import ctypes as C
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import glraster as R  # noqa: E402
from port.test import glcorpus as G  # noqa: E402

W, H = 800, 600


def ch(v):
    return np.stack([((v >> s) & 0x1f).astype(np.int16) for s in (10, 5, 0)], axis=-1)


def run_ids(lib, setup, step):
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
        pas = (d / "pass.gl.txt").read_bytes()
        dll.sg_raster_exec(r, pas, len(pas))
        buf = C.create_string_buffer(W * H * 2)
        dll.sg_raster_get_pixels(r, buf, W * H * 2)
        got = np.frombuffer(buf.raw, dtype="<u2")
        ids = np.zeros(W * H, np.int32)
        dll.sg_raster_get_tri_ids(r, ids.ctypes.data, W * H)
        n = dll.sg_raster_get_triangles(r, None, 0)
        tris = np.zeros(n, np.float64)
        dll.sg_raster_get_triangles(r, tris.ctypes.data, n)
        sb = C.create_string_buffer(W * H * 32)
        dll.sg_raster_get_source(r, sb, W * H * 32)
        fb = C.create_string_buffer(W * H * 12)
        dll.sg_raster_get_float(r, fb, W * H * 12)
        return (post, got, ids, tris.reshape(-1, 12), np.frombuffer(fb.raw, dtype="<f4").reshape(H, W, 3))
    finally:
        dll.sg_raster_free(r)


def main():
    args = sys.argv[1:]
    corpus, step = (args[0], args[1]) if len(args) > 1 else ("c7", args[0])
    setup = R.Setup(ROOT / "port" / "corpus" / "gl" / corpus)
    lib = L.load()
    post, got, ids, tris, ff = run_ids(lib, setup, step)
    d = np.abs(ch(got) - ch(post)).max(-1)
    bad = np.nonzero(d == 1)[0]
    print(step, "triangles", len(tris), "one-level", len(bad))
    tc = collections.Counter(int(ids[i]) for i in bad)
    print("distinct triangles with bad px", len(tc))
    for tid, n in tc.most_common(25):
        t = tris[tid]
        xs = t[0:6:2]; ys = t[1:6:2]
        print(f" tri {tid:6d} bad {n:3d} x {xs.round(5).tolist()} y {ys.round(5).tolist()} st {t[6:].round(5).tolist()}")
    # where in x are the vertices of all triangles: fraction of x
    fr = collections.Counter(round(float(x) % 1.0, 4) for x in tris[:, 0:6:2].ravel())
    print("x fractions (top)", fr.most_common(8))
    fy = collections.Counter(round(float(y) % 1.0, 4) for y in tris[:, 1:6:2].ravel())
    print("y fractions (top)", fy.most_common(8))


def model_frac(v, c, h):
    xp = c + (v - c) * (1.0 - 1.0 / (256.0 * h))
    f = 256.0 * xp
    return f - np.floor(f)


def classify(corpus, steps):
    setup = R.Setup(ROOT / "port" / "corpus" / "gl" / corpus)
    lib = L.load()
    tot = collections.Counter()
    drawn = collections.Counter()
    for step in steps:
        post, got, ids, tris, ff = run_ids(lib, setup, step)
        d = np.abs(ch(got) - ch(post)).max(-1)
        xs = tris[:, 0:6:2]; ys = tris[:, 1:6:2]
        clip = ((xs < 0) | (xs > 800) | (ys < 0) | (ys > 600)).any(1)
        fx = model_frac(xs, 400.0, 400.0); fy = model_frac(ys, 300.0, 300.0)
        tie = ((np.abs(fx - 0.5) < 0.03) | (np.abs(fy - 0.5) < 0.03)).any(1)
        cls = np.where(clip, 0, np.where(tie, 1, 2))
        valid = ids >= 0
        pc = np.full(ids.shape, -1); pc[valid] = cls[ids[valid]]
        c = collections.Counter(int(pc[i]) for i in np.nonzero(d == 1)[0])
        dc = collections.Counter(int(v) for v in pc[valid])
        tot.update(c); drawn.update(dc)
        print(f"{step:20s} bad clip {c[0]:4d} tie {c[1]:4d} other {c[2]:4d} none {c[-1]:3d} | drawn clip {dc[0]} tie {dc[1]} other {dc[2]}", flush=True)
    print("TOTAL bad", dict(tot), "drawn", dict(drawn))


def snaperr(corpus, steps):
    """bad-pixel rate per triangle as a function of the largest |snap error| of its vertices (in 1/256 units, 0 .. 0.5)"""
    setup = R.Setup(ROOT / "port" / "corpus" / "gl" / corpus)
    lib = L.load()
    bins = np.linspace(0.45, 0.5, 11)
    badc = np.zeros(10); drawc = np.zeros(10); clipb = clipd = 0
    for step in steps:
        post, got, ids, tris, ff = run_ids(lib, setup, step)
        d = np.abs(ch(got) - ch(post)).max(-1)
        xs = tris[:, 0:6:2]; ys = tris[:, 1:6:2]
        clip = ((xs < 0) | (xs > 800) | (ys < 0) | (ys > 600)).any(1)
        ex = np.abs(model_frac(xs, 400.0, 400.0) - 0.5); ey = np.abs(model_frac(ys, 300.0, 300.0) - 0.5)
        # snap error = 0.5 - distance from tie; take the max over the 6 coordinates
        se = (0.5 - np.minimum(ex, ey).min(1))
        b = np.clip(np.digitize(se, bins) - 1, 0, 9)
        valid = ids >= 0
        tb = np.full(ids.shape, -1); tb[valid] = np.where(clip[ids[valid]], -2, b[ids[valid]])
        for v in tb[valid]:
            if v >= 0: drawc[v] += 1
            else: clipd += 1
        for i in np.nonzero(d == 1)[0]:
            v = tb[i]
            if v >= 0: badc[v] += 1
            elif v == -2: clipb += 1
    for k in range(10):
        print(f"  max snap err {bins[k]:.2f}..{bins[k+1]:.2f}: bad {int(badc[k]):5d} drawn {int(drawc[k]):8d} rate {badc[k] / max(drawc[k], 1) * 1e4:7.2f} /1e4")
    print(f"  clipped: bad {clipb} drawn {clipd} rate {clipb / max(clipd, 1) * 1e4:.2f} /1e4")


if __name__ == "__main__":
    if sys.argv[1] == "snaperr":
        root = ROOT / "port" / "corpus" / "gl" / sys.argv[2]
        snaperr(sys.argv[2], sys.argv[3:] or sorted(p.name for p in (root / "snaps").iterdir()))
    elif sys.argv[1] == "classify":
        root = ROOT / "port" / "corpus" / "gl" / sys.argv[2]
        classify(sys.argv[2], sys.argv[3:] or sorted(p.name for p in (root / "snaps").iterdir()))
    else:
        main()
