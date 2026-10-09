# Batch c3cf: sound.dll - thirteen methods reached only through a vtable slot or a stored pointer
# (log/c3/c3cf_candidates.tsv): the controller-base list walkers of the 0x1005b59c / 0x1005b5c0 tables, the
# byte stream of the 0x1005b5f0 table, the Wave_Device stream starter of the 0x1005b1c0 table and eight
# sequencer slots of the Sound vtable 0x1005b000. Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA.
# Reimplementations in shim/src/re/c3cf.cpp, fixtures in re/frida/js/fixtures.d/c3cf.js.
#
# Every entry only touches controllers, streams, sequencers, list nodes, track records and sub-objects the
# fixture allocates: no live sound object, device, list, DirectSound interface or MIDI stream is involved,
# nothing is allocated or freed, and no sound.dll module global is read or written. Every vtable the bodies
# dispatch through belongs to a synthetic object; both arms call the same NativeCallback, which records what
# it was handed inside a compared state region.
#
# ret="void" wherever the original leaves a value in eax that is not a return value (CtrlBase_resetElements,
# CtrlBase_flushElements, streamConsumeByte, Seq_setField730Clamped); for those the evidence is entirely in
# the state regions.
#
# HOOKED CALLEES (the verifier must re-run these keys on the main build with the hook off; on
# instances\c3cf only c3cf.cpp is linked, so the runs recorded there already used the true originals):
#   Seq_setField730Clamped                                 -> Seq_applyChannelLevel 0x0002b260 (C3, c3ca)
#   Seq_reset                                              -> Seq_rewind            0x0001e1c0 (C3, c3ar)
#   Seq_getTrackFlagHigh / Seq_getListElementAt /
#   Seq_resetTrackControllers                              -> Seq_listGetAt         0x0001d780 (C3, c3au)
#   the other seven have no callee at all (the leaf rule: each carries at least 12 vectors).

# --- 0x10006b00 startStream: nine streams x two values. s_busy* take the `bit 0 already set` arm (return
# 0xd, nothing written); s_free* take the work arm, which writes +0xcc, +0x58 and +0xc8 and then calls the
# vtable slot +0x7c that records the state word it sees.
_STREAM_OBJS = ["$s_busy0", "$s_busy1", "$s_busy2", "$s_free0", "$s_free1", "$s_free2", "$s_free3",
                "$s_free4", "$s_free5"]
_STREAM = [(o, v) for o in _STREAM_OBJS for v in (0x11110000, -1)]
_STREAM_STATE = [(o, 0, 0x100) for o in _STREAM_OBJS]

# --- 0x1001c2a0 / 0x1001c2f0: fourteen controllers, one vector each (both bodies take no stack argument).
# The vector passes block+0x30 ("$p_*") and the state region is the whole block ("$k_*"), which holds the
# result cell, the list head, the walk cursor, the nodes and every element's call record.
_CTRL_NAMES = ["empty", "null1", "one", "two", "three", "four", "five", "six", "stop1", "stop2", "stop3",
               "stop5", "clear1", "clear2"]
_CTRL = [("$p_%s" % n,) for n in _CTRL_NAMES]
_CTRL_STATE = [("$k_%s" % n, 0, 0x400) for n in _CTRL_NAMES]

# --- 0x1001c370: thirteen controllers x four values. The values stay in 0..0x7f so that the final lookup
# this + 0x5c + value stays inside the 128-byte table the fixture fills.
_MAP_OBJS = ["$m_empty", "$m_nullelem", "$m_t3", "$m_t4", "$m_t5on", "$m_t5off", "$m_t6", "$m_t0",
             "$m_chain", "$m_chain5", "$m_mixed", "$m_null2", "$m_clear"]
_MAP = [(o, v) for o in _MAP_OBJS for v in (0, 1, 7, 0x7F)]
_MAP_STATE = [(o, 0, 0x200) for o in _MAP_OBJS]

# --- 0x1001c7a0 / 0x1001c7e0: twelve streams. streamReadByte's one stack argument is never read by the
# body, so it is given two values to show that; streamConsumeByte takes none.
_BYTE_OBJS = ["$b_mid0", "$b_first", "$b_last", "$b_mid1", "$b_mid2", "$b_mid3", "$b_eq0", "$b_eq1",
              "$b_eq2", "$b_eq3", "$b_over0", "$b_over1"]
_BYTE_READ = [(o, v) for o in _BYTE_OBJS for v in (0, -1)]
_BYTE_CONSUME = [(o,) for o in _BYTE_OBJS]
_BYTE_STATE = [(o, 0, 0x200) for o in _BYTE_OBJS]

# --- 0x10020f50: six sequencers x six values. The values cross both clamp edges (0x7f and 0) and include
# the two extremes of the signed compares.
_LEVEL_OBJS = ["$l_null0", "$l_null1", "$l_null2", "$l_dev0", "$l_dev1", "$l_dev2"]
_LEVEL = [(o, v) for o in _LEVEL_OBJS for v in (-0x80000000, -1, 0, 1, 0x7F, 0x80)]
_LEVEL_STATE = ([(o, off, 0x60) for o in _LEVEL_OBJS for off in (0, 0x700)]
                + [("$l_inner%d" % i, 0, 0x100) for i in range(3)])

