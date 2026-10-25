// Module `routing` (phase 3.8, stream 38g): the command effect of showRoutingMapScreen (the F5 routing map: the 50 x 50 tile
// map in four colour modes, the 18 holes with their tee-to-green lines, the walkers on the course, and the controls that
// reorder the holes), split from its drawing the way 3.7b split the other screens (log/p3/p37b_notes.txt). Written from the
// behaviour of the hook transcription (shim/src/re/c3et.cpp) as reading reference; no original code or address. What is kept:
//   * every simulation write: the hole records, the waypoint records and the tile-flag hole numbers a right click reorders,
//     the mode word a button click sets, the walker kind word a listed row rewrites, the golfers' hole bytes and the hole
//     count the exit rewrites,
//   * every call into another stage that writes state (spreadAllShade in the shade mode, appendCourseTitle, appendWalkerName,
//     appendDate, appendNumber for the listed walkers) in the original's order with its arguments,
//   * every division the original performs (a zero divisor stays a DivideError),
//   * the modal loop's input handling: the cursor read, the button under it, the pump, the click-or-key tests in the original's
//     order (the pump model of module commands).
// What is dropped: the tile map, hole and walker drawing (backdrop images, polygons, lines, sprites, text), the display slot
// calls, window refresh and the waits. Their scratch words (the line point, the last cell pointer) are not written.
// Roadmap S2.4: with a graphics backend attached (gfx.attach) the screen makes the original's drawing calls through
// gfx_drop (struct MapDraw below, in the order of c3et.cpp ShowRoutingMap_re). A drawn text the original builds in the
// text buffer is built there, drawn and the buffer's bytes put back, so the GameState after a pass with a backend equals
// the one without (the segment and boot gates run without one and with one).
#include <cstring>
#include <string>
#include <vector>

#include "portsim/arith.h"
#include "portsim/execonst.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/util.h"
#include "commands_internal.h"
#include "gfx_reports.h"

namespace portsim {

namespace {
// Negative control: 0 is the correct body.
//   1 the right-click reorder leaves the moved hole's tile numbers alone, 2 the exit remaps through the permutation instead
//   of its inverse, 3 the hole count after the exit is one more than the first hole with no par, 4 a button click sets the
//   mode to the button instead of the button - 1, 5 the scroll arrows are swapped, 6 the right half of the hole list starts at
//   x >= 0x180 instead of 0x190, 7 the waypoint record does not move with its hole, 8 a listed walker whose kind is 0xc keeps it,
//   9 a walker's date is formatted from the word shifted by 9
int g_negctl_routing_map = 0;
// showShortcutsScreen: 1 a right click ends it too, 2 the close button's box is 0x10 columns narrower, 3 the exit leaves the
// message-box mode byte at 1, 4 a key does not end it (only the close button does)
int g_negctl_shortcuts = 0;
// showMembershipRoster: 1 the exclusion bit of the type records is left alone, 2 the close button's box is 0x10 columns narrower,
// 3 a right click on an arrow redraws instead of leaving, 4 a left press on an arrow ends the screen, 5 the exit
// leaves the message-box mode byte at 1
int g_negctl_roster = 0;

// model_defects: the exit remap indexes a 20-entry table with each golfer's hole byte, and a byte of 20 or more reads the
// stack of the caller. The value is the caller's frame, which the port does not model: ON (the original) the port reports a
// missing resource there, OFF it leaves the byte alone (U-1020).
int g_defect_exit_remap_frame = 1;
const DefectDef kDefects[] = {
    {"showRoutingMapScreen.exitRemapReadsFrame", "U-1020",
     "a golfer's hole byte of 20 or more indexes past the exit's 20-entry table into the caller's frame", &g_defect_exit_remap_frame},
};

using cm::call_peer;

constexpr uint32_t kHoleSize = 0x208, kWaySize = 0x18, kWalkerSize = 0x4c, kGolferSize = 0x100;
constexpr int kButton = 4, kClickX = 2, kClickY = 3;   // g_rootInput slots (commands.cpp RootSlot)

void reset_text(Ctx &ctx) { ctx.mem.store<uint8_t>(state_va(ctx, &GameState::g_textBuffer), 0); }

// the button under (x, y): 1 left arrow-pair box, 0 / 2 / 3 the mode buttons, -2 quit, 9 / 10 the walker scroll arrows
// (mode 0 with more than eight walkers), -1 none
int32_t route_hit(int32_t mode, int32_t x, int32_t y, int32_t count) {
  if (x >= 0x3e && x < 0x8e && y >= 0x13b && y < 0x18b) return 1;
  if (x >= 0xb7 && x < 0x107 && y >= 0xfe && y < 0x14e) return 0;
  if (x >= 0x218 && x < 0x268 && y >= 0xfe && y < 0x14e) return 2;
  if (x >= 0x293 && x < 0x2e3 && y >= 0x13b && y < 0x18b) return 3;
  if (x >= 0x296 && x < 0x2e6 && y >= 0x214 && y < 0x255) return -2;
  if (mode == 0 && count > 8 && x >= 0x307) {
    if (x < 0x319 && y >= 0x40 && y < 0x65) return 9;
    if (x < 0x319 && y >= 0xcd && y < 0xf2) return 10;
  }
  return -1;
}

// ------------------------------------------------------------------------------------------------ the drawing (S2.4)
// The original's drawing calls of one redraw pass (c3et.cpp ShowRoutingMap_re, instruction addresses there), made only with
// a graphics backend attached. Images: g_spriteRef_8224c8 (loadInfoScreenArt, gfx_routing.cpp) cell offsets: +0 the bottom
// art, +0x2c / +0x58 / +0x84 / +0xb0 the course / employee / aura / value screens, +0xdc.. the twelve route buttons.
// Negative control (g_negctl_routing_map): 10 the selected hole's highlight is drawn one row lower.
constexpr uint32_t kBlack = 0x80000000u;
struct MapDraw {
  Ctx &ctx;
  GameState &gs;
  const Memory &m;
  bool on;
  uint32_t root, win, refs, text;
  explicit MapDraw(Ctx &c)
      : ctx(c),
        gs(c.gs()),
        m(c.mem),
        on(g_gfx_attached.load(std::memory_order_relaxed) != 0),
        root(c.gs().f_stateptr_4c1570[0]),
        win(state_va(c, &GameState::g_rootWindow)),
        refs(state_va(c, &GameState::g_spriteRef_8224c8)),
        text(state_va(c, &GameState::g_textBuffer)) {}
  void gx(const char *name, std::initializer_list<uint32_t> a) const {
    gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  }
  void scale(uint32_t den) const { gx("Display::setScale", {1, 1, den}); }   // display slot +0x9c (1, 1, den)
  void image(uint32_t off, int32_t x, int32_t y) const { gx("Image_draw", {refs + off, root, u32(x), u32(y), 0}); }
  void font(uint32_t f) const { gx("setDrawTarget", {root, f, 0, 0, 0}); }
  void centred(uint32_t s, int32_t x, int32_t y, uint32_t c = kBlack) const { gx("drawCenteredText", {s, u32(x), u32(y), c}); }
  void left(uint32_t s, int32_t x, int32_t y, uint32_t c = kBlack) const { gx("drawText4049d0", {s, u32(x), u32(y), c}); }
  // a text the original builds in the text buffer: built there, drawn by `draw`, and the buffer's bytes put back
  template <class F>
  void with_text(const std::string &s, F draw) const {
    std::vector<uint8_t> keep(0x1000);
    std::memcpy(keep.data(), m.at(text, 0x1000), 0x1000);
    std::memcpy(m.at(text, static_cast<uint32_t>(s.size() + 1), true), s.c_str(), s.size() + 1);
    draw(text);
    std::memcpy(m.at(text, 0x1000, true), keep.data(), 0x1000);
  }
  std::string str(uint32_t va) const {
    std::string s;
    for (uint32_t a = va; m.load<uint8_t>(a) != 0; ++a) s += static_cast<char>(m.load<uint8_t>(a));
    return s;
  }
  // mapToScreen456b70 (c3t.cpp): x = 6 (a + b) + 0x6a, y = 3 (b - a) + 0x1b8 - d, d = 1 when g_map822b80[0] != 0, else -1
  void map_to_screen(int32_t a, int32_t b, int32_t &x, int32_t &y) const {
    x = add32(mul32(add32(a, b), 6), 0x6a);
    y = add32(sub32(mul32(sub32(b, a), 3), gs.g_map822b80[0] != 0 ? 1 : -1), 0x1b8);
  }

