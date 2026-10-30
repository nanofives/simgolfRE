// The host-OpenGL sink of portsim/sim/gl_wgl.h (roadmap S3.15, U-1810) and its startup self-check (roadmap S3.20, decision G13).
// Registers the harness entries glWgl.selfCheck / glWgl.rasterCheck (below).
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/gl_wgl.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>

#include "portsim/gamefiles.h"
#include "portsim/registry.h"
#include "portsim/sim/gl_raster.h"
#include "portsim/sim/terrain_gl_setup.h"
#include <map>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace portsim {
namespace gl {

#if defined(_WIN32)

namespace {

// the opengl32 entry points the terrain renderer's calls need (OpenGL 1.1, all exported by opengl32.dll)
struct GlApi {
  HMODULE dll = nullptr;
  HGLRC(WINAPI *wglCreateContext)(HDC) = nullptr;
  BOOL(WINAPI *wglDeleteContext)(HGLRC) = nullptr;
  BOOL(WINAPI *wglMakeCurrent)(HDC, HGLRC) = nullptr;
  HGLRC(WINAPI *wglGetCurrentContext)() = nullptr;
  void(WINAPI *LoadIdentity)() = nullptr;
  void(WINAPI *PushMatrix)() = nullptr;
  void(WINAPI *PopMatrix)() = nullptr;
  void(WINAPI *Rotated)(double, double, double, double) = nullptr;
  void(WINAPI *Rotatef)(float, float, float, float) = nullptr;
  void(WINAPI *Translatef)(float, float, float) = nullptr;
  void(WINAPI *BindTexture)(uint32_t, uint32_t) = nullptr;
  void(WINAPI *Begin)(uint32_t) = nullptr;
  void(WINAPI *End)() = nullptr;
  void(WINAPI *TexCoord2fv)(const float *) = nullptr;
  void(WINAPI *TexCoord2f)(float, float) = nullptr;
  void(WINAPI *ArrayElement)(int32_t) = nullptr;
  void(WINAPI *Normal3fv)(const float *) = nullptr;
  void(WINAPI *Vertex3fv)(const float *) = nullptr;
  void(WINAPI *Vertex3f)(float, float, float) = nullptr;
  void(WINAPI *Enable)(uint32_t) = nullptr;
  void(WINAPI *Disable)(uint32_t) = nullptr;
  void(WINAPI *BlendFunc)(uint32_t, uint32_t) = nullptr;
  void(WINAPI *Flush)() = nullptr;
  void(WINAPI *Finish)() = nullptr;
  void(WINAPI *Hint)(uint32_t, uint32_t) = nullptr;
  void(WINAPI *EnableClientState)(uint32_t) = nullptr;
  void(WINAPI *FrontFace)(uint32_t) = nullptr;
  void(WINAPI *Viewport)(int32_t, int32_t, int32_t, int32_t) = nullptr;
  void(WINAPI *MatrixMode)(uint32_t) = nullptr;
  void(WINAPI *Ortho)(double, double, double, double, double, double) = nullptr;
  void(WINAPI *Lightfv)(uint32_t, uint32_t, const float *) = nullptr;
  void(WINAPI *Materialfv)(uint32_t, uint32_t, const float *) = nullptr;
  void(WINAPI *Materiali)(uint32_t, uint32_t, int32_t) = nullptr;
  void(WINAPI *GenTextures)(int32_t, uint32_t *) = nullptr;
  void(WINAPI *DeleteTextures)(int32_t, const uint32_t *) = nullptr;
  void(WINAPI *TexParameteri)(uint32_t, uint32_t, int32_t) = nullptr;
  void(WINAPI *TexParameterf)(uint32_t, uint32_t, float) = nullptr;
  void(WINAPI *TexEnvi)(uint32_t, uint32_t, int32_t) = nullptr;
  void(WINAPI *TexImage2D)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, const void *) = nullptr;
  void(WINAPI *VertexPointer)(int32_t, uint32_t, int32_t, const void *) = nullptr;
  void(WINAPI *NormalPointer)(uint32_t, int32_t, const void *) = nullptr;
  std::string why;

