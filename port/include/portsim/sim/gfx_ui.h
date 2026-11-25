// P40 (log/roadmap/P40/BRIEF.md, P40_notes.md): procedural UI drawn at the window's resolution, piloted on the Hole Stats screen.
//
// A UI SCENE is a list of vector elements in the 800 x 600 frame's coordinates (rounded rectangles with per-corner elliptic radii,
// ellipses and line segments, combined by a smooth union), each with a vertical gradient fill, an inner outline band, a bevel lit
// from the upper left (raised or sunken), a soft drop shadow and a gloss spot. ui_render rasterises a scene at any scale through a
// signed distance per pixel, so the same scene gives the 800 x 600 frame's pixels (scale 1, drawn into the root by the `ui` family,
// port/src/sim/gfx_ui.cpp) and the window's own pixels (the presenter, port/app/textres.cpp, at the content rectangle's scale).
//
// Transport: the root drawing is kept as a P9C text run (gfx.h GfxTextRun) whose face is kUiRunFace and whose text is the scene
// encoded (ui_encode, printable hex). The run is captured, peeled and published exactly as a text run (its visibility is the
// pixels of its box still holding the words it wrote), so everything the game draws later over the panel (the texts, the close
// button's hover image, a tip) stays over it, and the presenter's text paths (CPU draw, GPU tables) redraw it at the window's
// resolution through TextRaster, which hands a UI run to ui_render instead of GDI.
//
// THEMES (UiTheme): the colours, radii, outline widths, bevel and shadow of every piece. kUiThemeGame holds the values measured
// from the original's pixels (log/roadmap/P40/holestat_geometry.txt: interface\infoscreens\HoleSTAT.pcx and OkStates.pcx of the
// user's install, measured by log/roadmap/P40/measure_holestat.py); kUiThemeSlate is an alternate palette showing the control a
// theme gives. No art byte is copied: only measured numbers.
#ifndef PORTSIM_SIM_GFX_UI_H
#define PORTSIM_SIM_GFX_UI_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace portsim {

class Ctx;

// ---------------------------------------------------------------------------------------------------------- the option
// --ui-pilot off|game|slate (app main.cpp; default off): the Hole Stats screen's frame, title plate, label pills, value boxes,
// connectors, score ovals, comment box and close button drawn by the `ui` family instead of the HoleSTAT / OkStates art.
// Drawing only: the GameState is never written. Off: showHoleStatsScreen makes exactly the calls it made before P40.
enum UiPilotTheme : int32_t { kUiPilotOff = 0, kUiPilotGame = 1, kUiPilotSlate = 2 };  // the built-in themes' indexes
void gfx_set_ui_pilot(Ctx &ctx, int32_t theme);
int32_t gfx_ui_pilot(Ctx &ctx);

// ---------------------------------------------------------------------------------------------------------- the scene model
struct UiRgba {
  float r = 0, g = 0, b = 0, a = 0;  // channels 0..255, alpha 0..1
};
enum UiPrimKind : uint8_t { kUiRRect = 0, kUiEllipse = 1, kUiSegment = 2, kUiField = 3 };
struct UiPrim {
  uint8_t kind = kUiRRect;
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // the box (rounded rectangle, ellipse) or the segment's two ends
  float rx[4] = {0, 0, 0, 0};            // corner radii, top-left, top-right, bottom-right, bottom-left (rounded rectangle)
  float ry[4] = {0, 0, 0, 0};
  float w = 0;                           // a segment's width
  float bulge = 0;                       // a rounded rectangle's top edge raised by this much at its middle (a parabola)
  // P40B kUiField: a signed distance grid (ui_field_put) over the frame pixels [x0, x1) x [y0, y1), one value per pixel centre,
  // frame pixels, negative inside; sampled bilinearly. Made at run time from the user's art (gfx_ui.cpp), never stored.
  uint64_t field = 0;
};
struct UiElem {
  std::vector<UiPrim> prims;  // their union
  float smooth = 0;           // the union's blend radius (a fillet in the concave corners), 800-space
  UiRgba fill_top, fill_bot;  // the fill: a vertical gradient over [gy0, gy1] (800-space rows), clamped outside
  float gy0 = 0, gy1 = 1;
  UiRgba outline;             // a band of outline_w inside the edge
  float outline_w = 0;
  float bevel_w = 0;          // a band inside the edge lit from the upper left: light toward white, dark toward black
  float bevel_light = 0, bevel_dark = 0;
  uint8_t inset = 0;          // 1: sunken (the light and dark sides exchanged)
  UiRgba shadow;              // a soft shadow of the shape moved by (shadow_dx, shadow_dy), blurred over shadow_blur
  float shadow_dx = 0, shadow_dy = 0, shadow_blur = 0;
  UiRgba gloss;               // a soft spot (radial falloff) inside the shape
  float gloss_cx = 0, gloss_cy = 0, gloss_r = 0;
  // P40D: a colour grid (ui_tex_put) in place of the gradient: node (i, j) is the colour at the frame point
  // (tex_x0 + (i + 0.5) tex_step, tex_y0 + (j + 0.5) tex_step), sampled bilinearly (clamped at the grid's border). Made at run time
  // from the user's art (the game theme's report cells), never stored.
  uint64_t tex = 0;
  float tex_x0 = 0, tex_y0 = 0, tex_step = 1;
};
struct UiScene {
  float cx0 = 0, cy0 = 0, cx1 = 800, cy1 = 600;  // the clip (800-space; rows and columns of whole frame pixels)
  std::vector<UiElem> elems;                     // painted in order
};

