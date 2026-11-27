"""uivec states, step 3 (P41S, log/roadmap/P41/STATES_BRIEF.md): for every element whose approved states are NOT an exact alteration of
one base, the UNIFIED version (the base's geometry, each state's paint, moves snapped to the whole pixels the art moves by, a shadow's
offset / softness / spread / alpha kept) and a comparison image for Mariano. Nothing here changes an approved scene.

    py -3.12 re/tools/uivec/states_judge.py [--area <a,b>] [--id <id,...>]
        -> log/roadmap/P41/work8/<area>/<id>.png, <id>.json (the unified element file), results.tsv

THE IMAGE (one column per state, the element's states side by side):
  row 1  the art (the roster's crop of that state, log/roadmap/P41/roster/atlas_*.png)
  row 2  the approved drawing of that state
  row 3  the unified drawing (base + alteration)
  row 4  approved vs unified: the absolute difference x4 (black = the same)
  last   the overlay of every unified state of the variant (their mean: where they coincide it is sharp)
"""
import csv
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import states as S  # noqa: E402

ROOT = S.ROOT
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work8")
ROSTER = os.path.join(ROOT, "log", "roadmap", "P41", "roster")
_atlas = {}


def roster():
    with open(os.path.join(ROSTER, "roster.json"), encoding="utf-8") as f:
        return {e["id"]: e for e in json.load(f)["elements"]}


def atlas_crop(e, state, k):
    """the roster's crop of a state (by name, else by order), RGB float, or None"""
    if not e or not e.get("crops"):
        return None
    cs = e["crops"]
    c = next((c for c in cs if c.get("state") == state), cs[k] if k < len(cs) else None)
    if c is None:
        return None
    a = c["atlas"]
    if a not in _atlas:
        _atlas[a] = np.asarray(Image.open(os.path.join(ROSTER, f"atlas_{a:02d}.png")).convert("RGB"), float)
    im = _atlas[a]
    return im[c["y"]:c["y"] + c["h"], c["x"]:c["x"] + c["w"]]


def zoom_for(w, h):
    return int(max(2, min(8, math.floor(200 / max(w, h, 1)))))


def tile(img, z, w, h):
    """img scaled by z (nearest) into a w*z x h*z cell (padded / cut, grey ground)"""
    out = np.full((h * z, w * z, 3), 200.0)
    if img is None:
        return out
    big = np.kron(img, np.ones((z, z, 1)))
    hh, ww = min(big.shape[0], h * z), min(big.shape[1], w * z)
    out[:hh, :ww] = big[:hh, :ww]
    return out


def render_on(items, rect, z, bg=(198.0, 198.0, 214.0)):
    return S.render([it for it in items if "layers" in it], rect, float(z), np.asarray(bg))


def label(img, text):
    im = Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8))
    dr = ImageDraw.Draw(im)
    x0, y0, x1, y1 = dr.textbbox((3, 2), text)
    dr.rectangle((x0 - 2, y0 - 1, x1 + 2, y1 + 1), fill=(255, 255, 255))
    dr.text((3, 2), text, fill=(0, 0, 0))
    return np.asarray(im, float)


