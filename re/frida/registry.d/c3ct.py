# C3 batch c3ct (round 23, 2026-10-09) of golf_clean.exe UI: the EditBox caret family, scrollbarOnDrag and three
# ListBox handlers with their vtordisp thunks. Reimplementations in shim/src/re/c3ct.cpp, fixtures in
# re/frida/js/fixtures.d/c3ct.js, Purposes and per-jcc branch coverage in log/c3/c3ct_purpose.md.
#
# Every key freezes the game's first thread (re/frida/js/freeze.js): the functions write the drag-capture globals
# 0x0083ab30 / 0x0083ab34, 0x0083ab2c, 0x0083b650 and (through wrapText) 0x00839aa8.
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   EditBox::pixelToCharPos -> EditBox::wrapToLines 0x004862b0 (this batch), Widget_value 0x00477580 (c3c),
#                              drawMarkupRun 0x00477280 (c3o)
#   EditBox::begin/end/updateCaretDrag -> EditBox::pixelToCharPos 0x004863e0 (this batch)
#   ListBox::setSelection   -> Window::fwdScroll26c 0x0047b9f0 / fwdScroll270 0x0047ba10 (c3bx),
#                              ListModel::selectedId 0x00489950 (c3u), ListModel::idAt 0x00489a30 (c3v)
#   ListBox::onNavKey / onDrag -> ListBox::setSelection 0x00489f50 (this batch)
#   the two thunks          -> ListBox::onDrag 0x0048c8a0 / ListBox::onNavKey 0x0048aa00 (this batch)
# wrapToLines' wrapText 0x00476e20 and its callees (measureMarkupRun 0x00476ef0, wrapTextToWidth 0x00483980 (c3x),
# measureTextWidth 0x00483930 (c3ab)) and the Timer / invalidate callees are not hooked by this batch.

_EB_N = 6
_EB_STATE = ([(f"$eb{i}", 0, 0x700) for i in range(_EB_N)]
             + [(f"$ebch{i}", 0, 0x80) for i in range(_EB_N) if i != 1]
             + [("$tbuf", 0, 0x200), ("$lines0", 0, 0x40), ("$lines2", 0, 0x40), ("$lines5", 0, 0x40),
                ("$lbuf", 0, 0x100), ("$cnt", 0, 0x10), ("$rec", 0, 0x800),
                (0x0083AB30, 0, 4), (0x0083AB34, 0, 4), (0x00839AA8, 0, 4)])

_PTS = [(-5, -5), (0, 0), (7, 3), (20, 15), (33, 30), (60, 27), (90, 50), (150, 200), (1000, 1000), (12, -40),
        (45, 11), (3, 22)]
# lines == NULL: wrapToLines fills the local table (n is overwritten).
_PIX_WRAP = [(f"$eb{i}", x, y, 0, 1) for i in range(_EB_N) for (x, y) in _PTS]
# explicit tables: objects 0, 2 and 5 with their full n, and a shorter n so the clamp to n-1 bites earlier
_LINES_N = {0: 4, 2: 3, 5: 5}
_PIX_LINES = ([(f"$eb{i}", x, y, f"$lines{i}", n) for i, n in _LINES_N.items() for (x, y) in _PTS]
              + [(f"$eb{i}", x, y, f"$lines{i}", 1) for i in _LINES_N for (x, y) in _PTS[:6]]
              + [(f"$eb{i}", x, y, f"$lines{i}", 2) for i in _LINES_N for (x, y) in _PTS[3:9]])

_DRAG_PTS = [(0, 0), (25, 14), (70, 40), (-3, 500)]
_DRAG_VECTORS = [(f"$eb{i}", x, y) for i in range(4) for (x, y) in _DRAG_PTS]


def _eb(addr, nargs, fixture, vectors, ret="int"):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret=ret, args=["pointer"] + ["int"] * nargs,
                fixture=fixture, state=_EB_STATE, vectors=vectors)


def _eb_ptrs(addr, fixture, vectors):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret="int",
                args=["pointer", "int", "int", "pointer", "int"], fixture=fixture, state=_EB_STATE, vectors=vectors)


_SB_STATE = [(f"$sb{i}", 0, 0x40) for i in range(4)] + [("$rec", 0, 0x400), (0x0083AB30, 0, 4), (0x0083AB34, 0, 4)]
_SB_VECTORS = [(f"$sb{i}", a, b) for i in range(4) for (a, b) in ((0, 0), (5, -7), (0x7FFFFFFF, 1))]

