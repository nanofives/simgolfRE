// showHoleStatsScreen (phase 3.8, stream 38e, U-0949): the statistics screen of one hole, opened by mainLoop when
// g_msgState53df54 is negative (showHoleStatsScreen(-v)). Specification: the C4 hook source (c3eo.cpp) and the original's
// instructions; notes log/p38/38e_notes.md; tests port/test/test_holestats.py (emulator oracle with a scripted clock,
// golden digests, negative controls, the segments championship/02 and cov_a_keys2/02).
//
// WHAT IS KEPT. The screen draws a page and then loops, redrawing the whole page whenever the page index
// (timeGetTime() >> 10) % (rounds != 0 ? 5 : 3) changes, until a click (the button word) or a key (flushRedraw). The
// drawing is dropped (Surface_drawSprite, Surface_stretchBlit, Widget_setQuad, drawStringAt, Widget_draw, Image_draw,
// Window::refreshFull, waitTicks, enterModal483d30: log/p3/p37a_simwrites.py finds no simulation write and no simulation
// address taken in any of their call closures). Every page build is kept: the text the page assembles in g_textBuffer
// and the itoa scratch g_ui_821c50, the peers appendHoleName / appendCents / appendRatingLabel (appendCents formats into
// the hashed g_textBuf) and, for the five most frequent comments, buildGolferComment(kind, word, hole * 11, 0x98),
// whose writes reach simulation objects (g_commentTone, g_thoughtOnPartner, golfer record 152). The number of page builds
// depends on the wall clock, so every clock read the original makes in this function is asked of the context's
// InputSource at its own call site (InputSource::clock; a trace-driven source answers with the trace's next record at
// that site). The page's numbers are drawing only; they are computed because their divisions are the original's #DE.
// The cursor read of each pass (View_toLocal, for the close button's hover image) is made when the display exists, as
// the frame step's View_toLocal makes it, so a trace-driven source's cursor count stays aligned with the original's;
// its value only selects a drawn image.
#include "portsim/sim/holestats.h"

#include <map>
#include <memory>
#include <vector>

#include "commands_internal.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   showHoleStatsScreen  1 = the page index from bit 9 of the clock (timeGetTime() >> 9): the page builds follow the wrong
//                            clock edges
//                        2 = g_msgState53df54 is left as it was on the way out
//                        3 = the histogram's last row collects the scores up to 8 instead of 9
int g_negctl_holestats = 0;

using cm::G;

enum : int { kButton = 4, kClicked = 7 };  // g_rootInput slots: the button word (+0x10), the click flag (+0x1c)

// the static CRT's _itoa for radix 10 (a '-' and the magnitude for a negative value), at buf; returns buf
uint32_t itoa10(Ctx &ctx, int32_t v, uint32_t buf) {
  char tmp[16];
  int k = 0;
  uint32_t u = u32(v);
  const bool neg = v < 0;
  if (neg) u = 0u - u;
  do {
    tmp[k++] = static_cast<char>('0' + u % 10u);
    u /= 10u;
  } while (u != 0);
  uint32_t p = buf;
  if (neg) ctx.mem.store<uint8_t>(p++, '-');
  while (k > 0) ctx.mem.store<uint8_t>(p++, static_cast<uint8_t>(tmp[--k]));
  ctx.mem.store<uint8_t>(p, 0);
  return buf;
}

struct HoleStats {
  Ctx &ctx;
  InputSource *src;
  const int32_t hole;
  const uint32_t rec;   // the hole's course record (g_holes + hole * 0x208)
  const uint32_t text;  // g_textBuffer
  const uint32_t num;   // the itoa scratch g_ui_821c50

  HoleStats(Ctx &c, int32_t h)
      : ctx(c),
        src(input_source(c)),
        hole(h),
        rec(GS(g_holes) + u32(h) * 0x208u),
        text(GS(g_textBuffer)),
        num(GS(g_ui_821c50)) {}

