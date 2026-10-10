# C3 batch c3db (round 24, 2026-10-10) of golf_clean.exe: drawing helpers that reach jgld Surfaces, the palette holder
# [0x0083ad0c] and the display object [0x0083ad50]. Reimplementations in shim/src/re/c3db.cpp, fixtures in
# re/frida/js/fixtures.d/c3db.js (every key freezes the game's first thread), Purposes and per-jcc branch tables in
# log/c3/c3db_purpose.md.
#
# Hooked callees from other batches (the verifier re-runs these keys with them off):
#   golf_clean.exe: Widget_drawHLine 0x00478bb0 / Widget_drawVLine 0x00478be0 (c3t), Widget_fillArea 0x00478b50,
#     Surface_pixelPtr 0x004796a0 (c3p), polyEdgeStep 0x00492ed0 (c3x), polyEdgeAdvance 0x00492fa0, fillSpanDown
#     0x00493000, fillSpanUp 0x00493080 (c3t)
#   jgld.dll Surface slots: acquireDC2 8730, releaseDC2 87b0, pixelAddr 8830, call3a a810, call3b a870, fwdVfn18 a750,
#     fwdVfn20 a7b0, getClipRectPtr a990, width aa70, height aab0, getField40 aaf0, getDepthPtr ab30, drawLine 94e0,
#     line8 fb70, line16 b140, hline 103f0, vline 10620, hline16 ba90, vline16 bde0, fillRect16 dcf0, fillRectC8 11dd0;
#     Palette slots +0x18 / +0x1c
# Hooked callees inside this batch: Widget_drawHLineDepth -> Image::hline, drawColorRunA; Widget_drawVLineDepth ->
#   Image::vline, drawColorRunB; Widget_drawBox -> both depth dispatchers; Widget_drawBoxR -> Widget_drawBox;
#   Widget_drawBevelBoxR -> Widget_drawBevelBox; fillWindowRect -> Win_fillRect; fillConvexPoly -> fillConvexPoly8/16.

_ARENA = [("$arena", 0, 0xC000)]
_POLY = _ARENA + [(0x0083D34C, 0, 0x48)]
_LIST = _ARENA + [(0x0083A2D8, 0, 0x40), (0x0083AB94, 0, 4)]
_TV = [(f"$tv{k}", 0x1498, 0x40) for k in range(6)]

_W8 = ["$w0", "$w1", "$w8", "$w10"]           # 8-bit widgets: full clip, inner clip, no base, clip past bounds
_W16 = ["$w2", "$w3", "$w4", "$w9"]           # 16-bit widgets: code 0, code 1 inner clip, code 2, no base

# Horizontal dotted lines (x0, x1, y, colour). No vector clips to a single pixel: the original's counter wraps there.
_H_FULL = [(0, 10, 3, 0x41), (10, 0, 4, 0x42), (5, 5, 5, 0x43), (2, 9, -1, 0x44), (2, 9, 16, 0x45), (2, 9, 15, 0x46),
           (32, 40, 5, 0x47), (-10, -1, 5, 0x48), (-5, 6, 6, 0x49), (20, 50, 7, 0x4A), (-100, 100, 8, 0x1FF),
           (0, 1, 9, 0x4B), (0, 2, 10, 0x4C), (3, 30, 11, -128), (29, 31, 12, 0x80001234)]
_H_INNER = [(0, 10, 1, 0x51), (0, 10, 12, 0x52), (0, 10, 2, 0x53), (10, 30, 11, 0x54), (24, 30, 5, 0x55),
            (0, 3, 5, 0x56), (6, 20, 6, 0x57), (30, 5, 7, 0x58)]
_H_PAST = [(30, 38, 3, 0x61), (33, 38, 3, 0x62), (2, 9, 17, 0x63), (0, 20, 15, 0x64)]
_HLINE = ([("$w0",) + v for v in _H_FULL] + [("$w1",) + v for v in _H_INNER] + [("$w8",) + v for v in _H_FULL[:4]]
          + [("$w10",) + v for v in _H_PAST] + [("$wnull", 0, 10, 3, 0x41), ("$w2", 0, 10, 3, 0x41)])
