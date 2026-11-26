// P41G (log/roadmap/P41/PORT_INGAME_BRIEF.md, log/roadmap/P41/portgame/notes.md): the in-game interface drawn from the approved uivec
// element scenes (re/tools/uivec/scenes/<area>/<id>.json, exported per area by re/tools/uivec/port_game.py into
// port/src/sim/uivec_game_<area>.inc).
//
// KEYED BY THE DRAWN SPRITE. Every in-game interface picture is a cut the game made with cropSprite from one of its sheets, and
// crop_sprite (gfx.cpp) keeps that provenance on the Sprite8 (sheet, the crop rectangle's corner and size). A record of an element
// state is (sheet, cut rectangle in the sheet, scene); a drawn sprite takes every record of its sheet whose cut lies inside its crop
// rectangle covers to at least 80 % (the approved cuts are the art's trimmed boxes, inside the game's crops; a few are one to three
// pixels larger), so the state the game selects (idle, hover,
// pressed, disabled, selected: each a cut of its own) picks its own scene with no table of the game's SpriteRefs. The scene is drawn
// moved by the sprite's draw point minus its crop corner (the kept pixel (0, 0) lands at the draw point, as every primitive draws it),
// each record painted only inside its own cut there (hp.py render_state paints a scene over its cut's box: the approved look).
//
// The sim side (gfx.cpp, the sprite primitives draw16 / draw_g16_lut / Surface_stretchBlit / the scene family's blit16 /
// Surface_drawRegion, and gfx_ui.cpp ui_swap before P40's own HUD panels): with a UI theme on, a sprite with records draws them as a P40
// UI run (face kUiRunFace, text "uvg:..."), whose 1x render goes into the root (the frame the game's later draws and texts go over)
// and which the presenter re-renders at the window's resolution (port/app/textres.cpp). The art is not drawn. With the theme off
// none of this runs: the primitives draw the art (the original's calls).
//
// keep_art (hud_palettes): an element whose approved scene keeps a picture holds a KEEP ITEM: the sheet's own pixels where the union
// of the keep shapes covers (nearest neighbour, edge coverage clip(0.5 - d s), opaque art pixels only), after every layer (hp.py
// render_state). The sheet's 8-bit pixels are read from the game files on the first draw (uvg_prepare_sheet).
//
// Negative controls (g_negctl_uvgame, sg_negctl_set "uvgame.render"):
//   1 every scene drawn one frame pixel to the right of its place (the placement): the DLL's draw test reads RED
//   2 the bevel's normal upside down: the parity reads RED
//   3 the hook off: every sprite draws its art (the theme-off picture with the theme on)
//   4 no layer boxes (every layer evaluated at every pixel of its item: the render is the same, only slower; the speed reference)
//   5 every pixel a scene covers marked as the panel layer's write (gfx_ui.cpp draw_uvec; the default marks only the wholly covered
//     ones): the anti-aliased ring stays an opaque layer pixel holding the world of the panel's unmoved place (the moved layer's
//     one-pixel fringe, before P41G's fix)
//   6 a picture item's matte without its solid and clip fields (plain ground_diff: pinholes in the badge, pieces of the lines beside
//     it lifted): the picture's parity reads RED
#ifndef PORTSIM_SIM_GFX_UVGAME_H
#define PORTSIM_SIM_GFX_UVGAME_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace portsim {

class Ctx;
class Gfx;
struct Sprite8;
struct UvFrame;

// the element states of every exported area (port_game.py EXPORTED order, each area's records in its include's order)
int32_t uvg_cut_count();
struct UvgCutInfo {
  const char *area, *id, *state, *sheet;
  int32_t x, y, w, h;      // the cut in the sheet
  int32_t shift_x, shift_y;
  int32_t nitems;
  bool keep;               // holds a keep item
};
bool uvg_cut_info(int32_t i, UvgCutInfo &o);

// the records a sprite takes (indices into the cut list; empty when none)
// piece (optional): set when a record matched as a PIECE (the crop lies inside a larger record's cut: a 9-slice frame piece), which is
// drawn clipped to the crop
void uvg_match(const Sprite8 &s, std::vector<int32_t> &out, bool *piece = nullptr);
// the run text of records `cuts` drawn with offset (dx, dy) (frame = sheet + d): "uvg:<i>[+<j>...] <dx> <dy>"; each record is painted
// only inside its own cut at its drawn place (the box its scene was approved over)
std::string uvg_run_text(const std::vector<int32_t> &cuts, int32_t dx, int32_t dy, const int32_t *crop = nullptr);
inline bool uvg_is_run_text(const std::string &t) { return t.size() > 4 && t.compare(0, 4, "uvg:") == 0; }
bool uvg_run_bounds(const std::string &text, float &x0, float &y0, float &x1, float &y1);
// gfx_ui.h ui_render's contract: 4 floats per target pixel, premultiplied colour (0..255) and coverage
bool uvg_run_render(const std::string &text, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
                    std::vector<float> &prem);
// the most threads a render on the calling thread splits into (0: the machine's, up to 16); the presenter's worker sets half
void uvg_set_render_threads(int32_t n);
// the run's 1x render over its whole-pixel bounds, kept per text (the sim draws the same HUD runs every frame)
std::shared_ptr<const UvFrame> uvg_frame(const std::string &text);

// the sheet's 8-bit pixels for keep items (read once per sheet from the game files); false when it cannot be read
bool uvg_prepare_sheet(Ctx &ctx, const std::string &sheet);

// Sim side: draws sprite s's records at its draw point (x, y) (where the kept pixel (0, 0) goes) as a UI run; false (nothing drawn)
// when the theme is off, the sprite has no records or negative control 3 is on. Called by the primitives through uvg_hook.
bool uvg_draw(Gfx &g, Ctx &ctx, const Sprite8 &s, int32_t x, int32_t y);

}  // namespace portsim

#endif
