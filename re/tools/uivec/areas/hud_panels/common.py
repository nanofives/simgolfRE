"""P41 area hud_panels: shared access to the four sheets (Member list, Golfer action, Employee, Golfer info panels), the
element cells in sheet coordinates, an enlarged gridded viewer, a coverage dump, and the comparison / scoring writer.

Everything here is in SHEET coordinates: pixel (i, j) of a sheet covers [i, i+1) x [j, j+1). Scenes are sdf.py scenes in the
same coordinates. Transparent art pixels (index 0xfe / 0xff) show as FLAT, the roster's ground colour.

Usage: py -3.12 re/tools/uivec/areas/hud_panels/common.py zoom <sheet key> x y w h [scale] [out]
       py -3.12 re/tools/uivec/areas/hud_panels/common.py dump <sheet key> x y w h [lum|r|g|b]
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
UIVEC = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, UIVEC)
import fit  # noqa: E402
import sdf  # noqa: E402

ROOT = fit.ROOT
OUT = os.path.join(ROOT, "log", "roadmap", "P41", os.environ.get("HUDP_WORK", "work2"), "hud_panels")   # fix round: work2
FLAT = np.array((70, 110, 90), float)

SHEETS = {"M": "Interface/MemberPanel.pcx", "J": "Interface/JoeCoolPanel.pcx", "E": "Interface/EmployeePanel.pcx",
          "G": "Interface/GolferStats.pcx"}
_cache = {}


def sheet(key):
    """(rgb float H x W x 3 with transparent = FLAT, opaque mask)"""
    if key not in _cache:
        idx, pal = fit.load_sheet(SHEETS[key])
        rgb = pal[idx].astype(float)
        m = idx < 0xfe
        rgb[~m] = FLAT
        _cache[key] = (rgb, m)
    return _cache[key]


def _seg(key):
    name = os.path.splitext(os.path.basename(SHEETS[key]))[0]
    rows = {}
    with open(os.path.join(ROOT, "log", "roadmap", "P41", "roster", "seg", name + ".tsv")) as f:
        next(f)
        for line in f:
            i, x, y, w, h, _ = (int(v) for v in line.split("\t"))
            rows[i] = (x, y, w, h)
    return rows


def isl(key, *ns):
    s = _seg(key)
    return [s[n] for n in ns]


def _cells():
    C = {}
    M, J, E, G = "M", "J", "E", "G"
    sm, sj, se = _seg(M), _seg(J), _seg(E)
    C["member-panel"] = [("idle", M, (0xd7, 0x1da, 0x249, 0x7e))]
    C["member-slider"] = [("idle", M, sm[0])]
    C["member-bar"] = [(s, M, sm[n]) for s, n in (("a", 1), ("b", 2), ("c", 7), ("d", 8))]
    C["member-hearts"] = [(str(k), M, sm[n]) for k, n in enumerate((3, 4, 5, 6))]
    C["member-ball"] = [("a", M, sm[9]), ("b", M, sm[10])]
    C["member-mood-faces"] = [("strip", M, sm[11])]
    C["member-mood-faces-extra"] = [("row 1", M, sm[12]), ("row 2", M, sm[15])]
    C["member-page-left"] = [("idle", M, sm[13]), ("hover", M, sm[14])]
    C["member-page-right"] = [("idle", M, sm[22]), ("hover", M, sm[23])]
    C["member-skill-bar"] = [(f"level {i}", M, sm[n]) for i, n in enumerate((16, 17, 18, 19, 24, 25, 26))]
    C["member-pips"] = [("a", M, (0x1c2, 0xdc, 7, 7)), ("b", M, (0x1c2, 0xe6, 7, 7))]
    C["golfer-stats-button"] = [(s, M, sm[n]) for s, n in (("idle", 27), ("hover", 28), ("disabled", 29))]
    C["tab-golfer"] = [("idle", M, sm[30])]
    C["tab-golfer-staff"] = [("idle", M, sm[31])]
    C["golfer-action-panel"] = [("idle", J, (0xd6, 0x1da, 0x24a, 0x7e))]
    C["match-plate"] = [("idle", J, (0x129, 0x12c, 0x1f7, 0x7a))]
    for r, k in enumerate(("low", "rising", "flat", "high", "straight")):
        C[f"shot-shape-{k}"] = [(s, J, sj[4 * r + i]) for i, s in enumerate(("idle", "selected", "disabled", "hover"))]
    C["joecool-stats-button"] = [(s, J, sj[n]) for s, n in (("idle", 20), ("hover", 21), ("disabled", 22))]
    C["tab-golfers-group"] = [("idle", J, sj[24])]
    C["tab-golfer-swing"] = [("idle", J, sj[25])]
    C["joecool-tab-staff"] = [("idle", J, sj[26])]
    for k, y, h in (("club", 500, 32), ("crossed", 535, 31), ("trophy", 566, 31)):
        C[f"golfer-{k}-button"] = [(s, J, (50 * i, y, 38, h)) for i, s in enumerate(("idle", "selected", "disabled", "hover"))]
    C["employee-panel"] = [("idle", E, (0xd7, 0x1da, 0x249, 0x7e))]
    C["employee-pedestal"] = [(s, E, se[n]) for s, n in (("idle", 0), ("selected", 1), ("disabled", 2), ("idle 2", 7),
                                                          ("selected 2", 8), ("disabled 2", 9))]
    for k, a in (("hire", 3), ("fire", 10), ("resume", 14)):
        C[f"employee-{k}"] = [(s, E, se[a + i]) for i, s in enumerate(("idle", "selected", "disabled", "hover"))]
    C["employee-page-left"] = [(s, E, se[n]) for s, n in (("idle", 18), ("lit", 19), ("disabled", 20))]
    C["employee-page-right"] = [(s, E, se[n]) for s, n in (("idle", 21), ("lit", 22), ("disabled", 23))]
    C["tab-staff-group"] = [("idle", E, se[24])]
    C["employee-tab-golfer"] = [("idle", E, se[25])]
    C["employee-thumbs-up"] = [(s, E, se[n]) for s, n in (("idle", 27), ("hover", 28), ("disabled", 29))]
    C["golfer-info-panel"] = [("idle", G, (0x40, 0x21, 0x192, 0xf4))]
    C["golfer-story-band"] = [("idle", G, (0x40, 0x12c, 0x192, 0x8a))]
    tabs = ("info", "mood", "swing", "photo", "story", "ok", "history", "thought")
    for r, k in enumerate(tabs):
        st = ("idle", "hover", "disabled") if k == "ok" else ("idle", "hover", "selected")
        C[f"golfer-tab-{k}"] = [(s, G, (0x1f4 + 0x64 * c, 0x32 * r, 0x39, 0x32)) for c, s in enumerate(st)]
    return C


CELLS = _cells()


# ------------------------------------------------------------------------------------------------------------ viewing
def zoom(key, x, y, w, h, s=12, out=None, rgb=None):
    if rgb is None:
        rgb = sheet(key)[0]
    a = rgb[y:y + h, x:x + w]
    big = Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8)).resize((w * s, h * s), Image.NEAREST)
    d = ImageDraw.Draw(big)
    for g in range(0, max(w, h) + 1):
        major = ((x + g) % 10 == 0, (y + g) % 10 == 0)
        if g <= w:
            d.line([(g * s, 0), (g * s, h * s)], fill=(255, 0, 0) if major[0] else (150, 60, 60))
            if major[0] and g < w:
                d.text((g * s + 2, 2), str(x + g), fill=(255, 255, 0))
        if g <= h:
            d.line([(0, g * s), (w * s, g * s)], fill=(255, 0, 0) if major[1] else (150, 60, 60))
            if major[1] and g < h:
                d.text((2, g * s + 2), str(y + g), fill=(255, 255, 0))
    out = out or os.path.join(OUT, "_zoom.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    big.save(out)
    return out


def dump(key, x, y, w, h, chan="lum"):
    rgb, m = sheet(key)
    a = rgb[y:y + h, x:x + w]
    v = fit.lum(a) if chan == "lum" else a[..., "rgb".index(chan)]
    print("     " + "".join(f"{(x + i) % 100:4d}" for i in range(w)))
    for j in range(h):
        print(f"{y + j:4d} " + "".join(f"{int(v[j, i]):4d}" if m[y + j, x + i] else "   ." for i in range(w)))


# ------------------------------------------------------------------------------------------------------------ output
def render_cell(scene, rect, s):
    x, y, w, h = rect
    img = np.empty((h * s, w * s, 3))
    img[:] = FLAT
    sdf.render(scene, img, s, origin=(x, y))
    return img


def metrics(art, mask, low, alpha):
    """interior_mad: mean |channel difference| over art pixels inside the element, one pixel from its silhouette and away
    from the art's own strong edges (where a crisp shape and the art's antialiasing differ by construction).
    edge_px: silhouette mismatch (pixels that differ in opaque-ness) / silhouette perimeter = mean boundary offset."""
    L = fit.lum(art)
    er = mask.copy()
    for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        er &= np.roll(np.roll(mask, dj, 0), di, 1)
    g = np.zeros_like(L)
    for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
        g = np.maximum(g, np.abs(L - np.roll(np.roll(L, dj, 0), di, 1)))
    inner = er & (g < 30)
    mad = float(np.abs(low - art)[inner].mean()) if inner.any() else float("nan")
    got = alpha > 0.5
    mp = np.pad(mask, 1)
    per = (mp[1:] ^ mp[:-1]).sum() + (mp[:, 1:] ^ mp[:, :-1]).sum()
    edge = float((got ^ mask).sum() / max(per, 1))
    return mad, edge


def compare(el_id, scenes, z=None, extra=None, note=None):
    """scenes: {state: scene} for every cell of the element. Writes <id>.png (art | shapes, states stacked) and <id>.json.
    Returns (interior_mad, edge_px) averaged over the states."""
    cells = CELLS[el_id]
    tiles, mads, edges = [], [], []
    for st, key, rect in cells:
        x, y, w, h = rect
        rgb, m = sheet(key)
        art, mask = rgb[y:y + h, x:x + w], m[y:y + h, x:x + w]
        sc = scenes[st]
        zz = z or max(2, min(8, int(np.ceil(300 / min(w, h))), 1600 // (2 * w)))
        big = render_cell(sc, rect, zz)
        low = render_cell(sc, rect, 4).reshape(h, 4, w, 4, 3).mean(axis=(1, 3))
        alpha = (np.abs(low - FLAT).max(axis=2) > 25).astype(float)
        mad, edge = metrics(art, mask, low, alpha)
        mads.append(mad)
        edges.append(edge)
        left = np.kron(art, np.ones((zz, zz, 1)))
        t = np.concatenate([left, np.full((h * zz, 6, 3), 255.0), big], axis=1)
        tiles.append(t)
    W = max(t.shape[1] for t in tiles)
    sheet_img = np.concatenate([np.pad(t, ((0, 6), (0, W - t.shape[1]), (0, 0)), constant_values=255) for t in tiles])
    os.makedirs(OUT, exist_ok=True)
    Image.fromarray(np.clip(sheet_img + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(OUT, el_id + ".png"))
    doc = {"id": el_id, "coords": "sheet pixels of " + SHEETS[cells[0][1]], "states": {}}
    for st, key, rect in cells:
        doc["states"][st] = {"cell": list(rect), "scene": scenes[st]}
    if extra:
        doc.update(extra)
    with open(os.path.join(OUT, el_id + ".json"), "w") as f:
        json.dump(doc, f, default=_r)
    return float(np.nanmean(mads)), float(np.mean(edges))


def _r(o):
    if isinstance(o, (np.floating, np.integer)):
        return o.item()
    raise TypeError(type(o))


# ------------------------------------------------------------------------------------------------------------ results
RESULTS = os.path.join(OUT, "results.tsv")
COLS = ("id", "status", "how", "changed", "unsure")


def record(el_id, status, how, changed="", unsure=""):
    """One row of the fix round's results.tsv (id, status, how, changed, unsure)."""
    rows = {}
    if os.path.exists(RESULTS):
        with open(RESULTS, encoding="utf-8") as f:
            next(f)
            for line in f:
                p = line.rstrip("\n").split("\t")
                rows[p[0]] = p
    rows[el_id] = [el_id, status, how, changed, unsure]
    os.makedirs(OUT, exist_ok=True)
    with open(RESULTS, "w", encoding="utf-8") as f:
        f.write("\t".join(COLS) + "\n")
        for k in sorted(rows):
            f.write("\t".join(rows[k]) + "\n")


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "zoom":
        key, x, y, w, h = sys.argv[2], *(int(v) for v in sys.argv[3:7])
        s = int(sys.argv[7]) if len(sys.argv) > 7 else 12
        print(zoom(key, x, y, w, h, s, sys.argv[8] if len(sys.argv) > 8 else None))
    elif cmd == "dump":
        key, x, y, w, h = sys.argv[2], *(int(v) for v in sys.argv[3:7])
        dump(key, x, y, w, h, sys.argv[7] if len(sys.argv) > 7 else "lum")


