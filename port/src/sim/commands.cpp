// Commands (see portsim/sim/commands.h). Phase 3.7b, written from the specification (analysis notes, hook sources as
// reading reference, the corpora); no original address or code.
#include "portsim/sim/commands.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "commands_internal.h"
#include "portsim/gamefiles.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/course.h"
#include "portsim/sim/gfx.h"
#include "gfx_reports.h"
#include "portsim/sim/semantic.h"
#include "portsim/sim/tick.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   commands.rootHandler  1 = onPress1 stores the button word 2, 2 = the press handlers' scaled x is x / scale (not 2x),
//                         3 = onKey keeps a for codes 0x60..0x69, 4 = onUnpoint stores 0 instead of -1
//   pumpAndRedraw         1 = the click flag does not keep the button word, 2 = the key words are not copied after the wait,
//                         3 = not paced by its clock reads (a recorded key reaches the first pass)
//   handleEmployeeAction  1 = firing costs 0x18
//   clickEmployeeView     1 = camera x >> 9
//   clickGolferPanel      1 = shot style 7 -> 4
//   clickBuildTool433d30  1 = a tool sets flag bit 2 instead of bit 1
//   pickBuilding          1 = the uniqueness refusal at level 3
//   clickAmenityPanel     1 = case 5 draws Random::range(5)
//   clickSlotPanel        1 = the slot table read one entry later, 2 = no Random::range draw
//   drawAmenitiesPanel    1 = tool 2's row has 6 items
//   drawEmployeeDetailPanel 1 = the walker animation word becomes 0xa
//   drawEmployeeViewPanel 1 = the wide layout's column limit 0x2af (drops the fourth column at 0x2b0; 0x2f3 was an
//                           equivalent mutant: the wide columns are 0x145 + k * 0x79, none in 0x2b8..0x2f3)
//   tickMessage           1 = the open counter counts up to 4
//   report screens        1 = the mode byte is left 1, 2 = the financial total + 1, 3 = the loading picture draw 11
//   showHireEmployeeDialog 1 = the row mapping without the + 1, 2 = the row text appended instead of copied
//   showCreditsScreen 1 = a key does not end the scroll
//   selectDifficultyScreen 1 = the back button is not mapped to -1
//   showPopup             1 = a check-list click ORs the option bit (instead of toggling it), 2 = Up passes the first option
//   textInputBox          1 = the width cut at width - 0x18, 2 = an inserted character is stored + 1
//   panelListSelect       1 = item 7 sets shot style 3, 2 = the difficulty refusal from difficulty 3
//   menus (one switch)    1 = nearestMenuSpot's limit 31, 2 = showPreferencesMenu stores the result ^ 1,
//                         3 = drawScoreSummary formats the multiplier without + 1, 4 = showSystemMenu's spot 4 is 0x51
//   hit tests (mode 1 each): hitGolferPanel435f00 hotspot 10 returns 9, hitEmployeeSlot435570 row height 20,
//     hitEmployee436b00 the wide entries return idx + 1, hitTestSlot432f90 box width 0x3f, pickAmenity434980 hotspot 9
//     returns 7, hitBuildTool433c60 the -3 anchor returns -2, hitBuildingTool cell 8 returns 7
int g_negctl_root_handler = 0;
int g_negctl_pump = 0;
int g_negctl_employee_action = 0;
int g_negctl_employee_view = 0;
int g_negctl_golfer_panel = 0;
int g_negctl_build_tool = 0;
int g_negctl_pick_building = 0;
int g_negctl_amenity_panel = 0;
int g_negctl_slot_panel = 0;
int g_negctl_hit_golfer_panel = 0;
int g_negctl_hit_employee_slot = 0;
int g_negctl_hit_employee = 0;
int g_negctl_hit_test_slot = 0;
int g_negctl_pick_amenity = 0;
int g_negctl_hit_build_tool = 0;
int g_negctl_hit_building_tool = 0;
int g_negctl_amenities_panel = 0;
int g_negctl_detail_panel = 0;
int g_negctl_view_panel = 0;
int g_negctl_tick_message = 0;
int g_negctl_reports = 0;
// Compatibility switch (port/CORE.md "Defects"), 1 = behave like the original.
//   showFinancialReport.sumWriteBack  model_defects#24 (D24): the report sums entries 0..7 of every drawn year row and
//                            stores the int16 sum into entry 8 (+0x10) of that row, so opening the
//                            report changes the saved ledger. Fixed (0): the sum is drawn and not stored.
int g_defect_finance_sum_write_back = 1;
int g_negctl_hire_dialog = 0;
int g_negctl_credits = 0;
int g_negctl_difficulty = 0;
int g_negctl_popup = 0;
int g_negctl_text_input = 0;
int g_negctl_panel_list = 0;
int g_negctl_menus = 0;

