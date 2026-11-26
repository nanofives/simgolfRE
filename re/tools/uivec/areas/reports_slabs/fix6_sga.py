"""P41 round 6, reports_slabs item 1: the SGA emblem back on sga-slab, as art lifted with a matte.

The round SGA badge (lower left of the SGA report, SGA.pcx) is a picture (roster element sga-badge, Mariano's mark "art").
Rounds 1-2 pasted its pixels into the comparison images only (build.py keep_art_circles), so the approved scene carries no
trace of it and the port paints the slab over it.

Now the scene carries it as its last entry, a picture in wide.py's format (re/tools/uivec/wide.py, MATTES), lifted the way
the front end lifts its marks (wide_title.py MARKS_MATTE): matte "ground_diff" t0 9, t1 30, feather 4. G = the approved
slab scene beneath it rendered at 1x (frame pixel centres, the entries before it), A = the sheet pixel; alpha =
clip((|A - G| - t0) / (t1 - t0)), colour = G + (A - G) / alpha. So the badge, its soft edge and its cast shadow come out of
the sheet, and the panel the scene draws shows everywhere else in the cut. Nothing else in the scene changes.

The cut: the badge circle measured in round 1 (sheets.py keep_art_circles: centre 120, 339, r 62) and the shadow below and
to the right; checked here that no alpha lands outside the badge and its shadow (_read/sga_matte.png).

py -3.12 fix6_sga.py     writes work6/reports_slabs/sga-slab.json, sga-slab.png, _read/sga_matte.png
"""
import os
import sys

import numpy as np
from PIL import Image

import common as C
import slabkit as K
import fix6_common as F

sys.path.insert(0, C.UIVEC)
import sdf    # noqa: E402
import wide   # noqa: E402  (import only: the matte and the picture compositing)

EID = "sga-slab"
SHEET = "Interface/infoscreens/SGA.pcx"
CUT = [54, 273, 136, 136]          # x 54..189, y 273..408: the badge (r 62 about 120, 339) and its shadow
# wide.py's ground_diff (the front end's MARKS_MATTE numbers) plus two optional fields this round adds (wide.lift ignores
# them; the port's lift needs them added, see below):
#   "solid": a shape inside which alpha is 1 (the badge's own disc: where a badge pixel happens to be within t0 of the
#            panel colour, plain ground_diff leaves a pinhole);
#   "clip" + "clip_soft": alpha is multiplied by clip(1 - d / clip_soft, 0, 1), d = signed distance to the clip shape: the
#            badge's disc united with the same disc moved by its cast shadow's offset. Without it the rectangle also lifts
#            short pieces of the pill and box lines next to the badge, where the approved scene's lines sit about 1 px off
#            the art's (_read/sga_seam.png): blurred art lines beside crisp ones.
# Measured (fix6_sga.extent): the badge's alpha ends at r 61.2 about (120, 339) on the upper left, its shadow at r 64-67
# from 0 to 120 degrees (y down), peak 67 at 60 degrees: a 61.5 disc and the same disc moved (2, 3.5).
BADGE_C, BADGE_R = (120.0, 339.0), 61.5
SHADOW_D = (2.0, 3.5)
MATTE = {"kind": "ground_diff", "t0": 9.0, "t1": 30.0, "feather": 4.0,
         "solid": {"t": "circle", "c": list(BADGE_C), "r": 59.0},
         "clip": {"t": "union", "of": [{"t": "circle", "c": list(BADGE_C), "r": BADGE_R},
                                       {"t": "circle", "c": [BADGE_C[0] + SHADOW_D[0], BADGE_C[1] + SHADOW_D[1]], "r": BADGE_R}]},
         "clip_soft": 1.5}


def lift(pic, G):
    """(colour h x w x 3, alpha h x w) of the picture: wide.matte (ground_diff) with the solid / clip fields applied, then the
    feather as wide.lift does it. Pixel centres at frame (x + i + 0.5, y + j + 0.5)."""
    idx, pal = wide.fit.load_sheet(pic["sheet"])
    x, y, w, h = pic["rect"]
    A = pal[idx[y:y + h, x:x + w]].astype(float)
    m = pic["matte"]
    ax, ay = pic.get("at", [x, y])
    g = G[ay:ay + h, ax:ax + w]
    al, _ = wide.matte(A, g, m["t0"], m["t1"])
    jj, ii = np.mgrid[0:h, 0:w]
    X, Y = ax + ii + 0.5, ay + jj + 0.5
    if "solid" in m:
        al = np.maximum(al, np.clip(0.5 - sdf.sd(m["solid"], X, Y), 0.0, 1.0))
    col = np.where(al[..., None] > 1e-3, g + (A - g) / np.maximum(al[..., None], 1e-3), A)
    col = np.clip(col, 0, 255)
    if "clip" in m:
        al = al * np.clip(1.0 - sdf.sd(m["clip"], X, Y) / m["clip_soft"], 0.0, 1.0)
    f = float(m.get("feather", 0.0))
    if f > 0:
        e = np.minimum(np.minimum(ii + 0.5, w - ii - 0.5), np.minimum(jj + 0.5, h - jj - 0.5))
        al = al * np.clip(e / f, 0.0, 1.0)
    return col, al


