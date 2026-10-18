/* SimGolf data model, stream B: simulation state (phase 1 of re/PORT_PLAN.md).
 *
 * Clean C99, fixed-width types, no original addresses (they live in re/model/b_addresses.tsv).
 * Evidence, corrections and open ranges: re/analysis/data/model_b.md; open questions re/model/b_uncertain.tsv
 * (U-0100..U-0199). Every member sits at a multiple of its own size (MSVC default packing), so no pragma is needed.
 * Golfer types, walkers, golfer statistics, story slots, shot tracks, phrases, course records, finance, histories,
 * rankings, date, RNG, flag words, bulletin events and the shot / path scratch state.
 * Uses SgSurfaceObj from d.h only in comments (the Surface arrays are stream D / E objects).
 */
#ifndef SIMGOLF_MODEL_SECTION_B_H
#define SIMGOLF_MODEL_SECTION_B_H

#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define SGB_STATIC_ASSERT(cond, tag) _Static_assert(cond, #tag)
#elif defined(__cplusplus) && __cplusplus >= 201103L
#define SGB_STATIC_ASSERT(cond, tag) static_assert(cond, #tag)
#else
#define SGB_STATIC_ASSERT(cond, tag) typedef char sgb_static_assert_##tag[(cond) ? 1 : -1]
#endif

/* ---- Golfer record (re/analysis/data/golfer_record.md; 156 records of 0x100 bytes) ---- */

#define SGB_GOLFER_COUNT          156   /* 0x9c records: the block clear is 0x2700 dwords */
#define SGB_GOLFER_LIVE_COUNT     152   /* golfer loops end at record 152 */
#define SGB_GOLFER_REC_SCRATCH    152   /* swap / replay scratch */
#define SGB_GOLFER_REC_PREVIEW    153   /* character-editor preview */
#define SGB_GOLFER_REC_SYNTHETIC  154   /* built by mainLoop far code [U-0017] */
#define SGB_GOLFER_REC_TRIAL_COPY 155   /* trialShot's saved copy */
#define SGB_GOLFER_HOLE_SLOTS     19    /* per-hole arrays, slot 0 unused, holes 1..18 */
#define SGB_GOLFER_HOLE_LEAVING   0x13  /* hole byte when the golfer leaves */
#define SGB_GOLFER_SKILLS         12
#define SGB_WORLD_TILE_SHIFT      10    /* tile = world >> 10 */

/* flags (+0x10): only the bits with an established meaning */
#define SGB_GOLFER_F_PLAYER_MARK   0x00000200u /* the golfer whose index is in the player-golfer global */
#define SGB_GOLFER_F_STORY_PAIRED  0x00100000u /* paired with a story partner */
#define SGB_GOLFER_F_LEFT_COURSE   0x20000000u /* set with hole = 0x13 */
#define SGB_GOLFER_F_MEMBER_REQ    0x80000000u /* membership join / upgrade request */

typedef struct Golfer {
    int32_t  x;                    /* +0x00 world x, tile = x >> 10 */
    int32_t  y;                    /* +0x04 world y */
    int32_t  screen_x;             /* +0x08 -1 when worldToScreen returned 0 */
    int32_t  screen_y;             /* +0x0c */
    uint32_t flags;                /* +0x10 bits: golfer_record.md section 3 [U-0009] */
    int16_t  step_left;            /* +0x14 movement left in the current step */
    int16_t  skill_mask;           /* +0x16 bit i = skill[i] != 0 (bits 0..11); -1 = all, narrateShot */
    uint8_t  kind;                 /* +0x18 class bits 5..7, sub-kind bits 0..4 */
    uint8_t  prefs;                /* +0x19 bits 0..2 preferences, 4..7 [U-0010] */
    int8_t   facing;               /* +0x1a octant 0..7 */
    int8_t   shot_value;           /* +0x1b set by prepareShot, 0..20 [U-0011] */
    int8_t   club;                 /* +0x1c 0..13 */
    int8_t   state;                /* +0x1d animation / activity state [U-0012] */
    uint8_t  frame;                /* +0x1e frame in the current sequence */
    uint8_t  frame_base;           /* +0x1f frame-table base last chosen */
    int8_t   counter20;            /* +0x20 counter 1..0x78 [U-0012] */
    int8_t   hole;                 /* +0x21 -1 new, 1..18, 0x13 leaving */
    int8_t   strokes;              /* +0x22 strokes on the current hole */
    int8_t   hole_strokes[19];     /* +0x23 [hole] */
    int8_t   toward_zero36;        /* +0x36 stepped one unit toward 0 per hole [U-0013] */
    uint8_t  hole_counter[19];     /* +0x37 [hole] story counter low byte / (chapter << 3) + 1 */
    uint8_t  hole_chapter[19];     /* +0x4a [hole] story stage low byte */
    uint8_t  hole_mood[18];        /* +0x5d written as +0x5c + hole (hole 1..18) */
    uint8_t  pad_6f;               /* +0x6f only drawGolferInfoPanel's off-by-one store */
    uint8_t  thought_kind[10];     /* +0x70 history, [0] newest, bit 7 masked by readers */
    uint8_t  thought_place[10];    /* +0x7a hole * 11 + strokes (+1 for kind >= 4) */
    uint8_t  thought_timer;        /* +0x84 countdown */
    uint8_t  last_thought_kind;    /* +0x85 bit 7 from the thought-mode global */
    uint8_t  last_thought_place;   /* +0x86 */
    uint8_t  pad_87;               /* +0x87 no access */
    uint16_t thought_arg[10];      /* +0x88 bits 0..13 argument, 14 / 15 outcome */
    uint16_t last_thought_arg;     /* +0x9c */
    int16_t  wait;                 /* +0x9e < 0: skip steps */
    int16_t  clubs_used;           /* +0xa0 bit per club */
    int16_t  partner;              /* +0xa2 golfer index, -1 none */
    int16_t  mood;                 /* +0xa4 -10..10 (updateWalkers stores -11) */
    int16_t  meter_a6;             /* +0xa6 */
    int16_t  meter_a8;             /* +0xa8 */
    int16_t  meter_aa;             /* +0xaa */
    int16_t  shot_kind;            /* +0xac 0, 1, -1, 3, 4 */
    int16_t  pair_class;           /* +0xae 0..3 written, 0..5 read [U-0014] */
    int16_t  story_id;             /* +0xb0 index into the story definitions, -1 none */
    int16_t  story_stage;          /* +0xb2 */
    int16_t  story_counter;        /* +0xb4 */
    int16_t  type;                 /* +0xb6 golfer type 0..75 */
    uint16_t traits;               /* +0xb8 low byte from the type record, high byte random */
    int8_t   cls;                  /* +0xba 0..2 */
    uint8_t  name_rec;             /* +0xbb index into the 0x38-byte name records */
    uint8_t  mask_bc;              /* +0xbc bitmask [U-0015] */
    uint8_t  pad_bd;               /* +0xbd no access */
    int16_t  order;                /* +0xbe stamp from the order counter */
    int32_t  date_stamp;           /* +0xc0 copy of the game date */
    int32_t  shot_start_x;         /* +0xc4 world */
    int32_t  shot_start_y;         /* +0xc8 */
    int32_t  target_x;             /* +0xcc tile; -1 = player's shot pending */
    int32_t  target_y;             /* +0xd0 tile */
    int32_t  ball_x;               /* +0xd4 world; 0 = no ball */
    int32_t  ball_y;               /* +0xd8 */
    int32_t  ball_z;               /* +0xdc height */
    uint32_t ball_dir;             /* +0xe0 full circle = 2^32 */
    int32_t  ball_speed;           /* +0xe4 horizontal */
    int32_t  ball_vz;              /* +0xe8 vertical */
    int32_t  spin;                 /* +0xec */
    uint8_t  skill[12];            /* +0xf0 skill levels */
    uint8_t  file_tail[4];         /* +0xfc only copied with the .chr / .pro block [U-0016] */
} Golfer;
SGB_STATIC_ASSERT(sizeof(Golfer) == 0x100, Golfer_size);

