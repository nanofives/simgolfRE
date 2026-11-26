"""uivec -> the port, in game (P41G, log/roadmap/P41/PORT_INGAME_BRIEF.md, log/roadmap/P41/portgame/notes.md): the approved in-game
element scenes as generated includes (numbers only), one per area.

    py -3.12 re/tools/uivec/port_game.py --area <area>|all [--check]    -> port/src/sim/uivec_game_<area>.inc
    py -3.12 re/tools/uivec/port_game.py --list <area>                  the normalised records (id, state, sheet, cut)

THE RECORD. Every area wrote its scenes in its own coordinates (re/tools/uivec/scenes/<area>/<id>.json); this module brings each
element state to one record:
    id, state, sheet (the GameFiles::normalize name of the source sheet: lower case, '/' separators), cut = [x, y, w, h] (the state's
    rectangle in the sheet's pixels), shift = (sx, sy) (sheet point = scene point + shift), items (the scene, exported as is).
  hud_palettes  "source sheet pixels": states[s] = {sheet, cut, scene}; shift 0
  hud_panels    "sheet pixels of <sheet>": states[s] = {cell, scene}; shift 0
  popups        top "sheet" (or "sheets"[state]); states[s] = {box: x0, y0, x1, y1 in sheet pixels, scene, textures?}; shift 0
  reports_maps  states[s] = {origin, size, scene} in sheet pixels; the sheet from the roster; shift 0
  reports_slabs one "scene" for the element in its 800 x 600 sheet's pixels, "crops" [[state, [x, y, w, h]], ...]; shift 0
  marks         states = [{state, sheet, rect, scene}], the scene in crop pixels: shift = the rect's corner
THE PORT (port/src/sim/gfx_uvgame.cpp) finds the records of a drawn sprite by its provenance (Sprite8::sheet and the crop rectangle,
gfx_body.h): every record of that sheet whose cut lies inside the crop rectangle; the scene is drawn moved by the sprite's draw point
minus the crop's corner (plus the record's shift), clipped to the crop rectangle.
"""
import csv
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import port_export as PE  # noqa: E402

ROOT = PE.ROOT
SCENES = os.path.join(HERE, "scenes")
ROSTER = os.path.join(ROOT, "log", "roadmap", "P41", "roster", "roster.json")
AREAS = ["hud_palettes", "hud_panels", "popups", "reports_slabs", "reports_maps", "marks"]

_ROSTER = None


def roster():
    global _ROSTER
    if _ROSTER is None:
        with open(ROSTER, encoding="utf-8") as f:
            _ROSTER = {e["id"]: e for e in json.load(f)["elements"]}
    return _ROSTER


def norm(sheet):
    return sheet.replace("\\", "/").lower()


def manifest(area):
    with open(os.path.join(SCENES, "manifest.tsv"), encoding="utf-8") as f:
        return [r["id"] for r in csv.DictReader(f, delimiter="\t") if r["area"] == area]


def load(area, eid):
    p = os.path.join(SCENES, area, eid + ".json")
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def is_key_layer(L, cut, shift):
    """the colour cell's transparent key drawn as a layer: a flat black ([0, 0, 0]), opaque, hard-edged, unclipped rounded rectangle
    covering the whole cut. Some cells (the golfer info panel's tabs, the tacks, the magenta arrow) are black around the element in the
    colour sheet and the game's mask cell makes that black transparent; their approved scenes, drawn over the colour cell, paint it
    (the golfer info panel showed black boxes around its tabs, log/roadmap/P41/portgame/notes.md). Such a layer is not exported."""
    S = L["shape"]
    if list(L["fill"]) != [0, 0, 0] if not isinstance(L["fill"], dict) else True:
        return False
    if L.get("alpha", 1.0) != 1.0 or "soft" in L or "clip" in L or "fx" in L or S["t"] != "rrect" or S.get("rot", 0.0):
        return False
    x, y, w, h = cut
    cx, cy = S["c"][0] + shift[0], S["c"][1] + shift[1]
    hx, hy = S["half"]
    return cx - hx <= x + 1 and cy - hy <= y + 1 and cx + hx >= x + w - 1 and cy + hy >= y + h - 1


def drop_key_layers(items, cut, shift):
    out = []
    for it in items:
        if "layers" not in it:
            out.append(it)
            continue
        ls = [L for L in it["layers"] if not is_key_layer(L, cut, shift)]
        if len(ls) == len(it["layers"]):
            out.append(it)
        elif ls:
            out.append(dict(it, layers=ls))
    return out


def records(area):
    """the normalised records, the colour cells' transparent-key layers left out (is_key_layer)"""
    out = []
    for eid, st, sheet, cut, shift, items, extra in _records(area):
        out.append((eid, st, sheet, cut, shift, drop_key_layers(items, cut, shift) if area in KEY_AREAS else items, extra))
    return out


