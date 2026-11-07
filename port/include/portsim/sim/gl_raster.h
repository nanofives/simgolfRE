// The port's software rasteriser for the terrain renderer's GL calls (phase 5 step 3; plan re/PHASE5_TERRAIN_PLAN.md section 3).
//
// The original draws with Windows' generic OpenGL 1.1 implementation into a 16-bit RGB555 DIB. GlRaster is the same pipeline in
// portable C++: matrix stacks, the vertex transform, face culling, triangle scan conversion, texture sampling, blending and
// the RGB555 write. It is a GlSink (the calls the render path makes) plus the state calls the original's setup makes (viewport,
// projection, texture objects and parameters, lights, arrays), so a recorded stream can be replayed into it (exec_line) and the
// frame compared with the one the original produced (port/test/test_gl_raster.py).
//
// What is verified against the original and what is not is written in re/PHASE5_TERRAIN_PLAN.md; the sampling and rounding
// rules of Windows' implementation are not documented, so each one is a hypothesis the corpus judges.
#ifndef PORTSIM_SIM_GL_RASTER_H
#define PORTSIM_SIM_GL_RASTER_H

#include <cstdint>
#include <functional>  // P15A fix 3: GlRecord's chunk source
#include <map>
#include <memory>
#include <string>
#include <set>
#include <vector>

#include "portsim/sim/gl_sink.h"

namespace portsim {
namespace gl {

class GlRaster : public GlSink {
 public:
  // width x height pixels, 16 bits each (RGB555), row 0 = window y 0
  GlRaster(int width, int height);
  ~GlRaster() override;
  GlRaster(const GlRaster &) = delete;
  GlRaster &operator=(const GlRaster &) = delete;

  // Pixel-loop workers of the default pipeline (roadmap S3.10): n > 1 starts n threads that rasterise queued triangles (each owns
  // every n-th band of 4 rows, so every pixel gets the same writes in the same order as with none); 1 (the default) stops them and
  // rasterises each triangle at once. finish() waits until every queued triangle is written and adds their pixel counters to
  // stats; pixels(), glFlush, texture uploads and the general loop call it, a caller reading stats calls it first. Stream line
  // `#threads <n>`.
  void set_threads(int n);
  int threads() const;
  // P15A fix 2: workers started by later set_threads calls run at normal priority instead of the feeder's (the world layer's
  // rasteriser: its many workers leave the window's paint threads their share; the pixels do not depend on it)
  void set_workers_normal(bool on) { workers_normal_ = on; }
  void finish();
  // P15A fix 3 (log/roadmap/P15/P15A_notes.md section 5): THE MASK. With rows non-empty only the pixels of the rows y with
  // rows[y] != 0, and in them the columns x < cx0 or x >= cx1, are written; every other pixel keeps what it held. Each pixel's value
  // depends only on the writes to that pixel (both pixel loops compute a pixel from its own coverage, attributes and destination;
  // the textured walk's row starts are computed per row by tex_walk_ and its sums along a span are carried over the skipped
  // columns), so the masked pixels are the unmasked rasteriser's. A triangle whose box meets no masked pixel is dropped before its
  // set-up (stats.mask_skipped). Empty rows: no mask (the default).
  void set_mask(std::vector<uint8_t> rows, int cx0, int cx1);
  bool masked() const { return !mask_rows_.empty(); }
  // the mask's words of the framebuffer set to v (every word without a mask)
  void fill_masked(uint16_t v);
  // the mask's words of src copied into dst (resized to src's size first, new words 0); without a mask dst = src
  void copy_masked(const std::vector<uint16_t> &src, std::vector<uint16_t> &dst) const;
  // P15A fix 3: DISCARD. Primitives are dropped (no vertex is transformed, no pixel written) while every state call (matrices,
  // textures, lights, enables, viewport) still applies: a recorded pass whose pixels a later operation overwrites is replayed so
  void set_discard(bool on) { discard_ = on; }