  int32_t r32(uint32_t off) { return G<int32_t>(ctx, rec + off); }
  int32_t r16(uint32_t off) { return G<int16_t>(ctx, rec + off); }
  void clear_text() { ctx.mem.store<uint8_t>(text, 0); }
  void cat_num(int32_t v) { cm::cat(ctx, text, itoa10(ctx, v, num)); }
  // a timeGetTime the original makes at `site`
  uint32_t clock(int32_t site) {
    uint32_t t = 0;
    if (!src || !src->clock(site, t)) throw NeedInput{{"showHoleStatsScreen"}};
    return t;
  }
  uint32_t page_of(uint32_t t, int32_t div) {
    return (t >> (SG_NEGCTL(g_negctl_holestats) == 1 ? 9 : 10)) % u32(div);  // the original's unsigned `div`
  }
  // View_toLocal(root window, &x, &y) of a pass: the display's cursor when there is a display
  void cursor_read() {
    if (ctx.gs().g_graphicsDevice == 0) return;
    int32_t x = 0, y = 0;
    if (src) src->cursor(x, y);
  }
  // pumpMessages in the input pass: the batch that has arrived at this pump call (a Blocking pump of a modal loop; the
  // loop keeps passing while nothing arrives, so an exhausted source is not an error here: the clock ends the loop)
  void pump() {
    GameState &gs = ctx.gs();
    if (gs.g_rootInput[kClicked] == 0) gs.g_rootInput[kButton] = 0;
    std::vector<Command> b;
    if (src && src->batch(PumpKind::Blocking, b))
      for (const Command &c : b) apply_command(ctx, c);
    gs.g_rootInput[kClicked] = 0;
  }

