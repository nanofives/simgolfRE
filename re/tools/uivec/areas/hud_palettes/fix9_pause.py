"""P41 round 9 (2026-11-27, log/roadmap/P41/INSTALL78_BRIEF.md): the pause button as ONE element, Mariano's note on the round-8 card
r8--hud_palettes--hud-pause: "surroundings gone on button press".

Round 7 (fix7_pause.py) made the blink (hover cut 650,300, drawn over the tray at screen (94, 553)) the tray's own button with a
yellow body and nothing else: no rim, no cast shadow, no tray pixels, because it is drawn OVER the tray, whose button already has
them, and anything drawn twice there changes (a rim drawn twice thickens its anti-aliased edge, a soft shadow drawn twice darkens).
Seen as an element (the round-8 card shows the states on their own) the surround vanished on the yellow state.

Now both states are ONE drawing in the tray's own terms, an alteration of one base (the idle):
  * the surround: the tray pixels the sprite carries round the bubble are drawn as THE TRAY ITSELF there: every layer of
    hud-mode-base (plate, groove, ridge, cable, the other bubbles' shadows and necks, in the tray's paint order) clipped to the
    sprite's socket outline (the approved idle's "tray pixels" polygon, the cut's opaque extent);
  * the button: the tray's composed pause (hud-mode-base item 9: cast shadow, rim, body, bars), unchanged;
  * the yellow state changes only the body's paint: the light measured on the hover art (round 7's).
Drawn over the tray in the game the blink is then the same pixels as the tray outside the body (the same colours composited over
themselves change nothing, at any coverage), and on its own the element keeps rim, shadow and surround in both states.
The idle cut (600,300) is not drawn by the game (log/roadmap/P41/portgame/hud_palettes_map.tsv); its scene changes from the
round-4 "socket lit round the bubble" to the tray's own pixels inside the same outline.

Usage: py -3.12 fix9_pause.py   writes log/roadmap/P41/work9/hud_palettes/hud-pause.{json,png} (+ _scenes.json, _check.json),
results.tsv
"""
import copy
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "..")))
import hp  # noqa: E402
import geom as G  # noqa: E402
from hp import sdf  # noqa: E402
import fix7_pause as F7  # noqa: E402
import states as S  # noqa: E402

OUT = os.path.join(hp.ROOT, "log", "roadmap", "P41", "work9", "hud_palettes")
W7 = os.path.join(hp.ROOT, "log", "roadmap", "P41", "work7", "hud_palettes", "hud-pause.json")
W8 = os.path.join(hp.ROOT, "log", "roadmap", "P41", "work8", "hud_palettes", "hud-pause.json")
IDLE_D = (506, -254)     # screen -> idle sheet (the idle body (613.10, 313.84) is the tray's (107.10, 567.84))
HOVER_D = (-F7.DX, -F7.DY)   # screen -> hover sheet (556, -253)


def moved_items(items, dx, dy):
    out = []
    for it in items:
        o = dict(it)
        b = it["box"]
        o["box"] = [b[0] + dx, b[1] + dy, b[2] + dx, b[3] + dy]
        o["layers"] = [S.move_layer(L, dx, dy, clip=True) for L in it["layers"]]
        out.append(o)
    return out


def element_screen():
    """the pause element in screen coordinates: [surround items (tray layers clipped to the socket)..., the tray's pause item]"""
    mb = F7.load("hud-mode-base")["states"]["idle"]["scene"]
    idle7 = json.load(open(W7, encoding="utf-8"))["states"]["idle"]["scene"]
    sock = idle7[0]["layers"][0]
    assert sock["name"].startswith("the tray pixels"), sock["name"]
    sock_s = S.translate(sock["shape"], -IDLE_D[0], -IDLE_D[1])     # the socket outline, screen
    sb = idle7[0]["box"]
    sbox = [sb[0] - IDLE_D[0], sb[1] - IDLE_D[1], sb[2] - IDLE_D[0], sb[3] - IDLE_D[1]]
    out = []
    for k, it in enumerate(mb):
        if "layers" not in it:
            continue
        if k == 9:
            o = copy.deepcopy(it)
            for L in o["layers"]:
                L["name"] = "button: " + (L.get("name") or ("rim" if L["fill"] == [40.0, 48.0, 80.0] else "body"))
            out.append(o)
            continue
        b = it["box"]
        ib = [max(b[0], sbox[0]), max(b[1], sbox[1]), min(b[2], sbox[2]), min(b[3], sbox[3])]
        if ib[0] >= ib[2] or ib[1] >= ib[3]:
            continue
        ls = []
        for j, L in enumerate(it["layers"]):
            o = copy.deepcopy(L)
            o["clip"] = G.inter(L["clip"], sock_s) if "clip" in L else sock_s
            o["name"] = f"surround (tray item {k}): " + (L.get("name") or L["shape"]["t"])[:60]
            ls.append(o)
        out.append({"box": ib, "layers": ls})
    return out


