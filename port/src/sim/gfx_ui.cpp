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
//   3 (P40D) the art-fitted cell engine reads each art colour one 5-bit level low (level * 8 - 1; P41G: was level << 3, which the
//     game's packing in ui_over555 now draws exactly)
//   4 (P40E) P40D's draw: no picture element (the kept pixels blitted from the art after the scene's run) and the peel restoring
//     the world under the run's whole inside (ui_run_opaque null): the window's render shows the world through the kept pixels'
//     edges, the holes Mariano saw
//   5 (P40E) ui_run_opaque gives null: the peel restores the world under a UI run's whole inside (as before P40E: the smooth scaling
//     mixes it into the edges of what the game draws over the panel later)
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
#include "portsim/sim/gfx_uivec.h"  // P41: the title menu's buttons (ui_swap) and uivec runs
#include "portsim/sim/gfx_uvgame.h"  // P41G: the in-game element scenes (uvg runs)
#include "portsim/sim/gfx_uvwide.h"  // P41W: the wide front-end backdrops and their picture overlays

namespace portsim {

int g_negctl_gfx_ui = 0;
extern int g_negctl_uvgame;   // gfx_uvgame.cpp (P41G): 3 = the hook off

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
// P40D: an element's colour grid at frame point (x, y), bilinear between node centres, clamped at the border. A grid of step 1 (the
// art's own pixels: no coarser grid held its colours) is sampled edge-aware: where its four nearest nodes differ by more than 48
// levels in a channel each node is a block with a transition one target pixel wide (`k` target pixels per frame unit) at the block's
// edge, as the port's sharp sprite filter draws art pixels; where they differ by less than 24 (a gradient, a rim's shading) it is
// bilinear; between the two a blend of both
const UiTex *tex_of(uint64_t id) {
  thread_local std::unordered_map<uint64_t, const UiTex *> local;
  auto it = local.find(id);
  if (it != local.end()) return it->second;
  const UiTex *t = ui_tex_get(id);
  if (t) local[id] = t;
  return t;
}
bool tex_colour(const UiElem &e, float x, float y, UiRgba &c, float k) {
  const UiTex *t = tex_of(e.tex);
  if (!t || t->w < 1 || t->h < 1) return false;
  const float st = e.tex_step > 0 ? e.tex_step : 1.f;
  const float u = clampf((x - e.tex_x0) / st - 0.5f, 0.f, static_cast<float>(t->w - 1));
  const float v = clampf((y - e.tex_y0) / st - 0.5f, 0.f, static_cast<float>(t->h - 1));
  const int32_t i0 = static_cast<int32_t>(u), j0 = static_cast<int32_t>(v);
  const int32_t i1 = std::min(i0 + 1, t->w - 1), j1 = std::min(j0 + 1, t->h - 1);
  float fu = u - static_cast<float>(i0), fv = v - static_cast<float>(j0);
  const uint8_t *a = t->rgb.data() + (static_cast<size_t>(j0) * static_cast<size_t>(t->w) + static_cast<size_t>(i0)) * 3u;
  const uint8_t *b = t->rgb.data() + (static_cast<size_t>(j0) * static_cast<size_t>(t->w) + static_cast<size_t>(i1)) * 3u;
  const uint8_t *cc = t->rgb.data() + (static_cast<size_t>(j1) * static_cast<size_t>(t->w) + static_cast<size_t>(i0)) * 3u;
  const uint8_t *d = t->rgb.data() + (static_cast<size_t>(j1) * static_cast<size_t>(t->w) + static_cast<size_t>(i1)) * 3u;
  if (st <= 1.f && k > 1.f) {
    int32_t spread = 0;
    for (int ch = 0; ch < 3; ++ch) {
      const int32_t lo = std::min(std::min(a[ch], b[ch]), std::min(cc[ch], d[ch])), hi = std::max(std::max(a[ch], b[ch]), std::max(cc[ch], d[ch]));
      spread = std::max(spread, hi - lo);
    }
    const float tb = clampf((static_cast<float>(spread) - 24.f) / 24.f, 0.f, 1.f);
    if (tb > 0.f) {
      fu += (clampf((fu - 0.5f) * k + 0.5f, 0.f, 1.f) - fu) * tb;
      fv += (clampf((fv - 0.5f) * k + 0.5f, 0.f, 1.f) - fv) * tb;
    }
  }
  float o[3];
  for (int ch = 0; ch < 3; ++ch)
    o[ch] = (a[ch] * (1.f - fu) + b[ch] * fu) * (1.f - fv) + (cc[ch] * (1.f - fu) + d[ch] * fu) * fv;
  c = UiRgba{o[0], o[1], o[2], 1.f};
  return true;
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
    const bool opaque = e.tex != 0 || (e.fill_top.a >= 1.f && e.fill_bot.a >= 1.f);
    // the fill at a frame point: the colour grid (P40D) or the vertical gradient
    auto fill_at = [&](float fx, float fy) {
      UiRgba c;
      if (e.tex != 0 && tex_colour(e, fx, fy, c, sc)) return c;
      return lerp(e.fill_top, e.fill_bot, gspan > 0 ? clampf((fy - e.gy0) / gspan, 0.f, 1.f) : 0.f);
    };
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
              UiRgba c = fill_at(fx, fy);
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
            UiRgba c = fill_at(fx, fy);
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

std::shared_ptr<const std::vector<uint8_t>> ui_run_opaque(const std::string &text, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  if (SG_NEGCTL(g_negctl_gfx_ui) == 4 || SG_NEGCTL(g_negctl_gfx_ui) == 5 || x1 <= x0 || y1 <= y0) return nullptr;
  static std::mutex mu;
  static std::unordered_map<std::string, std::shared_ptr<const std::vector<uint8_t>>> cache;
  static size_t cached_bytes = 0;
  std::string key = text;
  for (int32_t v : {x0, y0, x1, y1}) key.append(reinterpret_cast<const char *>(&v), sizeof v);
  {
    std::lock_guard<std::mutex> l(mu);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
  }
  const int32_t w = x1 - x0, h = y1 - y0;
  auto m = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  // P41G: a uivec scene's kept scale-1 frame (the one draw_uvec drew: uv_frame / uvg_frame) gives the coverage; before, every run was
  // rendered again here at each first show (a report's bands: about as long as their draw)
  std::shared_ptr<const UvFrame> fr = uv_frame(text);
  if (!fr && uvg_is_run_text(text)) fr = uvg_frame(text);
  if (fr) {
    const int32_t fw = fr->x1 - fr->x0;
    for (int32_t y = std::max(y0, fr->y0); y < std::min(y1, fr->y1); ++y)
      for (int32_t x = std::max(x0, fr->x0); x < std::min(x1, fr->x1); ++x)
        (*m)[static_cast<size_t>(y - y0) * static_cast<size_t>(w) + static_cast<size_t>(x - x0)] =
            fr->prem[(static_cast<size_t>(y - fr->y0) * static_cast<size_t>(fw) + static_cast<size_t>(x - fr->x0)) * 4u + 3u] >= 0.999f ? 1 : 0;
  } else {
    std::vector<float> prem;
    if (!ui_run_render(text, 1.0, 1.0, 0.0, 0.0, x0, y0, w, h, prem)) return nullptr;   // P41: an encoded scene or a uivec scene
    for (size_t i = 0; i < m->size(); ++i) (*m)[i] = prem[4 * i + 3] >= 0.999f ? 1 : 0;
  }
  std::lock_guard<std::mutex> l(mu);
  if (cache.size() > 256 || cached_bytes > (64u << 20)) {   // a bound (the panels of a few screens)
    cache.clear();
    cached_bytes = 0;
  }
  cached_bytes += key.size() + m->size();
  cache[key] = m;
  return m;
}

bool ui_run_bounds(const std::string &text, float &x0, float &y0, float &x1, float &y1) {
  int32_t k = 0, dx = 0, dy = 0;
  if (uv_parse(text, k, dx, dy)) {   // P41 (gfx_uivec.h)
    uv_bounds(k, dx, dy, x0, y0, x1, y1);
    return true;
  }
  if (uw_is_run_text(text)) return uw_run_bounds(text, x0, y0, x1, y1);   // P41W (gfx_uvwide.h)
  if (uvg_is_run_text(text)) return uvg_run_bounds(text, x0, y0, x1, y1);   // P41G (gfx_uvgame.h)
  UiScene s;
  if (!ui_decode(text, s)) return false;
  ui_bounds(s, x0, y0, x1, y1);
  return true;
}
bool ui_run_render(const std::string &text, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w,
                   int32_t h, std::vector<float> &prem) {
  int32_t k = 0, dx = 0, dy = 0;
  if (uv_parse(text, k, dx, dy)) {   // P41 (gfx_uivec.h)
    uv_render(k, dx, dy, sx, sy, ox, oy, px0, py0, w, h, prem);
    return true;
  }
  if (uw_is_run_text(text)) return uw_run_render(text, sx, sy, ox, oy, px0, py0, w, h, prem);   // P41W (gfx_uvwide.h)
  if (uvg_is_run_text(text)) return uvg_run_render(text, sx, sy, ox, oy, px0, py0, w, h, prem);   // P41G (gfx_uvgame.h)
  UiScene s;
  if (!ui_decode(text, s)) return false;
  ui_render(s, sx, sy, ox, oy, px0, py0, w, h, prem);
  return true;
}

uint16_t ui_over555(const float *p, uint16_t bg) {
  uint16_t out = 0;
  for (int ch = 0; ch < 3; ++ch) {
    const int sh = 10 - 5 * ch;
    const uint32_t l = (bg >> sh) & 31u;
    const float e = static_cast<float>((l << 3) | (l >> 2));
    const float v = p[ch] + (1.f - p[3]) * e;
    // P41G: the game's own packing of an 8-bit channel (gfx.cpp's colour LUT, c >> 3): a scene's opaque colour gives the word the
    // game makes from the same palette colour, and coverage 0 the background's word. It was lround(v * 31 / 255), which put a
    // palette colour such as 240 one level under the game's (29 against 30): the arc bracket's bar (240, 208, 128) drew as
    // (29, 25, 16) beside the member panel's stub (247, 214, 132 = 30, 26, 16), a colour step the original does not have (both
    // pack to 30, 26, 16). 1e-3 absorbs float error under a whole value.
    const int32_t q = std::min<int32_t>(31, static_cast<int32_t>(std::floor(clampf(v, 0.f, 255.f) + 1e-3f)) >> 3);
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
// P40D: the colour grids (UiElem::tex), stored like the fields
namespace {
std::unordered_map<uint64_t, std::unique_ptr<UiTex>> &tex_store() {
  static std::unordered_map<uint64_t, std::unique_ptr<UiTex>> s;
  return s;
}
float g_tune[5] = {0.5f, 16.f, 1.f, 1.f, 16.f};   // gfx_ui.h ui_tune: edge margin, largest grid step, field smoothing, (P40E) shape zones on, a zone's least pixels
}  // namespace
uint64_t ui_tex_put(int32_t w, int32_t h, std::vector<uint8_t> rgb) {
  uint64_t id = 0x9e3779b97f4a7c15ull ^ (static_cast<uint64_t>(static_cast<uint32_t>(w)) << 32 | static_cast<uint32_t>(h));
  for (uint8_t v : rgb) id = (id ^ v) * 1099511628211ull;
  if (id == 0) id = 1;
  std::lock_guard<std::mutex> l(field_mu());
  auto &s = tex_store();
  if (!s.count(id)) {
    auto t = std::make_unique<UiTex>();
    t->w = w;
    t->h = h;
    t->rgb = std::move(rgb);
    s.emplace(id, std::move(t));
  }
  return id;
}
const UiTex *ui_tex_get(uint64_t id) {
  std::lock_guard<std::mutex> l(field_mu());
  auto it = tex_store().find(id);
  return it == tex_store().end() ? nullptr : it->second.get();
}
float ui_tune(int32_t k, float v) {
  if (k < 0 || k >= 5) return 0.f;
  const float o = g_tune[k];
  g_tune[k] = v;
  return o;
}
float ui_tune_get(int32_t k) { return k >= 0 && k < 5 ? g_tune[k] : 0.f; }

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
  if (g_tune[2] < 0.5f) {   // P40D tuning: no smoothing
    for (size_t i = 0; i < sd.size(); ++i) sm[i] = std::max(-32.f, std::min(32.f, sd[i]));
    return sm;
  }
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
constexpr char kMagic[] = "P40U3:";   // P40B: + the field id; P40D: + the colour grid
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
    put_bytes(o, &e.tex, sizeof e.tex);
    put_f(o, e.tex_x0);
    put_f(o, e.tex_y0);
    put_f(o, e.tex_step);
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
    r.bytes(&e.tex, sizeof e.tex);
    e.tex_x0 = r.f();
    e.tex_y0 = r.f();
    e.tex_step = r.f();
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

// P40D (defined with the cell engine below): an art cell pair drawn by the cell engine at (x, y) with its kept pixels from the art
// (Surface_stretchBlit's place), and a colour-keyed cell (Image_draw's: indexes 0xfe / 0xff transparent) at the Image_draw position
bool ui_cell_pair_draw(Gfx &g, Ctx &ctx, uint32_t cs, uint32_t ms, int32_t x, int32_t y, int32_t theme, bool border_only);
bool ui_cell_keyed_draw(Gfx &g, Ctx &ctx, uint32_t cs, int32_t x, int32_t y, int32_t theme);

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

// P41 (gfx_uivec.h): a uivec scene's run text drawn as draw_scene draws a UiScene: its scale-1 render (kept per text) over the root,
// kept as a UI run whose text is the scene's name, clipped to the scene's whole-pixel bounds
bool draw_uvec(Gfx &g, Ctx &ctx, const std::string &text) {
  std::shared_ptr<const UvFrame> fr = uv_frame(text);
  if (!fr && uvg_is_run_text(text)) fr = uvg_frame(text);   // P41G (gfx_uvgame.h): kept per text
  if (!fr && uw_is_run_text(text)) {   // P41W: a picture or scene overlay (gfx_uvwide.h), its scale-1 render over its whole-pixel bounds
    float fx0, fy0, fx1, fy1;
    if (!uw_run_bounds(text, fx0, fy0, fx1, fy1)) return false;
    auto f = std::make_shared<UvFrame>();
    f->x0 = static_cast<int32_t>(std::floor(fx0));
    f->y0 = static_cast<int32_t>(std::floor(fy0));
    f->x1 = std::max(f->x0, static_cast<int32_t>(std::ceil(fx1)));
    f->y1 = std::max(f->y0, static_cast<int32_t>(std::ceil(fy1)));
    uw_run_render(text, 1.0, 1.0, 0.0, 0.0, f->x0, f->y0, f->x1 - f->x0, f->y1 - f->y0, f->prem);
    fr = f;
  }
  if (!fr) return false;
  g.ensure_root(ctx);
  Surface16 &r = g.root;
  const int32_t x0 = std::max(0, fr->x0), y0 = std::max(0, fr->y0), x1 = std::min(r.w, fr->x1), y1 = std::min(r.h, fr->y1);
  if (x1 <= x0 || y1 <= y0) return true;
  gfxb::TextRunScope ts;
  // P41G: the run changes no pixel outside its bounds: the snapshot is that rectangle grown by 4 (the box margin is 3), not whole rows
  gfxb::text_run_begin_rect(g, r.px.data(), r.w, r.h, ts, x0 - 4, y0 - 4, x1 + 4, y1 + 4);
  const int32_t fw = fr->x1 - fr->x0;
  // P41G: only the pixels the scene covers wholly (ui_run_opaque's keep, coverage >= 0.999) are marked as the panel layer's writes. A
  // pixel of the anti-aliased ring holds the scene blended over what was there; marked, the layer kept it (or, peeled, the word under
  // it) as an opaque pixel of the layer, the world of the panel's unmoved place, and a moved layer showed that world as a jagged
  // one-pixel fringe along the edge (log/roadmap/P41/portgame/notes.md). Unmarked, the peel leaves the ring what the layer held
  // before the run (transparent over the world, or the panel art an earlier primitive of the layer wrote) and the presenter draws
  // the run's window-scale edge over that. Negative control uvgame 5 = every covered pixel marked (the fringe back).
  const bool mark_all = SG_NEGCTL(g_negctl_uvgame) == 5;
  for (int32_t y = y0; y < y1; ++y)
    for (int32_t x = x0; x < x1; ++x) {
      const float *p = fr->prem.data() + (static_cast<size_t>(y - fr->y0) * static_cast<size_t>(fw) + static_cast<size_t>(x - fr->x0)) * 4u;
      if (p[3] <= 0.f) continue;
      uint16_t &o = r.px[static_cast<size_t>(y) * static_cast<size_t>(r.w) + static_cast<size_t>(x)];
      o = ui_over555(p, o);
      if (mark_all || p[3] >= 0.999f) hud_mark(&o);
    }
  GameFont f;
  f.face = kUiRunFace;
  f.size = 3;
  const int32_t clip[4] = {x0, y0, x1, y1};
  gfxb::text_run_end(g, ctx, ts, r.px.data(), r.w, r.h, 0, 0, clip, 0, text, 0, 0, 0, 0, &f);
  return true;
}

int32_t theme_of(Ctx &ctx) { return gfx_ui_pilot(ctx); }

// P40D: with an art-coloured theme (game) the Hole Stats cells and the close button go through the cell engine (gfx_routing.cpp's
// HoleSTAT cuts in g_spriteRef_821200: the head +0, a row +0x58, the foot +0xb0, each mask 0x2c after; OkStates in g_spriteRef_820f40);
// the scenes above stay for the other themes and whenever the art is not loaded
bool hs_art(Gfx &g, Ctx &ctx, int32_t th, uint32_t off, int32_t y) {
  if (ui_theme(th).art_colours <= 0.5f) return false;
  const uint32_t cs = state_va(ctx, &GameState::g_spriteRef_821200) + off;
  return ui_cell_pair_draw(g, ctx, cs, cs + 0x2cu, 0, y, th, false);
}
void b_hs_head(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff) return;
  if (hs_art(g, ctx, th, 0, static_cast<int32_t>(a[0]))) return;
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
      e.tex_y0 += dy;
    }
  }
  draw_scene(g, ctx, s);
}
void b_hs_row(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff || SG_NEGCTL(g_negctl_gfx_ui) == 2) return;
  if (hs_art(g, ctx, th, 0x58, static_cast<int32_t>(a[0]))) return;
  draw_scene(g, ctx, row_scene(ui_theme(th), static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1])));
}
void b_hs_foot(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff) return;
  if (hs_art(g, ctx, th, 0xb0, static_cast<int32_t>(a[0]))) return;
  draw_scene(g, ctx, foot_scene(ui_theme(th), static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1])));
}
void b_hs_button(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t th = theme_of(ctx);
  if (th == kUiPilotOff) return;
  if (ui_theme(th).art_colours > 0.5f &&
      ui_cell_keyed_draw(g, ctx, state_va(ctx, &GameState::g_spriteRef_820f40) + (a[2] != 0 ? 0x2cu : 0u), static_cast<int32_t>(a[0]),
                         static_cast<int32_t>(a[1]), th))
    return;
  draw_scene(g, ctx, button_scene(ui_theme(th), static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), a[2] != 0));
}

}  // namespace
// P40B: the scene drawing and the button for the cell swap (gfx_ui.cpp)
void ui_draw_scene(Gfx &g, Ctx &ctx, const UiScene &s) { draw_scene(g, ctx, s); }
bool ui_draw_uvec(Gfx &g, Ctx &ctx, const std::string &text) { return draw_uvec(g, ctx, text); }
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
// the pixels kept as art: pictures and coloured marks (the box, grown by 2, of each 8-connected cluster of picture-colour pixels, and
// the dark / white parts joined to it), and large dark regions inside the panel. thick_only (P40D, the art-fitted engine): a large
// dark region is kept only when at least half of its pixels have all four neighbours dark (a marbled ground), not a network of
// lines (the roster's and the reports' table grids, 1 to 2 pixels wide, which are regions of the scene)
void keep_pictures(const std::vector<uint8_t> &cls, const std::vector<uint8_t> &outside, int32_t w, int32_t h,
                   std::vector<uint8_t> &keep, bool thick_only = false) {
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
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
    if (thick_only) {
      int32_t inner = 0;
      for (int32_t j : k.px) {
        const int32_t x = j % w, y = j / w;
        inner += x > 0 && x + 1 < w && y > 0 && y + 1 < h && cls[static_cast<size_t>(j - 1)] == kDark &&
                 cls[static_cast<size_t>(j + 1)] == kDark && cls[static_cast<size_t>(j - w)] == kDark && cls[static_cast<size_t>(j + w)] == kDark;
      }
      if (inner * 2 < k.n) continue;
    }
    for (int32_t j : k.px) keep[static_cast<size_t>(j)] = 1;
  }
}

}  // namespace

