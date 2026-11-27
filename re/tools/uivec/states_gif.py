"""uivec states_gif (P41S follow-up): one animated GIF per multi-state element (and per variant) so a position shift between
states is visible.

    py -3.12 re/tools/uivec/states_gif.py [--round r7|r8|r9|all] [--area <area>] [--id <id>] [--out <dir>]

Round 8 (log/roadmap/P41/work8/<area>/<id>.json, the unified element files): LEFT = the approved per-state scenes, RIGHT = the
unified element (states.apply).  Round 7 (the pending scenes in log/roadmap/P41/work7/<area>, plus the hud-speed scene files):
LEFT = the approved scenes of the same state where one exists (else the panel is left out), RIGHT = the round-7 scene.

Every state is put into its variant's BASE frame (the state's cut corner minus the base's cut corner is taken off, exactly the
"origin" of an element file), so the canvas never moves: one rectangle per element (the union of every state's bounds, both
sides, plus 3 px), an integer zoom that gives the short side >= 240 px (rendered by sdf at that scale), neutral ground
(198, 198, 214), a 1 px cross through the centre of the idle state's bounds and its outline (50% alpha, the same in every frame).
The state name is in a label strip above each panel (outside the canvas, so it never covers the button).
Output: <out>/<round>/<area>/<id>[_<variant>].gif and <out>/index.tsv (default out = log/roadmap/P41/gifs).
"""
import argparse
import glob
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import states as S  # noqa: E402
import sdf  # noqa: E402

ROOT = S.ROOT
GROUND = (198.0, 198.0, 214.0)
MARGIN = 3
MIN_SHORT = 240
MAX_BYTES = 1_500_000
DURATION = 800
ORDER = ["idle", "lit", "hover", "pressed", "selected", "active", "disabled", "dim"]
STRIP = 18
GAPPX = 8


def rank(name):
    n = name.lower()
    for i, w in enumerate(ORDER):
        if w in n.split() or n == w:
            return i
    return len(ORDER)


def order_states(names):
    return sorted(names, key=lambda n: (rank(n), names.index(n)))


def bounds_of(items):
    bs = [it["box"] for it in items if it.get("box")]
    if not bs:
        return None
    return [min(b[0] for b in bs), min(b[1] for b in bs), max(b[2] for b in bs), max(b[3] for b in bs)]


def union(bl):
    bl = [b for b in bl if b]
    if not bl:
        return None
    return [min(b[0] for b in bl), min(b[1] for b in bl), max(b[2] for b in bl), max(b[3] for b in bl)]


# ----------------------------------------------------------------------------------------------------------- scene sets
def frame_from_records(recs, names, base):
    """{state: items in the base frame} from records [(state, sheet, cut, shift, items, extra)]"""
    by = {r[0]: r for r in recs}
    if base not in by:
        return {}
    b = S._scene_rect(by[base][2], by[base][3])
    out = {}
    for n in names:
        if n not in by:
            continue
        r = by[n]
        rr = S._scene_rect(r[2], r[3])
        out[n] = S._moved_items(r[4], [rr[0] - b[0], rr[1] - b[1]])
    return out


def pick_base(names):
    for n in names:
        if rank(n) == 0:
            return n
    return names[0]


def groups_from_file(elem, rec_names):
    """{variant: [state names in record/file order]} using the element file's variants where it names the state"""
    g = {}
    seen = set()
    for st, ref in elem["states"].items():
        g.setdefault(ref["variant"], []).append(st)
        seen.add(st)
    for n in rec_names:
        if n not in seen:
            g.setdefault(S.parse_state(n)[0], []).append(n)
    return g


def r8_jobs(area_f=None, id_f=None):
    appr = {}
    jobs = []
    for p in sorted(glob.glob(os.path.join(ROOT, "log", "roadmap", "P41", "work8", "*", "*.json"))):
        elem = json.load(open(p, encoding="utf-8"))
        area, eid = elem["area"], elem["id"]
        if (area_f and area != area_f) or (id_f and eid != id_f):
            continue
        if area not in appr:
            appr[area] = S.area_records(area)
        recs = appr[area].get(eid, [])
        for var, names in groups_from_file(elem, [r[0] for r in recs]).items():
            vr = elem["variants"].get(var)
            if vr is None:
                continue
            base = vr["base"]
            after = {}
            for n in names:
                if n in vr["states"]:
                    after[n] = S.apply(vr, n)
            before = frame_from_records(recs, names, base)
            jobs.append(dict(round="r8", area=area, id=eid, variant=var, names=[n for n in names if n in after],
                             base=base, before=before, after=after))
    return jobs


