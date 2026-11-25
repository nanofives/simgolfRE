"""P41 fix round: the character editor's composed parts (CustGolfBckgrnd.pcx / CustGlfBckMale.pcx).

Mariano's notes:
- editor-name-card (redo) "buttons rendered below grid", editor-backdrop (redo) the same: the card and its stripes were drawn
  after the load and save bubbles, and the page after the button column, so the panels covered the buttons. Everything is
  now drawn in the game's order: list, card, pills, page, then every bubble on top.
- editor-name-card re-read from scratch (dumps, 2026-11-24): ONE card, not a cream card plus three boxed stripes: a 2 px navy
  line round an arched top (a circle fitted to the arch's top edge from x 60 to 240), straight sides (x 41-42 and 265-266)
  and a 3 px bottom (rows 105-107); cream above a 1 px navy line at row 44 and tan below it, the tan split by two 1 px
  hairlines (rows 64 and 84, a grey-brown); the left line casts a 6 px shadow into the tan (colours read along row 90), the
  card casts a shadow to its right (x 267-271) and below (rows 108-111). The load and save bubbles sit on the card's top
  corners, over the line, with their own cast shadows on the cream.
- editor-page "missing shadow below the buttons": the five bubbles that overlap the page's left edge cast a soft shadow to the
  lower right onto the cream. Fitted on the art (offset, softness, strength) and applied the same to every bubble of the
  screen, so all of them are lit alike.
- editor-list "lines on the right too thick": the cream column is cream rows under 1 px grey hairlines (every 16 rows), a 1 px
  grey line down its left side and a soft shade along its top; the first round drew boxes.py's inset rows on a grey field,
  which left 2-3 px grey bands. Redrawn as one cream rectangle with 1 px hairlines read row by row.
"""
import copy

import numpy as np

import common as c
import editor_screen as E
import fix2 as F
import fix2_bubbles as FB
import fix2_glyphs as FG
import fix2_pills as FP
import parts
from common import fit, sdf

BG_F = "Interface/CustGolfBckgrnd.pcx"
BG_M = "Interface/CustGlfBckMale.pcx"
CB = "Interface/CGButtons.pcx"


def col(a, x, y):
    return [round(float(v), 1) for v in a[y, x]]


# ------------------------------------------------------------------------------------------------------------ name card
def name_card(art):
    L = fit.lum(art)
    top = []
    for x in range(60, 241, 2):
        k = np.nonzero(L[2:40, x] < 110)[0]
        if len(k):
            top.append((x + 0.5, 2 + k[0]))
    cxa, cya, ra, res = fit.fit_circle(np.array(top), trim=0.85)
    navy = col(art, 42, 90)
    x0, x1 = 40.7, 266.9                      # outer edges of the side lines (x 40 is the left line's soft pixel)
    yb = 107.9
    outer = {"t": "inter", "of": [F.circ(cxa, cya, ra), F.box(x0, 0.0, x1, yb)]}
    inner = {"t": "inter", "of": [F.circ(cxa, cya, ra - 2.0), F.box(42.9, 0.0, 264.7, 105.0)]}
    cream = col(art, 150, 30)
    tan = col(art, 150, 90)
    hair = col(art, 150, 64)
    sh_rgb = [70, 70, 100]
    layers = [
        {"shape": F.mv(F.box(x1 - 6.0, 30.0, x1, yb), 2.2, 1.8), "fill": sh_rgb, "soft": 2.2, "alpha": 0.8},
        {"shape": F.mv(F.box(x0, yb - 6.0, x1, yb), 1.5, 2.2), "fill": sh_rgb, "soft": 2.2, "alpha": 0.8},
        {"shape": outer, "fill": navy},
        {"shape": inner, "fill": cream},
        {"shape": {"t": "inter", "of": [inner, F.box(0.0, 43.0, 800.0, 44.0)]}, "fill": col(art, 150, 43)},
        {"shape": F.box(42.9, 45.0, 264.7, 105.0), "fill": tan},
        {"shape": F.box(42.9, 45.0, 48.6, 105.0), "fill": {"linear": {"a": [42.9, 0.0], "b": [48.6, 0.0], "stops": [
            [0.0, col(art, 43, 90)], [0.2, col(art, 44, 90)], [0.4, col(art, 45, 90)], [0.6, col(art, 46, 90)],
            [0.8, col(art, 47, 90)], [1.0, tan]]}}},
        {"shape": F.box(42.9, 44.0, 264.7, 45.0), "fill": col(art, 150, 44)},
        {"shape": F.box(42.9, 64.0, 264.7, 65.0), "fill": hair},
        {"shape": F.box(42.9, 84.0, 264.7, 85.0), "fill": hair},
    ]
    return {"name": "name card", "box": [36, 0, 274, 114], "layers": layers}, [round(cxa, 2), round(cya, 2), round(ra, 2)]


