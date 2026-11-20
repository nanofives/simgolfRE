// The port's terrain renderer (phase 5 step 3; plan re/PHASE5_TERRAIN_PLAN.md): Terrain::render and the tile drawing it
// reaches, issuing the original's GL calls through a GlSink. Written from the specification, with the matched sources
// re/match/terrain_render.cpp, terrain_draw.cpp, terrain_culled.cpp, terrain_skirts.cpp, terrain_walls.cpp,
// terrain_paths.cpp and terrain_corneroverlay.cpp as reading reference. Tests: port/test/test_terrain_render.py, which
// replays recorded passes of the original (re/tools/gl_corpus.py) and compares the call streams line by line.
//
// The draw-list passes (roadmap step S3.1): localRender, stripRender and pathUpdateRender queue tiles with drawTile into the
// Terrain object's draw list (sorted back to front for the camera angle), follow walls with drawTileObjects, then render the
// list with Tile::render and clear it. Reading reference re/match/terrain_localrender.cpp, terrain_drawtile.cpp,
// terrain_strip.cpp, terrain_pathrender.cpp; evidence: kind-3 / kind-4 / kind-1 passes of the GL corpora
// (port/test/test_terrain_render.py).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/gl_sink.h"
#include "portsim/sim/terrain.h"  // P39A2: path_stairs_axes (layPath.stairs)
#include "portsim/sim/terrain_render.h"
#include "portsim/sim/terrain_state.h"  // P38G: terrain_draw_list_offset
#include "portsim/sim/world.h"  // P38A: grid bases and the world size

namespace portsim {

extern int g_defect_fairway_stripes_fixed;  // tick.cpp Tile::render.fairwayStripesFixed (P13A item 8)

namespace {

// Negative controls (deliberately wrong bodies, switched with terrain_render_negctl; 0 = the correct body):
//   1 drawSkirts passes its depth on unchanged        2 Tile::render binds the texture of every face
//   3 isCulled swaps its 90 and 180 degree tests      4 drawWalls swaps the two fence textures
//   5 drawPaths uses a half-width of 16.0             6 Terrain::render walks x upward at angle 0
//   7 drawCornerOverlays swaps masks 5 and 9          8 Tile::render rotates type-7 textures one quadrant too far
//   9 the draw list is rendered in reverse order       10 the draw list's second tile is not rendered
//  11 stripRender's outer loops start one row later   12 pathUpdateRender clips its left edge at x (not x + 1)
//  13 drawTileObjects follows no wall                 14 drawTile keeps duplicates (no same-tile test)
//  15 renderTileAtFiveOffsets draws nothing           16 renderTileAtFiveOffsets' second offset -10 (not -20)
//  17 enableBlackLight1 gives light 1 the ambient (0.2, 0.2, 0.2, 1) (GlRaster models light 0 only: blind in its pixels)
//  18 renderTileAtFiveOffsets swaps its 90 and 180 degree neighbour sets
//  19 a turned fairway tile's piece-2 faces draw piece 0 (P13A fix 6; Tile::render.fairwayStripesFixed = 0 only)
//  20 a stairs span is one tread (not one step per level; P39A2, layPath.stairs = 0 only)
//  21 a stairs span is drawn near to far (P39A2, layPath.stairs = 0 only)
//  22 a stairs face turned away from the camera is drawn too (P39A2's first version: it covered the treads)
int32_t g_negctl_terrain_render = 0;
int32_t g_cull_margin = 0;   // P8B: terrain_render_set_cull_margin
int32_t g_world_reach = 0, g_world_cull_y = 0;   // P35D: terrain_render_set_world_reach

// Terrain object fields
constexpr uint32_t kTerWidth = 0x14, kTerHeight = 0x18, kTerZoom = 0x1C, kTerScreenW = 0x20, kTerScreenH = 0x24;
constexpr uint32_t kTerTiles = 0x3A4, kTileSize = 0x248;
// Tile fields
constexpr uint32_t kTileHeight0 = 0x04, kTileHeight1 = 0x0C, kTileHeight2 = 0x14, kTileHeight3 = 0x1C;
constexpr uint32_t kTileType = 0x24, kTileRotation = 0x28, kTileX = 0x2C, kTileY = 0x30, kTileNeighbours = 0x34;
constexpr uint32_t kTileFaceCount = 0x44, kTileFaces = 0x48, kFaceSize = 0x38;
constexpr uint32_t kFaceVertex = 0x00, kFaceUv = 0x0C, kFaceTexture = 0x24, kFaceSlot = 0x28;
constexpr uint32_t kTileHasPath = 0x208, kTileConnected = 0x209, kTileDrawPath = 0x20A;
constexpr uint32_t kTileWallKind = 0x210, kTileWallOn = 0x234, kTileVariation = 0x240;
// the draw list (a std::list<Tile*>) after the tile array: +0 allocator, +4 head node, +8 size (+0x164ac4 / +0x164ac8 / +0x164acc at
// 50). P38G: at the context's N (terrain_state.h terrain_draw_list_offset); the fixed 50 offset read tile 2,500's data at N = 152, so
// begin_pass threw and every strip, local and path-update pass was dropped (edge scrolling showed a new picture ~4 times a second)
inline uint32_t ter_draw_list_size(const Ctx &c) { return terrain_draw_list_offset(c) + 8u; }
constexpr int32_t kTypeHidden = 0x14, kTypeRotated = 7;
constexpr uint32_t kTextureVariations = 0x19, kTextureSlots = 9;   // textures[type][variation][slot], 0x25 types

class Renderer {
 public:
  Renderer(Ctx &c, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain) : c_(c), gl_(gl), g_(g), terrain_(terrain) {}
  // P38A: the grid's side N (the DLL's 50 / 49 / 25 = N, N - 1, N / 2) and the translation that puts tile t's corner at the origin:
  // (N / 2 - t) * 100, the DLL's float(25 - t) * 100 (equal at 50: both exact)
  int32_t n() const { return world_n(c_); }
  float centre_shift(int32_t t) const { return static_cast<float>(world_n(c_)) * 0.5f - static_cast<float>(t); }

  // Terrain::render(pTile, angle)
  bool render(uint32_t ptile, float angle) {
    int32_t zoom = i32(terrain_ + kTerZoom);
    int32_t up, down, left, right;
    up = down = left = right = 16 << zoom;
    gl_.load_identity();
    gl_.push_matrix();
    gl_.rotated(static_cast<double>(g_.screen_scale), 1.0, 0.0, 0.0);
    gl_.rotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    if (ptile != 0) gl_.translatef(centre_shift(tile_x(ptile)) * 100.0f, 0.0f, centre_shift(tile_y(ptile)) * 100.0f);
    set_view_angle(angle);

    if (ptile == 0) {
      const int32_t c0 = n() / 2;   // the 30 x 30 window around the centre: 10 .. 40 at 50
      for (int32_t yn = c0 - 15; yn < c0 + 15; ++yn)
        for (int32_t xn = c0 + 15; xn > c0 - 15; --xn) tile_render(tile_at(xn, yn));
    } else {
      const int32_t px = tile_x(ptile), py = tile_y(ptile);
      // P35D: a world-layer pass walks g_world_reach more tiles each way (shifted by the zoom field like the 16), before the map clamps
      up += g_world_reach << zoom;
      down += g_world_reach << zoom;
      left += g_world_reach << zoom;
      right += g_world_reach << zoom;
      if (py < up) up = py;
      if (n() - py < down) down = n() - py;
      if (px < left) left = px;
      if (n() - px < right) right = n() - px;
      uint32_t t;
      if (angle == 0.0f) {
        for (int32_t y0 = py - up; y0 < py + down; ++y0) {
          if (g_negctl_terrain_render == 6) {
            for (int32_t x0 = px - left; x0 <= px + right; ++x0) {
              t = tile_at(x0, y0);
              if (t != 0 && !hidden(t) && !is_culled(t, ptile, angle)) tile_render(t);
            }
            continue;
          }
          for (int32_t x0 = px + right; x0 >= px - left; --x0) {
            t = tile_at(x0, y0);
            if (t != 0 && !hidden(t) && !is_culled(t, ptile, angle)) tile_render(t);
          }
        }
      } else if (angle == 90.0f) {
        for (int32_t x1 = px + right; x1 >= px - left; --x1)
          for (int32_t y1 = py + down; y1 >= py - up; --y1) {
            t = tile_at(x1, y1);
            if (t != 0 && !hidden(t) && !is_culled(t, ptile, angle)) tile_render(t);
          }
      } else if (angle == 180.0f) {
        for (int32_t y2 = py + down; y2 >= py - up; --y2)
          for (int32_t x2 = px - left; x2 < px + right; ++x2) {
            t = tile_at(x2, y2);
            if (t != 0 && !hidden(t) && !is_culled(t, ptile, angle)) tile_render(t);
          }
      } else {
        for (int32_t y3 = py - up; y3 < py + down; ++y3)
          for (int32_t x3 = px - left; x3 < px + right; ++x3) {
            t = tile_at(x3, y3);
            if (t != 0 && !hidden(t) && !is_culled(t, ptile, angle)) tile_render(t);
          }
      }
    }
    gl_.pop_matrix();
    gl_.flush();
    return true;
  }

