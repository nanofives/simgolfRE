// Body family `prim_fill` of the graphics backend (roadmap S2.4; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// solid fills, the panel tiler, the display's blit scale and the convex polygon fill the routing map and the load browser
// draw through. Specifications: the C3 / C4 hook sources named at each body (shim/src/re/*.cpp) and the matched jgld
// routines (re/match/jgld_*.cpp).
//
//   fillWindowRect(this, x, y, w, h, colour)
//       c3db.cpp: Win_fillRect on this + 0x274 (the Surface part of every Window class) with the same five arguments.
//   Win_fillRect(this, x, y, w, h, colour)
//       c3db.cpp: Widget_fillArea(this, {x, y, x + w, y + h}, colour) (c3ab.cpp: the handle's slot +0x44 = jgld
//       Surface::fillRectDispatch, re/match/jgld_surface.cpp), which for a 16-bit surface is Surface::fillRect16
//       (re/match/jgld_surface16.cpp): a direct colour (bit 31) keeps its low 16 bits; the rectangle is intersected with
//       the clip rectangle (IntersectRect: nothing for an empty result) and filled with the word. Modelled: the root
//       surface (clip = the whole surface, allocDrawBuffer's setClip) and a direct colour; a palette index is reported.
//   blitPanel(x, y, w, h, style)
//       c3da.cpp: 16 x 16 tiles from (x, y) while xi < x + w (columns) and yi < y + h (rows), each Surface_blit2 of one
//       Surface onto the target with the shade table g_color16Lut: style 0 always the tile g_sprites5a53c8 cell 9 (cut by
//       loadInterfaceArt's TransPopups load, gfx.cpp loadInterfaceArt.images), any other style one of the nine
//       g_surfaces541580 cells cx + 3 cy (cx 0 in the first column, 2 when xi + 16 >= x + w, else 1; cy the same for
//       rows; the last-tile test wins). Surface_blit2 adds the tile's trims and draws through Sprite::drawG
//       (gfxb::draw_g16, the shared helper the location screen's frames already use).
//   Display::setScale(nx, ny, den)
//       the display's slot +0x9c (re/names/jgld_3.tsv: Display::setScale stores g_blitScaleX / Y / Den, the scale the
//       sprite blitters read). The exe calls it through the display pointer (the routing map's `display slot +0x9c`
//       calls, c3et.cpp; drawQueuedSprite, c3ei.cpp); the port names the call by this sub-body name. The backend keeps the
//       three words in this family's state (gfxf::blit_scale); a body that draws a sprite reads them.
//   gfxf::fill_convex_poly16(dst, pts, n, colour)
//       fillConvexPoly16 (c3db.cpp) with its edge walker polyEdgeStep (c3x.cpp) / polyEdgeAdvance and span filler
//       fillSpanDown (c3t.cpp), on a 16-bit surface whose clip is the whole surface: the clip globals are right - 1 and
//       bottom - 1, so spans stop before column w - 1 and the scan before row h - 1. Used by the routing map's diamonds
//       (gfx_routing.cpp, drawRoutingPoly = fillConvexPoly on the root window's surface).
//
// Negative controls (g_negctl_gfx_prim_fill, sg_negctl_set "gfx.family.prim_fill"):
//   1 Win_fillRect fills one row less (bottom - 1)          2 blitPanel's style 0 takes the nine-cell tiles instead
//   3 the polygon fill draws the span's last pixel too (x1 inclusive)
//   4 Display::setScale keeps the scale at (1, 1, 1)
#include <algorithm>
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/gfx_scene.h"   // P23D fix 5: gfxs::bubble_fill

namespace portsim {

int g_negctl_gfx_prim_fill = 0;

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_prim_fill); }

struct FillState {
  int32_t scale[3] = {1, 1, 1};   // Display::setScale's g_blitScaleX / Y / Den
};
FillState &st(Gfx &g) { return g.state_of<FillState>("prim_fill"); }
}  // namespace

