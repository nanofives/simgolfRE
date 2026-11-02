"""S3.29 (U-1309): where GlRaster's remaining one-level pixels sit on the recorded passes: by filter (rho <= 1 nearest = the
terrain's GL_NEAREST magnification, else GL_LINEAR minification), textured or not, blended or not, and by the margin of the port's
pre-quantisation value to its 5-bit boundary (8-bit units).

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/29_resid.py [corpus] [step ...] [--knob i=v,...]
"""
import ctypes as C
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glcorpus as G, glraster as R, lib as L  # noqa: E402

W, H = 800, 600
KD = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])
STEPS = ["sandbox", "zoom1", "z2_r1", "hole", "fuzz1_z0", "fuzz7_z1", "view3"]


def ch(v):
    return np.stack([((v >> s) & 0x1f).astype(np.int16) for s in (10, 5, 0)], axis=-1)


def run(lib, setup, step, knobs):
    dll = lib.dll
    R.bind(dll)
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
        fb = C.create_string_buffer(W * H * 12)
        dll.sg_raster_get_float(r, fb, W * H * 12)
        sb = C.create_string_buffer(W * H * 32)
        dll.sg_raster_get_source(r, sb, W * H * 32)
        rho = np.zeros(W * H, np.float32)
        dll.sg_raster_get_rho(r, rho.ctypes.data, W * H * 4)
        return (post.reshape(H, W), got.reshape(H, W), np.frombuffer(fb.raw, "<f4").reshape(H, W, 3),
                np.frombuffer(sb.raw, "<f4").reshape(H, W, 8), rho.reshape(H, W))
    finally:
        dll.sg_raster_free(r)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    knobs = {}
    for a in sys.argv[1:]:
        if a.startswith("--knob="):
            knobs = {int(k): int(v) for k, v in (kv.split("=") for kv in a[7:].split(","))}
    corpus = args[0] if args else "c7"
    steps = args[1:] or STEPS
    lib = L.load()
    setup = R.Setup(pathlib.Path.cwd() / "port" / "corpus" / "gl" / corpus)
    tot = {}
    for s in steps:
        post, got, ffb, sfb, rho = run(lib, setup, s, knobs)
        P, Gt = ch(post), ch(got)
        d = np.abs(P - Gt)
        one = (d.max(-1) == 1)
        ys, xs = np.nonzero(one)
        k = KD[ys & 3, xs & 3]
        row = {"n": len(ys)}
        lin = rho[ys, xs] > 1.0 + 1e-6
        blend = sfb[ys, xs, 7] > 0.5
        white = (sfb[ys, xs, 0] == 255) & (sfb[ys, xs, 1] == 255) & (sfb[ys, xs, 2] == 255)
        row["nearest"] = int((~lin).sum()); row["linear"] = int(lin.sum())
        row["blend"] = int(blend.sum()); row["white_texel"] = int(white.sum())
        # margin of the differing channel(s)
        ms = []
        for y, x, kk in zip(ys, xs, k):
            for c in range(3):
                if d[y, x, c] == 1:
                    v = float(ffb[y, x, c])
                    q = v * 31 / 255 + (kk + 0.5) / 16
                    ms.append(abs(q - np.floor(q + 0.5)) * 255 / 31)
        ms = np.array(ms)
        row["m<0.02"] = int((ms < 0.02).sum()); row["m<0.1"] = int(((ms >= 0.02) & (ms < 0.1)).sum())
        row["m>=0.1"] = int((ms >= 0.1).sum())
        row["up"] = int((Gt - P == 1).sum()); row["down"] = int((Gt - P == -1).sum())
        row["beyond"] = int((d.max(-1) > 1).sum())
        print(s, row, flush=True)
        for kk, v in row.items():
            tot[kk] = tot.get(kk, 0) + v
    print("TOTAL", tot)


if __name__ == "__main__":
    main()