  // Terrain::localRender(pTile, pivot, angle): the 5 x 5 tiles around pTile (hidden ones skipped, no culling test) queued with
  // drawTile and drawTileObjects in the camera's order, the camera translated to `pivot`, then the list drawn.
  void local_render(uint32_t ptile, uint32_t pivot, float angle) {
    // the original asserts pTile != NULL before its first GL call; the debug assert ends the process there
    if (ptile == 0) throw Fault{0, 4};
    begin_pass(ptile != 0 ? pivot : 0, angle, ptile != 0);
    set_view_angle(angle);
    const int32_t px = tile_x(ptile), py = tile_y(ptile);
    auto visit = [&](int32_t x, int32_t y) {
      const uint32_t t = tile_at(x, y);
      if (t == 0 || hidden(t)) return;
      draw_tile(t, angle);
      draw_tile_objects(t, ptile, angle);
    };
    if (angle == 0.0f) {
      for (int32_t y = py - 2; y <= py + 2; ++y)
        for (int32_t x = px + 2; x >= px - 2; --x) visit(x, y);
    } else if (angle == 90.0f) {
      for (int32_t x = px + 2; x >= px - 2; --x)
        for (int32_t y = py + 2; y >= py - 2; --y) visit(x, y);
    } else if (angle == 180.0f) {
      for (int32_t y = py + 2; y >= py - 2; --y)
        for (int32_t x = px - 2; x <= px + 2; ++x) visit(x, y);
    } else {
      for (int32_t y = py - 2; y <= py + 2; ++y)
        for (int32_t x = px - 2; x <= px + 2; ++x) visit(x, y);
    }
    end_pass();
  }

  // Terrain::renderTileAtFiveOffsets(pTile, pivot, angle) (roadmap S3.28; reading reference re/match/terrain_edge.cpp), the draw
  // half of Terrain::lowerEdgeCorner, which calls it before Tile::lowerCorner. localRender's transform (no draw list); when one of
  // the three neighbours toward the camera is outside the grid or hidden, enableBlackLight1 and Tile::render of pTile at the five
  // cumulative offsets (10, 0, -10), (-20, 0, 0), (0, 0, 20), (20, 0, 0), (-10, 0, -10), then light 1 off and light 0 on. The
  // original asserts pTile != NULL first (its debug assert ends the process there), and reads the pivot's position only when
  // pTile != NULL, which the assert already holds.
  void render_tile_at_five_offsets(uint32_t ptile, uint32_t pivot, float angle) {
    if (ptile == 0) throw Fault{0, 4};
    if (g_negctl_terrain_render == 15) return;
    gl_.load_identity();
    gl_.push_matrix();
    gl_.rotated(static_cast<double>(g_.screen_scale), 1.0, 0.0, 0.0);
    gl_.rotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    gl_.translatef(centre_shift(tile_x(pivot)) * 100.0f, 0.0f, centre_shift(tile_y(pivot)) * 100.0f);
    set_view_angle(angle);
    const int32_t x = tile_x(ptile), y = tile_y(ptile);
    auto open = [&](int32_t nx, int32_t ny) {   // tileAt(...) == NULL || tileAt(...)->isHidden()
      const uint32_t t = tile_at(nx, ny);
      return t == 0 || hidden(t);
    };
    int32_t dx, dy;   // the neighbours toward the camera: (x + dx, y), (x, y + dy), (x + dx, y + dy)
    const bool swap18 = g_negctl_terrain_render == 18;
    if (angle == 0.0f) { dx = 1; dy = -1; }
    else if (angle == (swap18 ? 180.0f : 90.0f)) { dx = 1; dy = 1; }
    else if (angle == (swap18 ? 90.0f : 180.0f)) { dx = -1; dy = 1; }
    else { dx = -1; dy = -1; }
    if (open(x + dx, y) || open(x, y + dy) || open(x + dx, y + dy)) {
      enable_black_light1();
      gl_.translatef(10.0f, 0.0f, -10.0f);
      tile_render(ptile);
      gl_.translatef(g_negctl_terrain_render == 16 ? -10.0f : -20.0f, 0.0f, 0.0f);
      tile_render(ptile);
      gl_.translatef(0.0f, 0.0f, 20.0f);
      tile_render(ptile);
      gl_.translatef(20.0f, 0.0f, 0.0f);
      tile_render(ptile);
      gl_.translatef(-10.0f, 0.0f, -10.0f);
      tile_render(ptile);
      gl_.disable(0x4001);   // GL_LIGHT1
      gl_.enable(0x4000);    // GL_LIGHT0
    }
    gl_.pop_matrix();
    gl_.flush();
  }

  // Terrain::enableBlackLight1 (re/match/terrain_small3.cpp): light 0 off, light 1's ambient, diffuse and specular all 0 in that
  // order, light 1 on. With both lights' colours out of the sum only the global ambient term is left (GL_MODULATE: 0.04 x texel).
  void enable_black_light1() {
    gl_.disable(0x4000);
    const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float amb17[4] = {0.2f, 0.2f, 0.2f, 1.0f};   // control 17's wrong ambient
    gl_.light(0x4001, 0x1200, g_negctl_terrain_render == 17 ? amb17 : zero);   // GL_AMBIENT
    gl_.light(0x4001, 0x1201, zero);                                          // GL_DIFFUSE
    gl_.light(0x4001, 0x1202, zero);                                          // GL_SPECULAR
    gl_.enable(0x4001);
  }