# Vertical dotted lines (x, y0, y1, colour).
_V_FULL = [(3, 0, 10, 0x41), (4, 10, 0, 0x42), (5, 5, 5, 0x43), (-1, 2, 9, 0x44), (32, 2, 9, 0x45), (31, 2, 9, 0x46),
           (5, 16, 20, 0x47), (5, -10, -1, 0x48), (6, -5, 6, 0x49), (7, 10, 50, 0x4A), (8, -100, 100, 0x1FF),
           (9, 0, 1, 0x4B), (10, 0, 2, 0x4C), (11, 3, 14, -128), (12, 13, 15, 0x80001234)]
_V_INNER = [(1, 0, 10, 0x51), (24, 0, 10, 0x52), (4, 0, 10, 0x53), (23, 5, 30, 0x54), (5, 12, 20, 0x55),
            (5, 0, 1, 0x56), (6, 3, 9, 0x57), (7, 11, 2, 0x58)]
_V_PAST = [(3, 12, 19, 0x61), (3, 16, 19, 0x62), (33, 2, 9, 0x63), (31, 0, 15, 0x64)]
_VLINE = ([("$w0",) + v for v in _V_FULL] + [("$w1",) + v for v in _V_INNER] + [("$w8",) + v for v in _V_FULL[:4]]
          + [("$w10",) + v for v in _V_PAST] + [("$wnull", 3, 0, 10, 0x41), ("$w2", 3, 0, 10, 0x41)])

# 16-bit runs: the same geometry; colours exercise the lookup (low byte index, bit 31 skip, word mask).
_C16 = [0x17, 0x1FF, 0x80001234, 0xABCDE, -1, 0x7F]
_RUNA = ([("$w2",) + v[:3] + (_C16[i % 6],) for i, v in enumerate(_H_FULL)]
         + [("$w3",) + v[:3] + (_C16[i % 6],) for i, v in enumerate(_H_INNER)]
         + [("$w4", 0, 10, 3, 0x17), ("$w4", 0, 10, 4, 0x80000017), ("$w4", 5, 5, 5, 0x17), ("$w4", 0, 10, -1, 0x17)]
         + [("$w9", 0, 10, 3, 0x17), ("$w9", 0, 10, 3, 0x80000017), ("$wnull", 0, 10, 3, 0x17)])
_RUNB = ([("$w2",) + v[:3] + (_C16[i % 6],) for i, v in enumerate(_V_FULL)]
         + [("$w3",) + v[:3] + (_C16[i % 6],) for i, v in enumerate(_V_INNER)]
         + [("$w4", 3, 0, 10, 0x17), ("$w4", 4, 0, 10, 0x80000017), ("$w4", 5, 5, 5, 0x17), ("$w4", -1, 0, 10, 0x17)]
         + [("$w9", 3, 0, 10, 0x17), ("$w9", 3, 0, 10, 0x80000017), ("$wnull", 3, 0, 10, 0x17)])

# Depth dispatchers: 8-bit, 16-bit, 24-bit, 12-bit widgets (a null Surface faults in the original: not offered).
_DISP_W = ["$w0", "$w1", "$w2", "$w3", "$w5", "$w7", "$w4"]
_HDEPTH = [(w,) + v for w in _DISP_W for v in [(0, 10, 3, 0x41), (10, 0, 4, 0x17), (-5, 20, 6, 0x80001234),
                                                  (5, 5, 5, 0x43)]]
_VDEPTH = [(w,) + v for w in _DISP_W for v in [(3, 0, 10, 0x41), (4, 10, 0, 0x17), (6, -5, 13, 0x80001234),
                                                  (5, 5, 5, 0x43)]]

# Boxes (l, t, r, b, colour) inside and across the clip rectangles.
_BOXES = [(2, 2, 12, 9), (5, 3, 20, 14), (0, 0, 31, 15), (-5, -5, 10, 10), (3, 1, 20, 11), (5, 5, 5, 5), (5, 5, 4, 8),
          (10, 4, 26, 13)]
