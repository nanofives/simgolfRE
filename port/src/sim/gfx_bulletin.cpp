// Body family `bulletin` of the graphics backend (roadmap S2.11; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// bulletin board (the trophy shelf of showBulletinBoard). Specifications: c3em.cpp (ShowBulletinBoard_re) and c3fv.cpp
// (LoadInterfaceArt_re: the tables kIa11..kIa16 and the Row / TableRow cuts between them); functions are cited by name,
// their instruction addresses are in those sources.
//
//   loadInterfaceArt.bulletinArt
//       the board's cuts of loadInterfaceArt(1), every one kind 1, trim 0, from loadImage(img, name, 0, 0, 0x100, 2) with
//       the name an offset into f_strpool_4c95f4:
//         +0x3dc0 (kIa11's last load): Row(g_sprites53fd48, 11 cells at (9, 0xdd + 0x23 i), 0xf8 x 0x1d), Row(cells 11..21
//           at (0x223, 0xdd + 0x23 i)), kIa12's seven cuts (cells 22..28), Row(cells 29..31 at (0x119, 0xba + 0x23 i),
//           0xf8 x 0x1e);
//         +0x3dac (kIa13's load): kIa13's five cuts (cells 32..36), TableRow(cells 38..59: x, y from f_tab_point16_4c2d38
//           entries 0..21, w, h from its entries 25..46), TableRow(cells 67..69: entries 22..24 and 47..49);
//         +0x3d94 (kIa14) and +0x3d7c (kIa16): the same cuts into the masks g_sprites5a34f8 (kIa15's and kIa16's cuts and
//           the same Row / TableRow calls; the mask Row of cells 29..31 starts at x 0x11a).
//   showBulletinBoard.board
//       the board image: loadImage(board, f_strpool_4e3e28 + 0x30, g_palette5199c8, 0, 0x100, 0) into a frame Image of the
//       original (here a backend-local key) and Surface_fillRegion(board, root, 0, 0, 0x320, 0x258).
//   showBulletinBoard.event(x, y)
//       one listed event's frame, before its text line: cell 33 at (x - 7, y - 0xad).
//   showBulletinBoard.photo(x, y, drawn)
//       one listed event's photo: loadBmpSurface(photo, <the snapshot file name>) and Surface_blitThunk(photo, root, 0, 0, x,
//       y - 0xa6, 0xc8, 0xa0) are not modelled (the file is a saveBmp capture of the course, roadmap S3: reported), then the
//       pin Surface_stretchBlit(g_sprites53c570 cell 24 + drawn % 3, g_sprites5a4138 cell 24, root, x + 0x64, y - 0xb3, 0).
//   showBulletinBoard.fixed
//       the sprites after the event list, in the original's order: cell 34 at (0x258, 0x15e); the notes of the first three
//       records with a zero date (cell 38 + i, i = 0x1d / 0x1e / 0x1f for records 0 / 1 / 4 when g_difficulty < 2) at
//       (s / 2 + 0x268, s + 400), s growing by the short f_tab_point16_4c2d38 + 0x66 + 4 i minus 4; cells 27 (0x207, 0x18b),
//       28 (0xb1, 0x18b), 35 (0x33, 0x1e2), 36 (0x2dc, 0x1ff); with c = the dated records' count and l = -0xe c: 23 (0x11c,
//       l + 0x130), 26 at (0x120, l + 0x153) and (0x120, l + 0x14f); then, from l + 0x15d, the dated records' plates
//       (cell r, r mapped as for the notes) in reverse date order, 0xe apart, cell 25 after the first (2 further down);
//       22 (0x120, 0x15f) and 24 (0x139, 0x1e4). Each over the same cell of g_sprites5a34f8 (Surface_stretchBlit).
//
// Negative controls (g_negctl_gfx_bulletin, sg_negctl_set "gfx.family.bulletin"):
//   1 the board image drawn one pixel lower      2 the trophy (cell 22) one pixel to the right
//   3 the notes' x without the s / 2 term        4 the TableRow cuts read the w / h entries one entry later
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"

namespace portsim {

int g_negctl_gfx_bulletin = 0;

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_bulletin); }
bool control(int m) { return mode() == m; }

uint32_t pool(Ctx &ctx) { return state_va(ctx, &GameState::f_strpool_4c95f4); }
uint32_t board_key() { return gfx_local_key(0x71000); }

void call_body(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> a) {
  GfxBody b = gfxb::find_body(name);
  if (!b) {
    g.unsup(std::string("bulletin: no body ") + name);
    return;
  }
  uint32_t args[12] = {};
  int n = 0;
  for (uint32_t v : a) args[n++] = v;
  b(g, ctx, args, n);
}

