"""uivec states -> the port (P41S, log/roadmap/P41/states/notes.md): an element's states exported as ONE base drawing plus a short
list of PATCHES per state, instead of a complete scene per state.

Read by port_game.py export(): for every record (element state) whose element file re/tools/uivec/states/<area>/<id>.json says the
state is an exact alteration of its variant's base (status base / exact / variant-base) and whose non-layer items (kept art,
pictures, textures) are the base's own, the record points at the base's items (exported once per element variant) and carries:
  * shift + origin: the base items are in the base cut's coordinates (sheet = scene + shift);
  * PATCHES (UvgPatch rows: kind, target, row, box), applied by the port to the base's rows when it resolves the record:
      kind 0  layer `target` (its index among the base's layer rows) is replaced by layer row `row` (-1: not drawn)
      kind 1  item `target` (its index among the base's items) takes the box `box`
    A replacement row shares the base's shape and clip nodes; a moved layer's shape (and clip, when it moves too) is a MOVE node
    (kind "uv.move", p = dx, dy, one kid) over the base's node, a spread is a "grow" node; moved paints are new paint rows that
    share the base's stops and coefficients. No shape is copied.
Every other record is exported as before (its own scene).
"""
import copy
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
STATES_DIR = os.path.join(HERE, "states")
CONVERTIBLE = ("base", "exact", "variant-base")
_KINDS = []


def load(area):
    """{id: element} of the area's approved element files (round-7 pending files are not exported)"""
    d = os.path.join(STATES_DIR, area)
    out = {}
    if not os.path.isdir(d):
        return out
    for f in sorted(os.listdir(d)):
        if f.endswith(".json"):
            with open(os.path.join(d, f), encoding="utf-8") as fh:
                e = json.load(fh)
            if e.get("source", "approved") == "approved":
                out[e["id"]] = e
    return out


def _strip(L):
    return {k: v for k, v in L.items() if k not in ("id", "hidden")}


def _nonlayer(items):
    """the non-layer items of a scene (kept art, pictures) as JSON, in order"""
    return [json.dumps(it, sort_keys=True) for it in items if "layers" not in it]


def convertible(elem, st, items, extra, current=None):
    """the record's state is an exact alteration, its kept art / pictures / textures are the base's (moved by the origin), and the
    element file is current: the state's and its base's approved scenes hash as when it was derived (current: {state: items} of
    the element's records now)"""
    import states as STS
    if elem is None or st not in elem["states"]:
        return False
    ref = elem["states"][st]
    if ref.get("status") not in CONVERTIBLE or extra.get("textures"):
        return False
    var = elem["variants"][ref["variant"]]
    base = var["base"]
    if ref.get("sha") != STS.scene_sha(items):
        return False
    if current is not None and (base not in current or elem["states"][base].get("sha") != STS.scene_sha(current[base])):
        return False
    A = var["states"].get(st)
    if A is None:
        return False
    # an alteration that touches a kept-art item (moves it, switches it) is not exported as a patch
    keep_ids = {it.get("id") for it in var["items"] if "layers" not in it}
    touched = set(A.get("off", ())) | set(A.get("on", ())) | {i for g in A.get("move", ()) for i in g["layers"]}
    if keep_ids & touched:
        return False
    o = ref.get("origin", [0, 0])
    mine = _nonlayer(STS._moved_items(items, o) if o != [0, 0] and o != [0.0, 0.0] else items)
    base = _nonlayer([_strip(it) if "layers" not in it else it for it in var["items"]])
    return mine == base


def reference(elem, st):
    """(items in the base's coordinates, origin): states.apply of the record's state, what the port must draw"""
    import states as STS
    ref = elem["states"][st]
    return STS.apply(elem["variants"][ref["variant"]], st), ref.get("origin", [0, 0])


class BaseMap:
    """where a variant's base landed in the tables: per layer id its row and its index among the base's layer rows, per base item
    its index among the record's items"""

    def __init__(self):
        self.row, self.rel, self.layer, self.item_rel = {}, {}, {}, {}
        self.first_row = None


def emit_base(T, var, emit_item):
    """the variant's base items into the tables through emit_item(item) (port_game's item emitter); hidden layers included.
    Returns a BaseMap."""
    M = BaseMap()
    n_items_before = len(T.items)
    for bi, it in enumerate(var["items"]):
        if "layers" in it:
            la = len(T.layers)
            if M.first_row is None:
                M.first_row = la
            emit_item(dict(it, layers=[_strip(L) for L in it["layers"]]))
            for k, L in enumerate(it["layers"]):
                M.row[L["id"]] = la + k
                M.rel[L["id"]] = la + k - M.first_row
                M.layer[L["id"]] = L
            M.item_rel[bi] = len(T.items) - 1 - n_items_before
        else:
            emit_item(_strip(it))
    return M


def _move_node(T, kid, dx, dy):
    k = _KINDS.index("uv.move")
    T.nodes.append((k, 0, 0, len(T.kids), 1, 0, 0, [float(dx), float(dy)]))
    T.kids.append(kid)
    return len(T.nodes) - 1


