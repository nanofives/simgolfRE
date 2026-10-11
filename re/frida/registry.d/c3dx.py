# C3 batch c3dx (round 28, 2026-10-11) of golf_clean.exe UI and text rows: the scrollbar handlers, TextView::select /
# updateSelection and buildStatusText. Reimplementations in shim/src/re/c3dx.cpp, fixtures in
# re/frida/js/fixtures.d/c3dx.js, Purposes and per-jcc branch coverage in log/c3/c3dx_purpose.md.
#
# Every key freezes the game's first thread (re/frida/js/freeze.js): the scrollbar handlers write the drag-capture
# globals 0x0083ab30 / 0x0083ab34, 0x0083ab2c and 0x0083ff18, buildStatusText the shared text buffer 0x0051a068.
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   TextView::updateSelection / select -> ListModel::selectedId 0x00489950 (c3u), getFieldValueByKind 0x004a1370 (c3s)
#   buildStatusText -> buildGolferName 0x004676e0 (c3m)

_SB_N = 8
_SB_STATE = ([(f"$sb{i}", 0, 0x1400) for i in range(_SB_N)] + [("$rec", 0, 0x400),
             (0x0083AB2C, 0, 4), (0x0083AB30, 0, 4), (0x0083AB34, 0, 4), (0x0083FF18, 0, 4)])

# Boundary points of the x range 10..50 and the y range 8..40 the fixture gives every scrollbar.
_DOWN_PTS = [(0, 0), (9, 7), (10, 8), (30, 20), (50, 40), (51, 41), (-4, -4), (1000, 1000), (9, 41), (51, 7)]
_DOWN_VECTORS = [(f"$sb{i}", x, y) for i in range(_SB_N) for (x, y) in _DOWN_PTS]
_BY_VECTORS = [(f"$sb{i}", d) for i in range(_SB_N) for d in (1, 2, 0, 3, -1)]
_KEYS = (0x21, 0x22, 0x23, 0x24, 0x25, 0x20, 0, -1)
_KEY_VECTORS = [(f"$sb{i}", 0x1234, k) for i in range(_SB_N) for k in _KEYS]

_TV_N = 13
_TV_STATE = ([(f"$tv{i}", 0, 0x800) for i in range(_TV_N)] + [(f"$tv{i}", 0x1460, 0x400) for i in range(_TV_N)]
             + [(f"$tv{i}", 0x1f00, 0x200) for i in range(_TV_N)] + [("$rec", 0, 0x400)])
_SELECT_VECTORS = [(f"$tv{i}", w) for i in range(_TV_N) for w in (-2, -1, 0, 3, -9)]
_UPDATE_VECTORS = [(f"$tv{i}", a) for i in range(_TV_N) for a in (0, 5)]

_STATUS_STATE = [(0x0051A068, 0, 0x200), (0x005695A8, 0, 0x80), (0x0058A528, 0, 0x20), (0x005A9CD0, 0, 4)]
_STATUS_GOLFERS = [-1] + list(range(20, 30))
_STATUS_PRIOS = [-0x80000000, -200, -129, -128, -1, 0, 4, 5, 6, 99, 100, 101, 127, 128, 300, 0x7FFFFFFF]
_STATUS_VECTORS = [(g, p) for g in _STATUS_GOLFERS for p in _STATUS_PRIOS]


def _sb(addr, fixture, vectors, args):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret="int", args=["pointer"] + args,
                fixture=fixture, state=_SB_STATE, vectors=vectors)


def _tv(addr, fixture, vectors):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret="int", args=["pointer", "int"],
                fixture=fixture, state=_TV_STATE, vectors=vectors)


def _status(fixture):
    return dict(module="golf_clean.exe", addr=0x00406D50, abi="default", ret="int", args=["int", "int"],
                fixture=fixture, state=_STATUS_STATE, vectors=_STATUS_VECTORS)


