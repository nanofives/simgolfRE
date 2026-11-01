"""S3.21 (U-1309): map the real GL's interpolated colour error with a slow ramp.

One screen quad (two triangles, 2D ortho, untextured, unlit); red rises from R0 at the left to R0 + DR at the right (DR = a few
dither steps 1 / (31 * 16)), green and blue constant. A pixel's 5-bit red is floor(r * 31 + (k + 0.5) / 16); along a row, each
dither phase flips where r * 31 + (k + 0.5) / 16 crosses an integer, so the flip column of the real frame against the exact one
gives the real red's error there, with a resolution of one column's red step (DR / 800). Prints per row the error at each flip.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21_ramp.py [variant]
variants: x (red along x), y (red along y), tri (one triangle only, apex at the top)
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P  # noqa: E402

K = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def main():
    var = sys.argv[1] if len(sys.argv) > 1 else "x"
    R0, DR = 0.5003, 3.0 / (31 * 16)
    sc = P.Scene()
    sc.setup2d(1.0)
    x0, y0, x1, y1 = 0.37, 0.61, 799.43, 599.29
    if var == "x":
        cols = [(R0, 0.4, 0.4, 1.0), (R0 + DR, 0.4, 0.4, 1.0), (R0 + DR, 0.4, 0.4, 1.0), (R0, 0.4, 0.4, 1.0)]
    else:
        cols = [(R0, 0.4, 0.4, 1.0), (R0, 0.4, 0.4, 1.0), (R0 + DR, 0.4, 0.4, 1.0), (R0 + DR, 0.4, 0.4, 1.0)]
    sc.quad(x0, y0, x1, y1, colors=cols)
    d = sc.write("s321_ramp_" + var)
    real = P.real_frame(d).reshape(600, 800)
    red = ((real >> 10) & 31).astype(int)
    # the exact red at each pixel centre (the quad's two triangles share the linear function, so the plane is global)
    xs = np.arange(800) + 0.5
    ys = np.arange(600) + 0.5
    if var == "x":
        rx = R0 + DR * (xs - x0) / (x1 - x0)
        R = np.tile(rx, (600, 1))
        step = DR / (x1 - x0)
    else:
        ry = R0 + DR * (ys - y0) / (y1 - y0)
        R = np.tile(ry[:, None], (1, 800))
        step = DR / (y1 - y0)
    k = K[(np.arange(600) & 3)[:, None], (np.arange(800) & 3)[None, :]]
    exact = np.floor(R * 31 + (k + 0.5) / 16).astype(int)
    drawn = real != 0
    diff = (exact != red) & drawn
    print(var, "drawn", int(drawn.sum()), "differ", int(diff.sum()), "column red step", step)
    # for each differing pixel the red error is at least the distance of the exact value to its boundary (signed)
    q = R * 31 + (k + 0.5) / 16
    m = (q - np.floor(q + 0.5)) / 31   # signed distance of r to the boundary, in red units
    ys_, xs_ = np.nonzero(diff)
    for y, x in list(zip(ys_, xs_))[:60]:
        print(f"  ({x:3d},{y:3d}) exact {exact[y, x]} real {red[y, x]} needed error {(-m[y, x]):+.3e}")
    # summary by x band and y band
    if len(xs_):
        print("x histogram (bins of 50)", np.histogram(xs_, bins=16, range=(0, 800))[0].tolist())
        print("y histogram (bins of 50)", np.histogram(ys_, bins=12, range=(0, 600))[0].tolist())
        e = -m[ys_, xs_]
        print("needed error quantiles", np.quantile(e, [0, 0.1, 0.5, 0.9, 1]).tolist())




def dense():
    """red = R0 + gx * (x - x0) + gy * (y - y0): along each row (fixed y, fixed dither phase x & 3) the 5-bit red steps up where
    r * 31 + (k + 0.5) / 16 crosses an integer; the real flip column against the exact one gives the real red's error at that x
    (resolution 4 columns * gx). Prints the error per flip and a fit e = a + b * x per row group."""
    R0 = 0.5003
    gx = 3.0 / (31 * 16) / 800.0        # three dither steps across the screen
    gy = 1.0 / (31 * 16) / 600.0 * 7.3  # rows shift the flip columns
    sc = P.Scene()
    sc.setup2d(1.0)
    x0, y0, x1, y1 = 0.37, 0.61, 799.43, 599.29
    r = lambda x, y: R0 + gx * (x - x0) + gy * (y - y0)
    cols = [(r(x0, y0), 0.4, 0.4, 1.0), (r(x1, y0), 0.4, 0.4, 1.0), (r(x1, y1), 0.4, 0.4, 1.0), (r(x0, y1), 0.4, 0.4, 1.0)]
    sc.quad(x0, y0, x1, y1, colors=cols)
    d = sc.write("s321_ramp_dense")
    real = P.real_frame(d).reshape(600, 800)
    red = ((real >> 10) & 31).astype(int)
    pts = []
    for y in range(2, 597):
        for ph in range(4):
            xs = np.arange(ph, 800, 4)
            xs = xs[(xs > 1) & (xs < 798)]
            k = K[y & 3][ph]
            ex = np.floor(r(xs + 0.5, y + 0.5) * 31 + (k + 0.5) / 16).astype(int)
            rr = red[y, xs]
            for lvl in np.unique(ex)[1:]:
                fe = xs[np.argmax(ex >= lvl)]
                if not (rr >= lvl).any():
                    continue
                fr = xs[np.argmax(rr >= lvl)]
                pts.append((y, fe, fr, (fe - fr) * gx))
    pts = np.array(pts, float)
    print("flips", len(pts), "error (real - exact) at the flip, by x band of 100 px (mean, min, max):")
    for b in range(8):
        sel = (pts[:, 1] >= b * 100) & (pts[:, 1] < b * 100 + 100)
        if sel.any():
            e = pts[sel, 3]
            print(f"   x {b*100:3d}..{b*100+99}: n {int(sel.sum()):4d} mean {e.mean():+.2e} min {e.min():+.2e} max {e.max():+.2e}")
    print("by y band of 100 px:")
    for b in range(6):
        sel = (pts[:, 0] >= b * 100) & (pts[:, 0] < b * 100 + 100)
        if sel.any():
            e = pts[sel, 3]
            print(f"   y {b*100:3d}..{b*100+99}: n {int(sel.sum()):4d} mean {e.mean():+.2e} min {e.min():+.2e} max {e.max():+.2e}")
    np.save(pathlib.Path(d) / "flips.npy", pts)


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "dense":
    dense()
    sys.exit(0)


if __name__ == "__main__" and (len(sys.argv) < 2 or sys.argv[1] != "dense"):
    main()
