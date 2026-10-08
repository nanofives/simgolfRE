# terrain_2: Terrain.dll neighbourhood re-blend (hand-named, 2026-10-08)

`Tile::reblendNeighbourhood` 0x10013500 (Terrain.dll, debug build, matched 100% in `re/match/terrain_tile4.cpp`, where
it was `f_10013500`) calls `Tile::blendFaceTextures` 0x10013670 nine times: on the tile itself with its own type, then
with -1 (each tile's own type) on every non-NULL neighbour in `m_n[0..3]` (+0x34) and on the four diagonals reached
through two links (n0->n2, n1->n2, n1->n3, n0->n3). Its callers are `Tile::setTypeId` 0x10014020 and four unnamed
Tile methods (0x1000d480, 0x1000d540, 0x1000de60, 0x10015230). Named because `Tile::blendFaceTextures` was held at C2
with this function as its only caller (round 14, log/c3/c3ba_notes.txt).
