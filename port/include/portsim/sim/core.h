// Core state helpers of the simulation (phase 3.0, 2026-10-19): static initialisers and whole-record moves.
// Specifications: re/analysis/sim/0045c190_staticInit_gRngZero.md, re/analysis/util/0045de30_swapTableEntry.md,
// re/analysis/data/golfer_record.md (156 records of 0x100 bytes; record 152 is the scratch record).
#ifndef PORTSIM_SIM_CORE_H
#define PORTSIM_SIM_CORE_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

constexpr int32_t kGolferScratch = 152;  // golfer record used as the swap buffer (golfer_record.md)

// staticInit_gRngZero: the C++ static initialiser of g_rng (state = 0).
void static_init_rng_zero(Ctx &ctx);
// swapTableEntry: swaps golfer records a and b through the scratch record 152 (three 0x40-dword copies:
// a -> scratch, b -> a, scratch -> b; indices unchecked; the scratch record keeps a's old contents).
void swap_table_entry(Ctx &ctx, int32_t a, int32_t b);

}  // namespace portsim

#endif  // PORTSIM_SIM_CORE_H
