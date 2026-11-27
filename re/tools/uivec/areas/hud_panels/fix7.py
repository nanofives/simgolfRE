"""P41 hud_panels round 7 (2026-11-26, log/roadmap/P41/FIX7_BRIEF.md #5): one structure per button across its states.

Mariano: "el boton de contratar empleado se ve inconsistente en las 3 animaciones, este es el que hay que usar de base
[...] lo mismo con el del panel de los golfers [...] asegurate de que todos se vean con la misma estructura en las
variantes".

What was inconsistent (fix7_look.py renders the approved scenes):
- The three blue tab glyphs (staff figure, golfers group, golfer swing) each appear in three trays (yellow when that
  tray is the selected one, blue baked into the other two) and as separate cuts. The approved trays were built in round
  2 and never rebuilt, so the staff figure was drawn three ways (the round-1 blob figure on the yellow Employee tab, the
  round-2 man with a knobbed post baked into the Member / Action trays, the round-4 groundskeeper on the cuts), the
  group two ways (arms raised on the Employee tray), and every yellow tab used the round-1 glyphs and its own, larger
  measured bubble (rim r 15.2 against 14.8; halo r 18.75 against 16.39) although the art's rim sits at the same radius
  on yellow and blue tabs (dark rim 13.5 px from the cell centre at every angle, measured 2026-11-26).
- Tan bubble buttons (club / crossed / trophy, hire / fire / details, thumbs up, stats), the golfer info tabs, the
  pedestals and the shot pads: every state was measured on its own art, so rim, body and glyph drift by 0.1-0.35 px
  between states, the cast shadow was refitted per state (r 12.0-18.5), and the club column's lavender ground moved
  with the bubble in hover / disabled although the art's ground and stub stay put (column 36 identical in all four
  cells; only the bubble drops one pixel).

What is drawn now:
- Tabs: ONE scene per glyph, the approved idle cut (tab-golfer-staff: the round-4 groundskeeper; tab-golfers-group: the
  akimbo trio in Mariano's picture; tab-golfer: the swing), placed by whole pixels at every cut and tray position. Blue
  instances carry the canonical paint unchanged; a yellow instance keeps every shape and the navy glyph, and only the
  rim and body paint are re-measured on its yellow art (the shirt between the straps, which is the tab's own colour
  showing through, takes the yellow body colour).
- Multi-state buttons: every state is the idle scene's shapes moved by whole pixels where the art moves the button
  (measured per layer from the state scenes, then rounded; grounds that the art does not move are held), with the
  state's own paints. A layer whose state version differs from idle by more than 0.6 px after the move (in practice
  only cast shadows) keeps the state's own shape only when it fits the art clearly better (mean error over the pixels
  where the two versions differ at least 20 % lower), since a pressed button's shadow is light, not structure.

Usage: py -3.12 fix7.py [element ...]   writes log/roadmap/P41/work7/hud_panels/
"""
import copy
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import kit as K  # noqa: E402
import figs  # noqa: E402,F401   registers hud_panels.limb / rrect4
import fix7_audit as A  # noqa: E402

OUT = os.path.join(C.ROOT, "log", "roadmap", "P41", "work7", "hud_panels")
FLAT = C.FLAT


def key_of(doc):
    return [k for k, n in C.SHEETS.items() if n.lower() in doc["coords"].lower()][0]


def no_half(node):
    """a shape tree with half-plane anchors removed (a horizontal half-plane's x means nothing; the audit read it as a
    100 px move)"""
    if isinstance(node, list):
        return [no_half(n) for n in node]
    if not isinstance(node, dict):
        return node
    if node.get("t") == "half":
        return {"t": "half", "n": node.get("n")}
    if node.get("t") == "lathe":          # axis x and profile rows: points (x, row) so the move sees them
        return {"t": "lathe", "pts": [[node["x"], q[0], q[1]] for q in node["prof"]]}
    if isinstance(node.get("box"), list) and len(node["box"]) == 4 and "t" in node:
        b = node["box"]
        return {**{k: no_half(v) for k, v in node.items() if k != "box"}, "p": [[b[0], b[1]], [b[2], b[3]]]}
    return {k: no_half(v) for k, v in node.items()}


