"""P41 round 7 item 4 (log/roadmap/P41/FIX7_BRIEF.md): "el boton de pausa al parpadear se ve extrano".

While the game is paused, showMainRadialMenu blinks the pause button: on alternate blink phases it draws the hover cut
(3mainLowerLeft.pcx 650,300 36x38) at screen (94, 553) OVER the tray, whose own pause button is the idle state composed
into hud-mode-base (round 4, item 9 of its scene). log/roadmap/P41/portgame/hud_palettes_map.tsv: hud-pause idle (600,300)
is never drawn; hover is drawn by draw16 @650,300 -> 94,553. The port draws a scene clipped to its crop rectangle and moved by
draw point - crop corner, so the hover scene lands at screen = sheet + (-556, +253).

What differed between the two phases (measured on the approved scenes, screen coordinates):
  - phase A (tray item 9): rim circle (107.10, 567.84) r 13.14, body r 12.14, bars x 103 / 111 centre y 567.05; NO socket disc
    (round 4 dropped it so the plate and the cable run under the button); cast shadow circle (106.44, 567.84) r 10.51 a 0.40.
  - phase B (hover scene): rim (107.17, 567.89) r 13.19, body r 12.19; same bars; PLUS its "tray pixels" socket, a lit disc
    filling the whole 31 x 34 cut, painted over the tray (it hides the plate's light, the groove and the first pixels of the
    cable under the button) and its own cast shadow at (105.21, 569.27) a 0.30.
  So on every blink a light halo appeared round the yellow button and vanished round the tan one, the cable start
  flickered, the shadow jumped 1.2 px down-left, and the disc grew 0.05 px. That is the "strange" blink.

The fix: ONE structure. Phase B now draws only what changes with the state, over the tray's own button: the body (the same
circle as the tray's, r 12.14 at (107.10, 567.84)) with the measured yellow light, and the two bars, same shapes and positions,
in the tray's navy. The rim (same navy [40, 48, 80] in both phases), the cast shadow and the plate under it are the tray's
and are not drawn again (drawing the rim twice would thicken its anti-aliased edge on the yellow phase). The idle state is
the approved one with the same geometry (it is not drawn by the game; it stays as it was).
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402
import geom as G  # noqa: E402
from hp import sdf  # noqa: E402

OUT7 = os.path.join(hp.ROOT, "log", "roadmap", "P41", "work7", "hud_palettes")
SC = os.path.join(hp.UIVEC, "scenes", "hud_palettes")
SHEET = "Interface/3mainLowerLeft.pcx"
HOVER_CROP = (650, 300, 36, 38)     # the drawn crop rectangle (hud_palettes_map.tsv)
HOVER_AT = (94, 553)
DX, DY = HOVER_AT[0] - HOVER_CROP[0], HOVER_AT[1] - HOVER_CROP[1]      # sheet -> screen for the hover scene
WIN = (86, 546, 48, 50)             # screen window of the comparison images (x, y, w, h)


def load(eid):
    with open(os.path.join(SC, eid + ".json"), encoding="utf-8") as f:
        return json.load(f)


def tray_pause():
    """The tray's composed pause (hud-mode-base item 9) and its rim / body circles and bars, screen coordinates."""
    it = load("hud-mode-base")["states"]["idle"]["scene"][9]
    names = [L.get("name", "") for L in it["layers"]]
    assert names[3].startswith("left bar") and names[4].startswith("right bar"), names
    return it


