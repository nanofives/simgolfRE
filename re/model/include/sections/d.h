/* Stream D of the phase-1 data model: rendering and audio state of SimGolf v1.03 (golf_clean.exe).
 *
 * Generated layout notes live in re/analysis/data/model_d.md, addresses in re/model/d_addresses.tsv.
 * No original addresses appear in this header (decision 2); pointers are 32-bit values (uint32_t) because the
 * original is a 32-bit process and the save and memory dumps carry them that way.
 * Original defects are catalogued in model_d.md, never repaired here (decision 3).
 */
#ifndef SIMGOLF_MODEL_SECTION_D_H
#define SIMGOLF_MODEL_SECTION_D_H

#include <stdint.h>
#include <stddef.h>

#ifndef SG_STATIC_ASSERT
#define SG_STATIC_ASSERT(cond, msg) typedef char sg_static_assert_##msg[(cond) ? 1 : -1]
#endif

/* ------------------------------------------------------------------ constants */
enum {
    SGD_SOUND_SLOTS = 300,          /* Snd objects in the static array */
    SGD_SOUND_CONFIGURED = 219,     /* loadSoundTable calls (210 distinct slots, 9 slots configured twice) */
    SGD_SOUND_GROUPS = 16,          /* volume / mute groups of the music device; group id 0x10 = none */
    SGD_SOUND_GROUP_NONE = 0x10,
    SGD_SOUND_ENTRY_POINTS = 11,    /* sound.dll ordinals 1..11 resolved by GetProcAddress */

    SGD_SPRITE_QUEUE_CAP = 0x400,   /* parallel arrays of 1024 elements; enqueueSprite refuses at count > 0x3fe */
    SGD_SPRITE_QUEUE_LAST_INDEX = 0x3fe,

    SGD_SHEETS = 590,               /* asset sheet ids (flics); id 0x24e = the shadow stream slot */
    SGD_SHEET_PATH_LEN = 0x32,
    SGD_FLIC_SLOTS = 591,           /* Flic objects and stream objects */
    SGD_CELL_POOL = 0xdd40,         /* Surface cells shared by all sheets */
    SGD_CELL_FREE_RUNS = 100,       /* free list of the cell allocator (start / length arrays) */
    SGD_IMAGE_SLOTS = 190,          /* Cache482fd0 objects: image slots (163 of them are loaded by name) */

    SGD_TILE_SCREEN_CACHE = 2500,   /* 50 x 50 int16 cache of tile screen positions */
    SGD_SKY_ACTORS = 4,
    SGD_RECORD_BANKS = 6,
    SGD_RECORD_BANK_SLOTS = 16,
    SGD_MOVIE_IDS = 19,
    SGD_DRAW_LIST_CAP = 512
};

/* Snd::state (+0x44) bits as the code tests and sets them */
enum {
    SGD_SND_STATE_STARTED = 0x001,  /* set by Snd::start and SndMusic::setFile, tested before restarting */
    SGD_SND_STATE_LINKED = 0x002,   /* member of the doubly linked list g_soundHead / g_soundTail */
    SGD_SND_STATE_MODE_1 = 0x004,   /* Snd::setMode case 1 */
    SGD_SND_STATE_MODE_2 = 0x008,   /* Snd::setMode case 2 */
    SGD_SND_STATE_MODE_4 = 0x010,   /* Snd::setMode case 4 */
    SGD_SND_STATE_MODE_7 = 0x080,   /* Snd::setMode case 7 */
    SGD_SND_STATE_MODE_6 = 0x100,   /* Snd::setMode case 6 */
    SGD_SND_STATE_BIT_200 = 0x200   /* tested together with LINKED by shutdownSound */
};

/* Snd::configure option argument (loadSoundTable passes 4 for 192 slots and 0x14 for 18 slots) */
enum {
    SGD_SND_OPT_BIT_1 = 0x001,
    SGD_SND_OPT_BIT_2 = 0x002,      /* calls the device slot +0x48 with 1 */
    SGD_SND_OPT_OPEN_STREAM = 0x004,/* calls SndStream::open484c20 */
    SGD_SND_OPT_BIT_10 = 0x010,
    SGD_SND_OPT_BIT_40 = 0x040,
    SGD_SND_OPT_BIT_80 = 0x080,
    SGD_SND_OPT_BIT_100 = 0x100,
    SGD_SND_OPT_BIT_400 = 0x400
};

