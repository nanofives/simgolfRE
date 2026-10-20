// The leaf straddlers of mainLoop's frame (phase 3.7a; included by tick.cpp inside namespace portsim::tk). Stage B kept local
// copies of startMessage / appendCents / logTick (U-0903, U-0883, the course module's startMessage); these are the
// registered bodies, written from the C3 hook sources (shim/src/re/c3g.cpp, golf_writers.cpp, c3ev.cpp, c3ei.cpp).
struct Leaves : TkBase {
  using TkBase::TkBase;
  GameState &gs() { return ctx.gs(); }

  // startMessage(arg, prio, sound) (c3g.cpp): refused while a message shows (g_ui_569498 or g_msgState53df54) unless prio > 0,
  // and in panel mode 3; otherwise copies g_textBuffer to the message text unk_005a6d40, shows it (g_ui_569498 = 1), stores
  // arg / sound, the display time strlen / (3 or 4) + 16 into g_messageTimer, -prio into the delay for prio < 0, and two
  // RNG draws (the ticker position: range(600), range(200) + 200). Returns 1 when shown.
  int startMessage(int arg, int prio, int sound) {
    if (((GU8(GS(g_ui_569498)) != 0 || gs().g_msgState53df54 != 0) && prio <= 0) || gs().g_panelListMode == 3) return 0;
    gs().g_msgState53df54 = 0;
    StrCpy(GS(unk_005a6d40), kBuf);
    GU8(GS(g_ui_569498)) = 1;
    G32(GS(g_ui_5a34e4) + 8) = arg;
    gs().g_messageTimer = static_cast<int32_t>(vstrlen(ctx.mem, kBuf)) / ((gs().g_difficulty != 0 ? 1 : 0) + 3) + 16;
    G32(GS(f_w_4c2e00) + 8) = sound;
    if (prio < 0) G32(GS(g_ui_5694a0) + 4) = neg32(prio);
    if (SG_NEGCTL(g_negctl_start_message) == 1) {  // the two draws in the other order
      G32(GS(g_ui_56d1a8) + 4) = add32(Rnd(200), 200);
      G32(GS(g_ui_56d1a8)) = Rnd(600);
    } else {
      G32(GS(g_ui_56d1a8)) = Rnd(600);
      G32(GS(g_ui_56d1a8) + 4) = add32(Rnd(200), 200);
    }
    return 1;
  }

  // appendCents(n) (c3g.cpp): records n in g_ui_569628; appends "<-><n / 100>.<n % 100, two digits>" to g_textBuffer (a
  // negative value drops a trailing '+' first), the numbers through __itoa into g_textBuf
  void appendCents(int n) {
    G32(GS(g_ui_569628)) = n;
    if (n < 0) {
      const uint32_t len = vstrlen(ctx.mem, kBuf);
      if (GU8(kBuf + len - 1) == '+') GU8(kBuf + len - 1) = 0;
      putc_end('-');
      n = neg32(n);
    }
    StrCat(kBuf, static_cast<uint32_t>(Itoa(n / 100, GS(g_textBuf), 10)));
    putc_end('.');
    if (n % 100 < 10 && SG_NEGCTL(g_negctl_append_cents) != 1) putc_end('0');
    StrCat(kBuf, static_cast<uint32_t>(Itoa(n % 100, GS(g_textBuf), 10)));
  }
  void putc_end(uint8_t c) {
    const uint32_t e = kBuf + vstrlen(ctx.mem, kBuf);
    GU8(e) = c;
    GU8(e + 1) = 0;
  }

  // logTick(a, b) (golf_writers.cpp): the event history slot of the current game month gets (a | b)
  void logTick(int a, int b) {
    const int tick = gs().g_date;
    const int slot = tick / 1024 % 500 + (SG_NEGCTL(g_negctl_log_tick) == 1 ? 1 : 0);
    G16(GS(g_eventHistory) + 2u * static_cast<uint32_t>(slot)) = static_cast<int16_t>(a | b);
  }