def r7_jobs(area_f=None, id_f=None):
    jobs = []
    for area in ("frontend", "hud_palettes", "hud_panels"):
        if area_f and area != area_f:
            continue
        new = S.area_records(area, "work7")
        old = S.area_records(area)
        ov = S.work7_overrides(area)
        for eid in sorted(new):
            if eid not in ov or (id_f and eid != id_f):
                continue
            recs = new[eid]
            if len(recs) < 2:
                continue
            ef = os.path.join(S.STATES_DIR, area + "_work7", eid + ".json")
            if os.path.exists(ef):
                groups = groups_from_file(json.load(open(ef, encoding="utf-8")), [r[0] for r in recs])
            else:
                groups = {}
                for r in recs:
                    groups.setdefault(S.parse_state(r[0])[0], []).append(r[0])
            have = {r[0] for r in recs}
            for var, names in groups.items():
                names = [n for n in names if n in have]
                if len(names) < 2:
                    continue
                base = pick_base(names)
                after = frame_from_records(recs, names, base)
                before = frame_from_records(old.get(eid, []), names, base) if eid in old else {}
                jobs.append(dict(round="r7", area=area, id=eid, variant=var, names=names, base=base,
                                 before=before, after=after))
    # hud-speed: screen-pixel scene files, three speeds x idle / hover, no approved drawing to compare with
    if not area_f or area_f == "hud_palettes":
        d = os.path.join(ROOT, "log", "roadmap", "P41", "work7", "hud_palettes")
        for sp in (1, 2, 4):
            eid = "hud-speed"
            if id_f and id_f != eid:
                continue
            st = {}
            for state in ("idle", "hover"):
                p = os.path.join(d, f"hud-speed-{sp}x" + ("-hover" if state == "hover" else "") + ".json")
                if os.path.exists(p):
                    st[state] = json.load(open(p, encoding="utf-8"))["scene"]
            if len(st) == 2:
                jobs.append(dict(round="r7", area="hud_palettes", id=eid, variant=f"{sp}x", names=["idle", "hover"],
                                 base="idle", before={}, after=st))
    return jobs


def r9_jobs(area_f=None, id_f=None):
    """round 9 (log/roadmap/P41/work9/<area>/<id>.json, the two fixes of rounds 7 / 8): an element file (variants) is drawn through
    states.apply; a scene file through its records. LEFT = what Mariano judged (the round-8 unified element where one exists, else
    the round-7 scene), RIGHT = the round-9 version."""
    jobs = []
    for p in sorted(glob.glob(os.path.join(ROOT, "log", "roadmap", "P41", "work9", "*", "*.json"))):
        if p.endswith(("_scenes.json", "_check.json")):
            continue
        area = os.path.basename(os.path.dirname(p))
        doc = json.load(open(p, encoding="utf-8"))
        eid = doc["id"]
        if (area_f and area != area_f) or (id_f and eid != id_f):
            continue
        if "variants" in doc:
            p8 = os.path.join(ROOT, "log", "roadmap", "P41", "work8", area, eid + ".json")
            e8 = json.load(open(p8, encoding="utf-8")) if os.path.exists(p8) else None
            for var, vr in doc["variants"].items():
                names = [n for n, r in doc["states"].items() if r["variant"] == var and n in vr["states"]]
                after = {n: S._moved_items(S.apply(vr, n), [0, 0]) for n in names}
                before = {}
                if e8 and var in e8["variants"]:
                    v8 = e8["variants"][var]
                    before = {n: S.apply(v8, n) for n in names if n in v8["states"]}
                jobs.append(dict(round="r9", area=area, id=eid, variant=var, names=names, base=vr["base"], before=before,
                                 after=after))
        else:
            new = S.in_game_records(area, {eid: p})[eid] if area != "frontend" else S.frontend_records({eid: p})[eid]
            ov = S.work7_overrides(area)
            old = (S.in_game_records(area, {eid: ov[eid]}) if eid in ov else S.area_records(area)).get(eid, [])
            names = [r[0] for r in new]
            base = pick_base(names)
            jobs.append(dict(round="r9", area=area, id=eid, variant="", names=names, base=base,
                             before=frame_from_records(old, names, base), after=frame_from_records(new, names, base)))
    return jobs