  // one page build (the original's redraw block); returns the local round count the input pass divides by
  int32_t build(uint32_t page) {
    const uint32_t pool = GS(f_strpool_4d317c);
    int32_t rounds = r32(0x20);
    cm::cpy(ctx, text, pool + 0xf0);
    cm::call_peer(ctx, "appendHoleName", {u32(hole)});
    if ((G<uint8_t>(ctx, rec + 0x200) & 0x81) != 0) {
      cm::cat(ctx, text, GS(f_strpool_4c4970) + 4);
      cat_num(hole);
      cm::cat(ctx, text, GS(f_strpool_4c59e0));
    }
    cm::cpy(ctx, text, GS(f_strpool_4c8808) + 0xe8);
    clear_text();
    cat_num(r16(4));
    if (r32(0x20) != 0) {
      // one round off for every golfer on this hole (record byte +0x21) whose next byte is above 0
      const uint32_t g0 = GS(g_golfers);
      for (uint32_t p = g0 + 0x22; p < g0 + 0x9822; p += 0x100) {
        if (G<int8_t>(ctx, p - 1) != hole) continue;
        if (G<int8_t>(ctx, p) <= 0) continue;
        rounds = sub32(rounds, 1);
      }
      if (rounds == 0) rounds = 1;
      if (page <= 4) {  // the five value lines; a page above 4 shows none
        int32_t value = 0;
        uint32_t label = 0, suffix = 0;
        bool cents = false;
        switch (page) {
          case 0:
            label = pool + 0xe0;
            value = div32(r16(0x15c), r32(0x20));
            suffix = GS(f_strpool_4c60c8) + 4;
            break;
          case 1:
            label = pool + 0xd0;
            value = r16(0x166);
            suffix = GS(f_strpool_4c60c8) + 4;
            break;
          case 2:
            label = pool + 0xc0;
            value = div32(mul32(r16(0x15e), 100), r32(0x20));
            suffix = GS(f_strpool_4c60c4);
            break;
          case 3:
            label = pool + 0xb0;
            value = div32(mul32(r16(0x160), 100), rounds);
            suffix = GS(f_strpool_4c60c4);
            break;
          default:
            label = pool + 0xa0;
            value = div32(mul32(r16(0x162), 100), rounds);
            cents = true;
            break;
        }
        cm::cpy(ctx, text, label);
        clear_text();
        if (cents) cm::append_cents(ctx, value);
        else cat_num(value);
        if (suffix != 0) cm::cat(ctx, text, suffix);
      }
    }
    cm::cpy(ctx, text, GS(f_strpool_4c64a8) + 0x28);
    clear_text();
    cat_num(G<int8_t>(ctx, rec));
    // the score histogram: rows par - 2 (at least 1) .. + 5, the last row collecting every score up to 9; 8 columns of 11
    // words from record +0x28, each column's count starting at 8 and its weighted sum at par * 8
    const int32_t par = G<int8_t>(ctx, rec);
    int32_t total = 0, weighted = 0, col_w[8], col_c[8];
    for (int c = 0; c < 8; ++c) {
      col_w[c] = mul32(par, 8);
      col_c[c] = 8;
    }
    int32_t lo = sub32(par, 2);
    if (lo < 1) lo = 1;
    const int32_t hi = add32(lo, 5);
    const int32_t top = SG_NEGCTL(g_negctl_holestats) == 3 ? 8 : 9;
    for (int32_t score = lo; score <= hi; ++score) {
      clear_text();
      cat_num(score);
      if (score == hi) cm::cat(ctx, text, GS(f_strpool_4c5340));
      int32_t row = 0;
      for (int32_t col = 0; col < 8; ++col) {
        const int32_t last = score == hi ? top : score;
        for (int32_t s = score; s <= last; ++s) {
          const int32_t v = G<int16_t>(ctx, rec + 0x28 + 2 * u32(col * 11 + s));
          total = add32(total, v);
          row = add32(row, v);
          weighted = add32(weighted, mul32(v, s));
          col_c[col] = add32(col_c[col], v);
          col_w[col] = add32(col_w[col], mul32(v, s));
        }
      }
      clear_text();
      cat_num(row);
    }
    if (total != 0) {
      cm::cpy(ctx, text, pool + 0x74);
      clear_text();
      cm::append_cents(ctx, div32(mul32(weighted, 100), total));
    }
    // the rating line, when the record's dword +0x24 is non-zero: v = +0x158 * 100 / (+0x24 / 2 + rounds + 4) and the word
    // of v / 20 (0, 1, 2, more; a negative v has its own)
    if (r32(0x24) != 0) {
      const int32_t w = r32(0x24);
      const int32_t half = sar32(add32(w, w < 0 ? 1 : 0), 1);
      const int32_t v = div32(mul32(r16(0x158), 100), add32(add32(half, r32(0x20)), 4));
      clear_text();
      cat_num(v);
      cm::cat(ctx, text, pool + 0x70);
      uint32_t word;
      if (v < 0) {
        word = pool + 0x68;
      } else {
        // v / 20 (`imul 0x66666667; sar edx, 3`), not v / 10 as the hook source's comment and code have it (38e notes)
        const int32_t tier = v / 20;
        word = tier == 0 ? pool + 0x3c : tier == 1 ? pool + 0x44 : tier == 2 ? pool + 0x4c : pool + 0x58;
      }
      cm::cat(ctx, text, word);
    }
    // three columns' means against column 7's, each when both counts are non-zero
    if (col_c[7] != 0) {
      static const int kCmp[3] = {6, 5, 3};
      for (int c : kCmp) {
        if (col_c[c] == 0) continue;
        cm::cpy(ctx, text, GS(f_strpool_4c5340));
        const int32_t a = div32(mul32(col_w[c], 100), col_c[c]);
        const int32_t b = div32(mul32(col_w[7], 100), col_c[7]);
        cm::append_cents(ctx, sub32(a, b));
        cm::call_peer(ctx, "appendRatingLabel", {u32(ctx.gs().g_ui_569628)});
      }
    }
    // a par byte of 0 shows a banner whose text blinks with bit 9 of the clock: the read is the original's, its value
    // only decides the drawing
    if (G<uint8_t>(ctx, rec) == 0) (void)clock(kHsClockBanner);
    // the five most frequent comments (kinds below 0x32 of the 0x40 counters at +0xd8), each written by buildGolferComment
    // with the word at +0x16c + 2 kind (low 14 bits) and cleared from the local copy
    int32_t comment[0x40];
    for (uint32_t k = 0; k < 0x40; ++k) comment[k] = r16(0xd8 + 2 * k);
    int32_t best_k = 0;
    for (int round = 5; round != 0; --round) {
      int32_t best = 0;
      for (int32_t k = 0; k < 0x32; ++k)
        if (comment[k] > best) {
          best = comment[k];
          best_k = k;
        }
      if (best == 0) continue;
      if (r32(0x20) == 0) continue;
      const int32_t pct = div32(mul32(comment[best_k], 100), r32(0x20));
      clear_text();
      cat_num(pct);
      cm::cat(ctx, text, pool);
      cm::cat(ctx, text, GS(f_strpool_4c4e54));
      const uint32_t word = G<uint16_t>(ctx, rec + 0x16c + 2 * u32(best_k)) & 0x3fffu;
      cm::call_peer(ctx, "buildGolferComment", {u32(best_k), word, u32(mul32(hole, 11)), 0x98});
      cm::cat(ctx, text, GS(f_strpool_4c4e54));
      comment[best_k] = 0;
    }
    return rounds;
  }

