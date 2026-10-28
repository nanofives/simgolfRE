// Body family `hud` of the software graphics backend (roadmap S3.6; port/src/sim/gfx_hud.cpp, listed in
// port/src/sim/gfx_families.inc): the in-game Surface_blit3 sources and destinations, the interface art the HUD draws from
// (loadInterfaceArt(1)'s whole image half) and the HUD panels' drawing (title bar, meters, toolbar, bottom panel).
//
// The family keeps its own objects in the Gfx's family state (gfx_body.h Gfx::state_of): 16-bit surfaces other than the
// root (a frame Image the frame code copies the root into and back), the palettes loadImage fills into palette objects, the
// SpriteRefs a draw read with no modelled cut, and the HUD panels' dwell counter. Nothing is written into the GameState or
// its address space.
#ifndef PORTSIM_SIM_GFX_HUD_H
#define PORTSIM_SIM_GFX_HUD_H

#include <array>
#include <cstdint>

namespace portsim {

class Ctx;
class Gfx;
struct Surface16;

namespace gfxh {

// Surface_blit3(this, dst, x, y, w, h, x2, y2, w2, h2) for every case the core body (gfx.cpp) does not model: a 16-bit
// source (the root or a family surface), a destination other than the root, a scaled rectangle. Returns false when the
// case is not modelled here either (the core body then reports it).
bool surface_blit3(Gfx &g, Ctx &ctx, const uint32_t *a);
// the family's 16-bit surface held for an object key (a copy target of Surface_blit3), or nullptr
Surface16 *family_surface(Gfx &g, uint32_t key);
// Window::refreshFull, after the root surface reached the back buffer: TEST-ONLY, with SG_GFX_HUD_BGFILL=<word> set the
// root surface is filled with that RGB555 word, so the next frame draws over a known background (the gate's two-fill
// measure, log/roadmap/S3/6_hudgate.py); nothing without the variable
void after_present(Gfx &g, Ctx &ctx);
// the RGB555 words of the palette object at `key` (a PaletteCache: loadImage's palette argument), or nullptr when no load
// filled it; `create` makes an all-zero entry (roadmap S3.5: composeGolferSprite composes g_images[0]'s palette)
std::array<uint16_t, 256> *palette_of(Gfx &g, uint32_t key, bool create);
// gameMain's 0x10000-word half-bright table (the table g_halfBrightLut points at)
const std::array<uint16_t, 0x10000> &half_bright_table();

}  // namespace gfxh
}  // namespace portsim

#endif
