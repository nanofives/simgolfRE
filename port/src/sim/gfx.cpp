// Module `gfx` (phase 5, step 1, 2026-10-22): the software 16-bit graphics backend (portsim/sim/gfx.h). Plan and the
// specification table: re/PHASE5_GFX_PLAN.md. Tests: port/test/test_gfx.py. The bodies below follow the C3 / C4 hook
// sources named at each one (shim/src/re/*.cpp) and the matched jgld sources (re/match/jgld_*.cpp); functions are cited by
// name, their instruction addresses are in those sources.
//
// What the backend models (the title menu's chain): an 8-bit image per exe Image object (loadImage over an 8-bit RLE PCX
// read through the context's GameFiles, its palette as the 256 RGB555 words jgld's Palette::apply makes), a cropped 8-bit
// sprite per exe SpriteRef (cropSprite), the root window's 16-bit surface and the back buffer Display::present shows.
// Nothing is written into the GameState or its address space.
//
// Drawing bodies live here (the core table kBodies) and in family files port/src/sim/gfx_<family>.cpp listed in
// gfx_families.inc; what a body needs is in gfx_body.h (roadmap S2.1).
//
// Negative controls (g_negctl_gfx, read RED by port/test/test_gfx.py against the original's frame):
//   1 Image_draw one pixel to the right of the original's point (a wrong blit)
//   2 the palette word of the colour index at a loaded image's top-left pixel off by one (a wrong palette entry)
//   3 draw16's transparency test `> 0xfe` instead of `>= 0xfe` (index 0xfe drawn)
//   4 Window::refreshFull does not copy the root surface into the back buffer
//   5 Surface_stretchBlit (blitPair1) ignores the mask's blend bytes (every pixel the colour sprite's word)
//   6 drawG16's shade byte 0xf8 drawn as a palette colour instead of shading the destination through the table
//   7 drawCenteredText / drawTextWithColor leave the target's Widget_setQuad words alone (roadmap S2.10, U-1293)
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/gfx.h"
#include "gfx_body.h"

#include <algorithm>
#include <array>
#include <cmath>  // P24D: std::lround (the squeeze)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/gditext.h"  // P9C: GameFont of a run
#include "portsim/sim/gfx_hud.h"
#include "portsim/sim/gfx_scene.h"
#include "portsim/sim/gfx_text.h"
#include "portsim/sim/perf.h"  // P10A: the drawing's wall time for the host's profiler

namespace portsim {

std::atomic<int> g_gfx_attached{0};

namespace {
int g_negctl_gfx = 0;

std::mutex &mu() {
  static std::mutex m;
  return m;
}
std::map<const Ctx *, Gfx *> &attached() {
  static std::map<const Ctx *, Gfx *> m;
  return m;
}
std::map<const Ctx *, std::unique_ptr<Gfx>> &owned() {
  static std::map<const Ctx *, std::unique_ptr<Gfx>> m;
  return m;
}
// roadmap S7.9: the refresh hooks (gfx.h gfx_set_refresh_hook)
std::map<const Ctx *, std::pair<GfxRefreshHook, void *>> &refresh_hooks() {
  static std::map<const Ctx *, std::pair<GfxRefreshHook, void *>> m;
  return m;
}
}  // namespace

void gfx_set_refresh_hook(Ctx &ctx, GfxRefreshHook fn, void *arg) {
  std::lock_guard<std::mutex> l(mu());
  if (fn) refresh_hooks()[&ctx] = {fn, arg};
  else refresh_hooks().erase(&ctx);
}

namespace gfxf {
const int32_t *blit_scale(Gfx &g);   // gfx_prim_fill.cpp (roadmap S2.4): Display::setScale's three words
void fill_rect16(Surface16 &d, int32_t l, int32_t t, int32_t r, int32_t b, uint16_t c);   // gfx_prim_fill.cpp
}
namespace gfxe {
void compose(Gfx &g, Ctx &ctx);   // gfx_editbox.cpp (roadmap S3.19): the window tree's shown child surfaces onto the back buffer
}

void Gfx::unsup(const std::string &why) {
  if (++unsupported[why] == 1) notes.push_back(why);
}
// the root surface, made at its first use: Window::create's size scaleX(0x140) x scaleX(0xf0), scaleX(v) =
// (g_screenWidth * v + 160) / 320 (gditext.cpp's two scaled fonts use the same rule)
void Gfx::ensure_root(Ctx &ctx) {
  if (root.w) return;
  const int32_t sw = ctx.gs().g_screenWidth;
  const int32_t w = div32(add32(mul32(sw, 0x140), 160), 320), h = div32(add32(mul32(sw, 0xf0), 160), 320);
  root.make(w > 0 ? w : 0x320, h > 0 ? h : 0x258);
  back.make(root.w, root.h);
}
uint32_t Gfx::root_va(Ctx &ctx) { return ctx.gs().f_stateptr_4c1570[0]; }

namespace gfxb {

std::string vstring(const Memory &m, uint32_t va) {
  const uint32_t n = vstrlen(m, va);
  std::string s(n, '\0');
  if (n) std::memcpy(&s[0], m.at(va, n), n);
  return s;
}

// A file the backend writes for the harness (snapshots, the log): the files overlay, never the install (refused when the
// directory view writes through to disk).
bool put_file(Ctx &ctx, const std::string &name, const std::vector<uint8_t> &bytes) {
  GameFiles &f = ctx.files();
  if (auto *d = dynamic_cast<DirGameFiles *>(&f))
    if (d->write_through()) return false;
  return f.write(name, bytes);
}
std::vector<uint8_t> surface_bytes(const Surface16 &s) {
  std::vector<uint8_t> b(s.px.size() * 2);
  for (size_t i = 0; i < s.px.size(); ++i) {
    b[2 * i] = static_cast<uint8_t>(s.px[i] & 0xff);
    b[2 * i + 1] = static_cast<uint8_t>(s.px[i] >> 8);
  }
  return b;
}

// ------------------------------------------------------------------------------------------------ image loading
// decodePcx8 (c3ee.cpp DecodePaletted_re) into `img`, with the palette words for count entries from ofs; returns the
// original's result code (0 = decoded). The file's magic-text prefix (a copy of the exe's text) is not modelled: a file
// that does not start with the PCX manufacturer byte is reported.
static int decode_pcx8(Gfx &g, const std::vector<uint8_t> &data, int32_t ofs, int32_t count, bool pal_arg, Image8 &img) {
  if (data.size() < 0x80 + 0x300) return 3;
  const uint8_t *d = data.data();
  if (d[0] != 0xa) {
    g.unsup("loadImage: no PCX manufacturer byte (the magic-text prefix is not modelled)");
    return 3;
  }
  if (d[2] != 1) return 3;                                     // encoding
  if (d[1] < 4 && pal_arg) return 3;                           // version below 4 with a palette argument
  auto u16 = [&](size_t o) { return static_cast<int32_t>(d[o] | (d[o + 1] << 8)); };
  const int32_t h = u16(10) - u16(6) + 1, w = u16(8) - u16(4) + 1;
  const int32_t bpp = d[3], planes = d[65], bpl = u16(66);
  if (w <= 0 || h <= 0) {
    g.unsup("loadImage: empty or negative PCX extent");
    return 3;
  }
  // Surface_paint (w, h, 8): an 8 bpp DIB section, pitch (w + 3) >> 2 << 2, zero-filled
  img.w = w;
  img.h = h;
  img.pitch = (w + 3) & ~3;
  if (bpp <= 0 || bpp > 8 || (1 << bpp) * planes != 0x100) return 3;
  const int32_t line = (8 / bpp) * bpl * planes;
  const size_t cap = static_cast<size_t>(img.pitch) * static_cast<size_t>(h);
  img.px.assign(cap + static_cast<size_t>(line) + 0x40, 0);   // the decoder writes up to one line past the last row
  int64_t remaining = static_cast<int64_t>(line) * h;
  size_t src = 0x80, dst = 0;
  int32_t x = 0;
  while (remaining != 0) {
    if (src >= data.size()) {
      g.unsup("loadImage: RLE data ends before the image");
      return 7;
    }
    uint8_t b = d[src++];
    uint32_t n = 1;
    if ((b & 0xc0) == 0xc0) {
      n = b & 0x3f;
      if (src >= data.size()) return 7;
      b = d[src++];
    }
    const uint8_t v = static_cast<uint8_t>(b + static_cast<uint8_t>(ofs));
    if (dst + n > img.px.size()) {
      g.unsup("loadImage: RLE run past the image buffer");
      return 7;
    }
    std::memset(img.px.data() + dst, v, n);
    dst += n;
    x += static_cast<int32_t>(n);
    remaining -= n;
    if (x >= line) {
      dst += static_cast<size_t>(static_cast<int64_t>(img.pitch) - x);
      remaining += x - line;
      x = 0;
    }
  }
  img.px.resize(cap);
  // the palette: count entries from index ofs, read from the file's last 0x300 bytes as R, G, B
  const uint8_t *p = d + data.size() - 0x300;
  for (int32_t i = ofs; i < ofs + count; ++i, p += 3) {
    const int32_t k = ((i % 256) + 256) % 256;
    img.pal[static_cast<size_t>(k)] = rgb555(p[0], p[1], p[2]);
  }
  if (ofs != 0 || count < 0x100) g.unsup("loadImage: a partial palette (the other entries are the palette object's)");
  if (SG_NEGCTL(g_negctl_gfx) == 2 && !img.px.empty()) img.pal[img.px[0]] ^= 1;
  return 0;
}

// loadImage(this, name, pal, ofs, count, flags) (c3ee.cpp LoadImage_re): the name gets ".pcx" when no '.' follows its first
// character (AddDefaultExt walks back from the end down to, not including, the first byte). Flag bit 0 decodes into the
// image's own palette cache (a palette argument with it returns 3; roadmap S2.3): the file's entries ofs..ofs + count replace
// the cache's, as flag bit 1's fresh palette does, so a full palette (ofs 0, count 0x100) gives the same words; a partial one
// keeps the cache's other entries, which the backend does not model (reported below).
int load_image(Gfx &g, Ctx &ctx, uint32_t name_va, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img) {
  if (name_va == 0) return 3;
  return load_image_named(g, ctx, vstring(ctx.mem, name_va), pal, ofs, count, flags, img);
}
int load_image_named(Gfx &g, Ctx &ctx, std::string name, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags, Image8 &img) {
  if ((flags & 1) && pal != 0) return 3;
  bool dot = false;
  for (size_t q = name.size(); q > 0; --q)
    if (name[q] == '.') dot = true;   // name[size] is the NUL: never a '.'
  if (!dot) name += vstring(ctx.mem, state_va(ctx, &GameState::f_strpool_4e4218) + 0x1c);   // ".pcx"
  if (!ctx.files().exists(name)) return 6;
  const std::vector<uint8_t> data = ctx.files().read_all(name);
  return decode_pcx8(g, data, ofs, count, (flags & 3) != 0 || pal != 0, img);   // loadImageData gets the palette in use
}

// cropSprite(this, src, x, y, w, h, kind, trim) (c3ee.cpp CropSprite_re), reached through Surface_blit; the source must be
// an 8-bit image. Returns the original's result code; the sprite is removed first (Surface_free) as the original does.
int crop_sprite(Gfx &g, uint32_t self, const Image8 &src, int32_t x, int32_t y, int32_t w, int32_t h, bool trim) {
  g.sprites.erase(self);
  if (src.px.empty()) return 3;
  if (w == 0 || h == 0) return 3;
  if (add32(x, w) > src.w || add32(y, h) > src.h) return 3;
  if (x < 0 || y < 0 || w < 0 || h < 0) {
    g.unsup("Surface_blit: a negative crop rectangle");
    return 3;
  }
  auto row = [&](int32_t r) { return src.px.data() + static_cast<size_t>(y + r) * src.pitch + x; };
  auto clear = [&](const uint8_t *p, int32_t from, int32_t to) {   // bytes [from, to) all 0xff, with trim set
    if (!trim) return false;
    for (int32_t c = from; c < to; ++c)
      if (p[c] != 0xff) return false;
    return true;
  };
  int32_t top = 0;
  while (top < h && clear(row(top), 0, w)) ++top;
  if (top == h) return 9;
  int32_t keep_h = h - top;
  for (int32_t r = h - 1; r >= 0 && clear(row(r), 0, w); --r) --keep_h;
  if (keep_h == 0) return 9;
  int32_t left = w, right = w;
  for (int32_t r = top; r < top + keep_h; ++r) {
    const uint8_t *p = row(r);
    int32_t c = 0;
    while (c < w && trim && p[c] == 0xff) ++c;
    if (c < left) left = c;
    int32_t n = 0;
    for (int32_t k = w; k > 0 && trim && p[k - 1] == 0xff; --k) ++n;
    if (n < right) right = n;
  }
  const int32_t keep_w = w - right - left;
  if (keep_w <= 0) return 9;
  Sprite8 s;
  s.w = keep_w;
  s.h = keep_h;
  s.left = left;
  s.top = top;
  s.cut_w = w;
  s.cut_h = h;
  s.pal = src.pal;
  s.px.resize(static_cast<size_t>(keep_w) * keep_h);
  for (int32_t r = 0; r < keep_h; ++r) std::memcpy(s.px.data() + static_cast<size_t>(r) * keep_w, row(top + r) + left, keep_w);
  g.sprites[self] = std::move(s);
  return 0;
}

// ------------------------------------------------------------------------------------------------ drawing
// jgld Sprite::draw16_1001bc40 at scale 1 (re/match/jgld_blit16.cpp): the sprite's rectangle at (x, y) clipped to the
// destination; bytes >= 0xfe are transparent, the others the sprite palette's RGB555 word.
void draw16(const Sprite8 &s, Surface16 &d, int32_t x, int32_t y) {
  const int32_t l = std::max(x, 0), t = std::max(y, 0);
  const int32_t r = std::min(x + s.w, d.w), b = std::min(y + s.h, d.h);
  const uint8_t key = SG_NEGCTL(g_negctl_gfx) == 3 ? 0xff : 0xfe;
  for (int32_t yy = t; yy < b; ++yy) {
    const uint8_t *sp = s.px.data() + static_cast<size_t>(yy - y) * s.w + (l - x);
    uint16_t *dp = d.px.data() + static_cast<size_t>(yy) * d.w + l;
    for (int32_t xx = l; xx < r; ++xx, ++sp, ++dp)
      if (*sp < key) {
        *dp = s.pal[*sp];
        hud_mark(dp);  // P10C fix 2
      }
  }
}

// ------------------------------------------------------------------------------------------------ the bodies

static uint32_t sref(Ctx &ctx, SpriteRef (GameState::*arr)[14], uint32_t k) { return state_va(ctx, arr) + 0x2c * k; }

// showTitleScreen's button cuts (x, y, w, h in both title button pictures, and the SpriteRef index of the unselected one)
struct TitleButton { int x, y, w, h, unsel; };
static const TitleButton kTitleButtons[7] = {{0x2a, 0x15, 0x144, 0x79, 0},  {0x1a0, 0x1f, 0x16b, 0x8d, 2}, {0x1fe, 0x167, 0x10f, 0x6f, 4},
                                             {0x11f, 0x1da, 0x120, 0x73, 6}, {0xd, 0x17e, 0x11f, 0x73, 8},   {0xa8, 0xc6, 0x1d8, 0xb0, 12},
                                             {0x2d1, 0x210, 0x3a, 0x39, 10}};

// P23C fix 1 (tick_menu.inc, mainLoop.titleHotspotRange = 0; not a body: the menu calls it once per entry): the hover shape of
// the five labelled title buttons. The picture showTitleScreen cuts the highlighted buttons from (its fourth load below, the
// string pool's first name, the same arguments) is decoded again from the game files, without a backend, so the shape is the
// same with or without one attached; for each row of each button's cut rectangle, the first and the last column whose index is
// not 0xff (crop_sprite's transparent index, the pixels the menu's Image_draw leaves out), -1 / -1 for an empty row. rows[2k],
// rows[2k + 1] for row k counted over the buttons in order; at[i] = button i's first row. False (rows empty) when the picture
// cannot be read or a rectangle lies outside it.
bool title_hover_rows(Ctx &ctx, std::vector<int16_t> &rows, int32_t at[5]) {
  rows.clear();
  if (ctx.files_ptr == nullptr) return false;
  Gfx tmp;   // decode_pcx8's reports only
  Image8 img;
  if (load_image(tmp, ctx, state_va(ctx, &GameState::f_strpool_4c89ac) + 0x00, 0, 0, 0x100, 2, img) != 0) return false;
  for (int i = 0; i < 5; ++i) {
    const TitleButton &b = kTitleButtons[i];
    if (b.x < 0 || b.y < 0 || b.x + b.w > img.w || b.y + b.h > img.h) {
      rows.clear();
      return false;
    }
    at[i] = static_cast<int32_t>(rows.size() / 2);
    for (int32_t r = 0; r < b.h; ++r) {
      const uint8_t *p = img.px.data() + static_cast<size_t>(b.y + r) * static_cast<size_t>(img.pitch) + static_cast<size_t>(b.x);
      int32_t lo = -1, hi = -1;
      for (int32_t c = 0; c < b.w; ++c)
        if (p[c] != 0xff) {
          if (lo < 0) lo = c;
          hi = c;
        }
      rows.push_back(static_cast<int16_t>(lo));
      rows.push_back(static_cast<int16_t>(hi));
    }
  }
  return true;
}

// showTitleScreen (c3fb.cpp ShowTitleScreen_re): the drawing part. Its frame Image takes four loads with flags 2; the
// first two are cut into the 8-piece window-frame sets g_spriteRef_562368 / g_spriteRef_59d920 (4 sets, kind 1, no trim),
// the last two into the 7 title buttons, unselected into g_spriteRef_53b7e0[2b], highlighted into [2b + 1] (kind 1, trim).
// The intro movie (playAudioFile) is the video backend's.
static void b_show_title_screen(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  struct Piece { int k, sx, dy, w, h; };
  static const Piece kFrame[8] = {{1, 0, 0, 0x10, 0x10},    {2, 0, 0x11, 0x10, 0x10},     {3, 0, 0x22, 0x10, 0x1a},
                                  {4, 0x11, 0, 0x10, 0x10}, {5, 0x22, 0, 0x1a, 0x10},     {6, 0x22, 0x11, 0x1a, 0x10},
                                  {7, 0x22, 0x22, 0x1a, 0x1a}, {0, 0x11, 0x22, 0x10, 0x1a}};
  const TitleButton (&kButtons)[7] = kTitleButtons;
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c89ac);
  Image8 img;
  auto cut_frames = [&](uint32_t base) {
    for (int i = 0; i < 4; ++i) {
      const int row = i == 1 ? 200 : i == 2 ? 100 : 0;   // frameRow
      for (const Piece &f : kFrame)
        crop_sprite(g, base + 0x160u * i + 0x2cu * f.k, img, f.sx, row + f.dy, f.w, f.h, false);
    }
  };
  if (load_image(g, ctx, pool + 0x48, 0, 0, 0x100, 2, img) == 0) cut_frames(state_va(ctx, &GameState::g_spriteRef_562368));
  if (load_image(g, ctx, pool + 0x2c, 0, 0, 0x100, 2, img) == 0) cut_frames(state_va(ctx, &GameState::g_spriteRef_59d920));
  if (load_image(g, ctx, pool + 0x14, 0, 0, 0x100, 2, img) == 0)
    for (const TitleButton &b : kButtons)
      crop_sprite(g, sref(ctx, &GameState::g_spriteRef_53b7e0, b.unsel), img, b.x, b.y, b.w, b.h, true);
  if (load_image(g, ctx, pool + 0x00, 0, 0, 0x100, 2, img) == 0)
    for (const TitleButton &b : kButtons)
      crop_sprite(g, sref(ctx, &GameState::g_spriteRef_53b7e0, b.unsel + 1), img, b.x, b.y, b.w, b.h, true);
}

// loadImage(this, name, pal, ofs, count, flags)
static void b_load_image(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  Image8 img;
  if (load_image(g, ctx, a[1], a[2], static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]), a[5], img) == 0)
    g.images[a[0]] = std::move(img);
}

