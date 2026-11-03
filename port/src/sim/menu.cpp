// Module `menu` (phase 3.8, stream 38j): loadWorldAssets (simulation part), startNewCourse and pickAProScreen, the callees of
// mainLoop's title menu and session dispatch no other module ports, the sprite-sheet allocation they share (allocBlock,
// freeBlock, freeTable), plus the pseudo-function tick.menu (the negative-control switch of the menu / session bodies in
// tick_menu.inc). See portsim/sim/menu.h. Written from the C3 hook sources shim/src/re/c3fv.cpp (loadWorldAssets), c3ev.cpp
// (startNewCourse), c3fw.cpp (pickAProScreen), c3b.cpp (allocBlock, freeBlock) and from the instructions where the comments
// say so (loadFlic, a C2 row; freeTable); no original address or text.
#include "portsim/sim/menu.h"

#include <cstddef>
#include <cstring>
#include <string>

#include "commands_internal.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/course.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/flic.h"
#include "portsim/sim/loaders.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   loadWorldAssets  1 = the object-type table is not copied from its initial image, 2 = type 3's last name write (the
//                    build-item name at +0xb4) skipped, 3 = a free-list allocation leaves the cell cursor at the sheet's end, 4 = the
//                    terrain buttons not cut (their geometry left 0), 5 = the tool strip not cut
//   loadInterfaceArt 1 = the ranking cells drawn by Random::range(3), 2 = (full != 0) the first interface flic not loaded,
//                    3 = (full != 0) the palette cuts not stored
//   allocBlock       1 = a region of exactly n is carved instead of consumed (its length 0 left in place)
//   freeBlock        1 = the region before base grows by n + 1
//   startNewCourse   1 = the date is 0x2c01, 2 = a refused pro list leaves the flags set
//   pickAProScreen   1 = a row press loads the row below, 2 = the load button does not load the selection again,
//                    3 = the theme name is not restored on the load exit
//   showThemePackSelect 1 = a row press does not change g_theme, 2 = Cancel keeps the theme
//   tick.menu        (the menu / session bodies, tick_menu.inc) 1 = buildGolferBioText dropped from the menu's session reset,
//                    2 = g_date not reset by the menu, 3 = Session's course type taken from the next course record,
//                    4 = Session's building unlock level 7 instead of 6 on the new-course path
int g_negctl_load_world_assets = 0;
int g_negctl_load_interface_art = 0;
int g_negctl_seed_from_time = 0;   // 1 = the clock value + 1
int g_negctl_alloc_block = 0;
int g_negctl_free_block = 0;
int g_negctl_start_new_course = 0;
int g_negctl_pick_a_pro = 0;
int g_negctl_theme_pack = 0;
int g_negctl_menu = 0;

std::string read_str(const Memory &m, uint32_t s) {
  std::string r;
  for (uint32_t p = s;; ++p) {
    const uint8_t b = m.load<uint8_t>(p);
    if (b == 0) break;
    r.push_back(static_cast<char>(b));
  }
  return r;
}
void write_str(const Memory &m, uint32_t d, const std::string &s) {
  for (size_t k = 0; k <= s.size(); ++k)
    m.store<uint8_t>(d + static_cast<uint32_t>(k), k < s.size() ? static_cast<uint8_t>(s[k]) : 0);
}
}  // namespace

// ------------------------------------------------------------------------------------------------ the cell pool
// allocBlock(n) (c3b.cpp; the vector corpus has its vectors): first fit over the 100 free regions (start g_cellFreeStart[i]
// >= 0, length g_cellFreeLen[i] >= n): the region's front is carved off, or the region consumed when it is exactly n (length
// 0, start -1); returns its old start, -1 when none fits
int32_t alloc_block(Ctx &ctx, int32_t n) {
  GameState &gs = ctx.gs();
  for (int i = 0; i < 100; ++i) {
    if (gs.g_cellFreeStart[i] < 0 || gs.g_cellFreeLen[i] < n) continue;
    const int32_t r = gs.g_cellFreeStart[i];
    if (gs.g_cellFreeLen[i] > n || SG_NEGCTL(g_negctl_alloc_block) == 1) {
      gs.g_cellFreeStart[i] = add32(r, n);
      gs.g_cellFreeLen[i] = sub32(gs.g_cellFreeLen[i], n);
    } else {
      gs.g_cellFreeLen[i] = 0;
      gs.g_cellFreeStart[i] = -1;
    }
    return r;
  }
  return -1;
}
// freeBlock(base, n) (c3b.cpp): a region ending at base grows by n and absorbs every region that starts at its new end;
// otherwise (base, n) goes into the first empty slot (start < 0); none empty: nothing
void free_block(Ctx &ctx, int32_t base, int32_t n) {
  GameState &gs = ctx.gs();
  for (int i = 0; i < 100; ++i) {
    if (gs.g_cellFreeStart[i] < 0 || base != add32(gs.g_cellFreeStart[i], gs.g_cellFreeLen[i])) continue;
    gs.g_cellFreeLen[i] = add32(gs.g_cellFreeLen[i], add32(n, SG_NEGCTL(g_negctl_free_block) == 1 ? 1 : 0));
    for (int j = 0; j < 100; ++j)
      if (gs.g_cellFreeStart[j] == add32(gs.g_cellFreeStart[i], gs.g_cellFreeLen[i])) {
        gs.g_cellFreeLen[i] = add32(gs.g_cellFreeLen[i], gs.g_cellFreeLen[j]);
        gs.g_cellFreeStart[j] = -1;
        gs.g_cellFreeLen[j] = 0;
      }
    return;
  }
  for (int i = 0; i < 100; ++i)
    if (gs.g_cellFreeStart[i] < 0) {
      gs.g_cellFreeStart[i] = base;
      gs.g_cellFreeLen[i] = n;
      return;
    }
}
// freeTable(id) (re/analysis/util/0043d670_freeTable.md, from its instructions): a loaded sheet (first cell != -1, +0x13)
// gives its columns * rows cells back (freeBlock, +0x30), its cell surfaces are released one by one (the renderer's, +0x55),
// its columns become 0 (+0x64) and its first cell -1 (+0x6c); its rows stay
void free_table(Ctx &ctx, int32_t id) {
  GameState &gs = ctx.gs();
  if (gs.g_sheetFirstCell[id] == -1) return;
  free_block(ctx, gs.g_sheetFirstCell[id], mul32(gs.g_sheetCols[id], gs.g_sheetRows[id]));
  gs.g_sheetCols[id] = 0;
  gs.g_sheetFirstCell[id] = -1;
}