namespace gfxf {

// the display's blit scale (Display::setScale), (1, 1, 1) until a call changes it
const int32_t *blit_scale(Gfx &g) { return st(g).scale; }

// jgld Surface::fillRect16 with a direct colour on a surface whose clip is the whole surface
void fill_rect16(Surface16 &d, int32_t l, int32_t t, int32_t r, int32_t b, uint16_t c) {
  l = std::max(l, 0);
  t = std::max(t, 0);
  r = std::min(r, d.w);
  b = std::min(b, d.h);
  if (l >= r || t >= b) return;   // IntersectRect: an empty intersection fills nothing
  for (int32_t y = t; y < b; ++y) {
    std::fill(d.px.begin() + static_cast<ptrdiff_t>(y) * d.w + l, d.px.begin() + static_cast<ptrdiff_t>(y) * d.w + r, c);
    hud_mark_span(d.px.data() + static_cast<size_t>(y) * static_cast<size_t>(d.w) + static_cast<size_t>(l), static_cast<size_t>(r - l));  // P10C fix 2
  }
}

// fillConvexPoly16 on `d` (clip = the whole surface): pts = n {x, y} pairs
void fill_convex_poly16(Surface16 &d, const int32_t *pts, int n, uint16_t c) {
  const int32_t clip_r1 = d.w - 1, clip_b1 = d.h - 1, clip_l = 0, clip_t = 0;
  if (n <= 0) return;
  // the topmost vertex (first minimum) and the bottom one (first maximum) (scanPolygon, c3db.cpp)
  int32_t min_y = 0x7fff, max_y = -0x7ffd, start = 0, end = 0;
  for (int i = 0; i < n; ++i) {
    const int32_t y = pts[2 * i + 1];
    if (y < min_y) {
      min_y = y;
      start = i;
    }
    if (y > max_y) {
      max_y = y;
      end = i;
    }
  }
  if (min_y >= max_y) return;
  struct Edge {
    int dir = 0, rows = 0, endv = 0, x = 0, step = 0, sign = 0, err = 0, err_step = 0, err_reset = 0;
  };
  // polyEdgeStep (c3x.cpp)
  auto edge_step = [&](Edge &e, int idx) -> bool {
    if (idx == end) return false;
    for (;;) {
      int v = idx + e.dir;
      if (v > n - 1) v = 0;
      else if (v < 0) v = n - 1;
      const int32_t dy = sub32(pts[2 * v + 1], pts[2 * idx + 1]);
      e.rows = dy;
      if (dy == 0) {
        idx = v;
        if (v == end) return false;
        continue;
      }
      if (dy < 0) return false;
      e.endv = v;
      e.x = pts[2 * idx];
      const int32_t dx = sub32(pts[2 * v], pts[2 * idx]);
      if (dx >= 0) {
        e.sign = 1;
        e.step = dx / dy;
        e.err = 0;
        e.err_step = dx % dy;
      } else {
        const int32_t adx = -dx;
        e.sign = -1;
        e.err = 1 - dy;
        e.step = -(adx / dy);
        e.err_step = adx % dy;
      }
      e.err_reset = dy;
      return true;
    }
  };
  // polyEdgeAdvance (c3t.cpp)
  auto edge_advance = [&](Edge &e) -> bool {
    e.rows -= 1;
    if (e.rows == 0) return edge_step(e, e.endv);
    e.x = add32(e.x, e.step);
    e.err = add32(e.err, e.err_step);
    if (e.err > 0) {
      e.x = add32(e.x, e.sign);
      e.err = sub32(e.err, e.err_reset);
    }
    return true;
  };
  // fillSpanDown (c3t.cpp clipSpan): [x0, x1) clipped to [clip left, clip right - 1)
  auto span = [&](int32_t row, int32_t x0, int32_t x1) {
    if (mode() == 3) x1 = add32(x1, 1);
    if (x1 <= clip_l || x0 >= clip_r1 || x1 <= x0) return;
    if (x1 >= clip_r1) x1 = clip_r1;
    if (x0 < clip_l) x0 = clip_l;
    if (x1 <= x0) return;
    if (row < 0 || row >= d.h) return;
    std::fill(d.px.begin() + static_cast<ptrdiff_t>(row) * d.w + x0, d.px.begin() + static_cast<ptrdiff_t>(row) * d.w + x1, c);
    hud_mark_span(d.px.data() + static_cast<size_t>(row) * static_cast<size_t>(d.w) + static_cast<size_t>(x0), static_cast<size_t>(x1 - x0));  // P10C fix 2
  };
  int32_t row = min_y;
  Edge e1, e2;
  e1.dir = -1;
  if (!edge_step(e1, start)) return;
  e2.dir = 1;
  if (!edge_step(e2, start)) return;
  if (row >= clip_b1) return;
  for (;;) {
    if (row >= clip_t) {
      if (e1.x < e2.x) span(row, e1.x, e2.x);
      else span(row, e2.x, e1.x);
    }
    if (!edge_advance(e1)) return;
    if (!edge_advance(e2)) return;
    ++row;
    if (row >= clip_b1) return;
  }
}

// blitPanel(x, y, w, h, style) onto the root surface
void blit_panel(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t w, int32_t h, int32_t style) {
  g.ensure_root(ctx);
  const uint32_t nine = state_va(ctx, &GameState::g_surfaces541580);
  const uint32_t plain = state_va(ctx, &GameState::g_sprites5a53c8) + 0x2Cu * 9;
  const int32_t xe = add32(x, w), ye = add32(y, h);
  for (int32_t xi = x; xi < xe; xi += 16)
    for (int32_t yi = y; yi < ye; yi += 16) {
      uint32_t key = plain;
      if (style != 0 || mode() == 2) {
        int32_t cx = xi == x ? 0 : 1, cy = yi == y ? 0 : 1;
        if (add32(xi, 16) >= xe) cx = 2;
        if (add32(yi, 16) >= ye) cy = 2;
        key = nine + 0x2Cu * static_cast<uint32_t>(cx + 3 * cy);
      }
      auto it = g.sprites.find(key);
      if (it == g.sprites.end()) {
        g.unsup("blitPanel: a tile the backend has no sprite for");
        continue;
      }
      gfxb::draw_g16(g, it->second, g.root, add32(xi, it->second.left), add32(yi, it->second.top));
    }
}

}  // namespace gfxf

