// The state side of the port's Terrain.dll (phase 5 step 3; plan re/PHASE5_TERRAIN_PLAN.md section 13): construction, the tile
// grid and its faces, the vertex / normal arrays, normals and face textures. Layout and conventions: terrain_state.h. Tests:
// port/test/test_terrain_state.py, which compares with snapshots of the ORIGINAL's Terrain object and arrays (re/tools/gl_corpus.py).
#define _CRT_SECURE_NO_WARNINGS   // std::getenv / std::fopen of the development switch SG_TERRAIN_STATE_LOG
#include "portsim/sim/terrain_state.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>   // QueryThreadCycleTime of the development switch SG_TERRAIN_STATE_CYCLES
#endif

#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/sim/terrain_gl_setup.h"

namespace portsim {

namespace {

// Negative controls (deliberately wrong bodies; 0 = the correct body):
//   1 Faces::build skips the shared-edge copies between faces 2 / 3 and 4 / 5
//   2 initGrid places every tile one column to the right
//   3 buildArrays swaps the x - 1 and x + 1 neighbour links
//   4 faceNormal takes the cross product the other way round (b x a)
//   5 calcNormals leaves face 7 out of the centre vertex's sum
//   6 smoothNormals (0, 3) leaves out the diagonal tile's faces
//   7 normalize sums the squares in float instead of the x87's double
//   8 flagCode weighs the second neighbour 11 instead of 10
//   9 setFaceTexture sends face argument 1 to face slot 2 (and 0 to 3)
//  10 edgeKind's water arm returns 1 where it returns 2
//  11 Tile::reset's default type is 5 instead of 4
//  12 elevateCorner keeps the centre's min instead of its max      13 leaveType6 subtracts on its one += corner too
//  14 rand() adds 2531013                                          15 setPathFlagForType leaves type 0x16 out
//  16 lowerCorner's type-7 offset 12.0 instead of 13.0             17 setTypeId skips leaveType7
//  18 enterType7 dirties every neighbour                           19 setWall stores `on != 0` instead of its low byte
//  20 layPath ignores its direction flag                           21 setViewAngle maps 90 to quadrant 2
//  22 the exported resetTerrain does nothing
//  23 (S3.10b) the object view's one-byte stores are dropped (judges that the replays run through the view)
int32_t g_negctl = 0;

// ---- the object view (roadmap S3.10b, U-1388) ----
// Every access of this file goes through VPtr, which has Ptr's interface and semantics. While an exported method runs
// (terrain_dll_call), the Terrain object is resolved ONCE through Memory::at over its whole extent and the accesses that lie inside
// it read and write the host bytes directly; any other access (an address outside the object, one that straddles its end, or no
// view at all) goes to Memory as before and faults where it faulted. Equal results: Memory::at returns, for any sub-range of a range
// it resolved in one piece, the same host bytes at the same offset (the state and the mappings never overlap, so the sub-range
// resolves to the same mapping), and no mapping changes while a method runs. The object's mapping is writable (the view is made
// with write = true; a read-only one gets no view).
struct ObjView {
  const Memory *mem = nullptr;
  uint32_t base = 0, size = 0;
  uint8_t *host = nullptr;
};
thread_local const ObjView *t_view = nullptr;

template <class T> inline uint8_t *view_at(const Memory &m, uint32_t va) {
  const ObjView *v = t_view;
  if (v && v->mem == &m) {
    const uint32_t rel = va - v->base;
    if (rel < v->size && sizeof(T) <= v->size - rel) return v->host + rel;
  }
  return nullptr;
}
template <class T> inline T vload(const Memory &m, uint32_t va) {
  if (const uint8_t *p = view_at<T>(m, va)) {
    T x;
    std::memcpy(&x, p, sizeof(T));
    return x;
  }
  return m.load<T>(va);
}
template <class T> inline void vstore(const Memory &m, uint32_t va, const T &x) {
  if (uint8_t *p = view_at<T>(m, va)) {
    if constexpr (sizeof(T) == 1) {
      if (g_negctl == 23) return;
    }
    std::memcpy(p, &x, sizeof(T));
    return;
  }
  m.store<T>(va, x);
}

// Ptr's interface (the members this file uses) over the view. A VPtr made from an address looks the view up once and keeps the host
// bytes from there to the view's end (`span`); a VPtr derived from it (field, +, at, byte) moves inside that span without another
// lookup, and one that leaves the span, or was never in the view, takes vload / vstore (the view's own test, then Memory). So an
// access reads and writes exactly the bytes vload / vstore would.
template <class T> class VPtr {
 public:
  VPtr(const Memory &m, uint32_t va) : mem_(&m), va_(va) {
    const ObjView *v = t_view;
    if (v && v->mem == &m) {
      const uint32_t rel = va - v->base;
      if (rel < v->size) {
        host_ = v->host + rel;
        span_ = v->size - rel;
      }
    }
  }
  uint32_t va() const { return va_; }
  VPtr operator+(int32_t n) const { return derive<T>(u32(n) * static_cast<uint32_t>(sizeof(T))); }
  VPtr at(int32_t i) const { return *this + i; }
  T get() const {
    if (host_ && sizeof(T) <= span_) {
      T x;
      std::memcpy(&x, host_, sizeof(T));
      return x;
    }
    return vload<T>(*mem_, va_);
  }
  void set(const T &v) const {
    if (host_ && sizeof(T) <= span_) {
      if constexpr (sizeof(T) == 1) {
        if (g_negctl == 23) return;
      }
      std::memcpy(host_, &v, sizeof(T));
      return;
    }
    vstore<T>(*mem_, va_, v);
  }
  template <class M, class S = T> VPtr<M> field(M S::*m) const {
    static_assert(std::is_same<S, T>::value, "a member of another struct");
    return derive<M>(detail::member_offset(m));
  }
  template <class M, size_t N, class S = T> VPtr<M> field(M (S::*m)[N]) const {
    static_assert(std::is_same<S, T>::value, "a member of another struct");
    return derive<M>(detail::member_offset(m));
  }
  VPtr<uint8_t> byte(int32_t off) const { return derive<uint8_t>(u32(off)); }
  template <class U> VPtr<U> as_at(uint32_t off) const { return derive<U>(off); }   // (U *)((char *)p + off), wrapping
  // the host bytes of [va, va + n) when all of them lie in the span, else nullptr (then use get / set): for reading or writing a
  // whole record in place, the same bytes get / set would touch
  uint8_t *host(uint32_t n) const { return host_ && n <= span_ ? host_ : nullptr; }

 private:
  template <class U> friend class VPtr;
  VPtr(const Memory &m, uint32_t va, uint8_t *host, uint32_t span) : mem_(&m), va_(va), host_(host), span_(span) {}
  // the address va + off (wrapping); its host bytes when off stays inside the span
  template <class U> VPtr<U> derive(uint32_t off) const {
    if (host_ && off < span_) return VPtr<U>(*mem_, va_ + off, host_ + off, span_ - off);
    return VPtr<U>(*mem_, va_ + off, nullptr, 0);
  }
  const Memory *mem_;
  uint32_t va_;
  uint8_t *host_ = nullptr;
  uint32_t span_ = 0;
};

// Installs the view of the object at `base` for the current scope (none when `base` is 0 or Memory cannot resolve the whole object
// writable: then every access takes Memory's path).
class ViewScope {
 public:
  ViewScope(const Memory &m, uint32_t base) : prev_(t_view) {
    if (base == 0) return;
    try {
      v_.host = m.at(base, kTerrainObjectSize, true);
    } catch (const Fault &) {
      return;
    }
    v_.mem = &m;
    v_.base = base;
    v_.size = kTerrainObjectSize;
    t_view = &v_;
  }
  ~ViewScope() { t_view = prev_; }
  ViewScope(const ViewScope &) = delete;
  ViewScope &operator=(const ViewScope &) = delete;

