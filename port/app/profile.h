// P10A (log/roadmap/P10/P10A_notes.md): the app's frame profiler and its FPS overlay.
//
// Two threads make a picture. The sim thread runs the game's frames (LiveSession): a frame goes from one frame head to the next
// and is split into
//   sim      the simulation and everything not below (the frame code's own work)
//   gfx      the graphics backend's drawing (portsim gfx_drop, terrain passes excluded; perf.h)
//   terrain  the terrain passes: the 800 x 600 one and the world layer's (perf.h)
//   capture  the host's present: the back buffer read, the world image, the HUD layers and text runs taken for the window
//   wait     the frame wait (pacing.h frame_wait: the original's frame period, 87 ms, 125 ms with g_flags bit 17)
// The window thread paints (Presenter::paint): a paint is split into
//   base     the 800 x 600 picture scaled to the content rectangle
//   world    the world layer composed under it (P8B)
//   layers   the HUD layers composed over it (P9B / P9D / P9E)
//   text     the text runs redrawn at the window's resolution (P9C)
//   bands    the side bands (P7 / P9A)
//   overlay  this overlay
//   blit     SetDIBitsToDevice
// P14A (GPU path, gpu.cpp; appended so the earlier CSV columns keep their places):
//   upload   the frame's textures and the patches written to the GPU
//   patch    the CPU patches (text runs, tee lines, the overlay: the existing code clipped to their rectangles)
//   gpu      the GPU's own time for the paint (D3D11 timestamp queries, read one paint late; not CPU time)
//   present  waiting for the swap chain's latency object and Present
// Each phase is timed exclusively on its thread (ProfScope: an inner scope's time is not its outer scope's). Every second the
// profiler closes a line: fps = distinct frames the window showed per second (P12A fix 1, --smooth-camera on: paints whose picture
// changed per second, so the camera slide shows its own rate; the game's frames per second stay in the detail line), sim_fps = game frames per second, the sim phases in
// ms per game frame, the window phases in ms per paint. The line feeds the overlay and, with --profile-csv <file>, one CSV row.
#ifndef PORTAPP_PROFILE_H
#define PORTAPP_PROFILE_H

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

#include "portsim/sim/perf.h"  // P15A: kPdCount

namespace portapp {

enum class Phase : uint8_t { Sim, Gfx, Terrain, Capture, Wait, Base, World, Layers, Text, Bands, Overlay, Blit, Upload, Patch, Gpu, Present, Count };
constexpr int kPhases = static_cast<int>(Phase::Count);
const char *phase_name(Phase p);
// P14A: adds ns to phase p outside a ProfScope (the GPU's timestamp time), when the profiler is on
void prof_add(Phase p, int64_t ns);
// P14A: the overlay's detail line shows the GPU phases
void prof_set_gpu(bool on);

struct ProfLine {
  double t_s = 0;           // seconds since the profiler started, at the line's end
  double secs = 0;          // the line's length
  uint32_t frames = 0;      // game frames (frame heads)
  uint32_t shown = 0;       // paints that showed a new frame
  uint32_t paints = 0;      // paints
  uint32_t cursor = 0;      // P10E: paints that moved the tool preview to the cursor's tile between game frames
  uint32_t slide = 0;       // P12A fix 1: paints that showed the camera slide at a new offset between game frames
  uint32_t changed = 0;     // P12A fix 1: paints whose picture changed (a new frame, a preview move, a slide step, a hover swap)
  bool slide_line = false;  // P12A fix 1: fps counts `changed` (Profiler::set_fps_changed)
  double fps = 0, sim_fps = 0;
  double frame_ms = 0, frame_max_ms = 0;  // game frame length (head to head): mean, max
  double work_ms = 0;                     // game frame length without the wait: mean
  double paint_ms = 0, paint_max_ms = 0;  // paint: mean, max
  double ms[kPhases] = {};  // sim phases per game frame, window phases per paint
};

// times its scope into phase p on this thread, exclusively (when the profiler is on)
class ProfScope {
 public:
  explicit ProfScope(Phase p);
  ~ProfScope();
  ProfScope(const ProfScope &) = delete;
  ProfScope &operator=(const ProfScope &) = delete;

