// commands.cpp part: the LAYOUT EFFECTS of the panel-drawing straddlers. These draw the in-game panels every frame (P6 of
// mainLoop), and while they do they build the state the click handlers read next: the employee lists, the employee
// detail panel's scroll, selection, the walker animation word, the hire trigger, the amenity row selection. The port keeps
// exactly those writes and drops the drawing, the hover / tooltip counters (g_panelState[1], g_ui_5aa554: read only by
// drawing), the sprite-queue cells (g_ui_5aa6b8[3], addSprite) and the text they build in g_textBuffer (H5 scratch).
// appendDate is kept where the original calls it: it also stores g_year (economy.h). Specifications: c3fb.cpp
// (drawEmployeeViewPanel, drawGolferActionPanel), c3fc.cpp (drawEmployeeDetailPanel), c3ei.cpp (drawAmenitiesPanel,
// drawAmenityRow), c3ev.cpp (tickMessage), the C2 note of showInfoCursorPanel (its first branch,).

// drawAmenityRow: the row hit only (the strip, the icons and their sprites are drawing). p3 is tripled for
// p2 == 0x1a3; items whose bit in p10 (when p10 != 0) is clear are skipped; the last item within 0x14 of (p7, p8) (the y
// distance doubled) is the result, else -1.
int32_t amenity_row_hit(Ctx &, int32_t p1, int32_t p2, int32_t p3, int32_t p7, int32_t p8, uint32_t p10) {
  if (p2 == 0x1a3) p3 = mul32(p3, 3);
  const int32_t n = p3, n47 = mul32(47, n);
  const int32_t x0 = add32(sub32(p1, sar32(add32(n47, (n47 >> 31) & 3), 2)), 0x220);
  int32_t res = -1, a = 0;
  for (int32_t i = 0; i < n; ++i) {
    if (p10 == 0 || (p10 & (1u << (u32(i) & 31u))) != 0) {
      const int32_t px = add32(add32((a + (a < 0 ? 1 : 0)) / 2, x0), 0x2c);
      const int32_t py = 0x1f4 + ((i & 1) ? 0x1b : 0);
      if (approx_distance(sub32(p7, px), mul32(sub32(p8, py), 2)) < 0x14) res = i;
    }
    a = add32(a, 0x2f);
  }
  return res;
}

// drawAmenitiesPanel (a, b): for the active amenity tool f_w_4c2848[3] in 1, 4 (with g_amenityUnlockMask set),
// 0x13, 2, 0x10 (checked in that order; none of the steps changes the tool) the row under (a, b) becomes g_panelState[0]
int32_t draw_amenities_panel(Ctx &ctx, int32_t a, int32_t b) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_amenities_panel);
  const int32_t m = gs.f_w_4c2848[3];
  int32_t r = -1;
  if (m == 1) r = amenity_row_hit(ctx, -0x54, 0x208, 5, a, b, 0);
  else if (m == 4) r = gs.g_amenityUnlockMask != 0 ? amenity_row_hit(ctx, 0, 0x168, 0xe, a, b, u32(gs.g_amenityUnlockMask)) : -1;
  else if (m == 0x13) r = amenity_row_hit(ctx, -0x10, 0x226, 8, a, b, 0);
  else if (m == 2) r = amenity_row_hit(ctx, 0xd, 0x1a3, mode == 1 ? 6 : 5, a, b, 0);
  else if (m == 0x10) r = amenity_row_hit(ctx, 0x64, 0x12f, 7, a, b, 0);
  if (r != -1) gs.g_panelState[0] = r;
  return 0;
}

// showInfoCursorPanel (x, y): with the amenities palette open (g_buildPanelSlot) it is drawAmenitiesPanel(x, y);
// the info panel itself writes only hover counters and text. NOT PORTED: the info-panel path's store at writes
// the value 6 into the caller's saved esi (re/c3_hold.tsv), which is mainLoop's register state (the frame step's).
int32_t show_info_cursor_panel(Ctx &ctx, int32_t x, int32_t y) {
  if (ctx.gs().g_buildPanelSlot != 0) return draw_amenities_panel(ctx, x, y);
  return 0;
}

// drawEmployeeDetailPanel (x, y)
int32_t draw_employee_detail_panel(Ctx &ctx, int32_t x, int32_t y) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_detail_panel);
  const Memory &m = ctx.mem;
  const uint32_t emp = state_va(ctx, &GameState::g_walkers) + 0x10;  // the employee view of the walker records
  int32_t &scroll = gs.g_ui_5aa558;
  int32_t &listed = gs.g_ui_5aa6b8[2];
  int32_t &sel = gs.g_employeePanelMode;
  if (scroll > 0 && listed < 7) scroll = sub32(scroll, 2);
  (void)x;
  (void)y;
  if (sel != -1) {  // the circle around the selected employee: drawn with the zoom of g_ui_5a34e4[1], then zoom 4
    gs.g_zoomScale = gs.g_ui_5a34e4[1];
    gs.g_zoomScale = 4;
  }
  int32_t slot = neg32(scroll);
  listed = slot;
  for (int32_t k = 0; k < 64; ++k) {
    const uint32_t r = emp + u32(mul32(k, 0x4c));
    const int8_t type = static_cast<int8_t>(m.load<uint8_t>(r + 3));
    const uint8_t flags = m.load<uint8_t>(r + 2);
    if (type >= 0 || flags == 0 || type == -6) continue;
    if (slot < 0 || slot >= 8) {
      listed = slot = add32(slot, 1);
      continue;
    }
    int32_t t = -2 - type;
    if (flags & 8) t += 4;
    const int16_t anim = m.load<int16_t>(r + 0xe);
    const int32_t table = add32(anim < 0xb ? 0x20e : anim != 0xc ? 0x216 : 0x21e, t);
    const int32_t cols = *(state_arr(ctx, &GameState::g_sheetCols) + table);
    const int32_t phase = mod32(m.load<int16_t>(r + 0xc), cols);  // #DE when the sheet has no columns
    if (phase == 0 && anim == 0xc) m.store<int16_t>(r + 0xe, static_cast<int16_t>(mode == 1 ? 0xa : 0xb));
    slot = listed;
    (state_arr(ctx, &GameState::g_employeeDetailList) + slot).set(k);
    listed = slot = add32(slot, 1);
  }
  if (sel != -1) {
    const int32_t rec = *(state_arr(ctx, &GameState::g_employeeDetailList) + sel);
    const uint32_t r = emp + u32(mul32(rec, 0x4c));
    if (m.load<uint8_t>(r + 2) == 0) {
      sel = -1;
      return 0;
    }
    // the record's text block: the hire date and the current date go through appendDate (which stores g_year)
    call_peer(ctx, "appendDate", {u32(static_cast<int32_t>(m.load<int16_t>(r + 0x12)) << 10)});
    call_peer(ctx, "appendDate", {u32(gs.g_date)});
  }
  if (gs.g_ui_59ca54 == 0) {  // once per opening of the panel: hire, or the refusal sound when the list is full
    if (add32(listed, scroll) < 0x14) call_peer(ctx, "hireEmployee", {});
    else call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
    gs.g_ui_59ca54 = 1;
  }
  return 0;
}

