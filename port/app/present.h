// Presenting the port's RGB555 back buffer in the host window (roadmap S7.3).
//
// The port's graphics backend (portsim/sim/gfx.h) keeps jgld's back buffer: 800x600 RGB555 words, rows top-down. The host
// scales it to its client area with the mapping the input translator inverts (input_map.h Viewport, Translator::to_logical):
// the 4:3 content rectangle is the largest that fits the client, centred (black bars where the aspect differs), and client
// pixel (x, y) inside it shows logical pixel (floor((x - ox) * 800 / w), floor((y - oy) * 600 / h)). A click on a client pixel
// therefore reaches the logical pixel that pixel shows.
//
// Colour: each 5-bit channel v becomes the 8-bit (v << 3) | (v >> 2), the conversion the pinned reference frames use
// (re/frida/scenario_ab.py checkpoint_frame, re/tools/imgcmp.py rgb555_to_rgb), so a presented pixel equals the original's
// checkpoint pixel when the back buffers are equal. The scaled image is a 32-bit top-down DIB drawn 1:1
// (SetDIBitsToDevice): no GDI stretching decides a pixel.
#ifndef PORTAPP_PRESENT_H
#define PORTAPP_PRESENT_H

#include <cstdint>
#include <mutex>
#include <vector>

#include "input_map.h"

namespace portapp {

// Deliberately wrong bodies for the negative controls (`--negctl present-*`; production code uses None).
enum class PresentDefect : uint8_t {
  None = 0,
  Stale = 1,      // the first frame set stays on screen; later frames are ignored
  ByteSwap = 2,   // each RGB555 word's two bytes exchanged before conversion
  WorldMaskIgnored = 3,  // P8B: the world layer shows over the whole content rectangle (the frame's mask not read)
  HudUnmoved = 4,        // G32: the HUD compositor off (panels stay centred, no remap)
};

// The content rectangle for a client area of cw x ch.
Viewport fit_viewport(int32_t cw, int32_t ch);

// 8-bit channels of an RGB555 word
inline uint32_t rgb555_to_bgra(uint16_t v) {
  const uint32_t r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
  const uint32_t r8 = (r << 3) | (r >> 2), g8 = (g << 3) | (g >> 2), b8 = (b << 3) | (b >> 2);
  return 0xff000000u | (r8 << 16) | (g8 << 8) | b8;
}

// P5B item 4: how the 800x600 frame is scaled to the content rectangle. Nearest = S7.3's mapping (every client pixel shows
// exactly one logical pixel; the reference the app tests compare with). Smooth = bilinear over the 8-bit channels: client pixel
// centre (x + 0.5) * 800 / w - 0.5 (clamped to the frame) interpolated between its four neighbours, 8-bit weights. Auto =
// Nearest when the content rectangle is an integer multiple of 800x600 (1x, 2x, ...: sharp pixels, no blur), else Smooth.
enum class ScaleMode : uint8_t { Nearest = 0, Smooth = 1, Auto = 2 };

// P7 (widescreen, stage 1; log/roadmap/P7/PLAN.md). The game still draws 800x600; on a client wider than 4:3 the content
// rectangle is unchanged (fit_viewport: full height, centred, so the input mapping is unchanged) and the side bands, black
// before P7, can show the frame's own edge colours carried outward. BandFill is the host's per-frame hint (LiveSession: Extend
// for the full-screen 2D screens before a game's frame loop runs, Black once it runs); WideFill is the app option
// (`--wide-fill`): Off = black bands always (the pre-P7 image, byte for byte), Soft = each band column takes the frame's edge
// colour (the mean of its outer 4 logical columns, per row) blurred vertically by a radius that grows with the distance from
// the frame (d / 2 client pixels, at most 48 logical rows), Clamp = the frame's outermost column repeated, no blur.
// Rows of a letterbox (a client taller than 4:3) stay black.
enum class BandFill : uint8_t { Black = 0, Extend = 1 };
enum class WideFill : uint8_t { Off = 0, Soft = 1, Clamp = 2 };

// P8B (widescreen stage 2, log/roadmap/P8/P8B_notes.md): the world layer of a frame (portsim::LiveTerrain::world_frame). px holds
// pw x ph RGB555 words (rows top-down); the 800 x 600 frame covers world columns ox .. ox + fw - 1 (fw = ph * 800 / 600) and every
// row; mask (800 x 600) is 1 where the frame's pixel shows the world. Empty px = no world (the frame is shown as before P8B).
// G32 (widescreen stage 3, the HUD compositor of log/roadmap/P7/PLAN.md section 3; log/roadmap/P8/P8B_notes.md section 7): the
// game keeps drawing its HUD into the 800 x 600 frame and testing clicks in 800-space. Each panel rectangle below (800-space,
// half-open) has an anchor; in a client wider than the 4:3 content rectangle the frame's 2D pixels inside it (WorldImage mask 0)
// are moved by the anchor's distance to the window's edge (left: -origin_x client pixels, right: + the right band's width), the
// pixels they leave show the world image, and PanelRemap maps a point over a moved panel pixel back to the 800-space point. The
// panels keep their size relative to the window (everything is scaled by window height / 600).
struct HudPanel {
  int32_t x0, y0, x1, y1;
  int8_t anchor;  // -1 left edge, +1 right edge
  const char *name;
};
constexpr HudPanel kHudPanels[] = {
    {0, 0, 250, 95, -1, "title: logo, course name and date (top left)"},
    {630, 0, 800, 145, 1, "meters: money, happiness, rating (top right)"},
    {0, 425, 800, 600, -1, "bottom band: radial menu, System button, the palette / golfer / employee / building panels"},
};

// G32: the client-to-client point remap of the moved panels, shared by the window thread (Presenter::set_frame_world updates it;
// AppWindow remaps mouse messages) and the sim thread (LiveSession's cursor answers)
class PanelRemap {
 public:
  void set(bool on, const Viewport &vp, int32_t cw, const std::vector<uint8_t> &mask);
  void clear();
  // the client point the game sees for client point p: p - dx when p lies over a moved panel's 2D pixel, else p
  void map(int32_t &x, int32_t &y) const;
  uint64_t remapped() const;

