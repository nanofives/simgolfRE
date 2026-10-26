// Body family `results` of the graphics backend (roadmap S2.11; port/src/sim/gfx_body.h, listed in gfx_families.inc): what the
// TOURNAMENT RESULTS strip of showTournamentResults (c3eo.cpp ShowTournamentResults_re, the port's tick_panels.inc) needs
// beyond the bodies other families own. Its scorecard layout draws through Surface_drawSprite (g_spriteRef_821040, drawAlpha's
// quarter band: the reports family), Surface_stretchBlit (the bands of g_spriteRef_821cb8 over their masks: the core),
// setDrawTarget, drawCenteredText, drawText4049d0, Widget_setQuad (prim_text), Image_draw (the close button
// g_spriteRef_820f40: the core) and these:
//
//   drawStringRightAt(this, text, x, y, len)
//       c3df.cpp DrawString_re (the hooks.csv name drawStringRightAt): NULL text draws nothing; no font (the target's +0x5c,
//       the backend's font 0, or one whose +4 is 0) draws nothing; n = min(strlen(text), len) signed, n <= 0 draws nothing;
//       otherwise drawRichText(text, x - run, y, n) with run the provider's width of the n characters, through prim_text's
//       drawText path (gfxp::draw_text_run: the target's Widget_setQuad colour, the baseline at y + the font's ascent).
//   results.begin
//       a marker tick's showTournamentResults puts before the scorecard's first drawing call; it draws nothing. With this
//       family's probe on (negctl modes >= 0x10000, a measurement, not a control) the whole root surface is filled with the
//       probe pattern (gfxs::probe_fill, gfx_simfoto.cpp): the strip is modal, nothing under it is redrawn while it waits.
//
// The layout positions the original takes from the bands' SpriteRef fields +0x18 / +0x1c (the cut size cropSprite stores)
// come from the backend's cuts (gfx_sprite_cut, gfx_reports.cpp) in tick_panels.inc; the cuts are loadInfoScreenArt's
// tournament sets (gfx_routing.cpp).
//
// Negative controls (g_negctl_gfx_results, sg_negctl_set "gfx.family.results"):
//   1 drawStringRightAt ends one pixel right of x - run      2 tick_panels.inc advances a row by one pixel more
//   (mode 2 is read by tick_panels.inc through results_control)
#include <algorithm>
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"

namespace portsim {

int g_negctl_gfx_results = 0;

namespace gfxs {
void probe_fill(Gfx &g, Ctx &ctx, int mode, int32_t l, int32_t t, int32_t r, int32_t b);   // gfx_simfoto.cpp
}
namespace gfxp {   // gfx_prim_text.cpp: drawText's path in the target's quad colour
void draw_text_run(Gfx &g, Ctx &ctx, uint32_t target, const std::string &text, uint32_t va, int32_t x, int32_t y,
                   const char *who);
}

// tick_panels.inc's layout controls (mode 2)
bool results_control(int m) { return SG_NEGCTL(g_negctl_gfx_results) == m; }

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_results); }

void b_draw_string_right_at(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const uint32_t self = a[0], text = a[1];
  int32_t x = static_cast<int32_t>(a[2]);
  const int32_t y = static_cast<int32_t>(a[3]), len = static_cast<int32_t>(a[4]);
  if (text == 0) return;
  const uint32_t f = g.fonts[0];
  if (f == 0 || ctx.mem.load<uint32_t>(f + 4) == 0) return;
  std::string s = gfxb::vstring(ctx.mem, text);
  const int32_t n = std::min(static_cast<int32_t>(s.size()), len);
  if (n <= 0) return;
  s.resize(static_cast<size_t>(n));
  int32_t run = 0;
  TextMetrics *tm = text_metrics(ctx);
  if (!tm || !tm->glyph_width(ctx, f, text, n, run)) {
    g.unsup("drawStringRightAt: the provider cannot measure");
    return;
  }
  x = sub32(x, run);
  if (mode() == 1) x = add32(x, 1);
  gfxp::draw_text_run(g, ctx, self, s, text, x, y, "drawStringRightAt");
}

void b_begin(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  gfxs::probe_fill(g, ctx, mode(), 0, 0, g.root.w, g.root.h);
}

const GfxBodyDef kBodies[] = {
    {"drawStringRightAt", b_draw_string_right_at},
    {"results.begin", b_begin},
};

}  // namespace

extern const GfxFamily kGfxFamily_results = {"results", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
