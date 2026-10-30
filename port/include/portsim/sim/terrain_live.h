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
  LiveTerrainView &view() { return view_; }
  bool setup_names_differ() const { return dll_.setup.names_differ; }
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
  LiveTerrainView view_;
  bool to_gfx(const char *name, std::initializer_list<uint32_t> args);
  uint32_t terrain_tile_at_exe(int32_t x, int32_t y);
};

// updateTerrainRender's bracket for the attached TerrainApi when it is a LiveTerrain (no effect otherwise)
void terrain_live_frame(Ctx &ctx, bool begin);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_LIVE_H
