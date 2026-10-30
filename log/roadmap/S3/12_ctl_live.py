"""S3.12: negative controls of the LiveTerrain call log (terrain_live.cpp, roadmap S3.12 / U-1673), judged by the pinned
terrain_calls recordings exactly as test_terrain_live.test_calls_equal_the_original judges the default build:
  mode 0   the build as merged: the call log equals the original's (GREEN expected)
  mode 6   gfx.family.terrain = 6: set_keep_log(false) from the first call (the app's play-session setting in a run that reads
           the log): the log is empty, so it must read RED
  mode 7   gfx.family.terrain = 7: the compact record drops setType's variation, so the lazy line has no `var=`: RED

    (cd port/build/S3_12 && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/12_ctl_live.py [scenario ...])
"""
import os
import pathlib
import sys

TREE = pathlib.Path(os.environ.get("SG_TREE") or pathlib.Path(__file__).resolve().parents[3] / "port" / "build" / "S3_12")
sys.path.insert(0, str(TREE))

from port.test import lib as L  # noqa: E402
from port.test import terrainlive as TL  # noqa: E402


def main():
    lib = L.load(os.environ.get("SG_PORTSIM_DLL"))
    scen = sys.argv[1:] or TL.pinned()[:3]
    bad = 0
    for sc in scen:
        for mode in (0, 6, 7):
            lib.negctl("gfx.family.terrain", mode)
            try:
                r = TL.run(lib, sc)
            finally:
                lib.negctl("gfx.family.terrain", 0)
            want = mode == 0
            ok = r.calls_equal == want
            bad += not ok
            print(f"{sc}\tmode {mode}\tcalls_equal={r.calls_equal}\t{len(r.orig)}/{len(r.port)} calls\t"
                  f"first_diff={str(r.first_call_diff)[:120]}\t{'ok' if ok else 'WRONG'}", flush=True)
    print(f"controls: {'all as expected' if not bad else f'{bad} wrong'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
