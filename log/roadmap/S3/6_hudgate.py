"""S3.6 HUD gate: per in-game checkpoint, the HUD regions' pixels the port draws OPAQUELY must equal the reference.

The port's root surface keeps whatever the last frame left where nothing draws (the terrain is S3.2's, the scene sprites
S3.5's), so a region's raw px count mixes the HUD's own pixels with a background other steps own. Two framegate runs with the
test-only background fill (gfx_hud.cpp after_present: after every present the root is filled with SG_GFX_HUD_BGFILL) separate
them: a pixel equal in the run filled with 0x0000 and the run filled with 0x7fff was drawn opaquely in the snapshot's frame
(its value does not depend on the background); every other pixel is background or a blend over it (shaded strips, mask edges)
and is not judged here. Per checkpoint and region: opaque px, opaque px that differ from the reference, and the HUD-family
unsupported reasons before the snapshot (OWN_REASONS); a row is CLEAN when it has no own reason.

    cd port/build/S3_6_src && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S3/6_hudgate.py <out dir> <scenario>... [--reuse]
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

HUD = ["hud_title", "hud_meters", "toolbar", "bottom_panel"]
FILLS = {"f0": "0x0000", "f1": "0x7fff"}
# unsupported reasons that are this step's (a body of the hud / pixel families, or a drop they own)
OWN_PREFIX = ("Surface_blit3", "Surface_blit2", "Surface_drawScaled", "drawScaledSprite421b", "drawFrame", "drawTerrainToolbar",
              "showMainRadialMenu", "showInfoCursorPanel", "loadWorldAssets", "drawGolferActionPanel", "drawEmployeeViewPanel",
              "drawEmployeeDetailPanel", "drawAmenityRow", "drawAmenitiesPanel", "Widget_writePixel", "Widget_fillRect", "loadInterfaceArt", "no body: Surface_blit3",
              "no body: Widget_fillRect", "no body: drawFrameS2", "no body: drawFrameR3", "no body: drawTerrainToolbar",
              "no body: Surface_drawScaled2", "no body: drawScaledSprite421b90", "no body: Widget_writePixel")


def run(out, scen, reuse):
    for tag, fill in FILLS.items():
        d = out / tag
        tsv = d / f"{scen}.tsv"
        if reuse and tsv.exists():
            continue
        env = dict(os.environ, SG_GFX_HUD_BGFILL=fill)
        subprocess.run([sys.executable, "-m", "port.test.framegate", scen, "--no-probe", "--images", str(d / "img"), "--out", str(tsv)],
                       env=env, check=True, capture_output=True, text=True)


def own(reason):
    return any(reason.startswith(p) or reason.startswith("no body: " + p) for p in OWN_PREFIX)


def rect_mask(name):
    regs = dict(FG.INGAME_REGIONS)
    x, y, w, h = regs[name]
    m = np.zeros((GX.H, GX.W), bool)
    m[y:y + h, x:x + w] = True
    return m


def analyse(out, scen):
    rows = list(csv.DictReader(open(out / "f0" / f"{scen}.tsv", encoding="utf-8"), delimiter="\t"))
    res = []
    for r in rows:
        if r["kind"] != "ingame" or r["status"] == "NOT_RUN":
            continue
        c = r["checkpoint"]
        p0 = out / "f0" / "img" / f"{scen}.{c}.port.png"
        p1 = out / "f1" / "img" / f"{scen}.{c}.port.png"
        if not p0.exists() or not p1.exists():
            continue
        a0, a1 = np.asarray(Image.open(p0).convert("RGB")), np.asarray(Image.open(p1).convert("RGB"))
        ref = GX.reference(scen, c)
        opaque = (a0 == a1).all(axis=2)
        bad = opaque & (a0 != ref).any(axis=2)
        reasons = [x for x in r["unsupported_before"].split("|") if x]
        mine = [x for x in reasons if own(x)]
        row = {"scenario": scen, "checkpoint": c, "clean": int(not mine), "own_reasons": "|".join(mine),
               "other_reasons": "|".join(x for x in reasons if not own(x))}
        for n in HUD:
            m = rect_mask(n)
            row[f"{n}_opaque"] = int((opaque & m).sum())
            row[f"{n}_bad"] = int((bad & m).sum())
        res.append(row)
    return res


def main():
    out = pathlib.Path(sys.argv[1]).resolve()
    reuse = "--reuse" in sys.argv
    scens = [a for a in sys.argv[2:] if not a.startswith("--")]
    allrows = []
    for s in scens:
        run(out, s, reuse)
        rs = analyse(out, s)
        allrows += rs
        tot = {n: sum(r[f"{n}_bad"] for r in rs) for n in HUD}
        print(f"{s}: {len(rs)} in-game rows, clean {sum(r['clean'] for r in rs)}, bad opaque px {tot}", flush=True)
    cols = ["scenario", "checkpoint", "clean", "own_reasons", "other_reasons"] + [f"{n}_{k}" for n in HUD for k in ("opaque", "bad")]
    with open(out / "hudgate.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(cols) + "\n")
        for r in allrows:
            f.write("\t".join(str(r[k]) for k in cols) + "\n")
    print(json.dumps({"rows": len(allrows), "clean": sum(r["clean"] for r in allrows),
                      "clean_bad_px": sum(r[f"{n}_bad"] for r in allrows if r["clean"] for n in HUD)}))


if __name__ == "__main__":
    main()
