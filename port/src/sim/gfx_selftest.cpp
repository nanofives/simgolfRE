// TEST-ONLY body family `selftest` of the graphics backend (roadmap S2.1; port/src/sim/gfx_body.h, listed in gfx_families.inc).
// It proves the family table: port/test/test_gfx.py drives these bodies through gfx.call with names no frame code uses (the
// `gfx_selftest.` prefix), so they never draw in a scenario run. A real family looks the same with drop names of the original.
//
//   gfx_selftest.fillRoot(x, y, w, h, word)       fills the root surface's rectangle (clipped) with the RGB555 word
//   gfx_selftest.loadCopy(name, x, y)             loadImage(name, flags 2) through the shared helper, then a 1:1 copy of the
//                                                 whole image to (x, y) on the root surface (copy8): the helpers of gfx_body.h
//   gfx_selftest.quiet                            a not-drawing name: never reported unsupported, never handled
//
// Negative controls (g_negctl_gfx_selftest, sg_negctl_set "gfx.family.selftest"): 1 fillRoot leaves the rectangle's last column
// unfilled; 2 loadCopy copies to (x + 1, y).
#include <algorithm>
#include <cstdint>

#include "gfx_body.h"

namespace portsim {

int g_negctl_gfx_selftest = 0;

namespace {

void b_fill_root(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]);
  int32_t w = static_cast<int32_t>(a[2]);
  const int32_t h = static_cast<int32_t>(a[3]);
  if (SG_NEGCTL(g_negctl_gfx_selftest) == 1) w = sub32(w, 1);
  const uint16_t word = static_cast<uint16_t>(a[4]);
  Surface16 &d = g.root;
  const int32_t l = std::max(x, 0), t = std::max(y, 0), r = std::min(add32(x, w), d.w), b = std::min(add32(y, h), d.h);
  for (int32_t yy = t; yy < b; ++yy)
    for (int32_t xx = l; xx < r; ++xx) d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)] = word;
}

void b_load_copy(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  Image8 img;
  if (gfxb::load_image(g, ctx, a[0], 0, 0, 0x100, 2, img) != 0) {
    g.unsup("gfx_selftest.loadCopy: the image did not load");
    return;
  }
  int32_t x = static_cast<int32_t>(a[1]);
  if (SG_NEGCTL(g_negctl_gfx_selftest) == 2) x = add32(x, 1);
  gfxb::copy8(img, g.root, 0, 0, img.w, img.h, x, static_cast<int32_t>(a[2]));
}

const GfxBodyDef kBodies[] = {
    {"gfx_selftest.fillRoot", b_fill_root},
    {"gfx_selftest.loadCopy", b_load_copy},
};
const char *const kNotDrawing[] = {"gfx_selftest.quiet"};

}  // namespace

extern const GfxFamily kGfxFamily_selftest = {"selftest", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), kNotDrawing,
                                              sizeof(kNotDrawing) / sizeof(kNotDrawing[0])};

}  // namespace portsim
