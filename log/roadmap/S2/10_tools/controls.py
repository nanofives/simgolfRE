"""S2.10 controls: each negative control of this step's drawing read against the original's frame through framegate (a control is
RED when its checkpoint's pixels differ more than with the control off).

    cd <tree>; SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <this> <out tsv> [name=mode ...]
"""
import csv
import os
import pathlib
import subprocess
import sys

LOG = pathlib.Path(__file__).resolve().parents[1]
CONTROLS = [
    # (negctl, mode, scenario, checkpoint, what)
    ("gfx.family.chareditor", 1, "cov_r_s2_screens", "editor", "the other gender's background"),
    ("gfx.family.chareditor", 2, "cov_r_s2_screens", "editor", "the background one row lower"),
    ("gfx.family.chareditor", 3, "cov_a_keys3", "q_editor", "drawMessageBuffer one pixel right (the bio)"),
    ("gfx.family.buyland", 1, "cov_r_s2_screens", "buy_land", "drawTextLinesAt's lines one pixel further apart"),
    ("gfx.family.buyland", 2, "cov_r_s2_screens", "buy_land", "wrapTextToWidth breaks after the word that did not fit"),
    ("gfx.family.buyland", 3, "cov_r_s2_screens", "buy_land", "drawRight one pixel left"),
    ("gfx.attach", 7, "cov_r_s2_screens", "buy_land", "drawCenteredText / drawText4049d0 leave the quad (the lot label's colour)"),
    ("gfx.family.prim_text2", 3, "cov_r_s2_screens", "editor", "drawCenteredText2 leaves the quad"),
]


def run(neg, scen, chk, out):
    cmd = [sys.executable, "-m", "port.test.framegate", scen, "--checkpoints", chk, "--no-probe", "--out", str(out)]
    if neg:
        cmd += ["--negctl", neg]
    subprocess.run(cmd, capture_output=True, text=True)
    rows = list(csv.DictReader(open(out, encoding="utf-8"), delimiter="\t"))
    return rows[0] if rows else {}


def main():
    out = pathlib.Path(sys.argv[1])
    sel = set(sys.argv[2:])
    tmp = out.with_suffix(".d")
    tmp.mkdir(exist_ok=True)
    base = {}
    res = []
    for name, mode, scen, chk, what in CONTROLS:
        if sel and f"{name}={mode}" not in sel:
            continue
        if (scen, chk) not in base:
            r = run("", scen, chk, tmp / f"base_{scen}_{chk}.tsv")
            base[(scen, chk)] = int(r.get("total_px") or -1)
        r = run(f"{name}={mode}", scen, chk, tmp / f"{name}_{mode}_{scen}_{chk}.tsv")
        px = int(r.get("total_px") or -1)
        verdict = "RED" if px > base[(scen, chk)] else "BLIND"
        res.append([name, mode, scen, chk, base[(scen, chk)], px, verdict, what])
        print(f"[{len(res)}/{len(CONTROLS)}] {name}={mode} {scen}/{chk} base {base[(scen, chk)]} px {px} {verdict}", flush=True)
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(["negctl", "mode", "scenario", "checkpoint", "base_px", "control_px", "verdict", "what"])
        w.writerows(res)


if __name__ == "__main__":
    main()