HOOKS = {
    # scrollbarOnDown(this, x, y) with the drag capture free (0x0083ab30 = 0) and held by another widget.
    "scrollbarOnDown": _sb(0x00496C30, "c3dx_sb_free", _DOWN_VECTORS, ["int", "int"]),
    "scrollbarOnDown_held": _sb(0x00496C30, "c3dx_sb_held", _DOWN_VECTORS, ["int", "int"]),
    # scrollbarScrollBy(this, dir) and scrollbarOnKey(this, unused, key).
    "scrollbarScrollBy": _sb(0x004973C0, "c3dx_sb_held", _BY_VECTORS, ["int"]),
    "scrollbarOnKey": _sb(0x00496E30, "c3dx_sb_held", _KEY_VECTORS, ["int", "int"]),
    # TextView::select(this, which) and updateSelection(this, unused).
    "TextView::select": _tv(0x00490960, "c3dx_text", _SELECT_VECTORS),
    "TextView::updateSelection": _tv(0x00490A40, "c3dx_text", _UPDATE_VECTORS),
    # buildStatusText(golfer, priority) under four priority-byte / saved-text settings.
    "buildStatusText": _status("c3dx_status_low"),
    "buildStatusText_mid": _status("c3dx_status_mid"),
    "buildStatusText_high": _status("c3dx_status_high"),
    "buildStatusText_one": _status("c3dx_status_one"),
}


# ---- stampCourseTiles 0x0040e000: (x, y, kind, flags) ------------------------------------------------------------
# The tables it writes, its seeds' inputs and everything rebuildHeightfield 0x0042f7a0 recomputes (as c3n's _HF_STATE
# and c3ah's _OBJ_STATE): the level table, tile type / byte / word grids, the mark grid, the placed-object table
# (widened by 0x10 so the word after the table is compared), the RNG seed and the two course-centre bytes.
_STAMP_STATE = [(0x0053CAF0, 0, 5000), (0x005722E8, 0, 2564), (0x0056988C, 0, 2500), (0x0053BBAC, 0, 2500),
                (0x005A4998, 0, 2700), (0x0058BCB8, 0, 0x1010), (0x00822D9C, 0, 4), (0x00585860, 0, 2),
                (0x0051B770, 0, 20000), (0x00543018, 0, 2500), (0x005619A0, 0, 2500), (0x004C2E04, 0, 4),
                (0x0059E7B8, 0, 4)]
_STAMP_POS = [(2, 3), (10, 12), (20, 5), (31, 33), (7, 40), (40, 7)]
_STAMP_KINDS = [0, 3, 5, 6, 7, 0xA, 0xC, 0xF, 0x10, 0x11]
_STAMP_FLAGS = [-2, -1, 0, 0x10]
_STAMP_ALL = [(_STAMP_POS[(ki + fi) % len(_STAMP_POS)][0], _STAMP_POS[(ki + fi) % len(_STAMP_POS)][1], k, f)
              for ki, k in enumerate(_STAMP_KINDS) for fi, f in enumerate(_STAMP_FLAGS)]
_STAMP_SMALL = [(p[0], p[1], k, f) for (p, k, f) in
                [(_STAMP_POS[i % len(_STAMP_POS)], k, f) for i, (k, f) in
                 enumerate([(0, 0), (0, 0x120), (6, 0), (6, 0x40), (0xA, 0), (0xA, 0x1), (0xF, 0), (0x10, 0), (7, 0x400),
                            (3, 0x1000), (5, 0), (0xC, 0x20)])]]
_STAMP_CT = [(p[0], p[1], 0xC, f) for p in _STAMP_POS for f in (-2, 0, 0x20)]


def _stamp(fixture, vectors):
    return dict(module="golf_clean.exe", addr=0x0040E000, abi="default", ret="int", args=["int", "int", "int", "int"],
                fixture=fixture, state=_STAMP_STATE, vectors=vectors)


HOOKS.update({
    "stampCourseTiles": _stamp("c3dx_stamp_empty", _STAMP_ALL),
    "stampCourseTiles_one": _stamp("c3dx_stamp_one", _STAMP_SMALL),
    "stampCourseTiles_some": _stamp("c3dx_stamp_some", _STAMP_SMALL),
    "stampCourseTiles_full": _stamp("c3dx_stamp_full", _STAMP_SMALL[:6] + [(3, 3, 0, -1)]),
    "stampCourseTiles_ct1": _stamp("c3dx_stamp_ct1", _STAMP_CT),
    "stampCourseTiles_ct2": _stamp("c3dx_stamp_ct2", _STAMP_CT),
})


