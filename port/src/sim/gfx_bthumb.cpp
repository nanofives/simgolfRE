// P41 feedback 1, item 8: the build palette's building thumbnails from the world art (portsim/sim/gfx_bthumb.h has the contract,
// the measurements and the negative controls; notes log/roadmap/P41/feedback1/item8_notes.md). Functions of the original are cited
// by name.
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/gfx_bthumb.h"

#include <algorithm>
#include <chrono>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>

#include "commands_internal.h"   // cm::call_peer (cellAt)
#include "gfx_body.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"   // SG_NEGCTL
#include "portsim/sim/gfx.h"
#include "portsim/sim/gfx_hud.h"   // gfxh::palette_of
#include "portsim/sim/gfx_ui.h"    // ui_draw_uvec

namespace portsim {

extern int g_negctl_uvgame;   // gfx_uvgame.cpp

namespace {

struct BtPart {
  int16_t bank, frame, shade, size;
};
struct BtEntry {
  int8_t t, slot, v;
  float scale, ox, oy;   // thumbnail pixel (u, v) shows the work-surface point (ox + u / scale, oy + v / scale)
  int16_t tag;           // the palette object g_images + 0x58 * tag
  int8_t n;
  BtPart p[6];
};
#include "gfx_bthumb_table.inc"

constexpr int32_t kCellW = 75, kCellH = 100;   // loadWorldAssets' layout cut: 0x4b x 0x64

const BtEntry *entry(int t, int slot, int v) {
  for (const BtEntry &e : kBt)
    if (e.t == t && e.slot == slot && e.v == v) return &e;
  return nullptr;
}

// The composed picture of one entry on the world's work surface: premultiplied RGBA bytes over [x0, x0 + w) x [y0, y0 + h)
struct Comp {
  int32_t x0 = 0, y0 = 0, w = 0, h = 0;
  std::vector<uint8_t> px;   // premultiplied, alpha 0..255 (a half-bright pixel over a part's pixel is darkened into it)
  std::vector<uint8_t> sh;   // a half-bright pixel over nothing: its coverage (128), drawn only where the art's thumbnail is opaque
  std::vector<uint8_t> art;  // the art cut's opaque pixels (kCellW x kCellH, 1 = opaque): the plot the art draws its picture on
  const BtEntry *e = nullptr;
};
std::mutex &mu() {
  static std::mutex m;
  return m;
}
std::map<std::string, std::shared_ptr<const Comp>> &comps() {
  static std::map<std::string, std::shared_ptr<const Comp>> c;
  return c;
}

// flic.cpp crop_rect: the corner and size of loadFlic's crop on the 480 x 480 work surface for a load's size argument
void crop_rect(int32_t size, int32_t &x, int32_t &y, int32_t &w, int32_t &h) {
  if (size == 0) { x = 0xd2; y = 0xc8; w = h = 0x3c; return; }
  if (size == 1 || size == -1) { x = y = 0x64; w = 0x118; h = 0xf0; return; }
  if (size == 0xf0) { x = y = 0; w = h = 0x1e0; return; }
  x = y = 0xf0 - size;
  w = 2 * size;
  h = 3 * size / 2;
}

inline uint8_t c5to8(uint16_t w, int sh) {
  const uint32_t l = (w >> sh) & 31u;
  return static_cast<uint8_t>((l << 3) | (l >> 2));
}

// Composes entry e from the backend's world cells (sim side: cellAt and the palette objects live there). The parts are drawn in
// order; a part with shade k is drawn as drawQueuedSprite.draw draws a queue entry of shade k at zoom 4 on an 800-pixel screen
// (enqueueSprite's place: anchor - a4 * 4 * k / 16 for k > 0, anchor + a4 * 4 * k / 32 (C division) for k < 0, at display scale
// 4k / 16 or -4k / 32; a4 = the crop's half width, the anchor the work surface's centre 0xf0). Payload 0xfe / 0xff is transparent,
// 0xf8..0xfd the half-bright plane (a part's pixel under it halved; over nothing a black half-coverage shadow kept apart and
// drawn only on the art's plot), the rest the palette object's word.
std::shared_ptr<const Comp> compose(Gfx &g, Ctx &ctx, const BtEntry &e, const Sprite8 &art) {
  auto c = std::make_shared<Comp>();
  c->e = &e;
  // the art cut's kept pixels (payload 0xff: the layout sheet's transparent index) at (left, top) of the 75 x 100 cut
  c->art.assign(static_cast<size_t>(kCellW) * kCellH, 0);
  for (int32_t j = 0; j < art.h; ++j)
    for (int32_t i = 0; i < art.w; ++i) {
      const int32_t u = art.left + i, v = art.top + j;
      if (u < 0 || v < 0 || u >= kCellW || v >= kCellH) continue;
      if (art.px[static_cast<size_t>(j) * static_cast<size_t>(art.w) + static_cast<size_t>(i)] != 0xff)
        c->art[static_cast<size_t>(v) * kCellW + static_cast<size_t>(u)] = 1;
    }
  c->x0 = static_cast<int32_t>(std::floor(e.ox)) - 2;
  c->y0 = static_cast<int32_t>(std::floor(e.oy)) - 2;
  c->w = static_cast<int32_t>(std::ceil(kCellW / e.scale)) + 5;
  c->h = static_cast<int32_t>(std::ceil(kCellH / e.scale)) + 5;
  c->px.assign(static_cast<size_t>(c->w) * static_cast<size_t>(c->h) * 4u, 0);
  c->sh.assign(static_cast<size_t>(c->w) * static_cast<size_t>(c->h), 0);
  const uint32_t pal_obj = state_va(ctx, &GameState::g_images) + 0x58u * static_cast<uint32_t>(e.tag);
  const std::array<uint16_t, 256> *pobj = gfxh::palette_of(g, pal_obj, false);
  int drawn = 0;
  for (int k = 0; k < e.n; ++k) {
    const BtPart &p = e.p[k];
    const uint32_t key = cm::call_peer(ctx, "cellAt", {static_cast<uint32_t>(p.bank), static_cast<uint32_t>(p.frame), 0u}).eax;
    auto it = g.sprites.find(key);
    if (it == g.sprites.end() || it->second.w <= 0 || it->second.h <= 0) continue;
    const Sprite8 &s = it->second;
    const uint16_t *pal = pobj ? pobj->data() : s.pal.data();
    int32_t cx, cy, cw, ch;
    crop_rect(p.size, cx, cy, cw, ch);
    const int32_t a4 = cw / 2;
    double org, sc;
    if (p.shade > 0) {
      org = 0xf0 - (4 * a4 * p.shade) / 16;
      sc = 4.0 * p.shade / 16.0;
    } else {
      org = 0xf0 + (4 * a4 * p.shade) / 32;
      sc = -4.0 * p.shade / 32.0;
    }
    // every composite pixel whose centre falls on a sprite pixel (nearest, as the scaled sprite draw)
    const double bx = org + sc * s.left - c->x0, by = org + sc * s.top - c->y0;
    const int32_t i0 = std::max(0, static_cast<int32_t>(std::floor(bx))), i1 = std::min(c->w, static_cast<int32_t>(std::ceil(bx + sc * s.w)));
    const int32_t j0 = std::max(0, static_cast<int32_t>(std::floor(by))), j1 = std::min(c->h, static_cast<int32_t>(std::ceil(by + sc * s.h)));
    for (int32_t j = j0; j < j1; ++j) {
      const int32_t sy = static_cast<int32_t>(std::floor((j + 0.5 - by) / sc));
      if (sy < 0 || sy >= s.h) continue;
      for (int32_t i = i0; i < i1; ++i) {
        const int32_t sxp = static_cast<int32_t>(std::floor((i + 0.5 - bx) / sc));
        if (sxp < 0 || sxp >= s.w) continue;
        const uint8_t b = s.px[static_cast<size_t>(sy) * static_cast<size_t>(s.w) + static_cast<size_t>(sxp)];
        if (b >= 0xfe) continue;
        const size_t at = static_cast<size_t>(j) * static_cast<size_t>(c->w) + static_cast<size_t>(i);
        uint8_t *d = &c->px[at * 4u];
        if (b >= 0xf8) {   // half-bright: over a part's pixel darkened into it, over nothing kept apart (sh)
          if (d[3] == 0) {
            c->sh[at] = 128;
          } else {
            for (int q = 0; q < 3; ++q) d[q] = static_cast<uint8_t>(d[q] / 2);
          }
          continue;
        }
        c->sh[at] = 0;
        const uint16_t w = pal[b];
        d[0] = c5to8(w, 10);
        d[1] = c5to8(w, 5);
        d[2] = c5to8(w, 0);
        d[3] = 255;
      }
    }
    ++drawn;
  }
  if (drawn == 0) return nullptr;
  return c;
}

std::string key_of(int t, int slot, int row) {
  char b[48];
  std::snprintf(b, sizeof b, "%d.%d.%d", t, slot, row);
  return b;
}

struct BtRun {
  int t = 0, slot = 0, row = 0;
  int32_t dx = 0, dy = 0;
};
// "uvg:B<t>.<slot>.<row> <dx> <dy>"
bool parse(const std::string &text, BtRun &r) {
  if (!bthumb_is_text(text)) return false;
  int t, slot, row, n = 0;
  long dx, dy;
  if (std::sscanf(text.c_str() + 5, "%d.%d.%d %ld %ld%n", &t, &slot, &row, &dx, &dy, &n) != 5) return false;
  if (static_cast<size_t>(n) + 5 != text.size() || t < 0 || t > 3 || slot < 0 || slot > 8 || row < 0 || row > 3) return false;
  if (std::labs(dx) > 100000 || std::labs(dy) > 100000) return false;
  r.t = t;
  r.slot = slot;
  r.row = row;
  r.dx = static_cast<int32_t>(dx);
  r.dy = static_cast<int32_t>(dy);
  return true;
}

// the sheet names loadWorldAssets loads per course type (its switch on g_courseType before the layout cuts), as GameFiles keeps
// them (Image8::name: lower case, forward slashes); matched on the theme word and the layout suffix
const char *const kThemeWord[4] = {"parklands", "desert", "trop", "links"};

}  // namespace

bool bthumb_draw(Gfx &g, Ctx &ctx, const Sprite8 &s, int32_t x, int32_t y) {
  if (SG_NEGCTL(g_negctl_uvgame) == 9 || s.sheet.empty() || s.sx < 0 || s.sy < 0) return false;
  if (s.cut_w != kCellW || s.cut_h != kCellH || s.sx % kCellW != 0 || s.sy % kCellH != 0) return false;
  const std::string &n = s.sheet;
  if (n.size() < 18 || n.compare(0, 10, "interface/") != 0 || n.find(" layout") == std::string::npos) return false;
  const int t = static_cast<int>(static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_courseType))));
  if (t < 0 || t > 3 || n.find(kThemeWord[t]) == std::string::npos) return false;
  const int slot = s.sx / kCellW, row = s.sy / kCellH;
  if (slot > 8 || row > 3) return false;
  const BtEntry *e = entry(t, slot, row & 1);
  if (e == nullptr) return false;   // the driving range: its art
  const std::string k = key_of(t, slot, row);
  bool have;
  {
    std::lock_guard<std::mutex> l(mu());
    have = comps().count(k) != 0;
  }
  if (!have) {
    const auto t0 = std::chrono::steady_clock::now();
    std::shared_ptr<const Comp> c = compose(g, ctx, *e, s);
    if (const char *lp = std::getenv("SG_P41G_UILOG")) {   // development: the one composition per course type, slot and row (ms)
      if (FILE *lf = std::fopen(lp, "a")) {
        std::fprintf(lf, "%.3f\t%.2f\tbthumb_compose\t%s\t%d\n", std::chrono::duration<double>(t0.time_since_epoch()).count(),
                     std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), k.c_str(), c ? 1 : 0);
        std::fclose(lf);
      }
    }
    if (!c) return false;   // no world cell held (the course's sheets not loaded): the art
    std::lock_guard<std::mutex> l(mu());
    comps()[k] = c;
  }
  // the cut's pixel (0, 0) (sheet (sx, sy)) lands at (x - left, y - top): the kept pixel (0, 0) is sheet (sx + left, sy + top)
  char b[96];
  std::snprintf(b, sizeof b, "uvg:B%s %d %d", k.c_str(), static_cast<int>(x - s.left), static_cast<int>(y - s.top));
  return ui_draw_uvec(g, ctx, b);
}

