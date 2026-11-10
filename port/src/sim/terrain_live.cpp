// LiveTerrain: the TerrainApi over the port's own Terrain.dll state (terrain_live.h). Tests: port/test/test_terrain_live.py, which
// boots the port on a pinned scenario's trace with this interface attached and compares the calls and the state with a recording
// of the original on the same scenario (re/tools/terrain_calls.py --scenario).
#define _CRT_SECURE_NO_WARNINGS  // the development switch SG_LIVE_GETTERS uses getenv
#include "portsim/sim/terrain_live.h"

#include <algorithm>  // P10A: std::equal / std::fill (world_frame, world_cursor)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/terrain_render.h"
#include "portsim/sim/tick.h"

namespace portsim {

extern int g_negctl_gfx_terrain;   // gfx_terrain.cpp

namespace {
// The display surface's DC that initSystem receives: a headless run's engine has none (0), the port's display does exist (the
// rasteriser draws the terrain), so the interface hands initSystem a nonzero token in its place. Terrain.dll uses it only to create
// its GL context (initSystem tests it against 0).
constexpr uint32_t kDcToken = 1;

std::string hex(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%x", v);
  return b;
}

// P13D item 4 (terrain_live.h LiveTerrainWorld::tex_mag 2): a 2D RGB / RGBA texture image scaled 2 x 2 with the Catmull-Rom cubic
// (a = -0.5), the texture's own repeat wrap at its edges (the terrain sets GL_REPEAT), each channel rounded and clamped to 0..255.
// Sampled with GL_LINEAR at the window's resolution this keeps a texel's detail sharper than GL_LINEAR on the 64 x 64 image (soft)
// without GL_NEAREST's blocks. Output texel (X, Y) sits at source position ((X + 0.5) / 2 - 0.5, (Y + 0.5) / 2 - 0.5): the even and
// odd outputs are a quarter texel before / after each source texel, so the weights are two fixed sets.
std::vector<uint8_t> upscale2_cubic(const uint8_t *d, int32_t w, int32_t h, int bpp) {
  // weights of source texels i - 2 .. i + 1 for the output at i - 0.25 (even) and of i - 1 .. i + 2 for i + 0.25 (odd)
  auto cr = [](double t) {   // Catmull-Rom kernel, a = -0.5
    t = t < 0 ? -t : t;
    if (t < 1.0) return 1.5 * t * t * t - 2.5 * t * t + 1.0;
    if (t < 2.0) return -0.5 * t * t * t + 2.5 * t * t - 4.0 * t + 2.0;
    return 0.0;
  };
  double we[4], wo[4];
  for (int k = 0; k < 4; ++k) {
    we[k] = cr(-0.25 - (k - 2));   // source i - 2 + k, position i - 0.25
    wo[k] = cr(0.25 - (k - 1));    // source i - 1 + k, position i + 0.25
  }
  auto wrap = [](int32_t v, int32_t n) { return ((v % n) + n) % n; };
  const int32_t W = w * 2, H = h * 2;
  // pass 1: horizontal (w -> W) into doubles; pass 2: vertical (h -> H)
  std::vector<double> mid(static_cast<size_t>(W) * static_cast<size_t>(h) * static_cast<size_t>(bpp));
  for (int32_t y = 0; y < h; ++y)
    for (int32_t X = 0; X < W; ++X) {
      const int32_t i = X / 2;
      const bool odd = (X & 1) != 0;
      const double *wt = odd ? wo : we;
      const int32_t first = odd ? i - 1 : i - 2;
      for (int c = 0; c < bpp; ++c) {
        double acc = 0.0;
        for (int k = 0; k < 4; ++k)
          acc += wt[k] * d[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(wrap(first + k, w))) * bpp + c];
        mid[(static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(X)) * bpp + c] = acc;
      }
    }
  std::vector<uint8_t> out(static_cast<size_t>(W) * static_cast<size_t>(H) * static_cast<size_t>(bpp));
  for (int32_t Y = 0; Y < H; ++Y) {
    const int32_t j = Y / 2;
    const bool odd = (Y & 1) != 0;
    const double *wt = odd ? wo : we;
    const int32_t first = odd ? j - 1 : j - 2;
    for (int32_t X = 0; X < W; ++X)
      for (int c = 0; c < bpp; ++c) {
        double acc = 0.0;
        for (int k = 0; k < 4; ++k)
          acc += wt[k] * mid[(static_cast<size_t>(wrap(first + k, h)) * static_cast<size_t>(W) + static_cast<size_t>(X)) * bpp + c];
        const long v = std::lround(acc);
        out[(static_cast<size_t>(Y) * static_cast<size_t>(W) + static_cast<size_t>(X)) * bpp + c] =
            static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
      }
  }
  return out;
}

// P8B: the GL setup's fan to the primary sink (the 800 x 600 rasteriser, or SetupPair with the host's OpenGL) and the world layer's
// rasteriser (LiveTerrainWorld). Every call goes to both in the same order; the world's viewport is the setup's mapped into the
// world's pixels (x * fw / 800 + ox, y * ph / 600); texture names are the primary's (the world's must come out equal: both
// rasterisers start empty and hand out the lowest free name, gl_raster.h; a difference turns the world layer off).
class WorldFan : public gl::GlSink {
 public:
  WorldFan(gl::GlSink *a, LiveTerrainWorld &w) : a_(a), w_(w), b_(&w.sink()) {}   // P15A fix 3: the world's sink (tee or raster)
  void load_identity() override { a_->load_identity(); b_->load_identity(); }
  void push_matrix() override { a_->push_matrix(); b_->push_matrix(); }
  void pop_matrix() override { a_->pop_matrix(); b_->pop_matrix(); }
  void rotated(double an, double x, double y, double z) override { a_->rotated(an, x, y, z); b_->rotated(an, x, y, z); }
  void rotatef(float an, float x, float y, float z) override { a_->rotatef(an, x, y, z); b_->rotatef(an, x, y, z); }
  void translatef(float x, float y, float z) override { a_->translatef(x, y, z); b_->translatef(x, y, z); }
  void bind_texture(uint32_t t, uint32_t id) override { a_->bind_texture(t, id); b_->bind_texture(t, id); }
  void begin(uint32_t m) override { a_->begin(m); b_->begin(m); }
  void end() override { a_->end(); b_->end(); }
  void tex_coord2fv(const float *v) override { a_->tex_coord2fv(v); b_->tex_coord2fv(v); }
  void tex_coord2f(float s, float t) override { a_->tex_coord2f(s, t); b_->tex_coord2f(s, t); }
  void array_element(int32_t i) override { a_->array_element(i); b_->array_element(i); }
  void normal3fv(const float *v) override { a_->normal3fv(v); b_->normal3fv(v); }
  void vertex3fv(const float *v) override { a_->vertex3fv(v); b_->vertex3fv(v); }
  void vertex3f(float x, float y, float z) override { a_->vertex3f(x, y, z); b_->vertex3f(x, y, z); }
  void enable(uint32_t c) override { a_->enable(c); b_->enable(c); }
  void disable(uint32_t c) override { a_->disable(c); b_->disable(c); }
  void blend_func(uint32_t s, uint32_t d) override { a_->blend_func(s, d); b_->blend_func(s, d); }
  void flush() override { a_->flush(); b_->flush(); }
  void context_create() override { a_->context_create(); b_->context_create(); }
  void hint(uint32_t t, uint32_t m) override { a_->hint(t, m); b_->hint(t, m); }
  void enable_client_state(uint32_t a) override { a_->enable_client_state(a); b_->enable_client_state(a); }
  void front_face(uint32_t m) override { a_->front_face(m); b_->front_face(m); }
  void viewport(int32_t x, int32_t y, int32_t w, int32_t h) override {
    a_->viewport(x, y, w, h);
    vp_[0] = x;
    vp_[1] = y;
    vp_[2] = w;
    vp_[3] = h;
    has_vp_ = true;
    remap();
  }
  // the last setup viewport mapped into the world's current size (G32: called again after a resize)
  void remap() {
    if (!has_vp_) return;
    const int64_t fw = w_.fw, ph = w_.ph;
    b_->viewport(static_cast<int32_t>(w_.ox + vp_[0] * fw / 800), static_cast<int32_t>(vp_[1] * ph / 600),
                 static_cast<int32_t>(vp_[2] * fw / 800), static_cast<int32_t>(vp_[3] * ph / 600));
  }
  void matrix_mode(uint32_t m) override { a_->matrix_mode(m); b_->matrix_mode(m); }
  void ortho(double l, double r, double bo, double t, double n, double f) override {
    a_->ortho(l, r, bo, t, n, f);
    b_->ortho(l, r, bo, t, n, f);
  }
  void light(uint32_t l, uint32_t p, const float *v) override { a_->light(l, p, v); b_->light(l, p, v); }
  void material(uint32_t f, uint32_t p, const float *v) override { a_->material(f, p, v); b_->material(f, p, v); }
  void material_shininess(uint32_t f, int32_t s) override { a_->material_shininess(f, s); b_->material_shininess(f, s); }
  bool gen_textures(int32_t n, uint32_t *names) override {
    const bool r = a_->gen_textures(n, names);
    if (r && n > 0) {
      std::vector<uint32_t> mine(static_cast<size_t>(n), 0);
      b_->gen_textures(n, mine.data());
      for (int32_t i = 0; i < n; ++i)
        if (mine[static_cast<size_t>(i)] != names[i]) w_.names_differ = true;
    }
    return r;
  }
  void delete_textures(int32_t n, const uint32_t *names) override { a_->delete_textures(n, names); b_->delete_textures(n, names); }
  void tex_parameteri(uint32_t t, uint32_t p, int32_t v) override {
    a_->tex_parameteri(t, p, v);
    // P13D item 4: GL_TEXTURE_MAG_FILTER GL_NEAREST -> GL_LINEAR on the world layer only (LiveTerrainWorld::tex_mag)
    b_->tex_parameteri(t, p, w_.tex_mag >= 1 && p == 0x2800 && v == 0x2600 ? 0x2601 : v);
  }
  void tex_parameterf(uint32_t t, uint32_t p, float v) override { a_->tex_parameterf(t, p, v); b_->tex_parameterf(t, p, v); }
  void tex_envi(uint32_t t, uint32_t p, int32_t v) override { a_->tex_envi(t, p, v); b_->tex_envi(t, p, v); }
  void tex_image2d(uint32_t t, int32_t l, int32_t i, int32_t w, int32_t h, int32_t bo, uint32_t f, uint32_t ty,
                   const uint8_t *d) override {
    a_->tex_image2d(t, l, i, w, h, bo, f, ty, d);
    // P13D item 4: with tex_mag 2 the world's copy of a 2D RGB / RGBA byte image is uploaded 2 x 2 (upscale2_cubic)
    const int bpp = f == 0x1907 ? 3 : f == 0x1908 ? 4 : 0;   // GL_RGB / GL_RGBA
    if (w_.tex_mag == 2 && t == 0x0de1 && l == 0 && ty == 0x1401 && bpp != 0 && d && w > 1 && h > 1 && w <= 1024 && h <= 1024) {
      const std::vector<uint8_t> up = upscale2_cubic(d, w, h, bpp);
      b_->tex_image2d(t, l, i, w * 2, h * 2, bo, f, ty, up.data());
      return;
    }
    b_->tex_image2d(t, l, i, w, h, bo, f, ty, d);
  }
  void vertex_pointer(int32_t s, uint32_t t, int32_t st) override { a_->vertex_pointer(s, t, st); b_->vertex_pointer(s, t, st); }
  void normal_pointer(uint32_t t, int32_t st) override { a_->normal_pointer(t, st); b_->normal_pointer(t, st); }

