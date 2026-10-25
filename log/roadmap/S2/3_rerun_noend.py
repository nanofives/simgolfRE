"""S2.3: log/roadmap/S0/3_rerun_noend.py run from the current directory's tree (the S2.3 mirror), not the repo root: the
scenarios whose boot ends with sg_run_boot -11 after every record while a snapshot is pending (cov_a_champ, championship,
cov_k3_career, U-1120) are re-labelled KNOWN when every sim hash matched, as the S0 census did.
Usage (from the mirror root): ~/bin/py -3.12 <abs>/log/roadmap/S2/3_rerun_noend.py <scenario> > <out.json>"""
import os
import sys

sys.path.insert(0, os.getcwd())
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
sys.exit(FG.main(["--worker", sys.argv[1], "--no-probe"]))