# ----------------------------------------------------------------------------------------------------------- rendering
def font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:
        return ImageFont.load_default()


def draw_guides(img, idle_items, rect, s):
    """cross through the idle bounds' centre and its outline, 50% alpha, 1 px"""
    b = bounds_of(idle_items) if idle_items else None
    if not b:
        return
    H, W = img.shape[:2]
    cx, cy = (b[0] + b[2]) / 2, (b[1] + b[3]) / 2
    X = int(round((cx - rect[0]) * s - 0.5))
    Y = int(round((cy - rect[1]) * s - 0.5))
    X0, X1 = int(round((b[0] - rect[0]) * s)), int(round((b[2] - rect[0]) * s)) - 1
    Y0, Y1 = int(round((b[1] - rect[1]) * s)), int(round((b[3] - rect[1]) * s)) - 1
    mag = np.array([255.0, 0.0, 160.0])
    grn = np.array([0.0, 150.0, 0.0])

    def mix(ys, xs, col):
        img[ys, xs] = img[ys, xs] * 0.5 + col * 0.5
    if 0 <= X < W:
        mix(slice(0, H), X, mag)
    if 0 <= Y < H:
        mix(Y, slice(0, W), mag)
    for yy in (Y0, Y1):
        if 0 <= yy < H:
            mix(yy, slice(max(X0, 0), min(X1 + 1, W)), grn)
    for xx in (X0, X1):
        if 0 <= xx < W:
            mix(slice(max(Y0, 0), min(Y1 + 1, H)), xx, grn)


def render_panel(items, idle_items, rect, s):
    w, h = rect[2], rect[3]
    W, H = int(math.ceil(w * s)), int(math.ceil(h * s))
    img = np.empty((H, W, 3))
    img[:] = GROUND
    if items:
        sdf.render([it for it in items if "layers" in it], img, s, origin=(rect[0], rect[1]))
    img = np.clip(img, 0, 255)
    draw_guides(img, idle_items, rect, s)
    return img


def build(job, s_force=None):
    names = order_states(job["names"])
    sides = [("before", job["before"]), ("after", job["after"])] if job["before"] else [("after", job["after"])]
    allb = []
    for _, sc in sides:
        for n in names:
            if n in sc:
                allb.append(bounds_of(sc[n]))
    u = union(allb)
    if not u:
        return None
    x0, y0 = math.floor(u[0]) - MARGIN, math.floor(u[1]) - MARGIN
    x1, y1 = math.ceil(u[2]) + MARGIN, math.ceil(u[3]) + MARGIN
    rect = [x0, y0, x1 - x0, y1 - y0]
    s = s_force or max(1, math.ceil(MIN_SHORT / min(rect[2], rect[3])))
    idle = {k: sc.get(job["base"]) or (sc.get(names[0])) for k, sc in sides}
    fnt = font(12)
    frames = []
    for n in names:
        panels = []
        for k, sc in sides:
            im = Image.fromarray(render_panel(sc.get(n), idle[k], rect, s).round().astype(np.uint8))
            lab = Image.new("RGB", (im.width, STRIP), (40, 40, 48))
            d = ImageDraw.Draw(lab)
            d.text((4, 2), f"{'before' if k == 'before' else ('new' if len(sides) == 2 else 'new')}: {n}"
                   + ("" if sc.get(n) else "  (none)"), fill=(235, 235, 235), font=fnt)
            p = Image.new("RGB", (im.width, STRIP + im.height))
            p.paste(lab, (0, 0))
            p.paste(im, (0, STRIP))
            panels.append(p)
        W = sum(p.width for p in panels) + GAPPX * (len(panels) - 1)
        fr = Image.new("RGB", (W, panels[0].height), (40, 40, 48))
        x = 0
        for p in panels:
            fr.paste(p, (x, 0))
            x += p.width + GAPPX
        frames.append(fr)
    return frames, rect, s, names


