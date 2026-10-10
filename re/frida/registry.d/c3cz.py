# Batch c3cz (round 24, 2026-10-10): golf_clean.exe functions earlier batches dropped for the CRT heap.
# Reimplementations shim/src/re/c3cz.cpp, fixtures re/frida/js/fixtures.d/c3cz.js, purposes log/c3/c3cz_purpose.md.
#
# Every key runs under the fixture's heap stand-in (czHeap: malloc 0x004a504f and operator new 0x004a57f1 carve
# from an arena whose header - cursor, call counter, fail mask, fail size - is state; free 0x004a5007 only records),
# so both arms receive the same blocks, a scheduled NULL hits both arms alike, and the frees are compared state.
# Live globals a function writes are swapped under the shared freeze helper (re/frida/js/freeze.js) and listed in
# that key's state.
#
# Hooked callees (the verifier re-runs these with the callee hooks off):
#   THIS batch:  ChildArray::insert -> ChildArray_grow; ChildArray::remove -> Vec_release; listEraseRange,
#                Surface_free -> operatorDelete_4a4ffc; MsgBox::initDefaults -> MsgBox::setLabelA / setLabelB;
#                Surface_open, freeFonts -> Surface_free; Surface_free -> Cache483010::dtor -> Cache483070::reset;
#                initPairArrayGlobal -> allocPairArray; ListModel::dtor -> ListModel::clear, List402_clear;
#                alloc_sarray / alloc_barray -> alloc_small, alloc_large; self_destruct_jmemmgr -> free_pool
#   earlier:     Buf_alloc, ListModel::clear -> Buf_releaseData 0x004747e0 (c3cn); ListModel::dtor -> Buf_dtor
#                0x00474810 (c3cn); listPush -> listRemoveHead 0x004a4ed0 (c3cn); Label::setText -> Window::visible
#                0x004801f0; allocPairArray -> Buf_ctor 0x004747a0; alloc_small / alloc_large -> out_of_memory
#                0x004af0a0 (c3cc), jpeg_get_large 0x004b0460 (c3cn); free_pool / self_destruct -> jpeg_free_large
#                0x004b0470 (c3cn); jinit_c_coef_controller -> jround_up 0x004b04d0; std_huff_tables ->
#                add_huff_table 0x004ae8b0 (c3be); finish_pass_phuff -> emit_eobrun 0x004b25d0 (c3bo),
#                flush_bits_4b2510 0x004b2510 (c3aa) and through them emit_bits 0x004b1a10, emit_buffered_bits
#                0x004b27d0 (c3aa), dump_buffer_4b2420 0x004b2420 (c3be)
#   other round: Cache482f60::scalarDtorAdj20 -> Cache482f60::dtor 0x004835f0 (a c3db candidate)

EXE = "golf_clean.exe"


def _st(arena=0x4000, extra=()):
    return [("$blk", 0, 0x10000), ("$arena", 0, arena + 0x10), ("$freelog", 0, 0x404), ("$rec", 0, 0x400)] + list(extra)


def _h(addr, abi, ret, args, fixture, vectors, arena=0x4000, extra=()):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=args, fixture=fixture, state=_st(arena, extra),
                vectors=vectors)


_S = _st()
_MSG = [(0x0083B994, 0, 0x38)]
_C482 = [(0x00839650, 0, 4)]

# ChildArray contents (fixture `arrays`): array i slot k holds child (5k + i) % 12.
_INSERT = [("$a0", "$c0"), ("$a0", "$c1"), ("$a2", 0), ("$a3", "$c4"), ("$a3", "$c5"), ("$a4", "$c6"),
           ("$a4", "$c7"), ("$a10", "$c8"), ("$a10", "$c9"), ("$a15", "$c10"), ("$a9", "$c2"), ("$a11", "$c3"),
           ("$a12", "$c11")]
