// The exe side of the Terrain.dll boundary (see portsim/sim/terrain_api.h): the functions that push the course grids
// into the Terrain object and drive its rendering. Phase 3.1a, written from the specification
// (re/analysis/terrain/<addr>_<name>.md, the C3 hook sources shim/src/re/c3dw.cpp / c3ey.cpp / c3ez.cpp / c3ev.cpp as
// reading reference); no original address or code. applyTileVariation and applyTileWalls (backend-gfx rows that no
// stage-B child owns) are ported here because rebuildTileData calls them.
#include <map>
#include <vector>

#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/terrain.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_live.h"
#include "portsim/sim/terrain_state.h"  // P39B: g_defect_path_under_every_type
#include "portsim/sim/tiles.h"
#include "portsim/sim/perf.h"  // P38G: p38g_off (development A/B)
#include "portsim/sim/world.h"  // P38A: grid bases and the world size
#include "crossings.h"  // P39A1: no path band under a two-tile bridge's stream tile

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   applyCourseType      1 = walkable types get collar group 3 instead of 4
//   togglePath           1 = the connected-path re-path sweep skipped, 2 = (P39B) the texture flood does not cross path tiles
//   rebuildTerrainGrid   1 = corners raised one step short
//   applyTileElevation   1 = lowering always through lowerCorner (no blocked-neighbour test)
//   initTerrain          1 = the version flag never set
//   rebuildTileData      1 = tiles pushed only when the elevation changed
//   updateTerrainRender  1 = the unmoved-camera path never rebuilds the tile data
//   applyTileVariation   1 = type 6 does not request normals
//   applyTileWalls       1 = a removed wall keeps its height argument
int g_negctl_apply_course_type = 0;
int g_negctl_toggle_path = 0;
int g_negctl_rebuild_terrain_grid = 0;
int g_negctl_apply_tile_elevation = 0;
int g_negctl_init_terrain = 0;
int g_negctl_rebuild_tile_data = 0;
int g_negctl_update_terrain_render = 0;
int g_negctl_apply_tile_variation = 0;
int g_negctl_apply_tile_walls = 0;

std::map<const Ctx *, TerrainApi *> &apis() {
  static std::map<const Ctx *, TerrainApi *> m;
  return m;
}

// The Terrain object's fields the exe reads directly
constexpr uint32_t kTerWidth = 0x14, kTerHeight = 0x18, kTerTiles = 0x3A4, kTileSize = 0x248;
constexpr uint32_t kTileType = 0x24, kTileHeights = 0x4, kTileWalls = 0x234;

uint32_t terrain_self(Ctx &ctx) { return ctx.gs().gTerrain; }
// the exe's inline Terrain::tileAt: the tile's address, 0 outside the object's width / height
uint32_t tile_at_va(Ctx &ctx, int32_t x, int32_t y) {
  const uint32_t t = terrain_self(ctx);
  const int32_t w = ctx.mem.load<int32_t>(t + kTerWidth);
  if (x >= w || x < 0 || y >= ctx.mem.load<int32_t>(t + kTerHeight) || y < 0) return 0;
  return t + kTerTiles + u32(add32(x, mul32(y, w))) * kTileSize;
}
// the camera-rotation angle every render call builds: 2 -> 90.0f, 4 -> 180.0f, 6 -> -90.0f, else 0 (IEEE patterns)
uint32_t rot_angle(int32_t r) {
  switch (u32(r)) {
    case 2: return 0x42B40000u;
    case 4: return 0x43340000u;
    case 6: return 0xC2B40000u;
    default: return 0;
  }
}
Ptr<uint8_t> need_normals(Ctx &c) { return state_arr(c, &GameState::g_terrain820f2a) + 2; }
Ptr<uint8_t> need_path_render(Ctx &c) { return state_arr(c, &GameState::g_terrain820f2a) + 1; }
uint8_t flag_lo(Ctx &c, int32_t x, int32_t y) {
  return (grid_arr(c, &GameState::g_tileFlags) + tile_index(c, x, y)).as<uint8_t>().get();
}