 private:
  ObjView v_;
  const ObjView *prev_;
};

using TileP = VPtr<TerrainTile>;
using FaceP = VPtr<TerrainFace>;

TileP tile_p(Ctx &c, uint32_t va) { return TileP(c.mem, va); }
VPtr<TerrainObjectHeader> hdr_p(Ctx &c, uint32_t terrain) { return VPtr<TerrainObjectHeader>(c.mem, terrain); }

int32_t t_type(const TileP &t) { return t.field(&TerrainTile::type).get(); }
int32_t t_rot(const TileP &t) { return t.field(&TerrainTile::rotation).get(); }
uint32_t t_nb(const TileP &t, int32_t side) { return (t.field(&TerrainTile::neighbour) + side).get(); }
int32_t f_v(const TileP &t, int32_t f, int32_t k) { return ((t.field(&TerrainTile::face) + f).field(&TerrainFace::v) + k).get(); }
float fnorm(const TileP &t, int32_t f, int32_t k) { return ((t.field(&TerrainTile::face) + f).field(&TerrainFace::normal) + k).get(); }
int32_t t_type(Ctx &c, uint32_t t) { return tile_p(c, t).field(&TerrainTile::type).get(); }

// the module arrays; an index the original never bounds-checks reads foreign data there: a fault here
float *vtx(TerrainGlobals &g, int32_t i) {
  if (static_cast<uint32_t>(i) >= g.vertices.size() / 3) throw Fault{static_cast<uint32_t>(i), 12};
  return &g.vertices[3u * static_cast<uint32_t>(i)];
}
float *nrm(TerrainGlobals &g, int32_t i) {
  if (static_cast<uint32_t>(i) >= g.normals.size() / 3) throw Fault{static_cast<uint32_t>(i), 12};
  return &g.normals[3u * static_cast<uint32_t>(i)];
}
uint32_t &tex_slot(TerrainGlobals &g, int32_t type, int32_t variation, int32_t piece) {
  const uint64_t i = (static_cast<uint64_t>(static_cast<uint32_t>(type)) * kTerrainTextureVariations + static_cast<uint32_t>(variation)) *
                         kTerrainTextureSlots + static_cast<uint32_t>(piece);
  if (i >= g.textures.size()) throw Fault{static_cast<uint32_t>(i), 4};
  return g.textures[static_cast<size_t>(i)];
}

float f32(double v) { return static_cast<float>(v); }

// normalize (Terrain.dll's static helper): length = (float) sqrt of the x87 sum of squares, then each component divided by it
void normalize3(float *v) {
  float len;
  if (g_negctl == 7) len = f32(std::sqrt(static_cast<double>(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])));
  else {
    const double s = static_cast<double>(v[0]) * v[0] + static_cast<double>(v[1]) * v[1] + static_cast<double>(v[2]) * v[2];
    len = f32(std::sqrt(s));
  }
  v[0] = f32(static_cast<double>(v[0]) / len);
  v[1] = f32(static_cast<double>(v[1]) / len);
  v[2] = f32(static_cast<double>(v[2]) / len);
}

// roundHalf: the texture-coordinate selector of a float coordinate (0.5 -> 2, else truncated)
int32_t round_half(float x) { return x == 0.5f ? 2 : static_cast<int32_t>(x); }

// isSolidType: every type but 0 and 9 blends
bool is_solid_type(int32_t type) { return type != 9 && type != 0; }
// isOpenType: types 0, 1, 2, 3, 7, 8, 9
bool is_open_type(int32_t type) { return type == 2 || type == 7 || type == 1 || type == 0 || type == 9 || type == 8 || type == 3; }

// flagCode: three 0 / 1 / 2 edge kinds packed (4 / 40, 1 / 10, 2 / 20)
int32_t flag_code(int32_t a, int32_t b, int32_t c) {
  int32_t r = 0;
  if (a == 1) r += 4;
  else if (a == 2) r += 0x28;
  if (b == 1) r += 1;
  else if (b == 2) r += (g_negctl == 8 ? 11 : 10);
  if (c == 1) r += 2;
  else if (c == 2) r += 0x14;
  return r;
}

// idClass: the texture slot (piece) of a flag code
int32_t id_class(int32_t code) {
  switch (code) {
    case 0: case 1: case 2: case 10: case 0xc: case 0x14: case 0x15: return 0;
    case 3: case 0x2b: return 2;
    case 4: case 0xe: case 0x18: case 0x22: return 4;
    case 5: case 7: case 0x19: return 3;
    case 6: case 0x10: return 1;
    case 0x28: case 0x29: case 0x2a: case 0x3d: return 8;
    case 0x32: case 0x34: case 0x46: return 7;
    case 0x3c: return 5;
    case 0x1e: return 6;
    default: return 0;
  }
}

int32_t collar_of(const TerrainGlobals &g, int32_t type) {
  // g_collarInfo is a heap block of the n ints the exe passed: NULL before passCollarInfo (the original faults), and an index past
  // n reads the debug heap's guard bytes and the next block, which the port does not model (a fault here)
  if (!g.collar_set) throw Fault{0, 4};
  if (static_cast<uint32_t>(type) >= g.collar.size()) throw Fault{static_cast<uint32_t>(type), 4};
  return g.collar[static_cast<size_t>(type)];
}

}  // namespace

void terrain_state_negctl(int32_t mode) { g_negctl = mode; }

// ------------------------------------------------------------------------------------------------- Terrain.dll constants

namespace {
struct DllTable {
  const char *name;
  uint32_t rva, size;
  uint64_t fnv;
};
// the user's Terrain.dll (the anchored file of re/anchors.json): .data, file offset = RVA, no base relocation inside either table
const DllTable kSideNormals = {"side_normals", 0x63c40u, 96u, 0x8d2173301ec05d75ull};
const DllTable kTexCoords = {"tex_coords", 0x63ca0u, 368u, 0x8990157bf2dc3345ull};

uint64_t fnv1a64(const uint8_t *p, size_t n) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 0x100000001b3ull;
  return h;
}
uint32_t le32_at(const std::vector<uint8_t> &f, size_t off) {
  if (off + 4 > f.size()) throw MissingResource{"Terrain.dll: truncated PE headers"};
  return static_cast<uint32_t>(f[off]) | (static_cast<uint32_t>(f[off + 1]) << 8) | (static_cast<uint32_t>(f[off + 2]) << 16) |
         (static_cast<uint32_t>(f[off + 3]) << 24);
}
// the file offset of an RVA through the section table (the section whose raw data holds it)
size_t rva_to_offset(const std::vector<uint8_t> &f, uint32_t rva) {
  const uint32_t pe = le32_at(f, 0x3c);
  if (le32_at(f, pe) != 0x00004550u) throw MissingResource{"Terrain.dll: not a PE file"};
  const uint32_t nsec = le32_at(f, pe + 4) >> 16;              // FileHeader.NumberOfSections (high half of the dword at +4)
  const uint32_t optsz = le32_at(f, pe + 20) & 0xffffu;        // FileHeader.SizeOfOptionalHeader
  const size_t sec = pe + 24 + optsz;
  for (uint32_t i = 0; i < nsec; ++i) {
    const size_t s = sec + 40u * i;
    const uint32_t va = le32_at(f, s + 12), raw_size = le32_at(f, s + 16), raw_ptr = le32_at(f, s + 20);
    if (rva >= va && rva - va < raw_size) return raw_ptr + (rva - va);
  }
  throw MissingResource{"Terrain.dll: table outside the file's sections"};
}
std::vector<float> read_table(const std::vector<uint8_t> &f, const DllTable &t) {
  const size_t off = rva_to_offset(f, t.rva);
  if (off + t.size > f.size()) throw MissingResource{"Terrain.dll: truncated table"};
  if (fnv1a64(f.data() + off, t.size) != t.fnv) throw MissingResource{"Terrain.dll: not the anchored file (a constant table differs)"};
  std::vector<float> v(t.size / 4);
  std::memcpy(v.data(), f.data() + off, t.size);
  return v;
}
}  // namespace

bool terrain_load_dll_constants(Ctx &ctx, TerrainGlobals &g) {
  if (!ctx.files_ptr || !ctx.files().exists("Terrain.dll")) return false;
  const std::vector<uint8_t> f = ctx.files().read_all("Terrain.dll");
  const std::vector<float> sn = read_table(f, kSideNormals);
  g.tex_coords = read_table(f, kTexCoords);
  std::memcpy(g.side_normals, sn.data(), sizeof g.side_normals);
  return true;
}

// ---------------------------------------------------------------------------------------------------------------- tiles

uint32_t terrain_tile_at(Ctx &ctx, uint32_t terrain, int32_t x, int32_t y) {
  const auto h = hdr_p(ctx, terrain);
  const int32_t w = h.field(&TerrainObjectHeader::width).get(), ht = h.field(&TerrainObjectHeader::height).get();
  if (x >= w || x < 0 || y >= ht || y < 0) return 0;
  return terrain + kTerrainTilesOffset + static_cast<uint32_t>(x + y * w) * static_cast<uint32_t>(sizeof(TerrainTile));
}

void tile_reset(Ctx &ctx, uint32_t tile, int32_t x, int32_t y) {
  const TileP t = tile_p(ctx, tile);
  t.field(&TerrainTile::x).set(x);
  t.field(&TerrainTile::y).set(y);
  t.field(&TerrainTile::variation).set(0);
  const auto lv = t.field(&TerrainTile::level);
  lv.at(1).set(0);
  lv.at(7).set(0);
  lv.at(3).set(0);
  lv.at(5).set(0);
  for (int32_t i = 0; i < 4; ++i) t.field(&TerrainTile::neighbour).at(i).set(0u);
  t.field(&TerrainTile::type).set(g_negctl == 11 ? 5 : 4);
  t.field(&TerrainTile::rotation).set(0);
  // PathInfo::clear: all seven bytes
  t.field(&TerrainTile::connected).set(0);
  t.field(&TerrainTile::has_path).set(0);
  t.field(&TerrainTile::draw_path).set(0);
  for (int32_t i = 0; i < 4; ++i) t.field(&TerrainTile::path_b3).at(i).set(0);
  // WallInfo::clear: the flags of sides 0, 2, 6 and 4 only (the heights and the other five flags keep the allocation's fill)
  const auto on = t.field(&TerrainTile::wall_on);
  on.at(0).set(0);
  on.at(2).set(0);
  on.at(6).set(0);
  on.at(4).set(0);
}

