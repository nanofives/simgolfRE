# C3 batch c3be (round 14, 2026-10-08) of golf_clean.exe: seven IJG libjpeg 6a compress helpers and four engine
# forwarders that dispatch through one object pointer. Reimplementations in shim/src/re/c3be.cpp, fixtures in
# re/frida/js/fixtures.d/c3be.js, per-jcc branch coverage in log/c3/c3be_purpose.md.
#
# Hooked callees (the verifier should re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original
# callees; none of them is in this batch):
#   add_huff_table                    -> 004afad0 (jpeg_alloc_huff_table, c3af)
#   emit_jfif_app0 / emit_adobe_app14 -> 004afbb0, 004afbf0, 004afc10 (emit_byte/marker/2bytes, c3af)
#   Surface_box                       -> 00475b00 (Widget_fillRectR, c3ab)
# flush_bits' callee dump_buffer 0x004b19e0 (jchuff) is not hooked by any batch.

_DEST_STATE = [("$buf", 0, 0x40), ("$dest", 0, 8)]


def _ws(fixture, n):
    """State of a flush_bits key: the n working_state objects plus the shared output buffer and manager."""
    return dict(module="golf_clean.exe", addr=0x004B1B70, abi="default", ret="int", args=["pointer"],
                fixture=fixture, state=[(f"$w{i}", 0, 0x24) for i in range(n)] + _DEST_STATE,
                vectors=[(f"$w{i}",) for i in range(n)])


def _phuff(fixture):
    """State of a dump_buffer_4b2420 key: the twelve entropy objects, their managers and their error managers."""
    return dict(module="golf_clean.exe", addr=0x004B2420, abi="default", ret="void", args=["pointer"],
                fixture=fixture,
                state=[(f"$e{i}", 0, 0x30) for i in range(12)] + [(f"$d{i}", 0, 8) for i in range(12)] +
                      [(f"$x{i}", 0, 0x30) for i in range(12)],
                vectors=[(f"$e{i}",) for i in range(12)])


def _huff(fixture):
    """State of an add_huff_table key: the six htblptr cells and the six tables."""
    return dict(module="golf_clean.exe", addr=0x004AE8B0, abi="default", ret="void",
                args=["pointer", "pointer", "pointer", "pointer"], fixture=fixture,
                state=[(f"$hp{i}", 0, 4) for i in range(6)] + [(f"$tb{i}", 0, 0x118) for i in range(6)],
                vectors=[(f"$c{i}", f"$hp{i}", f"$b{i}", f"$v{i}") for i in range(6)])


