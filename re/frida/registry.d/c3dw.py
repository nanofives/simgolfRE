# C3 batch c3dw (round 28, 2026-10-11) of golf_clean.exe: rows earlier rounds dropped for heap use, imports, live globals or
# file I/O. Reimplementations: shim/src/re/c3dw.cpp. Fixtures: re/frida/js/fixtures.d/c3dw.js. Purposes and per-jcc tables:
# log/c3/c3dw_purpose.md.
#
# Every key freezes the game's first thread (re/frida/js/freeze.js), swaps a recording display into [0x0083ad50] and sets the
# globals of the vector's row through a "$rN" getter (c3dr's trick). Callees that run ORIGINAL (or their C3 hook) in both arms:
# showScreen 0x00483bb0, hideScreen 0x00483c10, redrawAll 0x00483c70, frameTick 0x00483cd0, enterModal 0x00483d30 (c3dq),
# serviceMovie 0x0049c8e0, gfxFlushA/B (inlined by the bodies). Hooked inside this batch and called by other bodies of it:
# pumpInput483c30 <- drainAndRedraw, flushRedraw; pumpInput483bd0, pumpMessages, flushRedraw <- pumpLight;
# drainAndRedraw <- waitRedrawReady.

EXE = "golf_clean.exe"

_REC = [("$rec", 0, 0x2000), ("$cnt", 0, 0x80)]
_NET = [(0x008400B0, 0, 0x18), (0x0083AD4C, 0, 4)]
_LOG = [("$implog", 0, 0x400)]


def _g(*addrs):
    return [(a, 0, 4) for a in addrs]


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


def _rows(n):
    return [(f"$r{i}",) for i in range(n)]


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- resetScrollText
HOOKS["resetScrollText"] = _h(0x00465560, "c3dw_rst", _rows(12), _REC + _NET + _g(0x00822D68, 0x00822B9C, 0x00822B98),
                              args=("int",))

# ---------------------------------------------------------------------------------------------------- drainAndRedraw
_FL = _REC + _NET + _g(0x00822B9C, 0x00822B98, 0x004D3834)
for _k in ("h0", "h1", "h3"):
    HOOKS[f"drainAndRedraw_{_k}"] = _h(0x0045AF00, f"c3dw_drain_{_k}", _rows(12), _FL, args=("int",))
for _k in ("h0", "h2"):
    HOOKS[f"flushRedraw_{_k}"] = _h(0x0045AE70, f"c3dw_flush_{_k}", _rows(12), _FL, args=("int",))
for _k in ("t0", "t2"):
    HOOKS[f"waitRedrawReady_{_k}"] = _h(0x0045AED0, f"c3dw_wait_{_k}", _rows(12), _FL + _g(0x00822D68), args=("int",))

# ---------------------------------------------------------------------------------------------------- pumpInput483bd0 / c30
for _k in ("s0", "s1", "s3"):
    HOOKS[f"pumpInput483bd0_{_k}"] = _h(0x00483BD0, f"c3dw_pi_bd0_{_k}", _rows(4), _REC + _NET, args=("int",))
    HOOKS[f"pumpInput483c30_{_k}"] = _h(0x00483C30, f"c3dw_pi_c30_{_k}", _rows(4), _REC + _NET, args=("int",))

for _k in ("n0", "n1"):
    HOOKS[f"pumpInput483bd0_{_k}"] = _h(0x00483BD0, f"c3dw_pi_bd0_{_k}", _rows(4), _REC + _NET + [("$clog", 0, 0x400)], args=("int",))
    HOOKS[f"pumpInput483c30_{_k}"] = _h(0x00483C30, f"c3dw_pi_c30_{_k}", _rows(4), _REC + _NET + [("$clog", 0, 0x400)], args=("int",))
HOOKS["waitRedrawReady_rep"] = _h(0x0045AED0, "c3dw_wait_rep", _rows(12), _FL + _g(0x00822D68) + [("$clog", 0, 0x400)], args=("int",))

