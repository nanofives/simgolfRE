"""S2.8: insert the hole stats screen's drawing calls into the mirror's port/src/sim/holestats.cpp (run from the mirror root)."""
import pathlib

p = pathlib.Path('port/src/sim/holestats.cpp')
s = p.read_text()


def rep(old, new, count=1):
    """replace `old`, letting each of its line breaks match blank lines too (the source has blank lines the old text omits)"""
    global s
    import re
    pat = re.escape(old).replace("\\\n", "\n").replace("\n", "\n(?:[ \t]*\n)*")
    found = re.findall(pat, s)
    assert len(found) == count, (old[:80], len(found))
    s = re.sub(pat, lambda m: new, s)


rep('''#include "portsim/sim/commands.h"
''', '''#include "portsim/sim/commands.h"
#include "gfx_reports.h"
''')
rep('''// its value only selects a drawn image.''', '''// its value only selects a drawn image.
//
// ROADMAP S2.8: with an attached graphics backend the screen's drawing calls are made at their places (gfxr::ScreenDraw,
// gfx_reports.h; c3eo.cpp ShowHoleStatsScreen_re cites each): the dimmed screen once, then per page build the HoleSTAT art's head
// cell, every label and value over the text the build leaves in g_textBuffer, the row cells of the banner and the comments
// (the row cell's height apart from the head cell's height), the foot cell and the close button; the cursor read of a pass
// switches the button's hover image. Nothing of it writes the GameState.''')
rep('''  const uint32_t num;   // the itoa scratch g_ui_821c50
  HoleStats(Ctx &c, int32_t h)
      : ctx(c),''', '''  const uint32_t num;   // the itoa scratch g_ui_821c50
  const gfxr::ScreenDraw d;
  const uint32_t root, font_b, art;   // the drawing target, g_fontRef_821ee8, the HoleSTAT art g_spriteRef_821200
  int32_t hover = 0, button_y = 0, y = 0;
  HoleStats(Ctx &c, int32_t h)
      : ctx(c),''')
rep('''        num(GS(g_ui_821c50)) {}''', '''        num(GS(g_ui_821c50)),
        d(c),
        root(d.root()),
        font_b(GS(g_fontRef_821ee8)),
        art(GS(g_spriteRef_821200)) {}
  // the drawing calls over the text the build just left in g_textBuffer
  void w_draw(int32_t x, int32_t yy, uint32_t s = 0) {
    if (!s) s = text;
    d.gx("Widget_draw", {root, font_b, s, u32(x), u32(yy), d.len(s)});
  }
  void s_at(int32_t x, int32_t yy, uint32_t font = 0) {
    d.gx("drawStringAt", {root, font ? font : font_b, text, u32(x), u32(yy), d.len(text)});
  }''')
rep('''  void cursor_read() {
    if (ctx.gs().g_graphicsDevice == 0) return;
    int32_t x = 0, y = 0;
    if (src) src->cursor(x, y);
  }''', '''  void cursor_read() {
    if (ctx.gs().g_graphicsDevice == 0) return;
    int32_t x = 0, cy = 0;
    if (src && src->cursor(x, cy) && d.on) {
      // the close button's box (0x23e..0x269, button_y..button_y + 0x2b): its hover image when the cursor enters or leaves
      const bool in = x >= 0x23e && x < 0x26a && cy >= button_y && cy < add32(button_y, 0x2c);
      if (in != (hover != 0)) {
        hover = in ? 1 : 0;
        d.gx("Image_draw", {GS(g_spriteRef_820f40) + (in ? 0x2cu : 0u), root, 0x23e, u32(button_y), 0});
        d.gx("Window::refreshFull", {d.window(), 0});
      }
    }
  }''')
