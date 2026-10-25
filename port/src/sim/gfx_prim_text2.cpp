// Body family `prim_text2` of the graphics backend (roadmap S2.4; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// centred text with a one-pixel shadow. Specification: c3dq.cpp (DrawCenteredText2_re, DrawCenteredText_re), c3et.cpp
// (AllocTextBuffer_re = drawTextRun, the run drawer) and the core's text path (gfx.cpp gfxb::draw_text_string).
//
//   drawCenteredText2(text, x, y, colour)
//       Widget_setQuad(target, colour, 1, 0, 1), then (text not NULL) drawCentered2(target, text, x, y, 0, strlen): the
//       text centred on x as drawCenteredText centres it (the core body), in the main colour `colour`, after a shadow run in
//       colour 1 at (x + 0, y + 1) (drawTextRun draws the shadow first, at the pen plus the state's shadow offsets +0x8c /
//       +0x9c, then the main run). Colour 1 has no bit 31: it is an index into the run palette (drawTextRun's palette arm:
//       the 16-bit target takes the palette of g_engine83ad18 + 4, else the surface's own, else g_engine83ad0c + 4, and
//       sets the colour by that entry's three bytes). The backend does not trace those Palette objects (heap objects of the
//       engine); entry 1 is taken as the measured colour RGB (0x80, 0, 0): the routing map's hole labels of cov_a_keys
//       k_dlg_R hold 6 shadow pixels of RGB555 (16, 0, 0) under the labels (log/roadmap/S2/4_notes.md, U-1230). Any other
//       palette index is reported as unsupported.
//
// Negative controls (g_negctl_gfx_prim_text2, sg_negctl_set "gfx.family.prim_text2"):
//   1 no shadow run                    2 the shadow one pixel right as well (offset (1, 1))
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"

namespace portsim {

int g_negctl_gfx_prim_text2 = 0;

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_prim_text2); }

// the run palette's entry 1 as a direct colour word (bit 31, r << 7, the core's conversion back gives r = 0x80): U-1230
constexpr uint32_t kRunPalette1 = 0x80000000u | (0x80u >> 3) << 10;

void b_draw_centered_text2(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (a[0] == 0) return;
  const std::string text = gfxb::vstring(ctx.mem, a[0]);
  const int32_t x = static_cast<int32_t>(a[1]), y = static_cast<int32_t>(a[2]);
  if (mode() != 1)
    gfxb::draw_text_string(g, ctx, text, a[0], mode() == 2 ? add32(x, 1) : x, add32(y, 1), kRunPalette1, true,
                           "drawCenteredText2 (shadow)");
  gfxb::draw_text_string(g, ctx, text, a[0], x, y, a[3], true, "drawCenteredText2");
}

const GfxBodyDef kBodies[] = {
    {"drawCenteredText2", b_draw_centered_text2},
};

}  // namespace

extern const GfxFamily kGfxFamily_prim_text2 = {"prim_text2", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
