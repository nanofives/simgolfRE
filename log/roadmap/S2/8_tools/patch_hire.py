"""S2.8: the hire dialog's drawing calls in the mirror's port/src/sim/commands_screens.inc (run from the mirror root)."""
import pathlib
import re

p = pathlib.Path('port/src/sim/commands_screens.inc')
s = p.read_text()


def rep(old, new, count=1):
    global s
    pat = re.escape(old).replace("\\\n", "\n").replace("\n", "\n(?:[ \t]*\n)*")
    found = re.findall(pat, s)
    assert len(found) == count, (old[:80], len(found))
    s = re.sub(pat, lambda m: new, s)


rep('''int32_t show_hire_employee_dialog(Ctx &ctx) {
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
    else sel = -1;''', '''// Roadmap S2.8: with an attached backend each pass is drawn as c3en.cpp ShowHireEmployeeDialog_re draws it (the screen dimmed once
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
    static const uint32_t kHead[4][2] = {{0x64, 0x97}, {0x50, 0xde}, {0x40, 0x125}, {0x2c, 0x16c}};   // pool offset, y''')
rep('''    for (int32_t k = 0; k < 8; ++k) {
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
    }''', '''    for (int32_t k = 0; k < 8; ++k) {
      if ((k & 1) == 0) {
        y = static_cast<int32_t>(kHead[k / 2][1]);
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
    d.gx("Window::refreshFull", {d.window(), 0});''')
p.write_text(s)
print("patched")
