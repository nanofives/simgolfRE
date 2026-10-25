// Body family `routing` of the graphics backend (roadmap S2.4; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// report-screen art loadInfoScreenArt cuts, and the routing map's tile diamond. Specifications: c3fv.cpp
// (LoadInfoScreenArt_re) and c3et.cpp (ShowRoutingMap_re), with c3dv.cpp (drawRoutingPoly) and c3db.cpp (fillConvexPoly).
//
//   loadInfoScreenArt.images
//       loadInfoScreenArt's image half (the five fonts are created by the boot, tick_boot.inc): a working Image, then per
//       file one loadImage(img, name, 0, 0, 0x100, 1) and the rectangles cut from it with Surface_blitTakeDirty(dest,
//       img, x, y, w, h, 1, trim) (= Surface_blit, gfx_surface.cpp: cropSprite into the destination Surface), in the
//       original's order (kOps below; the names are offsets into f_strpool_4d21a4, the destinations offsets into the model
//       objects named there). The loops of the original are unrolled into the table: five okstates buttons, five report
//       digit columns (the first three with twelve digit cells and a marker each), seventeen roster buttons, twelve route
//       buttons whose rectangles come from f_tab_layoutInts_4ba62c (+0x6c x, +0x9c y, +0xcc w, +0xfc h), twenty-one buy-land
//       buttons and the two six-band tournament sets. `the image's height` (the four route screens, the roster scroll bar's
//       width and height) is the loaded image's own size. A load that fails leaves the previous image in place (the
//       original's Image keeps its surface) and is reported.
//   drawRoutingPoly.diamond(sx, sy, colour)
//       the routing map's tile (c3et.cpp Diamond): drawRoutingPoly(root surface, {sx - 6, sy, sx, sy - 3, sx + 6, sy, sx,
//       sy + 3}, 4, colour) = fillConvexPoly on the root window's surface (c3dv.cpp) = fillConvexPoly16 for the 16-bit
//       surface (gfxf::fill_convex_poly16, gfx_prim_fill.cpp) with the direct colour's low 16 bits. The vertex list lives
//       in the original's frame, so the port passes the centre and builds the list here.
//
// Negative controls (g_negctl_gfx_routing, sg_negctl_set "gfx.family.routing"):
//   1 the route buttons' rectangles are read one table entry later   2 the diamond is one pixel wider on the right
//   3 the four route screens are cut 1 pixel shorter than their image
//   4 loadInfoScreenArt.images cuts nothing (the call's hook-up in the boot without effect)
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/execonst.h"
#include "portsim/ptr.h"

namespace portsim {

int g_negctl_gfx_routing = 0;

namespace gfxf {
void fill_convex_poly16(Surface16 &d, const int32_t *pts, int n, uint16_t c);   // gfx_prim_fill.cpp
}

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_routing); }

// the destination objects of the manifest (model names; offsets inside them)
enum Obj : uint8_t {
  kRef821040, kRef820f40, kInfo8210c8, kRef821308, kRef821c60, kRef821200, kRef8223c0, kRef821f48, kRef8222b8, kRef821bf8,
  kRef8224c8, kInfo821070, kInfo821120, kRef821cb8,
};
uint32_t obj_va(Ctx &ctx, Obj o) {
  switch (o) {
    case kRef821040: return state_va(ctx, &GameState::g_spriteRef_821040);
    case kRef820f40: return state_va(ctx, &GameState::g_spriteRef_820f40);
    case kInfo8210c8: return state_va(ctx, &GameState::g_infoSprites8210c8);
    case kRef821308: return state_va(ctx, &GameState::g_spriteRef_821308);
    case kRef821c60: return state_va(ctx, &GameState::g_spriteRef_821c60);
    case kRef821200: return state_va(ctx, &GameState::g_spriteRef_821200);
    case kRef8223c0: return state_va(ctx, &GameState::g_spriteRef_8223c0);
    case kRef821f48: return state_va(ctx, &GameState::g_spriteRef_821f48);
    case kRef8222b8: return state_va(ctx, &GameState::g_spriteRef_8222b8);
    case kRef821bf8: return state_va(ctx, &GameState::g_spriteRef_821bf8);
    case kRef8224c8: return state_va(ctx, &GameState::g_spriteRef_8224c8);
    case kInfo821070: return state_va(ctx, &GameState::g_infoSprites821070);
    case kInfo821120: return state_va(ctx, &GameState::g_infoSprites821120);
    case kRef821cb8: return state_va(ctx, &GameState::g_spriteRef_821cb8);
  }
  return 0;
}