  template <class F>
  bool get(F &f, const char *name) {
    f = reinterpret_cast<F>(reinterpret_cast<void *>(GetProcAddress(dll, name)));
    if (!f && why.empty()) why = std::string("opengl32.dll has no ") + name;
    return f != nullptr;
  }
  bool load() {
    dll = LoadLibraryA("opengl32.dll");
    if (!dll) {
      why = "opengl32.dll not found";
      return false;
    }
    bool ok = get(wglCreateContext, "wglCreateContext") & get(wglDeleteContext, "wglDeleteContext") &
              get(wglMakeCurrent, "wglMakeCurrent") & get(wglGetCurrentContext, "wglGetCurrentContext") &
              get(LoadIdentity, "glLoadIdentity") & get(PushMatrix, "glPushMatrix") & get(PopMatrix, "glPopMatrix") &
              get(Rotated, "glRotated") & get(Rotatef, "glRotatef") & get(Translatef, "glTranslatef") &
              get(BindTexture, "glBindTexture") & get(Begin, "glBegin") & get(End, "glEnd") & get(TexCoord2fv, "glTexCoord2fv") &
              get(TexCoord2f, "glTexCoord2f") & get(ArrayElement, "glArrayElement") & get(Normal3fv, "glNormal3fv") &
              get(Vertex3fv, "glVertex3fv") & get(Vertex3f, "glVertex3f") & get(Enable, "glEnable") & get(Disable, "glDisable") &
              get(BlendFunc, "glBlendFunc") & get(Flush, "glFlush") & get(Finish, "glFinish") & get(Hint, "glHint") &
              get(EnableClientState, "glEnableClientState") & get(FrontFace, "glFrontFace") & get(Viewport, "glViewport") &
              get(MatrixMode, "glMatrixMode") & get(Ortho, "glOrtho") & get(Lightfv, "glLightfv") & get(Materialfv, "glMaterialfv") &
              get(Materiali, "glMateriali") & get(GenTextures, "glGenTextures") & get(DeleteTextures, "glDeleteTextures") &
              get(TexParameteri, "glTexParameteri") & get(TexParameterf, "glTexParameterf") & get(TexEnvi, "glTexEnvi") &
              get(TexImage2D, "glTexImage2D") & get(VertexPointer, "glVertexPointer") & get(NormalPointer, "glNormalPointer");
    return ok;
  }
};

GlApi &api() {
  static GlApi a;
  static const bool loaded = a.load();
  (void)loaded;
  return a;
}

}  // namespace

struct WglSink::Impl {
  GlApi &gl = api();
  HDC dc = nullptr;
  HBITMAP bm = nullptr;
  HGDIOBJ old = nullptr;
  uint16_t *bits = nullptr;
  HGLRC rc = nullptr;
  std::string why;
  std::map<uint32_t, uint32_t> names;   // the port's texture name -> the implementation's
  int32_t vsize = 3, vstride = 0, nstride = 0;
  uint32_t vtype = 0x1406, ntype = 0x1406;   // GL_FLOAT
  const float *verts = nullptr, *norms = nullptr;
  bool map_names = true;   // false: negative control 11 (variant 3)

  bool cur() {
    if (!rc) return false;
    if (gl.wglGetCurrentContext() != rc) gl.wglMakeCurrent(dc, rc);
    return true;
  }
  uint32_t map(uint32_t id) const {
    if (!map_names) return id;
    auto it = names.find(id);
    return it == names.end() ? id : it->second;
  }
};