  // Terrain::stripRender(pivot, dir, angle): the tiles of diagonal strips around `pivot` for view direction 1..8. An odd
  // direction is two complete nested passes (its neighbouring even directions), each with its own matrix push, list and flush;
  // the outer pass then draws its own (empty) list. Any other direction draws nothing. Strip windows scale with the zoom shift.
  void strip_render(uint32_t pivot, int32_t dir, float angle) {
    begin_pass(pivot, angle, pivot != 0);
    set_view_angle(angle);
    const int32_t zoom = i32(terrain_ + kTerZoom);
    const int32_t late = g_negctl_terrain_render == 11 ? 1 : 0;
    auto visit = [&](uint32_t t) {
      if (!hidden(t) && !is_culled(t, pivot, angle)) {
        draw_tile(t, angle);
        draw_tile_objects(t, pivot, angle);
      }
    };
    switch (dir) {
      case 1: strip_render(pivot, 8, angle); strip_render(pivot, 2, angle); break;
      case 3: strip_render(pivot, 2, angle); strip_render(pivot, 4, angle); break;
      case 5: strip_render(pivot, 4, angle); strip_render(pivot, 6, angle); break;
      case 7: strip_render(pivot, 8, angle); strip_render(pivot, 6, angle); break;
      case 2: {
        const int32_t back = 15 << zoom, front = 3 << zoom, left = (zoom << zoom) + 5, right = 4;
        for (int32_t i = tile_x(pivot) - left + late; i < tile_x(pivot) + right; ++i)
          for (int32_t y = tile_y(pivot) - back, x = i; y < tile_y(pivot) + front; ++x, ++y) {
            const uint32_t t = tile_at(x, y);
            if (y > n() - 1 || x > n() - 1) break;
            if (x >= 0 && y >= 0) visit(t);
          }
        break;
      }
      case 4: {
        const int32_t near_x = 4 << zoom, far_x = 16 << zoom, front = 2, back = (zoom << zoom) + 9;
        for (int32_t j = tile_y(pivot) - back + late; j < tile_y(pivot) + front; ++j)
          for (int32_t x = tile_x(pivot) + far_x, y = j; x > tile_x(pivot) - near_x; --x, ++y) {
            const uint32_t t = tile_at(x, y);
            if (x < 0 || y > n() - 1) break;
            if (y >= 0 && x <= n() - 1) visit(t);
          }
        break;
      }
      case 6: {
        const int32_t near_x = 3 << zoom, far_x = 16 << zoom, front = 4, back = (zoom << zoom) + 8;
        for (int32_t j = tile_y(pivot) - back + late; j < tile_y(pivot) + front; ++j)
          for (int32_t x = tile_x(pivot) - far_x, y = j; x < tile_x(pivot) + near_x; ++x, ++y) {
            const uint32_t t = tile_at(x, y);
            if (y > n() - 1 || x > n() - 1) break;
            if (x >= 0 && y >= 0) visit(t);
          }
        break;
      }
      case 8: {
        const int32_t near_x = 4 << zoom, far_x = 17 << zoom, front = (zoom << zoom) + 12, back = -2;
        for (int32_t j = tile_y(pivot) + back + late; j < tile_y(pivot) + front; ++j)
          for (int32_t x = tile_x(pivot) - far_x, y = j; x < tile_x(pivot) + near_x; ++x, --y) {
            const uint32_t t = tile_at(x, y);
            if (y < 0 || x > n() - 1) break;
            if (x >= 0 && y <= n() - 1) visit(t);
          }
        break;
      }
      default: break;
    }
    end_pass();
  }

  // Terrain::pathUpdateRender(pivot, angle): the tiles with a path in the window of 15 << zoom tiles around `pivot`, queued like
  // the other passes. It does not call setViewAngle: the view quadrant is the one the previous pass left. Original quirks kept:
  // the left edge is clipped to x + 1 (not x) and the right edge to N - 1 - x (49 - x); pivot is read after the GL calls even when NULL.
  void path_update_render(uint32_t pivot, float angle) {
    const int32_t zoom = i32(terrain_ + kTerZoom);
    int32_t top, bottom, left, right;
    top = bottom = left = right = (15 + g_world_reach) << zoom;   // P35D: g_world_reach 0 outside the world layer's passes
    begin_pass(pivot, angle, pivot != 0);
    const int32_t px = tile_x(pivot), py = tile_y(pivot);
    if (py < top) top = py;
    if (n() - py < bottom) bottom = n() - py;
    if (px < left) left = g_negctl_terrain_render == 12 ? px : px + 1;
    if (n() - 1 - px < right) right = n() - 1 - px;
    for (int32_t y = py - top; y < py + bottom; ++y)
      for (int32_t x = px + right; x > px - left; --x) {
        const uint32_t t = tile_at(x, y);
        if (u8(t + kTileHasPath) != 0 && !hidden(t) && !is_culled(t, pivot, angle)) {
          draw_tile(t, angle);
          draw_tile_objects(t, pivot, angle);
        }
      }
    end_pass();
  }

 private:
  Ctx &c_;
  gl::GlSink &gl_;
  TerrainGlobals &g_;
  uint32_t terrain_;

  // ---- memory ----
  int32_t i32(uint32_t va) const { return c_.mem.load<int32_t>(va); }
  uint32_t u32v(uint32_t va) const { return c_.mem.load<uint32_t>(va); }
  uint8_t u8(uint32_t va) const { return c_.mem.load<uint8_t>(va); }
  int32_t tile_x(uint32_t t) const { return i32(t + kTileX); }
  int32_t tile_y(uint32_t t) const { return i32(t + kTileY); }
  int32_t tile_type(uint32_t t) const { return i32(t + kTileType); }
  bool hidden(uint32_t t) const { return tile_type(t) == kTypeHidden; }
  uint32_t neighbour(uint32_t t, int k) const { return u32v(t + kTileNeighbours + 4u * static_cast<uint32_t>(k)); }
  uint32_t face(uint32_t t, int f) const { return t + kTileFaces + kFaceSize * static_cast<uint32_t>(f); }
  // vertex index k (0..2) of face f of tile t
  int32_t fv(uint32_t t, int f, int k) const { return i32(face(t, f) + kFaceVertex + 4u * static_cast<uint32_t>(k)); }

  // Terrain::tileAt: the tile's address, 0 outside the grid
  uint32_t tile_at(int32_t x, int32_t y) const {
    const int32_t w = i32(terrain_ + kTerWidth), h = i32(terrain_ + kTerHeight);
    if (x >= w || x < 0 || y >= h || y < 0) return 0;
    return terrain_ + kTerTiles + static_cast<uint32_t>(x + y * w) * kTileSize;
  }

  // ---- globals (an index the original never bounds-checks reads foreign data there: a fault here) ----
  const float *vert(int32_t i) const {
    if (static_cast<uint32_t>(i) >= g_.vertices.size() / 3) throw Fault{static_cast<uint32_t>(i), 12};
    return &g_.vertices[3u * static_cast<uint32_t>(i)];
  }
  const float *norm(int32_t i) const {
    if (static_cast<uint32_t>(i) >= g_.normals.size() / 3) throw Fault{static_cast<uint32_t>(i), 12};
    return &g_.normals[3u * static_cast<uint32_t>(i)];
  }
  uint32_t texture(uint32_t type, uint32_t variation, uint32_t slot) const {
    const uint64_t i = (static_cast<uint64_t>(type) * kTextureVariations + variation) * kTextureSlots + slot;
    if (i >= g_.textures.size()) throw Fault{static_cast<uint32_t>(i), 4};
    return g_.textures[static_cast<size_t>(i)];
  }
  const float *tex_coord(int32_t a, int32_t b, int32_t view) const {
    const int64_t i = ((static_cast<int64_t>(a) * 3 + b) * 4 + view) * 2;
    if (i < 0 || static_cast<uint64_t>(i) + 2 > g_.tex_coords.size()) throw Fault{static_cast<uint32_t>(i), 8};
    return &g_.tex_coords[static_cast<size_t>(i)];
  }

  // ---- the draw-list passes ----
  // The draw list is a std::list<Tile*> in the Terrain object; every pass empties it before it returns, so a pass always finds
  // it empty (size 0 in every corpus snapshot). Its nodes live on Terrain.dll's heap: the port keeps the order in list_ and
  // leaves the object's header as it was, which is also what the original's clear() leaves.
  std::vector<uint32_t> list_;

  // the frame every draw-list pass opens with: modelview reset, pushed, the screen-scale tilt, the camera angle, the pivot
  void begin_pass(uint32_t pivot, float angle, bool translate) {
    if (i32(terrain_ + ter_draw_list_size(c_)) != 0) throw Fault{terrain_ + ter_draw_list_size(c_), 4};   // never seen: not modelled
    gl_.load_identity();
    gl_.push_matrix();
    gl_.rotated(static_cast<double>(g_.screen_scale), 1.0, 0.0, 0.0);
    gl_.rotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    if (translate)
      gl_.translatef(centre_shift(tile_x(pivot)) * 100.0f, 0.0f, centre_shift(tile_y(pivot)) * 100.0f);
  }

  // the list drawn front to back as queued (Tile::render each), cleared, the matrix popped, glFlush
  void end_pass() {
    std::vector<uint32_t> l;
    l.swap(list_);
    if (g_negctl_terrain_render == 9) std::reverse(l.begin(), l.end());
    for (size_t i = 0; i < l.size(); ++i) {
      if (g_negctl_terrain_render == 10 && i == 1) continue;
      tile_render(l[i]);
    }
    gl_.pop_matrix();
    gl_.flush();
  }