def pts_of(shape):
    p, s = [], []
    A.walk(no_half(shape), p, s)
    return np.array([[x, y] for _, x, y in p]) if p else np.zeros((0, 2)), np.array([v for _, v in s])


def low(scene, rct):
    x, y, w, h = rct
    return C.render_cell(scene, rct, 4).reshape(h, 4, w, 4, 3).mean(axis=(1, 3))


# ------------------------------------------------------------------------------------------------------------ unify
# items the art does not move (measured 2026-11-26): the club column's lavender ground and stub (column 36 identical in
# all four cells), the pedestal (its outline at row 21 / column 32 identical in all six cells)
FIXED_ITEMS = {"golfer-club-button": {0}, "golfer-crossed-button": {0}, "golfer-trophy-button": {0},
               "employee-pedestal": {0}}
FAMILIES = {"employee-pedestal": [["idle", "selected", "disabled"], ["idle 2", "selected 2", "disabled 2"]]}
# the shot pads' disabled art sits one pixel lower than idle, like selected and hover (outline rows 2 -> 3 and 38 -> 39,
# the white curve 22 -> 23 at the middle column; fix7_artmove.py), but its approved scene was drawn at idle's place:
# its layers take the selected state's per-layer moves (pad +1, lavender block held)
MOVE_AS = {f"shot-shape-{n}": {"disabled": "selected"} for n in ("low", "rising", "flat", "high", "straight")}