/* ---- gb1: people tables next to the golfer records (stream B, phase 1) ----
 * Evidence for every member: log/p1/b/gb1/notes.md; addresses: log/p1/b/gb1/addresses.tsv.
 * #include <stdint.h> assumed. */

/* ======================================================================== */
/* Walker / employee record: 64 records of 0x4c bytes                        */
/* ======================================================================== */

#define SGB_WALKER_COUNT          64     /* block clear 0x4c0 dwords; loops end after record 63 */

/* kind (+0x13): only spawnWalker stores it, from its argument */
#define SGB_WALKER_KIND_PLAYER    (-6)   /* the player's own character, record 0 (generateRandomCourse) */
#define SGB_WALKER_KIND_STAFF_A   (-2)   /* hire choices 0..2: approaches golfers, thoughts 0x22 / 0x3a */
#define SGB_WALKER_KIND_STAFF_B   (-3)   /* hire choices 3..5: sets golfer flag 0x4000 */
#define SGB_WALKER_KIND_STAFF_C   (-4)   /* hire choices 6..8: works tiles with tile flag 0x800 */
#define SGB_WALKER_KIND_STAFF_D   (-5)   /* hire choices 9..11: sells to golfers, income +2 */
/* kind = -2 - hire_choice / 3; staff default name = table index (bit3) - (2 * kind + 4), 0..7 */

/* flags (+0x12) */
#define SGB_WALKER_F_ACTIVE       0x01u  /* spawnWalker stores 1; the byte == 0 marks a free slot */
#define SGB_WALKER_F_MATCH_DONE   0x02u  /* set by matchUpdate for a walker whose kind equals the golfer index [U-0111] */
#define SGB_WALKER_F_EXPERT       0x08u  /* hire_choice % 3 == 2 */
#define SGB_WALKER_F_LEAVING      0x10u  /* walk to the exit tile and free the slot; no instruction sets it [U-0111] */

/* action (+0x14): the action started by updateWalkers' switch */
#define SGB_WALKER_ACT_STAFF_A    (-2)   /* 0xfe */
#define SGB_WALKER_ACT_STAFF_B    (-3)   /* 0xfd */
#define SGB_WALKER_ACT_STAFF_C    (-4)   /* 0xfc tile work */
#define SGB_WALKER_ACT_STAFF_D    (-5)   /* 0xfb sale */
#define SGB_WALKER_ACT_OBJECT     (-6)   /* 0xfa kind -4 on a placed-object target */
#define SGB_WALKER_ACT_B_LEAVER   (-7)   /* 0xf9 expert kind -3 on a golfer that left */
#define SGB_WALKER_ACT_LEAVING    0x23   /* periodic, while SGB_WALKER_F_LEAVING */