# ----------------------------------------------------------------------------------------------------------------- page
def page(art):
    """The description page (first round's reading, kept: straight top and bottom rows, a right edge that bulges as a circle
    fitted to it, a 2 px navy line), now with its cast shadow fitted to the right and below."""
    L = fit.lum(art)
    pts = []
    for yy in range(24, 238, 2):
        k = np.nonzero(L[yy, 600:760] < 90)[0]
        if len(k):
            pts.append((600 + k[0] + 0.5, yy + 0.5))
    ecx, ecy, er, _ = fit.fit_circle(np.array(pts), trim=0.8)
    rows_t = [yy for yy in range(5, 40) if L[yy, 600] < 90]
    rows_b = [yy for yy in range(220, 260) if L[yy, 600] < 90]
    t0, b1 = rows_t[0], rows_b[-1] + 1
    pg = {"t": "inter", "of": [F.box(440.0, t0, 800.0, b1), F.U(F.circ(ecx, ecy, er), F.box(440.0, 0.0, ecx, 300.0))]}
    pc = [round(float(v), 1) for v in np.median(art[60:200, 500:680].reshape(-1, 3), axis=0)]
    item = {"name": "page", "box": [440, t0 - 2, 745, b1 + 6], "layers": [
        {"shape": pg, "fill": E.NAVY}, {"shape": {"t": "grow", "of": pg, "d": -2.0}, "fill": pc}]}
    rect = (446, t0 - 2, 300, b1 - t0 + 8)
    wipe = [{"box": item["box"], "layers": [{"shape": {"t": "grow", "of": F.mv(pg, 2.0, 2.0), "d": 3.0}, "fill": [0, 0, 0]}]}]
    bg = F.wiped(art, wipe, rect, grow=0.5)
    sh = F.fit_shadow(art, pg, bg, (rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]), rgb=(50, 50, 85),
                      dxs=(1.0, 2.0, 3.0), dys=(1.0, 2.0, 3.0))
    if sh:
        item["layers"].insert(0, sh)
    return item, {"right_edge": [round(ecx, 2), round(ecy, 2), round(er, 2)], "rows": [t0, b1], "shadow": sh and sh["alpha"]}


# ----------------------------------------------------------------------------------------------------------------- list
def editor_list(art):
    L = fit.lum(art)
    hl_l = [yy for yy in range(270, 586) if L[yy, 200] < 60]
    hl_r = [yy for yy in range(270, 587) if L[yy, 600] < 200 and L[yy - 1, 600] > 200 and L[yy + 1, 600] > 200]
    fillL = col(art, 200, 400)
    cream = col(art, 600, 290)
    grey = col(art, 600, 284)
    lay = [{"shape": F.box(40.0, 268.0, 400.0, 587.0), "fill": [20, 30, 46]},
           {"shape": F.box(41.0, 269.0, 399.0, 587.0), "fill": [68, 66, 96]},
           {"shape": F.box(45.0, 273.0, 420.0, 600.0, 2.0), "fill": fillL, "soft": 2.4,
            "clip": F.box(41.0, 269.0, 399.0, 587.0)}]
    lay += [{"shape": F.box(41.0, yy, 399.0, yy + 1.0), "fill": [20, 33, 49]} for yy in hl_l]
    lay.append({"shape": F.box(399.0, 268.0, 400.0, 587.0), "fill": [20, 33, 49]})
    # the cream column: x 400 grey line, cream x 401-761, the soft shade along its top (rows 269-273, read at x 600)
    lay.append({"shape": F.box(400.0, 268.0, 762.0, 587.0), "fill": cream})
    lay.append({"shape": F.box(401.0, 269.0, 762.0, 274.0), "fill": {"linear": {"a": [0.0, 269.0], "b": [0.0, 274.0], "stops": [
        [k / 5.0, col(art, 600, 269 + k)] for k in range(6)]}}})
    lay.append({"shape": F.box(400.0, 268.0, 762.0, 269.0), "fill": col(art, 600, 268)})
    lay.append({"shape": F.box(400.0, 269.0, 401.0, 587.0), "fill": grey})
    for yy in hl_r:        # each hairline has a 1 px lighter row above and below it (255, 247, 231 on the art)
        lay.append({"shape": F.box(401.0, yy - 1.0, 762.0, yy + 2.0), "fill": col(art, 600, 283)})
        lay.append({"shape": F.box(401.0, yy, 762.0, yy + 1.0), "fill": grey})
    lay.append({"shape": F.box(761.0, 270.0, 762.0, 587.0), "fill": col(art, 761, 400)})
    return [{"name": "list", "box": [36, 264, 766, 592], "layers": lay}], {"hairlines_left": hl_l, "hairlines_right": hl_r}


