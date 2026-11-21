// P39A1: the two-tile scenic bridge's table, its building and removal, and its sidecar file (crossings.h). bridge.cpp is the view
// bridge (P8B); this module is the course's crossings. P39A3: gap bridges (the path tool across a 1-2 tile gap, asking first).
#include "crossings.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/bridge.h"    // points_popup
#include "portsim/sim/terrain.h"   // corner_heights
#include "portsim/sim/world.h"

namespace portsim {

int g_defect_two_tile_bridge = 1;
int g_defect_gap_bridge = 1;
thread_local bool t_crossings_in_undo = false;
thread_local int32_t t_crossings_redo_axis = -1;
thread_local bool t_gap_auto_yes = false;

namespace {
// Negative controls (every crossings.* entry shares the word): 1 = the land test at the ends skipped, 2 = the sidecar not read back,
// 3 = the partner not demolished; P39A3: 4 = a gap 2 levels deep is enough, 5 = a gap bridge's extra is not paid back, 6 = the
// question's answer ignored (always yes), 7 = no deck lift (walkers and the deck drawn at the ground)
int g_negctl_crossings = 0;

constexpr int8_t kWater = 0x11, kStream = 0x0A;
constexpr uint8_t kBridgeCode = 0x93;   // setGrids' code for the bridge tool (0x80 | 0x13), as H2_Build case 5 writes it
constexpr char kMagic[4] = {'S', 'G', 'X', 'B'};
constexpr uint32_t kVersion1 = 1;       // P39A1: 8 bytes per entry
constexpr uint32_t kVersion = 2;        // P39A3: 12 bytes per entry (type, span, deck, extra)
constexpr const char *kSidecarExt = ".sgx";

struct State {
  std::vector<Crossing> list;
  int32_t style = 0;
  bool declined = false;   // P39A3: the gap the player last answered no to (not asked again until another gap is asked)
  Crossing declined_gap;
  int32_t forced_answer = -1;   // P39A3 test entry crossings.gapAnswer: -1 the popup asks, 0 yes, 1 no
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
bool path_at(Ctx &ctx, int32_t x, int32_t y) { return on_grid(ctx, x, y) && (flags_at(ctx, x, y) & (1u << 5)) != 0; }
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
int32_t tile_x(const Crossing &c, int32_t k) { return c.x + k * crossing_dx(c); }
int32_t tile_y(const Crossing &c, int32_t k) { return c.y + k * crossing_dy(c); }
bool same_gap(const Crossing &a, const Crossing &b) { return a.x == b.x && a.y == b.y && a.axis == b.axis && a.span == b.span; }
}  // namespace

const std::vector<Crossing> &crossings_list(Ctx &ctx) { return state(ctx).list; }

int32_t crossing_at(Ctx &ctx, int32_t x, int32_t y, int32_t *part) {
  const std::vector<Crossing> &l = state(ctx).list;
  for (size_t i = 0; i < l.size(); ++i) {
    const Crossing &c = l[i];
    for (int32_t p = 0; p < c.span; ++p)
      if (tile_x(c, p) == x && tile_y(c, p) == y) {
        if (part) *part = p;
        return static_cast<int32_t>(i);
      }
  }
  return -1;
}

int32_t crossing_gap_at(Ctx &ctx, int32_t x, int32_t y) {
  const int32_t i = crossing_at(ctx, x, y);
  return i >= 0 && state(ctx).list[static_cast<size_t>(i)].type == kCrossingGap ? i : -1;
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
    const int32_t x = tile_x(c, p), y = tile_y(c, p);
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
void crossings_clear(Ctx &ctx) {
  State &s = state(ctx);
  s.list.clear();
  s.declined = false;
}
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
  // P39A3: the price the refund bytes could not hold comes back with the bridge (the bulldozer and the undo alike), into the cash and
  // the year's build-course row as demolishObject adds the refunds
  if (c.type == kCrossingGap && c.extra != 0 && SG_NEGCTL(g_negctl_crossings) != 5) {
    GameState &gs = ctx.gs();
    gs.g_cash_div100 = add32(gs.g_cash_div100, c.extra);
    FinanceYear &fy = gs.g_financeYears[gs.g_year % 100];
    fy.build_course = static_cast<int16_t>(fy.build_course + static_cast<int16_t>(c.extra));
    points_popup(ctx, c.extra, add32(shl32(x, 10), 0x200), add32(shl32(y, 10), 0x200), -1);
  }
  if (t_crossings_in_undo || SG_NEGCTL(g_negctl_crossings) == 3) return;   // the undo journal reverts each tile itself
  for (int32_t k = 0; k < c.span; ++k) {
    if (k == part) continue;
    const int32_t ox = tile_x(c, k), oy = tile_y(c, k);
    if (grid_ref(ctx, &GameState::g_tileBuildCode, tile_index(ctx, ox, oy)) == kBridgeCode) peer(ctx, "demolishObject", {u32(ox), u32(oy), 0});
  }
}

// ------------------------------------------------------------------------------------------------ P39A3 gap bridges
int32_t gap_edge_level(Ctx &ctx, int32_t bx, int32_t by, int32_t dx, int32_t dy) {
  int32_t a = 1, b = 3;   // +x
  if (dx < 0) a = 5, b = 7;
  else if (dy > 0) a = 3, b = 5;
  else if (dy < 0) a = 1, b = 7;
  return std::min(corner_heights(ctx, bx, by, a, 1), corner_heights(ctx, bx, by, b, 1));
}
int32_t gap_tile_top(Ctx &ctx, int32_t x, int32_t y) {
  int32_t v = corner_heights(ctx, x, y, 1, 1);
  for (int32_t c = 3; c < 8; c += 2) v = std::max(v, corner_heights(ctx, x, y, c, 1));
  return v;
}
int32_t gap_tile_bottom(Ctx &ctx, int32_t x, int32_t y) {
  int32_t v = corner_heights(ctx, x, y, 1, 1);
  for (int32_t c = 3; c < 8; c += 2) v = std::min(v, corner_heights(ctx, x, y, c, 1));
  return v;
}

namespace {
// a tile a gap bridge may span: on the grid, inside the course (not 0x14), no building (0x15 / 0x16), no path, no placed object, not
// the refusal bit, in no crossing
bool gap_tile_ok(Ctx &ctx, int32_t x, int32_t y) {
  if (!on_grid(ctx, x, y)) return false;
  const int8_t t = type_at(ctx, x, y);
  if (t == 0x14 || t == 0x15 || t == 0x16) return false;
  if (flags_at(ctx, x, y) & ((1u << 5) | (1u << 10) | (1u << 15))) return false;
  return crossing_at(ctx, x, y) < 0;
}
// a bank: on the grid, land (land_at), no building
bool gap_bank_ok(Ctx &ctx, int32_t x, int32_t y) {
  if (!land_at(ctx, x, y)) return false;
  const int8_t t = type_at(ctx, x, y);
  return t != 0x15 && t != 0x16;
}
// span s from bank (ax, ay) toward (dx, dy): the gap tiles' lowest corner at least the threshold below both bank edges
bool gap_span(Ctx &ctx, int32_t ax, int32_t ay, int32_t dx, int32_t dy, int32_t s, Crossing &out) {
  const int32_t bx = ax + (s + 1) * dx, by = ay + (s + 1) * dy;
  if (!gap_bank_ok(ctx, ax, ay) || !gap_bank_ok(ctx, bx, by)) return false;
  for (int32_t k = 1; k <= s; ++k)
    if (!gap_tile_ok(ctx, ax + k * dx, ay + k * dy)) return false;
  const int32_t deck = std::min(gap_edge_level(ctx, ax, ay, dx, dy), gap_edge_level(ctx, bx, by, -dx, -dy));
  // Mariano (P39A3 round 2): the gap's LOWEST corner, so a dry dip in any land counts as well as a flattened water channel
  int32_t bottom = gap_tile_bottom(ctx, ax + dx, ay + dy);
  for (int32_t k = 2; k <= s; ++k) bottom = std::min(bottom, gap_tile_bottom(ctx, ax + k * dx, ay + k * dy));
  const int32_t need = SG_NEGCTL(g_negctl_crossings) == 4 ? kGapLevels - 1 : kGapLevels;
  if (bottom > deck - need) return false;
  out = Crossing{};
  out.type = kCrossingGap;
  out.axis = static_cast<uint8_t>(dx != 0 ? 0 : 1);
  out.span = static_cast<uint8_t>(s);
  out.style = static_cast<uint8_t>(state(ctx).style & 7);   // the scenic deck art (SG_P39A3_DECK 1 / 2) takes the row's style
  out.deck = static_cast<uint8_t>(deck);
  // the first tile is the one with the smaller coordinate along the axis
  const int32_t f = dx + dy > 0 ? 1 : s;
  out.x = static_cast<int16_t>(ax + f * dx);
  out.y = static_cast<int16_t>(ay + f * dy);
  return true;
}
}  // namespace

bool gap_find(Ctx &ctx, int32_t x, int32_t y, Crossing &out) {
  if (!on_grid(ctx, x, y)) return false;
  static const int32_t kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};   // axis x before y
  for (int32_t s = 1; s <= kGapMaxSpan; ++s)
    for (const auto &d : kDirs) {
      const int32_t dx = d[0], dy = d[1];
      // (x, y) a bank: the far bank already a path
      if (gap_span(ctx, x, y, dx, dy, s, out) && path_at(ctx, x + (s + 1) * dx, y + (s + 1) * dy)) return true;
      // (x, y) the gap tile j of the span (bank A behind it): a path on either bank
      for (int32_t j = 1; j <= s; ++j) {
        const int32_t ax = x - j * dx, ay = y - j * dy;
        if (gap_span(ctx, ax, ay, dx, dy, s, out) && (path_at(ctx, ax, ay) || path_at(ctx, ax + (s + 1) * dx, ay + (s + 1) * dy)))
          return true;
      }
    }
  return false;
}

int32_t gap_price(Ctx &ctx, const Crossing &c) {
  const int32_t word = ctx.mem.load<int16_t>(state_va(ctx, &GameState::f_tab_buildItems_4c26b0) + 0x12u + 20u * u32(kScenicBridgeTool));
  int32_t bottom = gap_tile_bottom(ctx, c.x, c.y);
  for (int32_t k = 1; k < c.span; ++k) bottom = std::min(bottom, gap_tile_bottom(ctx, tile_x(c, k), tile_y(c, k)));
  const int32_t depth = std::max(0, c.deck - bottom);
  return add32(mul32(word, 2), mul32(mul32(kGapLevelCharge, depth), c.span));
}

void gap_build(Ctx &ctx, Crossing c, int32_t price) {
  const int32_t each = std::min(price / std::max<int32_t>(1, c.span), 127);
  c.extra = static_cast<uint16_t>(std::max(0, price - each * c.span));
  for (int32_t k = 0; k < c.span; ++k) {
    const int32_t x = tile_x(c, k), y = tile_y(c, k);
    uint16_t &w = flags_at(ctx, x, y);
    w = static_cast<uint16_t>(w | (1u << 5));   // the path bit only: walkers cross it, no scenic thought
    peer(ctx, "setGrids", {u32(x), u32(y), kBridgeCode, u32(each)});
  }
  crossing_add(ctx, c);
}

int32_t gap_deck_lift(Ctx &ctx, int32_t wx, int32_t wy) {
  if (!gap_bridges_on() || SG_NEGCTL(g_negctl_crossings) == 7) return 0;
  const int32_t i = crossing_gap_at(ctx, sar32(wx, 10), sar32(wy, 10));
  if (i < 0) return 0;
  return sub32(level_height_px(ctx, state(ctx).list[static_cast<size_t>(i)].deck), world_height_px(ctx, wx, wy));
}

std::string gap_question(int32_t price) {
  std::string digits = std::to_string(static_cast<long long>(price) * 100);
  std::string out;
  for (size_t i = 0; i < digits.size(); ++i) {
    if (i != 0 && (digits.size() - i) % 3 == 0) out += ',';
    out += digits[i];
  }
  return std::string("Build a bridge across this gap for ") + '\xA7' + out;   // the game's money sign (cp1252 0xa7), as its texts write it
}

int32_t gap_try(Ctx &ctx, int32_t x, int32_t y) {
  if (!gap_bridges_on() || !on_grid(ctx, x, y) || path_at(ctx, x, y)) return 0;
  Crossing c;
  if (!gap_find(ctx, x, y, c)) return 0;
  State &s = state(ctx);
  if (!t_gap_auto_yes && s.declined && same_gap(s.declined_gap, c)) return 0;
  const int32_t price = gap_price(ctx, c);
  bool yes = true;
  if (!t_gap_auto_yes && SG_NEGCTL(g_negctl_crossings) != 6) {
    if (s.forced_answer >= 0) {
      yes = s.forced_answer == 0;
    } else {
      // the question, then demolishObject's confirmation tail (f_strpool_4c4f98 + 0x1d4: "?", the option lines " Yes I do." and
      // " Ooops, no.": the game's own, which the language pack translates), shown by the game's yes / no popup (answer 0 = the first)
      std::string q = gap_question(price);
      const uint32_t tail = state_va(ctx, &GameState::f_strpool_4c4f98) + 0x1D4u;
      for (uint32_t i = 0; i < 0x40u; ++i) {
        const uint8_t ch = ctx.mem.load<uint8_t>(tail + i);
        if (ch == 0) break;
        q += static_cast<char>(ch);
      }
      const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
      for (size_t i = 0; i <= q.size(); ++i) ctx.mem.store<uint8_t>(text + static_cast<uint32_t>(i), i < q.size() ? static_cast<uint8_t>(q[i]) : 0);
      yes = peer(ctx, "showPopup", {0x190, 0x64, 1, 1, 0}).eax == 0;
    }
  }
  if (!yes) {
    s.declined = true;
    s.declined_gap = c;
    return 0;
  }
  s.declined = false;
  if (peer(ctx, "canAfford", {u32(price)}).eax == 0) return 0;
  gap_build(ctx, c, price);
  // the charge as H2_Build's tail takes it (ML+0x1058f..ML+0x1061e): the total dword g_holes+0x2888, the year's facilities row, the cash,
  // then the pointsPopup over the bridge and the bridge's build sound (H2_Build case 5's 0x107)
  GameState &gs = ctx.gs();
  const uint32_t total = state_va(ctx, &GameState::g_holes) + 0x2888u;
  ctx.mem.store<int32_t>(total, add32(ctx.mem.load<int32_t>(total), price));
  FinanceYear &fy = gs.g_financeYears[gs.g_year % 100];
  fy.facilities = static_cast<int16_t>(fy.facilities - static_cast<int16_t>(price));
  gs.g_cash_div100 = sub32(gs.g_cash_div100, price);
  const int32_t mid = (c.span - 1) / 2;
  const int32_t wx = add32(shl32(tile_x(c, mid), 10), 0x200), wy = add32(shl32(tile_y(c, mid), 10), 0x200);
  points_popup(ctx, neg32(price), wx, wy, -1);
  if (registry_find("playSoundAt") >= 0) peer(ctx, "playSoundAt", {0x107, u32(wx), u32(wy), 0});
  return crossing_gap_at(ctx, x, y) >= 0 ? 2 : 1;
}

// ------------------------------------------------------------------------------------------------ the sidecar
// Layout (little-endian): "SGXB", the version (2), the world's N, the count, then 12 bytes per crossing: x (u16), y (u16), axis,
// style, kind, type, span, deck, extra (u16). Version 1 (P39A1: 8 bytes, x, y, axis, style, kind, 0) is read as two-tile bridges.
// A load keeps an entry only when every tile is on the grid and carries the path bit with the bridge code 0x93 (a save the original
// game changed since drops what it removed).
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
    b.push_back(c.type);
    b.push_back(c.span);
    b.push_back(c.deck);
    b.push_back(static_cast<uint8_t>(c.extra));
    b.push_back(static_cast<uint8_t>(c.extra >> 8));
  }
  return b;
}

