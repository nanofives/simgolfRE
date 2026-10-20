"""3.7b: for every handler batch of the accepted segments, the call sites of the last clock read before it and of the
first one after it: tells whether a dispatch happened inside a waitTicks loop (pumpInput483c30) or after it"""
import collections, json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[3]; sys.path.insert(0, str(ROOT))
from port.test import trace_commands as TC
import trace_format as TF
segs = [x for x in json.loads((ROOT / "port/corpus/segments.json").read_text())["segments"] if x["status"] == "accepted"]
pairs = collections.Counter()
for sg in segs:
    tr = TF.read(ROOT / sg["dir"] / "trace.trace")
    last, pending = None, False
    for r in tr.records:
        if r.type == TF.CLOCKRUN and not TC._inside_handler(r.f["ra"]):
            if pending:
                pairs[(hex(last), hex(r.f["ra"]))] += 1
                pending = False
            last = r.f["ra"]
        elif r.type == TF.FRAME:
            if pending:
                pairs[(hex(last), "FRAME")] += 1
                pending = False
            last = TF.FRAME_RA
        elif r.type == TF.HANDLER:
            pending = True
for (a, b), n in pairs.most_common(30):
    print(n, a, "->", b)
