// The OpenGL boundary of the port's terrain renderer (phase 5 step 3; plan re/PHASE5_TERRAIN_PLAN.md section 3).
//
// The original's Terrain.dll draws with fixed-function OpenGL 1.1 calls into a 16-bit DIB (Windows' software
// implementation). The port's renderer code (portsim/sim/terrain_render.h) issues the same calls, in the same order and
// with bit-identical arguments, through this interface; what stands behind it decides what happens to them:
//   - GlStreamSink (below) writes each call as one text line, the form the recorder re/tools/gl_record.py writes for the
//     original (pointer arguments dropped, the values behind the glXXXfv pointers kept), so a pass can be compared with the
//     original's recorded pass line by line (port/test/test_terrain_render.py);
//   - a rasteriser sink (later) draws the calls into an RGB555 back buffer.
// Floats and doubles are written as their IEEE bit patterns, doubles as low dword then high dword (the stack order).
#ifndef PORTSIM_SIM_GL_SINK_H
#define PORTSIM_SIM_GL_SINK_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace portsim {
namespace gl {

// The numeric values of the GL 1.1 enumerants the render path names.
constexpr uint32_t kTriangles = 0x0004, kTriangleFan = 0x0006, kTexture2D = 0x0DE1, kBlend = 0x0BE2;
constexpr uint32_t kSrcAlpha = 0x0302, kOneMinusSrcAlpha = 0x0303;

class GlSink {
 public:
  virtual ~GlSink() = default;
  virtual void load_identity() = 0;
  virtual void push_matrix() = 0;
  virtual void pop_matrix() = 0;
  virtual void rotated(double angle, double x, double y, double z) = 0;
  virtual void rotatef(float angle, float x, float y, float z) = 0;
  virtual void translatef(float x, float y, float z) = 0;
  virtual void bind_texture(uint32_t target, uint32_t id) = 0;
  virtual void begin(uint32_t mode) = 0;
  virtual void end() = 0;
  virtual void tex_coord2fv(const float *v) = 0;  // two floats
  virtual void tex_coord2f(float s, float t) = 0;
  virtual void array_element(int32_t i) = 0;      // vertex / normal arrays the host set up with glVertexPointer / glNormalPointer
  virtual void normal3fv(const float *v) = 0;     // three floats
  virtual void vertex3fv(const float *v) = 0;     // three floats
  virtual void vertex3f(float x, float y, float z) = 0;
  virtual void enable(uint32_t cap) = 0;
  virtual void disable(uint32_t cap) = 0;
  virtual void blend_func(uint32_t src, uint32_t dst) = 0;
  virtual void flush() = 0;
};

// One text line per call: "<name> <hex args,...>" plus " v=<hex floats>" for the glXXXfv calls (no pointers), lines
// separated by '\n'. Same text as the recorder's lines after dropping the pointer argument of the vector calls.
class GlStreamSink : public GlSink {
 public:
  std::string text;
  int64_t calls = 0;

  void load_identity() override { line("glLoadIdentity"); }
  void push_matrix() override { line("glPushMatrix"); }
  void pop_matrix() override { line("glPopMatrix"); }
  void rotated(double a, double x, double y, double z) override {
    std::string s;
    arg_d(s, a); arg_d(s, x); arg_d(s, y); arg_d(s, z);
    line("glRotated", s);
  }
  void rotatef(float a, float x, float y, float z) override {
    std::string s;
    arg_f(s, a); arg_f(s, x); arg_f(s, y); arg_f(s, z);
    line("glRotatef", s);
  }
  void translatef(float x, float y, float z) override {
    std::string s;
    arg_f(s, x); arg_f(s, y); arg_f(s, z);
    line("glTranslatef", s);
  }
  void bind_texture(uint32_t target, uint32_t id) override {
    std::string s;
    arg_u(s, target); arg_u(s, id);
    line("glBindTexture", s);
  }
  void begin(uint32_t mode) override {
    std::string s;
    arg_u(s, mode);
    line("glBegin", s);
  }
  void end() override { line("glEnd"); }
  void tex_coord2fv(const float *v) override { vec("glTexCoord2fv", v, 2); }
  void tex_coord2f(float s0, float t0) override {
    std::string s;
    arg_f(s, s0); arg_f(s, t0);
    line("glTexCoord2f", s);
  }
  void array_element(int32_t i) override {
    std::string s;
    arg_u(s, static_cast<uint32_t>(i));
    line("glArrayElement", s);
  }
  void normal3fv(const float *v) override { vec("glNormal3fv", v, 3); }
  void vertex3fv(const float *v) override { vec("glVertex3fv", v, 3); }
  void vertex3f(float x, float y, float z) override {
    std::string s;
    arg_f(s, x); arg_f(s, y); arg_f(s, z);
    line("glVertex3f", s);
  }
  void enable(uint32_t cap) override {
    std::string s;
    arg_u(s, cap);
    line("glEnable", s);
  }
  void disable(uint32_t cap) override {
    std::string s;
    arg_u(s, cap);
    line("glDisable", s);
  }
  void blend_func(uint32_t a, uint32_t b) override {
    std::string s;
    arg_u(s, a); arg_u(s, b);
    line("glBlendFunc", s);
  }
  void flush() override { line("glFlush"); }

 private:
  static void hex(std::string &s, uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%x", static_cast<unsigned>(v));
    s += b;
  }
  static void arg_u(std::string &s, uint32_t v) {
    if (!s.empty()) s += ',';
    hex(s, v);
  }
  static void arg_f(std::string &s, float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    arg_u(s, b);
  }
  static void arg_d(std::string &s, double d) {
    uint64_t b;
    std::memcpy(&b, &d, 8);
    arg_u(s, static_cast<uint32_t>(b));        // low dword first, as on the stack
    arg_u(s, static_cast<uint32_t>(b >> 32));
  }
  void line(const char *name, const std::string &args = std::string()) {
    text += name;
    if (!args.empty()) { text += ' '; text += args; }
    text += '\n';
    ++calls;
  }
  void vec(const char *name, const float *v, int n) {
    std::string s = "v=";
    for (int i = 0; i < n; ++i) {
      if (i) s += ',';
      uint32_t b;
      std::memcpy(&b, &v[i], 4);
      hex(s, b);
    }
    line(name, s);
  }
};

}  // namespace gl
}  // namespace portsim

#endif  // PORTSIM_SIM_GL_SINK_H