  // Terrain::drawTile(t, center, angle): insert t into the draw list, kept back to front for the angle, unless the list already
  // holds a tile at t's position. Per angle the primary / secondary keys and their directions are:
  //   0: x descending, then y ascending      90: y descending, then x descending
  //   180: x ascending, then y descending    other: y ascending, then x ascending
  // The `other` branch tests `y < it.y` after a walk that stopped at the first it.y >= y, so that test never holds and the
  // secondary walk always runs (an equal-key insert lands before the first tile of the next row, as in the other branches).
  void draw_tile(uint32_t t, float angle) {
    const int32_t x = tile_x(t), y = tile_y(t);
    std::vector<uint32_t> &L = list_;
    if (L.empty()) {
      L.push_back(t);
      return;
    }
    size_t it = 0;
    const size_t n = L.size();
    auto X = [&](size_t i) { return tile_x(L[i]); };
    auto Y = [&](size_t i) { return tile_y(L[i]); };
    const bool dup = g_negctl_terrain_render == 14;
    if (angle == 0.0f) {
      while (it != n && x < X(it)) ++it;
      if (it == n || X(it) < x) { L.insert(L.begin() + static_cast<std::ptrdiff_t>(it), t); return; }
      while (it != n && y > Y(it) && X(it) == x) ++it;
    } else if (angle == 90.0f) {
      while (it != n && y < Y(it)) ++it;
      if (it == n || Y(it) < y) { L.insert(L.begin() + static_cast<std::ptrdiff_t>(it), t); return; }
      while (it != n && x < X(it) && Y(it) == y) ++it;
    } else if (angle == 180.0f) {
      while (it != n && x > X(it)) ++it;
      if (it == n || X(it) > x) { L.insert(L.begin() + static_cast<std::ptrdiff_t>(it), t); return; }
      while (it != n && y < Y(it) && X(it) == x) ++it;
    } else {
      while (it != n && y > Y(it)) ++it;
      if (it == n || Y(it) < y) { L.insert(L.begin() + static_cast<std::ptrdiff_t>(it), t); return; }
      while (it != n && x > X(it) && Y(it) == y) ++it;
    }
    if (!dup && it != n && Y(it) == y && X(it) == x) return;
    L.insert(L.begin() + static_cast<std::ptrdiff_t>(it), t);
  }

  // Terrain::drawTileObjects(t, center, angle): the neighbours behind t's walls that face the camera (wall 6 = x - 1 and 4 = y + 1
  // at 0 degrees, 6 / 0 at 90, 2 / 0 at 180, 2 / 4 otherwise, plus the diagonal tile between them), each not hidden and not
  // culled against `center`, queued and followed recursively in the order x side, y side, diagonal.
  void draw_tile_objects(uint32_t t, uint32_t center, float angle) {
    if (g_negctl_terrain_render == 13) return;
    const int32_t x = tile_x(t), y = tile_y(t);
    uint32_t n[3] = {0, 0, 0};
    auto wall = [&](uint32_t side) { return u8(t + kTileWallOn + side) != 0; };   // Terrain::getWall -> Tile::getWall
    int32_t sx, sy, wx, wy;   // the x-side wall and neighbour step, the y-side wall and neighbour step
    if (angle == 0.0f) { sx = -1; sy = 1; wx = 6; wy = 4; }
    else if (angle == 90.0f) { sx = -1; sy = -1; wx = 6; wy = 0; }
    else if (angle == 180.0f) { sx = 1; sy = -1; wx = 2; wy = 0; }
    else { sx = 1; sy = 1; wx = 2; wy = 4; }
    if (wall(static_cast<uint32_t>(wx))) {
      n[0] = tile_at(x + sx, y);
      n[2] = tile_at(x + sx, y + sy);
    }
    if (wall(static_cast<uint32_t>(wy))) {
      n[1] = tile_at(x, y + sy);
      n[2] = tile_at(x + sx, y + sy);
    }
    for (uint32_t k : n) {
      if (k != 0 && !hidden(k) && !is_culled(k, center, angle)) {
        draw_tile(k, angle);
        draw_tile_objects(k, center, angle);
      }
    }
  }

  // Terrain::setViewAngle
  void set_view_angle(float angle) {
    if (angle == 0.0f) g_.view_index = 0;
    else if (angle == 90.0f) g_.view_index = 1;
    else if (angle == 180.0f) g_.view_index = 2;
    else g_.view_index = 3;
  }

  int32_t max_height(uint32_t t) const {
    const int32_t h0 = i32(t + kTileHeight0), h1 = i32(t + kTileHeight1), h2 = i32(t + kTileHeight2), h3 = i32(t + kTileHeight3);
    const int32_t a = h0 > h3 ? h0 : h3;
    const int32_t b = h1 > h2 ? h1 : h2;
    return a > b ? a : b;
  }

  // Terrain::isCulled(t, center, angle): the screen-space test of one tile against the window, per camera angle.
  bool is_culled(uint32_t t, uint32_t center, float angle) const {
    const int32_t zoom = i32(terrain_ + kTerZoom);
    if (zoom == 2) return false;
    const int32_t w = i32(terrain_ + kTerScreenW), h = i32(terrain_ + kTerScreenH);
    // P8B: the world layer's side margin (0 = the original's test: adding 0 changes no comparison)
    const int32_t halfW = (w >> 1) * zoom + (g_cull_margin << zoom), halfH = (h >> 1) * zoom;
    // P35D: the world layer's bottom margin (0 = the original's test). Only the bottom: the height term lifts a tile, so a tile
    // whose lifted point is below the view can still reach into it, one above it cannot
    const int32_t lowH = halfH + (g_world_cull_y << zoom);
    const int32_t dy = tile_y(t) - tile_y(center), dx = tile_x(t) - tile_x(center);
    // The four footprint constants per window size; any other size leaves them uninitialised (the debug build's stack fill)
    int32_t A = static_cast<int32_t>(0xCCCCCCCCu), B = A, C = A, D = A;
    if (w == 800 && h == 600) { A = 0x40; B = 0x28; C = 0x20; D = 0x14; }
    else if (w == 1024 && h == 768) { A = 0x50; B = 0x34; C = 0x28; D = 0x1A; }
    else if (w == 1280 && h == 1024) { A = 0x68; B = 0x44; C = 0x34; D = 0x22; }
    const double ang = static_cast<double>(angle);
    const bool swap = g_negctl_terrain_render == 3;
    int32_t sx, sy;
    if (ang == 0.0) {
      sx = (w >> 1) + dy * C + dx * C;
      sy = (h >> 1) + dy * D - dx * D;
      sy -= max_height(t) * 5;
      return sx > w + halfW || sx < -A - halfW || sy > h + B + lowH || sy < -B - halfH;
    } else if (ang == (swap ? 180.0 : 90.0)) {
      sx = (w >> 1) - dy * C + dx * C;
      sy = (h >> 1) - dy * D - dx * D;
      sy -= max_height(t) * 5;
      return sx > w + C + halfW || sx < -A - halfW || sy > h + B + lowH || sy < -B - halfH;
    } else if (ang == (swap ? 90.0 : 180.0)) {
      sx = (w >> 1) - dy * C - dx * C;
      sy = (h >> 1) - dy * D + dx * D;
      sy -= max_height(t) * 5;
      return sx > w + A + halfW || sx < -C - halfW || sy > h + B + lowH || sy < -B - halfH;
    } else {
      sx = (w >> 1) + dy * C - dx * C;
      sy = (h >> 1) + dy * D + dx * D;
      sy -= max_height(t) * 5;
      return sx > w + C + halfW || sx < -C - halfW || sy > h + D + lowH || sy < -B - D - halfH;
    }
  }