def save_gif(frames, path):
    tall = Image.new("RGB", (frames[0].width, frames[0].height * len(frames)))
    for i, f in enumerate(frames):
        tall.paste(f, (0, i * f.height))
    pal = tall.quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    q = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    q[0].save(path, save_all=True, append_images=q[1:], duration=DURATION, loop=0, optimize=True, disposal=1)
    return os.path.getsize(path)


def make(job, out):
    s = None
    r = build(job)
    if r is None:
        return None
    while True:
        frames, rect, s, names = r
        sub = job["id"] + (("_" + job["variant"].replace(" ", "_")) if job["variant"] else "")
        path = os.path.join(out, job["round"], job["area"], sub + ".gif")
        size = save_gif(frames, path)
        if size <= MAX_BYTES or s <= 1:
            break
        r = build(job, s - 1)
    return dict(round=job["round"], area=job["area"], id=job["id"], variant=job["variant"] or "-",
                states=" > ".join(names), size=f"{frames[0].width}x{frames[0].height}", scale=s, bytes=size,
                file=os.path.relpath(path, ROOT).replace("\\", "/"))


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--round", default="all")
    ap.add_argument("--area")
    ap.add_argument("--id")
    ap.add_argument("--out", default=os.path.join(ROOT, "log", "roadmap", "P41", "gifs"))
    a = ap.parse_args(argv)
    import port_game as PG
    PG.load_area_kinds()   # the areas' shape kinds (hud_panels.limb, popups.sinter, rm.arc ...) for sdf.render
    jobs = []
    if a.round in ("r8", "all"):
        jobs += r8_jobs(a.area, a.id)
    if a.round in ("r7", "all"):
        jobs += r7_jobs(a.area, a.id)
    if a.round == "r9":
        jobs += r9_jobs(a.area, a.id)
    rows, fails = [], []
    for i, j in enumerate(jobs):
        try:
            r = make(j, a.out)
        except Exception as e:  # noqa: BLE001
            fails.append((j["round"], j["area"], j["id"], j["variant"], repr(e)))
            print(f"[{i + 1}/{len(jobs)}] {j['id']} {j['variant']} ERROR {e!r}", flush=True)
            continue
        if r is None:
            fails.append((j["round"], j["area"], j["id"], j["variant"], "no bounds"))
            print(f"[{i + 1}/{len(jobs)}] {j['id']} {j['variant']} NONE", flush=True)
            continue
        rows.append(r)
        print(f"[{i + 1}/{len(jobs)}] {j['round']} {j['id']} {j['variant']} PASS {r['size']} x{r['scale']} {r['bytes']}", flush=True)
    idx = os.path.join(a.out, "index.tsv")
    old = []
    if (a.area or a.id or a.round != "all") and os.path.exists(idx):
        keep = {(r["round"], r["area"], r["id"], r["variant"]) for r in rows}
        for ln in open(idx, encoding="utf-8").read().splitlines()[1:]:
            c = ln.split("\t")
            if (c[0], c[1], c[2], c[3]) not in keep:
                old.append(c)
    os.makedirs(a.out, exist_ok=True)
    with open(idx, "w", encoding="utf-8", newline="\n") as f:
        f.write("round\tarea\tid\tvariant\tstates\tframe_size\tfile\n")
        for c in old:
            f.write("\t".join(c) + "\n")
        for r in rows:
            f.write(f"{r['round']}\t{r['area']}\t{r['id']}\t{r['variant']}\t{r['states']}\t{r['size']}\t{r['file']}\n")
    tot = sum(r["bytes"] for r in rows)
    print(f"{len(rows)} gifs, {tot / 1e6:.1f} MB, {len(fails)} failed")
    for x in fails:
        print("FAILED", *x)


if __name__ == "__main__":
    main(sys.argv[1:])
