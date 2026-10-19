// Course and hole records, placed objects and animals, course titles and the shared text buffer's course / golfer
// names (phase 3 stage B, child 3.1b, 2026-10-19). Specifications: the analysis notes named in each comment
// (re/analysis/<subsystem>/<addr>_<name>.md), the C3 / C4 hook sources they cite, and the vector corpus.
//
// Like the original, nothing here bound-checks an index unless stated: a hole, golfer or table index outside its array
// reads or writes the neighbouring GameState bytes (model_defects.md section 1), through Ptr arithmetic.
// Strings are (pool member, byte offset) pairs as in portsim/sim/text.h; the text itself never appears in port code.
#ifndef PORTSIM_SIM_COURSE_H
#define PORTSIM_SIM_COURSE_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// ------------------------------------------------------------------------------------------------ records
// freeRecordAtPos (re/analysis/course/004011b0): every animal record (100) whose signed x word equals x and y word
// equals y gets type 0xffff (all matches, not only the first).
void free_record_at_pos(Ctx &ctx, int32_t x, int32_t y);
// initRecords (re/analysis/sim/00401750): every animal's type word = 0xffff; then for bank 0..5 resetRecordBank(bank)
// and placeRecord(bank, bank, 0, 0). Returns the last placeRecord result (bank 5).
int32_t init_records(Ctx &ctx);
// resetRecordBank (callee of initRecords): fills 8 dwords of g_recordBanks[bank].id and 9 dwords of .occupancy with -1.
void reset_record_bank(Ctx &ctx, int32_t bank);
// placeRecord (callee of initRecords): first free id slot (-1) of bank `bank` gets (type, row, col, anim 0xff) and
// the footprint bits of g_recordDefs[type] stamp the slot index into the bank's 6-wide occupancy grid; 0 when the bank
// is full, else 1.
int32_t place_record(Ctx &ctx, int32_t bank, int32_t type, int32_t row, int32_t col);
// clearMatching (re/analysis/sim/0040c860): for the 8 label slots i, g_labelQueueB[i] = 0 when g_labelQueueC[i] == a
// and g_floatLabelY[i] == b.
void clear_matching(Ctx &ctx, int32_t a, int32_t b);
// staticInit_820f04 (re/analysis/terrain/00449190): static_820f04 = __ftol(fabs(the exe's double f_k_dbl_4ba620)).
void static_init_820f04(Ctx &ctx);
// setPendingCourseInfo (re/analysis/course/004321a0): inline strcpy of the string at `s` into g_pendingCourseInfo
// (no bound: a string of 0xdc bytes or more runs into the following objects). eax = strlen(s) + 1.
uint32_t set_pending_course_info(Ctx &ctx, uint32_t s);
// clearBuffers (re/analysis/util/0045b880): g_namePool zeroed (0x1002 bytes), g_namePoolOffset filled with 0xff.
void clear_buffers(Ctx &ctx);

// ------------------------------------------------------------------------------------------------ names and titles
// appendString (re/analysis/util/0045b9f0): 0 when id == -1 or g_namePoolOffset[id] == -1; else strcat of
// g_namePool + offset onto g_textBuffer and 1. id is not bound-checked.
int32_t append_string(Ctx &ctx, int32_t id);
// landmarkName (re/analysis/course/004074a0): strcat of landmark `type`'s long (full != 0) or short name; type 10 has
// one name; any type outside 0..18 appends the default name.
void landmark_name(Ctx &ctx, int32_t type, int32_t full);
// buildGolferName (re/analysis/golfer/004676e0): appends golfer g's display name (title first when withTitle != 0),
// the form chosen by the kind byte's class bits (golfer_record.md).
void build_golfer_name(Ctx &ctx, int32_t g, int32_t with_title);
// buildStatusText (re/analysis/ui/00406d50): priority text of the status line; see the function's comment.
int32_t build_status_text(Ctx &ctx, int32_t golfer, int32_t priority);
// appendHoleName (re/analysis/ui/00407280): the hole's custom name (appendString(hole) when flags & 0x81), else by par
// from the three name tables; without the flags "Hole " + _itoa(hole).
void append_hole_name(Ctx &ctx, int32_t hole);
// appendCourseTitle (re/analysis/ui/0040daa0): custom title (appendString(0)) or the course site's location name, then
// unless full == -1 a size suffix by bucketValue(g_holesPlusOne - 1), long form when full == 1.
void append_course_title(Ctx &ctx, int32_t full);
// The static CRT's _itoa(v, buf, 10): '-' and the digits of the magnitude (INT_MIN gives 2147483648); returns buf.
uint32_t crt_itoa10(Ctx &ctx, int32_t v, uint32_t buf);

