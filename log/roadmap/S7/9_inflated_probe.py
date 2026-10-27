"""S7.9: which scenarios still take the live core's `stuck` (inflated) delivery on this base, and does live.recorder control 10
(the inflated dispatch's CURSOR record left out) read RED there. Run from the mirror root with SG_PORTSIM_DLL set:
    ~/bin/py -3.12 <this> [--control] scen...      (one line per scenario)"""
import os
import sys
import time

sys.path.insert(0, os.getcwd())
from port.test import lib as L, live as LV  # noqa: E402

a = sys.argv[1:]
control = "--control" in a
scens = [s for s in a if not s.startswith("--")]
lib = L.load()
for s in scens:
    t0 = time.time()
    r = LV.equivalence(s, lib)
    st = r.extra.get("stats", {})
    print(f"{s}\tstatus {r.status}\tinflated {st.get('inflated')}\tviolations {r.violations}\treplay_vs_live {r.replay_vs_live!r}"
          f"\t{time.time() - t0:.0f} s", flush=True)
    if control:
        for n in range(int(st.get("inflated") or 0)):
            r2 = LV.equivalence(s, lib, negctl=10 + 16 * n)
            print(f"{s}\tcontrol 10 n={n}\tstatus {r2.status}\treplay_vs_live {r2.replay_vs_live[:160]!r}", flush=True)
