"""S3.29: measured u error at the dither flips (29_flips.py --save) against a 29_texfit.py model's u - plane u at the same pixels.

    cd <tree> && ~/bin/py -3.12 ../../../log/roadmap/S3/29_ducmp.py <scene> <flips.npy> <model ...>
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import importlib  # noqa: E402
TF = importlib.import_module("29_texfit")


def main():
    scene, fl = sys.argv[1], np.load(sys.argv[2])
    tris, tex, real = TF.load(scene)
    covs = [TF.coverage(t) for t in tris]
    owner = {}
    for ti, rows in enumerate(covs):
        for (y, xs) in rows:
            for x in xs:
                owner[(y, int(x))] = ti
    meas = -(fl[:, 3] + fl[:, 4]) / 2 / 31   # alt_big: value = 255 (1 - alpha), so u error = -e / 31
    for nm in sys.argv[3:]:
        f = TF.MODELS[nm]
        pred = []
        cache = {}
        for (y, xa, xb) in fl[:, :3].astype(int):
            vals = []
            for x in (xa, xb):
                ti = owner.get((y, x))
                if ti is None:
                    vals.append(np.nan); continue
                key = (ti, y)
                if key not in cache:
                    rows = dict((yy, xs) for yy, xs in covs[ti])
                    xs = rows[y]
                    u, v, cols = f(tris[ti], y, xs, {})
                    up = TF.plane(tris[ti], TF.S_, xs + 0.5, y + 0.5)
                    cache[key] = dict(zip(xs.tolist(), (np.asarray(u, float) - up).tolist()))
                vals.append(cache[key][x])
            pred.append(np.nanmean(vals))
        pred = np.array(pred)
        d = meas - pred
        print(nm, "flips", len(fl), "measured - predicted: mean %.2e rms %.2e  |meas| rms %.2e" % (np.nanmean(d), np.sqrt(np.nanmean(d * d)), np.sqrt(np.mean(meas ** 2))))
        y = fl[:, 0]; xm = (fl[:, 1] + fl[:, 2]) / 2
        for yb in range(0, 600, 100):
            row = []
            for xb_ in range(0, 800, 100):
                s = (xm >= xb_) & (xm < xb_ + 100) & (y >= yb) & (y < yb + 100)
                row.append("%9.1e/%8.1e" % (np.nanmean(meas[s]), np.nanmean(pred[s])) if s.sum() > 3 else "        .         ")
            print("  y%3d" % yb, " ".join(row))


if __name__ == "__main__":
    main()
