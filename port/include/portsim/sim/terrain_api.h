// The Terrain.dll boundary of the simulation core (phase 3.1a, 2026-10-19; log/p3/p31a_notes.txt).
//
// golf_clean.exe keeps its own copy of the course in GameState grids (types, flags, levels) and pushes it into
// Terrain.dll's `Terrain` object for the 3D view: setType / elevateCorner / layPath / setWall per tile, render calls per
// frame. Measured for every exe function that calls Terrain.dll (applyCourseType, togglePath, rebuildTerrainGrid,
// applyTileElevation, initTerrain, rebuildTileData, updateTerrainRender, applyTileVariation, applyTileWalls): no
// simulation-class GameState write depends on a value Terrain.dll returns. Their writes outside Terrain.dll are the
// engine-class bytes g_terrain820f28 / g_terrain820f2a / g_terrain820f30, the pointer-class render list gTileBuffer, and
// updateTerrainRender's sim-class camera memory (g_terrainInitPending, g_terrainLastCamX / Y, g_terrainLastZoom, from its
// arguments) and g_tileWallMask[0] bit 1 (its "tile data rebuilt" flag, cleared by rebuildHeightfield).
// Terrain.dll's state (U-0607) therefore feeds rendering only; the port models it as this interface, which a renderer
// backend (phase 4) implements, and which tests implement with a recording fake (terrain_fake.cpp).
//
// Like the original, the Terrain object and its tiles live in the context's 32-bit address space: GameState::gTerrain
// holds the object's virtual address; the exe reads its fields directly (width +0x14, height +0x18, tile array +0x3a4
// with 0x248-byte tiles: the tile's type +0x24, corner heights +4 + 8 * k, wall bytes +0x234 + side), so the port reads
// them through Ptr like the exe and passes Tile pointers as virtual addresses. Every method returns what the original
// leaves in eax (callers return it on). Float arguments are their 32-bit IEEE patterns.
#ifndef PORTSIM_SIM_TERRAIN_API_H
#define PORTSIM_SIM_TERRAIN_API_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

class TerrainApi {
 public:
  virtual ~TerrainApi() = default;
  // Terrain.dll's methods the exe imports (thiscall on the object at `self`); names as the decorated exports
  virtual uint32_t pass_collar_info(uint32_t self, const int32_t *table, int32_t n) = 0;  // table: n ints
  virtual uint32_t load_new_course_type(uint32_t self, int32_t type) = 0;
  virtual uint32_t set_type(uint32_t self, uint32_t tile, int32_t type, int32_t variation) = 0;
  virtual uint32_t get_variation(uint32_t self, uint32_t tile) = 0;
  virtual uint32_t lay_path(uint32_t self, uint32_t tile, int32_t flag20, int32_t flag40) = 0;
  virtual uint32_t set_wall(uint32_t self, uint32_t tile, int32_t side, int32_t height, int32_t on) = 0;
  virtual uint32_t calc_normals(uint32_t self, uint32_t tile) = 0;
  virtual uint32_t calc_all_normals(uint32_t self, uint32_t tile) = 0;
  virtual uint32_t elevate_corner(uint32_t self, uint32_t tile, int32_t corner) = 0;
  virtual uint32_t lower_corner(uint32_t self, uint32_t tile, int32_t corner) = 0;
  virtual uint32_t lower_edge_corner(uint32_t self, uint32_t tile, int32_t corner, uint32_t tile2, uint32_t angle) = 0;
  virtual uint32_t has_path(uint32_t self, uint32_t tile) = 0;                       // bool in al
  virtual uint32_t has_connected_path(uint32_t self, int32_t x, int32_t y) = 0;      // bool in al
  virtual uint32_t update_path(uint32_t self, int32_t x, int32_t y, int32_t flag40) = 0;
  virtual uint32_t init_system(uint32_t self, int32_t w, int32_t h, uint32_t hdc, int32_t flag) = 0;
  virtual uint32_t init_terrain(uint32_t self) = 0;
  virtual uint32_t set_zoom_level(uint32_t self, int32_t zoom) = 0;
  virtual uint32_t render(uint32_t self, uint32_t tile, uint32_t angle) = 0;
  virtual uint32_t local_render(uint32_t self, uint32_t tile, uint32_t centre, uint32_t angle) = 0;
  virtual uint32_t path_update_render(uint32_t self, uint32_t tile, uint32_t angle) = 0;
  // engine services the same functions use (exe code or the OS in the original; drawing and input, no sim state)
  virtual uint32_t os_major_version() = 0;            // GetVersionExA's dwMajorVersion (initTerrain)
  virtual uint32_t acquire_dc() = 0;                  // the display surface's DC (initTerrain)
  virtual uint32_t pump_input() = 0;                  // the input pump rebuildTerrainGrid runs once per column
  virtual uint32_t window_paint() = 0;                // Window::paint(root window, 0x80000000)
  virtual uint32_t blit_full() = 0;                   // the full back-buffer blit ending a render
  virtual uint32_t blit_screen(int32_t w, int32_t h) = 0;   // the screen-to-window blit of an unmoved camera
  virtual uint32_t render_strip(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t rot, int32_t zoom) = 0;
  // the engine's allocator (operator new) for the render tile list's nodes; the port has no heap model (U-0823)
  virtual uint32_t operator_new(uint32_t size) = 0;
};

// The TerrainApi of a context (not owned); MissingResource when none is attached.
void set_terrain_api(Ctx &ctx, TerrainApi *api);
TerrainApi &terrain_api(Ctx &ctx);
bool has_terrain_api(Ctx &ctx);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_API_H
