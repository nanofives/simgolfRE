"""Lead's merge of the S6.1 / S6.2 / S6.6 decision fragments into re/port/defect_decisions.tsv.
S6.6 rows (disassembly + Unicorn) replace S6.1 / S6.2 rows with the same id; a base id that S6.6 split
(D40 -> D40a/b, D42 -> D42a/b/c, D52b -> D52b1/2/3) is dropped. A column `profile_fixed` says whether the
`fixed` profile sets the switch (recommendation fix and switch implemented later by S6.4 / S6.5)."""
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
S6 = ROOT / "log" / "roadmap" / "S6"


def read(p):
    with open(p, newline="", encoding="utf-8") as fh:
        rows = list(csv.reader(fh, delimiter="\t"))
    h = rows[0]
    return h, [dict(zip(h, r)) for r in rows[1:]]


def key(i):
    m = re.match(r"D(\d+)(.*)", i)
    if m:
        return (0, int(m.group(1)), m.group(2))
    return (1, 0, i)


def main():
    h1, a = read(S6 / "1_decisions.tsv")
    h2, b = read(S6 / "2_decisions.tsv")
    h6, c = read(S6 / "6_decisions.tsv")
    ids6 = {r["id"] for r in c}
    split = {"D40", "D42", "D52b"}
    out = {}
    for r in a + b:
        if r["id"] in ids6 or r["id"] in split:
            continue
        out[r["id"]] = dict(r, verified="first pass (model notes, port source)")
    for r in c:
        out[r["id"]] = dict(r, verified="disassembly + Unicorn (S6.6)")
    # S3 wave 2 (roadmap S3.4 / S3.4b, merged by integrator I6): the tropical-tree fault, decision D5 option a
    h3, d = read(S6 / "s3_4b_decisions.tsv")
    assert h3 == h6, h3
    for r in d:
        assert r["id"] not in out, r["id"]
        out[r["id"]] = dict(r, verified="stock-game measurement (S3.4 / S3.4b)")
    d5_s3 = {r["id"] for r in d}
    # roadmap S7.12 (G15, Mariano 2026-10-30): the course-sfx directory alias
    h7, g = read(ROOT / "log" / "roadmap" / "S7" / "12_decisions.tsv")
    assert h7 == h6, h7
    for r in g:
        assert r["id"] not in out, r["id"]
        out[r["id"]] = dict(r, verified="device-sink run (S7.12)")
    g15 = {r["id"] for r in g}
    cols = list(h6) + ["verified", "decision", "profile_fixed"]
    rows = sorted(out.values(), key=lambda r: key(r["id"]))
    # G3 (Mariano, re/roadmap/DECISIONS_PENDING.md): the two hangs become fix, the other 24 ask rows stay keep.
    g3_fix = {"D42c": "loadGolfPro.negativeWindowBound", "D52b3": "generateRandomCourse.placementBound"}
    for r in rows:
        r["decision"] = ""
        if r["id"] in g3_fix:
            r.update(recommendation="fix", switch=g3_fix[r["id"]], decision="G3 fix (wave 2)")
        elif r["recommendation"] == "ask":
            r.update(recommendation="keep", decision="G3 keep")
        elif r["id"] in g15:
            r["decision"] = "G15 fix (S7.12)"
        elif r["recommendation"] == "fix":
            r["decision"] = "D5 fix (S3 wave 2)" if r["id"] in d5_s3 else "D5 fix (wave 1)"
        if r["recommendation"] == "fix":
            r["switch_state"] = "implemented"
        r["profile_fixed"] = "yes" if r["recommendation"] == "fix" else "no"
    # --root <dir>: write <dir>/re/port/defect_decisions.tsv (an integrator's mirror; default the repo this script lives in)
    root = pathlib.Path(sys.argv[sys.argv.index("--root") + 1]) if "--root" in sys.argv else ROOT
    dest = root / "re" / "port" / "defect_decisions.tsv"
    if "--write" in sys.argv:
        with open(dest, "w", newline="", encoding="utf-8") as fh:
            fh.write("# re/port/defect_decisions.tsv: every defect of re/analysis/data/model_defects.md and every non-PASS row of "
                     "re/port/divergences.tsv with class, effects, recommendation and switch (schema log/roadmap/S6/SCHEMA.md). "
                     "Regenerate: log/roadmap/S6/merge_decisions.py --write\n")
            w = csv.writer(fh, delimiter="\t", lineterminator="\n")
            w.writerow(cols)
            for r in rows:
                w.writerow([r.get(c, "") for c in cols])
    n = {k: sum(1 for r in rows if r["recommendation"] == k) for k in ("fix", "ask", "keep")}
    print(len(rows), "rows", n)
    print("ask:", " ".join(r["id"] for r in rows if r["recommendation"] == "ask"))
    print("fix:", " ".join(r["id"] for r in rows if r["recommendation"] == "fix"))
    missing = [i for i in range(1, 53) if not any(re.match(rf"D{i}([a-z]\d?)?$", r["id"]) for r in rows)]
    print("model_defects numbers without a row:", missing)


main()
