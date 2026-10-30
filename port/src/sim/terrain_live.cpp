// LiveTerrain: the TerrainApi over the port's own Terrain.dll state (terrain_live.h). Tests: port/test/test_terrain_live.py, which
// boots the port on a pinned scenario's trace with this interface attached and compares the calls and the state with a recording
// of the original on the same scenario (re/tools/terrain_calls.py --scenario).
#define _CRT_SECURE_NO_WARNINGS  // the development switch SG_LIVE_GETTERS uses getenv
#include "portsim/sim/terrain_live.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/terrain_render.h"
#include "portsim/sim/tick.h"

namespace portsim {

extern int g_negctl_gfx_terrain;   // gfx_terrain.cpp

namespace {
// The display surface's DC that initSystem receives: a headless run's engine has none (0), the port's display does exist (the
// rasteriser draws the terrain), so the interface hands initSystem a nonzero token in its place. Terrain.dll uses it only to create
// its GL context (initSystem tests it against 0).
constexpr uint32_t kDcToken = 1;

std::string hex(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%x", v);
  return b;
}
}  // namespace

LiveTerrain::LiveTerrain(Ctx &ctx, uint32_t va, std::vector<uint32_t> snap_after)
    : ctx_(ctx), engine_(make_headless_terrain(ctx)), snap_after_(std::move(snap_after)) {
  dll_.terrain = va;
  g_.vertices.assign(static_cast<size_t>(22500) * 3, 0.0f);   // the DLL's bss arrays: 22,500 vertices each
  g_.normals.assign(static_cast<size_t>(22500) * 3, 0.0f);
  // the rasteriser the GL setup also goes to (the window size initTerrain passes is the screen's: 800 x 600, U-1310); the
  // constructor's texture load runs before any context exists, like the original's
  raster_ = std::make_unique<gl::GlRaster>(800, 600);
  raster_->no_context();
  dll_.setup.b = raster_.get();
}

LiveTerrain::~LiveTerrain() = default;

void LiveTerrain::keep_snapshots() {
  while (next_snap_ < snap_after_.size() && snap_after_[next_snap_] <= calls_) {
    Snapshot s;
    s.after = static_cast<uint32_t>(calls_);
    s.object = object_;
    s.vertices = g_.vertices;
    s.normals = g_.normals;
    s.textures = g_.textures;
    s.view_index = g_.view_index;
    snaps_.push_back(std::move(s));
    ++next_snap_;
  }
}

uint32_t LiveTerrain::call(const char *name, uint32_t self, std::initializer_list<uint32_t> args, const int32_t *table, int32_t n) {
  uint32_t a[5] = {0, 0, 0, 0, 0};
  size_t k = 0;
  for (uint32_t v : args) a[k++] = v;
  const uint32_t ret = terrain_dll_call(ctx_, g_, dll_, name, self, a, table, n);
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 6) keep_log_ = false;   // control 6: the log kept off in a run that reads it
  if (keep_log_) {
    LogRec r{static_cast<uint64_t>(log_size()), name, self, {a[0], a[1], a[2], a[3], a[4]}, ret, static_cast<uint8_t>(k), false, 0,
             {}, table != nullptr};
    if (table) r.tbl.assign(table, table + (n > 0 ? n : 0));
    // control 7: the compact record loses setType's variation (the lazy line then prints no `var=`)
    if (std::strcmp(name, "setType") == 0 && a[0] && SG_NEGCTL(g_negctl_gfx_terrain) != 7) {
      r.has_var = true;
      r.var = Ptr<TerrainTile>(ctx_.mem, a[0]).field(&TerrainTile::variation).get();
    }
    recs_.push_back(std::move(r));
  }
  ++calls_;
  keep_snapshots();
  return ret;
}

const std::vector<std::string> &LiveTerrain::log() const {
  // the recorder's line: seq tid depth name ecx a0..a4 ret [tbl=...] [var=...]; the pending records move into the lines
  for (const LogRec &r : recs_) {
    std::string line = std::to_string(r.seq) + " 0 0 " + r.name + " " + hex(r.self);
    for (size_t i = 0; i < 5; ++i) line += " " + (i < r.k ? hex(r.a[i]) : std::string("-"));
    line += " " + hex(r.ret);
    if (r.has_tbl) {
      line += " tbl=";
      for (size_t i = 0; i < r.tbl.size(); ++i) line += (i ? "," : "") + std::to_string(r.tbl[i]);
    }
    if (r.has_var) line += " var=" + std::to_string(r.var);
    log_.push_back(std::move(line));
  }
  recs_.clear();
  return log_;
}

