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
#include <map>
#include <string>
#include <vector>

#include "portsim/sim/gl_sink.h"

namespace portsim {
namespace gl {

class GlRaster : public GlSink {
 public:
  // width x height pixels, 16 bits each (RGB555), row 0 = window y 0
  GlRaster(int width, int height);

  int width() const { return w_; }
  int height() const { return h_; }
  std::vector<uint16_t> &pixels() { return fb_; }
  // diagnostics: the colour of every written pixel before it is quantised to 5 bits (0..255 per channel, 3 floats a pixel)
  void keep_float(bool on) {
    keep_float_ = on;
    if (on) { ffb_.assign(fb_.size() * 3, -1.0f); sfb_.assign(fb_.size() * 8, -1.0f); }
  }
  // per written pixel: source r g b (0..255, after lighting), source alpha (0..255), the destination's r g b 5-bit values and
  // 1 when the write was blended
  const std::vector<float> &source_pixels() const { return sfb_; }
  const std::vector<float> &float_pixels() const { return ffb_; }
  const std::vector<uint16_t> &pixels() const { return fb_; }

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

  // ---- the state calls of the original's setup ----
  void viewport(int32_t x, int32_t y, int32_t w, int32_t h);
  void matrix_mode(uint32_t mode);
  void ortho(double l, double r, double b, double t, double n, double f);
  void front_face(uint32_t mode);
  void tex_parameter(uint32_t pname, uint32_t value);
  void tex_env(uint32_t target, uint32_t pname, uint32_t value);   // only target GL_TEXTURE_ENV (0x2300) takes effect
  void light(uint32_t light, uint32_t pname, const float *v);       // GL_LIGHT0; pname AMBIENT / DIFFUSE / SPECULAR / POSITION
  void material(uint32_t pname, const float *v);                    // GL_SPECULAR only (the render path sets nothing else)
  void material_shininess(int32_t s);
  void normal3f(float x, float y, float z);
  void color4f(float r, float g, float b, float a);   // the current colour (used when lighting is off)
  void tex_image2d(uint32_t width, uint32_t height, uint32_t format, const uint8_t *data);   // GL_RGB / GL_RGBA, unsigned byte, level 0
  void delete_textures(const uint32_t *ids, int n);
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
  int tune[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  // counters of the last frame (what the rasteriser saw), for the tests' diagnostics
  struct Stats {
    int64_t triangles = 0, culled = 0, drawn_px = 0, blended_px = 0, bad_texture = 0;
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
  };
  struct Mat {
    double m[16];         // column-major
  };

  int w_, h_;
  std::vector<uint16_t> fb_;
  std::vector<float> ffb_, sfb_;
  bool keep_float_ = false;
  // matrices
  uint32_t mode_ = 0x1700;               // GL_MODELVIEW
  std::vector<Mat> stack_[2];
  Mat mv_, proj_;
  int32_t vp_x_ = 0, vp_y_ = 0, vp_w_ = 0, vp_h_ = 0;
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
  void mul(const Mat &b);
  void emit(double x, double y, double z, const float *normal);
  void triangle(const Vtx &a, const Vtx &b, const Vtx &c);
};

}  // namespace gl
}  // namespace portsim

#endif  // PORTSIM_SIM_GL_RASTER_H