// one manifest step: a load (name = offset into f_strpool_4d21a4) or a cut into obj + off. w / h of -1 = the loaded
// image's width / height.
struct Op {
  bool load;
  uint32_t name;
  Obj obj;
  uint32_t off;
  int32_t x, y, w, h;
  bool trim;
};
constexpr Op L(uint32_t name) { return {true, name, kRef821040, 0, 0, 0, 0, 0, false}; }
constexpr Op C(Obj o, uint32_t off, int32_t x, int32_t y, int32_t w, int32_t h, bool trim) {
  return {false, 0, o, off, x, y, w, h, trim};
}

// LoadInfoScreenArt_re's calls in order (c3fv.cpp, one line each there with its instruction address)
const Op kHead[] = {
    L(0x578), C(kRef821040, 0, 0, 0, 0x320, 0x258, true),                                    // black.pcx
    L(0x554),                                                                                // okstates.pcx: 5 buttons
    C(kRef820f40, 0x00, 1, 1, 0x2C, 0x2A, true), C(kRef820f40, 0x2C, 0x2E, 1, 0x2C, 0x2A, true),
    C(kRef820f40, 0x58, 0x5B, 1, 0x2C, 0x2A, true), C(kRef820f40, 0x84, 1, 1, 0x2C, 0x2A, true),
    C(kRef820f40, 0xB0, 0x2E, 0x2C, 0x2C, 0x2A, true),
    L(0x52c), C(kInfo8210c8, 0, 8, 7, 0x310, 0x121, false),                                  // FINANCEreport
    L(0x4fc), C(kInfo8210c8, 0x2C, 8, 7, 0x310, 0x121, false),                               // FINANCEreport_alpha
    L(0x4d4), C(kRef821308, 0x00, 0, 0, 0x320, 0x68, false), C(kRef821308, 0x58, 0, 0x81, 0x320, 0x11, false),
    C(kRef821308, 0xB0, 0, 0x1A4, 0x320, 0x1D, false), C(kRef821308, 0x108, 0, 0x1D4, 0x320, 0x49, false),   // coursereport
};
// (the five digit columns follow kHead; then kTail)
const Op kTail[] = {
    L(0x4a4), C(kRef821308, 0x2C, 0, 0, 0x320, 0x68, false), C(kRef821308, 0x84, 0, 0x81, 0x320, 0x11, false),
    C(kRef821308, 0xDC, 0, 0x1A4, 0x320, 0x1D, false), C(kRef821308, 0x134, 0, 0x1D4, 0x320, 0x49, false),   // _alpha
    L(0x484), C(kRef821c60, 0, 0x23, 0x1A, 0x2DA, 0x1A3, false),                              // SGA
    L(0x458), C(kRef821c60, 0x2C, 0x23, 0x1A, 0x2DA, 0x1A3, false),                           // SGAreport_alpha
    L(0x434), C(kRef821200, 0x00, 0, 0, 0x320, 0xDC, false), C(kRef821200, 0x58, 0, 0x11A, 0x320, 0x10, false),
    C(kRef821200, 0xB0, 0, 0x15B, 0x320, 0x37, false),                                         // HoleSTAT
    L(0x408), C(kRef821200, 0x2C, 0, 0, 0x320, 0xDC, false), C(kRef821200, 0x84, 0, 0x11A, 0x320, 0x10, false),
    C(kRef821200, 0xDC, 0, 0x15B, 0x320, 0x37, false),                                         // HoleSTAT_alpha
    L(0x3e4), C(kRef8223c0, 0x00, 0x94, 0x2D, 0x1F9, 0x66, false), C(kRef8223c0, 0x58, 0x94, 0xE0, 0x1F9, 0x11, false),
    C(kRef8223c0, 0xB0, 0x94, 0x141, 0x1F9, 0x3D, false),                                      // PlayComt
    L(0x3b8), C(kRef8223c0, 0x2C, 0x94, 0x2D, 0x1F9, 0x66, false), C(kRef8223c0, 0x84, 0x94, 0xE0, 0x1F9, 0x11, false),
    C(kRef8223c0, 0xDC, 0x94, 0x141, 0x1F9, 0x3D, false),                                      // PlayComt_alpha
    L(0x390), C(kRef821f48, 0, 0, 0, 0x320, 0x258, false),                                    // memberRoster
    L(0x360), C(kRef821f48, 0x2C, 0, 0, 0x320, 0x258, false),                                 // memberRoster_alpha
};
const Op kTail2[] = {
    L(0x2fc), C(kRef821f48, 0x344, 0, 0, -1, -1, true),                                        // memberRoster_scrollbar
    L(0x2d8), C(kRef8222b8, 0x00, 0xC3, 0x2D, 0x19B, 0x4F, false), C(kRef8222b8, 0x58, 0xC3, 0xE0, 0x19B, 0x11, false),
    C(kRef8222b8, 0xB0, 0xC3, 0x141, 0x19B, 0x3D, false),                                      // lowscore
    L(0x2ac), C(kRef8222b8, 0x2C, 0xC3, 0x2D, 0x19B, 0x4F, false), C(kRef8222b8, 0x84, 0xC3, 0xE0, 0x19B, 0x11, false),
    C(kRef8222b8, 0xDC, 0xC3, 0x141, 0x19B, 0x3D, false),                                      // lowscore_alpha
    L(0x284), C(kRef821bf8, 0, 0, 0, 0x320, 0x258, false),                                    // histograph
    L(0x258), C(kRef821bf8, 0x2C, 0, 0, 0x320, 0x258, false),                                 // histograph_alpha
    // the four route screens: full width, the image's own height
    L(0x228), C(kRef8224c8, 0x2C, 0, 0, 0x320, -1, false),                                    // route screens_course
    L(0x1f8), C(kRef8224c8, 0x58, 0, 0, 0x320, -1, false),                                    // route screens_employ
    L(0x1c8), C(kRef8224c8, 0x84, 0, 0, 0x320, -1, false),                                    // route screens_aura
    L(0x198), C(kRef8224c8, 0xB0, 0, 0, 0x320, -1, false),                                    // route screens_value
    L(0x168), C(kRef8224c8, 0x00, 0, 0xFD, 0x320, 0x15B, false),                              // route screens_bottom
};
const Op kTail3[] = {
    L(0x144), C(kRef8224c8, 0x2EC, 0, 0, 0x320, 0x258, false),                                // buy_land
};
const Op kTail4[] = {
    L(0xf8), C(kInfo821070, 0, 0, 0, 0x320, 0x258, false),                                    // hire
    L(0xd0), C(kInfo821070, 0x2C, 0, 0, 0x320, 0x258, false),                                 // hire_alpha
    L(0xb8), C(kInfo821120, 0, 0, 0, 0x320, 0x258, false),                                    // PairBase
    L(0x9c), C(kInfo821120, 0x2C, 0, 0, 0x149, 0x88, false), C(kInfo821120, 0x58, 0, 0x88, 0x149, 0x88, false),
    C(kInfo821120, 0x84, 0, 0x110, 0x149, 0x88, false), C(kInfo821120, 0xB0, 0x2B5, 0x1F6, 0x4B, 0x4B, false),   // PairButtons
};

