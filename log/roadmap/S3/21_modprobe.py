"""S3.21 (U-1392): GL_MODULATE with Gouraud colours, through the real GL (glreplay.exe) and GlRaster, on port/test/glprobe.py's Scene.

Scenes (screen-aligned 2D, 48 quads of 90 x 90 px, culling off, lighting off):
  gour_const   per-corner random colours, a constant-value texture (every texel v), bilinear at 2 texels / px: pixel = v * colour
  const_rand   one random colour per quad, a random texture, bilinear: the filtered texel times a constant colour
  gour_rand    per-corner random colours, a random texture, bilinear
  gour_untex   per-corner random colours, texturing off (the colour alone)
    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21_modprobe.py [scene ...] [--knob i=v,...]
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P, lib as L  # noqa: E402


def scene(kind, seed=11, scale=2.0, w=90.0):
    rng = np.random.default_rng(seed)
    sc = P.Scene()
    sc.setup2d(1.0)
    if kind != "gour_untex":
        if kind == "gour_const":
            data = bytes([200, 170, 120]) * (64 * 64)
        else:
            data = rng.integers(0, 256, 64 * 64 * 3, dtype=np.uint8).tobytes()
        sc.texture(64, 64, data, mini=0x2601, mag=0x2601)
        sc.state.append("glEnable 0xde1")
    for gy in range(6):
        for gx in range(8):
            ox, oy = gx * 100 + 3 + rng.uniform(0, 1), gy * 100 + 3 + rng.uniform(0, 1)
            span = w * scale / 64.0
            u0, v0 = rng.uniform(0, 1), rng.uniform(0, 1)
            if kind == "const_rand":
                c = tuple(rng.uniform(0.3, 1.0, 3)) + (1.0,)
                cols = [c] * 4
            else:
                cols = [tuple(rng.uniform(0.3, 1.0, 3)) + (1.0,) for _ in range(4)]
            sc.quad(ox, oy, ox + w, oy + w, texcoords=[(u0, v0), (u0 + span, v0), (u0 + span, v0 + span), (u0, v0 + span)], colors=cols)
    return sc


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    knobs = {}
    for a in sys.argv[1:]:
        if a.startswith("--knob="):
            knobs = {int(k): int(v) for k, v in (kv.split("=") for kv in a[7:].split(","))}
    lib = L.load()
    for kind in args or ["gour_untex", "gour_const", "const_rand", "gour_rand"]:
        d = scene(kind).write("s321_" + kind)
        real = P.real_frame(d)
        port = P.port_frame(lib, d, knobs or None)
        P.report(real, port, False, kind)


if __name__ == "__main__":
    main()
