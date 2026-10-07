# C3 batch c3p (2026-10-07): render-subsystem coordinate math (worldToScreen, screenToTile), the pixel-address helper
# Surface_pixelPtr, the palette chunk decoder Palette::decodeChunk and three small object helpers, reimplemented in
# shim/src/re/c3p.cpp. Format documented at the top of re/frida/hooks_registry.py; fixtures in
# re/frida/js/fixtures.d/c3p.js. The worldToScreen / screenToTile vectors were chosen with the branch model in
# log/c3/c3p_coverage_model.py; which side of every jcc each vector takes is listed in log/c3/c3p_purpose.md.

# --- worldToScreen 0x0042fb90: (wx, wy, out, margin); out 'o' = ($sx, $sy). Variant r1 (rotation 1: no projection
#     block runs) uses pre-filled out pairs: in = (100, 100), xl = (-500, 100), xh = (2000, 100), yl = (100, -500),
#     yh = (100, 2000). screenToTile 0x00430020: (px, py, useWorld). ---
_W2S_R0 = [(12343, 25537, 'o', 64), (16439, 31681, 'o', 0), (26679, 10177, 'o', 64), (1079, 1985, 'o', 0), (1079, 35777, 'o', 64), (10177, 26679, 'o', 64), (10752, 27136, 'o', 64), (11201, 27703, 'o', 64), (11319, 26561, 'o', 64), (13367, 51137, 'o', 0), (27136, 9728, 'o', 64), (21441, 26679, 'o', 0), (24631, 35777, 'o', 0), (16321, 27703, 'o', 64)]
_W2S_R2 = [(12343, 23489, 'o', 64), (28727, 37825, 'o', 0), (1079, 1985, 'o', 0), (1079, 14273, 'o', 0), (10177, 24631, 'o', 64), (10295, 24513, 'o', 64), (10752, 25088, 'o', 64), (11201, 25655, 'o', 64), (11319, 24513, 'o', 64), (14273, 1079, 'o', 0), (27585, 39991, 'o', 0), (20535, 27585, 'o', 0), (24631, 36801, 'o', 0), (15872, 27136, 'o', 0)]
_W2S_R4 = [(12343, 25537, 'o', 64), (11776, 27136, 'o', 0), (27585, 12343, 'o', 0), (1079, 1985, 'o', 0), (1079, 35777, 'o', 64), (10177, 26679, 'o', 64), (11201, 27703, 'o', 64), (11319, 26561, 'o', 64), (13367, 51137, 'o', 0), (14391, 25537, 'o', 0), (27136, 9728, 'o', 64), (20535, 26561, 'o', 64), (25537, 31799, 'o', 64), (15415, 28609, 'o', 64)]
_W2S_R6 = [(12343, 23489, 'o', 0), (36919, 29633, 'o', 0), (12225, 22583, 'o', 64), (1079, 1985, 'o', 0), (1079, 14273, 'o', 0), (10177, 24631, 'o', 64), (10295, 24513, 'o', 64), (10752, 25088, 'o', 64), (11319, 24513, 'o', 64), (14273, 1079, 'o', 0), (27136, 39424, 'o', 0), (32256, 23040, 'o', 0), (13824, 22016, 'o', 0), (23489, 33847, 'o', 0)]
_W2S_R0_W400 = [(12343, 25537, 'o', 64), (15415, 27585, 'o', 0), (34753, 22583, 'o', 0), (1079, 1985, 'o', 0), (1079, 35777, 'o', 64), (12225, 25655, 'o', 64), (12343, 24513, 'o', 64), (13249, 26679, 'o', 64), (13367, 51137, 'o', 0), (14391, 25537, 'o', 0), (25537, 12343, 'o', 64), (35895, 29633, 'o', 64), (31799, 24513, 'o', 0), (23040, 32256, 'o', 0)]
_W2S_R0_W500 = [(14391, 23489, 'o', 64), (20535, 30657, 'o', 64), (29184, 19968, 'o', 0), (1079, 1985, 'o', 0), (1079, 36801, 'o', 64), (12343, 51137, 'o', 0), (13824, 24064, 'o', 64), (14391, 24513, 'o', 64), (14848, 25088, 'o', 64), (15415, 24513, 'o', 0), (24513, 14391, 'o', 64), (23489, 18487, 'o', 64), (27136, 35328, 'o', 64), (26561, 32823, 'o', 0)]
_W2S_R0_DBL = [(12343, 25537, 'o', 64), (14273, 30775, 'o', 64), (18487, 25537, 'o', 64), (21559, 36801, 'o', 0), (26679, 18369, 'o', 0), (1079, 1985, 'o', 0), (1079, 35777, 'o', 64), (10177, 26679, 'o', 64), (10752, 27136, 'o', 64), (13367, 51137, 'o', 0), (27136, 9728, 'o', 64), (21441, 26679, 'o', 0), (24631, 35777, 'o', 0), (16321, 27703, 'o', 64)]
_W2S_R1 = [(1079, 1985, 'in', 0), (1079, 4033, 'in', 0), (1079, 1985, 'xl', 0), (1079, 1985, 'xh', 0), (1079, 1985, 'yl', 0), (1079, 1985, 'yh', 0), (1079, 3009, 'in', 0), (1079, 10177, 'in', 0), (1079, 11201, 'in', 0), (1079, 13249, 'in', 0), (1079, 20417, 'in', 0), (22016, 28160, 'in', 0), (6199, 3009, 'in', 0), (28160, 14848, 'in', 0)]
_STT_R0 = [(-60, 114, 0), (-60, -60, 1), (680, 114, 0), (791, 404, 0), (199, 172, 0), (606, 636, 0), (717, 27, 0), (273, 520, 0), (88, 201, 1), (125, 404, 1), (532, 114, 1), (273, 520, 1), (14, 27, 1), (125, 230, 1)]
_STT_R2 = [(-60, 491, 0), (-60, -60, 1), (680, 114, 0), (791, 404, 0), (199, 172, 0), (606, 636, 0), (717, 27, 0), (273, 520, 0), (88, 201, 1), (125, 404, 1), (532, 114, 1), (273, 520, 1), (14, 27, 1), (125, 230, 1)]
_STT_R4 = [(-60, 114, 0), (-60, -60, 1), (680, 114, 0), (791, 404, 0), (199, 172, 0), (606, 636, 0), (717, 27, 0), (569, 259, 0), (88, 201, 1), (125, 404, 1), (532, 114, 1), (273, 520, 1), (14, 27, 1), (125, 230, 1)]
_STT_R6 = [(-60, 491, 0), (-60, -60, 1), (680, 114, 0), (791, 404, 0), (199, 172, 0), (606, 636, 0), (717, 27, 0), (569, 259, 0), (88, 201, 1), (125, 404, 1), (532, 114, 1), (273, 520, 1), (14, 27, 1), (125, 230, 1)]
_STT_R0_DBL = [(-60, -60, 0), (-60, -60, 1), (-60, 201, 0), (495, 114, 0), (791, 549, 0), (828, 520, 0), (495, 85, 0), (88, 491, 0), (606, 85, 1), (606, 520, 1), (273, 85, 1), (199, 114, 1), (717, 143, 1), (199, 404, 1)]


