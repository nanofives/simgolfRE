/* SimGolf data model, stream C: course, terrain and world (phase 1 of re/PORT_PLAN.md).
 *
 * Clean C99, fixed-width types, no original addresses (they live in re/model/c_addresses.tsv).
 * Evidence, corrections and open ranges: re/analysis/data/model_c.md; open questions re/model/c_uncertain.tsv
 * (U-0200..). Struct layouts are packed by natural alignment only (MSVC default), every member at a multiple of
 * its own size, so no pragma is needed.
 *
 * Grid convention: the course is 50 x 50 tiles; every per-tile array is indexed [x][y] = x * 50 + y (x is the
 * outer index, the code's `col` in some notes). The corner-level grid is 51 x 51 indexed row * 51 + col.
 */
#ifndef SIMGOLF_MODEL_SECTION_C_H
#define SIMGOLF_MODEL_SECTION_C_H

#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define SGC_STATIC_ASSERT(cond, tag) _Static_assert(cond, #tag)
#elif defined(__cplusplus) && __cplusplus >= 201103L
#define SGC_STATIC_ASSERT(cond, tag) static_assert(cond, #tag)
#else
#define SGC_STATIC_ASSERT(cond, tag) typedef char sgc_static_assert_##tag[(cond) ? 1 : -1]
#endif

#define SGC_GRID_W 50
#define SGC_GRID_H 50
#define SGC_GRID_CELLS 2500
#define SGC_CORNER_W 51
#define SGC_CORNER_CELLS 2601

/* ------------------------------------------------------------------------------------------------ tile types
 * The value of the tile-type grid is the index of a TileTypeAttr record (23 records) and the Terrain.dll tile type.
 * Identifiers are ours, chosen from what the code does with each value (model_c.md section 2); the display names are
 * the record's own name fields, read from the user's exe at run time. Values 3, 6, 8 have no compare or store of
 * their own in the game logic. */
enum TileType {
    TILE_TEE = 0,
    TILE_GREEN = 1,
    TILE_FAIRWAY = 2,
    TILE_TYPE_3 = 3,
    TILE_ROUGH = 4,            /* the default fill */
    TILE_DEEP_ROUGH = 5,
    TILE_TYPE_6 = 6,
    TILE_SAND = 7,
    TILE_TYPE_8 = 8,
    TILE_POT_BUNKER = 9,
    TILE_STREAM = 10,          /* the themes rename record 10 */
    TILE_BRUSH = 11,
    TILE_FEATURE_12 = 12,      /* 12..18: mainLoop's per-tile switch indexed by type - 12 */
    TILE_TREE_13 = 13,
    TILE_TREE_14 = 14,
    TILE_TREE_15 = 15,
    TILE_TREE_16 = 16,
    TILE_WATER = 17,
    TILE_WETLAND = 18,
    TILE_TYPE_19 = 19,
    TILE_OUT_OF_BOUNDS = 20,   /* typeAtPos returns it off the grid; land not yet bought */
    TILE_BUILDING_A = 21,      /* stampCourseTiles writes (kind != 5) + 21 */
    TILE_BUILDING_B = 22,
    TILE_TYPE_COUNT = 23
};

typedef struct TileTypeAttr {         /* 0x30 bytes, 23 records, copied from the exe's file data at start */
    char     name[16];                /* +0x00 singular display name (renamed per course theme) */
    char     plural[16];              /* +0x10 */
    int8_t   u20;                     /* +0x20 [U-0204] */
    int8_t   u21;                     /* +0x21 [U-0204] */
    int8_t   walk;                    /* +0x22 <= 0 blocks a walker's step (canStep); g_typeAttr */
    int8_t   cost;                    /* +0x23 clearing cost factor (clearCost; shown * 100) */
    int8_t   u24;                     /* +0x24 summed over placement areas [U-0204] */
    int8_t   path_cost;               /* +0x25 initial value of the path-cost grid */
    int8_t   collar;                  /* +0x26 collar group passed to Terrain::passCollarInfo */
    int8_t   u27;                     /* +0x27 [U-0204] */
    int8_t   wall_height;             /* +0x28 wallHeight's result */
    uint8_t  pad29[3];                /* +0x29 no access */
    uint32_t flags;                   /* +0x2c TTA_* */
} TileTypeAttr;
SGC_STATIC_ASSERT(sizeof(TileTypeAttr) == 0x30, tile_type_attr_size);

