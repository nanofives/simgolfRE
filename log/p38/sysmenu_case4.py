"""Print the oracle verdict of every showSystemMenu golden case on the merged build (phase 3.8 check)."""
import sys
sys.path.insert(0, ".")
from port.test import lib as L
from port.test import test_commands as T
lib = L.load()
cmp = T._family("showSystemMenu")[2]
for k, c in enumerate(T._golden_cases(lib, "showSystemMenu")):
    v = cmp(lib, "showSystemMenu", c)
    r = T.emu_widget("showSystemMenu", *c[:2], c[2], c[3], c[4]) if v else None
    print(k, repr(v), [n for n, _ in r.log] if r else "")
