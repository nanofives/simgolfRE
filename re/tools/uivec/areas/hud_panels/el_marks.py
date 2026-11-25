"""P41 hud_panels: the Member list's small marks and plain chrome: hearts, the heart pips, the glossy ball, the white bars,
the two-bar slider, the skill bar, the page arrows (Member list and Employee panel) and the mood faces.
Every shape below was read off hex / character dumps of MemberPanel.pcx and EmployeePanel.pcx (2026-11-24); colours are
sampled on the art per cell (kit.sample / sample_dark), gradients are the art's own row or column profile of a flat lit box.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import fit  # noqa: E402
import sdf  # noqa: E402
from kit import U, line, curve, poly, rect, circle, ellipse, inter, sub, outline  # noqa: E402,F401

BG = "M"


def cell_bg(key, rct):
    """The flat lilac the marks sit on (the cell's corner pixel)."""
    x0, y0, w, h = rct
    return K.sample(key, rect(x0, y0, x0 + 1, y0 + 1), depth=0.0)


def vgrad(key, x, y0, y1, xa=None, xb=None):
    """A vertical linear paint through the art's colours down column x from row y0 to y1 - 1 (one stop per row)."""
    rows = list(range(y0, y1))
    st = [[(r - y0) / max(len(rows) - 1, 1), K.sample(key, rect(x, r, x + 1, r + 1), depth=0.0)] for r in rows]
    return {"linear": {"a": [0, y0 + 0.5], "b": [0, y1 - 0.5], "stops": st}}


def opaque_box(key, rct):
    rgb, m = C.sheet(key)
    x0, y0, w, h = rct
    mk = m[y0:y0 + h, x0:x0 + w]
    ys, xs = np.nonzero(mk)
    return x0 + xs.min(), y0 + ys.min(), x0 + xs.max() + 1, y0 + ys.max() + 1


# ------------------------------------------------------------------------------------------------------------ hearts
def heart(x0, y0, x1, y1):
    """A small heart filling the box: two round lobes on top and a point at the bottom middle."""
    w, h = x1 - x0, y1 - y0
    r = w * 0.24
    lob = U(circle(x0 + w * 0.27, y0 + r + 0.1, r + 0.15), circle(x1 - w * 0.27, y0 + r + 0.1, r + 0.15))
    tri = poly((x0 + 0.05, y0 + r + 0.6), (x1 - 0.05, y0 + r + 0.6), ((x0 + x1) / 2, y1))
    return U(lob, tri, rect(x0 + w * 0.29, y0 + 0.2, x1 - w * 0.29, y0 + r * 2))


def heart_layers(key, box):
    """One heart (read on M 450,4 grey and 600,14 red): the dark outline is its top edge and left side, a light line runs
    along its lower right; the fill is flat grey or red with a lighter middle."""
    x0, y0, x1, y1 = box
    hs = heart(x0 + 1.0, y0 + 1.0, x1 - 1.0, y1 - 1.1)
    big = heart(x0, y0, x1, y1)
    fill = K.sample(key, rect(x0 + 2, y0 + 2, x1 - 2, y0 + 4), depth=0.0)
    dark = K.sample_dark(key, rect(x0, y0, x1, y0 + 1), q=0.4)
    light = K.sample(key, rect(x1 - 2, y1 - 4, x1 - 1, y1 - 3), depth=0.0)
    return [K.lay({"t": "move", "of": big, "d": [0.5, 0.5]}, light, alpha=0.8),
            K.lay(big, dark),
            K.lay(hs, fill),
            K.lay(ellipse((x0 + x1) / 2 - 0.3, y0 + 2.6, 1.4, 0.9), K.sample(key, rect((x0 + x1) / 2 - 1, y0 + 2, (x0 + x1) / 2, y0 + 3), depth=0.0),
                  alpha=0.7)]


def heart_boxes(key, x0, y0, w, h):
    """The bounding box of each heart in a cell: runs of rows holding pixels clearly off the cell's lilac (the hearts are
    9 rows apart; the faint shading between them is left out by the threshold)."""
    rgb, m = C.sheet(key)
    a = rgb[y0:y0 + h, x0:x0 + w]
    bg = a[0, 0]
    diff = (np.abs(a - bg).max(axis=2) > 34) & m[y0:y0 + h, x0:x0 + w]
    rows = np.nonzero(diff.any(axis=1))[0]
    boxes, start = [], None
    for r in range(h + 1):
        on = r < h and diff[r].any()
        if on and start is None:
            start = r
        if not on and start is not None:
            cols = np.nonzero(diff[start:r].any(axis=0))[0]
            boxes.append((x0 + cols.min(), y0 + start, x0 + cols.max() + 1, y0 + r))
            start = None
    return boxes


def build_hearts(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), cell_bg(key, rct))]
        for b in heart_boxes(key, x0, y0, w, h):
            L += heart_layers(key, b)
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