// P39B layPath.underEveryType (play switch, 1 = the original; the Terrain-state half is in terrain_state.cpp). The flag-0x40 argument
// of layPath / updatePath picks a path tile's texture (0x21 paved where set, 0x20 otherwise: Tile::drawPaths). The original hands it
// TF_FLOOD_REACHED (bit 1 << 6, rebuildTileCoverage's clubhouse flood), which stops at every tee / green / fairway / firm fairway
// tile (walk byte <= 0), so a path past a fairway takes the dirt texture. At 0 the Terrain is handed bit 1 << 6 when TF_FLOOD_REACHED
// is set OR the texture-only flood path_texture_reach (terrain.h; it crosses path tiles of those types) reaches the tile. The flag
// word itself, which decides which buildings are joined to the clubhouse (income), is never written. The flood is computed once per
// outer call (rebuildTileData, rebuildTerrainGrid, togglePath) from the grids as they are at that moment.
thread_local const std::vector<uint8_t> *t_path_reach = nullptr;
struct PathReachScope {
  std::vector<uint8_t> reach;
  bool owner = false;
  explicit PathReachScope(Ctx &ctx) {
    if (SG_DEFECT(g_defect_path_under_every_type) || t_path_reach) return;
    path_texture_reach(ctx, reach, SG_NEGCTL(g_negctl_toggle_path) != 2);
    t_path_reach = &reach;
    owner = true;
  }
  ~PathReachScope() {
    if (owner) t_path_reach = nullptr;
  }
  PathReachScope(const PathReachScope &) = delete;
  PathReachScope &operator=(const PathReachScope &) = delete;
};
// the texture argument for tile (x, y): flag bit 1 << 6, or (switch at 0) the texture flood's answer as that bit
int32_t path_tex(Ctx &c, int32_t x, int32_t y) {
  const int32_t f = flag_lo(c, x, y) & (1 << 6);
  if (f || !t_path_reach || x < 0 || y < 0 || x >= world_n(c) || y >= world_n(c)) return f;
  return (*t_path_reach)[static_cast<size_t>(x) * static_cast<size_t>(world_n(c)) + static_cast<size_t>(y)] ? (1 << 6) : 0;
}
// the collar table passCollarInfo receives (applyCourseType, initTerrain): each type's collar group, 4 for every walkable
// type other than 5 and 7 when the course type is 1
void collar_table(Ctx &ctx, int32_t table[23], bool wrong) {
  const uint8_t mode = static_cast<uint8_t>(ctx.gs().g_courseType);
  for (int32_t t = 0; t < 23; ++t) {
    const Ptr<TileTypeAttr> a = state_arr(ctx, &GameState::g_tileTypeAttrs) + t;
    table[t] = a.field(&TileTypeAttr::collar).get();
    if (mode == 1 && a.field(&TileTypeAttr::walk).get() > 0 && t != 5 && t != 7) table[t] = wrong ? 3 : 4;
  }
}
}  // namespace

void set_terrain_api(Ctx &ctx, TerrainApi *api) {
  if (api) apis()[&ctx] = api;
  else apis().erase(&ctx);
}
bool has_terrain_api(Ctx &ctx) { return apis().count(&ctx) != 0; }

// syncTiles.marinaAnchorLand (P35F, play-test 17 item 8: "marina should be placed all above water, currently one tile has grass").
// evalPlacementArea refuses a Marina (build item 12) on course types 0 and 2 unless every footprint tile is water (type 0x11), but
// stampCourseTiles then writes 0x11 back to every footprint tile except the anchor (c == 0, r == 0), which keeps the building type
// 0x16; the Terrain draws a type-0x16 tile with the land texture, so one grass tile shows under the pier (the original draws it
// so: log/roadmap/P35/P35F_notes.md). 1 = the original. 0: the Terrain is handed type 0x11 for that anchor tile, so it draws
// water there like the other three; the course grids keep 0x16 (the placed-object pass, which draws the marina from the anchor,
// the ball, golfers, walkers, the hover text and saves see what the original sees). RENDER-ONLY: only the type value passed to
// Terrain::setType, and the compare against the Terrain tile's type before it, change.
int g_defect_marina_anchor_land = 1;
int g_defect_path_under_every_type = 1;   // P39B layPath.underEveryType (terrain_state.h; path_tex above)
int32_t terrain_tile_type(Ctx &ctx, int32_t x, int32_t y) { return terrain_draw_type(ctx, x, y, tile_type(ctx, x, y)); }
int32_t terrain_draw_type(Ctx &ctx, int32_t x, int32_t y, int32_t t) {
  if (SG_DEFECT(g_defect_marina_anchor_land) || t != 0x16) return t;
  const int8_t course = ctx.gs().g_courseType;
  if (course != 0 && course != 2) return t;  // stampCourseTiles' and evalPlacementArea's water rule for item 12
  // the anchor's flag word carries the build item + 1 in its low 5 bits (stampCourseTiles); 12 = Marina
  const uint16_t w = (grid_arr(ctx, &GameState::g_tileFlags) + tile_index(ctx, x, y)).get();
  return (w & ((1u << 5) - 1)) == 12 + 1 ? 0x11 : t;
}
TerrainApi &terrain_api(Ctx &ctx) {
  const auto it = apis().find(&ctx);
  if (it == apis().end()) throw MissingResource{"TerrainApi (Terrain.dll boundary) not attached"};
  return *it->second;
}

