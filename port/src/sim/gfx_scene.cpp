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
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>   // P15A: hr_flush's bands take the world thread's priority
#endif
#include "portsim/sim/gfx_scene.h"

#include <algorithm>
#include <unordered_map>
#include <array>
#include <atomic>
#include <functional>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/flic.h"
#include "portsim/sim/golfers.h"
#include "portsim/sim/gfx_hud.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_live.h"   // G32: the wide scene
#include "portsim/sim/terrain_overlay.h"  // P23D fix 1: grid_queue_begin
#include "portsim/sim/perf.h"   // P15A: detail counters

namespace portsim {
extern int g_defect_zoom8_font;  // tick.cpp mainLoop.zoom8Font (P13C)
extern int g_defect_label_under_sprites;  // tick.cpp renderSpriteQueue.labelUnderLaterSprites (P22C item 6)

int g_negctl_gfx_scene = 0;
extern int g_defect_tool_zoom4;  // tick.cpp mainLoop.toolForcesZoom4 (P11E / P13A: draw_alpha's offsets at zooms 1 and 2)

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
// P10D fix 1 (terrain_live.h LiveTerrainWorld::hr): while a queue entry is drawn on the wide scene, `tag` names the scene's
// words and every word a scene body writes there gets `val` in the tag plane; `hr` is the world whose hr plane the blitter
// skeleton draws the same sprite into (null outside the queue's wide draws)
struct HrPass {
  uintptr_t base = 0;   // the scene's first word (the swapped root's pixels)
  size_t n = 0;
  uint8_t *tag = nullptr;
  uint8_t val = 0;
  LiveTerrainWorld *hr = nullptr;
  int plane = 0;   // P10D fix 2: 0 = queued for hr_flush, 1 = drawn at once into hr, 2 = drawn at once into hrc
};
thread_local HrPass g_hr_pass;
// every pixel a scene body writes; TEST-ONLY mode 7 (the sprite-pixel gate's mask run, not a control) stores it inverted
inline void put(uint16_t &w, uint32_t v) {
  w = static_cast<uint16_t>(mode() == 7 ? v ^ 0x7fffu : v);
  hud_mark(&w);  // P10C fix 2
  if (g_hr_pass.tag) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(&w);
    if (p >= g_hr_pass.base && (p - g_hr_pass.base) / sizeof(uint16_t) < g_hr_pass.n)
      g_hr_pass.tag[(p - g_hr_pass.base) / sizeof(uint16_t)] = g_hr_pass.val;
  }
}

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
void blit16_core(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y, Scale sc, const char *who, Op &op) {
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

// ---- P10D fix 1: the same sprite into the world layer's hr plane (terrain_live.h LiveTerrainWorld::hr)
// An op that records layer pixels (AlphaOp::rec) records nothing for the hr plane
template <class Op>
auto hr_no_rec(Op &o, int) -> decltype(o.rec = nullptr, void()) { o.rec = nullptr; }
template <class Op>
void hr_no_rec(Op &, long) {}
// The scene rectangle the skeleton above covers is the rectangle (rx, ry) + (w, h) * (fx, fy): unscaled (rx, ry) = (x, y) and
// f = 1; scaled (rx, ry) = (x + (int)fx, y + (int)fy) with fx = (float)|nx| / den (the skeleton's placement). Scene pixel
// (sx, sy) maps to world pixel ((sx - scene_e) * fw / 800 + ox, sy * ph / 600), so world pixel (X, Y)'s centre reads the
// sprite's art at u = ((X + 0.5 - ox) * 800 / fw + scene_e - rx) / fx, v = ((Y + 0.5) * 600 / ph - ry) / fy. Each sample is
// bilinear over the four texels around (u - 0.5, v - 0.5); where one world pixel covers more than one texel (the art drawn
// smaller), n x n samples spread over the pixel (n = the texels per pixel rounded up, at most 4) are averaged. A texel is
// drawn through the entry's own pixel rule over the hr pixel (payload 0xff and texels outside the art leave it), and the
// results are mixed per 5-bit channel by the sample weights, so the art's edges and the plane / shade / alpha payloads blend
// with what is under them at the window's resolution.
//
// The draws are queued while the frame code runs (HrCmd: the art, the placement, the op with its own copy of the palette, since
// composeGolferSprite rewrites g_images[0]'s words between two golfer entries) and run at renderSpriteQueue's end (hr_flush) on
// several threads, each over its own band of world rows (the rows Y = t mod n, so the bands share the sprites evenly) and every
// command in queue order, so the painter's order is kept.
struct HrGeom {
  double rx = 0, ry = 0, fx = 1, fy = 1;
  bool ok = false;
};
HrGeom hr_geom(int32_t x, int32_t y, Scale sc, const Sprite8 &s) {
  HrGeom r;
  if (sc.den == 0 || sc.nx < 0 || sc.ny < 0 || s.w <= 0 || s.h <= 0) return r;   // the skeleton draws nothing for these
  const int32_t ax = abs32(sc.nx), ay = abs32(sc.ny), ad = abs32(sc.den);
  r.rx = x;
  r.ry = y;
  if (!(ax == ad && ay == ad)) {
    if (ax == 0 || ay == 0) return r;
    const float ffx = static_cast<float>(static_cast<double>(ax) / static_cast<double>(sc.den));
    const float ffy = static_cast<float>(static_cast<double>(ay) / static_cast<double>(sc.den));
    r.rx = static_cast<double>(add32(x, static_cast<int32_t>(ffx)));
    r.ry = static_cast<double>(add32(y, static_cast<int32_t>(ffy)));
    r.fx = ffx;
    r.fy = ffy;
  }
  r.ok = r.fx > 0.0 && r.fy > 0.0;
  return r;
}
// the world pixels a command can change: its art's rectangle mapped to the world, one pixel more on every side (the bilinear
// taps), clipped to the world; x0 >= x1 or y0 >= y1 = none
struct HrBox {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  bool operator==(const HrBox &o) const { return x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1; }
  bool empty() const { return x0 >= x1 || y0 >= y1; }
};
HrBox hr_box(const LiveTerrainWorld &w, const Sprite8 &s, const HrGeom &gm) {
  const double kx = static_cast<double>(w.fw) / 800.0, ky = static_cast<double>(w.ph) / 600.0;
  const double wx0 = (gm.rx - w.scene_e) * kx + w.ox, wx1 = (gm.rx + s.w * gm.fx - w.scene_e) * kx + w.ox;
  const double wy0 = gm.ry * ky, wy1 = (gm.ry + s.h * gm.fy) * ky;
  HrBox b;
  b.x0 = static_cast<int32_t>(std::max(0.0, std::floor(wx0) - 1));
  b.x1 = static_cast<int32_t>(std::min(static_cast<double>(w.pw), std::ceil(wx1) + 1));
  b.y0 = static_cast<int32_t>(std::max(0.0, std::floor(wy0) - 1));
  b.y1 = static_cast<int32_t>(std::min(static_cast<double>(w.ph), std::ceil(wy1) + 1));
  return b;
}

// P13D fix 5 (Mariano's play-test 6 item 4, option C; log/roadmap/P13/P13D_notes.md): --sprite-filter edge (hr_filter 3). Before
// the sharp sampling (filter 1) the art is scaled up by Scale2x (EPX), on its 8-bit payloads: each texel P becomes four, a corner
// taking its two edge neighbours' payload where those two are equal and differ from the other two (top left: left == up, left !=
// down, up != right), P elsewhere; a neighbour outside the art is 0xff (no draw). No new payload appears, so the entry's pixel rule
// (palette, plane / shade tables, alpha bodies, the 0xff skip) is unchanged, and the staircase of a diagonal edge or outline turns
// into a finer one with its corners cut. Applied once where a texel covers 1.5 world pixels or more, twice (4 x) from 3; below 1.5
// the art is used as it is. The scaled art replaces the command's art, and its texels are 1 / 2 (1 / 4) as wide (fx, fy divided):
// the rectangle and so hr_box are the same. The scaled payloads are cached per art (FNV-1a of the payloads and size, plus the
// level), cleared past 4096 entries or 64 MB.
uint64_t hr_hash(uint64_t h, const void *p, size_t n);   // below
std::vector<uint8_t> scale2x(const std::vector<uint8_t> &a, int32_t w, int32_t h) {
  std::vector<uint8_t> o(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  const size_t W = static_cast<size_t>(w) * 2;
  auto at = [&](int32_t x, int32_t y) -> uint8_t {
    return x < 0 || y < 0 || x >= w || y >= h ? uint8_t(0xff) : a[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
  };
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const uint8_t P = at(x, y), A = at(x, y - 1), B = at(x + 1, y), C = at(x - 1, y), D = at(x, y + 1);
      uint8_t *r0 = &o[static_cast<size_t>(2 * y) * W + static_cast<size_t>(2 * x)], *r1 = r0 + W;
      r0[0] = (C == A && C != D && A != B) ? A : P;
      r0[1] = (A == B && A != C && B != D) ? B : P;
      r1[0] = (D == C && D != B && C != A) ? C : P;
      r1[1] = (B == D && B != A && D != C) ? D : P;
    }
  return o;
}
struct HrEdgeCache {
  std::unordered_map<uint64_t, std::shared_ptr<const Sprite8>> art;
  size_t bytes = 0;
  int64_t hits = 0, misses = 0;
};
thread_local HrEdgeCache g_hr_edge;
// the art the hr planes sample for `s` at placement gm: with filter 3 and a texel of 1.5+ world pixels the cached Scale2x art (gm's
// texel size divided), else null (sample `s` itself). The commands share the cached art (no copy per draw).
std::shared_ptr<const Sprite8> hr_art(const LiveTerrainWorld &w, const Sprite8 &s, HrGeom &gm) {
  if (w.hr_filter != 3 || s.w <= 0 || s.h <= 0 || s.px.size() != static_cast<size_t>(s.w) * static_cast<size_t>(s.h)) return nullptr;
  const double mag = std::min(static_cast<double>(w.fw) / 800.0 * gm.fx, static_cast<double>(w.ph) / 600.0 * gm.fy);
  const int lv = mag >= 3.0 ? 2 : mag >= 1.5 ? 1 : 0;
  if (lv == 0 || static_cast<int64_t>(s.w) * s.h > 512 * 512) return nullptr;
  uint64_t key = 0xcbf29ce484222325ull;
  key = hr_hash(key, &s.w, sizeof(s.w));
  key = hr_hash(key, &s.h, sizeof(s.h));
  key = hr_hash(key, &lv, sizeof(lv));
  key = hr_hash(key, s.px.data(), s.px.size());
  HrEdgeCache &c = g_hr_edge;
  const int32_t f = 1 << lv;
  auto it = c.art.find(key);
  std::shared_ptr<const Sprite8> up;
  if (it != c.art.end()) {
    up = it->second;
    ++c.hits;
  } else {
    auto a = std::make_shared<Sprite8>();
    a->w = s.w * f;
    a->h = s.h * f;
    a->px = scale2x(s.px, s.w, s.h);
    if (lv == 2) a->px = scale2x(a->px, s.w * 2, s.h * 2);
    if (c.art.size() >= 4096 || c.bytes > (64u << 20)) {
      c.art.clear();
      c.bytes = 0;
    }
    c.bytes += a->px.size();
    up = a;
    c.art.emplace(key, up);
    ++c.misses;
  }
  gm.fx /= f;
  gm.fy /= f;
  return up;
}

// one command over the world pixels of its box inside the clip rectangle [cx0, cx1) x [cy0, cy1), into plane / cover (pw x ph)
template <class Op>
void hr_blit_rows(const LiveTerrainWorld &w, uint16_t *plane, uint8_t *cover, const Sprite8 &s, const HrGeom &gm, Op &op,
                  const HrBox &box, int32_t cx0, int32_t cy0, int32_t cx1, int32_t cy1) {
  const double rx = gm.rx, ry = gm.ry, fx = gm.fx, fy = gm.fy;
  const double kx = static_cast<double>(w.fw) / 800.0, ky = static_cast<double>(w.ph) / 600.0;
  const int32_t X0 = std::max(box.x0, cx0), X1 = std::min(box.x1, cx1);
  const int32_t Y0 = std::max(box.y0, cy0), Y1 = std::min(box.y1, cy1);
  if (X0 >= X1 || Y0 >= Y1) return;
  // texels per world pixel (u per X, v per Y) and the samples per axis
  const double du = 1.0 / (kx * fx), dv = 1.0 / (ky * fy);
  const int nsx = std::min(4, std::max(1, static_cast<int>(std::ceil(du - 1e-6))));
  const int nsy = std::min(4, std::max(1, static_cast<int>(std::ceil(dv - 1e-6))));
  // the samples' texel columns and their weights in 1 / 256 per (X, si); a column outside the art is -1
  struct Col {
    int32_t c0, c1;   // texel columns i0, i0 + 1 (-1: outside the art)
    uint32_t w1;      // the weight of i0 + 1 (i0 gets 256 - w1)
    uint32_t s1;      // P10H: the sharp filter's weight of i0 + 1
  };
  // P10H fix 1: the sharp filter. A sample at texel fraction t between texels i0 and i0 + 1 (bilinear weight t) gets the weight
  // clamp((t - 0.5) * k + 0.5, 0, 1) instead, k = the sample's world pixels per texel (at least 1): the share of the sample's own
  // footprint (1 / k texel wide, centred at t) lying on texel i0 + 1. Every texel is then a flat block and the step between two
  // texels is one window pixel wide whatever the scale (no uneven block widths, no shimmer at a non-integer scale); k = 1 is
  // bilinear. Filter 1 keeps the bilinear weights' total over the texels that draw (the outline's coverage) and spreads it over
  // them by the sharp weights, so the colours are sharp and the object's edge over what is under it is the smooth one.
  const int filter = w.hr_filter == 3 ? 1 : w.hr_filter;   // P13D fix 5: the edge filter samples its scaled art as sharp does
  const auto sharp_w = [](double t, double k) {
    const double s = std::min(1.0, std::max(0.0, (t - 0.5) * k + 0.5));
    return static_cast<uint32_t>(std::floor(s * 256.0 + 0.5));
  };
  Col cu[64 * 4];   // the clip rectangles are at most 64 columns wide (hr_flush's spans are cut to that)
  const int32_t nx = X1 - X0;
  if (nx > 64) {
    hr_blit_rows(w, plane, cover, s, gm, op, box, X0, Y0, X0 + 64, Y1);
    hr_blit_rows(w, plane, cover, s, gm, op, box, X0 + 64, Y0, X1, Y1);
    return;
  }
  const double ksx = std::max(1.0, nsx / du), ksy = std::max(1.0, nsy / dv);   // P10H: samples per texel, per axis
  for (int32_t X = X0; X < X1; ++X)
    for (int si = 0; si < nsx; ++si) {
      const double uu = (((X + (si + 0.5) / nsx) - w.ox) / kx + w.scene_e - rx) / fx - 0.5;
      const double uf = std::floor(uu);
      const int32_t i0 = static_cast<int32_t>(uf);
      cu[static_cast<size_t>(X - X0) * static_cast<size_t>(nsx) + static_cast<size_t>(si)] =
          Col{i0 >= 0 && i0 < s.w ? i0 : -1, i0 + 1 >= 0 && i0 + 1 < s.w ? i0 + 1 : -1,
              static_cast<uint32_t>(std::floor((uu - uf) * 256.0 + 0.5)), filter ? sharp_w(uu - uf, ksx) : 0u};
    }
  const uint8_t *art = s.px.data();
  const size_t sw = static_cast<size_t>(s.w);
  const int ns = nsx * nsy;
  const double inv = 1.0 / (65536.0 * ns);
  for (int32_t Y = Y0; Y < Y1; ++Y) {
    // the rows of every vertical sample (nullptr: outside the art) and their weights
    const uint8_t *r0[4], *r1[4];
    uint32_t wv[4], sv[4];
    bool rows = false;
    for (int sj = 0; sj < nsy; ++sj) {
      const double v = ((Y + (sj + 0.5) / nsy) / ky - ry) / fy - 0.5;
      const double vf = std::floor(v);
      const int32_t j0 = static_cast<int32_t>(vf);
      r0[sj] = j0 >= 0 && j0 < s.h ? art + static_cast<size_t>(j0) * sw : nullptr;
      r1[sj] = j0 + 1 >= 0 && j0 + 1 < s.h ? art + static_cast<size_t>(j0 + 1) * sw : nullptr;
      wv[sj] = static_cast<uint32_t>(std::floor((v - vf) * 256.0 + 0.5));
      sv[sj] = filter ? sharp_w(v - vf, ksy) : 0u;
      rows = rows || r0[sj] || r1[sj];
    }
    if (!rows) continue;
    uint16_t *hrow = plane + static_cast<size_t>(Y) * static_cast<size_t>(w.pw);
    uint8_t *crow = cover + static_cast<size_t>(Y) * static_cast<size_t>(w.pw);
    for (int32_t X = X0; X < X1; ++X) {
      const Col *tu = &cu[static_cast<size_t>(X - X0) * static_cast<size_t>(nsx)];
      const uint16_t d = hrow[X];
      uint32_t r = 0, gg = 0, b = 0;   // sums of weight (1 / 65536) * channel, at most 16 samples * 65536 * 31
      double rs = 0, gs = 0, bs = 0;   // P10H: the sharp filter's sums (same unit)
      bool any = false;
      for (int sj = 0; sj < nsy; ++sj) {
        const uint32_t bv = wv[sj];
        for (int si = 0; si < nsx; ++si) {
          const Col &cl = tu[si];
          const uint32_t bu = cl.w1;
          const uint8_t c[4] = {r0[sj] && cl.c0 >= 0 ? r0[sj][cl.c0] : uint8_t(0xff), r0[sj] && cl.c1 >= 0 ? r0[sj][cl.c1] : uint8_t(0xff),
                                r1[sj] && cl.c0 >= 0 ? r1[sj][cl.c0] : uint8_t(0xff), r1[sj] && cl.c1 >= 0 ? r1[sj][cl.c1] : uint8_t(0xff)};
          if (c[0] == c[1] && c[0] == c[2] && c[0] == c[3]) {
            uint16_t res = d;
            if (c[0] != 0xff) {
              op(c[0], res);
              if (res != d) any = true;
            }
            r += 65536u * ((res >> 10) & 31u);
            gg += 65536u * ((res >> 5) & 31u);
            b += 65536u * (res & 31u);
            continue;
          }
          const uint32_t wt[4] = {(256 - bu) * (256 - bv), bu * (256 - bv), (256 - bu) * bv, bu * bv};
          if (filter) {  // P10H fix 1: the sharp weights (see sharp_w above)
            const uint32_t su = cl.s1, sb = sv[sj];
            const uint32_t ws[4] = {(256 - su) * (256 - sb), su * (256 - sb), (256 - su) * sb, su * sb};
            uint16_t res[4];
            uint32_t cov = 0, sws = 0;   // over the texels that draw: bilinear and sharp weight
            for (int k = 0; k < 4; ++k) {
              res[k] = d;
              if (c[k] != 0xff) {
                op(c[k], res[k]);
                if (res[k] != d) any = true;
                cov += wt[k];
                sws += ws[k];
              }
            }
            // the drawing texels' weights: filter 1 = the bilinear coverage cov spread by the sharp weights (by the bilinear ones
            // when no drawing texel has a sharp weight: the sample sits on the outline's transparent side); filter 2 = the sharp
            // weights as they are (the outline area-weighted too). The rest of the sample keeps the pixel under it.
            const uint32_t *cw = filter == 1 && sws == 0 ? wt : ws;
            const double kk = filter == 2 ? 1.0 : static_cast<double>(cov) / static_cast<double>(sws ? sws : cov ? cov : 1u);
            double rest = 65536.0;
            for (int k = 0; k < 4; ++k) {
              if (c[k] == 0xff) continue;
              const double e = kk * cw[k];
              rest -= e;
              rs += e * ((res[k] >> 10) & 31u);
              gs += e * ((res[k] >> 5) & 31u);
              bs += e * (res[k] & 31u);
            }
            rs += rest * ((d >> 10) & 31u);
            gs += rest * ((d >> 5) & 31u);
            bs += rest * (d & 31u);
            continue;
          }
          for (int k = 0; k < 4; ++k) {
            uint16_t res = d;
            if (c[k] != 0xff && wt[k] != 0) {
              op(c[k], res);
              if (res != d) any = true;
            }
            r += wt[k] * ((res >> 10) & 31u);
            gg += wt[k] * ((res >> 5) & 31u);
            b += wt[k] * (res & 31u);
          }
        }
      }
      if (!any) continue;
      if (filter) {  // P10H: the sharp sums (the samples of the uniform fast path above added to r / gg / b)
        const auto chs = [inv](double c) { return std::min<uint32_t>(31u, static_cast<uint32_t>(c * inv + 0.5)); };
        hrow[X] = static_cast<uint16_t>((chs(r + rs) << 10) | (chs(gg + gs) << 5) | chs(b + bs));
        crow[X] = 1;
        continue;
      }
      const auto ch = [inv](uint32_t c) { return std::min<uint32_t>(31u, static_cast<uint32_t>(c * inv + 0.5)); };
      hrow[X] = static_cast<uint16_t>((ch(r) << 10) | (ch(gg) << 5) | ch(b));
      crow[X] = 1;
    }
  }
}

// FNV-1a over bytes, 64 bits
uint64_t hr_hash(uint64_t h, const void *p, size_t n) {
  const uint8_t *b = static_cast<const uint8_t *>(p);
  for (size_t i = 0; i < n; ++i) {
    h ^= b[i];
    h *= 0x100000001b3ull;
  }
  return h;
}

struct HrCmd {
  Sprite8 s;                         // the art (w, h, px)
  std::shared_ptr<const Sprite8> up;   // P13D fix 5: the edge filter's scaled art (sampled in place of s when set)
  HrGeom gm;
  HrBox box;
  std::array<uint16_t, 256> pal{};   // the op's palette words at the draw
  uint64_t sig = 0;                  // art, palette, op parameters, placement (hr_flush compares it with the last queue's)
  GfxMoverSrc src;                   // P12A fix 2: the queue entry's source (render-only; not in sig)
  int32_t trim_x = 0, trim_y = 0;    // P12A fix 2: the art's trim offsets at the draw's scale (sprite_draw adds them to x / y)
  int32_t art_w0 = 0, art_h0 = 0;    // P15C: the art's own size (s is left empty where the edge filter's scaled art `up` is used)
  const Sprite8 &art() const { return up ? *up : s; }  // P15C: the payloads the draw samples (as run does)
  virtual ~HrCmd() = default;
  virtual void run(const LiveTerrainWorld &w, uint16_t *plane, uint8_t *cover, int32_t cx0, int32_t cy0, int32_t cx1, int32_t cy1) = 0;
  // P12A fix 2: the same draw at another placement (gfx_hr_scene_redraw); const: any thread
  virtual void run_at(const LiveTerrainWorld &w, const HrGeom &g, const HrBox &b, uint16_t *plane, uint8_t *cover, int32_t cx0,
                      int32_t cy0, int32_t cx1, int32_t cy1) const = 0;
  virtual bool gpu(GfxHrGpuOp &o) const = 0;  // P15C: the op's parameters for the app's GPU path (false: not one it draws)
};
// P15C: an op the GPU path does not know (the ops it draws have their own overloads below their definitions)
template <class Op>
bool hr_gpu_op(const Op &, GfxHrGpuOp &) {
  return false;
}
template <class Op>
struct HrCmdT final : HrCmd {
  Op op;
  explicit HrCmdT(const Op &o) : op(o) {}
  void run(const LiveTerrainWorld &w, uint16_t *plane, uint8_t *cover, int32_t cx0, int32_t cy0, int32_t cx1, int32_t cy1) override {
    Op local = op;   // each thread its own copy (an op's report flags)
    hr_blit_rows(w, plane, cover, up ? *up : s, gm, local, box, cx0, cy0, cx1, cy1);
  }
  void run_at(const LiveTerrainWorld &w, const HrGeom &g, const HrBox &b, uint16_t *plane, uint8_t *cover, int32_t cx0, int32_t cy0,
              int32_t cx1, int32_t cy1) const override {
    Op local = op;
    hr_blit_rows(w, plane, cover, art(), g, local, b, cx0, cy0, cx1, cy1);  // P15C: art(), not s (empty under the edge filter)
  }
  bool gpu(GfxHrGpuOp &o) const override { return hr_gpu_op(op, o); }
};
// P10D fix 3: the queue's draws and what the last flush left. A queue whose terrain base and commands equal the last one's in a
// 32 x 32 tile is not drawn again there: the tile keeps the last flush's pixels.
}  // namespace (P12A fix 2: GfxHrScene is declared in gfx.h, outside it)
struct GfxHrScene {
  std::shared_ptr<LiveTerrainWorld> w;            // the world's geometry for hr_blit_rows (pw, ph, fw, ox, scene_e, hr_filter only)
  // the world layer's pixels at the queue's start, only in the 32 x 32 tiles of base_tile (P12A fix 3: the tiles the boxes of the
  // tagged draws meet once widened by the largest step an object may be drawn away from its place, kGfxMoverMaxStep; each tile is
  // copied once; a plane of pw x ph words whose other words are never written nor read, so a queue costs the copy of those tiles
  // instead of the whole base)
  std::unique_ptr<uint16_t[]> base;
  size_t base_n = 0;
  std::vector<uint8_t> base_tile;  // tx * ty, 1 = copied
  int32_t base_tx = 0, base_ty = 0;
  std::vector<std::shared_ptr<const HrCmd>> cmds;  // the queue's draws in order, then the draws made at once into hr after it
  std::vector<GfxHrDraw> draws;
};
namespace {
std::atomic<bool> g_hr_scene_on{false};  // P12A fix 2 (gfx_set_hr_scene): the app wants the published scenes
thread_local bool t_hr_selftest = false;  // P17B: this thread runs gfx_hr_gpu_selftest (no publication; the process-wide switch untouched)
std::atomic<int64_t> g_hr_pub_ns{0};      // P12A fix 3: the publications' time and count (gfx_hr_scene_cost)
std::atomic<int64_t> g_hr_pub_n{0};
thread_local GfxMoverSrc g_draw_src;      // P12A fix 2: the source of the queue entry b_queue_draw is drawing (hr_blit copies it)
thread_local GfxMoverSrc g_label_src;     // P12A fix 2: the source of the queue entry b_queue_label is drawing the label of
// P22C item 6 (renderSpriteQueue.labelUnderLaterSprites = 0): the golfer name labels of renderSpriteQueue's walk, kept with the name
// text the simulation part left in g_textBuffer for them, drawn at the walk's end (b_queue_end) over every sprite of the queue
struct DeferredLabel {
  uint32_t entry = 0;
  std::string text;
};
thread_local bool g_queue_walk = false;
thread_local std::vector<DeferredLabel> g_deferred_labels;
// P12A fix 2: the queue's render-only side table (gfx.h gfx_mover_source): entry n's source, and the one named for the next entry
thread_local std::array<GfxMoverSrc, 1024> g_mover_tab;
thread_local GfxMoverSrc g_mover_next;
struct HrQueue {
  LiveTerrainWorld *w = nullptr;
  std::vector<std::shared_ptr<HrCmd>> cmds;   // P12A fix 2: shared (the published scene keeps them)
  std::shared_ptr<GfxHrScene> pub;            // P12A fix 2: the last flush's scene (the draws made at once after it are added)
  bool pub_taken = false;                     // P12A fix 2: handed to a present (no more additions)
  // the world's hr / hr_cov planes are kept between queues: they hold the last flush's sprites (and the draws made at once into hr
  // after it, whose boxes post_boxes keeps for the next queue to draw again)
  bool last_ok = false;
  const LiveTerrainWorld *last_w = nullptr;
  int32_t last_pw = 0, last_ph = 0;
  std::vector<uint16_t> last_base;     // the world layer's pixels at the queue's start (updated in place at each start)
  std::vector<uint64_t> last_sig;
  std::vector<HrBox> last_box;
  std::vector<HrBox> post_boxes;
  std::vector<uint8_t> dirty;          // this queue's dirty tiles (tx x ty), from its start
  int32_t tx = 0, ty = 0;
  size_t defer_np = 0;                 // P15A: the frame thread's queue start recorded with an hr plane of this size (0: none)
  bool gpu = false;                    // P17B: the planes (hr, hr_cov, hrc, hrb) live in the thread's GPU backend (gfx.h GfxHrGpu)
  bool base_stale = false;             // P17B fix 3: last_base's words are not kept (the backend's base is the queue's base); size only
};
thread_local HrQueue g_hr_queue;
constexpr int32_t kHrTile = 32;
static_assert(kHrTile == kGfxHrTile, "P15C: the app reads the base tiles with gfx.h kGfxHrTile");

// P15A (gfx.h gfx_set_hr_defer; log/roadmap/P15/P15A_notes.md): the hr work recorded on the frame thread and run on the world thread
std::atomic<bool> g_hr_defer{false};
std::atomic<bool> g_hr_check{false};     // gfx_set_hr_defer_check: the frame thread records AND runs (the A/B check)
thread_local bool t_hr_runner = false;   // this thread runs a GfxHrJob (gfx_hr_job_run): the hr functions run, not record
bool hr_recording() { return g_hr_defer.load(std::memory_order_relaxed) && !t_hr_runner; }
bool hr_checking() { return g_hr_check.load(std::memory_order_relaxed); }
// the world geometry an hr op runs with (the frame thread's at the op), put on the world thread's LiveTerrainWorld first
struct HrGeo {
  int32_t pw = 0, ph = 0, fw = 0, ox = 0, scene_e = 0;
  int hr_filter = 0;
  void put(LiveTerrainWorld &w) const {
    w.pw = pw;
    w.ph = ph;
    w.fw = fw;
    w.ox = ox;
    w.scene_e = scene_e;
    w.hr_filter = hr_filter;
  }
};
HrGeo hr_geo(const LiveTerrainWorld &w) { return HrGeo{w.pw, w.ph, w.fw, w.ox, w.scene_e, w.hr_filter}; }

// ---- P17B (gfx.h GfxHrGpu; log/roadmap/P17/P17B_notes.md): the job thread's GPU backend for the hr planes
thread_local GfxHrGpu *t_hr_gpu = nullptr;
thread_local bool t_hr_gpu_dead = false;
GfxHrGpuCheck g_hr_gpu_st;   // the job thread's counters (one thread runs the jobs; read after it stopped)
int hr_gpu_check_every() {
  static const int n = [] {
    const char *e = std::getenv("SG_P17_WORLDCHECK");
    return e ? std::max(0, std::atoi(e)) : 0;
  }();
  return n;
}
// the backend when this thread runs jobs (or the self-test) and it has not failed
GfxHrGpu *hr_gpu_alive() { return t_hr_runner && t_hr_gpu && !t_hr_gpu_dead ? t_hr_gpu : nullptr; }
// a backend call failed: it is not used again on this thread. With the CPU planes kept beside it (SG_P17_WORLDCHECK) they carry on;
// otherwise the planes hold nothing: no reuse at the next queue start (every tile drawn again on the CPU from last_base, which the CPU
// always keeps), the flags off until then, and hr_lost tells the host that this job's picture is not to be published
void hr_gpu_failed(LiveTerrainWorld &w, const char *why) {
  if (t_hr_gpu && !t_hr_gpu_dead) t_hr_gpu->dead(why);
  t_hr_gpu_dead = true;
  ++g_hr_gpu_st.failures;
  HrQueue &q = g_hr_queue;
  const bool cpu = hr_gpu_check_every() > 0;
  q.gpu = false;
  w.hr_gpu = false;
  if (cpu) return;
  q.last_ok = false;
  w.hr_valid = false;
  w.hrc_valid = false;
  w.hrb_valid = false;
  w.hr_lost = true;
}
// the check (SG_P17_WORLDCHECK): the backend's planes against the CPU's (both kept)
void hr_gpu_compare(const LiveTerrainWorld &w, int64_t &serial, bool hr, bool hrc, bool box, const char *what) {
  const int every = hr_gpu_check_every();
  GfxHrGpu *G = hr_gpu_alive();
  if (every <= 0 || !G || (serial++ % every) != 0) return;
  std::vector<uint16_t> a, c, lo, hi;
  std::vector<uint8_t> b, d, bc;
  if (!G->read(hr ? &a : nullptr, hr ? &b : nullptr, hrc ? &c : nullptr, hrc ? &d : nullptr, box ? &lo : nullptr, box ? &hi : nullptr,
               box ? &bc : nullptr))
    return;
  int64_t bad = 0;
  size_t first = 0;
  auto cmp = [&](const auto &g, const auto &cpu) {
    if (g.size() != cpu.size()) {
      if (!bad) first = 0;
      bad += static_cast<int64_t>(std::max(g.size(), cpu.size()));
      return;
    }
    for (size_t i = 0; i < g.size(); ++i)
      if (g[i] != cpu[i]) {
        if (!bad) first = i;
        ++bad;
      }
  };
  if (hr) {
    cmp(a, w.hr);
    cmp(b, w.hr_cov);
  }
  if (hrc) {
    cmp(c, w.hrc);
    cmp(d, w.hrc_cov);
  }
  if (box) {
    cmp(lo, w.hrb_lo);
    cmp(hi, w.hrb_hi);
    cmp(bc, w.hrb_cov);
  }
  ++g_hr_gpu_st.compared;
  if (bad) {
    if (!g_hr_gpu_st.differing)
      std::fprintf(stderr, "P17 WORLDCHECK: %s: %lld words differ, first at (%d, %d)\n", what, static_cast<long long>(bad),
                   w.pw > 0 ? static_cast<int>(first % static_cast<size_t>(w.pw)) : 0, w.pw > 0 ? static_cast<int>(first / static_cast<size_t>(w.pw)) : 0);
    ++g_hr_gpu_st.differing;
    g_hr_gpu_st.differing_words += bad;
  }
}
// one command as the backend draws it: its op (gpu) and hr_blit_rows' sample tables over the whole box (the expressions below are
// hr_blit_rows' own; a column's / row's values do not depend on the clip rectangle hr_blit_rows is called with). false: the backend
// does not draw this command (an op it does not know, a control mode, an art whose size does not match its payloads)
bool hr_gpu_rec(const LiveTerrainWorld &w, GfxHrGpuOp &op, const Sprite8 &s, const HrGeom &gm, const HrBox &box, GfxHrGpuBatch &b) {
  if (mode() != 0) return false;
  if (s.w < 0 || s.h < 0 || s.px.size() != static_cast<size_t>(s.w) * static_cast<size_t>(s.h)) return false;
  op.art = s.px.data();
  op.art_w = s.w;
  op.art_h = s.h;
  const double rx = gm.rx, ry = gm.ry, fx = gm.fx, fy = gm.fy;
  const double kx = static_cast<double>(w.fw) / 800.0, ky = static_cast<double>(w.ph) / 600.0;
  const int32_t X0 = std::max(box.x0, 0), X1 = std::min(box.x1, w.pw);
  const int32_t Y0 = std::max(box.y0, 0), Y1 = std::min(box.y1, w.ph);
  if (X0 >= X1 || Y0 >= Y1) return true;   // nothing to draw (the caller adds no record)
  const double du = 1.0 / (kx * fx), dv = 1.0 / (ky * fy);
  const int nsx = std::min(4, std::max(1, static_cast<int>(std::ceil(du - 1e-6))));
  const int nsy = std::min(4, std::max(1, static_cast<int>(std::ceil(dv - 1e-6))));
  const int filter = w.hr_filter == 3 ? 1 : w.hr_filter;
  const auto sharp_w = [](double t, double k) {
    const double sv = std::min(1.0, std::max(0.0, (t - 0.5) * k + 0.5));
    return static_cast<uint32_t>(std::floor(sv * 256.0 + 0.5));
  };
  const double ksx = std::max(1.0, nsx / du), ksy = std::max(1.0, nsy / dv);
  GfxHrGpuRec r;
  r.op = static_cast<int32_t>(b.ops.size());
  r.nsx = nsx;
  r.nsy = nsy;
  r.filter = filter;
  const int ns = nsx * nsy;
  r.inv = 1.0 / (65536.0 * ns);
  r.x0 = X0;
  r.y0 = Y0;
  r.x1 = X1;
  r.y1 = Y1;
  r.col_off = static_cast<uint32_t>(b.cols.size() / 4);
  r.row_off = static_cast<uint32_t>(b.rows.size() / 4);
  b.cols.resize(b.cols.size() + static_cast<size_t>(X1 - X0) * static_cast<size_t>(nsx) * 4u);
  int32_t *e = b.cols.data() + static_cast<size_t>(r.col_off) * 4u;
  for (int32_t X = X0; X < X1; ++X)
    for (int si = 0; si < nsx; ++si, e += 4) {
      const double uu = (((X + (si + 0.5) / nsx) - w.ox) / kx + w.scene_e - rx) / fx - 0.5;
      const double uf = std::floor(uu);
      const int32_t i0 = static_cast<int32_t>(uf);
      e[0] = i0 >= 0 && i0 < s.w ? i0 : -1;
      e[1] = i0 + 1 >= 0 && i0 + 1 < s.w ? i0 + 1 : -1;
      e[2] = static_cast<int32_t>(static_cast<uint32_t>(std::floor((uu - uf) * 256.0 + 0.5)));
      e[3] = static_cast<int32_t>(filter ? sharp_w(uu - uf, ksx) : 0u);
    }
  b.rows.resize(b.rows.size() + static_cast<size_t>(Y1 - Y0) * static_cast<size_t>(nsy) * 4u);
  e = b.rows.data() + static_cast<size_t>(r.row_off) * 4u;
  for (int32_t Y = Y0; Y < Y1; ++Y)
    for (int sj = 0; sj < nsy; ++sj, e += 4) {
      const double v = ((Y + (sj + 0.5) / nsy) / ky - ry) / fy - 0.5;
      const double vf = std::floor(v);
      const int32_t j0 = static_cast<int32_t>(vf);
      e[0] = j0 >= 0 && j0 < s.h ? j0 : -1;
      e[1] = j0 + 1 >= 0 && j0 + 1 < s.h ? j0 + 1 : -1;
      e[2] = static_cast<int32_t>(static_cast<uint32_t>(std::floor((v - vf) * 256.0 + 0.5)));
      e[3] = static_cast<int32_t>(filter ? sharp_w(v - vf, ksy) : 0u);
    }
  b.ops.push_back(std::move(op));
  b.recs.push_back(r);
  g_hr_gpu_st.table_entries += static_cast<int64_t>(X1 - X0) * nsx + static_cast<int64_t>(Y1 - Y0) * nsy;
  return true;
}
thread_local GfxHrGpuBatch t_hr_batch;   // reused (its vectors keep their capacity)
}  // namespace (GfxHrJob is declared in gfx.h)
struct GfxHrJob {
  struct Op {  // P15A fix 3: each op's kind (gfx.h GfxWorldOp); a kGwGl op holds its GL stream instead of a function
    std::function<void(LiveTerrainWorld &)> f;
    GfxWorldOp kind = kGwHr;
    std::shared_ptr<gl::GlRec> gl;
    std::shared_ptr<std::vector<GfxWorldLine>> lines;   // P23D fix 2: a kGwLines op's lines (terrain_live.h gfx_world_lines_chunk)
    Op() = default;
    Op(std::function<void(LiveTerrainWorld &)> fn, GfxWorldOp k = kGwHr) : f(std::move(fn)), kind(k) {}
  };
  std::vector<Op> ops;
  std::shared_ptr<const std::vector<uint16_t>> root;   // the world root copied at the last queue start (null: none)
  uint64_t root_gen = 0;
  bool has_start = false;   // a queue start is among the ops (it is the first: hr_job_start drops what came before)
};
namespace {
thread_local std::shared_ptr<GfxHrJob> g_hr_job;   // the frame thread's ops since the last gfx_hr_take_job
GfxHrJob &hr_job() {
  if (!g_hr_job) g_hr_job = std::make_shared<GfxHrJob>();
  return *g_hr_job;
}
// P15A fix 3: an hr op (dropped at a later queue start), not a world surface op
bool hr_kind(GfxWorldOp k) { return k == kGwHr || k == kGwStart; }
// a queue start: the hr ops before it are dropped (gfx.h gfx_hr_job_merge; P15A fix 3: the world ops stay, in their order)
void hr_job_start(std::function<void(LiveTerrainWorld &)> op, std::shared_ptr<const std::vector<uint16_t>> root, uint64_t gen) {
  GfxHrJob &j = hr_job();
  j.ops.erase(std::remove_if(j.ops.begin(), j.ops.end(), [](const GfxHrJob::Op &o) { return hr_kind(o.kind); }), j.ops.end());
  j.ops.emplace_back(std::move(op), kGwStart);
  j.root = std::move(root);
  j.root_gen = gen;
  j.has_start = true;
}

template <class Op>
void hr_blit(LiveTerrainWorld &w, const Sprite8 &s, int32_t x, int32_t y, Scale sc, const Op &op0) {
  const PerfDetailScope pd(kPdHrEnqueue);  // P15A
  const auto t0 = std::chrono::steady_clock::now();
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  if ((hr_recording() ? g_hr_queue.defer_np : g_hr_queue.last_base.size()) != np || w.fw <= 0 || w.ph <= 0) return;   // P15A
  const HrGeom gm = hr_geom(x, y, sc, s);
  if (!gm.ok) return;
  auto c = std::make_unique<HrCmdT<Op>>(op0);
  hr_no_rec(c->op, 0);
  c->gm = gm;
  c->box = hr_box(w, s, gm);
  c->up = hr_art(w, s, c->gm);   // P13D fix 5: the edge filter's scaled art (the box is the unscaled art's: the same rectangle)
  c->art_w0 = s.w;               // P15C
  c->art_h0 = s.h;
  if (!c->up) {
    c->s.w = s.w;
    c->s.h = s.h;
    c->s.px = s.px;
  }
  if (c->op.pal) {
    std::copy(c->op.pal, c->op.pal + 256, c->pal.begin());
    c->op.pal = c->pal.data();
  }
  uint64_t h = 0xcbf29ce484222325ull;
  const uint64_t ext = c->op.sig();
  h = hr_hash(h, &ext, sizeof(ext));
  h = hr_hash(h, &gm.rx, sizeof(gm.rx));
  h = hr_hash(h, &gm.ry, sizeof(gm.ry));
  h = hr_hash(h, &gm.fx, sizeof(gm.fx));
  h = hr_hash(h, &gm.fy, sizeof(gm.fy));
  h = hr_hash(h, &s.w, sizeof(s.w));
  h = hr_hash(h, &s.h, sizeof(s.h));
  h = hr_hash(h, c->pal.data(), sizeof(uint16_t) * c->pal.size());
  h = hr_hash(h, s.px.data(), s.px.size());
  c->sig = h;
  c->src = g_draw_src;  // P12A fix 2 (not in sig: the tile reuse is unchanged)
  if (sc.den != 0) {    // P12A fix 2: sprite_draw's trim (left / top scaled), so the entry's own placement is x - trim_x
    c->trim_x = div32(mul32(s.left, sc.nx), sc.den);
    c->trim_y = div32(mul32(s.top, sc.ny), sc.den);
  }
  if (g_hr_queue.w != &w) g_hr_queue.cmds.clear();
  g_hr_queue.w = &w;
  g_hr_queue.cmds.push_back(std::move(c));
  ++w.hr_draws;
  w.hr_queue_us += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
}
// renderSpriteQueue's start with an hr plane: the base (the world layer's pixels wp) compared with the last queue's row by row; a
// differing row's span is copied into last_base and marks its tiles dirty, as do the boxes of the last frame's draws made at once
// into hr. Nothing kept (the first queue, a new size, SG_P10D_HR_REUSE=0): every tile dirty, the planes made new.
bool hr_reuse_on() {
  static const bool on = [] {
    const char *e = std::getenv("SG_P10D_HR_REUSE");
    return !(e && !std::strcmp(e, "0"));
  }();
  return on;
}
void hr_queue_begin(LiveTerrainWorld &w, const std::vector<uint16_t> &wp) {
  HrQueue &q = g_hr_queue;
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  q.cmds.clear();
  q.w = nullptr;
  q.tx = (w.pw + kHrTile - 1) / kHrTile;
  q.ty = (w.ph + kHrTile - 1) / kHrTile;
  q.dirty.assign(static_cast<size_t>(q.tx) * static_cast<size_t>(q.ty), 0);
  auto mark = [&](const HrBox &b) {
    if (b.empty()) return;
    for (int32_t j = b.y0 / kHrTile; j <= (b.y1 - 1) / kHrTile; ++j)
      for (int32_t i = b.x0 / kHrTile; i <= (b.x1 - 1) / kHrTile; ++i)
        q.dirty[static_cast<size_t>(j) * static_cast<size_t>(q.tx) + static_cast<size_t>(i)] = 1;
  };
  // P17B: the planes in the GPU backend when it wants this size (the CPU's beside them under SG_P17_WORLDCHECK); a change of home is
  // a start without reuse
  GfxHrGpu *G = hr_gpu_alive();
  if (G && (wp.size() != np || !G->want(w.pw, w.ph))) G = nullptr;
  const bool cpu = !G || hr_gpu_check_every() > 0;
  const bool planes = (!cpu || (w.hr.size() == np && w.hr_cov.size() == np)) && (G ? q.gpu && G->planes(w.pw, w.ph) : !q.gpu);
  const bool reuse = hr_reuse_on() && q.last_ok && q.last_w == &w && q.last_pw == w.pw && q.last_ph == w.ph &&
                     q.last_base.size() == np && planes && wp.size() == np && !q.base_stale;
  std::vector<int32_t> spans;   // P17B: the row spans copied into last_base (y, x0, x1)
  if (!reuse) {
    q.last_ok = false;
    q.last_base = wp;
    q.base_stale = false;   // fix 3
    if (cpu) {
      w.hr.assign(np, 0);
      w.hr_cov.assign(np, 0);
    } else {   // P17B: the CPU planes are not kept
      std::vector<uint16_t>().swap(w.hr);
      std::vector<uint8_t>().swap(w.hr_cov);
      std::vector<uint16_t>().swap(w.hrc);
      std::vector<uint8_t>().swap(w.hrc_cov);
      std::vector<uint16_t>().swap(w.hrb_lo);
      std::vector<uint16_t>().swap(w.hrb_hi);
      std::vector<uint8_t>().swap(w.hrb_cov);
    }
    std::fill(q.dirty.begin(), q.dirty.end(), uint8_t(1));
  } else {
    for (int32_t y = 0; y < w.ph; ++y) {
      const uint16_t *a = wp.data() + static_cast<size_t>(y) * static_cast<size_t>(w.pw);
      uint16_t *b = q.last_base.data() + static_cast<size_t>(y) * static_cast<size_t>(w.pw);
      if (!std::memcmp(a, b, sizeof(uint16_t) * static_cast<size_t>(w.pw))) continue;
      int32_t x0 = 0, x1 = w.pw;
      while (x0 < w.pw && a[x0] == b[x0]) ++x0;
      while (x1 > x0 && a[x1 - 1] == b[x1 - 1]) --x1;
      std::copy(a + x0, a + x1, b + x0);
      mark(HrBox{x0, y, x1, y + 1});
      if (G) spans.insert(spans.end(), {y, x0, x1});
    }
    for (const HrBox &b : q.post_boxes) mark(b);
  }
  q.post_boxes.clear();
  if (G) {   // P17B: the backend's planes and base (= last_base)
    ++(reuse ? g_hr_gpu_st.base_spans : g_hr_gpu_st.base_all);
    if ((!reuse && !G->reset(w.pw, w.ph)) || !G->base(w, wp, spans, !reuse)) {
      hr_gpu_failed(w, "queue start");   // a start: the CPU planes are made here (no reuse), nothing is lost
      w.hr_lost = false;
      if (!cpu) {
        w.hr.assign(np, 0);
        w.hr_cov.assign(np, 0);
      }
      q.last_ok = false;
      std::fill(q.dirty.begin(), q.dirty.end(), uint8_t(1));
      return;
    }
    q.gpu = true;
    w.hr_gpu = true;
  } else {
    q.gpu = false;
    w.hr_gpu = false;
  }
}

// P17B fix 3: a queue start whose compare runs on the GPU backend (gfx.h GfxHrGpu::compare): when the planes are there for this size
// and the start reuses them (the conditions of hr_queue_begin's reuse), the root (`root`, or with null the world's raster, read on the
// GPU when its frame is current) is compared there with the backend's base; the dirty tiles are marked from the returned row spans
// exactly as hr_queue_begin marks them, and last_base's words are no longer kept (base_stale: the publication reads its tiles from the
// backend). Under SG_P17_WORLDCHECK the CPU loop runs too (last_base kept) and its spans are compared. Anything else: hr_queue_begin.
void hr_queue_begin_gpu(LiveTerrainWorld &w, const std::vector<uint16_t> *root) {
  static const std::vector<uint16_t> kNone;
  HrQueue &q = g_hr_queue;
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  GfxHrGpu *G = hr_gpu_alive();
  const bool rok = root ? root->size() == np : (w.raster && w.raster->width() == w.pw && w.raster->height() == w.ph);
  const bool check = hr_gpu_check_every() > 0;
  static const bool on = [] {   // development switch SG_P17_GPU_START=0: the compare on the CPU (fix 2's way)
    const char *e = std::getenv("SG_P17_GPU_START");
    return !(e && e[0] == '0');
  }();
  const bool can = on && G && rok && np > 0 && G->want(w.pw, w.ph) && q.gpu && G->planes(w.pw, w.ph) && hr_reuse_on() && q.last_ok &&
                   q.last_w == &w && q.last_pw == w.pw && q.last_ph == w.ph && q.last_base.size() == np &&
                   (!check || (!q.base_stale && w.hr.size() == np && w.hr_cov.size() == np));
  auto cpu_words = [&]() -> const std::vector<uint16_t> & { return root ? *root : w.raster ? w.raster->cpixels() : kNone; };
  if (!can) {
    hr_queue_begin(w, cpu_words());
    return;
  }
  std::vector<int32_t> spans;
  if (G->compare(w, root, spans) != 1) {
    hr_gpu_failed(w, "compare");   // the CPU path from here: hr_queue_begin makes the planes again (no reuse)
    hr_queue_begin(w, cpu_words());
    return;
  }
  q.cmds.clear();
  q.w = nullptr;
  q.tx = (w.pw + kHrTile - 1) / kHrTile;
  q.ty = (w.ph + kHrTile - 1) / kHrTile;
  q.dirty.assign(static_cast<size_t>(q.tx) * static_cast<size_t>(q.ty), 0);
  auto mark = [&](const HrBox &b) {
    if (b.empty()) return;
    for (int32_t j = b.y0 / kHrTile; j <= (b.y1 - 1) / kHrTile; ++j)
      for (int32_t i = b.x0 / kHrTile; i <= (b.x1 - 1) / kHrTile; ++i)
        q.dirty[static_cast<size_t>(j) * static_cast<size_t>(q.tx) + static_cast<size_t>(i)] = 1;
  };
  for (size_t k = 0; k + 2 < spans.size(); k += 3) mark(HrBox{spans[k + 1], spans[k], spans[k + 2], spans[k] + 1});
  ++g_hr_gpu_st.starts_gpu;
  if (check) {   // SG_P17_WORLDCHECK: hr_queue_begin's loop over the CPU words and last_base (kept), its spans against the GPU's
    const std::vector<uint16_t> &wp = cpu_words();
    std::vector<int32_t> cs;
    for (int32_t y = 0; y < w.ph; ++y) {
      const uint16_t *a = wp.data() + static_cast<size_t>(y) * static_cast<size_t>(w.pw);
      uint16_t *b = q.last_base.data() + static_cast<size_t>(y) * static_cast<size_t>(w.pw);
      if (!std::memcmp(a, b, sizeof(uint16_t) * static_cast<size_t>(w.pw))) continue;
      int32_t x0 = 0, x1 = w.pw;
      while (x0 < w.pw && a[x0] == b[x0]) ++x0;
      while (x1 > x0 && a[x1 - 1] == b[x1 - 1]) --x1;
      std::copy(a + x0, a + x1, b + x0);
      cs.insert(cs.end(), {y, x0, x1});
    }
    ++g_hr_gpu_st.start_checks;
    if (cs != spans) {
      if (!g_hr_gpu_st.start_differing)
        std::fprintf(stderr, "P17 WORLDCHECK: queue start: the GPU's %zu row spans differ from the CPU loop's %zu\n", spans.size() / 3, cs.size() / 3);
      ++g_hr_gpu_st.start_differing;
    }
  } else {
    q.base_stale = true;
  }
  for (const HrBox &b : q.post_boxes) mark(b);
  q.post_boxes.clear();
  w.hr_gpu = true;
}

// the queued commands over the hr plane (renderSpriteQueue's end). The plane is the last flush's except in the DIRTY tiles: where
// the terrain base differs from the last queue's, and the boxes of the commands that differ from the last queue's command at the
// same index (both boxes); every tile when nothing is kept (the first queue, a new size). A dirty tile is reset to the base and
// every command whose box meets it is drawn there again in queue order, so the painter's order is kept and no pixel is blended
// twice. Tile rows go to up to 8 threads (row r to thread r mod n, one thread per 24 dirty tiles; SG_P10D_HR_THREADS=<n> sets
// the most, 1 = the frame thread); SG_P10D_HR_REUSE=0 makes every tile dirty (hr_queue_begin).
void hr_flush(LiveTerrainWorld &w) {
  PerfDetailScope pd(kPdHrFlush);  // P15A
  const auto t0 = std::chrono::steady_clock::now();
  struct Done {
    LiveTerrainWorld &w;
    std::chrono::steady_clock::time_point t0;
    ~Done() {
      w.hr_flush_us += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
      ++w.hr_queues;
    }
  } done{w, t0};
  HrQueue &q = g_hr_queue;
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  // P17B: the planes in the GPU backend (q.gpu), the CPU's too under SG_P17_WORLDCHECK
  GfxHrGpu *const G = q.gpu ? hr_gpu_alive() : nullptr;
  const bool cpu = !q.gpu || hr_gpu_check_every() > 0;
  if (q.last_base.size() != np || (cpu && (w.hr.size() != np || w.hr_cov.size() != np)) || (q.gpu && (!G || !G->planes(w.pw, w.ph))) ||
      q.dirty.size() != static_cast<size_t>(q.tx) * static_cast<size_t>(q.ty)) {
    q.cmds.clear();
    q.last_ok = false;
    w.hr_valid = false;
    return;
  }
  if (q.w != &w) q.cmds.clear();
  static const int want = [] {
    const char *e = std::getenv("SG_P10D_HR_THREADS");
    const int n = e ? std::atoi(e) : static_cast<int>(std::thread::hardware_concurrency());
    return std::max(1, std::min(8, n));
  }();
  const int32_t tx = q.tx, ty = q.ty;
  std::vector<uint8_t> &dirty = q.dirty;
  auto mark = [&](const HrBox &b) {
    if (b.empty()) return;
    for (int32_t j = b.y0 / kHrTile; j <= (b.y1 - 1) / kHrTile; ++j)
      for (int32_t i = b.x0 / kHrTile; i <= (b.x1 - 1) / kHrTile; ++i)
        dirty[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i)] = 1;
  };
  if (q.last_ok) {   // the commands that differ from the last queue's command at the same index: both boxes
    const size_t n = std::max(q.cmds.size(), q.last_sig.size());
    for (size_t i = 0; i < n; ++i) {
      const bool have_new = i < q.cmds.size(), have_old = i < q.last_sig.size();
      if (have_new && have_old && q.cmds[i]->sig == q.last_sig[i] && q.cmds[i]->box == q.last_box[i]) continue;
      if (have_new) mark(q.cmds[i]->box);
      if (have_old) mark(q.last_box[i]);
    }
  }
  int64_t nd = 0;
  for (uint8_t d : dirty) nd += d;
  w.hr_tiles += static_cast<int64_t>(dirty.size());
  w.hr_tiles_dirty += nd;
  if (G) {  // P17B: the dirty tiles drawn by the backend: the commands whose box meets one, in queue order
    const int64_t tp = perf_now_ns();
    GfxHrGpuBatch &b = t_hr_batch;
    b.clear();
    bool ok = true;
    for (const auto &c : q.cmds) {
      const HrBox &bx = c->box;
      if (bx.empty()) continue;
      bool meets = false;
      for (int32_t j = bx.y0 / kHrTile; !meets && j <= (bx.y1 - 1) / kHrTile; ++j)
        for (int32_t i = bx.x0 / kHrTile; i <= (bx.x1 - 1) / kHrTile; ++i)
          if (dirty[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i)]) {
            meets = true;
            break;
          }
      if (!meets) continue;
      GfxHrGpuOp op;
      if (!c->gpu(op) || !hr_gpu_rec(w, op, c->art(), c->gm, bx, b)) {
        ok = false;
        break;
      }
    }
    g_hr_gpu_st.prep_ns += perf_now_ns() - tp;
    ++g_hr_gpu_st.flushes;
    g_hr_gpu_st.batch_cmds += static_cast<int64_t>(b.recs.size());
    g_hr_gpu_st.dirty_tiles += nd;
    if (!ok || !G->flush(dirty, tx, ty, b)) {
      hr_gpu_failed(w, ok ? "flush" : "a command the backend does not draw");
      if (!cpu) {
        q.cmds.clear();
        q.w = nullptr;
        return;
      }
    }
  }
  if (!cpu) {
    w.hr_lost = false;
  } else {
  // a dirty tile starts from the base
  for (int32_t j = 0; j < ty; ++j)
    for (int32_t i = 0; i < tx; ++i) {
      if (!dirty[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i)]) continue;
      const int32_t x0 = i * kHrTile, x1 = std::min(w.pw, x0 + kHrTile);
      for (int32_t y = j * kHrTile; y < std::min(w.ph, (j + 1) * kHrTile); ++y) {
        const size_t o = static_cast<size_t>(y) * static_cast<size_t>(w.pw) + static_cast<size_t>(x0);
        std::copy(q.last_base.begin() + static_cast<std::ptrdiff_t>(o),
                  q.last_base.begin() + static_cast<std::ptrdiff_t>(o + static_cast<size_t>(x1 - x0)), w.hr.begin() + static_cast<std::ptrdiff_t>(o));
        std::fill(w.hr_cov.begin() + static_cast<std::ptrdiff_t>(o), w.hr_cov.begin() + static_cast<std::ptrdiff_t>(o + static_cast<size_t>(x1 - x0)),
                  uint8_t(0));
      }
    }
  if (nd > 0 && !q.cmds.empty()) {
    const int nt = std::max(1, std::min(std::min(want, static_cast<int>(ty)), static_cast<int>(std::max<int64_t>(1, nd / 24))));
    uint16_t *plane = w.hr.data();
    uint8_t *cover = w.hr_cov.data();
    auto band = [&](int t) {
      for (const auto &c : q.cmds) {
        const HrBox &b = c->box;
        if (b.empty()) continue;
        for (int32_t j = b.y0 / kHrTile; j <= (b.y1 - 1) / kHrTile; ++j) {
          if (j % nt != t) continue;
          // the runs of dirty tiles of tile row j inside the box (each at most 2 tiles wide: hr_blit_rows' column table)
          int32_t i = b.x0 / kHrTile;
          const int32_t ie = (b.x1 - 1) / kHrTile;
          while (i <= ie) {
            if (!dirty[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(i)]) {
              ++i;
              continue;
            }
            int32_t k = i + 1;
            while (k <= ie && k < i + 2 && dirty[static_cast<size_t>(j) * static_cast<size_t>(tx) + static_cast<size_t>(k)]) ++k;
            c->run(w, plane, cover, i * kHrTile, j * kHrTile, std::min(w.pw, k * kHrTile), std::min(w.ph, (j + 1) * kHrTile));
            i = k;
          }
        }
      }
    };
    if (nt == 1) {
      band(0);
    } else {
      std::vector<std::thread> th;
      th.reserve(static_cast<size_t>(nt - 1));
      for (int t = 1; t < nt; ++t) th.emplace_back(band, t);
#ifdef _WIN32
      if (t_hr_runner) {  // P15A: on the world thread the bands take its (lower) priority
        const int prio = GetThreadPriority(GetCurrentThread());
        if (prio != THREAD_PRIORITY_ERROR_RETURN)
          for (std::thread &x : th) SetThreadPriority(x.native_handle(), prio);
      }
#endif
      band(0);
      for (std::thread &t : th) t.join();
    }
  }
  w.hr_lost = false;
  if (q.gpu) {  // P17B: SG_P17_WORLDCHECK
    static int64_t serial = 0;
    hr_gpu_compare(w, serial, true, false, false, "hr_flush");
  }
  }  // P17B: the CPU planes
  w.hr_valid = true;
  q.last_sig.clear();
  q.last_box.clear();
  for (const auto &c : q.cmds) {
    q.last_sig.push_back(c->sig);
    q.last_box.push_back(c->box);
  }
  q.last_ok = true;
  q.last_w = &w;
  q.last_pw = w.pw;
  q.last_ph = w.ph;
  // P12A fix 2: the queue as the app redraws it between game frames (its base, its draws in order); the draws made at once into
  // hr after the queue's end are added by hr_now until a present takes it (gfx_hr_scene)
  q.pub.reset();
  q.pub_taken = false;
  pd.stop();  // P15A
  if (g_hr_scene_on.load(std::memory_order_relaxed) && !t_hr_selftest) {
    const PerfDetailScope pd_pub(kPdHrPub);  // P15A
    struct PubTime {  // P12A fix 3: the publication's own cost on this thread (gfx_hr_scene_cost)
      std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
      ~PubTime() {
        g_hr_pub_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
        g_hr_pub_n.fetch_add(1);
      }
    } pub_time;
    auto sc = std::make_shared<GfxHrScene>();
    sc->w = std::make_shared<LiveTerrainWorld>();
    sc->w->pw = w.pw;
    sc->w->ph = w.ph;
    sc->w->fw = w.fw;
    sc->w->ox = w.ox;
    sc->w->scene_e = w.scene_e;
    sc->w->hr_filter = w.hr_filter;
    // P12A fix 3: the base only around the tagged draws (no copy of the whole plane on this thread)
    const size_t nb = q.last_base.size();
    sc->base.reset(new uint16_t[nb]);  // uninitialised: only base_rects are written and read
    sc->base_n = nb;
    const double kmax = std::max(static_cast<double>(w.fw) / 800.0, static_cast<double>(w.ph) / 600.0);
    const int32_t grow = static_cast<int32_t>(std::ceil(kGfxMoverMaxStep * kmax)) + 4;
    sc->base_tx = (w.pw + kHrTile - 1) / kHrTile;
    sc->base_ty = (w.ph + kHrTile - 1) / kHrTile;
    sc->base_tile.assign(static_cast<size_t>(sc->base_tx) * static_cast<size_t>(sc->base_ty), 0);
    for (const auto &c : q.cmds) {
      if (c->src.kind == kGfxMoverNone || c->box.empty()) continue;
      const int32_t x0 = std::max(0, c->box.x0 - grow), y0 = std::max(0, c->box.y0 - grow), x1 = std::min(w.pw, c->box.x1 + grow),
                    y1 = std::min(w.ph, c->box.y1 + grow);
      if (x1 <= x0 || y1 <= y0) continue;
      for (int32_t j = y0 / kHrTile; j <= (y1 - 1) / kHrTile; ++j)
        for (int32_t i = x0 / kHrTile; i <= (x1 - 1) / kHrTile; ++i) {
          uint8_t &t = sc->base_tile[static_cast<size_t>(j) * static_cast<size_t>(sc->base_tx) + static_cast<size_t>(i)];
          if (t) continue;
          t = 1;
          if (q.base_stale) continue;   // P17B fix 3: the tiles come from the backend below
          const int32_t tx0 = i * kHrTile, tx1 = std::min(w.pw, tx0 + kHrTile);
          for (int32_t y = j * kHrTile; y < std::min(w.ph, (j + 1) * kHrTile); ++y) {
            const size_t o = static_cast<size_t>(y) * static_cast<size_t>(w.pw) + static_cast<size_t>(tx0);
            std::memcpy(sc->base.get() + o, q.last_base.data() + o, sizeof(uint16_t) * static_cast<size_t>(tx1 - tx0));
          }
        }
    }
    // P17B fix 3: last_base's words not kept: the flagged tiles read from the backend's base (the same words); SG_P17_WORLDCHECK (last_base
    // kept): read too and compared
    GfxHrGpu *const Gp = q.gpu ? hr_gpu_alive() : nullptr;
    bool pub_ok = true;
    if (q.base_stale) {
      pub_ok = Gp && Gp->read_base(sc->base_tile, sc->base_tx, sc->base_ty, sc->base.get());
      if (pub_ok) ++g_hr_gpu_st.pubs_gpu;
    } else if (Gp && hr_gpu_check_every() > 0) {
      std::unique_ptr<uint16_t[]> gb(new uint16_t[nb]);
      if (Gp->read_base(sc->base_tile, sc->base_tx, sc->base_ty, gb.get())) {
        int64_t bad = 0;
        for (int32_t j = 0; j < sc->base_ty; ++j)
          for (int32_t i = 0; i < sc->base_tx; ++i) {
            if (!sc->base_tile[static_cast<size_t>(j) * static_cast<size_t>(sc->base_tx) + static_cast<size_t>(i)]) continue;
            const int32_t tx0 = i * kHrTile, tx1 = std::min(w.pw, tx0 + kHrTile);
            for (int32_t y = j * kHrTile; y < std::min(w.ph, (j + 1) * kHrTile); ++y)
              for (int32_t x = tx0; x < tx1; ++x) {
                const size_t o = static_cast<size_t>(y) * static_cast<size_t>(w.pw) + static_cast<size_t>(x);
                bad += gb[o] != sc->base[o] ? 1 : 0;
              }
          }
        ++g_hr_gpu_st.pub_checks;
        if (bad) {
          if (!g_hr_gpu_st.pub_differing) std::fprintf(stderr, "P17 WORLDCHECK: publication: %lld base words differ\n", static_cast<long long>(bad));
          ++g_hr_gpu_st.pub_differing;
        }
      }
    }
    if (!pub_ok) {   // no base for the published scene: no publication (the window draws this frame's objects where the frame has them)
      hr_gpu_failed(w, "base tiles");
      sc.reset();
    }
    if (sc) {
    sc->cmds.reserve(q.cmds.size());
    sc->draws.reserve(q.cmds.size());
    for (const auto &c : q.cmds) {
      sc->cmds.push_back(c);
      GfxHrDraw d;
      d.src = c->src;
      d.box[0] = c->box.x0;
      d.box[1] = c->box.y0;
      d.box[2] = c->box.x1;
      d.box[3] = c->box.y1;
      d.art_w = c->art_w0;  // P15C: c->s.w is 0 where the edge filter's art is used
      d.art_h = c->art_h0;
      d.rx = c->gm.rx;
      d.ry = c->gm.ry;
      d.ex = c->gm.rx - c->trim_x;
      d.ey = c->gm.ry - c->trim_y;
      sc->draws.push_back(d);
    }
    q.pub = std::move(sc);
    }  // P17B fix 3 (sc)
  }
  q.cmds.clear();
  q.w = nullptr;
}

