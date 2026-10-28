// Body family `results` of the graphics backend (roadmap S2.11; port/src/sim/gfx_body.h, listed in gfx_families.inc): what the
// TOURNAMENT RESULTS strip of showTournamentResults (c3eo.cpp ShowTournamentResults_re, the port's tick_panels.inc) needs
// beyond the bodies other families own. Its scorecard layout draws through Surface_drawSprite (g_spriteRef_821040, drawAlpha's
// quarter band: the reports family), Surface_stretchBlit (the bands of g_spriteRef_821cb8 over their masks: the core),
// setDrawTarget, drawCenteredText, drawText4049d0, Widget_setQuad (prim_text), Image_draw (the close button
// g_spriteRef_820f40: the core), drawStringRightAt (the text family's body since roadmap S3.13: the original's drawRichText(text,
// x - drawMarkupRun(text, n), y, n), gfx_text.cpp; S2.11's body here measured the run with the provider's plain glyph widths
// and drew through prim_text's drawText path; its control 1 is the text family's 15) and this:
//
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
//   2 tick_panels.inc advances a row by one pixel more (read by tick_panels.inc through results_control); mode 1
//   (drawStringRightAt one pixel right) is the text family's 15 since roadmap S3.13
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
// tick_panels.inc's layout controls (mode 2)
bool results_control(int m) { return SG_NEGCTL(g_negctl_gfx_results) == m; }

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_results); }

void b_begin(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  gfxs::probe_fill(g, ctx, mode(), 0, 0, g.root.w, g.root.h);
}

const GfxBodyDef kBodies[] = {
    {"results.begin", b_begin},
};

}  // namespace

extern const GfxFamily kGfxFamily_results = {"results", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
