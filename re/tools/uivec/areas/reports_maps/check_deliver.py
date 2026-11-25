"""Buy Land / Routing map checks: buyland-exit (hover cell) and route-ok-hover; the blue backdrop checks and the empty
sockets are drawn by the backdrops with the same items (socket_item, check_item)."""
import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bub, bubbles as BU, check as K, common as C, deliver as D, lit, sdf

RING_R, RING_OFF = 30.17, (0.3, 1.2)     # socket ring (fit on 5 instances, residual 0.4 px): radius, centre - bubble centre
CHK = None     # the read outline as written (check.CHECK); the fitted one in read/check_fit.json put a jog in the foot
SOCK_FILE = os.path.join(C.OUT, "read", "socket_style.json")
# (sheet, socket centre, has a bubble)
SOCKETS = [("buy_land_buttons.pcx", (225.30, 158.28), True), ("buy_land.pcx", (693.28, 564.31), True),
           ("buy_land.pcx", (107.27, 564.26), False), ("route screens_bottom.pcx", (693.26, 564.22), True),
           ("route screens_bottom.pcx", (107.23, 564.22), False)]


def socket_shape(c):
    return {"t": "circle", "c": list(c), "r": RING_R}


def solve_socket():
    st = lit.new_style(ow=1.4, L=RING_R, out_w=0.0)   # a thin ring, no lip (a 1.8 px ring with a lip read heavy)
    insts = []
    for sh, c, full in SOCKETS:
        rgb, m = C.sheet(sh)
        box = (int(c[0] - 36), int(c[1] - 36), int(c[0] + 36), min(int(c[1] + 36), rgb.shape[0]))
        jj, ii = np.mgrid[box[1]:box[3], box[0]:box[2]]
        keep = m[box[1]:box[3], box[0]:box[2]].copy()
        if full:
            keep &= np.hypot(ii + .5 - c[0] + RING_OFF[0], jj + .5 - c[1] + RING_OFF[1]) > 26.5
        insts.append((socket_shape(c), rgb, box, keep))
    lit.solve(insts, st, dir_in=1.0)
    json.dump(st, open(SOCK_FILE, "w"))
    return st


def socket_item(c, st):
    return {"name": "socket", "box": [c[0] - 36, c[1] - 36, c[0] + 36, c[1] + 36], "layers": lit.layers(socket_shape(c), st)}


def check_inst(sheet, box, colour, body=None):
    rgb, m = C.sheet(sheet)
    if body is None:
        rim0 = bub.rim_circle(rgb, m, box)[:3]
        body = bub.body_circle(rgb, rim0)
    rim = [body[0] + 0.14, body[1] + 0.31, body[2] + 1.65]
    gl = K.check_shape(body[0] - K.CHECK_BODY[0], body[1] - K.CHECK_BODY[1], CHK)
    return {"sheet": sheet, "rim": rim, "body": body, "glyph": gl, "colour": colour, "box": None}


def check_item(inst, S):
    return BU.item(inst, S)


def deliver_cell(eid, sheet, cell, colour, sock_c, how):
    S = BU.load_style()
    st = json.load(open(SOCK_FILE))
    x, y, w, h = cell
    inst = check_inst(sheet, (x + 4, y + 4, w - 8, h - 8), colour)
    rgb, m = C.sheet(sheet)
    art = rgb[y:y + h, x:x + w].copy()
    FL = np.array([70., 110, 90])
    art[~m[y:y + h, x:x + w]] = FL
    sc = ([socket_item(sock_c, st)] if sock_c else []) + [check_item(inst, S)]
    wipe_s = socket_shape(sock_c) if sock_c else {"t": "circle", "c": inst["rim"][:2], "r": inst["rim"][2] + 1}
    base = C.wipe(art, {"t": "grow", "of": wipe_s, "d": 2.0}, origin=(x, y))
    D.deliver(eid, [("hover", art, base, sc, (x, y))], how, z=8)


if __name__ == "__main__":
    solve_socket()
    deliver_cell("buyland-exit", "buy_land_buttons.pcx", (0xc2, 0x7f, 0x40, 0x40), "yellow", SOCKETS[0][1],
                 "C1 yellow bubble (rim / body measured, paint from the lot balls' yellow) with the check mark authored as one "
                 "outline (long stroke leaning right with a flat top, short stroke with a flat top falling at 45 degrees, V notch, "
                 "one flat foot; read from coverage runs, iou 0.86), seated in the round socket: a navy ring "
                 "(r 30.17, fit residual 0.4 px over five sockets) round a sunk lavender well whose shade is solved on all five")
