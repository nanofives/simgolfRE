"""S3.14: per-frame breakdown of a kept app session's waits.tsv written by the TEMPORARY diagnostic build (columns 8.. are
cumulative: page faults, sim-thread CPU ms, working set MB, present ms, gfx.dump ms, frame_mu wait ms, mu wait ms, GDI TextOutA ms,
GDI surface copy ms, GDI calls, gfx body ms). Prints every frame whose work is >= the threshold, and per-session totals.

    ~/bin/py -3.12 log/roadmap/S3/14_diag_frames.py <session dir>... [--min 60]
"""
import pathlib
import sys

lo = 60
args = list(sys.argv[1:])
if "--min" in args:
    i = args.index("--min")
    lo = int(args[i + 1])
    del args[i:i + 2]
for a in args:
    rows = [[int(x) for x in l.split("\t")] for l in (pathlib.Path(a) / "rec" / "waits.tsv").read_text().splitlines() if l.strip()]
    print("==", a, len(rows), "frames")
    prev = None
    tot = {"gdi_text": 0, "gdi_copy": 0, "gdi_calls": 0, "gfx_body": 0}
    for r in rows:
        if prev is not None and len(r) >= 19:
            d = [r[i] - prev[i] for i in range(len(r))]
            for k, i in (("gdi_text", 15), ("gdi_copy", 16), ("gdi_calls", 17), ("gfx_body", 18)):
                tot[k] += d[i]
            if r[1] >= lo:
                print(f"  f{r[0]} work {r[1]} wait_end {r[3]} passes {d[6]} faults {d[8]} cpu {d[9]} present {d[11]} "
                      f"fmu {d[13]} mu {d[14]} gdi_text {d[15]} gdi_copy {d[16]} gdi_calls {d[17]} gfx_body {d[18]}")
        prev = r
    n = max(1, len(rows) - 1)
    print("  per frame mean:", {k: round(v / n, 2) for k, v in tot.items()})
