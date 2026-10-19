// Tile-grid and placed-object accessors (see portsim/sim/tiles.h). Phase 3.0, written from the specification (analysis
// notes, disassembly-level transcriptions in those notes, vector corpus); no original address or code.
#include "portsim/sim/tiles.h"

#include "portsim/ptr.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   tileType 1 = transposed index (y * 50 + x), 2 = zero-extended (BLIND: no negative type byte in the vectors)
//   tileByte 1 = sign-extended          tileFlag20 1 = bit 0x10
//   tileBlocked 1 = x == 50 allowed     typeAtPos 1 = shift 9               overlayCharAt 1 = walk >= 0
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

Grid<int8_t> types(Ctx &c) { return state_grid(c, &GameState::g_tileType, kGridW); }
Ptr<TileTypeAttr> attr(Ctx &c, int32_t type) { return state_arr(c, &GameState::g_tileTypeAttrs) + type; }
}  // namespace

int32_t tile_type(Ctx &ctx, int32_t x, int32_t y) {
  const int8_t t = SG_NEGCTL(g_negctl_tile_type) == 1 ? types(ctx)(y, x) : types(ctx)(x, y);
  return SG_NEGCTL(g_negctl_tile_type) == 2 ? i32(zx8(u32(t))) : t;
}

int32_t tile_byte(Ctx &ctx, int32_t x, int32_t y) {
  const uint8_t b = state_grid(ctx, &GameState::g_tileVariant, kGridW)(x, y);
  return SG_NEGCTL(g_negctl_tile_byte) == 1 ? sx8(b) : i32(b);
}

int32_t tile_flag20(Ctx &ctx, int32_t x, int32_t y) {
  // the original reads only the low byte of the flag word
  const uint8_t lo = state_grid(ctx, &GameState::g_tileFlags, kGridW).cell(x, y).as<uint8_t>().get();
  return lo & (SG_NEGCTL(g_negctl_tile_flag20) == 1 ? 0x10 : 0x20);
}

int32_t tile_blocked(Ctx &ctx, int32_t x, int32_t y) {
  const int32_t xmax = SG_NEGCTL(g_negctl_tile_blocked) == 1 ? kGridW + 1 : kGridW;
  if (x < 0 || x >= xmax || y < 0 || y >= kGridW) return 1;
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
  const int8_t m8 = state_grid(ctx, &GameState::g_tileWallMask, kGridW)(x, y);
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

int32_t in_range(Ctx &ctx, int32_t type, int32_t v, int32_t row, int32_t col) {
  int32_t hi, lo;
  if (type == 0x16 || type == 0x15) {
    const uint8_t f = state_grid(ctx, &GameState::g_tileFlags, kGridW).cell(row, col).as<uint8_t>().get();
    hi = (static_cast<int32_t>(f & 0x1F) - 1 > 3) ? 200 : 0;
    lo = 0;
  } else {
    hi = lo = type;  // the window is empty
  }
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  auto r = [&](uint32_t n) { return static_cast<int32_t>(random_range(seed, n) & 0xFFFFu); };
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

}  // namespace portsim