 private:
  gl::GlSink *a_;
  LiveTerrainWorld &w_;
  gl::GlSink *b_;
  int32_t vp_[4] = {0, 0, 0, 0};
  bool has_vp_ = false;
};

// P15A fix 3 (terrain_live.h LiveTerrainWorld::rec): the world's GL calls to its raster, the check's masked raster and the recorder,
// in that order; texture names are the raster's
class WorldTee : public gl::GlSink {
 public:
  WorldTee(gl::GlRaster *r, gl::GlRaster *s, gl::GlRecord *rec) : r_(r), s_(s), c_(rec) {}
  template <class F>
  void all(F f) {
    f(static_cast<gl::GlSink &>(*r_));
    if (s_) f(static_cast<gl::GlSink &>(*s_));
    if (c_) f(static_cast<gl::GlSink &>(*c_));
  }
  void load_identity() override { all([](gl::GlSink &g) { g.load_identity(); }); }
  void push_matrix() override { all([](gl::GlSink &g) { g.push_matrix(); }); }
  void pop_matrix() override { all([](gl::GlSink &g) { g.pop_matrix(); }); }
  void rotated(double an, double x, double y, double z) override { all([&](gl::GlSink &g) { g.rotated(an, x, y, z); }); }
  void rotatef(float an, float x, float y, float z) override { all([&](gl::GlSink &g) { g.rotatef(an, x, y, z); }); }
  void translatef(float x, float y, float z) override { all([&](gl::GlSink &g) { g.translatef(x, y, z); }); }
  void bind_texture(uint32_t t, uint32_t id) override { all([&](gl::GlSink &g) { g.bind_texture(t, id); }); }
  void begin(uint32_t m) override { all([&](gl::GlSink &g) { g.begin(m); }); }
  void end() override { all([](gl::GlSink &g) { g.end(); }); }
  void tex_coord2fv(const float *v) override { all([&](gl::GlSink &g) { g.tex_coord2fv(v); }); }
  void tex_coord2f(float s, float t) override { all([&](gl::GlSink &g) { g.tex_coord2f(s, t); }); }
  void array_element(int32_t i) override { all([&](gl::GlSink &g) { g.array_element(i); }); }
  void normal3fv(const float *v) override { all([&](gl::GlSink &g) { g.normal3fv(v); }); }
  void vertex3fv(const float *v) override { all([&](gl::GlSink &g) { g.vertex3fv(v); }); }
  void vertex3f(float x, float y, float z) override { all([&](gl::GlSink &g) { g.vertex3f(x, y, z); }); }
  void enable(uint32_t c) override { all([&](gl::GlSink &g) { g.enable(c); }); }
  void disable(uint32_t c) override { all([&](gl::GlSink &g) { g.disable(c); }); }
  void blend_func(uint32_t s, uint32_t d) override { all([&](gl::GlSink &g) { g.blend_func(s, d); }); }
  void flush() override { all([](gl::GlSink &g) { g.flush(); }); }
  void context_create() override { all([](gl::GlSink &g) { g.context_create(); }); }
  void hint(uint32_t t, uint32_t m) override { all([&](gl::GlSink &g) { g.hint(t, m); }); }
  void enable_client_state(uint32_t a) override { all([&](gl::GlSink &g) { g.enable_client_state(a); }); }
  void front_face(uint32_t m) override { all([&](gl::GlSink &g) { g.front_face(m); }); }
  void viewport(int32_t x, int32_t y, int32_t w, int32_t h) override { all([&](gl::GlSink &g) { g.viewport(x, y, w, h); }); }
  void matrix_mode(uint32_t m) override { all([&](gl::GlSink &g) { g.matrix_mode(m); }); }
  void ortho(double l, double r, double bo, double t, double n, double f) override {
    all([&](gl::GlSink &g) { g.ortho(l, r, bo, t, n, f); });
  }
  void light(uint32_t l, uint32_t p, const float *v) override { all([&](gl::GlSink &g) { g.light(l, p, v); }); }
  void material(uint32_t f, uint32_t p, const float *v) override { all([&](gl::GlSink &g) { g.material(f, p, v); }); }
  void material_shininess(uint32_t f, int32_t s) override { all([&](gl::GlSink &g) { g.material_shininess(f, s); }); }
  bool gen_textures(int32_t n, uint32_t *names) override {
    const bool r = r_->gen_textures(n, names);
    if (n > 0 && s_) {
      std::vector<uint32_t> mine(static_cast<size_t>(n), 0);
      s_->gen_textures(n, mine.data());
    }
    if (c_) c_->gen_textures(n, names);
    return r;
  }
  void delete_textures(int32_t n, const uint32_t *names) override { all([&](gl::GlSink &g) { g.delete_textures(n, names); }); }
  void tex_parameteri(uint32_t t, uint32_t p, int32_t v) override { all([&](gl::GlSink &g) { g.tex_parameteri(t, p, v); }); }
  void tex_parameterf(uint32_t t, uint32_t p, float v) override { all([&](gl::GlSink &g) { g.tex_parameterf(t, p, v); }); }
  void tex_envi(uint32_t t, uint32_t p, int32_t v) override { all([&](gl::GlSink &g) { g.tex_envi(t, p, v); }); }
  void tex_image2d(uint32_t t, int32_t l, int32_t i, int32_t w, int32_t h, int32_t bo, uint32_t f, uint32_t ty,
                   const uint8_t *d) override {
    all([&](gl::GlSink &g) { g.tex_image2d(t, l, i, w, h, bo, f, ty, d); });
  }
  void vertex_pointer(int32_t s, uint32_t t, int32_t st) override { all([&](gl::GlSink &g) { g.vertex_pointer(s, t, st); }); }
  void normal_pointer(uint32_t t, int32_t st) override { all([&](gl::GlSink &g) { g.normal_pointer(t, st); }); }

