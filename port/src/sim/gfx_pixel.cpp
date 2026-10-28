// Body family `pixel` of the graphics backend (roadmap S3.6 at the S3 lead's request; port/include/portsim/sim/gfx_pixel.h,
// port/src/sim/gfx_body.h, listed in gfx_families.inc). Specification: c3fu.cpp WidgetWritePixel_re (C3) and its note.
//
//   Widget_writePixel(this, x, y, colour)
//       this is a Surface (+4 its handle). A NULL handle returns 7; x < 0, y < 0 (signed), x >= the handle's width (slot +0xd8)
//       or y >= its height (slot +0xdc) return 0 with nothing written; otherwise Surface_pixelPtr(x, y) and, by the handle's
//       format record (slot +0xe4), an 8-bit surface gets the low byte of colour, a 16-bit one the low word (no other test:
//       bit 31 of a direct colour is not looked at, so it never reaches the surface), any other depth returns 0x17.
//       Modelled: the root surface (16 bits, the address f_stateptr_4c1570 holds) and the hud family's 16-bit surfaces; the
//       pixel is the top-down word at (x, y) (allocDrawBuffer's DIB, the one Surface_pixelPtr walks). Any other Surface is
//       reported.
//
// Negative control (g_negctl_gfx_pixel, sg_negctl_set "gfx.family.pixel"):
//   1 the word is written one row lower (y + 1)
//
// Widget_fillRect is S2's body (gfx_reports.cpp, the same jgld line16 with Surface_box's clip; U-1356, U-1278): S3.6's copy
// and its control 2 were removed when S2 wave 5 came into the tree (roadmap S3.6b).
#include "portsim/sim/gfx_pixel.h"

#include <cstdint>
#include <utility>

#include "gfx_body.h"
#include "portsim/sim/gfx_hud.h"

namespace portsim {

int g_negctl_gfx_pixel = 0;

namespace gfxpx {

bool write_pixel(Surface16 &d, int32_t x, int32_t y, uint32_t colour) {
  if (x < 0 || y < 0 || x >= d.w || y >= d.h) return false;
  d.px[static_cast<size_t>(y) * static_cast<size_t>(d.w) + static_cast<size_t>(x)] = static_cast<uint16_t>(colour);
  return true;
}

}  // namespace gfxpx

namespace {

void b_write_pixel(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  Surface16 *d = a[0] == g.root_va(ctx) ? &g.root : gfxh::family_surface(g, a[0]);
  if (!d) {
    g.unsup("Widget_writePixel: a Surface other than the root or a 16-bit family surface");
    return;
  }
  int32_t y = static_cast<int32_t>(a[2]);
  if (SG_NEGCTL(g_negctl_gfx_pixel) == 1 && y >= 0) ++y;
  gfxpx::write_pixel(*d, static_cast<int32_t>(a[1]), y, a[3]);
}

const GfxBodyDef kBodies[] = {
    {"Widget_writePixel", b_write_pixel},
};

}  // namespace

extern const GfxFamily kGfxFamily_pixel = {"pixel", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