/* g_spriteFlags values drawQueuedSprite tests */
enum {
    SGD_SPRITE_FLAG_NONE = -1,      /* 0xffffffff: plain blit of the sprite object */
    SGD_SPRITE_FLAG_GOLFER = 0x100, /* low byte = golfer index, composes the layered golfer sprite */
    SGD_SPRITE_FLAG_PALETTE_COPY = 0x200, /* low 9 bits index g_images: drawn with that Image as palette source */
    SGD_SPRITE_FLAG_SCALED_A = 0x400,
    SGD_SPRITE_FLAG_SCALED_B = 0x800
};

/* ------------------------------------------------------------------ object types */

/* One Snd object (class with a vtable, ctor Snd::ctorDerived): one sound slot. Slots are filled by loadSoundTable. */
typedef struct SgSnd {
    uint32_t vptr;          /* +0x00 pointer to the class's code table (4 distinct tables are stored by the ctors) */
    int32_t volume;         /* +0x04 0..0x7f, default 0x7f (Snd::ctor485260, setVolume masks 0x7f) */
    int32_t pan;            /* +0x08 -0x40..0x3f (Snd::setPan clamps) */
    uint32_t unk_0c[9];     /* +0x0c..+0x2f zeroed by the base ctor, never read by a Snd method [U-0320] */
    uint32_t loop;          /* +0x30 nonzero = looping; mapped to bit 1 by Snd::flags */
    int32_t field_34;       /* +0x34 stored by Snd::setField34, passed to the voice slot +0x4c [U-0320] */
    int32_t field_38;       /* +0x38 default 1000, nonzero only (Snd::setField38), passed to voice slot +0x04 [U-0320] */
    uint32_t group;         /* +0x3c 0..15 = index into SgSndGroupDevice.groups, 0x10 = none (default) */
    uint32_t voice;         /* +0x40 device voice object created by sound.dll entry 1, 0 = not open */
    uint32_t state;         /* +0x44 SGD_SND_STATE_* bits */
    uint32_t prev;          /* +0x48 previous Snd in the list, 0 = head */
    uint32_t next;          /* +0x4c next Snd in the list, 0 = tail */
    uint32_t path;          /* +0x50 heap copy of the file path (Snd::setName), 0 = none */
    int32_t mode;           /* +0x54 argument of Snd::setMode (1, 2, 4, 5, 6, 7) */
    uint32_t options;       /* +0x58 bits derived from the configure() argument */
    int32_t pitch;          /* +0x5c -0x4b0..0x4b0 (Snd::setPitch clamps) */
    uint32_t unk_60;        /* +0x60 ctorDerived stores the float 1.0 (0x3f800000); no reader found [U-0320] */
    uint32_t length_ms;     /* +0x64 voice slot +0xc8's result after open (compared with elapsed time in Snd::busy) */
    uint32_t started_at;    /* +0x68 timeGetTime() when Snd::play ran the deferred path */
} SgSnd;
SG_STATIC_ASSERT(sizeof(SgSnd) == 0x6c, SgSnd_size);

/* One record of the music device's group table (HashTable::bucketEmpty / bucketSet / Snd::setVolume). */
typedef struct SgSndGroup {
    uint8_t enabled;        /* +0x00 zero: Snd::play returns 0x14 without playing */
    uint8_t muted;          /* +0x01 nonzero: Snd::play returns 0x14 without playing */
    uint8_t unk_02[2];      /* +0x02 */
    int32_t gain;           /* +0x04 multiplied into the slot volume by Snd::setVolume (x87 scale constant at 0x4bacf0) */
    uint8_t unk_08[0x14];   /* +0x08 no access found [U-0321] */
} SgSndGroup;
SG_STATIC_ASSERT(sizeof(SgSndGroup) == 0x1c, SgSndGroup_size);

/* The music device object: a class with a vtable whose 16 group records start at +0x24. */
typedef struct SgSndGroupDevice {
    uint32_t vptr;          /* +0x00 resourceLoad calls [[this]+4] and [[this]+8] */
    uint8_t unk_04[0x10];   /* +0x04 [U-0322] */
    uint32_t device;        /* +0x14 object resourceLoad calls through ([[this+0x14]]+0x10) */
    uint8_t unk_18[0x0c];   /* +0x18 [U-0322] */
    SgSndGroup groups[16];  /* +0x24 .. +0x1e3 */
} SgSndGroupDevice;
SG_STATIC_ASSERT(sizeof(SgSndGroupDevice) == 0x1e4, SgSndGroupDevice_size);