void faces_build(Ctx &ctx, uint32_t tile, int32_t x, int32_t y, int32_t width) {
  const TileP t = tile_p(ctx, tile);
  const int32_t row = width * 3, yoff = y * row * 3, xoff = x * 3;
  t.field(&TerrainTile::face_count).set(8);
  auto put = [&](int32_t c, int32_t v0, int32_t v1, int32_t v2, const float tc[6]) {
    const FaceP f = (t.field(&TerrainTile::face) + c);
    const auto v = f.field(&TerrainFace::v);
    v.at(0).set(v0);
    v.at(1).set(v1);
    v.at(2).set(v2);
    for (int32_t k = 0; k < 6; ++k) f.field(&TerrainFace::tc).at(k).set(round_half(tc[k]));
    const auto n = f.field(&TerrainFace::normal);
    n.at(0).set(0.0f);
    n.at(1).set(1.0f);
    n.at(2).set(0.0f);
  };
  int32_t c = 0;
  for (int32_t i = 0; i < 2; ++i)
    for (int32_t j = 0; j < 2; ++j) {
      const int32_t base = yoff + xoff;
      // the coordinates the original computes in float: j / 2.0f, 1.0f - i / 2.0f ...
      const float u0 = j / 2.0f, u1 = (j + 1) / 2.0f, w0 = 1.0f - i / 2.0f, w1 = 1.0f - (i + 1) / 2.0f;
      const float a[6] = {u0, w0, u0, w1, u1, w1};
      put(c++, j + i * row + base, j + (i + 1) * row + base, j + 1 + (i + 1) * row + base, a);
      const float b[6] = {u0, w0, u1, w1, u1, w0};
      put(c++, j + i * row + base, j + 1 + (i + 1) * row + base, j + 1 + i * row + base, b);
    }
  if (g_negctl == 1) return;
  // faces 2 / 3 and 4 / 5 share an edge: each copies the other's vertex and coordinates
  auto copy_v = [&](int32_t fd, int32_t kd, int32_t fs, int32_t ks) { (t.field(&TerrainTile::face) + fd).field(&TerrainFace::v).at(kd).set(f_v(t, fs, ks)); };
  auto copy_tc = [&](int32_t fd, int32_t kd, int32_t fs, int32_t ks) {
    (t.field(&TerrainTile::face) + fd).field(&TerrainFace::tc).at(kd).set((t.field(&TerrainTile::face) + fs).field(&TerrainFace::tc).at(ks).get());
  };
  copy_v(2, 2, 3, 2);
  copy_tc(2, 4, 3, 4);
  copy_tc(2, 5, 3, 5);
  copy_v(3, 0, 2, 1);
  copy_tc(3, 0, 2, 2);
  copy_tc(3, 1, 2, 3);
  copy_v(4, 2, 5, 2);
  copy_tc(4, 4, 5, 4);
  copy_tc(4, 5, 5, 5);
  copy_v(5, 0, 4, 1);
  copy_tc(5, 0, 4, 2);
  copy_tc(5, 1, 4, 3);
}

// initGrid: a width x height grid of tiles, 3 x 3 vertices each, centred on the origin, height 0, normal (0, 1, 0). The tile size
// is the DLL's g_tileW / g_tileH, 100.0 each (Terrain.dll data, read by the instructions of initGrid).
void init_grid(TerrainGlobals &g, int32_t width, int32_t height) {
  const float tile_w = 100.0f, tile_h = 100.0f;
  const int32_t row = width * 3;
  for (int32_t y = 0; y < height; ++y)
    for (int32_t x0 = 0; x0 < width; ++x0) {
      const int32_t x = g_negctl == 2 ? x0 + 1 : x0;
      const int32_t base = x0 * 3 + y * row * 3;
      // x * g_tileW - w * g_tileW / 2.0f: one x87 expression, rounded to float on the store into the local
      const float ox = f32(static_cast<double>(x) * tile_w - static_cast<double>(width) * tile_w / 2.0);
      const float oz = f32(static_cast<double>(y) * tile_h - static_cast<double>(height) * tile_h / 2.0);
      for (int32_t i = 0; i < 3; ++i)
        for (int32_t j = 0; j < 3; ++j) {
          float *v = vtx(g, base + i + j * row);
          float *n = nrm(g, base + i + j * row);
          v[0] = f32(static_cast<double>(i) * (tile_w / 2.0) + ox);
          v[2] = f32(static_cast<double>(j) * (tile_h / 2.0) + oz);
          v[1] = 0.0f;
          n[0] = 0.0f;
          n[1] = 1.0f;
          n[2] = 0.0f;
        }
    }
}

bool terrain_build_arrays(Ctx &ctx, TerrainGlobals &g, uint32_t terrain) {
  const auto h = hdr_p(ctx, terrain);
  const int32_t w = h.field(&TerrainObjectHeader::width).get(), ht = h.field(&TerrainObjectHeader::height).get();
  init_grid(g, w, ht);
  for (int32_t x = 0; x < w; ++x)
    for (int32_t y = 0; y < ht; ++y) {
      const uint32_t t = terrain + kTerrainTilesOffset + static_cast<uint32_t>(x + y * w) * static_cast<uint32_t>(sizeof(TerrainTile));
      tile_reset(ctx, t, x, y);
      faces_build(ctx, t, x, y, w);
    }
  const int32_t sx_minus = g_negctl == 3 ? kSideXPlus : kSideXMinus, sx_plus = g_negctl == 3 ? kSideXMinus : kSideXPlus;
  for (int32_t x = 0; x < w; ++x)
    for (int32_t y = 0; y < ht; ++y) {
      const auto nb = tile_p(ctx, terrain + kTerrainTilesOffset + static_cast<uint32_t>(x + y * w) * static_cast<uint32_t>(sizeof(TerrainTile)))
                          .field(&TerrainTile::neighbour);
      nb.at(sx_minus).set(terrain_tile_at(ctx, terrain, x - 1, y));
      nb.at(kSideYMinus).set(terrain_tile_at(ctx, terrain, x, y - 1));
      nb.at(sx_plus).set(terrain_tile_at(ctx, terrain, x + 1, y));
      nb.at(kSideYPlus).set(terrain_tile_at(ctx, terrain, x, y + 1));
    }
  return true;
}

void terrain_init_terrain(Ctx &ctx, TerrainGlobals &g, uint32_t terrain) { terrain_build_arrays(ctx, g, terrain); }

void terrain_pass_collar_info(TerrainGlobals &g, const int32_t *table, int32_t n) {
  g.collar_set = true;
  g.collar.assign(table, table + (n > 0 ? n : 0));
}

void terrain_construct(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, int32_t width, int32_t height) {
  const auto h = hdr_p(ctx, terrain);
  // the member constructors first: the vector constructor of the 0x9c4 tiles runs Tile::Tile, whose members Faces (count 0),
  // PathInfo (clear) and WallInfo (clear) construct themselves (re/match/terrain_tile6.cpp); the draw list follows (below)
  for (int32_t i = 0; i < kTerrainMaxTiles; ++i) {
    const TileP t = tile_p(ctx, terrain + kTerrainTilesOffset + static_cast<uint32_t>(i) * static_cast<uint32_t>(sizeof(TerrainTile)));
    t.field(&TerrainTile::face_count).set(0);
    t.field(&TerrainTile::connected).set(0);
    t.field(&TerrainTile::has_path).set(0);
    t.field(&TerrainTile::draw_path).set(0);
    for (int32_t k = 0; k < 4; ++k) t.field(&TerrainTile::path_b3).at(k).set(0);
    const auto on = t.field(&TerrainTile::wall_on);
    on.at(0).set(0);
    on.at(2).set(0);
    on.at(6).set(0);
    on.at(4).set(0);
  }
  g.collar_set = false;
  g.collar.clear();
  h.field(&TerrainObjectHeader::width).set(width);
  h.field(&TerrainObjectHeader::height).set(height);
  h.field(&TerrainObjectHeader::zoom).set(0);
  h.field(&TerrainObjectHeader::m4).set(0.15f);
  h.field(&TerrainObjectHeader::m8).set(0.15f);
  h.field(&TerrainObjectHeader::mc).set(0.2f);
  h.field(&TerrainObjectHeader::m10).set(1.0f);
  h.field(&TerrainObjectHeader::glrc).set(0u);
  h.field(&TerrainObjectHeader::textures_loaded).set(0);
  for (int32_t x = 0; x < width; ++x)
    for (int32_t y = 0; y < height; ++y)
      tile_reset(ctx, terrain + kTerrainTilesOffset + static_cast<uint32_t>(x + y * width) * static_cast<uint32_t>(sizeof(TerrainTile)), x, y);
  h.field(&TerrainObjectHeader::flip_y).set(0);
  g.course_type = 0;
  for (int32_t i = 0; i < kTerrainTextureTypes; ++i) (h.field(&TerrainObjectHeader::types) + i).field(&TerrainTypeInfo::count).set(0);
  g.textures.assign(static_cast<size_t>(kTerrainTextureTypes) * kTerrainTextureVariations * kTerrainTextureSlots, 0u);
  // the draw list's constructor (std::list<Tile*>, re/match/terrain_list.cpp): the allocator byte is copied from a default-constructed
  // temporary in the debug build's stack fill (0xcc), the head node comes from operator new (a heap address: a token here), size 0
  VPtr<uint8_t>(ctx.mem, terrain + kTerrainDrawListOffset).set(0xcc);
  VPtr<uint32_t>(ctx.mem, terrain + kTerrainDrawListOffset + 4).set(kListHeadToken);
  VPtr<uint32_t>(ctx.mem, terrain + kTerrainDrawListOffset + 8).set(0u);
}

