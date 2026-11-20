// P39A1: the two-tile scenic bridge's table, its building and removal, and its sidecar file (crossings.h). bridge.cpp is the view
// bridge (P8B); this module is the course's crossings.
#include "crossings.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/world.h"

namespace portsim {

int g_defect_two_tile_bridge = 1;
thread_local bool t_crossings_in_undo = false;
thread_local int32_t t_crossings_redo_axis = -1;

namespace {
int g_negctl_crossings = 0;   // 1 = the land test at the ends skipped, 2 = the sidecar not read back, 3 = the partner not demolished

constexpr int8_t kWater = 0x11, kStream = 0x0A;
constexpr uint8_t kBridgeCode = 0x93;   // setGrids' code for the bridge tool (0x80 | 0x13), as H2_Build case 5 writes it
constexpr char kMagic[4] = {'S', 'G', 'X', 'B'};
constexpr uint32_t kVersion = 1;
constexpr const char *kSidecarExt = ".sgx";

struct State {
  std::vector<Crossing> list;
  int32_t style = 0;
};
State &state(Ctx &ctx) {
  if (!ctx.crossings) ctx.crossings = std::make_shared<State>();
  return *static_cast<State *>(ctx.crossings.get());
}

Ret peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  const FuncDef *f = registry_at(i, nullptr);
  uint32_t a[16] = {0};
  size_t n = 0;
  for (uint32_t v : args) a[n++] = v;
  Ret r;
  f->fn(ctx, a, r);
  return r;
}

bool on_grid(Ctx &ctx, int32_t x, int32_t y) { return x >= 0 && y >= 0 && x < world_n(ctx) && y < world_n(ctx); }
int8_t type_at(Ctx &ctx, int32_t x, int32_t y) { return grid_ref(ctx, &GameState::g_tileType, tile_index(ctx, x, y)); }
uint16_t &flags_at(Ctx &ctx, int32_t x, int32_t y) { return grid_ref(ctx, &GameState::g_tileFlags, tile_index(ctx, x, y)); }
// land a bridge may end on: on the grid, not water (0x11..0x13), not a stream, not land out of the course (0x14)
bool land_at(Ctx &ctx, int32_t x, int32_t y) {
  if (!on_grid(ctx, x, y)) return false;
  const int8_t t = type_at(ctx, x, y);
  return t != kStream && t != kWater && t != 0x12 && t != 0x13 && t != 0x14;
}
void put_u32(std::vector<uint8_t> &b, uint32_t v) {
  for (int k = 0; k < 4; ++k) b.push_back(static_cast<uint8_t>(v >> (8 * k)));
}
uint32_t get_u32(const std::vector<uint8_t> &b, size_t at) {
  uint32_t v = 0;
  for (int k = 0; k < 4; ++k) v |= static_cast<uint32_t>(b[at + k]) << (8 * k);
  return v;
}
}  // namespace

const std::vector<Crossing> &crossings_list(Ctx &ctx) { return state(ctx).list; }

int32_t crossing_at(Ctx &ctx, int32_t x, int32_t y, int32_t *part) {
  const std::vector<Crossing> &l = state(ctx).list;
  for (size_t i = 0; i < l.size(); ++i) {
    const Crossing &c = l[i];
    for (int32_t p = 0; p < 2; ++p)
      if (c.x + p * crossing_dx(c) == x && c.y + p * crossing_dy(c) == y) {
        if (part) *part = p;
        return static_cast<int32_t>(i);
      }
  }
  return -1;
}

bool crossing_tile_open(Ctx &ctx, int32_t x, int32_t y) {
  if (!on_grid(ctx, x, y)) return false;
  const int8_t t = type_at(ctx, x, y);
  if (t != kWater && t != kStream) return false;
  if (flags_at(ctx, x, y) & ((1u << 5) | (1u << 15))) return false;   // a path already (or the refusal bit)
  return crossing_at(ctx, x, y) < 0;
}

bool crossing_fit(Ctx &ctx, int32_t x, int32_t y, int32_t axis, Crossing &out) {
  const int32_t dx = axis == 0 ? 1 : 0, dy = axis == 0 ? 0 : 1;
  for (int32_t first = 0; first < 2; ++first) {   // the clicked tile as the pair's first tile, then as its second
    const int32_t x0 = x - first * dx, y0 = y - first * dy, x1 = x0 + dx, y1 = y0 + dy;
    if (!crossing_tile_open(ctx, x0, y0) || !crossing_tile_open(ctx, x1, y1)) continue;
    if (SG_NEGCTL(g_negctl_crossings) != 1 && (!land_at(ctx, x0 - dx, y0 - dy) || !land_at(ctx, x1 + dx, y1 + dy))) continue;
    out = Crossing{};
    out.x = static_cast<int16_t>(x0);
    out.y = static_cast<int16_t>(y0);
    out.axis = static_cast<uint8_t>(axis);
    out.style = static_cast<uint8_t>(state(ctx).style & 7);
    out.kind = static_cast<uint8_t>((type_at(ctx, x0, y0) == kStream ? 1 : 0) | (type_at(ctx, x1, y1) == kStream ? 2 : 0));
    return true;
  }
  return false;
}