  int width() const { return w_; }
  int height() const { return h_; }
  // P8B (widescreen stage 2, log/roadmap/P8/P8B_notes.md): the x clip planes at |x| <= kx * w instead of |x| <= w, so a buffer
  // wider than the viewport is drawn beyond the viewport's sides with the same clip coordinates (the world layer of
  // LiveTerrain). 1 (the default) keeps the original's planes and the code path unchanged.
  void set_clip_x(float kx) { finish(); clip_kx_ = kx; }
  float clip_x() const { return clip_kx_; }
  // P8B: the 4x4 dither matrix's column index taken from (x + dx) & 3 instead of x & 3 (a world layer whose 800 x 600 frame starts
  // at column ox sets dx = ox & 3 at render scale 1, so its central columns keep the frame's dither phase); 0 = the original's
  void set_dither_dx(int dx) { finish(); dither_dx_ = dx & 3; }
  // P8B (G32: the world layer follows window resizes): a new framebuffer of w x h words (0), textures, matrices, lights and every
  // other state kept; the caller sets the viewport again
  void resize_frame(int w, int h) {
    finish();
    w_ = w;
    h_ = h;
    fb_.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  }
  std::vector<uint16_t> &pixels() { finish(); return fb_; }
  // diagnostics: the colour of every written pixel before it is quantised to 5 bits (0..255 per channel, 3 floats a pixel)
  void keep_float(bool on) {
    finish();
    keep_float_ = on;
    if (on) { ffb_.assign(fb_.size() * 3, -1.0f); sfb_.assign(fb_.size() * 8, -1.0f); rfb_.assign(fb_.size(), -1.0f); tid_.assign(fb_.size(), -1); tris_.clear(); }
  }
  // per written pixel: source r g b (0..255, after lighting), source alpha (0..255), the destination's r g b 5-bit values and
  // 1 when the write was blended
  const std::vector<float> &source_pixels() const { return sfb_; }
  const std::vector<float> &float_pixels() const { return ffb_; }
  // per written pixel: the texel-per-pixel scale (rho) of its triangle, the last write's (-1 where nothing was written)
  const std::vector<float> &rho_pixels() const { return rfb_; }
  // per written pixel: the index of the triangle that wrote it last (-1 none); per drawn triangle 12 doubles: the window x, y of its
  // three vertices in the counter-clockwise order the rasteriser uses, then their s, t
  const std::vector<int32_t> &triangle_ids() const { return tid_; }
  const std::vector<double> &triangles() const { return tris_; }
  const std::vector<uint16_t> &pixels() const { const_cast<GlRaster *>(this)->finish(); return fb_; }

  // ---- the GlSink calls of the render path ----
  void load_identity() override;
  void push_matrix() override;
  void pop_matrix() override;
  void rotated(double angle, double x, double y, double z) override;
  void rotatef(float angle, float x, float y, float z) override;
  void translatef(float x, float y, float z) override;
  void bind_texture(uint32_t target, uint32_t id) override;
  void begin(uint32_t mode) override;
  void end() override;
  void tex_coord2fv(const float *v) override;
  void tex_coord2f(float s, float t) override;
  void array_element(int32_t i) override;
  void normal3fv(const float *v) override;
  void vertex3fv(const float *v) override;
  void vertex3f(float x, float y, float z) override;
  void enable(uint32_t cap) override;
  void disable(uint32_t cap) override;
  void blend_func(uint32_t src, uint32_t dst) override;
  void flush() override;