_BOX = [(w,) + b + (c,) for w in ["$w0", "$w1", "$w2", "$w3", "$w5"] for b, c in zip(_BOXES, [0x41, 0x17, 0x80001234,
                                                                                            0x1FF, 0x55, 0x66, 0x77, 0x2A])]
_BOXR = [(w, r, c) for w in ["$w0", "$w1", "$w2", "$w3"] for r, c in [("$r0", 0x41), ("$r1", 0x17), ("$r2", 0x1FF),
                                                                     ("$r4", 0x80001234), (0, 0x41)]]
_BEVEL = [(w,) + b + (c1, c2) for w in ["$w0", "$w1", "$w2", "$w3", "$w5", "$wnull"]
          for b, (c1, c2) in zip(_BOXES, [(0x41, 0x42), (0x17, 0x18), (0x80001234, 0x1FF), (0xF0, 0x0F),
                                          (0x55, 0x66), (0x01, 0x02), (0x77, 0x88), (0x2A, 0x2B)])]
_BEVELR = [(w, r, 0x41, 0x17) for w in ["$w0", "$w1", "$w2", "$w3", "$wnull"]
           for r in ["$r0", "$r1", "$r2", "$r3", "$r4", 0]]

# Fills (x, y, w, h, colour)
_FILLS = [(2, 3, 10, 5, 0x41), (-5, -5, 10, 10, 0x17), (20, 10, 30, 30, 0x1FF), (0, 0, 0, 0, 0x42), (5, 5, -3, 4, 0x43),
          (0, 0, 32, 16, 0x80001234), (40, 40, 5, 5, 0x44), (4, 2, 1, 1, 0x45)]
_FILL = [(w,) + f for w in ["$w0", "$w1", "$w2", "$w3", "$w5", "$wnull"] for f in _FILLS]
_WFILL = [(w,) + f for w in ["$win0", "$win1", "$win2", "$win3"] for f in _FILLS]

# Pixel reads
_PTS = [(0, 0), (5, 3), (31, 15), (32, 0), (0, 16), (10, 7), (17, 9), (-1, 0), (0, -1), (3, 4)]
_GETPIX = [(w,) + p for w in ["$w0", "$w2", "$w5", "$w6", "$w7", "$w8", "$w3"] for p in _PTS] + \
          [("$wnull", 0, 0), ("$wnull", -1, 2)]
_CPTS = [(0, 0, 0x77), (5, 3, 0x1AB), (31, 15, 0x77), (32, 0, 0x5C), (0, 16, 0x5D), (10, 7, 0x77), (17, 9, -2)]
_COMP = [(w,) + p for w in ["$w2", "$w3", "$w5", "$w6", "$w0", "$w7", "$w8", "$w9"] for p in _CPTS] + \
        [("$wnull", 0, 0, 0x33), ("$wnull", 5, 3, 0x34)]

# Polygons (holder, verts, count, colour). Holders are the widgets (Surface at +4).
_POLYS = [("$v_tri", 3), ("$v_quad", 4), ("$v_out", 3), ("$v_flat", 3), ("$v_pent", 5), ("$v_diam", 4),
          ("$v_thin", 4), ("$v_tri", 0), ("$v_tri", -1), ("$v_tri", 2)]
_P8 = ([(w, v, n, c) for w in ["$w0", "$w1"] for (v, n), c in zip(_POLYS, [0x41, 0x1A5, 0x43, 0x44, -1, 0x46, 0x47,
                                                                          0x48, 0x49, 0x4A])]
       + [("$w8", "$v_tri", 3, 0x41), ("$wnull", "$v_tri", 3, 0x41), ("$w0", 0, 3, 0x41), (0, "$v_tri", 3, 0x41)])
