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

  // ---- the calls of the original's setup (Terrain constructor, initSystem, initTerrain) ----
  // They default to nothing so a sink that only draws need not know them. Like a real driver, a sink ignores every call made before
  // a context exists (the Terrain constructor loads all its textures that way), and glGenTextures then writes no name.
  virtual void context_create() {}                         // wglCreateContext + wglMakeCurrent on the display surface
  virtual void hint(uint32_t target, uint32_t mode) { (void)target; (void)mode; }
  virtual void enable_client_state(uint32_t array) { (void)array; }
  virtual void front_face(uint32_t mode) { (void)mode; }
  virtual void viewport(int32_t x, int32_t y, int32_t w, int32_t h) { (void)x; (void)y; (void)w; (void)h; }
  virtual void matrix_mode(uint32_t mode) { (void)mode; }
  virtual void ortho(double l, double r, double b, double t, double n, double f) { (void)l; (void)r; (void)b; (void)t; (void)n; (void)f; }
  virtual void light(uint32_t light, uint32_t pname, const float *v4) { (void)light; (void)pname; (void)v4; }       // four floats
  virtual void material(uint32_t face, uint32_t pname, const float *v4) { (void)face; (void)pname; (void)v4; }      // four floats
  virtual void material_shininess(uint32_t face, int32_t s) { (void)face; (void)s; }
  // false when no context exists (the names are not written: the caller's variables keep their old values)
  virtual bool gen_textures(int32_t n, uint32_t *names) { (void)n; (void)names; return false; }
  virtual void delete_textures(int32_t n, const uint32_t *names) { (void)n; (void)names; }
  virtual void tex_parameteri(uint32_t target, uint32_t pname, int32_t v) { (void)target; (void)pname; (void)v; }
  virtual void tex_parameterf(uint32_t target, uint32_t pname, float v) { (void)target; (void)pname; (void)v; }
  virtual void tex_envi(uint32_t target, uint32_t pname, int32_t v) { (void)target; (void)pname; (void)v; }
  virtual void tex_image2d(uint32_t target, int32_t level, int32_t internal, int32_t w, int32_t h, int32_t border, uint32_t format,
                           uint32_t type, const uint8_t *data) {
    (void)target; (void)level; (void)internal; (void)w; (void)h; (void)border; (void)format; (void)type; (void)data;
  }
  virtual void vertex_pointer(int32_t size, uint32_t type, int32_t stride) { (void)size; (void)type; (void)stride; }
  virtual void normal_pointer(uint32_t type, int32_t stride) { (void)type; (void)stride; }
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

  // setup calls: the same lines as the recorder's, with the pointer arguments dropped (texture names and bytes: ids= / tex= tokens)
  void context_create() override {
    line("wglCreateContext");
    line("wglMakeCurrent");
    context_ = true;
  }
  void hint(uint32_t t, uint32_t m) override { line2("glHint", t, m); }
  void enable_client_state(uint32_t a) override { line1("glEnableClientState", a); }
  void front_face(uint32_t m) override { line1("glFrontFace", m); }
  void viewport(int32_t x, int32_t y, int32_t w, int32_t h) override {
    std::string s;
    arg_u(s, static_cast<uint32_t>(x)); arg_u(s, static_cast<uint32_t>(y)); arg_u(s, static_cast<uint32_t>(w)); arg_u(s, static_cast<uint32_t>(h));
    line("glViewport", s);
  }
  void matrix_mode(uint32_t m) override { line1("glMatrixMode", m); }
  void ortho(double l, double r, double b, double t, double n, double f) override {
    std::string s;
    arg_d(s, l); arg_d(s, r); arg_d(s, b); arg_d(s, t); arg_d(s, n); arg_d(s, f);
    line("glOrtho", s);
  }
  void light(uint32_t l, uint32_t p, const float *v) override {
    std::string s;
    arg_u(s, l); arg_u(s, p);
    line("glLightfv", s + " " + vec_text(v, 4));
  }
  void material(uint32_t face, uint32_t p, const float *v) override {
    std::string s;
    arg_u(s, face); arg_u(s, p);
    line("glMaterialfv", s + " " + vec_text(v, 4));
  }
  void material_shininess(uint32_t face, int32_t v) override {
    std::string s;
    arg_u(s, face); arg_u(s, 0x1601u); arg_u(s, static_cast<uint32_t>(v));
    line("glMateriali", s);
  }
  bool gen_textures(int32_t n, uint32_t *names) override {
    std::string s;
    arg_u(s, static_cast<uint32_t>(n));
    if (!context_) {
      line("glGenTextures", s);
      return false;
    }
    std::string ids = "ids=";
    for (int32_t i = 0; i < n; ++i) {
      names[i] = next_name_++;
      if (i) ids += ',';
      hex(ids, names[i]);
    }
    line("glGenTextures", s + " " + ids);
    return true;
  }
  void delete_textures(int32_t n, const uint32_t *names) override {
    std::string s;
    arg_u(s, static_cast<uint32_t>(n));
    std::string ids = "ids=";
    for (int32_t i = 0; i < n; ++i) {
      if (i) ids += ',';
      hex(ids, names[i]);
    }
    line("glDeleteTextures", s + " " + ids);
  }
  void tex_parameteri(uint32_t t, uint32_t p, int32_t v) override { line3("glTexParameteri", t, p, static_cast<uint32_t>(v)); }
  void tex_parameterf(uint32_t t, uint32_t p, float v) override {
    std::string s;
    arg_u(s, t); arg_u(s, p); arg_f(s, v);
    line("glTexParameterf", s);
  }
  void tex_envi(uint32_t t, uint32_t p, int32_t v) override { line3("glTexEnvi", t, p, static_cast<uint32_t>(v)); }
  void tex_image2d(uint32_t target, int32_t level, int32_t internal, int32_t w, int32_t h, int32_t border, uint32_t format, uint32_t type,
                   const uint8_t *data) override {
    std::string s;
    arg_u(s, target); arg_u(s, static_cast<uint32_t>(level)); arg_u(s, static_cast<uint32_t>(internal)); arg_u(s, static_cast<uint32_t>(w));
    arg_u(s, static_cast<uint32_t>(h)); arg_u(s, static_cast<uint32_t>(border)); arg_u(s, format); arg_u(s, type);
    // the recorder's FNV-1a (32 bit) of w * h * bytes-per-pixel bytes, "tex=0x<hex>"
    const int bpp = format == 0x1907u ? 3 : format == 0x1908u ? 4 : 0;
    if (data && bpp && w > 0 && h > 0) {
      uint32_t hh = 0x811c9dc5u;
      const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(bpp);
      for (size_t i = 0; i < n; ++i) { hh ^= data[i]; hh *= 0x01000193u; }
      std::string t = "tex=";
      hex(t, hh);
      line("glTexImage2D", s + " " + t);
    } else {
      line("glTexImage2D", s);
    }
  }
  void vertex_pointer(int32_t size, uint32_t type, int32_t stride) override { line3("glVertexPointer", static_cast<uint32_t>(size), type, static_cast<uint32_t>(stride)); }
  void normal_pointer(uint32_t type, int32_t stride) override { line2("glNormalPointer", type, static_cast<uint32_t>(stride)); }

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
  void line1(const char *name, uint32_t a) {
    std::string s;
    arg_u(s, a);
    line(name, s);
  }
  void line2(const char *name, uint32_t a, uint32_t b) {
    std::string s;
    arg_u(s, a); arg_u(s, b);
    line(name, s);
  }
  void line3(const char *name, uint32_t a, uint32_t b, uint32_t c) {
    std::string s;
    arg_u(s, a); arg_u(s, b); arg_u(s, c);
    line(name, s);
  }
  static std::string vec_text(const float *v, int n) {
    std::string s = "v=";
    for (int i = 0; i < n; ++i) {
      if (i) s += ',';
      uint32_t b;
      std::memcpy(&b, &v[i], 4);
      hex(s, b);
    }
    return s;
  }
  bool context_ = false;
  uint32_t next_name_ = 1;
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