// ------------------------------------------------------------------------------------------------ geometry
// nearestHole (re/analysis/course/00407340): 1-based number of the hole whose tee, second tee or pin point is nearest
// (x, y) by distance(); strict improvement only (ties keep the earlier hole); -1 when nothing is below 0xffff.
int32_t nearest_hole(Ctx &ctx, int32_t x, int32_t y);
// holeQuadrants (re/analysis/course/00407000): bit (value & 3) for each placed object of type 4 with value < 0x10
// within ((value * 5 + 0x28) * 5) / 3 tiles of (a, b) by tileDistance; the mask goes to g_holeDirMask; returns
// (mask & want) == want.
int32_t hole_quadrants(Ctx &ctx, int32_t a, int32_t b, uint32_t want);

// ------------------------------------------------------------------------------------------------ course table, intro
// seedCourseTable (re/analysis/sim/0046f2b0): seeds the 16 course records from the location table with g_rng draws.
void seed_course_table(Ctx &ctx);
// buildScenarioIntro (re/analysis/frontend/0045fd80): the scenario back-story at g_date 0x20 and the follow-up at 0x1400,
// both through startMessage.
void build_scenario_intro(Ctx &ctx);

// ------------------------------------------------------------------------------------------------ scenery, files, list
// sceneryLabel (re/analysis/course/00407b60): scenic dedication label of tile (a, b); calls 3.1a's decorationName through
// the registry (MissingResource while no module registers it).
int32_t scenery_label(Ctx &ctx, int32_t a, int32_t b, int32_t text);
// fileExists (re/analysis/util/004378a0): 1 when the logical name at `name` exists in the context's GameFiles.
int32_t file_exists(Ctx &ctx, uint32_t name);
// lookupThemeText (re/analysis/course/0045b660): opens the file at `path` in CRT text mode and reads lines of up to 0xf9
// characters, each losing its last character, until one equals `key` case-insensitively (-1 after 100 that do not);
// then appends each following line + the one-byte separator to g_textBuffer until a line starting with '*' or 100
// lines; returns the number of lines read after the key. A missing file faults like the original's unchecked fopen.
int32_t lookup_theme_text(Ctx &ctx, uint32_t path, uint32_t key);
// listEraseRange (re/analysis/util/0044ac00): std::list::erase(first, last) of the list `self` (thiscall); returns out.
uint32_t list_erase_range(Ctx &ctx, uint32_t self, uint32_t out, uint32_t first, uint32_t last);

// loadThemeFile (re/analysis/course/00437fa0): opens Themes\<g_theme>\<name> (index <= 0x4c) or Themes\Standard\<name>
// (built in g_textBuffer) as a binary file; 0 when absent. Reads golfer type `index`'s 0x230-byte record, its 0x4e2-byte
// phrase record and a 16-byte skill record (copied to golfer `slot`'s skills, and to g_playerSkills for index 0, when
// slot != -1 and (index != 0 or g_flags bit 0x4000000)); a type whose body byte is >= 0x14 gets the next free portrait
// tile of its sex (g_loopCounter59b76c, below 0x48). The portrait decode and tile blits are the render backend's (not
// ported). Returns 1.
int32_t load_theme_file(Ctx &ctx, uint32_t name, int32_t index, int32_t slot);
// loadThemes (re/analysis/course/004658b0): loads every .chr file of the current theme as golfer types 1.. .
int32_t load_themes(Ctx &ctx, int32_t enable);

// switchToCourse (re/analysis/course/00407d30): unless g_flags bit 0x4000000, courseActionMenu(arg) (ui, through the
// registry) picks a course; -1 cancels; otherwise switch_to_course_chosen.
void switch_to_course(Ctx &ctx, int32_t arg);
// The simulation part of switchToCourse once the menu answered `chosen` (for the 3.7 command layer): files the old
// course's year_left and the finance row, sets g_courseType and rebuilds the course through 3.1a's functions.
void switch_to_course_chosen(Ctx &ctx, int32_t chosen);

}  // namespace portsim

#endif  // PORTSIM_SIM_COURSE_H
