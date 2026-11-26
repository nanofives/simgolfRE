// P41 / P41W (portsim/sim/gfx_uivec.h, portsim/sim/gfx_uvwide.h): the uivec scene evaluator shared by the title buttons
// (gfx_uivec.cpp) and the wide front-end backdrops (gfx_uvwide.cpp). Every formula is re/tools/uivec/sdf.py's, in doubles, over the
// tables re/tools/uivec/port_export.py writes (uivec_title.inc, uivec_wide_<screen>.inc): a shape's signed distance (sd), a layer's
// paint (paint), and one item's layers composited over one pixel (composite_item: sdf.render's img * (1 - a) + colour * a, carried
// with the coverage so a premultiplied target and an opaque one both work).
#ifndef PORTSIM_SIM_GFX_UIVEC_EVAL_H
#define PORTSIM_SIM_GFX_UIVEC_EVAL_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace portsim {
namespace uveval {

enum UvKind : uint8_t { kCircle = 0, kEllipse, kPoly, kSeg, kHalf, kUnion, kInter, kSub, kGrow, kOutline, kStroke, kRRect, kArc,
                        // P41G: area kinds (sdf.EXTRA): hud_panels.limb (figs.py _limb), hud_panels.rrect4 (_rrect4), popups.sinter (hs3.py)
                        kLimb, kRRect4, kSInter,
                        // rm.smin (reports_maps parts.py _smin: a union with polynomial smooth-minimum fillets of size p[0])
                        kSMin,
                        // marks: superellipse (common.py _superellipse), pline (_pline), vline (fix2.py _vline, points resolved)
                        kSuper, kPLine, kVLine };
struct UvNode {
  uint8_t kind, cap, closed;  // cap: 0 butt, 1 flat, 2 round (a stroke)
  int32_t a, n;               // children (kids) / rings (rings) / points (pts pairs, a stroke)
  int32_t ns, ne;             // a stroke's segments cut by its first end's plane (from the start) and its last end's (from the end)
  double p[10];               // see port_export.py HEADER_TABLES
};
struct UvPaint {
  uint8_t kind;  // 0 flat, 1 bevel, 2 linear, 3 radial
  double rgb[3];
  double L;
  int32_t stops_a, nstops, coef_a;  // bevel coef: nstops x 5 terms x 3 channels; linear / radial: one colour per stop
  int32_t of;                       // a bevel's own shape (-1: the layer's)
  double g[4];                      // linear: a, b; radial: c, R; bevel: g[0] = 1 for the grid normal (below)
};
// P41W: a bevel on a shape other than a circle or an ellipse takes sdf.py's grid normal: np.gradient of the distance over the item's
// own pixel grid (central differences in pixel units, one-sided on the first and last column / row of the item's painted box), so the
// pixel's place in that box is needed
struct UvPix {
  int32_t px, py;          // the target pixel
  int32_t i0, i1, j0, j1;  // the item's painted box (sdf.render's, clipped to the target)
  double sx, sy, ox, oy;   // target pixel (x, y) shows the frame point ((x + 0.5 - ox) / sx, (y + 0.5 - oy) / sy)
};
struct UvLayer {
  int32_t shape, clip, paint;
  double alpha, soft;  // soft < 0: none
  int32_t falloff;
  double fcx, fcy, fr, fpow;
  int32_t fx_a, fx_n;  // P41G: the layer's effects in UvTab::fx (0, 0: none)
};
// P41G: a layer effect (sdf.py render_layer "fx"): kind 0 edge_light, 1 inner_shadow, 2 tone; v = w, pow, dir x / y, colour 1 (light /
// rgb), colour 2 (dark), k, a x / y, b x / y, add
struct UvFx {
  uint8_t kind;
  double v[16];
};
struct UvItem {
  double box[4];
  int32_t layer_a, nlayers;
};
struct UvTab {
  const UvNode *nodes;
  const int32_t *kids, *rings;
  const double *pts, *stops, *coef;
  const UvPaint *paints;
  const UvLayer *layers;
  const UvItem *items;
  const UvFx *fx = nullptr;   // P41G (null: no table set before P41G has effects)
};

constexpr double kDeg = 3.141592653589793 / 180.0;   // numpy's deg2rad factor
constexpr double kRad = 180.0 / 3.141592653589793;   // numpy's rad2deg factor

inline double clip01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
inline double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

// sdf.py _rot with the rotation's cosine and sine
inline void rot(double x, double y, double cx, double cy, double c, double s, double &px, double &py) {
  const double qx = x - cx, qy = y - cy;
  px = qx * c + qy * s;
  py = -qx * s + qy * c;
}

inline double sd_ellipse(double px, double py, double rx, double ry) {
  const double k0 = std::sqrt((px / rx) * (px / rx) + (py / ry) * (py / ry));
  const double k1 = std::sqrt((px / (rx * rx)) * (px / (rx * rx)) + (py / (ry * ry)) * (py / (ry * ry)));
  return k0 < 0.05 ? -std::min(rx, ry) : k0 * (k0 - 1.0) / std::max(k1, 1e-9);
}

inline double sd(const UvTab &T, int32_t id, double x, double y);

inline double sd_poly(const UvTab &T, const UvNode &S, double x, double y) {
  double d = 1e18;
  bool inside = false;
  for (int32_t r = 0; r < S.n; ++r) {
    const int32_t pa = T.rings[2 * (S.a + r)], pn = T.rings[2 * (S.a + r) + 1];
    const double *P = T.pts + 2 * pa;
    for (int32_t i = 0; i < pn; ++i) {
      const double ax = P[2 * i], ay = P[2 * i + 1];
      const int32_t j = (i + 1) % pn;
      const double bx = P[2 * j], by = P[2 * j + 1];
      const double ex = bx - ax, ey = by - ay;
      const double L = ex * ex + ey * ey;
      if (L < 1e-12) continue;
      const double wx = x - ax, wy = y - ay;
      const double u = clip01((wx * ex + wy * ey) / L);
      const double qx = wx - ex * u, qy = wy - ey * u;
      d = std::min(d, qx * qx + qy * qy);
      if (ey != 0) {
        const bool c = (ay <= y && by > y) || (by <= y && ay > y);
        if (c && x < ax + (y - ay) * ex / ey) inside = !inside;
      }
    }
  }
  return (inside ? -1.0 : 1.0) * std::sqrt(d);
}

inline double sd_stroke(const UvTab &T, const UvNode &S, double x, double y) {
  const double *P = T.pts + 2 * S.a;
  const int32_t np = S.n;
  const int32_t nseg = np - 1 + (S.closed ? 1 : 0);
  const double hw = S.p[0] / 2;
  // the planes across the two ends (sdf.py: cap butt / flat on an open stroke)
  const bool planes = S.cap <= 1 && !S.closed && np >= 2;
  double pl0 = 0, pl1 = 0;
  if (planes) {
    {
      const double px = P[0], py = P[1], ux = px - P[2], uy = py - P[3];
      const double n = std::max(std::hypot(ux, uy), 1e-9);
      pl0 = (x - px) * ux / n + (y - py) * uy / n;
    }
    {
      const double px = P[2 * (np - 1)], py = P[2 * (np - 1) + 1], ux = px - P[2 * (np - 2)], uy = py - P[2 * (np - 2) + 1];
      const double n = std::max(std::hypot(ux, uy), 1e-9);
      pl1 = (x - px) * ux / n + (y - py) * uy / n;
    }
  }
  double d = 1e18;
  for (int32_t i = 0; i < nseg; ++i) {
    const int32_t j = (i + 1) % np;
    const double ax = P[2 * i], ay = P[2 * i + 1], ex = P[2 * j] - ax, ey = P[2 * j + 1] - ay;
    const double wx = x - ax, wy = y - ay;
    const double u = clip01((wx * ex + wy * ey) / std::max(ex * ex + ey * ey, 1e-12));
    double di = std::hypot(wx - ex * u, wy - ey * u) - hw;
    if (planes) {
      if (i < S.ns) di = std::max(di, pl0);
      if (i >= nseg - S.ne) di = std::max(di, pl1);
    }
    d = std::min(d, di);
  }
  return d;
}

// P41G: figs.py _cone (iq's uneven capsule from a, radius r1, to b, radius r2) and _limb (n points, then their n widths as pairs)
inline double sd_cone(double x, double y, double ax, double ay, double bx, double by, double r1, double r2) {
  const double ex = bx - ax, ey = by - ay;
  const double h = std::max(std::hypot(ex, ey), 1e-9);
  const double ux = ex / h, uy = ey / h;
  const double px = x - ax, py = y - ay;
  const double v = px * ux + py * uy;
  const double u = std::abs(-px * uy + py * ux);
  if (std::abs(r1 - r2) >= h) return std::min(std::hypot(px, py) - r1, std::hypot(x - bx, y - by) - r2);
  const double s = (r1 - r2) / h, c = std::sqrt(1 - s * s);
  const double k = -s * u + c * v;
  if (k < 0) return std::hypot(u, v) - r1;
  if (k > c * h) return std::hypot(u, v - h) - r2;
  return u * c + v * s - r1;
}
inline double sd_limb(const UvTab &T, const UvNode &S, double x, double y) {
  const double *P = T.pts + 2 * S.a, *Wd = T.pts + 2 * (S.a + S.n);
  const int32_t n = S.n;
  if (n == 1) return std::hypot(x - P[0], y - P[1]) - Wd[0] / 2;
  double d = 1e18;
  for (int32_t k = 0; k + 1 < n; ++k) {
    double dk = sd_cone(x, y, P[2 * k], P[2 * k + 1], P[2 * k + 2], P[2 * k + 3], Wd[2 * k] / 2, Wd[2 * k + 2] / 2);
    if (S.cap == 1) {
      auto plane = [&](int32_t e, int32_t f) {
        const double ux = P[2 * e] - P[2 * f], uy = P[2 * e + 1] - P[2 * f + 1];
        const double nn = std::max(std::hypot(ux, uy), 1e-9);
        dk = std::max(dk, ((x - P[2 * e]) * ux + (y - P[2 * e + 1]) * uy) / nn);
      };
      if (k == 0) plane(0, 1);
      if (k == n - 2) plane(n - 1, n - 2);
    }
    d = std::min(d, dk);
  }
  return d;
}

// P41G: common.py _pline: an open polyline whose width runs from p[0] to p[1] along its length, round joins; cap 1 ("butt"): the
// segments starting within one (larger) width of the start are cut by the start's plane, those ending within it of the end by the end's
inline double sd_pline(const UvTab &T, const UvNode &S, double x, double y) {
  const double *P = T.pts + 2 * S.a;
  const int32_t n = S.n;
  double tot = 0;
  for (int32_t k = 0; k + 1 < n; ++k) tot += std::hypot(P[2 * k + 2] - P[2 * k], P[2 * k + 3] - P[2 * k + 1]);
  const double Lt = std::max(tot, 1e-9), w0 = S.p[0], w1 = S.p[1], wm = std::max(w0, w1);
  double d = 1e18, L = 0;
  for (int32_t k = 0; k + 1 < n; ++k) {
    const double ax = P[2 * k], ay = P[2 * k + 1], ex = P[2 * k + 2] - ax, ey = P[2 * k + 3] - ay;
    const double ll = std::max(ex * ex + ey * ey, 1e-12);
    const double wx = x - ax, wy = y - ay;
    const double u = clip01((wx * ex + wy * ey) / ll);
    const double t = (L + u * std::sqrt(ll)) / Lt;
    double dk = std::hypot(wx - ex * u, wy - ey * u) - (w0 * (1 - t) + w1 * t) / 2;
    const double Lnext = L + std::hypot(ex, ey);
    if (S.cap == 1) {
      if (L < wm) {
        const double ux = P[2] - P[0], uy = P[3] - P[1];
        const double nn = std::max(std::hypot(ux, uy), 1e-9);
        dk = std::max(dk, -((x - P[0]) * ux + (y - P[1]) * uy) / nn);
      }
      if (tot - Lnext < wm) {
        const double ux = P[2 * (n - 1)] - P[2 * (n - 2)], uy = P[2 * (n - 1) + 1] - P[2 * (n - 2) + 1];
        const double nn = std::max(std::hypot(ux, uy), 1e-9);
        dk = std::max(dk, ((x - P[2 * (n - 1)]) * ux + (y - P[2 * (n - 1) + 1]) * uy) / nn);
      }
    }
    d = std::min(d, dk);
    L = Lnext;
  }
  return d;
}

inline double sd(const UvTab &T, int32_t id, double x, double y) {
  const UvNode &S = T.nodes[id];
  switch (S.kind) {
    case kCircle:
      return std::hypot(x - S.p[0], y - S.p[1]) - S.p[2];
    case kEllipse: {
      double px, py;
      rot(x, y, S.p[0], S.p[1], S.p[5], S.p[6], px, py);
      return sd_ellipse(px, py, S.p[2], S.p[3]);
    }
    case kPoly:
      return sd_poly(T, S, x, y);
    case kSeg: {
      const double ax = S.p[0], ay = S.p[1], ex = S.p[2] - ax, ey = S.p[3] - ay;
      const double wx = x - ax, wy = y - ay;
      const double u = clip01((wx * ex + wy * ey) / (ex * ex + ey * ey));
      return std::hypot(wx - ex * u, wy - ey * u) - S.p[4] / 2;
    }
    case kHalf:
      return (x - S.p[0]) * S.p[2] + (y - S.p[1]) * S.p[3];
    case kUnion: {
      double d = sd(T, T.kids[S.a], x, y);
      for (int32_t i = 1; i < S.n; ++i) d = std::min(d, sd(T, T.kids[S.a + i], x, y));
      return d;
    }
    case kInter: {
      double d = sd(T, T.kids[S.a], x, y);
      for (int32_t i = 1; i < S.n; ++i) d = std::max(d, sd(T, T.kids[S.a + i], x, y));
      return d;
    }
    case kSub:
      return std::max(sd(T, T.kids[S.a], x, y), -sd(T, T.kids[S.a + 1], x, y));
    case kGrow:
      return sd(T, T.kids[S.a], x, y) - S.p[0];
    case kOutline:
      return std::abs(sd(T, T.kids[S.a], x, y)) - S.p[0] / 2;
    case kStroke:
      return sd_stroke(T, S, x, y);
    case kRRect: {
      double px, py;
      rot(x, y, S.p[0], S.p[1], S.p[6], S.p[7], px, py);
      const double r = std::min(S.p[4], std::min(S.p[2], S.p[3]));
      const double qx = std::abs(px) - (S.p[2] - r), qy = std::abs(py) - (S.p[3] - r);
      return std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) + std::min(std::max(qx, qy), 0.0) - r;
    }
    case kArc: {
      // sdf.py arc: rel = (degrees(arctan2(dy, dx)) - a0) % 360 (numpy's floor modulo)
      const double dx = x - S.p[0], dy = y - S.p[1];
      double rel = std::fmod(std::atan2(dy, dx) * kRad - S.p[3], 360.0);
      if (rel < 0) rel += 360.0;
      if (rel <= S.p[4]) return std::abs(std::hypot(dx, dy) - S.p[2]) - S.p[5] / 2;
      return std::min(std::hypot(x - S.p[6], y - S.p[7]), std::hypot(x - S.p[8], y - S.p[9])) - S.p[5] / 2;
    }
    case kLimb:
      return sd_limb(T, S, x, y);
    case kRRect4: {   // figs.py _rrect4: box p[0..3], radii tl tr br bl p[4..7]
      const double cx = (S.p[0] + S.p[2]) / 2, cy = (S.p[1] + S.p[3]) / 2, hx = (S.p[2] - S.p[0]) / 2, hy = (S.p[3] - S.p[1]) / 2;
      const double px = x - cx, py = y - cy;
      const double r = px > 0 ? (py > 0 ? S.p[6] : S.p[5]) : (py > 0 ? S.p[7] : S.p[4]);
      const double qx = std::abs(px) - hx + r, qy = std::abs(py) - hy + r;
      return std::min(std::max(qx, qy), 0.0) + std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) - r;
    }
    case kSuper: {   // common.py _superellipse: (|dx / a|^n + |dy / b|^n)^(1 / n) - 1, times min(a, b)
      const double u = std::abs(x - S.p[0]) / S.p[2], v = std::abs(y - S.p[1]) / S.p[3];
      return (std::pow(std::pow(u, S.p[4]) + std::pow(v, S.p[4]), 1.0 / S.p[4]) - 1.0) * std::min(S.p[2], S.p[3]);
    }
    case kPLine:
      return sd_pline(T, S, x, y);
    case kVLine: {   // fix2.py _vline: capsules along the spline points with the width interpolated along each segment
      const double *P = T.pts + 2 * S.a, *Wd = T.pts + 2 * (S.a + S.n);
      double d = 1e18;
      for (int32_t k = 0; k + 1 < S.n; ++k) {
        const double ax = P[2 * k], ay = P[2 * k + 1], ex = P[2 * k + 2] - ax, ey = P[2 * k + 3] - ay;
        const double ll = std::max(ex * ex + ey * ey, 1e-12);
        const double wx = x - ax, wy = y - ay;
        const double u = clip01((wx * ex + wy * ey) / ll);
        const double r = (Wd[2 * k] * (1 - u) + Wd[2 * k + 2] * u) / 2;
        d = std::min(d, std::hypot(wx - ex * u, wy - ey * u) - r);
      }
      return d;
    }
    case kSMin: {   // parts.py _smin
      double d = sd(T, T.kids[S.a], x, y);
      const double k = S.p[0];
      for (int32_t i = 1; i < S.n; ++i) {
        const double e = sd(T, T.kids[S.a + i], x, y);
        const double h = clip01(0.5 + 0.5 * (e - d) / k);
        d = e * (1 - h) + d * h - k * h * (1 - h);
      }
      return d;
    }
    case kSInter: {   // hs3.py _sinter: the round intersection of two shapes (radius p[0])
      const double d1 = sd(T, T.kids[S.a], x, y), d2 = sd(T, T.kids[S.a + 1], x, y), r = S.p[0];
      const double u = std::max(r + d1, 0.0), v = std::max(r + d2, 0.0);
      return std::min(-r, std::max(d1, d2)) + std::hypot(u, v);
    }
    default:
      return 1e18;
  }
}