  // Tile::render: the faces as triangles (a texture is bound only when it differs from the previous face's), then the
  // path overlay, the walls and the skirts.
  void tile_render(uint32_t t) {
    uint32_t last = 0xCCCCCCCCu;   // the original's uninitialised cache local: the debug build's stack fill
    const int32_t n = i32(t + kTileFaceCount);
    const int32_t view = g_.view_index;
    // P13A (Tile::render.fairwayStripesFixed = 0): a fairway / firm fairway tile (types 2 / 3) whose rotation (setType's third
    // argument: the exe's g_tileVariant byte, 12 + orientation % 12 for a tile the paint tool laid, mainLoop's H2_Terrain) has an
    // odd orientation takes the texture coordinates of the next view (Terrain.dll's table turns every (u, v) 90 degrees about the
    // centre from one view to the next) on its faces, so its mowing stripes turn 90 degrees; Tab steps the orientation. Every
    // face turns whole, its own piece included: the pieces' borders (FairwayA / FirmFairwayA 0001..0009: piece p is file p + 1) are
    // the same on all four sides (0 and 5..8 none, 1 a frame, 3 rounded, 4 the four corners rounded, 2 a small dark nub in each of
    // the four corners, the rim of an inner corner), so a 90-degree turn leaves each where it was. P19B item 4: P13A fix 6 drew a
    // turned tile's piece-2 faces with piece 0 (it took piece 2 for a two-sided border); that dropped the nubs and left light
    // notches in the fairway's dark rim (log/roadmap/P19/P19B_notes.md).
    int32_t uv_turn = view;
    if (!SG_DEFECT(g_defect_fairway_stripes_fixed) && (tile_type(t) == 2 || tile_type(t) == 3)) {
      const int32_t r = i32(t + kTileRotation);
      if (r >= 12 && r < 24 && ((r - 12) & 1) != 0) uv_turn = (view + 1) & 3;
    }
    for (int32_t i = 0; i < n; ++i) {
      const uint32_t f = face(t, i);
      uint32_t tex;
      if (view != 0 && tile_type(t) == kTypeRotated) {
        int32_t rot = i32(t + kTileRotation) % 4;
        rot -= view;
        rot = rot < 0 ? rot + 4 : rot;
        if (g_negctl_terrain_render == 8) rot = (rot + 1) % 4;
        tex = texture(static_cast<uint32_t>(rot + 0x1B), static_cast<uint32_t>(i32(t + kTileVariation)), u8(f + kFaceSlot));
      } else {
        tex = u32v(f + kFaceTexture);
      }
      if (uv_turn != view && u8(f + kFaceSlot) == 2 && g_negctl_terrain_render == 19)   // control 19: P13A fix 6's piece 0
        tex = texture(static_cast<uint32_t>(tile_type(t)), static_cast<uint32_t>(i32(t + kTileVariation)), 0u);
      if (tex != last || g_negctl_terrain_render == 2) {
        gl_.bind_texture(gl::kTexture2D, tex);
        last = tex;
      }
      gl_.begin(gl::kTriangles);
      for (uint32_t k = 0; k < 3; ++k) {
        gl_.tex_coord2fv(tex_coord(i32(f + kFaceUv + 8 * k), i32(f + kFaceUv + 8 * k + 4), uv_turn));
        gl_.array_element(i32(f + kFaceVertex + 4 * k));
      }
      gl_.end();
    }
    if (u8(t + kTileHasPath) != 0 && u8(t + kTileDrawPath) != 0) draw_paths(t);
    draw_walls(t);
    draw_skirts(t, 3);
  }

  // the three-vertex-of-a-face reference used by the immediate-mode drawing: face f, vertex k of tile t
  const float *fvert(uint32_t t, int f, int k) const { return vert(fv(t, f, k)); }

  // Tile::drawSkirts(depth): toward each neighbour of type 0x14 that faces the camera, a wall from the shared edge down to
  // y = -75, continued into the next neighbour along that edge, `depth` tiles at most.
  void draw_skirts(uint32_t t, int32_t depth) {
    if (tile_type(t) == kTypeHidden) return;
    const int32_t quad = g_.view_index;
    const int c = (quad == 0 || quad == 2) ? 0 : 1;
    gl_.bind_texture(gl::kTexture2D, g_.wall_texture);
    const int32_t next = g_negctl_terrain_render == 1 ? depth : depth - 1;
    // neighbour k, the side-normal block it uses, the two edge vertices (face, vertex), the quadrants that face it, and which
    // neighbour the skirt continues into for the first / second of those quadrants
    struct Skirt { int nb, norm, fa, va, fb, vb, q0, q1, n0, n1; };
    static const Skirt kSkirts[4] = {
        {1, 0, 5, 0, 6, 2, 0, 3, 3, 2},
        {0, 1, 2, 2, 0, 0, 1, 2, 3, 2},
        {2, 3, 0, 0, 4, 1, 0, 1, 0, 1},
        {3, 2, 7, 1, 3, 2, 2, 3, 1, 0},
    };
    for (const Skirt &s : kSkirts) {
      const uint32_t nb = neighbour(t, s.nb);
      if (nb == 0 || tile_type(nb) != kTypeHidden || !(quad == s.q0 || quad == s.q1)) continue;
      const float *a = fvert(t, s.fa, s.va);
      const float *b = fvert(t, s.fb, s.vb);
      gl_.begin(gl::kTriangles);
      gl_.normal3fv(g_.side_normals[s.norm][c]);
      gl_.tex_coord2f(0.0f, 1.0f);
      gl_.vertex3fv(a);
      gl_.tex_coord2f(0.0f, 0.0f);
      gl_.vertex3f(a[0], -75.0f, a[2]);
      gl_.tex_coord2f(1.0f, 0.0f);
      gl_.vertex3f(b[0], -75.0f, b[2]);
      gl_.tex_coord2f(0.0f, 1.0f);
      gl_.vertex3fv(a);
      gl_.tex_coord2f(1.0f, 0.0f);
      gl_.vertex3f(b[0], -75.0f, b[2]);
      gl_.tex_coord2f(1.0f, 1.0f);
      gl_.vertex3fv(b);
      gl_.end();
      const int cont = quad == s.q0 ? s.n0 : s.n1;
      const uint32_t cn = neighbour(t, cont);
      if (cn != 0 && depth > 0) draw_skirts(cn, next);
    }
  }

  // Tile::drawWalls: for each of the four edges with a wall flag and a neighbour, six quads' worth of triangles between this
  // tile's edge vertices and the neighbour's facing vertices.
  void draw_walls(uint32_t t) {
    const int c = (g_.view_index == 0 || g_.view_index == 2) ? 0 : 1;
    struct Ref { bool other; int face, v; };
    struct Wall { int on, nb, norm; Ref r[6]; };   // r = A..F: triangles (A,B,C) (A,C,D) (D,C,E) (D,E,F)
    static const Wall kWalls[4] = {
        {0, 0, 0, {{true, 5, 0}, {false, 0, 0}, {false, 1, 2}, {true, 5, 1}, {false, 2, 2}, {true, 6, 2}}},
        {6, 2, 2, {{true, 7, 1}, {false, 4, 1}, {false, 4, 0}, {true, 3, 1}, {false, 0, 0}, {true, 3, 2}}},
        {2, 3, 3, {{true, 0, 0}, {false, 3, 2}, {false, 3, 1}, {true, 4, 0}, {false, 7, 1}, {true, 4, 1}}},
        {4, 1, 1, {{true, 2, 2}, {false, 6, 2}, {false, 5, 1}, {true, 1, 2}, {false, 5, 0}, {true, 0, 0}}},
    };
    static const float kU[6] = {0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f};
    static const float kV[6] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    static const int kTri[12] = {0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5};
    for (const Wall &w : kWalls) {
      const uint32_t nb = neighbour(t, w.nb);
      if (u8(t + kTileWallOn + static_cast<uint32_t>(w.on)) == 0 || nb == 0) continue;
      const bool kind1 = i32(t + kTileWallKind + 4u * static_cast<uint32_t>(w.on)) == 1;
      const bool swap = g_negctl_terrain_render == 4;
      gl_.bind_texture(gl::kTexture2D, (kind1 != swap) ? g_.fence_texture1 : g_.fence_texture0);
      gl_.begin(gl::kTriangles);
      gl_.normal3fv(g_.side_normals[w.norm][c]);
      for (int i = 0; i < 12; ++i) {
        const int p = kTri[i];
        gl_.tex_coord2f(kU[p], kV[p]);
        gl_.vertex3fv(fvert(w.r[p].other ? nb : t, w.r[p].face, w.r[p].v));
      }
      gl_.end();
    }
  }

