"""Centre of the dark line crossing a scan (darkness-weighted), per column (vertical scans) or per row.
Usage: linecentre.py <sheet> v|h <from> <to> <step> <lo> <hi> [threshold=110]
v: for each x in from..to, scan y in lo..hi; h: for each y, scan x."""
import sys
import numpy as np
import common as C
import fit


def centre(raw, al, mode, k, lo, hi, thr=110.0):
    v = raw[lo:hi, k] if mode == "v" else raw[k, lo:hi]
    L = fit.lum(v)
    w = np.clip(thr - L, 0, None)
    if w.sum() < 20:
        return None, 0
    i = np.arange(lo, hi) + 0.5
    return float((w * i).sum() / w.sum()), float(w.sum() / (thr - 40))


if __name__ == "__main__":
    sh, mode = sys.argv[1], sys.argv[2]
    a0, a1, st, lo, hi = (int(v) for v in sys.argv[3:8])
    thr = float(sys.argv[8]) if len(sys.argv) > 8 else 110.0
    _, al, raw = C.art(sh)
    out = []
    for k in range(a0, a1, st):
        c, wid = centre(raw, al, mode, k, lo, hi, thr)
        out.append(f"{k}:{'-' if c is None else '%.2f/%.1f' % (c, wid)}")
    print(" ".join(out))
