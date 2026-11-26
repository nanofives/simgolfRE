// P41W (log/roadmap/P41/PORT_WIDE_BRIEF.md, log/roadmap/P41/portwide/notes.md): the front-end screens drawn wide from the approved
// wide scenes (re/tools/uivec/scenes/wide/<screen>-wide.json, method re/tools/uivec/wide.py).
//
// A WIDE SCREEN (uivec_wide_<screen>.inc, written by re/tools/uivec/port_export.py --wide) is its STATIC BACKDROP, a list of ops in
// paint order (plain uivec items, the ground's soft blobs as a compact table, pictures lifted from the game's sheets with the
// scene's mattes), and its OVERLAYS (entries the game draws itself, state by state: the title's buttons are gfx_uivec.h's scenes,
// the title's logo is a picture). The backdrop is valid for any frame x (span entries run to x = -2000 .. 2800), so a window of
// aspect a >= 4:3 shows frame x in [400 - 300 a, 400 + 300 a] of it with the 800 x 600 composition centred.
//
// The sim side (gfx_ui.cpp ui_swap): with a UI theme on, the game's full-screen Surface_blit3 of a screen's background image draws
// the backdrop's 1x render into the root (uw_frame555, the drawing the game's later draws go over) and marks the frame
// (Gfx::uw_screen -> GfxTextFrame::backdrop); the presenter (port/app/present.cpp) then renders the backdrop at the window's
// resolution over the whole client in 8 bits per channel (uw_render_bgra, once per screen and window size, on worker threads) and
// shows the frame's own pixels only where the game drew something that is not the backdrop or a UI run.
//
// Negative controls (g_negctl_uvwide, sg_negctl_set "uvwide.render"):
//   1 the ground blobs' softness doubled (every blob's edge twice as wide): the backdrop's parity reads RED
//   2 the upsampled ground misregistered (its grid read at twice the distance from the target's left edge): the 4K crop reads RED
//     (the ground is so soft that even a quarter-scale grid upsampled stays within 2.2 levels of the exact one: measured)
//   3 a ground_diff picture's matte without its feather (the cut's rectangle shows): the picture's parity reads RED
//   4 the backdrop hook off (gfx_ui.cpp ui_swap): the background image is copied as art, the frame carries no backdrop
//     (P41G round 6: also the in-game reports' hook, gfx_ui.cpp ui_wide_report: the routing map and Buy Land as their element scenes)
//
// P41G round 6: the routing map (one screen per mode: route, route_employ, route_aura, route_value) and Buy Land (buyland) are wide
// screens too, started by the game's Image_draw of their sheet at (0, 0) (gfx_ui.cpp ui_wide_report).
#ifndef PORTSIM_SIM_GFX_UVWIDE_H
#define PORTSIM_SIM_GFX_UVWIDE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace portsim {

class Ctx;

// the ground is rendered at most at this many target pixels per frame unit and upsampled bilinearly above it. Its blobs are soft over
// >= 16 frame units: measured on the title's 4K and 2.4x crops (2026-11-25), the upsampled ground differs from the exact one by at most
// 0.063 levels (cap 2.0: 0.034; even 0.25 stays within 2.2), and the cap halves the first-show time (4K 302 -> 194 ms, 5120 x 1440
// 686 -> 237 ms)
constexpr double kUwGroundScaleMax = 1.0;

int32_t uw_screen_count();
const char *uw_screen_name(int32_t k);
int32_t uw_screen_find(const std::string &name);
// the screen whose background sheet is `sheet` (a GameFiles::normalize name, e.g. "interface/titlebase.pcx"), -1 for none
int32_t uw_screen_by_bg(const std::string &sheet);
// the picture overlay drawn on the title menu's Image_draw of cut `slot` (g_spriteRef_53b7e0), -1 for none
int32_t uw_overlay_pic_for_slot(int32_t k, int32_t slot);
// picture `pic`'s place in the frame (the scene's "at"); false for none
bool uw_pic_origin(int32_t k, int32_t pic, int32_t &x, int32_t &y);