_REMOVE = [("$a3", "$c3"), ("$a3", "$c8"), ("$a3", "$c1"), ("$a3", "$c0"), ("$a3", 0), ("$a4", "$c9"),
           ("$a6", "$c4"), ("$a6", "$c1"), ("$a2", "$c0"), ("$a12", "$c5"), ("$a11", "$c4"), ("$a14", "$c2"),
           ("$a8", "$c1")]
_TEXT6 = [0, "$s0", "$s1", "$s2", "$s3", "$s4", "$s5"]
_PUSH = [("$L0", "$out", 0, 1, 2), ("$L0", "$out", 8, 3, 4), ("$L1", "$out", 0, 5, 6), ("$L1", "$out", 0x20, 7, 8),
         ("$L2", "$out", 4, 9, 10), ("$L3", "$out", 4, 11, 12), ("$L4", "$out", 4, 13, 14), ("$L5", "$out", 1, 15, 16),
         ("$L0", 0, 4, 17, 18), ("$L2", "$out", 0x100, 19, 20), ("$L5", "$out", 0, 21, 22),
         ("$L1", "$out", 0x3000, 23, 24)]
_ERASE = [("$L", "$out", "$n1", "$n0"), ("$L", "$out", "$n2", "$n4"), ("$L", "$out", "$n3", "$n3"),
          ("$L", "$out", "$n1", "$n2"), ("$L", "$out", "$n5", "$n0"), ("$L", "$out", "$n4", "$n1"),
          ("$L", "$out", "$n0", "$n0"), ("$L", "$out", "$n2", "$n0"), ("$L", "$out", "$n1", "$n5"),
          ("$L", "$out", "$n3", "$n5"), ("$L", "$out", "$n5", "$n2"), ("$L", "$out", "$n2", "$n3")]
_ECX = [0, 0x01000000, 0xFF000000, 0x12345678, 0x80000000, 0x7F000000, 0xABCDEF01, 0x00FFFFFF, 0x5A000000,
        0xC3000000, 0x01020304, 0xFE000000]
_OPEN = [("$S0", 0, 1, 2), ("$S1", "$s1", 10, 20), ("$S2", "$s2", -1, 0x7FFFFFFF), ("$S3", "$s3", 3, 4),
         ("$S4", "$s4", 0, 0), ("$S5", "$s0", 640, 480), ("$S6", "$s5", -5, 6), ("$S7", "$s1", 7, -8),
         ("$S12", "$s2", 9, 9), ("$S13", "$s3", 1, 1), ("$S14", "$s4", 100, 200), ("$S15", "$s5", 32, 16)]
_CFLAGS = [0, 1, 0, 0x100, -1, 0, 1, 0, 2, 0, 1, 0]
_DFLAGS = [1, 0, 3, 2, 1, 1, 0xFFFFFFFF, 0, 1, 0x80000000, 5, 4]
_LOCKS = 12

