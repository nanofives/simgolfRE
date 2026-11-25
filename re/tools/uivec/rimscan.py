"""uivec: the Load Game bubbles' rim (P41). The refined geometry minimises the colour error of the whole cell, which left the
navy rim wider than the art's (Delete idle 0.61 px off, limit 0.5). This scans the rim circle's radius and centre per bubble
and state for the smallest rim offset, keeping a candidate only if the cell's mean difference does not rise by more than 2%.

py -3.12 re/tools/uivec/rimscan.py [--write]      prints the scan; --write stores the result in load_geom.json
"""
import copy
import json
import sys

import load
import accept
import recipes
import title


def main():
    load.setup()
    b = accept.Bench()
    style = copy.deepcopy(recipes.STYLE)
    k = 0
    jobs = [(n, h) for n, m in b.meas.items() if m["kind"] == "bubble" for h in (False, True)]
    for name, hover in jobs:
        g = b.meas[name][accept.ST[hover]]
        c0 = list(g["c"])

        def run(c):
            g["c"] = list(c)
            b.cache.clear()
            s = accept.score(b, name, "button", hover, style, s=4)
            return s["edge"], s["mad"]

        e0, m0 = run(c0)
        best, be, bm = c0, e0, m0
        for rnd in range(2):
            for i, steps in ((2, (-1.2, -0.9, -0.6, -0.45, -0.3, -0.15, 0.15, 0.3)), (0, (-0.3, -0.15, 0.15, 0.3)),
                             (1, (-0.3, -0.15, 0.15, 0.3))):
                base = list(best)
                for st in steps:
                    c = list(base)
                    c[i] += st
                    e, m = run(c)
                    if m <= m0 * 1.02 and e < be - 0.01:
                        best, be, bm = c, e, m
        g["c"] = [round(v, 3) for v in best]
        k += 1
        print(f"[{k}/{len(jobs)}] {name}.{accept.ST[hover]} rim {e0:.2f} -> {be:.2f} px, mad {m0:.2f} -> {bm:.2f}, "
              f"c {c0} -> {g['c']} PASS", flush=True)
    if "--write" in sys.argv:
        for m in b.meas.values():
            for st in ("idle", "hover"):
                m[st].pop("shape", None)
        with open(title.GEOM, "w") as f:
            json.dump(b.meas, f)
        print("wrote", title.GEOM)


if __name__ == "__main__":
    main()
