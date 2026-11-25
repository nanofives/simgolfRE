"""Fix round 2: courseinfo.pcx medals (cash, fun, skill), cell a = lit state (31 px), cell b = plain state (32 px).

Read again at 16-28x (work2/marks/read/*.png) and as dark-glyph maps (glyphmap.py: pixels darker than their 7 x 7 mean):
- the bubble: a navy rim ~2.3 px, lighter on the left (measured by angle: (63, 73, 105) on the left down to (27, 37, 68)
  from the top round to the bottom in cell a), and a body lit from the left: its colour is a radial gradient around a light
  centre left of the middle (centre and colours fitted by least squares on the body pixels away from the glyph; round 1's
  bevel fit had no data under the glyph and left dark smudges there). Cell b adds a near-black drop shadow 1 px down-right.
  Cell a puts a crisp white glow (the glyph grown ~1.6 px) behind the glyph.
- glyphs, engraved in the measured glyph colour, as continuous lines (marks.vline: one smooth stroke, round joins):
  cash  = a serif dollar: an S whose curves are thick (3 px) and whose top, middle and bottom runs are thin (1 px), with a
          drooping terminal at the top right and a ball terminal at the bottom left, and two thin bars through it.
  fun   = a laughing face (the bubble is the face): two short squinting eye strokes; a wide open mouth: the upper lip slanting
          down from the left corner and running level to a right corner that flicks up, the mouth's outline down both sides
          and round the bottom, the dark band of the open mouth across it, and the lower lip as a second curve below.
  skill = a golf ball (ring) on a tee (cup and spike), an iron soled on the ground in front of the tee (toe curled up on
          the left, hosel on the right; the tee's spike shows below it) and a curved stroke rising from the hosel and bowing
          out to the right beside the ball (rows 15-21); cell a also shows a dot in the ball's centre and a short stroke right
          of the ball (rows 10-13).
"""
import numpy as np

import common as C
import fit
import glyphmap as G
import sdf
from fix2 import U, circ, lay, poly, vline

CELLS = {"cash": ((350, 69, 31, 31), (450, 69, 32, 32)), "fun": ((350, 169, 31, 32), (450, 169, 32, 32)),
         "skill": ((350, 269, 31, 31), (450, 269, 32, 32))}
T = [0.0, 0.25, 0.5, 0.75, 1.0]
RIM_DIR = np.array([-0.97, -0.24])
_D = np.diff(np.eye(len(T)), axis=0)
SMOOTH = _D.T @ _D                     # a penalty on jumps between neighbouring stops (keeps stops with few pixels sane)


# ------------------------------------------------------------------------------------------------------------- glyphs
def glyph_cash(state):
    S = vline([[20.6, 12.8], [20.2, 11.4], [18.6, 10.5], [15.5, 10.45], [12.5, 10.7], [10.9, 11.8], [11.0, 13.4],
               [12.8, 14.7], [15.5, 15.35], [18.3, 15.9], [20.2, 17.2], [20.2, 18.9], [18.6, 20.3], [15.4, 20.55],
               [12.3, 20.3], [10.8, 19.4], [10.7, 18.6]],
              [2.2, 2.2, 1.2, 1.0, 1.3, 2.8, 3.1, 1.8, 1.2, 2.0, 3.0, 2.8, 1.3, 1.0, 1.3, 2.4, 2.4])
    bars = [vline([[13.2, 6.0], [12.4, 25.0]], 1.2), vline([[17.9, 6.0], [17.0, 24.9]], 1.2)]
    return U(S, *bars)


def glyph_fun(state):
    eyes = [vline([[12.3, 6.3], [14.4, 8.7]], 1.6), vline([[19.3, 7.3], [21.4, 9.6]], 1.6)]
    lip = vline([[24.3, 12.3], [23.0, 13.35], [20.0, 13.45], [16.5, 12.95], [13.0, 11.85], [10.0, 10.65], [7.5, 9.5]], 1.25)
    mouth = vline([[7.9, 9.9], [9.0, 11.2], [9.5, 13.2], [10.0, 15.8], [10.7, 18.2], [11.9, 20.3], [13.6, 22.0], [15.6, 23.1],
                   [17.6, 23.2], [19.6, 22.1], [20.5, 20.3], [20.8, 18.5], [21.9, 17.0], [22.7, 15.3], [23.0, 13.4]], 1.25)
    band = vline([[9.9, 15.6], [16.0, 16.35], [22.3, 16.9]], [2.2, 2.6, 2.8])
    lower = vline([[11.2, 23.1], [12.3, 24.6], [14.0, 25.7], [15.7, 26.1], [17.6, 25.5], [19.6, 24.4], [21.2, 23.2]], 1.2)
    return U(*eyes, lip, mouth, band, lower)