int32_t apply_course_type(Ctx &ctx, int32_t arg) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  int32_t table[23];
  collar_table(ctx, table, SG_NEGCTL(g_negctl_apply_course_type) == 1);
  api().pass_collar_info(terrain_self(ctx), table, 0x17);
  return i32(api().load_new_course_type(terrain_self(ctx), arg));
}

namespace {
// P39A1 (play switch scenicBridge.twoTile = 0): a stream tile (0x0a) under a two-tile bridge keeps its path bit for the walkers and
// the game, but Terrain lays no path band there (the deck is drawn over it); every other tile reads tileFlag20 as before
int32_t laid_flag20(Ctx &ctx, int32_t x, int32_t y) {
  const int32_t f = tile_flag20(ctx, x, y);
  if (f == 0) return f;
  if (crossings_on() && tile_type(ctx, x, y) == 0x0A && crossing_at(ctx, x, y) >= 0) return 0;
  if (gap_bridges_on() && crossing_gap_at(ctx, x, y) >= 0) return 0;   // P39A3: a gap bridge's tiles: the deck is drawn instead
  return f;
}
// every tile with tileFlag20 is re-pathed, then the path-render byte is set
void update_all_paths(Ctx &ctx) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  for (int32_t xx = 0; xx < world_n(ctx); ++xx)
    for (int32_t yy = 0; yy < world_n(ctx); ++yy)
      if (laid_flag20(ctx, xx, yy) != 0) api().update_path(terrain_self(ctx), xx, yy, path_tex(ctx, xx, yy));  // P39B: flag_lo & 0x40 at 1
  need_path_render(ctx).set(1);
}
}  // namespace

int32_t toggle_path(Ctx &ctx, uint32_t tile, int32_t x, int32_t y) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  const uint32_t self = terrain_self(ctx);
  const PathReachScope reach(ctx);   // P39B: nothing at layPath.underEveryType = 1
  if (laid_flag20(ctx, x, y) != 0) {
    if (!(api().has_path(self, tile) & 0xFF)) {
      if (path_tex(ctx, x, y)) update_all_paths(ctx);   // P39B: flag_lo & 0x40 at 1
      const int32_t b40 = path_tex(ctx, x, y);
      const int32_t f = laid_flag20(ctx, x, y);
      return i32((api().lay_path(self, tile, f, b40) & ~0xFFu) | 1u);
    }
  }
  const int32_t f = laid_flag20(ctx, x, y);
  if (f != 0) return i32(u32(f) & ~0xFFu);
  const uint32_t hp = api().has_path(self, tile);
  if (!(hp & 0xFF)) return i32(hp & ~0xFFu);
  if ((api().has_connected_path(self, x, y) & 0xFF) && SG_NEGCTL(g_negctl_toggle_path) != 1) update_all_paths(ctx);
  const int32_t b40 = path_tex(ctx, x, y);   // P39B: flag_lo & 0x40 at 1
  const int32_t f2 = laid_flag20(ctx, x, y);
  return i32((api().lay_path(self, tile, f2, b40) & ~0xFFu) | 1u);
}

