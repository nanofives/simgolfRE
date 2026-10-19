// Text leaves of the simulation core (phase 3.0, 2026-10-19): appending names to the shared text buffer, file-name checks.
// Specifications: re/analysis/ui/0040a9a0_appendClubName.md, re/analysis/util/00405ac0_sanitizeFileName.md.
//
// STRINGS. The original's literals live in the exe's .data string pools, which are GameState objects of class `const`
// (f_strpool_*: filled from the user's exe by ExeConstants::fill_state, seeded by the vector corpus in tests). Port
// code names a literal as (pool member, byte offset in the pool) and copies it with the string idioms of
// portsim/ptr.h; it never contains the text itself.
#ifndef PORTSIM_SIM_TEXT_H
#define PORTSIM_SIM_TEXT_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// appendClubName: strcat of club `club`'s name (0 Driver .. 13 Putter) onto g_textBuffer; an index above 13
// (unsigned) leaves the buffer unchanged.
void append_club_name(Ctx &ctx, int32_t club);
// sanitizeFileName: 0 when the string at `s` contains a byte of the pool charset f_strpool_4c3ee0 (strpbrk);
// otherwise strips trailing spaces in place and returns 1 when the result is not empty.
int32_t sanitize_file_name(Ctx &ctx, uint32_t s);

}  // namespace portsim

#endif  // PORTSIM_SIM_TEXT_H
