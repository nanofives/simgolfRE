// Body family `ui` of the graphics backend (P40, log/roadmap/P40/P40_notes.md; portsim/sim/gfx_ui.h; listed in gfx_families.inc):
// the procedural UI pilot. With --ui-pilot game|slate (gfx_set_ui_pilot) showHoleStatsScreen (holestats.cpp) hands its art
// draws to these bodies instead of the HoleSTAT cells and the OkStates close button:
//
//   p40.hs_head (y)            the head cell's place (y = 0): the panel's top, the title plate, the label pills, the value boxes,
//                              the connectors, the score ovals and the comment box's top, clipped to the head cell's rows
//   p40.hs_row (y)             one comment / banner row cell at y (the panel's sides and the comment box's middle)
//   p40.hs_foot (y)            the foot cell at y (the panel's bottom with the button bay and the comment box's bottom)
//   p40.hs_button (x, y, hov)  the close button at (x, y), hov = 1 its hover look
//
// Each body builds a UiScene (800-space vector elements), draws it into the root at scale 1 and keeps it as a UI run (a P9C text
// run with face kUiRunFace and the scene as its text): the presenter draws the same scene again at the window's resolution
// (port/app/textres.cpp). The layout numbers are the art's, measured from the user's install (log/roadmap/P40/holestat_geometry.txt,
// measure_holestat.py): cell rows, pill and box rectangles, the connectors' places. No art byte is used.
//
// Negative controls (g_negctl_gfx_ui, sg_negctl_set "gfx.family.ui"):
//   1 ui_render's frame point is half a pixel off ((x - ox) / sx instead of (x + 0.5 - ox) / sx): every scaled render moves
//   2 p40.hs_row draws nothing (the comment box's middle rows missing)
#include <algorithm>
#include <tuple>
#include <map>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <thread>
#include <vector>

#include "portsim/ptr.h"
#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/sim/gditext.h"
#include "portsim/sim/gfx_ui.h"

namespace portsim {

int g_negctl_gfx_ui = 0;

// ------------------------------------------------------------------------------------------------ the option
void gfx_set_ui_pilot(Ctx &ctx, int32_t theme) {
  if (Gfx *g = gfx(ctx)) g->ui_pilot = theme;
}
int32_t gfx_ui_pilot(Ctx &ctx) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0) return kUiPilotOff;
  Gfx *g = gfx(ctx);
  return g != nullptr ? g->ui_pilot : kUiPilotOff;
}

// ------------------------------------------------------------------------------------------------ the rasteriser
namespace {
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

float sd_rrect(const UiPrim &p, float x, float y) {
  float y0 = p.y0;
  const float cxm = (p.x0 + p.x1) * 0.5f, hx = (p.x1 - p.x0) * 0.5f;
  if (p.bulge != 0 && hx > 0) {  // the top edge raised by bulge at the middle, a parabola to the corners
    const float t = clampf((x - cxm) / hx, -1.f, 1.f);
    y0 -= p.bulge * (1.f - t * t);
  }
  const float cx = cxm, cy = (y0 + p.y1) * 0.5f, hy = (p.y1 - y0) * 0.5f;
  if (hx <= 0 || hy <= 0) return 1e9f;
  const int k = x < cx ? (y < cy ? 0 : 3) : (y < cy ? 1 : 2);
  const float rx = std::min(p.rx[k], hx), ry = std::min(p.ry[k], hy);
  const float qx = std::fabs(x - cx) - (hx - rx), qy = std::fabs(y - cy) - (hy - ry);
  if (qx > 0 && qy > 0) {
    if (rx <= 0 || ry <= 0) return std::sqrt(qx * qx + qy * qy);
    // the ellipse quadrant (rx, ry) around the corner centre: k0 (k0 - 1) / k1, the gradient-normalised implicit distance
    const float ax = qx / rx, ay = qy / ry, bx = qx / (rx * rx), by = qy / (ry * ry);
    const float k0 = std::sqrt(ax * ax + ay * ay), k1 = std::sqrt(bx * bx + by * by);
    return k1 > 0 ? k0 * (k0 - 1.f) / k1 : -std::min(rx, ry);
  }
  return std::max(qx - rx, qy - ry);
}
float sd_segment(const UiPrim &p, float x, float y) {
  const float dx = p.x1 - p.x0, dy = p.y1 - p.y0, l2 = dx * dx + dy * dy;
  const float t = l2 > 0 ? clampf(((x - p.x0) * dx + (y - p.y0) * dy) / l2, 0.f, 1.f) : 0.f;
  const float ex = x - (p.x0 + t * dx), ey = y - (p.y0 + t * dy);
  return std::sqrt(ex * ex + ey * ey) - p.w * 0.5f;
}
// P40B: a field's value at frame point (x, y): bilinear between pixel centres, and beyond the grid the border value plus the
// distance past it
const UiField *field_of(uint64_t id) {
  thread_local std::unordered_map<uint64_t, const UiField *> local;
  auto it = local.find(id);
  if (it != local.end()) return it->second;
  const UiField *f = ui_field_get(id);
  if (f) local[id] = f;
  return f;
}
float sd_field(const UiPrim &p, float x, float y) {
  const UiField *f = field_of(p.field);
  if (!f || f->w < 2 || f->h < 2) return 1e9f;
  const float u = x - p.x0 - 0.5f, v = y - p.y0 - 0.5f;
  const float cu = clampf(u, 0.f, static_cast<float>(f->w - 1)), cv = clampf(v, 0.f, static_cast<float>(f->h - 1));
  const int32_t i0 = std::min(static_cast<int32_t>(cu), f->w - 2), j0 = std::min(static_cast<int32_t>(cv), f->h - 2);
  const float fu = cu - static_cast<float>(i0), fv = cv - static_cast<float>(j0);
  const float *r0 = f->d.data() + static_cast<size_t>(j0) * static_cast<size_t>(f->w) + static_cast<size_t>(i0);
  const float *r1 = r0 + f->w;
  const float val = (r0[0] * (1.f - fu) + r0[1] * fu) * (1.f - fv) + (r1[0] * (1.f - fu) + r1[1] * fu) * fv;
  const float ou = u - cu, ov = v - cv;
  return val + std::sqrt(ou * ou + ov * ov);
}
float sd_prim(const UiPrim &p, float x, float y) {
  if (p.kind == kUiField) return sd_field(p, x, y);
  if (p.kind == kUiSegment) return sd_segment(p, x, y);
  if (p.kind == kUiEllipse) {
    UiPrim e = p;
    const float hx = (p.x1 - p.x0) * 0.5f, hy = (p.y1 - p.y0) * 0.5f;
    for (int i = 0; i < 4; ++i) {
      e.rx[i] = hx;
      e.ry[i] = hy;
    }
    return sd_rrect(e, x, y);
  }
  return sd_rrect(p, x, y);
}
void prim_box(const UiPrim &p, float &x0, float &y0, float &x1, float &y1);
float sd_elem(const UiElem &e, float x, float y) {
  float d = 1e9f;
  for (const UiPrim &p : e.prims) {
    if (e.smooth <= 0 && d < 1e8f) {   // a primitive whose box lies farther than the nearest one so far cannot be nearer
      float bx0, by0, bx1, by1;
      prim_box(p, bx0, by0, bx1, by1);
      const float ox = std::max(std::max(bx0 - x, x - bx1), 0.f), oy = std::max(std::max(by0 - y, y - by1), 0.f);
      if (ox * ox + oy * oy >= d * d && d > 0) continue;
    }
    const float di = sd_prim(p, x, y);
    if (e.smooth > 0 && d < 1e8f) {
      const float h = clampf(0.5f + 0.5f * (di - d) / e.smooth, 0.f, 1.f);
      d = di + (d - di) * h - e.smooth * h * (1.f - h);   // the polynomial smooth minimum
    } else {
      d = std::min(d, di);
    }
  }
  return d;
}
void prim_box(const UiPrim &p, float &x0, float &y0, float &x1, float &y1) {
  const float m = p.kind == kUiSegment ? p.w * 0.5f : 0.f;
  x0 = std::min(p.x0, p.x1) - m;
  x1 = std::max(p.x0, p.x1) + m;
  y0 = std::min(p.y0, p.y1) - m - (p.kind == kUiRRect ? std::max(0.f, p.bulge) : 0.f);
  y1 = std::max(p.y0, p.y1) + m;
}
void elem_box(const UiElem &e, float &x0, float &y0, float &x1, float &y1) {
  x0 = y0 = 1e9f;
  x1 = y1 = -1e9f;
  for (const UiPrim &p : e.prims) {
    float a, b, c, d;
    prim_box(p, a, b, c, d);
    x0 = std::min(x0, a);
    y0 = std::min(y0, b);
    x1 = std::max(x1, c);
    y1 = std::max(y1, d);
  }
  x0 -= 1.f;
  y0 -= 1.f;
  x1 += 1.f;
  y1 += 1.f;
  if (e.shadow.a > 0) {
    const float b = e.shadow_blur + 1.f;
    x0 = std::min(x0, x0 + e.shadow_dx - b);
    y0 = std::min(y0, y0 + e.shadow_dy - b);
    x1 = std::max(x1, x1 + e.shadow_dx + b);
    y1 = std::max(y1, y1 + e.shadow_dy + b);
  }
}
UiRgba lerp(const UiRgba &a, const UiRgba &b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}
void over(float *dst, float r, float g, float b, float a) {   // straight colour (r, g, b) at alpha a over premultiplied dst
  const float k = 1.f - a;
  dst[0] = r * a + dst[0] * k;
  dst[1] = g * a + dst[1] * k;
  dst[2] = b * a + dst[2] * k;
  dst[3] = a + dst[3] * k;
}
}  // namespace

void ui_bounds(const UiScene &s, float &x0, float &y0, float &x1, float &y1) {
  x0 = y0 = 1e9f;
  x1 = y1 = -1e9f;
  for (const UiElem &e : s.elems) {
    float a, b, c, d;
    elem_box(e, a, b, c, d);
    x0 = std::min(x0, a);
    y0 = std::min(y0, b);
    x1 = std::max(x1, c);
    y1 = std::max(y1, d);
  }
  x0 = std::max(x0, s.cx0);
  y0 = std::max(y0, s.cy0);
  x1 = std::min(x1, s.cx1);
  y1 = std::min(y1, s.cy1);
}