void terrain_apply_gl_state(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, const TerrainGlState &st) {
  const auto h = hdr_p(ctx, terrain);
  for (int32_t i = 0; i < kTerrainTextureTypes; ++i) {
    const auto ti = h.field(&TerrainObjectHeader::types) + i;
    if (!st.type_name[i].empty()) {
      // strcpy: the name and its NUL; the rest of the 0x14 bytes keep what was there
      const std::string &s = st.type_name[i];
      for (size_t k = 0; k <= s.size(); ++k) ti.byte(static_cast<int32_t>(k)).set(k < s.size() ? static_cast<uint8_t>(s[k]) : 0);
    }
    ti.field(&TerrainTypeInfo::count).set(st.type_count[i]);
  }
  h.field(&TerrainObjectHeader::glrc).set(st.has_context ? kContextToken : 0u);
  h.field(&TerrainObjectHeader::flip_y).set(st.flip_y ? 1 : 0);
  h.field(&TerrainObjectHeader::textures_loaded).set(st.textures_loaded ? 1 : 0);
  if (st.has_context) {   // the window size is written by initSystem only (the constructor leaves it at the allocation's fill)
    h.field(&TerrainObjectHeader::screen_w).set(st.screen_w);
    h.field(&TerrainObjectHeader::screen_h).set(st.screen_h);
  }
  h.field(&TerrainObjectHeader::zoom).set(st.zoom);
  g.textures = st.textures;
  g.course_type = st.course_type;
  // the wall and fence textures Tile::render binds are entries of the texture table itself (the renderer's operands lie inside it):
  // [0x24][0][0] the skirt wall, [0x22][0][0] fences of kinds other than 1, [0x23][0][0] kind 1
  auto entry = [&](int32_t type) { return g.textures.empty() ? 0u : g.textures[static_cast<size_t>(type) * kTerrainTextureVariations * kTerrainTextureSlots]; };
  g.wall_texture = entry(0x24);
  g.fence_texture0 = entry(0x22);
  g.fence_texture1 = entry(0x23);
  if (st.has_context) g.screen_scale = st.screen_scale;   // initSystem's value for the window size
}

// --------------------------------------------------------------------------------------------------------------- normals

namespace {
void face_normal_at(TerrainGlobals &g, const FaceP &f) {
  // the face in place when all of it lies in the object view (reads and writes the bytes get / set would), else get / set in the
  // original's order
  uint8_t *const h = f.host(static_cast<uint32_t>(sizeof(TerrainFace)));
  const float *p0, *p1, *p2;
  if (h) {
    int32_t iv[3];
    std::memcpy(iv, h + offsetof(TerrainFace, v), sizeof iv);
    p0 = vtx(g, iv[0]);
    p1 = vtx(g, iv[1]);
    p2 = vtx(g, iv[2]);
  } else {
    const auto fv = f.field(&TerrainFace::v);
    p0 = vtx(g, fv.at(0).get());
    p1 = vtx(g, fv.at(1).get());
    p2 = vtx(g, fv.at(2).get());
  }
  float a[3], b[3];
  for (int k = 0; k < 3; ++k) a[k] = f32(static_cast<double>(p1[k]) - p0[k]);
  for (int k = 0; k < 3; ++k) b[k] = f32(static_cast<double>(p2[k]) - p1[k]);
  if (g_negctl == 4) std::swap(a, b);
  float n[3];
  n[0] = f32(static_cast<double>(a[1]) * b[2] - static_cast<double>(a[2]) * b[1]);
  n[1] = f32(static_cast<double>(a[2]) * b[0] - static_cast<double>(a[0]) * b[2]);
  n[2] = f32(static_cast<double>(a[0]) * b[1] - static_cast<double>(a[1]) * b[0]);
  normalize3(n);
  if (h) {
    std::memcpy(h + offsetof(TerrainFace, normal), n, sizeof n);
    return;
  }
  const auto fn = f.field(&TerrainFace::normal);
  for (int k = 0; k < 3; ++k) fn.at(k).set(n[k]);
}

// One tile's face vertex indices and face normals for smoothNormals / calcNormals: read in place when the whole tile lies in the
// object view, else through get (f is a face slot 0..7, k a vertex slot or component 0..2)
struct TileFaces {
  const TileP &p;
  const uint8_t *h;
  explicit TileFaces(const TileP &t) : p(t), h(t.host(static_cast<uint32_t>(sizeof(TerrainTile)))) {}
  static size_t at(int32_t f) { return offsetof(TerrainTile, face) + static_cast<size_t>(f) * sizeof(TerrainFace); }
  float n(int32_t f, int32_t k) const {
    if (!h) return fnorm(p, f, k);
    float x;
    std::memcpy(&x, h + at(f) + offsetof(TerrainFace, normal) + 4u * static_cast<size_t>(k), 4);
    return x;
  }
  int32_t v(int32_t f, int32_t k) const {
    if (!h) return f_v(p, f, k);
    int32_t x;
    std::memcpy(&x, h + at(f) + offsetof(TerrainFace, v) + 4u * static_cast<size_t>(k), 4);
    return x;
  }
};
}  // namespace

void face_normal(Ctx &ctx, TerrainGlobals &g, uint32_t face) { face_normal_at(g, FaceP(ctx.mem, face)); }

namespace {

// One target vertex of smoothNormals: own faces' sum, then `+= A + B` for each present neighbour, in the original's order.
struct Add {
  int which;  // 0 = fa, 1 = fb, 2 = fc
  int32_t f0, f1;
};
struct Target {
  int32_t face, vert;   // the own face and vertex slot naming the target vertex
  int32_t own0, own1;
  Add adds[3];
  int n_adds;
};
struct SmoothCase {
  int32_t own[4];
  int n_own;
  int32_t fa[3], fb[3], fc[3];
  int n_fa, n_fb, n_fc;
  Target t[3];
  int n_t;
  int32_t norm_order[3];   // indices into t: the normalize calls' order
};

// Tile::smoothNormals' four cases, (a, b) = (0, 2), (0, 3), (1, 2), (1, 3)
const SmoothCase kCases[4] = {
    {{0, 1, 2, 4}, 4, {4, 5, 6}, {2, 3, 7}, {6, 7, 0}, 3, 3, 2,
     {{1, 2, 1, 2, {{0, 5, 6}}, 1}, {0, 1, 0, 4, {{1, 3, 7}}, 1}, {0, 0, 0, 1, {{0, 4, 5}, {1, 2, 3}, {2, 6, 7}}, 3}}, 3, {2, 1, 0}},
    {{2, 3, 7, 0}, 3, {6, 7, 0}, {0, 4, 0}, {4, 5, 0}, 2, 2, 2,
     {{3, 1, 7, 3, {{1, 0, 4}}, 1}, {3, 2, 2, 3, {{0, 6, 7}, {1, 0, 1}, {2, 4, 5}}, 3}}, 2, {0, 1, 0}},
    {{4, 5, 6, 0}, 3, {0, 1, 2}, {6, 7, 0}, {2, 3, 0}, 3, 2, 2,
     {{4, 1, 5, 4, {{0, 0, 1}, {1, 6, 7}, {2, 2, 3}}, 3}, {5, 1, 5, 6, {{0, 1, 2}}, 1}}, 2, {0, 1, 0}},
    {{6, 7, 0, 0}, 2, {2, 3, 0}, {4, 5, 0}, {0, 1, 0}, 2, 2, 2,
     {{7, 1, 7, 6, {{0, 2, 3}, {1, 4, 5}, {2, 0, 1}}, 3}}, 1, {0, 0, 0}},
};

}  // namespace

namespace {
void smooth_normals(Ctx &ctx, TerrainGlobals &g, const TileP &tp, int32_t a, int32_t b) {
  // the neighbour tiles whose faces take part (getFaces is the tile + 0x44); each link is read once (the original reads some twice:
  // plain reads of the same dword, nothing written in between)
  uint32_t fa = 0, fb = 0, fc = 0;
  const uint32_t na = t_nb(tp, a);
  if (na != 0) fa = na;
  const uint32_t nbb = t_nb(tp, b);
  if (nbb != 0) {
    fb = nbb;
    if (na != 0) {
      const uint32_t d = t_nb(tile_p(ctx, fb), a);
      if (d != 0) fc = d;
    }
  }
  int ci;
  if (a == 0 && b == 2) ci = 0;
  else if (a == 0 && b == 3) ci = 1;
  else if (a == 1 && b == 2) ci = 2;
  else if (a == 1 && b == 3) ci = 3;
  else return;   // no other pair has a body
  const SmoothCase &sc = kCases[ci];
  const TileP ta = tile_p(ctx, fa), tb = tile_p(ctx, fb), tc = tile_p(ctx, fc);
  auto fnormal = [&](const TileP &t, int32_t f) { face_normal_at(g, t.field(&TerrainTile::face) + f); };
  for (int k = 0; k < sc.n_own; ++k) fnormal(tp, sc.own[k]);
  if (fa) for (int k = 0; k < sc.n_fa; ++k) fnormal(ta, sc.fa[k]);
  if (fb) for (int k = 0; k < sc.n_fb; ++k) fnormal(tb, sc.fb[k]);
  if (fc) for (int k = 0; k < sc.n_fc; ++k) fnormal(tc, sc.fc[k]);
  const uint32_t nbt[3] = {fa, fb, fc};
  // the sums only read face normals and write the module's normal array, so each tile's faces are read in place (TileFaces)
  const TileFaces own(tp), fta(ta), ftb(tb), ftc(tc);
  const TileFaces *nbp[3] = {&fta, &ftb, &ftc};
  // the targets are distinct vertices of this tile and each component is independent, so each target is summed whole, in order
  for (int i = 0; i < 3; ++i)
    for (int ti = 0; ti < sc.n_t; ++ti) {
      const Target &t = sc.t[ti];
      float *dst = nrm(g, own.v(t.face, t.vert));
      dst[i] = f32(static_cast<double>(own.n(t.own0, i)) + own.n(t.own1, i));
      for (int k = 0; k < t.n_adds; ++k) {
        const Add &ad = t.adds[k];
        if (g_negctl == 6 && ci == 1 && ad.which == 2) continue;
        if (!nbt[ad.which]) continue;
        const TileFaces &src = *nbp[ad.which];
        const double pair = static_cast<double>(src.n(ad.f0, i)) + src.n(ad.f1, i);
        dst[i] = f32(static_cast<double>(dst[i]) + pair);
      }
    }
  for (int k = 0; k < sc.n_t; ++k) {
    const Target &t = sc.t[sc.norm_order[k]];
    normalize3(nrm(g, own.v(t.face, t.vert)));
  }
}
}  // namespace

