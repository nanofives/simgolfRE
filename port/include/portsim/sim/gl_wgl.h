// A GL sink that draws through the host's own OpenGL 1.1 software implementation (roadmap S3.15, U-1810): Windows' generic
// opengl32 rendering into a 16-bit DIB section, the surface the original's Terrain.dll drew into. The original's terrain picture is
// that implementation's output, so on a Windows host this sink reproduces it bit for bit (re/tools/glexact/glreplay.exe does the same
// replay of the original's recorded passes: 0 of 480,000 pixels differ on the c7 corpus passes); GlRaster (gl_raster.h) stays the
// portable rasteriser, whose residual is within one 5-bit level on about 0.05 % of the textured pixels (U-1392).
//
// The DIB is what initSystem's DC holds in the original (log/roadmap/S3/2_notes.md; glreplay's meta.txt of every corpus pass):
// width x height, 16 bits per pixel, BI_BITFIELDS with masks 0x7c00 / 0x3e0 / 0x1f, biHeight = +height, memory row 0 = window y 0
// as in the backend's Surface16 and GlRaster. The pixel format is PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL with 16 colour bits
// (Terrain.dll's g_pfd). opengl32.dll is loaded at run time (LoadLibrary + GetProcAddress), so the library links nothing new and a
// host without it reports the sink unavailable.
//
// Like a driver, the sink ignores every call made before context_create() (the Terrain constructor's texture loads). Texture names:
// the port's setup chooses its names by the original's rule (terrain_state.h); this sink asks the implementation for its own names
// and maps the port's to them (gen_textures_mapped), as glreplay maps the recorded ones. The vertex / normal arrays are the port's
// TerrainGlobals arrays, given per pass with set_arrays (glVertexPointer / glNormalPointer are re-issued with their addresses).
// Every call makes the sink's context current on the calling thread when it is not.
#ifndef PORTSIM_SIM_GL_WGL_H
#define PORTSIM_SIM_GL_WGL_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "portsim/sim/gl_sink.h"

namespace portsim {
class Ctx;
namespace gl {

class WglSink : public GlSink {
 public:
  // variant: 0 = the original's DIB; the negative controls of gfx_terrain.cpp make a wrong one (1 = RGB565 masks, 2 = rows top-down,
  // 3 = the port's texture names bound unmapped, 4 = no pixel format chosen: the sink is not ok(), as on a host without one)
  WglSink(int width, int height, int variant = 0);
  ~WglSink() override;
  WglSink(const WglSink &) = delete;
  WglSink &operator=(const WglSink &) = delete;

  // false when the host has no opengl32 / no generic pixel format (the reason in why())
  bool ok() const;
  const std::string &why() const;
  bool has_context() const;
  int width() const { return w_; }
  int height() const { return h_; }

  // the DIB's pixels (RGB555, memory order) in and out around a pass; get_pixels finishes the GL work first (glFinish + GdiFlush)
  void set_pixels(const std::vector<uint16_t> &px);
  void get_pixels(std::vector<uint16_t> &px);
  void set_arrays(const float *vertices, const float *normals);
  // gen_textures with the port's names already chosen: asks the implementation for n names and maps names[i] to them
  bool gen_textures_mapped(int32_t n, const uint32_t *names);

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
  bool gen_textures(int32_t n, uint32_t *names) override;
  void delete_textures(int32_t n, const uint32_t *names) override;
  void tex_parameteri(uint32_t target, uint32_t pname, int32_t v) override;
  void tex_parameterf(uint32_t target, uint32_t pname, float v) override;
  void tex_envi(uint32_t target, uint32_t pname, int32_t v) override;
  void tex_image2d(uint32_t target, int32_t level, int32_t internal, int32_t w, int32_t h, int32_t border, uint32_t format,
                   uint32_t type, const uint8_t *data) override;
  void vertex_pointer(int32_t size, uint32_t type, int32_t stride) override;
  void normal_pointer(uint32_t type, int32_t stride) override;

  int64_t calls = 0;   // calls forwarded to the implementation (made with a context)

