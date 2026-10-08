# terrain_3: Tile::setTypeId's callees (hand-named, 2026-10-08)

All six are matched 100% in re/match (terrain_tile3.cpp, terrain_tile4.cpp, terrain_type6.cpp, where they were
`f_<addr>`) and have one caller, `Tile::setTypeId` 0x10014020. setTypeId first leaves the old type (old type 7 ->
`Tile::leaveType7` 0x100149e0, old type 6 -> `Tile::leaveType6` 0x10014190), stores the new type, then dispatches on it:
7 -> `Tile::enterType7` 0x1000d540 (-13.0, 0x1005f518), 6 -> `Tile::enterType6` 0x1000de60 (+20.0, 0x1005f51c),
0 and 9 -> `Tile::resetFaceTextures` 0x10015230, 4 and 0x14 -> `Tile::reblendNeighbourhood` 0x10013500 directly, and
0x11, 0xd..0x10 and every other type -> `Tile::setPathFlagForType` 0x1000d480. Every enter/reset path ends in
reblendNeighbourhood. Types 6 and 7 have no name in the type table the Terrain constructor fills (terrain_ctor.cpp), so
the names carry the numbers. Named to unblock setTypeId's callee group for C3 (log/c3/c3bf_notes.txt).