void LiveTerrain::drop_log() {
  std::vector<std::string>().swap(log_);
  std::vector<LogRec>().swap(recs_);
}

uint32_t LiveTerrain::get_instance() {
  if (!dll_.constructed) {
    object_.assign(kTerrainObjectSize, kNewFill);   // operator new's fill (debug CRT)
    uint32_t va = dll_.terrain;
    if (va == 0 || !ctx_.mem.map(va, object_.data(), kTerrainObjectSize, false)) {
      va = 0;
      uint32_t base = (ctx_.mem.data_base() + static_cast<uint32_t>(sizeof(GameState)) + (32u << 20)) & ~0xFFFFu;
      for (int tries = 0; tries < 64 && va == 0; ++tries, base += (4u << 20))
        if (ctx_.mem.map(base, object_.data(), kTerrainObjectSize, false)) va = base;
      if (va == 0) throw MissingResource{"LiveTerrain: no free range for the Terrain object"};
    }
    dll_.terrain = va;
  }
  return call("getInstance", 0, {});
}

uint32_t LiveTerrain::pass_collar_info(uint32_t self, const int32_t *table, int32_t n) {
  return call("passCollarInfo", self, {0u, static_cast<uint32_t>(n)}, table, n);
}
// negative controls 1 / 2 of the terrain body family (gfx_terrain.cpp): the rasteriser misses setZoomLevel's projection or
// loadNewCourseType's texture reload
uint32_t LiveTerrain::load_new_course_type(uint32_t self, int32_t type) {
  gl::GlSink *const b = dll_.setup.b;
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 2) dll_.setup.b = nullptr;
  const uint32_t r = call("loadNewCourseType", self, {static_cast<uint32_t>(type)});
  dll_.setup.b = b;
  return r;
}
uint32_t LiveTerrain::set_type(uint32_t self, uint32_t tile, int32_t type, int32_t variation) {
  return call("setType", self, {tile, static_cast<uint32_t>(type), static_cast<uint32_t>(variation)});
}
uint32_t LiveTerrain::lay_path(uint32_t self, uint32_t tile, int32_t flag20, int32_t flag40) {
  return call("layPath", self, {tile, static_cast<uint32_t>(flag20), static_cast<uint32_t>(flag40)});
}
uint32_t LiveTerrain::set_wall(uint32_t self, uint32_t tile, int32_t side, int32_t height, int32_t on) {
  return call("setWall", self, {tile, static_cast<uint32_t>(side), static_cast<uint32_t>(height), static_cast<uint32_t>(on)});
}
uint32_t LiveTerrain::calc_normals(uint32_t self, uint32_t tile) { return call("calcNormals", self, {tile}); }
uint32_t LiveTerrain::calc_all_normals(uint32_t self, uint32_t tile) { return call("calcAllNormals", self, {tile}); }
uint32_t LiveTerrain::elevate_corner(uint32_t self, uint32_t tile, int32_t corner) {
  return call("elevateCorner", self, {tile, static_cast<uint32_t>(corner)});
}
uint32_t LiveTerrain::lower_corner(uint32_t self, uint32_t tile, int32_t corner) {
  return call("lowerCorner", self, {tile, static_cast<uint32_t>(corner)});
}
uint32_t LiveTerrain::lower_edge_corner(uint32_t self, uint32_t tile, int32_t corner, uint32_t tile2, uint32_t angle) {
  return call("lowerEdgeCorner", self, {tile, static_cast<uint32_t>(corner), tile2, angle});
}
uint32_t LiveTerrain::update_path(uint32_t self, int32_t x, int32_t y, int32_t flag40) {
  return call("updatePath", self, {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(flag40)});
}
uint32_t LiveTerrain::init_system(uint32_t self, int32_t w, int32_t h, uint32_t hdc, int32_t flag) {
  return call("initSystem", self, {static_cast<uint32_t>(w), static_cast<uint32_t>(h), hdc, static_cast<uint32_t>(flag)});
}
uint32_t LiveTerrain::init_terrain(uint32_t self) { return call("initTerrain", self, {}); }
uint32_t LiveTerrain::reset_terrain(uint32_t self) { return call("resetTerrain", self, {}); }
uint32_t LiveTerrain::set_zoom_level(uint32_t self, int32_t zoom) {
  gl::GlSink *const b = dll_.setup.b;
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 1) dll_.setup.b = nullptr;
  const uint32_t r = call("setZoomLevel", self, {static_cast<uint32_t>(zoom)});
  dll_.setup.b = b;
  return r;
}
// The draw passes: the state side (the view quadrant) through terrain_dll_call, then the drawing through the backend (a pass draws
// nothing into the address space, so the order of the two does not matter to the state).
uint32_t LiveTerrain::render(uint32_t self, uint32_t tile, uint32_t angle) {
  const uint32_t r = call("render", self, {tile, angle});
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryRender), self, tile, angle, 0});
  return r;
}
uint32_t LiveTerrain::local_render(uint32_t self, uint32_t tile, uint32_t centre, uint32_t angle) {
  const uint32_t r = call("localRender", self, {tile, centre, angle});
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryLocalRender), self, tile, centre, angle});
  return r;
}
uint32_t LiveTerrain::path_update_render(uint32_t self, uint32_t tile, uint32_t angle) {
  const uint32_t r = call("pathUpdateRender", self, {tile, angle});
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryPathUpdateRender), self, tile, angle, 0});
  return r;
}