bool crossing_choose(Ctx &ctx, int32_t x, int32_t y, int32_t prefer_axis, Crossing &out) {
  return crossing_fit(ctx, x, y, prefer_axis & 1, out) || crossing_fit(ctx, x, y, (prefer_axis & 1) ^ 1, out);
}

void crossing_build(Ctx &ctx, const Crossing &c, int32_t price_each) {
  for (int32_t p = 0; p < 2; ++p) {
    const int32_t x = c.x + p * crossing_dx(c), y = c.y + p * crossing_dy(c);
    uint16_t &w = flags_at(ctx, x, y);
    if ((c.kind >> p) & 1) w = static_cast<uint16_t>(w | (1u << 5));   // a stream: the path bit only (no scenic thought)
    else w = static_cast<uint16_t>(static_cast<uint32_t>(c.style & 7) | (w & 0xE7F8u) | (1u << 8) | (1u << 5));  // H2_Build case 5's bits
    peer(ctx, "setGrids", {static_cast<uint32_t>(x), static_cast<uint32_t>(y), kBridgeCode, static_cast<uint32_t>(price_each)});
  }
  crossing_add(ctx, c);
}

void crossing_add(Ctx &ctx, const Crossing &c) { state(ctx).list.push_back(c); }
bool crossing_erase(Ctx &ctx, int32_t index) {
  std::vector<Crossing> &l = state(ctx).list;
  if (index < 0 || index >= static_cast<int32_t>(l.size())) return false;
  l.erase(l.begin() + index);
  return true;
}
void crossings_clear(Ctx &ctx) { state(ctx).list.clear(); }
int32_t crossings_style(Ctx &ctx) { return state(ctx).style; }
void crossings_set_style(Ctx &ctx, int32_t style) { state(ctx).style = style & 7; }
int32_t crossings_pref_axis(Ctx &ctx) {
  const GameState &gs = ctx.gs();
  return (gs.g_ui_5a34e4[3] + gs.g_cameraRot / 2) & 1;
}

void crossings_on_demolish(Ctx &ctx, int32_t x, int32_t y) {
  int32_t part = 0;
  const int32_t i = crossing_at(ctx, x, y, &part);
  if (i < 0) return;
  const Crossing c = state(ctx).list[static_cast<size_t>(i)];
  crossing_erase(ctx, i);
  if (t_crossings_in_undo || SG_NEGCTL(g_negctl_crossings) == 3) return;   // the undo journal reverts each tile itself
  const int32_t ox = c.x + (1 - part) * crossing_dx(c), oy = c.y + (1 - part) * crossing_dy(c);
  if (grid_ref(ctx, &GameState::g_tileBuildCode, tile_index(ctx, ox, oy)) == kBridgeCode) peer(ctx, "demolishObject", {u32(ox), u32(oy), 0});
}

// ------------------------------------------------------------------------------------------------ the sidecar
// Layout (little-endian): "SGXB", the version (1), the world's N, the count, then 8 bytes per bridge: x (u16), y (u16), axis,
// style, kind, 0. A load keeps an entry only when both its tiles are on the grid and carry the path bit with the bridge code 0x93
// (a save the original game changed since drops what it removed).
std::string crossings_sidecar_path(const std::string &save_path) {
  const size_t dot = save_path.find_last_of('.');
  const size_t slash = save_path.find_last_of("/\\");
  const std::string stem = dot != std::string::npos && (slash == std::string::npos || dot > slash) ? save_path.substr(0, dot) : save_path;
  return stem + kSidecarExt;
}

std::vector<uint8_t> crossings_encode(Ctx &ctx) {
  std::vector<uint8_t> b(kMagic, kMagic + 4);
  const std::vector<Crossing> &l = state(ctx).list;
  put_u32(b, kVersion);
  put_u32(b, static_cast<uint32_t>(world_n(ctx)));
  put_u32(b, static_cast<uint32_t>(l.size()));
  for (const Crossing &c : l) {
    b.push_back(static_cast<uint8_t>(c.x));
    b.push_back(static_cast<uint8_t>(static_cast<uint16_t>(c.x) >> 8));
    b.push_back(static_cast<uint8_t>(c.y));
    b.push_back(static_cast<uint8_t>(static_cast<uint16_t>(c.y) >> 8));
    b.push_back(c.axis);
    b.push_back(c.style);
    b.push_back(c.kind);
    b.push_back(0);
  }
  return b;
}

