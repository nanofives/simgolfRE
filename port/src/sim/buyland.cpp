// Module `buyland` (roadmap S2.10): showBuyLandScreen, the TRACTS FOR SALE screen the land popup's first option opens (mainLoop's
// frame shows the popup when bit 0 of g_eventFlags567a1c is set). Written from the behaviour of the C3 hook transcription
// (shim/src/re/c3fr.cpp ShowBuyLand_re) as reading reference; the emulator oracle (port/test/test_buyland.py, SG_EMU_ORACLE=1)
// judges it against the original. Functions of the original are cited by name, never by address.
//
// THE SCREEN. Nine lots, a 3 x 3 grid of 16 x 16 tile blocks of the 50 x 50 map g_tileType: lot n covers the rows
// (n % 3) * 16 + 1 .. + 16 and the columns (n / 3) * 16 + 1 .. + 16. A tile whose type byte is 0x14 is for sale; buying the lot
// turns each such tile into the type g_tileTypeSaved holds for it. Every redraw pass scans every lot:
//   * per tile for sale: its count, the count of its g_tileTypeSaved type (a per-type array of 23 counters in the original's
//     frame, indexed with the SIGNED type byte, so a negative type counts into the frame words below the array: the lot's
//     costs and the last lot's row number, which the port keeps as one array like the original's frame), one Random::range(3)
//     draw on g_rng, and when that draw's low word is 0 the sum grows by 1 << (g_landPurchaseCount & 31) (the sum starts at 5);
//   * a lot with tiles for sale is worth sum * 20 / 100, a lot without is worth the sum (5): on the FIRST pass only, cost[lot]
//     = worth * 10; a lot without tiles for sale is still priced (50 with no draw, the c3fr finding);
//   * the texts go into g_textBuffer (the lot line, its count / 10 in acres, its three most common types by the counters, the
//     price through appendNumber), the cash label through appendNumber(cash * 100): appendNumber writes g_textBuf as well.
// Before the lots the pass draws the map through showRoutingMapScreen with g_map822b80[0] = 1 (its draw-only mode) and the
// hole overlay (per hole with a par byte: cellAt for the tee and green sprites into g_map822b80[1], their mapToScreen points
// and the hole number through the CRT's itoa into g_ui_821c50 and g_textBuffer).
// THE LOOP. The selection starts at -1 (no lot); every pass reads the cursor (View_toLocal), finds the region under it (lots
// 0..8 in three columns of three 68-row bands from y 0x36, -2 the done button, -1 none) and redraws when it changed;
// otherwise a Blocking pump (pumpMessages) and the wait (waitTicks(1, 0), pacing), until the button word is set. Then:
// a left click on no region plays sound 0x18 and goes on; a right click returns 0; the done button returns 0; otherwise c =
// the frame word at index 1 + region (cost[region]; region -1 with a button word other than 1 or 2 reads the last lot's row
// number), c == 0 goes on, canAfford(c) == 0 redraws, else the purchase: cash -= price, the year's finance record (g_year %
// 100, signed) loses it, the lot's tiles for sale take their saved types, logTick(0x140, lot), and the result is 1.
//
// Drawing (Image_draw, the map's diamonds, lines, sprites and texts) runs only with a graphics backend attached
// (g_gfx_attached), through gfx_drop with the original's callee names and arguments (bodies: gfx.cpp, gfx_prim_text.cpp,
// gfx_reports.cpp, gfx_buyland.cpp); cellAt and addSprite of the hole overlay are the original's calls: cellAt is a pure
// lookup whose result the overlay stores, addSprite writes only the render-class sprite queue and draws, so it is handed to
// the backend and not run (the screen oracle stubs both, as for every 3.7b screen).
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS   // the development log (SG_BUYLAND_LOG) uses fopen / getenv
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "portsim/arith.h"
#include "portsim/execonst.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/util.h"
#include "commands_internal.h"

