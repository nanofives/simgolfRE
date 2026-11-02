"""S3.29 (U-1309): the blend arithmetic of the generic GL's float path, fitted on 29_probes.py's fin_blend scene (16 x 16 RGBA-textured
quads, one texel each, blended SRC_ALPHA / ONE_MINUS_SRC_ALPHA over a flat background whose real 5-bit values come from the
bg_only scene). Each variant gives the 5-bit value of every quad pixel from the texel (byte t, alpha byte A), the quad's float32
colour c, the destination D (5-bit) and the dither offset d; printed: wrong pixels per variant.

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_blendfit.py
"""
import pathlib
import struct
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P  # noqa: E402

f32 = np.float32
K = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])
SHOW = int(sys.argv[1]) if len(sys.argv) > 1 else 0
SHOWN = []


def fb(h):
    return struct.unpack("<f", struct.pack("<I", int(h, 16)))[0]


def variants(t, A, c, D, d):
    F = f32
    out = {}
    tn = F(F(t) / F(255.0))
    an = F(F(A) / F(255.0))
    c31 = F(F(c) * F(31.0))
    src = F(c31 * tn)
    aa = A / 255.0
    out["glraster"] = np.floor(((t * float(c)) * aa + D * 255.0 / 31.0 * (1 - aa)) * 31 / 255 + d)
    a = an
    oma = F(F(1.0) - a)
    out["f_sum"] = np.floor(F(F(F(src * a) + F(F(D) * oma)) + F(d)))
    out["f_lerp"] = np.floor(F(F(F(F(src - F(D)) * a) + F(D)) + F(d)))
    out["f_one"] = np.floor(F(F(float(src) * float(a) + float(D) * float(oma)) + F(d)))
    # alpha modulated in the 5-bit scale first (alpha * 31 / 31)
    a31 = F(F(an * F(31.0)) / F(31.0))
    out["f_a31"] = np.floor(F(F(F(src * a31) + F(F(D) * F(F(1.0) - a31))) + F(d)))
    # alpha from the texel byte with the colour alpha 1.0 scaled to 255: a = (A * 255 / 255) / 255
    a255 = F(F(F(A) * F(1.0)) / F(255.0))
    out["f_a255"] = np.floor(F(F(F(src * a255) + F(F(D) * F(F(1.0) - a255))) + F(d)))
    # the destination expanded to 8 bits first, the sum in 8 bits, then to 5 bits
    D8 = F(F(D) * F(255.0) / F(31.0))
    s8 = F(F(c) * F(t))
    out["f_8bit"] = np.floor(F(F(F(F(s8 * a) + F(D8 * oma)) * F(31.0) / F(255.0)) + F(d)))
    # the blend in the [0, 1] scale: source c * t / 255, destination D / 31, result times 31
    srcn = F(F(c) * tn)
    Dn = F(F(D) / F(31.0))
    out["f_n01"] = np.floor(F(F(F(F(srcn * a) + F(Dn * oma)) * F(31.0)) + F(d)))
    out["f_n01_lerp"] = np.floor(F(F(F(F(F(srcn - Dn) * a) + Dn) * F(31.0)) + F(d)))
    am = F(F(A) * F(1.0 / 255.0))
    out["f_sum_am"] = np.floor(F(F(F(src * am) + F(F(D) * F(F(1.0) - am))) + F(d)))
    # destination factor times destination first (dst * (1 - a)) then plus source * a (the same sum, other order of rounding)
    out["f_sum_ds"] = np.floor(F(F(F(F(D) * oma) + F(src * a)) + F(d)))
    # 31 / 255 scaling of the alpha-weighted 8-bit source
    out["f_src8"] = np.floor(F(F(F(F(F(F(c) * F(t)) * a) * F(31.0 / 255.0)) + F(F(D) * oma)) + F(d)))
    oma2 = F(F(F(255.0) - F(A)) / F(255.0))
    out["f_sum_oma255"] = np.floor(F(F(F(src * a) + F(F(D) * oma2)) + F(d)))
    out["f_sum_dfirst"] = np.floor(F(F(src * a) + F(F(F(D) * oma) + F(d))))
    out["f_sum_d3"] = np.floor(F(F(F(src * a) + F(d)) + F(F(D) * oma)))
    # the source scaled by alpha before the modulate: (c31 * (tn * a))
    out["f_sum_ta"] = np.floor(F(F(F(c31 * F(tn * a)) + F(F(D) * oma)) + F(d)))
    out["f_sum_ta2"] = np.floor(F(F(F(c31 * F(tn * a)) + F(F(D) * oma2)) + F(d)))
    return out


