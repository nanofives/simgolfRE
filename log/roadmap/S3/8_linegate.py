"""S3.8 line gate over framegate images: per checkpoint, the pixels the overlay bodies changed (the S3.8 build's frame against the base
build's, both from port.test.framegate --images) judged against the original's frame (port.test.gfx.reference).

    ~/bin/py -3.12 log/roadmap/S3/8_linegate.py <img_base> <img_s38> [--out tsv] [--scenarios a,b]

Columns: line_px (changed by the overlay), exact / one (largest 5-bit channel difference 1) / beyond (more) among them, then
fixed (base differed from the reference, S3.8 equals it), broke (base equal, S3.8 differs), and the G2 verdict on the line pixels
(one <= 1.5 % of line_px, beyond <= 70; port/test/glraster.py's constants as the census applies them).
"""
from __future__ import annotations

import argparse
import pathlib
import sys

import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "port" / "build" / "S3_8_src"
sys.path.insert(0, str(SRC))
from port.test import gfx as GX  # noqa: E402

G2_ONE_PCT, G2_BEYOND = 1.5, 70


def five(a: np.ndarray) -> np.ndarray:
    return (a.astype(np.int32) >> 3)


def judge(base: np.ndarray, s38: np.ndarray, ref: np.ndarray) -> dict:
    b5, s5, r5 = five(base), five(s38), five(ref)
    line = (b5 != s5).any(axis=2)
    d = np.abs(s5 - r5).max(axis=2)
    db = np.abs(b5 - r5).max(axis=2)
    n = int(line.sum())
    one = int(((d == 1) & line).sum())
    beyond = int(((d > 1) & line).sum())
    out = dict(line_px=n, exact=n - one - beyond, one=one, beyond=beyond,
               fixed=int(((db > 0) & (d == 0)).sum()), broke=int(((db == 0) & (d > 0)).sum()),
               total_base=int((db > 0).sum()), total_s38=int((d > 0).sum()))
    out["g2"] = "PASS" if n and one <= G2_ONE_PCT / 100.0 * n and beyond <= G2_BEYOND else ("-" if not n else "FAIL")
    # judged line pixels: those whose 7 x 7 neighbourhood holds no other pixel the base frame gets beyond one level wrong (outside the
    # line pixels): a later layer the port does not draw (a sprite, a panel, another screen) leaves its own errors around the line
    other = (db > 1) & ~line
    near = np.zeros_like(other)
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            near |= np.roll(np.roll(other, dy, axis=0), dx, axis=1)
    j = line & ~near
    jn, jone, jb = int(j.sum()), int(((d == 1) & j).sum()), int(((d > 1) & j).sum())
    out.update(judged=jn, j_one=jone, j_beyond=jb,
               j_g2="PASS" if jn and jone <= G2_ONE_PCT / 100.0 * jn and jb <= G2_BEYOND else ("-" if not jn else "FAIL"))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base", type=pathlib.Path)
    ap.add_argument("s38", type=pathlib.Path)
    ap.add_argument("--out", type=pathlib.Path)
    ap.add_argument("--scenarios")
    a = ap.parse_args()
    want = set(a.scenarios.split(",")) if a.scenarios else None
    rows = []
    for p in sorted(a.s38.glob("*.port.png")):
        scen, chk = p.name[:-len(".port.png")].split(".", 1)
        if want and scen not in want:
            continue
        q = a.base / p.name
        if not q.exists():
            continue
        ref = GX.reference(scen, chk)
        r = judge(np.asarray(Image.open(q).convert("RGB")), np.asarray(Image.open(p).convert("RGB")), ref)
        rows.append(dict(scenario=scen, checkpoint=chk, **r))
    cols = ["scenario", "checkpoint", "line_px", "exact", "one", "beyond", "fixed", "broke", "total_base", "total_s38", "g2", "judged", "j_one", "j_beyond", "j_g2"]
    lines = ["\t".join(cols)] + ["\t".join(str(r[c]) for c in cols) for r in rows]
    tot = {c: sum(r[c] for r in rows) for c in cols[2:] if c not in ("g2", "j_g2")}
    with_lines = [r for r in rows if r["line_px"]]
    lines.append("# rows %d, with line pixels %d, G2 PASS %d FAIL %d; sums %s" % (
        len(rows), len(with_lines), sum(r["g2"] == "PASS" for r in with_lines), sum(r["g2"] == "FAIL" for r in with_lines), tot))
    lines.append("# judged: G2 PASS %d FAIL %d (rows with judged line pixels)" % (
        sum(r["j_g2"] == "PASS" for r in rows), sum(r["j_g2"] == "FAIL" for r in rows)))
    text = "\n".join(lines) + "\n"
    if a.out:
        a.out.write_text(text)
    print(text if len(rows) < 80 else "\n".join(lines[-2:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