// ------------------------------------------------------------------------------------------------ P40D: the art-fitted cell engine
// The game theme (art_colours = 1) builds a cell's scene so that its scale-1 render repeats the art (log/roadmap/P40/P40_notes.md,
// P40D: measured by port/test/p40_pixcmp.py) and its window render keeps the shapes sharp:
//   * every opaque pixel belongs to one region: the outer shadow (dark pixels reached from the border), the panel, and the components
//     of the inner classes (dark lines and rings, rims, pills, cream boxes, white rows), drawn in that order;
//   * a region's shape is a distance field of its pixels whose value at each of its own pixel centres is at most -margin (inside) and
//     at each other pixel centre at least +margin (ui_tune 0; 0.5 gives the art's coverage exactly at scale 1), and at a soft edge
//     pixel (the mask cell's partial alpha) 0.5 - alpha, so the silhouette's antialiasing is the art's own and moves to its
//     subpixel place at any scale; a pill, box or row whose pixels are a rounded rectangle (at most 1 % of them off) is that shape;
//   * a region's colour is a grid fitted to the art's pixels (the coarsest step of 16 / 8 / 4 / 2 / 1 whose bilinear colour gives
//     each pixel's 5-bit level exactly for 99 % of them and within one level for all), so gradients, bevels and rims are the art's;
//   * a region drawn before an adjacent later one reaches one pixel into it (the later one covers it), so no seam of the colour
//     under them shows where two regions meet at the window's scale.
// The 5-bit colour of an art pixel is taken as level * 255 / 31, the value ui_over555 rounds back to the same level.
namespace {
struct TexFit {
  uint64_t id = 0;
  float x0 = 0, y0 = 0, step = 1;
};
// the colour grid of the pixels px (indexes into a w-wide cell; rgb: 3 straight channels per cell pixel), tested at the pixels whose
// `test` byte is set
TexFit fit_tex(const std::vector<float> &rgb, const std::vector<int32_t> &px, const std::vector<uint8_t> &test, int32_t w) {
  TexFit f;
  if (px.empty()) return f;
  int32_t bx0 = (1 << 30), by0 = (1 << 30), bx1 = -1, by1 = -1;
  for (int32_t i : px) {
    const int32_t x = i % w, y = i / w;
    bx0 = std::min(bx0, x);
    by0 = std::min(by0, y);
    bx1 = std::max(bx1, x + 1);
    by1 = std::max(by1, y + 1);
  }
  const int32_t bw = bx1 - bx0, bh = by1 - by0;
  const int32_t steps[] = {16, 8, 4, 2, 1};
  for (int32_t st : steps) {
    if (static_cast<float>(st) > ui_tune_get(1) && st != 1) continue;
    const int32_t nx = (bw + st - 1) / st, ny = (bh + st - 1) / st;
    std::vector<double> sum(static_cast<size_t>(nx) * static_cast<size_t>(ny) * 3u, 0.0);
    std::vector<int32_t> cnt(static_cast<size_t>(nx) * static_cast<size_t>(ny), 0);
    for (int32_t i : px) {
      const int32_t x = i % w, y = i / w;
      const size_t q = static_cast<size_t>((y - by0) / st) * static_cast<size_t>(nx) + static_cast<size_t>((x - bx0) / st);
      for (int ch = 0; ch < 3; ++ch) sum[q * 3u + static_cast<size_t>(ch)] += rgb[static_cast<size_t>(i) * 3u + static_cast<size_t>(ch)];
      ++cnt[q];
    }
    std::vector<float> node(sum.size(), 0.f);
    std::vector<uint8_t> known(cnt.size(), 0);
    for (size_t q = 0; q < cnt.size(); ++q)
      if (cnt[q] > 0) {
        known[q] = 1;
        for (int ch = 0; ch < 3; ++ch) node[q * 3u + static_cast<size_t>(ch)] = static_cast<float>(sum[q * 3u + static_cast<size_t>(ch)] / cnt[q]);
      }
    // nodes without pixels: the colour of the nearest node with pixels (one breadth-first pass over the 8 neighbours), so no colour
    // of another region reaches the edge
    {
      std::vector<int32_t> queue;
      queue.reserve(known.size());
      for (size_t q = 0; q < known.size(); ++q)
        if (known[q]) queue.push_back(static_cast<int32_t>(q));
      for (size_t head = 0; head < queue.size(); ++head) {
        const int32_t q = queue[head], i = q % nx, j = q / nx;
        for (int32_t dj = -1; dj <= 1; ++dj)
          for (int32_t di = -1; di <= 1; ++di) {
            const int32_t ii = i + di, jj = j + dj;
            if (ii < 0 || jj < 0 || ii >= nx || jj >= ny) continue;
            const size_t r = static_cast<size_t>(jj) * static_cast<size_t>(nx) + static_cast<size_t>(ii);
            if (known[r]) continue;
            known[r] = 1;
            for (int ch = 0; ch < 3; ++ch) node[r * 3u + static_cast<size_t>(ch)] = node[static_cast<size_t>(q) * 3u + static_cast<size_t>(ch)];
            queue.push_back(static_cast<int32_t>(r));
          }
      }
    }
    std::vector<uint8_t> bytes(node.size());
    for (size_t q = 0; q < node.size(); ++q) bytes[q] = static_cast<uint8_t>(std::lround(clampf(node[q], 0.f, 255.f)));
    // the test: each tested pixel's 5-bit level from the bilinear colour at its centre against the art's
    bool ok = true;
    if (st > 1) {
      size_t n_test = 0, n_off = 0;
      for (int32_t i : px) {
        if (!test[static_cast<size_t>(i)]) continue;
        const int32_t x = i % w, y = i / w;
        const float u = clampf((x + 0.5f - bx0) / st - 0.5f, 0.f, static_cast<float>(nx - 1));
        const float v = clampf((y + 0.5f - by0) / st - 0.5f, 0.f, static_cast<float>(ny - 1));
        const int32_t i0 = static_cast<int32_t>(u), j0 = static_cast<int32_t>(v);
        const int32_t i1 = std::min(i0 + 1, nx - 1), j1 = std::min(j0 + 1, ny - 1);
        const float fu = u - static_cast<float>(i0), fv = v - static_cast<float>(j0);
        int32_t worst = 0;
        for (int ch = 0; ch < 3; ++ch) {
          auto nv = [&](int32_t ii, int32_t jj) {
            return static_cast<float>(bytes[(static_cast<size_t>(jj) * static_cast<size_t>(nx) + static_cast<size_t>(ii)) * 3u + static_cast<size_t>(ch)]);
          };
          const float c = (nv(i0, j0) * (1.f - fu) + nv(i1, j0) * fu) * (1.f - fv) + (nv(i0, j1) * (1.f - fu) + nv(i1, j1) * fu) * fv;
          const int32_t lq = static_cast<int32_t>(std::lround(clampf(c, 0.f, 255.f) * 31.f / 255.f));
          const int32_t la = static_cast<int32_t>(std::lround(clampf(rgb[static_cast<size_t>(i) * 3u + static_cast<size_t>(ch)], 0.f, 255.f) * 31.f / 255.f));
          worst = std::max(worst, std::abs(lq - la));
        }
        ++n_test;
        if (worst > 1) {
          ok = false;
          break;
        }
        if (worst == 1) ++n_off;
      }
      if (ok && n_off * 100 > n_test) ok = false;
    }
    if (!ok) continue;
    f.id = ui_tex_put(nx, ny, std::move(bytes));
    f.x0 = static_cast<float>(bx0);
    f.y0 = static_cast<float>(by0);
    f.step = static_cast<float>(st);
    return f;
  }
  return f;
}

// a field primitive over the pixels `in` of the w x h cell (cropped to their box, padded; a box touching the cell's edge continues
// past it), with the centre constraints of the margin and, where cov is given and 0 < cov < 1, the value 0.5 - cov
// (`in` answers for a cell pixel index; the pixels it holds and the soft ones of cov lie in the box [x0, x1) x [y0, y1))
template <class In>
UiPrim field_in_box(const In &in, const std::vector<float> *cov, int32_t w, int32_t h, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  UiPrim p;
  p.kind = kUiField;
  if (x1 <= x0) {
    p.x0 = p.y0 = 0;
    p.x1 = p.y1 = 1;
    return p;
  }
  constexpr int32_t pad = 6, fp = 4;
  const int32_t gx0 = x0 - pad, gy0 = y0 - pad, W = (x1 - x0) + 2 * pad, H = (y1 - y0) + 2 * pad;
  auto cell = [&](int32_t gx, int32_t gy) {   // the cell pixel a grid sample stands for (clamped: the edge continues)
    const int32_t cx = std::min(std::max(gx0 + gx, 0), w - 1), cy = std::min(std::max(gy0 + gy, 0), h - 1);
    return static_cast<size_t>(cy) * static_cast<size_t>(w) + static_cast<size_t>(cx);
  };
  std::vector<uint8_t> ext(static_cast<size_t>(W) * static_cast<size_t>(H), 0);
  for (int32_t gy = 0; gy < H; ++gy)
    for (int32_t gx = 0; gx < W; ++gx) ext[static_cast<size_t>(gy) * static_cast<size_t>(W) + static_cast<size_t>(gx)] = in(cell(gx, gy)) ? 1 : 0;
  std::vector<float> sd = ui_field_from_mask(ext, W, H, fp);
  const float m = ui_tune_get(0);
  const int32_t FW = W + 2 * fp;
  for (int32_t gy = 0; gy < H; ++gy)
    for (int32_t gx = 0; gx < W; ++gx) {
      float &v = sd[static_cast<size_t>(gy + fp) * static_cast<size_t>(FW) + static_cast<size_t>(gx + fp)];
      const size_t c = cell(gx, gy);
      if (cov && (*cov)[c] > 0.f && (*cov)[c] < 1.f) v = 0.5f - (*cov)[c];
      else if (in(c)) v = std::min(v, -m);
      else v = std::max(v, m);
    }
  p.x0 = static_cast<float>(gx0 - fp);
  p.y0 = static_cast<float>(gy0 - fp);
  p.x1 = static_cast<float>(gx0 - fp + FW);
  p.y1 = static_cast<float>(gy0 - fp + H + 2 * fp);
  p.field = ui_field_put(FW, H + 2 * fp, std::move(sd));
  return p;
}
// P40E: the kept pixels (pictures, coloured marks, the HUD panels' painted insides: `keep`) as an element of the scene, the art's
// colours on a grid fitted as the regions' grids are (step 1 for a picture: each art pixel, sampled edge-aware at the window's scale,
// tex_colour). Before P40E the cell's draw blitted them into the frame after the scene's run; the presenter peels a run off the frame
// (the world under it comes back) and scales the frame smoothly, so every kept pixel next to a peeled one showed the world mixed in,
// and the run was not drawn over it (its visibility there was broken by the blit): dots of the world inside the panels at 1080 and 4K.
// Only the opaque kept pixels (mask 0) are the picture's; a soft one stays the region's under it (else both would cover it).
// `reach` (may be null): pixels of regions drawn after the picture, one pixel of which the picture's shape takes too (as a region
// reaches into a later adjacent one), so no colour of what lies under it shows along their common edge.
bool picture_elem(const uint8_t *colour, const uint16_t *pal, const uint8_t *mask, int32_t w, int32_t h, const std::vector<uint8_t> &keep,
                  const std::vector<uint8_t> *reach, UiElem &e) {
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  std::vector<uint8_t> in(n, 0), test(n, 0);
  std::vector<float> rgb(n * 3u, 0.f);
  std::vector<int32_t> px;
  int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
  auto grow = [&](int32_t x, int32_t y) {
    x0 = std::min(x0, x);
    y0 = std::min(y0, y);
    x1 = std::max(x1, x + 1);
    y1 = std::max(y1, y + 1);
  };
  for (size_t i = 0; i < n && i < keep.size(); ++i) {
    if (!keep[i] || colour[i] >= 0xff || mask[i] != 0) continue;
    const uint16_t v = pal[colour[i]];
    const int32_t l[3] = {(v >> 10) & 31, (v >> 5) & 31, v & 31};
    for (int ch = 0; ch < 3; ++ch) rgb[i * 3u + static_cast<size_t>(ch)] = static_cast<float>(l[ch]) * 255.f / 31.f;
    in[i] = test[i] = 1;
    px.push_back(static_cast<int32_t>(i));
    grow(static_cast<int32_t>(i % static_cast<size_t>(w)), static_cast<int32_t>(i / static_cast<size_t>(w)));
  }
  if (px.empty()) return false;
  if (reach)
    for (int32_t j : px) {
      const int32_t x = j % w, y = j / w;
      for (int32_t dy = -1; dy <= 1; ++dy)
        for (int32_t dx = -1; dx <= 1; ++dx) {
          const int32_t xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
          const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
          if (!in[q] && (*reach)[q]) {
            in[q] = 2;
            grow(xx, yy);
          }
        }
    }
  const TexFit f = fit_tex(rgb, px, test, w);
  e = UiElem{};
  e.tex = f.id;
  e.tex_x0 = f.x0;
  e.tex_y0 = f.y0;
  e.tex_step = f.step;
  const size_t m = static_cast<size_t>(px[px.size() / 2]);
  e.fill_top = e.fill_bot = UiRgba{rgb[m * 3u], rgb[m * 3u + 1], rgb[m * 3u + 2], 1.f};
  e.prims = {field_in_box([&](size_t i) { return in[i] != 0; }, nullptr, w, h, x0, y0, x1, y1)};
  return true;
}
// P40E item 2 (ui_tune 3 on, the default): the kept pixels split into SHAPES (vector regions) and a PICTURE (the art's pixels).
//   * flat zones: the 4-connected sets of kept opaque pixels whose neighbours differ by at most 2 five-bit levels in every channel (a
//     gradient's steps chain; the edge between two colours does not);
//   * a zone of at least ui_tune 4 pixels (16) is a shape when its 8-connected group of kept pixels has at least half of its pixels in
//     such zones (a ring, a disc, a square, a slot, a divider, a panel's ground); a group made of small zones is a picture (an icon, a
//     face, a logo) and stays the art's;
//   * a small-zone pixel between two shapes A and B whose colour lies on the segment between their local colours (within 2 levels; A
//     and B at least 4 levels apart) is their antialiased edge: it is owned by the one drawn first and the later one covers it by the
//     share t of its colour (the field value 0.5 - t, as a soft mask pixel), so the edge moves to its subpixel place at the window's
//     scale and stays sharp; a small-zone pixel within 3 levels of the one shape beside it joins that shape; the others are picture;
//   * shapes are drawn largest first, each a field over its pixels reaching one pixel into the shapes, picture pixels and `reach`
//     pixels drawn after it, with its own colour grid (fit_tex over its pixels without its antialiased ones); the picture last.
// keep then holds 3 for a shape's pixel, 1 for a picture pixel, 0 for a soft (mask) kept pixel (the region under it draws it).
void picture_zones(const uint8_t *colour, const uint16_t *pal, const uint8_t *mask, int32_t w, int32_t h, std::vector<uint8_t> &keep,
                   const std::vector<uint8_t> *reach, UiScene &out) {
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  std::vector<uint8_t> K(n, 0);
  std::vector<int32_t> lv(n * 3u, 0);
  std::vector<float> rgb(n * 3u, 0.f);
  for (size_t i = 0; i < n && i < keep.size(); ++i) {
    if (!keep[i] || colour[i] >= 0xff || mask[i] != 0) continue;
    K[i] = 1;
    const uint16_t v = pal[colour[i]];
    const int32_t l[3] = {(v >> 10) & 31, (v >> 5) & 31, v & 31};
    for (int ch = 0; ch < 3; ++ch) {
      lv[i * 3u + static_cast<size_t>(ch)] = l[ch];
      rgb[i * 3u + static_cast<size_t>(ch)] = static_cast<float>(l[ch]) * 255.f / 31.f;
    }
  }
  auto near_lv = [&](size_t a, size_t b, int32_t d) {
    for (int ch = 0; ch < 3; ++ch)
      if (std::abs(lv[a * 3u + static_cast<size_t>(ch)] - lv[b * 3u + static_cast<size_t>(ch)]) > d) return false;
    return true;
  };
  // the flat zones
  std::vector<int32_t> zone(n, -1), zsize;
  std::vector<int32_t> st;
  for (size_t i0 = 0; i0 < n; ++i0) {
    if (!K[i0] || zone[i0] >= 0) continue;
    const int32_t z = static_cast<int32_t>(zsize.size());
    zsize.push_back(0);
    zone[i0] = z;
    st.assign(1, static_cast<int32_t>(i0));
    while (!st.empty()) {
      const int32_t j = st.back();
      st.pop_back();
      ++zsize[static_cast<size_t>(z)];
      const int32_t x = j % w, y = j / w;
      const int32_t nb[4] = {x > 0 ? j - 1 : -1, x + 1 < w ? j + 1 : -1, y > 0 ? j - w : -1, y + 1 < h ? j + w : -1};
      for (int32_t q : nb)
        if (q >= 0 && K[static_cast<size_t>(q)] && zone[static_cast<size_t>(q)] < 0 && near_lv(static_cast<size_t>(j), static_cast<size_t>(q), 2)) {
          zone[static_cast<size_t>(q)] = z;
          st.push_back(q);
        }
    }
  }
  const int32_t smin = std::max(4, static_cast<int32_t>(ui_tune_get(4)));
  // the 8-connected groups and their share of pixels in large zones
  std::vector<int32_t> grp(n, -1), gn, gbig;
  for (size_t i0 = 0; i0 < n; ++i0) {
    if (!K[i0] || grp[i0] >= 0) continue;
    const int32_t g = static_cast<int32_t>(gn.size());
    gn.push_back(0);
    gbig.push_back(0);
    grp[i0] = g;
    st.assign(1, static_cast<int32_t>(i0));
    while (!st.empty()) {
      const int32_t j = st.back();
      st.pop_back();
      ++gn[static_cast<size_t>(g)];
      if (zsize[static_cast<size_t>(zone[static_cast<size_t>(j)])] >= smin) ++gbig[static_cast<size_t>(g)];
      const int32_t x = j % w, y = j / w;
      for (int32_t dy = -1; dy <= 1; ++dy)
        for (int32_t dx = -1; dx <= 1; ++dx) {
          const int32_t xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
          const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
          if (K[q] && grp[q] < 0) {
            grp[q] = g;
            st.push_back(static_cast<int32_t>(q));
          }
        }
    }
  }
  auto shape_group = [&](size_t i) { const size_t g = static_cast<size_t>(grp[i]); return gbig[g] * 2 >= gn[g]; };
  // the shapes, largest first
  std::vector<int32_t> zl;
  {
    std::vector<uint8_t> seen(zsize.size(), 0);
    for (size_t i = 0; i < n; ++i) {
      if (!K[i]) continue;
      const int32_t z = zone[i];
      if (seen[static_cast<size_t>(z)] || zsize[static_cast<size_t>(z)] < smin || !shape_group(i)) continue;
      seen[static_cast<size_t>(z)] = 1;
      zl.push_back(z);
    }
  }
  std::stable_sort(zl.begin(), zl.end(), [&](int32_t a, int32_t b) { return zsize[static_cast<size_t>(a)] > zsize[static_cast<size_t>(b)]; });
  std::vector<int32_t> rank(zsize.size(), -1);
  for (size_t k = 0; k < zl.size(); ++k) rank[static_cast<size_t>(zl[k])] = static_cast<int32_t>(k);
  std::vector<int32_t> own(n, -1), upper(n, -1);
  std::vector<float> ucov(n, 0.f);
  std::vector<uint8_t> nocol(n, 0);
  for (size_t i = 0; i < n; ++i)
    if (K[i] && rank[static_cast<size_t>(zone[i])] >= 0) own[i] = rank[static_cast<size_t>(zone[i])];
  // the small-zone pixels of the shape groups: an antialiased edge, a shape's own pixel, or picture
  for (size_t i = 0; i < n; ++i) {
    if (!K[i] || own[i] >= 0 || !shape_group(i)) continue;
    const int32_t x = static_cast<int32_t>(i % static_cast<size_t>(w)), y = static_cast<int32_t>(i / static_cast<size_t>(w));
    std::map<int32_t, std::array<float, 4>> cand;   // shape rank -> (count, colour sums)
    for (int32_t r = 1; r <= 2 && cand.empty(); ++r)
      for (int32_t dy = -r; dy <= r; ++dy)
        for (int32_t dx = -r; dx <= r; ++dx) {
          const int32_t xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
          const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
          if (!K[q] || rank[static_cast<size_t>(zone[q])] < 0) continue;
          std::array<float, 4> &c = cand[rank[static_cast<size_t>(zone[q])]];
          c[0] += 1.f;
          for (int ch = 0; ch < 3; ++ch) c[static_cast<size_t>(ch) + 1] += rgb[q * 3u + static_cast<size_t>(ch)];
        }
    if (cand.empty()) continue;
    std::vector<std::pair<float, int32_t>> by;
    for (auto &kv : cand) by.push_back({-kv.second[0], kv.first});
    std::sort(by.begin(), by.end());
    auto mean = [&](int32_t rk, int ch) { const auto &c = cand[rk]; return c[static_cast<size_t>(ch) + 1] / c[0]; };
    const float *c = &rgb[i * 3u];
    constexpr float kLv = 255.f / 31.f;
    if (by.size() >= 2) {
      const int32_t A = by[0].second, B = by[1].second;
      float d[3], dd = 0.f, dot = 0.f, span = 0.f;
      for (int ch = 0; ch < 3; ++ch) {
        d[ch] = mean(A, ch) - mean(B, ch);
        dd += d[ch] * d[ch];
        dot += (c[ch] - mean(B, ch)) * d[ch];
        span = std::max(span, std::fabs(d[ch]));
      }
      if (span >= 4.f * kLv && dd > 0.f) {
        const float t = clampf(dot / dd, 0.f, 1.f);
        float err = 0.f;
        for (int ch = 0; ch < 3; ++ch) err = std::max(err, std::fabs(c[ch] - (mean(B, ch) + t * d[ch])));
        if (err <= 2.f * kLv) {
          const int32_t D = std::min(A, B), U = std::max(A, B);   // drawn first / later
          const float cu = U == A ? t : 1.f - t;
          if (cu >= 0.98f) {
            own[i] = U;
          } else {
            own[i] = D;
            nocol[i] = 1;
            if (cu > 0.02f) {
              upper[i] = U;
              ucov[i] = cu;
            }
          }
          continue;
        }
      }
    }
    const int32_t A = by[0].second;
    float far = 0.f;
    for (int ch = 0; ch < 3; ++ch) far = std::max(far, std::fabs(c[ch] - mean(A, ch)));
    if (far <= 3.f * kLv) own[i] = A;
  }
  // the colour grids, then (second pass) each antialiased pixel's coverage from the colours the two grids draw there: the projection
  // of its colour on that segment; one the blend misses by more than one level (or with no upper shape left) becomes the lower
  // shape's own colour pixel (its grid then holds it), so the scale-1 render keeps the art's colour within one level
  std::vector<uint8_t> test(n, 0);
  std::vector<TexFit> fits(zl.size());
  auto fit_zone = [&](size_t k) {
    std::vector<int32_t> col;
    for (size_t i = 0; i < n; ++i)
      if (own[i] == static_cast<int32_t>(k) && !nocol[i]) col.push_back(static_cast<int32_t>(i));
    if (col.empty())
      for (size_t i = 0; i < n; ++i)
        if (own[i] == static_cast<int32_t>(k)) col.push_back(static_cast<int32_t>(i));
    for (int32_t j : col) test[static_cast<size_t>(j)] = 1;
    fits[k] = fit_tex(rgb, col, test, w);
    for (int32_t j : col) test[static_cast<size_t>(j)] = 0;
  };
  for (size_t k = 0; k < zl.size(); ++k) fit_zone(k);
  auto grid_at = [&](int32_t k, size_t i, float *o) {
    UiElem e;
    e.tex = fits[static_cast<size_t>(k)].id;
    e.tex_x0 = fits[static_cast<size_t>(k)].x0;
    e.tex_y0 = fits[static_cast<size_t>(k)].y0;
    e.tex_step = fits[static_cast<size_t>(k)].step;
    UiRgba c;
    if (!tex_colour(e, static_cast<float>(i % static_cast<size_t>(w)) + 0.5f, static_cast<float>(i / static_cast<size_t>(w)) + 0.5f, c, 1.f))
      return false;
    o[0] = c.r;
    o[1] = c.g;
    o[2] = c.b;
    return true;
  };
  {
    constexpr float kLv = 255.f / 31.f;
    std::vector<uint8_t> refit(zl.size(), 0);
    for (size_t i = 0; i < n; ++i) {
      if (!nocol[i] || own[i] < 0) continue;
      const float *c = &rgb[i * 3u];
      float dc[3], uc[3];
      bool ok = grid_at(own[i], i, dc);
      if (ok && upper[i] >= 0 && grid_at(upper[i], i, uc)) {
        float dd = 0.f, dot = 0.f;
        for (int ch = 0; ch < 3; ++ch) {
          dd += (uc[ch] - dc[ch]) * (uc[ch] - dc[ch]);
          dot += (c[ch] - dc[ch]) * (uc[ch] - dc[ch]);
        }
        const float t = dd > 0.f ? clampf(dot / dd, 0.f, 1.f) : 0.f;
        float err = 0.f;
        for (int ch = 0; ch < 3; ++ch) err = std::max(err, std::fabs(c[ch] - (dc[ch] + t * (uc[ch] - dc[ch]))));
        if (err <= kLv && t > 0.02f) {
          ucov[i] = std::min(t, 0.98f);
          continue;
        }
      } else if (ok) {
        float err = 0.f;
        for (int ch = 0; ch < 3; ++ch) err = std::max(err, std::fabs(c[ch] - dc[ch]));
        if (err <= kLv) {
          upper[i] = -1;
          continue;
        }
      }
      upper[i] = -1;   // the lower shape's own colour
      nocol[i] = 0;
      refit[static_cast<size_t>(own[i])] = 1;
    }
    for (size_t k = 0; k < zl.size(); ++k)
      if (refit[k]) fit_zone(k);
  }
  // the elements: the shapes in their order, then the picture
  std::vector<uint32_t> mark(n, 0);
  std::vector<float> cov(n, 0.f);
  std::vector<std::vector<int32_t>> pix(zl.size()), soft(zl.size());
  for (size_t i = 0; i < n; ++i) {
    if (own[i] >= 0) pix[static_cast<size_t>(own[i])].push_back(static_cast<int32_t>(i));
    if (upper[i] >= 0) soft[static_cast<size_t>(upper[i])].push_back(static_cast<int32_t>(i));
  }
  for (size_t k = 0; k < zl.size(); ++k) {
    const uint32_t tok = static_cast<uint32_t>(k) + 1u;
    int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
    auto grow = [&](int32_t j) {
      x0 = std::min(x0, j % w);
      y0 = std::min(y0, j / w);
      x1 = std::max(x1, j % w + 1);
      y1 = std::max(y1, j / w + 1);
    };
    std::vector<int32_t> col;
    for (int32_t j : pix[k]) {
      mark[static_cast<size_t>(j)] = tok;
      grow(j);
      if (!nocol[static_cast<size_t>(j)]) col.push_back(j);
    }
    if (col.empty()) col = pix[k];
    // one pixel into what is drawn after it: later shapes, the picture, the regions of `reach`
    for (int32_t j : pix[k]) {
      const int32_t x = j % w, y = j / w;
      for (int32_t dy = -1; dy <= 1; ++dy)
        for (int32_t dx = -1; dx <= 1; ++dx) {
          const int32_t xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
          const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
          if (mark[q] == tok) continue;
          const bool later = (K[q] && (own[q] < 0 || own[q] > static_cast<int32_t>(k))) || (reach && (*reach)[q]);
          if (!later || upper[q] == static_cast<int32_t>(k)) continue;
          mark[q] = tok;
          grow(static_cast<int32_t>(q));
        }
    }
    for (int32_t j : soft[k]) {
      cov[static_cast<size_t>(j)] = ucov[static_cast<size_t>(j)];
      grow(j);
    }
    const TexFit &f = fits[k];
    UiElem e;
    e.tex = f.id;
    e.tex_x0 = f.x0;
    e.tex_y0 = f.y0;
    e.tex_step = f.step;
    const size_t m = static_cast<size_t>(col[col.size() / 2]);
    e.fill_top = e.fill_bot = UiRgba{rgb[m * 3u], rgb[m * 3u + 1], rgb[m * 3u + 2], 1.f};
    e.prims = {field_in_box([&](size_t i) { return mark[i] == tok; }, &cov, w, h, x0, y0, x1, y1)};
    out.elems.push_back(std::move(e));
    for (int32_t j : soft[k]) cov[static_cast<size_t>(j)] = 0.f;
  }
  std::vector<uint8_t> pic(n, 0);
  for (size_t i = 0; i < n; ++i) pic[i] = K[i] && own[i] < 0;
  UiElem e;
  if (picture_elem(colour, pal, mask, w, h, pic, nullptr, e)) out.elems.push_back(std::move(e));
  for (size_t i = 0; i < keep.size(); ++i) keep[i] = !K[i] ? 0 : own[i] >= 0 ? 3 : 1;
}
// P40E: `keep` (1 = a kept pixel) becomes the scene's picture element, appended to `out`; keep then says how each kept pixel is drawn:
// 1 by the picture, 0 a soft one (the region under it draws it). Negative control 4: no picture, keep = 2 for every kept pixel (the
// cell's draw blits them from the art after the run, as before P40E: the holes of the window's render come back)
void keep_as_picture(const uint8_t *colour, const uint16_t *pal, const uint8_t *mask, int32_t w, int32_t h, std::vector<uint8_t> &keep,
                     const std::vector<uint8_t> *reach, UiScene &out) {
  if (SG_NEGCTL(g_negctl_gfx_ui) == 4) {
    for (uint8_t &k : keep)
      if (k) k = 2;
    return;
  }
  if (ui_tune_get(3) > 0.5f) {   // P40E item 2: the shapes among the kept pixels as vector regions, the rest a picture
    picture_zones(colour, pal, mask, w, h, keep, reach, out);
    return;
  }
  UiElem e;
  if (picture_elem(colour, pal, mask, w, h, keep, reach, e)) out.elems.push_back(std::move(e));
  for (size_t i = 0; i < keep.size(); ++i)
    if (keep[i] && mask[i] != 0) keep[i] = 0;
}
// the same over a whole-cell mask (its box found by a scan)
UiPrim field_box(const std::vector<uint8_t> &in, const std::vector<float> *cov, int32_t w, int32_t h) {
  int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
      if (in[i] || (cov && (*cov)[i] > 0.f)) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x + 1);
        y1 = std::max(y1, y + 1);
      }
    }
  return field_in_box([&](size_t i) { return in[i] != 0; }, cov, w, h, x0, y0, x1, y1);
}