// a drop of the terrain body family (gfx_terrain.cpp) when a backend is attached; false otherwise or when it was not handled
bool LiveTerrain::to_gfx(const char *name, std::initializer_list<uint32_t> args) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0 || gfx(ctx_) == nullptr) return false;
  uint32_t a[12] = {};
  int n = 0;
  for (uint32_t v : args)
    if (n < 12) a[n++] = v;
  return gfx_drop(ctx_, name, a, n);
}

void LiveTerrain::frame_begin() {
  view_.in_frame = true;
  view_.passes_in_frame = 0;
  to_gfx("terrain.frameBegin", {view_.frames});
}
void LiveTerrain::frame_end() {
  to_gfx("terrain.frameEnd", {view_.frames});
  view_.in_frame = false;
  ++view_.frames;
}
void terrain_live_frame(Ctx &ctx, bool begin) {
  if (!has_terrain_api(ctx)) return;
  if (auto *lt = dynamic_cast<LiveTerrain *>(&terrain_api(ctx))) {
    if (begin) lt->frame_begin();
    else lt->frame_end();
  }
}

// The getters (not logged, like the recorder): Tile::getVariation's char (movsx), Tile::hasPath's and isConnected's bool
// development switch SG_LIVE_GETTERS=zero: the getters answer 0 like the headless stand-in (to find what a getter's value changes)
bool getters_zero() {
  const char *e = std::getenv("SG_LIVE_GETTERS");
  return e && std::strcmp(e, "zero") == 0;
}
uint32_t LiveTerrain::get_variation(uint32_t, uint32_t tile) {
  if (getters_zero()) return 0;
  return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(Ptr<uint8_t>(ctx_.mem, tile + 0x28).get())));
}
uint32_t LiveTerrain::has_path(uint32_t, uint32_t tile) {
  if (getters_zero()) return 0;
  return Ptr<TerrainTile>(ctx_.mem, tile).field(&TerrainTile::has_path).get();
}
uint32_t LiveTerrain::has_connected_path(uint32_t self, int32_t x, int32_t y) {
  if (getters_zero()) return 0;
  return Ptr<TerrainTile>(ctx_.mem, terrain_tile_at(ctx_, self, x, y)).field(&TerrainTile::connected).get();
}

// engine services: a headless run's
uint32_t LiveTerrain::os_major_version() { return engine_->os_major_version(); }
uint32_t LiveTerrain::acquire_dc() {
  const uint32_t dc = engine_->acquire_dc();
  return dc != 0 ? dc : kDcToken;
}
uint32_t LiveTerrain::pump_input() { return engine_->pump_input(); }
// Window::paint(root window, 0x80000000): the root surface cleared (gfx_terrain.cpp); the engine's return value is the result
uint32_t LiveTerrain::window_paint() {
  const uint32_t r = engine_->window_paint();
  to_gfx("terrain.windowPaint", {});
  return r;
}
// The whole-surface copies (Surface_blitThunk): the root surface to the back surface ending a render, and the back surface to the
// root surface when the camera has not moved. terrain.blit's arguments: direction (1 = root to back), source x, y, destination
// x, y, width, height, and 1 when the strip's screen size was not one renderTerrainStrip knows.
uint32_t LiveTerrain::blit_full() {
  const uint32_t r = engine_->blit_full();
  const int32_t w = ctx_.gs().g_terrain820f30[0], h = ctx_.gs().g_terrain820f30[1];
  to_gfx("terrain.blit", {1, 0, 0, 0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 0});
  return r;
}
uint32_t LiveTerrain::blit_screen(int32_t w, int32_t h) {
  const uint32_t r = engine_->blit_screen(w, h);
  to_gfx("terrain.blit", {0, 0, 0, 0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 0});
  return r;
}

