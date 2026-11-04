// Body family `scene` of the software graphics backend (roadmap S3.5; portsim/sim/gfx_scene.h, listed in gfx_families.inc):
// the course view's sprites. Functions of the original are cited by name; their instruction addresses are in the C3 / C4
// hook sources named at each body (shim/src/re/*.cpp) and the matched jgld sources (re/match/jgld_*.cpp).
//
//   drawQueuedSprite.draw(i)      drawQueuedSprite (c3ei.cpp DrawQueuedSprite_re), the drawing of queue entry i: d = 0x10 (0xd
//                                 above a 1000-pixel screen, 0xa from 0x4b0, 0x10 again for a golfer entry); shade 0 or flags -1:
//                                 Surface_blit2 at the scale in force; otherwise the display scale (s, s, d) with s = shade *
//                                 g_zoomScale (shade < 0: s = -(shade * zoom), d doubled), then flags 0x100: composeGolferSprite's
//                                 palette layers and Sprite_draw with g_images[0]; 0x800 / 0x400: Sprite_drawScaled (reported);
//                                 0x200: Surface_drawSprite at alpha 0.6 with g_images[flags & 0x1ff]; else Sprite_draw with
//                                 g_images[flags]; then the scale restored to (z, z, 4), z = g_zoomScale when g_spriteFlag8371f8
//                                 is set, else g_sprite58b384
//   drawQueuedSprite.label(i)     the same function's golfer name label (after the simulation part built the text):
//                                 setDrawTarget with the font by zoom, drawCenteredText and the two Widget_fillRect bars
//   Sprite_draw(this, dst, x, y, mode, mask)              c3da.cpp SpriteDraw_re: handle slot +0x78 = Sprite::blt16a
//   Surface_drawSprite(this, dst, x, y, alpha, mask, flag) c3da.cpp SurfaceDrawSprite_re: handle slot +0x94 = Sprite::drawAlpha
//
// jgld (re/match/jgld_sprite.cpp, jgld_blit16b.cpp, jgld_blitY17.cpp, jgld_blit16.cpp):
//   Sprite::blt16a(dst, x, y, table, key): a 16-bit destination and an 8-bit sprite, then the sprite's palette word +0x10 =
//     key when key != 0 (never restored), draw16_100298b0: payload 0xff transparent, < 0xf8 the palette's word, 0xf8 + k the
//     word (k << 15) | destination of the plane table (the 4th argument; log/roadmap/S3/5_notes.md cites the operand)
//   Sprite::drawG(dst, x, y, table, key): +0x10 = key for the call, draw16_10028630: 0xff transparent, 0xf8 the destination
//     through the 16-bit table, the others the palette's word
//   Sprite::drawAlpha(dst, x, y, alpha, pal, flag): +0x10 = pal for the call; alpha picks the blend body by eighths with the
//     tolerance 0.03125; on an RGB555 surface (format word 1) the second body of each pair runs, whose 16-bit store keeps the
//     shifted fields without the final >> 4 (only red's lowest bit survives, the jgld_1.tsv finding). Modelled: alpha 0.5
//     (draw16_10041f80: (P + D) / 2 per field group) and 0.6 (draw16_1004ca10: ((3 D + P) >> 2 + P) >> 1); flag != 0 skips
//     payload 0xf8..0xfe as well as 0xff
//   The shared skeleton: |nx| == |den| and |ny| == |den| is the unscaled copy at (x, y) clipped to the destination's clip rect;
//   otherwise fx = (float)|nx| / den, the rectangle (x + (int)fx, y + (int)fy, (int)(w fx), (int)(h fy)) clipped, steps
//   (den << 16) / |n| in 16.16, source column floor((offX step + i step) / 2^16) per pixel and row floor((offY step + j step)
//   / 2^16) per row (the row carry adds one pitch: its two-entry skip table is read with index -1 on a carry, 5_notes.md). A negative numerator (the mirrored bodies) is reported.
//
// The palette a sprite is drawn with (U-1332 settled here): the jgld Sprite's +0x10 word, which Sprite::blt16a sets from the
// queue entry's palette object g_images[flags] (or g_images[0] for a golfer) and never restores; a sheet cell starts with 0
// (S3.4: every recorded cell), so a cell draws with the palette of its own queue entry. Palette objects are S3.6's
// (gfxh::palette_of: the RGB555 words a loadImage with a palette argument filled).
//
// Negative controls (g_negctl_gfx_scene, sg_negctl_set "gfx.family.scene"):
//   1 the plane blit takes plane k + 1 for payload 0xf8 + k
//   2 the 16.16 step is one unit larger (scaled draws read the next source pixel sooner)
//   3 composeGolferSprite's part palettes read one g_images slot too far (partAt(idx) = g_images[idx + 2])
//   4 drawQueuedSprite ignores the display scale (every entry drawn unscaled)
//   5 the plane blit restores the sprite's palette word after the call (a cell keeps no palette between draws)
//   6 the label's name text drawn one pixel lower
//   7 TEST-ONLY, not a control: every pixel a scene body writes is stored inverted (^ 0x7fff); a run in this mode against a
//     normal run marks the pixels whose final value a scene draw gave (port/test/test_gfx_scene.py's sprite-pixel gate)
//   8 TEST-ONLY, not a control: a sheet cell with no backend sprite is reported (default: counted, nothing drawn)
//   9 drawAlpha on the root takes the flag-cleared member of each window's pair (the 565-mask bodies)
//   10 Image_draw ignores its mask (the sprite's own palette word)
//   11 the composeGolferSprite body composes nothing
//   12 the addSprite body draws nothing
//   13 an entry past the golfer table (palette flags & 0x1ff >= 0x19c: updateVipResident's standing figure) draws nothing
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/gfx_scene.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/golfers.h"
#include "portsim/sim/gfx_hud.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_live.h"   // G32: the wide scene

namespace portsim {

int g_negctl_gfx_scene = 0;

namespace gfxf {
const int32_t *blit_scale(Gfx &g);  // gfx_prim_fill.cpp (roadmap S2.4): Display::setScale's three words
}

namespace gfxs {

const std::array<uint16_t, 5 * 0x8000> &color16_planes() {
  static const std::array<uint16_t, 5 * 0x8000> t = [] {
    std::array<uint16_t, 5 * 0x8000> a{};
    auto pack = [](uint32_t r, uint32_t g, uint32_t b) {
      return static_cast<uint16_t>((((r & 0xff) >> 3) << 10) | (((g & 0xff) >> 3) << 5) | ((b & 0xff) >> 3));
    };
    auto bright = [](uint32_t v) { return std::min<uint32_t>(v + 0x20, 0xff); };   // brightenComponent
    for (uint32_t k = 0; k < 5; ++k)
      for (uint32_t r = 0; r < 0x100; r += 8)
        for (uint32_t g = 0; g < 0x100; g += 8)
          for (uint32_t b = 0; b < 0x100; b += 8) {
            const uint32_t step = 0x400 + 0x100 * k;
            const auto fine = [&](uint32_t c) { return (((c / 8) * step) >> 8) & 0xff; };
            a[k * 0x8000 + pack(r, g, b)] =
                k < 4 ? pack(fine(r), fine(g), fine(b)) : pack(bright(r), bright(g), bright(b));
          }
    return a;
  }();
  return t;
}

}  // namespace gfxs

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_scene); }

// development: SG_GFX_SCENE_LOG=<file> appends one line per queue draw and per report of this family
void dlog(const std::string &s) {
  static FILE *fh = std::getenv("SG_GFX_SCENE_LOG") ? std::fopen(std::getenv("SG_GFX_SCENE_LOG"), "a") : nullptr;
  if (!fh) return;
  std::fprintf(fh, "%s\n", s.c_str());
  std::fflush(fh);
}
uint32_t u(int32_t v) { return static_cast<uint32_t>(v); }
// every pixel a scene body writes; TEST-ONLY mode 7 (the sprite-pixel gate's mask run, not a control) stores it inverted
inline void put(uint16_t &w, uint32_t v) { w = static_cast<uint16_t>(mode() == 7 ? v ^ 0x7fffu : v); }

struct SceneState {
  std::map<uint32_t, uint32_t> sprite_pal;  // sprite key -> the palette object its jgld Sprite's +0x10 word names
  std::array<uint16_t, 256> work{};         // composeGolferSprite's work buffer (RGB555 words), kept between calls
  bool work_seeded = false;                 // false until a layer filled an entry (U-1342)
  uint32_t queue_calls = 0;                 // renderSpriteQueue returns so far (the frame code's once-per-frame P4 call)
  std::vector<uint32_t> queue_capture;      // TEST-ONLY: call ordinals whose root surface is written out
  std::map<uint32_t, uint64_t> no_sprite;   // sheet cells a draw read that the backend holds no sprite for (draw counts)
  std::map<uint32_t, std::string> no_sprite_where;   // the loaded sheets holding that cell at its first such draw
};
SceneState &st(Gfx &g) { return g.state_of<SceneState>("scene"); }

struct Scale {
  int32_t nx, ny, den;
};
Scale cur_scale(Gfx &g) {
  const int32_t *s = gfxf::blit_scale(g);
  return {s[0], s[1], s[2]};
}
void set_scale(Gfx &g, Ctx &ctx, int32_t nx, int32_t ny, int32_t den) {
  static const GfxBody b = gfxb::find_body("Display::setScale");
  uint32_t a[12] = {u(nx), u(ny), u(den)};
  if (b)
    b(g, ctx, a, 3);
  else
    g.unsup("scene: no Display::setScale body");
}
void call_body(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  uint32_t a[12] = {};
  int n = 0;
  for (uint32_t v : args) a[n++] = v;
  const GfxBody b = gfxb::find_body(name);
  if (b)
    b(g, ctx, a, n);
  else
    g.unsup(std::string("scene: no body for ") + name);
}

bool in_cell_pool(Ctx &ctx, uint32_t key) {
  const uint32_t base = state_va(ctx, &GameState::g_cellPool);
  return key >= base && key - base < sizeof(GameState::g_cellPool);
}
// TEST-ONLY diagnosis of a cell without a sprite: every loaded sheet (first cell != -1) whose cells hold it, as
// `<sheet>:<index>:<first cell>:<cells of that sheet with a sprite>/<cells>:<path>`; `-` when none holds it
std::string sheet_of_cell(Gfx &g, Ctx &ctx, uint32_t key) {
  const GameState &gs = ctx.gs();
  const int32_t c = static_cast<int32_t>((key - state_va(ctx, &GameState::g_cellPool)) / 0x2cu);
  std::string out;
  for (int32_t i = 0; i < static_cast<int32_t>(sizeof(gs.g_sheetFirstCell) / 4); ++i) {
    const int32_t f = gs.g_sheetFirstCell[i];
    const int32_t n = std::max(1, gs.g_sheetCols[i] * gs.g_sheetRows[i]);
    if (f < 0 || c < f || c >= f + n) continue;
    int32_t have = 0;
    for (int32_t k = 0; k < n; ++k)
      have += g.sprites.count(state_va(ctx, &GameState::g_cellPool) + 0x2cu * static_cast<uint32_t>(f + k)) ? 1 : 0;
    std::string path;
    for (uint32_t k = 0; k < 50; ++k) {
      const char ch = static_cast<char>(gs.g_sheetPath[static_cast<size_t>(i) * 50 + k]);
      if (ch == 0) break;
      path += ch == ' ' ? '*' : ch;   // the dump is space-separated: a space in a path is written as '*'
    }
    out += (out.empty() ? "" : ",") + std::to_string(i) + ":" + std::to_string(c - f) + ":" + std::to_string(f) + ":" +
           std::to_string(have) + "/" + std::to_string(n) + ":" + path;
  }
  return out.empty() ? "-" : out;
}
uint32_t images_slot(Ctx &ctx, uint32_t k) { return state_va(ctx, &GameState::g_images) + 0x58u * k; }

// the palette words a Sprite draw uses: the sprite's +0x10 word after the key argument (persistent: Sprite::blt16a; for the
// call only: drawG / drawAlpha). A sheet cell's word starts at 0; any other cut starts with its source image's palette
// (cropSprite: the image handle's palette, the words the core keeps with the sprite). NULL: a NULL word, whose fallback
// (g_palClient1's palette) is not modelled.
const uint16_t *sprite_palette(Gfx &g, Ctx &ctx, uint32_t key, const Sprite8 &s, uint32_t pal_obj, bool persistent,
                               const char *who) {
  SceneState &ss = st(g);
  uint32_t use = 0;
  bool own = false;
  auto it = ss.sprite_pal.find(key);
  if (it != ss.sprite_pal.end())
    use = it->second;
  else
    own = !in_cell_pool(ctx, key);
  if (pal_obj != 0 && gfxh::palette_of(g, pal_obj, false) != nullptr) {
    if (persistent && mode() != 5) ss.sprite_pal[key] = pal_obj;
    use = pal_obj;
    own = false;
  }
  if (use != 0) {
    const std::array<uint16_t, 256> *p = gfxh::palette_of(g, use, false);
    if (p) return p->data();
  }
  if (own) return s.pal.data();
  dlog(std::string(who) + " nopal key=" + std::to_string(key) + " pal_obj=" + std::to_string(pal_obj) + " use=" + std::to_string(use));
  g.unsup(std::string(who) + ": a sprite with no palette (its +0x10 word is 0: g_palClient1's palette)");
  return nullptr;
}

