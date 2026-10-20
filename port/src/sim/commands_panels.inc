// commands.cpp part: the in-game panel click handlers H2 of mainLoop calls with the click position (g_rootInput[2..3]),
// and their hit tests. Specifications: the hook sources shim/src/re/c3fa.cpp (clickGolferPanel, clickEmployeeView,
// handleEmployeeAction), c3em.cpp (clickBuildTool433d30, pickBuilding, clickAmenityPanel, clickSlotPanel), c3g.cpp /
// c3z.cpp / c3l.cpp / c3be.cpp (the hit tests). Dropped: every Image_draw / drawWindow / Window::refreshFull (drawing).

// ---- hit tests (geometry over constants; pure except hitTestSlot432f90, which reads the palette sprites' heights) ----
// hitGolferPanel435f00: golfer-info-panel hotspot under (x, y) or -1; later matches win; 4..8 only in panel mode 3
int32_t hit_golfer_panel(Ctx &ctx, int32_t x, int32_t y) {
  const auto near = [&](int32_t cx, int32_t cy, int32_t r) { return approx_distance(sub32(x, cx), sub32(y, cy)) < r; };
  int32_t r = -1;
  if (near(0x111, 0x22f, 20)) r = 0;
  if (near(0x139, 0x207, 15)) r = 1;
  if (near(0x139, 0x224, 15)) r = 2;
  if (near(0x139, 0x244, 15)) r = 3;
  if (ctx.gs().g_panelListMode == 3) {
    if (near(0x19e, 0x1f9, 20)) r = 4;
    if (near(0x1ed, 0x1f9, 20)) r = 5;
    if (near(0x23c, 0x1f9, 20)) r = 6;
    if (near(0x28b, 0x1f9, 20)) r = 7;
    if (near(0x2da, 0x1f9, 20)) r = 8;
  }
  if (near(0x11e, 0x1ea, 15)) r = 9;
  if (near(0xe7, 0x21c, 15)) r = SG_NEGCTL(g_negctl_hit_golfer_panel) == 1 ? 9 : 10;
  return r;
}

// hitEmployeeSlot435570: the 4x4 employee-view grid (row + 4 * column) or one of five round buttons (-2..-6), else -1
int32_t hit_employee_slot(Ctx &, int32_t x, int32_t y) {
  int32_t r = -1;
  if (x >= 0x136 && y >= 0x1f2) r = add32(clamp(sub32(y, 0x1f2) / (SG_NEGCTL(g_negctl_hit_employee_slot) == 1 ? 20 : 21), 0, 3), mul32(clamp(sub32(x, 0x136) / 121, 0, 3), 4));
  if (approx_distance(sub32(x, 0x11d), sub32(y, 0x1ec)) < 20) r = -2;
  if (approx_distance(sub32(x, 0x100), sub32(y, 0x1fe)) < 20) r = -3;
  if (approx_distance(sub32(x, 0xe8), sub32(y, 0x21a)) < 20) r = -4;
  if (approx_distance(sub32(x, 0x14c), mul32(sub32(y, 0x250), 3)) < 20) r = -5;
  if (approx_distance(sub32(x, 0x304), mul32(sub32(y, 0x250), 3)) < 20) r = -6;
  return r;
}

// hitEmployee436b00: the employee detail panel: two fixed buttons (-2, -3), then the first entry of the point table
// f_tab_point16_4c7af0 (until x == -1) that contains the point (entries 2 and 3 use the wide test)
int32_t hit_employee(Ctx &ctx, int32_t x, int32_t y) {
  int32_t local = -1;
  if (approx_distance(sub32(x, 0x11e), sub32(y, 0x1ec)) < 0x10) local = -2;
  if (approx_distance(sub32(x, 0x100), sub32(y, 0x1fe)) < 0x10) local = -3;
  Ptr<int16_t> p = state_arr(ctx, &GameState::f_tab_point16_4c7af0).as<int16_t>();
  int32_t idx = 0;
  do {
    const int32_t px = *p, py = *(p + 1);
    if (idx == 2 || idx == 3) {
      if (approx_distance(sub32(mul32(sub32(x, px), 4), 0x20), sub32(sub32(y, py), 0x1e)) < 0x1e) return SG_NEGCTL(g_negctl_hit_employee) == 1 ? idx + 1 : idx;
    } else {
      if (approx_distance(sub32(sub32(x, idx < 8 ? 0x10 : 0x20), px), sub32(sub32(y, py), 0x10)) < 0x10) return idx;
    }
    p = p + 2;
    ++idx;
  } while (*p != -1);
  if (local == 1) local = 0;
  return local;
}

