# C3 batch c3de (round 25, 2026-10-10): golf_clean.exe functions that reach Win32 imports directly or through the
# Timer family. Every key's fixture installs __importStandIn (re/frida/js/importstandin.js) over the imports the
# function and its callees call; the call log `$implog` is a state region of every key, so the import calls (which, in
# which order, with which arguments) are compared between the arms. Keys that free or allocate also run under the heap
# stand-in (`$freelog`, `$arena`). Reimplementations in shim/src/re/c3de.cpp, fixtures in
# re/frida/js/fixtures.d/c3de.js, Purposes and per-jcc branch coverage in log/c3/c3de_purpose.md.
#
# Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
#   Timer::stop 0x00486ec0, Timer::reset 0x00486f10, Timer::dtor 0x00486ce0, Timer::init 0x00486d20,
#   Timer::startMmPeriodic 0x00486dc0, Timer::startMmOneShot 0x00486e40 (c3cy); operatorDelete_4a4ffc 0x004a4ffc (c3cn)
# Hooked in THIS batch and called by this batch's other functions:
#   HotList::resetFields 0x00492800 <- HotList::releaseArray, ::ctor;  HotList::releaseArray 0x00492830 <- ::dtor, ::alloc;
#   HotList::dtor 0x004928d0 <- ::scalarDtor

EXE = "golf_clean.exe"

_LOG = ("$implog", 0, 0x400)
_DISP = [("$disp", 4, 0xC), (0x0083AD4C, 0, 4)]
_HL = [("$hblk", 0, 0x80 * 12)] + _DISP + [_LOG]
_HLA = _HL + [("$freelog", 0, 0x404), (0x0083D348, 0, 4), ("$ablk", 0, 0x2000)]
_HV = [(f"$h{i}",) for i in range(12)]
_FLAGS = [1, 0, 3, 2, 1, 0, 0xFFFFFFFF, 0xFFFFFFFE, 0x101, 0x80000000, 1, 5]


