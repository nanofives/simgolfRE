"""S3.8 probes: Terrain::drawLine's exact GL call sequence through the REAL software GL (re/tools/glexact/glreplay.exe), so the
port's line rasteriser can be fitted to Windows' generic implementation stage by stage (the method of port/test/glprobe.py).

The context state is the one Terrain::initGL leaves (recorded in every port/corpus/gl stream after wglCreateContext): GL_LINE_SMOOTH on
with GL_LINE_SMOOTH_HINT = GL_NICEST, texturing and lighting on, front face GL_CW, culling on, viewport 0, 0, 800, 600. Each line is the
original's sequence (re/analysis/terrain/000048a0_TerrainDll_Terrain_drawLine.md): projection push + glOrtho(0, 800, 0, 600, -1, 1)
(the flip byte +0x28 is set in every recorded stream), modelview push, lighting / texturing off, blending on with
(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), glLineWidth(width), glColor4f(r, g, b, alpha / 10), one GL_LINES pair of glVertex2i, then the
state undone and glFlush.

    ~/bin/py -3.12 log/roadmap/S3/8_probe.py <name> ...     (writes port/corpus/gl/probe/s38_<name>/, prints a summary)
"""
from __future__ import annotations

import pathlib
import struct
import subprocess
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import glprobe as P  # noqa: E402

W, H = 800, 600
INIT = ["glHint 0xc50,0x1101", "glEnable 0xb20", "glHint 0xc52,0x1102", "glEnable 0xde1", "glEnable 0xb50", "glFrontFace 0x900",
        "glEnable 0xb44", "glViewport 0x0,0x0,0x320,0x258"]


def fbits(x: float) -> str:
    return "0x%x" % struct.unpack("<I", struct.pack("<f", x))[0]


def chan(color: int) -> tuple[float, float, float]:
    """drawLine's float channels: ((c >> 7) & 0xf8) / 255.0f etc. (single precision, as the original's fstp)"""
    def f(v):
        return struct.unpack("<f", struct.pack("<f", np.float32(v) / np.float32(255.0)))[0]
    return f((color >> 7) & 0xF8), f((color >> 2) & 0xF8), f((color << 3) & 0xF8)


def alpha_f(alpha: int) -> float:
    return float(np.float32(alpha) / np.float32(10.0))


def line_lines(x1, y1, x2, y2, color, width, alpha, flip=True):
    r, g, b = chan(color)
    o = [P.db(0), P.db(800), P.db(0), P.db(600)] if flip else [P.db(0), P.db(800), P.db(600), P.db(0)]
    return ["glMatrixMode 0x1701", "glPushMatrix", "glLoadIdentity", "glOrtho " + ",".join(o + [P.db(-1), P.db(1)]),
            "glMatrixMode 0x1700", "glPushMatrix", "glLoadIdentity", "glDisable 0xb50", "glDisable 0xde1", "glEnable 0xbe2",
            "glBlendFunc 0x302,0x303", "glLineWidth " + fbits(float(width)),
            "glColor4f " + ",".join(fbits(v) for v in (r, g, b, alpha_f(alpha))),
            "glBegin 0x1", "glVertex2i 0x%x,0x%x" % (x1 & 0xFFFFFFFF, y1 & 0xFFFFFFFF),
            "glVertex2i 0x%x,0x%x" % (x2 & 0xFFFFFFFF, y2 & 0xFFFFFFFF), "glEnd",
            "glEnable 0xb50", "glEnable 0xde1", "glDisable 0xbe2", "glPopMatrix", "glMatrixMode 0x1701", "glPopMatrix",
            "glMatrixMode 0x1700", "glFlush"]


def run(name: str, lines: list[tuple], pre: np.ndarray | None = None, extra_state: list[str] | None = None) -> tuple[np.ndarray, np.ndarray]:
    """lines: (x1, y1, x2, y2, color, width, alpha[, flip]) in order; pre: the DIB before (uint16 600 x 800, row 0 = memory row 0)"""
    sc = P.Scene()
    sc.state += INIT + (extra_state or [])
    for ln in lines:
        sc.pass_ += line_lines(*ln)
    d = sc.write("s38_" + name)
    if pre is None:
        pre = np.zeros((H, W), dtype="<u2")
    (d / "snaps" / "s1" / "dib_pre.bin").write_bytes(pre.astype("<u2").tobytes())
    real = P.real_frame(d).reshape(H, W)
    return pre, real