def composite(img, it, s, origin, G):
    """wide.composite_picture with this module's lift (premultiplied, bilinear, as the port's textured quad)"""
    rgb, al = lift(it["picture"], G)
    x, y, w, h = it["picture"]["rect"]
    X0, Y0 = (x - origin[0]) * s, (y - origin[1]) * s
    W, H = int(round(w * s)), int(round(h * s))
    pm = np.dstack([rgb * al[..., None], al * 255.0]).astype(np.float32)
    big = np.dstack([np.asarray(Image.fromarray(pm[..., k], "F").resize((W, H), Image.BILINEAR)) for k in range(4)])
    i0, j0 = int(round(X0)), int(round(Y0))
    Hh, Ww = img.shape[:2]
    a0, b0 = max(0, -i0), max(0, -j0)
    a1, b1 = min(W, Ww - i0), min(H, Hh - j0)
    if a1 <= a0 or b1 <= b0:
        return img
    sub = big[b0:b1, a0:a1]
    Al = sub[..., 3:4] / 255.0
    dst = img[j0 + b0:j0 + b1, i0 + a0:i0 + a1]
    dst[:] = dst * (1 - Al) + sub[..., :3]
    return img


def extent(al, cut, c=BADGE_C):
    """the radius about c where the picture's alpha first drops under 0.05, every 15 degrees"""
    x, y, w, h = cut
    out = {}
    for th in range(-180, 180, 15):
        t = np.radians(th)
        for r10 in range(560, 800, 2):
            r = r10 / 10
            i, j = int(np.floor(c[0] + r * np.cos(t) - x)), int(np.floor(c[1] + r * np.sin(t) - y))
            if not (0 <= i < w and 0 <= j < h) or al[j, i] < 0.05:
                out[th] = r
                break
    return out


def picture_entry():
    return {"name": "SGA emblem (art, lifted)", "picture": {"sheet": SHEET, "rect": list(CUT), "at": CUT[:2],
                                                            "matte": dict(MATTE)}}


def under_1x(scene):
    """the drawn scene at 1x over the frame, pictures left out (wide.under_1x for a plain item list)"""
    img = np.zeros((600, 800, 3))
    sdf.render([it for it in scene if "picture" not in it], img, 1.0)
    return img


def render_with_picture(scene, box, s, G):
    x0, y0, x1, y1 = box
    img = K.render([it for it in scene if "picture" not in it], box, s)
    for it in scene:
        if "picture" in it:
            composite(img, it, s, (x0, y0), G)
    return img


UNDER_X = 120.0     # the badge's centre: the pill and the cream box now start here, hidden under the badge


def _from_x(S, x_left):
    """an rrect moved and widened so its left edge is x_left, its right edge unchanged"""
    right = S["c"][0] + S["half"][0]
    S["c"] = [(x_left + right) / 2, S["c"][1]]
    S["half"] = [(right - x_left) / 2, S["half"][1]]


def under_badge(sc):
    """In the art the committee pill and the cream box run on under the badge; the approved scene ended them at the badge's
    edge (the pill with a round cap at x 146.6, the box at x 160), which shows beside the badge once it is drawn on top: a
    piece of the pill's cap above its upper right, a notch of slab between it and the box at its lower right. Their left
    ends move to x 120 (under the badge for every y they span: the badge disc r 61.5 about (120, 339) covers x 120 from
    y 277.5 to 400.5); right ends, heights and paints unchanged."""
    pill, pill_sh, box, slab = sc[161], sc[70], sc[91], sc[0]
    assert abs(pill["layers"][0]["shape"]["c"][1] - 293.12) < 0.01 and abs(pill_sh["layers"][0]["shape"]["of"]["c"][1] - 293.12) < 0.01
    assert box["layers"][0]["shape"]["c"] == [452.0, 340.5] and slab["layers"][0]["shape"]["b"]["of"][20]["c"] == [452.0, 340.5]
    lefts = [L["shape"]["c"][0] - L["shape"]["half"][0] for L in pill["layers"]]   # the inner shape is inset by the line width
    for L, x_left in zip(pill["layers"], lefts):
        _from_x(L["shape"], UNDER_X + (x_left - lefts[0]))
    _from_x(pill_sh["layers"][0]["shape"]["of"], UNDER_X)
    _from_x(box["layers"][0]["shape"], UNDER_X)
    _from_x(slab["layers"][0]["shape"]["b"]["of"][20], UNDER_X)
    for it in (pill, box):
        it["box"][0] = UNDER_X - 2.0
    pill_sh["box"][0] = UNDER_X - 2.0