namespace portsim {

// Negative controls (sg_negctl_set "showBuyLandScreen"): 0 is the correct body.
//   1 the first pass stores the worth, not worth * 10, as the cost      2 the bands of the hit test are 67 rows, not 68
//   3 the purchase leaves the tiles for sale alone                       4 no Random::range draw per tile for sale
//   5 logTick gets 0x141                                                  6 the worth of a lot is sum * 20 / 99
//   7 a right click buys like a left click                                8 the finance record of the year + 1 loses the price
int g_negctl_buy_land = 0;

namespace {

using cm::call_peer;
using cm::cat;
using cm::cpy;

constexpr int kButton = 4;   // g_rootInput slot of the button word (commands.cpp RootSlot)

int mode() { return SG_NEGCTL(g_negctl_buy_land); }

// development: SG_BUYLAND_LOG=<file> appends one line per loop step and the reason of a MissingResource
void dlog(const char *fmt, long long a = 0, long long b = 0, long long c = 0) {
  static FILE *fh = std::getenv("SG_BUYLAND_LOG") ? std::fopen(std::getenv("SG_BUYLAND_LOG"), "a") : nullptr;
  if (!fh) return;
  std::fprintf(fh, fmt, a, b, c);
  std::fputc(10, fh);
  std::fflush(fh);
}

// the static CRT's __itoa(v, buf, 10)
uint32_t itoa10(Ctx &ctx, int32_t v, uint32_t buf) {
  char tmp[16];
  int n = 0;
  uint32_t mag = v < 0 ? 0u - u32(v) : u32(v);
  do {
    tmp[n++] = static_cast<char>('0' + mag % 10);
    mag /= 10;
  } while (mag != 0);
  uint32_t p = buf;
  if (v < 0) ctx.mem.store<uint8_t>(p++, '-');
  while (n > 0) ctx.mem.store<uint8_t>(p++, static_cast<uint8_t>(tmp[--n]));
  ctx.mem.store<uint8_t>(p, 0);
  return buf;
}

// the region under (x, y): lots 0..8 (columns x < 0x106, 0x110 <= x < 0x20c, x >= 0x21a over 0xf <= x < 0x307, bands of 68
// rows from 0x36 to 0x102), -2 the done button (0x296 <= x < 0x2d6, 0x215 <= y < 0x255), -1 nothing
int32_t buy_hit(int32_t x, int32_t y) {
  if (x >= 0xf && x < 0x307 && y >= 0x36 && y < 0x102) {
    const int32_t band = div32(sub32(y, 0x36), mode() == 2 ? 67 : 68);
    if (x < 0x106) return band;
    if (x >= 0x21a) return add32(band, 6);
    if (x >= 0x110 && x < 0x20c) return add32(band, 3);
    return -1;
  }
  if (x >= 0x296 && x < 0x2d6 && y >= 0x215 && y < 0x255) return -2;
  return -1;
}

struct BuyLand {
  Ctx &ctx;
  GameState &gs;
  const Memory &m;
  const bool on;   // a graphics backend is attached: the drawing calls are made
  const uint32_t tb, itoa_buf, refs, root;
  // the original's frame words L58 .. Ldc as one array (see the header): [0] the last lot's row number, [1..9] cost[9],
  // [10..32] the per-type counters, [33..41] the lot numbers
  int32_t F[42] = {};
  int32_t label_x = 0, label_y = 0;   // L2c / L24 (uninitialised in the original until the first centre tile: drawing only)
  int32_t best = 0;                   // L30, kept from lot to lot
  bool first = true;

  explicit BuyLand(Ctx &c)
      : ctx(c),
        gs(c.gs()),
        m(c.mem),
        on(g_gfx_attached.load(std::memory_order_relaxed) != 0),
        tb(state_va(c, &GameState::g_textBuffer)),
        itoa_buf(state_va(c, &GameState::g_ui_821c50)),
        refs(state_va(c, &GameState::g_spriteRef_8224c8)),
        root(state_va(c, &GameState::g_rootWindow) + 0x274) {}

  void gx(const char *name, std::initializer_list<uint32_t> a) const {
    if (on) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  }
  void image(uint32_t ref, int32_t x, int32_t y) const { gx("Image_draw", {ref, root, u32(x), u32(y), 0}); }
  void font(uint32_t f) const { gx("setDrawTarget", {root, f, 0, 0, 0}); }
  // the dword at byte offset off + 4 i of f_tab_layoutInts_4ba62c in the user's exe (.rdata). The lot rectangles' y words of
  // lots 3..8 lie past the descriptor's 105 entries (its count is low, U-1290): read from the file's bytes up to the next
  // descriptor (f_k_dbl_4ba800, 0x1d4 bytes after the table's start). Drawing only.
  int32_t layout_at(uint32_t off, int32_t i) const {
    uint32_t size = 0;
    const uint8_t *p = ctx.exe().bytes("f_tab_layoutInts_4ba62c", &size);
    const uint32_t o = off + 4u * u32(i);
    if (o + 4 > 0x1d4) throw MissingResource{"showBuyLandScreen: a layout word past the table"};
    int32_t v;
    std::memcpy(&v, p + o, 4);
    return v;
  }
  // mapToScreen456b70: x = 6 (a + b) + 0x6a, y = 3 (b - a) + 0x1b8 + (g_map822b80[0] != 0 ? -1 : 1)
  void map_to_screen(int32_t a, int32_t b, int32_t &x, int32_t &y) const {
    x = add32(mul32(add32(a, b), 6), 0x6a);
    y = add32(sub32(mul32(sub32(b, a), 3), gs.g_map822b80[0] != 0 ? 1 : -1), 0x1b8);
  }
  int32_t &frame(int32_t i) {
    if (i < 0 || i >= 42) throw MissingResource{"showBuyLandScreen: a frame word outside the modelled locals (a type byte "
                                                "outside -10 .. 31 or a region the frame does not hold)"};
    return F[i];
  }