  // ---- the state calls of the original's setup (the GlSink overrides) ----
  // A driver ignores every call made while no context exists: no_context() puts the rasteriser in that state (the Terrain
  // constructor loads its textures that way), context_create() ends it. A rasteriser used for replays starts with a context.
  void no_context() { ctx_ = false; }
  void context_create() override { ctx_ = true; }
  void viewport(int32_t x, int32_t y, int32_t w, int32_t h) override;
  void matrix_mode(uint32_t mode) override;
  void ortho(double l, double r, double b, double t, double n, double f) override;
  void front_face(uint32_t mode) override;
  void light(uint32_t light, uint32_t pname, const float *v) override;     // GL_LIGHT0; pname AMBIENT / DIFFUSE / SPECULAR / POSITION
  void material(uint32_t face, uint32_t pname, const float *v) override;   // GL_SPECULAR only (the render path sets nothing else)
  void material_shininess(uint32_t face, int32_t s) override;
  bool gen_textures(int32_t n, uint32_t *names) override;                 // sequential names from 1, like the original's driver
  void delete_textures(int32_t n, const uint32_t *names) override;
  void tex_parameteri(uint32_t target, uint32_t pname, int32_t v) override;
  void tex_parameterf(uint32_t target, uint32_t pname, float v) override;
  void tex_envi(uint32_t target, uint32_t pname, int32_t v) override;      // only target GL_TEXTURE_ENV (0x2300) takes effect
  void tex_image2d(uint32_t target, int32_t level, int32_t internal, int32_t w, int32_t h, int32_t border, uint32_t format, uint32_t type,
                   const uint8_t *data) override;                           // GL_RGB / GL_RGBA, unsigned byte, level 0
  void normal3f(float x, float y, float z);
  void color4f(float r, float g, float b, float a);   // the current colour (used when lighting is off)
  void set_arrays(const float *vertices, size_t n_vertices, const float *normals, size_t n_normals);

  // ---- recorded streams ----
  // texture bytes by the recorder's FNV hash text ("0x..."): glTexImage2D lines name them with tex=
  void add_texture_bytes(const std::string &hash, std::vector<uint8_t> bytes) { tex_bytes_[hash] = std::move(bytes); }
  // one line of a recorded stream (stream form with pointers dropped is NOT accepted: the recorder's own lines, with p= / v= /
  // ids= / tex= tokens); false when the call is not one the rasteriser knows
  bool exec_line(const std::string &line);

  // experiment knobs for fitting the original's arithmetic (0 = the exact double-precision pipeline); set with the stream
  // line `#tune <index> <value>`: 0 bilinear weight bits, 1 weight rounding (0 floor, 1 round), 2 texture coordinate
  // fraction bits, 3 interpolated light colour bits, 4 vertex colour bits, 5 float32 transform (1) or double (0)
  // 8 filtered texel rounding (1 round, 2 floor, 3 ceil to an integer 0..255), 9 two-stage lerp (1 round / 2 floor per stage)
  // 11 negative controls (deliberately wrong pipelines): 1 no dither, 2 dither offset 0, 3 lighting colour white, 4 min filter
  // nearest, 5 texture env REPLACE, 6 blending ignored, 7 front faces culled, 8 no specular
  // 16 attribute-plane vertex rounding f32(v + 2^k) - 2^k, 17 attribute arithmetic in float32, 18 / 19 an absolute shift of the
  // attribute sample position in x / y, in units of 1e-5 pixel, 20 per-vertex attribute offsets sign(ndc) * value / 256 px where |ndc| >= 0.5,
  // 22 attribute setup from 8-fractional-bit window coordinates of a viewport scaled by half-size + 2^-8 (U-1076 experiments),
  // 27 = 1 the exact pow for the specular term instead of the generic GL's 256-entry nearest-entry table (U-1300),
  // 28 = 1 the earlier double-precision vertex model (window position scaled by 1 - 2^-8 / h and rounded) instead of the generic GL's
  // float32 vertex arithmetic (roadmap S3.21, U-1392), 29 = 1 no frustum clipping (with tune 28 = 0 the triangles crossing the view
  // volume are clipped like the generic GL's and drawn as the fan of the clipped polygon),
  // 32 = 1 the TEXTURED WALK (roadmap S3.29, U-1309): a textured triangle whose texture has a GL_LINEAR filter takes the generic GL's
  // float32 interpolation (float32 gradients, rows walked from the largest y, float32 sums along each span; GlRaster::TexWalk) for its
  // texture coordinates and colour instead of the exact plane, and an unblended GL_MODULATE pixel of it the float32 filter / modulate /
  // dither (walk_final in gl_raster.cpp); 33 (with 32) the gradient anchor (0 the lowest vertex, 1 the highest,
  // 2 the first in stream order), 34 = 1 (with 32) no restart of the row starts where the left edge turns at the middle vertex,
  // 35 = 1 (with 32) the double-precision filter / modulate / dither of the other pixels instead of the walk's float32 final stage
  static constexpr int kTunes = 40;
  int tune[kTunes] = {};