typedef struct Walker {
    int32_t  x;                    /* +0x00 world x, tile = x >> 10 */
    int32_t  y;                    /* +0x04 world y */
    int32_t  screen_x;             /* +0x08 -1 when off screen */
    int32_t  screen_y;             /* +0x0c */
    int8_t   post_x;               /* +0x10 assigned tile x, -1 none */
    int8_t   post_y;               /* +0x11 assigned tile y */
    uint8_t  flags;                /* +0x12 WALKER_F_* */
    int8_t   kind;                 /* +0x13 WALKER_KIND_* */
    int8_t   action;               /* +0x14 WALKER_ACT_* */
    uint8_t  action_timer;         /* +0x15 7 at start, -1 every 4 dates, effect at 4 */
    int8_t   facing;               /* +0x16 octant 0..7 */
    uint8_t  unk_17;               /* +0x17 no access */
    int16_t  steps_left;           /* +0x18 steps left in the current move */
    int16_t  wait;                 /* +0x1a ticks to wait before the next decision */
    int16_t  anim_frame;           /* +0x1c frame counter of the drawn sequence */
    int16_t  anim_seq;             /* +0x1e 0xb idle, 0xc action, 7..10 walking */
    int16_t  target_golfer;        /* +0x20 golfer index of the current action */
    int16_t  hire_month;           /* +0x22 g_date / 1024 at hire */
    int16_t  wages_paid;           /* +0x24 sum of wages paid */
    int16_t  services;             /* +0x26 count of completed actions */
    uint8_t  unk_28[2];            /* +0x28 no access */
    char     name[34];             /* +0x2a rename text, empty = default name */
} Walker;
SGB_STATIC_ASSERT(sizeof(Walker) == 0x4c, Walker_size);

/* ======================================================================== */
/* Golfer type record: 84 records of 0x230 bytes (initialised data)          */
/* ======================================================================== */

#define SGB_GOLFER_TYPE_COUNT        84  /* 0x2df0 dwords backed up / restored as one block */
#define SGB_GOLFER_TYPE_SAVED        76  /* save field 63: 0xa640 bytes */
#define SGB_GOLFER_TYPE_CUSTOM       76  /* customizeCharacter's character */
#define SGB_GOLFER_TYPE_STORY_40     77  /* pairGolfersWithStory, golfer kind 0x40 */
#define SGB_GOLFER_TYPE_STORY_80     78  /* pairGolfersWithStory, golfer kind 0x80 */
#define SGB_GOLFER_TYPE_STORY_60     79  /* pairGolfersWithStory, golfer kind 0x60 */
#define SGB_GOLFER_TYPE_EDIT_UNDO    83  /* showCharacterEditor's copy of the record being edited */

/* demographics (+0x21), copied into golfer word +0xb8 low byte */
#define SGB_GTYPE_DEMO_AGE_MASK      0x07u /* one-hot 1 / 2 / 4, golferAge input */
#define SGB_GTYPE_DEMO_MARITAL_MASK  0x78u /* one-hot 0x08 / 0x10 / 0x20 / 0x40 */
#define SGB_GTYPE_DEMO_FEMALE        0x80u /* typeBit7Clear returns !bit7 */
/* personality (+0x20): bits 0..4, five labels */
#define SGB_GTYPE_PERSONALITY_MASK   0x1fu
/* custom (+0x2c) */
#define SGB_GTYPE_CUSTOM_PREFS_MASK  0x07u /* copied into golfer +0x19 when the dword is non-zero */
#define SGB_GTYPE_CUSTOM_BIT3        0x08u /* toggled by the editor, drawing variant [U-0116] */
#define SGB_GTYPE_CUSTOM_EDITED      0x80u /* set by every editor change */

typedef struct GolferType {
    char     title[16];            /* +0x00 type title text */
    char     name[16];             /* +0x10 person's name text */
    uint8_t  personality;          /* +0x20 SGB_GTYPE_PERSONALITY_MASK bits */
    uint8_t  demographics;         /* +0x21 GTYPE_DEMO_* */
    int8_t   body;                 /* +0x22 body model 0..18; >= 19 uses type % 4 */
    uint8_t  look_23;              /* +0x23 low nibble 0..9, high nibble 0..7 [U-0115] */
    int8_t   look_24;              /* +0x24 0..9 [U-0115] */
    int8_t   look_25;              /* +0x25 0..9 [U-0115] */
    int8_t   look_26;              /* +0x26 composeGolferSprite only [U-0115] */
    int8_t   look_27;              /* +0x27 0..3 [U-0115] */
    int8_t   look_28;              /* +0x28 0..4 [U-0115] */
    uint8_t  unk_29[3];            /* +0x29 no access */
    uint32_t custom;               /* +0x2c GTYPE_CUSTOM_*; 0 = not edited */
    char     bio[512];             /* +0x30 text built by buildGolferBioText (types 0..75) */
} GolferType;
SGB_STATIC_ASSERT(sizeof(GolferType) == 0x230, GolferType_size);

/* ======================================================================== */
/* Per-type course state: 84 slots of 0x2c bytes, 76 used                    */
/* ======================================================================== */

#define SGB_GOLFER_TYPE_STATE_SLOTS  84  /* save block 0xe70; clear 0x39c dwords */
#define SGB_GOLFER_TYPE_STATE_USED   76  /* loop ends at record 76 */
#define SGB_TYPE_STATE_BANNED        0xff /* +0x29 */

typedef struct GolferTypeState {
    uint8_t  best_round;           /* +0x00 lowest non-zero round stroke total */
    int8_t   avg_to_par;           /* +0x01 running (old + v) / 2 of round score minus par */
    uint8_t  membership;           /* +0x02 level in bits 0..2 (U-0003) */
    uint8_t  hole_flags[19];       /* +0x03 [hole] bit flags */
    int8_t   hole_strokes[19];     /* +0x16 [hole] last strokes */
    uint8_t  ban;                  /* +0x29 SGB_TYPE_STATE_BANNED after a golfer quit */
    int16_t  rounds_started;       /* +0x2a counted per kind-0 golfer at pairing */
} GolferTypeState;
SGB_STATIC_ASSERT(sizeof(GolferTypeState) == 0x2c, GolferTypeState_size);