// drawEmployeeViewPanel (x, y): the detail panel while it is open; else the golfer list: every golfer on the
// course, or the partner of one that is, newest first, past the scroll offset g_ui_5aa6b8[4], laid out in columns (6 rows
// of 14 px while g_zoomScale < 4, else 4 rows of 21 px), into g_employeeViewList; the scroll offset drops by 0x10 when it
// passed the count
int32_t draw_employee_view_panel(Ctx &ctx, int32_t x, int32_t y) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_view_panel);
  if (gs.g_ui_561254 != 0) return draw_employee_detail_panel(ctx, x, y);
  const Memory &m = ctx.mem;
  const Ptr<int32_t> out0 = state_arr(ctx, &GameState::g_employeeViewList);
  for (int32_t k = 0; k < 0x98; ++k) (out0 + k).set(0);
  const uint32_t golfers = state_va(ctx, &GameState::g_golfers);
  const auto rec = [&](int32_t g) { return golfers + (u32(g) << 8); };
  int32_t row = 0, d = 0x145, count = 0;
  Ptr<int32_t> out = out0;
  for (int32_t i = 0; i < 0x98; ++i) {
    const int32_t g = mod32(add32(sub32(gs.g_lastAllocatedGolfer, i), 0x98), 0x98);
    const bool active = static_cast<int8_t>(m.load<uint8_t>(rec(g) + 0x21)) > 0;
    bool show = false;
    const uint16_t p = m.load<uint16_t>(rec(g) + 0xa2);
    if (p != 0xffff) {
      const int32_t q = static_cast<int16_t>(p);
      if (m.load<int16_t>(rec(q) + 0xa2) == g && static_cast<int8_t>(m.load<uint8_t>(rec(q) + 0x21)) > 0) show = true;
    }
    if (!show && !active) continue;
    ++count;
    if (count <= gs.g_ui_5aa6b8[4]) continue;
    const bool compact = gs.g_zoomScale < 4;
    const int32_t old = row++;
    if (old == (compact ? 6 : 4)) {
      row = 1;
      d = add32(d, compact ? 0x3c : 0x79);
      if (d > (compact ? 0x2f3 : (mode == 1 ? 0x2af : 0x2b7))) continue;
    }
    out.set(g);
    out = out + 1;
  }
  if (count != 0 && gs.g_ui_5aa6b8[4] > count) gs.g_ui_5aa6b8[4] = sub32(gs.g_ui_5aa6b8[4], 0x10);
  return 0;
}

// drawGolferActionPanel (x, y): the detail panel while it is open and the panel mode is not 3; the rest is
// drawing (drawMatchScoreboard and tileType included: they write nothing)
int32_t draw_golfer_action_panel(Ctx &ctx, int32_t x, int32_t y) {
  if (ctx.gs().g_ui_561254 != 0 && ctx.gs().g_panelListMode != 3) return draw_employee_detail_panel(ctx, x, y);
  return 0;
}

// tickMessage: the message box tick. A running delay g_ui_5694a0[1] counts down and returns. Otherwise the
// message text is copied into g_textBuffer, the open / close counter unk_005a9ccc moves (up to 3 while the mode byte is 1,
// down while not) and gives drawWindow's mode; then, unless g_flags bit 15 is set or g_date % 3 == 0, g_messageTimer counts
// down and clears the mode byte at 0. Returns what the original leaves in eax.
int32_t tick_message(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_tick_message);
  int32_t &delay = gs.g_ui_5694a0[1];
  if (delay != 0) return delay = sub32(delay, 1);
  cpy(ctx, GS(g_textBuffer), GS(unk_005a6d40));
  int32_t &count = gs.unk_005a9ccc;
  int32_t v = -1;
  if (gs.g_ui_569498 == 1) {
    if (count < (mode == 1 ? 4 : 3)) v = count = add32(count, 1);
  } else if (count != 0) {
    v = count;
    count = sub32(count, 1);
  }
  draw_window_effect(ctx, v);
  const uint32_t flags = gs.g_flags;
  if (flags & 0x8000u) return i32(flags);
  const int32_t date = gs.g_date;
  if (date % 3 == 0) return date / 3;
  const int32_t left = gs.g_messageTimer = sub32(gs.g_messageTimer, 1);
  if (left <= 0) gs.g_ui_569498 = 0;
  return left;
}
