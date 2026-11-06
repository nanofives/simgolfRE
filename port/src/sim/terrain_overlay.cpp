// Terrain::drawLine through the port's model of Windows' generic GL antialiased lines, and the exe's line drawers over it
// (roadmap S3.8). What the original does and what was measured: portsim/sim/terrain_overlay.h and log/roadmap/S3/8_notes.md.
//
// Module `terrain_overlay`: one test entry, overlay.rasterLine(buf, w, h, x1, y1, x2, y2, colour, width, alpha, flags, prev_width),
// draws one drawLine into a w x h RGB555 buffer of the address space at buf (flags bit 0 = the flip byte, bit 1 = no GL context;
// prev_width = the line width before the call as float bits, 0 = GL's default 1.0) and returns the pixels written. The gfx bodies
// (drawMapLine, drawMinimapSegment, setLinePoint, lineTo, drawCircle) are listed in gfx_terrain.cpp's family table.
#define _CRT_SECURE_NO_WARNINGS   // the development dump (SG_OVERLAY_DUMP) uses fopen / getenv
#include "portsim/sim/terrain_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/bridge.h"
#include "portsim/sim/gl_raster.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_live.h"
#include "portsim/sim/terrain_state.h"
#include "portsim/sim/util.h"

namespace portsim {

int g_negctl_terrain_overlay = 0;

namespace overlay {
namespace {

int mode() { return SG_NEGCTL(g_negctl_terrain_overlay); }

// the line rule (roadmap S3.24; log/roadmap/S3/24_notes.md). Default 2: per column of the major axis the generic GL draws
// N = floor(w * L / |major|) + 2 pixels, the pixel centres whose minor offset o from the line centre, times the tie sign, lies in
// [-N/2, N/2); the tie sign is sign(dx) * sign(dy) for an x-major line and sign(dx) for a y-major one. 0 = S3.8's model (no span);
// development variants: 1 = the sign of the minor delta, 3 = sign(dx) * sign(dy) on both axes, 4 = +1, 5 = -1, 6 = no tie decision
// (|o| <= N/2 + kSpanSlack), 7 = x-major sign(dx) * sign(dy), y-major -sign(dx) * sign(dy). Set by overlay.setRule or,
// before that, by SG_OVERLAY_RULE; negative control 11 = rule 0.
// Roadmap S3.31 (log/roadmap/S3/31_notes.md): 8 = rule 2's footprint with the ends through the generic GL's float32 vertex
// stage (float_ends below) instead of the double snap; 9 (the default) = rule 8 plus the second-vertex holes (second_vertex_hole
// below). Negative control 12 = the double snap of the ends under rules 8 / 9, 13 = no holes under rule 9.
constexpr double kSpanSlack = 0.002;
int g_line_rule = -1;
int line_rule() {
  if (g_line_rule < 0) {
    const char *e = std::getenv("SG_OVERLAY_RULE");
    g_line_rule = (e && *e) ? std::atoi(e) : 9;
  }
  return g_line_rule;
}

// THE FLOAT32 VERTEX STAGE (roadmap S3.31): drawLine's glVertex2i ends through glOrtho's matrix as the generic GL keeps it (each
// element computed in double, stored as float; the modelview is the identity, so the MVP is the projection), clip = m12 + v * m0
// per axis with every operation in float (GlRaster's vertex rule, roadmap S3.21); the segment is clipped in clip space against
// x >= -1, x <= 1, y >= -1, y <= 1 in that order (w = 1), t = d_in / (d_in - d_out) and the new end = in + t (out - in) per
// coordinate in float, from the inside end; window = float(ndc * float(h - 2^-8)) + h, rounded half to even to 2^-8 (h the
// viewport's half-size). Measured against the real GL (log/roadmap/S3/31_score.py): on S3.24's probe sets and the 9,534 dumped
// game calls it changes no pixel the double snap had right and fixes 170; the double snap differs from it at the integer
// coordinates x 200 and 599 and at ends the clip moved. Returns false when the segment lies outside the viewport.
// THE SECOND-VERTEX HOLES (roadmap S3.31, U-1826): at the second end b of a segment the generic GL generates no fragment for
// some pixels inside the end cap. In the segment's canonical frame (u along the major axis and v along the minor one, both
// increasing from the first end a to b, origin b; s = |minor / major|), with o = v - u s the pixel centre's minor offset from the
// line centre, N the span count and q = u' + v' s, a pixel is left out when 0 < frac(o + N/2) < 1 - s (its row held the same span
// cell in the previous column) and q > 1 - sqrt(1 + s^2) / 2 (it lies beyond the end cap q = sqrt(1 + s^2) / 2 mirrored about the
// perpendicular through b + (0.5, 0)). q is measured from b' = a + round(b - a) per axis (the first end plus the rounded delta:
// where the 2^-8 snap moved b but not a, the real GL keeps the pixels a q from b would leave out), o from b; a, b are the ends
// before the clip (a clipped second end leaves no hole on screen). A segment whose first end the clip moved gets no holes: on such
// lines the real GL kept pixels this rule leaves out (two of the unit scenes' lines, U-1982). Exact ties are kept
// (kHoleEps). Measured with glreplay on dense sub-pixel samples of the canonical plane (log/roadmap/S3/31_shape.py) and checked on
// every probe set and the dumped game calls (31_holes.py); the first end has no holes.
constexpr double kHoleEps = 1e-9;
bool second_vertex_hole(double px, double py, double ax, double ay, double bx, double by, double wd) {
  const double dx = bx - ax, dy = by - ay;
  const bool xmaj = std::fabs(dx) >= std::fabs(dy);
  const double maj = xmaj ? dx : dy, mnr = xmaj ? dy : dx;
  if (maj == 0.0) return false;
  // only the pixels within 2 px of the second end along the line can be holes (q > 1 - sqrt(1 + s^2) / 2 and inside the cap)
  const double len = std::sqrt(dx * dx + dy * dy);
  if (((px - bx) * dx + (py - by) * dy) / len < -2.0) return false;
  const double span_n = std::floor(wd * len / std::fabs(maj)) + 2.0;
  const double sM = maj < 0.0 ? -1.0 : 1.0, sm = mnr < 0.0 ? -1.0 : 1.0;
  const double sl = std::fabs(mnr / maj);
  const double u = ((xmaj ? px : py) - (xmaj ? bx : by)) * sM;
  const double v = ((xmaj ? py : px) - (xmaj ? by : bx)) * sm;
  const double o = v - u * sl;
  const double c = o + span_n / 2.0;
  const double f = c - std::floor(c);
  const double bpx = ax + std::nearbyint(dx), bpy = ay + std::nearbyint(dy);
  const double uq = ((xmaj ? px : py) - (xmaj ? bpx : bpy)) * sM;
  const double vq = ((xmaj ? py : px) - (xmaj ? bpy : bpx)) * sm;
  const double q = uq + vq * sl;
  return f > kHoleEps && f < 1.0 - sl - kHoleEps && q > 1.0 - 0.5 * std::sqrt(1.0 + sl * sl);
}

bool float_ends(const LineGl &gl, int32_t fb_w, int32_t fb_h, int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool clip_on, bool snap_on,
                double &ax, double &ay, double &bx, double &by, double *unclipped = nullptr) {
  const float mx0 = static_cast<float>(2.0 / gl.screen_w), mx12 = -1.0f;
  const double ybot = gl.flip_y ? 0.0 : gl.screen_h, ytop = gl.flip_y ? gl.screen_h : 0.0;
  const float my0 = static_cast<float>(2.0 / (ytop - ybot)), my12 = static_cast<float>(-(ytop + ybot) / (ytop - ybot));
  auto clipc = [](float m12, float m0, int32_t v) {
    volatile float p = static_cast<float>(v) * m0;
    volatile float c = m12 + p;
    return static_cast<float>(c);
  };
  float a[2] = {clipc(mx12, mx0, x1), clipc(my12, my0, y1)};
  float b[2] = {clipc(mx12, mx0, x2), clipc(my12, my0, y2)};
  const float a_in[2] = {a[0], a[1]}, b_in[2] = {b[0], b[1]};
  if (clip_on) {
    const int axis[4] = {0, 0, 1, 1};
    const float sg[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    for (int k = 0; k < 4; ++k) {
      const int i = axis[k];
      volatile float da = 1.0f + sg[k] * a[i], db = 1.0f + sg[k] * b[i];
      const float fa = da, fb = db;
      if (fa < 0.0f && fb < 0.0f) return false;
      if (fa < 0.0f || fb < 0.0f) {
        float *in = fa < 0.0f ? b : a, *out = fa < 0.0f ? a : b;
        const float din = fa < 0.0f ? fb : fa, dout = fa < 0.0f ? fa : fb;
        volatile float den = din - dout;
        volatile float t = din / den;
        float nw[2];
        for (int j = 0; j < 2; ++j) {
          volatile float diff = out[j] - in[j];
          volatile float step = t * diff;
          volatile float val = in[j] + step;
          nw[j] = val;
        }
        out[0] = nw[0];
        out[1] = nw[1];
      }
    }
  }
  auto win = [snap_on](float c, double h) {
    volatile float p = c * static_cast<float>(h - 1.0 / 256.0);
    const double w = static_cast<double>(p) + h;
    return snap_on ? std::nearbyint(256.0 * w) / 256.0 : w;
  };
  const double hx = fb_w / 2.0, hy = fb_h / 2.0;
  ax = win(a[0], hx);
  ay = win(a[1], hy);
  bx = win(b[0], hx);
  by = win(b[1], hy);
  if (unclipped) {
    unclipped[0] = win(a_in[0], hx);
    unclipped[1] = win(a_in[1], hy);
    unclipped[2] = win(b_in[0], hx);
    unclipped[3] = win(b_in[1], hy);
  }
  return true;
}

// the 4x4 ordered dither of the generic GL's 16-bit write (gl_raster.cpp quantize, fitted on the terrain frames)
constexpr int kDither[4][4] = {{15, 7, 13, 5}, {3, 11, 1, 9}, {12, 4, 14, 6}, {0, 8, 2, 10}};
int quantize(double c, int32_t x, int32_t y) {
  const double v = c * 31.0 / 255.0 + (kDither[y & 3][x & 3] + 0.5) / 16.0;
  const int o = static_cast<int>(std::floor(v));
  return o < 0 ? 0 : (o > 31 ? 31 : o);
}

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// development switch SG_OVERLAY_DUMP=<file> (roadmap S3.24): every drawn call appends its arguments, the GL state and the pixels of
// its box before and after, so the same calls can be replayed through the real software GL (log/roadmap/S3/24_probe.py)
struct LineDump {
  const char *path;
  uint16_t *fb;
  int32_t w, x0, y0, x1, y1;
  std::vector<uint8_t> rec;
  LineDump(uint16_t *f, int32_t fw, int32_t fh, int32_t a, int32_t b, int32_t c, int32_t d)
      : path(std::getenv("SG_OVERLAY_DUMP")), fb(f), w(fw), x0(std::max(0, a - 2)), y0(std::max(0, b - 2)),
        x1(std::min(fw - 1, c + 2)), y1(std::min(fh - 1, d + 2)) {}
  bool on() const { return path && *path && x1 >= x0 && y1 >= y0; }
  void put(int32_t v) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&v);
    rec.insert(rec.end(), p, p + 4);
  }
  void box() {
    for (int32_t y = y0; y <= y1; ++y) {
      const uint8_t *p = reinterpret_cast<const uint8_t *>(fb + static_cast<size_t>(y) * static_cast<size_t>(w) + x0);
      rec.insert(rec.end(), p, p + 2 * static_cast<size_t>(x1 - x0 + 1));
    }
  }
  void head(int32_t ax, int32_t ay, int32_t bx, int32_t by, uint32_t colour, int32_t width, int32_t alpha, const LineGl &gl) {
    uint32_t wb;
    std::memcpy(&wb, &gl.line_width, 4);
    for (int32_t v : {0x314c564f, ax, ay, bx, by, static_cast<int32_t>(colour), width, alpha, static_cast<int32_t>(wb),
                      gl.flip_y ? 1 : 0, gl.screen_w, gl.screen_h, w, x0, y0, x1, y1})
      put(v);
    box();
  }
  void tail() {
    box();
    if (FILE *f = std::fopen(path, "ab")) {
      std::fwrite(rec.data(), 1, rec.size(), f);
      std::fclose(f);
    }
  }
};

// U-1076's fixed-point window-coordinate setup: c + (v - c) * (1 - 2^-8 / h), rounded to 8 fractional bits
double snap(double v, double c, double h) {
  if (mode() == 5) return v;
  return std::floor(256.0 * (c + (v - c) * (1.0 - 1.0 / 256.0 / h)) + 0.5) / 256.0;
}

// Liang-Barsky clip of the segment to [0, w] x [0, h]; false when nothing is left
bool clip(double &x1, double &y1, double &x2, double &y2, double w, double h) {
  double t0 = 0.0, t1 = 1.0;
  const double dx = x2 - x1, dy = y2 - y1;
  const double p[4] = {-dx, dx, -dy, dy}, q[4] = {x1, w - x1, y1, h - y1};
  for (int i = 0; i < 4; ++i) {
    if (p[i] == 0.0) {
      if (q[i] < 0.0) return false;
      continue;
    }
    const double r = q[i] / p[i];
    if (p[i] < 0.0)
      t0 = std::max(t0, r);
    else
      t1 = std::min(t1, r);
  }
  if (t0 > t1) return false;
  const double ax = x1 + t0 * dx, ay = y1 + t0 * dy, bx = x1 + t1 * dx, by = y1 + t1 * dy;
  x1 = ax;
  y1 = ay;
  x2 = bx;
  y2 = by;
  return true;
}

// drawLine's channel floats: ((c >> 7) & 0xf8) / 255.0f and so on, computed in single precision as the original's fstp stores them
float channel(uint32_t bits8) { return static_cast<float>(bits8) / 255.0f; }

struct OverlayState {
  bool seeded = false;
  int32_t px = 0, py = 0;   // the line point (g_linePoint5a412c), seeded from the GameState at first use
};

// the harness's log of the drawLine calls that reach a GL context (overlay.logStart / overlay.logWrite)
struct CallLog {
  bool on = false;
  std::string text;
};

OverlayState &state(Gfx &g, Ctx &ctx) {
  OverlayState &s = g.state_of<OverlayState>("terrain_overlay");
  if (!s.seeded) {
    const auto p = state_arr(ctx, &GameState::g_linePoint5a412c);
    s.px = p[0];
    s.py = p[1];
    s.seeded = true;
  }
  return s;
}

LiveTerrain *live(Gfx &g, Ctx &ctx, const char *who) {
  LiveTerrain *lt = has_terrain_api(ctx) ? dynamic_cast<LiveTerrain *>(&terrain_api(ctx)) : nullptr;
  if (!lt) g.unsup(std::string(who) + ": the TerrainApi is not a LiveTerrain");
  return lt;
}

// the per-context GL line width (the terrain rasteriser's context keeps it between calls)
struct WidthState {
  float width = 1.0f;
};

// the harness's call log: the GL arguments the original passes, glLineWidth((float)width) and glColor4f(r, g, b, alpha / 10.0f) as
// float bits, the two glVertex2i ends, and whether a GL context existed
void log_call(Gfx &g, const char *who, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colour, int32_t width, int32_t alpha,
              bool context) {
  CallLog &cl = g.state_of<CallLog>("terrain_overlay.log");
  if (!cl.on) return;
  const float f[5] = {static_cast<float>(width), channel((colour >> 7) & 0xf8u), channel((colour >> 2) & 0xf8u),
                      channel((colour << 3) & 0xf8u), static_cast<float>(alpha) / 10.0f};
  uint32_t b[5];
  std::memcpy(b, f, sizeof(b));
  char line[160];
  std::snprintf(line, sizeof(line), "%s %d %d %d %d w=0x%x c=0x%x,0x%x,0x%x,0x%x ctx=%d\n", who, x1, y1, x2, y2, b[0], b[1], b[2], b[3],
                b[4], context ? 1 : 0);
  cl.text += line;
}

// P13D item 7 (log/roadmap/P13/P13D_notes.md): the same line on the world layer (terrain_live.h LiveTerrainWorld), drawn while
// lines_open (after the frame's terrain picture, before renderSpriteQueue: the hole's shot lines, mainLoop's drawMapLine pair after
// worldToScreen and tileToScreen). The world layer is the window-sized course picture the app shows beside the 800 frame's columns
// and, where the frame's pixel is terrain (back == T), inside them; before, these lines were drawn only into the 800 x 600 root, so in
// a wide window they stopped at the frame's left and right edges. Here the 800-space ends map to world pixels as the terrain does
// (x -> ox + x * fw / 800, y -> y * ph / 600, the root's row order), the width scales by ph / 600, and the line is the same
// antialiased footprint as draw_line's (coverage * alpha / 10 over the pixel, the same dither). The 800 root's pixels the line
// changed are then put into T, so inside the frame's columns the window shows the world's line too (one line, one resolution).
// Drawing only: the root, the GameState and the address space are unchanged.
void mirror_line_world(Gfx &g, LiveTerrainWorld &w, const LineGl &gl, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colour,
                       int32_t alpha, const std::vector<uint16_t> &before) {
  std::vector<uint16_t> &px = w.raster->pixels();
  if (w.pw <= 0 || w.ph <= 0 || w.fw <= 0 || px.size() != static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph)) return;
  const double s = static_cast<double>(w.ph) / 600.0, kx = static_cast<double>(w.fw) / 800.0;
  auto wy = [&](int32_t y) { return (gl.flip_y ? static_cast<double>(y) : static_cast<double>(gl.screen_h - y)) * s; };
  const double ax = w.ox + x1 * kx, ay = wy(y1), bx = w.ox + x2 * kx, by = wy(y2);
  const double dx = bx - ax, dy = by - ay, len = std::sqrt(dx * dx + dy * dy);
  if (len == 0.0) return;
  const double wd = std::min(static_cast<double>(gl.line_width), 10.0) * s;
  const double ux = dx / len, uy = dy / len, half = wd / 2.0, reach = half + 1.5;
  const double cr = clamp01(channel((colour >> 7) & 0xf8u)), cg = clamp01(channel((colour >> 2) & 0xf8u)),
               cb = clamp01(channel((colour << 3) & 0xf8u));
  const double ca = clamp01(static_cast<float>(alpha) / 10.0f);
  const int32_t x0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ax, bx) - reach)));
  const int32_t xe = std::min(w.pw - 1, static_cast<int32_t>(std::ceil(std::max(ax, bx) + reach)));
  const int32_t y0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ay, by) - reach)));
  const int32_t ye = std::min(w.ph - 1, static_cast<int32_t>(std::ceil(std::max(ay, by) + reach)));
  for (int32_t y = y0; y <= ye; ++y)
    for (int32_t x = x0; x <= xe; ++x) {
      const double qx = x + 0.5 - ax, qy = y + 0.5 - ay;
      const double t = qx * ux + qy * uy, n = -qx * uy + qy * ux;
      const double cov = clamp01(half + 0.5 - std::fabs(n)) * clamp01(t + 0.5) * clamp01(len - t + 0.5);
      if (cov <= 0.0) continue;
      const double al = ca * cov;
      uint16_t &d = px[static_cast<size_t>(y) * static_cast<size_t>(w.pw) + static_cast<size_t>(x)];
      const int dr = (d >> 10) & 31, dg = (d >> 5) & 31, db = d & 31;
      const double vr = cr * 255.0 * al + dr * 255.0 / 31.0 * (1.0 - al);
      const double vg = cg * 255.0 * al + dg * 255.0 / 31.0 * (1.0 - al);
      const double vb = cb * 255.0 * al + db * 255.0 / 31.0 * (1.0 - al);
      d = static_cast<uint16_t>((quantize(vr, x, y) << 10) | (quantize(vg, x, y) << 5) | quantize(vb, x, y));
    }
  if (before.size() == g.root.px.size() && w.terrain800.size() == g.root.px.size())
    for (size_t i = 0; i < before.size(); ++i)
      if (before[i] != g.root.px[i] && before[i] == w.terrain800[i]) w.terrain800[i] = g.root.px[i];
}

