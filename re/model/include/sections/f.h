/* Stream F of the phase-1 data model: initialised constant tables and configuration words of SimGolf v1.03
 * (golf_clean.exe, .rdata and initialised .data).
 *
 * Layout notes live in re/analysis/data/model_f.md, addresses and file offsets in re/model/f_addresses.tsv and
 * re/model/f_descriptors.tsv. No original addresses appear in this header (decision 2). The table CONTENT is not part
 * of the model (decision 1): the port reads it from the user's own exe at the file offsets listed in
 * f_descriptors.tsv. Original defects are catalogued in model_f.md, never repaired here (decision 3).
 */
#ifndef SIMGOLF_MODEL_SECTION_F_H
#define SIMGOLF_MODEL_SECTION_F_H

#include <stdint.h>
#include <stddef.h>

#ifndef SG_STATIC_ASSERT
#define SG_STATIC_ASSERT(cond, msg) typedef char sg_static_assert_##msg[(cond) ? 1 : -1]
#endif

/* ------------------------------------------------------------------ constants */
enum {
    SGF_RECORD_DEF_COUNT = 15,      /* record definitions (footprint + size + name) */
    SGF_ANIMAL_COUNT = 9,           /* animal / habitat records */
    SGF_LOCATION_COUNT = 16,        /* course-site records; the build-item table follows directly */
    SGF_BUILD_ITEM_COUNT = 20,      /* build palette items */
    SGF_QUOTE_COUNT = 20,           /* records per quote table (the last of the first table is all zero) */
    SGF_QUOTE_TABLES = 2,           /* quote tables of 0x44-byte records covered here; a third region follows */
    SGF_DIR_COUNT = 8,              /* neighbour direction offset tables (x and y) */

    SGF_NAME_LEN = 16,              /* build item / animal name field */
    SGF_QUOTE_TEXT_LEN = 64,
    SGF_RECORD_NAME_LEN = 32,
    SGF_LOCATION_NAME_LEN = 24
};

/* ------------------------------------------------------------------ small element types */
/* A pair of 16-bit values: the layout / hit-test point tables read as word pairs. Which member is x and which is y
 * differs by table (see model_f.md, U-0502). */
typedef struct F_Point16 {
    int16_t a;                      /* +0x00 */
    int16_t b;                      /* +0x02 */
} F_Point16;
SG_STATIC_ASSERT(sizeof(F_Point16) == 4, f_point16);

typedef struct F_Pair32 {
    int32_t a;                      /* +0x00 */
    int32_t b;                      /* +0x04 */
} F_Pair32;
SG_STATIC_ASSERT(sizeof(F_Pair32) == 8, f_pair32);

/* six dwords per record (0x18 bytes); only three full records are certain (U-0503) */
typedef struct F_Rec24 {
    int32_t w[6];                   /* +0x00 .. +0x14 */
} F_Rec24;
SG_STATIC_ASSERT(sizeof(F_Rec24) == 24, f_rec24);

/* ------------------------------------------------------------------ records with a name */
/* record definition: six footprint-bitmask bytes, a size byte, a name */
typedef struct F_RecordDef {
    uint8_t footprint[6];           /* +0x00 */
    int8_t size;                    /* +0x06 read with movsx */
    char name[SGF_RECORD_NAME_LEN]; /* +0x07 NUL padded */
} F_RecordDef;
SG_STATIC_ASSERT(sizeof(F_RecordDef) == 0x27, f_record_def);

typedef struct F_Animal {
    char name[SGF_NAME_LEN];        /* +0x00 */
    int8_t b10;                     /* +0x10 read with movsx (U-0510) */
    int8_t b11;                     /* +0x11 read with movsx (U-0510) */
} F_Animal;
SG_STATIC_ASSERT(sizeof(F_Animal) == 0x12, f_animal);

typedef struct F_Location {
    char name[SGF_LOCATION_NAME_LEN]; /* +0x00 */
    int8_t b18;                     /* +0x18 read with movsx */
    char text[33];                  /* +0x19 NUL-terminated text (U-0500) */
    int16_t v3a;                    /* +0x3a */
    int16_t v3c;                    /* +0x3c */
    uint8_t b3e;                    /* +0x3e */
    uint8_t b3f;                    /* +0x3f */
    uint8_t b40;                    /* +0x40 */
    char text41[65];                /* +0x41 NUL-terminated text (U-0500) */
} F_Location;
SG_STATIC_ASSERT(sizeof(F_Location) == 0x82, f_location);

/* The game overwrites the name of four records at load (state, not a constant). */
typedef struct F_BuildItem {
    char name[SGF_NAME_LEN];        /* +0x00 */
    int8_t b10;                     /* +0x10 read with movsx */
    uint8_t unk_11;                 /* +0x11 never addressed on its own (U-0507) */
    int16_t value12;                /* +0x12 shown as value * 100 in the building panel */
} F_BuildItem;
SG_STATIC_ASSERT(sizeof(F_BuildItem) == 0x14, f_build_item);

typedef struct F_Quote {
    int8_t hdr[4];                  /* +0x00 bytes 0..3 read with movsx (U-0508) */
    char text[SGF_QUOTE_TEXT_LEN];  /* +0x04 NUL padded */
} F_Quote;
SG_STATIC_ASSERT(sizeof(F_Quote) == 0x44, f_quote);

#endif /* SIMGOLF_MODEL_SECTION_F_H */