def body_index(item):
    return [i for i, L in enumerate(item["layers"]) if L["shape"]["t"] == "circle" and isinstance(L["fill"], dict)
            and "bevel" in L["fill"]][-1]


def main():
    os.makedirs(OUT, exist_ok=True)
    scr = element_screen()
    pause_k = [i for i, it in enumerate(scr) if it["layers"][0]["name"].startswith("button:")][0]
    w7 = json.load(open(W7, encoding="utf-8"))
    hov7 = w7["states"]["hover"]["scene"][0]["layers"][0]
    assert hov7["shape"]["t"] == "circle" and "bevel" in hov7["fill"]
    idle = moved_items(scr, *IDLE_D)
    hover = moved_items(scr, *HOVER_D)
    bi = body_index(hover[pause_k])
    bl = hover[pause_k]["layers"][bi]
    fill = copy.deepcopy(hov7["fill"])
    fill["bevel"]["of"] = copy.deepcopy(bl["fill"]["bevel"].get("of", bl["shape"])) if "of" in bl["fill"]["bevel"] else bl["shape"]
    if "of" not in idle[pause_k]["layers"][bi]["fill"]["bevel"]:
        fill["bevel"].pop("of", None)
    bl["fill"] = fill
    bl["name"] = "button: body (the yellow light measured on the hover art, round 7)"
    doc = copy.deepcopy(w7)
    doc["states"]["idle"]["scene"] = idle
    doc["states"]["hover"]["scene"] = hover
    doc["round9"] = ("Mariano round 8: 'surroundings gone on button press'. ONE drawing for both states: the tray's own layers clipped "
                     "to the sprite's socket outline (the surround), the tray's composed pause (shadow, rim, body, bars); the yellow "
                     "state changes only the body's paint. Over the tray in the game the blink changes nothing outside the body.")
    doc.pop("round7", None)
    with open(os.path.join(OUT, "hud-pause_scenes.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f)
    # the element file (states.py derive over the two scenes): must be base + an exact paint-only alteration
    recs = [("idle", None, doc["states"]["idle"]["cut"], (0, 0), idle, {}),
            ("hover", None, doc["states"]["hover"]["cut"], (0, 0), hover, {})]
    elem, rows = S.derive_element("hud_palettes", "hud-pause", recs)
    elem["source"] = "round 9 fix (work9), awaiting the parent's showing"
    with open(os.path.join(OUT, "hud-pause.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(elem, f, separators=(",", ":"))
    for r in rows:
        print("derive:", r["state"], r["status"], r["max_1x"], r["max_2.7x"], "paint", r["paint"], "moves", r["moves"], "off", r["off"],
              "on", r["on"], "origin", elem["states"][r["state"]]["origin"])
    # in-game blink check, as round 7's (fix7_pause.port_phase: the tray, then the hover scene moved to the screen, clipped to
    # its crop), plus the element on its own
    z = 8
    mb = F7.load("hud-mode-base")["states"]["idle"]["scene"]
    tray = [it for it in mb if "layers" in it]
    A = F7.port_phase(tray, None, z)
    B = F7.port_phase(tray, hover, z)
    B7 = F7.port_phase(tray, w7["states"]["hover"]["scene"], z)
    x, y, w, h = F7.WIN
    jj, ii = np.mgrid[0:h * z, 0:w * z]
    X, Y = (ii + 0.5) / z + x, (jj + 0.5) / z + y
    bc = scr[pause_k]["layers"][body_index(scr[pause_k])]["shape"]
    outside = np.hypot(X - bc["c"][0], Y - bc["c"][1]) > bc["r"] + 0.75
    res = {}
    for nm, img in (("round 7 (approved strip)", B7), ("round 9", B)):
        d = np.abs(img - A).max(-1)
        if nm == "round 9":
            yy, xx = np.nonzero(outside & (d > 1))
            if len(xx):
                print("blink diffs at screen x", round(xx.min() / z + x, 2), "..", round(xx.max() / z + x, 2), "y",
                      round(yy.min() / z + y, 2), "..", round(yy.max() / z + y, 2))
        res[nm] = {"max diff outside the body (levels, 8x)": round(float(d[outside].max()), 3),
                   "pixels (8x) changed outside the body by > 1": int((d[outside] > 1).sum())}
    # the element on its own (over the neutral ground): outside the body, hover minus idle (hover moved by its origin)
    o = elem["states"]["hover"]["origin"]
    cut = doc["states"]["idle"]["cut"]
    rect = [cut[0] - 3, cut[1] - 3, cut[2] + 6, cut[3] + 6]
    gi = S.render(idle, rect, z, np.array([198.0, 198.0, 214.0]))
    gh = S.render(S._moved_items(hover, o), rect, z, np.array([198.0, 198.0, 214.0]))
    H, Wd = gi.shape[:2]
    jj, ii = np.mgrid[0:H, 0:Wd]
    X, Y = (ii + 0.5) / z + rect[0], (jj + 0.5) / z + rect[1]
    bci = S.translate(bc, *IDLE_D)
    out2 = np.hypot(X - bci["c"][0], Y - bci["c"][1]) > bci["r"] + 0.75
    d = np.abs(gh - gi).max(-1)
    res["element alone: hover vs idle outside the body (levels, 8x)"] = round(float(d[out2].max()), 3)
    with open(os.path.join(OUT, "hud-pause_check.json"), "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))
    # image: in place (art A / B, round 8 unified, round 9) and the element on its own (round 8 unified vs round 9)
    artA, artB = F7.up(F7.art_window(False), z), F7.up(F7.art_window(True), z)
    e8 = json.load(open(W8, encoding="utf-8"))
    v8 = e8["variants"][""]
    r8i, r8h = S.apply(v8, "idle"), S._moved_items(S.apply(v8, "hover"), [0, 0])
    o8 = e8["states"]["hover"]["origin"]
    g8i = S.render(r8i, rect, z, np.array([198.0, 198.0, 214.0]))
    g8h = S.render(S.apply(v8, "hover"), [rect[0] + 0 * o8[0], rect[1], rect[2], rect[3]], z, np.array([198.0, 198.0, 214.0]))
    cols = [artA, artB, A, B7, A, B]
    labels = ["art tan", "art blink", "tray tan", "r7/r8 blink", "tray tan", "r9 blink"]
    F7.strip(cols, labels, os.path.join(OUT, "_inplace.png"))
    F7.strip([g8i, g8h, gi, gh], ["r8 idle (alone)", "r8 hover (alone)", "r9 idle (alone)", "r9 hover (alone)"],
             os.path.join(OUT, "_alone.png"))
    from PIL import Image
    a = np.asarray(Image.open(os.path.join(OUT, "_inplace.png")), float)
    b = np.asarray(Image.open(os.path.join(OUT, "_alone.png")), float)
    Wm = max(a.shape[1], b.shape[1])
    big = np.zeros((a.shape[0] + b.shape[0] + 8, Wm, 3)) + 30
    big[:a.shape[0], :a.shape[1]] = a
    big[a.shape[0] + 8:, :b.shape[1]] = b
    Image.fromarray(big.astype(np.uint8)).save(os.path.join(OUT, "hud-pause.png"))
    os.remove(os.path.join(OUT, "_inplace.png"))
    os.remove(os.path.join(OUT, "_alone.png"))
    with open(os.path.join(OUT, "results.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("id\tstate\tvariant\tbase\twas\tstatus\tmax_vs_approved_1x\tmax_vs_approved_2.7x\tchanged\tunsure\n")
        for r in rows:
            f.write(f"hud-pause\t{r['state']}\t-\tidle\t{'base' if r['state'] == 'idle' else 'unchanged (round 7 hover)'}\t"
                    f"{r['status']}\t-\t-\t"
                    "one drawing for both states: surround = the tray's own layers clipped to the socket outline, button = the "
                    "tray's composed pause; the yellow state = the idle with the body's paint only (derive: "
                    f"{r['status']}, paint {r['paint']}, moves '{r['moves']}', off '{r['off']}', on '{r['on']}'); in game the blink "
                    f"outside the body: {res['round 9']['max diff outside the body (levels, 8x)']} levels (round 7: "
                    f"{res['round 7 (approved strip)']['max diff outside the body (levels, 8x)']})\t"
                    "the idle cut's surround is now the tray's own pixels inside the same outline (it was round 4's lit socket); the "
                    "game never draws the idle cut\n")


if __name__ == "__main__":
    main()
