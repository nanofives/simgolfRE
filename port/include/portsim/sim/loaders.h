// File listings and the roster loader (phase 3.8, stream 38b; port/src/sim/loaders.cpp, tests port/test/test_loaders.py).
// Specifications: the analysis notes named in each comment (re/analysis/<subsystem>/<addr>_<name>.md), the C3 hook sources
// they cite and the emulator oracle of port/test/cmdemu.py. Files are read through the context's GameFiles
// (portsim/gamefiles.h); strings are (pool member, byte offset) pairs, the text itself never appears in port code.
#ifndef PORTSIM_SIM_LOADERS_H
#define PORTSIM_SIM_LOADERS_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// listMatchingFiles (re/analysis/util/0043d2a0): clears the 100-slot name table (g_fileNames, 0x64 bytes per slot), lists
// `pattern` through GameFiles::list and keeps the names that pass the filters: `require` (when not empty) must be a
// substring, `exclude` (when not empty) must not be, and the two fixed strings "Shadow" / "shadow" never are. A kept name
// is appended to g_textBuffer followed by the two-byte separator and copied into the next slot (the cursor is not bounded
// by the table: slot 100 onwards writes the following objects, like the original). Returns the count; a listing the
// directory refuses (no such directory) returns 0 at once.
int32_t list_matching_files(Ctx &ctx, uint32_t pattern, uint32_t require, uint32_t exclude);

// loadGolferRoster (re/analysis/golfer/004659a0): the four default golfer files, the theme's story-file list (falling back
// to the standard theme's, which sets g_flags bit 28), the story index, the celebrity and pro-golfer data files of the
// theme (or of the standard theme) and the theme's .pro golfers, appended after the pro records read. See the function's
// comment for the effects; returns the original's eax (the .pro list's count when it is <= 0, else 0). Both data files
// missing shows a message through scrollTextWindow (a ui function, called through the registry:
// MissingResource{"scrollTextWindow"} when no module registers it).
int32_t load_golfer_roster(Ctx &ctx);

}  // namespace portsim

#endif  // PORTSIM_SIM_LOADERS_H