int32_t rebuild_terrain_grid(Ctx &ctx) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  const uint32_t self = terrain_self(ctx);
  const PathReachScope reach(ctx);   // P39B: nothing at layPath.underEveryType = 1
  for (int32_t x = world_last(ctx); x >= 0; --x) {
    api().pump_input();
    for (int32_t y = world_last(ctx); y >= 0; --y) {
      const uint32_t tile = tile_at_va(ctx, x, y);
      const int32_t var = tile_byte(ctx, x, y);
      const int32_t type = terrain_tile_type(ctx, x, y);  // P35F syncTiles.marinaAnchorLand
      api().set_type(self, tile, type, var);
    }
  }
  const int32_t short_by = SG_NEGCTL(g_negctl_rebuild_terrain_grid) == 1 ? 1 : 0;
  for (int32_t x = world_last(ctx); x >= 0; --x) {
    api().pump_input();
    for (int32_t y = world_last(ctx); y >= 0; --y) {
      if (tile_type(ctx, x, y) == 0x11) continue;
      for (int32_t c = 1; c < 8; c += 2) {
        if (corner_height_at(ctx, x, y, c) <= 3) continue;
        int32_t n = 0;
        if (corner_height_at(ctx, x, y, c) - 3 <= 0) continue;
        do {
          api().elevate_corner(self, tile_at_va(ctx, x, y), c);
          ++n;
        } while (n < corner_height_at(ctx, x, y, c) - 3 - short_by);
      }
    }
  }
  for (int32_t x = world_last(ctx); x >= 0; --x)
    for (int32_t y = world_last(ctx); y >= 0; --y) api().calc_normals(self, tile_at_va(ctx, x, y));
  for (int32_t x = world_last(ctx); x >= 0; --x)
    for (int32_t y = world_last(ctx); y >= 0; --y)
      for (int32_t c = 0; c < 8; c += 2) {
        if (!wall_height(ctx, x, y, c)) continue;
        const uint32_t tile = tile_at_va(ctx, x, y);
        const int32_t h = wall_height(ctx, x, y, c);
        api().set_wall(self, tile, c, h, 1);
      }
  int32_t r = 0;
  for (int32_t x = world_last(ctx); x >= 0; --x)
    for (int32_t y = world_last(ctx); y >= 0; --y) {
      r = laid_flag20(ctx, x, y);
      if (!r) continue;
      const uint32_t tile = tile_at_va(ctx, x, y);
      const int32_t f40 = path_tex(ctx, x, y);   // P39B: flag_lo & 0x40 at 1
      const int32_t f20 = laid_flag20(ctx, x, y);
      r = i32(api().lay_path(self, tile, f20, f40));
    }
  return r;
}

namespace {
// is any of the eight neighbours blocked? (the original's order; the first nonzero answer stops the test)
bool neighbour_blocked(Ctx &ctx, int32_t x, int32_t y) {
  return tile_blocked(ctx, x - 1, y) || tile_blocked(ctx, x, y - 1) || tile_blocked(ctx, x + 1, y) ||
         tile_blocked(ctx, x, y + 1) || tile_blocked(ctx, x - 1, y - 1) || tile_blocked(ctx, x + 1, y - 1) ||
         tile_blocked(ctx, x + 1, y + 1) || tile_blocked(ctx, x - 1, y + 1);
}
void lower_step(Ctx &ctx, uint32_t tile, int32_t x, int32_t y, int32_t c, int32_t x2, int32_t y2) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  if (SG_NEGCTL(g_negctl_apply_tile_elevation) == 1 || !neighbour_blocked(ctx, x, y)) {
    api().lower_corner(terrain_self(ctx), tile, c);
    return;
  }
  const uint32_t angle = rot_angle(ctx.gs().g_cameraRot);
  api().lower_edge_corner(terrain_self(ctx), tile, c, tile_at_va(ctx, x2, y2), angle);
}
}  // namespace

int32_t apply_tile_elevation(Ctx &ctx, uint32_t tile, int32_t x, int32_t y, int32_t x2, int32_t y2) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  uint8_t res = 0;
  int32_t q = 0;
  for (int32_t c = 1, k = 0; c < 8; c += 2, ++k) {
    const uint32_t off = kTileHeights + 8 * u32(k);
    const auto height = [&] { return tile != 0 ? ctx.mem.load<int32_t>(tile + off) : 0; };
    int32_t h = height();
    q = corner_height_at(ctx, x, y, c) - 3;
    if (q > h) {
      api().elevate_corner(terrain_self(ctx), tile, c);
      need_normals(ctx).set(1);
      res = 1;
      for (;;) {
        h = height();
        q = corner_height_at(ctx, x, y, c) - 3;
        if (q <= h) break;
        api().elevate_corner(terrain_self(ctx), tile, c);
      }
      continue;
    }
    h = height();
    q = corner_height_at(ctx, x, y, c) - 3;
    if (q >= h) continue;
    lower_step(ctx, tile, x, y, c, x2, y2);
    need_normals(ctx).set(1);
    res = 1;
    for (;;) {
      h = height();
      q = corner_height_at(ctx, x, y, c) - 3;
      if (q >= h) break;
      lower_step(ctx, tile, x, y, c, x2, y2);
    }
  }
  return i32((u32(q) & ~0xFFu) | res);
}