// sdf.py normal(): the outward unit normal of a circle's or an ellipse's edge (the exporter refuses a bevel on any other shape)
inline void normal(const UvTab &T, int32_t id, double x, double y, double &nx, double &ny) {
  const UvNode &S = T.nodes[id];
  if (S.kind == kCircle) {
    const double px = x - S.p[0], py = y - S.p[1];
    const double n = std::max(std::hypot(px, py), 1e-9);
    nx = px / n;
    ny = py / n;
    return;
  }
  double px, py;
  const double c = S.p[5], s = S.p[6];
  rot(x, y, S.p[0], S.p[1], c, s, px, py);
  double gx = px / (S.p[2] * S.p[2]), gy = py / (S.p[3] * S.p[3]);
  const double n = std::max(std::hypot(gx, gy), 1e-12);
  gx /= n;
  gy /= n;
  nx = gx * c - gy * s;
  ny = gx * s + gy * c;
}

// sdf.py _stops: piecewise linear between the stops (u clipped to the first and last; a later pair wins at a shared stop)
inline void stops_colour(const double *st, const double *cols, int32_t K, double u, double out[3]) {
  u = clampd(u, st[0], st[K - 1]);
  out[0] = out[1] = out[2] = 0;
  for (int32_t k = 0; k + 1 < K; ++k) {
    const double t0 = st[k], t1 = st[k + 1];
    if (u >= t0 && u <= t1) {
      const double f = (u - t0) / std::max(t1 - t0, 1e-9);
      for (int c = 0; c < 3; ++c) out[c] = cols[3 * k + c] * (1 - f) + cols[3 * (k + 1) + c] * f;
    }
  }
}