// hitTestSlot432f90: two fixed hotspots (0x16, 0x15), then the first toolbar slot whose box contains the point: x in
// [xmin, xmin + 0x3e) from f_tab_point16_4c79b4, y in [top - 0x2c, top) with top = the slot sprite's height field
// (g_sprites570cb8 + 0x1c + 0xb0 i) - 0xa + the table's y; the walk ends after the slot whose byte f_tab_bytes_4c79a0[i + 1]
// is 0xff
int32_t hit_test_slot(Ctx &ctx, int32_t x, int32_t y) {
  int32_t ebx = -1;
  if (approx_distance(sub32(x, 0xed), sub32(y, 0x20d)) < 0x14) ebx = 0x16;
  if (approx_distance(sub32(x, 0x105), sub32(y, 0x242)) < 0xf) ebx = 0x15;
  const Ptr<int16_t> a = state_arr(ctx, &GameState::f_tab_point16_4c79b4).as<int16_t>();
  const Ptr<uint8_t> term = state_arr(ctx, &GameState::f_tab_bytes_4c79a0).as<uint8_t>() + 1;
  const uint32_t heights = state_va(ctx, &GameState::g_sprites570cb8) + 0x1c;
  for (int32_t i = 0;; ++i) {
    const int32_t xmin = *(a + 2 * i);
    if (x >= xmin && x < add32(xmin, SG_NEGCTL(g_negctl_hit_test_slot) == 1 ? 0x3f : 0x3e)) {
      const int32_t base = sub32(ctx.mem.load<int32_t>(heights + u32(mul32(i, 0xb0))), 0xa);
      const int32_t top = add32(base, *(a + (2 * i + 1)));
      if (y >= sub32(top, 0x2c) && y < top) return i;
    }
    if (*(term + i) == 0xff) return ebx;
  }
}

// pickAmenity434980: ten round hotspots, later tests win (no hotspot returns 8)
int32_t pick_amenity(Ctx &, int32_t x, int32_t y) {
  const auto near = [&](int32_t cx, int32_t cy, int32_t r) { return approx_distance(sub32(x, cx), sub32(y, cy)) < r; };
  int32_t r = -1;
  if (near(0x10d, 500, 16)) r = -2;
  if (near(0x12e, 0x233, 20)) r = 0;
  if (near(0x14d, 0x20e, 20)) r = 1;
  if (near(0x16b, 0x233, 20)) r = 2;
  if (near(0x1d6, 0x23f, 18)) r = 3;
  if (near(0x24f, 0x23f, 18)) r = 4;
  if (near(0x28c, 0x23f, 18)) r = 5;
  if (near(0x2ca, 0x23f, 18)) r = 6;
  if (near(0xf8, 0x237, 12)) r = 7;
  if (near(0x213, 0x244, 12)) r = SG_NEGCTL(g_negctl_pick_amenity) == 1 ? 7 : 9;
  return r;
}

// hitBuildTool433c60: every anchor tested, later hits win; the last anchor (-3) returns at once
int32_t hit_build_tool(Ctx &, int32_t x, int32_t y) {
  int32_t r = -1;
  if (approx_distance(sub32(x, 0x10e), sub32(y, 0x1f3)) < 0x10) r = -2;
  const int32_t ey = sub32(y, 0x223);
  if (approx_distance(sub32(x, 0x160), ey) < 0x19) r = 0;
  if (approx_distance(sub32(x, 0x1d3), ey) < 0x19) r = 1;
  if (approx_distance(sub32(x, 0x24b), ey) < 0x19) r = 2;
  if (approx_distance(sub32(x, 0x2cb), ey) < 0x10) r = 3;
  if (approx_distance(sub32(x, 0x10b), sub32(y, 0x231)) < 0xc) return SG_NEGCTL(g_negctl_hit_build_tool) == 1 ? -2 : -3;
  return r;
}

