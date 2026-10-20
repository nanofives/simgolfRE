"""3.7b: which functions the original's COMMAND segments ran, from the trace alone: every recorded call site (CLOCKRUN /
KEYSTATE / TAKE / MAPVK / SLEEP / KBSTATE / TIMERSET `ra`) mapped to its golf_clean.exe function, plus the H1 key codes
taken (TAKE ra 0x0041c0d7 / 0x0041c0e2). Output log/p3/p37b/seg_reach.tsv (function, segments, records, segment list)
and log/p3/p37b/seg_keys.tsv (segment, frame, which, value)."""
import bisect, json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF

fns = []
for line in (ROOT / "re/functions_ghidra.tsv").read_text().splitlines()[1:]:
    f = line.split("\t")
    for r in f[5].replace(";", ",").split(","):
        lo, hi = (int(x, 16) for x in r.split("-"))
        fns.append((lo, hi, f[1]))
fns.sort()
los = [f[0] for f in fns]


def fn_of(ra):
    if ra >= 0x80000000:
        return "dll%d" % ((ra >> 24) & 0x7f)
    i = bisect.bisect_right(los, ra - 1) - 1   # ra is the return address: the call ends at ra
    if i >= 0 and fns[i][0] <= ra - 1 < fns[i][1]:
        return fns[i][2]
    return "?%08x" % ra


segs = [x for x in json.loads((ROOT / "port/corpus/segments.json").read_text())["segments"]
        if x["status"] == "accepted" and x["class"] == "COMMAND"]
reach, keys = {}, []
for s in segs:
    name = "%s/%s" % (s["scenario"], s["segment"])
    tr = TF.read(ROOT / s["dir"] / "trace.trace")
    frame = -1
    for r in tr.records:
        d = TF.decode(r)
        if r.type == TF.FRAME:
            frame = d["frame"]
        ra = d.get("ra") if isinstance(d, dict) else None
        if ra is None:
            continue
        f = fn_of(ra)
        e = reach.setdefault(f, {})
        e[name] = e.get(name, 0) + 1
        if r.type == TF.TAKE:
            keys.append((name, frame, d["which"], d["value"], "%08x" % ra))
with open(ROOT / "log/p3/p37b/seg_reach.tsv", "w") as o:
    o.write("function\tsegments\trecords\tsegment_list\n")
    for f, e in sorted(reach.items(), key=lambda kv: -len(kv[1])):
        o.write("%s\t%d\t%d\t%s\n" % (f, len(e), sum(e.values()), ",".join(sorted(e))))
with open(ROOT / "log/p3/p37b/seg_keys.tsv", "w") as o:
    o.write("segment\tframe\twhich\tvalue\tra\n")
    for k in keys:
        o.write("%s\t%d\t%d\t%#x\t%s\n" % k)
print(len(segs), "segments,", len(reach), "functions,", len(keys), "takes")
