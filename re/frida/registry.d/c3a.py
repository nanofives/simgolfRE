# C3 batch c3a (2026-10-06): terrain/golfer/sim/course readers and writers, reimplemented in
# shim/src/re/c3a.cpp. Format documented at the top of re/frida/hooks_registry.py. Keys are the
# hooks.csv names. Writers declare their `state` regions so the A/B snapshots, compares and restores them.

# Tile coordinate grids (0..49) and out-of-range probes for the table readers.
_CELLS = [(x, y) for x in (0, 1, 2, 7, 10, 24, 25, 33, 47, 48, 49)
          for y in (0, 1, 2, 13, 19, 26, 37, 47, 48, 49)]
# Interior cells only, so the orthogonal neighbours of the writers stay on the 50x50 map.
_INNER = [(x, y) for x in (1, 2, 7, 10, 24, 25, 33, 47, 48) for y in (1, 2, 13, 26, 37, 47, 48)]

# Pixel positions (>> 10 gives the tile) for typeAtPos, plus a few off-map probes.
_PIX = [(x * 1024 + 300, y * 1024 + 700) for x in (0, 1, 2, 10, 24, 25, 48, 49) for y in (0, 1, 13, 26, 48, 49)]
_PIX += [(-1, 0), (-1024, 2048), (0x7FFFFFFF, 0), (50 * 1024, 10 * 1024), (10 * 1024, 50 * 1024)]

# World coordinates for sampleHeight (each axis is >> 2 then fed to bilinearSample).
_WORLD = [(x, y) for x in (-64, -16, 0, 4, 16, 32, 64, 128, 256, 512, 1024, 2048)
          for y in (-16, 0, 8, 24, 48, 96, 200, 400, 900)]

# cornerHeights queries: (a, b) tile coords and a corner index; odd c exercises the table read, even c returns 0.
_CORNER = [(a, b, c) for a in (-1, 0, 1, 2, 24, 25, 48, 49, 50) for b in (0, 1, 13, 48, 49) for c in (0, 1, 2, 3, 7)]

HOOKS = {}
HOOKS.update({
    # --- pure readers (leaf w.r.t. game state; callees run their originals) ---
    "tileByte": dict(module="golf_clean.exe", addr=0x004492F0, abi="default", ret="int", args=["int", "int"],
                     fixture="c3a_tiles", vectors=_CELLS),
    "tileFlag20": dict(module="golf_clean.exe", addr=0x004493B0, abi="default", ret="int", args=["int", "int"],
                       fixture="c3a_tiles", vectors=_CELLS),
    "typeAtPos": dict(module="golf_clean.exe", addr=0x0040BFA0, abi="default", ret="int", args=["int", "int"],
                      fixture="c3a_tiles", vectors=_PIX),
    "thoughtFlag": dict(module="golf_clean.exe", addr=0x004675D0, abi="default", ret="int", args=["int"],
                        fixture="c3a_golfer_flags", vectors=[(g,) for g in range(32)]),
    "thoughtBalance": dict(module="golf_clean.exe", addr=0x0045C420, abi="default", ret="int", args=["int"],
                           fixture="c3a_golfer_flags", vectors=[(g,) for g in range(32)]),
    "sampleHeight": dict(module="golf_clean.exe", addr=0x0042DBA0, abi="default", ret="int", args=["int", "int"],
                         fixture="c3a_terrain_read", vectors=_WORLD),
    "tileQuery449310": dict(module="golf_clean.exe", addr=0x00449310, abi="default", ret="int",
                            args=["int", "int", "int"], fixture="c3a_terrain_read", vectors=_CORNER),

    # --- cornerRange writes two out-params; the fixture supplies the pointers and they are the state regions ---
    "cornerRange": dict(module="golf_clean.exe", addr=0x0042F4B0, abi="default", ret="void",
                        args=["int", "int", "pointer", "pointer"], fixture="c3a_cornerrange",
                        state=[("$mx", 0, 4), ("$mn", 0, 4)],
                        vectors=[(x, y, "$mx", "$mn") for x, y in _CELLS]),

    # --- writers (state = the regions each one mutates) ---
    "freeAtTile": dict(module="golf_clean.exe", addr=0x00402930, abi="default", ret="void", args=["int", "int"],
                       fixture="c3a_records", state=[(0x005736B0, 0, 256 * 0x24)],
                       vectors=[(tx, ty) for tx in (0, 1, 2, 7, 24, 48, 49) for ty in (0, 3, 13, 25, 48, 49)]),
    "raiseFromNeighbours": dict(module="golf_clean.exe", addr=0x0042F6E0, abi="default", ret="int",
                                args=["int", "int", "int"], fixture="c3a_terrain_read",
                                state=[(0x00543018, 0, 2500)],
                                vectors=[(x, y, s) for x, y in _INNER for s in (0, 1)]),
    "relaxEdges42f530": dict(module="golf_clean.exe", addr=0x0042F530, abi="default", ret="void",
                             args=["int", "int"], fixture="c3a_terrain_read",
                             state=[(0x005619A0, 0, 2500)], vectors=_INNER),
    "nearestPlaced": dict(module="golf_clean.exe", addr=0x0040DDB0, abi="default", ret="int",
                          args=["int", "int", "int"], fixture="c3a_placed", state=[(0x00568D0C, 0, 4)],
                          vectors=[(t, x, y) for t in (0, 1, 2, 5, 6, 7, 9)
                                   for x in (0, 10 * 1024, 25 * 1024) for y in (0, 10 * 1024, 40 * 1024)]),
    "shotPower": dict(module="golf_clean.exe", addr=0x00422430, abi="default", ret="int",
                      args=["int", "int", "int"], fixture="c3a_shot",
                      state=[(0x005A47B8, 0, 40), (0x005685C8, 0, 40), (0x0053FD20, 0, 40), (0x005A9CE0, 0, 4)],
                      vectors=[(d, lie, putt) for d in (0, 10, 50, 100, 200, 500, 1000, 2000)
                               for lie in (0, 1, 2, 3) for putt in (0, 1)]),
})