# the areas whose cells carry a transparent key the game's mask removes (hud_panels: the golfer tabs; marks: tacks, magenta arrow)
KEY_AREAS = {"hud_panels", "marks"}


def _records(area):
    """[(id, state, sheet, cut, shift, items, extra)] in manifest order (extra: textures and other per-state keys kept for the port)"""
    out = []
    for eid in manifest(area):
        d = load(area, eid)
        if d is None:
            continue
        ros = roster().get(eid, {})
        if area == "hud_palettes":
            for st, s in d["states"].items():
                out.append((eid, st, norm(s["sheet"]), list(s["cut"]), (0, 0), s["scene"], {}))
        elif area == "hud_panels":
            sheet = d["coords"].split("sheet pixels of ", 1)[1]
            for st, s in d["states"].items():
                out.append((eid, st, norm(sheet), list(s["cell"]), (0, 0), s["scene"], {}))
        elif area == "popups":
            for st, s in d["states"].items():
                sheet = d["sheet"] if "sheet" in d else d["sheets"][st]
                x0, y0, x1, y1 = s["box"]
                out.append((eid, st, norm(sheet), [x0, y0, x1 - x0, y1 - y0], (0, 0), s["scene"],
                            {k: s[k] for k in ("textures",) if k in s}))
        elif area == "reports_maps":
            for st, s in d["states"].items():
                out.append((eid, st, norm(ros["sheet"]), [*s["origin"], *s["size"]], (0, 0), s["scene"], {}))
        elif area == "reports_slabs":
            if "crops" not in d:
                continue
            for st, rect in d["crops"]:
                out.append((eid, st, norm(ros["sheet"]), list(rect), (0, 0), d["scene"], {}))
        elif area == "marks":
            for s in d["states"]:
                r = s["rect"]
                out.append((eid, s["state"], norm(s["sheet"]), list(r), (r[0], r[1]), s["scene"], {}))
        else:
            raise ValueError(area)
    return out


# the modules that register each area's shape kinds with sdf.EXTRA (what sdf.render of an approved scene needs)
KIND_MODULES = {"hud_panels": ["figs"], "popups": ["hs3"], "reports_slabs": ["slabkit"], "reports_maps": ["digits", "parts"],
                "marks": ["common", "el_loose", "fix2"]}


def load_area_kinds():
    """imports every area's kind modules into this process (several areas have a module named `common`: each area's imports run with
    its own directory first on sys.path and its own `common` in sys.modules)"""
    import importlib
    saved = sys.modules.pop("common", None)
    try:
        for area, mods in KIND_MODULES.items():
            d = os.path.join(HERE, "areas", area)
            sys.modules.pop("common", None)
            sys.path.insert(0, d)
            try:
                for m in mods:
                    sys.modules.pop(m, None) if m == "common" else None
                    importlib.import_module(m)
            finally:
                sys.path.remove(d)
    finally:
        sys.modules.pop("common", None)
        if saved is not None:
            sys.modules["common"] = saved


def _area_kinds():
    """the area modules' polygon kinds, expanded by the modules' own builders (sdf.py's EXTRA evaluates the same polygons)"""
    if PE.AREA_EXPAND:
        return
    for area in ("reports_slabs", "reports_maps"):
        d = os.path.join(HERE, "areas", area)
        if d not in sys.path:
            sys.path.insert(0, d)
    import slabkit   # noqa: E402  (reports_slabs.rpoly: rpoly_ring)
    import digits    # noqa: E402  (rm.arc: band(_arc_pts(S), w))
    PE.AREA_EXPAND["reports_slabs.rpoly"] = lambda S: {"t": "poly", "rings": [slabkit.rpoly_ring(S)]}
    PE.AREA_EXPAND["rm.arc"] = lambda S: digits.band(digits._arc_pts(S), S["w"])
    d = os.path.join(HERE, "areas", "marks")
    if d not in sys.path:
        sys.path.insert(0, d)
    import numpy as np  # noqa: E402
    import fix2        # noqa: E402  (marks.vline: catmull)

    def star(S):   # common.py _star
        n = S.get("n", 5)
        cx, cy = S["c"]
        pts = []
        for k in range(2 * n):
            a = np.deg2rad(S.get("a0", -90.0) + k * 180.0 / n)
            rr = S["r"] if k % 2 == 0 else S["ri"]
            pts.append([float(cx + rr * np.cos(a)), float(cy + rr * np.sin(a))])
        return {"t": "poly", "rings": [pts]}

    def heart(S):   # el_loose.py _heart
        t = np.linspace(0, 2 * np.pi, 97)[:-1]
        hx = 16 * np.sin(t) ** 3
        hy = -(13 * np.cos(t) - 5 * np.cos(2 * t) - 2 * np.cos(3 * t) - np.cos(4 * t))
        x0, y0, x1, y1 = S["box"]
        hx = x0 + (hx - hx.min()) / (hx.max() - hx.min()) * (x1 - x0)
        hy = y0 + (hy - hy.min()) / (hy.max() - hy.min()) * (y1 - y0)
        return {"t": "poly", "rings": [np.c_[hx, hy].tolist()]}

    def vline(S):   # fix2.py _vline: the spline's points Q and the widths W along them
        closed = S.get("closed", False)
        n = S.get("n", 8)
        Q = fix2.catmull(S["pts"], n, closed)
        ws = S["ws"]
        wsx = list(ws) + ([ws[0]] if closed else [])
        W = []
        for k in range(len(wsx) - 1):
            for i in range(n):
                W.append(wsx[k] * (1 - i / n) + wsx[k + 1] * i / n)
        W.append(wsx[-1])
        return {"t": "marks.vline", "pts": [[float(q[0]), float(q[1]), float(w)] for q, w in zip(Q, W)]}
    PE.AREA_EXPAND["marks.star"] = star
    PE.AREA_EXPAND["marks.heart"] = heart
    PE.AREA_EXPAND["marks.vline"] = vline


