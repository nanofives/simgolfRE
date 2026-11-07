// P16B: the world terrain rasteriser's pixel loop on the GPU (log/roadmap/P16/P16B_notes.md). A gl::GlPixelGpu backend (gl_raster.h)
// for the world thread's GlRaster: a Direct3D 11 device of its own (made on the first draw, by the thread that draws), a compute pass
// that runs GlRaster::fast_rows_'s exact branch per pixel in triangle order (`precise double`, DDIV), the frame read back after each
// batch. Any failure hands the batch back to the CPU loops (exact) and the backend is not used again.
#ifndef PORTAPP_GPU_RASTER_H
#define PORTAPP_GPU_RASTER_H

#include <string>

namespace portapp {

// registers (on) or removes (off) the backend factory for the world rasters made after the call (terrain_live.cpp record_reset)
void gpu_raster_enable(bool on);
// session.json p16b: the switch, the device state and the per-batch costs
std::string gpu_raster_json();

}  // namespace portapp

#endif  // PORTAPP_GPU_RASTER_H
