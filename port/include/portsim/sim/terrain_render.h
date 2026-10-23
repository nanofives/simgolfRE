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

// Negative-control mode of the renderer's deliberately wrong bodies (0 = correct); see terrain_render.cpp.
void terrain_render_negctl(int32_t mode);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_RENDER_H