// Surface_blit(this, src, x, y, w, h, kind, trim) (c3h.cpp SurfaceBlit_re): an 8-bit source -> cropSprite
static void b_surface_blit(Gfx &g, Ctx &, const uint32_t *a, int) {
  auto it = g.images.find(a[1]);
  if (it == g.images.end()) {
    g.unsup("Surface_blit: a source the backend has no image for");
    return;
  }
  crop_sprite(g, a[0], it->second, static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]),
              static_cast<int32_t>(a[5]), static_cast<uint8_t>(a[7]) != 0);
}

// A 1:1 StretchBlt of an 8-bit image's rectangle (x, y, w, h) to (x2, y2) on a 16-bit surface, both clipped: each pixel the
// image's colour-table word (GDI converts the 8-bit DIB to RGB555 by the same truncation as Palette::apply)
void copy8(const Image8 &s, Surface16 &d, int32_t x, int32_t y, int32_t w, int32_t h, int32_t x2, int32_t y2) {
  for (int32_t r = 0; r < h; ++r) {
    const int32_t sy = y + r, dy = y2 + r;
    if (sy < 0 || sy >= s.h || dy < 0 || dy >= d.h) continue;
    for (int32_t c = 0; c < w; ++c) {
      const int32_t sx = x + c, dx = x2 + c;
      if (sx < 0 || sx >= s.w || dx < 0 || dx >= d.w) continue;
      uint16_t &o = d.px[static_cast<size_t>(dy) * static_cast<size_t>(d.w) + static_cast<size_t>(dx)];
      o = s.pal[s.px[static_cast<size_t>(sy) * static_cast<size_t>(s.pitch) + static_cast<size_t>(sx)]];
      hud_mark(&o);  // P10C fix 2
    }
  }
}

// Surface_blit3(this, dst, x, y, w, h, x2, y2, w2, h2) (c3h.cpp SurfaceBlit3_re): jgld Surface::stretchTo, StretchBlt SRCCOPY
// of this's rectangle onto dst's. Modelled: an 8-bit image onto the root surface at the same size (a copy through the
// image's colour table, which GDI converts to RGB555 by the same truncation as Palette::apply).
static void b_surface_blit3(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  auto it = g.images.find(a[0]);
  if (it == g.images.end() || a[1] != g.root_va(ctx) || a[4] != a[8] || a[5] != a[9]) {
    // roadmap S3.6: a 16-bit source, another destination or a scaled rectangle (gfx_hud.cpp)
    if (gfxh::surface_blit3(g, ctx, a)) return;
    g.unsup("Surface_blit3: a source or destination the backend does not model");
    return;
  }
  const int32_t x = static_cast<int32_t>(a[2]), y = static_cast<int32_t>(a[3]), w = static_cast<int32_t>(a[4]),
                h = static_cast<int32_t>(a[5]), x2 = static_cast<int32_t>(a[6]), y2 = static_cast<int32_t>(a[7]);
  copy8(it->second, g.root, x, y, w, h, x2, y2);
  g.note_root_write(x2, y2, w, h);
}

// Image_draw(this, surf, x, y, mask) (c3df.cpp ImageDraw_re): at (m_20 * t0 / t2 + x, m_24 * t1 / t2 + y) with the
// display's scale triple, (1, 1, 1) unless Display::setScale changed it (any other scale is reported; it is restored to 1 by every
// caller the menu reaches), through Sprite::drawB -> draw16_1001bc40 for an 8-bit sprite on the 16-bit root surface.
static void b_image_draw(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  // the display's blit scale (Display::setScale, gfx_prim_fill.cpp; roadmap S2.4): drawB's blitters scale by nx / den and
  // ny / den, which the backend does not model
  // roadmap S3.5: another scale, or a sprite whose palette word the scene family holds, is the scene family's (drawB under
  // the display scale, gfx_scene.cpp gfxs::image_draw)
  if (gfxs::image_draw(g, ctx, a)) return;
  if (!gfx_is_local_key(a[0]) && ctx.mem.load<uint32_t>(a[0] + 8) != 0) {
    g.unsup("Image_draw: the load arm (a file name at +8)");
    return;
  }
  if (a[4] != 0 || a[1] != g.root_va(ctx)) {
    g.unsup("Image_draw: a mask or a destination other than the root surface");
    return;
  }
  auto it = g.sprites.find(a[0]);
  if (it == g.sprites.end()) {
    g.unsup("Image_draw: a SpriteRef the backend has no sprite for");
    return;
  }
  const Sprite8 &s = it->second;
  int32_t x = add32(s.left, static_cast<int32_t>(a[2]));
  const int32_t y = add32(s.top, static_cast<int32_t>(a[3]));
  if (SG_NEGCTL(g_negctl_gfx) == 1) x = add32(x, 1);
  gfxs::GroundSnap gsnap;   // P23D fix 8 (gfx_scene.h): inside GfxGroundScope the image is also kept for the hr plane (drawB)
  const bool gr = g.hud.ground_depth > 0 && gfxs::ground_snap(g, s, x, y, 1, 1, 1, gsnap);
  draw16(s, g.root, x, y);
  if (gr) gfxs::ground_art(g, s, s.pal.data(), 5, x, y, 1, 1, 1, gsnap);
}

// setDrawTarget(this, surf, a, b, c) (c3k.cpp SetDrawTarget_re): the font words of the root surface
static void b_set_draw_target(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (a[0] != g.root_va(ctx)) {
    g.unsup("setDrawTarget: a target other than the root surface");
    return;
  }
  if (a[1] == 0) return;
  if (ctx.mem.load<uint32_t>(a[1] + 4) != 0) g.fonts[0] = a[1];
  g.fonts[1] = a[2];
  g.fonts[3] = a[4];
  g.fonts[2] = a[3];
}

// The text drawers over drawRichText's run drawer (c3et.cpp): the baseline at y + the largest +0x10 of fonts 0..2,
// SetTextColor from a direct colour word (bit 31: r = (c >> 7) & 0xf8, g = (c >> 2) & 0xf8, b = (c << 3) & 0xff) and TextOutA
// per tab-separated run with '_' drawn as a space. drawCenteredText(text, x, y, colour) (c3dq.cpp): setQuad(Root, colour, -1,
// 2, 2), then drawCentered2(Root, text, x, y, 0, strlen) (c3df.cpp): x + (0 - width) / 2 with width = drawMarkupRun's (one
// measure of the whole text in the base font when the text holds no markup token). drawTextWithColor(text, x, y, colour)
// (c3dq.cpp): the same setQuad, then drawText (c3ek.cpp: no font or a font whose +4 is 0 draws nothing) -> drawRichText at x.
// Modelled: text without a markup or tab character, a direct colour, no shadow (setQuad's -1).
// P23F: the 800-space ink columns [c0, c1) of `text` drawn from origin x by the provider (TA_BASELINE), measured on a scratch
// surface (nothing of the root, the back buffer or the GameState is touched); false when the provider draws no ink
static bool text_ink_columns(Ctx &ctx, TextMetrics &tm, uint32_t font, const std::string &text, int32_t x, int32_t ascent,
                             int32_t &c0, int32_t &c1) {
  const int32_t a = std::max<int32_t>(ascent, 8);
  const int32_t pad = 2 * a + 8, n = static_cast<int32_t>(text.size());
  const int32_t sw = 2 * pad + n * 2 * a, sh = 3 * a + 8, base = 2 * a + 4;
  if (n <= 0 || sw > 16384) return false;
  std::vector<uint16_t> s(static_cast<size_t>(sw) * static_cast<size_t>(sh), 0);
  if (!tm.draw_glyphs(ctx, font, text, pad, base, 0xffffffu, s.data(), sw, sh)) return false;
  int32_t lo = sw, hi = -1;
  for (int32_t yy = 0; yy < sh; ++yy)
    for (int32_t xx = 0; xx < sw; ++xx)
      if (s[static_cast<size_t>(yy) * static_cast<size_t>(sw) + static_cast<size_t>(xx)] != 0) {
        lo = std::min(lo, xx);
        hi = std::max(hi, xx);
      }
  if (hi < 0) return false;
  c0 = add32(x, lo - pad);
  c1 = add32(x, hi + 1 - pad);
  return true;
}
uint32_t lang_fit_font(Ctx &ctx, uint32_t font, uint32_t en_va, int32_t en_n, uint32_t tr_va, int32_t tr_n, int32_t bound_w) {
  if (lang::language() == lang::kEnglish || font == 0 || en_va == 0 || tr_va == 0 || en_n <= 0 || tr_n <= 0) return font;
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) return font;
  int32_t en_w = 0, tr_w = 0;
  if (!tm->glyph_width(ctx, font, en_va, en_n, en_w) || !tm->glyph_width(ctx, font, tr_va, tr_n, tr_w)) return font;
  const int32_t bound = bound_w > 0 ? bound_w : add32(en_w + en_w / 10, 2);
  if (tr_w <= bound) return font;
  GameFont me;
  try {
    me = game_font(ctx, font);
  } catch (...) {
    return font;
  }
  // the game's smaller fonts of this face and style, largest first
  std::vector<std::pair<int32_t, uint32_t>> ladder;
  for (size_t i = 0; i < game_font_count(); ++i) {
    const uint32_t r = game_font_ref(ctx, i);
    if (r == font || ctx.mem.load<uint32_t>(r + 4) == 0) continue;
    try {
      const GameFont f = game_font(ctx, r);
      if (f.face == me.face && f.style == me.style && f.size < me.size && f.size >= 9) ladder.emplace_back(f.size, r);
    } catch (...) {
    }
  }
  std::sort(ladder.begin(), ladder.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
  uint32_t pick = font;
  for (const auto &l : ladder) {
    pick = l.second;
    int32_t w = 0;
    if (tm->glyph_width(ctx, pick, tr_va, tr_n, w) && w <= bound) break;
  }
  return pick;
}

