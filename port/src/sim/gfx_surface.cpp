// Body family `surface` of the graphics backend (roadmap S2.3; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// lifetime of the exe's Image and Surface (SpriteRef, vtable 0x2c-byte class) objects as the backend sees them, and the
// blits between them that showCreditsScreen and mainLoop's frame code reach. Specifications: the C3 / C4 hook sources named
// at each body (shim/src/re/*.cpp) and their notes re/analysis/render/*.md; jgld routines from re/match/jgld_*.cpp.
//
// The backend keys an object by its 32-bit address (or a gfx_local_key for a frame-local one): g.images holds what an Image's
// handle (+4) holds after loadImage, g.sprites what a Surface's handle holds after cropSprite. "Releasing the handle" in the
// original (the display's slots +0x8c / +0x94) is erasing both entries of that key here.
//
//   Image_ctor(this)               c3dl.cpp: +4 = 0 (no handle): the key's entries are erased
//   Image_dtor(this)               c3di.cpp: Image_releaseAll, then the HotLists (not drawing): erased
//   Image_releaseAll(this)         c3dd.cpp: the handle at +4 released through the display: erased
//   Surface_ctor(this)             c3cz.cpp: +4 .. +0x24 = 0: erased
//   Surface_free(this)             c3cz.cpp: the handle released (display slot +0x94): erased
//   Surface_createFull(this, w, h, depth, size, a5, a6)
//                                  c3dl.cpp: Image_releaseAll, then a new w x h handle of that depth from the display's slot
//                                  +0x7c. The new surface holds no pixels a body reads: the key's entries are erased and nothing
//                                  is created, so a later read of it reports "no image" (loadImage, the only use the credits and
//                                  the frame code make of it, replaces the handle with its own 8-bit surface: decodePcx8's
//                                  Surface_paint)
//   Surface_createFull_fwd1(this, a1, a2, a3, a4, a5)
//                                  roadmap S3.17: mainLoop's forwarder (its two calls: the golfer-snapshot key 'B' and the
//                                  routing render keys '.' / '/', each on a frame-local Image just constructed, 0x320 x 0x258)
//                                  passes this through ecx and calls Surface_createFull(this, a1, a2, 0, a3, a4, a5): it
//                                  reads its five stack arguments, pushes them with a 0 as the third and pops 0x14 bytes
//                                  (re/analysis/render/00421b30_Surface_createFull_fwd1.md). The body hands that argument
//                                  list to Surface_createFull's body, so the key's entries are erased the same way
//   Surface_blitTakeDirty(this, src, x, y, w, h, kind, trim)
//                                  c3da.cpp: Surface_blit (cropSprite of src's 8-bit image into this), then src's palette cache
//                                  +0xac moves to this +0xc. The crop copies the palette words, so the move changes nothing the
//                                  backend draws (a later load into src makes a new cache there: c3ee.cpp LoadImage_re)
//   Surface_drawRegion(this, dst, x, y, mask)
//                                  c3be.cpp: this +8 != 0 returns 0x18 (nothing drawn); a NULL dst 0x10; then the handle's slot
//                                  +0x5c = jgld Sprite::draw (re/match/jgld_sprite.cpp) with (dst's handle, this +0x20 + x,
//                                  this +0x24 + y): for an 8-bit sprite on a 16-bit target draw8to16 = draw16_1001cde0
//                                  (re/match/jgld_blit16.cpp) at scale 1: bytes 0xff transparent, every other byte the sprite
//                                  palette's RGB555 word (Image_draw's drawB path skips 0xfe too; this one draws it)
//   Surface_blitThunk(this, dst, x1, y1, x2, y2, w, h)
//                                  c3da.cpp: Surface_blit3(this, dst, x1, y1, w, h, x2, y2, w, h) (the core body)
//
// Negative controls (g_negctl_gfx_surface, sg_negctl_set "gfx.family.surface"):
//   1 Surface_free leaves the sprite in place     2 Image_releaseAll leaves the image in place
//   3 Surface_createFull keeps the old image       4 Surface_blitTakeDirty crops with trim off
//   5 Surface_drawRegion draws byte 0xff too (no transparency)   6 Surface_drawRegion ignores the trim offsets +0x20 / +0x24
//   7 Surface_blitThunk passes (x2, y2) as the source corner and (x1, y1) as the destination's
//   8 Image_ctor / Surface_ctor leave the key's entries in place
//   9 Surface_createFull_fwd1 does nothing (the key keeps its image; roadmap S3.17)
#include <algorithm>
#include <cstdint>

#include "gfx_body.h"
#include "portsim/arith.h"

namespace portsim {

int g_negctl_gfx_surface = 0;

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_surface); }

