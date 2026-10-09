# C3 batch c3cr (round 22, 2026-10-09) of golf_clean.exe: UI rows below 0x491b70. Reimplementations in
# shim/src/re/c3cr.cpp, fixtures in re/frida/js/fixtures.d/c3cr.js, Purposes and per-jcc branch coverage in
# log/c3/c3cr_purpose.md.
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   every ListBox *_vtordisp thunk -> its body in this batch (0x0048cac0, 0x0048c4f0, 0x0048a9c0, 0x0048c520,
#                                     0x0048c560, 0x0048c610, 0x0048cbf0, 0x0048cc30, 0x0048caf0)
#   ListBox::scrollBy         -> Window::fwdScroll26c 0x0047b9f0 (C3, c3bx; the fixture leaves +0x26c null)
#   ListBox::fireSelectChanged -> ListModel::selectedId 0x00489950 (C3, c3u)
#   Container::cycleSelection -> Window::visible 0x004801f0 (C3, c3c)
#   EditBox::cycleSelectionIfOwned -> Container::cycleSelection 0x0047af60 (this batch)
#   Button::timerCallback     -> Button::onTimer 0x00488fd0 (C2, hooked by no batch)
# Every other call is a fixture recording stub on our own vtables.

# ---- ListBox ---------------------------------------------------------------------------------------------------
# Eight synthetic ListBoxes ($lb0..$lb7, their Window bases $w0..$w7 for the thunks). The whole 0x800-byte block of
# each, the six parents, the four children, the recorder and the "marked" flag 0x0083b654 are compared.
_LB_PARENTS = [0, 1, 2, 4, 6]
_LB_CHILDREN = [0, 2, 4, 6]
_LB_STATE = ([(f"$blk{i}", 0, 0x800) for i in range(8)]
             + [(f"$par{i}", 0, 0x200) for i in _LB_PARENTS]
             + [(f"$ch{i}", 0, 0x40) for i in _LB_CHILDREN]
             + [("$rec", 0, 0x400), (0x0083B654, 0, 4)])
# fireSelectChanged also writes 0x0083ab2c (the game's thread is frozen for those keys).
_LB_STATE_FROZEN = _LB_STATE + [(0x0083AB2C, 0, 4)]


def _two(base):
    # 8 objects with two argument pairs each = 16 vectors
    return ([(f"${base}{i}", 0x10 + i, -i - 1) for i in range(8)]
            + [(f"${base}{i}", 0x7FFFFFFF - i, 0x1000 * i) for i in range(8)])


def _scroll(base):
    # dir > 0, dir == 0 and dir < 0 on every object; the two ignored arguments vary too
    return ([(f"${base}{i}", 1, 0, 0) for i in range(8)]
            + [(f"${base}{i}", 0, 5, 6) for i in range(8)]
            + [(f"${base}{i}", -3, i, -i) for i in range(8)]
            + [(f"${base}{i}", 0x7FFFFFFF, 1, 1) for i in (0, 2, 4, 6)])


def _one(base):
    return [(f"${base}{i}", a) for i in range(8) for a in (0, 0x55 + i)]


def _zero(base):
    return [(f"${base}{i}",) for i in range(8)]


def _lb(addr, nargs, fixture, state, vectors):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret="int",
                args=["pointer"] + ["int"] * nargs, fixture=fixture, state=state, vectors=vectors)


# ---- Container / items -------------------------------------------------------------------------------------------
_CT_STATE = ([(f"$ct{c}", 0, 0x200) for c in range(8)]
             + [(f"$it{c}_{k}", 0, 0x180) for c in range(8) for k in range(4)]
             + [(f"$nd{c}_{k}", 0, 0x20) for c in range(8) for k in range(4)]
             + [("$orph0", 0, 0x180), ("$orph1", 0, 0x180), ("$orph2", 0, 0x180), ("$nullhead", 0, 0x200),
                ("$rec", 0, 0x400), (0x0083AB98, 0, 4)])
# Container::cycleSelection: one item of every container (and a second for the ones whose walk moves), plus the
# orphan with no parent. Never $orph2: its parent has a null head and the original would fault (0x0047b003).
_CYCLE_VECTORS = ([(f"$it{c}_0",) for c in range(8)]
                  + [(f"$it{c}_2",) for c in (0, 1, 2, 3, 5, 6)]
                  + [("$orph0",), ("$orph1",)])
# The callback variants: the containers whose call reaches the lose-focus notification.
_CYCLE_CB_VECTORS = [(f"$it{c}_0",) for c in (0, 1, 3, 5, 6)] + [(f"$it{c}_2",) for c in (0, 1, 3, 6)]
_FOCUS_VECTORS = ([(f"$it{c}_{k}",) for c in range(8) for k in range(4)]
                  + [("$orph0",), ("$orph1",), ("$orph2",)])
_IFOWNED_VECTORS = ([(f"$it{c}_{k}",) for c in range(8) for k in (0, 3)]
                    + [("$orph0",), ("$orph1",), ("$orph2",)])

_BTN_STATE = [(f"$btn{i}", 0, 0x80) for i in range(4)] + [("$rec", 0, 0x400)]
_BTN_VECTORS = [(0, 0), (1, 0), (2, 0), (-1, 0),
                (2, "$btn0"), (3, "$btn1"), (4, "$btn2"), (5, "$btn3"), (-1, "$btn0"), (0x7FFFFFFF, "$btn1"),
                (-0x80000000, "$btn2"), (0x100, "$btn3"), (2, "$btn3")]

