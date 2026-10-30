"""S3.12: compare the step files (S3.10 / S3.10b mirror trees) against bbc9072f, HEAD 01d84188 and the shared tree.

Line endings are normalised (CR LF vs LF) before comparing: git archive trees and the shared checkout differ only in that.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
A = ROOT / "port/build/S3_10_head"
B = ROOT / "port/build/S3_10b_head"
LIVE = ROOT / "log/roadmap/S3/10b_live"
FILES = [
    "port/src/sim/gl_raster.cpp", "port/include/portsim/sim/gl_raster.h", "port/src/sim/gfx_terrain.cpp",
    "port/test/test_gl_raster.py", "port/test/glraster.py", "port/src/sim/terrain_state.cpp",
    "port/include/portsim/sim/terrain_state.h", "port/test/test_terrain_state.py",
    "port/src/sim/terrain_live.cpp", "port/include/portsim/sim/terrain_live.h", "port/app/host.cpp",
]


def show(rev, f):
    r = subprocess.run(["git", "show", f"{rev}:{f}"], cwd=ROOT, capture_output=True)
    return r.stdout.replace(b"\r\n", b"\n") if r.returncode == 0 else None


def read(p):
    return p.read_bytes().replace(b"\r\n", b"\n") if p.exists() else None


def tag(x, y):
    if x is None or y is None:
        return "n/a"
    return "same" if x == y else "DIFF"


for f in FILES:
    bbc, head = show("bbc9072f", f), show("01d84188", f)
    a, b, sh = read(A / f), read(B / f), read(ROOT / f)
    print(f"{f}\n   bbc-HEAD {tag(bbc, head)}  A-bbc {tag(a, bbc)}  B-bbc {tag(b, bbc)}  B-A {tag(b, a)}  "
          f"shared-B {tag(sh, b)}  shared-HEAD {tag(sh, head)}")
for n in ["terrain_live.cpp", "terrain_live.h", "host.cpp"]:
    p = LIVE / n
    f = {"terrain_live.cpp": "port/src/sim/terrain_live.cpp", "terrain_live.h": "port/include/portsim/sim/terrain_live.h",
         "host.cpp": "port/app/host.cpp"}[n]
    print(f"10b_live/{n}: vs bbc {tag(read(p), show('bbc9072f', f))}  vs HEAD {tag(read(p), show('01d84188', f))}  "
          f"vs B {tag(read(p), read(B / f))}")
