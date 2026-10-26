// FreeType raster of jgld's TextOutA (roadmap S5.3): portsim/sim/fttext_raster.h. The model's measurements are in
// log/roadmap/S5/3_notes.md; the provider that uses it is FtTextMetrics::draw_glyphs (fttext.cpp).
#include "portsim/sim/fttext_raster.h"

#include "portsim/registry.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#if defined(PORTSIM_HAVE_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_MODULE_H
#include FT_DRIVER_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H
#endif

namespace portsim {
namespace ftraster {

namespace {

constexpr int kSamples = 6;   // horizontal samples per pixel; also the top level

int32_t floordiv(int64_t a, int64_t b) { return static_cast<int32_t>(a >= 0 ? a / b : -((-a + b - 1) / b)); }
int32_t ceildiv(int64_t a, int64_t b) { return -floordiv(-a, b); }

// The 8-bit gamma tables of the blend: dec[i] = round(255 (i / 255)^g), enc[i] = round(255 (i / 255)^(1 / g)),
// g = contrast / 1000, rounded half up (3_blend_check.py)
struct Gamma {
  int contrast = -1;
  uint8_t dec[256];
  uint8_t enc[256];
};
const Gamma &gamma_of(int contrast) {
  static std::mutex mu;
  static Gamma cache[4];
  static int next = 0;
  std::lock_guard<std::mutex> lk(mu);
  for (const Gamma &g : cache)
    if (g.contrast == contrast) return g;
  Gamma &g = cache[next];
  next = (next + 1) % 4;
  const double gm = contrast / 1000.0;
  for (int i = 0; i < 256; ++i) {
    g.dec[i] = static_cast<uint8_t>(std::floor(255.0 * std::pow(i / 255.0, gm) + 0.5));
    g.enc[i] = static_cast<uint8_t>(std::floor(255.0 * std::pow(i / 255.0, 1.0 / gm) + 0.5));
  }
  g.contrast = contrast;
  return g;
}

uint32_t mix(const Gamma &g, uint32_t f, uint32_t b, int a) {
  if (a <= 0) return b;
  if (a >= kSamples) return f;
  const int32_t F = g.dec[f << 3], D = g.dec[b << 3];
  const int64_t al = (static_cast<int64_t>(a) * 65536 * 2 + kSamples) / (2 * kSamples);   // round(a 65536 / 6)
  const int32_t m = D + floordiv((F - D) * al + 32768, 65536);
  return static_cast<uint32_t>(g.enc[m]) >> 3;
}

}  // namespace

uint32_t mix5(int contrast, uint32_t f, uint32_t b, int level) { return mix(gamma_of(contrast), f & 31u, b & 31u, level); }

void accumulate(Levels &run, const Levels &g) {
  if (g.w <= 0 || g.h <= 0) return;
  if (run.w <= 0 || run.h <= 0) {
    run = g;
    return;
  }
  const int32_t x0 = std::min(run.x0, g.x0), y0 = std::min(run.y0, g.y0);
  const int32_t x1 = std::max(run.x0 + run.w, g.x0 + g.w), y1 = std::max(run.y0 + run.h, g.y0 + g.h);
  if (x0 != run.x0 || y0 != run.y0 || x1 != run.x0 + run.w || y1 != run.y0 + run.h) {
    Levels n;
    n.x0 = x0, n.y0 = y0, n.w = x1 - x0, n.h = y1 - y0;
    n.lv.assign(static_cast<size_t>(n.w) * n.h * 3, 0);
    for (int32_t r = 0; r < run.h; ++r)
      std::memcpy(&n.lv[(static_cast<size_t>(run.y0 - y0 + r) * n.w + (run.x0 - x0)) * 3],
                  &run.lv[static_cast<size_t>(r) * run.w * 3], static_cast<size_t>(run.w) * 3);
    run = std::move(n);
  }
  for (int32_t r = 0; r < g.h; ++r) {
    uint8_t *d = &run.lv[(static_cast<size_t>(g.y0 - run.y0 + r) * run.w + (g.x0 - run.x0)) * 3];
    const uint8_t *s = &g.lv[static_cast<size_t>(r) * g.w * 3];
    for (int32_t i = 0; i < g.w * 3; ++i) {
      const int v = d[i] + s[i];
      d[i] = static_cast<uint8_t>(v > kSamples ? kSamples : v);
    }
  }
}

void blend555(const Levels &lv, uint32_t colorref, uint16_t *px, int32_t w, int32_t h, const Options &o) {
  const Gamma &g = gamma_of(o.contrast);
  const uint32_t fr = (colorref & 0xffu) >> 3, fg = ((colorref >> 8) & 0xffu) >> 3, fb = ((colorref >> 16) & 0xffu) >> 3;
  for (int32_t r = 0; r < lv.h; ++r) {
    const int32_t y = lv.y0 + r;
    if (y < 0 || y >= h) continue;
    for (int32_t c = 0; c < lv.w; ++c) {
      const int32_t x = lv.x0 + c;
      if (x < 0 || x >= w) continue;
      const uint8_t *l = &lv.lv[3u * (static_cast<size_t>(r) * lv.w + c)];
      if (!l[0] && !l[1] && !l[2]) continue;
      uint16_t &v = px[static_cast<size_t>(y) * w + x];
      const uint32_t br = (v >> 10) & 31u, bg = (v >> 5) & 31u, bb = v & 31u;
      v = static_cast<uint16_t>((mix(g, fr, br, l[0]) << 10) | (mix(g, fg, bg, l[1]) << 5) | mix(g, fb, bb, l[2]));
    }
  }
}

#if defined(PORTSIM_HAVE_FREETYPE)

bool set_interpreter(FT_LibraryRec_ *lib, unsigned version) {
  FT_UInt v = version;
  return FT_Property_Set(lib, "truetype", "interpreter-version", &v) == 0;
}

int y_smoothing(FT_FaceRec_ *face, int32_t ppem) {
  FT_ULong len = 0;
  if (FT_Load_Sfnt_Table(face, TTAG_gasp, 0, nullptr, &len) || len < 4) return kSamples;
  std::vector<uint8_t> t(len);
  if (FT_Load_Sfnt_Table(face, TTAG_gasp, 0, t.data(), &len)) return kSamples;
  const unsigned n = (t[2] << 8) | t[3];
  for (unsigned i = 0; i < n && 4 + 4 * i + 4 <= len; ++i) {
    const unsigned maxppem = (t[4 + 4 * i] << 8) | t[5 + 4 * i];
    const unsigned flags = (t[6 + 4 * i] << 8) | t[7 + 4 * i];
    if (static_cast<unsigned>(ppem) <= maxppem) return (flags & (1u << 3)) ? kSamples : 1;
  }
  return 1;
}

bool glyph_levels(FT_FaceRec_ *face, uint32_t glyph_index, int32_t pen_x, int32_t base_y, int32_t advance,
                  const Options &o, Levels &out) {
  out = Levels();
  const int V = o.subrows > 1 ? o.subrows : 1;
  FT_Pos sh = o.shift;   // 26.6 sample units
  if (o.centre) {
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING)) return false;
    const int64_t twice = static_cast<int64_t>(advance) * kSamples * 64 - static_cast<int64_t>(face->glyph->advance.x) * kSamples;
    sh += floordiv(twice + 1, 2);   // floor(twice / 2 + 1/2)
  }
  const FT_Int32 flags = FT_LOAD_NO_BITMAP | (o.hinting ? FT_LOAD_TARGET_NORMAL : FT_LOAD_NO_HINTING);
  if (FT_Load_Glyph(face, glyph_index, flags)) return false;
  FT_GlyphSlot slot = face->glyph;
  if (slot->format != FT_GLYPH_FORMAT_OUTLINE) return false;
  FT_Outline &ol = slot->outline;
  if (ol.n_points == 0 || ol.n_contours <= 0) return true;
  FT_Matrix m = {static_cast<FT_Fixed>(kSamples) << 16, 0, 0, static_cast<FT_Fixed>(V) << 16};
  FT_Outline_Transform(&ol, &m);
  FT_Outline_Translate(&ol, sh, 0);
  FT_BBox cb;
  FT_Outline_Get_CBox(&ol, &cb);
  const int32_t kx0 = floordiv(cb.xMin, 64) - 1, kx1 = ceildiv(cb.xMax, 64) + 1;
  const int32_t u0 = floordiv(floordiv(cb.yMin, 64) - 1, V) * V, u1 = ceildiv(ceildiv(cb.yMax, 64) + 1, V) * V;
  const int32_t cols = kx1 - kx0, rows = u1 - u0;
  FT_Outline_Translate(&ol, -static_cast<FT_Pos>(kx0) * 64, -static_cast<FT_Pos>(u0) * 64);
  const int pitch = (cols + 7) / 8;
  std::vector<uint8_t> buf(static_cast<size_t>(pitch) * rows, 0);
  FT_Bitmap bm;
  std::memset(&bm, 0, sizeof(bm));
  bm.rows = static_cast<unsigned>(rows);
  bm.width = static_cast<unsigned>(cols);
  bm.pitch = pitch;
  bm.buffer = buf.data();
  bm.num_grays = 2;
  bm.pixel_mode = FT_PIXEL_MODE_MONO;
  if (FT_Outline_Get_Bitmap(slot->library, &ol, &bm)) return false;
  // bitmap row j (top first) is subrow u = u1 - 1 - j above the baseline; pixel rows from the top: base_y - u1 / V ..
  const int32_t prow = rows / V;
  const int32_t G0 = pen_x * kSamples + kx0;   // global sample of column 0
  const int32_t p0 = floordiv(G0 - 8, kSamples), p1 = floordiv(G0 + cols + 2, kSamples) + 1;
  out.x0 = p0;
  out.w = p1 - p0;
  out.y0 = base_y - u1 / V;
  out.h = prow;
  out.lv.assign(static_cast<size_t>(out.w) * out.h * 3, 0);
  std::vector<int32_t> pre(static_cast<size_t>(cols) + 1);
  for (int32_t r = 0; r < prow; ++r) {
    pre[0] = 0;
    for (int32_t k = 0; k < cols; ++k) {
      int32_t s = 0;
      for (int32_t j = r * V; j < r * V + V; ++j) s += (buf[static_cast<size_t>(j) * pitch + (k >> 3)] >> (7 - (k & 7))) & 1;
      pre[k + 1] = pre[k] + s;
    }
    if (pre[cols] == 0) continue;
    auto count = [&](int32_t K) {   // set samples in global [K, K + 6), all subrows
      int32_t a = K - G0, b = K + kSamples - G0;
      if (a < 0) a = 0;
      if (b > cols) b = cols;
      return a < b ? pre[b] - pre[a] : 0;
    };
    for (int32_t p = p0; p < p1; ++p)
      for (int c = 0; c < 3; ++c) {
        const int sub = o.bgr ? 2 - c : c;
        const int32_t cnt = count(p * kSamples + 2 * sub - 2);
        out.lv[3u * (static_cast<size_t>(r) * out.w + (p - p0)) + c] = static_cast<uint8_t>((2 * cnt + V) / (2 * V));
      }
  }
  return true;
}