namespace {

// Win_fillRect(this, x, y, w, h, colour) on the surface `surf`
void win_fill(Gfx &g, Ctx &ctx, uint32_t surf, const uint32_t *a, const char *who) {
  g.ensure_root(ctx);
  if (surf != g.root_va(ctx)) {
    g.unsup(std::string(who) + ": a surface other than the root window's");
    return;
  }
  const uint32_t c = a[4];
  if (!(c & 0x80000000u)) {
    g.unsup(std::string(who) + ": a palette colour index");
    return;
  }
  const int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]);
  int32_t b = add32(y, static_cast<int32_t>(a[3]));
  if (mode() == 1) b = sub32(b, 1);
  const int32_t r = add32(x, static_cast<int32_t>(a[2]));
  // P23E fix 6 (gfx.h kGfxWideMarks): the terrain tool's dots (P4_holeGrid, inside its GfxWideLineScope) are recorded as still
  // filled rectangles with the root pixels they change, as terrain_draw_line records its lines (records only)
  Gfx::HudCapture &hc = g.hud;
  const bool rec = hc.lines_on && (hc.wide_lines & hc.wide_scope & kGfxWideMarks) != 0 && g.root.w == 800 && g.root.h == 600 &&
                   g.root.px.size() == static_cast<size_t>(800 * 600);
  const int32_t cx0 = std::max(0, x), cy0 = std::max(0, y), cx1 = std::min(g.root.w, r), cy1 = std::min(g.root.h, b);
  std::vector<uint16_t> before;
  if (rec && cx1 > cx0 && cy1 > cy0)
    for (int32_t yy = cy0; yy < cy1; ++yy)
      before.insert(before.end(), g.root.px.begin() + yy * g.root.w + cx0, g.root.px.begin() + yy * g.root.w + cx1);
  gfxf::fill_rect16(g.root, x, y, r, b, static_cast<uint16_t>(c & 0xffff));
  if (hc.ground_depth > 0 && r > x && b > y) hc.ground.push_back({x, y, r, b, static_cast<uint16_t>(c & 0xffff)});   // P23D fix 6
  if (hc.bubble_depth > 0) gfxs::bubble_fill(g, ctx, x, y, r, b, static_cast<uint16_t>(c & 0xffff));   // P23D fix 5
  if (!rec) return;
  if (hc.line800.size() != g.root.px.size()) {
    hc.line800.assign(g.root.px.size(), 0);
    hc.line_word.assign(g.root.px.size(), 0);
  }
  size_t k = 0;
  if (cx1 > cx0 && cy1 > cy0)
    for (int32_t yy = cy0; yy < cy1; ++yy)
      for (int32_t xx = cx0; xx < cx1; ++xx, ++k) {
        const size_t i = static_cast<size_t>(yy) * static_cast<size_t>(g.root.w) + static_cast<size_t>(xx);
        if (before[k] != g.root.px[i]) {
          hc.line800[i] = 1;
          hc.line_word[i] = g.root.px[i];
        }
      }
  GfxHudLine l;
  l.x1 = x;
  l.y1 = y;
  l.x2 = r;
  l.y2 = b;
  l.colour = c;
  l.alpha = 10;
  l.still = true;
  l.fill = true;
  hc.lines.push_back(l);
}

void b_fill_window_rect(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  win_fill(g, ctx, a[0] + 0x274, a + 1, "fillWindowRect");
}
void b_win_fill_rect(Gfx &g, Ctx &ctx, const uint32_t *a, int) { win_fill(g, ctx, a[0], a + 1, "Win_fillRect"); }

void b_blit_panel(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  gfxf::blit_panel(g, ctx, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
                   static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]));
}

void b_set_scale(Gfx &g, Ctx &, const uint32_t *a, int) {
  if (mode() == 4) return;
  FillState &s = st(g);
  s.scale[0] = static_cast<int32_t>(a[0]);
  s.scale[1] = static_cast<int32_t>(a[1]);
  s.scale[2] = static_cast<int32_t>(a[2]);
}

const GfxBodyDef kBodies[] = {
    {"fillWindowRect", b_fill_window_rect},
    {"Win_fillRect", b_win_fill_rect},
    {"blitPanel", b_blit_panel},
    {"Display::setScale", b_set_scale},
};

}  // namespace

extern const GfxFamily kGfxFamily_prim_fill = {"prim_fill", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