#define TTA_LEVEL_FIXED   0x01u       /* corner heights come from the per-tile level grid */
#define TTA_LEVEL_MIN     0x02u       /* level = min of the corner range */
#define TTA_LEVEL_MAX     0x04u       /* level = max of the corner range (wins over MIN) */
#define TTA_HEIGHT_3      0x08u       /* cornerHeights returns 3; heightAt / worldToScreen test it */

/* -------------------------------------------------------------------------------------------- per-tile grids
 * All [50][50], index x * 50 + y, except the corner levels [51][51] (x * 51 + y; tile (x, y) uses the vertices
 * (x, y-1), (x, y), (x+1, y-1), (x+1, y)) and the corner cache [2500][8] (bytes 1, 3, 5, 7 = corners). */
typedef int8_t   SgcTileTypeGrid[SGC_GRID_W][SGC_GRID_H];      /* enum TileType values */
typedef uint8_t  SgcByteGrid[SGC_GRID_W][SGC_GRID_H];
typedef int8_t   SgcSByteGrid[SGC_GRID_W][SGC_GRID_H];
typedef uint16_t SgcTileFlagGrid[SGC_GRID_W][SGC_GRID_H];      /* TF_* */
typedef uint8_t  SgcCornerLevelGrid[SGC_CORNER_W][SGC_CORNER_W];
typedef int8_t   SgcCornerCache[SGC_GRID_CELLS][8];
SGC_STATIC_ASSERT(sizeof(SgcTileFlagGrid) == 5000, tile_flag_grid_size);
SGC_STATIC_ASSERT(sizeof(SgcCornerLevelGrid) == 0xa29, corner_level_grid_size);
SGC_STATIC_ASSERT(sizeof(SgcCornerCache) == 0x4e20, corner_cache_size);

/* Tile flag word (model_c.md section 3). The low five bits are a field with three uses: the hole number on a
 * tee / cup tile, the placed-object kind + 1 on an object's origin tile, and a 0..7 phase on water tiles. */
#define TF_FIELD_MASK     0x001fu
#define TF_PATH           0x0020u     /* also Terrain::layPath's on flag */
#define TF_FLOOD_REACHED  0x0040u     /* floodMark / rebuildTileCoverage */
#define TF_CUP            0x0080u     /* with the hole number in TF_FIELD_MASK */
#define TF_BIT_0100       0x0100u     /* [U-0200] */
#define TF_BENCH          0x0200u
#define TF_COVERED        0x0400u     /* inside a placed object's footprint */
#define TF_BIT_0800       0x0800u     /* set with TF_ANIMATING by golfer thoughts; walkers clear it [U-0200] */
#define TF_FLOWERS        0x1000u
#define TF_BIT_2000       0x2000u     /* set only by generateRandomCourse [U-0200] */
#define TF_ANIMATING      0x4000u     /* frame in the per-tile animation frame grid */
#define TF_PERMANENT      0x8000u     /* demolishObject refuses */

/* Per-tile build-tool code grid: 0xff = none, bit 0x80 tested by buildToolLabel / demolishObject. */
#define SGC_BUILD_CODE_NONE 0xffu
#define SGC_ANIM_FRAME_RESTART 0xffu

/* ------------------------------------------------------------------------------------------------ top scores */

#define SGC_TOP_SCORES 10

typedef struct TopScore {             /* 0x9c bytes */
    char     designer[64];            /* +0x00 copied from the name the .srf writes as the designer */
    char     course_title[64];        /* +0x40 appendCourseTitle(0) */
    int32_t  fun_rating;              /* +0x80 */
    int32_t  skill_rating_x100;       /* +0x84 */
    int32_t  cash_div100;             /* +0x88 rank = (cash / 10 + skill + fun) * (level + 1) */
    uint8_t  u8c[10];                 /* +0x8c no access by the game logic [U-0206] */
    int16_t  level;                   /* +0x96 */
    int32_t  course_id;               /* +0x98 copy of the course id word (model_c.md section 7) */
} TopScore;
SGC_STATIC_ASSERT(sizeof(TopScore) == 0x9c, top_score_size);

/* ------------------------------------------------------------------------------------------------ hole records */