 private:
  struct Impl;
  std::unique_ptr<Impl> p_;
  int w_, h_;
};

// One sink for the GL setup that feeds two (LiveTerrain's GlRaster and a WglSink): every call goes to both, glGenTextures takes the
// first's names and maps them in the second.
class SetupPair : public GlSink {
 public:
  SetupPair(GlSink *a, WglSink *b) : a_(a), b_(b) {}
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
  void viewport(int32_t x, int32_t y, int32_t w, int32_t h) override { a_->viewport(x, y, w, h); b_->viewport(x, y, w, h); }
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
    if (r) b_->gen_textures_mapped(n, names);
    return r;
  }
  void delete_textures(int32_t n, const uint32_t *names) override { a_->delete_textures(n, names); b_->delete_textures(n, names); }
  void tex_parameteri(uint32_t t, uint32_t p, int32_t v) override { a_->tex_parameteri(t, p, v); b_->tex_parameteri(t, p, v); }
  void tex_parameterf(uint32_t t, uint32_t p, float v) override { a_->tex_parameterf(t, p, v); b_->tex_parameterf(t, p, v); }
  void tex_envi(uint32_t t, uint32_t p, int32_t v) override { a_->tex_envi(t, p, v); b_->tex_envi(t, p, v); }
  void tex_image2d(uint32_t t, int32_t l, int32_t i, int32_t w, int32_t h, int32_t bo, uint32_t f, uint32_t ty,
                   const uint8_t *d) override {
    a_->tex_image2d(t, l, i, w, h, bo, f, ty, d);
    b_->tex_image2d(t, l, i, w, h, bo, f, ty, d);
  }
  void vertex_pointer(int32_t s, uint32_t t, int32_t st) override { a_->vertex_pointer(s, t, st); b_->vertex_pointer(s, t, st); }
  void normal_pointer(uint32_t t, int32_t st) override { a_->normal_pointer(t, st); b_->normal_pointer(t, st); }

 private:
  GlSink *a_;
  WglSink *b_;
};

// The startup self-check (roadmap S3.20, decision G13, final: GlRaster is the default terrain renderer; the host's OpenGL is opt-in,
// and `auto` uses it only after this check, falling back to GlRaster). It draws a reference pass through a WglSink the way the live path does (the port's own GL setup from the user's install
// through GameFiles: Terrain constructor, initSystem 800 x 600, initTerrain; texture names chosen by the original's rule and mapped
// in the sink, as SetupPair does), then compares SHA-256 of a 32 x 32 rectangle of the DIB with a pinned hash of the ORIGINAL's
// pixels there. The reference (gl_wgl_ref.inc, generated by log/roadmap/S3/20_make_ref.py) is the part of a terrain pass the
// original drew and the recorder kept (port/corpus/gl/c7, pass z2_r1) that reaches that rectangle: 19 triangles. Its rectangle holds
// 45 pixels where GlRaster's residual (U-1392) differs from the original, so a rasteriser that is not the original's implementation
// reads RED (port/test/test_gl_wgl_selfcheck.py). No texture or other byte of the game's files is in the reference.
struct SelfCheck {
  bool pass = false;
  std::string reason;      // why it passed or failed
  std::string got, want;   // SHA-256 (hex) of the rectangle drawn / of the original's
  double ms = 0.0;         // wall time of the whole check (setup, pass, read back)
  std::vector<uint16_t> rect;   // the rectangle drawn (rows top to bottom in memory order), empty when nothing was drawn
};
// variant: the WglSink variant (as LiveTerrain makes it for the negative controls 8 / 9 / 11 / 12); perturb (negative controls of
// the check itself): 1 = the pinned hash with one hex digit changed, 2 = the reference pass with every texture coordinate moved by one texel
SelfCheck wgl_self_check(Ctx &ctx, int variant = 0, int perturb = 0);
// The same reference pass through the portable rasteriser (tests). Since roadmap S3.21 GlRaster equals the original on the reference
// rectangle; old_rules = 1 sets GlRaster tune 28 and 29 (S3.21's pre-rule vertex model and no clipping) on this rasteriser only,
// the check's negative control (45 of the 1,024 px differ, the U-1392 residual S3.20 measured)
SelfCheck raster_self_check(Ctx &ctx, int old_rules = 0);
// SHA-256 of a byte string, lower-case hex (FIPS 180-4)
std::string sha256_hex(const uint8_t *data, size_t n);

}  // namespace gl
}  // namespace portsim

#endif  // PORTSIM_SIM_GL_WGL_H