def new_hover(old_hover):
    """Phase B on the hover cut, sheet coordinates: body + bars of the tray's button (moved into the hover sheet frame),
    body paint = the hover's measured yellow bevel re-anchored on that circle, bars in the tray's navy."""
    t = tray_pause()
    mv = lambda o: G.moved(json.loads(json.dumps(o)), -DX, -DY)      # screen -> hover sheet
    rim_t, body_t, lb_t, rb_t = t["layers"][1], t["layers"][2], t["layers"][3], t["layers"][4]
    old = old_hover["states"]["hover"]["scene"][0]["layers"]
    body_o = old[3]
    assert body_o["shape"]["t"] == "circle" and "bevel" in body_o["fill"]
    fill = json.loads(json.dumps(body_o["fill"]))
    body_s = mv(body_t["shape"])
    fill["bevel"]["of"] = body_s
    navy = lb_t["fill"]
    layers = [
        {"shape": body_s, "fill": fill,
         "name": "the yellow body: the SAME circle as the tray's pause body (screen (107.10, 567.84) r 12.14), lit by the "
                 "light measured on the hover art; the rim, the cast shadow and the plate under it are the tray's (not redrawn)"},
        {"shape": mv(lb_t["shape"]), "fill": navy, "clip": mv(lb_t["clip"]) if "clip" in lb_t else body_s,
         "name": "left bar, rounded ends: the tray's bar, same shape and place, same navy"},
        {"shape": mv(rb_t["shape"]), "fill": navy, "clip": mv(rb_t["clip"]) if "clip" in rb_t else body_s,
         "name": "right bar, rounded ends: the tray's bar, same shape and place, same navy"},
    ]
    x, y = body_s["c"]
    r = body_s["r"]
    box = [int(np.floor(x - r - 1)), int(np.floor(y - r - 1)), int(np.ceil(x + r + 1)), int(np.ceil(y + r + 1))]
    return {"box": box, "layers": layers}, rim_t


def art_window(blink):
    rgb, mask = hp.BR.sheet(SHEET)
    x, y, w, h = WIN
    a = rgb[y:y + h, x:x + w].astype(float).copy()
    m = mask[y:y + h, x:x + w]
    a[~m] = hp.FLAT
    if blink:
        cx, cy, cw, ch = HOVER_CROP
        s = rgb[cy:cy + ch, cx:cx + cw].astype(float)
        sm = mask[cy:cy + ch, cx:cx + cw]
        ox, oy = HOVER_AT[0] - x, HOVER_AT[1] - y
        sub = a[oy:oy + ch, ox:ox + cw]
        sub[sm] = s[sm]
    return a


def render(items, z, clip_hover=None):
    x, y, w, h = WIN
    img = np.empty((h * z, w * z, 3))
    img[:] = hp.FLAT
    sdf.render(items, img, z, origin=(x, y))
    return img


def port_phase(tray_items, hover_scene, z):
    """What the port shows: the tray, then (blink phase) the hover scene moved to the screen, clipped to its crop."""
    x, y, w, h = WIN
    img = render(tray_items, z)
    if hover_scene is not None:
        cx, cy, cw, ch = HOVER_CROP
        clip = G.rect(HOVER_AT[0], HOVER_AT[1], HOVER_AT[0] + cw, HOVER_AT[1] + ch)
        its = []
        for it in hover_scene:
            it = G.moved(json.loads(json.dumps(it)), DX, DY)
            for L in it["layers"]:
                L["clip"] = G.inter(L["clip"], clip) if "clip" in L else clip
            its.append(it)
        sdf.render(its, img, z, origin=(x, y))
    return img


def up(a, z):
    return np.asarray(Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8)).resize((a.shape[1] * z, a.shape[0] * z), Image.NEAREST), float)


