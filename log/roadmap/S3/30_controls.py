"""S3.30: the numbers behind test_trace_call.py's controls: golfer_events' boot with tick.frame 0 / 20 / 21 / 22, the first divergent
record and objects of each, and the frame the injected call belongs to. Run from port/build/S3_30 with SG_PORTSIM_DLL set (x64) or
SG_BACKEND=win32. Writes log/roadmap/S3/30_work/controls_<backend>.json.
"""
import json
import os
import pathlib
import sys

T = pathlib.Path.cwd()
sys.path.insert(0, str(T))
sys.path.insert(0, str(T / "re" / "tools"))
import trace_format as TF  # noqa: E402
from port.test import boot as B  # noqa: E402
from port.test import lib as L  # noqa: E402

OUT = pathlib.Path(__file__).resolve().parent / "30_work"


def main():
    lib = L.load()
    tr = TF.read(B.TRACES / "golfer_events.trace")
    k, call = -1, None
    for r in tr.records:
        if r.type == TF.FRAME:
            k = r.f["frame"]
        elif r.type == TF.DELIVER and r.sub == 7:
            call = {"after_frame_record": k, "pos": r.f["pos"], "addr": hex(r.f["addr"]), "args": r.f["args"]}
    rows = {"call": call, "backend": L.backend()}
    for mode in (0, 20, 21, 22):
        lib.negctl("tick.frame", mode)
        try:
            r = B.run("golfer_events", lib=lib)
        finally:
            lib.negctl("tick.frame", 0)
        rows[str(mode)] = {"status": r.status, "records": r.records, "expected": r.expected,
                           "first_divergence": r.first_divergence, "detail": r.detail[:300], "run_detail": r.run_detail[:300]}
        print(mode, r.status, r.records, r.expected, json.dumps(r.first_divergence), flush=True)
    (OUT / f"controls_{L.backend()}.json").write_text(json.dumps(rows, indent=1))


if __name__ == "__main__":
    main()
