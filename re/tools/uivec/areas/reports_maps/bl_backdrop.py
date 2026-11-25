"""Buy Land screen, composed from this area's components (read 2026-11-24 from buy_land.pcx):
lavender ground; the title plate (plates.py); the nine capsules with their blue number balls (bl_caps, bl_balls); two lilac
pills on the window's top line (line centre (482.8, 259.8, 651.4, 277.8) and (662.8, 259.8, 777.6, 277.8), the Routing map's
pill style); the map window: a lavender slab in a navy line (x 7.8..791.8, y 269.7..537.8, r 8) with the window sunk into
it (x 18.5..780.5 from the line down past the screen's foot; its floor is the marbled ball, kept art); two big lavender
discs over the window's lower corners (circle fit on the right one's edge: centre (794.46, 718.7), r 243.88, residual
0.23 px; the left one its mirror about x 399.5), lit along their rims; the slab's lower line shows only at the two corners
(x < 88 and x > 672, up to the two sockets: between them the board and the marble lie over it); the empty socket at the lower left and the blue
check in its socket at the lower right (check_deliver.py); the lot board on top (bl_board.py).
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bl_balls as B, bl_board as BD, bl_caps as K, bubbles as BU, check as CK, check_deliver as CD   # noqa: E402,E401
import common as C, lit, parts as P, plates, route_tops as R                                          # noqa: E402,E401

DISC_R = [794.46, 718.7, 243.88]
DISC_L = [799.0 - 794.46, 718.7, 243.88]
SLAB = (7.8, 269.7, 791.8, 537.8, 8.0)
WINDOW = (18.5, 269.7, 780.5, 660.0)
PILLS = [(482.8, 259.8, 651.4, 277.8), (662.8, 259.8, 777.6, 277.8)]
DISC_FILE = os.path.join(C.OUT, "read", "bl_disc_style.json")


def disc_shapes():
    slab = P.rr(*SLAB)
    return [{"t": "inter", "of": [{"t": "circle", "c": d[:2], "r": d[2]}, {"t": "half", "p": [0, 600], "n": [0, 1]}]}
            for d in (DISC_L, DISC_R)]


def solve_discs():
    rgb, m = C.sheet("buy_land.pcx")
    st = lit.new_style(ow=0.0, L=6.0, out_w=0.0)
    jj, ii = np.mgrid[0:600, 0:800]
    # the board, the sockets and the capsule area are not the discs'
    board = np.abs(ii + 0.5 - 399.6) / 365.3 + np.abs(jj + 0.5 - 438.8) / 182.75 < 1.04
    socks = (np.hypot(ii + 0.5 - 107.27, jj + 0.5 - 564.26) < 33) | (np.hypot(ii + 0.5 - 693.28, jj + 0.5 - 564.31) < 33)
    keep = ~board & ~socks & (jj > 300)
    keep &= ~((ii < 12) | (ii > 788) | ((jj > 534) & (jj < 542)))      # the slab line
    lit.solve([(S, rgb, (0, 0, 800, 600), keep) for S in disc_shapes()], st, dir_in=1.0)
    json.dump(st, open(DISC_FILE, "w"))
    return st


def scene():
    rs = R.load_styles()
    cs = json.load(open(os.path.join(C.OUT, "read", "caps_fit.json")))
    S = BU.load_style()
    g, bst = BD.load()
    sock = json.load(open(CD.SOCK_FILE))
    dst = json.load(open(DISC_FILE))
    full = lambda L: {"name": "", "box": [0, 0, 800, 600], "layers": L}   # noqa: E731
    out = []
    window = P.rr(*WINDOW)
    slab = P.rr(*SLAB)
    not_window = {"t": "sub", "a": P.rr(-10, -10, 810, 610), "b": window}
    # the slab: its fill outside the window, the window's lip, the discs, then the slab's line where it shows
    sl = lit.layers(slab, rs["slab"])
    fill = sl[-1]
    fill["clip"] = not_window
    out.append(full([fill]))
    lip = lit.layers(window, rs["list_cream"])[0]
    lip["clip"] = {"t": "half", "p": [0, WINDOW[1] + 0.9], "n": [0, -1]}
    out.append(full([lip]))
    for D_ in disc_shapes():
        out.append(full(lit.layers(D_, dst)))
    line = {"shape": {"t": "outline", "of": slab, "w": rs["slab"]["ow"]}, "fill": rs["slab"]["line"],
            "clip": {"t": "union", "of": [P.rr(-10, -10, 88, 610), P.rr(672, -10, 810, 610), P.rr(-10, -10, 810, 400)]}}
    out.append(full([line]))
    for p in PILLS:
        out.append(full(lit.layers(P.rr(*p, (p[3] - p[1]) / 2), rs["pill"])))
    G = dict(R.PLATE_G, prof=[[0, 7.61], [25, 7.64], [50, 7.81], [75, 8.09], [100, 8.86], [125, 9.79], [150, 10.9], [170, 12.09],
                              [185, 13.44], [195, 14.83], [200, 15.65], [204, 16.4], [207.5, 17.0], [209.6, 17.8]])
    out.append(full(P.plate(P.title_plate(G), dict(cs["style"]["cream"], line=[33, 41, 82]))))
    out += K.items(cs["geom"], cs["style"])
    out += [BU.item(i, S) for i in B.instances() if i["state"] == "backdrop"]
    out.append(CD.socket_item((107.27, 564.26), sock))
    out.append(CD.socket_item((693.28, 564.31), sock))
    out += BD.scene(g, bst)
    # its rim runs into the socket ring, so the body is the one measured with a dark limit of 50 (bub.body_circle, this session)
    inst = CD.check_inst("buy_land.pcx", None, "blue", body=[692.9, 563.84, 23.23])
    out.append(BU.item(inst, S))
    return out


def base():
    """Lavender, and the kept marbled ball inside the window where nothing of the chrome lies over it."""
    rgb, m = C.sheet("buy_land.pcx")
    b = np.empty_like(rgb)
    b[:] = [148, 148, 198]
    import sdf
    jj, ii = np.mgrid[0:600, 0:800]
    x, y = ii + 0.5, jj + 0.5
    inwin = sdf.sd(P.rr(*WINDOW), x, y) < -0.5
    b[inwin] = rgb[inwin]
    # under the board and the discs the marble is wiped (their shapes are drawn there)
    return C.wipe(b, {"t": "union", "of": [{"t": "poly", "rings": [BD.diamond(*BD.load()[0]["outer"])]}] + disc_shapes()})


if __name__ == "__main__":
    import deliver as D
    if not os.path.exists(DISC_FILE):
        solve_discs()
    rgb, m = C.sheet("buy_land.pcx")
    sc = scene()
    b = base()
    det = [D.tile(rgb[y:y + h, x:x + w], b[y:y + h, x:x + w], sc, (x, y), 3) for x, y, w, h in
           ((0, 0, 260, 130), (0, 250, 160, 120), (0, 450, 260, 150), (540, 450, 260, 150), (440, 240, 360, 60))]
    D.deliver("buyland-backdrop", [("idle", rgb, b, sc, (0, 0))],
              "the screen composed from this area's components: lavender ground, title plate, nine capsules with blue number "
              "balls, two lilac pills on the window line, the lavender slab in its navy line with the map window sunk into it "
              "(the marbled ball inside stays art), two big lit lavender discs over the window's lower corners (circle fit "
              "residual 0.23 px; the left one mirrored), the empty socket, the blue check in its socket, the lot board. Full view "
              "1x on top, details at 3x below",
              unsure="the slab's lower line is drawn only at the two corners (x < 88, x > 672), where the art shows it; the "
                     "marble is wiped under the board and the discs from its own surroundings", extra_tiles=det, z=1)
