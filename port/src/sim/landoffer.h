// P38H (Mariano 2026-11-19, log/roadmap/P38/P38H_notes.md): I.M.Picky's land offer kept as purchase credits.
// The original: patronEvent's approval sets bit 0 of g_eventFlags567a1c, mainLoop's frame shows the land popup once and clears
// the word on every path, so one approval allows at most one purchase there and then (play switch mainLoop.landOfferOnce = 1).
// Fixed (0): the approval tops the course's credits up to 2; the popup still shows ("No, I'm fine." keeps them); the routing
// map (showRoutingMapScreen) shows a Buy land button while credits remain, which closes the map and asks mainLoop's frame for
// the TRACTS FOR SALE screen without the popup (bit 1 of g_eventFlags567a1c, a bit no code of the original tests: U-0168);
// each purchase still adds 1 to g_landPurchaseCount and takes one credit.
// The credits live in byte +0x21 of the current course's g_courseTable record, a byte only the whole-table clear, save and load
// touch (the whole-image scan, P38H_notes.md section 4), so they are saved in the .sve and belong to that course.
#pragma once

#include <cstdint>

#include "portsim/sim/commands.h"

namespace portsim {

extern int g_defect_land_offer_once;   // mainLoop.landOfferOnce (1 = the original)
constexpr uint32_t kLandOfferBuyNow = 1u << 1;   // g_eventFlags567a1c: open the buy-land screen without the popup
constexpr int32_t kLandOfferCredits = 2;         // what one approval tops the credits up to

// the switch is at 0 and the current course has a record (g_courseIndex 0 .. 16)
bool land_offer_credits_on(Ctx &ctx);
// the current course's credits (0 when land_offer_credits_on is false)
int32_t land_offer_credits(Ctx &ctx);
void set_land_offer_credits(Ctx &ctx, int32_t n);   // clamped to 0 .. 255; nothing when land_offer_credits_on is false

}  // namespace portsim