def main():
    d = P.PROBE / "s329_fin_blend"
    real = np.frombuffer((d / "snaps" / "s1" / "dib_post.bin").read_bytes(), "<u2").reshape(600, 800)
    bg = np.frombuffer((P.PROBE / "s329_bg_only" / "snaps" / "s1" / "dib_post.bin").read_bytes(), "<u2").reshape(600, 800)
    R5 = np.stack([(real >> 10) & 31, (real >> 5) & 31, real & 31], -1)
    B5 = np.stack([(bg >> 10) & 31, (bg >> 5) & 31, bg & 31], -1)
    tex = np.frombuffer(sorted((d / "tex").glob("*.bin"))[0].read_bytes(), np.uint8).reshape(64, 64, 4)
    lines = (d / "snaps" / "s1" / "pass.gl.txt").read_text().splitlines()
    quads, col, tc, n = [], None, None, 0
    tri = []
    for ln in lines:
        p = ln.split(" ", 1)
        if p[0] == "glColor4f":
            col = [fb(h) for h in p[1].split(",")]
        elif p[0] == "glTexCoord2f":
            tc = [fb(h) for h in p[1].split(",")]
        elif p[0] == "glVertex3f":
            tri.append(([fb(h) for h in p[1].split(",")], col, tc))
        elif p[0] == "glEnd":
            if tri[0][2] is not None and len(quads) % 1 == 0:
                quads.append(tri)
            tri = []
    # every second triangle starts a quad (glprobe.quad emits two); the first triangle's first vertex is (x0, y0)
    tot = {}
    npx = 0
    for q in quads[::2]:
        (x0, y0, _), c, (s_, t_) = q[0]
        x0, y0 = int(round(x0)), int(round(y0))
        i, j = int(s_ * 64), int(t_ * 64)
        tx = tex[j, i]
        for yy in range(y0, y0 + 16):
            for xx in range(x0, x0 + 16):
                dd = (K[yy & 3][xx & 3] + 0.5) / 16
                npx += 1
                for ch in range(3):
                    v = variants(float(tx[ch]), float(tx[3]), f32(c[ch]), float(B5[yy, xx, ch]), dd)
                    for nm, val in v.items():
                        tot[nm] = tot.get(nm, 0) + int(int(min(31, max(0, val))) != int(R5[yy, xx, ch]))
                    if SHOW and int(min(31, max(0, v["f_sum"]))) != int(R5[yy, xx, ch]) and len(SHOWN) < SHOW:
                        ex = float(c[ch]) * 31 * float(tx[ch]) / 255 * float(tx[3]) / 255 + float(B5[yy, xx, ch]) * (1 - float(tx[3]) / 255) + dd
                        SHOWN.append((xx, yy, ch, int(tx[ch]), int(tx[3]), int(B5[yy, xx, ch]), float(c[ch]), ex, int(v["f_sum"]), int(R5[yy, xx, ch])))
    print("quad pixels", npx)
    for nm, v in tot.items():
        print(f"  {nm:10s} wrong channel values {v}")
    for r in SHOWN:
        print("   (x %d, y %d) ch %d t %d A %d D %d c %.7f exact q %.6f f_sum %d real %d" % r)


if __name__ == "__main__":
    main()
