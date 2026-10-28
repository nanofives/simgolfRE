# S3.6 shared-file edits, exact text (log/roadmap/S3/6_merge.md section 1 lists the same replacements). Read by
# log/roadmap/S3/6_mirror.py, which applies each (file, old, new) with an exact-match check (CRLF-aware).
# the drop that hands a commands_layout.inc panel's drawing to an attached backend (edit 7)
DROP = ("  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {   // roadmap S3.6: the drawing (gfx_hud.cpp)\n"
        "    const uint32_t d[2] = {u32(x), u32(y)};\n"
        "    gfx_drop(ctx, \"%s.draw\", d, 2);\n"
        "  }\n")
EDITS = [
    # 0a. gfx.cpp load_image: the name test, then the same body over a host string (load_image_named), so a family can load a
    # path it builds itself without writing it into the address space (gfx_hud.cpp's LoadHeads)
    ("src/sim/gfx.cpp",
     """int load_image(Gfx &g, Ctx &ctx, uint32_t name_va, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img) {
  if (name_va == 0) return 3;
  if ((flags & 1) && pal != 0) return 3;
  std::string name = vstring(ctx.mem, name_va);
""",
     """int load_image(Gfx &g, Ctx &ctx, uint32_t name_va, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img) {
  if (name_va == 0) return 3;
  return load_image_named(g, ctx, vstring(ctx.mem, name_va), pal, ofs, count, flags, img);
}
int load_image_named(Gfx &g, Ctx &ctx, std::string name, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img) {
  if ((flags & 1) && pal != 0) return 3;
"""),
    # 0b. gfx_body.h: its declaration
    ("src/sim/gfx_body.h",
     """int load_image(Gfx &g, Ctx &ctx, uint32_t name_va, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img);
""",
     """int load_image(Gfx &g, Ctx &ctx, uint32_t name_va, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img);
// the same over a name the caller holds (roadmap S3.6)
int load_image_named(Gfx &g, Ctx &ctx, std::string name, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img);
"""),
    # 1. gfx.cpp: the header of the hud family
    ("src/sim/gfx.cpp",
     '#include "portsim/sim/commands.h"\n',
     '#include "portsim/sim/commands.h"\n#include "portsim/sim/gfx_hud.h"\n'),
    # 1b. gfx.cpp kBodies: loadInterfaceArt.images is the hud family's now (the whole image half; the core body modelled
    # its last loads only, which the family's body runs in the same order)
    ("src/sim/gfx.cpp",
     """    {"loadInterfaceArt.images", b_iart_images},
""",
     ""),
    # 1c. gfx.cpp: the core body is no longer registered (the integrator may delete it; kept here so the edit stays small)
    ("src/sim/gfx.cpp",
     "static void b_iart_images(Gfx &g, Ctx &ctx, const uint32_t *, int) {",
     "[[maybe_unused]] static void b_iart_images(Gfx &g, Ctx &ctx, const uint32_t *, int) {"),
    # 1d. gfx.cpp draw_g16: the shade table as a parameter (Sprite::drawG shades 0xf8 bytes through the table its caller
    # passes: g_color16Lut for blitPanel, g_halfBrightLut for the HUD's strips and boxes); the old entry keeps block 1
    ("src/sim/gfx.cpp",
     """void draw_g16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y) {
  const auto &lut = shade_lut();
  const int32_t l""",
     """void draw_g16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y) {
  draw_g16_lut(g, s, d, x, y, shade_lut().data(), shade_lut().size());
}
void draw_g16_lut(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y, const uint16_t *lut, size_t lut_n) {
  const int32_t l"""),
    ("src/sim/gfx.cpp",
     """        if (w >= 0x8000) {
          g.unsup("blitPanel: a destination word above the RGB555 range");""",
     """        if (w >= lut_n) {
          g.unsup("blitPanel: a destination word above the RGB555 range");"""),
    ("src/sim/gfx_body.h",
     """void draw_g16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y);
""",
     """void draw_g16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y);
// the same with the caller's shade table of lut_n words (roadmap S3.6)
void draw_g16_lut(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y, const uint16_t *lut, size_t lut_n);
"""),
    # 3. tick.cpp showMainRadialMenu fallback: its state steps unchanged; with a graphics backend attached the radial menu's
    # drawing is handed to the hud family afterwards (showMainRadialMenu.draw, with tip = the fallback reached the label copy)
    ("src/sim/tick.cpp",
     "int fb_show_main_radial_menu(TkBase &t, const uint32_t *a, int) {\n",
     "int fb_show_main_radial_menu_state(TkBase &t, const uint32_t *a, bool &tip) {\n"),
    ("src/sim/tick.cpp",
     "  t.GU8(t.kBuf) = 0;\n  if (!(static_cast<unsigned>(s) > 10)) {\n",
     "  t.GU8(t.kBuf) = 0;\n  tip = true;\n  if (!(static_cast<unsigned>(s) > 10)) {\n"),
    ("src/sim/tick.cpp",
     "  return 0;\n}\nint fb_draw_building_panel(",
     """  return 0;
}
// roadmap S3.6: the state steps above, then (a backend attached) the drawing, gfx_hud.cpp showMainRadialMenu.draw
int fb_show_main_radial_menu(TkBase &t, const uint32_t *a, int) {
  bool tip = false;
  const int r = fb_show_main_radial_menu_state(t, a, tip);
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {
    const uint32_t d[3] = {a[0], a[1], tip ? 1u : 0u};
    gfx_drop(t.ctx, "showMainRadialMenu.draw", d, 3);
  }
  return r;
}
int fb_draw_building_panel("""),
    # 4. gfx.cpp Window::refreshFull: the hud family's test-only background fill after the present (SG_GFX_HUD_BGFILL;
    # nothing without the variable)
    ("src/sim/gfx.cpp",
     "  g.back.px = g.root.px;\n",
     "  g.back.px = g.root.px;\n  gfxh::after_present(g, ctx);\n"),
    # 5. menu.cpp load_world_assets: an attached backend cuts the build palette's art (gfx_hud.cpp loadWorldAssets.images)
    ("src/sim/menu.cpp",
     "  gs.g_flag4c89a8 = 0;                         // loadWorldAssets+0x2c6d\n",
     "  gs.g_flag4c89a8 = 0;                         // loadWorldAssets+0x2c6d\n"
     "  // phase 5 (roadmap S3.6): an attached graphics backend cuts the build palette's images (gfx_hud.cpp)\n"
     "  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, \"loadWorldAssets.images\", nullptr, 0);\n"),
    # 6. commands_layout.inc show_info_cursor_panel: with a backend attached the panel's drawing (gfx_hud.cpp)
    ("src/sim/commands_layout.inc",
     "int32_t show_info_cursor_panel(Ctx &ctx, int32_t x, int32_t y) {\n",
     "int32_t show_info_cursor_panel(Ctx &ctx, int32_t x, int32_t y) {\n"
     "  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {   // roadmap S3.6: the drawing (gfx_hud.cpp)\n"
     "    const uint32_t d[2] = {u32(x), u32(y)};\n"
     "    gfx_drop(ctx, \"showInfoCursorPanel.draw\", d, 2);\n"
     "  }\n"),
    # 2. gfx.cpp b_surface_blit3: every case other than an 8-bit image onto the root at 1:1 goes to the hud family
    ("src/sim/gfx.cpp",
     """  auto it = g.images.find(a[0]);
  if (it == g.images.end() || a[1] != g.root_va(ctx)) {
    g.unsup("Surface_blit3: a source or destination the backend does not model");
    return;
  }
  const int32_t x = static_cast<int32_t>(a[2]), y = static_cast<int32_t>(a[3]), w = static_cast<int32_t>(a[4]),
                h = static_cast<int32_t>(a[5]), x2 = static_cast<int32_t>(a[6]), y2 = static_cast<int32_t>(a[7]),
                w2 = static_cast<int32_t>(a[8]), h2 = static_cast<int32_t>(a[9]);
  if (w != w2 || h != h2) {
    g.unsup("Surface_blit3: a scaled blit");
    return;
  }
  copy8(it->second, g.root, x, y, w, h, x2, y2);""",
     """  auto it = g.images.find(a[0]);
  if (it == g.images.end() || a[1] != g.root_va(ctx) || a[4] != a[8] || a[5] != a[9]) {
    // roadmap S3.6: a 16-bit source, another destination or a scaled rectangle (gfx_hud.cpp)
    if (gfxh::surface_blit3(g, ctx, a)) return;
    g.unsup("Surface_blit3: a source or destination the backend does not model");
    return;
  }
  const int32_t x = static_cast<int32_t>(a[2]), y = static_cast<int32_t>(a[3]), w = static_cast<int32_t>(a[4]),
                h = static_cast<int32_t>(a[5]), x2 = static_cast<int32_t>(a[6]), y2 = static_cast<int32_t>(a[7]);
  copy8(it->second, g.root, x, y, w, h, x2, y2);"""),
    # 7. commands_layout.inc: the bottom panel's other modes hand their drawing to an attached backend (gfx_hud.cpp
    # drawGolferActionPanel.draw is drawn; drawEmployeeViewPanel.draw / drawEmployeeDetailPanel.draw are reported)
    ("src/sim/commands_layout.inc",
     "int32_t draw_employee_detail_panel(Ctx &ctx, int32_t x, int32_t y) {\n",
     "int32_t draw_employee_detail_panel(Ctx &ctx, int32_t x, int32_t y) {\n" + DROP % "drawEmployeeDetailPanel"),
    ("src/sim/commands_layout.inc",
     "  if (gs.g_ui_561254 != 0) return draw_employee_detail_panel(ctx, x, y);\n",
     "  if (gs.g_ui_561254 != 0) return draw_employee_detail_panel(ctx, x, y);\n" + DROP % "drawEmployeeViewPanel"),
    ("src/sim/commands_layout.inc",
     "  if (ctx.gs().g_ui_561254 != 0 && ctx.gs().g_panelListMode != 3) return draw_employee_detail_panel(ctx, x, y);\n",
     "  if (ctx.gs().g_ui_561254 != 0 && ctx.gs().g_panelListMode != 3) return draw_employee_detail_panel(ctx, x, y);\n"
     + DROP % "drawGolferActionPanel"),
]
