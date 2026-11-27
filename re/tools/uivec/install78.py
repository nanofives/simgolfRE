"""uivec: install Mariano's judgement of rounds 7 and 8 (2026-11-27, log/roadmap/P41/INSTALL78_BRIEF.md, notes
log/roadmap/P41/install78/notes.md) into the approved scenes, re/tools/uivec/scenes/<area>/<id>.json, on top of what
collect_scenes.py (rounds 1-6) and the wide / round-6 steps put there.

  round 7  (log/roadmap/P41/work7/<area>/, cards <area>--<id>): the drawing replaces the approved scene; the five frontend
           *-backdrop scenes carry the round-7 idle Back arrows (no card of their own: approved with their buttons); the six
           hud-speed scenes (screen pixels of the 800 x 600 root, no sheet) become three elements hud-speed-<1x|2x|4x> with
           states idle / hover on the pseudo sheet "@root" (gfx_hud.cpp draws them by id, never by sprite provenance)
  round 8  (log/roadmap/P41/work8/<area>/<id>.json, cards r8--<area>--<id>): the UNIFIED element file; each of its states is
           written as that state's scene: states.apply(variant, state) moved by the state's origin (the element's base cut
           coordinates + origin = the state's own sheet coordinates); the element's other states keep their scenes. Round 8
           supersedes round 7 where both exist (the two stats buttons). The title / difficulty buttons (area "title") go to
           scenes/title/<id>.json (read by port_export.scenes / diff_scenes instead of title.py's hover drawing).
  round 9  (log/roadmap/P41/work9/<area>/): the two cards Mariano sent back: hud_panels golfer-tab-photo (a scene file: the
           glyph centred) and hud_palettes hud-pause (one drawing, idle + yellow paint: work9 hud-pause_scenes.json).
The approved unified element files are kept beside the scenes (re/tools/uivec/unified/<area>/<id>.json, numbers only): states.py
derive uses them as the element's alteration (a state Mariano approved as base + alteration is exact by construction, also when
it switches a base layer for one of its own, which derive would never propose by itself).

    py -3.12 re/tools/uivec/install78.py [--dry-run]     -> scenes/, unified/, scenes/manifest.tsv; prints what changed
"""
import copy
import csv
import glob
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import states as S  # noqa: E402

ROOT = S.ROOT
P41 = os.path.join(ROOT, "log", "roadmap", "P41")
SCENES = os.path.join(HERE, "scenes")
UNIFIED = os.path.join(HERE, "unified")
JUDGE = os.path.join(P41, "judge", "fb10", "judge7")
MANIFEST = os.path.join(SCENES, "manifest.tsv")
ROOT_SHEET = "@root"
FRONTEND_BACKDROPS = ["diff-backdrop", "load-backdrop", "pro-backdrop", "theme-backdrop", "world-backdrop"]


def rel(p):
    return os.path.relpath(p, ROOT).replace("\\", "/")


def marks():
    out = {}
    for p in glob.glob(os.path.join(JUDGE, "*.json")):
        k = os.path.basename(p)[:-5]
        out[k] = json.load(open(p, encoding="utf-8"))["verdict"]
    return out


def dump(path, doc, dry):
    if dry:
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f)


def _bbox(items):
    x0 = min(it["box"][0] for it in items)
    y0 = min(it["box"][1] for it in items)
    x1 = max(it["box"][2] for it in items)
    y1 = max(it["box"][3] for it in items)
    return [math.floor(x0), math.floor(y0), math.ceil(x1) - math.floor(x0), math.ceil(y1) - math.floor(y0)]


def speed_elements():
    """{eid: doc} the three speed buttons as hud_palettes elements on the pseudo sheet @root (cut = the bounds of both states)"""
    d = os.path.join(P41, "work7", "hud_palettes")
    out = {}
    for sp in (1, 2, 4):
        st = {}
        for state, suf in (("idle", ""), ("hover", "-hover")):
            p = os.path.join(d, f"hud-speed-{sp}x{suf}.json")
            st[state] = json.load(open(p, encoding="utf-8"))
        cut = _bbox(st["idle"]["scene"] + st["hover"]["scene"])
        out[f"hud-speed-{sp}x"] = (
            {"id": f"hud-speed-{sp}x", "coords": "source sheet pixels",
             "note": ("the port's own speed button (P33A, no art): scenes in screen pixels of the 800 x 600 root; the pseudo sheet "
                      "@root is never a sprite's sheet, gfx_hud.cpp draws the record by id (uvg_find) at no offset"),
             "states": {s: {"sheet": ROOT_SHEET, "cut": cut, "scene": st[s]["scene"]} for s in ("idle", "hover")}},
            [rel(os.path.join(d, f"hud-speed-{sp}x{suf}.json")) for suf in ("", "-hover")])
    return out


def state_scene(elem, st):
    """the state's scene in its own coordinates (base cut coordinates + origin)"""
    ref = elem["states"][st]
    items = S.apply(elem["variants"][ref["variant"]], st)
    o = ref.get("origin", [0, 0])
    return S._moved_items(items, [-o[0], -o[1]]) if o != [0, 0] and o != [0.0, 0.0] else items


