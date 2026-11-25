"""Member Roster OK (hover, memberRoster_buttons.pcx 1, 1, 44 x 44): the C1 yellow bubble (body measured (22.98, 22.09),
r 14.41) with the report check mark, the same outline as check.py scaled to this bubble (scale and place by the best
coverage overlap), on the cell's lavender square."""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bubbles as BU, check as K, common as C, deliver as D, fit, glyphs

rgb, m = C.sheet("memberRoster_buttons.pcx")
body = [22.98, 22.09, 14.41]
cx, cy, r = body
x0, y0, x1, y1 = 1, 1, 45, 45
jj, ii = np.mgrid[y0:y1, x0:x1]
inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < r * 0.9
cov, lo, hi = fit.dark_coverage(rgb[y0:y1, x0:x1], inside)


def shape(s, dx, dy):
    return {"t": "poly", "rings": [[[(x - K.CHECK_BODY[0]) * s + cx + dx, (y - K.CHECK_BODY[1]) * s + cy + dy] for x, y in K.CHECK]]}


best = None
for s in np.arange(0.54, 0.66, 0.01):
    for dx in np.arange(-1.5, 1.6, 0.25):
        for dy in np.arange(-1.5, 1.6, 0.25):
            sc = glyphs.shape_coverage(shape(s, dx, dy), (x0, y0, x1, y1), inside, s=2)
            v = float(np.minimum(cov, sc).sum() / max(np.maximum(cov, sc).sum(), 1e-9))
            if best is None or v > best[0]:
                best = (v, s, dx, dy)
print("check fit", best)
S = BU.load_style()
inst = {"rim": [cx + 0.1, cy + 0.2, r + 1.2], "body": body, "glyph": shape(*best[1:]), "colour": "yellow", "shadow": None}
art = rgb[y0:y1, x0:x1].copy()
mk = m[y0:y1, x0:x1]
art[~mk] = [70, 110, 90]
base = np.empty_like(art); base[:] = art[40, 4]; base[~mk] = [70, 110, 90]     # the square's lavender (its corner)
# its socket: the ring found by the rim fit (centre (23.01, 23.32), r 18.58), its well paint solved here
import lit
sock = {"t": "circle", "c": [23.01, 23.32], "r": 18.58}
sst = lit.new_style(ow=1.4, L=18.58, out_w=0.0)
jj2, ii2 = np.mgrid[0:46, 0:46]
keep = (np.hypot(ii2 + 0.5 - cx, jj2 + 0.5 - cy) > r + 2.0) & m[0:46, 0:46]
lit.solve([(sock, rgb, (0, 0, 46, 46), keep)], sst, dir_in=1.0)
sst["line"] = [36, 47, 84]      # the ring's pixels are blends 1 px wide: the navy of the other rings
items = [{"name": "socket", "box": [0, 0, 46, 46], "layers": lit.layers(sock, sst)}, BU.item(inst, S)]
D.deliver("roster-ok", [("hover", art, base, items, (x0, y0))],
          f"the C1 yellow bubble (paint from the lot balls' yellow) with the report check mark: check.py's one outline scaled "
          f"{best[1]:.2f} and placed by the best coverage overlap (iou {best[0]:.2f}); in a round socket (navy ring r 18.58 round a sunk well, its paint solved on this cell) on the cell's lavender square", z=8)
