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
#include <string>
#include <vector>

namespace portsim {

class Ctx;

// ---------------------------------------------------------------------------------------------------------- the option
// --ui-pilot off|game|slate (app main.cpp; default off): the Hole Stats screen's frame, title plate, label pills, value boxes,
// connectors, score ovals, comment box and close button drawn by the `ui` family instead of the HoleSTAT / OkStates art.
// Drawing only: the GameState is never written. Off: showHoleStatsScreen makes exactly the calls it made before P40.
enum UiPilotTheme : int32_t { kUiPilotOff = 0, kUiPilotGame = 1, kUiPilotSlate = 2 };
void gfx_set_ui_pilot(Ctx &ctx, int32_t theme);
int32_t gfx_ui_pilot(Ctx &ctx);

// ---------------------------------------------------------------------------------------------------------- the scene model
struct UiRgba {
  float r = 0, g = 0, b = 0, a = 0;  // channels 0..255, alpha 0..1
};
enum UiPrimKind : uint8_t { kUiRRect = 0, kUiEllipse = 1, kUiSegment = 2 };
struct UiPrim {
  uint8_t kind = kUiRRect;
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // the box (rounded rectangle, ellipse) or the segment's two ends
  float rx[4] = {0, 0, 0, 0};            // corner radii, top-left, top-right, bottom-right, bottom-left (rounded rectangle)
  float ry[4] = {0, 0, 0, 0};
  float w = 0;                           // a segment's width
  float bulge = 0;                       // a rounded rectangle's top edge raised by this much at its middle (a parabola)
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
};
struct UiScene {
  float cx0 = 0, cy0 = 0, cx1 = 800, cy1 = 600;  // the clip (800-space; rows and columns of whole frame pixels)
  std::vector<UiElem> elems;                     // painted in order
};

// the run's face marking a UI run (gfx.h GfxTextRun::face)
constexpr const char *kUiRunFace = "\x01p40ui";
inline bool ui_is_run_face(const std::string &face) { return face == kUiRunFace; }
std::string ui_encode(const UiScene &s);
bool ui_decode(const std::string &text, UiScene &s);

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

// ---------------------------------------------------------------------------------------------------------- the themes
struct UiTheme {
  UiRgba frame, frame_rim, frame_shade, frame_outline;  // the panel
  float frame_bevel = 0, frame_light = 0, frame_dark = 0;
  UiRgba shadow;                                        // the panel's drop shadow
  float shadow_d = 0, shadow_blur = 0;
  UiRgba plate, plate_edge, plate_inner;                // the title plate (sunken cream)
  UiRgba label_top, label_bot, value_top, value_bot;   // the pills: labels (raised) and values (sunken)
  UiRgba pill_outline;
  float pill_outline_w = 0, pill_bevel = 0;
  UiRgba line;                                          // the connectors
  float line_w = 0;
  UiRgba box, box_edge;                                 // the comment box
  UiRgba well, ball_top, ball_bot, ball_hover_top, ball_hover_bot, gloss, check;  // the close button
};
const UiTheme &ui_theme(int32_t theme);
// the Hole Stats scenes for the tests (gfx_ui.cpp): which 0 the head, 1 a row at y, 2 the foot at y, 3 the button at (x, y),
// h = its hover look
bool ui_test_scene(int32_t which, int32_t theme, int32_t x, int32_t y, int32_t h, UiScene &out);

}  // namespace portsim

#endif
