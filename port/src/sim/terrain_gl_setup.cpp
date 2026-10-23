// The GL setup of Terrain.dll produced by the port (see portsim/sim/terrain_gl_setup.h). Written from the specification; the matched
// sources named in the header are reading reference, no original address or code appears here.
#include "portsim/sim/terrain_gl_setup.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"

namespace portsim {

TerrainGlState::TerrainGlState() : textures(static_cast<size_t>(kTypes) * kVariations * kSlots, 0u) {}

namespace {

constexpr uint32_t kTexture2D = 0x0DE1, kTexture1D = 0x0DE0;
constexpr uint32_t kRgb = 0x1907, kRgba = 0x1908, kUnsignedByte = 0x1401;
constexpr uint32_t kMagFilter = 0x2800, kMinFilter = 0x2801, kWrapS = 0x2802, kWrapT = 0x2803, kEnvMode = 0x2200;
constexpr uint32_t kNearest = 0x2600, kLinear = 0x2601, kReplace = 0x1E01;
constexpr uint32_t kUninitialised = 0xCCCCCCCCu;   // the debug build's stack fill: the value of a local nothing wrote

// ---- file access: the original's paths are "./Data/Textures/<course>/..." with doubled slashes; GameFiles names are normalised ----
std::string normal_name(std::string p) {
  for (char &c : p)
    if (c == '\\') c = '/';
  while (p.rfind("./", 0) == 0) p.erase(0, 2);
  for (size_t i; (i = p.find("//")) != std::string::npos;) p.erase(i, 1);
  return p;
}

std::optional<std::vector<uint8_t>> read_all(GameFiles &f, const std::string &name) {
  const std::string n = normal_name(name);
  const int64_t size = f.size(n);
  if (size < 0) return std::nullopt;
  std::vector<uint8_t> b(static_cast<size_t>(size));
  if (size > 0 && f.read(n, 0, b.data(), static_cast<uint64_t>(size)) != size) return std::nullopt;
  return b;
}

uint32_t le32(const uint8_t *p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t le16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

// LoadDIBitmap: the file header, the info block (up to bfOffBits), then biSizeImage bytes (or the computed size); any short read fails.
struct Dib {
  int32_t width = 0, height = 0;
  std::vector<uint8_t> bits;
};
std::optional<Dib> load_dib(GameFiles &f, const std::string &name) {
  const auto file = read_all(f, name);
  if (!file) return std::nullopt;
  const std::vector<uint8_t> &b = *file;
  if (b.size() < 14) return std::nullopt;
  if (le16(&b[0]) != 0x4D42u) return std::nullopt;                      // 'BM'
  const int32_t off_bits = static_cast<int32_t>(le32(&b[10]));
  const int32_t info_size = off_bits - 14;
  if (info_size < 0 || static_cast<size_t>(14 + info_size) > b.size() || info_size < 24) return std::nullopt;
  Dib d;
  d.width = static_cast<int32_t>(le32(&b[14 + 4]));
  d.height = static_cast<int32_t>(le32(&b[14 + 8]));
  const uint16_t bit_count = le16(&b[14 + 14]);
  int32_t bitsize = static_cast<int32_t>(le32(&b[14 + 20]));              // biSizeImage
  if (bitsize == 0) bitsize = (d.width * bit_count + 7) / 8 * std::abs(d.height);
  if (bitsize < 0 || static_cast<size_t>(14 + info_size) + static_cast<size_t>(bitsize) > b.size()) return std::nullopt;
  d.bits.assign(b.begin() + 14 + info_size, b.begin() + 14 + info_size + bitsize);
  return d;
}

// textureLoad(file, alpha, minfilter, magfilter, wrap): a .bmp as a texture; 0 on failure. The red and blue bytes are swapped over
// width * height pixels of three bytes; the parameters are set the way the original sets them (NEAREST magnification, LINEAR
// minification whatever the arguments say, glTexEnvi called with the texture target, which the driver rejects).
uint32_t texture_load(Ctx &ctx, gl::GlSink &gl, const std::string &file, uint32_t minfilter, uint32_t wrap) {
  auto dib = load_dib(ctx.files(), file);
  if (!dib) return 0;
  const int64_t n = static_cast<int64_t>(dib->width) * dib->height;
  for (int64_t i = 0, q = 0; i < n && q + 2 < static_cast<int64_t>(dib->bits.size()); ++i, q += 3) std::swap(dib->bits[static_cast<size_t>(q)], dib->bits[static_cast<size_t>(q) + 2]);
  const uint32_t type = dib->height == 1 ? kTexture1D : kTexture2D;
  uint32_t texture = kUninitialised;
  gl.gen_textures(1, &texture);
  gl.bind_texture(type, texture);
  gl.tex_parameteri(type, kMagFilter, static_cast<int32_t>(kNearest));
  gl.tex_parameteri(type, kMinFilter, static_cast<int32_t>(kLinear));
  gl.tex_parameteri(type, kWrapS, static_cast<int32_t>(wrap));
  gl.tex_parameteri(type, kWrapT, static_cast<int32_t>(wrap));
  gl.tex_envi(type, kEnvMode, static_cast<int32_t>(kReplace));
  if (minfilter == kLinear || minfilter == kNearest) {
    gl.tex_image2d(type, 0, 3, dib->width, dib->height, 0, kRgb, kUnsignedByte, dib->bits.data());
  } else {
    throw MissingResource{"mip-mapped texture load (gluBuild*Mipmaps): not used by the original's call sites"};
  }
  return texture;
}

// LoadTGA(file): an uncompressed 24 / 32 bit .tga; 0 on failure. Parameters are set with glTexParameterf (the filters as floats).
uint32_t texture_load_tga(Ctx &ctx, gl::GlSink &gl, const std::string &file) {
  const auto f = read_all(ctx.files(), file);
  if (!f) return 0;
  const std::vector<uint8_t> &b = *f;
  static const uint8_t kHeader[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  if (b.size() < 18 || std::memcmp(b.data(), kHeader, 12) != 0) return 0;
  const uint8_t *h = &b[12];
  const uint32_t width = static_cast<uint32_t>(h[1]) * 256 + h[0], height = static_cast<uint32_t>(h[3]) * 256 + h[2];
  if (width == 0 || height == 0 || (h[4] != 24 && h[4] != 32)) return 0;
  const uint32_t bpp = h[4] / 8u, size = width * height * bpp;
  if (b.size() < 18 + static_cast<size_t>(size)) return 0;
  std::vector<uint8_t> img(b.begin() + 18, b.begin() + 18 + size);
  for (uint32_t i = 0; i < size; i += bpp) std::swap(img[i], img[i + 2]);
  uint32_t texture = kUninitialised;
  gl.gen_textures(1, &texture);
  gl.bind_texture(kTexture2D, texture);
  gl.tex_parameterf(kTexture2D, kMinFilter, static_cast<float>(kLinear));
  gl.tex_parameterf(kTexture2D, kMagFilter, static_cast<float>(kLinear));
  if (bpp == 3) gl.tex_image2d(kTexture2D, 0, static_cast<int32_t>(kRgb), static_cast<int32_t>(width), static_cast<int32_t>(height), 0, kRgb, kUnsignedByte, img.data());
  else gl.tex_image2d(kTexture2D, 0, static_cast<int32_t>(kRgba), static_cast<int32_t>(width), static_cast<int32_t>(height), 0, kRgba, kUnsignedByte, img.data());
  return texture;
}

uint32_t &slot(TerrainGlState &st, int type, int variation, int piece) {
  const size_t i = (static_cast<size_t>(type) * TerrainGlState::kVariations + static_cast<size_t>(variation)) * TerrainGlState::kSlots + static_cast<size_t>(piece);
  if (i >= st.textures.size()) throw Fault{static_cast<uint32_t>(i), 4};
  return st.textures[i];
}

// Terrain::reloadTextures: every name of the table is deleted (nine at a time) and cleared, the variation counts reset.
void reload_textures(gl::GlSink &gl, TerrainGlState &st) {
  if (!st.textures_loaded) return;
  for (int t = 0; t < TerrainGlState::kTypes; ++t) {
    for (int v = 0; v < TerrainGlState::kVariations; ++v) {
      uint32_t *row = &slot(st, t, v, 0);
      gl.delete_textures(9, row);
      for (int k = 0; k < 9; ++k) row[k] = 0;
    }
    st.type_count[t] = 0;
  }
  st.textures_loaded = false;
}

// Terrain::relight: (reload, then) load the course's texture sets.
void relight(Ctx &ctx, gl::GlSink &gl, TerrainGlState &st) {
  if (st.textures_loaded) reload_textures(gl, st);
  std::string base = "./Data/Textures/";
  switch (st.course_type) {
    case 2: base += "Tropical/"; break;
    case 1: base += "Desert/"; break;
    case 3: base += "Links/"; break;
    default: base += "Parkland/"; break;
  }
  int count = 0;
  for (int i = 0; i < 0x1f; ++i) {
    if (i == 0) {
      std::string path = base + st.type_name[i] + "A0001.bmp";
      for (char c1 = '1'; c1 < ':'; ++c1) {
        path[path.size() - 5] = c1;
        slot(st, 0, count++, 0) = texture_load(ctx, gl, path, kLinear, 0x2901);
      }
      for (char c2 = '1'; c2 < '3'; ++c2) {
        path[path.size() - 6] = c2;
        for (char c3 = '0'; c3 < ':'; ++c3) {
          if (count > 0x18) continue;
          path[path.size() - 5] = c3;
          slot(st, 0, count++, 0) = texture_load(ctx, gl, path, kLinear, 0x2901);
        }
      }
    } else {   // the original tests an array address against 0 (always true): unnamed types reach the loop and stop at the first failed load
      for (char letter = 'A'; letter < 'F'; ++letter) {
        std::string path = base + st.type_name[i] + letter + "0001.bmp";
        uint32_t tex = texture_load(ctx, gl, path, kLinear, 0x2901);
        if (tex) {
          slot(st, i, st.type_count[i], 0) = tex;
          for (char d = '2'; d < ':'; ++d) {
            path[path.size() - 5] = d;
            tex = texture_load(ctx, gl, path, kLinear, 0x2901);
            slot(st, i, st.type_count[i], d - '1') = tex;
          }
          st.type_count[i]++;
        } else {
          break;
        }
      }
    }
  }
  static const char *const kPath[4] = {"/Path.tga", "/PathCap.tga", "/PathInside.tga", "/PathCurve.tga"};
  static const char *const kPathX[4] = {"/PathX.tga", "/PathCapX.tga", "/PathInsideX.tga", "/PathCurveX.tga"};
  for (int k = 0; k < 4; ++k) slot(st, 0x20, 0, k) = texture_load_tga(ctx, gl, base + kPath[k]);
  st.type_count[0x20]++;
  for (int k = 0; k < 4; ++k) slot(st, 0x21, 0, k) = texture_load_tga(ctx, gl, base + kPathX[k]);
  st.type_count[0x21]++;
  slot(st, 0x23, 0, 0) = texture_load(ctx, gl, base + "/CliffTest.bmp", kLinear, 0x2901);
  st.type_count[0x23]++;
  slot(st, 0x22, 0, 0) = texture_load(ctx, gl, base + "/RetainWallA.bmp", kLinear, 0x2901);
  st.type_count[0x22]++;
  slot(st, 0x24, 0, 0) = texture_load(ctx, gl, base + "/strata.bmp", kLinear, 0x2901);
  st.type_count[0x24]++;
  st.textures_loaded = true;
}

// ---- the lighting file ----
float deg_to_rad(float d) { return d * 0.017453292f; }

// rotates v about one axis; the second component uses the already rotated first one, as in the original
void rotate_axis(float deg, float ax, float ay, float az, float *v) {
  const float rad = deg_to_rad(deg);
  const float c = static_cast<float>(std::cos(static_cast<double>(rad)));
  const float s = static_cast<float>(std::sin(static_cast<double>(rad)));
  if (ax != 0.0f && ay == 0.0f && az == 0.0f) {
    v[1] = static_cast<float>(static_cast<double>(c) * v[1] + static_cast<double>(-s) * v[2]);
    v[2] = static_cast<float>(static_cast<double>(s) * v[1] + static_cast<double>(c) * v[2]);
  } else if (ax == 0.0f && ay != 0.0f && az == 0.0f) {
    v[0] = static_cast<float>(static_cast<double>(c) * v[0] + static_cast<double>(s) * v[2]);
    v[2] = static_cast<float>(static_cast<double>(-s) * v[0] + static_cast<double>(c) * v[2]);
  } else if (ax == 0.0f && ay == 0.0f && az != 0.0f) {
    v[0] = static_cast<float>(static_cast<double>(c) * v[0] + static_cast<double>(-s) * v[1]);
    v[1] = static_cast<float>(static_cast<double>(s) * v[0] + static_cast<double>(c) * v[1]);
  }
}

void normalize3(float *v) {
  const float len = static_cast<float>(std::sqrt(static_cast<double>(v[0]) * v[0] + static_cast<double>(v[1]) * v[1] + static_cast<double>(v[2]) * v[2]));
  v[0] = v[0] / len;
  v[1] = v[1] / len;
  v[2] = v[2] / len;
}

// Terrain::loadLighting(file): the three colour lines after #AMBIENT, #DIFFUSE and #SPECULAR (integers / 255), light 0 aimed from
// (-0.5, 0.1, -1, 0) rotated 40 degrees about x and 45 about y, normalised, and a white specular material of shininess 13.
// The colour arrays are float[3] but glLightfv reads four: on the original's stack they lie specular, diffuse, ambient, so the
// fourth float of each is the first of the next array, and the fourth of the ambient is whatever lies above (here 0).
void load_lighting(Ctx &ctx, gl::GlSink &gl, const std::string &file) {
  const auto f = read_all(ctx.files(), file);
  if (!f) return;
  std::vector<std::string> lines;   // old iostream text mode: CR LF is read as a line end
  {
    std::string cur;
    for (uint8_t ch : *f) {
      if (ch == '\n') { lines.push_back(cur); cur.clear(); }
      else if (ch != '\r') cur += static_cast<char>(ch);
    }
    if (!cur.empty()) lines.push_back(cur);
  }
  size_t at = 0;
  auto first_token = [](const std::string &l) {
    const size_t a = l.find_first_not_of(' ');
    if (a == std::string::npos) throw Fault{0, 0};   // strtok returns NULL and the original's strcmp dereferences it
    return l.substr(a, l.find(' ', a) - a);
  };
  auto find_header = [&](const char *name) {
    for (;;) {
      if (at >= lines.size()) throw NoReturn{"loadLighting: header not found"};
      if (first_token(lines[at++]) == name) return;
    }
  };
  auto three = [&]() {
    if (at >= lines.size()) throw NoReturn{"loadLighting: missing colour line"};
    const std::string &l = lines[at++];
    float out[3];
    size_t p = 0;
    for (int i = 0; i < 3; ++i) {
      const size_t a = l.find_first_not_of(' ', p);
      if (a == std::string::npos) throw Fault{0, 0};
      const size_t e = l.find(' ', a);
      out[i] = static_cast<float>(std::atoi(l.substr(a, e == std::string::npos ? std::string::npos : e - a).c_str())) / 255.0f;
      p = e == std::string::npos ? l.size() : e;
    }
    return std::vector<float>{out[0], out[1], out[2]};
  };
  find_header("#AMBIENT");
  const auto amb = three();
  find_header("#DIFFUSE");
  const auto dif = three();
  find_header("#SPECULAR");
  const auto spe = three();
  float pos[4] = {-0.5f, 0.1f, -1.0f, 0.0f};
  rotate_axis(40.0f, 1.0f, 0.0f, 0.0f, pos);
  rotate_axis(45.0f, 0.0f, 1.0f, 0.0f, pos);
  normalize3(pos);
  const float stack[10] = {spe[0], spe[1], spe[2], dif[0], dif[1], dif[2], amb[0], amb[1], amb[2], 0.0f};   // specular, diffuse, ambient, (above)
  const float mat_specular[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gl.light(0x4000, 0x1203, pos);
  gl.light(0x4000, 0x1200, &stack[6]);
  gl.light(0x4000, 0x1201, &stack[3]);
  gl.light(0x4000, 0x1202, &stack[0]);
  gl.enable(0x4000);
  gl.material(0x404, 0x1202, mat_specular);
  gl.material_shininess(0x404, 13);
}

const char *lighting_file(int32_t course_type) {
  switch (course_type) {
    case 0: return "ParklandLighting.txt";
    case 2: return "TropicalLighting.txt";
    case 1: return "DesertLighting.txt";
    case 3: return "LinksLighting.txt";
    default: return "lighting.txt";
  }
}

// Terrain::initGL
void init_gl(gl::GlSink &gl, bool flip_y) {
  gl.hint(0x0C50, 0x1101);          // GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST
  gl.enable(0x0B20);                // GL_LINE_SMOOTH
  gl.hint(0x0C52, 0x1102);          // GL_LINE_SMOOTH_HINT, GL_NICEST
  gl.enable(0x0DE1);                // GL_TEXTURE_2D
  gl.enable(0x0B50);                // GL_LIGHTING
  gl.front_face(flip_y ? 0x0900u : 0x0901u);   // GL_CW / GL_CCW
  gl.enable(0x0B44);                // GL_CULL_FACE
  gl.enable_client_state(0x8074);   // GL_VERTEX_ARRAY
  gl.enable_client_state(0x8075);   // GL_NORMAL_ARRAY
}

// Terrain::resize(width, height): viewport and the orthographic projection (three window sizes have their own extents)
void resize(gl::GlSink &gl, bool flip_y, int32_t width, int32_t height) {
  if (height == 0) height = 1;
  if (width == 0) width = 1;
  gl.viewport(0, 0, width, height);
  gl.matrix_mode(0x1701);
  gl.load_identity();
  if (width == 800 && height == 600) { width = 1767; height = 1325; }
  else if (width == 1024 && height == 768) { width = 1810; height = 1303; }
  else if (width == 1280 && height == 1024) { width = 1740; height = 1392; }
  const double l = static_cast<double>((-width) >> 1), r = static_cast<double>(width >> 1);
  const double b = static_cast<double>((-height) >> 1), t = static_cast<double>(height >> 1);
  if (flip_y) gl.ortho(l, r, t, b, 5000.0, -5000.0);
  else gl.ortho(l, r, b, t, 5000.0, -5000.0);
  gl.matrix_mode(0x1700);
  gl.load_identity();
}

}  // namespace

void terrain_gl_construct(Ctx &ctx, gl::GlSink &gl, TerrainGlState &st) {
  st.course_type = 0;
  st.textures_loaded = false;
  for (int i = 0; i < TerrainGlState::kTypes; ++i) st.type_count[i] = 0;
  std::fill(st.textures.begin(), st.textures.end(), 0u);
  static const struct { int i; const char *name; } kNames[] = {
      {0, "Tee"}, {1, "PuttingGreen"}, {2, "Fairway"}, {3, "FirmFairway"}, {4, "Rough"}, {5, "DeepRough"}, {8, "GrassySand"},
      {9, "PotSandBunker"}, {10, "Overgrowth"}, {11, "Brush"}, {12, "Rock"}, {13, "Woods"}, {17, "WaterShallow"}, {18, "Marsh"},
      {19, "Overgrowth"}, {22, "Building"}, {24, "WaterDeep"}, {23, "WaterMiddle"}, {25, "WaterShallowDesert"}, {26, "TrickyGreen"},
      {27, "SandBunker1"}, {28, "SandBunker2"}, {29, "SandBunker3"}, {30, "SandBunker4"}};
  for (const auto &n : kNames) st.type_name[n.i] = n.name;
  relight(ctx, gl, st);
}

void terrain_gl_init_system(Ctx &ctx, gl::GlSink &gl, TerrainGlState &st, int32_t width, int32_t height, bool flip_y) {
  if (st.has_context) return;
  gl.context_create();
  st.has_context = true;
  st.flip_y = flip_y;
  init_gl(gl, flip_y);
  load_lighting(ctx, gl, lighting_file(st.course_type));   // initLighting -> rebuild
  relight(ctx, gl, st);
  resize(gl, flip_y, width, height);
  st.screen_w = width;
  st.screen_h = height;
  if (width == 800 && height == 600) st.screen_scale = 38.682186f;
  else if (width == 1024 && height == 768) st.screen_scale = 40.541603f;
  else if (width == 1280 && height == 1024) st.screen_scale = 40.83222f;
}

void terrain_gl_set_zoom_level(gl::GlSink &gl, TerrainGlState &st, int32_t level) {
  float w = 0.0f, h = 0.0f;   // any window size other than the three below leaves them at the debug build's stack fill
  gl.matrix_mode(0x1701);
  gl.load_identity();
  if (st.screen_w == 800 && st.screen_h == 600) { w = 1767.767f; h = 1325.8252f; }
  else if (st.screen_w == 1024 && st.screen_h == 768) { w = 1810.1934f; h = 1357.645f; }
  else if (st.screen_w == 1280 && st.screen_h == 1024) { w = 1740.5706f; h = 1392.4564f; }
  switch (level) {
    case 4:
      resize(gl, st.flip_y, st.screen_w, st.screen_h);
      st.zoom = 0;
      break;
    case 2:
      w *= 2.0f;
      h *= 2.0f;
      st.zoom = 1;
      break;
    case 1:
      w *= 4.0f;
      h *= 4.0f;
      st.zoom = 2;
      break;
    default: break;
  }
  if (st.flip_y) gl.ortho(static_cast<double>(-w * 0.5f), static_cast<double>(w * 0.5f), static_cast<double>(h * 0.5f), static_cast<double>(-h * 0.5f), 5000.0, -5000.0);
  else gl.ortho(static_cast<double>(-w * 0.5f), static_cast<double>(w * 0.5f), static_cast<double>(-h * 0.5f), static_cast<double>(h * 0.5f), 5000.0, -5000.0);
  gl.matrix_mode(0x1700);
}

// Terrain::loadNewCourseType(type): nothing unless the course type changes; then rebuild (the course's lighting file), reloadTextures
// and relight (re/match/terrain_methods2.cpp).
void terrain_gl_load_new_course_type(Ctx &ctx, gl::GlSink &gl, TerrainGlState &st, int32_t type) {
  if (st.course_type == type) return;
  st.course_type = type;
  load_lighting(ctx, gl, lighting_file(st.course_type));
  reload_textures(gl, st);
  relight(ctx, gl, st);
}

void terrain_gl_init_terrain(gl::GlSink &gl) {
  gl.vertex_pointer(3, 0x1406, 0);   // GL_FLOAT
  gl.normal_pointer(0x1406, 0);
}

extern const ModuleDef kModule_terrain_gl_setup = {"terrain_gl_setup", nullptr, 0};

}  // namespace portsim
