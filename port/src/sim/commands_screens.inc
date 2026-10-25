// commands.cpp part: the modal report and choice screens whose effect is a few writes plus the input they consume.
// Each draws its page once, waits (waitTicks(10, 0), no dispatch in the port), then loops: waitTicks(1, 0), a cursor read
// for the button hover (drawing only), pumpMessages (Blocking) and the exit tests in the original's order; on the way out
// the message-box mode byte g_ui_569498 is cleared and pumpLight drains what is left. Specification: c3en.cpp.

// the click-or-key loop of the report screens. button_first: the button word is tested before flushRedraw (the best
// scores, comments and histograph screens); otherwise flushRedraw runs first and the button word is tested after it (the
// financial report)
void click_or_key_loop(Ctx &ctx, bool button_first, const char *where) {
  GameState &gs = ctx.gs();
  for (;;) {
    pump_messages(ctx, PumpKind::Blocking, where);
    if (button_first) {
      if (gs.g_rootInput[kButton] != 0) break;
      if (flush_redraw(ctx) != 0) break;
    } else {
      const bool key = flush_redraw(ctx) != 0;
      if (gs.g_rootInput[kButton] != 0 || key) break;
    }
  }
}

void leave_report(Ctx &ctx, const char *where) {
  ctx.gs().g_ui_569498 = SG_NEGCTL(g_negctl_reports) == 1 ? 1 : 0;
  pump_light(ctx, where);
}