def glyph_skill(state):
    ball = {"t": "outline", "of": circ(14.2, 11.6, 5.0), "w": 1.35}
    tee = U(poly([[13.9, 17.9], [18.5, 17.9], [18.1, 18.8], [17.1, 19.5], [15.5, 19.5], [14.3, 18.8]]),
            poly([[15.95, 19.2], [17.1, 19.2], [17.0, 24.2], [16.5, 25.1], [16.0, 24.2]]))
    head = poly([[10.6, 21.0], [11.9, 21.25], [12.7, 22.0], [18.6, 22.0], [19.4, 21.2], [20.1, 21.6], [19.9, 22.6],
                 [17.4, 23.3], [16.2, 23.95], [11.2, 23.95], [10.1, 23.3], [9.9, 22.1]], smooth=1)
    shaft = vline([[21.3, 15.0], [22.8, 16.3], [23.3, 17.8], [22.7, 19.4], [21.1, 20.7], [19.6, 21.6]], [1.2, 1.3, 1.35, 1.35, 1.4, 1.4])
    parts = [ball, tee, head, shaft]
    if state == "a":                                   # only the lit cell shows these two
        parts += [circ(14.5, 11.8, 1.15), vline([[23.7, 10.2], [25.1, 13.4]], 1.15)]
    return U(*parts)


GLYPHS = {"cash": glyph_cash, "fun": glyph_fun, "skill": glyph_skill}


# ------------------------------------------------------------------------------------------------------------- bubble
def hat(t):
    t = np.clip(t, 0, 1)
    W = np.zeros((len(t), len(T)))
    for k in range(len(T) - 1):
        a, b = T[k], T[k + 1]
        inside = (t >= a) & (t <= b)
        f = (t - a) / (b - a)
        W[inside, k] += 1 - f[inside]
        W[inside, k + 1] += f[inside]
    return W


def fit_radial(art, mask, R):
    h, w = mask.shape
    jj, ii = np.mgrid[0:h, 0:w]
    x, y = ii[mask] + 0.5, jj[mask] + 0.5
    best = None
    for cx in np.arange(3, 22, 0.5):
        for cy in np.arange(3, 22, 0.5):
            W = hat(np.hypot(x - cx, y - cy) / R)
            cols = np.linalg.lstsq(W, art[mask], rcond=None)[0]
            res = np.abs(W @ cols - art[mask]).mean()
            if best is None or res < best[0]:
                best = (res, cx, cy, cols)
    return best


def dilate(m, k):
    for _ in range(k):
        m = m | np.roll(m, 1, 0) | np.roll(m, -1, 0) | np.roll(m, 1, 1) | np.roll(m, -1, 1)
    return m


_cache = {}


def _pixels(kind, state):
    x, y, w, h = CELLS[kind][0 if state == "a" else 1]
    art, m = C.crop(C.CI, x, y, w, h)
    import el_medals as M1                     # round 1's circle and rim-width measurements (they were right)
    cx, cy, r = M1.outer_circle(art, m)
    rw = M1.rim_width(art, cx, cy, r) - 0.5
    L = fit.lum(art)
    dark = G.box(L, 3) - L
    jj, ii = np.mgrid[0:h, 0:w]
    dist = np.hypot(ii + 0.5 - cx, jj + 0.5 - cy)
    glyph = m & (dark > 22) & (dist < r - rw - 0.5)
    # the glyph and its halo: within 1.4 px of the authored glyph (2.6 px in cell a, where the white glow lies)
    gd = sdf.sd(GLYPHS[kind](state), ii + 0.5, jj + 0.5)          # distance to the authored glyph
    excl = gd < (2.6 if state == "a" else 1.4)
    body = m & (dist < r - rw - 0.8) & ~excl
    return art, m, cx, cy, r, rw, L, glyph, body, ii, jj, dist


def measure(kind, state):
    """The bubble is the same art under all three glyphs, so its paint is fitted on the body pixels of all three cells
    (each with its own glyph masked out); the glyph ink and the glow are measured per cell."""
    key = (kind, state)
    if key in _cache:
        return _cache[key]
    if ("bubble", state) not in _cache:
        X, Y, V, RIM_T, RIM_V, geo = [], [], [], [], [], []
        for k in CELLS:
            art, m, cx, cy, r, rw, L, glyph, body, ii, jj, dist = _pixels(k, state)
            X.append(ii[body] + 0.5 - cx); Y.append(jj[body] + 0.5 - cy); V.append(art[body])
            rim = m & (dist > r - rw + 0.3) & (dist < r - 0.6)
            RIM_T.append(np.clip(((ii + 0.5 - cx) * RIM_DIR[0] + (jj + 0.5 - cy) * RIM_DIR[1])[rim] / r, 0, 1))
            RIM_V.append(art[rim])
            geo.append((cx, cy, r, rw))
        X, Y, V = np.concatenate(X), np.concatenate(Y), np.concatenate(V)
        cx, cy, r, rw = np.mean(geo, 0)
        R = r - rw + 2.0
        best = None
        for lx in np.arange(-10, 4, 0.5):              # the light centre, relative to the bubble's centre
            for ly in np.arange(-10, 4, 0.5):
                W = hat(np.hypot(X - lx, Y - ly) / R)
                cols = np.linalg.lstsq(W.T @ W + 20.0 * SMOOTH, W.T @ V, rcond=None)[0]
                res = np.abs(W @ cols - V).mean()
                if best is None or res < best[0]:
                    best = (res, lx, ly, cols)
        t = np.concatenate(RIM_T)
        rimc = np.linalg.lstsq(np.stack([1 - t, t], 1), np.concatenate(RIM_V), rcond=None)[0]
        _cache[("bubble", state)] = {"geo": (cx, cy, r, rw), "light": (best[1], best[2]), "R": R, "cols": best[3],
                                     "rim": rimc, "mad": best[0], "n": len(V)}
    B = _cache[("bubble", state)]
    art, m, cx, cy, r, rw, L, glyph, body, ii, jj, dist = _pixels(kind, state)
    out = {"c": [round(cx, 2), round(cy, 2)], "r": round(r, 2), "rw": round(rw, 2),
           "light": [round(cx + B["light"][0], 2), round(cy + B["light"][1], 2)], "R": round(B["R"], 2),
           "stops": [[t_, [int(round(v)) for v in np.clip(c_, 0, 255)]] for t_, c_ in zip(T, B["cols"])],
           "rim": [[int(round(v)) for v in np.clip(c_, 0, 255)] for c_ in B["rim"]],
           "ink": [int(v) for v in np.median(art[glyph], 0)],
           "glow": [int(v) for v in np.median(art[m & (L > 226)], 0)] if state == "a" else None,
           "body_mad": round(float(B["mad"]), 2), "body_n": int(B["n"])}
    _cache[key] = out
    return out


