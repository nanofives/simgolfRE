"""S2.11: render this step's checkpoints with a DLL and report, per checkpoint, the pixels that differ from the reference
and the unsupported drawing calls before the snapshot.

usage (from the mirror root): SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S2/11_tools/probe.py [scenario ...]
       --save <dir>   also write <dir>/<scenario>_<checkpoint>.png (port frame) and _diff.png (red where it differs)
"""
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import gfx as GX  # noqa: E402
from port.test import lib as L  # noqa: E402

CHECKS = {
    "cov_b_shots": ["results", "results_r2"],
    "cov_b_shots2": ["results", "results_r2", "results_r3"],
    "championship": ["tour_0180", "tournament_end"],
    "ui_tour": ["menus"],
    "cov_k1_animals": ["bought"],
    "cov_b_inject2": ["debug_overlay"],
    "cov_k2_simfoto": ["bottom", "right"],
    "cov_a_golfer": ["button_2"],
    "cov_r_s2_screens": ["bulletin"],
}


def main(argv):
    save = None
    if "--save" in argv:
        i = argv.index("--save")
        save = pathlib.Path(argv[i + 1])
        save.mkdir(parents=True, exist_ok=True)
        argv = argv[:i] + argv[i + 2:]
    negctl, name = 0, "gfx.attach"
    if "--negctl" in argv:
        i = argv.index("--negctl")
        name, negctl = argv[i + 1], int(argv[i + 2], 0)
        argv = argv[:i] + argv[i + 3:]
    scens = argv or list(CHECKS)
    lib = L.load()
    if not lib.exe_loaded():
        lib.exe_load()
    for s in scens:
        cps = [c for c in CHECKS[s] if (GX.REFS / s / "det_b" / f"{c}.png").exists()]
        run = GX.render(s, cps, lib, negctl=negctl, negctl_name=name)
        for c in cps:
            img = run.frames.get(c)
            if img is None:
                print(f"{s}\t{c}\tNO_SNAPSHOT")
                continue
            ref = GX.reference(s, c)
            d = (img != ref).any(2)
            print(f"{s}\t{c}\tdiff {int(d.sum())}\tunsup_before {run.unsupported_before.get(c)}")
            if save:
                Image.fromarray(img).save(save / f"{s}_{c}.png")
                v = img.copy()
                v[d] = (255, 0, 0)
                Image.fromarray(v).save(save / f"{s}_{c}_diff.png")
        print("---- log", s)
        for line in run.log.splitlines():
            if line.startswith("unsupported") or line.startswith("first") or line.startswith("snapshot"):
                print("   ", line[:220])


if __name__ == "__main__":
    main(sys.argv[1:])