def judge_element(area, eid, recs, ros):
    """(unified element dict, result rows, image) of one element whose derive found not-exact states"""
    elem, rows = S.derive_element(area, eid, recs)
    by = {r[0]: r for r in recs}
    bad = [r["state"] for r in rows if r["status"] == "not-exact"]
    if not bad:
        return None, [], None
    out_rows = []
    cols = []
    uni_elem = {"id": eid, "area": area, "source": "unified (round 8, awaiting judgement)", "variants": {}, "states": {}}
    for vname, var in elem["variants"].items():
        members = [n for n in elem["states"] if elem["states"][n]["variant"] == vname]
        if not any(n in bad for n in members):
            continue
        base = var["base"]
        b = by[base]
        brect = S._scene_rect(b[2], b[3])
        sts, rects, origins = [], {}, {}
        for n in members:
            r = by[n]
            rr = S._scene_rect(r[2], r[3])
            o = [0.0, 0.0] if json.dumps(r[4], sort_keys=True) == json.dumps(b[4], sort_keys=True) else \
                [float(rr[0] - brect[0]), float(rr[1] - brect[1])]
            sts.append((n, S._moved_items(r[4], o) if o != [0.0, 0.0] else r[4]))
            rects[n] = [rr[0] - o[0], rr[1] - o[1], rr[2], rr[3]]
            origins[n] = o
        uvar, rep = S.derive(sts, base, rects, origins, unify=True)
        named = dict(sts)
        uni = {}
        for n in members:
            oa = rep[n].get("origin_add")
            items = S.apply(uvar, n)
            appr = named[n]
            rect = rects[n]
            if oa:
                appr = S._moved_items(appr, oa)
                rect = [rect[0] - oa[0], rect[1] - oa[1], rect[2], rect[3]]
                origins[n] = [origins[n][0] + oa[0], origins[n][1] + oa[1]]
            uni[n] = (items, appr, rect)
            m = S.check(items, appr, rect)
            r = rep[n]
            geo = sorted(r.get("geom", []), key=lambda g: -g["residual"])
            changed = []
            if geo:
                changed.append("geometry taken from the base: " + ", ".join(f"{g['layer']} {g['residual']:g} px" for g in geo[:6])
                               + (f" (+{len(geo) - 6} layers)" if len(geo) > 6 else ""))
            if r.get("moves"):
                changed.append("moves " + ", ".join(f"({d[0]:g}, {d[1]:g}) x{c}" for d, c in r["moves"]))
            if r.get("spread"):
                changed.append("shadow spread " + ", ".join(f"{k} {v:g}" for k, v in r["spread"].items()))
            unsure = []
            if r.get("on") and r.get("off"):
                unsure.append(f"layers only this state draws ({', '.join(r['on'])}) replace base layers ({', '.join(r['off'])}): "
                              "kept as this state's own, ask whether the base's should be used")
            elif r.get("on"):
                unsure.append(f"layers only this state draws: {', '.join(r['on'])}")
            elif r.get("off"):
                unsure.append(f"base layers this state leaves out: {', '.join(r['off'])}")
            st0 = next(x["status"] for x in rows if x["state"] == n)
            out_rows.append({"id": eid, "state": n, "variant": vname or "-", "base": base, "was": st0,
                             "status": "base" if n == base else ("unchanged" if max(m) <= S.TOL_LEVELS else "unified"),
                             "max_vs_approved_1x": round(m[0], 2), "max_vs_approved_2.7x": round(m[1], 2),
                             "changed": "; ".join(changed), "unsure": "; ".join(unsure)})
            uni_elem["states"][n] = {"variant": vname, "origin": origins[n]}
        uni_elem["variants"][vname] = uvar
        # the image columns of this variant
        x0 = min(v[2][0] for v in uni.values())
        y0 = min(v[2][1] for v in uni.values())
        x1 = max(v[2][0] + v[2][2] for v in uni.values())
        y1 = max(v[2][1] + v[2][3] for v in uni.values())
        R = [x0, y0, x1 - x0, y1 - y0]
        z = zoom_for(R[2], R[3])
        W, H = int(math.ceil(R[2] * z)), int(math.ceil(R[3] * z))
        mean = np.zeros((H, W, 3))
        e = ros.get(eid)
        if e is None and area == "title":   # the title / difficulty buttons: the roster crops their pad (title-play-pad, ...)
            e = ros.get(eid.replace(".", "-") + "-pad")
        for k, n in enumerate(members):
            items, appr, rect = uni[n]
            art = atlas_crop(e, n, names_index(recs, n))
            a = render_on(appr, R, z)[:H, :W]
            u = render_on(items, R, z)[:H, :W]
            d = np.clip(np.abs(a - u).max(axis=2, keepdims=True) * 4, 0, 255).repeat(3, axis=2)
            mean += u
            col = [label(tile(art, z, int(math.ceil(R[2])), int(math.ceil(R[3])))[:H, :W], f"art {n}"),
                   label(a, "approved"), label(u, "unified"), label(d, "diff x4")]
            cols.append((vname, col))
        cols.append((vname, [label(mean / len(members), f"overlay {vname or 'states'}")]))
    img = compose(cols)
    return uni_elem, out_rows, img


def names_index(recs, n):
    return [r[0] for r in recs].index(n)


def compose(cols):
    gap = 6
    blocks = []
    for vname, col in cols:
        w = max(c.shape[1] for c in col)
        parts = []
        for c in col:
            parts.append(np.pad(c, ((0, gap), (0, w - c.shape[1]), (0, 0)), constant_values=255))
        blocks.append(np.concatenate(parts))
    H = max(b.shape[0] for b in blocks)
    blocks = [np.pad(b, ((0, H - b.shape[0]), (0, gap), (0, 0)), constant_values=255) for b in blocks]
    return np.concatenate(blocks, axis=1)


COLS = ["id", "state", "variant", "base", "was", "status", "max_vs_approved_1x", "max_vs_approved_2.7x", "changed", "unsure"]


def main(argv):
    areas = argv[argv.index("--area") + 1].split(",") if "--area" in argv else S.AREAS
    only = argv[argv.index("--id") + 1].split(",") if "--id" in argv else None
    if any(a not in ("frontend", "title") for a in areas):
        import port_game as PG
        PG.load_area_kinds()
        PG._area_kinds()
    ros = roster()
    for area in areas:
        recs = S.area_records(area, "approved")
        # an element redone in round 7 and not judged yet is judged on its round-7 drawing (pending-round-7)
        w7 = S.work7_overrides(area)
        recs7 = S.area_records(area, "work7") if w7 else {}
        d = os.path.join(OUT, area)
        os.makedirs(d, exist_ok=True)
        allrows = []
        ids = [eid for eid, rs in recs.items() if len(rs) > 1 and (not only or eid in only)]
        for k, eid in enumerate(ids):
            src = recs7[eid] if eid in w7 else recs[eid]
            uni, rows, img = judge_element(area, eid, src, ros)
            if uni is not None and eid in w7:
                uni["source"] = "unified from the round-7 drawing (pending-round-7), awaiting judgement"
                for r in rows:
                    r["was"] = r["was"] + " (round 7)"
            if uni is None:
                continue
            with open(os.path.join(d, eid + ".json"), "w", encoding="utf-8", newline="\n") as f:
                json.dump(uni, f, separators=(",", ":"))
            Image.fromarray(np.clip(img + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(d, eid + ".png"))
            allrows += rows
            n_un = sum(r["status"] == "unified" for r in rows)
            print(f"[{k + 1}/{len(ids)}] {area}/{eid}: {n_un} unified states", flush=True)
        if allrows:
            p = os.path.join(d, "results.tsv")
            with open(p, "w", encoding="utf-8", newline="\n") as f:
                w = csv.DictWriter(f, COLS, delimiter="\t", lineterminator="\n")
                w.writeheader()
                for r in allrows:
                    w.writerow(r)
            print(f"wrote {p} ({len(allrows)} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