void tile_smooth_normals(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t a, int32_t b) { smooth_normals(ctx, g, tile_p(ctx, tile), a, b); }

void tile_calc_normals(Ctx &ctx, TerrainGlobals &g, uint32_t tile) {
  const TileP t = tile_p(ctx, tile);
  if (!t.field(&TerrainTile::normals_dirty).get()) return;
  smooth_normals(ctx, g, t, 0, 2);
  smooth_normals(ctx, g, t, 1, 2);
  smooth_normals(ctx, g, t, 0, 3);
  smooth_normals(ctx, g, t, 1, 3);
  const TileFaces tf(t);
  float *c = nrm(g, tf.v(0, 2));
  for (int k = 0; k <= 2; ++k) {
    double s = tf.n(0, k);
    for (int f = 1; f < (g_negctl == 5 ? 7 : 8); ++f) s += tf.n(f, k);
    c[k] = f32(s);
  }
  normalize3(c);
  t.field(&TerrainTile::normals_dirty).set(0);
}

void terrain_calc_normals(Ctx &ctx, TerrainGlobals &g, uint32_t tile) {
  if (tile != 0) tile_calc_normals(ctx, g, tile);
}

void terrain_calc_all_normals(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, uint32_t tile) {
  const int32_t zoom = hdr_p(ctx, terrain).field(&TerrainObjectHeader::zoom).get();
  const int32_t a = 13 << zoom, b = a, c = a, d = a;
  const TileP t = tile_p(ctx, tile);
  for (int32_t y = t.field(&TerrainTile::y).get() - a; y < t.field(&TerrainTile::y).get() + b; ++y)
    for (int32_t x = t.field(&TerrainTile::x).get() + d; x > t.field(&TerrainTile::x).get() - c; --x) {
      const uint32_t n = terrain_tile_at(ctx, terrain, x, y);
      if (n) tile_calc_normals(ctx, g, n);
    }
}

// ---------------------------------------------------------------------------------------------------------- face textures

namespace {
// setFaceTexture with the tile's own type, rotation and variation already read (blendFaceTextures calls it eight times on one tile and
// writes only face textures and pieces in between, so those three fields read the same each time)
void set_face_texture(TerrainGlobals &g, const TileP &tp, int32_t own, int32_t rot, int32_t variation, int32_t type, int32_t face,
                      int32_t piece);
}  // namespace

void tile_set_face_texture(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type, int32_t face, int32_t piece) {
  const TileP tp = tile_p(ctx, tile);
  const int32_t own = t_type(tp), rot = t_rot(tp);
  if (face < 0 || face > 7) return;   // no case: nothing written (and the variation is not read)
  set_face_texture(g, tp, own, rot, tp.field(&TerrainTile::variation).get(), type, face, piece);
}

namespace {
void set_face_texture(TerrainGlobals &g, const TileP &tp, int32_t own, int32_t rot, int32_t variation, int32_t type, int32_t face,
                      int32_t piece) {
  switch (own) {
    case 0xd: case 0xe: case 0xf: case 0x10: type = 0xd; break;
    case 0x11:
      if (rot == 1) type = 0x17;
      else if (rot == 2) type = 0x18;
      break;
    case 6: case 0x15: type = 4; break;
    case 0x16:
      if (rot >= 0x40 && rot < 0x48) type = 4;
      break;
    case 1:
      if (rot & 0x80) type = 0x1a;
      break;
    case 7:
      switch (rot % 4) {   // C remainder: a negative rotation gives a negative remainder and no case
        case 0: type = 0x1b; break;
        case 1: type = 0x1c; break;
        case 2: type = 0x1d; break;
        case 3: type = 0x1e; break;
        default: break;
      }
      break;
    default: break;
  }
  if ((type == 4 || own == 7 || type == 2) && piece > 4) piece = 0;
  // face argument -> face slot
  static const int32_t kSlot[8] = {2, 3, 7, 6, 5, 4, 0, 1};
  static const int32_t kSlotWrong[8] = {3, 2, 7, 6, 5, 4, 0, 1};
  const int32_t slot = (g_negctl == 9 ? kSlotWrong : kSlot)[face];
  const FaceP f = tp.field(&TerrainTile::face) + slot;
  f.field(&TerrainFace::texture).set(tex_slot(g, type, variation, piece));
  f.field(&TerrainFace::piece).set(static_cast<uint8_t>(piece));
}

// edgeKind with the tile's own type and rotation already read (the original reads them only when `other` is not NULL: plain reads of
// the same tile's dwords, which blendFaceTextures has read before)
int32_t edge_kind(Ctx &ctx, const TerrainGlobals &g, int32_t own, int32_t rot, uint32_t other) {
  if (!other) return 0;
  const TileP op = tile_p(ctx, other);
  if (own == 0x11) {
    if (rot == 0 && own != t_type(op)) return 1;
    // getVariation returns the low byte of +0x28, sign-extended (char)
    const int32_t ov = static_cast<int8_t>(static_cast<uint8_t>(t_rot(op)));
    if (rot != ov) {
      if (rot == 0 || (rot == 1 && ov == 2)) return g_negctl == 10 ? 1 : 2;
      return 1;
    }
    return 0;
  }
  const int32_t ot = t_type(op);
  if (collar_of(g, own) != collar_of(g, ot)) return 1;
  if (ot != own) return 2;
  return 0;
}

void blend_face_textures(Ctx &ctx, TerrainGlobals &g, const TileP &tp, int32_t type) {
  if (type == -1) type = t_type(tp);
  if (type == 0x14) return;
  const uint32_t n0 = t_nb(tp, 0), n1 = t_nb(tp, 1), n2 = t_nb(tp, 2), n3 = t_nb(tp, 3);
  // the tile's own type and rotation: blendFaceTextures writes only face textures and pieces, so every edgeKind and setFaceTexture
  // call below reads these same values
  const int32_t own = t_type(tp), rot = t_rot(tp);
  const int32_t t0 = edge_kind(ctx, g, own, rot, n0), t1 = edge_kind(ctx, g, own, rot, n2), t2 = edge_kind(ctx, g, own, rot, n3),
                t3 = edge_kind(ctx, g, own, rot, n1);
  int32_t d0 = 0, d1 = 0, d2 = 0, d3 = 0;
  if (!is_solid_type(type)) return;
  const bool water_desert = g.course_type == 1 && type == 0x11;
  auto open = [&](uint32_t t) { return is_open_type(t_type(tile_p(ctx, t))); };
  auto nb = [&](uint32_t t, int32_t side) { return t_nb(tile_p(ctx, t), side); };
  bool b0 = false, b1 = false, b2 = false, b3 = false;
  if (n0 != 0) {
    if (water_desert && open(n0)) b0 = true;
    if (n2 != 0) {
      const uint32_t d = nb(n0, 2);
      if (water_desert) (void)open(d);   // b4: set, never read (the read itself can fault on a NULL diagonal)
      d0 = edge_kind(ctx, g, own, rot, d);
    }
    if (n3 != 0) {
      const uint32_t d = nb(n0, 3);
      if (water_desert) (void)open(d);
      d1 = edge_kind(ctx, g, own, rot, d);
    }
  }
  if (n1 != 0) {
    if (water_desert && open(n1)) b1 = true;
    if (n2 != 0) {
      const uint32_t d = nb(n1, 2);
      if (water_desert) (void)open(d);
      d2 = edge_kind(ctx, g, own, rot, d);
    }
    if (n3 != 0) {
      const uint32_t d = nb(n1, 3);
      if (water_desert) (void)open(d);
      d3 = edge_kind(ctx, g, own, rot, d);
    }
  }
  if (n2 != 0 && water_desert && open(n2)) b2 = true;
  if (n3 != 0 && water_desert && open(n3)) b3 = true;
  struct Step { int32_t a, b, c; bool flag; int32_t face; };
  const Step steps[8] = {{t0, t1, d0, b0, 7}, {t0, t2, d1, b0, 0}, {t2, t0, d1, b3, 1}, {t2, t3, d3, b3, 2},
                         {t3, t2, d3, b1, 3}, {t3, t1, d2, b1, 4}, {t1, t0, d0, b2, 6}, {t1, t3, d2, b2, 5}};
  const int32_t variation = tp.field(&TerrainTile::variation).get();
  for (const Step &s : steps) {
    const int32_t value = id_class(flag_code(s.a, s.b, s.c));
    set_face_texture(g, tp, own, rot, variation, s.flag ? 0x19 : type, s.face, value);
  }
}

void reblend_neighbourhood(Ctx &ctx, TerrainGlobals &g, const TileP &tp) {
  blend_face_textures(ctx, g, tp, t_type(tp));
  const uint32_t n0 = t_nb(tp, 0), n1 = t_nb(tp, 1), n2 = t_nb(tp, 2), n3 = t_nb(tp, 3);
  const TileP p0 = tile_p(ctx, n0), p1 = tile_p(ctx, n1), p2 = tile_p(ctx, n2), p3 = tile_p(ctx, n3);
  if (n0 != 0) blend_face_textures(ctx, g, p0, -1);
  if (n0 != 0 && n2 != 0) blend_face_textures(ctx, g, tile_p(ctx, t_nb(p0, 2)), -1);
  if (n2 != 0) blend_face_textures(ctx, g, p2, -1);
  if (n1 != 0 && n2 != 0) blend_face_textures(ctx, g, tile_p(ctx, t_nb(p1, 2)), -1);
  if (n1 != 0) blend_face_textures(ctx, g, p1, -1);
  if (n1 != 0 && n3 != 0) blend_face_textures(ctx, g, tile_p(ctx, t_nb(p1, 3)), -1);
  if (n3 != 0) blend_face_textures(ctx, g, p3, -1);
  if (n0 != 0 && n3 != 0) blend_face_textures(ctx, g, tile_p(ctx, t_nb(p0, 3)), -1);
}
}  // namespace

