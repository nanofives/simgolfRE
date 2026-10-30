"""S3.16: add drawGolferInfoPanel's drawing calls (c3fr.cpp GolferPanel_re, in its order and with its arguments) to the port's
draw_golfer_info_panel (dialogs.cpp), made only with a graphics backend attached (SkDraw.gx -> gfx_drop). Applied to a tree
given as the first argument (default port/build/S3_16); each old text must occur exactly once."""
import pathlib
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "port/build/S3_16")
p = root / "port/src/sim/dialogs.cpp"
t = p.read_text(encoding="utf-8")

R = [
# negctl doc
("""//                        slot 8 for mode != 0
""", """//                        slot 8 for mode != 0, 6 = (roadmap S3.16, a backend only) the name line drawn one pixel lower,
//                        7 = the meters drawn one pixel lower
"""),
# head: draw helper + header images + bubble
("""  const bool story = !(m.load<int16_t>(R + 0xb0) == -1 || gs.g_infoPanelToggle == 0);
  thought_bubble(ctx, g, -1);
""", """  const bool story = !(m.load<int16_t>(R + 0xb0) == -1 || gs.g_infoPanelToggle == 0);
  // roadmap S3.16: the panel's drawing (c3fr.cpp GolferPanel_re), only with a graphics backend attached (nothing written
  // into the GameState): the background (not story: Surface_blit2 of the panel image with the half-bright table, then its
  // frame; story: Image_draw of the frame and the story band), the portrait frame and the golfer's bubble, then each text
  // line, meter, scorecard cell, button and tooltip where the text is built
  const SkDraw d(ctx);
  const uint32_t surf = d.root, win = GS(g_rootWindow), widget = win + 0x274;
  auto target = [&](uint32_t self, uint32_t font) { d.gx("setDrawTarget", {self, font, 0, 0, 0}); };
  auto centred = [&](uint32_t text, int32_t x, int32_t y, uint32_t c) { d.gx("drawCenteredText", {text, u32(x), u32(y), c}); };
  auto clampv = [](int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); };
  if (!story) {
    d.gx("Surface_blit2", {GS(g_sprites541a18) + 0x1e4, surf, 0xec, 0x1a, ctx.mem.load<uint32_t>(GS(g_halfBrightLut)), 0});
    d.stretch(GS(g_sprites541a18) + 0x18c, GS(g_sprites58b390) + 0x18c, 0xec, 0x1a);
  } else {
    d.gx("Image_draw", {GS(g_sprites541a18) + 0x18c, surf, 0xec, 0x1a, 0});
    d.stretch(GS(g_sprites541a18) + 0x1b8, GS(g_sprites58b390) + 0x1b8, 0xec, 0xa5);
  }
  d.stretch(GS(g_sprites5a53c8) + 0x4fc, GS(g_sprites58b680) + 0x4fc, 0xac, -2);
  thought_bubble(ctx, g, -1, 0xac, -2, true);
  target(surf, GS(g_font519928));
"""),
("""  cat(ctx, tb, GS(f_strpool_4c59e0));
  // status line (the last of four bits wins) and age
""", """  cat(ctx, tb, GS(f_strpool_4c59e0));
  d.centred2(tb, 0x1a4, neg == 6 ? 0x29 : 0x28, 0x80000210u);
  target(surf, GS(g_font51b360));
  // status line (the last of four bits wins) and age
"""),
("""  cat_itoa(ctx, i32(call_peer(ctx, "golferAge", {u32(g)}).eax));
  // traits: bits 0..4 of the type's signed byte +0x20, joined by a comma, the last by "and"
  clear();
""", """  cat_itoa(ctx, i32(call_peer(ctx, "golferAge", {u32(g)}).eax));
  centred(tb, 0x1a4, 0x3e, 0x80000210u);
  // traits: bits 0..4 of the type's signed byte +0x20, joined by a comma, the last by "and"
  clear();
"""),
("""  // opinion
  cpy(ctx, tb, GS(f_strpool_4c4e54));
  call_peer(ctx, "appendOpinion", {u32(m.load<int16_t>(R + 0xa4)), 0});
  cat(ctx, tb, GS(f_strpool_4c4e54));
""", """  centred(tb, 0x1a4, 0x4b, 0x80000210u);
  // opinion
  cpy(ctx, tb, GS(f_strpool_4c4e54));
  call_peer(ctx, "appendOpinion", {u32(m.load<int16_t>(R + 0xa4)), 0});
  cat(ctx, tb, GS(f_strpool_4c4e54));
  d.centred2(tb, 0x1a4, 0x5b, 0x80004210u);
"""),
("""  // the meters: only thoughtBalance is a call (the rest is drawing)
  call_peer(ctx, "thoughtBalance", {u32(g)});
""", """  centred(tb, 0x1a4, 0x69, 0x80000210u);
  target(widget, GS(g_fontRef_519fd8));
  // the meters: only thoughtBalance is a call (the rest is drawing): a label and two fillWindowRect bands each, the
  // second v wide from the right, red over green when v > 0x28
  auto meter = [&](int32_t y, int32_t v) {
    if (neg == 7) ++y;
    const bool hi = v > 0x28;
    d.gx("fillWindowRect", {win, 0x222, u32(y), 0x50, 4, hi ? 0x80007d08u : 0x800023e8u});
    d.gx("fillWindowRect", {win, u32(sub32(0x272, v)), u32(y), u32(v), 4, hi ? 0x80006000u : 0x80001284u});
  };
  centred(GS(f_strpool_4d2120), 0x24a, 0x2c, 0x80000000u);
  meter(0x36, clampv(mul32(sub32(8, m.load<int16_t>(R + 0xa4)), 10), 0, 0x50));
  {
    const int32_t v = clampv(mul32(sub32(4, i32(call_peer(ctx, "thoughtBalance", {u32(g)}).eax)), 10), 0, 0x50);
    centred(GS(f_strpool_4d3968) + 0x94, 0x24a, 0x3c, 0x80000000u);
    meter(0x46, v);
  }
  centred(GS(f_strpool_4d3968) + 0x8c, 0x24a, 0x4c, 0x80000000u);
  meter(0x56, clampv(m.load<int16_t>(R + 0xaa) / 4, 0, 0x50));
  centred(GS(f_strpool_4d3968) + 0x84, 0x24a, 0x5c, 0x80000000u);
  meter(0x66, clampv(mul32(m.load<int16_t>(R + 0xa6), 5) / 2, 0, 0x50));
  centred(GS(f_strpool_4d3968) + 0x7c, 0x24a, 0x6c, 0x80000000u);
  meter(0x76, clampv(mul32(m.load<int16_t>(R + 0xa8), 5) / 2, 0, 0x50));
"""),
# scorecard
("""  for (int32_t h = 1, x = 0x10a; x < 0x24e; ++h, x += 0x12) {
    clear();
    cat_itoa(ctx, h);
    const int32_t sc = m.load<int8_t>(R + 0x23 + u32(h));
    if (sc != 0) {
      clear();
      cat_itoa(ctx, sc);
      total = add32(total, m.load<int8_t>(R + 0x23 + u32(h)));
    }
    if (hole == h) {
      clear();
      cat_itoa(ctx, m.load<int8_t>(R + 0x22));
      total = add32(total, m.load<int8_t>(R + 0x22));
    }
  }
  clear();
  cat_itoa(ctx, total);
  const int32_t prev = gs.g_infoPanelTab;
""", """  for (int32_t h = 1, x = 0x10a; x < 0x24e; ++h, x += 0x12) {
    target(surf, GS(g_font51b360));
    clear();
    cat_itoa(ctx, h);
    centred(tb, x, 0x84, 0x80000000u);
    const int32_t sc = m.load<int8_t>(R + 0x23 + u32(h));
    if (sc != 0) {
      // the strokes' colour against the hole's par (the signed byte at the hole record's start)
      const int32_t par = m.load<int8_t>(GS(g_holes) + u32(mul32(0x208, h)));
      uint32_t c = 0x80000000u;
      if (sc < par) c = 0x80007d08u;
      if (sc < par - 1) c = 0x80007ff0u;
      if (sc > par) c = 0x8000211fu;
      if (sc > par + 1) c = 0x80004010u;
      clear();
      cat_itoa(ctx, sc);
      target(surf, GS(g_font519928));
      centred(tb, x, 0x94, c);
      total = add32(total, m.load<int8_t>(R + 0x23 + u32(h)));
    }
    if (hole == h) {
      clear();
      cat_itoa(ctx, m.load<int8_t>(R + 0x22));
      target(surf, GS(g_font519928));
      centred(tb, x, 0x94, 0x80004210u);
      total = add32(total, m.load<int8_t>(R + 0x22));
    }
  }
  target(surf, GS(g_font519928));
  clear();
  cat_itoa(ctx, total);
  centred(tb, 0x256, 0x94, 0x80000000u);
  target(surf, GS(g_font51b360));
  // a button: Surface_stretchBlit(image + 0x2c s, mask + 0x2c s), s = 1 when it is the hovered one
  auto button = [&](uint32_t off, bool on_, int32_t x, int32_t y) {
    const uint32_t s = on_ ? 0x2cu : 0u;
    d.stretch(GS(g_surfaces56a948) + off + s, GS(g_surfaces567390) + off + s, x, y);
  };
  const int32_t prev = gs.g_infoPanelTab;
"""),
# story loop drawing
("""    for (;;) {
      clear();
      peer(ctx, "loadStoryStrings", {u32(m.load<int16_t>(R + 0xb0)), u32(-1), 0, 0});
      m.store<uint8_t>(tb, 0x27);
      cat(ctx, tb, GS(f_strpool_4d3968) + 0x6c);
      call_peer(ctx, "buildGolferName", {u32(g), 0});
      cat(ctx, tb, GS(f_strpool_4c6980) + 0x174);
      call_peer(ctx, "buildGolferName", {u32(partner), 0});
      int32_t y = 0xb3;
      if (m.load<int8_t>(R + 0x21) >= 1) {
        int32_t k = 1;
        do {""", """    for (;;) {
      target(surf, GS(g_fontRef_519fd8));
      clear();
      peer(ctx, "loadStoryStrings", {u32(m.load<int16_t>(R + 0xb0)), u32(-1), 0, 0});
      m.store<uint8_t>(tb, 0x27);
      cat(ctx, tb, GS(f_strpool_4d3968) + 0x6c);
      call_peer(ctx, "buildGolferName", {u32(g), 0});
      cat(ctx, tb, GS(f_strpool_4c6980) + 0x174);
      call_peer(ctx, "buildGolferName", {u32(partner), 0});
      d.gx("Widget_fillRect", {widget, 0x148, 0xaf, 0x210, 0xaf, 0x80000210u});
      centred(tb, 0x1ac, 0xa7, 0x800003ffu);
      int32_t y = 0xb3;
      if (m.load<int8_t>(R + 0x21) >= 1) {
        int32_t k = 1, x = 0xf8;
        do {"""),
("""            if (peer(ctx, "storyText", {u32(m.load<int16_t>(R + 0xb0)), u32(k), u32(other)}).eax != 0) {
              peer(ctx, "substituteStoryNames", {u32(other)});
              y += 9;
            }
            clear();
            if (peer(ctx, "storyText", {u32(m.load<int16_t>(R + 0xb0)), u32(k), u32(who)}).eax != 0) {
              peer(ctx, "substituteStoryNames", {u32(who)});
              y += 10;
            }
          }
          ++k;
        } while""", """            if (peer(ctx, "storyText", {u32(m.load<int16_t>(R + 0xb0)), u32(k), u32(other)}).eax != 0) {
              peer(ctx, "substituteStoryNames", {u32(other)});
              d.text(tb, x, y, g != who ? 0x80007c1fu : 0x80004210u);
              y += 9;
            }
            clear();
            if (peer(ctx, "storyText", {u32(m.load<int16_t>(R + 0xb0)), u32(k), u32(who)}).eax != 0) {
              peer(ctx, "substituteStoryNames", {u32(who)});
              d.text(tb, x, y, g != who ? 0x80004210u : 0x80007c1fu);
              y += 10;
            }
          }
          ++k;
          x += 10;
        } while"""),
("""        gs.g_infoPanelTab == 5)
      gs.g_infoPanelTab = -1;
    if (neg != 3) thought_bubble(ctx, partner, -1);
""", """        gs.g_infoPanelTab == 5)
      gs.g_infoPanelTab = -1;
    {
      const int32_t hv = gs.g_infoPanelTab;
      const bool pair4 = m.load<int16_t>(R + 0xb2) == 4 && m.load<int16_t>(rec(ctx, m.load<int16_t>(R + 0xa2)) + 0xb2) == 4;
      button(0, hv == 0, 0x10d, 0x10e);
      if (!pair4) button(0x318, hv == 5, 0x14e, 0x110);
      button(0x420, hv == 2, 0x18f, 0x110);
      button(0x39c, hv == 3, 0x1d0, 0x110);
      button(0x4a4, hv == 4, 0x20e, 0x110);
      d.stretch(GS(g_sprites5a53c8) + 0x4fc, GS(g_sprites58b680) + 0x4fc, 0x228, 0xb0);
    }
    if (neg != 3) thought_bubble(ctx, partner, -1, 0x228, 0xb0, true);
"""),
("""    static const uint32_t kStoryTips[6] = {0x60, 0x4c, 0x3c, 0x2c, 0, 0x1c};
    tooltip(ctx, gs.g_infoPanelTab, kStoryTips);
    return;
""", """    static const uint32_t kStoryTips[6] = {0x60, 0x4c, 0x3c, 0x2c, 0, 0x1c};
    tooltip(ctx, gs.g_infoPanelTab, kStoryTips);
    d.gx("drawPanelBackdrop", {u32(cx), u32(cy)});
    return;
"""),
("""  if (near(0x253, 0x106, 0x19)) gs.g_infoPanelTab = 4;
  if (gs.g_infoPanelTab != prev) {""", """  if (near(0x253, 0x106, 0x19)) gs.g_infoPanelTab = 4;
  button(0, gs.g_infoPanelTab == 0, 0x125, 0xed);
  button(0x108, gs.g_infoPanelTab == 1, 0x166, 0xed);
  button(0x18c, gs.g_infoPanelTab == 2, 0x1a7, 0xed);
  if (m.load<int16_t>(R + 0xb0) != -1) button(0x210, gs.g_infoPanelTab == 3, 0x1e8, 0xed);
  button(0x294, gs.g_infoPanelTab == 4, 0x23a, 0xed);
  if (gs.g_infoPanelTab != prev) {"""),
("""      static const uint32_t kTips[6] = {0x60, 0x4c, 0x3c, 0x10, 0, 0x1c};
      tooltip(ctx, gs.g_infoPanelTab, kTips);
    }
  }
""", """      static const uint32_t kTips[6] = {0x60, 0x4c, 0x3c, 0x10, 0, 0x1c};
      tooltip(ctx, gs.g_infoPanelTab, kTips);
      d.gx("drawPanelBackdrop", {u32(cx), u32(cy)});
    }
  }
  target(surf, GS(g_font51b360));
  int32_t ty = 0xaa;
"""),
("""    if (mode != 0 && !red) continue;
    if (++drawn >= 5) return;
""", """    if (mode != 0 && !red) continue;
    if (d.on) {
      // the line: Text_put's width (font g_font51b360) above 0x180 draws a purple line in g_fontRef_519fd8 and any other from
      // its 0x12th byte; the colour grey, green (top bits 0x4000), red (0xc000) or purple (kind 0x32)
      uint32_t c = 0x80006318u;
      if (top == 0x4000u) c = 0x800023e8u;
      if (top == 0xc000u) c = 0x80007d08u;
      if (m.load<uint8_t>(R + 0x70 + u32(k)) == 0x32) c = 0x80007c1fu;
      TextMetrics *tm = text_metrics(ctx);
      const int32_t w = tm ? tm->text_put(ctx, surf, GS(g_font51b360), tb) : 0;
      if (w > 0x180) {
        if (m.load<uint8_t>(R + 0x70 + u32(k)) == 0x32) {
          target(surf, GS(g_fontRef_519fd8));
          d.gx("drawTextSurface", {tb, 0xf8, u32(ty), c});
        } else {
          d.gx("drawTextSurface", {tb + 0x12, 0xf8, u32(ty), c});
        }
      } else {
        d.gx("drawTextSurface", {tb, 0xf8, u32(ty), c});
      }
    }
    ty += 0xd;
    target(surf, GS(g_font51b360));
    if (++drawn >= 5) return;
"""),
]
for o, n in R:
    assert t.count(o) == 1, o[:100]
    t = t.replace(o, n)
p.write_text(t, encoding="utf-8")
print("patched", p)
