"""Phase 3 progress dashboard (phase 2.6): every function of the vector manifest and every segment of the segment
manifest against the current portsim build.

    ~/bin/py -3.12 -m port.test.report [--no-segments] [--out log/p2] [--backend x64|win32]   (or $SG_BACKEND)

Writes <out>/p26_report_functions.tsv and <out>/p26_report_segments.tsv and prints a short summary. Corpus files are
read, never written.
"""
from __future__ import annotations

import argparse
import collections
import csv
import os
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test.lib32 import RunnerUnavailable  # noqa: E402
from port.test import segments as S  # noqa: E402
from port.test import vectors as V  # noqa: E402


def manifest_ok_counts() -> dict[str, int]:
    out = collections.Counter()
    with V.MANIFEST_TSV.open(newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            out[r["function"]] += int(r["ok_vectors"] or 0)
    return out


def function_rows(lib: L.Lib) -> list[dict]:
    vr = V.VectorRunner(lib)
    oks = manifest_ok_counts()
    rows = []
    names = V.manifest_functions()
    extra = [f for f, i in vr.funcs.items() if f not in names and i["module"] != "x87probe"]   # registered, not selected; the test-only probe is no game function
    for fn in names + extra:
        if fn in vr.funcs:
            r = vr.run_function(fn)
        else:
            r = V.FunctionResult(fn, False, vec_file=V.vec_path(fn).exists())
            r.coverage = vr.cov.get(fn, {}).get("verdict", "-")
            r.ok = oks.get(fn, 0)
        rows.append({"function": fn, "ported": "yes" if r.implemented else "no",
                     "module": vr.funcs.get(fn, {}).get("module", "-"), "verdict": r.verdict,
                     "vectors_ok": r.ok, "run": r.run, "passed": r.passed, "failed": r.failed,
                     "skipped": ";".join(f"{k}:{v}" for k, v in sorted(r.skipped.items())) or "-",
                     "distinct": r.distinct if r.implemented else "-", "coverage": r.coverage,
                     "warnings": "; ".join(r.warnings) or "-",
                     "first_failure": f"{r.failures[0].key} {r.failures[0].i}: {r.failures[0].why}" if r.failures else "-"})
    return rows


def segment_rows(lib: L.Lib) -> list[dict]:
    sr = S.SegmentRunner(lib)
    rows = []
    for m in S.manifest_rows():
        d = S.segment_dir(m)
        base = {"scenario": m["scenario"], "segment": int(m["segment"]), "class": m["class"], "frames": m["frames"],
                "manifest_status": m["status"]}
        if m["status"] != "accepted":
            rows.append({**base, "status": "NOT_ACCEPTED", "detail": m["reason"][:200]})
            continue
        if not d.exists():
            rows.append({**base, "status": "MISSING", "detail": str(d.relative_to(ROOT))})
            continue
        r = sr.run(S.load_segment(d))
        rows.append({**base, "status": r.status, "detail": r.detail[:300]})
    return rows


def write_tsv(path: pathlib.Path, rows: list[dict]):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "log" / "p2"))
    ap.add_argument("--no-segments", action="store_true")
    ap.add_argument("--backend", choices=["x64", "win32"], default=None, help="default $SG_BACKEND or x64 (phase 3.R)")
    a = ap.parse_args()
    if a.backend:
        os.environ["SG_BACKEND"] = a.backend
    t0 = time.time()
    try:
        lib = L.load()
    except RunnerUnavailable as e:
        print(f"SKIPPED: the win32 backend is unavailable: {e}")
        return 0
    out = pathlib.Path(a.out)
    fr = function_rows(lib)
    write_tsv(out / "p26_report_functions.tsv", fr)
    ported = [r for r in fr if r["ported"] == "yes"]
    v = collections.Counter(r["verdict"] for r in fr)
    print(f"backend {L.backend()}: {lib.build_info()}: {len(lib.functions())} port functions registered")
    print(f"functions (re/port/vectors_manifest.tsv + registered): {len(fr)}; ported {len(ported)}; verdicts "
          + ", ".join(f"{k} {n}" for k, n in sorted(v.items())))
    print(f"  ported vectors: {sum(r['passed'] for r in ported)} passed / {sum(r['run'] for r in ported)} run "
          f"(corpus ok vectors over all functions: {sum(int(r['vectors_ok']) for r in fr)})")
    for r in ported:
        print(f"  {r['function']:28s} {r['verdict']:10s} {r['passed']}/{r['run']} distinct {r['distinct']} "
              f"coverage {r['coverage']}" + (f"  WARNING {r['warnings']}" if r["warnings"] != "-" else ""))
    if not a.no_segments:
        sg = segment_rows(lib)
        write_tsv(out / "p26_report_segments.tsv", sg)
        s = collections.Counter(r["status"] for r in sg)
        print(f"segments (re/port/segments_manifest.tsv): {len(sg)}; " + ", ".join(f"{k} {n}" for k, n in sorted(s.items())))
        cls = collections.Counter((r["class"], r["status"]) for r in sg if r["manifest_status"] == "accepted")
        print("  by class: " + ", ".join(f"{c} {st} {n}" for (c, st), n in sorted(cls.items())))
    print(f"-> {out / 'p26_report_functions.tsv'}" + ("" if a.no_segments else f", {out / 'p26_report_segments.tsv'}")
          + f" ({time.time() - t0:.0f} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
