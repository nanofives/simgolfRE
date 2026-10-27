"""S7.9: live.stuck_replay over the stuck scenarios, plain and with live.recorder control 10 for every inflated dispatch.
Run from the mirror root:  ~/bin/py -3.12 <this> [--no-control] scen...   (SG_BACKEND picks x64 / win32)"""
import hashlib
import os
import sys
import time

sys.path.insert(0, os.getcwd())
from port.test import lib as L, live as LV  # noqa: E402

a = sys.argv[1:]
control = "--no-control" not in a
lib = L.load()
for s in [x for x in a if not x.startswith("--")] or list(LV.STUCK_SCENARIOS):
    t0 = time.time()
    r = LV.stuck_replay(s, lib)
    print(f"{s}\tplain\t{r.status}\trecords {r.records}\tinflated {r.inflated}\treplay_stuck {r.replay_stuck}\t"
          f"trace {hashlib.sha256(r.trace).hexdigest()[:16]}\t{r.detail[:200]}\t{time.time() - t0:.0f} s", flush=True)
    if control:
        for n in range(max(r.inflated, 0)):
            r2 = LV.stuck_replay(s, lib, negctl=10 + 16 * n)
            print(f"{s}\tcontrol 10 n={n}\t{r2.status}\tinflated {r2.inflated}\treplay_stuck {r2.replay_stuck}\t"
                  f"replay_vs_live {r2.replay_vs_live[:120]!r}", flush=True)
