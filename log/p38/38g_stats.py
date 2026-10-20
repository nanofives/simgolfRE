"""38g: outcome classes of the oracle cases (original side and port side) and which effects the cases reach.
Usage: SG_PORTSIM_DLL=... ~/bin/py -3.12 log/p38/38g_stats.py [N] [seed]"""
import collections, struct, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from port.test import lib as L, test_commands as TC, test_routing as T

n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
seed = sys.argv[2] if len(sys.argv) > 2 else "stats"
lib = L.load()
o = TC.objs(lib)
cl = collections.Counter()
eff = collections.Counter()
bad = []
for k, case in enumerate(T.iter_cases(lib, n, seed)):
    img, args, inputs, cur = case
    r = T.emu(img, args, inputs, cur)
    pe, pimg, code, used = T.port(lib, img, args, inputs, cur)
    cls = "NeedInput" if r.err.startswith("NeedInput") else ("err:" + r.err.split()[0]) if r.err else "ok"
    cl[(cls, code)] += 1
    d = T.compare(lib, case)
    if d:
        bad.append((k, d))
    if not r.err and not (pe is None):
        a = bytearray(img)
        # effects: the mode word, hole records, golfer bytes, walker kind
        def seg(nm, im): off, sz = o[nm]; return bytes(im[off:off + sz])
        for nm, tag in (("g_ui_4d20dc", "mode"), ("g_holes", "holes"), ("g_golfers", "golfers"), ("g_walkers", "walkers"),
                        ("g_tileFlags", "tileflags"), ("g_holePoints", "ways"), ("g_holesPlusOne", "count")):
            if seg(nm, img) != seg(nm, pimg):
                eff[tag] += 1
    gap = "log" if False else ""
print("outcome classes (original, port code):", dict(cl))
print("effects on the finished cases:", dict(eff))
print("differences:", bad)