bool crossings_decode(Ctx &ctx, const std::vector<uint8_t> &b) {
  crossings_clear(ctx);
  if (b.size() < 16 || std::memcmp(b.data(), kMagic, 4) != 0) return false;
  const uint32_t version = get_u32(b, 4);
  if (version != kVersion1 && version != kVersion) return false;
  if (static_cast<int32_t>(get_u32(b, 8)) != world_n(ctx)) return false;
  const uint32_t n = get_u32(b, 12);
  const size_t size = version == kVersion1 ? 8u : 12u;
  if (b.size() != 16u + size * static_cast<size_t>(n)) return false;
  for (uint32_t k = 0; k < n; ++k) {
    const size_t at = 16u + size * k;
    Crossing c;
    c.x = static_cast<int16_t>(b[at] | (b[at + 1] << 8));
    c.y = static_cast<int16_t>(b[at + 2] | (b[at + 3] << 8));
    c.axis = static_cast<uint8_t>(b[at + 4] & 1);
    c.style = static_cast<uint8_t>(b[at + 5] & 7);
    c.kind = static_cast<uint8_t>(b[at + 6] & 3);
    if (version == kVersion) {
      c.type = b[at + 7] == kCrossingGap ? kCrossingGap : kCrossingTwoTile;
      c.span = c.type == kCrossingGap ? static_cast<uint8_t>(std::clamp<int>(b[at + 8], 1, kGapMaxSpan)) : 2;
      c.deck = b[at + 9];
      c.extra = static_cast<uint16_t>(b[at + 10] | (b[at + 11] << 8));
    }
    bool ok = true;
    for (int32_t p = 0; p < c.span && ok; ++p) {
      const int32_t x = tile_x(c, p), y = tile_y(c, p);
      ok = on_grid(ctx, x, y) && (flags_at(ctx, x, y) & (1u << 5)) &&
           grid_ref(ctx, &GameState::g_tileBuildCode, tile_index(ctx, x, y)) == kBridgeCode && crossing_at(ctx, x, y) < 0;
    }
    if (ok) crossing_add(ctx, c);
  }
  return true;
}

