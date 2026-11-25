"""The report OK button (OkStates.pcx, 5 cells of 44 x 42: idle / hover / disabled, idle / hover row 2) and End of Year's
yellow check cut, as authored shapes.

Read (log/roadmap/P41/work/reports_slabs/_read/ok.png, the cell as text): on a flat lavender square sits a SOCKET (a thin
navy ring at r ~19.5 around a recessed lilac groove, dark at the bottom), the bubble's navy RIM (r ~14.5), the glass BODY
(blue / yellow / grey, lit from the upper right) and a thick navy CHECK: a short left arm with a flat top cut, a long right
arm with a flat top cut, a flat cut at the bottom; one 7-point polygon.

Paint is measured here: the socket is one disc whose bevel (depth from its edge x direction) is fitted on the art; the body
is recipes.bubble's body + rim with the body's bevel fitted on this art per state (BUBBLE_STOPS, as accept.py does on the
title menu), its specular and halo off (the fitted bevel carries the highlight).
"""
import json
import os
import sys

import numpy as np

import common as C
import slabkit as K

sys.path.insert(0, C.UIVEC)
import recipes  # noqa: E402

SHEET = "OkStates.pcx"
CELLS = {"idle": (1, 1), "hover": (46, 1), "disabled": (91, 1), "idle row 2": (1, 44), "hover row 2": (46, 44)}
W, H = 44, 42
# the check, read on the idle cell's grid (cell origin 1, 1 -> these are frame coordinates of that cell), clockwise
CHECK = [[13.6, 21.2], [18.2, 21.6], [21.2, 24.6], [24.6, 14.6], [29.6, 14.0], [24.5, 29.0], [18.4, 29.0]]
SOCKET_STOPS = [0, 0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 6]


def ring_fit(L, ox, oy, lo, hi, c0):
    """Circle through the darkest pixels of a ring between radii lo..hi around c0 (cell coordinates)."""
    yy, xx = np.mgrid[0:L.shape[0], 0:L.shape[1]]
    r = np.hypot(xx + 0.5 - c0[0], yy + 0.5 - c0[1])
    m = (r > lo) & (r < hi) & (L < 70)
    pts = np.c_[xx[m] + 0.5, yy[m] + 0.5]
    cx, cy, rr, res = C.fit.fit_circle(pts, trim=1.0)
    return cx + ox, cy + oy, rr, res