def build_pips(el):
    """The two 7 px cells are single hearts (a: red, b: grey), cut from the heart column with its lilac."""
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        bx = opaque_box(key, rct)
        L = [K.lay(rect(*bx), cell_bg(key, rct))]
        for b in heart_boxes(key, x0, y0, w, h):
            L += heart_layers(key, b)
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


# ------------------------------------------------------------------------------------------------------------ ball, bars
def ball_layers(key, cx, cy, r):
    """A glossy white ball in a navy line: the line a circle, the ball's paint a radial gradient from a light spot up and
    left of the centre, through the art's colours along the diagonal from the upper left to the lower right."""
    stops = []
    sx, sy = cx - 0.35 * r, cy - 0.4 * r
    for t in np.linspace(0, 1, 7):
        px, py = sx + (cx + 0.7 * r - sx) * t, sy + (cy + 0.7 * r - sy) * t
        stops.append([float(t), K.sample(key, rect(int(px), int(py), int(px) + 1, int(py) + 1), depth=0.0)])
    R = np.hypot(cx + 0.7 * r - sx, cy + 0.7 * r - sy)
    return [K.lay(circle(cx, cy, r), K.sample_dark(key, outline(circle(cx, cy, r - 0.5), 1.0), q=0.5)),
            K.lay(circle(cx, cy, r - 1.0), {"radial": {"c": [sx, sy], "r": float(R), "stops": stops}})]


def fit_disc(key, rct):
    rgb, m = C.sheet(key)
    x0, y0, w, h = rct
    pts = fit.edge_points(m[y0:y0 + h, x0:x0 + w]) + [x0, y0]
    cx, cy, r, _ = fit.fit_circle(pts, trim=1.0)
    return cx, cy, r


def build_ball(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        cx, cy, r = fit_disc(key, rct)
        scenes[st] = [K.item(ball_layers(key, cx, cy, r), box=[x0, y0, x0 + w, y0 + h])]
    return scenes


def bar_layers(key, x0, y0, x1, y1, r=2.5):
    """A white glossy bar: a navy outline rounded rectangle, the inside one pixel in with the art's own top-to-bottom
    colours (white at the top, grey or pink-grey at the bottom), measured down the bar's middle column."""
    xm = int((x0 + x1) / 2)
    return [K.lay(rect(x0, y0, x1, y1, r), K.sample(key, rect(xm, y0, xm + 1, y0 + 1), depth=0.0)),
            K.lay(rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, r - 1), vgrad(key, xm, int(y0) + 1, int(y1) - 1))]


