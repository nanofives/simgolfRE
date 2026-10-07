# terrain_1: Terrain.dll face blending (hand-named, 2026-10-08)

`Tile::blendFaceTextures` 0x10013670 (Terrain.dll, debug build, matched 100% in `re/match/terrain_blend.cpp`) picks
the eight face textures of a tile from the types of its four neighbours (`m_n[0..3]` at +0x34) and the four
diagonals reached through them. Type -1 means the tile's own type (+0x24); type 0x14 (hidden) returns at once, as does
a type `Tile::isSolidType` 0x100154a0 rejects. The neighbour values come from `Tile::edgeKind` 0x10015650, the face
writes go through `Tile::setFaceTexture` 0x10012ec0, with `Terrain::idClass` 0x10013dd0 and `Terrain::flagCode`
0x10013f00 mapping the combination to a texture. On course type 1 (g_courseType 0x10070a0c) with tile type 0x11, a
face next to a neighbour for which `Tile::isOpenType` 0x100155b0 holds is set with type 0x19 and the type is restored
to 0x11 afterwards. Four diagonal flags are set but never read. Named here because its five callees reached C3 (or
were held) only through this one caller, which was still `FUN_10013670`.
