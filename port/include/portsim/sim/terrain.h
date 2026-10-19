// Terrain, tile-grid and heightfield functions of the simulation core (phase 3.1a, stage B, 2026-10-19).
// Specifications: re/analysis/{terrain,sim,course,render,golfer}/<addr>_<name>.md, the C3/C4 hook sources named in
// re/port/simcore_order.tsv (reading reference) and the vector corpus. Grids are 50 x 50 (cell (x, y) = x * 50 + y),
// the corner-level grid g_cornerLevel is 51 x 51 (x * 51 + y). Like the original, nothing here bound-checks unless
// stated: an index outside a grid reads or writes the neighbouring GameState bytes (model_defects.md section 1).
#ifndef PORTSIM_SIM_TERRAIN_H
#define PORTSIM_SIM_TERRAIN_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

constexpr int32_t kCornerW = 51;  // g_cornerLevel row width

// ---- level 0
// freeAtTile: frees (x = -1) every live ball record whose start point (>> 10) lies on tile (tx, ty).
void free_at_tile(Ctx &ctx, int32_t tx, int32_t ty);
// raise / lowerCornerLevelsAround: every corner level of the 5 x 5 block centred on corner (x, y) more than its
// g_levelFalloffKernel entry below (above) the centre level is raised (lowered) by one (byte arithmetic).
void raise_corner_levels_around(Ctx &ctx, int32_t x, int32_t y);
void lower_corner_levels_around(Ctx &ctx, int32_t x, int32_t y);
// pushTripleEntry: appends (a, b, c) to g_editQueueX / Y / Mode while the count unk_005a9cd4 is below 0x100 (signed).
void push_triple_entry(Ctx &ctx, int32_t a, int32_t b, int32_t c);
// setGrids: g_tileBuildCode[x][y] = low byte of a, g_tileRefund[x][y] = low byte of b.
void set_grids(Ctx &ctx, int32_t x, int32_t y, int32_t a, int32_t b);
// floodMark: recursive flood fill of flag bit 0x40 over the orthogonal neighbours carrying bit 0x20 or 0x400, through
// tiles whose type's walk byte is > 0 (no bound check on the neighbours).
void flood_mark(Ctx &ctx, int32_t x, int32_t y);
// bilinearSample: bilinear interpolation of the signed 19-wide g_sampleGrid (axes biased by -0x80, 8.5 fixed point).
int32_t bilinear_sample(Ctx &ctx, int32_t a, int32_t b);
// clearTile: tile type 0x11 (current course setting 2, island) else 0x14; for setting 2 also clears flag bits 0x320.
void clear_tile(Ctx &ctx, int32_t x, int32_t y);

// ---- level 1
// canStep: 1 when a walker on (x, y) may step in direction dir (walk bytes, types 0x15 / 0x16, neighbour flags 0x120,
// tileBlocked of the neighbour, in the original's order).
int32_t can_step(Ctx &ctx, int32_t x, int32_t y, int32_t dir);
// evalPlacementArea: the placement score of an n x n area at (x, y) for build kind `kind`, or -1.
int32_t eval_placement_area(Ctx &ctx, int32_t x, int32_t y, int32_t n, int32_t kind);
// nearestPlaced: index of the nearest placed object of `type` to world point (x, y) (distance in g_nearTileDist), -1.
int32_t nearest_placed(Ctx &ctx, int32_t type, int32_t x, int32_t y);
// findNearestTargetTile: the nearest unblocked tile with flag 0x200 in the 9 x 9 block around (px, py) within
// maxDist tiles: g_nearTileDist / g_nearTileX / g_nearTileY (g_nearTileX = -1 when none).
void find_nearest_target_tile(Ctx &ctx, int32_t px, int32_t py, int32_t max_dist);
// sampleHeight: with x, y >> 2: clamp((bilinear(x, y) * 6 + bilinear(2x, 2y) * 4) * 7 / 64 / 2, 0, 0x200).
int32_t sample_height(Ctx &ctx, int32_t x, int32_t y);
// propagateType11: recomputes g_tileVariant[x][y] from the 8 neighbours' collar group (0x11) and variant bytes and,
// when it changed, recurses into every unblocked neighbour of collar group 0x11.
uint32_t propagate_type11(Ctx &ctx, int32_t x, int32_t y);  // returns the original's eax
// rebuildTileCoverage: clears flag bit 0x40 everywhere, flood-marks from the clubhouse footprint (placed object 0),
// then recomputes bit 0x40 of every placed object's flags byte.
void rebuild_tile_coverage(Ctx &ctx);
// relaxTile42f630 / raiseFromNeighbours: g_tileLevel[x][y] lowered (raised) to the lowest (highest) qualifying
// orthogonal neighbour of the same type; 1 when it changed.
int32_t relax_tile(Ctx &ctx, int32_t x, int32_t y);
int32_t raise_from_neighbours(Ctx &ctx, int32_t x, int32_t y, int32_t same_byte);
// spreadShade: stamps value * 2 / 3 over the 15 x 15 block centred on (row, col), falling off with approxDistance,
// into g_tileShadeB (positive) or out of g_tileShadeA (zero or negative), clamped to 0..255.
void spread_shade(Ctx &ctx, int32_t row, int32_t col, int32_t value);

