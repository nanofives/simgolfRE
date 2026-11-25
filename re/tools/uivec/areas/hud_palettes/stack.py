"""Stack several comparison pngs side by side (scaled to a common height) for one look.  py -3.12 stack.py out.png h id..."""
import os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp
out, h = sys.argv[1], int(sys.argv[2])
ims = []
for el in sys.argv[3:]:
    im = Image.open(os.path.join(hp.OUT, el + ".png"))
    s = h / im.height
    ims.append(im.resize((max(1, int(im.width * s)), h), Image.LANCZOS))
W = sum(i.width for i in ims) + 8 * len(ims)
can = Image.new("RGB", (W, h), (0, 0, 0))
x = 0
for i in ims:
    can.paste(i, (x, 0)); x += i.width + 8
can.save(out); print(out, can.size)
