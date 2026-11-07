// P17B (log/roadmap/P17/P17B_notes.md): the world thread's job after the terrain raster on the GPU. A portsim::GfxHrGpu backend (gfx.h)
// on P16B's world-thread device (gpu_raster.h gpu_raster_device): the hr planes (hr, hr_cov, hrc, hrc_cov, hrb_lo / hrb_hi / hrb_cov)
// as GPU textures, hr_flush's dirty tiles and the post-queue draws drawn by P15C's CSObj transcription of hr_blit_rows (gpu.h
// gpu_objects_hlsl) with entry points of this file, and world_frame_of's pixels composed from a portsim::WorldFramePlan
// (terrain_live.h) and read back into the WorldImage. Before the first use a START-UP SELF-TEST (gfx.h gfx_hr_gpu_selftest) draws a
// fixed stream with and without the backend and compares every plane; a difference or a device that does not start keeps the CPU
// path for the session. SG_P17_SELFTEST_CORRUPT=1 (development) flips one word of the self-test's GPU planes (the forced mismatch).
#ifndef PORTAPP_GPU_WORLD_H
#define PORTAPP_GPU_WORLD_H

#include <cstdint>
#include <string>
#include <vector>

namespace portsim {
struct LiveTerrainWorld;
class GfxHrGpu;
struct WorldFramePlan;
}  // namespace portsim

namespace portapp {

// --gpu-world: 0 off, 1 on, 2 auto (planes above 1920 x 1200 pixels: P16B's / P15C's limit)
void gpu_world_enable(int mode);
// the calling thread's backend (the world thread's), made and self-tested at the first call with a world it wants (pw x ph: the world's
// size before the job; auto: above the limit); null: off, failed or not started
portsim::GfxHrGpu *gpu_world_backend(int32_t pw, int32_t ph);
// SG_P17_WORLDCHECK: one world image compared with the CPU's (differing words and flags)
void gpu_world_image_check(int64_t differing);
// world_frame_of's px from the backend's planes: the root (the raster's GPU frame when `root` is that raster's current frame, else
// uploaded), then the plan's selection; pt / px_text: the same for the picture without the text runs (null: none). false: not done (the
// backend failed: the caller has no picture from it)
bool gpu_world_compose(const portsim::LiveTerrainWorld &w, const std::vector<uint16_t> &root, const portsim::WorldFramePlan &p,
                       std::vector<uint16_t> &px, const portsim::WorldFramePlan *pt, std::vector<uint16_t> *px_text);
// world_cursor_of's inputs: hrc / hrc_cov and the box planes (when hrb_valid) read back into w's vectors (pw x ph); false: failed
bool gpu_world_read_cursor(portsim::LiveTerrainWorld &w);
// session.json p17b
std::string gpu_world_json();
// host.cpp: one world job's parts (ns): the hr ops, the composition with its readback, the whole job
void gpu_world_job(int64_t ops_ns, int64_t image_ns, int64_t job_ns, bool gpu, bool lost);

}  // namespace portapp

#endif  // PORTAPP_GPU_WORLD_H