# ---------------------------------------------------------------------------------------------------------------- pills
def backdrop_pills(art, op, cell_info):
    """The option pills where the backdrop draws them (left column of five 22 px pills, right column of three 25 px ones),
    each measured like the CGButtons cells (fix2_pills), its cast shadow given the idle cell's numbers so all pills are lit
    alike."""
    L = fit.lum(art)
    items = []
    for xm, xr, h in ((95, (38, 118), 28), (210, (157, 112), 31)):
        rows = [y for y in range(108, 250) if L[y, xm] < 90 and L[y - 1, xm] >= 90]
        for y0, yb in zip(rows[::2], rows[1::2]):          # (top line, bottom line) of each pill, read down column xm
            rect = (xr[0], y0 - 2, xr[1], h)
            st, info = FP.pill(art, op, rect, "backdrop", rows=(y0, yb))
            it = st["scene"][0]
            ref = cell_info["tall" if xm == 210 else "short"]
            for Lr in it["layers"]:
                if "soft" in Lr and Lr.get("fill") == [45, 45, 70] and "shape" in Lr and Lr["shape"]["t"] == "move":
                    Lr["soft"], Lr["alpha"] = ref["soft"], ref["alpha"]
            items.append(it)
    return items


# -------------------------------------------------------------------------------------------------------------- bubbles
def fit_bubble_shadow(art, cx, cy, r, others=()):
    """The cast shadow of a bubble on the flat cream page: on the art it is dark next to the rim and fades over about 9 px
    (luminance 150 at 1 px out, 180 at 5 px, the page's 245 at 10 px, read on rays at 0-90 degrees), a profile one soft
    layer cannot make. Drawn as two dark copies of the circle moved (dx, dy): a tight one and a wide one; offsets and
    softness searched, the two strengths fitted by least squares on the lower right half where the ground is the page."""
    x0, y0 = int(cx - r - 4), int(cy - r - 4)
    x1, y1 = int(cx + r + 14), int(cy + r + 14)
    wipe = [{"box": [x0, y0, x1, y1], "layers": [{"shape": F.circ(cx + 2.0, cy + 2.0, r + 11.0), "fill": [0, 0, 0]}]}]
    bg = F.wiped(art, wipe, (x0, y0, x1 - x0, y1 - y0), grow=0.5)
    X, Y = parts.grid(x0, y0, x1, y1)
    ang = np.degrees(np.arctan2(Y - cy, X - cx))
    m = (fit.lum(bg[y0:y1, x0:x1]) > 200) & (ang > -45) & (ang < 135) & (np.hypot(X - cx, Y - cy) > r + 0.7)         & (np.hypot(X - cx, Y - cy) < r + 12)
    A = art[y0:y1, x0:x1][m]
    B = bg[y0:y1, x0:x1][m]
    C = np.array([50.0, 50.0, 85.0])
    best = None
    for dx, dy, g in [(a_, b_, g_) for a_, b_ in ((0.0, 0.0), (1.0, 1.0), (1.0, 2.0), (2.0, 2.0), (3.0, 3.0))
                      for g_ in (0.0, 2.0, 4.0, 6.0)]:
        d = np.hypot(X - cx - dx, Y - cy - dy)[m] - r - g
        for s1 in (1.0, 2.0, 3.0):
            for s2 in (4.0, 6.0, 8.0, 10.0, 12.0):
                ws = []
                for s_ in (s1, s2):
                    u = np.clip(0.5 - d / (2 * s_), 0, 1)
                    ws.append(u * u * (3 - 2 * u))
                D = np.stack([((C - B) * w[:, None]).ravel() for w in ws], axis=1)
                a_, *_ = np.linalg.lstsq(D, (A - B).ravel(), rcond=None)
                a_ = np.clip(a_, 0, 1)
                err = float(((A - B).ravel() - D @ a_) ** 2).sum() if False else float((((A - B).ravel() - D @ a_) ** 2).sum())
                if best is None or err < best[0]:
                    best = (err, dx, dy, g, s1, s2, float(a_[0]), float(a_[1]))
    _, dx, dy, g, s1, s2, a1, a2 = best
    return {"d": [dx, dy], "grow": g, "tight": [s1, round(a1, 3)], "wide": [s2, round(a2, 3)], "rgb": [50, 50, 85]}


