import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4]))
from port.test import lib as L
from port.test import test_chareditor_bio as B
lib = L.load()
case = B.bio_cases(lib, 3, 1)[int(sys.argv[1]) if len(sys.argv) > 1 else 2]
img, args, items, in_bio, cursors, rule, salt = case
pred = B.predict(items, in_bio)
groups = B.item_batches(pred)
print("items", items[:12], "groups", groups[:6])
r = B.bio_emu(lib, img, args, items, groups, cursors, rule, salt)
print("err", r.err, "groups done", r.groups_done)
for e in r.log:
    if e[0] in ("write",) or e[0] in ("uiKey",):
        print(e)
    elif e[0] not in ("display slot", "Text_put", "compositor slot"):
        print("  ", e[0], str(e[1])[:60] if len(e) > 1 else "")