  // The pixel loop of the default pipeline (every tune knob 0 except tune 11, keep_float off; roadmap S3.10): 1 (default) the fast
  // loop with the checked reciprocal path, 2 the fast loop with the general loop's exact expressions only, 0 the general loop.
  // Negative controls: 3 the reciprocal path without its boundary checks, 4 with reciprocals rounded to float (errors larger than
  // the checks allow). Set with the stream line `#fastpath <mode>`.
  int fast_mode = 1;
  // Negative control of the workers (roadmap S3.12): 1 = the last of several workers flips bit 0 of the first pixel it writes in
  // each row of a triangle (no effect with one thread, so it reads only the worker path). Stream line `#workerctl <mode>`.
  int worker_ctl = 0;

  // counters of the last frame (what the rasteriser saw), for the tests' diagnostics
  struct Stats {
    int64_t triangles = 0, culled = 0, drawn_px = 0, blended_px = 0, bad_texture = 0;
    int64_t fast_px = 0;   // pixels the checked reciprocal path wrote (the rest of drawn_px took the exact expressions)
    int64_t mask_skipped = 0;   // P15A fix 3: triangles whose box met no pixel of the mask
  } stats;

 private:
  struct Tex {
    uint32_t w = 0, h = 0, fmt = 0;
    std::vector<uint8_t> data;                   // RGB or RGBA
    uint32_t mag = 0x2601, min = 0x2702, wrap_s = 0x2901, wrap_t = 0x2901;   // GL defaults
  };
  struct Vtx {
    double x, y, z;       // window coordinates
    double s, t;
    double r, g, b, a;    // the lit colour (0..1)
    double qx, qy;        // the vertex snapped to 2^-8 by the float32 vertex rule (tune 28 = 0)
    float c[4];           // its float32 clip coordinates (tune 28 = 0)
  };
  struct Mat {
    double m[16];         // column-major
  };

  void tex_param_(uint32_t pname, uint32_t value);
  void tex_env_(uint32_t target, uint32_t pname, uint32_t value);
  bool ctx_ = true;
  std::set<uint32_t> used_names_;   // texture names in use (the lowest free one is handed out)

