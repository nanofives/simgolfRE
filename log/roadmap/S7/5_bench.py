"""S7.5: wall time of the app's --live-trace path with the terrain live against headless (the rasteriser's share of a run).

    ~/bin/py -3.12 log/roadmap/S7/5_bench.py [scenario ...]     (from the repo root; the mirror's build)
"""
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S7_5_head"
EXE = MIRROR / "port" / "build" / "S7_5" / "app" / "Release" / "simgolf_port.exe"

tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_5_bench_"))
try:
    for sc in sys.argv[1:] or ["course_season", "sandbox_basic"]:
        res = {}
        for t in ("live", "headless"):
            t0 = time.time()
            subprocess.run([str(EXE), "--install", str(ROOT / "original"), "--data", str(tmp / f"d_{sc}_{t}"), "--record-dir",
                            str(tmp / f"r_{sc}_{t}"), "--live-trace", str(MIRROR / "port" / "corpus" / "traces" / f"{sc}.trace"),
                            "--terrain", t], capture_output=True, cwd=str(MIRROR))
            s = json.loads((tmp / f"r_{sc}_{t}" / "session.json").read_text())
            res[t] = (time.time() - t0, s)
            print(f"{sc} {t}: {res[t][0]:.1f} s records {s['records']} passes {s['terrain_passes']} px {s['terrain_pixels']} calls {s.get('terrain_calls')}",
                  flush=True)
        (lt, ls), (ht, _hs) = res["live"], res["headless"]
        if ls["terrain_passes"] > 0:
            print(f"{sc}: terrain share {lt - ht:.1f} s over {ls['terrain_passes']} passes = {1000 * (lt - ht) / ls['terrain_passes']:.1f} "
                  f"ms per pass, {1e9 * (lt - ht) / max(1, ls['terrain_pixels']):.1f} ns per pixel", flush=True)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