// sdf.py _normal(d): np.gradient of shape `id`'s distance on the item's pixel grid at pixel q, normalised
inline void grid_normal(const UvTab &T, int32_t id, const UvPix &q, double d, double &nx, double &ny) {
  auto at = [&](int32_t px, int32_t py) { return sd(T, id, (px + 0.5 - q.ox) / q.sx, (py + 0.5 - q.oy) / q.sy); };
  double gx = 0, gy = 0;
  if (q.i1 - q.i0 >= 2) {
    if (q.px <= q.i0) gx = at(q.px + 1, q.py) - d;
    else if (q.px >= q.i1 - 1) gx = d - at(q.px - 1, q.py);
    else gx = (at(q.px + 1, q.py) - at(q.px - 1, q.py)) / 2;
  }
  if (q.j1 - q.j0 >= 2) {
    if (q.py <= q.j0) gy = at(q.px, q.py + 1) - d;
    else if (q.py >= q.j1 - 1) gy = d - at(q.px, q.py - 1);
    else gy = (at(q.px, q.py + 1) - at(q.px, q.py - 1)) / 2;
  }
  const double n = std::max(std::hypot(gx, gy), 1e-9);
  nx = gx / n;
  ny = gy / n;
}

inline void bevel_colour(const UvTab &T, const UvPaint &P, double dd, double nx, double ny, double out[3]);

