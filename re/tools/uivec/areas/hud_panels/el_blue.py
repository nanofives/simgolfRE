"""P41 hud_panels: the blue glass tab bubbles of the three trays (golfer, golfer with a post, group of three), 33 px cells.
A blue tab is: a lilac halo disc behind the bubble (light at the top, lavender at the bottom; its circle fitted to the
cell's opaque outline), the navy rim, the blue glass body (measured paint family "blue") and the navy glyph.
Glyphs read from coverage dumps of 22 px bodies (2026-11-24): small figures, so each part is a line or a small disc. The
glyph is one navy: the darkest third of the art's pixels under it (kit.sample_dark).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import bubble as B  # noqa: E402
import kit as K  # noqa: E402
import el_bubbles as EB  # noqa: E402
import fit  # noqa: E402
from kit import U, line, curve, poly, rect, circle, ellipse, outline  # noqa: E402,F401


def glyph_golfer():
    """A golfer in the follow-through (reference: M 0,400): the club held level behind the head, its shaft along y 409 from
    the hands (17.4, 409) back to x 8.4, where its head turns down to (7.4, 411.6); head round (19.6, 410) r 1.6; the right
    elbow out to (22.4, 410.4); a slim body down to the hips (18.2, 420.6); the front leg straight down to the foot
    (18.9, 425.4) with the foot along y 425.3 to x 20.6; the back leg bent down-left to a toe on the ground at (11, 424.8)."""
    return [
        ("club shaft", line((8.4, 409.0), (17.4, 409.0), w=1.1), ("sample_in", circle(14.5, 409.5, 0.3))),
        ("club head", line((8.6, 408.6), (7.6, 409.6), (7.4, 411.6), w=1.2, cap="round"), ("as", "club shaft")),
        ("head", circle(19.6, 410.1, 1.6), ("sample_in", circle(19.5, 409.5, 0.4))),
        ("arm", line((20.0, 411.6), (22.4, 410.4), w=1.2, cap="round"), ("as", "head")),
        ("body", line((19.6, 411.6), (18.6, 418.8), w=2.0, cap="round"), ("as", "head")),
        ("hips", line((18.6, 418.6), (17.9, 420.8), w=2.0, cap="round"), ("as", "head")),
        ("front leg", line((18.9, 420.4), (18.9, 425.2), w=1.4), ("as", "head")),
        ("front foot", line((18.4, 425.4), (20.6, 425.2), w=1.2, cap="round"), ("as", "head")),
        ("back leg", line((17.8, 420.6), (15.4, 422.0), (13.2, 423.4), (11.0, 424.8), w=1.4, cap="round"), ("as", "head")),
    ]


def glyph_staff():
    """A man beside an upright post (reference: M 0,450): the head drawn as a dark oval line with a light face inside (round
    (15, 461.8); columns 14 and 16 dark, 15 light over rows 460..463), the left arm raised (shoulder (12.8, 464.6), elbow (10.6,
    463.4), hand (11.6, 461.4)), the right arm reaching up to the post (21, 462.4); a torso block x 12.6..17.4, y 463.8..470;
    two legs (x 13.6, 16.6) to y 473.6 and two feet turned out (x 10.4..13.6 and 16.4..19.4 at y 474.6); the post x 22.4
    from y 460 to 472.8 with a small knob on top (22.2, 460.6)."""
    return [
        ("post", line((22.4, 460.0), (22.4, 472.8), w=1.0), ("sample_in", circle(22.5, 466.5, 0.3))),
        ("knob", circle(21.8, 460.9, 1.1), ("sample_in", circle(21.5, 461.5, 0.3))),
        ("head", outline(ellipse(15.0, 461.8, 1.4, 1.9), 0.9), ("as", "head")),
        ("left arm", line((12.8, 464.6), (10.6, 463.4), (11.6, 461.4), w=1.1, cap="round"), ("as", "head")),
        ("right arm", line((17.2, 464.6), (19.4, 463.8), (21.2, 462.4), w=1.0, cap="round"), ("as", "head")),
        ("torso", rect(12.6, 463.8, 17.4, 470.2, 0.6), ("sample_in", circle(15.5, 465.5, 0.4))),
        ("left leg", line((13.6, 470.0), (13.6, 473.8), w=1.3), ("as", "head")),
        ("right leg", line((16.6, 470.0), (16.6, 473.8), w=1.3), ("as", "head")),
        ("left foot", line((10.4, 474.6), (13.8, 474.6), w=1.3, cap="round"), ("as", "head")),
        ("right foot", line((16.4, 474.6), (19.4, 474.6), w=1.3, cap="round"), ("as", "head")),
    ]


def figure(hx, hy, x0, x1, top, hip, legs, foot):
    """A standing figure with its hands on its hips: head (hx, hy), a shoulder bar x0..x1 at `top`, elbows out one pixel
    beyond the bar and the hands back in at the hips, a body down to `hip`, two legs at x legs[0], legs[1] down to foot."""
    cx = (x0 + x1) / 2
    return [
        ("head", circle(hx, hy, 1.35)),
        ("shoulders", line((x0 + 0.6, top), (x1 - 0.6, top), w=1.4, cap="round")),
        ("left arm", line((x0 + 0.6, top), (x0 - 0.2, top + 1.8), (x0 + 1.4, hip - 0.6), w=1.1, cap="round")),
        ("right arm", line((x1 - 0.6, top), (x1 + 0.2, top + 1.8), (x1 - 1.4, hip - 0.6), w=1.1, cap="round")),
        ("body", rect(cx - 1.3, top, cx + 1.3, hip, 0.5)),
        ("left leg", line((legs[0], hip - 0.4), (legs[0], foot), w=1.2)),
        ("right leg", line((legs[1], hip - 0.4), (legs[1], foot), w=1.2)),
    ]


def glyph_group():
    """Three figures (reference: J 0,400): two standing with their hands on their hips at the lower left (head (11.6,
    413.8)) and right (head (24.6, 411.8)), and a third jumping between and above them (head (17.6, 410.4), arms flung up
    and out to (14.6, 411) and (19.8, 411), legs apart down to y 418)."""
    P = []
    for tag, parts in (("left", figure(11.6, 413.6, 9.2, 14.6, 415.2, 419.0, (10.4, 12.8), 424.0)),
                       ("right", figure(24.6, 411.8, 21.0, 28.4, 414.2, 419.0, (21.6, 23.9), 424.0))):
        for n, s in parts:
            P.append((f"{tag} {n}", s, ("sample_in", circle(11.5, 416.5, 0.4)) if not P else ("as", P[0][0])))
    P += [("top head", circle(17.6, 410.4, 1.3), ("as", P[0][0])),
          ("top arms", line((14.4, 411.0), (16.6, 412.2), (18.6, 412.2), (19.8, 411.0), w=1.0, cap="round"), ("as", P[0][0])),
          ("top body", line((17.4, 412.0), (17.4, 416.2), w=1.5), ("as", P[0][0])),
          ("top legs", line((16.0, 418.2), (16.8, 416.0), (18.0, 416.0), (18.8, 418.2), w=1.0, cap="round"), ("as", P[0][0]))]
    return P


REF = {
    "tab-golfer": (glyph_golfer, "tab-golfer"), "employee-tab-golfer": (glyph_golfer, "tab-golfer"),
    "tab-golfer-swing": (glyph_golfer, "tab-golfer"),
    "tab-golfer-staff": (glyph_staff, "tab-golfer-staff"), "joecool-tab-staff": (glyph_staff, "tab-golfer-staff"),
    "tab-golfers-group": (glyph_group, "tab-golfers-group"), "tab-staff-group": (glyph_group, "tab-golfers-group"),
}


def halo(key, rct, c):
    """The halo disc: a circle fitted to the cell's opaque outline; its paint a vertical gradient through the colours of
    its ring (outside the rim) at five heights."""
    rgb, m = C.sheet(key)
    x0, y0, w, h = rct
    mk = m[y0:y0 + h, x0:x0 + w]
    pts = fit.edge_points(mk) + [x0, y0]
    hx, hy, hr, _ = fit.fit_circle(pts, trim=1.2)
    jj, ii = np.mgrid[y0:y0 + h, x0:x0 + w]
    ring = mk & (np.hypot(ii + 0.5 - c[0], jj + 0.5 - c[1]) > c[2] + 0.8)
    stops = []
    for t in np.linspace(0, 1, 5):
        yy = hy - hr + t * 2 * hr
        sel = ring & (np.abs(jj + 0.5 - yy) < 2.5)
        if sel.any():
            stops.append([float(t), [round(float(v), 1) for v in np.median(rgb[y0:y0 + h, x0:x0 + w][sel], axis=0)]])
    disc = circle(round(hx, 2), round(hy, 2), round(hr, 2))
    return [K.lay(disc, {"linear": {"a": [0, hy - hr], "b": [0, hy + hr], "stops": stops}})], disc


def measure_blue(key, rct):
    """The blue tab's rim and body. The navy rim is the first dark ring met coming in from the halo (luminance under 95 on
    rays from the halo's centre); the dark crescent inside the body's lower right is shading, not rim (measure_bubbles'
    navy fit had taken it for the rim: r 12.4 where the art's rim is r 14.4)."""
    rgb, m = C.sheet(key)
    x0, y0, w, h = rct
    mk = m[y0:y0 + h, x0:x0 + w]
    hx, hy, hr, _ = fit.fit_circle(fit.edge_points(mk) + [x0, y0], trim=1.2)
    L = fit.lum(rgb)
    pts = []
    for th in np.deg2rad(np.arange(0, 360, 4.0)):
        for k in np.arange(hr - 0.5, hr * 0.6, -0.25):
            px, py = hx + np.cos(th) * k, hy + np.sin(th) * k
            if L[int(py), int(px)] < 95:
                pts.append((px, py))
                break
    cx, cy, r, _ = fit.fit_circle(pts, trim=0.8)
    c = [round(cx, 2), round(cy, 2), round(r + 0.3, 2)]
    body = B.title.measure_body(rgb, c[0], c[1], c[2]) or [c[0], c[1], c[2] - 1.3]
    return c, body


def build(el):
    gl, ref = REF[el]
    rg = EB.GEOM[ref]["idle"]
    scenes = {}
    for st, key, rct in C.CELLS[el]:
        g = EB.GEOM[el][st]
        g["c"], g["body"] = measure_blue(key, rct)
        dx, dy = (0, 0) if el == ref else K.match_offset((rg["key"], rg["body"]), (key, g["body"]))
        ground, gshape = halo(key, rct, g["c"])
        parts = gl()
        allp = U(*[K.shift(sh, dx, dy) for _, sh, _ in parts])
        navy = K.sample_dark(key, allp, q=0.35)
        lay = [{"name": n, "shape": K.shift(sh, dx, dy), "fill": navy} for n, sh, _ in parts]
        EB.GU[(el, st)] = U(*[L["shape"] for L in lay])
        p = dict(EB.PAINT["blue"])
        rr = (g["c"][2] + g["body"][2]) / 2
        p["rim"] = K.sample_dark(key, outline(circle(g["c"][0], g["c"][1], rr), g["c"][2] - g["body"][2]), q=0.5, grow=0.0)
        it = B.item(g["c"], g["body"], p, glyph=lay)
        scenes[st] = [K.item(ground, box=[rct[0], rct[1], rct[0] + rct[2], rct[1] + rct[3]]), it]
    return scenes


EB.BUILD.update({k: build for k in REF})