// ---- level 2
// heightBlend: the display height level 3..15 of tile (x, y) (3 off the grid or with g_flags bit 0; the corner level
// once g_date != 0; else sampleHeight scaled by g_heightRelief and the course record's relief and type).
int32_t height_blend(Ctx &ctx, int32_t x, int32_t y);
// rateTile: rating of golfer record i (skills, class, lie under the ball, preference flag), capped at 330.
int32_t rate_tile(Ctx &ctx, int32_t i);
// markTileKind11: variant 2 on every tile of collar group 0x11, then propagateType11 on each of them.
uint32_t mark_tile_kind11(Ctx &ctx);  // returns the original's eax
// spreadAllShade: clears g_tileShadeA / B, then (ball-stop total >= 10) spreads every g_tileMoodUp / Down byte.
void spread_all_shade(Ctx &ctx);
// genNoise: 18 x 19 Random::range(16) bytes into g_noiseSource (wrapped), a 17 x 19 wrapped copy into g_sampleGrid.
void gen_noise(Ctx &ctx);

// ---- level 3
// scanTileLine: score >= 0 of the (optionally curved) line of tiles from world point (x0, y0) to tile (tx, ty):
// walk bytes of the last cells, type-0xd hazards weighted by distance, edges toward blocked or 0x11 tiles. Type 1's
// walk byte is 0 while it runs and 0xff afterwards (the original's own stores).
int32_t scan_tile_line(Ctx &ctx, int32_t x0, int32_t y0, int32_t tx, int32_t ty, int32_t range, int32_t curve,
                       int32_t strict);

// ---- heightfield (levels 3..6)
// cornerRange: max / min of heightBlend over (x, y), (x+1, y), (x+1, y-1), (x, y-1).
void corner_range(Ctx &ctx, int32_t x, int32_t y, int32_t &mx, int32_t &mn);
// cornerHeights: height level of corner `corner` (odd; & 7 picks 1 / 3 / 5 / 7) of tile (x, y): 3 off the grid or for
// an even corner; the g_cornerCache byte when flag != 0 and it is non-zero; per the type's TTA_* flags the tile level
// or cornerRange's min / max, 3 for flag 8; else heightBlend of that corner.
int32_t corner_heights(Ctx &ctx, int32_t x, int32_t y, int32_t corner, int32_t flag);
// tileQuery449310: cornerHeights(a, b, c, 1).
int32_t tile_query449310(Ctx &ctx, int32_t a, int32_t b, int32_t c);
// slopeY / slopeX: the corner-height difference across the half of the tile that world point (x, y) lies in (0 for
// g_flags bit 0, types 7 / 9 and types with flags & 0xe).
int32_t slope_y(Ctx &ctx, int32_t x, int32_t y);
int32_t slope_x(Ctx &ctx, int32_t x, int32_t y);
// slopeMix: dirDy[dir] * slopeX + dirDx[dir] * slopeY, both clamped to -1..1 for odd dir.
int32_t slope_mix(Ctx &ctx, int32_t x, int32_t y, int32_t dir);
// relaxEdges42f530: recomputes g_tileWallMask[x][y] from the corner heights against the 4 orthogonal neighbours.
void relax_edges(Ctx &ctx, int32_t x, int32_t y);
// heightAt42fa30: terrain height (level - 3) * 16 at world point (x, y), bilinear over the tile's corners.
int32_t height_at(Ctx &ctx, int32_t x, int32_t y);
// rebuildHeightfield: clears and rebuilds g_tileLevel and g_cornerCache (three passes), then g_tileWallMask.
// Throws NoReturn when the relax / raise pass revisits a level grid (the original loops forever there). Returns the new
// g_flags (the original's eax, which stepCourseEditQueue returns).
uint32_t rebuild_heightfield(Ctx &ctx);