int32_t apply_tile_variation(Ctx &ctx, uint32_t tile, int32_t x, int32_t y) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  const int32_t cur = ctx.mem.load<int32_t>(tile + kTileType);  // no NULL test
  if (terrain_tile_type(ctx, x, y) == cur) {  // P35F syncTiles.marinaAnchorLand
    const uint32_t v = api().get_variation(terrain_self(ctx), tile);
    const int32_t r = tile_byte(ctx, x, y);
    if (u32(r) == v) return i32(u32(r) & ~0xFFu);
  }
  if (tile_type(ctx, x, y) == 7 || ctx.mem.load<int32_t>(tile + kTileType) == 7 ||
      (SG_NEGCTL(g_negctl_apply_tile_variation) != 1 &&
       (tile_type(ctx, x, y) == 6 || ctx.mem.load<int32_t>(tile + kTileType) == 6)))
    need_normals(ctx).set(1);
  const int32_t b = tile_byte(ctx, x, y);
  const int32_t t = terrain_tile_type(ctx, x, y);  // P35F syncTiles.marinaAnchorLand
  return i32((api().set_type(terrain_self(ctx), tile, t, b) & ~0xFFu) | 1u);
}

int32_t apply_tile_walls(Ctx &ctx, uint32_t tile, int32_t x, int32_t y) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  int32_t any = 0;
  uint32_t eax = 0;  // the original's eax: al is the result, the upper bytes the last wallHeight / setWall result
  const auto wall = [&](int32_t s) { return ctx.mem.load<uint8_t>(tile + kTileWalls + u32(s)); };
  for (int32_t s = 0; s < 8; s += 2) {
    eax = u32(wall_height(ctx, x, y, s));
    if (eax != 0) {
      const uint8_t wb = wall(s);
      eax = (eax & ~0xFFu) | wb;
      if (wb == 0) {
        const int32_t h = wall_height(ctx, x, y, s);
        eax = api().set_wall(terrain_self(ctx), tile, s, h, 1);
        any = 1;
        continue;
      }
    }
    eax = u32(wall_height(ctx, x, y, s));
    if (eax != 0) continue;
    const uint8_t wb = wall(s);
    eax = (eax & ~0xFFu) | wb;
    if (wb == 0) continue;
    eax = api().set_wall(terrain_self(ctx), tile, s, SG_NEGCTL(g_negctl_apply_tile_walls) == 1 ? 1 : 0, 0);
    any = 1;
  }
  return i32((eax & ~0xFFu) | u32(any));
}

namespace {
// std::list push of `tile` onto the render list gTileBuffer {+4 head node, +8 size}; the node comes from the engine's
// allocator
void push_tile(Ctx &ctx, uint32_t tile) {
  const Memory &m = ctx.mem;
  const uint32_t list = state_va(ctx, &GameState::gTileBuffer);
  const uint32_t head = m.load<uint32_t>(list + 4);
  const uint32_t at = m.load<uint32_t>(head);
  const uint32_t prev = m.load<uint32_t>(at + 4);
  const uint32_t node = terrain_api(ctx).operator_new(0xC);
  m.store<uint32_t>(node, at != 0 ? at : node);
  m.store<uint32_t>(node + 4, prev != 0 ? prev : node);
  m.store<uint32_t>(at + 4, node);
  m.store<uint32_t>(m.load<uint32_t>(node + 4), node);
  if (node + 8 != 0) m.store<uint32_t>(node + 8, tile);
  m.store<int32_t>(list + 8, add32(m.load<int32_t>(list + 8), 1));
}
}  // namespace

// P13D item 5 (log/roadmap/P13/P13D_notes.md): updateTerrainRender.syncTilesAfterEdit. The Terrain object's tiles (type and
// variation, corner heights, walls, paths) are brought in line with the course grids only by rebuildTileData, which the unmoved-camera
// path of updateTerrainRender runs while bit 1 of g_tileWallMask[0] is clear; rebuildHeightfield (which recomputes the height grid
// g_tileLevel from the tile types) clears it at every course edit. A loaded course keeps the grid it was loaded with until its first
// edit, so the first placement recomputes the heights and re-syncs every tile from 4 / zoom * 13 columns and rows before the camera
// to the map's far edges, and the picture changes far from the placed object (Monterey's river banks and ponds: measured in the
// original too, P13D_notes.md). Fixed (0): the first unmoved-camera frame after the terrain is initialised or repainted from scratch
// runs rebuildHeightfield and then rebuildTileData over the whole map (rows and columns from 0) once and, when tiles changed, draws
// one full render in place of their local renders, so the course shows its edited look from the start and the first edit changes
// only what it touches. At 1 nothing changes.
int g_defect_terrain_sync_after_edit = 1;
namespace {
thread_local bool t_sync_pending = false;   // a fresh terrain picture is waiting for its whole-map sync (switch at 0 only)
void rebuild_tile_data_impl(Ctx &ctx, int32_t x0, int32_t y0, bool whole);
}  // namespace

void rebuild_tile_data(Ctx &ctx, int32_t x0, int32_t y0) { rebuild_tile_data_impl(ctx, x0, y0, false); }

