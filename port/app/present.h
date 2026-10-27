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

class Presenter {
 public:
  explicit Presenter(PresentDefect d = PresentDefect::None) : defect_(d) {}
  // A new frame: 800 * 600 RGB555 words, rows top-down. Returns false when the size is wrong.
  bool set_frame(const std::vector<uint16_t> &px);
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
  std::vector<uint16_t> frame_;
  uint32_t frames_set_ = 0;
  mutable std::vector<uint32_t> scratch_;
};

}  // namespace portapp

#endif