def unify(el, doc=None, log=None):
    """-> (new doc, notes per state)"""
    doc = copy.deepcopy(doc or A.approved(el))
    key = key_of(doc)
    rgb, m = C.sheet(key)
    fams = FAMILIES.get(el, [list(doc["states"])])
    notes = {}
    for fam in fams:
        ref = fam[0]
        base = A.cell_rel(doc["states"][ref])
        fixed = FIXED_ITEMS.get(el, set())
        fixed_shapes = [json.dumps(L["shape"], sort_keys=True) for i in fixed for L in base[i]["layers"]]
        for st in fam[1:]:
            v = doc["states"][st]
            x, y, w, h = v["cell"]
            s = A.cell_rel(v)
            if len(s) != len(base):
                raise ValueError(f"{el} {st}: item count")
            new, cand, nt = [], [], []
            for i, (a, b) in enumerate(zip(base, s)):
                if len(a["layers"]) != len(b["layers"]):
                    raise ValueError(f"{el} {st} item {i}: layer count")
                info = []
                other = MOVE_AS.get(el, {}).get(st)
                so = A.cell_rel(doc["states"][other]) if other else None
                for k, (La, Lb) in enumerate(zip(a["layers"], b["layers"])):
                    pa, sa = pts_of(La["shape"])
                    pb, sb = pts_of(Lb["shape"])
                    if len(pa) != len(pb) or len(sa) != len(sb):
                        raise ValueError(f"{el} {st} item {i}: layer structure")
                    t = np.median(pb - pa, axis=0) if len(pa) else np.zeros(2)
                    mv = np.round(t)
                    res = max(float(np.abs(pb - pa - mv).max()) if len(pa) else 0.0,
                              float(np.abs(sb - sa).max()) if len(sa) else 0.0)
                    if other:
                        po, _ = pts_of(so[i]["layers"][k]["shape"])
                        mv = np.round(np.median(po - pa, axis=0)) if len(pa) else mv
                    info.append((mv, res, len(pa)))
                good = [(n, tuple(mv)) for mv, res, n in info if res <= 0.6]
                dom = max(good)[1] if good else (0.0, 0.0)
                if i in fixed:
                    dom = (0.0, 0.0)
                moved = sorted({tuple(int(q) for q in mv) for mv, res, n in info if res <= 0.6})
                item = {k: copy.deepcopy(b.get(k, a[k])) for k in a if k not in ("box", "layers")}
                item["box"] = list(b["box"])
                item["layers"] = []
                for k, (La, Lb) in enumerate(zip(a["layers"], b["layers"])):
                    mv, res, _ = info[k]
                    if i in fixed:
                        mv = np.zeros(2)
                    elif res > 0.6:
                        mv = np.array(dom)
                    L = {q: copy.deepcopy(val) for q, val in Lb.items() if q not in ("shape", "clip", "fill")}
                    L["shape"] = K.shift(La["shape"], float(mv[0]), float(mv[1]))
                    if "clip" in La:
                        if json.dumps(La["clip"], sort_keys=True) in fixed_shapes or i in fixed:
                            cm = np.zeros(2)
                        else:
                            ca, _ = pts_of(La["clip"])
                            cb, _ = pts_of(Lb["clip"])
                            cm = np.round(np.median(cb - ca, axis=0)) if len(ca) and len(ca) == len(cb) else mv
                        L["clip"] = K.shift(La["clip"], float(cm[0]), float(cm[1]))
                    pa, _ = pts_of(La["shape"])
                    pb, _ = pts_of(Lb["shape"])
                    if res > 0.6:
                        # a layer the state reshaped (a cast shadow): the candidate is idle's whole layer, its paint
                        # included (that paint was measured on that shape); the art test below decides
                        L.update({q: copy.deepcopy(val) for q, val in La.items() if q not in ("shape", "clip", "fill")})
                        L["fill"] = K.shift(La["fill"], float(mv[0]), float(mv[1])) if isinstance(La["fill"], dict) \
                            else copy.deepcopy(La["fill"])
                    else:
                        d = np.median(pa + mv - pb, axis=0) if len(pa) else np.zeros(2)
                        L["fill"] = K.shift(Lb["fill"], float(d[0]), float(d[1])) if isinstance(Lb["fill"], dict) \
                            else copy.deepcopy(Lb["fill"])
                    item["layers"].append(L)
                    if res > 0.6:
                        cand.append((i, k, copy.deepcopy(Lb), res))
                new.append(item)
                nt.append(f"item {i}: move {tuple(int(q) for q in dom)}" + (" (held: the art's ground does not move)"
                                                                            if i in fixed else "")
                          + (f", layer moves {moved}" if len(moved) > 1 else ""))
            # the layers that differ from idle beyond 0.6 px: keep the state's own only when it fits the art better
            art = rgb[y:y + h, x:x + w]
            mk = m[y:y + h, x:x + w]
            for i, k, Lb, res in cand:
                Lb = copy.deepcopy(Lb)
                if "clip" in new[i]["layers"][k]:      # a kept shadow is still clipped by the shared (held) ground
                    Lb["clip"] = copy.deepcopy(new[i]["layers"][k]["clip"])
                sa = K.shift(new, x, y)
                sb = copy.deepcopy(sa)
                Lown = K.shift(Lb, x, y)
                sb[i]["layers"][k] = Lown
                ra, rb = low(sa, (x, y, w, h)), low(sb, (x, y, w, h))
                diff = (np.abs(ra - rb).max(axis=2) > 3) & mk
                if not diff.any():
                    continue
                ea = float(np.abs(ra - art)[diff].mean())
                eb = float(np.abs(rb - art)[diff].mean())
                nm = Lb.get("name", Lb["shape"]["t"])
                if eb < 0.8 * ea:
                    new[i]["layers"][k] = Lb
                    nt.append(f"item {i} layer {k} ({nm}): own shape kept, it fits the art ({eb:.1f} vs idle's {ea:.1f}"
                              f" over {int(diff.sum())} px)")
                else:
                    nt.append(f"item {i} layer {k} ({nm}): idle's shape ({ea:.1f} vs own {eb:.1f} over "
                              f"{int(diff.sum())} px; was {res:.2f} px off)")
            v["scene"] = K.shift(new, x, y)
            notes[st] = nt
    return doc, notes