void draw_text_string(Gfx &g, Ctx &ctx, std::string text, uint32_t va, int32_t x0, int32_t y0, uint32_t colour, bool centre,
                      const char *who) {
  g.ensure_root(ctx);
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) {
    g.unsup(std::string(who) + ": no text provider");
    return;
  }
  if (text.empty()) return;
  // P24D: the text in the current language (a text at an address is redirected to its translation, so the provider measures
  // the translation; a text the caller built is looked up as it is)
  uint32_t en_va = 0;  // P24D fix 5d: the English text a translation replaced (its width bounds the box)
  int32_t en_n = 0;
  if (lang::language() != lang::kEnglish) {
    if (va != 0) {
      const uint32_t nva = lang::redirect(ctx, va);
      if (nva != va && text == vstring(ctx.mem, va)) {
        en_va = va;
        en_n = static_cast<int32_t>(text.size());
        va = nva;
        text = vstring(ctx.mem, va);
      }
    } else {
      text = lang::tr(text);
    }
  }
  // roadmap S3.7: markup, tabs and run-palette colours are the text family's (gfx_text.cpp gfxt::draw_string)
  bool rich = !(colour & 0x80000000u);
  for (char c : text)
    if (c == '{' || c == '}' || c == '[' || c == ']' || c == '$' || c == '=' || c == '^' || c == '\t') rich = true;
  if (rich) {
    gfxt::draw_string(g, ctx, text, x0, y0, colour, centre, who);
    return;
  }
  uint32_t font = g.fonts[0];
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return;   // drawText's guard: no font, nothing drawn
  if (en_va != 0) font = lang_fit_font(ctx, font, en_va, en_n, va, static_cast<int32_t>(text.size()), g.lang_box_w);  // P24D fix 5d
  int32_t width = 0, ascent = 0;
  if (centre && (va == 0 || !tm->glyph_width(ctx, font, va, static_cast<int32_t>(text.size()), width))) {
    g.unsup(std::string(who) + ": the provider cannot measure");
    return;
  }
  for (int i = 0; i < 3; ++i) {
    int32_t v = 0;
    if (g.fonts[i] && tm->glyph_ascent(ctx, g.fonts[i], v) && (i == 0 || v > ascent)) ascent = v;
  }
  for (char &c : text)
    if (c == '_') c = ' ';
  const int32_t x = centre ? add32(x0, (0 - width) / 2) : x0;
  const int32_t y = add32(y0, ascent);
  const uint32_t r = (colour >> 7) & 0xf8, gr = (colour >> 2) & 0xf8, b = (colour << 3) & 0xff;
  const uint32_t ref = r | (gr << 8) | (b << 16);
  // P24D fix 5 option 2: a translation the game's font ladder could not bring within its bound (lang_fit_font: 10 % + 2 pixels
  // over the English it replaces) is squeezed to that bound: each byte drawn by itself at its pen position scaled by bound /
  // width (the glyphs keep their shapes, the spacing tightens), centred on the same point. Every byte is its own text run, so
  // the window-resolution text and the GPU text redraw each at its own origin. Never in English, never when the text fits.
  // (only where the caller gave the box with text.box: the English width is a lower bound of a box, too loose to squeeze to)
  if (en_va != 0 && va != 0 && !text.empty() && g.lang_box_w > 0) {
    int32_t en_w = 0, tr_w = 0;
    const int32_t n = static_cast<int32_t>(text.size());
    if (tm->glyph_width(ctx, g.fonts[0], en_va, en_n, en_w) && tm->glyph_width(ctx, font, va, n, tr_w)) {
      const int32_t bound = g.lang_box_w > 0 ? g.lang_box_w : add32(en_w + en_w / 10, 2);  // text.box when the caller gave one
      if (tr_w > bound && tr_w > 0 && bound > 0) {
        const double k = std::max(0.6, static_cast<double>(bound) / static_cast<double>(tr_w));  // at most to 60 %
        lang::note("squeeze", text, true);  // SG_LANG_DUMP: the texts squeezed
        const int32_t xs = centre ? sub32(x0, bound / 2) : x0;
        for (int32_t i = 0; i < n; ++i) {
          if (text[static_cast<size_t>(i)] == ' ') continue;
          int32_t pre = 0;
          if (i > 0 && !tm->glyph_width(ctx, font, va, i, pre)) pre = 0;
          const int32_t px = add32(xs, static_cast<int32_t>(std::lround(pre * k)));
          const std::string one(1, text[static_cast<size_t>(i)]);
          TextRunScope t1;
          int32_t lo = 0, hi = -1;
          if (!tm->glyph_rows(ctx, font, y, g.root.h, lo, hi)) lo = 0, hi = -1;
          text_run_begin(g, g.root.px.data(), g.root.w, g.root.h, t1, lo, hi);
          if (!tm->draw_glyphs(ctx, font, one, px, y, ref, g.root.px.data(), g.root.w, g.root.h))
            g.unsup(std::string(who) + ": the provider cannot draw");
          const int32_t clip1[4] = {0, 0, g.root.w, g.root.h};
          text_run_end(g, ctx, t1, g.root.px.data(), g.root.w, g.root.h, 0, 0, clip1, font, one, px, y, ref, 0);
        }
        return;
      }
    }
  }
  TextRunScope ts;  // P9C: the run (inert with text capture off)
  // P24C fix 1: only the rows the provider's TextOutA can change (TextMetrics::glyph_rows), not the whole surface
  int32_t band_lo = 0, band_hi = -1;
  if (!tm->glyph_rows(ctx, font, y, g.root.h, band_lo, band_hi)) band_lo = 0, band_hi = -1;
  text_run_begin(g, g.root.px.data(), g.root.w, g.root.h, ts, band_lo, band_hi);
  // P22E item 7: a cursor pass's text: its whole columns (the centred width measured above, else measured here)
  // (P23E fix 4b: and the drag ruler's yards label, drawn inside P6_Cursor's GfxWideLineScope(kGfxWideRuler), after the queue)
  const bool cursor_text = g.hud.edge_on && ((g.hud.cursor_sprites && g.hud.cursor_depth > 0) ||
                                             (g.hud.wide_lines & g.hud.wide_scope & kGfxWideRuler) != 0);
  if (ts.on && cursor_text && !g.text.wide && va != 0) {
    int32_t tw = width;
    if (centre || tm->glyph_width(ctx, font, va, static_cast<int32_t>(text.size()), tw)) {
      ts.ink = tw > 0;
      ts.ink_x0 = x;
      ts.ink_x1 = add32(x, tw);
    }
  }
  // P23F (gfx.h gfx_set_frame_edge_text): a frame text over the course (the root holds the frame's terrain picture, no HUD layer
  // has begun) whose columns reach past the root's left or right edge: its whole columns, the advance the provider measures (or,
  // for a string with no address, its ink drawn on a scratch surface), as P22E's cursor-pass texts. A golfer name label of the
  // sprite queue (drawQueuedSprite.label) is left out: the wide scene draws it whole beside the frame (a world run, moving with
  // its figure), and a second copy here would not move with it (P23F fix 3: told by TextCapture::queue_label, set around the
  // label's frame copy, whether or not the entry has a mover source)
  // (P23F fix 2: and P11C's course tip held past the frame's edge, drawn after the HUD's layers began)
  if (ts.on && !cursor_text && g.hud.edge_frame && g.root_terrain && !g.text.queue_label &&
      (!g.hud.any || g.hud.tip_edge) && !g.text.wide && g.root.w == GfxTextRun::kRootW) {
    int32_t c0 = x, c1 = x;
    bool got = false;
    if (va != 0) {
      int32_t tw = width;
      got = (centre || tm->glyph_width(ctx, font, va, static_cast<int32_t>(text.size()), tw)) && tw > 0;
      c1 = add32(x, tw);
    } else if (x < 0 || add32(x, mul32(static_cast<int32_t>(text.size()), add32(ascent, ascent))) > g.root.w) {
      got = text_ink_columns(ctx, *tm, font, text, x, ascent, c0, c1);
    }
    if (got && (c0 < 0 || c1 > g.root.w)) {
      ts.ink = true;
      ts.ink_x0 = c0;
      ts.ink_x1 = c1;
      ts.frame = true;
    }
  }
  lang::note("glyph", text, false);  // P24D: SG_LANG_DUMP's record of what reaches the provider
  if (!tm->draw_glyphs(ctx, font, text, x, y, ref, g.root.px.data(), g.root.w, g.root.h))
    g.unsup(std::string(who) + ": the provider cannot draw");
  const int32_t clip[4] = {0, 0, g.root.w, g.root.h};
  text_run_end(g, ctx, ts, g.root.px.data(), g.root.w, g.root.h, 0, 0, clip, font, text, x, y, ref, 0);
}
void draw_text(Gfx &g, Ctx &ctx, const uint32_t *a, bool centre, const char *who) {
  if (a[0] == 0) return;
  draw_text_string(g, ctx, vstring(ctx.mem, a[0]), a[0], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), a[3], centre, who);
}
// Both wrappers start with Widget_setQuad(f_stateptr_4c1570[0], colour, -1, 2, 2) (c3dq.cpp), whose words stay on the target for
// the drawers that read them later (drawText, drawStringAt, ...): the prim_text family's body keeps them (roadmap S2.10, U-1293)
static void set_quad_root(Gfx &g, Ctx &ctx, uint32_t colour, uint32_t shadow, uint32_t dx, uint32_t dy) {
  if (SG_NEGCTL(g_negctl_gfx) == 7) return;
  const GfxBody q = find_body("Widget_setQuad");
  const uint32_t qa[5] = {ctx.gs().f_stateptr_4c1570[0], colour, shadow, dx, dy};
  if (q) q(g, ctx, qa, 5);
}
static void b_draw_centered_text(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  set_quad_root(g, ctx, a[3], 0xffffffffu, 2, 2);
  draw_text(g, ctx, a, true, "drawCenteredText");
}
static void b_draw_text_4049d0(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  set_quad_root(g, ctx, a[3], 0xffffffffu, 2, 2);
  draw_text(g, ctx, a, false, "drawTextWithColor");
}

// P13B item 13: drawPanelBackdrop.barOnTerrainOnly (1 = the original: the bar is the terrain view's line only)
int g_defect_backdrop_gl_only = 1;
bool chareditor_open(const Gfx &g);   // gfx_chareditor.cpp (this file's gfxb namespace is open here)
// drawPanelBackdrop(x, y) (c3dw.cpp DrawPanelBackdrop_re): setDrawTarget(Root, g_fontRef_519fd8, 0, 0, 0); w = 3 * strlen of
// the text buffer; half = x raised to w and lowered to 0x320 - w (signed); drawMapLine(half - w, y, half + w, y, 0x80000000,
// 0xa, 5) = Terrain::drawLine, an OpenGL line in the terrain view's GL context (re/match/terrain_drawline.cpp), which is not
// jgld's back buffer: the location screen's tip frame shows no line (U-1071); then drawCenteredText(text buffer, half, y - 5,
// 0x80007fff).
static void b_panel_backdrop(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  const uint32_t font = state_va(ctx, &GameState::g_fontRef_519fd8);
  if (ctx.mem.load<uint32_t>(font + 4) != 0) g.fonts[0] = font;
  g.fonts[1] = g.fonts[2] = g.fonts[3] = 0;
  // P24D fix 5: 3 per byte of the text in the current language (the one drawn below)
  const int32_t w = mul32(static_cast<int32_t>(vstrlen(ctx.mem, lang::redirect(ctx, text))), 3);
  int32_t half = static_cast<int32_t>(a[0]);
  // P23F fix 2 (gfx_text.cpp hover_help_terrain): P11C's course tip at a cursor near or past the frame's edge is held inside the
  // window's columns (the side columns' width wide_in each side) instead of the 800 frame's
  const int32_t side = g.hud.tip_edge ? g.hud.wide_in : 0;  // (P23F fix 3: the window's columns, as the text's edge columns)
  if (half < sub32(w, side)) half = sub32(w, side);
  if (half > add32(sub32(0x320, w), side)) half = add32(sub32(0x320, w), side);
  // roadmap S3.8: the bar is Terrain::drawLine through the overlay body (terrain_overlay.h); without a GL context it draws nothing
  if (GfxBody bar = gfxb::find_body("drawMapLine")) {
    const uint32_t la[7] = {static_cast<uint32_t>(sub32(half, w)), a[1], static_cast<uint32_t>(add32(half, w)), a[1], 0x80000000u, 0xau, 5u};
    bar(g, ctx, la, 7);
  }
  // P13B item 13 (drawPanelBackdrop.barOnTerrainOnly = 0): while the character editor is open, the bar the call asks for
  // (colour 0x80000000, the text's row) is also filled into the 2D target, so the editor's white tips read over its light art,
  // where the terrain view's line never shows (in game the line shows: the HUD tips are left as they are); it covers the
  // measured text with 4 pixels each side, at least the call's half - w .. half + w, and the rows from 2 above the text's top
  // (y - 5) to 2 under its descenders (a third of the ascent below the baseline)
  if (g_defect_backdrop_gl_only == 0 && gfxb::chareditor_open(g)) {
    g.ensure_root(ctx);
    int32_t asc = 0, tw = 0;
    TextMetrics *tm = text_metrics(ctx);
    const uint32_t f = g.fonts[0];
    const int32_t n = static_cast<int32_t>(vstrlen(ctx.mem, text));
    if (tm && f && n > 0 && tm->glyph_ascent(ctx, f, asc) && tm->glyph_width(ctx, f, text, n, tw)) {
      const int32_t hw = std::max(w, add32(tw / 2, 4));
      const int32_t top = sub32(static_cast<int32_t>(a[1]), 7);
      const int32_t bot = add32(add32(sub32(static_cast<int32_t>(a[1]), 5), add32(asc, asc / 3)), 2);
      gfxf::fill_rect16(g.root, sub32(half, hw), top, add32(half, hw), bot, 0);
    }
  }
  const uint32_t args[4] = {text, static_cast<uint32_t>(half), static_cast<uint32_t>(sub32(static_cast<int32_t>(a[1]), 5)),
                            0x80007fffu};
  draw_text(g, ctx, args, true, "drawPanelBackdrop");
}

// ---- P9C: text runs (gfx.h GfxTextRun, gfx_body.h Gfx::TextCapture)
void text_run_begin(Gfx &g, const uint16_t *px, int32_t w, int32_t h, TextRunScope &s, int32_t lo, int32_t hi) {
  s.on = g.text.on && w > 0 && h > 0;
  if (hi < lo) {  // the whole surface
    lo = 0;
    hi = h;
  }
  s.lo = std::max<int32_t>(0, std::min(lo, h));
  s.hi = std::max<int32_t>(s.lo, std::min(hi, h));
  if (s.on)
    s.snap.assign(px + static_cast<size_t>(s.lo) * static_cast<size_t>(w), px + static_cast<size_t>(s.hi) * static_cast<size_t>(w));
}
void text_run_end(Gfx &g, Ctx &ctx, TextRunScope &s, const uint16_t *px, int32_t w, int32_t h, int32_t ox, int32_t oy,
                  const int32_t clip[4], uint32_t fontref, const std::string &text, int32_t x, int32_t y, uint32_t colorref,
                  uint8_t target) {
  if (!s.on || !g.text.on || s.hi > h || s.snap.size() != static_cast<size_t>(w) * static_cast<size_t>(s.hi - s.lo)) return;
  int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int32_t yy = s.lo; yy < s.hi; ++yy) {  // P24C fix 1: the snapshotted rows (no other row changed)
    const uint16_t *a = s.snap.data() + static_cast<size_t>(yy - s.lo) * w, *b = px + static_cast<size_t>(yy) * w;
    if (std::memcmp(a, b, static_cast<size_t>(w) * 2) == 0) continue;
    for (int32_t xx = 0; xx < w; ++xx)
      if (a[xx] != b[xx]) {
        x0 = std::min(x0, xx);
        x1 = std::max(x1, xx);
      }
    y0 = std::min(y0, yy);
    y1 = yy;
  }
  if (x1 < 0) {  // the call changed no pixel: nothing to redraw, unless its whole ink lies past the root's edge (P22E item 7)
    const bool edge_only = s.ink && target == 0 && !g.text.wide && ox == 0 && oy == 0 && w == GfxTextRun::kRootW && clip[0] <= 0 &&
                           clip[2] >= w && (s.ink_x1 <= 0 || s.ink_x0 >= w);
    if (!edge_only) return;
    GameFont ef;
    try {
      ef = game_font(ctx, fontref);
    } catch (const MissingResource &) {
      ++g.text.stats.no_font;
      return;
    }
    const int32_t em = std::max<int32_t>(3, ef.size / 3);
    GfxTextRun e;
    e.fontref = fontref;
    e.face = ef.face;
    e.size = ef.size;
    e.style = ef.style;
    e.colorref = colorref;
    e.x = x;
    e.y = y;
    e.text = text;
    e.x0 = e.x1 = s.ink_x1 <= 0 ? 0 : w;  // an empty box at the edge: every column it shows lies outside the root
    // the rows: the baseline TA_BASELINE origin y, the font's size above it and a third below, the box's margin around them
    e.y0 = std::max<int32_t>(0, y - ef.size - em);
    e.y1 = std::min<int32_t>(h, y + ef.size / 3 + em + 1);
    // P23F fix 3: the edge columns end where the window does (HudCapture::wide_in): the presenter's course picture at the camera
    // slide's offset 0 holds no column past it, so a part beyond it would be drawn by the CPU path only, over the slide's gap
    e.ex0 = std::max(s.ink_x0 - em, -g.hud.wide_in);
    e.ex1 = std::min(s.ink_x1 + em, w + g.hud.wide_in);
    e.layer = -1;
    e.target = 0;
    if (e.y1 <= e.y0 || e.ex1 <= e.ex0 || (e.x0 == 0 && e.ex0 >= 0) || (e.x0 == w && e.ex1 <= w)) return;
    if (s.frame) {  // P23F: a frame text's, kept until the next frame's terrain picture
      g.text.frame_edge_runs.push_back(std::move(e));
      constexpr size_t kMaxFrameEdge = 512;  // a frame with no terrain picture to clear them (no world layer): the oldest go first
      if (g.text.frame_edge_runs.size() > kMaxFrameEdge) g.text.frame_edge_runs.erase(g.text.frame_edge_runs.begin());
      ++g.text.stats.frame_edge;
      return;
    }
    g.text.edge_runs.push_back(std::move(e));
    ++g.text.stats.edge;
    return;
  }
  ++g.text.stats.captured;
  GameFont gf;
  try {
    gf = game_font(ctx, fontref);
  } catch (const MissingResource &) {
    ++g.text.stats.no_font;  // a font the provider's table does not hold: the run stays in 800-space
    return;
  }
  // the box: the changed pixels plus a margin, inside the clip
  // (a margin of a third of the font size, at least 3: at another size the ink of the same string reaches past the 800-space one,
  // the dots of an i above it at 1280 x 720; the margin is only drawn where it still shows the run's own after words)
  const int32_t kMargin = std::max<int32_t>(3, gf.size / 3);
  const int32_t cx0 = std::max(clip[0], 0), cy0 = std::max(clip[1], 0), cx1 = std::min(clip[2], w), cy1 = std::min(clip[3], h);
  x0 = std::max(x0 - kMargin, cx0);
  y0 = std::max(std::max(y0 - kMargin, cy0), s.lo);  // (P24C fix 1: inside the snapshot's rows; the band holds the margin)
  x1 = std::min(x1 + 1 + kMargin, cx1);
  y1 = std::min(std::min(y1 + 1 + kMargin, cy1), s.hi);
  if (x1 <= x0 || y1 <= y0) return;
  Gfx::TextCapture::Run r;
  r.info.fontref = fontref;
  r.info.face = gf.face;
  r.info.size = gf.size;
  r.info.style = gf.style;
  r.info.colorref = colorref;
  if (g.text.wide && target == 0) {  // the wide scene's copy of a frame text (gfx_scene.cpp on_wide)
    ox += g.text.wide_ox;
    target = 2;
  }
  r.info.x = add32(x, ox);
  r.info.y = add32(y, oy);
  r.info.text = text;
  r.info.target = target;
  r.info.x0 = x0 + ox;
  r.info.y0 = y0 + oy;
  r.info.x1 = x1 + ox;
  r.info.y1 = y1 + oy;
  r.info.mover = gfx_mover_label();  // P12A fix 2: a golfer name label moves with its figure between game frames
  r.frame = g.text.frame_gen;        // P23F fix 3
  // P22E item 7 (gfx.h gfx_set_hud_edge): a cursor pass's text on the root whose whole columns the root's edge cut, and whose
  // changed pixels reach that edge's margin: the presenter may draw it past the edge (GfxTextRun::ex0 / ex1)
  if (s.ink && target == 0 && ox == 0 && w == GfxTextRun::kRootW && clip[0] <= 0 && clip[2] >= w) {
    // (P23F fix 3: not past the window's columns, HudCapture::wide_in, as the edge runs above)
    const int32_t e0 = std::max(s.ink_x0 - kMargin, -g.hud.wide_in), e1 = std::min(s.ink_x1 + kMargin, w + g.hud.wide_in);
    if ((e0 < 0 && x0 == 0) || (e1 > w && x1 == w)) {
      r.info.ex0 = e0;
      r.info.ex1 = e1;
      ++(s.frame ? g.text.stats.frame_edge : g.text.stats.edge);
    }
  }
  const size_t bw = static_cast<size_t>(x1 - x0);
  r.before.resize(bw * static_cast<size_t>(y1 - y0));
  r.after.resize(r.before.size());
  for (int32_t yy = y0; yy < y1; ++yy)
    for (int32_t xx = x0; xx < x1; ++xx) {
      const size_t i = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
      const size_t j = static_cast<size_t>(yy - y0) * bw + static_cast<size_t>(xx - x0);
      r.before[j] = s.snap[i - static_cast<size_t>(s.lo) * static_cast<size_t>(w)];
      r.after[j] = px[i];
    }
  // the HUD segment it belongs to (P9B's layers are made on the root only)
  r.seg = target == 0 && g.hud.on && g.hud.any ? g.hud.seg : -1;
  std::vector<Gfx::TextCapture::Run> &sink = target == 2 ? g.text.wide_runs : g.text.sink ? *g.text.sink : g.text.live;
  // A repeat: a text the game draws again over its own last copy each frame (the status meters, a label in a box nothing
  // repaints) leaves, once the blend has settled, a run equal to an earlier one in every word of before and after. The earlier
  // copy is removed and the new one kept at the end (its draw position and segment): the peel chain is unchanged (the copy
  // before it ends in the same words) and the list stays bounded.
  for (size_t k = sink.size(); k-- > 0;) {
    const Gfx::TextCapture::Run &e = sink[k];
    if (e.info.x0 == r.info.x0 && e.info.y0 == r.info.y0 && e.info.x1 == r.info.x1 && e.info.y1 == r.info.y1 &&
        e.info.x == r.info.x && e.info.y == r.info.y && e.info.colorref == r.info.colorref && e.info.fontref == r.info.fontref &&
        e.info.target == r.info.target && e.info.text == r.info.text && e.before == r.before && e.after == r.after) {
      sink.erase(sink.begin() + static_cast<std::ptrdiff_t>(k));
      ++g.text.stats.merged;
      break;
    }
  }
  sink.push_back(std::move(r));
  constexpr size_t kMaxLive = 4096;  // a screen that draws text forever without covering it: the oldest go first
  if (sink.size() > kMaxLive) {
    g.text.stats.dropped += sink.size() - kMaxLive;
    sink.erase(sink.begin(), sink.begin() + static_cast<std::ptrdiff_t>(sink.size() - kMaxLive));
  }
}

