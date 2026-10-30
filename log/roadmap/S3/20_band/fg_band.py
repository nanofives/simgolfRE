"""S3.20 band measurement on framegate rows: per scenario one boot with the single-context host GL (--gl wgl) and one per band count
with WglBands (--gl wglband, SG_GL_BANDS=N), each with SG_TERRAIN_PASS_LOG and --images. Prints, per run: the self-check verdict through
the sink, every checkpoint image equal to the single-context one (pixel count otherwise), the rows' status / regions equal, and the pass
times (render passes = entry kind 2; per updateTerrainRender call = the call's passes summed).

    ~/bin/py -3.12 log/roadmap/S3/20_band/fg_band.py <scenario> [...] [--bands 2,3,4,6]      (SG_TREE / SG_PORTSIM_DLL = the mirror)
"""
import argparse
import collections
import csv
import os
import pathlib
import statistics
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

ROOT = pathlib.Path(os.environ["SG_TREE"])


def run(scenario, gl, bands=None):
    d = pathlib.Path(tempfile.mkdtemp(prefix="s3_20_fgb_"))
    env = dict(os.environ, SG_TERRAIN_PASS_LOG=str(d / "pass.log"))
    if bands:
        env["SG_GL_BANDS"] = str(bands)
    r = subprocess.run([sys.executable, "-m", "port.test.framegate", scenario, "--no-probe", "--gl", gl, "--images", str(d / "img"),
                        "--out", str(d / "fg.tsv")], cwd=str(ROOT), env=env, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr[-2000:])
    rows = list(csv.DictReader(open(d / "fg.tsv", encoding="utf-8"), delimiter="\t"))
    imgs = {p.name.split(".")[1]: np.asarray(Image.open(p).convert("RGB")) for p in (d / "img").glob("*.port.png")}
    passes = [l.split() for l in (d / "pass.log").read_text().splitlines() if l.strip()] if (d / "pass.log").exists() else []
    return rows, imgs, [(int(x[0]), int(x[2]), int(x[3])) for x in passes]


def times(passes):
    rend = [us / 1000 for _k, kind, us in passes if kind == 2]
    per = collections.defaultdict(int)
    for k, _kind, us in passes:
        per[k] += us
    fr = sorted(v / 1000 for v in per.values())
    f = lambda v: f"n={len(v)} median={statistics.median(v):.1f} max={max(v):.1f}" if v else "n=0"  # noqa: E731
    kinds = collections.defaultdict(list)
    for k, kind, _us in passes:
        kinds[k].append(kind)
    first = min(per) if per else None
    multi = sorted(round(per[k] / 1000, 1) for k in per if kinds[k].count(2) >= 2)
    rest = sorted(per[k] / 1000 for k in per if k != first)
    loc = [us / 1000 for _k, kind, us in passes if kind == 3]
    return (f"local {f(loc)} sum={sum(loc):.0f} | render {f(rend)} | per-call {f(fr)} | first call {per[first] / 1000 if first is not None else 0:.1f} | "
            f"calls without the first {f(rest)} | calls with >= 2 render passes {multi}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenarios", nargs="+")
    ap.add_argument("--bands", default="2,3,4,6")
    a = ap.parse_args()
    for sc in a.scenarios:
        rows0, imgs0, p0 = run(sc, "wgl")
        print(f"{sc} single: {len(rows0)} rows {collections.Counter(r['status'] for r in rows0)} renderer "
              f"{set(r['renderer'] for r in rows0)} | {times(p0)}", flush=True)
        for n in [int(x) for x in a.bands.split(",")]:
            rows, imgs, p = run(sc, "wglband", n)
            diff_rows = [r["checkpoint"] for r, q in zip(rows, rows0) if (r["status"], r["regions"]) != (q["status"], q["regions"])]
            px = {c: int((imgs[c] != imgs0[c]).any(axis=2).sum()) for c in imgs0 if c in imgs}
            bad = {c: v for c, v in px.items() if v}
            missing = sorted(set(imgs0) ^ set(imgs))
            chk = set(r["renderer_reason"].split(": ")[1][:22] if ": " in r["renderer_reason"] else r["renderer_reason"] for r in rows)
            print(f"{sc} bands={n}: images {len(px)} compared, differing {bad or 0}, missing {missing or 0}, rows differing "
                  f"{diff_rows or 0}, check {chk} | {times(p)}", flush=True)


if __name__ == "__main__":
    main()