int32_t tile_edge_kind(Ctx &ctx, const TerrainGlobals &g, uint32_t tile, uint32_t other) {
  if (!other) return 0;
  const TileP tp = tile_p(ctx, tile);
  return edge_kind(ctx, g, t_type(tp), t_rot(tp), other);
}

void tile_blend_face_textures(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type) { blend_face_textures(ctx, g, tile_p(ctx, tile), type); }

void tile_reblend_neighbourhood(Ctx &ctx, TerrainGlobals &g, uint32_t tile) { reblend_neighbourhood(ctx, g, tile_p(ctx, tile)); }

void tile_reset_face_textures(Ctx &ctx, TerrainGlobals &g, uint32_t tile) {
  const TileP t = tile_p(ctx, tile);
  if (t.field(&TerrainTile::has_path).get()) t.field(&TerrainTile::draw_path).set(0);
  const int32_t n = t.field(&TerrainTile::face_count).get();
  if (t_type(t) == 0) {
    for (int32_t i = 0; i < n; ++i) (t.field(&TerrainTile::face) + i).field(&TerrainFace::texture).set(tex_slot(g, 0, t_rot(t), 0));
  } else {
    const int32_t var = t.field(&TerrainTile::variation).get();
    for (int32_t j = 0; j < n; ++j) (t.field(&TerrainTile::face) + j).field(&TerrainFace::texture).set(tex_slot(g, t_type(t), var, 3));
  }
  reblend_neighbourhood(ctx, g, t);
}

// ---------------------------------------------------------------------------------------------------------------- mutators

int32_t terrain_rand(TerrainGlobals &g) {
  g.rand_state = g.rand_state * 214013u + (g_negctl == 14 ? 2531013u : 2531011u);
  return static_cast<int32_t>((g.rand_state >> 16) & 0x7fffu);
}

namespace {

float &vy(TerrainGlobals &g, int32_t index) { return vtx(g, index)[1]; }
// vertex k of face f of tile t, as a vertex index; f and k may be the debug build's stack fill (an index the original never checks)
int32_t fvx(const TileP &t, int32_t f, int32_t k) { return (t.as_at<int32_t>(0x48u + static_cast<uint32_t>(f) * 0x38u) + k).get(); }
void add_y(TerrainGlobals &g, int32_t index, float d) {
  float &y = vy(g, index);
  y = f32(static_cast<double>(y) + d);
}
void set_dirty(const TileP &t) { t.field(&TerrainTile::normals_dirty).set(1); }

// the 3 x 3 neighbourhood's normals-dirty marks, in the original's order (a diagonal is reached through n0 / n1 and faults when that
// neighbour's own link is NULL); only_type >= 0: a neighbour is marked only when it has that type (enterType7)
void dirty_neighbourhood(Ctx &c, const TileP &t, int32_t only_type = -1) {
  auto mark = [&](const TileP &n) {
    if (only_type < 0 || t_type(n) == only_type) set_dirty(n);
  };
  set_dirty(t);
  const uint32_t n0 = t_nb(t, 0), n1 = t_nb(t, 1), n2 = t_nb(t, 2), n3 = t_nb(t, 3);
  const TileP p0 = tile_p(c, n0), p1 = tile_p(c, n1), p2 = tile_p(c, n2), p3 = tile_p(c, n3);
  if (n0) mark(p0);
  if (n0 && n2) mark(tile_p(c, t_nb(p0, 2)));
  if (n0 && n3) mark(tile_p(c, t_nb(p0, 3)));
  if (n2) mark(p2);
  if (n3) mark(p3);
  if (n1) mark(p1);
  if (n1 && n2) mark(tile_p(c, t_nb(p1, 2)));
  if (n1 && n3) mark(tile_p(c, t_nb(p1, 3)));
}

// The vertex walk of enter / leaveType6 and enter / leaveType7 (re/match/terrain_type6.cpp): the centre, then every vertex this tile
// shares with a neighbour or diagonal of type k, moved by d. `own00` is the move of this tile's face 0 vertex 0 (the n0 / n2 diagonal
// case): leaveType6 adds there where it subtracts everywhere else.
void type_walk(Ctx &c, TerrainGlobals &g, const TileP &t, int32_t k, float d, float own00) {
  auto is = [&](const TileP &x) { return t_type(x) == k; };
  add_y(g, fvx(t, 0, 2), d);
  const uint32_t n0 = t_nb(t, 0), n1 = t_nb(t, 1), n2 = t_nb(t, 2), n3 = t_nb(t, 3);
  const TileP p0 = tile_p(c, n0), p1 = tile_p(c, n1), p2 = tile_p(c, n2), p3 = tile_p(c, n3);
  if (n0 != 0 && is(p0)) {
    add_y(g, fvx(t, 1, 2), d);
    add_y(g, fvx(p0, 5, 1), d);
    if (n2 != 0 && is(p2)) {
      const TileP q = tile_p(c, t_nb(p0, 2));
      if (is(q)) {
        add_y(g, fvx(t, 0, 0), own00);
        add_y(g, fvx(p0, 4, 1), d);
        add_y(g, fvx(p2, 3, 2), d);
        add_y(g, fvx(q, 7, 1), d);
      }
    }
    if (n3 != 0 && is(p3)) {
      const TileP q = tile_p(c, t_nb(p0, 3));
      if (is(q)) {
        add_y(g, fvx(t, 3, 2), d);
        add_y(g, fvx(p0, 7, 1), d);
        add_y(g, fvx(p3, 0, 0), d);
        add_y(g, fvx(q, 4, 1), d);
      }
    }
  }
  if (n1 != 0 && is(p1)) {
    add_y(g, fvx(t, 5, 1), d);
    add_y(g, fvx(p1, 1, 2), d);
    if (n2 != 0 && is(p2)) {
      const TileP q = tile_p(c, t_nb(p1, 2));
      if (is(q)) {
        add_y(g, fvx(t, 5, 0), d);
        add_y(g, fvx(p1, 0, 0), d);
        add_y(g, fvx(p2, 7, 1), d);
        add_y(g, fvx(q, 3, 2), d);
      }
    }
    if (n3 != 0 && is(p3)) {
      const TileP q = tile_p(c, t_nb(p1, 3));
      if (is(q)) {
        add_y(g, fvx(t, 7, 1), d);
        add_y(g, fvx(p1, 3, 2), d);
        add_y(g, fvx(p3, 4, 1), d);
        add_y(g, fvx(q, 0, 0), d);
      }
    }
  }
  if (n2 != 0 && is(p2)) {
    add_y(g, fvx(t, 0, 1), d);
    add_y(g, fvx(p2, 3, 1), d);
  }
  if (n3 != 0 && is(p3)) {
    add_y(g, fvx(t, 3, 1), d);
    add_y(g, fvx(p3, 0, 1), d);
  }
}

// elevate / lowerCorner's six (face, vertex) pairs per corner: f5 = the corner itself, f0 = the opposite corner (the centre's mean),
// f1 / f2 = the two edge midpoints, f3 / f4 = their far ends. The switch has cases 1, 7, 3 and 5 only: any other corner leaves all
// twelve locals at the debug build's stack fill.
struct CornerMap {
  int32_t f5, v5, f0, v0, f1, v1, f2, v2, f3, v3, f4, v4;
};
CornerMap corner_map(int32_t corner) {
  switch (corner) {
    case 1: return {3, 2, 4, 1, 3, 1, 1, 2, 7, 1, 0, 0};
    case 7: return {0, 0, 7, 1, 1, 2, 0, 1, 3, 2, 4, 1};
    case 3: return {7, 1, 0, 0, 5, 1, 3, 1, 4, 1, 3, 2};
    case 5: return {4, 1, 3, 2, 0, 1, 5, 1, 0, 0, 7, 1};
    default: {
      const int32_t u = static_cast<int32_t>(0xccccccccu);
      return {u, u, u, u, u, u, u, u, u, u, u, u};
    }
  }
}

void corner_move(Ctx &c, TerrainGlobals &g, const TileP &t, int32_t corner, int32_t step) {
  const CornerMap m = corner_map(corner);
  const auto level = t.as_at<int32_t>(0) + corner;
  const float new_h = f32(static_cast<double>(add32(level.get(), step)) * g.corner_step);
  const float a = vy(g, fvx(t, m.f0, m.v0));
  const float b = vy(g, fvx(t, m.f3, m.v3));
  const float cc = vy(g, fvx(t, m.f4, m.v4));
  const float h = new_h;
  const float mean = f32((static_cast<double>(h) + a) * 0.5);
  float &centre = vy(g, fvx(t, 0, 2));
  const bool keep_max = step > 0 && g_negctl != 12;
  centre = keep_max ? (mean > centre ? mean : centre) : (mean < centre ? mean : centre);
  vy(g, fvx(t, m.f1, m.v1)) = f32((static_cast<double>(h) + b) * 0.5);
  vy(g, fvx(t, m.f2, m.v2)) = f32((static_cast<double>(h) + cc) * 0.5);
  level.set(add32(level.get(), step));
  vy(g, fvx(t, m.f5, m.v5)) = new_h;
  if (step < 0 && t_type(t) == 7) {
    const float low = f32(static_cast<double>(new_h) - (g_negctl == 16 ? 12.0 : 13.0));
    const uint32_t n0 = t_nb(t, 0), n1 = t_nb(t, 1), n2 = t_nb(t, 2), n3 = t_nb(t, 3);
    const TileP p0 = tile_p(c, n0), p1 = tile_p(c, n1), p2 = tile_p(c, n2), p3 = tile_p(c, n3);
    auto is7 = [&](const TileP &x) { return t_type(x) == 7; };
    vy(g, fvx(t, 0, 2)) = low;
    if (n0 != 0 && is7(p0)) {
      vy(g, fvx(t, 1, 2)) = low;
      if (n2 != 0 && is7(p2) && is7(tile_p(c, t_nb(p0, 2)))) vy(g, fvx(t, 0, 0)) = low;
      if (n3 != 0 && is7(p3) && is7(tile_p(c, t_nb(p0, 3)))) vy(g, fvx(t, 3, 2)) = low;
    }
    if (n1 != 0 && is7(p1)) {
      vy(g, fvx(t, 5, 1)) = low;
      if (n2 != 0 && is7(p2) && is7(tile_p(c, t_nb(p1, 2)))) vy(g, fvx(t, 5, 0)) = low;
      if (n3 != 0 && is7(p3) && is7(tile_p(c, t_nb(p1, 3)))) vy(g, fvx(t, 7, 1)) = low;
    }
    if (n2 != 0 && is7(p2)) vy(g, fvx(t, 0, 1)) = low;
    if (n3 != 0 && is7(p3)) vy(g, fvx(t, 3, 1)) = low;
  }
  dirty_neighbourhood(c, t);
}

void leave_type6(Ctx &ctx, TerrainGlobals &g, const TileP &t) {
  type_walk(ctx, g, t, 6, -20.0f, g_negctl == 13 ? -20.0f : 20.0f);
  dirty_neighbourhood(ctx, t);
}
void leave_type7(Ctx &ctx, TerrainGlobals &g, const TileP &t) {
  type_walk(ctx, g, t, 7, 13.0f, 13.0f);
  dirty_neighbourhood(ctx, t);
}
void enter_type6(Ctx &ctx, TerrainGlobals &g, const TileP &t) {
  type_walk(ctx, g, t, 6, 20.0f, 20.0f);
  dirty_neighbourhood(ctx, t);
  reblend_neighbourhood(ctx, g, t);
}
void enter_type7(Ctx &ctx, TerrainGlobals &g, const TileP &t) {
  t.field(&TerrainTile::draw_path).set(0);
  type_walk(ctx, g, t, 7, -13.0f, -13.0f);
  dirty_neighbourhood(ctx, t, g_negctl == 18 ? -1 : 7);
  reblend_neighbourhood(ctx, g, t);
}
void set_path_flag_for_type(Ctx &ctx, TerrainGlobals &g, const TileP &t, int32_t type) {
  if (t.field(&TerrainTile::has_path).get()) {
    const bool clear = type == 0x14 || type == 0x11 || (type == 0x16 && g_negctl != 15) || type == 0 || type == 2 || type == 1 ||
                       type == 3 || type == 7 || type == 9;
    t.field(&TerrainTile::draw_path).set(clear ? 0 : 1);
  }
  reblend_neighbourhood(ctx, g, t);
}
void set_type_id(Ctx &ctx, TerrainGlobals &g, const TileP &t, int32_t type) {
  if (t.field(&TerrainTile::has_path).get()) t.field(&TerrainTile::draw_path).set(1);
  if (t_type(t) == 7 && g_negctl != 17) leave_type7(ctx, g, t);
  if (t_type(t) == 6) leave_type6(ctx, g, t);
  else if (t_type(t) == 0x11) t.field(&TerrainTile::normals_dirty).set(1);
  t.field(&TerrainTile::type).set(type);
  switch (type) {
    case 7: enter_type7(ctx, g, t); break;
    case 0x11: set_path_flag_for_type(ctx, g, t, type); break;
    case 6: enter_type6(ctx, g, t); break;
    case 0xd: case 0xe: case 0xf: case 0x10: set_path_flag_for_type(ctx, g, t, 0xd); break;
    case 0: case 9: tile_reset_face_textures(ctx, g, t.va()); break;
    case 4: case 0x14: reblend_neighbourhood(ctx, g, t); break;
    default: set_path_flag_for_type(ctx, g, t, type); break;
  }
}

}  // namespace

