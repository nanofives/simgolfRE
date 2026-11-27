"""uivec -> the port (P41 port pilot and P41W, log/roadmap/P41/PORT_PILOT_BRIEF.md, PORT_WIDE_BRIEF.md): approved scenes as
generated includes (numbers only).

    py -3.12 re/tools/uivec/port_export.py [--check]                  the title buttons -> port/src/sim/uivec_title.inc
    py -3.12 re/tools/uivec/port_export.py --wide <screen>|all [--check]   a wide screen -> port/src/sim/uivec_wide_<screen>.inc

THE TITLE BUTTONS (uivec_title.inc): title.scene(title.measure(), hover) of each of the six title buttons, one scene per button and
state, keyed by the button's index in gfx.cpp's kTitleButtons (load 0, play 1, tutorial 2, champ 3, sandbox 4, exit 6; index 5 is
the centre picture, not a button, and stays art).

A WIDE SCREEN (uivec_wide_<screen>.inc, re/tools/uivec/scenes/wide/<screen>-wide.json, method re/tools/uivec/wide.py): the scene's
draw list split into the STATIC BACKDROP (rendered once per screen and window size by port/src/sim/gfx_uvwide.cpp) and the
OVERLAYS (entries the game draws itself, state by state: the title's pads and buttons are uivec_title.inc's scenes, drawn on the
menu's Image_draw of each cut; the logo is a picture drawn on the menu's Image_draw of its cut). The backdrop is a list of OPS in
paint order: an items op (plain sdf items), a blobs op (the ground's soft blobs as a compact table: centre, radii, angle, softness,
colour and alpha as integers in the units the scene rounds them to, so the values are exact), a picture op (art lifted from the
game's sheet at run time with its matte; no art bytes here). The leading ground ops (the base and the blobs) may be rendered at a
lower scale and upsampled (gfx_uvwide.cpp kGroundScaleMax); every other op at the window's scale.

Everything the C++ renderer (port/src/sim/gfx_uivec_eval.h) should not redo is resolved here, exactly as sdf.py computes it:
  * "smooth" corner cutting of polygons and strokes is applied (the points written are the cut ones);
  * "qstroke", "arrow" and "lathe" become the polygon sdf.py builds for them; "move" is applied to every coordinate under it;
  * "dimpled" becomes its boolean tree of plain shapes: the ball minus the union of its dimples (rounded squares or ellipses),
    and the outline ring (the ball minus the circle "rim" inside it, cut by the half plane of rim_l when it is set);
  * a stroke's square ends are written as the number of segments each end's plane cuts (sdf.py's "near" sets: all of them for
    "butt", the ones within two widths of the end for "flat", none for "round" or a closed stroke);
  * an ellipse's and a rounded rectangle's rotation as its cosine and sine, an arc's two end points.
The kinds left for the port: circle, ellipse, poly (even-odd rings), seg, half, union, inter, sub, grow, outline, stroke, rrect, arc.
Paints: flat, linear, radial, and bevel (on a circle or an ellipse, or over another such shape with "of": sdf.py's analytic normal;
on any other shape np.gradient's grid normal over the item's pixel grid, flagged g[0] = 1). Layer keys: shape, fill, alpha, soft, clip, falloff (no fx: refused).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import title  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(ROOT, "port", "src", "sim", "uivec_title.inc")
WIDE_DIR = os.path.join(HERE, "scenes", "wide")
# the title buttons in the order of gfx.cpp kTitleButtons (index 5, the centre picture, is not a button)
BUTTONS = [("load", 0), ("play", 1), ("tutorial", 2), ("champ", 3), ("sandbox", 4), ("exit", 6)]
# kTitleButtons' unselected SpriteRef slot of each button (the hover cut is the next slot)
SLOTS = {"load": 0, "play": 2, "tutorial": 4, "champ": 6, "sandbox": 8, "exit": 10}
KEY_KINDS = {"title": 1, "local": 2, "cgb": 3}
KINDS = ["circle", "ellipse", "poly", "seg", "half", "union", "inter", "sub", "grow", "outline", "stroke", "rrect", "arc",
         # P41G (port_game.py): area shape kinds (sdf.EXTRA), evaluated natively by gfx_uivec_eval.h with the area module's formula
         "hud_panels.limb", "hud_panels.rrect4", "popups.sinter", "rm.smin",
         "marks.superellipse", "marks.pline", "marks.vline",
         # P41S (states_port.py): a moved shape, p = dx, dy over one kid (a state's rigid move of a base layer)
         "uv.move"]
# P41G: area kinds that are a plain polygon built by their area module (port_game.py registers the builder): expanded to "poly"
AREA_EXPAND = {}
FX = {"edge_light": 0, "inner_shadow": 1, "tone": 2}
CAPS = {"butt": 0, "flat": 1, "round": 2}
MATTES = {"key": 0, "opaque": 1, "ground_diff": 2}

# Per wide screen: the background sheet the game blits full-screen (the hook's key: the image's file name, lower case) and the
# entries of the draw list the game draws itself (name -> how), which the static backdrop leaves out.
#   "buttons": the title's six pads and buttons (uivec_title.inc scenes on the menu's button cuts)
#   "picture:<slot>": a picture drawn on the menu's Image_draw of g_spriteRef_53b7e0 cut <slot>
WIDE = {
    "title": {"bg": "interface/titlebase.pcx", "overlays": {"pads and buttons": "buttons", "SimGolf logo": "picture:13"}},
    # "draws" (per screen): overlays keyed to the game's own Image_draw of a cut: {"name", "key": (kind, offset), "at": the draw
    # point the scene is placed for, and "scene": ("ref", <approved scene id>, <state>) | ("diffpy", <pad>, <hover>) | "restore"
    # (the backdrop's 1x pixels put back over the cut's drawn rectangle: a plain cut repainting an idle look the backdrop holds)}.
    # "origin": the scene's own point that lands on the draw point (default "at": a scene in screen coordinates drawn in place).
    # Key kinds: "title" g_spriteRef_53b7e0 + offset, "local" gfx_local_key(offset), "cgb" g_sprites59df20 + offset.
    "diff": {"bg": "interface/titleseldiffunsel.pcx", "overlays": {},
             # selectDifficultyScreen.redraw (gfx_boot_screens.cpp): the highlighted choice's TitleSelDiffMO cut k (frame object
             # gfx_local_key(0x71000 + 0x2c k)) drawn at its table point; the idle look is the backdrop (redrawn every pass)
             "draws": [{"name": f"{n} hover", "scene": ("diffpy", n, True), "key": ("local", 0x71000 + 0x2c * k), "at": at}
                       for k, (n, at) in enumerate([("easy", (0xc1, 0x20)), ("medium", (0x96, 0xa1)), ("hard", (0xa1, 0x140)),
                                                    ("expert", (0xc8, 0x1b4))])]
                      + [{"name": "back hover", "scene": ("ref", "diff-back-button", "hover"), "key": ("local", 0x71000 + 0x2c * 4),
                          "at": (0x2dc, 0x214), "origin": (0x2dc, 0x214)}]},
    "load": {"bg": "interface/title_loadgame.pcx", "overlays": {}, "draws": None},
    "theme": {"bg": "interface/title_themepacks.pcx", "overlays": {}, "draws": None},
    "world": {"bg": "interface/worldbase.pcx", "overlays": {}, "draws": None},
    "pro": {"bg": "interface/title_pickapro.pcx", "overlays": {}, "draws": None},
    "editor": {"bg": "interface/custgolfbckgrnd.pcx", "overlays": {}, "draws": None},
    "editor_male": {"bg": "interface/custglfbckmale.pcx", "overlays": {}, "draws": None},
    # P41G round 6 (log/roadmap/P41/work6/reports_maps/results.tsv): in-game full-screen reports drawn wide. "bg" is the sheet whose
    # Image_draw at (0, 0) starts the screen (gfx_ui.cpp ui_wide_report): the routing map's top per mode (its bottom sheet, drawn just
    # before, is restored from the backdrop), Buy Land's one full-screen sheet. "file": the scene file (one per route mode); "refs":
    # the approved element scenes the wide scene places (scenes/<area>/<id>.json, first state)
    "route": {"bg": "interface/infoscreens/route screens_course.pcx", "file": "route-wide.json", "overlays": {}, "draws": [],
              "refs": ("reports_maps", ["route-top-course", "route-bottom"])},
    "route_employ": {"bg": "interface/infoscreens/route screens_employ.pcx", "file": "route-wide-employ.json", "overlays": {},
                     "draws": [], "refs": ("reports_maps", ["route-top-employ", "route-bottom"])},
    "route_aura": {"bg": "interface/infoscreens/route screens_aura.pcx", "file": "route-wide-aura.json", "overlays": {},
                   "draws": [], "refs": ("reports_maps", ["route-top-aura", "route-bottom"])},
    "route_value": {"bg": "interface/infoscreens/route screens_value.pcx", "file": "route-wide-value.json", "overlays": {},
                    "draws": [], "refs": ("reports_maps", ["route-top-value", "route-bottom"])},
    "buyland": {"bg": "interface/infoscreens/buy_land.pcx", "file": "buyland-wide.json", "overlays": {}, "draws": [],
                "refs": ("reports_maps", ["buyland-backdrop"])},
}


def _browser_draws(base):
    """loadGameBrowserScreen's cuts (saveload.cpp browser_setup; pick a pro: the same layout, menu.cpp): element k of the frame
    objects gfx_local_key(base + 0x2c k), each drawn at its own place: plain 0..4 (the idle look the backdrop holds: restored),
    hover 5..9, disabled 10 (OK) and 14 (Delete), the approved load-* scenes in screen coordinates"""
    btn = [("ok", "load-ok-button", (0x258, 0x208)), ("back", "load-back-button", (0x2e4, 0x212)),
           ("up", "load-scroll-up", (0x30a, 0x64)), ("down", "load-scroll-down", (0x30a, 0x17c)),
           ("delete", "load-delete-button", (0x29e, 0x208))]
    out = []
    for k, (n, sc, at) in enumerate(btn):
        out.append({"name": f"{n} plain", "scene": "restore", "key": ("local", base + 0x2c * k), "at": at})
        out.append({"name": f"{n} hover", "scene": ("ref", sc, "hover"), "key": ("local", base + 0x2c * (5 + k)), "at": at})
    out.append({"name": "ok disabled", "scene": ("ref", "load-ok-button", "disabled"), "key": ("local", base + 0x2c * 10),
                "at": (0x258, 0x208)})
    out.append({"name": "delete disabled", "scene": ("ref", "load-delete-button", "disabled"), "key": ("local", base + 0x2c * 14),
                "at": (0x29e, 0x208)})
    return out


def _theme_draws():
    """showThemePackSelect (gfx_boot_screens.cpp): the buttons' plain B[k] (gfx_local_key(0x72400 + 0x2c k), restored) and hover A[k]
    (0x72000 + 0x2c k) at the button points; the row bar A7 and the cells A4 / A5 / A6 from the MO sheet (scenes in sheet
    coordinates: origin = the cut's corner in the sheet)"""
    pts = [(0x245, 0x208), (0x2d6, 0x215), (0x301, 0x51), (0x301, 0x1d7)]
    scs = ["theme-ok-button", "theme-back-button", "theme-scroll-up", "theme-scroll-down"]
    out = []
    for k, (at, sc) in enumerate(zip(pts, scs)):
        out.append({"name": f"{sc} plain", "scene": "restore", "key": ("local", 0x72400 + 0x2c * k), "at": at})
        out.append({"name": f"{sc} hover", "scene": ("ref", sc, "hover"), "key": ("local", 0x72000 + 0x2c * k), "at": at})
    for k, st, org in ((4, "plain", (1, 1)), (5, "lamp lit", (0x59, 1)), (6, "lamp plain", (0xb1, 1))):
        out.append({"name": f"cell {st}", "scene": ("ref", "theme-cell", st), "key": ("local", 0x72000 + 0x2c * k), "at": org,
                    "origin": org})
    out.append({"name": "row bar", "scene": ("ref", "theme-row-bar", "idle"), "key": ("local", 0x72000 + 0x2c * 7), "at": (1, 0x21),
                "origin": (1, 0x21)})
    return out


def _world_draws():
    """courseActionMenu (saveload.cpp, gfx.cpp b_cam_setup): WorldButton cuts arr(i) = gfx_local_key(0x2c i), scenes in the sheet's
    coordinates (origin = the cut's corner): refresh 0 / 1, save 2 / 3, load 4 / 5, back hover 7"""
    cuts = {0: (0xfa, 0x198), 1: (0x2d4, 0x198), 2: (0x226, 0x1ca), 3: (0x190, 0x1ca), 4: (0x226, 0x198), 5: (0x190, 0x198),
            7: (0x2dc, 0x214)}
    sc = {0: ("world-refresh", "idle"), 1: ("world-refresh", "hover"), 2: ("world-save", "idle"), 3: ("world-save", "hover"),
          4: ("world-load", "idle"), 5: ("world-load", "hover"), 7: ("world-back", "hover")}
    return [{"name": f"{sc[i][0]} {sc[i][1]}", "scene": ("ref",) + sc[i], "key": ("local", 0x2c * i), "at": cuts[i], "origin": cuts[i]}
            for i in sorted(cuts)]


def _editor_draws():
    """showCharacterEditor (chareditor.cpp): CGButtons cells g_sprites59df20 + 0x2c c drawn by Surface_drawScaled (hover 2 ctl, the
    set bits 2k + 1 / 7 + 2k) and the oval (cell 40, Image_draw); scenes in the sheet's coordinates (origin = the cut's corner,
    gfx_hud.cpp's CGbuttons cuts). The approved pills: cell 0 / 1 (tall, idle / selected), 4 / 5 (tall, the third), 6 / 7 (pill),
    14 / 15 (the last pill, its tail); the cells between them have the same cut size and look, drawn from the first pill's scene
    at their own corner (an overlay's origin is its scene's own cut corner, so the scene lands on the drawn cell)."""
    out = []
    tall = {0: ("idle", (0, 300)), 1: ("selected", (150, 300)), 2: ("idle", (0, 350)), 3: ("selected", (150, 350)),
            4: ("idle 3", (0, 400)), 5: ("selected 3", (150, 400))}
    for c, (st, org) in tall.items():
        sc_org = {"idle": (0, 300), "selected": (150, 300), "idle 3": (0, 400), "selected 3": (150, 400)}[st]
        out.append({"name": f"tall pill cell {c}", "scene": ("ref", "editor-pill-tall", st), "key": ("cgb", 0x2c * c), "at": org,
                    "origin": sc_org})
    for c in range(6, 16):
        k = (c - 6) // 2
        sel = c % 2 == 1
        org = (150 if sel else 0, 0x32 + 0x32 * k)
        st = ("tail selected" if sel else "tail idle") if c >= 14 else ("selected" if sel else "idle")
        sc_org = (150 if sel else 0, 250 if c >= 14 else 50)
        out.append({"name": f"pill cell {c}", "scene": ("ref", "editor-pill", st), "key": ("cgb", 0x2c * c), "at": org,
                    "origin": sc_org})
    for i, n in enumerate(["outfit", "family", "head", "shirt", "pants", "hair", "body", "gender"]):
        for sel in (0, 1):
            org = (300 + 50 * sel, 50 * i)
            out.append({"name": f"{n} {'selected' if sel else 'idle'}", "scene": ("ref", f"editor-{n}", "selected" if sel else "idle"),
                        "key": ("cgb", 0x2c * (20 + 2 * i + sel)), "at": org, "origin": org})
    for c, n, org in ((16, "load", (0, 0)), (18, "save", (0x64, 0)), (36, "reset", (0x1f4, 0)), (38, "ok", (0x1f4, 0x64))):
        out.append({"name": f"{n} hover", "scene": ("ref", f"editor-{n}", f"hover (CGButtons, yellow)"), "key": ("cgb", 0x2c * c),
                    "at": org, "origin": org})
    out.append({"name": "oval", "scene": ("ref", "editor-oval", "idle"), "key": ("cgb", 0x2c * 40), "at": (0x12c, 0x1f4),
                "origin": (0x12c, 0x1f4)})
    return out


WIDE["load"]["draws"] = _browser_draws(0x4000)
WIDE["pro"]["draws"] = _browser_draws(0x73000)
WIDE["theme"]["draws"] = _theme_draws()
WIDE["world"]["draws"] = _world_draws()
WIDE["editor"]["draws"] = WIDE["editor_male"]["draws"] = _editor_draws()


def _smooth_closed(rings, n):
    for _ in range(int(n)):
        rings = [[[a[0] * (1 - f) + b[0] * f, a[1] * (1 - f) + b[1] * f]
                  for a, b in zip(r, r[1:] + r[:1]) for f in (0.25, 0.75)] for r in rings]
    return rings


def _smooth_open(pts, n):
    for _ in range(int(n)):
        pts = [pts[0]] + [[a[0] * (1 - f) + b[0] * f, a[1] * (1 - f) + b[1] * f]
                          for a, b in zip(pts, pts[1:]) for f in (0.25, 0.75)] + [pts[-1]]
    return pts


def expand(S):
    """S with only the port's kinds (sdf.py's own constructions, same arithmetic)."""
    t = S["t"]
    if t in ("circle", "ellipse", "seg", "half", "rrect", "arc", "hud_panels.rrect4"):
        return dict(S)
    if t == "hud_panels.limb":   # figs.py _limb: its smooth rounds applied here (ends kept, widths interpolated)
        pts = [list(map(float, p)) for p in S["pts"]]
        for _ in range(int(S.get("smooth", 0))):
            out = [pts[0]]
            for a, b in zip(pts, pts[1:]):
                for f in (0.25, 0.75):
                    out.append([a[k] * (1 - f) + b[k] * f for k in range(3)])
            out.append(pts[-1])
            pts = out
        return {"t": t, "pts": pts, "cap": S.get("cap", "round")}
    if t == "popups.sinter":
        return {"t": t, "of": [expand(q) for q in S["of"]], "r": S["r"]}
    if t == "rm.smin":
        return {"t": t, "of": [expand(q) for q in S["of"]], "k": S["k"]}
    if t == "marks.superellipse":
        return dict(S)
    if t == "marks.pline":   # common.py _pline: the smooth rounds applied here (ends kept)
        return {"t": t, "pts": _smooth_open([list(map(float, p)) for p in S["pts"]], S.get("smooth", 0)), "w": S["w"],
                "w1": S.get("w1", S["w"]), "cap": S.get("cap", "butt")}
    if t == "marks.vline":   # fix2.py _vline: the Catmull-Rom points and their widths resolved here (AREA_EXPAND["marks.vline"])
        return AREA_EXPAND["marks.vline"](S)
    if t in AREA_EXPAND:
        return expand(AREA_EXPAND[t](S))
    if t == "poly":
        return {"t": "poly", "rings": _smooth_closed(S["rings"], S.get("smooth", 0))}
    if t == "stroke":
        pts = _smooth_open(S["pts"], S.get("smooth", 0))
        out = {"t": "stroke", "pts": pts, "w": S["w"], "cap": S.get("cap", "butt"), "closed": bool(S.get("closed"))}
        return out
    if t == "arrow":
        a, b = S["a"], S["b"]
        ux, uy = b[0] - a[0], b[1] - a[1]
        n = max(math.hypot(ux, uy), 1e-9)
        ux, uy = ux / n, uy / n
        px, py = -uy, ux
        bx, by = b[0] - S["hl"] * ux, b[1] - S["hl"] * uy
        w, hw = S["w"] / 2, S["hw"] / 2
        ring = [[a[0] + px * w, a[1] + py * w], [bx + px * w, by + py * w], [bx + px * hw, by + py * hw], [b[0], b[1]],
                [bx - px * hw, by - py * hw], [bx - px * w, by - py * w], [a[0] - px * w, a[1] - py * w]]
        return {"t": "poly", "rings": [ring]}
    if t == "qstroke":
        (ax, ay), (bx, by), (cx, cy) = S["p"]
        left, right = [], []
        for k in range(17):
            u = k / 16.0
            qx = (1 - u) ** 2 * ax + 2 * u * (1 - u) * bx + u * u * cx
            qy = (1 - u) ** 2 * ay + 2 * u * (1 - u) * by + u * u * cy
            tx = 2 * (1 - u) * (bx - ax) + 2 * u * (cx - bx)
            ty = 2 * (1 - u) * (by - ay) + 2 * u * (cy - by)
            n = max(math.hypot(tx, ty), 1e-9)
            h = (S["w"][0] * (1 - u) + S["w"][1] * u) / 2
            left.append([qx - ty / n * h, qy + tx / n * h])
            right.append([qx + ty / n * h, qy - tx / n * h])
        return {"t": "poly", "rings": [left + right[::-1]]}
    if t == "lathe":
        prof = S["prof"]
        ring = [[S["x"] - h, yy] for yy, h in prof] + [[S["x"] + h, yy] for yy, h in prof[::-1]]
        return {"t": "poly", "rings": _smooth_closed([ring], S.get("smooth", 0))}
    if t == "move":
        return translate(expand(S["of"]), S["d"][0], S["d"][1])
    if t in ("union", "inter"):
        return {"t": t, "of": [expand(q) for q in S["of"]]}
    if t == "sub":
        return {"t": "sub", "a": expand(S["a"]), "b": expand(S["b"])}
    if t == "grow":
        return {"t": "grow", "of": expand(S["of"]), "d": S["d"]}
    if t == "outline":
        return {"t": "outline", "of": expand(S["of"]), "w": S["w"]}
    if t == "dimpled":
        return dimpled(S)
    raise ValueError(f"port_export: shape kind {t!r} is not exported")


def dimpled(S):
    """sdf.py's dimpled ball as plain shapes: d = max(ball, -min(holes)); rim = max(ball, -(ball + rim)) [, max(.., (facing - rim_l) R)];
    the result min(d, rim)."""
    cx, cy = S["c"]
    R, sp = S["r"], S["spacing"]
    a = math.radians(S["turn"])
    L = [S["light"][0], S["light"][1], S["lz"]]
    ln = max(math.sqrt(L[0] ** 2 + L[1] ** 2 + L[2] ** 2), 1e-9)
    L = [v / ln for v in L]
    ball = {"t": "circle", "c": [cx, cy], "r": R}
    holes = []
    n = int(R / max(sp, 1.0)) + 2
    for j in range(-n, n + 1):
        for i in range(-n, n + 1):
            u = (i + 0.5 * (j % 2)) * sp + S["phase"][0]
            v = j * sp * 0.866 + S["phase"][1]
            px, py = u * math.cos(a) - v * math.sin(a), u * math.sin(a) + v * math.cos(a)
            rho = math.hypot(px, py) / R
            if rho >= 0.98:
                continue
            nz = math.sqrt(1 - rho * rho)
            lit = px / R * L[0] + py / R * L[1] + nz * L[2]
            rd = min(max(S["k"] * (lit - S["l0"]), 0.0), S.get("cap", 0.8)) * sp
            if rd < 0.3:
                continue
            if S.get("cell") == "square":
                h = rd * max(nz, 0.5) ** 0.5
                holes.append({"t": "rrect", "c": [cx + px, cy + py], "half": [h, h], "r": S.get("round", 0.45) * h, "rot": S["turn"]})
                continue
            holes.append({"t": "ellipse", "c": [cx + px, cy + py], "r": [rd * max(nz, 0.35), rd],
                          "rot": float(math.degrees(math.atan2(py, px)))})
    d = {"t": "sub", "a": ball, "b": {"t": "union", "of": holes}} if holes else ball
    rim = {"t": "sub", "a": ball, "b": {"t": "circle", "c": [cx, cy], "r": R - S["rim"]}}
    if "rim_l" in S:
        n2 = max(math.hypot(L[0], L[1]), 1e-9)
        nx, ny = L[0] / n2, L[1] / n2
        # (facing - rim_l) R = ((x - cx) Lx + (y - cy) Ly) / n2 - rim_l R: the half plane through c + n rim_l R along n
        rim = {"t": "inter", "of": [rim, {"t": "half", "p": [cx + nx * S["rim_l"] * R, cy + ny * S["rim_l"] * R], "n": [nx, ny]}]}
    return {"t": "union", "of": [d, rim]}


def translate(S, dx, dy):
    t = S["t"]
    mv = lambda p: [p[0] + dx, p[1] + dy]  # noqa: E731
    if t in ("circle", "ellipse", "rrect", "arc"):
        return dict(S, c=mv(S["c"]))
    if t == "poly":
        return dict(S, rings=[[mv(p) for p in r] for r in S["rings"]])
    if t == "stroke":
        return dict(S, pts=[mv(p) for p in S["pts"]])
    if t == "seg":
        return dict(S, a=mv(S["a"]), b=mv(S["b"]))
    if t == "half":
        return dict(S, p=mv(S["p"]))
    if t == "hud_panels.limb":
        return dict(S, pts=[[p[0] + dx, p[1] + dy, p[2]] for p in S["pts"]])
    if t == "marks.superellipse":
        return dict(S, c=mv(S["c"]))
    if t == "marks.pline":
        return dict(S, pts=[mv(p) for p in S["pts"]])
    if t == "marks.vline":
        return dict(S, pts=[[p[0] + dx, p[1] + dy, p[2]] for p in S["pts"]])
    if t == "hud_panels.rrect4":
        b = S["box"]
        return dict(S, box=[b[0] + dx, b[1] + dy, b[2] + dx, b[3] + dy])
    if t in ("union", "inter", "popups.sinter", "rm.smin"):
        return dict(S, of=[translate(q, dx, dy) for q in S["of"]])
    if t == "sub":
        return dict(S, a=translate(S["a"], dx, dy), b=translate(S["b"], dx, dy))
    if t in ("grow", "outline"):
        return dict(S, of=translate(S["of"], dx, dy))
    raise ValueError(t)


def stroke_near(S):
    """(segments cut by the first end's plane counted from the start, by the last end's plane counted from the end)"""
    pts, cap = S["pts"], S["cap"]
    segs = list(zip(pts, pts[1:])) + ([(pts[-1], pts[0])] if S["closed"] else [])
    if cap not in ("butt", "flat") or S["closed"]:
        return 0, 0
    if cap == "butt":
        return len(segs), len(segs)
    ln = [math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in segs]
    out = []
    for order in (range(len(segs)), range(len(segs) - 1, -1, -1)):
        k, run = 0, 0.0
        for i in order:
            k += 1
            run += ln[i]
            if run >= 2.0 * S["w"]:
                break
        out.append(k)
    return out[0], out[1]


def _rot_cs(deg):
    a = math.radians(deg)
    return math.cos(a), math.sin(a)


class Tables:
    def __init__(self):
        self.nodes, self.kids, self.rings, self.pts = [], [], [], []
        self.paints, self.stops, self.coef, self.layers, self.items, self.scenes = [], [], [], [], [], []
        self.fx = []   # P41G: layer effects (UvFx rows)

    def node(self, S):
        t = S["t"]
        k = KINDS.index(t)
        p, a, n, ns, ne, cap, closed = [], 0, 0, 0, 0, 0, 0
        if t == "circle":
            p = [S["c"][0], S["c"][1], S["r"]]
        elif t == "ellipse":
            c, s = _rot_cs(S.get("rot", 0.0))
            p = [S["c"][0], S["c"][1], S["r"][0], S["r"][1], S.get("rot", 0.0), c, s]
        elif t == "rrect":
            c, s = _rot_cs(S.get("rot", 0.0))
            p = [S["c"][0], S["c"][1], S["half"][0], S["half"][1], S.get("r", 0.0), S.get("rot", 0.0), c, s]
        elif t == "seg":
            p = [S["a"][0], S["a"][1], S["b"][0], S["b"][1], S["w"]]
        elif t == "half":
            p = [S["p"][0], S["p"][1], S["n"][0], S["n"][1]]
        elif t == "arc":
            ends = []
            for ang in (S["a0"], S["a0"] + S["sweep"]):
                ends += [S["c"][0] + S["r"] * math.cos(math.radians(ang)), S["c"][1] + S["r"] * math.sin(math.radians(ang))]
            p = [S["c"][0], S["c"][1], S["r"], S["a0"], S["sweep"], S["w"]] + ends
        elif t == "poly":
            a, n = len(self.rings), len(S["rings"])
            for r in S["rings"]:
                self.rings.append((len(self.pts), len(r)))
                self.pts.extend(r)
        elif t == "stroke":
            a, n = len(self.pts), len(S["pts"])
            self.pts.extend(S["pts"])
            p = [S["w"]]
            ns, ne = stroke_near(S)
            cap, closed = CAPS[S["cap"]], int(S["closed"])
        elif t == "hud_panels.limb":
            # the n points (x, y) then their n widths as (w, 0) pairs; cap 1 = "butt"
            a, n = len(self.pts), len(S["pts"])
            self.pts.extend([[q[0], q[1]] for q in S["pts"]])
            self.pts.extend([[q[2], 0.0] for q in S["pts"]])
            cap = 1 if S.get("cap") == "butt" else 0
        elif t == "hud_panels.rrect4":
            p = list(S["box"]) + list(S["r"])
        elif t == "marks.superellipse":
            p = [S["c"][0], S["c"][1], S["r"][0], S["r"][1], S["n"]]
        elif t == "marks.pline":   # the points; p = w (start), w1 (end); cap 1 = "butt"
            a, n = len(self.pts), len(S["pts"])
            self.pts.extend(S["pts"])
            p = [S["w"], S["w1"]]
            cap = 1 if S["cap"] == "butt" else 0
        elif t == "marks.vline":   # the spline's points then their widths as (w, 0) pairs (as limb)
            a, n = len(self.pts), len(S["pts"])
            self.pts.extend([[q[0], q[1]] for q in S["pts"]])
            self.pts.extend([[q[2], 0.0] for q in S["pts"]])
        else:
            ch = S["of"] if t in ("union", "inter", "popups.sinter", "rm.smin") else [S["a"], S["b"]] if t == "sub" else [S["of"]]
            ids = [self.node(q) for q in ch]
            a, n = len(self.kids), len(ids)
            self.kids.extend(ids)
            if t == "grow":
                p = [S["d"]]
            elif t == "outline":
                p = [S["w"]]
            elif t == "popups.sinter":
                p = [S["r"]]
            elif t == "rm.smin":
                p = [S["k"]]
        assert len(p) <= 10
        self.nodes.append((k, cap, closed, a, n, ns, ne, p))
        return len(self.nodes) - 1

    def _stops(self, stops):
        sa, ca = len(self.stops), len(self.coef)
        for t, c in stops:
            self.stops.append(t)
            self.coef.extend(c)
        return sa, len(stops), ca

    def paint(self, P, S):
        if isinstance(P, dict):
            if "linear" in P:
                l = P["linear"]
                sa, sn, ca = self._stops(l["stops"])
                self.paints.append((2, [0.0, 0.0, 0.0], 0.0, sa, sn, ca, -1, [l["a"][0], l["a"][1], l["b"][0], l["b"][1]]))
                return len(self.paints) - 1
            if "radial" in P:
                r = P["radial"]
                sa, sn, ca = self._stops(r["stops"])
                self.paints.append((3, [0.0, 0.0, 0.0], 0.0, sa, sn, ca, -1, [r["c"][0], r["c"][1], r["r"], 0.0]))
                return len(self.paints) - 1
            if "bevel" not in P:
                raise ValueError(f"port_export: paint {list(P)} is not exported")
            b = P["bevel"]
            of = -1
            S2 = expand(b["of"]) if b.get("of") else S
            if b.get("of"):
                of = self.node(S2)
            # sdf.py normal(): analytic on a circle or an ellipse, else np.gradient's grid normal (gfx_uivec_eval.h grid_normal)
            grid = 0.0 if S2["t"] in ("circle", "ellipse") else 1.0
            sa = len(self.stops)
            self.stops.extend(b["stops"])
            ca = len(self.coef)
            for row in b["coef"]:
                for term in row:
                    self.coef.extend(term)
            assert len(b["coef"]) == len(b["stops"]) and all(len(r) == 5 for r in b["coef"])
            self.paints.append((1, [0.0, 0.0, 0.0], b["L"], sa, len(b["stops"]), ca, of, [grid, 0.0, 0.0, 0.0]))
        else:
            self.paints.append((0, list(P), 0.0, 0, 0, 0, -1, [0.0] * 4))
        return len(self.paints) - 1

    def layer(self, L):
        extra = set(L) - {"shape", "fill", "alpha", "soft", "clip", "falloff", "key", "name", "fx"}
        if extra:
            raise ValueError(f"port_export: layer keys {sorted(extra)} are not exported")
        sh = self.node(expand(L["shape"]))
        cl = self.node(expand(L["clip"])) if "clip" in L else -1
        # the bevel's normal is the layer shape's own (circle / ellipse, analytic) or its "of" shape's
        pa = self.paint(L["fill"], expand(L["shape"]))
        f = L.get("falloff")
        fa = len(self.fx)
        for e in L.get("fx", ()):   # sdf.py render_layer's effects, in order: (t, w, pow, dir x / y, colour 1, colour 2, k, a, b, add)
            t = FX[e["t"]]
            if t == 0:
                row = (0, e["w"], e.get("pow", 1.0), e["dir"][0], e["dir"][1], *e["light"], *e["dark"], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
            elif t == 1:
                row = (1, e["w"], e.get("pow", 2.0), e["dir"][0], e["dir"][1], *e["rgb"], 0.0, 0.0, 0.0, e.get("k", 0.8),
                       0.0, 0.0, 0.0, 0.0, 0.0)
            else:
                row = (2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, *e["a"], *e["b"], e["add"])
            self.fx.append(row)
        self.layers.append((sh, cl, pa, L.get("alpha", 1.0), L.get("soft", -1.0),
                            (1, f["c"][0], f["c"][1], f["r"], f.get("pow", 2.0)) if f else (0, 0.0, 0.0, 0.0, 0.0),
                            (fa, len(self.fx) - fa)))

    def item(self, it):
        la = len(self.layers)
        for L in it["layers"]:
            self.layer(L)
        self.items.append((list(it["box"]), la, len(it["layers"])))

    def scene(self, name, button, hover, items):
        ia = len(self.items)
        for it in items:
            self.item(it)
        cx, cy = title.CELLS[name][:2]   # the cut's origin (title.py CELLS = kTitleButtons' x, y)
        self.scenes.append((name, button, hover, SLOTS[name] + hover, cx, cy, ia, len(items)))


def num(v):
    v = float(v)
    if v == int(v) and abs(v) < 1e15:
        return f"{int(v)}.0"
    return repr(v)


def emit_tables(T):
    """the shared tables (gfx_uivec_eval.h UvTab) as C++ lines"""
    lines = ["static const UvNode kNodes[] = {"]
    for k, cap, closed, a, n, ns, ne, p in T.nodes:
        pp = (p + [0.0] * 10)[:10]
        lines.append(f"    {{{k}, {cap}, {closed}, {a}, {n}, {ns}, {ne}, {{{', '.join(num(v) for v in pp)}}}}},")
    lines.append("};")
    lines.append("static const int32_t kKids[] = {" + (", ".join(str(v) for v in T.kids) or "0") + "};")
    lines.append("static const int32_t kRings[] = {" + (", ".join(f"{a}, {n}" for a, n in T.rings) or "0") + "};")
    lines.append("static const double kPts[] = {")
    for i in range(0, len(T.pts), 4):
        lines.append("    " + " ".join(f"{num(p[0])}, {num(p[1])}," for p in T.pts[i:i + 4]))
    if not T.pts:
        lines.append("    0.0,")
    lines.append("};")
    lines.append("static const double kStops[] = {" + (", ".join(num(v) for v in T.stops) or "0.0") + "};")
    lines.append("static const double kCoef[] = {")
    for i in range(0, len(T.coef), 15):
        lines.append("    " + ", ".join(num(v) for v in T.coef[i:i + 15]) + ",")
    if not T.coef:
        lines.append("    0.0,")
    lines.append("};")
    lines.append("static const UvPaint kPaints[] = {")
    for kind, rgb, Lb, sa, sn, ca, of, g in T.paints:
        lines.append(f"    {{{kind}, {{{', '.join(num(v) for v in rgb)}}}, {num(Lb)}, {sa}, {sn}, {ca}, {of}, "
                     f"{{{', '.join(num(v) for v in g)}}}}},")
    lines.append("};")
    lines.append("static const UvLayer kLayers[] = {")
    for sh, cl, pa, al, so, f, fx in T.layers:
        tail = f", {fx[0]}, {fx[1]}" if fx[1] else ""   # P41G: a layer's effects (zero-initialised when it has none)
        lines.append(f"    {{{sh}, {cl}, {pa}, {num(al)}, {num(so)}, {f[0]}, {num(f[1])}, {num(f[2])}, {num(f[3])}, {num(f[4])}{tail}}},")
    lines.append("};")
    lines.append("static const UvItem kItems[] = {")
    for box, la, ln in T.items:
        lines.append(f"    {{{{{', '.join(num(v) for v in box)}}}, {la}, {ln}}},")
    if not T.items:
        lines.append("    {{0.0, 0.0, 0.0, 0.0}, 0, 0},")
    lines.append("};")
    if T.fx:   # P41G: the effects table (a table set without effects leaves UvTab::fx null)
        lines.append("static const UvFx kFx[] = {")
        for row in T.fx:
            lines.append(f"    {{{row[0]}, {{{', '.join(num(v) for v in row[1:])}}}}},")
        lines.append("};")
        lines.append("static const UvTab kTab = {kNodes, kKids, kRings, kPts, kStops, kCoef, kPaints, kLayers, kItems, kFx};")
    else:
        lines.append("static const UvTab kTab = {kNodes, kKids, kRings, kPts, kStops, kCoef, kPaints, kLayers, kItems};")
    return lines


HEADER_TABLES = [
    "// UvNode: kind (gfx_uivec_eval.h UvKind), cap, closed, a, n, ns, ne, p[10] (ellipse: c, r, rot, cos, sin; rrect: c, half, r, rot,",
    "// cos, sin; arc: c, r, a0, sweep, w, its two end points); UvPaint: kind (0 flat, 1 bevel, 2 linear, 3 radial), rgb, L, stops a / n,",
    "// coef a (bevel: stops x 5 x 3; linear / radial: one colour per stop), the bevel's own shape (-1: the layer's), geometry (linear:",
    "// a, b; radial: c, R; bevel: 1 = grid normal); UvLayer: shape, clip, paint, alpha, soft (< 0: none), falloff (on, cx, cy, r, pow); UvItem: box, layers a / n.",
]


# ------------------------------------------------------------------------------------------------------------ title buttons
def scenes():
    """[(name, kTitleButtons index, hover, scene items)] as sdf.py renders them (title.scene of the measured geometry)"""
    meas = title.measure(verbose=False)
    out = []
    for name, idx in BUTTONS:
        for hover in (0, 1):
            out.append((name, idx, hover, title.scene(meas, bool(hover), only=(name,))))
    return out


def export():
    T = Tables()
    sc = scenes()
    for name, idx, hover, items in sc:
        T.scene(name, idx, hover, items)
    lines = ["// GENERATED by re/tools/uivec/port_export.py from the approved title menu scenes (re/tools/uivec/title.py: title_geom.json,",
             "// glyphs_title.json, style.json; P41). Numbers only. Do not edit: run the exporter. Read by gfx_uivec.cpp.",
             f"// {len(T.scenes)} scenes, {len(T.items)} items, {len(T.layers)} layers, {len(T.nodes)} shape nodes, {len(T.pts)} points."]
    lines += HEADER_TABLES
    lines.append("// UvSceneDef: name, button (kTitleButtons index), hover, SpriteRef slot, the cut's origin x / y, items a / n.")
    lines.append("namespace uvt {")
    lines += emit_tables(T)
    lines.append("static const UvSceneDef kScenes[] = {")
    for name, idx, hover, slot, cx, cy, ia, n in T.scenes:
        lines.append(f"    {{\"title.{name}\", {idx}, {hover}, {slot}, {cx}, {cy}, {ia}, {n}}},")
    lines.append("};")
    lines.append("}  // namespace uvt")
    return "\n".join(lines) + "\n"


# ------------------------------------------------------------------------------------------------------------ wide screens
def wide_scene(screen):
    with open(os.path.join(WIDE_DIR, WIDE.get(screen, {}).get("file", f"{screen}-wide.json"))) as f:
        return json.load(f)


_DIFF = None


def diff_scenes():
    """{(pad, hover): items} of the difficulty screen's pads (diff.py over title.py's machinery, which title.configure retargets
    globally: run in its own process so this one's title.py stays the title menu's). Equal to log/roadmap/P41/diff/title_scene.json."""
    global _DIFF
    if _DIFF is None:
        import subprocess
        code = ("import json, diff, title; diff.setup(); m = title.measure(verbose=False); "
                "print(json.dumps({f'{n}|{h}': title.scene(m, h, only=(n,)) for n in m for h in (0, 1)}))")
        r = subprocess.run([sys.executable, "-c", code], cwd=HERE, capture_output=True, text=True, check=True)
        _DIFF = {(k.split("|")[0], bool(int(k.split("|")[1]))): v for k, v in json.loads(r.stdout.strip().splitlines()[-1]).items()}
    return _DIFF


def overlay_items(spec):
    """the scene items of a "draws" entry's scene spec"""
    import wide
    if spec[0] == "diffpy":
        return diff_scenes()[(spec[1], bool(spec[2]))]
    if spec[0] == "ref":
        return wide.load_ref(spec[1], spec[2])
    raise ValueError(spec)


def wide_refs(screen):
    """the refs of a wide scene that are not plain approved scene files"""
    if screen == "title":
        return {"title.py:idle": title.scene(title.measure(verbose=False), False)}
    if "refs" in WIDE.get(screen, {}):   # P41G round 6: in-game element scenes (states as a dict: the first one's scene)
        area, ids = WIDE[screen]["refs"]
        out = {}
        for eid in ids:
            with open(os.path.join(HERE, "scenes", area, eid + ".json"), encoding="utf-8") as f:
                out[eid] = list(json.load(f)["states"].values())[0]["scene"]
        return out
    return {}


def _i(v, k, what):
    """v in units of 1 / k as an int, refused when it is not exact"""
    n = int(round(v * k))
    if n / k != v:
        raise ValueError(f"port_export: blob {what} {v!r} is not a multiple of 1/{k}")
    return n


def blob_row(e):
    """(cx, cy, rx, ry, rot, soft, r, g, b, alpha) in 1/100, 1/100, 1/100, 1/100, 1/10, 1/100, 1/10 x3, 1/1000, or None when the
    entry is no ground blob (wide.py blob_layer / ground_entries)"""
    if e.get("anchor") != "span" or " blob " not in e.get("name", "") or len(e.get("layers", ())) != 1:
        return None
    L = e["layers"][0]
    S = L["shape"]
    if set(L) != {"shape", "soft", "fill", "alpha"} or S["t"] != "ellipse" or isinstance(L["fill"], dict) or set(S) - {"t", "c", "r", "rot"}:
        return None
    row = (_i(S["c"][0], 100, "cx"), _i(S["c"][1], 100, "cy"), _i(S["r"][0], 100, "rx"), _i(S["r"][1], 100, "ry"),
           _i(S.get("rot", 0.0), 10, "rot"), _i(L["soft"], 100, "soft"), _i(L["fill"][0], 10, "r"), _i(L["fill"][1], 10, "g"),
           _i(L["fill"][2], 10, "b"), _i(L["alpha"], 1000, "alpha"))
    m = max(row[2], row[3]) + row[5] + 100
    box = [round((row[0] - m) / 100, 2), round((row[1] - m) / 100, 2), round((row[0] + m) / 100, 2), round((row[1] + m) / 100, 2)]
    if box != [round(v, 2) for v in e["box"]] or any(abs(a - b) > 0 for a, b in zip(box, e["box"])):
        raise ValueError(f"port_export: blob {e['name']} box {e['box']} is not blob_box {box}")
    if not all(-32768 <= v <= 32767 for v in row[2:]):
        raise ValueError(f"port_export: blob {e['name']} out of the table's range")
    return row


def wide_export(screen):
    import wide
    cfg = WIDE[screen]
    if "refs" in cfg:   # P41G round 6: the in-game areas' polygon kinds (rm.arc, reports_slabs.rpoly) expanded as port_game does
        import port_game
        port_game._area_kinds()
        AREA_EXPAND.update(port_game.PE.AREA_EXPAND)   # (run as __main__, port_game's port_export is another module object)
    sc = wide_scene(screen)
    refs = wide_refs(screen)
    T = Tables()
    blobs, ops, pics, overlays = [], [], [], []
    seen_overlay = False
    for e in sc["draw"]:
        how = cfg["overlays"].get(e.get("name"))
        if e.get("anchor", "centre") not in ("centre", "span"):
            raise ValueError(f"port_export: anchor {e.get('anchor')} (no screen uses left / right; a placement per aspect is not exported)")
        if "picture" in e:
            p = e["picture"]
            m = p["matte"]
            at = p.get("at", p["rect"][:2])
            pics.append((p["sheet"].lower().replace("\\", "/"), *p["rect"], at[0], at[1], MATTES[m["kind"]], m.get("t0", 0.0),
                         m.get("t1", 0.0), m.get("feather", 0.0), 0 if how else 1, e["name"]))
            if how:
                if not how.startswith("picture:"):
                    raise ValueError(f"port_export: overlay {e['name']} of a picture must be picture:<slot>")
                ox, oy = at
                overlays.append((e["name"], 1, 1, 0x2c * int(how.split(":")[1]), ox, oy, len(pics) - 1, 0, 0))
                seen_overlay = True
            else:
                if m["kind"] == "ground_diff" and (seen_overlay or any(o[0] == 2 for o in ops)):
                    raise ValueError("port_export: a ground_diff picture after an overlay or another picture (its ground is not modelled)")
                ops.append((2, len(pics) - 1, 1))
            continue
        if how:
            if how != "buttons":
                raise ValueError(f"port_export: overlay kind {how}")
            overlays.append((e["name"], 0, 0, 0, 0, 0, -1, 0, 0))
            seen_overlay = True
            continue
        row = blob_row(e) if "layers" in e else None
        if row is not None:
            if ops and ops[-1][0] == 1:
                ops[-1] = (1, ops[-1][1], ops[-1][2] + 1)
            else:
                ops.append((1, len(blobs), 1))
            blobs.append(row)
            continue
        placed = wide.place({"draw": [e]}, 4 / 3, refs)
        items = [it for kind, its in placed for it in its]
        ia = len(T.items)
        for it in items:
            T.item(it)
        if ops and ops[-1][0] == 0:
            ops[-1] = (0, ops[-1][1], ops[-1][2] + len(items))
        else:
            ops.append((0, ia, len(items)))
    shared_scenes = {}
    for d in cfg.get("draws") or []:
        kk, off = d["key"]
        ox, oy = d.get("origin", d["at"])
        if d["scene"] == "restore":
            overlays.append((d["name"], 3, KEY_KINDS[kk], off, ox, oy, -1, 0, 0))
            continue
        # P41S: one scene drawn on several cuts (the editor's pills: one scene for ten cells) is exported once; each overlay keeps
        # its own key and origin and points at the shared items
        sk = repr(d["scene"])
        if sk not in shared_scenes:
            items = overlay_items(d["scene"])
            ia = len(T.items)
            for it in items:
                T.item(it)
            shared_scenes[sk] = (ia, len(items))
        ia, nit = shared_scenes[sk]
        overlays.append((d["name"], 2, KEY_KINDS[kk], off, ox, oy, -1, ia, nit))
    # the leading ground ops (rendered at the ground's scale): the base rectangle alone in the first items op, then a blobs op
    nground = 0
    first = sc["draw"][0]
    if ops and ops[0][0] == 0 and ops[0][2] == 1 and first.get("anchor") == "span" and "ground base" in first.get("name", ""):
        nground = 2 if len(ops) > 1 and ops[1][0] == 1 else 1
    ns = f"uvw_{screen}"
    lines = [f"// GENERATED by re/tools/uivec/port_export.py --wide {screen} from re/tools/uivec/scenes/wide/{screen}-wide.json (P41W).",
             "// Numbers only (no art bytes: pictures are lifted from the game's sheets at run time). Do not edit: run the exporter.",
             f"// {len(ops)} ops, {len(blobs)} ground blobs, {len(T.items)} items, {len(T.layers)} layers, {len(T.nodes)} shape nodes, "
             f"{len(T.pts)} points, {len(pics)} pictures, {len(overlays)} overlays."]
    lines += HEADER_TABLES
    lines += ["// UvBlob: centre x / y (1/100), radii (1/100), angle (1/10 degree), softness (1/100), colour (1/10), alpha (1/1000);",
              "// UvwOp: kind (0 items, 1 blobs, 2 picture), first, count; UvwPic: sheet, rect x / y / w / h, at x / y, matte (0 key,",
              "// 1 opaque, 2 ground_diff), t0, t1, feather, static (1: in the backdrop), name; UvwOverlay: name, kind (0 the title",
              "// buttons, 1 a picture, 2 a scene, 3 restore the backdrop), key kind (1 g_spriteRef_53b7e0, 2 gfx_local_key, 3",
              "// g_sprites59df20) and offset, the scene's point that lands on the draw point, picture, items a / n;",
              "// UvwScreenDef: name, background sheet, ops, ground ops, pictures, overlays."]
    lines.append(f"namespace {ns} {{")
    lines += emit_tables(T)
    lines.append("static const UvBlob kBlobs[] = {")
    for i in range(0, len(blobs), 3):
        lines.append("    " + " ".join("{" + ", ".join(str(v) for v in r) + "}," for r in blobs[i:i + 3]))
    if not blobs:
        lines.append("    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},")
    lines.append("};")
    lines.append("static const UvwOp kOps[] = {" + " ".join(f"{{{k}, {a}, {n}}}," for k, a, n in ops) + "};")
    lines.append("static const UvwPic kPics[] = {")
    for (sh, x, y, w, h, ax, ay, mk, t0, t1, fe, st, name) in pics:
        lines.append(f"    {{\"{sh}\", {x}, {y}, {w}, {h}, {ax}, {ay}, {mk}, {num(t0)}, {num(t1)}, {num(fe)}, {st}, \"{name}\"}},")
    if not pics:
        lines.append("    {\"\", 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, \"\"},")
    lines.append("};")
    lines.append("static const UvwOverlay kOverlays[] = {")
    for name, kind, kk, off, ox, oy, pic, ia, n in overlays:
        lines.append(f"    {{\"{name}\", {kind}, {kk}, {off}, {ox}, {oy}, {pic}, {ia}, {n}}},")
    if not overlays:
        lines.append("    {\"\", -1, 0, 0, 0, 0, -1, 0, 0},")
    lines.append("};")
    lines.append(f"static const UvwScreenDef kScreen = {{\"{screen}\", \"{cfg['bg']}\", &kTab, kBlobs, {len(blobs)}, kOps, {len(ops)}, "
                 f"{nground}, kPics, {len(pics)}, kOverlays, {len(overlays)}}};")
    lines.append(f"}}  // namespace {ns}")
    return "\n".join(lines) + "\n", {"ops": len(ops), "blobs": len(blobs), "items": len(T.items), "layers": len(T.layers),
                                       "nodes": len(T.nodes), "pics": len(pics), "nground": nground}


def wide_out(screen):
    return os.path.join(ROOT, "port", "src", "sim", f"uivec_wide_{screen}.inc")


def _write_or_check(path, text, check):
    if check:
        cur = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
        print(f"{os.path.relpath(path, ROOT)}: " + ("up to date" if cur == text else "STALE: run re/tools/uivec/port_export.py"))
        return 0 if cur == text else 1
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"wrote {os.path.relpath(path, ROOT)} ({len(text)} bytes)")
    return 0


def main():
    check = "--check" in sys.argv
    if "--wide" in sys.argv:
        which = sys.argv[sys.argv.index("--wide") + 1]
        rc = 0
        for screen in (list(WIDE) if which == "all" else [which]):
            text, info = wide_export(screen)
            print(f"{screen}: {info}")
            rc |= _write_or_check(wide_out(screen), text, check)
        return rc
    return _write_or_check(OUT, export(), check)


if __name__ == "__main__":
    sys.exit(main())
