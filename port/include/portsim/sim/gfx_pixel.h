// Body family `pixel` of the software graphics backend (roadmap S3.6; port/src/sim/gfx_pixel.cpp, listed in
// port/src/sim/gfx_families.inc): Widget_writePixel, the single-pixel store of mainLoop's type-0x11 tile decoration, and
// Widget_fillRect (jgld line16). The census rows after an in-game frame (cov_r_quit's credits among them) reported
// Widget_writePixel because a run's unsupported list is cumulative; the credits screen itself does not call it. Writes only
// the backend's own surfaces.
#ifndef PORTSIM_SIM_GFX_PIXEL_H
#define PORTSIM_SIM_GFX_PIXEL_H

#include <cstdint>

namespace portsim {

struct Surface16;

namespace gfxpx {

// Widget_writePixel's store on a 16-bit surface: the low word of colour at (x, y); false (nothing written) outside it
bool write_pixel(Surface16 &d, int32_t x, int32_t y, uint32_t colour);

}  // namespace gfxpx
}  // namespace portsim

#endif