struct Manifest {
  Gfx &g;
  Ctx &ctx;
  uint32_t pool;
  Image8 img;
  bool have = false;
  void load(uint32_t name) {
    Image8 n;
    if (gfxb::load_image(g, ctx, pool + name, 0, 0, 0x100, 1, n) == 0) {
      img = std::move(n);
      have = true;
    } else {
      g.unsup("loadInfoScreenArt.images: a file that did not load");
    }
  }
  void cut(uint32_t dest, int32_t x, int32_t y, int32_t w, int32_t h, bool trim) {
    if (!have) {
      g.sprites.erase(dest);
      return;
    }
    gfxb::crop_sprite(g, dest, img, x, y, w < 0 ? img.w : w, h < 0 ? img.h : h, trim);
  }
  void run(const Op *ops, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      const Op &o = ops[i];
      if (o.load) {
        load(o.name);
        continue;
      }
      int32_t h = o.h;
      if (h < 0 && o.obj == kRef8224c8 && mode() == 3) h = img.h - 1;
      cut(obj_va(ctx, o.obj) + o.off, o.x, o.y, o.w, h, o.trim);
    }
  }
};

void b_info_art(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  if (mode() == 4) return;
  Manifest m{g, ctx, state_va(ctx, &GameState::f_strpool_4d21a4)};
  // f_tab_layoutInts_4ba62c lies in the exe's .rdata (the user's exe through ctx.exe()); tab(o) = its dword at byte offset o
  auto tab = [&](uint32_t o) { return ctx.exe().get<int32_t>("f_tab_layoutInts_4ba62c", o / 4); };
  m.run(kHead, sizeof(kHead) / sizeof(kHead[0]));
  // five report columns k: the first three cut twelve digit cells (x / w from the table +0x00 / +0x30, y = the column's
  // dword at +0x60 + 4k, height 0xf) into the 0x84-strided Surfaces from (row + 0x160) and a marker into row; every column
  // cuts (0xc2 + 0xf k, 0x22d) into row + 0x84 (row = g_spriteRef_821308 + 0x160 + 0x2c k)
  const uint32_t rows = obj_va(ctx, kRef821308) + 0x160;
  for (uint32_t k = 0; k < 5; ++k) {
    const uint32_t row = rows + 0x2Cu * k;
    if (k < 3) {
      const int32_t y = tab(0x60 + 4 * k);
      for (uint32_t j = 0; j < 12; ++j)
        m.cut(row + 0x160 + 0x84 * j, tab(4 * j), y, tab(0x30 + 4 * j), 0xF, true);
      m.cut(row, 0x223, static_cast<int32_t>(0x225 + 0xE * k), 0xB, 0xD, true);
    }
    m.cut(row + 0x84, static_cast<int32_t>(0xC2 + 0xF * k), 0x22D, 0xE, 0xF, true);
  }
  m.run(kTail, sizeof(kTail) / sizeof(kTail[0]));
  // memberRoster_buttons: seventeen buttons into g_spriteRef_821f48 + 0x58 + 0x2c i; c = -0x3e + 0x13 i picks the cell
  m.load(0x330);
  const uint32_t roster = obj_va(ctx, kRef821f48) + 0x58;
  for (int32_t i = 0, c = -0x3E; c < 0x105; ++i, c += 0x13) {
    const uint32_t dest = roster + 0x2Cu * static_cast<uint32_t>(i);
    if (c < 0xA6) m.cut(dest, (i >> 1) * 0x14 + 0x2E, (i & 1) ? 1 : 0x14, 0x13, 0x12, true);
    else if (c < 0xCC) m.cut(dest, c, 1, 0x12, 0x25, true);
    else m.cut(dest, -0x4C + 0x14 * i, 1, 0x13, 0x12, true);
  }
  m.run(kTail2, sizeof(kTail2) / sizeof(kTail2[0]));
  // the twelve route buttons into g_spriteRef_8224c8 + 0xdc + 0x2c i, rectangles from the layout table
  const uint32_t buttons = obj_va(ctx, kRef8224c8) + 0xDC;
  for (uint32_t i = 0; i < 12; ++i) {
    const uint32_t e = 4 * (i + (mode() == 1 ? 1u : 0u));
    m.cut(buttons + 0x2C * i, tab(0x6C + e), tab(0x9C + e), tab(0xCC + e), tab(0xFC + e), false);
  }
  m.run(kTail3, sizeof(kTail3) / sizeof(kTail3[0]));
  // buy_land_buttons: i < 18 a 9 x 2 grid of 0x3b x 0x3d cells into + 0x318 + 0x2c i; 18 / 19 into + 0x630 / + 0x65c;
  // 20 into + 0x318 + 0x2c * 20
  m.load(0x118);
  const uint32_t land = obj_va(ctx, kRef8224c8) + 0x318;
  for (int32_t i = 0; i < 0x15; ++i) {
    if (i < 0x12)
      m.cut(land + 0x2Cu * static_cast<uint32_t>(i), (i % 9) * 0x3C + 1, (i / 9) * 0x3F + 1, 0x3B, 0x3D, false);
    else if (i == 0x12)
      m.cut(obj_va(ctx, kRef8224c8) + 0x630, 1, 0x7F, 0xC0, 0x5F, false);
    else if (i == 0x13)
      m.cut(obj_va(ctx, kRef8224c8) + 0x65C, 0xC2, 0x7F, 0x40, 0x40, false);
    else
      m.cut(land + 0x2Cu * static_cast<uint32_t>(i), 0x103, 0x7F, 0x48, 0x3D, false);
  }
  m.run(kTail4, sizeof(kTail4) / sizeof(kTail4[0]));
  // tournament result and its alpha mask: six bands each, A = + 0x58 b, B = + 0x2c + 0x58 b (g_spriteRef_821cb8)
  static const int16_t kBand[6][2] = {{0, 0x6A}, {0xB4, 0x1A}, {0x7D, 0x16}, {0xE0, 0x12}, {0x165, 0x33}, {0x112, 0x33}};
  const uint32_t tourn = obj_va(ctx, kRef821cb8);
  for (uint32_t set = 0; set < 2; ++set) {
    m.load(set == 0 ? 0x70 : 0x3c);
    for (uint32_t b = 0; b < 6; ++b) m.cut(tourn + 0x2C * set + 0x58 * b, 0, kBand[b][0], 0x320, kBand[b][1], true);
  }
}

void b_diamond(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]);
  const uint32_t c = a[2];
  if (!(c & 0x80000000u)) {
    g.unsup("drawRoutingPoly: a palette colour index");
    return;
  }
  const int32_t r = mode() == 2 ? 7 : 6;
  const int32_t pts[8] = {sub32(x, 6), y, x, sub32(y, 3), add32(x, r), y, x, add32(y, 3)};
  gfxf::fill_convex_poly16(g.root, pts, 4, static_cast<uint16_t>(c & 0xffff));
}

const GfxBodyDef kBodies[] = {
    {"loadInfoScreenArt.images", b_info_art},
    {"drawRoutingPoly.diamond", b_diamond},
};

}  // namespace

extern const GfxFamily kGfxFamily_routing = {"routing", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
