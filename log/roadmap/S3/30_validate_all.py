"""S3.30: every trace of the local corpus (port/corpus/traces/*.trace) validates with the tree's trace_format.py (one process per
file: course_season_6y alone holds about 1.5 M records). Writes log/roadmap/S3/30_work/validate_all.tsv.

    ~/bin/py -3.12 log/roadmap/S3/30_validate_all.py [--segments]
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
TREE = ROOT / "port" / "build" / "S3_30"
OUT = ROOT / "log" / "roadmap" / "S3" / "30_work" / "validate_all.tsv"
CHILD = r"""
import sys, collections
sys.path.insert(0, sys.argv[1])
import trace_format as TF
try:
    tr = TF.read(sys.argv[2])
    errs = TF.validate(tr)
    k = collections.Counter(r.f["kind"] for r in tr.records if r.type == TF.DELIVER)
    print("OK" if not errs else "INVALID", len(tr.records), dict(k).get("call", 0), "; ".join(errs[:3]), sep="\t")
except TF.TraceError as e:
    print("PARSE", 0, 0, str(e), sep="\t")
"""


def main():
    files = sorted((ROOT / "port" / "corpus" / "traces").glob("*.trace"))
    if "--segments" in sys.argv:   # the segment slices (port/corpus/segments/<scenario>/<k>/trace.trace)
        files = sorted((ROOT / "port" / "corpus" / "segments").glob("*/*/trace.trace"))
        global OUT
        OUT = OUT.with_name("validate_segments.tsv")
    rows, bad = [], 0
    for i, p in enumerate(files, 1):
        r = subprocess.run([sys.executable, "-c", CHILD, str(TREE / "re" / "tools"), str(p)], capture_output=True, text=True)
        line = (r.stdout.strip().splitlines() or ["ERROR\t0\t0\t" + r.stderr.strip()[-200:]])[-1]
        st = line.split("\t")[0]
        bad += st != "OK"
        rows.append(f"{p.relative_to(ROOT).as_posix()}\t{line}")
        print(f"[{i}/{len(files)}] {p.name} {st}", flush=True)
    OUT.write_text("trace\tstatus\trecords\tcalls\terrors\n" + "\n".join(rows) + "\n", encoding="utf-8")
    print(f"{len(files) - bad} / {len(files)} valid")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