/* ======================================================================== */
/* Per-golfer-class statistics: 32 records of 0xb8 bytes                     */
/* ======================================================================== */

#define SGB_GOLFER_STATS_COUNT       32  /* index = class (golfer +0xba) + 4 * (golfer +0x19 & 7) */

typedef struct GolferStats {
    int32_t  first_shot_dist_sum;  /* +0x00 */
    int32_t  first_shots_low_lie;  /* +0x04 first shots on a tile with type attribute <= 0 */
    int32_t  greens_in_regulation; /* +0x08 */
    int32_t  stat_0c;              /* +0x0c holed balls and type-1 lie shots (two writers) */
    int32_t  first_shots;          /* +0x10 */
    int32_t  longest_first_shot;   /* +0x14 */
    uint8_t  unk_18[8];            /* +0x18 no access */
    int32_t  hole_stroke_sum[19];  /* +0x20 [hole] sum of golfer strokes */
    int32_t  hole_count[19];       /* +0x6c [hole] count for +0x20 */
} GolferStats;
SGB_STATIC_ASSERT(sizeof(GolferStats) == 0xb8, GolferStats_size);

/* ======================================================================== */
/* Golf pro roster record: 100 records of 0x38 bytes (golfer +0xbb)          */
/* ======================================================================== */

#define SGB_GOLF_PRO_COUNT           100 /* loops end at field +0x20 of record 100 */
#define SGB_GOLF_PRO_EMPTY           (-1) /* +0x20 */
#define SGB_GOLF_PRO_SKILLS          10

typedef struct GolfPro {
    char     name[32];             /* +0x00 first roster token */
    int8_t   look_hi;              /* +0x20 0..7 (= GolferType look_23 >> 4); -1 empty slot */
    int8_t   look_27;              /* +0x21 0..3 (= GolferType look_27) */
    int8_t   look_lo;              /* +0x22 0..9 (= GolferType look_23 & 0xf) */
    int8_t   look_24;              /* +0x23 0..9 (= GolferType look_24) */
    int8_t   look_25;              /* +0x24 0..9 (= GolferType look_25) */
    uint8_t  skill[12];            /* +0x25 [0..9] parsed 0..19; [10..11] only copied with the 12-byte block */
    uint8_t  unk_31[5];            /* +0x31 no access */
    int16_t  skill_sum;            /* +0x36 sum of skill[0..9] */
} GolfPro;
SGB_STATIC_ASSERT(sizeof(GolfPro) == 0x38, GolfPro_size);

/* ======================================================================== */
/* Story file names: 100 x 50-byte view of a 100 x 100-byte name list        */
/* ======================================================================== */

#define SGB_STORY_SLOT_COUNT         100
#define SGB_STORY_SLOT_LEN           50

typedef struct StorySlot {
    char     text[50];             /* +0x00 a file name (even slots) or the tail of one (odd slots) */
} StorySlot;
SGB_STATIC_ASSERT(sizeof(StorySlot) == 0x32, StorySlot_size);

/* gb2.h: stream B helper gb2 (stories, shot tracks, phrase / roster tables, special-visitor counters).
 * C99 fragment, <stdint.h> assumed. Addresses and evidence: addresses.tsv and notes.md in this directory. */
/* ---- Ball-flight track table (the global the names files call g_storyPairs) ---- */
#define SGB_SHOT_TRACK_COUNT      16   /* loop ends at the table end, e.g. addGolferPair, notes.md 1 */
#define SGB_SHOT_TRACK_POINTS     32   /* write index = count, count stops at 0x1f */
#define SGB_SHOT_TRACK_FREE       (-1) /* golfer word of an unused entry */
#define SGB_SHOT_TRACK_PREVIEW    0x98 /* golfer word mainLoop leaves after the shot preview of golfer 0x99 */

typedef struct ShotTrack {
    int16_t golfer;                /* +0x000 golfer index, -1 = free */
    int16_t partner;               /* +0x002 copy of the golfer's partner word (Golfer +0xa2) at registration */
    int16_t point_count;           /* +0x004 points stored, 0..31; the slot at index point_count is the next write */
    uint8_t pad_006[2];            /* +0x006 no access */
    int32_t point_x[32];           /* +0x008 world x: shot start (point 0) or ball x */
    int32_t point_y[32];           /* +0x088 world y */
    int32_t point_z[32];           /* +0x108 ball height, 0 for point 0 */
    uint8_t golfer_snapshot[256];  /* +0x188 whole Golfer record of `golfer` at registration */
    uint8_t partner_snapshot[256]; /* +0x288 whole Golfer record of the partner at registration */
} ShotTrack;
SGB_STATIC_ASSERT(sizeof(ShotTrack) == 0x388, ShotTrack_size);

/* ---- Per-golfer-type phrase table (84 records, each 25 slots of 50 bytes) ---- */
#define SGB_GOLFER_TYPE_SLOTS     84   /* rep movsd / stosd 0x668a dwords = 84 * 0x4e2 */
#define SGB_GOLFER_PHRASE_SLOTS   25   /* 0x4e2 / 50; the editor and buildGolferComment use slots 0..19 */
#define SGB_GOLFER_PHRASE_USED    20   /* entries before the 0xff end of the slot-kind byte table */
#define SGB_GOLFER_PHRASE_LEN     50

typedef struct GolferPhrase {
    uint8_t text[50];              /* +0x00 NUL-terminated text; empty (text[0] == 0) = no phrase */
} GolferPhrase;
SGB_STATIC_ASSERT(sizeof(GolferPhrase) == 0x32, GolferPhrase_size);