// P40B: the distance grids of kUiField primitives, kept for the process (a scene names one by its id; the presenter of the same
// process reads it). put returns the id (a hash of the grid; the same grid gives the same id), get nullptr for an unknown id.
struct UiField {
  int32_t w = 0, h = 0;
  std::vector<float> d;
};
uint64_t ui_field_put(int32_t w, int32_t h, std::vector<float> d);
const UiField *ui_field_get(uint64_t id);
// the signed distance grid of a w x h coverage mask (1 inside): exact distances between pixel centres, minus / plus half a pixel
// at the edge, smoothed by one 3 x 3 pass; `pad` pixels of outside added on every side (the grid is (w + 2 pad) x (h + 2 pad))
std::vector<float> ui_field_from_mask(const std::vector<uint8_t> &mask, int32_t w, int32_t h, int32_t pad);

// P40D: the colour grids of UiElem::tex, kept for the process like the fields (rgb: 3 bytes per node, rows top-down)
struct UiTex {
  int32_t w = 0, h = 0;
  std::vector<uint8_t> rgb;
};
uint64_t ui_tex_put(int32_t w, int32_t h, std::vector<uint8_t> rgb);
const UiTex *ui_tex_get(uint64_t id);
// P40D: tuning numbers of the game theme's cell engine (log/roadmap/P40/P40_notes.md, P40D), set by the tests' holestats.uiTune:
// 0 the edge margin (a field's value at its own pixel centres is at most -margin inside and at least +margin outside: 0.5 = the
// scale-1 render repeats the art's pixel coverage exactly), 1 the largest colour grid step tried, 2 the field smoothing (1 = one
// 3 x 3 pass, 0 = none). ui_tune returns the previous value; an unknown index does nothing.
float ui_tune(int32_t k, float v);
float ui_tune_get(int32_t k);

// the run's face marking a UI run (gfx.h GfxTextRun::face)
constexpr const char *kUiRunFace = "\x01p40ui";
inline bool ui_is_run_face(const std::string &face) { return face == kUiRunFace; }
std::string ui_encode(const UiScene &s);
bool ui_decode(const std::string &text, UiScene &s);
// P40E: the pixels of the frame box [x0, x1) x [y0, y1) that a UI run's scene (its text) covers wholly at scale 1 (coverage 1: 1,
// else 0), row by row; the peel (gfx.h GfxTextPeelRun::keep) leaves them holding the run's words. Kept per scene and box (the panels
// redraw the same scene every frame). Null for a text that is no scene, and under negative controls gfx.family.ui 4 and 5 (the peel
// restores the world under the whole run, as before P40E).
std::shared_ptr<const std::vector<uint8_t>> ui_run_opaque(const std::string &text, int32_t x0, int32_t y0, int32_t x1, int32_t y1);

// The scene over the target pixels [px0, px0 + w) x [py0, py0 + h): target pixel (x, y) shows the frame point
// ((x + 0.5 - ox) / sx, (y + 0.5 - oy) / sy), and lies in the clip when floor((x - ox) / sx) (likewise y) does (the presenter's
// floor mapping). `prem` gets 4 floats per pixel, rows top-down: red, green, blue premultiplied (0..255) and the coverage (0..1).
void ui_render(const UiScene &s, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
               std::vector<float> &prem);
// one premultiplied pixel over an RGB555 word (8-bit channels as rgb555_to_bgra expands them, rounded back to 5 bits)
uint16_t ui_over555(const float *p, uint16_t bg);
// the same per 5-bit channel level (TextTable's per-level lookup): the result word over the grey word L | L << 5 | L << 10
inline uint16_t ui_over_level(const float *p, uint32_t level) {
  return ui_over555(p, static_cast<uint16_t>(level | (level << 5) | (level << 10)));
}
// the scene's bounding box in 800-space (elements, outlines, shadows), clipped
void ui_bounds(const UiScene &s, float &x0, float &y0, float &x1, float &y1);
// P41 (gfx_uivec.h): a UI run's text is an encoded UiScene (ui_encode) or names a uivec scene (uv_run_text, "uv:..."). These take
// either: the scene's 800-space bounds and its render (ui_render's contract). False for a text that is neither.
bool ui_run_bounds(const std::string &text, float &x0, float &y0, float &x1, float &y1);
bool ui_run_render(const std::string &text, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w,
                   int32_t h, std::vector<float> &prem);

