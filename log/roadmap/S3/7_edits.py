# S3.7 shared-file edits, exact text (log/roadmap/S3/7_merge.md lists the same replacements). Applied by
# log/roadmap/S3/7_mirror.py after 6_edits.py / 6b_edits.py / 5_merge_edits.py, each (file relative to port/, old, new) with an
# exact-match check (CRLF-aware).
EDITS = [
    # 1. gfx.cpp: the text family's header
    ("src/sim/gfx.cpp",
     '#include "portsim/sim/gfx_scene.h"\n',
     '#include "portsim/sim/gfx_scene.h"\n#include "portsim/sim/gfx_text.h"\n'),
    # 2. gfx.cpp gfxb::draw_text_string: a text holding a markup delimiter or a tab, or a colour that is a run-palette index, is
    #    the text family's (drawRichText's markup scanner, the run drawer's tabs and palette colours); plain text is unchanged
    ("src/sim/gfx.cpp",
     "  for (char c : text)\n"
     "    if (c == '{' || c == '}' || c == '[' || c == ']' || c == '$' || c == '=' || c == '^' || c == '\\t') {\n"
     "      g.unsup(std::string(who) + \": markup or a tab in the text\");\n"
     "      return;\n"
     "    }\n"
     "  if (!(colour & 0x80000000u)) {\n"
     "    g.unsup(std::string(who) + \": a palette colour index\");\n"
     "    return;\n"
     "  }\n",
     "  // roadmap S3.7: markup, tabs and run-palette colours are the text family's (gfx_text.cpp gfxt::draw_string)\n"
     "  bool rich = !(colour & 0x80000000u);\n"
     "  for (char c : text)\n"
     "    if (c == '{' || c == '}' || c == '[' || c == ']' || c == '$' || c == '=' || c == '^' || c == '\\t') rich = true;\n"
     "  if (rich) {\n"
     "    gfxt::draw_string(g, ctx, text, x0, y0, colour, centre, who);\n"
     "    return;\n"
     "  }\n"),
    # 3. gfx.cpp: drawWindow.frame is the text family's whole drawWindow (golfer and picture windows, the slide, the shadowed
    #    message text); the core's sel == -1 / mode -2 body is no longer registered
    ("src/sim/gfx.cpp",
     "    {\"drawPanelBackdrop\", b_panel_backdrop},    {\"drawWindow.frame\", b_draw_window},\n",
     "    {\"drawPanelBackdrop\", b_panel_backdrop},\n"),
    ("src/sim/gfx.cpp",
     "static void b_draw_window(Gfx &g, Ctx &ctx, const uint32_t *a, int) {\n",
     "[[maybe_unused]] static void b_draw_window(Gfx &g, Ctx &ctx, const uint32_t *a, int) {\n"),
    # 4. commands: draw_window_effect takes the window's position (tickMessage's (0xf0, -8), the help windows' (0xc8, 0x154))
    #    and, with a backend attached, hands drawWindow's drawing over after the wrap, with h = 15 r + 0x20 of that wrap
    ("src/sim/commands_internal.h",
     "int32_t draw_window_effect(Ctx &ctx, int32_t mode);\n",
     "int32_t draw_window_effect(Ctx &ctx, int32_t mode, int32_t x = 0xf0, int32_t y = -8);\n"),
    ("src/sim/commands.cpp",
     "int32_t draw_window_effect(Ctx &ctx, int32_t mode) {\n"
     "  const int32_t sel = ctx.gs().f_w_4c2e00[2];\n"
     "  // phase 4: the window height H = 15 * wrapMessageBuffer(sel != -1 ? 0x25 : 0x2c) + 0x20 (at least 0x60 with a golfer) into\n"
     "  // g_value587ea0, which mainLoop reads, and the line breaks in g_textBuffer; with no TextMetrics both stay dropped (U-0942)\n"
     "  if (text_metrics(ctx)) ctx.gs().g_value587ea0 = window_height(ctx);\n",
     "int32_t draw_window_effect(Ctx &ctx, int32_t mode, int32_t x, int32_t y) {\n"
     "  const int32_t sel = ctx.gs().f_w_4c2e00[2];\n"
     "  // phase 4: the window height H = 15 * wrapMessageBuffer(sel != -1 ? 0x25 : 0x2c) + 0x20 (at least 0x60 with a golfer) into\n"
     "  // g_value587ea0, which mainLoop reads, and the line breaks in g_textBuffer; with no TextMetrics both stay dropped (U-0942)\n"
     "  if (text_metrics(ctx)) {\n"
     "    const int32_t r = wrap_message_buffer(ctx, sel != -1 ? 0x25 : 0x2c, state_va(ctx, &GameState::g_font51b360));\n"
     "    const int32_t h = add32(mul32(r, 15), 0x20);\n"
     "    ctx.gs().g_value587ea0 = sel != -1 && h < 0x60 ? 0x60 : h;\n"
     "    // roadmap S3.7: an attached graphics backend draws the window (gfx_text.cpp drawWindow.frame; a[4] = h)\n"
     "    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {\n"
     "      const uint32_t d[5] = {u32(x), u32(y), 0, u32(mode), u32(h)};\n"
     "      gfx_drop(ctx, \"drawWindow.frame\", d, 5);\n"
     "    }\n"
     "  }\n"),
    ("src/sim/commands_panels.inc",
     "    cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);\n"
     "    draw_window_effect(ctx, -2);\n"
     "    gs.g_flags &= ~2u;\n",
     "    cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);\n"
     "    draw_window_effect(ctx, -2, 0xc8, 0x154);\n"
     "    gs.g_flags &= ~2u;\n"),
    ("src/sim/commands_panels.inc",
     "    cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);\n"
     "    draw_window_effect(ctx, -2);\n"
     "    pump_and_redraw(ctx, 0, \"pickBuilding\");\n",
     "    cat(ctx, GS(g_textBuffer), GS(f_strpool_4c7de0) + 0x9c);\n"
     "    draw_window_effect(ctx, -2, 0xc8, 0x154);\n"
     "    pump_and_redraw(ctx, 0, \"pickBuilding\");\n"),
    # 5. gfx_hud.cpp (S3.6's file): the hover tips S3.6 reported are the text family's (U-1353)
    ("src/sim/gfx_hud.cpp",
     '#include "portsim/sim/gfx_scene.h"\n',
     '#include "portsim/sim/gfx_scene.h"\n#include "portsim/sim/gfx_text.h"\n'),
    ("src/sim/gfx_hud.cpp",
     "  if (sel == ip.prev && sel != -1 && sel < 0x14 && ip.count > 10) g.unsup(\"tooltip: showInfoCursorPanel's hover tip of a tool (frame, name, rating, story text)\");\n",
     "  if (sel == ip.prev && sel != -1 && sel < 0x14 && ip.count > 10) gfxt::info_panel_tool_tip(g, ctx, sel);   // roadmap S3.7\n"),
    ("src/sim/gfx_hud.cpp",
     "    if (sel >= 0x14 && ip.count > 10) g.unsup(\"tooltip: showInfoCursorPanel's hover tip of a slot (drawMapLine bar and text)\");\n",
     "    if (sel >= 0x14 && ip.count > 10) gfxt::info_panel_slot_tip(g, ctx, sel);   // roadmap S3.7\n"),
    ("src/sim/gfx_hud.cpp",
     "  if (L != -1 && ip.count > 10) g.unsup(\"tooltip: drawAmenitiesPanel's dwell tip (drawMapLine bars and centred text)\");\n",
     "  if (L != -1 && ip.count > 10) gfxt::amenities_tip(g, ctx, L, a, b);   // roadmap S3.7\n"),
    ("src/sim/gfx_hud.cpp",
     "  else if (M == 4 && mask != 0 && amenity_row(g, ctx, 0, 0x168, 0xe, 1, 0x64, -4, a, b, -1, mask, 0) != -1)\n"
     "    g.unsup(\"tooltip: drawAmenitiesPanel's hovered building row (two drawPanelBackdrop lines)\");\n",
     "  else if (M == 4 && mask != 0) {   // roadmap S3.7: the hovered row's two drawPanelBackdrop lines\n"
     "    const int32_t row = amenity_row(g, ctx, 0, 0x168, 0xe, 1, 0x64, -4, a, b, -1, mask, 0);\n"
     "    if (row != -1) gfxt::amenities_row_tip(g, ctx, row, a, b);\n"
     "  }\n"),
    ("src/sim/gfx_hud.cpp",
     "  if (h != -1 && ip.count > 10) g.unsup(\"tooltip: drawGolferActionPanel's dwell tip (drawPanelBackdrop)\");\n",
     "  if (h != -1 && ip.count > 10)   // roadmap S3.7\n"
     "    gfxt::action_panel_tip(g, ctx, h, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));\n"),
    # the employee panels' dwell tips: dwell() takes the tip's drawing as a callback
    ("src/sim/gfx_hud.cpp",
     "void dwell(Gfx &g, Ctx &ctx, int32_t hover, const char *tip) {\n",
     "void dwell(Gfx &g, Ctx &ctx, int32_t hover, const char *tip, const std::function<void()> &draw_tip) {\n"),
    ("src/sim/gfx_hud.cpp",
     "  if (hover != -1 && ip.count > 10) {\n"
     "    prec(g, \"tooltip\", {});\n"
     "    g.unsup(tip);\n"
     "  }\n",
     "  if (hover != -1 && ip.count > 10) {\n"
     "    prec(g, \"tooltip\", {});\n"
     "    (void)tip;\n"
     "    draw_tip();   // roadmap S3.7\n"
     "  }\n"),
    ("src/sim/gfx_hud.cpp",
     "  dwell(g, ctx, hover, \"tooltip: drawEmployeeViewPanel's dwell tip (drawMapLine bar and centred text)\");\n",
     "  dwell(g, ctx, hover, \"tooltip: drawEmployeeViewPanel's dwell tip (drawMapLine bar and centred text)\", [&]() {\n"
     "    gfxt::view_panel_tip(g, ctx, hover, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));\n"
     "  });\n"),
    ("src/sim/gfx_hud.cpp",
     "  dwell(g, ctx, hit, \"tooltip: drawEmployeeDetailPanel's dwell tip (drawPanelBackdrop)\");\n",
     "  dwell(g, ctx, hit, \"tooltip: drawEmployeeDetailPanel's dwell tip (drawPanelBackdrop)\", [&]() {\n"
     "    gfxt::detail_panel_tip(g, ctx, hit, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));\n"
     "  });\n"),
    # drawTerrainToolbar: the dwell counter is the shared one (InfoPanelState, defined further down: a helper declared here)
    ("src/sim/gfx_hud.cpp",
     "void b_terrain_toolbar(Gfx &g, Ctx &ctx, const uint32_t *a, int) {\n",
     "void toolbar_dwell(Gfx &g, Ctx &ctx, int32_t sel, int32_t a, int32_t b);   // roadmap S3.7 (after InfoPanelState)\n"
     "void b_terrain_toolbar(Gfx &g, Ctx &ctx, const uint32_t *a, int) {\n"),
    ("src/sim/gfx_hud.cpp",
     "  if (sel == ctx.mem.load<int32_t>(va(ctx, GSO(g_panelState, 4))) && sel != -1 &&\n"
     "      ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_ui_5aa554)) >= 10)\n"
     "    g.unsup(\"tooltip: drawTerrainToolbar's hover tip (drawPanelBackdrop)\");\n",
     "  toolbar_dwell(g, ctx, sel, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));\n"),
    ("src/sim/gfx_hud.cpp",
     "  return ip;\n"
     "}\n",
     "  return ip;\n"
     "}\n"
     "// drawTerrainToolbar's dwell (c3ei.cpp DrawTerrainToolbar_re: the counter pair g_panelState + 4 / g_ui_5aa554 the other panels\n"
     "// share): a new sel restarts it, the same one counts; after more than ten frames on a button the tip (roadmap S3.7)\n"
     "void toolbar_dwell(Gfx &g, Ctx &ctx, int32_t sel, int32_t a, int32_t b) {\n"
     "  InfoPanelState &ip = info_panel_state(g, ctx);\n"
     "  if (sel != ip.prev) {\n"
     "    ip.count = 0;\n"
     "    ip.prev = sel;\n"
     "    return;\n"
     "  }\n"
     "  ++ip.count;\n"
     "  if (sel != -1 && ip.count > 10) gfxt::terrain_toolbar_tip(g, ctx, sel, a, b);\n"
     "}\n"),
    # 7. a drop whose drawing MEASURES (Text_put: the frame code sizes the golfer labels' bars and places the thought bubbles by
    #    its width) gets the backend's value; without a backend the frame keeps the drop's 0 as before
    ("src/sim/gfx_body.h",
     "  int root_terrain = 0, back_terrain = 0;\n",
     "  int root_terrain = 0, back_terrain = 0;\n"
     "  // a value a body computed for the frame code (roadmap S3.7: Text_put's width), taken once by gfx_take_value\n"
     "  bool has_value = false;\n"
     "  int32_t value = 0;\n"),
    ("include/portsim/sim/gfx.h",
     "bool gfx_drop(Ctx &ctx, const char *name, const uint32_t *a, int n);\n",
     "bool gfx_drop(Ctx &ctx, const char *name, const uint32_t *a, int n);\n"
     "// tick.cpp: after gfx_drop, the value the body computed for the frame code (roadmap S3.7: Text_put's width); false when the\n"
     "// body set none (the frame keeps the drop's result)\n"
     "bool gfx_take_value(Ctx &ctx, int32_t &v);\n"),
    ("src/sim/gfx.cpp",
     "void gfx_end_of_input(Ctx &ctx) {\n",
     "bool gfx_take_value(Ctx &ctx, int32_t &v) {\n"
     "  Gfx *g = gfx(ctx);\n"
     "  if (!g || !g->has_value) return false;\n"
     "  g->has_value = false;\n"
     "  v = g->value;\n"
     "  return true;\n"
     "}\n"
     "\n"
     "void gfx_end_of_input(Ctx &ctx) {\n"),
    ("src/sim/tick.cpp",
     "      if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, name, a, n);\n"
     "      return 0;\n",
     "      if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {\n"
     "        gfx_drop(ctx, name, a, n);\n"
     "        int32_t v = 0;\n"
     "        if (gfx_take_value(ctx, v)) return v;   // roadmap S3.7: a measuring drop (Text_put)\n"
     "      }\n"
     "      return 0;\n"),
    # 6. gfx_prim_text.cpp: Widget_setQuad's words also reach the text family (drawStringRightAt draws in them)
    ("src/sim/gfx_prim_text.cpp",
     '#include "gfx_body.h"\n',
     '#include "gfx_body.h"\n#include "portsim/sim/gfx_text.h"\n'),
    ("src/sim/gfx_prim_text.cpp",
     "  st(g).quad[a[0]] = {mode() == 1 ? 0x80000000u : a[1], a[2], a[3], a[4]};\n",
     "  st(g).quad[a[0]] = {mode() == 1 ? 0x80000000u : a[1], a[2], a[3], a[4]};\n"
     "  gfxt::set_quad(g, a[0], 0, st(g).quad[a[0]][0], a[2], a[3], a[4]);   // roadmap S3.7\n"),
]