_P16 = ([(w, v, n, c) for w in ["$w2", "$w3"] for (v, n), c in zip(_POLYS, [0x17, 0x80001234, 0x1FF, 0x44, -1, 0x7F,
                                                                           0xABCDE, 0x18, 0x19, 0x1A])]
        + [("$w4", "$v_tri", 3, 0x17), ("$w4", "$v_tri", 3, 0x80000017), ("$w9", "$v_tri", 3, 0x17),
           ("$wnull", "$v_tri", 3, 0x17), ("$w2", 0, 3, 0x17), (0, "$v_tri", 3, 0x17)])
_P8 += [(w, v, n, c) for w in ["$w0", "$w1"] for v, n, c in [("$v_below", 3, 0x4D), ("$v_hook", 4, 0x4E)]]
_P16 += [(w, v, n, c) for w in ["$w2", "$w3"] for v, n, c in [("$v_below", 3, 0x1B), ("$v_hook", 4, 0x80001C1C)]]
# 0x0083d350 = 1 (fixture c3db_g350): the two polyEdgeStep failures, plus ordinary polygons (which store it)
_STALE = [("$v_low", 3), ("$v_low2", 3), ("$v_tri", 3), ("$v_hook", 4), ("$v_quad", 4)]
_P8S = [(w, v, n, 0x50 + i) for w in ["$w0", "$w1"] for i, (v, n) in enumerate(_STALE)]
_P16S = [(w, v, n, 0x20 + i) for w in ["$w2", "$w3"] for i, (v, n) in enumerate(_STALE)]
_PDISP = [(w, v, n, c) for w in ["$w0", "$w1", "$w2", "$w3", "$w5", "$w7"]
          for (v, n), c in zip(_POLYS[:5], [0x41, 0x17, 0x80001234, 0x1FF, -1])]

# listRemove(obj): every array object, a stranger, NULL, an address that is no object.
_REMOVE = [("$o0",), ("$o1",), ("$o2",), ("$o3",), ("$o4",), ("$o5",), ("$o6",), ("$o7",), ("$ostranger",), (0,),
           (0x5A5A0003,)]

_LS = [0, 1, 2, 3, -1, -2, -3, 7, 0x55555556, 0x7FFFFFFF, -0x80000000, 1000]
_LINESP = [(f"$tv{k}", v) for k in range(6) for v in _LS]


def _t(addr, args, vectors, fixture, ret="void", state=_ARENA, abi="thiscall"):
    return dict(module="golf_clean.exe", addr=addr, abi=abi, ret=ret, args=args, fixture=fixture, state=state,
                vectors=vectors)


_A4 = ["pointer", "int", "int", "int", "int"]
_A5 = ["pointer", "int", "int", "int", "int", "int"]