  void run() {
    const int32_t div = r32(0x20) != 0 ? 5 : 3;
    uint32_t page = page_of(clock(kHsClockEntry), div);
    for (;;) {
      const int32_t rounds = build(page);  // after the first build: waitTicks(10, 0) and enterModal483d30 (dropped)
      const int32_t div2 = rounds != 0 ? 5 : 3;
      cursor_read();  // waitTicks(1, 0) (dropped), View_toLocal
      bool redraw = false;
      if (page != page_of(clock(kHsClockAfterDraw), div2)) {
        page = page_of(clock(kHsClockNewPage), div2);
        continue;
      }
      for (;;) {
        pump();
        if (ctx.gs().g_rootInput[kButton] != 0) break;
        if (cm::flush_redraw(ctx) != 0) break;
        cursor_read();  // waitTicks(1, 0) (dropped), View_toLocal
        if (page != page_of(clock(kHsClockPass), div2)) {
          page = page_of(clock(kHsClockNewPage), div2);
          redraw = true;
          break;
        }
      }
      if (!redraw) break;
    }
    if (SG_NEGCTL(g_negctl_holestats) != 2) ctx.gs().g_msgState53df54 = 0;
    cm::pump_light(ctx, "showHoleStatsScreen");
  }
};

// ---- test entries: a scripted InputSource with a clock (port/test/test_holestats.py) ----
// The commands come in batches (each up to and including the first press or key, as QueueInput's); batch k arrives at
// clock value arrive[k]: a Blocking pump delivers the next batch once the last clock value the source returned is at
// least its arrival, a Drain pump nothing. The clock is a per-site script (QueueInput::set_clock); the cursor is fixed.
// cmdemu.Emu.run(clock=, gate=) gives the original the same script and the same arrival rule.
class TimedInput : public InputSource {
 public:
  TimedInput(std::vector<std::vector<Command>> batches, std::vector<uint32_t> arrive, int32_t cx, int32_t cy)
      : q_({}), batches_(std::move(batches)), arrive_(std::move(arrive)), cx_(cx), cy_(cy) {}
  bool batch(PumpKind kind, std::vector<Command> &out) override {
    out.clear();
    if (kind != PumpKind::Blocking || next_ >= batches_.size()) return false;
    if (!has_t_ || last_t_ < arrive_[next_]) return false;
    out = batches_[next_];
    used_ += out.size();
    ++next_;
    return true;
  }
  bool exhausted() const override { return next_ >= batches_.size(); }
  bool cursor(int32_t &x, int32_t &y) override {
    x = cx_;
    y = cy_;
    ++cursor_reads_;
    return true;
  }
  bool clock(int32_t site, uint32_t &t) override {
    if (!q_.clock(site, t)) return false;
    last_t_ = t;
    has_t_ = true;
    return true;
  }
  void set_clock(int32_t site, std::vector<uint32_t> v) { q_.set_clock(site, std::move(v)); }
  size_t used() const { return used_; }
  uint32_t cursor_reads() const { return cursor_reads_; }