#define SGC_HOLE_RECORDS 20     /* record 0 scratch (swap slot), 1..18 holes, 19 the golfer-hole 0x13 slot */
#define SGC_HOLE_MAX 18
#define SGC_HOLE_LEAVING 19     /* a golfer's hole byte 0x13 */

typedef struct HoleRecord {           /* 0x208 bytes */
    int8_t   par;                     /* +0x000 0 = no hole */
    int8_t   direction;               /* +0x001 */
    uint8_t  u002;                    /* +0x002 written once, never read [U-0201] */
    uint8_t  pad003;                  /* +0x003 no access */
    int16_t  length;                  /* +0x004 25 per tile of tee-to-cup distance (the .srf yards unit) */
    uint8_t  pad006[2];               /* +0x006 no access */
    int32_t  tee_x;                   /* +0x008 tile */
    int32_t  tee_y;                   /* +0x00c */
    int32_t  tee2_x;                  /* +0x010 second tee tile */
    int32_t  tee2_y;                  /* +0x014 */
    int32_t  cup_x;                   /* +0x018 tile */
    int32_t  cup_y;                   /* +0x01c */
    int32_t  first_shots;             /* +0x020 decays x -= x / 8 */
    int32_t  u024;                    /* +0x024 decays like +0x020 [U-0201] */
    int16_t  histogram[88];           /* +0x028 8 rows of 11 (row stride 0x16), cleared 8 x 10 */
    int16_t  comment_count[64];       /* +0x0d8 per comment index */
    int16_t  mood_sum;                /* +0x158 sum of thought mood changes on this hole (decays) */
    int16_t  u15a;                    /* +0x15a */
    int16_t  stat[8];                 /* +0x15c [6] (+0x166) is a running maximum, not decayed */
    uint16_t comment_last[64];        /* +0x16c last argument per comment index (& 0x3fff by the readers) */
    int32_t  u1ec;                    /* +0x1ec [U-0201] */
    int32_t  money;                   /* +0x1f0 pointsPopup subtracts its amount */
    int32_t  u1f4;                    /* +0x1f4 compared with 200 and 400 */
    int32_t  ticks;                   /* +0x1f8 counted while par != 0 */
    int32_t  magazine_score;          /* +0x1fc holeMagazineEvent */
    uint32_t flags;                   /* +0x200 */
    uint8_t  pad204[4];               /* +0x204 no access */
} HoleRecord;
SGC_STATIC_ASSERT(sizeof(HoleRecord) == 0x208, hole_record_size);

/* -------------------------------------------------------------------------------------------- placed objects */

#define SGC_PLACED_OBJECTS 256
#define SGC_OBJECT_FREE (-1)

typedef struct PlacedObject {         /* 0x10 bytes */
    int16_t  type;                    /* +0x00 -1 = free slot */
    int16_t  x;                       /* +0x02 tile */
    int16_t  y;                       /* +0x04 tile */
    uint8_t  orientation;             /* +0x06 0..3 */
    uint8_t  flags;                   /* +0x07 bit 0x40 recomputed by rebuildTileCoverage */
    int32_t  value;                   /* +0x08 meaning per type */
    int32_t  value2;                  /* +0x0c type 5: running rating */
} PlacedObject;
SGC_STATIC_ASSERT(sizeof(PlacedObject) == 0x10, placed_object_size);

/* ---------------------------------------------------------------------------------------------------- animals */

#define SGC_ANIMALS 100
#define SGC_ANIMAL_FREE (-1)

typedef struct Animal {               /* 0x3c bytes */
    int32_t  u00;                     /* +0x00 written 0 on spawn, never read */
    uint8_t  u04[28];                 /* +0x04 no access outside the save [U-0203] */
    int16_t  type;                    /* +0x20 -1 = free slot; flic id base from the species byte (stream B) */
    int16_t  kind;                    /* +0x22 spawn argument 1, indexes a 0x74-stride table */
    int16_t  pad24;                   /* +0x24 no access */
    int16_t  x;                       /* +0x26 tile */
    int16_t  y;                       /* +0x28 tile */
    int16_t  countdown;               /* +0x2a */
    int16_t  sprite_base;             /* +0x2c 0x140 / 0x14d / 0x15a, plus type */
    int16_t  pad2e;                   /* +0x2e no access */
    int32_t  sub_x;                   /* +0x30 position inside the tile */
    int32_t  sub_y;                   /* +0x34 */
    int8_t   heading;                 /* +0x38 0..7, turned by Random::range(3) - 1 and masked with 7; indexes the direction tables (stream B, merged by G1) */
    uint8_t  walk_steps;              /* +0x39 0x10 when a walk starts, -1 per step, idle again at <= 0 (stream B, merged by G1) */
    int8_t   frame;                   /* +0x3a */
    uint8_t  pad3b;                   /* +0x3b no access */
} Animal;
SGC_STATIC_ASSERT(sizeof(Animal) == 0x3c, animal_size);

