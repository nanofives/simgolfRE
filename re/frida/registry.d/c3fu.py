# C3 batch c3fu (round 35, 2026-10-14): rows round 33 classed NO_OBSERVABLE / NO_CALLER that write a global, return a
# field or have a reference. Reimplementations: shim/src/re/c3fu.cpp. Fixtures: re/frida/js/fixtures.d/c3fu.js (its header
# explains the per-vector getters: "$p<k>" / "$c<k>" / "$d<k>" / "$s<k>" write that vector's globals and are passed as
# `this`, which those bodies never read). Wrapped fixtures: c3ct_edit / c3ct_edit_b (c3ct.js), c3db_a (c3db.js).
# Purposes and per-jcc tables: log/c3/c3fu_purpose.md.
#
# Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
#   golf_clean.exe: StrList::dtor 0x00487260 (c3bx) <- NodeListB::dtor / NodeListC::dtor; Snd::stop484f00 0x00484f00 (c3ev),
#     shutdownSound 0x00483f10 (c3eu, stood in by the fixture) <- shutdownSoundSystem; freeGraphsyObject 0x004a0060 (c3dw)
#     and the eleven stood-in callees (shutdownBlitter 0x00492470, Timer::lockDec 0x00486ff0, freeFonts 0x0049ff30,
#     freeCursors 0x0049ead0, clearPrimarySurface 0x00483b10, freeGfxGlobal_840710 0x0049d280, freeFileWinBuf 0x0049d460,
#     MsgBox::freeLabels 0x00490c30, screenTick47d010 0x0047d010, clearScreenObjects 0x00483340, strFree 0x0049d0e0)
#     <- engineShutdown; EditBox::lineIndexAt 0x00486330 (c3s), drawMarkupRun 0x00477280 (c3o), Widget_value 0x00477580
#     (c3c) <- EditBox::charPosToPixel; Surface_pixelPtr 0x004796a0 (c3p) and jgld's Surface slots (c3ce) <- Widget_writePixel
#   sound.dll: SoundWindowObj_dtor 0x286a0 (c3dt) <- atexitDtor_100b49b8
# No function of this batch calls another function of this batch.

EXE = "golf_clean.exe"
SND = "sound.dll"


def _h(addr, fixture, vectors, state, ret="int", args=("pointer",), module=EXE, abi="thiscall"):
    return dict(module=module, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state,
                vectors=vectors)


def _sel(prefix, n):
    return [(f"${prefix}{k}",) for k in range(n)]


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- golf_clean.exe
# resetSpriteQueue (cdecl, no argument; `this` is the per-vector selector, never read). The globals and their neighbours.
_RSQ = [(0x00838200, 0, 4), (0x0083416C, 0, 0x14), (0x008371EC, 0, 0x18), (0x0082C15C, 0, 0xC)]
HOOKS["resetSpriteQueue"] = _h(0x004627D0, "c3fu_rsq", _sel("p", 12), _RSQ)

# NodeListB::dtor / NodeListC::dtor on twelve objects: +0x14 NULL (objects 0, 3, 6, 9) or a recording held object.
_NL = [("$oblk", 0, 0x300), ("$rec", 0, 0x400)]
HOOKS["NodeListB::dtor"] = _h(0x00487A60, "c3fu_nl", [(f"$o{i}",) for i in range(12)], _NL, ret="void")
HOOKS["NodeListC::dtor"] = _h(0x00487B80, "c3fu_nl", [(f"$o{i}",) for i in range(12)], _NL, ret="void")

# shutdownSoundSystem (cdecl, no argument): the Snd array refilled per vector, shutdownSound stood in.
_SSS = [(0x0080D840, 0, 0x7E90), ("$rec", 0, 0x2400), ("$clog", 0, 0x400)]
HOOKS["shutdownSoundSystem"] = _h(0x004490B0, "c3fu_sss", _sel("p", 6), _SSS)

# engineShutdown (cdecl, no argument): eleven callees stood in, freeGraphsyObject real on our display / handle per vector.
_ES = [(0x0083AD50, 0, 4), (0x0084092C, 0, 4), ("$rec", 0, 0x400), ("$clog", 0, 0x400), ("$implog", 0, 0x400)]
HOOKS["engineShutdown"] = _h(0x00485740, "c3fu_es", _sel("p", 8), _ES)

# EditBox::charPosToPixel(lines, n, pos, outX, outY) on c3ct's EditBoxes 0, 2, 5 (explicit line tables of 5, 4, 6 entries).
_EB_N = 6
_EB_STATE = ([(f"$eb{i}", 0, 0x700) for i in range(_EB_N)]
             + [(f"$ebch{i}", 0, 0x80) for i in range(_EB_N) if i != 1]
             + [("$tbuf", 0, 0x200), ("$lines0", 0, 0x40), ("$lines2", 0, 0x40), ("$lines5", 0, 0x40),
                ("$lbuf", 0, 0x100), ("$cnt", 0, 0x10), ("$rec", 0, 0x800),
                (0x0083AB30, 0, 4), (0x0083AB34, 0, 4), (0x00839AA8, 0, 4)])