// ------------------------------------------------------------------------------------------------ the blitter skeleton
// One covered destination pixel: op(payload byte, destination word).
template <class Op>
void blit16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y, Scale sc, const char *who, Op &op) {
  if (sc.den == 0) {
    g.unsup(std::string(who) + ": a display scale with denominator 0");
    return;
  }
  const int32_t ax = abs32(sc.nx), ay = abs32(sc.ny), ad = abs32(sc.den);
  if (sc.nx < 0 || sc.ny < 0) {
    g.unsup(std::string(who) + ": a mirrored display scale");
    return;
  }
  if ((ax == ad && ay == ad) || mode() == 4) {
    const int32_t l = std::max(x, 0), t = std::max(y, 0);
    const int32_t r = static_cast<int32_t>(std::min<int64_t>(static_cast<int64_t>(x) + s.w, d.w));
    const int32_t b = static_cast<int32_t>(std::min<int64_t>(static_cast<int64_t>(y) + s.h, d.h));
    for (int32_t yy = t; yy < b; ++yy)
      for (int32_t xx = l; xx < r; ++xx)
        op(s.px[static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x)],
           d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)]);
    return;
  }
  if (ax == 0 || ay == 0) {
    g.unsup(std::string(who) + ": a zero scale numerator (the original's step division faults)");
    return;
  }
  // (float)|n| / den: the quotient rounded to double (the x87 at 53 bits), then to float (fstp dword)
  const float fx = static_cast<float>(static_cast<double>(ax) / static_cast<double>(sc.den));
  const float fy = static_cast<float>(static_cast<double>(ay) / static_cast<double>(sc.den));
  const int32_t rx = add32(x, static_cast<int32_t>(fx)), ry = add32(y, static_cast<int32_t>(fy));
  const int32_t rw = static_cast<int32_t>(static_cast<double>(s.w) * static_cast<double>(fx));
  const int32_t rh = static_cast<int32_t>(static_cast<double>(s.h) * static_cast<double>(fy));
  const int32_t l = std::max(rx, 0), t = std::max(ry, 0);
  const int32_t r = static_cast<int32_t>(std::min<int64_t>(static_cast<int64_t>(rx) + rw, d.w));
  const int32_t b = static_cast<int32_t>(std::min<int64_t>(static_cast<int64_t>(ry) + rh, d.h));
  if (l >= r || t >= b) return;
  const uint32_t one = mode() == 2 ? 1u : 0u;
  const uint32_t stepx = u(div32(static_cast<int32_t>(u(sc.den) << 16), ax)) + one;
  const uint32_t stepy = u(div32(static_cast<int32_t>(u(sc.den) << 16), ay)) + one;
  const uint32_t a0 = stepx * u(l - rx), b0 = stepy * u(t - ry);
  bool outside = false;
  for (int32_t j = 0; j < b - t; ++j) {
    const uint64_t sy = (static_cast<uint64_t>(b0) + static_cast<uint64_t>(j) * stepy) >> 16;
    for (int32_t i = 0; i < r - l; ++i) {
      const uint64_t sx = (static_cast<uint64_t>(a0) + static_cast<uint64_t>(i) * stepx) >> 16;
      const uint64_t idx = sy * static_cast<uint64_t>(s.w) + sx;
      if (idx >= s.px.size()) {
        outside = true;
        continue;
      }
      op(s.px[static_cast<size_t>(idx)],
         d.px[static_cast<size_t>(t + j) * static_cast<size_t>(d.w) + static_cast<size_t>(l + i)]);
    }
  }
  if (outside) g.unsup(std::string(who) + ": a scaled read past the sprite's pixels");
}