// hitBuildingTool: two anchors (-2, -3) returned at once, then nine cells on a running offset (y doubled)
int32_t hit_building_tool(Ctx &, int32_t x, int32_t y) {
  if (approx_distance(sub32(x, 0xec), sub32(y, 0x20e)) < 0x10) return -2;
  if (approx_distance(sub32(x, 0x115), sub32(y, 0x237)) < 0xc) return -3;
  int32_t sx = sub32(x, 0x17f), sy = sub32(mul32(y, 2), 0x426);
  for (int32_t i = 0; i < 9; ++i) {
    if (approx_distance(sx, sy) < 0x28) return SG_NEGCTL(g_negctl_hit_building_tool) == 1 && i == 8 ? 7 : i;
    if (i == 4) {
      sy = sub32(sy, 0x50);
      sx = add32(sx, 0x118);
    } else {
      sx = sub32(sx, 0x50);
    }
  }
  return -1;
}

// ---- click handlers ----
// handleEmployeeAction (x, y): the employee detail panel. Returns -1 on every path.
int32_t employee_action(Ctx &ctx, int32_t r) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_employee_action);
  if (r == -3) {
    gs.g_ui_561254 = 0;
    gs.g_panelListMode = 4;
    return -1;
  }
  if (r == -2) {
    gs.g_ui_561254 = 0;
    gs.g_panelListMode = 2;
    return -1;
  }
  int32_t &sel = gs.g_employeePanelMode;
  const uint32_t walkers = state_va(ctx, &GameState::g_walkers);
  if (u32(r) <= 6) {
    switch (r) {
      case 0:
      case 1:
        sel = -1;
        gs.g_ui_59ca54 = 0;
        return -1;
      case 2:
        if (gs.g_ui_5aa558 == 0) return -1;
        sel = -1;
        gs.g_ui_5aa558 = sub32(gs.g_ui_5aa558, 2);
        return -1;
      case 3:
        sel = -1;
        gs.g_ui_5aa558 = add32(gs.g_ui_5aa558, 2);
        return -1;
      default: break;
    }
    if (gs.g_ui_59ca54 == 0 || sel == -1) return -1;
    const int32_t g = *(state_arr(ctx, &GameState::g_employeeDetailList) + sel);
    if (r == 4) {
      gs.f_w_4c2e00[4] = g;
    } else if (r == 6) {
      // rename: the walker's name into the text buffer, the modal text editor, the result copied into the record +0x2a
      const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
      ctx.mem.store<uint8_t>(text, 0);
      call_peer(ctx, "appendWalkerName", {u32(g)});
      call_peer(ctx, "textInputBox", {state_va(ctx, &GameState::f_strpool_4c80d0) + 0x2ac, 0xc8, 0x1b0, 0x1f});
      cpy(ctx, walkers + u32(mul32(g, 0x4c)) + 0x2a, text);
    } else {  // 5: fire the walker
      const uint32_t rec = walkers + u32(mul32(g, 0x4c));
      gs.g_cash_div100 = sub32(gs.g_cash_div100, mode == 1 ? 0x18 : 0x19);
      ctx.mem.store<uint8_t>(rec + 0x12, 0);
      call_peer(ctx, "pointsPopup", {u32(-0x19), ctx.mem.load<uint32_t>(rec), ctx.mem.load<uint32_t>(rec + 4), u32(-1)});
      const int32_t t = gs.g_year;
      gs.f_w_4c2e00[4] = -1;
      const uint32_t fy = state_va(ctx, &GameState::g_financeYears) + 0xa + u32(mul32(t % 100, 20));
      ctx.mem.store<int16_t>(fy, static_cast<int16_t>(ctx.mem.load<int16_t>(fy) - 0x19));
    }
    return -1;
  }
  if (r < 8) return -1;
  sel = r - 8;
  if (sel >= gs.g_ui_5aa6b8[2]) {
    sel = -1;
    return -1;
  }
  const uint32_t rec = walkers + u32(mul32(*(state_arr(ctx, &GameState::g_employeeDetailList) + sel), 0x4c));
  gs.gCameraTileX = sar32(ctx.mem.load<int32_t>(rec), 10);
  gs.gCameraTileY = sar32(ctx.mem.load<int32_t>(rec + 4), 10);
  return -1;
}

