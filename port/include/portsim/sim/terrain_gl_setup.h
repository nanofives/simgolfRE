// The GL setup of the original's Terrain.dll, produced by the port instead of replayed from a recording (phase 5 step 3; plan
// re/PHASE5_TERRAIN_PLAN.md). Written from the specification (re/match/terrain_ctor.cpp, terrain_system.cpp, terrain_tile4.cpp,
// terrain_tile3.cpp, terrain_textures.cpp, terrain_large1.cpp, terrain_tga.cpp, terrain_bitmap.cpp, terrain_lighting.cpp as reading
// reference) from the user's own game files (Data/Textures/<course>/*.bmp and *.tga, <Course>Lighting.txt) through GameFiles.
//
// What the original does, in order, and what these functions reproduce:
//   1. Terrain::Terrain names the tile types and calls relight(), which loads every texture of the Parkland set (518 BMP / TGA
//      files) with NO GL context yet: every call is a no-op in the driver, glGenTextures writes no name, and each loader goes on with
//      its uninitialised local (0xcccccccc, the debug build's stack fill), so the texture table ends up full of 0xcccccccc.
//   2. Terrain::initSystem creates the context, then initGL (state), initLighting (the lighting file: light 0 and the specular
//      material), relight again (now reloadTextures first: glDeleteTextures over the whole table, then the 518 loads for real, names
//      1..518 from the driver) and resize (viewport and orthographic projection).
//   3. Terrain::initTerrain sets the vertex and normal array pointers.
// Every GL call goes through a GlSink, so the same code feeds the stream comparison (GlStreamSink) and the rasteriser.
#ifndef PORTSIM_SIM_TERRAIN_GL_SETUP_H
#define PORTSIM_SIM_TERRAIN_GL_SETUP_H

#include <cstdint>
#include <string>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/gl_sink.h"

namespace portsim {

// The Terrain object's fields and the Terrain.dll globals the GL setup reads and writes.
struct TerrainGlState {
  static constexpr int kTypes = 0x25, kVariations = 0x19, kSlots = 9;
  std::string type_name[kTypes];        // Terrain +0x2c: names of the tile types ("" = never named: its files do not exist)
  int32_t type_count[kTypes] = {};      // variations loaded per type
  std::vector<uint32_t> textures;       // g_textures[type][variation][slot]: GL texture names
  int32_t course_type = 0;              // 0 Parkland, 1 Desert, 2 Tropical, 3 Links
  bool textures_loaded = false;
  bool flip_y = false;                  // Terrain +0x28: the projection's y flip and the clockwise front face
  bool has_context = false;
  int32_t screen_w = 0, screen_h = 0;   // Terrain +0x20 / +0x24
  float screen_scale = 0.0f;            // the x rotation of every pass, set per window size by initSystem
  int32_t zoom = 0;                     // Terrain +0x1c: the zoom shift 0 / 1 / 2 (setZoomLevel)
  TerrainGlState();
};

// Terrain::Terrain's GL part: the type names and the texture load without a context.
void terrain_gl_construct(Ctx &ctx, gl::GlSink &gl, TerrainGlState &st);
// Terrain::initSystem(width, height, dc, flipY) with a display surface (dc != 0): context, initGL, lighting, textures, projection.
void terrain_gl_init_system(Ctx &ctx, gl::GlSink &gl, TerrainGlState &st, int32_t width, int32_t height, bool flip_y);
// Terrain::setZoomLevel(level): level 4 = the normal view (the resize projection, zoom 0), 2 = zoom 1, 1 = zoom 2; any other level
// changes nothing but the projection matrix mode and its identity.
void terrain_gl_set_zoom_level(gl::GlSink &gl, TerrainGlState &st, int32_t level);
// Terrain::loadNewCourseType(type): when the type changes, the course's lighting, reloadTextures and relight.
void terrain_gl_load_new_course_type(Ctx &ctx, gl::GlSink &gl, TerrainGlState &st, int32_t type);
// Terrain::initTerrain's GL part: the vertex and normal pointers.
void terrain_gl_init_terrain(gl::GlSink &gl);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_GL_SETUP_H
