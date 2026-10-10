# C3 batch c3di (round 26, 2026-10-10) of golf_clean.exe: the Window tree teardown. Reimplementations in
# shim/src/re/c3di.cpp, fixtures in re/frida/js/fixtures.d/c3di.js (every key freezes the game's first thread, points the
# window globals at a synthetic tree, stands in for malloc/free/operator delete and for ReleaseCapture/timeKillEvent/
# KillTimer), Purposes and per-jcc branch tables in log/c3/c3di_purpose.md.
#
# Hooked callees of other batches (the verifier re-runs these keys with them off): HotList::resetFields 0x00492800,
# HotList::releaseArray 0x00492830, HotList::dtor 0x004928d0 (c3de); Timer::stop 0x00486ec0, Timer::reset 0x00486f10,
# Timer::dtor 0x00486ce0 (c3cy); Node::contains 0x0047b080 (c3d); View_unlinkFromParent 0x0047ada0, ChildArray::remove
# 0x0047e680, Vec_release 0x00479bb0, Cache483070::reset 0x00483070, operatorDelete_4a4ffc 0x004a4ffc (c3cz); listRemove
# 0x0047e520 (c3db); resetWidgetTable 0x00495eb0 (c3g); Image_releaseAll 0x00474cb0 (c3dd).
# Tooltip::hide 0x00480360 (called by layoutNode and Window::detach) is hooked by batch c3dl.
# Hooked callees inside this batch: layoutNode -> layoutNode (children);
# Window::ctorInit -> layoutNode; View_dtor -> layoutNode (-> Window::detach through the real vtable 0x004ba8b4 slot +8);
# Base4805a0::dtor -> Image_dtor, View_dtor; the deleting destructors -> their destructors; the adjustors -> the deleting
# destructors.

_G = [(0x0083A2D8, 0, 0x40), (0x0083AB94, 0, 4), (0x0083AB90, 0, 4), (0x0083AA98, 0, 4), (0x0083AAC0, 0, 8),
      (0x0083AAF0, 0, 4), (0x0083AB30, 0, 8), (0x0083AB40, 0, 8), (0x0083AB54, 0, 4), (0x0083AD4C, 0, 4)]
_ST = [("$arena", 0, 0x10000), ("$heap", 0, 0x410), ("$freelog", 0, 0x404), ("$implog", 0, 0x400)] + _G

_WINS = [f"$W{i}" for i in range(26)]
_IMGS = [f"$I{i}" for i in range(6)]
_W1 = [(w,) for w in _WINS]
# (window, flags): every window once, flags cycling 0..3 (bit 0 deletes), plus W0..W3 with every other flag value
_WF = [(w, i % 4) for i, w in enumerate(_WINS)] + [(f"$W{i}", f) for i in range(4) for f in (0, 1, 2, 3, 0xFF, 0x100)
                                                   if f != i % 4]
_WF274 = [(w + "_274", f) for w, f in _WF]
_IF = [(im, f) for im in _IMGS for f in (0, 1, 2, 3)]


def _t(addr, args, vectors, fixture, ret="void", abi="thiscall"):
    return dict(module="golf_clean.exe", addr=addr, abi=abi, ret=ret, args=args, fixture=fixture, state=_ST,
                vectors=vectors)


_P = ["pointer"]
_PF = ["pointer", "int"]

HOOKS = {
    "layoutNode": _t(0x00479F30, _P, _W1, "c3di_a"),
    "layoutNode_b": _t(0x00479F30, _P, _W1, "c3di_b"),
    "layoutNode_c": _t(0x00479F30, _P, _W1, "c3di_c"),
    "layoutNode_n": _t(0x00479F30, _P, _W1, "c3di_n"),
    "Window::ctorInit": _t(0x00480610, _P, _W1, "c3di_a", ret="uint32"),
    "Window::ctorInit_b": _t(0x00480610, _P, _W1, "c3di_b", ret="uint32"),
    "Window::ctorInit_n": _t(0x00480610, _P, _W1, "c3di_n", ret="uint32"),
    "Window::detach": _t(0x0047B820, _P, _W1, "c3di_a"),
    "Window::detach_b": _t(0x0047B820, _P, _W1, "c3di_b"),
    "Window::detach_c": _t(0x0047B820, _P, _W1, "c3di_c"),
    "Window::detach_n": _t(0x0047B820, _P, _W1, "c3di_n"),
    "View_dtor": _t(0x0047A3C0, _P, _W1, "c3di_r", ret="uint32"),
    "View_dtor_b": _t(0x0047A3C0, _P, _W1, "c3di_rb", ret="uint32"),
    "View_dtor_n": _t(0x0047A3C0, _P, _W1, "c3di_rn", ret="uint32"),
    "Image_dtor": _t(0x00474C40, _P, [(im,) for im in _IMGS], "c3di_r", ret="uint32"),
    "Image_dtor_n": _t(0x00474C40, _P, [(im,) for im in _IMGS], "c3di_rn", ret="uint32"),
    "Base4805a0::dtor": _t(0x004805A0, _P, _W1, "c3di_r", ret="uint32"),
    "Base4805a0::dtor_b": _t(0x004805A0, _P, _W1, "c3di_rb", ret="uint32"),
    "Base4805a0::dtor_n": _t(0x004805A0, _P, _W1, "c3di_rn", ret="uint32"),
    "View_deleteDtor": _t(0x00479F10, _PF, _WF, "c3di_r", ret="pointer"),
    "View_deleteDtor_n": _t(0x00479F10, _PF, _WF, "c3di_rn", ret="pointer"),
    "Image_deleteDtor": _t(0x00474C20, _PF, _IF, "c3di_r", ret="pointer"),
    "Image_deleteDtor_n": _t(0x00474C20, _PF, _IF, "c3di_rn", ret="pointer"),
    "View4804a0::scalarDtor": _t(0x00480580, _PF, _WF, "c3di_r", ret="pointer"),
    "View4804a0::scalarDtor_n": _t(0x00480580, _PF, _WF, "c3di_rn", ret="pointer"),
    "deletingDtor_404380": _t(0x00404380, _PF, _WF, "c3di_r", ret="pointer"),
    "deletingDtor_404380_n": _t(0x00404380, _PF, _WF, "c3di_rn", ret="pointer"),
    "View::scalarDtor47d810": _t(0x0047D810, _PF, _WF, "c3di_r", ret="pointer"),
    "View4804a0::scalarDtor_adj274": _t(0x00481B40, _PF, _WF274, "c3di_r", ret="pointer"),
    "View4804a0::scalarDtor_adj274_n": _t(0x00481B40, _PF, _WF274, "c3di_rn", ret="pointer"),
    "deletingDtor_404380_adj274": _t(0x00404950, _PF, _WF274, "c3di_r", ret="pointer"),
    "deletingDtor_404380_adj274_n": _t(0x00404950, _PF, _WF274, "c3di_rn", ret="pointer"),
}
