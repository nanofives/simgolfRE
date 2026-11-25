import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C, deliver as D, route_tops as R, route_meters as M

st = R.load_styles(); ms = M.load()
HOW = {"course": "nine lilac column pills on the slab line and a cream striped list with a raised cream box (line x 288.9..510.9, bottom 188.9, corners r 6) hanging from the line",
       "employ": "two sunk tables (x 32.5..392.5 / 406.5..766.5, y 67.5..245.6) of tan cells with a cream column, 2 px navy rules at y 114.5 / 159.5 / 204.5 and x 331.5 / 705.5; the slab line and a tan list floor from y 246.8",
       "aura": "one lilac pill on the slab line, a flat cream box (x 42..757, y 95..170) on the tan list floor, the aura meter",
       "value": "two lilac pills, navy bullets (r 2.9) on cream / tan stripes, the value meter"}
DET = {"course": [(0, 40, 120, 90), (250, 70, 120, 130), (680, 40, 120, 90)], "employ": [(20, 55, 100, 80), (300, 90, 120, 80), (0, 200, 120, 54)],
       "aura": [(0, 40, 120, 90), (90, 170, 140, 70), (560, 170, 140, 70)], "value": [(0, 40, 120, 90), (20, 90, 60, 80), (560, 170, 140, 70)]}
for mode in ("course", "employ", "aura", "value"):
    rgb, m = C.sheet(f"route screens_{mode}.pcx")
    base = np.empty_like(rgb); base[:] = R.LAV
    sc = R.scene(mode, st, ms)
    det = [D.tile(rgb[y:y + h, x:x + w], base[y:y + h, x:x + w], sc, (x, y), 4) for x, y, w, h in DET[mode]]
    D.deliver(f"route-top-{mode}", [("idle", rgb, base, sc, (0, 0))],
              "lavender ground, the arched title plate (Buy Land's profile, the capsules' plate style)" +
              ("" if mode == "employ" else ", a tan banner sunk into the lavender (x 42.5..756.5 from y 51.5), the lavender slab in a navy line (x 5.2..795.8 from y 91.8, r 8) with the list sunk into it (x 18.5..780.5)")
              + ": " + HOW[mode] + ". Every paint (sunk lips and inner shades, pill and slab lines) solved by least squares over all instances on the four sheets (lit.py); geometry read off pixel rows. Full view 1x on top, details at 4x below",
              unsure="the sunk lips' corners are mitred where the art's shade fades round them", extra_tiles=det, z=1)
for mode in ("aura", "value"):
    rgb, m = C.sheet(f"route screens_{mode}.pcx")
    x, y, w, h = 108, 182, 584, 52
    art = rgb[y:y + h, x:x + w]
    base = np.empty_like(art); base[:] = [243, 230, 204]
    sc = M.scene(mode, ms, st)
    fb = np.empty_like(rgb); fb[:] = [243, 230, 204]
    det = [D.tile(rgb[b:b + hh, a:a + ww], fb[b:b + hh, a:a + ww], sc, (a, b), 6) for a, b, ww, hh in ((108, 182, 110, 52), (582, 182, 110, 52))]
    D.deliver(f"route-meter-{mode}", [("idle", art, base, sc, (x, y))],
              "a rounded bar in a navy line (line centre x 193.5..607.4, y 194.0..229.6, r 6) filled with a left-to-right gradient "
              "(colours read off its middle row at 9 places), an inner shade under its top and left walls and a soft shadow below "
              "(fitted); two lilac end pills whose inner ends run under the bar; the lilac pill over its top (the column pills' style)",
              extra_tiles=det, z=2)