// sdf.py paint(): the layer's colour at (x, y) with its distance d. flip_ny: a negative control (the bevel's normal upside down).
// q: the pixel's place in its item (needed only by a grid-normal bevel)
inline void paint(const UvTab &T, const UvLayer &L, double x, double y, double d, double out[3], bool flip_ny = false,
                  const UvPix *q = nullptr) {
  const UvPaint &P = T.paints[L.paint];
  if (P.kind == 0) {
    out[0] = P.rgb[0];
    out[1] = P.rgb[1];
    out[2] = P.rgb[2];
    return;
  }
  if (P.kind == 2) {
    const double ex = P.g[2] - P.g[0], ey = P.g[3] - P.g[1];
    stops_colour(T.stops + P.stops_a, T.coef + P.coef_a, P.nstops, ((x - P.g[0]) * ex + (y - P.g[1]) * ey) / (ex * ex + ey * ey), out);
    return;
  }
  if (P.kind == 3) {
    stops_colour(T.stops + P.stops_a, T.coef + P.coef_a, P.nstops, std::hypot(x - P.g[0], y - P.g[1]) / P.g[2], out);
    return;
  }
  const int32_t sh = P.of >= 0 ? P.of : L.shape;
  const double dd = P.of >= 0 ? sd(T, P.of, x, y) : d;
  double nx = 0, ny = 0;
  if (P.g[0] != 0) {
    if (q) grid_normal(T, sh, *q, dd, nx, ny);
  } else {
    normal(T, sh, x, y, nx, ny);
  }
  if (flip_ny) ny = -ny;
  bevel_colour(T, P, dd, nx, ny, out);
}

