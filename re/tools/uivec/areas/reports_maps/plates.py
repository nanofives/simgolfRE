"""Title plates of Buy Land / Routing map / Member Roster: parts.title_plate (the read arch profile) + lit.py paint solved
on each screen's own plate (navy line, inner shade under the top wall, drop shadow below)."""
import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C, lit, parts as P, route_tops as R, deliver as D

KIND = dict(ow=1.8, L=4.0, out_w=3.0)


def plate_style(sheet, S, box):
    rgb, m = C.sheet(sheet)
    st = lit.new_style(**KIND)
    lit.solve([(S, rgb, box, None)], st)
    return st


def deliver_plate(eid, sheet, G, crop, how_extra=""):
    S = P.title_plate(G)
    x, y, w, h = crop
    # The plate is the capsules' component (a sunken cream plate in a navy line): their fitted style (bl_caps.py, five cream
    # capsules) with this screen's own line colour. A first try solved its own bevel paint and painted a white rim inside
    # the line that the art does not have.
    st = json.load(open(os.path.join(C.OUT, "read", "caps_fit.json")))["style"]["cream"]
    rgb, m = C.sheet(sheet)
    st = dict(st, line=[float(v) for v in np.median(rgb[y:y + 12, x + w // 2 - 20:x + w // 2 + 20].reshape(-1, 3)[
        np.argsort(C.fit.lum(rgb[y:y + 12, x + w // 2 - 20:x + w // 2 + 20].reshape(-1, 3)))[:40]], axis=0)])
    art = rgb[y:y + h, x:x + w]
    bg = np.median(art[h // 3:2 * h // 3, :3].reshape(-1, 3), axis=0)     # the ground beside the plate (its top rows can be a lit rim)
    base = np.empty_like(art); base[:] = bg
    sc = [{"name": "title plate", "box": [x, y, x + w, y + h], "layers": P.plate(S, st)}]
    full_base = np.empty_like(rgb); full_base[:] = bg
    det = [D.tile(rgb[b:b + hh, a:a + ww], full_base[b:b + hh, a:a + ww], sc, (a, b), 8)
           for a, b, ww, hh in ((x, y, 70, h), (x + w // 2 - 35, y, 70, h), (x + w - 70, y, 70, h))]
    D.deliver(eid, [("idle", art, base, sc, (x, y))],
              "arched cream plate: one outline polygon, its top edge the read symmetric arch profile (line centre, averaged over "
              "both halves) running into small rounded top corners, straight walls and a flat foot with r 2 corners; 1.8 px navy "
              "line; inner shade under the top and left walls and a soft drop shadow down-right: the capsules' fitted plate style" + how_extra
              + ". Full view on top, the two ends and the middle at 8x below", extra_tiles=det, z=3)
    return st


if __name__ == "__main__":
    deliver_plate("buyland-title-plate", "buy_land.pcx", dict(R.PLATE_G, prof=[[0, 7.61], [25, 7.64], [50, 7.81], [75, 8.09], [100, 8.86], [125, 9.79], [150, 10.9], [170, 12.09], [185, 13.44], [195, 14.83], [200, 15.65], [204, 16.4], [207.5, 17.0], [209.6, 17.8]]), (182, 0, 438, 52))