def _grow_node(T, kid, d):
    k = _KINDS.index("grow")
    T.nodes.append((k, 0, 0, len(T.kids), 1, 0, 0, [float(d)]))
    T.kids.append(kid)
    return len(T.nodes) - 1


def _fx_rows(T, fx, dx, dy, FX):
    """the effects as UvFx rows (port_export.Tables.layer's layout), tone points moved by (dx, dy)"""
    fa = len(T.fx)
    for e in fx:
        t = FX[e["t"]]
        if t == 0:
            row = (0, e["w"], e.get("pow", 1.0), e["dir"][0], e["dir"][1], *e["light"], *e["dark"], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        elif t == 1:
            row = (1, e["w"], e.get("pow", 2.0), e["dir"][0], e["dir"][1], *e["rgb"], 0.0, 0.0, 0.0, e.get("k", 0.8),
                   0.0, 0.0, 0.0, 0.0, 0.0)
        else:
            row = (2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, e["a"][0] + dx, e["a"][1] + dy, e["b"][0] + dx,
                   e["b"][1] + dy, e["add"])
        T.fx.append(row)
    return fa, len(T.fx) - fa


def patches(T, PE, var, st, M, patch_rows):
    """the state's patches appended to patch_rows ((kind, target, row, box)); returns (first, count)"""
    import states as STS
    global _KINDS
    _KINDS = PE.KINDS
    A = var["states"][st]
    pa = len(patch_rows)
    paint = A.get("paint", {})
    moves = {}
    for g in A.get("move", ()):
        for lid in g["layers"]:
            moves[lid] = (g["d"], bool(g.get("clip", False)))
    spread = A.get("spread", {})
    off = set(A.get("off", ()))
    on = set(A.get("on", ()))
    for lid, L in M.layer.items():
        rel, row = M.rel[lid], M.row[lid]
        if lid in off or (L.get("hidden") and lid not in on):
            patch_rows.append((0, rel, -1, (0.0, 0.0, 0.0, 0.0)))
            continue
        if lid not in paint and lid not in moves and lid not in spread:
            continue
        sh, cl, pa_, al, so, f, (fxa, fxn) = T.layers[row]
        (dx, dy), mclip = moves.get(lid, ((0.0, 0.0), False))
        eff = _strip(L)
        for k, v in paint.get(lid, {}).items():
            if v is None:
                eff.pop(k, None)
            else:
                eff[k] = copy.deepcopy(v)
        if lid in spread:
            sh = _grow_node(T, sh, spread[lid])
        if dx or dy:
            sh = _move_node(T, sh, dx, dy)
            if cl >= 0 and mclip:
                cl = _move_node(T, cl, dx, dy)
        # paint: a new row when overridden or when its coordinates move
        base_fill = L["fill"]
        fill = eff["fill"]
        moved_coords = (dx or dy) and isinstance(fill, dict)
        if "fill" in paint.get(lid, {}) or moved_coords:
            bp = T.paints[pa_]
            if isinstance(fill, dict) and "bevel" in fill:
                b = dict(fill["bevel"])
                of_base = bp[6] if isinstance(base_fill, dict) and "bevel" in base_fill and base_fill["bevel"].get("of") else -1
                b.pop("of", None)
                S_for_grid = PE.expand(base_fill["bevel"]["of"]) if of_base >= 0 else PE.expand(L["shape"])
                pa_ = T.paint({"bevel": b}, S_for_grid)
                if of_base >= 0:
                    of = _move_node(T, of_base, dx, dy) if (dx or dy) else of_base
                    kind, rgb, Lb, sa, sn, ca, _, g = T.paints[pa_]
                    T.paints[pa_] = (kind, rgb, Lb, sa, sn, ca, of, g)
            else:
                pa_ = T.paint(STS.move_paint(fill, dx, dy), PE.expand(L["shape"]))
        if "alpha" in paint.get(lid, {}):
            al = eff.get("alpha", 1.0)
        if "soft" in paint.get(lid, {}):
            so = eff.get("soft", -1.0)
        if "falloff" in paint.get(lid, {}) or ((dx or dy) and "falloff" in eff):
            ff = eff.get("falloff")
            f = (1, ff["c"][0] + dx, ff["c"][1] + dy, ff["r"], ff.get("pow", 2.0)) if ff else (0, 0.0, 0.0, 0.0, 0.0)
        if "fx" in paint.get(lid, {}) or ((dx or dy) and any(e["t"] == "tone" for e in eff.get("fx", ()))):
            fxa, fxn = _fx_rows(T, eff.get("fx", ()), dx, dy, PE.FX)
        T.layers.append((sh, cl, pa_, al, so, f, (fxa, fxn)))
        patch_rows.append((0, rel, len(T.layers) - 1, (0.0, 0.0, 0.0, 0.0)))
    for k, box in A.get("box", {}).items():
        bi = int(k)
        if bi in M.item_rel:
            patch_rows.append((1, M.item_rel[bi], -1, tuple(float(v) for v in box)))
    return pa, len(patch_rows) - pa