// showBestScoresScreen: a page without a simulation write. Roadmap S2.8: with an attached backend the page is drawn as c3en.cpp
// ShowBestScoresScreen_re draws it: the dimmed screen, the lowscore art's head cell, the title `Best <n> Hole Scores` (n =
// g_holesPlusOne - 1) and the two column heads, one row cell per non-zero entry of g_bestRounds (at most ten) with its score
// (centred at 0x227) and `<row + 1>.  ` + appendString(row + 0x14) (left at 0xf6), rows the row cell's height apart from the head
// cell's height + 0x2d, the foot cell and the OK button 0xe below it.
int32_t show_best_scores_screen(Ctx &ctx) {
  const gfxr::ScreenDraw d(ctx);
  if (d.on) {
    const uint32_t root = d.root(), font_b = GS(g_fontRef_821ee8), art = GS(g_spriteRef_8222b8), pool = GS(f_strpool_4d32f0);
    d.gx("reports.begin", {});
    d.gx("Surface_drawSprite", {GS(g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
    d.gx("Surface_stretchBlit", {art, art + 0x2c, root, 0xc3, 0x2d, 0});
    d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
    d.text(
        [&] {
          cpy(ctx, d.buf(), pool + 0x38);
          d.append(std::to_string(sub32(ctx.gs().g_holesPlusOne, 1)));
          cat(ctx, d.buf(), pool + 0x28);
        },
        [&](uint32_t s) { d.gx("drawStringAt", {root, GS(g_fontRef_821020), s, 0x19d, 0x3b, d.len(s)}); });
    d.gx("Widget_draw", {root, font_b, pool + 0x20, 0xec, 0x60, d.len(pool + 0x20)});
    d.gx("drawStringAt", {root, font_b, pool + 0x18, 0x227, 0x60, d.len(pool + 0x18)});
    const int32_t row_h = d.cut_h(art + 0x58), head_h = d.cut_h(art);
    const uint32_t rec = GS(g_bestRounds);
    int32_t rows = 0;
    for (; rows < 10; ++rows) {
      const int32_t v = ctx.mem.load<int32_t>(rec + 4 * u32(rows));
      if (v == 0) break;
      const uint32_t y = u32(add32(add32(mul32(row_h, rows), head_h), 0x2d));
      d.gx("Surface_stretchBlit", {art + 0x58, art + 0x84, root, 0xc3, y, 0});
      d.text([&] { d.put(std::to_string(v)); },
             [&](uint32_t s) { d.gx("drawStringAt", {root, font_b, s, 0x227, y, d.len(s)}); });
      d.text(
          [&] {
            d.put(std::to_string(rows + 1));
            cat(ctx, d.buf(), pool + 0x14);
            call_peer(ctx, "appendString", {u32(rows + 0x14)});
          },
          [&](uint32_t s) { d.gx("Widget_draw", {root, font_b, s, 0xf6, y, d.len(s)}); });
    }
    const int32_t end_y = add32(add32(mul32(row_h, rows), head_h), gfxr::ScreenDraw::ctl(13) ? 0x2e : 0x2d);
    d.gx("Surface_stretchBlit", {art + 0xb0, art + 0xdc, root, 0xc3, u32(end_y), 0});
    d.gx("Image_draw", {GS(g_spriteRef_820f40), root, 0x220, u32(add32(end_y, 0xe)), 0});
    d.gx("Window::refreshFull", {d.window(), 0});
  }
  click_or_key_loop(ctx, true, "showBestScoresScreen");
  leave_report(ctx, "showBestScoresScreen");
  return 0;
}
// showHistographScreen: the ten skill-axis labels go through appendCents (its formatting buffer g_textBuf is a hashed
// object): the left labels (y below 0x108) from scale * -2000 in steps of scale * 500, the right ones from 0 in steps of
// scale * 100, scale = 2 when g_skillRatingX100 > 0x9c4; the week count g_date / 1024 divides 600 (weeks + 1 == 0 is the
// original's #DE); the graphs themselves are drawing
//
// Roadmap S2.8: with an attached backend the page is drawn as c3en.cpp ShowHistographScreen_re draws it: the dimmed screen,
// the histograph art, the title and the four legend words, per axis row the skill label (the text the loop above leaves,
// centred at 0x4d in 0x80004010) and the cash label (`$<n>k` with the cash scale of g_cash_div100: 1, 2, 4, 10, 0x14, 0x28 or
// 0x50 above 0x9c4, 0x1388, 0x2710, 0x61a8, 0xc350, 0x186a0), then inside the clip box (0x6a, 0x48, 0x256, 0x1c2) one column
// per month of g_date / 1024 (step clamp(600 / (months + 1), 1, 4)): four segments from the previous month's height (skill,
// cash in 0x80007d08 when negative, fun, members * 4) and an event marker per non-zero g_eventHistory entry, then the clip
// back to the bounds and the OK button.
int32_t show_histograph_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const gfxr::ScreenDraw d(ctx);
  const uint32_t root = d.root(), art = GS(g_spriteRef_821bf8), p44 = GS(f_strpool_4d3344);
  d.gx("reports.begin", {});
  d.gx("Surface_drawSprite", {GS(g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
  d.gx("Surface_stretchBlit", {art, art + 0x2c, root, 0, 0, 0});
  d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
  d.gx("drawStringAt", {root, GS(g_fontRef_821020), p44 + 0x10, 0x177, 0x14, d.len(p44 + 0x10)});
  d.gx("setDrawTarget", {root, GS(g_fontRef_821ee8), 0, 0, 0});
  d.gx("drawCenteredText", {p44 + 8, 0xb1, 0x21e, 0x80000000u});
  d.gx("drawCenteredText", {GS(f_strpool_4c8808) + 0xe0, 0x14c, 0x21e, 0x80000000u});
  d.gx("drawCenteredText", {GS(f_strpool_4d2120), 0x1e8, 0x21e, 0x80000000u});
  d.gx("drawCenteredText", {p44, 0x284, 0x21e, 0x80000000u});
  const int32_t cash = gs.g_cash_div100;
  int32_t cash_scale = 1;
  if (cash > 0x9c4) cash_scale = 2;
  if (cash > 0x1388) cash_scale = 4;
  if (cash > 0x2710) cash_scale = 10;
  if (cash > 0x61a8) cash_scale = 0x14;
  if (cash > 0xc350) cash_scale = 0x28;
  if (cash > 0x186a0) cash_scale = 0x50;
  int32_t cash_axis = mul32(cash_scale, -200), cash_axis2 = 0;
  const int32_t scale = gs.g_skillRatingX100 > 0x9c4 ? 2 : 1;
  int32_t left = mul32(scale, -2000), right = 0;
  for (int32_t y = 0x202; y > 0xe; y -= 0x32) {
    ctx.mem.store<uint8_t>(GS(g_textBuffer), 0);
    append_cents(ctx, y < 0x108 ? left : right);
    d.gx("drawCenteredText", {d.buf(), 0x4d, u32(y), 0x80004010u});
    d.text(
        [&] {
          cpy(ctx, d.buf(), GS(f_strpool_4c61dc) + 4);
          d.append(std::to_string(y < 0x108 ? cash_axis : cash_axis2));
          cat(ctx, d.buf(), GS(f_strpool_4d3340));
        },
        [&](uint32_t t) { d.gx("drawCenteredText", {t, gfxr::ScreenDraw::ctl(19) ? 0x2d8u : 0x2d7u, u32(y), 0x80000000u}); });
    left = add32(left, mul32(scale, 500));
    right = add32(right, mul32(scale, 100));
    cash_axis = add32(cash_axis, mul32(cash_scale, 50));
    cash_axis2 = add32(cash_axis2, mul32(cash_scale, 10));
  }
  const int32_t step = clamp(div32(600, add32(gs.g_date / 1024, 1)), 1, 4);
  if (d.on) {
    d.gx("setDrawTarget", {root, GS(g_fontRef_519fd8), 0, 0, 0});
    d.gx("Surface_box", {root, 0x6a, 0x48, 0x256, 0x1c2});
    const int32_t months = gs.g_date / 1024;
    auto hist = [&](uint32_t base, int32_t k) { return static_cast<int32_t>(ctx.mem.load<int16_t>(base + 2 + 2 * u32(k))); };
    auto height = [](int32_t v) { return v > 0x1f4 ? add32(v / 10, 0xc8) : v / 2; };
    int32_t prev_skill = 0, prev_cash = 0, prev_fun = 0, prev_members = 0, slot = 0;
    int32_t x1 = 0x6b, x2 = add32(step, 0x6b);
    auto seg = [&](int32_t from, int32_t to, uint32_t colour) {
      d.gx("Widget_fillRect", {root, u32(x1), u32(sub32(0x209, from)), u32(x2), u32(sub32(0x209, to)), colour});
    };
    for (int32_t k = 0; k < months; ++k) {
      const int32_t sh = height(hist(GS(g_skillHistory), k) / scale);
      seg(prev_skill, sh, 0x80004010u);
      prev_skill = sh;
      const int32_t cw = hist(GS(g_cashHistory), k), cv = cw / cash_scale;
      const int32_t ch = cv > 0x1f4 ? add32(cv / 10, 0xc8) : abs32(cv) / 2;
      seg(prev_cash, ch, cw < 0 ? 0x80007d08u : 0x80000000u);
      prev_cash = ch;
      const int32_t fh = height(hist(GS(g_funHistory), k));
      seg(prev_fun, fh, 0x800003e0u);
      prev_fun = fh;
      const int32_t mh = mul32(hist(GS(g_memberTypeHistory), k), 4);
      seg(prev_members, mh, 0x80000210u);
      prev_members = mh;
      const int32_t code = hist(GS(g_eventHistory), k);
      if (code != 0) {
        // the event's text (the same pieces as the end-of-year highlights; a kind outside the two windows keeps the
        // previous event's text), drawn beside a marker that walks 10 pixels per event modulo 0x1c2
        slot = add32(mod32(slot, 0x1c2), 10);
        const int32_t base = sub32(0x209, prev_skill);
        d.gx("fillWindowRect", {d.window(), u32(add32(x2, 10)), u32(base), 2, 2, 0x80000210u});
        d.gx("Widget_fillRect", {root, u32(add32(x2, 11)), u32(base), u32(add32(x2, 11)), u32(sub32(0x209, slot)), 0x800003ffu});
        d.gx("fillWindowRect", {d.window(), u32(add32(x2, 11)), u32(sub32(0x209, slot)), 2, 2, 0x80000210u});
        // the marker's text (drawText4049d0 at (x2 + 14, 0x209 - slot - 3) inside the GDI clip region Surface_box selected)
        // is not drawn: no body, so the backend reports it (U-1273)
        d.gx("showHistographScreen.eventText", {u32(code)});
      }
      x1 = add32(x1, step);
      x2 = add32(x2, step);
    }
    if (gs.g_graphicsDevice != 0) d.gx("Surface::callBounds", {root});   // the original tests the surface handle +4, set once a display exists
    d.gx("Image_draw", {GS(g_spriteRef_820f40) + 0x84, root, 0x2c0, 0x227, 0});
    d.gx("Window::refreshFull", {d.window(), 0});
  }
  click_or_key_loop(ctx, true, "showHistographScreen");
  leave_report(ctx, "showHistographScreen");
  return 0;
}

// showFinancialReport: for every year column shown (the last year g_year clamped to 0..99 and the eight before
// it), entry 8 of the year's g_financeYears row (0x14 bytes, nine int16 entries) becomes the int16 sum of entries 0..7
// (model_defects: the report writes the total back)
// Roadmap S2.8: the page's drawing calls (c3en.cpp ShowFinancialReport_re) for an attached graphics backend
// (gfx_reports.h): the dimmed screen, the report art, the title, the nine row labels (pointer table f_ptrtab_4d2054, the
// total's 7 pixels lower), per year column the year label and the nine cells (right-aligned 0x22 right of the column; a zero
// entry draws the empty text g_str4e9a84; a negative value in 0x80006000) and the OK button; the cells' texts come from the
// same appendNumber calls, built in the text buffer and given back (gfxr::ScreenDraw::text).
int32_t show_financial_report(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const gfxr::ScreenDraw d(ctx);
  const uint32_t root = d.root(), font_b = GS(g_fontRef_821ee8);
  d.gx("reports.begin", {});
  d.gx("Surface_drawSprite", {GS(g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
  d.gx("Surface_stretchBlit", {GS(g_infoSprites8210c8), GS(g_infoSprites8210c8) + 0x2c, root, 8, 7, 0});
  d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
  const uint32_t title = GS(f_strpool_4d2c28) + 0x1e8;
  d.gx("Widget_draw", {root, GS(g_fontRef_821020), title, 0xda, 0x15, d.len(title)});
  for (int32_t i = 0; d.on && i < 9; ++i) {
    const uint32_t label = ctx.mem.load<uint32_t>(GS(f_ptrtab_4d2054) + 4 * u32(i));
    if (label != 0) d.gx("drawStringAt", {root, font_b, label, 0x62, u32((i != 8 ? 0 : 7) + i * 0x11 + 0x57), d.len(label)});
  }
  const int32_t last = clamp(gs.g_year, 0, 99);
  const int32_t first = clamp(last - 8, 0, 999);
  const uint32_t rows = state_va(ctx, &GameState::g_financeYears);
  for (int32_t year = first; year <= last; ++year) {   // every cell goes through appendNumber (g_textBuf is hashed)
    const uint32_t row = rows + u32(year * 0x14);
    const int32_t x = add32(mul32(sub32(year, first), 0x4b), 0xde);
    d.text([&] { d.put(std::to_string(add32(year, 0x7d1))); },
           [&](uint32_t s) {
             d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
             d.gx("drawStringAt", {root, font_b, s, u32(x), gfxr::ScreenDraw::ctl(11) ? 0x3du : 0x3cu, d.len(s)});
           });
    int32_t total = 0, cell_y = 0x56;
    auto cell = [&](int32_t shown, int32_t stored, bool zero_text) {
      d.text(
          [&] {
            d.put("");
            call_peer(ctx, "appendNumber", {u32(mul32(shown, 100))});
            if (zero_text) cpy(ctx, d.buf(), GS(g_str4e9a84));
          },
          [&](uint32_t s) {
            d.gx("Widget_setQuad", {root, static_cast<int16_t>(stored) < 0 ? 0x80006000u : 0x80000000u, 0xffffffffu, 2, 2});
            d.gx("drawStringAt2", {root, font_b, s, u32(x + 0x22), u32(cell_y), d.len(s)});
          });
    };
    for (int32_t i = 0; i < 8; ++i) {
      const int32_t v = ctx.mem.load<int16_t>(row + 2 * u32(i));
      call_peer(ctx, "appendNumber", {u32(mul32(v, 100))});
      cell(v, v, v == 0);
      cell_y += i != 7 ? 0x11 : 0x18;
      total = add32(total, v);
    }
    call_peer(ctx, "appendNumber", {u32(mul32(total, 100))});
    ctx.mem.store<int16_t>(row + 16, static_cast<int16_t>(SG_NEGCTL(g_negctl_reports) == 2 ? total + 1 : total));
    cell(total, ctx.mem.load<int16_t>(row + 16), false);
  }
  d.gx("Image_draw", {GS(g_spriteRef_820f40), root, 0x2d6, 0xf9, 0});
  d.gx("Window::refreshFull", {d.window(), 0});
  click_or_key_loop(ctx, false, "showFinancialReport");
  leave_report(ctx, "showFinancialReport");
  return 0;
}

// showPlayerCommentsReport: per comment kind the count over holes 1..18 (int16 at hole record +0xd8 + 2 kind;
// hole n's record is g_holes + 0x208 n) and the hole with the most; then up to 20 times the kind with the highest
// remaining count (kinds below 0x32) is written as a comment (buildGolferComment(kind, word & 0x3fff of hole record
// +0x16c + 2 kind, -1, 0x98): the simulation write) and its count cleared. Needs a non-zero total of the holes' dword +0x20.
// Roadmap S2.8: the original builds each drawn row in the text buffer (the quote f_strpool_4c4e54, buildGolferComment's text,
// the quote; then the percentage with its '%' and the hole number, both through itoa into g_ui_821c50) and the port now does
// the same writes (the buffers are not simulation objects); with an attached backend the page is drawn as c3en.cpp
// ShowPlayerCommentsReport_re draws it: the dimmed screen, the PlayComt art (head cell 0, a row cell per drawn row, the foot
// cell), the title and the three column heads, per row the comment (left at 0xb6, in the colour g_commentTone selects), the
// percentage (centred at 0x256) and the hole (centred at 0x1f8), rows 0xf apart from the head cell's height + 0x16, and the OK
// button 0xe below the foot cell.
int32_t show_player_comments_report(Ctx &ctx) {
  const Memory &m = ctx.mem;
  const gfxr::ScreenDraw d(ctx);
  const uint32_t root = d.root(), font_b = GS(g_fontRef_821ee8), art = GS(g_spriteRef_8223c0), pool = GS(f_strpool_4d317c);
  const uint32_t buf = d.buf(), num = GS(g_ui_821c50), quote = GS(f_strpool_4c4e54);
  d.gx("reports.begin", {});
  d.gx("Surface_drawSprite", {GS(g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
  d.gx("Surface_stretchBlit", {art, art + 0x2c, root, 0x94, 0x2d, 0});
  int32_t sprite_y = add32(d.cut_h(art), gfxr::ScreenDraw::ctl(12) ? 0x17 : 0x16);
  d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
  d.gx("drawStringAt", {root, GS(g_fontRef_821020), pool + 0x10c, 0x185, 0x3d, d.len(pool + 0x10c)});
  d.gx("Widget_draw", {root, font_b, pool + 0x1c, 0xb6, 0x60, d.len(pool + 0x1c)});
  d.gx("drawStringAt", {root, font_b, GS(f_strpool_4c8808) + 0xa0, 0x1f8, 0x60, d.len(GS(f_strpool_4c8808) + 0xa0)});
  d.gx("drawStringAt", {root, font_b, pool + 0x100, 0x256, 0x60, d.len(pool + 0x100)});
  auto itoa_into_buf = [&](int32_t v) {   // buf[0] = 0; strcat(buf, itoa(v, g_ui_821c50, 10))
    const std::string t = std::to_string(v);
    for (size_t i = 0; i <= t.size(); ++i) ctx.mem.store<uint8_t>(num + static_cast<uint32_t>(i), static_cast<uint8_t>(i < t.size() ? t[i] : 0));
    ctx.mem.store<uint8_t>(buf, 0);
    cat(ctx, buf, num);
  };
  const uint32_t holes = state_va(ctx, &GameState::g_holes);
  int32_t counts[0x40], best[0x40] = {0};
  int32_t total = 0;
  for (int32_t kind = 0; kind < 0x40; ++kind) {
    int32_t mx = 0;
    counts[kind] = 0;
    for (int32_t hole = 1; hole <= 18; ++hole) {
      const uint32_t rec = holes + u32(hole * 0x208);
      const int32_t c = m.load<int16_t>(rec + 0xd8 + 2 * u32(kind));
      counts[kind] = add32(counts[kind], c);
      if (kind == 0) total = add32(total, m.load<int32_t>(rec + 0x20));
      if (c > mx) {
        mx = c;
        best[kind] = hole;
      }
    }
  }
  int32_t best_kind = 0;
  for (int32_t row = 0x14; row != 0; --row) {
    int32_t mx = 0;
    for (int32_t k = 0; k < 0x32; ++k)
      if (counts[k] > mx) {
        mx = counts[k];
        best_kind = k;
      }
    if (mx != 0 && total != 0) {
      const int32_t hole = best[best_kind];
      const uint32_t arg = m.load<uint16_t>(holes + 0x16c + 2 * u32(add32(best_kind, mul32(hole, 0x104)))) & 0x3fffu;
      d.gx("Surface_stretchBlit", {art + 0x58, art + 0x84, root, 0x94, u32(sprite_y), 0});
      cpy(ctx, buf, quote);
      call_peer(ctx, "buildGolferComment", {u32(best_kind), arg, u32(-1), 0x98});
      cat(ctx, buf, quote);
      const uint32_t tone = ctx.gs().g_commentTone;
      const uint32_t colour = tone == 0x800023e8u ? 0x80001284u : tone == 0x80007d08u ? 0x80007d08u : 0x80000000u;
      const uint32_t text_y = u32(add32(sprite_y, 2));
      d.gx("Widget_setQuad", {root, colour, 0xffffffffu, 2, 2});
      d.gx("Widget_draw", {root, font_b, buf, 0xb6, text_y, d.len(buf)});
      itoa_into_buf(div32(mul32(counts[best_kind], 100), total));
      cat(ctx, buf, GS(f_strpool_4c60c4));
      d.gx("drawStringAt", {root, font_b, buf, 0x256, text_y, d.len(buf)});
      itoa_into_buf(hole);
      d.gx("drawStringAt", {root, font_b, buf, 0x1f8, text_y, d.len(buf)});
      counts[best_kind] = 0;
      sprite_y = add32(sprite_y, 0xf);
    }
  }
  d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
  d.gx("Surface_stretchBlit", {art + 0xb0, art + 0xdc, root, 0x94, u32(sprite_y), 0});
  d.gx("Image_draw", {GS(g_spriteRef_820f40), root, 0x24f, u32(add32(sprite_y, 0xe)), 0});
  d.gx("Window::refreshFull", {d.window(), 0});
  click_or_key_loop(ctx, true, "showPlayerCommentsReport");
  leave_report(ctx, "showPlayerCommentsReport");
  return 0;
}

// showHireEmployeeDialog: the row under the cursor (four bands of two rows), the dialog loop (pumpMessages,
// waitTicks, then: a button ends it; a key alone returns -1 without pumpLight), pumpLight, and the row mapped to
// (row / 2) * 3 + 1 + (row & 1) (-1 outside 0..0xc). The four animated sprites divide the frame counter by their sheet
// sizes (g_sheetCols[0x20e + s], [0x212 + s] for the hovered one): a zero size is the original's #DE.
// showCreditsScreen (c3en.cpp; the QUIT block's last screen): the lines of the CREDITS section of credits.txt are read
// (Reader::open over the global reader: the key `#` + section matched case-insensitively against a line stripped of its
// newline and spaces, then readLine until a line that starts with '#'; each kept line is a malloc'd copy, counted; no
// section = nothing happens). Then the scroll runs on the clock: t0 at the start, y = 0x258 - (now - t0) / 0x1e per pass
// (unsigned division), the loop ends when -y exceeds lineH * count + 0x96, on a mouse button or on a key (flushRedraw); while
// y < 0x258 each pass draws lines from y down, lineH apart, with one pumpMessages per line until y reaches 0x258. lineH is
// g_fontRef_821020's +0xc when its +8 is negative, else +0x10 + +8. The backgrounds, the logo, the text, the line blocks and
// waitTicks(1, 0) are drawing / waits. Its clock reads pace the recorded dispatches as pumpAndRedraw's do (commands.h, PACE).
namespace {
constexpr int32_t kSiteCreditsStart = 0x3bf68, kSiteCreditsPass = 0x3bfaa;   // the two timeGetTime returns, from ML's frame head
std::string credits_str(Ctx &ctx, uint32_t va) {
  std::string s;
  for (uint8_t c; (c = ctx.mem.load<uint8_t>(va)) != 0; ++va) s.push_back(static_cast<char>(c));
  return s;
}
int32_t credits_line_count(Ctx &ctx, bool &found) {
  found = false;
  std::string name = credits_str(ctx, GS(f_strpool_4d21a4) + 0x8);
  if (name.find('.') == std::string::npos) name += credits_str(ctx, GS(f_strpool_4e4418));
  if (ctx.files_ptr == nullptr || !ctx.files().exists(name)) return 0;
  const std::vector<uint8_t> data = ctx.files().read_all(name);
  // fgets in text mode: lines split at LF with CR LF read as LF, then stripNewline / trimSpaces
  std::vector<std::string> lines;
  std::string cur;
  for (size_t i = 0; i < data.size(); ++i) {
    if (data[i] == '\r' && i + 1 < data.size() && data[i + 1] == '\n') continue;
    if (data[i] == '\n') {
      lines.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(static_cast<char>(data[i]));
    }
  }
  if (!cur.empty()) lines.push_back(cur);
  auto trim = [](std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t k = 0;
    while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) ++k;
    return s.substr(k);
  };
  auto lower = [](std::string s) {
    for (char &c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return s;
  };
  const std::string key = lower(credits_str(ctx, GS(f_str_4e4438)) + credits_str(ctx, GS(f_strpool_4d21a4) + 0x14));
  size_t i = 0;
  while (i < lines.size() && lower(trim(lines[i])) != key) ++i;
  if (i == lines.size()) return 0;
  found = true;
  int32_t count = 0;
  for (++i; i < lines.size(); ++i) {
    const std::string t = trim(lines[i]);
    if (!t.empty() && t[0] == '#') break;
    ++count;
  }
  return count;
}
// Phase 5 / roadmap S2.3: the original's drawing calls, made at their places for an attached graphics backend
// (portsim/sim/gfx.h); without one each costs one test. The two Surfaces and the Image of the original's frame are backend
// objects named by gfx_local_key (offsets from 0x3000, apart from the other screens' frame objects); the kept lines are the
// backend's own (showCreditsScreen.lines, gfx_credits.cpp).
void credits_gx(Ctx &ctx, const char *name, std::initializer_list<uint32_t> a) {
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
}
constexpr uint32_t kCreditsBg = gfx_local_key(0x3000), kCreditsLogo = gfx_local_key(0x3100),
                   kCreditsImg = gfx_local_key(0x3200);
// the objects' end: both Surfaces freed, the Image destroyed, then each Surface freed again (its base class shares the
// destructor)
void credits_teardown(Ctx &ctx) {
  credits_gx(ctx, "Surface_free", {kCreditsBg});
  credits_gx(ctx, "Surface_free", {kCreditsLogo});
  credits_gx(ctx, "Image_dtor", {kCreditsImg});
  credits_gx(ctx, "Surface_free", {kCreditsLogo});
  credits_gx(ctx, "Surface_free", {kCreditsBg});
}
}  // namespace

int32_t show_credits_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_credits);
  const uint32_t root = gs.f_stateptr_4c1570[0], pool = GS(f_strpool_4d21a4);
  // the objects, the 800 x 600 surface of the Image, the background (the pool's text +0x28) taken whole by the first Surface,
  // the logo (text +0x1c) taken by the second at its own size, the Image's data released
  credits_gx(ctx, "Surface_ctor", {kCreditsBg});
  credits_gx(ctx, "Surface_ctor", {kCreditsLogo});
  credits_gx(ctx, "Image_ctor", {kCreditsImg});
  credits_gx(ctx, "Surface_createFull", {kCreditsImg, 0x320, 0x258, 0, 1, 0, 0});
  credits_gx(ctx, "loadImage", {kCreditsImg, pool + 0x28, 0, 0, 0x100, 1});
  credits_gx(ctx, "Surface_blitTakeDirty", {kCreditsBg, kCreditsImg, 0, 0, 0x320, 0x258, 1, 0});
  credits_gx(ctx, "loadImage", {kCreditsImg, pool + 0x1c, 0, 0, 0x100, 1});
  credits_gx(ctx, "showCreditsScreen.logo", {kCreditsLogo, kCreditsImg});
  credits_gx(ctx, "Image_releaseAll", {kCreditsImg});
  bool found = false;
  const int32_t count = credits_line_count(ctx, found);
  if (!found) {
    credits_teardown(ctx);
    return 0;
  }
  credits_gx(ctx, "showCreditsScreen.lines", {u32(count)});
  credits_gx(ctx, "setDrawTarget", {root, GS(g_fontRef_821020), 0, 0, 0});
  credits_gx(ctx, "Widget_setQuad", {root, 0x80007fffu, 0xffffffffu, 2, 2});
  InputSource *src = input_source(ctx);
  uint32_t t0 = 0;
  if (!src || !src->clock(kSiteCreditsStart, t0)) throw NeedInput{{"showCreditsScreen"}};
  const uint32_t font = GS(g_fontRef_821020);
  int32_t line_h = ctx.mem.load<int32_t>(font + 0xc);
  if (ctx.mem.load<int32_t>(font + 8) >= 0) line_h = add32(ctx.mem.load<int32_t>(font + 0x10), ctx.mem.load<int32_t>(font + 8));
  if (count == 0) {
    credits_teardown(ctx);
    return 0;
  }
  const int64_t total = int64_t(line_h) * count + 0x96;
  src->pace(kSiteCreditsPass);
  struct Unpace {
    InputSource *s;
    ~Unpace() { s->pace(0); }
  } unpace{src};
  bool running = true;
  do {
    uint32_t now = 0;
    if (!src->clock(kSiteCreditsPass, now)) throw NeedInput{{"showCreditsScreen"}};
    int32_t y = 0x258 - static_cast<int32_t>((now - t0) / 0x1e);
    credits_gx(ctx, "Image_draw", {kCreditsBg, root, 0, 0, 0});
    if (-int64_t(y) > total) running = false;
    if (y < 0x258)
      for (int32_t i = 0; i < count; ++i) {
        credits_gx(ctx, "showCreditsScreen.line", {u32(i), u32(y), kCreditsLogo});
        y = add32(y, line_h);
        // the scroll does not wait for input: once the source holds no further dispatch the pumps receive nothing, as the
        // original's do (a Blocking pump would ask for one and fail)
        if (src->exhausted()) {
          if (gs.g_rootInput[kClicked] == 0) gs.g_rootInput[kButton] = 0;
          gs.g_rootInput[kClicked] = 0;
        } else {
          pump_messages(ctx, PumpKind::Blocking, "showCreditsScreen");
        }
        if (y >= 0x258) break;
      }
    if (gs.g_rootInput[kButton] != 0) running = false;
    else if (mode != 1 && flush_redraw(ctx) != 0) running = false;
    credits_gx(ctx, "Window::refreshFull", {GS(g_rootWindow), 0});
  } while (running);
  credits_gx(ctx, "showCreditsScreen.free", {});
  credits_teardown(ctx);
  return 0;
}

// Roadmap S2.8: with an attached backend each pass is drawn as c3en.cpp ShowHireEmployeeDialog_re draws it (the screen dimmed once
// before the loop): the hire art over its mask, the OK button (its hover image when no row is selected and the cursor is in its
// box), the title centred in g_fontRef_821020, then in g_fontRef_821ee8 a band head before every second row (0xe7, at y 0x97,
// 0xde, 0x125, 0x16c) and each row 0x14 lower (8 more after the odd one), the selected row on a fillWindowRect bar (0x80007f40:
// the display's slot +0xb4 is jgld's `xor eax, eax; ret`), the four animated employees (addSprite of a FLIC sheet cell: no body,
// reported, roadmap S3) and the refresh.
int32_t show_hire_employee_dialog(Ctx &ctx) {
  GameState &gs = ctx.gs();
  InputSource *src = input_source(ctx);
  const gfxr::ScreenDraw d(ctx);
  const uint32_t root = d.root(), pool = GS(f_strpool_4d36bc);
  d.gx("reports.begin", {});
  d.gx("Surface_drawSprite", {GS(g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
  int32_t frame = 0, sel = -1;
  for (;;) {
    int32_t px = 0, py = 0;
    if (!src || !src->cursor(px, py)) throw NeedInput{{"showHireEmployeeDialog"}};
    if (py >= 0xab && py < 0xd3) sel = py >= 0xbf ? 1 : 0;
    else if (py >= 0xf2 && py < 0x11a) sel = 2 + (py >= 0x106 ? 1 : 0);
    else if (py >= 0x139 && py < 0x161) sel = 4 + (py >= 0x14d ? 1 : 0);
    else if (py >= 0x180 && py < 0x1a8) sel = 6 + (py >= 0x194 ? 1 : 0);
    else sel = -1;
    const bool over = sel == -1 && px >= 0x229 && px < 0x251 && py >= 0x1a8 && py < 0x1cf;
    d.gx("Surface_stretchBlit", {GS(g_infoSprites821070), GS(g_infoSprites821070) + 0x2c, root, 0, 0, 0});
    d.gx("Image_draw", {GS(g_spriteRef_820f40) + (over ? 0x2cu : 0u), root, 0x229, 0x1a6, 0});
    d.gx("setDrawTarget", {root, GS(g_fontRef_821020), 0, 0, 0});
    d.gx("drawCenteredText", {pool + 0x70, 0x179, 0x6b, 0x80000000u});
    d.gx("setDrawTarget", {root, GS(g_fontRef_821ee8), 0, 0, 0});
    int32_t y = 0;
    static const uint32_t kHead[4][2] = {{0x64, 0x97}, {0x50, 0xde}, {0x40, 0x125}, {0x2c, 0x16c}};   // pool offset, y
    // each row's text in g_textBuffer: its name from the pointer table f_ptrtab_4c1464, a separator, the cost through
    // appendNumber (g_textBuf is hashed) and a suffix, then drawn (phase 4, the boot path: without the copy the eight
    // appendNumber calls of every pass grew the text past g_textBuffer into the FontRefs above it, ui_tour record 1090)
    const uint32_t text = GS(g_textBuffer);
    for (int32_t k = 0; k < 8; ++k) {
      if ((k & 1) == 0) {
        y = static_cast<int32_t>(kHead[k / 2][1]) + (gfxr::ScreenDraw::ctl(18) ? 1 : 0);
        d.gx("drawText4049d0", {pool + kHead[k / 2][0], 0xe7, u32(y), 0x80000000u});
      }
      y = add32(y, 0x14);
      if (SG_NEGCTL(g_negctl_hire_dialog) != 2)
        cpy(ctx, text, ctx.mem.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c1464) + 4u * u32(k)));
      cat(ctx, text, state_va(ctx, &GameState::f_strpool_4d21a4) + 0x7b4);
      call_peer(ctx, "appendNumber", {u32(mul32(static_cast<int32_t>(*(state_arr(ctx, &GameState::f_tab_bytes_4c2e2c) + k)), 100))});
      cat(ctx, text, state_va(ctx, &GameState::f_strpool_4d36bc) + 0x20);
      if (sel == k) d.gx("fillWindowRect", {d.window(), 0xe7, u32(sub32(y, 2)), 0x123, 0x10, 0x80007f40u});
      d.gx("drawText4049d0", {text, 0xf6, u32(y), 0x80000000u});
      if ((k & 1) != 0) y = add32(y, 8);
    }
    frame = add32(frame, 1);
    for (int32_t s = 0, b = 1, sx = 0xc0; b < 9; b += 2, ++s, sx += 0x46) {
      const int32_t c = s * 4 + (sel == b ? 0x10 : 0);
      (void)mod32(frame, *(state_arr(ctx, &GameState::g_sheetCols) + (0x20e + c / 4)));
      const int32_t idx = sel == b ? s + 4 : s;
      d.gx("addSprite", {0x240, u32(sx), u32(y), 0x28, 0x28, 0, 4, u32(idx + 0x82)});
    }
    d.gx("Window::refreshFull", {d.window(), 0});
    pump_messages(ctx, PumpKind::Blocking, "showHireEmployeeDialog");
    if (gs.g_rootInput[kButton] != 0) break;
    if (flush_redraw(ctx) == 0) continue;
    if (gs.g_rootInput[kButton] == 0) return -1;
    break;
  }
  pump_light(ctx, "showHireEmployeeDialog");
  if (sel < 0 || sel > 0xc) sel = -1;
  return add32(add32(mul32(sel / 2, 3), SG_NEGCTL(g_negctl_hire_dialog) == 1 ? 0 : 1), sel & 1);
}

// selectDifficultyScreen: the button word cleared; loop: pumpMessages, the choice under the cursor (four
// ellipses and the back button 4, later tests win), waitTicks(2, 0), a button ends it, a key alone ends it with -1;
// 4 (back) returns -1
int32_t select_difficulty_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  InputSource *src = input_source(ctx);
  // roadmap S2.7: the drawing with an attached graphics backend (gfx_boot_screens.cpp): the frame objects, each pass's
  // redraw before the pump, the back button's tip after 0x1e passes on it, refreshFull after the choice
  const bool draw = g_gfx_attached.load(std::memory_order_relaxed) != 0;
  auto gx = [&](const char *name, std::initializer_list<uint32_t> a) {
    if (draw) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  };
  gx("selectDifficultyScreen.setup", {});
  gs.g_rootInput[kButton] = 0;
  int32_t sel = -1, dwell = 0;
  for (;;) {
    gx("selectDifficultyScreen.redraw", {static_cast<uint32_t>(sel)});
    pump_messages(ctx, PumpKind::Blocking, "selectDifficultyScreen");
    int32_t x = 0, y = 0;
    if (!src || !src->cursor(x, y)) throw NeedInput{{"selectDifficultyScreen"}};
    const int32_t prev = sel;
    sel = -1;
    if (approx_distance(sub32(x, 0x15c) / 2, sub32(y, 0x6b)) < 0x50) sel = 0;
    if (approx_distance(sub32(x, 0x13f) / 2, sub32(y, 0xda)) < 0x50) sel = 1;
    if (approx_distance(sub32(x, 0x140) / 2, sub32(y, 0x16f)) < 0x50) sel = 2;
    if (approx_distance(sub32(x, 0x16b) / 2, sub32(y, 0x1ef)) < 0x50) sel = 3;
    if (approx_distance(sub32(x, 0x2ff), sub32(y, 0x22d)) < 0x19) sel = 4;
    if (sel == prev) {
      dwell = add32(dwell, 1);
      if (dwell > 0x1e && sel == 4) gx("selectDifficultyScreen.tip", {});
    } else {
      dwell = 0;
    }
    gx("Window::refreshFull", {state_va(ctx, &GameState::g_rootWindow), 0});
    if (gs.g_rootInput[kButton] != 0) break;
    if (flush_redraw(ctx) == 0) continue;
    if (gs.g_rootInput[kButton] == 0) {
      sel = -1;
      break;
    }
    break;
  }
  if (sel == 4 && SG_NEGCTL(g_negctl_difficulty) != 1) sel = -1;
  gx("selectDifficultyScreen.teardown", {});
  return sel;
}

// showLoadingScreen (percent): the loading picture is Random::range(12) (always one of the twelve: loadImage is the
// renderer's) and the hint Random::range(11) (0..10 all have a text); both draws always happen, the rest is drawing
int32_t show_loading_screen(Ctx &ctx, int32_t percent) {
  const int32_t r = rng_range(ctx, SG_NEGCTL(g_negctl_reports) == 3 ? 11 : 12);
  const int32_t r2 = rng_range(ctx, 11);
  // roadmap S2.7: the drawing (picture r, hint r2, the bar) with an attached graphics backend (gfx_boot_screens.cpp)
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {
    const uint32_t a[3] = {static_cast<uint32_t>(percent), static_cast<uint32_t>(r), static_cast<uint32_t>(r2)};
    gfx_drop(ctx, "showLoadingScreen.draw", a, 3);
  }
  return 0;
}