namespace {
std::vector<GfxTextPeelRun> peel_runs(const std::vector<Gfx::TextCapture::Run> &rs, const std::vector<size_t> &order) {
  std::vector<GfxTextPeelRun> out;
  out.reserve(order.size());
  for (size_t k : order) {
    const Gfx::TextCapture::Run &r = rs[k];
    GfxTextPeelRun p;
    p.x0 = r.info.x0;
    p.y0 = r.info.y0;
    p.x1 = r.info.x1;
    p.y1 = r.info.y1;
    p.before = r.before.data();
    p.after = r.after.data();
    out.push_back(p);
  }
  return out;
}
// A segment of the HUD capture closed (a layer, or the centred gap before / after one): its runs are peeled over the root as it
// is now. `layer` is the index of the layer the segment made in g.hud.layers (-1 = none: its runs are not shown on the layered
// path), `ref` the root at the segment's start (a pixel equal to it without the glyph was not drawn by the layer: transparent).
void text_close_segment(Gfx &g, int64_t seg, int32_t layer, const std::vector<uint16_t> &ref) {
  if (!g.text.on) return;
  std::vector<Gfx::TextCapture::Run> &live = g.text.live;
  std::vector<size_t> order;
  for (size_t k = 0; k < live.size(); ++k)
    if (live[k].seg == seg && live[k].info.target == 0) order.push_back(k);
  if (order.empty()) return;
  if (layer < 0 || static_cast<size_t>(layer) >= g.hud.layers.size()) {
    for (size_t k : order) {
      live[k].info.layer = -2;
      live[k].info.vis_layer.clear();
    }
    return;
  }
  const GfxHudLayer &L = g.hud.layers[static_cast<size_t>(layer)];
  std::vector<uint16_t> cur = g.root.px;
  std::vector<std::vector<uint8_t>> vis;
  gfx_text_peel(cur, g.root.w, g.root.h, peel_runs(live, order), vis);
  for (size_t n = 0; n < order.size(); ++n) {
    live[order[n]].info.layer = layer;
    live[order[n]].info.vis_layer = std::move(vis[n]);
  }
  GfxTextLayerPatch patch;
  patch.layer = layer;
  const int32_t bw = L.x1 - L.x0;
  const bool rec = g.hud.rec_gen.size() == g.root.px.size();
  // P10C fix 2: a pixel the panel's primitives wrote before the glyph is the panel's even when its word equals the mark (the tool
  // names drawn over the palette's tree icons left transparent spots there)
  const bool wr = (L.id != kGfxLayerCentred || g.hud.wr_gap) && g.hud.wr_on && g.hud.wr_gen.size() == g.root.px.size();  // P13D item 17
  for (int32_t y = L.y0; y < L.y1; ++y)
    for (int32_t x = L.x0; x < L.x1; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(g.root.w) + static_cast<size_t>(x);
      if (cur[i] == g.root.px[i]) continue;
      GfxLayerPx p;
      if (i < ref.size() && cur[i] == ref[i] && !(wr && g.hud.wr_gen[i] == g.hud.gen)) {
        p.kind = 0;
        p.k = kGfxTextCoverMark;
      } else if (rec && g.hud.rec_gen[i] == g.hud.gen && g.hud.rec[i].kind >= 2 && g.hud.rec[i].px == cur[i]) {
        p = g.hud.rec[i];  // the blend the layer drew there before the glyph
      } else {
        p.kind = 1;
        p.px = cur[i];
      }
      patch.idx.push_back(static_cast<uint32_t>(static_cast<size_t>(y - L.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - L.x0)));
      patch.px.push_back(p);
    }
  if (!patch.idx.empty()) g.text.patches_pending.push_back(std::move(patch));  // collected for this frame's publish
}
// whether a visibility mask shows any of the run's glyph pixels (before != after)
bool shows_any(const Gfx::TextCapture::Run &r, const std::vector<uint8_t> &v) {
  for (size_t j = 0; j < v.size() && j < r.before.size(); ++j)
    if (v[j] && r.before[j] != r.after[j]) return true;
  return false;
}
// The peel over img with the runs of `order`, keeping only a run of which at least a quarter of the glyph pixels still show: a
// run the screen has drawn over keeps a few pixels whose new word equals its `after` by chance (white text under a white globe),
// and a window-resolution string drawn through those would leave dots. The others are peeled again without the dropped ones.
// keep[k] = 1 for the kept runs; their vis (or vis_layer with `layer_vis`) is set, the dropped ones' is cleared.
void peel_kept(std::vector<uint16_t> &img, int32_t w, int32_t h, std::vector<Gfx::TextCapture::Run> &live,
               const std::vector<size_t> &order, std::vector<uint8_t> &keep, bool layer_vis) {
  std::vector<uint16_t> work = img;
  std::vector<std::vector<uint8_t>> vis;
  gfx_text_peel(work, w, h, peel_runs(live, order), vis);
  std::vector<size_t> ok;
  for (size_t n = 0; n < order.size(); ++n) {
    const Gfx::TextCapture::Run &r = live[order[n]];
    size_t glyph = 0, shown = 0;
    for (size_t j = 0; j < r.before.size(); ++j)
      if (r.before[j] != r.after[j]) {
        ++glyph;
        if (j < vis[n].size() && vis[n][j]) ++shown;
      }
    if (shown > 0 && shown * 4 >= glyph) ok.push_back(order[n]);
  }
  if (ok.size() != order.size()) {  // again without the dropped runs
    work = img;
    gfx_text_peel(work, w, h, peel_runs(live, ok), vis);
  }
  img = std::move(work);
  for (size_t k : order) (layer_vis ? live[k].info.vis_layer : live[k].info.vis).clear();
  for (size_t n = 0; n < ok.size(); ++n) {
    (layer_vis ? live[ok[n]].info.vis_layer : live[ok[n]].info.vis) = std::move(vis[n]);
    keep[ok[n]] = 1;
  }
}
}  // namespace

// Window::refreshFull: the runs over the back buffer (and, with layers this frame, over the base), published with it
static void text_publish(Gfx &g, const GfxHudFrame *hud) {
  Gfx::TextCapture &c = g.text;
  if (!c.on) return;
  static const bool edge_keep = [] {  // P24F fix 1, SG_P24F_EDGEKEEP=1 (development): the edge columns as before
    const char *e = std::getenv("SG_P24F_EDGEKEEP");
    return e && e[0] == '1';
  }();
  std::vector<Gfx::TextCapture::Run> &live = c.live;
  GfxTextFrame f;
  f.serial = ++c.serial;
  f.patches = std::move(c.patches_pending);  // the layer patches the segments of this frame collected
  c.patches_pending.clear();
  // the back path: the root's runs in draw order, then the window targets' (composed over the root)
  std::vector<size_t> order;
  for (int t = 0; t < 2; ++t)
    for (size_t k = 0; k < live.size(); ++k)
      if (live[k].info.target == t) order.push_back(k);
  f.back = g.back.px;
  f.clean = g.back.px;
  std::vector<uint8_t> keep(live.size(), 0);
  peel_kept(f.clean, g.back.w, g.back.h, live, order, keep, false);
  bool layered = false;
  if (hud && !hud->empty()) {  // the layered path: the base's runs (drawn before the frame's first layer, or kept from before)
    layered = true;
    f.hud_serial = hud->serial;
    std::vector<size_t> base_order;
    for (size_t k = 0; k < live.size(); ++k)
      if (live[k].seg == -1 && live[k].info.target == 0) base_order.push_back(k);
    f.base_clean = hud->base;
    std::vector<uint8_t> base_keep(live.size(), 0);
    peel_kept(f.base_clean, g.root.w, g.root.h, live, base_order, base_keep, true);
    for (size_t k : base_order) live[k].info.layer = base_keep[k] ? -1 : -2;
  } else {
    f.patches.clear();
  }
  // P9D: a carried frame (gfx.h gfx_set_hud_modal): its carried layers' runs and patches as the frame they were peeled in published
  // them; the runs of its centred gap stay in that segment (the next carried publish peels them again)
  const bool carried = layered && hud->carried;
  const int64_t cseg = carried ? g.hud.carry_seg : -2;
  if (carried) {
    f.patches.insert(f.patches.begin(), c.carry_patches.begin(), c.carry_patches.end());
    f.runs.insert(f.runs.end(), c.carry_runs.begin(), c.carry_runs.end());
  }
  std::vector<Gfx::TextCapture::Run> next;
  for (size_t k = 0; k < live.size(); ++k) {
    // the layered path shows a run of this frame's layers (peeled at its segment) even when a later layer covers its place
    // in the back buffer: that layer may be composed elsewhere
    const bool in_layer = layered && live[k].info.layer >= -1 && shows_any(live[k], live[k].info.vis_layer);
    if (!keep[k] && !in_layer) {
      ++c.stats.dropped;
      continue;
    }
    if (!keep[k]) std::fill(live[k].info.vis.begin(), live[k].info.vis.end(), uint8_t(0));
    // P23F fix 3 (TextCapture::frame_gen): a run of an earlier frame keeps only its visible root pixels, not its edge columns
    if (live[k].frame != c.frame_gen) live[k].info.ex0 = live[k].info.ex1 = 0;
    f.runs.push_back(live[k].info);
    // P24F fix 1: over a root that holds no terrain picture (the world map, the course map: a screen over the whole root) the
    // run's edge columns would show beside a picture it was not drawn over. A frame whose HUD layers began before the screen
    // opened publishes the course frame's base runs (in_layer) with no visible root pixel, and the presenter drew their edge columns
    // whole past the 800 frame. The published copy keeps only its visible pixels (SG_P24F_EDGEKEEP=1: as before).
    if (!g.root_terrain && !edge_keep) f.runs.back().ex0 = f.runs.back().ex1 = 0;
    if (keep[k] || (live[k].seg == cseg && in_layer)) next.push_back(std::move(live[k]));
    else ++c.stats.dropped;
  }
  for (Gfx::TextCapture::Run &r : next) {  // the next frame: everything kept is under its first layer (the base)
    if (r.seg == cseg) continue;  // P9D: the carried gap's
    r.seg = -1;
    r.info.layer = -2;
    r.info.vis_layer.clear();
  }
  live = std::move(next);
  // P9D: a layered publish that the capture carries next: its carried layers' runs and patches (gfxb::hud_publish set carry_layers)
  if (layered && !carried && g.hud.modal) {
    const int32_t n = static_cast<int32_t>(g.hud.carry_layers.size());
    c.carry_runs.clear();
    c.carry_patches.clear();
    for (const GfxTextRun &r : f.runs)
      if (r.layer >= 0 && r.layer < n && r.target == 0) c.carry_runs.push_back(r);
    for (const GfxTextLayerPatch &q : f.patches)
      if (q.layer >= 0 && q.layer < n) c.carry_patches.push_back(q);
  }
  // P24F fix 1: the edge runs below belong to the terrain picture they were drawn beside. A screen drawn over the whole root (the
  // world map courseActionMenu, the course map showRoutingMapScreen, the full-screen reports: Gfx::note_root_write cleared
  // root_terrain) draws no terrain picture, so nothing cleared them, and they showed past its 800 frame over the bands or the wide
  // scene. They are published only while the root holds a terrain picture (a picture restored by terrain.blit counts again).
  // SG_P24F_EDGEKEEP=1 (development, edge_keep above): published with every frame, as before.
  if (g.root_terrain || edge_keep) {
    // P22E item 7: the cursor passes' texts wholly past the root's edge (the base's, after its other runs)
    f.runs.insert(f.runs.end(), c.edge_runs.begin(), c.edge_runs.end());
    // P23F: the frame texts wholly past the root's edge (the base's, kept until the next frame's terrain picture)
    f.runs.insert(f.runs.end(), c.frame_edge_runs.begin(), c.frame_edge_runs.end());
  } else if (!c.edge_runs.empty() || !c.frame_edge_runs.empty()) {
    ++c.stats.edge_held;
  }
  // the wide scene's world runs (beside the frame's columns), first: they are drawn over the world, under everything else
  if (g.back_terrain && !c.wide_published.empty()) f.runs.insert(f.runs.begin(), c.wide_published.begin(), c.wide_published.end());
  c.stats.published = f.runs.size();
  ++c.stats.frames;
  c.published = std::move(f);
}

// gfx_scene.cpp (G32's wide scene): the queue's start forgets the scene's runs; its end peels them off `scene` into `scene_text`
// (a copy) and keeps those not inside the frame's columns [0, 800) as world runs
void text_frame_start(Gfx &g) {
  g.text.frame_edge_runs.clear();
  ++g.text.frame_gen;  // P23F fix 3
}
void text_wide_begin(Gfx &g) {
  g.text.wide_runs.clear();
  g.text.wide_published.clear();
}
void text_wide_end(Gfx &g, const std::vector<uint16_t> &scene, int32_t scene_w, int32_t scene_e, std::vector<uint16_t> &scene_text) {
  scene_text.clear();
  if (!g.text.on || g.text.wide_runs.empty() || scene.size() != static_cast<size_t>(scene_w) * 600) return;
  scene_text = scene;
  std::vector<GfxTextPeelRun> pr;
  for (const Gfx::TextCapture::Run &r : g.text.wide_runs) {
    GfxTextPeelRun p;
    p.x0 = r.info.x0 + scene_e;
    p.y0 = r.info.y0;
    p.x1 = r.info.x1 + scene_e;
    p.y1 = r.info.y1;
    p.before = r.before.data();
    p.after = r.after.data();
    pr.push_back(p);
  }
  std::vector<std::vector<uint8_t>> vis;
  gfx_text_peel(scene_text, scene_w, 600, pr, vis);
  for (size_t k = 0; k < g.text.wide_runs.size(); ++k) {
    GfxTextRun r = g.text.wide_runs[k].info;
    if (r.x0 >= 0 && r.x1 <= 800) continue;  // inside the frame: the frame's own run of the same text is drawn
    r.vis = vis[k];
    r.vis_layer = vis[k];
    r.layer = -3;
    g.text.wide_published.push_back(std::move(r));
  }
}

void text_banner_begin(Gfx &g) {
  if (!g.text.on) return;
  static thread_local std::vector<Gfx::TextCapture::Run> banner_runs;
  banner_runs.clear();
  g.text.sink = &banner_runs;
}
void text_banner_end(Gfx &g, const std::vector<uint16_t> &img) {
  if (!g.text.on || !g.text.sink || g.text.sink == &g.text.live) return;
  std::vector<Gfx::TextCapture::Run> &rs = *g.text.sink;
  g.text.sink = &g.text.live;
  GfxTextFrame f;
  f.serial = ++g.text.serial;
  f.back = img;
  f.clean = img;
  std::vector<size_t> order;
  for (size_t k = 0; k < rs.size(); ++k) order.push_back(k);
  std::vector<std::vector<uint8_t>> vis;
  gfx_text_peel(f.clean, g.root.w, g.root.h, peel_runs(rs, order), vis);
  for (size_t k = 0; k < rs.size(); ++k) {
    rs[k].info.vis = std::move(vis[k]);
    f.runs.push_back(std::move(rs[k].info));
  }
  rs.clear();
  g.text.banner = std::move(f);
}

