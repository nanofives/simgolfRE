"""S3.20: the census with the new default (framegate --gl auto on port/build/S3_20, 20_out/S3_20/census.tsv) against S3.15's host-GL
census (15_census_wgl.tsv, S3.15's tree with --host-gl) and against I14's census (log/roadmap/merges/I14/census.tsv): counts, rows whose
status or region px differ from S3.15's, PASS rows of I14 that are not PASS now, and the renderer column of every row.

    ~/bin/py -3.12 log/roadmap/S3/20_census_cmp.py [<census.tsv>]     # -> 20_census_cmp.tsv (rows that differ from S3.15's), stdout
"""
import collections
import csv
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
M = HERE.parent / "merges"


def load(p):
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


def main():
    new = load(pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "20_out" / "S3_20" / "census.tsv")
    wgl, i14 = load(HERE / "15_census_wgl.tsv"), load(M / "I14" / "census.tsv")
    tot = lambda d: dict(collections.Counter(r["status"] for r in d.values()))  # noqa: E731
    print("rows new", len(new), tot(new), "| S3.15 host-GL", len(wgl), tot(wgl), "| I14", len(i14), tot(i14))
    print("renderer column:", dict(collections.Counter(r.get("renderer", "") for r in new.values())))
    print("renderer reasons:", dict(collections.Counter((r.get("renderer_reason") or "").split(" [")[0][:60] for r in new.values())))
    diff = []
    for k in sorted(set(new) | set(wgl)):
        a, b = wgl.get(k), new.get(k)
        sa, sb = (a or {}).get("status", "absent"), (b or {}).get("status", "absent")
        ra, rb = (a or {}).get("regions", ""), (b or {}).get("regions", "")
        if sa != sb or ra != rb:
            diff.append((k, sa, sb, ra, rb, (b or {}).get("detail", "")))
    lost = [k for k, r in i14.items() if r["status"] == "PASS" and new.get(k, {}).get("status") != "PASS"]
    trans = collections.Counter((d[1], d[2]) for d in diff)
    print("rows differing from S3.15 host-GL:", len(diff), dict(trans))
    print("I14 PASS rows not PASS now:", len(lost), lost[:10])
    with (HERE / "20_census_cmp.tsv").open("w", encoding="utf-8", newline="\n") as f:
        f.write("scenario\tcheckpoint\ts3_15_wgl\tnew\ts3_15_regions\tnew_regions\tnew_detail\n")
        for k, sa, sb, ra, rb, det in diff:
            f.write(f"{k[0]}\t{k[1]}\t{sa}\t{sb}\t{ra}\t{rb}\t{det}\n")


if __name__ == "__main__":
    main()
