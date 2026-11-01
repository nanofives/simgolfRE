"""S3.22: print gfx.check and the count of `body` lines of $gfx/bodies.txt for the DLL in SG_PORTSIM_DLL.
    cd port/build/S3_22 && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/22_count_bodies.py"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L  # noqa: E402
from port.test import gfx as GX  # noqa: E402
from port.test import test_gfx as TG  # noqa: E402

lib = L.load()
with TG._gstate(lib) as st:
    chk = st.call("gfx.check", []).eax
    lines = st.files_read("$gfx/bodies.txt").decode().splitlines()
bodies = [l for l in lines if l.startswith("body\t")]
print("gfx.check", chk, "bodies", len(bodies), "has bio", any("showCharacterEditor.bio" in l for l in lines))