bool bthumb_bounds(const std::string &text, float &x0, float &y0, float &x1, float &y1) {
  BtRun r;
  if (!parse(text, r)) return false;
  x0 = static_cast<float>(std::max(r.dx, 0));
  y0 = static_cast<float>(std::max(r.dy, 0));
  x1 = std::max(x0, static_cast<float>(std::min(r.dx + kCellW, 800)));
  y1 = std::max(y0, static_cast<float>(std::min(r.dy + kCellH, 600)));
  return true;
}

bool bthumb_render(const std::string &text, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
                   std::vector<float> &prem) {
  BtRun r;
  if (!parse(text, r)) return false;
  if (prem.size() != static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h)) * 4u)
    prem.assign(static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h)) * 4u, 0.f);
  std::shared_ptr<const Comp> c;
  {
    std::lock_guard<std::mutex> l(mu());
    auto it = comps().find(key_of(r.t, r.slot, r.row));
    if (it != comps().end()) c = it->second;
  }
  if (!c || w <= 0 || h <= 0) return true;
  const BtEntry &e = *c->e;
  const bool grey = r.row < 2 && SG_NEGCTL(g_negctl_uvgame) != 8;
  // the cut at its drawn place in target pixels: a pixel whose centre lies outside is not painted (render_cut's clip)
  const int32_t ci0 = std::max(px0, static_cast<int32_t>(std::ceil(r.dx * sx + ox - 0.5)));
  const int32_t ci1 = std::min(px0 + w, static_cast<int32_t>(std::ceil((r.dx + kCellW) * sx + ox - 0.5)));
  const int32_t cj0 = std::max(py0, static_cast<int32_t>(std::ceil(r.dy * sy + oy - 0.5)));
  const int32_t cj1 = std::min(py0 + h, static_cast<int32_t>(std::ceil((r.dy + kCellH) * sy + oy - 0.5)));
  // exact area coverage (item 8 follow-up; log/roadmap/P41/feedback1/item8/resample_cmp.py): a target pixel's square maps to a
  // rectangle of the composite, and each texel counts by the part of it inside. A box filter when reducing; when enlarging, crisp
  // texels with a one-pixel soft edge (bilinear blurred them, Lanczos rang at the roof edges). Separable: one span per column / row.
  struct Span {
    int32_t j0 = 0;
    std::vector<float> w;
  };
  auto spans = [&](int32_t a0, int32_t a1, double s, double o, int32_t d, double eo, int32_t c0) {
    std::vector<Span> v(static_cast<size_t>(std::max(0, a1 - a0)));
    for (int32_t q = a0; q < a1; ++q) {
      const double X0 = eo + ((q - o) / s - d) / e.scale - c0, X1 = eo + ((q + 1 - o) / s - d) / e.scale - c0;
      Span &sp = v[static_cast<size_t>(q - a0)];
      sp.j0 = static_cast<int32_t>(std::floor(X0));
      const int32_t j1 = static_cast<int32_t>(std::ceil(X1));
      const double inv = 1.0 / (X1 - X0);
      for (int32_t j = sp.j0; j < j1; ++j)
        sp.w.push_back(static_cast<float>(std::max(0.0, std::min(X1, j + 1.0) - std::max(X0, static_cast<double>(j))) * inv));
    }
    return v;
  };
  const std::vector<Span> xs = spans(ci0, ci1, sx, ox, r.dx, e.ox, c->x0), ys = spans(cj0, cj1, sy, oy, r.dy, e.oy, c->y0);
  for (int32_t py = cj0; py < cj1; ++py) {
    const double ly = (py + 0.5 - oy) / sy - r.dy;   // thumbnail row (the pixel centre: the art's plot test below)
    const Span &SY = ys[static_cast<size_t>(py - cj0)];
    for (int32_t px = ci0; px < ci1; ++px) {
      const double lx = (px + 0.5 - ox) / sx - r.dx;
      const Span &SX = xs[static_cast<size_t>(px - ci0)];
      float acc[5] = {0, 0, 0, 0, 0};
      for (size_t b = 0; b < SY.w.size(); ++b) {
        const int32_t yy = SY.j0 + static_cast<int32_t>(b);
        if (yy < 0 || yy >= c->h) continue;
        for (size_t a = 0; a < SX.w.size(); ++a) {
          const int32_t xx = SX.j0 + static_cast<int32_t>(a);
          if (xx < 0 || xx >= c->w) continue;
          const float wgt = SY.w[b] * SX.w[a];
          const size_t at = static_cast<size_t>(yy) * static_cast<size_t>(c->w) + static_cast<size_t>(xx);
          const uint8_t *p = &c->px[at * 4u];
          for (int k = 0; k < 4; ++k) acc[k] += wgt * p[k];
          acc[4] += wgt * c->sh[at];
        }
      }
      const float inv = 1.f;
      float cov = acc[3] * inv / 255.f;
      // the shadow over nothing, inside the art's plot only (the art draws no shadow past its picture)
      const int32_t au = static_cast<int32_t>(std::floor(lx)), av = static_cast<int32_t>(std::floor(ly));
      const bool plot = au >= 0 && av >= 0 && au < kCellW && av < kCellH && c->art[static_cast<size_t>(av) * kCellW + static_cast<size_t>(au)] != 0;
      const float scov = plot ? acc[4] * inv / 255.f : 0.f;
      if (cov <= 0.f && scov <= 0.f) continue;
      float col[3] = {acc[0] * inv, acc[1] * inv, acc[2] * inv};   // premultiplied 0..255
      if (grey && cov > 0.f) {   // the art's grey filter on the straight colour (gfx_bthumb.h)
        const float L = (0.299f * col[0] + 0.587f * col[1] + 0.114f * col[2]) / cov;
        const float g3[3] = {1.0042f * L - 13.22f, 1.0076f * L - 2.62f, 0.9728f * L + 37.56f};
        for (int q = 0; q < 3; ++q) col[q] = std::min(255.f, std::max(0.f, g3[q])) * cov;
      }
      cov = std::min(cov, 1.f);
      cov = cov + (1.f - cov) * std::min(scov, 1.f);   // body over the black shadow: colour unchanged, coverage grows
      float *d = &prem[(static_cast<size_t>(py - py0) * static_cast<size_t>(w) + static_cast<size_t>(px - px0)) * 4u];
      for (int q = 0; q < 3; ++q) d[q] = col[q] + (1.f - cov) * d[q];
      d[3] = cov + (1.f - cov) * d[3];
    }
  }
  return true;
}

// ------------------------------------------------------------------------------------------------ test entries
namespace {
// bthumb.entries(): the table's entries (course type x slot x level with a world picture)
void a_entries(Ctx &, const uint32_t *, Ret &r) { r.set_i32(static_cast<int32_t>(sizeof(kBt) / sizeof(kBt[0]))); }
// bthumb.prepared(): the composed pictures held (one per course type, slot and row drawn with the theme on)
void a_prepared(Ctx &, const uint32_t *, Ret &r) {
  std::lock_guard<std::mutex> l(mu());
  r.set_i32(static_cast<int32_t>(comps().size()));
}
const FuncDef kFuncs[] = {
    {"bthumb.entries", "default", "int", 0, 0, a_entries, &g_negctl_uvgame},
    {"bthumb.prepared", "default", "int", 0, 0, a_prepared, &g_negctl_uvgame},
};
}  // namespace

extern const ModuleDef kModule_gfx_bthumb = {"gfx_bthumb", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