# popups textures (kept pictures: {"texture": {sheet, rect}, "clip": shape}): pasted between the items drawn under them and the ones
# drawn over them (areas/popups/pages.py render_tex); the scene files do not record where, pages.py does: the number of items under
# the texture per element (default: all of them)
TEX_UNDER = {"pair-capsule": 1}


def export(area):
    _area_kinds()
    T = PE.Tables()
    pics = []   # PICTURE items (wide.py's format: art lifted with a matte against the items drawn beneath): one row each
    rows = []
    shared = {}   # one scene shared by several states (reports_slabs): exported once
    for eid, st, sheet, cut, shift, items, extra in records(area):
        tex = extra.get("textures") or []
        for t in tex:
            if norm(t["texture"]["sheet"]) != sheet:
                raise ValueError(f"port_game: {eid}.{st}: a texture from another sheet ({t['texture']['sheet']})")
        # one scene shared by several states (reports_slabs) is exported once; a keep item's box is the cut, so then per cut
        key = (id(items), tuple(cut) if (tex or any("keep_art" in it for it in items)) else None)
        if key not in shared:
            ia = len(T.items)
            keep = []
            n_under = TEX_UNDER.get(eid, len(items))
            for k, it in enumerate(items):
                if tex and k == n_under:
                    add_tex(T, tex)
                if "picture" in it:
                    add_pic(T, pics, it["picture"], shift, f"{eid}.{st}")
                    continue
                if "keep_art" in it:
                    keep.append(it["keep_art"])
                    continue
                T.item(it)
            if tex and n_under >= len(items):
                add_tex(T, tex)
            if keep:
                # hud_palettes hp.render_state: the art's own pixels where the union of the keep shapes covers (nearest, its edge
                # coverage clip(0.5 - d s)), after every layer: a KEEP ITEM (nlayers -1, layer_a = the union's node), box = the cut
                node = T.node(PE.expand({"t": "union", "of": keep}))
                x, y, w, h = cut
                T.items.append(([x - shift[0], y - shift[1], x + w - shift[0], y + h - shift[1]], node, -1))
            shared[key] = (ia, len(T.items) - ia)
        ia, n = shared[key]
        rows.append((eid, st, sheet, cut, shift, ia, n))
    ns = f"uvg_{area}"
    lines = [f"// GENERATED by re/tools/uivec/port_game.py --area {area} from the approved scenes re/tools/uivec/scenes/{area}/ (P41G).",
             "// Numbers only. Do not edit: run the exporter. Read by gfx_uvgame.cpp.",
             f"// {len(rows)} element states, {len(T.items)} items, {len(T.layers)} layers, {len(T.nodes)} shape nodes, {len(T.pts)} points, "
             f"{len(T.fx)} effects."]
    lines += PE.HEADER_TABLES
    lines += ["// UvgPic: sheet, rect x / y / w / h in it, at x / y (the scene point of its pixel (0, 0)), ground_diff t0 / t1 / feather, the",
              "// solid node (alpha 1 inside; -1 none), the clip node (alpha x clip(1 - d / clip_soft); -1 none), clip_soft, name. A",
              "// picture item is nlayers -2, layer_a = its row.",
              "// UvFx: kind (0 edge_light, 1 inner_shadow, 2 tone), v: w, pow, dir x / y, colour 1 (light / rgb), colour 2 (dark), k,",
              "// a x / y, b x / y, add. Kinds 13 limb (points then their widths as pairs; cap 1 butt), 14 rrect4 (box, radii tl tr br bl),",
              "// 15 sinter (two kids, r).",
              "// UvgCut: element id, state, sheet (GameFiles::normalize), cut x / y / w / h in the sheet, shift x / y (sheet = scene + shift),",
              "// items a / n."]
    lines.append(f"namespace {ns} {{")
    lines += PE.emit_tables(T)
    lines.append("static const UvgCut kCuts[] = {")
    for eid, st, sheet, cut, shift, ia, n in rows:
        lines.append(f"    {{\"{eid}\", \"{st}\", \"{sheet}\", {', '.join(str(int(round(v))) for v in cut)}, {int(shift[0])}, {int(shift[1])}, "
                     f"{ia}, {n}}},")
    lines.append("};")
    if pics:
        lines.append("static const UvgPic kPics[] = {")
        for (sh, x, y, w, h, ax, ay, t0, t1, fe, solid, clip, csoft, name) in pics:
            lines.append(f"    {{\"{sh}\", {x}, {y}, {w}, {h}, {ax}, {ay}, {PE.num(t0)}, {PE.num(t1)}, {PE.num(fe)}, {solid}, {clip}, "
                         f"{PE.num(csoft)}, \"{name}\"}},")
        lines.append("};")
        lines.append(f"static const UvgArea kArea = {{\"{area}\", &kTab, kCuts, {len(rows)}, kPics, {len(pics)}}};")
    else:
        lines.append(f"static const UvgArea kArea = {{\"{area}\", &kTab, kCuts, {len(rows)}}};")
    lines.append(f"}}  // namespace {ns}")
    return "\n".join(lines) + "\n", {"states": len(rows), "items": len(T.items), "layers": len(T.layers), "nodes": len(T.nodes),
                                       "fx": len(T.fx)}