rep('''    int32_t rounds = r32(0x20);
    cm::cpy(ctx, text, pool + 0xf0);
    cm::call_peer(ctx, "appendHoleName", {u32(hole)});
    if ((G<uint8_t>(ctx, rec + 0x200) & 0x81) != 0) {
      cm::cat(ctx, text, GS(f_strpool_4c4970) + 4);
      cat_num(hole);
      cm::cat(ctx, text, GS(f_strpool_4c59e0));
    }
    cm::cpy(ctx, text, GS(f_strpool_4c8808) + 0xe8);
    clear_text();
    cat_num(r16(4));''', '''    int32_t rounds = r32(0x20);
    d.gx("Surface_stretchBlit", {art, art + 0x2c, root, 0, 0, 0});
    y = d.cut_h(art);
    d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
    cm::cpy(ctx, text, pool + 0xf0);
    cm::call_peer(ctx, "appendHoleName", {u32(hole)});
    if ((G<uint8_t>(ctx, rec + 0x200) & 0x81) != 0) {
      cm::cat(ctx, text, GS(f_strpool_4c4970) + 4);
      cat_num(hole);
      cm::cat(ctx, text, GS(f_strpool_4c59e0));
    }
    s_at(0x181, 0x30, GS(g_fontRef_821020));
    cm::cpy(ctx, text, GS(f_strpool_4c8808) + 0xe8);
    w_draw(0x1b9, 0x53);
    clear_text();
    cat_num(r16(4));
    s_at(0x218, 0x53);''')
rep('''        cm::cpy(ctx, text, label);
        clear_text();
        if (cents) cm::append_cents(ctx, value);
        else cat_num(value);
        if (suffix != 0) cm::cat(ctx, text, suffix);
      }
    }
    cm::cpy(ctx, text, GS(f_strpool_4c64a8) + 0x28);
    clear_text();
    cat_num(G<int8_t>(ctx, rec));''', '''        cm::cpy(ctx, text, label);
        w_draw(0x1b9, 0x68);
        clear_text();
        if (cents) cm::append_cents(ctx, value);
        else cat_num(value);
        if (suffix != 0) cm::cat(ctx, text, suffix);
        s_at(0x24f, 0x68);
      }
    }
    cm::cpy(ctx, text, GS(f_strpool_4c64a8) + 0x28);
    w_draw(0x1b9, 0x7d);
    clear_text();
    cat_num(G<int8_t>(ctx, rec));
    s_at(0x218, 0x7d);
    w_draw(0xbe, 0xb0, pool + 0x84);''')
rep('''      clear_text();
      cat_num(score);
      if (score == hi) cm::cat(ctx, text, GS(f_strpool_4c5340));
      int32_t row = 0;''', '''      clear_text();
      cat_num(score);
      if (score == hi) cm::cat(ctx, text, GS(f_strpool_4c5340));
      const int32_t col_x = add32(mul32(sub32(score, lo), 0x22), 0x1bd);
      s_at(col_x, 0xa8);
      int32_t row = 0;''')
rep('''      clear_text();
      cat_num(row);
    }
    if (total != 0) {
      cm::cpy(ctx, text, pool + 0x74);
      clear_text();
      cm::append_cents(ctx, div32(mul32(weighted, 100), total));
    }''', '''      clear_text();
      cat_num(row);
      if (row != 0) s_at(col_x, 0xbc);
    }
    if (total != 0) {
      cm::cpy(ctx, text, pool + 0x74);
      w_draw(0x1b9, 0x92);
      clear_text();
      cm::append_cents(ctx, div32(mul32(weighted, 100), total));
      s_at(0x24f, 0x92);
    }
    w_draw(0xbe, 0x53, GS(f_strpool_4d2c28) + 0x43c);''')
rep('''      cm::cat(ctx, text, word);
    }''', '''      cm::cat(ctx, text, word);
      s_at(0x164, 0x53);
    }
    w_draw(0xbe, 0x68, pool + 0x34);
    w_draw(0xbe, 0x7d, pool + 0x28);
    w_draw(0xbe, 0x92, GS(f_strpool_4c8808) + 0xa8);''')
