"""S3.21 (U-1392): check the measured vertex rule on every recorded pass against the real GL's feedback points.

Rule (21_notes.md section 2): float32 matrices; MVP = P * MV in float32; clip = m3 + x*m0 + y*m1 + z*m2, each op float32;
window = round_half_even(256 * (f32(f32(ndc) * f32(h - 2^-8)) + c)) / 256 (c, h: viewport centre and half-size), ndc = clip / w
(w = 1 for the terrain's orthographic projection: checked, not assumed).

    ~/bin/py -3.12 log/roadmap/S3/21_vcheck.py [corpus ...]      # runs glfeed.exe (GLFEED_POINTS=1) per pass, prints wrong counts
"""
import importlib.util
import os
import pathlib
import subprocess
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
_s = importlib.util.spec_from_file_location("v21", HERE / "21_vfit.py")
V = importlib.util.module_from_spec(_s)
_s.loader.exec_module(V)
f32 = np.float32
EXE = HERE / "21_glfeed" / "glfeed.exe"
OUT = pathlib.Path(os.environ.get("S321_SCRATCH", HERE / "21_probe_out")) / "vcheck"


def predict(verts, vp):
    """vectorised over runs of vertices that share their matrices"""
    x0, y0, w, h = vp
    hx, hy = w / 2.0, h / 2.0
    res = np.zeros((len(verts), 3))
    i = 0
    while i < len(verts):
        mv, pr, mm = verts[i][1], verts[i][2], verts[i][3]
        j = i
        while j < len(verts) and verts[j][1] is mv and verts[j][2] is pr and verts[j][3] is mm:
            j += 1
        O = np.array([v[0] for v in verts[i:j]], np.float64).astype(np.float32)
        mvp = verts[i][3] if os.environ.get("S321_INC", "1") == "1" else V.mul(pr, mv, os.environ.get("S321_MVP", "f32"))
        c = []
        for r in range(4):
            if os.environ.get("S321_ORDER", "d") == "d":   # translation first
                s = np.full(len(O), f32(mvp[r, 3]), np.float32)
                for k in range(3):
                    s = f32(s + f32(O[:, k] * f32(mvp[r, k])))
            else:                                          # left to right, translation last
                s = f32(O[:, 0] * f32(mvp[r, 0]))
                for k in (1, 2):
                    s = f32(s + f32(O[:, k] * f32(mvp[r, k])))
                s = f32(s + f32(mvp[r, 3]))
            c.append(s)
        cw = c[3]
        res[i:j, 2] = (np.abs(c[0]) <= cw) & (np.abs(c[1]) <= cw) & (np.abs(c[2]) <= cw)
        inv = f32(f32(1) / cw)
        nx = np.where(cw == 1, c[0], f32(c[0] * inv)); ny = np.where(cw == 1, c[1], f32(c[1] * inv))
        res[i:j, 0] = np.round(256 * (f32(nx * f32(hx - 1 / 256))).astype(np.float64) + 256 * (x0 + hx)) / 256
        res[i:j, 1] = np.round(256 * (f32(ny * f32(hy - 1 / 256))).astype(np.float64) + 256 * (y0 + hy)) / 256
        res[i:j, 2] *= 1 if np.all(cw == 1) else 2   # 2 marks a run with w != 1
        i = j
    return res


def double_model(verts, vp):
    x0, y0, w, h = vp
    hx, hy = w / 2.0, h / 2.0
    out = []
    for o, mv, pr, _ in verts:
        c = pr @ (mv @ np.append(o, 1.0))
        out.append((np.round(256 * (x0 + hx + (c[0] / c[3]) * (hx - 1 / 256))) / 256,
                    np.round(256 * (y0 + hy + (c[1] / c[3]) * (hy - 1 / 256))) / 256))
    return np.array(out)


def check(corpus, step):
    OUT.mkdir(parents=True, exist_ok=True)
    f = OUT / f"{corpus}_{step}.f32"
    if not f.exists():
        r = subprocess.run([str(EXE), str(ROOT / "port/corpus/gl" / corpus), step, str(f)], env=dict(os.environ, GLFEED_POINTS="1"),
                           capture_output=True, text=True)
        if r.returncode != 0:
            return None
    pts = V.read_points(f)
    verts, vp = V.build(corpus, step, os.environ.get("S321_MAT", "f32"))
    p = predict(verts, vp)
    ins = p[:, 2] > 0
    if ins.sum() != len(pts):
        return ("PAIRING", int(ins.sum()), len(pts))
    if len(pts) == 0:
        return (0, 0, 0, 0, 0, 0)
    q = p[ins]
    ex = int((q[:, 0] != pts[:, 0]).sum()); ey = int((q[:, 1] != pts[:, 1]).sum())
    vd, _ = V.build(corpus, step, "f64")
    d = double_model(vd, vp)[ins]
    dx = int((d[:, 0] != pts[:, 0]).sum()); dy = int((d[:, 1] != pts[:, 1]).sum())
    return (ex, ey, dx, dy, len(pts), int((p[:, 2] == 2).sum()))


def main():
    corpora = sys.argv[1:] or ["c3", "c4", "c5", "c6", "c7", "g2", "g3", "g4", "g5", "g6"]
    tot = np.zeros(4, int); npts = 0; bad = 0; n = 0
    for c in corpora:
        for step in sorted(p.name for p in (ROOT / "port/corpus/gl" / c / "snaps").iterdir()):
            r = check(c, step)
            n += 1
            if r is None or r[0] == "PAIRING":
                print(f"{c}/{step}: {r}", flush=True); bad += 1
                continue
            ex, ey, dx, dy, k, w2 = r
            tot += (ex, ey, dx, dy); npts += k
            if ex or ey or w2:
                bad += 1
            print(f"{c}/{step}: points {k} rule x/y wrong {ex}/{ey}  double model {dx}/{dy}{'  w!=1 runs ' + str(w2) if w2 else ''}", flush=True)
    print(f"TOTAL passes {n} points {npts} rule wrong x {tot[0]} y {tot[1]}; double model wrong x {tot[2]} y {tot[3]}; passes not exact {bad}")


if __name__ == "__main__":
    main()