// clickEmployeeView (x, y): the employee view; with the detail panel open (g_ui_561254) it forwards to
// handleEmployeeAction. A grid slot selects its golfer (g_msgState53df54 = e + 1 when the record word +8 is not -1 or the
// state is already positive) and centres the camera on it; -3..-6 switch the panel mode / open the detail panel / scroll.
int32_t employee_view_action(Ctx &ctx, int32_t r) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_employee_view);
  if (r >= 0) {
    const int32_t e = *(state_arr(ctx, &GameState::g_employeeViewList) + r);
    if (e != -1) {
      const uint32_t rec = state_va(ctx, &GameState::g_golfers) + (u32(e) << 8);
      if (ctx.mem.load<int32_t>(rec + 8) != -1 || gs.g_msgState53df54 > 0) gs.g_msgState53df54 = add32(e, 1);
      gs.gCameraTileX = sar32(ctx.mem.load<int32_t>(rec), mode == 1 ? 9 : 10);
      gs.gCameraTileY = sar32(ctx.mem.load<int32_t>(rec + 4), 10);
    }
  }
  if (r == -3) {
    gs.g_panelListMode = 4;
  } else if (r == -4) {
    gs.g_ui_59ca54 = 1;
    gs.g_ui_561254 = 1;
  } else if (r == -5) {
    if (gs.g_ui_5aa6b8[4] != 0) gs.g_ui_5aa6b8[4] = sub32(gs.g_ui_5aa6b8[4], 0x10);
  } else if (r == -6) {
    gs.g_ui_5aa6b8[4] = add32(gs.g_ui_5aa6b8[4], 0x10);
  }
  return -1;
}

// clickGolferPanel (x, y): the golfer info / action panel (three Image_draw calls dropped; none writes state,
// so the flag word is read once). 0 opens the character editor, 1 / 2 set the shot-preview flags, 3 returns the command
// 0x4a, 4..8 choose the player's shot style, 9 panel mode 2, 10 opens the employee detail panel.
int32_t golfer_panel_action(Ctx &ctx, int32_t spot) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_golfer_panel);
  const uint32_t s = u32(spot);
  const uint32_t f = gs.g_flags;
  const int32_t n = gs.g_holesPlusOne;
  switch (s) {
    case 0: {
      const uint32_t golfers = state_va(ctx, &GameState::g_golfers);
      const int32_t t = ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_golferTypes) + 0x2c);
      ctx.mem.store<uint8_t>(golfers + 0x9818, 0x20);
      ctx.mem.store<int16_t>(golfers + 0x98b6, 0);
      ctx.mem.store<uint8_t>(golfers + 0x9819, static_cast<uint8_t>(t ? (t & 7) : 7));
      call_peer(ctx, "showCharacterEditor", {0, 0x98});
      return -1;
    }
    case 1:
      if (n > 1 && (f & ((1u << 26) | (1u << 21))) == 0 && gs.g_playerGolfer == -1) gs.g_flags = (f & ~0x2000u) | 0x4000u;
      return -1;
    case 2:
      if ((f & 0x2000u) && n > 1 && gs.g_proChallenger != -1 && gs.g_playerGolfer == -1 && (f & ((1u << 26) | (1u << 21))) == 0)
        gs.g_flags = f | 0x4000u;
      return -1;
    case 3:
      if ((f & (1u << 22)) && (f & ((1u << 26) | (1u << 21))) == 0 && gs.g_playerGolfer == -1) return 0x4a;
      return -1;
    case 4: gs.g_playerShotStyle = 0; return -1;
    case 5: gs.g_playerShotStyle = -1; return -1;
    case 6: gs.g_playerShotStyle = 1; return -1;
    case 7: gs.g_playerShotStyle = mode == 1 ? 4 : 3; return -1;
    case 8: gs.g_playerShotStyle = 4; return -1;
    case 9: gs.g_panelListMode = 2; return -1;
    case 10:
      gs.g_ui_59ca54 = 1;
      gs.g_ui_561254 = 1;
      return -1;
    default: return -1;
  }
}