// draw16_100298b0 (Sprite::blt16a's body)
struct PlaneOp {
  Gfx &g;
  const uint16_t *pal;
  bool bad = false;
  void operator()(uint8_t c, uint16_t &w) {
    if (c >= 0xff) return;
    if (c < 0xf8) {
      put(w, pal[c]);
      return;
    }
    const auto &t = gfxs::color16_planes();
    const uint32_t k = static_cast<uint32_t>(c - 0xf8) + (mode() == 1 ? 1u : 0u);
    const uint32_t idx = (k << 15) | w;
    if (idx >= t.size()) {
      bad = true;
      return;
    }
    put(w, t[idx]);
  }
};
// draw16_10028630 (Sprite::drawG's 16-bit body)
struct ShadeOp {
  const uint16_t *pal;
  const uint16_t *lut;
  size_t lut_n;
  bool bad = false;
  void operator()(uint8_t c, uint16_t &w) {
    if (c >= 0xff) return;
    if (c == 0xf8) {
      if (w >= lut_n) {
        bad = true;
        return;
      }
      put(w, lut[w]);
      return;
    }
    put(w, pal[c]);
  }
};
// BEGIN GENERATED drawAlpha blend programs (log/roadmap/S3/5_tools/alpha_ops.py gen)
// One step per instruction of the pixel block: {op, dst, src kind, src}; registers 0 eax 1 ebx 2 ecx 3 edx 4 esi;
// src kind 0 a register, 1 an immediate, 2 the palette word P, 3 the destination word D. The store keeps eax's low
// 16 bits. Read from re/match/jgld_blit16.cpp: all eight loops of each body hold this block (the tool checks it).
struct AStep {
  uint8_t op, dst, kind;
  uint32_t src;
};
struct AProgram {
  const char *body;
  const AStep *steps;
  size_t n;
};
const AStep kA_10041f80[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_10043430[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_10044940[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_10045e50[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_100473b0[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_10048910[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x4}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x4}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_10049ea0[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x4}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x4}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_1004b430[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x2}, {2, 1, 1, 0x2}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_1004ca10[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x2}, {2, 1, 1, 0x2}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_1004dff0[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x3}, {2, 1, 1, 0x3}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_1004f5d0[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x3}, {2, 1, 1, 0x3}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_10050bb0[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x1}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x1}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_100521d0[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x1}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x1}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_100537f0[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_10054e20[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {3, 2, 1, 0xf81f0}, {3, 3, 1, 0x7e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0xf81f0}, {3, 1, 1, 0x7e00}, {4, 0, 0, 1}};
const AStep kA_1002d890[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_1002ed60[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_10030290[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_100317c0[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_10032d60[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_10034300[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x4}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x4}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_100358b0[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x4}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x4}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_10036e60[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x2}, {2, 1, 1, 0x2}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_10038460[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x2}, {2, 1, 1, 0x2}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_10039a60[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x3}, {2, 1, 1, 0x3}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_1003b060[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x3}, {2, 1, 1, 0x3}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x1}, {2, 1, 1, 0x1}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_1003c660[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x1}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x1}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_1003dca0[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x3}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x1}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x3}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x1}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_1003f2e0[] = {{0, 0, 3, 0}, {0, 3, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
const AStep kA_10040930[] = {{0, 3, 3, 0}, {0, 0, 2, 0}, {1, 0, 1, 0x4}, {1, 3, 1, 0x4}, {0, 1, 0, 0}, {0, 2, 0, 3}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {3, 2, 1, 0x7c1f0}, {3, 3, 1, 0x3e00}, {0, 4, 0, 0}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {4, 0, 0, 2}, {1, 0, 1, 0x2}, {5, 0, 0, 4}, {0, 4, 0, 1}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 1, 0, 3}, {1, 1, 1, 0x2}, {5, 1, 0, 4}, {4, 0, 0, 2}, {4, 1, 0, 3}, {2, 0, 1, 0x4}, {2, 1, 1, 0x4}, {3, 0, 1, 0x7c1f0}, {3, 1, 1, 0x3e00}, {4, 0, 0, 1}, {2, 0, 1, 0x4}};
// [0, 15): the flag-cleared members (destination format word +4 == 1), [15, 30): the flag-set members
const AProgram kAlphaPrograms[] = {
    {"draw16_10041f80", kA_10041f80, sizeof(kA_10041f80) / sizeof(kA_10041f80[0])},
    {"draw16_10043430", kA_10043430, sizeof(kA_10043430) / sizeof(kA_10043430[0])},
    {"draw16_10044940", kA_10044940, sizeof(kA_10044940) / sizeof(kA_10044940[0])},
    {"draw16_10045e50", kA_10045e50, sizeof(kA_10045e50) / sizeof(kA_10045e50[0])},
    {"draw16_100473b0", kA_100473b0, sizeof(kA_100473b0) / sizeof(kA_100473b0[0])},
    {"draw16_10048910", kA_10048910, sizeof(kA_10048910) / sizeof(kA_10048910[0])},
    {"draw16_10049ea0", kA_10049ea0, sizeof(kA_10049ea0) / sizeof(kA_10049ea0[0])},
    {"draw16_1004b430", kA_1004b430, sizeof(kA_1004b430) / sizeof(kA_1004b430[0])},
    {"draw16_1004ca10", kA_1004ca10, sizeof(kA_1004ca10) / sizeof(kA_1004ca10[0])},
    {"draw16_1004dff0", kA_1004dff0, sizeof(kA_1004dff0) / sizeof(kA_1004dff0[0])},
    {"draw16_1004f5d0", kA_1004f5d0, sizeof(kA_1004f5d0) / sizeof(kA_1004f5d0[0])},
    {"draw16_10050bb0", kA_10050bb0, sizeof(kA_10050bb0) / sizeof(kA_10050bb0[0])},
    {"draw16_100521d0", kA_100521d0, sizeof(kA_100521d0) / sizeof(kA_100521d0[0])},
    {"draw16_100537f0", kA_100537f0, sizeof(kA_100537f0) / sizeof(kA_100537f0[0])},
    {"draw16_10054e20", kA_10054e20, sizeof(kA_10054e20) / sizeof(kA_10054e20[0])},
    {"draw16_1002d890", kA_1002d890, sizeof(kA_1002d890) / sizeof(kA_1002d890[0])},
    {"draw16_1002ed60", kA_1002ed60, sizeof(kA_1002ed60) / sizeof(kA_1002ed60[0])},
    {"draw16_10030290", kA_10030290, sizeof(kA_10030290) / sizeof(kA_10030290[0])},
    {"draw16_100317c0", kA_100317c0, sizeof(kA_100317c0) / sizeof(kA_100317c0[0])},
    {"draw16_10032d60", kA_10032d60, sizeof(kA_10032d60) / sizeof(kA_10032d60[0])},
    {"draw16_10034300", kA_10034300, sizeof(kA_10034300) / sizeof(kA_10034300[0])},
    {"draw16_100358b0", kA_100358b0, sizeof(kA_100358b0) / sizeof(kA_100358b0[0])},
    {"draw16_10036e60", kA_10036e60, sizeof(kA_10036e60) / sizeof(kA_10036e60[0])},
    {"draw16_10038460", kA_10038460, sizeof(kA_10038460) / sizeof(kA_10038460[0])},
    {"draw16_10039a60", kA_10039a60, sizeof(kA_10039a60) / sizeof(kA_10039a60[0])},
    {"draw16_1003b060", kA_1003b060, sizeof(kA_1003b060) / sizeof(kA_1003b060[0])},
    {"draw16_1003c660", kA_1003c660, sizeof(kA_1003c660) / sizeof(kA_1003c660[0])},
    {"draw16_1003dca0", kA_1003dca0, sizeof(kA_1003dca0) / sizeof(kA_1003dca0[0])},
    {"draw16_1003f2e0", kA_1003f2e0, sizeof(kA_1003f2e0) / sizeof(kA_1003f2e0[0])},
    {"draw16_10040930", kA_10040930, sizeof(kA_10040930) / sizeof(kA_10040930[0])},
};
// END GENERATED drawAlpha blend programs

// Sprite::drawAlpha's body for alpha on an RGB555 surface (format word 1: the second body of each pair; tolerance
// g_1011d378 = 0.03125), an index into kAlphaPrograms; -1 draws nothing (alpha outside [0, 1] returns 3; at or below the
// tolerance returns 0), -2 the opaque arm (100 - tolerance < alpha, which alpha <= 1 never reaches)
int alpha_body(float alpha) {
  const double a = alpha, e = 0.03125;
  if (a < 0.0 || a > 1.0) return -1;
  if (a > e + 0.5) {
    if (a > e + 0.75) {
      if (a > e + 0.875) return 100.0 - e < a ? -2 : 6;   // draw16_10049ea0
      if (a < 0.875 - e) return 12;                        // draw16_100521d0
      return 4;                                            // draw16_100473b0
    }
    if (a < 0.75 - e) {
      if (a > e + 0.625) return 14;                        // draw16_10054e20
      if (a < 0.625 - e) return 10;                        // draw16_1004f5d0
      return 8;                                            // draw16_1004ca10
    }
    return 2;                                              // draw16_10044940
  }
  if (a < 0.5 - e) {
    if (a > e + 0.25) {
      if (a > e + 0.375) return 9;                         // draw16_1004dff0
      if (a < 0.375 - e) return 13;                        // draw16_100537f0
      return 7;                                            // draw16_1004b430
    }
    if (a < 0.25 - e) {
      if (a > e + 0.125) return 11;                        // draw16_10050bb0
      if (a < 0.125 - e) return a > e ? 5 : -1;            // draw16_10048910
      return 3;                                            // draw16_10045e50
    }
    return 1;                                              // draw16_10043430
  }
  return 0;                                                // draw16_10041f80
}

// one blend body: 0xff transparent, flag != 0 also skips 0xf8..0xfe; the program over P = the palette word, D = the
// destination word, the store keeping eax's low 16 bits
struct AlphaOp {
  const uint16_t *pal;
  const AProgram *prog;
  bool flag;
  // P9D (gfx.h gfx_set_hud_modal): with `rec` (the quarter member draw16_1002ed60 on the root surface while the modal capture runs)
  // each written pixel is recorded as a kind-4 layer pixel; n / one_src / src0 tell a dim of every root pixel with one source word
  Gfx *rec = nullptr;
  size_t n = 0;
  bool one_src = true;
  uint16_t src0 = 0;
  void operator()(uint8_t c, uint16_t &w) {
    if (c >= 0xff) return;
    if (flag && c >= 0xf8) return;
    const uint16_t before = w;
    struct Note {
      AlphaOp &o;
      uint16_t &w;
      uint16_t before, src;
      ~Note() {
        if (!o.rec) return;
        o.rec->note_blend(static_cast<size_t>(&w - o.rec->root.px.data()), before, w, 4, src, 0);
        if (o.n++ == 0) o.src0 = src;
        else if (src != o.src0) o.one_src = false;
      }
    } note{*this, w, before, pal[c]};
    uint32_t r[5] = {0, 0, 0, 0, 0};
    for (size_t k = 0; k < prog->n; ++k) {
      const AStep &s = prog->steps[k];
      const uint32_t v = s.kind == 0 ? r[s.src] : s.kind == 1 ? s.src : s.kind == 2 ? pal[c] : w;
      uint32_t &d = r[s.dst];
      switch (s.op) {
        case 0: d = v; break;
        case 1: d <<= (v & 31); break;
        case 2: d >>= (v & 31); break;
        case 3: d &= v; break;
        case 4: d += v; break;
        default: d -= v; break;
      }
    }
    put(w, r[0]);
  }
};

// draw16_10024140 (Sprite::blt16c's body): payload 0xf8..0xff transparent; the palette word P reduced to one brightness
// byte k = (((P & 0x7c00) >> 7) + ((P & 0x3e0) >> 1) + ((P & 0x1f) << 3)) >> 2 (16-bit adds), and each 5-bit field f of the
// caller's colour C stored as (f * k) >> 8 (mul dl: an 8 x 8 product)
struct ColourOp {
  const uint16_t *pal;
  uint16_t colour;
  void operator()(uint8_t c, uint16_t &w) {
    if (c >= 0xf8) return;
    const uint32_t p = pal[c];
    const uint32_t k = ((((p & 0x7c00) >> 7) + ((p & 0x3e0) >> 1) + ((p & 0x1f) << 3)) & 0xffff) >> 2 & 0xff;
    const uint32_t b = ((colour & 0x1fu) * k) >> 8, g = (((colour & 0x3e0u) >> 5) * k) >> 8,
                   r = (((colour & 0x7c00u) >> 10) * k) >> 8;
    put(w, b | (g << 5) | (r << 10));
  }
};

// draw16_1001bc40 (Sprite::drawB's 8-to-16 body): payload 0xfe and 0xff transparent, the others the palette's word
struct DrawBOp {
  const uint16_t *pal;
  void operator()(uint8_t c, uint16_t &w) {
    if (c >= 0xfe) return;
    put(w, pal[c]);
  }
};

// ------------------------------------------------------------------------------------------------ the exe wrappers
// the sprite behind a SpriteRef key and the root destination; nullptr: nothing drawn (a NULL handle returns 7 in the
// original; a sheet cell the backend never decoded is reported)
const Sprite8 *source(Gfx &g, Ctx &ctx, uint32_t key, uint32_t dst, const char *who) {
  g.ensure_root(ctx);
  if (dst == 0) return nullptr;
  if (dst != g.root_va(ctx)) {
    g.unsup(std::string(who) + ": a destination other than the root surface");
    return nullptr;
  }
  auto it = g.sprites.find(key);
  if (it == g.sprites.end()) {
    // a sheet cell flic::sheet_to_gfx left without a sprite: its cut kept nothing (flic.cpp crop: result 3, an empty or
    // out-of-range rectangle, or 9, every pixel 0xff), so the original's handle draws an empty sprite, or the cell lies outside every loaded sheet (handle NULL: Sprite_draw
    // returns 7); either way nothing is drawn. The keys are kept (gfx_scene.dumpNoSprite) and port/test/test_gfx_scene.py checks
    // each against the original's record in a segment start image (kept size 0 or handle NULL); mode 8 reports them instead.
    if (in_cell_pool(ctx, key)) {
      SceneState &s = st(g);
      if (s.no_sprite[key]++ == 0) s.no_sprite_where[key] = sheet_of_cell(g, ctx, key);
      if (mode() == 8) g.unsup(std::string(who) + ": a sheet cell the backend has no sprite for");
    }
    return nullptr;
  }
  return &it->second;
}

// the wrappers' trim offset at the display scale in force: m_20 * nx / den + x (imul, idiv)
int32_t trim(int32_t m, int32_t n, int32_t den) { return den == 0 ? 0 : div32(mul32(m, n), den); }

// Sprite_draw(this, dst, x, y, table, mask) -> Sprite::blt16a; `planes` true: the table is g_color16Lut's
void sprite_draw(Gfx &g, Ctx &ctx, uint32_t key, int32_t x, int32_t y, uint32_t pal_obj, const char *who) {
  const Sprite8 *s = source(g, ctx, key, g.root_va(ctx), who);
  if (!s) return;
  const Scale sc = cur_scale(g);
  if (sc.den == 0) {
    g.unsup(std::string(who) + ": a display scale with denominator 0");
    return;
  }
  const uint16_t *pal = sprite_palette(g, ctx, key, *s, pal_obj, true, who);
  if (!pal) return;
  PlaneOp op{g, pal};
  blit16(g, *s, g.root, add32(trim(s->left, sc.nx, sc.den), x), add32(trim(s->top, sc.ny, sc.den), y), sc, who, op);
  if (op.bad) g.unsup(std::string(who) + ": a plane payload past build16BitColorTable's five blocks");
}

// Sprite_drawScaled(this, dst, x, y, colour, sx, sy, den, mask, extra) -> Sprite::blt16c at (x + m_20 sx / den, y + m_24 sy /
// den) under the scale (sx, sy, den), which it sets and restores; blt16c keeps the key as the sprite's palette word
void sprite_draw_scaled(Gfx &g, Ctx &ctx, uint32_t key, int32_t x, int32_t y, uint32_t colour, Scale sc, uint32_t pal_obj,
                        const char *who) {
  const Sprite8 *s = source(g, ctx, key, g.root_va(ctx), who);
  if (!s) return;
  if (sc.den == 0) {
    g.unsup(std::string(who) + ": a display scale with denominator 0");
    return;
  }
  const uint16_t *pal = sprite_palette(g, ctx, key, *s, pal_obj, true, who);
  if (!pal) return;
  ColourOp op{pal, static_cast<uint16_t>(colour)};
  blit16(g, *s, g.root, add32(trim(s->left, sc.nx, sc.den), x), add32(trim(s->top, sc.ny, sc.den), y), sc, who, op);
}

// Surface_blit2(this, dst, x, y, g_halfBrightLut, 0) -> Sprite::drawG (this +8 != 0 draws nothing)
void surface_blit2(Gfx &g, Ctx &ctx, uint32_t key, int32_t x, int32_t y, const char *who) {
  if (!gfx_is_local_key(key) && ctx.mem.load<uint32_t>(key + 8) != 0) return;
  const Sprite8 *s = source(g, ctx, key, g.root_va(ctx), who);
  if (!s) return;
  const Scale sc = cur_scale(g);
  if (sc.den == 0) {
    g.unsup(std::string(who) + ": a display scale with denominator 0");
    return;
  }
  const uint16_t *pal = sprite_palette(g, ctx, key, *s, 0, false, who);
  if (!pal) return;
  const auto &lut = gfxh::half_bright_table();
  ShadeOp op{pal, lut.data(), lut.size()};
  blit16(g, *s, g.root, add32(trim(s->left, sc.nx, sc.den), x), add32(trim(s->top, sc.ny, sc.den), y), sc, who, op);
  if (op.bad) g.unsup(std::string(who) + ": a destination word above the half-bright table");
}

// Surface_drawSprite(this, dst, x, y, alpha, mask, flag) -> Sprite::drawAlpha at (m_20 + x, m_24 + y), no display scale in
// the wrapper; drawAlpha's bodies read the display scale like the others
void draw_alpha(Gfx &g, Ctx &ctx, uint32_t key, uint32_t dst, int32_t x, int32_t y, uint32_t alpha_bits, uint32_t pal_obj,
                uint32_t flag, const char *who) {
  if (!gfx_is_local_key(key) && ctx.mem.load<uint32_t>(key + 8) != 0) return;
  const Sprite8 *s = source(g, ctx, key, dst, who);
  if (!s) return;
  float alpha;
  std::memcpy(&alpha, &alpha_bits, 4);
  const int body = alpha_body(alpha);
  if (body == -1) return;
  if (body == -2) {
    g.unsup(std::string(who) + ": drawAlpha's opaque arm");
    return;
  }
  const uint16_t *pal = sprite_palette(g, ctx, key, *s, pal_obj, false, who);
  if (!pal) return;
  // the member of the window's pair: Sprite::drawAlpha sets its flag byte to 1 and clears it only when the destination's
  // format word +4 is 1, and calls the first member of each pair with the flag set (roadmap S2.13 read the arms; the quarter
  // band's first member draw16_1002ed60 is the one S2.8 measured on the reference frames). The only destination here is the
  // root surface, drawn with the flag-set members [15, 30); mode 9 takes the flag-cleared member instead.
  const int member = mode() == 9 ? 0 : 15;
  AlphaOp op{pal, &kAlphaPrograms[body + member], static_cast<uint8_t>(flag) != 0};
  // P9D: the quarter member's pixels recorded for the modal capture (gfx_reports.cpp records the same blend at scale 1)
  const bool quarter = !std::strcmp(op.prog->body, "draw16_1002ed60") && mode() != 7;
  if (quarter && g.hud.any && g.hud.modal && dst == g.root_va(ctx)) op.rec = &g;
  blit16(g, *s, g.root, add32(s->left, x), add32(s->top, y), cur_scale(g), who, op);
  // every root pixel blended once with one word (the sprite covers the root at this scale): a whole-frame dim
  if (op.rec && op.one_src && op.n == g.root.px.size()) g.note_dim(op.src0);
}

// ------------------------------------------------------------------------------------------------ composeGolferSprite
// The palette layers of composeGolferSprite (c3ei.cpp FormatGolferTitle_re; the look-index choice is golfers_pro.inc's
// compose_golfer_sprite before its write-back): every layer copies part palette entries [0, n) into the work buffer
// (Palette::getRGBRange; skipped for a NULL part, which leaves the buffer as it is) and work entries [c1, c1 + c2) into the
// compositor, g_images[0]'s palette (Palette::setRGB). A layer whose n is below c1 + c2 copies work entries no layer of this
// call filled: those keep the previous call's values (the original's stack buffer, U-1342).
void compose_golfer(Gfx &g, Ctx &ctx, int32_t g0) {
  const uint32_t golfers = state_va(ctx, &GameState::g_golfers);
  const uint32_t types = state_va(ctx, &GameState::g_golferTypes);
  const uint32_t q1 = state_va(ctx, &GameState::f_tab_quotes_4d55e8), q2 = state_va(ctx, &GameState::f_tab_quotes_4d5b38);
  Memory &m = ctx.mem;
  int32_t gi = g0;
  int F1 = 0, F2 = 1;
  int32_t A = 0, B = 0, C = 0, D = 0, Ev = 0, ebx = 0;
  if (gi < 0x9c) {
    F2 = type_bit7_clear(ctx, gi);
    if (gi == 0xff) gi = 0;
    const uint32_t rec = golfers + (u(gi) << 8);
    const int32_t t = m.load<int16_t>(rec + 0xb6);
    D = t % 10;
    A = mul32(t, 3) % 10;
    const uint32_t T = types + 0x230u * u(t);
    const int32_t app = m.load<int8_t>(T + 0x22);
    if (app < 0x13)
      B = m.load<int8_t>((F2 != 0 ? q1 : q2) + 68u * u(app));
    else
      B = t % 4;
    const uint8_t fl = m.load<uint8_t>(rec + 0x19);
    ebx = A;
    if (!(fl & 4)) ebx = F2 != 0 ? 0 : 9;
    C = B;
    if (fl & 2) C = 4;
    if (F2 == 0) A = mul32(2, t) % 3 + 1;
    const int32_t off2 = type_bit7_clear(ctx, gi) != 0 ? 0 : 0x14;
    Ev = m.load<int8_t>(q1 + 3 + 68u * u(add32(m.load<int8_t>(T + 0x22), off2)));
    if (m.load<uint32_t>(T + 0x2c) != 0) {
      A = m.load<int8_t>(T + 0x24);
      D = m.load<int8_t>(T + 0x25);
      B = m.load<int8_t>(T + 0x27);
      ebx = m.load<uint8_t>(T + 0x23) & 0xf;
      C = m.load<int8_t>(T + 0x26);
      Ev = m.load<int8_t>(T + 0x28);
    }
    if (m.load<uint8_t>(rec + 0x18) == 0x20) {
      C = 4;
      F2 = 1;
      const uint32_t pro = state_va(ctx, &GameState::g_golfPros) + 56u * m.load<uint8_t>(rec + 0xbb);
      ebx = m.load<int8_t>(pro + 0x22);
      B = m.load<int8_t>(pro + 0x21);
      A = m.load<int8_t>(pro + 0x23);
      D = m.load<int8_t>(pro + 0x24);
    }
  } else {
    F1 = 1;
    C = 4;
    F2 = 1;
    const uint32_t o = state_va(ctx, &GameState::g_golferPhrases) + 0x183bdu + 37u * u(gi);
    B = m.load<int8_t>(o);
    ebx = add32(m.load<int8_t>(o + 1), 5);
    A = m.load<int8_t>(o + 2);
    D = m.load<int8_t>(o + 3);
  }
  SceneState &ss = st(g);
  std::array<uint16_t, 256> *comp = gfxh::palette_of(g, images_slot(ctx, 0), true);
  auto layer = [&](int64_t slot, int32_t n, int32_t c1, int32_t c2) {
    if (slot >= 0 && slot < 190) {
      const std::array<uint16_t, 256> *part = gfxh::palette_of(g, images_slot(ctx, static_cast<uint32_t>(slot)), false);
      if (part) {
        for (int32_t k = 0; k < n && k < 256; ++k) ss.work[static_cast<size_t>(k)] = (*part)[static_cast<size_t>(k)];
        ss.work_seeded = true;
      }
    } else if (slot != -1) {
      g.unsup("composeGolferSprite: a part index outside g_images");
    }
    for (int32_t k = 0; k < c2; ++k)
      if (c1 + k >= 0 && c1 + k < 256) (*comp)[static_cast<size_t>(c1 + k)] = ss.work[static_cast<size_t>(c1 + k)];
  };
  // partAt(idx) = g_images[idx + 1]'s palette (mode 3: g_images[idx + 2])
  auto part = [](int32_t idx) { return static_cast<int64_t>(idx) + (mode() == 3 ? 2 : 1); };
  if (F2 == 0) {
    B = add32(B, 0x8b);
    D = add32(D, 0x8b);
    Ev = add32(Ev, 0x8b);
    ebx = add32(ebx, 0x8b);
    C = add32(C, 0x8b);
    A = add32(A, 0x8b);
    layer(140, 0x78, 0, 0x78);
  } else {
    layer(1, 0x78, 0, 0x78);
  }
  layer(part(A), 0x14, 0, 0x14);
  layer(part(D), 0x28, 0x14, 0x14);
  layer(part(B), 0x3c, 0x28, 0x14);
  layer(part(ebx), 0x50, 0x3c, 0x14);
  if (F2 != 0) {
    layer(part(A), 0x79, 0x6b, 0xf);
    layer(part(D), 0x83, 0x7a, 0xa);
    layer(part(B), 0x88, 0x84, 0x32);
    if (F1 == 0) layer(part(C), 0x5a, 0x50, 7);
  } else {
    layer(part(Ev), 0x5a, 0x50, 0xa);
    layer(part(C), 0x64, 0x5a, 7);
    // g_images[140 + (g & 1)]'s palette into the work buffer, no compositor call
    const std::array<uint16_t, 256> *o2 = gfxh::palette_of(g, images_slot(ctx, 140u + (u(gi) & 1)), false);
    if (o2) {
      for (int32_t k = 0; k < 0xac; ++k) ss.work[static_cast<size_t>(k)] = (*o2)[static_cast<size_t>(k)];
      ss.work_seeded = true;
    }
  }
  int64_t last = 140;
  if (gi >= 0 && gi < 0x98) {
    const int32_t t2 = m.load<int16_t>(golfers + (u(gi) << 8) + 0xb6);
    const uint8_t b = m.load<uint8_t>(state_va(ctx, &GameState::g_golferTypeState) + 2 + 44u * u(t2)) & 7;
    if (static_cast<int8_t>(b) >= 4) last = 141;
  }
  layer(last, 0xac, 0x90, 0x1c);
}

// ------------------------------------------------------------------------------------------------ loadWorldAssets' palettes
// The palette objects loadWorldAssets fills for the course type (c3fv.cpp LoadWorldAssets_re): the building / scenery
// palettes and the water palette, the scenery tables' palettes, the tree (D1), D2, flower (D3) and tall-tree (D4) groups,
// each a loadImage(working image, path, palette object, 0, 0x100, 0): the file's palette into the object (S3.6's store,
// gfxh::palette_of; a file that does not open leaves the object as it was). The tables are generated from the C3 source.
struct PalLoad {
  uint32_t path, pal;  // GameState offsets
};
#define GSO(sym, off) (static_cast<uint32_t>(offsetof(GameState, sym)) + static_cast<uint32_t>(off))
// BEGIN GENERATED loadWorldAssets palette loads (log/roadmap/S3/5_tools/wa_tables.py gen)
// {path, palette object}: each a loadImage(working image, path, palette object, 0, 0x100, 0) of the table (the C3
// source's call site in the comment, as loadWorldAssets+<offset>); GSO = an offset in the GameState
const PalLoad kWaPalPark[] = {
    {GSO(f_strpool_4c95f4, 0x3918), GSO(g_images, 0x528)},  // loadWorldAssets+0x7a
    {GSO(f_strpool_4c95f4, 0x38f8), GSO(g_images, 0x8f0)},  // loadWorldAssets+0x94
    {GSO(f_strpool_4c95f4, 0x38d0), GSO(g_images, 0x580)},  // loadWorldAssets+0xae
    {GSO(f_strpool_4c95f4, 0x38ac), GSO(g_images, 0x5d8)},  // loadWorldAssets+0xc8
    {GSO(f_strpool_4c95f4, 0x388c), GSO(g_images, 0x630)},  // loadWorldAssets+0xe2
    {GSO(f_strpool_4c95f4, 0x386c), GSO(g_images, 0x898)},  // loadWorldAssets+0xfc
    {GSO(f_strpool_4c95f4, 0x384c), GSO(g_images, 0x948)},  // loadWorldAssets+0x116
    {GSO(f_strpool_4c95f4, 0x3828), GSO(g_images, 0x4d0)},  // loadWorldAssets+0x130
    {GSO(f_strpool_4c95f4, 0x3808), GSO(g_images, 0x7e8)},  // loadWorldAssets+0x14a
    {GSO(f_strpool_4c95f4, 0x37e8), GSO(g_images, 0x9a0)},  // loadWorldAssets+0x164
    {GSO(f_strpool_4c95f4, 0x37c8), GSO(g_images, 0x6e0)},  // loadWorldAssets+0x17e
    {GSO(f_strpool_4c95f4, 0x37ac), GSO(g_images, 0x9f8)},  // loadWorldAssets+0x198
    {GSO(f_strpool_4c95f4, 0x3790), GSO(g_images, 0x1c88)},  // loadWorldAssets+0x1b2
    {GSO(f_strpool_4c95f4, 0x3778), GSO(g_images, 0x1ef0)},  // loadWorldAssets+0x1cc
    {GSO(f_strpool_4c95f4, 0x3750), GSO(g_images, 0x33e8)},  // loadWorldAssets+0x1e6
    {GSO(f_strpool_4c95f4, 0x3728), GSO(g_images, 0x4048)},  // loadWorldAssets+0x200
    {GSO(f_strpool_4c95f4, 0x3704), GSO(g_images, 0x3440)},  // loadWorldAssets+0x21a
    {GSO(f_strpool_4c95f4, 0x36e4), GSO(g_images, 0x34f0)},  // loadWorldAssets+0x234
    {GSO(f_strpool_4c95f4, 0x36c4), GSO(g_images, 0x3548)},  // loadWorldAssets+0x24e
    {GSO(f_strpool_4c95f4, 0x36a4), GSO(g_images, 0x3498)},  // loadWorldAssets+0x268
    {GSO(f_strpool_4c95f4, 0x3684), GSO(g_images, 0x35a0)},  // loadWorldAssets+0x282
    {GSO(f_strpool_4c95f4, 0x3660), GSO(g_images, 0x35f8)},  // loadWorldAssets+0x29c
    {GSO(f_strpool_4c95f4, 0x3640), GSO(g_images, 0x3650)},  // loadWorldAssets+0x2b6
    {GSO(f_strpool_4c95f4, 0x3620), GSO(g_images, 0x36a8)},  // loadWorldAssets+0x2d0
    {GSO(f_strpool_4c95f4, 0x3604), GSO(g_images, 0x1a78)},  // loadWorldAssets+0x2ea
    {GSO(f_strpool_4c95f4, 0x35ec), GSO(g_images, 0x2208)},  // loadWorldAssets+0x304
    {GSO(f_strpool_4c95f4, 0x35b8), GSO(g_images, 0x2100)},  // loadWorldAssets+0x31e
    {GSO(f_strpool_4c95f4, 0x3598), GSO(g_images, 0x688)},  // loadWorldAssets+0x338
    {GSO(f_strpool_4c95f4, 0x357c), GSO(g_images, 0x1ff8)},  // loadWorldAssets+0x352
};
const PalLoad kWaPalLinks[] = {
    {GSO(f_strpool_4c95f4, 0x3534), GSO(g_images, 0x528)},  // loadWorldAssets+0x376
    {GSO(f_strpool_4c95f4, 0x3508), GSO(g_images, 0x8f0)},  // loadWorldAssets+0x390
    {GSO(f_strpool_4c95f4, 0x34ec), GSO(g_images, 0x580)},  // loadWorldAssets+0x3aa
    {GSO(f_strpool_4c95f4, 0x34d0), GSO(g_images, 0x5d8)},  // loadWorldAssets+0x3c4
    {GSO(f_strpool_4c95f4, 0x34b4), GSO(g_images, 0x630)},  // loadWorldAssets+0x3de
    {GSO(f_strpool_4c95f4, 0x3490), GSO(g_images, 0x898)},  // loadWorldAssets+0x3f8
    {GSO(f_strpool_4c95f4, 0x3474), GSO(g_images, 0x948)},  // loadWorldAssets+0x412
    {GSO(f_strpool_4c95f4, 0x3454), GSO(g_images, 0x4d0)},  // loadWorldAssets+0x42c
    {GSO(f_strpool_4c95f4, 0x3430), GSO(g_images, 0x7e8)},  // loadWorldAssets+0x446
    {GSO(f_strpool_4c95f4, 0x340c), GSO(g_images, 0x9a0)},  // loadWorldAssets+0x460
    {GSO(f_strpool_4c95f4, 0x33e4), GSO(g_images, 0x6e0)},  // loadWorldAssets+0x47a
    {GSO(f_strpool_4c95f4, 0x33c0), GSO(g_images, 0x9f8)},  // loadWorldAssets+0x494
    {GSO(f_strpool_4c95f4, 0x339c), GSO(g_images, 0x1c88)},  // loadWorldAssets+0x4ae
    {GSO(f_strpool_4c95f4, 0x3378), GSO(g_images, 0x1ef0)},  // loadWorldAssets+0x4c8
    {GSO(f_strpool_4c95f4, 0x3358), GSO(g_images, 0x33e8)},  // loadWorldAssets+0x4e2
    {GSO(f_strpool_4c95f4, 0x3728), GSO(g_images, 0x4048)},  // loadWorldAssets+0x4fc
    {GSO(f_strpool_4c95f4, 0x3338), GSO(g_images, 0x34f0)},  // loadWorldAssets+0x516
    {GSO(f_strpool_4c95f4, 0x3314), GSO(g_images, 0x3440)},  // loadWorldAssets+0x530
    {GSO(f_strpool_4c95f4, 0x32ec), GSO(g_images, 0x3548)},  // loadWorldAssets+0x54a
    {GSO(f_strpool_4c95f4, 0x32c8), GSO(g_images, 0x3498)},  // loadWorldAssets+0x564
    {GSO(f_strpool_4c95f4, 0x32a4), GSO(g_images, 0x35a0)},  // loadWorldAssets+0x57e
    {GSO(f_strpool_4c95f4, 0x3280), GSO(g_images, 0x35f8)},  // loadWorldAssets+0x598
    {GSO(f_strpool_4c95f4, 0x325c), GSO(g_images, 0x3650)},  // loadWorldAssets+0x5b2
    {GSO(f_strpool_4c95f4, 0x3238), GSO(g_images, 0x36a8)},  // loadWorldAssets+0x5cc
    {GSO(f_strpool_4c95f4, 0x3604), GSO(g_images, 0x1a78)},  // loadWorldAssets+0x5e6
    {GSO(f_strpool_4c95f4, 0x321c), GSO(g_images, 0x2208)},  // loadWorldAssets+0x600
    {GSO(f_strpool_4c95f4, 0x31ec), GSO(g_images, 0x2100)},  // loadWorldAssets+0x61a
    {GSO(f_strpool_4c95f4, 0x31c8), GSO(g_images, 0x688)},  // loadWorldAssets+0x634
    {GSO(f_strpool_4c95f4, 0x31a0), GSO(g_images, 0x1ff8)},  // loadWorldAssets+0x64e
};
const PalLoad kWaPalTrop[] = {
    {GSO(f_strpool_4c95f4, 0x3154), GSO(g_images, 0x528)},  // loadWorldAssets+0x67e
    {GSO(f_strpool_4c95f4, 0x3130), GSO(g_images, 0x8f0)},  // loadWorldAssets+0x698
    {GSO(f_strpool_4c95f4, 0x3104), GSO(g_images, 0x580)},  // loadWorldAssets+0x6b2
    {GSO(f_strpool_4c95f4, 0x30e0), GSO(g_images, 0x7e8)},  // loadWorldAssets+0x6cc
    {GSO(f_strpool_4c95f4, 0x30bc), GSO(g_images, 0x5d8)},  // loadWorldAssets+0x6e6
    {GSO(f_strpool_4c95f4, 0x3090), GSO(g_images, 0x630)},  // loadWorldAssets+0x700
    {GSO(f_strpool_4c95f4, 0x3068), GSO(g_images, 0x948)},  // loadWorldAssets+0x71a
    {GSO(f_strpool_4c95f4, 0x303c), GSO(g_images, 0x4d0)},  // loadWorldAssets+0x734
    {GSO(f_strpool_4c95f4, 0x3018), GSO(g_images, 0x898)},  // loadWorldAssets+0x74e
    {GSO(f_strpool_4c95f4, 0x2ff4), GSO(g_images, 0x9a0)},  // loadWorldAssets+0x768
    {GSO(f_strpool_4c95f4, 0x2fd0), GSO(g_images, 0x6e0)},  // loadWorldAssets+0x782
    {GSO(f_strpool_4c95f4, 0x2fb0), GSO(g_images, 0x9f8)},  // loadWorldAssets+0x79c
    {GSO(f_strpool_4c95f4, 0x2f88), GSO(g_images, 0x1c88)},  // loadWorldAssets+0x7b6
    {GSO(f_strpool_4c95f4, 0x2f60), GSO(g_images, 0x1ef0)},  // loadWorldAssets+0x7d0
    {GSO(f_strpool_4c95f4, 0x2f38), GSO(g_images, 0x33e8)},  // loadWorldAssets+0x7ea
    {GSO(f_strpool_4c95f4, 0x2f0c), GSO(g_images, 0x4048)},  // loadWorldAssets+0x804
    {GSO(f_strpool_4c95f4, 0x2ee0), GSO(g_images, 0x3440)},  // loadWorldAssets+0x81e
    {GSO(f_strpool_4c95f4, 0x2eb8), GSO(g_images, 0x3498)},  // loadWorldAssets+0x838
    {GSO(f_strpool_4c95f4, 0x2e90), GSO(g_images, 0x34f0)},  // loadWorldAssets+0x852
    {GSO(f_strpool_4c95f4, 0x2e68), GSO(g_images, 0x3548)},  // loadWorldAssets+0x86c
    {GSO(f_strpool_4c95f4, 0x2e40), GSO(g_images, 0x35a0)},  // loadWorldAssets+0x886
    {GSO(f_strpool_4c95f4, 0x2e14), GSO(g_images, 0x35f8)},  // loadWorldAssets+0x8a0
    {GSO(f_strpool_4c95f4, 0x2de8), GSO(g_images, 0x3650)},  // loadWorldAssets+0x8ba
    {GSO(f_strpool_4c95f4, 0x2dc0), GSO(g_images, 0x36a8)},  // loadWorldAssets+0x8d4
    {GSO(f_strpool_4c95f4, 0x2da4), GSO(g_images, 0x1a78)},  // loadWorldAssets+0x8ee
    {GSO(f_strpool_4c95f4, 0x2d8c), GSO(g_images, 0x2208)},  // loadWorldAssets+0x908
    {GSO(f_strpool_4c95f4, 0x2d60), GSO(g_images, 0x688)},  // loadWorldAssets+0x922
    {GSO(f_strpool_4c95f4, 0x2d2c), GSO(g_images, 0x2100)},  // loadWorldAssets+0x93c
};
const PalLoad kWaPalDesert[] = {
    {GSO(f_strpool_4c95f4, 0x2cec), GSO(g_images, 0x528)},  // loadWorldAssets+0x96c
    {GSO(f_strpool_4c95f4, 0x2ccc), GSO(g_images, 0x8f0)},  // loadWorldAssets+0x986
    {GSO(f_strpool_4c95f4, 0x2cac), GSO(g_images, 0x580)},  // loadWorldAssets+0x9a0
    {GSO(f_strpool_4c95f4, 0x2c8c), GSO(g_images, 0x7e8)},  // loadWorldAssets+0x9ba
    {GSO(f_strpool_4c95f4, 0x2c6c), GSO(g_images, 0x5d8)},  // loadWorldAssets+0x9d4
    {GSO(f_strpool_4c95f4, 0x2c4c), GSO(g_images, 0x630)},  // loadWorldAssets+0x9ee
    {GSO(f_strpool_4c95f4, 0x2c30), GSO(g_images, 0x948)},  // loadWorldAssets+0xa08
    {GSO(f_strpool_4c95f4, 0x2c14), GSO(g_images, 0x4d0)},  // loadWorldAssets+0xa22
    {GSO(f_strpool_4c95f4, 0x2bf4), GSO(g_images, 0x898)},  // loadWorldAssets+0xa3c
    {GSO(f_strpool_4c95f4, 0x2bd8), GSO(g_images, 0x9a0)},  // loadWorldAssets+0xa56
    {GSO(f_strpool_4c95f4, 0x2bbc), GSO(g_images, 0x6e0)},  // loadWorldAssets+0xa70
    {GSO(f_strpool_4c95f4, 0x2b98), GSO(g_images, 0x9f8)},  // loadWorldAssets+0xa8a
    {GSO(f_strpool_4c95f4, 0x2b74), GSO(g_images, 0x1c88)},  // loadWorldAssets+0xaa4
    {GSO(f_strpool_4c95f4, 0x2b50), GSO(g_images, 0x1ef0)},  // loadWorldAssets+0xabe
    {GSO(f_strpool_4c95f4, 0x2b2c), GSO(g_images, 0x33e8)},  // loadWorldAssets+0xad8
    {GSO(f_strpool_4c95f4, 0x2b00), GSO(g_images, 0x4048)},  // loadWorldAssets+0xaf2
    {GSO(f_strpool_4c95f4, 0x2ae0), GSO(g_images, 0x3440)},  // loadWorldAssets+0xb0c
    {GSO(f_strpool_4c95f4, 0x2ab8), GSO(g_images, 0x34f0)},  // loadWorldAssets+0xb26
    {GSO(f_strpool_4c95f4, 0x2a98), GSO(g_images, 0x3548)},  // loadWorldAssets+0xb40
    {GSO(f_strpool_4c95f4, 0x2a7c), GSO(g_images, 0x3498)},  // loadWorldAssets+0xb5a
    {GSO(f_strpool_4c95f4, 0x2a60), GSO(g_images, 0x35a0)},  // loadWorldAssets+0xb74
    {GSO(f_strpool_4c95f4, 0x2a40), GSO(g_images, 0x35f8)},  // loadWorldAssets+0xb8e
    {GSO(f_strpool_4c95f4, 0x2a20), GSO(g_images, 0x3650)},  // loadWorldAssets+0xba8
    {GSO(f_strpool_4c95f4, 0x2a00), GSO(g_images, 0x36a8)},  // loadWorldAssets+0xbc2
    {GSO(f_strpool_4c95f4, 0x29e4), GSO(g_images, 0x1a78)},  // loadWorldAssets+0xbdc
    {GSO(f_strpool_4c95f4, 0x29c8), GSO(g_images, 0x2208)},  // loadWorldAssets+0xbf6
    {GSO(f_strpool_4c95f4, 0x2998), GSO(g_images, 0x2100)},  // loadWorldAssets+0xc10
    {GSO(f_strpool_4c95f4, 0x2978), GSO(g_images, 0x688)},  // loadWorldAssets+0xc2a
    {GSO(f_strpool_4c95f4, 0x2958), GSO(g_images, 0x1ff8)},  // loadWorldAssets+0xc44
};
const PalLoad kWaJoinWater[] = {
    {GSO(f_strpool_4c95f4, 0x355c), GSO(g_images, 0x3d30)},  // loadWorldAssets+0xc5e
};
const PalLoad kWaSwTrop[] = {
    {GSO(f_strpool_4c95f4, 0xf94), GSO(g_images, 0x1ad0)},  // loadWorldAssets+0x253b
    {GSO(f_strpool_4c95f4, 0xed0), GSO(g_images, 0x1b28)},  // loadWorldAssets+0x25ed
    {GSO(f_strpool_4c95f4, 0xe98), GSO(g_images, 0x14a0)},  // loadWorldAssets+0x261f
};
const PalLoad kWaSwPark[] = {
    {GSO(f_strpool_4c95f4, 0xd58), GSO(g_images, 0x1ad0)},  // loadWorldAssets+0x2771
    {GSO(f_strpool_4c95f4, 0xed0), GSO(g_images, 0x1b28)},  // loadWorldAssets+0x2823
    {GSO(f_strpool_4c95f4, 0xcb8), GSO(g_images, 0x14a0)},  // loadWorldAssets+0x2855
};
const PalLoad kWaSwDesert[] = {
    {GSO(f_strpool_4c95f4, 0xb64), GSO(g_images, 0x1ad0)},  // loadWorldAssets+0x29a7
    {GSO(f_strpool_4c95f4, 0xed0), GSO(g_images, 0x1b28)},  // loadWorldAssets+0x2a59
    {GSO(f_strpool_4c95f4, 0xaa4), GSO(g_images, 0x14a0)},  // loadWorldAssets+0x2a8b
};
const PalLoad kWaSwLinks[] = {
    {GSO(f_strpool_4c95f4, 0x938), GSO(g_images, 0x1ad0)},  // loadWorldAssets+0x2bdd
    {GSO(f_strpool_4c95f4, 0xed0), GSO(g_images, 0x1b28)},  // loadWorldAssets+0x2c8f
    {GSO(f_strpool_4c95f4, 0x870), GSO(g_images, 0x14a0)},  // loadWorldAssets+0x2cc1
};
const PalLoad kWaD1Desert[] = {
    {GSO(f_strpool_4c95f4, 0x728), GSO(g_images, 0xcb8)},  // loadWorldAssets+0x2e24
    {GSO(f_strpool_4c95f4, 0x728), GSO(g_images, 0xd10)},  // loadWorldAssets+0x2e3e
    {GSO(f_strpool_4c95f4, 0x728), GSO(g_images, 0xd68)},  // loadWorldAssets+0x2e58
};
const PalLoad kWaD1Trop[] = {
    {GSO(f_strpool_4c95f4, 0x708), GSO(g_images, 0xcb8)},  // loadWorldAssets+0x2e8c
    {GSO(f_strpool_4c95f4, 0x6e4), GSO(g_images, 0xd10)},  // loadWorldAssets+0x2ea6
    {GSO(f_strpool_4c95f4, 0x6c0), GSO(g_images, 0xd68)},  // loadWorldAssets+0x2ec0
};
const PalLoad kWaD1Other[] = {
    {GSO(f_strpool_4c95f4, 0x6a4), GSO(g_images, 0xcb8)},  // loadWorldAssets+0x2ee1
    {GSO(f_strpool_4c95f4, 0x68c), GSO(g_images, 0xd10)},  // loadWorldAssets+0x2efb
    {GSO(f_strpool_4c95f4, 0x670), GSO(g_images, 0xd68)},  // loadWorldAssets+0x2f15
};
const PalLoad kWaD2Trop[] = {
    {GSO(f_strpool_4c95f4, 0x62c), GSO(g_images, 0xe18)},  // loadWorldAssets+0x2f52
    {GSO(f_strpool_4c95f4, 0x604), GSO(g_images, 0xe70)},  // loadWorldAssets+0x2f6c
};
const PalLoad kWaD2Park[] = {
    {GSO(f_strpool_4c95f4, 0x5ec), GSO(g_images, 0xe18)},  // loadWorldAssets+0x2f8f
    {GSO(f_strpool_4c95f4, 0x5cc), GSO(g_images, 0xe70)},  // loadWorldAssets+0x2fa9
    {GSO(f_strpool_4c95f4, 0x5b0), GSO(g_images, 0xec8)},  // loadWorldAssets+0x2fc3
    {GSO(f_strpool_4c95f4, 0x594), GSO(g_images, 0xf20)},  // loadWorldAssets+0x2fdd
};
const PalLoad kWaD3Desert[] = {
    {GSO(f_strpool_4c95f4, 0x574), GSO(g_images, 0xe18)},  // loadWorldAssets+0x3004
    {GSO(f_strpool_4c95f4, 0x554), GSO(g_images, 0xe70)},  // loadWorldAssets+0x301e
    {GSO(f_strpool_4c95f4, 0x530), GSO(g_images, 0xf78)},  // loadWorldAssets+0x3038
    {GSO(f_strpool_4c95f4, 0x50c), GSO(g_images, 0xfd0)},  // loadWorldAssets+0x3052
    {GSO(f_strpool_4c95f4, 0x4e8), GSO(g_images, 0x1028)},  // loadWorldAssets+0x306c
    {GSO(f_strpool_4c95f4, 0x4c4), GSO(g_images, 0x1080)},  // loadWorldAssets+0x3086
};
const PalLoad kWaD3Trop[] = {
    {GSO(f_strpool_4c95f4, 0x478), GSO(g_images, 0xf78)},  // loadWorldAssets+0x30ba
    {GSO(f_strpool_4c95f4, 0x450), GSO(g_images, 0xfd0)},  // loadWorldAssets+0x30d4
    {GSO(f_strpool_4c95f4, 0x428), GSO(g_images, 0x1028)},  // loadWorldAssets+0x30ee
    {GSO(f_strpool_4c95f4, 0x400), GSO(g_images, 0x1080)},  // loadWorldAssets+0x3108
};
const PalLoad kWaD3Other[] = {
    {GSO(f_strpool_4c95f4, 0x3b8), GSO(g_images, 0xf78)},  // loadWorldAssets+0x3129
    {GSO(f_strpool_4c95f4, 0x398), GSO(g_images, 0xfd0)},  // loadWorldAssets+0x3143
    {GSO(f_strpool_4c95f4, 0x374), GSO(g_images, 0x1028)},  // loadWorldAssets+0x315d
    {GSO(f_strpool_4c95f4, 0x350), GSO(g_images, 0x1080)},  // loadWorldAssets+0x3177
};
const PalLoad kWaD4Links[] = {
    {GSO(f_strpool_4c95f4, 0x310), GSO(g_images, 0xe18)},  // loadWorldAssets+0x31b4
    {GSO(f_strpool_4c95f4, 0x2f0), GSO(g_images, 0xe70)},  // loadWorldAssets+0x31ce
    {GSO(f_strpool_4c95f4, 0x2d0), GSO(g_images, 0xec8)},  // loadWorldAssets+0x31e8
    {GSO(f_strpool_4c95f4, 0x2b0), GSO(g_images, 0xf20)},  // loadWorldAssets+0x3202
};
const PalLoad kWaD4Desert[] = {
    {GSO(f_strpool_4c95f4, 0x288), GSO(g_images, 0x1130)},  // loadWorldAssets+0x322d
    {GSO(f_strpool_4c95f4, 0x288), GSO(g_images, 0x1188)},  // loadWorldAssets+0x3247
    {GSO(f_strpool_4c95f4, 0x288), GSO(g_images, 0x11e0)},  // loadWorldAssets+0x3261
};
const PalLoad kWaD4Trop[] = {
    {GSO(f_strpool_4c95f4, 0x258), GSO(g_images, 0x1130)},  // loadWorldAssets+0x3295
    {GSO(f_strpool_4c95f4, 0x258), GSO(g_images, 0x1188)},  // loadWorldAssets+0x32af
    {GSO(f_strpool_4c95f4, 0x258), GSO(g_images, 0x11e0)},  // loadWorldAssets+0x32c9
};
const PalLoad kWaD4Other[] = {
    {GSO(f_strpool_4c95f4, 0x23c), GSO(g_images, 0x1130)},  // loadWorldAssets+0x32ed
    {GSO(f_strpool_4c95f4, 0x220), GSO(g_images, 0x1188)},  // loadWorldAssets+0x3307
    {GSO(f_strpool_4c95f4, 0x208), GSO(g_images, 0x11e0)},  // loadWorldAssets+0x3321
};
const PalLoad kWaD4Links2[] = {
    {GSO(f_strpool_4c95f4, 0x1cc), GSO(g_images, 0x1130)},  // loadWorldAssets+0x334e
    {GSO(f_strpool_4c95f4, 0x1ac), GSO(g_images, 0x1188)},  // loadWorldAssets+0x3368
    {GSO(f_strpool_4c95f4, 0x18c), GSO(g_images, 0x11e0)},  // loadWorldAssets+0x3382
};
const PalLoad kPalWaterDesert = {GSO(f_strpool_4c95f4, 0x2938), GSO(g_images, 0x3d30)};
const PalLoad kPalWaterTrop = {GSO(f_strpool_4c95f4, 0x2d0c), GSO(g_images, 0x3d30)};
const PalLoad kPalWaterLinks = {GSO(f_strpool_4c95f4, 0x3180), GSO(g_images, 0x3d30)};
const PalLoad kPalWaterPark = {GSO(f_strpool_4c95f4, 0x355c), GSO(g_images, 0x3d30)};
const PalLoad kPalTreeDesert = {GSO(f_strpool_4c95f4, 0x728), GSO(g_images, 0xdc0)};
const PalLoad kPalTreeTrop = {GSO(f_strpool_4c95f4, 0x708), GSO(g_images, 0xdc0)};
const PalLoad kPalTreeOther = {GSO(f_strpool_4c95f4, 0x654), GSO(g_images, 0xdc0)};
const PalLoad kPalFlowerDesert = {GSO(f_strpool_4c95f4, 0x4a0), GSO(g_images, 0x10d8)};
const PalLoad kPalFlowerTrop = {GSO(f_strpool_4c95f4, 0x3d8), GSO(g_images, 0x10d8)};
const PalLoad kPalFlowerOther = {GSO(f_strpool_4c95f4, 0x330), GSO(g_images, 0x10d8)};
const PalLoad kPalTallLinks2 = {GSO(f_strpool_4c95f4, 0x16c), GSO(g_images, 0x1238)};
const PalLoad kPalTallDesert = {GSO(f_strpool_4c95f4, 0x288), GSO(g_images, 0x1238)};
const PalLoad kPalTallTrop = {GSO(f_strpool_4c95f4, 0x258), GSO(g_images, 0x1238)};
const PalLoad kPalTallOther = {GSO(f_strpool_4c95f4, 0x1ec), GSO(g_images, 0x1238)};
// END GENERATED loadWorldAssets palette loads
#undef GSO

void pal_load(Gfx &g, Ctx &ctx, const PalLoad &l) {
  const uint32_t base = ctx.mem.data_base();
  Image8 img;
  if (gfxb::load_image(g, ctx, base + l.path, base + l.pal, 0, 0x100, 0, img) != 0) return;
  *gfxh::palette_of(g, base + l.pal, true) = img.pal;
}
template <size_t N> void pal_run(Gfx &g, Ctx &ctx, const PalLoad (&t)[N]) {
  for (const PalLoad &l : t) pal_load(g, ctx, l);
}

// loadWorldAssets.palettes (menu.cpp hands it to the backend at the start of loadWorldAssets): the palette loads in the
// body's order for course type t = the signed byte g_courseType
void b_world_palettes(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  const int8_t t = static_cast<int8_t>(ctx.gs().g_courseType);
  switch (t) {
    case 1: pal_run(g, ctx, kWaPalDesert); pal_load(g, ctx, kPalWaterDesert); break;
    case 2: pal_run(g, ctx, kWaPalTrop); pal_load(g, ctx, kPalWaterTrop); break;
    case 3: pal_run(g, ctx, kWaPalLinks); pal_load(g, ctx, kPalWaterLinks); break;
    default: pal_run(g, ctx, kWaPalPark); pal_load(g, ctx, kPalWaterPark); break;
  }
  switch (t) {   // the scenery tables (their loadFlic calls are menu.cpp's)
    case 0: pal_run(g, ctx, kWaSwPark); break;
    case 1: pal_run(g, ctx, kWaSwDesert); break;
    case 2: pal_run(g, ctx, kWaSwTrop); break;
    case 3: pal_run(g, ctx, kWaSwLinks); break;
    default: break;
  }
  if (t == 1) { pal_run(g, ctx, kWaD1Desert); pal_load(g, ctx, kPalTreeDesert); }
  else if (t == 2) { pal_run(g, ctx, kWaD1Trop); pal_load(g, ctx, kPalTreeTrop); }
  else { pal_run(g, ctx, kWaD1Other); pal_load(g, ctx, kPalTreeOther); }
  if (t == 2) pal_run(g, ctx, kWaD2Trop);
  if (t == 0) pal_run(g, ctx, kWaD2Park);
  if (t == 1) { pal_run(g, ctx, kWaD3Desert); pal_load(g, ctx, kPalFlowerDesert); }
  else if (t == 2) { pal_run(g, ctx, kWaD3Trop); pal_load(g, ctx, kPalFlowerTrop); }
  else { pal_run(g, ctx, kWaD3Other); pal_load(g, ctx, kPalFlowerOther); }
  if (t == 3) { pal_run(g, ctx, kWaD4Links); pal_run(g, ctx, kWaD4Links2); pal_load(g, ctx, kPalTallLinks2); }
  else if (t == 1) { pal_run(g, ctx, kWaD4Desert); pal_load(g, ctx, kPalTallDesert); }
  else if (t == 2) { pal_run(g, ctx, kWaD4Trop); pal_load(g, ctx, kPalTallTrop); }
  else { pal_run(g, ctx, kWaD4Other); pal_load(g, ctx, kPalTallOther); }
}

// ------------------------------------------------------------------------------------------------ the bodies
// drawQueuedSprite's drawing of one entry (position already computed by enqueueSprite / addSprite)
void draw_entry_impl(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t p7, int32_t a8, uint32_t obj) {
  GameState &gs = ctx.gs();
  g.ensure_root(ctx);
  int32_t d = 0x10;
  const int32_t w = gs.g_screenWidth;
  if (w > 1000) d = 0xd;
  if (w >= 0x4b0) d = 0xa;
  if (a8 & 0x100) d = 0x10;
  const char *who = "drawQueuedSprite";
  if (mode() == 13 && (a8 & 0x100) && (a8 & 0x1ff) >= 0x19c) return;
  dlog("q obj=" + std::to_string(obj) + " a8=" + std::to_string(a8) + " p7=" + std::to_string(p7) + " x=" + std::to_string(x) +
       " y=" + std::to_string(y) + " pool=" + std::to_string(in_cell_pool(ctx, obj)) + " spr=" + std::to_string(g.sprites.count(obj)) +
       (g.sprites.count(obj) ? " w=" + std::to_string(g.sprites.at(obj).w) + " h=" + std::to_string(g.sprites.at(obj).h) + " l=" + std::to_string(g.sprites.at(obj).left) + " t=" + std::to_string(g.sprites.at(obj).top) : std::string()));
  if (p7 == 0) {
    surface_blit2(g, ctx, obj, x, y, who);
    return;
  }
  const int32_t zoom = gs.g_zoomScale;
  int32_t s;
  if (p7 > 0) {
    s = mul32(p7, zoom);
  } else {
    s = neg32(mul32(p7, zoom));
    d = add32(d, d);
  }
  set_scale(g, ctx, s, s, d);
  if (a8 == -1) {
    surface_blit2(g, ctx, obj, x, y, who);
  } else if (a8 & 0x100) {
    compose_golfer(g, ctx, a8 & 0xff);
    sprite_draw(g, ctx, obj, x, y, images_slot(ctx, 0), who);
  } else if (a8 & 0xc00) {
    // Sprite_drawScaled at (s, s, d) with the colour 0x7c00 (flag 0x800) or 0x7fff (0x400) and g_images[flags & 0xff]
    sprite_draw_scaled(g, ctx, obj, x, y, (a8 & 0x800) ? 0x7c00u : 0x7fffu, Scale{s, s, d}, images_slot(ctx, u(a8) & 0xff), who);
  } else if (a8 & 0x200) {
    const uint32_t slot = u(a8) & 0x1ff;
    if (slot >= 190)
      g.unsup("drawQueuedSprite: a palette slot outside g_images");
    else
      call_body(g, ctx, "Surface_drawSprite", {obj, g.root_va(ctx), u(x), u(y), 0x3f19999au, images_slot(ctx, slot), 1});
  } else {
    if (u(a8) >= 190)
      g.unsup("drawQueuedSprite: a palette slot outside g_images");
    else
      sprite_draw(g, ctx, obj, x, y, images_slot(ctx, u(a8)), who);
  }
  const int32_t z = gs.g_spriteFlag8371f8 != 0 ? zoom : gs.g_sprite58b384;
  set_scale(g, ctx, z, z, 4);
}

// development (SG_GFX_SCENE_LOG set): the bounding box and count of the root pixels one entry changed
void draw_entry(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t p7, int32_t a8, uint32_t obj) {
  static const bool logging = std::getenv("SG_GFX_SCENE_LOG") != nullptr;
  if (!logging) {
    draw_entry_impl(g, ctx, x, y, p7, a8, obj);
    return;
  }
  g.ensure_root(ctx);
  const std::vector<uint16_t> before = g.root.px;
  draw_entry_impl(g, ctx, x, y, p7, a8, obj);
  int x0 = 1 << 30, x1 = -1, y0 = 1 << 30, y1 = -1, n = 0;
  for (int yy = 0; yy < g.root.h; ++yy)
    for (int xx = 0; xx < g.root.w; ++xx) {
      const size_t i = static_cast<size_t>(yy) * static_cast<size_t>(g.root.w) + static_cast<size_t>(xx);
      if (g.root.px[i] != before[i]) {
        ++n;
        x0 = std::min(x0, xx); x1 = std::max(x1, xx); y0 = std::min(y0, yy); y1 = std::max(y1, yy);
      }
    }
  dlog("  bbox n=" + std::to_string(n) + " x=" + std::to_string(x0) + ".." + std::to_string(x1) + " y=" + std::to_string(y0) + ".." + std::to_string(y1));
}

// ---- G32: the wide scene (terrain_live.h LiveTerrainWorld::scene). The queue's draws run a second time with the wide surface in
// the root surface's place and every x moved by scene_e; the 800 x 600 root is put back after each one, so the frame is unchanged.
LiveTerrainWorld *wide_world(Ctx &ctx) {
  LiveTerrain *lt = has_terrain_api(ctx) ? dynamic_cast<LiveTerrain *>(&terrain_api(ctx)) : nullptr;
  LiveTerrainWorld *w = lt ? lt->world() : nullptr;
  return w && w->valid && w->scene_w > 0 ? w : nullptr;
}
template <class F>
void on_wide(Gfx &g, LiveTerrainWorld &w, F f) {
  Surface16 wide;
  wide.w = w.scene_w;
  wide.h = 600;
  wide.px.swap(w.scene);
  std::swap(g.root, wide);
  const int rt = g.root_terrain;
  g.text.wide = true;  // P9C: a text drawn here is the scene's (gfx.cpp text_run_end)
  g.text.wide_ox = -w.scene_e;
  f();
  g.text.wide = false;
  g.root_terrain = rt;
  std::swap(g.root, wide);
  wide.px.swap(w.scene);
}
// renderSpriteQueue's start: the wide surface's base
void b_queue_begin(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  g.hud.cursor2d.clear();  // P9E fix 4: the cursor passes come after the queue
  LiveTerrain *lt = has_terrain_api(ctx) ? dynamic_cast<LiveTerrain *>(&terrain_api(ctx)) : nullptr;
  LiveTerrainWorld *w = lt ? lt->world() : nullptr;
  if (!w || !w->valid || g.root.w != 800 || g.root.h != 600) {
    if (w) w->scene_w = 0;
    return;
  }
  const int32_t E = w->ox > 0 ? static_cast<int32_t>((static_cast<int64_t>(w->ox) * 800 + w->fw - 1) / w->fw) : 0;
  const int32_t W = 800 + 2 * E;
  w->scene_e = E;
  w->scene_w = W;
  w->scene_base.assign(static_cast<size_t>(W) * 600, 0);
  const std::vector<uint16_t> &wp = w->raster->pixels();
  for (int32_t y = 0; y < 600; ++y) {
    uint16_t *row = w->scene_base.data() + static_cast<size_t>(y) * static_cast<size_t>(W);
    int32_t wy = static_cast<int32_t>(static_cast<int64_t>(y) * w->ph / 600);
    if (wy >= w->ph) wy = w->ph - 1;
    for (int32_t x = 0; x < W; ++x) {
      const int32_t fx = x - E;
      if (fx >= 0 && fx < 800) {
        row[x] = g.root.px[static_cast<size_t>(y) * 800 + static_cast<size_t>(fx)];
        continue;
      }
      int64_t wx = w->ox + static_cast<int64_t>(fx) * w->fw / 800;
      if (wx < 0) wx = 0;
      if (wx >= w->pw) wx = w->pw - 1;
      row[x] = wp[static_cast<size_t>(wy) * static_cast<size_t>(w->pw) + static_cast<size_t>(wx)];
    }
  }
  w->scene = w->scene_base;
  w->s0 = g.root.px;
  w->scene_pre.clear();  // P9E fix 4: the cursor passes come after the queue
  w->scene_cur.clear();
  w->scene_text.clear();
  gfxb::text_wide_begin(g);  // P9C
}

void b_queue_draw(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t i = static_cast<int32_t>(a[0]);
  if (i < 0 || i >= 1024) {
    g.unsup("drawQueuedSprite: an entry outside the queue arrays");
    return;
  }
  const GameState &gs = ctx.gs();
  const size_t k = static_cast<size_t>(i);
  draw_entry(g, ctx, gs.g_spriteX[k], gs.g_spriteY[k], gs.g_spriteShade[k], static_cast<int32_t>(gs.g_spriteFlags[k]),
             gs.g_spriteObj[k]);
  if (LiveTerrainWorld *w = wide_world(ctx))   // G32: the same entry on the wide scene
    on_wide(g, *w, [&] {
      draw_entry(g, ctx, add32(gs.g_spriteX[k], w->scene_e), gs.g_spriteY[k], gs.g_spriteShade[k],
                 static_cast<int32_t>(gs.g_spriteFlags[k]), gs.g_spriteObj[k]);
    });
}

// drawQueuedSprite's golfer label: after the simulation part built the name in g_textBuffer
void queue_label(Gfx &g, Ctx &ctx, const uint32_t *a, int32_t xoff);
void b_queue_label(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  queue_label(g, ctx, a, 0);
  if (LiveTerrainWorld *w = wide_world(ctx)) on_wide(g, *w, [&] { queue_label(g, ctx, a, w->scene_e); });   // G32
}
// the label of queue entry a[0], its x moved by xoff (G32: the wide scene)
void queue_label(Gfx &g, Ctx &ctx, const uint32_t *a, int32_t xoff) {
  const int32_t i = static_cast<int32_t>(a[0]);
  if (i < 0 || i >= 1024) return;
  GameState &gs = ctx.gs();
  const size_t k = static_cast<size_t>(i);
  if (gs.g_spriteShade[k] == 0) return;
  const int32_t a8 = static_cast<int32_t>(gs.g_spriteFlags[k]);
  if (a8 == -1 || !(a8 & 0x100)) return;
  const int32_t gi = a8 & 0xff;
  if (gi == 0xff || gi >= 0x98) return;
  Memory &m = ctx.mem;
  const uint32_t rec = state_va(ctx, &GameState::g_golfers) + (u(gi) << 8);
  const int32_t zoom = gs.g_zoomScale;
  const uint32_t font = zoom > 4 ? state_va(ctx, &GameState::g_font51b360) : state_va(ctx, &GameState::g_fontRef_519fd8);
  const uint32_t root = g.root_va(ctx);
  call_body(g, ctx, "setDrawTarget", {root, font, 0, 0, 0});
  bool label = false;
  if ((gs.g_preferences & 1) && zoom >= 4) {
    bool id_ok = m.load<uint8_t>(rec + 0x84) == 0;
    const int32_t me = gs.f_w_4c2e00[3];
    if (!id_ok) id_ok = gi == me || gi == (me ^ 1);
    if (id_ok) {
      const int8_t c = m.load<int8_t>(rec + 0x1d);
      if (c >= 6 && c != 0x10) {
        const uint32_t fl = m.load<uint32_t>(rec + 0x10);
        if (!(fl & (1u << 29)) || (static_cast<uint8_t>(gs.g_date) & 6))
          if (!(fl & 0x8000)) label = true;
      }
    }
  }
  if (!label) label = (gs.g_flags & 0x20) != 0;
  if (!label) return;
  const uint32_t fl = m.load<uint32_t>(rec + 0x10);
  const uint32_t col = (fl & (1u << 29)) ? 0x80007d08u : 0x80007fffu;
  const int32_t ax = add32(gs.g_spriteAnchorX[k], xoff), ay = gs.g_spriteAnchorY[k];
  call_body(g, ctx, "drawCenteredText",
            {state_va(ctx, &GameState::g_textBuffer), u(ax), u(mode() == 6 ? ay + 1 : ay), col});
  const int16_t sv = m.load<int16_t>(rec + 0xa4);
  uint32_t c2 = 0x80007d08u;
  if (sv > 2) c2 = 0x80006300u;
  if (sv > 4) c2 = 0x80001304u;
  if (sv > 6) c2 = 0x800023e8u;
  if (sv < 0) c2 = 0x80000000u;
  if (gs.g_panelListMode == 2 && !(gs.g_flags & (1u << 21))) {
    const int32_t y9 = add32(ay, 9);
    call_body(g, ctx, "Widget_fillRect", {root, u(add32(ax, -12)), u(y9), u(add32(ax, 12)), u(y9), 0x80000000u});
    const int32_t s4 = static_cast<int32_t>(m.load<int16_t>(rec + 0xa4)) - 4;
    const int32_t xe = add32(s4, add32(ax, mul32(s4, 2)));
    call_body(g, ctx, "Widget_fillRect", {root, u(add32(ax, -12)), u(y9), u(xe), u(y9), c2});
  }
}

// Sprite_draw(this, dst, x, y, table, mask) from the frame code: the table is g_color16Lut's at every call site, the mask
// a palette object or 0
void b_sprite_draw(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (a[1] == 0) return;
  if (a[1] != g.root_va(ctx)) {
    g.unsup("Sprite_draw: a destination other than the root surface");
    return;
  }
  sprite_draw(g, ctx, a[0], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), a[5], "Sprite_draw");
}

