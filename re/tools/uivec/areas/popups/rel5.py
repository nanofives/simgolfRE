"""P41 popups round 5 (2026-11-25): relight the four info buttons approved in round 1 (chart, checklist, notice, pair) on the
round-2 siblings' recipe (ib2.py: one navy rim colour for every state, the lit disc body of disc2.py with the style fitted on
all ten buttons, the same hover shadow), keeping their approved glyph shapes exactly.

The glyph layers are read back from the round-1 scenes (log/roadmap/P41/work/popups/info-button-<key>.json), so every shape,
position and tuned number is the approved one; only their clip changes to the new body circle (same radius: rim 1.9 px in
both rounds). The glyph light that the art shows and the siblings' round-2 glyphs also carry is added as layers over them
(LIGHT below, read on the idle cells' RGB, printed by read5.py).

py -3.12 rel5.py [key ...]   -> log/roadmap/P41/work5/popups/info-button-<key>.png (art | round 1 | relit), .json
py -3.12 rel5.py --family    -> log/roadmap/P41/work5/popups/_family.png
"""
import copy
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c      # noqa: E402
import infobuttons as ib  # noqa: E402
import ib2              # noqa: E402  (imports w2, which repoints c.OUT; w5.apply() below sets it back to work5)
import disc2            # noqa: E402
import w5               # noqa: E402

w5.apply()
KEYS5 = ("chart", "checklist", "notice", "pair")
SIBS = ("flag", "globe", "money", "pin", "tiles", "trophy")
R1 = os.path.join(c.ROOT, "log", "roadmap", "P41", "work", "popups")
R2 = os.path.join(c.ROOT, "log", "roadmap", "P41", "work2", "popups")


def mv(S, dx, dy):
    return {"t": "move", "of": S, "d": [dx, dy]}


def round1(k):
    with open(os.path.join(R1, f"info-button-{k}.json")) as f:
        return json.load(f)


def glyph_of(layers):
    """The glyph layers of a round-1 item: everything after the body (the first 'move' layer on)."""
    i = next(n for n, L in enumerate(layers) if L["shape"]["t"] == "move")
    return copy.deepcopy(layers[i:])


def P(pt, o):
    """A cell point (x, y) in sheet coordinates for a glyph placed at offset o."""
    return [pt[0] + o[0], pt[1] + o[1]]


def light_notice(G, o, body):
    """The speech bubble is a raised navy shape lit from the upper left, like the flag's green (round 2):
    - a lilac rim along its top and upper-left edge, inside the round-1 outline (row 7: a0a8b8 9898b8 9090a0 a0a0a0 from
      x 12 to 19; row 8 x 9-11: a0a8b8 9898b8 888098; row 11 x 5-6: b8b0a8 9090a0): the bubble minus itself moved 0.8 px
      right and 0.9 px down (the art's light reaches down the left side to row 13), soft 0.3, (156, 156, 184) on the left fading to (132, 136, 164) on the right, above y 16.5;
    - under it a band of lighter navy about 2 px deep (row 8 x 12-14: 7878a0 606880 404868; row 9 x 10-12: 7878a0 586078
      485070; row 12 x 6-7: 686888 384060; the right half of the top rows 8-9: 707898 7878a0 / 586078 606880): the bubble minus
      itself moved 1.3 px right and 2.0 px down, (112, 116, 156) at the upper left to (64, 72, 108) at the lower right, soft;
      the lower half of the bubble (rows 17-21) is flat 283058 in the art, so the band is cut at y 16.5;
    - the tail's left edge is a lighter navy 1 px line (rows 24-27 x 16-17: 707880, 888098, 586078 against 303858 inside): the
      tail minus itself moved 0.75 px right, (108, 112, 140) at 0.8 (the lilac rim drawn over it was too bright);
    - the '!' is cut into the bubble, so its wall on the upper-left side is in shadow (bar: x 15 887868 / 988868 / 686060
      against c0a870 / c8b078 in the middle and a89870 / b0a880 on the right, rows 10-15; dot: x 15 585858 against a89870):
      a 0.55 px band along the left and top of the bar and the dot, (136, 120, 100)."""
    bub, tail, bar, dot = (L["shape"] for L in G[:4])
    upper = {"t": "half", "p": P((0.0, 16.5), o), "n": [0.0, 1.0]}
    rim = {"t": "inter", "of": [{"t": "sub", "a": bub, "b": mv(bub, 0.8, 0.9)}, upper]}
    band = {"t": "inter", "of": [{"t": "sub", "a": bub, "b": mv(bub, 1.3, 2.0)}, upper]}
    tedge = {"t": "sub", "a": tail, "b": mv(tail, 0.75, 0.3)}
    out = [G[0], G[1],
           {"shape": band, "fill": {"linear": {"a": P((6.0, 9.0), o), "b": P((24.0, 16.0), o),
                                               "stops": [[0.0, [112, 116, 156]], [1.0, [64, 72, 108]]]}},
            "soft": 0.5, "clip": {"t": "inter", "of": [bub, body]}},
           {"shape": rim, "fill": {"linear": {"a": P((6.0, 10.0), o), "b": P((26.0, 9.0), o),
                                              "stops": [[0.0, [156, 156, 184]], [1.0, [132, 136, 164]]]}},
            "soft": 0.3, "clip": {"t": "inter", "of": [bub, body]}},
           {"shape": tedge, "fill": [108, 112, 140], "alpha": 0.8, "soft": 0.3, "clip": {"t": "inter", "of": [tail, body]}},
           G[2], G[3]]
    for hole in (bar, dot):
        sh = {"t": "sub", "a": hole, "b": mv(hole, 0.55, 0.55)}
        out.append({"shape": sh, "fill": [136, 120, 100], "alpha": 0.85, "clip": {"t": "inter", "of": [hole, body]}})
    return out + G[4:]