std::map<const Ctx *, InputSource *> &sources() {
  static std::map<const Ctx *, InputSource *> m;
  return m;
}
std::map<const Ctx *, TextMetrics *> &metrics() {
  static std::map<const Ctx *, TextMetrics *> m;
  return m;
}

// scaleX: (g_screenWidth * v + 160) / 320, signed (the press handlers call it with 2)
int32_t scale_x(Ctx &ctx, int32_t v) { return div32(add32(mul32(ctx.gs().g_screenWidth, v), 160), 320); }

// g_rootInput (int32[16] from): the slots the root handlers write
enum RootSlot : int {
  kScaledX = 0,   // +0x00 2x / scaleX(2)
  kScaledY = 1,   // +0x04
  kClickX = 2,    // +0x08
  kClickY = 3,    // +0x0c
  kButton = 4,    // +0x10 1 left, 2 right
  kShift = 5,     // +0x14 GetAsyncKeyState(VK_SHIFT) bits 8..15
  kCtrl = 6,      // +0x18 GetAsyncKeyState(VK_CONTROL) bits 8..15
  kClicked = 7,   // +0x1c 1
  kPressB = 8,    // +0x20 0 (onPress1 / 2), 1 (onPress1b / 2b)
  kPointX = 13,   // +0x34 -1 = no button down
  kPointY = 14,   // +0x38
};

int32_t &root(Ctx &ctx, int slot) { return ctx.gs().g_rootInput[slot]; }

// the deadline tail of onPoint / onUnpoint / onKey: deadline word 0 clears the byte g_ui83ab9c (eax 0); otherwise the
// clock is compared unsigned and only a time above the deadline clears it (eax = the time)
int32_t point_tail(Ctx &ctx, uint32_t now) {
  GameState &gs = ctx.gs();
  const uint32_t deadline = gs.g_audio5aa774;
  if (deadline == 0) {
    gs.g_ui83ab9c = 0;
    return 0;
  }
  if (now > deadline) gs.g_ui83ab9c = 0;
  return i32(now);
}

// onPress1 / onPress2 (button 1 / 2)
int32_t press(Ctx &ctx, const Command &c, int32_t button) {
  const int mode = SG_NEGCTL(g_negctl_root_handler);
  const int32_t x = c.a0, y = c.a1;
  const int32_t s1 = scale_x(ctx, 2);
  root(ctx, kScaledX) = mode == 2 ? div32(x, s1) : div32(add32(x, x), s1);
  const int32_t s2 = scale_x(ctx, 2);
  root(ctx, kClickX) = x;
  root(ctx, kClickY) = y;
  if (button == 2) root(ctx, kButton) = 2;
  root(ctx, kScaledY) = div32(add32(y, y), s2);
  root(ctx, kCtrl) = i32((c.async_ctrl >> 8) & 0xffu);
  if (button == 1) root(ctx, kButton) = mode == 1 ? 2 : 1;
  root(ctx, kShift) = i32((c.async_shift >> 8) & 0xffu);
  root(ctx, kClicked) = 1;
  root(ctx, kPressB) = 0;
  return button == 1 ? 1 : i32(c.async_shift);
}

// onPress1b / onPress2b: the same positions, no key state, the second flag word 1
int32_t press_b(Ctx &ctx, const Command &c, int32_t button) {
  const int32_t x = c.a0, y = c.a1;
  const int32_t s1 = scale_x(ctx, 2);
  root(ctx, kScaledX) = div32(add32(x, x), s1);
  const int32_t s2 = scale_x(ctx, 2);
  root(ctx, kClickX) = x;
  root(ctx, kClickY) = y;
  root(ctx, kScaledY) = div32(add32(y, y), s2);
  root(ctx, kButton) = button;
  root(ctx, kPressB) = 1;
  root(ctx, kClicked) = 1;
  return 1;
}