  // Tile::drawPaths: a blended band (half-width 16.67) toward every neighbour that has a path, collected in a 4-bit mask, and
  // the corner overlays that close it.
  void draw_paths(uint32_t t) {
    if (u8(t + kTileDrawPath) == 0) return;
    int32_t mask = 0;
    const float w = g_negctl_terrain_render == 5 ? 16.0f : 16.67f;
    const bool n0 = neighbour(t, 0) != 0 && u8(neighbour(t, 0) + kTileHasPath) != 0;
    const bool n2 = neighbour(t, 2) != 0 && u8(neighbour(t, 2) + kTileHasPath) != 0;
    const bool n3 = neighbour(t, 3) != 0 && u8(neighbour(t, 3) + kTileHasPath) != 0;
    const bool n1 = neighbour(t, 1) != 0 && u8(neighbour(t, 1) + kTileHasPath) != 0;
    const uint32_t tex = u8(t + kTileConnected) != 0 ? 0x21u : 0x20u;
    // P39A2 layPath.stairs = 0: the halves along a stairs axis (terrain.h path_stairs_axes of this tile) are drawn as steps after
    // the corner overlays instead of as a band; nothing changes on a tile with no stairs axis (or at 1)
    bool stair[4] = {false, false, false, false};
    if (!SG_DEFECT(g_defect_path_stairs)) stairs_halves(t, n0, n1, n2, n3, stair);
    gl_.enable(gl::kBlend);
    gl_.blend_func(gl::kSrcAlpha, gl::kOneMinusSrcAlpha);
    gl_.bind_texture(gl::kTexture2D, texture(tex, 0, 0));
    // one vertex of a band: normal of vertex `nv`, texture coordinate, position (x offset ox from vertex `px`, y z from `pyz`)
    auto vtx = [&](int32_t nv, float s, float tt, float x, float y, float z) {
      gl_.normal3fv(norm(nv));
      gl_.tex_coord2f(s, tt);
      gl_.vertex3f(x, y, z);
    };
    const int32_t f0v2 = fv(t, 0, 2), f1v2 = fv(t, 1, 2);
    if (n0) {
      mask += 1;
      const float *a = vert(f0v2), *b = vert(f1v2);
      if (!stair[0]) {
        gl_.begin(gl::kTriangleFan);
        vtx(f0v2, 0.333f, 0.667f, a[0] - w, a[1], a[2]);
        vtx(f0v2, 0.667f, 0.667f, w + a[0], a[1], a[2]);
        vtx(f1v2, 0.667f, 1.0f, w + a[0], b[1], b[2]);
        vtx(f1v2, 0.333f, 1.0f, a[0] - w, b[1], b[2]);
        gl_.end();
      }
    }
    if (n1) {
      mask += 2;
      gl_.bind_texture(gl::kTexture2D, texture(tex, 0, 0));
      const int32_t f6v1 = fv(t, 6, 1), f6v2 = fv(t, 6, 2);
      const float *a = vert(f0v2), *b = vert(f6v1);
      if (!stair[1]) {
        gl_.begin(gl::kTriangleFan);
        vtx(f0v2, 0.333f, 0.333f, a[0] - w, a[1], a[2]);
        vtx(f6v1, 0.333f, 0.0f, b[0] - w, b[1], b[2]);
        vtx(f6v2, 0.667f, 0.0f, w + b[0], b[1], b[2]);
        vtx(f0v2, 0.667f, 0.333f, w + a[0], a[1], a[2]);
        gl_.end();
      }
    }
    if (n3) {
      mask += 4;
      gl_.bind_texture(gl::kTexture2D, texture(tex, 0, 0));
      const int32_t f7v2 = fv(t, 7, 2);
      const float *a = vert(f0v2), *b = vert(f7v2);
      if (!stair[3]) {
        gl_.begin(gl::kTriangleFan);
        vtx(f0v2, 0.667f, 0.667f, a[0], a[1], a[2] - w);
        vtx(f0v2, 0.667f, 0.333f, a[0], a[1], w + a[2]);
        vtx(f7v2, 1.0f, 0.333f, b[0], b[1], w + b[2]);
        vtx(f7v2, 1.0f, 0.667f, b[0], b[1], b[2] - w);
        gl_.end();
      }
    }
    if (n2) {
      mask += 8;
      gl_.bind_texture(gl::kTexture2D, texture(tex, 0, 0));
      const int32_t f4v0 = fv(t, 4, 0);
      const float *a = vert(f0v2), *b = vert(f4v0);
      if (!stair[2]) {
        gl_.begin(gl::kTriangleFan);
        vtx(f0v2, 0.333f, 0.667f, a[0], a[1], a[2] - w);
        vtx(f4v0, 0.0f, 0.667f, b[0], b[1], b[2] - w);
        vtx(f4v0, 0.0f, 0.333f, b[0], b[1], w + b[2]);
        vtx(f0v2, 0.333f, 0.333f, a[0], a[1], w + a[2]);
        gl_.end();
      }
    }
    draw_corner_overlays(t, mask, static_cast<int32_t>(tex));
    if (stair[0] || stair[1] || stair[2] || stair[3]) draw_stairs(t, stair, tex, w);
    gl_.disable(gl::kBlend);
  }