# --- 0x10022240: twelve sequencers, one vector each (no stack argument).
_RESET_OBJS = ["$r%d" % i for i in range(12)]
_RESET = [(o,) for o in _RESET_OBJS]
_RESET_STATE = [(o, off, n) for o in _RESET_OBJS
                for off, n in ((0xA0, 0x20), (0x1F0, 0x20), (0x210, 0x10), (0x730, 0x20))]

# --- 0x100227a0: four sequencers x six values; a leaf, 24 vectors.
_F240_OBJS = ["$q0", "$q1", "$q2", "$q3"]
_F240 = [(o, v) for o in _F240_OBJS for v in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x1234)]
_F240_STATE = [(o, 0x230, 0x20) for o in _F240_OBJS]

# --- 0x10023000: nine list shapes x four ids, plus eight ids aimed at a particular node of a particular
# list (the second and third of f_three, the duplicate of f_dup, the element behind the null of f_nullmid,
# the first and last of f_five and both extremes of f_extreme).
_FIND_OBJS = ["$f_inactive", "$f_empty", "$f_nullfirst", "$f_one", "$f_three", "$f_dup", "$f_nullmid",
              "$f_five", "$f_extreme"]
_FIND = ([(o, i) for o in _FIND_OBJS for i in (11, 7, 0, 99)]
         + [("$f_three", 22), ("$f_three", 33), ("$f_dup", 9), ("$f_nullmid", 33), ("$f_five", 1),
            ("$f_five", 5), ("$f_extreme", -1), ("$f_extreme", 0x7FFFFFFF)])
_FIND_STATE = [(o, 0x210, 0x20) for o in _FIND_OBJS]

# --- 0x10023f10: five sequencers whose vtable slot +0xb4 reports 0, 1, 3, 0x105 and 0x00ff0002 tracks.
# Every accepted index also has a node (Seq_listGetAt writes 0 past the end and the body dereferences it);
# the rejected indices are the ones ABOVE the count, which is where `jbe` differs from `jb`. The two counts
# with bits above the low byte are what shows that the returned dword is the count with its low byte
# replaced, not a plain 0 or 1.
_FLAG = [("$g_c0_n2", 0), ("$g_c0_n2", 1), ("$g_c0_n2", 5),
         ("$g_c1_n3", 0), ("$g_c1_n3", 1), ("$g_c1_n3", 2), ("$g_c1_n3", 3),
         ("$g_c3_n5", 0), ("$g_c3_n5", 1), ("$g_c3_n5", 2), ("$g_c3_n5", 3), ("$g_c3_n5", 4),
         ("$g_c3_n5", 100),
         ("$g_c5_n6", 0), ("$g_c5_n6", 1), ("$g_c5_n6", 2), ("$g_c5_n6", 3), ("$g_c5_n6", 4),
         ("$g_c5_n6", 5), ("$g_c5_n6", 0x106),
         ("$g_chi_n3", 0), ("$g_chi_n3", 1), ("$g_chi_n3", 2), ("$g_chi_n3", 0x00FF0003)]
_FLAG_OBJS = ["$g_c0_n2", "$g_c1_n3", "$g_c3_n5", "$g_c5_n6", "$g_chi_n3"]
_FLAG_STATE = [(o, off, n) for o in _FLAG_OBJS for off, n in ((0x1F0, 0x20), (0x300, 0x300))]

# --- 0x100241f0: five list shapes x six indices with the guard at 0, plus the guard-non-zero reject.
_ELEM_OBJS = ["$e_empty", "$e_one", "$e_three", "$e_five", "$e_nullmid"]
_ELEM = ([(o, 0, i) for o in _ELEM_OBJS for i in (-1, 0, 1, 2, 4, 9)]
         + [(o, 1, 0) for o in _ELEM_OBJS])
_ELEM_STATE = [(o, 0x1F0, 0x20) for o in _ELEM_OBJS]

# --- 0x10024630: a five-record list (every record a different set of filled sub-object slots, including
# both ends of the walk 0x254 and 0x58), a one-record list and an empty list; the indices past the end take
# the 0x24 arm.
_SEQCTRL = [("$c_r5", 0), ("$c_r5", 1), ("$c_r5", 2), ("$c_r5", 3), ("$c_r5", 4), ("$c_r5", 5),
            ("$c_r5", 9), ("$c_r1", 0), ("$c_r1", 1), ("$c_r0", 0)]
_SEQCTRL_STATE = [("$ctrls", 0, 12 * 0x40),
                  ("$c_r5_arena", 0, 5 * 0x300), ("$c_r1_arena", 0, 0x300), ("$c_r0_arena", 0, 0x40),
                  ("$c_r5", 0x1F0, 0x20), ("$c_r1", 0x1F0, 0x20), ("$c_r0", 0x1F0, 0x20)]

