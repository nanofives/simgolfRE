"""P41 area hud_palettes: shared helpers (element crops in sheet coordinates, comparison images, scene json).

Coordinates: every scene of this area is in the SOURCE SHEET's pixel coordinates (pixel (i, j) covers [i, i+1) x [j, j+1)),
so a scene can be checked against its cut directly. Each element's json records the sheet and the cut rectangle per state.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
UIVEC = os.path.normpath(os.path.join(HERE, "..", ".."))
ROOT = os.path.normpath(os.path.join(UIVEC, "..", "..", ".."))
ROSTER = os.path.join(ROOT, "log", "roadmap", "P41", "roster")
OUT1 = os.path.join(ROOT, "log", "roadmap", "P41", "work", "hud_palettes")      # round 1 (left as it is)
OUT2 = os.path.join(ROOT, "log", "roadmap", "P41", "work2", "hud_palettes")     # fix round 2 (left as it is)
OUT = os.path.join(ROOT, "log", "roadmap", "P41", "work3", "hud_palettes")      # fix round 3 (2026-11-24)
# round 3's elements (Mariano's round-2 verdict fix / redo); every other element's outputs go to a scratch folder
FIX_IDS = {"amenities-strip", "building-palette", "hud-golfers-mode", "hud-mode-base", "hud-mode-base--a", "hud-mode-base--b"}
OTHER = os.path.join(os.environ.get("TEMP", OUT), "hp_round3_other")


def out_dir(el_id):
    return OUT if el_id in FIX_IDS else OTHER


def scene_json(el_id):
    """An element's scene json: the newest round that redid it (round 3, then 2, then 1)."""
    for d in (OUT, OUT2):
        p = os.path.join(d, el_id + ".json")
        if os.path.exists(p):
            return p
    return os.path.join(OUT1, el_id + ".json")
sys.path.insert(0, UIVEC)
sys.path.insert(0, ROSTER)
import fit  # noqa: E402
import sdf  # noqa: E402
import build_roster as BR  # noqa: E402

FLAT = np.array((70, 110, 90), float)
_table = None


def table():
    global _table
    if _table is None:
        BR.ELEMENTS.clear()
        BR.build_table()
        _table = {e["id"]: e for e in BR.ELEMENTS}
    return _table


def crops(el_id):
    """[(state, sheet, x0, y0, rgb H x W x 3 uint8, mask H x W bool)] with x0, y0 the cut's top left in its sheet."""
    el = table()[el_id]
    out = []
    for spec in el["crops"]:
        st = spec[0]
        if isinstance(spec[1], str) and spec[1].startswith("#"):
            opts = spec[2] if len(spec) > 2 else {}
            src = opts.get("sheet", el["sheet"])
            x, y, w, h = BR.island(src, int(spec[1][1:]))
        else:
            x, y, w, h = spec[1:5]
            opts = spec[5] if len(spec) > 5 else {}
            src = opts.get("sheet", el["sheet"])
        assert "bg" not in opts
        rgb, mask = BR.sheet(src)
        H, W = mask.shape
        x0, y0, x1, y1 = max(0, x), max(0, y), min(W, x + w), min(H, y + h)
        m = mask[y0:y1, x0:x1]
        if opts.get("trim", True) and m.any():
            ys, xs = np.nonzero(m)
            x0, y0, x1, y1 = x0 + xs.min(), y0 + ys.min(), x0 + xs.max() + 1, y0 + ys.max() + 1
        out.append((st, src, int(x0), int(y0), rgb[y0:y1, x0:x1].copy(), mask[y0:y1, x0:x1].copy()))
    return out


def sheet_rgb(rel):
    return BR.sheet(rel)


def art_on_flat(rgb, mask):
    a = rgb.astype(float).copy()
    a[~mask] = FLAT
    return a


def render_state(scene, x0, y0, w, h, z, bg=None, art=None):
    """The scene drawn at z pixels per sheet pixel over the cut's box (bg: H x W x 3 art-scale background, default flat)."""
    if bg is None:
        img = np.empty((h * z, w * z, 3))
        img[:] = FLAT
    else:
        img = np.asarray(Image.fromarray(np.clip(bg, 0, 255).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST), float).copy()
    sdf.render([it for it in scene if "layers" in it], img, z, origin=(x0, y0))
    keep = [it["keep_art"] for it in scene if "keep_art" in it]
    if keep and art is not None:
        # a kept picture: the art's own pixels where the keep shape covers (nearest-neighbour, coverage-blended at its edge)
        a = np.asarray(Image.fromarray(np.clip(art, 0, 255).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST), float)
        jj, ii = np.mgrid[0:h * z, 0:w * z]
        X = (ii + 0.5) / z + x0
        Y = (jj + 0.5) / z + y0
        d = sdf.sd({"t": "union", "of": keep}, X, Y)
        k = np.clip(0.5 - d * z, 0, 1)[..., None]
        img[:] = img * (1 - k) + a * k
    return img


def zoom_for(w, h, short_min=300, max_w=1600):
    z = max(1, int(np.ceil(short_min / max(1, min(w, h)))))
    while z > 1 and 2 * w * z + 8 > max_w:
        z -= 1
    return z


