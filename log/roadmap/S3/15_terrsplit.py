"""S3.15: split a checkpoint's differing HUD top-bar pixels by where the difference comes from, using the original's recorded
root at updateTerrainRender's return (log/p5/terrain/<label>/frames/<chk>/exit.bin, S3.2's recorder) and the port's root at the
same frame (terrainLive.keepFrame).

    cd port/build/S3_15 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/15_terrsplit.py <scenario> <label> <chk,...>

Classes per differing pixel p (final port frame != final reference frame):
  T  terrain exit already differs (orig exit != port exit) and both finals equal their own exits (the HUD draws nothing there)
  TB terrain exit differs and the HUD changed the pixel after the terrain (a 2D body over a terrain pixel that was already off)
  B  terrain exits equal: the difference is made after updateTerrainRender (a 2D body)
"""
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import framegate as FG  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import terrainlive as TL  # noqa: E402
from port.test import gfx as GX  # noqa: E402

REG = dict(FG.INGAME_REGIONS)
TOP = ("hud_title", "hud_meters", "hud_message", "toolbar", "bottom_panel", "view")


def rgb(a):
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=-1).astype(int)


def main():
    scen, label, chks = sys.argv[1], sys.argv[2], sys.argv[3].split(",")
    lib = L.load()
    rec = TL.recorded_frames(label)
    for c in chks:
        e = rec[c]
        k = e["meta"]["frame"]
        pf = TL.port_frames(lib, scen, [k], c)
        cap = []
        o = FG.region_numbers
        FG.region_numbers = lambda p, r, kd: (cap.append((p.copy(), r.copy())), o(p, r, kd))[1]
        FG.run_scenario(scen, [c], probe=False)
        FG.region_numbers = o
        port, ref = cap[0]
        P, R = port.astype(int) >> 3, ref.astype(int) >> 3
        OE, PE = rgb(e["exit"]), rgb(pf[(k, "exit")])
        diff = np.abs(P - R).max(2) > 0
        ediff = (OE != PE).any(2)
        hudo = (R != OE).any(2)      # the original changed the pixel after the terrain
        hudp = (P != PE).any(2)
        print(f"== {scen} {c} frame {k}  diff {diff.sum()}  terrain-exit diff {ediff.sum()}")
        lab, names = FG.label_map("ingame")
        for i, n in enumerate(names):
            d = diff & (lab == i)
            T = d & ediff & ~hudo & ~hudp
            TB = d & ediff & (hudo | hudp)
            Bc = d & ~ediff
            print(f"  {n}: {d.sum()}  T {T.sum()}  TB {TB.sum()}  B {Bc.sum()}")
            for cls, mm in (("TB", TB), ("B", Bc)):
                ys, xs = np.nonzero(mm)
                for y, x in list(zip(ys, xs))[:12]:
                    print(f"    {cls} ({x},{y}) port {tuple(P[y, x])} ref {tuple(R[y, x])} portExit {tuple(PE[y, x])} "
                          f"origExit {tuple(OE[y, x])}")


if __name__ == "__main__":
    main()
