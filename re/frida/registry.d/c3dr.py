# C3 batch c3dr (round 27, 2026-10-11) of golf_clean.exe: non-drawing rows. Reimplementations in shim/src/re/c3dr.cpp,
# fixtures in re/frida/js/fixtures.d/c3dr.js, Purposes and per-jcc branch coverage in log/c3/c3dr_purpose.md.
#
# Hooked callees from other batches (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses
# original callees; on the c3dr instance only c3dr.cpp and c3cu.cpp (fixtures of the thunk keys) are linked):
#   static stubs  -> Image_ctor 0x00474ae0, EditBox::ctor 0x00486070, ctorGlobalObj49c020 0x0049c020, C4837f0::ctor
#                    0x004837f0, Surface_ctor 0x00473ab0 (and what they call)
#   thunks        -> the c3cu dispatchers 0x004a1130 0x004a13c0 0x004a0db0 0x004a0e60 0x004a0eb0 0x004a0e20 0x004a0e90
#                    0x004a0e40 0x004a0c90 0x004a0ff0 0x004a1090
#   wobble        -> Random::range 0x0045c1e0, Random::next 0x0045c1a0, sign 0x00467150
#   KeyTable::ctor -> Buf_ctor 0x004747a0; Flic::ctorBase482b60 -> Surface_ctor; Flic::ctor481b50 -> Flic::ctorBase482b60
#                    (inside this batch)

M = "golf_clean.exe"

HOOKS = {}

# ---------------------------------------------------------------------------------------------------------------------
# Part 1 - static-initialisation stubs. One call per key; variants live / pat / zero (fixture c3dr_s_<addr>_<variant>).
# ---------------------------------------------------------------------------------------------------------------------
_STUBS = [  # (name, stub addr, global, size, disp?)
    ("staticInit_004043b0", 0x004043B0, 0x0051B068, 0x2B4, True),
    ("staticInit_004058f0", 0x004058F0, 0x005A7148, 0x2B4, True),
    ("staticInit_004061b0", 0x004061B0, 0x00568D10, 0x2B4, True),
    ("staticInit_004061f0", 0x004061F0, 0x00587AE8, 0x2B4, True),
    ("staticCtor_43d640", 0x0043D640, 0x005AA778, 0x2B4, True),
    ("gImageCtor_838f98", 0x0046DE40, 0x00838F98, 0x2B4, True),
    ("initGlobalImage", 0x00473A70, 0x00839348, 0x2B4, True),
    ("gImageCtor_83a500", 0x004799F0, 0x0083A500, 0x2B4, True),
    ("gImageCtor_83a7b8", 0x00479A40, 0x0083A7B8, 0x2B4, True),
    ("staticCtor_4385a0", 0x004385A0, 0x005A9F78, 0x5DC, True),
    ("initGlobalObj_49c020", 0x0049BFF0, 0x008400C8, 0x5C8, True),
    ("screenMgrCtor4837b0", 0x004837B0, 0x0083AD20, 8, False),
    ("initGfxGlobal_840710", 0x0049D160, 0x00840710, 0x28, False),
    ("initGfxGlobal_8407c0", 0x0049D2A0, 0x008407C0, 0x28, False),
    ("initGfxGlobal_840790", 0x0049D300, 0x00840790, 0x28, False),
    ("initGfxGlobal_8407f0", 0x0049D360, 0x008407F0, 0x28, False),
    ("initGfxGlobal_840830", 0x0049D490, 0x00840830, 0x28, False),
    ("initGfxGlobal_840890", 0x0049D4F0, 0x00840890, 0x28, False),
    ("initGfxGlobal_840860", 0x0049D550, 0x00840860, 0x28, False),
    ("initGfxGlobal_8408c8", 0x0049EB00, 0x008408C8, 0x28, False),
    ("initGfxGlobal_8408f8", 0x0049EB60, 0x008408F8, 0x28, False),
]
for _n, _a, _g, _sz, _d in _STUBS:
    for _v in ("live", "pat", "zero"):
        HOOKS[f"{_n}_{_v}"] = dict(
            module=M, addr=_a, abi="default", ret="pointer", args=[], fixture=f"c3dr_s_{_a:08x}_{_v}",
            state=[(_g, 0, _sz)] + ([("$disp", 0, 8), (0x0083AD4C, 0, 4)] if _d else []), vectors=[()])