typedef struct GolferTypePhrases {
    GolferPhrase slot[25];         /* +0x000 slot i holds the phrase for thought kind slotKind[i] (i < 20) */
} GolferTypePhrases;
SGB_STATIC_ASSERT(sizeof(GolferTypePhrases) == 0x4e2, GolferTypePhrases_size);

/* ---- Celebrity roster read from the theme's celebrity data file (100 records of 0x25 bytes) ---- */
#define SGB_CELEBRITY_COUNT       100
#define SGB_CELEBRITY_EMPTY       (-1) /* category byte of an unused record (0xff) */

typedef struct CelebrityRecord {
    uint8_t name[32];              /* +0x00 NUL-terminated name (first token of the line) */
    int8_t  category;              /* +0x20 (first char of token 2 - 'A') % 11, -1 = empty; indexes an 11-entry string table */
    int8_t  look_a;                /* +0x21 1..3 from token 3, else 0; composeGolferSprite reads it [U-0127] */
    int8_t  look_b;                /* +0x22 digit % 10 from token 4; composeGolferSprite adds 5 [U-0127] */
    int8_t  look_c;                /* +0x23 digit % 10 from token 5 [U-0127] */
    int8_t  look_d;                /* +0x24 digit % 10 from token 6 [U-0127] */
} CelebrityRecord;
SGB_STATIC_ASSERT(sizeof(CelebrityRecord) == 0x25, CelebrityRecord_size);

/* ---- Scenery dedication table (100 records of 8 bytes) ---- */
#define SGB_SCENIC_DEDICATION_COUNT 100

typedef struct ScenicDedication {
    int16_t tile_x;                /* +0x00 -1 = free; compared with the tile x of mainLoop's per-tile pass */
    int16_t tile_y;                /* +0x02 */
    int16_t golfer_type;           /* +0x04 golfer-type index whose name sceneryLabel appends */
    uint8_t pad_06[2];             /* +0x06 no access */
} ScenicDedication;
SGB_STATIC_ASSERT(sizeof(ScenicDedication) == 8, ScenicDedication_size);

/* ---- Constants of the scalars ---- */
#define SGB_AMENITY_MASK_NEW_GAME   0x3fff /* bits 0..13 set by mainLoop at a new game */
#define SGB_VISITOR60_MAX           8      /* pairGolfersWithStory stops at count >= 8 */
#define SGB_VISITOR_PENDING_BIT     0x80   /* bit 7 of the two visitor counters */
#define SGB_TOURNAMENT_PREP_PENDING (-1)   /* set by loadGolfPro */
#define SGB_TOURNAMENT_PREP_ASK     (-2)   /* mainLoop: -1 -> -2, then buildTournamentPrep */
#define SGB_TIP_MIN_DATE_GAP        500    /* emitGolferThought: g_date - last tip date must exceed 0x1f4 */
#define SGB_ARRIVAL_ORDER_TOURNEY   100    /* loadGolfPro restarts the order stamp at 0x64 */
#define SGB_SHOT_POWER_CACHE        10

/* gb3.h - stream B helper gb3: economy, finance, course records, rankings and economy scalars.
 * C99 fragment, <stdint.h> assumed. Addresses are in addresses.tsv, evidence in notes.md. */
/* ---- course (location) records: the career's course slots ---- */
#define SGB_COURSE_RECORD_COUNT        17   /* save size 0x30e / 0x2e; slots 0..15 are seeded and looped over */
#define SGB_COURSE_RECORD_USED         16
#define SGB_COURSE_YEAR_NONE           (-1) /* +0x22 / +0x23 = 0xff */

enum CourseRelief  { SGB_RELIEF_FLAT = 0, SGB_RELIEF_ROLLING = 1, SGB_RELIEF_HILLY = 2 };
enum CourseSetting { SGB_SETTING_INLAND = 0, SGB_SETTING_COASTAL = 1, SGB_SETTING_ISLAND = 2 };

typedef struct CourseRecord {
    char    name[24];          /* +0x00 copied from the location table entry +0x00 */
    int16_t map_x;             /* +0x18 copied from location table +0x3a */
    int16_t map_y;             /* +0x1a copied from location table +0x3c */
    int8_t  location_id;       /* +0x1c location table index 0..15, 0xff = unseeded */
    uint8_t acres;             /* +0x1d land size printed after the buy label */
    int8_t  relief;            /* +0x1e enum CourseRelief, from location table +0x40 */
    int8_t  setting;           /* +0x1f enum CourseSetting, from location table +0x3f */
    int8_t  course_type;       /* +0x20 0 Park 1 Desert 2 Tropical 3 Links, from location table +0x3e */
    uint8_t unk_21;            /* +0x21 U-0140 cleared only */
    int8_t  year_bought;       /* +0x22 years since 2001, -1 = not owned */
    int8_t  year_left;         /* +0x23 years since 2001, -1 = never left */
    uint8_t award_hole_honours;/* +0x24 snapshot of g_awardHoleHonours */
    uint8_t award_lots;        /* +0x25 snapshot of g_awardLots */
    uint8_t award_zero;        /* +0x26 always stored 0 */
    uint8_t award_tournament;  /* +0x27 snapshot of g_awardTournamentPoints */
    uint8_t award_stories;     /* +0x28 snapshot of g_awardStories */
    uint8_t unk_29[3];         /* +0x29 U-0140 never addressed */
    uint8_t cash_hi;           /* +0x2c g_cash_div100 / 256 (signed division), low byte kept */
    uint8_t cash_lo;           /* +0x2d g_cash_div100 % 256 (signed remainder) */
} CourseRecord;
SGB_STATIC_ASSERT(sizeof(CourseRecord) == 0x2e, CourseRecord_size);