HOOKS = {
    # ---------------------------------------------------------------- Part 1: CRT heap leaves
    "operatorDelete_4a4ffc": _h(0x004A4FFC, "default", "void", ["pointer"], "c3cz_bufs",
                                [(0,)] + [(f"$b{i}",) for i in range(11)]),
    "ChildArray_grow": _h(0x00479B40, "thiscall", "void", ["pointer"], "c3cz_arrays",
                          [(f"$a{i}",) for i in range(16)]),
    "Vec_release": _h(0x00479BB0, "thiscall", "void", ["pointer"], "c3cz_arrays",
                      [(f"$a{i}",) for i in range(16)]),
    "ChildArray::insert": _h(0x0047E5F0, "thiscall", "void", ["pointer", "pointer"], "c3cz_arrays", _INSERT),
    "ChildArray::remove": _h(0x0047E680, "thiscall", "void", ["pointer", "pointer"], "c3cz_arrays", _REMOVE),
    "Buf_alloc": _h(0x00474820, "thiscall", "int", ["pointer", "uint32"], "c3cz_bufs",
                    [("$b0", 0), ("$b1", 1), ("$b2", 7), ("$b3", 16), ("$b4", 0x100), ("$b5", 0x333),
                     ("$b6", 0x333), ("$b7", 0x20), ("$b8", 0x1000), ("$b9", 5), ("$b10", 0x3FF0), ("$b11", 0x5000)]),
    "EditBox::resizeBuffer": _h(0x00486250, "thiscall", "void", ["pointer", "int"], "c3cz_texts",
                                [("$e0", 0), ("$e1", 5), ("$e1", 10), ("$e2", 3), ("$e3", 0), ("$e4", 40), ("$e5", 1),
                                 ("$e6", 6), ("$e7", 2), ("$e8", 64), ("$e10", 0), ("$e11", 100)]),
    "Button::setTooltip": _h(0x00488A20, "thiscall", "int", ["pointer", "pointer"], "c3cz_texts_fail",
                             [("$t0", "$s1"), ("$t1", "$s2"), ("$t2", 0), ("$t3", 0), ("$t4", "$s0"), ("$t5", "$s3"),
                              ("$t6", "$s3"), ("$t7", "$s4"), ("$t8", "$s5"), ("$t9", "$s1"), ("$t10", "$s2"),
                              ("$t11", "$s4")]),
    "setStringField": _h(0x004A29C0, "thiscall", "int", ["pointer", "pointer", "int"], "c3cz_texts_fail",
                         [("$f0", 0, 0), ("$f1", 0, 1), ("$f2", 0, 2), ("$f0", "$s1", 0), ("$f1", "$s2", 1),
                          ("$f2", "$s3", 2), ("$f3", "$s3", 3), ("$f4", "$s4", 3), ("$f5", "$s0", 0),
                          ("$f6", "$s5", 1), ("$f7", 0, 3), ("$f8", "$s2", 2), ("$f9", "$s1", 3)]),
    "listPush": _h(0x004A4DB0, "thiscall", "int", ["pointer", "pointer", "uint32", "int", "int"], "c3cz_push", _PUSH),
    "listPush_failnode": _h(0x004A4DB0, "thiscall", "int", ["pointer", "pointer", "uint32", "int", "int"],
                            "c3cz_push_fail0", _PUSH),
    "listPush_faildata": _h(0x004A4DB0, "thiscall", "int", ["pointer", "pointer", "uint32", "int", "int"],
                            "c3cz_push_fail1", _PUSH),
    "listEraseRange": _h(0x0044AC00, "thiscall", "pointer", ["pointer", "pointer", "pointer", "pointer"], "c3cz_erase",
                         _ERASE),
    "allocTileBuffer": _h(0x00449210, "thiscall", "pointer", ["pointer"], "c3cz_tilebuf", [(v,) for v in _ECX],
                          arena=0x400, extra=[(0x00820F18, 0, 12)]),
    "View_unlinkFromParent": _h(0x0047ADA0, "thiscall", "void", ["pointer"], "c3cz_views",
                                [(f"$v{i}",) for i in range(15)]),
    "Label::setText": _h(0x0047B8F0, "thiscall", "void", ["pointer", "pointer"], "c3cz_labels",
                         [("$l0", "$s1"), ("$l1", "$s2"), ("$l2", "$s5"), ("$l3", "$s1"), ("$l4", 0), ("$l5", 0),
                          ("$l6", "$s3"), ("$l7", "$s3"), ("$l8", "$s4"), ("$l9", "$s0"), ("$l10", "$s3"),
                          ("$l11", "$s2")]),
    "MsgBox::setLabelA_old": _h(0x0048DE00, "default", "int", ["pointer"], "c3cz_msgbox_old",
                                [(t,) for t in _TEXT6], extra=_MSG),
    "MsgBox::setLabelA_null": _h(0x0048DE00, "default", "int", ["pointer"], "c3cz_msgbox_null",
                                 [(t,) for t in _TEXT6], extra=_MSG),
    "MsgBox::setLabelB_old": _h(0x0048DE90, "default", "int", ["pointer"], "c3cz_msgbox_old",
                                [(t,) for t in _TEXT6], extra=_MSG),
    "MsgBox::setLabelB_null": _h(0x0048DE90, "default", "int", ["pointer"], "c3cz_msgbox_null",
                                 [(t,) for t in _TEXT6], extra=_MSG),
    "HotList::freeEntryTip_g0": _h(0x00492660, "thiscall", "void", ["pointer"], "c3cz_tips_g0",
                                   [(f"$h{i}",) for i in range(12)], extra=[(0x0083D348, 0, 4)]),
    "HotList::freeEntryTip_g1": _h(0x00492660, "thiscall", "void", ["pointer"], "c3cz_tips_g1",
                                   [(f"$h{i}",) for i in range(12)], extra=[(0x0083D348, 0, 4)]),

    # ---------------------------------------------------------------- Part 2: Surface and caches
    "Surface_ctor": _h(0x00473AB0, "thiscall", "pointer", ["pointer"], "c3cz_pairs", [(f"$p{i}",) for i in range(12)]),
    "Surface_free_disp": _h(0x00473AE0, "thiscall", "void", ["pointer"], "c3cz_surf_disp",
                            [(f"$S{i}",) for i in range(16)]),
    "Surface_free_nodisp": _h(0x00473AE0, "thiscall", "void", ["pointer"], "c3cz_surf_nodisp",
                              [(f"$S{i}",) for i in range(16)]),
    "Surface_open_disp": _h(0x00473B50, "thiscall", "int", ["pointer", "pointer", "int", "int"], "c3cz_surf_disp",
                            _OPEN),
    "Surface_open_nodisp": _h(0x00473B50, "thiscall", "int", ["pointer", "pointer", "int", "int"],
                              "c3cz_surf_nodisp", _OPEN),
    "Cache483070::reset_disp": _h(0x00483070, "thiscall", "void", ["pointer"], "c3cz_surf_disp",
                                  [(f"$C{i}",) for i in range(12)]),
    "Cache483070::reset_nodisp": _h(0x00483070, "thiscall", "void", ["pointer"], "c3cz_surf_nodisp",
                                    [(f"$C{i}",) for i in range(12)]),
    "Cache483010::dtor_disp": _h(0x00483010, "thiscall", "void", ["pointer"], "c3cz_surf_disp",
                                 [(f"$C{i}",) for i in range(12)]),
    "Cache483010::dtor_nodisp": _h(0x00483010, "thiscall", "void", ["pointer"], "c3cz_surf_nodisp",
                                   [(f"$C{i}",) for i in range(12)]),
    "createCache": _h(0x0049C830, "thiscall", "int", ["pointer", "pointer"], "c3cz_create",
                      [(f"$k{i}", f"$o{i}") for i in range(12)]),
    "createCache_fail": _h(0x0049C830, "thiscall", "int", ["pointer", "pointer"], "c3cz_create_fail",
                           [(f"$k{i}", f"$o{i}") for i in range(12)]),
    "Cache482f60::ctor_g0": _h(0x00482F60, "thiscall", "pointer", ["pointer", "int"], "c3cz_c482_g0",
                               [(f"$q{i}", _CFLAGS[i]) for i in range(12)], extra=_C482),
    "Cache482f60::ctor_g1": _h(0x00482F60, "thiscall", "pointer", ["pointer", "int"], "c3cz_c482_g1",
                               [(f"$q{i}", _CFLAGS[i]) for i in range(12)], extra=_C482),
    "Cache482f60::scalarDtorAdj20_g0": _h(0x00483750, "thiscall", "pointer", ["pointer", "uint32"], "c3cz_c482_g0",
                                          [(f"$d{i}", _DFLAGS[i]) for i in range(12)], extra=_C482),
    "Cache482f60::scalarDtorAdj20_g1": _h(0x00483750, "thiscall", "pointer", ["pointer", "uint32"], "c3cz_c482_g1",
                                          [(f"$d{i}", _DFLAGS[i]) for i in range(12)], extra=_C482),
    "allocPairArray": _h(0x0049D050, "thiscall", "pointer", ["pointer"], "c3cz_pairs",
                         [(f"$p{i}",) for i in range(12)]),

    # ---------------------------------------------------------------- Part 3: node lists
    "List402_clear": _h(0x004026A0, "thiscall", "void", ["pointer"], "c3cz_lists", [(f"$l{i}",) for i in range(12)]),
    "ListModel::clear": _h(0x004894B0, "thiscall", "void", ["pointer"], "c3cz_lists",
                           [(f"$m{i}",) for i in range(12)]),
    "ListModel::clear_realvt": _h(0x004894B0, "thiscall", "void", ["pointer"], "c3cz_lists_realvt",
                                  [(f"$m{i}",) for i in range(12)]),
    "ListModel::dtor": _h(0x00489370, "thiscall", "void", ["pointer"], "c3cz_lists_frozen",
                          [(f"$m{i}",) for i in range(12)], extra=_C482),
    "panelSubDtor4a33e0": _h(0x004A33E0, "thiscall", "void", ["pointer"], "c3cz_lists",
                             [(f"$ps{i}",) for i in range(12)]),

    # ---------------------------------------------------------------- Part 4: libjpeg
    "alloc_small": _h(0x004AEF70, "default", "pointer", ["pointer", "int", "uint32"], "c3cz_jmem",
                      [("$j0", 0, 0x10), ("$j1", 0, 0x20), ("$j2", 0, 0x100), ("$j3", 1, 0x10), ("$j4", 1, 0x40),
                       ("$j5", 0, 0x10), ("$j6", 1, 0x30), ("$j7", 0, 0x20), ("$j0", 1, 0xFFF0), ("$j1", 2, 8),
                       ("$j5", 0, 0xFFD8), ("$j2", 1, 7), ("$j3", 0, 0), ("$j3", -1, 0x10),
                       ("$j6", -1, 0x40)], arena=0x30000),
    "alloc_small_fail": _h(0x004AEF70, "default", "pointer", ["pointer", "int", "uint32"], "c3cz_jmem_fail",
                           [("$j0", 0, 0x10), ("$j5", 0, 0x10), ("$j5", 0, 0xFFD8), ("$j4", 1, 0x40),
                            ("$j3", 1, 0x10), ("$j1", 0, 0x20), ("$j6", 1, 0x99), ("$j7", 1, 0x8),
                            ("$j2", 0, 0x100), ("$j0", 1, 0x200)], arena=0x30000),
    "alloc_large": _h(0x004AF0C0, "default", "pointer", ["pointer", "int", "uint32"], "c3cz_jmem",
                      [("$j0", 0, 0x10), ("$j1", 1, 0x100), ("$j5", 0, 7), ("$j6", 1, 0x40), ("$j2", 0, 0xFFF0),
                       ("$j3", 2, 0x20), ("$j4", 0, 0), ("$j7", 1, 0x1000), ("$j8", 0, 0x30), ("$j9", 1, 8),
                       ("$j10", 0, 0x18), ("$j11", 1, 0x80), ("$j0", -1, 0x10), ("$j4", -1, 0x20)], arena=0x30000),
    "alloc_sarray": _h(0x004AF170, "default", "pointer", ["pointer", "int", "uint32", "uint32"], "c3cz_jmem",
                       [("$j0", 1, 100, 10), ("$j1", 1, 0x1000, 40), ("$j2", 0, 1, 3), ("$j3", 1, 0xFFE0, 2),
                        ("$j4", 1, 0xFFF0, 0), ("$j5", 0, 10, 0), ("$j6", 1, 7, 9), ("$j7", 0, 0x2000, 8),
                        ("$j8", 1, 64, 64), ("$j9", 0, 3, 1), ("$j10", 1, 0x800, 33), ("$j11", 0, 1000, 65)],
                       arena=0x30000),
    "alloc_barray": _h(0x004AF220, "default", "pointer", ["pointer", "int", "uint32", "uint32"], "c3cz_jmem",
                       [("$j0", 1, 2, 10), ("$j1", 1, 32, 40), ("$j2", 0, 1, 3), ("$j3", 1, 0x1FF, 3),
                        ("$j4", 1, 0x200, 0), ("$j5", 0, 4, 0), ("$j6", 1, 3, 9), ("$j7", 0, 64, 4),
                        ("$j8", 1, 1, 64), ("$j9", 0, 5, 1), ("$j10", 1, 16, 20), ("$j11", 0, 8, 40)],
                       arena=0x30000),
    "free_pool": _h(0x004AF920, "default", "void", ["pointer", "int"], "c3cz_jmem",
                    [("$j0", 0), ("$j1", 0), ("$j2", 0), ("$j3", 1), ("$j4", 1), ("$j5", 0), ("$j6", 1), ("$j7", 0),
                     ("$j8", 1), ("$j9", 1), ("$j10", 0), ("$j11", 1), ("$j1", 2), ("$j5", 2), ("$j4", -1),
                     ("$j6", -1)], arena=0x30000),
    "self_destruct_jmemmgr": _h(0x004AFA20, "default", "void", ["pointer"], "c3cz_jmem",
                                [(f"$j{i}",) for i in range(12)], arena=0x30000),
    "jinit_marker_writer": _h(0x004AFAF0, "default", "void", ["pointer"], "c3cz_jinit",
                              [(f"$c{i}",) for i in range(12)]),
    "jinit_c_coef_controller": _h(0x004B06F0, "default", "void", ["pointer", "int"], "c3cz_jinit",
                                  [(f"$c{i}", [1, 0, 1, 1, 1, 0, 1, 0, 1, 2, -1, 1][i]) for i in range(12)]),
    "jinit_huff_encoder": _h(0x004B12B0, "default", "void", ["pointer"], "c3cz_jinit",
                             [(f"$c{i}",) for i in range(12)]),
    "std_huff_tables": _h(0x004AE850, "default", "void", ["pointer"], "c3cz_jinit", [(f"$c{i}",) for i in range(12)]),
    "finish_pass_phuff": _h(0x004B3380, "default", "void", ["pointer"], "c3cz_phuff", [(f"$c{i}",) for i in range(14)]),
}

