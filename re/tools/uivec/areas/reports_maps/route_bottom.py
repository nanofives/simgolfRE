"""Routing map bottom panel (route screens_bottom.pcx, y 253..600), read 2026-11-24.

What it IS: the lower part of the Routing map's lavender SLAB (navy line x 4.8 / 795.5, bottom y 537.8, the same slab as
the top band's), with the tan LIST floor of the top band running on down to y 392 (a light lip below it), and the MAP
WINDOW sunk below that (from y 408.5; its floor the marbled ball, kept art); over it the SCALLOPED ARCH: a lavender band
whose top is one big arc (circle centre (400, 842), r 585: peak y 257 at x 400) and four round SEATS for the mode bubbles
(circle fits r 40.67..41.01, residual 0.3), with four rounded slots cut through it where the marble shows (ellipse fits);
the four mode BUBBLES (route_buttons.py, idle); the diamond FRAME of Buy Land's board (its edges match pixel for pixel)
round a black window; the two big lit DISCS, the slab line at the lower corners, the empty SOCKET and the blue CHECK in
its socket (bl_backdrop.py, check_deliver.py: this region matches Buy Land's but for the marble).
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bl_backdrop as BB, bl_board as BD, bubbles as BU, check_deliver as CD   # noqa: E402,E401
import common as C, lit, parts as P, route_buttons as RB, route_tops as RT      # noqa: E402,E401
import sdf                                                                        # noqa: E402

SHEET = "route screens_bottom.pcx"
Y0 = 253
OFFS = {"staff": (60, 314), "value": (482, 314), "course": (-79, 255), "aura": (87, 255)}     # cell -> panel (template match)
SEATS = [(102.2, 355.41, 40.83), (224.26, 294.66, 41.01), (575.8, 294.59, 41.0), (697.91, 355.31, 40.67)]
HOLES = [(491.33, 289.56, 48.13, 9.6, 13.84), (306.93, 289.99, 46.9, 9.62, -12.67), (624.38, 333.47, 37.49, 25.95, 80.87),
         (175.63, 344.25, 30.26, 18.78, -21.51)]
SLAB = (4.8, 200.0, 795.5, 537.8, 8.0)
LIST = (18.5, 200.0, 780.5, 392.0)
WINDOW = (18.5, 408.5, 780.5, 660.0)
ARCH_FILE = os.path.join(C.OUT, "read", "route_arch_style.json")


ARC = (400.0, 842.0, 585.0, 571.5)      # the band: between these two radii (the slots' tops at y ~275 give its width)


def arch_shape():
    """A narrow band under the big arc (an annulus 13.5 px wide) and the four round seats; the marble shows below the band
    (a first reading filled everything under the arc and cut ellipse slots: it hid the marble the art shows)."""
    cx, cy, ro, ri = ARC
    band = {"t": "inter", "of": [{"t": "circle", "c": [cx, cy], "r": ro}, P.rr(18.5, 250.0, 780.5, 470.0)]}
    band = {"t": "sub", "a": band, "b": {"t": "circle", "c": [cx, cy], "r": ri}}
    seats = [{"t": "circle", "c": [x, y], "r": r} for x, y, r in SEATS]
    return {"t": "union", "of": [band] + seats}


def frame_outer():
    g, st = BD.load()
    return {"t": "poly", "rings": [BD.diamond(*g["outer"])]}


def solve_arch():
    rgb, m = C.sheet(SHEET)
    jj, ii = np.mgrid[0:600, 0:800]
    x, y = ii + 0.5, jj + 0.5
    keep = (jj >= Y0) & (sdf.sd(frame_outer(), x, y) > 2.0)
    for name, (dx, dy) in OFFS.items():
        cx, cy, R = RB.CENTRES[(name, "idle")]
        keep &= np.hypot(x - cx - dx, y - cy - dy) > R + 1.5
    st = lit.new_style(ow=0.0, L=6.0, out_w=4.0)
    lit.solve([(arch_shape(), rgb, (0, 200, 800, 480), keep[200:480])], st, dir_in=1.0)
    json.dump(st, open(ARCH_FILE, "w"))
    return st


def scene():
    rs = RT.load_styles()
    ast = json.load(open(ARCH_FILE))
    bst = json.load(open(RB.STYLE_FILE))
    S = BU.load_style()
    sock = json.load(open(CD.SOCK_FILE))
    dst = json.load(open(BB.DISC_FILE))
    full = lambda L, n="": {"name": n, "box": [0, Y0 - 2, 800, 600], "layers": L}    # noqa: E731
    out = []
    slab = P.rr(*SLAB)
    win = P.rr(*WINDOW)
    lst = P.rr(*LIST)
    sl = lit.layers(slab, rs["slab"])
    fill = sl[-1]
    fill["clip"] = {"t": "sub", "a": P.rr(-10, -10, 810, 610), "b": {"t": "inter", "of": [P.rr(*WINDOW[:2], WINDOW[2], 660.0) if False else P.rr(18.5, 250.0, 780.5, 660.0), {"t": "circle", "c": ARC[:2], "r": ARC[2]}]}}
    out.append(full([fill], "slab"))
    ll = lit.layers(lst, rs["list_tan"])
    above = {"t": "sub", "a": P.rr(-10, -10, 810, 610), "b": {"t": "circle", "c": ARC[:2], "r": ARC[2]}}
    for L_ in ll:             # the tan floor shows above the arch only
        L_["clip"] = above
    out.append(full(ll, "list floor"))
    out.append(full(lit.layers(arch_shape(), ast), "arch"))
    for D_ in BB.disc_shapes():
        out.append(full(lit.layers(D_, dst), "disc"))
    out.append(full([{"shape": {"t": "outline", "of": slab, "w": rs["slab"]["ow"]}, "fill": rs["slab"]["line"],
                      "clip": {"t": "union", "of": [P.rr(-10, -10, 88, 610), P.rr(672, -10, 810, 610), P.rr(-10, -10, 810, 530)]}}],
                    "slab line"))
    for name, (dx, dy) in OFFS.items():
        cx, cy, R = RB.CENTRES[(name, "idle")]
        g = {"t": "move", "of": RB.GLYPHS[name](), "d": [dx, dy]}
        out.append({"name": f"{name} bubble", "box": [cx + dx - 37, cy + dy - 37, cx + dx + 37, cy + dy + 37],
                    "layers": RB.layers((cx + dx, cy + dy, R), bst, "blue", g)})
    g, bs = BD.load()
    board = BD.scene(g, bs)[0]
    black = {"t": "poly", "rings": [BD.diamond(*g["black"])]}
    board["layers"] = board["layers"][:3] + [{"shape": black, "fill": [0, 0, 0]}]    # shadow, frame, black window
    out.append(board)
    out.append(CD.socket_item((107.23, 564.22), sock))
    out.append(CD.socket_item((693.26, 564.22), sock))
    out.append(BU.item(CD.check_inst(SHEET, None, "blue", body=[692.9, 563.84, 23.23]), S))
    return out


def base():
    rgb, m = C.sheet(SHEET)
    b = np.empty_like(rgb)
    b[:] = [148, 150, 198]
    jj, ii = np.mgrid[0:600, 0:800]
    x, y = ii + 0.5, jj + 0.5
    inner = {"t": "inter", "of": [{"t": "circle", "c": ARC[:2], "r": ARC[3]}, P.rr(18.5, 250.0, 780.5, 660.0)]}
    marble = (sdf.sd(inner, x, y) < -0.5) & (y >= Y0)
    b[marble] = rgb[marble]
    return C.wipe(b, {"t": "union", "of": [frame_outer()] + BB.disc_shapes()})


if __name__ == "__main__":
    import deliver as D
    RB.GLYPHS = dict(RB.GLYPHS)
    solve_arch()
    rgb, m = C.sheet(SHEET)
    b = base()
    sc = scene()
    y0, h = Y0, 600 - Y0
    art = rgb[y0:600].copy()
    art[~m[y0:600]] = [70, 110, 90]
    det = [D.tile(rgb[y:y + hh, x:x + ww], b[y:y + hh, x:x + ww], sc, (x, y), 3) for x, y, ww, hh in
           ((0, 253, 260, 170), (300, 253, 200, 90), (540, 253, 260, 170))]
    D.deliver("route-bottom", [("idle", art, b[y0:600], sc, (0, y0))],
              "the slab (line x 4.8 / 795.5) with the top band's tan list floor running down to y 392 and the map window sunk "
              "below it; the scalloped arch: a lavender band 13.5 px wide under a big arc (centre (400, 842), r 585) and four "
              "round seats (circle fits r ~40.9, residual 0.3), the marble showing below the band, its rim light solved on the art; the four ringed mode bubbles (route_buttons, idle); Buy Land's diamond frame round a black "
              "window; the two lit discs, the corner slab lines, the empty socket and the blue check. The marble stays art. "
              "Full view 1x, details 3x",
              unsure="the band's width (13.5 px) is read where the slots' tops are; its left / right ends run into the slab", extra_tiles=det, z=1)
