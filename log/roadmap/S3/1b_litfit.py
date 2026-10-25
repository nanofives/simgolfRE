"""S3.1b (U-1300): which lighting arithmetic reproduces the real software GL on constant-colour lit quads (glprobe untex lit).
Each 16 x 16 quad covers every dither cell 16 times, so its real 5-bit values bound the real colour per channel to an interval
[lo, hi) of width 1/16 level (c * 31 / 255 + (k + 0.5) / 16 floored). A model is scored by how many quad-channels it puts
outside their interval.

    cd port/build/S3_head && PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_litfit.py [terrain|dif|spe|amb] [seed]
"""
import struct
import sys

import numpy as np

from port.test import glprobe as GP

K = GP.K


def bits(h):
    return struct.unpack("<f", struct.pack("<I", h))[0]


LDIR = np.array([bits(0xbf1b7a09), bits(0x3f44d46b), bits(0x3e4cc8db)])
LAMB = np.array([bits(0x3f48c8c9), bits(0x3f48c8c9), bits(0x3f34b4b5)])
LDIF = np.array([bits(0x3f70f0f1)] * 3)
LSPE = np.array([bits(0x3f800000), bits(0x3f800000), bits(0x3f75f5f6)])
Z = np.zeros(3)
CONFIGS = {"terrain": (LAMB, LDIF, LSPE), "dif": (Z, LDIF, Z), "spe": (Z, Z, LSPE), "amb": (LAMB, Z, Z),
           "difw": (Z, np.ones(3), Z), "spew": (Z, Z, np.ones(3))}


def intervals(real, quads, side=16):
    r5 = GP.rgb5(real).reshape(GP.H, GP.W, 3)
    yy, xx = np.mgrid[0:GP.H, 0:GP.W]
    d = (K[yy & 3, xx & 3] + 0.5) / 16.0
    out = []
    for x, y, _ in quads:
        L = r5[y:y + side, x:x + side]
        dd = d[y:y + side, x:x + side][..., None]
        lo = (L - dd).max(axis=(0, 1))       # real c*31/255 >= L - d  for every cell
        hi = (L + 1 - dd).min(axis=(0, 1))   # real c*31/255 <  L + 1 - d
        out.append((lo, hi, L))
    return out


def lighting(n, amb, dif, spe, spec=lambda nh: nh ** 13, ndl=None):
    l = LDIR / np.linalg.norm(LDIR)
    col = np.full(3, 0.04) + amb * 0.2
    ndotl = max(0.0, float(np.dot(n, l))) if ndl is None else ndl(n, l)
    col = col + ndotl * dif * 0.8
    if ndotl > 0:
        h = l + np.array([0, 0, 1.0])
        h = h / np.linalg.norm(h)
        col = col + spec(max(0.0, float(np.dot(n, h)))) * spe
    return np.clip(col, 0, 1)


def score(ivs, quads, fn):
    bad, margins = 0, []
    for (lo, hi, L), (_, _, n) in zip(ivs, quads):
        v = fn(np.asarray(n, dtype=np.float64)) * 31.0
        for k in range(3):
            if L[..., k].min() == 31 or L[..., k].max() == 0:      # saturated: no information
                continue
            if not (lo[k] <= v[k] < hi[k]):
                bad += 1
                margins.append((lo[k] - v[k]) if v[k] < lo[k] else (hi[k] - v[k]))
    return bad, np.array(margins)


def main():
    cfg = sys.argv[1] if len(sys.argv) > 1 else "terrain"
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    amb, dif, spe = CONFIGS[cfg]
    light = GP.light_lines(amb=list(amb) + [1.0], dif=list(dif) + [1.0], spe=list(spe) + [1.0])
    sc, quads = GP.untex_scene("lit", seed=seed, light=light)
    d = sc.write(f"untex_lit_{cfg}_{seed}")
    real = GP.real_frame(d)
    ivs = intervals(real, quads)
    models = {"double": lambda n: lighting(n, amb, dif, spe)}
    for name, fn in models.items():
        bad, m = score(ivs, quads, fn)
        print(f"{cfg}/{seed} {name}: {bad} of {3 * len(quads)} quad-channels outside; margins quantiles",
              np.round(np.quantile(m, [0, .1, .5, .9, 1]), 4) if m.size else "-", "up", int((m > 0).sum()), "down", int((m < 0).sum()))
    return ivs, quads


if __name__ == "__main__":
    main()