// composeGolferSprite(g) for a screen that composes a golfer outside the sprite queue (the character editor, drawThoughtBubble:
// roadmap S2.10 issues it after the simulation part): the palette layers into g_images[0], as drawQueuedSprite.draw runs them
void b_compose(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (mode() == 11) return;
  compose_golfer(g, ctx, static_cast<int32_t>(a[0]));
}
// addSprite(a1 .. a8) drawn at once, without a queue entry (gfxs::add_sprite): the screens' figures (buy land, hire)
void b_add_sprite(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (mode() == 12) return;
  int32_t v[8];
  for (int i = 0; i < 8; ++i) v[i] = static_cast<int32_t>(a[i]);
  gfxs::add_sprite(g, ctx, v);
}

// TEST-ONLY gfx_scene.blitTest (gfx.call, no frame code uses the name): one jgld blit body over constructed objects in the
// context's address space, the result in the files overlay `$gfx/scene_blit.bin` (the destination words). Arguments:
// a[0] = op | extra << 16 (op 0 the plane blit draw16_100298b0 with the color16 planes, 1 drawG's draw16_10028630 with the
// half-bright table, 2 draw16_10024140 with the colour `extra`, 3 drawB's draw16_1001bc40, 0x10 + k drawAlpha's body k of kAlphaPrograms with the flag
// `extra`), a[1] the sprite's bytes, a[2] its width | height << 16, a[3] 256 palette words, a[4] the destination words, a[5]
// its width | height << 16, a[6] x, a[7] y, a[8] nx, a[9] ny, a[10] den. port/test/test_gfx_scene.py runs the ORIGINAL body
// in an emulator on the same objects.
void b_blit_test(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const uint32_t op = a[0] & 0xffff, extra = a[0] >> 16;
  Sprite8 s;
  s.w = static_cast<int32_t>(a[2] & 0xffff);
  s.h = static_cast<int32_t>(a[2] >> 16);
  s.px.resize(static_cast<size_t>(s.w) * static_cast<size_t>(s.h));
  for (size_t i = 0; i < s.px.size(); ++i) s.px[i] = ctx.mem.load<uint8_t>(a[1] + static_cast<uint32_t>(i));
  std::array<uint16_t, 256> pal{};
  for (uint32_t i = 0; i < 256; ++i) pal[i] = ctx.mem.load<uint16_t>(a[3] + 2 * i);
  Surface16 d;
  d.make(static_cast<int32_t>(a[5] & 0xffff), static_cast<int32_t>(a[5] >> 16));
  for (size_t i = 0; i < d.px.size(); ++i) d.px[i] = ctx.mem.load<uint16_t>(a[4] + 2 * static_cast<uint32_t>(i));
  const Scale sc{static_cast<int32_t>(a[8]), static_cast<int32_t>(a[9]), static_cast<int32_t>(a[10])};
  const int32_t x = static_cast<int32_t>(a[6]), y = static_cast<int32_t>(a[7]);
  if (op == 0) {
    PlaneOp o{g, pal.data()};
    blit16(g, s, d, x, y, sc, "gfx_scene.blitTest", o);
  } else if (op == 1) {
    const auto &lut = gfxh::half_bright_table();
    ShadeOp o{pal.data(), lut.data(), lut.size()};
    blit16(g, s, d, x, y, sc, "gfx_scene.blitTest", o);
  } else if (op == 2) {
    ColourOp o{pal.data(), static_cast<uint16_t>(extra)};
    blit16(g, s, d, x, y, sc, "gfx_scene.blitTest", o);
  } else if (op == 3) {
    DrawBOp o{pal.data()};
    blit16(g, s, d, x, y, sc, "gfx_scene.blitTest", o);
  } else if (op >= 0x10 && op < 0x10 + sizeof(kAlphaPrograms) / sizeof(kAlphaPrograms[0])) {
    AlphaOp o{pal.data(), &kAlphaPrograms[op - 0x10], extra != 0};
    blit16(g, s, d, x, y, sc, "gfx_scene.blitTest", o);
  } else if (op >= 0x40 && op < 0x40 + 15) {
    // the registered Surface_drawSprite body (gfx_reports.cpp's, delegating to gfxs::surface_draw_sprite) with alpha at the
    // centre of window k, the sprite under a local key with the palette as its own, the destination at the root's top left
    // the centre of the band alpha_body maps to body k
    static const float kWindow[15] = {0.5f, 0.25f, 0.75f, 0.125f, 0.875f, 0.0625f, 0.9375f, 0.375f, 0.625f, 0.4375f,
                                      0.5625f, 0.1875f, 0.8125f, 0.3125f, 0.6875f};
    const float alpha = kWindow[op - 0x40];
    uint32_t ab;
    std::memcpy(&ab, &alpha, 4);
    const uint32_t key = gfx_local_key(0xE0000);
    s.pal = pal;
    g.sprites[key] = s;
    g.ensure_root(ctx);
    const Surface16 saved = g.root;
    std::fill(g.root.px.begin(), g.root.px.end(), uint16_t{0});
    for (int32_t r = 0; r < d.h && r < g.root.h; ++r)
      for (int32_t c = 0; c < d.w && c < g.root.w; ++c)
        g.root.px[static_cast<size_t>(r) * g.root.w + c] = d.px[static_cast<size_t>(r) * d.w + c];
    const Scale before = cur_scale(g);
    set_scale(g, ctx, sc.nx, sc.ny, sc.den);
    call_body(g, ctx, "Surface_drawSprite", {key, g.root_va(ctx), u(x), u(y), ab, 0, extra});
    set_scale(g, ctx, before.nx, before.ny, before.den);
    for (int32_t r = 0; r < d.h && r < g.root.h; ++r)
      for (int32_t c = 0; c < d.w && c < g.root.w; ++c)
        d.px[static_cast<size_t>(r) * d.w + c] = g.root.px[static_cast<size_t>(r) * g.root.w + c];
    g.root = saved;
    g.sprites.erase(key);
  } else if (op == 0x60) {
    // the registered addSprite body with shade 0 and flags -1 (drawQueuedSprite's Surface_blit2 arm = Sprite::drawG with the
    // half-bright table) for a sprite under a local key with `pal` as its own palette; g_zoomScale 0 puts it at (x, y)
    const uint32_t key = gfx_local_key(0xE3000);
    s.pal = pal;
    g.sprites[key] = s;
    g.ensure_root(ctx);
    const Surface16 saved = g.root;
    std::fill(g.root.px.begin(), g.root.px.end(), uint16_t{0});
    for (int32_t r = 0; r < d.h && r < g.root.h; ++r)
      for (int32_t c = 0; c < d.w && c < g.root.w; ++c)
        g.root.px[static_cast<size_t>(r) * g.root.w + c] = d.px[static_cast<size_t>(r) * d.w + c];
    const Scale before = cur_scale(g);
    set_scale(g, ctx, sc.nx, sc.ny, sc.den);
    call_body(g, ctx, "addSprite", {u(x), u(y), 0, u(s.w), u(s.h), key, 0, 0xffffffffu});
    set_scale(g, ctx, before.nx, before.ny, before.den);
    for (int32_t r = 0; r < d.h && r < g.root.h; ++r)
      for (int32_t c = 0; c < d.w && c < g.root.w; ++c)
        d.px[static_cast<size_t>(r) * d.w + c] = g.root.px[static_cast<size_t>(r) * g.root.w + c];
    g.root = saved;
    g.sprites.erase(key);
  } else if (op == 0x50 || op == 0x51) {
    // the registered Image_draw body with a palette object as its mask (0x50: drawB takes the mask's palette for the call) or
    // none (0x51: the sprite's own palette); the mask palette is `pal`, the sprite's own palette its complement
    const uint32_t key = gfx_local_key(0xE1000), mask = gfx_local_key(0xE2000);
    for (int i = 0; i < 256; ++i) s.pal[i] = static_cast<uint16_t>(pal[i] ^ 0x7fff);
    g.sprites[key] = s;
    std::array<uint16_t, 256> *mp = gfxh::palette_of(g, mask, true);
    if (mp) *mp = pal;
    g.ensure_root(ctx);
    const Surface16 saved = g.root;
    std::fill(g.root.px.begin(), g.root.px.end(), uint16_t{0});
    for (int32_t r = 0; r < d.h && r < g.root.h; ++r)
      for (int32_t c = 0; c < d.w && c < g.root.w; ++c)
        g.root.px[static_cast<size_t>(r) * g.root.w + c] = d.px[static_cast<size_t>(r) * d.w + c];
    const Scale before = cur_scale(g);
    set_scale(g, ctx, sc.nx, sc.ny, sc.den);
    call_body(g, ctx, "Image_draw", {key, g.root_va(ctx), u(x), u(y), op == 0x50 ? mask : 0u});
    set_scale(g, ctx, before.nx, before.ny, before.den);
    for (int32_t r = 0; r < d.h && r < g.root.h; ++r)
      for (int32_t c = 0; c < d.w && c < g.root.w; ++c)
        d.px[static_cast<size_t>(r) * d.w + c] = g.root.px[static_cast<size_t>(r) * g.root.w + c];
    g.root = saved;
    g.sprites.erase(key);
  } else {
    g.unsup("gfx_scene.blitTest: an unknown op");
    return;
  }
  std::vector<uint8_t> out(d.px.size() * 2);
  std::memcpy(out.data(), d.px.data(), out.size());
  gfxb::put_file(ctx, "$gfx/scene_blit.bin", out);
}