 private:
  gl::GlRaster *r_, *s_;
  gl::GlRecord *c_;
};

// P15A fix 3: the mask of the pixels b_queue_begin samples from a pw x ph world (rows y * ph / 600 of the 600 logical rows; the
// columns of the logical side columns: left of mask_cx0 and from mask_cx1 on); set on raster (mask_on) and on the check's sample
void world_mask(LiveTerrainWorld &w) {
  if (!w.mask_on && !w.sample) return;
  w.mask_rows.assign(static_cast<size_t>(std::max<int32_t>(w.ph, 0)), 0);
  for (int32_t y = 0; y < 600; ++y) {
    int32_t wy = static_cast<int32_t>(static_cast<int64_t>(y) * w.ph / 600);
    if (wy >= w.ph) wy = w.ph - 1;
    if (wy >= 0) w.mask_rows[static_cast<size_t>(wy)] = 1;
  }
  // b_queue_begin's E and its world column of logical column fx (clamped to the world)
  const int32_t E = w.ox > 0 ? static_cast<int32_t>((static_cast<int64_t>(w.ox) * 800 + w.fw - 1) / w.fw) : 0;
  auto col = [&](int32_t fx) {
    int64_t wx = w.ox + static_cast<int64_t>(fx) * w.fw / 800;
    if (wx < 0) wx = 0;
    if (wx >= w.pw) wx = w.pw - 1;
    return static_cast<int32_t>(wx);
  };
  int32_t cx0 = 0, cx1 = w.pw;
  for (int32_t fx = -E; fx < 0; ++fx) cx0 = std::max(cx0, col(fx) + 1);
  for (int32_t fx = 800; fx < 800 + E; ++fx) cx1 = std::min(cx1, col(fx));
  w.mask_cx0 = cx0;
  w.mask_cx1 = std::max(cx0, cx1);
  if (w.mask_on) w.raster->set_mask(w.mask_rows, w.mask_cx0, w.mask_cx1);
  if (w.sample) w.sample->set_mask(w.mask_rows, w.mask_cx0, w.mask_cx1);
}

// the size-dependent parts of a world layer (set_world_view, resize_world)
void world_geometry(LiveTerrainWorld &w, int32_t pw, int32_t ph) {
  w.pw = pw;
  w.ph = ph;
  w.fw = static_cast<int32_t>(static_cast<int64_t>(ph) * 800 / 600);
  w.ox = (pw - w.fw) / 2;
  // the side margin in logical pixels: the ox world pixels left of the frame, at fw / 800 world pixels per logical one, rounded up
  w.margin = static_cast<int32_t>((static_cast<int64_t>(w.ox) * 800 + w.fw - 1) / w.fw) + 1;
  w.raster->set_clip_x(static_cast<float>(static_cast<double>(pw) / static_cast<double>(w.fw)));
  // the dither phase of the frame's own columns (window x - ox): exact at render scale 1; any scale keeps a 4x4 matrix
  w.raster->set_dither_dx(-w.ox);
  w.back = std::make_shared<const std::vector<uint16_t>>(static_cast<size_t>(pw) * static_cast<size_t>(ph), uint16_t{0});
  if (w.rec) {  // P15A fix 3: the masked back surfaces, and the check's raster
    w.back_m.assign(static_cast<size_t>(pw) * static_cast<size_t>(ph), 0);
    if (w.sample) {
      terrain_world_raster_geometry(*w.sample, pw, ph);
      w.sample_back.assign(w.back_m.size(), 0);
    }
  }
  world_mask(w);
}
// P15A fix 3: the world thread's raster made new at pw x ph (kGwReset), as set_world_view made this one
void record_reset(int32_t pw, int32_t ph, bool resize) {
  gfx_world_op(kGwReset, [pw, ph, resize](LiveTerrainWorld &W) {
    if (resize && W.raster) {
      W.raster->resize_frame(pw, ph);
      W.raster->set_gpu(gl::make_pixel_gpu(pw, ph));   // P16B fix 2: the backend chosen again for the new size (--gpu-terrain auto)
    } else {
      W.raster = std::make_unique<gl::GlRaster>(pw, ph);
      W.raster->no_context();
      W.raster->set_workers_normal(false);   // at the world thread's priority (the frame thread's work first)
      W.raster->set_threads(terrain_world_raster_threads());
      W.raster->set_gpu(gl::make_pixel_gpu(pw, ph));   // P16B: the app's pixel backend when one is registered (--gpu-terrain), else none
    }
    terrain_world_raster_geometry(*W.raster, pw, ph);
    W.back_m.assign(static_cast<size_t>(pw) * static_cast<size_t>(ph), 0);
  });
}
}  // namespace

void terrain_world_raster_geometry(gl::GlRaster &r, int32_t pw, int32_t ph) {
  const int32_t fw = static_cast<int32_t>(static_cast<int64_t>(ph) * 800 / 600);
  r.set_clip_x(static_cast<float>(static_cast<double>(pw) / static_cast<double>(fw)));
  r.set_dither_dx(-((pw - fw) / 2));
}

bool LiveTerrain::set_world_view(int32_t pw, int32_t ph, std::string *why) {
  auto fail = [why](const char *t) {
    if (why) *why = t;
    return false;
  };
  if (calls_ != 0) return fail("asked after the first Terrain call: the setup already went to the primary sink alone");
  if (ph < 600 / 4 || pw <= 0) return fail("a world layer smaller than a quarter of the frame");
  const int32_t fw = static_cast<int32_t>(static_cast<int64_t>(ph) * 800 / 600);
  if (pw < fw) return fail("a world layer narrower than the 800 x 600 frame's aspect");
  auto w = std::make_unique<LiveTerrainWorld>();
  w->raster = std::make_unique<gl::GlRaster>(pw, ph);
  w->raster->no_context();
  if (gfx_world_raster_defer()) {  // P15A fix 3 (terrain_live.h LiveTerrainWorld::rec)
    w->rec = true;
    w->mask_on = !gfx_hr_check_on();
    if (!w->mask_on) {
      w->sample = std::make_unique<gl::GlRaster>(pw, ph);
      w->sample->no_context();
    }
    w->recorder = std::make_unique<gl::GlRecord>([] { return gfx_world_gl_chunk(); });
    w->tee = std::make_unique<WorldTee>(w->raster.get(), w->sample.get(), w->recorder.get());
    record_reset(pw, ph, false);
  }
  world_geometry(*w, pw, ph);
  w->terrain800.assign(static_cast<size_t>(800) * 600, 0);
  world_ = std::move(w);
  world_fan_ = std::make_unique<WorldFan>(dll_.setup.b, *world_);
  dll_.setup.b = world_fan_.get();
  if (why) *why = "on";
  return true;
}

bool LiveTerrain::resize_world(int32_t pw, int32_t ph) {
  if (!world_ || ph < 600 / 4 || pw < static_cast<int32_t>(static_cast<int64_t>(ph) * 800 / 600)) return false;
  if (pw == world_->pw && ph == world_->ph) return true;
  world_->raster->resize_frame(pw, ph);
  ++world_->gen;  // P15A
  world_->grid_defer = world_->grid_prev_ok = false;  // P23D fix 1: a new root holds no grid
  world_->grid_prev.clear();
  world_->grid_cur.clear();
  if (world_->sample) world_->sample->resize_frame(pw, ph);  // P15A fix 3
  if (world_->rec) record_reset(pw, ph, true);
  world_geometry(*world_, pw, ph);
  if (world_fan_) static_cast<WorldFan *>(world_fan_.get())->remap();
  world_->valid = false;
  world_->dirty = true;
  world_->strip_full = false;
  ++world_->resizes;
  return true;
}

// P10A fix 2: the world image of a present in work proportional to the scene's sprite pixels. world_frame and world_cursor give
// the same pixels as before (P8B / G32 / P9C / P9E fix 4, the loops kept in the comments below); what changed is the order of the
// work. The scene is in logical pixels (scene_w x 600) and world column x shows scene column col(x) = floor((x - ox) * 800 / fw) +
// scene_e, which never decreases as x grows: scene column l covers the world columns [first[l], last[l]) (none when first[l] < 0).
// So each logical row's differing scene columns are found once (scene_w compares) and only their world columns are written, for
// every world row that shows that logical row (floor(y * 600 / ph)); before, every world pixel was visited (pw x ph, 2.07 M at
// 1920 x 1080) and world_cursor allocated and scanned its planes even when no preview was kept.
namespace {
struct SceneCols {
  std::vector<int32_t> first, last;
};
SceneCols scene_cols(const LiveTerrainWorld &w) {
  SceneCols c;
  c.first.assign(static_cast<size_t>(w.scene_w), -1);
  c.last.assign(static_cast<size_t>(w.scene_w), -1);
  for (int32_t x = 0; x < w.pw; ++x) {
    const int64_t d = static_cast<int64_t>(x - w.ox) * 800;
    const int32_t lx = static_cast<int32_t>(d >= 0 ? d / w.fw : -((-d + w.fw - 1) / w.fw)) + w.scene_e;
    if (lx < 0 || lx >= w.scene_w) continue;
    if (c.first[static_cast<size_t>(lx)] < 0) c.first[static_cast<size_t>(lx)] = x;
    c.last[static_cast<size_t>(lx)] = x + 1;
  }
  return c;
}
}  // namespace

bool LiveTerrain::world_frame(const std::vector<uint16_t> &back800, bool back_terrain, std::vector<uint16_t> &px,
                              std::vector<uint8_t> &mask, bool text, bool cursor_apart) const {
  if (!world_) return false;
  return world_frame_of(world_.get(), world_->raster->pixels(), back800, back_terrain, px, mask, text, cursor_apart);
}