// P41G: the bevel paint's colour at depth dd (the bevel shape's distance) with the outward normal (nx, ny)
inline void bevel_colour(const UvTab &T, const UvPaint &P, double dd, double nx, double ny, double out[3]) {
  const double *st = T.stops + P.stops_a;
  const int32_t K = P.nstops;
  const double u = clampd(-dd / P.L, st[0], st[K - 1]);
  const double H[5] = {1.0, nx, ny, nx * nx - ny * ny, 2 * nx * ny};
  out[0] = out[1] = out[2] = 0;
  for (int32_t k = 0; k < K; ++k) {
    const double t = st[k];
    double w = 0;
    if (k > 0) {
      const double a = st[k - 1];
      if (u >= a && u <= t) w = (u - a) / std::max(t - a, 1e-9);
    }
    if (k < K - 1) {
      const double b = st[k + 1];
      if (u > t && u <= b) w = (b - u) / std::max(b - t, 1e-9);
    }
    if (k == 0 && u <= t) w = 1.0;
    if (k == K - 1 && u >= t) w = 1.0;
    if (w == 0) continue;
    const double *c = T.coef + P.coef_a + 15 * k;
    for (int32_t h = 0; h < 5; ++h) {
      const double wh = w * H[h];
      out[0] += wh * c[3 * h];
      out[1] += wh * c[3 * h + 1];
      out[2] += wh * c[3 * h + 2];
    }
  }
}

