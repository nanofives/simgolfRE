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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

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
float sd_prim(const UiPrim &p, float x, float y) {
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

// ------------------------------------------------------------------------------------------------ the transport
namespace {
constexpr char kMagic[] = "P40U1:";
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
namespace {
constexpr UiRgba rgb(float r, float g, float b, float a = 1.f) { return UiRgba{r, g, b, a}; }
UiTheme make_game() {
  // measured from the user's install (log/roadmap/P40/holestat_geometry.txt): HoleSTAT.pcx head / row / foot cells, OkStates.pcx
  UiTheme t;
  t.frame = rgb(148, 150, 198);         // the panel's flat colour (the head cell's most frequent colour)
  t.frame_bevel = 5.f;                  // the left rim (174..132 over x 161..165) and the right shade (143..105 over x 633..638)
  t.frame_light = 0.60f;
  t.frame_dark = 0.45f;
  t.shadow = rgb(20, 24, 48, 0.45f);    // the dark rows under the foot (96, 98, 129 at y 368..372)
  t.shadow_d = 3.f;
  t.shadow_blur = 4.f;
  t.plate = rgb(255, 247, 231);         // the title plate's cream, its 2-pixel outline (42, 49, 91), an inner top shade
  t.plate_edge = rgb(42, 49, 91);
  t.label_top = rgb(170, 172, 226);     // the label pills: (147..182) shading into (191, 193, 251)
  t.label_bot = rgb(191, 193, 251);
  t.value_top = rgb(232, 225, 210);     // the value boxes: (196..243) shading into the cream
  t.value_bot = rgb(255, 247, 231);
  t.pill_outline = rgb(32, 42, 75);
  t.pill_outline_w = 1.6f;
  t.pill_bevel = 3.f;
  t.line = rgb(32, 42, 75);             // the connectors
  t.line_w = 2.f;
  t.box = rgb(255, 247, 231);           // the comment box and its sunken surround (96, 98, 129 above, 174..208 below)
  t.box_edge = rgb(176, 178, 214);
  t.well = rgb(45, 52, 88);             // the close button: the dark well, the blue ball (122..213), its hover yellow
  t.ball_top = rgb(140, 155, 240);
  t.ball_bot = rgb(200, 228, 255);
  t.ball_hover_top = rgb(240, 180, 0);
  t.ball_hover_bot = rgb(255, 238, 40);
  t.gloss = rgb(255, 255, 255, 0.9f);
  t.check = rgb(36, 47, 84);
  t.frame_rim = rgb(139, 141, 186);     // the button's ring
  t.frame_outline = rgb(40, 48, 90);
  return t;
}
UiTheme make_slate() {
  // an alternate palette (not the game's): a dark slate panel, white boxes, teal accents, a deeper shadow
  UiTheme t = make_game();
  t.frame = rgb(58, 70, 96);
  t.frame_light = 0.30f;
  t.frame_dark = 0.55f;
  t.shadow = rgb(0, 0, 0, 0.6f);
  t.shadow_d = 5.f;
  t.shadow_blur = 8.f;
  t.plate = rgb(250, 251, 253);
  t.plate_edge = rgb(64, 196, 214);
  t.label_top = rgb(206, 228, 238);
  t.label_bot = rgb(226, 242, 248);
  t.value_top = rgb(240, 242, 245);
  t.value_bot = rgb(255, 255, 255);
  t.pill_outline = rgb(18, 26, 40);
  t.line = rgb(64, 196, 214);
  t.line_w = 2.4f;
  t.box = rgb(250, 251, 253);
  t.box_edge = rgb(84, 98, 128);
  t.well = rgb(18, 24, 36);
  t.ball_top = rgb(30, 160, 180);
  t.ball_bot = rgb(110, 225, 235);
  t.ball_hover_top = rgb(235, 110, 40);
  t.ball_hover_bot = rgb(255, 190, 90);
  t.check = rgb(12, 20, 32);
  t.frame_rim = rgb(84, 98, 128);
  t.frame_outline = rgb(12, 18, 30);
  return t;
}
}  // namespace

const UiTheme &ui_theme(int32_t theme) {
  static const UiTheme game = make_game(), slate = make_slate();
  return theme == kUiPilotSlate ? slate : game;
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
  UiElem ring = flat(t.frame_rim);
  ring.prims = {ellipse(cx - 20.5f, cy - 20.5f, cx + 20.5f, cy + 20.5f)};
  ring.outline = t.frame_outline;
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
  g.note_root_write(x0, y0, x1 - x0, y1 - y0);
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

}  // namespace portsim