namespace {
// the target rows [ra, rb) of ui_render
void render_rows(const UiScene &s, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
                 int32_t ra, int32_t rb, std::vector<float> &prem) {
  (void)h;
  const float half = SG_NEGCTL(g_negctl_gfx_ui) == 1 ? 0.f : 0.5f;
  const float sc = static_cast<float>(std::sqrt(sx * sy));  // frame units -> target pixels (for the edge's antialiasing)
  // the target columns / rows whose frame pixel (the floor mapping) lies in the clip
  auto in_clip = [](double v, float lo, float hi) { const double f = std::floor(v); return f >= lo && f < hi; };
  for (const UiElem &e : s.elems) {
    float ex0, ey0, ex1, ey1;
    elem_box(e, ex0, ey0, ex1, ey1);
    ex0 = std::max(ex0, s.cx0);
    ey0 = std::max(ey0, s.cy0);
    ex1 = std::min(ex1, s.cx1);
    ey1 = std::min(ey1, s.cy1);
    if (ex1 <= ex0 || ey1 <= ey0) continue;
    const int32_t tx0 = std::max(px0, static_cast<int32_t>(std::floor(ex0 * sx + ox)) - 1);
    const int32_t tx1 = std::min(px0 + w, static_cast<int32_t>(std::ceil(ex1 * sx + ox)) + 1);
    const int32_t ty0 = std::max(ra, static_cast<int32_t>(std::floor(ey0 * sy + oy)) - 1);
    const int32_t ty1 = std::min(rb, static_cast<int32_t>(std::ceil(ey1 * sy + oy)) + 1);
    const float gspan = e.gy1 - e.gy0;
    const float eps = 0.25f;
    const bool opaque = e.fill_top.a >= 1.f && e.fill_bot.a >= 1.f;
    const float band = std::max(e.bevel_w, e.outline_w) + 1.f / sc + 1.5f;   // the edge's bands, its antialiasing and a margin
    const float sreach = e.shadow.a > 0 ? std::max(e.shadow_blur, 1.f / sc) * 0.5f + 1.5f : 0.f;
    constexpr int32_t kTile = 16;
    const float tr = static_cast<float>(kTile * 0.70711 / std::min(sx, sy)) + 1.f;   // a tile's half diagonal, frame units
    for (int32_t by = ty0; by < ty1; by += kTile)
      for (int32_t bx = tx0; bx < tx1; bx += kTile) {
        const int32_t bx1 = std::min(bx + kTile, tx1), by1 = std::min(by + kTile, ty1);
        // the tile classified by the distance at its centre (the distances change by at most one frame unit per unit moved):
        // wholly outside the shape and its shadow (skipped), wholly inside past the edge bands (the fill only), or mixed
        const float cxf = static_cast<float>(((bx + bx1) * 0.5 - ox) / sx), cyf = static_cast<float>(((by + by1) * 0.5 - oy) / sy);
        const float dc = sd_elem(e, cxf, cyf);
        bool far = dc - tr > 1.f / sc + 0.5f;
        if (far && e.shadow.a > 0) far = sd_elem(e, cxf - e.shadow_dx, cyf - e.shadow_dy) - tr > sreach;
        if (far) continue;
        const bool deep = opaque && dc + tr < -band;
        for (int32_t ty = by; ty < by1; ++ty) {
          if (!in_clip((ty - oy) / sy, s.cy0, s.cy1)) continue;
          const float fy = static_cast<float>((ty + half - oy) / sy);
          for (int32_t tx = bx; tx < bx1; ++tx) {
            if (!in_clip((tx - ox) / sx, s.cx0, s.cx1)) continue;
            const float fx = static_cast<float>((tx + half - ox) / sx);
            float *dst = prem.data() + (static_cast<size_t>(ty - py0) * static_cast<size_t>(w) + static_cast<size_t>(tx - px0)) * 4u;
            if (deep) {
              UiRgba c = lerp(e.fill_top, e.fill_bot, gspan > 0 ? clampf((fy - e.gy0) / gspan, 0.f, 1.f) : 0.f);
              if (e.gloss.a > 0 && e.gloss_r > 0) {
                const float gx = fx - e.gloss_cx, gy = fy - e.gloss_cy;
                float gk = clampf(1.f - std::sqrt(gx * gx + gy * gy) / e.gloss_r, 0.f, 1.f);
                gk = gk * gk * e.gloss.a;
                c = lerp(c, UiRgba{e.gloss.r, e.gloss.g, e.gloss.b, c.a}, gk);
              }
              over(dst, c.r, c.g, c.b, 1.f);
              continue;
            }
            if (e.shadow.a > 0) {
              const float sd = sd_elem(e, fx - e.shadow_dx, fy - e.shadow_dy);
              const float b = std::max(e.shadow_blur, 1.f / sc);
              float t = clampf(0.5f - sd / b, 0.f, 1.f);
              t = t * t * (3.f - 2.f * t);
              if (t > 0) over(dst, e.shadow.r, e.shadow.g, e.shadow.b, e.shadow.a * t);
            }
            const float d = sd_elem(e, fx, fy);
            const float cov = clampf(0.5f - d * sc, 0.f, 1.f);
            if (cov <= 0) continue;
            UiRgba c = lerp(e.fill_top, e.fill_bot, gspan > 0 ? clampf((fy - e.gy0) / gspan, 0.f, 1.f) : 0.f);
            const float depth = -d;
            if (e.bevel_w > 0 && depth < e.bevel_w) {
              const float nx = sd_elem(e, fx + eps, fy) - sd_elem(e, fx - eps, fy);
              const float ny = sd_elem(e, fx, fy + eps) - sd_elem(e, fx, fy - eps);
              const float nl = std::sqrt(nx * nx + ny * ny);
              if (nl > 0) {
                // the outward normal against the light from above and a little left: (-0.4, -0.92) normalised. The art's rims: the top
                // edge 208 and the left edge 174 over the panel's 148 (HoleSTAT head cell, x 400 / y 150)
                float lam = (nx * -0.39872f + ny * -0.91707f) / nl;
                if (e.inset) lam = -lam;
                float f = clampf(1.f - depth / e.bevel_w, 0.f, 1.f);
                f = f * f * (3.f - 2.f * f);
                if (lam > 0) {
                  const float k = e.bevel_light * lam * f;
                  c.r += (255.f - c.r) * k;
                  c.g += (255.f - c.g) * k;
                  c.b += (255.f - c.b) * k;
                } else {
                  const float k = 1.f - e.bevel_dark * -lam * f;
                  c.r *= k;
                  c.g *= k;
                  c.b *= k;
                }
              }
            }
            if (e.outline_w > 0 && e.outline.a > 0) {
              const float o = clampf((e.outline_w - depth) * sc + 0.5f, 0.f, 1.f) * e.outline.a;
              c = lerp(c, UiRgba{e.outline.r, e.outline.g, e.outline.b, c.a}, o);
            }
            if (e.gloss.a > 0 && e.gloss_r > 0) {
              const float gx = fx - e.gloss_cx, gy = fy - e.gloss_cy;
              float gk = clampf(1.f - std::sqrt(gx * gx + gy * gy) / e.gloss_r, 0.f, 1.f);
              gk = gk * gk * e.gloss.a;
              c = lerp(c, UiRgba{e.gloss.r, e.gloss.g, e.gloss.b, c.a}, gk);
            }
            over(dst, c.r, c.g, c.b, c.a * cov);
          }
        }
      }
  }
}
}  // namespace

void ui_render(const UiScene &s, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
               std::vector<float> &prem) {
  prem.assign(static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h)) * 4u, 0.f);
  if (w <= 0 || h <= 0 || sx <= 0 || sy <= 0) return;
  // bands of rows on the machine's threads (each writes only its rows); a small target on the caller's thread
  const int64_t px = static_cast<int64_t>(w) * h;
  const int32_t n = px < 60000 ? 1 : static_cast<int32_t>(std::min<unsigned>(std::max(1u, std::thread::hardware_concurrency()), 16u));
  const int32_t band = (h + n - 1) / n;
  std::vector<std::thread> ts;
  for (int32_t k = 1; k < n; ++k) {
    const int32_t ra = py0 + k * band, rb = std::min(py0 + h, ra + band);
    if (ra < rb) ts.emplace_back(render_rows, std::cref(s), sx, sy, ox, oy, px0, py0, w, h, ra, rb, std::ref(prem));
  }
  render_rows(s, sx, sy, ox, oy, px0, py0, w, h, py0, std::min(py0 + h, py0 + band), prem);
  for (std::thread &t : ts) t.join();
}

uint16_t ui_over555(const float *p, uint16_t bg) {
  uint16_t out = 0;
  for (int ch = 0; ch < 3; ++ch) {
    const int sh = 10 - 5 * ch;
    const uint32_t l = (bg >> sh) & 31u;
    const float e = static_cast<float>((l << 3) | (l >> 2));
    const float v = p[ch] + (1.f - p[3]) * e;
    const int32_t q = static_cast<int32_t>(std::lround(clampf(v, 0.f, 255.f) * 31.f / 255.f));
    out = static_cast<uint16_t>(out | (static_cast<uint32_t>(q) << sh));
  }
  return out;
}

// ------------------------------------------------------------------------------------------------ P40B: distance fields
namespace {
std::mutex &field_mu() {
  static std::mutex m;
  return m;
}
std::unordered_map<uint64_t, std::unique_ptr<UiField>> &field_store() {
  static std::unordered_map<uint64_t, std::unique_ptr<UiField>> s;
  return s;
}
// the 1-D squared distance transform (Felzenszwalb and Huttenlocher) of f over n samples into d; samples of f >= 1e11 are no
// sites (a row with none gives 1e12 everywhere)
void edt1(const float *f, int32_t n, float *d, std::vector<int32_t> &v, std::vector<double> &z) {
  v.assign(static_cast<size_t>(n), 0);
  z.assign(static_cast<size_t>(n) + 1, 0.0);
  int32_t k = -1;
  for (int32_t q = 0; q < n; ++q) {
    if (f[q] >= 1e11f) continue;
    const double fq = static_cast<double>(f[q]) + static_cast<double>(q) * q;
    double sv = -1e300;
    while (k >= 0) {
      const int32_t p = v[static_cast<size_t>(k)];
      sv = (fq - (static_cast<double>(f[p]) + static_cast<double>(p) * p)) / (2.0 * (q - p));
      if (sv <= z[static_cast<size_t>(k)]) {
        --k;
        continue;
      }
      break;
    }
    ++k;
    v[static_cast<size_t>(k)] = q;
    z[static_cast<size_t>(k)] = k == 0 ? -1e300 : sv;
    z[static_cast<size_t>(k) + 1] = 1e300;
  }
  if (k < 0) {
    for (int32_t q = 0; q < n; ++q) d[q] = 1e12f;
    return;
  }
  int32_t j = 0;
  for (int32_t q = 0; q < n; ++q) {
    while (j < k && z[static_cast<size_t>(j) + 1] < static_cast<double>(q)) ++j;
    const int32_t p = v[static_cast<size_t>(j)];
    d[q] = static_cast<float>(static_cast<double>(q - p) * (q - p) + f[p]);
  }
}
// the distance from every cell to the nearest cell with `on` set (squared, then rooted)
std::vector<float> edt2(const std::vector<uint8_t> &on, int32_t w, int32_t h) {
  const float inf = 1e12f;
  std::vector<float> g(static_cast<size_t>(w) * static_cast<size_t>(h));
  for (size_t i = 0; i < g.size(); ++i) g[i] = on[i] ? 0.f : inf;
  std::vector<float> f(static_cast<size_t>(std::max(w, h))), d(f.size());
  std::vector<int32_t> v;
  std::vector<double> z;
  for (int32_t x = 0; x < w; ++x) {
    for (int32_t y = 0; y < h; ++y) f[static_cast<size_t>(y)] = g[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
    edt1(f.data(), h, d.data(), v, z);
    for (int32_t y = 0; y < h; ++y) g[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = d[static_cast<size_t>(y)];
  }
  for (int32_t y = 0; y < h; ++y) {
    float *row = g.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
    edt1(row, w, d.data(), v, z);
    for (int32_t x = 0; x < w; ++x) row[x] = std::sqrt(d[static_cast<size_t>(x)]);
  }
  return g;
}
}  // namespace

uint64_t ui_field_put(int32_t w, int32_t h, std::vector<float> d) {
  uint64_t id = 1469598103934665603ull ^ (static_cast<uint64_t>(static_cast<uint32_t>(w)) << 32 | static_cast<uint32_t>(h));
  for (float v : d) {
    uint32_t b;
    std::memcpy(&b, &v, 4);
    id = (id ^ b) * 1099511628211ull;
  }
  if (id == 0) id = 1;
  std::lock_guard<std::mutex> l(field_mu());
  auto &s = field_store();
  if (!s.count(id)) {
    auto f = std::make_unique<UiField>();
    f->w = w;
    f->h = h;
    f->d = std::move(d);
    s.emplace(id, std::move(f));
  }
  return id;
}
const UiField *ui_field_get(uint64_t id) {
  std::lock_guard<std::mutex> l(field_mu());
  auto it = field_store().find(id);
  return it == field_store().end() ? nullptr : it->second.get();
}
std::vector<float> ui_field_from_mask(const std::vector<uint8_t> &mask, int32_t w, int32_t h, int32_t pad) {
  const int32_t W = w + 2 * pad, H = h + 2 * pad;
  std::vector<uint8_t> in(static_cast<size_t>(W) * static_cast<size_t>(H), 0), out(in.size(), 1);
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x)
      if (mask[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]) {
        const size_t i = static_cast<size_t>(y + pad) * static_cast<size_t>(W) + static_cast<size_t>(x + pad);
        in[i] = 1;
        out[i] = 0;
      }
  const std::vector<float> to_in = edt2(in, W, H), to_out = edt2(out, W, H);
  std::vector<float> sd(in.size());
  for (size_t i = 0; i < sd.size(); ++i) sd[i] = in[i] ? -(to_out[i] - 0.5f) : to_in[i] - 0.5f;
  std::vector<float> sm(sd.size());   // one 3 x 3 box pass: the staircase of a pixel mask softened (straight edges keep their place)
  for (int32_t y = 0; y < H; ++y)
    for (int32_t x = 0; x < W; ++x) {
      float acc = 0;
      int32_t n = 0;
      for (int32_t dy = -1; dy <= 1; ++dy)
        for (int32_t dx = -1; dx <= 1; ++dx) {
          const int32_t xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
          acc += sd[static_cast<size_t>(yy) * static_cast<size_t>(W) + static_cast<size_t>(xx)];
          ++n;
        }
      sm[static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)] = std::max(-32.f, std::min(32.f, acc / static_cast<float>(n)));
    }
  return sm;
}