def light_chart(G, o, body):
    """The orange line is lit on its upper-left side, like the check of the checklist (approved round 1) and the lit tops of
    the round-2 glyphs: its core is f86000 (248, 96, 0) (rows 11-21 along the middle of every leg) and its upper-left edge a
    pale orange (row 10 x 18-19: f8b888 f8a870; row 16 x 10-11: f0c080 f8b888; row 15 x 16: f8a870), paler than any mix of the
    core with the tan body (f09440). So: the line in the core colour, then the line minus itself moved 0.55 px right and
    0.55 px down in (248, 170, 120) at 0.8, soft (a wider band read as a second line beside it at 8x). The navy axes are flat in the art (their light pixels are the edge mix), as the round-2
    money sign is, so they stay as approved."""
    ax_v, ax_h, line = G[0], G[1], copy.deepcopy(G[2])
    line["fill"] = [248, 100, 10]
    edge = {"t": "sub", "a": line["shape"], "b": mv(line["shape"], 0.4, 0.55)}
    return [ax_v, ax_h, line,
            {"shape": edge, "fill": [248, 170, 120], "alpha": 0.8, "soft": 0.3, "clip": {"t": "inter", "of": [line["shape"], body]}}] + G[3:]


def light_checklist(G, o, body):
    """The box frame is a raised navy line lit from the upper left, like the flag's green (round 2): on every side its upper or
    left edge is lilac and the rest navy 283058 (top side, row 10 over row 11: 9898b8 9090a0 888098 687090 606880 .. 707898
    7878a0; left side, column 9 beside column 10: 9898b8 7878a0 586078 404868 down to 485070; bottom side, row 24 over row 25:
    606880; right side, column 23 beside column 24: 485070), brightest at the top-left corner. So: the frame minus itself
    moved 0.6 px right and down, (156, 156, 188) at the top-left corner to (88, 96, 132) at the bottom right. The check's
    own light line (round 1) stays."""
    frame = G[0]["shape"]
    band = {"t": "sub", "a": frame, "b": mv(frame, 0.6, 0.6)}
    return [G[0],
            {"shape": band, "fill": {"linear": {"a": P((9.5, 10.5), o), "b": P((24.5, 25.5), o),
                                                "stops": [[0.0, [156, 156, 188]], [0.5, [100, 106, 140]], [1.0, [88, 96, 132]]]}},
             "clip": {"t": "inter", "of": [frame, body]}}] + G[1:]


def light_pair(G, o, body):
    """The golfers are 1 px navy lines; their few lighter pixels (head tops a0a8b8 9090a0) are the line's edge mixed with the
    lit body, the same as the round-2 money sign's, which was approved flat. No glyph light is added."""
    return G


LIGHT = {"notice": light_notice, "chart": light_chart, "checklist": light_checklist, "pair": light_pair}


