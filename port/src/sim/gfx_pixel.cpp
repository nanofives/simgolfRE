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
//   Widget_readPixel(this, x, y) (roadmap S3.26; c3db.cpp GetPixel_re, C4): 0 for a NULL handle, x < 0, y < 0 (signed), x >= the
//       handle's width or y >= its height; otherwise the pixel Surface_pixelPtr(x, y) addresses, the word on a 16-bit surface.
//       It is a drop of the frame code (tick.cpp kDrops) whose only use gates a dropped sprite draw: mainLoop P5's zoom box draws
//       g_sprite567b08 over each ball whose magnified pixel is 0x7fff (ML+0x95d3; log/roadmap/S3/25_notes.md "Group 7"). With a
//       backend the read is handed back to the frame code through gfx_take_value (Text_put's way, S3.7); without one the drop
//       returns 0 as before, so no GameState byte depends on it. Modelled on the root and the hud family's 16-bit surfaces;
//       any other Surface is reported and reads 0.
//
//   gfx_sprite_ref_read(ctx, va, size, v) (roadmap S3.26; not a body: the frame code calls it): a read the frame code makes into
//       a SpriteRef object (0x2c bytes) the backend holds a sprite for answers what cropSprite stored there (c3ee.cpp
//       CropSprite_re): +0x10 the kept width, +0x14 the kept height, +0x18 / +0x1c the requested width / height, +0x20 left
//       trim, +0x24 top trim (dwords at those offsets); a byte of the handle +4..+7 reads 1 (the original holds a non-NULL heap
//       pointer there, whose bytes are not reproducible: U-1950). Anything else is not modelled (false: the caller reads the
//       GameState). Used by tick_frame.inc P4_selectedSprite.
//
// Negative control (g_negctl_gfx_pixel, sg_negctl_set "gfx.family.pixel"):
//   1 the word is written one row lower (y + 1)
//   2 Widget_readPixel hands back the pixel with bit 0 flipped
//   3 gfx_sprite_ref_read answers +0x1c (the requested height) one larger
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

void b_read_pixel(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  g.has_value = true;
  g.value = 0;
  Surface16 *d = a[0] == g.root_va(ctx) ? &g.root : gfxh::family_surface(g, a[0]);
  if (!d) {
    g.unsup("Widget_readPixel: a Surface other than the root or a 16-bit family surface");
    return;
  }
  const int32_t x = static_cast<int32_t>(a[1]), y = static_cast<int32_t>(a[2]);
  if (x < 0 || y < 0 || x >= d->w || y >= d->h) return;
  g.value = d->px[static_cast<size_t>(y) * static_cast<size_t>(d->w) + static_cast<size_t>(x)];
  if (SG_NEGCTL(g_negctl_gfx_pixel) == 2) g.value ^= 1;
}

const GfxBodyDef kBodies[] = {
    {"Widget_writePixel", b_write_pixel},
    {"Widget_readPixel", b_read_pixel},
};

}  // namespace

extern const GfxFamily kGfxFamily_pixel = {"pixel", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

bool gfx_sprite_ref_read(Ctx &ctx, uint32_t va, uint32_t size, uint32_t &v) {
  Gfx *g = gfx(ctx);
  if (!g) return false;
  auto it = g->sprites.upper_bound(va);
  if (it == g->sprites.begin()) return false;
  --it;
  const uint32_t o = va - it->first;
  if (o >= 0x2c) return false;
  const Sprite8 &s = it->second;
  if (size == 1 && o >= 4 && o < 8) {
    v = 1;
    return true;
  }
  if (size != 4) return false;
  switch (o) {
    case 0x10: v = static_cast<uint32_t>(s.w); return true;
    case 0x14: v = static_cast<uint32_t>(s.h); return true;
    case 0x18: v = static_cast<uint32_t>(s.cut_w); return true;
    case 0x1c: v = static_cast<uint32_t>(s.cut_h + (SG_NEGCTL(g_negctl_gfx_pixel) == 3 ? 1 : 0)); return true;
    case 0x20: v = static_cast<uint32_t>(s.left); return true;
    case 0x24: v = static_cast<uint32_t>(s.top); return true;
    default: return false;
  }
}

}  // namespace portsim
