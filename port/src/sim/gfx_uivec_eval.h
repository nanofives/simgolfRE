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

enum UvKind : uint8_t { kCircle = 0, kEllipse, kPoly, kSeg, kHalf, kUnion, kInter, kSub, kGrow, kOutline, kStroke, kRRect, kArc };
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

// one item's layers over the pixel at frame point (x, y): px = red, green, blue (premultiplied or opaque, 0..255) and the coverage.
// s = frame units -> target pixels (the hard edge's width, sdf.render's s)
inline void composite_item(const UvTab &T, const UvItem &I, double x, double y, double s, double px[4], bool flip_ny = false,
                           const UvPix *q = nullptr) {
  for (int32_t li = 0; li < I.nlayers; ++li) {
    const UvLayer &L = T.layers[I.layer_a + li];
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