  // the hole overlay of one pass: holes 1..18 with a non-zero par byte
  void holes() {
    const uint32_t hb = state_va(ctx, &GameState::g_holes), wb = state_va(ctx, &GameState::g_holePoints);
    const uint32_t cols = state_va(ctx, &GameState::g_sheetCols);
    for (int32_t h = 1; h <= 18; ++h) {
      const uint32_t rec = hb + 0x208u * u32(h), way = wb + 0x18u * u32(h);
      const int8_t par = m.load<int8_t>(rec);
      if (par == 0) continue;
      int32_t sx = 0, sy = 0;
      map_to_screen(m.load<int32_t>(rec + 8), m.load<int32_t>(rec + 0xc), sx, sy);
      gx("setLinePoint", {u32(sx), u32(sy)});
      const int32_t k = clamp(sub32(par, 3), 0, 2);
      uint32_t cell = call_peer(ctx, "cellAt", {u32(add32(k, 0x18d)), u32(sub32(m.load<int32_t>(cols + 4u * u32(0x18d + k)), 1)),
                                                0})
                          .eax;
      gs.g_map822b80[1] = static_cast<int32_t>(cell);
      gx("addSprite", {u32(sx), u32(sy), u32(sy), 0x28, 0x28, cell, 4, 0x5e});
      m.store<uint8_t>(tb, 0);
      cat(ctx, tb, itoa10(ctx, h, itoa_buf));
      const bool numbers = gs.g_ui_4d20dc != 0;
      const int32_t w2 = m.load<int32_t>(way + 0x10), w1 = m.load<int32_t>(way + 8);
      if (w2 != -1 || w1 != -1) {
        const uint32_t p = w2 != -1 ? way + 0x10 : way + 8;
        map_to_screen(sar32(m.load<int32_t>(p), 10), sar32(m.load<int32_t>(p + 4), 10), sx, sy);
        gx("lineTo", {u32(sx), u32(sy), 0x80007fffu, 2, 0xa});
        if (numbers) gx("drawCenteredText2", {tb, u32(sx), u32(sub32(sy, 4)), 0x80007ff0u});
      } else {
        const int32_t tx = sx, ty = sy;
        map_to_screen(m.load<int32_t>(rec + 0x18), m.load<int32_t>(rec + 0x1c), sx, sy);
        if (numbers) gx("drawCenteredText2", {tb, u32(div32(add32(sx, tx), 2)), u32(sub32(div32(add32(ty, sy), 2), 4)), 0x80007ff0u});
      }
      map_to_screen(m.load<int32_t>(rec + 0x18), m.load<int32_t>(rec + 0x1c), sx, sy);
      gx("lineTo", {u32(sx), u32(sy), 0x80007fffu, 2, 0xa});
      const int32_t b = gs.g_courseType;
      cell = call_peer(ctx, "cellAt", {u32(add32(b, 0x189)), 0, 3}).eax;
      gs.g_map822b80[1] = static_cast<int32_t>(cell);
      gx("addSprite", {u32(sx), u32(sy), u32(sy), 0x28, 0x28, cell, 2, u32(add32(b, 0x60))});
    }
  }