def main():
    prev = F.approved(EID)
    doc = F.deep(prev)
    under_badge(doc["scene"])
    doc["scene"] = doc["scene"] + [picture_entry()]
    doc["round6"] = ("the SGA emblem as its last entry: a picture lifted from SGA.pcx with wide.py's ground_diff matte "
                     "(t0 9, t1 30, feather 4) against this scene drawn beneath it")
    F.save(EID, doc)

    G = under_1x(doc["scene"])        # the scene beneath the picture: every entry before it
    rgb0, al0 = wide.lift(doc["scene"][-1]["picture"], G)        # the plain front-end matte, for the record
    print("plain ground_diff extent:", extent(al0, CUT))
    rgb, al = lift(doc["scene"][-1]["picture"], G)
    x, y, w, h = CUT
    art = C.art("SGA.pcx")[0]
    A = art[y:y + h, x:x + w]
    comp = G[y:y + h, x:x + w] * (1 - al[..., None]) + rgb * al[..., None]
    jj, ii = np.mgrid[0:h, 0:w]
    rr = np.hypot(ii + x + 0.5 - 120.0, jj + y + 0.5 - 339.0)
    print("with solid + clip extent:", extent(al, CUT))
    print("alpha: inside r 60: min %.2f   ring 60..66: mean %.2f   beyond r 70: max %.2f, px > 0.1: %d" % (
        al[rr < 60].min(), al[(rr >= 60) & (rr < 66)].mean(), al[rr > 70].max(), int((al[rr > 70] > 0.1).sum())))
    print("composite vs art over the cut: mean |d| %.2f, max %.1f" % (np.abs(comp - A).mean(), np.abs(comp - A).max()))
    z = 3
    def up(im):
        return np.kron(np.clip(im, 0, 255), np.ones((z, z, 1)))
    gap = np.zeros((h * z, 6, 3)) + 20
    strip = np.concatenate([up(A), gap, up(G[y:y + h, x:x + w]), gap, up(np.repeat(al[..., None] * 255, 3, -1)), gap, up(comp)], 1)
    Image.fromarray(K.to8(strip)).save(os.path.join(F.OUT6, "_read", "sga_matte.png"))

    # the comparison: art | previous approved | new, the whole slab at 2x in tiles and the emblem at 4x
    rows = [("emblem 3x", (50, 270, 194, 412), 3),
            ("emblem's upper right (pill runs under it) 8x", (136, 274, 200, 312), 8),
            ("emblem's lower right (box runs under it, cast shadow) 8x", (136, 376, 200, 412), 8),
            ("emblem's left edge 8x", (52, 300, 116, 345), 8),
            ("slab 1x, left", (35, 26, 400, 445), 1), ("slab 1x, right", (400, 26, 765, 445), 1)]
    tiles = []
    for lab, box, s in rows:
        bx0, by0, bx1, by1 = box
        Aa = np.kron(art[by0:by1, bx0:bx1], np.ones((s, s, 1)))
        P = K.render(prev["scene"], box, s)
        N = render_with_picture(doc["scene"], box, s, G)
        g = np.zeros((Aa.shape[0], 8, 3)) + 20
        tiles.append((lab, np.concatenate([Aa, g, P, g, N], 1)))
    Wt = max(t.shape[1] for _, t in tiles)
    out = []
    for lab, t in tiles:
        head = np.zeros((16, Wt, 3)) + 20
        body = np.zeros((t.shape[0], Wt, 3)) + 20
        body[:, :t.shape[1]] = t
        out += [head, body]
    im = Image.fromarray(K.to8(np.concatenate(out, 0)))
    from PIL import ImageDraw
    d = ImageDraw.Draw(im)
    yy = 0
    for lab, t in tiles:
        d.text((4, yy + 2), f"{lab}: art (left) | previous approved (middle) | new (right)", fill=(255, 255, 0))
        yy += 16 + t.shape[0]
    im.save(os.path.join(F.OUT6, EID + ".png"))
    print(im.size)


if __name__ == "__main__":
    main()
