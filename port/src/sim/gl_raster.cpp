// The port's software rasteriser (phase 5 step 3). See portsim/sim/gl_raster.h. Module file only because every file of
// src/sim is a module; it registers no function (the C entry points are sg_raster_* in portsim_c.h).
#define _CRT_SECURE_NO_WARNINGS   // the development switch SG_RASTER_TUNE uses getenv
#include "portsim/sim/gl_raster.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>   // the workers take the priority of the thread that feeds them (GlRaster::set_threads)
#endif

#include "portsim/registry.h"
#include "portsim/registry_list.h"

namespace portsim {
namespace gl {

namespace {
constexpr uint32_t kModelview = 0x1700, kProjection = 0x1701;
constexpr uint32_t kCullFace = 0x0B44, kBlendCap = 0x0BE2;
constexpr uint32_t kGlRgb = 0x1907, kGlRgba = 0x1908;
constexpr uint32_t kNearest = 0x2600, kLinear = 0x2601;

float bits_f(uint32_t b) {
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}
double bits_d(uint32_t lo, uint32_t hi) {
  const uint64_t q = (static_cast<uint64_t>(hi) << 32) | lo;
  double d;
  std::memcpy(&d, &q, 8);
  return d;
}
double edge(double ax, double ay, double bx, double by, double px, double py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}
// The write to a 16-bit RGB555 surface: GL_DITHER is on by default and Windows' generic implementation dithers with a 4x4
// ordered matrix. Fitted to the original's frames (pre-quantisation colours against the written 5-bit values, all three
// channels, 4 passes): out = floor(c * 31 / 255 + (k + 0.5) / 16), k from this table indexed by (window y & 3, x & 3).
constexpr int kDither[4][4] = {{15, 7, 13, 5}, {3, 11, 1, 9}, {12, 4, 14, 6}, {0, 8, 2, 10}};
int quantize(double c, int x, int y, int ctl) {
  const double d = ctl == 1 ? 0.5 : ctl == 2 ? kDither[y & 3][x & 3] / 16.0 : (kDither[y & 3][x & 3] + 0.5) / 16.0;
  const double v = c * 31.0 / 255.0 + d;
  const int o = static_cast<int>(std::floor(v));
  return o < 0 ? 0 : (o > 31 ? 31 : o);
}
// TRIANGLE SETUP (U-1076, re/PHASE5_TERRAIN_PLAN.md section 18): the generic GL sets up coverage and attributes from vertex window
// coordinates in fixed point with 8 fractional bits, rounded to nearest, of a viewport whose half-size is h - 2^-8:
// x' = round(256 * (c + (x - c) * (1 - 2^-8 / h))) / 256 (c, h: the viewport's centre and half-size, per axis), and samples at the
// pixel centres. Measured with probes of the real software GL (a per-vertex offset of exactly 1/256 px where |ndc| >= 0.5 for
// integer vertices, the mean of a linear ramp for fractional ones). The older model below (tune 26 = 1) kept exact edges and
// sampled at x' = c + (x - c) * (1 + e), the straight-line fit of that staircase: e = 1.046e-5 in x, 1.355e-5 in y (glshift_fit.py);
// tune[6] / tune[15] override e in units of 1e-9 (0 = the fitted constant, -1 = none).
// Since roadmap S3.21 the snapped positions come from the generic GL's own float32 vertex arithmetic (VERTEX RULE in emit); the
// scaled double model above is what `#tune 28 1` restores. Its residue sat on the vertices within a float ulp of a 2^-8 tie (U-1392).
constexpr double kAttrScaleX = 1.046e-5, kAttrScaleY = 1.355e-5;
// SPECULAR (U-1300, log/roadmap/S3/1b_notes.md): the generic GL does not evaluate (n.h)^shininess; it reads a table of 256 entries
// spanning [t, 1], t = 0.0005^(1 / shininess) (below t the term is 0), at the entry NEAREST to n.h, without interpolation:
// i = round((n.h - t) * 255 / (1 - t)), value (t + i * (1 - t) / 255)^shininess. Measured with untextured constant-colour quads of
// the real software GL (port/test/glprobe.py untex, white specular only): 0 of 4,923 quads outside the interval their 16 dither
// cells allow, against 793 for the exact pow; shininess 13 is the terrain's (initGL's glMateriali). GlRaster::spec_lookup_.
int wrap_repeat(int64_t i, uint32_t n) {
  const int64_t m = i % static_cast<int64_t>(n);
  return static_cast<int>(m < 0 ? m + n : m);
}

// ---- helpers of the fast pixel loop (GlRaster::fast_triangle_, roadmap S3.10): each returns exactly what the general loop's
// expression returns, without its CRT call or 64-bit division
// static_cast<int>(std::floor(v)): the truncation corrected by one below zero, std::floor itself outside +-2^31 and for NaN
inline int floor_int(double v) {
  if (!(v > -2147483648.0 && v < 2147483648.0)) return static_cast<int>(std::floor(v));
  const int i = static_cast<int>(v);
  return v < static_cast<double>(i) ? i - 1 : i;
}
// static_cast<int64_t>(std::floor(v)), the same way (a truncated double is an integer the double holds exactly)
inline int64_t floor_i64(double v) {
  if (!(v > -4.0e18 && v < 4.0e18)) return static_cast<int64_t>(std::floor(v));
  const int64_t i = static_cast<int64_t>(v);
  return v < static_cast<double>(i) ? i - 1 : i;
}
// wrap_repeat for an index already in [0, n) without the division
inline int wrap_fast(int64_t i, uint32_t n) { return static_cast<uint64_t>(i) < n ? static_cast<int>(i) : wrap_repeat(i, n); }
// floor(a / b) and ceil(a / b) for b > 0
inline int64_t floor_div(int64_t a, int64_t b) {
  const int64_t q = a / b;
  return (a % b != 0 && a < 0) ? q - 1 : q;
}
inline int64_t ceil_div(int64_t a, int64_t b) {
  const int64_t q = a / b;
  return (a % b != 0 && a > 0) ? q + 1 : q;
}
// The textured walk's final stage (tune 32, roadmap S3.29): the generic GL filters, modulates and dithers a walked pixel in float32.
// Measured with the boundary probes log/roadmap/S3/29_probes.py fin_near / fin_lin (each quad's exact value on a 5-bit boundary for
// one dither cell, so the arithmetic decides it): texel = byte / 255.0f (a float division; byte * (1 / 255) gave 784 / 2,144 wrong
// px, the double model 10,064 / 9,792, this form 0 / 0 of 371,712); GL_LINEAR weights (1 - a)(1 - b), a (1 - b), (1 - a) b, a b in
// float32 from a = frac(u - 0.5) of the float32 u, summed in texel order (j0 i0, j0 i1, j1 i0, j1 i1); the 5-bit value = colour x 31
// (float32, wacc[2..4]) times the texel, plus the dither offset (k + 0.5) / 16, floored and clamped.
inline uint16_t walk_final(const uint8_t *td, uint32_t tw, uint32_t th, int bpp, bool linear, const float wacc[5], float d) {
  float tx[3];
  if (!linear) {
    const int iu = wrap_repeat(static_cast<int64_t>(std::floor(static_cast<double>(wacc[0]))), tw);
    const int iv = wrap_repeat(static_cast<int64_t>(std::floor(static_cast<double>(wacc[1]))), th);
    const uint8_t *tp = &td[(static_cast<size_t>(iv) * tw + iu) * bpp];
    for (int k = 0; k < 3; ++k) tx[k] = static_cast<float>(tp[k]) / 255.0f;
  } else {
    const float fu = wacc[0] - 0.5f, fv = wacc[1] - 0.5f;
    const float fi = std::floor(fu), fj = std::floor(fv);
    const float ax = fu - fi, ay = fv - fj;
    const int64_t i0 = static_cast<int64_t>(fi), j0 = static_cast<int64_t>(fj);
    const size_t x0 = static_cast<size_t>(wrap_repeat(i0, tw)), x1 = static_cast<size_t>(wrap_repeat(i0 + 1, tw));
    const size_t y0 = static_cast<size_t>(wrap_repeat(j0, th)) * tw, y1 = static_cast<size_t>(wrap_repeat(j0 + 1, th)) * tw;
    const float oma = 1.0f - ax, omb = 1.0f - ay;
    const float m0 = oma * omb, m1 = ax * omb, m2 = oma * ay, m3 = ax * ay;
    const uint8_t *q0 = &td[(y0 + x0) * bpp], *q1 = &td[(y0 + x1) * bpp], *q2 = &td[(y1 + x0) * bpp], *q3 = &td[(y1 + x1) * bpp];
    for (int k = 0; k < 3; ++k) {
      const float p0 = m0 * (static_cast<float>(q0[k]) / 255.0f), p1 = m1 * (static_cast<float>(q1[k]) / 255.0f);
      const float p2 = m2 * (static_cast<float>(q2[k]) / 255.0f), p3 = m3 * (static_cast<float>(q3[k]) / 255.0f);
      float acc = p0 + p1;
      acc = acc + p2;
      acc = acc + p3;
      tx[k] = acc;
    }
  }
  int o5[3];
  for (int k = 0; k < 3; ++k) {
    const float val = wacc[2 + k] * tx[k];
    const float q = val + d;
    const int o = static_cast<int>(std::floor(q));
    o5[k] = o < 0 ? 0 : (o > 31 ? 31 : o);
  }
  return static_cast<uint16_t>((o5[0] << 10) | (o5[1] << 5) | o5[2]);
}
// the dither offsets quantize() adds for control 0 (the correct pipeline), 1 and 2, and dst * 255.0 / 31.0 for a 5-bit dst, each
// computed by the same expression as there
struct FastTables {
  double dither[3][4][4];
  double expand5[32];
  FastTables() {
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 4; ++x) {
        dither[0][y][x] = (kDither[y][x] + 0.5) / 16.0;
        dither[1][y][x] = 0.5;
        dither[2][y][x] = kDither[y][x] / 16.0;
      }
    for (int d = 0; d < 32; ++d) expand5[d] = d * 255.0 / 31.0;
  }
};
const FastTables &fast_tables() {
  static const FastTables t;
  return t;
}
// quantize() with the dither offset d already chosen
inline int quantize_d(double c, double d) {
  const int o = floor_int(c * 31.0 / 255.0 + d);
  return o < 0 ? 0 : (o > 31 ? 31 : o);
}
}  // namespace

GlRaster::GlRaster(int width, int height) : w_(width), h_(height), fb_(static_cast<size_t>(width) * height, 0) {
  std::memset(mv_.m, 0, sizeof mv_.m);
  std::memset(proj_.m, 0, sizeof proj_.m);
  mv_.m[0] = mv_.m[5] = mv_.m[10] = mv_.m[15] = 1.0;
  proj_ = mv_;
  std::memset(fmv_.m, 0, sizeof fmv_.m);
  fmv_.m[0] = fmv_.m[5] = fmv_.m[10] = fmv_.m[15] = 1.0f;
  fproj_ = fmv_;
  fmvp_ = fmv_;
  // SG_RASTER_TUNE=<i>=<v>[,<i>=<v>...]: tune knobs for every rasteriser of the process (development and negative controls only,
  // e.g. `28=1,29=1` puts the pre-S3.21 vertex model and no clipping back on the live terrain path; unset = every knob 0)
  if (const char *env = std::getenv("SG_RASTER_TUNE")) {
    const char *p = env;
    while (*p) {
      char *e = nullptr;
      const long i = std::strtol(p, &e, 10);
      if (e == p || *e != '=') break;
      const char *q = e + 1;
      const long v = std::strtol(q, &e, 10);
      if (e == q) break;
      if (i >= 0 && i < kTunes) tune[i] = static_cast<int>(v);
      p = *e == ',' ? e + 1 : e;
      if (e == q || (*e != ',' && *e != 0)) break;
    }
  }
  vp_w_ = width;
  vp_h_ = height;
}

// ------------------------------------------------------------------------------------------------ matrices
void GlRaster::mul(const Mat &b) {
  Mat &a = cur();
  Mat c;
  for (int j = 0; j < 4; ++j)
    for (int i = 0; i < 4; ++i) {
      double s = 0;
      for (int k = 0; k < 4; ++k) s += a.m[i + 4 * k] * b.m[k + 4 * j];
      c.m[i + 4 * j] = (tune[5] & 1) ? static_cast<double>(static_cast<float>(s)) : s;
    }
  a = c;
}
// FLOAT32 MATRICES (roadmap S3.21, U-1392): measured with feedback-mode probes of the real software GL (log/roadmap/S3/21_notes.md):
// every product and sum of the matrix arithmetic is a float32 operation, a product's four terms summed in order k = 0..3 (so a
// translation column is x * m0 + y * m1 + z * m2 + m3), and the MVP is the modelview operations applied to the MVP itself.
// Volatile stores keep each operation a separately rounded float on every compiler setting.
void GlRaster::fmul_(const FMat &a, const FMat &b, FMat &r) {
  FMat c;
  for (int j = 0; j < 4; ++j)
    for (int i = 0; i < 4; ++i) {
      volatile float s = a.m[i] * b.m[4 * j];
      for (int k = 1; k < 4; ++k) {
        volatile float p = a.m[i + 4 * k] * b.m[k + 4 * j];
        s = s + p;
      }
      c.m[i + 4 * j] = s;
    }
  r = c;
}
void GlRaster::fpost_(const FMat &b) {
  if (mode_ == kProjection) {
    fmul_(fproj_, b, fproj_);
    fmul_(fproj_, fmv_, fmvp_);
  } else {
    fmul_(fmv_, b, fmv_);
    fmul_(fmvp_, b, fmvp_);
  }
}
void GlRaster::load_identity() {
  Mat &a = cur();
  std::memset(a.m, 0, sizeof a.m);
  a.m[0] = a.m[5] = a.m[10] = a.m[15] = 1.0;
  FMat &f = mode_ == kProjection ? fproj_ : fmv_;
  std::memset(f.m, 0, sizeof f.m);
  f.m[0] = f.m[5] = f.m[10] = f.m[15] = 1.0f;
  if (mode_ == kProjection) fmul_(fproj_, fmv_, fmvp_);
  else fmvp_ = fproj_;
}
void GlRaster::push_matrix() {
  stack_[mode_ == kProjection ? 1 : 0].push_back(cur());
  if (mode_ == kProjection) {
    fstack_[1].push_back(fproj_);
  } else {
    fstack_[0].push_back(fmv_);
    fstack_mvp_.push_back(fmvp_);
  }
}
void GlRaster::pop_matrix() {
  auto &st = stack_[mode_ == kProjection ? 1 : 0];
  if (st.empty()) return;
  cur() = st.back();
  st.pop_back();
  if (mode_ == kProjection) {
    if (!fstack_[1].empty()) { fproj_ = fstack_[1].back(); fstack_[1].pop_back(); }
    fmul_(fproj_, fmv_, fmvp_);
  } else if (!fstack_[0].empty()) {
    fmv_ = fstack_[0].back(); fstack_[0].pop_back();
    fmvp_ = fstack_mvp_.back(); fstack_mvp_.pop_back();
  }
}
void GlRaster::matrix_mode(uint32_t mode) { mode_ = mode; }
void GlRaster::rotated(double angle, double x, double y, double z) {
  const double n = std::sqrt(x * x + y * y + z * z);
  if (n == 0) return;
  {
    // the float32 rotation (roadmap S3.21): the axis normalised in float, cos / sin of the angle in radians rounded to float,
    // the diagonal as the Red Book's u u^T + cos (I - u u^T): x^2 + (1 - x^2) c (a probe with a general axis needs that form: the
    // (1 - c) x^2 + c form is one float ulp off), the off-diagonal terms (1 - c) x y -+ s z
    volatile float fx = static_cast<float>(x), fy = static_cast<float>(y), fz = static_cast<float>(z);
    volatile float xx = fx * fx, yy = fy * fy, zz = fz * fz;
    volatile float n2 = xx + yy;
    n2 = n2 + zz;
    const float fn = static_cast<float>(std::sqrt(static_cast<double>(n2)));
    if (fn != 1.0f) { fx = fx / fn; fy = fy / fn; fz = fz / fn; xx = fx * fx; yy = fy * fy; zz = fz * fz; }
    const double rad = angle * 3.14159265358979323846 / 180.0;
    const float c = static_cast<float>(std::cos(rad)), s = static_cast<float>(std::sin(rad));
    volatile float t = 1.0f - c;
    auto diag = [c](float q) { volatile float a = 1.0f - q; volatile float b = a * c; volatile float r = q + b; return static_cast<float>(r); };
    auto off = [](float t_, float p, float q, float sv, float w, bool plus) {
      volatile float a = t_ * p; volatile float b = a * q; volatile float d = sv * w;
      volatile float r = plus ? b + d : b - d;
      return static_cast<float>(r);
    };
    FMat r;
    const float v[16] = {diag(xx),                 off(t, fx, fy, s, fz, true),  off(t, fx, fz, s, fy, false), 0,
                         off(t, fx, fy, s, fz, false), diag(yy),                 off(t, fy, fz, s, fx, true),  0,
                         off(t, fx, fz, s, fy, true),  off(t, fy, fz, s, fx, false), diag(zz),                 0,
                         0, 0, 0, 1};
    std::memcpy(r.m, v, sizeof v);
    fpost_(r);
  }
  x /= n; y /= n; z /= n;
  const double rad = angle * 3.14159265358979323846 / 180.0, c = std::cos(rad), s = std::sin(rad), t = 1 - c;
  Mat r;
  const double v[16] = {t * x * x + c,     t * x * y + s * z, t * x * z - s * y, 0,
                        t * x * y - s * z, t * y * y + c,     t * y * z + s * x, 0,
                        t * x * z + s * y, t * y * z - s * x, t * z * z + c,     0,
                        0, 0, 0, 1};
  std::memcpy(r.m, v, sizeof v);
  mul(r);
}
void GlRaster::rotatef(float angle, float x, float y, float z) { rotated(angle, x, y, z); }
void GlRaster::translatef(float x, float y, float z) {
  Mat r;
  std::memset(r.m, 0, sizeof r.m);
  r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
  r.m[12] = x; r.m[13] = y; r.m[14] = z;
  mul(r);
  FMat f;
  std::memset(f.m, 0, sizeof f.m);
  f.m[0] = f.m[5] = f.m[10] = f.m[15] = 1.0f;
  f.m[12] = x; f.m[13] = y; f.m[14] = z;
  fpost_(f);
}
void GlRaster::ortho(double l, double r, double b, double t, double n, double f) {
  Mat o;
  std::memset(o.m, 0, sizeof o.m);
  o.m[0] = 2.0 / (r - l);
  o.m[5] = 2.0 / (t - b);
  o.m[10] = -2.0 / (f - n);
  o.m[12] = -(r + l) / (r - l);
  o.m[13] = -(t + b) / (t - b);
  o.m[14] = -(f + n) / (f - n);
  o.m[15] = 1.0;
  mul(o);
  FMat fo;   // the elements in double, stored as float (a probe of the terrain's projection: float operands give the same values)
  for (int i = 0; i < 16; ++i) fo.m[i] = static_cast<float>(o.m[i]);
  fpost_(fo);
}
void GlRaster::viewport(int32_t x, int32_t y, int32_t w, int32_t h) { vp_x_ = x; vp_y_ = y; vp_w_ = w; vp_h_ = h; }

// ------------------------------------------------------------------------------------------------ state
void GlRaster::enable(uint32_t cap) {
  if (cap == 0x0DE1) texture2d_ = true;
  else if (cap == kCullFace) cull_ = true;
  else if (cap == kBlendCap) blend_ = true;
  else if (cap == 0x0B50) lighting_ = true;
  else if (cap == 0x4000) light0_ = true;
}
void GlRaster::disable(uint32_t cap) {
  if (cap == 0x0DE1) texture2d_ = false;
  else if (cap == kCullFace) cull_ = false;
  else if (cap == kBlendCap) blend_ = false;
  else if (cap == 0x0B50) lighting_ = false;
  else if (cap == 0x4000) light0_ = false;
}
void GlRaster::front_face(uint32_t mode) { front_ = mode; }
void GlRaster::blend_func(uint32_t src, uint32_t dst) { blend_src_ = src; blend_dst_ = dst; }
void GlRaster::tex_param_(uint32_t pname, uint32_t value) {
  Tex &t = textures_[bound_];
  if (pname == 0x2800) t.mag = value;
  else if (pname == 0x2801) t.min = value;
  else if (pname == 0x2802) t.wrap_s = value;
  else if (pname == 0x2803) t.wrap_t = value;
}
void GlRaster::tex_env_(uint32_t target, uint32_t pname, uint32_t value) {
  // glTexEnvi(GL_TEXTURE_2D, ...) as the original calls it is GL_INVALID_ENUM and changes nothing: the mode stays GL_MODULATE
  if (target == 0x2300 && pname == 0x2200) env_mode_ = value;
}
void GlRaster::light(uint32_t light, uint32_t pname, const float *v) {
  if (!ctx_ || light != 0x4000) return;
  if (pname == 0x1200) std::memcpy(light_[0], v, 16);
  else if (pname == 0x1201) std::memcpy(light_[1], v, 16);
  else if (pname == 0x1202) std::memcpy(light_[2], v, 16);
  else if (pname == 0x1203) {   // the position / direction is transformed by the modelview matrix at the time of the call
    const double *m = mv_.m;
    for (int i = 0; i < 4; ++i) light_[3][i] = static_cast<float>(m[i] * v[0] + m[4 + i] * v[1] + m[8 + i] * v[2] + m[12 + i] * v[3]);
  }
}
void GlRaster::material(uint32_t, uint32_t pname, const float *v) {
  if (ctx_ && pname == 0x1202) std::memcpy(mat_spec_, v, 16);
}
void GlRaster::material_shininess(uint32_t, int32_t sh) {
  if (ctx_) shininess_ = static_cast<float>(sh);
}
void GlRaster::tex_parameteri(uint32_t, uint32_t pname, int32_t v) {
  if (ctx_) tex_param_(pname, static_cast<uint32_t>(v));
}
void GlRaster::tex_parameterf(uint32_t, uint32_t pname, float v) {
  if (ctx_) tex_param_(pname, static_cast<uint32_t>(v));   // the filters arrive as floats
}
void GlRaster::tex_envi(uint32_t target, uint32_t pname, int32_t v) {
  if (ctx_) tex_env_(target, pname, static_cast<uint32_t>(v));
}
bool GlRaster::gen_textures(int32_t n, uint32_t *names) {
  if (!ctx_) return false;
  for (int32_t i = 0; i < n; ++i) {   // the lowest name not in use (gl_sink.h GlStreamSink::take_name)
    uint32_t k = 1;
    while (used_names_.count(k)) ++k;
    used_names_.insert(k);
    names[i] = k;
  }
  return true;
}
void GlRaster::color4f(float r, float g, float b, float a) { cur_c_[0] = r; cur_c_[1] = g; cur_c_[2] = b; cur_c_[3] = a; }
void GlRaster::normal3f(float x, float y, float z) { cur_n_[0] = x; cur_n_[1] = y; cur_n_[2] = z; }
void GlRaster::tex_image2d(uint32_t, int32_t level, int32_t, int32_t w, int32_t h, int32_t, uint32_t format, uint32_t, const uint8_t *data) {
  finish();   // queued triangles read the texel bytes this may replace
  if (!ctx_ || level != 0 || w <= 0 || h <= 0 || !data) return;
  const uint32_t width = static_cast<uint32_t>(w), height = static_cast<uint32_t>(h);
  Tex &t = textures_[bound_];
  t.w = width;
  t.h = height;
  t.fmt = format;
  const size_t n = static_cast<size_t>(width) * height * (format == kGlRgba ? 4 : 3);
  t.data.assign(data, data + n);
}
void GlRaster::delete_textures(int32_t n, const uint32_t *ids) {
  finish();
  if (!ctx_) return;
  for (int i = 0; i < n; ++i) {
    textures_.erase(ids[i]);
    used_names_.erase(ids[i]);
  }
}
void GlRaster::bind_texture(uint32_t, uint32_t id) {
  if (ctx_) bound_ = id;
}
void GlRaster::set_arrays(const float *v, size_t nv, const float *n, size_t nn) {
  varr_.assign(v, v + nv * 3);
  narr_.assign(n, n + nn * 3);
}
void GlRaster::flush() { finish(); }
void GlRaster::tex_coord2fv(const float *v) { cur_s_ = v[0]; cur_t_ = v[1]; }
void GlRaster::tex_coord2f(float s, float t) { cur_s_ = s; cur_t_ = t; }
void GlRaster::normal3fv(const float *v) { cur_n_[0] = v[0]; cur_n_[1] = v[1]; cur_n_[2] = v[2]; }

// ------------------------------------------------------------------------------------------------ primitives
void GlRaster::begin(uint32_t mode) {
  prim_ = mode;
  in_begin_ = true;
  verts_.clear();
}
// the specular table (SPECULAR above): t = 0.0005^(1 / shininess), entry i = (t + i / scale)^shininess, scale = 255 / (1 - t); the
// 256 entries are computed once per shininess by the same expressions (roadmap S3.10)
double GlRaster::spec_lookup_(double nh, double shininess) {
  if (shininess != spec_sh_) {
    spec_sh_ = shininess;
    spec_t_ = std::pow(0.0005, 1.0 / shininess);
    spec_scale_ = 255.0 / (1.0 - spec_t_);
    for (int i = 0; i < 256; ++i) spec_tab_[i] = std::pow(spec_t_ + static_cast<double>(i) / spec_scale_, shininess);
  }
  if (nh <= spec_t_) return 0.0;
  const double i = std::min(255.0, std::floor((nh - spec_t_) * spec_scale_ + 0.5));
  return spec_tab_[static_cast<int>(i)];
}

void GlRaster::emit(double x, double y, double z, const float *normal) {
  // object -> eye -> clip (orthographic here, but the divide is done) -> window
  const double *m = mv_.m, *p = proj_.m;
  const bool g32 = (tune[5] & 2) != 0;   // experiment: float32 geometry (every intermediate rounded to single precision)
  auto G = [g32](double x) { return g32 ? static_cast<double>(static_cast<float>(x)) : x; };
  const double ex = G(G(G(G(m[0] * x) + G(m[4] * y)) + G(m[8] * z)) + m[12]);
  const double ey = G(G(G(G(m[1] * x) + G(m[5] * y)) + G(m[9] * z)) + m[13]);
  const double ez = G(G(G(G(m[2] * x) + G(m[6] * y)) + G(m[10] * z)) + m[14]);
  const double ew = G(G(G(G(m[3] * x) + G(m[7] * y)) + G(m[11] * z)) + m[15]);
  const double cx = G(G(G(G(p[0] * ex) + G(p[4] * ey)) + G(p[8] * ez)) + G(p[12] * ew));
  const double cy = G(G(G(G(p[1] * ex) + G(p[5] * ey)) + G(p[9] * ez)) + G(p[13] * ew));
  const double cz = G(G(G(G(p[2] * ex) + G(p[6] * ey)) + G(p[10] * ez)) + G(p[14] * ew));
  const double cw = G(G(G(G(p[3] * ex) + G(p[7] * ey)) + G(p[11] * ez)) + G(p[15] * ew));
  Vtx v;
  v.x = G(vp_x_ + G(G(G(cx / cw) + 1.0) * 0.5) * vp_w_);
  v.y = G(vp_y_ + G(G(G(cy / cw) + 1.0) * 0.5) * vp_h_);
  v.z = (cz / cw + 1.0) * 0.5;
  v.qx = v.qy = 0;
  if (tune[28] == 0) {
    // VERTEX RULE (roadmap S3.21, U-1392; measured against the real GL's feedback points, log/roadmap/S3/21_notes.md): the object
    // coordinates as floats through the float32 MVP, the translation added FIRST (m3 + x m0 + y m1 + z m2, each operation rounded
    // to float), then window = ndc * (half-size - 2^-8) rounded to float, plus the centre, rounded half to even to 2^-8 in one step
    // (a biased float add). Matches every vertex of the recorded passes (21_vcheck_all.log); the double model above misses the
    // vertices whose window coordinate sits within a float ulp of a 2^-8 rounding tie.
    const float *M = fmvp_.m;
    const float o[3] = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    float clip[4];
    for (int r = 0; r < 4; ++r) {
      volatile float acc = M[12 + r];
      for (int k = 0; k < 3; ++k) {
        volatile float term = o[k] * M[r + 4 * k];
        acc = acc + term;
      }
      clip[r] = acc;
    }
    std::memcpy(v.c, clip, sizeof clip);
    window_of_(clip, v);
  }
  v.s = cur_s_;
  v.t = cur_t_;
  v.r = cur_c_[0]; v.g = cur_c_[1]; v.b = cur_c_[2]; v.a = cur_c_[3];
  const bool f32 = tune[12] != 0;   // experiment: round every lighting intermediate to float32
  auto Q = [f32](double x) { return f32 ? static_cast<double>(static_cast<float>(x)) : x; };
  if (lighting_) {
    // eye-space normal = the inverse transpose of the modelview's upper 3x3 applied to the object normal
    // (computed once per modelview: the same expressions of the same nine values give the same inverse, roadmap S3.10)
    const double key[9] = {m[0], m[4], m[8], m[1], m[5], m[9], m[2], m[6], m[10]};
    if (!inv_ok_ || std::memcmp(key, inv_key_, sizeof key) != 0) {
      const double a0 = key[0], a1 = key[1], a2 = key[2], b0 = key[3], b1 = key[4], b2 = key[5], c0 = key[6], c1 = key[7], c2 = key[8];
      const double det = a0 * (b1 * c2 - b2 * c1) - a1 * (b0 * c2 - b2 * c0) + a2 * (b0 * c1 - b1 * c0);
      inv_[0] = (b1 * c2 - b2 * c1) / det; inv_[1] = (a2 * c1 - a1 * c2) / det; inv_[2] = (a1 * b2 - a2 * b1) / det;
      inv_[3] = (b2 * c0 - b0 * c2) / det; inv_[4] = (a0 * c2 - a2 * c0) / det; inv_[5] = (a2 * b0 - a0 * b2) / det;
      inv_[6] = (b0 * c1 - b1 * c0) / det; inv_[7] = (a1 * c0 - a0 * c1) / det; inv_[8] = (a0 * b1 - a1 * b0) / det;
      std::memcpy(inv_key_, key, sizeof key);
      inv_ok_ = true;
    }
    const double i00 = inv_[0], i01 = inv_[1], i02 = inv_[2], i10 = inv_[3], i11 = inv_[4], i12 = inv_[5];
    const double i20 = inv_[6], i21 = inv_[7], i22 = inv_[8];
    // (M^-1)^T n: row j of the transpose is column j of the inverse
    const double nx = Q(i00 * normal[0] + i10 * normal[1] + i20 * normal[2]);
    const double ny = Q(i01 * normal[0] + i11 * normal[1] + i21 * normal[2]);
    const double nz = Q(i02 * normal[0] + i12 * normal[1] + i22 * normal[2]);
    double col[3];
    for (int k = 0; k < 3; ++k) col[k] = 0.2 * 0.2;   // the global ambient (0.2) times the material ambient (0.2), both GL defaults
    if (light0_) {
      double lx = light_[3][0], ly = light_[3][1], lz = light_[3][2];
      const double ln = std::sqrt(lx * lx + ly * ly + lz * lz);
      if (ln > 0) { lx /= ln; ly /= ln; lz /= ln; }
      const double ndotl = Q(std::max(0.0, nx * lx + ny * ly + nz * lz));
      for (int k = 0; k < 3; ++k) col[k] += light_[0][k] * 0.2 + ndotl * light_[1][k] * 0.8;   // ambient and diffuse (material defaults)
      if (ndotl > 0 && tune[11] != 8) {
        double hx = lx, hy = ly, hz = lz + 1.0;   // the half vector with a viewer at infinity on +z
        const double hn = std::sqrt(hx * hx + hy * hy + hz * hz);
        if (hn > 0) { hx /= hn; hy /= hn; hz /= hn; }
        const double ndoth = Q(std::max(0.0, nx * hx + ny * hy + nz * hz));
        double nh = ndoth;
        if (tune[7] != 0) {   // experiment: a look-up table of the specular exponent with |tune[7]| entries (negative: floor)
          const double n = static_cast<double>(tune[7] < 0 ? -tune[7] : tune[7]);
          nh = (tune[7] < 0 ? std::floor(nh * n) : std::floor(nh * n + 0.5)) / n;
        }
        double f;
        if (tune[7] == 0 && tune[27] == 0 && shininess_ > 0) f = spec_lookup_(nh, shininess_);   // tune 27 = 1: the exact pow
        else f = Q(nh > 0 ? std::pow(nh, shininess_) : 0.0);
        for (int k = 0; k < 3; ++k) col[k] += f * light_[2][k] * mat_spec_[k];
      }
    }
    v.r = std::min(1.0, std::max(0.0, col[0]));
    v.g = std::min(1.0, std::max(0.0, col[1]));
    v.b = std::min(1.0, std::max(0.0, col[2]));
    if (tune[4] > 0) {
      const double q = static_cast<double>((1ull << tune[4]) - 1);
      v.r = std::floor(v.r * q + 0.5) / q;
      v.g = std::floor(v.g * q + 0.5) / q;
      v.b = std::floor(v.b * q + 0.5) / q;
    }
  }
  verts_.push_back(v);
}
void GlRaster::window_of_(const float c[4], Vtx &v) const {
  float nx = c[0], ny = c[1];
  if (c[3] != 1.0f) {   // not measured: the terrain's projections are orthographic (w = 1)
    volatile float inv = 1.0f / c[3];
    volatile float a = c[0] * inv, b = c[1] * inv;
    nx = a; ny = b;
  }
  const double hx = vp_w_ * 0.5, hy = vp_h_ * 0.5;
  volatile float px = nx * static_cast<float>(hx - 1.0 / 256.0), py = ny * static_cast<float>(hy - 1.0 / 256.0);
  const double wx = static_cast<double>(px) + (vp_x_ + hx), wy = static_cast<double>(py) + (vp_y_ + hy);
  v.x = wx;
  v.y = wy;
  v.qx = std::nearbyint(256.0 * wx) / 256.0;
  v.qy = std::nearbyint(256.0 * wy) / 256.0;
}

// FRUSTUM CLIPPING (roadmap S3.21, U-1392; measured against the real GL's triangle feedback, log/roadmap/S3/21_notes.md): a triangle
// with a vertex outside -w <= x, y, z <= w is culled first (on its snapped window positions; a zero area is not a back face), then
// clipped in clip space against x >= -w, x <= w, y >= -w, y <= w, z >= -w, z <= w in that order, Sutherland-Hodgman with the
// classic output order (for each edge prev -> cur: the intersection when the edge crosses, then cur when inside); an intersection
// is t = d_in / (d_in - d_out) (d = w +- coordinate, float) and every attribute is in + t (out - in) in float, from the inside
// vertex; the polygon is drawn as the fan of its first vertex. Without it (GlRaster before S3.21, tune 29 = 1) the unclipped
// triangle's attribute planes stand where the real GL has the fan triangles', the residue in the bands at the viewport edges.
bool GlRaster::clip_triangle_(const Vtx &a, const Vtx &b, const Vtx &c) {
  const Vtx *in3[3] = {&a, &b, &c};
  bool out = false;
  const float kx = clip_kx_;   // P8B (set_clip_x): 1 = the original's planes (the branches below then run the old expressions)
  for (const Vtx *p : in3)
    for (int k = 0; k < 3; ++k) {
      if (k == 0 && kx != 1.0f) {
        const float wx = kx * p->c[3];
        out = out || !(p->c[0] >= -wx && p->c[0] <= wx);
      } else {
        out = out || !(p->c[k] >= -p->c[3] && p->c[k] <= p->c[3]);
      }
    }
  if (!out) return false;
  const double area = edge(a.qx, a.qy, b.qx, b.qy, c.qx, c.qy);
  const bool front = (front_ == 0x0901) ? area >= 0 : area <= 0;
  ++stats.triangles;
  if (cull_ && (tune[11] == 7 ? front : !front)) { ++stats.culled; return true; }
  struct CV { float c[4]; float s, t, r, g, b, a; };
  auto to = [](const Vtx &v) {
    CV r;
    std::memcpy(r.c, v.c, sizeof r.c);
    r.s = static_cast<float>(v.s); r.t = static_cast<float>(v.t);
    r.r = static_cast<float>(v.r); r.g = static_cast<float>(v.g); r.b = static_cast<float>(v.b); r.a = static_cast<float>(v.a);
    return r;
  };
  std::vector<CV> poly = {to(a), to(b), to(c)}, next;
  auto dist = [kx](const CV &v, int plane) {
    const int k = plane >> 1;
    if (k == 0 && kx != 1.0f) {   // P8B: the moved x planes
      volatile float wx = kx * v.c[3];
      volatile float dx = (plane & 1) ? wx - v.c[0] : wx + v.c[0];
      return static_cast<float>(dx);
    }
    volatile float d = (plane & 1) ? v.c[3] - v.c[k] : v.c[3] + v.c[k];
    return static_cast<float>(d);
  };
  auto lerp = [](float p, float q, float t) {
    volatile float d = q - p;
    volatile float m = t * d;
    volatile float r = p + m;
    return static_cast<float>(r);
  };
  for (int plane = 0; plane < 6 && !poly.empty(); ++plane) {
    const size_t n = poly.size();
    std::vector<float> d(n);
    bool all_in = true;
    for (size_t i = 0; i < n; ++i) { d[i] = dist(poly[i], plane); all_in = all_in && d[i] >= 0; }
    if (all_in) continue;
    next.clear();
    for (size_t i = 0; i < n; ++i) {
      const size_t pi = (i + n - 1) % n;
      const float dp = d[pi], dc = d[i];
      if ((dp >= 0) != (dc >= 0)) {
        const CV &ii = dp >= 0 ? poly[pi] : poly[i];
        const CV &oo = dp >= 0 ? poly[i] : poly[pi];
        const float di = dp >= 0 ? dp : dc, dout = dp >= 0 ? dc : dp;
        volatile float den = di - dout;
        volatile float t = di / den;
        CV nv;
        for (int k = 0; k < 4; ++k) nv.c[k] = lerp(ii.c[k], oo.c[k], t);
        nv.s = lerp(ii.s, oo.s, t); nv.t = lerp(ii.t, oo.t, t);
        nv.r = lerp(ii.r, oo.r, t); nv.g = lerp(ii.g, oo.g, t); nv.b = lerp(ii.b, oo.b, t); nv.a = lerp(ii.a, oo.a, t);
        next.push_back(nv);
      }
      if (dc >= 0) next.push_back(poly[i]);
    }
    poly.swap(next);
  }
  if (poly.size() < 3) return true;
  // the unclipped vertices keep their own values (the clipper copies them); new ones take the window rule of their clip coordinates
  std::vector<Vtx> vs(poly.size());
  for (size_t i = 0; i < poly.size(); ++i) {
    const CV &p = poly[i];
    Vtx &v = vs[i];
    const Vtx *orig = nullptr;
    for (const Vtx *q : in3)
      if (std::memcmp(q->c, p.c, sizeof p.c) == 0 && static_cast<float>(q->s) == p.s && static_cast<float>(q->t) == p.t) orig = q;
    if (orig) { v = *orig; continue; }
    std::memcpy(v.c, p.c, sizeof p.c);
    window_of_(p.c, v);
    v.z = 0.5;
    v.s = p.s; v.t = p.t; v.r = p.r; v.g = p.g; v.b = p.b; v.a = p.a;
  }
  in_clip_ = true;
  for (size_t i = 1; i + 1 < vs.size(); ++i) triangle(vs[0], vs[i], vs[i + 1]);
  in_clip_ = false;
  --stats.triangles;   // counted once above for the original triangle; the fan's triangles count themselves
  return true;
}

void GlRaster::array_element(int32_t i) {
  const size_t k = static_cast<size_t>(static_cast<uint32_t>(i)) * 3;
  if (k + 3 > varr_.size()) { ++stats.bad_texture; return; }
  const float *nrm = k + 3 <= narr_.size() ? &narr_[k] : cur_n_;
  emit(varr_[k], varr_[k + 1], varr_[k + 2], nrm);
}
void GlRaster::vertex3fv(const float *v) { emit(v[0], v[1], v[2], cur_n_); }
void GlRaster::vertex3f(float x, float y, float z) { emit(x, y, z, cur_n_); }
void GlRaster::end() {
  in_begin_ = false;
  if (prim_ == kTriangles) {
    for (size_t i = 0; i + 2 < verts_.size(); i += 3) triangle(verts_[i], verts_[i + 1], verts_[i + 2]);
  } else if (prim_ == kTriangleFan) {
    for (size_t i = 1; i + 1 < verts_.size(); ++i) triangle(verts_[0], verts_[i], verts_[i + 1]);
  }
  verts_.clear();
}

void GlRaster::triangle(const Vtx &a, const Vtx &b, const Vtx &c) {
  if (tune[28] == 0 && tune[29] == 0 && !in_clip_ && clip_triangle_(a, b, c)) return;
  ++stats.triangles;
  const double area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
  if (area == 0) return;
  const bool front = (front_ == 0x0901) ? area > 0 : area < 0;
  if (!in_clip_ && cull_ && (tune[11] == 7 ? front : !front)) { ++stats.culled; return; }
  const auto tit = textures_.find(bound_);
  // A texture object with no image (id 0, or never loaded) is incomplete, and OpenGL then draws without texturing: the
  // fragment is the lit colour. A white 1x1 texel stands in for it (modulated by the colour, it is the colour).
  static const Tex kWhite = [] {
    Tex t;
    t.w = t.h = 1;
    t.fmt = kGlRgb;
    t.data = {255, 255, 255};
    t.mag = t.min = kNearest;
    return t;
  }();
  const bool textured = texture2d_ && tit != textures_.end() && tit->second.w != 0;
  if (!textured) ++stats.bad_texture;
  const Tex &tex = textured ? tit->second : kWhite;
  // work with the counter-clockwise order so that every edge function is positive inside
  const Vtx *v0 = &a, *v1 = &b, *v2 = &c;
  double ar = area;
  if (ar < 0) { std::swap(v1, v2); ar = -ar; }
  // texture-space derivatives (texels per pixel) for the minification / magnification choice
  const double u0 = v0->s * tex.w, u1 = v1->s * tex.w, u2 = v2->s * tex.w;
  const double t0 = v0->t * tex.h, t1 = v1->t * tex.h, t2 = v2->t * tex.h;
  const double dudx = ((u1 - u0) * (v2->y - v0->y) - (u2 - u0) * (v1->y - v0->y)) / ar;
  const double dudy = ((u2 - u0) * (v1->x - v0->x) - (u1 - u0) * (v2->x - v0->x)) / ar;
  const double dvdx = ((t1 - t0) * (v2->y - v0->y) - (t2 - t0) * (v1->y - v0->y)) / ar;
  const double dvdy = ((t2 - t0) * (v1->x - v0->x) - (t1 - t0) * (v2->x - v0->x)) / ar;
  const double rho = std::max(std::sqrt(dudx * dudx + dvdx * dvdx), std::sqrt(dudy * dudy + dvdy * dvdy));
  const uint32_t filter = rho <= 1.0 + 1e-6 ? tex.mag : tex.min;   // exactly one texel per pixel is still magnification in the original
  const bool linear = (filter == kLinear || filter == 0x2701 || filter == 0x2703) && tune[11] != 4;
  const int minx = std::max(0, static_cast<int>(std::floor(std::min({v0->x, v1->x, v2->x}))));
  const int maxx = std::min(w_ - 1, static_cast<int>(std::ceil(std::max({v0->x, v1->x, v2->x}))));
  const int miny = std::max(0, static_cast<int>(std::floor(std::min({v0->y, v1->y, v2->y}))));
  const int maxy = std::min(h_ - 1, static_cast<int>(std::ceil(std::max({v0->y, v1->y, v2->y}))));
  const int bpp = tex.fmt == kGlRgba ? 4 : 3;
  // tie-break (U-1300): an edge (counter-clockwise order, window y = memory row) owns the pixel centres exactly on it when it points
  // to +y, or to +x on a horizontal edge; as spans, rows with ymin <= y + 0.5 < ymax and columns with xl < x + 0.5 <= xr. Measured
  // with probes of the real GL (glprobe tie: 48 coverage differences against 288 .. 923 for the other five rules, the 48 all at
  // a vertex or a diagonal edge exactly on a centre) and on g6/i005_raised_pathupd (pixels more than one level off 145 -> 10:
  // path bands whose horizontal edge rounds to y = 344.5 exactly). tune 10 = 2 restores the earlier rule (-x on horizontal edges).
  const int tie = tune[10] == 0 ? 2 : tune[10] == 2 ? 0 : tune[10];   // experiment: rules 1, 3, 4, 5 (see `inclusive`)
  auto inclusive = [tie](const Vtx &p, const Vtx &q) {
    const double ex = q.x - p.x, ey = q.y - p.y;
    if (tie == 4) return false;
    if (tie == 5) return true;
    return ((tie & 1) ? ey < 0 : ey > 0) || (ey == 0 && ((tie & 2) ? ex > 0 : ex < 0));
  };
  const bool fixed = tune[26] == 0;   // the fixed-point setup (default); tune 26 = 1: the earlier exact edges + linear attribute scale
  if (fixed) {
    const double hx = vp_w_ * 0.5, hy = vp_h_ * 0.5, cxv = vp_x_ + hx, cyv = vp_y_ + hy;
    const Vtx *vs[3] = {v0, v1, v2};
    // tune 24: the rounding to 8 fractional bits (0 half to even, 1 floor, 2 ceil, 3 toward the centre, 4 half up, 5 half down)
    auto q = [this](double v, double c) {
      const double f = 256.0 * v;
      switch (tune[24]) {
        case 1: return std::floor(f) / 256.0;
        case 2: return std::ceil(f) / 256.0;
        case 3: return (v >= c ? std::floor(f) : std::ceil(f)) / 256.0;
        case 4: return std::floor(f + 0.5) / 256.0;   // half up
        case 5: return std::ceil(f - 0.5) / 256.0;    // half down
        default: return std::nearbyint(f) / 256.0;    // half to even
      }
    };
    // the scale: tune 22 = +-1 is 1 +- 2^-8 / half-size; |tune 22| > 1 gives it in hundredths (-100 = -1); tune 21: the centre moved by
    // tune 21 / 256 px
    const double k = tune[22] == 0 ? -1.0 : (tune[22] == 1 || tune[22] == -1) ? tune[22] : tune[22] / 100.0;
    const double cxs = cxv + tune[21] / 256.0, cys = cyv + tune[21] / 256.0;
    for (int i = 0; i < 3; ++i) {
      if (tune[28] == 0) {   // the float32 vertex rule's snapped positions (emit)
        sx_[i] = vs[i]->qx;
        sy_[i] = vs[i]->qy;
        continue;
      }
      if (tune[25] != 0) {
        // float32 evaluation of ndc * (half-size - 2^-8) + centre (tune 25)
        const float nx = static_cast<float>((vs[i]->x - cxs) / hx), ny = static_cast<float>((vs[i]->y - cys) / hy);
        const float sxf = static_cast<float>(hx + k / 256.0), syf = static_cast<float>(hy + k / 256.0);
        sx_[i] = q(static_cast<double>(nx * sxf + static_cast<float>(cxs)), cxs);
        sy_[i] = q(static_cast<double>(ny * syf + static_cast<float>(cys)), cys);
        continue;
      }
      sx_[i] = q(cxs + (vs[i]->x - cxs) * (1.0 + k / (256.0 * hx)), cxs);
      sy_[i] = q(cys + (vs[i]->y - cys) * (1.0 + k / (256.0 * hy)), cys);
    }
  }
  // tune 23 (with 22): coverage from the same fixed-point vertices
  Vtx cov[3] = {*v0, *v1, *v2};
  if (fixed && tune[23] == 0)   // tune 23 = 1: the fixed-point vertices for the attributes only
    for (int i = 0; i < 3; ++i) { cov[i].x = sx_[i]; cov[i].y = sy_[i]; }
  const Vtx *c0 = &cov[0], *c1 = &cov[1], *c2 = &cov[2];
  const bool cinc0 = inclusive(*c1, *c2), cinc1 = inclusive(*c2, *c0), cinc2 = inclusive(*c0, *c1);
  // TEXTURED WALK (tune 32, roadmap S3.29, U-1309): the generic GL interpolates a textured triangle whose texture has a GL_LINEAR
  // filter in float32 (GlRaster::tex_walk_); an all-GL_NEAREST texture and an untextured triangle take other paths (not modelled here)
  std::shared_ptr<const TexWalk> walk;
  if (tune[32] != 0 && fixed && tune[28] == 0 && textured && !(tex.mag == kNearest && (tex.min == kNearest || tex.min == 0x2700))) {
    const Vtx *const abc[3] = {&a, &b, &c};
    const Vtx *const cv[3] = {c0, c1, c2};
    const bool ci[3] = {cinc0, cinc1, cinc2};
    walk = tex_walk_(abc, tex, minx, maxx, miny, maxy, cv, ci);
  }
  // the default pipeline (every experiment knob 0 except the negative controls of tune 11 and the walk's 32..35, no float
  // diagnostics) has a fast loop
  bool fast = fixed && !keep_float_ && fast_mode != 0;
  for (int i = 0; i < kTunes && fast; ++i) fast = (i == 11 || (i >= 32 && i <= 35) || tune[i] == 0);
  if (fast) {
    const bool inc[3] = {cinc0, cinc1, cinc2};
    if (fast_triangle_(v0, v1, v2, ar, tex, linear, minx, maxx, miny, maxy, inc, walk)) return;
  }
  finish();   // the general loop writes after every queued triangle
  const int32_t tri_id = keep_float_ ? static_cast<int32_t>(tris_.size() / 12) : -1;
  if (keep_float_) {
    for (const Vtx *q : {v0, v1, v2}) { tris_.push_back(q->x); tris_.push_back(q->y); }
    for (const Vtx *q : {v0, v1, v2}) { tris_.push_back(q->s); tris_.push_back(q->t); }
  }
  // ATTRIBUTE STEPPING (roadmap S3.21b, U-1309; opt-in `#tune 30 1`, `#tune 31 <F>` adds the texture coordinates): the generic GL's
  // incremental fixed point, measured on a slow-ramp probe (log/roadmap/S3/21_notes.md section 10): rows walk from the largest y
  // down; the first covered row's start (its first covered pixel) is the plane value, every later row's start is the previous one
  // plus ONE held step trunc(Q * (-dA/dy * rows + dA/dx * (ix - ix_prev))) / Q, and each pixel adds trunc(Q * dA/dx) / Q; colour
  // Q = 31 * 2^16 (the 5-bit range, 16.16), texture coordinates Q = texture size * 2^F (texel units).
  const bool stepping = tune[30] != 0 && fixed && !(textured && tune[30] == 2);
  const int nstep = (stepping && tune[31] > 0) ? 5 : 3;
  std::vector<int> st_ix;
  std::vector<double> st_v, st_q;
  double st_gx[5] = {}, st_gy[5] = {};
  if (stepping) {
    const double A[5][3] = {{v0->r, v1->r, v2->r}, {v0->g, v1->g, v2->g}, {v0->b, v1->b, v2->b},
                            {v0->s * tex.w, v1->s * tex.w, v2->s * tex.w}, {v0->t * tex.h, v1->t * tex.h, v2->t * tex.h}};
    // colour quantum: untextured 31 * 2^16 (measured); textured triangles by tune 30 (1: the same, 2: not stepped, 3: 255 * 2^16,
    // 4: 2^16, 5: 31 * 2^16 * 8)
    double cq = 31.0 * 65536.0;
    if (textured) cq = tune[30] == 3 ? 255.0 * 65536.0 : tune[30] == 4 ? 65536.0 : tune[30] == 5 ? 31.0 * 65536.0 * 8.0 : cq;
    const double scl[5] = {cq, cq, cq, std::ldexp(1.0, tune[31]), std::ldexp(1.0, tune[31])};
    const double arS = edge(sx_[0], sy_[0], sx_[1], sy_[1], sx_[2], sy_[2]);
    const double dx1 = sx_[1] - sx_[0], dy1 = sy_[1] - sy_[0], dx2 = sx_[2] - sx_[0], dy2 = sy_[2] - sy_[0];
    for (int k = 0; k < nstep; ++k) {
      const double d1 = A[k][1] - A[k][0], d2 = A[k][2] - A[k][0];
      st_gx[k] = arS != 0 ? (d1 * dy2 - d2 * dy1) / arS : 0.0;
      st_gy[k] = arS != 0 ? (d2 * dx1 - d1 * dx2) / arS : 0.0;
    }
    auto Q = [&](double v, int k) { return std::trunc(v * scl[k]) / scl[k]; };
    const int rows = maxy - miny + 1;
    st_ix.assign(rows, INT32_MIN);
    st_v.assign(static_cast<size_t>(rows) * 5, 0.0);
    st_q.assign(5, 0.0);
    for (int k = 0; k < nstep; ++k) st_q[k] = Q(st_gx[k], k);
    int prev_y = 0, prev_ix = 0;
    double prev[5] = {};
    bool have = false;
    const double arA0 = std::fabs(arS);
    for (int y = maxy; y >= miny; --y) {
      int ix = INT32_MIN;
      for (int x = minx; x <= maxx && ix == INT32_MIN; ++x) {
        const double px = x + 0.5, py = y + 0.5;
        const double w0 = edge(c1->x, c1->y, c2->x, c2->y, px, py);
        const double w1 = edge(c2->x, c2->y, c0->x, c0->y, px, py);
        const double w2 = edge(c0->x, c0->y, c1->x, c1->y, px, py);
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        if ((w0 == 0 && !cinc0) || (w1 == 0 && !cinc1) || (w2 == 0 && !cinc2)) continue;
        ix = x;
      }
      if (ix == INT32_MIN) continue;
      double cur[5];
      if (!have) {
        const double px = ix + 0.5, py = y + 0.5;
        const double b0 = edge(sx_[1], sy_[1], sx_[2], sy_[2], px, py), b1 = edge(sx_[2], sy_[2], sx_[0], sy_[0], px, py);
        const double b2 = edge(sx_[0], sy_[0], sx_[1], sy_[1], px, py);
        for (int k = 0; k < nstep; ++k) cur[k] = arA0 != 0 ? (b0 * A[k][0] + b1 * A[k][1] + b2 * A[k][2]) / arA0 : A[k][0];
        have = true;
      } else {
        for (int k = 0; k < nstep; ++k)
          cur[k] = prev[k] + Q(-st_gy[k] * (prev_y - y) + st_gx[k] * (ix - prev_ix), k);
      }
      st_ix[y - miny] = ix;
      for (int k = 0; k < nstep; ++k) { st_v[static_cast<size_t>(y - miny) * 5 + k] = cur[k]; prev[k] = cur[k]; }
      prev_y = y;
      prev_ix = ix;
    }
  }
  for (int y = miny; y <= maxy; ++y) {
    const int32_t wix = walk ? walk->ix[static_cast<size_t>(y - walk->miny)] : INT32_MIN;
    float wacc[5] = {};
    int wx = INT32_MIN;
    for (int x = minx; x <= maxx; ++x) {
      const double px = x + 0.5, py = y + 0.5;
      const double w0 = edge(c1->x, c1->y, c2->x, c2->y, px, py);
      const double w1 = edge(c2->x, c2->y, c0->x, c0->y, px, py);
      const double w2 = edge(c0->x, c0->y, c1->x, c1->y, px, py);
      if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      if ((w0 == 0 && !cinc0) || (w1 == 0 && !cinc1) || (w2 == 0 && !cinc2)) continue;
      // the attributes are evaluated at the shifted position (spx, spy); coverage above used the pixel centre
      const double ex_ = fixed ? 0.0 : tune[6] == 0 ? kAttrScaleX : tune[6] < 0 ? 0.0 : tune[6] * 1e-9;
      const double ey_ = fixed ? 0.0 : tune[15] == 0 ? kAttrScaleY : tune[15] < 0 ? 0.0 : tune[15] * 1e-9;
      const double spx = px + (px - (vp_x_ + vp_w_ * 0.5)) * ex_ + tune[18] * 1e-5, spy = py + (py - (vp_y_ + vp_h_ * 0.5)) * ey_ + tune[19] * 1e-5;
      double a0 = edge(v1->x, v1->y, v2->x, v2->y, spx, spy);
      double a1 = edge(v2->x, v2->y, v0->x, v0->y, spx, spy);
      double a2 = edge(v0->x, v0->y, v1->x, v1->y, spx, spy);
      double arA = ar;   // the area the attribute weights are divided by
      if (fixed) {
        // attribute setup from fixed-point window coordinates, 8 fractional bits, of a viewport scaled by
        // (half-size + 2^-8) about its centre: x' = round(256 * (c + (x - c) * (1 + 2^-8 / h))) / 256; sampled at the pixel centre
        const double a0x = sx_[0], a0y = sy_[0], a1x = sx_[1], a1y = sy_[1], a2x = sx_[2], a2y = sy_[2];
        a0 = edge(a1x, a1y, a2x, a2y, spx, spy);   // spx / spy: the pixel centre (plus tune 18 / 19 only, with 6 / 15 off)
        a1 = edge(a2x, a2y, a0x, a0y, spx, spy);
        a2 = edge(a0x, a0y, a1x, a1y, spx, spy);
        arA = std::fabs(edge(a0x, a0y, a1x, a1y, a2x, a2y));
      }
      if (tune[20] != 0) {
        // experiment (U-1076): the attribute plane is set up from vertices offset by sign(ndc) * 2^-8 px per axis where |ndc| >= 0.5 (the
        // probes' per-vertex rule), sampled at the pixel centre; coverage keeps the exact vertices
        const double hx = vp_w_ * 0.5, hy = vp_h_ * 0.5, cxv = vp_x_ + hx, cyv = vp_y_ + hy, o = tune[20] / 256.0;
        const double oy = (tune[21] != 0 ? tune[21] : tune[20]) / 256.0;   // tune 21: the y offset's own value (0 = the same as x)
        auto off = [&](double p, double c, double h, double oo) {
          const double n = (p - c) / h;
          return n >= 0.5 ? oo : n <= -0.5 ? -oo : 0.0;
        };
        const double x0 = v0->x + off(v0->x, cxv, hx, o), y0 = v0->y + off(v0->y, cyv, hy, oy);
        const double x1 = v1->x + off(v1->x, cxv, hx, o), y1 = v1->y + off(v1->y, cyv, hy, oy);
        const double x2 = v2->x + off(v2->x, cxv, hx, o), y2 = v2->y + off(v2->y, cyv, hy, oy);
        a0 = edge(x1, y1, x2, y2, px, py);
        a1 = edge(x2, y2, x0, y0, px, py);
        a2 = edge(x0, y0, x1, y1, px, py);
        arA = std::fabs(edge(x0, y0, x1, y1, x2, y2));
      }
      if (tune[16] > 0 || tune[17] != 0) {
        // experiment (U-1076): the attribute plane from vertices rounded as f32(v + 2^k) - 2^k (a float32 window-coordinate bias,
        // tune 16 = k) and / or with every difference and product in float32 (tune 17 = 1); coverage above stays exact
        const double B = tune[16] > 0 ? std::ldexp(1.0, tune[16]) : 0.0;
        auto snap = [B](double v) { return B > 0 ? static_cast<double>(static_cast<float>(v + B)) - B : v; };
        const bool f = tune[17] != 0;
        auto F = [f](double v) { return f ? static_cast<double>(static_cast<float>(v)) : v; };
        const double x0 = snap(v0->x), y0 = snap(v0->y), x1 = snap(v1->x), y1 = snap(v1->y), x2 = snap(v2->x), y2 = snap(v2->y);
        auto E = [&](double ax, double ay, double bx, double by, double qx, double qy) {
          return F(F(F(bx - ax) * F(qy - ay)) - F(F(by - ay) * F(qx - ax)));
        };
        a0 = E(x1, y1, x2, y2, spx, spy);
        a1 = E(x2, y2, x0, y0, spx, spy);
        a2 = E(x0, y0, x1, y1, spx, spy);
        arA = std::fabs(E(x0, y0, x1, y1, x2, y2));
      }
      if (tune[13] != 0 || tune[14] != 0) {   // experiment: a further shift of 1/256 pixel per unit
        const double sx = tune[13] / 256.0, sy = tune[14] / 256.0;
        a0 += (v1->y - v2->y) * sx + (v2->x - v1->x) * sy;
        a1 += (v2->y - v0->y) * sx + (v0->x - v2->x) * sy;
        a2 += (v0->y - v1->y) * sx + (v1->x - v0->x) * sy;
      }
      // Nearest sampling is exact at the pixel centre (probes: 0 differences at 0.25..0.75 texel per pixel without any shift);
      // the bilinear filter and the interpolated colours are evaluated at the shifted position.
      const double s = linear ? (a0 * v0->s + a1 * v1->s + a2 * v2->s) / arA : (w0 * v0->s + w1 * v1->s + w2 * v2->s) / ar;
      const double t = linear ? (a0 * v0->t + a1 * v1->t + a2 * v2->t) / arA : (w0 * v0->t + w1 * v1->t + w2 * v2->t) / ar;
      double u = s * tex.w, v = t * tex.h;
      const int st_row = y - miny;
      const bool st_px = stepping && st_ix[st_row] != INT32_MIN;
      if (st_px && nstep == 5) {   // the stepped texture coordinates (texel units)
        u = st_v[static_cast<size_t>(st_row) * 5 + 3] + (x - st_ix[st_row]) * st_q[3];
        v = st_v[static_cast<size_t>(st_row) * 5 + 4] + (x - st_ix[st_row]) * st_q[4];
      }
      const bool wpx = wix != INT32_MIN;
      if (wpx) {   // the textured walk (tune 32): the row start, then one float32 sum per pixel along the span
        if (wx == INT32_MIN || x < wx) {
          const float *st0 = &walk->start[static_cast<size_t>(y - walk->miny) * 5];
          for (int k = 0; k < 5; ++k) wacc[k] = st0[k];
          wx = wix;
        }
        for (; wx < x; ++wx)
          for (int k = 0; k < 5; ++k) wacc[k] = wacc[k] + walk->gx[k];
        u = wacc[0];
        v = wacc[1];
      }
      if (tune[2] > 0) {
        const double q = static_cast<double>(1ull << tune[2]);
        u = std::floor(u * q) / q;
        v = std::floor(v * q) / q;
      }
      double r, g, bl, al = 255.0;
      if (!linear) {
        const int iu = wrap_repeat(static_cast<int64_t>(std::floor(u)), tex.w), iv = wrap_repeat(static_cast<int64_t>(std::floor(v)), tex.h);
        const uint8_t *tp = &tex.data[(static_cast<size_t>(iv) * tex.w + iu) * bpp];
        r = tp[0]; g = tp[1]; bl = tp[2];
        if (bpp == 4) al = tp[3];
      } else {
        const double fu = u - 0.5, fv = v - 0.5;
        const int64_t i0 = static_cast<int64_t>(std::floor(fu)), j0 = static_cast<int64_t>(std::floor(fv));
        double ax = fu - i0, ay = fv - j0;
        if (tune[0] > 0) {
          const double q = static_cast<double>(1ull << tune[0]);
          ax = (tune[1] ? std::floor(ax * q + 0.5) : std::floor(ax * q)) / q;
          ay = (tune[1] ? std::floor(ay * q + 0.5) : std::floor(ay * q)) / q;
        }
        const int xs[2] = {wrap_repeat(i0, tex.w), wrap_repeat(i0 + 1, tex.w)};
        const int ys[2] = {wrap_repeat(j0, tex.h), wrap_repeat(j0 + 1, tex.h)};
        double acc[4] = {0, 0, 0, 0};
        auto rnd = [&](double x, int mode) { return mode == 1 ? std::floor(x + 0.5) : mode == 2 ? std::floor(x) : mode == 3 ? std::ceil(x) : x; };
        if (tune[9] > 0) {   // experiment: horizontal then vertical lerp, each stage rounded to an integer 0..255
          for (int k = 0; k < bpp; ++k) {
            double rowv[2];
            for (int dy = 0; dy < 2; ++dy) {
              const double p0 = tex.data[(static_cast<size_t>(ys[dy]) * tex.w + xs[0]) * bpp + k];
              const double p1 = tex.data[(static_cast<size_t>(ys[dy]) * tex.w + xs[1]) * bpp + k];
              rowv[dy] = rnd(p0 * (1 - ax) + p1 * ax, tune[9]);
            }
            acc[k] = rnd(rowv[0] * (1 - ay) + rowv[1] * ay, tune[9]);
          }
        } else
        for (int dy = 0; dy < 2; ++dy)
          for (int dx = 0; dx < 2; ++dx) {
            const double wgt = (dx ? ax : 1 - ax) * (dy ? ay : 1 - ay);
            const uint8_t *tp = &tex.data[(static_cast<size_t>(ys[dy]) * tex.w + xs[dx]) * bpp];
            for (int k = 0; k < bpp; ++k) acc[k] += wgt * tp[k];
          }
        if (tune[8] > 0) for (int k = 0; k < 4; ++k) acc[k] = rnd(acc[k], tune[8]);
        r = acc[0]; g = acc[1]; bl = acc[2];
        if (bpp == 4) al = acc[3];
      }
      double cr = (a0 * v0->r + a1 * v1->r + a2 * v2->r) / arA, cg = (a0 * v0->g + a1 * v1->g + a2 * v2->g) / arA;
      double cb = (a0 * v0->b + a1 * v1->b + a2 * v2->b) / arA;
      if (st_px) {   // the stepped colour (21b)
        const double *sv = &st_v[static_cast<size_t>(st_row) * 5];
        const double dxs = x - st_ix[st_row];
        cr = sv[0] + dxs * st_q[0]; cg = sv[1] + dxs * st_q[1]; cb = sv[2] + dxs * st_q[2];
      }
      if (wpx) { cr = wacc[2] / 31.0; cg = wacc[3] / 31.0; cb = wacc[4] / 31.0; }
      if (tune[3] > 0) {
        const double q = static_cast<double>((1ull << tune[3]) - 1);
        cr = std::floor(cr * q + 0.5) / q;
        cg = std::floor(cg * q + 0.5) / q;
        cb = std::floor(cb * q + 0.5) / q;
      }
      if (tune[11] == 3) cr = cg = cb = 1.0;
      if (env_mode_ == 0x2100 && tune[11] != 5) { r *= cr; g *= cg; bl *= cb; }   // GL_MODULATE
      uint16_t &dst = fb_[static_cast<size_t>(y) * w_ + x];
      double ro = r, go = g, bo = bl;
      if (keep_float_) {
        float *sp = &sfb_[(static_cast<size_t>(y) * w_ + x) * 8];
        sp[0] = static_cast<float>(r); sp[1] = static_cast<float>(g); sp[2] = static_cast<float>(bl); sp[3] = static_cast<float>(al);
        sp[4] = static_cast<float>((dst >> 10) & 31); sp[5] = static_cast<float>((dst >> 5) & 31); sp[6] = static_cast<float>(dst & 31);
        sp[7] = blend_ ? 1.0f : 0.0f;
        rfb_[static_cast<size_t>(y) * w_ + x] = static_cast<float>(rho);
        tid_[static_cast<size_t>(y) * w_ + x] = tri_id;
      }
      if (blend_ && tune[11] != 6) {
        // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA against the 5-bit destination expanded to 8 bits
        const double alpha = al / 255.0;
        const int dr = ((dst >> 10) & 31), dg = ((dst >> 5) & 31), db = dst & 31;
        ro = r * alpha + dr * 255.0 / 31.0 * (1 - alpha);
        go = g * alpha + dg * 255.0 / 31.0 * (1 - alpha);
        bo = bl * alpha + db * 255.0 / 31.0 * (1 - alpha);
        ++stats.blended_px;
      }
      if (keep_float_) {
        float *f = &ffb_[(static_cast<size_t>(y) * w_ + x) * 3];
        f[0] = static_cast<float>(ro); f[1] = static_cast<float>(go); f[2] = static_cast<float>(bo);
      }
      const int ctl = tune[11];   // deliberately wrong behaviours for the negative controls (0 = the correct pipeline)
      if (wpx && tune[35] == 0 && !blend_ && env_mode_ == 0x2100 && ctl == 0) {   // the walk's float32 final stage
        const float d = static_cast<float>((kDither[y & 3][(x + dither_dx_) & 3] + 0.5) / 16.0);
        dst = walk_final(tex.data.data(), tex.w, tex.h, bpp, linear, wacc, d);
        ++stats.drawn_px;
        continue;
      }
      const int qx = x + dither_dx_;   // P8B: set_dither_dx (0 = x)
      dst = static_cast<uint16_t>((quantize(ro, qx, y, ctl) << 10) | (quantize(go, qx, y, ctl) << 5) | quantize(bo, qx, y, ctl));
      ++stats.drawn_px;
    }
  }
}

// everything the fast pixel loop reads of one triangle (captured when the triangle is set up, so a queued job does not depend on
// later state calls; the texel pointer stays valid because every texture upload / deletion runs the queue first)
struct GlRaster::FastJob {
  uint16_t *fb;
  int fbw, miny, maxy, minx, maxx;
  int ddx;   // P8B: GlRaster::dither_dx_
  int64_t A[3], B[3], X[3], Y[3];
  bool inc[3];
  double s0, s1, s2, t0, t1, t2, r0, r1, r2, g0, g1, g2, b0, b1, b2;
  double div_st, arA, inv_st, inv_a, twd, thd;
  const uint8_t *td;
  uint32_t tw, th;
  int bpp, dith;
  bool linear, modulate, blend, white, approx, sloppy;
  bool wctl;   // worker_ctl (negative control)
  std::shared_ptr<const GlRaster::TexWalk> walk;   // the textured walk (tune 32), else empty
  bool wfinal;   // the walk's float32 final stage (tune 32 with tune 35 = 0 and tune 11 = 0)
};

// The pixel-loop workers (set_threads): a ring of jobs written in order by the thread that sets the triangles up; worker k
// rasterises, for every job in order, only the rows whose 4-row band (y >> 2) is k modulo the worker count. Each pixel then sees
// the same writes in the same order as with one thread, so the frame is the same; the counters are summed by finish().
struct GlRaster::Pool {
  static constexpr uint64_t kCap = 4096;
  std::vector<FastJob> ring;
  std::atomic<uint64_t> published{0};
  std::unique_ptr<std::atomic<uint64_t>[]> done;
  std::vector<std::array<int64_t, 3>> counters;   // per worker: drawn, blended, fast (written by the worker before done)
  std::mutex m;
  std::condition_variable cv_work, cv_done;
  bool stop = false;
  int parts = 0;
  std::vector<std::thread> threads;