# ---- scrollbarThumbRect 0x004974d0: (this, out) over the 25 thumb objects of c3dx_thumb ---------------------------
_TH_N = 25
_THUMB_STATE = [(f"$th{i}", 0, 0x700) for i in range(_TH_N)] + [("$out", 0, 0x20)]
HOOKS["scrollbarThumbRect"] = dict(module="golf_clean.exe", addr=0x004974D0, abi="thiscall", ret="int",
                                   args=["pointer", "pointer"], fixture="c3dx_thumb", state=_THUMB_STATE,
                                   vectors=[(f"$th{i}", "$out") for i in range(_TH_N)])


# ---- radioSelect 0x004a0350: (group, id) over the ten groups of c3dx_radio ---------------------------------------
# Hooked callee: Button::setMode 0x004890e0 (c3u).
_RG_KEYS = [[10, 20, 30], [10, 20, 30, 20], [5, 6], [7, 8, 9], [1, 2], [], [4, 4, 4], [11, 12], [21, 22, 23], [31, 32, 33]]
_RG_CUR = [20, 20, -1, 8, 2, 3, 4, 99, 21, 32]
_RG_STATE = ([(f"$rg{g}", 0, 0x100) for g in range(len(_RG_KEYS))]
             + [(f"$rb{g}_{i}", 0x570, 0x80) for g, ks in enumerate(_RG_KEYS) for i in range(len(ks))]
             + [(f"$rb{g}_{i}", 0x5e0, 0x10) for g, ks in enumerate(_RG_KEYS) for i in range(len(ks))]
             + [("$rec", 0, 0x800)])
_RG_VECTORS = [(f"$rg{g}", i) for g, ks in enumerate(_RG_KEYS)
               for i in sorted(set(ks + [_RG_CUR[g], 999, -1]))]
HOOKS["radioSelect"] = dict(module="golf_clean.exe", addr=0x004A0350, abi="thiscall", ret="int",
                            args=["pointer", "int"], fixture="c3dx_radio", state=_RG_STATE, vectors=_RG_VECTORS)


# ---- netListRemove 0x0049bec0: (list, key) over the nine lists of c3dx_nodelist ------------------------------------
_NL_KEYS = [[5, 6, 7], [5, 6, 7], [9], [3, 4], [], [1, 2, 3, 4], [8, 9, 10], [2, 2, 3], [6, 7]]
_NL_STATE = ([(f"$nl{g}", 0, 0x40) for g in range(len(_NL_KEYS))]
             + [(f"$nn{g}_{i}", 0, 0x60) for g, ks in enumerate(_NL_KEYS) for i in range(len(ks))]
             + [("$rec", 0, 0x800)])
_NL_VECTORS = [(f"$nl{g}", k) for g, ks in enumerate(_NL_KEYS) for k in sorted(set(ks + [99, -1]))]
HOOKS["netListRemove"] = dict(module="golf_clean.exe", addr=0x0049BEC0, abi="thiscall", ret="int",
                              args=["pointer", "int"], fixture="c3dx_nodelist", state=_NL_STATE,
                              vectors=_NL_VECTORS)


# ---- Snd::start 0x00484e70 and SndStream::open484c20 0x00484c20 over the ten Snd objects of c3dx_snd_* ---------------
# Hooked callee: createSound4840e0 0x004840e0 (c3dm).
_SN_N = 10
_SN_STATE = [(f"$sn{i}", 0, 0x100) for i in range(_SN_N)] + [("$rec", 0, 0x800)]
for _mode in ("ok", "fail", "nolib"):
    _sfx = "" if _mode == "ok" else "_" + _mode
    HOOKS["Snd::start" + _sfx] = dict(module="golf_clean.exe", addr=0x00484E70, abi="thiscall", ret="int",
                                      args=["pointer"], fixture="c3dx_snd_" + _mode, state=_SN_STATE,
                                      vectors=[(f"$sn{i}",) for i in range(_SN_N)])
    HOOKS["SndStream::open484c20" + _sfx] = dict(module="golf_clean.exe", addr=0x00484C20, abi="thiscall",
                                                 ret="int", args=["pointer", "int"], fixture="c3dx_snd_" + _mode,
                                                 state=_SN_STATE,
                                                 vectors=[(f"$sn{i}", n) for i in range(_SN_N) for n in (0, 5, -1)])