def build_bars(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        bx = opaque_box(key, rct)
        scenes[st] = [K.item(bar_layers(key, *bx, r=3.0), box=[x0, y0, x0 + w, y0 + h])]
    return scenes


def build_slider(el):
    """The two-bar slider (M 0,0): a lilac block, two white bars (x 10..101, y 2..17 and 24..39) and the ball knob on
    the left (a disc round (9.5, 19.5) r 8, overlapping both bars), and the block's dark right edge line (x 120)."""
    st, key, rct = C.CELLS[el][0]
    x0, y0, w, h = rct
    L = [K.lay(rect(x0, y0, x0 + w, y0 + h), cell_bg(key, rct))]
    L.append(K.lay(rect(x0 + w - 1, y0, x0 + w, y0 + h), K.sample(key, rect(120, 10, 121, 30), depth=0.0)))
    L += bar_layers(key, 10.0, 2.0, 101.0, 17.0, 3.0)
    L += bar_layers(key, 10.0, 23.0, 101.0, 38.0, 3.0)
    rgb, m = C.sheet(key)
    knob = (rgb[0:44, 0:20].sum(axis=2) > 600)
    j, i = np.nonzero(knob)
    cx, cy = 9.6, 19.4
    L += ball_layers(key, cx, cy, 8.0)
    return {st: [K.item(L, box=[x0, y0, x0 + w, y0 + h])]}


# ------------------------------------------------------------------------------------------------------------ skill bar
def build_skill(el):
    """The skill bar (M 200,200, 83 x 6): seven separate sunken sockets 11 px apart on the lilac (each a dark rounded slot
    x 201.4 + 11 k .. 212.6 + 11 k over rows 0..3, a light line under it on row 4, its 8 x 2 light at x 203 + 11 k); the lit ones are red, orange, amber, yellow, lime, green, bright green
    from the left, the unlit ones navy; a lighter divider between sockets."""
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), cell_bg(key, rct))]
        for k in range(7):
            sx = x0 + 3 + 11 * k
            cxs = int(sx + 4)
            L.append(K.lay(line((sx - 1.0, y0 + 4.5), (sx + 9.4, y0 + 4.5), w=1.0),
                           K.sample(key, rect(cxs, y0 + 4, cxs + 1, y0 + 5), depth=0.0)))
            L.append(K.lay(rect(sx - 1.6, y0, sx + 9.6, y0 + 4, 1.4), K.sample(key, rect(cxs, y0, cxs + 1, y0 + 1), depth=0.0)))
            L.append(K.lay(rect(sx, y0 + 1, sx + 8, y0 + 3, 0.4), K.sample(key, rect(sx + 2, y0 + 1, sx + 6, y0 + 3), depth=0.0)))
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


# ------------------------------------------------------------------------------------------------------------ arrows
def build_member_arrows(el):
    """Two cream triangles side by side on a lilac tab (M 0,150 / 0,250): the tab's two top rows lighter; each triangle
    has its tip at the left (or right) on y 156.5 and an upright base 12 px away (y 151.6..161.6), a dark line along
    its upper edge (its shadow side) and a soft light edge along the lower one."""
    scenes = {}
    right = el.endswith("right")
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), vgrad(key, x0 + 17, y0, y0 + h))]
        for k in range(2):
            if not right:
                tip, base = x0 + 1.2 + 18 * k, x0 + 15.6 + 18 * k
            else:
                tip, base = x0 + w - 1.2 - 18 * (1 - k), x0 + w - 15.6 - 18 * (1 - k)
            ty = y0 + 6.6
            tri = poly((tip, ty), (base, y0 + 1.6), (base, y0 + 11.6))
            mx = (tip + base) / 2
            cream = K.sample(key, rect(int(mx) + (1 if not right else -2), int(ty), int(mx) + (2 if not right else -1), int(ty) + 1), depth=0.0)
            edge = K.sample_dark(key, line((tip + (base - tip) * 0.3, ty - 1.6), (tip + (base - tip) * 0.6, ty - 3.4), w=0.6), q=0.3)
            L += [K.lay(tri, cream),
                  K.lay(line((tip + (base - tip) * 0.08, ty - 0.5), (base, y0 + 1.6), w=1.1), edge, alpha=0.8)]
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


