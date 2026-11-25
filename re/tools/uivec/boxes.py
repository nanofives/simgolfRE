"""uivec: flat boxes (P41): pills, plates, list stripes and pages. On the front-end screens these are flat-coloured rounded
rectangles, most with a thin navy outline. Each is found on the art by its exact fill colour, measured (box, corner radius,
outline colour and width), drawn as two shapes (the outline's rounded rectangle under the fill's) and scored against the art.

Usage: py -3.12 re/tools/uivec/boxes.py <sheet under original\\> <name>
       e.g. py -3.12 re/tools/uivec/boxes.py Interface/Title_LoadGame.pcx load
  -> log/roadmap/P41/<name>/boxes.tsv, boxes_scene.json (numbers only), boxes_compare.png (art | shapes | difference x4)
A component that is not a rounded rectangle (overlap of the best one with its pixels under FIT_MIN) is listed as SKIPPED:
it is another kind of shape, or a box partly hidden by something drawn over it.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import sdf      # noqa: E402

FIT_MIN = 0.965
MIN_AREA = 60
# As for the buttons (accept.py): a box passes when its INSIDE has the art's colour and its line sits where the art's does
# (the centre of the darkness across each side, art against shapes, within half a pixel). The pixels on the line itself are
# reported (mad, w16) and not gated: on a pill 16 pixels tall they are a third of the picture.
LIMITS = {"imad": 3.0, "edge": 0.5}


def components(mask, min_area):
    H, W = mask.shape
    seen = np.zeros(mask.shape, bool)
    out = []
    for j0, i0 in zip(*np.nonzero(mask)):
        if seen[j0, i0]:
            continue
        seen[j0, i0] = True
        stack, comp = [(j0, i0)], []
        while stack:
            j, i = stack.pop()
            comp.append((j, i))
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                a, b = j + dj, i + di
                if 0 <= a < H and 0 <= b < W and mask[a, b] and not seen[a, b]:
                    seen[a, b] = True
                    stack.append((a, b))
        if len(comp) >= min_area:
            out.append(np.array(comp))
    return out


def fit_rrect(comp):
    """(x0, y0, x1, y1, r, overlap): the rounded rectangle over the component's extent whose pixel centres match it best."""
    j0, i0 = comp.min(0)
    j1, i1 = comp.max(0) + 1
    h, w = j1 - j0, i1 - i0
    m = np.zeros((h, w), bool)
    m[comp[:, 0] - j0, comp[:, 1] - i0] = True
    yy, xx = np.mgrid[0:h, 0:w]
    best = None
    for r in np.arange(0, min(h, w) / 2 + 0.26, 0.25):
        qx = np.abs(xx + 0.5 - w / 2) - (w / 2 - r)
        qy = np.abs(yy + 0.5 - h / 2) - (h / 2 - r)
        d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r
        got = d < 0
        iou = (got & m).sum() / max((got | m).sum(), 1)
        if best is None or iou > best[1] + 1e-9:
            best = (float(r), float(iou))
    return int(i0), int(j0), int(i1), int(j1), best[0], best[1]


