# C3 batch c3cc (round 19, 2026-10-08) of golf_clean.exe: two child-release wrappers, the third global close
# hook, the scrollbar child's two entry points and five IJG libjpeg 6a routines.
# Reimplementations in shim/src/re/c3cc.cpp, fixtures in re/frida/js/fixtures.d/c3cc.js, Purposes and per-jcc
# branch coverage in log/c3/c3cc_purpose.md.
#
# Hooked callees (the verifier should re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses
# original callees; none of them is in this batch):
#   releaseWithHookA         -> 00487430 closeHookA (C3, c3bx)
#   releaseWithHookB         -> 004879f0 closeHookB (C3, c3bx)
#   orderPair                -> 00493580 swapInts (C3, c3r)
#   scrollbarResetThumb      -> 004979a0 scrollbarResetState (C3, c3v)
#   jpeg_set_linear_quality  -> 004ae600 jpeg_add_quant_table (C3, c3aa) and, on the _alloc fixture,
#                               004afab0 jpeg_alloc_quant_table (C3, c3af) under it
#   jpeg_destroy_compress    -> 004afa90 jpeg_destroy (C3, c3r)
# The pointers the rest dispatch through (the child's vtable slot +0x14, the widget's vtable slot +0x120, the
# global close hook 0x0083af90, the libjpeg error manager's error_exit at err+0 and the memory manager's
# self_destruct at mem+0x28) are the fixture's own callbacks, not hooks.

# The three close-hook globals are state regions so diff_hook restores them between the arms and after the run.
_HOOK_A = (0x0083AF80, 0, 4)
_HOOK_B = (0x0083AF88, 0, 4)
_HOOK_C = (0x0083AF90, 0, 4)

# The eight synthetic objects of the release fixtures plus the dispatch recorder.
_REL_STATE = [(f"$o{i}", 0, 0x20) for i in range(8)] + [("$rec", 0, 0x10)]

# orderPair: the four dwords the function writes in each widget, plus the redraw recorder.
_ORDER_STATE = [(f"$g{i}", 0x580, 0x10) for i in range(3)] + [("$rec", 0, 0x10)]

# scrollbarResetThumb: +0x57c (the branch field) through +0x5c4 (past +0x5c0) of each widget.
_SCROLL_STATE = [(f"$w{i}", 0x57C, 0x48) for i in range(6)]

_QUANT_STATE = [("$c", 0, 0x180), ("$err", 0, 0x84), ("$q0", 0, 0x84), ("$q1", 0, 0x84)]

# Scale factors covering every clamp of jpeg_add_quant_table 0x004ae600 on the two standard tables
# (entries 16..121 at 0x004bcb08 and 17..99 at 0x004bcc08): <= 0 -> 1 (0x004ae665), > 0x7fff -> 0x7fff
# (0x004ae674), and > 0xff with force_baseline -> 0xff (0x004ae689).
_SCALE = [-100, -1, 0, 1, 25, 50, 100, 300, 1000, 100000]


def _release(addr, fixture, hook_global):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret="int", args=["pointer"],
                fixture=fixture, state=_REL_STATE + [hook_global],
                vectors=[(f"$o{i}",) for i in range(8)])


