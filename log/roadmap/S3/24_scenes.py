"""S3.24: the four probe scenes of test_terrain_overlay.py (rand, widths, short, flip) through the Python model under a rule,
against the real GL's frames cached in port/corpus/gl/probe/s38t_<name>/snaps/s1/dib_post.bin: (changed, differ, one, beyond)."""
import importlib.util, sys, pathlib
import numpy as np
ROOT = pathlib.Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("p24", ROOT / "log/roadmap/S3/24_probe.py"); P = importlib.util.module_from_spec(spec); spec.loader.exec_module(P)
sys.path.insert(0, str(ROOT / "port/build/S3_24"))
spec2 = importlib.util.spec_from_file_location("tto", ROOT / "port/build/S3_24/port/test/test_terrain_overlay.py"); T = importlib.util.module_from_spec(spec2); spec2.loader.exec_module(T)


def run(name, rule):
    lines, pre = T.scene(name)
    fb = pre.astype("<u2").copy()
    lw = 1.0
    for x1, y1, x2, y2, c, w, a, flip in lines:
        if w > 0:
            lw = float(w)
        r = dict(x1=x1, y1=y1, x2=x2, y2=y2, colour=c, width=w, alpha=a, flip=int(flip), lw=lw, sw=800, sh=600, fw=800)
        g = P.geometry(r)
        if g is None:
            continue
        ax, ay, bx, by, half = g
        if ax == bx and ay == by:
            continue
        reach = half + 1.5
        x0 = max(0, int(np.floor(min(ax, bx) - reach))); xe = min(799, int(np.ceil(max(ax, bx) + reach)))
        y0 = max(0, int(np.floor(min(ay, by) - reach))); ye = min(599, int(np.ceil(max(ay, by) + reach)))
        r["box"] = (x0, y0, xe, ye)
        r["pre"] = fb[y0:ye + 1, x0:xe + 1].copy()
        cov, _ = P.coverage(r, rule)
        fb[y0:ye + 1, x0:xe + 1] = P.blend(r, cov)
    real = np.frombuffer((ROOT / f"port/corpus/gl/probe/s38t_{name}/snaps/s1/dib_post.bin").read_bytes(), "<u2").reshape(600, 800)
    return T.compare(real, pre, fb)


for rule in sys.argv[1:]:
    print(rule, {n: run(n, rule) for n in ("rand", "widths", "short", "flip")}, flush=True)


def frame(name, rule):
    global run
    lines, pre = T.scene(name)
    fb = pre.astype("<u2").copy()
    lw = 1.0
    for x1, y1, x2, y2, c, w, a, flip in lines:
        if w > 0:
            lw = float(w)
        r = dict(x1=x1, y1=y1, x2=x2, y2=y2, colour=c, width=w, alpha=a, flip=int(flip), lw=lw, sw=800, sh=600, fw=800)
        g = P.geometry(r)
        if g is None:
            continue
        ax, ay, bx, by, half = g
        if ax == bx and ay == by:
            continue
        reach = half + 1.5
        x0 = max(0, int(np.floor(min(ax, bx) - reach))); xe = min(799, int(np.ceil(max(ax, bx) + reach)))
        y0 = max(0, int(np.floor(min(ay, by) - reach))); ye = min(599, int(np.ceil(max(ay, by) + reach)))
        r["box"] = (x0, y0, xe, ye)
        r["pre"] = fb[y0:ye + 1, x0:xe + 1].copy()
        cov, _ = P.coverage(r, rule)
        fb[y0:ye + 1, x0:xe + 1] = P.blend(r, cov)
    return fb