/* ---- per-year finance ledger (financial report), units of $100 ---- */
#define SGB_FINANCE_YEAR_COUNT 100     /* index = year % 100 */
enum FinanceColumn {
    SGB_FIN_GREENS_FEES = 0, SGB_FIN_HOME_SITES = 1, SGB_FIN_FOOD_DRINK = 2, SGB_FIN_BUILD_COURSE = 3,
    SGB_FIN_FACILITIES = 4, SGB_FIN_SALARIES = 5, SGB_FIN_MAINT_INTEREST = 6, SGB_FIN_OTHER = 7, SGB_FIN_TOTAL = 8
};
typedef struct FinanceYear {
    int16_t greens_fees;       /* +0x00 */
    int16_t home_sites;        /* +0x02 */
    int16_t food_drink;        /* +0x04 */
    int16_t build_course;      /* +0x06 */
    int16_t facilities;        /* +0x08 */
    int16_t salaries;          /* +0x0a */
    int16_t maint_interest;    /* +0x0c */
    int16_t other;             /* +0x0e */
    int16_t total;             /* +0x10 rewritten by the report screen */
    int16_t unk_12;            /* +0x12 U-0141 never addressed */
} FinanceYear;
SGB_STATIC_ASSERT(sizeof(FinanceYear) == 0x14, FinanceYear_size);

/* ---- monthly history rings (histograph / end-of-year graphs) ---- */
#define SGB_HISTORY_MONTHS 500          /* index = (date / 1024) % 500 */
#define SGB_EVENT_KIND_SHIFT 5          /* history event word = kind << 5 | argument */

/* ---- Top 10 course ranking ---- */
#define SGB_RANKING_COUNT 10
#define SGB_RANKING_PLAYER_SLOT 9
typedef struct CourseRanking {
    int32_t hole_honours;      /* +0x00 icon count, component 0 */
    int32_t lots;              /* +0x04 icon count, component 1 */
    int32_t cash_100k;         /* +0x08 icon count, component 2 */
    int32_t component3;        /* +0x0c icon count, component 3 (player: always 0) */
    int32_t stories;           /* +0x10 icon count, component 4 */
    int32_t total;             /* +0x14 sum, the sort key */
} CourseRanking;
SGB_STATIC_ASSERT(sizeof(CourseRanking) == 0x18, CourseRanking_size);

/* ---- two world points per hole (stream C) ---- */
#define SGB_HOLE_POINTS_COUNT 19       /* index = hole number, 1..18 used */
typedef struct HolePoints {
    int32_t a_x;               /* +0x00 world x */
    int32_t a_y;               /* +0x04 world y */
    uint8_t unk_08[8];         /* +0x08 U-0142 never addressed */
    int32_t b_x;               /* +0x10 world x, -1 = none */
    int32_t b_y;               /* +0x14 world y */
} HolePoints;
SGB_STATIC_ASSERT(sizeof(HolePoints) == 0x18, HolePoints_size);

/* ---- animal habitat slots ---- */
#define SGB_HABITAT_SLOT_COUNT 128
#define SGB_HABITAT_KIND_FREE  0xff
typedef struct HabitatSlot {
    int32_t x;                 /* +0x00 world x = (tile << 10) + 0x200 */
    int32_t y;                 /* +0x04 world y */
    int32_t state;             /* +0x08 -20 at creation; < 0 waits (animation 0x103); bit 0 and bit 1 flags */
    int16_t link;              /* +0x0c index of another habitat slot whose bit 1 is tested, -1 = none */
    int16_t anim;              /* +0x0e animation base 0x103 / 0x10c / 0x115 / 0x11e; flic id = kind + anim */
    uint8_t frame;             /* +0x10 (frame + 1) % frame count of flic kind + anim */
    int8_t  direction;         /* +0x11 Random::range(4) at creation, rewritten by drawWorldObjects */
    uint8_t kind;              /* +0x12 habitat kind argument, 0xff = free slot */
    uint8_t unk_13;            /* +0x13 U-0143 never addressed */
} HabitatSlot;
SGB_STATIC_ASSERT(sizeof(HabitatSlot) == 0x14, HabitatSlot_size);

/* ---- ball records copied from a golfer's ball state ---- */
#define SGB_BALL_RECORD_COUNT 256
typedef struct BallRecord {
    int32_t start_x;           /* +0x00 golfer +0xd4 at insertion */
    int32_t start_y;           /* +0x04 golfer +0xd8 at insertion */
    int32_t x;                 /* +0x08 moving world x, -1 = free record */
    int32_t y;                 /* +0x0c moving world y */
    int32_t height;            /* +0x10 golfer +0xdc */
    int32_t direction;         /* +0x14 golfer +0xe0 */
    int32_t speed;             /* +0x18 golfer +0xe4 / 2 */
    int32_t vspeed;            /* +0x1c golfer +0xe8 / 3 */
    int32_t date;              /* +0x20 g_date at insertion */
} BallRecord;
SGB_STATIC_ASSERT(sizeof(BallRecord) == 0x24, BallRecord_size);

/* ---- golfer counts per (class, slot) ---- */
#define SGB_GOLFER_CLASS_COUNT 4
#define SGB_GOLFER_SLOT_COUNT  8

/* ---- terrain corner levels (stream C) ---- */
#define SGB_CORNER_GRID 51             /* uint8_t [51][51], index row * 51 + col */

/* ---- best rounds ---- */
#define SGB_BEST_ROUND_COUNT 10        /* int32 strokes, ascending, 0 = empty */

