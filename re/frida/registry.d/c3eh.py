# C3 batch c3eh (round 29, 2026-10-12) of golf_clean.exe: drawing bodies on synthetic objects and a recording display.
# Reimplementations: shim/src/re/c3eh.cpp. Fixtures: re/frida/js/fixtures.d/c3eh.js (generated from log/c3/c3eh_parts).
# Purposes and per-jcc tables: log/c3/c3eh_purpose.md.
#
# Every key freezes the game's first thread (re/frida/js/freeze.js). Callees that run ORIGINAL (or their C3 hook in the main
# build, switched off in the callee-off pass): drawMapLine 0x004493d0 (c3dw).

EXE = "golf_clean.exe"
_P = "pointer"

_REC = [("$rec", 0, 0x2000), ("$cnt", 0, 0x80)]
_CLOG = ("$clog", 0, 0x1000)
_IMPLOG = ("$implog", 0, 0x400)


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- drawMinimapSegment
_W = [0, 1, -1, 5, 0x7FFFFFFF, -0x80000000, 100, 639, 7, 0x1234, -77, 3]
_A7 = [tuple(_W[(i + k * 3) % 12] for k in range(7)) for i in range(12)]
HOOKS["drawMinimapSegment"] = _h(0x00406220, "c3eh_dms", _A7, [_IMPLOG, (0x00820ED0, 0, 4)], args=("int",) * 7)

# ---------------------------------------------------------------------------------------------------- comboSetItems
_CS = _REC + [_CLOG] + [(f"$o{i}", 0, 0x3600) for i in range(8)]
HOOKS["comboSetItems"] = _h(0x00493EF0, "c3eh_cs", [(f"$o{i}", f"$s{j}") for i in range(8) for j in range(5)], _CS,
                            args=(_P, _P), abi="thiscall")

# ---------------------------------------------------------------------------------------------------- drawCircle
# Callees that run ORIGINAL (their C3 hooks are switched off in the main build's callee-off pass): foldRange 0x00467270, absDiff
# 0x004672b0, worldToScreen 0x0042fb90, setLinePoint 0x0040bf00, lineTo 0x0040bf20, drawMapLine 0x004493d0 (-> Terrain::drawLine stood in).
_CIRC_G = [(a, 0, 4) for a in (0x004C2BA0, 0x004C2BA4, 0x004C2844, 0x00822C8C, 0x00822C90, 0x005685F4, 0x004C2E00, 0x005A9CC0)]
_CIRC_STATE = [("$implog", 0, 0x1000), (0x00820ED0, 0, 4), (0x005A412C, 0, 8)] + _CIRC_G
_RAD = [0, 5, 10, 25, 60, 120, 250, -25]
HOOKS["drawCircle"] = _h(0x00407C60, "c3eh_circ",
                         [(0x5000 + 37 * r, 0x5400 + 41 * r, rad, f"$r{r}") for r in range(10) for rad in _RAD],
                         _CIRC_STATE, args=("int",) * 4)

# ---------------------------------------------------------------------------------------------------- drawWindowFrame
# blitPanel 0x0040ca10 and Surface_stretchBlit 0x00473f60 are stood in (calls in $clog); the display slot 0x9c is recorded in $rec.
_WF = _REC + [_CLOG]
_WFV = [(x, y, w, h, f) for (x, y) in [(0, 0), (100, 50), (-7, 300)] for w in [0, 8, 16, 17, 31, 32, 33, 47, 48, 63, 100, 160]
        for h in [16, 20, 31] for f in [0, 1]][::5]
_WFV += [(10, 20, 64, 32, 1), (10, 20, 64, 32, 0), (10, 20, 65, 33, 1), (10, 20, 47, 15, 1), (0, 0, 16, 0, 1), (5, 5, 28, 28, 1),
         (5, 5, 29, 28, 0), (5, 5, 144, 17, 1), (-30, -40, 200, 100, 1)]
HOOKS["drawWindowFrame"] = _h(0x0040CDD0, "c3eh_wf", _WFV, _WF, args=("int",) * 5)
HOOKS["drawWindowFrame_nodisp"] = _h(0x0040CDD0, "c3eh_wf_nodisp", _WFV[::2], _WF, args=("int",) * 5)

# ---------------------------------------------------------------------------------------------------- buildControlStrip47d610
HOOKS["buildControlStrip47d610"] = _h(0x0047D610, "c3eh_bcs", [(f"$o{i}",) for i in range(18)],
                                      _REC + [_CLOG] + [(f"$o{i}", 0, 0x300) for i in range(18)], args=(_P,), abi="thiscall")

# ---------------------------------------------------------------------------------------------------- panelBuildChildren
# Callees that run ORIGINAL (their C3 hooks are switched off in the main build's callee-off pass): setQuad 0x00476310, setQuad70
# 0x00476340, setQuad74 0x00476370, setDrawTarget 0x004762d0, Text_put 0x00477250, Widget_value 0x00477580.
_NP = 20
HOOKS["panelBuildChildren"] = _h(0x004A2A60, "c3eh_pbc", [(f"$o{i}",) for i in range(_NP)],
                                 [("$rec", 0, 0x2000), ("$E", 0, _NP * 0x200), ("$BK", 0, _NP * 0x200), ("$tm", 0, 0x300)],
                                 args=(_P,), abi="thiscall")

