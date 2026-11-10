// A TerrainApi that keeps Terrain.dll's state itself (phase 5 step 3; plan re/PHASE5_TERRAIN_PLAN.md section 14): every Terrain
// method the exe calls goes to terrain_dll_call (terrain_state.h) on a Terrain object the port builds in the context's address space,
// so the exe-side callers (terrain_dll.cpp, the boot path, the frame loop) produce the object, its arrays and its texture table the
// renderer draws from. The engine services of the interface (input pump, window paint, blits, operator new for the render list) are
// those of a headless run (tick.h make_headless_terrain).
//
// For the comparison with a recording of the original (re/tools/terrain_calls.py --scenario): every call is logged in the recorder's
// line form, and copies of the state can be kept after a given number of calls.
//
// Drawing (roadmap S3.2): the original's GL context draws into the root window's surface (initTerrain hands initSystem the DC of the
// jgld surface of the root window), and the exe keeps the finished terrain picture in its back surface (gameMain's image object
// g_image51b068: updateTerrainRender copies the root surface there after every pass and back when the camera has not moved). Here the
// GL setup also goes to a rasteriser (gl::GlRaster, the TerrainDll's SetupTee), and when a graphics backend is attached to the context
// (gfx.h) the draw passes, Window::paint of the root window, the two whole-surface copies and renderTerrainStrip's shifted copy are
// handed to it by drop name (the body family `terrain`, gfx_terrain.cpp), which draws them into the backend's root surface. None of
// it writes the GameState or the address space: the simulation is the same with or without a backend.
#ifndef PORTSIM_SIM_TERRAIN_LIVE_H
#define PORTSIM_SIM_TERRAIN_LIVE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <set>

#include "portsim/registry.h"
#include "portsim/sim/gl_raster.h"
#include "portsim/sim/gl_wgl.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_state.h"

