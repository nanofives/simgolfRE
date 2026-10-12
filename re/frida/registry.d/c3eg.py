# C3 batch c3eg (round 29, 2026-10-12) of golf_clean.exe: window plumbing. Reimplementations: shim/src/re/c3eg.cpp. Fixtures:
# re/frida/js/fixtures.d/c3eg.js. Purposes and per-jcc tables: log/c3/c3eg_purpose.md.
#
# Keys freeze the game's first thread (re/frida/js/freeze.js) and swap a recording display into [0x0083ad50] where the
# function reaches it. Callees that run ORIGINAL (or their C3 hook) in both arms: Window::visible 0x004801f0, View::toParent
# 0x0047b170, View::offsetRectToParent 0x0047b0d0. Stood in (calleeStand, logged in $clog): Window::moveTo 0x0047b420,
# drawCursor 0x0047d130, Surface_blitThunk 0x00475c60, drawTooltip 0x0047cdb0.

EXE = "golf_clean.exe"

_REC = [("$rec", 0, 0x2000)]
_CLOG = [("$clog", 0, 0x400)]
_IMP = [("$implog", 0, 0x400)]
_P = ("pointer",)
_I = ("int",)


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- Window::invalidate
HOOKS["Window::invalidate"] = _h(0x00480CE0, "c3eg_inv", [(f"$w{i}",) for i in range(12)], _REC, args=_P, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- Surface_clip
_CLIP_STATE = _REC + _IMP + [(0x0083AA78, 0, 0x10), (0x0083AA98, 0, 4)]
_CLIP_V = [("$w0", "$q0"), ("$w0", 0), ("$w1", "$q1"), ("$w1", 0), ("$w2", "$q2"), ("$w2", 0), ("$w3", "$q3"), ("$w3", 0),
           ("$w4", "$q4"), ("$w4", 0), ("$w5", "$q5"), ("$w5", 0), ("$w6", 0), ("$w7", "$q3"), ("$w8", 0), ("$w9", 0),
           ("$w10", "$q1"), ("$w11", 0)]
HOOKS["Surface_clip"] = _h(0x00479A80, "c3eg_clip", _CLIP_V, _CLIP_STATE, args=_P * 2, abi="thiscall")
HOOKS["Surface_clip_fg"] = _h(0x00479A80, "c3eg_clip_fg", _CLIP_V, _CLIP_STATE, args=_P * 2, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- Window::layoutScrollbars
HOOKS["Window::layoutScrollbars"] = _h(0x0047D570, "c3eg_lsb", [(f"$w{i}",) for i in range(12)], _REC + _CLOG, args=_P,
                                       abi="thiscall")

# ---------------------------------------------------------------------------------------------------- EditBox_setCursorChar
_C = [0x7F00, 0x7F8A, 0x7EFF, 0x7F8B, 0, -1, -0x80000000, 0x7FFFFFFF, 0x7F85, 0x7F50, 0x7F01, 0x7F89, 0x7F83, 0x100, 0x7F00]
HOOKS["EditBox_setCursorChar"] = _h(0x0047AB00, "c3eg_ecc", [("$eblk", c) for c in _C], _CLOG + [("$eblk", 0, 0x300)],
                                    args=_P + _I, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- addDirtyRect
HOOKS["addDirtyRect"] = _h(0x0047D060, "c3eg_adr", [(f"$r{i}",) for i in range(12)],
                           _CLOG + _IMP + [(0x0083A4D8, 0, 0x10), (0x0083AAA0, 0, 0x30)], args=_I)

# ---------------------------------------------------------------------------------------------------- handlers
# Each row of the fixture (getter $t<i>) sets the globals and cells of one vector and returns its `this`. Stood in (calleeStand,
# logged in $clog): Panel::notify 0x0047e120, mouseDispatch23c 0x0047bf40, HotList::hitTest 0x00492a90, Window::click 0x0047c1b0;
# the release handlers stand in the four mouseDispatch bodies; onKeyChar stands in detach, show and Window::key.
_W3 = [("$wblk", 0, 3 * 0x500)]
_CELLS = [("$cells", 0, 0x100)]
_PRESS_ARGS = [(1, 10, 20, 0), (1, 10, 20, 1), (1, 5, 6, 0), (1, 5, 6, 1), (1, 7, 8, 0), (1, 9, 9, 1), (0, 3, 4, 0), (1, 3, 4, 1),
               (1, 1, 2, 0), (1, 2, 1, 1), (0x7FFFFFFF, -1, 0x7FFFFFFF, -5), (-3, -0x80000000, 0, 0)]
HOOKS["Window::onPressHot"] = _h(0x0047F010, "c3eg_press", [(f"$t{i}",) + a for i, a in enumerate(_PRESS_ARGS)],
                                 _CLOG + _REC + _W3 + _CELLS + [(0x0083AB40, 0, 0x10)], args=_P + _I * 4, abi="thiscall")
_REL_ARGS = [(50, 30, 1), (500, 30, 2), (0, 0, 3), (99, 49, 4), (50, 30, 5), (50, 30, 6), (50, 30, 7), (10, 10, 8), (100, 50, 9),
             (-1, -1, 10), (99, 50, 11), (50, 30, 12), (105, 30, 13), (50, 30, 14), (50, 30, 15)]
for _n, _a in (("150", 0x0047F0F0), ("158", 0x0047F230)):
    HOOKS[f"Window::onReleaseInner{_n}"] = _h(_a, f"c3eg_rel{_n}", [(f"$t{i}",) + a for i, a in enumerate(_REL_ARGS)],
                                              _CLOG + _W3 + [(0x0083AB40, 0, 8)], args=_P + _I * 3, abi="thiscall")
_KEY_ARGS = [0x67, 0x67, 0x41, 0xFF, 0x80, 0x7F, 0x67, 0x30, 0x31, 0x67, 0x1267, 0x20]
HOOKS["Window::onKeyChar"] = _h(0x0047EF30, "c3eg_key", [(f"$t{i}", c) for i, c in enumerate(_KEY_ARGS)],
                                _CLOG + _REC + [("$wblk", 0, 4 * 0x500)] + _CELLS +
                                [(0x0083ABA4, 0, 0xC), (0x004E42EC, 0, 4), (0x008400C8 + 0xA0, 0, 4), (0x0083AB88, 0, 4)],
                                args=_P + _I, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- relayoutClip / redrawDrawList
# relayoutClip is recursive: the original arm's calls to itself enter the hook too (the recursion is compared through the
# leaf-rooted vectors, whose original body runs without recursion). Window::visible, View::offsetRectToLocal and
# View::offsetRectToParent run ORIGINAL, IntersectRect is forwarded.
_RL_V = [("$t0", "$q5"), ("$t1", "$q0"), ("$t2", "$q1"), ("$t3", "$q0"), ("$t4", "$q0"), ("$t5", "$q2"), ("$t6", "$q3"),
         ("$t7", "$q1"), ("$t8", "$q4"), ("$t9", "$q5"), ("$t10", "$q3"), ("$t11", "$q5")]
HOOKS["relayoutClip"] = _h(0x0047E140, "c3eg_rl", _RL_V, _IMP + [("$wblk", 0, 8 * 0x500), ("$cells", 0, 0x100), (0x0083A7BC, 0, 4)],
                           args=_P * 2)
_RD_W = [0, 1, 0, 1, 5, 0, 1, 0, 3, 1, 0, 3, 2, 0]
HOOKS["redrawDrawList"] = _h(0x0047F8E0, "c3eg_rdl", [(f"$t{i}", f"$w{_RD_W[i]}") for i in range(14)],
                             _REC + _CLOG + _IMP + [("$wblk", 0, 6 * 0x500), (0x00839AC0, 0, 0x40), (0x0083AB90, 0, 4),
                                                    (0x0083AAE0, 0, 4), (0x0083AAA0, 0, 0x30)], args=_P * 2)

# ---------------------------------------------------------------------------------------------------- Window::show
# Stood in (calleeStand, $clog): Panel::notify 0x0047e120, rebuildDrawList 0x0047e450, Window::resized 0x0047bc60,
# Surface_clip 0x00479a80. View::offsetRectToLocal / offsetRectToParent run ORIGINAL.
_SHOW_F = [0, 0, 0, 4, 7, 1, 0, 0, 0, 0x104, 2, 3, 0]
HOOKS["Window::show"] = _h(0x0047B670, "c3eg_show", [(f"$t{i}", f) for i, f in enumerate(_SHOW_F)],
                           _CLOG + _REC + [("$wblk", 0, 6 * 0x500), (0x0083AB40, 0, 8), (0x0083AA98, 0, 4), (0x0083AD50, 0, 4)],
                           args=_P + _I, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- Window::mouseEvent238
# HotList::hitTestRect 0x00492b10, HotList::onMouseMove 0x00492cc0 and HotList::cancelTip 0x00492d40 are stood in ($clog). The two
# early exits return the entry eax (naked entry); the keys that take them compare it.
_ME_ARGS = [(3, 4, 0), (3, 4, 0), (5, 6, 0), (7, 8, 1), (7, 8, 1), (1, 2, 0), (1, 2, 0), (9, 9, 0), (9, 9, 0), (0x7FFFFFFF, -1, 0),
            (2, 3, -1), (0, 0, 0)]
HOOKS["Window::mouseEvent238"] = _h(0x0047BE40, "c3eg_me", [(f"$t{i}",) + a for i, a in enumerate(_ME_ARGS)],
                                    _CLOG + _REC + [("$wblk", 0, 4 * 0x500), ("$cells", 0, 0x100), (0x0083AB2C, 0, 4)],
                                    args=_P + _I * 3, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- ListBox::onMouse
# Stood in ($clog): ListModel::selectedId 0x00489950, ListBox::measure 0x0048a120 (c3eh's row: no hook is installed here, only a
# stand-in), ListBox::build 0x0048a5d0, setSelection 0x00489f50, Window::show 0x0047b670, fwdRange26c/270 and setField26c/270.
HOOKS["ListBox::onMouse"] = _h(0x0048C6E0, "c3eg_lbm", [(f"$t{i}",) for i in range(12)],
                               _CLOG + _REC + _IMP + [("$lb", 0, 12 * 0x800), ("$pb", 0, 12 * 0x200), ("$cells", 0, 0x100)],
                               args=_P, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- drawTooltip
# Stood in ($clog, 0x1000 bytes): Widget_fillRectR, Widget_drawBevelBoxR, Widget_fillArea, setDrawTarget, Widget_setQuad,
# drawCentered2, drawCentered3, Image_draw. The CRT strchr runs ORIGINAL. The live display is replaced by a recording one.
HOOKS["drawTooltip"] = _h(0x0047CDB0, "c3eg_tip", [(f"$t{i}",) for i in range(12)],
                          _REC + [("$clog", 0, 0x1000), ("$txtblk", 0, 0x400), (0x0083AABC, 0, 4), (0x0083AAC4, 0, 0x18),
                                  (0x0083AAA0, 0, 0x30), (0x0083A2C8, 0, 0x10), (0x0083A7BC, 0, 4), (0x0083AD50, 0, 4),
                                  (0x004E42C4, 0, 4), (0x004E42C0, 0, 4)], args=_P)

# ---------------------------------------------------------------------------------------------------- Window::dispatchAccel
# Window::key 0x0047c5d0 is stood in ($clog); MapVirtualKeyA goes through the import stand-in ($implog): odd codes map to 0, even
# ones to 0x41. The recursion (child windows) enters the hook in both arms.
_ACC_KEYS = [0x41, 0x41, 0x42, 0x65, 0x41, 0x41, 0x43, 0x45, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x41, 0x41, 0x60,
             0x69, 0x6A, 0x41, 0x42, 0x41, 0x41, 0x42, 0x42, 0x42, 0x42, 0x41, 0x41]
HOOKS["Window::dispatchAccel"] = _h(0x0047C6C0, "c3eg_acc", [(f"$t{i}", k) for i, k in enumerate(_ACC_KEYS)],
                                    _CLOG + _REC + _IMP + [("$wblk", 0, 3 * 0x500), ("$cells", 0, 0x100), (0x0083AB2C, 0, 4)],
                                    args=_P + _I, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- ListBox::typeAhead
# Rows 0..199 walk the arrow keys (0x25..0x28 and the keypad 0x64, 0x68, 0x66, 0x62) over a deterministic grid of geometries
# (fixture c3eg_ta), rows 200.. type characters; the key of row i is the getter $k<i>. Stood in ($clog): 0x0047b9f0, 0x0047ba10,
# ListModel::selectedId, setSelection, Window::invalidate; MapVirtualKeyA is stood in ($implog); CRT tolower runs ORIGINAL.
_TA_N = 200 + 6 + 22
HOOKS["ListBox::typeAhead"] = _h(0x0048AEA0, "c3eg_ta", [(f"$t{i}", f"$k{i}") for i in range(_TA_N)],
                                 _REC + [("$clog", 0, 0x1000)] + _IMP + [("$lb", 0, _TA_N * 0x800), ("$pb", 0, _TA_N * 0x200),
                                                                         ("$nodes", 0, _TA_N * 8 * 0x20), (0x0083AB2C, 0, 4),
                                                                         (0x0083B650, 0, 4)], args=_P + _I, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- drawCursor
# One row per vector (fixture c3eg_dc): $t<i> is the window (or 0), $b<i> the flag argument, $x<i> / $y<i> the position. Stood in
# ($clog, 0x1000 bytes): hitTestScreen, View::toGlobal, Surface_createFull, Surface_blitThunk, drawTooltip, addDirtyRect; the
# user32 preset stands in ShowCursor, LoadCursorA, SetCursor and forwards UnionRect ($implog).
_DC_N = 40
HOOKS["drawCursor"] = _h(0x0047D130, "c3eg_dc", [(f"$t{i}", f"$b{i}", f"$x{i}", f"$y{i}") for i in range(_DC_N)],
                         _REC + [("$clog", 0, 0x1000)] + _IMP + [("$wblk", 0, 3 * 0x500), ("$cells", 0, 0x100),
                                                                 (0x0083AB18, 0, 4), (0x0083AAA0, 0, 0x30), (0x0083A4D8, 0, 0x10),
                                                                 (0x0083AB4C, 0, 4), (0x0083ABA0, 0, 4), (0x0083A504, 0, 4),
                                                                 (0x0083AD50, 0, 4)], args=_P + _I * 3)
