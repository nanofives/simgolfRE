"""S2.10: the skill-point dialog's drawing calls in port/src/sim/dialogs.cpp (exact-anchor edits, applied to a tree root).

    ~/bin/py -3.12 log/roadmap/S2/10_tools/edit_dialogs.py <tree root>

Every inserted line runs only with a graphics backend attached (SkDraw::on): the GameState without one is untouched.
"""
import pathlib
import sys

EDITS = [
    ('#include "portsim/sim/commands.h"\n#include "portsim/sim/util.h"\n',
     '#include "portsim/sim/commands.h"\n#include "portsim/sim/gfx.h"\n#include "portsim/sim/util.h"\n'),
    # the bubble: the drawing of drawThoughtBubble (c3ei.cpp DrawThoughtBubble_re) around its two composeGolferSprite calls
    ("""// drawThoughtBubble (g, x, y, mode) without its drawing: composeGolferSprite(g) once, twice with mode 1 (its two sprite
// layers each compose the golfer); its other callees (typeBit7Clear, thoughtFlag) only read
void thought_bubble(Ctx &ctx, int32_t g, int32_t mode) {
  peer(ctx, "composeGolferSprite", {u32(g)});
  if (mode == 1) peer(ctx, "composeGolferSprite", {u32(g)});
}
""",
     """// Roadmap S2.10: the drawing calls of the skill dialog and of drawThoughtBubble, made only with a graphics backend attached
// (gfx_drop; c3em.cpp SkillPointDialog_re and c3ei.cpp DrawThoughtBubble_re in their order and with their arguments)
struct SkDraw {
  Ctx &ctx;
  const bool on;
  const uint32_t root;
  explicit SkDraw(Ctx &c)
      : ctx(c), on(g_gfx_attached.load(std::memory_order_relaxed) != 0), root(c.gs().f_stateptr_4c1570[0]) {}
  void gx(const char *name, std::initializer_list<uint32_t> a) const {
    if (on) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  }
  void stretch(uint32_t s, uint32_t mask, int32_t x, int32_t y) const { gx("Surface_stretchBlit", {s, mask, root, u32(x), u32(y), 0}); }
  void scaled(uint32_t s, int32_t x, int32_t y) const { gx("Surface_drawScaled", {s, root, u32(x), u32(y), 1, 1, 1, 0}); }
  void text(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawText4049d0", {t, u32(x), u32(y), c}); }
  void centred2(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawCenteredText2", {t, u32(x), u32(y), c}); }
};

// drawThoughtBubble (g, x, y, mode): composeGolferSprite(g) once, twice with mode 1 (its two sprite layers each compose the
// golfer); its other callees (typeBit7Clear, thoughtFlag) only read. With `draw` (roadmap S2.10) and a backend attached, the
// drawing between: mode 0 a white box (Win_fillRect at (x + 4, y), 0x50 x 0x60); mode != 1 the head g_sprites5628e8 cell
// 3 * (look + 72 * typeBit7Clear) + thoughtFlag at (x, y); mode 1 the hat (type record +0x2c bit 3) at (x + 0x10, y + 0x4c),
// the body g_surfaces5439e0 cell (hair column + 4 with the hat, + 8 * typeBit7Clear) at (x + 0x13, y + 0x4c) in the composed
// palette g_images[0], and the head g_sprites59e7c0 (look below 0x14: 3 * (look + 20 * typeBit7Clear) + thoughtFlag, at (x,
// y - 0x14)) or g_sprites5628e8 (at (x - 0x14, y - 0x14)).
void thought_bubble(Ctx &ctx, int32_t g, int32_t mode, int32_t x = 0, int32_t y = 0, bool draw = false) {
  const SkDraw d(ctx);
  const bool on = draw && d.on;
  const Memory &m = ctx.mem;
  const uint32_t T = GS(g_golferTypes) + u32(mul32(m.load<int16_t>(rec(ctx, g) + 0xb6), 0x230));
  const int32_t look = m.load<int8_t>(T + 0x22);
  const int32_t col = (m.load<uint8_t>(T + 0x23) >> 4) & 3;
  auto tb7 = [&] { return i32(call_peer(ctx, "typeBit7Clear", {u32(g)}).eax); };
  auto tf = [&] { return i32(call_peer(ctx, "thoughtFlag", {u32(g)}).eax); };
  if (on && mode == 0) d.gx("Win_fillRect", {d.root, u32(x + 4), u32(y), 0x50, 0x60, 0x80007fffu});
  peer(ctx, "composeGolferSprite", {u32(g)});
  if (on) d.gx("composeGolferSprite", {u32(g)});
  if (mode != 1) {
    if (on) d.scaled(GS(g_sprites5628e8) + 0x2cu * u32(add32(mul32(3, add32(look, mul32(tb7(), 72))), tf())), x, y);
    return;
  }
  const bool hat = (m.load<uint8_t>(T + 0x2c) & 8) != 0;
  if (on && hat) d.gx("Image_draw", {GS(g_sprite5787a0), d.root, u32(x + 0x10), u32(y + 0x4c), 0});
  peer(ctx, "composeGolferSprite", {u32(g)});
  if (!on) return;
  d.gx("composeGolferSprite", {u32(g)});
  const int32_t e = add32(col + (hat ? 4 : 0), mul32(tb7(), 8));
  d.gx("Image_draw", {GS(g_surfaces5439e0) + 0x2cu * u32(e), d.root, u32(x + 0x13), u32(y + 0x4c), GS(g_images)});
  if (look < 0x14)
    d.scaled(GS(g_sprites59e7c0) + 0x2cu * u32(add32(mul32(3, add32(look, mul32(tb7(), 20))), tf())), x, y - 0x14);
  else
    d.scaled(GS(g_sprites5628e8) + 0x2cu * u32(add32(mul32(3, add32(look, mul32(tb7(), 72))), tf())), x - 0x14, y - 0x14);
}
"""),
    # the pass: frame, title, the golfer's backdrop, bubble and stand
    ("""  int32_t lx = 0, ly = 0;  // frame slots only the cursor read writes (zeroed-stack convention before it)
  for (;;) {
    if (xbase >= 0) {
      int32_t who = golfer;
      if (golfer == -1) {
        const uint32_t r = rec(ctx, 0x99);
        m.store<int16_t>(r + 0xb6, 0);
        m.store<int16_t>(r + 0xb8, static_cast<int16_t>(m.load<int8_t>(GS(g_golferTypes) + 0x21)));
        who = 0x99;
      }
      thought_bubble(ctx, who, 1);
    }
""",
     """  int32_t lx = 0, ly = 0;  // frame slots only the cursor read writes (zeroed-stack convention before it)
  const SkDraw d(ctx);
  const int32_t titleX = add32(xbase, 0xd2);
  for (;;) {
    if (xbase < 0) d.gx("drawFrameR3", {u32(xbase + 0x4e), 0x32, 0xd0, 0x13c, 1});
    else d.gx("drawFrameR3", {u32(xbase + 0x2e), 0x32, 0x140, 0x13c, 1});
    d.gx("setDrawTarget", {d.root, GS(g_font519928), 0, 0, 0});
    d.centred2(title, titleX, 0x3a, 0x80007fffu);
    if (xbase >= 0) {
      d.scaled(GS(g_sprite59bf98), xbase + 0xf7, 0x56);
      int32_t who = golfer;
      if (golfer == -1) {
        const uint32_t r = rec(ctx, 0x99);
        m.store<int16_t>(r + 0xb6, 0);
        m.store<int16_t>(r + 0xb8, static_cast<int16_t>(m.load<int8_t>(GS(g_golferTypes) + 0x21)));
        who = 0x99;
      }
      thought_bubble(ctx, who, 1, xbase + 0xff, 0x66, true);
      d.stretch(GS(g_surfaces5678b8) + 0xdc, GS(g_sprites56a7b8) + 0xdc, xbase + 0x146, 0x13e);
    }
"""),
    # the rows
    ("""    if (points > 0) read_cursor(ctx, lx, ly, "skillPointDialog");
    const int32_t colX = add32(xbase, 0x52);
    const int32_t arrowX = add32(xbase, 0x32);
    for (int32_t i = 0; i < 10; ++i) {
      const int32_t rowY = 0x5a + 0x18 * i;
      int32_t value;
      if (golfer != -1 && m.load<int16_t>(rec(ctx, golfer) + 0xb6) != 0)
        value = m.load<uint8_t>(rec(ctx, golfer) + 0xf0 + u32(i));
      else
        value = m.load<uint8_t>(skills + u32(i));
      if (lx < colX) {
        if (lx >= arrowX && ly > rowY && ly <= rowY + (mode == 4 ? 0xb : 0xc)) hover = i;
        if (lx < colX && lx >= arrowX && ly > rowY + 0xc && ly <= rowY + 0x18) hover = i + 10;
      }
      if (value != 0) {
        cpy(ctx, tb, value < 10 ? GS(f_strpool_4c5340) : GS(g_str4e9a84));
        cat(ctx, tb, itoa10(ctx, value * 10, GS(g_infoPanelNumBuf)));
        cat(ctx, tb, GS(f_strpool_4c60c4));
      }
    }
    if (points > 0) {
      cpy(ctx, tb, GS(f_strpool_4d3c3c) + 0x10);
      cat(ctx, tb, itoa10(ctx, left, GS(g_infoPanelNumBuf)));
      cat(ctx, tb, GS(f_strpool_4d3c3c));
    }
    if (points == -1) return -1;
""",
     """    if (points > 0) read_cursor(ctx, lx, ly, "skillPointDialog");
    d.gx("setDrawTarget", {d.root, GS(g_font51b360), 0, 0, 0});
    const int32_t colX = add32(xbase, 0x52);
    const int32_t arrowX = add32(xbase, 0x32);
    const uint32_t s5 = GS(g_sprites5a53c8), m5 = GS(g_sprites58b680);
    for (int32_t i = 0; i < 10; ++i) {
      const int32_t rowY = 0x5a + 0x18 * i;
      int32_t value;
      if (golfer != -1 && m.load<int16_t>(rec(ctx, golfer) + 0xb6) != 0)
        value = m.load<uint8_t>(rec(ctx, golfer) + 0xf0 + u32(i));
      else
        value = m.load<uint8_t>(skills + u32(i));
      d.stretch(s5 + 0x478, m5 + 0x478, colX, rowY);
      if (points > 0) d.stretch(s5 + 0x528, m5 + 0x528, arrowX, rowY);
      if (lx < colX) {
        if (lx >= arrowX && ly > rowY && ly <= rowY + (mode == 4 ? 0xb : 0xc)) {
          d.stretch(s5 + 0x554, m5 + 0x554, arrowX, rowY);
          hover = i;
        }
        if (lx < colX && lx >= arrowX && ly > rowY + 0xc && ly <= rowY + 0x18) {
          d.stretch(s5 + 0x580, m5 + 0x580, arrowX, rowY + 0xc);
          hover = i + 10;
        }
      }
      if (value != 0) {
        cpy(ctx, tb, value < 10 ? GS(f_strpool_4c5340) : GS(g_str4e9a84));
        cat(ctx, tb, itoa10(ctx, value * 10, GS(g_infoPanelNumBuf)));
        cat(ctx, tb, GS(f_strpool_4c60c4));
        d.text(tb, xbase + 0x57, rowY + 7, 0x80000000u);
      }
      d.text(m.load<uint32_t>(GS(f_ptrtab_4c2c3c) + 4 * u32(i)), xbase + 0x8a, rowY + 7, value != 0 ? 0x80000000u : 0x80004210u);
    }
    if (points > 0) {
      cpy(ctx, tb, GS(f_strpool_4d3c3c) + 0x10);
      cat(ctx, tb, itoa10(ctx, left, GS(g_infoPanelNumBuf)));
      cat(ctx, tb, GS(f_strpool_4d3c3c));
      d.centred2(tb, titleX, 0x50, used > points ? 0x80007d08u : 0x80007fffu);
    }
    if (points == -1) return -1;
    d.gx("Window::refreshFull", {GS(g_rootWindow), 0});
"""),
    # showCharacterIntro: drawWindow's drawing (the help text over the course) after its effect
    ("""  cm::draw_window_effect(ctx, -2);
  const int32_t r = skill_point_dialog(ctx, GS(g_golferTypes) + 0x10, 0, points, -1, 0xc8);""",
     """  cm::draw_window_effect(ctx, -2);
  SkDraw(ctx).gx("drawWindow.frame", {0xc8, 0x172, 0x80007fffu, u32(-2)});   // drawWindow(0xc8, 0x172, 0x80007fff, -2)
  const int32_t r = skill_point_dialog(ctx, GS(g_golferTypes) + 0x10, 0, points, -1, 0xc8);"""),
]


def main(root: str) -> int:
    p = pathlib.Path(root) / "port" / "src" / "sim" / "dialogs.cpp"
    raw = p.read_bytes()
    crlf = b"\r\n" in raw
    s = raw.decode("utf-8").replace("\r\n", "\n")
    for i, (old, new) in enumerate(EDITS, 1):
        n = s.count(old)
        if n != 1:
            print(f"edit {i}: anchor found {n} times")
            return 1
        s = s.replace(old, new)
    if crlf:
        s = s.replace("\n", "\r\n")
    p.write_bytes(s.encode("utf-8"))
    print(f"{p}: {len(EDITS)} edits applied")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