// ------------------------------------------------------------------------------------------------ loadWorldAssets
// The sprite-sheet tables loadWorldAssets rebuilds are the renderer's (class render), but mainLoop's drawing passes store the
// address of a sheet cell (cellAt: g_cellPool + 0x2c * (g_sheetCols[s] * row + g_sheetFirstCell[s] + col)) into the sim word
// unk_005a9cc8, so where each sheet lands in the cell pool is simulation input: the port keeps the allocation (the free
// regions, the cursor, each sheet's first cell, columns and rows) and drops the pixels.
namespace {
#define GSO(sym, off) (static_cast<uint32_t>(offsetof(GameState, sym)) + static_cast<uint32_t>(off))
struct FlicCall {
  uint32_t id, path;    // the sheet; the flic's name (a GameState offset of the exe's string)
  int32_t frames, delay, flag;
};
#include "menu_flics.inc"
#undef GSO

// loadWorldAssets' release of a sheet range (c3fv.cpp FreeRange): every sheet first .. last that is loaded is freed, then its
// first cell is -1 and the first byte of its path 0
void free_range(Ctx &ctx, int32_t first, int32_t last) {
  GameState &gs = ctx.gs();
  for (int32_t id = first; id <= last; ++id) {
    if (gs.g_sheetFirstCell[id] != -1) free_table(ctx, id);
    gs.g_sheetFirstCell[id] = -1;
    gs.g_sheetPath[50 * id] = 0;
  }
}

// The column count flicOpenStream (c3eu.cpp) leaves for a flic: Flic::open maps the file (missing: no stream, 0);
// Flic::buildFrameTable (c3fa.cpp) wants the FLIC magic word 0xaf12 at +4 and depth 8 at +0xc (else no stream, 0), keeps a
// header already converted (dword +0x1a = 0xf1f1f2f2) and converts another one with word +0x62 = its frame count word +6;
// the count is the header's word +0x62. Every flic of the shipped game is converted (log/p38/38j_notes.md section 4).
int32_t flic_cols(Ctx &ctx, const std::string &name) {
  uint8_t h[0x64] = {0};
  if (ctx.files().read(name, 0, h, sizeof h) < static_cast<int64_t>(sizeof h)) return 0;
  auto w16 = [&](int o) { return static_cast<int32_t>(h[o] | (h[o + 1] << 8)); };
  const uint32_t mark = static_cast<uint32_t>(h[0x1a]) | static_cast<uint32_t>(h[0x1b]) << 8 |
                        static_cast<uint32_t>(h[0x1c]) << 16 | static_cast<uint32_t>(h[0x1d]) << 24;
  if (w16(4) != 0xaf12 || w16(0xc) != 8) return 0;
  return mark == 0xf1f1f2f2u ? w16(0x62) : w16(6);
}

// loadFlic(id, path, frames, delay, flag) (re/analysis/render/0043d740_loadFlic.md, a C2 row: written from its instructions,
// cited by offset). The prefix: g_flicLoadCount + 1 (+0x0..+0x15); on every 100th load waitTicks(10, 0) and, while
// g_difficulty is -1 (boot only: a session sets it from the menu first), showLoadingScreen(count / 4) (+0x1f..+0x34); then
// doAllTasks (+0x54: a Load pump, U-1051: it takes the one dispatch a trace puts before the load's first palette QPC). A delay of -1 loads one frame
// (+0x5d..+0x62). The sheet id + g_sheetBase (+0x87..+0x91) gets the path (+0xab..+0xd1) and its columns from the flic
// (flicOpenStream +0x231; an optional shadow flic's open writes the same word first, +0x178), rows = frames (+0x266). The
// cells: with a free list (g_cellFreeStart[0] != -2, +0x263) allocBlock(rows * columns) (+0x279); none fits (-1) or no free
// list: from the cursor's cell (+0x290..+0x2a1). Past cell 0xdd40 it reports <Too many flics> (scrollTextWindow, +0x2c0) and
// keeps no cells; otherwise the sheet's first cell is set (+0x2d8) and the cursor walks the sheet's cells (one per frame and
// column; one cell for delay -1, +0x447). A free-list allocation puts the cursor back where it was (+0x488..+0x498).
void load_flic(Ctx &ctx, const FlicCall &f, int mode) {
  GameState &gs = ctx.gs();
  const int32_t c = add32(gs.g_flicLoadCount, 1);
  gs.g_flicLoadCount = c;
  if (c % 100 == 0 && gs.g_difficulty == -1) {
    cm::call_peer(ctx, "showLoadingScreen", {static_cast<uint32_t>(c / 4)});
    // roadmap S2.7, with an attached graphics backend only: the Window::refreshFull(root, 0) that follows the loading screen
    // (+0x3c..+0x43), then waitTicks(10, 0) (+0x48..+0x4c) over the source's clock reads at its three sites (waitTicks+0x33,
    // +0x37, +0x63; offsets from the frame-head return address as tick.cpp keys them), so a checkpoint taken inside that wait
    // shows this loading screen. Negative controls 11 / 12 of gfx.family.boot_screens leave out one each.
    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {
#if PORTSIM_NEGCTL
      static const int fam = registry_find("gfx.family.boot_screens");
      const FuncDef *fd = fam >= 0 ? registry_at(fam, nullptr) : nullptr;
      const int neg = fd && fd->negctl ? *fd->negctl : 0;
#else
      const int neg = 0;
#endif
      if (neg != 11) {
        const uint32_t a[2] = {state_va(ctx, &GameState::g_rootWindow), 0};
        gfx_drop(ctx, "Window::refreshFull", a, 2);
      }
      InputSource *src = input_source(ctx);
      constexpr int32_t kSiteWait = 0x4c384;
      uint32_t t0 = 0, t = 0;
      if (neg != 12 && src && src->clock(kSiteWait, t0) && src->clock(kSiteWait + 4, t))
        while (t - t0 < 100u && src->clock(kSiteWait + 0x30, t)) {
        }
    }
  }
  cm::pump_load(ctx);   // +0x54 doAllTasks
  const int32_t frames = f.delay == -1 ? 1 : f.frames;
  const int32_t id = add32(static_cast<int32_t>(f.id), gs.g_sheetBase);
  const uint32_t path = ctx.mem.data_base() + f.path;
  vstrcpy(ctx.mem, state_va(ctx, &GameState::g_sheetPath) + static_cast<uint32_t>(mul32(id, 50)), path);
  // the flic's file name in the text buffer (+0x1a5..+0x21d, after the shadow flic's name for flag != 0, +0xd9..+0x154)
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c8a28);
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  if (f.flag != 0) {   // the shadow flic's name (its open writes the column word the main open writes again)
    vstrcpy(ctx.mem, text, pool + 0x3c);
    vstrcat(ctx.mem, text, path);
    vstrcat(ctx.mem, text, pool + 0x30);
  }
  vstrcpy(ctx.mem, text, pool + 0x3c);
  vstrcat(ctx.mem, text, path);
  vstrcat(ctx.mem, text, pool + 0x28);
  const int32_t cols = flic_cols(ctx, flic::resolve_name(ctx.files(), read_str(ctx.mem, text)));
  gs.g_sheetCols[id] = cols;
  gs.g_sheetRows[id] = frames;
  int32_t saved = gs.g_cellCursor, base;
  if (gs.g_cellFreeStart[0] != -2) {
    base = alloc_block(ctx, mul32(frames, cols));
    gs.g_cellCursor = base;
  } else {
    saved = -1;
    base = gs.g_cellCursor;
  }
  if (base == -1) {
    base = saved;
    saved = -1;
    gs.g_cellCursor = base;
  }
  if (add32(mul32(cols, frames), base) >= 0xdd40) {
    cm::call_peer(ctx, "scrollTextWindow", {pool + 0x14, 100, 100});
  } else {
    gs.g_sheetFirstCell[id] = base;
    const int32_t cells = frames > 0 && cols > 0 ? (f.delay == -1 ? 1 : mul32(frames, cols)) : 0;
    gs.g_cellCursor = add32(base, cells);
    // the cells' pixels for an attached graphics backend (S3.4, portsim/sim/flic.h): loadFlic's frame-by-frame cut of the
    // flic into the sheet's cells g_cellPool[base ..], judged against the original's sheet memory (port/test/test_flic.py)
    if (g_gfx_attached.load(std::memory_order_relaxed) != 0)
      flic::sheet_to_gfx(ctx, path, f.frames, f.delay, f.flag,
                         state_va(ctx, &GameState::g_cellPool) + 0x2cu * static_cast<uint32_t>(base));
  }
  if (saved != -1 && mode != 3) gs.g_cellCursor = saved;
}