WglSink::WglSink(int width, int height, int variant) : p_(std::make_unique<Impl>()), w_(width), h_(height) {
  Impl &p = *p_;
  p.map_names = variant != 3;
  if (!p.gl.why.empty()) {
    p.why = p.gl.why;
    return;
  }
  // the original's DIB (glreplay's meta.txt of every corpus pass): 16 bpp, BI_BITFIELDS 555, biHeight +h
  struct {
    BITMAPINFOHEADER h;
    DWORD m[3];
  } bi{};
  bi.h.biSize = sizeof(BITMAPINFOHEADER);
  bi.h.biWidth = width;
  bi.h.biHeight = variant == 2 ? -height : height;
  bi.h.biPlanes = 1;
  bi.h.biBitCount = 16;
  bi.h.biCompression = BI_BITFIELDS;
  bi.m[0] = variant == 1 ? 0xf800 : 0x7c00;
  bi.m[1] = variant == 1 ? 0x7e0 : 0x3e0;
  bi.m[2] = 0x1f;
  p.dc = CreateCompatibleDC(nullptr);
  void *bits = nullptr;
  p.bm = p.dc ? CreateDIBSection(p.dc, reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
  if (!p.bm) {
    p.why = "CreateDIBSection failed";
    return;
  }
  p.bits = static_cast<uint16_t *>(bits);
  p.old = SelectObject(p.dc, p.bm);
  PIXELFORMATDESCRIPTOR pfd{};
  pfd.nSize = sizeof pfd;
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL;
  pfd.cColorBits = 16;
  const int pf = variant == 4 ? 0 : ChoosePixelFormat(p.dc, &pfd);   // variant 4: control 12 (as if the host had none)
  if (!pf || !SetPixelFormat(p.dc, pf, &pfd)) p.why = "no generic pixel format for a 16-bit DIB";
}

WglSink::~WglSink() {
  Impl &p = *p_;
  if (p.rc) {
    if (p.gl.wglGetCurrentContext() == p.rc) p.gl.wglMakeCurrent(nullptr, nullptr);
    p.gl.wglDeleteContext(p.rc);
  }
  if (p.dc) {
    if (p.old) SelectObject(p.dc, p.old);
    DeleteDC(p.dc);
  }
  if (p.bm) DeleteObject(p.bm);
}

bool WglSink::ok() const { return p_->why.empty(); }
const std::string &WglSink::why() const { return p_->why; }
bool WglSink::has_context() const { return p_->rc != nullptr; }

void WglSink::set_pixels(const std::vector<uint16_t> &px) {
  if (!p_->bits || px.size() != static_cast<size_t>(w_) * static_cast<size_t>(h_)) return;
  GdiFlush();
  std::memcpy(p_->bits, px.data(), px.size() * 2);
}

void WglSink::get_pixels(std::vector<uint16_t> &px) {
  if (!p_->bits) return;
  if (p_->cur()) p_->gl.Finish();
  GdiFlush();
  px.assign(p_->bits, p_->bits + static_cast<size_t>(w_) * static_cast<size_t>(h_));
}

void WglSink::set_arrays(const float *vertices, const float *normals) {
  Impl &p = *p_;
  p.verts = vertices;
  p.norms = normals;
  if (!p.cur()) return;
  p.gl.VertexPointer(p.vsize, p.vtype, p.vstride, p.verts);
  p.gl.NormalPointer(p.ntype, p.nstride, p.norms);
}

void WglSink::context_create() {
  Impl &p = *p_;
  if (!ok()) return;
  if (p.rc) {
    if (p.gl.wglGetCurrentContext() == p.rc) p.gl.wglMakeCurrent(nullptr, nullptr);
    p.gl.wglDeleteContext(p.rc);
    p.names.clear();
  }
  p.rc = p.gl.wglCreateContext(p.dc);
  if (!p.rc) {
    p.why = "wglCreateContext failed";
    return;
  }
  p.gl.wglMakeCurrent(p.dc, p.rc);
}

bool WglSink::gen_textures_mapped(int32_t n, const uint32_t *names) {
  Impl &p = *p_;
  if (!p.cur() || n <= 0) return false;
  std::vector<uint32_t> real(static_cast<size_t>(n), 0);
  p.gl.GenTextures(n, real.data());
  ++calls;
  for (int32_t i = 0; i < n; ++i) p.names[names[i]] = real[static_cast<size_t>(i)];
  return true;
}

bool WglSink::gen_textures(int32_t n, uint32_t *names) {
  Impl &p = *p_;
  if (!p.cur()) return false;
  p.gl.GenTextures(n, names);
  ++calls;
  for (int32_t i = 0; i < n; ++i) p.names[names[i]] = names[i];
  return true;
}

void WglSink::delete_textures(int32_t n, const uint32_t *names) {
  Impl &p = *p_;
  if (!p.cur() || n <= 0) return;
  std::vector<uint32_t> real(static_cast<size_t>(n));
  for (int32_t i = 0; i < n; ++i) {
    real[static_cast<size_t>(i)] = p.map(names[i]);
    p.names.erase(names[i]);
  }
  p.gl.DeleteTextures(n, real.data());
  ++calls;
}

#define WGL_CALL(expr) \
  do {                 \
    if (!p_->cur()) return; \
    p_->gl.expr;       \
    ++calls;           \
  } while (0)

void WglSink::load_identity() { WGL_CALL(LoadIdentity()); }
void WglSink::push_matrix() { WGL_CALL(PushMatrix()); }
void WglSink::pop_matrix() { WGL_CALL(PopMatrix()); }
void WglSink::rotated(double a, double x, double y, double z) { WGL_CALL(Rotated(a, x, y, z)); }
void WglSink::rotatef(float a, float x, float y, float z) { WGL_CALL(Rotatef(a, x, y, z)); }
void WglSink::translatef(float x, float y, float z) { WGL_CALL(Translatef(x, y, z)); }
void WglSink::bind_texture(uint32_t target, uint32_t id) { WGL_CALL(BindTexture(target, p_->map(id))); }
void WglSink::begin(uint32_t mode) { WGL_CALL(Begin(mode)); }
void WglSink::end() { WGL_CALL(End()); }
void WglSink::tex_coord2fv(const float *v) { WGL_CALL(TexCoord2fv(v)); }
void WglSink::tex_coord2f(float s, float t) { WGL_CALL(TexCoord2f(s, t)); }
void WglSink::array_element(int32_t i) { WGL_CALL(ArrayElement(i)); }
void WglSink::normal3fv(const float *v) { WGL_CALL(Normal3fv(v)); }
void WglSink::vertex3fv(const float *v) { WGL_CALL(Vertex3fv(v)); }
void WglSink::vertex3f(float x, float y, float z) { WGL_CALL(Vertex3f(x, y, z)); }
void WglSink::enable(uint32_t cap) { WGL_CALL(Enable(cap)); }
void WglSink::disable(uint32_t cap) { WGL_CALL(Disable(cap)); }
void WglSink::blend_func(uint32_t s, uint32_t d) { WGL_CALL(BlendFunc(s, d)); }
void WglSink::flush() { WGL_CALL(Flush()); }
void WglSink::hint(uint32_t t, uint32_t m) { WGL_CALL(Hint(t, m)); }
void WglSink::enable_client_state(uint32_t a) { WGL_CALL(EnableClientState(a)); }
void WglSink::front_face(uint32_t m) { WGL_CALL(FrontFace(m)); }
void WglSink::viewport(int32_t x, int32_t y, int32_t w, int32_t h) { WGL_CALL(Viewport(x, y, w, h)); }
void WglSink::matrix_mode(uint32_t m) { WGL_CALL(MatrixMode(m)); }
void WglSink::ortho(double l, double r, double b, double t, double n, double f) { WGL_CALL(Ortho(l, r, b, t, n, f)); }
void WglSink::light(uint32_t l, uint32_t pn, const float *v) { WGL_CALL(Lightfv(l, pn, v)); }
void WglSink::material(uint32_t f, uint32_t pn, const float *v) { WGL_CALL(Materialfv(f, pn, v)); }
void WglSink::material_shininess(uint32_t f, int32_t s) { WGL_CALL(Materiali(f, 0x1601, s)); }   // GL_SHININESS
void WglSink::tex_parameteri(uint32_t t, uint32_t pn, int32_t v) { WGL_CALL(TexParameteri(t, pn, v)); }
void WglSink::tex_parameterf(uint32_t t, uint32_t pn, float v) { WGL_CALL(TexParameterf(t, pn, v)); }
void WglSink::tex_envi(uint32_t t, uint32_t pn, int32_t v) { WGL_CALL(TexEnvi(t, pn, v)); }
void WglSink::tex_image2d(uint32_t t, int32_t l, int32_t i, int32_t w, int32_t h, int32_t b, uint32_t f, uint32_t ty,
                          const uint8_t *d) {
  WGL_CALL(TexImage2D(t, l, i, w, h, b, f, ty, d));
}
void WglSink::vertex_pointer(int32_t size, uint32_t type, int32_t stride) {
  p_->vsize = size;
  p_->vtype = type;
  p_->vstride = stride;
  if (p_->verts) WGL_CALL(VertexPointer(size, type, stride, p_->verts));
}
void WglSink::normal_pointer(uint32_t type, int32_t stride) {
  p_->ntype = type;
  p_->nstride = stride;
  if (p_->norms) WGL_CALL(NormalPointer(type, stride, p_->norms));
}
#undef WGL_CALL

#else  // not Windows: no host implementation; every call is ignored and ok() is false

struct WglSink::Impl {
  std::string why = "no host OpenGL implementation on this platform";
};
WglSink::WglSink(int width, int height, int) : p_(std::make_unique<Impl>()), w_(width), h_(height) {}
WglSink::~WglSink() = default;
bool WglSink::ok() const { return false; }
const std::string &WglSink::why() const { return p_->why; }
bool WglSink::has_context() const { return false; }
void WglSink::set_pixels(const std::vector<uint16_t> &) {}
void WglSink::get_pixels(std::vector<uint16_t> &) {}
void WglSink::set_arrays(const float *, const float *) {}
bool WglSink::gen_textures_mapped(int32_t, const uint32_t *) { return false; }
void WglSink::load_identity() {}
void WglSink::push_matrix() {}
void WglSink::pop_matrix() {}
void WglSink::rotated(double, double, double, double) {}
void WglSink::rotatef(float, float, float, float) {}
void WglSink::translatef(float, float, float) {}
void WglSink::bind_texture(uint32_t, uint32_t) {}
void WglSink::begin(uint32_t) {}
void WglSink::end() {}
void WglSink::tex_coord2fv(const float *) {}
void WglSink::tex_coord2f(float, float) {}
void WglSink::array_element(int32_t) {}
void WglSink::normal3fv(const float *) {}
void WglSink::vertex3fv(const float *) {}
void WglSink::vertex3f(float, float, float) {}
void WglSink::enable(uint32_t) {}
void WglSink::disable(uint32_t) {}
void WglSink::blend_func(uint32_t, uint32_t) {}
void WglSink::flush() {}
void WglSink::context_create() {}
void WglSink::hint(uint32_t, uint32_t) {}
void WglSink::enable_client_state(uint32_t) {}
void WglSink::front_face(uint32_t) {}
void WglSink::viewport(int32_t, int32_t, int32_t, int32_t) {}
void WglSink::matrix_mode(uint32_t) {}
void WglSink::ortho(double, double, double, double, double, double) {}
void WglSink::light(uint32_t, uint32_t, const float *) {}
void WglSink::material(uint32_t, uint32_t, const float *) {}
void WglSink::material_shininess(uint32_t, int32_t) {}
bool WglSink::gen_textures(int32_t, uint32_t *) { return false; }
void WglSink::delete_textures(int32_t, const uint32_t *) {}
void WglSink::tex_parameteri(uint32_t, uint32_t, int32_t) {}
void WglSink::tex_parameterf(uint32_t, uint32_t, float) {}
void WglSink::tex_envi(uint32_t, uint32_t, int32_t) {}
void WglSink::tex_image2d(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, const uint8_t *) {}
void WglSink::vertex_pointer(int32_t, uint32_t, int32_t) {}
void WglSink::normal_pointer(uint32_t, int32_t) {}

#endif

// ------------------------------------------------------------------------------------------------ the startup self-check (S3.20)

std::string sha256_hex(const uint8_t *data, size_t n) {
  static const uint32_t K[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
      0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
      0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
  uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  std::vector<uint8_t> m(data, data + n);
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  const uint64_t bits = static_cast<uint64_t>(n) * 8u;
  for (int i = 7; i >= 0; --i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
  auto rr = [](uint32_t x, int k) { return (x >> k) | (x << (32 - k)); };
  for (size_t o = 0; o < m.size(); o += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = static_cast<uint32_t>(m[o + 4 * i]) << 24 | static_cast<uint32_t>(m[o + 4 * i + 1]) << 16 |
             static_cast<uint32_t>(m[o + 4 * i + 2]) << 8 | m[o + 4 * i + 3];
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 = rr(w[i - 15], 7) ^ rr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 = rr(w[i - 2], 17) ^ rr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const uint32_t t1 = hh + (rr(e, 6) ^ rr(e, 11) ^ rr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
      const uint32_t t2 = (rr(a, 2) ^ rr(a, 13) ^ rr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  static const char *hx = "0123456789abcdef";
  std::string s;
  for (uint32_t v : h)
    for (int k = 28; k >= 0; k -= 4) s += hx[(v >> k) & 15];
  return s;
}

namespace {

#include "gl_wgl_ref.inc"

float fbits(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
double dbits(uint32_t lo, uint32_t hi) {
  const uint64_t u = static_cast<uint64_t>(hi) << 32 | lo;
  double d;
  std::memcpy(&d, &u, 8);
  return d;
}

// The setup's first sink when the self-check feeds a WglSink through SetupPair: it only chooses the texture names, by the rule of
// the original's driver and of GlRaster::gen_textures (the lowest name not in use), so the WglSink maps them like the live path.
class NameSink : public GlSink {
 public:
  void context_create() override { ctx_ = true; }
  bool gen_textures(int32_t n, uint32_t *names) override {
    if (!ctx_) return false;
    for (int32_t i = 0; i < n; ++i) {
      uint32_t k = 1;
      while (used_.count(k)) ++k;
      used_.insert(k);
      names[i] = k;
    }
    return true;
  }
  void delete_textures(int32_t n, const uint32_t *names) override {
    if (!ctx_) return;
    for (int32_t i = 0; i < n; ++i) used_.erase(names[i]);
  }
  void load_identity() override {}
  void push_matrix() override {}
  void pop_matrix() override {}
  void rotated(double, double, double, double) override {}
  void rotatef(float, float, float, float) override {}
  void translatef(float, float, float) override {}
  void bind_texture(uint32_t, uint32_t) override {}
  void begin(uint32_t) override {}
  void end() override {}
  void tex_coord2fv(const float *) override {}
  void tex_coord2f(float, float) override {}
  void array_element(int32_t) override {}
  void normal3fv(const float *) override {}
  void vertex3fv(const float *) override {}
  void vertex3f(float, float, float) override {}
  void enable(uint32_t) override {}
  void disable(uint32_t) override {}
  void blend_func(uint32_t, uint32_t) override {}
  void flush() override {}

 private:
  bool ctx_ = false;
  std::set<uint32_t> used_;
};

// The reference pass (gl_wgl_ref.inc) into a sink whose setup is done: the recorded modelview calls, then the program, then the pop
// and flush the pass ends with. perturb 2 moves every texture coordinate by 1/64 in s (one texel of the 64-texel tile textures; moving
// only the first one is an equivalent mutant: its triangle's pixels in the rectangle are drawn over by later ones).
void ref_run(GlSink &g, int perturb) {
  const size_t nh = sizeof(kRefHead) / sizeof(kRefHead[0]);
  for (size_t i = 0; i < nh;) {
    switch (kRefHead[i]) {
      case 0: g.load_identity(); i += 1; break;
      case 1: g.push_matrix(); i += 1; break;
      case 2:
        g.rotated(dbits(kRefHead[i + 1], kRefHead[i + 2]), dbits(kRefHead[i + 3], kRefHead[i + 4]), dbits(kRefHead[i + 5], kRefHead[i + 6]),
                  dbits(kRefHead[i + 7], kRefHead[i + 8]));
        i += 9;
        break;
      case 3: g.rotatef(fbits(kRefHead[i + 1]), fbits(kRefHead[i + 2]), fbits(kRefHead[i + 3]), fbits(kRefHead[i + 4])); i += 5; break;
      case 4: g.translatef(fbits(kRefHead[i + 1]), fbits(kRefHead[i + 2]), fbits(kRefHead[i + 3])); i += 4; break;
      default: return;
    }
  }
  const size_t np = sizeof(kRefProgram) / sizeof(kRefProgram[0]);
  for (size_t i = 0; i < np;) {
    float v[3];
    switch (kRefProgram[i]) {
      case 1: g.bind_texture(kTexture2D, kRefProgram[i + 1]); i += 2; break;
      case 2: g.begin(kTriangles); i += 1; break;
      case 3: g.end(); i += 1; break;
      case 4:
      case 7:
        v[0] = fbits(kRefProgram[i + 1]);
        v[1] = fbits(kRefProgram[i + 2]);
        if (perturb == 2) v[0] += 1.0f / 64.0f;
        if (kRefProgram[i] == 4) g.tex_coord2fv(v);
        else g.tex_coord2f(v[0], v[1]);
        i += 3;
        break;
      case 5: g.array_element(static_cast<int32_t>(kRefProgram[i + 1])); i += 2; break;
      case 6:
      case 8:
        for (int k = 0; k < 3; ++k) v[k] = fbits(kRefProgram[i + 1 + k]);
        if (kRefProgram[i] == 6) g.normal3fv(v);
        else g.vertex3fv(v);
        i += 4;
        break;
      default: return;
    }
  }
  g.pop_matrix();
  g.flush();
}

std::vector<float> ref_floats(const uint32_t *u, size_t n) {
  std::vector<float> f(n);
  for (size_t i = 0; i < n; ++i) f[i] = fbits(u[i]);
  return f;
}

std::vector<uint16_t> cut_rect(const std::vector<uint16_t> &px, int w) {
  std::vector<uint16_t> r;
  if (px.size() < static_cast<size_t>(w) * static_cast<size_t>(kRefY + kRefH)) return r;
  for (int y = kRefY; y < kRefY + kRefH; ++y)
    for (int x = kRefX; x < kRefX + kRefW; ++x) r.push_back(px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]);
  return r;
}

void judge(SelfCheck &s, int perturb) {
  std::vector<uint8_t> b(s.rect.size() * 2);
  for (size_t i = 0; i < s.rect.size(); ++i) {
    b[2 * i] = static_cast<uint8_t>(s.rect[i] & 0xff);
    b[2 * i + 1] = static_cast<uint8_t>(s.rect[i] >> 8);
  }
  s.got = sha256_hex(b.data(), b.size());
  s.want = kRefSha256;
  if (perturb == 1) s.want[0] = s.want[0] == '0' ? '1' : '0';   // negative control: a wrong pinned hash
  s.pass = s.got == s.want;
  s.reason = s.pass ? "the reference pass equals the original's pixels (SHA-256 of the 32 x 32 rectangle)"
                    : "the reference pass differs from the original's pixels (SHA-256 " + s.got + ", want " + s.want + ")";
}

// The port's own GL setup the live path makes (LiveTerrain, terrain_dll.cpp; sg_gl_setup_to_raster): the Terrain constructor
// without a context, initSystem(800, 600, flip) with one, initTerrain's array pointers; then setZoomLevel for a zoomed reference.
void ref_setup(Ctx &ctx, GlSink &g) {
  TerrainGlState st;
  terrain_gl_construct(ctx, g, st);
  terrain_gl_init_system(ctx, g, st, 800, 600, true);
  terrain_gl_init_terrain(g);
  if constexpr (kRefZoom == 1) terrain_gl_set_zoom_level(g, st, 2);
  else if constexpr (kRefZoom == 2) terrain_gl_set_zoom_level(g, st, 1);
}

template <class F>
bool guarded(SelfCheck &s, F &&f) {
  try {
    f();
    return true;
  } catch (const MissingResource &e) {
    s.reason = std::string("the reference setup is missing a resource: ") + (e.what ? e.what : "?");
  } catch (const Fault &e) {
    s.reason = "the reference setup faulted at va " + std::to_string(e.va);
  } catch (...) {
    s.reason = "the reference setup threw";
  }
  return false;
}

}  // namespace

SelfCheck wgl_self_check(Ctx &ctx, int variant, int perturb) {
  SelfCheck s;
  const auto t0 = std::chrono::steady_clock::now();
  WglSink w(800, 600, variant);
  if (!w.ok()) {
    s.reason = "no host OpenGL: " + w.why();
  } else {
    NameSink names;
    SetupPair pair(&names, &w);
    const std::vector<float> v = ref_floats(kRefVertices, sizeof(kRefVertices) / 4), n = ref_floats(kRefNormals, sizeof(kRefNormals) / 4);
    if (guarded(s, [&] { ref_setup(ctx, pair); })) {
      if (!w.has_context() || !w.ok()) {
        s.reason = "no host OpenGL context: " + w.why();
      } else {
        w.set_pixels(std::vector<uint16_t>(static_cast<size_t>(800) * 600, 0));
        w.set_arrays(v.data(), n.data());
        ref_run(w, perturb);
        std::vector<uint16_t> px;
        w.get_pixels(px);
        s.rect = cut_rect(px, 800);
        judge(s, perturb);
      }
    }
  }
  s.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return s;
}

SelfCheck raster_self_check(Ctx &ctx) {
  SelfCheck s;
  const auto t0 = std::chrono::steady_clock::now();
  GlRaster r(800, 600);
  r.no_context();
  const std::vector<float> v = ref_floats(kRefVertices, sizeof(kRefVertices) / 4), n = ref_floats(kRefNormals, sizeof(kRefNormals) / 4);
  if (guarded(s, [&] { ref_setup(ctx, r); })) {
    r.pixels().assign(static_cast<size_t>(800) * 600, 0);
    r.set_arrays(v.data(), v.size() / 3, n.data(), n.size() / 3);
    ref_run(r, 0);
    s.rect = cut_rect(r.pixels(), 800);
    judge(s, 0);
  }
  s.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return s;
}

}  // namespace gl

namespace {
// Harness entries (roadmap S3.20): glWgl.selfCheck(variant, perturb) runs the startup self-check (1 = it passed) and writes
// `$wgl/selfcheck.txt` (pass, reason, got, want, ms) and `$wgl/selfcheck_rect.555` (the rectangle drawn); glWgl.rasterCheck() the
// same reference through GlRaster (`$wgl/raster_rect.555`).
void put(Ctx &c, const std::string &name, const std::string &text) {
  GameFiles &f = c.files();
  if (auto *d = dynamic_cast<DirGameFiles *>(&f))
    if (d->write_through()) return;
  f.write(name, std::vector<uint8_t>(text.begin(), text.end()));
}
void put_rect(Ctx &c, const std::string &name, const std::vector<uint16_t> &px) {
  std::string b(px.size() * 2, '\0');
  for (size_t i = 0; i < px.size(); ++i) {
    b[2 * i] = static_cast<char>(px[i] & 0xff);
    b[2 * i + 1] = static_cast<char>(px[i] >> 8);
  }
  put(c, name, b);
}
std::string text_of(const gl::SelfCheck &s) {
  char ms[32];
  std::snprintf(ms, sizeof ms, "%.1f", s.ms);
  return std::string("pass ") + (s.pass ? "1" : "0") + "\nreason " + s.reason + "\ngot " + s.got + "\nwant " + s.want + "\nms " + ms + "\n";
}
void a_self_check(Ctx &c, const uint32_t *a, Ret &r) {
  const gl::SelfCheck s = gl::wgl_self_check(c, static_cast<int>(a[0]), static_cast<int>(a[1]));
  put(c, "$wgl/selfcheck.txt", text_of(s));
  put_rect(c, "$wgl/selfcheck_rect.555", s.rect);
  r.set_u32(s.pass ? 1u : 0u);
}
void a_raster_check(Ctx &c, const uint32_t *, Ret &r) {
  const gl::SelfCheck s = gl::raster_self_check(c);
  put(c, "$wgl/rastercheck.txt", text_of(s));
  put_rect(c, "$wgl/raster_rect.555", s.rect);
  r.set_u32(s.pass ? 1u : 0u);
}
const FuncDef kFuncs[] = {
    {"glWgl.selfCheck", "default", "int", 2, 0, a_self_check, nullptr},
    {"glWgl.rasterCheck", "default", "int", 0, 0, a_raster_check, nullptr},
};
}  // namespace

extern const ModuleDef kModule_gl_wgl = {"gl_wgl", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