  // tickMessage() (c3ev.cpp): H1's message ticker. A pending delay counts down first; otherwise the message text goes back
  // into g_textBuffer, the slide counter unk_005a9ccc moves toward 3 while shown (toward 0 after), drawWindow draws it, and
  // every third date unit is skipped; else g_messageTimer counts down and hides the message at 0. Returns the original's eax.
  int tickMessage() {
    const uint32_t delay = GS(g_ui_5694a0) + 4;
    if (G32(delay) != 0) return --G32(delay), G32(delay);
    StrCpy(kBuf, GS(unk_005a6d40));
    const uint32_t count = GS(unk_005a9ccc);
    int v = -1;
    if (GU8(GS(g_ui_569498)) == 1) {
      if (G32(count) < 3) v = ++G32(count), G32(count);
    } else if (G32(count) != 0) {
      v = G32(count);
      --G32(count);
    }
    drawWindow(0xf0, -8, G32(GS(g_ui_5a34e4) + 8), v);
    const int flags = G32(GS(g_flags));
    if (flags & 0x8000) return flags;
    const int date = gs().g_date;
    if (date % 3 == 0) return date / 3;
    const int left = SG_NEGCTL(g_negctl_tick_message) == 1 ? gs().g_messageTimer : --gs().g_messageTimer;
    if (left <= 0) GU8(GS(g_ui_569498)) = 0;
    return left;
  }

  // drawWindow(x, y, p3, mode) (c3ei.cpp), simulation part: with a portrait selection (f_w_4c2e00+8 >= 0) drawThoughtBubble's
  // composeGolferSprite. The window height it stores into the hidden word g_value587ea0 depends on wrapMessageBuffer's line
  // count, which measures text with the renderer's font (Text_put): not computed (U-0921). The frames, blits, the wrapped
  // text and the sound are drawing / audio. Returns 0 (no frame code reads the result).
  int drawWindow(int x, int y, int p3, int mode) {
    (void)x;
    (void)y;
    (void)p3;
    (void)mode;
    const int sel = G32(GS(f_w_4c2e00) + 8);
    if (sel != -1 && sel >= 0 && SG_NEGCTL(g_negctl_draw_window) != 1) drawThoughtBubble(sel, 0, 0, -1);
    return 0;
  }

  // drawThoughtBubble(g, x, y, mode) (c3ei.cpp), simulation part: composeGolferSprite(g), twice for mode 1
  int drawThoughtBubble(int g, int x, int y, int mode) {
    (void)x;
    (void)y;
    if (SG_NEGCTL(g_negctl_draw_thought_bubble) == 1) return 0;
    cc("composeGolferSprite", g);
    if (mode != 1) return 0;
    cc("composeGolferSprite", g);
    return 0;
  }

  // saveGame(name) (c3ep.cpp), the state part of the autosave P5 runs twice a game month: the path "<dir><name><ext>" is
  // built in g_textBuffer, g_saveFd = _open(path) (the CRT descriptor: see save_descriptor), the header text (course
  // title, the day of the month through __itoa into g_textBuf, appendDate, which also refreshes g_year) is built in
  // g_textBuffer, then the header and serializeCourse(0)'s save body are written and the file closed. The file writes are
  // output (dropped); serializeCourse in save mode writes no state (census, notes). Returns 0 (_close's result).
  int saveGame(uint32_t name) {
    // the frame copy of the name (char[0x78]) is the original's local; the text below reads only the copy
    std::vector<uint8_t> copy;
    for (uint32_t p = name;; ++p) {
      const uint8_t b = GU8(p);
      copy.push_back(b);
      if (b == 0) break;
    }
    FrameSlots path(*this, static_cast<uint32_t>(copy.size()));
    for (size_t k = 0; k < copy.size(); ++k) GU8(path.va + static_cast<uint32_t>(k)) = copy[k];
    StrCpy(kBuf, GS(f_strpool_4c3ee0) + 0x6c);
    StrCat(kBuf, path.va);
    StrCat(kBuf, GS(f_strpool_4c3ee0) + 0x64);
    gs().g_saveFd = save_descriptor();
    StrCpy(kBuf, GS(f_strpool_4c52bc));
    cc("appendCourseTitle", 0);
    StrCat(kBuf, GS(f_strpool_4c528c) + 0x2c);
    int m = static_cast<int>(static_cast<uint32_t>(gs().g_date) & 0x800003ffu);
    if (m < 0) m = ((m - 1) | static_cast<int>(0xfffffc00u)) + 1;
    int v = mul32(m, 30);
    v = sar32(add32(v, sar32(v, 31) & 0x3ff), 10);
    v = v + 1;
    StrCat(kBuf, static_cast<uint32_t>(Itoa(v, GS(g_textBuf), 10)));
    StrCat(kBuf, GS(f_strpool_4c3f70));
    if (SG_NEGCTL(g_negctl_save_game) != 1) cc("appendDate", gs().g_date);
    return 0;
  }
  // The descriptor the static CRT's _open returns for the save file. The port writes no file; the value is the descriptor
  // the last save recorded (g_saveFd) when one did, else 4, the value every recorded save in the segment corpus left
  // (U-0922).
  int save_descriptor() { return gs().g_saveFd >= 0 ? gs().g_saveFd : 4; }

