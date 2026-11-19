// Tile-grid and placed-object accessors (see portsim/sim/tiles.h). Phase 3.0, written from the specification (analysis
// notes, disassembly-level transcriptions in those notes, vector corpus); no original address or code.
#include "portsim/sim/tiles.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "portsim/ptr.h"
#include "portsim/sim/terrain_api.h"  // P38B: world_resize tells the Terrain
#include "portsim/sim/util.h"
#include "portsim/sim/world.h"  // P38A: grid bases and the world size

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   tileType 1 = transposed index (y * N + x), 2 = zero-extended (BLIND: no negative type byte in the vectors)
//   tileByte 1 = sign-extended          tileFlag20 1 = bit 0x10
//   tileBlocked 1 = x == N allowed     typeAtPos 1 = shift 9               overlayCharAt 1 = walk >= 0
//   wallHeight 1 = no neighbour fallback, 2 = dirDx / dirDy swapped, 3 = mask zero-extended (BLIND: needs dir >= 8
//              with a negative mask byte)
//   objectAt 1 = no level extension, 2 = right edge inclusive, 3 = type 7 also extended (BLIND: no type-7 object
//            with a level other than 1 in the vectors)
//   inRange 1 = courseType 1's type 0xf window lo 0x32, 2 = no RNG draw for type 0x10
int g_negctl_tile_type = 0;
int g_negctl_tile_byte = 0;
int g_negctl_tile_flag20 = 0;
int g_negctl_tile_blocked = 0;
int g_negctl_type_at_pos = 0;
int g_negctl_overlay_char_at = 0;
int g_negctl_wall_height = 0;
int g_negctl_object_at = 0;
int g_negctl_in_range = 0;

Grid<int8_t> types(Ctx &c) { return tile_grid(c, &GameState::g_tileType); }
Ptr<TileTypeAttr> attr(Ctx &c, int32_t type) { return state_arr(c, &GameState::g_tileTypeAttrs) + type; }
}  // namespace

int32_t tile_type(Ctx &ctx, int32_t x, int32_t y) {
  const int8_t t = SG_NEGCTL(g_negctl_tile_type) == 1 ? types(ctx)(y, x) : types(ctx)(x, y);
  return SG_NEGCTL(g_negctl_tile_type) == 2 ? i32(zx8(u32(t))) : t;
}

int32_t tile_byte(Ctx &ctx, int32_t x, int32_t y) {
  const uint8_t b = tile_grid(ctx, &GameState::g_tileVariant)(x, y);
  return SG_NEGCTL(g_negctl_tile_byte) == 1 ? sx8(b) : i32(b);
}

int32_t tile_flag20(Ctx &ctx, int32_t x, int32_t y) {
  // the original reads only the low byte of the flag word
  const uint8_t lo = tile_grid(ctx, &GameState::g_tileFlags).cell(x, y).as<uint8_t>().get();
  return lo & (SG_NEGCTL(g_negctl_tile_flag20) == 1 ? 0x10 : 0x20);
}

int32_t tile_blocked(Ctx &ctx, int32_t x, int32_t y) {
  const int32_t xmax = SG_NEGCTL(g_negctl_tile_blocked) == 1 ? world_n(ctx) + 1 : world_n(ctx);
  if (x < 0 || x >= xmax || y < 0 || y >= world_n(ctx)) return 1;
  return types(ctx)(x, y) == kTileOutOfBounds ? 1 : 0;
}

int32_t type_at_pos(Ctx &ctx, int32_t px, int32_t py) {
  const int32_t shift = SG_NEGCTL(g_negctl_type_at_pos) == 1 ? 9 : 10;
  const int32_t x = sar32(px, shift), y = sar32(py, shift);
  if (tile_blocked(ctx, x, y)) return kTileOutOfBounds;
  return types(ctx)(x, y);
}

