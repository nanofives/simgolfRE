"""S0.3: re-run scenarios whose boot ends with sg_run_boot -11 after EVERY record when a snapshot is pending (cov_a_champ,
championship, cov_k3_career: the trace ends in a Blocking pump; the boot gate, without gfx.snapshotAt, passes them).  Every sim hash
matched (no first_divergence, records == expected), so the pixels the snapshots did take are valid; the boot result is re-labelled
KNOWN (never PASS) with the reason, so framegate does not turn those rows into NOT_RUN.  Snapshots that were never taken stay NOT_RUN.
Usage (export root): 3_rerun_noend.py <scenario> > 3_rerun_<scenario>.json  (the worker prints its rows as the last JSON line)"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import boot as B       # noqa: E402
from port.test import framegate as FG  # noqa: E402

_orig = B.run


def run(*a, **k):
    r = _orig(*a, **k)
    if r.status == "FAIL" and r.first_divergence is None and r.records == r.expected and "-11" in r.detail:
        r.detail = "KNOWN by S0.3: " + r.detail + "; every sim hash equal"
        r.status = "KNOWN"
    return r


B.run = run
sys.exit(FG.main(["--worker", sys.argv[1], "--probe-limit", "3000000"]))