// Terrain::drawLine into the backend's root surface
void terrain_draw_line(Gfx &g, Ctx &ctx, const char *who, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colour,
                       int32_t width, int32_t alpha) {
  g.ensure_root(ctx);
  if (mode() == 10) return;   // control 10: the overlay draws nothing (the frame tests' baseline)
  // the Terrain object drawMapLine passes (ecx = gTerrain): its GL context handle, flip byte and screen size
  const uint32_t ter = ctx.gs().gTerrain;
  LineGl gl;
  try {
    if (ter == 0) throw Fault{0, 0};
    const Ptr<TerrainObjectHeader> h(ctx.mem, ter);
    gl.context = h.field(&TerrainObjectHeader::glrc).get() != 0;
    gl.flip_y = h.field(&TerrainObjectHeader::flip_y).get() != 0;
    gl.screen_w = h.field(&TerrainObjectHeader::screen_w).get();
    gl.screen_h = h.field(&TerrainObjectHeader::screen_h).get();
  } catch (const Fault &) {
    g.unsup(std::string(who) + ": no Terrain object at gTerrain (the original's drawLine would read through it)");
    return;
  }
  // no GL context (before Terrain::initSystem, or a headless run's zeroed object): the driver ignores every call
  if (!gl.context && mode() != 9) {
    log_call(g, who, x1, y1, x2, y2, colour, width, alpha, false);
    return;
  }
  LiveTerrain *lt = live(g, ctx, who);
  if (!lt) return;
  gl::GlRaster &r = lt->raster();
  if (r.width() != g.root.w || r.height() != g.root.h) {
    g.unsup(std::string(who) + ": a root surface of another size than the rasteriser's");
    return;
  }
  WidthState &ws = g.state_of<WidthState>("terrain_overlay.width");
  gl.line_width = ws.width;
  if (gl.context && (gl.screen_w != g.root.w || gl.screen_h != g.root.h)) {
    g.unsup(std::string(who) + ": a glOrtho extent other than the viewport (the Terrain object's screen size)");
    return;
  }
  log_call(g, who, x1, y1, x2, y2, colour, width, alpha, gl.context);
  // P10I fix 1 (gfx.h gfx_set_hud_lines): P7's tee-to-cursor line (ML+0xb9b1) and the tee tool's green-to-cursor arm (ML+0xbb2a),
  // drawMinimapSegment(.., 0x80007fff, 2, 7) inside a cursor pass, are recorded with the root pixels they change. Records only.
  const bool rec = g.hud.lines_on && g.hud.cursor_sprites && g.hud.cursor_depth > 0 && !std::strcmp(who, "drawMinimapSegment") &&
                   width == 2 && alpha == 7 && colour == 0x80007fffu && g.root.w == 800 && g.root.h == 600;
  std::vector<uint16_t> before;
  // P13D item 7: a line drawn between the frame's terrain picture and the sprite queue goes on the world layer too
  LiveTerrainWorld *const world = lt->world();
  const bool mirror = !rec && world && world->valid && world->lines_open && world->raster && g.hud.cursor_depth == 0 &&
                      (gl.context || mode() == 9) && mode() != 14;
  if (rec || mirror) before = g.root.px;
  draw_line(g.root.px.data(), g.root.w, g.root.h, gl, x1, y1, x2, y2, colour, width, alpha);
  if (mirror) mirror_line_world(g, *world, gl, x1, y1, x2, y2, colour, alpha, before);   // gl.line_width: this call's
  if (rec && before.size() == g.root.px.size()) {
    Gfx::HudCapture &c = g.hud;
    if (c.line800.size() != before.size()) {
      c.line800.assign(before.size(), 0);
      c.line_word.assign(before.size(), 0);
    }
    for (size_t i = 0; i < before.size(); ++i)
      if (before[i] != g.root.px[i]) {
        c.line800[i] = 1;
        c.line_word[i] = g.root.px[i];
      }
    GfxHudLine l;
    l.x1 = x1;
    l.y1 = y1;
    l.x2 = x2;
    l.y2 = y2;
    l.colour = colour;
    l.width = gl.line_width;
    l.alpha = alpha;
    c.lines.push_back(l);
  }
  ws.width = gl.line_width;
  if (gl.context || mode() == 9) apply_state(r, colour, alpha);
}

}  // namespace

