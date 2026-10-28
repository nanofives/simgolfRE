"""S3.7 text gate: per in-game checkpoint, the OPAQUE pixels of the text regions must equal the reference.

The method is S3.6's (log/roadmap/S3/6b_hudgate.py): two framegate runs with the test-only root fill SG_GFX_HUD_BGFILL 0x0000 /
0x7fff after every present and --no-live (the terrain S3.2 draws is judged under G2, not pixel-exact, and stays out); a pixel
equal in both runs was drawn opaquely in the snapshot's frame and is compared with the reference. Blends (the shaded window
panels, the masked edges of the portraits) and pixels nothing drew are not judged here; the frame tests and the oracles cover
the rest. Regions (x, y, w, h):
  hud_message  framegate's region (the message line and the top of the ticker window)
  ticker       (0xc0, 0, 0x1f0, 0x98): drawWindow's ticker window at (0xf0, -8) with its golfer portrait
  tipband      (0xc0, 0x170, 0x280, 0x80): the HUD panels' hover tips above the bottom panel

    cd port/build/S3_7_run && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S3/7_textgate.py <out dir> [--reuse] [--jobs N]
        [scenario ...]   (default: every pinned scenario, framegate --all)
Writes <out>/textgate.tsv (one row per in-game checkpoint: opaque / bad px per region, the unsupported reasons before it) and
prints the totals.
"""
import csv
import json
import os
import pathlib
import subprocess
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.getcwd())
from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402

REGIONS = {"hud_message": dict(FG.INGAME_REGIONS)["hud_message"], "ticker": (0xC0, 0, 0x1F0, 0x98),
           "tipband": (0xC0, 0x170, 0x280, 0x80)}
FILLS = {"f0": "0x0000", "f1": "0x7fff"}


def run(out, scens, reuse, jobs):
    for tag, fill in FILLS.items():
        d = out / tag
        tsv = d / "gate.tsv"
        if reuse and tsv.exists():
            continue
        d.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, SG_GFX_HUD_BGFILL=fill)
        cmd = [sys.executable, "-m", "port.test.framegate"] + (scens or ["--all"]) + [
            "--jobs", str(jobs), "--no-probe", "--no-live", "--images", str(d / "img"), "--out", str(tsv)]
        if reuse:
            cmd.append("--resume")
        print("run", tag, flush=True)
        subprocess.run(cmd, env=env, check=True)


def rows_of(tsv):
    d = pathlib.Path(str(tsv) + ".d")
    out = []
    for p in sorted(d.glob("*.json")):
        out += json.loads(p.read_text())["rows"]
    return out


def main():
    args = sys.argv[1:]
    out = pathlib.Path(args[0]).resolve()
    reuse = "--reuse" in args
    jobs = int(args[args.index("--jobs") + 1]) if "--jobs" in args else 2
    scens = [a for i, a in enumerate(args[1:], 1) if not a.startswith("--") and args[i - 1] != "--jobs"]
    run(out, scens, reuse, jobs)
    res = []
    for r in rows_of(out / "f0" / "gate.tsv"):
        if r.get("kind") != "ingame" or r["status"] == "NOT_RUN":
            continue
        s, c = r["scenario"], r["checkpoint"]
        p0, p1 = out / "f0" / "img" / f"{s}.{c}.port.png", out / "f1" / "img" / f"{s}.{c}.port.png"
        if not p0.exists() or not p1.exists():
            continue
        a0, a1 = np.asarray(Image.open(p0).convert("RGB")), np.asarray(Image.open(p1).convert("RGB"))
        ref = GX.reference(s, c)
        opaque = (a0 == a1).all(axis=2)
        bad = opaque & (a0 != ref).any(axis=2)
        row = {"scenario": s, "checkpoint": c, "reasons": r.get("unsupported_before", "")}
        for n, (x, y, w, h) in REGIONS.items():
            row[f"{n}_opaque"] = int(opaque[y:y + h, x:x + w].sum())
            row[f"{n}_bad"] = int(bad[y:y + h, x:x + w].sum())
        res.append(row)
    cols = ["scenario", "checkpoint"] + [f"{n}_{k}" for n in REGIONS for k in ("opaque", "bad")] + ["reasons"]
    with open(out / "textgate.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(cols) + "\n")
        for row in res:
            f.write("\t".join(str(row[k]) for k in cols) + "\n")
    tot = {n: sum(r[f"{n}_bad"] for r in res) for n in REGIONS}
    rows_bad = {n: sum(1 for r in res if r[f"{n}_bad"]) for n in REGIONS}
    print(json.dumps({"rows": len(res), "bad_px": tot, "rows_with_bad": rows_bad}))


if __name__ == "__main__":
    main()