# ------------------------------------------------------------------------------------------------------------ tabs
# the canonical tab of each glyph: the approved idle cut (sheet key, cell top-left)
CANON = {"staff": ("tab-golfer-staff", "M", (0, 450)), "group": ("tab-golfers-group", "J", (0, 400)),
         "golfer": ("tab-golfer", "M", (0, 400))}
CUTS = {"tab-golfer-staff": "staff", "joecool-tab-staff": "staff", "tab-golfers-group": "group",
        "tab-staff-group": "group", "tab-golfer": "golfer", "employee-tab-golfer": "golfer", "tab-golfer-swing": "golfer"}
TRAY_EL = {"M": "member-panel", "J": "golfer-action-panel", "E": "employee-panel"}
TAB_AT = {"group": (270, 476), "golfer": (240, 495), "staff": (217, 523)}     # 33 px cells in the trays
SELECTED = {"M": "group", "J": "golfer", "E": "staff"}


def canon_items(fam):
    el, key, (x, y) = CANON[fam]
    doc = A.approved(el)
    return A.cell_rel(doc["states"]["idle"])


def yellow_paint(items, key):
    """re-measure the rim and body paint of a placed canonical tab on the yellow art; glyph paints stay (navy), the
    shirt cloth takes the yellow body colour sampled between the straps"""
    import bubble as B
    import el_figs as F
    import sdf
    it = items[1]
    rim, body = it["layers"][0], it["layers"][1]
    c = rim["shape"]["c"] + [rim["shape"]["r"]]
    bc = body["shape"]["c"] + [body["shape"]["r"]]
    glyph = K.U(*[L["shape"] for L in it["layers"][2:]])
    coef = B.solve_family([B.body_rows(key, c, bc, trim_glyph=glyph)])
    coef[-1] = [coef[-1][0], [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]
    rim["fill"] = F.rim_paint(key, c, bc[2])
    body["fill"] = {"bevel": {"L": bc[2], "stops": B.STOPS, "coef": coef}}
    rgb, m = C.sheet(key)
    for L in it["layers"][2:]:
        if L.get("name") == "shirt cloth":
            x0, y0, x1, y1 = [int(v) for v in (bc[0] - bc[2], bc[1] - bc[2], bc[0] + bc[2] + 1, bc[1] + bc[2] + 1)]
            jj, ii = np.mgrid[y0:y1, x0:x1]
            ins = sdf.sd(L["shape"], ii + 0.5, jj + 0.5) < 0
            px = rgb[y0:y1, x0:x1][ins]
            px = px[C.fit.lum(px) > 120] if len(px) else px
            if len(px):
                L["fill"] = [round(float(v), 1) for v in np.median(px, axis=0)]
    return items


def tab_items(fam, key, x, y, yellow=False):
    items = K.shift(canon_items(fam), x, y)
    return yellow_paint(items, key) if yellow else items


def cut_doc(el):
    fam = CUTS[el]
    doc = copy.deepcopy(A.approved(el))
    key = key_of(doc)
    v = doc["states"]["idle"]
    x, y = v["cell"][:2]
    v["scene"] = tab_items(fam, key, x, y)
    return doc


def tray_doc(key):
    el = TRAY_EL[key]
    doc = copy.deepcopy(A.approved(el))
    st = list(doc["states"])[0]
    sc = doc["states"][st]["scene"]
    centres = {f: (x + 16.5, y + 16.5) for f, (x, y) in TAB_AT.items()}
    keep, first = [], None
    for i, it in enumerate(sc):
        bx = it["box"]
        cx, cy = (bx[0] + bx[2]) / 2, (bx[1] + bx[3]) / 2
        hit = (bx[2] - bx[0]) < 46 and any(np.hypot(cx - a, cy - b) < 4 for a, b in centres.values())
        if hit:
            first = i if first is None else first
        else:
            keep.append(it)
    tabs = []
    for fam in ("group", "golfer", "staff"):
        x, y = TAB_AT[fam]
        tabs += tab_items(fam, key, x, y, yellow=(SELECTED[key] == fam))
    removed = len(sc) - len(keep)
    doc["states"][st]["scene"] = keep[:first] + tabs + keep[first:]
    return doc, removed


# ------------------------------------------------------------------------------------------------------------ images
def edges(img):
    L = img.mean(axis=2)
    g = np.zeros_like(L)
    g[:, 1:] = np.maximum(g[:, 1:], np.abs(np.diff(L, axis=1)))
    g[1:, :] = np.maximum(g[1:, :], np.abs(np.diff(L, axis=0)))
    return np.clip(g / 40.0, 0, 1)


COLS = [(220, 30, 30), (20, 150, 40), (30, 60, 220), (230, 140, 0), (160, 30, 200), (0, 160, 170)]


def overlay(renders):
    out = np.full(renders[0].shape, 255.0)
    for k, r in enumerate(renders):
        e = edges(r)[..., None] * 0.55
        out = out * (1 - e) + np.array(COLS[k % len(COLS)], float) * e
    return out


def label(img, text):
    im = Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8))
    ImageDraw.Draw(im).text((3, 2), text, fill=(255, 255, 0))
    return np.asarray(im).astype(float)