// P41G: the effects with the layer's grid normal (nx, ny) given (the grid renderer reads it from its distance grid)
inline void apply_fx_n(const UvTab &T, const UvLayer &L, double x, double y, double d, double nx, double ny, double c[3]) {
  for (int32_t i = 0; i < L.fx_n; ++i) {
    const UvFx &F = T.fx[L.fx_a + i];
    const double *v = F.v;
    if (F.kind == 2) {
      const double ex = v[13] - v[11], ey = v[14] - v[12];
      const double u = clip01(((x - v[11]) * ex + (y - v[12]) * ey) / (ex * ex + ey * ey));
      for (int k = 0; k < 3; ++k) c[k] += (1 - 2 * u) * v[15];
      continue;
    }
    const double e = std::pow(clip01(1 + d / v[0]), v[1]);
    if (F.kind == 0) {
      const double lit = nx * v[2] + ny * v[3];
      const double up = e * clip01(lit), dn = e * clip01(-lit);
      for (int k = 0; k < 3; ++k) c[k] = c[k] + up * v[4 + k] - dn * v[7 + k];
    } else {
      const double wall = clip01(nx * v[2] + ny * v[3]);
      const double kk = v[10] * wall * e;
      for (int k = 0; k < 3; ++k) c[k] = c[k] * (1 - kk) + v[4 + k] * kk;
    }
  }
}

// P41G: sdf.py render_layer's effects on the layer's colour, in order (edge_light / inner_shadow take np.gradient's normal of the layer's
// distance on the item's pixel grid: q needed; without q they are left out)
inline void apply_fx(const UvTab &T, const UvLayer &L, double x, double y, double d, double c[3], const UvPix *q) {
  for (int32_t i = 0; i < L.fx_n; ++i) {
    const UvFx &F = T.fx[L.fx_a + i];
    const double *v = F.v;
    if (F.kind == 2) {   // tone
      const double ex = v[13] - v[11], ey = v[14] - v[12];
      const double u = clip01(((x - v[11]) * ex + (y - v[12]) * ey) / (ex * ex + ey * ey));
      for (int k = 0; k < 3; ++k) c[k] += (1 - 2 * u) * v[15];
      continue;
    }
    if (!q) continue;
    double nx = 0, ny = 0;
    grid_normal(T, L.shape, *q, d, nx, ny);
    const double e = std::pow(clip01(1 + d / v[0]), v[1]);
    if (F.kind == 0) {   // edge_light
      const double lit = nx * v[2] + ny * v[3];
      const double up = e * clip01(lit), dn = e * clip01(-lit);
      for (int k = 0; k < 3; ++k) c[k] = c[k] + up * v[4 + k] - dn * v[7 + k];
    } else {             // inner_shadow
      const double wall = clip01(nx * v[2] + ny * v[3]);
      const double kk = v[10] * wall * e;
      for (int k = 0; k < 3; ++k) c[k] = c[k] * (1 - kk) + v[4 + k] * kk;
    }
  }
}

