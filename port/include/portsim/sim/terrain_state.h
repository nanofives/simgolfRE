// The state side of the port's Terrain.dll (phase 5 step 3; plan re/PHASE5_TERRAIN_PLAN.md section 13): the Terrain object, its
// tiles and faces, and the vertex / normal arrays the renderer (terrain_render.h) draws from. Written from the specification, with
// the matched sources re/match/terrain_ctor.cpp, terrain_tile4.cpp, terrain_tile5.cpp, terrain_tile6.cpp, terrain_faces.cpp,
// terrain_face.cpp, terrain_small1..4.cpp, terrain_smooth.cpp, terrain_blend.cpp, terrain_methods.cpp, terrain_methods2.cpp as reading
// reference, not copied.
//
// Where the state lives, like the original's:
//   - the Terrain object (TerrainObjectHeader, then 2,500 TerrainTile, then the draw list) in the context's 32-bit address space, so
//     the renderer and the exe-side callers read the same bytes; the host maps a block of kTerrainObjectSize bytes for it. The
//     original allocates it with the debug CRT's operator new (Terrain::getInstance), which fills new memory with 0xcd: fields no
//     code writes keep that fill (the even corner slots, the wall heights, five of the nine wall bytes), and a comparison with the
//     original's object needs the same fill (kNewFill).
//   - the module data (vertex and normal arrays, texture table, collar table, course type) in TerrainGlobals.
//
// Floating point: the original's x87 code runs with the 53-bit control word; every float expression here is computed in double and
// rounded to float where the original stores to a float, which is what that x87 code does (a sum or product of floats is exact
// or rounded once to 53 bits, then once to 24 bits on the store).
#ifndef PORTSIM_SIM_TERRAIN_STATE_H
#define PORTSIM_SIM_TERRAIN_STATE_H

#include <cstddef>
#include <cstdint>

#include <string>

#include "portsim/registry.h"
#include "portsim/sim/gl_sink.h"
#include "portsim/sim/terrain_gl_setup.h"
#include "portsim/sim/terrain_render.h"

