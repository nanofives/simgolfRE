// The world size of the simulation (P38A, 2026-11-19; plan re/roadmap/P38_LARGE_MAPS.md phase A).
//
// The original's world is 50 x 50 tiles: 20 GameState grids of one entry per tile, index x * 50 + y (re/model/include/sections/c.h),
// the 51 x 51 vertex grid g_cornerLevel (index x * 51 + y), the 8-byte corner cache g_cornerCache (8 * tile index), the
// Terrain.dll object (tiles x + y * width, terrain_state.h) and its vertex arrays. Every port site that indexes one of them, bounds a
// tile coordinate or walks a whole grid takes the size from here, so a later phase can run another N (phase B maps the grids into
// side storage; nothing in phase A sets N to anything but 50, and at 50 every address, read and write order is the original's).
//
// Rules (port/test/test_world_size.py refuses a new size literal next to a grid symbol):
//  - a grid's base is grid_va / grid_arr / tile_grid / vertex_grid, never state_va / state_arr / state_grid / GS() of the member;
//  - a tile index is tile_index (x * N + y), a vertex index vertex_index (x * (N + 1) + y), a corner-cache entry 8 * tile index;
//  - strides and bounds are world_n (N, the original's 50 / 0x32), world_v (N + 1, 51), world_last (N - 1, 49 / 0x31: the
//    camera's bound), world_buy (N - 2, the 48 buyable rows / columns 1 .. N - 2), world_lot ((N - 2) / 3, the 16 of a land
//    lot's side), world_tiles (N * N, 2500), world_verts
//    ((N + 1)^2, 2601). Record sizes, UI coordinates and other values that only look alike stay literals (the allowlist,
//    port/test/world_size_allow.tsv, names the reviewed ones).
#ifndef PORTSIM_SIM_WORLD_H
#define PORTSIM_SIM_WORLD_H

#include <cstddef>
#include <cstdint>

#include "gamestate.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"