# ---------------------------------------------------------------------------------------------------- pumpMessages / pumpLight
_PM = _REC + _NET + _LOG + _g(0x00822D68, 0x00822D74)
for _k in ("k0", "k1", "k3"):
    HOOKS[f"pumpMessages_{_k}"] = _h(0x0045C030, f"c3dw_pm_{_k}", _rows(8), _PM, args=("int",))
_PL = _PM + _g(0x004D3834, 0x00822B9C, 0x00822B98)
for _k in ("a", "b"):
    HOOKS[f"pumpLight_{_k}"] = _h(0x0045C150, f"c3dw_pl_{_k}", _rows(6), _PL, args=("int",))

HOOKS["pumpLight_c"] = _h(0x0045C150, "c3dw_pl_c", _rows(6), _PL + [("$clog", 0, 0x400)], args=("int",))

# ---------------------------------------------------------------------------------------------------- Terrain glue
# The exe's Terrain.dll calls go through import thunks (jmp [IAT slot]): the slots are stood in (importstandin.js, thiscall)
# and the Terrain object [0x00820ed0] is our own block. Call logs ($implog) and the call log of the stood-in exe callees
# ($clog) are compared state.
_TER = [("$implog", 0, 0x400), (0x00820ED0, 0, 4)]
_W = [0, 1, -1, 5, 0x7FFFFFFF, -0x80000000, 100, 639, 7, 0x1234, -77, 3]
_A7 = [tuple(_W[(i + k * 3) % 12] for k in range(7)) for i in range(12)]
HOOKS["drawMapLine"] = _h(0x004493D0, "c3dw_dml", _A7, _TER, args=("int",) * 7)
_A5 = [tuple(_W[(i + k * 5) % 12] for k in range(5)) for i in range(12)]
HOOKS["lineTo"] = _h(0x0040BF20, "c3dw_lt", _A5, _TER + [(0x005A412C, 0, 8)], args=("int",) * 5)
HOOKS["refreshTerrain"] = _h(0x00449520, "c3dw_rt", _rows(8), _TER + [("$clog", 0, 0x400), (0x00820F2A, 0, 1)], args=("int",))
HOOKS["refreshTerrain_noterrain"] = _h(0x00449520, "c3dw_rt_n", _rows(8),
                                       _TER + [("$clog", 0, 0x400), (0x00820F2A, 0, 1)], args=("int",))
_FT = _TER + [("$arena", 0, 0x1010), ("$freelog", 0, 0x404)]
HOOKS["freeTerrain"] = _h(0x00449860, "c3dw_ft", [()], _FT, ret="void")
HOOKS["freeTerrain_none"] = _h(0x00449860, "c3dw_ft_none", [()], _FT, ret="void")
_CT_STATE = _TER + [(0x00578372, 0, 0x454), (0x005A34E0, 0, 1)]
HOOKS["applyCourseType"] = _h(0x00449400, "c3dw_act", [(f"$r{i}",) for i in range(12)], _CT_STATE, args=("int",))

# ---------------------------------------------------------------------------------------------------- window glue
# Windows are 0x300-byte blocks of ours ($wblk, 16 of them: $w0..$w15) with a recording vtable; the exe callees are stood in
# (calleeStand, logged in $clog) or run over these objects. Hooked inside this batch and called by other bodies of it:
# Panel::notify -> bringToFront / zRaise; uiKeyEvent / uiKeyEvent148 -> currentFocusOwner (the keys are re-run with them off).
_WIN = [("$rec", 0, 0x2000), ("$cnt", 0, 0x80), ("$clog", 0, 0x400)]
_WINBLK = [("$wblk", 0, 0x3000), ("$arrs", 0, 0x200)]
_ZG = [(0x0083A2D8, 0, 0x80), (0x0083AB94, 0, 4)]
_P = ("pointer",)
_I = ("int",)

_a = [0x41, 0x42, 0x43, 0, -1, 0x7FFFFFFF, 5, -77, 0x1234, 0x42, 0x41, 639]
HOOKS["Window::fwdMouseEvent238"] = _h(
    0x0047EFF0, "c3dw_fwd238", [("$w0", _a[i], _a[(i + 5) % 12], _a[(i + 7) % 12]) for i in range(12)], _WIN + _WINBLK,
    args=_P + _I * 3, abi="thiscall")

