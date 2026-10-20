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
#include "portsim/sim/loaders.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   loadWorldAssets  1 = the object-type table is not copied from its initial image, 2 = type 3's last name write (the
//                    build-item name at +0xb4) skipped, 3 = a free-list allocation leaves the cell cursor at the sheet's end
//   loadInterfaceArt 1 = the ranking cells drawn by Random::range(3)
//   allocBlock       1 = a region of exactly n is carved instead of consumed (its length 0 left in place)
//   freeBlock        1 = the region before base grows by n + 1
//   startNewCourse   1 = the date is 0x2c01, 2 = a refused pro list leaves the flags set
//   pickAProScreen   1 = a row press loads the row below, 2 = the load button does not load the selection again,
//                    3 = the theme name is not restored on the load exit
//   tick.menu        (the menu / session bodies, tick_menu.inc) 1 = buildGolferBioText dropped from the menu's session reset,
//                    2 = g_date not reset by the menu, 3 = Session's course type taken from the next course record,
//                    4 = Session's building unlock level 7 instead of 6 on the new-course path
int g_negctl_load_world_assets = 0;
int g_negctl_load_interface_art = 0;
int g_negctl_alloc_block = 0;
int g_negctl_free_block = 0;
int g_negctl_start_new_course = 0;
int g_negctl_pick_a_pro = 0;
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
// pumpInput483bd0 (+0x54: a pump of a modal site, the port's takes nothing, U-1052). A delay of -1 loads one frame
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
  if (c % 100 == 0 && gs.g_difficulty == -1) cm::call_peer(ctx, "showLoadingScreen", {static_cast<uint32_t>(c / 4)});
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
  const int32_t cols = flic_cols(ctx, read_str(ctx.mem, text));
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
  }
  if (saved != -1 && mode != 3) gs.g_cellCursor = saved;
}

void run(Ctx &ctx, const FlicCall *t, int n, int mode) {
  for (int k = 0; k < n; ++k) load_flic(ctx, t[k], mode);
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
// i + 1 marks in its cells Random::range(4); then with full == 0 loadWorldAssets. The boot path (full != 0: the sheet tables
// reset and every interface flic and image) runs only in mainLoop's Setup, which the port does not run: MissingResource.
int32_t load_interface_art(Ctx &ctx, int32_t full) {
  if (full != 0) throw MissingResource{"loadInterfaceArt(full != 0): the boot art (mainLoop's Setup)"};
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
  return load_world_assets(ctx);
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
// enterModal483d30 (the modal stack push and focus clear of the window tree, the UI's): through the registry when a module
// registers it (a test records it), else nothing, as module saveload calls it
void enter_modal(Ctx &ctx) {
  try {
    cm::call_peer(ctx, "enterModal483d30", {});
  } catch (const MissingResource &e) {
    if (e.what == nullptr || std::strcmp(e.what, "enterModal483d30") != 0) throw;
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
  int32_t sel = -1, scroll = 0, n = 0, max_scroll = 0, result = 0;
  gs.g_rootInput[kRootButton] = 0;
  for (;;) {
    // ---- redraw (the drawing dropped): the .pro files listed, the visible rows through the text buffer
    vstrcpy(m, text, p8808 + 0xC);
    n = list_matching_files(ctx, text, none, none);
    max_scroll = sub32(n, 0x10);
    if (scroll > max_scroll) scroll = max_scroll;
    if (scroll < 0) scroll = 0;
    for (int32_t i = scroll; i < add32(scroll, 0x10) && i < n; ++i) vstrcpy(m, text, name_at(ctx, i));
    if (sel != -1) {   // the selected pro's card: its texts
      vstrcpy(m, text, state_va(ctx, &GameState::g_golferTypes) + 0x10);
      vstrcat(m, text, p8808);
      vstrcpy(m, text, quote);
      m.store<int16_t>(state_va(ctx, &GameState::g_golfers) + 0xB6, 0);
      cm::call_peer(ctx, "buildGolferComment", {0x3E, 0, 0, 0});
      vstrcat(m, text, quote);
      const uint32_t names = state_va(ctx, &GameState::f_ptrtab_4c2c3c);
      for (uint32_t i = 0; i < 10; ++i) {
        vstrcpy(m, text, m.load<uint32_t>(names + 4u * i));
        vstrcpy(m, text, state_va(ctx, &GameState::f_strpool_4c5340));
        // the skill byte of row i (signed) * 10
        const int32_t v = mul32(m.load<int8_t>(state_va(ctx, &GameState::g_playerSkills) + i), 10);
        vstrcat(m, text, crt_itoa10(ctx, v, state_va(ctx, &GameState::g_panelNumberText)));
        vstrcat(m, text, state_va(ctx, &GameState::f_strpool_4c60c4));
      }
    }
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
        dwell = 0;
      } else {
        dwell = add32(dwell, 1);
        if (dwell > 0x1E && hot != -1) {
          m.store<uint8_t>(text, 0);
          if (hot == 0) vstrcpy(m, text, sel == -1 ? none : p8794 + 0x50);
          else if (hot == 1) vstrcpy(m, text, state_va(ctx, &GameState::f_strpool_4c84e8) + 0xA8);
          else if (hot == 4) vstrcpy(m, text, p8794 + 0x58);
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
        if (mode != 2) cm::call_peer(ctx, "loadThemeFile", {name_at(ctx, sel), 0, 1});
        result = 1;
        break;
      }
      if (action == 1) {   // back: the theme name stays as the screen set it
        enter_modal(ctx);
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
  }
  enter_modal(ctx);
  if (mode != 3) write_str(m, theme, theme_copy);
  return result;
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

const FuncDef kFuncs[] = {
    {"loadWorldAssets", "default", "void", 0, 0, a_load_world_assets, &g_negctl_load_world_assets},
    {"loadInterfaceArt", "default", "void", 1, 0, a_load_interface_art, &g_negctl_load_interface_art},
    {"allocBlock", "default", "int", 1, 0, a_alloc_block, &g_negctl_alloc_block},
    {"freeBlock", "default", "void", 2, 0, a_free_block, &g_negctl_free_block},
    {"freeTable", "default", "void", 1, 0, a_free_table, nullptr},
    {"startNewCourse", "default", "int", 0, 0, a_start_new_course, &g_negctl_start_new_course},
    {"pickAProScreen", "default", "int", 0, 0, a_pick_a_pro, &g_negctl_pick_a_pro},
    // the switch of the title menu / session bodies of the tick module (tick_menu.inc reads it through the registry);
    // calling it does nothing
    {"tick.menu", "default", "void", 0, 0, [](Ctx &, const uint32_t *, Ret &) {}, &g_negctl_menu},
};
}  // namespace

extern const ModuleDef kModule_menu = {"menu", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
