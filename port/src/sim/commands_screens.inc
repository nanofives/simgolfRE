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

// showBestScoresScreen: a page without a simulation write
int32_t show_best_scores_screen(Ctx &ctx) {
  click_or_key_loop(ctx, true, "showBestScoresScreen");
  leave_report(ctx, "showBestScoresScreen");
  return 0;
}
// showHistographScreen: the ten skill-axis labels go through appendCents (its formatting buffer g_textBuf is a hashed
// object): the left labels (y below 0x108) from scale * -2000 in steps of scale * 500, the right ones from 0 in steps of
// scale * 100, scale = 2 when g_skillRatingX100 > 0x9c4; the week count g_date / 1024 divides 600 (weeks + 1 == 0 is the
// original's #DE); the graphs themselves are drawing
int32_t show_histograph_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int32_t scale = gs.g_skillRatingX100 > 0x9c4 ? 2 : 1;
  int32_t left = mul32(scale, -2000), right = 0;
  for (int32_t y = 0x202; y > 0xe; y -= 0x32) {
    ctx.mem.store<uint8_t>(GS(g_textBuffer), 0);
    append_cents(ctx, y < 0x108 ? left : right);
    left = add32(left, mul32(scale, 500));
    right = add32(right, mul32(scale, 100));
  }
  (void)div32(600, add32(gs.g_date / 1024, 1));
  click_or_key_loop(ctx, true, "showHistographScreen");
  leave_report(ctx, "showHistographScreen");
  return 0;
}

// showFinancialReport: for every year column shown (the last year g_year clamped to 0..99 and the eight before
// it), entry 8 of the year's g_financeYears row (0x14 bytes, nine int16 entries) becomes the int16 sum of entries 0..7
// (model_defects: the report writes the total back)
int32_t show_financial_report(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int32_t last = clamp(gs.g_year, 0, 99);
  const int32_t first = clamp(last - 8, 0, 999);
  const uint32_t rows = state_va(ctx, &GameState::g_financeYears);
  for (int32_t year = first; year <= last; ++year) {   // every cell goes through appendNumber (g_textBuf is hashed)
    const uint32_t row = rows + u32(year * 0x14);
    int32_t total = 0;
    for (int32_t i = 0; i < 8; ++i) {
      const int32_t v = ctx.mem.load<int16_t>(row + 2 * u32(i));
      call_peer(ctx, "appendNumber", {u32(mul32(v, 100))});
      total = add32(total, v);
    }
    call_peer(ctx, "appendNumber", {u32(mul32(total, 100))});
    ctx.mem.store<int16_t>(row + 16, static_cast<int16_t>(SG_NEGCTL(g_negctl_reports) == 2 ? total + 1 : total));
  }
  click_or_key_loop(ctx, false, "showFinancialReport");
  leave_report(ctx, "showFinancialReport");
  return 0;
}

// showPlayerCommentsReport: per comment kind the count over holes 1..18 (int16 at hole record +0xd8 + 2 kind;
// hole n's record is g_holes + 0x208 n) and the hole with the most; then up to 20 times the kind with the highest
// remaining count (kinds below 0x32) is written as a comment (buildGolferComment(kind, word & 0x3fff of hole record
// +0x16c + 2 kind, -1, 0x98): the simulation write) and its count cleared. Needs a non-zero total of the holes' dword +0x20.
int32_t show_player_comments_report(Ctx &ctx) {
  const Memory &m = ctx.mem;
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
      call_peer(ctx, "buildGolferComment", {u32(best_kind), arg, u32(-1), 0x98});
      counts[best_kind] = 0;
    }
  }
  click_or_key_loop(ctx, true, "showPlayerCommentsReport");
  leave_report(ctx, "showPlayerCommentsReport");
  return 0;
}

// showHireEmployeeDialog: the row under the cursor (four bands of two rows), the dialog loop (pumpMessages,
// waitTicks, then: a button ends it; a key alone returns -1 without pumpLight), pumpLight, and the row mapped to
// (row / 2) * 3 + 1 + (row & 1) (-1 outside 0..0xc). The four animated sprites divide the frame counter by their sheet
// sizes (g_sheetCols[0x20e + s], [0x212 + s] for the hovered one): a zero size is the original's #DE.
int32_t show_hire_employee_dialog(Ctx &ctx) {
  GameState &gs = ctx.gs();
  InputSource *src = input_source(ctx);
  int32_t frame = 0, sel = -1;
  for (;;) {
    int32_t px = 0, py = 0;
    if (!src || !src->cursor(px, py)) throw NeedInput{{"showHireEmployeeDialog"}};
    if (py >= 0xab && py < 0xd3) sel = py >= 0xbf ? 1 : 0;
    else if (py >= 0xf2 && py < 0x11a) sel = 2 + (py >= 0x106 ? 1 : 0);
    else if (py >= 0x139 && py < 0x161) sel = 4 + (py >= 0x14d ? 1 : 0);
    else if (py >= 0x180 && py < 0x1a8) sel = 6 + (py >= 0x194 ? 1 : 0);
    else sel = -1;
    for (int32_t k = 0; k < 8; ++k)   // the cost of each row through appendNumber (g_textBuf is hashed)
      call_peer(ctx, "appendNumber", {u32(mul32(static_cast<int32_t>(*(state_arr(ctx, &GameState::f_tab_bytes_4c2e2c) + k)), 100))});
    frame = add32(frame, 1);
    for (int32_t s = 0, b = 1; b < 9; b += 2, ++s) {
      const int32_t c = s * 4 + (sel == b ? 0x10 : 0);
      (void)mod32(frame, *(state_arr(ctx, &GameState::g_sheetCols) + (0x20e + c / 4)));
    }
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
  gs.g_rootInput[kButton] = 0;
  int32_t sel = -1;
  for (;;) {
    pump_messages(ctx, PumpKind::Blocking, "selectDifficultyScreen");
    int32_t x = 0, y = 0;
    if (!src || !src->cursor(x, y)) throw NeedInput{{"selectDifficultyScreen"}};
    sel = -1;
    if (approx_distance(sub32(x, 0x15c) / 2, sub32(y, 0x6b)) < 0x50) sel = 0;
    if (approx_distance(sub32(x, 0x13f) / 2, sub32(y, 0xda)) < 0x50) sel = 1;
    if (approx_distance(sub32(x, 0x140) / 2, sub32(y, 0x16f)) < 0x50) sel = 2;
    if (approx_distance(sub32(x, 0x16b) / 2, sub32(y, 0x1ef)) < 0x50) sel = 3;
    if (approx_distance(sub32(x, 0x2ff), sub32(y, 0x22d)) < 0x19) sel = 4;
    if (gs.g_rootInput[kButton] != 0) break;
    if (flush_redraw(ctx) == 0) continue;
    if (gs.g_rootInput[kButton] == 0) {
      sel = -1;
      break;
    }
    break;
  }
  if (sel == 4 && SG_NEGCTL(g_negctl_difficulty) != 1) sel = -1;
  return sel;
}

// showLoadingScreen (percent): the loading picture is Random::range(12) (always one of the twelve: loadImage is the
// renderer's) and the hint Random::range(11) (0..10 all have a text); both draws always happen, the rest is drawing
int32_t show_loading_screen(Ctx &ctx, int32_t percent) {
  (void)percent;
  rng_range(ctx, SG_NEGCTL(g_negctl_reports) == 3 ? 11 : 12);
  rng_range(ctx, 11);
  return 0;
}
