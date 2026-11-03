// LiveTerrain: the TerrainApi over the port's own Terrain.dll state (terrain_live.h). Tests: port/test/test_terrain_live.py, which
// boots the port on a pinned scenario's trace with this interface attached and compares the calls and the state with a recording
// of the original on the same scenario (re/tools/terrain_calls.py --scenario).
#define _CRT_SECURE_NO_WARNINGS  // the development switch SG_LIVE_GETTERS uses getenv
#include "portsim/sim/terrain_live.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

// P8B: the GL setup's fan to the primary sink (the 800 x 600 rasteriser, or SetupPair with the host's OpenGL) and the world layer's
// rasteriser (LiveTerrainWorld). Every call goes to both in the same order; the world's viewport is the setup's mapped into the
// world's pixels (x * fw / 800 + ox, y * ph / 600); texture names are the primary's (the world's must come out equal: both
// rasterisers start empty and hand out the lowest free name, gl_raster.h; a difference turns the world layer off).
class WorldFan : public gl::GlSink {
 public:
  WorldFan(gl::GlSink *a, LiveTerrainWorld &w) : a_(a), w_(w), b_(w.raster.get()) {}
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
  gl::GlSink *a_;
  LiveTerrainWorld &w_;
  gl::GlRaster *b_;
  int32_t vp_[4] = {0, 0, 0, 0};
  bool has_vp_ = false;
};

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
  w.back.assign(static_cast<size_t>(pw) * static_cast<size_t>(ph), 0);
}
}  // namespace

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
  world_geometry(*world_, pw, ph);
  if (world_fan_) static_cast<WorldFan *>(world_fan_.get())->remap();
  world_->valid = false;
  world_->dirty = true;
  world_->strip_full = false;
  ++world_->resizes;
  return true;
}

bool LiveTerrain::world_frame(const std::vector<uint16_t> &back800, bool back_terrain, std::vector<uint16_t> &px,
                              std::vector<uint8_t> &mask) const {
  if (!world_ || world_->names_differ || !world_->valid || !back_terrain) return false;
  if (back800.size() != world_->terrain800.size()) return false;
  px = world_->raster->pixels();
  mask.resize(back800.size());
  const LiveTerrainWorld &w = *world_;
  if (!w.scene_valid || w.s1.size() != back800.size() || w.scene.size() != static_cast<size_t>(w.scene_w) * 600) {
    for (size_t i = 0; i < back800.size(); ++i) mask[i] = back800[i] == w.terrain800[i] ? 1 : 0;
    return true;
  }
  // G32: the wide scene's sprite pixels over the world (nearest: world pixel (x, y) shows scene pixel
  // (floor((x - ox) * 800 / fw) + scene_e, floor(y * 600 / ph)))
  std::vector<int32_t> col(static_cast<size_t>(w.pw));
  for (int32_t x = 0; x < w.pw; ++x) {
    const int64_t d = static_cast<int64_t>(x - w.ox) * 800;
    const int64_t q = d >= 0 ? d / w.fw : -((-d + w.fw - 1) / w.fw);
    col[static_cast<size_t>(x)] = static_cast<int32_t>(q) + w.scene_e;
  }
  for (int32_t y = 0; y < w.ph; ++y) {
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y) * 600 / w.ph);
    const uint16_t *sr = w.scene.data() + static_cast<size_t>(ly) * static_cast<size_t>(w.scene_w);
    const uint16_t *br = w.scene_base.data() + static_cast<size_t>(ly) * static_cast<size_t>(w.scene_w);
    uint16_t *dst = px.data() + static_cast<size_t>(y) * static_cast<size_t>(w.pw);
    for (int32_t x = 0; x < w.pw; ++x) {
      const int32_t lx = col[static_cast<size_t>(x)];
      if (lx >= 0 && lx < w.scene_w && sr[lx] != br[lx]) dst[x] = sr[lx];
    }
  }
  const bool s0_ok = w.s0.size() == back800.size();
  for (size_t i = 0; i < back800.size(); ++i)
    mask[i] = back800[i] == w.terrain800[i] || (back800[i] == w.s1[i] && (!s0_ok || w.s1[i] != w.s0[i])) ? 1 : 0;
  return true;
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