# ---- ListBox ----------------------------------------------------------------------------------------------------
_LB_N = 16
_LB_SEL = [4, 6, 1, -1, 7, 0, 1, 2, 0, 0, 0, 0, -1, 0, -20, 0]   # the selected index M+0xf0 the fixture sets per ListBox
_LB_MULTI = [0, 1, 3, 4, 7, 12, 13, 14, 15]                  # ListBoxes with the multi-select flag T-0x50 set
_LB_STATE = ([(f"$blk{i}", 0, 0x800) for i in range(_LB_N)]
             + [(f"$aux{i}", 0, 0x1000) for i in range(_LB_N)]
             + [(f"$lst{i}", 0, 0x400) for i in range(_LB_N)]
             + [("$rec", 0, 0x2000), (0x0083AB2C, 0, 4), (0x0083B650, 0, 4), (0x0083B654, 0, 4)])

_SEL_VECTORS = [(f"$s{i}", idx, nfy) for i in range(_LB_N)
                for idx in sorted({-1, 0, 1, 3, 5, 8, 11, _LB_SEL[i]}) for nfy in (0, 1)]
_GRID_X = (-20, 0, 15, 45, 95)
_GRID_Y = (-10, 0, 9, 25, 47, 80, 200)
_GRID = [(x, y) for x in _GRID_X for y in _GRID_Y]


def _pts(base, objs, pts=_GRID):
    return [(f"${base}{i}", x, y) for i in objs for (x, y) in pts]


def _lb(addr, nargs, fixture, vectors, ret="int"):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret=ret, args=["pointer"] + ["int"] * nargs,
                fixture=fixture, state=_LB_STATE, vectors=vectors)


_SUB_PTS = [(0, 0), (15, 9), (45, 25), (95, 47), (-20, 80), (15, 200)]

HOOKS = {
    # EditBox::pixelToCharPos: wrapped (lines NULL) on both width variants, then the explicit line tables.
    "EditBox::pixelToCharPos": _eb_ptrs(0x004863E0, "c3ct_edit", _PIX_WRAP + _PIX_LINES),
    "EditBox::pixelToCharPos_b": _eb_ptrs(0x004863E0, "c3ct_edit_b", _PIX_WRAP),
    # EditBox::wrapToLines(lines, count): one call per object and width variant.
    "EditBox::wrapToLines": dict(module="golf_clean.exe", addr=0x004862B0, abi="thiscall", ret="int",
                                 args=["pointer", "pointer", "pointer"], fixture="c3ct_edit", state=_EB_STATE,
                                 vectors=[(f"$eb{i}", "$lbuf", "$cnt") for i in range(_EB_N)]),
    "EditBox::wrapToLines_b": dict(module="golf_clean.exe", addr=0x004862B0, abi="thiscall", ret="int",
                                   args=["pointer", "pointer", "pointer"], fixture="c3ct_edit_b", state=_EB_STATE,
                                   vectors=[(f"$eb{i}", "$lbuf", "$cnt") for i in range(_EB_N)]),
    # EditBox::setCaretActive: on and off on every object (the Timer at +0x5ac is idle).
    "EditBox::setCaretActive": _eb(0x00486B30, 1, "c3ct_edit",
                                   [(f"$eb{i}", on) for i in range(_EB_N) for on in (0, 1, -1, 0x7FFFFFFF)]),
    # The three caret-drag handlers under each capture setting (0x0083ab30, 0x0083ab34):
    #   none (0, 0), own ($eb0, 0), second ($eb1, $eb0), secondonly (0, $eb0), both ($eb0, $eb0).
    # updateCaretDrag has no "none" key: its skip side at 0x00486c54 is taken by $eb1..$eb3 under _own already.
    # Its _secondonly key returns 0 for every vector on purpose: it is the only one that takes 0x00486c58 (je).
    **{f"EditBox::{fn}{sfx}": _eb(addr, 2, f"c3ct_edit{sfx}", _DRAG_VECTORS)
       for fn, addr in (("beginCaretDrag", 0x00486B90), ("endCaretDrag", 0x00486BE0),
                        ("updateCaretDrag", 0x00486C40))
       for sfx in ("", "_own", "_second", "_secondonly", "_both")
       if not (fn == "updateCaretDrag" and sfx == "")},
    # scrollbarOnDrag under five capture settings ($sb0..$sb3): none, own, second, both, other.
    **{f"scrollbarOnDrag{sfx}": dict(module="golf_clean.exe", addr=0x00496D20, abi="thiscall", ret="int",
                                     args=["pointer", "int", "int"], fixture=f"c3ct_scroll{sfx}", state=_SB_STATE,
                                     vectors=_SB_VECTORS)
       for sfx in ("", "_own", "_second", "_both", "_other")},

    # ListBox::setSelection(S, index, notify) on $s0..$s7; the marked variant sets 0x0083b654 to 1.
    "ListBox::setSelection": _lb(0x00489F50, 2, "c3ct_list", _SEL_VECTORS),
    "ListBox::setSelection_marked": _lb(0x00489F50, 2, "c3ct_list_marked",
                                        [v for v in _SEL_VECTORS if v[2] == 1]),
    # ListBox::onDrag(T, x, y): void (no caller reads eax), state only.
    "ListBox::onDrag": _lb(0x0048C8A0, 2, "c3ct_list", _pts("t", range(_LB_N)), ret="void"),
    # ListBox::onNavKey(T, x, y) with Ctrl / Shift forced up or down through the shim's GetKeyState overlay.
    "ListBox::onNavKey": _lb(0x0048AA00, 2, "c3ct_list", _pts("t", range(_LB_N)), ret="void"),
    "ListBox::onNavKey_ctrl": _lb(0x0048AA00, 2, "c3ct_list_ctrl", _pts("t", _LB_MULTI), ret="void"),
    "ListBox::onNavKey_shift": _lb(0x0048AA00, 2, "c3ct_list_shift", _pts("t", _LB_MULTI), ret="void"),
    "ListBox::onNavKey_ctrlshift": _lb(0x0048AA00, 2, "c3ct_list_ctrlshift", _pts("t", _LB_MULTI), ret="void"),
    # The vtordisp thunks, entered with the Window base $w<i> (whose [W-4] is W - T).
    "ListBox::onDrag_vtordisp": _lb(0x0048CC80, 2, "c3ct_list", _pts("w", range(_LB_N), _SUB_PTS), ret="void"),
    "ListBox::onNavKey_vtordisp": _lb(0x0048CCB0, 2, "c3ct_list_shift", _pts("w", range(_LB_N), _SUB_PTS),
                                      ret="void"),
}