 private:
  bool on_;
};

class Profiler {
 public:
  static Profiler &get();
  // on: the clocks run (the portsim counters too); off: every call returns at once
  void set_enabled(bool on);
  bool enabled() const;
  bool open_csv(const std::string &path);  // one row per line
  void close_csv();
  // sim thread: a frame head (closes the previous game frame)
  void frame_head();
  // window thread: a paint ended (new_frame: it showed a frame the previous paint did not), its length; P10E: cursor = it showed
  // the live tool preview at a new place without a new frame
  // P12A fix 1: slide = it showed the camera slide at a new offset without a new frame; changed = its picture changed
  void painted(bool new_frame, int64_t ns, bool cursor = false, bool slide = false, bool changed = false);
  // P12A fix 1 (--smooth-camera on): fps = paints whose picture changed per second (a slide shows its real rate), the CSV and the
  // summary gain slide / changed paints; off = P10A's fps (distinct frames shown)
  void set_fps_changed(bool on) { fps_changed_ = on; }
  ProfLine last() const;
  uint64_t lines() const;
  // the overlay's font (the game's own face, taken by the host once the game created its fonts; empty until then)
  void set_font(const std::string &face, uint32_t style);
  bool font(std::string &face, uint32_t &style) const;
  // the summary of every line so far (session.json): means of fps and of each phase, over lines with frames
  std::string summary_json() const;

 private:
  Profiler();
  void roll(int64_t now);  // closes the line when a second has passed (mu_ held)
  mutable std::mutex mu_;
  bool on_ = false;
  std::FILE *csv_ = nullptr;
  int64_t t0_ = 0, line_t0_ = 0;
  int64_t head_ = 0;  // the last frame head
  int64_t perf_prev_[4] = {};
  int64_t detail_prev_[portsim::kPdCount] = {};  // P15A (frame_log)
  void frame_log(int64_t total, int64_t wait, int64_t cap, int64_t gfx_all, int64_t d800, int64_t dworld, int64_t dnest);
  int64_t phase_prev_[kPhases] = {};
  // the open line
  uint32_t frames_ = 0, shown_ = 0, paints_ = 0, cursor_ = 0;  // P10E: cursor_
  uint32_t slide_ = 0, changed_ = 0;                            // P12A fix 1
  bool fps_changed_ = false;                                    // P12A fix 1
  double sum_[kPhases] = {};
  double frame_sum_ = 0, frame_max_ = 0, work_sum_ = 0, paint_sum_ = 0, paint_max_ = 0;
  ProfLine last_;
  uint64_t lines_ = 0;
  // the summary
  uint64_t sum_lines_ = 0;
  double tot_fps_ = 0, tot_sim_fps_ = 0, tot_frame_ = 0, tot_paint_ = 0, tot_ms_[kPhases] = {};
  double tot_paints_ = 0, tot_cursor_ = 0;  // P10E: paints and cursor paints per second
  double tot_slide_ = 0, tot_shown_ = 0;    // P12A fix 1
  std::string face_;
  uint32_t style_ = 0;
};

// the overlay text for a line: mode 1 = the fps, mode 2 = the fps and the phases (three lines)
std::string overlay_text(const ProfLine &l, int mode);

// P16A (log/roadmap/P16/P16A_notes.md): development switch SG_P16_PAINTLOG=<file>, the window thread's timeline. Every event is
// kept in memory by the thread that makes it and written once at the end (winlog_close), so the log costs no file I/O while it
// runs. Kinds: V a vertical blank the pacer saw (pacer thread), T a pacer tick handled, F a game frame taken (kMsgPresent), P a
// paint (WM_PAINT: its wall time, the window thread's own cycles in it, the swap chain's latency wait and Present, the picture's
// flags, the swap chain's frame statistics after the Present). Clock: portsim::perf_now_ns (steady_clock = QueryPerformanceCounter
// in ns, the same clock as Python's time.perf_counter_ns); cycles: QueryThreadCycleTime, with the TSC rate in the header.
// Nothing in the game or the picture reads any of it.
bool winlog_on();
void winlog_open_env();
void winlog_vblank(int64_t ns);                // pacer thread: a vertical blank returned at ns (also kept as the latest)
void winlog_event(char kind, int64_t t0, uint64_t cyc0);  // window thread: an event that started at t0 / cyc0 ends now
uint64_t winlog_cycles();                      // this thread's cycles so far
// the paint's details, set by the paint itself before its P event is written
void winlog_paint_flags(bool fresh, bool changed);
void winlog_paint_gpu(int64_t wait_ns, int64_t present_ns, uint32_t last_present, uint32_t st_present, uint32_t st_refresh,
                      uint32_t st_sync_refresh, int64_t st_sync_qpc);
// the GPU path's cumulative stage times (ns: frame upload, world upload, course, final patches, draws, text, objects); the P event
// keeps this paint's share
void winlog_paint_stages(const int64_t *cum, int n);
void winlog_close();

}  // namespace portapp

#endif
