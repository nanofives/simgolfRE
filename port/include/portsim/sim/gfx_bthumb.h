// P41 feedback 1, item 8 (log/roadmap/P41/ITEM8_BRIEF.md, notes log/roadmap/P41/feedback1/item8_notes.md): the build palette's
// building thumbnails drawn from the game's own WORLD art at the window's resolution (theme on only).
//
// The thumbnails are cuts of loadWorldAssets' layout sheet (`interface\<theme> Layout`, ten columns k of four 75 x 100 cells:
// row 0 level-0 grey, 1 upgraded grey, 2 level-0 colour, 3 upgraded colour; drawBuildingPanel draws slot j from column j). Slot j
// is placed-object type j + 6 (pickBuilding stores the tool as slot + 6; mainLoop's tool switch and buildSceneSprites take it as the object type). With a UI theme on, such a cut is drawn as a UI run
// "uvg:B<t>.<slot>.<row> <dx> <dy>" whose picture is that object composed from the world's own sheet cells (the cells the course
// view draws: cellAt(bank, frame 0, facing 0) in the backend's sprites, with the palette object buildSceneSprites draws them with),
// placed as buildSceneSprites places them, fitted to the art's thumbnail (scale and place measured per course type, slot and
// level: log/roadmap/P41/feedback1/item8/fit_all.py) and resampled from the cells' own pixels at the window's scale. Rows 0 / 1
// take the art's grey filter, measured from the four layout sheets (grey row vs colour row of the same column, 157,523 pixel
// pairs): L = 0.299 R + 0.587 G + 0.114 B, R' = 1.0042 L - 13.22, G' = 1.0076 L - 2.62, B' = 0.9728 L + 37.56 (clamped; rmse
// 6.4 / 6.5 / 6.8 per channel, the palettes' quantisation). The driving range (slot 4) keeps its art: no world picture matches
// its thumbnail. Theme off: none of this runs (the art and the original's calls).
//
// Negative controls (g_negctl_uvgame, sg_negctl_set "uvgame.render"; 3 already turns every uvg hook off):
//   8 the grey filter left out: locked and built buildings in full colour
//   9 the building thumbnails drawn from their art (this hook off, the rest of the theme on)
#ifndef PORTSIM_SIM_GFX_BTHUMB_H
#define PORTSIM_SIM_GFX_BTHUMB_H

#include <cstdint>
#include <string>
#include <vector>

namespace portsim {

class Ctx;
class Gfx;
struct Sprite8;

// Sim side (gfx_uvgame.cpp uvg_draw, theme on): sprite s drawn with its kept pixel (0, 0) at (x, y). True when it is a layout-sheet
// thumbnail with a world picture; then the run was drawn (ui_draw_uvec) and the art must not be.
bool bthumb_draw(Gfx &g, Ctx &ctx, const Sprite8 &s, int32_t x, int32_t y);
inline bool bthumb_is_text(const std::string &t) { return t.size() > 5 && t.compare(0, 5, "uvg:B") == 0; }
bool bthumb_bounds(const std::string &text, float &x0, float &y0, float &x1, float &y1);
// gfx_ui.h ui_render's contract (4 floats per target pixel, premultiplied colour 0..255 and coverage), drawn over prem
bool bthumb_render(const std::string &text, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
                   std::vector<float> &prem);

}  // namespace portsim

#endif
