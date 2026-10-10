# C3 batch c3cu (round 23, 2026-10-09) of golf_clean.exe: the composite-view layer from 0x004a0c90 up. Reimplementations
# in shim/src/re/c3cu.cpp, fixtures in re/frida/js/fixtures.d/c3cu.js, Purposes and per-jcc branch coverage in
# log/c3/c3cu_purpose.md.
#
# Hooked callees from other batches (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses
# original callees; on the c3cu instance they are unhooked, since only c3cu.cpp is linked there):
#   hotHitCb0..6                -> HotList::hitTest 0x00492a90 (C3, c3d) -> pointInRect 0x00492610 (C3, golf_tables)
#   viewDispatchSlot29          -> ListBox::forwardClick 0x0048c610 (C3, c3cr)
#   viewDispatchSelectChanged   -> ListBox::fireSelectChanged 0x0048c560 (C3, c3cr) -> ListModel::selectedId
#                                  0x00489950 (C3, c3u); listSelectionChanged 0x0049e640 -> selectedId
#   viewDispatchCycleSelection, panelRowCycleSelection -> Container::cycleSelection 0x0047af60 (C3, c3cr)
#   viewDispatchScrollBy        -> ListBox::scrollBy 0x0048caf0 (C3, c3cr) -> Window::fwdScroll26c 0x0047b9f0 (C3, c3bx)
# Hooked callees inside this batch: every *_vtordisp -> its body; viewDispatchSlot31/32/33/34/39 -> hotHitCb1/2/3/4/6;
#   viewDispatchSelectChanged -> hotHitCb5; viewDispatchSlot60 -> widgetShow; viewDispatchCycleSelection ->
#   panelRowCycleSelection.

# The call recorder (every virtual slot and callback the fixture installs records its target and arguments there).
_STATE = [("$rec", 0, 0x400)]
# Dispatchers: plus every ListBox span L-0x34 .. end of model and every listPanel model (the fields scrollBy and
# ListModel::selectedId write).
_STATE_D = _STATE + [(f"$lbz_{v}", 0, 0x6B0) for v in range(7)] + [(f"$lpz_{v}", 0, 0x100) for v in range(4)]
# viewDispatchSelectChanged's tags 1, 2 and 0x10 write 0x0083ab2c (the game's first thread is frozen for that key).
_STATE_FROZEN = _STATE_D + [(0x0083AB2C, 0, 4)]

# Points against the six HotList rects (c3cu.js): entry 0 (5,5), 1 (30,30), 2 only (55,55), 2 and 4 overlapping
# (70,70) -> 4, 4 only (95,95), 3 (110,10), 5 (205,205); misses: (300,300), (-1,-1), the half-open corner (10,10),
# (0x7fffffff, 0).
_PTS = [(5, 5), (30, 30), (55, 55), (70, 70), (95, 95), (110, 10), (205, 205), (300, 300), (-1, -1), (10, 10),
        (0x7FFFFFFF, 0)]
_PTS_SHORT = [(5, 5), (70, 70), (110, 10), (205, 205), (300, 300)]

_P8 = range(5)       # panelSub variants
_R4 = range(6)       # panelRow variants
_D2 = range(7)       # ListBox variants
_D16 = range(4)      # listPanel variants
_D1 = range(3)       # optionDialog variants
_DX = range(8)       # tags no case handles

_ALL_D = ([f"$d8_{v}" for v in _P8] + [f"$d4_{v}" for v in _R4] + [f"$d2_{v}" for v in _D2]
          + [f"$d16_{v}" for v in _D16] + [f"$d1_{v}" for v in _D1] + [f"$dx_{v}" for v in _DX])
_NON8_D = [d for d in _ALL_D if not d.startswith("$d8_")]


def _e(addr, nargs, vectors, ret="int", fixture="c3cu_view", state=_STATE):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret=ret, args=["pointer"] + ["int"] * nargs,
                fixture=fixture, state=state, vectors=vectors)


def _d(addr, nargs, vectors, ret="int"):
    return _e(addr, nargs, vectors, ret=ret, state=_STATE_D)


def _hot(prefix):
    # every point on every panelSub variant (55 vectors)
    return [(f"${prefix}_{v}", x, y) for v in _P8 for (x, y) in _PTS]


def _tag8_dispatch():
    # the tag-8 objects with five points each, then every other object with one pair (its tag only reads [D-8])
    return ([(f"$d8_{v}", x, y) for v in _P8 for (x, y) in _PTS_SHORT]
            + [(d, 5, 5) for d in _NON8_D])


_HOT = [(0x004A43F0, 0x004A49F0), (0x004A4430, 0x004A4A00), (0x004A4470, 0x004A4A10), (0x004A44B0, 0x004A4A20),
        (0x004A44F0, 0x004A4A30), (0x004A4530, 0x004A4A50), (0x004A4570, 0x004A4A60)]

