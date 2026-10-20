"""Summary of log/p3/p37a_cmdsegs.tsv (or argv[1]): statuses, stop kinds, missing callees with the callers' path."""
import collections
import csv
import re
import sys

rows = list(csv.DictReader(open(sys.argv[1] if len(sys.argv) > 1 else "log/p3/p37a_cmdsegs.tsv", encoding="utf-8"), delimiter="\t"))
st = collections.Counter(r["status"] for r in rows)
print(dict(st))
kinds = collections.Counter()
miss = collections.defaultdict(list)
for r in rows:
    if r["status"] == "PASS":
        continue
    d = r["run_detail"]
    m = re.match(r"(missing: [^;]*|fault at va \d+|divide error|no return: [^;]*|exception)", d)
    path = re.search(r"stopped in ([^;]*)", d)
    if m:
        k = m.group(1)
        if k.startswith("fault"):
            k = "fault"
        kinds[k] += 1
        if k.startswith("missing"):
            miss[k[9:]].append((r["segment"], path.group(1) if path else ""))
        elif k == "fault" or k.startswith("no return") or k.startswith("divide"):
            print("  ", k, r["segment"], r["frames"], path.group(1) if path else "")
    else:
        kinds["divergence"] += 1
        print("   divergence", r["segment"], r["frames"], "record", r["record"], r["objects"][:120])
for k, v in kinds.most_common():
    print(v, k)
for k, v in sorted(miss.items(), key=lambda kv: -len(kv[1])):
    print(f"MISSING {k}: {len(v)} segments; paths: {sorted(set(p for _, p in v))[:4]}")
nc = [r["segment"] + ":" + r["noncausal"] for r in rows if r["noncausal"]]
print("noncausal:", len(nc), nc)