void erase_key(Gfx &g, uint32_t k) {
  g.images.erase(k);
  g.sprites.erase(k);
}

void b_image_ctor(Gfx &g, Ctx &, const uint32_t *a, int) {
  if (mode() == 8) return;
  erase_key(g, a[0]);
}
void b_image_release(Gfx &g, Ctx &, const uint32_t *a, int) {
  if (mode() == 2) return;
  erase_key(g, a[0]);
}
void b_image_dtor(Gfx &g, Ctx &ctx, const uint32_t *a, int n) { b_image_release(g, ctx, a, n); }
void b_surface_ctor(Gfx &g, Ctx &, const uint32_t *a, int) {
  if (mode() == 8) return;
  erase_key(g, a[0]);
}
void b_surface_free(Gfx &g, Ctx &, const uint32_t *a, int) {
  if (mode() == 1) return;
  erase_key(g, a[0]);
}
void b_create_full(Gfx &g, Ctx &, const uint32_t *a, int) {
  if (mode() == 3) return;
  erase_key(g, a[0]);
}

// a core or family body by its drop name (a body of another file the original reaches through a call)
void call_body(Gfx &g, Ctx &ctx, const char *name, const uint32_t *a, int n) {
  if (GfxBody b = gfxb::find_body(name)) {
    b(g, ctx, a, n);
    return;
  }
  g.unsup(std::string("gfx_surface: no body ") + name);
}

// Surface_createFull_fwd1(this, a1 .. a5) = Surface_createFull(this, a1, a2, 0, a3, a4, a5)
void b_create_full_fwd1(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (mode() == 9) return;
  const uint32_t b[7] = {a[0], a[1], a[2], 0, a[3], a[4], a[5]};
  call_body(g, ctx, "Surface_createFull", b, 7);
}

void b_blit_take(Gfx &g, Ctx &ctx, const uint32_t *a, int n) {
  uint32_t b[12] = {};
  for (int i = 0; i < 12; ++i) b[i] = a[i];
  if (mode() == 4) b[7] = 0;
  call_body(g, ctx, "Surface_blit", b, n);
}

void b_draw_region(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  if (!gfx_is_local_key(a[0]) && ctx.mem.load<uint32_t>(a[0] + 8) != 0) return;   // 0x18: not available, nothing drawn
  if (a[1] == 0) return;                                                         // 0x10
  if (a[4] != 0 || a[1] != g.root_va(ctx)) {
    g.unsup("Surface_drawRegion: a mask or a destination other than the root surface");
    return;
  }
  auto it = g.sprites.find(a[0]);
  if (it == g.sprites.end()) {
    g.unsup("Surface_drawRegion: a Surface the backend has no sprite for");
    return;
  }
  const Sprite8 &s = it->second;
  const bool trims = mode() != 6;
  const int32_t x = add32(static_cast<int32_t>(a[2]), trims ? s.left : 0);
  const int32_t y = add32(static_cast<int32_t>(a[3]), trims ? s.top : 0);
  const uint32_t key = mode() == 5 ? 0x100u : 0xffu;
  Surface16 &d = g.root;
  const int32_t l = std::max(x, 0), t = std::max(y, 0), r = std::min(x + s.w, d.w), b = std::min(y + s.h, d.h);
  for (int32_t yy = t; yy < b; ++yy) {
    const uint8_t *sp = s.px.data() + static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(l - x);
    uint16_t *dp = d.px.data() + static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(l);
    for (int32_t xx = l; xx < r; ++xx, ++sp, ++dp)
      if (*sp < key) *dp = s.pal[*sp];
  }
}

void b_blit_thunk(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const bool swap = mode() == 7;
  const uint32_t x1 = swap ? a[4] : a[2], y1 = swap ? a[5] : a[3], x2 = swap ? a[2] : a[4], y2 = swap ? a[3] : a[5];
  const uint32_t b[10] = {a[0], a[1], x1, y1, a[6], a[7], x2, y2, a[6], a[7]};
  call_body(g, ctx, "Surface_blit3", b, 10);
}

const GfxBodyDef kBodies[] = {
    {"Image_ctor", b_image_ctor},
    {"Image_dtor", b_image_dtor},
    {"Image_releaseAll", b_image_release},
    {"Surface_ctor", b_surface_ctor},
    {"Surface_free", b_surface_free},
    {"Surface_createFull", b_create_full},
    {"Surface_createFull_fwd1", b_create_full_fwd1},
    {"Surface_blitTakeDirty", b_blit_take},
    {"Surface_drawRegion", b_draw_region},
    {"Surface_blitThunk", b_blit_thunk},
};

}  // namespace

extern const GfxFamily kGfxFamily_surface = {"surface", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
