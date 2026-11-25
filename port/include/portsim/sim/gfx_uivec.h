// P41 port pilot (log/roadmap/P41/PORT_PILOT_BRIEF.md, log/roadmap/P41/pilot/notes.md): the approved uivec scenes drawn by the port.
//
// A UIVEC SCENE is the renderer re/tools/uivec/sdf.py's scene (its docstring is the format): items painted in order, each a box in
// 800 x 600 frame coordinates and a list of layers (a shape's signed distance, a paint, the coverage from the distance with a soft
// edge, a clip shape and a radial falloff, an alpha). re/tools/uivec/port_export.py writes the approved title menu scenes into
// port/src/sim/uivec_title.inc with every construction sdf.py builds resolved into plain shapes, and gfx_uivec.cpp evaluates them
// with sdf.py's formulas (doubles), so a render equals sdf.render of the same scene at any scale (port/test/test_p41_uivec.py).
//
// The title menu (gfx_ui.cpp ui_swap): with a UI theme on, Image_draw of a title button's cut (showTitleScreen's SpriteRefs, slot
// 2 b + hovered) draws the scene of that button and state instead, moved by the draw's point minus the cut's origin. The drawing is
// kept as a P40 UI run (face kUiRunFace) whose text names the scene (uv_run_text), so the presenter renders it again at the
// window's resolution (gfx_ui.h ui_run_render, port/app/textres.cpp).
#ifndef PORTSIM_SIM_GFX_UIVEC_H
#define PORTSIM_SIM_GFX_UIVEC_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace portsim {

// the scenes built in (uivec_title.inc): count, name ("title.<button>"), the kTitleButtons index and the state of scene k
int32_t uv_scene_count();
const char *uv_scene_name(int32_t k);
int32_t uv_scene_button(int32_t k);
int32_t uv_scene_hover(int32_t k);
// the scene of title button `button` (gfx.cpp kTitleButtons index) in its idle (0) or hover (1) state, -1 when none
int32_t uv_title_scene(int32_t button, int32_t hover);
// the scene drawn for showTitleScreen's title cut `slot` (2 b + hovered of g_spriteRef_53b7e0), with the cut's origin in the
// title pictures (cut_x, cut_y); -1 for a slot without one (the centre picture, slots 12 / 13, stays art)
int32_t uv_title_slot_scene(int32_t slot, int32_t &cut_x, int32_t &cut_y);

// a UI run's text naming scene k moved by (dx, dy) frame pixels, and its parse (false for any other text)
std::string uv_run_text(int32_t k, int32_t dx, int32_t dy);
bool uv_parse(const std::string &text, int32_t &k, int32_t &dx, int32_t &dy);
inline bool uv_is_run_text(const std::string &text) { return text.size() > 3 && text.compare(0, 3, "uv:") == 0; }

// the union of scene k's item boxes moved by (dx, dy) (800-space)
void uv_bounds(int32_t k, int32_t dx, int32_t dy, float &x0, float &y0, float &x1, float &y1);
// scene k moved by (dx, dy) over the target pixels [px0, px0 + w) x [py0, py0 + h): target pixel (x, y) shows the frame point
// ((x + 0.5 - ox) / sx, (y + 0.5 - oy) / sy) (gfx_ui.h ui_render's contract, sdf.render's pixel centres). An item paints the target
// pixels of its box (sdf.render: columns floor(x0 sx + ox) .. ceil(x1 sx + ox)). `prem` gets 4 floats per pixel, rows top-down:
// red, green, blue premultiplied (0..255) and the coverage (0..1); the layers composited over it in order equal sdf.render's
// img * (1 - a) + colour * a over any background.
void uv_render(int32_t k, int32_t dx, int32_t dy, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w,
               int32_t h, std::vector<float> &prem);
// the scale-1 render of a run text over its whole-pixel bounds [x0, x1) x [y0, y1), kept per text (the title menu redraws its six
// buttons every pass); null for a text that is no uivec scene
struct UvFrame {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  std::vector<float> prem;
};
std::shared_ptr<const UvFrame> uv_frame(const std::string &text);

}  // namespace portsim

#endif
