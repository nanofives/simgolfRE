"""P24D (log/roadmap/P24/P24D_notes.md, step vi): Spanish copies of the game images with English words in their pixels, drawn
from the user's own install with the game's own TrueType font, for the LAN build only (written into the gitignored lang\\ folder,
shipped as game\\Lang\\es\\<path>; the port reads them in place of the original while Spanish is on). No game text or art lives
in this script: the rectangles and the words come from lang\\art_es.tsv.

    ~/bin/py -3.12 scripts/lang/redraw_art.py --list Interface/TROPHYparts.pcx      # the plates the detector finds (x y w h)
    ~/bin/py -3.12 scripts/lang/redraw_art.py [--install original] [--spec lang/art_es.tsv] [--out lang/es] [--preview <dir>]

lang\\art_es.tsv rows: `image<TAB>x,y,w,h<TAB>text[<TAB>options]` (UTF-8; image relative to the install; options `size=N`,
`font=manu|klepto`, `pad=N`, `color=r,g,b`). Per rectangle: each row of the plate is refilled with the colour of its text-free
end columns (the plates are horizontal gradients), then the text is drawn centred in the colour of the darkest pixels the
English had (or `color=`), shrunk until it fits. The image keeps its palette (8-bit PCX: every pixel mapped back to the nearest
palette entry), size and format."""
from __future__ import annotations

import argparse
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
FONTS = {"manu": "manu3_.TTF", "klepto": "KLEPTO__.TTF"}


def plates(im: Image.Image, min_w: int = 60, min_h: int = 8, max_h: int = 24):
    """Horizontal bright plates: runs of rows where a long run of bright pixels exists (the detector for --list)."""
    rgb = im.convert("RGB")
    w, h = rgb.size
    px = rgb.load()
    bright = [[sum(px[x, y]) > 300 for x in range(w)] for y in range(h)]
    found = []
    seen = set()
    for y in range(h):
        x = 0
        while x < w:
            if bright[y][x]:
                x0 = x
                while x < w and bright[y][x]:
                    x += 1
                if x - x0 >= min_w and (x0, y) not in seen:
                    y1 = y
                    while y1 + 1 < h and sum(bright[y1 + 1][x0:x]) > 0.8 * (x - x0):
                        y1 += 1
                    if min_h <= y1 - y + 1 <= max_h:
                        found.append((x0, y, x - x0, y1 - y + 1))
                    for yy in range(y, y1 + 1):
                        seen.add((x0, yy))
            else:
                x += 1
    return found