def _w2s(vs):
    return [(wx, wy, "$sx" if o == "o" else "$sx_" + o, "$sy" if o == "o" else "$sy_" + o, m) for wx, wy, o, m in vs]


def _stt(vs):
    return [(px, py, "$tx", "$ty", use) for px, py, use in vs]


_W2S_ARGS = ["int", "int", "pointer", "pointer", "int"]
_W2S_STATE = [("$sx", 0, 4), ("$sy", 0, 4)]
_W2S_STATE_R1 = [("$sx_" + o, 0, 4) for o in ("in", "xl", "xh", "yl", "yh")] + \
                [("$sy_" + o, 0, 4) for o in ("in", "xl", "xh", "yl", "yh")]
# screenToTile writes *tx / *ty and, through tileToScreen 0x0042f940 (useWorld == 0), the tile caches g_tileSX
# 0x0055eb40 / g_tileSY 0x0055fec8 (2500 shorts each): all are state, so both arms start from the zeroed caches.
_STT_STATE = [("$tx", 0, 4), ("$ty", 0, 4), (0x0055EB40, 0, 5000), (0x0055FEC8, 0, 5000)]

# --- Surface_pixelPtr 0x004796a0: wrappers $w<k> hold a synthetic jgld Surface $s<k> (real class vtable) at +4;
#     $w16b has another bounds rectangle, $wnobits a zero base pointer, $wnull a null surface. The +0x10 virtual
#     bumps a counter at surface+0x4c8 and stores surface+0x4cc, so those 16 bytes of every surface are state. ---
_PIX_XY = [(0, 0), (1, 0), (0, 1), (99, 49), (-3, 7), (37, 21), (100, 0), (0, 50), (150, 60), (-1, -1)]
_PIX = [(w, x, y) for w in ("$w8", "$w16", "$w24", "$w32") for x, y in _PIX_XY]
_PIX += [("$w16b", x, y) for x, y in ((0, 0), (63, 39), (64, 0), (0, 40), (10, 10))]
_PIX += [(w, x, y) for w in ("$w12", "$w20", "$w7", "$w40", "$wnobits") for x, y in ((5, 5), (200, 5), (5, 200))]
_PIX += [("$wnull", 0, 0), ("$wnull", 5, -1), ("$wnull", -1, 0), ("$wnull", -5, 3)]
_PIX_STATE = [("$s" + k, 0x4C0, 0x10) for k in ("8", "16", "24", "32", "16b", "12", "20", "7", "40", "nobits")]

_PAL = [("$t", "$c_empty"), ("$t", "$c_one"), ("$t", "$c_skip"), ("$t", "$c_wrap"), ("$t", "$c_multi"),
        ("$t", "$c_full"), ("$t", "$c_full2"), ("$t", "$c_zero_skip"), ("$tn", "$c_full"), ("$tn", "$c_full2"),
        ("$tz", "$c_one"), ("$tz", "$c_full")]