  // rankValue(v) (c3q.cpp): q = v / 200 truncated toward zero; 0 when q == 0, 1 when q <= 2 (every negative q too), 2 when
  // q <= 5, else 3 + (q > 9). buildSceneSprites ranks a building's takings with it.
  int rankValue(int v) {
    const int q = v / 200;
    if (q == 0) return 0;
    if (q <= 2) return SG_NEGCTL(g_negctl_rank_value) == 1 ? 2 : 1;
    if (q <= 5) return 2;
    return q > 9 ? 4 : 3;
  }

  // playHabitatSound(kind, a, b) (c3eu.cpp): kind 0 / 1 / 3 / 4 / 6 / 5 plays the sound 0x3b / 0x3a / 0xc3 / 0xc4 / 0xc5 /
  // 0xc6 at (a, b) through playSoundAt and returns its eax; any other kind returns kind. Kept, not dropped: playSoundAt (the
  // bridge module) draws g_rng when worldToScreen puts the point on screen (U-0860).
  int playHabitatSound(int kind, int a, int b) {
    static const int kId[7] = {0x3b, 0x3a, -1, 0xc3, 0xc4, 0xc6, 0xc5};  // by kind: 2 has no sound
    if (kind < 0 || kind > 6 || kId[kind] < 0) return kind;
    if (SG_NEGCTL(g_negctl_play_habitat_sound) == 1) return kind;  // negative control: no playSoundAt (no RNG draw)
    return cc("playSoundAt", kId[kind], a, b, 0);
  }

  // canAfford(cost) (c3em.cpp): 1 when cost <= 0, cost <= cash, in a championship (g_flags bit 24) or with at most 3 holes;
  // otherwise the "cannot afford" text (cost and cash as money, appendNumber) goes into g_textBuffer, drawWindow shows it
  // and pumpAndRedraw(0) waits for a click or key (an input-gated block: the registered pumpAndRedraw takes the run's next
  // input), and the result is 0
  int canAfford(int cost) {
    if (cost <= 0) return 1;
    if (SG_NEGCTL(g_negctl_can_afford) == 1 ? cost < gs().g_cash_div100 : cost <= gs().g_cash_div100) return 1;
    if ((GU32(GS(g_flags)) & 0x1000000u) != 0) return 1;
    if (G32(GS(g_holesPlusOne)) <= 3) return 1;
    StrCpy(kBuf, GS(f_strpool_4c4944) + 0x18);
    cc("appendNumber", mul32(cost, 100));
    StrCat(kBuf, GS(f_strpool_4c4944) + 4);
    cc("appendNumber", mul32(gs().g_cash_div100, 100));
    StrCat(kBuf, GS(f_strpool_4c4944));
    drawWindow(0xc8, 0x96, static_cast<int>(0x80007fffu), -2);
    cc("pumpAndRedraw", 0);
    return 0;
  }