def covdump(key, x, y, w, h, inside=None):
    """The dark glyph's coverage as text over a box ('#' > 0.85, '+' > 0.6, ':' > 0.35, '.' > 0.12)."""
    rgb, m = sheet(key)
    a = rgb[y:y + h, x:x + w]
    ins = m[y:y + h, x:x + w] if inside is None else inside
    cov, lo, hi = fit.dark_coverage(a, ins)
    print(f"lo {lo:.0f} hi {hi:.0f}")
    print("     " + "".join(str((x + i) // 10 % 10) for i in range(w)))
    print("     " + "".join(str((x + i) % 10) for i in range(w)))
    for j in range(h):
        print(f"{y + j:4d} " + "".join("#" if c > 0.85 else "+" if c > 0.6 else ":" if c > 0.35 else "." if c > 0.12 else " "
                                     for c in cov[j]))
    return cov


def hexdump(key, x, y, w, h):
    """RGB as 6-digit hex per pixel ('......' = transparent), for reading multi-colour glyphs."""
    rgb, m = sheet(key)
    print("     " + " ".join(f"{(x + i) % 1000:6d}" for i in range(w)))
    for j in range(h):
        print(f"{y + j:4d} " + " ".join(("%02x%02x%02x" % tuple(int(t) for t in rgb[y + j, x + i])) if m[y + j, x + i] else "......"
                                      for i in range(w)))
