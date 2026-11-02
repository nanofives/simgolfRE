"""S3.29 (U-1309): textured probes for Windows' generic GL, replayed through re/tools/glexact/glreplay.exe (port/test/glprobe.py's
Scene), written to port/corpus/gl/probe/s329_<name>/ (real frame = out.bin and snaps/s1/dib_post.bin).

Scenes (2D ortho over the screen, one quad of two triangles unless named otherwise, GL_MODULATE, lighting off, culling off):
  uramp_lin / uramp_near   red texel = 4 * column, green = 4 * row, blue 128; white colour; s rises slowly along x and y
                           (filtered red = 4 * (u - 0.5) under GL_LINEAR, the texture coordinate read through the filter)
  vramp_lin                the same with t rising (green carries it)
  cramp_lin / cramp_near   constant texture (200, 170, 120); red colour ramp like 21_ramp.py dense (colour under texture)
  cramp_untex              the same colour ramp untextured (S3.21's ramp probe, for reference)
  tris_lin                 48 random triangles, random colours, the smooth texture at 2..4 texels / px (minification)

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_probes.py [name ...]
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P  # noqa: E402

X0, Y0, X1, Y1 = 0.37, 0.61, 799.43, 599.29


def smooth_tex():
    a = np.zeros((64, 64, 3), np.uint8)
    a[:, :, 0] = (4 * np.arange(64))[None, :]
    a[:, :, 1] = (4 * np.arange(64))[:, None]
    a[:, :, 2] = 128
    return a.tobytes()


def quad_scene(tex, mode, s_of, t_of, col_of):
    sc = P.Scene()
    sc.setup2d(1.0)
    if tex is not None:
        f = 0x2601 if mode == "lin" else 0x2600
        if mode == "mix":   # the terrain's filters: GL_NEAREST magnification, GL_LINEAR minification
            sc.texture(64, 64, tex, mag=0x2600, mini=0x2601)
        else:
            sc.texture(64, 64, tex, mag=f, mini=f)
        sc.state.append("glEnable 0xde1")
    pts = [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)]
    cols = [col_of(x, y) for x, y in pts]
    tcs = [(s_of(x, y), t_of(x, y)) for x, y in pts] if tex is not None else None
    sc.quad(X0, Y0, X1, Y1, colors=cols, texcoords=tcs)
    return sc


def scenes():
    out = {}
    # s in texels: u = U0 + gx * x + gy * y ; the filtered red is 4 * (u - 0.5): 3 dither steps (3 * 8.226 / 16 8-bit units)
    # across the screen in x, 7.3 down the rows
    step8 = 255.0 / 31.0 / 16.0
    U0 = 20.3
    gx = 3 * step8 / 4.0 / 800.0
    gy = 7.3 * step8 / 4.0 / 600.0
    s_of = lambda x, y: (U0 + gx * x + gy * y) / 64.0
    t_c = lambda x, y: 30.37 / 64.0
    white = lambda x, y: (1.0, 1.0, 1.0, 1.0)
    for m in ("lin", "near", "mix"):
        out["uramp_" + m] = quad_scene(smooth_tex(), m, s_of, t_c, white)
    out["vramp_lin"] = quad_scene(smooth_tex(), "lin", lambda x, y: 30.37 / 64.0, s_of, white)
    # the colour ramp of 21_ramp.py dense
    R0 = 0.5003
    cgx = 3.0 / (31 * 16) / 800.0
    cgy = 1.0 / (31 * 16) / 600.0 * 7.3
    col = lambda x, y: (R0 + cgx * (x - X0) + cgy * (y - Y0), 0.4, 0.4, 1.0)
    const = bytes([200, 170, 120]) * (64 * 64)
    for m in ("lin", "near", "mix"):
        out["cramp_" + m] = quad_scene(const, m, lambda x, y: 0.31, lambda x, y: 0.47, col)
    out["cramp_untex"] = quad_scene(None, "lin", None, None, col)
    # random triangles over the smooth texture, minified
    rng = np.random.default_rng(29)
    sc = P.Scene()
    sc.setup2d(1.0)
    sc.texture(64, 64, smooth_tex(), mag=0x2601, mini=0x2601)
    sc.state.append("glEnable 0xde1")
    for gyi in range(6):
        for gxi in range(8):
            ox, oy = gxi * 100 + 5, gyi * 100 + 5
            pts = [(ox + rng.uniform(0, 30), oy + rng.uniform(0, 30)), (ox + rng.uniform(60, 90), oy + rng.uniform(0, 40)),
                   (ox + rng.uniform(20, 90), oy + rng.uniform(50, 90))]
            cols = [tuple(rng.uniform(0.3, 1, 3)) + (1.0,) for _ in range(3)]
            k = rng.uniform(2, 4) / 64.0
            u0, v0 = rng.uniform(0.1, 0.4), rng.uniform(0.1, 0.4)
            tcs = [(u0 + (x - ox) * k, v0 + (y - oy) * k * 0.7) for x, y in pts]
            sc.tri(*pts, colors=cols, texcoords=tcs)
    out["tris_lin"] = sc
    return out


def main():
    want = sys.argv[1:]
    for name, sc in scenes().items():
        if want and name not in want:
            continue
        d = sc.write("s329_" + name)
        real = P.real_frame(d)
        print(name, "drawn", int((real != 0).sum()), flush=True)


if __name__ == "__main__":
    main()


def alt_tex():
    """red = 255 * (column & 1), green 128, blue 0: under GL_LINEAR at u steps of 2 texels per pixel the red value is 255 * alpha or
    255 * (1 - alpha), alpha = frac(u - 0.5), so u's error shows with a gain of 255 per texel"""
    a = np.zeros((64, 64, 3), np.uint8)
    a[:, :, 0] = (255 * (np.arange(64) & 1))[None, :]
    a[:, :, 1] = 128
    return a.tobytes()