# ---- optionDialogOnMouse, listSelectionChanged, Timer_fire --------------------------------------------------------
_OD_STATE = [(f"$odblk{i}", 0, 0x400) for i in range(5)] + [("$rec", 0, 0x400)]
_OD_PTS = [(x, y) for x in (-15, 0, 9, 25, 61, 130) for y in (-9, 0, 7, 20, 44, 101)]
_LP_STATE = ([(f"$lpblk{i}", 0, 0x400) for i in range(5)] + [(f"$lplst{i}", 0, 0x200) for i in range(5)]
             + [("$rec", 0, 0x400), (0x0083AB2C, 0, 4)])
_LP_ARGS = ((0, 0), (3, -1), (0x7FFFFFFF, 9))
_TM_STATE = [(f"$tm{i}", 0, 0x40) for i in range(6)] + [("$rec", 0, 0x400), (0x0083AD4C, 0, 4), (0x0083AFF0, 0, 4)]

HOOKS.update({
    # optionDialogOnMouse(X, x, y) on $od0..$od4 (a leaf: 180 vectors); the thunk on the Window bases $odw<i>.
    "optionDialogOnMouse": dict(module="golf_clean.exe", addr=0x0049FDA0, abi="thiscall", ret="int",
                                args=["pointer", "int", "int"], fixture="c3ct_option", state=_OD_STATE,
                                vectors=[(f"$od{i}", x, y) for i in range(5) for (x, y) in _OD_PTS]),
    "optionDialogOnMouse_vtordisp": dict(module="golf_clean.exe", addr=0x0049FFB0, abi="thiscall", ret="int",
                                         args=["pointer", "int", "int"], fixture="c3ct_option", state=_OD_STATE,
                                         vectors=[(f"$odw{i}", x, y) for i in range(5) for (x, y) in _OD_PTS[::3]]),
    # listSelectionChanged(Y, a, b) on $lp0..$lp4; the thunk on $lpv<i> (= Y + 0x100, [p-4] = 0x100).
    "listSelectionChanged": dict(module="golf_clean.exe", addr=0x0049E640, abi="thiscall", ret="int",
                                 args=["pointer", "int", "int"], fixture="c3ct_panel", state=_LP_STATE,
                                 vectors=[(f"$lp{i}", a, b) for i in range(5) for (a, b) in _LP_ARGS]),
    "listSelectionChanged_vtordisp": dict(module="golf_clean.exe", addr=0x00491BA0, abi="thiscall", ret="int",
                                          args=["pointer", "int", "int"], fixture="c3ct_panel", state=_LP_STATE,
                                          vectors=[(f"$lpv{i}", a, b) for i in range(5) for (a, b) in _LP_ARGS]),
    # Timer_fire(timer), stdcall, on $tm0..$tm5 under four settings of (0x0083ad4c, 0x0083aff0).
    **{f"Timer_fire{sfx}": dict(module="golf_clean.exe", addr=0x00495CB0, abi="stdcall", ret="int", args=["pointer"],
                                fixture=f"c3ct_timer{sfx}", state=_TM_STATE, vectors=[(f"$tm{i}",) for i in range(6)])
       for sfx in ("", "_busy", "_owner0", "_owner3")},
})

