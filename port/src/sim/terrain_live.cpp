// LiveTerrain: the TerrainApi over the port's own Terrain.dll state (terrain_live.h). Tests: port/test/test_terrain_live.py, which
// boots the port on a pinned scenario's trace with this interface attached and compares the calls and the state with a recording
// of the original on the same scenario (re/tools/terrain_calls.py --scenario).
#define _CRT_SECURE_NO_WARNINGS  // the development switch SG_LIVE_GETTERS uses getenv
#include "portsim/sim/terrain_live.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "portsim/ptr.h"
#include "portsim/sim/tick.h"

namespace portsim {

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
}

LiveTerrain::~LiveTerrain() = default;

void LiveTerrain::keep_snapshots() {
  while (next_snap_ < snap_after_.size() && snap_after_[next_snap_] <= log_.size()) {
    Snapshot s;
    s.after = static_cast<uint32_t>(log_.size());
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
  // the recorder's line: seq tid depth name ecx a0..a4 ret [tbl=...]
  std::string line = std::to_string(log_.size()) + " 0 0 " + name + " " + hex(self);
  for (size_t i = 0; i < 5; ++i) line += " " + (i < k ? hex(a[i]) : std::string("-"));
  line += " " + hex(ret);
  if (table) {
    line += " tbl=";
    for (int32_t i = 0; i < n; ++i) line += (i ? "," : "") + std::to_string(table[i]);
  }
  if (std::strcmp(name, "setType") == 0 && a[0]) line += " var=" + std::to_string(Ptr<TerrainTile>(ctx_.mem, a[0]).field(&TerrainTile::variation).get());
  log_.push_back(std::move(line));
  keep_snapshots();
  return ret;
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
uint32_t LiveTerrain::load_new_course_type(uint32_t self, int32_t type) { return call("loadNewCourseType", self, {static_cast<uint32_t>(type)}); }
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
uint32_t LiveTerrain::set_zoom_level(uint32_t self, int32_t zoom) { return call("setZoomLevel", self, {static_cast<uint32_t>(zoom)}); }
uint32_t LiveTerrain::render(uint32_t self, uint32_t tile, uint32_t angle) { return call("render", self, {tile, angle}); }
uint32_t LiveTerrain::local_render(uint32_t self, uint32_t tile, uint32_t centre, uint32_t angle) {
  return call("localRender", self, {tile, centre, angle});
}
uint32_t LiveTerrain::path_update_render(uint32_t self, uint32_t tile, uint32_t angle) {
  return call("pathUpdateRender", self, {tile, angle});
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
uint32_t LiveTerrain::window_paint() { return engine_->window_paint(); }
uint32_t LiveTerrain::blit_full() { return engine_->blit_full(); }
uint32_t LiveTerrain::blit_screen(int32_t w, int32_t h) { return engine_->blit_screen(w, h); }
uint32_t LiveTerrain::render_strip(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t rot, int32_t zoom) {
  return engine_->render_strip(x, y, dx, dy, rot, zoom);
}
uint32_t LiveTerrain::operator_new(uint32_t size) { return engine_->operator_new(size); }

extern const ModuleDef kModule_terrain_live = {"terrain_live", nullptr, 0};

}  // namespace portsim