// ---- P9B: the HUD layer capture (gfx.h GfxHudFrame, gfx_body.h Gfx::HudCapture)
namespace {
// P10C fix 2 (gfx_body.h HudWriteMark): marking on for the panel layer gfx_layer_begin just opened, off at its close
void hud_writes_arm(Gfx &g) {
  Gfx::HudCapture &c = g.hud;
  HudWriteMark &m = g_hud_wm;
  m = HudWriteMark{};
  c.wr_gap = false;  // P13D item 17: a panel layer's own marking ends a gap's
  c.wr_box[0] = c.wr_box[1] = 0;
  c.wr_box[2] = c.wr_box[3] = -1;
  if (!c.wr_on || c.cur <= kGfxLayerCentred || g.root.px.empty()) return;
  if (c.wr_gen.size() != g.root.px.size()) c.wr_gen.assign(g.root.px.size(), 0);
  m.lo = reinterpret_cast<uintptr_t>(g.root.px.data());
  m.hi = m.lo + g.root.px.size() * sizeof(uint16_t);
  m.gen = c.wr_gen.data();
  m.cur = c.gen;
  m.w = g.root.w;
}
// P13D item 17: the marking over a modal screen's centred gap (blend generation c.gen, the gap's), until the next layer opens or
// the carried capture ends (gfx_body.h HudCapture::wr_gap)
void hud_writes_arm_gap(Gfx &g) {
  Gfx::HudCapture &c = g.hud;
  HudWriteMark &m = g_hud_wm;
  m = HudWriteMark{};
  c.wr_gap = false;
  if (!c.wr_on || g.root.px.empty()) return;
  if (c.wr_gen.size() != g.root.px.size()) c.wr_gen.assign(g.root.px.size(), 0);
  m.lo = reinterpret_cast<uintptr_t>(g.root.px.data());
  m.hi = m.lo + g.root.px.size() * sizeof(uint16_t);
  m.gen = c.wr_gen.data();
  m.cur = c.gen;
  m.w = g.root.w;
  c.wr_gap = true;
}
void hud_writes_disarm(Gfx &g) {
  Gfx::HudCapture &c = g.hud;
  HudWriteMark &m = g_hud_wm;
  if (m.lo != 0 && m.x1 >= 0 && m.cur == c.gen) {
    c.wr_box[0] = m.x0;
    c.wr_box[1] = m.y0;
    c.wr_box[2] = m.x1;
    c.wr_box[3] = m.y1;
  }
  m = HudWriteMark{};
}
// the root pixels that differ from `ref` as one layer (bounding box, kind 1 = the root's word, or the open layer's blend record
// where it is still the pixel's word), comparing `now` (the root, or the back buffer for its child-window composition) with `ref`
bool hud_diff(Gfx &g, const std::vector<uint16_t> &ref, const std::vector<uint16_t> &now, int32_t id, bool use_rec,
              GfxHudLayer &out) {
  const int32_t w = g.root.w, h = g.root.h;
  if (ref.size() != now.size() || now.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) return false;
  int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
  // P10C fix 2: a panel layer's pixels are also the ones its primitives wrote (gfx_body.h HudWriteMark), whatever the word
  const Gfx::HudCapture &hc = g.hud;
  const bool wr_panel = use_rec && id != kGfxLayerCentred && hc.wr_on && hc.wr_gen.size() == now.size() && hc.wr_box[2] >= 0;
  // P13D item 17: a modal screen's centred gap keeps the pixels its drawing wrote (the marking is still running: its box from it)
  const HudWriteMark &gm = g_hud_wm;
  const bool wr_gap = use_rec && id == kGfxLayerCentred && hc.wr_on && hc.wr_gap && gm.lo != 0 && gm.cur == hc.gen && gm.x1 >= 0 &&
                      hc.wr_gen.size() == now.size();
  const bool wr = wr_panel || wr_gap;
  const int32_t wb[4] = {wr_gap ? gm.x0 : hc.wr_box[0], wr_gap ? gm.y0 : hc.wr_box[1], wr_gap ? gm.x1 : hc.wr_box[2],
                         wr_gap ? gm.y1 : hc.wr_box[3]};
  for (int32_t y = 0; y < h; ++y) {
    const uint16_t *a = ref.data() + static_cast<size_t>(y) * w, *b = now.data() + static_cast<size_t>(y) * w;
    if (std::memcmp(a, b, static_cast<size_t>(w) * 2) == 0) continue;
    for (int32_t x = 0; x < w; ++x)
      if (a[x] != b[x]) {
        x0 = std::min(x0, x);
        x1 = std::max(x1, x);
      }
    y0 = std::min(y0, y);
    y1 = y;
  }
  if (wr) {
    x0 = std::min(x0, wb[0]);
    y0 = std::min(y0, wb[1]);
    x1 = std::max(x1, wb[2]);
    y1 = std::max(y1, wb[3]);
  }
  if (x1 < 0) return false;
  out = GfxHudLayer{};
  out.id = id;
  out.anchor = gfx_layer_anchor(id);
  out.x0 = x0;
  out.y0 = y0;
  out.x1 = x1 + 1;
  out.y1 = y1 + 1;
  // P9D: a centred segment that dimmed the whole frame (gfx_reports.cpp Surface_drawSprite, Gfx::note_dim)
  if (id == kGfxLayerCentred && use_rec && g.hud.dim_set && g.hud.dim_gen == g.hud.gen) {
    out.dim = 1;
    out.dim_src = g.hud.dim_src;
  }
  const int32_t bw = out.x1 - out.x0;
  out.px.assign(static_cast<size_t>(bw) * static_cast<size_t>(out.y1 - out.y0), GfxLayerPx{});
  const bool rec = use_rec && g.hud.rec_gen.size() == now.size();
  for (int32_t y = out.y0; y < out.y1; ++y)
    for (int32_t x = out.x0; x < out.x1; ++x) {
      const size_t i = static_cast<size_t>(y) * w + static_cast<size_t>(x);
      if (ref[i] == now[i] && !(wr && hc.wr_gen[i] == hc.gen)) continue;
      GfxLayerPx &p = out.px[static_cast<size_t>(y - out.y0) * bw + static_cast<size_t>(x - out.x0)];
      p.kind = 1;
      p.px = now[i];
      if (rec && g.hud.rec_gen[i] == g.hud.gen && g.hud.rec[i].px == now[i] && g.hud.rec[i].kind >= 2) {
        p.kind = g.hud.rec[i].kind;
        p.src = g.hud.rec[i].src;
        p.k = g.hud.rec[i].k;
      }
    }
  return true;
}
void hud_gap(Gfx &g) {  // the root's changes since the last boundary, outside every layer: a centred layer
  GfxHudLayer l;
  if (hud_diff(g, g.hud.mark, g.root.px, kGfxLayerCentred, true, l)) {
    l.popups = g.hud.popups && !g.hud.pieces.empty() ? 1 : 0;  // P9E
    g.hud.layers.push_back(std::move(l));
  }
}
// P9E: the frame's popups for a publish: each piece's box over the owner plane; nothing when no piece has an opener panel
void hud_pieces_publish(Gfx &g, GfxHudFrame &f) {
  Gfx::HudCapture &c = g.hud;
  if (!c.popups || c.pieces.empty()) return;
  g.hud_own_sync();
  bool any = false;
  for (const GfxHudPiece &p : c.pieces) any = any || p.opener != kGfxLayerCentred;
  if (!any || c.owner.size() != g.root.px.size()) return;
  f.pieces = c.pieces;
  for (GfxHudPiece &p : f.pieces) p.x0 = p.y0 = p.x1 = p.y1 = 0;
  std::vector<uint8_t> seen(f.pieces.size(), 0);
  const int32_t w = g.root.w, h = g.root.h;
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const uint8_t o = c.owner[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
      if (o == 0 || o > f.pieces.size()) continue;
      GfxHudPiece &p = f.pieces[o - 1u];
      if (!seen[o - 1u]) {
        seen[o - 1u] = 1;
        p.x0 = x;
        p.y0 = y;
        p.x1 = x + 1;
        p.y1 = y + 1;
        continue;
      }
      p.x0 = std::min(p.x0, x);
      p.x1 = std::max(p.x1, x + 1);
      p.y1 = y + 1;
    }
  f.owner = c.owner;
}
}  // namespace

// Window::refreshFull: the layers since the last publish, plus the changes since the last layer closed and the back buffer's own
// composition (child windows) as centred layers; a refreshFull inside an open layer publishes none (a modal screen's frames).
// P9D (gfx_set_hud_modal): a refreshFull inside an open layer that a screen's first call closed (hud_modal_mark) publishes like one
// outside it; a carried capture publishes the last layered frame's base and layers with the screen's drawing since that frame as
// the centred layer after them (nothing when a 2D write has covered the whole root since: no terrain under it); after every
// layered publish the capture is carried (hud_carry_arm) until the next frame starts.
static void hud_publish(Gfx &g) {
  Gfx::HudCapture &c = g.hud;
  if (!c.on) return;
  GfxHudFrame f;
  f.serial = ++c.serial;
  const bool split = c.modal && c.depth > 0 && c.any && c.cur < 0 && !c.aborted;
  const bool carried = c.modal && c.carry;
  if (carried && !g.root_terrain) {
    g.hud_carry_drop();
    f.none = 1;
  } else if (c.any && (c.depth == 0 || split || carried)) {
    const size_t before = c.layers.size();
    hud_gap(g);
    text_close_segment(g, c.seg, c.layers.size() > before ? static_cast<int32_t>(c.layers.size() - 1) : -1, c.mark);  // P9C
    if (!carried) ++c.seg;  // P9D: a carried gap keeps its segment (its runs are peeled again at the next carried publish)
    const size_t own = c.layers.size();
    GfxHudLayer l;
    if (hud_diff(g, g.root.px, g.back.px, kGfxLayerCentred, false, l)) c.layers.push_back(std::move(l));
    f.base = c.base;
    f.back = g.back.px;
    if (c.modal) {
      if (!carried) c.carry_layers.assign(c.layers.begin(), c.layers.begin() + static_cast<std::ptrdiff_t>(own));
      f.layers = c.layers;
    } else {
      f.layers = std::move(c.layers);
    }
    f.carried = carried ? 1 : 0;
    if (!c.cursor2d.empty()) f.cursor2d = c.cursor2d;  // P9E fix 4
    if (!c.lines.empty() && c.line800.size() == g.root.px.size()) {  // P10I fix 1
      f.lines = c.lines;
      f.line800 = c.line800;
      f.line_word = c.line_word;
    }
    hud_pieces_publish(g, f);  // P9E
    // P19C item 8: where the golfer strip's layer goes: right of the left-anchored panel pixels that share its rows
    for (GfxHudLayer &sl : f.layers) {
      if (sl.id != kGfxLayerStrip) continue;
      int32_t xr = -1;
      for (const GfxHudLayer &pl : f.layers) {
        if (pl.id == kGfxLayerStrip || pl.anchor >= 0) continue;
        for (int32_t y = std::max(sl.y0, pl.y0); y < std::min(sl.y1, pl.y1); ++y)
          for (int32_t x = pl.x1 - 1; x >= std::max(pl.x0, xr); --x)
            if (pl.at(x, y)) {
              xr = std::max(xr, x + 1);
              break;
            }
      }
      sl.strip_x = xr;
    }
  } else {
    f.none = c.depth > 0 ? 2 : 1;
    if (c.nl_dim && c.modal) {  // P21C: the screen dimmed the whole frame with no layer open (end of year)
      f.dim_all = 1;
      f.dim_src = c.nl_src;
    }
  }
  if (c.depth > 0) c.aborted = true;
  c.layers.clear();
  c.any = false;
  c.cur = -1;
  const bool arm = c.modal && !f.empty();
  c.published = std::move(f);
  if (arm) g.hud_carry_arm(carried);
  c.carry_split = arm && c.depth > 0;  // P9E fix 2: armed inside an open panel scope (a modal screen the panel opened)
}

}  // namespace gfxb

// P9D: the carry (gfx.h gfx_set_hud_modal). arm: the capture continues the frame just published, its base and layers (without the
// child-window composition) kept, everything drawn on the root from now on a centred gap of the carried segment; `again` = the
// publish was itself carried: the gap stays measured from the same root, segment and blend generation (the screen's drawing
// accumulates over the same frame)
void Gfx::hud_carry_arm(bool again) {
  HudCapture &c = hud;
  c.carry = true;
  c.any = true;
  c.cur = -1;
  c.layers = c.carry_layers;
  if (!again) {
    c.mark = root.px;
    c.carry_seg = c.seg;
    ++c.gen;
    c.dim_set = false;
    gfxb::hud_writes_arm_gap(*this);  // P13D item 17: the screen's drawing over the carried frame is marked
  }
}
// drop: the next frame starts (gfx_terrain.cpp: a terrain pass, its restore, a window paint; gfx_layer_begin: the next layer); the
// carried gap's runs become the next frame's base runs, as every kept run does at a publish
void Gfx::hud_carry_drop() {
  HudCapture &c = hud;
  c.nl_dim = false;  // P21C: a new frame starts
  if (c.wr_gap) {  // P13D item 17: the gap's marking ends with the carry
    g_hud_wm = HudWriteMark{};
    c.wr_gap = false;
  }
  if (!c.carry) return;
  c.carry = false;
  c.any = false;
  c.cur = -1;
  c.layers.clear();
  for (TextCapture::Run &r : text.live)
    if (r.seg == c.carry_seg && c.carry_seg >= 0) {
      r.seg = -1;
      r.info.layer = -2;
      r.info.vis_layer.clear();
    }
  text.patches_pending.clear();
  c.carry_seg = -1;
}
// a screen's first drawing call (reports.begin, dialogs.begin) inside an open panel layer: the layer is closed here as gfx_layer_end
// closes it, and the rest of the scope is a centred gap of the frame (its refreshFull then publishes the frame's layers)
void Gfx::hud_modal_mark() {
  HudCapture &c = hud;
  // P13D item 17: a screen drawn in the frame's centred gap (outside every panel layer): its writes are marked from here on
  if (c.on && c.modal && c.any && !c.aborted && c.cur < 0 && !c.wr_gap) gfxb::hud_writes_arm_gap(*this);
  if (!c.on || !c.modal || c.depth == 0 || !c.any || c.aborted || c.cur < 0) return;
  gfxb::hud_writes_disarm(*this);  // P10C fix 2
  GfxHudLayer l;
  const bool made = gfxb::hud_diff(*this, c.mark, root.px, c.cur, true, l);
  if (made) c.layers.push_back(std::move(l));
  gfxb::text_close_segment(*this, c.seg, made ? static_cast<int32_t>(c.layers.size() - 1) : -1, c.mark);
  ++c.seg;
  c.mark = root.px;
  ++c.gen;
  c.cur = -1;
  gfxb::hud_writes_arm_gap(*this);  // P13D item 17: the screen's drawing in the rest of the scope is marked
}
// the report screens' dim covered every root pixel with one source word (gfx_reports.cpp Surface_drawSprite)
void Gfx::note_dim(uint16_t src) {
  if (!hud.on || !hud.modal) return;
  if (!hud.any) {  // P21C: no layer in this frame: the dim goes on the layer-less frame (hud_publish)
    hud.nl_dim = true;
    hud.nl_src = src;
    return;
  }
  hud.dim_set = true;
  hud.dim_gen = hud.gen;
  hud.dim_src = src;
}
void gfx_set_hud_modal(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  if (!on) g->hud_carry_drop();
  g->hud.modal = on && g->hud.on;
}
bool gfx_hud_modal(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  return g && g->hud.on && g->hud.modal;
}
// ---- P9E: the popups' place (gfx.h gfx_set_hud_popups)
void Gfx::hud_own_sync() {
  HudCapture &c = hud;
  const size_t n = root.px.size();
  if (c.owner.size() != n || c.own_mark.size() != n) {
    c.owner.assign(n, 0);
    c.own_mark = root.px;
    return;
  }
  const uint8_t o = c.open.empty() || c.open.back() < 0 ? 0 : static_cast<uint8_t>(std::min<int32_t>(c.open.back() + 1, 255));
  const uint16_t *a = c.own_mark.data(), *b = root.px.data();
  for (size_t i = 0; i < n; ++i)
    if (a[i] != b[i]) c.owner[i] = o;
  c.own_mark = root.px;
}
void Gfx::hud_pieces_reset() {
  HudCapture &c = hud;
  if (!c.popups || (c.pieces.empty() && c.open.empty())) return;
  // the open popups stay pieces (renumbered from 0, their openers kept); every pixel belongs to nobody again
  std::vector<GfxHudPiece> keep;
  for (int32_t &k : c.open) {
    if (k < 0) continue;
    keep.push_back(c.pieces[static_cast<size_t>(k)]);
    k = static_cast<int32_t>(keep.size() - 1);
  }
  c.pieces = std::move(keep);
  c.owner.assign(root.px.size(), 0);
  c.own_mark = root.px;
}
void gfx_set_hud_close(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  g->hud.close_keep = on && g->hud.on;
  g->hud.carry_split = false;
}
void gfx_set_hud_popups(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  Gfx::HudCapture &c = g->hud;
  c.popups = on && c.on;
  c.pieces.clear();
  c.open.clear();
  c.owner.clear();
  c.own_mark.clear();
}
// the opener: the open panel layer (a popup a panel's own code opens); else, for a click, the popup piece that owns the clicked
// pixel (its opener: a popup opened from a popup stays with it), nothing when the clicked pixel was drawn since the last boundary
// outside every popup (a modal screen's own drawing, a label), else the topmost panel layer with a pixel at the click
void gfx_hud_popup_begin(Ctx &ctx, int32_t cx, int32_t cy, bool click, bool text_box) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.on || !g->hud.popups) return;
  Gfx::HudCapture &c = g->hud;
  g->ensure_root(ctx);
  g->hud_own_sync();
  if (c.pieces.size() >= 250) {  // the owner plane holds 254 pieces; a frame never opens that many: this one owns nothing
    c.open.push_back(-1);
    return;
  }
  GfxHudPiece p;
  p.text = text_box ? 1 : 0;  // P10C fix 4
  p.parent = c.open.empty() ? -1 : c.open.back();
  const int32_t w = g->root.w, h = g->root.h;
  const bool in = cx >= 0 && cy >= 0 && cx < w && cy < h;
  const size_t i = in ? static_cast<size_t>(cy) * static_cast<size_t>(w) + static_cast<size_t>(cx) : 0;
  if (c.depth > 0 && c.cur > kGfxLayerCentred && !c.aborted) {
    p.opener = c.cur;
  } else if (click && in) {
    const uint8_t o = i < c.owner.size() ? c.owner[i] : 0;
    if (o != 0 && o <= c.pieces.size()) {
      p.opener = c.pieces[o - 1u].opener;
    } else if (!(c.any && c.mark.size() == g->root.px.size() && c.mark[i] != g->root.px[i])) {
      const std::vector<GfxHudLayer> &ls = !c.layers.empty() ? c.layers : c.published.layers;
      for (size_t k = ls.size(); k-- > 0;)
        if (ls[k].id != kGfxLayerCentred && ls[k].at(cx, cy)) {
          p.opener = ls[k].id;
          break;
        }
    }
  }
  c.pieces.push_back(p);
  c.open.push_back(static_cast<int32_t>(c.pieces.size() - 1));
}
void gfx_hud_popup_end(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.on || !g->hud.popups || g->hud.open.empty()) return;
  g->hud_own_sync();
  g->hud.open.pop_back();
}
uint16_t gfx_layer_quarter(uint16_t p, uint16_t d) {  // gfx_reports.cpp quarter_blend with its two halvings
  const uint32_t P = static_cast<uint32_t>(p) << 4, D = static_cast<uint32_t>(d) << 4;
  uint32_t e = ((P & 0x7c1f0u) + (D & 0x7c1f0u)) >> 1, f = ((P & 0x3e00u) + (D & 0x3e00u)) >> 1;
  e = (e + (D & 0x7c1f0u)) >> 1;
  f = (f + (D & 0x3e00u)) >> 1;
  return static_cast<uint16_t>((((e & 0x7c1f0u) + (f & 0x3e00u)) >> 4) & 0xffffu);
}

