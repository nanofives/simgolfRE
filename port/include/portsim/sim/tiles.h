// Tile-grid and placed-object accessors of the simulation core (phase 3.0, 2026-10-19). Stage B (3.1a terrain, 3.1b
// course, 3.3 golfers) calls these instead of indexing the grids itself.
// Specifications: re/analysis/terrain/<addr>_<name>.md, re/analysis/util/00456bb0_overlayCharAt.md,
// re/analysis/course/0040df80_objectAt.md, re/analysis/sim/004070b0_inRange.md and the vector corpus.
//
// The grids are 50 x 50, cell (x, y) = element x * 50 + y (re/model/include/sections/c.h). Like the original, these
// accessors do NOT bound-check unless stated: an index outside the grid reads the neighbouring GameState bytes
// (model_defects.md section 1), and a type byte outside 0..22 indexes g_tileTypeAttrs out of range the same way.
#ifndef PORTSIM_SIM_TILES_H
#define PORTSIM_SIM_TILES_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

constexpr int32_t kGridW = 50;            // SGC_GRID_W
constexpr int32_t kTileOutOfBounds = 20;  // TILE_OUT_OF_BOUNDS: off the grid or land not bought

// tileType: g_tileType[x][y] sign-extended (unchecked).
int32_t tile_type(Ctx &ctx, int32_t x, int32_t y);
// tileByte: g_tileVariant[x][y] zero-extended (unchecked).
int32_t tile_byte(Ctx &ctx, int32_t x, int32_t y);
// tileFlag20: bit 0x20 (TF_PATH) of the low byte of g_tileFlags[x][y] (unchecked).
int32_t tile_flag20(Ctx &ctx, int32_t x, int32_t y);
// tileBlocked: 1 when (x, y) is off the grid or its type is 20, else 0 (the only checked accessor).
int32_t tile_blocked(Ctx &ctx, int32_t x, int32_t y);
// typeAtPos: the tile type under world point (px, py) (0x400 units per tile, arithmetic shift), 20 when blocked.
int32_t type_at_pos(Ctx &ctx, int32_t px, int32_t py);
// overlayCharAt: 3 when the walk byte (+0x22) of the type of (x, y) is > 0, else 2.
int32_t overlay_char_at(Ctx &ctx, int32_t x, int32_t y);
// wallHeight: 0 unless bit `dir` (count mod 32) of the sign-extended wall mask of (x, y) is set; then the cell type's
// signed wall height, or when that is 0 the wall height of the neighbour (x + dirDx[dir], y + dirDy[dir]).
int32_t wall_height(Ctx &ctx, int32_t x, int32_t y, int32_t dir);
// objectAt: index of the first placed object whose footprint covers tile (x, y), or -1. Footprint side = the
// build item's +0x10 byte, plus g_objectTypeLevel[type] - 1 for types >= 6 other than 7; free slots (type -1) skipped.
int32_t object_at(Ctx &ctx, int32_t x, int32_t y);
// inRange: 1 when lo < v < hi (signed) for the window of tile type `type`; types 0x15 / 0x16 start from the hole
// field of g_tileFlags[row][col] (hi 200 or 0, lo 0), others from hi = lo = type; types 0xd..0x10 then draw hi
// from g_rng by g_courseType 0..3. Advances g_rng exactly when the original does.
int32_t in_range(Ctx &ctx, int32_t type, int32_t v, int32_t row, int32_t col);

}  // namespace portsim

#endif  // PORTSIM_SIM_TILES_H