// ------------------------------------------------------------------------------------------------ the transport
namespace {
constexpr char kMagic[] = "P40U2:";   // P40B: + the field id
void put_bytes(std::string &o, const void *p, size_t n) {
  static const char *hx = "0123456789abcdef";
  const uint8_t *b = static_cast<const uint8_t *>(p);
  for (size_t i = 0; i < n; ++i) {
    o += hx[b[i] >> 4];
    o += hx[b[i] & 15];
  }
}
void put_f(std::string &o, float v) { put_bytes(o, &v, sizeof v); }
void put_c(std::string &o, const UiRgba &c) {
  put_f(o, c.r);
  put_f(o, c.g);
  put_f(o, c.b);
  put_f(o, c.a);
}
struct Reader {
  const std::string &s;
  size_t i;
  bool ok = true;
  bool bytes(void *p, size_t n) {
    uint8_t *b = static_cast<uint8_t *>(p);
    for (size_t k = 0; k < n; ++k) {
      if (i + 2 > s.size()) return ok = false;
      auto nib = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
      const int hi = nib(s[i]), lo = nib(s[i + 1]);
      if (hi < 0 || lo < 0) return ok = false;
      b[k] = static_cast<uint8_t>(hi << 4 | lo);
      i += 2;
    }
    return true;
  }
  float f() {
    float v = 0;
    bytes(&v, sizeof v);
    return v;
  }
  UiRgba c() { return UiRgba{f(), f(), f(), f()}; }
};
}  // namespace

std::string ui_encode(const UiScene &s) {
  std::string o = kMagic;
  for (float v : {s.cx0, s.cy0, s.cx1, s.cy1}) put_f(o, v);
  const uint32_t n = static_cast<uint32_t>(s.elems.size());
  put_bytes(o, &n, sizeof n);
  for (const UiElem &e : s.elems) {
    const uint32_t m = static_cast<uint32_t>(e.prims.size());
    put_bytes(o, &m, sizeof m);
    for (const UiPrim &p : e.prims) {
      put_bytes(o, &p.kind, 1);
      for (float v : {p.x0, p.y0, p.x1, p.y1, p.rx[0], p.rx[1], p.rx[2], p.rx[3], p.ry[0], p.ry[1], p.ry[2], p.ry[3], p.w, p.bulge})
        put_f(o, v);
      put_bytes(o, &p.field, sizeof p.field);
    }
    put_f(o, e.smooth);
    put_c(o, e.fill_top);
    put_c(o, e.fill_bot);
    put_f(o, e.gy0);
    put_f(o, e.gy1);
    put_c(o, e.outline);
    put_f(o, e.outline_w);
    put_f(o, e.bevel_w);
    put_f(o, e.bevel_light);
    put_f(o, e.bevel_dark);
    put_bytes(o, &e.inset, 1);
    put_c(o, e.shadow);
    put_f(o, e.shadow_dx);
    put_f(o, e.shadow_dy);
    put_f(o, e.shadow_blur);
    put_c(o, e.gloss);
    put_f(o, e.gloss_cx);
    put_f(o, e.gloss_cy);
    put_f(o, e.gloss_r);
  }
  return o;
}

bool ui_decode(const std::string &text, UiScene &s) {
  s = UiScene{};
  const size_t ml = sizeof kMagic - 1;
  if (text.size() < ml || text.compare(0, ml, kMagic) != 0) return false;
  Reader r{text, ml};
  s.cx0 = r.f();
  s.cy0 = r.f();
  s.cx1 = r.f();
  s.cy1 = r.f();
  uint32_t n = 0;
  r.bytes(&n, sizeof n);
  if (!r.ok || n > 4096) return false;
  s.elems.resize(n);
  for (UiElem &e : s.elems) {
    uint32_t m = 0;
    r.bytes(&m, sizeof m);
    if (!r.ok || m > 4096) return false;
    e.prims.resize(m);
    for (UiPrim &p : e.prims) {
      r.bytes(&p.kind, 1);
      p.x0 = r.f();
      p.y0 = r.f();
      p.x1 = r.f();
      p.y1 = r.f();
      for (float &v : p.rx) v = r.f();
      for (float &v : p.ry) v = r.f();
      p.w = r.f();
      p.bulge = r.f();
      r.bytes(&p.field, sizeof p.field);
    }
    e.smooth = r.f();
    e.fill_top = r.c();
    e.fill_bot = r.c();
    e.gy0 = r.f();
    e.gy1 = r.f();
    e.outline = r.c();
    e.outline_w = r.f();
    e.bevel_w = r.f();
    e.bevel_light = r.f();
    e.bevel_dark = r.f();
    r.bytes(&e.inset, 1);
    e.shadow = r.c();
    e.shadow_dx = r.f();
    e.shadow_dy = r.f();
    e.shadow_blur = r.f();
    e.gloss = r.c();
    e.gloss_cx = r.f();
    e.gloss_cy = r.f();
    e.gloss_r = r.f();
  }
  return r.ok && r.i == text.size();
}

// ------------------------------------------------------------------------------------------------ the themes
// P40B: the themes are data (ui_themes.inc; gfx_ui.h ui_themes_load). Each key names a UiTheme field.
namespace {
const char kBuiltinThemes[] =
#include "ui_themes.inc"
    ;
struct ThemeKey {
  const char *key;
  UiRgba UiTheme::*colour;
  float UiTheme::*number;
};
const ThemeKey kThemeKeys[] = {
    {"frame", &UiTheme::frame, nullptr},           {"frame_bevel", nullptr, &UiTheme::frame_bevel},
    {"frame_light", nullptr, &UiTheme::frame_light}, {"frame_dark", nullptr, &UiTheme::frame_dark},
    {"shadow", &UiTheme::shadow, nullptr},         {"shadow_d", nullptr, &UiTheme::shadow_d},
    {"shadow_blur", nullptr, &UiTheme::shadow_blur}, {"plate", &UiTheme::plate, nullptr},
    {"plate_edge", &UiTheme::plate_edge, nullptr}, {"label_top", &UiTheme::label_top, nullptr},
    {"label_bot", &UiTheme::label_bot, nullptr},   {"value_top", &UiTheme::value_top, nullptr},
    {"value_bot", &UiTheme::value_bot, nullptr},   {"pill_outline", &UiTheme::pill_outline, nullptr},
    {"pill_outline_w", nullptr, &UiTheme::pill_outline_w}, {"pill_bevel", nullptr, &UiTheme::pill_bevel},
    {"art_colours", nullptr, &UiTheme::art_colours},
    {"line", &UiTheme::line, nullptr},             {"line_w", nullptr, &UiTheme::line_w},
    {"box", &UiTheme::box, nullptr},               {"box_edge", &UiTheme::box_edge, nullptr},
    {"row", &UiTheme::row, nullptr},               {"grid", &UiTheme::grid, nullptr},
    {"ring", &UiTheme::ring, nullptr},             {"ring_outline", &UiTheme::ring_outline, nullptr},
    {"well", &UiTheme::well, nullptr},             {"ball_top", &UiTheme::ball_top, nullptr},
    {"ball_bot", &UiTheme::ball_bot, nullptr},     {"ball_hover_top", &UiTheme::ball_hover_top, nullptr},
    {"ball_hover_bot", &UiTheme::ball_hover_bot, nullptr}, {"gloss", &UiTheme::gloss, nullptr},
    {"check", &UiTheme::check, nullptr},
};
std::string trim(const std::string &v) {
  size_t a = v.find_first_not_of(" \t\r"), b = v.find_last_not_of(" \t\r");
  return a == std::string::npos ? std::string() : v.substr(a, b - a + 1);
}
// parse over `themes`; false with err on a malformed line
bool parse_themes(const std::string &text, std::vector<UiTheme> &themes, std::string &err) {
  UiTheme *cur = nullptr;
  size_t pos = 0;
  int line_no = 0;
  while (pos <= text.size()) {
    size_t e = text.find('\n', pos);
    if (e == std::string::npos) e = text.size();
    std::string ln = text.substr(pos, e - pos);
    pos = e + 1;
    ++line_no;
    const size_t hash = ln.find('#');
    if (hash != std::string::npos) ln.resize(hash);
    ln = trim(ln);
    if (ln.empty()) continue;
    if (ln.front() == '[') {
      if (ln.back() != ']' || ln.size() < 3) {
        err = "line " + std::to_string(line_no) + ": a section needs [name]";
        return false;
      }
      const std::string name = trim(ln.substr(1, ln.size() - 2));
      auto it = std::find_if(themes.begin(), themes.end(), [&](const UiTheme &t) { return t.name == name; });
      if (it == themes.end()) {
        UiTheme t = themes.empty() ? UiTheme() : themes.front();   // a new theme starts from the first (game)
        t.name = name;
        themes.push_back(t);
        cur = &themes.back();
      } else {
        cur = &*it;
      }
      continue;
    }
    const size_t eq = ln.find('=');
    if (eq == std::string::npos || !cur) {
      err = "line " + std::to_string(line_no) + ": expected key = value inside a [section]";
      return false;
    }
    const std::string key = trim(ln.substr(0, eq)), val = trim(ln.substr(eq + 1));
    if (key == "base") {
      auto it = std::find_if(themes.begin(), themes.end(), [&](const UiTheme &t) { return t.name == val; });
      if (it == themes.end()) {
        err = "line " + std::to_string(line_no) + ": no theme " + val;
        return false;
      }
      const std::string keep = cur->name;
      *cur = *it;
      cur->name = keep;
      continue;
    }
    std::vector<float> nums;
    {
      size_t i = 0;
      while (i < val.size()) {
        char *endp = nullptr;
        const float f = std::strtof(val.c_str() + i, &endp);
        if (endp == val.c_str() + i) {
          err = "line " + std::to_string(line_no) + ": not a number in " + val;
          return false;
        }
        nums.push_back(f);
        i = static_cast<size_t>(endp - val.c_str());
        while (i < val.size() && (val[i] == ' ' || val[i] == '\t')) ++i;
      }
    }
    const ThemeKey *k = nullptr;
    for (const ThemeKey &tk : kThemeKeys)
      if (key == tk.key) k = &tk;
    if (!k) {
      err = "line " + std::to_string(line_no) + ": unknown key " + key;
      return false;
    }
    if (k->colour) {
      if (nums.size() != 3 && nums.size() != 4) {
        err = "line " + std::to_string(line_no) + ": " + key + " needs r g b [alpha]";
        return false;
      }
      (cur->*(k->colour)) = UiRgba{nums[0], nums[1], nums[2], nums.size() == 4 ? nums[3] : 1.f};
    } else {
      if (nums.size() != 1) {
        err = "line " + std::to_string(line_no) + ": " + key + " needs one number";
        return false;
      }
      (cur->*(k->number)) = nums[0];
    }
  }
  return true;
}
std::vector<UiTheme> &themes() {
  static std::vector<UiTheme> t = [] {
    std::vector<UiTheme> v;
    std::string err;
    parse_themes(kBuiltinThemes, v, err);   // the built-in text is checked by test_p40_ui_pilot (a theme per name, no error)
    if (v.empty()) v.push_back(UiTheme{});
    return v;
  }();
  return t;
}
}  // namespace