/* ------------------------------------------------------------------------------- Terrain.dll boundary (heap)
 * The exe holds one pointer to this object; Terrain.dll allocates it (getInstance, `new Terrain(50, 50)`) and the
 * exe destroys it (~Terrain, then the exe's own operator delete). Layout from the 100%-matched sources
 * re/match/terrain*.cpp. The exe reads only width / height (inline tileAt), Tile.type, Tile.corner[odd] and
 * Tile.wall_on directly; everything else goes through the 26 imported methods (model_c.md section 9).
 * NOTE the DLL's tile index is x + y * width, the transpose of the exe grids' x * 50 + y. */

#define SGC_TERRAIN_TYPE_SLOTS 37

typedef struct TerrainTypeInfo {      /* 0x18 bytes */
    char     name[20];                /* +0x00 */
    int32_t  count;                   /* +0x14 */
} TerrainTypeInfo;
SGC_STATIC_ASSERT(sizeof(TerrainTypeInfo) == 0x18, terrain_type_info_size);

typedef struct TerrainTile {          /* 0x248 bytes */
    int32_t  corner[9];               /* +0x000 odd indices 1, 3, 5, 7 are the corner heights [U-0210] */
    int32_t  type;                    /* +0x024 default 4; 0x14 hidden */
    int32_t  rotation;                /* +0x028 read as a char by getVariation (the exe's tile_variant byte) */
    int32_t  x;                       /* +0x02c */
    int32_t  y;                       /* +0x030 */
    uint32_t neighbour[4];            /* +0x034 TerrainTile* (32-bit image): y-1, y+1, x-1, x+1 */
    int32_t  face_count;              /* +0x044 */
    uint8_t  face[8][0x38];           /* +0x048 Face records, internals in re/match/terrain_tile4.cpp */
    uint8_t  has_path;                /* +0x208 */
    uint8_t  path_connected;          /* +0x209 */
    uint8_t  draw_path;               /* +0x20a */
    uint8_t  path_info[4];            /* +0x20b rest of the 7-byte PathInfo */
    uint8_t  pad20f;                  /* +0x20f */
    int32_t  wall_height[9];          /* +0x210 */
    uint8_t  wall_on[9];              /* +0x234 indexed by side 0, 2, 4, 6 */
    uint8_t  pad23d[3];               /* +0x23d */
    int32_t  variation;               /* +0x240 */
    uint8_t  normals_dirty;           /* +0x244 */
    uint8_t  pad245[3];               /* +0x245 */
} TerrainTile;
SGC_STATIC_ASSERT(sizeof(TerrainTile) == 0x248, terrain_tile_size);

typedef struct TerrainObject {        /* 0x164ad0 bytes, allocated by Terrain.dll */
    uint32_t glrc;                    /* +0x000 HGLRC */
    float    ambient[4];              /* +0x004 0.15, 0.15, 0.2, 1.0 */
    int32_t  width;                   /* +0x014 50, read by the exe's inline tileAt */
    int32_t  height;                  /* +0x018 50 */
    int32_t  zoom;                    /* +0x01c shift count (setZoomLevel) */
    int32_t  screen_w;                /* +0x020 */
    int32_t  screen_h;                /* +0x024 */
    uint8_t  flip_y;                  /* +0x028 */
    uint8_t  textures_loaded;         /* +0x029 */
    uint8_t  pad02a[2];               /* +0x02a */
    TerrainTypeInfo types[SGC_TERRAIN_TYPE_SLOTS]; /* +0x02c */
    TerrainTile tiles[2500];          /* +0x3a4 index x + y * width */
    uint8_t  draw_list[12];           /* +0x164ac4 std::list<Tile*> (VC6 layout, opaque) */
} TerrainObject;
SGC_STATIC_ASSERT(sizeof(TerrainObject) == 0x164ad0, terrain_object_size);

#endif /* SIMGOLF_MODEL_SECTION_C_H */