HOOKS = {
    # 0x10006b00: slot 34 of the 0x1005b1c0 table. Eighteen vectors; six of them take the early 0xd arm.
    "startStream": dict(module="sound.dll", addr=0x00006B00, abi="thiscall", ret="int",
                        args=["pointer", "int"], fixture="c3cf_stream",
                        state=_STREAM_STATE, vectors=_STREAM),
    # 0x1001c2a0: slot 5 of the 0x1005b59c table. Fourteen vectors, one per list shape; the walk cursor, the
    # 0x18 written at this-0x28 and every element's call record are compared.
    "CtrlBase_resetElements": dict(module="sound.dll", addr=0x0001C2A0, abi="thiscall", ret="void",
                                   args=["pointer"], fixture="c3cf_ctrl",
                                   state=_CTRL_STATE, vectors=_CTRL),
    # 0x1001c2f0: slot 6 of the same table, the same walk through vtable slot +0x18 and without the
    # this-0x28 store.
    "CtrlBase_flushElements": dict(module="sound.dll", addr=0x0001C2F0, abi="thiscall", ret="void",
                                   args=["pointer"], fixture="c3cf_ctrl",
                                   state=_CTRL_STATE, vectors=_CTRL),
    # 0x1001c370: slot 1 of the 0x1005b5c0 table. Fifty-two vectors over thirteen list shapes.
    "Ctrl5b5c0_mapValue": dict(module="sound.dll", addr=0x0001C370, abi="thiscall", ret="int",
                               args=["pointer", "int"], fixture="c3cf_map",
                               state=_MAP_STATE, vectors=_MAP),
    # 0x1001c7a0: slot 4 of the 0x1005b5f0 table. Twenty-four vectors; the second argument is never read by
    # the body, which is why the two values of each stream must agree.
    "streamReadByte": dict(module="sound.dll", addr=0x0001C7A0, abi="thiscall", ret="int",
                           args=["pointer", "int"], fixture="c3cf_bytes",
                           state=_BYTE_STATE, vectors=_BYTE_READ),
    # 0x1001c7e0: slot 6 of the same table, the void form; twelve vectors.
    "streamConsumeByte": dict(module="sound.dll", addr=0x0001C7E0, abi="thiscall", ret="void",
                              args=["pointer"], fixture="c3cf_bytes",
                              state=_BYTE_STATE, vectors=_BYTE_CONSUME),
    # 0x10020f50: slot 16 of the Sound vtable. Thirty-six vectors; the callee Seq_applyChannelLevel writes
    # +4 and, for the three sequencers with an inner device, calls its vtable slot +0x40.
    "Seq_setField730Clamped": dict(module="sound.dll", addr=0x00020F50, abi="thiscall", ret="void",
                                   args=["pointer", "int"], fixture="c3cf_seqlevel",
                                   state=_LEVEL_STATE, vectors=_LEVEL),
    # 0x10022240: slot 46 of the Sound vtable. Twelve vectors; the callee Seq_rewind seeds the reader cursor
    # at +0x204 from the head at +0x1fc and clears bits of the reader byte at +0xac.
    "Seq_reset": dict(module="sound.dll", addr=0x00022240, abi="thiscall", ret="int",
                      args=["pointer"], fixture="c3cf_seqreset",
                      state=_RESET_STATE, vectors=_RESET),
    # 0x100227a0: slot 57 of the Sound vtable, a leaf; twenty-four vectors.
    "Seq_setField240": dict(module="sound.dll", addr=0x000227A0, abi="thiscall", ret="int",
                            args=["pointer", "int"], fixture="c3cf_seqfield240",
                            state=_F240_STATE, vectors=_F240),
    # 0x10023000: slot 39 of the Sound vtable, a leaf; forty-four vectors.
    "Seq_findTrackOrZero": dict(module="sound.dll", addr=0x00023000, abi="thiscall", ret="pointer",
                                args=["pointer", "int"], fixture="c3cf_seqfind",
                                state=_FIND_STATE, vectors=_FIND),
    # 0x10023f10: slot 44 of the Sound vtable. Twenty-four vectors over five track counts.
    "Seq_getTrackFlagHigh": dict(module="sound.dll", addr=0x00023F10, abi="thiscall", ret="int",
                                 args=["pointer", "uint32"], fixture="c3cf_seqflag",
                                 state=_FLAG_STATE, vectors=_FLAG),
    # 0x100241f0: slot 70 of the Sound vtable. Thirty-five vectors; the body writes the looked-up element
    # into the caller's own first argument slot, which a call made through the harness cannot observe, so
    # the evidence is the return value and the list cursor at +0x204.
    "Seq_getListElementAt": dict(module="sound.dll", addr=0x000241F0, abi="thiscall", ret="pointer",
                                 args=["pointer", "int", "int"], fixture="c3cf_seqelem",
                                 state=_ELEM_STATE, vectors=_ELEM),
    # 0x10024630: slot 34 of the Sound vtable. Ten vectors; the sub-object arena records the 128-slot walk.
    "Seq_resetTrackControllers": dict(module="sound.dll", addr=0x00024630, abi="thiscall", ret="int",
                                      args=["pointer", "uint32"], fixture="c3cf_seqctrl",
                                      state=_SEQCTRL_STATE, vectors=_SEQCTRL),
}