void run(Ctx &ctx, const FlicCall *t, int n, int mode) {
  for (int k = 0; k < n; ++k) load_flic(ctx, t[k], mode);
}
// cropSprite (its hook in c3ee.cpp) into a palette SpriteRef stores the cut's kind (+0x28), width (+0x18) and height
// (+0x1c) before it reads a pixel; the trimmed size and offsets (+0x10, +0x14, +0x20, +0x24) come from the pixels and are the
// renderer's. The build palette's hit test (hitTestSlot432f90) reads the height.
void cut_geometry(Ctx &ctx, uint32_t sp, uint32_t w, uint32_t h, int32_t kind) {
  ctx.mem.store<int32_t>(sp + 0x28, kind);
  ctx.mem.store<uint32_t>(sp + 0x18, w);
  ctx.mem.store<uint32_t>(sp + 0x1c, h);
}

}  // namespace
#define RUN(t) run(ctx, t, t##_n, mode)

// loadWorldAssets (c3fv.cpp LoadWorldAssets_re, which cites every instruction): its loadFlic calls, sheet releases and table
// writes in their order; the palettes, images and cut rectangles between them are the renderer's
int32_t load_world_assets(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_load_world_assets);
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int8_t t = static_cast<int8_t>(gs.g_courseType);
  const bool known = static_cast<uint32_t>(static_cast<int32_t>(t)) <= 3u;   // the jump tables' unsigned range test
  const uint32_t attrs = state_va(ctx, &GameState::g_tileTypeAttrs);
  const uint32_t items = state_va(ctx, &GameState::f_tab_buildItems_4c26b0);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c95f4);
  const uint32_t pool2 = state_va(ctx, &GameState::f_strpool_4c8a28);
  gs.g_flag4c89a8 = 0;                         // loadWorldAssets+0x2c6d
  // phase 5 (roadmap S3.6): an attached graphics backend cuts the build palette's images (gfx_hud.cpp)
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, "loadWorldAssets.images", nullptr, 0);
  // roadmap S3.5: the palette objects the type's loads fill (gfx_scene.cpp), which the sprite queue draws with
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, "loadWorldAssets.palettes", nullptr, 0);
  // phase 4 (the boot path): the terrain buttons (loadWorldAssets+0xcc3..+0xd27, c3fv.cpp): 13 rows of 4 cells of 0x3e x 0x36
  // cut into the 52 SpriteRefs from g_sprites570cb8 (stride 0x2c). The pixels are the renderer's; the geometry the cut leaves
  // is read by the build palette's hit test (hitTestSlot432f90, the height at +0x1c): the size twice (+0x10 / +0x14 and
  // +0x18 / +0x1c), 0 at +0x20 / +0x24 and 1 at +0x28 (measured: all 52 in the frame-head images of cov_b_career_types,
  // course_season, championship and cov_k1_animals; the exe's initial image holds 0). Every course-type branch loads a
  // palette image into the working image first, so the cut runs for every type.
  for (uint32_t i = 0; i < 52 && mode != 4; ++i) {
    const uint32_t sp = state_va(ctx, &GameState::g_sprites570cb8) + 0x2c * i;
    m.store<int32_t>(sp + 0x10, 0x3e);
    m.store<int32_t>(sp + 0x14, 0x36);
    m.store<int32_t>(sp + 0x18, 0x3e);
    m.store<int32_t>(sp + 0x1c, 0x36);
    m.store<int32_t>(sp + 0x20, 0);
    m.store<int32_t>(sp + 0x24, 0);
    m.store<int32_t>(sp + 0x28, 1);
  }
  // the tool strip (+0xd29..+0xef6, c3fv.cpp): per course type 0..3 three cells per column, x from 0x208 in steps of 0x50
  // while below 0x2f8, into SpriteRefs 52 + k, 56 + k and 60 + k at y 0 / 0x96 / 0x12c, sizes from the type's table
  if (known) {
    static const int32_t kStrip[4][6] = {{0x41, 0x54, 0x3E, 0x46, 0x3E, 0x58}, {0x4B, 0x4A, 0x3E, 0x43, 0x41, 0x68},
                                         {0x48, 0x4F, 0x3E, 0x51, 0x41, 0x68}, {0x43, 0x54, 0x3E, 0x58, 0x43, 0x4C}};
    const int32_t *s = kStrip[t];
    const uint32_t spr = state_va(ctx, &GameState::g_sprites570cb8);
    for (uint32_t k = 0; k < 3; ++k)
      for (uint32_t r = 0; r < 3; ++r)
        if (mode != 5) cut_geometry(ctx, spr + 0x2c * (52 + 4 * r + k), static_cast<uint32_t>(s[2 * r]), static_cast<uint32_t>(s[2 * r + 1]), 1);
  }
  gs.g_sheetBase = 0;                          // +0xfd8
  free_range(ctx, 0x1B6, 0x207);               // +0xfde..+0x1011
  free_range(ctx, 0x22E, 0x23C);               // +0x1013..+0x1046
  // buildings and homes: four independent compares (+0x1048, +0x152f, +0x1a39, +0x1f9b)
  if (t == 2) RUN(kWaFlicTrop);
  if (t == 1) RUN(kWaFlicDesert);
  if (t == 3) RUN(kWaFlicLinks);
  if (t == 0) RUN(kWaFlicPark);
  // scenery, each type ending in the waterfall top (+0x2dfe)
  switch (t) {
    case 0: RUN(kWaSwPark); load_flic(ctx, kFlicWaterTop_park, mode); break;
    case 1: RUN(kWaSwDesert); load_flic(ctx, kFlicWaterTop_desert, mode); break;
    case 2: RUN(kWaSwTrop); load_flic(ctx, kFlicWaterTop_trop, mode); break;
    case 3: RUN(kWaSwLinks); load_flic(ctx, kFlicWaterTop_links, mode); break;
    default: break;
  }
  // the object-type table (+0x33a1): for a type 0..3 its initial image (0x114 dwords), then the type's names and bytes
  if (known && mode != 1)
    std::memcpy(m.at(attrs, 0x450, true), m.at(state_va(ctx, &GameState::f_tab_tileTypeAttrInit_4c1a40), 0x450), 0x450);
  auto cpy = [&](uint32_t d, uint32_t s) { vstrcpy(m, d, s); };
  uint32_t marina = 0;
  switch (t) {
    case 0:   // Parkland
      cpy(attrs + 480, pool + 216); cpy(attrs + 528, pool + 332); cpy(attrs + 864, pool + 204);
      cpy(items + 140, pool + 244); cpy(items + 280, pool + 192); cpy(items + 180, pool + 176);
      marina = pool + 168;
      break;
    case 1:   // Desert
      cpy(attrs + 192, pool + 356); cpy(attrs + 240, pool + 348); cpy(attrs + 528, pool + 340);
      cpy(attrs + 480, pool + 332); cpy(attrs + 672, pool + 324); cpy(attrs + 688, pool + 316);
      cpy(attrs + 624, pool + 304); cpy(attrs + 640, pool + 288); cpy(attrs + 720, pool + 276);
      cpy(attrs + 736, pool + 264); cpy(attrs + 864, pool + 256);
      m.store<uint8_t>(attrs + 226, 1);
      m.store<uint8_t>(attrs + 278, 5);
      m.store<uint8_t>(attrs + 904, 0);
      m.store<uint32_t>(attrs + 908, 0);
      cpy(items + 140, pool + 244); cpy(items + 280, pool + 236); cpy(items + 180, pool + 232);
      marina = pool + 224;
      break;
    case 2:   // Tropical
      cpy(attrs + 480, pool + 216); cpy(attrs + 528, pool + 332); cpy(attrs + 624, pool + 152);
      cpy(attrs + 640, pool + 136); cpy(attrs + 672, pool + 120); cpy(attrs + 688, pool + 104);
      cpy(attrs + 720, pool + 276); cpy(attrs + 736, pool + 264); cpy(attrs + 864, pool + 204);
      cpy(items + 140, pool + 244); cpy(items + 280, pool + 92); cpy(items + 180, pool + 80);
      marina = pool + 168;
      break;
    case 3:   // Links
      cpy(attrs + 480, pool + 72); cpy(attrs + 528, pool + 64); cpy(attrs + 672, pool + 56);
      cpy(attrs + 688, pool + 48); cpy(attrs + 624, pool + 36); cpy(attrs + 640, pool + 24);
      cpy(attrs + 720, pool + 12); cpy(attrs + 736, pool); cpy(items + 140, state_va(ctx, &GameState::f_strpool_4c95f0));
      cpy(items + 180, pool + 232); cpy(items + 240, pool + 224); cpy(items + 280, pool2 + 3008);
      if (mode != 2) cpy(items + 180, pool2 + 3000);
      marina = pool2 + 2992;
      break;
    default:
      break;
  }
  if (known) cpy(items + 240, marina);         // +0x3abd
  free_range(ctx, 0x190, 0x1B3);               // the third range (+0x3aec..+0x3b25)
  // flowers and sailboats (+0x3b27): Desert / Tropical their own; the others the park or links set, the sailboats and the
  // common flowers; every type ends in the corner flowers (+0x420a)
  if (t == 1) {
    RUN(kWaFDesert);
    load_flic(ctx, kFlicCorner_desert, mode);
  } else if (t == 2) {
    RUN(kWaFTrop);
    load_flic(ctx, kFlicCorner_trop, mode);
  } else {
    if (t == 0) {
      RUN(kWaFPark);
      load_flic(ctx, kFlicSail_park, mode);
    } else {
      RUN(kWaFLinks);
      load_flic(ctx, kFlicSail_other, mode);
    }
    RUN(kWaFCommon);
    load_flic(ctx, kFlicCorner_other, mode);
  }
  if (t == 2) RUN(kWaGTrop);                   // +0x4217
  if (t == 0) RUN(kWaGPark);                   // +0x42ac
  if (t == 1) RUN(kWaGDesert);                 // +0x433d
  // large trees (+0x43ce): Links (tested twice, the type unchanged in between), Desert / Tropical, the others; the shared
  // tail 0x197 (+0x4513)
  if (t == 3) {
    RUN(kWaGLinks);
    RUN(kWaGLinks2);
    load_flic(ctx, kFlicBigTree_links2, mode);
  } else if (t == 1 || t == 2) {
    RUN(kWaGPalmDT);
    load_flic(ctx, kFlicBigTree_palmdt, mode);
  } else {
    RUN(kWaGPalm);
    load_flic(ctx, kFlicBigTree_palm, mode);
  }
  return 0;
}
#undef RUN