int32_t root_handler(Ctx &ctx, const Command &c) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_root_handler);
  switch (c.handler) {
    case 1: return press(ctx, c, 1);  // onPress1 (WM_LBUTTONUP: the click)
    case 2:                           // onPoint (WM_LBUTTONDOWN)
      root(ctx, kPointX) = c.a0;
      root(ctx, kPointY) = c.a1;
      return point_tail(ctx, c.t);
    case 3:  // onUnpoint (WM_LBUTTONUP, after onPress1)
      root(ctx, kPointX) = mode == 4 ? 0 : -1;
      return point_tail(ctx, c.t);
    case 4: return press(ctx, c, 2);  // onPress2
    case 5:                           // onPoint2: no deadline tail, eax = x
      root(ctx, kPointX) = c.a0;
      root(ctx, kPointY) = c.a1;
      return c.a0;
    case 6:  // onUnpoint2 (void)
      root(ctx, kPointX) = -1;
      return 0;
    case 7: {  // onKey (WM_CHAR): (a, code)
      const int32_t a = c.a0, code = c.a1;
      gs.gDirtyFlagB = (code >= 0x60 && code <= 0x69 && mode != 3) ? 0 : a;
      gs.gDirtyFlagA = code;
      gs.g_key838208 = code == 0x1000d ? 1 : 0;
      if (gs.g_audio5aa774 == 0 || c.t > gs.g_audio5aa774) gs.g_ui83ab9c = 0;
      return 1;
    }
    case 8:  // onChar (WM_KEYDOWN)
      gs.gDirtyFlagA = c.a0;
      gs.g_char83821c = c.a0;
      return 0;
    case 9:  // onChar0 (WM_KEYUP)
      gs.g_char83821c = 0;
      return 0;
    case 10: return press_b(ctx, c, 1);  // onPress1b
    case 11: return press_b(ctx, c, 2);  // onPress2b
    default: return 0;
  }
}

}  // namespace

bool is_stop_handler(uint32_t h) { return h == 1 || h == 4 || h == 7 || h == 8 || h == 10 || h == 11; }

void set_input_source(Ctx &ctx, InputSource *src) {
  if (src) sources()[&ctx] = src;
  else sources().erase(&ctx);
}
InputSource *input_source(Ctx &ctx) {
  const auto it = sources().find(&ctx);
  return it == sources().end() ? nullptr : it->second;
}

void set_text_metrics(Ctx &ctx, TextMetrics *tm) {
  if (tm) metrics()[&ctx] = tm;
  else metrics().erase(&ctx);
}
TextMetrics *text_metrics(Ctx &ctx) {
  const auto it = metrics().find(&ctx);
  return it == metrics().end() ? nullptr : it->second;
}

// ------------------------------------------------------------------------------------------------ shared helpers
namespace ev {
int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound);  // port/src/sim/events_calls.inc
void append_cents(Ctx &ctx, int32_t n);
}  // namespace ev

namespace cm {

int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound) { return ev::start_message(ctx, arg, prio, sound); }
void append_cents(Ctx &ctx, int32_t n) { ev::append_cents(ctx, n); }

// test seam: per context, peers whose calls are recorded into a log block instead of run (commands.recordPeer); the
// oracle stubs the same callees in the original and compares the two call sequences. Records: 8 dwords (FNV-1a of the
// name, up to 6 arguments, the argument count); the dword at the log base counts them.
struct PeerLog {
  uint32_t va = 0, cap = 0;
  std::vector<uint32_t> names;
};
std::map<const Ctx *, PeerLog> &peer_logs() {
  static std::map<const Ctx *, PeerLog> m;
  return m;
}
uint32_t fnv1a(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}

Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const auto pl = peer_logs().find(&ctx);
  if (pl != peer_logs().end()) {
    const uint32_t h = fnv1a(name);
    for (uint32_t n : pl->second.names)
      if (n == h) {
        const uint32_t k = ctx.mem.load<uint32_t>(pl->second.va);
        if (k < pl->second.cap) {
          const uint32_t rec = pl->second.va + 4 + 32 * k;
          ctx.mem.store<uint32_t>(rec, h);
          uint32_t j = 0;
          for (uint32_t v : args)
            if (j < 6) ctx.mem.store<uint32_t>(rec + 4 + 4 * j++, v);
          ctx.mem.store<uint32_t>(rec + 28, static_cast<uint32_t>(args.size()));
        }
        ctx.mem.store<uint32_t>(pl->second.va, k + 1);
        return Ret{};
      }
  }
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

Ret call_optional(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args, uint32_t none) {
  if (registry_find(name) >= 0) return call_peer(ctx, name, args);
  Ret r;
  r.eax = none;
  return r;
}

int32_t rng_range(Ctx &ctx, int32_t n) {
  return i32(random_range(state_obj(ctx, &GameState::g_rng).field(&Random::state).ref(), u32(n)) & 0xffffu);
}

int32_t take_key(Ctx &ctx) {
  const int32_t v = ctx.gs().gDirtyFlagB;
  ctx.gs().gDirtyFlagB = 0;
  return v;
}
int32_t take_code(Ctx &ctx) {
  const int32_t v = ctx.gs().gDirtyFlagA;
  ctx.gs().gDirtyFlagA = 0;
  return v;
}