def relit_scene(k, col, st, r1):
    r = ib.KEYS.index(k)
    disc0 = ib.measure_disc(r, 0)
    disc = ib.measure_disc(r, col)
    o = (disc[0] - disc0[0], 50 * r + disc[1] - disc0[1])
    it, body = ib2.base(r, col, disc, st)
    G = glyph_of(r1["states"][st]["scene"][0]["layers"])
    for L in G:
        L["clip"] = body
    it["layers"] += LIGHT[k](G, o, body)
    it["name"] = f"info-button-{k} {st}"
    return [it]


def panels(k, z=8):
    """Rows (one per state) of art | round-1 drawing | relit drawing, all at z."""
    r = ib.KEYS.index(k)
    r1 = round1(k)
    rows, scenes, stats = [], {}, []
    for col, st in enumerate(ib.STATES):
        box = ib.cell(r, col)
        s1 = r1["states"][st]["scene"]
        s5 = relit_scene(k, col, st, r1)
        a = np.kron(c.art(ib.SHEET, box), np.ones((z, z, 1)))
        p1 = c.render_box(s1, box, z, c.FLAT)
        p5 = c.render_box(s5, box, z, c.FLAT)
        sep = np.full((a.shape[0], 8, 3), 255.0)
        rows.append(np.concatenate([a, sep, p1, sep, p5], axis=1))
        scenes[st] = s5
        stats.append((st, c.score(s1, ib.SHEET, box)[0], c.score(s5, ib.SHEET, box)[0]))
    W = rows[0].shape[1]
    img = np.concatenate([np.pad(t, ((0, 6), (0, W - t.shape[1]), (0, 0)), constant_values=255) for t in rows])
    w5.check()
    p = os.path.join(c.OUT, f"info-button-{k}.png")
    Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(p)
    c.save_scene(f"info-button-{k}", {"id": f"info-button-{k}", "sheet": "Interface/" + ib.SHEET,
                                      "states": {st: {"box": list(ib.cell(r, i)), "scene": scenes[st]}
                                                 for i, st in enumerate(ib.STATES)}})
    return p, stats


def family(z=4):
    """All ten info buttons at z on the art's own ground: per row the art (idle, hover, selected), then the drawing; the four
    relit in round 5 come from work5, the six siblings from their approved round-2 scenes in work2."""
    tiles = []
    labels = []
    for k in ib.KEYS:
        r = ib.KEYS.index(k)
        src = os.path.join(c.OUT if k in KEYS5 else R2, f"info-button-{k}.json")
        with open(src) as f:
            d = json.load(f)
        row = []
        for col, st in enumerate(ib.STATES):
            row.append(np.kron(c.art(ib.SHEET, ib.cell(r, col)), np.ones((z, z, 1))))
        gap = np.full((row[0].shape[0], 24, 3), 255.0)
        for col, st in enumerate(ib.STATES):
            row.append(c.render_box(d["states"][st]["scene"], ib.cell(r, col), z, c.FLAT))
        parts = []
        for i, t in enumerate(row):
            parts.append(t)
            parts.append(gap if i == 2 else np.full((t.shape[0], 4, 3), 255.0))
        tiles.append(np.concatenate(parts, axis=1))
        labels.append(("round 5 relit" if k in KEYS5 else "round 2 sibling") + f": {k}")
    lab_w = 150
    W = max(t.shape[1] for t in tiles) + lab_w
    out = []
    for t in tiles:
        pad = np.full((t.shape[0] + 6, W, 3), 255.0)
        pad[:t.shape[0], lab_w:lab_w + t.shape[1]] = t
        out.append(pad)
    img = Image.fromarray(np.clip(np.concatenate(out) + 0.5, 0, 255).astype(np.uint8))
    dr = ImageDraw.Draw(img)
    h = tiles[0].shape[0] + 6
    for i, s in enumerate(labels):
        dr.text((6, i * h + h // 2 - 12), s.split(": ")[1], fill=(0, 0, 0))
        dr.text((6, i * h + h // 2 + 2), s.split(": ")[0], fill=(160, 0, 0) if "relit" in s else (90, 90, 90))
    w5.check()
    p = os.path.join(c.OUT, "_family.png")
    img.save(p)
    return p


if __name__ == "__main__":
    args = sys.argv[1:]
    if args == ["--family"]:
        print(family())
        sys.exit(0)
    keys = args or list(KEYS5)
    for i, k in enumerate(keys):
        p, st = panels(k)
        print(f"[{i + 1}/{len(keys)}] info-button-{k} " + " ".join(f"{s} mad {a:.2f}->{b:.2f}" for s, a, b in st) + f"  {p}")