  // the backdrop: only while g_map822b80[0] is 0; modes -1..2 pick their screen, others the course one
  void backdrop(int32_t mode) const {
    if (gs.g_map822b80[0] != 0) return;
    image(0, 0, 0xfd);
    image(mode >= -1 && mode < 3 ? 0x58u + 0x2Cu * static_cast<uint32_t>(mode) : 0x2Cu, 0, 0);
  }

  // the two tile passes: pass 1 a diamond in 0x80001284 under every tile whose type byte of
  // unk_0058dd34 is 0xff; pass 2 one diamond per tile in the mode's colour (no tile = type 0x14, skipped by both)
  void tiles(int32_t mode) const {
    const uint32_t grid = state_va(ctx, &GameState::g_tileType), tbyte = state_va(ctx, &GameState::unk_0058dd34);
    const uint32_t attrs = state_va(ctx, &GameState::g_tileTypeAttrs);
    for (int32_t x = 0; x < 50; ++x)
      for (int32_t y = 0; y < 50; ++y) {
        const int32_t t = m.load<int8_t>(grid + u32(x * 50 + y));
        if (t == 0x14 || m.load<uint8_t>(tbyte + u32(t)) != 0xff) continue;
        int32_t sx, sy;
        map_to_screen(x, y, sx, sy);
        gx("drawRoutingPoly.diamond", {u32(sx), u32(sy), 0x80001284u});
      }
    uint32_t colour = 0;
    for (int32_t x = 0; x < 50; ++x)
      for (int32_t y = 0; y < 50; ++y) {
        const int32_t t = m.load<int8_t>(grid + u32(x * 50 + y));
        if (t == 0x14) continue;
        int32_t sx, sy;
        map_to_screen(x, y, sx, sy);
        if (mode == 2) {   // the home value: rateLot and evalPlacementArea
          const int32_t lot = i32(cm::call_peer(ctx, "rateLot", {u32(x), u32(y)}).eax);
          const int32_t area = i32(cm::call_peer(ctx, "evalPlacementArea", {u32(x), u32(y), 2, 5}).eax);
          const int32_t v = clamp(mul32(sub32(lot / 4, area), 3) / 2, 0, 0xff);
          colour = 0x80000000u | ((static_cast<uint32_t>(v) & 0xff) >> 3) << 5;
          if (area == -1) colour = 0x80002000u;
        }
        if (mode == 1) {   // the aura: shade A >> 3 red, shade B >> 3 green, (A + B) & 0xff >> 4 blue
          const uint32_t a = m.load<uint8_t>(state_va(ctx, &GameState::g_tileShadeA) + u32(x * 50 + y));
          const uint32_t b = m.load<uint8_t>(state_va(ctx, &GameState::g_tileShadeB) + u32(x * 50 + y));
          colour = 0x80000000u | (a >> 3) << 10 | (b >> 3) << 5 | ((a + b) & 0xff) >> 4;
          if (t == 0x11) colour = 0x80000218u;
        }
        if (mode != 0 && cm::call_peer(ctx, "tileBlocked", {u32(x), u32(y)}).eax != 0) colour = 0x80002108u;
        if (mode <= 0) {   // the type record's byte +0x26 through the jump table
          switch (m.load<int8_t>(attrs + 0x26 + 0x30u * u32(t))) {
            case 0: case 1: colour = 0x80003394u; break;
            case 2: colour = 0x80001310u; break;
            case 4: colour = 0x80000204u; break;
            case 7: colour = 0x80006310u; break;
            case 10: colour = 0x80005304u; break;
            case 13: colour = 0x80001184u; break;
            case 17: colour = 0x80000218u; break;
            case 18: colour = 0x800010c8u; break;
            default: colour = 0x8000318cu; break;
          }
          if (cm::call_peer(ctx, "tileBlocked", {u32(x), u32(y)}).eax != 0) colour = 0x80000000u;
        }
        gx("drawRoutingPoly.diamond", {u32(sx), u32(sy), colour});
      }
  }

  // the title and the mode's legend; the text buffer holds the course title (appendCourseTitle(1))
  void legend(int32_t mode) const {
    const uint32_t p35c8 = state_va(ctx, &GameState::f_strpool_4d35c8), p3414 = state_va(ctx, &GameState::f_strpool_4d3414);
    font(state_va(ctx, &GameState::g_fontRef_821f28));
    const int32_t ty = mode != 0 ? 0x3a : 0x30;
    if (mode == 0) with_text(str(text) + str(p35c8 + 0x88), [&](uint32_t s) { centred(s, 0x190, ty); });
    else centred(text, 0x190, ty);
    font(state_va(ctx, &GameState::g_fontRef_821020));
    centred(p35c8 + 0x7c, 0x190, 0xf);
    font(state_va(ctx, &GameState::g_fontRef_821ee8));
    if (mode == -1) {
      const uint32_t hole = state_va(ctx, &GameState::f_strpool_4c581c) + 0x100, par = state_va(ctx, &GameState::f_strpool_4d35c4),
                     yds = state_va(ctx, &GameState::f_strpool_4d35c0), tim = state_va(ctx, &GameState::f_strpool_4d2120) + 4;
      centred(p35c8 + 0x38, 0x190, 0x55);
      centred(p35c8 + 0x1c, 0x190, 0x6c);
      centred(p35c8 + 0x00, 0x190, 0x7e);
      centred(hole, 0x47, 0x55);
      centred(hole, 0x234, 0x55);
      centred(par, 0x84, 0x55);
      centred(par, 0x276, 0x55);
      centred(yds, 0xb6, 0x55);
      centred(yds, 0x2a8, 0x55);
      centred(tim, 0xef, 0x55);
      centred(tim, 0x2e1, 0x55);
    } else if (mode == 1) {
      centred(p35c8 + 0x58, 0x190, 0xbc);
      centred(p35c8 + 0x50, 0x190, 0x55);
      left(state_va(ctx, &GameState::f_strpool_4c6980) + 0x58, 0x270, 0xce);
      left(p35c8 + 0x48, 0x88, 0xce);
    } else if (mode == 2) {
      centred(p35c8 + 0x6c, 0x190, 0xbc);
      left(p35c8 + 0x64, 0x277, 0xce);
      left(state_va(ctx, &GameState::f_strpool_4d32e8) + 4, 0x8e, 0xce);
    }
    if (mode == 1) {   // Widget_setQuad(root, black, -1, 2, 2), the explanation through drawCenteredTextLines
      gx("Widget_setQuad", {root, kBlack, 0xffffffffu, 2, 2});
      with_text(str(p3414 + 0x12c), [&](uint32_t s) { gx("drawCenteredTextLines", {root, s, 0xfa, 0x6e, 0x12c}); });
    }
    if (mode == 2) {
      left(p3414 + 0x108, 0x33, 0x56);
      left(p3414 + 0xec, 0x33, 0x68);
      left(p3414 + 0xd0, 0x33, 0x79);
      left(p3414 + 0xac, 0x33, 0x8a);
      // "Building a " + the building name at f_tab_buildItems_4c26b0 + 0xf0
      with_text(str(p3414 + 0xa0) + str(state_va(ctx, &GameState::f_tab_buildItems_4c26b0) + 0xf0),
                [&](uint32_t s) { left(s, 0x33, 0x9c); });
      left(p3414 + 0x7c, 0x1ee, 0x56);
      left(p3414 + 0x64, 0x1ee, 0x68);
      left(p3414 + 0x48, 0x1ee, 0x79);
      left(p3414 + 0x24, 0x1ee, 0x8a);
      left(p3414 + 0x04, 0x1ee, 0x9c);
    }
  }