int32_t overlay_char_at(Ctx &ctx, int32_t x, int32_t y) {
  const int8_t walk = attr(ctx, types(ctx)(x, y)).field(&TileTypeAttr::walk).get();
  const bool open = SG_NEGCTL(g_negctl_overlay_char_at) == 1 ? walk >= 0 : walk > 0;
  return open ? 3 : 2;
}

int32_t wall_height(Ctx &ctx, int32_t x, int32_t y, int32_t dir) {
  const int8_t m8 = tile_grid(ctx, &GameState::g_tileWallMask)(x, y);
  const int32_t mask = SG_NEGCTL(g_negctl_wall_height) == 3 ? i32(zx8(u32(m8))) : m8;
  if ((shl32(1, dir) & mask) == 0) return 0;
  const int8_t h = attr(ctx, types(ctx)(x, y)).field(&TileTypeAttr::wall_height).get();
  if (h != 0 || SG_NEGCTL(g_negctl_wall_height) == 1) return h;
  const bool swap = SG_NEGCTL(g_negctl_wall_height) == 2;
  const int32_t dx = state_arr(ctx, &GameState::f_tab_dirDx_4c2878)[dir];
  const int32_t dy = state_arr(ctx, &GameState::f_tab_dirDy_4c2898)[dir];
  if (swap) return attr(ctx, types(ctx).cell(add32(x, dy), add32(dx, y)).get()).field(&TileTypeAttr::wall_height).get();
  // the original computes (x + dx) * 50 + dy + y
  const int8_t nt = types(ctx).cell(add32(x, dx), add32(dy, y)).get();
  return attr(ctx, nt).field(&TileTypeAttr::wall_height).get();
}

int32_t object_at(Ctx &ctx, int32_t x, int32_t y) {
  const Ptr<PlacedObject> objs = state_arr(ctx, &GameState::g_placed_objects);
  const Ptr<F_BuildItem> items = state_arr(ctx, &GameState::f_tab_buildItems_4c26b0);
  const Ptr<int32_t> level = state_arr(ctx, &GameState::g_objectTypeLevel);
  const int mode = SG_NEGCTL(g_negctl_object_at);
  const int32_t n = static_cast<int32_t>(sizeof(GameState::g_placed_objects) / sizeof(PlacedObject));
  for (int32_t i = 0; i < n; ++i) {
    const Ptr<PlacedObject> o = objs + i;
    const int16_t type = o.field(&PlacedObject::type).get();
    if (type == -1) continue;
    int32_t side = (items + type).field(&F_BuildItem::b10).get();
    if (type >= 6 && type != 5 && (type != 7 || mode == 3) && mode != 1) side = add32(side, sub32(level[type], 1));
    const int32_t rx = o.field(&PlacedObject::x).get(), ry = o.field(&PlacedObject::y).get();
    const int32_t ex = add32(rx, side), ey = add32(ry, side);
    if (x < rx || (mode == 2 ? x > ex : x >= ex)) continue;
    if (y < ry || y >= ey) continue;
    return i;
  }
  return -1;
}

