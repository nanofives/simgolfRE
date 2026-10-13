# C3 batch c3eu (round 32, 2026-10-13): golf_clean.exe rows earlier rounds dropped for the heap, an import, file I/O or a
# constant result. Reimplementations: shim/src/re/c3eu.cpp. Fixtures: re/frida/js/fixtures.d/c3eu.js (some wrap c3ef's /
# c3eg's / c3cz's / c3dl's fixtures, which must be loaded: the batch list links every batch). Purposes and per-jcc tables:
# log/c3/c3eu_purpose.md.
#
# Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
#   scaleX 0x00404970 (golf_math), Surface_free 0x00473ae0 (c3cz), BinkPlayer::close 0x00487060 (c3dl),
#   Cursor::destroy 0x004884b0 (c3dl), StrList::ctor 0x00487210 (c3dl), ListBox::dtor 0x00489de0 (c3ef),
#   ListModel::dtor 0x00489370 (c3cz), Base4805a0::dtor 0x004805a0, ListBox::onKey 0x0048aea0 (c3eg),
#   HashTable::remove 0x004876c0 (c3dm), Snd::unlinkFromList 0x004844e0 (c3dm), operatorDelete_4a4ffc 0x004a4ffc (c3cn)
# Hooked in THIS batch and called by this batch's other functions:
#   BinkPlayer::dtor <- BinkPlayer::scalarDtor; ListBox::deletingDtor <- ListBox::deletingDtor_vtordisp;
#   viewDeletingDtor <- viewDeletingDtor_vtordisp; Snd::setVtbl4852d0 <- Snd::dtor (C2) <- Screen::dtor, SndMusic::dtor

EXE = "golf_clean.exe"

_IMP = [("$implog", 0, 0x400)]
_HEAP = [("$arena", 0, 0x110), ("$freelog", 0, 0x404)]
_CLOG = [("$clog", 0, 0x400)]
_FL = [1, 0, 3, 2, 0xFFFFFFFF, 0xFFFFFFFE, 0x101, 0x80000000, 1, 5, 0, 1]

# re/frida/registry.d/c3ef.py UI_STATE (copied: the same window-tree globals uitree.js points into the tree)
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]


def UI_STATE(arena):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, 0x1000)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