namespace {
void rebuild_tile_data_impl(Ctx &ctx, int32_t x0, int32_t y0, bool whole) {
  const PathReachScope reach(ctx);   // P39B: one texture flood for every togglePath of the pass (nothing at the switch's 1)
  // P11A: zoom 8 (mainLoop.zoomInStopsAt4 = 0, the only way it arrives) takes zoom 4's 13: 4 / 8 = 0 would start at the camera's
  // own row and column and leave the tiles above and left of it out
  const int32_t zq = ctx.gs().g_zoomScale > 4 ? 4 : ctx.gs().g_zoomScale;
  const int32_t N = world_n(ctx);  // the grid side (50 in the original)
  const int32_t d = whole ? N : div32(4, zq) * 13;   // P13D item 5: whole = from row and column 0
  int32_t x = sub32(x0, d);
  if (x >= N) return;
  for (;;) {
    if (x < 0) x = 0;
    int32_t y = sub32(y0, whole ? N : div32(4, zq) * 13);
    if (!(y >= N)) {
      for (;;) {
        if (y < 0) y = 0;
        if (tile_blocked(ctx, x, y) == 0) {
          const uint32_t tile = tile_at_va(ctx, x, y);
          const uint8_t var = static_cast<uint8_t>(apply_tile_variation(ctx, tile, x, y));
          const uint8_t ele = static_cast<uint8_t>(apply_tile_elevation(ctx, tile, x, y, x0, y0));
          const uint8_t wal = static_cast<uint8_t>(apply_tile_walls(ctx, tile, x, y));
          const uint8_t pth = static_cast<uint8_t>(toggle_path(ctx, tile, x, y));
          const uint8_t any = SG_NEGCTL(g_negctl_rebuild_tile_data) == 1 ? ele : static_cast<uint8_t>(var | ele | wal | pth);
          if (any != 0) push_tile(ctx, tile);
        }
        if (!(++y < N)) break;
      }
    }
    if (!(++x < N)) break;
  }
}
}  // namespace

int32_t init_terrain(Ctx &ctx, int32_t w, int32_t h) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  GameState &g = ctx.gs();
  if (api().os_major_version() >= 5 && SG_NEGCTL(g_negctl_init_terrain) != 1) g.g_terrain820f28 = 1;
  g.g_terrain820f30[0] = w;
  g.g_terrain820f30[1] = h;
  int32_t table[23];
  collar_table(ctx, table, false);
  api().pass_collar_info(terrain_self(ctx), table, 0x17);
  const uint32_t flag = g.g_terrain820f28;
  const uint32_t hdc = api().acquire_dc();
  api().init_system(terrain_self(ctx), w, h, hdc, i32(flag));
  api().init_terrain(terrain_self(ctx));
  return rebuild_terrain_grid(ctx);
}

