// P14A (log/roadmap/P14/P14A_notes.md): the window's picture composed and presented by the GPU.
//
// Direct3D 11 on the window thread, a DXGI flip-model swap chain (FLIP_DISCARD, 2 buffers, BGRA8, the frame-latency waitable object
// at latency 1, Present(1, 0)). Per paint the GPU draws what Presenter::render draws at the same state:
//   course pass  the base (nearest or render's 8-bit bilinear taps) and the world layer (compose_world's pos / mask / taps) at the
//                camera slide's viewport, the previous course picture under the uncovered strip (fill_gap, clamped addressing),
//                into a client-size R32_UINT target holding packed BGRA words (the CPU path's own words);
//   course patches  the course text runs' rectangles, rendered by the CPU (Presenter::render_rect, course only, offset 0), drawn at
//                the slide's offset;
//   HUD pass     compose_layers' per-pixel loop (layers in draw order, each pass at its client offset; the taps and their
//                exclusions; over() kinds 1-4 with the shade tables as data; the whole-window dim; the tool previews from world
//                pixels), from the course target into the back buffer;
//   final patches  the rectangles of the HUD text runs, the tee lines and the FPS overlay, rendered by the CPU (render_rect then the
//                overlay), drawn over it.
//   P15B         the HUD text runs that do not move with an object are no final patches: each run's GDI result per pixel as a
//                function of the word under it (textres.h TextTable, 32 GDI draws per run, cached while the run is unchanged) is
//                applied by the HUD pass right after the run's layer, as compose_layers' draw_runs does; the FPS overlay (its box
//                darkened to half, then its runs' tables) at the end of the HUD pass (SG_APP_P15=off: P14A's patches).
// Every pixel rule on the GPU is integer arithmetic on the stored words, transcribed from present.cpp; GDI text and the lines'
// double arithmetic stay on the CPU (patches hold the final pixels of their rectangles). Frames the GPU path does not take (screens
// without the layered course view, the negative controls) are rendered by Presenter::render and uploaded whole.
#ifndef PORTAPP_GPU_H
#define PORTAPP_GPU_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace portapp {

class Presenter;
struct Viewport;

// P17B (log/roadmap/P17/P17B_notes.md): P15C's objects pass HLSL (hr_blit_rows' per-pixel work and the five ops' bodies) as text, for
// the world thread's hr planes (gpu_world.cpp): one transcription for both
const char *gpu_objects_hlsl(size_t &n);

class GpuCompositor {
 public:
  GpuCompositor();
  ~GpuCompositor();
  GpuCompositor(const GpuCompositor &) = delete;
  GpuCompositor &operator=(const GpuCompositor &) = delete;
  // the device, the swap chain on `hwnd` and the shaders; false = not started (`why`)
  bool start(void *hwnd, std::string &why);
  bool ok() const;
  void stop();
  // one paint of `p`'s current state (slx, sly: the slide offset render would draw the course picture at)
  void paint(const Presenter &p, int32_t cw, int32_t ch, int32_t slx, int32_t sly);
  std::string json() const;
  // the next presented picture written to `path` (a 32-bit BMP of the back buffer, read back before Present)
  void request_shot(const std::string &path);
  // the next vertical blank of the swap chain's monitor (any thread); false = none
  bool wait_vblank();
  // P15C: the moving objects' compute pass is available (the presenter's GPU objects path; false = its CPU redraw)
  bool objects_ok() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> d_;
  bool ensure_size(int32_t cw, int32_t ch);
  void lose(const char *where, long hr);
  void upload_frame(const Presenter &p, const Viewport &vp, bool text);
  void upload_world(const Presenter &p, bool text);
  bool world_from_shared(const struct WorldImage &w, bool text);  // P17B fix 2
  void run_objects(const Presenter &p);  // P15C
  void build_course_patches(const Presenter &p, const Viewport &vp, bool text);
  void build_final_patches(const Presenter &p, const Viewport &vp, bool text, int32_t slx, int32_t sly, int32_t cdx, int32_t shx,
                           int32_t shy, bool moving_tools);
  void build_text_tables(const Presenter &p, const Viewport &vp, bool text);  // P15B
  void build_overlay_tables(const Presenter &p, int32_t cw, int32_t ch);      // P15B fix 2
  void upload_text_tables(int32_t cw, int32_t ch);                            // P15B
  void paint_layered(const Presenter &p, int32_t cw, int32_t ch, int32_t slx, int32_t sly);
  void paint_full(const Presenter &p, int32_t cw, int32_t ch);
  void paint_flat(const Presenter &p, int32_t cw, int32_t ch);
  void verify(const Presenter &p, int32_t cw, int32_t ch, bool layered);
  void fetch_prev(const Presenter &p);
  void shot_if_asked(int32_t cw, int32_t ch);
  void upload_tables(const Viewport &vp, bool near_, bool wnear, int32_t wox, int32_t wfw, int32_t wpw, int32_t wph);
  void ts_begin();
  void ts_end();
};

}  // namespace portapp

#endif