# ---- listCurrentIndex 0x004941e0: (this) over the twelve objects of c3dx_listcur ------------------------------------
# The head of every list is non-NULL: the empty-head arm reads address 4 and faults in the original.
_LC_N = 12
_LC_STATE = [(f"$lc{i}", 0x1480, 0x300) for i in range(_LC_N)] + [(f"$lc{i}", 0x2d90, 0x200) for i in range(_LC_N)]     + [(f"$lc{i}", 0, 0x10) for i in range(_LC_N)]
HOOKS["listCurrentIndex"] = dict(module="golf_clean.exe", addr=0x004941E0, abi="thiscall", ret="int",
                                 args=["pointer"], fixture="c3dx_listcur", state=_LC_STATE,
                                 vectors=[(f"$lc{i}",) for i in range(_LC_N)])


# ---- movieSelectStream 0x0049cf50: (this, n) over the twelve movie objects of c3dx_movie -----------------------------
# Object 3 has a NULL array (and the enable dword 0 is object 3's partner in -1 results via object 11's enable -1 case).
_MV_N = 13
_MV_STATE = [(f"$mv{i}", 0xb0, 0x20) for i in range(_MV_N)] + [(f"$mv{i}", 0x930, 0x30) for i in range(_MV_N)]
HOOKS["movieSelectStream"] = dict(module="golf_clean.exe", addr=0x0049CF50, abi="thiscall", ret="int",
                                  args=["pointer", "int"], fixture="c3dx_movie", state=_MV_STATE,
                                  vectors=[(f"$mv{i}", n) for i in range(_MV_N) for n in (-3, -1, 0, 1, 2, 3, 4, 6)])


# ---- listPanelReadValue 0x0049e7d0: (this, a, b) over the eight list panels of c3dx_listpanel -----------------------
# Hooked callee: Widget_value 0x00477580 (c3c). The state is the model window R+0x20 .. R+0x100 of each panel.
_LP_DS = [0x400, 0x440, 0x480, 0x400, 0x440, 0x480, 0x400, 0x440, 0x400]
_LP_STATE = [(f"$lps{i}", 0x24 + ds + 0x20, 0xe0) for i, ds in enumerate(_LP_DS)] + [("$rec", 0, 0x800)]
_LP_VECTORS = [(f"$lp{i}", a, b) for i in range(len(_LP_DS)) for a in (0, 5, 24, 100, -7)
               for b in (0, 1, 5, 12, 30, 100, -4)]
HOOKS["listPanelReadValue"] = dict(module="golf_clean.exe", addr=0x0049E7D0, abi="thiscall", ret="int",
                                   args=["pointer", "int", "int"], fixture="c3dx_listpanel", state=_LP_STATE,
                                   vectors=_LP_VECTORS)

# The vtordisp thunk 0x00491b60 (input this + 0x600, displacement 0x600 in front): run with listPanelReadValue's hook
# off for the original arm (a tail jmp lands in the hook otherwise).
HOOKS["listPanelReadValue_vtordisp"] = dict(
    module="golf_clean.exe", addr=0x00491B60, abi="thiscall", ret="int", args=["pointer", "int", "int"],
    fixture="c3dx_listpanel", state=_LP_STATE + [(f"$lps{i}", 0x60c, 0x10) for i in range(len(_LP_DS))],
    vectors=[(f"$lpu{i}", a, b) for i in range(len(_LP_DS)) for a in (0, 24, -7) for b in (0, 5, 30)])


# ---- List401c70_dtor 0x00401c70 / C4021e0_dtor 0x004021e0: (P) over the twelve lists of c3dx_dtorlist ---------------
_DL_N = 12
_DL_NODES = [3, 1, 2, 0, 2, 4, 2, 3, 1, 5, 2, 3]
_DL_STATE = ([(f"$dl{g}", 0, 0x80) for g in range(_DL_N)]
             + [(f"$dn{g}_{i}", 0, 0x60) for g in range(_DL_N) for i in range(_DL_NODES[g])] + [("$rec", 0, 0x1000)])
for _name, _addr in (("List401c70_dtor", 0x00401C70), ("C4021e0_dtor", 0x004021E0)):
    HOOKS[_name] = dict(module="golf_clean.exe", addr=_addr, abi="thiscall", ret="int", args=["pointer"],
                        fixture="c3dx_dtorlist", state=_DL_STATE, vectors=[(f"$dp{g}",) for g in range(_DL_N)])

