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
}  // namespace

int32_t show_credits_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_credits);
  bool found = false;
  const int32_t count = credits_line_count(ctx, found);
  if (!found) return 0;
  InputSource *src = input_source(ctx);
  uint32_t t0 = 0;
  if (!src || !src->clock(kSiteCreditsStart, t0)) throw NeedInput{{"showCreditsScreen"}};
  const uint32_t font = GS(g_fontRef_821020);
  int32_t line_h = ctx.mem.load<int32_t>(font + 0xc);
  if (ctx.mem.load<int32_t>(font + 8) >= 0) line_h = add32(ctx.mem.load<int32_t>(font + 0x10), ctx.mem.load<int32_t>(font + 8));
  if (count == 0) return 0;
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
    if (-int64_t(y) > total) running = false;
    if (y < 0x258)
      for (int32_t i = 0; i < count; ++i) {
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
  } while (running);
  return 0;
}

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
    // each row's text in g_textBuffer: its name from the pointer table f_ptrtab_4c1464, a separator, the cost through
    // appendNumber (g_textBuf is hashed) and a suffix, then drawn (phase 4, the boot path: without the copy the eight
    // appendNumber calls of every pass grew the text past g_textBuffer into the FontRefs above it, ui_tour record 1090)
    const uint32_t text = GS(g_textBuffer);
    for (int32_t k = 0; k < 8; ++k) {
      if (SG_NEGCTL(g_negctl_hire_dialog) != 2)
        cpy(ctx, text, ctx.mem.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c1464) + 4u * u32(k)));
      cat(ctx, text, state_va(ctx, &GameState::f_strpool_4d21a4) + 0x7b4);
      call_peer(ctx, "appendNumber", {u32(mul32(static_cast<int32_t>(*(state_arr(ctx, &GameState::f_tab_bytes_4c2e2c) + k)), 100))});
      cat(ctx, text, state_va(ctx, &GameState::f_strpool_4d36bc) + 0x20);
    }
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
