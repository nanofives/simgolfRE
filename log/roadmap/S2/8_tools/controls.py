"""S2.8: run the reports family's negative controls over the judged cov_r_s2_screens checkpoints (run from the mirror root
with SG_PORTSIM_DLL set). The held masks come from one set of underlay boots; each mode is one more boot.

usage: ~/bin/py -3.12 <this> --out log/roadmap/S2/8_controls.tsv [--modes 1,2,...]
"""
import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L  # noqa: E402
from port.test import test_gfx_reports as R  # noqa: E402

MODES = [1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 18, 19]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--modes")
    a = ap.parse_args()
    modes = [int(m) for m in a.modes.split(",")] if a.modes else MODES
    lib = L.load()
    cps = R.JUDGED_CHECKPOINTS
    held = R.held_masks(cps, lib)
    base = R.judge(cps, lib, held=held)
    rows = ["mode\t" + "\t".join(cps) + "\tverdict"]
    rows.append("0\t" + "\t".join(str(base[c]["diff"]) for c in cps) + "\tbaseline")
    print(rows[-1], flush=True)
    for i, m in enumerate(modes, 1):
        r = R.judge(cps, lib, negctl=(R.FAMILY, m), held=held)
        diffs = [r[c]["diff"] for c in cps]
        red = any(d != base[c]["diff"] or r[c]["judged"] != base[c]["judged"] for d, c in zip(diffs, cps))
        rows.append(f"{m}\t" + "\t".join(str(d) for d in diffs) + ("\tRED" if red else "\tblind"))
        print(f"[{i}/{len(modes)}] mode {m} {'RED' if red else 'blind'} " + " ".join(f"{c}={d}" for c, d in zip(cps, diffs)),
              flush=True)
    pathlib.Path(a.out).write_text("\n".join(rows) + "\n")


if __name__ == "__main__":
    main()
