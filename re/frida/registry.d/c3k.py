"""c3k batch: club-name text builder, window inner-size adjuster, draw-target setter, record-bank placer, tile
corner-height reader and connected-path flood of golf_clean.exe. Keys are the hooks.csv names; diff_hook/c3_verify
key off the address. Writers list their `state` regions (snapshot/compare/restore per vector). shim/src/re/c3k.cpp."""

# appendClubName writes only the text buffer at 0x0051a068.
_TEXT = [(0x0051A068, 0, 64)]
# placeRecord writes the record banks starting at 0x004e6d20 (0x74 bytes per bank; the occupancy grid can spill a
# little past a bank, so snapshot a generous 0x200 that covers banks 0..3).
_BANKS = [(0x004E6D20, 0, 0x200)]
# propagateType11 writes the connected-path grid at 0x0056988c (50x50 bytes).
_PATH = [(0x0056988C, 0, 2500)]

HOOKS = {
    # appendClubName 0x0040a9a0: strcat of the club's name; clubs 0..13 append a name, anything else appends nothing.
    "appendClubName": dict(module="golf_clean.exe", addr=0x0040A9A0, abi="default", ret="void", args=["int"],
                           fixture="c3k_text_empty", state=_TEXT,
                           vectors=[(c,) for c in range(14)] + [(14,), (15,), (-1,), (0x7FFFFFFF,)]),

    # Window::innerSize 0x0047cb10: adjusts *w/*h by the window's frame flags. One window object per flag
    # combination (c3k_windows o0..o9); $w/$h are the shared input cells and the state regions, reset per vector.
    "Window::innerSize": dict(module="golf_clean.exe", addr=0x0047CB10, abi="thiscall", ret="void",
                              args=["pointer", "pointer", "pointer"], fixture="c3k_windows",
                              state=[("$w", 0, 4), ("$h", 0, 4)],
                              vectors=[(f"$o{i}", "$w", "$h") for i in range(10)]),

    # setDrawTarget 0x004762d0: returns 3 for a null surface; otherwise stores the surface (only when surf+4 != 0)
    # and three dwords into this+0x5c..0x68. surf is 0 (NULL), $s0 (surf+4 == 0) or $s1 (surf+4 != 0).
    "setDrawTarget": dict(module="golf_clean.exe", addr=0x004762D0, abi="thiscall", ret="int",
                          args=["pointer", "pointer", "int", "int", "int"], fixture="c3k_drawtarget",
                          state=[("$obj", 0x5C, 16)],
                          vectors=[("$obj", 0, 11, 22, 33), ("$obj", 0, 1, 2, 3),
                                   ("$obj", "$s0", 100, 200, 300), ("$obj", "$s0", -1, -2, -3),
                                   ("$obj", "$s0", 0, 0, 0), ("$obj", "$s1", 100, 200, 300),
                                   ("$obj", "$s1", -1, -2, -3), ("$obj", "$s1", 7, 8, 9),
                                   ("$obj", "$s1", 0x7FFFFFFF, 0, 1), ("$obj", "$s1", 12, 34, 56),
                                   ("$obj", "$s0", 5, 6, 7), ("$obj", 0, 0, 0, 0)]),

    # placeRecord 0x00401040: places footprint `type` into record bank `bank`. bank 0 is seeded all-free (places
    # slot 0), bank 1 has slot 0 taken (places slot 1), bank 2 is full (returns 0). Types 0/1/2 have different
    # footprints so the occupancy writes differ.
    "placeRecord": dict(module="golf_clean.exe", addr=0x00401040, abi="default", ret="int",
                        args=["int", "int", "int", "int"], fixture="c3k_records", state=_BANKS,
                        vectors=[(0, 0, 0, 0), (0, 1, 0, 0), (0, 2, 0, 0), (0, 0, 1, 1), (0, 1, 1, 0),
                                 (0, 2, 0, 1), (1, 0, 0, 0), (1, 1, 0, 0), (1, 2, 1, 1),
                                 (2, 0, 0, 0), (2, 1, 0, 0), (2, 2, 0, 0)]),

    # cornerHeights 0x0040bfe0: corner height of a tile. Cells are seeded with types that take each flag branch
    # (type 1 -> heightBlend, 2 -> cornerRange min, 3 -> cornerRange max, 4 -> 3, 5/6 -> the 0x543018 byte); the
    # cache at 0x0051b770 is seeded non-zero at (5,5) corner 1 for the flag!=0 hit.
    "cornerHeights": dict(module="golf_clean.exe", addr=0x0040BFE0, abi="default", ret="int",
                          args=["int", "int", "int", "int"], fixture="c3k_corners",
                          vectors=[(5, 5, 1, 0), (5, 5, 3, 0), (5, 5, 5, 0), (5, 5, 7, 0),
                                   (6, 6, 1, 0), (7, 7, 1, 0), (8, 8, 1, 0), (9, 9, 1, 0), (10, 10, 1, 0),
                                   (5, 5, 1, 1), (5, 5, 3, 1), (11, 11, 7, 0),
                                   (-1, 5, 1, 0), (5, 5, 2, 0), (49, 49, 5, 0)]),

    # propagateType11 0x0042f1c0: recomputes the path byte at (x, y) and recurses into type-0x11 neighbours. A 5x5
    # block of type-0x11 tiles (x,y in 20..24) sits in a type-2 grid; only cell (22,22) starts non-zero, so the
    # center vector changes the grid and the others leave it unchanged.
    "propagateType11": dict(module="golf_clean.exe", addr=0x0042F1C0, abi="default", ret="void",
                            args=["int", "int"], fixture="c3k_pathgrid", state=_PATH,
                            vectors=[(22, 22), (21, 21), (20, 20), (23, 23), (22, 21), (24, 24), (0, 0)]),
}