// ------------------------------------------------------------------------------------------------ loadInterfaceArt
// loadInterfaceArt(full) (c3fv.cpp LoadInterfaceArt_re, which cites every instruction), the new-game part: genNoise and
// clearBuffers (through the registry), the course's tables cleared (holes, golfers, walkers, the five histories, the class
// slot counts, the finance years, the golfer statistics, the course ranking; the hole count g_holesPlusOne = 1, the hole
// points -1, tile flags, ball stops and corner levels 0, the theme slots 0..7, every placed object's type -1, the challenge
// level 0); nine ranking rows drawn by Random::range(9) on g_rng until an empty one comes up, the i-th (i = 0..8) getting
// i + 1 marks in its cells Random::range(4); then with full == 0 loadWorldAssets. full != 0 (mainLoop's Setup only, phase 4:
// the boot path) runs the full arm instead: menu_iart.inc's steps in the original's order (the phrase table, the sheet tables
// and the cell free list reset, every interface loadFlic, the stores between the loads and the two golfer-head scans); the
// palettes, images and cut rectangles between them are the renderer's.
namespace {
enum IaKind : uint8_t { IaSet, IaFill, IaFlic, IaHeads, IaCut };
struct IaStep {
  IaKind k;
  uint32_t a, b, c;
  int32_t frames, delay, flag;
};
#define GSO(sym, off) (static_cast<uint32_t>(offsetof(GameState, sym)) + static_cast<uint32_t>(off))
#include "menu_iart.inc"
#undef GSO

// LoadHeads (c3fv.cpp): listMatchingFiles(the heads pattern, "", "") lists the head files into g_fileNames (0x64-byte names);
// the counter starts at 0x13 and the scan stops at 0x48; each name whose first byte is `up` or `low` is loaded from the heads
// directory + the name (built in g_textBuffer) and cut into three Surfaces (the renderer's), and the counter goes up by one
void load_heads(Ctx &ctx, uint32_t counter, uint8_t up, uint8_t low) {
  const Memory &m = ctx.mem;
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4cd6a4);
  const uint32_t empty = state_va(ctx, &GameState::g_str4e9a84);
  const int32_t n = static_cast<int32_t>(cm::call_peer(ctx, "listMatchingFiles", {pool + 8, empty, empty}).eax);
  m.store<int32_t>(counter, 0x13);
  if (n <= 0) return;
  uint32_t name = state_va(ctx, &GameState::g_fileNames);
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  for (int32_t i = 0; i < n; ++i, name += 0x64) {
    if (m.load<int32_t>(counter) >= 0x48) break;
    const uint8_t c = m.load<uint8_t>(name);
    if (c != up && c != low) continue;
    vstrcpy(m, text, pool);
    vstrcat(m, text, name);
    m.store<int32_t>(counter, add32(m.load<int32_t>(counter), 1));
  }
}