  // addScore() (c3ah.cpp): inserts the course into the 10-row top-score table g_topScores (0x9c-byte rows: name +0,
  // text +0x40, fun +0x80, skill +0x84, cash +0x88, level word +0x96, course id +0x98) and returns its row, -1 when it
  // does not place or a better row already holds this course id; the score is (cash / 10 + skill + fun) * (level + 1)
  int addScore() {
    const uint32_t rows = GS(g_topScores);
    auto row = [rows](int i) { return rows + 0x9cu * static_cast<uint32_t>(i); };
    const int id = gs().g_courseId;
    const int score = mul32(gs().g_cash_div100 / 10 + G32(GS(g_skillRatingX100)) + G32(GS(g_funRating)),
                            gs().g_difficulty + (SG_NEGCTL(g_negctl_add_score) == 1 ? 2 : 1));
    G32(GS(g_scrollTextWin) + 0x58) = -1;
    int i = 0;
    for (;;) {
      const uint32_t r = row(i);
      const int rs = mul32(G32(r + 0x88) / 10 + G32(r + 0x80) + G32(r + 0x84), G16(r + 0x96) + 1);
      if (score >= rs) break;
      if (G32(r + 0x98) == id) return -1;
      if (++i == 10) return -1;
    }
    int slot = 9;
    for (int j = 9; j >= i; --j)
      if (G32(row(j) + 0x98) == id) slot = j;
    for (int j = slot; j > i; --j) rep_movs(ctx.mem, row(j), row(j - 1), 0x9c);
    GU8(kBuf) = 0;
    G32(row(i) + 0x88) = gs().g_cash_div100;
    G32(row(i) + 0x80) = G32(GS(g_funRating));
    G32(row(i) + 0x84) = G32(GS(g_skillRatingX100));
    G16(row(i) + 0x96) = static_cast<int16_t>(gs().g_difficulty);
    G32(row(i) + 0x98) = id;
    StrCpy(row(i), GS(g_golferTypes) + 0x10);
    cc("appendCourseTitle", 0);
    G32(GS(g_scrollTextWin) + 0x58) = i;
    StrCpy(row(i) + 0x40, kBuf);
    return i;
  }

  // hireEmployee() (c3fa.cpp): the hire dialog showHireEmployeeDialog (a modal screen: phase 3.7b's) gives a row k; a row
  // outside 0..11 returns; a mechanic row (k % 3 == 2) on a course without a building-level bucket refuses with a message;
  // canAfford(0); then spawnWalker(-2 - k / 3) creates the walker, which starts next to the first tee (or the clubhouse
  // with a random side) facing its entry tile, mechanics flagged, its hire day the current game month
  int hireEmployee() {
    const int k = cc("showHireEmployeeDialog");
    const int m = 0;  // enterModal483d30's eax (a pump reset; no caller reads hireEmployee's result)
    if (k < 0 || k >= 12) return m;
    const int b = cc("bucketValue", G32(GS(g_holesPlusOne)) - 1);
    if (b < 1 && k % 3 == 2) {
      StrCpy(kBuf, GS(f_strpool_4c4f98) + 0x290);
      return startMessage(static_cast<int>(0x80006000u), 1, -1);
    }
    if (canAfford(0) == 0) return 0;
    const int g = cc("spawnWalker", -2 - k / 3);
    const uint32_t r = GS(g_walkers) + 0x4cu * static_cast<uint32_t>(g);
    if (GU8(GS(g_holes) + 0x208) != 0) {
      const uint32_t c = (GU8(GS(g_holes) + 0x209) + 2u) & 7u;
      GU8(r + 0x10) = static_cast<uint8_t>(GU8(GS(f_tab_dirDx_4c2878) + 4 * c) + GU8(GS(g_holes) + 0x210));
      GU8(r + 0x11) = static_cast<uint8_t>(GU8(GS(f_tab_dirDy_4c2898) + 4 * c) + GU8(GS(g_holes) + 0x214));
    } else {
      const int r1 = Rnd(2);
      GU8(r + 0x10) = static_cast<uint8_t>(((r1 & 0xffff) ? 3 : -3) + GU8(GS(g_holes) + 0x26a0));
      const int r2 = Rnd(2);
      GU8(r + 0x11) = static_cast<uint8_t>(((r2 & 0xffff) ? 3 : -3) + GU8(GS(g_holes) + 0x26a4));
    }
    if (k % 3 == 2) GU8(r + 0x12) |= 8;
    const int dy = shl32(G8(r + 0x11), 10) - G32(r + 4) + 0x200;
    const int dx = shl32(G8(r + 0x10), 10) - G32(r) + 0x200;
    const int a = cc("angleFixed", dx, dy);
    const int d = ((((a >> 28) & 0xf) + 1) >> 1) & 7;
    G8(r + 0x16) = static_cast<int8_t>(d);
    G32(r) += shl32(G32(GS(f_tab_dirDx_4c2878) + 4u * d), 10) + 0x200;
    G32(r + 4) += shl32(G32(GS(f_tab_dirDy_4c2898) + 4u * d), 10) + 0x200;
    const int date = gs().g_date;
    const int day = (date + ((date >> 31) & 0x3ff)) >> 10;
    G16(r + 0x22) = static_cast<int16_t>(day + (SG_NEGCTL(g_negctl_hire_employee) == 1 ? 1 : 0));
    return day;
  }

