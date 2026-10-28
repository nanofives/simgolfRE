"""S3.6: one image per HUD-gate row with bad opaque pixels (port f0 | reference | bad mask, the whole frame at half size) and a
TSV of each row's bad-pixel bounding box per region, for attributing the residuals by hand (6_notes.md, the gate table).

    cd port/build/S3_6_src && ~/bin/py -3.12 <abs>/log/roadmap/S3/6_sheet.py <gate out dir>
"""
import csv
import os
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.getcwd())
from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402

HUD = ["hud_title", "hud_meters", "toolbar", "bottom_panel"]


def main():
    out = pathlib.Path(sys.argv[1]).resolve()
    sheet = out / "sheet"
    sheet.mkdir(exist_ok=True)
    regs = dict(FG.INGAME_REGIONS)
    rows = list(csv.DictReader(open(out / "hudgate.tsv", encoding="utf-8"), delimiter="\t"))
    res = []
    for r in rows:
        if not any(int(r[f"{n}_bad"]) for n in HUD):
            continue
        s, c = r["scenario"], r["checkpoint"]
        a0 = np.asarray(Image.open(out / "f0" / "img" / f"{s}.{c}.port.png").convert("RGB"))
        a1 = np.asarray(Image.open(out / "f1" / "img" / f"{s}.{c}.port.png").convert("RGB"))
        ref = GX.reference(s, c)
        bad = (a0 == a1).all(axis=2) & (a0 != ref).any(axis=2)
        row = {"scenario": s, "checkpoint": c}
        for n in HUD:
            x, y, w, h = regs[n]
            ys, xs = np.nonzero(bad[y:y + h, x:x + w])
            row[n] = f"{len(xs)}@{xs.min() + x},{ys.min() + y}-{xs.max() + x},{ys.max() + y}" if len(xs) else "0"
        res.append(row)
        big = np.concatenate([a0, ref, (bad[..., None] * 255).repeat(3, 2).astype(np.uint8)], 1)[::2, ::2]
        Image.fromarray(big).save(sheet / f"{s}.{c}.png")
    with open(out / "sheet.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(["scenario", "checkpoint"] + HUD) + "\n")
        for r in res:
            f.write("\t".join(r[k] for k in ["scenario", "checkpoint"] + HUD) + "\n")
    print(len(res), "rows with bad pixels")


if __name__ == "__main__":
    main()
