"""S3.8: where the overlay's line pixels miss the reference in one checkpoint (bounding boxes of the beyond-one-level pixels, and
a crop of base / s38 / reference side by side, magnified, in log/roadmap/S3/8_img/)."""
import pathlib, sys
import numpy as np
from PIL import Image
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "build" / "S3_8_src"))
from port.test import gfx as GX  # noqa: E402
base_dir, s38_dir, scen, chk = sys.argv[1:5]
b = np.asarray(Image.open(f"{base_dir}/{scen}.{chk}.port.png").convert("RGB")).astype(int) >> 3
s = np.asarray(Image.open(f"{s38_dir}/{scen}.{chk}.port.png").convert("RGB")).astype(int) >> 3
r = GX.reference(scen, chk).astype(int) >> 3
line = (b != s).any(axis=2)
d = np.abs(s - r).max(axis=2)
bad = line & (d > 1)
ys, xs = np.nonzero(bad)
print("line", int(line.sum()), "beyond", int(bad.sum()))
if len(ys):
    print("beyond bbox x %d..%d y %d..%d" % (xs.min(), xs.max(), ys.min(), ys.max()))
    # rows histogram
miss = (~line) & (np.abs(b - r).max(axis=2) > 0) & (np.abs(s - r).max(axis=2) > 0)
out = ROOT / "log" / "roadmap" / "S3" / "8_img"; out.mkdir(exist_ok=True)
box = sys.argv[5] if len(sys.argv) > 5 else ("%d,%d,%d,%d" % (max(0, xs.min() - 30), max(0, ys.min() - 30), min(800, xs.max() + 30), min(600, ys.max() + 30)) if len(ys) else None)
if box:
    x0, y0, x1, y1 = map(int, box.split(","))
    crops = [np.asarray(Image.open(f"{d_}/{scen}.{chk}.port.png").convert("RGB"))[y0:y1, x0:x1] for d_ in (base_dir, s38_dir)]
    crops.append(GX.reference(scen, chk)[y0:y1, x0:x1].astype(np.uint8))
    img = np.concatenate(crops, axis=1)
    Image.fromarray(img).resize((img.shape[1] * 4, img.shape[0] * 4), Image.NEAREST).save(out / f"{scen}.{chk}.png")