template <class Op>
void hr_now(LiveTerrainWorld &w, const Sprite8 &s, int32_t x, int32_t y, Scale sc, const Op &op, bool cur);
template <class Op>
void hr_box_now(LiveTerrainWorld &w, const Sprite8 &s, int32_t x, int32_t y, Scale sc, const Op &op);
// P15A: a post-queue draw recorded with copies of its art, its op and the op's palette (composeGolferSprite rewrites g_images[0]'s
// words between two golfer entries, as hr_blit's HrCmd copy notes); the op's other pointers are static tables
template <class Op>
struct HrNowRec {
  Sprite8 s;
  Op op;
  std::array<uint16_t, 256> pal{};
  int32_t x = 0, y = 0;
  Scale sc{};
  bool cur = false, box = false;
  HrGeo geo;
  explicit HrNowRec(const Op &o) : op(o) {}
};
template <class Op>
void hr_record_now(const LiveTerrainWorld &w, const Sprite8 &s, int32_t x, int32_t y, Scale sc, const Op &op, bool cur, bool box) {
  auto r = std::make_shared<HrNowRec<Op>>(op);
  r->s = s;
  if (r->op.pal) {
    std::copy(r->op.pal, r->op.pal + 256, r->pal.begin());
    r->op.pal = r->pal.data();
  }
  r->x = x;
  r->y = y;
  r->sc = sc;
  r->cur = cur;
  r->box = box;
  r->geo = hr_geo(w);
  hr_job().ops.emplace_back([r](LiveTerrainWorld &W) {
    r->geo.put(W);
    if (r->box) hr_box_now(W, r->s, r->x, r->y, r->sc, r->op);
    else hr_now(W, r->s, r->x, r->y, r->sc, r->op, r->cur);
  });
}

