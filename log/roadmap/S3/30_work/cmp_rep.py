"""Compare two golfer_events traces with the HOST's mouse moves left out: a MSG 0x200 whose lParam is the physical cursor's client
point (lp HOST_LP) and the window procedure's CURSOR read right after it (U-1970). Prints counts and whether the rest is equal."""
import sys
sys.path.insert(0, "port/build/S3_30/re/tools")
import trace_format as TF
HOST_LP = 0x023802ED
def strip(tr):
    out, n, skip = [], 0, False
    for r in tr.records:
        if skip and r.type == TF.CURSOR:
            skip = False; continue
        skip = False
        if r.type == TF.MSG and r.f["msg"] == 0x200 and r.f["lp"] == HOST_LP:
            skip = True; n += 1; continue
        out.append(r)
    return out, n
def key(r):
    f = dict(r.f)
    if r.type == TF.CLOCKRUN: return None   # runs are split around the removed records
    return (r.type, r.sub, tuple(sorted((k, str(v)) for k, v in f.items())))
res = []
for p in sys.argv[1:]:
    tr = TF.read(p)
    out, n = strip(tr)
    calls = []
    for c in TF.clock_calls(tr): calls.append(c)
    res.append(([key(r) for r in out if r.type != TF.CLOCKRUN], n, calls, len(tr.records)))
    print(p, "records", len(tr.records), "host mouse moves", n, "clock calls", len(calls))
a, b = res[0], res[1]
print("non-clock records equal:", a[0] == b[0], len(a[0]), len(b[0]))
print("clock calls equal:", a[2] == b[2])