  // the lot's text: "Buy tract #<n>, <count / 10> acres of <three most common types>"
  void lot_text(int32_t lot, int32_t count) {
    const uint32_t p368c = state_va(ctx, &GameState::f_strpool_4d368c);
    const uint32_t comma = state_va(ctx, &GameState::f_strpool_4c528c) + 0x2c;
    const uint32_t attrs = state_va(ctx, &GameState::g_tileTypeAttrs);
    cpy(ctx, tb, p368c + 0xc);
    cat(ctx, tb, itoa10(ctx, add32(lot, 1), itoa_buf));
    cat(ctx, tb, comma);
    cat(ctx, tb, itoa10(ctx, div32(count, 10), itoa_buf));
    cat(ctx, tb, p368c + 4);
    cat(ctx, tb, p368c);
    for (int32_t k = 0; k < 3; ++k) {
      int32_t most = 0;
      for (int32_t t = 0; t < 23; ++t)
        if (frame(10 + t) > most) {
          best = t;
          most = frame(10 + t);
        }
      if (k == 1) cat(ctx, tb, comma);
      else if (k == 2) cat(ctx, tb, state_va(ctx, &GameState::f_strpool_4c7818));
      const uint32_t rec = attrs + 0x30u * u32(best);
      const uint32_t name = m.load<uint8_t>(rec + 0x26) == 0xd ? rec + 0x10 : rec;
      frame(10 + best) = 0;
      cat(ctx, tb, name);
    }
  }

  // one redraw pass (the original's block under its redraw flag)
  void pass(int32_t sel) {
    image(refs + 0x2ec, 0, 0);
    if (on && sel >= 0) image(refs + 0x630, layout_at(0x174, sel), layout_at(0x198, sel));
    gs.g_map822b80[0] = 1;
    call_peer(ctx, "showRoutingMapScreen", {});
    gs.g_map822b80[0] = 0;
    font(state_va(ctx, &GameState::g_fontRef_821ee8));
    holes();
    font(state_va(ctx, &GameState::g_fontRef_821020));
    const uint32_t p36bc = state_va(ctx, &GameState::f_strpool_4d36bc);
    gx("drawCenteredText", {p36bc + 0x10, 0x190, 0x10, 0x80000000u});
    font(state_va(ctx, &GameState::g_fontRef_821ee8));
    gx("drawText", {root, p36bc, 0x224, 0x107, vstrlen(m, p36bc)});
    cpy(ctx, tb, state_va(ctx, &GameState::f_strpool_4c61dc) + 4);
    call_peer(ctx, "appendNumber", {u32(gs.g_cash_div100) * 100u});
    gx("drawCenteredText", {tb, 0x2d0, 0x108, 0x80000000u});
    gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});

    const uint32_t tiles = state_va(ctx, &GameState::g_tileType), types = state_va(ctx, &GameState::g_tileTypeSaved);
    const int32_t shift = gs.g_landPurchaseCount & 31;
    for (int32_t lot = 0; lot < 9; ++lot) {
      const uint32_t img = refs + 0x318 + 0x2cu * u32(lot);
      const int32_t q = lot / 3, r = lot % 3;
      F[0] = r;
      int32_t sum = 5, count = 0;
      const int32_t row0 = r * 16 + 1, col0 = q * 16 + 1;
      for (int32_t t = 0; t < 23; ++t) F[10 + t] = 0;
      for (int32_t j = 0; j < 16; ++j)
        for (int32_t i = 0; i < 16; ++i) {
          const int32_t row = i + row0, col = j + col0;
          const uint32_t idx = u32(row * 50 + col);
          if (m.load<int8_t>(tiles + idx) != 0x14) continue;
          ++count;
          ++frame(10 + m.load<int8_t>(types + idx));
          if (mode() != 4) {
            const int32_t rnd = cm::rng_range(ctx, 3);
            if ((rnd & 0xffff) == 0) sum = add32(sum, static_cast<int32_t>(1u << shift));
          }
          int32_t mx = 0, my = 0;
          map_to_screen(row, col, mx, my);
          if (i == 8 && j == 8) {
            label_x = mx;
            label_y = my;
          }
        }
      int32_t value;
      if (count != 0) {
        const int32_t c = div32(mul32(sum, 20), mode() == 6 ? 99 : 100);
        if (on) lot_label(c, lot);
        value = c;
      } else {
        value = sum;
      }
      label_x = add32(q << 8, lot >= 6 ? 0x58 : 0x4e);
      label_y = add32(mul32(r, 68), 0x3e);
      if (first) F[1 + lot] = mode() == 1 ? value : mul32(value, 10);
      F[33 + lot] = lot;
      if (count == 0) {
        if (on) image(img + 0x18c, layout_at(0x12c, lot), layout_at(0x150, lot));
        cpy(ctx, tb, state_va(ctx, &GameState::f_strpool_4d368c) + 0x18);
        gx("drawTextWithColor", {tb, u32(add32(label_x, 4)), u32(add32(label_y, 2)), 0x80002108u});
        continue;
      }
      lot_text(lot, count);
      gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
      gx("drawTextLinesAt", {root, tb, u32(label_x), u32(label_y), 0xa5});
      label_y = add32(label_y, mul32(font_height(), 2));
      cpy(ctx, tb, state_va(ctx, &GameState::f_strpool_4d3680));
      call_peer(ctx, "appendNumber", {u32(F[1 + lot]) * 100u});
      gx("drawRight", {root, tb, u32(add32(label_x, 0xa5)), u32(label_y), 0x80000000u, vstrlen(m, tb)});
      if (on && sel == lot) image(img, layout_at(0x12c, lot), layout_at(0x150, lot));
    }
    cat(ctx, tb, state_va(ctx, &GameState::f_strpool_4d35c8) + 0x94);
    image(refs + 0x688, 0x47, 0x215);
    if (sel == -2) image(refs + 0x65c, 0x296, 0x215);
    gx("Window::refreshFull", {state_va(ctx, &GameState::g_rootWindow), 0});
    first = false;
  }

