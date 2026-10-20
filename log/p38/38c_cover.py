"""38c: per-jcc branch coverage (both sides of every jcc in the original's own ranges) of the generated oracle cases"""
import sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]; sys.path.insert(0, str(ROOT))
from port.test import lib as L, test_reports as TR, cmdemu as CE
lib = L.load()
name = sys.argv[1]; n = int(sys.argv[2]); seed = sys.argv[3] if len(sys.argv) > 3 else "cover"
cover = {}
for case in TR.iter_cases(lib, name, n, seed):
    img, args, inputs, cur = case
    TR.emu(name, img, args, inputs, cur, cover=cover)
sites, both, missing = CE.coverage_summary(cover)
print(f"{name}\t{sites}\t{both}\t" + " ".join(f"{a}:{s}" for a, s in missing))