def measure(rgb):
    """Every flat box of the sheet: list of dicts (box, r, fill, outline, ow)."""
    H, W = rgb.shape[:2]
    key = (rgb[..., 0].astype(np.int32) << 16) | (rgb[..., 1].astype(np.int32) << 8) | rgb[..., 2]
    vals, counts = np.unique(key, return_counts=True)
    lum = fit.lum(rgb.astype(float))
    boxes, skipped = [], []
    for v, n in zip(vals, counts):
        if n < 400:
            continue
        col = [int(v >> 16), int((v >> 8) & 255), int(v & 255)]
        if fit.lum(np.array(col, float)) < 150:      # the fills are light: lilac, cream, tan
            continue
        for comp in components(key == v, MIN_AREA):
            x0, y0, x1, y1, r, iou = fit_rrect(comp)
            if min(x1 - x0, y1 - y0) < 6:      # a line, not a box
                continue
            behind = False
            if iou < FIT_MIN:
                skipped.append((col, x0, y0, x1, y1, len(comp), round(iou, 3)))
                continue
            # the outline: the darkest ring just outside the fill
            a0, b0, a1, b1 = max(y0 - 3, 0), max(x0 - 3, 0), min(y1 + 3, H), min(x1 + 3, W)
            yy, xx = np.mgrid[a0:a1, b0:b1]
            S = {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}
            d = sdf.sd(S, xx + 0.5, yy + 0.5)
            L = lum[a0:a1, b0:b1]
            DARK = 72.0      # navy and the list's hairlines are under it; the grey panel behind the pills (about 90) is not
            e = {"box": [x0, y0, x1, y1], "r": r, "fill": col, "fit": round(iou, 3)}
            # The outline is read along rays going out from the middle of each straight side: how many pixels until the
            # first dark one (0, or 1 when a blended pixel lies between the fill and its line: the fill's true edge is then
            # half a pixel further out) and how many dark pixels follow. The side-by-side median decides; a side with a
            # neighbour pressed against it does not spoil it.
            # Sub-pixel: a pixel between the fill and the line that is a blend of the two tells where the fill really
            # ends (a third dark = the line starts a third of a pixel before that pixel's far side); the same at the
            # line's outer side against what lies beyond.
            inner, outer, cols, sides, bands = [], [], [], {}, {}
            cxm, cym = (x0 + x1) // 2, (y0 + y1) // 2
            fill_l = float(fit.lum(np.array(col, float)))
            for side, (px, py, dx, dy) in zip("tblr", ((cxm, y0 - 1, 0, -1), (cxm, y1, 0, 1), (x0 - 1, cym, -1, 0),
                                                       (x1, cym, 1, 0))):
                run = []
                for k in range(6):
                    xx_, yy_ = px + dx * k, py + dy * k
                    if not (0 <= xx_ < W and 0 <= yy_ < H):
                        break
                    run.append((float(lum[yy_, xx_]), rgb[yy_, xx_]))
                first = next((k for k, (l, _) in enumerate(run) if l < DARK), None)
                if first is None or first > 4:
                    continue
                if first > 1:      # several shaded pixels between the fill and its line: a sunken plate's inner shadow
                    bands[side] = [[int(c) for c in run[k][1]] for k in range(first)]
                    inner.append(float(first)); outer.append(float(first + 1)); cols.append(run[first][1])
                    sides[side] = (float(first), float(first + 1))
                    continue
                n = 0
                while first + n < len(run) and run[first + n][0] < DARK:
                    n += 1
                dark_l = min(l for l, _ in run[first:first + n])
                start = float(first)
                if first == 1:      # the blend pixel before the line
                    start -= min(max((fill_l - run[0][0]) / max(fill_l - dark_l, 1.0), 0.0), 1.0)
                end = float(first + n)
                if first + n + 1 < len(run):      # the pixel after the line, against the one beyond it
                    beyond = run[first + n + 1][0]
                    part = (beyond - run[first + n][0]) / max(beyond - dark_l, 1.0)
                    if 0.15 < part < 0.85:
                        end += part
                inner.append(start)
                outer.append(end)
                sides[side] = (round(start, 2), round(min(end, start + 2.6), 2))
                cols.append(run[first][1])
            if len(inner) >= 2:
                e["grow"] = round(float(np.median(inner)), 2)
                e["outline"] = [int(c) for c in np.median(np.array(cols), axis=0)]
                # each side's own width (its line's end minus its start); the median side, never over 1.6 px (a
                # neighbour pressed against a side thickens that side only)
                e["ow"] = round(min(float(np.median([o - i for o, i in zip(outer, inner)])), 1.6), 2)
                # The line is not the same on every side: on these screens it is thin above a pill and thick below it (the
                # pill is lit from above and its line doubles as its shadow). Each side keeps its own start and end; a side
                # that could not be read takes the median.
                med = (e["grow"], round(e["grow"] + e["ow"], 2))
                e["sides"] = {k: list(sides.get(k, med)) for k in "tblr"}
                if bands:
                    e["bands"] = bands
            boxes.append(e)
    # a box partly hidden by others drawn over it (the list's first row under the pills): a skipped component of a colour
    # that did give boxes, covering most of its extent, is that box; it is drawn before the smaller ones
    fills = {tuple(e["fill"]) for e in boxes}
    kept = []
    for c in skipped:
        if tuple(c[0]) in fills and c[6] >= 0.6:
            boxes.append({"box": [c[1], c[2], c[3], c[4]], "r": 0.0, "fill": c[0], "fit": c[6], "behind": True,
                          "outline": [47, 55, 107], "ow": 1.1, "grow": 0.0,
                          "sides": {k: [0.0, 1.1] for k in "tblr"}})
        elif tuple(c[0]) in fills:
            kept.append(c)
    boxes.sort(key=lambda e: -(e["box"][2] - e["box"][0]) * (e["box"][3] - e["box"][1]))
    return boxes, kept