int8_t gfx_layer_anchor(int32_t id) {
  switch (id) {
    case kGfxLayerTitle: case kGfxLayerBottom: case kGfxLayerStrip: return -1;
    case kGfxLayerMeters: case kGfxLayerPanel: return 1;
    default: return 0;
  }
}
std::atomic<int> g_gfx_hud_capture{0};
thread_local HudWriteMark g_hud_wm;  // P10C fix 2 (gfx_body.h)
void gfx_set_hud_writes(Ctx &ctx, bool on) {  // P10C fix 2
  Gfx *g = gfx(ctx);
  if (!g) return;
  g->hud.wr_on = on;
  if (!on) g_hud_wm = HudWriteMark{};
}
void gfx_set_hud_capture(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g || g->hud.on == on) return;
  g->hud = Gfx::HudCapture{};
  g->hud.on = on;
  g_gfx_hud_capture.fetch_add(on ? 1 : -1);
}
bool gfx_hud_capture(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  return g && g->hud.on;
}
bool gfx_hud_frame(Ctx &ctx, GfxHudFrame &out) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.on || g->hud.published.serial == 0) return false;
  out = g->hud.published;
  return true;
}
// ---- P9C
std::atomic<int> g_gfx_text_capture{0};
static std::atomic<int> g_text_peel_witness{1};  // P10B fix 3 (gfx.h gfx_set_text_peel_witness)
void gfx_set_text_peel_witness(bool on) { g_text_peel_witness.store(on ? 1 : 0); }
void gfx_text_peel(std::vector<uint16_t> &img, int32_t w, int32_t h, const std::vector<GfxTextPeelRun> &runs,
                   std::vector<std::vector<uint8_t>> &vis) {
  vis.assign(runs.size(), {});
  const bool witness = g_text_peel_witness.load() != 0;
  std::vector<uint8_t> dist;
  std::vector<int32_t> queue;
  for (size_t k = runs.size(); k-- > 0;) {
    const GfxTextPeelRun &r = runs[k];
    if (r.x1 <= r.x0 || r.y1 <= r.y0) continue;
    const int32_t bw = r.x1 - r.x0, bh = r.y1 - r.y0;
    std::vector<uint8_t> &v = vis[k];
    v.assign(static_cast<size_t>(bw) * static_cast<size_t>(bh), 0);
    // v = 1 where the box pixel still holds the run's after word (P9C: a later write breaks the chain there)
    for (int32_t y = std::max(r.y0, 0); y < std::min(r.y1, h); ++y)
      for (int32_t x = std::max(r.x0, 0); x < std::min(r.x1, w); ++x) {
        const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
        const size_t j = static_cast<size_t>(y - r.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - r.x0);
        if (i < img.size() && img[i] == r.after[j]) v[j] = 1;
      }
    if (witness) {  // P10B fix 3: keep only the pixels near a witness (gfx.h kGfxTextPeelSpread)
      dist.assign(v.size(), 0xff);
      queue.clear();
      for (int32_t y = 0; y < bh; ++y)
        for (int32_t x = 0; x < bw; ++x) {
          const int32_t j = y * bw + x;
          if (!v[static_cast<size_t>(j)]) continue;
          const uint16_t a = r.after[j];
          const bool seen = (x > 0 && v[static_cast<size_t>(j - 1)] && r.after[j - 1] != a) ||
                            (x + 1 < bw && v[static_cast<size_t>(j + 1)] && r.after[j + 1] != a) ||
                            (y > 0 && v[static_cast<size_t>(j - bw)] && r.after[j - bw] != a) ||
                            (y + 1 < bh && v[static_cast<size_t>(j + bw)] && r.after[j + bw] != a);
          if (seen) {
            dist[static_cast<size_t>(j)] = 0;
            queue.push_back(j);
          }
        }
      for (size_t q = 0; q < queue.size(); ++q) {
        const int32_t j = queue[q], x = j % bw, y = j / bw;
        const uint8_t d = dist[static_cast<size_t>(j)];
        if (d >= kGfxTextPeelSpread) continue;
        for (int32_t yy = std::max(y - 1, 0); yy <= std::min(y + 1, bh - 1); ++yy)
          for (int32_t xx = std::max(x - 1, 0); xx <= std::min(x + 1, bw - 1); ++xx) {
            const int32_t n = yy * bw + xx;
            if (!v[static_cast<size_t>(n)] || dist[static_cast<size_t>(n)] != 0xff) continue;
            dist[static_cast<size_t>(n)] = static_cast<uint8_t>(d + 1);
            queue.push_back(n);
          }
      }
      for (size_t j = 0; j < v.size(); ++j)
        if (dist[j] == 0xff) v[j] = 0;
    }
    for (int32_t y = std::max(r.y0, 0); y < std::min(r.y1, h); ++y)
      for (int32_t x = std::max(r.x0, 0); x < std::min(r.x1, w); ++x) {
        const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
        const size_t j = static_cast<size_t>(y - r.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - r.x0);
        if (v[j]) img[i] = r.before[j];
      }
  }
}
void gfx_set_text_capture(Ctx &ctx, bool on) {
  Gfx *g = gfx(ctx);
  if (!g || g->text.on == on) return;
  g->text = Gfx::TextCapture{};
  g->text.on = on;
  g->text.sink = &g->text.live;
  g_gfx_text_capture.fetch_add(on ? 1 : -1);
}
bool gfx_text_capture(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  return g && g->text.on;
}
bool gfx_text_frame(Ctx &ctx, GfxTextFrame &out) {
  Gfx *g = gfx(ctx);
  if (!g || !g->text.on || g->text.published.serial == 0) return false;
  out = g->text.published;
  return true;
}
bool gfx_text_banner(Ctx &ctx, GfxTextFrame &out) {
  Gfx *g = gfx(ctx);
  if (!g || !g->text.on || g->text.banner.serial == 0) return false;
  out = g->text.banner;
  return true;
}
GfxTextStats gfx_text_stats(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  return g ? g->text.stats : GfxTextStats{};
}
const uint16_t *gfx_shade_table(uint8_t id, size_t &n) {
  if (id == 0) {
    n = gfxb::shade_lut().size();
    return gfxb::shade_lut().data();
  }
  if (id == 1) {
    n = gfxh::half_bright_table().size();
    return gfxh::half_bright_table().data();
  }
  n = 0;
  return nullptr;
}
void gfx_layer_begin(Ctx &ctx, int32_t id) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.on) return;
  Gfx::HudCapture &c = g->hud;
  if (c.depth++ > 0) return;
  g->ensure_root(ctx);
  // P9D: the next frame's first layer ends a carried capture. P9E fix 2: not a layer of the frame that closed a modal screen opened
  // inside a panel's scope (the carry continues: the gap since the screen's last publish is closed below as a centred layer)
  if (c.close_keep && c.carry && c.carry_split) c.carry_split = false;
  else g->hud_carry_drop();
  c.aborted = false;
  if (!c.any) {
    c.base = g->root.px;
    c.mark = g->root.px;
    c.any = true;
  } else {
    const size_t before = c.layers.size();
    gfxb::hud_gap(*g);
    gfxb::text_close_segment(*g, c.seg, c.layers.size() > before ? static_cast<int32_t>(c.layers.size() - 1) : -1, c.mark);  // P9C
    c.mark = g->root.px;
  }
  ++c.seg;  // P9C: the layer's segment
  c.cur = id;
  ++c.gen;  // the layer's blend records
  gfxb::hud_writes_arm(*g);  // P10C fix 2
}
void gfx_layer_end(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  if (!g || !g->hud.on || g->hud.depth == 0) return;
  Gfx::HudCapture &c = g->hud;
  if (--c.depth > 0) return;
  gfxb::hud_writes_disarm(*g);  // P10C fix 2: the marked box into wr_box
  if (c.any && !c.aborted && c.cur >= 0) {
    GfxHudLayer l;
    const bool made = gfxb::hud_diff(*g, c.mark, g->root.px, c.cur, true, l);
    if (made) c.layers.push_back(std::move(l));
    gfxb::text_close_segment(*g, c.seg, made ? static_cast<int32_t>(c.layers.size() - 1) : -1, c.mark);  // P9C
    ++c.seg;  // P9C: the centred gap after it
    c.mark = g->root.px;
    ++c.gen;  // the centred gap's blend records
  }
  c.aborted = false;
  c.cur = -1;
}
// the frame-code callees that draw one HUD panel (tick.cpp opens the layer around the call; the inline passes of P6 open theirs in
// tick_frame.inc)
int32_t gfx_layer_of_callee(const char *n) {
  if (!n) return -1;
  switch (n[0]) {
    case 's':
      if (!std::strcmp(n, "showMainRadialMenu") || !std::strcmp(n, "showInfoCursorPanel")) return kGfxLayerBottom;
      // the tournament leader board is drawn in the title strip's place (its frame at (0, 8)); its scorecard mode dims the whole
      // frame, which leaves no world layer, so no layer is composed there
      if (!std::strcmp(n, "showTournamentResults")) return kGfxLayerTitle;
      break;
    case 'd':
      if (!std::strcmp(n, "drawBuildingPanel") || !std::strcmp(n, "drawTerrainToolbar") || !std::strcmp(n, "drawAmenitiesPanel"))
        return kGfxLayerBottom;
      // drawGolferInfoPanel is not one: its popup is drawn at the top of the frame, over the view (it stays centred)
      if (!std::strcmp(n, "drawEmployeeViewPanel") || !std::strcmp(n, "drawEmployeeDetailPanel") ||
          !std::strcmp(n, "drawGolferActionPanel"))
        return kGfxLayerPanel;
      break;
    default:
      break;
  }
  return -1;
}

namespace gfxb {

// Window::refreshFull(this, 0): redrawDrawList -> Window::blitToTarget copies the root window's surface into the screen
// image that Display::present shows (the window tree's other members and the tooltip are not modelled: none is drawn in
// the title menu or the location screen)
static void b_refresh_full(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  if (a[0] != state_va(ctx, &GameState::g_rootWindow)) {
    g.unsup("Window::refreshFull: a window other than the root");
    return;
  }
  if (SG_NEGCTL(g_negctl_gfx) == 4) return;
  g.back.px = g.root.px;
  // P24F fix 1 (gfxb::root_covered): a root that 2D draws covered in pieces holds no terrain picture either (as one whole-surface
  // write, Gfx::note_root_write): no world layer beside it, no edge runs. SG_P24F_COVER=0 (development): the flag as before.
  static const bool cover_off = [] {
    const char *e = std::getenv("SG_P24F_COVER");
    return e && e[0] == '0';
  }();
  if (g.root_terrain && !cover_off) {
    // SG_P24F_COVER_LOG=1 (development): the test's calls, covered calls and time on stderr every 100 calls
    static const bool cover_log = [] {
      const char *e = std::getenv("SG_P24F_COVER_LOG");
      return e && e[0] == '1';
    }();
    const int64_t t0 = cover_log ? perf_now_ns() : 0;
    const bool covered = gfxb::root_covered(g, ctx);
    if (covered) {
      g.root_terrain = 0;
      ++g.text.stats.root_covered;
    }
    if (cover_log) {
      static uint64_t calls = 0, hits = 0;
      static int64_t ns = 0, ns_max = 0, ns_hit = 0;
      const int64_t d = perf_now_ns() - t0;
      ++calls;
      ns += d;
      ns_max = std::max(ns_max, d);
      if (covered) {
        ++hits;
        ns_hit += d;
      }
      if (calls % 100 == 0)
        std::fprintf(stderr, "P24F cover calls %llu covered %llu mean_us %.2f max_us %.1f covered_mean_us %.1f\n",
                     static_cast<unsigned long long>(calls), static_cast<unsigned long long>(hits), ns * 1e-3 / calls,
                     ns_max * 1e-3, hits ? ns_hit * 1e-3 / hits : 0.0);
    }
  }
  gfxe::compose(g, ctx);   // roadmap S3.19: Window::blitToTarget of every shown child window over the root's copy
  hud_publish(g);          // P9B: the frame's HUD layers (inert with capture off)
  text_publish(g, g.hud.on ? &g.hud.published : nullptr);  // P9C: the frame's text runs (inert with capture off)
  gfxh::after_present(g, ctx);
  g.back_terrain = g.root_terrain;
  std::pair<GfxRefreshHook, void *> h{nullptr, nullptr};
  {
    std::lock_guard<std::mutex> l(mu());
    auto it = refresh_hooks().find(&ctx);
    if (it != refresh_hooks().end()) h = it->second;
  }
  if (h.first) h.first(h.second);  // roadmap S7.9: a live run presents here (gfx.h)
}

// Surface_fillRegion(this, s, x, y, w, h) (c3h.cpp SurfaceFillRegion_re): this's handle slot +0x40 (Surface::stretchTo) with
// the rectangle (x, y, x + w, y + h) as source and destination: a 1:1 copy of an 8-bit image's rectangle onto the root surface
static void b_fill_region(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  auto it = g.images.find(a[0]);
  if (it == g.images.end() || a[1] != g.root_va(ctx)) {
    g.unsup("Surface_fillRegion: a source or destination the backend does not model");
    return;
  }
  const int32_t x = static_cast<int32_t>(a[2]), y = static_cast<int32_t>(a[3]);
  copy8(it->second, g.root, x, y, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]), x, y);
  g.note_root_write(x, y, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]));
}

// Surface_stretchBlit(this, mask, dst, x, y, extra) (c3da.cpp): this's handle slot +0x54 = jgld Sprite::bltS (16-bit target,
// both sprites 8 bpp, extra = a palette override) -> blitPair1 (re/match/jgld_blitY4.cpp): this sprite's rectangle at (x, y)
// (no trim offset) clipped to the destination; the mask sprite is read at the same offsets with this sprite's pitch (create8
// sets the pitch to the width). A byte 0xff in either skips the pixel; a mask byte 0 writes the colour's RGB555 word; any
// other mask byte m writes, per channel, (((dst8 * m) >> 8) + src8) >> 3 with dst8 / src8 the channel as `(w >> 7) & 0xf8`,
// `(w >> 2) & 0xf8`, `(w << 3) & 0xf8`, red shifted left 10 and green 5, the three ORed into a 16-bit word (a channel sum
// above 31 carries into the next field).
static void b_stretch_blit(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  if (a[2] != g.root_va(ctx) || a[5] != 0) {
    g.unsup("Surface_stretchBlit: a destination other than the root surface or a palette override");
    return;
  }
  auto si = g.sprites.find(a[0]), mi = g.sprites.find(a[1]);
  if (si == g.sprites.end() || mi == g.sprites.end()) {
    g.unsup("Surface_stretchBlit: a SpriteRef the backend has no sprite for");
    return;
  }
  const Sprite8 &s = si->second, &m = mi->second;
  Surface16 &d = g.root;
  const int32_t x = static_cast<int32_t>(a[3]), y = static_cast<int32_t>(a[4]);
  const int32_t l = std::max(x, 0), t = std::max(y, 0), r = std::min(x + s.w, d.w), b = std::min(y + s.h, d.h);
  const bool wrong = SG_NEGCTL(g_negctl_gfx) == 5;
  for (int32_t yy = t; yy < b; ++yy)
    for (int32_t xx = l; xx < r; ++xx) {
      const size_t o = static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x);
      const uint8_t c = s.px[o];
      if (o >= m.px.size()) {   // the original reads past a smaller mask; not modelled
        g.unsup("Surface_stretchBlit: a mask smaller than its sprite");
        continue;
      }
      const uint8_t k = m.px[o];
      if (c >= 0xff || k >= 0xff) continue;
      uint16_t &w = d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)];
      const uint16_t sw = s.pal[c];
      hud_mark(&w);  // P10C fix 2
      if (k == 0 || wrong) {
        w = sw;
        continue;
      }
      const uint16_t before = w;  // P9B: the blend's destination (gfx_body.h note_blend)
      auto ch = [&](uint32_t dv, uint32_t sv) {
        return static_cast<uint16_t>(static_cast<uint16_t>(((dv * k) >> 8) + sv) >> 3);
      };
      const uint16_t rr = static_cast<uint16_t>(ch((w >> 7) & 0xf8u, (sw >> 7) & 0xf8u) << 10);
      const uint16_t gg = static_cast<uint16_t>(ch((w >> 2) & 0xf8u, (sw >> 2) & 0xf8u) << 5);
      const uint16_t bb = ch((static_cast<uint32_t>(w) << 3) & 0xf8u, (static_cast<uint32_t>(sw) << 3) & 0xf8u);
      w = static_cast<uint16_t>(rr | gg | bb);
      if (g.hud.any) g.note_blend(static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx), before, w, 2, sw, k);
    }
}