def build_employee_arrows(el):
    """The Employee panel's tall page tabs (16 x 62, flat lavender): one long dart pointing out, its tip at mid height on
    the outer side (x 1, y 231.5 on E 0,200) and its back a shallow chevron (x 15 at the top and bottom ends, 13 at mid
    height), read from the luminance rows; lighter lilac idle, near white lit, darker than the tab disabled."""
    scenes = {}
    right = el.endswith("right")
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        X = (lambda u: x0 + u) if not right else (lambda u: x0 + w - u)  # noqa: E731
        dart = poly((X(1.0), y0 + 31.5), (X(15.0), y0 + 0.6), (X(13.0), y0 + 31.5), (X(15.0), y0 + 62.0))
        tab = rect(x0, y0, x0 + w, y0 + h)
        cx = int(X(8.0)) - (1 if right else 0)
        L = [K.lay(tab, K.sample(key, rect(int(X(3.0)) - (1 if right else 0), y0 + 5, int(X(3.0)) + (0 if right else 1), y0 + 10), depth=0.0)),
             K.lay(dart, K.sample(key, rect(cx, y0 + 29, cx + 1, y0 + 34), depth=0.0))]
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


# ------------------------------------------------------------------------------------------------------------ faces
FACES = {   # cell-relative (0..16): eyes (cx, cy, rx, ry, hollow), mouth kind and numbers, brows
    1: dict(eyes=[(5.0, 4.6, 1.6, 2.1, True), (10.7, 4.6, 1.6, 2.1, True)], mouth=("grin", 8.6)),
    2: dict(eyes=[(5.0, 4.6, 1.6, 2.1, True), (10.7, 4.6, 1.6, 2.1, True)], mouth=("teeth smile", 10.2)),
    3: dict(eyes=[(5.2, 4.9, 1.5, 1.9, False), (10.8, 4.9, 1.4, 1.9, False)],
            mouth=("line", [(2.6, 9.2), (5.0, 11.2), (7.6, 11.6), (10.4, 11.2), (12.8, 9.2)], 1.1)),
    4: dict(eyes=[(5.6, 5.0, 1.1, 1.8, False), (10.2, 5.0, 1.1, 1.8, False)],
            mouth=("line", [(2.6, 9.4), (5.0, 11.2), (7.4, 11.6), (9.8, 11.2), (11.8, 9.4)], 1.1)),
    5: dict(eyes=[(5.3, 6.0, 1.4, 2.2, False), (11.3, 6.0, 1.4, 2.2, False)], mouth=("line", [(5.0, 11.5), (11.8, 11.5)], 1.0)),
    6: dict(eyes=[(5.0, 5.4, 2.0, 1.5, False), (11.6, 5.4, 2.0, 1.5, False)], mouth=("line", [(3.0, 10.5), (13.0, 10.5)], 1.0)),
    7: dict(eyes=[(4.9, 4.9, 1.8, 1.8, False), (10.6, 4.9, 1.8, 1.8, False)],
            mouth=("line", [(2.0, 11.4), (4.6, 9.9), (7.5, 9.4), (10.4, 9.9), (12.8, 11.4)], 1.3)),
    8: dict(eyes=[(5.0, 5.2, 1.6, 1.4, False), (10.6, 5.2, 1.6, 1.4, False)], brows=[((2.6, 3.0), (6.8, 4.2)), ((13.0, 3.0), (8.8, 4.2))],
            mouth=("open", 7.6, 11.0, 4.4, 2.2)),
    9: dict(eyes=[(5.0, 5.2, 1.6, 1.4, False), (10.6, 5.2, 1.6, 1.4, False)], brows=[((2.6, 3.0), (6.8, 4.2)), ((13.0, 3.0), (8.8, 4.2))],
            mouth=("open", 7.6, 11.0, 5.4, 2.4)),
    10: dict(eyes=[(5.0, 5.4, 1.6, 1.3, False), (10.6, 5.4, 1.6, 1.3, False)],
             brows=[((1.8, 1.6), (6.8, 3.6)), ((13.8, 1.6), (8.8, 3.6))], mouth=("teeth", 2.6, 9.0, 13.0, 12.2)),
}