def redraw(im: Image.Image, rect, text: str, opts: dict, font_dir: pathlib.Path, info: list | None = None) -> Image.Image:
    x, y, w, h = rect
    rgb = im.convert("RGB")
    px = rgb.load()
    pad = int(opts.get("pad", 4))
    # the text colour: the darkest pixels inside the plate (the English letters), unless given
    if "color" in opts:
        col = tuple(int(v) for v in opts["color"].split(","))
    else:
        cells = sorted((sum(px[xx, yy]), px[xx, yy]) for yy in range(y, y + h) for xx in range(x + pad, x + w - pad))
        dark = [c for _, c in cells[: max(1, len(cells) // 12)]]
        col = tuple(sum(c[i] for c in dark) // len(dark) for i in range(3))
    # remove the English: in each column the letter pixels (darker than `cut` of the column's brightest) take the mean of the
    # column's plate pixels, so the plate's own shading (across x for the trophy plates) stays as it was
    cut = float(opts.get("cut", 0.8))
    rpad = int(opts.get("rpad", 1))
    if opts.get("fill", "column") == "none":
        pass  # draw only (the rectangle was cleared by an earlier row)
    elif opts.get("fill", "column") == "solid":
        # text on a flat background (bg=r,g,b): the rectangle becomes that colour
        bg = tuple(int(v) for v in opts.get("bg", "0,0,0").split(","))
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                px[xx, yy] = bg
    elif opts.get("fill", "column") == "inpaint":
        # a textured plate with light letters and dark shadows: those pixels (outside [lo, hi] in r + g + b, one pixel wider)
        # are filled from their filled neighbours, inwards from the plate's own texture
        lo, hi = int(opts.get("lo", 115)), int(opts.get("hi", 350))
        inner = [(xx, yy) for yy in range(y + rpad, y + h - rpad) for xx in range(x + pad, x + w - pad)]
        bad = {(xx, yy) for xx, yy in inner if not lo <= sum(px[xx, yy]) <= hi}
        grown = set(bad)
        for _ in range(int(opts.get("grow", 1))):
            ring = set()
            for xx, yy in grown:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    q = (xx + dx, yy + dy)
                    if x + pad <= q[0] < x + w - pad and y + rpad <= q[1] < y + h - rpad:
                        ring.add(q)
            grown |= ring
        todo = grown
        while todo:
            nxt = set()
            for xx, yy in todo:
                nb = [px[xx + dx, yy + dy] for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1))
                      if (xx + dx, yy + dy) not in todo and x <= xx + dx < x + w and y <= yy + dy < y + h]
                if nb:
                    px[xx, yy] = tuple(sum(c[i] for c in nb) // len(nb) for i in range(3))
                else:
                    nxt.add((xx, yy))
            if len(nxt) == len(todo):
                break
            todo = nxt
    elif opts.get("fill", "column") == "row":
        # a plate shaded down y (a bevelled box): every row inside takes its end columns' mean
        for yy in range(y + rpad, y + h - rpad):
            ends = [px[xx, yy] for xx in range(x, x + pad)] + [px[xx, yy] for xx in range(x + w - pad, x + w)]
            avg = tuple(sum(c[i] for c in ends) // len(ends) for i in range(3))
            for xx in range(x + pad, x + w - pad):
                px[xx, yy] = avg
    else:
        for xx in range(x + rpad, x + w - rpad):
            col_px = [px[xx, yy] for yy in range(y, y + h)]
            top = max(sum(c) for c in col_px)
            plate = [c for c in col_px if sum(c) >= cut * top] or col_px
            avg = tuple(sum(c[i] for c in plate) // len(plate) for i in range(3))
            for yy in range(y, y + h):
                if sum(px[xx, yy]) < cut * top:
                    px[xx, yy] = avg
    if text == "~":  # erase only
        if im.mode == "P":
            out = rgb.quantize(palette=im, dither=Image.Dither.NONE)
            out.info.update(im.info)
            return out
        return rgb
    d = ImageDraw.Draw(rgb)
    lines = text.split("|")  # '|' breaks a line (a two-line plate)
    size = int(opts.get("size", h // len(lines) - 2))
    path = font_dir / FONTS[opts.get("font", "manu")]
    bold = int(opts.get("bold", 0))
    while True:
        f = ImageFont.truetype(str(path), size)
        boxes = [d.textbbox((0, 0), s, font=f) for s in lines]
        step = max(b - t for _, t, _, b in boxes) + 1
        if size <= 6 or (max(r - l for l, _, r, _ in boxes) + bold <= w - 2 * pad and step * len(lines) <= h):
            break
        size -= 1
    y0 = y + (h - step * len(lines)) // 2
    # rot=deg: the text drawn on a clear layer over the rectangle, turned by deg (counter-clockwise) about its centre and laid
    # over the picture (a tilted note); the drawing below then goes to that layer
    rot = float(opts.get("rot", 0))
    base, layer = rgb, None
    if rot:
        layer = Image.new("RGBA", rgb.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(layer)
    for i, (s, (l, t, r, b)) in enumerate(zip(lines, boxes)):
        # align=left: the first line at the rectangle's left (pad), the next ones indented by `indent` (a handwritten list)
        if opts.get("align") == "left":
            tx = x + pad - l + (int(opts.get("indent", 0)) if i else 0)
        else:
            tx = x + (w - (r - l)) // 2 - l
        ty = y0 + i * step + (step - (b - t)) // 2 - t
        if "shadow" in opts:  # shadow=r,g,b: the letters' dark copy one pixel down and right first (an embossed plate)
            sc = tuple(int(v) for v in opts["shadow"].split(","))
            for k in range(1 + bold):
                d.text((tx + k + 1, ty + 1), s, font=f, fill=sc)
        for k in range(1 + bold):  # bold=1: drawn twice, one pixel apart (the plates' heavy capitals)
            d.text((tx + k, ty), s, font=f, fill=col)
        if info is not None and not rot:  # P26C (--sharp): the line as a text run: its ink box, pen origin, baseline, size
            l2, t2, r2, b2 = d.textbbox((tx, ty), s, font=f)
            info.append({"box": (l2, t2, r2 + bold, b2), "x": tx, "base": ty + f.getmetrics()[0], "size": size,
                         "font": opts.get("font", "manu"), "bold": bold, "col": col[:3], "text": s,
                         "shadow": opts.get("shadow", "-")})
    if layer is not None:
        layer = layer.rotate(rot, resample=Image.Resampling.BICUBIC, center=(x + w / 2, y + h / 2))
        base.paste(layer.convert("RGB"), (0, 0), layer)
        rgb = base
    if im.mode == "P":
        out = rgb.quantize(palette=im, dither=Image.Dither.NONE)
        out.info.update(im.info)
        return out
    return rgb


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--install", default=str(ROOT / "original"))
    ap.add_argument("--spec", default=str(ROOT / "lang" / "art_es.tsv"))
    ap.add_argument("--out", default=str(ROOT / "lang" / "es"))
    ap.add_argument("--preview", default="")
    ap.add_argument("--list", default="")
    # P26C (log/roadmap/P26/P26C_notes.md, item 11): for the port's sharp art text, beside each image only (the redrawn image itself
    # is not written): <stem>.blank<ext>, the image with every row's words erased (a rotated row keeps its drawn words), and
    # <stem>.runs.tsv, one line per drawn text line: x0,y0,x1,y1 (ink box) x baseline size font bold r,g,b text shadow (r,g,b of
    # the dark copy one pixel down and right, or -) (image pixels)
    ap.add_argument("--sharp", action="store_true")
    a = ap.parse_args()
    inst = pathlib.Path(a.install)
    if a.list:
        for r in plates(Image.open(inst / a.list)):
            print("%d,%d,%d,%d" % r)
        return 0
    rows = {}
    for n, line in enumerate(pathlib.Path(a.spec).read_text(encoding="utf-8-sig").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        p = line.split("\t")
        if len(p) < 3:
            sys.exit(f"{a.spec} line {n}: image<TAB>x,y,w,h<TAB>text")
        opts = dict(o.split("=", 1) for o in p[3:] if "=" in o)
        rows.setdefault(p[0], []).append((tuple(int(v) for v in p[1].split(",")), p[2], opts))
    if a.sharp:
        for img, items in rows.items():
            im, blank, runs = Image.open(inst / img), Image.open(inst / img), []
            for rect, text, opts in items:
                info = []
                im = redraw(im, rect, text, opts, inst, info)
                blank = redraw(blank, rect, text if float(opts.get("rot", 0)) else "~", opts, inst)
                runs += info
            src = pathlib.Path(img)
            dst = pathlib.Path(a.out) / src.parent / (src.stem + ".blank" + src.suffix)
            dst.parent.mkdir(parents=True, exist_ok=True)
            blank.save(dst)
            with open(pathlib.Path(a.out) / src.parent / (src.stem + ".runs.tsv"), "w", encoding="utf-8", newline="") as o:
                for r in runs:
                    o.write("%d,%d,%d,%d\t%d\t%d\t%d\t%s\t%d\t%d,%d,%d\t%s\t%s\n" % (*r["box"], r["x"], r["base"], r["size"], r["font"],
                                                                               r["bold"], *r["col"], r["text"], r["shadow"]))
            same = (pathlib.Path(a.out) / img).exists() and list(Image.open(pathlib.Path(a.out) / img).getdata()) == list(im.getdata())
            print(f"{img}: {len(runs)} runs, blank -> {dst} (the redrawn image equals {a.out}: {same})")
        return 0
    for img, items in rows.items():
        im = Image.open(inst / img)
        for rect, text, opts in items:
            im = redraw(im, rect, text, opts, inst)
        dst = pathlib.Path(a.out) / img
        dst.parent.mkdir(parents=True, exist_ok=True)
        im.save(dst)
        if a.preview:
            pv = pathlib.Path(a.preview)
            pv.mkdir(parents=True, exist_ok=True)
            im.convert("RGB").save(pv / (pathlib.Path(img).stem + ".png"))
        print(f"{img}: {len(items)} plates -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
