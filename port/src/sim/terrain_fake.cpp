// TEST-ONLY: a recording Terrain behind portsim/sim/terrain_api.h (phase 3.1a). Tests attach it to a context with
// `terrainFake::install(log, capacity, heap, heap_size, os_major)` and compare its call log (and the memory it changed)
// with the same fake run behind the ORIGINAL exe code in Unicorn (port/test/test_terrain.py, FakeTerrain there).
// The fake is no model of Terrain.dll: it keeps just enough state for the exe's loops to end (corner heights move by one
// per elevate / lower call) and makes every eax a pure function of the call's arguments. Its behaviour is the contract
// with the Python mirror; change both together.
//
// Log block: dword count, then per call 8 dwords {method id, a0..a5, eax} (records past `capacity` are not stored, the
// count still rises). Method ids: the order of the TerrainApi methods, 1 = pass_collar_info .. 28 = operator_new.
#include <map>
#include <memory>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/terrain_api.h"

namespace portsim {

namespace {
int g_negctl_terrain_fake = 0;  // no wrong body: the fake is test scaffolding (registered for the negctl table)

enum : uint32_t {
  kPassCollarInfo = 1, kLoadNewCourseType, kSetType, kGetVariation, kLayPath, kSetWall, kCalcNormals, kCalcAllNormals,
  kElevateCorner, kLowerCorner, kLowerEdgeCorner, kHasPath, kHasConnectedPath, kUpdatePath, kInitSystem, kInitTerrain,
  kSetZoomLevel, kRender, kLocalRender, kPathUpdateRender, kOsMajorVersion, kAcquireDc, kPumpInput, kWindowPaint,
  kBlitFull, kBlitScreen, kRenderStrip, kOperatorNew,
};

uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
uint32_t mix(uint32_t id, const uint32_t a[6]) {
  return id * 0x9E3779B1u ^ a[0] * 0x85EBCA6Bu ^ rotl(a[1], 5) ^ a[2] * 0xC2B2AE35u ^ (a[3] << 11) ^ a[4] ^ (a[5] << 17);
}

class FakeTerrain : public TerrainApi {
 public:
  FakeTerrain(const Memory &m, uint32_t log, uint32_t cap, uint32_t heap, uint32_t heap_size, uint32_t os_major)
      : m_(m), log_(log), cap_(cap), heap_(heap), heap_size_(heap_size), os_major_(os_major) {
    m_.store<uint32_t>(log_, 0);
  }

  uint32_t pass_collar_info(uint32_t, const int32_t *table, int32_t n) override {
    uint32_t h = 0x811C9DC5u;  // FNV-1a over the table's n dwords
    for (int32_t i = 0; i < n; ++i)
      for (int b = 0; b < 4; ++b) h = (h ^ ((u32(table[i]) >> (8 * b)) & 0xFF)) * 0x01000193u;
    return rec(kPassCollarInfo, {h, u32(n)});
  }
  uint32_t load_new_course_type(uint32_t, int32_t type) override { return rec(kLoadNewCourseType, {u32(type)}); }
  uint32_t set_type(uint32_t, uint32_t tile, int32_t type, int32_t var) override {
    if (tile) {
      m_.store<int32_t>(tile + 0x24, type);
      m_.store<int32_t>(tile + 0x28, var);
    }
    return rec(kSetType, {tile, u32(type), u32(var)});
  }
  uint32_t get_variation(uint32_t, uint32_t tile) override {
    return rec(kGetVariation, {tile}, tile ? m_.load<uint32_t>(tile + 0x28) : 0u, true);
  }
  uint32_t lay_path(uint32_t, uint32_t tile, int32_t f20, int32_t f40) override {
    if (tile) m_.store<uint32_t>(tile + 0x2C, (f20 ? 1u : 0u) | (f40 ? 2u : 0u));
    return rec(kLayPath, {tile, u32(f20), u32(f40)});
  }
  uint32_t set_wall(uint32_t, uint32_t tile, int32_t side, int32_t h, int32_t on) override {
    if (tile) m_.store<uint8_t>(tile + 0x234 + (u32(side) & 7), on ? static_cast<uint8_t>(h) : 0);
    return rec(kSetWall, {tile, u32(side), u32(h), u32(on)});
  }
  uint32_t calc_normals(uint32_t, uint32_t tile) override { return rec(kCalcNormals, {tile}); }
  uint32_t calc_all_normals(uint32_t, uint32_t tile) override { return rec(kCalcAllNormals, {tile}); }
  uint32_t elevate_corner(uint32_t, uint32_t tile, int32_t c) override {
    move(tile, c, +1);
    return rec(kElevateCorner, {tile, u32(c)});
  }
  uint32_t lower_corner(uint32_t, uint32_t tile, int32_t c) override {
    move(tile, c, -1);
    return rec(kLowerCorner, {tile, u32(c)});
  }
  uint32_t lower_edge_corner(uint32_t, uint32_t tile, int32_t c, uint32_t tile2, uint32_t angle) override {
    move(tile, c, -1);
    return rec(kLowerEdgeCorner, {tile, u32(c), tile2, angle});
  }
  uint32_t has_path(uint32_t, uint32_t tile) override {
    const uint32_t bit = tile ? (m_.load<uint32_t>(tile + 0x2C) & 1u) : 0u;
    return rec(kHasPath, {tile}, bit, false);
  }
  uint32_t has_connected_path(uint32_t, int32_t x, int32_t y) override {
    return rec(kHasConnectedPath, {u32(x), u32(y)}, u32(x + y) & 1u, false);
  }
  uint32_t update_path(uint32_t, int32_t x, int32_t y, int32_t f40) override {
    return rec(kUpdatePath, {u32(x), u32(y), u32(f40)});
  }
  uint32_t init_system(uint32_t, int32_t w, int32_t h, uint32_t hdc, int32_t flag) override {
    return rec(kInitSystem, {u32(w), u32(h), hdc, u32(flag) & 0xFF});
  }
  uint32_t init_terrain(uint32_t) override { return rec(kInitTerrain, {}); }
  uint32_t set_zoom_level(uint32_t, int32_t zoom) override { return rec(kSetZoomLevel, {u32(zoom)}); }
  uint32_t render(uint32_t, uint32_t tile, uint32_t angle) override { return rec(kRender, {tile, angle}); }
  uint32_t local_render(uint32_t, uint32_t tile, uint32_t centre, uint32_t angle) override {
    return rec(kLocalRender, {tile, centre, angle});
  }
  uint32_t path_update_render(uint32_t, uint32_t tile, uint32_t angle) override {
    return rec(kPathUpdateRender, {tile, angle});
  }
  uint32_t os_major_version() override { return rec(kOsMajorVersion, {}, os_major_, true); }
  uint32_t acquire_dc() override { return rec(kAcquireDc, {}, 0xDC01u, true); }
  uint32_t pump_input() override { return rec(kPumpInput, {}); }
  uint32_t window_paint() override { return rec(kWindowPaint, {}); }
  uint32_t blit_full() override { return rec(kBlitFull, {}); }
  uint32_t blit_screen(int32_t w, int32_t h) override { return rec(kBlitScreen, {u32(w), u32(h)}); }
  uint32_t render_strip(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t rot, int32_t zoom) override {
    return rec(kRenderStrip, {u32(x), u32(y), u32(dx), u32(dy), u32(rot), u32(zoom)});
  }
  uint32_t operator_new(uint32_t size) override {
    const uint32_t p = heap_ + used_;
    used_ += (size + 15u) & ~15u;
    if (used_ > heap_size_) throw Fault{p, size};
    return rec(kOperatorNew, {size}, p, true);
  }