  // drawLabels() (c3dv.cpp): the 8 floating text labels: a live timer (g_labelQueueB[k]) counts down, and an on-screen
  // label's text (g_labelQueueText + 0x40 k) goes into g_textBuffer for its drawing (dropped). Returns the last slot's eax.
  int drawLabels() {
    int eax = 0;
    for (int k = 0; k < 8; k++) {
      const uint32_t t = GS(g_labelQueueB) + 4u * k;
      eax = G32(t);
      if (eax == 0) continue;
      const int wy = G32(GS(g_floatLabelY) + 4u * k);
      if (SG_NEGCTL(g_negctl_draw_labels) != 1) G32(t) = eax - 1;
      LV sx(*this), sy(*this);
      eax = cc("worldToScreen", G32(GS(g_labelQueueC) + 4u * k).get(), wy, &sx, &sy, 0);
      if (eax == 0) continue;
      StrCpy(kBuf, GS(g_labelQueueText) + 0x40u * k);
      eax = 0;  // drawCenteredText2's result (drawing)
    }
    return eax;
  }

  // drawMoneyLabels() (c3dv.cpp): the 8 money popups: unless g_flags bit 24 (championship), a live timer
  // (g_moneyLabelTime[k]) counts down, and an on-screen one builds "<+><amount * 100 as money>" in g_textBuffer
  // (appendNumber, which writes g_textBuf) for its drawing (dropped)
  int drawMoneyLabels() {
    int eax = 0;
    for (int k = 0; k < 8; k++) {
      const uint32_t t = GS(g_moneyLabelTime) + 4u * k;
      eax = G32(t);
      if (eax == 0) continue;
      if ((GU32(GS(g_flags)) & 0x1000000u) != 0) continue;
      const int wy = G32(GS(g_moneyLabelY) + 4u * k);
      if (SG_NEGCTL(g_negctl_draw_money_labels) != 1) G32(t) = eax - 1;
      LV sx(*this), sy(*this);
      eax = cc("worldToScreen", G32(GS(g_moneyLabelX) + 4u * k).get(), wy, &sx, &sy, 0);
      if (eax == 0) continue;
      const int amount = G32(GS(g_moneyLabelValue) + 4u * k);
      StrCpy(kBuf, amount > 0 ? GS(f_strpool_4c5340) : GS(g_str4e9a84));
      cc("appendNumber", mul32(amount, 100));
      eax = 0;  // drawCenteredText2's result (drawing)
    }
    return eax;
  }
};
