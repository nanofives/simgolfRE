// Tile labels of the terrain module (see portsim/sim/terrain.h): decorationName and buildToolLabel. Phase 3.1a, written
// from the specification (re/analysis/course/00407700_decorationName.md, re/analysis/ui/0040a160_buildToolLabel.md, the
// C3 hook sources as reading reference, the vector corpus). Strings are (pool member, byte offset) pairs; no original
// address, code or text.
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/terrain.h"
#include "portsim/sim/tiles.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   decorationName  1 = category 0x12 takes category 0x13's string, 2 = the variant picks by % 4
//   buildToolLabel  1 = the 0x8000 flag test skipped, 2 = item kind 0x10 takes kind 1's string
int g_negctl_decoration_name = 0;
int g_negctl_build_tool_label = 0;

// byte offsets in f_strpool_4c4970 (decorationName's strings)
enum : uint32_t {
  kDnBuilding = 0x10, kDnLot = 0x2E4, kDnKind2 = 0x2F0, kDnDefault = 0x2FC, kDnCat7 = 0x308, kDnType4Plain = 0x314,
  kDnType4Ct1 = 0x328, kDnType4Rec10 = 0x33C, kDnCat4Other = 0x42C, kDnCat4Rec13 = 0x438, kDnCat4C1 = 0x444,
  kDnCatD10 = 0x46C, kDnCatDF = 0x454, kDnCatDCt1 = 0x45C, kDnCat12 = 0x478, kDnCatC = 0x484, kDnCat11Obj = 0x494,
  kDnCat11Path = 0x49C, kDnCat11T13 = 0x4AC, kDnCat13 = 0x4B8,
};
constexpr uint32_t kDnVarCt1[5] = {0x41C, 0x408, 0x3F4, 0x3E4, 0x3D4};
constexpr uint32_t kDnVarCt2[5] = {0x3C4, 0x3AC, 0x39C, 0x38C, 0x37C};
constexpr uint32_t kDnVarCt03[5] = {0x36C, 0x358, 0x3F4, 0x348, 0x37C};

// byte offsets in f_strpool_4c4f98 (buildToolLabel's strings)
enum : uint32_t {
  kBlOffMap = 0x1C4, kBlFlag8000 = 0x1B8, kBlObject = 0x1AC, kBlItem80 = 0x19C, kBlKind0 = 0x190, kBlKind1 = 0x180,
  kBlKind10 = 0x16C, kBlKind13 = 0x154, kBlItem = 0x148, kBlObject2 = 0x140,
};

uint32_t text_va(Ctx &c) { return state_va(c, &GameState::g_textBuffer); }
uint32_t pool_a(Ctx &c, uint32_t off) { return state_va(c, &GameState::f_strpool_4c4970) + off; }
uint32_t pool_b(Ctx &c, uint32_t off) { return state_va(c, &GameState::f_strpool_4c4f98) + off; }
Grid<int8_t> types(Ctx &c) { return state_grid(c, &GameState::g_tileType, kGridW); }
Grid<uint16_t> flags(Ctx &c) { return state_grid(c, &GameState::g_tileFlags, kGridW); }
Ptr<PlacedObject> placed(Ctx &c, int32_t i) { return state_arr(c, &GameState::g_placed_objects) + i; }

// 3.1b's landmarkName (re/analysis/course/004074a0), called through the registry by its hooks.csv name.
void landmark_name_peer(Ctx &ctx, int32_t type, int32_t full) {
  const int i = registry_find("landmarkName");
  if (i < 0) throw MissingResource{"decorationName: landmarkName is not registered (3.1b)"};
  const uint32_t a[2] = {u32(type), u32(full)};
  Ret r;
  registry_at(i, nullptr)->fn(ctx, a, r);
}
}  // namespace

