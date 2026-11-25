import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bl_balls as B, bl_caps as K, bubbles as BU, common as C, deliver as D, view as V

S = BU.load_style()
j = json.load(open(os.path.join(C.OUT, "read", "caps_fit.json")))
insts = [i for i in B.instances() if i["state"] == "backdrop"]
scene = K.items(j["geom"], j["style"]) + [BU.item(i, S) for i in insts]
rgb, m = C.sheet("buy_land.pcx")
x, y, w, h = 12, 48, 776, 210
art = rgb[y:y + h, x:x + w]
base = np.empty_like(art); base[:] = B.LAV
full_base = np.empty_like(rgb); full_base[:] = B.LAV
det = []
for win in ((12, 48, 130, 76), (230, 108, 110, 84), (520, 180, 130, 78)):
    a = rgb[win[1]:win[1] + win[3], win[0]:win[0] + win[2]]
    b = full_base[win[1]:win[1] + win[3], win[0]:win[0] + win[2]]
    det.append(D.tile(a, b, scene, win[:2], 5))
D.deliver("buyland-capsules", [("idle", art, base, scene, (x, y))],
          "nine flat rounded boxes (line-centre boxes read from the navy pixel rows: 37.5/295.5/561.5..257.9/515.9/781.9 x "
          "53/121/189..117/185/253, corner r 9; the left corner meets the ball's rim), cream or tan, 2 px navy line, an inner "
          "shade under the top and left walls and a soft drop shadow down-right (both fitted); each with its blue C1 number ball "
          "(paint measured on the nine blue balls, digits from digits.py). Full view on top, three details at 5x below",
          unsure="the capsule line is a touch heavier than the art's", extra_tiles=det, z=2)