def hcat(ts, gap=6):
    H = max(t.shape[0] for t in ts)
    ts = [np.pad(t, ((0, H - t.shape[0]), (0, 0), (0, 0)), constant_values=255) for t in ts]
    return np.concatenate(sum([[t, np.full((H, gap, 3), 255.0)] for t in ts], [])[:-1], axis=1)


def vcat(ts, gap=6):
    W = max(t.shape[1] for t in ts)
    ts = [np.pad(t, ((0, 0), (0, W - t.shape[1]), (0, 0)), constant_values=255) for t in ts]
    return np.concatenate(sum([[t, np.full((gap, W, 3), 255.0)] for t in ts], [])[:-1], axis=0)


def save(img, name):
    os.makedirs(OUT, exist_ok=True)
    Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(OUT, name))


def compare_rows(rows, z, align=None):
    """rows: [(label, key, rct, prev scene, new scene)] -> art | previous | new per row, then the overlay of every new
    row's edges (aligned by `align`: per row (dx, dy) whole px removed before overlaying)"""
    out, news = [], []
    for k, (lab, key, rct, prev, new) in enumerate(rows):
        x, y, w, h = rct
        rgb, _ = C.sheet(key)
        art = np.kron(rgb[y:y + h, x:x + w], np.ones((z, z, 1)))
        p = C.render_cell(prev, rct, z) if prev is not None else np.full(art.shape, 255.0)
        n = C.render_cell(new, rct, z)
        out.append(hcat([label(art, lab), p, n]))
        if align:
            dx, dy = align[k]
            n = C.render_cell(new, (x + dx, y + dy, w, h), z)
        news.append(n)
    ov = overlay(news)
    legend = ", ".join(f"{rows[k][0]}" for k in range(len(rows)))
    out.append(hcat([label(ov, "overlay of the new states' edges"), np.full((ov.shape[0], 4, 3), 255.0)]))
    return vcat(out), legend


def write_doc(doc, el):
    doc["id"] = el          # the approved trophy scene still carries its round-3 version tag (golfer-trophy-button--b)
    with open(os.path.join(OUT, el + ".json"), "w") as f:
        json.dump(doc, f, default=C._r)


