// Saved games, course files and the load-game browser (phase 3.8, stream 38b; port/src/sim/saveload.cpp, tests
// port/test/test_saveload.py). Specifications: the analysis notes named in each comment (re/analysis/<subsystem>/
// <addr>_<name>.md), the C3 hook sources shim/src/re/c3ep.cpp (saveGame, loadGame, loadCourse), c3fa.cpp
// (deserializeCourse), c3fb.cpp (serializeCourse), c3fw.cpp (loadGameBrowserScreen) and re/model/sve_fields.tsv.
//
// FILES. The original opens a file with the CRT's _open and moves bytes with _read / _write through the descriptor
// kept in g_saveFd (saveFieldIo). The port reads a file through the context's GameFiles (portsim/gamefiles.h) and writes
// the finished file back with GameFiles::write when the original closes it; the descriptor value it leaves in g_saveFd is
// the one the CRT hands out for the first file opened (4: the value every recorded save in the segment corpus left,
// U-0922) and -1 when the file cannot be opened (missing for reading, no writable GameFiles for writing). A read that finds
// fewer bytes than asked leaves the rest of the destination as it was, like _read.
#ifndef PORTSIM_SIM_SAVELOAD_H
#define PORTSIM_SIM_SAVELOAD_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// saveGame (re/analysis/save/0040b4a0): path = the saved-games directory + `name` + ".sve" built in g_textBuffer; opens it
// for writing (g_saveFd), builds the 0x64-byte header line (course title, day of the month, date: appendCourseTitle and
// appendDate are called through the registry) in g_textBuffer, writes those 0x64 bytes and serializeCourse(0)'s save
// body, closes. Returns the close result (0, -1 when the file could not be opened).
int32_t save_game(Ctx &ctx, uint32_t name);

// loadGame (re/analysis/save/0040b9b0): path = (championship ? the championship directory : the saved-games directory) +
// `name`; reads the 0x64-byte header (two for a championship file) into g_textBuffer and returns the close result when
// header_only. Otherwise reads the body (serializeCourse(1)), rewrites g_flags (bits 2 and 3 cleared, bit 2 kept from
// the entry value, bit 30 set), clears g_panelListMode, resets two word slots to -1 and rebuilds the terrain tables
// (rebuildHeightfield, rebuildTileCoverage, markTileKind11 through the registry). With `finish` set it shows the loading
// message unless the name is the browsing save's, applies the course type (applyCourseType), reloads the world assets when
// the type changed and refreshes the terrain. The drawing callees (drawWindow, Window::refreshFull, loadWorldAssets,
// refreshTerrain, loadInterfaceArt) are called only when a module registers them. Returns finish (0) when it is 0, else
// the last call's eax (0 here).
int32_t load_game(Ctx &ctx, uint32_t name, int32_t finish, int32_t header_only, int32_t championship);

// loadCourse (re/analysis/save/0040b840): opens the file whose path stands in g_textBuffer, reads its 0x64-byte header
// over the buffer; mode 2 stops there, mode 0 shows the loading message and rebuilds the interface art and bios; a second
// header is read (mode 3 stops there) and the course body is read with deserializeCourse(1); mode 0 then clears
// g_panelListMode, resets the two slots and rebuilds the heightfield. Returns the close result, or rebuildHeightfield's
// eax for mode 0.
int32_t load_course(Ctx &ctx, int32_t mode);

// loadGameBrowserScreen (re/analysis/frontend/0043b610): the modal saved-game / championship-course browser, simulation
// effects only (the drawing, the hot-button tips and the scroll bar are not ported). See the function's comment.
int32_t load_game_browser_screen(Ctx &ctx, int32_t resume, int32_t championship);

// tierPrice (re/analysis/economy/0046f1d0): the price of a course tier 0..15 (500, 600, 700, 800, 1200, 1500, 2000, 2500, 3000, 4000,
// 5000, 6000, 7000, 8000, 9000, 10000), 500 for any other tier (the range test is unsigned).
int32_t tier_price(int32_t tier);

// courseActionMenu (re/analysis/ui/0046f550): the world map where the player picks, buys, saves or loads a course; simulation
// effects only (see the function's comment). Returns -1 after back or a load, or the course record bought.
int32_t course_action_menu(Ctx &ctx);

// saveCourse (re/analysis/save/0040b680): saves the course named in g_textBuffer (two header lines, the course body, the marker).
int32_t save_course(Ctx &ctx);
// saveGameDialog (re/analysis/save/00405b10): the name prompt of the save key, then saveGame.
int32_t save_game_dialog(Ctx &ctx);
// promptAndSaveFile (re/analysis/save/00437910): writes golfer type `index` as a character file (.chr / .pro); 1 when written.
int32_t prompt_and_save_file(Ctx &ctx, int32_t index, int32_t mode, int32_t championship);

}  // namespace portsim

#endif  // PORTSIM_SIM_SAVELOAD_H
