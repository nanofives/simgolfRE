import sys
sys.path.insert(0, ".")
from port.test import lib as L, test_reports as TR
lib = L.load()
name, k = sys.argv[1], int(sys.argv[2])
img, args, inputs, cur = TR.cases(lib, name, k + 1, 1)[k]
r = TR.emu(name, img, args, inputs, cur)
print(args, len(inputs), "err:", r.err, "consumed", r.consumed)
for e in (r.log[:int(sys.argv[3])] if len(sys.argv)>3 else r.log[-25:]):
    print(e)
