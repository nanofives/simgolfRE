// P10A (log/roadmap/P10/P10A_notes.md): wall-time counters of the sim-side drawing, read by the host's profiler
// (port/app/profile.h). Counters only: nothing in the simulation reads them, so the GameState, the frames and the segments are
// the same with or without a reader. Off (no clock read) until the host sets g_perf_on.
//
//   kPerfGfx           every outermost gfx_drop call (gfx.cpp): the backend's drawing, the terrain passes included
//   kPerfTerrain800    the 800 x 600 terrain passes (gfx_terrain.cpp b_pass, the rasteriser or the host's OpenGL)
//   kPerfTerrainWorld  the world layer's passes (gfx_terrain.cpp world_pass, P8B)
//   kPerfNested        host work run inside a gfx_drop (the host adds it: it is subtracted from kPerfGfx)
#ifndef PORTSIM_SIM_PERF_H
#define PORTSIM_SIM_PERF_H

#include <atomic>
#include <chrono>
#include <cstdint>

namespace portsim {

enum PerfCounter : int { kPerfGfx = 0, kPerfTerrain800 = 1, kPerfTerrainWorld = 2, kPerfNested = 3, kPerfCount = 4 };

inline std::atomic<bool> g_perf_on{false};
inline std::atomic<int64_t> g_perf_ns[kPerfCount] = {};
inline thread_local int g_perf_gfx_depth = 0;  // gfx_drop calls open on this thread

inline int64_t perf_now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline int64_t perf_read(PerfCounter c) { return g_perf_ns[c].load(std::memory_order_relaxed); }
inline void perf_add(PerfCounter c, int64_t ns) { g_perf_ns[c].fetch_add(ns, std::memory_order_relaxed); }

// times its scope into counter c when the counters are on
class PerfScope {
 public:
  explicit PerfScope(PerfCounter c) : c_(c), t0_(g_perf_on.load(std::memory_order_relaxed) ? perf_now_ns() : 0) {}
  ~PerfScope() {
    if (t0_) perf_add(c_, perf_now_ns() - t0_);
  }
  PerfScope(const PerfScope &) = delete;
  PerfScope &operator=(const PerfScope &) = delete;

 private:
  PerfCounter c_;
  int64_t t0_;
};

// gfx_drop's scope: only the outermost call on the thread is timed (a drop that runs other drops counts once)
class PerfGfxScope {
 public:
  PerfGfxScope() : t0_(g_perf_gfx_depth++ == 0 && g_perf_on.load(std::memory_order_relaxed) ? perf_now_ns() : 0) {}
  ~PerfGfxScope() {
    --g_perf_gfx_depth;
    if (t0_) perf_add(kPerfGfx, perf_now_ns() - t0_);
  }
  PerfGfxScope(const PerfGfxScope &) = delete;
  PerfGfxScope &operator=(const PerfGfxScope &) = delete;

 private:
  int64_t t0_;
};

}  // namespace portsim

#endif