namespace {
// inRange's body; `widest` (P32D, in_range_widest): every Random::range(n) of the window answers n - 1 (the highest window) and
// g_rng is not drawn
int32_t in_range_body(Ctx &ctx, int32_t type, int32_t v, int32_t row, int32_t col, bool widest) {
  int32_t hi, lo;
  if (type == 0x16 || type == 0x15) {
    const uint8_t f = tile_grid(ctx, &GameState::g_tileFlags).cell(row, col).as<uint8_t>().get();
    hi = (static_cast<int32_t>(f & 0x1F) - 1 > 3) ? 200 : 0;
    lo = 0;
  } else {
    hi = lo = type;  // the window is empty
  }
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  auto r = [&](uint32_t n) {
    if (widest) return static_cast<int32_t>((n & 0xFFFFu) ? (n & 0xFFFFu) - 1 : 0);
    return static_cast<int32_t>(random_range(seed, n) & 0xFFFFu);
  };
  const int mode = SG_NEGCTL(g_negctl_in_range);
  const auto arm16 = [&] {  // type 0x10, shared by modes 0..2
    if (type != 0x10) return;
    if (mode == 2) {
      hi = 0x190;
    } else {
      hi = add32(r(200), 0x190);
    }
    lo = 0x4B;
  };
  switch (ctx.gs().g_courseType) {  // signed byte, 0..3 (any other value keeps the initial window)
    case 0:
      if (type == 0xD) hi = add32(r(100), 0x190), lo = 0x32;
      else if (type == 0xE) hi = add32(r(100), 0x64), lo = 0x14;
      else if (type == 0xF) hi = add32(r(100), 0x64), lo = 0x32;
      else arm16();
      break;
    case 1:
      if (type == 0xD) hi = add32(r(100), 0x190), lo = 0x32;
      else if (type == 0xE) hi = add32(r(100), 0x64), lo = 0x14;
      else if (type == 0xF) hi = add32(r(100), 0x12C), lo = mode == 1 ? 0x32 : 0x64;
      else arm16();
      break;
    case 2:
      if (type == 0xD) hi = add32(r(100), 0x64), lo = 0x14;
      else if (type == 0xE) hi = add32(r(100), 0x190), lo = 0x32;
      else if (type == 0xF) hi = add32(r(100), 0x64), lo = 0x32;
      else arm16();
      break;
    case 3:
      if (type >= 0xD && type <= 0x10) hi = add32(r(100), 0x190), lo = 0x14;
      break;
    default:
      break;
  }
  return (v < hi && v > lo) ? 1 : 0;
}
}  // namespace

