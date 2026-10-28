"""S3.14: the boot's present timeline of a kept app session: for every present made before the first frame head, the clock
value (ms) at its recording position (the last timeGetTime the trace served at or before it), and the gaps between them.
A driver that waits for `quiet` seconds without a present (app_replay.Driver.wait_quiet, test_app_live._wait_quiet) can
return during the boot when a gap exceeds it.

    ~/bin/py -3.12 log/roadmap/S3/14_boot_gaps.py <session dir> [...]
"""
import bisect
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF  # noqa: E402

for arg in sys.argv[1:]:
    rec = pathlib.Path(arg) / "rec"
    tr = TF.read(rec / "trace.trace")
    calls = [(o, v) for o, _ra, _s, kind, v in TF.clock_calls(tr) if kind == 1]
    calls.sort()
    ords = [o for o, _ in calls]
    rows = [l.split("\t") for l in (rec / "presents.tsv").read_text().splitlines()[1:] if l.strip()]
    heads = [l.split("\t") for l in (rec / "frames.tsv").read_text().splitlines() if l.strip()]
    t_head1 = int(heads[0][1]) if heads else None
    out = []
    for r in rows:
        pos, frame = int(r[2]), int(r[4])
        i = bisect.bisect_right(ords, pos - 1) - 1
        t = calls[i][1] if i >= 0 else None
        out.append((int(r[0]), frame, pos, t))
    boot = [o for o in out if o[3] is not None and (t_head1 is None or o[3] < t_head1)]
    gaps = [(b[3] - a[3], a[0], b[0]) for a, b in zip(boot, boot[1:])]
    print(f"== {arg}: {len(boot)} presents before the first frame head (T {t_head1}); first T {boot[0][3] if boot else None}")
    for g, a, b in sorted(gaps, reverse=True)[:8]:
        print(f"  gap {g} ms between present {a} and {b}")
    if t_head1 and boot:
        print(f"  last boot present -> first frame head: {t_head1 - boot[-1][3]} ms")