_BTF = [("$p0", "$k0"), ("$p0", "$k1"), ("$p0", "$k3"), ("$p0", "$k5"), ("$p0", "$other"), ("$p1", "$k0"), ("$p1", "$k4"),
        ("$p2", "$k3"), ("$p2", "$k1"), ("$p3", "$k2"), ("$p0", "$k2"), ("$p0", "$k4")]
HOOKS["ChildArray::bringToFront"] = _h(0x0047E700, "c3dw_btf", _BTF, _WIN + _WINBLK, args=_P * 2, abi="thiscall")
_BTF_E = [("$p0", 0), ("$p0", "$flagged"), ("$p1", 0), ("$p3", "$flagged"), ("$p2", "$flagged"), ("$p3", 0), ("$p0", "$k3"), ("$p0", "$k5")]
HOOKS["ChildArray::bringToFront_early"] = _h(0x0047E700, "c3dw_btf_early", _BTF_E, _WIN + _WINBLK, ret="void",
                                             args=_P * 2, abi="thiscall")

HOOKS["zRaise"] = _h(0x0047E580, "c3dw_zr", [(f"$r{i}",) for i in range(12)], _WIN + _ZG, args=_P)
HOOKS["zRaise_early"] = _h(0x0047E580, "c3dw_zr_early", [(f"$r{i}",) for i in range(6)], _WIN + _ZG, ret="void", args=_P)

_PN = [("$k0",), ("$k1",), ("$k3",), ("$k5",), ("$l0",), ("$l1",), ("$l2",), ("$l3",), ("$k0p3",), ("$flagged",)]
HOOKS["Panel::notify"] = _h(0x0047E120, "c3dw_pn", _PN, _WIN + _WINBLK + _ZG, args=_P, abi="thiscall")

_CMD = [(0x41, 0), (0x41, 1), (0x42, 0), (0x42, 5), (0x43, 0), (0x43, 9), (0, 0), (0, 3), (-1, -1), (0x7FFFFFFF, 0),
        (0x42, -0x80000000), (0x41, -1)]
_CMDS = _WIN + [(0x008400B8, 0, 4), (0x0083AB88, 0, 4)]
HOOKS["Window::onCommand"] = _h(0x0047EE70, "c3dw_cmd", [("$w0", a, b) for a, b in _CMD], _CMDS, args=_P + _I * 2,
                                abi="thiscall")
HOOKS["Window::onCommand_nonet"] = _h(0x0047EE70, "c3dw_cmd_nonet", [("$w0", a, b) for a, b in _CMD], _CMDS,
                                      args=_P + _I * 2, abi="thiscall")

_SF = []
for _w in ("$pw", "$np", "$fl", "$fl2"):
    for _k in range(3):
        _SF.append((_w, (1, 0, 5)[_k], (0, 7, -3)[_k], (9, 0x1234, 0)[_k], (0, 1, 2)[_k]))
HOOKS["Window::setFocus"] = _h(0x0047F1B0, "c3dw_sf", _SF, _WIN + _WINBLK + [(0x0083AB40, 0, 0xC)], args=_P + _I * 4,
                               abi="thiscall")

_RR = []
for _w in ("$v", "$iv", "$vp", "$vpi"):
    for _k in range(3):
        _RR.append((_w, (1, -5, 0x7FFFFFFF)[_k], (2, 9, 5)[_k], (30, 0, 0x7FFFFFFF)[_k], (40, -2, 1)[_k], (0, 1, 2)[_k]))
HOOKS["Window::refreshRect"] = _h(0x00480C20, "c3dw_rr", _RR, _WIN + _WINBLK, args=_P + _I * 5, abi="thiscall")

_FOG = [(0x0083AB54, 0, 4), (0x0083B990, 0, 4), (0x0083AB38, 0, 4), (0x0083AB60, 0, 4), (0x0083AB3C, 0, 4)]
HOOKS["currentFocusOwner"] = _h(0x0047F2F0, "c3dw_cfo", [(f"$r{i}",) for i in range(12)], _WIN + _FOG, ret="pointer",
                                args=_I)
