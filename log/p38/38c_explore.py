"""38c: run the port over the generated cases and print what they reach (score buckets, errors). Port only."""
import collections, os, struct, sys
sys.path.insert(0, ".")
from port.test import lib as L, test_reports as TR, test_commands as TC
lib = L.load()
name = sys.argv[1]
n = int(sys.argv[2]) if len(sys.argv) > 2 else 300
cs = TR.cases(lib, name, n, sys.argv[3] if len(sys.argv) > 3 else "explore")
o = TC.objs(lib)
def rd(img, nm, at=0):
    return struct.unpack_from("<i", img, o[nm][0] + at)[0]
hist = collections.Counter(); scores=[]
for img, args, inputs, cur in cs:
    b = bytearray(img); struct.pack_into("<i", b, o["g_courseReportScore"][0], -777); img = bytes(b)
    pe, pimg, code, used = TR.port(lib, name, img, args, inputs, cur)
    if code == -9 and "-v" in sys.argv:
        try:
            lib.state
        except Exception: pass
        print("code -9", args, len(inputs))
    mode = args[0] if args else None
    key = (mode, code)
    if name == "showCourseReport" and mode in (1, 2) and code == 0:
        sc = rd(pimg, "g_courseReportScore")
        key += ("changed" if sc != -777 else "same", "score=100" if sc == 100 else ("score>=90" if sc >= 90 else ("score>0" if sc>0 else "")))
        if sc != -777: scores.append(sc)
        key += (("flagsrep",) if (rd(pimg, "g_flags") != rd(img, "g_flags")) else ())
    hist[key] += 1
for k, v in sorted(hist.items(), key=str):
    print(v, k)

print(sorted(scores))
