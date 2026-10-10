# C3 batch c3dm (round 26, 2026-10-10): golf_clean.exe functions outside the UI that earlier notes named without a
# hard blocker. Reimplementations in shim/src/re/c3dm.cpp, fixtures in re/frida/js/fixtures.d/c3dm.js, Purposes and
# per-jcc branch coverage in log/c3/c3dm_purpose.md.
#
# Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
#   Buf_dtor 0x00474810 (c3cn), StrList::dtor 0x00487260 (c3bx), KeyTable::dtor 0x004882f0 (c3cn),
#   freeLoggerBuf 0x004a01a0 (c3cn), NodeList::clear 0x00487310 (c3cn), listClear4a4d 0x004a4d50 (c3cn),
#   operatorDelete_4a4ffc 0x004a4ffc (c3cn)
# Hooked in THIS batch and called by this batch's other functions:
#   NodeList::dtor 0x00487390 <- NodeList::scalarDtor 0x004872f0
#   List480::dtor / BinkFrame::scalarDtor share one body but call nothing hooked here.

EXE = "golf_clean.exe"

_HEAP = [("$eblk", 0, 0x8000), ("$arena", 0, 0x2010), ("$freelog", 0, 0x404), ("$rec", 0, 0x40)]
_FL = [1, 0, 3, 2, 1, 0, 0xFFFFFFFF, 0xFFFFFFFE, 0x101, 0x80000000, 5, 4]


def _dt(addr, fixture, prefix, n=12, state=_HEAP, ret="pointer"):
    """scalar / deleting destructor (this, flags): every object with a flag mix, then every object with flag 1."""
    vecs = [(f"${prefix}{i}", _FL[i]) for i in range(n)] + [(f"${prefix}{i}", 1) for i in range(n)]
    return dict(module=EXE, addr=addr, abi="thiscall", ret=ret, args=["pointer", "uint32"], fixture=fixture,
                state=state, vectors=vecs)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- Part 1
HOOKS["Buf_deleteDtor"] = _dt(0x004747C0, "c3dm_dtors", "bf")
HOOKS["pairArray_deleteDtor"] = _dt(0x0049D070, "c3dm_dtors", "bf")
HOOKS["StrList::scalarDtor"] = _dt(0x00487240, "c3dm_dtors", "sl")
HOOKS["NodeListB::scalarDtor"] = _dt(0x00487A40, "c3dm_dtors", "sl")
HOOKS["NodeListC::scalarDtor"] = _dt(0x00487B60, "c3dm_dtors", "sl")
HOOKS["KeyTable::scalarDtor"] = _dt(0x004882D0, "c3dm_dtors", "kt")
HOOKS["loggerDeleteDtor"] = _dt(0x004A0160, "c3dm_dtors", "lg")
HOOKS["NodeList::scalarDtor"] = _dt(0x004872F0, "c3dm_dtors", "ht")
HOOKS["NodeList::clearScalarDtor"] = _dt(0x00487CE0, "c3dm_dtors", "nl")
HOOKS["BinkFrame::scalarDtor"] = _dt(0x00480410, "c3dm_dtors", "ll")
HOOKS["NodeList::dtor"] = dict(module=EXE, addr=0x00487390, abi="thiscall", ret="void", args=["pointer"],
                               fixture="c3dm_dtors", state=_HEAP, vectors=[(f"$ht{i}",) for i in range(12)])
HOOKS["List480::dtor"] = dict(module=EXE, addr=0x00480390, abi="thiscall", ret="void", args=["pointer"],
                              fixture="c3dm_dtors", state=_HEAP, vectors=[(f"$ll{i}",) for i in range(12)])
HOOKS["deletingDtor_401c30"] = dict(
    module=EXE, addr=0x00401C30, abi="thiscall", ret="pointer", args=["pointer", "uint32"], fixture="c3dm_dd401",
    state=_HEAP + [(0x00839650, 0, 4)],
    vectors=[(f"$dd{i}", _FL[i]) for i in range(12)] + [(f"$dd{i}", 1) for i in range(12)])