// P10D fix 2 (P10F: also the placement box, draw_alpha): a draw after the queue's end, at once on the calling thread, at scene
// position (x, y): with `cur` into hrc (a copy of hr made at the first such draw, hrc_cov = 1 where it drew), else into hr
template <class Op>
void hr_now(LiveTerrainWorld &w, const Sprite8 &s, int32_t x, int32_t y, Scale sc, const Op &op, bool cur) {
  const PerfDetailScope pd(kPdHrNow);  // P15A
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  if (hr_recording()) {  // P15A: the flags this draw sets, then the draw recorded (own art, op and palette) for the world thread
    if (!w.hr_valid || g_hr_queue.defer_np != np || w.fw <= 0) return;
    if (!hr_checking()) {
      if (cur) w.hrc_valid = true;
      hr_record_now(w, s, x, y, sc, op, cur, false);
      return;
    }
    hr_record_now(w, s, x, y, sc, op, cur, false);   // the check: recorded, then run here too
  }
  // P17B: the planes in the GPU backend (the CPU's too under SG_P17_WORLDCHECK)
  const bool gq = g_hr_queue.gpu;
  GfxHrGpu *const G = gq ? hr_gpu_alive() : nullptr;
  const bool cpu = !gq || hr_gpu_check_every() > 0;
  if (!w.hr_valid || (cpu && (w.hr.size() != np || w.hr_cov.size() != np)) || (gq && (!G || !G->planes(w.pw, w.ph))) || w.fw <= 0) return;
  if (cur && !w.hrc_valid) {
    if (cpu) {
      w.hrc = w.hr;
      w.hrc_cov.assign(np, 0);
    }
    if (G && !G->make_hrc()) {
      hr_gpu_failed(w, "hrc");
      if (!cpu) return;
    }
    w.hrc_valid = true;
  }
  const HrGeom gm = hr_geom(x, y, sc, s);
  if (!gm.ok) return;
  Op local = op;
  hr_no_rec(local, 0);
  const HrPass keep = g_hr_pass;
  g_hr_pass.tag = nullptr;   // the planes' words are not the scene's
  const HrBox box = hr_box(w, s, gm);
  HrGeom ga = gm;   // P13D fix 5: the edge filter's scaled art
  const std::shared_ptr<const Sprite8> up = hr_art(w, s, ga);
  const Sprite8 &a = up ? *up : s;
  if (cpu) hr_blit_rows(w, cur ? w.hrc.data() : w.hr.data(), cur ? w.hrc_cov.data() : w.hr_cov.data(), a, ga, local, box, 0, 0, w.pw, w.ph);
  if (G && g_hr_queue.gpu && !box.empty()) {  // P17B: the same draw by the backend
    const int64_t tp = perf_now_ns();
    GfxHrGpuBatch &b = t_hr_batch;
    b.clear();
    GfxHrGpuOp gop;
    const bool ok = hr_gpu_op(local, gop) && hr_gpu_rec(w, gop, a, ga, box, b);
    g_hr_gpu_st.prep_ns += perf_now_ns() - tp;
    ++g_hr_gpu_st.nows;
    if (!ok || (!b.recs.empty() && !G->now(cur ? kHrPlaneHrc : kHrPlaneHr, b))) {
      hr_gpu_failed(w, ok ? "now" : "a draw the backend does not draw");
      if (!cpu) {
        g_hr_pass = keep;
        return;
      }
    } else if (cpu) {
      static int64_t serial = 0;
      hr_gpu_compare(w, serial, !cur, cur, false, "hr_now");
    }
  }
  if (!cur) g_hr_queue.post_boxes.push_back(box);   // fix 3: the next queue draws these tiles again
  g_hr_pass = keep;
  ++w.hr_draws;
  HrQueue &q = g_hr_queue;
  if (!cur && q.pub && !q.pub_taken) {  // P12A fix 2: the published scene redraws it too (in this order, over the queue's draws)
    auto c = std::make_shared<HrCmdT<Op>>(op);
    hr_no_rec(c->op, 0);
    c->s.w = s.w;
    c->s.h = s.h;
    c->s.px = s.px;
    c->gm = ga;   // P15C: as drawn above (the edge filter's art and its divided texel size; ga = gm without it)
    c->up = up;
    c->art_w0 = s.w;
    c->art_h0 = s.h;
    c->box = box;
    if (c->op.pal) {
      std::copy(c->op.pal, c->op.pal + 256, c->pal.begin());
      c->op.pal = c->pal.data();
    }
    GfxHrDraw d;
    d.box[0] = box.x0;
    d.box[1] = box.y0;
    d.box[2] = box.x1;
    d.box[3] = box.y1;
    d.art_w = s.w;
    d.art_h = s.h;
    d.rx = gm.rx;
    d.ry = gm.ry;
    d.ex = gm.rx;
    d.ey = gm.ry;
    q.pub->cmds.push_back(std::move(c));
    q.pub->draws.push_back(d);
  }
}