/* Opaque placeholders for objects whose layout belongs to stream E (sizes come from constructor loops and
 * allocators; the word array keeps 4-byte aligned field accesses checkable). */
typedef struct SgSurfaceObj { uint32_t w[11]; } SgSurfaceObj;          /* Surface_ctor object */
SG_STATIC_ASSERT(sizeof(SgSurfaceObj) == 0x2c, SgSurfaceObj_size);
typedef struct SgFlicObj { uint32_t w[33]; } SgFlicObj;                /* Flic::ctor481b50 object */
SG_STATIC_ASSERT(sizeof(SgFlicObj) == 0x84, SgFlicObj_size);
typedef struct SgFlicStream { uint32_t w[17]; } SgFlicStream;          /* Flic::ctorBase482b60 object */
SG_STATIC_ASSERT(sizeof(SgFlicStream) == 0x44, SgFlicStream_size);
typedef struct SgCache58 { uint32_t w[22]; } SgCache58;                /* Cache482fd0 object: the 190 image slots and the screen manager */
SG_STATIC_ASSERT(sizeof(SgCache58) == 0x58, SgCache58_size);
typedef struct SgCursorObj { uint32_t vptr; } SgCursorObj;             /* Cursor object: only the vtable store is seen [U-0305] */
SG_STATIC_ASSERT(sizeof(SgCursorObj) == 4, SgCursorObj_size);

/* Convex polygon rasteriser state (fillConvexPoly8 / fillConvexPoly16 / polyEdgeStep / fillSpanDown / fillSpanUp). */
typedef struct SgPolyFill {
    uint32_t fill_color;    /* +0x00 written as 4 bytes, read as 1 byte (8-bit fill) or 2 bytes (16-bit fill) */
    int32_t bottom_vertex;  /* +0x04 index of the vertex with the largest y */
    uint32_t unk_08;        /* +0x08 no access found [U-0323] */
    uint32_t verts;         /* +0x0c pointer to int32 pairs (x at +0, y at +4), set from the caller's argument */
    int32_t clip_left;      /* +0x10 clip rectangle word 0 */
    int32_t clip_top;       /* +0x14 clip rectangle word 1 */
    uint8_t unk_18[0x14];   /* +0x18 no access found [U-0323] */
    int32_t vert_count;     /* +0x2c */
    uint32_t pixels16;      /* +0x30 locked pixel base of a 16-bit surface (display slot +0x20) */
    uint32_t pixels8;       /* +0x34 locked pixel base of an 8-bit surface (slot +0x18) */
    int32_t scan_y;         /* +0x38 current scan line */
    int32_t pitch;          /* +0x3c bytes per row (the 16-bit path stores 2x the pixel pitch) */
    int32_t clip_right;     /* +0x40 right edge minus one */
    int32_t clip_bottom;    /* +0x44 bottom edge minus one */
} SgPolyFill;
SG_STATIC_ASSERT(sizeof(SgPolyFill) == 0x48, SgPolyFill_size);

/* Flying scenery actor (drawWorldObjects, 4 records saved in the .sve file). */
typedef struct SgSkyActor {
    int32_t x;              /* +0x00 world coordinate, -1 = slot free (kind 0) */
    int32_t y;              /* +0x04 world coordinate */
    int32_t size;           /* +0x08 0..0x28 */
    int8_t dir;             /* +0x0c direction index into two per-direction step tables */
    int8_t kind;            /* +0x0d switch selector of drawWorldObjects (sign-extended by movsx at its use) */
    uint8_t unk_0e[2];      /* +0x0e [U-0324] */
} SgSkyActor;
SG_STATIC_ASSERT(sizeof(SgSkyActor) == 0x10, SgSkyActor_size);

/* Placement bank (placeRecord / resetRecordBank / drawRecords): 16 placed objects over a 6 x 6 occupancy grid. */
typedef struct SgRecordBank {
    int16_t id[16];         /* +0x00 object type, -1 = free slot */
    int8_t x[16];           /* +0x20 column of the object's footprint origin */
    int8_t y[16];           /* +0x30 row */
    int8_t anim[16];        /* +0x40 animation step, -1 = none */
    int8_t occupancy[36];   /* +0x50 [row * 6 + column] = slot index, -1 = free */
} SgRecordBank;
SG_STATIC_ASSERT(sizeof(SgRecordBank) == 0x74, SgRecordBank_size);

#endif /* SIMGOLF_MODEL_SECTION_D_H */
