"""Deliver buyland-ball-1..9 (lit over dim)."""
import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bl_balls as B, bubbles as BU, common as C, deliver as D, digits as DG

S = BU.load_style()
S["edge"] = [0.15, 0.15]       # the rim 1.7 px, as the art's (the fit's 0.6 / 0.1 drew it 2.2 px and heavy)
with open(BU.STYLE_FILE, "w") as f:
    json.dump(S, f)
insts = B.instances()
WHAT = {n: dict(DG.authored()[n]) for n in range(1, 10)}
for n in range(1, 10):
    states = []
    for inst in insts:
        if inst["n"] != n or inst["state"] == "backdrop":
            continue
        rgb, _ = C.sheet(inst["sheet"])
        x0, y0, x1, y1 = inst["box"]
        states.append((inst["state"], rgb[y0:y1, x0:x1], B.base_for(inst), [BU.item(inst, S)], (x0, y0)))
    parts = "; ".join(DG.authored()[n][k][0] for k in range(len(DG.authored()[n])))
    D.deliver(f"buyland-ball-{n}", states,
              f"C1 glass bubble (navy rim 1.7 px, body lit by a measured bevel: yellow lit / silver dim, specular spot upper left, "
              f"soft cast shadow on its capsule) with the digit {n} authored from its parts ({parts}); drawn over this area's own "
              f"capsule and lavender",
              unsure="the art's rim is softer (antialiased into its shadow); the digits' strokes are a little lighter than the art's")