def _h(addr, fixture, vectors, state, ret="int", args=("pointer",), abi="thiscall"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- capsLockOff
# Argument-free: one key per setting (keys[0x14], GetKeyboardState's and keybd_event's results).
for _v in range(12):
    HOOKS[f"capsLockOff_v{_v}"] = dict(module=EXE, addr=0x004321D0, abi="mscdecl", ret="uint32", args=[],
                                       fixture=f"c3de_caps_{_v}", state=[_LOG], vectors=[()])

# ---------------------------------------------------------------------------------------------------- showMessageBox
HOOKS["showMessageBox"] = dict(
    module=EXE, addr=0x00485590, abi="mscdecl", ret="int", args=["pointer", "pointer"], fixture="c3de_msgbox",
    state=[_LOG],
    vectors=[("$s0", "$s1"), ("$s1", "$s0"), ("$s2", "$s2"), (0, "$s3"), ("$s4", 0), (0, 0), ("$s5", "$s0"),
             ("$s0", "$s5"), ("$s3", "$s4"), ("$s4", "$s3"), ("$s1", "$s1"), ("$s5", "$s5")])

# ---------------------------------------------------------------------------------------------------- EditBox::insertChar
# (this, ch, a2). Objects $e0..$e13 (EB_ROWS in the fixture): append at the end, insert in the middle / at the start,
# empty buffer, full buffer (MessageBeep), a character below 0x20 (returns 0), negative characters, a2 that must not matter.
HOOKS["EditBox::insertChar"] = dict(
    module=EXE, addr=0x004866F0, abi="thiscall", ret="int", args=["pointer", "int", "int"], fixture="c3de_editbox",
    state=[("$eblk", 0, 0x5C0 * 14), ("$ebufs", 0, 0x40 * 14), _LOG],
    vectors=[("$e0", 0x78, 0x60), ("$e1", 0x78, 0), ("$e2", 0x41, 0x65), ("$e3", 0x7A, 0), ("$e4", 0x77, 0),
             ("$e5", 0x100, 0x69), ("$e6", 0x41, 0), ("$e7", -1, 0x30), ("$e8", 0xFF, 0), ("$e9", 0x7FFFFFFF, 0),
             ("$e10", 0x20, 0), ("$e11", 0, 0x60), ("$e12", 0x1F, 0x69), ("$e13", 0x20, 0), ("$e7", -0x80000000, 0),
             ("$e8", 0x61, 0x66), ("$e1", 5, 0), ("$e0", 0x1FF, 0), ("$e9", 0x7E, 0x61), ("$e12", 0x21, 0x68),
             ("$e13", 0x7F, 0x30), ("$e2", 0x80, 0x62)])

# ---------------------------------------------------------------------------------------------------- HotList family
HOOKS["HotList::resetFields"] = _h(0x00492800, "c3de_hl_plain", _HV, _HL)
HOOKS["HotList::resetFields_nodisp"] = _h(0x00492800, "c3de_hl_plain_nodisp", _HV, _HL)
HOOKS["HotList::ctor"] = _h(0x00492850, "c3de_hl_plain", _HV, _HL, ret="pointer")
HOOKS["HotList::ctor_nodisp"] = _h(0x00492850, "c3de_hl_plain_nodisp", _HV, _HL, ret="pointer")

HOOKS["HotList::releaseArray"] = _h(0x00492830, "c3de_hl_arrays", _HV, _HLA)
HOOKS["HotList::releaseArray_nodisp"] = _h(0x00492830, "c3de_hl_arrays_nodisp", _HV, _HLA)
HOOKS["HotList::dtor"] = _h(0x004928D0, "c3de_hl_arrays", _HV, _HLA)
HOOKS["HotList::dtor_nodisp"] = _h(0x004928D0, "c3de_hl_arrays_nodisp", _HV, _HLA)
HOOKS["HotList::scalarDtor"] = _h(0x004928B0, "c3de_hl_arrays", [(f"$h{i}", _FLAGS[i]) for i in range(12)], _HLA,
                                  ret="pointer", args=("pointer", "uint32"))

# (this, n): n = 0 (an empty array), a few sizes, -1 (the size wraps: the allocation fails)
_NV = [3, 0, 1, 10, 25, -1, 2, 0, 7, 4, 5, 1]
HOOKS["HotList::alloc"] = _h(0x00492920, "c3de_hl_alloc", [(f"$h{i}", _NV[i]) for i in range(12)],
                             _HLA + [("$arena", 0, 0x4010)], ret="pointer", args=("pointer", "int"))
HOOKS["HotList::alloc_fail"] = _h(0x00492920, "c3de_hl_alloc_fail", [(f"$h{i}", _NV[i]) for i in range(12)],
                                  _HLA + [("$arena", 0, 0x4010)], ret="pointer", args=("pointer", "int"))
HOOKS["HotList::grow"] = _h(0x00492690, "c3de_hl_grow", _HV, _HLA + [("$arena", 0, 0x8010)], ret="void")
# a failed allocation is only survivable with a count of 0 (rows 2, 3 and 8)
HOOKS["HotList::grow_fail"] = _h(0x00492690, "c3de_hl_grow_fail", [("$h2",), ("$h3",), ("$h8",)],
                                 _HLA + [("$arena", 0, 0x8010)], ret="void")

# (this, a1, a2, a3): a1 is compared with an entry's +0x18, a2 with its +0x14 (fixture: moveLists)
_MOVE = [("$h0", 1, 1, 7), ("$h0", 1, 2, 7), ("$h0", 2, 3, 8), ("$h0", 2, 1, 9), ("$h0", 5, 5, 0), ("$h0", 1, 3, 1),
         ("$h0", 2, 2, 1), ("$h0", -1, -1, -1), ("$h1", 1, 1, 7), ("$h2", 2, 3, 3), ("$h3", 2, 1, 0), ("$h4", 9, 9, 5),
         ("$h4", 9, 8, 5), ("$h5", 9, 9, 6), ("$h6", 1, 1, 0), ("$h6", 2, 2, 2), ("$h7", 2, 2, 4), ("$h8", 105, 5, 0),
         ("$h8", 106, 6, 0), ("$h8", 111, 11, 3), ("$h8", 0x7FFFFFFF, -0x80000000, 1), ("$h9", 103, 3, 1),
         ("$h10", 1, 1, 1), ("$h11", 100, 0, 5), ("$h11", 1, 1, 2)]
# the NULL-display key only starts timers with a delay below 0x32 (the SetTimer branch reads the display unchecked)
_MOVE_MM = [("$h0", 1, 1, 7), ("$h0", 1, 2, 7), ("$h0", 2, 3, 8), ("$h0", 5, 5, 0), ("$h0", 1, 3, 1), ("$h2", 2, 3, 3),
            ("$h4", 9, 9, 5), ("$h4", 9, 8, 5), ("$h6", 1, 1, 0), ("$h6", 2, 2, 2), ("$h8", 105, 5, 0),
            ("$h8", 106, 6, 0), ("$h8", 111, 11, 3), ("$h10", 1, 1, 1), ("$h11", 100, 0, 5)]
HOOKS["HotList::onMouseMove"] = _h(0x00492CC0, "c3de_hl_move", _MOVE, _HL, args=("pointer", "int", "int", "int"))
HOOKS["HotList::onMouseMove_nodisp"] = _h(0x00492CC0, "c3de_hl_move_nodisp", _MOVE_MM, _HL,
                                          args=("pointer", "int", "int", "int"))

# eax is the caller's on the skip path (no caller reads it: 0x0047bf08 jumps, 0x00495a28 is followed by `mov eax`)
HOOKS["HotList::cancelTip"] = _h(0x00492D40, "c3de_hl_cancel", _HV, _HL, ret="void")
HOOKS["HotList::cancelTip_nodisp"] = _h(0x00492D40, "c3de_hl_cancel_nodisp", _HV, _HL, ret="void")

# ---------------------------------------------------------------------------------------------------- Timer::scalarDtor
HOOKS["Timer::scalarDtor"] = dict(
    module=EXE, addr=0x00486CC0, abi="thiscall", ret="pointer", args=["pointer", "uint32"], fixture="c3de_timers",
    state=[("$tblk", 0, 0x40 * 12)] + _DISP + [_LOG, ("$freelog", 0, 0x404)],
    vectors=[(f"$t{i}", _FLAGS[i]) for i in range(12)])

# ---------------------------------------------------------------------------------------------------- startColorCycle
# (this, id): tables $c0..$c7 and Timers $tm0..$tm7 (fixture colorTables)
HOOKS["startColorCycle"] = dict(
    module=EXE, addr=0x004832E0, abi="thiscall", ret="int", args=["pointer", "int"], fixture="c3de_colors",
    state=[("$tblk", 0, 0x80 * 8), ("$timers", 0, 0x40 * 8)] + _DISP + [_LOG],
    vectors=[("$c0", 10), ("$c0", 12), ("$c0", 14), ("$c0", 99), ("$c0", 15), ("$c1", 77), ("$c1", 99), ("$c1", 21),
             ("$c2", -1), ("$c2", 33), ("$c2", 30), ("$c3", 43), ("$c3", 0), ("$c4", 52), ("$c5", 65), ("$c6", -1),
             ("$c6", 73), ("$c6", 71), ("$c7", 80), ("$c7", 85), ("$c3", 41), ("$c5", 60)])

# ---------------------------------------------------------------------------------------------------- allocColorRange
# (this, id, lo, n, interval): tables $d0..$d7 (fixture colorRange). lo < 10, n above 0xf6 - lo and id == -1 return 3; a
# record already holding the id releases its Timer (vtable slot 0, flags 1) and buffer first; a failed allocation returns 4.
_CR = [("$d0", 7, 20, 5, 100), ("$d0", 0x30, 20, 5, 100), ("$d0", 99, 12, 3, 1000), ("$d1", 4, 10, 1, 20),
       ("$d1", 1, 30, 100, 13), ("$d2", 3, 0x30, 0xC8, 50), ("$d2", 1, 0xFF, -9, 5), ("$d3", 99, 100, 10, 1000),
       ("$d3", 15, 10, 236, 13), ("$d3", 12, 10, 237, 13), ("$d4", 23, 11, 5, 7), ("$d5", 32, 12, 4, 9),
       ("$d6", 53, 0x15, 6, 10), ("$d7", 44, 0x10A, 2, 2), ("$d7", 41, 9, 2, 2), ("$d7", -1, 20, 1, 1),
       ("$d7", 45, 0x109, 1, 1), ("$d6", 56, 0x1FE, 0, 5), ("$d5", 36, 0x40, 0, 777), ("$d4", 22, 0xF6, 0, 1)]
_CRS = [("$dblk", 0, 0x100 * 8), ("$tpool", 0, 0x40 * 12), ("$arena", 0, 0x4010), ("$freelog", 0, 0x404)] + _DISP + [_LOG]
for _k, _fx in (("", "c3de_colorrange"), ("_f1", "c3de_colorrange_f1"), ("_f2", "c3de_colorrange_f2")):
    HOOKS[f"allocColorRange{_k}"] = dict(module=EXE, addr=0x00483190, abi="thiscall", ret="int",
                                         args=["pointer", "int", "int", "int", "int"], fixture=_fx, state=_CRS,
                                         vectors=_CR)
