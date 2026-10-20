"""branch coverage of the oracle cases per function (both sides of each jcc of the function's own ranges)"""
import os, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[3]; sys.path.insert(0, str(ROOT))
from port.test import lib as L, test_commands as T, cmdemu as CE
lib = L.load(os.environ["SG_PORTSIM_DLL"], rebuild=False)
names = sys.argv[1:] or T.PANELS
for name in names:
    cover = {}
    for img, args, inputs in T.panel_cases(lib, name, 500, 1):
        emu = CE.Emu()
        for a, n, pops, eax in T.DROPPED: emu.stub(a, n, eax, pops)
        for a, (nm, n) in T.GAP_PEERS.items(): emu.stub(a, n, 0, False, nm)
        emu.run(T.ENTRY[name], args, img, inputs=inputs, cover=cover)
    n, both, missing = CE.coverage_summary(cover)
    print(f"{name}\t{n}\t{both}\t{' '.join(f'{a}:{s}' for a, s in missing)}")
