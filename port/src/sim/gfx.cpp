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
#include "portsim/sim/gfx_hud.h"
#include "portsim/sim/gfx_scene.h"
#include "portsim/sim/gfx_text.h"

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
      if (*sp < key) *dp = s.pal[*sp];
  }
}

// ------------------------------------------------------------------------------------------------ the bodies

static uint32_t sref(Ctx &ctx, SpriteRef (GameState::*arr)[14], uint32_t k) { return state_va(ctx, arr) + 0x2c * k; }

// showTitleScreen (c3fb.cpp ShowTitleScreen_re): the drawing part. Its frame Image takes four loads with flags 2; the
// first two are cut into the 8-piece window-frame sets g_spriteRef_562368 / g_spriteRef_59d920 (4 sets, kind 1, no trim),
// the last two into the 7 title buttons, unselected into g_spriteRef_53b7e0[2b], highlighted into [2b + 1] (kind 1, trim).
// The intro movie (playAudioFile) is the video backend's.
static void b_show_title_screen(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  struct Piece { int k, sx, dy, w, h; };
  static const Piece kFrame[8] = {{1, 0, 0, 0x10, 0x10},    {2, 0, 0x11, 0x10, 0x10},     {3, 0, 0x22, 0x10, 0x1a},
                                  {4, 0x11, 0, 0x10, 0x10}, {5, 0x22, 0, 0x1a, 0x10},     {6, 0x22, 0x11, 0x1a, 0x10},
                                  {7, 0x22, 0x22, 0x1a, 0x1a}, {0, 0x11, 0x22, 0x10, 0x1a}};
  struct Button { int x, y, w, h, unsel; };
  static const Button kButtons[7] = {{0x2a, 0x15, 0x144, 0x79, 0},  {0x1a0, 0x1f, 0x16b, 0x8d, 2}, {0x1fe, 0x167, 0x10f, 0x6f, 4},
                                     {0x11f, 0x1da, 0x120, 0x73, 6}, {0xd, 0x17e, 0x11f, 0x73, 8},   {0xa8, 0xc6, 0x1d8, 0xb0, 12},
                                     {0x2d1, 0x210, 0x3a, 0x39, 10}};
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
    for (const Button &b : kButtons)
      crop_sprite(g, sref(ctx, &GameState::g_spriteRef_53b7e0, b.unsel), img, b.x, b.y, b.w, b.h, true);
  if (load_image(g, ctx, pool + 0x00, 0, 0, 0x100, 2, img) == 0)
    for (const Button &b : kButtons)
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
      d.px[static_cast<size_t>(dy) * static_cast<size_t>(d.w) + static_cast<size_t>(dx)] =
          s.pal[s.px[static_cast<size_t>(sy) * static_cast<size_t>(s.pitch) + static_cast<size_t>(sx)]];
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
  draw16(s, g.root, x, y);
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
void draw_text_string(Gfx &g, Ctx &ctx, std::string text, uint32_t va, int32_t x0, int32_t y0, uint32_t colour, bool centre,
                      const char *who) {
  g.ensure_root(ctx);
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) {
    g.unsup(std::string(who) + ": no text provider");
    return;
  }
  if (text.empty()) return;
  // roadmap S3.7: markup, tabs and run-palette colours are the text family's (gfx_text.cpp gfxt::draw_string)
  bool rich = !(colour & 0x80000000u);
  for (char c : text)
    if (c == '{' || c == '}' || c == '[' || c == ']' || c == '$' || c == '=' || c == '^' || c == '\t') rich = true;
  if (rich) {
    gfxt::draw_string(g, ctx, text, x0, y0, colour, centre, who);
    return;
  }
  const uint32_t font = g.fonts[0];
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return;   // drawText's guard: no font, nothing drawn
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
  if (!tm->draw_glyphs(ctx, font, text, x, y, r | (gr << 8) | (b << 16), g.root.px.data(), g.root.w, g.root.h))
    g.unsup(std::string(who) + ": the provider cannot draw");
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
  const int32_t w = mul32(static_cast<int32_t>(vstrlen(ctx.mem, text)), 3);
  int32_t half = static_cast<int32_t>(a[0]);
  if (half < w) half = w;
  if (half > sub32(0x320, w)) half = sub32(0x320, w);
  // roadmap S3.8: the bar is Terrain::drawLine through the overlay body (terrain_overlay.h); without a GL context it draws nothing
  if (GfxBody bar = gfxb::find_body("drawMapLine")) {
    const uint32_t la[7] = {static_cast<uint32_t>(sub32(half, w)), a[1], static_cast<uint32_t>(add32(half, w)), a[1], 0x80000000u, 0xau, 5u};
    bar(g, ctx, la, 7);
  }
  const uint32_t args[4] = {text, static_cast<uint32_t>(half), static_cast<uint32_t>(sub32(static_cast<int32_t>(a[1]), 5)),
                            0x80007fffu};
  draw_text(g, ctx, args, true, "drawPanelBackdrop");
}

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
  gfxe::compose(g, ctx);   // roadmap S3.19: Window::blitToTarget of every shown child window over the root's copy
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
      if (k == 0 || wrong) {
        w = sw;
        continue;
      }
      auto ch = [&](uint32_t dv, uint32_t sv) {
        return static_cast<uint16_t>(static_cast<uint16_t>(((dv * k) >> 8) + sv) >> 3);
      };
      const uint16_t rr = static_cast<uint16_t>(ch((w >> 7) & 0xf8u, (sw >> 7) & 0xf8u) << 10);
      const uint16_t gg = static_cast<uint16_t>(ch((w >> 2) & 0xf8u, (sw >> 2) & 0xf8u) << 5);
      const uint16_t bb = ch((static_cast<uint32_t>(w) << 3) & 0xf8u, (static_cast<uint32_t>(sw) << 3) & 0xf8u);
      w = static_cast<uint16_t>(rr | gg | bb);
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
      if (c == 0xf8 && SG_NEGCTL(g_negctl_gfx) != 6) {
        if (w >= lut_n) {
          g.unsup("blitPanel: a destination word above the RGB555 range");
          continue;
        }
        w = lut[w];
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
  const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
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

bool gfx_drop(Ctx &ctx, const char *name, const uint32_t *a, int n) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0) return false;
  Gfx *g = gfx(ctx);
  if (!g) return false;
  if (GfxBody body = gfxb::find_body(name)) {
    ++g->handled[name];
    uint32_t args[12] = {};
    for (int i = 0; i < n && i < 12; ++i) args[i] = a[i];
    try {
      body(*g, ctx, args, n);
    } catch (...) {
      g->unsup(std::string(name) + ": the body failed (an exception)");
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
};
}  // namespace

extern const ModuleDef kModule_gfx = {"gfx", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                      sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