namespace portsim {

// What the terrain body family keeps per LiveTerrain (gfx_terrain.cpp): the exe's back surface, the frame count, and the frames
// whose root surface the harness wants copied (sg_call terrainLive.keepFrame(k), terrain_live.cpp).
struct LiveTerrainView {
  std::vector<uint16_t> back_surface;   // the exe's back surface (RGB555, rows top-down like the root surface), made at first use
  int32_t back_w = 0, back_h = 0;
  int back_terrain = 0;                 // the back surface holds a terrain picture
  std::set<uint32_t> keep_frames;       // updateTerrainRender ordinals (from 0, every call of the process) to copy
  uint32_t frames = 0;                  // updateTerrainRender calls begun
  bool in_frame = false;
  int32_t passes_in_frame = 0;
  int64_t passes = 0, pixels_drawn = 0; // passes rasterised, pixels they wrote (GlRaster stats)
};

// P8B (widescreen stage 2, log/roadmap/P8/P8B_notes.md): the WORLD LAYER, a second rasteriser of pw x ph pixels that gets the same
// GL setup (viewports mapped, x clip planes moved out: gl_raster.h set_clip_x) and every terrain pass the 800 x 600 one gets, with
// isCulled's x bounds widened by `margin` (terrain_render.h). Outside the GameState and the address space: nothing a pass reads
// changes (TerrainGlobals::view_index is put back after each world pass). The presenter composes it under the 800 frame.
struct LiveTerrainWorld {
  int32_t pw = 0, ph = 0;      // the world layer's pixels
  int32_t fw = 0, ox = 0;      // the 800 x 600 frame's width (= ph * 800 / 600) and left edge in world pixels
  int32_t margin = 0;          // isCulled's side margin in the frame's logical pixels
  std::unique_ptr<gl::GlRaster> raster;   // its pixels() are the world's root surface
  // the world's copy of the exe's back surface (P15A fix 2: shared and never written in place, so a copy of the root taken while
  // the root equals it is this buffer: b_queue_begin's root for the world thread)
  std::shared_ptr<const std::vector<uint16_t>> back;
  std::vector<uint16_t> terrain800;       // T: the 800 frame's terrain picture (the root surface after each terrain operation;
                                          // a pass updates only the pixels it changed)
  bool valid = false;          // a pass has drawn
  // P13D fix (item 7, log/roadmap/P13/P13D_notes.md): between a frame's terrain picture (a pass, windowPaint, the back surface put
  // back) and renderSpriteQueue's start the overlay lines (drawMapLine / lineTo / drawCircle: the hole's shot lines) are drawn a
  // second time on the world layer, at its resolution, so they go on past the 800 frame's columns (terrain_overlay.cpp)
  bool lines_open = false;
  bool strip_full = false;     // a strip pass was drawn as a full render: the next shifted copy of the back surface is skipped
  bool names_differ = false;   // a texture name of the world's rasteriser differed from the primary's (the layer is then off)
  // G32 (dynamic view): after resize_world the world is redrawn whole at the next updateTerrainRender return, from the pivot and
  // angle of the last full or strip pass (the camera's), when one has run
  bool dirty = false;
  bool has_last = false;
  uint32_t last_self = 0, last_pivot = 0, last_angle = 0;
  int64_t resizes = 0, redraws = 0;
  // G32: the WIDE SCENE (gfx_scene.cpp): every sprite-queue entry and golfer label drawn a second time into scene_w x 600 logical
  // pixels, the frame's 800 columns at scene_e, over scene_base (the root surface at the queue's start in those columns, the world
  // layer sampled down beside them); s1 = the root surface at the queue's end. With scene_valid, world_frame lays the scene's sprite
  // pixels over the world, and the frame's pixel p shows that world image where back[p] == T[p] (terrain) or back[p] == s1[p] !=
  // s0[p] (a pixel the queue drew); the rest (2D drawn before the queue in the frame, such as walker labels, and the HUD after it)
  // stays the frame's
  std::vector<uint16_t> scene, scene_base, s0, s1;   // s0 = the root surface at the queue's start
  // P9C: the scene with the glyphs of the text runs drawn into it (the golfer labels' second draw) put back, made at the queue's
  // end while text capture is on (empty = none); world_frame(..., text = true) lays it in place of `scene`
  std::vector<uint16_t> scene_text;
  // P9E fix 4 (gfx.h gfx_set_hud_cursor): the scene before the frame code's cursor passes (the first one's start) and after them
  // (the last one's end), cleared at the queue's start; where they differ the scene holds a tool preview drawn at the cursor
  std::vector<uint16_t> scene_pre, scene_cur;
  int32_t scene_w = 0, scene_e = 0;
  bool scene_valid = false;
  // P10D fix 1 (log/roadmap/P10/P10D_notes.md): the sprite queue at the world layer's resolution. With hr_on (the app's host,
  // --sprite-res native) every queue entry the wide scene draws between renderSpriteQueue's begin and end is drawn a third time
  // into hr (pw x ph, started from the world layer's pixels), each covered world pixel sampled from the sprite's own art
  // (bilinear, supersampled where the art is drawn smaller than one texel per world pixel) through the same pixel rule (palette,
  // plane / shade tables, alpha bodies); hr_cov = 1 where it drew. tag (scene_w x 600) = 1 where the scene's pixel was last
  // written by such a draw, 0 where by anything else (the golfer labels, the cursor passes); tag_pre = tag at the first cursor
  // pass (P9E fix 4's scene_pre). world_frame then lays hr where hr_cov and the scene's pixels only where tag is 0. Drawing only:
  // nothing a pass, the root or the GameState holds changes.
  bool hr_on = false;
  // P10H fix 1 (log/roadmap/P10/P10H_notes.md): the resampling filter of hr / hrc / hrb (gfx_scene.cpp hr_blit_rows). 0 = smooth
  // (P10D's bilinear), 1 = sharp (each art pixel a flat block at the window's scale with area-weighted edges one window pixel wide;
  // the object's outline against what is under it keeps the bilinear coverage), 2 = sharp-area (the outline area-weighted too; a
  // development comparison), 3 = edge (P13D fix 5: sharp over the art scaled up by Scale2x, gfx_scene.cpp hr_art). Set once with hr_on
  // (app --sprite-filter).
  int hr_filter = 0;
  // P13D item 4 (log/roadmap/P13/P13D_notes.md): the world rasteriser's magnification filter of the terrain's .bmp textures. The
  // original's textureLoad sets GL_NEAREST magnification on them (GL_LINEAR on the .tga path textures); at the window's resolution
  // a 64 x 64 tile texture is magnified up to 3.6 times (zoom 8 at 1080 rows), so every texel shows as a block. 0 = as the original,
  // 1 = GL_LINEAR on the world layer (the 800 frame keeps NEAREST), 2 = GL_LINEAR over the world's copy of each 2D texture scaled
  // 2 x 2 with the Catmull-Rom cubic (terrain_live.cpp upscale2_cubic). Set before the first Terrain call (app --terrain-filter).
  int tex_mag = 0;
  bool hr_open = false;    // between renderSpriteQueue.begin and .end
  bool hr_valid = false;   // hr holds the last queue's sprites
  std::vector<uint16_t> hr;
  std::vector<uint8_t> hr_cov, tag, tag_pre;
  // P10D fix 2: queue entries drawn after renderSpriteQueue's end (the frame code's cursor passes: the tool previews of P6_ToolLabel
  // and P7) are drawn at once at the world's resolution: inside a cursor pass kept apart (gfx.h gfx_set_hud_cursor) into hrc, a
  // copy of hr made at the first such draw (hrc_cov = 1 where they drew; tag 2 in the scene), any other into hr itself (tag 1).
  // world_cursor gives hrc's pixels for the presenter's cursor previews; world_frame without cursor_apart lays hrc over hr.
  std::vector<uint16_t> hrc;
  std::vector<uint8_t> hrc_cov;
  bool hrc_valid = false;
  // P10F (log/roadmap/P10/P10F_notes.md): P7's translucent placement box (a cursor pass's Surface_drawSprite at alpha 0.5, outside
  // the queue) at the world's resolution (gfx_scene.cpp draw_alpha). It is drawn into hrc (under the cursor sprites drawn after it;
  // hrc_cov is left to them) and into the blend pair hrb_lo / hrb_hi, the same box over black (0) and over white (0x7fff) words,
  // hrb_cov = 1 where it drew: world_cursor gives a box-only pixel as that pair, which the presenter blends over the pixel under it
  // wherever P10E moves the preview, so the box stays translucent away from the place it was drawn at. box800 = the 800 x 600 root
  // pixels it wrote and box_word the word it left there (world_cursor adds to mask800 those the published back buffer still holds
  // with that word, so the frame's 800-space box is left out but a line or text drawn over it later is not). All cleared at the queue's start
  // (hrb_valid false; the planes are kept and only hrb_area, the box's world rectangle since their last reset, is reset).
  std::vector<uint8_t> box800;
  std::vector<uint16_t> box_word;
  std::vector<uint16_t> hrb_lo, hrb_hi;
  std::vector<uint8_t> hrb_cov;
  bool hrb_valid = false;
  int32_t hrb_area[4] = {0, 0, 0, 0};
  // P17B (gfx.h GfxHrGpu): hr, hr_cov, hrc, hrc_cov and the hrb planes live in the thread's GPU backend (the vectors above are then
  // not kept, except beside it under SG_P17_WORLDCHECK); hr_lost: a backend failure left no picture in the planes (until the next
  // queue's flush on the CPU)
  bool hr_gpu = false;
  bool hr_lost = false;
  int64_t hr_draws = 0;
  int64_t hr_queues = 0, hr_prep_us = 0, hr_queue_us = 0, hr_label_us = 0, hr_flush_us = 0;   // P10D: costs (session.json)
  int64_t hr_tiles = 0, hr_tiles_dirty = 0;   // P10D fix 3: 32 x 32 tiles flushed, and of them drawn again (session.json)
  int64_t scene_passes = 0;
  int64_t passes = 0, pixels = 0, us = 0;
  // P15A (log/roadmap/P15/P15A_notes.md): counts every change of the world root's pixels (a world pass, windowPaint's fill, the back
  // surface put back, a mirrored overlay line, a resize or its redraw), so a copy of the root taken at a count is known current
  uint64_t gen = 0;
  // P15A fix 2: `back` equals the root since the root's count was back_gen (blit 1 / 0 copy only when it changed since)
  uint64_t back_gen = ~0ull;
  // P23D fix 1 (gfx.h gfx_set_wide_grid; terrain_overlay.cpp grid_*): the grid lines' frame-coherent skip. grid_prev: the grid lines
  // drawn on the root after it was made equal to `back` (back_gen grid_back_gen), the root's count after them grid_after_gen;
  // grid_prev_ok: the root is exactly back + grid_prev now. When the next frame's blit 0 finds nothing changed since (gen ==
  // grid_after_gen, back_gen == grid_back_gen), the root is left as it is (grid_defer) and the frame's lines are only compared
  // (grid_cur); the first that differs, or any other change of the root, puts the back and the lines so far on the root then.
  struct GridLine {
    int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    uint32_t colour = 0;
    bool operator==(const GridLine &o) const { return x1 == o.x1 && y1 == o.y1 && x2 == o.x2 && y2 == o.y2 && colour == o.colour; }
  };
  std::vector<GridLine> grid_prev, grid_cur;
  bool grid_prev_ok = false, grid_cur_ok = false, grid_defer = false, grid_matched = false;
  uint64_t grid_after_gen = 0, grid_back_gen = 0, grid_lines_gen = 0;
  int64_t copies_skipped = 0, skip_checked = 0, skip_differing = 0;   // fix 2 (session.json world p15a; SG_P15_SKIP_CHECK=1 compares)
  // P15A fix 3 (log/roadmap/P15/P15A_notes.md section 5; gfx.h gfx_set_world_raster_defer): THE WORLD RASTER ON THE WORLD THREAD.
  // With rec every surface operation of this world is also recorded into the frame thread's job: the setup's and the passes' GL
  // calls through `recorder` (a gl::GlRecord behind `tee`), the fills, copies, lines and sizes as gfx_world_op ops. The world
  // thread's own LiveTerrainWorld holds the full raster (made by the recorded kGwReset) and back_m (its back surface, written in
  // place). With mask_on this raster draws only the rows and side columns b_queue_begin samples (GlRaster::set_mask; mask_rows /
  // mask_cx0 / mask_cx1), so its other pixels, and back_m's (the masked back surface that replaces `back`), are not the world's.
  // The check (SG_P15_CHECK=1): raster stays whole and `sample` is a second raster, masked, fed the same operations (sample_back its
  // back); b_queue_begin compares their samples (mask_checks, mask_differing, mask_px_differing).
  bool rec = false, mask_on = false;
  std::unique_ptr<gl::GlRecord> recorder;
  std::unique_ptr<gl::GlSink> tee;   // raster + sample + recorder: the sink the world's setup and passes draw through
  std::unique_ptr<gl::GlRaster> sample;
  std::vector<uint16_t> back_m, sample_back;
  std::vector<uint8_t> mask_rows;
  int32_t mask_cx0 = 0, mask_cx1 = 0;
  int64_t mask_checks = 0, mask_differing = 0, mask_px_differing = 0;
  gl::GlSink &sink() { return tee ? *tee : static_cast<gl::GlSink &>(*raster); }
  // gate (sg_call terrainLive.worldCheck(1)): after every terrain operation of a world whose frame is 800 x 600 world pixels (render
  // scale 1), the world root's central 800 columns compared with T: operations checked, operations with a difference, pixels
  bool check_central = false;
  int64_t checks = 0, checks_differing = 0, diff_px = 0;
};

// P15A fix 3: the world rasteriser's pixel-loop workers (gfx_terrain.cpp; the frame thread's and the world thread's rasterisers)
int terrain_world_raster_threads();
// P15A fix 3 (gfx.h GfxWorldOp, gfx_scene.cpp): frame thread: the GL stream at the end of the job being recorded (a new kGwGl op when
// the last op is another kind)
gl::GlRec *gfx_world_gl_chunk();
// P23D fix 2 (log/roadmap/P23/P23D_notes.md): frame thread: the overlay lines at the end of the job being recorded (a new kGwLines op
// when the last op is another kind). The op draws them on the world thread's raster in order as one patch (terrain_overlay.cpp
// world_lines_run: no read back of a frame the GPU holds), each as line_px_body draws it.
struct GfxWorldLine {
  double x1 = 0, y1 = 0, x2 = 0, y2 = 0;   // the 800-space ends as line_px_body takes them
  uint32_t colour = 0;
  int32_t alpha = 0;
  float line_width = 1.0f;
  bool flip_y = true;
  int32_t screen_h = 600;
  int32_t pw = 0, ph = 0, fw = 0, ox = 0;   // the world it was drawn for
};
std::vector<GfxWorldLine> *gfx_world_lines_chunk();
void world_lines_run(LiveTerrainWorld &w, const std::vector<GfxWorldLine> &lines);
// P15A fix 3: a world rasteriser's size-dependent state (the x clip planes and the dither phase of a pw x ph world)
void terrain_world_raster_geometry(gl::GlRaster &r, int32_t pw, int32_t ph);

// P15A (log/roadmap/P15/P15A_notes.md): LiveTerrain::world_frame / world_cursor over a given world and a given copy of its root
// (the world thread's: the hr planes of its own LiveTerrainWorld, the root and the scene planes copied at the present). The same
// bodies; the LiveTerrain members call them with world() and the world rasteriser's pixels.
bool world_frame_of(const LiveTerrainWorld *w, const std::vector<uint16_t> &root, const std::vector<uint16_t> &back800, bool back_terrain,
                    std::vector<uint16_t> &px, std::vector<uint8_t> &mask, bool text, bool cursor_apart);
bool world_cursor_of(const LiveTerrainWorld *w, std::vector<uint16_t> &px, std::vector<uint8_t> &cov, std::vector<uint8_t> &mask800,
                     std::vector<uint16_t> *hi, const std::vector<uint16_t> *back800);
// P17B (log/roadmap/P17/P17B_notes.md): world_frame_of's work split for a GPU that holds the root and the hr planes (gfx.h GfxHrGpu).
// The same tests and loops, run up to the per-pixel copies: the mask (as world_frame_of writes it), whether hr / hrc are laid, and the
// runs as a table, so that px(x, y) = runs[ly * scene_w + lx] (bit 16 set: the low 16 bits) with ly = rowmap[y], lx = colmap[x] (-1:
// none), else hrc where lay_hrc and hrc_cov, else hr where lay_hr and hr_cov, else root, is world_frame_of's px. hr_n / hrc_n: the size
// of those planes (pw x ph when they exist), root_n the root's. false where world_frame_of returns false.
struct WorldFramePlan {
  bool lay_hr = false, lay_hrc = false;
  int32_t pw = 0, ph = 0, scene_w = 0;
  std::vector<uint32_t> runs;     // scene_w x 600, or empty (no runs: px = the laid planes over the root)
  std::vector<int32_t> colmap;    // pw
  std::vector<int32_t> rowmap;    // ph
};
bool world_frame_plan_of(const LiveTerrainWorld *w, size_t root_n, size_t hr_n, size_t hrc_n, const std::vector<uint16_t> &back800,
                         bool back_terrain, bool text, bool cursor_apart, WorldFramePlan &plan, std::vector<uint8_t> &mask);

class LiveTerrain : public TerrainApi {
 public:
  // va: where getInstance puts the object (0 = a free range of the address space). snap_after: call counts after which a copy
  // of the state is kept (the k-th logged call, counting from 1; 0 = before any call).
  LiveTerrain(Ctx &ctx, uint32_t va, std::vector<uint32_t> snap_after);
  ~LiveTerrain() override;

