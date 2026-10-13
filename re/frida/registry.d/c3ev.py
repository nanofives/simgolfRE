# C3 batch c3ev (round 32, 2026-10-13) of golf_clean.exe: vtordisp thunks, fixed this-adjustors, scalar / panel deleting
# destructors, root-window input handlers, static initialisers and small table walkers. Reimplementations in
# shim/src/re/c3ev.cpp; fixtures in re/frida/js/fixtures.d/c3ev.js (all on re/frida/js/uitree.js); Purposes and per-jcc
# branch tables in log/c3/c3ev_purpose.md.
#
# Every key compares UI_STATE (uitree.js: arena, free log, import log, the stand-ins' / display's recorder, window-tree
# globals, z-list) plus the live globals its body writes.

EXE = "golf_clean.exe"

UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000


def UI_STATE(arena):
    # + $alog: every stand-in call with all its stack arguments (fixtures.d/c3ev.js uiTree / stand)
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE),
             ("$alog", 0, 0x1000)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


def _h(addr, fixture, vectors, arena, ret="int", args=("pointer",), abi="thiscall", extra=()):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture,
                state=UI_STATE(arena) + list(extra), vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- vtordisp thunks
# c3ev_thunks: $p0..$p11 = s + 4 of scratch blocks whose [s] is the displacement (0, 4, 8, 0x1c, 0x24, 0x8c, 0xe0, 0x1fc,
# -4, -0x100, 0x7fffffff, -0x80000000); every target is a recorder (its `this` and first two arguments in $rec, its result
# a hash of all of them).
_TA = 0x2000
_P = [f"$p{i}" for i in range(12)]
_A2 = [(0, 0), (5, -7)]
_A4 = [(0, 0, 0, 0), (1, -2, 0x30, 0x7FFFFFFF)]
_F1 = [0, 1, 3, -1]
for _addr, _name, _n in ((0x0048CD10, "ListBox::onScroll_vtordisp", 2), (0x0048CD60, "ListBox::build_vtordisp", 4),
                         (0x00491BF0, "panelBuildRow_vtordisp", 4), (0x004A30F0, "panelCreate4a2400_vtordisp", 4),
                         (0x004A4AB0, "panelBuild4a3880_vtordisp", 4), (0x004A3040, "panelRowDeletingDtor_vtordisp", 1),
                         (0x004A4970, "panelSubDeletingDtor_vtordisp", 1)):
    _tail = {1: [(f,) for f in _F1[:2]], 2: _A2, 4: _A4}[_n]
    HOOKS[_name] = _h(_addr, "c3ev_thunks", [(p,) + t for p in _P for t in _tail], _TA, args=("pointer",) + ("int",) * _n)

# ---------------------------------------------------------------------------------------------------- fixed adjustors
# `this` values are plain numbers: the adjustor subtracts its constant and the stood-in target only records it.
_THIS = [0x274, 0x1000, 0x00400000, 0x7FFF0000, 0xFFFFFFFF, 0, 0x12345678, 0x80000000, 0x2D0, 0x5C, 0x10000274, 0xDEADBEEF]
_ADJ_V = [(t, f) for t, f in zip(_THIS, [0, 1, 3, -1, 2, 1, 0, 0x100, 1, 0, 5, 1])]
for _addr, _name in ((0x0049BFD0, "screenObj_deleteDtor_adj5c"), (0x0043CBF0, "editBoxTimer_deleteDtor_adj274"),
                     (0x00489140, "Button::scalarDtor_adj274"), (0x004918D0, "Dialog::scalarDtor_adj274"),
                     (0x0049C820, "globalObjDeleteDtor49c800_adj274")):
    HOOKS[_name] = _h(_addr, "c3ev_thunks", _ADJ_V, _TA, args=("pointer", "int"))