namespace {
int32_t update_terrain_render_body(Ctx &ctx, int32_t x, int32_t y, int32_t zoom, int32_t rot);
}
// The frame bracket for the drawing (roadmap S3.2, terrain_live.h): the body's every exit returns through here.
int32_t update_terrain_render(Ctx &ctx, int32_t x, int32_t y, int32_t zoom, int32_t rot) {
  terrain_live_frame(ctx, true);
  const int32_t r = update_terrain_render_body(ctx, x, y, zoom, rot);
  terrain_live_frame(ctx, false);
  return r;
}
namespace {
int32_t update_terrain_render_body(Ctx &ctx, int32_t x, int32_t y, int32_t zoom, int32_t rot) {
  const auto api = [&ctx]() -> TerrainApi & { return terrain_api(ctx); };  // looked up per call: a path that calls no Terrain method needs none
  GameState &g = ctx.gs();
  const uint32_t self = terrain_self(ctx);
  const auto store_camera = [&] {
    g.g_terrainLastCamX = x;
    g.g_terrainLastCamY = y;
  };
  const bool first = g.g_terrainInitPending != 0;
  if (first || g.g_terrain820f2a[0] != 0) {
    if (first) init_terrain(ctx, g.g_screenWidth, g.g_screenHeight);
    api().window_paint();
    api().render(self, tile_at_va(ctx, x, y), rot_angle(rot));
    g.g_terrain820f2a[0] = 0;
    g.g_terrainInitPending = 0;
    t_sync_pending = !SG_DEFECT(g_defect_terrain_sync_after_edit);   // P13D item 5
    g.g_terrainLastCamX = x;
    g.g_terrainLastCamY = y;
    return i32(api().blit_full());
  }
  const int32_t dx = sub32(x, g.g_terrainLastCamX);
  const int32_t dy = sub32(y, g.g_terrainLastCamY);
  if (rot != g.g_terrain820f30[2]) {
    api().window_paint();
    api().render(self, tile_at_va(ctx, x, y), rot_angle(rot));
    const uint32_t r = api().blit_full();
    store_camera();
    g.g_terrain820f30[2] = rot;
    return i32(r);
  }
  if (zoom != g.g_terrainLastZoom) {
    api().set_zoom_level(self, zoom);
    api().window_paint();
    api().render(self, tile_at_va(ctx, x, y), rot_angle(rot));
    const uint32_t r = api().blit_full();
    store_camera();
    g.g_terrainLastZoom = zoom;
    return i32(r);
  }
  if (dy == 0 && dx == 0) {
    api().blit_screen(g.g_terrain820f30[0], g.g_terrain820f30[1]);
    // P13D item 5 (updateTerrainRender.syncTilesAfterEdit = 0): the whole map once after a fresh terrain picture
    const bool whole = t_sync_pending && !SG_DEFECT(g_defect_terrain_sync_after_edit);
    t_sync_pending = false;
    if (whole || (!(grid_ref(ctx, &GameState::g_tileWallMask, 0) & 2) && SG_NEGCTL(g_negctl_update_terrain_render) != 1)) {
      if (whole) rebuild_heightfield(ctx);   // the height grid as every course edit recomputes it (terrain.cpp)
      {
        struct SyncFlag {   // P38G (terrain_live.h): the edge passes of the whole-map sync are repainted below
          explicit SyncFlag(bool on) { t_terrain_sync_repaints = on; }
          ~SyncFlag() { t_terrain_sync_repaints = false; }
        } sync_flag(whole && !p38g_off());
        rebuild_tile_data_impl(ctx, x, y, whole);
      }
      int8_t &wm0 = grid_ref(ctx, &GameState::g_tileWallMask, 0);  // the absolute byte 0 (bit 1: tiles synced)
      wm0 = static_cast<int8_t>(wm0 | 2);
    }
    const uint32_t list = state_va(ctx, &GameState::gTileBuffer);
    if (ctx.mem.load<int32_t>(list + 8) == 0) return 0;
    if (whole) {   // one full render of the synced terrain (as the repaint-from-scratch path draws it) instead of a local render per tile
      if (need_normals(ctx).get() != 0) {
        api().calc_all_normals(self, tile_at_va(ctx, x, y));
        need_normals(ctx).set(0);
      }
      const Memory &m = ctx.mem;
      const uint32_t head = m.load<uint32_t>(list + 4);
      for (uint32_t node = m.load<uint32_t>(head); node != head;) {   // the list emptied as below
        const uint32_t cur = node;
        node = m.load<uint32_t>(cur);
        const uint32_t next = m.load<uint32_t>(cur), prev = m.load<uint32_t>(cur + 4);
        m.store<uint32_t>(prev, next);
        m.store<uint32_t>(next + 4, prev);
        m.store<int32_t>(list + 8, sub32(m.load<int32_t>(list + 8), 1));
      }
      g.g_terrainListCursor = head;
      need_path_render(ctx).set(0);
      api().window_paint();
      api().render(self, tile_at_va(ctx, x, y), rot_angle(rot));
      return i32(api().blit_full());
    }
    if (need_normals(ctx).get() != 0) {
      api().calc_all_normals(self, tile_at_va(ctx, x, y));
      need_normals(ctx).set(0);
    }
    const Memory &m = ctx.mem;
    uint32_t head = m.load<uint32_t>(list + 4);
    uint32_t it = m.load<uint32_t>(head);
    g.g_terrainListCursor = it;
    if (it != head) {
      do {
        api().local_render(self, m.load<uint32_t>(it + 8), tile_at_va(ctx, x, y), rot_angle(rot));
        it = m.load<uint32_t>(it);
        g.g_terrainListCursor = it;
      } while (it != m.load<uint32_t>(list + 4));
    }
    // listEraseRange(list, out, head->next, head): every node unlinked, the size dropped (operator delete: no heap
    // model, U-0823); its out slot is the zoom argument's, which nothing reads afterwards
    head = m.load<uint32_t>(list + 4);
    for (uint32_t node = m.load<uint32_t>(head); node != head;) {
      const uint32_t cur = node;
      node = m.load<uint32_t>(cur);
      const uint32_t next = m.load<uint32_t>(cur), prev = m.load<uint32_t>(cur + 4);
      m.store<uint32_t>(prev, next);
      m.store<uint32_t>(next + 4, prev);
      m.store<int32_t>(list + 8, sub32(m.load<int32_t>(list + 8), 1));
    }
    if (need_path_render(ctx).get() != 0) {
      api().path_update_render(self, tile_at_va(ctx, x, y), rot_angle(rot));
      need_path_render(ctx).set(0);
    }
    return i32(api().blit_full());
  }
  api().window_paint();
  // P11A: zoom 8 (mainLoop.zoomInStopsAt4 = 0, the only way it arrives) renders every camera move whole: renderTerrainStrip's
  // windows and shifted copies are sized for the zoom shifts of levels 1 / 2 / 4
  if (add32(abs32(dy), abs32(dx)) > 2 || zoom > 4)
    api().render(self, tile_at_va(ctx, x, y), rot_angle(rot));
  else
    api().render_strip(x, y, dx, dy, rot, zoom);
  const uint32_t r = api().blit_full();
  store_camera();
  return i32(r);
}
}  // namespace

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_apply_course_type(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(apply_course_type(c, S(a, 0))); }
void a_toggle_path(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(toggle_path(c, a[0], S(a, 1), S(a, 2))); }
void a_rebuild_terrain_grid(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(rebuild_terrain_grid(c)); }
void a_apply_tile_elevation(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(apply_tile_elevation(c, a[0], S(a, 1), S(a, 2), S(a, 3), S(a, 4)));
}
void a_init_terrain(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(init_terrain(c, S(a, 0), S(a, 1))); }
void a_rebuild_tile_data(Ctx &c, const uint32_t *a, Ret &) { rebuild_tile_data(c, S(a, 0), S(a, 1)); }
void a_update_terrain_render(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(update_terrain_render(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3)));
}
void a_apply_tile_variation(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(apply_tile_variation(c, a[0], S(a, 1), S(a, 2))); }
void a_apply_tile_walls(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(apply_tile_walls(c, a[0], S(a, 1), S(a, 2))); }