int32_t decoration_name(Ctx &ctx, int32_t x, int32_t y, int32_t id) {
  int32_t type;
  if (id == -1) {
    type = types(ctx)(x, y);
  } else if (id & 0x100) {
    const Ptr<PlacedObject> o = placed(ctx, id & 0xFF);
    x = o.field(&PlacedObject::x).get();
    y = o.field(&PlacedObject::y).get();
    type = types(ctx)(x, y);
  } else {
    type = id;
  }
  const uint32_t text = text_va(ctx);
  const auto cat = [&](uint32_t off) { vstrcat(ctx.mem, text, pool_a(ctx, off)); };
  if (type == 0x16 || type == 0x15) {
    const Ptr<PlacedObject> o = placed(ctx, state_grid(ctx, &GameState::g_tileVariant, kGridW)(x, y));
    const int16_t kind = o.field(&PlacedObject::type).get();
    if (kind == 4) {
      landmark_name_peer(ctx, o.field(&PlacedObject::value).get(), 0);
      return 1;
    }
    cat(kind == 2 ? kDnKind2 : (type == 0x16 ? kDnBuilding : kDnLot));
    return 1;
  }
  // read on use only, as the original does: x and y are arbitrary when id names the type directly
  const int8_t course_type = ctx.gs().g_courseType;
  const auto course_rec = [&] {  // the current course record's location id (+0x1c), unsigned
    return static_cast<uint8_t>(
        (state_arr(ctx, &GameState::g_courseTable) + ctx.gs().g_courseIndex).field(&CourseRecord::location_id).get());
  };
  const auto fl = [&] { return flags(ctx)(x, y); };
  const auto obj_byte = [&] { return state_grid(ctx, &GameState::g_tileVariant, kGridW)(x, y); };
  const int32_t category =
      (state_arr(ctx, &GameState::g_tileTypeAttrs) + type).field(&TileTypeAttr::collar).get();
  uint32_t s;
  switch (category) {
    case 4:
      if (type == 4) {
        if ((fl() & 0x1000) == 0) {
          if (course_rec() == 0xA) s = kDnType4Rec10;
          else s = course_type == 1 ? kDnType4Ct1 : kDnType4Plain;
        } else {
          const int32_t v = obj_byte() % (SG_NEGCTL(g_negctl_decoration_name) == 2 ? 4 : 5);
          if (course_type == 1) s = kDnVarCt1[v];
          else if (course_type == 2) s = kDnVarCt2[v];
          else if (course_type == 0 || course_type == 3) s = kDnVarCt03[v];
          else return 1;
        }
      } else if (type == 0xA) {
        s = course_type == 1 ? kDnCatC : kDnCatD10;
      } else if (type == 0xC) {
        s = course_type == 1 ? kDnCat4C1 : kDnCatC;
      } else {
        s = course_rec() == 0xD ? kDnCat4Rec13 : kDnCat4Other;
      }
      break;
    case 7: s = kDnCat7; break;
    case 0xC: s = kDnCatC; break;
    case 0xD:
      if (type == 0x10) s = kDnCatD10;
      else if (course_type == 1) s = kDnCatDCt1;
      else if (type == 0xD) s = kDnCatD10;
      else if (type == 0xF) s = kDnCatDF;
      else if (type == 0xE) s = kDnCatD10;
      else return 1;
      break;
    case 0x11:
      if (type == 0x13) s = kDnCat11T13;
      else if (fl() & 0x20) s = kDnCat11Path;
      else if (obj_byte() != 0 && static_cast<uint8_t>(types(ctx)(x, y)) == 0x11) s = kDnCat11Obj;
      else s = course_type == 1 ? kDnCatC : kDnCat12;
      break;
    case 0x12: s = SG_NEGCTL(g_negctl_decoration_name) == 1 ? kDnCat13 : kDnCat12; break;
    case 0x13: s = kDnCat13; break;
    case 0x15:
    case 0x16: s = kDnBuilding; break;
    default: s = kDnDefault; break;
  }
  cat(s);
  return 1;
}

int32_t build_tool_label(Ctx &ctx, int32_t x, int32_t y) {
  const uint32_t text = text_va(ctx);
  const auto cpy = [&](uint32_t off) { vstrcpy(ctx.mem, text, pool_b(ctx, off)); };
  const auto object_name = [&](int32_t idx) {  // F_BuildItem name of the object's type
    const int16_t type = placed(ctx, idx).field(&PlacedObject::type).get();
    return state_va(ctx, &GameState::f_tab_buildItems_4c26b0) + u32(mul32(type, 20));
  };
  if (tile_blocked(ctx, x, y) != 0) {
    cpy(kBlOffMap);
    return 0;
  }
  ctx.mem.store<uint8_t>(text, 0);
  const uint16_t f = flags(ctx)(x, y);
  if ((f & 0x8000) && SG_NEGCTL(g_negctl_build_tool_label) != 1) {
    cpy(kBlFlag8000);
    return 0;
  }
  if (f & 0x400) {
    const int32_t idx = object_at(ctx, x, y);
    if (idx != -1) {
      cpy(kBlObject);
      vstrcat(ctx.mem, text, object_name(idx));
      return 1;
    }
  }
  const uint8_t item = state_grid(ctx, &GameState::g_tileBuildCode, kGridW)(x, y);
  if (item != 0xFF) {
    if (item & 0x80) {
      cpy(kBlItem80);
      const uint8_t kind = item & 0x7F;
      if (kind == 0) cpy(kBlKind0);
      if (kind == 1) cpy(kBlKind1);
      if (kind == 0x10) cpy(SG_NEGCTL(g_negctl_build_tool_label) == 2 ? kBlKind1 : kBlKind10);
      if (kind == 0x13) cpy(kBlKind13);
      return 1;
    }
    if (static_cast<int8_t>(item) < 0x7D) {
      vstrcat(ctx.mem, text, pool_b(ctx, kBlItem));
      vstrcat(ctx.mem, text, state_va(ctx, &GameState::g_tileTypeAttrs) + u32(static_cast<int8_t>(item)) * 0x30u);
      return 1;
    }
  }
  if (f & 0x200) {
    cpy(kBlKind1);
    return 0;
  }
  if (f & 0x400) {
    const int32_t idx = object_at(ctx, x, y);
    if (idx != -1) {
      cpy(kBlObject2);
      vstrcat(ctx.mem, text, object_name(idx));
      return 0;
    }
  }
  if (f & 0x20) {
    cpy(kBlKind0);
    return 0;
  }
  if (f & 0x1000) {
    cpy(kBlItem80);
    return 0;
  }
  ctx.mem.store<uint8_t>(text, 0);
  return 0;
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_decoration_name(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(decoration_name(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_build_tool_label(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(build_tool_label(c, S(a, 0), S(a, 1))); }

const FuncDef kFuncs[] = {
    {"decorationName", "default", "int", 3, 0, a_decoration_name, &g_negctl_decoration_name},
    {"buildToolLabel", "default", "int", 2, 0, a_build_tool_label, &g_negctl_build_tool_label},
};
}  // namespace

extern const ModuleDef kModule_terrain_text = {"terrain_text", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