def strip(cols, labels, out):
    gap = 8
    H = max(c.shape[0] for c in cols)
    W = sum(c.shape[1] for c in cols) + gap * (len(cols) - 1)
    big = np.zeros((H + 16, W, 3)) + 30
    x = 0
    for c in cols:
        big[16:16 + c.shape[0], x:x + c.shape[1]] = c
        x += c.shape[1] + gap
    im = Image.fromarray(np.clip(big + 0.5, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    x = 0
    for c, t in zip(cols, labels):
        d.text((x + 3, 2), t, fill=(255, 255, 0))
        x += c.shape[1] + gap
    im.save(out)
    return out


def main():
    os.makedirs(OUT7, exist_ok=True)
    z = 8
    mb = load("hud-mode-base")["states"]["idle"]["scene"]
    tray_items = [it for it in mb if "layers" in it]
    old = load("hud-pause")
    hov, rim_t = new_hover(old)
    new = json.loads(json.dumps(old))
    new["states"]["hover"]["scene"] = [hov]
    new["round7"] = ("item 4: the blink. The hover state (drawn over the tray while paused) is the tray's own pause button "
                     "with only the paint changed: the same body circle and bars as hud-mode-base's composed pause, a yellow "
                     "light, the tray's navy; no socket disc and no second cast shadow (the tray's stay under it).")
    with open(os.path.join(OUT7, "hud-pause.json"), "w", encoding="utf-8") as f:
        json.dump(new, f)
    # the four phases (A, B, A, B) of art, previous port, new port
    artA, artB = up(art_window(False), z), up(art_window(True), z)
    prevA = port_phase(tray_items, None, z)
    prevB = port_phase(tray_items, old["states"]["hover"]["scene"], z)
    newB = port_phase(tray_items, [hov], z)
    rows = [("art", [artA, artB, artA, artB]), ("previous", [prevA, prevB, prevA, prevB]), ("new", [prevA, newB, prevA, newB])]
    gap = 6
    imgs = []
    for nm, cols in rows:
        p = os.path.join(OUT7, f"_row_{nm}.png")
        strip(cols, [f"{nm} A (tan)", f"{nm} B (blink)", f"{nm} A", f"{nm} B"], p)
        imgs.append(np.asarray(Image.open(p), float))
    W = max(i.shape[1] for i in imgs)
    big = np.zeros((sum(i.shape[0] for i in imgs) + gap * 2, W, 3)) + 30
    y = 0
    for i in imgs:
        big[y:y + i.shape[0], :i.shape[1]] = i
        y += i.shape[0] + gap
    Image.fromarray(big.astype(np.uint8)).save(os.path.join(OUT7, "hud-pause.png"))
    for nm, _ in rows:
        os.remove(os.path.join(OUT7, f"_row_{nm}.png"))
    # art LEFT / previous MIDDLE / new RIGHT, states side by side: the brief's layout, at 8x
    cols = [artA, artB, prevA, prevB, prevA, newB]
    strip(cols, ["art idle", "art blink", "prev idle", "prev blink", "new idle", "new blink"],
          os.path.join(OUT7, "hud-pause_states.png"))
    # checks: outside the body circle, phase B must equal phase A (only the paint inside the body changes)
    x, y, w, h = WIN
    jj, ii = np.mgrid[0:h * z, 0:w * z]
    X, Y = (ii + 0.5) / z + x, (jj + 0.5) / z + y
    bc = hov["layers"][0]["shape"]
    bcx, bcy = bc["c"][0] + DX, bc["c"][1] + DY
    dist = np.hypot(X - bcx, Y - bcy)
    outside = dist > bc["r"] + 0.75
    res = {}
    for nm, B in (("previous", prevB), ("new", newB)):
        d = np.abs(B - prevA).max(-1)
        res[nm] = {"max diff outside the body": round(float(d[outside].max()), 2),
                   "pixels (8x) changed outside the body by > 4": int((d[outside] > 4).sum())}
    # overlay of the two phases' edges: rim edge identical?
    res["rim circle (screen)"] = [round(rim_t["shape"]["c"][0], 3), round(rim_t["shape"]["c"][1], 3), round(rim_t["shape"]["r"], 3)]
    res["new body circle (screen)"] = [round(bcx, 3), round(bcy, 3), round(bc["r"], 3)]
    with open(os.path.join(OUT7, "hud-pause_check.json"), "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))


if __name__ == "__main__":
    main()
