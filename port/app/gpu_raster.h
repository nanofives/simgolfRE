// P16B: the world terrain rasteriser's pixel loop on the GPU (log/roadmap/P16/P16B_notes.md). A gl::GlPixelGpu backend (gl_raster.h)
// for the world thread's GlRaster: a Direct3D 11 device of its own (made on the first draw, by the thread that draws), a compute pass
// that runs GlRaster::fast_rows_'s exact branch per pixel in triangle order (`precise double`, DDIV), the frame read back after each
// batch. Any failure hands the batch back to the CPU loops (exact) and the backend is not used again.
#ifndef PORTAPP_GPU_RASTER_H
#define PORTAPP_GPU_RASTER_H

#include <string>

namespace portsim {
namespace gl {
class GlRaster;
}
}  // namespace portsim

namespace portapp {

// the backend factory for the world rasters made (or resized) after the call (terrain_live.cpp record_reset): mode 0 none (off), 1 every
// raster (on), 2 rasters above 1920 x 1200 pixels (auto, P16B fix 2). Before the first backend the device runs the START-UP SELF-TEST
// (gpu_raster.cpp selftest): a fixed GL stream drawn by the GPU and by the CPU loops and compared word by word; on a difference, or a
// device that does not start, the factory gives no backend for the rest of the session (the CPU raster). SG_P16_SELFTEST_CORRUPT=1
// (development) flips one word of the self-test's GPU result (the forced mismatch).
void gpu_raster_enable(int mode);
// session.json p16b: the switch, the device state and the per-batch costs
std::string gpu_raster_json();
// P17B (log/roadmap/P17/P17B_notes.md; the pass unchanged): the device every backend of the process shares (made and started on the
// calling thread if not yet: the world thread), as ID3D11Device * / ID3D11DeviceContext *; false = none (why)
bool gpu_raster_device(void **dev, void **ctx, std::string &why);
// P17B: r's frame on that device (an ID3D11Texture2D *, R16_UINT, r's size) when r's backend is this file's and its copy equals r's
// frame; null otherwise
void *gpu_raster_frame(const portsim::gl::GlRaster &r);
// P17B: P17A's submit thread idle (it and the drawing thread share the context one at a time; the world thread's hr planes use it
// between passes, after this wait)
void gpu_raster_context_wait();

}  // namespace portapp

#endif  // PORTAPP_GPU_RASTER_H