/* ---- difficulty ---- */
enum Difficulty { SGB_DIFF_NONE = -1, SGB_DIFF_EASY = 0, SGB_DIFF_MODERATE = 1, SGB_DIFF_DIFFICULT = 2, SGB_DIFF_IMPOSSIBLE = 3 };

/* gb4.h - stream B helper gb4: date, RNG, session flag words, preferences, player skills, message timers,
 * text scratch buffers. C99 fragment, <stdint.h> assumed. Original addresses live in addresses.tsv only.
 * Surface-object arrays this group established (count and stride) use stream D's opaque SgSurfaceObj (0x2c bytes,
 * d.h) and are listed in addresses.tsv with other_stream=D; they are not redefined here. */
/* ---- game date (one int32) -------------------------------------------------------------------------------------
 * 1024 ticks per month, 8 months per year (index 0 = the third calendar month .. 7 = the tenth), year 0 = 2001.
 * One mainLoop frame adds 1; with SGB_GF_DATE_EVEN_STEP set an odd result is rounded up to even. */
#define SGB_DATE_TICKS_PER_MONTH   0x400
#define SGB_DATE_MONTHS_PER_YEAR   8
#define SGB_DATE_TICKS_PER_YEAR    0x2000
#define SGB_DATE_BASE_YEAR         2001
#define SGB_DATE_YEAR(d)           ((d) / SGB_DATE_TICKS_PER_YEAR)              /* signed division, then + 2001 */
#define SGB_DATE_MONTH(d)          (((d) / SGB_DATE_TICKS_PER_MONTH) % 8)       /* 0..7 */
#define SGB_DATE_MONTH_SERIAL(d)   ((d) / SGB_DATE_TICKS_PER_MONTH)             /* history tables index it % 500 */
#define SGB_DATE_TICK(d)           ((d) & 0x3ff)                               /* position inside the month */
#define SGB_DATE_SESSION_START     0x2001   /* mainLoop, course opened while date <= 0x2000 */
#define SGB_DATE_NEW_COURSE        0x2c00   /* startNewCourse */
#define SGB_DATE_HISTORY_MONTHS    500      /* idiv 0x1f4 on the month serial */

/* ---- RNG object (passed as `this` to Random::next / Random::range) -------------------------------------------- */
typedef struct Random {
    uint32_t state;   /* +0x00 LCG: state = state * 0x41c64e6d + 0x3039; output (state >> 16) & 0x7fff */
} Random;
SGB_STATIC_ASSERT(sizeof(Random) == 0x4, Random_size);
#define SGB_RNG_MUL     0x41c64e6du
#define SGB_RNG_ADD     0x3039u
#define SGB_RNG_SEED_K  37u      /* seedFromTime: state = timeGetTime() * 37 */

/* ---- g_flags: session flag word (save field 70) --------------------------------------------------------------
 * Names below follow the mechanics the code shows; SG_GF_B<n> = no meaning determined (U-0160..U-0163). */
#define SGB_GF_DRAWING_WORLD    0x00000001u  /* drawWorldObjects sets on entry, clears on exit; slopeX/slopeY test */
#define SGB_GF_B1               0x00000002u  /* set with a pending build tool, cleared on cancel [U-0163] */
#define SGB_GF_PAUSED           0x00000004u  /* toggled by key code 0x50; gates the per-tick simulation calls */
#define SGB_GF_DATE_EVEN_STEP   0x00000008u  /* date += date & 1 after the tick increment; no setter [U-0160] */
#define SGB_GF_B4               0x00000010u  /* [U-0160] */
#define SGB_GF_B5               0x00000020u  /* [U-0160] */
#define SGB_GF_B6               0x00000040u  /* [U-0160] */
#define SGB_GF_B7               0x00000080u  /* [U-0160] */
#define SGB_GF_B8               0x00000100u  /* updateWalkers sets around a call and clears after; pathStep tests */
#define SGB_GF_B10              0x00000400u  /* toggled by key code 0x54 [U-0160] */
#define SGB_GF_B11              0x00000800u  /* [U-0163] */
#define SGB_GF_B12              0x00001000u  /* [U-0163] */
#define SGB_GF_CUSTOMIZE_ARG    0x00002000u  /* argument of customizeCharacter under SGB_GF_CUSTOMIZE_REQ [U-0162] */
#define SGB_GF_CUSTOMIZE_REQ    0x00004000u  /* frame head calls customizeCharacter, which clears 13 and 14 */
#define SGB_GF_B15              0x00008000u  /* cleared with the tutorial page; stops the message timer [U-0160] */
#define SGB_GF_B17              0x00020000u  /* [U-0160] */
#define SGB_GF_B18              0x00040000u  /* [U-0163] */
#define SGB_GF_B19              0x00080000u  /* golfer step capped at 3 while set [U-0163] */
#define SGB_GF_B20              0x00100000u  /* [U-0160] */
#define SGB_GF_TOURNAMENT       0x00200000u  /* loadGolfPro sets after building the field; gates save tail B */
#define SGB_GF_B22              0x00400000u  /* [U-0163] */
#define SGB_GF_B23              0x00800000u  /* chooseShotTarget sets and clears around its body [U-0163] */
#define SGB_GF_FREE_MONEY       0x01000000u  /* frame head resets the purse to 10000, canAfford always 1 */
#define SGB_GF_B25              0x02000000u  /* [U-0163] */
#define SGB_GF_B26              0x04000000u  /* menu path with course type -1, startNewCourse [U-0161] */
#define SGB_GF_B27              0x08000000u  /* [U-0163] */
#define SGB_GF_STORY_FALLBACK   0x10000000u  /* theme has no text files: story files come from the standard theme */
#define SGB_GF_CASH_KEY_USED    0x20000000u  /* set with purse += 100 by key code 0x2b; never tested */
#define SGB_GF_SAVE_TAIL_A      0x40000000u  /* gates save tail A; the title menu resets the word to this */
#define SGB_GF_TOURNAMENT_ANY   (SGB_GF_TOURNAMENT | SGB_GF_B26)  /* tested together 0x4200000 */

