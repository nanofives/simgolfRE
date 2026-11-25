"""P41 round 5 (2026-11-25): relight route-top-employ / -aura / -value on route-top-course's approved round-2 recipe.

Recipe (route_tops2.py, approved on route-top-course): each sunk recess (banner, list, table) = its outer lip as solved in
round one + a FLAT floor + ONE inner shadow (black at alpha a with a soft rounded rectangle cut out, ishadow.json), the
same numbers on every recess. Round one had a nearest-edge bevel inside each recess instead (mitred corners, a light band
inside the right and bottom walls).

What the art of these three sheets shows (pixel rows, route screens_<mode>.pcx):
  employ table top / left walls: 161/255 on the cream column (x 360, y 68), 153/243 on the cells (x 100, y 68), 24/36 on
  the vertical rule (x 331, y 68): the shadow lies OVER the cream column and the rules, ratio 0.63 at the wall as on the
  course banner / list. Right wall: no darkening on the floor (x 386..392, y 90 all 255), the light lip outside it.
  Foot: 233/243 one px above the slab line (the soft rectangle's foot). The table floor is 243,230,204 (the stripe colour),
  not the tan the round-two FLOOR table guessed (round two never drew employ).
  aura / value list under the slab line: 0.77, 0.86, 0.95 of the floor, the profile of course's list.
So: the same layers as course, with the shadow drawn after everything lying on the floor (stripes, cream column, rules,
cream box, bullets) and before the pills, the slab, the meter and the plate. Shapes unchanged (route_tops.components).
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C          # noqa: E402
import deliver as D         # noqa: E402
import route_tops as R      # noqa: E402
import route_tops2 as R2    # noqa: E402
import route_meters as M    # noqa: E402
import sdf                  # noqa: E402

OUT5 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work5", "reports_maps")
OUT1 = os.path.join(C.ROOT, "log", "roadmap", "P41", "work", "reports_maps")
OUT2 = R2.OUT2
FLOOR5 = dict(R2.FLOOR, table=R.STRIPE)


def on_floor(kind):
    return isinstance(kind, tuple) and kind[0] in ("flat", "stripe")


def scene(mode, styles, ish, meters=None, bsh=None):
    bsh = bsh or R2.load_box()
    saved = dict(R2.FLOOR)
    R2.FLOOR.clear()
    R2.FLOOR.update(FLOOR5)
    try:
        out, later = [], []
        for n, k, S in R.components(mode):
            if not isinstance(k, tuple) and k in FLOOR5:
                a, b = R2.item_split(n, k, S, styles, ish)
                out.append(a)
                later.append(b)
                continue
            if on_floor(k):
                out.append(R.item(n, k, S, styles))
                continue
            out += later        # the shadows go on once everything on the floor is down
            later = []
            if isinstance(k, tuple) and k[0] == "meter":
                if meters is not None:
                    out += M.scene(mode, meters, styles)
                continue
            it = R2.box_item(n, S, styles, bsh) if k == "box" else R.item(n, k, S, styles)
            if k == "slab" and mode == "employ":
                # the slab's outer lip is an opaque lavender paint solved on the lavender ground; above its top line it would
                # paint a lavender band over the tables' feet (the art: floor 243 down to y 244, 233 at y 245, then the
                # line). Keep the lip only below the top line (its sides), as the list's lip is kept on its sides and foot.
                top = S["c"][1] - S["half"][1]
                it["layers"][0]["clip"] = {"t": "half", "p": [0, top + 0.9], "n": [0, -1]}
            out.append(it)
        return out + later
    finally:
        R2.FLOOR.clear()
        R2.FLOOR.update(saved)


def render(base, sc, origin, z):
    h, w = base.shape[:2]
    k = int(np.ceil(4.0 / z))
    s = z * k
    img = np.kron(base, np.ones((s, s, 1)))
    sdf.render(sc, img, s, origin=origin)
    if k > 1:
        img = img.reshape(h * z, k, w * z, k, 3).mean(axis=(1, 3))
    return img


def tile3(rgb, base, sc1, sc5, win, z):
    x, y, w, h = win
    a = np.kron(rgb[y:y + h, x:x + w], np.ones((z, z, 1)))
    b = render(base[y:y + h, x:x + w], sc1, (x, y), z)
    c = render(base[y:y + h, x:x + w], sc5, (x, y), z)
    g = np.full((h * z, 6, 3), 255.0)
    return np.concatenate([a, g, b, g, c], axis=1)


def round1_scene(eid):
    with open(os.path.join(OUT1, eid + ".json")) as f:
        return json.load(f)["states"]["idle"]["scene"]


def mad_win(rgb, base, sc, wins, s=4):
    t = []
    for x, y, w, h in wins:
        img = np.kron(base[y:y + h, x:x + w], np.ones((s, s, 1)))
        sdf.render(sc, img, s, origin=(x, y))
        low = img.reshape(h, s, w, s, 3).mean(axis=(1, 3))
        t.append(np.abs(low - rgb[y:y + h, x:x + w]).mean())
    return float(np.mean(t))


# 8x detail windows: recess corners (TL / TR / foot) and what lies on the floor near the walls
WINS = {
    "employ": [(24, 58, 40, 40), (366, 58, 44, 40), (320, 58, 40, 40), (24, 220, 40, 34), (366, 220, 44, 34), (740, 58, 40, 40)],
    "aura": [(30, 42, 44, 40), (732, 42, 44, 40), (0, 82, 44, 40), (756, 82, 44, 40), (30, 150, 44, 40), (730, 150, 44, 40)],
    "value": [(30, 42, 44, 40), (732, 42, 44, 40), (0, 82, 44, 40), (756, 82, 44, 40), (20, 100, 44, 40), (460, 100, 44, 40)],
}
COURSE_WINS = [(30, 42, 44, 40), (732, 42, 44, 40), (0, 82, 44, 40), (756, 82, 44, 40), (280, 160, 44, 40), (488, 160, 44, 40)]
TILE_LABEL = {
    "employ": "table 1 TL, table 1 TR, cream column top, table 1 foot, table 2 foot / TL, table 2 TR",
    "aura": "banner TL / TR, list TL / TR, cream box BL / BR",
    "value": "banner TL / TR, list TL / TR, bullets and stripes by the left wall, the second column's bullets",
}


def ish5():
    """route-top-course's inner shadow; its foot offset b (3.0) was the untuned start value (course's recess feet are hidden
    under the slab / off the sheet). The employ table's foot shows no shade (243 down to the slab line), so b = r: no shade
    on the right and bottom walls, as measured on the right walls. Course renders the same either way (checked)."""
    ish = dict(R2.load_ish())
    ish["b"] = ish["r"]
    return ish


def build(mode):
    st = R.load_styles()
    ish = ish5()
    ms = M.load()
    rgb, m = C.sheet(f"route screens_{mode}.pcx")
    base = np.empty_like(rgb)
    base[:] = R.LAV
    sc1 = round1_scene(f"route-top-{mode}")
    sc5 = scene(mode, st, ish, ms)
    return rgb, base, sc1, sc5


def full3(rgb, base, sc1, sc5):
    a = rgb.astype(float)
    b = render(base, sc1, (0, 0), 1)
    c = render(base, sc5, (0, 0), 1)
    g = np.full((rgb.shape[0], 6, 3), 255.0)
    img = np.concatenate([a, g, b, g, c], axis=1)
    from PIL import Image
    k = 1600 / img.shape[1]
    return np.asarray(Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).resize(
        (1600, int(round(img.shape[0] * k))), Image.LANCZOS), float)


def deliver(mode, changed, unsure=""):
    os.makedirs(OUT5, exist_ok=True)
    eid = f"route-top-{mode}"
    rgb, base, sc1, sc5 = build(mode)
    tiles = [full3(rgb, base, sc1, sc5)]
    tiles += [tile3(rgb, base, sc1, sc5, w, 8) for w in WINS[mode][:3]]
    tiles += [tile3(rgb, base, sc1, sc5, w, 8) for w in WINS[mode][3:]]
    C.save_png(C.stack(tiles), os.path.join(OUT5, f"{eid}.png"))
    m1, _ = D.score(rgb, base, sc1, (0, 0))
    m5, _ = D.score(rgb, base, sc5, (0, 0))
    w1 = mad_win(rgb, base, sc1, WINS[mode])
    w5 = mad_win(rgb, base, sc5, WINS[mode])
    js = {"id": eid, "round": 5, "recipe": "route_tops2 (route-top-course round 2)", "ishadow": ish5(),
          "states": {"idle": {"origin": [0.0, 0.0], "size": [800, int(rgb.shape[0])], "scene": sc5}}}
    with open(os.path.join(OUT5, f"{eid}.json"), "w") as f:
        json.dump(js, f)
    print(f"[fix5] {eid}: interior MAD r1 {m1:.2f} -> r5 {m5:.2f}; corner windows MAD r1 {w1:.2f} -> r5 {w5:.2f}", flush=True)
    return {"id": eid, "status": "done", "changed": changed, "unsure": unsure,
            "interior_mad": round(m5, 2), "window_mad_r1": round(w1, 2), "window_mad_r5": round(w5, 2)}


def family():
    """The four route-top bands side by side at 4x: the same windows (banner / list top-left corner, list right wall)
    from each sheet, rendered on the lavender ground; course is the approved round-2 scene."""
    st = R.load_styles()
    ish = ish5()
    bsh = R2.load_box()
    ms = M.load()
    cols = []
    for mode in ("course", "employ", "aura", "value"):
        rgb, m = C.sheet(f"route screens_{mode}.pcx")
        base = np.empty_like(rgb)
        base[:] = R.LAV
        sc = R2.scene("course", st, ish, bsh=bsh) if mode == "course" else scene(mode, st, ish, ms)
        if mode == "employ":
            ws = [(20, 56, 92, 60), (300, 56, 92, 60), (700, 200, 92, 54)]
        else:
            ws = [(20, 40, 92, 60), (700, 40, 92, 60), (0, 80, 92, 60)]
        col = [render(base[y:y + h, x:x + w], sc, (x, y), 4) for x, y, w, h in ws]
        cols.append(C.stack(col, gap=6))
    H = max(c.shape[0] for c in cols)
    cols = [np.pad(c, ((0, H - c.shape[0]), (0, 0), (0, 0)), constant_values=255) for c in cols]
    g = np.full((H, 8, 3), 255.0)
    img = np.concatenate(sum([[c, g] for c in cols], [])[:-1], axis=1)
    # full bands at 1x underneath (two per row)
    full = []
    for mode in ("course", "employ", "aura", "value"):
        rgb, m = C.sheet(f"route screens_{mode}.pcx")
        base = np.empty_like(rgb)
        base[:] = R.LAV
        sc = R2.scene("course", st, ish, bsh=bsh) if mode == "course" else scene(mode, st, ish, ms)
        full.append(render(base, sc, (0, 0), 1))
    hh = min(f.shape[0] for f in full)
    full = [f[:hh] for f in full]
    row1 = np.concatenate([full[0], np.full((hh, 6, 3), 255.0), full[1]], axis=1)
    row2 = np.concatenate([full[2], np.full((hh, 6, 3), 255.0), full[3]], axis=1)
    out = C.stack([img, row1, row2], gap=10)
    if out.shape[1] > 1600:
        from PIL import Image
        k = 1600 / out.shape[1]
        out = np.asarray(Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8)).resize(
            (1600, int(out.shape[0] * k)), Image.LANCZOS), float)
    C.save_png(out, os.path.join(OUT5, "_family.png"))
    print("[fix5] _family.png", out.shape, flush=True)


def preview(mode, z=8):
    rgb, base, sc1, sc5 = build(mode)
    t = C.stack([tile3(rgb, base, sc1, sc5, w, z) for w in WINS[mode]])
    p = os.path.join(OUT5, "read", f"preview_{mode}.png")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    C.save_png(t, p)
    print(p, "MAD windows r1 %.2f r5 %.2f" % (mad_win(rgb, base, sc1, WINS[mode]), mad_win(rgb, base, sc5, WINS[mode])))


if __name__ == "__main__":
    for md in sys.argv[1:] or ("employ", "aura", "value"):
        preview(md)
