// The random course generator of the terrain module (phase 3.1a, stage B, 2026-10-19). Specification:
// re/analysis/terrain/00470a60_applyTerrainEdit.md (sections A..M), the C3 hook source it cites (reading reference) and
// the disassembly; verified against the original under Unicorn (port/test/test_terrain_gen.py, SG_EMU_ORACLE=1).
// Grids as in portsim/sim/terrain.h: cell (x, y) = x * 50 + y, corner levels x * 51 + y; nothing is bound-checked
// unless the original checks (model_defects.md section 1 and #6: the river and smoothing loops step one row outside).
#ifndef PORTSIM_SIM_TERRAIN_GEN_H
#define PORTSIM_SIM_TERRAIN_GEN_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// generateRandomCourse (hooks.csv name; the C3 hook calls it applyTerrainEdit): generates course n at random from course
// record n of g_courseTable (location_id c0, acres, relief, setting, course_type; year_bought = low byte of g_year).
//  A  g_heightRelief from relief (0x30 / 0x20 / 0x10, else kept), adjusted by n / 4 and g_flags bit 0x01000000.
//  B  every tile gets the base type s10 (4 for course types 0..3; with the flag clear 5 / 0xc for n >= 8, 0xb / 0xc
//     for n >= 12), its path cost; g_ballStopCount, g_tileMoodUp / Down, g_tileVariant zeroed.
//  C  random-walk blobs; D  a river from column 49 (setting != 2); E  a ragged west shore (setting 1);
//  F  flag bit 0x1000 cleared, g_cornerLevel = heightBlend over 51 x 51; G  24 patch walks; H  32 Random::range(0x2a)
//     calls whose 0x100 flags are cleared at once, type 0x10 -> 0xd; I  9 * (4 - g_difficulty) tiles get flag 0x100;
//  J  level smoothing next to water; K  the clubhouse (stampCourseTiles kind 0xf) and the features of c0;
//  L  g_difficulty more objects; M  spawnWalker(-6) posted at the clubhouse, g_tileTypeSaved = g_tileType, clearTile
//     border sweeps, markTileKind11, rebuildHeightfield. Returns rebuildHeightfield's eax (the new g_flags).
// Throws MissingResource when the base type is read from the original's uninitialised stack slot (course type 4 or
// outside 0..4, unless n >= 8 with the flag clear), and NoReturn when a search loop has no candidate left (the original
// spins forever there). spawnWalker (child 3.3) is called through the registry.
int32_t generate_random_course(Ctx &ctx, int32_t n);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_GEN_H
