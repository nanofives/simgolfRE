"""I5: compare the merged mirror's ranked census (frame_census.tsv) with S0's re/roadmap/frame_census.tsv (HEAD 4bfefc2a, 609 rows).
Prints status counts of both, per-row status transitions over S0's rows, the rows new since S0 (scenarios pinned later) and the
section rows each merge request names: cov_r_quit's credits (S2.3), cov_k1_routing routing / cov_a_keys k_dlg_L / k_dlg_R (S2.4),
cov_a_quit / cov_a_quit_title (S2.5), and the 22 rows of cov_a_champ / championship / cov_k3_career (S3.3: never NOT_RUN).
Usage: ~/bin/py -3.12 log/roadmap/merges/S2S3-5/census_compare.py <new frame_census.tsv> [<old>]"""
import collections
import csv
import sys


def rd(p):
    with open(p, encoding="utf-8") as f:
        lines = [l for l in f if not l.startswith("#")]
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(lines, delimiter="\t")}


new = rd(sys.argv[1])
old = rd(sys.argv[2] if len(sys.argv) > 2 else "re/roadmap/frame_census.tsv")
print("rows: old", len(old), "new", len(new))
print("status old:", dict(collections.Counter(r["status"] for r in old.values())))
print("status new:", dict(collections.Counter(r["status"] for r in new.values())))
over = {k: new[k] for k in old if k in new}
print("status new over S0's rows:", dict(collections.Counter(r["status"] for r in over.values())), "missing", len(old) - len(over))
tr = collections.Counter((old[k]["status"], new[k]["status"]) for k in over)
print("transitions over S0's rows:", {f"{a}->{b}": n for (a, b), n in sorted(tr.items())})
worse = [k for k in over if old[k]["status"] == "PASS" and new[k]["status"] != "PASS"]
print("PASS rows lost:", len(worse), worse[:20])
gained = sorted(k for k in over if old[k]["status"] != "PASS" and new[k]["status"] == "PASS")
print("PASS rows gained:", len(gained))
for k in gained:
    print("  +", "/".join(k), old[k]["status"])
added = sorted(set(new) - set(old))
print("rows new since S0:", len(added), dict(collections.Counter(new[k]["status"] for k in added)))
print("  by scenario:", dict(collections.Counter(k[0] for k in added)))
print("primary causes new:", collections.Counter(r["primary_cause"] for r in new.values() if r["status"] != "PASS").most_common(12))


def show(sc, cps=None):
    ks = sorted(k for k in new if k[0] == sc and (cps is None or k[1] in cps))
    st = collections.Counter(new[k]["status"] for k in ks)
    print(f"section {sc}: {dict(st)}", "" if cps is None else {k[1]: (new[k]["status"], new[k]["total_px"]) for k in ks})


show("cov_r_quit", {"quit_00", "quit_01", "quit_02", "quit_03", "credits"})
show("cov_k1_routing", {"routing"})
show("cov_a_keys", {"k_dlg_L", "k_dlg_R"})
show("cov_a_quit")
show("cov_a_quit_title")
for sc in ("cov_a_champ", "championship", "cov_k3_career"):
    show(sc)
