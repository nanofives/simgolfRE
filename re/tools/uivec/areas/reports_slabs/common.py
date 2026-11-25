"""P41 area reports_slabs: shared helpers (art access, enlarged gridded views, comparison images).

The report sheets come with an alpha sheet (<name>_alpha.pcx, grey 0..255) that antialiases the slab's outer edge and
removes the designer's green guide marks (opaque in the colour sheet, alpha 0). The art as the player sees it is
colour * alpha + backdrop * (1 - alpha); here the backdrop is the roster's flat green (70, 110, 90).
"""
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
# round 2 (fix round, 2026-11-24) writes to work2; RS_OUT=work brings back round 1
OUT = os.path.join(ROOT, "log", "roadmap", "P41", os.environ.get("RS_OUT", "work2"), "reports_slabs")
os.makedirs(OUT, exist_ok=True)
RESULTS = "results2" if os.environ.get("RS_OUT", "work2") == "work2" else "results"
FLAT = np.array((70, 110, 90), float)
IS = "Interface/infoscreens/"
ALPHA = {"SGA.pcx": "SGAreport_alpha.pcx"}

_cache = {}


def art(sheet):
    """(rgb float H x W x 3 composited over FLAT, alpha 0..1, raw rgb) of a report sheet (name under infoscreens)."""
    if sheet in _cache:
        return _cache[sheet]
    idx, pal = fit.load_sheet(IS + sheet)
    rgb = pal[idx].astype(float)
    an = ALPHA.get(sheet, sheet.replace(".pcx", "_alpha.pcx"))
    if os.path.exists(os.path.join(fit.ORIG, IS + an)):
        ai, ap = fit.load_sheet(IS + an)
        a = ap[ai][..., 0].astype(float) / 255.0
        a = np.where(idx >= 0xfe, 0.0, a)
    else:
        a = (idx < 0xfe).astype(float)
    comp = rgb * a[..., None] + FLAT * (1 - a[..., None])
    _cache[sheet] = (comp, a, rgb)
    return _cache[sheet]


def zoom(img, x, y, w, h, s=8, out=None, grid=True):
    """An enlarged view of img[y:y+h, x:x+w] with a pixel grid (every 10th line brighter) and x / y labels."""
    sub = np.clip(img[y:y + h, x:x + w], 0, 255).astype(np.uint8)
    big = np.kron(sub, np.ones((s, s, 1), np.uint8))
    M = 28
    canvas = Image.new("RGB", (w * s + M, h * s + M), (0, 0, 0))
    canvas.paste(Image.fromarray(big), (M, M))
    d = ImageDraw.Draw(canvas)
    if grid:
        for i in range(w + 1):
            c = (255, 80, 80) if (x + i) % 10 == 0 else (40, 40, 40)
            if s >= 4 or (x + i) % 10 == 0:
                d.line([(M + i * s, M), (M + i * s, M + h * s)], fill=c)
            if (x + i) % 10 == 0:
                d.text((M + i * s + 1, 2), str(x + i), fill=(255, 255, 0))
        for j in range(h + 1):
            c = (255, 80, 80) if (y + j) % 10 == 0 else (40, 40, 40)
            if s >= 4 or (y + j) % 10 == 0:
                d.line([(M, M + j * s), (M + w * s, M + j * s)], fill=c)
            if (y + j) % 10 == 0:
                d.text((1, M + j * s + 1), str(y + j), fill=(255, 255, 0))
    if out:
        canvas.save(out)
    return canvas


def dump(img, x, y, w, h):
    """Rows of 'r,g,b' for a small box (reading exact colours)."""
    for j in range(y, y + h):
        print(j, " ".join("%3d,%3d,%3d" % tuple(int(v) for v in img[j, i]) for i in range(x, x + w)))
