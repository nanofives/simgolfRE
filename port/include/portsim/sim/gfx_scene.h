// Body family `scene` of the software graphics backend (roadmap S3.5; port/src/sim/gfx_scene.cpp, listed in
// port/src/sim/gfx_families.inc): the course view's sprites. The sprite queue's draws (drawQueuedSprite: buildings, scenery,
// trees, animals, flags, walkers, golfers through composeGolferSprite's palette layers and their name labels), the frame's
// direct Sprite_draw / Surface_drawSprite calls, and the jgld blitters they reach (Sprite::blt16a = the plane blit
// draw16_100298b0, Sprite::drawG = draw16_10028630, Sprite::drawAlpha's 16-bit blend bodies), at the display's blit scale
// (Display::setScale, gfx_prim_fill.cpp) unscaled or 16.16-stepped.
//
// The family keeps, per sprite key, the palette the jgld Sprite holds in its +0x10 word (Sprite::blt16a sets it from its key
// argument and never restores it, so a cell keeps the palette of its last queue draw), and the compositor's work buffer of
// composeGolferSprite (the original's stack buffer, whose entries a layer does not fill keep the previous call's values).
// Nothing is written into the GameState or its address space.
#ifndef PORTSIM_SIM_GFX_SCENE_H
#define PORTSIM_SIM_GFX_SCENE_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace portsim {

class Ctx;
class Gfx;
struct Sprite8;   // P23D fix 3 (gfx_body.h)

namespace gfxs {

// build16BitColorTable's five 0x8000-word blocks in RGB555 (the table g_color16Lut points at): block k < 4 holds, at
// pack(r, g, b) for coarse r, g, b in steps of 8, pack of ((c / 8) * (0x400 + 0x100 k)) >> 8 per channel; block 4 holds pack of
// brightenComponent(c) = min(c + 0x20, 0xff). Plane k of the plane blit (payload 0xf8 + k) reads word (k << 15) | dst.
const std::array<uint16_t, 5 * 0x8000> &color16_planes();

// addSprite(x, y, key, w, h, obj, shade, flags) drawn without its queue entry (for a body that replicates a panel which
// calls addSprite, e.g. S3.6's showInfoCursorPanel / drawAmenityRow): the position enqueueSprite / addSprite compute, then
// drawQueuedSprite's drawing at that position (a golfer entry's name label is not drawn)
void add_sprite(Gfx &g, Ctx &ctx, const int32_t a[8]);

// Image_draw(this, dst, x, y, mask) (a[0..4]) when the display scale is not (1, 1, 1) or the sprite's jgld palette word is the
// scene family's (a sheet cell, or a sprite a queue draw gave a palette): Sprite::drawB (draw16_1001bc40, payload >= 0xfe
// transparent) at (m_20 nx / den + x, m_24 ny / den + y), unscaled or 16.16-stepped. false: not this family's case (gfx.cpp's
// own body draws it). A mask palette object (a[4] != 0) is drawB's key for this call at any scale.
bool image_draw(Gfx &g, Ctx &ctx, const uint32_t *a);

// Surface_drawSprite(this, dst, x, y, alpha bits, mask, flag) (a[0..6]): Sprite::drawAlpha's body for the alpha window, the
// member a root destination selects (the flag-set member of each pair), the display scale in force and a mask palette object.
// The name's registered body is gfx_reports.cpp's (roadmap S2.13), which hands this function every case outside its own
// quarter band at (1, 1, 1) with no mask (log/roadmap/S3/5_merge.md). Returns true.
bool surface_draw_sprite(Gfx &g, Ctx &ctx, const uint32_t *a);

// P23D fix 3 (gfx.h gfx_set_wide_bubbles): a comment bubble's piece the HUD body drew on the root at (x, y) (Sprite::drawG with the
// table lut), also into the wide scene's hr plane after the queue and marked for the world image (inert outside GfxBubbleScope)
void bubble_shade(Gfx &g, Ctx &ctx, const Sprite8 &s, int32_t x, int32_t y, const uint16_t *lut, size_t n);
// P23D fix 4: a golfer comment's bar the overlay drew on the root inside [x0, xe] x [y0, ye] (before: those pixels before it, row-major):
// the pixels it changed are marked for the world image and their bounding box is drawn into the hr plane as a black band through
// drawAlpha's half program (P10F's placement box op), so it lies under the frame's text runs on every path; where the box reaches the
// root's left or right edge it runs on to the line's own end lx0 / lx1 (800-space), past the root; a bar that changed no root pixel
// (before null: wholly past the root's edge) is the band of rows yc - 5 .. yc + 4 between its ends
void bubble_bar(Gfx &g, Ctx &ctx, int32_t x0, int32_t y0, int32_t xe, int32_t ye, const uint16_t *before, int32_t lx0, int32_t lx1,
                int32_t yc);
}  // namespace gfxs
}  // namespace portsim

#endif