# ---------------------------------------------------------------------------------------------------- scalar deleting dtors
# c3ev_sdtor: $o0..$o11 arena blocks (+0x60 = 0, 1, 0, 0x5000, 0, -1, 0, 7, 0, 0x80000000, 0, 2); the destructor each one
# calls is a recorder; operator delete / free are recorded in $freelog.
_SDF = [0, 1, 2, 3, -1, 0x100, 1, 0, 0xFFFFFFFE, 5, 1, 0x80000001]
_SD_V = [(f"$o{i}", f) for i, f in enumerate(_SDF)]
for _addr, _name in ((0x00488630, "Button::scalarDtor"), (0x004916F0, "Dialog::scalarDtor"),
                     (0x0049C800, "globalObjDeleteDtor49c800"), (0x0043CBD0, "editBoxTimer_deleteDtor"),
                     (0x00404260, "deletingDtor_404260"), (0x0049BF70, "screenObj_deleteDtor")):
    HOOKS[_name] = _h(_addr, "c3ev_sdtor", _SD_V, _TA, args=("pointer", "int"))

# c3ev_real: real Buttons ($bN) and EditBoxes ($eN) built by their constructors; _s = object + 0x274.
_RA = 0x10000
_RF = [0, 1, 3, 2]
HOOKS["Button::scalarDtor_real"] = _h(0x00488630, "c3ev_real", [(f"$b{i}", f) for i, f in enumerate(_RF)], _RA, args=("pointer", "int"))
HOOKS["editBoxTimer_deleteDtor_real"] = _h(0x0043CBD0, "c3ev_real", [(f"$e{i}", f) for i, f in enumerate(_RF)], _RA,
                                           args=("pointer", "int"))
HOOKS["Button::scalarDtor_adj274_real"] = _h(0x00489140, "c3ev_real", [(f"$b{i}_s", f) for i, f in enumerate(_RF)], _RA,
                                             args=("pointer", "int"))
HOOKS["editBoxTimer_deleteDtor_adj274_real"] = _h(0x0043CBF0, "c3ev_real", [(f"$e{i}_s", f) for i, f in enumerate(_RF)], _RA,
                                                  args=("pointer", "int"))

# ---------------------------------------------------------------------------------------------------- panel deleting dtors
# c3ev_parts: $faN Field4a2250 / $psN PanelSub parts with 0, 1, 3, 0, 2, 5 rows; _w the Window base, _t a vtordisp argument
# leading to _w. Real callees: viewFieldDtor4a2330 / panelSubDtor4a3480, ListModel::dtor, Base4805a0::dtor.
_PA = 0x20000
_PF = [1, 0, 3, 2, 1, 0]
HOOKS["panelRowDeletingDtor"] = _h(0x004A3050, "c3ev_parts", [(f"$fa{i}_w", f) for i, f in enumerate(_PF)], _PA, args=("pointer", "int"))
HOOKS["panelSubDeletingDtor"] = _h(0x004A4980, "c3ev_parts", [(f"$ps{i}_w", f) for i, f in enumerate(_PF)], _PA, args=("pointer", "int"))
HOOKS["panelRowDeletingDtor_vtordisp_real"] = _h(0x004A3040, "c3ev_parts", [(f"$fa{i}_t", f) for i, f in enumerate(_PF)], _PA,
                                                 args=("pointer", "int"))
HOOKS["panelSubDeletingDtor_vtordisp_real"] = _h(0x004A4970, "c3ev_parts", [(f"$ps{i}_t", f) for i, f in enumerate(_PF)], _PA,
                                                 args=("pointer", "int"))

# ---------------------------------------------------------------------------------------------------- root-window input
# c3ev_input_<v>: patterned input globals; v = screen width / key timer / clock (fixture comment). `this` is unused (0).
_IA = 0x1000
_IN = [(g, 0, 4) for g in (0x00822D58, 0x00822D5C, 0x00822D60, 0x00822D64, 0x00822D68, 0x00822D6C, 0x00822D70, 0x00822D74,
                          0x00822D78, 0x00822D8C, 0x0083821C, 0x00822B98, 0x00822B9C, 0x00838208, 0x00822C8C, 0x005AA774)]