def item(e):
    x0, y0, x1, y1 = e["box"]
    sd_ = e.get("sides")

    def rr(k, r):       # the box moved out on each side by that side's own amount
        t, b_, l, r_ = (sd_[q][k] for q in "tblr") if sd_ else (0.0, 0.0, 0.0, 0.0)
        return {"t": "rrect", "c": [(x0 - l + x1 + r_) / 2, (y0 - t + y1 + b_) / 2],
                "half": [(x1 - x0 + l + r_) / 2, (y1 - y0 + t + b_) / 2], "r": r + (t + b_ + l + r_) / 4 if r else 0.0, "rot": 0.0}

    L = []
    if "outline" in e:
        L.append({"shape": rr(1, e["r"]), "fill": e["outline"]})
    L.append({"shape": rr(0, e["r"]), "fill": e["fill"]})
    # a sunken plate: the shaded pixels inside its line, as nested boxes from the line inward, each one pixel smaller
    # on the sides that have the shading
    bands = e.get("bands") or {}
    if bands:
        deep = max(len(v) for v in bands.values())
        lead = max(bands, key=lambda q: len(bands[q]))
        for j in range(deep):       # j = 0 is the pixel next to the line
            ext = {q: (max(len(bands[q]) - j, 0) if q in bands else sd_[q][0]) for q in "tblr"}
            S = {"t": "rrect", "c": [(x0 - ext["l"] + x1 + ext["r"]) / 2, (y0 - ext["t"] + y1 + ext["b"]) / 2],
                 "half": [(x1 - x0 + ext["l"] + ext["r"]) / 2, (y1 - y0 + ext["t"] + ext["b"]) / 2],
                 "r": e["r"] + max(deep - j, 0) * 0.5, "rot": 0.0}
            L.append({"shape": S, "fill": bands[lead][len(bands[lead]) - 1 - j] if j < len(bands[lead]) else e["fill"]})
        L.append({"shape": {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2],
                            "r": e["r"], "rot": 0.0}, "fill": e["fill"]})
    return {"box": [x0 - 4, y0 - 4, x1 + 4, y1 + 4], "layers": L}


