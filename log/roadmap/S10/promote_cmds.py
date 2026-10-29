"""usage: promote_cmds.py <inventory pdb> <source substring> [<module-flag>]  -> promote command lines for C3 rows
the inventory marks c4_candidate whose smsg_status is 100 (a canonical CSV exists in log/diff)."""
import csv
import glob
import sys

pdb, sub = sys.argv[1], sys.argv[2]
mod = sys.argv[3] if len(sys.argv) > 3 else ""
n = 0
for r in csv.DictReader(open(f"log/roadmap/S10/inventory_{pdb}.tsv"), delimiter="\t"):
    if r["c4_candidate"] != "yes" or sub not in r["smsg_source"]:
        continue
    if r["smsg_status"] != "100":
        print(f"# {r['va']} {r['hooks_name']} {r['smsg_source']}: status {r['smsg_status'] or 'none'} {r['smsg_score']}")
        continue
    key = r["va"][2:].zfill(8) if r["va"].startswith("0x") else r["va"]
    pre = "" if not mod else mod + "_"
    hits = glob.glob(f"log/diff/{pre}{key}_*.match.csv") or glob.glob(f"log/diff/{key}_*.match.csv")
    for h in sorted(hits)[:1]:
        extra = f" --module {mod}" if mod else ""
        print(f"~/bin/py -3.12 scripts/re_classify.py promote {r['va']} --to C4{extra} --evidence {h.replace(chr(92), '/')}   # {r['hooks_name']}")
        n += 1
print(f"# {n} commands", file=sys.stderr)
