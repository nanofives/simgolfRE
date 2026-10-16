# C3 batch c3fz (round 37, 2026-10-16) of golf_clean.exe: EditBox::onKey 0x004867b0 and __ArrayUnwind 0x004a5793.
# Reimplementations in shim/src/re/c3fz.cpp, fixtures in re/frida/js/fixtures.d/c3fz.js, Purposes and per-jcc branch
# coverage in log/c3/c3fz_purpose.md, notes in log/c3/c3fz_notes.txt.
#
# EditBox::onKey keys freeze the game's first thread (wrapToLines -> wrapText writes 0x00839aa8) and run under the import
# stand-in for USER32!MessageBeep (every beep is a compared record in $implog).
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   EditBox::onKey -> EditBox::wrapToLines 0x004862b0 and EditBox::pixelToCharPos 0x004863e0 (c3ct),
#                     EditBox::charPosToPixel 0x00486360 (c3fu), Widget_value 0x00477580 (c3c); their own callees
#                     EditBox::lineIndexAt 0x00486330 (c3s) and drawMarkupRun 0x00477280 (c3o) are hooked too
#   __ArrayUnwind  -> none of its callees is hooked (the filter 0x004a8975 is called through its address in both arms);
#                     its caller ehVectorConstructorIterator 0x004a55d2 (c3fo) is re-run with this hook on and off.

# Mirror of OBJS in re/frida/js/fixtures.d/c3fz.js: the keys run on each object, in order.
_VK_BACK, _VK_END, _VK_HOME, _VK_LEFT, _VK_UP, _VK_RIGHT, _VK_DOWN, _VK_DEL = 0x08, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2e
_OBJ_KEYS = [
    [_VK_LEFT, _VK_BACK, _VK_UP, _VK_RIGHT, _VK_DOWN],
    [_VK_RIGHT, _VK_DOWN, _VK_END, _VK_DEL, _VK_LEFT],
    [_VK_UP, _VK_UP, _VK_DOWN, _VK_DOWN],
    [_VK_DOWN, _VK_UP, _VK_HOME, _VK_END],
    [_VK_BACK, _VK_BACK, _VK_DEL, _VK_DEL, _VK_RIGHT],
    [_VK_DEL, _VK_DEL, _VK_BACK, _VK_END],
    [_VK_UP, _VK_DOWN, _VK_DOWN, _VK_UP],
    [_VK_RIGHT, _VK_DOWN, _VK_UP, _VK_UP],
    [_VK_DOWN, _VK_DOWN, _VK_UP, _VK_LEFT, _VK_RIGHT],
    [_VK_DOWN, _VK_UP, _VK_RIGHT, _VK_LEFT],
    [_VK_DOWN, _VK_DOWN, _VK_UP],
    [_VK_UP, _VK_UP, _VK_DOWN, _VK_BACK],
    [_VK_DOWN, _VK_UP, _VK_RIGHT, _VK_RIGHT, _VK_DEL],
    [_VK_UP, _VK_DOWN, _VK_BACK, _VK_BACK],
    [_VK_LEFT, _VK_RIGHT, _VK_HOME, _VK_END, _VK_DEL, _VK_BACK, _VK_UP, _VK_DOWN],
    [_VK_DOWN, _VK_DOWN, _VK_DOWN, _VK_UP, _VK_UP],
    [_VK_DOWN, _VK_UP, _VK_LEFT, _VK_RIGHT],
    [_VK_UP, _VK_DOWN, _VK_DEL, _VK_BACK],
    [0x41, 0x07, 0x29, 0x2f, 0x1b, 0x0d, 0x09, 0xffff, 0x00, 0x2a, 0x21, 0x22, 0x30000,
     0x10025, 0x00ab0008, 0xffff0027, 0x7fff0023, 0x80000024],
]
_NOBJ = len(_OBJ_KEYS)
_NO_CHILD = (6, 7, 8)   # layout row 1 has no child (slot +0xd8 gives width 0)
_EDIT_STATE = ([(f"$eb{i}", 0, 0x700) for i in range(_NOBJ)]
               + [(f"$ebch{i}", 0, 0x80) for i in range(_NOBJ) if i not in _NO_CHILD]
               + [(f"$txt{i}", 0, 0x80) for i in range(_NOBJ)]
               + [("$rec", 0, 0x1000), ("$implog", 0, 0x400), (0x00839AA8, 0, 4)])
_EDIT_VECTORS = [(f"$eb{i}", k) for i, keys in enumerate(_OBJ_KEYS) for k in keys]


def _edit(fixture):
    return dict(module="golf_clean.exe", addr=0x004867B0, abi="thiscall", ret="int", args=["pointer", "uint32"],
                fixture=fixture, state=_EDIT_STATE, vectors=_EDIT_VECTORS)


_UW_STATE = [("$D", 0, 0x210), ("$arr", 0, 0x2000), ("$blk", 0x30, 4)]


def _uw(fixture, rows):
    return dict(module="golf_clean.exe", addr=0x004A5793, abi="stdcall", ret="void",
                args=["pointer", "uint32", "int", "pointer"], fixture=fixture, state=_UW_STATE,
                vectors=[("$arrend", s, n, "$dtor" if d else 0) for s, n, d in rows])


HOOKS = {
    "EditBox::onKey": _edit("c3fz_edit"),
    "EditBox::onKey_b": _edit("c3fz_edit_b"),
    # (size, count, dtor given); no destructor raises
    "__ArrayUnwind": _uw("c3fz_unwind_none", [
        (4, 0, 1), (4, 1, 1), (4, 5, 1), (8, 3, 1), (0x2C, 4, 1), (0, 3, 1), (4, -1, 1), (4, -5, 1), (12, 10, 1),
        (0x10C, 2, 1), (1, 7, 1), (0x40, 40, 1), (0xFFFFFFFC, 3, 1), (4, 0, 0), (4, -3, 0)]),
    # the first destructor call raises: nothing else runs; counts of 0 and below never call it
    "__ArrayUnwind_t0": _uw("c3fz_unwind_t0", [(4, 1, 1), (8, 3, 1), (0x2C, 2, 1), (4, 0, 1), (4, -2, 1), (4, 40, 1)]),
    # the third destructor call (index 2) raises: counts 1 and 2 finish normally
    "__ArrayUnwind_t2": _uw("c3fz_unwind_t2", [
        (4, 3, 1), (4, 2, 1), (4, 1, 1), (8, 8, 1), (0x2C, 5, 1), (0, 6, 1), (0x10C, 4, 1), (12, 2, 1), (4, 0, 1)]),
}