_IN += [(0x0083AB9C, 0, 1)]
_XY = [(0, 0), (1, 1), (799, 599), (-5, 7), (400, 300), (0x3FFFFFFF, -0x40000000), (2, 3), (160, 120), (-1, -1), (1234, -987)]
_KEYS = [(7, 0x5F), (8, 0x60), (9, 0x65), (10, 0x69), (11, 0x6A), (12, 0x1000D), (13, 0x1000C), (0, 0), (-1, -1), (0x41, 0x41)]
_CH = [0, 1, 0x41, 0x7A, 0x0D, 0x1B, 0xFF, 0x100, -1, 0x7FFFFFFF]
for _v in "abcde":
    _fx = f"c3ev_input_{_v}"
    HOOKS[f"rootWin_onPress1b_{_v}"] = _h(0x004657E0, _fx, [(0, x, y) for x, y in _XY], _IA, args=("pointer", "int", "int"), extra=_IN)
    HOOKS[f"rootWin_onPress2b_{_v}"] = _h(0x00465840, _fx, [(0, x, y) for x, y in _XY], _IA, args=("pointer", "int", "int"), extra=_IN)
    HOOKS[f"rootWin_onKey_{_v}"] = _h(0x00465750, _fx, [(0, a, c) for a, c in _KEYS], _IA, args=("pointer", "int", "int"), extra=_IN)
for _v in "ab":
    _fx = f"c3ev_input_{_v}"
    HOOKS[f"rootWin_onChar_{_v}"] = _h(0x004657B0, _fx, [(0, c) for c in _CH], _IA, args=("pointer", "int"), extra=_IN)
    HOOKS[f"rootWin_onChar0_{_v}"] = _h(0x004657D0, _fx, [(0, c) for c in _CH], _IA, args=("pointer", "int"), extra=_IN)
    HOOKS[f"rootWin_onUnpoint2_{_v}"] = _h(0x00465740, _fx, [(0, x, y) for x, y in _XY], _IA, ret="void",
                                          args=("pointer", "int", "int"), extra=_IN)

# ---------------------------------------------------------------------------------------------------- tables
_TBL = [(0x0056D1D8, 0, 100 * 0x3C)]
for _v in ("none", "all", "odd", "ends", "fffe"):
    HOOKS[f"tickAnimals_{_v}"] = _h(0x00401C00, f"c3ev_animals_{_v}", [()], _IA, ret="void", args=(), abi="default", extra=_TBL)
HOOKS["resetPair"] = _h(0x004041C0, "c3ev_pair", [(i,) for i in (0, 1, 2, 5, 10, -1, -3, 100, 0x7FFFFFFF, -0x80000000, 7, 3)],
                        _IA, args=("int",), abi="default")
for _v in ("00", "a5", "7f"):
    HOOKS[f"initSinTable_{_v}"] = _h(0x00491C10, f"c3ev_sin_{_v}", [()], _IA, args=(), abi="default",
                                     extra=[(0x0083B9F4, 0, 0x404)])

# ---------------------------------------------------------------------------------------------------- static initialisers
HOOKS["staticInit_404360"] = _h(0x00404360, "c3ev_static", [(f"$v{i}",) for i in range(6)], 0x4000)
for _v in ("00", "cd", "5a"):
    HOOKS[f"staticInit_00404320_{_v}"] = _h(0x00404320, f"c3ev_root_{_v}", [()], _IA, args=(), abi="default",
                                            extra=[(0x00519A50, 0, 0x598)])

# ---------------------------------------------------------------------------------------------------- Link482::resolve
HOOKS["Link482::resolve"] = _h(0x00482E20, "c3ev_link", [(f"$l{i}",) for i in range(12)], 0x2000)

# ==================================================================================================================== tranche 2
# Snd / Screen scalar deleting destructors on c3ev_sdtor (their destructors are recorders there too).
for _addr, _name in ((0x004841C0, "Snd::scalarDtor4841c0"), (0x00484880, "SndMusic::scalarDtor"),
                     (0x004852B0, "Snd::scalarDtor4852b0"), (0x004853B0, "Screen::scalarDtor")):
    HOOKS[_name] = _h(_addr, "c3ev_sdtor", _SD_V, _TA, args=("pointer", "int"))