  uint32_t pass_collar_info(uint32_t self, const int32_t *table, int32_t n) override;
  uint32_t load_new_course_type(uint32_t self, int32_t type) override;
  uint32_t set_type(uint32_t self, uint32_t tile, int32_t type, int32_t variation) override;
  uint32_t get_variation(uint32_t self, uint32_t tile) override;
  uint32_t lay_path(uint32_t self, uint32_t tile, int32_t flag20, int32_t flag40) override;
  uint32_t set_wall(uint32_t self, uint32_t tile, int32_t side, int32_t height, int32_t on) override;
  uint32_t calc_normals(uint32_t self, uint32_t tile) override;
  uint32_t calc_all_normals(uint32_t self, uint32_t tile) override;
  uint32_t elevate_corner(uint32_t self, uint32_t tile, int32_t corner) override;
  uint32_t lower_corner(uint32_t self, uint32_t tile, int32_t corner) override;
  uint32_t lower_edge_corner(uint32_t self, uint32_t tile, int32_t corner, uint32_t tile2, uint32_t angle) override;
  uint32_t has_path(uint32_t self, uint32_t tile) override;
  uint32_t has_connected_path(uint32_t self, int32_t x, int32_t y) override;
  uint32_t update_path(uint32_t self, int32_t x, int32_t y, int32_t flag40) override;
  uint32_t init_system(uint32_t self, int32_t w, int32_t h, uint32_t hdc, int32_t flag) override;
  uint32_t init_terrain(uint32_t self) override;
  uint32_t set_zoom_level(uint32_t self, int32_t zoom) override;
  uint32_t render(uint32_t self, uint32_t tile, uint32_t angle) override;
  uint32_t local_render(uint32_t self, uint32_t tile, uint32_t centre, uint32_t angle) override;
  uint32_t path_update_render(uint32_t self, uint32_t tile, uint32_t angle) override;
  uint32_t os_major_version() override;
  uint32_t acquire_dc() override;
  uint32_t pump_input() override;
  uint32_t window_paint() override;
  uint32_t blit_full() override;
  uint32_t blit_screen(int32_t w, int32_t h) override;
  uint32_t render_strip(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t rot, int32_t zoom) override;
  uint32_t operator_new(uint32_t size) override;
  uint32_t get_instance() override;
  uint32_t reset_terrain(uint32_t self) override;

