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
// Each phase is timed exclusively on its thread (ProfScope: an inner scope's time is not its outer scope's). Every second the
// profiler closes a line: fps = distinct frames the window showed per second, sim_fps = game frames per second, the sim phases in
// ms per game frame, the window phases in ms per paint. The line feeds the overlay and, with --profile-csv <file>, one CSV row.
#ifndef PORTAPP_PROFILE_H
#define PORTAPP_PROFILE_H

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

namespace portapp {

enum class Phase : uint8_t { Sim, Gfx, Terrain, Capture, Wait, Base, World, Layers, Text, Bands, Overlay, Blit, Count };
constexpr int kPhases = static_cast<int>(Phase::Count);
const char *phase_name(Phase p);

struct ProfLine {
  double t_s = 0;           // seconds since the profiler started, at the line's end
  double secs = 0;          // the line's length
  uint32_t frames = 0;      // game frames (frame heads)
  uint32_t shown = 0;       // paints that showed a new frame
  uint32_t paints = 0;      // paints
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
  // window thread: a paint ended (new_frame: it showed a frame the previous paint did not), its length
  void painted(bool new_frame, int64_t ns);
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
  int64_t phase_prev_[kPhases] = {};
  // the open line
  uint32_t frames_ = 0, shown_ = 0, paints_ = 0;
  double sum_[kPhases] = {};
  double frame_sum_ = 0, frame_max_ = 0, work_sum_ = 0, paint_sum_ = 0, paint_max_ = 0;
  ProfLine last_;
  uint64_t lines_ = 0;
  // the summary
  uint64_t sum_lines_ = 0;
  double tot_fps_ = 0, tot_sim_fps_ = 0, tot_frame_ = 0, tot_paint_ = 0, tot_ms_[kPhases] = {};
  std::string face_;
  uint32_t style_ = 0;
};

// the overlay text for a line: mode 1 = the fps, mode 2 = the fps and the phases (three lines)
std::string overlay_text(const ProfLine &l, int mode);

}  // namespace portapp

#endif
