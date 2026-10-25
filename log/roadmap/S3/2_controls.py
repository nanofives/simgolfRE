"""S3.2: the terrain family's negative controls with their numbers (how many recorded copies each one breaks), run from the S3.2 root:

    cd port/build/S3_2_root && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/2_controls.py
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L  # noqa: E402
from port.test import test_terrain_live as T  # noqa: E402

lib = L.load()
for mode, (label, why) in sorted(T.TERRAIN_NEGCTL.items()):
    ok = T._port(lib, label)
    bad = T._port(lib, label, negctl=("gfx.family.terrain", mode))
    keys = T._copies(label)
    good = [T._judge(T._rec(label), ok, c, w) for _l, c, w in keys]
    res = [T._judge(T._rec(label), bad, c, w) for _l, c, w in keys]
    red = [(c, w, r) for (_l, c, w), r in zip(keys, res) if r is None or not r["pass"]]
    worst = max((r["big"] for _c, _w, r in red if r), default=0)
    print(f"mode {mode} ({why}) on {label}: correct body {sum(1 for r in good if r and r['pass'])}/{len(keys)} G2, "
          f"control RED on {len(red)}/{len(keys)} copies, largest beyond-one-level count {worst}", flush=True)
    for c, w, r in red[:3]:
        print(f"    {c} {w}: {r}")
