# C3 batch c3t (2026-10-07): render functions with static callers that the test scenarios never reach: the polygon
# span fillers fillSpanDown / fillSpanUp, the edge stepper polyEdgeAdvance, fill16, mapToScreen456b70, and five
# wrappers forwarding to a drawing object's virtual methods (Widget_drawHLine / VLine, Stream4838f0::puts,
# Surface::clear / copyRaw, Surface_fill). Reimplemented in shim/src/re/c3t.cpp; fixtures in
# re/frida/js/fixtures.d/c3t.js; which side of every jcc each vector takes is listed in log/c3/c3t_purpose.md.

# fillSpanDown / fillSpanUp (x0, x1). c3t_span: clip [4, 40). c3t_span_inv: clip min 30 >= max 10.
_SPAN = [(5, 10), (0, 10), (10, 10), (12, 5), (0, 4), (0, 3), (40, 50), (39, 50), (-100, 100), (4, 40), (5, 6),
         (-0x80000000, 0x7FFFFFFF), (0x7FFFFFFF, -0x80000000), (20, 21), (3, 5)]
_SPAN_INV = [(0, 40), (5, 35), (0, 20), (15, 35), (9, 31), (-5, 100), (10, 40), (0, 30), (29, 31), (-100, -50)]
_SPAN_STATE = [("$buf", 0, 0x200)]

# fill16 (dst, color, count): dst $a = buf+0x40 (dword-aligned), $b = buf+0x42.
_FILL16 = [("$a", 0x1234, c) for c in (0, 1, 2, 3, 4, 7, 8, 15, 30, -1, -2, -3, -5)]
_FILL16 += [("$b", 0xFFFFBEEF, c) for c in (1, 2, 3, 6, 9, -1)]
_FILL16 += [("$a", 0, 5), ("$b", 0xFFFF, 4), ("$a", 0x00010002, 6)]

# mapToScreen456b70 (a, b, outX, outY); the last vector passes the same pointer twice (outY's store wins).
_MAP_AB = [(0, 0), (1, 0), (0, 1), (10, 20), (20, 10), (-5, 7), (100, -100), (0x7FFFFFFF, 1), (-0x80000000, -1),
           (0x15555555, 0x15555556), (3, 3), (49, 0)]
_MAP = [(a, b, "$ox", "$oy") for a, b in _MAP_AB] + [(5, 9, "$ox", "$ox")]
_MAP_ARGS = ["int", "int", "pointer", "pointer"]
_MAP_STATE = [("$out", 0, 8)]

# Widget_drawHLine / Widget_drawVLine (this, a1, a2, a3, a4); w0, w3, w7 have a null drawing object.
_WLINE = [("$w%d" % i, 10 + i, 200 - 3 * i, 7 * i + 1, 0x100 + i * 0x11) for i in range(12)]
_WLINE += [("$w1", 0xFFFFFFFF, 0, 0x80000000, 0), ("$w4", 0, 0, 0, 0xFFFF), ("$w3", 1, 2, 3, 4)]

# Stream4838f0::puts (this, text); p0, p4 have a null stream; 0 = NULL text.
_PUTS = [("$p0", "$t2"), ("$p1", "$t0"), ("$p1", "$t1"), ("$p2", "$t2"), ("$p3", "$t3"), ("$p4", "$t3"),
         ("$p1", 0), ("$p2", "$t4"), ("$p3", "$t1"), ("$p5", "$t2"), ("$p6", "$t3"), ("$p7", 0), ("$p7", "$t4"),
         ("$p0", 0)]

# Surface::clear (a1, a2, holder) / Surface::copyRaw (src, a2, holder); holders h0..h8 have byte counts
# 0, 1, 2, 3, 4, 5, 9, 63, 64 (A x B).
_CLEAR = [(i * 3, -i, "$h%d" % i) for i in range(9)] + [(0, 0, "$h7"), (0x7FFFFFFF, 5, "$h3"), (-1, -1, "$h8")]
_COPY = [("$src%d" % (i & 1), i, "$h%d" % i) for i in range(9)] + [("$src1", 0, "$h8"), ("$src0", 7, "$h7"),
                                                                    ("$src1", -1, "$h5")]
_SURF_STATE = [("$bufs", 0, 9 * 0x40), ("$rec", 0, 0x40)]