  uint64_t min_done() const {
    uint64_t lo = ~0ull;
    for (int k = 0; k < parts; ++k) lo = std::min(lo, done[k].load());
    return lo;
  }
  void work(int k) {
    uint64_t i = 0;
    for (;;) {
      const uint64_t n = published.load();
      if (i < n) {
        int64_t *c = counters[static_cast<size_t>(k)].data();
        for (; i < n; ++i) fast_rows_(ring[i % kCap], k, parts, c);
        done[k].store(i);
        std::lock_guard<std::mutex> lk(m);
        cv_done.notify_all();
        continue;
      }
      std::unique_lock<std::mutex> lk(m);
      cv_work.wait(lk, [&] { return stop || published.load() > i; });
      if (stop && published.load() <= i) return;
    }
  }
};

GlRaster::~GlRaster() { set_threads(1); }

void GlRaster::set_threads(int n) {
  finish();
  if (pool_) {
    {
      std::lock_guard<std::mutex> lk(pool_->m);
      pool_->stop = true;
    }
    pool_->cv_work.notify_all();
    for (std::thread &t : pool_->threads) t.join();
    pool_.reset();
  }
  if (n <= 1) return;
  pool_ = std::make_unique<Pool>();
  Pool &p = *pool_;
  p.ring.resize(Pool::kCap);
  p.parts = n;
  p.done = std::make_unique<std::atomic<uint64_t>[]>(static_cast<size_t>(n));
  for (int k = 0; k < n; ++k) p.done[k].store(0);
  p.counters.assign(static_cast<size_t>(n), {0, 0, 0});
#ifdef _WIN32
  // the workers run at the priority of the thread that feeds them (P15A fix 2: or normal, set_workers_normal)
  const int prio = workers_normal_ ? THREAD_PRIORITY_NORMAL : GetThreadPriority(GetCurrentThread());
#endif
  for (int k = 0; k < n; ++k) {
    p.threads.emplace_back([&p, k] { p.work(k); });
#ifdef _WIN32
    if (prio != THREAD_PRIORITY_ERROR_RETURN) SetThreadPriority(p.threads.back().native_handle(), prio);
#endif
  }
}

int GlRaster::threads() const { return pool_ ? pool_->parts : 1; }

void GlRaster::finish() {
  if (!pool_) return;
  Pool &p = *pool_;
  const uint64_t n = p.published.load();
  {
    std::unique_lock<std::mutex> lk(p.m);
    p.cv_work.notify_all();
    p.cv_done.wait(lk, [&] { return p.min_done() >= n; });
  }
  for (std::array<int64_t, 3> &c : p.counters) {
    stats.drawn_px += c[0];
    stats.blended_px += c[1];
    stats.fast_px += c[2];
    c = {0, 0, 0};
  }
}

void GlRaster::queue_(const FastJob &j) {
  Pool &p = *pool_;
  const uint64_t n = p.published.load();
  if (n - p.min_done() >= Pool::kCap) {
    std::unique_lock<std::mutex> lk(p.m);
    p.cv_work.notify_all();
    p.cv_done.wait(lk, [&] { return n - p.min_done() < Pool::kCap; });
  }
  p.ring[n % Pool::kCap] = j;
  p.published.store(n + 1);
  if (((n + 1) & 15) == 0) {
    std::lock_guard<std::mutex> lk(p.m);
    p.cv_work.notify_all();
  }
}

// The general loop of triangle() with every tune knob 0 (tune 11 aside) and keep_float_ off, computed faster to the same bits
// (roadmap S3.10, log/roadmap/S3/10_notes.md section 2):
//   - the setup vertices sx_ / sy_ are multiples of 2^-8 (nearbyint(256 v) / 256); while they and the pixel centres stay below
//     2^14 in magnitude, every edge function of the general loop, (bx - ax) * (py - ay) - (by - ay) * (px - ax), is exact in
//     double (differences of 23 significant bits, products of 46, their difference of 47), so it equals the int64 value in units
//     of 2^-16 used here, and its sign and zero tests are the same. The attribute weights a0..a2 of the general loop are the same
//     edge functions of the same vertices at the same centre (spx = px, spy = py with tunes 6 / 15 / 18 / 19 at 0), so they are
//     these values converted back to double (exact below 2^53);
//   - per row, each edge's inside test (w > 0, or w == 0 on an owning edge) is linear in the pixel column, so the covered pixels
//     are the integer interval the three tests give (floor / ceil divisions), clipped to the general loop's bounding box;
//   - floor, the repeat wrap, the dither offset and dst * 255 / 31 are the helpers and tables above.
// The exact branch of the loop writes every other expression (texture coordinates, filtering, colours, modulation, blending,
// quantisation) as the general loop does, in the same order. Returns false without writing when a precondition fails: the caller
// then runs the general loop.
// THE CHECKED RECIPROCAL PATH (fast_mode 1) computes a pixel first with the five divisions by the triangle's areas and the
// three by 255 replaced by products with the reciprocals, and keeps that pixel only when it cannot differ from the exact branch's:
//   - the numerators are the same doubles, so each quotient differs from the exact one by at most 1.5 * 2^-52 relative; with the
//     texel coordinates u, v below 2^16 they differ by less than 1e-10, and so do the bilinear fractions ax, ay (fu - floor(fu) is
//     exact). The texel indices are equal when the fraction is farther than kTexEps = 1e-9 from 0 and 1 (nearest: u - floor(u));
//   - then each filtered channel differs by at most 255 * 2 (|d ax| + |d ay|) < 1.1e-7, the vertex colours are in [0, 1] (checked
//     per triangle), the blend's alpha by at most 4.4e-10, the blended channel by at most 2.2e-7, and the quantiser's
//     c * 31 / 255 + d by at most 3e-8: floor gives the same 5-bit value when the fraction is farther than kQuantEps = 1e-6 from 0
//     and 1 (30 times that bound);
// any other pixel (and every pixel of a triangle with a zero or non-finite area, or colours outside [0, 1]) takes the exact branch.
// With workers (set_threads) the job is queued and rasterised by them; finish() waits for it.
// port/test/test_gl_raster.py compares whole frames of the loops (fast_mode 0, 1, 2, workers) on the recorded streams.
// TEXTURED WALK (roadmap S3.29, U-1309; log/roadmap/S3/29_notes.md): measured with probes of the real software GL
// (log/roadmap/S3/29_probes.py through re/tools/glexact/glreplay.exe, fitted by 29_texfit.py). A textured triangle whose texture has
// a GL_LINEAR filter is interpolated in float32, not as an exact plane:
//   - attributes: u = s * width, v = t * height (texel units), r, g, b = float32(colour) * 31 (the 5-bit range);
//   - gradients (float32, every product and sum rounded): with c the vertex of lowest window y (the first in stream order on a
//     tie; tune 33 picks another), a, b the next two in stream order, dxAC = xa - xc, dxBC, dyAC, dyBC, area = dxAC dyBC - dxBC
//     dyAC, r = 1 / area, t1 = dyAC r, t2 = dyBC r, t3 = dxAC r, t4 = dxBC r, dA/dx = dAC t2 - dBC t1, dA/dy = dBC t3 - dAC t4;
//   - rows walk from the largest y down; the first covered row's start (its first covered pixel) is float32 of the exact plane value
//     there; when the middle vertex lies left of the long edge, the first row below it starts the same way again (the left edge
//     turns; tune 34 = 1 keeps walking); every other row's start is the previous one plus float32(-dA/dy * rows + dA/dx * pixels),
//     each product and the sum rounded to float32;
//   - along a span every pixel adds dA/dx in float32.
// Probes: s329_alt_big (u's error amplified 255 x by alternating texels) 64,599 -> 1 differing px, const_rand 285 -> 5, gour_rand
// 306 -> 0, cramp_lin 31 -> 0 (29_notes.md section 2 for every probe and what is left).
std::shared_ptr<const GlRaster::TexWalk> GlRaster::tex_walk_(const Vtx *const abc[3], const Tex &tex, int minx, int maxx, int miny,
                                                             int maxy, const Vtx *const cov[3], const bool inc[3]) const {
  auto w = std::make_shared<TexWalk>();
  w->miny = miny;
  const int rows = maxy - miny + 1;
  if (rows <= 0) return nullptr;
  w->ix.assign(static_cast<size_t>(rows), INT32_MIN);
  w->start.assign(static_cast<size_t>(rows) * 5, 0.0f);
  // the attributes of the three vertices in stream order: double (the plane) and float32 (the gradients)
  double A[5][3];
  float Af[5][3];
  for (int i = 0; i < 3; ++i) {
    const Vtx &p = *abc[i];
    A[0][i] = p.s * tex.w;
    A[1][i] = p.t * tex.h;
    const double cc[3] = {p.r, p.g, p.b};
    for (int k = 0; k < 3; ++k) A[2 + k][i] = cc[k] * 31.0;
    Af[0][i] = static_cast<float>(A[0][i]);
    Af[1][i] = static_cast<float>(A[1][i]);
    for (int k = 0; k < 3; ++k) {
      const float cf = static_cast<float>(cc[k]);
      Af[2 + k][i] = cf * 31.0f;
    }
  }
  const double X[3] = {abc[0]->qx, abc[1]->qx, abc[2]->qx}, Y[3] = {abc[0]->qy, abc[1]->qy, abc[2]->qy};
  // the gradient anchor: the lowest vertex (tune 33: 1 the highest, 2 the first in stream order)
  int ci = 0;
  for (int i = 1; i < 3; ++i) {
    if (tune[33] == 1 ? Y[i] > Y[ci] : Y[i] < Y[ci]) ci = i;
  }
  if (tune[33] == 2) ci = 0;
  const int ai = (ci + 1) % 3, bi = (ci + 2) % 3;
  const float dxAC = static_cast<float>(X[ai]) - static_cast<float>(X[ci]), dxBC = static_cast<float>(X[bi]) - static_cast<float>(X[ci]);
  const float dyAC = static_cast<float>(Y[ai]) - static_cast<float>(Y[ci]), dyBC = static_cast<float>(Y[bi]) - static_cast<float>(Y[ci]);
  const float p1 = dxAC * dyBC, p2 = dxBC * dyAC;
  const float area = p1 - p2;
  if (!(area != 0.0f)) return nullptr;
  const float rcp = 1.0f / area;
  const float t1 = dyAC * rcp, t2 = dyBC * rcp, t3 = dxAC * rcp, t4 = dxBC * rcp;
  float gy[5];
  for (int k = 0; k < 5; ++k) {
    const float dAC = Af[k][ai] - Af[k][ci], dBC = Af[k][bi] - Af[k][ci];
    const float x1 = dAC * t2, x2 = dBC * t1, y1 = dBC * t3, y2 = dAC * t4;
    w->gx[k] = x1 - x2;
    gy[k] = y1 - y2;
  }
  // the exact plane over the snapped vertices (stream order)
  const double ar = edge(X[0], Y[0], X[1], Y[1], X[2], Y[2]);
  auto plane = [&](int k, double px, double py) {
    const double w0 = edge(X[1], Y[1], X[2], Y[2], px, py), w1 = edge(X[2], Y[2], X[0], Y[0], px, py);
    const double w2 = edge(X[0], Y[0], X[1], Y[1], px, py);
    return (w0 * A[k][0] + w1 * A[k][1] + w2 * A[k][2]) / ar;
  };
  // the left edge's turn: the middle vertex (by y, then stream order) left of the long edge
  int o[3] = {0, 1, 2};
  for (int i = 0; i < 3; ++i)
    for (int k = i + 1; k < 3; ++k)
      if (Y[o[k]] < Y[o[i]] || (Y[o[k]] == Y[o[i]] && o[k] < o[i])) std::swap(o[i], o[k]);
  const int vmin = o[0], vmid = o[1], vmax = o[2];
  bool midleft = false;
  if (Y[vmin] != Y[vmax]) {
    const double xl = X[vmax] + (X[vmin] - X[vmax]) * (Y[vmid] - Y[vmax]) / (Y[vmin] - Y[vmax]);
    midleft = X[vmid] < xl && tune[34] == 0;
  }
  // rows from the largest y down: the first covered pixel (the general loop's coverage test), then the row start
  const Vtx *c0 = cov[0], *c1 = cov[1], *c2 = cov[2];
  bool have = false, switched = false;
  int prev_y = 0, prev_ix = 0;
  float prev[5] = {};
  for (int y = maxy; y >= miny; --y) {
    int ix = INT32_MIN;
    for (int x = minx; x <= maxx && ix == INT32_MIN; ++x) {
      const double px = x + 0.5, py = y + 0.5;
      const double e0 = edge(c1->x, c1->y, c2->x, c2->y, px, py);
      const double e1 = edge(c2->x, c2->y, c0->x, c0->y, px, py);
      const double e2 = edge(c0->x, c0->y, c1->x, c1->y, px, py);
      if (e0 < 0 || e1 < 0 || e2 < 0) continue;
      if ((e0 == 0 && !inc[0]) || (e1 == 0 && !inc[1]) || (e2 == 0 && !inc[2])) continue;
      ix = x;
    }
    if (ix == INT32_MIN) continue;
    const bool lower = midleft && (y + 0.5) < Y[vmid];
    float cur[5];
    if (!have || (lower && !switched)) {
      for (int k = 0; k < 5; ++k) cur[k] = static_cast<float>(plane(k, ix + 0.5, y + 0.5));
    } else {
      const float fdy = static_cast<float>(prev_y - y), fdi = static_cast<float>(ix - prev_ix);
      for (int k = 0; k < 5; ++k) {
        const float sy = (-gy[k]) * fdy, sx = w->gx[k] * fdi;
        const float st = sy + sx;
        cur[k] = prev[k] + st;
      }
    }
    switched = switched || lower;
    have = true;
    w->ix[static_cast<size_t>(y - miny)] = ix;
    for (int k = 0; k < 5; ++k) {
      w->start[static_cast<size_t>(y - miny) * 5 + k] = cur[k];
      prev[k] = cur[k];
    }
    prev_y = y;
    prev_ix = ix;
  }
  return w;
}

bool GlRaster::fast_triangle_(const Vtx *v0, const Vtx *v1, const Vtx *v2, double ar, const Tex &tex, bool linear, int minx,
                              int maxx, int miny, int maxy, const bool inc[3], std::shared_ptr<const TexWalk> walk) {
  constexpr double kLim = 16384.0;   // 2^14
  if (w_ >= 16384 || h_ >= 16384) return false;
  for (int i = 0; i < 3; ++i)
    if (!(std::fabs(sx_[i]) < kLim && std::fabs(sy_[i]) < kLim)) return false;
  FastJob j;
  const int ctl = tune[11];
  j.fb = fb_.data();
  j.fbw = w_;
  j.ddx = dither_dx_;
  j.minx = minx; j.maxx = maxx; j.miny = miny; j.maxy = maxy;
  j.dith = ctl == 1 ? 1 : ctl == 2 ? 2 : 0;
  j.modulate = env_mode_ == 0x2100 && ctl != 5;
  j.blend = blend_ && ctl != 6;
  j.white = ctl == 3;
  // fixed-point vertices in units of 2^-8 (exact: sx_ * 256 is an integer below 2^22)
  for (int i = 0; i < 3; ++i) { j.X[i] = static_cast<int64_t>(sx_[i] * 256.0); j.Y[i] = static_cast<int64_t>(sy_[i] * 256.0); }
  // edge i runs from vertex P = (i + 1) % 3 to Q = (i + 2) % 3: w_i = A (PY - Py) - B (PX - Px), PX = 256 x + 128, PY = 256 y + 128
  for (int i = 0; i < 3; ++i) {
    const int p = (i + 1) % 3, q = (i + 2) % 3;
    j.A[i] = j.X[q] - j.X[p];
    j.B[i] = j.Y[q] - j.Y[p];
    j.inc[i] = inc[i];
  }
  j.arA = std::fabs(edge(sx_[0], sy_[0], sx_[1], sy_[1], sx_[2], sy_[2]));
  j.div_st = linear ? j.arA : ar;   // the general loop divides the texture coordinates by arA (linear) or ar (nearest)
  j.linear = linear;
  j.bpp = tex.fmt == kGlRgba ? 4 : 3;
  j.td = tex.data.data();
  j.tw = tex.w;
  j.th = tex.h;
  j.twd = static_cast<double>(tex.w);
  j.thd = static_cast<double>(tex.h);
  j.s0 = v0->s; j.s1 = v1->s; j.s2 = v2->s; j.t0 = v0->t; j.t1 = v1->t; j.t2 = v2->t;
  j.r0 = v0->r; j.r1 = v1->r; j.r2 = v2->r; j.g0 = v0->g; j.g1 = v1->g; j.g2 = v2->g; j.b0 = v0->b; j.b1 = v1->b; j.b2 = v2->b;
  // the checked reciprocal path (fast_mode 1; negative controls: 3 skips the boundary checks, 4 rounds the reciprocals to float)
  j.sloppy = fast_mode == 3;
  j.wctl = worker_ctl == 1;
  j.walk = std::move(walk);
  j.wfinal = tune[35] == 0 && ctl == 0;
  bool approx = (fast_mode == 1 || fast_mode == 3 || fast_mode == 4) && j.div_st > 0 && j.div_st < 1e300 && j.arA > 0 &&
                j.arA < 1e300;
  for (const Vtx *p : {v0, v1, v2})
    approx = approx && p->r >= 0 && p->r <= 1 && p->g >= 0 && p->g <= 1 && p->b >= 0 && p->b <= 1 && std::isfinite(p->s) &&
             std::isfinite(p->t);
  j.approx = approx;
  j.inv_st = approx ? 1.0 / j.div_st : 0.0;
  j.inv_a = approx ? 1.0 / j.arA : 0.0;
  if (fast_mode == 4) {
    j.inv_st = static_cast<float>(j.inv_st);
    j.inv_a = static_cast<float>(j.inv_a);
  }
  if (pool_) {
    queue_(j);
  } else {
    int64_t c[3] = {0, 0, 0};
    fast_rows_(j, 0, 1, c);
    stats.drawn_px += c[0];
    stats.blended_px += c[1];
    stats.fast_px += c[2];
  }
  return true;
}

// the rows y of job j with (y >> 2) % parts == part; c: drawn, blended and reciprocal-path pixel counts
void GlRaster::fast_rows_(const FastJob &j, int part, int parts, int64_t *c) {
  const FastTables &tb = fast_tables();
  const double(*dith)[4] = tb.dither[j.dith];
  constexpr double kUnit = 1.0 / 65536.0;
  constexpr double kTexLim = 65536.0, kTexEps = 1e-9, kQuantEps = 1e-6, kInv255 = 1.0 / 255.0, k31of255 = 31.0 / 255.0;
  const bool linear = j.linear, modulate = j.modulate, blend = j.blend, white = j.white, approx = j.approx, sloppy = j.sloppy;
  const int bpp = j.bpp;
  const uint8_t *td = j.td;
  const uint32_t tw = j.tw, th = j.th;
  const double twd = j.twd, thd = j.thd, div_st = j.div_st, arA = j.arA, inv_st = j.inv_st, inv_a = j.inv_a;
  const double s0 = j.s0, s1 = j.s1, s2 = j.s2, t0 = j.t0, t1 = j.t1, t2 = j.t2;
  const double r0 = j.r0, r1 = j.r1, r2 = j.r2, g0 = j.g0, g1 = j.g1, g2 = j.g2, b0 = j.b0, b1 = j.b1, b2 = j.b2;
  int64_t drawn = 0, blended = 0, fastn = 0;
  for (int y = j.miny; y <= j.maxy; ++y) {
    if (parts > 1 && ((y >> 2) % parts) != part) continue;
    const int64_t PY = 256 * static_cast<int64_t>(y) + 128;
    int64_t xl = j.minx, xr = j.maxx, K[3];
    for (int i = 0; i < 3; ++i) {
      const int p = (i + 1) % 3;
      K[i] = j.A[i] * (PY - j.Y[p]) + j.B[i] * j.X[p];   // w_i(x) = K_i - B_i * PX
      const int64_t R = K[i] - (j.inc[i] ? 0 : 1);        // inside: w_i >= 0 on an owning edge, w_i >= 1 otherwise: B_i * PX <= R
      if (j.B[i] > 0) xr = std::min(xr, floor_div(floor_div(R, j.B[i]) - 128, 256));
      else if (j.B[i] < 0) xl = std::max(xl, ceil_div(ceil_div(-R, -j.B[i]) - 128, 256));
      else if (R < 0) xr = xl - 1;
    }
    if (xl > xr) continue;
    const int64_t PX0 = 256 * xl + 128;
    int64_t w0 = K[0] - j.B[0] * PX0, w1 = K[1] - j.B[1] * PX0, w2 = K[2] - j.B[2] * PX0;
    const int64_t d0 = 256 * j.B[0], d1 = 256 * j.B[1], d2 = 256 * j.B[2];
    uint16_t *row = j.fb + static_cast<size_t>(y) * static_cast<size_t>(j.fbw);
    const double(&drow)[4] = dith[y & 3];
    // the textured walk (tune 32): the row's float32 accumulators from its first covered pixel (the general loop's rule)
    const GlRaster::TexWalk *wk = j.walk.get();
    const int32_t wix = wk ? wk->ix[static_cast<size_t>(y - wk->miny)] : INT32_MIN;
    float wacc[5] = {};
    int wx = INT32_MIN;
    if (wix != INT32_MIN) {
      const float *st0 = &wk->start[static_cast<size_t>(y - wk->miny) * 5];
      for (int k = 0; k < 5; ++k) wacc[k] = st0[k];
      wx = wix;
    }
    for (int x = static_cast<int>(xl); x <= static_cast<int>(xr); ++x, w0 -= d0, w1 -= d1, w2 -= d2) {
      const double a0 = static_cast<double>(w0) * kUnit, a1 = static_cast<double>(w1) * kUnit, a2 = static_cast<double>(w2) * kUnit;
      const bool wpx = wix != INT32_MIN;
      if (wpx) {
        if (x < wx) {
          const float *st0 = &wk->start[static_cast<size_t>(y - wk->miny) * 5];
          for (int k = 0; k < 5; ++k) wacc[k] = st0[k];
          wx = wix;
        }
        for (; wx < x; ++wx)
          for (int k = 0; k < 5; ++k) wacc[k] = wacc[k] + wk->gx[k];
      }
      if (approx && !wpx) {
        // the same pixel with the divisions replaced by products with the reciprocals; kept only when every rounding decision is
        // farther from its boundary than the two computations can differ (bounds in the comment of fast_triangle_)
        const double s = (a0 * s0 + a1 * s1 + a2 * s2) * inv_st, t = (a0 * t0 + a1 * t1 + a2 * t2) * inv_st;
        const double u = s * twd, v = t * thd;
        if (std::fabs(u) < kTexLim && std::fabs(v) < kTexLim) {
          double r, g, bl, al = 255.0, fr_u, fr_v;
          if (!linear) {
            const int64_t iu0 = floor_i64(u), iv0 = floor_i64(v);
            fr_u = u - iu0;
            fr_v = v - iv0;
            const uint8_t *tp = &td[(static_cast<size_t>(wrap_fast(iv0, th)) * tw + wrap_fast(iu0, tw)) * bpp];
            r = tp[0]; g = tp[1]; bl = tp[2];
            if (bpp == 4) al = tp[3];
          } else {
            const double fu = u - 0.5, fv = v - 0.5;
            const int64_t i0 = floor_i64(fu), j0 = floor_i64(fv);
            const double ax = fu - i0, ay = fv - j0;
            fr_u = ax;
            fr_v = ay;
            const int x0 = wrap_fast(i0, tw), x1 = wrap_fast(i0 + 1, tw);
            const size_t ry0 = static_cast<size_t>(wrap_fast(j0, th)) * tw, ry1 = static_cast<size_t>(wrap_fast(j0 + 1, th)) * tw;
            const uint8_t *p00 = &td[(ry0 + x0) * bpp], *p01 = &td[(ry0 + x1) * bpp];
            const uint8_t *p10 = &td[(ry1 + x0) * bpp], *p11 = &td[(ry1 + x1) * bpp];
            const double w00 = (1 - ax) * (1 - ay), w01 = ax * (1 - ay), w10 = (1 - ax) * ay, w11 = ax * ay;
            r = w00 * p00[0] + w01 * p01[0] + w10 * p10[0] + w11 * p11[0];
            g = w00 * p00[1] + w01 * p01[1] + w10 * p10[1] + w11 * p11[1];
            bl = w00 * p00[2] + w01 * p01[2] + w10 * p10[2] + w11 * p11[2];
            if (bpp == 4) al = w00 * p00[3] + w01 * p01[3] + w10 * p10[3] + w11 * p11[3];
          }
          if (fr_u > kTexEps && fr_u < 1.0 - kTexEps && fr_v > kTexEps && fr_v < 1.0 - kTexEps) {
            if (modulate) {
              r *= white ? 1.0 : (a0 * r0 + a1 * r1 + a2 * r2) * inv_a;
              g *= white ? 1.0 : (a0 * g0 + a1 * g1 + a2 * g2) * inv_a;
              bl *= white ? 1.0 : (a0 * b0 + a1 * b1 + a2 * b2) * inv_a;
            }
            const uint16_t dsts = row[x];
            if (blend) {
              const double alpha = al * kInv255, beta = 1 - alpha;
              r = r * alpha + tb.expand5[(dsts >> 10) & 31] * beta;
              g = g * alpha + tb.expand5[(dsts >> 5) & 31] * beta;
              bl = bl * alpha + tb.expand5[dsts & 31] * beta;
            }
            const double d = drow[(x + j.ddx) & 3];
            const double qr = r * k31of255 + d, qg = g * k31of255 + d, qb = bl * k31of255 + d;
            const int orr = floor_int(qr), og = floor_int(qg), ob = floor_int(qb);
            const double fr = qr - orr, fg = qg - og, fb = qb - ob;
            if (sloppy || (fr > kQuantEps && fr < 1.0 - kQuantEps && fg > kQuantEps && fg < 1.0 - kQuantEps && fb > kQuantEps &&
                           fb < 1.0 - kQuantEps)) {
              auto clamp5 = [](int o) { return o < 0 ? 0 : (o > 31 ? 31 : o); };
              row[x] = static_cast<uint16_t>((clamp5(orr) << 10) | (clamp5(og) << 5) | clamp5(ob));
              if (blend) ++blended;
              ++drawn;
              ++fastn;
              continue;
            }
          }
        }
      }
      if (wpx && j.wfinal && !blend && modulate) {   // the walk's float32 final stage, as the general loop
        row[x] = walk_final(td, tw, th, bpp, linear, wacc, static_cast<float>(drow[(x + j.ddx) & 3]));
        ++drawn;
        continue;
      }
      // the exact branch: the general loop's expressions (the walked values with tune 32)
      const double s = (a0 * s0 + a1 * s1 + a2 * s2) / div_st;
      const double t = (a0 * t0 + a1 * t1 + a2 * t2) / div_st;
      const double u = wpx ? static_cast<double>(wacc[0]) : s * twd, v = wpx ? static_cast<double>(wacc[1]) : t * thd;
      double r, g, bl, al = 255.0;
      if (!linear) {
        const int iu = wrap_fast(floor_i64(u), tw), iv = wrap_fast(floor_i64(v), th);
        const uint8_t *tp = &td[(static_cast<size_t>(iv) * tw + iu) * bpp];
        r = tp[0]; g = tp[1]; bl = tp[2];
        if (bpp == 4) al = tp[3];
      } else {
        const double fu = u - 0.5, fv = v - 0.5;
        const int64_t i0 = floor_i64(fu), j0 = floor_i64(fv);
        const double ax = fu - i0, ay = fv - j0;
        const int x0 = wrap_fast(i0, tw), x1 = wrap_fast(i0 + 1, tw);
        const size_t ry0 = static_cast<size_t>(wrap_fast(j0, th)) * tw, ry1 = static_cast<size_t>(wrap_fast(j0 + 1, th)) * tw;
        const uint8_t *p00 = &td[(ry0 + x0) * bpp], *p01 = &td[(ry0 + x1) * bpp];
        const uint8_t *p10 = &td[(ry1 + x0) * bpp], *p11 = &td[(ry1 + x1) * bpp];
        const double w00 = (1 - ax) * (1 - ay), w01 = ax * (1 - ay), w10 = (1 - ax) * ay, w11 = ax * ay;
        double acc[4] = {0, 0, 0, 0};
        for (int k = 0; k < bpp; ++k) acc[k] += w00 * p00[k];
        for (int k = 0; k < bpp; ++k) acc[k] += w01 * p01[k];
        for (int k = 0; k < bpp; ++k) acc[k] += w10 * p10[k];
        for (int k = 0; k < bpp; ++k) acc[k] += w11 * p11[k];
        r = acc[0]; g = acc[1]; bl = acc[2];
        if (bpp == 4) al = acc[3];
      }
      double cr = (a0 * r0 + a1 * r1 + a2 * r2) / arA, cg = (a0 * g0 + a1 * g1 + a2 * g2) / arA;
      double cb = (a0 * b0 + a1 * b1 + a2 * b2) / arA;
      if (wpx) { cr = wacc[2] / 31.0; cg = wacc[3] / 31.0; cb = wacc[4] / 31.0; }
      if (white) cr = cg = cb = 1.0;
      if (modulate) { r *= cr; g *= cg; bl *= cb; }
      uint16_t &dst = row[x];
      double ro = r, go = g, bo = bl;
      if (blend) {
        const double alpha = al / 255.0;
        const int dr = ((dst >> 10) & 31), dg = ((dst >> 5) & 31), db = dst & 31;
        ro = r * alpha + tb.expand5[dr] * (1 - alpha);
        go = g * alpha + tb.expand5[dg] * (1 - alpha);
        bo = bl * alpha + tb.expand5[db] * (1 - alpha);
        ++blended;
      }
      const double d = drow[(x + j.ddx) & 3];
      dst = static_cast<uint16_t>((quantize_d(ro, d) << 10) | (quantize_d(go, d) << 5) | quantize_d(bo, d));
      ++drawn;
    }
    if (j.wctl && parts > 1 && part == parts - 1) row[xl] ^= 1;   // negative control: a wrong pixel from the last worker only
  }
  c[0] += drawn;
  c[1] += blended;
  c[2] += fastn;
}

// ------------------------------------------------------------------------------------------------ recorded streams
bool GlRaster::exec_line(const std::string &line) {
  if (line.rfind("#tune ", 0) == 0) {
    char *e = nullptr;
    const long i = std::strtol(line.c_str() + 6, &e, 10);
    const long v = std::strtol(e, nullptr, 10);
    if (i >= 0 && i < kTunes) tune[i] = static_cast<int>(v);
    return true;
  }
  if (line.rfind("#threads ", 0) == 0) {
    set_threads(static_cast<int>(std::strtol(line.c_str() + 9, nullptr, 10)));
    return true;
  }
  if (line.rfind("#workerctl ", 0) == 0) {
    finish();
    worker_ctl = static_cast<int>(std::strtol(line.c_str() + 11, nullptr, 10));
    return true;
  }
  if (line.rfind("#fastpath ", 0) == 0) {
    fast_mode = static_cast<int>(std::strtol(line.c_str() + 10, nullptr, 10));
    return true;
  }
  if (line.empty() || line[0] == '#') return true;
  const size_t sp = line.find(' ');
  const std::string name = line.substr(0, sp);
  std::vector<uint32_t> a, ids;
  std::vector<uint32_t> vbits;
  std::string tex;
  if (sp != std::string::npos) {
    size_t i = sp + 1;
    while (i < line.size()) {
      size_t j = line.find(' ', i);
      if (j == std::string::npos) j = line.size();
      const std::string tok = line.substr(i, j - i);
      auto parse_list = [](const std::string &s, size_t from, std::vector<uint32_t> &out) {
        size_t k = from;
        while (k < s.size()) {
          char *e = nullptr;
          out.push_back(static_cast<uint32_t>(std::strtoul(s.c_str() + k, &e, 16)));
          k = static_cast<size_t>(e - s.c_str());
          if (k < s.size() && s[k] == ',') ++k;
        }
      };
      if (tok.rfind("v=", 0) == 0) parse_list(tok, 2, vbits);
      else if (tok.rfind("ids=", 0) == 0) parse_list(tok, 4, ids);
      else if (tok.rfind("tex=", 0) == 0) tex = tok.substr(4);
      else if (tok.rfind("p=", 0) == 0) {}
      else parse_list(tok, 0, a);
      i = j + 1;
    }
  }
  auto A = [&](size_t k) { return k < a.size() ? a[k] : 0u; };
  auto F = [&](size_t k) { return bits_f(A(k)); };
  auto D = [&](size_t k) { return bits_d(A(2 * k), A(2 * k + 1)); };
  float f3[3] = {0, 0, 0};
  float f2[2] = {0, 0};
  for (size_t k = 0; k < 3 && k < vbits.size(); ++k) f3[k] = bits_f(vbits[k]);
  for (size_t k = 0; k < 2 && k < vbits.size(); ++k) f2[k] = bits_f(vbits[k]);
  float f4[4] = {0, 0, 0, 0};
  for (size_t k = 0; k < 4 && k < vbits.size(); ++k) f4[k] = bits_f(vbits[k]);
  if (name == "glMaterialfv") { material(A(0), A(1), f4); return true; }
  if (name == "glMateriali") { material_shininess(A(0), static_cast<int32_t>(A(2))); return true; }
  if (name == "glLightfv") { light(A(0), A(1), f4); return true; }
  if (name == "glNormal3fv") { normal3fv(f3); return true; }
  if (name == "glNormal3f") { normal3f(F(0), F(1), F(2)); return true; }
  if (name == "glTexParameterf") { tex_parameterf(A(0), A(1), F(2)); return true; }
  if (name == "glEnableClientState" || name == "glHint" || name == "glGenTextures" ||
      name == "glVertexPointer" || name == "glNormalPointer" || name == "glLineWidth")
    return true;
  if (name == "glColor4f") { color4f(F(0), F(1), F(2), F(3)); return true; }
  if (name == "glFrontFace") front_face(A(0));
  else if (name == "glEnable") enable(A(0));
  else if (name == "glDisable") disable(A(0));
  else if (name == "glVertex3fv") vertex3fv(f3);
  else if (name == "glTexCoord2fv") tex_coord2fv(f2);
  else if (name == "glArrayElement") array_element(static_cast<int32_t>(A(0)));
  else if (name == "glTexParameteri") tex_parameteri(A(0), A(1), static_cast<int32_t>(A(2)));
  else if (name == "glTexEnvi") tex_envi(A(0), A(1), static_cast<int32_t>(A(2)));
  else if (name == "glDeleteTextures") delete_textures(static_cast<int32_t>(ids.size()), ids.data());
  else if (name == "glBindTexture") bind_texture(A(0), A(1));
  else if (name == "glTexImage2D") {
    const auto it = tex_bytes_.find(tex);
    if (it == tex_bytes_.end()) return true;   // a pre-context upload the recorder has no bytes for
    tex_image2d(A(0), static_cast<int32_t>(A(1)), static_cast<int32_t>(A(2)), static_cast<int32_t>(A(3)), static_cast<int32_t>(A(4)),
                static_cast<int32_t>(A(5)), A(6), A(7), it->second.data());
  }
  else if (name == "glViewport") viewport(static_cast<int32_t>(A(0)), static_cast<int32_t>(A(1)), static_cast<int32_t>(A(2)), static_cast<int32_t>(A(3)));
  else if (name == "glScalef") { Mat r; std::memset(r.m, 0, sizeof r.m); r.m[0] = F(0); r.m[5] = F(1); r.m[10] = F(2); r.m[15] = 1.0; mul(r); }
  else if (name == "glTexCoord2f") tex_coord2f(F(0), F(1));
  else if (name == "glVertex3f") vertex3f(F(0), F(1), F(2));
  else if (name == "glRotated") rotated(D(0), D(1), D(2), D(3));
  else if (name == "glRotatef") rotatef(F(0), F(1), F(2), F(3));
  else if (name == "glTranslatef") translatef(F(0), F(1), F(2));
  else if (name == "glMatrixMode") matrix_mode(A(0));
  else if (name == "glPushMatrix") push_matrix();
  else if (name == "glLoadIdentity") load_identity();
  else if (name == "glOrtho") ortho(D(0), D(1), D(2), D(3), D(4), D(5));
  else if (name == "glBlendFunc") blend_func(A(0), A(1));
  else if (name == "glBegin") begin(A(0));
  else if (name == "glEnd") end();
  else if (name == "glPopMatrix") pop_matrix();
  else if (name == "glFlush") flush();
  else return false;
  return true;
}

}  // namespace gl

extern const ModuleDef kModule_gl_raster = {"gl_raster", nullptr, 0};

}  // namespace portsim