# ---- the four slot bodies (0x0049fe20 and 0x0049fc30 have no hooks.csv row yet) and their vtordisp thunks -----------------------------------------------------------
_OD_STATE_FROZEN = _OD_STATE + [(0x0083AB2C, 0, 4)]
_ARGS2 = ((0, 0), (1, -1), (0x7FFFFFFF, 5))
_ARGS1 = (0, 7, -3)


def _od(addr, fixture, state, base, args):
    one = isinstance(args[0], int)
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret="int",
                args=["pointer"] + (["int"] if one else ["int", "int"]), fixture=fixture, state=state,
                vectors=[(f"${base}{i}", a) if one else (f"${base}{i}",) + tuple(a) for i in range(5) for a in args])


HOOKS.update({
    # bodies (entered with X / Y) and thunks (entered with the Window base W, [W-4] = W - X; or Y + 0x100 for the panel)
    "listPanelSlot60": _od(0x0049E7B0, "c3ct_panel", _LP_STATE, "lp", _ARGS1),
    "listPanelSlot60_vtordisp": _od(0x00491BB0, "c3ct_panel", _LP_STATE, "lpv", _ARGS1),
    "optionDialogSlot28": _od(0x0049FE20, "c3ct_option", _OD_STATE, "od", _ARGS2),
    "optionDialogSlot28_vtordisp": _od(0x0049FFC0, "c3ct_option", _OD_STATE, "odw", _ARGS2),
    "optionDialogSlot38": _od(0x0049FC30, "c3ct_option_frozen", _OD_STATE_FROZEN, "od", _ARGS2),
    "optionDialogSlot38_vtordisp": _od(0x0049FFF0, "c3ct_option_frozen", _OD_STATE_FROZEN, "odw", _ARGS2),
    "optionDialogSlot60": _od(0x0049FD80, "c3ct_option", _OD_STATE, "od", _ARGS1),
    "optionDialogSlot60_vtordisp": _od(0x004A0000, "c3ct_option", _OD_STATE, "odw", _ARGS1),
})

# ---- viewDispatchSlot27 0x004a0ab0 and its thunk 0x00491980 (fixture c3ct_view: option, panel and ListBox objects
# with a tag at V-8; $vx<k> carry tag 8 and tags with no case) ----------------------------------------------------
_VIEW_STATE = ([(f"$odblk{i}", 0, 0x400) for i in range(5)] + [(f"$lpblk{i}", 0, 0x400) for i in range(5)]
               + [(f"$lplst{i}", 0, 0x200) for i in range(5)]
               + [(f"$blk{i}", 0, 0x800) for i in range(_LB_N)] + [(f"$aux{i}", 0, 0x1000) for i in range(_LB_N)]
               + [(f"$lst{i}", 0, 0x400) for i in range(_LB_N)]
               + [("$vxblk", 0, 0x400), ("$rec", 0, 0x2000),
                  (0x0083AB2C, 0, 4), (0x0083B650, 0, 4), (0x0083B654, 0, 4)])
_VIEW_PTS = [(0, 0), (9, 7), (25, 20), (61, 44), (-15, 101), (130, -9), (45, 25)]
_VIEW_OBJS = ([f"vo{i}" for i in range(5)] + [f"vl{i}" for i in (0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)]
              + [f"vp{i}" for i in range(5)])
_VIEW_X = [f"vx{k}" for k in range(8)]


def _view(addr, sfx, objs, ret="int"):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret=ret, args=["pointer", "int", "int"],
                fixture="c3ct_view", state=_VIEW_STATE,
                vectors=[(f"${o}{sfx}", x, y) for o in objs for (x, y) in _VIEW_PTS])


HOOKS.update({
    "viewDispatchSlot27": _view(0x004A0AB0, "", _VIEW_OBJS + _VIEW_X),
    "viewDispatchSlot27_vtordisp": _view(0x00491980, "p", _VIEW_OBJS + _VIEW_X),
})
