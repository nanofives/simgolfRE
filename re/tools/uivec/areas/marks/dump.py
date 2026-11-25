"""Dump every marks crop as text grids and gridded PNGs into log/roadmap/P41/work/marks/read/."""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as C  # noqa: E402

R = os.path.join(C.OUT, "read")
os.makedirs(R, exist_ok=True)
crops = {}
for i in list(range(0, 5)):
    crops[f"gb-mood-{i}"] = (C.GB, 16 * i, 0x144, 16, 16)
for i in range(5, 35):
    crops[f"gb-{i:02d}"] = (C.GB, 16 * i, 0x144, 16, 16)
for r in range(5):
    for c in range(4):
        crops[f"tack-r{r}c{c}"] = (C.TA, 50 * c, 50 * r, 25, 20)
crops["arrow-a"] = (C.TA, 0, 250, 46, 54)
crops["arrow-b"] = (C.TA, 50, 250, 46, 54)
for n, (x, y, w, h) in {"cash-a": (350, 69, 31, 31), "cash-b": (450, 69, 32, 32), "fun-a": (350, 169, 31, 32),
                        "fun-b": (450, 169, 32, 32), "skill-a": (350, 269, 31, 31), "skill-b": (450, 269, 32, 32)}.items():
    crops["course-" + n] = (C.CI, x, y, w, h)
for k, (rel, x, y, w, h) in crops.items():
    C.textgrid(rel, x, y, w, h, os.path.join(R, k + ".txt"))
    art, m = C.crop(rel, x, y, w, h)
    C.gridpng(C.on_flat(art, m), 24 if w <= 32 else 14, os.path.join(R, k + ".png"))
print(len(crops))