namespace portsim {

#pragma pack(push, 1)
// One triangle of a tile (Faces::build writes v and tc; setFaceTexture texture and piece; faceNormal normal).
struct TerrainFace {
  int32_t v[3];          // +0x00 vertex indices into the vertex / normal arrays
  int32_t tc[6];         // +0x0c texture-coordinate selectors (roundHalf: 0, 1 or 2 for 0.0, 1.0, 0.5)
  uint32_t texture;      // +0x24 GL texture name
  uint8_t piece;         // +0x28 the texture slot it came from (0..8)
  uint8_t pad29[3];      // +0x29 never written (0xcd)
  float normal[3];       // +0x2c
};
static_assert(sizeof(TerrainFace) == 0x38, "TerrainFace");
static_assert(offsetof(TerrainFace, tc) == 0x0c && offsetof(TerrainFace, texture) == 0x24, "TerrainFace");
static_assert(offsetof(TerrainFace, piece) == 0x28 && offsetof(TerrainFace, normal) == 0x2c, "TerrainFace");

// Neighbour slots (Terrain::buildArrays' setNeighbour calls): 0 = y - 1, 1 = y + 1, 2 = x - 1, 3 = x + 1.
enum TerrainSide : int32_t { kSideYMinus = 0, kSideYPlus = 1, kSideXMinus = 2, kSideXPlus = 3 };

struct TerrainTile {
  int32_t level[8];      // +0x00 corner heights in steps of 15.0 at the odd indices 1, 3, 5, 7; the even ones are never written
  uint8_t pad20[4];      // +0x20 never written
  int32_t type;          // +0x24 0x14 = hidden, 7 = the rotated (bunker) type, 6 = the raised type
  int32_t rotation;      // +0x28 rotation / variant (Tile::setRotation; water's depth class for type 0x11)
  int32_t x, y;          // +0x2c, +0x30 grid position
  uint32_t neighbour[4]; // +0x34 Tile pointers by TerrainSide (0 at the grid edge)
  int32_t face_count;    // +0x44 the Faces block starts here: count, then eight faces
  TerrainFace face[8];   // +0x48
  uint8_t has_path;      // +0x208 PathInfo b0 (Tile::layPath)
  uint8_t connected;     // +0x209 b1 (Tile::setConnected / layPath's direction flag)
  uint8_t draw_path;     // +0x20a b2 (cleared on the path-carrying types)
  uint8_t path_b3[4];    // +0x20b b3..b6, cleared by PathInfo::clear
  uint8_t pad20f;        // +0x20f
  int32_t wall_height[9];// +0x210 WallInfo heights by side (Tile::setWall)
  uint8_t wall_on[9];    // +0x234 WallInfo flags; PathInfo-like clear zeroes 0, 2, 4, 6 only
  uint8_t pad23d[3];     // +0x23d
  int32_t variation;     // +0x240 texture variation (Terrain::setType draws it)
  uint8_t normals_dirty; // +0x244
  uint8_t pad245[3];     // +0x245
};
static_assert(sizeof(TerrainTile) == 0x248, "TerrainTile");
static_assert(offsetof(TerrainTile, type) == 0x24 && offsetof(TerrainTile, x) == 0x2c && offsetof(TerrainTile, neighbour) == 0x34,
              "TerrainTile");
static_assert(offsetof(TerrainTile, face_count) == 0x44 && offsetof(TerrainTile, face) == 0x48, "TerrainTile");
static_assert(offsetof(TerrainTile, has_path) == 0x208 && offsetof(TerrainTile, wall_height) == 0x210, "TerrainTile");
static_assert(offsetof(TerrainTile, wall_on) == 0x234 && offsetof(TerrainTile, variation) == 0x240, "TerrainTile");
static_assert(offsetof(TerrainTile, normals_dirty) == 0x244, "TerrainTile");

struct TerrainTypeInfo {
  char name[0x14];       // strcpy'd by the constructor (the texture file stem)
  int32_t count;         // variations loaded (relight); Terrain::setType draws a variation below it
};
static_assert(sizeof(TerrainTypeInfo) == 0x18, "TerrainTypeInfo");

struct TerrainObjectHeader {
  uint32_t glrc;         // +0x00 initSystem's GL context handle (0 from the constructor; initSystem returns at once when set). The
                         //       OS chooses its value (0x10000 in every recording): the port writes kContextToken, compare zero / nonzero
  float m4, m8, mc, m10; // +0x04 0.15, 0.15, 0.2, 1.0 (constructor; no reader in the ported code)
  int32_t width, height; // +0x14, +0x18 grid size (50 x 50 from getInstance)
  int32_t zoom;          // +0x1c zoom shift (setZoomLevel)
  int32_t screen_w, screen_h;  // +0x20, +0x24 (resize)
  uint8_t flip_y;        // +0x28 (initSystem)
  uint8_t textures_loaded;     // +0x29
  uint8_t pad2a[2];
  TerrainTypeInfo types[0x25]; // +0x2c
};
static_assert(sizeof(TerrainObjectHeader) == 0x3a4, "TerrainObjectHeader");
#pragma pack(pop)

constexpr uint32_t kTerrainTilesOffset = 0x3a4;
constexpr int32_t kTerrainMaxTiles = 0x9c4;   // 50 x 50
constexpr uint32_t kTerrainDrawListOffset = kTerrainTilesOffset + kTerrainMaxTiles * sizeof(TerrainTile);   // std::list<Tile*>
constexpr uint32_t kTerrainObjectSize = kTerrainDrawListOffset + 0x0c;
static_assert(kTerrainDrawListOffset == 0x164ac4 && kTerrainObjectSize == 0x164ad0, "Terrain object size (operator new argument)");
constexpr uint8_t kNewFill = 0xcd;            // the debug CRT's fill of new memory
constexpr uint32_t kListHeadToken = 2;        // the port's stand-in for the draw list's heap head node (compare zero / nonzero)
constexpr uint32_t kContextToken = 1;         // the port's stand-in for a GL context handle in TerrainObjectHeader::glrc
constexpr int32_t kTerrainTextureTypes = 0x25, kTerrainTextureVariations = 0x19, kTerrainTextureSlots = 9;

// ---- construction ----
// Terrain::Terrain(width, height)'s state part over a mapped block (the GL part, relight, is terrain_gl_construct): header fields,
// Tile::reset of every tile, type counts 0, the texture table zeroed, g_collarInfo NULL, course type 0, and the type names.
void terrain_construct(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, int32_t width, int32_t height);
// What the GL setup leaves in the object (relight's type counts, initSystem's flip flag, resize's window size, setZoomLevel's
// zoom, the textures-loaded flag) and in the globals (texture table, course type).
void terrain_apply_gl_state(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, const TerrainGlState &st);
// Terrain::buildArrays: initGrid, then per tile reset + Faces::build, then the neighbour links. Returns true.
bool terrain_build_arrays(Ctx &ctx, TerrainGlobals &g, uint32_t terrain);
// Terrain::initTerrain's state part (buildArrays; the GL part is terrain_gl_init_terrain).
void terrain_init_terrain(Ctx &ctx, TerrainGlobals &g, uint32_t terrain);
// Terrain::passCollarInfo(table, n): the collar group per tile type that blending compares.
void terrain_pass_collar_info(TerrainGlobals &g, const int32_t *table, int32_t n);

// ---- tiles ----
uint32_t terrain_tile_at(Ctx &ctx, uint32_t terrain, int32_t x, int32_t y);   // 0 outside the grid
void tile_reset(Ctx &ctx, uint32_t tile, int32_t x, int32_t y);
void faces_build(Ctx &ctx, uint32_t tile, int32_t x, int32_t y, int32_t width);
void init_grid(TerrainGlobals &g, int32_t width, int32_t height);

// ---- normals ----
void face_normal(Ctx &ctx, TerrainGlobals &g, uint32_t face);           // Terrain::faceNormal
void tile_smooth_normals(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t a, int32_t b);
void tile_calc_normals(Ctx &ctx, TerrainGlobals &g, uint32_t tile);    // Tile::calcNormals (only when dirty)
void terrain_calc_normals(Ctx &ctx, TerrainGlobals &g, uint32_t tile); // exported Terrain::calcNormals (NULL-safe)
void terrain_calc_all_normals(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, uint32_t tile);

// ---- face textures ----
void tile_set_face_texture(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type, int32_t face, int32_t piece);
void tile_blend_face_textures(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type);   // -1 = the tile's own type
void tile_reblend_neighbourhood(Ctx &ctx, TerrainGlobals &g, uint32_t tile);
void tile_reset_face_textures(Ctx &ctx, TerrainGlobals &g, uint32_t tile);   // types 0 and 9 (setTypeId)
int32_t tile_edge_kind(Ctx &ctx, const TerrainGlobals &g, uint32_t tile, uint32_t other);

// ---- mutators (step 3) ----
int32_t terrain_rand(TerrainGlobals &g);   // rand() of Terrain.dll's debug CRT on the game thread
void terrain_set_type(Ctx &ctx, TerrainGlobals &g, uint32_t terrain, uint32_t tile, int32_t type, int32_t rotation);
void tile_set_type_id(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type);
void tile_enter_type6(Ctx &ctx, TerrainGlobals &g, uint32_t tile);
void tile_leave_type6(Ctx &ctx, TerrainGlobals &g, uint32_t tile);
void tile_enter_type7(Ctx &ctx, TerrainGlobals &g, uint32_t tile);
void tile_leave_type7(Ctx &ctx, TerrainGlobals &g, uint32_t tile);
void tile_set_path_flag_for_type(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t type);
void tile_elevate_corner(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t corner);
void tile_lower_corner(Ctx &ctx, TerrainGlobals &g, uint32_t tile, int32_t corner);
void tile_lay_path(Ctx &ctx, uint32_t tile, bool on, int32_t dir);
void tile_set_wall(Ctx &ctx, uint32_t tile, int32_t side, int32_t height, uint8_t on);
void terrain_set_view_angle(TerrainGlobals &g, float angle);
void terrain_reset_terrain(Ctx &ctx, TerrainGlobals &g, uint32_t terrain);

// ---- the exported methods, as the exe calls them (step 4) ----
// The DLL's whole state for one context: the object's address (the host maps kTerrainObjectSize bytes there, filled with kNewFill),
// the module globals, the GL setup's state and a sink for the GL calls the setup makes (the replay keeps only their state).
struct TerrainDll {
  uint32_t terrain = 0;          // where getInstance puts the object
  bool constructed = false;      // g_terrain != NULL
  TerrainGlState st;
  gl::GlStreamSink sink;
};
// One call of an exported Terrain method by its undecorated name ("setType", "elevateCorner", ..., "getInstance"), `self` = ecx,
// args = the stack dwords as the exe pushed them (floats as bit patterns; bool arguments use their low byte), `table` / `n` the
// ints behind passCollarInfo's pointer. Returns eax where the method returns a value (getInstance: the object; render: 1), else 0.
// Methods with no state effect (drawLine, drawCircle, the splines, renderTile, the getters) only check their arguments' reads;
// methods not ported throw MissingResource{name}.
uint32_t terrain_dll_call(Ctx &ctx, TerrainGlobals &g, TerrainDll &dll, const std::string &name, uint32_t self, const uint32_t *args,
                          const int32_t *table, int32_t n);

// Negative-control mode of the state side's deliberately wrong bodies (0 = correct); see terrain_state.cpp.
void terrain_state_negctl(int32_t mode);

}  // namespace portsim

#endif  // PORTSIM_SIM_TERRAIN_STATE_H