def build(sheet=SHEET, cells=CELLS, check=CHECK, check_origin=(1, 1), out_id="report-ok", how="", size=(44, 42),
          files=True, bare=False):
    """bare: the bubble alone (socket, rim, body, check), for a bubble drawn into a slab: no square, no stubs."""
    global W, H
    W, H = size
    art, alpha, raw = C.art(sheet)
    Lm = C.fit.lum(raw)
    scene_all, rows, info = [], [], {}
    for st, (ox, oy) in cells.items():
        sub = Lm[oy:oy + H, ox:ox + W]
        c0 = (22.0, 21.5)
        rcx, rcy, rr, rres = ring_fit(sub, ox, oy, 12.5, 16.5, c0)
        scx, scy, sr, sres = ring_fit(sub, ox, oy, 17.5, 21.5, (rcx - ox, rcy - oy))
        # the square: its own flat lavender (median of the cell's corners)
        corners = np.concatenate([raw[oy:oy + 3, ox:ox + 3].reshape(-1, 3), raw[oy + H - 3:oy + H, ox + W - 3:ox + W].reshape(-1, 3)])
        bg = [float(v) for v in np.median(corners, axis=0)]
        sq = {"t": "rrect", "c": [ox + W / 2, oy + H / 2], "half": [W / 2, H / 2], "r": 0.0, "rot": 0.0}
        sock = {"t": "circle", "c": [scx, scy], "r": sr + 0.9}
        rim = {"t": "circle", "c": [rcx, rcy], "r": rr + 0.9}
        body = {"t": "circle", "c": [rcx, rcy], "r": rr - 0.9}
        full = alpha > 0.99
        # socket bevel: pixels of the socket disc outside the rim
        def sel_sock(i, x, y, d):
            j, ii = (y - 0.5).astype(int), (x - 0.5).astype(int)
            return full[j, ii] & (d < -0.2) & (np.hypot(x - rcx, y - rcy) > rr + 1.4)
        sock_coef, srms = K.fit_bevel([sock], art, sel_sock, SOCKET_STOPS)
        # glyph polygon in this cell
        # the check rides with the bubble: placed about this cell's rim centre (it was read on the idle cell, rim
        # centre 23.21, 22.27)
        gl = [[p[0] - 23.21 + rcx, p[1] - 22.27 + rcy] for p in check]
        G = {"t": "poly", "rings": [gl]}
        Gd = {"t": "grow", "of": G, "d": 0.9}

        def sel_body(i, x, y, d):
            j, ii = (y - 0.5).astype(int), (x - 0.5).astype(int)
            return full[j, ii] & (d < -0.4) & (C.sdf.sd(Gd, x, y) > 0)
        body_coef, brms = K.fit_bevel([body], art, sel_body, recipes.BUBBLE_STOPS, L=body["r"], linear=True)
        # the specular spot: the centroid and spread of the brightest body pixels (> 92 % of the body's peak luminance)
        yy0, xx0 = np.mgrid[oy:oy + H, ox:ox + W]
        # (the spot sits in the upper-left quarter of the body on every state; the broad light on the right is the bevel's)
        inb = (np.hypot(xx0 + 0.5 - rcx, yy0 + 0.5 - rcy) < rr - 1.5) & (C.sdf.sd(Gd, xx0 + 0.5, yy0 + 0.5) > 0) & (
            xx0 + 0.5 < rcx) & (yy0 + 0.5 < rcy)
        Lb = np.where(inb, Lm[oy:oy + H, ox:ox + W], 0)
        hot = Lb > 0.92 * Lb.max()
        hx, hy = float((xx0 + 0.5)[hot].mean()), float((yy0 + 0.5)[hot].mean())
        hr = max(float(np.sqrt(hot.sum() / np.pi)) * 1.8, 1.5)
        sp = [(hx - rcx) / rr, (hy - rcy) / rr, hr / rr, 0.95, 1.6]
        # rim and glyph colours: medians of their pixels
        yy, xx = np.mgrid[oy:oy + H, ox:ox + W]
        rd = np.hypot(xx + 0.5 - rcx, yy + 0.5 - rcy)
        rim_col = [float(v) for v in np.median(raw[oy:oy + H, ox:ox + W][np.abs(rd - rr) < 0.5], axis=0)]
        gin = C.sdf.sd({"t": "grow", "of": G, "d": -0.7}, xx + 0.5, yy + 0.5) < 0
        glyph_col = [float(v) for v in np.median(raw[oy:oy + H, ox:ox + W][gin], axis=0)] if gin.any() else rim_col
        style = json.loads(json.dumps(recipes.STYLE["bubble"]))
        style["body"] = [(rcx - rcx) / rr, 0.0, (rr - 0.9) / rr]
        style["edge"] = [0.9, 0.0]
        style["halo"] = [0.0, 0.0, 0.0, 0.2, 0.0]
        style[st] = {"rim": rim_col, "shade": body_coef, "glyph": glyph_col, "sp": sp, "spec": [255, 255, 255]}
        bub = recipes.bubble(rcx, rcy, rr, state=st, glyph=G, style=style)
        if bare:
            # in a slab's foot the socket reads as a lilac groove (lit like the slab: pale at the top and sides, darker
            # at the bottom) closed by a crisp navy ring about 2 px wide on r ~20 (radial medians, _read/socket.txt)
            yy1, xx1 = np.mgrid[oy - 4:oy + H + 4, ox - 4:ox + W + 4]
            rr1 = np.hypot(xx1 + 0.5 - scx, yy1 + 0.5 - scy)
            th = np.degrees(np.arctan2(yy1 + 0.5 - scy, xx1 + 0.5 - scx))
            sub1 = raw[oy - 4:oy + H + 4, ox - 4:ox + W + 4]
            g = (rr1 > rr + 1.6) & (rr1 < sr - 1.6)
            med = lambda m: [float(v) for v in np.median(sub1[m], axis=0)]
            g_top, g_side, g_bot = med(g & (np.abs(th + 90) < 35)), med(g & ((np.abs(th) < 30) | (np.abs(th) > 150))),                 med(g & (np.abs(th - 90) < 35))
            ring_col = med((np.abs(rr1 - sr) < 0.6))
            groove = {"t": "circle", "c": [scx, scy], "r": sr - 0.95}
            ring = {"t": "outline", "of": {"t": "circle", "c": [scx, scy], "r": sr}, "w": 1.9}
            gp = {"linear": {"a": [scx, scy - sr], "b": [scx, scy + sr], "stops": [[0.0, g_top], [0.5, g_side], [1.0, g_bot]]}}
            layers = [{"shape": groove, "fill": gp}, {"shape": ring, "fill": ring_col}] + bub["layers"]
        else:
            layers = [{"shape": sq, "fill": bg}, {"shape": sock, "fill": K.bevel(1.0, SOCKET_STOPS, sock_coef)}] + bub["layers"]
        # wire stubs entering the cell at its left / right edge (a dark run in the edge column, outside the socket)
        stubs = []
        for col, xe in ((ox + 1, ox), (ox + W - 2, ox + W)):
            v = Lm[oy:oy + H, col]
            for j in range(H):
                y = oy + j + 0.5
                if v[j] < 75 and np.hypot(col + 0.5 - scx, y - scy) > sr + 1.5 and (j == 0 or v[j - 1] >= 75):
                    k = j
                    while k + 1 < H and v[k + 1] < 75:
                        k += 1
                    if k - j <= 2:
                        c_, w_ = __import__("parts").line_profile(raw, "v", col, oy + max(j - 2, 0), oy + min(k + 3, H), "lo")
                        if c_ is not None:
                            xin = scx - (sr - 0.5) if xe == ox else scx + (sr - 0.5)
                            stubs.append({"t": "stroke", "pts": [[xe - 0.5 if xe == ox else xe + 0.5, c_], [xin, c_]],
                                          "w": max(w_, 1.2), "cap": "butt"})
        for S in ([] if bare else stubs):
            layers.insert(2, {"shape": S, "fill": rim_col})
        it = {"box": [ox - 1, oy - 1, ox + W + 1, oy + H + 1], "layers": layers, "part": st}
        scene_all.append(it)
        rows.append((st, (ox, oy, W, H)))
        info[st] = {"rim": [round(rcx, 2), round(rcy, 2), round(rr, 2), round(rres, 3)],
                    "socket": [round(scx, 2), round(scy, 2), round(sr, 2), round(sres, 3)],
                    "rms": {"socket": round(srms, 2), "body": round(brms, 2)}}
    def mine(box, s):
        img = np.empty(((box[3] - box[1]) * s, (box[2] - box[0]) * s, 3))
        img[:] = C.FLAT
        C.sdf.render(scene_all, img, s, origin=(box[0], box[1]))
        return img
    if not files:
        return scene_all
    K.compare_png(os.path.join(C.OUT, out_id + ".png"), art, mine, rows, scale=7)
    mads = []
    for st, (x, y, w, h) in rows:
        low = K.down(mine((x, y, x + w, y + h), 4), 4)
        mads.append(round(float(np.abs(low - art[y:y + h, x:x + w])[2:-2, 2:-2].mean()), 2))
    json.dump({"id": out_id, "sheet": sheet, "cells": cells, "check": check, "scene": scene_all, "measured": info},
              open(os.path.join(C.OUT, out_id + ".json"), "w"))
    print(out_id, info, mads)
    return mads, info


if __name__ == "__main__":
    build()
    # End of Year's own yellow check cut (the sheet's cell at 551, 486; the screen draws the OkStates button)
    build(sheet="ENDoYEAR.pcx", cells={"idle": (551, 486)}, out_id="endofyear-ok-hover", size=(44, 44))