def put_state(area, doc, st, items):
    """writes a state's scene into the area's scene file format"""
    if area == "frontend":
        for s in doc["states"]:
            if s["name"] == st:
                s["scene"] = items
                return
        raise KeyError(st)
    if area in ("hud_palettes", "hud_panels", "popups", "reports_maps"):
        doc["states"][st]["scene"] = items
        return
    raise ValueError(area)


def main(argv):
    dry = "--dry-run" in argv
    mk = marks()
    bad = [k for k, v in mk.items() if v != "ok"]
    fixes = {"hud_panels--golfer-tab-photo", "r8--hud_palettes--hud-pause"}
    assert set(bad) == fixes, bad
    with open(MANIFEST, encoding="utf-8") as f:
        man = list(csv.DictReader(f, delimiter="\t"))
    mrow = {(r["area"], r["id"]): r for r in man}
    done = []

    def set_row(area, eid, rnd, src):
        r = mrow.get((area, eid))
        if r is None:
            r = {"area": area, "id": eid, "round": "", "source": ""}
            man.append(r)
            mrow[(area, eid)] = r
        r["round"], r["source"] = str(rnd), src
        done.append((area, eid, rnd))

    # ---- round 7
    for key in sorted(mk):
        if key.startswith("r8--"):
            continue
        area, eid = key.split("--", 1)
        if eid.startswith("hud-speed"):
            continue
        if key in fixes:
            src = os.path.join(P41, "work9", area, eid + ".json")
            rnd = 9
        else:
            src = os.path.join(P41, "work7", area, eid + ".json")
            rnd = 7
        assert os.path.exists(src), src
        doc = json.load(open(src, encoding="utf-8"))
        dump(os.path.join(SCENES, area, eid + ".json"), doc, dry)
        set_row(area, eid, rnd, rel(src))
    for eid in FRONTEND_BACKDROPS:
        src = os.path.join(P41, "work7", "frontend", eid + ".json")
        dump(os.path.join(SCENES, "frontend", eid + ".json"), json.load(open(src, encoding="utf-8")), dry)
        set_row("frontend", eid, 7, rel(src))
    for eid, (doc, srcs) in speed_elements().items():
        dump(os.path.join(SCENES, "hud_palettes", eid + ".json"), doc, dry)
        set_row("hud_palettes", eid, 7, " + ".join(srcs))
    # ---- round 8 (and round 9's pause, which replaces its round-8 card)
    titles = {}
    for key in sorted(mk):
        if not key.startswith("r8--"):
            continue
        _, area, eid = key.split("--", 2)
        if key in fixes:   # hud-pause: round 9's two scenes (one drawing, idle + yellow body)
            src = os.path.join(P41, "work9", area, "hud-pause_scenes.json")
            doc = json.load(open(src, encoding="utf-8"))
            dump(os.path.join(SCENES, area, eid + ".json"), doc, dry)
            uni = json.load(open(os.path.join(P41, "work9", area, eid + ".json"), encoding="utf-8"))
            dump(os.path.join(UNIFIED, area, eid + ".json"), uni, dry)
            set_row(area, eid, 9, rel(src) + " (element " + rel(os.path.join(P41, "work9", area, eid + ".json")) + ")")
            continue
        usrc = os.path.join(P41, "work8", area, eid + ".json")
        uni = json.load(open(usrc, encoding="utf-8"))
        dump(os.path.join(UNIFIED, area, eid + ".json"), uni, dry)
        if area == "title":
            titles[eid] = (uni, usrc)
            continue
        p = os.path.join(SCENES, area, eid + ".json")
        doc = json.load(open(p, encoding="utf-8")) if not dry or os.path.exists(p) else None
        if dry and (area, eid) in {(a, e) for a, e, _ in done}:   # (a dry run did not write round 7's copy)
            doc = json.load(open(os.path.join(P41, "work7", area, eid + ".json"), encoding="utf-8"))
        for st in uni["states"]:
            put_state(area, doc, st, state_scene(uni, st))
        doc["round8"] = f"states {', '.join(uni['states'])} from the approved unified element {rel(usrc)} (states.apply + origin)"
        dump(p, doc, dry)
        prev = mrow.get((area, eid), {}).get("source", "")
        set_row(area, eid, 8, rel(usrc) + (f" over {prev}" if prev and "work8" not in prev else ""))
    for eid, (uni, usrc) in sorted(titles.items()):
        doc = {"id": eid, "coords": "screen pixels (the title / difficulty screen)",
               "note": "states from the approved unified element (states.apply + origin); port_export.scenes / diff_scenes read them",
               "states": {st: {"scene": state_scene(uni, st)} for st in uni["states"]}}
        dump(os.path.join(SCENES, "title", eid + ".json"), doc, dry)
        set_row("title", eid, 8, rel(usrc))
    if not dry:
        with open(MANIFEST, "w", encoding="utf-8", newline="\n") as f:
            w = csv.DictWriter(f, ["area", "id", "round", "source"], delimiter="\t", lineterminator="\n")
            w.writeheader()
            for r in man:
                w.writerow(r)
    cnt = {}
    for a, e, r in done:
        cnt[r] = cnt.get(r, 0) + 1
    print(("[dry run] " if dry else "") + f"{len(done)} installs: " + ", ".join(f"round {k}: {v}" for k, v in sorted(cnt.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
