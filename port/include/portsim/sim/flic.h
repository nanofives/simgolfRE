// The FLIC sprite-sheet decoder (roadmap S3.4, 2026-10-24; notes log/roadmap/S3/4_notes.md). Module `flic`
// (port/src/sim/flic.cpp), tests port/test/test_flic.py.
//
// loadFlic (a C2 row: re/analysis/render/0043d740_loadFlic.md, written from its instructions) opens a .flc animation (and an
// optional shadow animation) and cuts its frames into the cells of a sprite sheet: per cell it clears a 480 x 480 8-bit
// working image to 0xff, draws the shadow's and the main animation's current frame into it (bytes >= 0xfe transparent) and
// crops a rectangle that depends on loadFlic's fourth argument, trimmed of 0xff rows and columns (cropSprite). The port's
// simulation (menu.cpp load_flic) keeps only where the cells land in the cell pool; this decoder makes the cells' pixels,
// their geometry and the sheet's palette from the user's own .flc files, read through GameFiles.
//
// The decoder is plain C++ over bytes: it reads no GameState and writes nothing into it. Its oracle is the original's sheet
// memory recorded after each load (re/tools/flic_cells_rec.py -> port/corpus/flic/).
#ifndef PORTSIM_SIM_FLIC_H
#define PORTSIM_SIM_FLIC_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/registry.h"

namespace portsim {
namespace flic {

// A load's outcome. Every non-zero status is a case where the ORIGINAL faults or does not return (the port reports it and
// makes no cells for it):
enum Status : int32_t {
  kOk = 0,
  kMainOpenFailed = 1,    // the main .flc is missing or fails Flic::open's checks: flicOpenStream returns NULL and loadFlic's
                          // Image_draw reads through it (U-1701: every tropical course on the shipped install)
  kShadowOpenFailed = 2,  // the shadow file exists but fails Flic::open's checks: the same NULL draw
  kNoNextFrame = 3,       // flicNextFrame returns NULL (the last frame of the last sequence was passed): the same NULL draw
  kDecodeFault = 4,       // a chunk writes or reads outside its frame or file (the original corrupts memory there)
  kUnconverted = 5,       // a header without SimGolf's 0xf1f1f2f2 conversion mark (not modelled; the shipped files are all
                          // converted)
  kBadCrop = 6,           // a crop rectangle with a negative corner (cropSprite reads before the image; not reached by the
                          // shipped tables)
};

// One sheet cell as cropSprite leaves it: `result` is cropSprite's (0 = a sprite, 9 = nothing left after the trim, 3 = the
// rectangle leaves the image), the SpriteRef words +0x10 (kept width), +0x14 (kept height), +0x18 (crop width), +0x1c (crop
// height), +0x20 (left trim), +0x24 (top trim), +0x28 (kind, 1) and the kept rectangle's bytes, row by row (pitch = width).
struct Cell {
  int32_t result = 0;
  int32_t keep_w = 0, keep_h = 0, w = 0, h = 0, left = 0, top = 0, kind = 1;
  std::vector<uint8_t> px;
};

// The 256 three-byte entries the flic's colour chunks (types 4 and 11) leave in the palette object of the load's slot
// (Palette::decodeChunk; every shipped colour chunk sets all 256 entries). `set` is false when the load decoded none.
struct Palette {
  bool set = false;
  std::array<uint8_t, 0x300> rgb{};
};

struct Load {
  std::string path;       // loadFlic's path argument, e.g. "bldgs\park\sign" (the files are flics\<path>.flc and
                          // flics\<path>Shadow.flc)
  int32_t frames = 1;     // loadFlic's third argument: the sheet's rows
  int32_t size = 1;       // its fourth argument: the crop rectangle's selector (-1 = one cell only)
  int32_t flag = 0;       // its fifth argument: non-zero = draw the shadow animation under the main one when it exists
};

struct Result {
  int32_t status = kOk;
  std::string detail;           // the failing file or chunk for a non-zero status
  int32_t cols = 0;             // the main flic's frames per sequence (header word +0x62), loadFlic's column count
  int32_t seqs = 0;             // its sequences (header word +0x60)
  bool shadow = false;          // the shadow animation was drawn
  std::vector<Cell> cells;      // in the cell cursor's order: row by row, column by column (one cell for size -1)
  Palette pal, shadow_pal;      // the load's slot and the shadow slot after the load
};

// The file names loadFlic builds: "flics\" + path + ".flc" and "flics\" + path + "Shadow.flc" (the exe's strings, read by the
// caller from the user's exe; passed in so this decoder holds none)
struct Names {
  std::string prefix, main_suffix, shadow_suffix;
};

// The crop rectangle loadFlic cuts for a size argument (the four arms after its frame draw)
struct Rect {
  int32_t x, y, w, h;
};
Rect crop_rect(int32_t size);

Result load(const GameFiles &files, const Names &names, const Load &args);

// The file name a load opens (log/roadmap/S3/4b_merge.md section 1; works with no gfx backend): a name holding `Tree Cerc` /
// `Tree Drac` whose file is missing raises Fault{8, 4} at switch flic.treeAliasNames = 1 (the stock game's death: an access
// violation reading 0x8 in Image_draw on the NULL frame Surface, U-1330) and is returned with the install's underscore
// spelling at 0; every other name is returned unchanged.
std::string resolve_name(const GameFiles &files, const std::string &name);

// The hook-up for menu.cpp's load_flic (log/roadmap/S3/4_merge.md section 2): with a gfx backend attached to the context,
// decodes the load (path = the loadFlic path argument's address, the exe's string; the file-name parts are the exe's
// strings menu.cpp already builds the names from) and puts each cell into the backend's sprites under its SpriteRef
// address first_cell_va + 0x2c * k (a cell cropSprite left empty is removed). Without a backend it does nothing and
// returns -1; otherwise the load's Status (a non-zero one is reported as unsupported, no cells are set). Writes nothing
// into the GameState.
int32_t sheet_to_gfx(Ctx &ctx, uint32_t path_va, int32_t frames, int32_t size, int32_t flag, uint32_t first_cell_va);
// P6B (measurement only): the wall time all sheet_to_gfx calls of the process took (microseconds) and their count; the app
// logs the deltas per world load (session.json `loads`)
void sheet_to_gfx_stats(uint64_t &us, uint64_t &calls);

}  // namespace flic
}  // namespace portsim

#endif