bool world_frame_of(const LiveTerrainWorld *wp, const std::vector<uint16_t> &root, const std::vector<uint16_t> &back800, bool back_terrain,
                    std::vector<uint16_t> &px, std::vector<uint8_t> &mask, bool text, bool cursor_apart) {
  if (!wp || wp->names_differ || !wp->valid || !back_terrain) return false;
  if (back800.size() != wp->terrain800.size()) return false;
  px = root;
  mask.resize(back800.size());
  const LiveTerrainWorld &w = *wp;
  if (!w.scene_valid || w.s1.size() != back800.size() || w.scene.size() != static_cast<size_t>(w.scene_w) * 600) {
    for (size_t i = 0; i < back800.size(); ++i) mask[i] = back800[i] == w.terrain800[i] ? 1 : 0;
    return true;
  }
  // G32: the wide scene's sprite pixels over the world (nearest: world pixel (x, y) shows scene pixel (col(x), floor(y * 600 / ph)))
  // where the scene's pixel differs from scene_base. P9C: text = scene_text in place of the scene. P9E fix 4: cursor_apart = the
  // cursor passes' previews left out (scene_pre where scene_cur differs from it; not with scene_text, made before them).
  const bool use_text = text && w.scene_text.size() == w.scene.size();
  const std::vector<uint16_t> &scene = use_text ? w.scene_text : w.scene;
  const bool apart = cursor_apart && !use_text && w.scene_pre.size() == w.scene.size() && w.scene_cur.size() == w.scene.size();
  const SceneCols c = scene_cols(w);
  // P10D fix 1: the queue's sprites at the world's resolution (hr where hr_cov); the scene then gives only the pixels its tag
  // plane says something other than a queue sprite drew (labels, cursor passes)
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  const bool hr = w.hr_valid && w.hr.size() == np && w.hr_cov.size() == np && w.tag.size() == w.scene.size() && px.size() == np;
  if (hr)
    for (size_t i = 0; i < np; ++i)
      if (w.hr_cov[i]) px[i] = w.hr[i];
  const bool hr_pre = hr && w.tag_pre.size() == w.scene.size();
  // P10D fix 2: the cursor passes' previews (hrc, a copy of hr with them drawn) over it, unless they are taken apart (or the
  // picture is the one without the text runs, whose scene_text was made before the cursor passes)
  const bool hrc = hr && w.hrc_valid && w.hrc.size() == np && w.hrc_cov.size() == np;
  if (hrc && !cursor_apart && !text)
    for (size_t i = 0; i < np; ++i)
      if (w.hrc_cov[i]) px[i] = w.hrc[i];
  const bool pre_tags = cursor_apart || text;   // tag 2 (a cursor pass's sprite) reads the tag before the cursor passes
  struct Run {
    int32_t x0, x1;
    uint16_t v;
  };
  std::vector<Run> runs;
  int32_t row_ly = -1;
  for (int32_t y = 0; y < w.ph; ++y) {
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y) * 600 / w.ph);
    if (ly != row_ly) {  // the logical row's differing scene columns (consecutive world rows share it)
      row_ly = ly;
      runs.clear();
      const size_t r0 = static_cast<size_t>(ly) * static_cast<size_t>(w.scene_w);
      const uint16_t *sr = scene.data() + r0, *br = w.scene_base.data() + r0;
      const uint16_t *pr = apart ? w.scene_pre.data() + r0 : nullptr, *cr = apart ? w.scene_cur.data() + r0 : nullptr;
      for (int32_t lx = 0; lx < w.scene_w; ++lx) {
        const bool from_pre = apart && cr[lx] != pr[lx];
        const uint16_t v = from_pre ? pr[lx] : sr[lx];
        if (v == br[lx] || c.first[static_cast<size_t>(lx)] < 0) continue;
        if (hr) {   // P10D fix 1
          const size_t si = r0 + static_cast<size_t>(lx);
          uint8_t t = from_pre ? (hr_pre ? w.tag_pre[si] : 0) : w.tag[si];
          if (t == 2 && pre_tags) t = hr_pre ? w.tag_pre[si] : 0;   // fix 2: the pixel as it was before the cursor passes
          if (t != 0) continue;   // a queue sprite's pixel (1) or a cursor pass's sprite in hrc (2): the hr planes have it
          if (use_text && !from_pre && w.scene_text[si] != w.scene[si]) continue;   // a peeled glyph (P9C redraws it)
        }
        runs.push_back({c.first[static_cast<size_t>(lx)], c.last[static_cast<size_t>(lx)], v});
      }
    }
    uint16_t *dst = px.data() + static_cast<size_t>(y) * static_cast<size_t>(w.pw);
    for (const Run &r : runs) std::fill(dst + r.x0, dst + r.x1, r.v);
  }
  const bool s0_ok = w.s0.size() == back800.size();
  for (size_t i = 0; i < back800.size(); ++i)
    mask[i] = back800[i] == w.terrain800[i] || (back800[i] == w.s1[i] && (!s0_ok || w.s1[i] != w.s0[i])) ? 1 : 0;
  return true;
}

// P17B (terrain_live.h): world_frame_of above with the per-pixel copies left to the caller's GPU; every test and loop is the one above
bool world_frame_plan_of(const LiveTerrainWorld *wp, size_t root_n, size_t hr_n, size_t hrc_n, const std::vector<uint16_t> &back800,
                         bool back_terrain, bool text, bool cursor_apart, WorldFramePlan &plan, std::vector<uint8_t> &mask) {
  plan.lay_hr = plan.lay_hrc = false;
  plan.runs.clear();
  plan.colmap.clear();
  plan.rowmap.clear();
  plan.scene_w = 0;
  if (!wp || wp->names_differ || !wp->valid || !back_terrain) return false;
  if (back800.size() != wp->terrain800.size()) return false;
  mask.resize(back800.size());
  const LiveTerrainWorld &w = *wp;
  plan.pw = w.pw;
  plan.ph = w.ph;
  if (!w.scene_valid || w.s1.size() != back800.size() || w.scene.size() != static_cast<size_t>(w.scene_w) * 600) {
    for (size_t i = 0; i < back800.size(); ++i) mask[i] = back800[i] == w.terrain800[i] ? 1 : 0;
    return true;
  }
  const bool use_text = text && w.scene_text.size() == w.scene.size();
  const std::vector<uint16_t> &scene = use_text ? w.scene_text : w.scene;
  const bool apart = cursor_apart && !use_text && w.scene_pre.size() == w.scene.size() && w.scene_cur.size() == w.scene.size();
  const SceneCols c = scene_cols(w);
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  const bool hr = w.hr_valid && hr_n == np && w.tag.size() == w.scene.size() && root_n == np;
  plan.lay_hr = hr;
  const bool hr_pre = hr && w.tag_pre.size() == w.scene.size();
  const bool hrc = hr && w.hrc_valid && hrc_n == np;
  plan.lay_hrc = hrc && !cursor_apart && !text;
  const bool pre_tags = cursor_apart || text;
  plan.scene_w = w.scene_w;
  plan.runs.assign(static_cast<size_t>(w.scene_w) * 600, 0u);
  plan.colmap.assign(static_cast<size_t>(w.pw), -1);
  for (int32_t lx = 0; lx < w.scene_w; ++lx)
    if (c.first[static_cast<size_t>(lx)] >= 0)
      for (int32_t x = c.first[static_cast<size_t>(lx)]; x < c.last[static_cast<size_t>(lx)]; ++x) plan.colmap[static_cast<size_t>(x)] = lx;
  plan.rowmap.resize(static_cast<size_t>(w.ph));
  for (int32_t y = 0; y < w.ph; ++y) plan.rowmap[static_cast<size_t>(y)] = static_cast<int32_t>(static_cast<int64_t>(y) * 600 / w.ph);
  for (int32_t ly = 0; ly < 600; ++ly) {   // world_frame_of's logical row loop (its runs, per logical pixel)
    const size_t r0 = static_cast<size_t>(ly) * static_cast<size_t>(w.scene_w);
    const uint16_t *sr = scene.data() + r0, *br = w.scene_base.data() + r0;
    const uint16_t *pr = apart ? w.scene_pre.data() + r0 : nullptr, *cr = apart ? w.scene_cur.data() + r0 : nullptr;
    uint32_t *out = plan.runs.data() + r0;
    for (int32_t lx = 0; lx < w.scene_w; ++lx) {
      const bool from_pre = apart && cr[lx] != pr[lx];
      const uint16_t v = from_pre ? pr[lx] : sr[lx];
      if (v == br[lx] || c.first[static_cast<size_t>(lx)] < 0) continue;
      if (hr) {
        const size_t si = r0 + static_cast<size_t>(lx);
        uint8_t t = from_pre ? (hr_pre ? w.tag_pre[si] : 0) : w.tag[si];
        if (t == 2 && pre_tags) t = hr_pre ? w.tag_pre[si] : 0;
        if (t != 0) continue;
        if (use_text && !from_pre && w.scene_text[si] != w.scene[si]) continue;
      }
      out[lx] = 0x10000u | v;
    }
  }
  const bool s0_ok = w.s0.size() == back800.size();
  for (size_t i = 0; i < back800.size(); ++i)
    mask[i] = back800[i] == w.terrain800[i] || (back800[i] == w.s1[i] && (!s0_ok || w.s1[i] != w.s0[i])) ? 1 : 0;
  return true;
}

bool LiveTerrain::world_cursor(std::vector<uint16_t> &px, std::vector<uint8_t> &cov, std::vector<uint8_t> &mask800,
                               std::vector<uint16_t> *hi, const std::vector<uint16_t> *back800) const {
  return world_cursor_of(world_.get(), px, cov, mask800, hi, back800);
}