// renderSpriteQueue.end (tick_render.inc's renderSpriteQueue after its loop): counts the call; a call ordinal a test asked
// for (gfx_scene.captureQueueAt) writes the root surface to `$gfx/queue_<ordinal>.555` (the ORIGINAL's at the same return is
// recorded by log/roadmap/S3/5_tools/scene_rec.py)
void b_queue_end(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  if (LiveTerrainWorld *w = wide_world(ctx)) {   // G32: the frame at the queue's end
    g.ensure_root(ctx);
    w->s1 = g.root.px;
    w->scene_valid = true;
    gfxb::text_wide_end(g, w->scene, w->scene_w, w->scene_e, w->scene_text);  // P9C: the scene without its runs' glyphs
    ++w->scene_passes;
    // development switch SG_WIDE_SCENE_DUMP=<prefix>: the wide scene, its base and s1 of every queue pass go to <prefix>_<pass>.*
    if (const char *dp = std::getenv("SG_WIDE_SCENE_DUMP")) {
      const std::string base = std::string(dp) + "_" + std::to_string(w->scene_passes);
      auto put = [&](const char *ext, const std::vector<uint16_t> &v) {
        if (FILE *f = std::fopen((base + ext).c_str(), "wb")) {
          std::fwrite(v.data(), 2, v.size(), f);
          std::fclose(f);
        }
      };
      put(".scene", w->scene);
      put(".base", w->scene_base);
      put(".s1", w->s1);
    }
  }
  SceneState &ss = st(g);
  ss.queue_calls += 1;
  dlog("end " + std::to_string(ss.queue_calls));
  if (std::find(ss.queue_capture.begin(), ss.queue_capture.end(), ss.queue_calls) == ss.queue_capture.end()) return;
  g.ensure_root(ctx);
  gfxb::put_file(ctx, "$gfx/queue_" + std::to_string(ss.queue_calls) + ".555", gfxb::surface_bytes(g.root));
}
// TEST-ONLY gfx_scene.captureQueueAt(ordinal): ask for the root surface at that renderSpriteQueue return
void b_capture_at(Gfx &g, Ctx &, const uint32_t *a, int) { st(g).queue_capture.push_back(a[0]); }

