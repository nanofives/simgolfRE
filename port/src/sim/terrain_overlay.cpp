// Terrain::drawLine through the port's model of Windows' generic GL antialiased lines, and the exe's line drawers over it
// (roadmap S3.8). What the original does and what was measured: portsim/sim/terrain_overlay.h and log/roadmap/S3/8_notes.md.
//
// Module `terrain_overlay`: one test entry, overlay.rasterLine(buf, w, h, x1, y1, x2, y2, colour, width, alpha, flags, prev_width),
// draws one drawLine into a w x h RGB555 buffer of the address space at buf (flags bit 0 = the flip byte, bit 1 = no GL context;
// prev_width = the line width before the call as float bits, 0 = GL's default 1.0) and returns the pixels written. The gfx bodies
// (drawMapLine, drawMinimapSegment, setLinePoint, lineTo, drawCircle) are listed in gfx_terrain.cpp's family table.
#include "portsim/sim/terrain_overlay.h"

#include <algorithm>
#include <cmath>
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

// the 4x4 ordered dither of the generic GL's 16-bit write (gl_raster.cpp quantize, fitted on the terrain frames)
constexpr int kDither[4][4] = {{15, 7, 13, 5}, {3, 11, 1, 9}, {12, 4, 14, 6}, {0, 8, 2, 10}};
int quantize(double c, int32_t x, int32_t y) {
  const double v = c * 31.0 / 255.0 + (kDither[y & 3][x & 3] + 0.5) / 16.0;
  const int o = static_cast<int>(std::floor(v));
  return o < 0 ? 0 : (o > 31 ? 31 : o);
}

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

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
  draw_line(g.root.px.data(), g.root.w, g.root.h, gl, x1, y1, x2, y2, colour, width, alpha);
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
  const double sx = static_cast<double>(fb_w) / gl.screen_w, sy = static_cast<double>(fb_h) / gl.screen_h;
  double ax = x1 * sx, bx = x2 * sx;
  double ay = flip ? y1 * sy : (gl.screen_h - static_cast<double>(y1)) * sy;
  double by = flip ? y2 * sy : (gl.screen_h - static_cast<double>(y2)) * sy;
  if (mode() != 3 && !clip(ax, ay, bx, by, fb_w, fb_h)) return 0;
  const double cx = fb_w / 2.0, cy = fb_h / 2.0;
  ax = snap(ax, cx, cx);
  bx = snap(bx, cx, cx);
  ay = snap(ay, cy, cy);
  by = snap(by, cy, cy);
  const double dx = bx - ax, dy = by - ay, len = std::sqrt(dx * dx + dy * dy);
  if (len == 0.0) return 0;
  const double ux = dx / len, uy = dy / len, half = wd / 2.0;
  const double reach = half + 1.5;
  const int32_t x0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ax, bx) - reach)));
  const int32_t xe = std::min(fb_w - 1, static_cast<int32_t>(std::ceil(std::max(ax, bx) + reach)));
  const int32_t y0 = std::max(0, static_cast<int32_t>(std::floor(std::min(ay, by) - reach)));
  const int32_t ye = std::min(fb_h - 1, static_cast<int32_t>(std::ceil(std::max(ay, by) + reach)));
  int64_t written = 0;
  for (int32_t y = y0; y <= ye; ++y) {
    for (int32_t x = x0; x <= xe; ++x) {
      const double px = x + 0.5 - ax, py = y + 0.5 - ay;
      const double t = px * ux + py * uy, n = -px * uy + py * ux;
      double cov;
      if (mode() == 1)
        cov = (std::fabs(n) <= half && t >= 0.0 && t <= len) ? 1.0 : 0.0;
      else
        cov = clamp01(half + 0.5 - std::fabs(n)) * clamp01(t + 0.5) * clamp01(len - t + 0.5);
      if (cov <= 0.0) continue;
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
    {"overlay.logStart", "default", "uint32", 0, 0, a_log_start, nullptr},
    {"overlay.logWrite", "default", "uint32", 0, 0, a_log_write, nullptr},
};

}  // namespace

extern const ModuleDef kModule_terrain_overlay = {"terrain_overlay", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