HOOKS = {
    # ListBox bodies on $lb<i>. Leaves (no hooks.csv callee) have >= 10 vectors each.
    "ListBox::clearSelection": _lb(0x0048CAC0, 2, "c3cr_listbox", _LB_STATE, _two("lb")),
    "ListBox::fwdOnScroll": _lb(0x0048C4F0, 2, "c3cr_listbox", _LB_STATE, _two("lb")),
    "ListBox::onNavKeyMarked": _lb(0x0048A9C0, 2, "c3cr_listbox", _LB_STATE, _two("lb")),
    "ListBox::fireSelectChangedMarked": _lb(0x0048C520, 2, "c3cr_listbox", _LB_STATE, _two("lb")),
    "ListBox::fireSelectChanged": _lb(0x0048C560, 2, "c3cr_listbox_frozen", _LB_STATE_FROZEN, _two("lb")),
    "ListBox::forwardClick": _lb(0x0048C610, 2, "c3cr_listbox", _LB_STATE, _two("lb")),
    "ListBox::fwdInnerVfn20": _lb(0x0048CBF0, 0, "c3cr_listbox", _LB_STATE,
                                  _zero("lb") + [("$lb0",), ("$lb4",), ("$lb6",)]),
    "ListBox::fwdInnerVfn24": _lb(0x0048CC30, 1, "c3cr_listbox", _LB_STATE, _one("lb")),
    "ListBox::scrollBy": _lb(0x0048CAF0, 3, "c3cr_listbox", _LB_STATE, _scroll("lb")),

    # The vtordisp thunks, entered with the Window base $w<i> (whose [W-4] is W - this).
    "ListBox::clearSelection_vtordisp": _lb(0x0048CC90, 2, "c3cr_listbox", _LB_STATE, _two("w")),
    "ListBox::fwdOnScroll_vtordisp": _lb(0x0048CD00, 2, "c3cr_listbox", _LB_STATE, _two("w")),
    "ListBox::onNavKeyMarked_vtordisp": _lb(0x0048CCC0, 2, "c3cr_listbox", _LB_STATE, _two("w")),
    "ListBox::fireSelectChangedMarked_vtordisp": _lb(0x0048CCF0, 2, "c3cr_listbox", _LB_STATE, _two("w")),
    "ListBox::fireSelectChanged_vtordisp": _lb(0x0048CCE0, 2, "c3cr_listbox_frozen", _LB_STATE_FROZEN, _two("w")),
    "ListBox::forwardClick_vtordisp": _lb(0x0048CCA0, 2, "c3cr_listbox", _LB_STATE, _two("w")),
    "ListBox::fwdInnerVfn20_vtordisp": _lb(0x0048CBE0, 0, "c3cr_listbox", _LB_STATE,
                                           _zero("w") + [("$w0",), ("$w4",), ("$w6",)]),
    "ListBox::fwdInnerVfn24_vtordisp": _lb(0x0048CC20, 1, "c3cr_listbox", _LB_STATE, _one("w")),
    "ListBox::scrollBy_vtordisp": _lb(0x0048CC70, 3, "c3cr_listbox", _LB_STATE, _scroll("w")),

    # Window::isFocusChild: every item of every container plus the three orphans (35 vectors, leaf).
    "Window::isFocusChild": dict(module="golf_clean.exe", addr=0x0047D7D0, abi="thiscall", ret="int",
                                 args=["pointer"], fixture="c3cr_focus", state=_CT_STATE, vectors=_FOCUS_VECTORS),

    # Container::cycleSelection: plain stubs, then the two variants whose lose-focus stub clears the container's
    # list head (_cb_head: 0x0047b05d taken) or the cursor node's item (_cb_item: 0x0047b067 taken).
    "Container::cycleSelection": dict(module="golf_clean.exe", addr=0x0047AF60, abi="thiscall", ret="int",
                                      args=["pointer"], fixture="c3cr_focus", state=_CT_STATE,
                                      vectors=_CYCLE_VECTORS),
    "Container::cycleSelection_cb_head": dict(module="golf_clean.exe", addr=0x0047AF60, abi="thiscall", ret="int",
                                              args=["pointer"], fixture="c3cr_focus_cb_head", state=_CT_STATE,
                                              vectors=_CYCLE_CB_VECTORS),
    "Container::cycleSelection_cb_item": dict(module="golf_clean.exe", addr=0x0047AF60, abi="thiscall", ret="int",
                                              args=["pointer"], fixture="c3cr_focus_cb_item", state=_CT_STATE,
                                              vectors=_CYCLE_CB_VECTORS),

    # EditBox::cycleSelectionIfOwned: owned items (tail jump) and orphans (eax 0). Never an item whose parent has
    # a null head, for the reason above ($orph2 is an orphan only for isFocusChild; here its parent $nullhead has
    # count 4 and a null head, so it is left out).
    "EditBox::cycleSelectionIfOwned": dict(module="golf_clean.exe", addr=0x00486B20, abi="thiscall", ret="int",
                                           args=["pointer"], fixture="c3cr_focus", state=_CT_STATE,
                                           vectors=[v for v in _IFOWNED_VECTORS if v != ("$orph2",)]),

    # Button::timerCallback(which, button), cdecl. A null button for which 0, 1, 2, -1; otherwise any which but
    # 0 and 1 (those reach Timer::stop / Timer::initStartOnce, Win32 timers).
    "Button::timerCallback": dict(module="golf_clean.exe", addr=0x00488FB0, abi="default", ret="void",
                                  args=["int", "pointer"], fixture="c3cr_buttons", state=_BTN_STATE,
                                  vectors=_BTN_VECTORS),
}