// Sim side: lifts the screen's pictures from the game's sheets (the mattes) and renders its 1x frame. Done once per screen and
// process; false (and nothing kept) when a sheet cannot be read.
bool uw_prepare(Ctx &ctx, int32_t k);
bool uw_prepared(int32_t k);
// the static backdrop's 1x render over the 800 x 600 frame as RGB555 words (the root drawing); null before uw_prepare
std::shared_ptr<const std::vector<uint16_t>> uw_frame555(int32_t k);

// The static backdrop over target pixels [px0, px0 + w) x [py0, py0 + h): target pixel (x, y) shows the frame point
// ((x + 0.5 - ox) / sx, (y + 0.5 - oy) / sy) (sdf.render's pixel centres, wide.render's placement). rgb gets 3 floats per pixel
// (0..255, rows top-down), as wide.render before its final rounding. ground_cap: kUwGroundScaleMax (<= 0: the ground exact at any
// scale). Pictures need uw_prepare (left out before it). Runs on the machine's threads.
void uw_render(int32_t k, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
               std::vector<float> &rgb, double ground_cap = kUwGroundScaleMax);
// the same as 0xffRRGGBB words (BGRA in memory), each channel floor(clip(v + 0.5, 0, 255)) (wide.render's rounding)
void uw_render_bgra(int32_t k, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
                    std::vector<uint32_t> &out, double ground_cap = kUwGroundScaleMax);

// A picture overlay as a P40 UI run (face kUiRunFace): its text "uwp:<screen> <picture> <dx> <dy>" (the picture moved by dx, dy
// frame pixels from its scene place), the parse, its 800-space bounds and its render (gfx_ui.h ui_render's contract: 4 floats per
// pixel, premultiplied colour and coverage; the lifted picture upsampled bilinearly with premultiplied alpha, wide.composite_picture).
std::string uw_pic_run_text(int32_t k, int32_t pic, int32_t dx, int32_t dy);
bool uw_pic_parse(const std::string &text, int32_t &k, int32_t &pic, int32_t &dx, int32_t &dy);
inline bool uw_is_pic_text(const std::string &text) { return text.size() > 4 && text.compare(0, 4, "uwp:") == 0; }
// An overlay scene as a UI run: "uwo:<screen> <overlay> <dx> <dy>" (the scene moved by dx, dy from its place). uw_run_* take either
// kind of text (uwp / uwo); uw_is_run_text says whether a text is one.
std::string uw_ovl_run_text(int32_t k, int32_t ovl, int32_t dx, int32_t dy);
inline bool uw_is_run_text(const std::string &text) {
  return text.size() > 4 && (text.compare(0, 4, "uwp:") == 0 || text.compare(0, 4, "uwo:") == 0);
}
bool uw_run_bounds(const std::string &text, float &x0, float &y0, float &x1, float &y1);
bool uw_run_render(const std::string &text, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w, int32_t h,
                   std::vector<float> &prem);
// the screen's overlays keyed to the game's draws (port_export.py "draws" and the title's logo)
struct UwOverlayInfo {
  const char *name = "";
  int32_t kind = -1;      // 0 the title buttons, 1 a picture, 2 a scene, 3 the backdrop restored over the cut
  int32_t key_kind = 0;   // 1 g_spriteRef_53b7e0, 2 gfx_local_key, 3 g_sprites59df20
  uint32_t key_off = 0;
  int32_t ox = 0, oy = 0;  // the scene's point that lands on the draw point
  int32_t pic = -1;
};
int32_t uw_overlay_count(int32_t k);
bool uw_overlay_info(int32_t k, int32_t i, UwOverlayInfo &o);
bool uw_pic_bounds(const std::string &text, float &x0, float &y0, float &x1, float &y1);
bool uw_pic_render(const std::string &text, double sx, double sy, double ox, double oy, int32_t px0, int32_t py0, int32_t w,
                   int32_t h, std::vector<float> &prem);

// the duration of the last uw_render_bgra per screen, microseconds (the presenter's first-show time; tools read it)
int64_t uw_last_render_us(int32_t k);

}  // namespace portsim

#endif