# ---- Window::doLayout 0x00480d70: (this) over the ten windows of c3dx_layout -----------------------------------------
# Hooked callee: Window::visible 0x004801f0 (c3c).
_WL_N = 10
HOOKS["Window::doLayout"] = dict(module="golf_clean.exe", addr=0x00480D70, abi="thiscall", ret="int",
                                 args=["pointer"], fixture="c3dx_layout",
                                 state=[(f"$wl{i}", 0, 0x700) for i in range(_WL_N)] + [("$rec", 0, 0x800),
                                                                                           (0x0083AB2C, 0, 4)],
                                 vectors=[(f"$wl{i}",) for i in range(_WL_N)])

# ---- panelSubSlot60 0x004a4870: (Q) over the twelve objects of c3dx_slot60 -------------------------------------------
_PS_N = 12
HOOKS["panelSubSlot60"] = dict(module="golf_clean.exe", addr=0x004A4870, abi="thiscall", ret="int",
                               args=["pointer", "int"], fixture="c3dx_slot60",
                               state=[(f"$pb{i}", 0, 0x400) for i in range(_PS_N)] + [("$rec", 0, 0x800)],
                               vectors=[(f"$pq{i}", (0, 1, 7, -5)[i % 4]) for i in range(_PS_N)])  # `ret 4`: the argument is unused


# ---- ListBox::selectById 0x00489ed0: (S, id) over the nine list boxes of c3dx_selectbyid ---------------------------
# Hooked callee: ListBox::setSelection 0x00489f50 (c3ct) and, under it, ListModel::idAt 0x00489a30 (c3v).
_SI_KEYS = [[10, 20, 30], [10, 20, 30], [5, 5, 7], [], [], [8, 9], [1, 2], [3, 4, 5, 6], [7, 8, 9]]
HOOKS["ListBox::selectById"] = dict(
    module="golf_clean.exe", addr=0x00489ED0, abi="thiscall", ret="int", args=["pointer", "int"],
    fixture="c3dx_selectbyid", state=[(f"$sx{i}", 0, 0x600) for i in range(len(_SI_KEYS))] + [("$rec", 0, 0x1000)],
    vectors=[(f"$sx{i}", k) for i, ks in enumerate(_SI_KEYS) for k in sorted(set(ks + [99, -1]))])


# ---- snapshotTables 0x00462800 / initRecords 0x00401750: argument-free, one call per key -------------------------------
# The span of the ten tables (and the two gap dwords) is one state region; initRecords' regions are the position records
# and the record banks. Hooked callees of initRecords: resetRecordBank 0x00401000 (c3e), placeRecord 0x00401040 (c3k).
for _k in range(10):
    HOOKS[f"snapshotTables_p{_k}"] = dict(module="golf_clean.exe", addr=0x00462800, abi="default", ret="int", args=[],
                                          fixture=f"c3dx_snap{_k}", state=[(0x0082415C, 0, 0x140A8)], vectors=[()])
for _k in range(3):
    HOOKS[f"initRecords_p{_k}"] = dict(module="golf_clean.exe", addr=0x00401750, abi="default", ret="int", args=[],
                                       fixture=f"c3dx_records{_k}",
                                       state=[(0x0056D1D8, 0, 0x1770), (0x004E6D20, 0, 0x490)], vectors=[()])


# ---- ListModel::append 0x00489890, TextView::addText 0x0048df20, ListBox::addItem 0x0048c640, sendCommand 0x004a09a0 --------
# Every key runs under the heap stand-in (fixtures c3dx_lm*, c3dx_tvadd*, c3dx_lbadd*), so both arms get the same blocks.
# Hooked callees (the verifier re-runs these keys with them off): S3List_add 0x00401d10 and postMessageLine 0x004a0600
# (c3dd); inside this batch sendCommand -> ListModel::append / ListBox::addItem.
_LIST_STATE = [("$blk", 0, 0x20000), ("$arena", 0, 0x8010), ("$freelog", 0, 0x404), ("$rec", 0, 0x400),
               (0x00839650, 0, 4)]


def _lst(addr, args, fixture, vectors, ret="int"):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret=ret, args=args, fixture=fixture,
                state=_LIST_STATE, vectors=vectors)