  // the font the root surface holds at the label (setDrawTarget's last font, g_fontRef_821ee8): its +8 < 0 ? +0xc : +0x10 + +8
  int32_t font_height() const {
    const uint32_t f = state_va(ctx, &GameState::g_fontRef_821ee8);
    const int32_t a = m.load<int32_t>(f + 8);
    return a < 0 ? m.load<int32_t>(f + 0xc) : add32(m.load<int32_t>(f + 0x10), a);
  }

  // the lot number on the map: the dword c with its low word replaced by f_str_4d36b8's word and its low byte raised by the
  // lot number, read as a string (with the lot number's dword behind it): the label is drawn from the text buffer, whose
  // bytes are put back after the call
  void lot_label(int32_t c, int32_t lot) {
    uint8_t lbl[9] = {};
    const uint32_t lw = (u32(c) & 0xffff0000u) | m.load<uint16_t>(state_va(ctx, &GameState::f_str_4d36b8));
    std::memcpy(lbl, &lw, 4);
    lbl[0] = static_cast<uint8_t>(lbl[0] + lot);
    std::memcpy(lbl + 4, &lot, 4);
    const size_t n = strnlen(reinterpret_cast<const char *>(lbl), 8);
    uint8_t keep[9];
    for (uint32_t i = 0; i < 9; ++i) keep[i] = m.load<uint8_t>(tb + i);
    for (uint32_t i = 0; i < 9; ++i) m.store<uint8_t>(tb + i, i < n ? lbl[i] : 0);
    gx("drawStringAt", {root, state_va(ctx, &GameState::g_fontRef_821f28), tb, u32(sub32(label_x, 7)), u32(sub32(label_y, 4)),
                        static_cast<uint32_t>(n)});
    for (uint32_t i = 0; i < 9; ++i) m.store<uint8_t>(tb + i, keep[i]);
  }

  int32_t run() {
    bool redraw = true;
    int32_t sel = -1, hit = 0;
    InputSource *src = input_source(ctx);
    for (;;) {
      if (redraw) {
        redraw = false;
        pass(sel);
      }
      hit = sel;
      bool changed = false;
      for (;;) {
        const int32_t prev = hit;
        int32_t x = 0, y = 0;
        if (!src || !src->cursor(x, y)) throw NeedInput{{"showBuyLandScreen"}};
        hit = buy_hit(x, y);
        dlog("cursor %lld %lld hit %lld", x, y, hit);
        if (prev != hit) {
          changed = true;
          break;
        }
        cm::pump_messages(ctx, PumpKind::Blocking, "showBuyLandScreen");
        if (gs.g_rootInput[kButton] != 0) break;
      }
      if (changed) {
        sel = hit;
        redraw = true;
        continue;
      }
      const int32_t click = gs.g_rootInput[kButton];
      dlog("click %lld hit %lld", click, hit);
      sel = hit;
      int32_t s;
      if (click == 1 || (mode() == 7 && click == 2)) {
        if (hit == -1) {
          cm::call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
          continue;
        }
        s = hit;
      } else {
        if (click == 2) return 0;
        s = sel;
      }
      if (s == -2) return 0;
      const int32_t c = frame(1 + s);
      if (c == 0) continue;
      if (call_peer(ctx, "canAfford", {u32(c)}).eax == 0) {
        redraw = true;
        continue;
      }
      // the purchase
      const int32_t price = frame(1 + sel);
      gs.g_cash_div100 = sub32(gs.g_cash_div100, price);
      const int32_t yr = add32(static_cast<int16_t>(gs.g_year) % 100, mode() == 8 ? 1 : 0);
      const uint32_t fin = state_va(ctx, &GameState::g_financeYears) + 0xe + u32(mul32(yr, 20));
      m.store<int16_t>(fin, static_cast<int16_t>(m.load<int16_t>(fin) - static_cast<int16_t>(price)));
      const int32_t r0 = add32(mul32(sel % 3, 16), 1), c0 = add32(mul32(sel / 3, 16), 1);
      const uint32_t tiles = state_va(ctx, &GameState::g_tileType), types = state_va(ctx, &GameState::g_tileTypeSaved);
      if (mode() != 3)
        for (int32_t c2 = 0; c2 < 16; ++c2)
          for (int32_t k = 0; k < 16; ++k) {
            const uint32_t idx = u32(add32(add32(mul32(add32(r0, k), 50), c2), c0));
            if (m.load<int8_t>(tiles + idx) == 0x14) m.store<int8_t>(tiles + idx, m.load<int8_t>(types + idx));
          }
      call_peer(ctx, "logTick", {mode() == 5 ? 0x141u : 0x140u, u32(sel)});
      return 1;
    }
  }
};

