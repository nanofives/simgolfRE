"""38c: oracle run over the generated cases with a classification of what the original did (so a GREEN is not empty)."""
import collections, sys, time
sys.path.insert(0, ".")
from port.test import lib as L, test_reports as TR
lib = L.load()
name = sys.argv[1]; n = int(sys.argv[2]); seed = sys.argv[3] if len(sys.argv) > 3 else 1
t0 = time.time()
cat = collections.Counter(); bad = []
for k, case in enumerate(TR.iter_cases(lib, name, n, seed)):
    img, args, inputs, cur = case
    r = TR.emu(name, img, args, inputs, cur)
    kind = "ok" if not r.err else ("needinput" if r.err.startswith("NeedInput") else r.err[:40])
    d = TR.compare(lib, name, case)
    cat[(tuple(args), kind, "GREEN" if not d else "RED")] += 1
    if d: bad.append((k, args, d))
for k, v in sorted(cat.items(), key=str): print(v, k)
print("bad", bad[:6], f"{time.time()-t0:.0f}s")