def show(a: np.ndarray, x0, y0, x1, y1, ch=0):
    """5-bit channel (0 r, 1 g, 2 b) of a window, rows printed as they are in memory"""
    sh = (10, 5, 0)[ch]
    for y in range(y0, y1):
        print("%4d " % y + " ".join("%2d" % ((int(a[y, x]) >> sh) & 31) for x in range(x0, x1)))


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "explore"
    if which == "explore":
        white = 0x7FFF
        L = [(20, 20, 120, 20, white, 1, 10), (20, 40, 120, 45, white, 1, 10), (20, 60, 120, 60, white, 2, 10),
             (20, 90, 120, 100, white, 2, 10), (20, 130, 120, 130, white, 10, 10), (200, 20, 200, 120, white, 1, 10),
             (220, 20, 225, 120, white, 2, 10), (250, 20, 250, 120, white, 10, 10)]
        pre, real = run("explore", L)
        print("changed px", int((real != pre).sum()))
        show(real, 18, 17, 40, 24)
        print()
        show(real, 18, 57, 40, 64)
        print()
        show(real, 18, 123, 30, 138)
        print()
        show(real, 196, 17, 205, 24)
        print()
        show(real, 244, 17, 257, 24)


# ------------------------------------------------------------------------------------------------ models (python, for fitting)
K = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def quant(c: np.ndarray, ys: np.ndarray, xs: np.ndarray) -> np.ndarray:
    o = np.floor(c * 31.0 / 255.0 + (K[ys % 4, xs % 4] + 0.5) / 16.0)
    return np.clip(o, 0, 31).astype(int)


def area_cov(x1, y1, x2, y2, w, x0, y0, nx, ny, ss=32):
    """exact-ish area coverage of the rectangle (segment widened by w perpendicular) over pixel squares [x, x+1) x [y, y+1)"""
    dx, dy = x2 - x1, y2 - y1
    L = np.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    s = (np.arange(ss) + 0.5) / ss
    gx = (np.arange(nx)[:, None] + s[None, :]).reshape(-1) + x0
    gy = (np.arange(ny)[:, None] + s[None, :]).reshape(-1) + y0
    X, Y = np.meshgrid(gx, gy)
    t = (X - x1) * ux + (Y - y1) * uy
    n = -(X - x1) * uy + (Y - y1) * ux
    inside = (t >= 0) & (t <= L) & (np.abs(n) <= w / 2)
    return inside.reshape(ny, ss, nx, ss).mean(axis=(1, 3))


