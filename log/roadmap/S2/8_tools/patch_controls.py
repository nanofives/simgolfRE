"""S2.8: the screens' negative controls (reports family modes 11..19) and body modes 6..8 in the mirror (run from its root)."""
import pathlib
import re


def patch(path, pairs):
    p = pathlib.Path(path)
    s = p.read_text()
    for old, new in pairs:
        pat = re.escape(old).replace("\\\n", "\n").replace("\n", "\n(?:[ \t]*\n)*")
        n = len(re.findall(pat, s))
        assert n == 1, (path, old[:70], n)
        s = re.sub(pat, lambda m: new, s)
    p.write_text(s)


C = "gfxr::ScreenDraw::ctl"
patch('port/src/sim/commands_screens.inc', [
    # 11 financial: the year labels one row lower
    ('''d.gx("drawStringAt", {root, font_b, s, u32(x), 0x3c, d.len(s)});''',
     '''d.gx("drawStringAt", {root, font_b, s, u32(x), gfxr::ScreenDraw::ctl(11) ? 0x3du : 0x3cu, d.len(s)});'''),
    # 12 comments: the rows start one row lower
    ('''int32_t sprite_y = add32(d.cut_h(art), 0x16);''',
     '''int32_t sprite_y = add32(d.cut_h(art), gfxr::ScreenDraw::ctl(12) ? 0x17 : 0x16);'''),
    # 13 best scores: the foot cell one row lower
    ('''const int32_t end_y = add32(add32(mul32(row_h, rows), head_h), 0x2d);''',
     '''const int32_t end_y = add32(add32(mul32(row_h, rows), head_h), gfxr::ScreenDraw::ctl(13) ? 0x2e : 0x2d);'''),
    # 18 hire: the band heads one row lower
    ('''y = static_cast<int32_t>(kHead[k / 2][1]);''',
     '''y = static_cast<int32_t>(kHead[k / 2][1]) + (gfxr::ScreenDraw::ctl(18) ? 1 : 0);'''),
    # 19 histograph: the cash labels one pixel right
    ('''[&](uint32_t t) { d.gx("drawCenteredText", {t, 0x2d7, u32(y), 0x80000000u}); });''',
     '''[&](uint32_t t) { d.gx("drawCenteredText", {t, gfxr::ScreenDraw::ctl(19) ? 0x2d8u : 0x2d7u, u32(y), 0x80000000u}); });'''),
])
patch('port/src/sim/holestats.cpp', [
    # 14 hole stats: the title one row lower
    ('''s_at(0x181, 0x30, GS(g_fontRef_821020));''', '''s_at(0x181, gfxr::ScreenDraw::ctl(14) ? 0x31 : 0x30, GS(g_fontRef_821020));'''),
])
patch('port/src/sim/routing.cpp', [
    # 15 roster: the type names one pixel right
    ('''[&](uint32_t s) { w_draw(s, 0x1c, row_y); });''', '''[&](uint32_t s) { w_draw(s, gfxr::ScreenDraw::ctl(15) ? 0x1d : 0x1c, row_y); });'''),
    # 16 shortcuts: the middle line in black
    ('''d.gx("Widget_setQuad", {root, 0x80000200u, 0xffffffffu, 2, 2});''',
     '''d.gx("Widget_setQuad", {root, gfxr::ScreenDraw::ctl(16) ? 0x80000000u : 0x80000200u, 0xffffffffu, 2, 2});'''),
])
patch('port/src/sim/reports.cpp', [
    # 17 end of year: the highlight lines one pixel further apart
    ('''const int32_t step = d.cut_h(line);''', '''const int32_t step = add32(d.cut_h(line), gfxr::ScreenDraw::ctl(17) ? 1 : 0);'''),
])
patch('port/src/sim/gfx_reports.cpp', [
    ('''      x = add32(x, sub32(w, run) / 2);''', '''      x = add32(x, (mode() == 6 ? add32(sub32(w, run), 1) : sub32(w, run)) / 2);'''),
    ('''  c.narrowed = true;
}''', '''  c.narrowed = mode() != 7;
}'''),
    ('''void b_call_bounds(Gfx &g, Ctx &, const uint32_t *, int) { st(g).clip.narrowed = false; }''',
     '''void b_call_bounds(Gfx &g, Ctx &, const uint32_t *, int) {
  if (mode() != 8) st(g).clip.narrowed = false;
}'''),
    ('''//   5 Widget_fillRect's line ends one pixel further right''',
     '''//   5 Widget_fillRect's line ends one pixel further right    6 Widget_draw2 centres with (w - run + 1) / 2
//   7 Surface_box leaves the clip as it was                    8 Surface::callBounds leaves the clip narrowed
//   the screens' own drawing controls (gfx_reports.h ScreenDraw::ctl): 11 financial year labels 1 lower, 12 comment rows 1
//   lower, 13 best-scores foot cell 1 lower, 14 hole-stats title 1 lower, 15 roster type names 1 right, 16 shortcuts middle line
//   black, 17 end-of-year lines 1 further apart, 18 hire band heads 1 lower, 19 histograph cash labels 1 right'''),
])
print("patched")