HOOKS = {
    "Image::hline": _t(0x00485D40, _A4, _HLINE, "c3db_a"),
    "Image::vline": _t(0x00485E80, _A4, _VLINE, "c3db_a"),
    "drawColorRunA": _t(0x00494D30, _A4, _RUNA, "c3db_a"),
    "drawColorRunA_nopal": _t(0x00494D30, _A4, _RUNA, "c3db_b"),
    "drawColorRunB": _t(0x00494F00, _A4, _RUNB, "c3db_a"),
    "drawColorRunB_nopal": _t(0x00494F00, _A4, _RUNB, "c3db_b"),
    "Widget_drawHLineDepth": _t(0x00478C10, _A4, _HDEPTH, "c3db_a"),
    "Widget_drawHLineDepth_nopal": _t(0x00478C10, _A4, _HDEPTH, "c3db_b"),
    "Widget_drawVLineDepth": _t(0x00478C70, _A4, _VDEPTH, "c3db_a"),
    "Widget_drawVLineDepth_nopal": _t(0x00478C70, _A4, _VDEPTH, "c3db_b"),
    "Widget_drawBox": _t(0x00479610, _A5, _BOX, "c3db_a"),
    "Widget_drawBoxR": _t(0x00479670, ["pointer", "pointer", "int"], _BOXR, "c3db_a", ret="int"),
    "Widget_drawBevelBox": _t(0x00479560, _A5 + ["int"], _BEVEL, "c3db_a"),
    "Widget_drawBevelBoxR": _t(0x004795D0, ["pointer", "pointer", "int", "int"], _BEVELR, "c3db_a", ret="int"),
    "Win_fillRect": _t(0x00478AF0, _A5, _FILL, "c3db_a", ret="int"),
    "fillWindowRect": _t(0x00480B00, _A5, _WFILL, "c3db_a", ret="int"),
    "drawLineH": _t(0x00478DF0, ["pointer", "int", "int"], _GETPIX, "c3db_a", ret="uint32"),
    "blendRed": _t(0x00479830, ["pointer", "int", "int", "int"], _COMP, "c3db_a", ret="uint8"),
    "blendRed_fmt0": _t(0x00479830, ["pointer", "int", "int", "int"], _COMP, "c3db_b", ret="uint8"),
    "blendRed_fmt2": _t(0x00479830, ["pointer", "int", "int", "int"], _COMP, "c3db_c", ret="uint8"),
    "blendGreen": _t(0x004798C0, ["pointer", "int", "int", "int"], _COMP, "c3db_a", ret="uint8"),
    "blendGreen_fmt0": _t(0x004798C0, ["pointer", "int", "int", "int"], _COMP, "c3db_b", ret="uint8"),
    "blendGreen_fmt2": _t(0x004798C0, ["pointer", "int", "int", "int"], _COMP, "c3db_c", ret="uint8"),
    "drawPixelRun": _t(0x00479950, ["pointer", "int", "int", "int"], _COMP, "c3db_a", ret="uint8"),
    "fillConvexPoly8": _t(0x00493100, ["pointer", "pointer", "int", "int"], _P8, "c3db_a", ret="int", state=_POLY,
                          abi="default"),
    "fillConvexPoly16": _t(0x004932D0, ["pointer", "pointer", "int", "int"], _P16, "c3db_a", ret="int", state=_POLY,
                           abi="default"),
    "fillConvexPoly16_nopal": _t(0x004932D0, ["pointer", "pointer", "int", "int"], _P16, "c3db_b", ret="int",
                                 state=_POLY, abi="default"),
    "fillConvexPoly8_stale": _t(0x00493100, ["pointer", "pointer", "int", "int"], _P8S, "c3db_g350", ret="int",
                                state=_POLY, abi="default"),
    "fillConvexPoly16_stale": _t(0x004932D0, ["pointer", "pointer", "int", "int"], _P16S, "c3db_g350", ret="int",
                                 state=_POLY, abi="default"),
    "fillConvexPoly": _t(0x00493520, ["pointer", "pointer", "int", "int"], _PDISP, "c3db_a", ret="int", state=_POLY,
                         abi="default"),
    "listRemove": _t(0x0047E520, ["pointer"], _REMOVE, "c3db_a", state=_LIST, abi="default"),
    "listRemove_dup": _t(0x0047E520, ["pointer"], _REMOVE, "c3db_b", state=_LIST, abi="default"),
    "listRemove_empty": _t(0x0047E520, ["pointer"], _REMOVE, "c3db_c", state=_LIST, abi="default"),
    "listRemove_neg": _t(0x0047E520, ["pointer"], _REMOVE, "c3db_n", state=_LIST, abi="default"),
    "listRemove_one": _t(0x0047E520, ["pointer"], _REMOVE, "c3db_one", state=_LIST, abi="default"),
    "TextView::setLineSpacing": _t(0x0048E0B0, ["pointer", "int"], _LINESP, "c3db_a", state=_ARENA + _TV),
    "TextView::setLineSpacing_narrow": _t(0x0048E0B0, ["pointer", "int"], _LINESP, "c3db_b", state=_ARENA + _TV),
    "TextView::setLineSpacing_wide": _t(0x0048E0B0, ["pointer", "int"], _LINESP, "c3db_c", state=_ARENA + _TV),
    "TextView::setLineSpacing_nodisp": _t(0x0048E0B0, ["pointer", "int"], _LINESP, "c3db_n", state=_ARENA + _TV),
}
