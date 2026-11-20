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
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

namespace portsim {

enum PerfCounter : int { kPerfGfx = 0, kPerfTerrain800 = 1, kPerfTerrainWorld = 2, kPerfNested = 3, kPerfCount = 4 };

inline std::atomic<bool> g_perf_on{false};
inline std::atomic<int64_t> g_p31_world_ns{0};  // P31B DEV: perf_now_ns at the last renderSpriteQueue end (item 7 measurement)
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

// P38G development: SG_P38G_FUNCPROF=<file> times every callee of the tick (tick.cpp TkBase::call) (inclusive wall time and count, by name) and the
// mainLoop phases (DevProfScope), and appends a snapshot of every row ("T <steady ns>" then "name count ns") about once a second.
// Off unless set (one getenv at the first call); nothing in the simulation reads it.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)  // getenv, fopen
#endif
namespace devprof {
struct Row {
  int64_t n = 0, ns = 0;
};
inline bool on() {
  static const bool o = std::getenv("SG_P38G_FUNCPROF") != nullptr;
  return o;
}
inline std::mutex &mu() {
  static std::mutex m;
  return m;
}
inline std::map<std::string, Row> &rows() {
  static std::map<std::string, Row> r;
  return r;
}
// per thread, keyed by the name literal's address; merged into rows() by name and written out about once a second
inline thread_local std::unordered_map<const char *, Row> t_rows;
inline thread_local int64_t t_last = 0;
inline void add(const char *name, int64_t ns, int64_t now) {
  Row &r = t_rows[name];
  ++r.n;
  r.ns += ns;
  if (now - t_last <= 1000000000LL) return;
  t_last = now;
  std::lock_guard<std::mutex> g(mu());
  for (const auto &kv : t_rows) {
    Row &d = rows()[kv.first];
    d.n += kv.second.n;
    d.ns += kv.second.ns;
  }
  t_rows.clear();
  if (FILE *f = std::fopen(std::getenv("SG_P38G_FUNCPROF"), "a")) {
    std::fprintf(f, "T %lld\n", static_cast<long long>(now));
    for (const auto &kv : rows()) std::fprintf(f, "%s %lld %lld\n", kv.first.c_str(), static_cast<long long>(kv.second.n), static_cast<long long>(kv.second.ns));
    std::fclose(f);
  }
}
}  // namespace devprof
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
// P38G (log/roadmap/P38/P38G_notes.md): the P38G speed fixes off, the before arm of a measurement or of an equality test in one
// build. Development only: SG_P38G_OFF=1 at start, or sg_p38g_off(1) (portsim_c.h) at run time; every fix gives the same results
// either way (port/test/test_p38g_speed.py compares them)
inline std::atomic<int> g_p38g_off{-1};  // -1 = from SG_P38G_OFF
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)  // getenv
#endif
inline bool p38g_off() {
  const int v = g_p38g_off.load(std::memory_order_relaxed);
  if (v >= 0) return v != 0;
  static const bool o = [] {
    const char *e = std::getenv("SG_P38G_OFF");
    return e != nullptr && e[0] == '1';
  }();
  return o;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
struct DevProfScope {
  const char *n;
  int64_t t0;
  explicit DevProfScope(const char *name) : n(name), t0(devprof::on() ? perf_now_ns() : 0) {}
  ~DevProfScope() {
    if (t0) {
      const int64_t now = perf_now_ns();
      devprof::add(n, now - t0, now);
    }
  }
};

// P15A (log/roadmap/P15/P15A_notes.md): detail counters of the client-size work on the sim thread, read only by the host's frame
// log (SG_P15_FRAMELOG, port/app/profile.cpp). Inclusive wall time of each scope; the scopes named here do not nest in each other.
enum PerfDetail : int {
  kPdWorldRender = 0,  // world_pass: the world rasteriser's pass (gfx_terrain.cpp), the strip pass's clear included
  kPdWorldT,           // b_pass / world_pass: the 800 root's copy before the pass and T's update after it
  kPdWorldCopy,        // the world surfaces' copies: blit 1 / 0 (back <-> root), windowPaint's fill, frameEnd's redraw after a resize
  kPdQueueBegin,       // renderSpriteQueue's start: the wide scene's base and hr_queue_begin
  kPdHrFlush,          // hr_flush without the publication
  kPdHrPub,            // hr_flush's publication (P12A fix 2)
  kPdHrEnqueue,        // hr_blit: the queue's commands made (their signatures)
  kPdHrNow,            // hr_now / hr_box_now: the draws after the queue's end
  kPdLines,            // the overlay lines mirrored on the world layer (terrain_overlay.cpp)
  kPdQueueEnd,         // renderSpriteQueue's end without hr_flush (s1, the text-clean scene)
  kPdCapDump,          // host present: gfx.dump and the back buffer read
  kPdCapText,          // host present: the text frame
  kPdCapCursor,        // host present: world_cursor and the preview box
  kPdCapWorld,         // host present: world_frame
  kPdCapWorldText,     // host present: world_frame for the text runs
  kPdCapHrScene,       // host present: the hr scene handed over (gfx_hr_scene)
  kPdCount
};
inline std::atomic<int64_t> g_perf_detail_ns[kPdCount] = {};
inline int64_t perf_detail_read(PerfDetail c) { return g_perf_detail_ns[c].load(std::memory_order_relaxed); }
class PerfDetailScope {
 public:
  explicit PerfDetailScope(PerfDetail c) : c_(c), t0_(g_perf_on.load(std::memory_order_relaxed) ? perf_now_ns() : 0) {}
  ~PerfDetailScope() { stop(); }
  void stop() {
    if (t0_) g_perf_detail_ns[c_].fetch_add(perf_now_ns() - t0_, std::memory_order_relaxed);
    t0_ = 0;
  }
  PerfDetailScope(const PerfDetailScope &) = delete;
  PerfDetailScope &operator=(const PerfDetailScope &) = delete;

 private:
  PerfDetail c_;
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