// clickBuildTool433d30 (x, y): the terrain tool palette. -2 closes it, -3 shows the help window (then waits for a click),
// a tool index selects it (flag bit 1) and returns the flag word; -1 is returned unchanged.
int32_t build_tool_action(Ctx &ctx, int32_t r) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_build_tool);
  if (r == -2) {
    gs.g_buildPanelPick = 0;
    gs.g_panelListMode = 1;
    gs.g_flags &= ~2u;
    return -1;
  }
  if (r == -3) {
    cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c7ec4));
    gs.f_w_4c2e00[2] = -0x16;
    cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);
    draw_window_effect(ctx, -2);
    gs.g_flags &= ~2u;
    gs.g_buildPanelPick = 0;
    gs.g_panelListMode = 1;
    return pump_and_redraw(ctx, 0, "clickBuildTool433d30");
  }
  if (r >= 0) {
    gs.g_ui_542f20 = u32(r);
    gs.f_w_4c2848[0] = -1;
    gs.g_flags |= mode == 1 ? 4u : 2u;
    return i32(gs.g_flags);
  }
  return r;
}

// pickBuilding (x, y): the building palette (forwards to clickBuildTool433d30 when the terrain tools are
// open). A cell selects building r + 6 as the active tool (f_w_4c2848[3]) unless it is refused: an existing unique one
// (g_objectTypeLevel[r + 6] >= 2), a slot whose holes are too few (bucketValue(holes - 1) < 2), or one not unlocked yet
// (index >= g_buildingUnlockLevel); a refusal plays the refusal sound, posts the reason (startMessage) and clears the tool.
// Returns g_panelState[5] on that path.
int32_t building_action(Ctx &ctx, int32_t r) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_pick_building);
  if (r == -3) {
    cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c7ec4));
    gs.f_w_4c2e00[2] = -0x16;
    cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);
    draw_window_effect(ctx, -2);
    pump_and_redraw(ctx, 0, "pickBuilding");
    return 0;
  }
  if (r == -2) {
    gs.g_buildPanelPick = 1;
    gs.g_panelListMode = 1;
    return 0;
  }
  if (r == -1) return -1;
  const Ptr<int32_t> level = state_arr(ctx, &GameState::g_objectTypeLevel);
  const int32_t slot = *(level + (r + 6));
  int32_t index = r + 6;
  gs.f_w_4c2848[3] = index;
  bool refuse = false;
  if (slot != 0) {
    if (bucket_value(sub32(gs.g_holesPlusOne, 1)) < 2) refuse = true;
    else index = gs.f_w_4c2848[3];
  }
  if (!refuse && *(level + index) >= (mode == 1 ? 3 : 2)) refuse = true;
  if (refuse) {
    call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
    const int32_t cur = gs.f_w_4c2848[3];
    if (*(level + cur) >= 2) cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c80d0));
    else cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c8084));
    start_message(ctx, i32(0x80006000u), 1, -1);
    index = -1;
    gs.f_w_4c2848[3] = -1;
  }
  if (index >= gs.g_buildingUnlockLevel) {
    call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
    cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c7f1c) + 0x120);
    start_message(ctx, i32(0x80006000u), 1, -1);
    gs.f_w_4c2848[3] = -1;
  }
  return gs.g_panelState[5];
}