HOOKS["uiKeyEvent148"] = _h(0x00495990, "c3dw_uk148", [(f"$r{i}",) for i in range(12)], _WIN + _FOG, args=_I, abi="stdcall")
_A2 = [1, 0, 5, 1, 0, 3, 2, 0, 1, 7, 0, 9]
HOOKS["uiKeyEvent"] = _h(0x00495910, "c3dw_uk", [(f"$r{i}", _A2[i]) for i in range(12)],
                         _WIN + _FOG + [("$implog", 0, 0x400), (0x0083AAFC, 0, 8)], args=_I * 2, abi="stdcall")

_BOX = [tuple(_W[(i + k * 2) % 12] for k in range(6)) for i in range(12)]
HOOKS["drawBox"] = _h(0x00477E60, "c3dw_box", _BOX, _WIN, args=_I * 6, abi="stdcall")

_TV = [(f"$v{i}", _W[i], _W[(i + 5) % 12]) for i in range(12)]
HOOKS["TextView_beginMeasure"] = _h(0x004785E0, "c3dw_tvbm", _TV,
                                    _WIN + [("$views", 0, 0x80 * 12), (0x00839AA0, 0, 12)], args=_P + _I * 2, abi="thiscall")

# ---------------------------------------------------------------------------------------------------- display glue
# scene(): a recording display in [0x0083ad50]; a slot whose result a row chooses returns the word at $cell.
_DG = _WIN + [("$cell", 0, 0x40)]
HOOKS["clearPrimarySurface"] = _h(0x00483B10, "c3dw_cps", _rows(12), _DG + [(0x0083AD44, 0, 4)], args=("int",))
HOOKS["clearPrimarySurface_nodisp"] = _h(0x00483B10, "c3dw_cps_nodisp", _rows(12), _DG + [(0x0083AD44, 0, 4)], ret="void",
                                         args=("int",))
HOOKS["resetScreenState"] = _h(0x00483320, "c3dw_rss", _rows(12), _DG + [(0x0083AC90, 0, 0x14)], args=("int",))
HOOKS["pumpInputDisplay483da0"] = _h(0x00483DA0, "c3dw_pid", _rows(12), _DG + [(0x0083AD4C, 0, 4)], args=("int",))
HOOKS["pumpInputDisplay483da0_nodisp"] = _h(0x00483DA0, "c3dw_pid_nodisp", _rows(12), _DG + [(0x0083AD4C, 0, 4)],
                                            ret="void", args=("int",))
HOOKS["Cache483030::attach"] = _h(0x00483030, "c3dw_cat", [(f"$r{i}",) for i in range(12)],
                                  _DG + [("$sblk", 0, 0x40 * 12)], args=_P, abi="thiscall")
_FGO = _DG + [("$implog", 0, 0x400), (0x0084092C, 0, 4), (0x0083AD50, 0, 4)]
HOOKS["freeGraphsyObject"] = _h(0x004A0060, "c3dw_fgo", _rows(12), _FGO, args=("int",))
HOOKS["freeGraphsyObject_nodisp"] = _h(0x004A0060, "c3dw_fgo_nodisp", _rows(12), _FGO, args=("int",))
_TVS = [-0, 10, 7, 3, -9, 1, -1, 0x7FFFFFFF, 100, 0x40000000, -0x80000000, 5]
HOOKS["TextView::setFontSize"] = _h(0x0048E120, "c3dw_tvfs", [(f"$r{i}", _TVS[i]) for i in range(12)],
                                    _DG + [("$vblk", 0, 0x2000 * 12), ("$qblk", 0, 0x40 * 12)], args=_P + _I, abi="thiscall")
HOOKS["TextView::setFontSize_nodisp"] = _h(0x0048E120, "c3dw_tvfs_nodisp", [(f"$r{i}", _TVS[i]) for i in range(12)],
                                           _DG + [("$vblk", 0, 0x2000 * 12), ("$qblk", 0, 0x40 * 12)], args=_P + _I,
                                           abi="thiscall")
HOOKS["Text_flush"] = _h(0x00478970, "c3dw_tfl", [(f"$r{i}",) for i in range(12)],
                         _WIN + [("$sblk", 0, 0x80 * 12), (0x00839658, 0, 0x80), (0x00839A9C, 0, 12)], args=_P, abi="thiscall")
