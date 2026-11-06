// Body family `chareditor` of the graphics backend (roadmap S2.10; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// character editor's own objects (showCharacterEditor, port/src/sim/chareditor.cpp, which makes the original's drawing calls in
// c3fx.cpp ShowCharacterEditor_re's order) and the text-buffer drawer it reaches that no other family registers.
//
//   showCharacterEditor.setup(path)        the editor's frame Image (Image_ctor at entry) loaded with loadImage(path, 0, 0, 0x100,
//                                          2): the gender's background (f_strpool_4c84e8 + 0x218 female, + 0x1fc male); the
//                                          gender control reloads it into the same Image. Backend key gfx_local_key(0x7a000).
//   showCharacterEditor.background(root)   every pass: Surface_blitThunk(image, root, 0, 0, 0, 0, 0x320, 0x258) (the core
//                                          Surface_blit3 through gfx_surface.cpp's thunk body)
//   showCharacterEditor.teardown           the Image destructor: the backend's image dropped
//   drawMessageBuffer(cols, x, y, colour)  the text buffer wrapped at cols * 8 pixels (gfxb::draw_message_buffer, gfx.cpp)
//
// Negative controls (g_negctl_gfx_chareditor, sg_negctl_set "gfx.family.chareditor"):
//   1 the background is the other gender's   2 the background is blitted one row lower   3 drawMessageBuffer's x one pixel right
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/gfx.h"

namespace portsim {

int g_negctl_gfx_chareditor = 0;

namespace {

constexpr uint32_t kImage = gfx_local_key(0x7a000);

int mode() { return SG_NEGCTL(g_negctl_gfx_chareditor); }

void b_setup(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  uint32_t path = a[0];
  if (mode() == 1) {   // the other gender's file: the pool's two names are 0x1c apart
    const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c84e8);
    path = path == pool + 0x218 ? pool + 0x1fc : pool + 0x218;
  }
  Image8 img;
  if (gfxb::load_image(g, ctx, path, 0, 0, 0x100, 2, img) != 0) {
    g.unsup("showCharacterEditor.setup: the background did not load");
    return;
  }
  g.images[kImage] = std::move(img);
}

void b_background(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (g.images.find(kImage) == g.images.end()) {
    g.unsup("showCharacterEditor.background: no background image");
    return;
  }
  const uint32_t y = mode() == 2 ? 1u : 0u;
  const uint32_t b[8] = {kImage, a[0], 0, 0, 0, y, 0x320, 0x258};
  const GfxBody thunk = gfxb::find_body("Surface_blitThunk");
  if (!thunk) {
    g.unsup("showCharacterEditor.background: no Surface_blitThunk body");
    return;
  }
  thunk(g, ctx, b, 8);
}

void b_teardown(Gfx &g, Ctx &, const uint32_t *, int) { g.images.erase(kImage); }

void b_message_buffer(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t x = add32(static_cast<int32_t>(a[1]), mode() == 3 ? 1 : 0);
  gfxb::draw_message_buffer(g, ctx, static_cast<int32_t>(a[0]), x, static_cast<int32_t>(a[2]), a[3]);
}

const GfxBodyDef kBodies[] = {
    {"showCharacterEditor.setup", b_setup},
    {"showCharacterEditor.background", b_background},
    {"showCharacterEditor.teardown", b_teardown},
    {"drawMessageBuffer", b_message_buffer},
};

}  // namespace

namespace gfxb {
// P13B item 13: the character editor is open (its background Image is held from showCharacterEditor.setup to .teardown)
bool chareditor_open(const Gfx &g) { return g.images.find(kImage) != g.images.end(); }
}  // namespace gfxb

extern const GfxFamily kGfxFamily_chareditor = {"chareditor", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