  struct Snapshot {
    uint32_t after = 0;               // calls made before it (calls())
    std::vector<uint8_t> object;
    std::vector<float> vertices, normals;
    std::vector<uint32_t> textures;
    int32_t view_index = 0;
  };
  // the recorder's lines, formatted on demand from the compact records `call` keeps (roadmap S3.10b: formatting each line as the
  // call ran cost about 1 us per call, 14 ms of a zoom key's 14,804 calls)
  const std::vector<std::string> &log() const;
  // the calls logged since the last drop_log (the lines `log()` would hold), without formatting them; drop_log empties the log (the
  // next call's seq is 0 again, as when a reader emptied the vector `log()` returns)
  size_t log_size() const { return log_.size() + recs_.size(); }
  void drop_log();
  // roadmap S7.8 / S3.12 (U-1673): false = record no call at all (a play session: nothing reads the lines; the app's
  // TerrainLogBound sets it); calls() counts every call either way, and the snapshots are placed by it
  void set_keep_log(bool keep) { keep_log_ = keep; }
  uint64_t calls() const { return calls_; }
  const std::vector<Snapshot> &snapshots() const { return snaps_; }
  uint32_t object_va() const { return dll_.terrain; }
  TerrainGlobals &globals() { return g_; }
  // drawing (roadmap S3.2)
  gl::GlRaster &raster() { return *raster_; }
  // roadmap S3.15 (U-1810): the host's own OpenGL beside the rasteriser (gl_wgl.h), made before the setup's first call (the
  // constructor reads SG_GL_BACKEND=wgl; true = in use). host_gl() is null when it is not in use or the host has none.
  bool use_host_gl(bool on);
  gl::WglSink *host_gl() { return wgl_ && wgl_->ok() && wgl_->has_context() ? wgl_.get() : nullptr; }
  const std::string host_gl_why() const { return wgl_ ? wgl_->why() : std::string("not selected"); }
  // roadmap S3.20 (decision G13, final: GlRaster is the default, the host's OpenGL opt-in): the terrain renderer of this LiveTerrain,
  // chosen before its first call. mode "raster" = GlRaster (the default); "wgl" or "host" = the host's OpenGL without the check
  // (S3.15's opt-in); "auto" = the host's OpenGL when it exists and the startup self-check (gl_wgl.h wgl_self_check) draws the
  // original's reference pixels, else GlRaster. The constructor applies SG_GL_BACKEND (raster | wgl | host | auto) when it is set;
  // an unset variable leaves GlRaster.
  struct RendererChoice {
    std::string mode = "raster";     // what was asked
    std::string backend = "raster";  // what draws: "wgl" (the host's OpenGL) or "raster" (GlRaster)
    std::string reason = "the default (no choice made)";
    bool checked = false;            // the self-check ran
    gl::SelfCheck check;
  };
  const RendererChoice &choose_renderer(const std::string &mode);
  const RendererChoice &renderer() const { return choice_; }
  LiveTerrainView &view() { return view_; }
  bool setup_names_differ() const { return dll_.setup.names_differ; }
  // P8B: the world layer of pw x ph pixels (section above). Before the first Terrain call (the setup must reach it from the
  // start); false (with `why`) otherwise or for a size smaller than the 800 x 600 frame's aspect allows (pw < ph * 4 / 3).
  bool set_world_view(int32_t pw, int32_t ph, std::string *why = nullptr);
  LiveTerrainWorld *world() { return world_ && !world_->names_differ ? world_.get() : nullptr; }
  // G32: a new size for the world layer (the window was resized): framebuffer, viewport mapping, clip planes, dither phase and cull
  // margin follow; the picture is redrawn whole at the next updateTerrainRender return (LiveTerrainWorld::dirty). False without one.
  bool resize_world(int32_t pw, int32_t ph);
  // The world layer for a presented frame: px = its pixels (pw * ph), mask[p] = 1 where the frame's pixel p (800 x 600) shows the
  // world (it equals the terrain picture T there). False (nothing written) without a world layer, before its first pass, or when
  // the frame holds no terrain (back_terrain 0: the 2D screens and full-screen overlays are shown as before).
  bool world_frame(const std::vector<uint16_t> &back800, bool back_terrain, std::vector<uint16_t> &px, std::vector<uint8_t> &mask,
                   bool text = false,  // P9C: text = the scene without its text runs' glyphs (scene_text)
                   bool cursor_apart = false) const;  // P9E fix 4: the scene without the cursor passes' previews (scene_pre there)
  // P9E fix 4: the cursor passes' previews alone: px / cov (pw x ph) the world pixels they show (nearest, as world_frame lays the
  // scene), mask800 (800 x 600) the frame's pixels they drew (the scene's frame columns). False when the last queue pass kept none.
  // P10F: with `hi`, a pixel of the placement box alone has cov 2, px = the box over a black word and (*hi)[j] = over a white one
  // (LiveTerrainWorld::hrb_lo / hrb_hi); without `hi` such a pixel is hrc's (the box over the world it was drawn on), cov 1.
  // With `back800` (the published 800 x 600 back buffer) a box pixel joins mask800 only where it still holds the box's word.
  bool world_cursor(std::vector<uint16_t> &px, std::vector<uint8_t> &cov, std::vector<uint8_t> &mask800,
                    std::vector<uint16_t> *hi = nullptr, const std::vector<uint16_t> *back800 = nullptr) const;
  // updateTerrainRender's entry and return (terrain_dll.cpp): the frame bracket of the drawing
  void frame_begin();
  void frame_end();