bool world_cursor_of(const LiveTerrainWorld *wp, std::vector<uint16_t> &px, std::vector<uint8_t> &cov, std::vector<uint8_t> &mask800,
                     std::vector<uint16_t> *hi, const std::vector<uint16_t> *back800) {
  if (hi) hi->clear();
  if (!wp || wp->names_differ || !wp->valid) return false;
  const LiveTerrainWorld &w = *wp;
  const size_t n = static_cast<size_t>(w.scene_w) * 600;
  if (!w.scene_valid || w.scene_w <= 0 || w.scene_pre.size() != n || w.scene_cur.size() != n) return false;
  // the logical rows / columns holding a preview pixel (scene_cur != scene_pre); none = nothing to give
  int32_t ly0 = 600, ly1 = -1, lx0 = w.scene_w, lx1 = -1;
  for (int32_t ly = 0; ly < 600; ++ly) {
    const size_t r0 = static_cast<size_t>(ly) * static_cast<size_t>(w.scene_w);
    if (std::equal(w.scene_cur.begin() + static_cast<std::ptrdiff_t>(r0), w.scene_cur.begin() + static_cast<std::ptrdiff_t>(r0 + w.scene_w),
                   w.scene_pre.begin() + static_cast<std::ptrdiff_t>(r0)))
      continue;
    ly0 = std::min(ly0, ly);
    ly1 = ly;
    for (int32_t lx = 0; lx < w.scene_w; ++lx)
      if (w.scene_cur[r0 + static_cast<size_t>(lx)] != w.scene_pre[r0 + static_cast<size_t>(lx)]) {
        lx0 = std::min(lx0, lx);
        lx1 = std::max(lx1, lx);
      }
  }
  // P10F: the placement box drawn at the world's resolution (box800, hrb_*), which the scene does not hold (the palm tool's only
  // preview)
  const size_t np = static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph);
  const bool box = w.hr_valid && w.hrc_valid && w.hrb_valid && w.box800.size() == static_cast<size_t>(800) * 600 &&
                   w.hrc.size() == np && w.hrb_lo.size() == np && w.hrb_hi.size() == np && w.hrb_cov.size() == np;
  if (ly1 < 0 && !box) {  // P10A: the planes are not made (the caller drops them on false)
    px.clear();
    cov.clear();
    mask800.clear();
    return false;
  }
  mask800.assign(static_cast<size_t>(800) * 600, 0);
  bool any = false;
  for (int32_t y = ly0; y <= ly1; ++y)
    for (int32_t x = std::max(0, lx0 - w.scene_e); x < 800 && x + w.scene_e <= lx1; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w.scene_w) + static_cast<size_t>(x + w.scene_e);
      if (w.scene_cur[i] != w.scene_pre[i]) {
        mask800[static_cast<size_t>(y) * 800 + static_cast<size_t>(x)] = 1;
        any = true;
      }
    }
  if (box) {
    const bool chk = back800 && back800->size() == w.box800.size() && w.box_word.size() == w.box800.size();
    for (size_t i = 0; i < w.box800.size(); ++i)
      if (w.box800[i] && (!chk || (*back800)[i] == w.box_word[i])) mask800[i] = 1;
  }
  px.assign(static_cast<size_t>(w.pw) * static_cast<size_t>(w.ph), 0);
  cov.assign(px.size(), 0);
  // P10D fix 2: the previews at the world's resolution (hrc where hrc_cov)
  if (w.hr_valid && w.hrc_valid && w.hrc.size() == px.size() && w.hrc_cov.size() == px.size()) {
    for (size_t j = 0; j < px.size(); ++j)
      if (w.hrc_cov[j]) {
        px[j] = w.hrc[j];
        cov[j] = 1;
        any = true;
      }
    if (box) {  // P10F: the box's pixels without a cursor sprite over them: the blend pair (with hi), else hrc's
      if (hi) hi->assign(px.size(), 0);
      for (int32_t Y = w.hrb_area[1]; Y < w.hrb_area[3]; ++Y)
        for (int32_t X = w.hrb_area[0]; X < w.hrb_area[2]; ++X) {
          const size_t j = static_cast<size_t>(Y) * static_cast<size_t>(w.pw) + static_cast<size_t>(X);
          if (!w.hrb_cov[j] || cov[j]) continue;
          px[j] = hi ? w.hrb_lo[j] : w.hrc[j];
          if (hi) (*hi)[j] = w.hrb_hi[j];
          cov[j] = hi ? 2 : 1;
          any = true;
        }
    }
    return any;
  }
  const SceneCols c = scene_cols(w);
  for (int32_t y = 0; y < w.ph; ++y) {
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y) * 600 / w.ph);
    if (ly < ly0 || ly > ly1) continue;
    const uint16_t *pr = w.scene_pre.data() + static_cast<size_t>(ly) * static_cast<size_t>(w.scene_w);
    const uint16_t *cr = w.scene_cur.data() + static_cast<size_t>(ly) * static_cast<size_t>(w.scene_w);
    for (int32_t lx = lx0; lx <= lx1; ++lx) {
      if (c.first[static_cast<size_t>(lx)] < 0 || cr[lx] == pr[lx]) continue;
      for (int32_t x = c.first[static_cast<size_t>(lx)]; x < c.last[static_cast<size_t>(lx)]; ++x) {
        const size_t j = static_cast<size_t>(y) * static_cast<size_t>(w.pw) + static_cast<size_t>(x);
        px[j] = cr[lx];
        cov[j] = 1;
        any = true;
      }
    }
  }
  return any;
}

LiveTerrain::LiveTerrain(Ctx &ctx, uint32_t va, std::vector<uint32_t> snap_after)
    : ctx_(ctx), engine_(make_headless_terrain(ctx)), snap_after_(std::move(snap_after)) {
  dll_.terrain = va;
  g_.vertices.assign(static_cast<size_t>(22500) * 3, 0.0f);   // the DLL's bss arrays: 22,500 vertices each
  g_.normals.assign(static_cast<size_t>(22500) * 3, 0.0f);
  // the rasteriser the GL setup also goes to (the window size initTerrain passes is the screen's: 800 x 600, U-1310); the
  // constructor's texture load runs before any context exists, like the original's
  raster_ = std::make_unique<gl::GlRaster>(800, 600);
  raster_->no_context();
  dll_.setup.b = raster_.get();
  // roadmap S3.15 (U-1810): SG_GL_BACKEND=wgl also feeds the setup to the host's own OpenGL (gl_wgl.h); the terrain passes then
  // draw through it (gfx_terrain.cpp). Read when the interface is made, so a run keeps one choice. Roadmap S3.20: raster / host
  // (= wgl) / auto too.
  const char *be = std::getenv("SG_GL_BACKEND");
  if (be && (std::strcmp(be, "wgl") == 0 || std::strcmp(be, "host") == 0 || std::strcmp(be, "auto") == 0 || std::strcmp(be, "raster") == 0))
    choose_renderer(be);
}

// Roadmap S3.20 (decision G13). The host's OpenGL is made like use_host_gl does (with the negative controls' sink variants), the
// self-check draws its reference through a sink of the same variant; a refused check or a host without a 16-bit generic pixel
// format leaves GlRaster. Controls (g_negctl_gfx_terrain): 13 the pinned hash wrong, 14 the reference pass perturbed (every texture
// coordinate one texel off): both make "auto" fall back to GlRaster with the check's reason.
const LiveTerrain::RendererChoice &LiveTerrain::choose_renderer(const std::string &asked) {
  const std::string mode = asked == "host" ? "wgl" : asked;   // `host` is `wgl`'s other name
  RendererChoice c;
  c.mode = mode;
  if (calls_ != 0) {
    c.reason = "asked after the first Terrain call: the setup already went to the rasteriser alone";
    if (host_gl()) c.backend = "wgl";
    choice_ = c;
    return choice_;
  }
  if (mode == "raster") {
    use_host_gl(false);
    c.reason = "GlRaster (the default renderer, decision G13)";
  } else if (mode == "wgl") {
    if (use_host_gl(true)) {
      c.backend = "wgl";
      c.reason = "the host's OpenGL asked for (no self-check)";
    } else {
      c.reason = "the host's OpenGL asked for, unavailable: " + host_gl_why();
      use_host_gl(false);
    }
  } else {   // auto
    if (!use_host_gl(true)) {
      c.reason = "no host OpenGL: " + host_gl_why();
      use_host_gl(false);
    } else {
      const int ctl = SG_NEGCTL(g_negctl_gfx_terrain);
      const int variant = ctl == 8 ? 1 : ctl == 9 ? 2 : ctl == 11 ? 3 : ctl == 12 ? 4 : 0;
      c.check = gl::wgl_self_check(ctx_, variant, ctl == 13 ? 1 : ctl == 14 ? 2 : 0);
      c.checked = true;
      if (c.check.pass) {
        c.backend = "wgl";
        c.reason = "self-check passed: " + c.check.reason;
      } else {
        c.reason = "self-check failed: " + c.check.reason;
        use_host_gl(false);
      }
    }
  }
  choice_ = c;
  return choice_;
}