// courseActionMenu's setup (c3fw.cpp CourseActionMenu_re): its frame Image loads interface\WorldButton.pcx (flags 2) and cuts
// seven pieces into its Surface array (kind 1, trim 1), then loads interface\WorldBase.pcx with its palette cache (flags 0)
// and cuts piece 6; the Image keeps WorldBase, which every frame copies whole onto the root surface (Surface_fillRegion).
static void b_cam_setup(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4e4004);
  static const struct { int elem, x, y, w, h; } kCuts[7] = {
      {0, 0x0FA, 0x198, 0x4C, 0x72}, {1, 0x2D4, 0x198, 0x4C, 0x72}, {2, 0x226, 0x1CA, 0x4C, 0x40}, {3, 0x190, 0x1CA, 0x4C, 0x40},
      {4, 0x226, 0x198, 0x4C, 0x32}, {5, 0x190, 0x198, 0x4C, 0x32}, {7, 0x2DC, 0x214, 0x44, 0x44}};
  Image8 img;
  if (load_image(g, ctx, pool + 0x50, 0, 0, 0x100, 2, img) == 0)
    for (const auto &c : kCuts) crop_sprite(g, gfx_local_key(0x2Cu * static_cast<uint32_t>(c.elem)), img, c.x, c.y, c.w, c.h, true);
  Image8 base;
  if (load_image(g, ctx, pool + 0x38, 1, 0, 0x100, 0, base) == 0) {
    crop_sprite(g, gfx_local_key(0x2C * 6), base, 0x2DC, 0x214, 0x44, 0x44, true);
    g.images[gfx_local_key(0x1000)] = std::move(base);
  }
}

// loadInterfaceArt(1)'s image half, the part modelled: its last loads (c3fv.cpp kIa41..kIa47 and the cut helpers after them).
// Each load replaces the whole working Image, so this tail does not depend on the loads before it. ChooseAlphaButtons_A ->
// WideRow into g_sprites5a4138 cells 0..3; choose{Parkland,Desert,Tropical,Links}Buttons -> WideRows into g_sprites53c570
// cells 0..15 (four per course kind); TacksandArrow -> four columns c of four 0x14 x 0x18 cells at (0x32 c, 0x32 r) into cells
// 16 + 4 c + r; TacksandArrow_A -> the same cells of g_sprites5a4138. WideRow: cells at x = 2 + 0xc8 i, y 0, the first two
// w0 x h0, the others 0xb6 x h1, kind 1, trim 1.
[[maybe_unused]] static void b_iart_images(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c95f4);
  const uint32_t plates = state_va(ctx, &GameState::g_sprites53c570), masks = state_va(ctx, &GameState::g_sprites5a4138);
  Image8 img;
  auto load = [&](uint32_t off) { return load_image(g, ctx, pool + off, 0, 0, 0x100, 2, img) == 0; };
  auto wide = [&](uint32_t dest, int32_t w0, int32_t h0, int32_t h1) {
    uint32_t i = 0;
    for (int32_t x = 2; x < 0x322; x += 0xC8, ++i)
      crop_sprite(g, dest + 0x2Cu * i, img, x, 0, i >= 2 ? 0xB6 : w0, i >= 2 ? h1 : h0, true);
  };
  auto columns = [&](uint32_t dest) {
    for (uint32_t c = 0; c < 4; ++c)
      for (uint32_t r = 0; r < 4; ++r)
        crop_sprite(g, dest + 0xB0u * c + 0x2Cu * r, img, static_cast<int32_t>(0x32 * c), static_cast<int32_t>(0x32 * r), 0x14, 0x18,
                    true);
  };
  auto grid3 = [&](uint32_t dest, bool trim) {   // Grid(dest, 3, 3, 0, 0x11, 0, 0x11, 0x10, 0x10, 1, trim)
    for (uint32_t r = 0; r < 3; ++r)
      for (uint32_t c = 0; c < 3; ++c)
        crop_sprite(g, dest + 0x2Cu * (3 * r + c), img, static_cast<int32_t>(0x11 * c), static_cast<int32_t>(0x11 * r), 0x10, 0x10,
                    trim);
  };
  // the translucent popup pieces (kIa19 / kIa20 / kIa23): s_TransPopups -> the style-0 tile g_sprites5a53c8 cell 9 at
  // (0x13d, 0) and the 3 x 3 panel tiles g_surfaces541580 (trimmed); TransPopups -> the 3 x 3 border pieces
  // g_sprites5a53c8 (untrimmed); TransPopups_A -> their masks g_sprites58b680 (untrimmed)
  if (load(15668)) {
    crop_sprite(g, state_va(ctx, &GameState::g_sprites5a53c8) + 0x2C * 9, img, 0x13D, 0, 0x10, 0x10, true);
    grid3(state_va(ctx, &GameState::g_surfaces541580), true);
  }
  if (load(15644)) grid3(state_va(ctx, &GameState::g_sprites5a53c8), false);
  if (load(15620)) grid3(state_va(ctx, &GameState::g_sprites58b680), false);
  if (load(14852)) wide(masks, 0x3C, 0x32, 0x4A);                // ChooseAlphaButtons_A
  if (load(14820)) wide(plates, 0x3B, 0x31, 0x49);               // chooseParklandButtons
  if (load(14788)) wide(plates + 0x2C * 4, 0x3B, 0x31, 0x49);    // chooseDesertButtons
  if (load(14756)) wide(plates + 0x2C * 8, 0x3B, 0x31, 0x49);    // chooseTropicalButtons
  if (load(14724)) wide(plates + 0x2C * 12, 0x3B, 0x31, 0x49);   // chooseLinksButtons
  if (load(14700)) columns(plates + 0x2C * 16);                  // TacksandArrow
  if (load(14672)) columns(masks + 0x2C * 16);                   // TacksandArrow_A
}

// ------------------------------------------------------------------------------------------------ drawWindow (sel == -1)
// The shade table blitPanel's tiles read: block 1 of build16BitColorTable (c3ad.cpp fillBlock, step 0x400) in RGB555 (the
// display's format slot answers 0): for every coarse triple (r, g, b) in steps of 8 the word at pack(r, g, b) is pack of
// ((c / 8) * 0x400 >> 8) per channel, pack = (r >> 3) << 10 | (g >> 3) << 5 | b >> 3. It is the table g_color16Lut points at.
const std::array<uint16_t, 0x8000> &shade_lut() {
  static const std::array<uint16_t, 0x8000> t = [] {
    std::array<uint16_t, 0x8000> a{};
    auto pack = [](uint32_t r, uint32_t g, uint32_t b) {
      return static_cast<uint16_t>((((r & 0xff) >> 3) << 10) | (((g & 0xff) >> 3) << 5) | ((b & 0xff) >> 3));
    };
    for (uint32_t r = 0; r < 0x100; r += 8)
      for (uint32_t g = 0; g < 0x100; g += 8)
        for (uint32_t b = 0; b < 0x100; b += 8)
          a[pack(r, g, b)] = pack(((r / 8) * 0x400) >> 8, ((g / 8) * 0x400) >> 8, ((b / 8) * 0x400) >> 8);
    return a;
  }();
  return t;
}

// jgld Sprite::drawG -> draw16_10028630 (re/match/jgld_blit16b.cpp) at scale 1: bytes 0xff transparent, 0xf8 replace the
// destination word w by lut[w], the others the palette's word
void draw_g16(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y) {
  draw_g16_lut(g, s, d, x, y, shade_lut().data(), shade_lut().size());
}
void draw_g16_lut(Gfx &g, const Sprite8 &s, Surface16 &d, int32_t x, int32_t y, const uint16_t *lut, size_t lut_n) {
  const int32_t l = std::max(x, 0), t = std::max(y, 0), r = std::min(x + s.w, d.w), b = std::min(y + s.h, d.h);
  for (int32_t yy = t; yy < b; ++yy)
    for (int32_t xx = l; xx < r; ++xx) {
      const uint8_t c = s.px[static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x)];
      uint16_t &w = d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)];
      if (c >= 0xff) continue;
      hud_mark(&w);  // P10C fix 2
      if (c == 0xf8 && SG_NEGCTL(g_negctl_gfx) != 6) {
        if (w >= lut_n) {
          g.unsup("blitPanel: a destination word above the RGB555 range");
          continue;
        }
        const uint16_t before = w;
        w = lut[w];
        if (g.hud.any && &d == &g.root) {  // P9B: a shade over the layer's background (gfx_body.h note_blend)
          const uint8_t id = lut == shade_lut().data() ? 0 : lut == gfxh::half_bright_table().data() ? 1 : 0xff;
          if (id != 0xff) g.note_blend(static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx), before, w, 3, 0, id);
        }
      } else {
        w = s.pal[c];
      }
    }
}

// the 16-pixel rounding of the frame drawers (c3da.cpp round16): a size not a multiple of 16 grows by r = 15 - (size & 15)
// and the position moves back by r / 2
static void round16(int32_t &pos, int32_t &size) {
  const int32_t low = size & 0xf;
  if (low != 0) {
    const int32_t r = 0xf - low;
    size += r;
    pos -= r / 2;
  }
}

// blitPanel(x, y, w, h, 1) (c3da.cpp BlitPanel_re): 16 x 16 tiles from g_surfaces541580 (cell cx + 3 cy: 0 in the first
// column / row, 2 when the tile reaches the end, else 1) through Surface_blit2 (the tile's trims added) -> Sprite::drawG with
// the shade table
static void blit_panel(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
  const uint32_t tiles = state_va(ctx, &GameState::g_surfaces541580);
  const int32_t xe = add32(x, w), ye = add32(y, h);
  for (int32_t xi = x; xi < xe; xi += 16)
    for (int32_t yi = y; yi < ye; yi += 16) {
      int32_t cx = xi == x ? 0 : 1, cy = yi == y ? 0 : 1;
      if (add32(xi, 16) >= xe) cx = 2;
      if (add32(yi, 16) >= ye) cy = 2;
      auto it = g.sprites.find(tiles + 0x2Cu * static_cast<uint32_t>(cx + 3 * cy));
      if (it == g.sprites.end()) {
        g.unsup("blitPanel: a tile the backend has no sprite for");
        continue;
      }
      draw_g16(g, it->second, g.root, xi + it->second.left, yi + it->second.top);
    }
}

// drawFrameR3(x, y, w, h, 1) (c3da.cpp DrawFrameR3_re): the display scale (1, 1, 1), 16-rounding, blitPanel, then the eight
// border pieces (top-left, top, bottom, left, right, top-right, bottom-right, bottom-left) from g_sprites5a53c8 cells
// 0, 1, 7, 3, 5, 2, 8, 6 over the same cells of g_sprites58b680 (Surface_stretchBlit); the edges repeat every 16 from
// pos + 16, (len - 16 - 1) >> 4 times, when len - 16 > 16
void draw_frame_r3(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
  round16(x, w);
  round16(y, h);
  blit_panel(g, ctx, x, y, w, h);
  const uint32_t s = state_va(ctx, &GameState::g_sprites5a53c8), m = state_va(ctx, &GameState::g_sprites58b680);
  static const uint32_t kCell[8] = {0, 1, 7, 3, 5, 2, 8, 6};
  const uint32_t root = g.root_va(ctx);
  auto piece = [&](int i, int32_t px, int32_t py) {
    const uint32_t a[6] = {s + 0x2Cu * kCell[i], m + 0x2Cu * kCell[i], root, static_cast<uint32_t>(px),
                           static_cast<uint32_t>(py), 0};
    b_stretch_blit(g, ctx, a, 6);
  };
  piece(0, x, y);
  if (w - 0x10 > 0x10) {
    uint32_t n = static_cast<uint32_t>(w - 0x10 - 1) >> 4;
    int32_t xi = x + 0x10;
    do {
      piece(1, xi, y);
      piece(2, xi, y + h - 0x10);
      xi += 0x10;
    } while (--n != 0);
  }
  if (h - 0x10 > 0x10) {
    uint32_t n = static_cast<uint32_t>(h - 0x10 - 1) >> 4;
    int32_t yi = y + 0x10;
    do {
      piece(3, x, yi);
      piece(4, x + w - 0x10, yi);
      yi += 0x10;
    } while (--n != 0);
  }
  piece(5, x + w - 0x10, y);
  piece(6, x + w - 0x10, y + h - 0x10);
  piece(7, x, y + h - 0x10);
}

// Widget_value(target) (c3c.cpp Value_re): the target's font (+0x5c; the backend's font 0): its +8 >= 0 gives +0x10 + +8,
// else +0xc (the line height Font::create stored)
static int32_t widget_value(Gfx &g, Ctx &ctx) {
  const uint32_t f = g.fonts[0] ? g.fonts[0] : ctx.gs().g_primarySurface;
  const int32_t i = ctx.mem.load<int32_t>(f + 8);
  return i >= 0 ? add32(ctx.mem.load<int32_t>(f + 0x10), i) : ctx.mem.load<int32_t>(f + 0xc);
}

// drawMessageBuffer(cols, x, y, colour) (c3dv.cpp DrawValueWidget_re) over the text buffer: per character the width of that
// one character (Text_put of " " with its first byte replaced) accumulates; ' ', newline and '^' mark the break; a newline or
// '^', or a width above cols * 8, draws the line from its start to the break (drawTextSurface when the line height H =
// Widget_value - 4, at least 10, is above 16 and the colour is white, else drawTextWithColor), y += H, and the scan resumes
// after the break; y > 0x248 stops. A pending line is drawn at the end. drawTextSurface's shadow (setQuad's 1, 0, 1: palette
// colour 1 one pixel down) is the run palette's entry 1 as prim_text2 measured it (U-1230; U-1072 before roadmap S2.11).
void draw_message_buffer(Gfx &g, Ctx &ctx, int32_t cols, int32_t x, int32_t y, uint32_t colour) {
  TextMetrics *tm = text_metrics(ctx);
  const uint32_t buf = lang::redirect(ctx, state_va(ctx, &GameState::g_textBuffer));  // P24D: wrapped in the current language
  const Memory &m = ctx.mem;
  int32_t H = sub32(widget_value(g, ctx), 4);
  if (H < 10) H = 10;
  const uint32_t len = vstrlen(m, buf);
  if (len == 0 || !tm) {
    if (!tm) g.unsup("drawMessageBuffer: no text provider");
    return;
  }
  auto draw_line = [&](uint32_t st, uint32_t e) {
    const uint8_t saved = m.load<uint8_t>(buf + e);
    std::string line;
    for (uint32_t k = st; k < e && k < len; ++k) line += static_cast<char>(m.load<uint8_t>(buf + k));
    (void)saved;
    // the line is drawn from a copy (the original cuts the buffer at the break and restores it)
    if (H > 0x10 && colour == 0x80007fffu) {
      // drawTextSurface(line, x, y, white) (c3dq.cpp DrawTextSurface_re; roadmap S2.11): its bottom clamp (y = h - v - 1
      // when v + y >= h, v = Widget_value, h the root's height), then the shadow in the run palette's entry 1 one pixel
      // down (RGB (0x80, 0, 0) as prim_text2 measured it, U-1230) and the white text
      int32_t yy = y;
      const int32_t v = widget_value(g, ctx);
      if (add32(v, yy) >= g.root.h) yy = sub32(sub32(g.root.h, v), 1);
      draw_text_string(g, ctx, line, 0, x, add32(yy, 1), 0x80000000u | (0x80u >> 3) << 10, false, "drawTextSurface (shadow)");
      draw_text_string(g, ctx, line, 0, x, yy, colour, false, "drawTextSurface");
      return;
    }
    draw_text_string(g, ctx, line, 0, x, y, colour, false, "drawMessageBuffer");
  };
  uint32_t st = 0, e = 0, i = 0;
  int32_t acc = 0;
  do {
    const uint8_t c = m.load<uint8_t>(buf + i);
    int32_t w = 0;
    if (!tm->glyph_width(ctx, g.fonts[0], buf + i, 1, w)) {
      g.unsup("drawMessageBuffer: the provider cannot measure");
      return;
    }
    acc = add32(acc, w);
    if (c == ' ' || c == 0x0a || c == '^') e = i;
    if (c == 0x0a || c == '^' || acc > cols * 8) {
      draw_line(st, e);
      y = add32(y, H);
      acc = 0;
      st = e + 1;
      if (c == '^') st = e + 2;
      i = e;
      if (y > 0x248) return;
    }
    ++i;
  } while (i < len);
  if (acc > 0 && y <= 0x248) draw_line(st, len);
}