# argument-free functions: one call per key, one key per fixture variant
for v in "abcde":
    HOOKS[f"MsgBox::initDefaults_{v}"] = _h(0x00490BF0, "default", "int", [], f"c3cz_msginit_{v}", [()],
                                            extra=_MSG + [(0x0083AD44, 0, 4)])
for v in ("block", "odd", "null"):
    HOOKS[f"freeFileWinBuf_{v}"] = _h(0x0049D460, "default", "void", [], f"c3cz_fwb_{v}", [()],
                                      extra=[(0x00840820, 0, 4)])
    HOOKS[f"freeResampleTable_{v}"] = _h(0x00481B20, "default", "void", [], f"c3cz_frt_{v}", [()],
                                         extra=[(0x0083AC10, 0, 4)])
for v in ("disp_a", "disp_b", "disp_c", "nodisp"):
    HOOKS[f"freeFonts_{v}"] = _h(0x0049FF30, "default", "void", [], f"c3cz_fonts_{v}", [()],
                                 extra=[(0x008408C8, 0, 0x60)])
for v in range(3):
    HOOKS[f"initPairArrayGlobal_{v}"] = _h(0x0049D010, "default", "pointer", [], f"c3cz_pairg_{v}", [()],
                                           extra=[(0x008406E8, 0, 0x20)])
for v in range(_LOCKS):
    HOOKS[f"Timer::lockInc_{v}"] = _h(0x00486FE0, "default", "int", [], f"c3cz_lock_{v}", [()],
                                      extra=[(0x0083AFEC, 0, 4)])
    HOOKS[f"Timer::lockDec_{v}"] = _h(0x00486FF0, "default", "void", [], f"c3cz_lock_{v}", [()],
                                      extra=[(0x0083AFEC, 0, 4)])
