import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4]))
from port.test import lib as L
from port.test import test_chareditor_bio as B
lib = L.load()
k = int(sys.argv[1])
case = B.bio_cases(lib, k + 1, 1)[k]
img, args, items, in_bio, cursors, rule, salt = case
pred = B.predict(items, in_bio)
groups = B.item_batches(pred)
print("items", list(enumerate(items))[:int(sys.argv[2])])
print("groups", groups[:8])
r = B.bio_emu(lib, img, args, items, groups, cursors, rule, salt)
print("err", r.err, "groups done", r.groups_done)
n = 0
for e in r.log:
    if e[0] == "write" or e[0] in ("Window::create", "Window::ctorInit", "malloc", "pumpMessages"):
        print(e); n += 1
    if n > 120: break