// clickAmenityPanel (x, y): the amenities / landmarks palette; cases 5, 6 and 9 draw the variant with
// Random::range(4 / 7 / 8); 7 shows the help window
int32_t amenity_action(Ctx &ctx, int32_t r) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_amenity_panel);
  int32_t &tool = gs.f_w_4c2848[3];
  switch (r) {
    case 0: tool = 3; return -1;
    case 1: tool = 0; return -1;
    case 2: tool = gs.g_ui_56d1a8[2] > 0 ? 5 : -1; return -1;
    case 3:
      gs.g_panelState[4] = 0;
      gs.g_panelState[0] = 0;
      tool = 1;
      return 0;
    case 4:
      tool = 4;
      gs.g_panelState[4] = -1;
      gs.g_panelState[0] = -1;
      return -1;
    case 5:
    case 6:
    case 9: {
      tool = r == 5 ? 2 : r == 6 ? 0x10 : 0x13;
      const int32_t v = rng_range(ctx, r == 5 ? (mode == 1 ? 5 : 4) : r == 6 ? 7 : 8);
      gs.g_panelState[4] = v;
      gs.g_panelState[0] = v;
      return v;
    }
    case 7:
    case 8:
      cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c7ec4));
      gs.f_w_4c2e00[2] = -0x16;
      cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);
      draw_window_effect(ctx, -2);
      pump_and_redraw(ctx, 0, "clickAmenityPanel");
      return gs.g_panelState[5];
    default: break;
  }
  if (r == -2) {
    gs.g_buildPanelSlot = 0;
    gs.g_panelListMode = 0;
  }
  const int32_t m = tool;
  if (m == 4 || m == 2 || m == 1 || m == 0x10 || m == 0x13) {
    const int32_t sel = gs.g_panelState[0];
    if (sel != -1 || m == 4) {
      gs.g_panelState[4] = sel;
      return sel;
    }
  }
  return 0;
}

// clickSlotPanel (x, y): the course-building toolbar (forwards to clickAmenityPanel when that palette is
// open). A slot sets the tool f_w_4c2848[0] from f_tab_bytes_4c79a0 (signed), 0x14 clears it, 0x15 shows the help window,
// 0x16 opens the amenities palette. Every path ends with Random::range(3), which it returns.
int32_t slot_action(Ctx &ctx, int32_t r) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_slot_panel);
  if (r != -1) {
    if (r < 0x14) {
      gs.f_w_4c2848[0] = static_cast<int8_t>(*(state_arr(ctx, &GameState::f_tab_bytes_4c79a0) + (mode == 1 ? r + 1 : r)));
    } else if (r == 0x14) {
      gs.f_w_4c2848[0] = -1;
    } else if (r == 0x15) {
      cpy(ctx, GS(g_textBuffer), GS(f_strpool_4c7ec4));
      gs.f_w_4c2e00[2] = -0x16;
      cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);
      draw_window_effect(ctx, -2);
      pump_and_redraw(ctx, 0, "clickSlotPanel");
    } else if (r == 0x16) {
      gs.g_buildPanelSlot = 1;
      gs.g_panelListMode = 0;
    }
  }
  return mode == 2 ? 0 : rng_range(ctx, 3);
}

// ---- the click handlers: the forwarding test, the hit test, the action (the semantic Command kinds run the action) ----
int32_t handle_employee_action(Ctx &ctx, int32_t x, int32_t y) { return employee_action(ctx, hit_employee(ctx, x, y)); }
int32_t click_employee_view(Ctx &ctx, int32_t x, int32_t y) {
  if (ctx.gs().g_ui_561254 != 0) return handle_employee_action(ctx, x, y);
  return employee_view_action(ctx, hit_employee_slot(ctx, x, y));
}
int32_t click_golfer_panel(Ctx &ctx, int32_t x, int32_t y) {
  if (ctx.gs().g_ui_561254 != 0) return handle_employee_action(ctx, x, y);
  return golfer_panel_action(ctx, hit_golfer_panel(ctx, x, y));
}
int32_t click_build_tool(Ctx &ctx, int32_t x, int32_t y) { return build_tool_action(ctx, hit_build_tool(ctx, x, y)); }
int32_t pick_building(Ctx &ctx, int32_t x, int32_t y) {
  if (ctx.gs().g_buildPanelPick != 0) return click_build_tool(ctx, x, y);
  return building_action(ctx, hit_building_tool(ctx, x, y));
}
int32_t click_amenity_panel(Ctx &ctx, int32_t x, int32_t y) { return amenity_action(ctx, pick_amenity(ctx, x, y)); }
int32_t click_slot_panel(Ctx &ctx, int32_t x, int32_t y) {
  if (ctx.gs().g_buildPanelSlot != 0) return click_amenity_panel(ctx, x, y);
  return slot_action(ctx, hit_test_slot(ctx, x, y));
}
