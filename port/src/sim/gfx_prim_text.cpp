// Body family `prim_text` of the graphics backend (roadmap S2.3; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// text state of a drawing target and the plain text drawer the credits use. Specifications: c3c.cpp (Widget_setQuad), c3ek.cpp
// (drawText, drawRichText), c3et.cpp (drawTextRun) and their notes re/analysis/ui/*.md.
//
//   Widget_setQuad(this, colour, shadow, dx, dy)
//       stores the four words at this +0x6c, +0x7c, +0x8c, +0x9c (state 0's main colour, shadow colour and shadow offsets of
//       the target). The backend keeps them per target in this family's state of the Gfx; nothing else reads them here.
//   drawText(this, text, x, y, limit)
//       a NULL text draws nothing; no font at this +0x5c (the backend's font 0, setDrawTarget) or a font whose +4 is 0 draws
//       nothing; n = min(strlen(text), limit) signed, n <= 0 draws nothing; otherwise drawRichText(text, x, y, n) on this, whose
//       run drawer drawTextRun draws the n characters ('_' as a space) at the pen with the baseline y + the tallest of fonts 0..2
//       and the main colour +0x6c (a direct colour with bit 31: r = (c >> 7) & 0xf8, g = (c >> 2) & 0xf8, b = (c << 3) & 0xff)
//       through the shared helper gfxb::draw_text_string. Modelled: the root surface as target, a direct colour, no shadow (the
//       shadow colour +0x7c is -1: drawTextRun skips the shadow run), text without a markup or tab character (drawRichText's
//       markup scanner is not modelled; draw_text_string reports such text), state 0 (drawTextRun picks the colours of the state
//       +0x30 when +0x40 is set; the backend does not track +0x30, U-1222).
//
// Other families draw a std::string through drawText's path with gfxp::draw_text_run (declared there): the credits keep their
// lines in the backend, the original keeps them in malloc'd blocks only drawing reads.
//
// Negative controls (g_negctl_gfx_prim_text, sg_negctl_set "gfx.family.prim_text"):
//   1 Widget_setQuad stores 0x80000000 (black) as the main colour   2 drawText draws one pixel to the right of x
//   3 drawText ignores the limit (the whole string)
#include <cstdint>
#include <map>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"

namespace portsim {

int g_negctl_gfx_prim_text = 0;

namespace {

struct TextState {
  std::map<uint32_t, std::array<uint32_t, 4>> quad;   // target -> {+0x6c, +0x7c, +0x8c, +0x9c}
};
TextState &st(Gfx &g) { return g.state_of<TextState>("prim_text"); }
int mode() { return SG_NEGCTL(g_negctl_gfx_prim_text); }

void b_set_quad(Gfx &g, Ctx &, const uint32_t *a, int) {
  st(g).quad[a[0]] = {mode() == 1 ? 0x80000000u : a[1], a[2], a[3], a[4]};
}

}  // namespace

namespace gfxp {
// drawText's path for a text the caller holds: target this, text (already cut to its length), pen (x, y); `who` names the
// caller in unsupported reasons
void draw_text_run(Gfx &g, Ctx &ctx, uint32_t target, const std::string &text, uint32_t va, int32_t x, int32_t y,
                   const char *who) {
  g.ensure_root(ctx);
  if (target != g.root_va(ctx)) {
    g.unsup(std::string(who) + ": a target other than the root surface");
    return;
  }
  const uint32_t font = g.fonts[0];
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return;   // drawText returns 3, nothing drawn
  if (text.empty()) return;
  auto it = st(g).quad.find(target);
  if (it == st(g).quad.end()) {
    g.unsup(std::string(who) + ": no Widget_setQuad seen for the target (its colour is not known)");
    return;
  }
  if (it->second[1] != 0xffffffffu) {
    g.unsup(std::string(who) + ": a shadow colour (not modelled)");
    return;
  }
  if (mode() == 2) x = add32(x, 1);
  gfxb::draw_text_string(g, ctx, text, va, x, y, it->second[0], false, who);
}
}  // namespace gfxp

namespace {

void b_draw_text(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (a[1] == 0) return;
  std::string s = gfxb::vstring(ctx.mem, a[1]);
  const int32_t limit = static_cast<int32_t>(a[4]);
  int32_t n = static_cast<int32_t>(s.size());
  if (mode() != 3 && limit < n) n = limit;
  if (n <= 0) return;
  s.resize(static_cast<size_t>(n));
  gfxp::draw_text_run(g, ctx, a[0], s, a[1], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), "drawText");
}

const GfxBodyDef kBodies[] = {
    {"Widget_setQuad", b_set_quad},
    {"drawText", b_draw_text},
};

}  // namespace

extern const GfxFamily kGfxFamily_prim_text = {"prim_text", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