#else   // no FreeType: the raster cannot run

bool set_interpreter(FT_LibraryRec_ *, unsigned) { return false; }
int y_smoothing(FT_FaceRec_ *, int32_t) { return 1; }
bool glyph_levels(FT_FaceRec_ *, uint32_t, int32_t, int32_t, int32_t, const Options &, Levels &out) {
  out = Levels();
  return false;
}

#endif

}  // namespace ftraster

// ------------------------------------------------------------------------------------------------ registry
// Module `fttext_raster`: the raster's test entry (port/test/test_fttext_raster.py). The raster itself runs through
// FtTextMetrics::draw_glyphs (module fttext).
namespace {
// fttext_raster.blend5(f, b, level, contrast): one channel of the 5-5-5 ClearType blend, text 5 bits f, surface 5 bits b,
// level 0..6, contrast (0 = 1200, the measured host's)
void a_blend5(Ctx &, const uint32_t *a, Ret &r) {
  r.set_i32(static_cast<int32_t>(ftraster::mix5(a[3] ? static_cast<int>(a[3]) : 1200, a[0], a[1], static_cast<int>(a[2]))));
}
const FuncDef kFuncs[] = {
    {"fttext_raster.blend5", "default", "int", 4, 0, a_blend5, nullptr},
};
}  // namespace

extern const ModuleDef kModule_fttext_raster = {"fttext_raster", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