_CPP_STATE = _EB_STATE + [("$ox", 0, 0x10), ("$oy", 0, 0x10)]
_CPP_POS = {0: (5, [0, 1, 5, 6, 7, 16, 17, 18, 29, 30, 31, 44, 45]),
            2: (4, [0, 3, 10, 11, 12, 21, 22, 32, 33]),
            5: (6, [0, 7, 8, 14, 15, 25, 26, 39, 40, 53, 54])}
_CPP = []
for _i, (_n, _ps) in _CPP_POS.items():
    _CPP += [(f"$eb{_i}", f"$lines{_i}", _n, p, "$ox", "$oy") for p in _ps]
    _CPP += [(f"$eb{_i}", f"$lines{_i}", m, p, "$ox", "$oy") for m in (1, 2, 3) for p in _ps[::3]]
_CPP += [("$eb0", "$lines0", 5, 20, "$ox", "$ox"), ("$eb5", "$lines5", 6, 41, "$oy", "$oy")]
_CPP_ARGS = ("pointer", "pointer", "int", "int", "pointer", "pointer")
HOOKS["EditBox::charPosToPixel"] = _h(0x00486360, "c3fu_cpp", _CPP, _CPP_STATE, args=_CPP_ARGS)
HOOKS["EditBox::charPosToPixel_b"] = _h(0x00486360, "c3fu_cpp_b", _CPP, _CPP_STATE, args=_CPP_ARGS)

# Widget_writePixel(x, y, colour) on c3db's widgets: w0 / w1 / w10 8-bit (w1 an inner clip, w10 a clip past the bounds),
# w2 / w3 / w4 16-bit, w5 24-bit, w6 32-bit, w7 depth 12, w8 / w9 no pixel base, wnull no Surface. Bounds 32 x 16.
_WP_PTS = [(0, 0), (5, 3), (31, 15), (32, 0), (0, 16), (10, 7), (17, 9), (-1, 0), (0, -1), (3, 4), (1, 1), (24, 12),
           (-0x80000000, 2), (2, 0x7FFFFFFF)]
_WP_COL = [0x1AB, 0x77, 0xFFFFFFFF, 0x12345678, 0x80, 0xFEDC]
_WP = []
for _j, _w in enumerate(["$w0", "$w1", "$w2", "$w3", "$w4", "$w5", "$w6", "$w7", "$w8", "$w9", "$w10"]):
    _WP += [(_w, x, y, _WP_COL[(_j + k) % len(_WP_COL)]) for k, (x, y) in enumerate(_WP_PTS)]
_WP += [("$wnull", 0, 0, 0x55), ("$wnull", -1, 3, 0x66)]
# wr8 / wr16 / wr24: our Surfaces with a recording vtable (c3fu.js c3fu_wp): every slot call is logged at $wrrec.
_WR_PTS = [(0, 0), (5, 3), (31, 15), (0, 16), (31, 16), (32, 0), (3, 15), (-1, 4), (10, 7), (0, 15)]
_WP += [(w, x, y, _WP_COL[(k + j) % len(_WP_COL)]) for j, w in enumerate(["$wr8", "$wr16", "$wr24"])
        for k, (x, y) in enumerate(_WR_PTS)]
HOOKS["Widget_writePixel"] = _h(0x00478EA0, "c3fu_wp", _WP,
                                [("$arena", 0, 0xC000), ("$wrrec", 0, 0x400), ("$wrpix", 0, 0xC00)],
                                args=("pointer", "int", "int", "uint32"))

# ---------------------------------------------------------------------------------------------------- sound.dll (RVAs)
HOOKS["getDsCoopLevel"] = _h(0x0003A770, "c3fu_coop", _sel("c", 12), [("$g", 0, 4)], module=SND)
HOOKS["getWaveDeviceInterface"] = _h(0x0000E2D0, "c3fu_wdi", _sel("d", 12), [("$g", 0, 4)], ret="uint32", module=SND)
HOOKS["cfltcvtInit"] = _h(0x00042506, "c3fu_cflt", _sel("p", 12), [("$cvt", 0, 0x20)], ret="uint32", module=SND)
HOOKS["atexitDtor_100b49b8"] = _h(0x000091F0, "c3fu_swo", _sel("s", 12), [("$swo", 0, 0x10), ("$implog", 0, 0x400)],
                                  ret="uint32", module=SND)