def medal_scene(kind, state):
    q = measure(kind, state)
    (cx, cy), r, rw = q["c"], q["r"], q["rw"]
    outer = circ(cx, cy, r)
    body = circ(cx, cy, round(r - rw, 2))
    G_ = GLYPHS[kind](state)
    rim_paint = {"linear": {"a": [cx, cy], "b": [round(cx + RIM_DIR[0] * r, 2), round(cy + RIM_DIR[1] * r, 2)],
                            "stops": [[0, q["rim"][0]], [1, q["rim"][1]]]}}
    L = []
    if state == "b":
        L.append(lay(circ(cx + 1, cy + 1, r), [14, 14, 20]))                                     # drop shadow
    L.append(lay(outer, rim_paint))
    L.append(lay(body, {"radial": {"c": q["light"], "r": q["R"], "stops": q["stops"]}}, soft=0.55))
    if state == "a":
        L.append(lay({"t": "grow", "of": G_, "d": 1.25}, q["glow"], soft=0.3, clip=body))      # the glow: crisp, not blurred
    L.append(lay(G_, q["ink"], clip=body))
    return [{"box": [-1, -1, 34, 34], "layers": L}]


HOW = {"cash": "a serif dollar sign: an S whose curves are 3 px and whose runs are 1 px, a drooping terminal at the top right, "
               "a ball terminal at the bottom left, two thin bars through it (one continuous variable-width stroke)",
       "fun": "a laughing face: two squinting eye strokes, an open mouth (upper lip slanting from the left corner to a right "
              "corner that flicks up, the outline down both sides and round the bottom, the dark band across it) and the lower "
              "lip as a second curve below",
       "skill": "a golf ball (ring) on a tee (cup and spike), an iron soled in front of the tee (toe curled up, hosel on the "
                "right, the spike showing below it) and a curved stroke bowing out from the hosel beside the ball; cell a adds "
                "the dot in the ball's centre and a short stroke right of the ball, as the art does"}
CHANGED = {
    "cash": "redone: the S now has the art's thick-and-thin serif shape and terminals (round 1: an even 2.3 px S); the bubble "
            "lighting is a measured radial body + a rim lit from the left (no smudges), the glow is crisp",
    "fun": "redone: the mouth is one continuous outline with the lower lip as its own curve, eyes as squints; bubble lighting "
           "measured (radial body, rim lit from the left), crisp glow instead of a blur",
    "skill": "redone from a new read (round 1 was blocked): ball on a tee with an iron soled in front of it, every stroke "
             "continuous; bubble lighting measured, crisp glow"}
UNSURE = {"skill": "reading still uncertain: ball on a tee + iron fits every stroke except the curved one beside the ball "
                   "(rows 15-21), which bows out and hooks back instead of rising like a shaft, and the short stroke right of "
                   "the ball that only cell a shows; both drawn as the art has them. A golfer figure (ring = head, the tee = "
                   "shoulders and body, the curve = arms swinging the club) is the other reading; Mariano may know which"}
ELEMENTS = {}
for _k in CELLS:
    ELEMENTS[f"course-{_k}-medal"] = {
        "how": "blue glass bubble: navy rim lit from the left (two colours along the light direction, measured), body = radial "
               "gradient around a light centre left of the middle (fitted), cell b a near-black drop shadow 1 px down-right, "
               "cell a a crisp white glow (glyph grown 1.25 px) behind the glyph; glyph engraved in the measured ink: " + HOW[_k],
        "changed": CHANGED[_k], "unsure": UNSURE.get(_k, ""), "z": 10,
        "states": [("a", C.CI, CELLS[_k][0], (lambda k=_k: medal_scene(k, "a"))),
                   ("b", C.CI, CELLS[_k][1], (lambda k=_k: medal_scene(k, "b")))]}