bool LiveTerrain::use_host_gl(bool on) {
  // P8B: a world layer made before keeps its fan, rebuilt over the new primary sink
  struct Refan {
    LiveTerrain &t;
    ~Refan() {
      if (t.world_ && t.world_fan_ && t.dll_.setup.b != t.world_fan_.get()) {
        t.world_fan_ = std::make_unique<WorldFan>(t.dll_.setup.b, *t.world_);
        t.dll_.setup.b = t.world_fan_.get();
      }
    }
  } refan{*this};
  if (!on) {
    dll_.setup.b = raster_.get();
    pair_.reset();
    wgl_.reset();
    return true;
  }
  if (wgl_) return wgl_->ok();
  // negative controls 8 / 9 / 11 / 12 (gfx_terrain.cpp): a DIB with RGB565 masks / with its rows top-down / texture names not
  // mapped / no pixel format (the fallback: the sink is not ok, the setup stays GlRaster's alone)
  const int ctl = SG_NEGCTL(g_negctl_gfx_terrain);
  wgl_ = std::make_unique<gl::WglSink>(800, 600, ctl == 8 ? 1 : ctl == 9 ? 2 : ctl == 11 ? 3 : ctl == 12 ? 4 : 0);
  if (!wgl_->ok()) return false;
  pair_ = std::make_unique<gl::SetupPair>(raster_.get(), wgl_.get());
  dll_.setup.b = pair_.get();
  return true;
}

LiveTerrain::~LiveTerrain() = default;

void LiveTerrain::keep_snapshots() {
  while (next_snap_ < snap_after_.size() && snap_after_[next_snap_] <= calls_) {
    Snapshot s;
    s.after = static_cast<uint32_t>(calls_);
    s.object = object_;
    s.vertices = g_.vertices;
    s.normals = g_.normals;
    s.textures = g_.textures;
    s.view_index = g_.view_index;
    snaps_.push_back(std::move(s));
    ++next_snap_;
  }
}

uint32_t LiveTerrain::call(const char *name, uint32_t self, std::initializer_list<uint32_t> args, const int32_t *table, int32_t n) {
  uint32_t a[5] = {0, 0, 0, 0, 0};
  size_t k = 0;
  for (uint32_t v : args) a[k++] = v;
  const uint32_t ret = terrain_dll_call(ctx_, g_, dll_, name, self, a, table, n);
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 6) keep_log_ = false;   // control 6: the log kept off in a run that reads it
  if (keep_log_) {
    LogRec r{static_cast<uint64_t>(log_size()), name, self, {a[0], a[1], a[2], a[3], a[4]}, ret, static_cast<uint8_t>(k), false, 0,
             {}, table != nullptr};
    if (table) r.tbl.assign(table, table + (n > 0 ? n : 0));
    // control 7: the compact record loses setType's variation (the lazy line then prints no `var=`)
    if (std::strcmp(name, "setType") == 0 && a[0] && SG_NEGCTL(g_negctl_gfx_terrain) != 7) {
      r.has_var = true;
      r.var = Ptr<TerrainTile>(ctx_.mem, a[0]).field(&TerrainTile::variation).get();
    }
    recs_.push_back(std::move(r));
  }
  ++calls_;
  keep_snapshots();
  return ret;
}

const std::vector<std::string> &LiveTerrain::log() const {
  // the recorder's line: seq tid depth name ecx a0..a4 ret [tbl=...] [var=...]; the pending records move into the lines
  for (const LogRec &r : recs_) {
    std::string line = std::to_string(r.seq) + " 0 0 " + r.name + " " + hex(r.self);
    for (size_t i = 0; i < 5; ++i) line += " " + (i < r.k ? hex(r.a[i]) : std::string("-"));
    line += " " + hex(r.ret);
    if (r.has_tbl) {
      line += " tbl=";
      for (size_t i = 0; i < r.tbl.size(); ++i) line += (i ? "," : "") + std::to_string(r.tbl[i]);
    }
    if (r.has_var) line += " var=" + std::to_string(r.var);
    log_.push_back(std::move(line));
  }
  recs_.clear();
  return log_;
}

void LiveTerrain::drop_log() {
  std::vector<std::string>().swap(log_);
  std::vector<LogRec>().swap(recs_);
}

uint32_t LiveTerrain::get_instance() {
  if (!dll_.constructed) {
    object_.assign(kTerrainObjectSize, kNewFill);   // operator new's fill (debug CRT)
    uint32_t va = dll_.terrain;
    if (va == 0 || !ctx_.mem.map(va, object_.data(), kTerrainObjectSize, false)) {
      va = 0;
      uint32_t base = (ctx_.mem.data_base() + static_cast<uint32_t>(sizeof(GameState)) + (32u << 20)) & ~0xFFFFu;
      for (int tries = 0; tries < 64 && va == 0; ++tries, base += (4u << 20))
        if (ctx_.mem.map(base, object_.data(), kTerrainObjectSize, false)) va = base;
      if (va == 0) throw MissingResource{"LiveTerrain: no free range for the Terrain object"};
    }
    dll_.terrain = va;
  }
  return call("getInstance", 0, {});
}

uint32_t LiveTerrain::pass_collar_info(uint32_t self, const int32_t *table, int32_t n) {
  return call("passCollarInfo", self, {0u, static_cast<uint32_t>(n)}, table, n);
}
// negative controls 1 / 2 of the terrain body family (gfx_terrain.cpp): the rasteriser misses setZoomLevel's projection or
// loadNewCourseType's texture reload
uint32_t LiveTerrain::load_new_course_type(uint32_t self, int32_t type) {
  gl::GlSink *const b = dll_.setup.b;
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 2) dll_.setup.b = nullptr;
  const uint32_t r = call("loadNewCourseType", self, {static_cast<uint32_t>(type)});
  dll_.setup.b = b;
  return r;
}
uint32_t LiveTerrain::set_type(uint32_t self, uint32_t tile, int32_t type, int32_t variation) {
  return call("setType", self, {tile, static_cast<uint32_t>(type), static_cast<uint32_t>(variation)});
}
uint32_t LiveTerrain::lay_path(uint32_t self, uint32_t tile, int32_t flag20, int32_t flag40) {
  return call("layPath", self, {tile, static_cast<uint32_t>(flag20), static_cast<uint32_t>(flag40)});
}
uint32_t LiveTerrain::set_wall(uint32_t self, uint32_t tile, int32_t side, int32_t height, int32_t on) {
  return call("setWall", self, {tile, static_cast<uint32_t>(side), static_cast<uint32_t>(height), static_cast<uint32_t>(on)});
}
uint32_t LiveTerrain::calc_normals(uint32_t self, uint32_t tile) { return call("calcNormals", self, {tile}); }
uint32_t LiveTerrain::calc_all_normals(uint32_t self, uint32_t tile) { return call("calcAllNormals", self, {tile}); }
uint32_t LiveTerrain::elevate_corner(uint32_t self, uint32_t tile, int32_t corner) {
  return call("elevateCorner", self, {tile, static_cast<uint32_t>(corner)});
}
uint32_t LiveTerrain::lower_corner(uint32_t self, uint32_t tile, int32_t corner) {
  return call("lowerCorner", self, {tile, static_cast<uint32_t>(corner)});
}
// roadmap S3.28 (U-1952): Terrain::lowerEdgeCorner draws first (renderTileAtFiveOffsets with the tile, the pivot and the angle) and
// lowers the corner after it (Tile::lowerCorner; re/match/terrain_methods.cpp); the pass reads the tile's heights and the
// vertex arrays before the lowering, so it is handed to the backend BEFORE the state call. A NULL tile does neither. Control 15
// of the terrain body family: the pass is not handed over.
uint32_t LiveTerrain::lower_edge_corner(uint32_t self, uint32_t tile, int32_t corner, uint32_t tile2, uint32_t angle) {
  if (tile != 0 && SG_NEGCTL(g_negctl_gfx_terrain) != 15)
    to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryRenderTileAtFiveOffsets), self, tile, tile2, angle});
  return call("lowerEdgeCorner", self, {tile, static_cast<uint32_t>(corner), tile2, angle});
}
uint32_t LiveTerrain::update_path(uint32_t self, int32_t x, int32_t y, int32_t flag40) {
  return call("updatePath", self, {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(flag40)});
}
uint32_t LiveTerrain::init_system(uint32_t self, int32_t w, int32_t h, uint32_t hdc, int32_t flag) {
  return call("initSystem", self, {static_cast<uint32_t>(w), static_cast<uint32_t>(h), hdc, static_cast<uint32_t>(flag)});
}
uint32_t LiveTerrain::init_terrain(uint32_t self) { return call("initTerrain", self, {}); }
uint32_t LiveTerrain::reset_terrain(uint32_t self) { return call("resetTerrain", self, {}); }
uint32_t LiveTerrain::set_zoom_level(uint32_t self, int32_t zoom) {
  gl::GlSink *const b = dll_.setup.b;
  if (SG_NEGCTL(g_negctl_gfx_terrain) == 1) dll_.setup.b = nullptr;
  const uint32_t r = call("setZoomLevel", self, {static_cast<uint32_t>(zoom)});
  dll_.setup.b = b;
  return r;
}
// The draw passes: the state side (the view quadrant) through terrain_dll_call, then the drawing through the backend (a pass draws
// nothing into the address space, so the order of the two does not matter to the state).
uint32_t LiveTerrain::render(uint32_t self, uint32_t tile, uint32_t angle) {
  const uint32_t r = call("render", self, {tile, angle});
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryRender), self, tile, angle, 0});
  return r;
}
uint32_t LiveTerrain::local_render(uint32_t self, uint32_t tile, uint32_t centre, uint32_t angle) {
  const uint32_t r = call("localRender", self, {tile, centre, angle});
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryLocalRender), self, tile, centre, angle});
  return r;
}
uint32_t LiveTerrain::path_update_render(uint32_t self, uint32_t tile, uint32_t angle) {
  const uint32_t r = call("pathUpdateRender", self, {tile, angle});
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryPathUpdateRender), self, tile, angle, 0});
  return r;
}

