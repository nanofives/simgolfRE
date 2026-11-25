"""py -3.12 side.py out.png scale id ... : put comparison images side by side, scaled."""
import os, sys
from PIL import Image
import common as C
out, k = sys.argv[1], float(sys.argv[2])
ims = [Image.open(os.path.join(C.OUT, i + ".png")) for i in sys.argv[3:]]
ims = [i.resize((int(i.size[0] * k), int(i.size[1] * k)), Image.LANCZOS) for i in ims]
sh = Image.new("RGB", (sum(i.size[0] + 10 for i in ims), max(i.size[1] for i in ims)), "white")
x = 0
for i in ims:
    sh.paste(i, (x, 0)); x += i.size[0] + 10
sh.save(out); print(out, sh.size)
