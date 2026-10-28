// Body family `simfoto` of the graphics backend (roadmap S2.11; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// message ball's art and the held-pixel probe of the SimFoto frame and the message ball. Roadmap S3.13: drawWindow has ONE
// body, the text family's drawWindow.frame (gfx_text.cpp), which draws the golfer arm described below (head included) and
// calls this family's probe (gfxs::window_probe / head_probe); S2.11's drawWindow.golfer and its controls 2..4 moved there.
// Specifications: c3ei.cpp (DrawWindow_re, DrawThoughtBubble_re), c3fy.cpp (P5_simfoto), c3fv.cpp (LoadInterfaceArt_re, the
// tables kIa22 / kIa25 and the loads before them), c3da.cpp (DrawFrameR3_re), c3dv.cpp (drawMessageBuffer); functions are
// cited by name, their instruction addresses are in those sources.
//
//   loadInterfaceArt.ballArt
//       the ball's two cuts of loadInterfaceArt(1), each from a loadImage(img, name, 0, 0, 0x100, 2) whose name is an offset
//       into f_strpool_4c95f4: TransPopups (+0x3d1c, the load of kIa20, which kIa21 / kIa22 cut from) -> g_sprites5a53c8
//       cell 29 (+0x4fc) = Blit(0, 0x12c, 0x8c, 0x8c, kind 1, trim 0), kIa22's second op; TransPopups_A (+0x3d04, kIa23's load)
//       -> g_sprites58b680 cell 29 with the same rectangle, kIa25's second op. The other cuts of these two files are the
//       core's and the dialogs family's; loading a file twice gives the same image.
//   drawWindow's golfer arm (S2.11's drawWindow.golfer; since roadmap S3.13 the text family's drawWindow.frame)
//       drawWindow's arm for f_w_4c2e00[2] = sel >= 0 (DrawWindow_re): the window origin moves to ex = x + 0x1c, ey = y + 0x2c
//       and the mode becomes -2; H = g_value587ea0 (the simulation part's window height, at least 0x60 with a golfer), h = the
//       unclamped height 15 * r + 0x20 (r = wrapMessageBuffer's count; the fifth slot, tick's drawWindow passes it).
//       drawFrameR3(ex - 8, ey - 8, 0x1a0, H + 0x10, 1) (gfxb::draw_frame_r3); the ball Surface_stretchBlit(g_sprites5a53c8
//       cell 29, its mask g_sprites58b680 cell 29, root, ex - 0x32, ey - 0x28, 0); drawThoughtBubble(sel, ex - 0x32,
//       ey - 0x28, -1): mode -1 neither fills (mode 0) nor takes the mode-1 arm: composeGolferSprite(sel) and
//       Surface_drawScaled(head, root, ex - 0x32, ey - 0x28, 1, 1, 1, 0) of the head g_sprites5628e8 + 0x2c * (3 * (app +
//       72 * typeBit7Clear(sel)) + thoughtFlag(sel)). The head is drawn by the text family (Surface_drawScaled, roadmap S3.6;
//       palette composeGolferSprite's, S3.5). Every head is a 0x8c x 0x8c cut drawn at the ball's origin, so the head lies
//       inside the ball's square: with the probe on, that square is painted after the head, and its pixels are held. Then ey += 0x10; with
//       h < 0x60, ey += (H - h - 0x10) / 2 truncated toward zero; setDrawTarget(root, g_font51b360, 0, 0, 0);
//       drawMessageBuffer(0x63, ex + 0x64, ey, 0x80007fff) (gfxb::draw_message_buffer: the colour is (mode + 3 != 0 ? 0x7fe7
//       : 0) + 0x80000018 with mode -2). The other selections (sel < -1: the icon variants) are reported.
//   simfoto.begin(l, t, r, b)
//       a marker tick's P5_simfoto puts before the SimFoto frame's first fill, with the frame's outer rectangle [l, r) x [t, b)
//       (the 16-pixel border included); it draws nothing.
//
// THE PROBE (negctl modes >= 0x10000 of this family: a measurement, not a control; port/test/test_gfx_dialogs2.py): at
// simfoto.begin and at the start of drawWindow's golfer arm the rectangle the dialog covers is filled with the probe pattern (the
// colour mode & 0x7fff below 0x20000, else a pseudo-random RGB555 pattern seeded by the mode's low word, position-dependent,
// the same generator as the dialogs family's), and after the ball the head's square is painted with it too. A pixel that
// differs between probe runs depends on what lies under the dialog (the course, roadmap S3) or on the head: it is held.
// The rectangle of the golfer arm is the union of the frame (after drawFrameR3's 16-rounding) and the ball's square.
//
// Negative controls (g_negctl_gfx_simfoto, sg_negctl_set "gfx.family.simfoto"):
//   1 the ball's cut one row lower (y 0x12d)
//   (S2.11's 2 / 3 / 4 for the golfer arm are the text family's 12 / 13 / 14 since roadmap S3.13)
#include <algorithm>
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"