# listClearThunk 0x004a4d30 (this, v): twelve lists of listClear4a4d (head +4, node next +0x10, payload +8)
HOOKS["listClearThunk"] = dict(module=EXE, addr=0x004A4D30, abi="thiscall", ret="void", args=["pointer", "int"],
                               fixture="c3dm_dtors", state=_HEAP,
                               vectors=[(f"$lc{i}", v) for i, v in enumerate(
                                   [0, 1, -1, 0x28, 0x7FFFFFFF, -0x80000000, 7, 0x1234, 3, 99, -5, 0x40])])
HOOKS["HashTable::remove"] = dict(
    module=EXE, addr=0x004876C0, abi="thiscall", ret="int", args=["pointer", "pointer"], fixture="c3dm_hashremove",
    state=_HEAP,
    vectors=[("$hr", "$A0"), ("$hr", "$B0"), ("$hr", "$B1"), ("$hr", "$B2"), ("$hr", "$C0"), ("$hr", "$C1"),
             ("$hr", "$D1"), ("$hr", "$D0"), ("$hr", "$E0"), ("$hr", "$E1"), ("$hr", "$E2"), ("$hr", "$E3"),
             ("$hr", "$X3"), ("$hr", "$X4"), ("$hr", "$Z10"), ("$hr", "$ZFF"), ("$hr", "$Z7"), ("$hr", 0)])

# ---------------------------------------------------------------------------------------------------- Part 2
# genNoise 0x004673e0 (argument-free): one key per generator seed 0x00822d9c; the tables at 0x00838aa0 are pre-filled
# 0x6d so every store is visible. Callee Random::range 0x0045c1e0 (c3f) is hooked: the verifier re-runs these with it off.
_SEEDS = 12
for _i in range(_SEEDS):
    HOOKS[f"genNoise_s{_i}"] = dict(module=EXE, addr=0x004673E0, abi="default", ret="void", args=[],
                                    fixture=f"c3dm_noise_{_i}", state=[(0x00822D9C, 0, 4), (0x00838AA0, 0, 0x2C0)],
                                    vectors=[()])

# markTileKind11 0x0042f2c0 (argument-free): one key per map layout (fixture c3dm_kind_<k>). The callee propagateType11
# 0x0042f1c0 (c3k, held at C2) is hooked in the main build: the verifier re-runs these with it off.
for _k in range(8):
    HOOKS[f"markTileKind11_w{_k}"] = dict(module=EXE, addr=0x0042F2C0, abi="default", ret="void", args=[],
                                          fixture=f"c3dm_kind_{_k}", state=[(0x0056988C, 0, 2500)], vectors=[()])

# floodMark 0x0042f120 (x, y): the coverage words 0x0053caf0; recursion goes through the function's own address
_FM = [(8, 8), (15, 15), (12, 12), (13, 13), (10, 11), (10, 10), (19, 19), (8, 19), (19, 8), (14, 9), (30, 30),
       (32, 32), (34, 34), (25, 25), (2, 2), (40, 40), (11, 10), (9, 12)]
HOOKS["floodMark"] = dict(module=EXE, addr=0x0042F120, abi="default", ret="void", args=["int", "int"],
                          fixture="c3dm_flood", state=[(0x0053CAF0, 0, 5000)], vectors=_FM)

# ---------------------------------------------------------------------------------------------------- Part 3
# libjpeg 6a compression API over synthetic compress structs (fixtures c3dm_j*). State: each struct block ($c<i>, 0x600
# bytes: the cinfo, error manager, memory manager and the controller objects), its alloc_small pool ($p<i>) /
# comp_info array ($k<i>) / table block ($t<i>) when the row has one, and the shared recorder $rec.
# Hooked callees of other batches (re-run with the hooks off): jpeg_set_colorspace 0x004ae990 (c3bj), jpeg_set_quality
# 0x004ae740 (c3bx), std_huff_tables 0x004ae850, jpeg_add_quant_table 0x004ae600 (c3aa), jpeg_suppress_tables 0x004ae220,
# jpeg_abort 0x004afa60 (c3r), jpeg_alloc_quant_table 0x004afab0, jpeg_alloc_huff_table 0x004afad0.
# Hooked in THIS batch and called by this batch's other functions: jpeg_default_colorspace <- jpeg_set_defaults.


