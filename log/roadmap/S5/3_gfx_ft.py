"""S5.3 gate (c): the six gfx checkpoints that PASS at HEAD (sandbox_basic menu, cov_b_location refused / site_hover / buttons /
tip / back) booted with a chosen text provider attached in place of GDI, the snapshot compared with the original's back
buffer (port/test/gfx.py reference) in total, per region (menu: gfx.menu_regions) and inside the label boxes S5.2 recorded.
Mirrors port/test/gfx.render, with SG_TEXT_METRICS=none so boot.run attaches no GDI provider and `before` attaches
<provider>.attach first.

    SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 log/roadmap/S5/3_gfx_ft.py [--provider fttext|gditext] [--negctl N]
        [--backend x64|win32 --w32-dir port/build/S5_3_w32] [--out <json>]
"""
import argparse
import json
import os
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
os.environ["SG_TEXT_METRICS"] = "none"
from port.test import boot, gfx as GX, labels as LB  # noqa: E402
TF = GX.TF

CHECKPOINTS = {"sandbox_basic": ["menu"], "cov_b_location": ["refused", "site_hover", "buttons", "tip", "back"]}


def run(lib, provider, scen, chks):
    ords = sorted((GX.checkpoint_ordinal(scen, c), c) for c in chks)
    no_frames = not (boot.expected_records(scen)[1] or [])
    tr = TF.read(boot.TRACES / f"{scen}.trace")
    max_records = sum(1 for r in tr.records if r.type == TF.FRAME and r.f["pos"] <= ords[-1][0]) + 1
    frames = {}

    def before(st):
        st.call(f"{provider}.attach", [])
        st.call("gfx.attach", [])
        for o, _c in ords:
            st.call("gfx.snapshotAt", [o])

    def after(st, _res):
        for k, (_o, c) in enumerate(ords):
            raw = st.files_read(f"$gfx/snapshot_{k}.555")
            frames[c] = None if raw is None or len(raw) != GX.W * GX.H * 2 else np.frombuffer(raw, "<u2").reshape(GX.H, GX.W)
        st.call("gfx.detach", [])
        st.call(f"{provider}.detach", [])

    res = boot.run(scen, lib, max_records=max_records, before=before, after=after, to_end=no_frames)
    return res, frames


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--provider", default="fttext")
    ap.add_argument("--negctl", type=int, default=0)
    ap.add_argument("--backend", default="x64", choices=["x64", "win32"])
    ap.add_argument("--w32-dir")
    ap.add_argument("--out")
    a = ap.parse_args()
    lib = LB.load_lib(a.backend, a.w32_dir)
    prev = lib.negctl("fttext.fontWidth", a.negctl) if a.negctl else None
    summary = json.loads((LB.OUT / "checkpoints" / "summary.x64.json").read_text())
    out = {}
    try:
        for scen, chks in CHECKPOINTS.items():
            res, frames = run(lib, a.provider, scen, chks)
            for c in chks:
                img = frames.get(c)
                if img is None:
                    out[f"{scen}/{c}"] = {"boot": res.status, "total": -1}
                    continue
                rgbimg = LB.rgb(img)
                ref = GX.reference(scen, c)
                d = GX.imgcmp.exact_diff(rgbimg, ref, GX.menu_regions() if c == "menu" else None)
                mask = (rgbimg != ref).any(axis=2)
                boxes = summary[scen]["checkpoints"][c]["boxes"]
                inbox = np.zeros_like(mask)
                for x0, y0, x1, y1 in boxes:
                    inbox[max(y0, 0):y1, max(x0, 0):x1] = True
                out[f"{scen}/{c}"] = {"boot": res.status, "total": d["total"], "regions": d["regions"], "bbox": d["bbox"],
                                      "in_label_boxes": int((mask & inbox).sum()), "outside_label_boxes": int((mask & ~inbox).sum())}
                r = out[f"{scen}/{c}"]
                print(f"{scen}/{c}: boot {r['boot']} {r['total']} px differ (in label boxes {r['in_label_boxes']}, outside "
                      f"{r['outside_label_boxes']}), regions {r['regions']}", flush=True)
    finally:
        if a.negctl:
            lib.negctl("fttext.fontWidth", prev or 0)
    if a.out:
        pathlib.Path(a.out).write_text(json.dumps(out, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