  // ---- P39A2 layPath.stairs = 0: stairs (re/roadmap/P39_COURSE_FEATURES.md "P39A decided", log/roadmap/P39/P39A2_notes.md) ----
  // Half k (the band toward neighbour k: 0 y - 1, 1 y + 1, 2 x - 1, 3 x + 1, terrain_state.h TerrainSide) runs from the centre vertex
  // (face 0 vertex 2) to the edge vertex its band ends at; its texture corners and its fan order are the band's in draw_paths. The y
  // axis (bit 1 << 1 of path_stairs_axes) holds halves 0 / 1, the x axis (bit 1 << 0) halves 2 / 3.
  struct StairHalf { int32_t face, vtx; float tc[2][2][2]; bool order_b; };   // tc[u][side][s / t]: u 0 centre / 1 edge, side - / +
  static const StairHalf &stair_half(int k) {
    static const StairHalf kHalves[4] = {
        {1, 2, {{{0.333f, 0.667f}, {0.667f, 0.667f}}, {{0.333f, 1.0f}, {0.667f, 1.0f}}}, false},
        {6, 1, {{{0.333f, 0.333f}, {0.667f, 0.333f}}, {{0.333f, 0.0f}, {0.667f, 0.0f}}}, true},
        {4, 0, {{{0.333f, 0.667f}, {0.333f, 0.333f}}, {{0.0f, 0.667f}, {0.0f, 0.333f}}}, true},
        {7, 2, {{{0.667f, 0.667f}, {0.667f, 0.333f}}, {{1.0f, 0.667f}, {1.0f, 0.333f}}}, false},
    };
    return kHalves[k];
  }
  // which halves of tile t are drawn as stairs: a half whose band is drawn (the neighbour has a path), on a stairs axis, rising at
  // least a quarter level between its two vertices (a flat half of a stairs tile keeps its band)
  void stairs_halves(uint32_t t, bool n0, bool n1, bool n2, bool n3, bool out[4]) {
    const int32_t axes = path_stairs_axes(c_, tile_x(t), tile_y(t));
    if (axes == 0) return;
    const bool on[4] = {n0, n1, n2, n3};
    const float step = g_.corner_step > 0.0f ? g_.corner_step : 15.0f;
    const float hc = vert(fv(t, 0, 2))[1];
    for (int k = 0; k < 4; ++k) {
      if (!on[k] || (axes & (k < 2 ? 2 : 1)) == 0) continue;
      const float he = vert(fv(t, stair_half(k).face, stair_half(k).vtx))[1];
      out[k] = (he > hc ? he - hc : hc - he) >= 0.25f * step;
    }
  }
  // the camera's side along GL axis `al` (0 x, 2 z) for the four views, +1 or -1 (Terrain::render's tile order: at angle 0 it walks
  // y upward and x downward, so the camera is toward +y (+z) and -x; at 90 toward -x / -y, at 180 toward +x / -y, at -90 toward +x / +y)
  float camera_side(int al) const {
    const int32_t v = g_.view_index;
    return al == 2 ? ((v == 0 || v == 3) ? 1.0f : -1.0f) : ((v == 0 || v == 1) ? -1.0f : 1.0f);
  }
  // the stairs halves as one span per axis (both halves of an axis: edge to edge through the centre), the y axis first
  void draw_stairs(uint32_t t, const bool stair[4], uint32_t tex, float w) {
    if (stair[0] || stair[1]) draw_stair_span(t, 2, stair[0], stair[1], tex, w);
    if (stair[2] || stair[3]) draw_stair_span(t, 0, stair[2], stair[3], tex, w);
  }
  // One axis as steps, along GL axis `al` (2: halves 0 / 1, the y axis; 0: halves 2 / 3, the x axis), from the end of the stairs
  // half on the low side (or the centre) to the end of the one on the high side (or the centre). The span's centre line (edge
  // vertex, centre vertex, edge vertex) is cut where it crosses a level (a multiple of the corner step, the height one Terrain level
  // adds: elevate / lowerCorner), so a tile's rise gives one step per level and neighbouring stairs meet on the same lattice. Each
  // piece is a flat tread at the highest point of its piece of the line (never under the slope), in the path texture with the
  // band's texture coordinates of the half it lies in (split at the centre); a riser in the retaining-wall texture (0x22,
  // RetainWallA) where two treads, or a tread and the span's end, differ; both sides of each tread closed down to the line with the
  // same wall. A vertical face takes the side normal the walls and skirts use for a face toward that neighbour (Tile::drawSkirts:
  // toward 1 -> block 0, 0 -> 1, 3 -> 2, 2 -> 3; the row by view quadrant). Only a vertical face whose side the camera is on is drawn:
  // with no depth test a face turned away, painted after its tread, covered the tread (the dark saw edges seen from the top of a
  // climb, rotations 2 / 3 on a stairs rising toward +x); a drawn face is sent in both windings (culling keeps the one facing the
  // window whatever glFrontFace says). The pieces go far to near.
  void draw_stair_span(uint32_t t, int al, bool lo_on, bool hi_on, uint32_t tex, float w) {
    const int ac = 2 - al;
    const int k_lo = al == 2 ? 0 : 2, k_hi = k_lo + 1;   // the half toward -s and the one toward +s (s = the GL coordinate al)
    const float *cv = vert(fv(t, 0, 2));
    const float *ev[2] = {vert(fv(t, stair_half(k_lo).face, stair_half(k_lo).vtx)), vert(fv(t, stair_half(k_hi).face, stair_half(k_hi).vtx))};
    const float step = g_.corner_step > 0.0f ? g_.corner_step : 15.0f, eps = 0.01f * step;
    // the line: up to three points by rising s
    float ps[3], ph[3];
    int np = 0;
    if (lo_on) { ps[np] = ev[0][al]; ph[np++] = ev[0][1]; }
    ps[np] = cv[al]; ph[np++] = cv[1];
    if (hi_on) { ps[np] = ev[1][al]; ph[np++] = ev[1][1]; }
    auto line = [&](float s) {
      for (int i = 0; i + 1 < np; ++i)
        if (s <= ps[i + 1] || i + 2 == np) return ph[i] + (ph[i + 1] - ph[i]) * (s - ps[i]) / (ps[i + 1] - ps[i]);
      return ph[0];
    };
    // the cuts: the span's ends and every level the line crosses inside a segment
    constexpr int kMax = 64;
    float cut[kMax];
    int nc = 0;
    cut[nc++] = ps[0];
    for (int i = 0; i + 1 < np; ++i) {
      const float h0 = ph[i], h1 = ph[i + 1], lo = std::min(h0, h1), hi = std::max(h0, h1);
      if (g_negctl_terrain_render == 20 || hi - lo < eps) continue;
      float seg[kMax];
      int ns = 0;
      for (int m = static_cast<int>(std::floor(lo / step)) + 1; static_cast<float>(m) * step < hi - eps && ns < kMax / 2; ++m) {
        const float hm = static_cast<float>(m) * step;
        if (hm > lo + eps) seg[ns++] = ps[i] + (ps[i + 1] - ps[i]) * (hm - h0) / (h1 - h0);
      }
      if (h1 < h0) std::reverse(seg, seg + ns);   // by rising s
      for (int j = 0; j < ns && nc < kMax - 2; ++j) cut[nc++] = seg[j];
    }
    cut[nc++] = ps[np - 1];
    float tread[kMax];
    for (int i = 0; i + 1 < nc; ++i) {
      float y = std::max(line(cut[i]), line(cut[i + 1]));
      if (cut[i] < cv[al] - eps && cv[al] + eps < cut[i + 1]) y = std::max(y, cv[1]);   // the centre vertex inside the piece
      tread[i] = y;
    }
    // half k's parameter u (0 centre, 1 edge) of s, and a point of half k: u, across side (-1 / +1), height y
    auto u_of = [&](int k, float s) {
      const float *b = ev[k == k_lo ? 0 : 1];
      return (s - cv[al]) / (b[al] - cv[al]);
    };
    auto pos = [&](int k, float u, float side, float y, float out[3]) {
      const float *b = ev[k == k_lo ? 0 : 1];
      out[al] = cv[al] + u * (b[al] - cv[al]);
      out[ac] = cv[ac] + u * (b[ac] - cv[ac]) + side * w;
      out[1] = y;
    };
    const int c = (g_.view_index == 0 || g_.view_index == 2) ? 0 : 1;
    static const int kFaceNorm[4] = {1, 0, 3, 2};
    static const float kUp[3] = {0.0f, 1.0f, 0.0f};
    // a vertical quad (four points, texture (s, t) each) facing neighbour `toward`, in both windings; skipped when the camera is on
    // its other side (neighbours 0 / 1 are -z / +z, 2 / 3 are -x / +x; control 22 draws it anyway)
    auto wall = [&](const float p[4][3], const float st[4][2], int toward) {
      const float cam = camera_side(toward < 2 ? 2 : 0), dir = (toward & 1) ? 1.0f : -1.0f;
      if (cam * dir < 0.0f && g_negctl_terrain_render != 22) return;
      gl_.bind_texture(gl::kTexture2D, g_.fence_texture0);
      for (int pass = 0; pass < 2; ++pass) {
        gl_.begin(gl::kTriangleFan);
        gl_.normal3fv(g_.side_normals[kFaceNorm[toward]][c]);
        for (int i = 0; i < 4; ++i) {
          const int j = pass == 0 ? i : 3 - i;
          gl_.tex_coord2f(st[j][0], st[j][1]);
          gl_.vertex3fv(p[j]);
        }
        gl_.end();
      }
    };
    // the half a piece of the span (s0 < s1) lies in: below the centre the -s half, above it the +s half
    auto half_of = [&](float s0, float s1) { return 0.5f * (s0 + s1) < cv[al] ? k_lo : k_hi; };
    auto riser = [&](int j) {   // at cut[j], between what lies before it and after it along s
      const float before = j == 0 ? line(cut[0]) : tread[j - 1], after = j == nc - 1 ? line(cut[nc - 1]) : tread[j];
      if (std::fabs(before - after) < eps) return;
      const float lo = std::min(before, after), hi = std::max(before, after);
      const int k = j == 0 ? half_of(cut[0], cut[1]) : j == nc - 1 ? half_of(cut[nc - 2], cut[nc - 1])
                                                                     : half_of(cut[j] - eps, cut[j] + eps);
      const float u = u_of(k, cut[j]);
      float p[4][3];
      pos(k, u, -1.0f, lo, p[0]);
      pos(k, u, 1.0f, lo, p[1]);
      pos(k, u, 1.0f, hi, p[2]);
      pos(k, u, -1.0f, hi, p[3]);
      const float top = 0.05f + (hi - lo) / 40.0f;
      const float st[4][2] = {{0.25f, 0.05f}, {0.75f, 0.05f}, {0.75f, top}, {0.25f, top}};
      wall(p, st, after < before ? k_hi : k_lo);   // it faces its lower side (+s when the ground falls that way)
    };
    // tread i over cut[i]..cut[i + 1] and its two sides, in one or two pieces (split at the centre)
    auto piece = [&](int i, float s0, float s1) {
      const int k = half_of(s0, s1);
      const StairHalf &h = stair_half(k);
      float u0 = u_of(k, s0), u1 = u_of(k, s1);
      if (u0 > u1) std::swap(u0, u1);
      const float y = tread[i];
      gl_.bind_texture(gl::kTexture2D, texture(tex, 0, 0));
      gl_.begin(gl::kTriangleFan);
      const float fan_a[4][2] = {{u0, 0}, {u0, 1}, {u1, 1}, {u1, 0}}, fan_b[4][2] = {{u0, 0}, {u1, 0}, {u1, 1}, {u0, 1}};
      const float (*fan)[2] = h.order_b ? fan_b : fan_a;
      for (int q = 0; q < 4; ++q) {
        const float u = fan[q][0];
        const int side = fan[q][1] != 0.0f ? 1 : 0;
        float p[3];
        pos(k, u, side ? 1.0f : -1.0f, y, p);
        gl_.normal3fv(kUp);
        gl_.tex_coord2f(h.tc[0][side][0] + u * (h.tc[1][side][0] - h.tc[0][side][0]),
                        h.tc[0][side][1] + u * (h.tc[1][side][1] - h.tc[0][side][1]));
        gl_.vertex3fv(p);
      }
      gl_.end();
      const float g0 = line(s0), g1 = line(s1), base = std::min(g0, g1);
      if (y - base < eps) return;
      const float ua = u_of(k, s0), ub = u_of(k, s1);
      for (int side = 0; side < 2; ++side) {
        const float sd = side ? 1.0f : -1.0f;
        float p[4][3];
        pos(k, ua, sd, g0, p[0]);
        pos(k, ub, sd, g1, p[1]);
        pos(k, ub, sd, y, p[2]);
        pos(k, ua, sd, y, p[3]);
        const float sa = (s0 - ps[0]) / 100.0f, sb = (s1 - ps[0]) / 100.0f;
        const float st[4][2] = {{sa, 0.05f + (g0 - base) / 40.0f}, {sb, 0.05f + (g1 - base) / 40.0f},
                                {sb, 0.05f + (y - base) / 40.0f}, {sa, 0.05f + (y - base) / 40.0f}};
        wall(p, st, al == 2 ? (side ? 3 : 2) : (side ? 1 : 0));
      }
    };
    auto tread_block = [&](int i, bool up) {
      const float s0 = cut[i], s1 = cut[i + 1], sc = cv[al];
      if (s0 < sc - eps && sc + eps < s1) {   // across the centre: two pieces, the far one first
        if (up) { piece(i, s0, sc); piece(i, sc, s1); }
        else { piece(i, sc, s1); piece(i, s0, sc); }
      } else {
        piece(i, s0, s1);
      }
    };
    // far to near: with the camera toward +s the far end is the low s; control 21 reverses it
    const bool up = (camera_side(al) > 0.0f) != (g_negctl_terrain_render == 21);
    const int n = nc - 1;
    for (int q = 0; q < n; ++q) {
      const int i = up ? q : n - 1 - q;
      riser(up ? i : i + 1);
      tread_block(i, up);
    }
    riser(up ? n : 0);
  }