 private:
  QueueInput q_;  // its per-site clock script only
  std::vector<std::vector<Command>> batches_;
  std::vector<uint32_t> arrive_;
  size_t next_ = 0, used_ = 0;
  uint32_t last_t_ = 0, cursor_reads_ = 0;
  bool has_t_ = false;
  int32_t cx_, cy_;
};
std::map<const Ctx *, std::unique_ptr<TimedInput>> &owned() {
  static std::map<const Ctx *, std::unique_ptr<TimedInput>> m;
  return m;
}
// holestats.setScript(cmd_va, n_cmds, arrive_va, clock_va, n_clock, cx, cy): n_cmds raw commands of 6 dwords (handler, a0,
// a1, async_ctrl, async_shift, t), one arrival dword per batch at arrive_va, n_clock (site, value) pairs in script order
void a_set_script(Ctx &c, const uint32_t *a, Ret &) {
  std::vector<std::vector<Command>> batches;
  std::vector<Command> cur;
  for (uint32_t k = 0; k < a[1]; ++k) {
    uint32_t w[6];
    for (uint32_t j = 0; j < 6; ++j) w[j] = c.mem.load<uint32_t>(a[0] + 24 * k + 4 * j);
    Command cmd;
    cmd.kind = CmdKind::RootHandler;
    cmd.handler = w[0];
    cmd.a0 = i32(w[1]);
    cmd.a1 = i32(w[2]);
    cmd.async_ctrl = w[3];
    cmd.async_shift = w[4];
    cmd.t = w[5];
    cur.push_back(cmd);
    if (is_stop_handler(cmd.handler) || k + 1 == a[1]) {
      batches.push_back(std::move(cur));
      cur.clear();
    }
  }
  std::vector<uint32_t> arrive;
  for (size_t k = 0; k < batches.size(); ++k) arrive.push_back(c.mem.load<uint32_t>(a[2] + 4 * u32(static_cast<int32_t>(k))));
  std::map<int32_t, std::vector<uint32_t>> script;
  std::vector<int32_t> order;
  for (uint32_t k = 0; k < a[4]; ++k) {
    const int32_t site = i32(c.mem.load<uint32_t>(a[3] + 8 * k));
    if (!script.count(site)) order.push_back(site);
    script[site].push_back(c.mem.load<uint32_t>(a[3] + 8 * k + 4));
  }
  auto src = std::make_unique<TimedInput>(std::move(batches), std::move(arrive), i32(a[5]), i32(a[6]));
  for (int32_t s : order) src->set_clock(s, script[s]);
  set_input_source(c, src.get());
  owned()[&c] = std::move(src);
}
void a_consumed(Ctx &c, const uint32_t *, Ret &r) {
  const auto it = owned().find(&c);
  r.set_u32(it == owned().end() ? 0u : static_cast<uint32_t>(it->second->used()));
}
void a_cursor_reads(Ctx &c, const uint32_t *, Ret &r) {
  const auto it = owned().find(&c);
  r.set_u32(it == owned().end() ? 0u : it->second->cursor_reads());
}
void a_clear_script(Ctx &c, const uint32_t *, Ret &) {
  set_input_source(c, nullptr);
  owned().erase(&c);
}
// the registry needs a negative-control variable for every entry; the test entries share one that nothing reads
int g_negctl_none = 0;

const FuncDef kFuncs[] = {
    {"showHoleStatsScreen", "default", "void", 1, 0,
     [](Ctx &c, const uint32_t *a, Ret &) { show_hole_stats_screen(c, i32(a[0])); }, &g_negctl_holestats},
    {"holestats.setScript", "default", "void", 7, 0, a_set_script, &g_negctl_none},
    {"holestats.consumed", "default", "int", 0, 0, a_consumed, &g_negctl_none},
    {"holestats.cursorReads", "default", "int", 0, 0, a_cursor_reads, &g_negctl_none},
    {"holestats.clearScript", "default", "void", 0, 0, a_clear_script, &g_negctl_none},
};
}  // namespace

void show_hole_stats_screen(Ctx &ctx, int32_t hole) { HoleStats(ctx, hole).run(); }

extern const ModuleDef kModule_holestats = {"holestats", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