void pump_messages(Ctx &ctx, PumpKind kind, const char *where) {
  GameState &gs = ctx.gs();
  if (gs.g_rootInput[kClicked] == 0) gs.g_rootInput[kButton] = 0;
  // doMouse: jgld's pumps deliver the messages that have arrived
  InputSource *src = input_source(ctx);
  if (kind == PumpKind::Blocking && (!src || src->exhausted())) throw NeedInput{{where}};
  std::vector<Command> batch;
  if (src && src->batch(kind, batch))
    for (const Command &c : batch) apply_command(ctx, c);
  gs.g_rootInput[kClicked] = 0;
}

// doAllTasks in loadFlic (+0x54): the display's showScreen loop dispatches what has arrived; no click / button words
// are touched (pumpMessages' are not this pump's)
void pump_load(Ctx &ctx) {
  InputSource *src = input_source(ctx);
  std::vector<Command> batch;
  if (src && src->batch(PumpKind::Load, batch))
    for (const Command &c : batch) apply_command(ctx, c);
}

void pump_light(Ctx &ctx, const char *where) {
  GameState &gs = ctx.gs();
  gs.g_rootInput[kButton] = 0;
  do {
    pump_messages(ctx, PumpKind::Drain, where);  // doAllTasks before it dispatches nothing in the port
  } while (gs.g_rootInput[kButton] != 0 || flush_redraw(ctx) != 0);
}

int32_t flush_redraw(Ctx &ctx) {
  GameState &gs = ctx.gs();
  if (gs.g_redrawPending != 0) {
    gs.g_redrawPending = 0;
    gs.gDirtyFlagB = 0;
    gs.gDirtyFlagA = 0;
  }
  if (gs.gDirtyFlagA != 0 || gs.gDirtyFlagB != 0) {
    gs.g_redrawPending = 1;
    return 1;
  }
  return 0;
}

// pumpAndRedraw's two timeGetTime reads as clock sites (offsets from mainLoop's frame-head return address, InputSource::clock):
// `start` before the loop and `now` after each pass's pump
constexpr int32_t kSitePumpStart = 0x4c49d, kSitePumpPass = 0x4c4b6;

int32_t pump_and_redraw(Ctx &ctx, int32_t limit, const char *where) {
  (void)limit;  // every caller passes 0: the wait has no time limit (the clock values are not used)
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_pump);
  gs.g_rootInput[15] = 0;
  gs.g_rootInput[kButton] = 0;
  // PACE: the original's reads pace the passes, so a recorded dispatch reaches the pass it reached there (commands.h)
  InputSource *src = input_source(ctx);
  uint32_t t = 0;
  const bool paced = src != nullptr && mode != 3 && src->clock(kSitePumpStart, t);
  if (paced) src->pace(kSitePumpPass);
  struct Unpace {
    InputSource *s;
    ~Unpace() {
      if (s) s->pace(0);
    }
  } unpace{paced ? src : nullptr};
  for (;;) {
    pump_messages(ctx, PumpKind::Blocking, where);
    if (paced && !src->clock(kSitePumpPass, t)) src->pace(0);   // no read left at the site: nothing more to pace by
    if (mode == 1) gs.g_rootInput[kButton] = 0;
    if (gs.g_rootInput[kButton] != 0) break;
    if (flush_redraw(ctx) != 0) break;
    if (gs.g_flag_83afcc != 0) break;
  }
  if (mode != 2) {
    gs.g_rootInput[15] = gs.gDirtyFlagB;
    gs.g_rootInput[9] = gs.gDirtyFlagA;
  }
  if (flush_redraw(ctx) != 0) {
    gs.g_rootInput[15] = take_key(ctx);
    gs.g_rootInput[9] = take_code(ctx);
  }
  return 1;
}

// wrapMessageBuffer(cols) (c3dv.cpp, which cites every instruction): breaks g_textBuffer into lines no wider than cols * 8
// as measured by Text_put on the current text object (f_stateptr_4c1570[0]) with the font `font` its caller's setDrawTarget
// selected: '^' becomes ' ' plus a line break after it, an existing LF counts, a span wider than the limit breaks at its last
// space (that byte becomes LF), '_' becomes ' '. Returns the number of breaks. The original measures a copy of the span on
// its stack; the port measures the span in place behind a temporary NUL (Text_put stops at the NUL: the same width).
// Needs the context's TextMetrics (MissingResource{"Text_put"} without it).
int32_t wrap_message_buffer(Ctx &ctx, int32_t cols, uint32_t font) {
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) throw MissingResource{"Text_put"};
  const Memory &m = ctx.mem;
  const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
  int32_t count = 0, S = 0, L = -1;
  uint32_t i = 0;
  if (vstrlen(m, buf) == 0) return 0;
  do {
    uint32_t p = buf + i;
    if (m.load<uint8_t>(p) == '^') {
      m.store<uint8_t>(p, ' ');
      S = i32(i) + 2;
      ++count;
      m.store<uint8_t>(buf + i + 1, 0x0a);
    }
    if (m.load<uint8_t>(p) == 0x0a) {
      ++count;
      S = i32(i) + 1;
    }
    if (i32(i) > S) {
      const uint8_t keep = m.load<uint8_t>(buf + i);
      m.store<uint8_t>(buf + i, 0);
      const int32_t w = tm->text_put(ctx, ctx.gs().f_stateptr_4c1570[0], font, buf + u32(S));
      m.store<uint8_t>(buf + i, keep);
      if (w > mul32(cols, 8) && L != -1) {
        i = u32(L);
        ++count;
        p = buf + i;
        L = -1;
        m.store<uint8_t>(p, 0x0a);
        S = i32(i) + 1;
      }
    }
    if (m.load<uint8_t>(p) == '_') m.store<uint8_t>(p, ' ');
    if (m.load<uint8_t>(p) == ' ') L = i32(i);
    ++i;
  } while (i < vstrlen(m, buf));
  return count;
}