HOOKS = {}
HOOKS.update({
    # --- three object helpers (a leaf, a constructor, one virtual call), 12 vectors each ---
    "Link482::data": dict(module="golf_clean.exe", addr=0x00482E10, abi="thiscall", ret="pointer", args=["pointer"],
                          fixture="c3p_link", vectors=[("$o%d" % i,) for i in range(12)]),
    "C4837f0::ctor": dict(module="golf_clean.exe", addr=0x004837F0, abi="thiscall", ret="pointer", args=["pointer"],
                          fixture="c3p_ctor", state=[("$obj", 0, 0xC0)], vectors=[("$o%d" % i,) for i in range(12)]),
    "Cache483060::detach": dict(module="golf_clean.exe", addr=0x00483060, abi="thiscall", ret="int",
                                args=["pointer"], fixture="c3p_detach", state=[("$rec", 0, 0x10), ("$obj", 0, 0xC0)],
                                vectors=[("$o%d" % i,) for i in range(12)]),

    # --- pixel address through the jgld Surface virtuals (+0x10 base, +0xd8/+0xdc size, +0xe0 stride, +0xe4 depth) ---
    "Surface_pixelPtr": dict(module="golf_clean.exe", addr=0x004796A0, abi="thiscall", ret="pointer",
                             args=["pointer", "int", "int"], fixture="c3p_surface", state=_PIX_STATE, vectors=_PIX),

    # --- palette chunk decoder: $t has a palette object, $tn a null one (created by the vtable +0 call), $tz a null
    #     this+0x74. $rec records the get/set/create calls and the 0x300-byte array handed to the set virtual ---
    "Palette::decodeChunk": dict(module="golf_clean.exe", addr=0x004826F0, abi="thiscall", ret="int",
                                 args=["pointer", "pointer"], fixture="c3p_pal",
                                 state=[("$rec", 0, 0x320), ("$P", 4, 4), ("$Pn", 4, 4)], vectors=_PAL),

    # --- worldToScreen: one entry per global setting (rotation 0x005685f4, view width 0x00822c8c, flag 0x005a9cc0) ---
    "worldToScreen": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                          fixture="c3p_w2s_r0", state=_W2S_STATE, vectors=_w2s(_W2S_R0)),
    "worldToScreen_r2": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                             fixture="c3p_w2s_r2", state=_W2S_STATE, vectors=_w2s(_W2S_R2)),
    "worldToScreen_r4": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                             fixture="c3p_w2s_r4", state=_W2S_STATE, vectors=_w2s(_W2S_R4)),
    "worldToScreen_r6": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                             fixture="c3p_w2s_r6", state=_W2S_STATE, vectors=_w2s(_W2S_R6)),
    "worldToScreen_w400": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                               fixture="c3p_w2s_r0_w400", state=_W2S_STATE, vectors=_w2s(_W2S_R0_W400)),
    "worldToScreen_w500": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                               fixture="c3p_w2s_r0_w500", state=_W2S_STATE, vectors=_w2s(_W2S_R0_W500)),
    "worldToScreen_dbl": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                              fixture="c3p_w2s_r0_dbl", state=_W2S_STATE, vectors=_w2s(_W2S_R0_DBL)),
    "worldToScreen_r1": dict(module="golf_clean.exe", addr=0x0042FB90, abi="default", ret="int", args=_W2S_ARGS,
                             fixture="c3p_w2s_r1", state=_W2S_STATE_R1, vectors=_w2s(_W2S_R1)),

    # --- screenToTile: one entry per rotation, plus the 0x005a9cc0 flag; useWorld 0 and 1 in each ---
    "screenToTile": dict(module="golf_clean.exe", addr=0x00430020, abi="default", ret="void", args=_W2S_ARGS,
                         fixture="c3p_stt_r0", state=_STT_STATE, vectors=_stt(_STT_R0)),
    "screenToTile_r2": dict(module="golf_clean.exe", addr=0x00430020, abi="default", ret="void", args=_W2S_ARGS,
                            fixture="c3p_stt_r2", state=_STT_STATE, vectors=_stt(_STT_R2)),
    "screenToTile_r4": dict(module="golf_clean.exe", addr=0x00430020, abi="default", ret="void", args=_W2S_ARGS,
                            fixture="c3p_stt_r4", state=_STT_STATE, vectors=_stt(_STT_R4)),
    "screenToTile_r6": dict(module="golf_clean.exe", addr=0x00430020, abi="default", ret="void", args=_W2S_ARGS,
                            fixture="c3p_stt_r6", state=_STT_STATE, vectors=_stt(_STT_R6)),
    "screenToTile_dbl": dict(module="golf_clean.exe", addr=0x00430020, abi="default", ret="void", args=_W2S_ARGS,
                             fixture="c3p_stt_r0_dbl", state=_STT_STATE, vectors=_stt(_STT_R0_DBL)),
})