// a drop of the terrain body family (gfx_terrain.cpp) when a backend is attached; false otherwise or when it was not handled
bool LiveTerrain::to_gfx(const char *name, std::initializer_list<uint32_t> args) {
  if (g_gfx_attached.load(std::memory_order_relaxed) == 0 || gfx(ctx_) == nullptr) return false;
  uint32_t a[12] = {};
  int n = 0;
  for (uint32_t v : args)
    if (n < 12) a[n++] = v;
  return gfx_drop(ctx_, name, a, n);
}

void LiveTerrain::frame_begin() {
  view_.in_frame = true;
  view_.passes_in_frame = 0;
  to_gfx("terrain.frameBegin", {view_.frames});
}
void LiveTerrain::frame_end() {
  to_gfx("terrain.frameEnd", {view_.frames});
  view_.in_frame = false;
  ++view_.frames;
}
void terrain_live_frame(Ctx &ctx, bool begin) {
  if (!has_terrain_api(ctx)) return;
  if (auto *lt = dynamic_cast<LiveTerrain *>(&terrain_api(ctx))) {
    if (begin) lt->frame_begin();
    else lt->frame_end();
  }
}

// The getters (not logged, like the recorder): Tile::getVariation's char (movsx), Tile::hasPath's and isConnected's bool
// development switch SG_LIVE_GETTERS=zero: the getters answer 0 like the headless stand-in (to find what a getter's value changes)
bool getters_zero() {
  const char *e = std::getenv("SG_LIVE_GETTERS");
  return e && std::strcmp(e, "zero") == 0;
}
uint32_t LiveTerrain::get_variation(uint32_t, uint32_t tile) {
  if (getters_zero()) return 0;
  return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(Ptr<uint8_t>(ctx_.mem, tile + 0x28).get())));
}
uint32_t LiveTerrain::has_path(uint32_t, uint32_t tile) {
  if (getters_zero()) return 0;
  return Ptr<TerrainTile>(ctx_.mem, tile).field(&TerrainTile::has_path).get();
}
uint32_t LiveTerrain::has_connected_path(uint32_t self, int32_t x, int32_t y) {
  if (getters_zero()) return 0;
  return Ptr<TerrainTile>(ctx_.mem, terrain_tile_at(ctx_, self, x, y)).field(&TerrainTile::connected).get();
}

// engine services: a headless run's
uint32_t LiveTerrain::os_major_version() { return engine_->os_major_version(); }
uint32_t LiveTerrain::acquire_dc() {
  const uint32_t dc = engine_->acquire_dc();
  return dc != 0 ? dc : kDcToken;
}
uint32_t LiveTerrain::pump_input() { return engine_->pump_input(); }
// Window::paint(root window, 0x80000000): the root surface cleared (gfx_terrain.cpp); the engine's return value is the result
uint32_t LiveTerrain::window_paint() {
  const uint32_t r = engine_->window_paint();
  to_gfx("terrain.windowPaint", {});
  return r;
}
// The whole-surface copies (Surface_blitThunk): the root surface to the back surface ending a render, and the back surface to the
// root surface when the camera has not moved. terrain.blit's arguments: direction (1 = root to back), source x, y, destination
// x, y, width, height, and 1 when the strip's screen size was not one renderTerrainStrip knows.
uint32_t LiveTerrain::blit_full() {
  const uint32_t r = engine_->blit_full();
  const int32_t w = ctx_.gs().g_terrain820f30[0], h = ctx_.gs().g_terrain820f30[1];
  to_gfx("terrain.blit", {1, 0, 0, 0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 0});
  return r;
}
uint32_t LiveTerrain::blit_screen(int32_t w, int32_t h) {
  const uint32_t r = engine_->blit_screen(w, h);
  to_gfx("terrain.blit", {0, 0, 0, 0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 0});
  return r;
}

