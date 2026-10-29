"""S10.9: print compact review lines for a slice of 9_facts_<key>.json.
~/bin/py -3.12 log/roadmap/S10/9_tools/review.py <key> <start> <count> [--other-only]"""
import json, pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import shape  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[4]
key, start, cnt = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
d = json.loads((ROOT / f"log/roadmap/S10/9_facts_{key}.json").read_text(encoding="utf-8"))
sel = None
if "--verdict" in sys.argv:
    vt = sys.argv[sys.argv.index("--verdict") + 1]
    sel = {l.split("	")[0] for l in (ROOT / f"log/roadmap/S10/9_verdict_{key}.tsv").read_text().splitlines()[1:] if l.split("	")[1] == vt}
items = [(k, v) for k, v in d.items() if sel is None or k in sel][start:start + cnt]
for k, v in items:
    kd, det = shape.kind(v["body"])
    if "--other-only" in sys.argv and kd != "OTHER":
        continue
    body = " ; ".join(b[9:] for b in v["body"][:14 if kd == "OTHER" else 6])
    refs = [f"{r['detail'].replace('of the table at ', '@')[:28]} by {','.join(i[11:] for i in r['installers'])[:40]} {r['smsg_vtable']}"
            for r in v["refs"]][:3]
    extra = []
    if v["imports"]:
        extra.append("IMP " + ",".join(v["imports"]))
    if v["callers"]:
        extra.append("CALLERS " + ",".join(c[11:] for c in v["callers"])[:120])
    if v["callees"]:
        extra.append("CALLEES " + ",".join(c[11:] for c in v["callees"])[:160])
    if v["strtext"]:
        extra.append("STR " + "|".join(str(s)[:30] for s in v["strtext"]))
    print(f"{k} {v['theirs']} [{v['size']}{'' if v['size'] == v['theirs_size'] else '/' + str(v['theirs_size'])}"
          f"{' M-' if v['master'] == '-' else ''}] {kd}: {body}")
    for e in extra + refs:
        print("     ", e)
