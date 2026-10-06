"""c3d batch: View / Node / HotList geometry and hit-tests of golf_clean.exe. One entry per fixture variant so every
branch is exercised. Keys are filename-safe underscore forms of the hooks.csv names (View::toParent -> View_toParent);
diff_hook/re_classify key off the address, so the key is only the CSV label. shim/src/re/c3d.cpp."""

# The View transforms write *x/*y; the whole 12-pair coordinate buffer (96 bytes) is the snapshotted state region.
_VIEW_STATE = [("$coords", 0, 96)]
_VIEW_VEC = [("$obj", f"$x{i}", f"$y{i}") for i in range(12)]

# offsetRect writes the RECT; the whole 10-rect buffer (160 bytes) is the state region. A null RECT (0) exercises the
# early-return branch (11 vectors total).
_RECT_STATE = [("$rects", 0, 160)]
_RECT_VEC = [("$obj", f"$r{i}") for i in range(10)] + [("$obj", 0)]

# HotList outputs land in the 32-byte scratch buffer (aOut, bOut, outRect); it is the state region.
_HOT_STATE = [("$scratch", 0, 0x20)]
# (x, y) chosen to hit entry 0..5, the 2/4 overlap (-> 4), rect-2-only (-> 2), and two misses (-> -1); plus null-output
# vectors to exercise the if(aOut)/if(bOut) guards.
_HOT_XY = [(5, 5), (30, 30), (70, 70), (110, 10), (205, 205), (55, 55), (15, 15), (1000, 1000)]

HOOKS = {
    # View::toParent 0x0047b170 (writer of *x/*y). flat = no parent recursion; parent = recurse (bit 0x20);
    # scroll = recurse and subtract parent origin (bits 0x8020).
    "View_toParent": dict(module="golf_clean.exe", addr=0x0047B170, abi="thiscall", ret="void",
                          args=["pointer", "pointer", "pointer"], fixture="c3d_view_flat", state=_VIEW_STATE,
                          vectors=_VIEW_VEC),
    "View_toParent_parent": dict(module="golf_clean.exe", addr=0x0047B170, abi="thiscall", ret="void",
                                 args=["pointer", "pointer", "pointer"], fixture="c3d_view_parent", state=_VIEW_STATE,
                                 vectors=_VIEW_VEC),
    "View_toParent_scroll": dict(module="golf_clean.exe", addr=0x0047B170, abi="thiscall", ret="void",
                                 args=["pointer", "pointer", "pointer"], fixture="c3d_view_scroll", state=_VIEW_STATE,
                                 vectors=_VIEW_VEC),
    # View::fromParent 0x0047b200 (mirror of toParent).
    "View_fromParent": dict(module="golf_clean.exe", addr=0x0047B200, abi="thiscall", ret="void",
                            args=["pointer", "pointer", "pointer"], fixture="c3d_view_flat", state=_VIEW_STATE,
                            vectors=_VIEW_VEC),
    "View_fromParent_parent": dict(module="golf_clean.exe", addr=0x0047B200, abi="thiscall", ret="void",
                                   args=["pointer", "pointer", "pointer"], fixture="c3d_view_parent", state=_VIEW_STATE,
                                   vectors=_VIEW_VEC),
    "View_fromParent_scroll": dict(module="golf_clean.exe", addr=0x0047B200, abi="thiscall", ret="void",
                                   args=["pointer", "pointer", "pointer"], fixture="c3d_view_scroll", state=_VIEW_STATE,
                                   vectors=_VIEW_VEC),
    # View::offsetRectToParent 0x0047b0d0 (writes the RECT via toParent). flat and scrolling-parent view.
    "View_offsetRectToParent": dict(module="golf_clean.exe", addr=0x0047B0D0, abi="thiscall", ret="void",
                                    args=["pointer", "pointer"], fixture="c3d_rect_flat", state=_RECT_STATE,
                                    vectors=_RECT_VEC),
    "View_offsetRectToParent_parent": dict(module="golf_clean.exe", addr=0x0047B0D0, abi="thiscall", ret="void",
                                           args=["pointer", "pointer"], fixture="c3d_rect_parent", state=_RECT_STATE,
                                           vectors=_RECT_VEC),
    # View::offsetRectToLocal 0x0047b120 (writes the RECT via toLocal 0x0047b290).
    "View_offsetRectToLocal": dict(module="golf_clean.exe", addr=0x0047B120, abi="thiscall", ret="void",
                                   args=["pointer", "pointer"], fixture="c3d_rect_flat", state=_RECT_STATE,
                                   vectors=_RECT_VEC),
    "View_offsetRectToLocal_parent": dict(module="golf_clean.exe", addr=0x0047B120, abi="thiscall", ret="void",
                                          args=["pointer", "pointer"], fixture="c3d_rect_parent", state=_RECT_STATE,
                                          vectors=_RECT_VEC),
    # Node::contains 0x0047b080 (pure predicate, returns 0/1). Direct children, recursion (D via A, F via C), the node
    # itself, an unrelated node Z, a null n, and queries rooted at A/B/C.
    "Node_contains": dict(module="golf_clean.exe", addr=0x0047B080, abi="thiscall", ret="int",
                          args=["pointer", "pointer"], fixture="c3d_node_tree",
                          vectors=[("$root", "$A"), ("$root", "$B"), ("$root", "$C"), ("$root", "$D"), ("$root", "$E"),
                                   ("$root", "$F"), ("$root", "$Z"), ("$root", "$root"), ("$root", 0), ("$A", "$D"),
                                   ("$A", "$Z"), ("$B", "$D"), ("$C", "$F"), ("$C", "$A")]),
    # nearestMenuSpot 0x004326a0 (reads the global spot table; returns the winning index, 6 -> -1). Queries equal to
    # each spot's (sy, sx) win that index; far queries return -1.
    "nearestMenuSpot": dict(module="golf_clean.exe", addr=0x004326A0, abi="default", ret="int", args=["int", "int"],
                            fixture="c3d_spots",
                            vectors=[(20, 10), (60, 30), (10, -40), (100, 100), (80, 5), (-10, 200), (70, 70), (-5, -5),
                                     (5000, 5000), (-5000, 0), (0, 0), (1000, -1000)]),
    # HotList::hitTest 0x00492a90 (returns the hit index, writes aOut/bOut). inRect 0x00492610 is hooked (C3).
    "HotList_hitTest": dict(module="golf_clean.exe", addr=0x00492A90, abi="thiscall", ret="int",
                            args=["pointer", "int", "int", "pointer", "pointer"], fixture="c3d_hotlist",
                            state=_HOT_STATE,
                            vectors=[("$obj", x, y, "$aOut", "$bOut") for x, y in _HOT_XY]
                                    + [("$obj", 5, 5, 0, 0), ("$obj", 30, 30, "$aOut", 0)]),
    # HotList::hitTestRect 0x00492b10 (as hitTest, also copies the hit entry's rect to outRect).
    "HotList_hitTestRect": dict(module="golf_clean.exe", addr=0x00492B10, abi="thiscall", ret="int",
                                args=["pointer", "int", "int", "pointer", "pointer", "pointer"], fixture="c3d_hotlist",
                                state=_HOT_STATE,
                                vectors=[("$obj", x, y, "$aOut", "$bOut", "$outRect") for x, y in _HOT_XY]
                                        + [("$obj", 70, 70, 0, 0, "$outRect"), ("$obj", 15, 15, "$aOut", "$bOut", "$outRect")]),
}