def add_pic(T, pics, pic, shift, what):
    """a PICTURE item (wide.py's format, the fields of areas/reports_slabs/fix6_sga.py: matte ground_diff t0 / t1 / feather, optional
    solid and clip + clip_soft): its row in kPics and an item nlayers -2 whose box is the picture's rectangle at its place, one pixel
    more each side (the placement's rounding at a scale). The port lifts it at its first draw against the record's items before it
    at 1x over the frame (fix6_sga.lift, wide.under_1x)."""
    m = pic["matte"]
    if m["kind"] != "ground_diff":
        raise ValueError(f"port_game: {what}: a picture with matte {m['kind']} (only ground_diff is ported in game)")
    if set(m) - {"kind", "t0", "t1", "feather", "solid", "clip", "clip_soft"} or set(pic) - {"sheet", "rect", "at", "matte"}:
        raise ValueError(f"port_game: {what}: picture fields the port does not know: {sorted(set(m) | set(pic))}")
    x, y, w, h = (int(v) for v in pic["rect"])
    ax, ay = (int(v) for v in pic.get("at", [x, y]))
    ax, ay = ax - int(shift[0]), ay - int(shift[1])   # scene coordinates (scene = sheet - shift)
    solid = T.node(PE.expand(m["solid"])) if "solid" in m else -1
    clip = T.node(PE.expand(m["clip"])) if "clip" in m else -1
    pics.append((norm(pic["sheet"]), x, y, w, h, ax, ay, float(m.get("t0", 0.0)), float(m.get("t1", 0.0)), float(m.get("feather", 0.0)),
                 solid, clip, float(m.get("clip_soft", 1.0)), what))
    T.items.append(([ax - 1, ay - 1, ax + w + 1, ay + h + 1], len(pics) - 1, -2))


def add_tex(T, tex):
    """each texture as a KEEP ITEM (the sheet's own pixels where its clip shape covers, edge coverage clip(0.5 - d s): pages.py
    render_tex; the texture rectangle is in the same sheet's pixels as the scene, so the sample is the sheet pixel under the point)"""
    for t in tex:
        x, y, w, h = t["texture"]["rect"]
        node = T.node(PE.expand(t["clip"]))
        T.items.append(([x, y, x + w, y + h], node, -1))


def out_path(area):
    return os.path.join(ROOT, "port", "src", "sim", f"uivec_game_{area}.inc")


def main():
    check = "--check" in sys.argv
    if "--list" in sys.argv:
        for r in records(sys.argv[sys.argv.index("--list") + 1]):
            print("\t".join(str(v) for v in r[:5]))
        return 0
    which = sys.argv[sys.argv.index("--area") + 1] if "--area" in sys.argv else "all"
    rc = 0
    for area in (EXPORTED if which == "all" else [which]):
        text, info = export(area)
        print(f"{area}: {info}")
        rc |= PE._write_or_check(out_path(area), text, check)
    return rc


# the areas the port draws (gfx_uvgame.cpp includes one file per area)
EXPORTED = ["hud_palettes", "hud_panels", "popups", "reports_slabs", "reports_maps", "marks"]

if __name__ == "__main__":
    sys.exit(main())