def _jstate(rows):
    st = []
    for i, (pool, comp, tables) in enumerate(rows):
        st.append((f"$c{i}", 0, 0x600))
        if pool:
            st.append((f"$p{i}", 0, 0x1100))
        if comp:
            st.append((f"$k{i}", 0, 0x348))
        if tables:
            st.append((f"$t{i}", 0, 0x1000))
    return st + [("$jrec", 0, 0x40)]


_DCS_FLAGS = [(False, True, False)] * 12
_SD_FLAGS = [(True, False, False)] * 6 + [(True, True, False), (True, True, True), (True, False, False), (True, True, False),
             (True, False, False), (True, True, False), (True, False, False), (True, False, True), (True, True, False),
             (True, True, True)]
_WS_FLAGS = [(False, False, False)] * 14
_SC_FLAGS = [(False, False, False), (False, False, False), (False, False, True), (False, False, True),
             (False, False, True), (False, False, False), (False, False, True), (False, False, True),
             (False, False, False), (False, False, True), (False, False, False), (False, False, False)]
_FC_FLAGS = [(False, False, False)] * 12

HOOKS["jpeg_default_colorspace"] = dict(
    module=EXE, addr=0x004AE910, abi="default", ret="void", args=["pointer"], fixture="c3dm_jdcs",
    state=_jstate(_DCS_FLAGS), vectors=[(f"$c{i}",) for i in range(12)])
HOOKS["jpeg_set_defaults"] = dict(
    module=EXE, addr=0x004AE760, abi="default", ret="void", args=["pointer"], fixture="c3dm_jdef",
    state=_jstate(_SD_FLAGS), vectors=[(f"$c{i}",) for i in range(16)])
# (cinfo, scanlines, num_lines): the scanline pointer is only recorded; counts cover 0, 1, below / equal / above the rows
# left, and the unsigned extremes
_WS_N = [5, 0, 1, 5, 3, 0xFFFFFFFF, 7, 2, 6, 4, 9, 1, 3, 0x20]
HOOKS["jpeg_write_scanlines"] = dict(
    module=EXE, addr=0x004AE400, abi="default", ret="uint32", args=["pointer", "pointer", "uint32"],
    fixture="c3dm_jwrite", state=_jstate(_WS_FLAGS),
    vectors=[(f"$c{i}", 0x1000 + 0x10 * i, _WS_N[i]) for i in range(14)]
            + [("$c0", 0x2000, 0xFFFFFFFF), ("$c0", 0x2010, 10), ("$c0", 0x2020, 11), ("$c3", 0x2030, 2), ("$c3", 0x2040, 3),
               ("$c8", 0x2050, 7), ("$c10", 0x2060, 1), ("$c1", 0x2070, 0)])
HOOKS["jpeg_start_compress"] = dict(
    module=EXE, addr=0x004AE380, abi="default", ret="void", args=["pointer", "int"], fixture="c3dm_jstart",
    state=_jstate(_SC_FLAGS),
    vectors=[(f"$c{i}", w) for i, w in enumerate([1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1])]
            + [("$c2", 0), ("$c3", 1), ("$c4", 0), ("$c0", -1), ("$c1", 0x100), ("$c9", 0)])
