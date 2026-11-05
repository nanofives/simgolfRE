// P10A: the app's frame profiler (profile.h).
#include "profile.h"

#include <algorithm>
#include <atomic>

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
                                     "overlay", "blit"};
}  // namespace

const char *phase_name(Phase p) { return static_cast<int>(p) < kPhases ? kNames[static_cast<int>(p)] : "?"; }

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
  std::fputc('\n', csv_);
  std::fflush(csv_);
  return true;
}

void Profiler::close_csv() {
  std::lock_guard<std::mutex> lk(mu_);
  if (csv_) std::fclose(csv_);
  csv_ = nullptr;
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
  }
  head_ = now;
  for (int i = 0; i < 4; ++i) perf_prev_[i] = perf[i];
  for (Phase p : {Phase::Capture, Phase::Wait}) phase_prev_[static_cast<int>(p)] = ph[static_cast<int>(p)];
  roll(now);
}

void Profiler::painted(bool new_frame, int64_t ns) {
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
  l.fps = shown_ / l.secs;
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
    for (int i = 0; i < kPhases; ++i) tot_ms_[i] += l.ms[i];
  }
  if (csv_) {
    std::fprintf(csv_, "%.3f,%.3f,%u,%u,%u,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f", l.t_s, l.secs, l.frames, l.shown, l.paints, l.fps,
                 l.sim_fps, l.frame_ms, l.frame_max_ms, l.work_ms, l.paint_ms, l.paint_max_ms);
    for (int i = 0; i < kPhases; ++i) std::fprintf(csv_, ",%.3f", l.ms[i]);
    std::fputc('\n', csv_);
    std::fflush(csv_);
  }
  frames_ = shown_ = paints_ = 0;
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
  for (int i = 0; i < kPhases; ++i) {
    std::snprintf(b, sizeof b, ",\"%s_ms\":%.2f", kNames[i], tot_ms_[i] / n);
    s += b;
  }
  return s + "}";
}

std::string overlay_text(const ProfLine &l, int mode) {
  char b[200];
  if (mode <= 1) {
    std::snprintf(b, sizeof b, "%.1f fps", l.fps);
    return b;
  }
  auto m = [&](Phase p) { return l.ms[static_cast<int>(p)]; };
  std::string s;
  std::snprintf(b, sizeof b, "%.1f fps   game %.1f/s   frame %.0f ms (max %.0f)\n", l.fps, l.sim_fps, l.frame_ms, l.frame_max_ms);
  s += b;
  std::snprintf(b, sizeof b, "sim %.1f  gfx %.1f  terrain %.1f  capture %.1f  wait %.1f\n", m(Phase::Sim), m(Phase::Gfx),
                m(Phase::Terrain), m(Phase::Capture), m(Phase::Wait));
  s += b;
  std::snprintf(b, sizeof b, "paint %.1f  base %.1f  world %.1f  layers %.1f  text %.1f  bands %.1f  blit %.1f", l.paint_ms,
                m(Phase::Base), m(Phase::World), m(Phase::Layers), m(Phase::Text), m(Phase::Bands), m(Phase::Blit));
  s += b;
  return s;
}

}  // namespace portapp