def alt_scenes():
    """alt_big: u = 3.37 + (2 + eps) x + gy y texels (s up to 25), t on a texel centre (30.5 / 64: beta 0); eps / gy give 3 / 7.3
    dither steps of red across x / down the rows"""
    step8 = 255.0 / 31.0 / 16.0
    eps = 3 * step8 / 255.0 / 800.0
    gy = 7.3 * step8 / 255.0 / 600.0
    s_of = lambda x, y: (3.37 + (2.0 + eps) * x + gy * y) / 64.0
    return {"alt_big": quad_scene(alt_tex(), "lin", s_of, lambda x, y: 30.5 / 64.0, lambda x, y: (1.0, 1.0, 1.0, 1.0))}


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "alt":
    for name, sc in alt_scenes().items():
        d = sc.write("s329_" + name)
        real = P.real_frame(d)
        print(name, "drawn", int((real != 0).sum()), flush=True)
    sys.exit(0)


def perm_scenes():
    """perm_rand / perm_big: random triangles over a random texture (GL_LINEAR, 2..4 texels / px), random Gouraud colours, each
    triangle's vertex list rotated by a random k and its winding reversed at random (the gradient anchor and the walk start)"""
    out = {}
    for name, n, size, seed in (("perm_rand", 8, 90, 31), ("perm_big", 3, 260, 32)):
        rng = np.random.default_rng(seed)
        sc = P.Scene()
        sc.setup2d(1.0)
        sc.texture(64, 64, rng.integers(0, 256, 64 * 64 * 3, dtype=np.uint8).tobytes(), mag=0x2600, mini=0x2601)
        sc.state.append("glEnable 0xde1")
        cell = 800 // n
        for gyi in range(600 // cell):
            for gxi in range(n):
                ox, oy = gxi * cell + 5, gyi * cell + 5
                q = size / 90.0
                pts = [(ox + rng.uniform(0, 30) * q, oy + rng.uniform(0, 30) * q), (ox + rng.uniform(60, 90) * q, oy + rng.uniform(0, 40) * q),
                       (ox + rng.uniform(20, 90) * q, oy + rng.uniform(50, 90) * q)]
                cols = [tuple(rng.uniform(0.3, 1, 3)) + (1.0,) for _ in range(3)]
                k = rng.uniform(2, 4) / 64.0
                ang = rng.uniform(0, 2 * np.pi)
                u0, v0 = rng.uniform(0.0, 1.0), rng.uniform(0.0, 1.0)
                tcs = [(u0 + ((x - ox) * np.cos(ang) - (y - oy) * np.sin(ang)) * k, v0 + ((x - ox) * np.sin(ang) + (y - oy) * np.cos(ang)) * k)
                       for x, y in pts]
                idx = [0, 1, 2]
                r = int(rng.integers(0, 3))
                idx = idx[r:] + idx[:r]
                if rng.random() < 0.5:
                    idx = idx[::-1]
                sc.tri(*[pts[i] for i in idx], colors=[cols[i] for i in idx], texcoords=[tcs[i] for i in idx])
        out[name] = sc
    return out


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "perm":
    for name, sc in perm_scenes().items():
        d = sc.write("s329_" + name)
        real = P.real_frame(d)
        print(name, "drawn", int((real != 0).sum()), flush=True)
    sys.exit(0)


KD4 = [[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]]


def fin_scene(seed=41):
    """fin_near: 1,184 constant-colour 16 x 16 quads over a random texture (the terrain's filters, magnified: one texel per quad, s / t
    on its centre); each quad's colour c puts t * c * 31 / 255 + (k + 0.5) / 16 exactly on a 5-bit boundary for one dither cell k,
    c rounded to float32 (glColor4f), so the generic GL's final arithmetic decides that cell. Returns (scene, quads) with quads =
    [(x, y, texel index (i, j), c)]."""
    rng = np.random.default_rng(seed)
    tex = rng.integers(40, 256, 64 * 64 * 3, dtype=np.uint8)
    sc = P.Scene()
    sc.setup2d(1.0)
    sc.texture(64, 64, tex.tobytes(), mag=0x2600, mini=0x2601)
    sc.state.append("glEnable 0xde1")
    T = tex.reshape(64, 64, 3)
    quads = []
    for gy in range(600 // 18):
        for gx in range(800 // 18):
            x, y = gx * 18 + 1, gy * 18 + 1
            i, j = int(rng.integers(0, 64)), int(rng.integers(0, 64))
            ch = int(rng.integers(0, 3))
            t = float(T[j, i, ch])
            kx, ky = int(rng.integers(0, 4)), int(rng.integers(0, 4))
            k = KD4[(y + ky) & 3][(x + kx) & 3]
            d = (k + 0.5) / 16
            n_hi = int(np.floor(t * 31 / 255 + d))
            n = int(rng.integers(max(1, n_hi // 2), n_hi + 1))
            c = (n - d) * 255.0 / (31.0 * t)
            c = min(c, 1.0)
            col = [float(rng.uniform(0.3, 1.0)) for _ in range(3)]
            col[ch] = c
            s_, t_ = (i + 0.5) / 64.0, (j + 0.5) / 64.0
            sc.quad(x, y, x + 16, y + 16, colors=[tuple(col) + (1.0,)] * 4, texcoords=[(s_, t_)] * 4)
            quads.append((x, y, i, j, tuple(col)))
    return sc, quads


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "fin":
    sc, quads = fin_scene()
    d = sc.write("s329_fin_near")
    real = P.real_frame(d)
    print("fin_near drawn", int((real != 0).sum()), "quads", len(quads), flush=True)
    sys.exit(0)


def fin_lin_scene(seed=43):
    """fin_lin: like fin_near with GL_LINEAR in both filters and the quad's (s, t) constant at a random float position: every pixel
    of a quad has the same bilinear mix of four texels; c puts the exact mix * c * 31 / 255 + d on a boundary for one dither cell"""
    rng = np.random.default_rng(seed)
    tex = rng.integers(40, 256, 64 * 64 * 3, dtype=np.uint8)
    sc = P.Scene()
    sc.setup2d(1.0)
    sc.texture(64, 64, tex.tobytes(), mag=0x2601, mini=0x2601)
    sc.state.append("glEnable 0xde1")
    T = tex.reshape(64, 64, 3).astype(np.float64)
    quads = []
    for gy in range(600 // 18):
        for gx in range(800 // 18):
            x, y = gx * 18 + 1, gy * 18 + 1
            s_ = float(np.float32(rng.uniform(0.02, 0.98)))
            t_ = float(np.float32(rng.uniform(0.02, 0.98)))
            u, v = float(np.float32(np.float32(s_) * np.float32(64))), float(np.float32(np.float32(t_) * np.float32(64)))
            fu, fv = u - 0.5, v - 0.5
            i0, j0 = int(np.floor(fu)), int(np.floor(fv))
            ax, ay = fu - i0, fv - j0
            ch = int(rng.integers(0, 3))
            f = ((1 - ax) * (1 - ay) * T[j0 % 64, i0 % 64, ch] + ax * (1 - ay) * T[j0 % 64, (i0 + 1) % 64, ch] +
                 (1 - ax) * ay * T[(j0 + 1) % 64, i0 % 64, ch] + ax * ay * T[(j0 + 1) % 64, (i0 + 1) % 64, ch])
            kx, ky = int(rng.integers(0, 4)), int(rng.integers(0, 4))
            k = KD4[(y + ky) & 3][(x + kx) & 3]
            d = (k + 0.5) / 16
            n_hi = int(np.floor(f * 31 / 255 + d))
            n = int(rng.integers(max(1, n_hi // 2), n_hi + 1))
            c = min((n - d) * 255.0 / (31.0 * f), 1.0)
            col = [float(rng.uniform(0.3, 1.0)) for _ in range(3)]
            col[ch] = c
            sc.quad(x, y, x + 16, y + 16, colors=[tuple(col) + (1.0,)] * 4, texcoords=[(s_, t_)] * 4)
            quads.append((x, y, s_, t_, tuple(col)))
    return sc, quads


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "finlin":
    sc, quads = fin_lin_scene()
    d = sc.write("s329_fin_lin")
    real = P.real_frame(d)
    print("fin_lin drawn", int((real != 0).sum()), "quads", len(quads), flush=True)
    sys.exit(0)


def blend_scenes(seed=47):
    """fin_blend: a flat untextured background (0.35, 0.55, 0.75) and, over it with GL_BLEND (SRC_ALPHA, ONE_MINUS_SRC_ALPHA),
    16 x 16 quads of an RGBA texture (the terrain's filters, magnified: one texel per quad); each quad's colour puts the exact blend
    c * 31 * t / 255 * a + dst * (1 - a) + (k + 0.5) / 16 on a 5-bit boundary for one dither cell (dst read from bg_only's real frame).
    Writes bg_only first (the background alone)."""
    rng = np.random.default_rng(seed)
    bgc = (0.35, 0.55, 0.75, 1.0)
    sc0 = P.Scene()
    sc0.setup2d(1.0)
    sc0.quad(0.0, 0.0, 800.0, 600.0, colors=[bgc] * 4)
    d0 = sc0.write("s329_bg_only")
    bg = P.real_frame(d0).reshape(600, 800)
    B5 = np.stack([(bg >> 10) & 31, (bg >> 5) & 31, bg & 31], -1)
    tex = rng.integers(40, 256, 64 * 64 * 4, dtype=np.uint8).reshape(64, 64, 4)
    tex[:, :, 3] = rng.integers(30, 226, (64, 64), dtype=np.uint8)
    sc = P.Scene()
    sc.setup2d(1.0)
    sc.pass_ += []
    sc.quad(0.0, 0.0, 800.0, 600.0, colors=[bgc] * 4)
    sc.texture(64, 64, tex.tobytes(), fmt=0x1908, mag=0x2600, mini=0x2601)
    sc.state.append("glEnable 0xde1")
    sc.state.append("glDisable 0xde1")
    # the texture is enabled and blending switched on after the background, inside the pass
    sc.pass_.append("glEnable 0xde1")
    sc.pass_.append("glBlendFunc 0x302,0x303")
    sc.pass_.append("glEnable 0xbe2")
    for gy in range(600 // 18):
        for gx in range(800 // 18):
            x, y = gx * 18 + 1, gy * 18 + 1
            i, j = int(rng.integers(0, 64)), int(rng.integers(0, 64))
            ch = int(rng.integers(0, 3))
            t, a = float(tex[j, i, ch]), float(tex[j, i, 3]) / 255.0
            kx, ky = int(rng.integers(0, 4)), int(rng.integers(0, 4))
            px, py = x + kx, y + ky
            k = KD4[py & 3][px & 3]
            dd = (k + 0.5) / 16
            D = float(B5[py, px, ch])
            lo = D * (1 - a) + dd
            hi = 31 * t / 255 * a + lo
            if int(np.floor(hi)) <= int(np.ceil(lo)):
                n = int(np.floor(hi))
            else:
                n = int(rng.integers(int(np.ceil(lo)), int(np.floor(hi)) + 1))
            c = (n - dd - D * (1 - a)) / (31 * t / 255 * a)
            c = float(min(max(c, 0.0), 1.0))
            col = [float(rng.uniform(0.3, 1.0)) for _ in range(3)]
            col[ch] = c
            s_, t_ = (i + 0.5) / 64.0, (j + 0.5) / 64.0
            sc.quad(x, y, x + 16, y + 16, colors=[tuple(col) + (1.0,)] * 4, texcoords=[(s_, t_)] * 4)
    return sc


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "blend":
    sc = blend_scenes()
    d = sc.write("s329_fin_blend")
    real = P.real_frame(d)
    print("fin_blend drawn", int((real != 0).sum()), flush=True)
    sys.exit(0)


def iso_scene(seed=53, w=32.0, h=20.0, jitter=6.0):
    """iso: the terrain's tile shape in 2D: diamonds w x h (corners left, top, right, bottom, centre) of four triangles around the
    centre, texcoords 0 / 0.5 / 1 as the recorded passes bind them, one of 8 random textures per tile (mag NEAREST, min LINEAR: the
    terrain's filters), per-vertex random colours shared by neighbouring tiles (Gouraud in place of the lighting), corner heights
    jittered by up to `jitter` px, fractional offsets (0.226, 0.325) like the recorded window positions"""
    rng = np.random.default_rng(seed)
    sc = P.Scene()
    sc.setup2d(1.0)
    tids = []
    for k in range(8):
        tids.append(sc.texture(64, 64, rng.integers(0, 256, 64 * 64 * 3, dtype=np.uint8).tobytes(), mag=0x2600, mini=0x2601))
    sc.state.append("glEnable 0xde1")
    nx, ny = int(800 // w) + 2, int(600 // (h / 2)) + 2
    hgt = rng.uniform(-jitter, jitter, (ny + 2, nx + 2))
    colc = rng.uniform(0.35, 1.0, (ny + 2, nx + 2, 3))
    for j in range(ny):
        for i in range(nx):
            ox = i * w + (j % 2) * w / 2 - w / 2 + 0.226
            oy = j * h / 2 - h / 2 + 0.325
            # corners: left (i, j), top, right, bottom on a staggered grid; heights and colours per corner
            Lp = (ox, oy + h / 2 + hgt[j + 1, i]); Tp = (ox + w / 2, oy + hgt[j, i + 1]); Rp = (ox + w, oy + h / 2 + hgt[j + 1, i + 2])
            Bp = (ox + w / 2, oy + h + hgt[j + 2, i + 1])
            Cp = (ox + w / 2, oy + h / 2 + (hgt[j + 1, i] + hgt[j + 1, i + 2] + hgt[j, i + 1] + hgt[j + 2, i + 1]) / 4)
            cL, cT, cR, cB = (tuple(colc[j + 1, i]), tuple(colc[j, i + 1]), tuple(colc[j + 1, i + 2]), tuple(colc[j + 2, i + 1]))
            cC = tuple((np.array(cL) + np.array(cT) + np.array(cR) + np.array(cB)) / 4)
            sc.pass_.append("glBindTexture 0xde1,0x%x" % tids[int(rng.integers(0, 8))])
            for (A, ca, ta), (B, cb, tb) in (((Lp, cL, (0.0, 0.0)), (Tp, cT, (1.0, 0.0))), ((Tp, cT, (1.0, 0.0)), (Rp, cR, (1.0, 1.0))),
                                             ((Rp, cR, (1.0, 1.0)), (Bp, cB, (0.0, 1.0))), ((Bp, cB, (0.0, 1.0)), (Lp, cL, (0.0, 0.0)))):
                sc.tri(Cp, A, B, colors=[cC + (1.0,), ca + (1.0,), cb + (1.0,)], texcoords=[(0.5, 0.5), ta, tb])
    return sc


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "iso":
    for name, kw in (("iso", {}), ("iso_z1", {"w": 64.0, "h": 40.0, "jitter": 12.0}), ("iso_z0", {"w": 16.0, "h": 10.0, "jitter": 3.0})):
        d = iso_scene(**kw).write("s329_" + name)
        real = P.real_frame(d)
        print(name, "drawn", int((real != 0).sum()), flush=True)
    sys.exit(0)


ISO_COV_COLOURS = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255), (255, 128, 0), (128, 0, 255)]


def iso_cov_scene(seed=53, w=32.0, h=20.0, jitter=6.0):
    """iso_cov: iso_scene's geometry with constant textures of 8 distinct colours (white vertex colour), triangle k of tile (i, j)
    bound to texture (2 i + 3 j + k) % 8 + its tile parity, so a pixel's real colour names its owning triangle's texture"""
    rng = np.random.default_rng(seed)
    sc = P.Scene()
    sc.setup2d(1.0)
    tids = [sc.texture(64, 64, bytes(c) * (64 * 64), mag=0x2600, mini=0x2601) for c in ISO_COV_COLOURS]
    sc.state.append("glEnable 0xde1")
    nx, ny = int(800 // w) + 2, int(600 // (h / 2)) + 2
    hgt = rng.uniform(-jitter, jitter, (ny + 2, nx + 2))
    for j in range(ny):
        for i in range(nx):
            ox = i * w + (j % 2) * w / 2 - w / 2 + 0.226
            oy = j * h / 2 - h / 2 + 0.325
            Lp = (ox, oy + h / 2 + hgt[j + 1, i]); Tp = (ox + w / 2, oy + hgt[j, i + 1]); Rp = (ox + w, oy + h / 2 + hgt[j + 1, i + 2])
            Bp = (ox + w / 2, oy + h + hgt[j + 2, i + 1])
            Cp = (ox + w / 2, oy + h / 2 + (hgt[j + 1, i] + hgt[j + 1, i + 2] + hgt[j, i + 1] + hgt[j + 2, i + 1]) / 4)
            for k, ((A, ta), (B, tb)) in enumerate((((Lp, (0.0, 0.0)), (Tp, (1.0, 0.0))), ((Tp, (1.0, 0.0)), (Rp, (1.0, 1.0))),
                                                   ((Rp, (1.0, 1.0)), (Bp, (0.0, 1.0))), ((Bp, (0.0, 1.0)), (Lp, (0.0, 0.0))))):
                sc.pass_.append("glBindTexture 0xde1,0x%x" % tids[(2 * i + 3 * j + k + (j % 2) * 4) % 8])
                sc.tri(Cp, A, B, colors=[(1.0, 1.0, 1.0, 1.0)] * 3, texcoords=[(0.5, 0.5), ta, tb])
    return sc


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "isocov":
    for name, kw in (("iso_cov", {}), ("iso_cov_z0", {"w": 16.0, "h": 10.0, "jitter": 3.0})):
        d = iso_cov_scene(**kw).write("s329_" + name)
        real = P.real_frame(d)
        print(name, "drawn", int((real != 0).sum()), flush=True)
    sys.exit(0)