// TEST-ONLY gfx_scene.dumpNoSprite: `$gfx/scene_nosprite.txt`, one line `<key> <draws>` per sheet cell a draw read with no sprite
void b_dump_no_sprite(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  std::string s;
  for (const auto &kv : st(g).no_sprite) s += std::to_string(kv.first) + " " + std::to_string(kv.second) + " " + st(g).no_sprite_where[kv.first] + "\n";
  gfxb::put_file(ctx, "$gfx/scene_nosprite.txt", std::vector<uint8_t>(s.begin(), s.end()));
}

// TEST-ONLY gfx_scene.spriteInfo(key, n): `$gfx/scene_spriteinfo.bin`, per key from `key` in steps of 0x2c (n keys) i32 w, h
// of the backend's sprite (-1, -1 when it holds none)
void b_sprite_info(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  std::vector<uint8_t> o;
  for (uint32_t k = 0; k < a[1]; ++k) {
    const auto it = g.sprites.find(a[0] + 0x2cu * k);
    const int32_t v[2] = {it == g.sprites.end() ? -1 : it->second.w, it == g.sprites.end() ? -1 : it->second.h};
    o.insert(o.end(), reinterpret_cast<const uint8_t *>(v), reinterpret_cast<const uint8_t *>(v) + 8);
  }
  gfxb::put_file(ctx, "$gfx/scene_spriteinfo.bin", o);
}

