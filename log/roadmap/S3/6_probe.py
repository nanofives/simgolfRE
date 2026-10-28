"""S3.6 probe: one framegate boot of a scenario's checkpoints with the gfx backend, keeping what the census row does not show:
the whole gfx.log, the port / reference / diff images and (with --trace) tick's SG_TICK_TRACE call trace of the run.

    cd port/build/S3_6_src && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S3/6_probe.py <scenario> <chk,chk> <out dir> [--trace]
"""
import os
import pathlib
import sys

out = pathlib.Path(sys.argv[3]).resolve()
out.mkdir(parents=True, exist_ok=True)
if "--trace" in sys.argv:
    os.environ["SG_TICK_TRACE"] = str(out / "tick_trace.txt")
sys.path.insert(0, os.getcwd())
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402

logs = []
_orig = FG.parse_gfx_log


def _keep(txt):
    logs.append(txt)
    return _orig(txt)


FG.parse_gfx_log = _keep
scen, chks = sys.argv[1], sys.argv[2].split(",")
rows = FG.run_scenario(scen, chks, images=out, probe=False)
(out / f"{scen}.gfx_log.txt").write_text(logs[0] if logs else "", encoding="utf-8")
for r in rows:
    c = r["checkpoint"]
    ref = GX.reference(scen, c)
    Image.fromarray(ref).save(out / f"{scen}.{c}.ref.png")
    print(c, r["status"], r.get("regions", ""), "|", r.get("first_unsupported_drawing", ""))
FG.write_tsv(out / f"{scen}.tsv", rows)
