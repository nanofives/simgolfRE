// P10A: the app's frame profiler (profile.h).
#include "profile.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#endif

#include "portsim/sim/perf.h"

namespace portapp {

namespace {
std::atomic<bool> g_on{false};
std::atomic<int64_t> g_phase_ns[kPhases] = {};

// the exclusive timing stack of one thread
struct Stack {
  struct Entry {
    Phase p;
    int64_t t0;     // the scope's start (inclusive time for kPerfNested)
    bool nested;    // the outermost scope of this thread opened inside a gfx_drop
  };
  Entry e[16];
  int n = 0;
  int64_t mark = 0;
};
thread_local Stack t_stack;

const char *const kNames[kPhases] = {"sim", "gfx", "terrain", "capture", "wait", "base", "world", "layers", "text", "bands",
                                     "overlay", "blit", "upload", "patch", "gpu", "present"};  // P14A: the last four
std::atomic<bool> g_gpu_line{false};  // P14A
}  // namespace

const char *phase_name(Phase p) { return static_cast<int>(p) < kPhases ? kNames[static_cast<int>(p)] : "?"; }

void prof_add(Phase p, int64_t ns) {  // P14A
  if (g_on.load(std::memory_order_relaxed)) g_phase_ns[static_cast<int>(p)].fetch_add(ns, std::memory_order_relaxed);
}
void prof_set_gpu(bool on) { g_gpu_line.store(on); }  // P14A

ProfScope::ProfScope(Phase p) : on_(g_on.load(std::memory_order_relaxed)) {
  if (!on_) return;
  Stack &s = t_stack;
  const int64_t now = portsim::perf_now_ns();
  if (s.n > 0) g_phase_ns[static_cast<int>(s.e[s.n - 1].p)].fetch_add(now - s.mark, std::memory_order_relaxed);
  if (s.n < 16) s.e[s.n] = {p, now, s.n == 0 && portsim::g_perf_gfx_depth > 0};
  ++s.n;
  s.mark = now;
}

ProfScope::~ProfScope() {
  if (!on_) return;
  Stack &s = t_stack;
  const int64_t now = portsim::perf_now_ns();
  if (s.n > 0 && s.n <= 16) {
    const Stack::Entry &e = s.e[s.n - 1];
    g_phase_ns[static_cast<int>(e.p)].fetch_add(now - s.mark, std::memory_order_relaxed);
    if (e.nested) portsim::perf_add(portsim::kPerfNested, now - e.t0);
  }
  if (s.n > 0) --s.n;
  s.mark = now;
}

Profiler &Profiler::get() {
  static Profiler p;
  return p;
}

Profiler::Profiler() = default;

void Profiler::set_enabled(bool on) {
  std::lock_guard<std::mutex> lk(mu_);
  on_ = on;
  g_on.store(on);
  portsim::g_perf_on.store(on);
  const int64_t now = portsim::perf_now_ns();
  if (!t0_) t0_ = now;
  line_t0_ = now;
}

bool Profiler::enabled() const { return g_on.load(std::memory_order_relaxed); }

bool Profiler::open_csv(const std::string &path) {
  std::lock_guard<std::mutex> lk(mu_);
  if (csv_) std::fclose(csv_);
  csv_ = std::fopen(path.c_str(), "wb");
  if (!csv_) return false;
  std::fprintf(csv_, "t_s,secs,frames,shown,paints,fps,sim_fps,frame_ms,frame_max_ms,work_ms,paint_ms,paint_max_ms");
  for (int i = 0; i < kPhases; ++i) std::fprintf(csv_, ",%s_ms", kNames[i]);
  std::fprintf(csv_, ",cursor_paints");  // P10E
  if (fps_changed_) std::fprintf(csv_, ",slide_paints,changed_paints");  // P12A fix 1
  std::fputc('\n', csv_);
  std::fflush(csv_);
  return true;
}

void Profiler::close_csv() {
  std::lock_guard<std::mutex> lk(mu_);
  if (csv_) std::fclose(csv_);
  csv_ = nullptr;
}

// P15A (log/roadmap/P15/P15A_notes.md): development switch SG_P15_FRAMELOG=<file>, one line per game frame on the sim thread, in
// microseconds: the frame (head to head), the wait, the capture, the gfx drops (inclusive: the terrain passes and nested host work
// inside), the 800 terrain passes, the world passes, the nested host work, then every perf.h detail counter (PerfDetail order)
void Profiler::frame_log(int64_t total, int64_t wait, int64_t cap, int64_t gfx_all, int64_t d800, int64_t dworld, int64_t dnest) {
  static std::FILE *f = [] {
    const char *e = std::getenv("SG_P15_FRAMELOG");
    std::FILE *h = e ? std::fopen(e, "wb") : nullptr;
    if (h) {
      std::fprintf(h, "frame_us wait_us capture_us gfx_all_us t800_us tworld_us nested_us");
      static const char *const kD[portsim::kPdCount] = {"world_render", "world_t", "world_copy", "queue_begin", "hr_flush", "hr_pub",
                                                        "hr_enqueue", "hr_now", "lines", "queue_end", "cap_dump", "cap_text",
                                                        "cap_cursor", "cap_world", "cap_world_text", "cap_hr_scene"};
      for (int i = 0; i < portsim::kPdCount; ++i) std::fprintf(h, " %s_us", kD[i]);
      std::fputc('\n', h);
    }
    return h;
  }();
  if (!f) return;
  int64_t d[portsim::kPdCount];
  for (int i = 0; i < portsim::kPdCount; ++i) d[i] = portsim::perf_detail_read(static_cast<portsim::PerfDetail>(i));
  std::fprintf(f, "%lld %lld %lld %lld %lld %lld %lld", static_cast<long long>(total / 1000), static_cast<long long>(wait / 1000),
               static_cast<long long>(cap / 1000), static_cast<long long>(gfx_all / 1000), static_cast<long long>(d800 / 1000),
               static_cast<long long>(dworld / 1000), static_cast<long long>(dnest / 1000));
  for (int i = 0; i < portsim::kPdCount; ++i) std::fprintf(f, " %lld", static_cast<long long>((d[i] - detail_prev_[i]) / 1000));
  std::fputc('\n', f);
  std::fflush(f);
  for (int i = 0; i < portsim::kPdCount; ++i) detail_prev_[i] = d[i];
}

void Profiler::frame_head() {
  if (!g_on.load(std::memory_order_relaxed)) return;
  const int64_t now = portsim::perf_now_ns();
  std::lock_guard<std::mutex> lk(mu_);
  int64_t perf[4];
  for (int i = 0; i < 4; ++i) perf[i] = portsim::perf_read(static_cast<portsim::PerfCounter>(i));
  int64_t ph[kPhases];
  for (int i = 0; i < kPhases; ++i) ph[i] = g_phase_ns[i].load(std::memory_order_relaxed);
  if (head_) {
    const double ms = 1e-6;
    const int64_t total = now - head_;
    const int64_t d800 = perf[portsim::kPerfTerrain800] - perf_prev_[portsim::kPerfTerrain800];
    const int64_t dworld = perf[portsim::kPerfTerrainWorld] - perf_prev_[portsim::kPerfTerrainWorld];
    const int64_t dnest = perf[portsim::kPerfNested] - perf_prev_[portsim::kPerfNested];
    const int64_t terrain = d800 + dworld;
    const int64_t gfx = std::max<int64_t>(0, perf[portsim::kPerfGfx] - perf_prev_[portsim::kPerfGfx] - terrain - dnest);
    const int64_t cap = ph[static_cast<int>(Phase::Capture)] - phase_prev_[static_cast<int>(Phase::Capture)];
    const int64_t wait = ph[static_cast<int>(Phase::Wait)] - phase_prev_[static_cast<int>(Phase::Wait)];
    const int64_t sim = std::max<int64_t>(0, total - gfx - terrain - cap - wait);
    sum_[static_cast<int>(Phase::Sim)] += sim * ms;
    sum_[static_cast<int>(Phase::Gfx)] += gfx * ms;
    sum_[static_cast<int>(Phase::Terrain)] += terrain * ms;
    sum_[static_cast<int>(Phase::Capture)] += cap * ms;
    sum_[static_cast<int>(Phase::Wait)] += wait * ms;
    frame_sum_ += total * ms;
    frame_max_ = std::max(frame_max_, total * ms);
    work_sum_ += (total - wait) * ms;
    ++frames_;
    frame_log(total, wait, cap, perf[portsim::kPerfGfx] - perf_prev_[portsim::kPerfGfx], d800, dworld, dnest);  // P15A
  }
  head_ = now;
  for (int i = 0; i < 4; ++i) perf_prev_[i] = perf[i];
  for (Phase p : {Phase::Capture, Phase::Wait}) phase_prev_[static_cast<int>(p)] = ph[static_cast<int>(p)];
  roll(now);
}

void Profiler::painted(bool new_frame, int64_t ns, bool cursor, bool slide, bool changed) {
  winlog_paint_flags(new_frame, new_frame || cursor || slide || changed);  // P16A
  if (!g_on.load(std::memory_order_relaxed)) return;
  const int64_t now = portsim::perf_now_ns();
  std::lock_guard<std::mutex> lk(mu_);
  for (int i = static_cast<int>(Phase::Base); i < kPhases; ++i) {
    const int64_t v = g_phase_ns[i].load(std::memory_order_relaxed);
    sum_[i] += (v - phase_prev_[i]) * 1e-6;
    phase_prev_[i] = v;
  }
  ++paints_;
  if (new_frame) ++shown_;
  if (cursor) ++cursor_;  // P10E
  if (slide) ++slide_;      // P12A fix 1
  if (changed) ++changed_;
  paint_sum_ += ns * 1e-6;
  paint_max_ = std::max(paint_max_, ns * 1e-6);
  roll(now);
}

void Profiler::roll(int64_t now) {
  if (!line_t0_) line_t0_ = now;
  const int64_t span = now - line_t0_;
  if (span < 1000000000LL) return;
  ProfLine l;
  l.secs = span * 1e-9;
  l.t_s = (now - t0_) * 1e-9;
  l.frames = frames_;
  l.shown = shown_;
  l.paints = paints_;
  l.cursor = cursor_;  // P10E
  l.slide = slide_;    // P12A fix 1
  l.changed = changed_;
  l.slide_line = fps_changed_;
  // P12A fix 1: with fps_changed_ the paints whose picture changed (a slide's steps count), else the frames shown (P10A)
  l.fps = (fps_changed_ ? changed_ : shown_) / l.secs;
  l.sim_fps = frames_ / l.secs;
  l.frame_ms = frames_ ? frame_sum_ / frames_ : 0;
  l.frame_max_ms = frame_max_;
  l.work_ms = frames_ ? work_sum_ / frames_ : 0;
  l.paint_ms = paints_ ? paint_sum_ / paints_ : 0;
  l.paint_max_ms = paint_max_;
  for (int i = 0; i < kPhases; ++i) {
    const uint32_t n = i < static_cast<int>(Phase::Base) ? frames_ : paints_;
    l.ms[i] = n ? sum_[i] / n : 0;
  }
  last_ = l;
  ++lines_;
  if (frames_) {
    ++sum_lines_;
    tot_fps_ += l.fps;
    tot_sim_fps_ += l.sim_fps;
    tot_frame_ += l.frame_ms;
    tot_paint_ += l.paint_ms;
    tot_paints_ += paints_ / l.secs;  // P10E
    tot_cursor_ += cursor_ / l.secs;
    tot_slide_ += slide_ / l.secs;  // P12A fix 1
    tot_shown_ += shown_ / l.secs;
    for (int i = 0; i < kPhases; ++i) tot_ms_[i] += l.ms[i];
  }
  if (csv_) {
    std::fprintf(csv_, "%.3f,%.3f,%u,%u,%u,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f", l.t_s, l.secs, l.frames, l.shown, l.paints, l.fps,
                 l.sim_fps, l.frame_ms, l.frame_max_ms, l.work_ms, l.paint_ms, l.paint_max_ms);
    for (int i = 0; i < kPhases; ++i) std::fprintf(csv_, ",%.3f", l.ms[i]);
    std::fprintf(csv_, ",%u", l.cursor);  // P10E
    if (fps_changed_) std::fprintf(csv_, ",%u,%u", l.slide, l.changed);  // P12A fix 1
    std::fputc('\n', csv_);
    std::fflush(csv_);
  }
  frames_ = shown_ = paints_ = cursor_ = slide_ = changed_ = 0;
  for (double &v : sum_) v = 0;
  frame_sum_ = frame_max_ = work_sum_ = paint_sum_ = paint_max_ = 0;
  line_t0_ = now;
}

ProfLine Profiler::last() const {
  std::lock_guard<std::mutex> lk(mu_);
  return last_;
}

uint64_t Profiler::lines() const {
  std::lock_guard<std::mutex> lk(mu_);
  return lines_;
}

void Profiler::set_font(const std::string &face, uint32_t style) {
  std::lock_guard<std::mutex> lk(mu_);
  face_ = face;
  style_ = style;
}

bool Profiler::font(std::string &face, uint32_t &style) const {
  std::lock_guard<std::mutex> lk(mu_);
  face = face_;
  style = style_;
  return !face_.empty();
}

std::string Profiler::summary_json() const {
  std::lock_guard<std::mutex> lk(mu_);
  char b[160];
  const double n = sum_lines_ ? static_cast<double>(sum_lines_) : 1.0;
  std::snprintf(b, sizeof b, "{\"on\":%d,\"lines\":%llu,\"fps\":%.2f,\"sim_fps\":%.2f,\"frame_ms\":%.2f,\"paint_ms\":%.2f", on_ ? 1 : 0,
                static_cast<unsigned long long>(sum_lines_), tot_fps_ / n, tot_sim_fps_ / n, tot_frame_ / n, tot_paint_ / n);
  std::string s = b;
  std::snprintf(b, sizeof b, ",\"paints_per_s\":%.2f,\"cursor_paints_per_s\":%.2f", tot_paints_ / n, tot_cursor_ / n);  // P10E
  s += b;
  if (fps_changed_) {  // P12A fix 1
    std::snprintf(b, sizeof b, ",\"slide_paints_per_s\":%.2f,\"frames_shown_per_s\":%.2f", tot_slide_ / n, tot_shown_ / n);
    s += b;
  }
  for (int i = 0; i < kPhases; ++i) {
    std::snprintf(b, sizeof b, ",\"%s_ms\":%.2f", kNames[i], tot_ms_[i] / n);
    s += b;
  }
  return s + "}";
}

std::string overlay_text(const ProfLine &l, int mode) {
  char b[240];
  if (mode <= 1) {
    std::snprintf(b, sizeof b, "%.1f fps", l.fps);
    return b;
  }
  auto m = [&](Phase p) { return l.ms[static_cast<int>(p)]; };
  std::string s;
  if (l.slide_line)  // P12A fix 1: the slide's paints too
    std::snprintf(b, sizeof b, "%.1f fps   game %.1f/s   frame %.0f ms (max %.0f)   paints %.0f/s  cursor %.0f/s  slide %.0f/s\n",
                  l.fps, l.sim_fps, l.frame_ms, l.frame_max_ms, l.secs > 0 ? l.paints / l.secs : 0.0,
                  l.secs > 0 ? l.cursor / l.secs : 0.0, l.secs > 0 ? l.slide / l.secs : 0.0);
  else
    std::snprintf(b, sizeof b, "%.1f fps   game %.1f/s   frame %.0f ms (max %.0f)   paints %.0f/s  cursor %.0f/s\n", l.fps,
                  l.sim_fps, l.frame_ms, l.frame_max_ms, l.secs > 0 ? l.paints / l.secs : 0.0, l.secs > 0 ? l.cursor / l.secs : 0.0);
  s += b;
  std::snprintf(b, sizeof b, "sim %.1f  gfx %.1f  terrain %.1f  capture %.1f  wait %.1f\n", m(Phase::Sim), m(Phase::Gfx),
                m(Phase::Terrain), m(Phase::Capture), m(Phase::Wait));
  s += b;
  std::snprintf(b, sizeof b, "paint %.1f  base %.1f  world %.1f  layers %.1f  text %.1f  bands %.1f  blit %.1f", l.paint_ms,
                m(Phase::Base), m(Phase::World), m(Phase::Layers), m(Phase::Text), m(Phase::Bands), m(Phase::Blit));
  s += b;
  if (g_gpu_line.load()) {  // P14A: the GPU path's phases
    std::snprintf(b, sizeof b, "  upload %.1f  patch %.1f  gpu %.1f  present %.1f", m(Phase::Upload), m(Phase::Patch), m(Phase::Gpu),
                  m(Phase::Present));
    s += b;
  }
  return s;
}

// P16A: SG_P16_PAINTLOG (profile.h)
namespace {
struct WinEv {
  char kind;
  int64_t t0, t1;
  uint64_t cyc;  // the thread's cycles inside the event
  int64_t a[8];
  int64_t st[7];  // the GPU stages in this paint (winlog_paint_stages)
};
struct WinLog {
  std::atomic<bool> on{false};
  std::string path;
  std::vector<WinEv> ev;      // window thread
  std::vector<int64_t> vb;    // pacer thread
  std::atomic<int64_t> last_vb{0};
  bool fresh = false, changed = false;
  int64_t gpu[7] = {};
  bool gpu_set = false;
  int64_t open_ns = 0;
  uint64_t open_tsc = 0;
  int64_t st_prev[7] = {}, st_cur[7] = {};
};
WinLog g_wl;
}  // namespace

bool winlog_on() { return g_wl.on.load(std::memory_order_relaxed); }

void winlog_open_env() {
  const char *p = std::getenv("SG_P16_PAINTLOG");
  if (!p || !*p) return;
  g_wl.path = p;
  g_wl.ev.reserve(1u << 20);
  g_wl.vb.reserve(1u << 20);
  g_wl.open_ns = portsim::perf_now_ns();
#ifdef _WIN32
  g_wl.open_tsc = __rdtsc();
#endif
  g_wl.on.store(true);
}

uint64_t winlog_cycles() {
#ifdef _WIN32
  ULONG64 c = 0;
  QueryThreadCycleTime(GetCurrentThread(), &c);
  return c;
#else
  return 0;
#endif
}

void winlog_vblank(int64_t ns) {
  if (!winlog_on()) return;
  g_wl.last_vb.store(ns, std::memory_order_relaxed);
  if (g_wl.vb.size() < g_wl.vb.capacity()) g_wl.vb.push_back(ns);
}

void winlog_paint_flags(bool fresh, bool changed) {
  g_wl.fresh = fresh;
  g_wl.changed = changed;
}

void winlog_paint_gpu(int64_t wait_ns, int64_t present_ns, uint32_t last_present, uint32_t st_present, uint32_t st_refresh,
                      uint32_t st_sync_refresh, int64_t st_sync_qpc) {
  if (!winlog_on()) return;
  g_wl.gpu[0] = wait_ns;
  g_wl.gpu[1] = present_ns;
  g_wl.gpu[2] = last_present;
  g_wl.gpu[3] = st_present;
  g_wl.gpu[4] = st_refresh;
  g_wl.gpu[5] = st_sync_refresh;
  g_wl.gpu[6] = st_sync_qpc;
  g_wl.gpu_set = true;
}

void winlog_paint_stages(const int64_t *cum, int n) {
  if (!winlog_on()) return;
  for (int i = 0; i < 7 && i < n; ++i) g_wl.st_cur[i] = cum[i];
}

void winlog_event(char kind, int64_t t0, uint64_t cyc0) {
  if (!winlog_on()) return;
  WinEv e{};
  e.kind = kind;
  e.t0 = t0;
  e.t1 = portsim::perf_now_ns();
  e.cyc = winlog_cycles() - cyc0;
  e.a[0] = g_wl.last_vb.load(std::memory_order_relaxed);
  if (kind == 'P') {
    e.a[1] = (g_wl.fresh ? 1 : 0) | (g_wl.changed ? 2 : 0) | (g_wl.gpu_set ? 4 : 0);
    for (int i = 0; i < 6; ++i) e.a[2 + i] = g_wl.gpu_set ? g_wl.gpu[i] : -1;
    g_wl.fresh = g_wl.changed = g_wl.gpu_set = false;
    for (int i = 0; i < 7; ++i) {
      e.st[i] = g_wl.st_cur[i] - g_wl.st_prev[i];
      g_wl.st_prev[i] = g_wl.st_cur[i];
    }
  }
  if (g_wl.ev.size() < g_wl.ev.capacity()) g_wl.ev.push_back(e);
}

void winlog_close() {
  if (!winlog_on()) return;
  g_wl.on.store(false);
  std::FILE *f = std::fopen(g_wl.path.c_str(), "wb");
  if (!f) return;
  const int64_t ns = portsim::perf_now_ns();
  double tsc_hz = 0;
#ifdef _WIN32
  const uint64_t tsc = __rdtsc();
  if (ns > g_wl.open_ns) tsc_hz = static_cast<double>(tsc - g_wl.open_tsc) * 1e9 / static_cast<double>(ns - g_wl.open_ns);
  LARGE_INTEGER qf;
  QueryPerformanceFrequency(&qf);
  std::fprintf(f, "# tsc_hz %.0f qpc_hz %lld\n", tsc_hz, static_cast<long long>(qf.QuadPart));
#endif
  std::fprintf(f, "# kind t0_ns t1_ns cycles last_vblank_ns flags wait_ns present_ns last_present st_present st_refresh st_sync_refresh"
                  " frame_ns world_ns course_ns final_ns draw_ns text_ns obj_ns\n");
  for (int64_t v : g_wl.vb) std::fprintf(f, "V %lld\n", static_cast<long long>(v));
  for (const WinEv &e : g_wl.ev) {
    std::fprintf(f, "%c %lld %lld %llu %lld", e.kind, static_cast<long long>(e.t0), static_cast<long long>(e.t1),
                 static_cast<unsigned long long>(e.cyc), static_cast<long long>(e.a[0]));
    if (e.kind == 'P') {
      for (int i = 1; i < 8; ++i) std::fprintf(f, " %lld", static_cast<long long>(e.a[i]));
      for (int i = 0; i < 7; ++i) std::fprintf(f, " %lld", static_cast<long long>(e.st[i]));
    }
    std::fputc('\n', f);
  }
  std::fclose(f);
}

}  // namespace portapp