// drawWindow's H (c3ei.cpp, drawWindow+0x1d..drawWindow+0x5e): 15 * wrapMessageBuffer(sel != -1 ? 0x25 : 0x2c) + 0x20 on the font its
// setDrawTarget selects (g_font51b360), at least 0x60 with a golfer selected (f_w_4c2e00[2] != -1)
// roadmap S2.11: the unclamped height 15 * r + 0x20 of the last call (drawWindow's text offset for h < 0x60 needs it; tick's
// drawWindow reads it through "commands.windowHeightRaw" for the graphics backend). No simulation state.
thread_local int32_t t_window_h_raw = 0;
int32_t window_height(Ctx &ctx) {
  const int32_t sel = ctx.gs().f_w_4c2e00[2];
  const int32_t r = wrap_message_buffer(ctx, sel != -1 ? 0x25 : 0x2c, state_va(ctx, &GameState::g_font51b360));
  int32_t h = add32(mul32(r, 15), 0x20);
  t_window_h_raw = h;
  if (sel != -1 && h < 0x60) h = 0x60;
  return h;
}

int32_t draw_window_effect(Ctx &ctx, int32_t mode, int32_t x, int32_t y) {
  const int32_t sel = ctx.gs().f_w_4c2e00[2];
  // phase 4: the window height H = 15 * wrapMessageBuffer(sel != -1 ? 0x25 : 0x2c) + 0x20 (at least 0x60 with a golfer) into
  // g_value587ea0, which mainLoop reads, and the line breaks in g_textBuffer; with no TextMetrics both stay dropped (U-0942)
  if (text_metrics(ctx)) {
    const int32_t r = wrap_message_buffer(ctx, sel != -1 ? 0x25 : 0x2c, state_va(ctx, &GameState::g_font51b360));
    const int32_t h = add32(mul32(r, 15), 0x20);
    ctx.gs().g_value587ea0 = sel != -1 && h < 0x60 ? 0x60 : h;
    // roadmap S3.7: an attached graphics backend draws the window (gfx_text.cpp drawWindow.frame; a[4] = h)
    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {
      const uint32_t d[5] = {u32(x), u32(y), 0, u32(mode), u32(h)};
      gfx_drop(ctx, "drawWindow.frame", d, 5);
    }
  }
  if (sel == -1 && mode == 0) call_optional(ctx, "playSound", {0x10b, 0x64, 0, 0, 0});
  if (sel >= 0) call_peer(ctx, "composeGolferSprite", {u32(sel)});
  return 0;
}

// roadmap S2.11: drawWindow(x, y, p3, mode)'s drawing for an attached graphics backend (gfx_text.cpp drawWindow.frame since
// roadmap S3.13), after window_height stored H; the unclamped height in the fifth slot. Draws nothing into the GameState.
// Used by draw_window_gfx_saved only: tickMessage and the help windows draw through draw_window_effect (roadmap S3.7).
void draw_window_gfx(Ctx &ctx, int32_t x, int32_t y, int32_t mode) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0) return;
  const uint32_t v[5] = {u32(x), u32(y), 0, u32(mode), u32(text_metrics(ctx) ? t_window_h_raw : 0)};
  gfx_drop(ctx, "drawWindow.frame", v, 5);
}