# C404200_dtor / Dialog::subDtor with every callee a recorder (c3ev_cdtor, $o0..$o11).
HOOKS["C404200_dtor"] = _h(0x00404200, "c3ev_cdtor", [(f"$o{i}",) for i in range(12)], 0x1000)
HOOKS["Dialog::subDtor"] = _h(0x004914D0, "c3ev_cdtor", [(f"$o{i}",) for i in range(12)], 0x1000)

# scrollbar creators (ecx forwarded: plain numbers for `this`); scrollbarCreate 0x00496330 a recorder.
_SC = [(0x1000, 1, 2, 3, 4, 5), (0x2000, 0, 0, 7, 0, 9), (0x3000, 6, 7, 0, 8, 1), (0x4000, -1, 0x7FFFFFFF, -3, -4, 0),
       (0, 0, 0, 1, 1, 0), (0x5000, 9, 9, 9, 0, 9), (0xDEAD0000, 0x10, 0x20, 0x30, 0x40, 0x50), (0x6000, 2, 3, 0, 0, 4),
       (0x7000, 0x123, 0x456, 0x789, 0xABC, 0xDEF), (0x8000, -9, -8, 1, -6, -5)]
_SCS = [(0x0083FF10, 0, 4), (0x0083FF60, 0, 4)]
for _v in "ab":
    for _addr, _name in ((0x00496690, "scrollbarCreateA_impl"), (0x004966D0, "scrollbarCreateB_impl"),
                         (0x00496710, "scrollbarCreateA"), (0x00496740, "scrollbarCreateB")):
        HOOKS[f"{_name}_{_v}"] = _h(_addr, f"c3ev_scroll_{_v}", _SC, 0x1000, args=("pointer", "int", "int", "int", "int", "int"),
                                    extra=_SCS)

# Link482b::send (c3ev_link2: Image::seek a recorder)
HOOKS["Link482b::send"] = _h(0x00482E40, "c3ev_link2", [(f"$l{i}", a, b, c) for i in range(12)
                                                        for a, b, c in ((0, 0, 0), (3, -1, 7))], 0x2000,
                             args=("pointer", "int", "int", "int"))

# Image::step / Image::seek (c3ev_image: decodeImageChunks a recorder; images im0..im4, links lk0..lk4, fixture comment)
_IMA = 0x4000
_STEP_V = [(f"$im{i}", f"$lk{j}", a) for i in (0, 1, 2, 4) for j in range(5) for a in ((0, 9) if j % 2 else (5,))]
HOOKS["Image::step"] = _h(0x00482420, "c3ev_image", _STEP_V, _IMA, args=("pointer", "pointer", "int"))
_SEEK = {0: (0, 1, 5, 6), 1: (0, 1, 2, 3, 4, 35), 2: (-1, 0, 5, 31), 3: (0, 1), 4: (31, 0, 1, 32)}
# pos == -1 uses `frame` itself as the table slot (0x004824af), so that pair only runs with 0 <= frame < 32 (the table's
# 32 entries); a negative pos other than -1 would never reach its target step (the counter only counts up).
_SEEK_V = [(f"$im{i}", fr, pos, f"$lk{j}", a4) for i, frs in _SEEK.items() for fr in frs
           for pos, j, a4 in ((-1, fr % 5, 0), (0, (fr + 1) % 5, 3), (1, (fr + 2) % 5, 0), (3, 4, 1), (9, 0, 2))
           if pos != -1 or 0 <= fr < 32]
HOOKS["Image::seek"] = _h(0x00482490, "c3ev_image", _SEEK_V, _IMA, args=("pointer", "int", "int", "pointer", "int"))

# freeTable (c3ev_free_<v>: 16 table rows, freeBlock / Surface_free recorders)
_FT = [(0x005A9370, 0, 0x40), (0x0053F3E8, 0, 0x40), (0x005A5A2C, 0, 0x40)]
for _v in "ab":
    HOOKS[f"freeTable_{_v}"] = _h(0x0043D670, f"c3ev_free_{_v}", [(i,) for i in range(16)], 0x1000, args=("int",),
                                  abi="default", extra=_FT)