void load_interface_art_full(Ctx &ctx) {
  const Memory &m = ctx.mem;
  const uint32_t base = m.data_base();
  const int mode = SG_NEGCTL(g_negctl_load_interface_art);
  for (int k = 0; k < kIaFull_n; ++k) {
    const IaStep &st = kIaFull[k];
    switch (st.k) {
      case IaSet: m.store<uint32_t>(base + st.a, st.b); break;
      case IaFill:
        for (uint32_t i = 0; i < st.b; ++i) m.store<uint32_t>(base + st.a + 4 * i, st.c);
        break;
      case IaFlic: {
        if (mode == 2 && st.a == 0x175) break;   // negative control: the first interface flic not loaded
        const FlicCall f{st.a, st.b, st.frames, st.delay, st.flag};
        load_flic(ctx, f, 0);
        break;
      }
      case IaHeads: load_heads(ctx, base + st.a, static_cast<uint8_t>(st.b & 0xff), static_cast<uint8_t>(st.b >> 8)); break;
      case IaCut:
        if (mode != 3) cut_geometry(ctx, base + st.a, st.b, st.c, st.flag);
        break;
    }
  }
  // phase 5: an attached graphics backend loads and cuts the images of the full arm (the part it models, gfx.cpp)
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, "loadInterfaceArt.images", nullptr, 0);
  // roadmap S2.9: the cuts the dialogs draw (the popup border, its masks, the option bullets; gfx_dialogs.cpp)
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, "loadInterfaceArt.dialogArt", nullptr, 0);
  // roadmap S2.11: the message ball and its mask (gfx_simfoto.cpp), the bulletin board's art (gfx_bulletin.cpp)
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, "loadInterfaceArt.ballArt", nullptr, 0);
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, "loadInterfaceArt.bulletinArt", nullptr, 0);
}
}  // namespace

int32_t load_interface_art(Ctx &ctx, int32_t full) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  cm::call_peer(ctx, "genNoise", {});
  cm::call_peer(ctx, "clearBuffers", {});
  auto fill = [&](uint32_t va, uint32_t n, uint32_t v) {
    for (uint32_t i = 0; i < n; ++i) m.store<uint32_t>(va + 4 * i, v);
  };
  auto va = [&](auto member) { return state_va(ctx, member); };
  fill(va(&GameState::g_holes), 0xA28, 0);
  fill(va(&GameState::g_golfers), 0x2700, 0);
  fill(va(&GameState::g_walkers), 0x4C0, 0);
  fill(va(&GameState::g_skillHistory), 0xFA, 0);
  fill(va(&GameState::g_cashHistory), 0xFA, 0);
  fill(va(&GameState::g_funHistory), 0xFA, 0);
  fill(va(&GameState::g_memberTypeHistory), 0xFA, 0);
  fill(va(&GameState::g_eventHistory), 0xFA, 0);
  fill(va(&GameState::g_golferClassSlotCounts), 0x20, 0);
  fill(va(&GameState::g_financeYears), 0x1F4, 0);
  fill(va(&GameState::g_golferStats), 0x5C0, 0);
  const uint32_t ranking = va(&GameState::g_courseRanking);
  fill(ranking, 0x3C, 0);
  gs.g_holesPlusOne = 1;
  fill(va(&GameState::g_holePoints), 0x72, 0xFFFFFFFFu);
  fill(va(&GameState::g_tileFlags), 0x4E2, 0);
  fill(va(&GameState::g_ballStopCount), 0x271, 0);
  const uint32_t corners = va(&GameState::g_cornerLevel);
  fill(corners, 0x28A, 0);
  m.store<uint8_t>(corners + 0x28A * 4, 0);
  for (int32_t i = 0; i < 8; ++i) gs.g_themeSlots59bfc4[i] = i;
  for (PlacedObject &o : gs.g_placed_objects) o.type = -1;
  gs.g_challengeLevel = 0;
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  for (int32_t i = 0; i < 9; ++i) {
    uint32_t r;
    do {
      r = random_range(seed, 9) & 0xFFFFu;
    } while (m.load<int32_t>(ranking + 0x14 + 0x18 * r) != 0);
    for (int32_t k = 0; k < i + 1; ++k) {
      const uint32_t c = random_range(seed, SG_NEGCTL(g_negctl_load_interface_art) == 1 ? 3 : 4) & 0xFFFFu;
      const uint32_t cell = ranking + 4 * (6 * r + c), count = ranking + 0x14 + 0x18 * r;
      m.store<int32_t>(cell, add32(m.load<int32_t>(cell), 1));
      m.store<int32_t>(count, add32(m.load<int32_t>(count), 1));
    }
  }
  if (full != 0) {
    load_interface_art_full(ctx);
    return 0;
  }
  return load_world_assets(ctx);
}

// ------------------------------------------------------------------------------------------------ seedFromTime
// seedFromTime (c3cy.cpp; called once, by gameMain): t = timeGetTime(), stored in g_seedTime and, times 37, as the seed of
// g_rng; returns t. The clock value is the original's read at the call's return address (InputSource::clock, the site as an
// offset from mainLoop's frame-head return address): a run with no such read stops with NeedInput (phase 4, the boot path).
constexpr int32_t kSiteSeedFromTime = 0x574e7;
int32_t seed_from_time(Ctx &ctx) {
  InputSource *src = input_source(ctx);
  uint32_t t = 0;
  if (src == nullptr || !src->clock(kSiteSeedFromTime, t)) throw NeedInput{{"seedFromTime"}};
  if (SG_NEGCTL(g_negctl_seed_from_time) == 1) t += 1;
  GameState &gs = ctx.gs();
  gs.g_seedTime = static_cast<int32_t>(t);
  state_obj(ctx, &GameState::g_rng).field(&Random::state).ref() = t + (t + t * 8) * 4;
  return static_cast<int32_t>(t);
}

// ------------------------------------------------------------------------------------------------ startNewCourse
int32_t start_new_course(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_start_new_course);
  GameState &gs = ctx.gs();
  const int32_t saved = gs.g_difficulty;
  const int32_t r = static_cast<int32_t>(cm::call_peer(ctx, "loadGameBrowserScreen", {1, 1}).eax);
  if (r == 0) return r;
  gs.g_difficulty = saved;
  gs.g_cash_div100 = 1000;
  gs.g_date = mode == 1 ? 0x2c01 : 0x2c00;
  gs.g_flags = 1u << 26;
  const int32_t b = static_cast<int32_t>(cm::call_peer(ctx, "pickAProScreen", {}).eax);
  if (b == 0) {
    if (mode != 2) gs.g_flags = 0;
    return 0;
  }
  cm::call_peer(ctx, "loadGolfPro", {0});
  gs.g_cash_div100 = 1000;
  return 1;
}