  // the hole list row of hole h in mode -1
  void hole_row(int32_t mode, int32_t h, uint32_t rec, int32_t selected, int mutant) const {
    if (mode != -1) return;
    const int32_t y = add32(mul32((h - 1) % 9, 17), 0x68), col = h <= 9 ? 0x47 : 0x239;
    if (h == selected) {
      const int32_t hy = mutant == 10 ? add32(y, 17) : y;
      gx("fillWindowRect", {win, u32(col - 0x27), u32(hy - 3), 0xee, 0x11, 0x800023e8u});
      gx("fillWindowRect", {win, u32(col - 0x26), u32(hy - 2), 0xec, 0xf, 0x80007fffu});
    }
    const int8_t par = m.load<int8_t>(rec);
    with_text(std::to_string(h), [&](uint32_t s) { centred(s, col, y, par != 0 ? kBlack : 0x80006318u); });
    if (par == 0) return;
    with_text(std::to_string(par), [&](uint32_t s) { centred(s, col + 0x3d, y); });
    with_text(std::to_string(m.load<int16_t>(rec + 4)), [&](uint32_t s) { centred(s, col + 0x6f, y); });
    const int32_t n = m.load<int32_t>(rec + 0x20);
    if (n != 0)
      with_text(std::to_string(div32(div32(m.load<int32_t>(rec + 0x1ec), n), 40)) + str(state_va(ctx, &GameState::f_strpool_4d3140)),
                [&](uint32_t s) { centred(s, col + 0xa8, y); });
  }

  // the hole's tee, waypoint line, number and green on the map. Drawn: the hole number
  // (drawCenteredText2 in 0x80007ff0, outside the employee mode) at the waypoint, or halfway between tee and green. Not
  // drawn, reported through drops without a body: the tee and flag sprites (addSprite of a sprite-sheet cell: the FLIC
  // sheets and the sprite queue's scaled draw are roadmap S3.4 / S3.5) and the lines (lineTo = Terrain::drawLine, an
  // OpenGL line into the same back buffer: S3.8).
  void hole_map(int32_t mode, int32_t h, uint32_t rec, uint32_t way) const {
    int32_t tx, ty;
    map_to_screen(m.load<int32_t>(rec + 8), m.load<int32_t>(rec + 0xc), tx, ty);
    gx("showRoutingMapScreen.teeSprite", {u32(tx), u32(ty)});
    int32_t ax = 0, ay = 0;
    bool have = true;
    if (m.load<int32_t>(way + 0x10) != -1) {
      ax = sar32(m.load<int32_t>(way + 0x10), 10);
      ay = sar32(m.load<int32_t>(way + 0x14), 10);
    } else if (m.load<int32_t>(way + 8) != -1) {
      ax = sar32(m.load<int32_t>(way + 8), 10);
      ay = sar32(m.load<int32_t>(way + 0xc), 10);
    } else {
      have = false;
    }
    int32_t gx_, gy_;
    map_to_screen(m.load<int32_t>(rec + 0x18), m.load<int32_t>(rec + 0x1c), gx_, gy_);
    if (have) {
      int32_t sx, sy;
      map_to_screen(ax, ay, sx, sy);
      gx("showRoutingMapScreen.line", {u32(sx), u32(sy)});
      if (mode != 0)
        with_text(std::to_string(h), [&](uint32_t s) { gx("drawCenteredText2", {s, u32(sx), u32(sub32(sy, 4)), 0x80007ff0u}); });
    } else if (mode != 0) {
      with_text(std::to_string(h), [&](uint32_t s) {
        gx("drawCenteredText2", {s, u32(add32(gx_, tx) / 2), u32(sub32(add32(gy_, ty) / 2, 4)), 0x80007ff0u});
      });
    }
    gx("showRoutingMapScreen.line", {u32(gx_), u32(gy_)});
    gx("showRoutingMapScreen.flagSprite", {u32(gx_), u32(gy_)});
  }

  // the buttons: the hovered button's image, the mode's own button, the compass, then the refresh
  void buttons(int32_t mode, int32_t state) const {
    scale(1);
    struct Img { uint32_t off; int32_t x, y; };
    static const Img kState[5] = {{0x134, 0xb6, 0xfe}, {0xdc, 0x3c, 0x139}, {0x160, 0x1e3, 0xfe}, {0x108, 0x264, 0x139},
                                  {0x23c, 0x296, 0x214}};
    const Img *si = state == -2 ? &kState[4] : state == 1 ? &kState[1] : state == 0 ? &kState[0] : state == 2 ? &kState[2]
                  : state == 3 ? &kState[3] : nullptr;
    if (si) image(si->off, si->x, si->y);
    if (mode == 0) {
      if (state == 9) image(0x268, 0x307, 0x40);
      else if (state == 10) image(0x294, 0x307, 0xcd);
      image(0x18c, 0x3c, 0x139);
    } else if (mode == -1) {
      image(0x1e4, 0xb6, 0xfe);
    } else if (mode == 1) {
      image(0x210, 0x1e2, 0xfe);
    } else if (mode == 2) {
      image(0x1b8, 0x264, 0x139);
    }
    image(0x688, 0x47, 0x215);
    gx("Window::refreshFull", {win, 0});
  }
};

struct Screen {
  Ctx &ctx;
  GameState &gs;
  const Memory &m;
  int mutant;
  uint32_t holes, ways, walkers, golfers, flags, sheet;
  int32_t perm[20];

  explicit Screen(Ctx &c)
      : ctx(c),
        gs(c.gs()),
        m(c.mem),
        mutant(SG_NEGCTL(g_negctl_routing_map)),
        holes(state_va(c, &GameState::g_holes)),
        ways(state_va(c, &GameState::g_holePoints)),
        walkers(state_va(c, &GameState::g_walkers)),
        golfers(state_va(c, &GameState::g_golfers)),
        flags(state_va(c, &GameState::g_tileFlags)),
        sheet(state_va(c, &GameState::g_sheetCols)) {
    for (int i = 0; i < 20; ++i) perm[i] = i;
  }

