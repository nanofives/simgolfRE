"""3.7b: the raw input of every accepted segment as Commands (port/test/trace_commands.py): per segment the number of
handler records, batches, and batches per dispatch context. Output log/p3/p37b/segment_commands.tsv"""
import csv, json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[3]; sys.path.insert(0, str(ROOT))
from port.test import trace_commands as TC
import trace_format as TF
segs = [x for x in json.loads((ROOT / "port/corpus/segments.json").read_text())["segments"] if x["status"] == "accepted"]
w = csv.writer(open(ROOT / "log/p3/p37b/segment_commands.tsv", "w", newline=""), delimiter="	")
w.writerow(["scenario", "segment", "class", "frames", "handler_records", "batches", "contexts", "handlers_by_id"])
tot = {"COMMAND": [0, 0, 0], "TIME-FORWARD": [0, 0, 0]}
ctxs = {}
for r in segs:
    tr = TF.read(ROOT / r["dir"] / "trace.trace")
    s = TC.summary(tr)
    for k, v in s["by_context"].items():
        ctxs[k] = ctxs.get(k, 0) + v
    t = tot[r["class"]]; t[0] += 1; t[1] += s["commands"]; t[2] += s["batches"]
    w.writerow([r["scenario"], r["segment"], r["class"], r["frames"], s["commands"], s["batches"],
                ";".join(f"{k}:{v}" for k, v in sorted(s["by_context"].items())),
                ";".join(f"{k}:{v}" for k, v in s["handlers"].items() if v)])
print(tot); print(sorted(ctxs.items(), key=lambda kv: -kv[1])[:30])