  // Tile::drawCornerOverlays(mask, type): for an overlay mask 0..15 up to four corner triangles over the face vertices,
  // with texture slot 2 or 3 of `type`
  void draw_corner_overlays(uint32_t t, int32_t mask, int32_t type) {
    int a = 0, b = 0, c = 0, d = 0;
    if (g_negctl_terrain_render == 7) { if (mask == 5) mask = 9; else if (mask == 9) mask = 5; }
    switch (mask) {
      case 0: a = b = c = d = 3; break;
      case 1: c = d = 3; break;
      case 2: a = b = 3; break;
      case 4: a = c = 3; break;
      case 8: b = d = 3; break;
      case 5: c = 3; b = 2; break;
      case 9: d = 3; a = 2; break;
      case 7: b = d = 2; break;
      case 11: a = c = 2; break;
      case 13: a = b = 2; break;
      case 6: a = 3; d = 2; break;
      case 10: b = 3; c = 2; break;
      case 14: c = d = 2; break;
      case 15: a = b = c = d = 2; break;
      default: return;
    }
    struct Corner { int slot; float s0, t0, s1, t1, s2, t2; int f0, v0, f1, v1, f2, v2; };
    const Corner corners[4] = {
        {a, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0, 1, 0, 2, 1, 2},
        {b, 0.5f, 1.0f, 0.5f, 0.5f, 1.0f, 0.5f, 1, 2, 0, 2, 3, 1},
        {c, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f, 0.5f, 5, 1, 4, 2, 4, 0},
        {d, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 3, 1, 4, 2, 5, 1},
    };
    for (const Corner &k : corners) {
      if (k.slot == 0) continue;
      gl_.bind_texture(gl::kTexture2D, texture(static_cast<uint32_t>(type), 0, static_cast<uint32_t>(k.slot)));
      gl_.begin(gl::kTriangles);
      gl_.tex_coord2f(k.s0, k.t0);
      gl_.array_element(fv(t, k.f0, k.v0));
      gl_.tex_coord2f(k.s1, k.t1);
      gl_.array_element(fv(t, k.f1, k.v1));
      gl_.tex_coord2f(k.s2, k.t2);
      gl_.array_element(fv(t, k.f2, k.v2));
      gl_.end();
    }
  }
};

}  // namespace

bool terrain_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t tile, float angle) {
  Renderer r(ctx, gl, g, terrain);
  return r.render(tile, angle);
}

void terrain_local_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t tile, uint32_t pivot, float angle) {
  Renderer r(ctx, gl, g, terrain);
  r.local_render(tile, pivot, angle);
}

void terrain_strip_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t pivot, int32_t dir, float angle) {
  Renderer r(ctx, gl, g, terrain);
  r.strip_render(pivot, dir, angle);
}

void terrain_path_update_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t pivot, float angle) {
  Renderer r(ctx, gl, g, terrain);
  r.path_update_render(pivot, angle);
}

void terrain_render_tile_at_five_offsets(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t tile, uint32_t pivot,
                                         float angle) {
  Renderer r(ctx, gl, g, terrain);
  r.render_tile_at_five_offsets(tile, pivot, angle);
}

bool terrain_render_entry(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, int32_t entry, uint32_t terrain, uint32_t a0, uint32_t a1,
                          uint32_t a2) {
  auto f = [](uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  };
  switch (entry) {
    case kEntryStripRender: terrain_strip_render(ctx, gl, g, terrain, a0, static_cast<int32_t>(a1), f(a2)); return true;
    case kEntryRender: terrain_render(ctx, gl, g, terrain, a0, f(a1)); return true;
    case kEntryLocalRender: terrain_local_render(ctx, gl, g, terrain, a0, a1, f(a2)); return true;
    case kEntryPathUpdateRender: terrain_path_update_render(ctx, gl, g, terrain, a0, f(a1)); return true;
    case kEntryRenderTileAtFiveOffsets: terrain_render_tile_at_five_offsets(ctx, gl, g, terrain, a0, a1, f(a2)); return true;
    default: return false;
  }
}

void terrain_render_negctl(int32_t mode) { g_negctl_terrain_render = mode; }
void terrain_render_set_cull_margin(int32_t m) { g_cull_margin = m; }
void terrain_render_set_world_reach(int32_t tiles, int32_t cull_y) {
  g_world_reach = tiles;
  g_world_cull_y = cull_y;
}

// No sg_call functions yet: the renderer needs a sink and the host's globals, so it has its own C entry points (portsim_c.h).
extern const ModuleDef kModule_terrain_render = {"terrain_render", nullptr, 0};

}  // namespace portsim
