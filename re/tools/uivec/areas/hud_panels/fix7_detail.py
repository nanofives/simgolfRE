"""print which layer parameters differ between a reference state and the others (after the whole-px move)"""
import sys
import numpy as np
import fix7_audit as A

def detail(el, ref="idle", lim=12):
    doc = A.approved(el); sts = doc["states"]
    base = A.cell_rel(sts[ref])
    for st, v in sts.items():
        if st == ref: continue
        s = A.cell_rel(v)
        for i, (a, b) in enumerate(zip(base, s)):
            ok, t, it, res, rs = A.item_compare(a, b)
            if not ok: print(el, st, i, "structure"); continue
            out = []
            for k, (La, Lb) in enumerate(zip(a["layers"], b["layers"])):
                p1, s1 = A.layer_geom(La); p2, s2 = A.layer_geom(Lb)
                for (n, x1, y1), (_, x2, y2) in zip(p1, p2):
                    d = max(abs(x2 - x1 - it[0]), abs(y2 - y1 - it[1]))
                    if d > 0.2: out.append((d, f"L{k}:{La.get('name', La['shape']['t'])}{n} ({x1:.2f},{y1:.2f})->({x2-it[0]:.2f},{y2-it[1]:.2f})"))
                for (n, v1), (_, v2) in zip(s1, s2):
                    if abs(v2 - v1) > 0.2: out.append((abs(v2 - v1), f"L{k}:{La.get('name', La['shape']['t'])}{n} {v1:.2f}->{v2:.2f}"))
            out.sort(reverse=True)
            print(f"{el} {st} item{i} move {it}: " + " | ".join(o for _, o in out[:lim]))

if __name__ == "__main__":
    ref = "idle"
    args = sys.argv[1:]
    if args and args[0].startswith("--ref="):
        ref = args[0][6:]; args = args[1:]
    for el in args: detail(el, ref)