int64_t draw_line(uint16_t *fb, int32_t fb_w, int32_t fb_h, LineGl &gl, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colour,
                  int32_t width, int32_t alpha) {
  if (!gl.context && mode() != 9) return 0;   // no current context: the driver ignores every call
  if (width > 0) gl.line_width = static_cast<float>(width);   // glLineWidth(w <= 0) is GL_INVALID_VALUE: the width stays
  const double wd = mode() == 8 ? static_cast<double>(gl.line_width) : std::min(static_cast<double>(gl.line_width), 10.0);
  // glColor4f(r, g, b, alpha / 10.0f), each component clamped to [0, 1]
  const double cr = clamp01(channel((colour >> 7) & 0xf8u)), cg = clamp01(channel((colour >> 2) & 0xf8u)),
               cb = clamp01(channel((colour << 3) & 0xf8u));
  const double ca = mode() == 2 ? 1.0 : clamp01(static_cast<float>(alpha) / 10.0f);
  // window coordinates through glOrtho and the viewport (0, 0, fb_w, fb_h)
  const bool flip = mode() == 4 ? !gl.flip_y : gl.flip_y;
  // the line rule (see line_rule); control 11 = rule 0, control 12 = the double snap of the ends under rules 8 / 9, control 13 =
  // no second-vertex holes under rule 9
  const int rule = mode() == 11 ? 0 : line_rule();
  const bool float_vertex = (rule == 8 || rule == 9) && mode() != 12;
  bool holes = rule == 9 && mode() != 13;
  double ax, ay, bx, by;
  double ends0[4] = {0.0, 0.0, 0.0, 0.0};   // the ends before the clip (the second-vertex holes are measured from them)
  if (float_vertex) {
    LineGl g2 = gl;
    g2.flip_y = flip;
    if (!float_ends(g2, fb_w, fb_h, x1, y1, x2, y2, mode() != 3, mode() != 5, ax, ay, bx, by, ends0)) return 0;
    if (ax != ends0[0] || ay != ends0[1]) holes = false;   // the clip moved the first end
  } else {
    const double sx = static_cast<double>(fb_w) / gl.screen_w, sy = static_cast<double>(fb_h) / gl.screen_h;
    ax = x1 * sx;
    bx = x2 * sx;
    ay = flip ? y1 * sy : (gl.screen_h - static_cast<double>(y1)) * sy;
    by = flip ? y2 * sy : (gl.screen_h - static_cast<double>(y2)) * sy;
    {
      const double cx0 = fb_w / 2.0, cy0 = fb_h / 2.0;
      ends0[0] = snap(ax, cx0, cx0);
      ends0[1] = snap(ay, cy0, cy0);
      ends0[2] = snap(bx, cx0, cx0);
      ends0[3] = snap(by, cy0, cy0);
    }
    if (mode() != 3 && !clip(ax, ay, bx, by, fb_w, fb_h)) return 0;
    const double cx = fb_w / 2.0, cy = fb_h / 2.0;
    if (snap(ax, cx, cx) != ends0[0] || snap(ay, cy, cy) != ends0[1]) holes = false;   // the clip moved the first end
    ax = snap(ax, cx, cx);
    bx = snap(bx, cx, cx);
    ay = snap(ay, cy, cy);
    by = snap(by, cy, cy);
  }
  const double dx = bx - ax, dy = by - ay, len = std::sqrt(dx * dx + dy * dy);
  if (len == 0.0) return 0;
  const double ux = dx / len, uy = dy / len, half = wd / 2.0;
  const double reach = half + 1.5;
  const int32_t x0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ax, bx) - reach)));
  const int32_t xe = std::min(fb_w - 1, static_cast<int32_t>(std::ceil(std::max(ax, bx) + reach)));
  const int32_t y0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ay, by) - reach)));
  const int32_t ye = std::min(fb_h - 1, static_cast<int32_t>(std::ceil(std::max(ay, by) + reach)));
  LineDump dump(fb, fb_w, fb_h, x0, y0, xe, ye);
  if (dump.on()) dump.head(x1, y1, x2, y2, colour, width, alpha, gl);
  // the span rule (roadmap S3.24): per column of the major axis the real GL draws N = floor(w * L / |major|) + 2 pixels, the
  // pixel centres whose minor offset s from the line centre (o times the tie sign) lies in [-N/2, N/2)
  const bool xmaj = std::fabs(dx) >= std::fabs(dy);
  double span_n = 0.0, span_sign = 1.0;
  if (rule != 0) {
    span_n = std::floor(wd * len / std::max(std::fabs(dx), std::fabs(dy))) + 2.0;
    const double sgx = dx < 0.0 ? -1.0 : 1.0, sgy = dy < 0.0 ? -1.0 : 1.0;
    span_sign = (rule == 2 || rule == 8 || rule == 9) ? (xmaj ? sgx * sgy : sgx) : rule == 1 ? (xmaj ? sgy : sgx) : rule == 3 ? sgx * sgy
                : rule == 7 ? (xmaj ? sgx * sgy : -sgx * sgy) : rule == 5 ? -1.0 : 1.0;
  }
  int64_t written = 0;
  for (int32_t y = y0; y <= ye; ++y) {
    for (int32_t x = x0; x <= xe; ++x) {
      if (rule != 0) {
        const double o = xmaj ? (y + 0.5) - (ay + (x + 0.5 - ax) * dy / dx) : (x + 0.5) - (ax + (y + 0.5 - ay) * dx / dy);
        const double s = o * span_sign;
        if (rule == 6 ? std::fabs(o) > span_n / 2.0 + kSpanSlack : !(s >= -span_n / 2.0 && s < span_n / 2.0)) continue;
      }
      const double px = x + 0.5 - ax, py = y + 0.5 - ay;
      const double t = px * ux + py * uy, n = -px * uy + py * ux;
      double cov;
      if (mode() == 1)
        cov = (std::fabs(n) <= half && t >= 0.0 && t <= len) ? 1.0 : 0.0;
      else
        cov = clamp01(half + 0.5 - std::fabs(n)) * clamp01(t + 0.5) * clamp01(len - t + 0.5);
      if (cov <= 0.0) continue;
      if (holes && second_vertex_hole(x + 0.5, y + 0.5, ends0[0], ends0[1], ends0[2], ends0[3], wd)) continue;
      const double al = ca * cov;
      uint16_t &d = fb[static_cast<size_t>(y) * static_cast<size_t>(fb_w) + static_cast<size_t>(x)];
      const int dr = (d >> 10) & 31, dg = (d >> 5) & 31, db = d & 31;
      const double vr = cr * 255.0 * al + dr * 255.0 / 31.0 * (1.0 - al);
      const double vg = cg * 255.0 * al + dg * 255.0 / 31.0 * (1.0 - al);
      const double vb = cb * 255.0 * al + db * 255.0 / 31.0 * (1.0 - al);
      d = static_cast<uint16_t>((quantize(vr, x, y) << 10) | (quantize(vg, x, y) << 5) | quantize(vb, x, y));
      ++written;
    }
  }
  if (dump.on()) dump.tail();
  return written;
}

