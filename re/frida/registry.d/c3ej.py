# C3 batch c3ej (round 30, 2026-10-12) of golf_clean.exe UI: the view dispatchers of the TextView composite
# (viewCtor4a0740) with their vtordisp thunks, and the listPanel / optionDialog bodies, on REAL panels built by the game's
# own constructors through re/frida/js/uitree.js. Reimplementations in shim/src/re/c3ej.cpp; fixtures in
# re/frida/js/fixtures.d/c3ej.js; Purposes and per-jcc branch tables in log/c3/c3ej_purpose.md.
#
# Every key builds TextViews in the heap stand-in arena ($heap) while the first thread is frozen: UI_STATE compares that
# arena, the free log, the import log, the recording display's log and the window-tree globals (uitree.js GLOBALS).
# The dispatchers read the tag dword at (Window base - 8) = composite +0x1f4, which no instruction of the exe writes
# (log/c3/c3ej_notes.txt), so the fixtures write it: one TextView per tag.

EXE = "golf_clean.exe"

# re/frida/js/uitree.js: GLOBALS (4 bytes each), the z-list array, the recorder size
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000


def UI_STATE(arena):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


def _h(addr, fixture, vectors, arena, ret="int", args=("pointer",), abi="thiscall", extra=()):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture,
                state=UI_STATE(arena) + list(extra), vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- dispatchers
# c3ej_disp: ten composites (viewCtor4a0740) c0..c9 whose tag dwords (composite +0x1f4) are TAGS = 1, 2, 4, 8, 0x10, 0, 3, 0x11,
# 0x10001, 0xCDCDCDCD (every case of the byte table, the unmapped tags and the large values). wN = the Window base (composite
# +0x1fc, the `this` of the Window-table dispatchers), lN = the ListModel base (+0x774, viewDispatchBuild's `this`), cN the
# composite (viewDispatchOpen / Reset), lpN the listPanel part's Y; pwN / plN / plpN are scratch-block thunk arguments with a
# non-zero displacement back to wN / lN / lpN.
_DA = 0x18000
_N10 = range(10)
_AB = [(0, 0), (5, 7), (-1, 0x7FFFFFFF), (0x30, -2)]
_W = [f"$w{i}" for i in _N10]
HOOKS["viewDispatchSlot28"] = _h(0x004A0A10, "c3ej_disp", [(w, a, b) for w in _W for a, b in _AB[:3]], _DA, args=("pointer", "int", "int"))
HOOKS["viewDispatchSlot30"] = _h(0x004A0B50, "c3ej_disp", [(w, a, b) for w in _W for a, b in _AB[:3]], _DA, args=("pointer", "int", "int"))
HOOKS["viewDispatchSlot36"] = _h(0x004A0BF0, "c3ej_disp", [(w, a) for w in _W for a in (0x25, 0x28, 0x20)], _DA, args=("pointer", "int"))
HOOKS["viewDispatchDraw"] = _h(0x004A0D30, "c3ej_disp", [(w,) for w in _W], _DA)
HOOKS["viewDispatchOnScroll"] = _h(0x004A0ED0, "c3ej_disp", [(w, a, b) for w in _W for a, b in ((-2, 3), (-1, 9), (0, 4), (2, 0))], _DA,
                                   args=("pointer", "int", "int"))
HOOKS["viewDispatchBuild"] = _h(0x004A0EF0, "c3ej_disp", [(f"$l{i}", a, b, c, d) for i in _N10
                                                           for a, b, c, d in ((0, 0, 0, 0), (1, 2, 3, 4), (-1, 0x40, 7, 0))],
                                _DA, args=("pointer", "int", "int", "int", "int"))
HOOKS["viewDispatchOpen"] = _h(0x004A1250, "c3ej_disp", [(f"$c{i}", k) for i in _N10 for k in (0, 0x20, -1)], _DA, args=("pointer", "int"))
HOOKS["viewDispatchReset"] = _h(0x004A12E0, "c3ej_disp", [(f"$c{i}", k) for i in _N10 for k in (0, 0x20, -1)], _DA, args=("pointer", "int"))
HOOKS["listPanelSlot28"] = _h(0x0049E990, "c3ej_disp", [(f"$lp{i}", a, b) for i in _N10 for a, b in _AB[:2]], _DA,
                              args=("pointer", "int", "int"))