_LM_VEC = [(f"$lm{i}", t, x) for i in range(12) for t in (0, "$t0", "$t1") for x in (0x51, -3)]
# failure keys: only lists without a heap (malloc kind): a failed heapAlloc shows a message box and exits
_LM_FAIL = [(f"$lm{i}", "$t0", 0x61) for i in (0, 3, 6, 10)]
HOOKS["ListModel::append"] = _lst(0x00489890, ["pointer", "pointer", "int"], "c3dx_lm", _LM_VEC)
HOOKS["ListModel::append_fail0"] = _lst(0x00489890, ["pointer", "pointer", "int"], "c3dx_lm_fail0", _LM_FAIL)
HOOKS["ListModel::append_fail1"] = _lst(0x00489890, ["pointer", "pointer", "int"], "c3dx_lm_fail1", _LM_FAIL)

_TV_VEC = ([(f"$tv{i}", f"$t{j}", (i + j) % 2) for i in range(12) for j in range(6)]
           + [(f"$tv{i}", "$t0", 0x100) for i in range(4)] + [(f"$tv{i}", "$t1", 0x1ff) for i in range(4)]
           + [(f"$tv{i}", 0, f) for i in range(3) for f in (0, 1)])
_TV_FAIL = [(f"$tv{i}", f"$t{i % 4}", i % 2) for i in (0, 3, 6, 10)]
HOOKS["TextView::addText"] = _lst(0x0048DF20, ["pointer", "pointer", "int"], "c3dx_tvadd", _TV_VEC)
HOOKS["TextView::addText_fail0"] = _lst(0x0048DF20, ["pointer", "pointer", "int"], "c3dx_tvadd_fail0", _TV_FAIL)

_LB_VEC = [(f"$lb{i}", t, x, f) for i in range(12) for (t, x, f) in (("$t0", 0x11, 0), ("$t1", -2, 1), (0, 5, 0))]
_LB_FAIL = [(f"$lb{i}", "$t0", 0x21, i % 2) for i in (0, 2, 5, 9)]
HOOKS["ListBox::addItem"] = _lst(0x0048C640, ["pointer", "pointer", "int", "int"], "c3dx_lbadd", _LB_VEC)
HOOKS["ListBox::addItem_fail0"] = _lst(0x0048C640, ["pointer", "pointer", "int", "int"], "c3dx_lbadd_fail0", _LB_FAIL)
HOOKS["ListBox::addItem_fail1"] = _lst(0x0048C640, ["pointer", "pointer", "int", "int"], "c3dx_lbadd_fail1", _LB_FAIL)

_SC_VEC = [(f"$lb{i}", a, b) for i in range(12) for (a, b) in (("$t0", 0x31), (0, 0x32), ("$t2", -4))]
HOOKS["sendCommand"] = _lst(0x004A09A0, ["pointer", "pointer", "int"], "c3dx_lbadd", _SC_VEC)
HOOKS["sendCommand_fail0"] = _lst(0x004A09A0, ["pointer", "pointer", "int"], "c3dx_lbadd_fail0",
                                  [(f"$lb{i}", "$t0", 0x41) for i in (0, 2, 5, 9)])


# ---- flipBmpRows 0x00475410: (pixels, header) over the eighteen bitmaps of c3dx_bmp ---------------------------------------
# stdcall, no value. operator new 0x004a57f1 and the free behind operator delete 0x004a4ffc run under the heap stand-in;
# operator delete itself (c3cz) is a hooked callee of the verifier's callee-off run.
HOOKS["flipBmpRows"] = dict(module="golf_clean.exe", addr=0x00475410, abi="stdcall", ret="void",
                            args=["pointer", "pointer"], fixture="c3dx_bmp", state=_LIST_STATE,
                            vectors=[(f"$bp{i}", f"$bh{i}") for i in range(18)])


# ---- TextView::setName 0x0048e010 (malloc / free) and Snd::setName 0x00484b80 (operator new / delete) -------------------
# Six objects of each x the names NULL, "plain text", "", 70 characters, "^" (fixture c3dx_names); TextView::setName also
# under a failing malloc (c3dx_names_fail0 -> returns 4). Snd::setName has no NULL check after operator new, so it has no
# failing key. Hooked callee of Snd::setName: operator delete 0x004a4ffc (c3cz).
_NAMES = [0, "$t0", "$t6", "$t7", "$t4"]
HOOKS["TextView::setName"] = _lst(0x0048E010, ["pointer", "pointer"], "c3dx_names",
                                  [(f"$sv{i}", n) for i in range(6) for n in _NAMES])