// ------------------------------------------------------------------------------------------------ pickAProScreen
namespace {
constexpr int kRootClickX = 2, kRootClickY = 3, kRootButton = 4;   // g_rootInput slots (commands.cpp RootSlot)
uint32_t name_at(Ctx &ctx, int32_t i) { return state_va(ctx, &GameState::g_fileNames) + static_cast<uint32_t>(mul32(i, 100)); }
// flushInput (the modal stack push and focus clear of the window tree, the UI's): through the registry when a module
// registers it (a test records it), else nothing, as module saveload calls it
void enter_modal(Ctx &ctx) {
  try {
    cm::call_peer(ctx, "flushInput", {});
  } catch (const MissingResource &e) {
    if (e.what == nullptr || std::strcmp(e.what, "flushInput") != 0) throw;
  }
}
}  // namespace

int32_t pick_a_pro_screen(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_pick_a_pro);
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  const uint32_t theme = state_va(ctx, &GameState::g_theme);
  const uint32_t p8808 = state_va(ctx, &GameState::f_strpool_4c8808);
  const uint32_t p8794 = state_va(ctx, &GameState::f_strpool_4c8794);
  const uint32_t quote = state_va(ctx, &GameState::f_strpool_4c4e54);
  const uint32_t none = state_va(ctx, &GameState::g_str4e9a84);
  const std::string theme_copy = read_str(m, theme);
  vstrcpy(m, theme, state_va(ctx, &GameState::f_strpool_4c3c5c));   // the championship theme's name for the screen
  // roadmap S2.7: the drawing with an attached graphics backend (c3fw.cpp PickAProScreen_re; frame objects and cuts in
  // gfx_boot_screens.cpp, which keys them gfx_local_key(0x73000 + 0x2c k) and gfx_local_key(0x73400)): each redraw, the
  // hover changes, the tips, the loading message
  auto gfx_pick_a_pro_surface = [](uint32_t k) { return gfx_local_key(0x73000 + 0x2C * k); };
  auto gfx_pick_a_pro_image = []() { return gfx_local_key(0x73400); };
  const bool draw = g_gfx_attached.load(std::memory_order_relaxed) != 0;
  auto gx = [&](const char *name, std::initializer_list<uint32_t> a) {
    if (draw) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  };
  const uint32_t root = gs.f_stateptr_4c1570[0], win = state_va(ctx, &GameState::g_rootWindow);
  static const int32_t kBtnX[5] = {0x258, 0x2E4, 0x30A, 0x30A, 0x29E}, kBtnY[5] = {0x208, 0x212, 0x064, 0x17C, 0x208};
  auto img_at = [&](uint32_t k, int32_t x, int32_t y) {
    gx("Image_draw", {gfx_pick_a_pro_surface(k), root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0});
  };
  auto disabled = [&]() {   // the load and delete buttons' disabled images while nothing is selected
    img_at(10, 0x258, 0x208);
    img_at(14, 0x29E, 0x208);
  };
  constexpr uint32_t kInk = 0x80000000u, kWhite = 0x80007FFFu, kSkill = 0x80000210u;
  // the rows' pen x 0x140; negative control 13 of gfx.family.boot_screens draws them at 0x141
  uint32_t row_x = 0x140;
#if PORTSIM_NEGCTL
  if (draw) {
    static const int fam = registry_find("gfx.family.boot_screens");
    const FuncDef *fd = fam >= 0 ? registry_at(fam, nullptr) : nullptr;
    if (fd && fd->negctl && *fd->negctl == 13) row_x = 0x141;
  }
