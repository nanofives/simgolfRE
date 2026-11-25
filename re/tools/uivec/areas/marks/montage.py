"""py -3.12 montage.py out.png id ... : stack comparison images (scaled to width 640) for viewing."""
import os, sys
from PIL import Image
import common as C
out = sys.argv[1]
ims = []
for i in sys.argv[2:]:
    im = Image.open(os.path.join(C.OUT, i + ".png"))
    k = min(1.0, 400 / im.size[0])
    ims.append(im.resize((int(im.size[0] * k), int(im.size[1] * k)), Image.LANCZOS))
cols = 3 if len(ims) > 3 else len(ims)
W = max(i.size[0] for i in ims) + 10
H = max(i.size[1] for i in ims) + 10
rows = (len(ims) + cols - 1) // cols
sh = Image.new("RGB", (W * cols, H * rows), "white")
for k, im in enumerate(ims):
    sh.paste(im, ((k % cols) * W, (k // cols) * H))
sh.save(out)
print(out, sh.size)
