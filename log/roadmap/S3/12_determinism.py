"""S3.12: is the app's live-trace frame at a checkpoint the same in every run? test_app_ingame's run 2 on the wave 6 tree read
cov_a_keys k_dlg_J 33 px away from framegate's frame (run 1 and the x64 suite's run: 0). Runs N times, per run and per checkpoint the
SHA-1 of the app path's snapshot (simgolf_port.exe --live-trace) and of framegate's own frame (in-process sg_run_boot with gfx +
LiveTerrain), with the rasteriser workers as configured (SG_RASTER_THREADS: unset = gfx_terrain's default, 1 = none).

    (cd port/build/<tree> && . ../../../log/roadmap/S3/12_env.sh && SG_RASTER_THREADS=<n> ~/bin/py -3.12 \\
        ../../../log/roadmap/S3/12_determinism.py <scenario> <runs> [--fg] [--checkpoints a,b] > <log>)
"""
import argparse
import collections
import hashlib
import os
import pathlib
import sys

TREE = pathlib.Path(os.environ.get("SG_TREE") or pathlib.Path.cwd())
sys.path.insert(0, str(TREE))
sys.path.insert(0, str(TREE / "port" / "test"))
os.chdir(TREE)
import test_app_ingame as T  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario")
    ap.add_argument("runs", type=int)
    ap.add_argument("--fg", action="store_true", help="also framegate's in-process frames each run")
    ap.add_argument("--checkpoints", default="")
    a = ap.parse_args()
    exe = pathlib.Path(os.environ["SG_APP_EXE"])
    plan = T.checkpoint_plan(a.scenario)
    want = set(a.checkpoints.split(",")) if a.checkpoints else None
    seen = collections.defaultdict(collections.Counter)
    print(f"tree {TREE} SG_RASTER_THREADS={os.environ.get('SG_RASTER_THREADS', '(default)')} scenario {a.scenario}", flush=True)
    for k in range(a.runs):
        app = T.app_live_trace(exe, a.scenario, plan)
        for row in plan["rows"]:
            c = row["checkpoint"]
            if want and c not in want:
                continue
            raw = app["snapshots"].get(c)
            h = hashlib.sha1(raw).hexdigest()[:12] if raw else "none"
            seen[("app", c)][h] += 1
        if a.fg:
            cks = [r["checkpoint"] for r in plan["rows"] if not want or r["checkpoint"] in want]
            _rows, imgs = T.framegate_frames(a.scenario, cks)
            for c in cks:
                im = imgs.get(c)
                seen[("fg", c)][hashlib.sha1(im.tobytes()).hexdigest()[:12] if im is not None else "none"] += 1
        print(f"run {k} done", flush=True)
    for (side, c), cnt in sorted(seen.items(), key=lambda x: (x[0][1], x[0][0])):
        print(f"{c}\t{side}\t{'STABLE' if len(cnt) == 1 else 'VARIES'}\t{dict(cnt)}", flush=True)


if __name__ == "__main__":
    main()
