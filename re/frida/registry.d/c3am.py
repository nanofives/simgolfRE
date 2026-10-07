# Batch c3am: Terrain.dll Tile accessors/mutators, the Terrain::getWall forwarder, the PathInfo/WallInfo clears and
# the CRT abs used by the bitmap loaders. addr is the RVA (VA = 0x10000000 + RVA); fixture c3am_tiles builds our own
# 14 Tile buffers (never the live Terrain object).
_TILES = ["$t%d" % i for i in range(14)]
_SIDES = list(range(9))
_STATE = [("$base", 0, 14 * 0x248)]   # the whole tile block: the three writers are compared on it

HOOKS = {
    # --- readers over the 14 tiles (one vector per tile: 14 distinct field values) -----------------------------
    "Tile::getType": dict(module="Terrain.dll", addr=0x00001F60, abi="thiscall", ret="int", args=["pointer"],
                          fixture="c3am_tiles", vectors=[(t,) for t in _TILES]),
    "Tile::getX": dict(module="Terrain.dll", addr=0x00005960, abi="thiscall", ret="int", args=["pointer"],
                       fixture="c3am_tiles", vectors=[(t,) for t in _TILES]),
    "Tile::getY": dict(module="Terrain.dll", addr=0x00006810, abi="thiscall", ret="int", args=["pointer"],
                       fixture="c3am_tiles", vectors=[(t,) for t in _TILES]),
    # types 0x14 at tiles 0 and 10 take the sete's true side; the other 12 take the false side
    "Tile::isHidden": dict(module="Terrain.dll", addr=0x00015460, abi="thiscall", ret="uint8", args=["pointer"],
                           fixture="c3am_tiles", vectors=[(t,) for t in _TILES]),
    # je 0x100154c1 taken by 9; je 0x100154c7 taken by 0; every other value falls through to the store of 1
    "Tile::isSolidType": dict(module="Terrain.dll", addr=0x000154A0, abi="thiscall", ret="uint8",
                              args=["pointer", "int"], fixture="c3am_tiles",
                              vectors=[("$t0", v) for v in (-0x80000000, -9, -1, 0, 1, 2, 4, 6, 7, 8, 9, 10, 0x11,
                                                            0x14, 0x24, 0x7FFFFFFF)]),
    # byte offset arithmetic: sides 0..8 on four tiles, plus out-of-range and negative offsets inside the tile
    "Tile::getWall": dict(module="Terrain.dll", addr=0x00001ED0, abi="thiscall", ret="uint8",
                          args=["pointer", "int"], fixture="c3am_tiles",
                          vectors=[(t, s) for t in _TILES[:4] for s in _SIDES]
                                  + [("$t4", s) for s in (-0x20, -4, -1, 9, 0x10, 0x13)]),
    # forwards (tile, side) to Tile::getWall 0x10001ed0 and ignores its own this
    "Terrain::getWall": dict(module="Terrain.dll", addr=0x00001E80, abi="thiscall", ret="uint8",
                             args=["pointer", "pointer", "int"], fixture="c3am_tiles",
                             vectors=[("$ter", t, s) for t in _TILES[:4] for s in _SIDES]
                                     + [("$ter", "$t5", s) for s in (-4, -1, 9, 0x13)]),

    # --- writers (state = the whole tile block) ---------------------------------------------------------------
    # the flag is stored as a BYTE from [ebp+0x10], so 0x1234 stores 0x34; the height is a dword at side*4+0x210
    "Tile::setWall": dict(module="Terrain.dll", addr=0x00015400, abi="thiscall", ret="void",
                          args=["pointer", "int", "int", "int"], fixture="c3am_tiles", state=_STATE,
                          vectors=[(_TILES[i % 4], s, h, on)
                                   for i, (s, h, on) in enumerate(
                                       [(s, s * 7 - 3, s & 1) for s in _SIDES]
                                       + [(s, -s * 1000, 1) for s in _SIDES]
                                       + [(s, 0x7FFFFFFF, 0) for s in _SIDES]
                                       + [(0, 0, 0x1234), (1, -0x80000000, 0xFF), (13, 42, 1), (-1, 7, 0),
                                          (-4, -7, 1), (9, 0x1000, 0x100)])]),
    # i*4+0x34; -1 and 4 land on the neighbouring fields of our own buffer, 0 passes NULL
    "Tile::setNeighbour": dict(module="Terrain.dll", addr=0x0000C520, abi="thiscall", ret="void",
                               args=["pointer", "int", "pointer"], fixture="c3am_tiles", state=_STATE,
                               vectors=[(_TILES[i % 4], k, n)
                                        for i, (k, n) in enumerate(
                                            [(k, "$t%d" % (k + 6)) for k in range(4)]
                                            + [(k, 0) for k in range(4)]
                                            + [(k, "$t13") for k in range(4)]
                                            + [(-1, "$t7"), (4, "$t8"), (4, 0), (-1, 0)])]),
    # zeroes seven bytes of each PathInfo block (all seeded non-zero, so every vector changes the state)
    "PathInfo::clear": dict(module="Terrain.dll", addr=0x0000F6E0, abi="thiscall", ret="void", args=["pointer"],
                            fixture="c3am_tiles", state=_STATE, vectors=[("$p%d" % i,) for i in range(14)]),
    # zeroes only wall flags 0, 2, 4, 6 of each WallInfo block; flags 1, 3, 5, 7, 8 and the heights stay
    "WallInfo::clear": dict(module="Terrain.dll", addr=0x0000F7A0, abi="thiscall", ret="void", args=["pointer"],
                            fixture="c3am_tiles", state=_STATE, vectors=[("$w%d" % i,) for i in range(14)]),

    # --- __cdecl leaf: jl 0x100158b8 splits negatives from zero and positives -----------------------------------
    "iabs": dict(module="Terrain.dll", addr=0x000158B0, abi="default", ret="int", args=["int"],
                 vectors=[(v,) for v in (-0x80000000, -0x7FFFFFFF, -100000, -1000, -129, -128, -2, -1, 0, 1, 2,
                                         127, 128, 129, 1000, 100000, 0x3FFFFFFF, 0x7FFFFFFE, 0x7FFFFFFF,
                                         -0x40000000, -7, 7, -16, 16)]),
}
