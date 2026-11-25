"""Tile several comparison pngs into one look image.  py -3.12 sheet.py out.png cols shrink id..."""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402

out, cols, shrink = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
ims = [Image.open(os.path.join(hp.OUT, i + ".png")) for i in sys.argv[4:]]
ims = [i.resize((int(i.width / shrink), int(i.height / shrink)), Image.NEAREST) for i in ims]
W = max(i.width for i in ims)
H = max(i.height for i in ims)
can = Image.new("RGB", (cols * (W + 6), ((len(ims) + cols - 1) // cols) * (H + 6)))
for k, i in enumerate(ims):
    can.paste(i, ((k % cols) * (W + 6), (k // cols) * (H + 6)))
can.save(out)
print(out, can.size)
