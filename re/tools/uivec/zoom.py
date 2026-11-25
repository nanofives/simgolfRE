"""uivec: an enlarged, gridded view of a frame box of the title art (base + overlay cell) for reading shapes by eye.
Usage: py -3.12 re/tools/uivec/zoom.py <x> <y> <w> <h> [unsel|mo] [scale] [out.png]
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import fit  # noqa: E402


def title_art(over="unsel"):
    base_idx, base_pal = fit.load_sheet("Interface/TitleBASE.pcx")
    o_idx, o_pal = fit.load_sheet("Interface/TitleUnSel.pcx" if over == "unsel" else "Interface/TitleMO.pcx")
    art = base_pal[base_idx].copy()
    m = o_idx < 0xfe
    art[m] = o_pal[o_idx][m]
    return art


def main():
    x, y, w, h = (int(v) for v in sys.argv[1:5])
    over = sys.argv[5] if len(sys.argv) > 5 else "unsel"
    s = int(sys.argv[6]) if len(sys.argv) > 6 else 14
    out = sys.argv[7] if len(sys.argv) > 7 else os.path.join(fit.ROOT, "log", "roadmap", "P41", "title", "zoom.png")
    art = title_art(over)[y:y + h, x:x + w]
    big = Image.fromarray(art).resize((w * s, h * s), Image.NEAREST)
    d = ImageDraw.Draw(big)
    for g in range(0, max(w, h), 2):
        major = (g % 10) == 0
        col = (255, 0, 0) if major else (255, 170, 170)
        if g < w:
            d.line([(g * s, 0), (g * s, h * s)], fill=col)
            if major:
                d.text((g * s + 2, 2), str(x + g), fill=(255, 255, 0))
        if g < h:
            d.line([(0, g * s), (w * s, g * s)], fill=col)
            if major:
                d.text((2, g * s + 2), str(y + g), fill=(255, 255, 0))
    big.save(out)
    print(out)


if __name__ == "__main__":
    main()