# ------------------------------------------------------------------------------------------------------------ run
def run_tabs(results):
    prev_tray = {k: A.approved(TRAY_EL[k]) for k in "MJE"}
    trays = {}
    for k in "MJE":
        trays[k], removed = tray_doc(k)
        print(TRAY_EL[k], "tab items replaced:", removed)
    cuts = {el: cut_doc(el) for el in CUTS}
    for fam in ("staff", "group", "golfer"):
        rows = []
        for k in "MJE":
            x, y = TAB_AT[fam]
            st = list(trays[k]["states"])[0]
            col = "yellow" if SELECTED[k] == fam else "blue"
            rows.append((f"{TRAY_EL[k]} ({col})", k, (x, y, 33, 33), prev_tray[k]["states"][st]["scene"],
                         trays[k]["states"][st]["scene"]))
        for el, f in CUTS.items():
            if f != fam:
                continue
            d = cuts[el]
            v = d["states"]["idle"]
            rows.append((f"{el} (cut)", key_of(d), tuple(v["cell"]), A.approved(el)["states"]["idle"]["scene"],
                         v["scene"]))
        img, legend = compare_rows(rows, 8)
        for el, f in CUTS.items():
            if f == fam:
                save(img, el + ".png")
        results.append((fam, legend))
    for el, d in cuts.items():
        write_doc(d, el)
    for k in "MJE":
        write_doc(trays[k], TRAY_EL[k])
        st = list(trays[k]["states"])[0]
        x0, y0 = 214, 474
        rct = (x0, y0, 100, 100)
        img, _ = compare_rows([(TRAY_EL[k], k, rct, prev_tray[k]["states"][st]["scene"],
                                trays[k]["states"][st]["scene"])], 5)
        save(img, TRAY_EL[k] + ".png")
    return cuts, trays


def run_multi(el):
    prev = A.approved(el)
    doc, notes = unify(el, prev)
    write_doc(doc, el)
    key = key_of(doc)
    rows, align = [], []
    w = doc["states"][list(doc["states"])[0]]["cell"][2]
    z = max(4, min(8, 1580 // (3 * w + 12)))
    ref = list(doc["states"])[0]
    base = A.cell_rel(doc["states"][ref])
    for st, v in doc["states"].items():
        rows.append((st, key, tuple(v["cell"]), prev["states"][st]["scene"], v["scene"]))
        s = A.cell_rel(v)
        mv = A.item_compare(base[-1], s[-1])[2] if st != ref else (0, 0)
        align.append(mv if mv else (0, 0))
    img, _ = compare_rows(rows, z, align=align)
    save(img, el + ".png")
    # cross-check: the button's move in the new scene (its item with the most points) against the art's own move
    import fix7_artmove as AM
    for r in (["idle", "idle 2"] if el == "employee-pedestal" else [None]):
        for st, (mv, sc, _, _) in AM.art_moves(el, r).items():
            if el == "employee-pedestal" and st.endswith(" 2") != (r == "idle 2"):
                continue
            rf = r or ref
            b0, s0 = A.cell_rel(doc["states"][rf]), A.cell_rel(doc["states"][st])
            big = max(range(len(b0)), key=lambda i: len(b0[i]["layers"]))     # the bubble / pad item
            moves = []
            for La, Lb in zip(b0[big]["layers"], s0[big]["layers"]):
                pa, _ = pts_of(La["shape"])
                pb, _ = pts_of(Lb["shape"])
                if len(pa):
                    moves.append(tuple(int(q) for q in np.round(np.median(pb - pa, axis=0))))
            dom = max(set(moves), key=moves.count) if moves else (0, 0)
            notes.setdefault(st, []).append(f"art moves the button {mv}, the new scene {dom}" +
                                            ("" if tuple(mv) == dom else "  <-- MISMATCH"))
    return notes


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    args = sys.argv[1:]
    if not args or "tabs" in args:
        res = []
        run_tabs(res)
    for el in [a for a in args if a != "tabs"] or A.MULTI:
        notes = run_multi(el)
        with open(os.path.join(OUT, "_notes_" + el + ".json"), "w") as f:
            json.dump(notes, f, indent=1)
        print(el, json.dumps(notes))