// roadmap S2.11: drawWindow(x, y, p3, mode)'s drawing for callers whose simulation part leaves drawWindow out (narrateShot):
// the window-height pass (wrapMessageBuffer's line breaks in g_textBuffer, H into g_value587ea0) runs as the original's
// drawWindow runs it, then the drawing; the text buffers (gfxr::ScreenDraw::text) and g_value587ea0 are put back after, so
// the GameState equals a run without a backend. Without a backend or a TextMetrics provider nothing happens.
void draw_window_gfx_saved(Ctx &ctx, int32_t x, int32_t y, int32_t mode) {
  gfxr::ScreenDraw d(ctx);
  if (!d.on || !text_metrics(ctx)) return;
  const int32_t saved_h = ctx.gs().g_value587ea0;
  d.text([&] { ctx.gs().g_value587ea0 = window_height(ctx); }, [&](uint32_t) { draw_window_gfx(ctx, x, y, mode); });
  ctx.gs().g_value587ea0 = saved_h;
}

}  // namespace cm

namespace {
using namespace cm;
#include "commands_panels.inc"
#include "commands_layout.inc"
#include "commands_screens.inc"
#include "commands_popup.inc"
#include "commands_keys.inc"
#include "commands_menus.inc"
}  // namespace

// ------------------------------------------------------------------------------------------------ apply_command
int32_t apply_command(Ctx &ctx, const Command &cmd) {
  switch (cmd.kind) {
    case CmdKind::None: return 0;
    case CmdKind::RootHandler: return root_handler(ctx, cmd);
    case CmdKind::GolferPanelAction: return golfer_panel_action(ctx, cmd.arg[0]);
    case CmdKind::EmployeeAction: return employee_action(ctx, cmd.arg[0]);
    case CmdKind::EmployeeViewSelect: return employee_view_action(ctx, cmd.arg[0]);
    case CmdKind::BuildToolSelect: return build_tool_action(ctx, cmd.arg[0]);
    case CmdKind::BuildingSelect: return building_action(ctx, cmd.arg[0]);
    case CmdKind::AmenitySelect: return amenity_action(ctx, cmd.arg[0]);
    case CmdKind::SlotPanelSelect: return slot_action(ctx, cmd.arg[0]);
    default:
      if (is_semantic_input(cmd.kind)) return semantic_apply(ctx, cmd);  // phase 3.8 (38i): input actions 0x200..
      throw MissingResource{"command kind not implemented"};
  }
}

void apply_commands(Ctx &ctx, const std::vector<Command> &cmds) {
  for (const Command &c : cmds) apply_command(ctx, c);
}

namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

Command raw_from(const uint32_t *w) {
  Command cmd;
  cmd.kind = CmdKind::RootHandler;
  cmd.handler = w[0];
  cmd.a0 = static_cast<int32_t>(w[1]);
  cmd.a1 = static_cast<int32_t>(w[2]);
  cmd.async_ctrl = w[3];
  cmd.async_shift = w[4];
  cmd.t = w[5];
  return cmd;
}

// test entry: one semantic command (kind, arg0)
void a_semantic(Ctx &c, const uint32_t *a, Ret &r) {
  Command cmd;
  cmd.kind = static_cast<CmdKind>(a[0]);
  cmd.arg[0] = static_cast<int32_t>(a[1]);
  r.set_i32(apply_command(c, cmd));
}
// test entry: one RootHandler command (handler, a0, a1, async_ctrl, async_shift, t)
void a_root_handler(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(apply_command(c, raw_from(a))); }