HOOKS["jpeg_finish_compress"] = dict(
    module=EXE, addr=0x004AE270, abi="default", ret="void", args=["pointer"], fixture="c3dm_jfinish",
    state=_jstate(_FC_FLAGS), vectors=[(f"$c{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------- Part 4
# The Window mouse dispatchers (thiscall, x, y, release). Windows $ignA (flags bit 0x200000), $ignB (byte +0xa0 bit 8),
# $plain (slots answer 1), $miss (the hit slot answers 0), $bare (no callbacks, no panels, no hot spots), $onlyCb,
# $onlyPanel and $both. Hot spots e0 = [0,100) x [0,100) and e1 = [50,150) x [50,150) (e1 is tested first).
# Hooked callee of another batch (re-run with the hook off): HotList::hitTestRect 0x00492b10 (C3).
_WIN_STATE = [("$rec", 0, 4 + 48 * 16), (0x0083AB2C, 0, 4)]
_PTS = [(10, 10), (60, 60), (120, 120), (200, 200), (100, 10), (-1, 5), (99, 99), (149, 50)]
_MV = ([("$ignA", 10, 10, 0), ("$ignA", 10, 10, 1), ("$ignB", 60, 60, 0), ("$ignB", 60, 60, 1), ("$both", 5, 5, 0)]
       + [("$plain", x, y, 0) for x, y in _PTS] + [("$miss", x, y, 0) for x, y in _PTS]
       + [("$bare", x, y, 0) for x, y in _PTS[:3]]
       + [("$plain", 10, 10, 1), ("$plain", 7, 8, 2), ("$bare", 5, 6, 1), ("$miss", 60, 60, -1),
          ("$onlyCb", 10, 10, 0), ("$onlyCb", 10, 10, 1), ("$onlyPanel", 10, 10, 0), ("$onlyPanel", 120, 120, 0),
          ("$onlyPanel", 10, 10, 1), ("$plain", 0x7FFFFFFF, -0x80000000, 0), ("$plain", 3, 4, 0x80000000)])
for _name, _addr in (("23c", 0x0047BF40), ("240", 0x0047C010), ("244", 0x0047C0E0), ("24c", 0x0047C290),
                     ("250", 0x0047C360), ("258", 0x0047C500)):
    HOOKS[f"Window::mouseDispatch{_name}"] = dict(
        module=EXE, addr=_addr, abi="thiscall", ret="void", args=["pointer", "int", "int", "int"],
        fixture="c3dm_win", state=_WIN_STATE, vectors=_MV)
# Window::click also runs the global callback 0x0083ab04: one key with it set, one with it null
HOOKS["Window::click"] = dict(
    module=EXE, addr=0x0047C1B0, abi="thiscall", ret="void", args=["pointer", "int", "int", "int"],
    fixture="c3dm_win", state=_WIN_STATE + [(0x0083AB04, 0, 4)], vectors=_MV)
HOOKS["Window::click_noglobal"] = dict(
    module=EXE, addr=0x0047C1B0, abi="thiscall", ret="void", args=["pointer", "int", "int", "int"],
    fixture="c3dm_win_noglobal", state=_WIN_STATE + [(0x0083AB04, 0, 4)], vectors=_MV[:24] + _MV[-8:])

# ---------------------------------------------------------------------------------------------------- Part 5
# Thin wrappers over fixture devices. c3dm_dev: widgets $w0..$w11 and sound objects $s0..$s11 (the device is null in every
# fourth), palettes $p0..$p5 (device present in the odd ones; slot 0 of the palette creates one) and the 0x28-byte-header
# entry table $tbl. c3dm_surf: surfaces $s0..$s11 over a swapped display (first thread frozen).
_DEV_STATE = [("$rec", 0, 0x40), ("$wblk", 0, 0x180), ("$sblk", 0, 0x600), ("$pblk", 0, 0xC0)]
_SURF_STATE = [("$rec", 0, 0x40), ("$sblk", 0, 0x240)]
HOOKS["Widget_clearArea"] = dict(
    module=EXE, addr=0x00478B30, abi="thiscall", ret="void", args=["pointer", "int"], fixture="c3dm_dev",
    state=_DEV_STATE, vectors=[(f"$w{i}", v) for i, v in enumerate([0, 1, -1, 0x7FFFFFFF, 5, 0x100, -0x80000000, 77, 8, 9, 10, 11])])
HOOKS["Snd::queryDevice4845d0"] = dict(
    module=EXE, addr=0x004845D0, abi="thiscall", ret="int", args=["pointer"], fixture="c3dm_dev",
    state=_DEV_STATE, vectors=[(f"$s{i}",) for i in range(12)])
HOOKS["Snd::status"] = dict(
    module=EXE, addr=0x004845E0, abi="thiscall", ret="int", args=["pointer"], fixture="c3dm_dev",
    state=_DEV_STATE, vectors=[(f"$s{i}",) for i in range(12)])
HOOKS["Palette::setEntries"] = dict(
    module=EXE, addr=0x004830E0, abi="thiscall", ret="int", args=["pointer", "pointer", "int", "int"],
    fixture="c3dm_dev", state=_DEV_STATE,
    vectors=[("$p1", "$tbl", 0, 4), ("$p1", "$tbl", 5, 3), ("$p1", "$tbl", 10, 0), ("$p1", "$tbl", 7, -2),
             ("$p0", "$tbl", 0, 8), ("$p2", "$tbl", 20, 16), ("$p1", 0, 0, 4), ("$p0", 0, 3, 2),
             ("$p3", "$tbl", 0, 64), ("$p4", "$tbl", 100, 100), ("$p5", "$tbl", 0, 1), ("$p5", "$tbl", 255, 1),
             ("$p3", "$tbl", 1, 1), ("$p0", "$tbl", -1, 3), ("$p2", "$tbl", 2, 200)])
HOOKS["Surface::releaseBacking"] = dict(
    module=EXE, addr=0x004838B0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dm_surf",
    state=_SURF_STATE, vectors=[(f"$s{i}",) for i in range(12)])
_IDS = [0, 1, -1, 0x7FFFFFFF, 7, 0x40, 9, 3, 0, 5, 100, -0x80000000]
HOOKS["Surface::createFromId"] = dict(
    module=EXE, addr=0x00483800, abi="thiscall", ret="int", args=["pointer", "int", "int", "int"], fixture="c3dm_surf",
    state=_SURF_STATE, vectors=[(f"$s{i}", _IDS[i], i * 3 - 4, 0x20 - i) for i in range(12)] + [("$s1", 5, 0, 0), ("$s2", 6, 1, 2)])
_WH = [(0, 5), (5, 0), (0, 0), (1, 1), (640, 480), (-1, 7), (7, -1), (0x10000, 0x10000), (3, 3), (800, 600), (2, 9), (4, 4)]
HOOKS["Surface::createSized"] = dict(
    module=EXE, addr=0x00483850, abi="thiscall", ret="int", args=["pointer", "int", "int", "int", "int"],
    fixture="c3dm_surf", state=_SURF_STATE,
    vectors=[(f"$s{i}", _WH[i][0], _WH[i][1], i, 0x30 - i) for i in range(12)] + [("$s1", 8, 8, 1, 2), ("$s4", 16, 4, 0, 0)])

# ---------------------------------------------------------------------------------------------------- Part 6
# Snd wrappers. Globals (saved and restored under a frozen first thread): the sound function table 0x0083af6c.., the list
# tail / head 0x0083af64 / 0x0083af68, the library flag 0x0083afc4 (0x0083af64..0x0083afcb is the state region of the table
# keys) and, for startSoundSlot, the first 12 live slot objects at 0x0080d840 (vtable pointers replaced for the key).
_SND_TBL = [("$rec", 0, 0x40), (0x0083AF64, 0, 0x68)]
_SND_P = [0, 1, -1, 0x7FFFFFFF, 5, 0x100, -0x80000000, 77]
HOOKS["releaseSoundObj"] = dict(module=EXE, addr=0x00484110, abi="default", ret="int", args=["int"],
                                fixture="c3dm_snd_lib", state=_SND_TBL, vectors=[(p,) for p in _SND_P])
HOOKS["releaseSoundObj_nolib"] = dict(module=EXE, addr=0x00484110, abi="default", ret="int", args=["int"],
                                      fixture="c3dm_snd_nolib", state=_SND_TBL, vectors=[(p,) for p in (0, 5, -1, 9)])
HOOKS["createSound4840e0"] = dict(module=EXE, addr=0x004840E0, abi="default", ret="int", args=["int", "int", "int"],
                                  fixture="c3dm_snd_lib", state=_SND_TBL,
                                  vectors=[(0, 0, 0), (1, 2, 3), (-1, 5, 7), (0x7FFFFFFF, 1, 1), (10, 10, 10), (3, 0, -3),
                                           (0x100, -0x80000000, 5), (9, 8, 7)])
HOOKS["createSound4840e0_nolib"] = dict(module=EXE, addr=0x004840E0, abi="default", ret="int", args=["int", "int", "int"],
                                        fixture="c3dm_snd_nolib", state=_SND_TBL,
                                        vectors=[(0, 0, 0), (1, 2, 3), (-1, 5, 7), (9, 8, 7)])
for _s in range(10):
    HOOKS[f"clearSoundPtrs_p{_s}"] = dict(module=EXE, addr=0x00484130, abi="default", ret="void", args=[],
                                          fixture=f"c3dm_snd_clear_{_s}", state=[(0x0083AF64, 0, 0x68)], vectors=[()])
_SND_OBJ = [("$oblk", 0, 12 * 0x80), ("$rec", 0, 0x40)]
HOOKS["Snd::stop484550"] = dict(module=EXE, addr=0x00484550, abi="thiscall", ret="int", args=["pointer"],
                                fixture="c3dm_snd_objs", state=_SND_OBJ, vectors=[(f"$o{i}",) for i in range(12)])
_AF = [0, 1, 2, 3, -1, 0x7FFFFFFF, 6, 7, 0x100, -2, 5, 4]
HOOKS["Snd::applyField484750"] = dict(module=EXE, addr=0x00484750, abi="thiscall", ret="int", args=["pointer", "int"],
                                      fixture="c3dm_snd_objs", state=_SND_OBJ,
                                      vectors=[(f"$o{i}", _AF[i]) for i in range(12)])
HOOKS["Snd::applyField4847b0"] = dict(module=EXE, addr=0x004847B0, abi="thiscall", ret="int", args=["pointer", "int"],
                                      fixture="c3dm_snd_objs", state=_SND_OBJ,
                                      vectors=[(f"$o{i}", _AF[i]) for i in range(12)])
_MODES = [0, 1, 3, 4, 5, 7, 0x104, 0x100, -1, 8, 0x0F, 2]
HOOKS["Snd::applyMode484e30"] = dict(module=EXE, addr=0x00484E30, abi="thiscall", ret="int", args=["pointer", "int", "int"],
                                     fixture="c3dm_snd_objs", state=_SND_OBJ,
                                     vectors=[(f"$o{i}", _AF[i], _MODES[i]) for i in range(12)])
_SND_LIST = [("$nblk", 0, 8 * 0x80), (0x0083AF64, 0, 8), ("$rec", 0, 0x40)]
HOOKS["Snd::unlinkFromList"] = dict(module=EXE, addr=0x004844E0, abi="thiscall", ret="int", args=["pointer"],
                                    fixture="c3dm_snd_list5", state=_SND_LIST,
                                    vectors=[("$n0",), ("$n1",), ("$n2",), ("$n3",), ("$n4",), ("$x0",), ("$x1",)])
HOOKS["Snd::unlinkFromList_two"] = dict(module=EXE, addr=0x004844E0, abi="thiscall", ret="int", args=["pointer"],
                                        fixture="c3dm_snd_list2", state=_SND_LIST, vectors=[("$n0",), ("$n1",)])
HOOKS["Snd::unlinkFromList_one"] = dict(module=EXE, addr=0x004844E0, abi="thiscall", ret="int", args=["pointer"],
                                        fixture="c3dm_snd_list1", state=_SND_LIST, vectors=[("$n0",), ("$x0",)])
HOOKS["startSoundSlot"] = dict(module=EXE, addr=0x00448200, abi="default", ret="int", args=["int"],
                               fixture="c3dm_snd_slots", state=[("$rec", 0, 0x40)], vectors=[(i,) for i in range(12)])