namespace portsim {

constexpr int32_t kWorldOriginal = 50;  // the original's tiles a side (SGC_GRID_W); every .sve and fixed course is this size

// The GameState holds the grids for the original size only (its layout is pinned: corpus images, oracle, goldens, save tables).
constexpr size_t kGridTiles = static_cast<size_t>(kWorldOriginal) * kWorldOriginal;               // 2500
constexpr size_t kGridVerts = static_cast<size_t>(kWorldOriginal + 1) * (kWorldOriginal + 1);   // 2601

inline int32_t world_n(const Ctx &c) { return c.world_n; }                    // tiles a side
inline int32_t world_v(const Ctx &c) { return c.world_n + 1; }                // vertices a side
inline int32_t world_last(const Ctx &c) { return c.world_n - 1; }             // highest tile coordinate (camera bound)
inline int32_t world_buy(const Ctx &c) { return c.world_n - 2; }              // buyable rows / columns (1 .. N - 2)
inline int32_t world_lot(const Ctx &c) { return (c.world_n - 2) / 3; }      // a land lot's side (16): 3 x 3 lots
inline int32_t world_tiles(const Ctx &c) { return c.world_n * c.world_n; }    // entries of a tile grid
inline int32_t world_verts(const Ctx &c) { return (c.world_n + 1) * (c.world_n + 1); }
// the world's extent in world units (0x400 per tile): N << 10 (0xc800 at 50), the last unit (N << 10) - 1 (0xc7ff)
inline int32_t world_units(const Ctx &c) { return c.world_n << 10; }
// tile (x, y) of a tile grid and vertex (x, y) of the vertex grid, with the original's wrapping imul / add (no bound check)
#if defined(SG_P38A_NEGCTL_N51)  // negative control build only (log/roadmap/P38/P38A_notes.md): this accessor reads N as N + 1
inline int32_t tile_index(const Ctx &c, int32_t x, int32_t y) { return add32(mul32(x, c.world_n + 1), y); }
#else
inline int32_t tile_index(const Ctx &c, int32_t x, int32_t y) { return add32(mul32(x, c.world_n), y); }
#endif
inline int32_t vertex_index(const Ctx &c, int32_t x, int32_t y) { return add32(mul32(x, c.world_n + 1), y); }

namespace detail {
template <size_t K> constexpr bool is_world_grid() { return K == kGridTiles || K == kGridVerts || K == 8 * kGridTiles; }
// P38B: the side address of the grid at GameState offset `off` (N != 50): its cluster's base + (off - lo) * scale
inline uint32_t side_va(const Ctx &c, uint32_t off) {
  for (int32_t k = 0; k < c.world_nclusters; ++k) {
    const Ctx::WorldCluster &w = c.world_clusters[k];
    if (off >= w.lo && off < w.hi) return w.va + (off - w.lo) * c.world_scale;
  }
  throw Fault{off, 0};
}
}  // namespace detail

// The virtual address of element 0 of a world grid: the GameState member while N == 50; P38B: its block in the side storage
// world_resize mapped for N != 50 (every grid scaled by the same factor inside its cluster: no per-grid lookup).
template <class M, size_t K> uint32_t grid_va(Ctx &c, M (GameState::*m)[K]) {
  static_assert(detail::is_world_grid<K>(), "grid_va names a per-tile or per-vertex grid of the GameState");
  if (c.world_n == kWorldOriginal) return c.mem.va_of(&(c.gs().*m)[0]);
  return detail::side_va(c, static_cast<uint32_t>(reinterpret_cast<const uint8_t *>(&(c.gs().*m)[0]) - c.bytes()));
}
template <class M, size_t K> Ptr<M> grid_arr(Ctx &c, M (GameState::*m)[K]) { return Ptr<M>(c.mem, grid_va(c, m)); }
// Entries of a grid at the current N: N * N, (N + 1)^2 for the vertex grid, 8 * N * N for the corner cache.
template <size_t K> int32_t grid_entries(const Ctx &c) {
  return K == kGridVerts ? world_verts(c) : K == 8 * kGridTiles ? 8 * world_tiles(c) : world_tiles(c);
}
// Host access for the typed-member sites (a whole-grid loop, std::vector copies, e.type_new = ...[i]): the grid's host memory,
// valid for grid_entries entries until the next mapping change, and one entry by flat index (no wrap: the index must be
// inside the grid, as it is at every such site; Memory::at throws Fault outside every mapping).
template <class M, size_t K> M *grid_host(Ctx &c, M (GameState::*m)[K]) {
  return reinterpret_cast<M *>(c.mem.at(grid_va(c, m), static_cast<uint32_t>(grid_entries<K>(c)) * sizeof(M), true));
}
template <class M, size_t K> M &grid_ref(Ctx &c, M (GameState::*m)[K], int32_t i) {
  return c.mem.ref<M>(grid_va(c, m) + static_cast<uint32_t>(i) * static_cast<uint32_t>(sizeof(M)), true);
}
// The dword count of the original's `rep stosd` over a whole tile grid of `elem_bytes` per entry (0x271 for a byte grid, 0x4e2 for
// g_tileFlags at N = 50), and the bytes left after them when N * N * elem_bytes is not a multiple of 4 (N odd; 0 at 50 and 152):
// grid_fill writes the dwords then the tail's bytes from the pattern's low end.
inline int32_t grid_dwords(const Ctx &c, int32_t elem_bytes) { return world_tiles(c) * elem_bytes / 4; }
inline int32_t grid_tail(const Ctx &c, int32_t elem_bytes) { return world_tiles(c) * elem_bytes % 4; }
inline void grid_fill_tail(Ctx &c, uint32_t grid, int32_t elem_bytes, uint32_t pattern) {
  const uint32_t at = grid + static_cast<uint32_t>(grid_dwords(c, elem_bytes)) * 4u;
  for (int32_t i = 0; i < grid_tail(c, elem_bytes); ++i) c.mem.store<uint8_t>(at + static_cast<uint32_t>(i), static_cast<uint8_t>(pattern >> (8 * i)));
}

// ---- P38B: the world size changes (world.cpp)
// world_resize: N = 50 drops the side storage (the GameState grids again); another N (>= 50, (N - 2) % 3 == 0 for equal lots,
// <= 256) maps zeroed side blocks for every grid, the path ring and the walker posts, and tells an attached TerrainApi
// (TerrainApi::world_resized: the Terrain object and its vertex arrays at N). The grids' contents are not carried over: the
// caller starts a new course. Returns false for an N it does not take.
bool world_resize(Ctx &c, int32_t n);
bool world_size_ok(int32_t n);
// pathStep's ring: g_pathQueueX / Y (1,024 entries, mask 0x3ff) at 50, a side ring of the next power of two >= N * N above
uint32_t world_ring_va(Ctx &c, int axis);
uint32_t world_ring_mask(const Ctx &c);
// walker posts: Walker::post_x / post_y (int8) while N <= 127, an int16 side table above (-1 = no post, like the byte 0xff)
int32_t walker_post(Ctx &c, int32_t walker, int axis);
void set_walker_post(Ctx &c, int32_t walker, int axis, int32_t v);
// A tile grid as Grid<M> (cell(x, y) = x * N + y) and the vertex grid (x * (N + 1) + y).
template <class M, size_t K> Grid<M> tile_grid(Ctx &c, M (GameState::*m)[K]) {
  static_assert(K == kGridTiles, "tile_grid names a per-tile grid");
  return Grid<M>{grid_arr(c, m), c.world_n};
}
template <class M, size_t K> Grid<M> vertex_grid(Ctx &c, M (GameState::*m)[K]) {
  static_assert(K == kGridVerts, "vertex_grid names the per-vertex grid");
  return Grid<M>{grid_arr(c, m), c.world_n + 1};
}

}  // namespace portsim

#endif  // PORTSIM_SIM_WORLD_H