const FuncDef kFuncs[] = {
    {"applyCourseType", "default", "int", 1, 0, a_apply_course_type, &g_negctl_apply_course_type},
    {"togglePath", "default", "int", 3, 0, a_toggle_path, &g_negctl_toggle_path},
    {"rebuildTerrainGrid", "default", "int", 0, 0, a_rebuild_terrain_grid, &g_negctl_rebuild_terrain_grid},
    {"applyTileElevation", "default", "int", 5, 0, a_apply_tile_elevation, &g_negctl_apply_tile_elevation},
    {"initTerrain", "default", "int", 2, 0, a_init_terrain, &g_negctl_init_terrain},
    {"rebuildTileData", "default", "void", 2, 0, a_rebuild_tile_data, &g_negctl_rebuild_tile_data},
    {"updateTerrainRender", "default", "int", 4, 0, a_update_terrain_render, &g_negctl_update_terrain_render},
    {"applyTileVariation", "default", "int", 3, 0, a_apply_tile_variation, &g_negctl_apply_tile_variation},
    {"applyTileWalls", "default", "uint8", 3, 0, a_apply_tile_walls, &g_negctl_apply_tile_walls},
};
// P13D item 5: the module's play switch (re/port/play_profile.tsv)
const DefectDef kDefects[] = {
    {"updateTerrainRender.syncTilesAfterEdit", "P13D",
     "the Terrain tiles (types, heights, walls, paths) are synced with the course grids only after a course edit, so the first "
     "placement changes the terrain far from it (river banks, ponds); fixed: the whole map is synced once when the terrain is "
     "initialised or repainted from scratch, then one full render",
     &g_defect_terrain_sync_after_edit},
    {"syncTiles.marinaAnchorLand", "P35F",
     "a Marina must stand on four water tiles (course types 0 and 2) but its anchor tile keeps the building type, which the "
     "Terrain draws as grass under the pier; fixed: the Terrain is handed water for that tile (render-only, the course grids "
     "keep the building type)",
     &g_defect_marina_anchor_land},
    {"layPath.underEveryType", "P39B",
     "a path is not drawn on tee, green, fairway, firm fairway, sand, pot bunker and large-building tiles, and the paved texture "
     "of a path joined to the clubhouse stops at the first tee / green / fairway tile it crosses; fixed: the path is drawn on "
     "every type except tee and green (it stops at their edge) and the paved texture follows the path across them (drawing "
     "only: the flood that joins buildings to the clubhouse is unchanged)",
     &g_defect_path_under_every_type},
};
}  // namespace

extern const ModuleDef kModule_terrain_dll = {"terrain_dll", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                              sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
