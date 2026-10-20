"""Phase 3.7a: where the final POLL of each segment's trace slice sits (the call site of the clock call it rides on)."""
import sys, collections, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as T
from port.test import segments as S
cls = sys.argv[1] if len(sys.argv) > 1 else "TIME-FORWARD"
for r in S.manifest_rows():
    if r["status"] != "accepted" or (cls != "all" and r["class"] != cls):
        continue
    tr = T.read(S.segment_dir(r) / "trace.trace")
    calls = {o: (ra, kind) for o, ra, slot, kind, v in T.clock_calls(tr)}
    poll = [x for x in tr.records if x.type == T.POLL and (x.sub & 2)][0]
    frames = [x for x in tr.records if x.type == T.FRAME]
    ra, kind = calls.get(poll.f["pos"] - 1, (0, 0))
    after = [x for x in frames if x.f["pos"] == poll.f["pos"]]
    since = poll.f["pos"] - frames[-1].f["pos"]
    print(r["scenario"], r["segment"], "frames", len(frames), "poll pos", poll.f["pos"], "site %#x kind %d" % (ra, kind),
          "calls after last FRAME", since, "FRAME at poll" if after else "")
