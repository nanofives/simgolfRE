# C3 batch c3bx (round 18, 2026-10-08) of golf_clean.exe: eleven functions that dispatch through one pointer each.
# Reimplementations in shim/src/re/c3bx.cpp, fixtures in re/frida/js/fixtures.d/c3bx.js, Purposes and per-jcc
# branch coverage in log/c3/c3bx_purpose.md.
#
# Hooked callees (the verifier should re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original
# callees; none of them is in this batch):
#   jpeg_set_quality                 -> 004ae700 jpeg_quality_scaling (C3, c3r) and, under
#                                       004ae6c0 jpeg_set_linear_quality (C2, not hooked),
#                                       004ae600 jpeg_add_quant_table (C3, c3be) and
#                                       004afab0 jpeg_alloc_quant_table (C3, c3af)
#   Window_fwdScroll26c / _fwdScroll270 -> 004967f0 setWidgetScalar (C3)
#   Window_fwdRange26c  / _fwdRange270  -> under 00496770 orderPair (C2, not hooked), 00493580 swapInts (C3)
# The vtable slots the other seven dispatch through (child +0x14, this +8, sub +0xd8, widget +0x120, dest +0xc)
# and the two global hook pointers 0x0083af80 / 0x0083af88 are the fixture's own callbacks, not hooks.

# The two close-hook globals are state regions so diff_hook restores them between the arms and after the run.
_HOOK_A = (0x0083AF80, 0, 4)
_HOOK_B = (0x0083AF88, 0, 4)

# The six synthetic widgets of c3bx_fwd: only the four dwords the two callees read and write are compared
# (+0x580 low bound, +0x584 high bound, +0x588 reverse flag, +0x58c result), plus the redraw recorder.
_FWD_STATE = [(f"$g{i}", 0x580, 0x10) for i in range(6)] + [("$rec", 0, 0x10)]

_QUANT_STATE = [("$c", 0, 0x180), ("$err", 0, 0x84), ("$q0", 0, 0x84), ("$q1", 0, 0x84)]

_Q = [-0x80000000, -1, 0, 1, 2, 10, 49, 50, 51, 75, 99, 100, 101, 1000, 0x7FFFFFFF]


def _dump(fixture, n):
    """dump_buffer: the n working_states and their destination managers (the manager's spare dword at +0x20
    records the refused calls of the FALSE fixture)."""
    return dict(module="golf_clean.exe", addr=0x004B19E0, abi="default", ret="int", args=["pointer"],
                fixture=fixture,
                state=[(f"$w{i}", 0, 0x24) for i in range(n)] + [(f"$d{i}", 0, 0x24) for i in range(n)],
                vectors=[(f"$w{i}",) for i in range(n)])