namespace portsim {

int g_negctl_gfx_simfoto = 0;

namespace gfxs {
// the probe pattern over [l, r) x [t, b) of the root surface (shared with the results family, gfx_results.cpp)
void probe_fill(Gfx &g, Ctx &ctx, int mode, int32_t l, int32_t t, int32_t r, int32_t b) {
  if (mode < 0x10000) return;
  g.ensure_root(ctx);
  l = std::max(l, 0);
  t = std::max(t, 0);
  r = std::min(r, g.root.w);
  b = std::min(b, g.root.h);
  uint32_t x = 0x9e3779b9u ^ static_cast<uint32_t>(mode & 0xffff);
  for (int32_t yy = 0; yy < g.root.h; ++yy)
    for (int32_t xx = 0; xx < g.root.w; ++xx) {
      uint16_t w = static_cast<uint16_t>(mode & 0x7fff);
      if (mode >= 0x20000) {   // xorshift32, one step per pixel of the whole surface (so a pixel's word is its own)
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        w = static_cast<uint16_t>(x & 0x7fff);
      }
      if (xx >= l && xx < r && yy >= t && yy < b)
        g.root.px[static_cast<size_t>(yy) * static_cast<size_t>(g.root.w) + static_cast<size_t>(xx)] = w;
    }
}
}  // namespace gfxs

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_simfoto); }
bool control(int m) { return mode() == m; }

uint32_t pool(Ctx &ctx) { return state_va(ctx, &GameState::f_strpool_4c95f4); }

// ------------------------------------------------------------------------------------------------ the art
void b_ball_art(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  const int32_t y = control(1) ? 0x12D : 0x12C;
  auto cut = [&](uint32_t name, uint32_t dest) {
    Image8 img;
    if (gfxb::load_image(g, ctx, pool(ctx) + name, 0, 0, 0x100, 2, img) != 0) {
      g.unsup("loadInterfaceArt.ballArt: a file that did not load");
      return;
    }
    gfxb::crop_sprite(g, dest, img, 0, y, 0x8C, 0x8C, false);
  };
  cut(0x3D1C, state_va(ctx, &GameState::g_sprites5a53c8) + 0x2C * 29);   // TransPopups
  cut(0x3D04, state_va(ctx, &GameState::g_sprites58b680) + 0x2C * 29);   // TransPopups_A
}

// ------------------------------------------------------------------------------------------------ the message ball
// drawFrameR3's 16-rounding (c3da.cpp round16), for the probe's rectangle
void round16(int32_t &pos, int32_t &size) {
  const int32_t low = size & 0xf;
  if (low != 0) {
    const int32_t r = 0xf - low;
    size += r;
    pos -= r / 2;
  }
}

void b_simfoto_begin(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  gfxs::probe_fill(g, ctx, mode(), static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
                   static_cast<int32_t>(a[3]));
}

const GfxBodyDef kBodies[] = {
    {"loadInterfaceArt.ballArt", b_ball_art},
    {"simfoto.begin", b_simfoto_begin},
};

}  // namespace

namespace gfxs {
// drawWindow's golfer arm, for the text family's drawWindow.frame (roadmap S3.13): with this family's probe on, the frame's
// rectangle (fx, fy, 0x1a0, fh after drawFrameR3's 16-rounding) and the ball's square at (bx, by) get the probe pattern
void window_probe(Gfx &g, Ctx &ctx, int32_t fx, int32_t fy, int32_t fh, int32_t bx, int32_t by) {
  if (mode() < 0x10000) return;
  int32_t px = fx, pw = 0x1A0, py = fy, ph = fh;
  round16(px, pw);
  round16(py, ph);
  probe_fill(g, ctx, mode(), px, py, add32(px, pw), add32(py, ph));
  probe_fill(g, ctx, mode(), bx, by, add32(bx, 0x8C), add32(by, 0x8C));
}
// after the head: with the probe on, the ball's square painted again (its pixels held)
void head_probe(Gfx &g, Ctx &ctx, int32_t bx, int32_t by) {
  if (mode() >= 0x10000) probe_fill(g, ctx, mode(), bx, by, add32(bx, 0x8C), add32(by, 0x8C));
}
}  // namespace gfxs

extern const GfxFamily kGfxFamily_simfoto = {"simfoto", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