// TEST-ONLY gfx_scene.setPalette(key, words): the palette object `key` (gfxh::palette_of) gets the 256 RGB555 words at `words`,
// as a loadImage with a palette argument would fill it; lets a test draw a sheet cell with a mask palette (roadmap S3.11:
// test_flic's hook-up test, U-1332)
void b_set_palette(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  std::array<uint16_t, 256> &p = *gfxh::palette_of(g, a[0], true);
  for (uint32_t i = 0; i < 256; ++i) p[i] = ctx.mem.load<uint16_t>(a[1] + 2 * i);
}

const GfxBodyDef kBodies[] = {
    {"loadWorldAssets.palettes", b_world_palettes},
    {"drawQueuedSprite.draw", b_queue_draw},
    {"drawQueuedSprite.label", b_queue_label},
    {"Sprite_draw", b_sprite_draw},
    {"composeGolferSprite", b_compose},
    {"addSprite", b_add_sprite},
    {"gfx_scene.blitTest", b_blit_test},
    {"renderSpriteQueue.end", b_queue_end},
    {"renderSpriteQueue.begin", b_queue_begin},   // G32
    {"gfx_scene.captureQueueAt", b_capture_at},
    {"gfx_scene.dumpNoSprite", b_dump_no_sprite},
    {"gfx_scene.spriteInfo", b_sprite_info},
    {"gfx_scene.setPalette", b_set_palette},
};

}  // namespace