HOOKS["TextView::setName_fail0"] = _lst(0x0048E010, ["pointer", "pointer"], "c3dx_names_fail0",
                                        [(f"$sv{i}", n) for i in range(6) for n in ("$t0", "$t7", 0)])
HOOKS["Snd::setName"] = _lst(0x00484B80, ["pointer", "pointer"], "c3dx_names",
                             [(f"$sd{i}", n) for i in range(6) for n in _NAMES])


# ---- Flic::indexFrames 0x00481e40: (this, frames, extra) over the eight Flic objects of c3dx_flic ---------------------------
# Object 3 has no stream offset (result 7). operator new 0x004a57f1 / free (behind operator delete 0x004a4ffc, a hooked
# callee of c3cz) run under the heap stand-in; the failing key makes the first operator new return NULL (result 4).
_FLIC_ARGS = [(1, 0), (2, 1), (3, 2), (4, 3), (0, 5), (5, 0), (2, 2), (0x10003, 1), (3, 0x20001)]
HOOKS["Flic::indexFrames"] = _lst(0x00481E40, ["pointer", "int", "int"], "c3dx_flic",
                                  [(f"$fl{i}", a, b) for i in range(8) for (a, b) in _FLIC_ARGS])
HOOKS["Flic::indexFrames_fail0"] = _lst(0x00481E40, ["pointer", "int", "int"], "c3dx_flic_fail0",
                                        [(f"$fl{i}", a, b) for i in (0, 1, 4, 5) for (a, b) in ((2, 1), (4, 3))])


# ---- SndMusic::setFile 0x004842f0: (this, name) over the eight SndMusic objects of c3dx_music_* --------------------------
# Hooked callees (the verifier re-runs these keys with them off): createSound4840e0 0x004840e0 (c3dm) and operator delete
# 0x004a4ffc (c3cz). operator new / the free behind operator delete run under the heap stand-in.
# The last two vectors of each key resolve $tnl, whose getter clears the library flag (the no-library arm returns 1).
for _mode in ("ok", "fail"):
    HOOKS["SndMusic::setFile" + ("" if _mode == "ok" else "_" + _mode)] = _lst(
        0x004842F0, ["pointer", "pointer"], "c3dx_music_" + _mode,
        [(f"$sm{i}", n) for i in range(8) for n in ("$t0", "$t6", "$t7")] + [("$sm0", "$tnl"), ("$sm2", "$tnl")])


# ---- Snd::configure 0x00484a40, Snd::open 0x00484c80, Snd::reopen 0x00484d60 over the twelve Snd objects of c3dx_sndfam_* ---
# Original callees in the instance: Snd::setName 0x00484b80, SndStream::open484c20 0x00484c20, SndMusic::setFile 0x004842f0
# (this batch), createSound4840e0 0x004840e0 and releaseSoundObj 0x00484110 (c3dm), operator delete (c3cz). operator new / the
# free behind operator delete run under the heap stand-in.
_SF_FLAGS = [0, 1, 2, 4, 0x14, 0x40, 0x84, 0x100, 0x400, 0x7FF, 6, 0x10, 0x80, 0x180]
for _mode in ("ok", "fail"):
    _s = "" if _mode == "ok" else "_fail"
    HOOKS["Snd::configure" + _s] = _lst(
        0x00484A40, ["pointer", "pointer", "int"], "c3dx_sndfam_" + _mode,
        [(f"$sf{i}", n, f) for i in range(12) for n in ("$t0",) for f in _SF_FLAGS]
        + [(f"$sf{i}", "$t6", f) for i in (0, 3, 6) for f in (0x15, 0x5d7)], ret="void")
    HOOKS["Snd::open" + _s] = _lst(0x00484C80, ["pointer", "pointer"], "c3dx_sndfam_" + _mode,
                                   [(f"$sf{i}", n) for i in range(12) for n in ("$t0", "$t6")])
    HOOKS["Snd::reopen" + _s] = _lst(0x00484D60, ["pointer"], "c3dx_sndfam_" + _mode,
                                     [(f"$sf{i}",) for i in range(12)])
