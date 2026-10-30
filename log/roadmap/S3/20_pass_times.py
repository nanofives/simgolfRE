"""S3.20: the terrain pass times of the two renderers on the same framegate boots (SG_TERRAIN_PASS_LOG, gfx_terrain.cpp): one boot per
scenario with --gl raster (GlRaster and its workers) and one with --gl wgl (the host's OpenGL), then microseconds per pass by entry
kind (1 strip, 2 render, 3 local, 4 pathUpdate) and per updateTerrainRender call (a frame's passes summed).

    ~/bin/py -3.12 log/roadmap/S3/20_pass_times.py [scenario ...]      (SG_PORTSIM_DLL = the build to measure)
"""
import collections
import os
import pathlib
import statistics
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(os.environ.get("SG_TREE") or pathlib.Path(__file__).resolve().parents[3])


def run(scenario, gl):
    log = pathlib.Path(tempfile.mkdtemp(prefix="s3_20_pt_")) / "pass.log"
    env = dict(os.environ, SG_TERRAIN_PASS_LOG=str(log))
    subprocess.run([sys.executable, "-m", "port.test.framegate", scenario, "--no-probe", "--gl", gl,
                    "--out", str(log.parent / "fg.tsv")], cwd=str(ROOT), env=env, capture_output=True, check=True)
    rows = [l.split() for l in log.read_text().splitlines() if l.strip()]
    return [(int(r[0]), int(r[2]), int(r[3])) for r in rows]   # (frame k, entry kind, microseconds)


def summary(rows):
    by_kind = collections.defaultdict(list)
    by_frame = collections.defaultdict(int)
    for k, kind, us in rows:
        by_kind[kind].append(us / 1000)
        by_frame[k] += us
    fr = [v / 1000 for v in by_frame.values()]
    s = {f"kind{k}": f"n={len(v)} median={statistics.median(v):.1f} max={max(v):.1f} ms" for k, v in sorted(by_kind.items())}
    s["per_frame"] = f"n={len(fr)} median={statistics.median(fr):.1f} max={max(fr):.1f} ms"
    return s


def main():
    for sc in sys.argv[1:] or ["sandbox_basic", "cov_k3_camera2"]:
        for gl in ("raster", "wgl"):
            print(sc, gl, summary(run(sc, gl)), flush=True)


if __name__ == "__main__":
    main()
