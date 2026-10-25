"""S2.8: the histograph's drawing calls in the mirror's port/src/sim/commands_screens.inc (run from the mirror root)."""
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


rep('''int32_t show_histograph_screen(Ctx &ctx) {
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
  click_or_key_loop(ctx, true, "showHistographScreen");''', '''//
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
        [&](uint32_t t) { d.gx("drawCenteredText", {t, 0x2d7, u32(y), 0x80000000u}); });
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
    if (ctx.mem.load<uint32_t>(root + 4) != 0) d.gx("Surface::callBounds", {root});
    d.gx("Image_draw", {GS(g_spriteRef_820f40) + 0x84, root, 0x2c0, 0x227, 0});
    d.gx("Window::refreshFull", {d.window(), 0});
  }
  click_or_key_loop(ctx, true, "showHistographScreen");''')
p.write_text(s)
print("patched")