# vtordisp thunks of the dispatchers (sub ecx, [ecx-4]; jmp): `$wN` / `$lN` have the natural displacement 0 ([W-4] = 0), the
# `$pwN` / `$plN` / `$plpN` pointers are s+4 of a scratch block whose first dword is the displacement back to the target.
_PW = [f"$w{i}" for i in range(0, 10, 3)] + [f"$pw{i}" for i in _N10]
_PL = [f"$l{i}" for i in range(0, 10, 3)] + [f"$pl{i}" for i in _N10]
HOOKS["viewDispatchSlot28_vtordisp"] = _h(0x00491990, "c3ej_disp", [(w, a, b) for w in _PW for a, b in _AB[:2]], _DA, args=("pointer", "int", "int"))
HOOKS["viewDispatchSlot30_vtordisp"] = _h(0x004919B0, "c3ej_disp", [(w, a, b) for w in _PW for a, b in _AB[:2]], _DA, args=("pointer", "int", "int"))
HOOKS["viewDispatchSlot36_vtordisp"] = _h(0x00491A00, "c3ej_disp", [(w, a) for w in _PW for a in (0x25, 0x20)], _DA, args=("pointer", "int"))
HOOKS["viewDispatchDraw_vtordisp"] = _h(0x00491960, "c3ej_disp", [(w,) for w in _PW], _DA)
for _a, _nm in ((0x00491A30, "57"), (0x00491A40, "58")):
    HOOKS[f"viewDispatchOnScroll_vtordisp{_nm}"] = _h(_a, "c3ej_disp", [(w, a, b) for w in _PW for a, b in ((-2, 3), (2, 0))], _DA,
                                                      args=("pointer", "int", "int"))
HOOKS["viewDispatchBuild_vtordisp"] = _h(0x00491AA0, "c3ej_disp", [(l, a, b, c, d) for l in _PL for a, b, c, d in ((0, 0, 0, 0), (1, 2, 3, 4))],
                                         _DA, args=("pointer", "int", "int", "int", "int"))
HOOKS["listPanelSlot28_vtordisp"] = _h(0x00491B70, "c3ej_disp", [(f"$plp{i}", a, b) for i in _N10 for a, b in _AB[:2]], _DA,
                                       args=("pointer", "int", "int"))

# ---------------------------------------------------------------------------------------------------- listPanel part
# c3ej_lp_<v>: standalone listPanel parts p0..p3 (rows, layouts, parent callbacks per variant: fixture comment). `pN` = the
# part (vbptr at +0), `pN_Y` = part + 0x24 (the `this` of the Y-based bodies). All draw callees (Surface_blitThunk 0x00475c60,
# Image_draw 0x00473e60, drawCentered 0x00477cd0, drawRight2 0x004781f0, drawTextLines 0x004782d0, Widget_drawBoxR 0x00479670)
# are stood in with a recorder that logs every argument in $clog.
_LA = 0x10000
_CLOG = [("$clog", 0, 0x1000)]
_P = [0, 1, 2]
_DRAW_V = ([(f"$p{i}", ix, 0) for i in _P for ix in (-1, 0, 1, 2, 3, 4, 5, 6, 9)]
           + [(f"$p{i}", ix, 1) for i in _P for ix in (0, 1, 2, 3, 5)] + [("$p3", 0, 0), ("$p3", -1, 1), ("$p3", 2, 1)]
           + [("$p4", 0, 0), ("$p4", 1, 1), ("$p5", 0, 0), ("$p5", 2, 1), ("$p5", 4, 0)]
           + [("$p6", ix, 0) for ix in (-1, 0, 1, 2, 3, 5)] + [("$p6", ix, 1) for ix in (0, 2, 3)])
