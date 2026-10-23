// The port's software rasteriser (phase 5 step 3). See portsim/sim/gl_raster.h. Module file only because every file of
// src/sim is a module; it registers no function (the C entry points are sg_raster_* in portsim_c.h).
#include "portsim/sim/gl_raster.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

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
// Where the original evaluates colours and texture coordinates: not at the pixel centre but at a position scaled about the
// viewport centre, x' = c + (x - c) * (1 + e). Fitted from integer-scale texture probes (port/test/glshift_fit.py): e = 1.046e-5
// in x, 1.355e-5 in y, no offset, no cross term (rms residual 0.0008 pixel). Coverage is NOT shifted. tune[6] / tune[15]
// override e in units of 1e-9 (0 = the fitted constant, -1 = none).
constexpr double kAttrScaleX = 1.046e-5, kAttrScaleY = 1.355e-5;
int wrap_repeat(int64_t i, uint32_t n) {
  const int64_t m = i % static_cast<int64_t>(n);
  return static_cast<int>(m < 0 ? m + n : m);
}
}  // namespace

GlRaster::GlRaster(int width, int height) : w_(width), h_(height), fb_(static_cast<size_t>(width) * height, 0) {
  std::memset(mv_.m, 0, sizeof mv_.m);
  std::memset(proj_.m, 0, sizeof proj_.m);
  mv_.m[0] = mv_.m[5] = mv_.m[10] = mv_.m[15] = 1.0;
  proj_ = mv_;
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
void GlRaster::load_identity() {
  Mat &a = cur();
  std::memset(a.m, 0, sizeof a.m);
  a.m[0] = a.m[5] = a.m[10] = a.m[15] = 1.0;
}
void GlRaster::push_matrix() { stack_[mode_ == kProjection ? 1 : 0].push_back(cur()); }
void GlRaster::pop_matrix() {
  auto &st = stack_[mode_ == kProjection ? 1 : 0];
  if (st.empty()) return;
  cur() = st.back();
  st.pop_back();
}
void GlRaster::matrix_mode(uint32_t mode) { mode_ = mode; }
void GlRaster::rotated(double angle, double x, double y, double z) {
  const double n = std::sqrt(x * x + y * y + z * z);
  if (n == 0) return;
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
  for (int32_t i = 0; i < n; ++i) names[i] = next_name_++;
  return true;
}
void GlRaster::color4f(float r, float g, float b, float a) { cur_c_[0] = r; cur_c_[1] = g; cur_c_[2] = b; cur_c_[3] = a; }
void GlRaster::normal3f(float x, float y, float z) { cur_n_[0] = x; cur_n_[1] = y; cur_n_[2] = z; }
void GlRaster::tex_image2d(uint32_t, int32_t level, int32_t, int32_t w, int32_t h, int32_t, uint32_t format, uint32_t, const uint8_t *data) {
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
  if (!ctx_) return;
  for (int i = 0; i < n; ++i) textures_.erase(ids[i]);
}
void GlRaster::bind_texture(uint32_t, uint32_t id) {
  if (ctx_) bound_ = id;
}
void GlRaster::set_arrays(const float *v, size_t nv, const float *n, size_t nn) {
  varr_.assign(v, v + nv * 3);
  narr_.assign(n, n + nn * 3);
}
void GlRaster::flush() {}
void GlRaster::tex_coord2fv(const float *v) { cur_s_ = v[0]; cur_t_ = v[1]; }
void GlRaster::tex_coord2f(float s, float t) { cur_s_ = s; cur_t_ = t; }
void GlRaster::normal3fv(const float *v) { cur_n_[0] = v[0]; cur_n_[1] = v[1]; cur_n_[2] = v[2]; }

// ------------------------------------------------------------------------------------------------ primitives
void GlRaster::begin(uint32_t mode) {
  prim_ = mode;
  in_begin_ = true;
  verts_.clear();
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
  v.s = cur_s_;
  v.t = cur_t_;
  v.r = cur_c_[0]; v.g = cur_c_[1]; v.b = cur_c_[2]; v.a = cur_c_[3];
  const bool f32 = tune[12] != 0;   // experiment: round every lighting intermediate to float32
  auto Q = [f32](double x) { return f32 ? static_cast<double>(static_cast<float>(x)) : x; };
  if (lighting_) {
    // eye-space normal = the inverse transpose of the modelview's upper 3x3 applied to the object normal
    const double a0 = m[0], a1 = m[4], a2 = m[8], b0 = m[1], b1 = m[5], b2 = m[9], c0 = m[2], c1 = m[6], c2 = m[10];
    const double det = a0 * (b1 * c2 - b2 * c1) - a1 * (b0 * c2 - b2 * c0) + a2 * (b0 * c1 - b1 * c0);
    const double i00 = (b1 * c2 - b2 * c1) / det, i01 = (a2 * c1 - a1 * c2) / det, i02 = (a1 * b2 - a2 * b1) / det;
    const double i10 = (b2 * c0 - b0 * c2) / det, i11 = (a0 * c2 - a2 * c0) / det, i12 = (a2 * b0 - a0 * b2) / det;
    const double i20 = (b0 * c1 - b1 * c0) / det, i21 = (a1 * c0 - a0 * c1) / det, i22 = (a0 * b1 - a1 * b0) / det;
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
        const double f = Q(nh > 0 ? std::pow(nh, shininess_) : 0.0);
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
  ++stats.triangles;
  const double area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
  if (area == 0) return;
  const bool front = (front_ == 0x0901) ? area > 0 : area < 0;
  if (cull_ && (tune[11] == 7 ? front : !front)) { ++stats.culled; return; }
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
  // top-left tie-break: an edge owns the pixel centres exactly on it when it points down, or left on a horizontal edge
  const int tie = tune[10];   // experiment: which side of a shared edge owns the pixel centres lying exactly on it
  auto inclusive = [tie](const Vtx &p, const Vtx &q) {
    const double ex = q.x - p.x, ey = q.y - p.y;
    if (tie == 4) return false;
    if (tie == 5) return true;
    return ((tie & 1) ? ey < 0 : ey > 0) || (ey == 0 && ((tie & 2) ? ex > 0 : ex < 0));
  };
  const bool inc0 = inclusive(*v1, *v2), inc1 = inclusive(*v2, *v0), inc2 = inclusive(*v0, *v1);
  for (int y = miny; y <= maxy; ++y) {
    for (int x = minx; x <= maxx; ++x) {
      const double px = x + 0.5, py = y + 0.5;
      const double w0 = edge(v1->x, v1->y, v2->x, v2->y, px, py);
      const double w1 = edge(v2->x, v2->y, v0->x, v0->y, px, py);
      const double w2 = edge(v0->x, v0->y, v1->x, v1->y, px, py);
      if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      if ((w0 == 0 && !inc0) || (w1 == 0 && !inc1) || (w2 == 0 && !inc2)) continue;
      // the attributes are evaluated at the shifted position (spx, spy); coverage above used the pixel centre
      const double ex_ = tune[6] == 0 ? kAttrScaleX : tune[6] < 0 ? 0.0 : tune[6] * 1e-9;
      const double ey_ = tune[15] == 0 ? kAttrScaleY : tune[15] < 0 ? 0.0 : tune[15] * 1e-9;
      const double spx = px + (px - (vp_x_ + vp_w_ * 0.5)) * ex_, spy = py + (py - (vp_y_ + vp_h_ * 0.5)) * ey_;
      double a0 = edge(v1->x, v1->y, v2->x, v2->y, spx, spy);
      double a1 = edge(v2->x, v2->y, v0->x, v0->y, spx, spy);
      double a2 = edge(v0->x, v0->y, v1->x, v1->y, spx, spy);
      if (tune[13] != 0 || tune[14] != 0) {   // experiment: a further shift of 1/256 pixel per unit
        const double sx = tune[13] / 256.0, sy = tune[14] / 256.0;
        a0 += (v1->y - v2->y) * sx + (v2->x - v1->x) * sy;
        a1 += (v2->y - v0->y) * sx + (v0->x - v2->x) * sy;
        a2 += (v0->y - v1->y) * sx + (v1->x - v0->x) * sy;
      }
      // Nearest sampling is exact at the pixel centre (probes: 0 differences at 0.25..0.75 texel per pixel without any shift);
      // the bilinear filter and the interpolated colours are evaluated at the shifted position.
      const double s = linear ? (a0 * v0->s + a1 * v1->s + a2 * v2->s) / ar : (w0 * v0->s + w1 * v1->s + w2 * v2->s) / ar;
      const double t = linear ? (a0 * v0->t + a1 * v1->t + a2 * v2->t) / ar : (w0 * v0->t + w1 * v1->t + w2 * v2->t) / ar;
      double u = s * tex.w, v = t * tex.h;
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
      double cr = (a0 * v0->r + a1 * v1->r + a2 * v2->r) / ar, cg = (a0 * v0->g + a1 * v1->g + a2 * v2->g) / ar;
      double cb = (a0 * v0->b + a1 * v1->b + a2 * v2->b) / ar;
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
      dst = static_cast<uint16_t>((quantize(ro, x, y, ctl) << 10) | (quantize(go, x, y, ctl) << 5) | quantize(bo, x, y, ctl));
      ++stats.drawn_px;
    }
  }
}

// ------------------------------------------------------------------------------------------------ recorded streams
bool GlRaster::exec_line(const std::string &line) {
  if (line.rfind("#tune ", 0) == 0) {
    char *e = nullptr;
    const long i = std::strtol(line.c_str() + 6, &e, 10);
    const long v = std::strtol(e, nullptr, 10);
    if (i >= 0 && i < 16) tune[i] = static_cast<int>(v);
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