# tickMessage (argument-free: one key per variant)
_TK = [(a, 0, 4) for a in (0x005694A4, 0x005A9CCC, 0x005A34EC, 0x0059E7B8, 0x00834170, 0x005A7144)]
_TK += [(0x00569498, 0, 1), (0x005A6D40, 0, 0x40), (0x0051A068, 0, 0x100)]
for _v in "abcdefgh":
    HOOKS[f"tickMessage_{_v}"] = _h(0x0040D6A0, f"c3ev_tick_{_v}", [()], 0x1000, args=(), abi="default", extra=_TK)

# arrayStaticInit_43d460 / dynInit_arrayStaticInit_43d460 (c3ev_arr_<v>)
for _v in ("ok", "fail"):
    HOOKS[f"arrayStaticInit_43d460_{_v}"] = _h(0x0043D460, f"c3ev_arr_{_v}", [()], 0x1000, args=(), abi="default",
                                               extra=[(0x005AA748, 0, 0x2C)])
    HOOKS[f"dynInit_arrayStaticInit_43d460_{_v}"] = _h(0x0043D450, f"c3ev_arr_{_v}", [()], 0x1000, args=(), abi="default",
                                                       extra=[(0x005AA748, 0, 0x2C)])

# ==================================================================================================================== tranche 3
HOOKS["Snd::stop484f00"] = _h(0x00484F00, "c3ev_sndstop", [(f"$s{i}",) for i in range(10)], 0x2000)
HOOKS["Snd::dtor"] = _h(0x004841E0, "c3ev_snddtor", [(f"$d{i}",) for i in range(10)], 0x2000)
_MV = [(f"$m{i}",) for i in range(14)]
HOOKS["serviceMovieFrame"] = _h(0x0049C910, "c3ev_movie", _MV, 0x6000)
HOOKS["movieDecodeStart"] = _h(0x0049CAE0, "c3ev_movie", _MV, 0x6000)
HOOKS["movieReadProcess"] = _h(0x0049CB20, "c3ev_movie", _MV, 0x6000)

# ==================================================================================================================== tranche 4
# KeyTable::find (c3ev_keys: tables k0 golfers.txt / 5 entries, k1 COURSE.TXT / count 1, k2 x.dat / 3, k3 zero.txt /
# count 0; names n0..n9, keys y0..y12 in the fixture's KNAMES / KKEYS order)
_KF_V = [("$k0", "$n0", "$y0"), ("$k0", "$n1", "$y1"), ("$k0", "$n8", "$y2"), ("$k0", "$n0", "$y3"), ("$k0", "$n0", "$y4"),
         ("$k0", "$n0", "$y5"), ("$k0", "$n0", "$y6"), ("$k0", "$n2", "$y0"), ("$k0", "$n3", "$y0"), ("$k0", "$n9", "$y0"),
         ("$k1", "$n4", "$y8"), ("$k1", "$n4", "$y7"), ("$k2", "$n5", "$y9"), ("$k2", "$n5", "$y10"), ("$k2", "$n6", "$y9"),
         ("$k2", "$n5", "$y12"), ("$k3", "$n7", "$y11"), ("$k0", "$n0", "$y10")]
HOOKS["KeyTable::find"] = _h(0x00488310, "c3ev_keys", _KF_V, 0x2000, args=("pointer", "pointer", "pointer"))

# jinit_compress_master (c3ev_jinit: $c0..$c11, the ten jinit_* callees and the three cinfo method pointers recorders)
HOOKS["jinit_compress_master"] = _h(0x004B03A0, "c3ev_jinit", [(f"$c{i}",) for i in range(12)], 0x4000, abi="default")

# loadGraphsyObject (c3ev_graphsy: path blocks $g0..$g11 and NULL; [0x0084092c] compared)
HOOKS["loadGraphsyObject"] = _h(0x004A00A0, "c3ev_graphsy", [(0,)] + [(f"$g{i}",) for i in range(12)], 0x2000, abi="default",
                                extra=[(0x0084092C, 0, 4)])