_ONSCROLL_V = [(f"$p{i}_Y", a, b) for i in _P for a, b in ((0, 0), (10, 1), (130, 30), (65, 15), (-5, 3), (500, 2), (7, 100))] + [("$p3_Y", 0, 5)]
_ONSCROLL_V += [("$p4_Y", 10, 1), ("$p4_Y", 0, 0), ("$p4_Y", 65, 15), ("$p4_Y", 130, 30), ("$p4_Y", 500, 2), ("$p5_Y", 130, 30), ("$p6_Y", 0, 0), ("$p6_Y", 10, 1), ("$p6_Y", 65, 15), ("$p6_Y", 130, 70)]
_REFRESH_V = [(f"$p{i}_Y", k) for i in _P for k in (0x24, 0x25, 0x26, 0x27, 0x28, 0x62, 0x64, 0x66, 0x68, 0x69, 0x41, 0x25, 0x27)] + [("$p3_Y", 0x28), ("$p4_Y", 0x28), ("$p6_Y", 0x25), ("$p6_Y", 0x28), ("$p6_Y", 0x26)]
for _v in "abc":
    _fx = f"c3ej_lp_{_v}"
    HOOKS[f"listPanelDraw_{_v}"] = _h(0x0049DAB0, _fx, _DRAW_V, _LA, ret="void", args=("pointer", "int", "int"), extra=_CLOG)
    HOOKS[f"listPanelOnScroll_{_v}"] = _h(0x0049E230, _fx, _ONSCROLL_V, _LA, args=("pointer", "int", "int"), extra=_CLOG)
    HOOKS[f"listPanelRefresh_{_v}"] = _h(0x0049E450, _fx, _REFRESH_V, _LA, args=("pointer", "int"), extra=_CLOG)
    HOOKS[f"listPanelDrawContents_{_v}"] = _h(0x0049E680, _fx, [(f"$p{i}_Y",) for i in range(7)], _LA, extra=_CLOG)
    HOOKS[f"listPanelDrawContents_vtordisp_{_v}"] = _h(0x00491B50, _fx, [(f"$p{i}_Y",) for i in range(7)], _LA, extra=_CLOG)
    HOOKS[f"listPanelOnScroll_vtordisp_{_v}"] = _h(0x00491B80, _fx, [(f"$p{i}_Y", 10, 1) for i in range(4)], _LA,
                                                    args=("pointer", "int", "int"), extra=_CLOG)
    HOOKS[f"listPanelRefresh_vtordisp_{_v}"] = _h(0x00491B90, _fx, [(f"$p{i}_Y", 0x28) for i in range(4)], _LA,
                                                   args=("pointer", "int"), extra=_CLOG)


HOOKS["listPanelDraw_d"] = _h(0x0049DAB0, "c3ej_lp_d", [(f"$p{i}", ix, fl) for i in (0, 1, 6) for ix, fl in ((0, 0), (1, 1), (2, 0), (-1, 0))],
                              _LA, ret="void", args=("pointer", "int", "int"), extra=_CLOG)

# ---------------------------------------------------------------------------------------------------- optionDialog part
# c3ej_od_<v>: standalone optionDialog parts p0..p3 (rows, layouts, parent / child slot answers per variant: fixture comment);
# `pN` = the part, `pN_X` = part + 0x1c (the `this` of the X-based bodies), `pN_T` = the ListModel base (optionDialogBuild's
# `this`). The drawing callees are stood in with a recorder logging every argument in $clog (fixture installStands).
_OL_V = ([(f"$p{i}", ix, 0, 0) for i in _P for ix in (-1, 0, 1, 2, 3, 4, 5, 9)]
         + [(f"$p{i}", ix, 1, 0) for i in _P for ix in (0, 1, 3, 4)] + [("$p3", 0, 0, 0), ("$p3", -1, 1, 0), ("$p3", 2, 1, 0), ("$p4", 0, 0, 0), ("$p4", 1, 1, 0), ("$p5", 0, 0, 0), ("$p5", 2, 1, 0)]
         + [("$p6", ix, 0, 0) for ix in (-1, 0, 1, 2, 3, 5)] + [("$p6", ix, 1, 0) for ix in (0, 2, 3)])