void tile_elevate_corner(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t corner) { corner_move(ctx, g, tile_p(ctx, tile), corner, 1); }
void tile_lower_corner(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t corner) { corner_move(ctx, g, tile_p(ctx, tile), corner, -1); }
void tile_leave_type6(Ctx &ctx, TerrainGlobals &g, uint32_t tile) { leave_type6(ctx, g, tile_p(ctx, tile)); }
void tile_leave_type7(Ctx &ctx, TerrainGlobals &g, uint32_t tile) { leave_type7(ctx, g, tile_p(ctx, tile)); }
void tile_enter_type6(Ctx &ctx, TerrainGlobals &g, uint32_t tile) { enter_type6(ctx, g, tile_p(ctx, tile)); }
void tile_enter_type7(Ctx &ctx, TerrainGlobals &g, uint32_t tile) { enter_type7(ctx, g, tile_p(ctx, tile)); }
void tile_set_path_flag_for_type(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type) {
  set_path_flag_for_type(ctx, g, tile_p(ctx, tile), type);
}
void tile_set_type_id(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type) { set_type_id(ctx, g, tile_p(ctx, tile), type); }

void terrain_set_type(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, uint32_t tile, int32_t type, int32_t rotation) {
  // m_types[type].variations: the type index is not checked
  const int32_t count = VPtr<int32_t>(ctx.mem, terrain + 0x2cu + static_cast<uint32_t>(type) * 0x18u + 0x14u).get();
  const TileP t = tile_p(ctx, tile);
  if (count > 0) t.field(&TerrainTile::variation).set(terrain_rand(g) % count);
  else t.field(&TerrainTile::variation).set(0);
  t.field(&TerrainTile::rotation).set(rotation);
  set_type_id(ctx, g, t, type);
}

void tile_lay_path(Ctx &ctx, uint32_t tile, bool on, int32_t dir) {
  const TileP t = tile_p(ctx, tile);
  if (on) {
    t.field(&TerrainTile::has_path).set(1);
    if (dir && g_negctl != 20) t.field(&TerrainTile::connected).set(1);
    const int32_t ty = t_type(ctx, tile);
    const bool clear = ty == 0x16 || ty == 0 || ty == 2 || ty == 1 || ty == 3 || ty == 7 || ty == 9;
    t.field(&TerrainTile::draw_path).set(clear ? 0 : 1);
  } else {
    t.field(&TerrainTile::has_path).set(0);
    t.field(&TerrainTile::connected).set(0);
    t.field(&TerrainTile::draw_path).set(0);
  }
}

void tile_set_wall(Ctx &ctx, uint32_t tile, int32_t side, int32_t height, uint8_t on) {
  const TileP t = tile_p(ctx, tile);
  (t.field(&TerrainTile::wall_on) + side).set(on);
  (t.field(&TerrainTile::wall_height) + side).set(height);
}

void terrain_set_view_angle(TerrainGlobals &g, float angle) {
  if (angle == 0.0f) g.view_index = 0;
  else if (angle == 90.0f) g.view_index = g_negctl == 21 ? 2 : 1;
  else if (angle == 180.0f) g.view_index = 2;
  else g.view_index = 3;
}

void terrain_reset_terrain(Ctx &ctx, TerrainGlobals &g, uint32_t terrain) {
  const auto h = hdr_p(ctx, terrain);
  const int32_t w = h.field(&TerrainObjectHeader::width).get(), ht = h.field(&TerrainObjectHeader::height).get();
  for (int32_t x = 0; x < w; ++x)
    for (int32_t y = 0; y < ht; ++y)
      tile_reset(ctx, terrain + kTerrainTilesOffset + static_cast<uint32_t>(x + y * w) * static_cast<uint32_t>(sizeof(TerrainTile)), x, y);
  terrain_build_arrays(ctx, g, terrain);
}

