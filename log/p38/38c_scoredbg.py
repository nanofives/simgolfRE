import sys, struct
sys.path.insert(0, ".")
from port.test import lib as L, test_reports as TR, test_commands as TC
lib = L.load()
img, args, inputs, cur = next(TR.iter_cases(lib, "showCourseReport", 1, "x"))
o = TC.objs(lib)
off = o["g_holes"][0]
def rd(fmt, at): return struct.unpack_from(fmt, img, at)[0]
print("hp1", rd("<i", o["g_holesPlusOne"][0]), "diff", rd("<i", o["g_difficulty"][0]), "mask", hex(rd("<I", o["g_buildingKindMask"][0])))
for h in (1, 2, 11, 18):
    r = off + 0x208 * h
    print(h, "par", rd("<b", r), "len", rd("<h", r+4), "r20", rd("<i", r+0x20), "e4", rd("<i", r+0x24), "158", rd("<h", r+0x158), "1ec", rd("<i", r+0x1ec),
          "1fc", rd("<i", r+0x1fc), "ee/104/110", rd("<h", r+0xee), rd("<h", r+0x104), rd("<h", r+0x110), "cell(6,9)", rd("<h", r+0x28+2*(11*6+9)))
for mode in (2,):
    pe, pimg, code, used = TR.port(lib, "showCourseReport", img, [mode], [], (0, 0))
    print("mode", mode, "code", code, "score", struct.unpack_from("<i", pimg, o["g_courseReportScore"][0])[0],
          "flags", hex(struct.unpack_from("<I", pimg, o["g_flags"][0])[0]), "before", hex(rd("<I", o["g_flags"][0])))
# python mirror of the grading arithmetic
def tdiv(a, b): 
    q = abs(a) // abs(b); return q if (a < 0) == (b < 0) else -q
thr = 0x32 if rd("<i", o["g_difficulty"][0]) else 0x19
S = [0]*13; cA = cB = cC = 0; many = 0; nh = 0
f40 = rd("<I", o["g_flags"][0]) & 0x40
for h in range(1, 19):
    r = off + 0x208 * h; par = rd("<b", r)
    if par == 0: continue
    R94 = [8]*8; Rb8 = [par*8]*8; cnt = 0; wsum = 0
    for b in range(1, 8):
        for rr in range(8):
            for j in range(b, (9 if b == 7 else b) + 1):
                v = rd("<h", r + 0x28 + 2*(11*rr + j)); cnt += v; wsum += v*j; R94[rr] += v; Rb8[rr] += v*j
    nh += 1
    if not (tdiv(rd("<h", r+0x104), 2) + rd("<h", r+0x110) + rd("<h", r+0xee) < 8): many += 1
    S[0] += rd("<h", r+4)
    r20 = rd("<i", r+0x20)
    S[3] += 0 if r20 == 0 else tdiv(tdiv(rd("<i", r+0x1ec), r20), 40)
    S[4] += 0 if r20 == 0 else tdiv(rd("<h", r+0x158)*100, tdiv(rd("<i", r+0x24), 2) + r20 + 4)
    ref = tdiv(Rb8[7]*100, R94[7]); d = {}
    for name, rr in (("A", 6), ("B", 5), ("C", 3)):
        d[name] = tdiv(Rb8[rr]*100, R94[rr]) - ref
    if d["A"] >= thr: cA += 1
    if d["B"] >= thr: cB += 1
    if d["C"] >= thr: cC += 1
    if rd("<i", r+0x1fc) < 2: S[8] += 1
print("S0", S[0], "S3", S[3], "S4", S[4], "S8", S[8], "many", many, "cA/B/C", cA, cB, cC, "thr", thr, "f40", f40)
