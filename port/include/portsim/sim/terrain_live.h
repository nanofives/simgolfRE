// A TerrainApi that keeps Terrain.dll's state itself (phase 5 step 3; plan re/PHASE5_TERRAIN_PLAN.md section 14): every Terrain
// method the exe calls goes to terrain_dll_call (terrain_state.h) on a Terrain object the port builds in the context's address space,
// so the exe-side callers (terrain_dll.cpp, the boot path, the frame loop) produce the object, its arrays and its texture table the
// renderer draws from. The engine services of the interface (input pump, window paint, blits, operator new for the render list) are
// those of a headless run (tick.h make_headless_terrain).
//
// For the comparison with a recording of the original (re/tools/terrain_calls.py --scenario): every call is logged in the recorder's
// line form, and copies of the state can be kept after a given number of calls.
#ifndef PORTSIM_SIM_TERRAIN_LIVE_H
#define PORTSIM_SIM_TERRAIN_LIVE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_state.h"

namespace portsim {

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
    uint32_t after = 0;               // calls logged before it
    std::vector<uint8_t> object;
    std::vector<float> vertices, normals;
    std::vector<uint32_t> textures;
    int32_t view_index = 0;
  };
  const std::vector<std::string> &log() const { return log_; }
  const std::vector<Snapshot> &snapshots() const { return snaps_; }
  uint32_t object_va() const { return dll_.terrain; }
  TerrainGlobals &globals() { return g_; }

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
  std::vector<std::string> log_;
};

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_LIVE_H
