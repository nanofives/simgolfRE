"""S7.1 control search: a cursor read whose one-pixel error (live.recorder mode 4 + 16 n: x + 1 at the n-th cursor read)
reaches simulation state in the equivalence gate (live run vs sg_run_boot). Reads of the trace host are taken in the port's
read order; n is probed at in-game frames, the run cut a few records after the probe's frame.

    ~/bin/py -3.12 log/roadmap/S7/1_cursor_ctl_search.py <scenario> [max probes]     (run from the S7.1 mirror root)
"""
import pathlib
import sys
import time

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as T  # noqa: E402
from port.test import live  # noqa: E402
from port.test import lib as L  # noqa: E402

scen = sys.argv[1]
probes = int(sys.argv[2]) if len(sys.argv) > 2 else 40
tr = T.read(ROOT / "port" / "corpus" / "traces" / f"{scen}.trace")
frames = []  # frame (live index) of each cursor record the port reads
fr = 0
after_mm = False
for r in tr.records:
    wp = after_mm and r.type == T.CURSOR
    after_mm = r.type == T.MSG and r.f.get("msg") == 0x200
    if wp:
        continue
    if r.type == T.FRAME:
        fr = r.f["frame"] + 1
    elif r.type == T.CURSOR and r.sub == 1:
        frames.append(fr)
ingame = [i for i, f in enumerate(frames) if f >= 40]
step = max(1, len(ingame) // probes)
lib = L.load()
for i in ingame[::step][:probes]:
    t = time.time()
    r = live.equivalence(scen, lib, max_records=frames[i] + 3, negctl=4 + 16 * i, replay=False)
    print(f"read {i} frame {frames[i]}: {r.status} {r.live_vs_boot[:200]} ({time.time() - t:.1f} s)", flush=True)
    if r.status != "PASS":
        break