// test entries: attach a QueueInput of n raw commands (6 dwords each) read from the address space at va; report how many
// were consumed; detach
std::map<const Ctx *, std::unique_ptr<QueueInput>> &owned_queues() {
  static std::map<const Ctx *, std::unique_ptr<QueueInput>> m;
  return m;
}
void a_set_input(Ctx &c, const uint32_t *a, Ret &) {
  std::vector<Command> cmds;
  for (uint32_t k = 0; k < a[1]; ++k) {
    uint32_t w[6];
    for (uint32_t j = 0; j < 6; ++j) w[j] = c.mem.load<uint32_t>(a[0] + 24 * k + 4 * j);
    cmds.push_back(raw_from(w));
  }
  auto q = std::make_unique<QueueInput>(std::move(cmds));
  set_input_source(c, q.get());
  owned_queues()[&c] = std::move(q);
}
// test entries: record calls of the peer with this FNV-1a name hash into the log at va (cap records); clear all
void a_record_peer(Ctx &c, const uint32_t *a, Ret &) {
  PeerLog &l = cm::peer_logs()[&c];
  l.va = a[1];
  l.cap = a[2];
  l.names.push_back(a[0]);
}
void a_record_clear(Ctx &c, const uint32_t *, Ret &) { cm::peer_logs().erase(&c); }
void a_set_cursor(Ctx &c, const uint32_t *a, Ret &) {
  auto &q = owned_queues()[&c];
  if (!q) {
    q = std::make_unique<QueueInput>(std::vector<Command>{});
    set_input_source(c, q.get());
  }
  q->set_cursor(static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
}
void a_input_consumed(Ctx &c, const uint32_t *, Ret &r) {
  const auto it = owned_queues().find(&c);
  r.set_u32(it == owned_queues().end() ? 0u : static_cast<uint32_t>(it->second->consumed()));
}
void a_clear_input(Ctx &c, const uint32_t *, Ret &) {
  set_input_source(c, nullptr);
  owned_queues().erase(&c);
}

// test provider (commands.setTextWidth): Text_put's width as a fixed function of the text bytes, sum over the bytes c of
// base + (c & mask). The oracle stubs the original's Text_put with the same function, so both sides measure alike; what
// the test judges is everything the widths feed (U-0944).
class ScriptedWidths : public TextMetrics {
 public:
  ScriptedWidths(int32_t base, uint32_t mask) : base_(base), mask_(mask) {}
  int32_t text_put(Ctx &ctx, uint32_t, uint32_t, uint32_t text) override {
    int32_t w = 0;
    for (uint32_t p = text;; ++p) {
      const uint8_t c = ctx.mem.load<uint8_t>(p);
      if (c == 0) return w;
      w = add32(w, add32(base_, i32(c & mask_)));
    }
  }

 private:
  int32_t base_;
  uint32_t mask_;
};
std::map<const Ctx *, std::unique_ptr<ScriptedWidths>> &owned_metrics() {
  static std::map<const Ctx *, std::unique_ptr<ScriptedWidths>> m;
  return m;
}
void a_set_text_width(Ctx &c, const uint32_t *a, Ret &) {
  auto tm = std::make_unique<ScriptedWidths>(static_cast<int32_t>(a[0]), a[1]);
  set_text_metrics(c, tm.get());
  owned_metrics()[&c] = std::move(tm);
}
void a_clear_text_width(Ctx &c, const uint32_t *, Ret &) {
  set_text_metrics(c, nullptr);
  owned_metrics().erase(&c);
}

#define XY(fn) [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(fn(c, S(a, 0), S(a, 1))); }
void a_pump_and_redraw(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(pump_and_redraw(c, S(a, 0), "pumpAndRedraw")); }
void a_flush_redraw(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(flush_redraw(c)); }

int g_negctl_none = 0;
const FuncDef kFuncs[] = {
    {"commands.rootHandler", "default", "int", 6, 0, a_root_handler, &g_negctl_root_handler},
    {"commands.semantic", "default", "int", 2, 0, a_semantic, &g_negctl_none},
    {"commands.setInput", "default", "void", 2, 0, a_set_input, &g_negctl_none},
    {"commands.inputConsumed", "default", "int", 0, 0, a_input_consumed, &g_negctl_none},
    {"commands.setCursor", "default", "void", 2, 0, a_set_cursor, &g_negctl_none},
    {"commands.recordPeer", "default", "void", 3, 0, a_record_peer, &g_negctl_none},
    {"commands.recordClear", "default", "void", 0, 0, a_record_clear, &g_negctl_none},
    {"commands.clearInput", "default", "void", 0, 0, a_clear_input, &g_negctl_none},
    {"commands.setTextWidth", "default", "void", 2, 0, a_set_text_width, &g_negctl_none},
    {"commands.clearTextWidth", "default", "void", 0, 0, a_clear_text_width, &g_negctl_none},
    {"showPopup", "default", "int", 5, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(show_popup(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3), S(a, 4))); },
     &g_negctl_popup},
    {"panelListSelect", "default", "int", 2, 0, XY(panel_list_select), &g_negctl_panel_list},
    {"nearestMenuSpot", "default", "int", 2, 0, XY(nearest_menu_spot), &g_negctl_menus},
    {"showSystemMenu", "default", "int", 2, 0, XY(show_system_menu), &g_negctl_menus},
    {"showPreferencesMenu", "default", "int", 0, 0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(show_preferences_menu(c)); },
     &g_negctl_menus},
    {"drawScoreSummary", "default", "void", 0, 0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(draw_score_summary(c)); },
     &g_negctl_menus},
    {"textInputBox", "default", "int", 4, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(text_input_box(c, a[0], S(a, 1), S(a, 2), S(a, 3))); },
     &g_negctl_text_input},
    {"pumpAndRedraw", "default", "int", 1, 0, a_pump_and_redraw, &g_negctl_pump},
    // phase 4 (internal): drawWindow's window height with its wrapMessageBuffer pass, for tick's drawWindow
    {"commands.windowHeight", "default", "int", 0, 0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(cm::window_height(c)); }, nullptr},
    // roadmap S2.11 (internal): drawWindow(x, y, .., mode)'s drawing with the window-height pass put back (narrateShot)
    {"commands.drawWindowGfx", "default", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { cm::draw_window_gfx_saved(c, S(a, 0), S(a, 1), S(a, 2)); r.set_i32(0); }, nullptr},
    // roadmap S2.11 (internal): the unclamped height of the last commands.windowHeight call
    {"commands.windowHeightRaw", "default", "int", 0, 0, [](Ctx &, const uint32_t *, Ret &r) { r.set_i32(cm::t_window_h_raw); }, nullptr},
    {"flushRedraw", "default", "int", 0, 0, a_flush_redraw, &g_negctl_none},
    {"hitGolferPanel435f00", "default", "int", 2, 0, XY(hit_golfer_panel), &g_negctl_hit_golfer_panel},
    {"hitEmployeeSlot435570", "default", "int", 2, 0, XY(hit_employee_slot), &g_negctl_hit_employee_slot},
    {"hitEmployee436b00", "default", "int", 2, 0, XY(hit_employee), &g_negctl_hit_employee},
    {"hitTestSlot432f90", "default", "int", 2, 0, XY(hit_test_slot), &g_negctl_hit_test_slot},
    {"pickAmenity434980", "default", "int", 2, 0, XY(pick_amenity), &g_negctl_pick_amenity},
    {"hitBuildTool433c60", "default", "int", 2, 0, XY(hit_build_tool), &g_negctl_hit_build_tool},
    {"hitBuildingTool", "default", "int", 2, 0, XY(hit_building_tool), &g_negctl_hit_building_tool},
    {"handleEmployeeAction", "default", "int", 2, 0, XY(handle_employee_action), &g_negctl_employee_action},
    {"clickEmployeeView", "default", "int", 2, 0, XY(click_employee_view), &g_negctl_employee_view},
    {"clickGolferPanel", "default", "int", 2, 0, XY(click_golfer_panel), &g_negctl_golfer_panel},
    {"clickBuildTool433d30", "default", "int", 2, 0, XY(click_build_tool), &g_negctl_build_tool},
    {"pickBuilding", "default", "int", 2, 0, XY(pick_building), &g_negctl_pick_building},
    {"clickAmenityPanel", "default", "int", 2, 0, XY(click_amenity_panel), &g_negctl_amenity_panel},
    {"clickSlotPanel", "default", "int", 2, 0, XY(click_slot_panel), &g_negctl_slot_panel},
    {"drawAmenitiesPanel", "default", "void", 2, 0, XY(draw_amenities_panel), &g_negctl_amenities_panel},
    {"showInfoCursorPanel", "default", "int", 2, 0, XY(show_info_cursor_panel), &g_negctl_amenities_panel},
    {"drawEmployeeDetailPanel", "default", "int", 2, 0, XY(draw_employee_detail_panel), &g_negctl_detail_panel},
    {"drawEmployeeViewPanel", "default", "int", 2, 0, XY(draw_employee_view_panel), &g_negctl_view_panel},
    {"drawGolferActionPanel", "default", "int", 2, 0, XY(draw_golfer_action_panel), &g_negctl_detail_panel},
    {"tickMessage", "default", "int", 0, 0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(tick_message(c)); },
     &g_negctl_tick_message},
