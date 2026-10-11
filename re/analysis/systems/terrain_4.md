# terrain_4: the Terrain singleton pointer, and three Terrain renames (2026-10-11)

Round 27 (batch c3dp). `g_terrain` 0x10106b4c is the Terrain singleton: Terrain::getInstance 0x100031a0 tests it
(0x100031cd), stores a newly built Terrain there (0x1000321b) and returns it (0x10003221).

Three matched methods were renamed in their sources (re/match/terrain_edge.cpp, terrain_small3.cpp, terrain_tile3.cpp,
still 100%), so re/names/terrain.tsv carries the new names: Terrain::renderTileAtFiveOffsets 0x10038900 (was lowerEdge;
it draws the tile five times at translate offsets and lowers nothing), Terrain::enableBlackLight1 0x10003830 (was
light1Off; it disables GL_LIGHT0 and enables GL_LIGHT1 with zero colours), Terrain::initLighting 0x100037e0 (was
initLists; it loads the lighting file and touches no list).
