// The port's terrain renderer: the draw side of the original's Terrain.dll (phase 5 step 3; plan
// re/PHASE5_TERRAIN_PLAN.md). Written from the specification (re/match/terrain_render.cpp, terrain_draw.cpp,
// terrain_culled.cpp, terrain_skirts.cpp, terrain_walls.cpp, terrain_paths.cpp, terrain_corneroverlay.cpp as reading
// reference), not copied.
//
// Inputs, like the original's:
//   - the Terrain object and its 2,500 tiles live in the context's 32-bit address space (portsim/ptr.h); the renderer reads
//     their fields through the context. Layout (documented in terrain_api.h): grid width / height, zoom shift, window size,
//     tiles of 0x248 bytes; per tile its grid position, type, four neighbour pointers, up to eight faces (three vertex
//     indices, three texture-coordinate selectors, a texture id, a slot), path flags, wall flags and heights.
//   - the module-level data Terrain.dll keeps next to the code, here TerrainGlobals: the vertex and normal arrays the
//     faces index, the texture-id table, the wall and fence textures, the texture-coordinate table, the side normals and
//     the two view values (the screen scale and the quadrant index setViewAngle writes). The host fills it: the test harness
//     from a recorded snapshot, and later the port's own Terrain model.
// Output: GL calls on a GlSink, in the original's order with its argument bit patterns.
#ifndef PORTSIM_SIM_TERRAIN_RENDER_H
#define PORTSIM_SIM_TERRAIN_RENDER_H

#include <cstdint>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/gl_sink.h"

namespace portsim {

struct TerrainGlobals {
  std::vector<float> vertices;       // 3 floats per vertex index
  std::vector<float> normals;        // 3 floats per vertex index
  std::vector<uint32_t> textures;    // [type 0x25][variation 0x19][slot 9] GL texture ids
  uint32_t wall_texture = 0;         // the skirt wall
  uint32_t fence_texture0 = 0;       // wall kinds other than 1
  uint32_t fence_texture1 = 0;       // wall kind 1
  std::vector<float> tex_coords;     // [selector 0..][selector 1: 3][quadrant 4][2]
  float side_normals[4][2][3] = {};  // four blocks of two normals (row by view quadrant), in the order the data lies
  float screen_scale = 0.0f;         // the x rotation of every pass
  int32_t view_index = 0;            // the camera quadrant 0..3 (written by set_view_angle)
  // state side only (terrain_state.h); the renderer does not read these
  bool collar_set = false;           // g_collarInfo != NULL (passCollarInfo has run)
  std::vector<int32_t> collar;       // g_collarInfo: collar group per tile type, as many entries as the exe passed
  int32_t course_type = 0;           // g_courseType: 0 Parkland, 1 Desert, 2 Tropical, 3 Links
  uint32_t rand_state = 1;           // the game thread's rand() state in Terrain.dll's debug CRT (_holdrand, 1 until srand)
  float corner_step = 15.0f;         // g_splineHeight: elevate / lowerCorner's step, setSplineHeight's target
};

// Terrain::render(tile, angle) of the original: draws the window of tiles around `tile` (or the fixed region without one)
// for one camera angle. Always returns true.
bool terrain_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t tile, float angle);

// The draw-list passes (roadmap step S3.1). Each queues tiles into the Terrain object's draw list (drawTile: back to front for
// the angle, one entry per position; drawTileObjects: the neighbours behind camera-facing walls), then draws the list with
// Tile::render, empties it, pops the matrix and flushes.
//   localRender(tile, pivot, angle): the 5 x 5 tiles around `tile`, the camera at `pivot`; faults on a NULL tile (the
//     original's debug assert).
//   stripRender(pivot, dir, angle): diagonal strips for view direction 1..8 (odd = two nested passes of the neighbouring
//     even directions; any other value draws nothing).
//   pathUpdateRender(pivot, angle): the path tiles of the window around `pivot`; keeps the view quadrant the last pass set.
void terrain_local_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t tile, uint32_t pivot, float angle);
void terrain_strip_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t pivot, int32_t dir, float angle);
void terrain_path_update_render(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t pivot, float angle);

// renderTileAtFiveOffsets(tile, pivot, angle) (roadmap S3.28): the draw half of Terrain::lowerEdgeCorner(tile, corner, pivot,
// angle), which calls it before lowering the corner. When a neighbour toward the camera is missing or hidden it draws the tile five
// times at +-10 offsets with light 0 off and a black light 1 on (only the global ambient term is left: a near-black silhouette).
// Faults on a NULL tile (the original's debug assert).
void terrain_render_tile_at_five_offsets(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, uint32_t terrain, uint32_t tile, uint32_t pivot,
                                         float angle);

// The four exported render entry points by the corpus's entry kind, with the raw argument dwords of the exported call (floats
// as bit patterns): 1 stripRender(a0 pivot, a1 dir, a2 angle), 2 render(a0 tile, a1 angle), 3 localRender(a0 tile, a1 pivot,
// a2 angle), 4 pathUpdateRender(a0 pivot, a1 angle), and 5 renderTileAtFiveOffsets(a0 tile, a1 pivot, a2 angle) = lowerEdgeCorner's
// arguments 0, 2 and 3 (its corner is the state side's). Returns false for any other kind (nothing drawn).
enum TerrainRenderEntry : int32_t {
  kEntryStripRender = 1, kEntryRender = 2, kEntryLocalRender = 3, kEntryPathUpdateRender = 4, kEntryRenderTileAtFiveOffsets = 5
};
bool terrain_render_entry(Ctx &ctx, gl::GlSink &gl, TerrainGlobals &g, int32_t entry, uint32_t terrain, uint32_t a0, uint32_t a1,
                          uint32_t a2);

// Negative-control mode of the renderer's deliberately wrong bodies (0 = correct); see terrain_render.cpp.
void terrain_render_negctl(int32_t mode);
// P8B (widescreen stage 2, log/roadmap/P8/P8B_notes.md): the world layer's passes widen isCulled's x bounds by m logical pixels on
// each side (shifted left by the Terrain object's zoom field, like halfW); 0 (the default) = the original's bounds. The caller sets
// it around one pass and puts 0 back.
void terrain_render_set_cull_margin(int32_t m);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_RENDER_H