int32_t in_range(Ctx &ctx, int32_t type, int32_t v, int32_t row, int32_t col) { return in_range_body(ctx, type, v, row, col, false); }
int32_t in_range_widest(Ctx &ctx, int32_t type, int32_t v, int32_t row, int32_t col) {
  return in_range_body(ctx, type, v, row, col, true);
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_tile_type(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(tile_type(c, S(a, 0), S(a, 1))); }
void a_tile_byte(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(tile_byte(c, S(a, 0), S(a, 1))); }
void a_tile_flag20(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(tile_flag20(c, S(a, 0), S(a, 1))); }
void a_tile_blocked(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(tile_blocked(c, S(a, 0), S(a, 1))); }
void a_type_at_pos(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(type_at_pos(c, S(a, 0), S(a, 1))); }
void a_overlay_char_at(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(overlay_char_at(c, S(a, 0), S(a, 1))); }
void a_wall_height(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(wall_height(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_object_at(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(object_at(c, S(a, 0), S(a, 1))); }
void a_in_range(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(in_range(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3))); }

const FuncDef kFuncs[] = {
    {"tileType", "default", "int", 2, 0, a_tile_type, &g_negctl_tile_type},
    {"tileByte", "default", "int", 2, 0, a_tile_byte, &g_negctl_tile_byte},
    {"tileFlag20", "default", "int", 2, 0, a_tile_flag20, &g_negctl_tile_flag20},
    {"tileBlocked", "default", "int", 2, 0, a_tile_blocked, &g_negctl_tile_blocked},
    {"typeAtPos", "default", "int", 2, 0, a_type_at_pos, &g_negctl_type_at_pos},
    {"overlayCharAt", "default", "int8", 2, 0, a_overlay_char_at, &g_negctl_overlay_char_at},
    {"wallHeight", "default", "int", 3, 0, a_wall_height, &g_negctl_wall_height},
    {"objectAt", "default", "int", 2, 0, a_object_at, &g_negctl_object_at},
    {"inRange", "default", "int", 4, 0x027F, a_in_range, &g_negctl_in_range},
};
}  // namespace

extern const ModuleDef kModule_tiles = {"tiles", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

// ================================================================================================ P38B: the world size
// The world size's side storage (P38B, 2026-11-19; plan re/roadmap/P38_LARGE_MAPS.md phase B; portsim/sim/world.h).
//
// The GameState's grids hold the original's 50 x 50 world (their offsets are pinned). For N != 50 world_resize maps side blocks:
// the grids sorted by GameState offset are grouped into clusters (a gap above kClusterGap starts a new one), and each cluster is
// ONE zeroed host block at a free virtual address in which grid g's element 0 sits at (offset(g) - cluster lo) * scale + pad.
// scale = ceil(entries at N / entries at 50) + 1 over every grid, so a grid's N-sized block ends before the next grid's
// starts, with at least one 50-sized grid of zero bytes between them (the original's unchecked neighbour reads land there).
// grid_va is then arithmetic: no lookup per access. The path ring and the walker posts are side tables of their own.

namespace {
constexpr uint32_t kClusterGap = 0x40000;   // grids further apart than this go to separate clusters
constexpr uint32_t kSideBase = 0x60000000u;  // the first address tried for the side blocks (64 KB steps after it)
constexpr int32_t kWalkers = static_cast<int32_t>(sizeof(GameState::g_walkers) / sizeof(Walker));

struct GridInfo {
  uint32_t off, entries50, elem;  // offset in the GameState, entries at 50, bytes per entry
};

std::vector<GridInfo> grids() {
  GameState *g = nullptr;  // offsets only
  const auto off = [&](const void *p) { return static_cast<uint32_t>(reinterpret_cast<const uint8_t *>(p) - reinterpret_cast<const uint8_t *>(g)); };
  std::vector<GridInfo> v;
#define SG_G(m) v.push_back({off(&g->m[0]), static_cast<uint32_t>(sizeof(g->m) / sizeof(g->m[0])), static_cast<uint32_t>(sizeof(g->m[0]))})
  SG_G(g_cornerCache); SG_G(g_tilePathCost); SG_G(g_tileFlags); SG_G(g_ballStopCount); SG_G(g_tileTypeSaved);
  SG_G(g_tileLevel); SG_G(g_tileScreenX); SG_G(g_tileScreenY); SG_G(g_tileWallMask); SG_G(g_tileVariant);
  SG_G(g_tileMoodDown); SG_G(g_tileType); SG_G(tile_anim_frame); SG_G(g_tileBuildCode); SG_G(g_pathGrid);
  SG_G(g_tileRefund); SG_G(g_cornerLevel); SG_G(g_tileMoodUp); SG_G(g_tileShadeA); SG_G(g_tileShadeB);
#undef SG_G
  std::sort(v.begin(), v.end(), [](const GridInfo &a, const GridInfo &b) { return a.off < b.off; });
  return v;
}

struct WorldSide {
  Memory *mem = nullptr;
  std::vector<std::vector<uint8_t>> blocks;  // one per cluster, then the ring
  std::vector<uint32_t> bases;
  uint32_t ring_va = 0, ring_entries = 0;
  std::vector<int16_t> posts;  // 2 per walker (x, y) when N > 127
  // no destructor unmapping: the Ctx's Memory is destroyed before its world_side (world_resize unmaps explicitly)
};

WorldSide *side(const Ctx &c) { return static_cast<WorldSide *>(c.world_side.get()); }

// a free address for `size` bytes at or after `from` (Memory::map refuses overlaps)
uint32_t map_free(Memory &m, uint8_t *host, uint32_t size, uint32_t from) {
  for (uint64_t base = from; base + size < 0xF0000000ull; base += 0x10000u)
    if (m.map(static_cast<uint32_t>(base), host, size, false)) return static_cast<uint32_t>(base);
  throw MissingResource{"world_resize: no free address range for the side storage"};
}
}  // namespace

bool world_size_ok(int32_t n) { return n == kWorldOriginal || (n > kWorldOriginal && n <= 256 && (n - 2) % 3 == 0); }

bool world_resize(Ctx &c, int32_t n) {
  if (!world_size_ok(n)) return false;
  if (WorldSide *old = side(c))
    for (uint32_t b : old->bases) c.mem.unmap(b);
  c.world_side.reset();
  c.world_nclusters = 0;
  c.world_scale = 0;
  c.world_n = n;
  if (n != kWorldOriginal) {
    const std::vector<GridInfo> gs = grids();
    uint32_t scale = 1;
    for (const GridInfo &g : gs) {
      const uint32_t need = g.entries50 == kGridVerts ? static_cast<uint32_t>(world_verts(c))
                            : g.entries50 == 8 * kGridTiles ? 8u * static_cast<uint32_t>(world_tiles(c))
                                                           : static_cast<uint32_t>(world_tiles(c));
      scale = std::max(scale, (need + g.entries50 - 1) / g.entries50 + 1);
    }
    auto s = std::make_shared<WorldSide>();
    s->mem = &c.mem;
    const uint32_t pad = 2u * static_cast<uint32_t>(n + 1) * 8u;  // zero bytes before a cluster's first grid
    uint32_t from = kSideBase;
    for (size_t i = 0; i < gs.size();) {
      size_t j = i + 1;
      while (j < gs.size() && gs[j].off - (gs[j - 1].off + gs[j - 1].entries50 * gs[j - 1].elem) < kClusterGap) ++j;
      const uint32_t lo = gs[i].off, hi = gs[j - 1].off + gs[j - 1].entries50 * gs[j - 1].elem;
      const uint32_t size = (hi - lo) * scale + 2 * pad;
      s->blocks.emplace_back(size, 0);
      const uint32_t va = map_free(c.mem, s->blocks.back().data(), size, from);
      s->bases.push_back(va);
      from = va + size + 0x10000u;
      if (c.world_nclusters >= 4) throw MissingResource{"world_resize: more than 4 grid clusters"};
      c.world_clusters[c.world_nclusters++] = Ctx::WorldCluster{lo, hi, va + pad};
      i = j;
    }
    c.world_scale = scale;
    // pathStep's ring: the next power of two >= N * N entries per axis
    uint32_t r = 1;
    while (r < static_cast<uint32_t>(world_tiles(c))) r <<= 1;
    s->ring_entries = r;
    s->blocks.emplace_back(static_cast<size_t>(r) * 2u * 2u, 0);
    s->ring_va = map_free(c.mem, s->blocks.back().data(), r * 4u, from);
    s->bases.push_back(s->ring_va);
    if (n > 127) s->posts.assign(static_cast<size_t>(kWalkers) * 2u, -1);
    c.world_side = s;
  }
  if (has_terrain_api(c)) terrain_api(c).world_resized(n);
  return true;
}

uint32_t world_ring_va(Ctx &c, int axis) {
  if (c.world_n == kWorldOriginal || !side(c))
    return axis == 0 ? state_va(c, &GameState::g_pathQueueX) : state_va(c, &GameState::g_pathQueueY);
  return side(c)->ring_va + (axis == 0 ? 0u : side(c)->ring_entries * 2u);
}
uint32_t world_ring_mask(const Ctx &c) {
  if (c.world_n == kWorldOriginal || !side(c)) return 0x3FF;
  return side(c)->ring_entries - 1;
}

int32_t walker_post(Ctx &c, int32_t walker, int axis) {
  WorldSide *s = side(c);
  if (c.world_n <= 127 || !s || s->posts.empty()) {
    const Ptr<Walker> w = state_arr(c, &GameState::g_walkers) + walker;
    return axis == 0 ? w.field(&Walker::post_x).get() : w.field(&Walker::post_y).get();
  }
  return s->posts[static_cast<size_t>(walker) * 2u + static_cast<size_t>(axis)];
}
void set_walker_post(Ctx &c, int32_t walker, int axis, int32_t v) {
  WorldSide *s = side(c);
  if (c.world_n <= 127 || !s || s->posts.empty()) {
    const Ptr<Walker> w = state_arr(c, &GameState::g_walkers) + walker;
    (axis == 0 ? w.field(&Walker::post_x) : w.field(&Walker::post_y)).set(static_cast<int8_t>(v));
    return;
  }
  s->posts[static_cast<size_t>(walker) * 2u + static_cast<size_t>(axis)] = static_cast<int16_t>(v);
}

}  // namespace portsim