// renderTerrainStrip(surf, x, y, dx, dy, rot, zoom) (re/analysis/terrain/0044a6e0_renderTerrainStrip.md, the C3 note of the exe
// function): Terrain::stripRender(tileAt(x, y), D, angle(rot)) draws the strip the step uncovered, then Surface_blitThunk copies the
// back surface's rectangle at (sx, sy) of size (cw, ch) onto the root surface at (tx, ty): the old picture shifted by the step.
//   The screen size g_terrain820f30 picks four widths A, L10, L14, L18 (800 x 600: 0x20, 0x14, 0x28, 0x40; 1024 x 768: 0x28,
//   0x1a, 0x34, 0x50; 1280 x 1024: 0x34, 0x22, 0x44, 0x68; any other size leaves A = rot and the other three uninitialised:
//   reported by the blit body); the shift s = 2 / 1 / 0 for zoom 1 / 2 / 4, else rot (a `sar` count: its low five bits);
//   r2 = 6 for rot 2, 2 for rot 6, else rot. The step gives D: (-2,0), (-1,0) 7; (-1,-1) 8; (-1,1) 6; (0,-2), (0,-1) 1;
//   (0,1), (0,2) 5; (1,-1) 2; (1,0), (2,0) 3; (1,1) 4; any other step leaves D = zoom. e = D + r2, folded to e % 9 + 1 above 8,
//   selects the rectangle; m is |dx| or |dy| as each case says (which one depends on rot being 0 or 4); any other e passes rot as
//   the four position values and (dy, dx) as the size.
// The stripRender call is not logged (the recorder's comparison keeps the TerrainApi methods only); its state effect, the view
// quadrant, is applied. The return value is the engine's (the original's is the blit's, which updateTerrainRender drops).
uint32_t LiveTerrain::render_strip(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t rot, int32_t zoom) {
  const uint32_t r = engine_->render_strip(x, y, dx, dy, rot, zoom);
  const GameState &gs = ctx_.gs();
  const int32_t w = gs.g_terrain820f30[0], h = gs.g_terrain820f30[1];
  int32_t A = rot, L10 = 0, L14 = 0, L18 = 0;
  bool known = true;
  if (w == 800 && h == 600) {
    A = 0x20; L10 = 0x14; L14 = 0x28; L18 = 0x40;
  } else if (w == 1024 && h == 768) {
    A = 0x28; L10 = 0x1a; L14 = 0x34; L18 = 0x50;
  } else if (w == 1280 && h == 1024) {
    A = 0x34; L10 = 0x22; L14 = 0x44; L18 = 0x68;
  } else {
    known = false;
  }
  const int32_t s = (zoom == 1 ? 2 : zoom == 2 ? 1 : zoom == 4 ? 0 : rot) & 31;
  const int32_t r2 = rot == 2 ? 6 : rot == 6 ? 2 : rot;
  int32_t D = zoom;
  if (dx == -2) {
    if (dy == 0) D = 7;
  } else if (dx == -1) {
    if (dy == -1) D = 8;
    else if (dy == 0) D = 7;
    else if (dy == 1) D = 6;
  } else if (dx == 0) {
    if (dy == -2 || dy == -1) D = 1;
    else if (dy == 1 || dy == 2) D = 5;
  } else if (dx == 1) {
    if (dy == -1) D = 2;
    else if (dy == 0) D = 3;
    else if (dy == 1) D = 4;
  } else if (dx == 2) {
    if (dy == 0) D = 3;
  }
  int32_t e = add32(D, r2);
  if (e > 8) e = add32(mod32(e, 9), 1);
  const bool side = rot != 0 && rot != 4;
  const int32_t ax = abs32(dx), ay = abs32(dy);
  const int32_t As = A >> s, L10s = L10 >> s;
  int32_t sx = 0, sy = 0, tx = 0, ty = 0, cw = dy, ch = dx;
  switch (e) {
    case 1: {
      const int32_t m = side ? ax : ay;
      tx = mul32(As, m); ty = mul32(L10s, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 2: ty = L14 >> s; cw = w; ch = sub32(h, ty); break;
    case 3: {
      const int32_t m = side ? ay : ax;
      sx = mul32(As, m); ty = mul32(L10s, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 4: sx = L18 >> s; cw = sub32(w, sx); ch = h; break;
    case 5: {
      const int32_t m = side ? ax : ay;
      sx = mul32(As, m); sy = mul32(L10s, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 6: sy = L14 >> s; cw = w; ch = sub32(h, sy); break;
    case 7: {
      const int32_t m = side ? ay : ax;
      sy = mul32(L10s, m); tx = mul32(As, m); cw = sub32(w, As); ch = sub32(h, L10s);
      break;
    }
    case 8: tx = L18 >> s; cw = sub32(w, tx); ch = h; break;
    default: sx = sy = tx = ty = rot; break;
  }
  const uint32_t self = gs.gTerrain;
  const uint32_t pivot = terrain_tile_at_exe(x, y);
  const uint32_t angle = rot == 2 ? 0x42B40000u : rot == 4 ? 0x43340000u : rot == 6 ? 0xC2B40000u : 0u;   // 90, 180, -90 or 0
  const uint32_t sa[5] = {pivot, static_cast<uint32_t>(D), angle, 0, 0};
  terrain_dll_call(ctx_, g_, dll_, "stripRender", self, sa, nullptr, 0);
  to_gfx("terrain.pass", {static_cast<uint32_t>(kEntryStripRender), self, pivot, static_cast<uint32_t>(D), angle});
  to_gfx("terrain.blit", {0, static_cast<uint32_t>(sx), static_cast<uint32_t>(sy), static_cast<uint32_t>(tx),
                          static_cast<uint32_t>(ty), static_cast<uint32_t>(cw), static_cast<uint32_t>(ch), known ? 0u : 1u});
  return r;
}

// the exe's tileAt over gTerrain (renderTerrainStrip's inline copy): 0 outside the object's width / height
uint32_t LiveTerrain::terrain_tile_at_exe(int32_t x, int32_t y) {
  const uint32_t t = ctx_.gs().gTerrain;
  const auto hp = Ptr<TerrainObjectHeader>(ctx_.mem, t);
  const int32_t w = hp.field(&TerrainObjectHeader::width).get();
  if (x >= w || x < 0 || y >= hp.field(&TerrainObjectHeader::height).get() || y < 0) return 0;
  return t + kTerrainTilesOffset + static_cast<uint32_t>(add32(x, mul32(y, w))) * static_cast<uint32_t>(sizeof(TerrainTile));
}
uint32_t LiveTerrain::operator_new(uint32_t size) { return engine_->operator_new(size); }

// Harness entries (roadmap S3.2), by sg_call: terrainLive.keepFrame(k) asks the terrain body family to copy the root surface around
// updateTerrainRender's k-th call (from 0) into `$gfx/terrain_<k>_*.555`; terrainLive.passes / terrainLive.pixels return the passes
// rasterised and the pixels they wrote (low dword); terrainLive.frames the updateTerrainRender calls seen. A context whose TerrainApi
// is not a LiveTerrain throws MissingResource. terrainLive.hostGl(on) selects the host's own OpenGL (below).
namespace {
LiveTerrain &need_live(Ctx &c) {
  LiveTerrain *lt = has_terrain_api(c) ? dynamic_cast<LiveTerrain *>(&terrain_api(c)) : nullptr;
  if (!lt) throw MissingResource{"terrainLive: no LiveTerrain attached"};
  return *lt;
}
void a_keep_frame(Ctx &c, const uint32_t *a, Ret &r) {
  need_live(c).view().keep_frames.insert(a[0]);
  r.set_u32(0);
}
void a_passes(Ctx &c, const uint32_t *, Ret &r) { r.set_u32(static_cast<uint32_t>(need_live(c).view().passes)); }
void a_pixels(Ctx &c, const uint32_t *, Ret &r) { r.set_u32(static_cast<uint32_t>(need_live(c).view().pixels_drawn)); }
void a_frames(Ctx &c, const uint32_t *, Ret &r) { r.set_u32(need_live(c).view().frames); }
// terrainLive.hostGl(on) (roadmap S3.15): the host's own OpenGL beside the rasteriser (gl_wgl.h), chosen before the LiveTerrain's
// first call (the GL setup must reach it from the start); returns 1 when it is in use, 0 when the host has none or it came too late
void a_host_gl(Ctx &c, const uint32_t *a, Ret &r) {
  LiveTerrain &lt = need_live(c);
  r.set_u32(lt.calls() == 0 && lt.use_host_gl(a[0] != 0) && a[0] != 0 ? 1u : 0u);
}
// terrainLive.renderer(mode) (roadmap S3.20): 0 = GlRaster, 1 = the host's OpenGL without the check, 2 = auto (the self-check
// decides); returns 1 when the host's OpenGL draws, and writes the choice to `$wgl/renderer.txt` (mode, backend, reason, check,
// got, want, ms). Before the LiveTerrain's first call.
void a_renderer(Ctx &c, const uint32_t *a, Ret &r) {
  LiveTerrain &lt = need_live(c);
  const LiveTerrain::RendererChoice &ch = lt.choose_renderer(a[0] == 0 ? "raster" : a[0] == 1 ? "wgl" : "auto");
  char ms[32];
  std::snprintf(ms, sizeof ms, "%.1f", ch.check.ms);
  const std::string t = "mode " + ch.mode + "\nbackend " + ch.backend + "\nreason " + ch.reason + "\ncheck " +
                        (ch.checked ? (ch.check.pass ? "pass" : "fail") : "none") + "\ngot " + ch.check.got + "\nwant " +
                        ch.check.want + "\nms " + ms + "\n";
  GameFiles &f = c.files();
  auto *d = dynamic_cast<DirGameFiles *>(&f);
  if (!d || !d->write_through()) f.write("$wgl/renderer.txt", std::vector<uint8_t>(t.begin(), t.end()));
  r.set_u32(ch.backend == "wgl" ? 1u : 0u);
}
// P8B: terrainLive.worldView(pw, ph) makes the world layer (LiveTerrain::set_world_view; 1 = on, 0 = refused, the reason in
// `$gfx/world_why.txt`); terrainLive.worldCheck(on) switches the render-scale-1 gate on (terrain_live.h); terrainLive.worldStats(i)
// returns 0 passes, 1 pixels, 2 microseconds, 3 checks, 4 checks with a difference, 5 differing pixels (low dword), 6 pw, 7 ph, 8 ox;
// terrainLive.worldDump() writes the world root `$gfx/world.555` (pw * ph words) and `$gfx/world_t800.555` (T), returns 1, 0 without
// a world layer
void put(Ctx &c, const std::string &name, const std::vector<uint8_t> &bytes) {   // a harness file (never the install), as a_renderer
  GameFiles &f = c.files();
  auto *d = dynamic_cast<DirGameFiles *>(&f);
  if (!d || !d->write_through()) f.write(name, bytes);
}
void a_world_view(Ctx &c, const uint32_t *a, Ret &r) {
  std::string why;
  const bool ok = need_live(c).set_world_view(static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), &why);
  put(c, "$gfx/world_why.txt", std::vector<uint8_t>(why.begin(), why.end()));
  r.set_u32(ok ? 1u : 0u);
}
void a_world_check(Ctx &c, const uint32_t *a, Ret &r) {
  LiveTerrainWorld *w = need_live(c).world();
  if (w) w->check_central = a[0] != 0;
  r.set_u32(w ? 1u : 0u);
}
void a_world_stats(Ctx &c, const uint32_t *a, Ret &r) {
  LiveTerrainWorld *w = need_live(c).world();
  if (!w) throw MissingResource{"terrainLive.worldStats: no world layer"};
  const int64_t v[] = {w->passes, w->pixels, w->us, w->checks, w->checks_differing, w->diff_px, w->pw, w->ph, w->ox};
  r.set_u32(a[0] < 9 ? static_cast<uint32_t>(v[a[0]]) : 0u);
}
void a_world_dump(Ctx &c, const uint32_t *, Ret &r) {
  LiveTerrainWorld *w = need_live(c).world();
  if (!w) {
    r.set_u32(0);
    return;
  }
  auto bytes = [](const std::vector<uint16_t> &px) {
    std::vector<uint8_t> b(px.size() * 2);
    std::memcpy(b.data(), px.data(), b.size());
    return b;
  };
  put(c, "$gfx/world.555", bytes(w->raster->pixels()));
  put(c, "$gfx/world_t800.555", bytes(w->terrain800));
  r.set_u32(1);
}
const FuncDef kFuncs[] = {
    {"terrainLive.worldView", "default", "int", 2, 0, a_world_view, nullptr},
    {"terrainLive.worldCheck", "default", "int", 1, 0, a_world_check, nullptr},
    {"terrainLive.worldStats", "default", "int", 1, 0, a_world_stats, nullptr},
    {"terrainLive.worldDump", "default", "int", 0, 0, a_world_dump, nullptr},
    {"terrainLive.keepFrame", "default", "int", 1, 0, a_keep_frame, nullptr},
    {"terrainLive.passes", "default", "int", 0, 0, a_passes, nullptr},
    {"terrainLive.pixels", "default", "int", 0, 0, a_pixels, nullptr},
    {"terrainLive.frames", "default", "int", 0, 0, a_frames, nullptr},
    {"terrainLive.hostGl", "default", "int", 1, 0, a_host_gl, nullptr},
    {"terrainLive.renderer", "default", "int", 1, 0, a_renderer, nullptr},
};
}  // namespace

extern const ModuleDef kModule_terrain_live = {"terrain_live", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
