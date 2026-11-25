"""P41 hud_panels fix round (2026-11-24): the three trays recomposed (member-panel and golfer-action-panel redo,
employee-panel "better shading").

What changed against round 1 (el_trays.py, whose part placement is kept):
- The plate's left lobe is the art's own silhouette, read from the opaque runs of every row (all three trays share it):
  the three tab halos (circles fitted to the silhouette's outer arcs: (286.5, 492.5) r 19.0, (256.1, 511.2) r 18.6,
  (233.2, 539.3) r 18.7) and the plate, whose top edge is row 494 and whose left edge comes off the bottom halo at
  (228.5, 557.5) and runs down x 225 from row 570. Round 1 built the lobe from three stacked rounded rectangles (a
  stepped edge the art does not have).
- The lobe's outer edge is a one-pixel dark line (the art's dark pixels round the halos' upper arcs and down x 225);
  the plate's top edge stays lit as before.
- Each halo sits in a raised socket whose lower-right side catches the light: a soft light ring outside the halo,
  sampled where the art shows it.
- Recessed boxes are sunken (dark top and left walls, lit right and bottom walls, measured: member well 109-76 under
  the lit top edge, 93-77 on its left, 156-143 on its right, 189 at its foot); their lit surface is measured over a
  depth fitting each box (the Employee note box shades down over 15 rows: 157 under its dark top line to 240), where
  round 1 used 4 px everywhere.
- Every button is the fix round's own redrawn element (blue tabs, bubbles, pedestals), placed as before.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_slabs as S  # noqa: E402
import el_trays as T1  # noqa: E402
from kit import U, rect, circle, poly, inter, sub, outline  # noqa: E402,F401

HALOS = [(286.53, 492.45, 19.0), (256.12, 511.23, 18.61), (233.2, 539.25, 18.67)]
RECESS_L = {"M": [4.0, 3.0], "J": [5.0, 4.0, 4.0, 4.0], "E": [5.0, 11.0]}
# the Employee note box: its top edge is an arch (dark line at row 516 between the bubbles, 520-521 at x 633 / 781,
# 523-524 at the ends: a circle round (708, 1081) R 565), its corners r 4; inside, the shade under the top line runs
# down 10 rows (157 -> 240) and in from the left over 6 px (91 -> 240)
NOTE_BOX = inter(rect(619.0, 500.0, 798.0, 598.0, 4.0), circle(708.0, 1081.0, 565.0))


def recesses(key):
    r = T1.recesses(key)
    return [r[0], NOTE_BOX] if key == "E" else r


def _bb(r):
    hx, hy = r["half"]
    return [r["c"][0] - hx, r["c"][1] - hy, r["c"][0] + hx, r["c"][1] + hy]


REC_BOX = {k: [_bb(r) for r in T1.recesses(k)] for k in "MJE"}
REC_BOX["E"][1] = [619.0, 514.0, 798.0, 598.0]
# the hire / fire / details bubbles are placed without their cells' own piece of note box (the arched box is drawn here)
T1.PLACED["E"] = [(e, s, "bubble" if e in ("employee-hire", "employee-fire", "employee-resume") else h)
                  for e, s, h in T1.PLACED["E"]]


def silhouette():
    plate = poly((268.0, 494.0), (840.0, 494.0), (840.0, 650.0), (225.0, 650.0), (225.0, 571.0), (225.6, 567.0),
                 (227.0, 562.0), (228.6, 557.6), (240.0, 545.0), (256.0, 528.0), (272.0, 506.0), smooth=0)
    return U(plate, *[circle(*h) for h in HALOS])


def yellow_tab(key):
    """The selected (yellow) tab with the fix round's lit one-pixel rim (el_figs.rim_paint) and its body shading
    re-measured on the corrected body circle, so it is lit like the blue tabs beside it."""
    import bubble as B
    import el_figs as F
    scene, cover = T1.yellow_tab(key)
    it = scene[1]
    rim, bl = it["layers"][0], it["layers"][1]
    cx, cy = rim["shape"]["c"]
    r = rim["shape"]["r"]
    b0 = bl["shape"]["c"]
    body = [cx + (b0[0] - cx) * 0.5, cy + (b0[1] - cy) * 0.5, r - F.RIM_W]
    glyph = U(*[L["shape"] for L in it["layers"][2:]])
    coef = B.solve_family([B.body_rows(key, [cx, cy, r], body, trim_glyph=glyph)])
    coef[-1] = [coef[-1][0], [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]
    rim["fill"] = F.rim_paint(key, [cx, cy, r], body[2])
    bl["shape"] = {"t": "circle", "c": body[:2], "r": body[2]}
    bl["fill"] = {"bevel": {"L": body[2], "stops": B.STOPS, "coef": coef}}
    for L in it["layers"][2:]:
        L["clip"] = bl["shape"]
    return scene, cover


def build_tray(el):
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    cell = rect(x0, y0, x0 + w, y0 + h)
    sil = silhouette()
    plate = inter(sil, cell)
    rec = recesses(key)
    items, covers = T1.placed_items(key)
    ytab, ycover = yellow_tab(key)
    covers.append(ycover)
    halos = U(*[circle(*hh) for hh in HALOS])
    excl = U(*(rec + covers + [halos]))
    box = [x0, y0, x0 + w, y0 + h]
    L = [K.lay(plate, S.fit_bevel(key, plate, box, exclude=excl, L=5.0, of=sil))]
    # lit rings on the lower right of each halo socket
    rgb, m = C.sheet(key)
    for cx, cy, r in HALOS:
        px, py = cx + (r + 1.6) * 0.6, cy + (r + 1.6) * 0.8
        col = [round(float(v), 1) for v in rgb[int(py), int(px)]]
        ring = inter(sub(circle(cx, cy, r + 3.2), circle(cx, cy, r)), {"t": "half", "p": [cx, cy], "n": [-0.6, -0.8]})
        L.append(K.lay(ring, col, soft=1.4, clip=plate, alpha=0.85))
    # the rivet at the lobe's foot (round (233, 591) r 7): lit dome from its upper left (191 at (232, 588)) down to the
    # shadow on its lower right (85-93 at (236-238, 599))
    hx, hy = 232.0, 588.0
    stops = []
    for t in np.linspace(0, 1, 6):
        px, py = hx + t * 6.5, hy + t * 11.0
        stops.append([float(t), [round(float(v), 1) for v in rgb[min(int(py), y0 + h - 1), int(px)]]])
    L.append(K.lay(circle(233.0, 591.5, 7.5), {"radial": {"c": [hx + 0.5, hy + 0.5], "r": 12.8, "stops": stops}},
                   soft=1.6, clip=plate))
    stub = rect(214.0 if key == "J" else 215.0, 588.0, 228.0, 600.0, 1.5)
    L.append(K.lay(stub, K.sample(key, rect(216, 592, 222, 598), depth=0.0)))
    for i, r in enumerate(rec):
        ex = U(*(covers + rec[i + 1:])) if (covers or rec[i + 1:]) else None
        bb = REC_BOX[key][i]
        b = [max(bb[0], x0), max(bb[1], y0), min(bb[2], x0 + w), min(bb[3], y0 + h)]
        rc = inter(r, cell)
        L.append(K.lay(rc, S.fit_bevel(key, rc, [int(b[0]), int(b[1]), int(np.ceil(b[2])), int(np.ceil(b[3]))],
                                       exclude=ex, L=RECESS_L[key][i], of=r)))
    # the lobe's dark outer line (left-facing edges only: x < 306; the plate's top edge is lit)
    dark = [round(float(v), 1) for v in np.median(np.array([rgb[575, 225], rgb[580, 225], rgb[566, 226]]), axis=0)]
    edge = inter(sub(plate, {"t": "grow", "of": sil, "d": -1.0}), rect(x0, y0, 306.0, 586.0))
    tail = [K.item([K.lay(edge, dark)], box=[x0, y0, 310, 590])]
    scene = [K.item(L, box=box)] + items + ytab + tail
    return {st: scene}


EB.BUILD.update({"member-panel": build_tray, "golfer-action-panel": build_tray, "employee-panel": build_tray})
