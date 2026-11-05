// Body family `text` of the software graphics backend (roadmap S3.7; port/src/sim/gfx_text.cpp, listed in
// port/src/sim/gfx_families.inc): the exe's marked-up text on the root surface (drawRichText's markup scanner, the run drawer
// drawTextRun with its shadow run and palette colours, drawMarkupRun's width), the message window drawWindow (the ticker and
// the help windows, with the golfer portrait), drawWindowFrame, and the HUD panels' hover tips.
//
// The family keeps the root text object's state the original keeps in the Image at g_rootWindow + 0x274 (the three style /
// dropdown sets of drawRichText, drawMarkupRun and measureMarkupRun, the link state, the per-style colours) in the Gfx's
// family state. Nothing is written into the GameState for good: a text the original builds in g_textBuffer is built there and
// the buffer put back, as the other families do.
#ifndef PORTSIM_SIM_GFX_TEXT_H
#define PORTSIM_SIM_GFX_TEXT_H

#include <cstdint>
#include <string>

namespace portsim {

class Ctx;
class Gfx;

namespace gfxt {

// Widget_setQuad (k = 0) / Widget_setQuad70 / 74 / 78 (k = 1 / 2 / 3) on `target`: the style's main colour, shadow colour and
// shadow offsets (the Image's +0x6c, +0x7c, +0x8c, +0x9c plus 4 k)
void set_quad(Gfx &g, uint32_t target, int k, uint32_t colour, uint32_t shadow, uint32_t dx, uint32_t dy);
// The core text helper's path (gfx.cpp gfxb::draw_text_string hands over a text holding a markup character or a tab, or a
// colour that is a palette index): drawRichText on the root at (x0, y0), centred on x0 by drawMarkupRun's width when `centre`,
// every run in `colour` (the caller's state-0 main colour) with no shadow run (the callers draw their shadow pass themselves)
void draw_string(Gfx &g, Ctx &ctx, const std::string &text, int32_t x0, int32_t y0, uint32_t colour, bool centre,
                 const char *who);
// Text_put on the root surface: drawMarkupRun(text, strlen(text)) in the root's fonts (0 when no text provider can measure)
int32_t text_put(Gfx &g, Ctx &ctx, const std::string &text);

// The hover tips of the HUD panels (gfx_hud.cpp hands them over where S3.6 reported them; roadmap S3.7, U-1353)
// showInfoCursorPanel's tip of tool `sel` (0..0x13) and of a slot (sel >= 0x14) after ten frames of dwell
void info_panel_tool_tip(Gfx &g, Ctx &ctx, int32_t sel);
void info_panel_slot_tip(Gfx &g, Ctx &ctx, int32_t sel);
// drawTerrainToolbar's tip of `sel` at the cursor (a, b)
void terrain_toolbar_tip(Gfx &g, Ctx &ctx, int32_t sel, int32_t a, int32_t b);
// drawGolferActionPanel's / drawEmployeeViewPanel's / drawEmployeeDetailPanel's tip of the hovered control at the cursor (x, y)
void action_panel_tip(Gfx &g, Ctx &ctx, int32_t hit, int32_t x, int32_t y);
void view_panel_tip(Gfx &g, Ctx &ctx, int32_t hover, int32_t x, int32_t y);
void detail_panel_tip(Gfx &g, Ctx &ctx, int32_t hit, int32_t x, int32_t y);
// drawAmenitiesPanel's dwell tip of item L and its mode-4 hovered building row r, at the cursor (a, b)
void amenities_tip(Gfx &g, Ctx &ctx, int32_t L, int32_t a, int32_t b);
void amenities_row_tip(Gfx &g, Ctx &ctx, int32_t r, int32_t a, int32_t b);

// P11C (log/roadmap/P11/P11C_notes.md): the hover help. hover_help_line(t) = tile type t's name and short clauses from its live
// g_tileTypeAttrs record ("" for no type); hover_help_tool = palette tool sel's line above showInfoCursorPanel's tip;
// hover_help_terrain = the line of the tile under the cursor (cx, cy) after the HUD tips' dwell. Drawing only (gfx.h
// gfx_set_hover_help); the GameState is read, never written.
std::string hover_help_line(Ctx &ctx, int32_t t);
void hover_help_tool(Gfx &g, Ctx &ctx, int32_t sel);
void hover_help_terrain(Gfx &g, Ctx &ctx, int32_t cx, int32_t cy);

}  // namespace gfxt
}  // namespace portsim

#endif