HOOKS = {
    # jpeg_std_error: fills a jpeg_error_mgr with constants and returns it. Leaf: twelve 0xAA-filled managers
    # (twelve distinct return pointers, each a state change; the untouched msg_parm union is compared too).
    "jpeg_std_error": dict(module="golf_clean.exe", addr=0x004AEC80, abi="default", ret="pointer",
                           args=["pointer"], fixture="c3be_err",
                           state=[(f"$e{i}", 0, 0x84) for i in range(12)],
                           vectors=[(f"$e{i}",) for i in range(12)]),

    # start_iMCU_row: thirteen cinfos covering both sides of the signed compare at 0x004b0878 and of the
    # unsigned compare at 0x004b088c (including total_iMCU_rows 0 and 0x80000000). Leaf, 13 vectors.
    "start_iMCU_row": dict(module="golf_clean.exe", addr=0x004B0860, abi="default", ret="void",
                           args=["pointer"], fixture="c3be_coef",
                           state=[(f"$k{i}", 0, 0x20) for i in range(13)],
                           vectors=[(f"$c{i}",) for i in range(13)]),

    # add_huff_table: *htblptr non-NULL (0x004ae8ba jne taken) and *htblptr NULL (the jpeg_alloc_huff_table
    # call at 0x004ae8c1 plus the store at 0x004ae8c9); our alloc_small hands both arms the same buffer.
    "add_huff_table": _huff("c3be_huff_preset"),
    "add_huff_table_alloc": _huff("c3be_huff_alloc"),

    # emit_jfif_app0: twelve density_unit / X_density / Y_density combinations; no conditional branch.
    "emit_jfif_app0": dict(module="golf_clean.exe", addr=0x004AFC80, abi="default", ret="void",
                           args=["pointer"], fixture="c3be_jfif", state=_DEST_STATE,
                           vectors=[(f"$c{i}",) for i in range(12)]),

    # emit_adobe_app14: jpeg_color_space 0..7, -1 and 100 -> the transform byte 1 (space 3, 0x004afd84),
    # 2 (space 5, 0x004afd89) and 0 (every other value, 0x004afd8b).
    "emit_adobe_app14": dict(module="golf_clean.exe", addr=0x004AFD20, abi="default", ret="void",
                             args=["pointer"], fixture="c3be_adobe", state=_DEST_STATE,
                             vectors=[(f"$c{i}",) for i in range(10)]),

    # flush_bits: one key per branch group of the inlined emit_bits.
    #   _ws        free_in_buffer 0x40: the loop entry 0x004b1b98 on both sides and the 0xFF stuffing 0x004b1bc5
    #   _full      free_in_buffer 1: dump_buffer after the first byte (0x004b1bb0 not taken), success
    #   _stuff     free_in_buffer 2 with an emitted 0xFF: dump_buffer after the stuffed zero (0x004b1bd8)
    #   _fail      the same as _full with empty_output_buffer returning FALSE (0x004b1bbd taken, returns 0)
    #   _fail2     the same as _stuff with empty_output_buffer returning FALSE (0x004b1be5 taken, returns 0)
    #   _loop      put_bits >= 9: the back edge 0x004b1bf4 taken (two to four bytes per call)
    "flush_bits": _ws("c3be_ws", 12),
    "flush_bits_full": _ws("c3be_ws_full", 6),
    "flush_bits_stuff": _ws("c3be_ws_stuff", 4),
    "flush_bits_fail": _ws("c3be_ws_fail", 4),
    "flush_bits_fail2": _ws("c3be_ws_fail2", 4),
    "flush_bits_loop": _ws("c3be_ws_loop", 8),

    # dump_buffer_4b2420: twelve entropy objects whose managers carry different next_output_byte /
    # free_in_buffer, so the copies back to entropy+0x10/+0x14 differ per vector. Leaf, 24 vectors over the two
    # keys: empty_output_buffer TRUE (0x004b2435 taken) and FALSE (the ERREXIT at 0x004b243c/0x004b2449).
    "dump_buffer_4b2420": _phuff("c3be_phuff"),
    "dump_buffer_4b2420_err": _phuff("c3be_phuff_err"),

    # netNameOf: ids of slots 0..15 (slot 15 repeats slot 1's id: the first match wins) plus five ids no slot
    # holds (the 0x0049aa50 NULL return). Leaf, 20 vectors, 16 distinct return values.
    "netNameOf": dict(module="golf_clean.exe", addr=0x0049AA30, abi="thiscall", ret="pointer",
                      args=["pointer", "int"], fixture="c3be_net",
                      vectors=[("$obj", i) for i in (10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130,
                                                     140, 150, 0, -1, 15, 1000, 0x7FFFFFFF)]),

    # Surface_drawRegion: 0x18 (this+8 set), 0x10 (NULL source), 7 (either device NULL) and the device call
    # through vtable slot +0x5c, with and without a mask surface. Leaf, 14 vectors.
    "Surface_drawRegion": dict(module="golf_clean.exe", addr=0x00473DF0, abi="thiscall", ret="int",
                               args=["pointer", "pointer", "int", "int", "pointer"], fixture="c3be_surf",
                               state=[("$rec", 0, 0x20)],
                               vectors=[("$t_ok", "$s_ok", 0, 0, 0),
                                        ("$t_ok", "$s_ok", 3, 5, 0),
                                        ("$t_ok", "$s_ok", -3, -5, "$m_ok"),
                                        ("$t_ok2", "$s_ok2", 100, 200, "$m_ok"),
                                        ("$t_ok2", "$s_ok", 0x7FFF, -0x8000, 0),
                                        ("$t_ok", "$s_ok", 1000000, -1000000, "$m_ok"),
                                        ("$t_ok", "$s_nodev", 1, 2, 0),
                                        ("$t_ok2", "$s_nodev", 5, 6, "$m_ok"),
                                        ("$t_nodev", "$s_ok", 1, 2, 0),
                                        ("$t_busy", "$s_ok", 1, 2, 0),
                                        ("$t_busy2", 0, 1, 2, 0),
                                        ("$t_busy", 0, 0, 0, "$m_ok"),
                                        ("$t_ok", 0, 1, 2, 0),
                                        ("$t_nodev", 0, 0, 0, 0)]),

    # DC_call84: 7 (device NULL), 0x10 (handle NULL) and the device call through vtable slot +0x84 with the
    # handle payload h+4. Leaf, 12 vectors.
    "DC_call84": dict(module="golf_clean.exe", addr=0x00475B60, abi="thiscall", ret="int",
                      args=["pointer", "pointer", "int", "int"], fixture="c3be_dc",
                      state=[("$rec", 0, 0x20)],
                      vectors=[("$d_ok", "$h0", 0, 0),
                               ("$d_ok", "$h0", 1, 2),
                               ("$d_ok", "$h1", -1, -2),
                               ("$d_ok", "$h2", 100, 200),
                               ("$d_ok", "$h3", 0x7FFFFFFF, 1),
                               ("$d_ok", "$h4", 0x12345678, -0x7FFFFFFF),
                               ("$d_ok2", "$h0", 5, 6),
                               ("$d_ok2", "$h1", -5, -6),
                               ("$d_ok2", "$h4", 0, -1),
                               ("$d_nodev", "$h0", 1, 2),
                               ("$d_nodev", 0, 0, 0),
                               ("$d_ok", 0, 1, 2)]),

    # Surface_box: the RECT built on the stack reaches vtable slot +0x34 through 0x00475b00, which returns 0x18
    # for a NULL device. Eleven vectors (the callee 0x00475b00 is hooked by c3ab).
    "Surface_box": dict(module="golf_clean.exe", addr=0x00475B20, abi="thiscall", ret="int",
                        args=["pointer", "int", "int", "int", "int"], fixture="c3be_box",
                        state=[("$rec", 0, 0x20)],
                        vectors=[("$b_ok", 0, 0, 0, 0),
                                 ("$b_ok", 1, 2, 3, 4),
                                 ("$b_ok", -1, -2, 3, 4),
                                 ("$b_ok", 10, 20, -30, -40),
                                 ("$b_ok", 100, 200, 300, 400),
                                 ("$b_ok", -1000, 0, 0, 1000),
                                 ("$b_ok", 0x7FFF, 0x7FFF, 1, 1),
                                 ("$b_ok", 5, 5, 0, 0),
                                 ("$b_nodev", 1, 2, 3, 4),
                                 ("$b_nodev", 0, 0, 0, 0),
                                 ("$b_ok", -7, 9, 11, -13)]),
}