_UPD_V = ([(f"$p{i}_X", a, b) for i in _P for a, b in ((0, 0), (61, 0), (62, 0), (70, 16), (130, 48), (10, 80), (200, 5), (-10, 3), (31, 33))]
          + [("$p4_X", 0, 0), ("$p6_X", 0, 0), ("$p6_X", 45, 62), ("$p6_X", 91, 130), ("$p5_X", 62, 0)])
_KEY_V = ([(f"$p{i}_X", k) for i in _P for k in (0x20, 0x25, 0x26, 0x27, 0x28, 0x62, 0x64, 0x66, 0x68, 0x41, 0x69, 0x1F, 0x25, 0x26, 0x28)]
          + [("$p4_X", 0x25), ("$p4_X", 0x20), ("$p6_X", 0x25), ("$p6_X", 0x26), ("$p6_X", 0x28), ("$p6_X", 0x27)])
_ODC_V = [(f"$p{i}_X",) for i in range(7)]
for _v in "abc":
    _fx = f"c3ej_od_{_v}"
    HOOKS[f"optionListDraw_{_v}"] = _h(0x0049F370, _fx, _OL_V, _LA, ret="void", args=("pointer", "int", "int", "int"), extra=_CLOG)
    HOOKS[f"optionDialogUpdate_{_v}"] = _h(0x0049F9A0, _fx, _UPD_V, _LA, args=("pointer", "int", "int"), extra=_CLOG)
    HOOKS[f"optionDialogOnKey_{_v}"] = _h(0x0049FA90, _fx, _KEY_V, _LA, args=("pointer", "int"), extra=_CLOG)
    HOOKS[f"optionDialogDrawContents_{_v}"] = _h(0x0049FC60, _fx, _ODC_V, _LA, extra=_CLOG)
    HOOKS[f"optionDialogDrawContents_vtordisp_{_v}"] = _h(0x0049FFA0, _fx, _ODC_V, _LA, extra=_CLOG)
    HOOKS[f"optionDialogUpdate_vtordisp_{_v}"] = _h(0x0049FFD0, _fx, [(f"$p{i}_X", 10, 1) for i in range(4)], _LA,
                                                     args=("pointer", "int", "int"), extra=_CLOG)
    HOOKS[f"optionDialogOnKey_vtordisp_{_v}"] = _h(0x0049FFE0, _fx, [(f"$p{i}_X", 0x28) for i in range(4)], _LA,
                                                    args=("pointer", "int"), extra=_CLOG)

# optionDialogBuild: parents P (bit 12 of +0x9c set) / P2 (clear), positions and flag words; a NULL parent returns 3.
_OB_V = [("$p0_T", "$P", 5, 6, 0), ("$p1_T", "$P2", 7, 8, 0x4), ("$p2_T", "$P", -3, 9, 0x80), ("$p3_T", "$P2", 0, 0, 0x100000),
         ("$p0_T", 0, 1, 2, 3)]
_OBA = 0x30000
HOOKS["optionDialogBuild"] = _h(0x0049ED20, "c3ej_odb", _OB_V[:4] + _OB_V[4:], _OBA, args=("pointer", "pointer", "int", "int", "int"),
                                extra=_CLOG)
HOOKS["optionDialogBuild_vtordisp"] = _h(0x004A0040, "c3ej_odb", [("$p0_T", "$P", 5, 6, 0), ("$p1_T", "$P2", 7, 8, 0x4)], _OBA,
                                         args=("pointer", "pointer", "int", "int", "int"), extra=_CLOG)

# deleting destructors: d0..d5 optionDialog parts (rows / attached children on some), l0..l2 listPanel parts. Each part once.
_DA2 = 0xC000
HOOKS["optionDialogDeletingDtor"] = _h(0x0049FF60, "c3ej_oddtor", [(f"$d{i}_X", f) for i, f in zip(range(6), (0, 1, 3, 2, 1, 0))],
                                       _DA2, args=("pointer", "int"), extra=_CLOG)
HOOKS["optionDialogDeletingDtor_vtordisp"] = _h(0x0049FF50, "c3ej_oddtor", [(f"$d{i}_X", f) for i, f in zip(range(6), (1, 0, 1, 0, 1, 1))],
                                                _DA2, args=("pointer", "int"), extra=_CLOG)