// P10F (terrain_live.h LiveTerrainWorld::hrb_*): P7's placement box at scene position (x, y), at once on the calling thread, into
// hrc (covering hrb_cov, not hrc_cov) and into the blend pair hrb_lo / hrb_hi (the box over 0 and over 0x7fff words)
template <class Op>
void hr_box_now(LiveTerrainWorld &w, const Sprite8 &s, int32_t x, int32_t y, Scale sc, const Op &op) {
  const PerfDetailScope pd(kPdHrNow);  // P15A
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  if (hr_recording()) {  // P15A (as hr_now above)
    if (!w.hr_valid || g_hr_queue.defer_np != np || w.fw <= 0) return;
    if (!hr_checking()) {
      w.hrc_valid = true;
      w.hrb_valid = true;
      hr_record_now(w, s, x, y, sc, op, false, true);
      return;
    }
    hr_record_now(w, s, x, y, sc, op, false, true);
  }
  // P17B: the planes in the GPU backend (the CPU's too under SG_P17_WORLDCHECK)
  const bool gq = g_hr_queue.gpu;
  GfxHrGpu *const G = gq ? hr_gpu_alive() : nullptr;
  const bool cpu = !gq || hr_gpu_check_every() > 0;
  if (!w.hr_valid || (cpu && (w.hr.size() != np || w.hr_cov.size() != np)) || (gq && (!G || !G->planes(w.pw, w.ph))) || w.fw <= 0) return;
  if (!w.hrc_valid) {
    if (cpu) {
      w.hrc = w.hr;
      w.hrc_cov.assign(np, 0);
    }
    if (G && !G->make_hrc()) {
      hr_gpu_failed(w, "hrc");
      if (!cpu) return;
    }
    w.hrc_valid = true;
  }
  if (!w.hrb_valid && G && g_hr_queue.gpu) {  // P17B: the backend's box planes (made, or the last boxes' rectangle reset)
    if (!G->reset_box(false, w.hrb_area)) {
      hr_gpu_failed(w, "box planes");
      if (!cpu) return;
    }
    if (!cpu) {
      w.hrb_area[0] = w.hrb_area[1] = w.hrb_area[2] = w.hrb_area[3] = 0;
      w.hrb_valid = true;
    }
  }
  if (!w.hrb_valid) {
    if (w.hrb_lo.size() != np || w.hrb_hi.size() != np || w.hrb_cov.size() != np) {
      w.hrb_lo.assign(np, 0);
      w.hrb_hi.assign(np, 0x7fff);
      w.hrb_cov.assign(np, 0);
    } else {
      for (int32_t Y = w.hrb_area[1]; Y < w.hrb_area[3]; ++Y) {   // the last boxes' rectangle only
        const size_t r = static_cast<size_t>(Y) * static_cast<size_t>(w.pw);
        const size_t a = r + static_cast<size_t>(w.hrb_area[0]), b = r + static_cast<size_t>(w.hrb_area[2]);
        std::fill(w.hrb_lo.begin() + static_cast<std::ptrdiff_t>(a), w.hrb_lo.begin() + static_cast<std::ptrdiff_t>(b), uint16_t(0));
        std::fill(w.hrb_hi.begin() + static_cast<std::ptrdiff_t>(a), w.hrb_hi.begin() + static_cast<std::ptrdiff_t>(b), uint16_t(0x7fff));
        std::fill(w.hrb_cov.begin() + static_cast<std::ptrdiff_t>(a), w.hrb_cov.begin() + static_cast<std::ptrdiff_t>(b), uint8_t(0));
      }
    }
    w.hrb_area[0] = w.hrb_area[1] = w.hrb_area[2] = w.hrb_area[3] = 0;
    w.hrb_valid = true;
  }
  const HrGeom gm = hr_geom(x, y, sc, s);
  if (!gm.ok) return;
  const HrBox box = hr_box(w, s, gm);
  if (box.empty()) return;
  const HrPass keep = g_hr_pass;
  g_hr_pass.tag = nullptr;   // the planes' words are not the scene's
  HrGeom ga = gm;   // P13D fix 5: the edge filter's scaled art
  const std::shared_ptr<const Sprite8> up = hr_art(w, s, ga);
  const Sprite8 &a = up ? *up : s;
  if (cpu)
    for (uint16_t *plane : {w.hrc.data(), w.hrb_lo.data(), w.hrb_hi.data()}) {
      Op local = op;
      hr_no_rec(local, 0);
      hr_blit_rows(w, plane, w.hrb_cov.data(), a, ga, local, box, 0, 0, w.pw, w.ph);
    }
  if (G && g_hr_queue.gpu) {  // P17B: the same three draws by the backend
    const int64_t tp = perf_now_ns();
    GfxHrGpuBatch &b = t_hr_batch;
    b.clear();
    Op local = op;
    hr_no_rec(local, 0);
    GfxHrGpuOp gop;
    const bool ok = hr_gpu_op(local, gop) && hr_gpu_rec(w, gop, a, ga, box, b);
    g_hr_gpu_st.prep_ns += perf_now_ns() - tp;
    ++g_hr_gpu_st.nows;
    if (!ok || (!b.recs.empty() && !G->now(kHrPlaneBox, b))) {
      hr_gpu_failed(w, ok ? "box" : "a box the backend does not draw");
      if (!cpu) {
        g_hr_pass = keep;
        return;
      }
    } else if (cpu) {
      static int64_t serial = 0;
      hr_gpu_compare(w, serial, false, true, true, "hr_box_now");
    }
  }
  g_hr_pass = keep;
  int32_t *A = w.hrb_area;
  if (A[0] >= A[2] || A[1] >= A[3]) {
    A[0] = box.x0, A[1] = box.y0, A[2] = box.x1, A[3] = box.y1;
  } else {
    A[0] = std::min(A[0], box.x0), A[1] = std::min(A[1], box.y0), A[2] = std::max(A[2], box.x1), A[3] = std::max(A[3], box.y1);
  }
  ++w.hr_draws;
}