void apply_state(gl::GlRaster &r, uint32_t colour, int32_t alpha) {
  r.enable(0x0b50);    // GL_LIGHTING
  r.enable(0x0de1);    // GL_TEXTURE_2D
  r.disable(0x0be2);   // GL_BLEND
  r.blend_func(0x0302, 0x0303);
  r.color4f(channel((colour >> 7) & 0xf8u), channel((colour >> 2) & 0xf8u), channel((colour << 3) & 0xf8u),
            static_cast<float>(alpha) / 10.0f);
  r.matrix_mode(0x1700);   // GL_MODELVIEW (the projection matrix and the modelview matrix are popped back)
}

std::vector<CirclePoint> circle_points(Ctx &ctx, int32_t cx, int32_t cy, int32_t r) {
  // the radius in world units: (r << 10) * 0x51eb851f >> 32 >> 3 plus its sign bit (a division by 100 rounded toward zero)
  const int64_t prod = static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(r) << 10)) * 0x51eb851fLL;
  const int32_t s = static_cast<int32_t>(prod >> 32) >> 3;
  const int32_t radius = s + static_cast<int32_t>(static_cast<uint32_t>(s) >> 31);
  std::vector<CirclePoint> out;
  int32_t sx = r, sy = 0;   // worldToScreen writes them on every visible path; the slots start as the radius argument and 0
  uint32_t angle = 0;
  for (int k = 0; k < 25; ++k) {
    const int32_t x = add32(fold_range(ctx, static_cast<int32_t>(angle), radius), cx);
    const int32_t y = add32(abs_diff(ctx, static_cast<int32_t>(angle), radius), cy);
    const bool vis = world_to_screen(ctx, x, y, sx, sy, 0) != 0;
    out.push_back({sx, sy, vis});
    angle += 0x0aaaaaaau;
  }
  return out;
}