// ------------------------------------------------------------------------------------------------------ exported methods

bool SetupTee::gen_textures(int32_t n, uint32_t *names) {
  const bool r = a_->gen_textures(n, names);
  if (b && n > 0) {
    std::vector<uint32_t> other(static_cast<size_t>(n), 0);
    const bool rb = b->gen_textures(n, other.data());
    if (rb != r || (r && !std::equal(other.begin(), other.end(), names))) names_differ = true;
  }
  return r;
}

namespace {
// Development switch SG_TERRAIN_STATE_LOG=<file> (roadmap S3.10b, U-1388): the state calls between two draw passes are timed per
// method, and the first draw pass after them appends one line: `calls <n> us <total> | <method> <count> <us> ...`; with
// SG_TERRAIN_STATE_CYCLES=1 as well, the line also has `kcyc <thread kilocycles>` after the total (QueryThreadCycleTime around each
// call: cycles this thread ran, unaffected by preemption, for comparing builds on a loaded machine; the two reads add a constant
// per call). Off (no clock read) when the variable is unset; both are read once per process.
struct StateProfile {
  struct Row {
    std::string name;
    uint64_t count = 0;
    double us = 0.0;
  };
  std::vector<Row> rows;
  uint64_t calls = 0, cycles = 0;
  double us = 0.0;
  void add(const std::string &name, double t, uint64_t cyc) {
    ++calls;
    us += t;
    cycles += cyc;
    for (Row &r : rows)
      if (r.name == name) {
        ++r.count;
        r.us += t;
        return;
      }
    rows.push_back(Row{name, 1, t});
  }
  void flush(const char *file) {
    if (calls == 0) return;
    if (FILE *f = std::fopen(file, "a")) {
      std::fprintf(f, "calls %llu us %.0f", static_cast<unsigned long long>(calls), us);
      if (cycles) std::fprintf(f, " kcyc %llu", static_cast<unsigned long long>(cycles / 1000));
      std::fprintf(f, " |");
      for (const Row &r : rows) std::fprintf(f, " %s %llu %.0f", r.name.c_str(), static_cast<unsigned long long>(r.count), r.us);
      std::fprintf(f, "\n");
      std::fclose(f);
    }
    rows.clear();
    calls = 0;
    cycles = 0;
    us = 0.0;
  }
};
const char *state_log_file() {
  static const char *const f = [] {
    const char *e = std::getenv("SG_TERRAIN_STATE_LOG");
    return (e && *e) ? e : nullptr;
  }();
  return f;
}
bool state_cycles() {
  static const bool on = [] {
    const char *e = std::getenv("SG_TERRAIN_STATE_CYCLES");
    return e && *e == '1';
  }();
  return on;
}
uint64_t thread_cycles() {
#ifdef _WIN32
  ULONG64 c = 0;
  QueryThreadCycleTime(GetCurrentThread(), &c);
  return c;
#else
  return 0;
#endif
}
StateProfile &state_profile() {
  static StateProfile p;
  return p;
}
uint32_t terrain_dll_call_body(Ctx &ctx, TerrainGlobals &g, TerrainDll &dll, const std::string &name, uint32_t self, const uint32_t *a,
                               const int32_t *table, int32_t n);
}  // namespace

uint32_t terrain_dll_call(Ctx &ctx, TerrainGlobals &g, TerrainDll &dll, const std::string &name, uint32_t self, const uint32_t *a,
                          const int32_t *table, int32_t n) {
  // the object view for this call (getInstance included: the host maps the object before it); the object is the DLL's, wherever
  // the method's ecx points
  const ViewScope view(ctx.mem, dll.terrain != 0 ? dll.terrain : self);
  const char *log = state_log_file();
  if (!log) return terrain_dll_call_body(ctx, g, dll, name, self, a, table, n);
  const bool pass = name == "render" || name == "localRender" || name == "stripRender" || name == "pathUpdateRender";
  if (pass) state_profile().flush(log);
  const bool cyc = state_cycles();
  const uint64_t c0 = cyc ? thread_cycles() : 0;
  const auto t0 = std::chrono::steady_clock::now();
  const uint32_t r = terrain_dll_call_body(ctx, g, dll, name, self, a, table, n);
  const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
  if (!pass) state_profile().add(name, us, cyc ? thread_cycles() - c0 : 0);
  return r;
}

namespace {
uint32_t terrain_dll_call_body(Ctx &ctx, TerrainGlobals &g, TerrainDll &dll, const std::string &name, uint32_t self, const uint32_t *a,
                               const int32_t *table, int32_t n) {
  auto i32 = [&](int k) { return static_cast<int32_t>(a[k]); };
  auto fl = [&](int k) {
    float f;
    std::memcpy(&f, &a[k], 4);
    return f;
  };
  auto sync = [&]() {
    dll.sink.text.clear();
    terrain_apply_gl_state(ctx, g, self, dll.st);
  };
  // the three methods a course rebuild calls thousands of times first (each test returns; the names are distinct)
  if (name == "elevateCorner") {
    if (a[0]) tile_elevate_corner(ctx, g, a[0], i32(1));
    return 0;
  }
  if (name == "setType") {
    terrain_set_type(ctx, g, self, a[0], i32(1), i32(2));
    return 0;
  }
  if (name == "calcNormals") {
    terrain_calc_normals(ctx, g, a[0]);
    return 0;
  }
  if (name == "getInstance") {
    if (!dll.constructed) {
      terrain_construct(ctx, g, dll.terrain, 50, 50);
      terrain_gl_construct(ctx, dll.setup, dll.st);
      terrain_apply_gl_state(ctx, g, dll.terrain, dll.st);
      dll.sink.text.clear();
      terrain_load_dll_constants(ctx, g);   // the module's constant tables (static data: there before getInstance runs)
      dll.constructed = true;
    }
    return dll.terrain;
  }
  if (name == "passCollarInfo") {
    terrain_pass_collar_info(g, table, n);
    return 0;
  }
  if (name == "initSystem") {
    if (VPtr<uint32_t>(ctx.mem, self).get() != 0) return 0;
    if (a[2] == 0) throw MissingResource{"initSystem without a display surface"};
    terrain_gl_init_system(ctx, dll.setup, dll.st, i32(0), i32(1), (a[3] & 0xffu) != 0);
    sync();
    return 0;
  }
  if (name == "initTerrain") {
    terrain_init_terrain(ctx, g, self);
    return 0;
  }
  if (name == "resetTerrain") {
    if (g_negctl != 22) terrain_reset_terrain(ctx, g, self);
    return 0;
  }
  if (name == "loadNewCourseType") {
    dll.st.course_type = g.course_type;
    terrain_gl_load_new_course_type(ctx, dll.setup, dll.st, i32(0));
    sync();
    return 0;
  }
  if (name == "setZoomLevel") {
    terrain_gl_set_zoom_level(dll.setup, dll.st, i32(0));
    sync();
    return 0;
  }
  if (name == "lowerCorner") {
    if (a[0]) tile_lower_corner(ctx, g, a[0], i32(1));
    return 0;
  }
  if (name == "lowerEdgeCorner") {
    if (a[0]) {
      // renderTileAtFiveOffsets(tile, other, amount) first: it draws, reads the pivot's position and sets the view quadrant
      (void)tile_p(ctx, a[2]).field(&TerrainTile::x).get();
      terrain_set_view_angle(g, fl(3));
      tile_lower_corner(ctx, g, a[0], i32(1));
    }
    return 0;
  }
  if (name == "calcAllNormals") {
    terrain_calc_all_normals(ctx, g, self, a[0]);
    return 0;
  }
  if (name == "setWall") {
    tile_set_wall(ctx, a[0], i32(1), i32(2), static_cast<uint8_t>(g_negctl == 19 ? (a[3] != 0 ? 1u : 0u) : (a[3] & 0xffu)));
    return 0;
  }
  if (name == "layPath") {
    tile_lay_path(ctx, a[0], a[1] != 0, i32(2));
    return 0;
  }
  if (name == "updatePath") {
    const uint32_t t = terrain_tile_at(ctx, self, i32(0), i32(1));
    tile_p(ctx, t).field(&TerrainTile::connected).set(a[2] != 0 ? 1 : 0);
    return 0;
  }
  if (name == "setSplineHeight") {
    g.corner_step = fl(0);
    return 0;
  }
  // the draw passes: their state effect is the view quadrant (the draw list they fill is empty again when they return)
  if (name == "render") {
    terrain_set_view_angle(g, fl(1));
    return 1;
  }
  if (name == "localRender" || name == "stripRender") {
    terrain_set_view_angle(g, fl(2));
    return 0;
  }
  if (name == "pathUpdateRender" || name == "drawLine" || name == "drawCircle" || name == "drawCardinalSpline" ||
      name == "drawBezierSpline" || name == "renderTile" || name == "resize")
    return 0;
  throw MissingResource{"terrain_dll_call: method not ported"};
}
}  // namespace

// No sg_call functions: the state side works on the host's Terrain object and globals through its own C entry points (sg_ts_*).
extern const ModuleDef kModule_terrain_state = {"terrain_state", nullptr, 0};

}  // namespace portsim
