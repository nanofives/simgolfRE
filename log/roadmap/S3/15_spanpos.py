"""S3.15 (U-1392): where inside its triangle each pixel GlRaster gets wrong lies (against glreplay's real software GL), compared
with every drawn pixel: column index from the span's first pixel and row index from the triangle's top row. An error that grows
with either points at an incremental (stepped) evaluation of the attributes in the real GL.

    cd port/build/S3_15 && SG_PORTSIM_DLL=... S31B_SCRATCH=... PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/15_spanpos.py c7 fuzz1_z0__tex
"""
import ctypes as C
import os
import pathlib
import sys

import numpy as np

from port.test import glcorpus as G, glraster as R, lib as L

SCR = pathlib.Path(os.environ["S31B_SCRATCH"])
W, H = 800, 600


def rgb(a):
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=-1).astype(int)


def run(lib, setup, step):
    dll = lib.dll
    R.bind(dll)
    dll.sg_raster_get_tri_ids.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t]
    dll.sg_raster_get_triangles.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t]
    dll.sg_raster_get_triangles.restype = C.c_size_t
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
        ids = (C.c_int32 * (W * H))()
        dll.sg_raster_get_tri_ids(r, ids, W * H)
        n = dll.sg_raster_get_triangles(r, None, 0)
        tr = (C.c_double * n)()
        dll.sg_raster_get_triangles(r, tr, n)
        return (post, np.frombuffer(buf.raw, dtype="<u2"), np.frombuffer(ids, dtype=np.int32).reshape(H, W),
                np.frombuffer(tr, dtype=np.float64).reshape(-1, 12))
    finally:
        dll.sg_raster_free(r)


def main():
    label, step = sys.argv[1], sys.argv[2]
    lib = L.load()
    post, got, ids, tris = run(lib, R.Setup(SCR / label), step)
    P, Gt = rgb(post).reshape(H, W, 3), rgb(got).reshape(H, W, 3)
    bad = (np.abs(P - Gt).max(2) == 1)
    ys, xs = np.nonzero(ids >= 0)
    col, row = [], []
    for y, x in zip(ys, xs):
        t = tris[ids[y, x]]
        vx, vy = t[0:6:2], t[1:6:2]
        top = np.ceil(vy.min() - 0.5)
        # span start on this row: the smallest x of the triangle at y + 0.5 (intersection of the edges with the row)
        yc = y + 0.5
        xsx = []
        for i in range(3):
            ax, ay, bx, by = vx[i], vy[i], vx[(i + 1) % 3], vy[(i + 1) % 3]
            if (ay <= yc < by) or (by <= yc < ay):
                xsx.append(ax + (yc - ay) * (bx - ax) / (by - ay))
        xl = min(xsx) if xsx else x + 0.5
        col.append(x - int(np.ceil(xl - 0.5)))
        row.append(int(y - top))
    col, row = np.array(col), np.array(row)
    b = bad[ys, xs]
    print(f"{label}/{step}: drawn {len(xs)} one-level {b.sum()}")
    for name, v in (("column from span start", col), ("row from top", row)):
        edges = [0, 1, 2, 4, 8, 12, 16, 24, 32, 48, 64, 1 << 20]
        print(" ", name)
        for lo, hi in zip(edges[:-1], edges[1:]):
            m = (v >= lo) & (v < hi)
            if m.sum():
                print(f"    [{lo:3d},{hi:7d})  px {m.sum():7d}  bad {b[m].sum():5d}  rate {1e4 * b[m].sum() / m.sum():7.2f} per 10k")


if __name__ == "__main__":
    main()