#define V0(fn) [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(fn(c)); }
    {"showBestScoresScreen", "default", "void", 0, 0, V0(show_best_scores_screen), &g_negctl_reports},
    {"showHistographScreen", "default", "void", 0, 0, V0(show_histograph_screen), &g_negctl_reports},
    {"showFinancialReport", "default", "void", 0, 0, V0(show_financial_report), &g_negctl_reports},
    {"showPlayerCommentsReport", "default", "void", 0, 0, V0(show_player_comments_report), &g_negctl_reports},
    {"showHireEmployeeDialog", "default", "int", 0, 0, V0(show_hire_employee_dialog), &g_negctl_hire_dialog},
    {"showCreditsScreen", "default", "void", 0, 0, V0(show_credits_screen), &g_negctl_credits},
    {"selectDifficultyScreen", "default", "int", 0, 0, V0(select_difficulty_screen), &g_negctl_difficulty},
    {"showLoadingScreen", "default", "int", 1, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(show_loading_screen(c, static_cast<int32_t>(a[0]))); },
     &g_negctl_reports},
    {"pumpLight", "default", "int", 0, 0, [](Ctx &c, const uint32_t *, Ret &r) { pump_light(c, "pumpLight"); r.set_i32(0); },
     &g_negctl_none},
#undef V0
};
#undef XY

const DefectDef kDefects[] = {
    {"showFinancialReport.sumWriteBack", "model_defects#24",
     "the report writes each drawn year row's column sum back into the row's entry 8 (+0x10)", &g_defect_finance_sum_write_back},
};
}  // namespace

extern const ModuleDef kModule_commands = {"commands", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                           sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
