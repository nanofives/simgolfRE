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
#include <vector>

#include "input_map.h"

namespace portapp {

// Deliberately wrong bodies for the negative controls (`--negctl present-*`; production code uses None).
enum class PresentDefect : uint8_t {
  None = 0,
  Stale = 1,      // the first frame set stays on screen; later frames are ignored
  ByteSwap = 2,   // each RGB555 word's two bytes exchanged before conversion
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
  // P7: the side-band fill (Off = black bands, the pre-P7 behaviour)
  void set_wide(WideFill w) { wide_ = w; }
  WideFill wide() const { return wide_; }
  BandFill band() const { return band_; }
  bool has_frame() const { return !frame_.empty(); }
  const std::vector<uint16_t> &frame() const { return frame_; }
  uint32_t frames_set() const { return frames_set_; }
  // P6B item 2: the `Loading` box drawn over the frame while a world load runs (done = pumps so far, 0 = off; expected = the
  // pumps the bar is scaled to). Drawn into the window DC only (paint): frame(), render() and every frame hash are unchanged
  void set_loading(uint32_t done, uint32_t expected) {
    loading_done_ = done;
    loading_expected_ = expected;
  }
  bool loading() const { return loading_done_ != 0; }

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
  uint32_t loading_done_ = 0, loading_expected_ = 0;  // P6B item 2
  mutable std::vector<uint32_t> scratch_;
};

}  // namespace portapp

#endif