// P41G: a conservative box of shape `id` (b = x0, y0, x1, y1, +-1e300 when unbounded): every point outside it grown by m has sd >= m
// (exact distances: circle, poly, seg, stroke, rrect, arc, limb, rrect4; union = min, inter / sub / sinter = max-like, grow / outline
// shift the distance; the ellipse's distance is an approximation, so its box is grown by its larger radius as well)
inline void node_bounds(const UvTab &T, int32_t id, double b[4]) {
  const UvNode &S = T.nodes[id];
  constexpr double kInf = 1e300;
  auto set = [&](double x0, double y0, double x1, double y1) {
    b[0] = x0;
    b[1] = y0;
    b[2] = x1;
    b[3] = y1;
  };
  auto pts_box = [&](const double *P, int32_t n, double g) {
    double x0 = kInf, y0 = kInf, x1 = -kInf, y1 = -kInf;
    for (int32_t i = 0; i < n; ++i) {
      x0 = std::min(x0, P[2 * i]);
      y0 = std::min(y0, P[2 * i + 1]);
      x1 = std::max(x1, P[2 * i]);
      y1 = std::max(y1, P[2 * i + 1]);
    }
    set(x0 - g, y0 - g, x1 + g, y1 + g);
  };
  switch (S.kind) {
    case kCircle:
      set(S.p[0] - S.p[2], S.p[1] - S.p[2], S.p[0] + S.p[2], S.p[1] + S.p[2]);
      return;
    case kEllipse: {
      const double r = 2 * std::max(S.p[2], S.p[3]);
      set(S.p[0] - r, S.p[1] - r, S.p[0] + r, S.p[1] + r);
      return;
    }
    case kPoly: {
      double x0 = kInf, y0 = kInf, x1 = -kInf, y1 = -kInf;
      for (int32_t r = 0; r < S.n; ++r) {
        double q[4];
        const int32_t pa = T.rings[2 * (S.a + r)], pn = T.rings[2 * (S.a + r) + 1];
        double *bb = b;
        b = q;
        pts_box(T.pts + 2 * pa, pn, 0);
        b = bb;
        x0 = std::min(x0, q[0]);
        y0 = std::min(y0, q[1]);
        x1 = std::max(x1, q[2]);
        y1 = std::max(y1, q[3]);
      }
      set(x0, y0, x1, y1);
      return;
    }
    case kSeg:
      set(std::min(S.p[0], S.p[2]) - S.p[4] / 2, std::min(S.p[1], S.p[3]) - S.p[4] / 2, std::max(S.p[0], S.p[2]) + S.p[4] / 2,
          std::max(S.p[1], S.p[3]) + S.p[4] / 2);
      return;
    case kStroke:
      pts_box(T.pts + 2 * S.a, S.n, S.p[0] / 2);
      return;
    case kPLine:
      pts_box(T.pts + 2 * S.a, S.n, std::max(S.p[0], S.p[1]) / 2);
      return;
    case kVLine:
    case kLimb: {
      double wmax = 0;
      for (int32_t i = 0; i < S.n; ++i) wmax = std::max(wmax, T.pts[2 * (S.a + S.n + i)]);
      pts_box(T.pts + 2 * S.a, S.n, wmax / 2);
      return;
    }
    case kRRect: {
      const double ex = S.p[2] * std::abs(S.p[6]) + S.p[3] * std::abs(S.p[7]), ey = S.p[2] * std::abs(S.p[7]) + S.p[3] * std::abs(S.p[6]);
      set(S.p[0] - ex, S.p[1] - ey, S.p[0] + ex, S.p[1] + ey);
      return;
    }
    case kRRect4:
      set(S.p[0], S.p[1], S.p[2], S.p[3]);
      return;
    case kArc:
      set(S.p[0] - S.p[2] - S.p[5] / 2, S.p[1] - S.p[2] - S.p[5] / 2, S.p[0] + S.p[2] + S.p[5] / 2, S.p[1] + S.p[2] + S.p[5] / 2);
      return;
    case kUnion:
    case kSMin: {   // (smin lies at most k / 4 below the plain minimum: its box grown by k / 4)
      double x0 = kInf, y0 = kInf, x1 = -kInf, y1 = -kInf, q[4];
      for (int32_t i = 0; i < S.n; ++i) {
        node_bounds(T, T.kids[S.a + i], q);
        x0 = std::min(x0, q[0]);
        y0 = std::min(y0, q[1]);
        x1 = std::max(x1, q[2]);
        y1 = std::max(y1, q[3]);
      }
      const double g = S.kind == kSMin ? std::abs(S.p[0]) / 4 + 1e-9 : 0.0;
      set(x0 - g, y0 - g, x1 + g, y1 + g);
      return;
    }
    case kInter:
    case kSInter: {
      double x0 = -kInf, y0 = -kInf, x1 = kInf, y1 = kInf, q[4];
      for (int32_t i = 0; i < S.n; ++i) {
        node_bounds(T, T.kids[S.a + i], q);
        x0 = std::max(x0, q[0]);
        y0 = std::max(y0, q[1]);
        x1 = std::min(x1, q[2]);
        y1 = std::min(y1, q[3]);
      }
      set(x0, y0, x1, y1);
      return;
    }
    case kSub:
      node_bounds(T, T.kids[S.a], b);
      return;
    case kGrow:
    case kOutline: {
      node_bounds(T, T.kids[S.a], b);
      const double g = S.kind == kGrow ? std::max(0.0, S.p[0]) : S.p[0] / 2;
      if (b[0] > -kInf) b[0] -= g;
      if (b[1] > -kInf) b[1] -= g;
      if (b[2] < kInf) b[2] += g;
      if (b[3] < kInf) b[3] += g;
      if (S.kind == kOutline) {   // the line along an unbounded shape's edge: unbounded (a half plane's outline)
        double q[4];
        node_bounds(T, T.kids[S.a], q);
        if (q[0] <= -kInf || q[1] <= -kInf || q[2] >= kInf || q[3] >= kInf) set(-kInf, -kInf, kInf, kInf);
      }
      return;
    }
    default:   // kHalf and anything else: unbounded
      set(-kInf, -kInf, kInf, kInf);
  }
}
// P41G: true when shape `id`'s distance is 1-Lipschitz (|sd(p) - sd(q)| <= |p - q|): the exact distances and their min / max / shifts;
// not the ellipse's approximation, not rrect4 (its radius jumps between quadrants), not sinter (its rounding can reach sqrt 2), not a
// half plane with a normal longer than 1
inline bool node_lip(const UvTab &T, int32_t id) {
  const UvNode &S = T.nodes[id];
  switch (S.kind) {
    case kCircle: case kPoly: case kSeg: case kStroke: case kRRect: case kArc: case kLimb:
      return true;
    case kHalf:
      return std::hypot(S.p[2], S.p[3]) <= 1.0 + 1e-12;
    case kUnion: case kInter: case kSub: case kGrow: case kOutline:
      for (int32_t i = 0; i < (S.kind == kSub ? 2 : S.kind == kGrow || S.kind == kOutline ? 1 : S.n); ++i)
        if (!node_lip(T, T.kids[S.a + i])) return false;
      return true;
    default:
      return false;
  }
}
// a layer's shape box and clip box (8 doubles, frame units, ungrown); a pixel's layer can be skipped when it lies outside the shape
// box grown by the edge width (soft, or 0.5 / s for a hard edge) or outside the clip box grown by 0.5 / s: its coverage is 0 there
inline void layer_boxes(const UvTab &T, int32_t li, double out[8]) {
  const UvLayer &L = T.layers[li];
  node_bounds(T, L.shape, out);
  if (L.clip >= 0) {
    node_bounds(T, L.clip, out + 4);
  } else {
    out[4] = out[5] = -1e300;
    out[6] = out[7] = 1e300;
  }
}
inline bool layer_outside(const double *lb, double soft, double x, double y, double s) {
  const double m = (soft >= 0 ? soft : 0.5 / s) + 1e-9, mc = 0.5 / s + 1e-9;
  return x < lb[0] - m || x > lb[2] + m || y < lb[1] - m || y > lb[3] + m || x < lb[4] - mc || x > lb[6] + mc || y < lb[5] - mc ||
         y > lb[7] + mc;
}

