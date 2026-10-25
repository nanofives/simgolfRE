"""S1.5: what the bio oracle cases reach in the original: run outcome, bio changed, final length, groups delivered."""
import collections, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4]))
from port.test import lib as L
from port.test import test_chareditor_bio as B
from port.test.test_commands import objs
lib = L.load()
seed = sys.argv[1] if len(sys.argv) > 1 else "1"
n = int(sys.argv[2]) if len(sys.argv) > 2 else 120
c = collections.Counter()
lens = collections.Counter()
for img, args, items, in_bio, cursors, rule, salt in B.bio_cases(lib, n, seed):
    pred = B.predict(items, in_bio)
    groups = B.item_batches(pred)
    r = B.bio_emu(lib, img, args, items, groups, cursors, rule, salt)
    c["err:" + (r.err.split(":")[0] if r.err else "none")] += 1
    to = objs(lib)["g_golferTypes"][0] + 0x230 * args[0] + 0x30
    before, after = img[to:to + 0x200].split(b"\0")[0], r.image[to:to + 0x200].split(b"\0")[0]
    c["bio changed" if before != after else "bio same"] += 1
    lens[min(len(after), 500) // 100 * 100] += 1
    c["consumed 12/13 cmds"] += sum(1 for cs in pred for x in cs if x.handler in (12, 13))
    c["Window::create calls"] += sum(1 for e in r.log if e[0] == "Window::create")
print(dict(c)); print("final bio length buckets", dict(sorted(lens.items())))
