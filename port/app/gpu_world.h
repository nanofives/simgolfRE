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
#include <memory>
#include <string>
#include <vector>

namespace portsim {
struct LiveTerrainWorld;
class GfxHrGpu;
struct WorldFramePlan;
}  // namespace portsim

namespace portapp {

// P17B fix 2: the composed pictures as textures shared with the window's device (DXGI shared handles of R16_UINT textures with a keyed
// mutex: the world thread acquires key `key[k]` (0, or 1 when the window never took the last picture of that slot), writes, releases key
// 1; the window acquires key 1, copies the texture into its own on the GPU, releases key 0 and stores 0 in key[k]). k = 0 the picture
// (WorldImage::px), 1 the picture without the text runs (px_text). The slot goes back to the world thread's pool when the last reference
// to this object is dropped (the WorldImage is destroyed, reset or recycled). The words equal px / px_text, which FIX 2 still reads back.
struct GpuWorldPic {
  void *handle[2] = {nullptr, nullptr};   // null: no such picture
  int32_t w = 0, h = 0;
  int *key[2] = {nullptr, nullptr};      // the slot's next acquire key per texture (written by the window after its release)
  std::shared_ptr<void> slot;              // the slot's hold
};

// --gpu-world: 0 off, 1 on, 2 auto (planes above 1920 x 1200 pixels: P16B's / P15C's limit)
void gpu_world_enable(int mode);
// the calling thread's backend (the world thread's), made and self-tested at the first call with a world it wants (pw x ph: the world's
// size before the job; auto: above the limit); null: off, failed or not started
portsim::GfxHrGpu *gpu_world_backend(int32_t pw, int32_t ph);
// SG_P17_WORLDCHECK: one world image compared with the CPU's (differing words and flags)
void gpu_world_image_check(int64_t differing);
// world_frame_of's px from the backend's planes: the root (the raster's GPU frame when `root` is that raster's current frame, else
// uploaded), then the plan's selection; pt / px_text: the same for the picture without the text runs (null: none). false: not done (the
// backend failed: the caller has no picture from it). pic (fix 2): the same pictures as shared textures (null when no slot of the pool is
// free or sharing failed: the window uploads px / px_text as before)
// fix 2: root null = w's raster (its GPU frame when current, else its CPU words)
bool gpu_world_compose(const portsim::LiveTerrainWorld &w, const std::vector<uint16_t> *root, const portsim::WorldFramePlan &p,
                       std::vector<uint16_t> &px, const portsim::WorldFramePlan *pt, std::vector<uint16_t> *px_text,
                       std::shared_ptr<const GpuWorldPic> *pic);
// fix 2: the shared pictures on / off (default on; development switch SG_P17_SHARE=0 = off: the window uploads the words as in fix 1)
void gpu_world_share(bool on);
// world_cursor_of's inputs: hrc / hrc_cov and the box planes (when hrb_valid) read back into w's vectors (pw x ph); false: failed
bool gpu_world_read_cursor(portsim::LiveTerrainWorld &w);
// session.json p17b
std::string gpu_world_json();
// SG_P17_JOBLOG (development): the calling thread's GPU parts since the last call (ns): the start's compare, the base tiles' read, the
// composition's wait and row copy, the whole composition, the flush; reset
void gpu_world_take_parts(int64_t out[6]);
// host.cpp: one world job's parts (ns): the hr ops, the composition with its readback, the whole job
void gpu_world_job(int64_t ops_ns, int64_t image_ns, int64_t job_ns, bool gpu, bool lost);

}  // namespace portapp

#endif  // PORTAPP_GPU_WORLD_H