# renderSpriteQueue (argument-free: one key per list shape; head and next table compared)
for _v in ("empty", "one", "chain", "long", "zero"):
    HOOKS[f"renderSpriteQueue_{_v}"] = _h(0x00463100, f"c3ev_sprites_{_v}", [()], 0x1000, args=(), abi="default",
                                          extra=[(0x0082C160, 0, 4), (0x008361F4, 0, 0x100)])

# panelBuildRow (c3ev_prow: panels $o0..$o5 = panel + 0x59c, owners $w0..$w3 and NULL; panelLayoutText / Window::create
# recorders)
_PR_V = [("$o0", "$w0", 1, 2, 0), ("$o0", "$w1", 3, 4, 0x10), ("$o0", 0, 5, 6, 7), ("$o1", "$w0", 1, 2, 0),
         ("$o2", "$w2", -1, 0x7FFFFFFF, 0x100000), ("$o3", "$w0", 0, 0, 0), ("$o4", "$w3", 9, 8, 0xFFFFFFFF),
         ("$o5", "$w1", 0x40, 0x50, 0x1000), ("$o5", "$w0", -5, -6, 0xFFEFFFFF), ("$o2", 0, 0, 0, 0),
         ("$o4", "$w1", 0x123, 0x456, 2), ("$o0", "$w2", 0x80000000, 1, 0x80000000)]
HOOKS["panelBuildRow"] = _h(0x0049D6D0, "c3ev_prow", _PR_V, 0x8000, args=("pointer", "pointer", "int", "int", "int"))

# movieReadFrame (c3ev_mread: $m0..$m11; listPush through the masked stand-in, mmioRead / timeGetTime import stand-ins)
HOOKS["movieReadFrame"] = _h(0x0049CB90, "c3ev_mread", [(f"$m{i}",) for i in range(12)], 0x10000)
# moviePlayFrame (c3ev_play: $p0..$p14, every callee a recorder answering from the object; fixture comment)
HOOKS["moviePlayFrame"] = _h(0x0049CCC0, "c3ev_play", [(f"$p{i}",) for i in range(18)], 0x14000)

# applyTileWalls (c3ev_walls: tiles $t0..$t4; the result is a byte, `mov al` 0x0044a3fa)
_TW_V = [("$t0", 1, 0), ("$t0", 0, 0), ("$t1", 0xF, 0), ("$t1", 3, 5), ("$t2", 0xF, 0), ("$t2", 6, 9), ("$t3", 0xF, 0),
         ("$t3", 1, 2), ("$t4", 0xF, 0), ("$t4", 5, 5), ("$t0", 0xFF, 0x0F), ("$t1", -1, 0), ("$t2", 0x31, 0x22)]
HOOKS["applyTileWalls"] = _h(0x0044A380, "c3ev_walls", _TW_V, 0x2000, ret="uint8", args=("pointer", "int", "int"),
                             abi="default")

# startNewCourse (argument-free: one key per screen outcome; the four globals it writes compared)
_NC = [(0x00822C88, 0, 4), (0x00571FD4, 0, 4), (0x00834170, 0, 4), (0x0059E7B8, 0, 4)]
for _v in "abcd":
    HOOKS[f"startNewCourse_{_v}"] = _h(0x0046DDD0, f"c3ev_newcourse_{_v}", [()], 0x1000, args=(), abi="default", extra=_NC)

# initLogFile / dynInit_initLogFile (c3ev_log_<v>)
for _v in ("ok", "nofile", "nomem"):
    HOOKS[f"initLogFile_{_v}"] = _h(0x004A0130, f"c3ev_log_{_v}", [()], 0x1000, args=(), abi="default",
                                    extra=[(0x00840930, 0, 0x10)])
    HOOKS[f"dynInit_initLogFile_{_v}"] = _h(0x004A0120, f"c3ev_log_{_v}", [()], 0x1000, args=(), abi="default",
                                            extra=[(0x00840930, 0, 0x10)])