// ---- walker path (terrain_path.cpp)
// pathStep: next direction (0..7, or -1) from world point (p3, p4) toward (p1, p2) for golfer p5: a path-cost wave
// over g_pathGrid (queue g_pathQueueX / Y) from the target tile, avoiding the partner p5 ^ 1, then the cheapest
// neighbour of the walker's tile from a Random::range(8) start; may set g_flags bit 0x80000 on path tiles.
int32_t path_step(Ctx &ctx, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5);

// ---- new-course setup (terrain_setup.cpp); callees of other children through the registry
// buildGolferBioText: the new-course reset (course, golfer, walker, object and history tables), the twelve arrival
// type picks and the bios of golfer types 0..75; returns initRecords' result. NoReturn when a pick is impossible.
int32_t build_golfer_bio_text(Ctx &ctx);
// customizeCharacter: the player's pair of golfers (updateMembership x2), optionally the chosen golf pro (its .pro
// file through loadThemeFile into golfer type 76); returns g2's flags with the start walker's byte in the low byte.
int32_t customize_character(Ctx &ctx, int32_t with_pro);

// ---- footprints (terrain_course.cpp)
// stampCourseTiles: stamps a building / object of `kind` at tile (x, y): level copy, tile types, path cost, flags and
// variant over its side x side square, kind-specific marks; with flags >= 0 a placed-object record (first free slot,
// 0 when none of the first 0x100) and its footprint, then rebuildHeightfield. Returns the record index or 0.
int32_t stamp_course_tiles(Ctx &ctx, int32_t x, int32_t y, int32_t kind, int32_t flags);
// clearObjectFootprint: frees placed object `index` (a lot is refunded through rateLot / pointsPopup / the finance
// year), resets the tiles it covered and runs rebuildHeightfield.
uint32_t clear_object_footprint(Ctx &ctx, int32_t index);  // eax: rebuildHeightfield's

// stepCourseEditQueue: one step of the terrain-edit queue (camera glide toward g_cameraGoalX, or the next queued entry:
// flag xor, stamp, level raise / lower, tee / cup markers with a golfer pair), then markTileKind11, rebuildTileCoverage,
// rebuildHeightfield; returns the original's eax on each path.
int32_t step_course_edit_queue(Ctx &ctx);

// demolishObject: the demolish tool on tile (x, y): a placed object asks through showPopup (phase 3.7) and is cleared;
// a building or hole part is removed and refunded; flag-only tiles lose their flag. Returns the original's eax.
int32_t demolish_object(Ctx &ctx, int32_t x, int32_t y);
// buildTournamentPrep: corrects every open hole's par to its length class (showPopup answers which changes stay),
// then with answer bit 0 places up to two decorations per hole by a random walk from the cup. Returns the eax.
int32_t build_tournament_prep(Ctx &ctx);

// ---- labels (terrain_text.cpp)
// decorationName: appends the scenery name of tile (x, y) (id -1), of placed object id & 0xff (bit 0x100) or of tile
// type id to g_textBuffer; kind-4 building tiles go through 3.1b's landmarkName (registry). Returns 1.
int32_t decoration_name(Ctx &ctx, int32_t x, int32_t y, int32_t id);
// buildToolLabel: the build / remove cursor label of tile (x, y) in g_textBuffer; 1 when it names a removable item.
int32_t build_tool_label(Ctx &ctx, int32_t x, int32_t y);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_H
