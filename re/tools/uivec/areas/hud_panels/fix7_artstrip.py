"""art of every state of an element side by side at z with a pixel grid and the idle rim circle (moved by the given
whole-px move) drawn on each, to see whether the art moves the button."""
import sys, json
import numpy as np
from PIL import Image, ImageDraw
import common as C
import fix7_audit as A

def strip(el, out, z=10):
    doc = A.approved(el); sts = doc["states"]
    key = [k for k, n in C.SHEETS.items() if n.lower() in doc["coords"].lower()][0]
    rgb, m = C.sheet(key)
    ref = "idle" if "idle" in sts else list(sts)[0]
    base = A.cell_rel(sts[ref])
    # the rim circle of idle: the bubble item's first circle with 'linear' fill after the shadow (layer 1)
    circ = [L["shape"] for L in base[-1]["layers"] if L["shape"]["t"] == "circle"]
    tiles = []
    for st, v in sts.items():
        x, y, w, h = v["cell"]
        a = np.clip(rgb[y:y + h, x:x + w] + 0.5, 0, 255).astype(np.uint8)
        im = Image.fromarray(a).resize((w * z, h * z), Image.NEAREST)
        d = ImageDraw.Draw(im)
        for g in range(w + 1): d.line([(g * z, 0), (g * z, h * z)], fill=(90, 90, 90))
        for g in range(h + 1): d.line([(0, g * z), (w * z, g * z)], fill=(90, 90, 90))
        s = A.cell_rel(v)
        it = A.item_compare(base[-1], s[-1])[2] if st != ref else (0, 0)
        for c in circ[1:3]:
            cx, cy, r = c["c"][0] + it[0], c["c"][1] + it[1], c["r"]
            d.ellipse([(cx - r) * z, (cy - r) * z, (cx + r) * z, (cy + r) * z], outline=(255, 0, 0))
        d.text((3, 3), f"{st} {it}", fill=(255, 255, 0))
        tiles.append(np.asarray(im))
    H = max(t.shape[0] for t in tiles)
    img = np.concatenate([np.pad(t, ((0, H - t.shape[0]), (0, 6), (0, 0)), constant_values=255) for t in tiles], axis=1)
    Image.fromarray(img).save(out)

if __name__ == "__main__":
    import os
    od = sys.argv[1]
    for el in sys.argv[2:]:
        strip(el, os.path.join(od, el + "_art.png"))
