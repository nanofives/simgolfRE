"""S2.10: the character editor's drawing calls in port/src/sim/chareditor.cpp (exact-anchor edits, applied to a tree root).

    ~/bin/py -3.12 log/roadmap/S2/10_tools/edit_chareditor.py <tree root>     (port/src/sim/chareditor.cpp under it)

Every inserted line runs only with a graphics backend attached (EdDraw::on): the GameState without one is untouched.
"""
import pathlib
import sys

EDITS = [
    # 1. includes
    ('#include "portsim/sim/commands.h"\n#include "portsim/sim/util.h"\n',
     '#include "portsim/sim/commands.h"\n#include "portsim/sim/gfx.h"\n#include "portsim/sim/util.h"\n'),
    # 2. the drawing helper, before show_character_editor's comment block
    ("// Entry: the tip state word cleared, the edited record copied to golfer type record 83",
     """// Roadmap S2.10: the editor's drawing (c3fx.cpp ShowCharacterEditor_re's callees in its order, with its arguments), made only
// with a graphics backend attached (gfx_drop; bodies in gfx.cpp, gfx_prim_text.cpp, gfx_prim_fill.cpp, gfx_dialogs.cpp,
// gfx_chareditor.cpp and, for the sprite cuts, the scaled draws and the golfer palette, the in-game families). The texts are the
// ones the port builds in g_textBuffer anyway; a draw never writes the GameState.
struct EdDraw {
  Ctx &ctx;
  const bool on;
  const uint32_t root, win;
  explicit EdDraw(Ctx &c)
      : ctx(c),
        on(g_gfx_attached.load(std::memory_order_relaxed) != 0),
        root(c.gs().f_stateptr_4c1570[0]),
        win(state_va(c, &GameState::g_rootWindow)) {}
  void gx(const char *name, std::initializer_list<uint32_t> a) const {
    if (on) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  }
  void font(uint32_t f) const { gx("setDrawTarget", {root, f, 0, 0, 0}); }
  // Surface_drawScaled(sprite, root, x, y, 1, 1, 1, 0)
  void scaled(uint32_t s, int32_t x, int32_t y) const { gx("Surface_drawScaled", {s, root, u32(x), u32(y), 1, 1, 1, 0}); }
  void image(uint32_t s, int32_t x, int32_t y, uint32_t pal = 0) const { gx("Image_draw", {s, root, u32(x), u32(y), pal}); }
  void centred(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawCenteredText", {t, u32(x), u32(y), c}); }
  void centred2(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawCenteredText2", {t, u32(x), u32(y), c}); }
  void text(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawText4049d0", {t, u32(x), u32(y), c}); }
  void fill(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) const {
    gx("fillWindowRect", {win, u32(x), u32(y), u32(w), u32(h), c});
  }
  // a control's spot (f_tab_point16_4c7b90: x, y words per control)
  int32_t spot(int32_t k, int32_t half) const {
    return ctx.mem.load<int16_t>(GS(f_tab_point16_4c7b90) + 4u * u32(k) + 2u * u32(half));
  }
};

// Entry: the tip state word cleared, the edited record copied to golfer type record 83"""),
    # 3. setup: the frame Image and the gender's background
    ("""  Scratch xy(ctx, 8);  // x and y: frame slots only View_toLocal writes (zeroed-stack convention before the first read)
""",
     """  Scratch xy(ctx, 8);  // x and y: frame slots only View_toLocal writes (zeroed-stack convention before the first read)
  const EdDraw d(ctx);
  // the frame Image and the background of the character's gender (Image_ctor, loadImage(path, 0, 0, 0x100, 2))
  d.gx("showCharacterEditor.setup", {pool(ctx, (B(R1 + 0x21) & 0x80) ? 0x218u : 0x1fcu)});
"""),
    # 4. the pass's background, the control's hover sprite and the name area's hover line, the figure
    ("""  for (;;) {
    view_to_local(ctx, xy.va, xy.va + 4, "showCharacterEditor");
    const int32_t x = m.load<int32_t>(xy.va), y = m.load<int32_t>(xy.va + 4);
    int32_t ctl = nearest_char_control(ctx, x, y);
    if (ctl == 0xf && (B(R1 + 0x21) & 0x80) == 0) ctl = -1;
    // (the name area's hover line only picks text colours)
    // ---- the figure
    call_peer(ctx, "composeGolferSprite", {u32(kPreview)});
""",
     """  for (;;) {
    d.gx("showCharacterEditor.background", {d.root});
    view_to_local(ctx, xy.va, xy.va + 4, "showCharacterEditor");
    const int32_t x = m.load<int32_t>(xy.va), y = m.load<int32_t>(xy.va + 4);
    int32_t ctl = nearest_char_control(ctx, x, y);
    if (ctl == 0xf && (B(R1 + 0x21) & 0x80) == 0) ctl = -1;
    // the name area's hover line (it only picks text colours) or the control's hover sprite
    int32_t hot = -1;
    if (d.on) {
      if (ctl == -1 || ctl == 0x14) {
        if (abs32(sub32(x, 0x9a)) < 0x50 && abs32(sub32(y, 0x40)) < 0x28) {
          const int32_t q = div32(sub32(y, 0x18), 0x14);
          if (u32(q) <= 4) hot = q;
        }
      } else {
        d.scaled(GS(g_sprites59df20) + 0x58u * u32(ctl), d.spot(ctl, 0), d.spot(ctl, 1));
      }
    }
    // ---- the figure
    if (d.on) {
      const uint32_t R = preview_rec();
      if (B(R + 0x2c) & 8) d.image(GS(g_sprite5787a0), 0x160, 0x74);
    }
    call_peer(ctx, "composeGolferSprite", {u32(kPreview)});
    if (d.on) {
      // the golfer's palette layers into g_images[0], the body sheet drawn with it, then the head (three expressions a row)
      d.gx("composeGolferSprite", {u32(kPreview)});
      const uint32_t R = preview_rec();
      const int32_t hs = (B(R + 0x23) >> 4) & 3, body = m.load<int8_t>(R + 0x22);
      const int32_t sheet = (B(R + 0x2c) & 8) ? hs + 4 : hs;
      const int32_t g = type_bit7_clear(ctx, kPreview);
      d.image(GS(g_surfaces5439e0) + 0x2cu * u32(add32(sheet, mul32(g, 8))), 0x163, 0x74, GS(g_images));
      const int32_t tf = i32(call_peer(ctx, "thoughtFlag", {u32(kPreview)}).eax);
      if (body < 0x14)
        d.scaled(GS(g_sprites59e7c0) + 0x2cu * u32(add32(mul32(add32(body, mul32(g, 0x14)), 3), tf)), 0x150, 0x14);
      else
        d.scaled(GS(g_sprites5628e8) + 0x2cu * u32(add32(mul32(add32(body, mul32(g, 0x48)), 3), tf)), 0x13c, 0x14);
      if (ctx.gs().g_graphicsDevice != 0) d.gx("Display::setScale", {1, 1, 1});
      d.image(GS(g_sprites59df20) + 0x6e0, 0x12b, 0xdc);
    }
"""),
    # 5. the name lines
    ("""    m.store<uint8_t>(tb, 0);
    call_peer(ctx, "buildGolferName", {u32(kPreview), 0});
    if (B(preview_rec() + 0x2c) & 8) cat(ctx, tb, pool(ctx, 500));
    cpy(ctx, tb, GS(f_strpool_4c59e0) + 4);
    cat(ctx, tb, preview_rec());
    cat(ctx, tb, GS(f_strpool_4c59e0));
    {
      const uint8_t st = B(P + 0xb8);
      m.store<uint8_t>(tb, 0);
      if (st & 8) cpy(ctx, tb, pool(ctx, 492));
      if (st & 0x10) cpy(ctx, tb, pool(ctx, 484));
      if (st & 0x20) cpy(ctx, tb, pool(ctx, 472));
      if (st & 0x40) cpy(ctx, tb, pool(ctx, 464));
    }
    {
      const uint8_t st = B(P + 0xb8);
      m.store<uint8_t>(tb, 0);
      if (st & 1) cpy(ctx, tb, pool(ctx, 456));
      if (st & 2) cpy(ctx, tb, pool(ctx, 444));
      if (st & 4) cpy(ctx, tb, pool(ctx, 436));
    }
""",
     """    d.font(GS(g_font519928));
    m.store<uint8_t>(tb, 0);
    call_peer(ctx, "buildGolferName", {u32(kPreview), 0});
    if (B(preview_rec() + 0x2c) & 8) cat(ctx, tb, pool(ctx, 500));
    d.centred2(tb, 0x9a, 0x18, hot != 0 ? 0x80000848u : 0x80007b20u);
    cpy(ctx, tb, GS(f_strpool_4c59e0) + 4);
    cat(ctx, tb, preview_rec());
    cat(ctx, tb, GS(f_strpool_4c59e0));
    d.centred2(tb, 0x9a, 0x30, hot != 1 ? 0x80000848u : 0x80007fffu);
    d.font(GS(g_font51b360));
    {
      const uint8_t st = B(P + 0xb8);
      m.store<uint8_t>(tb, 0);
      if (st & 8) cpy(ctx, tb, pool(ctx, 492));
      if (st & 0x10) cpy(ctx, tb, pool(ctx, 484));
      if (st & 0x20) cpy(ctx, tb, pool(ctx, 472));
      if (st & 0x40) cpy(ctx, tb, pool(ctx, 464));
    }
    if (hot == 2) d.centred2(tb, 0x9a, 0x45, 0x80007fffu);
    else d.centred(tb, 0x9a, 0x45, 0x80000848u);
    {
      const uint8_t st = B(P + 0xb8);
      m.store<uint8_t>(tb, 0);
      if (st & 1) cpy(ctx, tb, pool(ctx, 456));
      if (st & 2) cpy(ctx, tb, pool(ctx, 444));
      if (st & 4) cpy(ctx, tb, pool(ctx, 436));
    }
    if (hot == 3) d.centred2(tb, 0x9a, 0x5a, 0x80007fffu);
    else d.centred(tb, 0x9a, 0x5a, 0x80000848u);
"""),
    # 6. the personality and preference labels with their selected sprites
    ("""    for (uint32_t k = 0; k < 5; ++k) cpy(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 4 * (3 + k)));
    for (uint32_t k = 0; k < 3; ++k) {
      cpy(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 4 * k));
      SB(tb, B(tb) + 0xe0);
    }
""",
     """    d.font(GS(g_font51b360));
    const int32_t traits = m.load<int8_t>(preview_rec() + 0x20);
    for (uint32_t k = 0; k < 5; ++k) {
      const bool bit = ((1 << k) & traits) != 0;
      if (bit) d.scaled(GS(g_sprites59df20) + 0x134 + 0x58 * k, d.spot(i32(3 + k), 0), d.spot(i32(3 + k), 1));
      cpy(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 4 * (3 + k)));
      d.centred(tb, 0x5d, i32(0x7a + 0x1c * k), bit ? 0x80000848u : 0x80004210u);
    }
    d.font(GS(g_font519928));
    for (uint32_t k = 0; k < 3; ++k) {
      const bool bit = ((1u << k) & B(P + 0x19)) != 0;
      if (bit) d.scaled(GS(g_sprites59df20) + 0x2c + 0x58 * k, d.spot(i32(k), 0), d.spot(i32(k), 1));
      cpy(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 4 * k));
      SB(tb, B(tb) + 0xe0);
      d.centred(tb, 0xd4, i32(0x8e + 0x23 * k), bit ? 0x80000848u : 0x80004210u);
    }
"""),
    # 7. the member's skills
    ("""      // ---- the member's ten skills (the player's skills for character 0)
      for (uint32_t k = 0; k < 10; ++k) {
        const uint32_t skill = character != 0 ? B(Gr + 0xf0 + k) : B(GS(g_playerSkills) + k);
        if (skill != 0) {
          cpy(ctx, tb, GS(f_strpool_4c5340));
          cat(ctx, tb, itoa10(ctx, i32(skill * 10), nb));
          cat(ctx, tb, GS(f_strpool_4c60c4));
        }
      }
""",
     """      // ---- the member's ten skills (the player's skills for character 0)
      d.centred(pool(ctx, 0x1a0), 0x249, 0x1e, 0x80000000u);
      d.font(GS(g_font51b360));
      for (uint32_t k = 0; k < 10; ++k) {
        const uint32_t skill = character != 0 ? B(Gr + 0xf0 + k) : B(GS(g_playerSkills) + k);
        const int32_t yy = i32(0x3d + 0x10 * k);
        d.fill(0x1f2, yy - 3, 0x2a, 0xe, 0x80000000u);
        d.fill(0x1f3, yy - 2, 0x28, 0xc, skill != 0 ? 0x800023e8u : 0x80007d08u);
        if (skill != 0) {
          cpy(ctx, tb, GS(f_strpool_4c5340));
          cat(ctx, tb, itoa10(ctx, i32(skill * 10), nb));
          cat(ctx, tb, GS(f_strpool_4c60c4));
          d.text(tb, 0x1f5, yy, 0x80000000u);
        }
        d.text(m.load<uint32_t>(GS(f_ptrtab_4c2c3c) + 4 * k), 0x222, yy, skill != 0 ? 0x80000000u : 0x80004210u);
      }
"""),
    # 8. the type card and bio
    ("""      const uint32_t T = GS(g_golferTypeState) + u32(mul32(character, 0x2c));
      cpy(ctx, tb, pool(ctx, 340));
      cat(ctx, tb, B(T) != 0 ? itoa10(ctx, B(T), nb) : GS(f_strpool_4c52bc));
      cpy(ctx, tb, pool(ctx, 328));
      {
        const int32_t v = m.load<int8_t>(T + 1);
        cat(ctx, tb, v > 0 ? itoa10(ctx, v, nb) : GS(f_strpool_4c52bc));
      }
      cpy(ctx, tb, pool(ctx, 312));
      {
        const int32_t v = m.load<int16_t>(T + 0x2a);
        cat(ctx, tb, v != 0 ? itoa10(ctx, v, nb) : pool(ctx, 304));
      }
      cpy(ctx, tb, R1 + 0x30);
""",
     """      const uint32_t T = GS(g_golferTypeState) + u32(mul32(character, 0x2c));
      if (d.on) {
        int32_t kind = B(T + 2) & 7;
        if (B(T + 0x29) == 0xff) kind = -1;
        else if (kind > 3) d.fill(0x1f4, 0x1c, 0xa0, 0x11, 0x80000210u);
        switch (kind + 1) {
          case 0: d.centred(pool(ctx, 0x194), 0x244, 0x1e, 0x80006000u); break;
          case 2: d.centred(GS(f_strpool_4c3ac0), 0x244, 0x1e, 0x80006318u); break;
          case 3: d.centred(pool(ctx, 0x18c), 0x244, 0x1e, 0x80004210u); break;
          case 4: d.centred(pool(ctx, 0x17c), 0x244, 0x1e, 0x80002108u); break;
          case 5: d.centred(pool(ctx, 0x170), 0x244, 0x1e, 0x80007ff0u); break;
          case 6: d.centred2(pool(ctx, 0x160), 0x244, 0x1e, 0x80007fffu); break;
          default: break;
        }
      }
      d.font(GS(g_font51b360));
      cpy(ctx, tb, pool(ctx, 340));
      cat(ctx, tb, B(T) != 0 ? itoa10(ctx, B(T), nb) : GS(f_strpool_4c52bc));
      d.centred(tb, 0x244, 0x32, 0x80002108u);
      cpy(ctx, tb, pool(ctx, 328));
      {
        const int32_t v = m.load<int8_t>(T + 1);
        cat(ctx, tb, v > 0 ? itoa10(ctx, v, nb) : GS(f_strpool_4c52bc));
      }
      d.centred(tb, 0x244, 0x40, 0x80002108u);
      cpy(ctx, tb, pool(ctx, 312));
      {
        const int32_t v = m.load<int16_t>(T + 0x2a);
        cat(ctx, tb, v != 0 ? itoa10(ctx, v, nb) : pool(ctx, 304));
      }
      d.centred(tb, 0x244, 0x4e, 0x80002108u);
      d.centred(pool(ctx, 0x12c), 0x244, 0x62, ctl != 0x14 ? 0x8000211fu : 0x80007b20u);
      cpy(ctx, tb, R1 + 0x30);
      d.gx("drawMessageBuffer", {0x1e, 0x1e4, 0x72, 0x80000018u});
"""),
    # 9. the comment lines
    ("""      uint32_t line = GS(g_golferPhrases) + u32(mul32(character, 0x4e2));
      for (;;) {""",
     """      uint32_t line = GS(g_golferPhrases) + u32(mul32(character, 0x4e2));
      d.font(GS(g_font51b360));
      int32_t yy = 0x10f;
      for (;;) {"""),
    ("""        cpy(ctx, tb, m.load<uint32_t>(labels));
        cat(ctx, tb, GS(f_strpool_4c5d20) + 148);
        gs.g_commentTone = 0x80000848u;
        cpy(ctx, tb, line);
        if (B(tb) == 0) {
          kind = m.load<int8_t>(kinds);
          peer(ctx, "buildGolferComment",
               {u32(kind), m.load<uint32_t>(GS(g_thoughtArgByKind) + 4 * u32(kind)) & 0x3fffu, 0, u32(kPreview)});
          gs.g_commentTone = 0x80004210u;
        }
""",
     """        cpy(ctx, tb, m.load<uint32_t>(labels));
        cat(ctx, tb, GS(f_strpool_4c5d20) + 148);
        d.text(tb, 0x34, yy, gs.g_commentTone);
        gs.g_commentTone = 0x80000848u;
        cpy(ctx, tb, line);
        if (B(tb) == 0) {
          kind = m.load<int8_t>(kinds);
          peer(ctx, "buildGolferComment",
               {u32(kind), m.load<uint32_t>(GS(g_thoughtArgByKind) + 4 * u32(kind)) & 0x3fffu, 0, u32(kPreview)});
          gs.g_commentTone = 0x80004210u;
        }
        d.text(tb, 0x19c, yy, gs.g_commentTone);
        yy += 0x10;
"""),
    # 10. the hover tip and the pass's refresh
    ("""          if (t < 12) cpy(ctx, tb, pool(ctx, kTips[t]));
          else if (t == 12 && !member) cpy(ctx, tb, pool(ctx, kTipBio));
""",
     """          bool draw = true;
          if (t < 12) cpy(ctx, tb, pool(ctx, kTips[t]));
          else if (t == 12 && !member) cpy(ctx, tb, pool(ctx, kTipBio));
          else draw = false;
          if (draw && t <= 12 && B(tb) != 0) d.gx("drawPanelBackdrop", {u32(x), u32(y)});
"""),
    ("""    pump_pass(ctx, "showCharacterEditor");
    int32_t btn = gs.g_rootInput[kButton];""",
     """    d.gx("Window::refreshFull", {d.win, 0});
    pump_pass(ctx, "showCharacterEditor");
    int32_t btn = gs.g_rootInput[kButton];"""),
    # 11. the gender's background reload
    ("""        case 17:  // the gender (the background Image reload is drawing)
          SB(R1 + 0x21, B(R1 + 0x21) ^ 0x80);
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          break;""",
     """        case 17:  // the gender (the background Image reload is drawing)
          SB(R1 + 0x21, B(R1 + 0x21) ^ 0x80);
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          d.gx("showCharacterEditor.setup", {pool(ctx, (B(R1 + 0x21) & 0x80) ? 0x218u : 0x1fcu)});
          break;"""),
    # 12. the bio entry's panel
    ("""        case 20:  // the bio (non-members): the global EditBox edits the record's bio until a press or Enter
          if (member) break;
""",
     """        case 20:  // the bio (non-members): the global EditBox edits the record's bio until a press or Enter
          if (member) break;
          d.gx("drawPanelS2", {0x1d8, 0x68, 0x13c, 0xd8, 1});
          d.gx("showCharacterEditor.bio", {});   // the EditBox's own drawing: reported, not modelled
"""),
    # 13. the exit: the frame Image
    ("""  cm::call_optional(ctx, "startSoundSlot", {(B(R1 + 0x21) & 0x80) ? 0x7du : 0x80u});
  cm::call_optional(ctx, "playSound", {0x2d, 0x64, 0, 0, 0});
  return 0;
}""",
     """  cm::call_optional(ctx, "startSoundSlot", {(B(R1 + 0x21) & 0x80) ? 0x7du : 0x80u});
  cm::call_optional(ctx, "playSound", {0x2d, 0x64, 0, 0, 0});
  d.gx("showCharacterEditor.teardown", {});
  return 0;
}"""),
]

def main(root: str) -> int:
    p = pathlib.Path(root) / "port" / "src" / "sim" / "chareditor.cpp"
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