HOOKS = {}
for _k, (_body, _thunk) in enumerate(_HOT):
    HOOKS[f"hotHitCb{_k}"] = _e(_body, 2, _hot("p8"))
    HOOKS[f"hotHitCb{_k}_vtordisp"] = _e(_thunk, 2, _hot("t8"))

HOOKS.update({
    # panelSubSlot60_vtordisp: thunk -> 0x004a4870 -> slot +0x120 of the base at [P-0xe0]+4 (records); the argument
    # is not read by the body, so three values per variant.
    "panelSubSlot60_vtordisp": _e(0x004A4A70, 1, [(f"$t8_{v}", a) for v in _P8 for a in (0, 1, -5)]),

    # widgetShow / its thunk: on = 0 (all ten children), 1, 5, -1 (first child only) on every panelRow variant.
    "widgetShow": _e(0x004A2FA0, 1, [(f"$r4_{v}", a) for v in _R4 for a in (0, 1, 5, -1)]),
    "widgetShow_vtordisp": _e(0x004A30A0, 1, [(f"$t4_{v}", a) for v in _R4 for a in (0, 1, 5, -1)]),
    # panelRowCycleSelection / its thunk: no arguments; the six variants take the slot +0x128 path or each fallback.
    "panelRowCycleSelection": _e(0x004A2FF0, 0, [(f"$r4_{v}",) for v in _R4]),
    "panelRowCycleSelection_vtordisp": _e(0x004A30B0, 0, [(f"$t4_{v}",) for v in _R4]),

    # The dispatchers, on every object of the fixture.
    "viewDispatchSelectChanged": _e(0x004A0C90, 2,
                                    [(f"$d8_{v}", x, y) for v in _P8 for (x, y) in _PTS_SHORT]
                                    + [(d, a, b) for d in _NON8_D for (a, b) in ((3, 4), (-1, 0x7FFFFFFF))],
                                    fixture="c3cu_view_frozen", state=_STATE_FROZEN),
    # tag 8 only on the count-0 panelSub ($d8_2): panelOnPick's hit path draws (panelDraw4a3f10 0x004a3f10).
    "viewDispatchSlot29": _d(0x004A0DB0, 2,
                             [("$d8_2", x, y) for (x, y) in _PTS_SHORT]
                             + [(d, a, b) for d in _NON8_D for (a, b) in ((3, 4), (-1, 0x7FFFFFFF))]),
    "viewDispatchSlot31": _d(0x004A0E60, 2, _tag8_dispatch()),
    "viewDispatchSlot32": _d(0x004A0EB0, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSlot33": _d(0x004A0E20, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSlot34": _d(0x004A0E90, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSlot39": _d(0x004A0E40, 2, _tag8_dispatch(), ret="void"),
    "viewDispatchSlot60": _d(0x004A0FF0, 1, [(d, a) for d in _ALL_D for a in (0, 1, 7)]),
    "viewDispatchCycleSelection": _d(0x004A1090, 0, [(d,) for d in _ALL_D]),
    "viewDispatchSlot6": _d(0x004A1130, 0, [(d,) for d in _ALL_D]),
    # scrollBy: dir > 0, dir == 0, dir < 0 and INT_MAX on every ListBox; one vector on every other object.
    "viewDispatchScrollBy": _d(0x004A13C0, 3,
                               [(f"$d2_{v}", a, b, c) for v in _D2
                                for (a, b, c) in ((1, 0, 0), (0, 5, 6), (-3, 1, -1), (0x7FFFFFFF, 1, 1))]
                               + [(d, 1, 2, 3) for d in _NON8_D if not d.startswith("$d2_")]
                               + [(f"$d8_{v}", 1, 2, 3) for v in _P8]),
})

# gSurfaceDtor_*: three Surface aggregates each (handles on every slot, odd slots, none), with the recording display
# swapped into 0x0083ad50 (fixture c3cu_surfdtor) and with 0x0083ad50 = 0 (c3cu_surfdtor_nodisp); the game's first
# thread is frozen for both. ret void: eax is whatever the last Surface_free / iterator left.
_SD_STATE = [("$rec", 0, 0x400)] + [(f"$sd{o}", 0, 0xA00) for o in range(3)]
for _a, _n in [(0x0044AE00, "gSurfaceDtor_44ae00"), (0x0044AFB0, "gSurfaceDtor_44afb0"), (0x0044B1B0, "gSurfaceDtor_44b1b0"),
               (0x0044B3E0, "gSurfaceDtor_44b3e0"), (0x0044B4F0, "gSurfaceDtor_44b4f0"), (0x0044B690, "gSurfaceDtor_44b690"),
               (0x0044B790, "gSurfaceDtor_44b790")]:
    HOOKS[_n] = _e(_a, 0, [("$sd0",), ("$sd1",), ("$sd2",)], ret="void", fixture="c3cu_surfdtor", state=_SD_STATE)
    HOOKS[_n + "_nodisp"] = _e(_a, 0, [("$sd0",), ("$sd1",), ("$sd2",)], ret="void", fixture="c3cu_surfdtor_nodisp",
                               state=_SD_STATE)