# Surface_fill (this, a1, a2, a3, a4, a5); f0, f5 have a null drawing object.
_SFILL = [("$f%d" % i, i * 10, i * 3 - 4, 100 + i, 0xAAAA0000 + i, i * 0x101) for i in range(8)]
_SFILL += [("$f1", -1, 0x7FFFFFFF, -0x80000000, 0, 0), ("$f2", 0, 0, 0, 0xFFFFFFFF, 1), ("$f3", 5, 5, 5, 5, 5),
           ("$f5", 1, 2, 3, 4, 5)]

HOOKS = {
    "fillSpanDown": dict(module="golf_clean.exe", addr=0x00493000, abi="default", ret="void", args=["int", "int"],
                         fixture="c3t_span", state=_SPAN_STATE, vectors=_SPAN),
    "fillSpanDown_inv": dict(module="golf_clean.exe", addr=0x00493000, abi="default", ret="void", args=["int", "int"],
                             fixture="c3t_span_inv", state=_SPAN_STATE, vectors=_SPAN_INV),
    "fillSpanUp": dict(module="golf_clean.exe", addr=0x00493080, abi="default", ret="void", args=["int", "int"],
                       fixture="c3t_span", state=_SPAN_STATE, vectors=_SPAN),
    "fillSpanUp_inv": dict(module="golf_clean.exe", addr=0x00493080, abi="default", ret="void", args=["int", "int"],
                           fixture="c3t_span_inv", state=_SPAN_STATE, vectors=_SPAN_INV),
    "fill16": dict(module="golf_clean.exe", addr=0x00493630, abi="default", ret="void",
                   args=["pointer", "uint32", "int"], fixture="c3t_fill16", state=[("$buf", 0, 0x100)],
                   vectors=_FILL16),
    "mapToScreen456b70": dict(module="golf_clean.exe", addr=0x00456B70, abi="default", ret="void", args=_MAP_ARGS,
                              fixture="c3t_map_f0", state=_MAP_STATE, vectors=_MAP),
    "mapToScreen456b70_f1": dict(module="golf_clean.exe", addr=0x00456B70, abi="default", ret="void", args=_MAP_ARGS,
                                 fixture="c3t_map_f1", state=_MAP_STATE, vectors=_MAP),
    "mapToScreen456b70_fneg": dict(module="golf_clean.exe", addr=0x00456B70, abi="default", ret="void",
                                   args=_MAP_ARGS, fixture="c3t_map_fneg", state=_MAP_STATE, vectors=_MAP),
    "polyEdgeAdvance": dict(module="golf_clean.exe", addr=0x00492FA0, abi="default", ret="int", args=["pointer"],
                            fixture="c3t_poly", state=[("$edges", 0, 14 * 0x24)],
                            vectors=[("$e%d" % i,) for i in range(14)]),
    "Widget_drawHLine": dict(module="golf_clean.exe", addr=0x00478BB0, abi="thiscall", ret="void",
                             args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3t_wline",
                             state=[("$rec", 0, 0x40)], vectors=_WLINE),
    "Widget_drawVLine": dict(module="golf_clean.exe", addr=0x00478BE0, abi="thiscall", ret="void",
                             args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3t_wline",
                             state=[("$rec", 0, 0x40)], vectors=_WLINE),
    "Stream4838f0::puts": dict(module="golf_clean.exe", addr=0x004838F0, abi="thiscall", ret="int",
                               args=["pointer", "pointer"], fixture="c3t_puts", state=[("$rec", 0, 0x40)],
                               vectors=_PUTS),
    "Surface::clear": dict(module="golf_clean.exe", addr=0x00482940, abi="stdcall", ret="int",
                           args=["int", "int", "pointer"], fixture="c3t_surf", state=_SURF_STATE, vectors=_CLEAR),
    "Surface::copyRaw": dict(module="golf_clean.exe", addr=0x00482A80, abi="stdcall", ret="int",
                             args=["pointer", "int", "pointer"], fixture="c3t_surf", state=_SURF_STATE,
                             vectors=_COPY),
    "Surface_fill": dict(module="golf_clean.exe", addr=0x00475DA0, abi="thiscall", ret="int",
                         args=["pointer", "int", "int", "int", "uint32", "int"], fixture="c3t_sfill",
                         state=[("$rec", 0, 0x40)], vectors=_SFILL),
}