// syncTiles (c3ey.cpp SyncTiles_re), the frame's first call after a purchase (mainLoop's land popup): per column x = 49 .. 0 the
// input pump (TerrainApi::pump_input, as rebuildTerrainGrid), then per y = 49 .. 0 the Terrain tile (x, y) (the exe's inline
// Terrain::tileAt: no NULL test, the 50 x 50 grid always holds it) gets Terrain::setType(tile, tileType, tileByte) when its type
// field +0x24 differs from tileType(x, y) or, failing that, Terrain::getVariation(tile) differs from tileByte(x, y); finally
// the byte g_terrain820f2a[0] = 1, which makes the next updateTerrainRender repaint from scratch (terrain_dll.cpp: that path
// leaves bit 1 of g_tileWallMask[0] alone, the incremental one sets it). Returns the last call's eax (setType's, or tileByte's).
// Negative control (sg_negctl_set "syncTiles"): 1 the byte is not set.
int g_negctl_sync_tiles = 0;
int32_t sync_tiles(Ctx &ctx) {
  TerrainApi &api = terrain_api(ctx);
  const Memory &m = ctx.mem;
  const uint32_t t = ctx.gs().gTerrain;
  int32_t r = 0;
  for (int32_t x = 49; x >= 0; --x) {
    api.pump_input();
    for (int32_t y = 49; y >= 0; --y) {
      const int32_t w = m.load<int32_t>(t + 0x14);
      const uint32_t tile =
          (x >= w || y >= m.load<int32_t>(t + 0x18)) ? 0u : t + 0x3a4 + u32(add32(x, mul32(y, w))) * 0x248u;
      const int32_t cur = m.load<int32_t>(tile + 0x24);
      r = i32(call_peer(ctx, "tileType", {u32(x), u32(y)}).eax);
      if (r == cur) {
        const int32_t v = i32(api.get_variation(t, tile));
        r = i32(call_peer(ctx, "tileByte", {u32(x), u32(y)}).eax);
        if (r == v) continue;
      }
      const int32_t var = i32(call_peer(ctx, "tileByte", {u32(x), u32(y)}).eax);
      const int32_t type = i32(call_peer(ctx, "tileType", {u32(x), u32(y)}).eax);
      r = i32(api.set_type(t, tile, type, var));
    }
  }
  if (SG_NEGCTL(g_negctl_sync_tiles) != 1) ctx.gs().g_terrain820f2a[0] = 1;
  return r;
}

const FuncDef kFuncs[] = {
    {"syncTiles", "default", "int", 0, 0, [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(sync_tiles(c)); },
     &g_negctl_sync_tiles},
    {"showBuyLandScreen", "default", "int", 0, 0,
     [](Ctx &c, const uint32_t *, Ret &r) {
       try {
         r.set_i32(BuyLand(c).run());
       } catch (const MissingResource &e) {
         dlog(e.what ? e.what : "?");
         throw;
       }
     },
     &g_negctl_buy_land},
};
}  // namespace

extern const ModuleDef kModule_buyland = {"buyland", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), nullptr, 0};

}  // namespace portsim