void body_draw_map_line(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  terrain_draw_line(g, ctx, "drawMapLine", static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
                    static_cast<int32_t>(a[3]), a[4], static_cast<int32_t>(a[5]), static_cast<int32_t>(a[6]));
}

void body_draw_minimap_segment(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  terrain_draw_line(g, ctx, "drawMinimapSegment", static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
                    static_cast<int32_t>(a[3]), a[4], static_cast<int32_t>(a[5]), static_cast<int32_t>(a[6]));
}

void body_set_line_point(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  OverlayState &s = state(g, ctx);
  s.px = static_cast<int32_t>(a[0]);
  s.py = static_cast<int32_t>(a[1]);
}

// lineTo(x, y, colour, width, alpha): drawMapLine(x, y, point x, point y, colour, width, alpha), then the point = (x, y)
void body_line_to(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  OverlayState &s = state(g, ctx);
  terrain_draw_line(g, ctx, "lineTo", static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), s.px, s.py, a[2],
                    static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]));
  if (mode() == 6) return;
  s.px = static_cast<int32_t>(a[0]);
  s.py = static_cast<int32_t>(a[1]);
}

// drawCircle(cx, cy, r, colour): after an invisible step the next visible one only sets the line point; the others draw
// lineTo(sx, sy, colour, 2, 7); the step that follows a visible one compares the previous sx with -1 (c3eh.cpp's `prev`)
void body_draw_circle(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const std::vector<CirclePoint> pts =
      circle_points(ctx, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
  int32_t prev = -1;
  for (const CirclePoint &p : pts) {
    if (!p.visible) {
      prev = -1;
      continue;
    }
    const uint32_t xy[2] = {static_cast<uint32_t>(p.sx), static_cast<uint32_t>(p.sy)};
    if (prev == -1) {
      body_set_line_point(g, ctx, xy, 2);
    } else {
      const uint32_t la[5] = {xy[0], xy[1], a[3], mode() == 7 ? 3u : 2u, 7u};
      body_line_to(g, ctx, la, 5);
    }
    prev = p.sx;
  }
}

}  // namespace overlay