// drawWindow(x, y, p3, mode): the text family's drawWindow.frame (gfx_text.cpp) is its only body (roadmap S3.13)

const GfxBodyDef kBodies[] = {
    {"showTitleScreen", b_show_title_screen},     {"loadImage", b_load_image},
    {"Surface_blit", b_surface_blit},             {"Surface_blit3", b_surface_blit3},
    {"Image_draw", b_image_draw},                 {"setDrawTarget", b_set_draw_target},
    {"drawCenteredText", b_draw_centered_text},   {"drawTextWithColor", b_draw_text_4049d0},
    {"Window::refreshFull", b_refresh_full},      {"Surface_fillRegion", b_fill_region},
    {"Surface_stretchBlit", b_stretch_blit},      {"courseActionMenu.setup", b_cam_setup},
    {"drawPanelBackdrop", b_panel_backdrop},
};

// Drops that draw nothing (pacing, audio, input, files, the window tree's modal stack): not reported as unsupported drawing.
// Each one's reason in tick.cpp's kDrops table says it does not draw: waitTicks "wait loop (pacing)", startSoundSlot / playSound
// "audio request", Snd::status "audio device status query", capsLockOff "keybd_event", emptyFunc_4a0890 "empty function",
// saveCourseJpg "screenshot file", flushInput "modal stack push and focus clear of the window tree" (port/test/framegate.py
// NOT_DRAWING holds the same names). Widget_readPixel has a body since roadmap S3.26 (gfx_pixel.cpp: it hands the pixel back).
const char *const kNotDrawing[] = {"waitTicks", "startSoundSlot", "playSound",     "Snd::status",     "capsLockOff",
                                   "emptyFunc_4a0890", "saveCourseJpg", "flushInput"};

}  // namespace gfxb

// ------------------------------------------------------------------------------------------------ the family table
// Every family of gfx_families.inc by explicit reference (port/src/sim/gfx_body.h).
#define SG_GFX_FAMILY(f)                  \
  extern const GfxFamily kGfxFamily_##f; \
  extern int g_negctl_gfx_##f;
#include "gfx_families.inc"
#undef SG_GFX_FAMILY

namespace {
const GfxFamily *const kFamilies[] = {
#define SG_GFX_FAMILY(f) &kGfxFamily_##f,
#include "gfx_families.inc"
#undef SG_GFX_FAMILY
    nullptr};
constexpr size_t kFamilyCount = sizeof(kFamilies) / sizeof(kFamilies[0]) - 1;

struct BodyIndex {
  std::map<std::string, std::pair<GfxBody, const char *>> bodies;   // name -> body, owner
  std::map<std::string, const char *> not_drawing;
  std::vector<std::string> duplicates;                                // "<name>\t<owner>\t<other owner>"
  BodyIndex() {
    auto add = [&](const char *owner, const GfxBodyDef *b, size_t n, const char *const *nd, size_t nn) {
      for (size_t i = 0; i < n; ++i) {
        auto it = bodies.find(b[i].name);
        if (it != bodies.end() || not_drawing.count(b[i].name))
          duplicates.push_back(std::string(b[i].name) + "\t" + owner + "\t" + (it != bodies.end() ? it->second.second : "not-drawing"));
        else
          bodies[b[i].name] = {b[i].body, owner};
      }
      for (size_t i = 0; nd && i < nn; ++i) {
        if (bodies.count(nd[i]) || not_drawing.count(nd[i]))
          duplicates.push_back(std::string(nd[i]) + "\t" + owner + "\tnot-drawing");
        else
          not_drawing[nd[i]] = owner;
      }
    };
    add("gfx", gfxb::kBodies, sizeof(gfxb::kBodies) / sizeof(gfxb::kBodies[0]), gfxb::kNotDrawing,
        sizeof(gfxb::kNotDrawing) / sizeof(gfxb::kNotDrawing[0]));
    for (size_t k = 0; k < kFamilyCount; ++k) {
      const GfxFamily &f = *kFamilies[k];
      add(f.family, f.bodies, f.count, f.not_drawing, f.not_drawing_count);
    }
  }
};
const BodyIndex &body_index() {
  static const BodyIndex ix;   // built once (C++11 thread-safe local static)
  return ix;
}
}  // namespace

GfxBody gfxb::find_body(const char *name, const char **family) {
  const BodyIndex &ix = body_index();
  auto it = ix.bodies.find(name);
  if (it == ix.bodies.end()) return nullptr;
  if (family) *family = it->second.second;
  return it->second.first;
}

void set_gfx(Ctx &ctx, Gfx *g) {
  std::lock_guard<std::mutex> l(mu());
  auto &m = attached();
  const bool had = m.count(&ctx) != 0;
  if (g) {
    m[&ctx] = g;
    if (!had) g_gfx_attached.fetch_add(1);
  } else if (had) {
    m.erase(&ctx);
    g_gfx_attached.fetch_sub(1);
  }
}
Gfx *gfx(Ctx &ctx) {
  std::lock_guard<std::mutex> l(mu());
  auto it = attached().find(&ctx);
  return it == attached().end() ? nullptr : it->second;
}
// P8B: the back buffer's terrain flag (Gfx::back_terrain); false without a backend
bool gfx_back_terrain(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  return g != nullptr && g->back_terrain != 0;
}

// P11C (gfx.h gfx_set_hover_help)
void gfx_set_hover_help(Ctx &ctx, bool on) {
  if (Gfx *g = gfx(ctx)) g->hover_help = on;
}
// P13A (gfx.h gfx_set_hover_help_course)
void gfx_set_hover_help_course(Ctx &ctx, bool on) {
  if (Gfx *g = gfx(ctx)) g->hover_help_course = on;
}
// P22D fix 3 (gfx.h gfx_set_meter_tips)
void gfx_set_meter_tips(Ctx &ctx, bool on) {
  if (Gfx *g = gfx(ctx)) g->meter_tips = on;
}
// P24G (gfx.h gfx_set_tourney_hold)
void gfx_set_tourney_hold(Ctx &ctx, bool on) {
  if (Gfx *g = gfx(ctx)) g->tourney_hold = on;
}
bool gfx_meter_tips(Ctx &ctx) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0) return false;
  Gfx *g = gfx(ctx);
  return g != nullptr && g->meter_tips;
}
bool gfx_hover_help(Ctx &ctx) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0) return false;
  Gfx *g = gfx(ctx);
  return g != nullptr && g->hover_help;
}

bool gfx_drop(Ctx &ctx, const char *name, const uint32_t *a, int n) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0) return false;
  Gfx *g = gfx(ctx);
  if (!g) return false;
  if (GfxBody body = gfxb::find_body(name)) {
    const PerfGfxScope perf_scope;  // P10A (perf.h kPerfGfx)
    ++g->handled[name];
    uint32_t args[12] = {};
    for (int i = 0; i < n && i < 12; ++i) args[i] = a[i];
    // P9E fix 4: a sprite draw of a cursor pass (gfx.h gfx_set_hud_cursor): the root pixels it changes are the tool preview's
    const bool cur2d = g->hud.cursor_sprites && g->hud.cursor_depth > 0 &&
                       (!std::strcmp(name, "Surface_drawSprite") || !std::strcmp(name, "drawQueuedSprite.draw"));
    std::vector<uint16_t> before;
    if (cur2d) {
      g->ensure_root(ctx);
      before = g->root.px;
    }
    try {
      body(*g, ctx, args, n);
    } catch (...) {
      g->unsup(std::string(name) + ": the body failed (an exception)");
    }
    if (cur2d && before.size() == g->root.px.size()) {
      std::vector<uint8_t> &m = g->hud.cursor2d;
      if (m.size() != before.size()) m.assign(before.size(), 0);
      for (size_t i = 0; i < before.size(); ++i)
        if (before[i] != g->root.px[i]) m[i] = 1;
    }
    return true;
  }
  if (body_index().not_drawing.count(name)) return false;
  g->unsup(std::string("no body: ") + name);
  return false;
}

void gfx_progress(Ctx &ctx, uint32_t pos) {
  Gfx *g = gfx(ctx);
  if (!g) return;
  while (g->snap_next < g->snap_at.size() && pos > g->snap_at[g->snap_next]) {
    g->ensure_root(ctx);
    g->snap_unsupported.push_back(g->unsupported.size());
    g->snap_terrain.push_back(g->back_terrain);
    try {
      gfxb::put_file(ctx, "$gfx/snapshot_" + std::to_string(g->snap_next) + ".555", gfxb::surface_bytes(g->back));
      // development (roadmap S3.26): SG_GFX_SNAPSTATE=1 also keeps the GameState of the snapshot's instant
      static const char *snapstate = std::getenv("SG_GFX_SNAPSTATE");
      if (snapstate && snapstate[0] == '1')
        gfxb::put_file(ctx, "$gfx/state_" + std::to_string(g->snap_next) + ".bin",
                       std::vector<uint8_t>(ctx.bytes(), ctx.bytes() + sizeof(GameState)));
    } catch (...) {
      g->unsup("snapshot: the files overlay refused it");
    }
    ++g->snap_next;
  }
}

bool gfx_take_value(Ctx &ctx, int32_t &v) {
  Gfx *g = gfx(ctx);
  if (!g || !g->has_value) return false;
  g->has_value = false;
  v = g->value;
  return true;
}

void gfx_end_of_input(Ctx &ctx) {
  Gfx *g = gfx(ctx);
  if (g) gfx_progress(ctx, UINT32_MAX);
}
namespace {
// gfx.attach(): a backend owned by this context (replaces any other)
void a_attach(Ctx &c, const uint32_t *, Ret &r) {
  auto g = std::make_unique<Gfx>();
  set_gfx(c, g.get());
  owned()[&c] = std::move(g);
  r.set_i32(0);
}
void a_detach(Ctx &c, const uint32_t *, Ret &r) {
  set_gfx(c, nullptr);
  owned().erase(&c);
  r.set_i32(0);
}
Gfx &need(Ctx &c) {
  Gfx *g = gfx(c);
  if (!g) throw MissingResource{"gfx backend (gfx.attach)"};
  return *g;
}
// gfx.snapshotAt(pos): copy the back buffer into `$gfx/snapshot_<k>.555` (k = the request's order) when the run first
// consumes a trace item past clock ordinal pos; requests must come in ascending order
void a_snapshot_at(Ctx &c, const uint32_t *a, Ret &r) {
  Gfx &g = need(c);
  if (!g.snap_at.empty() && a[0] < g.snap_at.back()) throw MissingResource{"gfx.snapshotAt: requests out of order"};
  g.snap_at.push_back(a[0]);
  r.set_i32(static_cast<int32_t>(g.snap_at.size() - 1));
}
// gfx.dump(which): the back buffer (0) or the root surface (1) as it is now, into `$gfx/back.555` / `$gfx/root.555`
void a_dump(Ctx &c, const uint32_t *a, Ret &r) {
  Gfx &g = need(c);
  g.ensure_root(c);
  r.set_i32(gfxb::put_file(c, a[0] ? "$gfx/root.555" : "$gfx/back.555", gfxb::surface_bytes(a[0] ? g.root : g.back)) ? 0 : 1);
}
// gfx.log(): `$gfx/log.txt`, one line per handled and per unsupported case with its count, then `first\t<k>\t<reason>` for
// every distinct unsupported reason in first-occurrence order (snapshot k's `unsupported before it` count n means the first n
// of them; port/test/framegate.py reads them); returns the number of unsupported cases (distinct reasons)
void a_log(Ctx &c, const uint32_t *, Ret &r) {
  Gfx &g = need(c);
  std::string s;
  for (const auto &kv : g.handled) s += "handled\t" + kv.first + "\t" + std::to_string(kv.second) + "\n";
  for (const auto &kv : g.unsupported) s += "unsupported\t" + kv.first + "\t" + std::to_string(kv.second) + "\n";
  for (size_t k = 0; k < g.notes.size(); ++k) s += "first\t" + std::to_string(k) + "\t" + g.notes[k] + "\n";
  s += "snapshots\t" + std::to_string(g.snap_next) + " of " + std::to_string(g.snap_at.size()) + "\n";
  for (size_t k = 0; k < g.snap_unsupported.size(); ++k)
    s += "snapshot\t" + std::to_string(k) + "\tunsupported before it\t" + std::to_string(g.snap_unsupported[k]) + "\n";
  for (size_t k = 0; k < g.snap_terrain.size(); ++k)
    s += "snapshot_terrain\t" + std::to_string(k) + "\t" + std::to_string(g.snap_terrain[k]) + "\n";
  gfxb::put_file(c, "$gfx/log.txt", std::vector<uint8_t>(s.begin(), s.end()));
  r.set_i32(static_cast<int32_t>(g.unsupported.size()));
}

// gfx.call(name, a0..a10): test entry, gfx_drop(name, a0..a10) with the NUL-terminated drop name at the address `name` of the
// context's address space; returns 1 when a body handled it. Lets a test drive one body without a trace.
void a_call(Ctx &c, const uint32_t *a, Ret &r) {
  need(c);
  const std::string name = gfxb::vstring(c.mem, a[0]);
  r.set_i32(gfx_drop(c, name.c_str(), a + 1, 11) ? 1 : 0);
}
// gfx.check(): `$gfx/bodies.txt`, one line `body\t<name>\t<owner>` per registered body, `not_drawing\t<name>\t<owner>` per
// not-drawing drop and `duplicate\t<name>\t<owner>\t<other owner>` per name registered twice (the later one is ignored);
// returns the number of duplicates. Needs no backend attached, only the context's files overlay.
void a_check(Ctx &c, const uint32_t *, Ret &r) {
  const BodyIndex &ix = body_index();
  std::string s;
  for (const auto &kv : ix.bodies) s += "body\t" + kv.first + "\t" + kv.second.second + "\n";
  for (const auto &kv : ix.not_drawing) s += "not_drawing\t" + kv.first + "\t" + kv.second + "\n";
  for (const auto &d : ix.duplicates) s += "duplicate\t" + d + "\n";
  gfxb::put_file(c, "$gfx/bodies.txt", std::vector<uint8_t>(s.begin(), s.end()));
  r.set_i32(static_cast<int32_t>(ix.duplicates.size()));
}
// gfx.family.<f>(): the number of bodies family f registers (its registry entry carries the family's negctl switch)
template <const GfxFamily &F>
void a_family(Ctx &, const uint32_t *, Ret &r) {
  r.set_i32(static_cast<int32_t>(F.count));
}

const FuncDef kFuncs[] = {
    {"gfx.attach", "default", "int", 0, 0, a_attach, &g_negctl_gfx},
    {"gfx.detach", "default", "int", 0, 0, a_detach, &g_negctl_gfx},
    {"gfx.snapshotAt", "default", "int", 1, 0, a_snapshot_at, &g_negctl_gfx},
    {"gfx.dump", "default", "int", 1, 0, a_dump, &g_negctl_gfx},
    {"gfx.log", "default", "int", 0, 0, a_log, &g_negctl_gfx},
    {"gfx.call", "default", "int", 12, 0, a_call, &g_negctl_gfx},
    {"gfx.check", "default", "int", 0, 0, a_check, &g_negctl_gfx},
#define SG_GFX_FAMILY(f) {"gfx.family." #f, "default", "int", 0, 0, a_family<kGfxFamily_##f>, &g_negctl_gfx_##f},
#include "gfx_families.inc"
#undef SG_GFX_FAMILY
};
}  // namespace

extern int g_defect_themepack_col5;  // gfx_boot_screens.cpp
extern int g_defect_tip_dwell;       // gfx_hud.cpp (P6B)
extern int g_defect_tip_box;         // gfx_text.cpp (P11B fix 2)
namespace {
const DefectDef kDefects[] = {
    {"showThemePackSelect.column5StackWord", "P5A",
     "the theme-pack screen draws a sixth column cell at a stack word the original never writes (U-1260: a yellow cell at x -17 on "
     "the current theme's row); fixed: no column-5 cell",
     &g_defect_themepack_col5},
    {"hoverTip.tenFrameDwell", "P6B",
     "the HUD panels draw a control's hover tip only after more than 10 frames on it (about 1.0 s; showInfoCursorPanel+0xc7 / "
     "+0x76f, drawTerrainToolbar+0x140, drawBuildingPanel+0x55, drawAmenitiesPanel+0x4fd, drawEmployeeViewPanel+0x6ca, "
     "drawGolferActionPanel+0x635); fixed: more than 3 frames (about 0.37 s)",
     &g_defect_tip_dwell},
    {"infoCursorTip.fixedBox", "P11B",
     "the Build palette's tool tip is a fixed drawFrameR3 box (0xa0 x 0x70 at row 0x192) with its horizontal meter at row 0x1ea, "
     "drawn before the palette, so the palette covers the box from row 0x1e2 down and the meter with it; fixed: the box ends 8 "
     "rows under the meter, which sits 8 rows under the description or the vertical meter, moved up to stay above row 0x1e2",
     &g_defect_tip_box},
    {"drawPanelBackdrop.barOnTerrainOnly", "P13B",
     "drawPanelBackdrop draws its tip's dark bar only as a terrain-view line (drawMapLine), which a 2D screen never shows, so the "
     "white tip text of the character editor stands on the editor's own light art; fixed: while the editor is open the bar is "
     "also filled into the 2D target under the text",
     &gfxb::g_defect_backdrop_gl_only},
};
}  // namespace

extern const ModuleDef kModule_gfx = {"gfx", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                      sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