template <class Op>
void blit16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y, Scale sc, const char *who, Op &op) {
  blit16_core(g, s, d, x, y, sc, who, op);
  if (g_hr_pass.hr && &d == &g.root) {
    if (g_hr_pass.plane == 0) {
      hr_blit(*g_hr_pass.hr, s, x, y, sc, op);
      return;
    }
    // P10D fix 2: a draw after the queue's end, at once on this thread (the cursor passes draw a few entries)
    hr_now(*g_hr_pass.hr, s, x, y, sc, op, g_hr_pass.plane == 2);
  }
}

// draw16_100298b0 (Sprite::blt16a's body)
struct PlaneOp {
  Gfx &g;
  const uint16_t *pal;
  bool bad = false;
  uint64_t sig() const { return 1; }   // P10D fix 3: the op's parameters besides its palette
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
  uint64_t sig() const { return 2 ^ (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(lut)) << 4) ^ (static_cast<uint64_t>(lut_n) << 40); }
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
  uint64_t sig() const { return 3 ^ (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(prog)) << 4) ^ (flag ? 1ull << 62 : 0); }
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
// P10F: an op that also marks (mark[i] = 1) every root pixel i the wrapped op writes (AlphaOp's own skips: payload 0xff, and
// 0xf8..0xfe with the flag)
template <class Op>
struct MarkOp {
  Op &in;
  uint8_t *mark;
  uint16_t *word;   // the word the op left there
  const uint16_t *base;
  size_t n;
  bool flag;
  void operator()(uint8_t c, uint16_t &w) {
    in(c, w);
    if (c >= 0xff || (flag && c >= 0xf8)) return;
    const size_t i = static_cast<size_t>(&w - base);
    if (i >= n) return;
    mark[i] = 1;
    word[i] = w;
  }
};
LiveTerrainWorld *wide_world(Ctx &ctx);   // G32 (below)

// draw16_10024140 (Sprite::blt16c's body): payload 0xf8..0xff transparent; the palette word P reduced to one brightness
// byte k = (((P & 0x7c00) >> 7) + ((P & 0x3e0) >> 1) + ((P & 0x1f) << 3)) >> 2 (16-bit adds), and each 5-bit field f of the
// caller's colour C stored as (f * k) >> 8 (mul dl: an 8 x 8 product)
struct ColourOp {
  const uint16_t *pal;
  uint16_t colour;
  uint64_t sig() const { return 4 ^ (static_cast<uint64_t>(colour) << 8); }
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
  uint64_t sig() const { return 5; }
  void operator()(uint8_t c, uint16_t &w) {
    if (c >= 0xfe) return;
    put(w, pal[c]);
  }
};

// P15C (gfx.h GfxHrGpuOp): the ops' parameters for the app's GPU path; each body above is transcribed there
bool hr_gpu_op(const PlaneOp &o, GfxHrGpuOp &r) {
  r.kind = 1;
  r.pal = o.pal;
  r.plane_k = mode() == 1 ? 1u : 0u;
  return o.pal != nullptr;
}
bool hr_gpu_op(const ShadeOp &o, GfxHrGpuOp &r) {
  r.kind = 2;
  r.pal = o.pal;
  r.lut = o.lut;
  r.lut_n = static_cast<uint32_t>(o.lut_n);
  return o.pal != nullptr && (o.lut != nullptr || o.lut_n == 0) && o.lut_n <= (1u << 20);
}
bool hr_gpu_op(const AlphaOp &o, GfxHrGpuOp &r) {
  if (!o.pal || !o.prog || o.rec) return false;
  r.kind = 3;
  r.pal = o.pal;
  r.flag = o.flag ? 1u : 0u;
  r.steps.clear();
  for (size_t k = 0; k < o.prog->n; ++k) {
    const AStep &a = o.prog->steps[k];
    r.steps.insert(r.steps.end(), {a.op, a.dst, a.kind, a.src});
  }
  return true;
}
bool hr_gpu_op(const ColourOp &o, GfxHrGpuOp &r) {
  r.kind = 4;
  r.pal = o.pal;
  r.colour = o.colour;
  return o.pal != nullptr;
}
bool hr_gpu_op(const DrawBOp &o, GfxHrGpuOp &r) {
  r.kind = 5;
  r.pal = o.pal;
  return o.pal != nullptr;
}

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
  // P11E: Sprite::drawAlpha adds the sprite's offsets (this +0x20 / +0x24) unscaled, where every other body here takes them at the
  // display scale in force (trim). At scale 1 the two agree, and the original draws its translucent previews (the hole pin, the
  // tree and object previews: queue flag 0x200, mainLoop ML+0xb5ba / ML+0xb8da / ML+0xab98..ML+0xae87) only at zoom 4, where the
  // sprite queue leaves scale 1. At g_zoomScale 8 (reached only with mainLoop.zoomInStopsAt4 = 0) the scale is 2 and the unscaled
  // offsets put those sprites off their tile (a pin 37 px left of its green), so there the offsets are taken at the scale too.
  // Zooms 1, 2 and 4 keep the original's rule. P13A: with mainLoop.toolForcesZoom4 = 0 the previews are drawn at zooms 1 and 2
  // too (a tool is held at every zoom), at scale 1/2 and 1/4, so there the offsets are taken at the scale as well.
  const int32_t zs = ctx.gs().g_zoomScale;
  const bool z8 = zs == 8 || (zs != 4 && !SG_DEFECT(g_defect_tool_zoom4));
  const Scale sc0 = cur_scale(g);
  const int32_t ox = z8 ? trim(s->left, sc0.nx, sc0.den) : s->left;
  const int32_t oy = z8 ? trim(s->top, sc0.ny, sc0.den) : s->top;
  // the member of the window's pair: Sprite::drawAlpha sets its flag byte to 1 and clears it only when the destination's
  // format word +4 is 1, and calls the first member of each pair with the flag set (roadmap S2.13 read the arms; the quarter
  // band's first member draw16_1002ed60 is the one S2.8 measured on the reference frames). The only destination here is the
  // root surface, drawn with the flag-set members [15, 30); mode 9 takes the flag-cleared member instead.
  const int member = mode() == 9 ? 0 : 15;
  AlphaOp op{pal, &kAlphaPrograms[body + member], static_cast<uint8_t>(flag) != 0};
  // P9D: the quarter member's pixels recorded for the modal capture (gfx_reports.cpp records the same blend at scale 1)
  const bool quarter = !std::strcmp(op.prog->body, "draw16_1002ed60") && mode() != 7;
  if (quarter && g.hud.any && g.hud.modal && dst == g.root_va(ctx)) op.rec = &g;
  // P10F: a draw of a cursor pass kept apart onto the 800 root while the hr plane holds the queue's sprites (P7's translucent
  // placement box at the cursor tile, mainLoop ML+0xb53b / ML+0xb6ae at alpha 0.5): the same art also at the window's resolution
  // (hr_box_now: hrc and the blend pair hrb_lo / hrb_hi; scene position = root position + scene_e), and the root pixels it wrote
  // marked in box800, so the cursor previews (world_cursor's mask800) take them out of the frame's own pixels. Outside a scene
  // draw only (on_wide swaps the scene into the root: scene is then empty).
  LiveTerrainWorld *bw = nullptr;
  static const bool box_on = [] {  // development: SG_P10F_BOX=0 = the frame's 800-space box (P10D)
    const char *e = std::getenv("SG_P10F_BOX");
    return !(e && e[0] == '0');
  }();
  if (box_on && !g_hr_pass.hr && g.hud.cursor_sprites && g.hud.cursor_depth > 0 && dst == g.root_va(ctx) && g.root.w == 800 &&
      g.root.h == 600 && mode() == 0)
    if (LiveTerrainWorld *w = wide_world(ctx))
      if (!w->hr_open && w->hr_valid && w->scene.size() == static_cast<size_t>(w->scene_w) * 600) bw = w;
  if (!bw) {
    blit16(g, *s, g.root, add32(ox, x), add32(oy, y), cur_scale(g), who, op);
  } else {
    if (bw->box800.size() != g.root.px.size()) bw->box800.assign(g.root.px.size(), 0);
    if (bw->box_word.size() != g.root.px.size()) bw->box_word.assign(g.root.px.size(), 0);
    MarkOp<AlphaOp> mo{op, bw->box800.data(), bw->box_word.data(), g.root.px.data(), g.root.px.size(), op.flag};
    blit16_core(g, *s, g.root, add32(ox, x), add32(oy, y), cur_scale(g), who, mo);   // blit16 without an hr pass
    hr_box_now(*bw, *s, add32(add32(ox, x), bw->scene_e), add32(oy, y), cur_scale(g), op);
  }
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

// P10G: the path goes through flic::resolve_palette_name, so at flic.treeAliasNames = 0 the tropical palettes the exe names
// with spaces (PalCerc, PalDrac, paltallpalm) are read from the install's underscore folders; before, those five palette
// objects stayed empty and every palm / tropical tree drawn with them was skipped as a sprite with no palette
void pal_load(Gfx &g, Ctx &ctx, const PalLoad &l) {
  const uint32_t base = ctx.mem.data_base();
  Image8 img;
  const std::string ext = gfxb::vstring(ctx.mem, state_va(ctx, &GameState::f_strpool_4e4218) + 0x1c);   // ".pcx"
  const std::string name = flic::resolve_palette_name(ctx.files(), gfxb::vstring(ctx.mem, base + l.path), ext);
  if (gfxb::load_image_named(g, ctx, name, base + l.pal, 0, 0x100, 0, img) != 0) return;
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
// P23D fix 6 (gfx.h gfx_set_wide_ground): the frame's ground marks kept since the last queue, queued into hr before any queue entry
// (DrawBOp over a one-colour art of each rectangle at scale 1, no mover source), the root pixels still holding their word marked
std::atomic<uint64_t> g_ground_queued{0}, g_ground_past{0};   // P23D fix 6: rectangles queued into hr, of them past the root's edge
void ground_flush(Gfx &g, LiveTerrainWorld &w) {
  static std::FILE *glog = std::getenv("SG_P23_GROUND_LOG") ? std::fopen(std::getenv("SG_P23_GROUND_LOG"), "a") : nullptr;   // development
  static uint32_t queue_n = 0;
  ++queue_n;
  std::vector<Gfx::HudCapture::GroundRect> list;
  list.swap(g.hud.ground);
  if (list.empty() || !gfx_wide_ground() || !w.hr_open || w.scene_w <= 0 || mode() != 0) return;
  if (w.bub800.size() != g.root.px.size()) w.bub800.assign(g.root.px.size(), 0);
  if (w.bub_word.size() != g.root.px.size()) w.bub_word.assign(g.root.px.size(), 0);
  const GfxMoverSrc keep = g_draw_src;
  g_draw_src = GfxMoverSrc{};
  for (const Gfx::HudCapture::GroundRect &r : list) {
    if (r.x1 <= r.x0 || r.y1 <= r.y0) continue;
    for (int32_t y = std::max(r.y0, 0); y < std::min(r.y1, 600); ++y)
      for (int32_t x = std::max(r.x0, 0); x < std::min(r.x1, 800); ++x) {
        const size_t i = static_cast<size_t>(y) * 800 + static_cast<size_t>(x);
        if (g.root.px[i] != r.word) continue;   // covered since by a later mark of another colour
        w.bub800[i] = 1;
        w.bub_word[i] = r.word;
      }
    Sprite8 s;
    s.w = r.x1 - r.x0;
    s.h = r.y1 - r.y0;
    s.px.assign(static_cast<size_t>(s.w) * static_cast<size_t>(s.h), 0);
    s.pal.fill(0);
    s.pal[0] = r.word;
    DrawBOp op{s.pal.data()};
    hr_blit(w, s, add32(r.x0, w.scene_e), r.y0, Scale{1, 1, 1}, op);
    g_ground_queued.fetch_add(1, std::memory_order_relaxed);
    const bool past = r.x0 < 0 || r.x1 > 800;
    if (past) g_ground_past.fetch_add(1, std::memory_order_relaxed);
    if (glog) {   // every rectangle; the last column is 1 past the root's edge
      std::fprintf(glog, "%u %d %d %d %d %04x %d\n", queue_n, r.x0, r.y0, r.x1, r.y1, r.word, past ? 1 : 0);
      std::fflush(glog);
    }
  }
  g_draw_src = keep;
}
// renderSpriteQueue's start: the wide surface's base
void b_queue_begin(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g_queue_walk = true;  // P22C item 6
  g_deferred_labels.clear();
  g.ensure_root(ctx);
  g.hud.cursor2d.clear();  // P9E fix 4: the cursor passes come after the queue
  g.hud.lines.clear();     // P10I fix 1: so do their tee lines
  g.text.edge_runs.clear();  // P22E item 7: and their texts past the root's edge
  g.hud.wide_e = 0;          // P23F fix 2
  g.hud.wide_in = 0;         // P23F fix 3
  g.hud.line800.clear();
  g.hud.line_word.clear();
  LiveTerrain *lt = has_terrain_api(ctx) ? dynamic_cast<LiveTerrain *>(&terrain_api(ctx)) : nullptr;
  LiveTerrainWorld *w = lt ? lt->world() : nullptr;
  if (w) overlay::grid_queue_begin(*w);  // P23D fix 1: the grid lines' frame settled and kept as the next one's reference
  if (w) w->lines_open = false;  // P13D item 7: the overlay lines drawn before the queue went on the world layer too
  if (!w || !w->valid || g.root.w != 800 || g.root.h != 600) {
    if (w) w->scene_w = 0;
    g.hud.ground.clear();  // P23D fix 6
    return;
  }
  const int32_t E = w->ox > 0 ? static_cast<int32_t>((static_cast<int64_t>(w->ox) * 800 + w->fw - 1) / w->fw) : 0;
  const int32_t W = 800 + 2 * E;
  w->scene_e = E;
  w->scene_w = W;
  g.hud.wide_e = E;  // P23F fix 2
  g.hud.wide_in = w->ox > 0 ? static_cast<int32_t>(static_cast<int64_t>(w->ox) * 800 / w->fw) : 0;  // P23F fix 3
  const PerfDetailScope pd(kPdQueueBegin);  // P15A
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
  if (w->sample) {  // P15A fix 3, the check (SG_P15_CHECK): the masked raster's samples against the full raster's
    const std::vector<uint16_t> &sp = w->sample->pixels();
    int64_t d = 0;
    if (sp.size() == wp.size()) {
      for (int32_t y = 0; y < 600; ++y) {
        int32_t wy = static_cast<int32_t>(static_cast<int64_t>(y) * w->ph / 600);
        if (wy >= w->ph) wy = w->ph - 1;
        for (int32_t x = 0; x < W; ++x) {
          const int32_t fx = x - E;
          if (fx >= 0 && fx < 800) continue;
          int64_t wx = w->ox + static_cast<int64_t>(fx) * w->fw / 800;
          if (wx < 0) wx = 0;
          if (wx >= w->pw) wx = w->pw - 1;
          const size_t k = static_cast<size_t>(wy) * static_cast<size_t>(w->pw) + static_cast<size_t>(wx);
          if (sp[k] != wp[k]) ++d;
        }
      }
    } else {
      d = 1;
    }
    ++w->mask_checks;
    if (d) ++w->mask_differing;
    w->mask_px_differing += d;
  }
  w->scene = w->scene_base;
  w->s0 = g.root.px;
  w->scene_pre.clear();  // P9E fix 4: the cursor passes come after the queue
  w->scene_cur.clear();
  w->scene_text.clear();
  gfxb::text_wide_begin(g);  // P9C
  // P10D fix 1: the hr plane starts from the world layer's pixels (the wide scene's base outside the frame's columns), no tag
  const auto hr_t0 = std::chrono::steady_clock::now();
  w->hr_valid = false;
  w->hrc_valid = false;  // P10D fix 2
  w->box800.clear();     // P10F
  w->bub800.clear();     // P23D fix 3
  w->hrb_valid = false;  // P10F
  g_hr_queue.cmds.clear();
  g_hr_queue.w = nullptr;
  w->hr_open = w->hr_on && mode() == 0 && wp.size() == static_cast<size_t>(w->pw) * static_cast<size_t>(w->ph);
  if (hr_recording()) {  // P15A: the start recorded (with a copy of the world root), the planes are the world thread's
    const HrGeo geo = hr_geo(*w);
    if (w->hr_open && w->rec) {
      // P15A fix 3: the world thread's own raster is the root at this op (the frame thread's may be masked): no copy
      g_hr_queue.defer_np = wp.size();
      hr_job_start([geo](LiveTerrainWorld &W) {
        geo.put(W);
        W.hr_valid = false;
        W.hrc_valid = false;
        W.hrb_valid = false;
        g_hr_queue.cmds.clear();
        g_hr_queue.w = nullptr;
        static const std::vector<uint16_t> kNone;
        (void)kNone;
        hr_queue_begin_gpu(W, nullptr);   // P17B fix 3: the compare on the GPU when the planes are there (else hr_queue_begin over cpixels)
      }, nullptr, 0);
      w->tag.assign(w->scene.size(), 0);
    } else if (w->hr_open) {
      // P15A fix 2: the last copy again while the root has not changed since (LiveTerrainWorld::gen)
      static thread_local std::shared_ptr<const std::vector<uint16_t>> last_root;
      static thread_local uint64_t last_gen = ~0ull;
      static thread_local const LiveTerrainWorld *last_w = nullptr;
      if (w->back && w->back_gen == w->gen && w->back->size() == wp.size())
        last_root = w->back;   // the root equals back: no copy
      else if (!last_root || last_gen != w->gen || last_w != w || last_root->size() != wp.size())
        last_root = std::make_shared<const std::vector<uint16_t>>(wp);
      last_gen = w->gen;
      last_w = w;
      auto root = last_root;
      g_hr_queue.defer_np = wp.size();
      hr_job_start([geo, root](LiveTerrainWorld &W) {
        geo.put(W);
        W.hr_valid = false;
        W.hrc_valid = false;
        W.hrb_valid = false;
        g_hr_queue.cmds.clear();
        g_hr_queue.w = nullptr;
        hr_queue_begin(W, *root);
      }, root, w->gen);
      w->tag.assign(w->scene.size(), 0);
    } else {
      g_hr_queue.defer_np = 0;
      hr_job_start([geo](LiveTerrainWorld &W) {
        geo.put(W);
        W.hr_valid = false;
        W.hrc_valid = false;
        W.hrb_valid = false;
        g_hr_queue.cmds.clear();
        g_hr_queue.w = nullptr;
        g_hr_queue.last_ok = false;
        W.hr.clear();
        W.hr_cov.clear();
      }, nullptr, 0);
      w->tag.clear();
    }
  }
  if (!hr_recording() || hr_checking()) {  // P15A: run here (as before), unless only recorded
    if (w->hr_open) {
      hr_queue_begin(*w, wp);   // fix 3: the tiles to draw again
      w->tag.assign(w->scene.size(), 0);
    } else {
      g_hr_queue.last_ok = false;
      w->hr.clear();
      w->hr_cov.clear();
      w->tag.clear();
    }
  }
  w->tag_pre.clear();
  w->hr_prep_us += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - hr_t0).count();
  ground_flush(g, *w);  // P23D fix 6: the ground marks drawn before the queue, as its first hr commands
}
// P10D fix 1: f() draws on the wide scene (inside on_wide) with every word it writes tagged `val`; with `hr` the blitter
// skeleton also draws into that world's hr plane
template <class F>
void tagged(Gfx &g, LiveTerrainWorld &w, uint8_t val, bool hr, F f, int plane = 0) {
  if (w.tag.size() != g.root.px.size()) {
    f();
    return;
  }
  const HrPass keep = g_hr_pass;
  g_hr_pass.base = reinterpret_cast<uintptr_t>(g.root.px.data());
  g_hr_pass.n = g.root.px.size();
  g_hr_pass.tag = w.tag.data();
  g_hr_pass.val = val;
  g_hr_pass.hr = hr ? &w : nullptr;
  g_hr_pass.plane = plane;
  f();
  g_hr_pass = keep;
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
  struct SrcScope {  // P12A fix 2: the entry's source for the hr draws (HrCmd::src)
    explicit SrcScope(int32_t i) { g_draw_src = gfx_mover_of(i); }
    ~SrcScope() { g_draw_src = GfxMoverSrc{}; }
  } src_scope(i);
  if (LiveTerrainWorld *w = wide_world(ctx))   // G32: the same entry on the wide scene
    on_wide(g, *w, [&] {
      // P10D fix 1: a queue entry's words tagged 1 and drawn into the hr plane too (queued for hr_flush). Fix 2: an entry drawn
      // after the queue's end while the hr plane holds it (the cursor passes) drawn at once: inside a cursor pass kept apart
      // into hrc (tag 2), else into hr (tag 1). Without an hr plane (frame mode) tag 0: its scene pixels stay over the world.
      const bool post = !w->hr_open && w->hr_valid && mode() == 0;
      const bool cur = post && g.hud.cursor_sprites && g.hud.cursor_depth > 0;
      tagged(g, *w, w->hr_open ? 1 : post ? (cur ? 2 : 1) : 0, w->hr_open || post, [&] {
        draw_entry(g, ctx, add32(gs.g_spriteX[k], w->scene_e), gs.g_spriteY[k], gs.g_spriteShade[k],
                   static_cast<int32_t>(gs.g_spriteFlags[k]), gs.g_spriteObj[k]);
      }, w->hr_open ? 0 : cur ? 2 : 1);
    });
}