#endif
  gx("pickAProScreen.setup", {});
  int32_t sel = -1, scroll = 0, n = 0, max_scroll = 0, result = 0;
  gs.g_rootInput[kRootButton] = 0;
  for (;;) {
    // ---- redraw: the .pro files listed, the visible rows through the text buffer
    gx("setDrawTarget", {root, state_va(ctx, &GameState::g_fontRef_519a40), 0, 0, 0});
    gx("Surface_blitThunk", {gfx_pick_a_pro_image(), root, 0, 0, 0, 0, 0x320, 0x258});
    if (sel == -1) disabled();
    gx("drawCenteredText", {p8808 + 0x28, 0x1F8, 0x2A, kInk});
    vstrcpy(m, text, p8808 + 0xC);
    n = list_matching_files(ctx, text, none, none);
    gx("setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
    max_scroll = sub32(n, 0x10);
    if (scroll > max_scroll) scroll = max_scroll;
    if (scroll < 0) scroll = 0;
    int32_t row_y = 0x74;
    for (int32_t i = scroll; i < add32(scroll, 0x10) && i < n; ++i) {
      vstrcpy(m, text, name_at(ctx, i));
      if (sel == i) gx("fillWindowRect", {win, 0x136, static_cast<uint32_t>(row_y - 1), 0x1C8, 0xF, 0x80001284u});
      gx("drawTextWithColor", {text, row_x, static_cast<uint32_t>(row_y), sel == i ? kWhite : kInk});
      row_y = add32(row_y, 0x10);
    }
    if (sel != -1) {   // the selected pro's card: its texts
      if (draw) {   // the portrait: Surface g_sprites5628e8 + 0x2c + 0x84 (c + 72 t), c the signed type byte +0x22, t = 1
                    // when bit 7 of +0x21 is clear (Surface_drawScaled at (1, 1, 1) = Image_draw's path)
        const uint32_t gt = state_va(ctx, &GameState::g_golferTypes);
        const int32_t t = (m.load<uint8_t>(gt + 0x21) & 0x80) ? 0 : 1;
        const int32_t c = m.load<int8_t>(gt + 0x22);
        gx("Image_draw", {state_va(ctx, &GameState::g_sprites5628e8) + 0x2C + static_cast<uint32_t>(mul32(add32(c, mul32(t, 72)), 0x84)),
                          root, 0x45, 0x3D, 0});
      }
      vstrcpy(m, text, state_va(ctx, &GameState::g_golferTypes) + 0x10);
      vstrcat(m, text, p8808);
      gx("drawTextWithColor", {text, 0x38, 0xFF, kInk});
      vstrcpy(m, text, quote);
      m.store<int16_t>(state_va(ctx, &GameState::g_golfers) + 0xB6, 0);
      cm::call_peer(ctx, "buildGolferComment", {0x3E, 0, 0, 0});
      vstrcat(m, text, quote);
      gx("pickAProScreen.textSurface", {text, 0x24, 0x232, kWhite});
      gx("drawTextWithColor", {p8794 + 0x60, 0x38, 0x220, kInk});
      const uint32_t names = state_va(ctx, &GameState::f_ptrtab_4c2c3c);
      int32_t skill_y = 0x117;
      for (uint32_t i = 0; i < 10; ++i) {
        vstrcpy(m, text, m.load<uint32_t>(names + 4u * i));
        gx("drawTextWithColor", {text, 0x5E, static_cast<uint32_t>(skill_y), kSkill});
        vstrcpy(m, text, state_va(ctx, &GameState::f_strpool_4c5340));
        // the skill byte of row i (signed) * 10
        const int32_t v = mul32(m.load<int8_t>(state_va(ctx, &GameState::g_playerSkills) + i), 10);
        vstrcat(m, text, crt_itoa10(ctx, v, state_va(ctx, &GameState::g_panelNumberText)));
        vstrcat(m, text, state_va(ctx, &GameState::f_strpool_4c60c4));
        gx("drawCenteredText", {text, 0x3B, static_cast<uint32_t>(skill_y), kSkill});
        skill_y = add32(skill_y, 0x18);
      }
    }
    gx("Window::refreshFull", {win, 0});
    // ---- the input loop: pumpMessages, the hot button under the cursor, the tip after 0x1e passes on one button
    int32_t hot = -1, dwell = 0, action = 0;
    for (;;) {
      cm::pump_messages(ctx, PumpKind::Blocking, "pickAProScreen");
      InputSource *src = input_source(ctx);
      int32_t x = 0, y = 0;
      if (!src || !src->cursor(x, y)) throw NeedInput{{"pickAProScreen"}};
      const int32_t prev = hot;
      hot = -1;
      if (approx_distance(sub32(x, 0x279), sub32(y, 0x22B)) < 0x1A) hot = 0;
      if (approx_distance(sub32(x, 0x301), sub32(y, 0x22C)) < 0x18) hot = 1;
      if (approx_distance(sub32(x, 0x310), sub32(y, 0x78)) < 0x14) hot = 2;
      if (approx_distance(sub32(x, 0x310), sub32(y, 0x190)) < 0x14) hot = 3;
      if (approx_distance(sub32(x, 0x2C0), sub32(y, 0x22B)) < 0x17) hot = 4;
      if (hot != prev) {
        if (prev != -1) img_at(static_cast<uint32_t>(prev), kBtnX[prev], kBtnY[prev]);
        if (hot != -1) img_at(static_cast<uint32_t>(5 + hot), kBtnX[hot], kBtnY[hot]);
        if (sel == -1) disabled();
        gx("Window::refreshFull", {win, 0});
        dwell = 0;
      } else {
        dwell = add32(dwell, 1);
        if (dwell > 0x1E && hot != -1) {
          m.store<uint8_t>(text, 0);
          if (hot == 0) vstrcpy(m, text, sel == -1 ? none : p8794 + 0x50);
          else if (hot == 1) vstrcpy(m, text, state_va(ctx, &GameState::f_strpool_4c84e8) + 0xA8);
          else if (hot == 4) vstrcpy(m, text, p8794 + 0x58);
          gx("drawCenteredText2", {text, static_cast<uint32_t>(kBtnX[hot] + 0x1E), static_cast<uint32_t>(kBtnY[hot] + 0x10), kWhite});
          gx("Window::refreshFull", {win, 0});
        }
      }
      if (gs.g_rootInput[kRootButton] == 0) {
        if (cm::flush_redraw(ctx) == 0) continue;
        if (gs.g_rootInput[kRootButton] == 0 && cm::take_key(ctx) == 0x1B) {
          hot = 1;
          action = 1;
          break;
        }
      }
      action = hot == -1 ? -2 : hot;
      break;
    }

    if (action != -2) {
      gs.g_rootInput[kRootButton] = 0;
      if (static_cast<uint32_t>(action) > 4u) continue;
      if (action == 0) {   // load
        if (sel < 0 || sel >= n) {
          result = 0;
          break;
        }
        gx("setDrawTarget", {root, state_va(ctx, &GameState::g_font519928), 0, 0, 0});   // the loading message
        gx("drawTextWithColor", {state_va(ctx, &GameState::f_strpool_4c3f74) + 0x34, 0x184, 0x1D8, kWhite});
        gx("setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
        gx("Window::refreshFull", {win, 0});
        if (mode != 2) cm::call_peer(ctx, "loadThemeFile", {name_at(ctx, sel), 0, 1});
        result = 1;
        break;
      }
      if (action == 1) {   // back: the theme name stays as the screen set it
        enter_modal(ctx);
        gx("pickAProScreen.teardown", {});
        return 0;
      }
      if (action == 2) {
        scroll = clamp(sub32(scroll, 4), 0, max_scroll);
        continue;
      }
      if (action == 3) {
        scroll = clamp(add32(scroll, 4), 0, max_scroll);
        continue;
      }
      // 4: delete the selection (always from the championship directory) after a confirmation
      vstrcpy(m, text, p8794 + 0x2C);
      const uint32_t victim = name_at(ctx, sel);
      vstrcat(m, text, victim);
      vstrcat(m, text, p8794);
      if (cm::call_peer(ctx, "showPopup", {0x190, 0x64, 1, 1, 0}).eax != 0) continue;
      vstrcpy(m, text, state_va(ctx, &GameState::f_strpool_4c52c0) + 0x68);
      vstrcat(m, text, victim);
      ctx.files().remove(read_str(m, text));
      continue;
    }
    // a press over the list at the last press position
    if (gs.g_rootInput[kRootClickX] <= 0x140) continue;
    const int32_t my = gs.g_rootInput[kRootClickY];
    if (my < 0x74) continue;
    sel = add32(sub32(my, 0x74) / 16, scroll);
    if (sel < n) {
      cm::call_peer(ctx, "loadThemeFile", {name_at(ctx, mode == 1 ? add32(sel, 1) : sel), 0, 1});
      continue;
    }
    sel = -1;
    disabled();   // a press on no row
  }
  enter_modal(ctx);
  if (mode != 3) write_str(m, theme, theme_copy);
  gx("pickAProScreen.teardown", {});
  return result;
}

// ------------------------------------------------------------------------------------------------ listThemes
// listThemes (c3ep.cpp): listMatchingFiles over the themes pattern f_strpool_4e40a0 with both filters empty, then the
// directory table g_fileNames from entry ONE to its end (entry 0 is FindFirstFileA's "."): a name that contains the string
// f_strpool_4c4944, an empty one and the championship theme f_strpool_4c3c5c are skipped, the others copied into
// g_themeName (0x32 each); g_theme becomes "Standard". Returns the count.
int32_t list_themes(Ctx &ctx) {
  const Memory &m = ctx.mem;
  const uint32_t none = state_va(ctx, &GameState::g_str4e9a84);
  list_matching_files(ctx, state_va(ctx, &GameState::f_strpool_4e40a0), none, none);
  const std::string dot = read_str(m, state_va(ctx, &GameState::f_strpool_4c4944));
  const std::string champ = read_str(m, state_va(ctx, &GameState::f_strpool_4c3c5c));
  const uint32_t table = state_va(ctx, &GameState::g_fileNames);
  uint32_t out = state_va(ctx, &GameState::g_themeName);
  int32_t count = 0;
  for (uint32_t ent = table + 0x64; ent < table + sizeof(GameState::g_fileNames); ent += 0x64) {
    const std::string name = read_str(m, ent);
    if (name.find(dot) != std::string::npos || name.empty() || name == champ) continue;
    ++count;
    vstrcpy(m, out, ent);
    out += 0x32;
  }
  vstrcpy(m, state_va(ctx, &GameState::g_theme), state_va(ctx, &GameState::f_strpool_4c6ba4) + 0x6c);
  return count;
}

// ------------------------------------------------------------------------------------------------ showThemePackSelect
// showThemePackSelect (c3fw.cpp; the title menu's theme-pack screen): listThemes, then a modal loop over the theme rows. The
// six FindFirstFileA probes per theme (which pack files exist: the flag column icons), the Surface arrays, the sheet cuts and
// the drawing are the renderer's. Each redraw copies every theme name to g_textBuffer to compare it with g_theme. Each pass:
// waitTicks(2, 0) (dropped: its clock reads are not this loop's), pumpMessages, the cursor, the hot button by
// approxDistance (0 Load, 1 Cancel, 2 / 3 the arrows), the tip text after 0x1e passes on one button; with no button down a
// flushRedraw that reports a key acts as Cancel. A press: Load keeps g_theme, Cancel sets it back to "Standard", an arrow
// redraws, a press on no button picks row (click y - 0x5b) / 32 into g_theme (outside the list: leave as is). The original
// returns ??_M's eax, which the title menu does not read.
int32_t show_theme_pack_select(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_theme_pack);
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  const uint32_t theme = state_va(ctx, &GameState::g_theme);
  const uint32_t names = state_va(ctx, &GameState::g_themeName);
  const int32_t count = list_themes(ctx);
  // roadmap S2.7: the drawing with an attached graphics backend (gfx_boot_screens.cpp): the probes and frame objects, each
  // redraw, the hover changes, the tips
  const bool draw = g_gfx_attached.load(std::memory_order_relaxed) != 0;
  auto gx = [&](const char *name, std::initializer_list<uint32_t> a) {
    if (draw) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  };
  gx("showThemePackSelect.setup", {static_cast<uint32_t>(count)});
  enter_modal(ctx);
  gs.g_rootInput[kRootButton] = 0;
  int exit_kind = 3;
  for (;;) {
    // ---- redraw: the rows; the current theme's row is the selection
    int32_t selected = -1;
    for (int32_t i = 0; i < count; ++i) {
      const uint32_t nm = names + 0x32u * static_cast<uint32_t>(i);
      vstrcpy(m, text, nm);
      if (read_str(m, text) == read_str(m, theme)) selected = static_cast<int32_t>(nm);
    }
    gx("showThemePackSelect.redraw", {static_cast<uint32_t>(count)});
    // ---- the input loop
    int32_t hot = -1, dwell = 0;
    bool redraw = false;
    for (;;) {
      cm::pump_messages(ctx, PumpKind::Blocking, "showThemePackSelect");
      InputSource *src = input_source(ctx);
      int32_t x = 0, y = 0;
      if (!src || !src->cursor(x, y)) throw NeedInput{{"showThemePackSelect"}};
      const int32_t prev = hot;
      hot = -1;
      if (approx_distance(sub32(x, 0x267), sub32(y, 0x22B)) < 0x1A) hot = 0;
      if (approx_distance(sub32(x, 0x2ED), sub32(y, 0x22C)) < 0x18) hot = 1;
      if (approx_distance(sub32(x, 0x30A), sub32(y, 0x63)) < 0x14) hot = 2;
      if (approx_distance(sub32(x, 0x30A), sub32(y, 0x1E7)) < 0x14) hot = 3;
      if (hot != prev) {
        gx("showThemePackSelect.hover", {static_cast<uint32_t>(prev), static_cast<uint32_t>(hot)});
        dwell = 0;
      } else {
        dwell = add32(dwell, 1);
        if (dwell > 0x1E && hot != -1) {
          m.store<uint8_t>(text, 0);
          if (hot == 0)
            vstrcpy(m, text, selected == -1 ? state_va(ctx, &GameState::g_str4e9a84) : state_va(ctx, &GameState::f_strpool_4c8794) + 0x50);
          else if (hot == 1)
            vstrcpy(m, text, state_va(ctx, &GameState::f_strpool_4c84e8) + 0xA8);
          gx("showThemePackSelect.tip", {text, static_cast<uint32_t>(hot), static_cast<uint32_t>(dwell)});
        }
      }
      int32_t btn = gs.g_rootInput[kRootButton];
      if (btn == 0) {
        if (cm::flush_redraw(ctx) == 0) continue;
        btn = gs.g_rootInput[kRootButton];
        if (btn == 0) hot = 1;   // a key: Cancel
      }
      if (hot == -1) {
        const int32_t idx = div32(sub32(gs.g_rootInput[kRootClickY], 0x5B), 32);
        if (idx < 0 || idx >= count) break;                     // exit kind 3
        if (mode != 1) vstrcpy(m, theme, names + 0x32u * static_cast<uint32_t>(idx));
        redraw = true;
        break;
      }
      gs.g_rootInput[kRootButton] = 0;
      if (hot == 0) {
        exit_kind = 2;
        break;
      }
      if (hot == 1) {
        exit_kind = 1;
        break;
      }
      redraw = true;
      break;
    }
    if (!redraw) break;
  }
  if (exit_kind == 1 && mode != 2) vstrcpy(m, theme, state_va(ctx, &GameState::f_strpool_4c6ba4) + 0x6c);
  gx("showThemePackSelect.teardown", {});
  return 0;
}