// ------------------------------------------------------------------------------------------------ the art
struct Cuts {
  Gfx &g;
  Ctx &ctx;
  Image8 img;
  bool have = false;
  void load(uint32_t off) {
    img = Image8{};
    have = gfxb::load_image(g, ctx, pool(ctx) + off, 0, 0, 0x100, 2, img) == 0;
    if (!have) g.unsup("loadInterfaceArt.bulletinArt: a file that did not load");
  }
  void cut(uint32_t dest, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (have) gfxb::crop_sprite(g, dest, img, x, y, w, h, false);
  }
  void row(uint32_t dest, int n, int32_t x, int32_t y, int32_t dy, int32_t w, int32_t h) {
    for (int i = 0; i < n; ++i, dest += 0x2C, y += dy) cut(dest, x, y, w, h);
  }
  // TableRow: entry k of f_tab_point16_4c2d38 from xy (x, y) and from wh (w, h), 4 bytes per entry
  void table_row(uint32_t dest, uint32_t end, uint32_t xy, uint32_t wh) {
    const uint32_t t = state_va(ctx, &GameState::f_tab_point16_4c2d38);
    if (control(4)) wh += 4;
    for (uint32_t k = 0; dest < end; dest += 0x2C, k += 4)
      cut(dest, ctx.mem.load<int16_t>(t + xy + k), ctx.mem.load<int16_t>(t + xy + 2 + k), ctx.mem.load<int16_t>(t + wh + k),
          ctx.mem.load<int16_t>(t + wh + 2 + k));
  }
  // one set: the colour cells into `s` or the masks into `s` (the same rectangles); first / second = the two loads
  void set(uint32_t s, uint32_t first, uint32_t second) {
    load(first);
    row(s, 11, 9, 0xDD, 0x23, 0xF8, 0x1D);
    row(s + 0x2C * 11, 11, 0x223, 0xDD, 0x23, 0xF8, 0x1D);
    static const int16_t k12[7][4] = {{0x11B, 0x165, 0xF8, 0x92}, {0x114, 0x124, 0x101, 0x2F}, {0x13D, 0x1EA, 0xB7, 0x34},
                                      {0x9, 0xBA, 0xF8, 0x13},     {0x9, 0x74, 0xF8, 0x1E},     {0x1FE, 0xD, 0x84, 0xA2},
                                      {0x289, 0xD, 0x82, 0xA3}};
    for (uint32_t i = 0; i < 7; ++i) cut(s + 0x2C * (22 + i), k12[i][0], k12[i][1], k12[i][2], k12[i][3]);
    row(s + 0x2C * 29, 3, s == state_va(ctx, &GameState::g_sprites53fd48) ? 0x119 : 0x11A, 0xBA, 0x23, 0xF8, 0x1E);
    load(second);
    static const int16_t k13[5][4] = {{0x29C, 0x1FF, 0x35, 0x34}, {0x3F, 0xFB, 0xE5, 0xD1}, {0x3A, 0x31, 0xCF, 0xB2},
                                      {0x33, 0x1E2, 0xB8, 0x50},  {0x2DC, 0x1FF, 0x35, 0x34}};
    static const uint32_t cell13[5] = {32, 33, 34, 35, 36};
    for (uint32_t i = 0; i < 5; ++i) cut(s + 0x2C * cell13[i], k13[i][0], k13[i][1], k13[i][2], k13[i][3]);
    table_row(s + 0x2C * 38, s + 0x2C * 60, 0x00, 0x64);
    table_row(s + 0x2C * 67, s + 0x2C * 70, 0x58, 0xBC);
  }
};

void b_bulletin_art(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  Cuts c{g, ctx};
  c.set(state_va(ctx, &GameState::g_sprites53fd48), 0x3DC0, 0x3DAC);
  c.set(state_va(ctx, &GameState::g_sprites5a34f8), 0x3D94, 0x3D7C);
}

// ------------------------------------------------------------------------------------------------ the board
void blt(Gfx &g, Ctx &ctx, uint32_t cell, int32_t x, int32_t y) {
  call_body(g, ctx, "Surface_stretchBlit",
            {state_va(ctx, &GameState::g_sprites53fd48) + 0x2C * cell, state_va(ctx, &GameState::g_sprites5a34f8) + 0x2C * cell,
             g.root_va(ctx), static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0});
}