def main():
    sheet, name = sys.argv[1], sys.argv[2]
    out = os.path.join(fit.ROOT, "log", "roadmap", "P41", name)
    os.makedirs(out, exist_ok=True)
    idx, pal = fit.load_sheet(sheet)
    rgb = pal[idx]
    boxes, skipped = measure(rgb)
    scene = [item(e) for e in boxes]
    with open(os.path.join(out, "boxes_scene.json"), "w") as f:
        json.dump({"boxes": boxes, "scene": scene}, f)
    s = 4
    art = rgb.astype(float)
    # The shapes are drawn over the art with every box WIPED first (its place and 3 pixels round it filled with the colour
    # found just beyond): an outline the shapes fail to draw then shows as a difference instead of being lent by the art.
    wiped = art.copy()
    for e in sorted(boxes, key=lambda e: (e["box"][2] - e["box"][0]) * (e["box"][3] - e["box"][1])):
        if e.get("behind"):
            continue
        x0, y0, x1, y1 = e["box"]
        a0, b0, a1, b1 = max(y0 - 6, 0), max(x0 - 6, 0), min(y1 + 6, 600), min(x1 + 6, 800)
        yy, xx = np.mgrid[a0:a1, b0:b1]
        S = {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": e["r"], "rot": 0.0}
        d = sdf.sd(S, xx + 0.5, yy + 0.5)
        # what lies behind each wiped pixel: the art's own pixel found by walking straight out of the box to just past
        # its line (2.6 px out), so the surroundings (a wire between two pills, a shaded panel) stay as they are
        sub = art[a0:a1, b0:b1]
        gy, gx = np.gradient(d)
        n_ = np.maximum(np.hypot(gx, gy), 1e-6)
        step = np.clip(2.6 - d, 0, None)
        tx = np.clip(np.rint(xx + gx / n_ * step).astype(int), b0, b1 - 1) - b0
        ty = np.clip(np.rint(yy + gy / n_ * step).astype(int), a0, a1 - 1) - a0
        m = d < 2.2
        wiped[a0:a1, b0:b1][m] = sub[ty[m], tx[m]]
    img = np.kron(wiped, np.ones((s, s, 1)))
    sdf.render(scene, img, s)
    low = img.reshape(600, s, 800, s, 3).mean(axis=(1, 3))
    diff = np.abs(low - art)
    rows, fails = [], 0
    la, lv = fit.lum(art), fit.lum(low)

    def centre(Lm, px, py, dx, dy):
        """The centre of the darkness along 7 pixels across a side (None when there is none)."""
        v = []
        for k in range(-3, 4):
            xx_, yy_ = px + dx * k, py + dy * k
            if not (0 <= xx_ < 800 and 0 <= yy_ < 600):
                return None
            v.append(float(Lm[yy_, xx_]))
        v = np.array(v)
        w = np.clip(np.interp(np.arange(7), [0, 6], [v[0], v[-1]]) - v, 0, None)
        return float((w * np.arange(7)).sum() / w.sum()) if w.sum() > 25 else None

    for k, e in enumerate(boxes):
        x0, y0, x1, y1 = e["box"]
        a0, b0, a1, b1 = max(y0 - 2, 0), max(x0 - 2, 0), min(y1 + 2, 600), min(x1 + 2, 800)
        d = diff[a0:a1, b0:b1]
        mad, w16 = float(d.mean()), float((d.max(axis=2) <= 16).mean())
        inner = diff[y0 + 2:y1 - 2, x0 + 2:x1 - 2]
        imad = float(inner.mean()) if inner.size else 0.0
        edge = 0.0
        if "outline" in e and not e.get("behind"):
            cxm, cym = (x0 + x1) // 2, (y0 + y1) // 2
            for px, py, dx, dy in ((cxm, y0 - 1, 0, 1), (cxm, y1, 0, 1), (x0 - 1, cym, 1, 0), (x1, cym, 1, 0)):
                ca, cv = centre(la, px, py, dx, dy), centre(lv, px, py, dx, dy)
                if ca is not None:
                    edge = max(edge, abs(ca - cv) if cv is not None else 9.9)
        ok = imad <= LIMITS["imad"] and edge <= LIMITS["edge"]
        fails += not ok
        rows.append((e, mad, w16, ok, imad, edge))
        print(f"[{k + 1}/{len(boxes)}] box {x0},{y0} {x1 - x0}x{y1 - y0} {'PASS' if ok else 'FAIL'}", flush=True)
    print()
    print(f"{'box':>10s} {'size':>9s} {'r':>5s} {'fill':>13s} {'in mad':>6s} {'line px':>7s} | {'all mad':>7s} {'all w16':>7s}  result")
    with open(os.path.join(out, "boxes.tsv"), "w") as f:
        f.write("x0	y0	w	h	r	fill	in_mad	line_px	mad	w16	result" + chr(10))
        for e, mad, w16, ok, imad, edge in rows:
            x0, y0, x1, y1 = e["box"]
            fill = ",".join(str(c) for c in e["fill"])
            print(f"{f'{x0},{y0}':>10s} {f'{x1 - x0}x{y1 - y0}':>9s} {e['r']:5.2f} {fill:>13s} {imad:6.2f} {edge:7.2f} | {mad:7.2f} "
                  f"{w16:7.3f}  {'PASS' if ok else 'FAIL'}")
            f.write(f"{x0}	{y0}	{x1 - x0}	{y1 - y0}	{e['r']}	{fill}	{imad:.2f}	{edge:.2f}	{mad:.2f}	{w16:.3f}	"
                    f"{'PASS' if ok else 'FAIL'}" + chr(10))
    print()
    print(f"limits {LIMITS}: {len(rows) - fails} PASS, {fails} FAIL; {len(skipped)} components SKIPPED (not rounded rectangles):")
    for c in skipped:
        print("   fill", c[0], "at", c[1:3], "size", f"{c[3] - c[1]}x{c[4] - c[2]}", "pixels", c[5], "best overlap", c[6])
    z = 2
    big = lambda q: np.asarray(Image.fromarray(np.clip(q + 0.5, 0, 255).astype(np.uint8)).resize((800 * z, 600 * z), Image.LANCZOS), float)   # noqa: E731
    sheet_img = np.concatenate([big(np.kron(art, np.ones((s, s, 1)))), big(img), big(np.kron(np.clip(diff * 4, 0, 255), np.ones((s, s, 1))))], axis=1)
    Image.fromarray(np.clip(sheet_img + 0.5, 0, 255).astype(np.uint8)).save(os.path.join(out, "boxes_compare.png"))
    print(os.path.join(out, "boxes_compare.png"))


if __name__ == "__main__":
    main()