def restyle(item, shadow):
    """A bubble item with its own shadow layer replaced by the screen's shared one (moved to its circle)."""
    it = copy.deepcopy(item)
    circ = [L for L in it["layers"] if L["shape"].get("t") == "circle" and "soft" not in L][0]["shape"]
    it["layers"] = [L for L in it["layers"] if not ("soft" in L and L["shape"].get("t") == "circle")]
    dx, dy = shadow["d"]
    for key in ("tight", "wide"):
        s_, a_ = shadow[key]
        it["layers"].insert(0, {"shape": F.circ(circ["c"][0] + dx, circ["c"][1] + dy, circ["r"] + shadow["grow"]), "fill": shadow["rgb"],
                                "soft": s_, "alpha": a_})
    shadow = dict(shadow, soft=shadow["wide"][0])
    cx, cy, r = circ["c"][0], circ["c"][1], circ["r"]
    m = r + shadow["grow"] + shadow["soft"] + 3          # the soft edge spreads both ways: the box must hold all of it
    it["box"] = [min(it["box"][0], cx + dx - m), min(it["box"][1], cy + dy - m), max(it["box"][2], cx + dx + m),
                 max(it["box"][3], cy + dy + m)]
    return it


def glyph_bubbles(art, cg):
    """All eight tan bubbles where the backdrop draws them: the five redrawn ones (fix2_glyphs) and the three approved ones
    (family, head, gender: their first-round glyphs and placement) in the same paint recipe."""
    out = {}
    for name, k in (("outfit", 0), ("family", 1), ("head", 2), ("shirt", 3), ("pants", 4), ("hair", 5), ("body", 6), ("gender", 7)):
        y = 50 * k
        if name in FG.GLYPHS:
            g_local = FG.GLYPHS[name][1]()
            hint = FG.HINT.get(name)
            window = FG.WINDOWS[name]
        else:
            g_local = E.GLYPHS[name]()
            hint = None
            window = {"family": (420, 50, 500, 120), "head": (420, 100, 500, 170), "gender": (295, 120, 370, 200)}[name]
        cx, cy, r, _ = E.rim_circle(cg, (300, y, 42, 42), hint=hint)
        v, dx, dy = E.locate(cg, (300, y, 42, 42), art, window)
        if name in FG.GLYPHS:
            gl, _ = FG.centred(g_local, cx + dx, cy + dy, 300 + dx, y + dy)
        else:
            gl = F.mv(g_local, 300 + dx, y + dy)
        out[name] = FG.bubble(art, cx + dx, cy + dy, r, gl)
    return out


