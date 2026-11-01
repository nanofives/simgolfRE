"""S3.21 (U-1392): the real GL's per-vertex lit colour (feedback points, GL_3D_COLOR_TEXTURE) against GlRaster's double lighting
and float32 candidates.

    ~/bin/py -3.12 log/roadmap/S3/21_litfit.py <corpus> <step>     (points from 21_vcheck.py's scratch: <corpus>_<step>.f32)
"""
import importlib.util
import math
import os
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
_s = importlib.util.spec_from_file_location("c21", HERE / "21_vcheck.py")
C = importlib.util.module_from_spec(_s)
_s.loader.exec_module(C)
V = C.V
f32 = np.float32


def parse(corpus, step):
    """per emitted vertex: (object xyz, normal, lighting on, current colour, mv double, mvp)"""
    root = V.ROOT / "port" / "corpus" / "gl" / corpus
    lines = (root / "stream.gl.txt").read_text().splitlines()
    sd = root / "snaps" / step
    meta = dict(ln.split(" ", 1) for ln in (sd / "meta.txt").read_text().splitlines() if " " in ln)
    sidx = int(meta["stateIdx"])
    ctx = next(i for i, ln in enumerate(lines) if ln.startswith("wglCreateContext"))
    st = {"mode": 0x1700, "mv": np.eye(4), "smv": [], "light": {}, "lit": False, "l0": False, "col": [1, 1, 1, 1], "n": [0, 0, 1],
          "spec": [0, 0, 0, 1], "sh": 0.0}

    def op(name, a, v):
        if name == "glMatrixMode": st["mode"] = int(a[0], 16)
        elif st["mode"] == 0x1700 and name == "glLoadIdentity": st["mv"] = np.eye(4)
        elif st["mode"] == 0x1700 and name == "glRotated":
            st["mv"] = st["mv"] @ V.rot(*[V.dbits(a[2 * i], a[2 * i + 1]) for i in range(4)], "f64")
        elif st["mode"] == 0x1700 and name == "glRotatef":
            st["mv"] = st["mv"] @ V.rot(*[V.fbits(x) for x in a[:4]], "f64")
        elif st["mode"] == 0x1700 and name == "glTranslatef":
            st["mv"] = st["mv"] @ V.trans(*[V.fbits(x) for x in a[:3]])
        elif st["mode"] == 0x1700 and name == "glPushMatrix": st["smv"].append(st["mv"].copy())
        elif st["mode"] == 0x1700 and name == "glPopMatrix": st["mv"] = st["smv"].pop()
        elif name == "glLightfv" and int(a[0], 16) == 0x4000:
            vals = [V.fbits(x) for x in v[:4]]
            pn = int(a[1], 16)
            if pn == 0x1203:
                m = st["mv"]
                vals = [float(f32(m[i, 0] * vals[0] + m[i, 1] * vals[1] + m[i, 2] * vals[2] + m[i, 3] * vals[3])) for i in range(4)]
            st["light"][pn] = vals
        elif name == "glMaterialfv" and int(a[1], 16) == 0x1202: st["spec"] = [V.fbits(x) for x in v[:4]]
        elif name == "glMateriali" and int(a[1], 16) == 0x1601: st["sh"] = float(int(a[2], 16))
        elif name == "glEnable" and int(a[0], 16) == 0x0B50: st["lit"] = True
        elif name == "glDisable" and int(a[0], 16) == 0x0B50: st["lit"] = False
        elif name == "glEnable" and int(a[0], 16) == 0x4000: st["l0"] = True
        elif name == "glDisable" and int(a[0], 16) == 0x4000: st["l0"] = False
        elif name == "glColor4f": st["col"] = [V.fbits(x) for x in a[:4]]
        elif name == "glNormal3fv": st["n"] = [V.fbits(x) for x in v[:3]]
        elif name == "glNormal3f": st["n"] = [V.fbits(x) for x in a[:3]]

    inb = False
    for ln in lines[ctx + 1:sidx]:
        n, a, v = V.args(ln)
        if n == "glBegin": inb = True
        elif n == "glEnd": inb = False
        elif not inb: op(n, a, v)
    vtx = np.frombuffer((sd / "vtx.bin").read_bytes(), "<f4").reshape(-1, 3)
    nrm = np.frombuffer((sd / "nrm.bin").read_bytes(), "<f4").reshape(-1, 3)
    out = []
    for ln in (sd / "pass.gl.txt").read_text().splitlines():
        n, a, v = V.args(ln)
        if n == "glArrayElement":
            i = int(a[0], 16)
            out.append((vtx[i], nrm[i] if i < len(nrm) else np.array(st["n"], np.float32), st["lit"], list(st["col"]), st["mv"]))
        elif n == "glVertex3fv":
            out.append((np.array([V.fbits(x) for x in v[:3]], np.float32), np.array(st["n"], np.float32), st["lit"], list(st["col"]), st["mv"]))
        elif n == "glVertex3f":
            out.append((np.array([V.fbits(x) for x in a[:3]], np.float32), np.array(st["n"], np.float32), st["lit"], list(st["col"]), st["mv"]))
        else:
            op(n, a, v)
    return out, st