// renderTerrainStrip(surf, x, y, dx, dy, rot, zoom) (re/analysis/terrain/0044a6e0_renderTerrainStrip.md, the C3 note of the exe
// function): Terrain::stripRender(tileAt(x, y), D, angle(rot)) draws the strip the step uncovered, then Surface_blitThunk copies the
// back surface's rectangle at (sx, sy) of size (cw, ch) onto the root surface at (tx, ty): the old picture shifted by the step.
//   The screen size g_terrain820f30 picks four widths A, L10, L14, L18 (800 x 600: 0x20, 0x14, 0x28, 0x40; 1024 x 768: 0x28,
//   0x1a, 0x34, 0x50; 1280 x 1024: 0x34, 0x22, 0x44, 0x68; any other size leaves A = rot and the other three uninitialised:
//   reported by the blit body); the shift s = 2 / 1 / 0 for zoom 1 / 2 / 4, else rot (a `sar` count: its low five bits);
//   r2 = 6 for rot 2, 2 for rot 6, else rot. The step gives D: (-2,0), (-1,0) 7; (-1,-1) 8; (-1,1) 6; (0,-2), (0,-1) 1;
//   (0,1), (0,2) 5; (1,-1) 2; (1,0), (2,0) 3; (1,1) 4; any other step leaves D = zoom. e = D + r2, folded to e % 9 + 1 above 8,
//   selects the rectangle; m is |dx| or |dy| as each case says (which one depends on rot being 0 or 4); any other e passes rot as
//   the four position values and (dy, dx) as the size.
// The stripRender call is not logged (the recorder's comparison keeps the TerrainApi methods only); its state effect, the view
// quadrant, is applied. The return value is the engine's (the original's is the blit's, which updateTerrainRender drops).
uint32_t LiveTerrain::render_strip(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t rot, int32_t zoom) {
  const uint32_t r = engine_->render_strip(x, y, dx, dy, rot, zoom);
  const GameState &gs = ctx_.gs();
  const int32_t w = gs.g_terrain820f30[0], h = gs.g_terrain820f30[1];
  int32_t A = rot, L10 = 0, L14 = 0, L18 = 0;
  bool known = true;
  if (w == 800 && h == 600) {
    A = 0x20; L10 = 0x14; L14 = 0x28; L18 = 0x40;
  } else if (w == 1024 && h == 768) {
    A = 0x28; L10 = 0x1a; L14 = 0x34; L18 = 0x50;
  } else if (w == 1280 && h == 1024) {
    A = 0x34; L10 = 0x22; L14 = 0x44; L18 = 0x68;
  } else {
    known = false;
  }
  const int32_t s = (zoom == 1 ? 2 : zoom == 2 ? 1 : zoom == 4 ? 0 : rot) & 31;
  const int32_t r2 = rot == 2 ? 6 : rot == 6 ? 2 : rot;
  int32_t D = zoom;
  if (dx == -2) {
    if (dy == 0) D = 7;
  } else if (dx == -1) {
    if (dy == -1) D = 8;
    else if (dy == 0) D = 7;
    else if (dy == 1) D = 6;
  } else if (dx == 0) {
    if (dy == -2 || dy == -1) D = 1;
    else if (dy == 1 || dy == 2) D = 5;
  } else if (dx == 1) {
    if (dy == -1) D = 2;
    else if (dy == 0) D = 3;
    else if (dy == 1) D = 4;
  } else if (dx == 2) {
    if (dy == 0) D = 3;
  }
  int32_t e = add32(D, r2);
  if (e > 8) e = add32(mod32(e, 9), 1);
  const bool side = rot != 0 && rot != 4;
  const int32_t ax = abs32(dx), ay = abs32(dy);
  const int32_t As = A >> s, L10s = L10 >> s;
  int32_t sx = 0, sy = 0, tx = 0, ty = 0, cw = dy, ch = dx;
  switch (e) {
    case 1: {
      const int32_t m = side ? ax : ay;
      tx = mul32(As, m); ty = mul32(L10s, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 2: ty = L14 >> s; cw = w; ch = sub32(h, ty); break;
    case 3: {
      const int32_t m = side ? ay : ax;
      sx = mul32(As, m); ty = mul32(L10s, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 4: sx = L18 >> s; cw = sub32(w, sx); ch = h; break;
    case 5: {
      const int32_t m = side ? ax : ay;
      sx = mul32(As, m); sy = mul32(L10s, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 6: sy = L14 >> s; cw = w; ch = sub32(h, sy); break;
    case 7: {
      const int32_t m = side ? ay : ax;
      sy = mul32(L10s, m); tx = mul32(As, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 8: tx = L18 >> s; cw = sub32(w, tx); ch = h; break;
    default: sx = sy = tx = ty = rot; break;
  }
  const uint32_t self = gs.gTerrain;
  const uint32_t pivot = terrain_tile_at_exe(x, y);
  const uint32_t angle = rot == 2 ? 0x42B40000u : rot == 4 ? 0x43340000u : rot == 6 ? 0xC2B40000u : 0u;   // 90, 180, -90 or 0
  const uint32_t sa[5] = {pivot, static_cast<uint32_t>(D), angle, 0, 0};
  terrain_dll_call(ctx_, g_, dll_, "stripRender", self, sa, nullptr, 0);
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryStripRender), self, pivot, static_cast<uint32_t>(D), angle});
  to_gfx("terrain.blit", {0, static_cast<uint32_t>(sx), static_cast<uint32_t>(sy), static_cast<uint32_t>(tx),
                          static_cast<uint32_t>(ty), static_cast<uint32_t>(cw), static_cast<uint32_t>(ch), known ? 0u : 1u});
  return r;
}

// the exe's tileAt over gTerrain (renderTerrainStrip's inline copy): 0 outside the object's width / height
uint32_t LiveTerrain::terrain_tile_at_exe(int32_t x, int32_t y) {
  const uint32_t t = ctx_.gs().gTerrain;
  const auto hp = Ptr<TerrainObjectHeader>(ctx_.mem, t);
  const int32_t w = hp.field(&TerrainObjectHeader::width).get();
  if (x >= w || x < 0 || y >= hp.field(&TerrainObjectHeader::height).get() || y < 0) return 0;
  return t + kTerrainTilesOffset + static_cast<uint32_t>(add32(x, mul32(y, w))) * static_cast<uint32_t>(sizeof(TerrainTile));
}
uint32_t LiveTerrain::operator_new(uint32_t size) { return engine_->operator_new(size); }

// Harness entries (roadmap S3.2), by sg_call: terrainLive.keepFrame(k) asks the terrain body family to copy the root surface around
// updateTerrainRender's k-th call (from 0) into `$gfx/terrain_<k>_*.555`; terrainLive.passes / terrainLive.pixels return the passes
// rasterised and the pixels they wrote (low dword); terrainLive.frames the updateTerrainRender calls seen. A context whose TerrainApi
// is not a LiveTerrain throws MissingResource.
namespace {
LiveTerrain &need_live(Ctx &c) {
  LiveTerrain *lt = has_terrain_api(c) ? dynamic_cast<LiveTerrain *>(&terrain_api(c)) : nullptr;
  if (!lt) throw MissingResource{"terrainLive: no LiveTerrain attached"};
  return *lt;
}
void a_keep_frame(Ctx &c, const uint32_t *a, Ret &r) {
  need_live(c).view().keep_frames.insert(a[0]);
  r.set_u32(0);
}
void a_passes(Ctx &c, const uint32_t *, Ret &r) { r.set_u32(static_cast<uint32_t>(need_live(c).view().passes)); }
void a_pixels(Ctx &c, const uint32_t *, Ret &r) { r.set_u32(static_cast<uint32_t>(need_live(c).view().pixels_drawn)); }
void a_frames(Ctx &c, const uint32_t *, Ret &r) { r.set_u32(need_live(c).view().frames); }
const FuncDef kFuncs[] = {
    {"terrainLive.keepFrame", "default", "int", 1, 0, a_keep_frame, nullptr},
    {"terrainLive.passes", "default", "int", 0, 0, a_passes, nullptr},
    {"terrainLive.pixels", "default", "int", 0, 0, a_pixels, nullptr},
    {"terrainLive.frames", "default", "int", 0, 0, a_frames, nullptr},
};
}  // namespace

extern const ModuleDef kModule_terrain_live = {"terrain_live", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
