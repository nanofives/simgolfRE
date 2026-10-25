"""S3.3 repro: boot a scenario with gfx attached and one gfx.snapshotAt pending; print sg_run_boot's code and run_detail.
Usage: 3_repro.py <scenario> [snap_pos|none]   (SG_PORTSIM_DLL selects the build)"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import boot as B  # noqa: E402
from port.test import lib as L   # noqa: E402

scenario = sys.argv[1]
pos = sys.argv[2] if len(sys.argv) > 2 else "100"
captured = {}


def before(st):
    st.call("gfx.attach", [])
    if pos != "none":
        st.call("gfx.snapshotAt", [int(pos)])


def after(st, res):
    captured["detail"] = res.run_detail


r = B.run(scenario, L.load(), before=before, after=after, to_end=True)
print(r.status, r.records, r.expected, r.detail)
print("run_detail:", captured.get("detail"))