  int32_t mode() const { return static_cast<int32_t>(gs.g_ui_4d20dc); }
  uint32_t hole(int32_t h) const { return holes + u32(mul32(static_cast<int32_t>(kHoleSize), h)); }
  uint32_t way(int32_t h) const { return ways + u32(mul32(static_cast<int32_t>(kWaySize), h)); }

  // the word map of the tiles: the low five bits hold the hole number (the tee tile at +8 / +0xc, the green tile at +0x18 /
  // +0x1c of the hole record; no bound test, as the original has none)
  void tile_number(uint32_t rec, uint32_t off, int32_t h) const {
    const int32_t x = m.load<int32_t>(rec + off), y = m.load<int32_t>(rec + off + 4);
    const uint32_t at = flags + 2u * u32(add32(mul32(x, 50), y));
    m.store<uint16_t>(at, static_cast<uint16_t>((m.load<uint16_t>(at) & 0xffe0u) | static_cast<uint32_t>(h)));
  }
  void number_hole(int32_t k) const {
    tile_number(hole(k), 8, k);
    tile_number(hole(k), 0x18, k);
  }
  void copy_hole(int32_t to, int32_t from) const {
    rep_movs(m, hole(to), hole(from), static_cast<uint32_t>(kHoleSize));
    if (mutant != 7) rep_movs(m, way(to), way(from), static_cast<uint32_t>(kWaySize));
    // the waypoint records stay in step with the holes
  }
  void clear_scratch() const {
    std::memset(m.at(hole(0), static_cast<uint32_t>(kHoleSize), true), 0, kHoleSize);
    std::memset(m.at(way(0), static_cast<uint32_t>(kWaySize), true), 0, kWaySize);
  }

  // one redraw pass without the drawing: the shade mode's spread, the title, the hole list's divisions and the walker rows.
  // Returns false when the screen ends here (the map's own end flag). `more` = walkers are hidden below the scroll window.
  bool redraw(int32_t scroll, int32_t count, bool &more, int32_t state, int32_t selected) {
    const MapDraw d(ctx);
    if (d.on) d.backdrop(mode());
    if (mode() == 1) call_peer(ctx, "spreadAllShade", {});
    if (d.on) {
      d.scale(5);   // display slot +0x9c (1, 1, 5)
      d.tiles(mode());
    }
    if (gs.g_map822b80[0] != 0) {
      if (d.on) d.scale(1);   // the display scale back to (1, 1, 1)
      return false;
    }
    reset_text(ctx);
    call_peer(ctx, "appendCourseTitle", {1});
    if (d.on) {
      d.legend(mode());
      for (int32_t h = 1; h <= 18; ++h) {
        d.hole_row(mode(), h, hole(h), selected, mutant);
        if (m.load<int8_t>(hole(h)) != 0) d.hole_map(mode(), h, hole(h), way(h));
      }
      if (mode() == 0) d.gx("showRoutingMapScreen.walkers", {});   // the walker list and markers: no body, reported
      d.buttons(mode(), state);
    }
    if (mode() == -1) {
      // the hole list prints each hole's average time: the one division the screen makes there
      for (int32_t h = 1; h <= 18; ++h) {
        const uint32_t rec = hole(h);
        if (m.load<int8_t>(rec) == 0) continue;
        const int32_t n = m.load<int32_t>(rec + 0x20);
        if (n != 0) (void)div32(div32(m.load<int32_t>(rec + 0x1ec), n), 40);
      }
    }
    int32_t skipped = 0, n = 1;
    more = false;
    (void)count;   // the scroll bar of a long list divides by the walker count, above 8 there: no failing division
    if (mode() != 0) return true;
    for (int32_t i = 0; i < 64; ++i) {
      const uint32_t w = walkers + kWalkerSize * static_cast<uint32_t>(i);
      const uint8_t fl = m.load<uint8_t>(w + 0x12);
      if (fl == 0) continue;
      const int32_t st = m.load<int8_t>(w + 0x13);
      if (st >= 0) continue;
      int32_t base = sub32(-2, st);
      if ((fl & 8) != 0) base = add32(base, 4);
      if (st != -6) {
        if (skipped < scroll) {
          ++skipped;
        } else if (sub32(n, skipped) > 8) {
          more = true;
        } else {
          // the row: its sprite sheet's frame column, and a kind-0xc walker on frame 0 becomes kind 0xb
          const int32_t kind = m.load<int16_t>(w + 0x1e);
          int32_t table = kind < 0xb ? 0x20e : (kind == 0xc ? 0x21e : 0x216);
          table = add32(table, base);
          const int32_t col = mod32(m.load<int16_t>(w + 0x1c), m.load<int32_t>(sheet + 4u * u32(table)));
          if (col == 0 && kind == 0xc && mutant != 8) m.store<int16_t>(w + 0x1e, 0xb);
          reset_text(ctx);
          call_peer(ctx, "appendWalkerName", {u32(i)});
          reset_text(ctx);
          call_peer(ctx, "appendDate", {u32(shl32(m.load<int16_t>(w + 0x22), mutant == 9 ? 9 : 10))});
          call_peer(ctx, "appendNumber", {u32(mul32(m.load<int16_t>(w + 0x24), 100))});
        }
      }
      // the walker's marker on the map: its number (counted here) and its name
      if (st != -6) n = add32(n, 1);
      reset_text(ctx);
      call_peer(ctx, "appendWalkerName", {u32(i)});
    }
    return true;
  }

  // a right click: the selected hole moves to the position under the cursor (its hole record, waypoint record and
  // permutation entry go to the scratch slot, the holes between shift by one, it lands at the target)
  void move_hole(int32_t s, int32_t t) {
    copy_hole(0, s);
    const int32_t saved = perm[s];
    perm[0] = saved;
    if (t > s) {
      for (int32_t k = s; k < t; ++k) {
        copy_hole(k, k + 1);
        perm[k] = perm[k + 1];
        number_hole(k);
      }
    } else if (t < s) {
      for (int32_t k = s; k > t; --k) {
        copy_hole(k, k - 1);
        perm[k] = perm[k - 1];
        number_hole(k);
      }
    }
    copy_hole(t, 0);
    perm[t] = perm[0];
    perm[0] = 0;
    if (mutant == 1) {
      tile_number(hole(t), 8, t);
    } else {
      number_hole(t);
    }
    clear_scratch();
  }