// one item's layers over the pixel at frame point (x, y): px = red, green, blue (premultiplied or opaque, 0..255) and the coverage.
// s = frame units -> target pixels (the hard edge's width, sdf.render's s)
inline void composite_item(const UvTab &T, const UvItem &I, double x, double y, double s, double px[4], bool flip_ny = false,
                           const UvPix *q = nullptr, const double *lboxes = nullptr) {
  for (int32_t li = 0; li < I.nlayers; ++li) {
    const UvLayer &L = T.layers[I.layer_a + li];
    // P41G: lboxes (layer_boxes of the item's layers, 8 doubles each): a layer whose box the point lies outside covers 0 there
    if (lboxes && layer_outside(lboxes + 8 * li, L.soft, x, y, s)) continue;
    const double d = sd(T, L.shape, x, y);
    double a;
    if (L.soft >= 0) {
      const double u = clip01(0.5 - d / (2 * L.soft));
      a = u * u * (3 - 2 * u);
    } else {
      a = clip01(0.5 - d * s);
    }
    if (a <= 0) continue;
    if (L.falloff) a *= std::pow(clip01(1 - std::hypot(x - L.fcx, y - L.fcy) / L.fr), L.fpow);
    if (a <= 0) continue;
    if (L.clip >= 0) a *= clip01(0.5 - sd(T, L.clip, x, y) * s);
    a *= L.alpha;
    if (a <= 0) continue;
    double c[3];
    paint(T, L, x, y, d, c, flip_ny, q);
    if (L.fx_n > 0 && T.fx != nullptr) apply_fx(T, L, x, y, d, c, q);
    const double k1 = 1 - a;
    px[0] = clampd(c[0], 0, 255) * a + px[0] * k1;
    px[1] = clampd(c[1], 0, 255) * a + px[1] * k1;
    px[2] = clampd(c[2], 0, 255) * a + px[2] * k1;
    px[3] = a + px[3] * k1;
  }
}

}  // namespace uveval
}  // namespace portsim

#endif
