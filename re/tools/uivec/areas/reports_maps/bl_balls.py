"""Buy Land number balls: 9 lit (yellow) and 9 dim (silver) cells of buy_land_buttons.pcx, and the 9 blue balls of the
backdrop. Each is the C1 bubble (navy rim, lit glass body, specular spot upper left, soft cast shadow down-right on its
capsule) with its digit from digits.py. The cells are drawn over this area's own capsules and lavender (bl_caps.py), moved to
the cell's place on the screen (offsets found by matching the cell's surroundings to buy_land.pcx)."""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C      # noqa: E402
import bub              # noqa: E402
import bubbles as BU    # noqa: E402
import digits as D      # noqa: E402
import glyphs           # noqa: E402
import sdf              # noqa: E402
import fit              # noqa: E402

LAV = [148.0, 148.0, 198.0]
# sheet cell pixel + offset = buy_land.pcx pixel (best match of the cell's lavender / capsule surroundings)
OFF = {"lit": {1: (15, 53), 2: (-44, 121), 3: (-105, 189), 4: (93, 53), 5: (32, 121), 6: (-27, 189), 7: (177, 53),
               8: (119, 121), 9: (58, 189)},
       "dim": {1: (14, -10), 2: (-46, 58), 3: (-106, 126), 4: (91, -10), 5: (31, 58), 6: (-29, 126), 7: (177, -10),
               8: (117, 58), 9: (57, 126)}}
BACK = [(43, 86), (43, 154), (43, 223), (301, 86), (301, 154), (301, 223), (566, 86), (566, 154), (566, 223)]
DIGITS_FIT = os.path.join(C.OUT, "read", "digits_fit.json")


def caps():
    import bl_caps
    p = os.path.join(C.OUT, "read", "caps_fit.json")
    if os.path.exists(p):
        with open(p) as f:
            j = json.load(f)
        return bl_caps, j["geom"], j["style"]
    return bl_caps, copy.deepcopy(bl_caps.GEOM), {"cream": bl_caps.style(bl_caps.CREAM), "tan": bl_caps.style(bl_caps.TAN)}


def digit_shapes():
    if os.path.exists(DIGITS_FIT):
        with open(DIGITS_FIT) as f:
            return {int(k): v for k, v in json.load(f).items()}
    return {n: D.shape(n) for n in range(1, 10)}


def move(S, dx, dy):
    return {"t": "move", "of": S, "d": [round(dx, 3), round(dy, 3)]}


def place_glyph(sh, rgb, body, box):
    """The digit's offset on another instance: the body-centre difference, then the best sub-pixel shift (IoU of coverage)."""
    cx, cy, r = body
    x0, y0, x1, y1 = box
    jj, ii = np.mgrid[y0:y1, x0:x1]
    inside = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy) < r * 0.9
    cov, lo, hi = fit.dark_coverage(rgb[y0:y1, x0:x1], inside)
    best = None
    for dy in np.arange(-1.2, 1.21, 0.3):
        for dx in np.arange(-1.2, 1.21, 0.3):
            sc = glyphs.shape_coverage(move(sh, dx, dy), box, inside, s=2)
            v = float(np.minimum(cov, sc).sum() / max(np.maximum(cov, sc).sum(), 1e-9))
            if best is None or v > best[0]:
                best = (v, dx, dy)
    return best


def instances(verbose=False):
    rgb_b, m_b = C.sheet("buy_land_buttons.pcx")
    rgb_s, m_s = C.sheet("buy_land.pcx")
    dg = digit_shapes()
    bc, g, st = caps()
    lit_body = {}
    out = []
    for n in range(1, 10):
        i = n - 1
        for state, yy, colour in (("lit", 1, "yellow"), ("dim", 0x40, "silver")):
            box = (0x3c * i + 1, yy, 0x3b, 0x3d)
            rim = bub.rim_circle(rgb_b, m_b, box)[:3]
            body = bub.body_circle(rgb_b, rim)
            bx = (box[0], box[1], box[0] + box[2], box[1] + box[3])
            if state == "lit":
                lit_body[n] = body
                gl, q = dg[n], None
            else:
                sh = move(dg[n], body[0] - lit_body[n][0], body[1] - lit_body[n][1])
                q = place_glyph(sh, rgb_b, body, bx)
                gl = move(sh, q[1], q[2])
            dx, dy = OFF[state][n]
            cap = bc.shape(g, n)
            out.append({"id": f"buyland-ball-{n}", "state": state, "sheet": "buy_land_buttons.pcx", "rim": [round(v, 3) for v in rim],
                        "body": body, "glyph": gl, "colour": colour, "box": bx, "off": (dx, dy),
                        "shadow": move(cap, -dx, -dy), "n": n})
            if verbose:
                print(n, state, out[-1]["rim"], body, q)
        # the backdrop's blue ball
        ax, ay = BACK[i]
        rim0 = bub.rim_circle(rgb_s, m_s, (ax - 30, ay - 30, 60, 60))[:3]
        body = bub.body_circle(rgb_s, rim0)
        # the backdrop's rim runs into the ball's own dark shadow, so its circle is taken from the body as on the cells
        # (rim centre = body + (0.14, 0.31), radius + 1.65: the lit cells' measured relation)
        rim = [round(body[0] + 0.14, 3), round(body[1] + 0.31, 3), round(body[2] + 1.65, 3)]
        bx = (int(ax - 40), int(ay - 34), int(ax + 40), int(ay + 34))
        sh = move(dg[n], body[0] - lit_body[n][0], body[1] - lit_body[n][1])
        q = place_glyph(sh, rgb_s, body, bx)
        out.append({"id": f"buyland-ball-{n}", "state": "backdrop", "sheet": "buy_land.pcx", "rim": [round(v, 3) for v in rim],
                    "body": body, "glyph": move(sh, q[1], q[2]), "colour": "blue", "box": bx, "off": (0, 0),
                    "shadow": bc.shape(g, n), "n": n})
        if verbose:
            print(n, "backdrop", out[-1]["rim"], body, q)
    return out


def base_for(inst):
    """Lavender + this area's capsules (and nothing of the ball) under the instance's box."""
    bc, g, st = caps()
    x0, y0, x1, y1 = inst["box"]
    dx, dy = inst["off"]
    base = np.empty((y1 - y0, x1 - x0, 3))
    base[:] = LAV
    s = 4
    img = np.kron(base, np.ones((s, s, 1)))
    sdf.render(bc.items(g, st), img, s, origin=(x0 + dx, y0 + dy))
    return img.reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))


if __name__ == "__main__":
    insts = instances(verbose=True)
    S = BU.start_style()
    B = BU.bench(insts, base_for)
    for col in ("yellow", "silver", "blue"):
        n = BU.measure_paint(insts, S, col)
        print(col, "paint rows", n, "rim", S[col]["rim"], "glyph", S[col]["glyph"])
    for col in ("yellow", "silver", "blue"):
        BU.fit_spot(insts, S, B, col, passes=2)
        BU.measure_paint(insts, S, col)
    BU.fit_rim_edges(insts, S, B)
    BU.fit_shadow(insts, S, B)
    with open(BU.STYLE_FILE, "w") as f:
        json.dump(S, f)
    print(json.dumps({k: S[k] for k in ("sh", "edge", "halo", "body")}))
