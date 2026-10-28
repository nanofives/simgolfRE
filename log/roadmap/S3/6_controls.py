"""S3.6 frame-level negative controls: framegate runs of one scenario with gfx.family.hud / gfx.family.pixel modes, each compared
with the mode-0 run of the same DLL per checkpoint and HUD region (raw px against the reference). RED = some checkpoint's region
count differs from mode 0 (or the row's status / unsupported list changes).

    cd port/build/S3_6_src && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S3/6_controls.py <out dir> <scenario> name=mode...
"""
import csv
import os
import pathlib
import subprocess
import sys

HUD = ("hud_title", "hud_meters", "toolbar", "bottom_panel", "view")


def run(out, scen, negctl):
    tag = negctl.replace("=", "_") if negctl else "mode0"
    tsv = out / f"{scen}.{tag}.tsv"
    if not tsv.exists():
        cmd = [sys.executable, "-m", "port.test.framegate", scen, "--no-probe", "--out", str(tsv)]
        if negctl:
            cmd += ["--negctl", negctl]
        subprocess.run(cmd, check=True, capture_output=True, text=True)
    rows = {}
    for r in csv.DictReader(open(tsv, encoding="utf-8"), delimiter="\t"):
        reg = {}
        for part in (r["regions"] or "").split(";"):
            if "=" in part:
                k, v = part.split("=", 1)
                reg[k] = int(v.split("/")[0])
        rows[r["checkpoint"]] = (r["status"], r["unsupported_before"], reg)
    return rows


def main():
    out = pathlib.Path(sys.argv[1]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    scen = sys.argv[2]
    base = run(out, scen, None)
    for nc in sys.argv[3:]:
        rows = run(out, scen, nc)
        diffs = []
        for c, (st, un, reg) in rows.items():
            b = base.get(c)
            if b is None:
                continue
            for n in HUD:
                if reg.get(n, 0) != b[2].get(n, 0):
                    diffs.append(f"{c}:{n} {b[2].get(n, 0)}->{reg.get(n, 0)}")
            if st != b[0] or un != b[1]:
                diffs.append(f"{c}: status/unsupported changed")
        print(f"{scen} {nc}: {'RED' if diffs else 'GREEN'} ({len(diffs)} differences) {'; '.join(diffs[:6])}", flush=True)


if __name__ == "__main__":
    main()