HOOKS = {
    # dump_buffer 0x004b19e0 (jchuff.c): empty_output_buffer TRUE (the jne at 0x004b19f5 taken, both copies
    # run) and FALSE (not taken, 0 returned and the working state untouched). Leaf, 14 vectors.
    "dump_buffer_4b19e0": _dump("c3bx_dump_ok", 8),
    "dump_buffer_4b19e0_fail": _dump("c3bx_dump_fail", 6),

    # jpeg_set_quality 0x004ae740: fifteen ratings covering every arm of jpeg_quality_scaling 0x004ae700
    # (<= 0, 1..49, 50..100, > 100) against both force_baseline values. The compared state is the whole cinfo,
    # the error manager and the two JQUANT_TBLs jpeg_add_quant_table 0x004ae600 fills. 30 vectors.
    "jpeg_set_quality": dict(module="golf_clean.exe", addr=0x004AE740, abi="default", ret="void",
                             args=["pointer", "int", "int"], fixture="c3bx_quality", state=_QUANT_STATE,
                             vectors=[("$c", q, fb) for q in _Q for fb in (0, 1)]),
    # The same with quant_tbl_ptrs[0] and [1] NULL: jpeg_alloc_quant_table 0x004afab0 runs through the
    # fixture's bump allocator and the two pointers are stored at 0x004ae63e. The pool replaces the two tables
    # in the compared state. 6 vectors.
    "jpeg_set_quality_alloc": dict(module="golf_clean.exe", addr=0x004AE740, abi="default", ret="void",
                                   args=["pointer", "int", "int"], fixture="c3bx_quality_alloc",
                                   state=[("$c", 0, 0x180), ("$err", 0, 0x84), ("$pool", 0, 0x400)],
                                   vectors=[("$c", 1, 1), ("$c", 25, 0), ("$c", 50, 1), ("$c", 75, 0),
                                            ("$c", 100, 1), ("$c", 0, 0)]),
    # global_state 0x63: both jpeg_add_quant_table calls raise ERREXIT1 0x12 at 0x004ae617 before touching the
    # table, so only the error manager moves. 4 vectors.
    "jpeg_set_quality_badstate": dict(module="golf_clean.exe", addr=0x004AE740, abi="default", ret="void",
                                      args=["pointer", "int", "int"], fixture="c3bx_quality_badstate",
                                      state=_QUANT_STATE,
                                      vectors=[("$c", 1, 0), ("$c", 50, 1), ("$c", 100, 0), ("$c", -5, 1)]),

    # StrList::dtor 0x00487260: the vtable store of 0x004baeac in every object, the null-child return at
    # 0x00487272 (three vectors) and the tail dispatch through the child's slot +0x14 (nine vectors, six
    # distinct child ids). Declared void: the null path leaves eax untouched, so only memory is compared.
    # Leaf, 12 vectors.
    "StrList_dtor": dict(module="golf_clean.exe", addr=0x00487260, abi="thiscall", ret="void",
                         args=["pointer"], fixture="c3bx_strlist",
                         state=[(f"$o{i}", 0, 0x20) for i in range(12)] + [("$rec", 0, 0x10)],
                         vectors=[(f"$o{i}",) for i in range(12)]),

    # releaseWithHookC 0x00487c40: the null-child return at 0x00487c5d (three vectors) and the two-call path
    # (nine vectors). Three of those nine have a child that swaps the parent's vtable during the first call, so
    # the second dispatch uses the NEW slot +8 - which is what makes the re-read of [esi] at 0x00487c4f
    # observable. The object (vtable, id and the cleared child field) and the recorder are compared.
    # Leaf, 12 vectors.
    "releaseWithHookC": dict(module="golf_clean.exe", addr=0x00487C40, abi="thiscall", ret="int",
                             args=["pointer"], fixture="c3bx_relc",
                             state=[(f"$o{i}", 0, 0x20) for i in range(12)] + [("$rec", 0, 0x10)],
                             vectors=[(f"$o{i}",) for i in range(12)]),

    # comboListQuery 0x00493ed0: the null sub-object return 0 at 0x00493ee2 (three vectors) and the tail jump
    # through slot +0xd8 at 0x00493edc (nine vectors, nine distinct returns). Leaf, 12 vectors.
    "comboListQuery": dict(module="golf_clean.exe", addr=0x00493ED0, abi="thiscall", ret="int",
                           args=["pointer"], fixture="c3bx_combo", state=[("$rec", 0, 0x10)],
                           vectors=[(f"$o{i}",) for i in range(12)]),

    # closeHookA 0x00487430: one key per setting of the global 0x0083af80 (the fixture writes it; it is a state
    # region, so diff_hook restores it). _set covers the handle-zero return 0x14 (0x00487438 taken) and the
    # hook path (handle and hook both non-null: return 0 and the handle cleared); _null covers the
    # hook-null return 0x14 (0x00487441 taken) on both sides of the handle test. Leaf, 11 vectors.
    "closeHookA_set": dict(module="golf_clean.exe", addr=0x00487430, abi="thiscall", ret="int",
                           args=["pointer"], fixture="c3bx_closeA_set",
                           state=[(f"$o{i}", 0, 0x20) for i in range(6)] + [("$rec", 0, 0x10), _HOOK_A],
                           vectors=[(f"$o{i}",) for i in range(6)]),
    "closeHookA_null": dict(module="golf_clean.exe", addr=0x00487430, abi="thiscall", ret="int",
                            args=["pointer"], fixture="c3bx_closeA_null",
                            state=[(f"$o{i}", 0, 0x20) for i in range(5)] + [("$rec", 0, 0x10), _HOOK_A],
                            vectors=[(f"$o{i}",) for i in range(5)]),

    # closeHookB 0x004879f0: the same against the global 0x0083af88. Leaf, 11 vectors.
    "closeHookB_set": dict(module="golf_clean.exe", addr=0x004879F0, abi="thiscall", ret="int",
                           args=["pointer"], fixture="c3bx_closeB_set",
                           state=[(f"$o{i}", 0, 0x20) for i in range(6)] + [("$rec", 0, 0x10), _HOOK_B],
                           vectors=[(f"$o{i}",) for i in range(6)]),
    "closeHookB_null": dict(module="golf_clean.exe", addr=0x004879F0, abi="thiscall", ret="int",
                            args=["pointer"], fixture="c3bx_closeB_null",
                            state=[(f"$o{i}", 0, 0x20) for i in range(5)] + [("$rec", 0, 0x10), _HOOK_B],
                            vectors=[(f"$o{i}",) for i in range(5)]),

    # Window::fwdScroll26c 0x0047b9f0: the null-child return at 0x0047ba04 ($wn and $w27, whose +0x26c is 0)
    # and the forward to setWidgetScalar 0x004967f0 on three widgets whose bounds put the value below the low
    # bound, inside the range and above the high bound, with the reverse flag at +0x588 clear ($wa -> g0) and
    # set ($wb -> g2, $wc -> g4). 15 vectors.
    "Window_fwdScroll26c": dict(module="golf_clean.exe", addr=0x0047B9F0, abi="thiscall", ret="void",
                                args=["pointer", "int"], fixture="c3bx_fwd", state=_FWD_STATE,
                                vectors=[("$wa", -0x80000000), ("$wa", 9), ("$wa", 10), ("$wa", 50),
                                         ("$wa", 90), ("$wa", 91), ("$wa", 0x7FFFFFFF),
                                         ("$wb", -1), ("$wb", 0), ("$wb", 5),
                                         ("$wc", -2000), ("$wc", 0), ("$wc", 2000),
                                         ("$wn", 42), ("$w27", 7)]),

    # Window::fwdScroll270 0x0047ba10: the same against the child at +0x270 ($wa -> g1 with a range across
    # zero, $wb -> g3 whose low bound is ABOVE its high bound, $wc -> g5 whose high bound is INT_MAX), with
    # $wn and $w26 holding a null +0x270. 14 vectors.
    "Window_fwdScroll270": dict(module="golf_clean.exe", addr=0x0047BA10, abi="thiscall", ret="void",
                                args=["pointer", "int"], fixture="c3bx_fwd", state=_FWD_STATE,
                                vectors=[("$wa", -6), ("$wa", -5), ("$wa", 0), ("$wa", 5), ("$wa", 6),
                                         ("$wa", 0x7FFFFFFF),
                                         ("$wb", 0), ("$wb", 100), ("$wb", 200),
                                         ("$wc", -1), ("$wc", 1000), ("$wc", 0x7FFFFFFF),
                                         ("$wn", 1), ("$w26", 3)]),

    # Window::fwdRange26c 0x0047ba30: the null-child return at 0x0047ba49 ($wn, $w27) and the forward to
    # orderPair 0x00496770 with both sides of its SIGNED compare at 0x0049677b (a < b straight through,
    # a >= b swapped by swapInts 0x00493580), including equal pairs and the INT_MIN / INT_MAX pair that an
    # unsigned compare would order the other way. The first argument is the one at [esp+4]. 16 vectors.
    "Window_fwdRange26c": dict(module="golf_clean.exe", addr=0x0047BA30, abi="thiscall", ret="void",
                               args=["pointer", "int", "int"], fixture="c3bx_fwd", state=_FWD_STATE,
                               vectors=[("$wa", 0, 10), ("$wa", 10, 0), ("$wa", 5, 5),
                                        ("$wa", -20, -5), ("$wa", -5, -20),
                                        ("$wa", -0x80000000, 0x7FFFFFFF), ("$wa", 0x7FFFFFFF, -0x80000000),
                                        ("$wa", 100, -100),
                                        ("$wb", 1, 2), ("$wb", 2, 1), ("$wb", 0, 0),
                                        ("$wc", -1000, 1000), ("$wc", 1000, -1000), ("$wc", 7, 7),
                                        ("$wn", 1, 2), ("$w27", 3, 4)]),

    # Window::fwdRange270 0x0047ba50: the same against the child at +0x270, with $wn and $w26 holding a null
    # +0x270. 16 vectors.
    "Window_fwdRange270": dict(module="golf_clean.exe", addr=0x0047BA50, abi="thiscall", ret="void",
                               args=["pointer", "int", "int"], fixture="c3bx_fwd", state=_FWD_STATE,
                               vectors=[("$wa", -5, 5), ("$wa", 5, -5), ("$wa", 0, 0),
                                        ("$wa", -0x80000000, 0x7FFFFFFF), ("$wa", 0x7FFFFFFF, -0x80000000),
                                        ("$wa", 3, 3),
                                        ("$wb", 50, 100), ("$wb", 100, 50), ("$wb", -1, -1),
                                        ("$wc", 0, 0x7FFFFFFF), ("$wc", 0x7FFFFFFF, 0), ("$wc", -7, 9),
                                        ("$wc", 9, -7),
                                        ("$wn", 1, 2), ("$w26", 3, 4), ("$w26", 4, 3)]),
}
