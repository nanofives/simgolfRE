"""S3.4c: what the emulated original and the port do on the tropical cases of port/test/test_menu.py (stock and alias):
the original's error, the port's code, and the tropical tree sheets' column counts in each image.
Run from the mirror root with SG_PORTSIM_DLL set:  ~/bin/py -3.12 log/roadmap/S3/4c_probe.py"""
import pathlib
import struct
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import test_menu as M  # noqa: E402

lib = L.load()
cases = [(sc, M.LOAD_WORLD_ASSETS, "loadWorldAssets", []) for sc in M.SCEN if sc[2] == 2]
cases += [(sc, M.LOAD_INTERFACE_ART, "loadInterfaceArt", [0]) for sc in M.LIA_SCEN if sc[2] == 2]
for sc, entry, name, args in cases:
    img = M.scen_image(lib, sc)
    alias = M.is_alias(sc)
    r, calls = M.run_original(lib, entry, args, img, alias)
    eax, code, out, pcalls = M.run_port(lib, name, args, img, alias=alias)
    cols_o = struct.unpack("<512i", M.get(lib, r.image, "g_sheetCols", 2048))
    cols_p = struct.unpack("<512i", M.get(lib, out, "g_sheetCols", 2048))
    nz = sum(1 for c in cols_o if c > 0)
    print(f"{sc[0]:34s} original err={r.err!r:58s} port code={code} eax={eax} "
          f"sheets with cols>0 original {nz} port {sum(1 for c in cols_p if c > 0)} equal={cols_o == cols_p} "
          f"calls {len(calls)} diff={M.first_diff(lib, out, r.image, M.MASK)!r}")