  // the exit: every golfer's positive hole byte through the inverse of the permutation, the hole count, the first free hole
  int32_t leave() {
    int32_t inv[20];
    for (int i = 0; i < 20; ++i) inv[perm[i]] = i;
    for (int32_t g = 0; g < 152; ++g) {
      const uint32_t at = golfers + kGolferSize * static_cast<uint32_t>(g) + 0x21;
      const int32_t b = m.load<int8_t>(at);
      if (b <= 0) continue;
      if (b < 20) {
        m.store<uint8_t>(at, static_cast<uint8_t>(mutant == 2 ? perm[b] : inv[b]));
      } else if (SG_DEFECT(g_defect_exit_remap_frame)) {
        throw MissingResource{"showRoutingMapScreen: the caller's frame (a hole byte of 20 or more)"};
      }
    }
    gs.g_holesPlusOne = 0x13;
    for (int32_t h = 1; h <= 18; ++h)
      if (m.load<int8_t>(hole(h)) == 0) {
        gs.g_holesPlusOne = mutant == 3 ? h + 1 : h;
        return static_cast<int32_t>(hole(h));
      }
    return static_cast<int32_t>(hole(19));
  }
};

int32_t show_routing_map_screen(Ctx &ctx) {
  Screen s(ctx);
  GameState &gs = s.gs;
  InputSource *src = input_source(ctx);
  int32_t scroll = 0, selected = 1, state = -1;
  // the listed walkers: flag byte set, a negative state byte other than the one the list skips
  int32_t count = 0;
  for (int32_t i = 0; i < 64; ++i) {
    const uint32_t w = s.walkers + kWalkerSize * static_cast<uint32_t>(i);
    if (s.m.load<uint8_t>(w + 0x13) != 0xfa && s.m.load<uint8_t>(w + 0x12) != 0 && s.m.load<int8_t>(w + 0x13) < 0) ++count;
  }
  bool more = false;
  for (;;) {
    if (!s.redraw(scroll, count, more, state, selected)) break;
    bool redraw = false, quit = false;
    while (!redraw && !quit) {
      int32_t px = 0, py = 0;
      if (!src || !src->cursor(px, py)) throw NeedInput{{"showRoutingMapScreen"}};
      const int32_t prev = state;
      state = route_hit(s.mode(), px, py, count);
      if (prev != state) {   // the hovered button changed: redraw first
        redraw = true;
        break;
      }
      cm::pump_messages(ctx, PumpKind::Blocking, "showRoutingMapScreen");
      int32_t click = gs.g_rootInput[kButton];
      bool clicked = true;
      if (click == 0) {
        if (cm::flush_redraw(ctx) == 0) continue;
        click = gs.g_rootInput[kButton];
        clicked = click != 0;
      }
      int32_t cy = gs.g_rootInput[kClickY];
      if (clicked && cy > 0x118) {   // the button row: the mode buttons set the mode, quit ends, the rest redraws
        if (state >= 0) gs.g_ui_4d20dc = static_cast<uint32_t>(s.mutant == 4 ? state : state - 1);
        if (state == -2) quit = true;
        else redraw = true;
        break;
      }
      if (state == 9) {
        if (s.mutant == 5) {
          if (more) ++scroll;
        } else if (scroll > 0) {
          --scroll;
        }
      } else if (state == 10 && more) {
        if (s.mutant == 5) {
          if (scroll > 0) --scroll;
        } else {
          ++scroll;
        }
      }
      if (cy >= 0x101) {   // a click below the map, or a key after one, ends the screen
        quit = true;
        break;
      }
      if (click == 1) {   // a left click selects the hole under it
        int32_t v = clamp(add32(sub32(cy, 0x68) / 17, 1), click, 0x13);
        if (gs.g_rootInput[kClickX] >= (s.mutant == 6 ? 0x180 : 0x190)) v = add32(v, 9);
        selected = clamp(v, 1, 0x12);
        click = gs.g_rootInput[kButton];
        cy = gs.g_rootInput[kClickY];
      }
      if (click == 2) {   // a right click moves the selected hole there
        int32_t t = clamp(add32(sub32(cy, 0x68) / 17, 1), 1, 0x13);
        if (gs.g_rootInput[kClickX] >= 0x190) t = add32(t, 9);
        t = clamp(t, 1, 0x12);
        s.move_hole(selected, t);
        click = gs.g_rootInput[kButton];
      }
      if (click != 0) redraw = true;
      else quit = true;
    }
    if (quit) break;
  }
  return s.leave();
}

// showShortcutsScreen: the keyboard-shortcuts help page and its one-button modal loop. The page (two backgrounds, the shortcut
// captions, the close button in its two states) is drawing. What the loop does: a Blocking pump, the cursor read, the close
// button's box (a hover only redraws the button), a left press inside the box clears the keep-going byte, a key ends the loop
// (flushRedraw), and on the way out the message-box mode byte g_ui_569498 is cleared and pumpLight drains what is left.
// The original returns what the Image destructor returns; no caller reads it and the port returns 0.
// Roadmap S2.8: the shortcut texts, in the original's order (c3es.cpp kShortcutTop then kShortcutRest; generated by
// log/roadmap/S2/8_tools/gen_shortcuts.py): Widget_draw (1) or drawStringAt (0), the title font g_fontRef_821020 (1) or
// g_fontRef_821ee8, the string (GameState offset), x, y. The line between the two parts is kShortcutMid.
#define GSO(sym, off) (static_cast<uint32_t>(offsetof(GameState, sym)) + static_cast<uint32_t>(off))
struct ShortcutText {
  uint8_t widget, title;
  uint32_t text;
  int16_t x, y;
};
const ShortcutText kShortcutTop[] = {
    {0, 1, GSO(f_strpool_4d2c28, 0x184), 0x11f, 0x1c},
    {0, 0, GSO(f_strpool_4d2c28, 0x180), 0x44, 0x51},
    {1, 0, GSO(f_strpool_4d2c28, 0x168), 0x6c, 0x51},
    {0, 0, GSO(f_strpool_4d2c28, 0x164), 0x44, 0x62},
    {1, 0, GSO(f_strpool_4d2c28, 0x14c), 0x6c, 0x62},
    {0, 0, GSO(f_strpool_4d2c28, 0x148), 0x44, 0x73},
    {1, 0, GSO(f_strpool_4d2c28, 0x13c), 0x6c, 0x73},
    {0, 0, GSO(f_strpool_4d2c28, 0x138), 0x44, 0x84},
    {1, 0, GSO(f_strpool_4d2c28, 0x124), 0x6c, 0x84},
    {0, 0, GSO(f_strpool_4d2c28, 0x120), 0x44, 0x95},
    {1, 0, GSO(f_strpool_4d2c28, 0x108), 0x6c, 0x95},
    {0, 0, GSO(f_strpool_4d2c28, 0x104), 0x44, 0xa6},
    {1, 0, GSO(f_strpool_4d2c28, 0xf8), 0x6c, 0xa6},
    {0, 0, GSO(f_strpool_4d2c28, 0xf4), 0x44, 0xb7},
    {1, 0, GSO(f_strpool_4d2c28, 0xe4), 0x6c, 0xb7},
    {0, 0, GSO(f_strpool_4d2c28, 0xe0), 0x44, 0xc8},
};
const ShortcutText kShortcutMid = {1, 0, GSO(f_strpool_4d2c28, 0xc0), 0x6c, 0xc8};
const ShortcutText kShortcutRest[] = {
    {0, 0, GSO(f_strpool_4d2c28, 0xbc), 0x44, 0xd9},
    {1, 0, GSO(f_strpool_4d2c28, 0xa8), 0x6c, 0xd9},
    {0, 0, GSO(f_strpool_4d2c28, 0xa4), 0x44, 0xea},
    {1, 0, GSO(f_strpool_4d2c28, 0x84), 0x6c, 0xea},
    {0, 0, GSO(f_strpool_4c5c0c, 0x0), 0x44, 0xfb},
    {1, 0, GSO(f_strpool_4d2c28, 0x6c), 0x6c, 0xfb},
    {0, 0, GSO(f_strpool_4d2c28, 0x64), 0x44, 0x10c},
    {1, 0, GSO(f_strpool_4d2c28, 0x4c), 0x6c, 0x10c},
    {0, 0, GSO(f_strpool_4d2c28, 0x44), 0x44, 0x11d},
    {1, 0, GSO(f_strpool_4d2c28, 0x24), 0x6c, 0x11d},
    {0, 0, GSO(f_strpool_4d2c28, 0x1c), 0x44, 0x12e},
    {1, 0, GSO(f_strpool_4d2c28, 0x0), 0x6c, 0x12e},
    {0, 0, GSO(f_strpool_4d2c24, 0x0), 0x44, 0x13f},
    {1, 0, GSO(f_strpool_4d2be8, 0x14), 0x6c, 0x13f},
    {0, 0, GSO(f_strpool_4d2be8, 0x10), 0x44, 0x157},
    {1, 0, GSO(f_strpool_4d2be8, 0x0), 0x6c, 0x157},
    {0, 0, GSO(f_strpool_4d2be4, 0x0), 0x1bc, 0x51},
    {1, 0, GSO(f_strpool_4d2bcc, 0x0), 0x1e3, 0x51},
    {0, 0, GSO(f_strpool_4d2bc8, 0x0), 0x1bc, 0x62},
    {1, 0, GSO(f_strpool_4d2bb0, 0x0), 0x1e3, 0x62},
    {0, 0, GSO(f_strpool_4d2bac, 0x0), 0x1bc, 0x73},
    {1, 0, GSO(f_strpool_4d2b98, 0x0), 0x1e3, 0x73},
    {0, 0, GSO(f_strpool_4d2b94, 0x0), 0x1bc, 0x84},
    {1, 0, GSO(f_strpool_4d2b7c, 0x0), 0x1e3, 0x84},
    {0, 0, GSO(f_strpool_4d2b78, 0x0), 0x1bc, 0x95},
    {1, 0, GSO(f_strpool_4d2b64, 0x0), 0x1e3, 0x95},
    {0, 0, GSO(f_strpool_4d2b60, 0x0), 0x1bc, 0xa6},
    {1, 0, GSO(f_strpool_4d2b4c, 0x0), 0x1e3, 0xa6},
    {0, 0, GSO(f_strpool_4d2b48, 0x0), 0x1bc, 0xb7},
    {1, 0, GSO(f_strpool_4d2b34, 0x0), 0x1e3, 0xb7},
    {0, 0, GSO(f_strpool_4d2b30, 0x0), 0x1bc, 0xc8},
    {1, 0, GSO(f_strpool_4d2b18, 0x0), 0x1e3, 0xc8},
    {0, 0, GSO(f_strpool_4d2b14, 0x0), 0x1bc, 0xd9},
    {1, 0, GSO(f_strpool_4d2a94, 0x70), 0x1e3, 0xd9},
    {0, 0, GSO(f_strpool_4c61dc, 0x8), 0x1bc, 0xea},
    {1, 0, GSO(f_strpool_4d2a94, 0x5c), 0x1e3, 0xea},
    {0, 0, GSO(f_strpool_4d2a94, 0x54), 0x1bc, 0xfb},
    {1, 0, GSO(f_strpool_4d2a94, 0x3c), 0x1e3, 0xfb},
    {0, 0, GSO(f_strpool_4d2a94, 0x34), 0x1bc, 0x10c},
    {1, 0, GSO(f_strpool_4d2a94, 0x20), 0x1e3, 0x10c},
    {0, 0, GSO(f_strpool_4d2a94, 0x18), 0x1bc, 0x11d},
    {1, 0, GSO(f_strpool_4d2a94, 0x0), 0x1e3, 0x11d},
    {0, 0, GSO(f_strpool_4d2a90, 0x0), 0x1bc, 0x12e},
    {1, 0, GSO(f_strpool_4d21a4, 0x8d4), 0x1e3, 0x12e},
    {0, 0, GSO(f_strpool_4c1430, 0x0), 0x1bc, 0x13f},
    {1, 0, GSO(f_strpool_4d21a4, 0x8bc), 0x1e3, 0x13f},
};
#undef GSO

// With an attached backend the page is drawn as c3es.cpp ShowShortcutsScreen_re draws it: its frame Image and two Surfaces
// (backend keys gfx_local_key(0x5000 / 0x5100 / 0x5200)) take interface\infoscreens\shortcuts.pcx and its alpha mask
// (f_strpool_4d2c28 + 0x1c4 / + 0x198, 0x320 x 0x199 each), the screen is dimmed, the page blended on with its mask, the 63
// texts drawn (the middle line in 0x80000200: the display's slot +0xb4 is jgld's `xor eax, eax; ret`, which selects it), the
// close button, then each pass refreshes the window and switches the button's hover image; the objects are freed at the end.
int32_t show_shortcuts_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int mutant = SG_NEGCTL(g_negctl_shortcuts);
  InputSource *src = input_source(ctx);
  const gfxr::ScreenDraw d(ctx);
  const uint32_t root = d.root(), img = gfx_local_key(0x5000), sa = gfx_local_key(0x5100), sb = gfx_local_key(0x5200);
  const uint32_t pool = GS(f_strpool_4d2c28), base = GS(f_strpool_4d2c28) - static_cast<uint32_t>(offsetof(GameState, f_strpool_4d2c28));
  auto text = [&](const ShortcutText &t) {
    const uint32_t va = base + t.text;
    d.gx(t.widget ? "Widget_draw" : "drawStringAt",
         {root, t.title ? GS(g_fontRef_821020) : GS(g_fontRef_821ee8), va, u32(t.x), u32(t.y), d.len(va)});
  };
  bool hot = false;
  if (d.on) {
    d.gx("reports.begin", {});
    d.gx("Image_ctor", {img});
    d.gx("Surface_ctor", {sa});
    d.gx("Surface_ctor", {sb});
    d.gx("Surface_createFull", {img, 0x320, 0x258, 0, 1, 0, 0});
    d.gx("loadImage", {img, pool + 0x1c4, 0, 0, 0x100, 1});
    d.gx("Surface_blitTakeDirty", {sa, img, 0, 0, 0x320, 0x199, 1, 0});
    d.gx("loadImage", {img, pool + 0x198, 0, 0, 0x100, 1});
    d.gx("Surface_blitTakeDirty", {sb, img, 0, 0, 0x320, 0x199, 1, 0});
    d.gx("Image_releaseAll", {img});
    d.gx("Surface_drawSprite", {GS(g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
    d.gx("Surface_stretchBlit", {sa, sb, root, 0, 0, 0});
    d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
    for (const ShortcutText &t : kShortcutTop) text(t);
    d.gx("Widget_setQuad", {root, gfxr::ScreenDraw::ctl(16) ? 0x80000000u : 0x80000200u, 0xffffffffu, 2, 2});
    text(kShortcutMid);
    d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
    for (const ShortcutText &t : kShortcutRest) text(t);
    d.gx("Image_draw", {GS(g_spriteRef_820f40), root, 0x2c6, 0x16a, 0});
    d.gx("Window::refreshFull", {d.window(), 0});
  }
  bool again = true;
  for (;;) {
    cm::pump_messages(ctx, PumpKind::Blocking, "showShortcutsScreen");
    int32_t x = 0, y = 0;
    if (!src || !src->cursor(x, y)) throw NeedInput{{"showShortcutsScreen"}};
    d.gx("Window::refreshFull", {d.window(), 0});
    if (d.on) {
      const bool in = x >= 0x2c6 && x < 0x2f2 && y >= 0x16a && y < 0x196;
      if (in != hot) {
        hot = in;
        d.gx("Image_draw", {GS(g_spriteRef_820f40) + (in ? 0x2cu : 0u), root, 0x2c6, 0x16a, 0});
        d.gx("Window::refreshFull", {d.window(), 0});
      }
    }
    const int32_t x1 = mutant == 2 ? 0x2e2 : 0x2f2;
    if (x >= 0x2c6 && x < x1 && y >= 0x16a && y < 0x196) {
      const int32_t b = gs.g_rootInput[kButton];
      if (b == 1 || (mutant == 1 && b == 2)) again = false;
    }
    if (cm::flush_redraw(ctx) != 0 && mutant != 4) break;
    if (!again) break;
  }
  gs.g_ui_569498 = mutant == 3 ? 1 : 0;
  cm::pump_light(ctx, "showShortcutsScreen");
  d.gx("Surface_free", {sa});
  d.gx("Surface_free", {sb});
  d.gx("Surface_free", {sb});
  d.gx("Surface_free", {sa});
  d.gx("Image_dtor", {img});
  return 0;
}

// showMembershipRoster: the member-type roster page (the rows are chosen by repeated selection over the 76 type records with the
// bit 7 of the flag byte as the pass marker, then drawn) and its modal loop. The one simulation effect of the page is that
// marker: every redraw sets it on the listed records and then clears it from ALL 76 records, so the exclusion bit those records
// carry on entry is gone after the first redraw (U-1023). The scroll offset (the arrows move it by clamp(.., 1, 3)) only picks
// which rows are drawn and has no further effect. Loop: the hovered zone (close button, up arrow, down arrow, none) changes as
// the cursor moves; a Blocking pump per pass; a button or a key ends the pass; a left press on the close button leaves the screen,
// on an arrow or no zone redraws; any other button leaves unless the pass ended because the cursor left an arrow zone.
//
// Roadmap S2.8: with an attached backend each redraw is drawn as c3eo.cpp ShowMembershipRoster_re draws it (struct RosterDraw
// below): the rows chosen by the same repeated selection over a copy of the flag bytes (the marker the original sets is the
// port's write above, so the copy gives the drawing the same rows), the scroll offset the arrows move kept for the drawing,
// and the hover images of the three zones.
namespace {
struct RosterDraw {
  Ctx &ctx;
  const gfxr::ScreenDraw d;
  uint32_t root, font_b, art, types, names;
  int32_t count = 0;
  explicit RosterDraw(Ctx &c)
      : ctx(c),
        d(c),
        root(d.root()),
        font_b(state_va(c, &GameState::g_fontRef_821ee8)),
        art(state_va(c, &GameState::g_spriteRef_821f48)),
        types(state_va(c, &GameState::g_golferTypeState)),
        names(state_va(c, &GameState::g_golferTypes) + 0x10) {
    // the records not excluded with a member count, at least 1
    for (uint32_t k = 0; k < 76; ++k)
      if ((c.mem.load<uint8_t>(types + 0x2c * k + 2) & 0x80) == 0 && c.mem.load<uint16_t>(types + 0x2c * k + 0x2a) != 0) ++count;
    if (count == 0) count = 1;
  }
  void image(uint32_t sref, int32_t x, int32_t y) const { d.gx("Image_draw", {sref, root, u32(x), u32(y), 0}); }
  void w_draw(uint32_t s, int32_t x, int32_t y, uint32_t font = 0) const {
    d.gx("Widget_draw", {root, font ? font : font_b, s, u32(x), u32(y), d.len(s)});
  }
  void s_at(uint32_t s, int32_t x, int32_t y) const { d.gx("drawStringAt", {root, font_b, s, u32(x), u32(y), d.len(s)}); }
  // a number through itoa (g_ui_821c50) into the text buffer, or the text f_strpool_4c52bc when `show` is false
  template <class F>
  void number(int32_t v, bool show, F draw) const {
    d.text(
        [&] {
          d.put("");
          if (show) d.append(std::to_string(v));
          else d.append_va(state_va(ctx, &GameState::f_strpool_4c52bc));
        },
        draw);
  }
  void page(int32_t offset) const {
    if (!d.on) return;
    const Memory &m = ctx.mem;
    const uint32_t p32f0 = state_va(ctx, &GameState::f_strpool_4d32f0), p4e8 = state_va(ctx, &GameState::f_strpool_4c84e8);
    d.gx("Surface_stretchBlit", {art, art + 0x2c, root, 0, 0, 0});
    d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
    w_draw(p32f0, 0xd4, 0x13, state_va(ctx, &GameState::g_fontRef_821020));
    w_draw(p4e8 + 0x18c, 0x21, 0x3c);
    const uint32_t head[4] = {state_va(ctx, &GameState::f_strpool_4d32e8) + 4, state_va(ctx, &GameState::f_strpool_4d32e8),
                              state_va(ctx, &GameState::f_strpool_4d32d8) + 8, state_va(ctx, &GameState::f_strpool_4d32d8)};
    static const int32_t kHeadX[4] = {0xaa, 0xd1, 0xf9, 0x140};
    for (int h = 0; h < 4; ++h) s_at(head[h], kHeadX[h], 0x3c);
    for (int32_t hole = 1, hx = 0x17d; hx < 0x2f7; hx += 0x15, ++hole)
      d.text([&] { d.put(std::to_string(hole)); },
             [&](uint32_t s) { d.gx("Widget_draw2", {root, font_b, s, u32(hx), 0x3c, 0x13, d.len(s)}); });
    if (count > 0x16) {
      const int32_t thumb = div32(mul32(364, offset), count);
      image(art + 0x344, 0x2ff, 0x50);
      int32_t h = div32(0x1f48, count);
      if (h >= sub32(0x16c, thumb)) h = sub32(0x16c, thumb);
      d.gx("fillWindowRect", {d.window(), 0x305, u32(add32(thumb, 0x7a)), 6, u32(h), 0x80007fffu});
    }
    // the rows: repeated selection of the smallest name (`<=`: a later equal name wins) over the records not excluded with
    // members, the seed name f_strpool_4d317c + 0x148; each pick marked in the copy
    uint8_t flags[76];
    for (uint32_t k = 0; k < 76; ++k) flags[k] = m.load<uint8_t>(types + 0x2c * k + 2);
    const std::string seed = gfxb_vstring(state_va(ctx, &GameState::f_strpool_4d317c) + 0x148);
    int32_t row_counter = 0;
    for (int32_t row_y = 0x59; row_y < 0x211;) {
      std::string best = seed;
      int32_t pick = -1;
      for (int32_t k = 0; k < 76; ++k) {
        const uint32_t rec = types + 0x2c * u32(k);
        if ((flags[k] & 0x80) != 0 || m.load<uint16_t>(rec + 0x2a) == 0) continue;
        const std::string nm = gfxb_vstring(names + 0x230 * u32(k));
        if (cmp(best, nm) < 0) continue;
        pick = k;
        best = nm;
      }
      if (pick < 0) break;
      const uint32_t rec = types + 0x2c * u32(pick);
      flags[pick] = static_cast<uint8_t>(flags[pick] | 0x80);
      if (row_counter < offset) {
        ++row_counter;
        continue;
      }
      int32_t status = flags[pick] & 7;
      if (m.load<uint8_t>(rec + 0x29) == 0xff) status = -1;
      d.text([&] { d.put(""); d.append_va(names + 0x230 * u32(pick)); }, [&](uint32_t s) { w_draw(s, gfxr::ScreenDraw::ctl(15) ? 0x1d : 0x1c, row_y); });
      if (status != 1) {
        const int32_t k = status >= 3 ? (status > 3 ? 2 : 1) : (status == -1 ? 3 : 0);
        image(art + 0x84 + 0x58 * u32(k), 0x7a, sub32(row_y, 4));
      }
      const uint8_t v0 = m.load<uint8_t>(rec);
      number(v0, v0 != 0, [&](uint32_t s) { s_at(s, 0xaa, row_y); });
      const int8_t v1 = m.load<int8_t>(rec + 1);
      number(v1, v1 > 0, [&](uint32_t s) { w_draw(s, 0xd1, row_y); });
      const int16_t v2 = m.load<int16_t>(rec + 0x2a);
      number(v2, v2 != 0, [&](uint32_t s) { w_draw(s, 0xf9, row_y); });
      for (int32_t hole = 1, hx = 0x17d; hx < 0x2f7; hx += 0x15, ++hole) {
        const uint8_t f = m.load<uint8_t>(rec + 3 + u32(hole));
        if ((f & 3) != 0) {
          const int32_t e = (f & 1) != 0 ? ((~hole) & 1) | 8 : ((~hole) & 1) | 0xa;
          image(art + 0x58 + 0x2c * u32(e), hx, sub32(row_y, 4));
        }
        if ((f & 4) != 0) image(art + 0x58 + 0x2c * u32(((~hole) & 1) | 6), hx, sub32(row_y, 4));
      }
      // the status label (index status + 1: Resigned, none, Visitor, Member, Silver Member, Gold Member)
      const uint32_t label[6] = {p4e8 + 0x194, 0, state_va(ctx, &GameState::f_strpool_4c3ac0), p4e8 + 0x18c, p4e8 + 0x17c,
                                 p4e8 + 0x170};
      const uint32_t sel = u32(status + 1);
      if (sel <= 5 && label[sel] != 0) s_at(label[sel], 0x140, row_y);
      row_y += 0x14;
    }
    const uint32_t p17c = state_va(ctx, &GameState::f_strpool_4d317c);
    const uint32_t foot[6] = {p4e8 + 0x18c, p4e8 + 0x17c, p4e8 + 0x170, p4e8 + 0x194, p17c + 0x13c, p17c + 0x12c};
    static const int32_t kFootX[6] = {0x52, 0xd0, 0x146, 0x1c2, 0x23d, 0x2b5};
    for (int f = 0; f < 6; ++f) s_at(foot[f], kFootX[f], 0x218);
  }
  // after the redraw's cursor read: the close button in its normal state and the refresh
  void finish() const {
    image(state_va(ctx, &GameState::g_spriteRef_820f40), 0x2dc, 0x224);
    d.gx("Window::refreshFull", {d.window(), 0});
  }
  // the hover images of a zone change that does not end the pass (hover <= 0 before it; c3eo.cpp's input loop)
  void hover_to(int32_t from, int32_t to) const {
    if (!d.on) return;
    const uint32_t ok = state_va(ctx, &GameState::g_spriteRef_820f40);
    if (to == 0) {
      image(ok + 0x2c, 0x2dc, 0x224);
    } else {
      if (from == 0) image(ok, 0x2dc, 0x224);
      if (to == 1) image(art + 0x268, 0x2ff, 0x50);
      if (to == 2) image(art + 0x294, 0x2ff, 0x1ea);
    }
    d.gx("Window::refreshFull", {d.window(), 0});
  }
  std::string gfxb_vstring(uint32_t va) const {
    std::string r;
    for (uint8_t c; (c = ctx.mem.load<uint8_t>(va)) != 0; ++va) r.push_back(static_cast<char>(c));
    return r;
  }
  // the inlined strcmp (unsigned bytes)
  static int cmp(const std::string &a, const std::string &b) {
    size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
    const unsigned ca = i < a.size() ? static_cast<uint8_t>(a[i]) : 0, cb = i < b.size() ? static_cast<uint8_t>(b[i]) : 0;
    return ca < cb ? -1 : ca > cb ? 1 : 0;
  }
};
}  // namespace

int32_t show_membership_roster(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int mutant = SG_NEGCTL(g_negctl_roster);
  InputSource *src = input_source(ctx);
  const uint32_t types = state_va(ctx, &GameState::g_golferTypeState);
  int32_t hover = -1;
  bool excluded[76];
  for (uint32_t k = 0; k < 76; ++k) excluded[k] = (m.load<uint8_t>(types + 0x2c * k + 2) & 0x80) != 0;
  const RosterDraw rd(ctx);
  int32_t offset = 0;   // the scroll offset (drawing only)
  if (rd.d.on) {
    rd.d.gx("reports.begin", {});
    if (gs.g_graphicsDevice != 0) rd.d.gx("Display::setScale", {1, 1, 1});
    rd.d.gx("Surface_drawSprite", {state_va(ctx, &GameState::g_spriteRef_821040), rd.root, 0, 0, 0x3e800000u, 0, 1});
  }
  for (;;) {
    rd.page(offset);
    for (uint32_t k = 0; k < 76; ++k) {
      if (mutant == 1 && excluded[k]) continue;   // control: the exclusion bit survives
      m.store<uint8_t>(types + 0x2c * k + 2, m.load<uint8_t>(types + 0x2c * k + 2) & 0x7f);
    }
    int32_t px = 0, py = 0;
    if (src) (void)src->cursor(px, py);   // the cursor read before the first wait: its point is not used
    rd.finish();
    hover = -1;
    bool left = false;
    for (;;) {
      int32_t x = 0, y = 0;
      if (!src || !src->cursor(x, y)) throw NeedInput{{"showMembershipRoster"}};
      const int32_t x1 = mutant == 2 ? 0x2f8 : 0x308;
      int32_t zone = -1;
      if (x >= 0x2dc && x < x1 && y >= 0x224 && y < 0x250) zone = 0;
      else if (x >= 0x2ff && x < 0x311 && y >= 0x50 && y < 0x75) zone = 1;
      else if (x >= 0x2ff && x < 0x311 && y >= 0x1ea && y < 0x20f) zone = 2;
      bool leave = false;
      if (zone != hover) {
        if (hover > 0) {
          left = true;
          leave = true;
        } else {
          rd.hover_to(hover, zone);
        }
        hover = zone;
      }
      if (leave) break;
      cm::pump_messages(ctx, PumpKind::Blocking, "showMembershipRoster");
      if (gs.g_rootInput[kButton] != 0) break;
      if (cm::flush_redraw(ctx) != 0) break;
    }
    const int32_t b = gs.g_rootInput[kButton];
    if (b == 1) {
      if (hover == 0 || (mutant == 4 && hover > 0)) break;
      if (hover == 1 && offset > 0) offset = sub32(offset, clamp(offset, 1, 3));
      if (hover == 2 && sub32(rd.count, 0x16) > offset) offset = add32(offset, clamp(sub32(sub32(rd.count, offset), 0x16), 1, 3));
    } else if (!left && !(mutant == 3 && hover > 0)) {
      break;
    }
  }
  gs.g_ui_569498 = mutant == 5 ? 1 : 0;
  cm::pump_light(ctx, "showMembershipRoster");
  return 0;
}

const FuncDef kFuncs[] = {
    {"showMembershipRoster", "default", "int", 0,
     0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(show_membership_roster(c)); }, &g_negctl_roster},
    {"showShortcutsScreen", "default", "int", 0,
     0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(show_shortcuts_screen(c)); }, &g_negctl_shortcuts},
    {"showRoutingMapScreen", "default", "int", 0,
     0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(show_routing_map_screen(c)); }, &g_negctl_routing_map},
};
}  // namespace

extern const ModuleDef kModule_routing = {"routing", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                         sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