HOOKS["listPanelDeletingDtor_vtordisp"] = _h(0x00491AC0, "c3ej_oddtor", [(f"$l{i}_W", f) for i, f in zip(range(3), (1, 0, 3))],
                                             _DA2, args=("pointer", "int"), extra=_CLOG)

# ---------------------------------------------------------------------------------------------------- compositeScreen
# c3ej_cs_<mode>: four windows w0..w3 (flags +0xa0 0 / 2 / 2 / 0, minimum sizes, rects: fixture comment), g_dragMode = mode, origin
# globals (3, 4); the pointer (x, y) is converted by View::fromParent (flags bit 1 clear) or View::toGlobal, so the point
# that yields dx = dy = 0 is (base + origin): w0 (16, 28), w1 (8, 10), w2 (3, 4), w3 (14, 18). Vectors move the pointer by
# ddx in (-30, -5, 0, 7, 40) and ddy in (-12, 0, 9) from there. Callees run real: relayoutClip, redrawDrawList, drawTooltip,
# View::fromParent / toGlobal / offsetRectToParent / ToLocal, Window::adjustSizeOuter, UnionRect.
_CSG = [(a, 0, 4) for a in (0x0083AB1C, 0x0083AB20, 0x0083AB24, 0x0083AB28, 0x0083AB4C, 0x0083AB18)]
_CS_BASE = {0: (16, 28), 1: (8, 10), 2: (3, 4), 3: (14, 18)}
_CS_V = [(f"$w{i}", bx + ddx, by + ddy) for i, (bx, by) in _CS_BASE.items()
         for ddx in (-30, -5, 0, 7, 40) for ddy in (-12, 0, 9)]
_CSA = 0x10000
for _m in (2, 3, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18):
    HOOKS[f"compositeScreen_m{_m}"] = _h(0x0047FAB0, f"c3ej_cs_{_m}", _CS_V, _CSA, args=("pointer", "int", "int"),
                                         extra=_CLOG + _CSG)

# uiPointerMove: hitTestScreen / drawCursor / compositeScreen stood in (fixture comment); windows w0..w2 with recording slots.
_UP_V = [(-5, 3), (50, 10), (150, 20), (250, 30), (99, 1), (100, 2), (199, 3), (200, 4), (0, 0), (-1, -1)]
for _v in "abcdef":
    HOOKS[f"uiPointerMove_{_v}"] = _h(0x004959B0, f"c3ej_up_{_v}", _UP_V, 0x8000, args=("int", "int"), abi="stdcall",
                                      extra=_CLOG + _CSG)

HOOKS["optionListDraw_d"] = _h(0x0049F370, "c3ej_od_d", [(f"$p{i}", ix, st, 0) for i in (0, 1, 6) for ix, st in ((0, 0), (1, 1), (2, 0), (-1, 0))],
                               _LA, ret="void", args=("pointer", "int", "int", "int"), extra=_CLOG)
# allocation failure inside Window::create (the +0x568 Image, 0x0048077b): optionDialogBuild returns the non-zero result
HOOKS["optionDialogBuild_f"] = _h(0x0049ED20, "c3ej_odb_f", [("$p0_T", "$P", 5, 6, 0), ("$p1_T", "$P2", 7, 8, 4)], _OBA,
                                  args=("pointer", "pointer", "int", "int", "int"), extra=_CLOG)
# viewDispatchOpen / Reset with the n-th allocation after the build failing: the setKey / reset error return of stage n
for _n in range(6):
    HOOKS[f"viewDispatchOpen_f{_n}"] = _h(0x004A1250, f"c3ej_dispf_{_n}", [(f"$c{i}", 0x20) for i in (0, 2, 4, 5)], _DA, args=("pointer", "int"))
    HOOKS[f"viewDispatchReset_f{_n}"] = _h(0x004A12E0, f"c3ej_dispf_{_n}", [(f"$c{i}", 0x20) for i in (0, 2, 4, 5)], _DA, args=("pointer", "int"))