# ---------------------------------------------------------------------------------------------------------------
# Two build-palette hit tests added after the first A/B round: pure functions of the cursor position whose only
# callee is approxDistance 0x00467170 (C3, shim/src/re/golf_math.cpp, linked into every build), so no fixture and
# no state region. The vectors are the anchor points of each arm, points a few pixels off them, and points far
# from every anchor. Re-run with SIMGOLF_HOOKS_OFF=00467170 so the original arm uses the original callee.
HOOKS.update({
    # hitBuildingTool: -2 and -3 for the two fixed anchors, 0..8 for the nine palette cells, -1 for a miss.
    # 24 vectors, 12 distinct results.
    "hitBuildingTool": dict(module="golf_clean.exe", addr=0x004340A0, abi="default", ret="int",
                            args=["int", "int"],
                            vectors=[(0xEC, 0x20E), (0xF0, 0x210), (0x115, 0x237), (0x118, 0x23A),
                                     (0x17F, 0x213), (0x1CF, 0x213), (0x21F, 0x213), (0x26F, 0x213),
                                     (0x2BF, 0x213), (0x1A7, 0x23B), (0x1F7, 0x23B), (0x247, 0x23B),
                                     (0x297, 0x23B), (0x17F, 0x214), (0x2BF, 0x214), (0x297, 0x23C),
                                     (0x1A0, 0x213), (0, 0), (0x400, 0x400), (1000, 2000), (-100, -200),
                                     (0x300, 0x300), (0x7FFF, 0x7FFF), (-0x8000, 0x100)]),
    # hitBuildTool433c60: -2, 0, 1, 2, 3 for the five anchors in that order of precedence, -3 for the last one
    # (which overrides every earlier hit) and -1 for a miss. 20 vectors, 7 distinct results.
    "hitBuildTool433c60": dict(module="golf_clean.exe", addr=0x00433C60, abi="default", ret="int",
                               args=["int", "int"],
                               vectors=[(0x10E, 0x1F3), (0x160, 0x223), (0x1D3, 0x223), (0x24B, 0x223),
                                        (0x2CB, 0x223), (0x10B, 0x231), (0x112, 0x1F6), (0x165, 0x226),
                                        (0x1D8, 0x226), (0x250, 0x226), (0x2D0, 0x225), (0x10E, 0x234),
                                        (0x200, 0x223), (0x290, 0x223), (0x120, 0x200), (0, 0),
                                        (0x400, 0x400), (1000, 2000), (-100, -200), (0x7FFF, 0x7FFF)]),
})