_DPB_A2 = [0, 20, 100, 300, 599, -3, 7, 160, 30, 0x7FFFFFFF, 12, 1]
HOOKS["drawPanelBackdrop"] = _h(0x00432620, "c3dw_dpb", [(f"$r{i}", _DPB_A2[i]) for i in range(12)],
                                _WIN + [(0x0051A068, 0, 0x120)], args=_I * 2)

# ---------------------------------------------------------------------------------------------------- file / CRT glue
# The CRT stdio / low-level I/O functions and the exe callees are stood in (calleeStand, logged in $clog with the pointed-to
# words); no real file, FILE or descriptor is touched. Hooked inside this batch and called by other bodies of it:
# readLineGlobal -> Reader::readLine, logLine / readSectionGlobal / KeyTable::lookup -> unhooked callees.
_IO = _WIN
HOOKS["Reader::readLine"] = _h(0x00488230, "c3dw_rdl", [(f"$rd{i}",) for i in range(12)],
                               _IO + [("$rdblk", 0, 0x170 * 12), ("$bufs", 0, 0x200 * 12)], ret="pointer", args=_P,
                               abi="thiscall")
_CELL = _IO + [("$cellb", 0, 0x40)]
HOOKS["readLineGlobal"] = _h(0x00487E90, "c3dw_rlg", _rows(12), _CELL, args=("int",))
HOOKS["readSectionGlobal"] = _h(0x00487E70, "c3dw_rsg", [(f"$r{i}", _W[(i + 4) % 12]) for i in range(12)], _CELL, args=_I * 2)
HOOKS["term_destination"] = _h(0x004AE590, "c3dw_tdest", [(f"$ci{i}",) for i in range(12)],
                               _IO + [("$tblk", 0, 0x100 * 12)], args=_P)
HOOKS["output_message"] = _h(0x004AECF0, "c3dw_omsg", [(f"$ci{i}",) for i in range(12)],
                             _IO + [("$cblk", 0, 0x100 * 12)], args=_P)
_FD = [(0x0053E638, 0, 4), (0x00568D08, 0, 4)]
HOOKS["saveFieldIo"] = _h(0x0040AF70, "c3dw_sfio", [(f"$r{i}", _W[i]) for i in range(12)], _IO + _FD, args=_P + _I)
HOOKS["flushCourseInfo"] = _h(0x00432170, "c3dw_fci", _rows(12), _IO + [(0x00587DA0, 0, 0x40)], ret="void", args=("int",))
HOOKS["resolveAndOpenFile"] = _h(0x004A00F0, "c3dw_rof",
                                 [(f"$p{i % 6}", f"$m{(i // 2) % 3}") for i in range(12)], _IO, args=_P * 2)
HOOKS["logLine"] = _h(0x004A0320, "c3dw_logl", [tuple(_W[(i + k * 3) % 12] for k in range(5)) for i in range(12)], _IO,
                      args=_I * 5)
_KT_ENA = [(0x0083B274, 0, 4), (0x0083B394, 0, 4), (0x0083B4B4, 0, 4), (0x0083B5D4, 0, 4)]
for _k in "abcd":
    HOOKS[f"KeyTable::lookup_{_k}"] = _h(0x00488420, f"c3dw_ktl_{_k}", [(f"$r{i}", _W[(i + 3) % 12]) for i in range(12)],
                                         _IO + _KT_ENA, args=_I * 2)
HOOKS["MsgBox::freeLabels"] = _h(0x00490C30, "c3dw_mbfl", _rows(12),
                                 _IO + [("$arena", 0, 0x1010), ("$freelog", 0, 0x404), (0x0083B994, 0, 0x38)], ret="void",
                                 args=("int",))
HOOKS["initGolferState"] = _h(0x0046D040, "c3dw_igs", _rows(12),
                              _IO + [(0x005794D0, 0, 0x9800), (0x0056A51C, 0, 4), (0x0059AE7C, 0, 4),
                                     (0x00575AB0, 0, 0x2490 + 1)], ret="void", args=("int",))