const UiTheme &ui_theme(int32_t theme) {
  const std::vector<UiTheme> &t = themes();
  return theme >= 1 && static_cast<size_t>(theme) <= t.size() ? t[static_cast<size_t>(theme - 1)] : t.front();
}
bool ui_themes_load(const std::string &text, std::string &err) {
  std::vector<UiTheme> t = themes();
  if (!parse_themes(text, t, err)) return false;
  themes() = std::move(t);
  return true;
}
int32_t ui_theme_index(const std::string &name) {
  const std::vector<UiTheme> &t = themes();
  for (size_t i = 0; i < t.size(); ++i)
    if (t[i].name == name) return static_cast<int32_t>(i + 1);
  return 0;
}
std::vector<std::string> ui_theme_names() {
  std::vector<std::string> n;
  for (const UiTheme &t : themes()) n.push_back(t.name);
  return n;
}

// ------------------------------------------------------------------------------------------------ the Hole Stats scenes
namespace {
constexpr float kFar = 4000.f;  // a side that continues past the cell (the cells are clipped; only the cell's rows show)

UiPrim rrect(float x0, float y0, float x1, float y1, float r) {
  UiPrim p;
  p.kind = kUiRRect;
  p.x0 = x0;
  p.y0 = y0;
  p.x1 = x1;
  p.y1 = y1;
  for (int i = 0; i < 4; ++i) p.rx[i] = p.ry[i] = r;
  return p;
}
UiPrim ellipse(float x0, float y0, float x1, float y1) {
  UiPrim p = rrect(x0, y0, x1, y1, 0);
  p.kind = kUiEllipse;
  return p;
}
UiPrim seg(float x0, float y0, float x1, float y1, float w) {
  UiPrim p;
  p.kind = kUiSegment;
  p.x0 = x0;
  p.y0 = y0;
  p.x1 = x1;
  p.y1 = y1;
  p.w = w;
  return p;
}
UiElem flat(const UiRgba &c) {
  UiElem e;
  e.fill_top = e.fill_bot = c;
  return e;
}

// The panel: one shape over every cell (its top in the head, its sides everywhere, its bottom with the button bay in the foot);
// `foot` = the foot cell's offset from the sheet (screen y - 347) or none. Head rows: the big upper-left corner (rx 80, ry 46: the
// head's extents y 33 -> x 238 .. y 78 -> x 161), the top edge to x 570, a step down to the right part at y 70 to x 639.
UiElem panel(const UiTheme &t, bool top, bool foot, float fo) {
  UiElem e = flat(t.frame);
  e.bevel_w = t.frame_bevel;
  e.bevel_light = t.frame_light;
  e.bevel_dark = t.frame_dark;
  const float y1 = foot ? 368.f + fo : kFar;
  UiPrim left = rrect(161, top ? 33.f : -kFar, 570, y1, 4);
  if (top) {
    left.rx[0] = 80;
    left.ry[0] = 46;
  }
  left.rx[2] = left.ry[2] = 0;
  left.rx[3] = left.ry[3] = foot ? 5.f : 0.f;
  UiPrim right = rrect(500, top ? 70.f : -kFar, 639, y1, 5);
  right.rx[0] = right.ry[0] = 0;
  if (!top) right.rx[1] = right.ry[1] = 0;
  right.rx[3] = right.ry[3] = 0;
  right.rx[2] = right.ry[2] = foot ? 5.f : 0.f;
  e.prims = {left, right};
  if (foot) {   // the button bay below the bottom edge, rounded (radius 17) at both lower corners (extents y 386..401)
    UiPrim bay = rrect(554, 300 + fo, 639, 397 + fo, 17);
    bay.rx[0] = bay.ry[0] = bay.rx[1] = bay.ry[1] = 0;
    e.prims.push_back(bay);
    e.shadow = t.shadow;
    e.shadow_dx = 0;
    e.shadow_dy = t.shadow_d;
    e.shadow_blur = t.shadow_blur;
  }
  e.smooth = 4.f;   // the concave corners (x 570, y 70; x 554 on the bottom edge) rounded
  return e;
}
// the comment box: a sunken surround (light below / right, dark above / left) and the cream box (x 178..620 from y 214; its
// bottom at the foot's y 349)
void comment_box(const UiTheme &t, UiScene &s, bool top, bool foot, float fo) {
  const float y0 = top ? 209.f : -kFar, y1 = foot ? 354.f + fo : kFar;
  UiElem edge = flat(t.box_edge);
  edge.prims = {rrect(174, y0, 625, y1, 3)};
  edge.bevel_w = 4.f;
  edge.bevel_light = 0.35f;
  edge.bevel_dark = 0.55f;
  edge.inset = 1;
  s.elems.push_back(edge);
  UiElem box = flat(t.box);
  box.prims = {rrect(178, top ? 214.f : -kFar, 620, foot ? 349.f + fo : kFar, 1.5f)};
  box.bevel_w = 2.f;
  box.bevel_dark = 0.12f;
  box.inset = 1;
  s.elems.push_back(box);
}
UiElem pill(const UiTheme &t, float x0, float y0, float x1, float y1, bool value, bool oval = false) {
  UiElem e;
  e.fill_top = value ? t.value_top : t.label_top;
  e.fill_bot = value ? t.value_bot : t.label_bot;
  e.gy0 = y0 + 1.5f;
  e.gy1 = y0 + 6.f;
  e.prims = {oval ? ellipse(x0, y0, x1, y1) : rrect(x0, y0, x1, y1, (y1 - y0) * 0.5f)};
  e.outline = t.pill_outline;
  e.outline_w = t.pill_outline_w;
  e.bevel_w = t.pill_bevel;
  e.bevel_light = 0.25f;
  e.bevel_dark = 0.10f;
  e.shadow = UiRgba{t.shadow.r, t.shadow.g, t.shadow.b, 0.30f};
  e.shadow_dy = 1.5f;
  e.shadow_blur = 2.f;
  return e;
}

UiScene head_scene(const UiTheme &t) {
  UiScene s;
  s.cx0 = 0;
  s.cy0 = 0;
  s.cx1 = 800;
  s.cy1 = 220;   // the head cell's rows (gfx_routing.cpp's HoleSTAT cut: 0, 0, 800 x 220)
  s.elems.push_back(panel(t, true, false, 0));
  // the title plate: cream, a 2-pixel outline (columns 229-230 / 557-558, rows 71-72 at the bottom), its top edge raised 9 pixels
  // at the middle (outline rows 46-47 at x 232 / 554, 37-38 at x 400)
  UiElem plate = flat(t.plate);
  UiPrim pp = rrect(229, 46, 559, 73, 3);
  pp.bulge = 9;
  plate.prims = {pp};
  plate.outline = t.plate_edge;
  plate.outline_w = 2.f;
  plate.bevel_w = 4.f;
  plate.bevel_dark = 0.20f;
  plate.inset = 1;
  s.elems.push_back(plate);
  comment_box(t, s, true, false, 0);
  // the connectors, under the pills
  UiElem ln = flat(t.line);
  const float lw = t.line_w;
  const float rows[4] = {89, 110, 131, 152};
  ln.prims.push_back(seg(167, 89, 167, 209, lw));   // the left bracket and its branches to the six left pills
  for (float y : {89.f, 110.f, 131.f, 152.f, 184.f, 209.f}) ln.prims.push_back(seg(167, y, 184, y, lw));
  for (float y : rows) ln.prims.push_back(seg(282, y, 300, y, lw));   // label -> value, left column
  ln.prims.push_back(seg(489, 89, 507, 89, lw));
  ln.prims.push_back(seg(539, 110, 557, 110, lw));
  ln.prims.push_back(seg(489, 131, 507, 131, lw));
  ln.prims.push_back(seg(539, 152, 557, 152, lw));
  auto bracket = [&](float ya, float yb, float x) {   // a "(" joining two pills' left ends (the art's: x 432 at the ends, 420 deep)
    const float ym = (ya + yb) * 0.5f, r = (yb - ya) * 0.5f;
    float px = x, py = ya;
    for (int k = 1; k <= 12; ++k) {
      const float a = -1.5707963f + 3.1415926f * k / 12.f;
      const float nx = x - 11.5f * std::cos(a), ny = ym + r * std::sin(a);
      ln.prims.push_back(seg(px, py, nx, ny, lw));
      px = nx;
      py = ny;
    }
  };
  bracket(89, 110, 432);
  bracket(131, 152, 432);
  bracket(174, 194, 432);
  ln.prims.push_back(seg(408, 184, 421, 184, lw));   // the shots pill -> the score ovals' bracket
  for (int k = 0; k < 6; ++k) {
    const float cx = 444.5f + 34.f * k;
    if (k < 5) ln.prims.push_back(seg(cx + 12, 174, cx + 22, 174, lw));
    ln.prims.push_back(seg(cx, 180, cx, 188, lw));
  }
  s.elems.push_back(ln);
  // the label pills (lavender) and the value boxes (cream): the art's fills (log/roadmap/P40/holestat_geometry.txt) with their
  // 2-pixel outline around them
  const float L[][4] = {{182, 80, 282, 98},   {432, 80, 489, 98},   {182, 101, 282, 119}, {432, 101, 539, 119},
                        {182, 122, 282, 140}, {432, 122, 489, 140}, {182, 143, 282, 161}, {432, 143, 539, 161},
                        {182, 174, 408, 194}, {182, 199, 286, 219}};
  for (const auto &b : L) s.elems.push_back(pill(t, b[0], b[1], b[2], b[3], false));
  const float V[][4] = {{298, 80, 413, 98},   {505, 80, 566, 98},   {298, 101, 413, 119}, {555, 101, 626, 119},
                        {298, 122, 413, 140}, {505, 122, 566, 140}, {298, 143, 413, 161}, {555, 143, 626, 161}};
  for (const auto &b : V) s.elems.push_back(pill(t, b[0], b[1], b[2], b[3], true));
  for (int k = 0; k < 6; ++k) {   // the score ovals: lavender (x 434 + 34 k, y 168..180 filled), cream below (y 188..200)
    const float x = 432.f + 34.f * k;
    s.elems.push_back(pill(t, x, 165, x + 25, 183, false, true));
    s.elems.push_back(pill(t, x + 1, 185, x + 24, 203, true, true));
  }
  return s;
}
UiScene row_scene(const UiTheme &t, int32_t y, int32_t h) {
  UiScene s;
  s.cx0 = 0;
  s.cy0 = static_cast<float>(y);
  s.cx1 = 800;
  s.cy1 = static_cast<float>(y + h);
  s.elems.push_back(panel(t, false, false, 0));
  comment_box(t, s, false, false, 0);
  return s;
}
UiScene foot_scene(const UiTheme &t, int32_t y, int32_t h) {
  UiScene s;
  s.cx0 = 0;
  s.cy0 = static_cast<float>(y);
  s.cx1 = 800;
  s.cy1 = static_cast<float>(y + h);
  const float fo = static_cast<float>(y) - 347.f;   // the foot cell is the sheet's rows 347..402 (gfx_routing.cpp's cut)
  s.elems.push_back(panel(t, false, true, fo));
  comment_box(t, s, false, true, fo);
  return s;
}
// the close button over its 44 x 42 cell at (x, y): a raised ring, the dark well, the ball (gloss at its upper left) and the check
UiScene button_scene(const UiTheme &t, int32_t x, int32_t y, bool hover) {
  UiScene s;
  s.cx0 = static_cast<float>(x);
  s.cy0 = static_cast<float>(y);
  s.cx1 = static_cast<float>(x + 44);
  s.cy1 = static_cast<float>(y + 42);
  const float cx = x + 22.f, cy = y + 21.f;
  UiElem ring = flat(t.ring);
  ring.prims = {ellipse(cx - 20.5f, cy - 20.5f, cx + 20.5f, cy + 20.5f)};
  ring.outline = t.ring_outline;
  ring.outline_w = 1.5f;
  ring.bevel_w = 3.f;
  ring.bevel_light = 0.4f;
  ring.bevel_dark = 0.4f;
  s.elems.push_back(ring);
  UiElem well = flat(t.well);
  well.prims = {ellipse(cx - 16.5f, cy - 16.5f, cx + 16.5f, cy + 16.5f)};
  s.elems.push_back(well);
  UiElem ball;
  ball.fill_top = hover ? t.ball_hover_top : t.ball_top;
  ball.fill_bot = hover ? t.ball_hover_bot : t.ball_bot;
  ball.gy0 = cy - 15;
  ball.gy1 = cy + 15;
  ball.prims = {ellipse(cx - 15, cy - 15, cx + 15, cy + 15)};
  ball.bevel_w = 5.f;
  ball.bevel_light = 0.15f;
  ball.bevel_dark = 0.35f;
  ball.gloss = t.gloss;
  ball.gloss_cx = cx - 7;
  ball.gloss_cy = cy - 8;
  ball.gloss_r = 7;
  s.elems.push_back(ball);
  UiElem chk = flat(t.check);
  chk.prims = {seg(cx - 8, cy + 0.5f, cx - 3, cy + 7, 4.5f), seg(cx - 3, cy + 7, cx + 8, cy - 9, 4.5f)};
  s.elems.push_back(chk);
  return s;
}

// The scene into the root at scale 1 (each pixel the scene over the root's word) and kept as a UI run.
void draw_scene(Gfx &g, Ctx &ctx, const UiScene &s) {
  g.ensure_root(ctx);
  Surface16 &r = g.root;
  float fx0, fy0, fx1, fy1;
  ui_bounds(s, fx0, fy0, fx1, fy1);
  const int32_t x0 = std::max(0, static_cast<int32_t>(std::floor(fx0))), y0 = std::max(0, static_cast<int32_t>(std::floor(fy0)));
  const int32_t x1 = std::min(r.w, static_cast<int32_t>(std::ceil(fx1))), y1 = std::min(r.h, static_cast<int32_t>(std::ceil(fy1)));
  if (x1 <= x0 || y1 <= y0) return;
  gfxb::TextRunScope ts;
  gfxb::text_run_begin(g, r.px.data(), r.w, r.h, ts, std::max(0, y0 - 4), std::min(r.h, y1 + 4));
  std::vector<float> prem;
  ui_render(s, 1.0, 1.0, 0.0, 0.0, x0, y0, x1 - x0, y1 - y0, prem);
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) {
      const float *p = prem.data() + (static_cast<size_t>(y - y0) * static_cast<size_t>(x1 - x0) + static_cast<size_t>(x - x0)) * 4u;
      if (p[3] <= 0.f) continue;
      uint16_t &o = r.px[static_cast<size_t>(y) * static_cast<size_t>(r.w) + static_cast<size_t>(x)];
      o = ui_over555(p, o);
      hud_mark(&o);
    }
  // (no Gfx::note_root_write: the art draws this replaces, Surface_stretchBlit and Image_draw, make none; P40B: a full-screen
  // cell noted as a root write hid the world beside it)
  GameFont f;
  f.face = kUiRunFace;
  f.size = 3;   // text_run_end's box margin: max(3, size / 3) = 3 pixels around the changed pixels
  const int32_t clip[4] = {static_cast<int32_t>(s.cx0), static_cast<int32_t>(s.cy0), static_cast<int32_t>(s.cx1),
                           static_cast<int32_t>(s.cy1)};
  gfxb::text_run_end(g, ctx, ts, r.px.data(), r.w, r.h, 0, 0, clip, 0, ui_encode(s), 0, 0, 0, 0, &f);
}