 private:
  uint32_t call(const char *name, uint32_t self, std::initializer_list<uint32_t> args, const int32_t *table = nullptr,
                int32_t n = 0);
  void keep_snapshots();
  Ctx &ctx_;
  std::unique_ptr<TerrainApi> engine_;
  std::vector<uint8_t> object_;
  TerrainGlobals g_;
  TerrainDll dll_;
  std::vector<uint32_t> snap_after_;
  size_t next_snap_ = 0;
  std::vector<Snapshot> snaps_;
  struct LogRec {
    uint64_t seq;                     // the line's number (calls logged before it since the last drop)
    const char *name;                 // a string literal of this file's call sites
    uint32_t self, a[5], ret;
    uint8_t k;                        // arguments given (the others print `-`)
    bool has_var;                     // setType with a tile: the variation it drew, read when the call returned
    int32_t var;
    std::vector<int32_t> tbl;         // passCollarInfo's table (`tbl=` when `has_tbl`)
    bool has_tbl;
  };
  mutable std::vector<LogRec> recs_;       // calls not formatted yet (log() moves them into log_)
  mutable std::vector<std::string> log_;
  bool keep_log_ = true;
  uint64_t calls_ = 0;
  std::unique_ptr<gl::GlRaster> raster_;
  std::unique_ptr<gl::WglSink> wgl_;
  std::unique_ptr<gl::SetupPair> pair_;
  std::unique_ptr<LiveTerrainWorld> world_;   // P8B
  std::unique_ptr<gl::GlSink> world_fan_;     // P8B: the setup's fan to the primary sink and the world's rasteriser
  RendererChoice choice_;
  LiveTerrainView view_;
  bool to_gfx(const char *name, std::initializer_list<uint32_t> args);
  uint32_t terrain_tile_at_exe(int32_t x, int32_t y);
};

// updateTerrainRender's bracket for the attached TerrainApi when it is a LiveTerrain (no effect otherwise)
void terrain_live_frame(Ctx &ctx, bool begin);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_LIVE_H