# ---------------------------------------------------------------------------------------------------- ListBox::drawContents (+ vtordisp)
# Callees stood in (calls and the bevel rectangles in $clog): drawNinePatch 0x004791b0, Surface_blitThunk 0x00475c60, Window::paint
# 0x004808c0, Widget_drawBevelBoxR 0x004795d0. The thunk's keys enter at T + 0x20 ([T + 0x1c] = 0x20).
_LBN = 24
_LB = [("$rec", 0, 0x2000), _CLOG, ("$cell", 0, 16), ("$arena", 0, _LBN * 0x1000), (0x0083AB2C, 0, 4), (0x0083B650, 0, 4)]
HOOKS["ListBox::drawContents"] = _h(0x0048C130, "c3eh_lb", [(f"$o{i}",) for i in range(_LBN)], _LB, args=(_P,), abi="thiscall")
HOOKS["ListBox::drawContents_vtordisp"] = _h(0x0048CC60, "c3eh_lb", [(f"$u{i}",) for i in range(_LBN)], _LB, args=(_P,), abi="thiscall")

# ---------------------------------------------------------------------------------------------------- ListBox::measure
# Callees that run ORIGINAL (their C3 hooks are switched off in the main build's callee-off pass): setQuad 0x00476310, setQuad70
# 0x00476340, setQuad74 0x00476370, setDrawTarget 0x004762d0, Text_put 0x00477250, Widget_value 0x00477580. [0x0083ff10] is set per key.
_MSN = 120
_MS = [("$rec", 0, 0x2000), ("$MB", 0, _MSN * 0x1000), (0x0083FF10, 0, 4)]
HOOKS["ListBox::measure"] = _h(0x0048A120, "c3eh_ms_a", [(f"$o{i}",) for i in range(_MSN)], _MS, args=(_P,), abi="thiscall")
HOOKS["ListBox::measure_b"] = _h(0x0048A120, "c3eh_ms_b", [(f"$o{i}",) for i in range(_MSN)], _MS, args=(_P,), abi="thiscall")

# ---------------------------------------------------------------------------------------------------- drawScrollText
# Stood in (calls in $clog): drawFrameS2 0x0040d0b0, drawRectScaled 0x0046e710, drawText4049d0 0x004049d0, drawTextScaled 0x00404a20,
# Window::refreshFull 0x00480c80. Run ORIGINAL (their C3 hooks are switched off in the main build's callee-off pass): scaleX 0x00404970,
# clamp 0x00467130, Text_put 0x00477250. The second argument is a row's selection value ($rN).
_SC = [("$rec", 0, 0x2000), ("$tm", 0, 0x300), ("$TB", 0, 10 * 0x400), _CLOG, (0x00838EC4, 0, 0xD8), (0x0083927C, 0, 4), (0x00839260, 0, 4)]
HOOKS["drawScrollText"] = _h(0x0046E260, "c3eh_sc", [(f"$s{k}", f"$r{i}") for k in range(10) for i in range(10)], _SC,
                             args=(_P, "int"))

# ---------------------------------------------------------------------------------------------------- drawMatchScoreboard
# Stood in (calls in $clog): setDrawTarget 0x004762d0, drawText4049d0 0x004049d0, drawCenteredText 0x00404b70, buildGolferName
# 0x004676e0, Text_put 0x00477250. Run ORIGINAL (C3 hook switched off in the callee-off pass): _itoa 0x004ad425, appendNumber 0x0042dc00.
_SB = [_CLOG, (0x0051A068, 0, 0x100), (0x00824134, 0, 0x20)]
HOOKS["drawMatchScoreboard"] = _h(0x00461110, "c3eh_sb", [(f"$r{i}",) for i in range(14)], _SB, args=("int",))

# ---------------------------------------------------------------------------------------------------- drawPanelContents
# Stood in (calls in $clog): blitMid 0x00475fb0, Widget_clearArea 0x00478b30, Widget_drawBevelBoxR 0x004795d0, setQuad 0x00476310,
# setDrawTarget 0x004762d0, Widget_fillRectR 0x00475b00, drawCentered 0x00477cd0, drawRight2 0x004781f0, drawCentered3 0x00477eb0.
# Objects 1 and 2 leave through the early exits: the original leaves eax = 0 there under the harness (measured), as the body does.
_PCN = 17
_PCW = [-1, 0, 1, 2, 3, 9]
_PC = [("$rec", 0, 0x2000), _CLOG, ("$cell", 0, 16), ("$OBJ", 0, _PCN * 0x300)]
HOOKS["drawPanelContents"] = _h(0x0047D850, "c3eh_pc", [(f"$o{i}", w) for i in range(_PCN) for w in _PCW], _PC,
                                args=(_P, "int"), abi="thiscall")