int32_t theme_of(Ctx &ctx) { return gfx_ui_pilot(ctx); }

void b_hs_head(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff) return;
  UiScene s = head_scene(ui_theme(th));
  const float dy = static_cast<float>(static_cast<int32_t>(a[0]));   // the head's y (0: drawn at the cut's own place)
  if (dy != 0) {
    s.cy0 += dy;
    s.cy1 += dy;
    for (UiElem &e : s.elems) {
      for (UiPrim &p : e.prims) {
        p.y0 += dy;
        p.y1 += dy;
      }
      e.gy0 += dy;
      e.gy1 += dy;
      e.gloss_cy += dy;
    }
  }
  draw_scene(g, ctx, s);
}
void b_hs_row(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff || SG_NEGCTL(g_negctl_gfx_ui) == 2) return;
  draw_scene(g, ctx, row_scene(ui_theme(th), static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1])));
}
void b_hs_foot(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff) return;
  draw_scene(g, ctx, foot_scene(ui_theme(th), static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1])));
}
void b_hs_button(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff) return;
  draw_scene(g, ctx, button_scene(ui_theme(th), static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), a[2] != 0));
}

}  // namespace
// P40B: the scene drawing and the button for the cell swap (gfx_ui.cpp)
void ui_draw_scene(Gfx &g, Ctx &ctx, const UiScene &s) { draw_scene(g, ctx, s); }
UiScene ui_button_scene(const UiTheme &t, int32_t x, int32_t y, bool hover) { return button_scene(t, x, y, hover); }
namespace {
const GfxBodyDef kBodies[] = {
    {"p40.hs_head", b_hs_head},
    {"p40.hs_row", b_hs_row},
    {"p40.hs_foot", b_hs_foot},
    {"p40.hs_button", b_hs_button},
};
}  // namespace

// a scene by name for the tests (holestats.cpp's ui test entries): 0 head, 1 row at y, 2 foot at y, 3 button at (x, y) hover h
bool ui_test_scene(int32_t which, int32_t theme, int32_t x, int32_t y, int32_t h, UiScene &out) {
  const UiTheme &t = ui_theme(theme);
  switch (which) {
    case 0: out = head_scene(t); return true;
    case 1: out = row_scene(t, y, 16); return true;
    case 2: out = foot_scene(t, y, 55); return true;
    case 3: out = button_scene(t, x, y, h != 0); return true;
    default: return false;
  }
}

extern const GfxFamily kGfxFamily_ui = {"ui", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};


// ================================================================================================ P40B: the cell engine
// P40B (log/roadmap/P40/P40_notes.md; portsim/sim/gfx_ui.h): the report screens drawn procedurally. With a UI theme on
// (gfx_set_ui_pilot), gfx_drop hands two kinds of drop here before their bodies (gfx.cpp, ui_swap):
//   Surface_stretchBlit(colour, mask, root, x, y, 0) of a report art cell pair (the SpriteRef arrays loadInfoScreenArt fills:
//     coursereport, FINANCEreport, SGA, PlayComt, memberRoster, lowscore, histograph, hire, tournament result) is drawn as the
//     cell's scene (ui_cell_scene, built once per cell and theme from the user's art) at (x, y), then the cell's pixels the scene
//     does not draw (the SGA logo, coloured marks) from the art as the original draws them;
//   Image_draw of an OkStates button cell (the reports' OK / close button) on the root is drawn as the theme's button.
// The screens make the same calls with the theme on or off; off, nothing here runs. Drawing only.

namespace {
enum Cls : uint8_t { kNone = 0, kFrame = 1, kDark = 2, kCream = 3, kWhite = 4, kPill = 5, kOther = 6, kRim = 7 };

Cls classify(int r, int g, int b) {
  const int mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
  if ((r + g + b) / 3 < 95) return kDark;
  if (r >= 235 && g >= 235 && b >= 235 && std::abs(r - b) < 12) return kWhite;
  if (r >= 225 && g >= 212 && b >= 185 && r - b >= 10 && mx - mn < 80) return kCream;
  if (r >= 195 && g >= 175 && b >= 120 && r - b >= 25 && r - b <= 95 && r >= g) return kCream;   // the tan rows and boxes
  if (b >= 222 && r >= 160 && r <= 215 && b - r >= 30 && b - r <= 80) return kPill;
  if (mx - mn > 70) return kOther;   // red / green / yellow marks, pictures
  if ((r + g + b) / 3 < 140 && b - r >= 20) return kRim;   // the mid-dark lavender of outline-only pills and grid lines
  return kFrame;
}

struct Comp {
  int32_t x0, y0, x1, y1, n;
  std::vector<int32_t> px;   // indexes into the cell
};
// the 4-connected components of the pixels whose class is `c`
std::vector<Comp> components(const std::vector<uint8_t> &cls, int32_t w, int32_t h, uint8_t c) {
  std::vector<Comp> out;
  std::vector<uint8_t> seen(cls.size(), 0);
  std::vector<int32_t> stack;
  for (int32_t i = 0; i < w * h; ++i) {
    if (cls[static_cast<size_t>(i)] != c || seen[static_cast<size_t>(i)]) continue;
    Comp k{w, h, -1, -1, 0, {}};
    stack.assign(1, i);
    seen[static_cast<size_t>(i)] = 1;
    while (!stack.empty()) {
      const int32_t j = stack.back();
      stack.pop_back();
      const int32_t x = j % w, y = j / w;
      k.px.push_back(j);
      k.x0 = std::min(k.x0, x);
      k.y0 = std::min(k.y0, y);
      k.x1 = std::max(k.x1, x + 1);
      k.y1 = std::max(k.y1, y + 1);
      const int32_t nb[4] = {x > 0 ? j - 1 : -1, x + 1 < w ? j + 1 : -1, y > 0 ? j - w : -1, y + 1 < h ? j + w : -1};
      for (int32_t q : nb)
        if (q >= 0 && cls[static_cast<size_t>(q)] == c && !seen[static_cast<size_t>(q)]) {
          seen[static_cast<size_t>(q)] = 1;
          stack.push_back(q);
        }
    }
    k.n = static_cast<int32_t>(k.px.size());
    out.push_back(std::move(k));
  }
  return out;
}

// a field primitive over a mask of the cell (frame coordinates relative to the cell); a mask that reaches the cell's edge is
// continued past it (the cell is a band of a taller panel: its cut edges are no edges)
UiPrim field_prim(const std::vector<uint8_t> &mask, int32_t w, int32_t h) {
  constexpr int32_t pad = 6;
  const int32_t W = w + 2 * pad, H = h + 2 * pad;
  std::vector<uint8_t> ext(static_cast<size_t>(W) * static_cast<size_t>(H), 0);
  for (int32_t y = 0; y < H; ++y)
    for (int32_t x = 0; x < W; ++x) {
      const int32_t sx = std::min(std::max(x - pad, 0), w - 1), sy = std::min(std::max(y - pad, 0), h - 1);
      ext[static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)] =
          mask[static_cast<size_t>(sy) * static_cast<size_t>(w) + static_cast<size_t>(sx)];
    }
  UiPrim p;
  p.kind = kUiField;
  p.x0 = static_cast<float>(-pad - 4);
  p.y0 = static_cast<float>(-pad - 4);
  p.x1 = static_cast<float>(w + pad + 4);
  p.y1 = static_cast<float>(h + pad + 4);
  p.field = ui_field_put(W + 8, H + 8, ui_field_from_mask(ext, W, H, 4));
  return p;
}

