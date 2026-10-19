// Economy and membership (phase 3.2, 2026-10-19): rating text, clearing and lot values, ball-record decay, golfer
// membership draws. Specifications: re/analysis/<subsystem>/<addr>_<name>.md of each function, the hook sources (reading
// reference) and the vector corpus. Text literals are named as (pool member, byte offset); the text never appears here.
#ifndef PORTSIM_SIM_ECONOMY_H
#define PORTSIM_SIM_ECONOMY_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// computeRatingTrend: weighted average of hole record idx's histogram rows (column picked by sel 1 / 2 / 4) minus the
// all-columns average, both x100; 0 when the last count is 0 or sel picks a zero count.
int32_t compute_rating_trend(Ctx &ctx, int32_t idx, int32_t sel);
// appendDate: g_year = date / 8192; strcat "<month name> <year + 2001>" onto g_textBuffer (month = date / 1024 % 8, unchecked).
void append_date(Ctx &ctx, int32_t date);
// appendNumber: strcat n onto g_textBuffer with thousands separators (',' and zero padding), '-' for negatives.
void append_number(Ctx &ctx, int32_t n);
// appendRatingLabel: strcat of the label for v (negative, v / 25 in 0..3, above 3).
void append_rating_label(Ctx &ctx, int32_t v);
// appendToBuffer45b8b0: store g_textBuffer into the g_namePool text store (slot id, or the first free slot above 0x20
// for -1); returns the slot or -1 when it does not fit.
int32_t append_to_buffer_45b8b0(Ctx &ctx, int32_t id);
// clearCost: cost of clearing tile (x, y) by type (0 blocked, 12 / 0 / -16 / -8 / 32 / 16 special types, cost table otherwise).
int32_t clear_cost(Ctx &ctx, int32_t x, int32_t y);
// rateLot: value of a 2x2 lot at (x, y): ring clearCost sum times the best hole value / 40.
int32_t rate_lot(Ctx &ctx, int32_t x, int32_t y);
// updateRollingStats: one step of the 256 ball records (move, height, decay, free after the grace period).
void update_rolling_stats(Ctx &ctx);
// updateMembership: draw a new golfer into slot g_nextGolferSlot % 0x98; returns the slot, or -1 after 999 refused type draws.
int32_t update_membership(Ctx &ctx, int32_t members_only);

}  // namespace portsim

#endif  // PORTSIM_SIM_ECONOMY_H