def _h(addr, fixture, vectors, state, ret="int", args=("pointer",), abi="thiscall"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- root-window handlers
# (this, x, y), this never read (0). Globals: press words 0x00822d58..0x00822d7b, point words 0x00822d8c / 0x00822d90.
_ROOT = [(0x00822D58, 0, 0x24), (0x00822D8C, 0, 8)]
_PT = [(0, 0, 0), (0, 1, 2), (0, -1, -2), (0, 799, 599), (0, 0x7FFFFFFF, -0x80000000), (0, 400, 300), (0, -800, 12),
       (0, 12345, -678), (0, 3, 7), (0, 0x10000, 0x20000), (0, -0x80000000, 0x7FFFFFFF), (0, 64, 0)]
_PA = ["pointer", "int", "int"]
HOOKS["rootWin_onPoint2"] = _h(0x00465720, "c3eu_root_pt", _PT, _ROOT, args=_PA)
for _v in ("d0", "lt", "eq", "gt", "one", "hi"):
    _st = _ROOT + [(0x005AA774, 0, 4), (0x0083AB9C, 0, 1)] + _IMP
    HOOKS[f"rootWin_onPoint_{_v}"] = _h(0x00465610, f"c3eu_root_tick_{_v}", [_PT[1], _PT[4], _PT[7]], _st, args=_PA)
    HOOKS[f"rootWin_onUnpoint_{_v}"] = _h(0x00465650, f"c3eu_root_tick_{_v}", [_PT[0], _PT[3], _PT[10]], _st, args=_PA)
_PR = [(0, 0, 0), (0, 100, 200), (0, -50, 37), (0, 799, 599), (0, 0x7FFFFFFF, -0x80000000), (0, -1, 1), (0, 12345, -678),
       (0, 3, 7)]
for _i in range(5):
    _st = _ROOT + [(0x00822C8C, 0, 4)] + _IMP
    HOOKS[f"rootWin_onPress1_{_i}"] = _h(0x00465580, f"c3eu_root_press_{_i}", _PR, _st, args=_PA)
    HOOKS[f"rootWin_onPress2_{_i}"] = _h(0x00465680, f"c3eu_root_press_{_i}", _PR, _st, args=_PA)

# ---------------------------------------------------------------------------------------------------- vtable store, ctor
_O = [(f"$o{i}",) for i in range(12)]
HOOKS["Snd::setVtbl4852d0"] = _h(0x004852D0, "c3eu_junk40", _O, [("$oblk", 0, 0x40 * 12)], ret="void")
# arrayStaticInit_448160 (): the Snd array 0x0080d840 (0x7e90 bytes) on c3cp's three fixtures for 0x00448150.
for _v in ("live", "pat", "zero"):
    HOOKS[f"arrayStaticInit_448160_{_v}"] = _h(0x00448160, f"c3eu_sndarr_{_v}", [()],
                                               [(0x0080D840, 0, 0x7E90), ("$atexitlog", 0, 0x44)], ret="void", args=(),
                                               abi="default")
HOOKS["NodeList::ctor"] =_h(0x00487280, "c3eu_junk200", _O, [("$oblk", 0, 0x200 * 12)], ret="pointer")

# ---------------------------------------------------------------------------------------------------- small destructors
# unwindFree_4041f0 on c3cz's Surfaces ($S0..$S15; c3cz_surf_disp / _nodisp, state as c3cz's _st()).
_CZ = [("$blk", 0, 0x10000), ("$arena", 0, 0x4010), ("$freelog", 0, 0x404), ("$rec", 0, 0x400)]
HOOKS["unwindFree_4041f0_disp"] = _h(0x004041F0, "c3eu_surf_disp", [(f"$S{i}",) for i in range(16)], _CZ, ret="void")
HOOKS["unwindFree_4041f0_nodisp"] = _h(0x004041F0, "c3eu_surf_nodisp", [(f"$S{i}",) for i in range(16)], _CZ, ret="void")
# BinkPlayer::dtor on c3dl's Bink objects; the scalar destructor on c3eu's copy with the recording deallocator.
HOOKS["BinkPlayer::dtor"] = _h(0x00487040, "c3eu_dlbink", [(f"$b{i}",) for i in range(12)], [("$bblk", 0, 0x10 * 12)] + _IMP)
HOOKS["BinkPlayer::scalarDtor"] = _h(0x00487020, "c3eu_bink", [(f"$b{i}", _FL[i]) for i in range(12)],
                                     [("$bblk", 0, 0x10 * 12)] + _HEAP + _IMP, ret="pointer", args=("pointer", "uint32"))
HOOKS["Cursor_deleteDtor"] = _h(0x0045C000, "c3eu_cursor", [(f"$c{i}", _FL[i]) for i in range(12)],
                                [("$cblk", 0, 0x20 * 12), ("$subs", 0, 0x10 * 12), ("$rec", 0, 0x400)] + _HEAP + _IMP,
                                ret="pointer", args=("pointer", "uint32"))

# ListBox::deletingDtor and its thunk on c3ef's ListBox parts (lbN_w, the Window base; lbdN its vtordisp pointer).
_LBF = (1, 0, 3, 2, 1, 0)
for _v in "abc":
    _fx = f"c3eu_lbparts_{_v}"
    HOOKS[f"ListBox::deletingDtor_{_v}"] = _h(0x0048CBA0, _fx, [(f"$lb{i}_w", _LBF[i]) for i in range(6)],
                                              UI_STATE(0x28000), ret="pointer", args=("pointer", "uint32"))
    HOOKS[f"ListBox::deletingDtor_vtordisp_{_v}"] = _h(0x0048CB90, _fx, [(f"$lbd{i}", _LBF[5 - i]) for i in range(6)],
                                                       UI_STATE(0x28000), ret="pointer", args=("pointer", "uint32"))
# viewDeletingDtor and its thunk on the composites inside c3ef's TextViews (cwN = composite + 0x1fc; cwdN its thunk pointer).
_TVF = (1, 0, 3, 2)
for _v in "abc":
    _fx = f"c3eu_tv_{_v}"
    HOOKS[f"viewDeletingDtor_{_v}"] = _h(0x00491900, _fx, [(f"$cw{i}", _TVF[i]) for i in range(4)],
                                         UI_STATE(0x14000), ret="pointer", args=("pointer", "uint32"))
    HOOKS[f"viewDeletingDtor_vtordisp_{_v}"] = _h(0x004918F0, _fx, [(f"$cwd{i}", _TVF[3 - i]) for i in range(4)],
                                                  UI_STATE(0x14000), ret="pointer", args=("pointer", "uint32"))

# ListBox::typeAhead_vtordisp on c3eg's typeAhead rows (state as c3eg's ListBox::typeAhead key).
_TA_N = 200 + 6 + 22
_TA_ROWS = [k * 8 for k in range(24)] + list(range(200, 228))
_TA_STATE = ([("$rec", 0, 0x2000), ("$clog", 0, 0x1000), ("$implog", 0, 0x400), ("$lb", 0, _TA_N * 0x800),
              ("$pb", 0, _TA_N * 0x200), ("$nodes", 0, _TA_N * 8 * 0x20), (0x0083AB2C, 0, 4), (0x0083B650, 0, 4)])
HOOKS["ListBox::typeAhead_vtordisp"] = _h(0x0048CCD0, "c3eu_ta", [(f"$td{k}", f"$k{k}") for k in _TA_ROWS], _TA_STATE,
                                          args=("pointer", "int"))

# Screen::dtor / SndMusic::dtor (s0..s11), two settings of the sound-library flag 0x0083afc4.
_SND = ([("$sblk", 0, 0x80 * 12 + 0x60 * 24 + 0x20 * 12 + 0x100), ("$rec", 0, 0x400), ("$librec", 0, 0x10)] + _HEAP
        + [(0x0083AF64, 0, 8), (0x0083AFC4, 0, 4), (0x0083AF70, 0, 4)])
_S = [(f"$s{i}",) for i in range(12)]
for _v in ("lib0", "lib1"):
    HOOKS[f"Screen::dtor_{_v}"] = _h(0x004853D0, f"c3eu_screen_{_v}", _S, _SND)
    HOOKS[f"SndMusic::dtor_{_v}"] = _h(0x004848A0, f"c3eu_sndmusic_{_v}", _S, _SND)

# ---------------------------------------------------------------------------------------------------- file readers
# lookupThemeText (path, key): files f0..f6 (th0..th6), keys k0 alpha, k1 omega, k2 psi, k3 empty, k4 q, k5 missing.
_TH = [("$f0", "$k0"), ("$f0", "$k5"), ("$f1", "$k0"), ("$f2", "$k0"), ("$f3", "$k0"), ("$f4", "$k1"), ("$f4", "$k2"),
       ("$f5", "$k0"), ("$f5", "$k3"), ("$f5", "$k4"), ("$f6", "$k0"), ("$f1", "$k4"), ("$f3", "$k3"), ("$f6", "$k5")]
HOOKS["lookupThemeText"] = _h(0x0045B660, "c3eu_theme", _TH, [("$fsblock", 0, 0x4000), (0x0051A068, 0, 0x1000)],
                              args=("pointer", "pointer"), abi="default")
# loadTopScores (): absent file (_s0.._s3, generator seeds) and present files of 0x618 / 0x100 / 0x700 bytes.
_TOP = [(0x00541CE0, 0, 0x618), (0x0051A068, 0, 0x40), (0x00568D08, 0, 4), (0x00822D9C, 0, 8), ("$lio", 0, 0x1000)] + _IMP
for _v in ("s0", "s1", "s2", "s3", "p618", "p100", "p700"):
    HOOKS[f"loadTopScores_{_v}"] = _h(0x0040AD60, f"c3eu_top_{_v}", [()], _TOP, args=(), abi="default")

# resolveDataPath (name): n0..n5 names, NULL and the result buffer 0x0083c004 itself; four directory settings.
_RV = [("$n0",), ("$n1",), ("$n2",), ("$n3",), ("$n4",), ("$n5",), (0,), (0x0083C004,)]
for _v in ("abc", "b", "ab", "bc"):
    HOOKS[f"resolveDataPath_{_v}"] = _h(0x00491DA0, f"c3eu_resolve_{_v}", _RV, [(0x0083BE04, 0, 0x400)] + _IMP,
                                        ret="pointer", abi="default")

# ---------------------------------------------------------------------------------------------------- libjpeg start-up
_JM = [("$cblk", 0, 0x800), ("$eblk", 0, 0x100), ("$errlog", 0, 0x100), ("$arena", 0, 0x410), ("$freelog", 0, 0x404)] + _CLOG
# key suffixes differ by more than case: Windows file names of the CSVs are case-insensitive
for _v, _k in (("none", "none"), ("m", "mlower"), ("M", "mupper"), ("plain", "plain"), ("k", "k"), ("bad", "bad"),
               ("empty", "empty")):
    HOOKS[f"jinit_memory_mgr_{_k}"] = _h(0x004AEE30, f"c3eu_jmem_{_v}", [(f"$c{i}",) for i in range(4)], _JM,
                                         abi="default")
_CC = [("$c0", 0x3D, 0x168), ("$c1", 0x3C, 0x168), ("$c2", 0x3D, 0x164), ("$c3", 0, 0), ("$c0", 0x3E, 0x200),
       ("$c1", 0x3D, 0x168), ("$c2", -1, 0x169), ("$c3", 0x3D, 0x16C)]
for _v in ("none", "m"):
    HOOKS[f"jpeg_CreateCompress_{_v}"] = _h(0x004AE150, f"c3eu_jmem_{_v}", _CC, _JM, ret="pointer",
                                            args=("pointer", "int", "uint32"), abi="default")

# ---------------------------------------------------------------------------------------------------- dispatchers
for _v in ("n0", "neg", "n1", "n3", "n19", "shrink", "grow", "drop0"):
    HOOKS[f"serviceMovie_{_v}"] = _h(0x0049C8E0, f"c3eu_movie_{_v}", [()], [(0x00840698, 0, 0x50)] + _CLOG, args=(),
                                     abi="default")
# gfxFlushB: with no network object eax is the caller's (not compared: ret void); every other path sets eax.
HOOKS["gfxFlushB_noobj"] = _h(0x00497B20, "c3eu_gfx_noobj", [()], [(0x008400B0, 0, 0xC)] + _CLOG, ret="void", args=(),
                              abi="default")
for _v in ("nosess", "call1", "call2", "call3", "call4"):
    HOOKS[f"gfxFlushB_{_v}"] = _h(0x00497B20, f"c3eu_gfx_{_v}", [()], [(0x008400B0, 0, 0xC)] + _CLOG, args=(),
                                  abi="default")
# netFlush: n0..n7 have bit 29 of +0xe8 set (eax = netSendMessages' result), n8..n11 clear (eax is the caller's: ret void).
_NF = [("$nblk", 0, 0x100 * 12), ("$rec", 0, 0x400)] + _CLOG
HOOKS["netFlush"] = _h(0x00497CC0, "c3eu_netflush", [(f"$n{i}",) for i in range(8)], _NF)
HOOKS["netFlush_clear"] = _h(0x00497CC0, "c3eu_netflush", [(f"$n{i}",) for i in range(8, 12)], _NF, ret="void")

# ---------------------------------------------------------------------------------------------------- second pass
# playHabitatSound (kind, a, b): every kind arm, kinds outside the six (eax = kind), playSoundAt stood in.
_HB = [(0, 3, 4), (1, -2, 7), (2, 5, 5), (3, 0x100, 0x200), (4, 9, -9), (5, 0x7FFFFFFF, 1), (6, 12, 34), (7, 1, 1),
       (-1, 2, 3), (0x100, 4, 4), (-0x80000000, 0, 0), (5, -5, 6), (0, 0, 0)]
HOOKS["playHabitatSound"] = _h(0x00405A00, "c3eu_habitat", _HB, _CLOG, args=("int", "int", "int"), abi="default")
# flicNextFrame (slot) on rows 0..11; flicOpenStream (path, slot, outCount): odd paths make Flic::open fail.
_FLS = [(0x004F65C8, 0, 0x84 * 12), (0x0050FC20, 0, 0x44 * 12), ("$hdr", 0, 0x80 * 12), ("$outs", 0, 0x30)] + _CLOG
HOOKS["flicNextFrame"] = _h(0x00404090, "c3eu_flic", [(i,) for i in range(12)], _FLS, args=("int",), abi="default")
_FO = [(0x1000, 0, "$out0"), (0x1001, 1, "$out1"), (0x1003, 2, 0), (0x1002, 3, "$out3"), (0x1004, 4, 0),
       (0x1006, 5, "$out5"), (0x2001, 6, "$out6"), (0x2000, 7, "$out7"), (0x2002, 8, "$out8"), (0x2004, 9, 0),
       (0x2005, 10, 0), (0x2006, 11, "$out11")]
HOOKS["flicOpenStream"] = _h(0x00404120, "c3eu_flicopen", _FO, _FLS, args=("int", "int", "pointer"), abi="default")
# globalObjDtor49c780 (this) on twelve patterned objects.
HOOKS["globalObjDtor49c780"] = _h(0x0049C780, "c3eu_gobj", _O, [("$oblk", 0, 0x600 * 12)] + _CLOG)
# initSound (device, flags): version check 0 (every arm), 3 and -1 (early return).
_IS = [(0x10, 0), (0x10, 2), (0x10, 0x102), (0x10, 8), (0x10, 0x10A), (0x11, 2), (0x11, 0xA), (0x20, 0xFFFFFFFF),
       (0x21, 0xFFFFFFFF), (0x30, 0x100), (0x40, 0xA), (0x42, 0xA)]
HOOKS["initSound"] = _h(0x00483E90, "c3eu_isnd_ok", _IS, [(0x0083AFC4, 0, 4)] + _CLOG, args=("int", "uint32"),
                        abi="default")
# logWrite (this, s1, s2, d1, d2, d3): loggers t0..t4; s2 = 0 selects the short format; global 0x00840940 0 / 1.
_LA = ("pointer", "int", "int", "int", "int", "int")
_LW = [("$t0", 0x111, 0x222, 3, 4, 5), ("$t0", 0x111, 0, 3, 4, 5), ("$t1", 1, 2, 3, 4, 5), ("$t2", 1, 2, 3, 4, 5),
       ("$t3", 1, 2, 3, 4, 5), ("$t4", 7, 0, -1, -2, -3), ("$t4", 8, 9, 0x7FFFFFFF, 0, 1), ("$t3", 0, 0, 0, 0, 0),
       ("$t0", 0, 0, 0, 0, 0), ("$t2", 0, 0, 6, 6, 6), ("$t4", -1, -1, -1, -1, -1), ("$t1", 0, 0, 0, 0, 0)]
_LS = [("$tblk", 0, 0xA0), ("$names", 0, 0xA0), (0x00840940, 0, 4)] + _CLOG
HOOKS["logWrite"] = _h(0x004A0280, "c3eu_logw", _LW, _LS, args=_LA)
HOOKS["logWrite_off"] = _h(0x004A0280, "c3eu_logw_off", _LW[:6], _LS, args=_LA)
# writeCourseInfo (path): paths p0..p3 per variant of the printed globals.
for _v in ("a", "b", "c"):
    HOOKS[f"writeCourseInfo_{_v}"] = _h(0x00431FA0, f"c3eu_cinfo_{_v}", [(f"$p{i}",) for i in range(4)],
                                        [("$pblk", 0, 0x400), ("$fout", 0, 0x1000), (0x0051A068, 0, 0x400)] + _CLOG,
                                        abi="default")
# shutdownSound (): one call per list variant (sizes as in the fixture's SHUT table).
for _v, _n in (("off", 1), ("empty", 0), ("n1", 1), ("n4", 4), ("n8", 8), ("v6", 6)):
    HOOKS[f"shutdownSound_{_v}"] = _h(0x00483F10, f"c3eu_shut_{_v}", [()],
                                      [("$nblk", 0, 0x80 * max(_n, 1)), ("$rec", 0, 0x200), (0x0083AF64, 0, 8),
                                       (0x0083AFC4, 0, 4)] + _HEAP + _CLOG, args=(), abi="default")
for _v in ("ver3", "verneg"):
    HOOKS[f"initSound_{_v}"] = _h(0x00483E90, f"c3eu_isnd_{_v}", _IS[:4], [(0x0083AFC4, 0, 4)] + _CLOG,
                                  args=("int", "uint32"), abi="default")
