"""S3.7: attribute the text gate's bad opaque pixels. A pixel is this step's when the text family changed it: it differs
between the base build's census image and the family's (both raw framegate runs with the terrain live:
log/roadmap/S3/7_base/img and 7_d2/img). Bad opaque pixels the family did not change were already drawn (or not) the same way
without it: another step's.

    cd port/build/S3_7_run && ~/bin/py -3.12 <abs>/log/roadmap/S3/7_attrib.py <gate dir> <base img dir> <family img dir>
"""
import csv
import json
import os
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.getcwd())
from port.test import gfx as GX  # noqa: E402

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import importlib.util  # noqa: E402

spec = importlib.util.spec_from_file_location("tg", pathlib.Path(__file__).resolve().parent / "7_textgate.py")
TG = importlib.util.module_from_spec(spec)
spec.loader.exec_module(TG)


def main():
    gate, base, fam = (pathlib.Path(a) for a in sys.argv[1:4])
    rows = list(csv.DictReader(open(gate / "textgate.tsv", encoding="utf-8"), delimiter="\t"))
    tot_mine = {n: 0 for n in TG.REGIONS}
    tot_other = {n: 0 for n in TG.REGIONS}
    mine_rows = []
    for r in rows:
        s, c = r["scenario"], r["checkpoint"]
        if not any(int(r[f"{n}_bad"]) for n in TG.REGIONS):
            continue
        a0 = np.asarray(Image.open(gate / "f0" / "img" / f"{s}.{c}.port.png").convert("RGB"))
        a1 = np.asarray(Image.open(gate / "f1" / "img" / f"{s}.{c}.port.png").convert("RGB"))
        ref = GX.reference(s, c)
        bad = (a0 == a1).all(axis=2) & (a0 != ref).any(axis=2)
        pb, pf = base / f"{s}.{c}.port.png", fam / f"{s}.{c}.port.png"
        if not pb.exists() or not pf.exists():
            continue
        changed = (np.asarray(Image.open(pb).convert("RGB")) != np.asarray(Image.open(pf).convert("RGB"))).any(axis=2)
        out = {}
        for n, (x, y, w, h) in TG.REGIONS.items():
            m = int((bad & changed)[y:y + h, x:x + w].sum())
            o = int((bad & ~changed)[y:y + h, x:x + w].sum())
            tot_mine[n] += m
            tot_other[n] += o
            out[n] = (m, o)
        if any(v[0] for v in out.values()):
            ys, xs = np.nonzero(bad & changed)
            mine_rows.append((s, c, out, (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())) if len(xs) else None))
    for s, c, out, bb in mine_rows:
        print(s, c, {k: v[0] for k, v in out.items()}, "bbox", [hex(v) for v in bb] if bb else "-")
    print(json.dumps({"family_changed_bad_px": tot_mine, "unchanged_bad_px": tot_other, "rows_with_family_bad": len(mine_rows)}))


if __name__ == "__main__":
    main()