def spec_tab(nh, sh):
    t = 0.0005 ** (1.0 / sh)
    if nh <= t:
        return 0.0
    i = min(255.0, math.floor((nh - t) * (255.0 / (1.0 - t)) + 0.5))
    return (t + i / (255.0 / (1.0 - t))) ** sh


def glraster_colour(o, nv, lit, col, mv, st):
    """GlRaster::emit's lighting (double)"""
    if not lit:
        return col[:3]
    m3 = np.array([[mv[0, 0], mv[0, 1], mv[0, 2]], [mv[1, 0], mv[1, 1], mv[1, 2]], [mv[2, 0], mv[2, 1], mv[2, 2]]])
    inv = np.linalg.inv(m3)
    n = inv.T @ np.array(nv, np.float64)
    amb, dif, spc, pos = (st["light"].get(k, d) for k, d in ((0x1200, [0, 0, 0, 1]), (0x1201, [1, 1, 1, 1]), (0x1202, [1, 1, 1, 1]), (0x1203, [0, 0, 1, 0])))
    c = [0.04] * 3
    if st["l0"]:
        l = np.array(pos[:3], np.float64); l /= np.linalg.norm(l)
        ndl = max(0.0, float(n @ l))
        for k in range(3):
            c[k] += amb[k] * 0.2 + ndl * dif[k] * 0.8
        if ndl > 0:
            h = l + np.array([0, 0, 1.0]); h /= np.linalg.norm(h)
            nh = max(0.0, float(n @ h))
            f = spec_tab(nh, st["sh"]) if st["sh"] > 0 else 0.0
            for k in range(3):
                c[k] += f * spc[k] * st["spec"][k]
    return [min(1.0, max(0.0, x)) for x in c]


def main():
    corpus, step = sys.argv[1], sys.argv[2]
    pts = V.read_points(C.OUT / f"{corpus}_{step}.f32")
    verts, vp = V.build(corpus, step, "f32")
    p = C.predict(verts, vp)
    ins = p[:, 2] > 0
    lv, st = parse(corpus, step)
    assert len(lv) == len(verts)
    lv = [x for x, keep in zip(lv, ins) if keep]
    got = pts[:, 3:6].astype(np.float64)
    ours = np.array([glraster_colour(*x, st) for x in lv])
    d = np.abs(ours - got)
    print("vertices", len(lv), "lit", sum(1 for x in lv if x[2]), "light", st["light"], "sh", st["sh"])
    print("max |double - real|", d.max(), " > 1e-7:", int((d > 1e-7).sum()), " > 1e-6:", int((d > 1e-6).sum()), "of", d.size)
    print("float32 of the double equals the real:", int((ours.astype(np.float32) == pts[:, 3:6]).sum()), "of", d.size)
    k = np.argsort(-d.max(1))[:8]
    for i in k:
        print("  ", ours[i].tolist(), got[i].tolist(), lv[i][1].tolist())


if __name__ == "__main__":
    main()