// drawQueuedSprite's golfer label: after the simulation part built the name in g_textBuffer
void queue_label(Gfx &g, Ctx &ctx, const uint32_t *a, int32_t xoff);
void b_queue_label(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (g_queue_walk && !SG_DEFECT(g_defect_label_under_sprites)) {  // P22C item 6: drawn at the walk's end (b_queue_end)
    const GameState &gs = ctx.gs();
    const char *t = reinterpret_cast<const char *>(gs.g_textBuffer);
    g_deferred_labels.push_back({a[0], std::string(t, strnlen(t, sizeof gs.g_textBuffer - 1))});
    return;
  }
  struct LabelScope {  // P12A fix 2: the text runs of this label carry the entry's source (gfx.cpp capture: GfxTextRun::mover)
    explicit LabelScope(int32_t i) { g_label_src = gfx_mover_of(i); }
    ~LabelScope() { g_label_src = GfxMoverSrc{}; }
  } label_scope(static_cast<int32_t>(a[0]));
  g.text.queue_label = true;  // P23F fix 3: the frame copy is not a frame text (gfx.cpp draw_text_string's edge rule)
  queue_label(g, ctx, a, 0);
  g.text.queue_label = false;
  if (LiveTerrainWorld *w = wide_world(ctx))   // G32
    on_wide(g, *w, [&] {
      // P10D fix 1: the label (text and bars, other families' bodies) is not a sprite: the scene words it changed in the band
      // around its anchor get tag 0, so the label stays over the hr plane
      const int32_t i = static_cast<int32_t>(a[0]);
      // only a golfer entry has a label (queue_label's first returns: shade 0, flags -1 or without bit 8, golfer 0xff or past 0x97)
      bool band = w->tag.size() == g.root.px.size() && i >= 0 && i < 1024;
      if (band) {
        const GameState &gs = ctx.gs();
        const int32_t a8 = static_cast<int32_t>(gs.g_spriteFlags[static_cast<size_t>(i)]);
        band = gs.g_spriteShade[static_cast<size_t>(i)] != 0 && a8 != -1 && (a8 & 0x100) && (a8 & 0xff) != 0xff && (a8 & 0xff) < 0x98;
      }
      const auto lt0 = std::chrono::steady_clock::now();
      int32_t bx0 = 0, bx1 = 0, by0 = 0, by1 = 0;
      std::vector<uint16_t> before;
      if (band) {
        const int32_t ax = add32(ctx.gs().g_spriteAnchorX[static_cast<size_t>(i)], w->scene_e);
        const int32_t ay = ctx.gs().g_spriteAnchorY[static_cast<size_t>(i)];
        bx0 = std::max(0, ax - 320), bx1 = std::min(g.root.w, ax + 320);
        by0 = std::max(0, ay - 48), by1 = std::min(g.root.h, ay + 24);
        for (int32_t y = by0; y < by1 && bx0 < bx1; ++y) {
          const uint16_t *row = g.root.px.data() + static_cast<size_t>(y) * static_cast<size_t>(g.root.w);
          before.insert(before.end(), row + bx0, row + bx1);
        }
      }
      queue_label(g, ctx, a, w->scene_e);
      if (band) {
        size_t n = 0;
        for (int32_t y = by0; y < by1; ++y)
          for (int32_t x = bx0; x < bx1; ++x, ++n) {
            const size_t p = static_cast<size_t>(y) * static_cast<size_t>(g.root.w) + static_cast<size_t>(x);
            if (g.root.px[p] != before[n]) w->tag[p] = 0;
          }
      }
      w->hr_label_us += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - lt0).count();
    });
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
  const uint32_t font = (zoom > 4 && SG_DEFECT(g_defect_zoom8_font)) ? state_va(ctx, &GameState::g_font51b360) : state_va(ctx, &GameState::g_fontRef_519fd8);
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
  g_queue_walk = false;  // P22C item 6: the labels the walk kept, in queue order, each with its own name text in g_textBuffer
  if (!g_deferred_labels.empty()) {
    std::vector<DeferredLabel> v;
    v.swap(g_deferred_labels);
    GameState &gs = ctx.gs();
    std::vector<int8_t> keep(gs.g_textBuffer, gs.g_textBuffer + sizeof gs.g_textBuffer);
    for (const DeferredLabel &l : v) {
      std::memcpy(gs.g_textBuffer, l.text.c_str(), l.text.size() + 1);
      const uint32_t e[1] = {l.entry};
      b_queue_label(g, ctx, e, 1);
    }
    std::memcpy(gs.g_textBuffer, keep.data(), keep.size());  // the buffer as the walk left it (drawing only)
  }
  if (LiveTerrainWorld *w = wide_world(ctx)) {   // G32: the frame at the queue's end
    g.ensure_root(ctx);
    PerfDetailScope pd(kPdQueueEnd);  // P15A
    w->s1 = g.root.px;
    w->scene_valid = true;
    pd.stop();
    if (w->hr_open && hr_recording()) {  // P15A: the flush recorded with the queue's commands; hr_valid as hr_flush sets it
      auto cmds = std::make_shared<std::vector<std::shared_ptr<HrCmd>>>();
      if (hr_checking()) {
        if (g_hr_queue.w == w) *cmds = g_hr_queue.cmds;   // the check: the synchronous flush below takes them too
      } else {
        if (g_hr_queue.w == w) cmds->swap(g_hr_queue.cmds);
        g_hr_queue.cmds.clear();
        g_hr_queue.w = nullptr;
      }
      const HrGeo geo = hr_geo(*w);
      hr_job().ops.emplace_back([geo, cmds](LiveTerrainWorld &W) {
        geo.put(W);
        g_hr_queue.cmds = *cmds;
        g_hr_queue.w = &W;
        hr_flush(W);
      });
      w->hr_valid = g_hr_queue.defer_np == static_cast<size_t>(w->pw) * static_cast<size_t>(w->ph);
    }
    if (w->hr_open && (!hr_recording() || hr_checking())) {
      w->hr_valid = false;   // as before the recorded flush set it: hr_flush sets it
      hr_flush(*w);  // P10D fix 1: the queued hr draws, then the hr plane holds this queue's sprites
    }
    w->hr_valid = w->hr_open && w->hr_valid;  // P10D fix 1: the hr plane holds this queue's sprites (hr_flush sets it)
    w->hr_open = false;
    {
      const PerfDetailScope pd2(kPdQueueEnd);  // P15A
      gfxb::text_wide_end(g, w->scene, w->scene_w, w->scene_e, w->scene_text);  // P9C: the scene without its runs' glyphs
    }
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

// ---- P12A fix 2 (gfx.h): the queue entries' sources and the published hr scene
void gfx_mover_source(int8_t kind, int32_t index, int32_t wx, int32_t wy, float fx, float fy) {
  GfxMoverSrc s;
  s.kind = kind;
  s.index = index;
  s.wx = wx;
  s.wy = wy;
  s.fx = fx;  // P19A item 7
  s.fy = fy;
  g_mover_next = s;
}
void gfx_mover_trail_log(int32_t golfer, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {  // P22C (development)
  static std::FILE *f = std::getenv("SG_P22_TRAILLOG") ? std::fopen(std::getenv("SG_P22_TRAILLOG"), "w") : nullptr;
  if (!f) return;
  std::fprintf(f, "%lld\t%d\t%d\t%d\t%d\t%d\n", static_cast<long long>(perf_now_ns() / 1000), golfer, x1, y1, x2, y2);
  std::fflush(f);
}
bool gfx_mover_trail_pin() {  // P22C item 3 (gfx.h)
  static const bool on = !(std::getenv("SG_P22_TRAILPIN") && std::strcmp(std::getenv("SG_P22_TRAILPIN"), "0") == 0);
  return on && !gfx_trail_glide();  // P22C fix 3: with --trail-glide the ball glides and its trail moves with it
}
void gfx_mover_queued(int32_t n) {
  if (n >= 0 && n < static_cast<int32_t>(g_mover_tab.size())) g_mover_tab[static_cast<size_t>(n)] = g_mover_next;
  g_mover_next = GfxMoverSrc{};
}
void gfx_mover_reset() {
  g_mover_tab.fill(GfxMoverSrc{});
  g_mover_next = GfxMoverSrc{};
}
GfxMoverSrc gfx_mover_of(int32_t n) {
  return n >= 0 && n < static_cast<int32_t>(g_mover_tab.size()) ? g_mover_tab[static_cast<size_t>(n)] : GfxMoverSrc{};
}
GfxMoverSrc gfx_mover_label() { return g_label_src; }
void gfx_set_hr_scene(bool on) { g_hr_scene_on.store(on, std::memory_order_relaxed); }
void gfx_hr_scene_cost(int64_t &ns, int64_t &n) {
  ns = g_hr_pub_ns.load();
  n = g_hr_pub_n.load();
}
std::shared_ptr<const GfxHrScene> gfx_hr_scene(Ctx &) {
  HrQueue &q = g_hr_queue;  // the sim thread's (a present runs on the thread that flushed)
  q.pub_taken = true;
  return q.pub;
}
// P15A (gfx.h): the world thread
void gfx_set_hr_defer(bool on) { g_hr_defer.store(on, std::memory_order_relaxed); }
void gfx_set_hr_defer_check(bool on) { g_hr_check.store(on, std::memory_order_relaxed); }
bool gfx_hr_check_on() { return g_hr_check.load(std::memory_order_relaxed) && g_hr_defer.load(std::memory_order_relaxed); }
bool gfx_hr_defer() { return g_hr_defer.load(std::memory_order_relaxed); }
std::shared_ptr<GfxHrJob> gfx_hr_take_job() {
  std::shared_ptr<GfxHrJob> j = std::move(g_hr_job);
  g_hr_job.reset();
  if (!j) j = std::make_shared<GfxHrJob>();
  return j;
}
void gfx_hr_job_merge(std::shared_ptr<GfxHrJob> &into, const std::shared_ptr<GfxHrJob> &next) {
  if (!next) return;
  if (!into) {
    into = next;
    return;
  }
  if (next->has_start) {  // the hr ops before next's start are dropped (P15A fix 3: into's world ops kept, before next's ops)
    std::vector<GfxHrJob::Op> ops;
    for (auto &op : into->ops)
      if (!hr_kind(op.kind)) ops.push_back(std::move(op));
    for (auto &op : next->ops) ops.push_back(op);
    into->ops = std::move(ops);
    into->root = next->root;
    into->root_gen = next->root_gen;
    into->has_start = true;
    return;
  }
  for (auto &op : next->ops) into->ops.push_back(op);
}
namespace {
GfxWorldRun g_world_run;   // P15A fix 3: gfx_world_run_stats (the job thread's)
// P18A (development, SG_P18_DUMP=<file>): the world raster's recorded GL streams and the ops around them that bound its batches, for
// log/roadmap/P18/P18A_work/setup_bench.cpp. Records: u32 kind (1 GL stream, 2 the raster made / resized, 3 fill, 4 blit 1, 5 blit 0),
// i32 width, i32 height, then for kind 1: u8 discard, u8 draws, u32 words + the words, u32 arrays + per array u32 id, u32 floats (the
// floats follow when the id is new), u32 blobs + per blob u32 id, u32 bytes (the bytes when new). SG_P18_DUMP_PASSES=<n> (default 150):
// the streams that draw, at most.
struct P18Dump {
  std::FILE *f = nullptr;
  int left = 0;
  std::map<const void *, uint32_t> ids;
  std::vector<std::shared_ptr<const void>> keep;   // the arrays and images named, kept alive (their addresses stay unique)
};
P18Dump *p18_dump() {
  static P18Dump *d = [] {
    const char *e = std::getenv("SG_P18_DUMP");
    if (!e || !e[0]) return static_cast<P18Dump *>(nullptr);
    auto *x = new P18Dump;
    x->f = std::fopen(e, "wb");
    const char *n = std::getenv("SG_P18_DUMP_PASSES");
    x->left = n ? std::atoi(n) : 150;
    return x->f ? x : static_cast<P18Dump *>(nullptr);
  }();
  return d && d->left > 0 ? d : nullptr;
}
void p18_put(P18Dump &d, uint32_t v) { std::fwrite(&v, 4, 1, d.f); }
template <class T>
void p18_put_shared(P18Dump &d, const std::shared_ptr<const std::vector<T>> &v) {
  const auto it = d.ids.find(v.get());
  const bool fresh = it == d.ids.end();
  const uint32_t id = fresh ? static_cast<uint32_t>(d.ids.size()) : it->second;
  if (fresh) {
    d.ids[v.get()] = id;
    d.keep.push_back(v);
  }
  p18_put(d, id);
  p18_put(d, static_cast<uint32_t>(v->size()));
  if (fresh && !v->empty()) std::fwrite(v->data(), sizeof(T), v->size(), d.f);
}
void p18_dump_op(uint32_t kind, LiveTerrainWorld &w, const gl::GlRec *g, bool discard) {
  P18Dump *d = p18_dump();
  if (!d || !w.raster) return;
  p18_put(*d, kind);
  p18_put(*d, static_cast<uint32_t>(w.raster->width()));
  p18_put(*d, static_cast<uint32_t>(w.raster->height()));
  if (kind == 1) {
    const uint8_t b[2] = {static_cast<uint8_t>(discard ? 1 : 0), static_cast<uint8_t>(g->draws ? 1 : 0)};
    std::fwrite(b, 1, 2, d->f);
    p18_put(*d, static_cast<uint32_t>(g->w.size()));
    std::fwrite(g->w.data(), 4, g->w.size(), d->f);
    p18_put(*d, static_cast<uint32_t>(g->arrays.size()));
    for (const auto &a : g->arrays) p18_put_shared(*d, a);
    p18_put(*d, static_cast<uint32_t>(g->blobs.size()));
    for (const auto &a : g->blobs) p18_put_shared(*d, a);
    if (g->draws && !discard && --d->left == 0) std::fflush(d->f);
  }
}
}  // namespace
void gfx_hr_job_run(GfxHrJob &j, LiveTerrainWorld &w) {
  t_hr_runner = true;
  // P15A fix 3 (gfx.h): which world ops' surface work a later op reads, walking back from the job's end (both read there)
  const size_t n = j.ops.size();
  std::vector<uint8_t> live(n, 1);
  bool need_root = true, need_back = true;
  for (size_t i = n; i-- > 0;) {
    const GfxHrJob::Op &op = j.ops[i];
    switch (op.kind) {
      case kGwStart: need_root = true; break;   // hr_queue_begin reads the root
      case kGwGl: if (op.gl && op.gl->draws && !need_root) live[i] = 0; break;   // draws over the root (reads it where it blends)
      case kGwFill: live[i] = need_root ? 1 : 0; need_root = false; break;
      case kGwBlit1:   // back = root
        if (need_back) { need_root = true; need_back = false; } else { live[i] = 0; }
        break;
      case kGwBlit0:   // root = back
        if (need_root) { need_back = true; need_root = false; } else { live[i] = 0; }
        break;
      case kGwLines: live[i] = need_root ? 1 : 0; break;   // blends over the root
      case kGwReset: need_root = need_back = false; break;
      default: break;
    }
  }
  GfxWorldRun &st = g_world_run;
  for (size_t i = 0; i < n; ++i) {
    GfxHrJob::Op &op = j.ops[i];
    if (op.kind == kGwGl) {
      if (!op.gl) continue;
      if (!w.raster) {
        ++st.no_raster;
        continue;
      }
      gl::GlRaster &r = *w.raster;
      const bool discard = !live[i];
      p18_dump_op(1, w, op.gl.get(), discard);
      const auto t0 = std::chrono::steady_clock::now();
      r.set_discard(discard);
      gl::gl_replay(*op.gl, r);
      r.set_discard(false);
      if (!discard && op.gl->draws) {
        r.finish();
        st.gl_live_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
      }
      ++(discard ? st.gl_discarded : op.gl->draws ? st.gl_live : st.gl_state);
      continue;
    }
    if (!live[i]) {
      ++(op.kind == kGwFill ? st.fills_skipped : op.kind == kGwLines ? st.lines_skipped : st.blits_skipped);
      continue;
    }
    switch (op.kind) {
      case kGwFill: ++st.fills; break;
      case kGwBlit1: case kGwBlit0: ++st.blits; break;
      case kGwLines: ++st.lines; break;
      case kGwReset: ++st.resets; break;
      case kGwState: ++st.states; break;
      default: break;
    }
    if (op.f) op.f(w);
    if (op.kind == kGwReset || op.kind == kGwFill || op.kind == kGwBlit1 || op.kind == kGwBlit0)   // P18A: SG_P18_DUMP
      p18_dump_op(op.kind == kGwReset ? 2u : op.kind == kGwFill ? 3u : op.kind == kGwBlit1 ? 4u : 5u, w, nullptr, false);
  }
  t_hr_runner = false;
}
// P15A fix 3 (gfx.h)
namespace {
std::atomic<bool> g_world_raster_defer{false};
}  // namespace
void gfx_set_world_raster_defer(bool on) { g_world_raster_defer.store(on, std::memory_order_relaxed); }
bool gfx_world_raster_defer() { return g_world_raster_defer.load(std::memory_order_relaxed); }
void gfx_world_op(GfxWorldOp kind, std::function<void(LiveTerrainWorld &)> f) { hr_job().ops.emplace_back(std::move(f), kind); }
gl::GlRec *gfx_world_gl_chunk() {
  GfxHrJob &j = hr_job();
  if (j.ops.empty() || j.ops.back().kind != kGwGl) {
    GfxHrJob::Op op;
    op.kind = kGwGl;
    op.gl = std::make_shared<gl::GlRec>();
    j.ops.push_back(std::move(op));
  }
  return j.ops.back().gl.get();
}
std::vector<GfxWorldLine> *gfx_world_lines_chunk() {   // P23D fix 2
  GfxHrJob &j = hr_job();
  if (j.ops.empty() || j.ops.back().kind != kGwLines || !j.ops.back().lines) {
    auto l = std::make_shared<std::vector<GfxWorldLine>>();
    GfxHrJob::Op op([l](LiveTerrainWorld &W) { world_lines_run(W, *l); }, kGwLines);
    op.lines = l;
    j.ops.push_back(std::move(op));
  }
  return j.ops.back().lines.get();
}
const GfxWorldRun &gfx_world_run_stats() { return g_world_run; }

// ---- P17B (gfx.h GfxHrGpu)
void gfx_set_hr_gpu(GfxHrGpu *g) {
  t_hr_gpu = g;
  t_hr_gpu_dead = false;
}
GfxHrGpu *gfx_hr_gpu() { return t_hr_gpu_dead ? nullptr : t_hr_gpu; }
const GfxHrGpuCheck &gfx_hr_gpu_stats() { return g_hr_gpu_st; }

namespace {
// THE SELF-TEST's stream: a 256 x 192 world (fw 256: a texel of art drawn unscaled covers 0.32 world pixels, so the samples per axis
// reach 4; scales 3, 5, 10 and 7 / 2 reach 1, the sharp weights and Scale2x at 1.5 and 3 world pixels per texel), each op kind but
// PlaneOp (its struct holds the Gfx; its body is P15C's, checked in the app by SG_P17_WORLDCHECK), two queues per filter (the second
// with a changed root band, moved and changed commands and the first queue's hr draw: the tile reuse), hr_now into hr and hrc, a box
uint32_t st_rnd(uint32_t &s) {
  s = s * 1664525u + 1013904223u;
  return s >> 8;
}
Sprite8 st_art(uint32_t &seed, int32_t w, int32_t h) {
  Sprite8 a;
  a.w = w;
  a.h = h;
  a.px.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
  // blocks of one payload (the uniform fast path), transparent 0xff borders, the 0xf8..0xfe range (the shade / alpha skips)
  const uint8_t pick[8] = {0x10, 0x20, 0x33, 0xf8, 0xfa, 0xfe, 0xff, 0x07};
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const uint32_t r = st_rnd(seed);
      uint8_t v = (x / 3 + y / 2) % 5 == 0 ? pick[r % 8] : static_cast<uint8_t>(0x10 + (x / 4) * 7 + (y / 4) * 3);
      if (x == 0 || y == 0 || x == w - 1 || y == h - 1) v = (r & 3) ? 0xff : v;
      a.px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = v;
    }
  return a;
}
struct StPlanes {
  std::vector<uint16_t> hr, hrc, lo, hi;
  std::vector<uint8_t> cov, hrc_cov, bcov;
};
// one run of the stream on world W; snaps: the planes after each queue (read from W's vectors, or from g when given)
bool st_run(LiveTerrainWorld &W, GfxHrGpu *g, std::vector<StPlanes> &snaps) {
  static std::vector<uint16_t> lut;
  if (lut.empty()) {
    uint32_t s = 77;
    lut.resize(0x8000);
    for (uint16_t &v : lut) v = static_cast<uint16_t>(st_rnd(s) & 0x7fff);
  }
  std::array<uint16_t, 256> pal{};
  {
    uint32_t s = 5;
    for (uint16_t &v : pal) v = static_cast<uint16_t>(st_rnd(s) & 0x7fff);
  }
  const int32_t pw = 256, ph = 192;
  W.pw = pw;
  W.ph = ph;
  W.fw = 256;
  W.ox = 0;
  W.scene_e = 0;
  uint32_t seed = 0x1357u;
  std::vector<uint16_t> root(static_cast<size_t>(pw) * ph);
  for (uint16_t &v : root) v = static_cast<uint16_t>(st_rnd(seed) & 0x7fff);
  const Scale scales[5] = {{1, 1, 1}, {3, 3, 1}, {5, 5, 1}, {10, 10, 1}, {7, 7, 2}};
  for (int f = 0; f <= 3; ++f) {
    W.hr_filter = f;
    for (int qn = 0; qn < 2; ++qn) {
      if (qn == 1)
        for (int32_t y = 40; y < 52; ++y)
          for (int32_t x = 10 + y; x < 120 + y; ++x) root[static_cast<size_t>(y) * pw + static_cast<size_t>(x)] ^= 0x0421;
      W.hr_valid = false;
      W.hrc_valid = false;
      W.hrb_valid = false;
      g_hr_queue.cmds.clear();
      g_hr_queue.w = nullptr;
      hr_queue_begin_gpu(W, &root);   // fix 3: with the backend, a reusing start compares on the GPU (hr_queue_begin without it)
      uint32_t cs = 0x2468u + static_cast<uint32_t>(f) * 31u;
      for (int k = 0; k < 18; ++k) {
        const Sprite8 a = st_art(cs, 6 + static_cast<int32_t>(st_rnd(cs) % 30), 5 + static_cast<int32_t>(st_rnd(cs) % 26));
        int32_t x = static_cast<int32_t>(st_rnd(cs) % 760), y = static_cast<int32_t>(st_rnd(cs) % 560);
        if (qn == 1 && k % 4 == 1) x += 3;   // moved: its old and new boxes dirty
        const Scale sc = scales[static_cast<size_t>(k) % 5];
        switch (k % 4) {
          case 0: hr_blit(W, a, x, y, sc, ShadeOp{pal.data(), lut.data(), lut.size()}); break;
          case 1: hr_blit(W, a, x, y, sc, AlphaOp{pal.data(), &kAlphaPrograms[static_cast<size_t>(k) % 30], (k & 4) != 0}); break;
          case 2: hr_blit(W, a, x, y, sc, ColourOp{pal.data(), static_cast<uint16_t>(qn == 1 && k == 6 ? 0x5ad6 : 0x3def)}); break;
          default: hr_blit(W, a, x, y, sc, DrawBOp{pal.data()}); break;
        }
      }
      hr_flush(W);
      if (!W.hr_valid) return false;
      Sprite8 a = st_art(cs, 20, 14);
      hr_now(W, a, 300, 200, scales[1], DrawBOp{pal.data()}, false);
      hr_now(W, a, 320 + qn, 260, scales[4], ColourOp{pal.data(), 0x7c1f}, true);
      hr_box_now(W, a, 500, 100 + qn, scales[2], AlphaOp{pal.data(), &kAlphaPrograms[0], false});
      StPlanes p;
      if (g) {
        if (!g->read(&p.hr, &p.cov, &p.hrc, &p.hrc_cov, &p.lo, &p.hi, &p.bcov)) return false;
      } else {
        p.hr = W.hr;
        p.cov = W.hr_cov;
        p.hrc = W.hrc;
        p.hrc_cov = W.hrc_cov;
        p.lo = W.hrb_lo;
        p.hi = W.hrb_hi;
        p.bcov = W.hrb_cov;
      }
      snaps.push_back(std::move(p));
    }
  }
  // P18B (log/roadmap/P18/P18B_notes.md): a sharp sample whose exact channel sum is on a rounding boundary, the case 2560 x 1440 met
  // (pixel (426, 1285)): the world scale 2.4 (fw 1920, ph 1440: kx = ky = 2.4) and the art at half scale (Scale 1 / 2: fx = fy = 0.5), so
  // world pixel (10, 7) samples texel fraction (0.25, 0.75) of texels (8, 5) .. (9, 6): bu 64, bv 192, sharp su 51, sb 205 (k = 1.2);
  // three of those texels draw palette word 0x56ef (blue 15) and the fourth is 0xff, over 0x3227 (blue 7): cov 53248, sws 55081, and
  // blue = (15 * 53248 + 7 * 12288) / 65536 + 0.5 = 14.0 exactly, so any fused multiply-add in the sums gives 13 (P18B: the RTX 3070's
  // driver fused them until blit() guarded its products). The rest of the 12 x 8 art draws beside it (more pixels compared).
  {
    W.pw = 16;
    W.ph = 1440;
    W.fw = 1920;
    W.ox = 0;
    W.scene_e = 0;
    W.hr_filter = 1;
    W.hr_valid = false;
    W.hrc_valid = false;
    W.hrb_valid = false;
    std::vector<uint16_t> root2(static_cast<size_t>(W.pw) * static_cast<size_t>(W.ph), 0x3227);
    std::array<uint16_t, 256> pal2 = pal;
    pal2[0x26] = 0x56ef;
    Sprite8 a;
    a.w = 12;
    a.h = 8;
    a.px.assign(static_cast<size_t>(a.w) * static_cast<size_t>(a.h), 0xff);
    a.px[static_cast<size_t>(5) * 12 + 8] = 0x26;
    a.px[static_cast<size_t>(5) * 12 + 9] = 0x26;
    a.px[static_cast<size_t>(6) * 12 + 8] = 0x26;
    g_hr_queue.cmds.clear();
    g_hr_queue.w = nullptr;
    hr_queue_begin_gpu(W, &root2);
    hr_blit(W, a, 0, 0, Scale{1, 1, 2}, DrawBOp{pal2.data()});
    hr_flush(W);
    if (!W.hr_valid) return false;
    StPlanes p;
    if (g) {
      if (!g->read(&p.hr, &p.cov, nullptr, nullptr, nullptr, nullptr, nullptr)) return false;
    } else {
      p.hr = W.hr;
      p.cov = W.hr_cov;
    }
    snaps.push_back(std::move(p));
  }
  return true;
}
}  // namespace

bool gfx_hr_gpu_selftest(GfxHrGpu &g, std::string &result, int64_t &words, int64_t &differing) {
  words = differing = 0;
  if (mode() != 0) {
    result = "not run: a gfx_scene control mode is on";
    return false;
  }
  HrQueue keep;
  std::swap(keep, g_hr_queue);
  const bool runner = t_hr_runner, dead = t_hr_gpu_dead;
  GfxHrGpu *const prev = t_hr_gpu;
  const HrPass pass = g_hr_pass;
  g_hr_pass = HrPass{};
  const bool selftest = t_hr_selftest;
  t_hr_selftest = true;   // (fix 2: it may run beside the world thread, so the process-wide publication switch is not touched)
  const GfxHrGpuCheck saved = g_hr_gpu_st;   // the stream's counters are not the session's
  t_hr_runner = true;
  std::vector<StPlanes> cpu, gpu;
  t_hr_gpu = nullptr;
  t_hr_gpu_dead = false;
  LiveTerrainWorld a, b;
  const bool ok_cpu = st_run(a, nullptr, cpu);
  g_hr_queue = HrQueue{};
  t_hr_gpu = &g;
  t_hr_gpu_dead = false;
  const bool ok_gpu = st_run(b, &g, gpu) && !t_hr_gpu_dead;
  g_hr_gpu_st = saved;
  g_hr_queue = HrQueue{};
  std::swap(keep, g_hr_queue);
  t_hr_runner = runner;
  t_hr_gpu = prev;
  t_hr_gpu_dead = dead;
  g_hr_pass = pass;
  t_hr_selftest = selftest;
  if (!ok_cpu || !ok_gpu || cpu.size() != gpu.size()) {
    result = !ok_cpu ? "FAIL: the CPU run drew no plane" : "FAIL: the backend failed during the stream";
    return false;
  }
  std::string first;
  auto cmp = [&](const auto &x, const auto &y, const char *name, size_t q) {
    words += static_cast<int64_t>(std::max(x.size(), y.size()));
    if (x.size() != y.size()) {
      differing += static_cast<int64_t>(std::max(x.size(), y.size()));
      if (first.empty()) first = std::string(name) + " size of queue " + std::to_string(q);
      return;
    }
    for (size_t i = 0; i < x.size(); ++i)
      if (x[i] != y[i]) {
        if (first.empty()) {
          char buf[160];
          std::snprintf(buf, sizeof buf, "%s of queue %zu at (%zu, %zu) gpu 0x%x cpu 0x%x", name, q, i % 256, i / 256,
                        static_cast<unsigned>(y[i]), static_cast<unsigned>(x[i]));
          first = buf;
        }
        ++differing;
      }
  };
  for (size_t q = 0; q < cpu.size(); ++q) {
    cmp(cpu[q].hr, gpu[q].hr, "hr", q);
    cmp(cpu[q].cov, gpu[q].cov, "hr_cov", q);
    cmp(cpu[q].hrc, gpu[q].hrc, "hrc", q);
    cmp(cpu[q].hrc_cov, gpu[q].hrc_cov, "hrc_cov", q);
    cmp(cpu[q].lo, gpu[q].lo, "hrb_lo", q);
    cmp(cpu[q].hi, gpu[q].hi, "hrb_hi", q);
    cmp(cpu[q].bcov, gpu[q].bcov, "hrb_cov", q);
  }
  if (differing) {
    result = "FAIL: " + std::to_string(differing) + " of " + std::to_string(words) + " words differ, first: " + first;
    return false;
  }
  result = "pass: " + std::to_string(words) + " words in " + std::to_string(cpu.size()) + " queues, 0 differing";
  return true;
}
size_t gfx_hr_job_ops(const GfxHrJob &j) { return j.ops.size(); }
std::shared_ptr<const std::vector<uint16_t>> gfx_hr_job_root(const GfxHrJob &j, uint64_t &gen) {
  gen = j.root_gen;
  return j.root;
}
std::shared_ptr<const GfxHrScene> gfx_hr_scene_take() {
  HrQueue &q = g_hr_queue;
  q.pub_taken = true;
  return q.pub;
}
void gfx_hr_job_add_take(GfxHrJob &j, std::shared_ptr<std::shared_ptr<const GfxHrScene>> slot) {
  j.ops.emplace_back([slot](LiveTerrainWorld &) { *slot = gfx_hr_scene_take(); });
}
const std::vector<GfxHrDraw> &gfx_hr_scene_draws(const GfxHrScene &s) { return s.draws; }
int32_t gfx_hr_scene_pw(const GfxHrScene &s) { return s.w ? s.w->pw : 0; }
int32_t gfx_hr_scene_ph(const GfxHrScene &s) { return s.w ? s.w->ph : 0; }
void gfx_hr_scene_redraw(const GfxHrScene &s, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const std::vector<double> &dx,
                         const std::vector<double> &dy, std::vector<uint16_t> &out, std::vector<uint8_t> &cov) {
  static const std::vector<GfxTrailLine> none;
  static const std::vector<double> nd;
  gfx_hr_scene_redraw(s, x0, y0, x1, y1, dx, dy, out, cov, none, nd, nd);
}
void gfx_hr_scene_redraw(const GfxHrScene &s, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const std::vector<double> &dx,
                         const std::vector<double> &dy, std::vector<uint16_t> &out, std::vector<uint8_t> &cov,
                         const std::vector<GfxTrailLine> &lines, const std::vector<double> &ldx, const std::vector<double> &ldy) {
  out.clear();
  cov.clear();
  if (!s.w) return;
  const LiveTerrainWorld &w = *s.w;
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  if (!s.base || s.base_n != np) return;
  x0 = std::max(x0, 0);
  y0 = std::max(y0, 0);
  x1 = std::min(x1, w.pw);
  y1 = std::min(y1, w.ph);
  if (x1 <= x0 || y1 <= y0) return;
  // P12A fix 3: every tile the rectangle meets must have been copied (nothing else of the base was)
  if (s.base_tile.size() != static_cast<size_t>(s.base_tx) * static_cast<size_t>(s.base_ty)) return;
  for (int32_t j = y0 / kHrTile; j <= (y1 - 1) / kHrTile; ++j)
    for (int32_t i = x0 / kHrTile; i <= (x1 - 1) / kHrTile; ++i)
      if (!s.base_tile[static_cast<size_t>(j) * static_cast<size_t>(s.base_tx) + static_cast<size_t>(i)]) return;
  thread_local std::vector<uint16_t> plane;  // whole-world scratch planes (hr_blit_rows indexes them by world pixel)
  thread_local std::vector<uint8_t> cover;
  if (plane.size() != np) {
    plane.assign(np, 0);
    cover.assign(np, 0);
  }
  for (int32_t y = y0; y < y1; ++y) {
    const size_t o = static_cast<size_t>(y) * static_cast<size_t>(w.pw) + static_cast<size_t>(x0);
    std::copy(s.base.get() + o, s.base.get() + o + static_cast<size_t>(x1 - x0), plane.begin() + static_cast<std::ptrdiff_t>(o));
    std::fill(cover.begin() + static_cast<std::ptrdiff_t>(o), cover.begin() + static_cast<std::ptrdiff_t>(o + static_cast<size_t>(x1 - x0)),
              uint8_t(0));
  }
  // P22C fix 3: the trail lines over the base, under every draw of the queue (where the frame drew them: before renderSpriteQueue)
  for (size_t i = 0; i < lines.size(); ++i)
    gfx_trail_line_px(lines[i], i < ldx.size() ? ldx[i] : 0.0, i < ldy.size() ? ldy[i] : 0.0, plane.data(), w.pw, w.ph, x0, y0, x1, y1,
                      cover.data());
  const HrPass keep = g_hr_pass;
  g_hr_pass = HrPass{};  // no scene tags on this thread
  for (size_t k = 0; k < s.cmds.size(); ++k) {
    const HrCmd &c = *s.cmds[k];
    HrGeom g = c.gm;
    HrBox b = c.box;
    const double mx = k < dx.size() ? dx[k] : 0.0, my = k < dy.size() ? dy[k] : 0.0;
    if (mx != 0.0 || my != 0.0) {
      g.rx += mx;
      g.ry += my;
      b = hr_box(w, c.art(), g);  // P15C: the art run samples (its rectangle is the unscaled art's: the same box)
    }
    if (b.empty() || b.x1 <= x0 || b.x0 >= x1 || b.y1 <= y0 || b.y0 >= y1) continue;
    c.run_at(w, g, b, plane.data(), cover.data(), x0, y0, x1, y1);
  }
  g_hr_pass = keep;
  const size_t rw = static_cast<size_t>(x1 - x0);
  out.resize(rw * static_cast<size_t>(y1 - y0));
  cov.resize(out.size());
  for (int32_t y = y0; y < y1; ++y) {
    const size_t o = static_cast<size_t>(y) * static_cast<size_t>(w.pw) + static_cast<size_t>(x0);
    const size_t r = static_cast<size_t>(y - y0) * rw;
    std::copy(plane.begin() + static_cast<std::ptrdiff_t>(o), plane.begin() + static_cast<std::ptrdiff_t>(o + rw), out.begin() + static_cast<std::ptrdiff_t>(r));
    std::copy(cover.begin() + static_cast<std::ptrdiff_t>(o), cover.begin() + static_cast<std::ptrdiff_t>(o + rw), cov.begin() + static_cast<std::ptrdiff_t>(r));
  }
}

// ---- P15C (gfx.h): gfx_hr_scene_redraw on the app's GPU
bool gfx_hr_scene_gpu_op(const GfxHrScene &s, size_t k, GfxHrGpuOp &out) {
  out = GfxHrGpuOp{};
  if (k >= s.cmds.size() || mode() != 0) return false;  // (put's test-only mode 7 and the controls stay on the CPU)
  const HrCmd &c = *s.cmds[k];
  if (!c.gpu(out)) return false;
  const Sprite8 &a = c.art();
  if (a.w < 0 || a.h < 0 || a.px.size() != static_cast<size_t>(a.w) * static_cast<size_t>(a.h)) return false;
  out.art = a.px.data();
  out.art_w = a.w;
  out.art_h = a.h;
  return true;
}

void gfx_hr_scene_box(const GfxHrScene &s, size_t k, double dx, double dy, int32_t box[4]) {
  box[0] = box[1] = box[2] = box[3] = 0;
  if (k >= s.cmds.size() || !s.w) return;
  const HrCmd &c = *s.cmds[k];
  HrBox b = c.box;
  if (dx != 0.0 || dy != 0.0) {  // gfx_hr_scene_redraw's own test and box
    HrGeom g = c.gm;
    g.rx += dx;
    g.ry += dy;
    b = hr_box(*s.w, c.art(), g);
  }
  box[0] = b.x0;
  box[1] = b.y0;
  box[2] = b.x1;
  box[3] = b.y1;
}

// hr_blit_rows' placement of the samples, its expressions unchanged (the per-pixel loop is the GPU's)
bool gfx_hr_scene_tables(const GfxHrScene &s, size_t k, double dx, double dy, int32_t cx0, int32_t cy0, int32_t cx1, int32_t cy1,
                         GfxHrTables &out) {
  out.cols.clear();
  out.rows.clear();
  if (k >= s.cmds.size() || !s.w) return false;
  const LiveTerrainWorld &w = *s.w;
  const HrCmd &c = *s.cmds[k];
  HrGeom gm = c.gm;
  HrBox box = c.box;
  if (dx != 0.0 || dy != 0.0) {
    gm.rx += dx;
    gm.ry += dy;
    box = hr_box(w, c.art(), gm);
  }
  out.box[0] = box.x0;
  out.box[1] = box.y0;
  out.box[2] = box.x1;
  out.box[3] = box.y1;
  if (box.empty()) return false;
  const Sprite8 &a = c.art();
  const double rx = gm.rx, ry = gm.ry, fx = gm.fx, fy = gm.fy;
  const double kx = static_cast<double>(w.fw) / 800.0, ky = static_cast<double>(w.ph) / 600.0;
  const int32_t X0 = std::max(box.x0, cx0), X1 = std::min(box.x1, cx1);
  const int32_t Y0 = std::max(box.y0, cy0), Y1 = std::min(box.y1, cy1);
  if (X0 >= X1 || Y0 >= Y1) return false;
  const double du = 1.0 / (kx * fx), dv = 1.0 / (ky * fy);
  const int nsx = std::min(4, std::max(1, static_cast<int>(std::ceil(du - 1e-6))));
  const int nsy = std::min(4, std::max(1, static_cast<int>(std::ceil(dv - 1e-6))));
  const int filter = w.hr_filter == 3 ? 1 : w.hr_filter;
  const auto sharp_w = [](double t, double k) {
    const double s = std::min(1.0, std::max(0.0, (t - 0.5) * k + 0.5));
    return static_cast<uint32_t>(std::floor(s * 256.0 + 0.5));
  };
  const double ksx = std::max(1.0, nsx / du), ksy = std::max(1.0, nsy / dv);
  out.x0 = X0;
  out.y0 = Y0;
  out.x1 = X1;
  out.y1 = Y1;
  out.nsx = nsx;
  out.nsy = nsy;
  out.filter = filter;
  const int ns = nsx * nsy;
  out.inv = 1.0 / (65536.0 * ns);
  out.cols.resize(static_cast<size_t>(X1 - X0) * static_cast<size_t>(nsx) * 4u);
  for (int32_t X = X0; X < X1; ++X)
    for (int si = 0; si < nsx; ++si) {
      const double uu = (((X + (si + 0.5) / nsx) - w.ox) / kx + w.scene_e - rx) / fx - 0.5;
      const double uf = std::floor(uu);
      const int32_t i0 = static_cast<int32_t>(uf);
      int32_t *e = &out.cols[(static_cast<size_t>(X - X0) * static_cast<size_t>(nsx) + static_cast<size_t>(si)) * 4u];
      e[0] = i0 >= 0 && i0 < a.w ? i0 : -1;
      e[1] = i0 + 1 >= 0 && i0 + 1 < a.w ? i0 + 1 : -1;
      e[2] = static_cast<int32_t>(static_cast<uint32_t>(std::floor((uu - uf) * 256.0 + 0.5)));
      e[3] = static_cast<int32_t>(filter ? sharp_w(uu - uf, ksx) : 0u);
    }
  out.rows.resize(static_cast<size_t>(Y1 - Y0) * static_cast<size_t>(nsy) * 4u);
  for (int32_t Y = Y0; Y < Y1; ++Y)
    for (int sj = 0; sj < nsy; ++sj) {
      const double v = ((Y + (sj + 0.5) / nsy) / ky - ry) / fy - 0.5;
      const double vf = std::floor(v);
      const int32_t j0 = static_cast<int32_t>(vf);
      int32_t *e = &out.rows[(static_cast<size_t>(Y - Y0) * static_cast<size_t>(nsy) + static_cast<size_t>(sj)) * 4u];
      e[0] = j0 >= 0 && j0 < a.h ? j0 : -1;
      e[1] = j0 + 1 >= 0 && j0 + 1 < a.h ? j0 + 1 : -1;
      e[2] = static_cast<int32_t>(static_cast<uint32_t>(std::floor((v - vf) * 256.0 + 0.5)));
      e[3] = static_cast<int32_t>(filter ? sharp_w(v - vf, ksy) : 0u);
    }
  return true;
}

const uint16_t *gfx_hr_scene_base(const GfxHrScene &s, const uint8_t *&tiles, int32_t &tx, int32_t &ty) {
  tiles = s.base_tile.empty() ? nullptr : s.base_tile.data();
  tx = s.base_tx;
  ty = s.base_ty;
  const size_t np = s.w ? static_cast<size_t>(s.w->pw) * static_cast<size_t>(s.w->ph) : 0;
  return s.base && s.base_n == np && np > 0 ? s.base.get() : nullptr;
}

const uint16_t *gfx_hr_plane_table(size_t &n) {
  const auto &t = gfxs::color16_planes();
  n = t.size();
  return t.data();
}

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
// ---- P10I fix 1 (gfx.h gfx_set_hud_lines): the tee lines of the cursor passes (recorded in terrain_overlay.cpp terrain_draw_line)
void gfx_set_hud_lines(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  g->hud.lines_on = on && g->hud.on;
}
// ---- P22E item 7 (gfx.h gfx_set_hud_edge): the placement box's edges (terrain_overlay.cpp) and the edge-cut texts (gfx.cpp
// text_run_end) of the cursor passes
void gfx_set_hud_edge(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  g->hud.edge_on = on && g->hud.on;
}
// ---- P23F (gfx.h gfx_set_frame_edge_text): the frame's own texts over the course (gfx.cpp draw_text_string / text_run_end)
void gfx_set_frame_edge_text(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  g->hud.edge_frame = on;
  if (!on) g->text.frame_edge_runs.clear();
}
// ---- P23E (gfx.h gfx_set_hud_wide_lines): more still lines (terrain_overlay.cpp terrain_draw_line)
void gfx_set_hud_wide_lines(Ctx &ctx, uint32_t rows) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  g->hud.wide_lines = g->hud.on ? rows : 0u;
}
void gfx_wide_line_scope(Ctx &ctx, uint32_t row, bool open) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  if (open) g->hud.wide_scope |= row;
  else g->hud.wide_scope &= ~row;
}
// ---- P23D fix 3 (gfx.h gfx_set_wide_bubbles)
namespace {
std::atomic<bool> g_wide_bubbles{false};
}
void gfx_set_wide_bubbles(bool on) { g_wide_bubbles.store(on, std::memory_order_relaxed); }
bool gfx_wide_bubbles() { return g_wide_bubbles.load(std::memory_order_relaxed); }
namespace {
std::atomic<bool> g_wide_ground{false};   // P23D fix 6
}
void gfx_set_wide_ground(bool on) { g_wide_ground.store(on, std::memory_order_relaxed); }
bool gfx_wide_ground() { return g_wide_ground.load(std::memory_order_relaxed); }
void gfx_wide_ground_stats(uint64_t &queued, uint64_t &past) {
  queued = g_ground_queued.load(std::memory_order_relaxed);
  past = g_ground_past.load(std::memory_order_relaxed);
}
void gfx_ground_begin(Ctx &ctx) {
  if (Gfx *g = gfx(ctx)) ++g->hud.ground_depth;
}
void gfx_ground_end(Ctx &ctx) {
  if (Gfx *g = gfx(ctx)) g->hud.ground_depth = g->hud.ground_depth > 0 ? g->hud.ground_depth - 1 : 0;
}
void gfx_bubble_begin(Ctx &ctx) {
  if (Gfx *g = gfx(ctx)) ++g->hud.bubble_depth;
}
void gfx_bubble_end(Ctx &ctx) {
  if (Gfx *g = gfx(ctx)) g->hud.bubble_depth = g->hud.bubble_depth > 0 ? g->hud.bubble_depth - 1 : 0;
}
namespace gfxs {
void bubble_shade(Gfx &g, Ctx &ctx, const Sprite8 &s, int32_t x, int32_t y, const uint16_t *lut, size_t n) {
  if (g.hud.bubble_depth <= 0 || g_hr_pass.hr || g.root.w != 800 || g.root.h != 600 || mode() != 0) return;
  LiveTerrainWorld *w = wide_world(ctx);
  if (!w || w->hr_open || !w->hr_valid || w->scene_w <= 0 || w->scene.size() != static_cast<size_t>(w->scene_w) * 600) return;
  if (w->bub800.size() != g.root.px.size()) w->bub800.assign(g.root.px.size(), 0);
  if (w->bub_word.size() != g.root.px.size()) w->bub_word.assign(g.root.px.size(), 0);
  // the root pixels the piece wrote (draw_g16_lut's loop: every sprite byte below 0xff), with the word they hold now
  const int32_t l = std::max(x, 0), t = std::max(y, 0), r = std::min(x + s.w, g.root.w), b = std::min(y + s.h, g.root.h);
  for (int32_t yy = t; yy < b; ++yy)
    for (int32_t xx = l; xx < r; ++xx) {
      if (s.px[static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x)] >= 0xff) continue;
      const size_t i = static_cast<size_t>(yy) * static_cast<size_t>(g.root.w) + static_cast<size_t>(xx);
      w->bub800[i] = 1;
      w->bub_word[i] = g.root.px[i];
    }
  ShadeOp op{s.pal.data(), lut, n};
  hr_now(*w, s, add32(x, w->scene_e), y, Scale{1, 1, 1}, op, false);
}
// P23D fix 5 (gfx_scene.h): a thought bubble's Surface_drawScaled piece and its fillWindowRect box (P4_golferOne ML+0x7640 / ML+0x7754
// / ML+0x7778), each also into the hr plane after the queue (DrawBOp: drawB's rule; a mirrored piece as the art mirrored, which is
// what draw_b16's mirrored body reads over the same rectangle) and the root pixels it wrote marked
LiveTerrainWorld *bubble_world(Gfx &g, Ctx &ctx) {
  if (g.hud.bubble_depth <= 0 || g_hr_pass.hr || g.root.w != 800 || g.root.h != 600 || mode() != 0) return nullptr;
  LiveTerrainWorld *w = wide_world(ctx);
  if (!w || w->hr_open || !w->hr_valid || w->scene_w <= 0 || w->scene.size() != static_cast<size_t>(w->scene_w) * 600) return nullptr;
  if (w->bub800.size() != g.root.px.size()) w->bub800.assign(g.root.px.size(), 0);
  if (w->bub_word.size() != g.root.px.size()) w->bub_word.assign(g.root.px.size(), 0);
  return w;
}
void bubble_mark(Gfx &g, LiveTerrainWorld &w, int32_t x, int32_t y) {
  const size_t i = static_cast<size_t>(y) * 800 + static_cast<size_t>(x);
  w.bub800[i] = 1;
  w.bub_word[i] = g.root.px[i];
}
void bubble_scaled(Gfx &g, Ctx &ctx, const Sprite8 &s, int32_t x, int32_t y, int32_t nx, int32_t ny, int32_t den) {
  LiveTerrainWorld *w = bubble_world(g, ctx);
  if (!w || s.w <= 0 || s.h <= 0) return;
  const bool mirror = nx < 0 && nx != INT32_MIN && -nx != den;
  const int32_t ax = mirror ? -nx : nx;
  if (ax <= 0 || ny <= 0 || den <= 0) return;
  if (ax == den && ny == den) {   // draw_b16's unscaled copy (draw16): every byte below 0xfe at (x, y)
    for (int32_t yy = std::max(y, 0); yy < std::min(y + s.h, 600); ++yy)
      for (int32_t xx = std::max(x, 0); xx < std::min(x + s.w, 800); ++xx)
        if (s.px[static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x)] < 0xfe) bubble_mark(g, *w, xx, yy);
  } else {   // draw_b16's scaled walk (its rectangle, steps and mirrored column)
    const float fx = static_cast<float>(ax) / static_cast<float>(den), fy = static_cast<float>(ny) / static_cast<float>(den);
    const int32_t L = add32(x, static_cast<int32_t>(fx)), T = add32(y, static_cast<int32_t>(fy));
    const int32_t W = static_cast<int32_t>(static_cast<double>(s.w) * static_cast<double>(fx));
    const int32_t H = static_cast<int32_t>(static_cast<double>(s.h) * static_cast<double>(fy));
    const uint64_t stx = static_cast<uint64_t>(static_cast<uint32_t>(den << 16) / static_cast<uint32_t>(ax));
    const uint64_t sty = static_cast<uint64_t>(static_cast<uint32_t>(den << 16) / static_cast<uint32_t>(ny));
    for (int32_t yy = std::max(T, 0); yy < std::min(add32(T, H), 600); ++yy) {
      const uint64_t sr = (sty * static_cast<uint64_t>(yy - T)) >> 16;
      for (int32_t xx = std::max(L, 0); xx < std::min(add32(L, W), 800); ++xx) {
        uint64_t sc = (stx * static_cast<uint64_t>(xx - L)) >> 16;
        if (mirror) sc = static_cast<uint64_t>(s.w) - 1u - sc;
        if (sr >= static_cast<uint64_t>(s.h) || sc >= static_cast<uint64_t>(s.w)) continue;
        if (s.px[static_cast<size_t>(sr) * static_cast<size_t>(s.w) + static_cast<size_t>(sc)] < 0xfe) bubble_mark(g, *w, xx, yy);
      }
    }
  }
  Sprite8 m;
  const Sprite8 *a = &s;
  if (mirror) {
    m = s;
    for (int32_t r = 0; r < m.h; ++r) std::reverse(m.px.begin() + static_cast<ptrdiff_t>(r) * m.w, m.px.begin() + static_cast<ptrdiff_t>(r + 1) * m.w);
    a = &m;
  }
  DrawBOp op{a->pal.data()};
  hr_now(*w, *a, add32(x, w->scene_e), y, Scale{ax, ny, den}, op, false);
}
void bubble_fill(Gfx &g, Ctx &ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t word) {
  LiveTerrainWorld *w = bubble_world(g, ctx);
  if (!w || x1 <= x0 || y1 <= y0) return;
  for (int32_t y = std::max(y0, 0); y < std::min(y1, 600); ++y)
    for (int32_t x = std::max(x0, 0); x < std::min(x1, 800); ++x) bubble_mark(g, *w, x, y);
  Sprite8 s;   // the box: one-colour art of its size at scale 1
  s.w = x1 - x0;
  s.h = y1 - y0;
  s.px.assign(static_cast<size_t>(s.w) * static_cast<size_t>(s.h), 0);
  s.pal.fill(0);
  s.pal[0] = word;
  DrawBOp op{s.pal.data()};
  hr_now(*w, s, add32(x0, w->scene_e), y0, Scale{1, 1, 1}, op, false);
}
void bubble_bar(Gfx &g, Ctx &ctx, int32_t x0, int32_t y0, int32_t xe, int32_t ye, const uint16_t *before, int32_t lx0, int32_t lx1,
                int32_t yc) {
  if (g.hud.bubble_depth <= 0 || g_hr_pass.hr || g.root.w != 800 || g.root.h != 600 || mode() != 0) return;
  LiveTerrainWorld *w = wide_world(ctx);
  if (!w || w->hr_open || !w->hr_valid || w->scene_w <= 0 || w->scene.size() != static_cast<size_t>(w->scene_w) * 600) return;
  if (w->bub800.size() != g.root.px.size()) w->bub800.assign(g.root.px.size(), 0);
  if (w->bub_word.size() != g.root.px.size()) w->bub_word.assign(g.root.px.size(), 0);
  const int32_t bw = xe - x0 + 1;
  int32_t bx0 = xe + 1, by0 = ye + 1, bx1 = x0 - 1, by1 = y0 - 1;
  if (!before) {   // wholly past the root: the band between the line's ends, rows yc - 5 .. yc + 4
    bx0 = std::max(lx0, -w->scene_e);
    bx1 = std::min(lx1 - 1, 799 + w->scene_e);
    by0 = std::max(yc - 5, 0);
    by1 = std::min(yc + 4, 599);
    y0 = 1;
    ye = 0;   // no root pixel to mark
  }
  for (int32_t y = y0; y <= ye; ++y)
    for (int32_t x = x0; x <= xe; ++x) {
      const size_t i = static_cast<size_t>(y) * 800 + static_cast<size_t>(x);
      if (before[static_cast<size_t>(y - y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - x0)] == g.root.px[i]) continue;
      w->bub800[i] = 1;
      w->bub_word[i] = g.root.px[i];
      bx0 = std::min(bx0, x);
      by0 = std::min(by0, y);
      bx1 = std::max(bx1, x);
      by1 = std::max(by1, y);
    }
  if (bx1 < bx0 || by1 < by0) return;
  if (before && bx0 == 0 && lx0 < 0) bx0 = std::max(lx0, -w->scene_e);           // cut by the root's left edge: the band to the line's end
  if (before && bx1 == 799 && lx1 > 800) bx1 = std::min(lx1 - 1, 799 + w->scene_e);   // and by its right edge
  Sprite8 s;   // the band: black art of the changed pixels' box, drawn at scale 1 (each texel one 800-space pixel)
  s.w = bx1 - bx0 + 1;
  s.h = by1 - by0 + 1;
  s.px.assign(static_cast<size_t>(s.w) * static_cast<size_t>(s.h), 0);
  s.pal.fill(0);
  const int body = alpha_body(0.5f);
  if (body < 0) return;
  AlphaOp op{s.pal.data(), &kAlphaPrograms[body + 15], false};
  hr_now(*w, s, add32(bx0, w->scene_e), by0, Scale{1, 1, 1}, op, false);
}
}  // namespace gfxs
void gfx_cursor_sprites_begin(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.cursor_sprites) return;
  ++g->hud.cursor_depth;
  LiveTerrainWorld *w = wide_world(ctx);
  if (!w || !w->scene_valid) return;
  if (w->scene_pre.size() != w->scene.size()) {
    w->scene_pre = w->scene;  // the first pass's start (P6_ToolLabel, then P7)
    w->tag_pre = w->tag;      // P10D fix 1: the tags of scene_pre
  }
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
