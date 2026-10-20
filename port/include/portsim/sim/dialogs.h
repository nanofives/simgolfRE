// Dialogs (phase 3.8, stream 38d): the skill-point dialog and the screens built on it, and the golfer info panel.
// Module `dialogs` (port/src/sim/dialogs.cpp), tests port/test/test_dialogs.py, notes log/p38/38d_notes.md.
//
// skillPointDialog is a modal loop (U-0865): every pass reads the cursor once (only when it hands out points), asks the
// context's InputSource for one Blocking dispatch, and applies the click it finds to the row arrow under the cursor read
// in that pass. The port runs the original's pass structure, so a trace-driven source (tick.cpp TraceInput) hands each
// recorded dispatch to the pass that follows the same number of cursor reads as in the original.
#ifndef PORTSIM_SIM_DIALOGS_H
#define PORTSIM_SIM_DIALOGS_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// skillPointDialog (title, mask, points, golfer, xbase): returns the skill mask word (sign-extended) when the dialog is
// left through its OK button, -1 at once when points == -1 (one drawing pass only)
int32_t skill_point_dialog(Ctx &ctx, uint32_t title, int32_t mask, int32_t points, int32_t golfer, int32_t xbase);
// showCharacterIntro (): the new player's skill points (10 + 3 per bulletin event); stores and returns the mask
int32_t show_character_intro(Ctx &ctx);
// drawGolferInfoPanel (g, mode): the golfer info panel's simulation effects (no result)
void draw_golfer_info_panel(Ctx &ctx, int32_t g, int32_t mode);
// drawScrollText (text, sel): the scroll-text box's layout (Text_put widths through the context's TextMetrics); returns
// sel. runScrollText (text): the menu loop over it, the chosen entry or -1. scrollTextWindow (text, x, y): the same at a
// position. showBulletinBoard (): the end-of-season board and its skill award.
int32_t draw_scroll_text(Ctx &ctx, uint32_t text, int32_t sel);
int32_t run_scroll_text(Ctx &ctx, uint32_t text);
int32_t scroll_text_window(Ctx &ctx, uint32_t text, int32_t x, int32_t y);
void show_bulletin_board(Ctx &ctx);

}  // namespace portsim

#endif  // PORTSIM_SIM_DIALOGS_H