for _v in ("live", "pat", "zero"):
    HOOKS[f"initPanelState_{_v}"] = dict(
        module=M, addr=0x0044AC60, abi="default", ret="pointer", args=[], fixture=f"c3dr_s_0044ac60_{_v}",
        state=[(0x00821020, 0, 8), (0x00821EC8, 0, 0x68)], vectors=[()])

# ---------------------------------------------------------------------------------------------------------------------
# Part 2 - vtordisp thunks. Input W = D + 0x40 of every c3cu view object (fixture c3dr_view; the SelectChanged key uses
# c3dr_view_frozen because its tags 1, 2 and 0x10 write 0x0083ab2c). Vector sets are c3cu's with $w for $d.
# ---------------------------------------------------------------------------------------------------------------------
_PTS = [(5, 5), (30, 30), (55, 55), (70, 70), (95, 95), (110, 10), (205, 205), (300, 300), (-1, -1), (10, 10),
        (0x7FFFFFFF, 0)]
_PTS_SHORT = [(5, 5), (70, 70), (110, 10), (205, 205), (300, 300)]
_P8, _R4, _D2, _D16, _D1, _DX = range(5), range(6), range(7), range(4), range(3), range(8)
_ALL_W = ([f"$w8_{v}" for v in _P8] + [f"$w4_{v}" for v in _R4] + [f"$w2_{v}" for v in _D2]
          + [f"$w16_{v}" for v in _D16] + [f"$w1_{v}" for v in _D1] + [f"$wx_{v}" for v in _DX])
_NON8_W = [w for w in _ALL_W if not w.startswith("$w8_")]

_STATE = [("$rec", 0, 0x400)]
_STATE_D = _STATE + [(f"$lbz_{v}", 0, 0x6B0) for v in range(7)] + [(f"$lpz_{v}", 0, 0x100) for v in range(4)]
_STATE_FROZEN = _STATE_D + [(0x0083AB2C, 0, 4)]


def _t(addr, nargs, vectors, ret="int", fixture="c3dr_view", state=_STATE_D):
    return dict(module=M, addr=addr, abi="thiscall", ret=ret, args=["pointer"] + ["int"] * nargs,
                fixture=fixture, state=state, vectors=vectors)


def _tag8_dispatch():
    return ([(f"$w8_{v}", x, y) for v in _P8 for (x, y) in _PTS_SHORT] + [(w, 5, 5) for w in _NON8_W])