def main():
    art, op = c.sheet(BG_F)
    male, opm = c.sheet(BG_M)
    cg, cgo = c.sheet(CB)
    FB.editor_bubbles.__globals__["F"]._only = lambda eid: True      # build the bubbles without delivering them again
    save_deliver = F.deliver
    F.deliver = lambda *a, **k: None
    try:
        FB.editor_bubbles()
    finally:
        F.deliver = save_deliver
    bub = dict(FB.BACKDROP)
    ok_glyph = E.g_ok()
    cxo, cyo, ro, _ = E.rim_circle(cg, (500, 100, 58, 57), hint=(526.0, 126.5, 27.0))
    v, dxo, dyo = E.locate(cg, (500, 100, 58, 57), art, (725, 180, 800, 260))
    okb = E.bubble(art, cxo + dxo, cyo + dyo, ro, F.mv(ok_glyph, dxo, dyo), rim_w=2.2)
    okb["layers"][-1]["fill"] = F.glyph_paint(art, okb["layers"][-1]["shape"], okb["box"], fallback=okb["layers"][-1]["fill"])
    gb = glyph_bubbles(art, cg)

    # the shared bubble shadow: fitted on the five page bubbles (flat cream ground), median of the numbers
    fits = []
    for name in ("outfit", "family", "head", "shirt", "pants"):
        circ = [L for L in gb[name]["layers"] if L["shape"].get("t") == "circle" and "soft" not in L][0]["shape"]
        fits.append(fit_bubble_shadow(art, circ["c"][0], circ["c"][1], circ["r"]))
    print("bubble shadow fits", fits)
    shadow = {"d": [float(np.median([f["d"][0] for f in fits])), float(np.median([f["d"][1] for f in fits]))],
              "grow": float(np.median([f["grow"] for f in fits])),
              "tight": [float(np.median([f["tight"][0] for f in fits])), round(float(np.median([f["tight"][1] for f in fits])), 3)],
              "wide": [float(np.median([f["wide"][0] for f in fits])), round(float(np.median([f["wide"][1] for f in fits])), 3)],
              "rgb": [50, 50, 85]}
    print("bubble shadow", shadow)
    gb = {k: restyle(v, shadow) for k, v in gb.items()}
    bub = {k: restyle(v, shadow) for k, v in bub.items()}
    okb = restyle(okb, shadow)

    # ---- name card (redo)
    card, arch = name_card(art)
    card_scene = [card, bub["editor-load"], bub["editor-save"]]
    F.deliver("editor-name-card", [{"name": "idle", "art": art, "scene": card_scene, "rect": (24, 0, 256, 116)}],
              "one card: a 2 px navy line round an arched top (circle fitted to the arch's top edge), straight sides and a "
              "3 px bottom; cream above a 1 px navy line at row 44, tan below with two 1 px hairlines (rows 64, 84) and the "
              "left line's 6 px shadow in the tan; the card's cast shadow right and below; the load and save bubbles drawn "
              "over its top corners with their cast shadows",
              "redone from a new reading: one card with a tan field and two hairlines (the first round's three boxed "
              "stripes and the extra vertical lines are gone), and the load / save bubbles now drawn ABOVE the card",
              z=4, extra={"arch": arch, "bubble_shadow": shadow})

    # ---- page
    pg, pinfo = page(art)
    page_scene = [pg] + [gb[n] for n in ("outfit", "family", "head", "shirt", "pants")]
    F.deliver("editor-page", [{"name": "idle", "art": art, "scene": page_scene, "rect": (430, 8, 316, 250)}],
              "a cream page in a 2 px navy line (straight top and bottom, a right edge bulging as a fitted circle) with its "
              "fitted cast shadow; the five bubbles of the button column over its left edge, each with the shared cast "
              "shadow (two soft dark copies of the circle, a tight and a wide one, fitted on these five; median offset "
              f"{shadow['d']}, tight {shadow['tight']}, wide {shadow['wide']} as [softness, strength])",
              "the buttons' shadows on the page are drawn (one fitted recipe for every bubble on the screen); the page's own "
              "shadow added", z=2, extra={"page": pinfo, "bubble_shadow": shadow})

    # ---- list
    lst, linfo = editor_list(art)
    F.deliver("editor-list", [{"name": "idle", "art": art, "scene": lst, "rect": (36, 262, 732, 330)}],
              f"a dark column (navy line, inner shade, {len(linfo['hairlines_left'])} navy hairlines, divider) beside one cream "
              f"rectangle with {len(linfo['hairlines_right'])} 1 px grey hairlines read row by row, a 1 px grey line down its "
              "left side and the soft shade along its top",
              "right column redrawn as one cream rectangle with 1 px hairlines (the first round's inset rows on a grey field "
              "left 2-3 px grey bands)", z=1, extra=linfo)

    # ---- the backdrop
    import json
    pill_info = json.load(open(F.c.OUT + "/editor-pill.json"))["info"][0]["shadow"]
    tall_info = json.load(open(F.c.OUT + "/editor-pill-tall.json"))["info"][0]["shadow"]
    pills = backdrop_pills(art, op, {"short": pill_info, "tall": tall_info})
    order = lst + [card] + pills + [pg] + [gb[n] for n in ("outfit", "family", "head", "shirt", "pants")] \
        + [gb[n] for n in ("hair", "body", "gender")] + [bub["editor-load"], bub["editor-save"], bub["editor-reset"], okb]
    male_order = [it for it in order if it is not gb["hair"]]
    F.deliver("editor-backdrop", [{"name": "female", "art": art, "scene": order, "rect": (0, 0, 800, 600)},
                                  {"name": "male", "art": male, "scene": male_order, "rect": (0, 0, 800, 600)}],
              "the editor screen in the game's order: the list, the name card, the eight option pills, the page, then every "
              "bubble on top (the five-button column over the page's edge, the three by the portrait, load / save on the "
              "card's corners, reset and OK), every bubble with the same fitted cast shadow; the cloud, the portrait "
              "window, the cream backing behind the pills, the wires and the reset / OK socket rings stay the texture; "
              "the male version lacks the hair button",
              "redone in draw order: no panel covers a button any more (the card and page were drawn over the bubbles); "
              "the pills, which are in this picture, are drawn now; all bubbles share one shadow recipe", z=1)


if __name__ == "__main__":
    main()