def face_layers(key, ox, oy, n):
    """One 16 px mood face: a disc (round (7.8, 7.7) r 7.6) in its colour with a darker rim line, the eyes, and the mouth."""
    S = FACES[n]
    disc = circle(ox + 7.8, oy + 7.7, 7.6)
    fill = K.sample(key, rect(ox + 12, oy + 8, ox + 13, oy + 9), depth=0.0)     # the right cheek
    rim = K.sample(key, rect(ox + 0, oy + 7, ox + 1, oy + 9), depth=0.0)
    dark = K.sample_dark(key, rect(ox + 2, oy + 2, ox + 14, oy + 14), q=0.12)
    white = [235, 235, 225]
    L = [K.lay(disc, rim), K.lay(circle(ox + 7.8, oy + 7.7, 6.6), fill)]
    for cx, cy, rx, ry, hollow in S["eyes"]:
        e = ellipse(ox + cx, oy + cy, rx, ry)
        L.append(K.lay(outline(e, 0.9) if hollow else e, dark))
    for a, b in S.get("brows", []):
        L.append(K.lay(line((ox + a[0], oy + a[1]), (ox + b[0], oy + b[1]), w=1.2, cap="round"), dark))
    m = S["mouth"]
    if m[0] in ("grin", "teeth smile"):
        top = m[1]
        ry = 12.8 - top
        half = inter(ellipse(ox + 7.8, oy + top, 5.6, ry), rect(ox + 1, oy + top, ox + 15, oy + 15))
        L += [K.lay(half, dark), K.lay(inter(ellipse(ox + 7.8, oy + top + 0.2, 4.2, ry - 1.1), rect(ox + 1, oy + top + 0.7, ox + 15, oy + 15)), white)]
    elif m[0] == "line":
        L.append(K.lay(curve(*[(ox + x, oy + y) for x, y in m[1]], w=m[2], cap="round", smooth=2), dark))
    elif m[0] == "open":
        _, cx, cy, rx, ry = m
        L.append(K.lay(ellipse(ox + cx, oy + cy, rx, ry), dark))
    elif m[0] == "teeth":
        _, ax, ay, bx, by = m
        L += [K.lay(rect(ox + ax, oy + ay, ox + bx, oy + by, 1.0), dark),
              K.lay(rect(ox + ax + 1.2, oy + ay + 0.9, ox + bx - 1.2, oy + by - 0.9), white)]
        for t in range(1, 5):
            xx = ox + ax + 1.2 + t * (bx - ax - 2.4) / 5
            L.append(K.lay(line((xx, oy + ay + 0.9), (xx, oy + by - 0.9), w=0.7), dark))
    return L


def build_faces(el):
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        x0, y0, w, h = rct
        L = [K.lay(rect(x0, y0, x0 + w, y0 + h), cell_bg(key, rct))]
        first = 1 if el == "member-mood-faces" else 5
        for k in range(w // 16):
            L += face_layers(key, x0 + 16 * k, y0, first + k)
        scenes[st] = [K.item(L, box=[x0, y0, x0 + w, y0 + h])]
    return scenes


EB.BUILD.update({"member-hearts": build_hearts, "member-pips": build_pips, "member-ball": build_ball, "member-bar": build_bars,
                 "member-slider": build_slider, "member-skill-bar": build_skill, "member-page-left": build_member_arrows,
                 "member-page-right": build_member_arrows, "employee-page-left": build_employee_arrows,
                 "employee-page-right": build_employee_arrows, "member-mood-faces": build_faces,
                 "member-mood-faces-extra": build_faces})
