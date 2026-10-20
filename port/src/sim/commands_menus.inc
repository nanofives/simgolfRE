// commands.cpp part: the menu-bar click handler and its popups (showSystemMenu, showPreferencesMenu, nearestMenuSpot) and
// the score summary screen. Specification: the C3 hook sources c3d.cpp (nearestMenuSpot), c3fa.cpp
// (showPreferencesMenu), c3fb.cpp (showSystemMenu, there named OptionsMenuBuild) and c3er.cpp (drawScoreSummary).

// nearestMenuSpot (x, y): the spot of f_tab_point16_4c7930 (ended by x == -1) whose approxDistance to the point times
// (i + 6) / 8 (signed) is smallest and below 30; spot 6 reads as none (-1)
int32_t nearest_menu_spot(Ctx &ctx, int32_t x, int32_t y) {
  const Memory &m = ctx.mem;
  const uint32_t spots = GS(f_tab_point16_4c7930);
  int32_t best = -1, best_d = SG_NEGCTL(g_negctl_menus) == 1 ? 31 : 30, i = 0;
  do {
    const int32_t sx = m.load<int16_t>(spots + 4 * u32(i)), sy = m.load<int16_t>(spots + 4 * u32(i) + 2);
    const int32_t d = mul32(approx_distance(sub32(x, sx), sub32(y, sy)), i + 6) / 8;
    if (d < best_d) {
      best_d = d;
      best = i;
    }
    ++i;
  } while (m.load<int16_t>(spots + 4 * u32(i)) != -1);
  return best == 6 ? -1 : best;
}

// showPreferencesMenu: the check list (the header line, then each of the six option strings of f_ptrtab_4c2ca8 plus a
// line break) as showPopup (0x190, 0xc8, g_preferences, 0, 0); the result becomes g_preferences and is returned
int32_t show_preferences_menu(Ctx &ctx) {
  const uint32_t text = GS(g_textBuffer);
  cpy(ctx, text, GS(f_strpool_4c7c30) + 4);
  for (int32_t k = 0; k < 6; ++k) {
    cat(ctx, text, ctx.gs().f_ptrtab_4c2ca8[k]);
    cat(ctx, text, GS(f_strpool_4c7c30));
  }
  const int32_t r = show_popup(ctx, 0x190, 0xc8, i32(ctx.gs().g_preferences), 0, 0);
  ctx.gs().g_preferences = u32(SG_NEGCTL(g_negctl_menus) == 2 ? r ^ 1 : r);
  return r;
}

// the System popup: its text, the course name (g_golferTypes + 0x10 in the model: the address the original reads, a
// model overlap noted in log/p3/p37b_notes.txt) and the tail, as showPopup (0xfa, 0x154, 1, 1, 0)
int32_t system_popup(Ctx &ctx) {
  const uint32_t text = GS(g_textBuffer);
  cpy(ctx, text, GS(f_strpool_4c7cac));
  cat(ctx, text, GS(g_golferTypes) + 0x10);
  cat(ctx, text, GS(f_strpool_4c7c30) + 0x18);
  return show_popup(ctx, 0xfa, 0x154, 1, 1, 0);
}