  int w_, h_;
  std::vector<uint16_t> fb_;
  std::vector<float> ffb_, sfb_;
  std::vector<float> rfb_;   // rho per pixel (keep_float)
  std::vector<int32_t> tid_; // triangle index per pixel (keep_float)
  std::vector<double> tris_; // 12 per triangle (keep_float)
  double sx_[3] = {}, sy_[3] = {};   // tune 22: the triangle's fixed-point setup vertices
  bool keep_float_ = false;
  // matrices
  uint32_t mode_ = 0x1700;               // GL_MODELVIEW
  std::vector<Mat> stack_[2];
  Mat mv_, proj_;
  int32_t vp_x_ = 0, vp_y_ = 0, vp_w_ = 0, vp_h_ = 0;
  float clip_kx_ = 1.0f;   // P8B: set_clip_x
  int dither_dx_ = 0;      // P8B: set_dither_dx
  bool workers_normal_ = false;  // P15A fix 2: set_workers_normal
  // P15A fix 3: set_mask (mask_pre_[y] = masked rows above y), set_discard
  std::vector<uint8_t> mask_rows_;
  std::vector<int32_t> mask_pre_;
  int mask_cx0_ = 0, mask_cx1_ = 0;
  bool discard_ = false;
  // state
  bool texture2d_ = false, cull_ = false, blend_ = false, lighting_ = false, light0_ = false;
  uint32_t front_ = 0x0901;              // GL_CCW
  uint32_t blend_src_ = 1, blend_dst_ = 0;
  uint32_t env_mode_ = 0x2100;           // GL_MODULATE
  std::map<uint32_t, Tex> textures_;
  uint32_t bound_ = 0;
  std::vector<float> varr_, narr_;
  // current vertex attributes and the primitive being assembled
  float cur_s_ = 0, cur_t_ = 0;
  float cur_n_[3] = {0, 0, 1};
  float cur_c_[4] = {1, 1, 1, 1};
  // lighting: light 0 (ambient, diffuse, specular, eye-space position), material specular and shininess; the rest are GL defaults
  float light_[4][4] = {{0, 0, 0, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {0, 0, 1, 0}};
  float mat_spec_[4] = {0, 0, 0, 1};
  float shininess_ = 0;
  uint32_t prim_ = 0;
  bool in_begin_ = false;
  std::vector<Vtx> verts_;
  std::map<std::string, std::vector<uint8_t>> tex_bytes_;

  Mat &cur() { return mode_ == 0x1701 ? proj_ : mv_; }
  // the generic GL's own matrices (roadmap S3.21): float32, column-major; the MVP is updated by every modelview operation itself
  // (not recomputed as P * MV) and recomputed only when the projection changes; push / pop of the modelview saves it too
  struct FMat {
    float m[16];
  };
  FMat fmv_, fproj_, fmvp_;
  std::vector<FMat> fstack_[2], fstack_mvp_;
  void fpost_(const FMat &b);       // the current float matrix times b, and the MVP with it
  static void fmul_(const FMat &a, const FMat &b, FMat &r);
  void mul(const Mat &b);
  void emit(double x, double y, double z, const float *normal);
  // emit's per-vertex invariants (roadmap S3.10): the inverse of the modelview's upper 3x3 for its nine values, the specular table
  bool inv_ok_ = false;
  double inv_key_[9] = {}, inv_[9] = {};
  double spec_lookup_(double nh, double shininess);
  double spec_sh_ = -1.0, spec_t_ = 0.0, spec_scale_ = 0.0, spec_tab_[256] = {};
  void triangle(const Vtx &a, const Vtx &b, const Vtx &c);
  // the window position of float32 clip coordinates under the vertex rule (x, y unrounded; qx, qy snapped to 2^-8)
  void window_of_(const float c[4], Vtx &v) const;
  // frustum clipping (tune 29 = 0): true when the triangle was handled (culled, clipped and drawn as a fan, or clipped away)
  bool clip_triangle_(const Vtx &a, const Vtx &b, const Vtx &c);
  bool in_clip_ = false;
  // the default pipeline's pixel loop (roadmap S3.10): the same values as triangle()'s general loop, computed with exact integer
  // edge functions and row spans; false (nothing written) when its preconditions do not hold
  // the textured walk (tune 32, roadmap S3.29): per row of the triangle's box the first covered pixel and the float32 row-start values
  // of u, v (texel units) and r, g, b (times 31), and the float32 x gradients a span adds per pixel
  struct TexWalk {
    int miny = 0;
    std::vector<int32_t> ix;
    std::vector<float> start;   // 5 per row
    float gx[5] = {};
  };
  std::shared_ptr<const TexWalk> tex_walk_(const Vtx *const abc[3], const Tex &tex, int minx, int maxx, int miny, int maxy,
                                           const Vtx *const cov[3], const bool inc[3]) const;
  bool fast_triangle_(const Vtx *v0, const Vtx *v1, const Vtx *v2, double ar, const Tex &tex, bool linear, int minx, int maxx,
                      int miny, int maxy, const bool inc[3], std::shared_ptr<const TexWalk> walk = nullptr);
  struct FastJob;   // one queued triangle (gl_raster.cpp)
  struct Pool;
  std::unique_ptr<Pool> pool_;
  void queue_(const FastJob &j);
  static void fast_rows_(const FastJob &j, int part, int parts, int64_t *counters);
};

// P15A fix 3 (log/roadmap/P15/P15A_notes.md section 5): a RECORDED GL STREAM. GlRecord writes every GlSink call (and set_arrays) as
// words, with copies of the texture images and the vertex / normal arrays; gl_replay makes the same calls, in the same order and
// with the same bits, on a GlRaster. The world layer's rasteriser on the world thread is fed this way (terrain_live.h
// LiveTerrainWorld::recorder).
struct GlRec {
  std::vector<uint32_t> w;                                         // opcode, then its argument words
  std::vector<std::shared_ptr<const std::vector<uint8_t>>> blobs;  // texture images (tex_image2d)
  std::vector<std::shared_ptr<const std::vector<float>>> arrays;   // set_arrays: the vertices and normals
  bool draws = false;                                              // a primitive was begun (the replay writes pixels)
};
class GlRecord : public GlSink {
 public:
  // chunk(): the stream the next call is appended to (asked at every call: the caller may start a new one between calls)
  explicit GlRecord(std::function<GlRec *()> chunk) : chunk_(std::move(chunk)) {}
  void load_identity() override;
  void push_matrix() override;
  void pop_matrix() override;
  void rotated(double angle, double x, double y, double z) override;
  void rotatef(float angle, float x, float y, float z) override;
  void translatef(float x, float y, float z) override;
  void bind_texture(uint32_t target, uint32_t id) override;
  void begin(uint32_t mode) override;
  void end() override;
  void tex_coord2fv(const float *v) override;
  void tex_coord2f(float s, float t) override;
  void array_element(int32_t i) override;
  void normal3fv(const float *v) override;
  void vertex3fv(const float *v) override;
  void vertex3f(float x, float y, float z) override;
  void enable(uint32_t cap) override;
  void disable(uint32_t cap) override;
  void blend_func(uint32_t src, uint32_t dst) override;
  void flush() override;
  void context_create() override;
  void hint(uint32_t target, uint32_t mode) override;
  void enable_client_state(uint32_t array) override;
  void front_face(uint32_t mode) override;
  void viewport(int32_t x, int32_t y, int32_t w, int32_t h) override;
  void matrix_mode(uint32_t mode) override;
  void ortho(double l, double r, double b, double t, double n, double f) override;
  void light(uint32_t light, uint32_t pname, const float *v4) override;
  void material(uint32_t face, uint32_t pname, const float *v4) override;
  void material_shininess(uint32_t face, int32_t s) override;
  bool gen_textures(int32_t n, uint32_t *names) override;   // recorded; writes no name (the caller's sink hands them out)
  void delete_textures(int32_t n, const uint32_t *names) override;
  void tex_parameteri(uint32_t target, uint32_t pname, int32_t v) override;
  void tex_parameterf(uint32_t target, uint32_t pname, float v) override;
  void tex_envi(uint32_t target, uint32_t pname, int32_t v) override;
  void tex_image2d(uint32_t target, int32_t level, int32_t internal, int32_t w, int32_t h, int32_t border, uint32_t format, uint32_t type,
                   const uint8_t *data) override;
  void vertex_pointer(int32_t size, uint32_t type, int32_t stride) override;
  void normal_pointer(uint32_t type, int32_t stride) override;
  // GlRaster::set_arrays: the arrays copied when they differ from the last recorded ones (else those are named again)
  void set_arrays(const float *vertices, size_t n_vertices, const float *normals, size_t n_normals);
  int64_t calls = 0, array_copies = 0;

 private:
  std::function<GlRec *()> chunk_;
  std::shared_ptr<const std::vector<float>> last_v_, last_n_;
  GlRec &c() { ++calls; return *chunk_(); }
};
// the calls of r on g (GlRaster::set_arrays for the recorded arrays); gen_textures' names go to a scratch array
void gl_replay(const GlRec &r, GlRaster &g);

}  // namespace gl
}  // namespace portsim

#endif  // PORTSIM_SIM_GL_RASTER_H