void b_board(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  const uint32_t key = board_key();
  g.images.erase(key);
  call_body(g, ctx, "loadImage",
            {key, state_va(ctx, &GameState::f_strpool_4e3e28) + 0x30, state_va(ctx, &GameState::g_palette5199c8), 0, 0x100, 0});
  if (!g.images.count(key)) {
    g.unsup("showBulletinBoard.board: the board image did not load");
    return;
  }
  call_body(g, ctx, "Surface_fillRegion", {key, g.root_va(ctx), 0, control(1) ? 1u : 0u, 0x320, 0x258});
}

void b_event(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  blt(g, ctx, 33, sub32(static_cast<int32_t>(a[0]), 7), sub32(static_cast<int32_t>(a[1]), 0xAD));
}

void b_photo(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]), drawn = static_cast<int32_t>(a[2]);
  g.unsup("showBulletinBoard: a photo (loadBmpSurface of a saveBmp capture of the course, roadmap S3)");
  call_body(g, ctx, "Surface_stretchBlit",
            {state_va(ctx, &GameState::g_sprites53c570) + 0x2Cu * (24 + static_cast<uint32_t>(drawn % 3)),
             state_va(ctx, &GameState::g_sprites5a4138) + 0x2C * 24, g.root_va(ctx), static_cast<uint32_t>(add32(x, 0x64)),
             static_cast<uint32_t>(sub32(y, 0xB3)), 0});
}

// a record index as the board maps it (records 0, 1, 4 take cells 0x1d, 0x1e, 0x1f when g_difficulty < 2)
int32_t mapped(Ctx &ctx, int32_t i) {
  if (ctx.gs().g_difficulty < 2) {
    if (i == 0) return 0x1D;
    if (i == 1) return 0x1E;
    if (i == 4) return 0x1F;
  }
  return i;
}

void b_fixed(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  const Memory &m = ctx.mem;
  const uint32_t ev = state_va(ctx, &GameState::g_bulletinEvents) + 0x28;
  auto date = [&](int32_t i) { return m.load<int32_t>(ev + 0x30 * static_cast<uint32_t>(i)); };
  // the dated records in increasing date order (the original's selection sort: the first smallest date wins)
  int32_t order[22];
  bool used[22] = {};
  int32_t count = 0;
  for (;;) {
    int32_t best = -1, best_date = 0x7ffffff;
    for (int32_t i = 0; i < 22; ++i)
      if (date(i) != 0 && !used[i] && date(i) < best_date) {
        best_date = date(i);
        best = i;
      }
    if (best == -1) break;
    order[count++] = best;
    used[best] = true;
  }
  blt(g, ctx, 34, 0x258, 0x15E);
  const uint32_t tab = state_va(ctx, &GameState::f_tab_point16_4c2d38);
  int32_t s = 0, slots = 0;
  for (int32_t i = 0; i < 0x16; ++i) {
    if (date(i) != 0) continue;
    blt(g, ctx, 38 + static_cast<uint32_t>(mapped(ctx, i)), control(3) ? 0x268 : add32(s / 2, 0x268), add32(s, 400));
    ++slots;
    s = add32(s, -4 + m.load<int16_t>(tab + 0x66 + 4 * static_cast<uint32_t>(i)));
    if (slots > 2) break;
  }
  blt(g, ctx, 27, 0x207, 0x18B);
  blt(g, ctx, 28, 0xB1, 0x18B);
  blt(g, ctx, 35, 0x33, 0x1E2);
  blt(g, ctx, 36, 0x2DC, 0x1FF);
  int32_t ly = mul32(count, -0xE);
  blt(g, ctx, 23, 0x11C, add32(ly, 0x130));
  blt(g, ctx, 26, 0x120, add32(ly, 0x153));
  blt(g, ctx, 26, 0x120, add32(ly, 0x14F));
  ly = add32(ly, 0x15D);
  for (int32_t k = 0; k < count; ++k) {
    blt(g, ctx, static_cast<uint32_t>(mapped(ctx, order[count - 1 - k])), 0x120, ly);
    int32_t next = add32(ly, 0xE);
    if (k == 0) {
      blt(g, ctx, 25, 0x120, next);
      next = add32(ly, 0x10);
    }
    ly = next;
  }
  blt(g, ctx, 22, control(2) ? 0x121 : 0x120, 0x15F);
  blt(g, ctx, 24, 0x139, 0x1E4);
}

const GfxBodyDef kBodies[] = {
    {"loadInterfaceArt.bulletinArt", b_bulletin_art},
    {"showBulletinBoard.board", b_board},
    {"showBulletinBoard.event", b_event},
    {"showBulletinBoard.photo", b_photo},
    {"showBulletinBoard.fixed", b_fixed},
};

}  // namespace

extern const GfxFamily kGfxFamily_bulletin = {"bulletin", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