// whether a pixel centre lies in the rounded rectangle (x0, y0)-(x1, y1) of corner radius r
bool in_rrect(float px, float py, float x0, float y0, float x1, float y1, float r) {
  if (px < x0 || px > x1 || py < y0 || py > y1) return false;
  const float cx = std::min(std::max(px, x0 + r), x1 - r), cy = std::min(std::max(py, y0 + r), y1 - r);
  return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
}
}  // namespace

bool ui_cell_scene(const uint8_t *colour, const uint16_t *pal, const uint8_t *mask, int32_t w, int32_t h, const UiTheme &t,
                   UiScene &out, std::vector<uint8_t> &keep) {
  out = UiScene{};
  out.cx0 = 0;
  out.cy0 = 0;
  out.cx1 = static_cast<float>(w);
  out.cy1 = static_cast<float>(h);
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  keep.assign(n, 0);
  std::vector<uint8_t> cls(n, kNone);
  std::vector<uint32_t> rgbv(n, 0);   // the pixel's colour (r | g << 8 | b << 16), for the art's median colours
  bool any = false;
  for (size_t i = 0; i < n; ++i) {
    const uint8_t c = colour[i], k = mask[i];
    if (c >= 0xff || k >= 0xff || k >= 0x80) continue;   // transparent, or less than half covering (a soft edge)
    const uint16_t v = pal[c];
    const int r = ((v >> 10) & 31) << 3, g = ((v >> 5) & 31) << 3, b = (v & 31) << 3;
    cls[i] = classify(r, g, b);
    rgbv[i] = static_cast<uint32_t>(r) | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b) << 16;
    any = true;
  }
  if (!any) return false;
  // the outside: transparent and dark pixels reached from the cell's border (an outer dark rim or shadow is not panel)
  std::vector<uint8_t> outside(n, 0);
  {
    std::vector<int32_t> st;
    auto push = [&](int32_t i) {
      if (outside[static_cast<size_t>(i)]) return;
      const uint8_t c = cls[static_cast<size_t>(i)];
      if (c != kNone && c != kDark) return;
      outside[static_cast<size_t>(i)] = 1;
      st.push_back(i);
    };
    for (int32_t x = 0; x < w; ++x) {
      if (cls[static_cast<size_t>(x)] == kNone) push(x);
      if (cls[static_cast<size_t>((h - 1) * w + x)] == kNone) push((h - 1) * w + x);
    }
    for (int32_t y = 0; y < h; ++y) {
      if (cls[static_cast<size_t>(y * w)] == kNone) push(y * w);
      if (cls[static_cast<size_t>(y * w + w - 1)] == kNone) push(y * w + w - 1);
    }
    while (!st.empty()) {
      const int32_t j = st.back();
      st.pop_back();
      const int32_t x = j % w, y = j / w;
      if (x > 0) push(j - 1);
      if (x + 1 < w) push(j + 1);
      if (y > 0) push(j - w);
      if (y + 1 < h) push(j + w);
    }
  }
  // pictures and coloured marks: every opaque pixel in the box (grown by 2) of each 8-connected cluster of picture-colour pixels
  // stays the art's (the SGA badge's white letters and dark ring sit among its coloured parts; a legend dot keeps its rim)
  {
    std::vector<uint8_t> seen(n, 0);
    std::vector<int32_t> st;
    for (int32_t i = 0; i < w * h; ++i) {
      if (cls[static_cast<size_t>(i)] != kOther || seen[static_cast<size_t>(i)]) continue;
      int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
      st.assign(1, i);
      seen[static_cast<size_t>(i)] = 1;
      while (!st.empty()) {
        const int32_t j = st.back();
        st.pop_back();
        const int32_t x = j % w, y = j / w;
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
        for (int32_t dy = -1; dy <= 1; ++dy)
          for (int32_t dx = -1; dx <= 1; ++dx) {
            const int32_t xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
            if (cls[q] == kOther && !seen[q]) {
              seen[q] = 1;
              st.push_back(static_cast<int32_t>(q));
            }
          }
      }
      for (int32_t y = std::max(0, y0 - 2); y <= std::min(h - 1, y1 + 2); ++y)
        for (int32_t x = std::max(0, x0 - 2); x <= std::min(w - 1, x1 + 2); ++x) {
          const size_t q = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
          if (cls[q] != kNone) keep[q] = 1;
        }
      // then the picture's dark and white parts joined to it (a badge's ring and letters), up to 12 pixels past that box
      std::vector<int32_t> grow;
      for (int32_t y = std::max(0, y0 - 2); y <= std::min(h - 1, y1 + 2); ++y)
        for (int32_t x = std::max(0, x0 - 2); x <= std::min(w - 1, x1 + 2); ++x)
          if (keep[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]) grow.push_back(y * w + x);
      while (!grow.empty()) {
        const int32_t j = grow.back();
        grow.pop_back();
        const int32_t x = j % w, y = j / w;
        for (int32_t dy = -1; dy <= 1; ++dy)
          for (int32_t dx = -1; dx <= 1; ++dx) {
            const int32_t xx = x + dx, yy = y + dy;
            if (xx < std::max(0, x0 - 14) || yy < std::max(0, y0 - 14) || xx > std::min(w - 1, x1 + 14) || yy > std::min(h - 1, y1 + 14))
              continue;
            const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
            if (keep[q] || (cls[q] != kDark && cls[q] != kWhite && cls[q] != kOther)) continue;
            keep[q] = 1;
            grow.push_back(static_cast<int32_t>(q));
          }
      }
    }
  }
  // large dark regions inside the panel (a marbled ground: no line is that big) stay the art's too
  for (const Comp &k : components(cls, w, h, kDark)) {
    if (k.n < 1500) continue;
    bool in = true;
    for (int32_t j : k.px)
      if (outside[static_cast<size_t>(j)]) {
        in = false;
        break;
      }
    if (!in) continue;
    for (int32_t j : k.px) keep[static_cast<size_t>(j)] = 1;
  }
  // the kept pixels are panel for the panel's shape and no part of a box or a line
  for (size_t i = 0; i < n; ++i)
    if (keep[i] && cls[i] != kNone) cls[i] = kFrame;
  std::vector<uint8_t> panel(n, 0);
  bool shadow = false;
  for (size_t i = 0; i < n; ++i) {
    panel[i] = cls[i] != kNone && !outside[i];
    if (cls[i] == kDark && outside[i]) shadow = true;
  }
  // the median colour of the pixels of class c among `sel` (per channel), or `dflt` without the art colours or without pixels
  const bool art = t.art_colours > 0.5f;
  auto median = [&](const std::vector<int32_t> &idx, const UiRgba &dflt) {
    if (!art || idx.empty()) return dflt;
    UiRgba o{0, 0, 0, 1.f};
    std::vector<int32_t> v(idx.size());
    for (int ch = 0; ch < 3; ++ch) {
      for (size_t k = 0; k < idx.size(); ++k) v[k] = static_cast<int32_t>((rgbv[static_cast<size_t>(idx[k])] >> (8 * ch)) & 0xff);
      std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2), v.end());
      (ch == 0 ? o.r : ch == 1 ? o.g : o.b) = static_cast<float>(v[v.size() / 2]);
    }
    return o;
  };
  std::vector<int32_t> frame_px, line_px;
  for (size_t i = 0; i < n; ++i)
    if (cls[i] == kFrame && !outside[i] && !keep[i]) frame_px.push_back(static_cast<int32_t>(i));   // (kept art is no panel colour)
  UiElem fr;
  fr.fill_top = fr.fill_bot = median(frame_px, t.frame);
  fr.bevel_w = t.frame_bevel;
  fr.bevel_light = t.frame_light;
  fr.bevel_dark = t.frame_dark;
  fr.prims = {field_prim(panel, w, h)};
  if (shadow) {
    fr.shadow = t.shadow;
    fr.shadow_dy = t.shadow_d;
    fr.shadow_blur = t.shadow_blur;
  }
  out.elems.push_back(fr);
  // the boxes: each component of the pill / cream / white classes, with the dark ring around it as its outline
  std::vector<uint8_t> ring_used(n, 0);
  std::vector<UiElem> boxes;
  for (uint8_t c : {kPill, kCream, kWhite}) {
    for (const Comp &k : components(cls, w, h, c)) {
      if (k.n < 6) continue;
      std::vector<uint8_t> in(n, 0);
      for (int32_t j : k.px) in[static_cast<size_t>(j)] = 1;
      // the ring: up to two pixels around the component; its share of dark pixels decides the outline's width
      int32_t width = 0;
      std::vector<uint8_t> grown = in;
      for (int32_t step = 1; step <= 2; ++step) {
        std::vector<uint8_t> next = grown;
        int32_t ring = 0, dark = 0;
        for (int32_t y = std::max(0, k.y0 - step); y < std::min(h, k.y1 + step); ++y)
          for (int32_t x = std::max(0, k.x0 - step); x < std::min(w, k.x1 + step); ++x) {
            const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            if (grown[i]) continue;
            const bool nb = (x > 0 && grown[i - 1]) || (x + 1 < w && grown[i + 1]) || (y > 0 && grown[i - static_cast<size_t>(w)]) ||
                            (y + 1 < h && grown[i + static_cast<size_t>(w)]);
            if (!nb) continue;
            next[i] = 1;
            ++ring;
            if (cls[i] == kDark) ++dark;
          }
        if (ring == 0 || dark * 2 < ring) break;
        width = step;
        for (size_t i = 0; i < n; ++i)
          if (next[i] && !grown[i] && cls[i] == kDark) ring_used[i] = 1;
        grown = std::move(next);
      }
      // the fit: a rounded rectangle over the box (radius from the corners' missing area), or an ellipse; a field when neither
      // covers the component (IoU < 0.9)
      const bool top = k.y0 == 0, bottom = k.y1 == h, left = k.x0 == 0, right = k.x1 == w;
      const float bw = static_cast<float>(k.x1 - k.x0), bh = static_cast<float>(k.y1 - k.y0);
      const float miss = bw * bh - static_cast<float>(k.n);
      float r = std::sqrt(std::max(0.f, miss) / (4.f * 0.2146f));
      r = std::min(r, std::min(bw, bh) * 0.5f);
      if (top || bottom || left || right) r = std::min(r, 2.f);
      const float x0 = static_cast<float>(k.x0), y0 = static_cast<float>(k.y0), x1 = static_cast<float>(k.x1), y1 = static_cast<float>(k.y1);
      int32_t inter = 0, uni = 0;
      for (int32_t y = k.y0; y < k.y1; ++y)
        for (int32_t x = k.x0; x < k.x1; ++x) {
          const bool a = in[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] != 0;
          const bool b = in_rrect(x + 0.5f, y + 0.5f, x0, y0, x1, y1, r);
          inter += a && b;
          uni += a || b;
        }
      const float wd = static_cast<float>(width);
      UiPrim pr;
      if (uni > 0 && inter * 10 >= uni * 9) {
        pr.kind = kUiRRect;
        pr.x0 = x0 - wd - (left ? 40.f : 0.f);
        pr.y0 = y0 - wd - (top ? 40.f : 0.f);
        pr.x1 = x1 + wd + (right ? 40.f : 0.f);
        pr.y1 = y1 + wd + (bottom ? 40.f : 0.f);
        for (int q = 0; q < 4; ++q) pr.rx[q] = pr.ry[q] = r + wd;
      } else {
        std::vector<uint8_t> m2 = grown;   // the component and its outline ring
        pr = field_prim(m2, w, h);
      }
      UiElem e;
      if (c == kPill) {
        e.fill_top = t.label_top;
        e.fill_bot = t.label_bot;
      } else if (c == kCream) {
        e.fill_top = t.value_top;
        e.fill_bot = t.value_bot;
      } else {
        e.fill_top = e.fill_bot = t.row;
      }
      if (art) {   // the box's own colour; the theme's top shade kept as the same step darker
        const UiRgba m = median(k.px, e.fill_bot);
        const float dr = e.fill_bot.r - e.fill_top.r, dg = e.fill_bot.g - e.fill_top.g, db = e.fill_bot.b - e.fill_top.b;
        e.fill_bot = m;
        e.fill_top = UiRgba{m.r - dr, m.g - dg, m.b - db, 1.f};
      }
      e.gy0 = y0 + 0.5f;
      e.gy1 = y0 + 4.5f;
      e.prims = {pr};
      if (width > 0) {
        e.outline = t.pill_outline;
        e.outline_w = wd * 0.8f;
      }
      if (c == kPill && bh >= 8.f) {   // the raised pills; boxes and table cells stay flat as the art paints them
        e.bevel_w = t.pill_bevel;
        e.bevel_light = 0.25f;
        e.bevel_dark = 0.10f;
      }
      boxes.push_back(std::move(e));
    }
  }
  // the rims (outline-only pills, grid lines): one field in their median colour
  {
    std::vector<uint8_t> rims(n, 0);
    std::vector<int32_t> rim_px;
    for (size_t i = 0; i < n; ++i)
      if (cls[i] == kRim && !outside[i]) {
        rims[i] = 1;
        rim_px.push_back(static_cast<int32_t>(i));
      }
    if (rim_px.size() >= 8) {
      UiElem rm;
      rm.fill_top = rm.fill_bot = median(rim_px, UiRgba{t.frame.r * 0.75f, t.frame.g * 0.75f, t.frame.b * 0.8f, 1.f});
      rm.prims = {field_prim(rims, w, h)};
      out.elems.push_back(rm);
    }
  }
  // the dark lines inside the panel that are no box's outline: connectors, separators
  {
    std::vector<uint8_t> lines(n, 0);
    bool any_line = false;
    for (size_t i = 0; i < n; ++i)
      if (cls[i] == kDark && !outside[i] && !ring_used[i]) {
        lines[i] = 1;
        line_px.push_back(static_cast<int32_t>(i));
        any_line = true;
      }
    if (any_line) {
      UiElem ln;
      ln.fill_top = ln.fill_bot = median(line_px, t.line);
      ln.prims = {field_prim(lines, w, h)};
      out.elems.push_back(ln);
    }
  }
  for (UiElem &e : boxes) out.elems.push_back(std::move(e));
  return true;
}