bool cell_scene_art(const uint8_t *colour, const uint16_t *pal, const uint8_t *mask, int32_t w, int32_t h, const UiTheme &t,
                    UiScene &out, std::vector<uint8_t> &keep, bool border_only) {
  (void)t;
  out = UiScene{};
  out.cx0 = 0;
  out.cy0 = 0;
  out.cx1 = static_cast<float>(w);
  out.cy1 = static_cast<float>(h);
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  keep.assign(n, 0);
  std::vector<uint8_t> cls(n, kNone);
  std::vector<float> alpha(n, 0.f), rgb(n * 3u, 0.f);
  std::vector<uint8_t> darkish(n, 0);   // a soft pixel whose straight colour is dark (the outer shadow's soft part)
  bool any = false;
  for (size_t i = 0; i < n; ++i) {
    const uint8_t c = colour[i], k = mask[i];
    if (c >= 0xff || k >= 0xff) continue;
    const float a = 1.f - static_cast<float>(k) / 256.f;   // Surface_stretchBlit keeps (dst * k) >> 8 of what lies under
    const uint16_t v = pal[c];
    const int32_t l[3] = {(v >> 10) & 31, (v >> 5) & 31, v & 31};
    // (negctl 3: the colour one 5-bit level low, level * 8 - 1: what P40B's reading, level << 3, gave under ui_over555's former
    // rounding for most levels; with the game's packing, c >> 3, level << 3 itself is exact)
    const bool low = SG_NEGCTL(g_negctl_gfx_ui) == 3;
    for (int ch = 0; ch < 3; ++ch)
      rgb[i * 3u + static_cast<size_t>(ch)] =
          std::min(255.f, (low ? std::max(0.f, static_cast<float>(l[ch]) * 8.f - 1.f) : static_cast<float>(l[ch]) * 255.f / 31.f) / a);
    alpha[i] = a;
    const int r = static_cast<int>(rgb[i * 3u]), g = static_cast<int>(rgb[i * 3u + 1]), b = static_cast<int>(rgb[i * 3u + 2]);
    const Cls cc = classify(r, g, b);
    darkish[i] = cc == kDark;
    if (a >= 0.5f) {
      cls[i] = cc;
      any = true;
    }
  }
  if (!any) return false;
  // the outside: transparent, dark and soft dark pixels reached from the cell's border
  std::vector<uint8_t> outside(n, 0);
  {
    std::vector<int32_t> st;
    auto pass = [&](size_t i) { return cls[i] == kNone ? (alpha[i] < 0.5f && (alpha[i] == 0.f || darkish[i])) : cls[i] == kDark; };
    auto push = [&](int32_t i) {
      if (outside[static_cast<size_t>(i)] || !pass(static_cast<size_t>(i))) return;
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
  keep_pictures(cls, outside, w, h, keep, true);
  for (size_t i = 0; i < n; ++i)
    if (keep[i] && cls[i] != kNone) cls[i] = kFrame;
  std::vector<uint8_t> all_test(n, 0);
  for (size_t i = 0; i < n; ++i) all_test[i] = alpha[i] >= 1.f ? 1 : 0;
  auto make = [&](const std::vector<int32_t> &px, UiPrim prim) {
    UiElem e;
    const TexFit f = fit_tex(rgb, px, all_test, w);
    e.tex = f.id;
    e.tex_x0 = f.x0;
    e.tex_y0 = f.y0;
    e.tex_step = f.step;
    if (!px.empty()) {
      const size_t i = static_cast<size_t>(px[px.size() / 2]);
      e.fill_top = e.fill_bot = UiRgba{rgb[i * 3u], rgb[i * 3u + 1], rgb[i * 3u + 2], 1.f};
    }
    e.prims = {prim};
    return e;
  };
  // the outer shadow (dark pixels outside the panel, opaque or soft)
  {
    std::vector<uint8_t> in(n, 0);
    std::vector<float> cov(n, 0.f);
    std::vector<int32_t> px;
    bool has = false;
    for (size_t i = 0; i < n; ++i)
      if (outside[i] && alpha[i] > 0.f) {
        in[i] = alpha[i] >= 0.5f;
        cov[i] = alpha[i];
        if (alpha[i] >= 0.25f) px.push_back(static_cast<int32_t>(i));
        has = true;
      }
    if (has && !px.empty()) {
      for (size_t i = 0; i < n; ++i)   // one pixel into the panel drawn over it
        if (!outside[i] && alpha[i] >= 0.5f) {
          const int32_t x = static_cast<int32_t>(i % static_cast<size_t>(w)), y = static_cast<int32_t>(i / static_cast<size_t>(w));
          bool nb = false;
          for (int32_t dy = -1; dy <= 1 && !nb; ++dy)
            for (int32_t dx = -1; dx <= 1 && !nb; ++dx) {
              const int32_t xx = x + dx, yy = y + dy;
              if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
              const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
              nb = outside[q] && alpha[q] >= 0.5f;
            }
          if (nb) in[i] = 1;
        }
      out.elems.push_back(make(px, field_box(in, &cov, w, h)));
    }
  }
  // the panel: every pixel not outside, its silhouette's soft pixels at their coverage; its colour from its own (frame) pixels
  std::vector<uint8_t> panel(n, 0);
  std::vector<float> pcov(n, 0.f);
  std::vector<int32_t> frame_px;
  for (size_t i = 0; i < n; ++i) {
    if (outside[i] || alpha[i] <= 0.f) continue;
    panel[i] = alpha[i] >= 0.5f;
    pcov[i] = alpha[i];
    // the frame's pixels, the soft silhouette, and (P40E) every soft pixel: a soft kept pixel is the panel's (the picture draws only
    // the opaque ones)
    if (alpha[i] >= 0.25f && ((cls[i] == kFrame && !keep[i]) || cls[i] == kNone || alpha[i] < 1.f)) frame_px.push_back(static_cast<int32_t>(i));
  }
  if (frame_px.empty())
    for (size_t i = 0; i < n; ++i)
      if (panel[i]) frame_px.push_back(static_cast<int32_t>(i));
  out.elems.push_back(make(frame_px, field_box(panel, &pcov, w, h)));
  if (border_only) {   // P40C: the panel's shape only; its inside (farther than 5 pixels from the edge) stays the art
    std::vector<float> dist(n, 1e9f);
    for (int32_t y = 0; y < h; ++y)
      for (int32_t x = 0; x < w; ++x) {
        const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
        if (!panel[i]) {
          dist[i] = 0;
          continue;
        }
        if (x == 0 || y == 0) dist[i] = 1;
        if (x > 0) dist[i] = std::min(dist[i], dist[i - 1] + 1);
        if (y > 0) dist[i] = std::min(dist[i], dist[i - static_cast<size_t>(w)] + 1);
      }
    for (int32_t y = h - 1; y >= 0; --y)
      for (int32_t x = w - 1; x >= 0; --x) {
        const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
        if (x == w - 1 || y == h - 1) dist[i] = std::min(dist[i], 1.f);
        if (x + 1 < w) dist[i] = std::min(dist[i], dist[i + 1] + 1);
        if (y + 1 < h) dist[i] = std::min(dist[i], dist[i + static_cast<size_t>(w)] + 1);
      }
    for (size_t i = 0; i < n; ++i)
      if (cls[i] != kNone && (dist[i] > 5.f || (panel[i] && cls[i] != kFrame))) keep[i] = 1;
    keep_as_picture(colour, pal, mask, w, h, keep, nullptr, out);   // P40E: over the panel
    return true;
  }
  // P40E: the kept pixels drawn by the scene over the panel, reaching one pixel into the inner regions drawn after them
  {
    std::vector<uint8_t> inner(n, 0);
    for (size_t i = 0; i < n; ++i)
      inner[i] = !outside[i] && !keep[i] && (cls[i] == kDark || cls[i] == kRim || cls[i] == kPill || cls[i] == kCream || cls[i] == kWhite);
    keep_as_picture(colour, pal, mask, w, h, keep, &inner, out);
  }
  // the inner regions, in drawing order
  const uint8_t order[] = {kDark, kRim, kPill, kCream, kWhite};
  std::vector<uint8_t> rank(n, 0);   // 1 + the place in `order` of each inner pixel, 0 for the others
  for (size_t i = 0; i < n; ++i) {
    if (outside[i] || keep[i] || cls[i] == kNone) continue;
    for (uint8_t k = 0; k < 5; ++k)
      if (cls[i] == order[k]) rank[i] = static_cast<uint8_t>(k + 1);
  }
  std::vector<uint32_t> owner(n, 0);
  uint32_t token = 0;
  for (uint8_t k = 0; k < 5; ++k) {
    const uint8_t c = order[k];
    std::vector<uint8_t> lab(n, kNone);
    for (size_t i = 0; i < n; ++i)
      if (rank[i] == k + 1) lab[i] = c;
    std::vector<int32_t> small;
    auto emit = [&](const std::vector<int32_t> &px, const Comp *comp) {
      const uint32_t own = ++token;   // this region's pixels: owner == own (owner is shared by every region, never cleared)
      int32_t bx0 = w, by0 = h, bx1 = -1, by1 = -1;
      for (int32_t j : px) {
        owner[static_cast<size_t>(j)] = own;
        bx0 = std::min(bx0, j % w);
        by0 = std::min(by0, j / w);
        bx1 = std::max(bx1, j % w + 1);
        by1 = std::max(by1, j / w + 1);
      }
      auto in = [&](size_t i) { return owner[i] == own; };
      // a box, pill or row that is a rounded rectangle: that shape
      if (comp && (c == kPill || c == kCream || c == kWhite) && comp->n >= 12) {
        const float bw = static_cast<float>(comp->x1 - comp->x0), bh = static_cast<float>(comp->y1 - comp->y0);
        const float miss = bw * bh - static_cast<float>(comp->n);
        float r = std::sqrt(std::max(0.f, miss) / (4.f * 0.2146f));
        r = std::min(r, std::min(bw, bh) * 0.5f);
        const float x0 = static_cast<float>(comp->x0), y0 = static_cast<float>(comp->y0), x1 = static_cast<float>(comp->x1),
                    y1 = static_cast<float>(comp->y1);
        int32_t off = 0;
        for (int32_t y = comp->y0; y < comp->y1; ++y)
          for (int32_t x = comp->x0; x < comp->x1; ++x)
            off += in(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) !=
                   in_rrect(x + 0.5f, y + 0.5f, x0, y0, x1, y1, r);
        if (off * 100 <= comp->n) {
          UiPrim pr;
          pr.kind = kUiRRect;
          // a box cut by the cell's edge continues past it
          pr.x0 = x0 - (comp->x0 == 0 ? 40.f : 0.f);
          pr.y0 = y0 - (comp->y0 == 0 ? 40.f : 0.f);
          pr.x1 = x1 + (comp->x1 == w ? 40.f : 0.f);
          pr.y1 = y1 + (comp->y1 == h ? 40.f : 0.f);
          for (int q = 0; q < 4; ++q) pr.rx[q] = pr.ry[q] = r;
          out.elems.push_back(make(px, pr));
          return;
        }
      }
      // one pixel into the regions drawn after this one
      for (int32_t j : px) {
        const int32_t x = j % w, y = j / w;
        for (int32_t dy = -1; dy <= 1; ++dy)
          for (int32_t dx = -1; dx <= 1; ++dx) {
            const int32_t xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            const size_t q = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
            if (rank[q] > k + 1 && owner[q] != own) {
              owner[q] = own;
              bx0 = std::min(bx0, xx);
              by0 = std::min(by0, yy);
              bx1 = std::max(bx1, xx + 1);
              by1 = std::max(by1, yy + 1);
            }
          }
      }
      out.elems.push_back(make(px, field_in_box(in, nullptr, w, h, bx0, by0, bx1, by1)));
    };
    for (const Comp &comp : components(lab, w, h, c)) {
      if (comp.n >= 64) emit(comp.px, &comp);
      else small.insert(small.end(), comp.px.begin(), comp.px.end());
    }
    if (!small.empty()) emit(small, nullptr);
  }
  return true;
}
}  // namespace

bool ui_cell_scene(const uint8_t *colour, const uint16_t *pal, const uint8_t *mask, int32_t w, int32_t h, const UiTheme &t,
                   UiScene &out, std::vector<uint8_t> &keep, bool border_only) {
  if (t.art_colours > 0.5f) return cell_scene_art(colour, pal, mask, w, h, t, out, keep, border_only);   // P40D: the game theme
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
  keep_pictures(cls, outside, w, h, keep);
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
  if (border_only) {   // P40C: the panel's shape only; its inside (farther than 5 pixels from the edge) stays the art
    std::vector<uint8_t> outer(n, 0);
    for (size_t i = 0; i < n; ++i) outer[i] = panel[i] ? 0 : 1;
    std::vector<float> dist(n, 1e9f);
    {   // the distance (city block, two passes) from every panel pixel to the nearest non-panel pixel
      for (int32_t y = 0; y < h; ++y)
        for (int32_t x = 0; x < w; ++x) {
          const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
          if (outer[i]) {
            dist[i] = 0;
            continue;
          }
          if (x == 0 || y == 0) dist[i] = 1;
          if (x > 0) dist[i] = std::min(dist[i], dist[i - 1] + 1);
          if (y > 0) dist[i] = std::min(dist[i], dist[i - static_cast<size_t>(w)] + 1);
        }
      for (int32_t y = h - 1; y >= 0; --y)
        for (int32_t x = w - 1; x >= 0; --x) {
          const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
          if (x == w - 1 || y == h - 1) dist[i] = std::min(dist[i], 1.f);
          if (x + 1 < w) dist[i] = std::min(dist[i], dist[i + 1] + 1);
          if (y + 1 < h) dist[i] = std::min(dist[i], dist[i + static_cast<size_t>(w)] + 1);
        }
    }
    for (size_t i = 0; i < n; ++i)
      if (cls[i] != kNone && (dist[i] > 5.f || (panel[i] && cls[i] != kFrame))) keep[i] = 1;
    keep_as_picture(colour, pal, mask, w, h, keep, nullptr, out);   // P40E
    return true;
  }
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
  keep_as_picture(colour, pal, mask, w, h, keep, nullptr, out);   // P40E: the pictures over everything
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
const CellScene &cell_scene(Gfx &g, uint32_t cs, uint32_t ms, const Sprite8 &s, const Sprite8 &m, int32_t theme,
                            bool border_only = false) {
  static std::map<std::tuple<const Gfx *, uint32_t, uint32_t, int32_t, size_t, bool>, CellScene> cache;
  const auto key = std::make_tuple(static_cast<const Gfx *>(&g), cs, ms, theme, s.px.size(), border_only);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  CellScene c;
  std::vector<uint8_t> mk(s.px.size(), 0xff);
  for (size_t i = 0; i < mk.size() && i < m.px.size(); ++i) mk[i] = m.px[i];
  c.ok = ui_cell_scene(s.px.data(), s.pal.data(), mk.data(), s.w, s.h, ui_theme(theme), c.scene, c.keep, border_only);
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
    e.tex_x0 += dx;
    e.tex_y0 += dy;
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
  // P41G (gfx_uvgame.h): pieces with approved records: the original body draws the frame, each piece's Surface_stretchBlit drawn from
  // its record (a piece of the approved frame, clipped to the piece) instead of P40's assembled cell scene
  if (SG_NEGCTL(g_negctl_uvgame) != 3) {
    std::vector<int32_t> cuts;
    for (int k = 0; k < 8; ++k) {
      uvg_match(*ps[k], cuts);
      if (!cuts.empty()) return false;
    }
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
      if (b.c.keep[o] != 2) continue;   // (P40E: only negative control 4 leaves pixels to blit after the run)
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

bool ui_cell_pair_draw(Gfx &g, Ctx &ctx, uint32_t cs, uint32_t ms, int32_t x, int32_t y, int32_t theme, bool border_only) {
  auto si = g.sprites.find(cs), mi = g.sprites.find(ms);
  if (si == g.sprites.end() || mi == g.sprites.end()) return false;
  const CellScene &c = cell_scene(g, cs, ms, si->second, mi->second, theme, border_only);
  if (!c.ok) return false;
  g.ensure_root(ctx);
  ui_draw_scene(g, ctx, moved(c.scene, static_cast<float>(x), static_cast<float>(y)));
  // the pixels the scene does not draw, from the art as Surface_stretchBlit draws them (copy or blend by the mask)
  const Sprite8 &s = si->second, &m = mi->second;
  Surface16 &d = g.root;
  for (int32_t yy = std::max(y, 0); yy < std::min(y + s.h, d.h); ++yy)
    for (int32_t xx = std::max(x, 0); xx < std::min(x + s.w, d.w); ++xx) {
      const size_t o = static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x);
      if (c.keep[o] != 2 || o >= m.px.size()) continue;   // (P40E: negctl 4 only)
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
bool ui_cell_keyed_draw(Gfx &g, Ctx &ctx, uint32_t cs, int32_t px, int32_t py, int32_t theme) {
  auto si = g.sprites.find(cs);
  if (si == g.sprites.end()) return false;
  const Sprite8 &sp = si->second;
  Sprite8 mask;   // a mask cell for the engine: 0 (opaque) where draw16 draws, 0xff where it skips
  mask.w = sp.w;
  mask.h = sp.h;
  mask.px.resize(sp.px.size());
  for (size_t i = 0; i < sp.px.size(); ++i) mask.px[i] = sp.px[i] >= 0xfe ? 0xff : 0;
  const CellScene &c = cell_scene(g, cs, 0, sp, mask, theme);
  if (!c.ok) return false;
  g.ensure_root(ctx);
  const int32_t x = add32(sp.left, px), y = add32(sp.top, py);
  ui_draw_scene(g, ctx, moved(c.scene, static_cast<float>(x), static_cast<float>(y)));
  Surface16 &d = g.root;   // the kept pixels (the marbled ground, the icons) as draw16 draws them
  for (int32_t yy = std::max(y, 0); yy < std::min(y + sp.h, d.h); ++yy)
    for (int32_t xx = std::max(x, 0); xx < std::min(x + sp.w, d.w); ++xx) {
      const size_t o = static_cast<size_t>(yy - y) * static_cast<size_t>(sp.w) + static_cast<size_t>(xx - x);
      if (c.keep[o] != 2 || sp.px[o] >= 0xfe) continue;   // (P40E: negctl 4 only)
      uint16_t &wd = d.px[static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx)];
      wd = sp.pal[sp.px[o]];
      hud_mark(&wd);
    }
  return true;
}

// gfx.cpp gfx_drop: true when the drop was drawn here
extern int g_negctl_uvwide;   // gfx_uvwide.cpp (P41W)

// P41W (gfx_ui.h): gfx.cpp b_surface_blit3's first step with a UI theme on (every path to a full-screen copy reaches that body: the
// menu's MV_blitFull through gfx_drop, the boot screens' Surface_blitThunk through the body registry)
bool ui_wide_backdrop(Gfx &g, Ctx &ctx, const uint32_t *a) {
  if (g.ui_pilot == kUiPilotOff || a[1] != g.root_va(ctx)) return false;
  const bool full = a[2] == 0 && a[3] == 0 && a[4] == 800 && a[5] == 600 && a[6] == 0 && a[7] == 0 && a[8] == 800 && a[9] == 600;
  if (!full) return false;
  g.uw_screen = -1;   // any other full-screen copy ends the backdrop
  auto it = g.images.find(a[0]);
  const int32_t k = it != g.images.end() ? uw_screen_by_bg(it->second.name) : -1;
  if (k < 0 || SG_NEGCTL(g_negctl_uvwide) == 4 || !uw_prepare(ctx, k)) return false;
  g.ensure_root(ctx);
  const auto f = uw_frame555(k);
  if (!f || f->size() != g.root.px.size()) return false;
  g.root.px = *f;
  g.note_root_write(0, 0, 800, 600);
  g.uw_screen = k;
  // every run on the root before this copy is covered by it: dropped (a run of the screen before can match the new backdrop's words
  // by chance and be kept by the peel: the title's Sandbox well, cream in RGB555 as the theme table, showed over it, 2026-11-25)
  std::vector<Gfx::TextCapture::Run> &live = g.text.live;
  live.erase(std::remove_if(live.begin(), live.end(), [](const Gfx::TextCapture::Run &r) { return r.info.target == 0; }), live.end());
  return true;
}

// P41G round 6 (gfx_uvwide.h; log/roadmap/P41/portgame/notes.md): an in-game full-screen report drawn wide. The routing map's redraw
// draws its bottom sheet at (0, 253) and then the mode's top sheet at (0, 0), Buy Land its one full-screen sheet at (0, 0): an
// Image_draw of a wide screen's own sheet ("bg") from its corner at (0, 0) writes the screen's backdrop over the root and marks the
// frame, as ui_wide_backdrop does for a front-end screen's full-screen copy; the route bottom's draw writes the backdrop's pixels over
// the rectangle it covers (from the route screen shown, else the course mode's; the top drawn next turns the pixels still holding
// that screen's backdrop into its own mode's). Both write only their sheet's rectangle, so nothing the game drew elsewhere is lost. The
// game's later draws (the map, the buttons' states, the texts) go over it as before. Negative control uvwide 4: off.
bool ui_wide_report(Gfx &g, Ctx &ctx, const uint32_t *a) {
  if (g.ui_pilot == kUiPilotOff || a[1] != g.root_va(ctx) || a[4] != 0 || SG_NEGCTL(g_negctl_uvwide) == 4) return false;
  auto sp = g.sprites.find(a[0]);
  if (sp == g.sprites.end()) return false;
  const Sprite8 &s = sp->second;
  // a panel: a cut of the sheet's whole width drawn at its own place in the sheet (the buttons' cuts of the same sheets are not)
  if (s.sx != 0 || s.sheet.empty() || s.cut_w != 800 || i32(a[2]) != s.sx || i32(a[3]) != s.sy) return false;
  static const std::string kRouteBottom = "interface/infoscreens/route screens_bottom.pcx";
  if (s.sheet == kRouteBottom) {
    int32_t k = g.uw_screen;
    if (k < 0 || std::strncmp(uw_screen_name(k), "route", 5) != 0) k = uw_screen_find("route");
    if (k < 0 || !uw_prepare(ctx, k)) return false;
    const auto f = uw_frame555(k);
    g.ensure_root(ctx);
    if (!f || f->size() != g.root.px.size()) return false;
    const int32_t x0 = std::max(0, i32(a[2]) + s.left), y0 = std::max(0, i32(a[3]) + s.top);
    const int32_t x1 = std::min(g.root.w, i32(a[2]) + s.left + s.w), y1 = std::min(g.root.h, i32(a[3]) + s.top + s.h);
    for (int32_t y = y0; y < y1; ++y)
      for (int32_t x = x0; x < x1; ++x) {
        const size_t j = static_cast<size_t>(y) * static_cast<size_t>(g.root.w) + static_cast<size_t>(x);
        g.root.px[j] = (*f)[j];
      }
    if (x1 > x0 && y1 > y0) g.note_root_write(x0, y0, x1 - x0, y1 - y0);
    g.uw_screen = k;
    return true;
  }
  if (s.sy != 0) return false;
  const int32_t k = uw_screen_by_bg(s.sheet);
  if (k < 0 || std::strncmp(uw_screen_name(k), "route", 5) != 0 && std::strcmp(uw_screen_name(k), "buyland") != 0) return false;
  if (!uw_prepare(ctx, k)) return false;
  g.ensure_root(ctx);
  const auto f = uw_frame555(k);
  if (!f || f->size() != g.root.px.size()) return false;
  // the backdrop over the rectangle the sheet covers; outside it, a pixel still holding the previous wide screen's backdrop word (the
  // route bottom restored from another mode, the screen shown before) takes this screen's word, and what the game drew stays
  const int32_t x1 = std::min(g.root.w, s.left + s.w), y1 = std::min(g.root.h, s.top + s.h);
  const int32_t x0 = std::max(0, s.left), y0 = std::max(0, s.top);
  const int32_t old = g.uw_screen;
  const auto fo = old >= 0 && old != k ? uw_frame555(old) : nullptr;
  for (int32_t y = 0; y < g.root.h; ++y)
    for (int32_t x = 0; x < g.root.w; ++x) {
      const size_t j = static_cast<size_t>(y) * static_cast<size_t>(g.root.w) + static_cast<size_t>(x);
      if (x >= x0 && x < x1 && y >= y0 && y < y1) g.root.px[j] = (*f)[j];
      else if (fo && fo->size() == f->size() && g.root.px[j] == (*fo)[j]) g.root.px[j] = (*f)[j];
    }
  g.note_root_write(0, 0, 800, 600);
  g.uw_screen = k;
  // every run inside the rectangle is covered by it (ui_wide_backdrop drops every root run for a full-screen copy)
  std::vector<Gfx::TextCapture::Run> &live = g.text.live;
  live.erase(std::remove_if(live.begin(), live.end(),
                            [&](const Gfx::TextCapture::Run &r) {
                              return r.info.target == 0 && r.info.x0 >= x0 && r.info.x1 <= x1 && r.info.y0 >= y0 && r.info.y1 <= y1;
                            }),
             live.end());
  return true;
}

// P41W (gfx_ui.h): an Image_draw (args sprite, surface, x, y, mask) of a cut that is one of the shown wide screen's overlays
bool ui_wide_overlay(Gfx &g, Ctx &ctx, const uint32_t *a) {
  if (g.ui_pilot == kUiPilotOff || g.uw_screen < 0 || a[1] != g.root_va(ctx) || a[4] != 0) return false;
  auto sp = g.sprites.find(a[0]);
  if (sp == g.sprites.end()) return false;
  const int32_t k = g.uw_screen, n = uw_overlay_count(k);
  for (int32_t i = 0; i < n; ++i) {
    UwOverlayInfo o;
    if (!uw_overlay_info(k, i, o) || o.kind < 1) continue;
    uint32_t base = 0;
    if (o.key_kind == 1) base = state_va(ctx, &GameState::g_spriteRef_53b7e0);
    else if (o.key_kind == 2) base = gfx_local_key(0);
    else if (o.key_kind == 3) base = state_va(ctx, &GameState::g_sprites59df20);
    else continue;
    if (a[0] != base + o.key_off) continue;
    const int32_t dx = i32(a[2]) - o.ox, dy = i32(a[3]) - o.oy;
    if (o.kind == 1) return ui_draw_uvec(g, ctx, uw_pic_run_text(k, o.pic, dx, dy));
    if (o.kind == 2) return ui_draw_uvec(g, ctx, uw_ovl_run_text(k, i, dx, dy));
    // 3: the backdrop's 1x pixels over the rectangle the cut would cover (its trims and kept size at the draw point)
    const auto f = uw_frame555(k);
    if (!f) return false;
    g.ensure_root(ctx);
    const Sprite8 &s = sp->second;
    const int32_t x0 = std::max(0, i32(a[2]) + s.left), y0 = std::max(0, i32(a[3]) + s.top);
    const int32_t x1 = std::min(g.root.w, i32(a[2]) + s.left + s.w), y1 = std::min(g.root.h, i32(a[3]) + s.top + s.h);
    for (int32_t y = y0; y < y1; ++y)
      for (int32_t x = x0; x < x1; ++x) {
        const size_t j = static_cast<size_t>(y) * static_cast<size_t>(g.root.w) + static_cast<size_t>(x);
        if (j < f->size()) g.root.px[j] = (*f)[j];
      }
    return true;
  }
  return false;
}

bool ui_swap(Gfx &g, Ctx &ctx, const char *name, const uint32_t *a, int n) {
  const int32_t theme = g.ui_pilot;
  if (theme == kUiPilotOff) return false;
  g.ensure_root(ctx);
  if (!std::strcmp(name, "Surface_stretchBlit") && n >= 6 && a[2] == g.root_va(ctx) && a[5] == 0) {
    // P41G (gfx_uvgame.h): a cut with approved element scenes is drawn from them, before P40's cell engine (the HUD panels below)
    auto si = g.sprites.find(a[0]);
    if (si != g.sprites.end() && uvg_draw(g, ctx, si->second, i32(a[3]), i32(a[4]))) return true;
  }
  if (!std::strcmp(name, "Surface_stretchBlit") && n >= 6) {
    // the loadInfoScreenArt pairs (mask = colour + 0x2c), and the frame-local pairs of the End of Year strips (reports.cpp: top 0x5500,
    // bottom 0x5700, line 0x5900) and the Shortcuts page (routing.cpp: 0x5100), whose masks are the colour key + 0x100
    const bool local = (a[0] == gfx_local_key(0x5500) || a[0] == gfx_local_key(0x5700) || a[0] == gfx_local_key(0x5900) ||
                        a[0] == gfx_local_key(0x5100)) && a[1] == a[0] + 0x100u;
    // P40C: the HUD panels (loadInterfaceArt: g_sprites541a18 cell k over g_sprites58b390 cell k, k 1..10: the palettes (the radial
    // menu's base, cell 0, is a painted wheel: kept as art), the member / action / match / employee / elevation panels, the golfer info panel and its story band),
    // border-only
    const uint32_t hud_c = state_va(ctx, &GameState::g_sprites541a18), hud_m = state_va(ctx, &GameState::g_sprites58b390);
    const bool hud = a[0] >= hud_c + 0x2cu && a[0] < hud_c + 11u * 0x2cu && (a[0] - hud_c) % 0x2cu == 0 && a[1] == hud_m + (a[0] - hud_c);
    if (a[2] != g.root_va(ctx) || a[5] != 0 || !(hud || local || (a[1] == a[0] + 0x2cu && report_cell(ctx, a[0])))) return false;
    return ui_cell_pair_draw(g, ctx, a[0], a[1], i32(a[3]), i32(a[4]), theme, hud);
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
  if (!std::strcmp(name, "Image_draw") && n >= 5 && ui_wide_report(g, ctx, a)) return true;   // P41G round 6: the wide reports
  if (!std::strcmp(name, "Image_draw") && n >= 4 && ui_wide_overlay(g, ctx, a)) return true;   // P41W (gfx_ui.h)
  if (!std::strcmp(name, "Image_draw") && n >= 4 && a[1] == g.root_va(ctx) && (n < 5 || a[4] == 0)) {
    // P41G (gfx_uvgame.h): a cut with approved element scenes drawn from them (the kept pixel (0, 0) at the draw point plus the trims, as
    // Image_draw draws at scale 1), before P40's routing-map panels and OK buttons below
    auto si = g.sprites.find(a[0]);
    if (si != g.sprites.end() && uvg_draw(g, ctx, si->second, add32(si->second.left, i32(a[2])), add32(si->second.top, i32(a[3]))))
      return true;
  }
  if (!std::strcmp(name, "Surface_drawScaled") && n >= 8 && a[4] == 1 && a[5] == 1 && a[6] == 1 && a[7] == 0) {   // P41W: the editor's cells
    const uint32_t d[5] = {a[0], a[1], a[2], a[3], 0};
    if (ui_wide_overlay(g, ctx, d)) return true;
  }
  if (!std::strcmp(name, "Image_draw") && n >= 4 && a[1] == g.root_va(ctx) && (n < 5 || a[4] == 0)) {
    // P41 (gfx_uivec.h, log/roadmap/P41/pilot/notes.md): the title menu's buttons (showTitleScreen's cuts in g_spriteRef_53b7e0, slot
    // 2 b + hovered; the menu and the boot draw them at the cut's own place) as the approved uivec scene of that button and state,
    // moved by the draw's point minus the cut's origin. Only a cut the backend holds (as the original body, which draws nothing
    // without one); the centre picture (slots 12 / 13) stays art (log/roadmap/P41/pilot/notes.md: why its ellipse is not drawn).
    const uint32_t tb = state_va(ctx, &GameState::g_spriteRef_53b7e0);
    if (a[0] >= tb && a[0] < tb + 12u * 0x2cu && (a[0] - tb) % 0x2cu == 0 && g.sprites.count(a[0]) != 0) {
      int32_t cx = 0, cy = 0;
      const int32_t k = uv_title_slot_scene(static_cast<int32_t>((a[0] - tb) / 0x2cu), cx, cy);
      if (k >= 0 && ui_draw_uvec(g, ctx, uv_run_text(k, i32(a[2]) - cx, i32(a[3]) - cy))) return true;
    }
  }
  if (!std::strcmp(name, "Image_draw") && n >= 4 && a[1] == g.root_va(ctx)) {
    // P40B batch 3: the routing map's panels (route screens: the bottom c0, the four tops c1..c4) and the buy-land screen (+0x2ec),
    // drawn by Image_draw without a mask (indexes 0xfe / 0xff transparent, draw16): the cell's scene at (left + x, top + y)
    const uint32_t rs = state_va(ctx, &GameState::g_spriteRef_8224c8);
    if (a[0] == rs || a[0] == rs + 0x2cu || a[0] == rs + 0x58u || a[0] == rs + 0x84u || a[0] == rs + 0xb0u || a[0] == rs + 0x2ecu) {
      if (ui_cell_keyed_draw(g, ctx, a[0], i32(a[2]), i32(a[3]), theme)) return true;
    }
  }
  if (!std::strcmp(name, "Image_draw") && n >= 4) {
    const uint32_t ok = state_va(ctx, &GameState::g_spriteRef_820f40);
    if (a[1] != g.root_va(ctx) || a[0] < ok || a[0] >= ok + 5u * 0x2cu || (a[0] - ok) % 0x2cu != 0) return false;
    const bool hover = a[0] == ok + 0x2cu;
    if (ui_theme(theme).art_colours > 0.5f && ui_cell_keyed_draw(g, ctx, a[0], i32(a[2]), i32(a[3]), theme)) return true;   // P40D
    ui_draw_scene(g, ctx, ui_button_scene(ui_theme(theme), i32(a[2]), i32(a[3]), hover));
    return true;
  }
  return false;
}

}  // namespace portsim