 private:
  // corner c's height (+4 + 8 * ((c - 1) / 2 & 3)) moves by d; a NULL tile faults (the loops around it would not end)
  void move(uint32_t tile, int32_t c, int32_t d) {
    if (!tile) throw Fault{0, 4};
    const uint32_t at = tile + 4 + 8 * ((u32(c - 1) >> 1) & 3u);
    m_.store<int32_t>(at, add32(m_.load<int32_t>(at), d));
  }
  // logs the call; eax = mix(id, args), or `value` (whole eax when `whole`, else al over mix's upper bytes)
  uint32_t rec(uint32_t id, std::initializer_list<uint32_t> args, uint32_t value = 0, bool whole = false,
               bool use_value = true) {
    uint32_t a[6] = {};
    int k = 0;
    for (uint32_t v : args) a[k++] = v;
    uint32_t eax = mix(id, a);
    if (use_value && (whole || id == kHasPath || id == kHasConnectedPath)) eax = whole ? value : ((eax & ~0xFFu) | value);
    const uint32_t n = m_.load<uint32_t>(log_);
    if (n < cap_) {
      const uint32_t r = log_ + 4 + n * 32;
      m_.store<uint32_t>(r, id);
      for (int i = 0; i < 6; ++i) m_.store<uint32_t>(r + 4 + 4 * u32(i), a[i]);
      m_.store<uint32_t>(r + 28, eax);
    }
    m_.store<uint32_t>(log_, n + 1);
    return eax;
  }

  const Memory &m_;
  uint32_t log_, cap_, heap_, heap_size_, os_major_;
  uint32_t used_ = 0;
};

std::map<const Ctx *, std::unique_ptr<FakeTerrain>> &fakes() {
  static std::map<const Ctx *, std::unique_ptr<FakeTerrain>> m;
  return m;
}

void a_install(Ctx &c, const uint32_t *a, Ret &r) {
  set_terrain_api(c, nullptr);
  auto f = std::make_unique<FakeTerrain>(c.mem, a[0], a[1], a[2], a[3], a[4]);
  set_terrain_api(c, f.get());
  fakes()[&c] = std::move(f);
  r.set_u32(0);
}
void a_remove(Ctx &c, const uint32_t *, Ret &r) {
  set_terrain_api(c, nullptr);
  fakes().erase(&c);
  r.set_u32(0);
}

const FuncDef kFuncs[] = {
    {"terrainFake::install", "default", "int", 5, 0, a_install, &g_negctl_terrain_fake},
    {"terrainFake::remove", "default", "int", 0, 0, a_remove, &g_negctl_terrain_fake},
};
}  // namespace

extern const ModuleDef kModule_terrain_fake = {"terrain_fake", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