 private:
  mutable std::mutex mu_;
  bool on_ = false;
  Viewport vp_;
  int32_t cw_ = 0;
  std::vector<uint8_t> mask_;
  mutable uint64_t remapped_ = 0;
};

struct WorldImage {
  std::vector<uint16_t> px;
  std::vector<uint8_t> mask;
  int32_t pw = 0, ph = 0, fw = 0, ox = 0;
  bool empty() const { return px.empty(); }
};

class Presenter {
 public:
  explicit Presenter(PresentDefect d = PresentDefect::None, ScaleMode s = ScaleMode::Nearest) : defect_(d), scale_(s) {}
  void set_scale(ScaleMode s) { scale_ = s; }
  ScaleMode scale() const { return scale_; }
  // the mode a content rectangle of w x h is drawn with (Auto resolved)
  ScaleMode effective_scale(int32_t w, int32_t h) const;
  // A new frame: 800 * 600 RGB555 words, rows top-down. Returns false when the size is wrong. `band`: P7's hint for the side
  // bands (used only when set_wide is not Off).
  bool set_frame(const std::vector<uint16_t> &px, BandFill band = BandFill::Black);
  // P8B: the same with the frame's world layer (moved in; an empty one = none). With a world layer the client image is the world
  // wherever the frame does not cover it or its mask says the world shows, and the frame's own pixel elsewhere (section above)
  bool set_frame_world(const std::vector<uint16_t> &px, BandFill band, WorldImage &&world);
  const WorldImage &world() const { return world_; }
  // G32: the HUD compositor (kHudPanels; off = P8B stage 2: the frame's 2D pixels stay centred) and the remap it keeps current
  void set_hud_anchor(bool on) { hud_anchor_ = on; }
  void set_remap(PanelRemap *r) { remap_ = r; }
  // P7: the side-band fill (Off = black bands, the pre-P7 behaviour)
  void set_wide(WideFill w) { wide_ = w; }
  WideFill wide() const { return wide_; }
  BandFill band() const { return band_; }
  bool has_frame() const { return !frame_.empty(); }
  const std::vector<uint16_t> &frame() const { return frame_; }
  uint32_t frames_set() const { return frames_set_; }
  // The client image for cw x ch: BGRA words, rows top-down (vp receives the content rectangle).
  void render(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vp = nullptr) const;

#ifdef _WIN32
  // Draws the client image into `hdc` (a window DC or a paint DC) at (0, 0).
  void paint(void *hdc, int32_t cw, int32_t ch) const;
#endif

 private:
  PresentDefect defect_;
  ScaleMode scale_;
  std::vector<uint16_t> frame_;
  uint32_t frames_set_ = 0;
  WideFill wide_ = WideFill::Off;  // P7
  BandFill band_ = BandFill::Black;
  void fill_bands(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const;  // P7
  WorldImage world_;                                                                                  // P8B
  bool hud_anchor_ = false;                                                                           // G32
  PanelRemap *remap_ = nullptr;                                                                       // G32
  void compose_world(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const;  // P8B
  mutable std::vector<uint32_t> scratch_;
};

}  // namespace portapp

#endif