// ------------------------------------------------------------------------------------------------ registry
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_load_world_assets(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(load_world_assets(c)); }
void a_load_interface_art(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(load_interface_art(c, S(a, 0))); }
void a_alloc_block(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(alloc_block(c, S(a, 0))); }
void a_free_block(Ctx &c, const uint32_t *a, Ret &) { free_block(c, S(a, 0), S(a, 1)); }
void a_free_table(Ctx &c, const uint32_t *a, Ret &) { free_table(c, S(a, 0)); }
void a_start_new_course(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(start_new_course(c)); }
void a_pick_a_pro(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(pick_a_pro_screen(c)); }
void a_list_themes(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(list_themes(c)); }
void a_theme_pack(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(show_theme_pack_select(c)); }

const FuncDef kFuncs[] = {
    {"loadWorldAssets", "default", "void", 0, 0, a_load_world_assets, &g_negctl_load_world_assets},
    {"loadInterfaceArt", "default", "void", 1, 0, a_load_interface_art, &g_negctl_load_interface_art},
    {"seedFromTime", "default", "int", 0, 0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(seed_from_time(c)); }, &g_negctl_seed_from_time},
    {"allocBlock", "default", "int", 1, 0, a_alloc_block, &g_negctl_alloc_block},
    {"freeBlock", "default", "void", 2, 0, a_free_block, &g_negctl_free_block},
    {"freeTable", "default", "void", 1, 0, a_free_table, nullptr},
    {"startNewCourse", "default", "int", 0, 0, a_start_new_course, &g_negctl_start_new_course},
    {"pickAProScreen", "default", "int", 0, 0, a_pick_a_pro, &g_negctl_pick_a_pro},
    {"listThemes", "default", "int", 0, 0, a_list_themes, nullptr},
    {"showThemePackSelect", "default", "int", 0, 0, a_theme_pack, &g_negctl_theme_pack},
    // the switch of the title menu / session bodies of the tick module (tick_menu.inc reads it through the registry);
    // calling it does nothing
    {"tick.menu", "default", "void", 0, 0, [](Ctx &, const uint32_t *, Ret &) {}, &g_negctl_menu},
};
}  // namespace

extern const ModuleDef kModule_menu = {"menu", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
