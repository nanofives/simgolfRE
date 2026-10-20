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
#include <cstring>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/util.h"
#include "commands_internal.h"

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
  bool redraw(int32_t scroll, int32_t count, bool &more) {
    if (mode() == 1) call_peer(ctx, "spreadAllShade", {});
    if (gs.g_map822b80[0] != 0) return false;
    reset_text(ctx);
    call_peer(ctx, "appendCourseTitle", {1});
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
    if (!s.redraw(scroll, count, more)) break;
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
int32_t show_shortcuts_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int mutant = SG_NEGCTL(g_negctl_shortcuts);
  InputSource *src = input_source(ctx);
  bool again = true;
  for (;;) {
    cm::pump_messages(ctx, PumpKind::Blocking, "showShortcutsScreen");
    int32_t x = 0, y = 0;
    if (!src || !src->cursor(x, y)) throw NeedInput{{"showShortcutsScreen"}};
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
  return 0;
}

// showMembershipRoster: the member-type roster page (the rows are chosen by repeated selection over the 76 type records with the
// bit 7 of the flag byte as the pass marker, then drawn) and its modal loop. The one simulation effect of the page is that
// marker: every redraw sets it on the listed records and then clears it from ALL 76 records, so the exclusion bit those records
// carry on entry is gone after the first redraw (U-1023). The scroll offset (the arrows move it by clamp(.., 1, 3)) only picks
// which rows are drawn and has no further effect. Loop: the hovered zone (close button, up arrow, down arrow, none) changes as
// the cursor moves; a Blocking pump per pass; a button or a key ends the pass; a left press on the close button leaves the screen,
// on an arrow or no zone redraws; any other button leaves unless the pass ended because the cursor left an arrow zone.
int32_t show_membership_roster(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int mutant = SG_NEGCTL(g_negctl_roster);
  InputSource *src = input_source(ctx);
  const uint32_t types = state_va(ctx, &GameState::g_golferTypeState);
  int32_t hover = -1;
  bool excluded[76];
  for (uint32_t k = 0; k < 76; ++k) excluded[k] = (m.load<uint8_t>(types + 0x2c * k + 2) & 0x80) != 0;
  for (;;) {
    for (uint32_t k = 0; k < 76; ++k) {
      if (mutant == 1 && excluded[k]) continue;   // control: the exclusion bit survives
      m.store<uint8_t>(types + 0x2c * k + 2, m.load<uint8_t>(types + 0x2c * k + 2) & 0x7f);
    }
    int32_t px = 0, py = 0;
    if (src) (void)src->cursor(px, py);   // the cursor read before the first wait: its point is not used
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