HOOKS = {
    # releaseWithHookA 0x00487460: three vectors take the null-child return at 0x0048747d, five run the child's
    # vtable slot +0x14 (0x0048746c), then closeHookA 0x00487430 (0x00487471) and the clear at 0x00487476.
    # The _set key has the global 0x0083af80 pointing at the fixture hook (closeHookA runs it at 0x00487443
    # and clears +0x14 itself at 0x00487445); _null leaves the global NULL, so closeHookA returns 0x14 at
    # 0x00487450 without clearing. 16 vectors over both keys.
    "releaseWithHookA_set": _release(0x00487460, "c3cc_relA_set", _HOOK_A),
    "releaseWithHookA_null": _release(0x00487460, "c3cc_relA_null", _HOOK_A),

    # releaseWithHookB 0x00487ac0: the same against closeHookB 0x004879f0 and the global 0x0083af88.
    # 16 vectors over both keys.
    "releaseWithHookB_set": _release(0x00487AC0, "c3cc_relB_set", _HOOK_B),
    "releaseWithHookB_null": _release(0x00487AC0, "c3cc_relB_null", _HOOK_B),

    # closeHookC 0x00487bd0: one key per setting of the global 0x0083af90 (the fixture writes it; it is a state
    # region, so diff_hook restores it). _set covers the handle-zero return 0x14 (0x00487bd8 taken) and the
    # hook path (handle and hook both non-null: the hook runs at 0x00487be3, +0x14 is cleared at 0x00487be5 and
    # the result is 0); _null covers the hook-null return 0x14 (0x00487be1 taken) on both sides of the handle
    # test. Leaf, 11 vectors over both keys.
    "closeHookC_set": dict(module="golf_clean.exe", addr=0x00487BD0, abi="thiscall", ret="int",
                           args=["pointer"], fixture="c3cc_closeC_set",
                           state=[(f"$o{i}", 0, 0x20) for i in range(6)] + [("$rec", 0, 0x10), _HOOK_C],
                           vectors=[(f"$o{i}",) for i in range(6)]),
    "closeHookC_null": dict(module="golf_clean.exe", addr=0x00487BD0, abi="thiscall", ret="int",
                            args=["pointer"], fixture="c3cc_closeC_null",
                            state=[(f"$o{i}", 0, 0x20) for i in range(5)] + [("$rec", 0, 0x10), _HOOK_C],
                            vectors=[(f"$o{i}",) for i in range(5)]),

    # orderPair 0x00496770: both sides of the SIGNED compare at 0x0049677b (a < b straight through with
    # +0x588 = 0; a >= b, including equal pairs, swapped by swapInts 0x00493580 with +0x588 = 1), including
    # the INT_MIN / INT_MAX pairs an unsigned compare would order the other way. The return value is whatever
    # the widget's vtable slot +0x120 returns (0x004967c3). 18 vectors.
    "orderPair": dict(module="golf_clean.exe", addr=0x00496770, abi="thiscall", ret="int",
                      args=["pointer", "int", "int"], fixture="c3cc_order", state=_ORDER_STATE,
                      vectors=[("$g0", 0, 10), ("$g0", 10, 0), ("$g0", 5, 5),
                               ("$g0", -20, -5), ("$g0", -5, -20),
                               ("$g0", -0x80000000, 0x7FFFFFFF), ("$g0", 0x7FFFFFFF, -0x80000000),
                               ("$g1", -1, 0), ("$g1", 0, -1), ("$g1", 100, -100), ("$g1", 0, 0),
                               ("$g1", 1, 2),
                               ("$g2", -1000, 1000), ("$g2", 1000, -1000), ("$g2", 7, 7),
                               ("$g2", -0x80000000, -0x80000000), ("$g2", 0x7FFFFFFF, 0x7FFFFFFF),
                               ("$g2", 3, 4)]),

    # scrollbarResetThumb 0x004967d0: the store of the caller's dword into +0x5c0 (0x004967d4) and the four
    # stores scrollbarResetState 0x004979a0 makes from it. $w0 and $w1 carry +0x57c == -1 (the je at
    # 0x004979c7 taken, so +0x5b4 / +0x5b8 keep the span and +0x5ac / +0x5b0 stay 0); $w2..$w5 carry 0, 1, -2
    # and INT_MAX (not taken, so +0x5ac / +0x5b0 become 1 and +0x5b4 / +0x5b8 the span minus one). 14 vectors.
    "scrollbarResetThumb": dict(module="golf_clean.exe", addr=0x004967D0, abi="thiscall", ret="void",
                                args=["pointer", "int"], fixture="c3cc_scroll", state=_SCROLL_STATE,
                                vectors=[("$w0", 0), ("$w0", 100), ("$w1", -5), ("$w1", 0x7FFFFFFF),
                                         ("$w2", 0), ("$w2", 1), ("$w2", 50),
                                         ("$w3", -1), ("$w3", 0x7FFFFFFF),
                                         ("$w4", -0x80000000), ("$w4", 7),
                                         ("$w5", 0), ("$w5", 1000), ("$w5", -3)]),

    # jpeg_set_linear_quality 0x004ae6c0: ten scale factors against both force_baseline values. The compared
    # state is the whole cinfo, the error manager and the two JQUANT_TBLs the two jpeg_add_quant_table
    # 0x004ae600 calls fill from the basic tables at 0x004bcb08 and 0x004bcc08. 20 vectors.
    "jpeg_set_linear_quality": dict(module="golf_clean.exe", addr=0x004AE6C0, abi="default", ret="void",
                                    args=["pointer", "int", "int"], fixture="c3cc_linq", state=_QUANT_STATE,
                                    vectors=[("$c", s, fb) for s in _SCALE for fb in (0, 1)]),
    # The same with quant_tbl_ptrs[0] and [1] NULL: jpeg_alloc_quant_table 0x004afab0 runs through the
    # fixture's bump allocator and the two pointers are stored at 0x004ae63e. The pool replaces the two tables
    # in the compared state. 6 vectors.
    "jpeg_set_linear_quality_alloc": dict(module="golf_clean.exe", addr=0x004AE6C0, abi="default", ret="void",
                                          args=["pointer", "int", "int"], fixture="c3cc_linq_alloc",
                                          state=[("$c", 0, 0x180), ("$err", 0, 0x84), ("$pool", 0, 0x400)],
                                          vectors=[("$c", 1, 1), ("$c", 25, 0), ("$c", 50, 1),
                                                   ("$c", 100, 0), ("$c", 300, 1), ("$c", 100000, 0)]),
    # global_state 0x63: both jpeg_add_quant_table calls raise ERREXIT1 0x12 at 0x004ae617 and, with the
    # fixture's RETURNING error_exit, go on to fill the table. 6 vectors.
    "jpeg_set_linear_quality_badstate": dict(module="golf_clean.exe", addr=0x004AE6C0, abi="default",
                                             ret="void", args=["pointer", "int", "int"],
                                             fixture="c3cc_linq_badstate", state=_QUANT_STATE,
                                             vectors=[("$c", 1, 0), ("$c", 50, 1), ("$c", 100, 0),
                                                      ("$c", -5, 1), ("$c", 300, 1), ("$c", 100000, 0)]),

    # out_of_memory 0x004af0a0: twelve `which` values, each leaving a different err+0x18 (0x004af0b4) and a
    # different recorder hash, on top of the constant message code 0x35 (0x004af0a7). The error manager is the
    # state region. Leaf, 12 vectors.
    "out_of_memory": dict(module="golf_clean.exe", addr=0x004AF0A0, abi="default", ret="void",
                          args=["pointer", "int"], fixture="c3cc_oom", state=[("$err", 0, 0x84)],
                          vectors=[("$c", w) for w in (0, 1, 2, 3, 4, 7, 0x55, 1000, 0x12345678,
                                                       -1, -0x80000000, 0x7FFFFFFF)]),

    # jpeg_open_backing_store 0x004b0490: ten compress objects whose error managers differ in their pre-fill,
    # in the value already in err+0x14 (so the store of 0x30 at 0x004b0497 shows against a different
    # background) and in the seed at +0x40 the recorder mixes in. The second and third arguments vary although
    # no instruction reads them. Leaf, 10 vectors.
    "jpeg_open_backing_store": dict(module="golf_clean.exe", addr=0x004B0490, abi="default", ret="void",
                                    args=["pointer", "pointer", "int"], fixture="c3cc_obs",
                                    state=[(f"$e{i}", 0, 0x84) for i in range(10)],
                                    vectors=[("$c0", 0, 0), ("$c1", 0, 1), ("$c2", 0, -1),
                                             ("$c3", 0, 0x7FFFFFFF), ("$c4", 0, -0x80000000),
                                             ("$c5", "$e5", 1000), ("$c6", "$e6", 0x10000),
                                             ("$c7", 0, 12345), ("$c8", "$e8", -999), ("$c9", 0, 0x40)]),

    # jpeg_mem_available 0x004b0480: fourteen argument lists with fourteen distinct third dwords, which is the
    # only one the body reads (0x004b0480). The compress object is never dereferenced, so NULL is passed.
    # Leaf, 14 vectors, no state.
    "jpeg_mem_available": dict(module="golf_clean.exe", addr=0x004B0480, abi="default", ret="int",
                               args=["pointer", "int", "int", "int"],
                               vectors=[(0, 0, 0, 0), (0, 1, 2, 3), (0, 0x1000, 0x2000, 0x40),
                                        (0, -1, -2, -3), (0, 0, -0x80000000, 0),
                                        (0, 0x7FFFFFFF, 0x7FFFFFFE, 1), (0, 5, 0x7FFFFFFF, 9),
                                        (0, 100, 50, 25), (0, 0x10000, 0x100000, 0x1000),
                                        (0, -7, 7, -7), (0, 0, 1, 0x7FFFFFFF), (0, 3, -4, 5),
                                        (0, 0x55AA, 0xAA55, 0x1234), (0, 1 << 20, 1 << 24, 1 << 28)]),

    # jpeg_destroy_compress 0x004ae210: ten compress objects, three with a null memory manager (the je at
    # 0x004afa9d taken inside jpeg_destroy 0x004afa90) and seven whose self_destruct at mem+0x28 is the
    # fixture's callback (0x004afaa0). Both the cleared cinfo+4 / cinfo+0x10 (0x004afaa6 / 0x004afaa9) and the
    # dispatch recorder are compared. 10 vectors.
    "jpeg_destroy_compress": dict(module="golf_clean.exe", addr=0x004AE210, abi="default", ret="void",
                                  args=["pointer"], fixture="c3cc_destroy",
                                  state=[(f"$c{i}", 0, 0x20) for i in range(10)]
                                        + [(f"$m{i}", 0, 0x40) for i in (1, 2, 4, 5, 6, 8, 9)]
                                        + [("$rec", 0, 0x10)],
                                  vectors=[(f"$c{i}",) for i in range(10)]),
}