rep('''      static const int kCmp[3] = {6, 5, 3};
      for (int c : kCmp) {
        if (col_c[c] == 0) continue;
        cm::cpy(ctx, text, GS(f_strpool_4c5340));
        const int32_t a = div32(mul32(col_w[c], 100), col_c[c]);
        const int32_t b = div32(mul32(col_w[7], 100), col_c[7]);
        cm::append_cents(ctx, sub32(a, b));
        cm::call_peer(ctx, "appendRatingLabel", {u32(ctx.gs().g_ui_569628)});
      }
    }''', '''      static const int kCmp[3][2] = {{6, 0x68}, {5, 0x7d}, {3, 0x92}};
      for (const auto &k : kCmp) {
        const int c = k[0];
        if (col_c[c] == 0) continue;
        cm::cpy(ctx, text, GS(f_strpool_4c5340));
        const int32_t a = div32(mul32(col_w[c], 100), col_c[c]);
        const int32_t b = div32(mul32(col_w[7], 100), col_c[7]);
        cm::append_cents(ctx, sub32(a, b));
        cm::call_peer(ctx, "appendRatingLabel", {u32(ctx.gs().g_ui_569628)});
        s_at(0x164, k[1]);
      }
    }
    w_draw(0xbe, 0xca, pool + 0x1c);''')
rep('''    if (G<uint8_t>(ctx, rec) == 0) (void)clock(kHsClockBanner);''', '''    if (G<uint8_t>(ctx, rec) == 0) {
      d.gx("Surface_stretchBlit", {art + 0x58, art + 0x84, root, 0, u32(y), 0});
      const uint32_t t = clock(kHsClockBanner);
      if ((t & 0x200) != 0) {
        d.gx("Widget_setQuad", {root, 0x80007d08u, 0xffffffffu, 2, 2});
        d.gx("drawStringAt", {root, font_b, pool + 8, 0x190, u32(y), d.len(pool + 8)});
      }
      y = add32(y, d.cut_h(art + 0x58));
    }''')
rep('''      if (r32(0x20) == 0) continue;
      const int32_t pct = div32(mul32(comment[best_k], 100), r32(0x20));''', '''      if (r32(0x20) == 0) continue;
      d.gx("Surface_stretchBlit", {art + 0x58, art + 0x84, root, 0, u32(y), 0});
      const int32_t pct = div32(mul32(comment[best_k], 100), r32(0x20));''')
rep('''      cm::cat(ctx, text, GS(f_strpool_4c4e54));
      comment[best_k] = 0;
    }
    return rounds;''', '''      cm::cat(ctx, text, GS(f_strpool_4c4e54));
      const uint32_t tone = ctx.gs().g_commentTone;
      d.gx("Widget_setQuad",
           {root, tone == 0x800023e8u ? 0x80001284u : tone == 0x80007d08u ? 0x80007d08u : 0x80000000u, 0xffffffffu, 2, 2});
      s_at(0x190, y);
      y = add32(y, d.cut_h(art + 0x58));
      comment[best_k] = 0;
    }
    d.gx("Surface_stretchBlit", {art + 0xb0, art + 0xdc, root, 0, u32(y), 0});
    button_y = add32(y, 8);
    d.gx("Image_draw", {GS(g_spriteRef_820f40) + (hover != 0 ? 0x2cu : 0u), root, 0x23e, u32(button_y), 0});
    d.gx("Window::refreshFull", {d.window(), 0});
    return rounds;''')
rep('''    const int32_t div = r32(0x20) != 0 ? 5 : 3;
    uint32_t page = page_of(clock(kHsClockEntry), div);''', '''    const int32_t div = r32(0x20) != 0 ? 5 : 3;
    uint32_t page = page_of(clock(kHsClockEntry), div);
    d.gx("reports.begin", {});
    d.gx("Surface_drawSprite", {GS(g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});''')
p.write_text(s)
print("patched")