namespace {
// the report art the swap draws: the SpriteRef arrays loadInfoScreenArt fills (gfx_routing.cpp) and how many cells each holds
bool report_cell(Ctx &ctx, uint32_t sref) {
  const uint32_t bases[] = {state_va(ctx, &GameState::g_spriteRef_821308), state_va(ctx, &GameState::g_infoSprites8210c8),
                            state_va(ctx, &GameState::g_spriteRef_821c60), state_va(ctx, &GameState::g_spriteRef_8223c0),
                            state_va(ctx, &GameState::g_spriteRef_821f48), state_va(ctx, &GameState::g_spriteRef_8222b8),
                            state_va(ctx, &GameState::g_spriteRef_821bf8), state_va(ctx, &GameState::g_infoSprites821070),
                            state_va(ctx, &GameState::g_spriteRef_821cb8)};
  for (uint32_t b : bases)
    if (sref >= b && sref < b + 12u * 0x2cu && (sref - b) % 0x2cu == 0) return true;
  return false;
}
struct CellScene {
  UiScene scene;
  std::vector<uint8_t> keep;
  bool ok = false;
};
// the scene of a cell pair for a theme, built once (the art does not change while the backend lives)
const CellScene &cell_scene(Gfx &g, uint32_t cs, uint32_t ms, const Sprite8 &s, const Sprite8 &m, int32_t theme) {
  static std::map<std::tuple<const Gfx *, uint32_t, uint32_t, int32_t, size_t>, CellScene> cache;
  const auto key = std::make_tuple(static_cast<const Gfx *>(&g), cs, ms, theme, s.px.size());
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  CellScene c;
  std::vector<uint8_t> mk(s.px.size(), 0xff);
  for (size_t i = 0; i < mk.size() && i < m.px.size(); ++i) mk[i] = m.px[i];
  c.ok = ui_cell_scene(s.px.data(), s.pal.data(), mk.data(), s.w, s.h, ui_theme(theme), c.scene, c.keep);
  if (cache.size() > 256) cache.clear();
  return cache[key] = std::move(c);
}
UiScene moved(const UiScene &s, float dx, float dy) {
  UiScene o = s;
  o.cx0 += dx;
  o.cx1 += dx;
  o.cy0 += dy;
  o.cy1 += dy;
  for (UiElem &e : o.elems) {
    for (UiPrim &p : e.prims) {
      p.x0 += dx;
      p.x1 += dx;
      p.y0 += dy;
      p.y1 += dy;
    }
    e.gy0 += dy;
    e.gy1 += dy;
    e.gloss_cx += dx;
    e.gloss_cy += dy;
  }
  return o;
}
}  // namespace

