"""P3: summarise the long-session runs in log/roadmap/P3/runs/<name>/memory.tsv.

    ~/bin/py -3.12 log/roadmap/P3/summarize.py [--steps] [name ...]

Per run: test_app_present's _growth (private bytes of the LAST sample minus the first sample at frame >= 2000), the same
measure taken at the pre-teardown sample (first of the samples with the final frame number), and the extra columns written
under SG_P3_MEMX (heap allocated / committed summed over GetProcessHeaps, heaps, threads, MEM_PRIVATE committed, private
regions, private commit in regions >= 512 KB, writable committed non-private) at frame 2000, pre-teardown and end.
--steps prints every sample-to-sample change of private bytes that is not 0.
"""
import pathlib
import sys

RUNS = pathlib.Path(__file__).resolve().parent / "runs"
COLS = ["frame", "ws", "peak", "priv", "tlog", "heap_alloc", "heap_commit", "heaps", "threads", "priv_commit", "priv_regions",
        "priv_big", "wr_shared"]


def load(name):
    rows = []
    for line in (RUNS / name / "memory.tsv").read_text().splitlines()[1:]:
        if line:
            rows.append([int(x) for x in line.split("\t")])
    return rows


def main(argv):
    steps = "--steps" in argv
    names = [a for a in argv if not a.startswith("--")] or sorted(p.name for p in RUNS.iterdir() if (p / "memory.tsv").exists())
    for n in names:
        rows = load(n)
        after = [r for r in rows if r[0] >= 2000]
        a0 = after[0]
        final = rows[-1][0]
        tail = [r for r in rows if r[0] == final]
        pre = tail[0] if len(tail) >= 3 else None
        meta = (RUNS / n / "meta.txt").read_text().split("\n")[-2] if (RUNS / n / "meta.txt").exists() else ""
        print(f"== {n}: growth(test)={rows[-1][3] - a0[3]:,}  pre-teardown growth="
              f"{format(pre[3] - a0[3], ',') if pre else 'n/a'}  max-after-2000={max(r[3] for r in after) - a0[3]:,}  {meta}")
        if len(a0) > 5:
            for label, r in (("f2000", a0), ("pre", pre), ("end", rows[-1])):
                if r is None:
                    continue
                print("   " + label.ljust(6) + "  ".join(f"{c}={v:,}" for c, v in zip(COLS[3:], r[3:]) if c != "tlog"))
        if steps:
            prev = None
            for r in rows:
                if prev is not None and r[3] != prev[3]:
                    extra = ""
                    if len(r) > 5:
                        extra = "  dheapA={:+,} dheapC={:+,} dpriv_big={:+,} dthreads={:+}".format(
                            r[5] - prev[5], r[6] - prev[6], r[11] - prev[11], r[8] - prev[8])
                    print(f"   frame {prev[0]:>6} -> {r[0]:>6}: {r[3] - prev[3]:+,}{extra}")
                prev = r


if __name__ == "__main__":
    main(sys.argv[1:])
