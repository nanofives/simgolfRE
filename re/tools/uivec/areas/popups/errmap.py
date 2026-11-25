"""Per-pixel error of a saved scene against the art, as digits (max channel error / 10, '.' under 10)."""
import json, sys, os
import numpy as np
import common as c


def errmap(scene, rel, box, ground=c.FLAT, s=5):
    x0, y0, x1, y1 = box
    low = c.render_box(scene, box, s, ground).reshape(y1 - y0, s, x1 - x0, s, 3).mean(axis=(1, 3))
    a = c.art(rel, box, ground)
    return np.abs(low - a).max(axis=2), low, a


if __name__ == "__main__":
    eid, st = sys.argv[1], sys.argv[2]
    d = json.load(open(os.path.join(c.OUT, eid + ".json")))
    S = d["states"][st]
    e, low, a = errmap(S["scene"], d["sheet"], S["box"])
    print("    " + "".join(str(x % 10) for x in range(e.shape[1])))
    for j in range(e.shape[0]):
        print(f"{j:3d} " + "".join("." if v < 10 else str(min(int(v / 10), 9)) for v in e[j]))