def compare(el_id, scenes, z=None, out=None, labels=True):
    """scenes: {state: scene}. Writes <OUT>/<id>.png: art left, shapes right, one row per state (crop order)."""
    rows = []
    cs = [c for c in crops(el_id) if c[0] in scenes]
    W = max(c[4].shape[1] for c in cs)
    H = max(c[4].shape[0] for c in cs)
    if z is None:
        z = zoom_for(W, H)
    for st, src, x0, y0, rgb, mask in cs:
        h, w = mask.shape
        art = art_on_flat(rgb, mask)
        a = np.asarray(Image.fromarray(art.astype(np.uint8)).resize((w * z, h * z), Image.NEAREST), float)
        r = render_state(scenes[st], x0, y0, w, h, z, art=art)
        row = np.zeros((H * z, 2 * W * z + 8, 3))
        row[:] = (30, 30, 30)
        row[:h * z, :w * z] = a
        row[:h * z, W * z + 8:W * z + 8 + w * z] = r
        rows.append(row)
    sep = np.zeros((6, rows[0].shape[1], 3)) + 30
    big = np.concatenate(sum(([r, sep] for r in rows), [])[:-1])
    im = Image.fromarray(np.clip(big + 0.5, 0, 255).astype(np.uint8))
    if labels:
        d = ImageDraw.Draw(im)
        y = 0
        for (st, *_), r in zip(cs, rows):
            d.text((4, y + 2), st, fill=(255, 255, 0))
            y += r.shape[0] + 6
    out = out or os.path.join(out_dir(el_id), f"{el_id}.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    im.save(out)
    return out


def score(el_id, scenes, z=4):
    """{state: (interior_mad, edge_px)}: mean abs RGB difference over the cut's opaque pixels, and the share-free count of
    pixels whose coverage disagrees (opaque in art vs nearly-flat in the render)."""
    res = {}
    for st, src, x0, y0, rgb, mask in crops(el_id):
        if st not in scenes:
            continue
        h, w = mask.shape
        r = render_state(scenes[st], x0, y0, w, h, z, art=art_on_flat(rgb, mask))
        r = r.reshape(h, z, w, z, 3).mean((1, 3))
        d = np.abs(r - rgb.astype(float)).mean(-1)
        er = mask & (np.abs(r - FLAT).sum(-1) < 6)
        ex = ~mask & (np.abs(r - FLAT).sum(-1) > 60)
        res[st] = (float(d[mask].mean()), int(er.sum() + ex.sum()))
    return res


def save_scene(el_id, scenes, extra=None, name=None):
    cs = {c[0]: c for c in crops(el_id)}
    doc = {"id": el_id, "coords": "source sheet pixels", "states": {}}
    for st, sc in scenes.items():
        _, src, x0, y0, rgb, mask = cs[st]
        doc["states"][st] = {"sheet": src, "cut": [x0, y0, mask.shape[1], mask.shape[0]], "scene": sc}
    if extra:
        doc.update(extra)
    name = name or el_id
    os.makedirs(out_dir(name), exist_ok=True)
    with open(os.path.join(out_dir(name), f"{name}.json"), "w") as f:
        json.dump(doc, f, default=lambda o: float(o) if isinstance(o, np.floating) else int(o))


def gridview(rgb, mask, x0, y0, z=12, out=None):
    """An enlarged gridded view of a cut, sheet coordinates labelled every 10 px."""
    h, w = mask.shape
    big = Image.fromarray(art_on_flat(rgb, mask).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST)
    d = ImageDraw.Draw(big)
    for g in range(0, max(w, h) + 1):
        major = ((x0 + g) % 10 == 0, (y0 + g) % 10 == 0)
        if g <= w:
            d.line([(g * z, 0), (g * z, h * z)], fill=(255, 0, 0) if major[0] else (120, 60, 60))
            if major[0]:
                d.text((g * z + 2, 2), str(x0 + g), fill=(255, 255, 0))
        if g <= h:
            d.line([(0, g * z), (w * z, g * z)], fill=(255, 0, 0) if major[1] else (120, 60, 60))
            if major[1]:
                d.text((2, g * z + 2), str(y0 + g), fill=(255, 255, 0))
    if out:
        big.save(out)
    return big


def dump(rgb, mask, x0, y0, test):
    """A cut as text: '#' where test(rgb) holds, '.' opaque otherwise, ' ' transparent."""
    h, w = mask.shape
    t = test(rgb.astype(float))
    lines = ["     " + "".join(str((x0 + i) // 10 % 10) for i in range(w)),
             "     " + "".join(str((x0 + i) % 10) for i in range(w))]
    for j in range(h):
        lines.append(f"{y0 + j:4d} " + "".join("#" if (mask[j, i] and t[j, i]) else ("." if mask[j, i] else " ")
                                                for i in range(w)))
    return "\n".join(lines)


def write_results(rows):
    p = os.path.join(OUT, "results.tsv")
    old = {}
    if os.path.exists(p):
        with open(p) as f:
            hdr = f.readline()
            for line in f:
                c = line.rstrip("\n").split("\t")
                old[c[0]] = c
    for r in rows:
        old[r[0]] = [str(v) for v in r]
    with open(p, "w") as f:
        f.write("id\tstatus\thow\tinterior_mad\tedge_px\tunsure\n")
        for k in sorted(old):
            f.write("\t".join(old[k]) + "\n")