// ---------------------------------------------------------------------------------------------------------- the themes
struct UiTheme {
  std::string name;
  UiRgba frame;                                         // the panel
  float frame_bevel = 0, frame_light = 0, frame_dark = 0;
  UiRgba shadow;                                        // the panel's drop shadow
  float shadow_d = 0, shadow_blur = 0;
  UiRgba plate, plate_edge;                             // the title plate (sunken cream)
  UiRgba label_top, label_bot, value_top, value_bot;   // the pills: labels (raised) and values (sunken)
  UiRgba pill_outline;
  float pill_outline_w = 0, pill_bevel = 0;
  float art_colours = 0;   // P40B: 1 = a report cell's regions take the art's own median colours (the game theme), 0 = the theme's
  UiRgba line;                                          // the connectors
  float line_w = 0;
  UiRgba box, box_edge;                                 // the comment box and its surround
  UiRgba row, grid;                                     // P40B: white table rows, grid lines
  UiRgba ring, ring_outline, well, ball_top, ball_bot, ball_hover_top, ball_hover_bot, gloss, check;  // the close button
};
// P40B: the themes are data (port/src/sim/ui_themes.inc, built in; the app adds ui_themes.ini over them with ui_themes_load). Index 0
// is "off"; 1 = the first theme (game), 2 = slate, then any the file adds. ui_theme(k) for an unknown k gives the first theme.
const UiTheme &ui_theme(int32_t theme);
// parse `text` (the ui_themes.inc format) over the current themes: a section replaces or adds a theme. False with `err` (line and
// reason) on a malformed line; the themes are then left as they were.
bool ui_themes_load(const std::string &text, std::string &err);
int32_t ui_theme_index(const std::string &name);   // 0 for "off" or an unknown name
std::vector<std::string> ui_theme_names();
// the Hole Stats scenes for the tests (gfx_ui.cpp): which 0 the head, 1 a row at y, 2 the foot at y, 3 the button at (x, y),
// h = its hover look
bool ui_test_scene(int32_t which, int32_t theme, int32_t x, int32_t y, int32_t h, UiScene &out);
// P40B (gfx_ui.cpp): with a theme on, the backend draws the report screens' art cells and their OK buttons procedurally.
// ui_cell_scene builds a cell's scene from the user's art (the colour cell, its mask cell; relative to the cell's top-left):
// the panel from the cell's opaque pixels as a distance field, each pill / cream box / white row as a fitted rounded
// rectangle or ellipse (a field when no fit holds) with its dark outline, the dark lines inside as a field, coloured by the
// theme; `keep` gets the pixels kept as art (pictures, coloured marks). False for an empty cell. P40E: the kept pixels are the
// scene's own picture element (their art colours, sampled edge-aware at the window's scale), so the scene's run covers every pixel
// of the cell and nothing is drawn after it: keep = 1 where the picture draws, 0 elsewhere (a soft kept pixel is the region's under
// it); 2 only under negative control gfx.family.ui 4 (blitted from the art after the run, as before P40E).
// border_only (P40C, the HUD panels: painted slots and shading): only the panel's shape is drawn; every panel pixel farther than
// 5 pixels from its edge, and every box, line and rim, is kept as art.
// P40D: a theme with art_colours (game) takes the art-fitted engine instead (gfx_ui.cpp cell_scene_art): one region per class
// component, each a field (or a rounded rectangle when its pixels are one) with the art's coverage at its pixel centres and a colour
// grid fitted to the art, so the scale-1 render repeats the art (port/test/p40_pixcmp.py measures it at 1x, 1080 and 4K).
bool ui_cell_scene(const uint8_t *colour, const uint16_t *pal, const uint8_t *mask, int32_t w, int32_t h, const UiTheme &t,
                   UiScene &out, std::vector<uint8_t> &keep, bool border_only = false);
class Gfx;
void ui_draw_scene(Gfx &g, Ctx &ctx, const UiScene &s);
// P41: a uivec scene's run text (gfx_uivec.h uv_run_text) drawn into the root and kept as a UI run; false for another text
bool ui_draw_uvec(Gfx &g, Ctx &ctx, const std::string &text);
// P41W (gfx_uvwide.h): a full-screen Surface_blit3 (args as the body's) of a wide screen's background image onto the root drawn as
// that screen's backdrop (its 1x render, Gfx::uw_screen set); any other full-screen copy clears uw_screen. False: not drawn here.
bool ui_wide_backdrop(Gfx &g, Ctx &ctx, const uint32_t *a);
// P41W: an Image_draw (args sprite, surface, x, y, mask) of one of the shown wide screen's overlay cuts drawn as that overlay (a scene
// or picture run moved by the draw point minus its place, or the backdrop's 1x pixels over the cut's rectangle). False: not one.
bool ui_wide_overlay(Gfx &g, Ctx &ctx, const uint32_t *a);
UiScene ui_button_scene(const UiTheme &t, int32_t x, int32_t y, bool hover);

}  // namespace portsim

#endif
