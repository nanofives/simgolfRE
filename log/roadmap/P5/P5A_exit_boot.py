"""P5A item 1: the boot gate's cov_a_quit / cov_a_quit_title (their quit is a click inside the exit icon, (750, 553) in cov_a_quit)
with mainLoop.titleExitHotspotWide at 1 (the original) and at 0 (the fix). Run from a tree root."""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import boot
from port.test import lib as L
lib = L.load()
for sw in (1, 0):
    lib.defect_reset(); lib.defect("mainLoop.titleExitHotspotWide", bool(sw))
    for sc in ("cov_a_quit", "cov_a_quit_title"):
        r = boot.run(sc, lib)
        print(f"switch {sw} {sc}: {getattr(r, 'status', r)} {getattr(r, 'records', '')}/{getattr(r, 'expected', '')}")
lib.defect_reset()
