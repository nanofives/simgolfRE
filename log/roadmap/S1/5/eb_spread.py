"""S1.5: outcome spread of the EditBox oracle cases (faults, eax values, image changes) per function."""
import collections, os, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4]))
from port.test import lib as L
from port.test import test_chareditor_bio as B
lib = L.load()
for name in B.EB_FUNCS:
    c = collections.Counter()
    for img, args in B.eb_cases(lib, name, 300, 1):
        r, heap = B.eb_emu(lib, name, img, args)
        if r.err:
            c["fault"] += 1
            continue
        c[f"eax={r.eax:#x}" if r.eax < 16 else "eax=other"] += 1
        c["changed" if r.image != img else "unchanged"] += 1
    print(name, dict(c))