namespace {

float float_bits(uint32_t v) {
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

// overlay.rasterLine(buf, w, h, x1, y1, x2, y2, colour, width, alpha, flags, prev_width): one drawLine into the RGB555 buffer of
// the address space at buf; returns the pixels written
void a_raster_line(Ctx &ctx, const uint32_t *a, Ret &ret) {
  const int32_t w = static_cast<int32_t>(a[1]), h = static_cast<int32_t>(a[2]);
  if (w <= 0 || h <= 0) throw Fault{a[0], 0};
  uint16_t *fb = reinterpret_cast<uint16_t *>(ctx.mem.at(a[0], static_cast<uint32_t>(w) * static_cast<uint32_t>(h) * 2u, true));
  overlay::LineGl gl;
  gl.flip_y = (a[10] & 1u) != 0;
  gl.context = (a[10] & 2u) == 0;
  gl.screen_w = w;
  gl.screen_h = h;
  gl.line_width = a[11] != 0 ? float_bits(a[11]) : 1.0f;
  const int64_t n = overlay::draw_line(fb, w, h, gl, static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]),
                                       static_cast<int32_t>(a[6]), a[7], static_cast<int32_t>(a[8]), static_cast<int32_t>(a[9]));
  ret.set_u32(static_cast<uint32_t>(n));
  uint32_t wb;
  std::memcpy(&wb, &gl.line_width, 4);
  ret.edx = wb;
}