// showSystemMenu (x, y): the menu-bar click. nearestMenuSpot picks the spot: 0..2 toggle the speed word
// g_panelListMode between 5 and the spot (then f_w_4c2848[0] and [3] = -1, g_msgState53df54 = 0, -1); 3 the Information
// popup (a right button returns -0x70; the answers map to command codes, 10 shows the best scores, 11 the score
// summary); 4 0x50; 5 the System popup (0 'S', 1 'L', 2 'C', 3 save for championship play: promptAndSaveFile, then a
// message, or the popup again when it was cancelled; 4 rename: the course title into the text buffer, textInputBox, a
// non-empty sanitised name goes to appendToBuffer45b8b0, else the refusal sound; 5 the preferences; 6 'n'; 7 quit:
// unk_0059b734 = 1 and Esc); 6 '?', 7 'z', 8 'x', 9 -0x24, 10 -0x21; other spots and answers -1
int32_t show_system_menu(Ctx &ctx, int32_t x, int32_t y) {
  GameState &gs = ctx.gs();
  const int32_t spot = nearest_menu_spot(ctx, x, y);
  int32_t &speed = gs.g_panelListMode;
  switch (u32(spot)) {
    case 0: speed = speed != 0 ? 0 : 5; break;
    case 1: speed = speed == 1 ? 5 : 1; break;
    case 2: speed = speed == 2 ? 5 : 2; break;
    case 3: {
      if (gs.g_rootInput[kButton] == 2) return -0x70;
      const uint32_t text = GS(g_textBuffer);
      cpy(ctx, text, GS(f_strpool_4c7d80));
      cat(ctx, text, GS(f_strpool_4c7d00));
      static const int32_t kInfo[10] = {0x3f, -0x70, -0x71, 0x52, -0x72, -0x76, -0x73, -0x78, -0x79, 0x57};
      const uint32_t r = u32(show_popup(ctx, 0xc8, 0xfa, 1, 1, 0));
      if (r < 10) return kInfo[r];
      if (r == 10) call_peer(ctx, "showBestScoresScreen", {});
      else if (r == 11) call_peer(ctx, "drawScoreSummary", {});
      return -1;
    }
    case 4: return SG_NEGCTL(g_negctl_menus) == 4 ? 0x51 : 0x50;
    case 5: {
      uint32_t r = u32(system_popup(ctx));
      for (;;) {
        switch (r) {
          case 0: return 0x53;
          case 1: return 0x4c;
          case 2: return 0x43;
          case 3:
            if (call_peer(ctx, "promptAndSaveFile", {0, 1, 1}).eax != 0) {
              const uint32_t text = GS(g_textBuffer);
              cpy(ctx, text, GS(g_golferTypes) + 0x10);
              cat(ctx, text, GS(f_strpool_4c5ec8) + 0x50);
              start_message(ctx, i32(0x80006000u), 0, -1);
              return -1;
            }
            r = u32(system_popup(ctx));
            continue;
          case 4: {
            const uint32_t text = GS(g_textBuffer);
            ctx.mem.store<uint8_t>(text, 0);
            call_peer(ctx, "appendCourseTitle", {u32(-1)});
            call_peer(ctx, "textInputBox", {GS(f_strpool_4c5bbc) + 0x3c, 0xc8, 0x14c, 0x20});
            if (ctx.mem.load<uint8_t>(text) != 0 && call_peer(ctx, "sanitizeFileName", {text}).eax != 0) {
              call_peer(ctx, "appendToBuffer45b8b0", {0});
              return -1;
            }
            call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
            return -1;
          }
          case 5: call_peer(ctx, "showPreferencesMenu", {}); return -1;
          case 6: return 0x6e;
          case 7:
            gs.unk_0059b734 = 1;
            return 0x1b;
          default: return -1;
        }
      }
    }
    case 6: return 0x3f;
    case 7: return 0x7a;
    case 8: return 0x78;
    case 9: return -0x24;
    case 10: return -0x21;
    default: return -1;
  }
  gs.f_w_4c2848[0] = -1;
  gs.g_msgState53df54 = 0;
  gs.f_w_4c2848[3] = -1;
  return -1;
}

// drawScoreSummary: addScore, then the ten g_topScores rows (0x9c bytes: name, text, fun +0x80, skill +0x84, cash +0x88,
// level int16 +0x96) drawn over two images; what stays without the drawing is the text the rows format into
// g_textBuffer, the itoa scratch g_scoreSummaryBuf and appendCents' g_textBuf (U-0946), and the pumpAndRedraw(0) wait
int32_t draw_score_summary(Ctx &ctx) {
  call_peer(ctx, "addScore", {});
  const Memory &m = ctx.mem;
  const uint32_t text = GS(g_textBuffer), num = GS(g_scoreSummaryBuf), strs = GS(f_strpool_4e4130);
  for (int32_t i = 0; i < 10; ++i) {
    const uint32_t row = GS(g_topScores) + u32(0x9c * i);
    if (m.load<uint8_t>(row) == 0) continue;
    const int32_t fun = m.load<int32_t>(row + 0x80), skill = m.load<int32_t>(row + 0x84);
    const int32_t cash = m.load<int32_t>(row + 0x88), level = m.load<int16_t>(row + 0x96);
    cpy(ctx, text, strs + 0x38);   // the cash line
    cat(ctx, text, crt_itoa10(ctx, cash / 10, num));
    cpy(ctx, text, strs + 0x30);   // the skill line
    append_cents(ctx, skill);
    cpy(ctx, text, strs + 0x28);   // the fun line
    cat(ctx, text, crt_itoa10(ctx, fun, num));
    m.store<uint8_t>(text, 0);     // the total
    cat(ctx, text, crt_itoa10(ctx, mul32(add32(add32(cash / 10, skill), fun), level + 1), num));
    cpy(ctx, text, strs + 0x18);   // the multiplier line
    cat(ctx, text, crt_itoa10(ctx, SG_NEGCTL(g_negctl_menus) == 3 ? level : level + 1, num));
    cat(ctx, text, GS(f_strpool_4c59e0));
    if (i == 0) cpy(ctx, text, row + 0x40);   // row 0's own text line
  }
  pump_and_redraw(ctx, 0, "drawScoreSummary");
  return 0;
}
