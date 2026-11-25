"""P41 hud_panels round 3 (2026-11-24): member-mood-faces and member-mood-faces-extra (rejected twice, no note), two
versions.

Round 2 beside the art at 16x (fix3.sides, luminance maps of M 450,100 printed per face): the features were already placed
pixel by pixel (eye blocks, mouth corners, teeth rows, brows, horns match the art's extents), so the five largest
differences are in how they are painted, not where:
1. the rim: a thick near-black ring (the darkest 30 % of the edge pixels) all round; the art's edge is the face's own dark
   shade (dark green on the green faces, olive on the yellow ones, maroon on the red ones), uneven, blending into the
   lavender.
2. the eyes: solid near-black ellipses; the art's eyes are dark maroon / navy blocks with a darker core and one lighter
   pixel (a glint) in most faces.
3. the lines (smiles, frowns, brows): flat bars of one colour; the art's lines have a dark core inside a mid-tone edge
   (thickness), e.g. face 3's smile row 11 is 3 at its core with 7-9 above and below it.
4. the skin: one smooth radial paint; the art's skin is lighter at the centre with darker patches toward the rim (two or
   three tones).
5. faces 8-10: the angry features read as cartoon shapes; the art's brows and mouths there are mixed dark tones.

A (faithful): patches.patch_layers on the whole face (every pixel that is not the lavender ground), k = 8 tones, clipped
to the face's silhouette.
B (clean): round 2's measured feature shapes (el_faces2.face_parts) painted with tone: a soft rim in the face's own edge
tone, the skin's radial paint, every feature as a mid-tone shape (its pixels' 55-90 % luminance band) over a dark core (its darkest 35 %, the
shape shrunk 0.35 px), and a glint on
each eye at the art's lightest pixel deep inside that eye (0.45 px) when it is lighter than the eye by 10 levels or more and
still darker than 0.6 x the skin (so it is the eye's own light pixel, not skin under the eye's edge);
the dark parts' colours are sampled without the teeth and glint pixels.
"""
import copy
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_marks as EM  # noqa: E402
import el_faces2 as F2  # noqa: E402
import fit  # noqa: E402
import patches as P  # noqa: E402
import sdf  # noqa: E402
from kit import U, rect, circle  # noqa: E402,F401

ELS = ("member-mood-faces", "member-mood-faces-extra")


def _px_in(shape, ox, oy, a, m, depth=0.0):
    jj, ii = np.mgrid[0:16, 0:16]
    d = sdf.sd(K.shift(shape, ox, oy), ii + ox + 0.5, jj + oy + 0.5)
    return (d < -depth) & m


def face_b(key, ox, oy, n):
    rgb, msk = C.sheet(key)
    a = rgb[oy:oy + 16, ox:ox + 16]
    m = msk[oy:oy + 16, ox:ox + 16]
    L = fit.lum(a)
    parts = F2.face_parts(n)
    sil = F2.silhouette(n)
    jj, ii = np.mgrid[0:16, 0:16]
    rd = np.hypot(ii + 0.5 - F2.AXIS, jj + 0.5 - 8.0)
    ring = (rd > 6.6) & (rd < 7.6) & m
    feat = np.zeros((16, 16), bool)
    for _, s, _ in parts:
        feat |= _px_in({"t": "grow", "of": s, "d": 0.6}, 0, 0, a, m)
    edge = [round(float(v), 1) for v in np.median(a[ring & ~feat], axis=0)]
    body = F2.measure_body(key, ox, oy, parts)
    out = [K.lay(K.shift(sil, ox, oy), edge),
           K.lay(K.shift({"t": "grow", "of": sil, "d": -0.9}, ox, oy), body, soft=0.8)]
    col = lambda sel: [round(float(v), 1) for v in np.median(a[sel], axis=0)]   # noqa: E731
    light = np.zeros((16, 16), bool)        # the teeth and glints: left out of the dark parts' samples
    for name, s, pk in parts:
        if pk in ("teeth", "grey"):
            light |= _px_in({"t": "grow", "of": s, "d": 0.3}, 0, 0, a, m)
    for name, s, pk in parts:
        inside = _px_in(s, 0, 0, a, m, depth=0.15) & m
        if pk not in ("teeth", "grey") and (inside & ~light).sum() >= 2:
            inside &= ~light
        if not inside.any():
            inside = _px_in({"t": "grow", "of": s, "d": 0.3}, 0, 0, a, m) & m
        Li = L[inside]
        if pk in ("teeth", "grey") or name.startswith("glint") or name.startswith("teeth"):
            # light parts: their own colour, as in round 2 (the brightest half of their pixels)
            c = col(inside) if pk == "grey" else col(inside & (L >= np.median(Li)))
            out.append({"name": name, "shape": K.shift(s, ox, oy), "fill": c})
            continue
        band = inside & (L >= np.quantile(Li, 0.55)) & (L <= np.quantile(Li, 0.9))
        mid = col(band) if band.sum() >= 1 else col(inside)
        core = col(inside & (L <= np.quantile(Li, 0.35))) if inside.sum() > 2 else col(inside)
        out.append({"name": name, "shape": K.shift(s, ox, oy), "fill": mid})
        out.append({"name": name + " core", "shape": K.shift({"t": "grow", "of": s, "d": -0.35}, ox, oy), "fill": core})
        if name == "eyes" and n not in (1, 2, 10):        # faces 1, 2, 10 carry their measured glints as parts
            deep = _px_in(s, 0, 0, a, m, depth=0.45) & m
            for half in (0, 1):
                hsel = deep & ((ii < 8) if half == 0 else (ii >= 8))
                if hsel.sum() < 3:
                    continue
                j, i = np.unravel_index(np.argmax(np.where(hsel, L, -1)), L.shape)
                skin = float(np.median(L[(rd < 6.0) & ~feat & m]))
                if np.median(L[hsel]) + 10 <= L[j, i] <= 0.6 * skin:      # a lit eye pixel, not the skin round it
                    out.append({"name": "eye glint", "shape": circle(ox + i + 0.5, oy + j + 0.5, 0.5),
                                "fill": [round(float(v), 1) for v in a[j, i]]})
    return out


def build(el):
    outA, outB = {}, {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        n = int(st.split()[-1])
        bgl = [K.lay(rect(x0, y0, x0 + w, y0 + h), EM.cell_bg(key, rct))]
        bg = [K.item(bgl, box=[x0, y0, x0 + w, y0 + h])]
        sil = K.shift(F2.silhouette(n), x0, y0)
        mask, art = P.glyph_mask(key, rct, bg, None, thr=12.0)
        lay = P.patch_layers(key, rct, bg, mask=mask, k=8)
        for L in lay:
            L["clip"] = sil
        outA[st] = [K.item(copy.deepcopy(bgl) + lay, box=[x0, y0, x0 + w, y0 + h])]
        outB[st] = [K.item(copy.deepcopy(bgl) + face_b(key, x0, y0, n), box=[x0, y0, x0 + w, y0 + h])]
    return {"a": outA, "b": outB}


for _el in ELS:
    EB.BUILD[_el] = build