// overlay.setRule(rule): the line rule (overlay::line_rule); eax = the rule before the call
void a_set_rule(Ctx &, const uint32_t *a, Ret &ret) {
  ret.set_u32(static_cast<uint32_t>(overlay::line_rule()));
  overlay::g_line_rule = static_cast<int>(a[0]);
}

// overlay.logStart(): the attached backend logs every Terrain::drawLine call from now on (eax 0 without a backend)
void a_log_start(Ctx &ctx, const uint32_t *, Ret &ret) {
  Gfx *g = gfx(ctx);
  if (g) g->state_of<overlay::CallLog>("terrain_overlay.log").on = true;
  ret.set_u32(g ? 1u : 0u);
}

// overlay.logWrite(): the log into the files overlay as `$gfx/overlay_calls.txt` (one line per call: caller, the four ends, the
// float bits of glLineWidth's and glColor4f's arguments, ctx = a GL context existed); eax = the lines
void a_log_write(Ctx &ctx, const uint32_t *, Ret &ret) {
  Gfx *g = gfx(ctx);
  if (!g) {
    ret.set_u32(0);
    return;
  }
  const overlay::CallLog &cl = g->state_of<overlay::CallLog>("terrain_overlay.log");
  gfxb::put_file(ctx, "$gfx/overlay_calls.txt", std::vector<uint8_t>(cl.text.begin(), cl.text.end()));
  ret.set_u32(static_cast<uint32_t>(std::count(cl.text.begin(), cl.text.end(), '\n')));
}

const FuncDef kFuncs[] = {
    {"overlay.rasterLine", "default", "uint32", 12, 0, a_raster_line, &g_negctl_terrain_overlay},
    {"overlay.setRule", "default", "uint32", 1, 0, a_set_rule, nullptr},
    {"overlay.logStart", "default", "uint32", 0, 0, a_log_start, nullptr},
    {"overlay.logWrite", "default", "uint32", 0, 0, a_log_write, nullptr},
};

}  // namespace

extern const ModuleDef kModule_terrain_overlay = {"terrain_overlay", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