/* ---- g_preferences: six popup toggles (save field 85), showPreferencesMenu; -1 at the title screen ----------- */
#define SGB_PREF_BIT0   0x01u   /* key code 0x4e toggles it [U-0166] */
#define SGB_PREF_BIT1   0x02u   /* [U-0166] */
#define SGB_PREF_BIT2   0x04u   /* emitGolferThought shows its text only while set [U-0166] */
#define SGB_PREF_BIT3   0x08u   /* [U-0166] */
#define SGB_PREF_BIT4   0x10u   /* [U-0166] */
#define SGB_PREF_BIT5   0x20u   /* placeHabitat spawns only while set [U-0166] */
#define SGB_PREF_COUNT  6

/* ---- course type (one int8, save field 4) --------------------------------------------------------------------- */
#define SGB_COURSE_TYPE_SANDBOX   (-1)   /* set on the sandbox menu path */
#define SGB_COURSE_TYPE_COUNT     4      /* 0..3: jump tables of 4 entries */

/* ---- comment tone (one uint32: the colour word buildGolferComment picks) ------------------------------------- */
#define SGB_COMMENT_TONE_A   0x80006318u
#define SGB_COMMENT_TONE_B   0x800023e8u
#define SGB_COMMENT_TONE_C   0x80007d08u

/* ---- player skills block (save field 57): 2 rows of 16 signed bytes, 12 used, values 0..10 ------------------ */
#define SGB_PLAYER_SKILL_ROWS   2
#define SGB_PLAYER_SKILL_STRIDE 16
#define SGB_PLAYER_SKILLS_USED  12
#define SGB_PLAYER_SKILL_MAX    10

/* ---- per placed-object-type table cleared every frame ----------------------------------------------------------- */
#define SGB_OBJECT_TYPE_SLOTS   20
#define SGB_OBJECT_TYPE_CAP     0x63

/* ---- message timer --------------------------------------------------------------------------------------------- */
#define SGB_MESSAGE_TIMER_KEY   0x80   /* value the key path and buildScenarioIntro store */

/* gb5.h - stream B helper gb5 (SimGolf phase-1 data model): simulation odds and ends.
 * C99 fragment; <stdint.h> assumed. Addresses and evidence live in addresses.tsv / notes.md.
 * Every member below is cited in notes.md (instruction addresses). */

/* ---- bulletin-board events: 22 records, saved whole (save field 71) -------------------------------- */
#define SGB_BULLETIN_EVENT_COUNT 22          /* loop end after record 21; clear 0x108 dwords */

typedef struct BulletinEvent {
    int8_t   text[40];                   /* +0x00 file-default text; zeroed by the new-game reset, read by no game code */
    int32_t  date;                       /* +0x28 g_date when first posted, 0 = never posted */
    int32_t  course_id;                  /* +0x2c value of the course-id global when posted */
} BulletinEvent;
SGB_STATIC_ASSERT(sizeof(BulletinEvent) == 0x30, BulletinEvent_size);

/* ---- save field 59: 0x12 bytes, only the word at +8 is addressed ---------------------------------- */
typedef struct SkillMaskBlock {
    uint8_t  unk_00[8];                  /* +0x00 U-0185 */
    uint16_t player_skill_mask;          /* +0x08 bit i set iff skill byte i != 0, i < 12 */
    uint8_t  unk_0a[8];                  /* +0x0a U-0185 */
} SkillMaskBlock;
SGB_STATIC_ASSERT(sizeof(SkillMaskBlock) == 0x12, SkillMaskBlock_size);


/* ---- path finding (pathStep) ----------------------------------------------------------------------- */
#define SGB_PATH_GRID_W      50
#define SGB_PATH_GRID_H      50
#define SGB_PATH_QUEUE_LEN   0x400           /* index & 0x3ff */

/* ---- sprite-sheet column counts g_sheetCols (stream D names it) --------------------------------------------------------------- */
#define SGB_SHEET_SLOT_COUNT 0x24e           /* block clear 0x24e dwords */

/* ---- shot-power cache ------------------------------------------------------------------------------ */
#define SGB_SHOT_POWER_CACHE_LEN 10          /* ring index mod 10 */

/* ---- forced-event request bits (g_forcedEventBits) -------------------------------------------------- */
#define SGB_FORCE_EV_GOLFER_THOUGHT  0x01    /* consumed by addGolferToTable */
#define SGB_FORCE_EV_MAGAZINE        0x02    /* consumed by holeMagazineEvent */
#define SGB_FORCE_EV_STORY_STEP      0x04    /* mainLoop: advanceStory + golfer flag 0x200000, then cleared */
#define SGB_FORCE_EV_MOOD_DROP       0x08    /* mainLoop: nearest golfer mood -1 + thought 8, then cleared */
#define SGB_FORCE_EV_NARRATE         0x10    /* consumed by scanShotArea */
#define SGB_FORCE_EV_DATE_JUMP       0x40    /* mainLoop: date >= 0x2001, skill rating 0x1f4, then cleared */
#define SGB_FORCE_EV_OBJECT_OFFER    0x80    /* mainLoop object-offer path */

/* ---- probe-shot landing tiles (mainLoop far code) --------------------------------------------------- */
#define SGB_PROBE_SHOT_SLOTS 10              /* x and y arrays 0x28 bytes apart */

#endif /* SIMGOLF_MODEL_SECTION_B_H */
