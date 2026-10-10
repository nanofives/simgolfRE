# C3 batch c3dl (round 26, 2026-10-10): golf_clean.exe rows earlier batches dropped for the CRT heap or a Win32 import.
# Every key's fixture installs the shared stand-ins it needs (re/frida/js/heapstandin.js, importstandin.js, freeze.js); the
# arena + free log (heap) and the import call log are state regions, so allocations, frees and import calls are compared
# between the arms. Reimplementations: shim/src/re/c3dl.cpp. Fixtures: re/frida/js/fixtures.d/c3dl.js. Purposes and
# per-jcc tables: log/c3/c3dl_purpose.md.
#
# Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
#   Surface_free 0x00473ae0 (c3cz), MappedFile::dtor 0x00492dc0 (c3cy), operatorDelete_4a4ffc 0x004a4ffc (c3cn)
# Hooked in THIS batch and called by this batch's other functions:
#   Reader::closeFile <- Reader::free <- Reader::allocBuffers <- Reader::ctor

EXE = "golf_clean.exe"

_LOG = ("$implog", 0, 0x400)
_HEAP = [("$arena", 0, 0x4010), ("$freelog", 0, 0x404)]


def _h(addr, fixture, vectors, state, ret="int", args=("pointer",), abi="thiscall"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- callSoundHook94
# One key per (handle 0x0083afc0, hook result): the hook is a native callback whose call count is hookrec+0.
for _v in range(12):
    HOOKS[f"callSoundHook94_v{_v}"] = dict(
        module=EXE, addr=0x004840C0, abi="mscdecl", ret="int", args=[], fixture=f"c3dl_hook_{_v}",
        state=[("$hookrec", 0, 16), (0x0083AFC0, 0, 4), (0x0083AF94, 0, 4)], vectors=[()])

# ---------------------------------------------------------------------------------------------------- Timer::ctor / StrList::ctor
_OV = [(f"$o{i}",) for i in range(12)]
HOOKS["Timer::ctor"] = _h(0x00486C90, "c3dl_timer_ctor", _OV, [("$oblk", 0, 0x40 * 12)], ret="pointer")
HOOKS["StrList::ctor"] = _h(0x00487210, "c3dl_strlist_ctor", _OV, [("$oblk", 0, 0x40 * 12)], ret="pointer")

# ---------------------------------------------------------------------------------------------------- MappedFile::scalarDtor
# (this, flags): bit 0 hands the object to operator delete (the recording deallocator)
_FLAGS = [1, 0, 3, 2, 1, 0, 0xFFFFFFFF, 0xFFFFFFFE, 0x101, 0x80000000, 1, 5]
HOOKS["MappedFile::scalarDtor"] = _h(
    0x00492DA0, "c3dl_mf_scalar", [(f"$mf{i}", _FLAGS[i]) for i in range(12)],
    [("$mfblk", 0, 0x20 * 12), _LOG] + _HEAP, ret="pointer", args=("pointer", "uint32"))

# ---------------------------------------------------------------------------------------------------- Cursor::destroy
HOOKS["Cursor::destroy"] = _h(0x004884B0, "c3dl_cursor", [(f"$c{i}",) for i in range(12)],
                              [("$cblk", 0, 0x20 * 12), ("$subs", 0, 0x10 * 12), ("$rec", 0, 0x400), _LOG])

# ---------------------------------------------------------------------------------------------------- BinkPlayer::close
HOOKS["BinkPlayer::close"] = _h(0x00487060, "c3dl_bink", [(f"$b{i}",) for i in range(12)],
                                [("$bblk", 0, 0x10 * 12), _LOG])

# ---------------------------------------------------------------------------------------------------- Reader family
_RDS = [("$rblk", 0, 0x170 * 12), ("$rbufs", 0, 0x40 * 12), ("$fclog", 0, 0x40)]
_RV = [(f"$r{i}",) for i in range(12)]
HOOKS["Reader::closeFile"] = _h(0x00487F80, "c3dl_reader_close", _RV, _RDS)
HOOKS["Reader::free"] = _h(0x00487F30, "c3dl_reader_free", _RV, _RDS + _HEAP, ret="void")
_NV = [0x10, 0x20, 0, 1, 0x100, 0x20000000, 7, 0x3F00, 0x1FFF, 0x2001, 0x4000, 0x40]
for _k, _fx in (("", "c3dl_reader_alloc"), ("_f0", "c3dl_reader_alloc_f0"), ("_f1", "c3dl_reader_alloc_f1")):
    HOOKS[f"Reader::allocBuffers{_k}"] = _h(0x00487EE0, _fx, [(f"$r{i}", _NV[i]) for i in range(12)], _RDS + _HEAP,
                                            args=("pointer", "uint32"))
    HOOKS[f"Reader::ctor{_k}"] = _h(0x00487EA0, _fx, [(f"$r{i}", _NV[11 - i]) for i in range(12)], _RDS + _HEAP,
                                    ret="pointer", args=("pointer", "uint32"))

# ---------------------------------------------------------------------------------------------------- critical-section list
_LS = [("$lblk", 0, 0x30 * 12), ("$nblk", 0, 0x18 * 8 * 12), ("$dblk", 0, 0x40 * 8 * 12), ("$outs", 0, 0x40 * 6),
       _LOG] + _HEAP
HOOKS["listClear4a4b"] = _h(0x004A4B00, "c3dl_list", [(f"$l{i}",) for i in range(12)], _LS)

# (this, src, size, a3, a4, a5)
_APP_ARGS = ("pointer", "pointer", "uint32", "uint32", "uint32", "uint32")
_APP = [("$l0", "$src", 0, 1, 2, 3), ("$l0", "$src", 5, 4, 5, 6), ("$l0", 0, 4, 7, 8, 9), ("$l1", "$src", 1, 10, 11, 12),
        ("$l2", "$src", 0x21, 13, 14, 15), ("$l3", 0, 0, 16, 17, 18), ("$l5", "$src", 8, 19, 20, 21),
        ("$l10", "$src", 0x20000000, 22, 23, 24), ("$l4", 0, 0x40, 25, 26, 27), ("$l11", "$src", 3, 28, 29, 30),
        ("$l7", "$src", 0, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF), ("$l9", "$src", 0x7F, 31, 32, 33)]
for _k, _fx in (("", "c3dl_list"), ("_fnode", "c3dl_list_fnode"), ("_fdata", "c3dl_list_fdata")):
    HOOKS[f"listAppend4a4b60{_k}"] = _h(0x004A4B60, _fx, _APP, _LS, args=_APP_ARGS)

# (this, dst, outA, outB, outSize)
_POP = [("$l0", "$o0", "$o1", "$o2", "$o3"), ("$l1", "$o0", "$o1", "$o2", "$o3"), ("$l2", "$o0", "$o1", "$o2", "$o3"),
        ("$l2", 0, "$o1", "$o2", "$o3"), ("$l4", "$o0", 0, 0, 0), ("$l4", "$o4", "$o1", 0, "$o3"),
        ("$l5", "$o0", "$o1", "$o2", 0), ("$l6", "$o4", "$o1", "$o2", "$o3"), ("$l8", "$o5", "$o1", 0, "$o3"),
        ("$l9", "$o0", 0, "$o2", "$o3"), ("$l11", "$o0", "$o1", "$o2", "$o3"), ("$l3", 0, 0, 0, 0),
        ("$l7", "$o0", "$o1", "$o2", "$o3"), ("$l10", "$o0", "$o1", "$o2", "$o3")]
HOOKS["queuePop"] = _h(0x004A4C70, "c3dl_list", _POP, _LS, ret="uint32",
                       args=("pointer", "pointer", "pointer", "pointer", "pointer"))

# ---------------------------------------------------------------------------------------------------- netBeginOp0 / netBeginOp1
# (this, a): objects $n0..$n11 carry different +0xe8 flag words; the net global 0x008400b0 is 1 (on) or 0 (off)
_NETSTATE = [("$blk", 0, 0x10000), ("$rec", 0, 0x400), (0x008400B0, 0, 4)]
_NA = [0, 1, -1, 0x7FFFFFFF, 0x80000000, 5, 0x1234, 7, 0, 9, 2, 3]
for _n, _addr in (("netBeginOp0", 0x00497C20), ("netBeginOp1", 0x00497C70)):
    HOOKS[_n] = _h(_addr, "c3dl_net_on", [(f"$n{i}", _NA[i]) for i in range(12)], _NETSTATE, args=("pointer", "int"))
    HOOKS[f"{_n}_off"] = _h(_addr, "c3dl_net_off", [(f"$n{i}", _NA[11 - i]) for i in range(5)], _NETSTATE,
                            args=("pointer", "int"))

# ---------------------------------------------------------------------------------------------------- screenFlip
for _v in range(10):
    HOOKS[f"screenFlip_v{_v}"] = dict(module=EXE, addr=0x00483BA0, abi="mscdecl", ret="int", args=[],
                                      fixture=f"c3dl_flip_{_v}", state=[("$blk", 0, 0x10000), ("$rec", 0, 0x400),
                                                                        (0x0083AD50, 0, 4)], vectors=[()])

# ---------------------------------------------------------------------------------------------------- DC_calla0
# (this, a, b, c, d): d3 and d9 have no inner object (7); an `a` of 0 returns 0x10
_DC = [("$d0", "$a0", 1, 2, 3), ("$d1", "$a1", 0x10, 0x20, 0x30), ("$d2", "$a2", -1, -2, -3), ("$d3", "$a3", 4, 5, 6),
       ("$d4", 0, 7, 8, 9), ("$d5", "$a5", 0, 0, 0), ("$d6", "$a6", 0x7FFFFFFF, 0x80000000, 1), ("$d7", "$a7", 11, 12, 13),
       ("$d8", "$a8", 14, 15, 16), ("$d9", "$a9", 17, 18, 19), ("$d10", 0, 20, 21, 22), ("$d11", "$a11", 23, 24, 25),
       ("$d2", "$a10", 26, 27, 28)]
HOOKS["DC_calla0"] = _h(0x00475C20, "c3dl_dc", _DC, [("$blk", 0, 0x10000), ("$rec", 0, 0x400)],
                        args=("pointer", "pointer", "int", "int", "int"))

# ---------------------------------------------------------------------------------------------------- freeCursors
for _v in "abcdef":
    HOOKS[f"freeCursors_{_v}"] = dict(
        module=EXE, addr=0x0049EAD0, abi="mscdecl", ret="void", args=[], fixture=f"c3dl_cursors_{_v}",
        state=[("$blk", 0, 0x10000), ("$rec", 0, 0x400), (0x00840830, 0, 0x90), (0x0083AD50, 0, 4)] + _HEAP,
        vectors=[()])

# ---------------------------------------------------------------------------------------------------- loggerSetPath / loggerCtor
_LGS = [("$blk", 0, 0x10000), ("$oplog", 0, 0x60), ("$fclog", 0, 0x40)] + _HEAP
_LGV = [("$g0", "$p0"), ("$g1", "$p1"), ("$g2", "$p2"), ("$g3", "$p3"), ("$g4", "$p4"), ("$g5", "$p5"), ("$g6", 0),
        ("$g7", "$p6"), ("$g8", "$p7"), ("$g9", "$p8"), ("$g10", "$p2"), ("$g11", "$p3"), ("$g4", "$p5"), ("$g1", "$p4")]
for _k, _fx in (("", "c3dl_logger"), ("_f0", "c3dl_logger_f0")):
    HOOKS[f"loggerSetPath{_k}"] = _h(0x004A01D0, _fx, _LGV, _LGS, args=("pointer", "pointer"))
    HOOKS[f"loggerCtor{_k}"] = _h(0x004A0180, _fx, _LGV, _LGS, ret="pointer", args=("pointer", "pointer"))

# ---------------------------------------------------------------------------------------------------- List_reset / Widget_reset / addDropdownSpot
_DISP = [("$disp", 4, 0xC), (0x0083AD4C, 0, 4)]
_IMG = [("$iblk", 0, 0x2C0 * 12), ("$apool", 0, 0x4000), ("$arena", 0, 0x8010), ("$freelog", 0, 0x404),
        (0x0083D348, 0, 4), _LOG] + _DISP
_IV = [(f"$i{i}",) for i in range(12)]
for _k, _fx in (("", "c3dl_img_reset"), ("_fail", "c3dl_img_reset_f")):
    HOOKS[f"List_reset{_k}"] = _h(0x00478A20, _fx, _IV, _IMG, ret="uint32")
    HOOKS[f"Widget_reset{_k}"] = _h(0x00478A70, _fx, _IV, _IMG, ret="pointer")
_DD = [(f"$i{i}", i, i + 1, i * 3, -i, 0x100 + i, 7) for i in range(12)]
HOOKS["addDropdownSpot"] = _h(0x00478A90, "c3dl_img_dropdown", _DD, _IMG,
                              args=("pointer", "int", "int", "int", "int", "int", "int"))

# ---------------------------------------------------------------------------------------------------- constructors
# Twelve junk-filled objects per key (the whole block is state); the live globals the constructor copies take a pattern per
# key (three seeds), so a store of the wrong global or a missing store shows.
_CTS = [(0x0083AD4C, 0, 4), ("$disp", 4, 0xC), _LOG]
for _name, _addr, _fx, _size, _ret in (
        ("Image_ctor", 0x00474AE0, "image", 0x2C0, "pointer"), ("View_ctor", 0x00479C40, "view", 0x280, "pointer"),
        ("View4804a0::ctor", 0x004804A0, "view4804", 0x580, "pointer"), ("EditBox::ctor", 0x00486070, "editbox", 0x600, "pointer"),
        ("EditBox::resetFields", 0x00485FF0, "editreset", 0x600, "uint32"),
        ("ctorGlobalObj49c020", 0x0049C020, "globalobj", 0x600, "pointer")):
    for _s in range(3):
        HOOKS[f"{_name}_s{_s}"] = _h(_addr, f"c3dl_ctor_{_fx}_{_s}", [(f"$o{i}",) for i in range(12)],
                                     [("$oblk", 0, _size * 12)] + _CTS, ret=_ret)

# ---------------------------------------------------------------------------------------------------- resetViewState47cfd0
for _s in range(6):
    HOOKS[f"resetViewState47cfd0_s{_s}"] = dict(
        module=EXE, addr=0x0047CFD0, abi="mscdecl", ret="int", args=[], fixture=f"c3dl_resetview_{_s}",
        state=[("$oblk", 0, 0x40), (0x0083A4E8, 0, 0x10)] + _CTS, vectors=[()])

# ---------------------------------------------------------------------------------------------------- Surface_createFull
# (this, a1, a2, a3, a4, a5, a6): a4 = 0 keeps or sets the size field (+8); rows with +8 = 0 are i0, i3, i6, i9
_IMGX = _IMG + [("$eblk", 0, 0x10000), ("$rec", 0, 0x400)]
_CF = [(f"$i{i}", 10 + i, 20 + i, 30 + i, [0, 0x40, 0, 7, 0x80000000, 0][i % 6], 0x1000 + i, 99) for i in range(12)]
for _k, _fx in (("r0", "c3dl_createfull_0"), ("r0n", "c3dl_createfull_0n"), ("r0z", "c3dl_createfull_0z"),
                ("r5", "c3dl_createfull_5"), ("rneg", "c3dl_createfull_neg")):
    HOOKS[f"Surface_createFull_{_k}"] = _h(0x00474DD0, _fx, _CF, _IMGX,
                                           args=("pointer", "int", "int", "int", "int", "int", "int"))

# ---------------------------------------------------------------------------------------------------- sound library loader
_SND = [(0x0083AF6C, 0, 0x5C), _LOG]
for _k in ("loaded", "loadfail", "ok", "okc07", "oklo", "okhi", "old", "newer", "zero", "f1", "f5", "f11"):
    HOOKS[f"soundVersionCheck_{_k}"] = dict(module=EXE, addr=0x00483FC0, abi="mscdecl", ret="int", args=[],
                                            fixture=f"c3dl_snd_{_k}", state=_SND, vectors=[()])
for _v in range(10):
    HOOKS[f"freeSoundLib_v{_v}"] = dict(module=EXE, addr=0x00484060, abi="mscdecl", ret="int", args=[],
                                        fixture=f"c3dl_sndfree_{_v}", state=_SND + [(0x0083AFC4, 0, 4)], vectors=[()])
_H74 = [("$hookrec", 0, 16), (0x0083AFC0, 0, 4), (0x0083AF74, 0, 4)]
HOOKS["callSoundHook74"] = dict(module=EXE, addr=0x00484090, abi="mscdecl", ret="int", args=["int", "int"],
                                fixture="c3dl_hook74_on", state=_H74,
                                vectors=[(1, 2), (0, 0), (-1, 5), (0x7FFFFFFF, 1), (0x80000000, 0x80000000), (7, -9),
                                         (100, 200), (3, 4), (-3, -4), (0x12345, 0x6789), (2, 2), (5, 0)])
HOOKS["callSoundHook74_off"] = dict(module=EXE, addr=0x00484090, abi="mscdecl", ret="int", args=["int", "int"],
                                    fixture="c3dl_hook74_off", state=_H74, vectors=[(1, 2), (0, 0), (-1, 5)])

# ---------------------------------------------------------------------------------------------------- BinkPlayer::frame
HOOKS["BinkPlayer::frame"] = _h(0x00487180, "c3dl_binkframe", [(f"$p{i}",) for i in range(12)],
                                [("$pblk", 0, 0x10 * 12), ("$bks", 0, 0x40 * 12), ("$bfs", 0, 0x40 * 12), _LOG], ret="uint8")

# ---------------------------------------------------------------------------------------------------- setNewHeap
# (size, heap): heaps $h0 .. $h9 (fixture newHeap); the flagged heaps only get sizes that fit (a full flagged heap would
# reach the out-of-memory arm, which exits the process)
_NHV = [(0x10, "$h0"), (0x400, "$h0"), (0x100, "$h1"), (0x900, "$h2"), (0x20, "$h3"), (0x400, "$h3"), (0x20, "$h4"),
        (0x500, "$h5"), (0x100, "$h6"), (0x30, "$h7"), (0x200, "$h8"), (0x50, "$h9"), (0x20, 0), (0x1000, 0), (0, 0),
        (0x20000000, 0)]
_NHS = [("$blk", 0, 0x10000), ("$arena", 0, 0x8010), ("$freelog", 0, 0x404), (0x00839650, 0, 4), ("$imp", 0, 0x400)]
HOOKS["setNewHeap"] = dict(module=EXE, addr=0x004823C0, abi="mscdecl", ret="pointer", args=["uint32", "pointer"],
                           fixture="c3dl_newheap", state=_NHS, vectors=_NHV)
HOOKS["setNewHeap_f0"] = dict(module=EXE, addr=0x004823C0, abi="mscdecl", ret="pointer", args=["uint32", "pointer"],
                              fixture="c3dl_newheap_f0", state=_NHS,
                              vectors=[(0x20, 0), (0x40, 0), (0, 0), (0x100, 0)])

# ---------------------------------------------------------------------------------------------------- tooltip timers
# One fixture (c3dl_tips): windows $w0..$w7, HotLists $h0..$h11 (owner +0xc, entry arrays with rects and texts), Buttons
# $b0..$b11. Tooltip::show 0x00480220 is a recording stub on the A/B thread ($tiplog); the Timer family runs under timerEnv.
_TIP = [("$tblk", 0, 0x10000), ("$tapool", 0, 0x4000), ("$tiplog", 0, 0x210), ("$rec", 0, 0x400),
        (0x0083AAC8, 0, 4), (0x0083AAD8, 0, 4), (0x004E42C4, 0, 4), (0x0083AD44, 0, 4), (0x0083D348, 0, 4), _LOG] + _DISP
HOOKS["HotList::showTip"] = _h(0x00492BD0, "c3dl_tips",
                               [("$h0", 0), ("$h0", 2), ("$h1", 1), ("$h2", 0), ("$h3", 2), ("$h4", 0), ("$h5", 1), ("$h6", 4),
                                ("$h7", 0), ("$h8", 1), ("$h9", 3), ("$h10", 2), ("$h11", 0), ("$h3", 0), ("$h6", 2)],
                               _TIP, args=("pointer", "int"))
HOOKS["HotList::tipTimerCallback"] = dict(
    module=EXE, addr=0x00492BB0, abi="mscdecl", ret="void", args=["int", "pointer"], fixture="c3dl_tips", state=_TIP,
    vectors=[(0, "$h0"), (2, "$h1"), (1, 0), (0, "$h2"), (3, "$h3"), (0, 0), (1, "$h6"), (0, "$h7"), (2, "$h8"), (1, "$h9"),
             (0, "$h4"), (1, "$h11")])
HOOKS["Button::onTimer"] = _h(0x00488FD0, "c3dl_tips",
                              [("$b0", 0), ("$b1", 0), ("$b2", 0), ("$b3", 0), ("$b4", 0), ("$b5", 1), ("$b6", 1), ("$b7", 1),
                               ("$b8", 2), ("$b9", -1), ("$b10", 5), ("$b11", 1), ("$b0", 1), ("$b4", 1), ("$b9", 0),
                               ("$b10", 0), ("$b11", 0)], _TIP, args=("pointer", "int"))
HOOKS["Button::onTipTimer"] = _h(0x00488E90, "c3dl_tips",
                                 [(f"$b{i}", i, -i) for i in range(12)], _TIP, args=("pointer", "int", "int"))

# ---------------------------------------------------------------------------------------------------- Tooltip::show / Tooltip::hide
# show (this, text, rect): texts $t0 .. $t7, owners $w0 .. $w5, rects $r0 .. $r7 (fixture showEnv); keys a..f differ in the font
# row, the screen width of the fake display (NULL for c), and the visible-tip global (non-zero: Tooltip::hide draws first).
_SHOW = [("$tblk", 0, 0x10000), ("$tiplog", 0, 0x210), ("$rec", 0, 0x400), (0x0083AAC0, 0, 8), (0x0083AAC8, 0, 4),
         (0x0083A2C8, 0, 0x10), (0x0083AD50, 0, 4)]
_SV = [("$w0", "$t0", "$r0"), ("$w1", "$t1", "$r0"), ("$w2", "$t2", "$r1"), ("$w3", "$t3", "$r2"), ("$w4", "$t4", "$r3"),
       ("$w5", "$t5", "$r4"), ("$w0", "$t6", "$r5"), ("$w1", "$t7", "$r6"), ("$w2", "$t0", "$r7"), ("$w3", "$t6", "$r2"),
       ("$w4", "$t2", "$r4"), ("$w5", "$t1", "$r7"), ("$w0", "$t6", "$r7"), ("$w1", "$t4", "$r6")]
for _k in "abcdef":
    HOOKS[f"Tooltip::show_{_k}"] = _h(0x00480220, f"c3dl_tipshow_{_k}", _SV, _SHOW, args=("pointer", "pointer", "pointer"))
# NULL text or NULL rect: eax is the caller's on those paths (no caller passes either), so the result is not compared
HOOKS["Tooltip::show_null"] = _h(0x00480220, "c3dl_tipshow_a",
                                 [("$w0", 0, "$r0"), ("$w1", "$t0", 0), ("$w2", 0, 0), ("$w3", 0, "$r1")], _SHOW,
                                 ret="void", args=("pointer", "pointer", "pointer"))
for _i in range(6):
    HOOKS[f"Tooltip::hide_v{_i}"] = dict(module=EXE, addr=0x00480360, abi="mscdecl", ret="int", args=[],
                                         fixture=f"c3dl_tiphide_{_i}", state=[("$tiplog", 0, 0x210), (0x0083AAC0, 0, 8),
                                                                              (0x0083A2C8, 0, 0x10)], vectors=[()])

# ---------------------------------------------------------------------------------------------------- Button::beginPress / onMouseMove
_BTN = [("$tblk", 0, 0x10000), ("$rec", 0, 0x400), (0x0083AB2C, 0, 4), (0x0083AB30, 0, 8), (0x0083AB40, 0, 12), (0x0083AAC4, 0, 4),
        (0x0083D348, 0, 4), _LOG] + _DISP
# beginPress (this, a1, a2): key a leaves the capture global 0x0083ab30 at 0, key b presets it
_BPV = [(f"$b{i}", 0, 0) for i in range(12)]
HOOKS["Button::beginPress_a"] = _h(0x00488AB0, "c3dl_btn_bp_a", _BPV, _BTN, args=("pointer", "int", "int"))
HOOKS["Button::beginPress_b"] = _h(0x00488AB0, "c3dl_btn_bp_b", [("$b0", 1, 2), ("$b2", 3, 4), ("$b3", 5, 6), ("$b7", 7, 8),
                                                                  ("$b9", 9, 10), ("$b11", 11, 12)], _BTN, args=("pointer", "int", "int"))
# onMouseMove (this, x, y): scenario i makes button _MMB[i] the active one; points inside, outside, on the edge and far away
# (the rect is moved to the origin: 0, 0, right - left, bottom - top)
_MMB = ["$b2", "$b3", "$b0", "$b11", "$b6", "$b9", "$b8", "$b5", "$b1", "$b7"]
for _i, _b in enumerate(_MMB):
    HOOKS[f"Button::onMouseMove_s{_i}"] = _h(
        0x00488CF0, f"c3dl_btn_mm_{_i}",
        [(_b, 25, 15), (_b, -1, -1), (_b, 0, 0), (_b, 5, 100), (_b, 1000, 1000), (_b, 1, 0), ("$b1", 25, 15), ("$b4", 1, 1)],
        _BTN, args=("pointer", "int", "int"))