namespace {
// P40B batch 2: the 9-slice frame drawers (drawPanelS2: the popups; drawFrameR3: the message windows and tips; drawFrameS2: the
// course-style frames). Their eight pieces are laid out at the call's (16-rounded) size exactly as the bodies lay them out
// (frame_border: corners, the edges every 16 from pos + 16), the interior filled as the body fills it, and the assembled picture
// goes through ui_cell_scene. drawFrameR3's shaded interior (blitPanel, the half-bright tiles) becomes a half-dark rounded
// rectangle under the frame.
void round16b(int32_t &pos, int32_t &size) {
  const int32_t low = size & 0xf;
  if (low != 0) {
    const int32_t r = 0xf - low;
    size += r;
    pos -= r / 2;
  }
}
bool frame_drawer(Gfx &g, Ctx &ctx, const char *name, const uint32_t *a, int32_t theme) {
  int kind = -1;   // 0 drawPanelS2, 1 drawFrameR3, 2 drawFrameS2
  if (!std::strcmp(name, "drawPanelS2")) kind = 0;
  else if (!std::strcmp(name, "drawFrameR3")) kind = 1;
  else if (!std::strcmp(name, "drawFrameS2")) kind = 2;
  else return false;
  int32_t x = i32(a[0]), y = i32(a[1]), w = i32(a[2]), h = i32(a[3]);
  round16b(x, w);
  round16b(y, h);
  if (w < 32 || h < 32 || w > 800 || h > 600) return false;
  static const uint32_t kCell[8] = {0, 1, 7, 3, 5, 2, 8, 6};
  static const uint32_t kOff[8] = {0x2c, 0xb0, 0, 0x58, 0x108, 0xdc, 0x134, 0x84};
  uint32_t sp[8], mp[8];
  int32_t style = 0;
  for (int k = 0; k < 8; ++k) {
    if (kind == 0) {
      sp[k] = state_va(ctx, &GameState::g_sprites5612e8) + 0x2Cu * (30 + kCell[k]);
      mp[k] = state_va(ctx, &GameState::g_sprites58ccb8) + 0x2Cu * (30 + kCell[k]);
    } else if (kind == 1) {
      sp[k] = state_va(ctx, &GameState::g_sprites5a53c8) + 0x2Cu * kCell[k];
      mp[k] = state_va(ctx, &GameState::g_sprites58b680) + 0x2Cu * kCell[k];
    } else {
      style = static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_courseType)));
      sp[k] = state_va(ctx, &GameState::g_spriteRef_562368) + static_cast<uint32_t>(style * 0x160) + kOff[k];
      mp[k] = state_va(ctx, &GameState::g_spriteRef_59d920) + static_cast<uint32_t>(style * 0x160) + kOff[k];
    }
  }
  const Sprite8 *ps[8], *pm[8];
  for (int k = 0; k < 8; ++k) {
    auto si = g.sprites.find(sp[k]), mi = g.sprites.find(mp[k]);
    if (si == g.sprites.end() || mi == g.sprites.end()) return false;
    ps[k] = &si->second;
    pm[k] = &mi->second;
  }
  struct Built {
    CellScene c;
    std::vector<uint8_t> idx, msk;
    std::array<uint16_t, 256> pal{};
  };
  static std::map<std::tuple<const Gfx *, int, int32_t, int32_t, int32_t, int32_t, uint32_t>, Built> cache;
  const auto key = std::make_tuple(static_cast<const Gfx *>(&g), kind, w, h, style, theme, a[4]);
  auto it = cache.find(key);
  if (it == cache.end()) {
    Built b;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    b.idx.assign(n, 0xff);
    b.msk.assign(n, 0xff);
    b.pal = ps[0]->pal;
    // a palette slot the pieces do not use, for the fill colour of the body
    std::vector<uint8_t> used(256, 0);
    for (int k = 0; k < 8; ++k)
      for (uint8_t v : ps[k]->px) used[v] = 1;
    int32_t slot = -1;
    for (int32_t v = 0xfe; v >= 0 && slot < 0; --v)
      if (!used[static_cast<size_t>(v)]) slot = v;
    auto fill = [&](int32_t fx, int32_t fy, int32_t fw, int32_t fh, uint16_t colour) {
      if (slot < 0) return;
      b.pal[static_cast<size_t>(slot)] = colour;
      for (int32_t yy = std::max(0, fy); yy < std::min(h, fy + fh); ++yy)
        for (int32_t xx = std::max(0, fx); xx < std::min(w, fx + fw); ++xx) {
          b.idx[static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx)] = static_cast<uint8_t>(slot);
          b.msk[static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx)] = 0;
        }
    };
    if (kind == 0) fill(4, 4, w - 8, h - 8, 0x4e79);
    if (kind == 2 && a[4] != 0) fill(2, 2, w - 4, h - 4, 0x7fdc);
    auto piece = [&](int k, int32_t px, int32_t py) {
      const Sprite8 &sp8 = *ps[k], &m = *pm[k];
      for (int32_t yy = 0; yy < sp8.h; ++yy)
        for (int32_t xx = 0; xx < sp8.w; ++xx) {
          const int32_t X = px + xx, Y = py + yy;
          if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
          const size_t o = static_cast<size_t>(yy) * static_cast<size_t>(sp8.w) + static_cast<size_t>(xx);
          if (o >= m.px.size() || sp8.px[o] >= 0xff || m.px[o] >= 0xff) continue;
          b.idx[static_cast<size_t>(Y) * static_cast<size_t>(w) + static_cast<size_t>(X)] = sp8.px[o];
          b.msk[static_cast<size_t>(Y) * static_cast<size_t>(w) + static_cast<size_t>(X)] = m.px[o];
        }
    };
    piece(0, 0, 0);
    if (w - 0x10 > 0x10)
      for (int32_t xi = 0x10, c = (w - 0x10 - 1) >> 4; c > 0; --c, xi += 0x10) {
        piece(1, xi, 0);
        piece(2, xi, h - 0x10);
      }
    if (h - 0x10 > 0x10)
      for (int32_t yi = 0x10, c = (h - 0x10 - 1) >> 4; c > 0; --c, yi += 0x10) {
        piece(3, 0, yi);
        piece(4, w - 0x10, yi);
      }
    piece(5, w - 0x10, 0);
    piece(6, w - 0x10, h - 0x10);
    piece(7, 0, h - 0x10);
    b.c.ok = ui_cell_scene(b.idx.data(), b.pal.data(), b.msk.data(), w, h, ui_theme(theme), b.c.scene, b.c.keep);
    if (b.c.ok && kind == 1 && a[4] != 0) {   // the shaded interior under the frame
      UiElem e;
      e.fill_top = e.fill_bot = UiRgba{0, 0, 0, 0.5f};
      UiPrim r;
      r.kind = kUiRRect;
      r.x0 = 2;
      r.y0 = 2;
      r.x1 = static_cast<float>(w - 2);
      r.y1 = static_cast<float>(h - 2);
      for (int q = 0; q < 4; ++q) r.rx[q] = r.ry[q] = 6.f;
      e.prims = {r};
      b.c.scene.elems.insert(b.c.scene.elems.begin(), e);
    }
    if (cache.size() > 256) cache.clear();
    it = cache.emplace(key, std::move(b)).first;
  }
  const Built &b = it->second;
  if (!b.c.ok) return false;
  ui_draw_scene(g, ctx, moved(b.c.scene, static_cast<float>(x), static_cast<float>(y)));
  Surface16 &d = g.root;   // the kept pixels from the assembled art, as Surface_stretchBlit draws them
  for (int32_t yy = std::max(y, 0); yy < std::min(y + h, d.h); ++yy)
    for (int32_t xx = std::max(x, 0); xx < std::min(x + w, d.w); ++xx) {
      const size_t o = static_cast<size_t>(yy - y) * static_cast<size_t>(w) + static_cast<size_t>(xx - x);
      if (!b.c.keep[o]) continue;
      const uint8_t cc = b.idx[o], k = b.msk[o];
      if (cc >= 0xff || k >= 0xff) continue;
      uint16_t &wd = d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)];
      hud_mark(&wd);
      const uint16_t sw = b.pal[cc];
      if (k == 0) {
        wd = sw;
        continue;
      }
      auto ch = [&](uint32_t dv, uint32_t sv) { return static_cast<uint16_t>(static_cast<uint16_t>(((dv * k) >> 8) + sv) >> 3); };
      wd = static_cast<uint16_t>((ch((wd >> 7) & 0xf8u, (sw >> 7) & 0xf8u) << 10) | (ch((wd >> 2) & 0xf8u, (sw >> 2) & 0xf8u) << 5) |
                                 ch((static_cast<uint32_t>(wd) << 3) & 0xf8u, (static_cast<uint32_t>(sw) << 3) & 0xf8u));
    }
  return true;
}
}  // namespace

// gfx.cpp gfx_drop: true when the drop was drawn here
bool ui_swap(Gfx &g, Ctx &ctx, const char *name, const uint32_t *a, int n) {
  const int32_t theme = g.ui_pilot;
  if (theme == kUiPilotOff) return false;
  g.ensure_root(ctx);
  if (!std::strcmp(name, "Surface_stretchBlit") && n >= 6) {
    // the loadInfoScreenArt pairs (mask = colour + 0x2c), and the frame-local pairs of the End of Year strips (reports.cpp: top 0x5500,
    // bottom 0x5700, line 0x5900) and the Shortcuts page (routing.cpp: 0x5100), whose masks are the colour key + 0x100
    const bool local = (a[0] == gfx_local_key(0x5500) || a[0] == gfx_local_key(0x5700) || a[0] == gfx_local_key(0x5900) ||
                        a[0] == gfx_local_key(0x5100)) && a[1] == a[0] + 0x100u;
    if (a[2] != g.root_va(ctx) || a[5] != 0 || !(local || (a[1] == a[0] + 0x2cu && report_cell(ctx, a[0])))) return false;
    auto si = g.sprites.find(a[0]), mi = g.sprites.find(a[1]);
    if (si == g.sprites.end() || mi == g.sprites.end()) return false;
    const CellScene &c = cell_scene(g, a[0], a[1], si->second, mi->second, theme);
    if (!c.ok) return false;
    const int32_t x = i32(a[3]), y = i32(a[4]);
    ui_draw_scene(g, ctx, moved(c.scene, static_cast<float>(x), static_cast<float>(y)));
    // the pixels the scene does not draw, from the art as Surface_stretchBlit draws them (copy or blend by the mask)
    const Sprite8 &s = si->second, &m = mi->second;
    Surface16 &d = g.root;
    for (int32_t yy = std::max(y, 0); yy < std::min(y + s.h, d.h); ++yy)
      for (int32_t xx = std::max(x, 0); xx < std::min(x + s.w, d.w); ++xx) {
        const size_t o = static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x);
        if (!c.keep[o] || o >= m.px.size()) continue;
        const uint8_t cc = s.px[o], k = m.px[o];
        if (cc >= 0xff || k >= 0xff) continue;
        uint16_t &w = d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)];
        const uint16_t sw = s.pal[cc];
        hud_mark(&w);
        if (k == 0) {
          w = sw;
          continue;
        }
        auto ch = [&](uint32_t dv, uint32_t sv) { return static_cast<uint16_t>(static_cast<uint16_t>(((dv * k) >> 8) + sv) >> 3); };
        w = static_cast<uint16_t>((ch((w >> 7) & 0xf8u, (sw >> 7) & 0xf8u) << 10) | (ch((w >> 2) & 0xf8u, (sw >> 2) & 0xf8u) << 5) |
                                  ch((static_cast<uint32_t>(w) << 3) & 0xf8u, (static_cast<uint32_t>(sw) << 3) & 0xf8u));
      }
    return true;
  }
  if (!std::strcmp(name, "blitPanel") && n >= 4) {
    // P40B: the shaded panel (the half-bright tiles of blitPanel: message windows, tips) as a half-dark rounded rectangle
    const int32_t x = i32(a[0]), y = i32(a[1]), w = i32(a[2]), h = i32(a[3]);
    if (w <= 0 || h <= 0) return false;
    UiScene sc;
    sc.cx0 = static_cast<float>(x);
    sc.cy0 = static_cast<float>(y);
    sc.cx1 = static_cast<float>(x + w);
    sc.cy1 = static_cast<float>(y + h);
    UiElem e;
    e.fill_top = e.fill_bot = UiRgba{0, 0, 0, 0.5f};
    UiPrim r;
    r.kind = kUiRRect;
    r.x0 = static_cast<float>(x);
    r.y0 = static_cast<float>(y);
    r.x1 = static_cast<float>(x + w);
    r.y1 = static_cast<float>(y + h);
    for (int q = 0; q < 4; ++q) r.rx[q] = r.ry[q] = 6.f;
    e.prims = {r};
    sc.elems.push_back(e);
    ui_draw_scene(g, ctx, sc);
    return true;
  }
  if (n >= 5 && frame_drawer(g, ctx, name, a, theme)) return true;
  if (!std::strcmp(name, "Image_draw") && n >= 4 && a[1] == g.root_va(ctx)) {
    // P40B batch 3: the routing map's panels (route screens: the bottom c0, the four tops c1..c4) and the buy-land screen (+0x2ec),
    // drawn by Image_draw without a mask (indexes 0xfe / 0xff transparent, draw16): the cell's scene at (left + x, top + y)
    const uint32_t rs = state_va(ctx, &GameState::g_spriteRef_8224c8);
    if (a[0] == rs || a[0] == rs + 0x2cu || a[0] == rs + 0x58u || a[0] == rs + 0x84u || a[0] == rs + 0xb0u || a[0] == rs + 0x2ecu) {
      auto si = g.sprites.find(a[0]);
      if (si != g.sprites.end()) {
        const Sprite8 &sp = si->second;
        Sprite8 mask;   // a mask cell for the engine: 0 (opaque) where draw16 draws, 0xff where it skips
        mask.w = sp.w;
        mask.h = sp.h;
        mask.px.resize(sp.px.size());
        for (size_t i = 0; i < sp.px.size(); ++i) mask.px[i] = sp.px[i] >= 0xfe ? 0xff : 0;
        const CellScene &c = cell_scene(g, a[0], 0, sp, mask, theme);
        if (c.ok) {
          const int32_t x = add32(sp.left, i32(a[2])), y = add32(sp.top, i32(a[3]));
          ui_draw_scene(g, ctx, moved(c.scene, static_cast<float>(x), static_cast<float>(y)));
          Surface16 &d = g.root;   // the kept pixels (the marbled ground, the icons) as draw16 draws them
          for (int32_t yy = std::max(y, 0); yy < std::min(y + sp.h, d.h); ++yy)
            for (int32_t xx = std::max(x, 0); xx < std::min(x + sp.w, d.w); ++xx) {
              const size_t o = static_cast<size_t>(yy - y) * static_cast<size_t>(sp.w) + static_cast<size_t>(xx - x);
              if (!c.keep[o] || sp.px[o] >= 0xfe) continue;
              uint16_t &wd = d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)];
              wd = sp.pal[sp.px[o]];
              hud_mark(&wd);
            }
          return true;
        }
      }
    }
  }
  if (!std::strcmp(name, "Image_draw") && n >= 4) {
    const uint32_t ok = state_va(ctx, &GameState::g_spriteRef_820f40);
    if (a[1] != g.root_va(ctx) || a[0] < ok || a[0] >= ok + 5u * 0x2cu || (a[0] - ok) % 0x2cu != 0) return false;
    const bool hover = a[0] == ok + 0x2cu;
    ui_draw_scene(g, ctx, ui_button_scene(ui_theme(theme), i32(a[2]), i32(a[3]), hover));
    return true;
  }
  return false;
}

}  // namespace portsim