def phases(name, x1, y1, x2, y2, w, color=0x7FFF, alpha=10, base=(10, 10), step=(190, 140)):
    """the same line drawn at 16 dither phases (copies shifted by whole pixels i, j in 0..3); returns (intervals lo, hi) of
    the pre-quantisation channel value c (0..255) per pixel of the copy at phase (0, 0), red channel, black background"""
    L, offs = [], []
    for k in range(16):
        i, j = k % 4, k // 4
        ox = base[0] + (k % 4) * step[0] + i
        oy = base[1] + (k // 4) * step[1] + j
        offs.append((ox, oy, i, j))
        L.append((x1 + ox, y1 + oy, x2 + ox, y2 + oy, color, w, alpha))
    pre, real = run(name, L)
    x0, y0 = min(x1, x2) - 8, min(y1, y2) - 8
    nx, ny = abs(x2 - x1) + 16, abs(y2 - y1) + 16
    lo = np.zeros((ny, nx))
    hi = np.full((ny, nx), 255.0)
    for ox, oy, i, j in offs:
        blk = (real[y0 + oy:y0 + oy + ny, x0 + ox:x0 + ox + nx] >> 10) & 31
        ys, xs = np.mgrid[y0 + oy:y0 + oy + ny, x0 + ox:x0 + ox + nx]
        d = (K[ys % 4, xs % 4] + 0.5) / 16.0
        # floor(c * 31 / 255 + d) = q  <=>  (q - d) * 255 / 31 <= c < (q + 1 - d) * 255 / 31
        lo = np.maximum(lo, np.where(blk > 0, (blk - d) * 255 / 31, 0))
        hi = np.minimum(hi, np.where(blk < 31, (blk + 1 - d) * 255 / 31, 255))
    return lo, hi, (x0, y0)


def ramp_cov(x1, y1, x2, y2, w, x0, y0, nx, ny, cap=0.5, side=0.5):
    """distance-ramp model: at a pixel centre, n = signed distance from the segment's line, t = position along it from (x1, y1);
    coverage = clamp(w / 2 + side - |n|) * clamp(t + cap) * clamp(L - t + cap), each clamped to [0, 1]"""
    dx, dy = x2 - x1, y2 - y1
    L = np.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    Y, X = np.mgrid[y0:y0 + ny, x0:x0 + nx] + 0.5
    t = (X - x1) * ux + (Y - y1) * uy
    n = -(X - x1) * uy + (Y - y1) * ux
    c = np.clip(w / 2 + side - np.abs(n), 0, 1) * np.clip(t + cap, 0, 1) * np.clip(L - t + cap, 0, 1)
    return c


def model_frame(pre: np.ndarray, lines, cap=0.5, side=0.5) -> np.ndarray:
    """the python model of a sequence of drawLine calls over pre (uint16 RGB555): ramp coverage times alpha, blended
    (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) against the 5-bit destination expanded by 255 / 31, written with the 4x4 dither"""
    fb = pre.astype(np.int64).copy()
    for ln in lines:
        x1, y1, x2, y2, color, w, alpha = ln[:7]
        flip = ln[7] if len(ln) > 7 else True
        if not flip:
            y1, y2 = 600 - y1, 600 - y2
        r, g, b = chan(color)
        a = alpha_f(alpha)
        half = w / 2 + 1
        x0 = int(np.floor(min(x1, x2) - half - 1)); y0 = int(np.floor(min(y1, y2) - half - 1))
        nx = int(abs(x2 - x1) + 2 * half + 3); ny = int(abs(y2 - y1) + 2 * half + 3)
        if x1 == x2 and y1 == y2:
            continue
        cov = ramp_cov(x1, y1, x2, y2, w, x0, y0, nx, ny, cap, side)
        ys, xs = np.mgrid[y0:y0 + ny, x0:x0 + nx]
        ok = (cov > 0) & (xs >= 0) & (xs < 800) & (ys >= 0) & (ys < 600)
        yy, xx, cc = ys[ok], xs[ok], cov[ok]
        d = fb[yy, xx]
        al = a * cc
        out = 0
        for sh, s in ((10, r), (5, g), (0, b)):
            dc = (d >> sh) & 31
            v = s * 255.0 * al + dc * 255.0 / 31.0 * (1 - al)
            out = out | (quant(v, yy, xx) << sh)
        fb[yy, xx] = out
    return fb.astype("<u2")


def clip_segment(x1, y1, x2, y2, W=800.0, H=600.0):
    """Liang-Barsky clip of the segment to [0, W] x [0, H]; None when nothing is left"""
    t0, t1 = 0.0, 1.0
    dx, dy = x2 - x1, y2 - y1
    for p, q in ((-dx, x1), (dx, W - x1), (-dy, y1), (dy, H - y1)):
        if p == 0:
            if q < 0:
                return None
            continue
        r = q / p
        if p < 0:
            t0 = max(t0, r)
        else:
            t1 = min(t1, r)
    if t0 > t1:
        return None
    return x1 + t0 * dx, y1 + t0 * dy, x1 + t1 * dx, y1 + t1 * dy


def model_frame_clip(pre, lines, cap=0.5, side=0.5):
    out = []
    for ln in lines:
        x1, y1, x2, y2 = ln[:4]
        flip = ln[7] if len(ln) > 7 else True
        if not flip:
            y1, y2 = 600 - y1, 600 - y2
        c = clip_segment(float(x1), float(y1), float(x2), float(y2))
        if c is None:
            continue
        out.append((c[0], c[1], c[2], c[3]) + tuple(ln[4:7]))
    return model_frame(pre, out, cap, side)


def snap(v: float, c: float, h: float, bits: int = 8) -> float:
    """U-1076's window-coordinate setup: c + (v - c) * (1 - 2^-8 / h), rounded to `bits` fractional bits"""
    s = 1 << bits
    return np.floor(s * (c + (v - c) * (1 - 2.0 ** -8 / h)) + 0.5) / s


def model_frame_snap(pre, lines, cap=0.5, side=0.5, bits=8):
    out = []
    for ln in lines:
        x1, y1, x2, y2 = ln[:4]
        flip = ln[7] if len(ln) > 7 else True
        if not flip:
            y1, y2 = 600 - y1, 600 - y2
        c = clip_segment(float(x1), float(y1), float(x2), float(y2))
        if c is None:
            continue
        a, b, cc, d = c
        out.append((snap(a, 400, 400, bits), snap(b, 300, 300, bits), snap(cc, 400, 400, bits), snap(d, 300, 300, bits)) + tuple(ln[4:7]))
    return model_frame(pre, out, cap, side)