void crossings_after_save(Ctx &ctx, const std::string &save_path) {
  if (!crossings_any_on() || ctx.files_ptr == nullptr) return;
  const std::string side = crossings_sidecar_path(save_path);
  if (state(ctx).list.empty()) {
    if (ctx.files().exists(side)) ctx.files().remove(side);
    return;
  }
  ctx.files().write(side, crossings_encode(ctx));
}

void crossings_after_load(Ctx &ctx, const std::string &save_path) {
  crossings_clear(ctx);
  if (!crossings_any_on() || ctx.files_ptr == nullptr || SG_NEGCTL(g_negctl_crossings) == 2) return;
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
// P39A3: x, y, axis, span (4 bits each from bit 0: x & 0xff, y & 0xff << 8, axis << 16, span << 20, type << 24, deck << 26)
uint32_t pack_gap(const Crossing &c) {
  return static_cast<uint32_t>(c.x & 0xFF) | (static_cast<uint32_t>(c.y & 0xFF) << 8) | (static_cast<uint32_t>(c.axis & 1) << 16) |
         (static_cast<uint32_t>(c.span & 0xF) << 20) | (static_cast<uint32_t>(c.type & 3) << 24) | (static_cast<uint32_t>(c.deck & 0x3F) << 26);
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
// crossings.gapFind(x, y): the gap a path laid on (x, y) would bridge (pack_gap), -1 when none
void a_gap_find(Ctx &c, const uint32_t *a, Ret &r) {
  Crossing x;
  r.set_u32(gap_find(c, S(a, 0), S(a, 1), x) ? pack_gap(x) : 0xFFFFFFFFu);
}
// crossings.gapPrice(x, y): gap_price of that gap, -1 when none
void a_gap_price(Ctx &c, const uint32_t *a, Ret &r) {
  Crossing x;
  r.set_i32(gap_find(c, S(a, 0), S(a, 1), x) ? gap_price(c, x) : -1);
}
// crossings.gapAnswer(answer): the popup's answer for the next questions (-1 ask, 0 yes, 1 no)
void a_gap_answer(Ctx &c, const uint32_t *a, Ret &r) {
  state(c).forced_answer = S(a, 0);
  r.set_i32(state(c).forced_answer);
}
// crossings.gapTry(x, y): gap_try
void a_gap_try(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(gap_try(c, S(a, 0), S(a, 1))); }
// crossings.getGap(i): pack_gap of entry i, -1 past the end; crossings.extra(i): its extra
void a_get_gap(Ctx &c, const uint32_t *a, Ret &r) {
  const std::vector<Crossing> &l = crossings_list(c);
  r.set_u32(a[0] < l.size() ? pack_gap(l[a[0]]) : 0xFFFFFFFFu);
}
void a_extra(Ctx &c, const uint32_t *a, Ret &r) {
  const std::vector<Crossing> &l = crossings_list(c);
  r.set_i32(a[0] < l.size() ? l[a[0]].extra : -1);
}
// crossings.edge(x, y, dir 0..3 = +x -x +y -y), crossings.top(x, y), crossings.bottom(x, y)
void a_edge(Ctx &c, const uint32_t *a, Ret &r) {
  static const int32_t d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  const int32_t k = S(a, 2) & 3;
  r.set_i32(gap_edge_level(c, S(a, 0), S(a, 1), d[k][0], d[k][1]));
}
void a_top(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(gap_tile_top(c, S(a, 0), S(a, 1))); }
// crossings.addGap(x, y, axis, span, deck): a gap bridge entry as is (no tile written; the 0x2b test's deck), its index
void a_add_gap(Ctx &c, const uint32_t *a, Ret &r) {
  Crossing x;
  x.type = kCrossingGap;
  x.x = static_cast<int16_t>(S(a, 0));
  x.y = static_cast<int16_t>(S(a, 1));
  x.axis = static_cast<uint8_t>(S(a, 2) & 1);
  x.span = static_cast<uint8_t>(std::clamp(S(a, 3), 1, kGapMaxSpan));
  x.deck = static_cast<uint8_t>(S(a, 4));
  crossing_add(c, x);
  r.set_i32(static_cast<int32_t>(crossings_list(c).size()) - 1);
}
void a_bottom(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(gap_tile_bottom(c, S(a, 0), S(a, 1))); }
// crossings.deckLift(wx, wy): gap_deck_lift; crossings.groundPx(wx, wy) / crossings.levelPx(level): the two terms
void a_deck_lift(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(gap_deck_lift(c, S(a, 0), S(a, 1))); }
void a_ground_px(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(world_height_px(c, S(a, 0), S(a, 1))); }
void a_level_px(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(level_height_px(c, S(a, 0))); }

const FuncDef kFuncs[] = {
    {"crossings.count", "default", "int", 0, 0, a_count, &g_negctl_crossings},
    {"crossings.get", "default", "uint32", 1, 0, a_get, &g_negctl_crossings},
    {"crossings.at", "default", "int", 2, 0, a_at, &g_negctl_crossings},
    {"crossings.clear", "default", "int", 0, 0, a_clear, &g_negctl_crossings},
    {"crossings.fit", "default", "uint32", 3, 0, a_fit, &g_negctl_crossings},
    {"crossings.build", "default", "int", 4, 0, a_build, &g_negctl_crossings},
    {"crossings.setStyle", "default", "int", 1, 0, a_set_style, &g_negctl_crossings},
    {"crossings.gapFind", "default", "uint32", 2, 0, a_gap_find, &g_negctl_crossings},
    {"crossings.gapPrice", "default", "int", 2, 0, a_gap_price, &g_negctl_crossings},
    {"crossings.gapAnswer", "default", "int", 1, 0, a_gap_answer, &g_negctl_crossings},
    {"crossings.gapTry", "default", "int", 2, 0, a_gap_try, &g_negctl_crossings},
    {"crossings.getGap", "default", "uint32", 1, 0, a_get_gap, &g_negctl_crossings},
    {"crossings.extra", "default", "int", 1, 0, a_extra, &g_negctl_crossings},
    {"crossings.edge", "default", "int", 3, 0, a_edge, &g_negctl_crossings},
    {"crossings.top", "default", "int", 2, 0, a_top, &g_negctl_crossings},
    {"crossings.addGap", "default", "int", 5, 0, a_add_gap, &g_negctl_crossings},
    {"crossings.bottom", "default", "int", 2, 0, a_bottom, &g_negctl_crossings},
    {"crossings.deckLift", "default", "int", 2, 0, a_deck_lift, &g_negctl_crossings},
    {"crossings.groundPx", "default", "int", 2, 0, a_ground_px, &g_negctl_crossings},
    {"crossings.levelPx", "default", "int", 1, 0, a_level_px, &g_negctl_crossings},
};
// the module's play switches (re/port/play_profile.tsv)
const DefectDef kDefects[] = {
    {"scenicBridge.twoTile", "P39A1",
     "the Scenic Bridge covers one water tile, and two side by side draw as a boardwalk; fixed: the bridge row's ninth item builds a "
     "two-tile scenic bridge over water or stream tiles with land at both ends (twice the price, kept in the port's crossings table "
     "and a sidecar file next to the save)",
     &g_defect_two_tile_bridge},
    {"layPath.gapBridge", "P39A3",
     "a path laid across a ravine follows the ground down and up; fixed: the Pathway (or the path terrain tool) next to a gap of 1 or "
     "2 tiles at least 3 levels below both banks asks first and on yes builds a bridge whose deck stays at bank height (kept in the "
     "crossings table and the sidecar)",
     &g_defect_gap_bridge},
};
}  // namespace

extern const ModuleDef kModule_crossings = {"crossings", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                            sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