bool crossings_decode(Ctx &ctx, const std::vector<uint8_t> &b) {
  crossings_clear(ctx);
  if (b.size() < 16 || std::memcmp(b.data(), kMagic, 4) != 0 || get_u32(b, 4) != kVersion) return false;
  if (static_cast<int32_t>(get_u32(b, 8)) != world_n(ctx)) return false;
  const uint32_t n = get_u32(b, 12);
  if (b.size() != 16u + 8u * static_cast<size_t>(n)) return false;
  for (uint32_t k = 0; k < n; ++k) {
    const size_t at = 16u + 8u * k;
    Crossing c;
    c.x = static_cast<int16_t>(b[at] | (b[at + 1] << 8));
    c.y = static_cast<int16_t>(b[at + 2] | (b[at + 3] << 8));
    c.axis = static_cast<uint8_t>(b[at + 4] & 1);
    c.style = static_cast<uint8_t>(b[at + 5] & 7);
    c.kind = static_cast<uint8_t>(b[at + 6] & 3);
    bool ok = true;
    for (int32_t p = 0; p < 2 && ok; ++p) {
      const int32_t x = c.x + p * crossing_dx(c), y = c.y + p * crossing_dy(c);
      ok = on_grid(ctx, x, y) && (flags_at(ctx, x, y) & (1u << 5)) &&
           grid_ref(ctx, &GameState::g_tileBuildCode, tile_index(ctx, x, y)) == kBridgeCode && crossing_at(ctx, x, y) < 0;
    }
    if (ok) crossing_add(ctx, c);
  }
  return true;
}

void crossings_after_save(Ctx &ctx, const std::string &save_path) {
  if (!crossings_on() || ctx.files_ptr == nullptr) return;
  const std::string side = crossings_sidecar_path(save_path);
  if (state(ctx).list.empty()) {
    if (ctx.files().exists(side)) ctx.files().remove(side);
    return;
  }
  ctx.files().write(side, crossings_encode(ctx));
}

void crossings_after_load(Ctx &ctx, const std::string &save_path) {
  crossings_clear(ctx);
  if (!crossings_on() || ctx.files_ptr == nullptr || SG_NEGCTL(g_negctl_crossings) == 2) return;
  const std::string side = crossings_sidecar_path(save_path);
  if (ctx.files().exists(side)) crossings_decode(ctx, ctx.files().read_all(side));
}

void crossings_after_delete(Ctx &ctx, const std::string &save_path) {
  if (ctx.files_ptr == nullptr) return;
  const std::string side = crossings_sidecar_path(save_path);
  if (ctx.files().exists(side)) ctx.files().remove(side);
}

// ------------------------------------------------------------------------------------------------ test entries
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
uint32_t pack(const Crossing &c) {
  return static_cast<uint32_t>(c.x & 0xFF) | (static_cast<uint32_t>(c.y & 0xFF) << 8) | (static_cast<uint32_t>(c.axis) << 16) |
         (static_cast<uint32_t>(c.style) << 20) | (static_cast<uint32_t>(c.kind) << 24);
}
void a_count(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(static_cast<int32_t>(crossings_list(c).size())); }
void a_get(Ctx &c, const uint32_t *a, Ret &r) {
  const std::vector<Crossing> &l = crossings_list(c);
  r.set_u32(a[0] < l.size() ? pack(l[a[0]]) : 0xFFFFFFFFu);
}
void a_at(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(crossing_at(c, S(a, 0), S(a, 1))); }
void a_clear(Ctx &c, const uint32_t *, Ret &r) {
  crossings_clear(c);
  r.set_i32(0);
}
// crossings.fit(x, y, prefer_axis): the pair a click there builds (packed), -1 when none fits
void a_fit(Ctx &c, const uint32_t *a, Ret &r) {
  Crossing x;
  r.set_u32(crossing_choose(c, S(a, 0), S(a, 1), S(a, 2), x) ? pack(x) : 0xFFFFFFFFu);
}
// crossings.build(x, y, prefer_axis, price_each): fit and build (1), 0 when nothing fits
void a_build(Ctx &c, const uint32_t *a, Ret &r) {
  Crossing x;
  if (!crossing_choose(c, S(a, 0), S(a, 1), S(a, 2), x)) {
    r.set_i32(0);
    return;
  }
  crossing_build(c, x, S(a, 3));
  r.set_i32(1);
}
void a_set_style(Ctx &c, const uint32_t *a, Ret &r) {
  crossings_set_style(c, S(a, 0));
  r.set_i32(crossings_style(c));
}

const FuncDef kFuncs[] = {
    {"crossings.count", "default", "int", 0, 0, a_count, &g_negctl_crossings},
    {"crossings.get", "default", "uint32", 1, 0, a_get, &g_negctl_crossings},
    {"crossings.at", "default", "int", 2, 0, a_at, &g_negctl_crossings},
    {"crossings.clear", "default", "int", 0, 0, a_clear, &g_negctl_crossings},
    {"crossings.fit", "default", "uint32", 3, 0, a_fit, &g_negctl_crossings},
    {"crossings.build", "default", "int", 4, 0, a_build, &g_negctl_crossings},
    {"crossings.setStyle", "default", "int", 1, 0, a_set_style, &g_negctl_crossings},
};
// the module's play switch (re/port/play_profile.tsv)
const DefectDef kDefects[] = {
    {"scenicBridge.twoTile", "P39A1",
     "the Scenic Bridge covers one water tile, and two side by side draw as a boardwalk; fixed: the bridge row's ninth item builds a "
     "two-tile scenic bridge over water or stream tiles with land at both ends (twice the price, kept in the port's crossings table "
     "and a sidecar file next to the save)",
     &g_defect_two_tile_bridge},
};
}  // namespace

extern const ModuleDef kModule_crossings = {"crossings", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                            sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