namespace gfxs {

// Image_draw(this, dst, x, y, mask) (c3df.cpp ImageDraw_re) for the cases gfx.cpp's body hands over (see gfx_scene.h)
bool image_draw(Gfx &g, Ctx &ctx, const uint32_t *a) {
  const Scale sc = cur_scale(g);
  const bool unit1 = sc.nx == 1 && sc.ny == 1 && sc.den == 1;
  // a mask (a palette object) is drawB's key for this call (c3df.cpp ImageDraw_re passes mask->m_4; jgld Sprite::drawB sets
  // the Sprite's palette word to it and puts the old one back): drawn here at any scale
  if (unit1 && a[4] == 0 && st(g).sprite_pal.count(a[0]) == 0 && !in_cell_pool(ctx, a[0])) return false;
  g.ensure_root(ctx);
  const char *who = "Image_draw";
  if (!gfx_is_local_key(a[0]) && ctx.mem.load<uint32_t>(a[0] + 8) != 0) {
    g.unsup("Image_draw: the load arm (a file name at +8)");
    return true;
  }
  if (a[1] != g.root_va(ctx)) {
    g.unsup("Image_draw: a destination other than the root surface");
    return true;
  }
  auto it = g.sprites.find(a[0]);
  if (it == g.sprites.end()) {
    g.unsup("Image_draw: a SpriteRef the backend has no sprite for");
    return true;
  }
  if (sc.den == 0) {
    g.unsup(std::string(who) + ": a display scale with denominator 0");
    return true;
  }
  const Sprite8 &s = it->second;
  const uint16_t *pal = sprite_palette(g, ctx, a[0], s, mode() == 10 ? 0 : a[4], false, who);
  if (!pal) return true;
  DrawBOp op{pal};
  blit16(g, s, g.root, add32(trim(s.left, sc.nx, sc.den), static_cast<int32_t>(a[2])),
         add32(trim(s.top, sc.ny, sc.den), static_cast<int32_t>(a[3])), sc, who, op);
  return true;
}

// Surface_drawSprite(this, dst, x, y, alpha, mask, flag) (c3da.cpp SurfaceDrawSprite_re: handle slot +0x94 = Sprite::drawAlpha
// with this +0x20 / +0x24 added unscaled): every alpha window, the display scale in force and a mask palette object. The one
// registered body of the name is gfx_reports.cpp's (roadmap S2.13); it hands the cases it does not model to this function.
bool surface_draw_sprite(Gfx &g, Ctx &ctx, const uint32_t *a) {
  draw_alpha(g, ctx, a[0], a[1], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), a[4], a[5], a[6], "Surface_drawSprite");
  return true;
}

// addSprite (c3ei.cpp AddSprite_re) without its queue entry: the position as enqueueSprite / addSprite compute it, then
// drawQueuedSprite's drawing (the name label of a golfer entry is not drawn: it needs the queue's anchor and the text the
// simulation part builds)
void add_sprite(Gfx &g, Ctx &ctx, const int32_t a[8]) {
  const GameState &gs = ctx.gs();
  int32_t d = 0x10;
  const int32_t w = gs.g_screenWidth;
  if (w > 1000) d = 0xd;
  if (w >= 0x4b0) d = 0xa;
  if (a[7] & 0x100) d = 0x10;
  const int32_t s = gs.g_zoomScale;
  int32_t x, y;
  if (a[6] == 0) {
    const int32_t px = mul32(s, a[3]), py = mul32(s, a[4]);
    x = sub32(a[0], sar32(add32(px, sar32(px, 31) & 3), 2));
    y = sub32(a[1], sar32(add32(py, sar32(py, 31) & 3), 2));
  } else if (a[6] > 0) {
    x = sub32(a[0], div32(mul32(mul32(s, a[3]), a[6]), d));
    y = sub32(a[1], div32(mul32(mul32(s, a[4]), a[6]), d));
  } else {
    x = add32(div32(mul32(mul32(s, a[3]), a[6]), add32(d, d)), a[0]);
    y = add32(div32(mul32(mul32(s, a[4]), a[6]), add32(d, d)), a[1]);
  }
  draw_entry(g, ctx, x, y, a[6], a[7], static_cast<uint32_t>(a[5]));
}

}  // namespace gfxs

// ---- P9E fix 4 (gfx.h gfx_set_hud_cursor): the wide scene around the frame code's cursor passes
void gfx_set_hud_cursor(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  g->hud.cursor_sprites = on && g->hud.on;
}
void gfx_cursor_sprites_begin(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.cursor_sprites) return;
  ++g->hud.cursor_depth;
  LiveTerrainWorld *w = wide_world(ctx);
  if (!w || !w->scene_valid) return;
  if (w->scene_pre.size() != w->scene.size()) w->scene_pre = w->scene;  // the first pass's start (P6_ToolLabel, then P7)
}
void gfx_cursor_sprites_end(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.cursor_sprites) return;
  if (g->hud.cursor_depth > 0) --g->hud.cursor_depth;
  LiveTerrainWorld *w = wide_world(ctx);
  if (!w || !w->scene_valid || w->scene_pre.size() != w->scene.size()) return;
  w->scene_cur = w->scene;
}

extern const GfxFamily kGfxFamily_scene = {"scene", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