HOOKS.update({
    "viewDispatchSlot6_vtordisp": _t(0x00491940, 0, [(w,) for w in _ALL_W]),
    "viewDispatchScrollBy_vtordisp": _t(
        0x00491970, 3,
        [(f"$w2_{v}", a, b, c) for v in _D2 for (a, b, c) in ((1, 0, 0), (0, 5, 6), (-3, 1, -1), (0x7FFFFFFF, 1, 1))]
        + [(w, 1, 2, 3) for w in _NON8_W if not w.startswith("$w2_")] + [(f"$w8_{v}", 1, 2, 3) for v in _P8]),
    "viewDispatchSlot29_vtordisp": _t(
        0x004919A0, 2,
        [("$w8_2", x, y) for (x, y) in _PTS_SHORT] + [(w, a, b) for w in _NON8_W for (a, b) in ((3, 4), (-1, 0x7FFFFFFF))]),
    "viewDispatchSlot31_vtordisp": _t(0x004919C0, 2, _tag8_dispatch()),
    "viewDispatchSlot32_vtordisp": _t(0x004919D0, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSlot33_vtordisp": _t(0x004919E0, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSlot34_vtordisp": _t(0x004919F0, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSelectChanged_vtordisp": _t(
        0x00491A10, 2,
        [(f"$w8_{v}", x, y) for v in _P8 for (x, y) in _PTS_SHORT]
        + [(w, a, b) for w in _NON8_W for (a, b) in ((3, 4), (-1, 0x7FFFFFFF))],
        fixture="c3dr_view_frozen", state=_STATE_FROZEN),
    "viewDispatchSlot39_vtordisp": _t(0x00491A20, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSlot60_vtordisp": _t(0x00491A50, 1, [(w, a) for w in _ALL_W for a in (0, 1, 7)]),
    "viewDispatchCycleSelection_vtordisp": _t(0x00491A60, 0, [(w,) for w in _ALL_W]),
})

# ---------------------------------------------------------------------------------------------------------------------
# Part 3 - small logic rows
# ---------------------------------------------------------------------------------------------------------------------
_NG = 12  # values per global (fixture c3dr_*_<i>)
for _i in range(_NG):
    HOOKS[f"setDirtyFlagA_v{_i}"] = dict(module=M, addr=0x0045AE30, abi="default", ret="int", args=[],
                                         fixture=f"c3dr_dirtyA_{_i}", state=[(0x00822B98, 0, 4)], vectors=[()])
    HOOKS[f"setDirtyFlagB_v{_i}"] = dict(module=M, addr=0x0045AE50, abi="default", ret="int", args=[],
                                         fixture=f"c3dr_dirtyB_{_i}", state=[(0x00822B9C, 0, 4)], vectors=[()])

# blitError: one key, twelve vectors; the argument is ignored by the function, its fixture getter sets the global
HOOKS["blitError"] = dict(module=M, addr=0x00492460, abi="default", ret="int", args=["int"], fixture="c3dr_blit",
                          state=[(0x0083C310, 0, 4)], vectors=[(f"$v{_i}",) for _i in range(_NG)])

# wobble: 19 RNG seeds (fixture c3dr_wobble_<i>), one call each. Range results r = 50 51 49 60 40 69 31 70 30 89 11 90
# 10 100 0 45 55 35 65 (see log/c3/alt/c3dr_wobble_seeds.py).
for _i in range(19):
    HOOKS[f"wobble_s{_i}"] = dict(module=M, addr=0x00405920, abi="default", ret="int", args=[],
                                  fixture=f"c3dr_wobble_{_i}", state=[(0x00822D9C, 0, 4)], vectors=[()])

# matchInputCode: ring pointer index PI (fixture c3dr_match_<PI>)
_MATCH_N = [1, 2, 3, 4, 5, 9, 10, 11, 12]
_MATCH_X = [(3, 0), (3, 1), (3, 2), (10, 9), (12, 11), (12, 0), (5, 4), (2, 1)]
for _pi in (9, 5, 0, 1):
    HOOKS[f"matchInputCode_p{_pi}"] = dict(
        module=M, addr=0x0047EEE0, abi="default", ret="int", args=["pointer", "int"], fixture=f"c3dr_match_{_pi}",
        state=[(0x004E42EC, 0, 4), (0x0083ABA4, 0, 0x10)],
        vectors=([("$m0", 0), ("$m0", -5), ("$m1", -1)] + [(f"$m{n}", n) for n in _MATCH_N]
                 + [(f"$x{n}_{j}", n) for (n, j) in _MATCH_X] + [("$m5", 3), ("$m12", 4)]))

_B12 = [(f"$b{i}",) for i in range(12)]
HOOKS["Cache482fd0_ctor"] = dict(module=M, addr=0x00482FD0, abi="thiscall", ret="pointer", args=["pointer"],
                                 fixture="c3dr_cache", state=[(f"$b{i}", 0, 0x60) for i in range(12)], vectors=_B12)
HOOKS["KeyTable_ctor"] = dict(module=M, addr=0x004882A0, abi="thiscall", ret="pointer", args=["pointer"],
                              fixture="c3dr_keytable", state=[(f"$b{i}", 0, 0x120) for i in range(12)], vectors=_B12)
HOOKS["Flic_ctorBase482b60"] = dict(module=M, addr=0x00482B60, abi="thiscall", ret="pointer", args=["pointer"],
                                    fixture="c3dr_flicbase", state=[(f"$b{i}", 0, 0x60) for i in range(12)],
                                    vectors=_B12)
HOOKS["Flic_ctor481b50"] = dict(module=M, addr=0x00481B50, abi="thiscall", ret="pointer", args=["pointer"],
                                fixture="c3dr_flic", state=[(f"$b{i}", 0, 0x90) for i in range(12)], vectors=_B12)

_DEC_V = ([(f"$a{i}", 0) for i in range(4)] + [(f"$f{i}", 1) for i in range(4)] + [("$a0", 7), ("$a1", -1)])
for _tag, _addr, _fx in (("Decoder482ae0_ctor", 0x00482AE0, "ae0"), ("Decoder482b20_ctor", 0x00482B20, "b20")):
    for _g in ("g0", "g1"):
        HOOKS[f"{_tag}_{_g}"] = dict(
            module=M, addr=_addr, abi="thiscall", ret="pointer", args=["pointer", "int"],
            fixture=f"c3dr_dec_{_fx}_{_g}", state=[(f"$b{i}", 0, 0x100) for i in range(8)] + [(0x00839650, 0, 4)],
            vectors=_DEC_V)

HOOKS["Window_releaseScrollbars"] = dict(
    module=M, addr=0x0047D510, abi="thiscall", ret="void", args=["pointer"], fixture="c3dr_scroll",
    state=[("$rec", 0, 0x400)] + [(f"$w{i}", 0x150, 0xC) for i in range(16)], vectors=[(f"$w{i}",) for i in range(16)])

_ZCOUNTS = 7
for _ci in range(_ZCOUNTS):
    HOOKS[f"zPush_c{_ci}"] = dict(
        module=M, addr=0x0047E4C0, abi="default", ret="int", args=["pointer"], fixture=f"c3dr_zpush_{_ci}",
        state=[(0x0083A2D8, 0, 0x30), (0x0083AB94, 0, 4)], vectors=[("$wf",), ("$wn",), ("$wz",), ("$wf2",)])
HOOKS["zPush_null"] = dict(
    module=M, addr=0x0047E4C0, abi="default", ret="void", args=["pointer"], fixture="c3dr_zpush_1",
    state=[(0x0083A2D8, 0, 0x30), (0x0083AB94, 0, 4)], vectors=[(0,)])

# ---------------------------------------------------------------------------------------------------------------------
# Part 4 - viewDispatchSlot7 (+ thunk) on c3cu's view objects, Reader::scalarDtor
# ---------------------------------------------------------------------------------------------------------------------
_ALL_D = ([f"$d8_{v}" for v in _P8] + [f"$d4_{v}" for v in _R4] + [f"$d2_{v}" for v in _D2]
          + [f"$d16_{v}" for v in _D16] + [f"$d1_{v}" for v in _D1] + [f"$dx_{v}" for v in _DX])
_ARGS7 = (0, 1, 7, -5)
HOOKS["viewDispatchSlot7"] = dict(
    module=M, addr=0x004A1170, abi="thiscall", ret="int", args=["pointer", "int"], fixture="c3cu_view", state=_STATE,
    vectors=[(d, a) for d in _ALL_D for a in ((0x1234,) if not d.startswith("$d2_") else _ARGS7)])
HOOKS["viewDispatchSlot7_vtordisp"] = _t(
    0x00491950, 1, [(w, a) for w in _ALL_W for a in ((0x1234,) if not w.startswith("$w2_") else _ARGS7)])

_RFL = (1, 0, 0x101, 2)
HOOKS["Reader_scalarDtor"] = dict(
    module=M, addr=0x00488460, abi="thiscall", ret="pointer", args=["pointer", "uint32"], fixture="c3dr_reader",
    state=[(f"$o{m}", 0, 0x170) for m in range(8)] + [("$freelog", 0, 0x404)],
    vectors=[(f"$o{m}", f) for m in range(8) for f in _RFL])
