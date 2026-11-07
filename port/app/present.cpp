// Presenting the RGB555 back buffer (present.h).
#include "present.h"

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>  // P6B item 2 (std::min / max in paint)
#include <cmath>    // P10A: lround (the overlay)
#include <cstring>  // P10A: memcpy (the overlay)
#include <map>      // P12A fix 2
#include <tuple>
#include <string>

#include "gpu.h"               // P14A
#include "portsim/sim/perf.h"  // P10A: perf_now_ns
#include "profile.h"           // P10A
#include <atomic>              // P10A: RowPool
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace portapp {

// P10A: the rows of one compose loop split over worker threads. The loops it runs (render's base scale, compose_world's world
// pass, each compose_layers pass) read the frame's planes and the presenter's tables and write each client row from that row's own
// pixels only, so a row's result does not depend on which thread ran it or in what order: the image is the same as one thread's.
// Workers: min(4, hardware threads / 2) - 1 plus the calling (window) thread; SG_PAINT_THREADS=<n> sets the total (1 = no workers).
namespace {
class RowPool {
 public:
  static RowPool &get() {
    static RowPool p;
    return p;
  }
  // fn(ya, yb) over [y0, y1) in chunks; returns when every chunk ran
  void run(int32_t y0, int32_t y1, const std::function<void(int32_t, int32_t)> &fn) {
    const int32_t n = y1 - y0;
    const int32_t parts = std::min<int32_t>(static_cast<int32_t>(workers_.size()) + 1, n / 32);
    if (parts <= 1) {
      if (n > 0) fn(y0, y1);
      return;
    }
    {
      std::lock_guard<std::mutex> lk(mu_);
      fn_ = &fn;
      y0_ = y0;
      n_ = n;
      parts_ = parts;
      next_.store(0);
      pending_ = parts;
      ++gen_;
    }
    cv_.notify_all();
    work();
    std::unique_lock<std::mutex> lk(mu_);
    done_.wait(lk, [&] { return pending_ == 0; });
    fn_ = nullptr;
  }
  ~RowPool() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      stop_ = true;
    }
    cv_.notify_all();
    for (std::thread &t : workers_) t.join();
  }

 private:
  RowPool() {
    int total = 0;
    if (const char *e = std::getenv("SG_PAINT_THREADS")) total = std::atoi(e);
    if (total <= 0) total = std::min(4, std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2));
    for (int i = 1; i < total; ++i) workers_.emplace_back([this] { loop(); });
  }
  void work() {  // takes chunks until none is left
    for (;;) {
      const int32_t k = next_.fetch_add(1);
      if (k >= parts_) return;
      const int32_t a = y0_ + static_cast<int32_t>(static_cast<int64_t>(n_) * k / parts_);
      const int32_t b = y0_ + static_cast<int32_t>(static_cast<int64_t>(n_) * (k + 1) / parts_);
      (*fn_)(a, b);
      std::lock_guard<std::mutex> lk(mu_);
      if (--pending_ == 0) done_.notify_all();
    }
  }
  void loop() {
    uint64_t seen = 0;
    for (;;) {
      {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&] { return stop_ || gen_ != seen; });
        if (stop_) return;
        seen = gen_;
      }
      work();
    }
  }
  std::vector<std::thread> workers_;
  std::mutex mu_;
  std::condition_variable cv_, done_;
  const std::function<void(int32_t, int32_t)> *fn_ = nullptr;
  int32_t y0_ = 0, n_ = 0, parts_ = 0, pending_ = 0;
  std::atomic<int32_t> next_{0};
  uint64_t gen_ = 0;
  bool stop_ = false;
};
}  // namespace

Presenter::Presenter(PresentDefect d, ScaleMode s) : defect_(d), scale_(s) {}
Presenter::~Presenter() = default;  // P14A: GpuCompositor is complete here

Viewport fit_viewport(int32_t cw, int32_t ch) {
  Viewport v;
  if (cw <= 0 || ch <= 0) {
    v.client_w = kLogicalW;
    v.client_h = kLogicalH;
    return v;
  }
  if (static_cast<int64_t>(cw) * kLogicalH >= static_cast<int64_t>(ch) * kLogicalW) {  // wider than 4:3: bars left / right
    v.client_h = ch;
    v.client_w = static_cast<int32_t>(static_cast<int64_t>(ch) * kLogicalW / kLogicalH);
    v.origin_x = (cw - v.client_w) / 2;
  } else {  // taller: bars top / bottom
    v.client_w = cw;
    v.client_h = static_cast<int32_t>(static_cast<int64_t>(cw) * kLogicalH / kLogicalW);
    v.origin_y = (ch - v.client_h) / 2;
  }
  return v;
}

bool Presenter::set_frame_world(const std::vector<uint16_t> &px, BandFill band, WorldImage &&world) {
  const bool stale = defect_ == PresentDefect::Stale && !frame_.empty();
  WorldImage old = std::move(world_);  // P10A: handed back in `world` below (set_frame clears world_)
  const bool ok = set_frame(px, band);
  if (ok && !stale) {
    const bool sane = !world.empty() && world.pw > 0 && world.ph > 0 &&
                      world.px.size() == static_cast<size_t>(world.pw) * static_cast<size_t>(world.ph) &&
                      world.mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH && world.fw > 0;
    world_ = sane ? std::move(world) : WorldImage{};
    // P11B fix 1: the panel layer at the chosen edge (every reader below, the remap, the runs and the hover swaps take l.anchor)
    for (portsim::GfxHudLayer &l : world_.hud.layers)
      if (l.id == portsim::kGfxLayerPanel) l.anchor = panel_anchor_;
    slide_start(world_);  // P12A fix 1
    objects_start(old);   // P12A fix 2
  }
  if (!stale) world = std::move(old);  // P10A: the previous world image's buffers go back to the caller
  else world_ = std::move(old);
  return ok;
}

// ---- P12A fix 1: the camera slide (present.h)
bool camera_shift(const CameraSnap &a, const CameraSnap &b, int32_t &dx, int32_t &dy) {
  dx = dy = 0;
  if (!a.ok || !b.ok || a.zoom != b.zoom || a.rot != b.rot) return false;
  const int64_t X = static_cast<int64_t>(b.cam_x) - a.cam_x, Y = static_cast<int64_t>(b.cam_y) - a.cam_y, Z = b.zoom;
  int64_t sx = 0, sy = 0;
  switch (b.rot) {
    case 0: sx = -8 * Z * (X + Y); sy = 5 * Z * (X - Y); break;
    case 2: sx = 8 * Z * (X - Y); sy = 5 * Z * (X + Y); break;
    case 4: sx = 8 * Z * (X + Y); sy = 5 * Z * (Y - X); break;
    case 6: sx = 8 * Z * (Y - X); sy = -5 * Z * (X + Y); break;
    default: return false;
  }
  if (sx < INT32_MIN || sx > INT32_MAX || sy < INT32_MIN || sy > INT32_MAX) return false;
  dx = static_cast<int32_t>(sx);
  dy = static_cast<int32_t>(sy);
  return true;
}

void Presenter::slide_start(const WorldImage &w) {
  SlideStart st;
  ++slide_stats_.frames;
  // the game frame period: the mean interval of the sim thread's presents (40..200 ms intervals only: a paused game or a modal
  // screen's burst does not count)
  if (w.cam.t_ns > 0 && last_present_ns_ > 0) {
    const double d = static_cast<double>(w.cam.t_ns - last_present_ns_);
    if (d >= 40e6 && d <= 200e6) period_ns_ = period_ns_ > 0 ? period_ns_ * 0.75 + d * 0.25 : d;
  }
  if (w.cam.t_ns > 0) last_present_ns_ = w.cam.t_ns;
  if (period_ns_ <= 0) period_ns_ = 93e6;  // the original's 87 ms + one waitTicks quantum (P10A_notes.md)
  st.period_ms = period_ns_ / 1e6;
  slide_stats_.period_ms = st.period_ms;
  const int64_t now = portsim::perf_now_ns();
  const bool was_on = slide_on_;
  const int64_t end_prev = t0_ns_ + static_cast<int64_t>(slide_ns_);
  slide_on_ = false;
  cur_x_ = cur_y_ = 0;
  nd_x_ = nd_y_ = 0;
  if (!smooth_) return;
  bool dim = false;
  for (const portsim::GfxHudLayer &l : w.hud.layers) dim = dim || l.dim != 0;
  int32_t dx = 0, dy = 0;
  int reason = 0;
  if (layered() && !w.hud_none && !dim && w.cam.ok) {  // the next frame's cursor reads project with CameraSnap::next_*
    CameraSnap n = w.cam;
    n.cam_x = w.cam.next_x;
    n.cam_y = w.cam.next_y;
    n.zoom = w.cam.next_zoom;
    n.rot = w.cam.next_rot;
    if (!camera_shift(w.cam, n, nd_x_, nd_y_) || std::abs(nd_x_) > kSlideMaxX || std::abs(nd_y_) > kSlideMaxY) nd_x_ = nd_y_ = 0;
  }
  st.ndx = nd_x_;
  st.ndy = nd_y_;
  if (!layered() || w.hud_none || dim) reason = 4;
  else if (!w.cam.ok || !painted_cam_.ok || !(scene_ok_ || prev_ok_)) reason = 1;
  else if (!camera_shift(painted_cam_, w.cam, dx, dy)) reason = 2;
  else if (std::abs(dx) > kSlideMaxX || std::abs(dy) > kSlideMaxY) reason = 3;
  st.dx = dx;
  st.dy = dy;
  const int zi = w.cam.zoom == 1 ? 0 : w.cam.zoom == 2 ? 1 : w.cam.zoom == 4 ? 2 : w.cam.zoom == 8 ? 3 : -1;
  if (zi >= 0 && reason == 0) {
    slide_stats_.max_dx[zi] = std::max(slide_stats_.max_dx[zi], std::abs(dx));
    slide_stats_.max_dy[zi] = std::max(slide_stats_.max_dy[zi], std::abs(dy));
  }
  double s0x = 0, s0y = 0;
  if (reason == 0) {
    // the scale of the last paint's content rectangle (the client the slide's pixels are counted in)
    const Viewport pv = fit_viewport(scene_ok_ ? scene_w_ : prev_w_, scene_ok_ ? scene_h_ : prev_h_);
    s0x = painted_x_ - static_cast<double>(dx) * pv.client_w / kLogicalW;
    s0y = painted_y_ - static_cast<double>(dy) * pv.client_h / kLogicalH;
    const int32_t pw = scene_ok_ ? scene_w_ : prev_w_, ph = scene_ok_ ? scene_h_ : prev_h_;
    if (std::fabs(s0x) >= 0.6 * pw || std::fabs(s0y) >= 0.6 * ph) reason = 3;
    else if (std::fabs(s0x) < 0.5 && std::fabs(s0y) < 0.5) reason = 5;
  }
  st.reason = reason;
  st.s0x = s0x;
  st.s0y = s0y;
  slide_start_ = st;
  if (reason == 1) ++slide_stats_.snaps_cam;
  if (reason == 2) ++slide_stats_.snaps_view;
  if (reason == 3) ++slide_stats_.snaps_far;
  if (reason == 4) ++slide_stats_.snaps_screen;
  if (reason != 0) {
    if (reason != 5) prev_ok_ = false;
    return;
  }
  // the uncovered pixels show the course picture of the last paint (if no paint came since the last frame, the one before it is
  // still what the window shows and stays the underlay)
  if (scene_ok_) {
    scene_prev_.swap(scene_last_);
    ++swap_gen_;  // P14A: the GPU path swaps its course targets the same way
    prev_w_ = scene_w_;
    prev_h_ = scene_h_;
    scene_ok_ = false;
    prev_ok_ = true;
  }
  ++slide_stats_.slides;
  slide_on_ = true;
  s0x_ = s0x;
  s0y_ = s0y;
  s0x_i_ = static_cast<int32_t>(std::lround(s0x));
  s0y_i_ = static_cast<int32_t>(std::lround(s0y));
  t0_ns_ = now;
  // a camera move slides until one game frame period after the frame's present on the sim thread (CameraSnap::t_ns: so the window's
  // late or early taking of a frame does not stretch or squeeze the slide, and consecutive slides end where the next frame is due);
  // a frame with no move inside a slide finishes the remaining offset by the time the running slide would have ended (one display
  // period at least)
  const double end_new = static_cast<double>(w.cam.t_ns > 0 ? w.cam.t_ns : now) + period_ns_;
  slide_ns_ = (dx != 0 || dy != 0 || !was_on) ? std::max<double>(end_new - static_cast<double>(now), 16e6)
                                               : std::max<double>(static_cast<double>(end_prev - now), 16e6);
  cur_x_ = s0x_i_;
  cur_y_ = s0y_i_;
}

bool Presenter::slide_tick(int64_t now_ns, const Viewport &vp) {
  ++slide_stats_.ticks;
  int32_t x = 0, y = 0;
  if (slide_on_) {
    const double f = slide_ns_ > 0 ? static_cast<double>(now_ns - t0_ns_) / slide_ns_ : 1.0;
    if (f >= 1.0) {
      slide_on_ = false;
    } else {
      const double g = 1.0 - std::max(0.0, f);
      x = static_cast<int32_t>(std::lround(s0x_ * g));
      y = static_cast<int32_t>(std::lround(s0y_ * g));
    }
  }
  const bool changed = x != cur_x_ || y != cur_y_;
  cur_x_ = x;
  cur_y_ = y;
  if (remap_) {  // the game's offset: the slide's minus the next camera's shift in client pixels
    const int32_t gx = vp.client_w > 0 ? static_cast<int32_t>(std::lround(static_cast<double>(nd_x_) * vp.client_w / kLogicalW)) : 0;
    const int32_t gy = vp.client_h > 0 ? static_cast<int32_t>(std::lround(static_cast<double>(nd_y_) * vp.client_h / kLogicalH)) : 0;
    remap_->set_slide(x, y, x - gx, y - gy);
  }
  if (changed) ++slide_stats_.moves;
  return changed;
}

// ---- P12A fix 2: the moving objects between two game frames (present.h objects_tick)
void Presenter::objects_start(const WorldImage &prev) {
  movers_.clear();
  patched_.clear();
  objects_t0_ = portsim::perf_now_ns();
  if (!objects_on_) return;
  ++object_stats_.frames;
  const std::shared_ptr<const portsim::GfxHrScene> &ns = world_.hr_scene, &ps = prev.hr_scene;
  if (!ns || !ps || ns == ps) return;  // no queue, or the same queue presented again (nothing moved)
  if (portsim::gfx_hr_scene_pw(*ns) != portsim::gfx_hr_scene_pw(*ps) || portsim::gfx_hr_scene_ph(*ns) != portsim::gfx_hr_scene_ph(*ps) ||
      world_.px.size() != static_cast<size_t>(portsim::gfx_hr_scene_pw(*ns)) * static_cast<size_t>(portsim::gfx_hr_scene_ph(*ns)))
    return;
  int32_t cdx = 0, cdy = 0;  // the camera's shift between the two pictures (800-space)
  if (!camera_shift(prev.cam, world_.cam, cdx, cdy)) return;
  const std::vector<portsim::GfxHrDraw> &nd = portsim::gfx_hr_scene_draws(*ns), &pd = portsim::gfx_hr_scene_draws(*ps);
  // the previous frame's tagged draws by (kind, record, occurrence)
  std::map<std::tuple<int, int32_t, int32_t>, size_t> pm;
  std::map<std::pair<int, int32_t>, int32_t> cnt;
  for (size_t i = 0; i < pd.size(); ++i) {
    const portsim::GfxMoverSrc &s = pd[i].src;
    if (s.kind == portsim::kGfxMoverNone) continue;
    const int32_t o = cnt[{s.kind, s.index}]++;
    pm[{s.kind, s.index, o}] = i;
  }
  cnt.clear();
  for (size_t k = 0; k < nd.size(); ++k) {
    const portsim::GfxMoverSrc &s = nd[k].src;
    if (s.kind == portsim::kGfxMoverNone) continue;
    const int32_t o = cnt[{s.kind, s.index}]++;
    const auto it = pm.find({s.kind, s.index, o});
    if (it == pm.end()) continue;  // new this frame: drawn where the frame has it
    const portsim::GfxHrDraw &a = pd[it->second], &b = nd[k];
    // the step from the entries' own placements (GfxHrDraw::ex / ey: the same for every animation cel, whose art and trim differ;
    // the cel itself is the frame's, drawn at the blended place)
    if (a.art_w != b.art_w || a.art_h != b.art_h) ++object_stats_.rejected_art;  // (counted: a cel change, still matched)
    const bool ball = s.kind == portsim::kGfxMoverBall || s.kind == portsim::kGfxMoverShadow;
    const int64_t lim = ball ? kMoverMaxWorldBall : kMoverMaxWorld;
    const double dx = a.ex + cdx - b.ex, dy = a.ey + cdy - b.ey;
    if (std::llabs(static_cast<int64_t>(a.src.wx) - b.src.wx) > lim || std::llabs(static_cast<int64_t>(a.src.wy) - b.src.wy) > lim ||
        std::fabs(dx) > kMoverMaxStep || std::fabs(dy) > kMoverMaxStep) {
      ++object_stats_.rejected_far;  // a teleport (the next tee, a new ball): drawn where the frame has it
      continue;
    }
    ++object_stats_.matched;
    if (dx == 0.0 && dy == 0.0) continue;
    Mover m;
    m.k = k;
    m.dx = dx;
    m.dy = dy;
    m.extrap = ball;
    m.src = s;
    m.ox = ball ? 0.0 : dx;  // the place drawn at the frame's arrival
    m.oy = ball ? 0.0 : dy;
    movers_.push_back(m);
    if (s.kind > 0 && s.kind < 8) ++object_stats_.moved_kind[static_cast<size_t>(s.kind)];
  }
  if (movers_.empty()) return;
  ++object_stats_.moved;
  px_orig_ = world_.px;
  pxt_orig_ = world_.px_text;
  for (Mover &m : movers_) m.ox = m.oy = 1e9;  // objects_tick draws the first place
}

bool Presenter::objects_tick(int64_t now_ns) {
  ++object_stats_.ticks;
  if (movers_.empty() || !world_.hr_scene) return false;
  const portsim::GfxHrScene &sc = *world_.hr_scene;
  const int32_t pw = portsim::gfx_hr_scene_pw(sc), ph = portsim::gfx_hr_scene_ph(sc);
  if (world_.px.size() != static_cast<size_t>(pw) * static_cast<size_t>(ph) || px_orig_.size() != world_.px.size() || world_.fw <= 0)
    return false;
  const double T = period_ns_ > 0 ? period_ns_ : 93e6;
  const double t = std::min(1.0, std::max(0.0, static_cast<double>(now_ns - objects_t0_) / T));
  // P12A fix 3: the repaint cap (the last place of a frame, t = 1, is drawn whenever it is due)
  // P14A fix 2: --object-rate auto decides the cap from the path and the client size of the last paint
  const int auto_hz = object_auto_ ? object_rate_now() : 0;
  const double min_ns = object_auto_ ? (auto_hz > 0 ? 1e9 / auto_hz : 0.0) : object_min_ns_;
  if (min_ns > 0 && t < 1.0 && static_cast<double>(now_ns - object_last_ns_) < min_ns) return false;
  const double kx = static_cast<double>(world_.fw) / kLogicalW, ky = static_cast<double>(ph) / kLogicalH;
  // the offsets now, to a quarter of a world pixel; nothing to draw when none changed
  bool changed = false;
  for (Mover &m : movers_) {
    const double ox = m.extrap ? -m.dx * t : m.dx * (1.0 - t), oy = m.extrap ? -m.dy * t : m.dy * (1.0 - t);
    const double qx = std::round(ox * kx * 4) / (kx * 4), qy = std::round(oy * ky * 4) / (ky * 4);
    if (qx != m.ox || qy != m.oy) changed = true;
    m.ox = qx;
    m.oy = qy;
  }
  if (!changed) return false;
  object_last_ns_ = now_ns;
  const int64_t t0 = portsim::perf_now_ns();
  const bool text = pxt_orig_.size() == px_orig_.size() && world_.px_text.size() == world_.px.size();
  auto restore = [&](const std::array<int32_t, 4> &r) {
    for (int32_t y = r[1]; y < r[3]; ++y) {
      const size_t o = static_cast<size_t>(y) * static_cast<size_t>(pw);
      std::copy(px_orig_.begin() + static_cast<std::ptrdiff_t>(o + r[0]), px_orig_.begin() + static_cast<std::ptrdiff_t>(o + r[2]),
                world_.px.begin() + static_cast<std::ptrdiff_t>(o + r[0]));
      if (text)
        std::copy(pxt_orig_.begin() + static_cast<std::ptrdiff_t>(o + r[0]), pxt_orig_.begin() + static_cast<std::ptrdiff_t>(o + r[2]),
                  world_.px_text.begin() + static_cast<std::ptrdiff_t>(o + r[0]));
    }
  };
  for (const auto &r : patched_) restore(r);
  world_dirty_.insert(world_dirty_.end(), patched_.begin(), patched_.end());  // P14A: the GPU path uploads these rectangles
  patched_.clear();
  // the rectangles: each moved object's box at the frame's place and at the place drawn now (2 world pixels of margin), merged
  const std::vector<portsim::GfxHrDraw> &dr = portsim::gfx_hr_scene_draws(sc);
  std::vector<std::array<int32_t, 4>> regs;
  std::vector<double> dxv(dr.size(), 0.0), dyv(dr.size(), 0.0);
  for (const Mover &m : movers_) {
    if (m.k >= dr.size()) continue;
    dxv[m.k] = m.ox;
    dyv[m.k] = m.oy;
    if (m.ox == 0.0 && m.oy == 0.0) continue;
    const int32_t *b = dr[m.k].box;
    const double sx = m.ox * kx, sy = m.oy * ky;
    std::array<int32_t, 4> r = {std::min(b[0], static_cast<int32_t>(std::floor(b[0] + sx))) - 2,
                                std::min(b[1], static_cast<int32_t>(std::floor(b[1] + sy))) - 2,
                                std::max(b[2], static_cast<int32_t>(std::ceil(b[2] + sx))) + 2,
                                std::max(b[3], static_cast<int32_t>(std::ceil(b[3] + sy))) + 2};
    r = {std::max(0, r[0]), std::max(0, r[1]), std::min(pw, r[2]), std::min(ph, r[3])};
    if (r[2] > r[0] && r[3] > r[1]) regs.push_back(r);
  }
  // (P12A fix 3: the rectangles are not merged: each lies inside its object's base rectangle of the scene, gfx.h kGfxMoverMaxStep;
  // where two overlap, both redraws hold every moved draw, so the pixels they share are the same)
  std::vector<uint16_t> out, out0;
  std::vector<uint8_t> cov, cov0;
  const std::vector<double> none;
  for (const auto &r : regs) {
    portsim::gfx_hr_scene_redraw(sc, r[0], r[1], r[2], r[3], dxv, dyv, out, cov);
    portsim::gfx_hr_scene_redraw(sc, r[0], r[1], r[2], r[3], none, none, out0, cov0);  // where the frame's draws covered
    if (out.size() != static_cast<size_t>(r[2] - r[0]) * static_cast<size_t>(r[3] - r[1]) || out0.size() != out.size()) continue;
    const size_t rw = static_cast<size_t>(r[2] - r[0]);
    for (int32_t y = r[1]; y < r[3]; ++y)
      for (int32_t x = r[0]; x < r[2]; ++x) {
        const size_t i = static_cast<size_t>(y - r[1]) * rw + static_cast<size_t>(x - r[0]);
        if (!cov[i] && !cov0[i]) continue;  // no draw there before or after: the frame's own pixel (labels' bars, the terrain)
        const size_t j = static_cast<size_t>(y) * static_cast<size_t>(pw) + static_cast<size_t>(x);
        world_.px[j] = out[i];
        if (text) world_.px_text[j] = out[i];
      }
    object_stats_.redraw_px += static_cast<uint64_t>(out.size());
    patched_.push_back(r);
    world_dirty_.push_back(r);  // P14A
  }
  ++object_stats_.redraws;
  ++world_gen_;  // P14A
  object_stats_.redraw_ns += portsim::perf_now_ns() - t0;
  changed_pending_ = true;
  return true;
}

int Presenter::object_rate_now() const {  // P14A fix 2
  if (!object_auto_) return object_min_ns_ > 0 ? static_cast<int>(std::lround(1e9 / object_min_ns_)) : 0;
  return gpu_on() && painted_px_ > 0 && painted_px_ <= kObjectAutoPx ? 0 : kObjectAutoHz;
}

void Presenter::frame_objects(int32_t tagged[8], int32_t moving[8]) const {
  for (int k = 0; k < 8; ++k) tagged[k] = moving[k] = 0;
  if (world_.hr_scene)
    for (const portsim::GfxHrDraw &d : portsim::gfx_hr_scene_draws(*world_.hr_scene))
      if (d.src.kind > 0 && d.src.kind < 8) ++tagged[d.src.kind];
  for (const Mover &m : movers_)
    if (m.src.kind > 0 && m.src.kind < 8) ++moving[m.src.kind];
}

void Presenter::mover_offset(const portsim::GfxMoverSrc &src, const Viewport &vp, int32_t &dx, int32_t &dy) const {
  dx = dy = 0;
  for (const Mover &m : movers_)
    if (m.src == src && m.ox < 1e8) {
      dx = static_cast<int32_t>(std::lround(m.ox * vp.client_w / kLogicalW));
      dy = static_cast<int32_t>(std::lround(m.oy * vp.client_h / kLogicalH));
      return;
    }
}

// the client pixels the slid course picture does not cover (outside the world layer's place at the slid viewport `vs`) from the
// last paint's course picture, moved with the slide: client pixel p shows scene_prev_ at p - (offset - start offset)
void Presenter::fill_gap(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vs, int32_t slx, int32_t sly) const {
  if (prev_w_ != cw || prev_h_ != ch || scene_prev_.size() != static_cast<size_t>(cw) * static_cast<size_t>(ch)) return;
  const WorldImage &w = world_;
  auto fdiv = [](int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
  int32_t rx0 = vs.origin_x, rx1 = vs.origin_x + vs.client_w;
  const int32_t ry0 = std::max(0, vs.origin_y), ry1 = std::min(ch, vs.origin_y + vs.client_h);
  if (w.fw > 0) {
    rx0 = std::min<int32_t>(rx0, vs.origin_x + static_cast<int32_t>(fdiv(static_cast<int64_t>(-w.ox) * vs.client_w, w.fw)));
    rx1 = std::max<int32_t>(rx1, vs.origin_x - static_cast<int32_t>(fdiv(-static_cast<int64_t>(w.pw - w.ox) * vs.client_w, w.fw)));
  }
  rx0 = std::max(0, rx0);
  rx1 = std::min(cw, rx1);
  const int32_t ux = slx - s0x_i_, uy = sly - s0y_i_;
  const Canvas C = cv_;  // P14A: the canvas's rows / columns inside the client
  const int32_t cy0 = std::max(0, C.y0), cy1 = std::min(ch, C.y0 + C.h), cx0 = std::max(0, C.x0), cx1 = std::min(cw, C.x0 + C.w);
  RowPool::get().run(cy0, cy1, [&](int32_t ya, int32_t yb) {
    for (int32_t y = ya; y < yb; ++y) {
      const int32_t sy = std::min(std::max(y - uy, 0), ch - 1);
      const uint32_t *src = scene_prev_.data() + static_cast<size_t>(sy) * static_cast<size_t>(cw);
      uint32_t *dst = out.data() + static_cast<size_t>(y - C.y0) * static_cast<size_t>(C.w);
      const bool inside = y >= ry0 && y < ry1 && rx1 > rx0;
      auto span = [&](int32_t a, int32_t b) {
        a = std::max(a, cx0);
        b = std::min(b, cx1);
        for (int32_t x = a; x < b; ++x) dst[x - C.x0] = src[std::min(std::max(x - ux, 0), cw - 1)];
      };
      if (!inside) {
        span(0, cw);
      } else {
        span(0, rx0);
        span(rx1, cw);
      }
    }
  });
}

int32_t Presenter::layer_dx(const portsim::GfxHudLayer &l, const Viewport &vp, int32_t dxl, int32_t dxr) const {
  if (l.id == portsim::kGfxLayerStrip && strip_right_ && l.x1 > l.x0 && vp.client_w > 0) {
    // the palette ends at the content rectangle's right edge once moved left (client x = client_w); the strip's first face
    // starts there: its own 800-space left edge x0 maps to origin_x + x0 * scale before the move
    const int32_t at = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(l.x0) * vp.client_w / kLogicalW);
    return vp.client_w - at;
  }
  return l.anchor < 0 ? dxl : l.anchor > 0 ? dxr : 0;
}

// ---- G32: PanelRemap
void PanelRemap::set(bool on, const Viewport &vp, int32_t cw, const std::vector<uint8_t> &mask) {
  std::lock_guard<std::mutex> lk(mu_);
  layered_ = false;  // P9B
  on_ = on && vp.origin_x > 0 && mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
  vp_ = vp;
  cw_ = cw;
  if (on_) mask_ = mask;
}
void PanelRemap::clear() {
  std::lock_guard<std::mutex> lk(mu_);
  on_ = false;
}
void PanelRemap::set_cursor_answer(int32_t dx, int32_t lx, int32_t ly) const {  // P9E fix 4
  std::lock_guard<std::mutex> lk(mu_);
  cursor_dx_ = dx;
  cursor_lx_ = lx;
  cursor_ly_ = ly;
}
int32_t PanelRemap::cursor_dx() const {
  std::lock_guard<std::mutex> lk(mu_);
  return on_ && layered_ ? cursor_dx_ : 0;
}
void PanelRemap::cursor_answer(int32_t &lx, int32_t &ly) const {  // P10E
  std::lock_guard<std::mutex> lk(mu_);
  lx = cursor_lx_;
  ly = cursor_ly_;
}
uint64_t PanelRemap::remapped() const {
  std::lock_guard<std::mutex> lk(mu_);
  return remapped_;
}
void PanelRemap::set_layers(bool on, const Viewport &vp, int32_t cw, std::vector<RemapLayer> &&layers) {
  std::lock_guard<std::mutex> lk(mu_);
  on_ = on && vp.origin_x > 0;
  vp_ = vp;
  cw_ = cw;
  layered_ = true;
  layers_ = std::move(layers);
}
void PanelRemap::map(int32_t &x, int32_t &y) const {
  bool v = false;
  map(x, y, v);
}
void PanelRemap::map(int32_t &x, int32_t &y, bool &vacated) const { map_impl(x, y, vacated, true, 1); }
void PanelRemap::map(int32_t &x, int32_t &y, bool &vacated, int32_t &slid_x, int32_t &slid_y) const {  // P12A fix 1
  slid_x = slid_y = 0;
  map_impl(x, y, vacated, true, 1, &slid_x, &slid_y);
}
void PanelRemap::map_noslide(int32_t &x, int32_t &y) const {  // P12A fix 1
  bool v = false;
  map_impl(x, y, v, true, 0);
}
void PanelRemap::peek(int32_t &x, int32_t &y) const {  // P10E
  bool v = false;
  map_impl(x, y, v, false, 2);
}
void PanelRemap::peek(int32_t &x, int32_t &y, int32_t &slid_x, int32_t &slid_y) const {  // P12A fix 1
  bool v = false;
  slid_x = slid_y = 0;
  map_impl(x, y, v, false, 2, &slid_x, &slid_y);
}
void PanelRemap::set_slide(int32_t pic_dx, int32_t pic_dy, int32_t game_dx, int32_t game_dy) const {  // P12A fix 1
  std::lock_guard<std::mutex> lk(mu_);
  slide_dx_ = pic_dx;
  slide_dy_ = pic_dy;
  game_dx_ = game_dx;
  game_dy_ = game_dy;
}
uint64_t PanelRemap::slid() const {  // P12A fix 1
  std::lock_guard<std::mutex> lk(mu_);
  return slid_;
}
void PanelRemap::map_impl(int32_t &x, int32_t &y, bool &vacated, bool count, int slide, int32_t *slx, int32_t *sly) const {
  vacated = false;
  std::lock_guard<std::mutex> lk(mu_);
  if (!on_ || vp_.client_w <= 0 || vp_.client_h <= 0) return;
  const int32_t dxl = -vp_.origin_x, dxr = cw_ - vp_.origin_x - vp_.client_w;
  if (layered_) {  // P9B: the topmost layer with a pixel under the point decides
    if (y < vp_.origin_y || y >= vp_.origin_y + vp_.client_h) return;
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp_.origin_y) * kLogicalH / vp_.client_h);
    bool course = true;  // P12A fix 1: the point lies over the course picture (no layer pixel, or a centred pixel the slide moves)
    for (size_t k = layers_.size(); k-- > 0;) {
      const RemapLayer &l = layers_[k];
      const int32_t dx = l.fixed ? l.dx : l.anchor < 0 ? dxl : l.anchor > 0 ? dxr : 0;  // P9E: fixed
      const int32_t sx = x - dx;
      if (sx < vp_.origin_x || sx >= vp_.origin_x + vp_.client_w) continue;
      const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(sx - vp_.origin_x) * kLogicalW / vp_.client_w);
      if (lx < l.x0 || lx >= l.x1 || ly < l.y0 || ly >= l.y1) continue;
      const size_t i = static_cast<size_t>(ly - l.y0) * static_cast<size_t>(l.x1 - l.x0) + static_cast<size_t>(lx - l.x0);
      if (!l.cover[i]) continue;
      if (dx != 0) {
        x = sx;
        if (count) ++remapped_;
        return;
      }
      course = i < l.slides.size() && l.slides[i] != 0;  // P12A fix 1
      break;  // a centred layer's pixel: the point stays (P9E fix 3: and may still be a vacated place, below)
    }
    // P12A fix 1: a point over the course while the camera slide moves the shown picture (or the next frame's camera differs from
    // the picture's): `map` gives the game the place the shown picture has under the point, `peek` that place in the shown frame's
    // coordinates (applied after the vacated test below, which keeps the window's own point)
    const int32_t odx = slide == 1 ? game_dx_ : slide == 2 ? slide_dx_ : 0, ody = slide == 1 ? game_dy_ : slide == 2 ? slide_dy_ : 0;
    struct SlideAfter {
      const PanelRemap &r;
      int32_t &x, &y;
      bool on;
      bool count;
      int32_t dx, dy;
      int32_t *ox, *oy;
      ~SlideAfter() {
        if (!on) return;
        x -= dx;
        y -= dy;
        if (ox) *ox = dx;
        if (oy) *oy = dy;
        if (count) ++r.slid_;
      }
    } slide_after{*this, x, y, course && (odx != 0 || ody != 0), count, odx, ody, slx, sly};
    // P9E fix 3: no moved layer under the point; a moved panel layer's box holding its 800-space point makes it a vacated place
    // (a centred pixel there, a golfer's label, was over the panel in the 800-space frame and is over the course now)
    if (x >= vp_.origin_x && x < vp_.origin_x + vp_.client_w) {
      const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp_.origin_x) * kLogicalW / vp_.client_w);
      for (const RemapLayer &l : layers_) {
        const int32_t dx = l.fixed ? 0 : l.anchor < 0 ? dxl : l.anchor > 0 ? dxr : 0;
        if (dx != 0 && lx >= l.x0 && lx < l.x1 && ly >= l.y0 && ly < l.y1) {
          vacated = true;
          break;
        }
      }
    }
    return;
  }
  for (const HudPanel &p : kHudPanels) {
    const int32_t sx = x - (p.anchor < 0 ? dxl : dxr);
    if (sx < vp_.origin_x || sx >= vp_.origin_x + vp_.client_w || y < vp_.origin_y || y >= vp_.origin_y + vp_.client_h) continue;
    const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(sx - vp_.origin_x) * kLogicalW / vp_.client_w);
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp_.origin_y) * kLogicalH / vp_.client_h);
    if (lx < p.x0 || lx >= p.x1 || ly < p.y0 || ly >= p.y1) continue;
    if (mask_[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)]) continue;  // the world shows there
    x = sx;
    if (count) ++remapped_;
    return;
  }
  // P9E fix 3: not remapped, and its 800-space point inside a moved panel rectangle: a vacated place
  if (x >= vp_.origin_x && x < vp_.origin_x + vp_.client_w && y >= vp_.origin_y && y < vp_.origin_y + vp_.client_h) {
    const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp_.origin_x) * kLogicalW / vp_.client_w);
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp_.origin_y) * kLogicalH / vp_.client_h);
    for (const HudPanel &p : kHudPanels)
      if (lx >= p.x0 && lx < p.x1 && ly >= p.y0 && ly < p.y1) vacated = true;
  }
}

bool Presenter::set_frame(const std::vector<uint16_t> &px, BandFill band) {
  if (px.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return false;
  ++frames_set_;
  ++content_gen_;  // P14A
  if (defect_ == PresentDefect::Stale && !frame_.empty()) return true;
  frame_ = px;
  band_ = band;
  world_ = WorldImage{};  // P8B: a frame set without its world layer has none
  live_ = CursorLive{};  // P10E: the new frame's previews are at its own cursor tile
  slide_on_ = false;     // P12A fix 1: a frame without a slide start (set_frame_world's slide_start may begin one) is unmoved
  cur_x_ = cur_y_ = 0;
  nd_x_ = nd_y_ = 0;
  movers_.clear();  // P12A fix 2 (objects_start may find new ones)
  patched_.clear();
  world_dirty_.clear();  // P14A: a new world image is uploaded whole
  hover_pristine_.clear();  // P10I fix 2: the new frame's own looks
  if (defect_ == PresentDefect::ByteSwap)
    for (uint16_t &v : frame_) v = static_cast<uint16_t>((v >> 8) | (v << 8));
  return true;
}

ScaleMode Presenter::effective_scale(int32_t w, int32_t h) const {
  if (scale_ != ScaleMode::Auto) return scale_;
  const bool integer = w > 0 && w % kLogicalW == 0 && h == w / kLogicalW * kLogicalH;
  return integer ? ScaleMode::Nearest : ScaleMode::Smooth;
}

// P10A: the logical mask under which compose_world writes the world over every content-rectangle pixel of this paint (the base's
// pixels there are never seen, so render does not compute them), or null. compose_world's `wmask` (mask_text when the runs are
// drawn), its mask test (lx, ly by the floor mapping) and its range tests: a content pixel's world position lies inside the world
// (x in [ox - 1, ox + fw), y in [-1, ph)) when 0 <= ox and ox + fw <= pw. The anchor path's frame_img copy is read only where that
// mask is 0. Null with the mask-ignored control or a world whose geometry is not that.
const uint8_t *Presenter::base_hidden(const Viewport &vp, bool text) const {
  const WorldImage &w = world_;
  if (w.empty() || defect_ == PresentDefect::WorldMaskIgnored || vp.client_w <= 0 || vp.client_h <= 0) return nullptr;
  if (w.ox < 0 || w.fw <= 0 || w.ox + w.fw > w.pw || w.ph <= 0) return nullptr;
  const bool wtext = text && w.mask_text.size() == w.mask.size() && w.px_text.size() == w.px.size();
  const std::vector<uint8_t> &m = wtext ? w.mask_text : w.mask;
  return m.size() == static_cast<size_t>(kLogicalW) * kLogicalH ? m.data() : nullptr;
}

void Presenter::render(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vpo) const {
  cv_ = Canvas{0, 0, cw > 0 ? cw : 0, ch > 0 ? ch : 0, true};  // P14A: the whole client
  render_impl(cw, ch, out, vpo);
}

// P14A: render over a rectangle of the client (present.h render_rect)
void Presenter::render_rect(int32_t cw, int32_t ch, int32_t x0, int32_t y0, int32_t w, int32_t h, bool course,
                            std::vector<uint32_t> &out) const {
  const Canvas was = cv_;
  const bool q = quiet_;
  quiet_ = true;
  force_slide0_ = course_only_ = course;
  cv_ = Canvas{x0, y0, w > 0 ? w : 0, h > 0 ? h : 0, false};
  render_impl(cw, ch, out, nullptr);
  quiet_ = q;
  force_slide0_ = course_only_ = false;
  cv_ = was;
}

void Presenter::render_impl(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vpo) const {
  const ProfScope prof(Phase::Base);  // P10A
  const Viewport vp = fit_viewport(cw, ch);
  if (vpo) *vpo = vp;
  const Canvas C = cv_;  // P14A: client pixel (x, y) is out[(y - C.y0) * C.w + x - C.x0]
  out.assign(static_cast<size_t>(C.w) * static_cast<size_t>(C.h), 0xff000000u);
  if (frame_.empty() || cw <= 0 || ch <= 0 || C.w <= 0 || C.h <= 0) return;
  const bool side = C.full && !quiet_;  // P14A: the scene kept for the slide and the remap only from a paint of the whole client
  // P9B: with the frame's HUD layers the scaled image is the layers' base (the root before the first panel drew); the layers
  // are composed over it in compose_layers
  // P9C: with the frame's text runs, the picture without their low-resolution glyphs (the runs are drawn after the scaling)
  const bool text = text_now(vp);
  const std::vector<uint16_t> &fsrc = layered() ? hud_for(text).base
                                      : text && text_defect_ != TextDefect::Unremoved ? text_.clean : frame_;
  // P12A fix 1: the camera slide moves the course picture (the base, the world, their runs) by (slx, sly) client pixels: `vs` is the
  // content rectangle it is drawn into; the HUD layers keep `vp`. The rows / columns of vs outside the client are not drawn.
  const bool keep = smooth_ && layered();  // the course picture is kept for the next frame's slide
  if (!keep && side) {  // a screen without the course picture: the next in-game frame starts unmoved
    scene_ok_ = prev_ok_ = false;
    painted_cam_ = CameraSnap{};
  }
  int32_t slx = 0, sly = 0;
  if (keep && !force_slide0_ && slide_on_ && prev_ok_ && prev_w_ == cw && prev_h_ == ch) {
    slx = cur_x_;
    sly = cur_y_;
  }
  Viewport vs = vp;
  vs.origin_x += slx;
  vs.origin_y += sly;
  // (P14A: the canvas's rows / columns; the whole client for paint)
  const int32_t ya0 = std::max(0, C.y0 - vs.origin_y), ya1 = std::min(vs.client_h, C.y0 + C.h - vs.origin_y);
  const int32_t xa0 = std::max(0, C.x0 - vs.origin_x), xa1 = std::min(vs.client_w, C.x0 + C.w - vs.origin_x);
  const int32_t off_x = vs.origin_x - C.x0;  // P14A: a vs column's index in a canvas row
  if (probe_ != 0) {  // P14A: the HUD layers over a one-word course picture (present.h probe_)
    std::fill(out.begin(), out.end(), probe_);
    if (layered()) compose_layers(out, cw, ch, vp, slx, sly);
    return;
  }
  auto tail = [&]() {  // the world, then (layered) the uncovered pixels, the course runs, the kept picture and the HUD layers
    if (!world_.empty()) compose_world(out, cw, ch, vs);  // P8B
    if (layered()) {
      if (slx != 0 || sly != 0) fill_gap(out, cw, ch, vs, slx, sly);
      if (text) {  // P9C (moved here from compose_layers by P12A: the same calls, before every layer)
        draw_runs(out, cw, ch, vs, true, -3, 0);  // the wide scene's world runs (beside the frame's columns)
        draw_runs(out, cw, ch, vs, true, -1, 0);  // the base's
      }
      if (keep && side) {
        scene_last_.resize(out.size());
        std::memcpy(scene_last_.data(), out.data(), out.size() * sizeof(uint32_t));
        scene_w_ = cw;
        scene_h_ = ch;
        painted_x_ = slx;
        painted_y_ = sly;
        painted_cam_ = world_.cam;
        scene_ok_ = true;
      }
      if (!course_only_) compose_layers(out, cw, ch, vp, slx, sly);  // P14A: course_only_ = render_rect's course picture
    }
    if (text && !layered() && !course_only_) draw_runs(out, cw, ch, vp, false, 0, 0);  // P9C (the layered path draws them above)
    if (world_.empty() && wide_ != WideFill::Off && band_ != BandFill::Black)
      fill_bands(out, cw, ch, vp);  // P7 (P13B: ExtendClamp too); P14A fix 3: the sides the canvas holds (fill_bands' side_in)
  };
  if (effective_scale(vp.client_w, vp.client_h) == ScaleMode::Smooth && vp.client_w > 0 && vp.client_h > 0) {
    // P5B item 4: bilinear. The frame converted to BGRA once, then per client pixel four neighbours with 8-bit weights.
    std::vector<uint32_t> &src = base_bgra_;  // P10A: kept between paints
    if (src.size() != fsrc.size() || base_bgra_src_ != fsrc.data() || base_bgra_gen_ != content_gen_) {  // P14A: once per content
      src.resize(fsrc.size());
      for (size_t i = 0; i < fsrc.size(); ++i) src[i] = rgb555_to_bgra(fsrc[i]);
      base_bgra_src_ = fsrc.data();
      base_bgra_gen_ = content_gen_;
    }
    const uint8_t *hidden = base_hidden(vp, text);  // P10A
    std::vector<int32_t> &hlx = base_lx_;
    if (hidden) {
      hlx.resize(static_cast<size_t>(vp.client_w));
      for (int32_t x = 0; x < vp.client_w; ++x) hlx[static_cast<size_t>(x)] = static_cast<int32_t>(static_cast<int64_t>(x) * kLogicalW / vp.client_w);
    }
    struct Tap {
      int32_t i0, i1;
      uint32_t f;  // weight of i1, 0..256
    };
    auto taps = [](int32_t n_out, int32_t n_in) {
      std::vector<Tap> t(static_cast<size_t>(n_out));
      for (int32_t k = 0; k < n_out; ++k) {
        // source position of the pixel centre, in 1/256: ((k + 0.5) * n_in / n_out - 0.5) * 256
        int64_t p = ((2 * static_cast<int64_t>(k) + 1) * n_in * 128) / n_out - 128;
        if (p < 0) p = 0;
        const int64_t last = static_cast<int64_t>(n_in - 1) * 256;
        if (p > last) p = last;
        Tap &a = t[static_cast<size_t>(k)];
        a.i0 = static_cast<int32_t>(p >> 8);
        a.f = static_cast<uint32_t>(p & 255);
        a.i1 = a.i0 + 1 < n_in ? a.i0 + 1 : a.i0;
      }
      return t;
    };
    const std::vector<Tap> tx = taps(vp.client_w, kLogicalW), ty = taps(vp.client_h, kLogicalH);
    auto mix = [](uint32_t a, uint32_t b, uint32_t f) {  // per channel a + (b - a) * f / 256
      const uint32_t g = 256 - f;
      const uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
      const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
      return rb | (ag << 8);
    };
    RowPool::get().run(ya0, ya1, [&](int32_t ya, int32_t yb) {  // P10A: rows over the paint threads; P12A: vs, clipped
      for (int32_t y = ya; y < yb; ++y) {
        const Tap &v = ty[static_cast<size_t>(y)];
        const uint32_t *s0 = src.data() + static_cast<size_t>(v.i0) * kLogicalW;
        const uint32_t *s1 = src.data() + static_cast<size_t>(v.i1) * kLogicalW;
        uint32_t *dst = out.data() + static_cast<size_t>(y + vs.origin_y - C.y0) * static_cast<size_t>(C.w);  // P14A: canvas row
        const uint8_t *hrow = hidden ? hidden + static_cast<size_t>(static_cast<int64_t>(y) * kLogicalH / vp.client_h) * kLogicalW : nullptr;
        for (int32_t x = xa0; x < xa1; ++x) {
          if (hrow && hrow[hlx[static_cast<size_t>(x)]]) continue;  // P10A: the world covers it
          const Tap &u = tx[static_cast<size_t>(x)];
          const uint32_t a = mix(s0[u.i0], s0[u.i1], u.f), b = mix(s1[u.i0], s1[u.i1], u.f);
          dst[off_x + x] = mix(a, b, v.f) | 0xff000000u;
        }
      }
    });
    tail();  // P12A (the calls that followed here, in the same order)
    return;
  }
  std::vector<int32_t> col(static_cast<size_t>(vp.client_w));
  for (int32_t x = 0; x < vp.client_w; ++x)
    col[static_cast<size_t>(x)] = static_cast<int32_t>(static_cast<int64_t>(x) * kLogicalW / vp.client_w);
  const uint8_t *hidden = base_hidden(vp, text);  // P10A
  RowPool::get().run(ya0, ya1, [&](int32_t ya, int32_t yb) {  // P10A: rows over the paint threads; P12A: vs, clipped
    for (int32_t y = ya; y < yb; ++y) {
      const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y) * kLogicalH / vp.client_h);
      const uint16_t *src = fsrc.data() + static_cast<size_t>(ly) * kLogicalW;
      uint32_t *dst = out.data() + static_cast<size_t>(y + vs.origin_y - C.y0) * static_cast<size_t>(C.w);  // P14A: canvas row
      const uint8_t *hrow = hidden ? hidden + static_cast<size_t>(ly) * kLogicalW : nullptr;  // P10A: the world covers those
      for (int32_t x = xa0; x < xa1; ++x)
        if (!hrow || !hrow[col[static_cast<size_t>(x)]]) dst[off_x + x] = rgb555_to_bgra(src[col[static_cast<size_t>(x)]]);
    }
  });
  tail();  // P12A (the calls that followed here, in the same order)
}

// ---- P9C: the text runs (textres.h)
void Presenter::set_text(portsim::GfxTextFrame &&tf) {
  ++content_gen_;  // P14A
  text_ok_ = text_layered_ok_ = false;
  text_ = portsim::GfxTextFrame{};
  hud_text_ = portsim::GfxHudFrame{};
  if (!text_native_ || tf.empty() || tf.back.size() != frame_.size() || tf.clean.size() != frame_.size()) return;
  if (defect_ == PresentDefect::Stale && frames_set_ > 1) return;
  text_ = std::move(tf);
  text_ok_ = true;
  // the layered path: the base and the layers without the runs' glyphs, when they belong to the frame's HUD layers
  if (!world_.empty() && !world_.hud.empty() && text_.hud_serial == world_.hud.serial &&
      text_.base_clean.size() == world_.hud.base.size()) {
    text_layered_ok_ = true;
    hud_text_ = world_.hud;  // the layers without the runs' glyphs (world_.hud stays: the picture at 800 x 600)
    if (text_defect_ != TextDefect::Unremoved) {
      hud_text_.base = text_.base_clean;
      for (const portsim::GfxTextLayerPatch &p : text_.patches) {
        if (p.layer < 0 || static_cast<size_t>(p.layer) >= hud_text_.layers.size()) continue;
        std::vector<portsim::GfxLayerPx> &lp = hud_text_.layers[static_cast<size_t>(p.layer)].px;
        for (size_t k = 0; k < p.idx.size() && k < p.px.size(); ++k)
          if (p.idx[k] < lp.size()) lp[p.idx[k]] = p.px[k];
      }
    }
    text_.base_clean.clear();
    text_.patches.clear();
  }
  text_.back.clear();  // checked; not needed any more
}

const portsim::GfxHudFrame &Presenter::hud_for(bool text) const {
  return text && text_layered_ok_ ? hud_text_ : world_.hud;
}

bool Presenter::text_now(const Viewport &vp) const {
  if (!text_ok_ || vp.client_w <= 0 || vp.client_h <= 0) return false;
  if (layered() && !text_layered_ok_) return false;
  const bool unit = vp.client_w == kLogicalW && vp.client_h == kLogicalH;
  return !unit || text_unit_redraw_;
}

// P9E: the popup piece that owns 800-space pixel (x, y) (GfxHudFrame::owner), -1 for none
int32_t Presenter::piece_of(const portsim::GfxHudFrame &f, int32_t x, int32_t y) {
  if (x < 0 || y < 0 || x >= kLogicalW || y >= kLogicalH || f.owner.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return -1;
  const uint8_t o = f.owner[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  return o != 0 && o <= f.pieces.size() ? static_cast<int32_t>(o) - 1 : -1;
}

// the runs of one layer (layered: -1 the base, k the layer; flat: every run) at dx client pixels
void Presenter::draw_runs(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp, bool layered_path, int32_t layer,
                          int32_t dx) const {
  const ProfScope prof(Phase::Text);  // P10A
  (void)ch;  // P14A: the runs are drawn into the canvas (cv_), whose size bounds them
  if (probe_ != 0) return;  // P14A: the probes compose the layers alone
  if (!raster_) raster_.reset(new TextRaster());
  raster_->set_defect(text_defect_);
  raster_->set_aa(text_aa_);
  raster_->set_fit(text_fit_);  // P10B fix 2
  // G32's rectangle compositor (no layers): a run inside a moved panel rectangle moves with it
  const bool rect = !layered_path && !world_.empty() && hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved &&
                    !world_.hud_none;  // P9D
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  for (const portsim::GfxTextRun &r : text_.runs) {
    if (layered_path && r.layer != layer) continue;
    if (r.target == 2 && world_.empty()) continue;  // a world run with no world under it
    int32_t d = dx;
    // P9E: a run of a layer with popup pixels moves with the popup that owns its box's centre
    if (layered_path && layer >= 0 && piece_frame_ && static_cast<size_t>(layer) < piece_frame_->layers.size() &&
        piece_frame_->layers[static_cast<size_t>(layer)].popups) {
      const int32_t k = piece_of(*piece_frame_, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2);
      if (k >= 0 && static_cast<size_t>(k) < piece_dx_.size() && piece_dx_[static_cast<size_t>(k)] != 0)
        d = piece_dx_[static_cast<size_t>(k)];
    }
    if (rect)
      for (const HudPanel &p : kHudPanels)
        if (r.x0 >= p.x0 && r.x1 <= p.x1 && r.y0 >= p.y0 && r.y1 <= p.y1) {
          d = p.anchor < 0 ? dxl : dxr;
          break;
        }
    // P14A: the canvas's own coordinates (the whole client for paint): the viewport moved by its origin
    Viewport vc = vp;
    vc.origin_x -= cv_.x0;
    vc.origin_y -= cv_.y0;
    if (objects_on_ && r.mover.kind != portsim::kGfxMoverNone && !movers_.empty()) {  // P12A fix 2: a label moves with its object
      int32_t mx = 0, my = 0;
      mover_offset(r.mover, vp, mx, my);
      Viewport vr = vc;
      vr.origin_x += mx;
      vr.origin_y += my;
      raster_->draw(r, layered_path, vr, d, out, cv_.w, cv_.h);
      continue;
    }
    raster_->draw(r, layered_path, vc, d, out, cv_.w, cv_.h);
  }
}

// P8B: the world layer under the frame. Client pixel (x, y) maps to world (ox + (x - vp.origin_x) * fw / vp.client_w,
// (y - vp.origin_y) * ph / vp.client_h): the identity when the world was made at this client size. Inside the content rectangle
// the frame's pixel stays where the mask of its logical pixel (floor((x - origin) * 800 / client_w), likewise y) is 0. The world
// is sampled at the nearest pixel when the mapping is 1:1 or the scale mode resolves to Nearest, else bilinear (8-bit weights).
void Presenter::compose_world(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  const ProfScope prof(Phase::World);  // P10A
  const WorldImage &w = world_;
  if (vp.client_w <= 0 || vp.client_h <= 0 || cw <= 0 || ch <= 0) return;
  // G32: the HUD compositor. The frame's image as drawn (frame_img), the mask with every moved panel's 2D pixels vacated (they show
  // the world), then each panel's 2D pixels pasted at their anchored place
  const bool layers = layered();  // P9B: the panels are layers composed after the world (compose_layers), not rectangles
  const bool anchor = !layers && hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved && !w.hud_none;  // P9D: hud_none
  // P9C: with the runs drawn, the mask taken over the picture without their glyphs (a label over the course shows the world)
  const bool wtext = text_now(vp) && w.mask_text.size() == w.mask.size() && w.px_text.size() == w.px.size();
  const std::vector<uint8_t> &wmask = wtext ? w.mask_text : w.mask;
  const std::vector<uint16_t> &wpx = wtext ? w.px_text : w.px;
  if (remap_ && !layers && !quiet_) remap_->set(anchor, vp, cw, w.mask);  // P14A: not from render_rect
  const Canvas C = cv_;  // P14A: the columns / rows of the canvas
  std::vector<uint32_t> frame_img;
  std::vector<uint8_t> mask_moved;
  const uint8_t *maskp = wmask.data();
  if (anchor) {
    frame_img = out;
    mask_moved = wmask;
    for (const HudPanel &p : kHudPanels)
      for (int32_t y = p.y0; y < p.y1; ++y)
        for (int32_t x = p.x0; x < p.x1; ++x) mask_moved[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)] = 1;
    maskp = mask_moved.data();
  }
  const bool identity = w.fw == vp.client_w && w.ph == vp.client_h;
  const bool nearest = identity || effective_scale(vp.client_w, vp.client_h) == ScaleMode::Nearest;
  static const std::vector<uint32_t> pal = [] {
    std::vector<uint32_t> t(32768);
    for (uint32_t v = 0; v < 32768; ++v) t[v] = rgb555_to_bgra(static_cast<uint16_t>(v));
    return t;
  }();
  // the world position of each client column / row: nearest = floor((k - origin) * n_world / n_client) + world origin (exact);
  // bilinear = the client pixel centre in 1/256 world pixels, ((k - origin + 0.5) * n_world / n_client - 0.5) * 256
  auto pos = [nearest](int32_t k, int32_t origin, int32_t n_client, int32_t n_world, int32_t world_origin) -> int64_t {
    const int64_t d = static_cast<int64_t>(k) - origin;
    if (nearest) {
      const int64_t num = d * n_world;
      const int64_t q = num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client);
      return (q + world_origin) * 256;
    }
    const int64_t num = (2 * d + 1) * n_world * 128;
    const int64_t q = num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client);
    return q - 128 + static_cast<int64_t>(world_origin) * 256;
  };
  // (P14A: indexed by x - C.x0 / y - C.y0)
  std::vector<int64_t> wx(static_cast<size_t>(C.w)), wy(static_cast<size_t>(C.h));
  for (int32_t x = C.x0; x < C.x0 + C.w; ++x) wx[static_cast<size_t>(x - C.x0)] = pos(x, vp.origin_x, vp.client_w, w.fw, w.ox);
  for (int32_t y = C.y0; y < C.y0 + C.h; ++y) wy[static_cast<size_t>(y - C.y0)] = pos(y, vp.origin_y, vp.client_h, w.ph, 0);
  auto mix = [](uint32_t a, uint32_t b, uint32_t f) {
    const uint32_t g = 256 - f;
    const uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
    const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
    return rb | (ag << 8);
  };
  // P10A: per column and per row once: the world pixel index of the tap and of its right / lower neighbour clamped into the world
  // (as `at` clamped them), the weight, whether the column / row is outside the world, and the column's logical x (-1 outside the
  // content rectangle); then the pixel loop does no division and no clamp
  std::vector<int32_t> &ct = cw_tab_;
  ct.resize(static_cast<size_t>(C.w) * 4);
  auto clampi = [](int64_t v, int32_t n) { return static_cast<int32_t>(v < 0 ? 0 : v >= n ? n - 1 : v); };
  for (int32_t x = C.x0; x < C.x0 + C.w; ++x) {
    const int64_t sx = wx[static_cast<size_t>(x - C.x0)];
    const int64_t x0 = sx >> 8;
    int32_t *c = ct.data() + static_cast<size_t>(x - C.x0) * 4;
    c[0] = x0 < -2 || x0 >= w.pw + 2 ? -1 : clampi(x0, w.pw);  // -1: outside the world (stays as it is)
    c[1] = clampi(x0 + 1, w.pw);
    c[2] = static_cast<int32_t>(sx & 255);
    c[3] = x >= vp.origin_x && x < vp.origin_x + vp.client_w
               ? static_cast<int32_t>(static_cast<int64_t>(x - vp.origin_x) * kLogicalW / vp.client_w)
               : -1;
  }
  const bool mask_on = defect_ != PresentDefect::WorldMaskIgnored;
  RowPool::get().run(C.y0, C.y0 + C.h, [&](int32_t ya, int32_t yb) {  // P10A: rows over the paint threads
    for (int32_t y = ya; y < yb; ++y) {
      const int64_t sy = wy[static_cast<size_t>(y - C.y0)];
      const int64_t y0 = sy >> 8;
      if (y0 < -2 || y0 >= w.ph + 2) continue;  // rows outside the world stay as they are (within 2: the edge row, clamped)
      const bool row_in = y >= vp.origin_y && y < vp.origin_y + vp.client_h;
      const int32_t ly = row_in ? static_cast<int32_t>(static_cast<int64_t>(y - vp.origin_y) * kLogicalH / vp.client_h) : 0;
      const uint8_t *mrow = maskp + static_cast<size_t>(ly) * kLogicalW;
      const uint16_t *r0 = wpx.data() + static_cast<size_t>(clampi(y0, w.ph)) * static_cast<size_t>(w.pw);
      const uint16_t *r1 = wpx.data() + static_cast<size_t>(clampi(y0 + 1, w.ph)) * static_cast<size_t>(w.pw);
      const uint32_t fy = static_cast<uint32_t>(sy & 255);
      uint32_t *dst = out.data() + static_cast<size_t>(y - C.y0) * static_cast<size_t>(C.w);  // P14A: canvas row, column x - C.x0
      for (int32_t x = 0; x < C.w; ++x) {
        const int32_t *c = ct.data() + static_cast<size_t>(x) * 4;
        if (row_in && c[3] >= 0 && mask_on && !mrow[c[3]]) continue;  // the frame's own pixel
        if (c[0] < 0) continue;  // columns outside the world stay as they are (within 2: the edge column, clamped)
        if (nearest) {
          dst[x] = pal[r0[c[0]] & 0x7fff];
        } else {
          const uint32_t fx = static_cast<uint32_t>(c[2]);
          const uint32_t a = mix(pal[r0[c[0]] & 0x7fff], pal[r0[c[1]] & 0x7fff], fx), b = mix(pal[r1[c[0]] & 0x7fff], pal[r1[c[1]] & 0x7fff], fx);
          dst[x] = mix(a, b, fy) | 0xff000000u;
        }
      }
    }
  });
  if (layers) return;  // P9B: render composes the layers next (P12A: after the course runs, with the unslid viewport)
  if (!anchor || !C.full) return;  // P14A: G32's rectangles on the whole client only (the canvas is the client there)
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  for (const HudPanel &p : kHudPanels) {
    const int32_t dx = p.anchor < 0 ? dxl : dxr;
    const int32_t cx0 = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x0) * vp.client_w / kLogicalW);
    const int32_t cx1 = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x1) * vp.client_w / kLogicalW);
    const int32_t cy0 = vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(p.y0) * vp.client_h / kLogicalH);
    const int32_t cy1 = vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(p.y1) * vp.client_h / kLogicalH);
    for (int32_t y = std::max(cy0, 0); y < std::min(cy1, ch); ++y) {
      const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp.origin_y) * kLogicalH / vp.client_h);
      for (int32_t x = std::max(cx0, 0); x < std::min(cx1, cw); ++x) {
        const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp.origin_x) * kLogicalW / vp.client_w);
        if (lx < p.x0 || lx >= p.x1 || ly < p.y0 || ly >= p.y1) continue;
        if (wmask[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)]) continue;  // not a 2D pixel of the frame
        const int32_t tx = x + dx;
        if (tx < 0 || tx >= cw) continue;
        out[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(tx)] =
            frame_img[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(x)];
      }
    }
  }
}

// ---- P9B: the layered compositor
bool Presenter::layered() const {
  return defect_ != PresentDefect::LayerRectMove && !world_.empty() && !world_.hud.empty() &&
         world_.hud.base.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
}

// Every layer of the frame in draw order over what is already composed (the base and the world). A layer moved to an edge is
// drawn at its anchor: left = -origin_x client pixels, right = + the right band's width, centred = 0, at the frame's scale (window
// height / 600). Client pixel (x, y) of a layer at offset dx shows the layer's 800-space pixel under (x - dx, y): nearest takes the
// one pixel, smooth the four around the client pixel's centre with bilinear weights, each of them composed over the client pixel
// already there (d): not drawn = d, opaque = its word, mask blend = jgld blitPair1's channel rule over d's 5-bit channels with the
// layer's source and weight, shade = the shade table at d's RGB555 word. The weighted sum of the four gives smooth panel edges with
// no world pixel chosen inside a panel by a mask, and a blended pixel (frame borders, half-bright strips) shows what is under it at
// the panel's new place.
// P14A: compose_layers' P9E piece offsets (factored out of it, the same body)
bool Presenter::pieces_on(const portsim::GfxHudFrame &f, const Viewport &vp) const {
  const bool anchor = hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved;
  return anchor && popups_on_ && !f.pieces.empty() && f.owner.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
}

void Presenter::layer_pieces(const portsim::GfxHudFrame &f, const Viewport &vp, int32_t cw) const {
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  // P9E: each popup piece's client offset: its opener panel's anchor offset, clamped so the piece's box stays inside the window
  piece_dx_.assign(f.pieces.size(), 0);
  piece_frame_ = nullptr;
  const bool pieces = pieces_on(f, vp);
  if (pieces) {
    piece_frame_ = &f;
    std::vector<uint8_t> centred(f.pieces.size(), 0);  // P10C fix 4: pieces moved by the text-box centring
    for (size_t k = 0; k < f.pieces.size(); ++k) {
      const portsim::GfxHudPiece &p = f.pieces[k];
      const int8_t a = layer_anchor(p.opener);  // P11B fix 1
      // P10C fix 4: a piece drawn inside a centred dialog (its textInputBox, the overwrite question) moves with that dialog
      if (text_boxes_ && a == 0 && p.parent >= 0 && static_cast<size_t>(p.parent) < k && centred[static_cast<size_t>(p.parent)]) {
        piece_dx_[k] = piece_dx_[static_cast<size_t>(p.parent)];
        centred[k] = 1;
        continue;
      }
      // P10C fix 4: a text input dialog no panel opened (the SAVE GAME box: saveGameDialog's panel, x 0x36 .. 0x2aa) is centred:
      // its box's centre to the frame's centre, in client pixels
      const bool centre = text_boxes_ && a == 0 && p.text && p.opener == portsim::kGfxLayerCentred;
      if ((a == 0 && !centre) || p.x1 <= p.x0) continue;
      int32_t d = centre ? static_cast<int32_t>(static_cast<int64_t>(kLogicalW - p.x0 - p.x1) * vp.client_w / (2 * kLogicalW))
                  : a < 0 ? dxl : dxr;
      const int32_t left = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x0) * vp.client_w / kLogicalW);
      const int32_t right = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x1) * vp.client_w / kLogicalW);
      if (right + d > cw) d = cw - right;
      if (left + d < 0) d = -left;
      piece_dx_[k] = d;
      centred[k] = centre ? 1 : 0;
    }
  }
}

// P14A: compose_layers' searches for the draw-order places of the tool previews (P9E fix 4: the first centred layer holding one of
// their pixels, else after every layer) and of the tee lines (P10I fix 1: the first centred layer holding a line pixel, not before the
// previews), the same loops, run once per frame / content generation instead of at every paint
void Presenter::draw_order(const portsim::GfxHudFrame &f, bool cur, bool lines, size_t &cur_layer, size_t &line_layer) const {
  const uint64_t key = content_gen_ * 4 + (cur ? 2 : 0) + (lines ? 1 : 0);
  if (order_key_ == &f && order_gen_ == key && cur_layer_c_ <= f.layers.size() && line_layer_c_ <= f.layers.size()) {
    cur_layer = cur_layer_c_;
    line_layer = line_layer_c_;
    return;
  }
  const WorldImage &W = world_;
  auto cur_at = [&](int32_t x, int32_t y) {
    return cur && x >= 0 && y >= 0 && x < kLogicalW && y < kLogicalH && W.cur_mask[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  };
  auto line_at = [&](int32_t x, int32_t y, const portsim::GfxLayerPx &p) {
    if (!lines || x < 0 || y < 0 || x >= kLogicalW || y >= kLogicalH) return false;
    const size_t i = static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x);
    return f.line800[i] != 0 && p.kind == 1 && p.px == f.line_word[i];
  };
  cur_layer = f.layers.size();
  for (size_t li = 0; cur && li < f.layers.size() && cur_layer == f.layers.size(); ++li) {
    const portsim::GfxHudLayer &l = f.layers[li];
    if (l.id != portsim::kGfxLayerCentred) continue;
    for (int32_t y = l.y0; y < l.y1 && cur_layer == f.layers.size(); ++y)
      for (int32_t x = l.x0; x < l.x1; ++x)
        if (cur_at(x, y) && l.at(x, y)) {
          cur_layer = li;
          break;
        }
  }
  line_layer = f.layers.size();
  for (size_t li = 0; lines && li < f.layers.size() && line_layer == f.layers.size(); ++li) {
    const portsim::GfxHudLayer &l = f.layers[li];
    if (l.id != portsim::kGfxLayerCentred) continue;
    for (int32_t y = l.y0; y < l.y1 && line_layer == f.layers.size(); ++y) {
      const uint8_t *m = f.line800.data() + static_cast<size_t>(y) * kLogicalW;
      for (int32_t x = l.x0; x < l.x1; ++x)
        if (m[x]) {
          const portsim::GfxLayerPx *p = l.at(x, y);
          if (p && line_at(x, y, *p)) {
            line_layer = li;
            break;
          }
        }
    }
  }
  if (cur && line_layer < cur_layer) line_layer = cur_layer;  // the previews first, as P7 draws them
  order_key_ = &f;
  order_gen_ = key;
  cur_layer_c_ = cur_layer;
  line_layer_c_ = line_layer;
}

// P14A: compose_layers' P9B click remap (factored out of it, the same body)
void Presenter::layer_remap(const portsim::GfxHudFrame &f, const Viewport &vp, int32_t cw) const {
  if (!remap_ || quiet_) return;
  const bool anchor = hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved;
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  const bool pieces = pieces_on(f, vp);
  auto moved_piece = [&](const portsim::GfxHudLayer &l, int32_t x, int32_t y) -> int32_t {
    if (!pieces || !l.popups) return -1;
    const int32_t k = piece_of(f, x, y);
    return k >= 0 && piece_dx_[static_cast<size_t>(k)] != 0 ? k : -1;
  };
  {
    std::vector<RemapLayer> rl;
    rl.reserve(f.layers.size());
    for (const portsim::GfxHudLayer &l : f.layers) {
      // P10I fix 2: a layer a hover swap changed keeps the frame's own coverage (clicks are unchanged)
      const size_t lix = static_cast<size_t>(&l - f.layers.data());
      const std::vector<portsim::GfxLayerPx> &lpx =
          lix < hover_pristine_.size() && hover_pristine_[lix].size() == l.px.size() ? hover_pristine_[lix] : l.px;
      RemapLayer r;
      r.anchor = anchor ? l.anchor : 0;
      if (anchor && l.id == portsim::kGfxLayerStrip && strip_right_) {  // P13C item 10: the strip at its own place
        r.fixed = true;
        r.dx = layer_dx(l, vp, dxl, dxr);
      }
      r.x0 = l.x0;
      r.y0 = l.y0;
      r.x1 = l.x1;
      r.y1 = l.y1;
      r.cover.resize(l.px.size());
      for (size_t i = 0; i < l.px.size(); ++i)
        r.cover[i] = lpx[i].kind || lpx[i].k == portsim::kGfxTextCoverMark ? 1 : 0;  // P9C: a removed glyph is still the layer's
      // P12A fix 1: with the camera slide on, a centred layer's tool preview / tee line pixels move with the course picture: a point
      // over them is a point over the course (PanelRemap::map_impl). Made at every paint, also at offset 0, so the first tick of a
      // new slide (which sets the remap's offset before the next paint) already finds them
      if (smooth_ && l.id == portsim::kGfxLayerCentred) {
        const WorldImage &Wc = world_;
        const bool cm = Wc.cur_mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
        const bool c2m = f.cursor2d.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
        const bool lm = f.line800.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
        if (cm || c2m || lm) {
          r.slides.assign(r.cover.size(), 0);
          const int32_t bw = l.x1 - l.x0;
          for (int32_t y = l.y0; y < l.y1; ++y)
            for (int32_t x = l.x0; x < l.x1; ++x) {
              const size_t i = static_cast<size_t>(y - l.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - l.x0);
              const size_t j = static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x);
              if (r.cover[i] && ((cm && Wc.cur_mask[j]) || (c2m && f.cursor2d[j]) || (lm && f.line800[j]))) r.slides[i] = 1;
            }
        }
      }
      // P9E: each moved popup piece's pixels are a layer of their own at the piece's offset, above the rest of this layer
      std::vector<RemapLayer> moved;
      if (pieces && l.popups) {
        std::vector<int32_t> slot(f.pieces.size(), -1);
        for (size_t k = 0; k < f.pieces.size(); ++k) {
          if (piece_dx_[k] == 0) continue;
          slot[k] = static_cast<int32_t>(moved.size());
          RemapLayer m;
          m.fixed = true;
          m.dx = piece_dx_[k];
          m.x0 = r.x0;
          m.y0 = r.y0;
          m.x1 = r.x1;
          m.y1 = r.y1;
          m.cover.assign(r.cover.size(), 0);
          moved.push_back(std::move(m));
        }
        const int32_t bw = l.x1 - l.x0;
        for (int32_t y = l.y0; y < l.y1; ++y)
          for (int32_t x = l.x0; x < l.x1; ++x) {
            const size_t i = static_cast<size_t>(y - l.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - l.x0);
            if (!r.cover[i]) continue;
            const int32_t k = moved_piece(l, x, y);
            if (k < 0) continue;
            moved[static_cast<size_t>(slot[static_cast<size_t>(k)])].cover[i] = 1;
            r.cover[i] = 0;
          }
      }
      rl.push_back(std::move(r));
      for (RemapLayer &m : moved) rl.push_back(std::move(m));
    }
    remap_->set_layers(anchor, vp, cw, std::move(rl));
  }
}

void Presenter::compose_layers(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp, int32_t slx,
                               int32_t sly) const {
  const ProfScope prof(Phase::Layers);  // P10A
  const bool text = text_now(vp);  // P9C: the layers without the runs' glyphs when the runs are drawn
  const portsim::GfxHudFrame &f = hud_for(text);
  const bool anchor = hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved;
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  const Canvas C = cv_;  // P14A: the canvas (the whole client for paint)
  const int32_t kx0 = std::max(0, C.x0), kx1 = std::min(cw, C.x0 + C.w), ky0 = std::max(0, C.y0), ky1 = std::min(ch, C.y0 + C.h);
  auto px_at = [&](int32_t x, int32_t y) -> uint32_t & {  // P14A: client pixel (x, y) of the canvas
    return out[static_cast<size_t>(y - C.y0) * static_cast<size_t>(C.w) + static_cast<size_t>(x - C.x0)];
  };
  layer_pieces(f, vp, cw);  // P9E (P14A: factored out, the same body)
  const bool pieces = pieces_on(f, vp);
  // the moved piece (index) of a layer pixel, -1 when it stays at the layer's own place
  auto moved_piece = [&](const portsim::GfxHudLayer &l, int32_t x, int32_t y) -> int32_t {
    if (!pieces || !l.popups) return -1;
    const int32_t k = piece_of(f, x, y);
    return k >= 0 && piece_dx_[static_cast<size_t>(k)] != 0 ? k : -1;
  };
  layer_remap(f, vp, cw);  // P9B (P14A: factored out, the same body; not from render_rect)
  const bool nearest = effective_scale(vp.client_w, vp.client_h) == ScaleMode::Nearest;
  const bool opaque_blends = defect_ == PresentDefect::LayerBlendOpaque;
  // P9E fix 4: the tool previews at the cursor (WorldImage::cur_*): drawn at the offset of the layer the last cursor answer was
  // moved by, in the frame's draw order where its copy of them is (the first centred layer holding one of their pixels, else
  // after every layer); that copy is left out of the centred layers (the world under it shows)
  const WorldImage &W = world_;
  const bool cur = W.cur_mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH && W.cur_px.size() == W.px.size() &&
                   W.cur_cov.size() == W.px.size() && W.cur_box[2] > W.cur_box[0];
  const bool c2 = f.cursor2d.size() == static_cast<size_t>(kLogicalW) * kLogicalH;  // the frame's own 2D preview pixels
  // P10E: between game frames the previews follow the cursor's tile (present.h CursorLive): the live answer's client offset
  // replaces the sim's last one, and (shx, shy) client pixels move them by the tile's 800-space step
  const bool live = live_.active && (cur || c2);
  const int32_t cdx = (cur || c2) && anchor && remap_ ? (live ? live_.dx : remap_->cursor_dx()) : 0;
  auto scale_round = [](int32_t v, int32_t n, int32_t d) {
    const int64_t p = static_cast<int64_t>(v) * n;
    return static_cast<int32_t>(p >= 0 ? (p + d / 2) / d : -((-p + d / 2) / d));
  };
  const int32_t shx = live ? scale_round(live_.sx, vp.client_w, kLogicalW) : 0;
  const int32_t shy = live ? scale_round(live_.sy, vp.client_h, kLogicalH) : 0;
  // P12A fix 1: the camera slide moves the previews with the course picture
  const bool c2move = c2 && (cdx != 0 || shx != 0 || shy != 0 || slx != 0 || sly != 0);  // the 2D preview pixels drawn apart (their own pass)
  auto c2_at = [&](int32_t x, int32_t y) {
    return c2move && x >= 0 && y >= 0 && x < kLogicalW && y < kLogicalH &&
           f.cursor2d[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  };
  auto cur_at = [&](int32_t x, int32_t y) {
    return cur && x >= 0 && y >= 0 && x < kLogicalW && y < kLogicalH && W.cur_mask[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  };
  const bool box = W.cur_hi.size() == W.px.size();  // P10F: the placement box's blend pair (cur_cov 2)
  auto draw_cursor = [&]() {  // nearest world pixels, the plane's box only
    if (!cur || vp.client_w <= 0 || vp.client_h <= 0 || W.fw <= 0 || W.ph <= 0) return;
    const int32_t cox = vp.origin_x + cdx + shx + slx, coy = vp.origin_y + shy + sly;  // P10E: (shx, shy) = 0 outside a live move; P12A: the slide
    // (P14A: clipped to the canvas, kx0 .. ky1: the client for paint)
    const int32_t cx0 = std::max<int32_t>(kx0, cox + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[0] - W.ox) * vp.client_w / W.fw) - 1);
    const int32_t cx1 = std::min<int32_t>(kx1, cox + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[2] - W.ox) * vp.client_w / W.fw) + 2);
    const int32_t cy0 = std::max<int32_t>(ky0, coy + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[1]) * vp.client_h / W.ph) - 1);
    const int32_t cy1 = std::min<int32_t>(ky1, coy + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[3]) * vp.client_h / W.ph) + 2);
    for (int32_t y = cy0; y < cy1; ++y) {
      const int64_t dy = static_cast<int64_t>(y - coy) * W.ph;
      const int32_t wy = static_cast<int32_t>(dy >= 0 ? dy / vp.client_h : -((-dy + vp.client_h - 1) / vp.client_h));
      if (wy < 0 || wy >= W.ph) continue;
      uint32_t *dst = &px_at(cx0, y) - cx0;  // P14A: indexed by the client column
      for (int32_t x = cx0; x < cx1; ++x) {
        const int64_t dxw = static_cast<int64_t>(x - cox) * W.fw;
        const int32_t wx = W.ox + static_cast<int32_t>(dxw >= 0 ? dxw / vp.client_w : -((-dxw + vp.client_w - 1) / vp.client_w));
        if (wx < 0 || wx >= W.pw) continue;
        const size_t j = static_cast<size_t>(wy) * static_cast<size_t>(W.pw) + static_cast<size_t>(wx);
        if (W.cur_cov[j] == 1) {
          dst[x] = rgb555_to_bgra(W.cur_px[j]);
        } else if (W.cur_cov[j] == 2 && box) {  // P10F: the box alone, blended over the pixel under it: lo + (hi - lo) * d / 255
          const uint32_t lo = rgb555_to_bgra(W.cur_px[j]), hi = rgb555_to_bgra(W.cur_hi[j]), d = dst[x];
          uint32_t o = 0xff000000u;
          for (int sh = 0; sh < 24; sh += 8) {
            const int32_t l = static_cast<int32_t>((lo >> sh) & 0xffu), h = static_cast<int32_t>((hi >> sh) & 0xffu);
            const int32_t v = l + ((h - l) * static_cast<int32_t>((d >> sh) & 0xffu) + 127) / 255;
            o |= static_cast<uint32_t>(std::clamp(v, 0, 255)) << sh;
          }
          dst[x] = o;
        }
      }
    }
  };
  // P10I fix 1: the frame's tee lines (GfxHudFrame::lines) at the window's resolution. Their 800-space pixels (line800, where the
  // layer pixel still holds the word the line left: the yards label drawn over it keeps its own) are left out of the centred
  // layers, and each line is drawn again as Terrain::drawLine draws it (terrain_overlay.h): glColor4f(((c >> 7) & 0xf8) / 255,
  // ((c >> 2) & 0xf8) / 255, ((c << 3) & 0xf8) / 255, alpha / 10), the GL width clamped to 10 scaled to client pixels, coverage
  // clamp(w / 2 + 0.5 - |n|) * clamp(t + 0.5) * clamp(L - t + 0.5) at each pixel centre, blended over the client pixel. The first
  // end (the tee, or the green for the tee tool) stays; the second (the cursor tile's screen point, P7's F+0x68 / F+0x58) moves
  // with the cursor previews (cdx + shx, shy). Drawn where the frame draws it: after the cursor previews (P7 draws the box and the
  // green / pin before the line), before the layer that holds its pixels (and so under the yards label that layer's runs redraw).
  const bool lines = line_native_ && !f.lines.empty() && f.line800.size() == static_cast<size_t>(kLogicalW) * kLogicalH &&
                     f.line_word.size() == f.line800.size();
  auto line_at = [&](int32_t x, int32_t y, const portsim::GfxLayerPx &p) {
    if (!lines || x < 0 || y < 0 || x >= kLogicalW || y >= kLogicalH) return false;
    const size_t i = static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x);
    return f.line800[i] != 0 && p.kind == 1 && p.px == f.line_word[i];
  };
  // P14A: the previews' and the lines' places in the draw order, once per frame / content (draw_order: the same searches)
  size_t cur_layer = f.layers.size(), line_layer = f.layers.size();
  draw_order(f, cur, lines, cur_layer, line_layer);
  auto draw_lines = [&]() {
    if (!lines || vp.client_w <= 0 || vp.client_h <= 0) return;
    const double sx = static_cast<double>(vp.client_w) / kLogicalW, sy = static_cast<double>(vp.client_h) / kLogicalH;
    for (const portsim::GfxHudLine &ln : f.lines) {
      const double cr = static_cast<double>((ln.colour >> 7) & 0xf8u), cg = static_cast<double>((ln.colour >> 2) & 0xf8u),
                   cb = static_cast<double>((ln.colour << 3) & 0xf8u);
      const double ca = std::clamp(static_cast<double>(ln.alpha) / 10.0, 0.0, 1.0);
      if (ca <= 0.0) continue;
      const double half = std::min(static_cast<double>(ln.width), 10.0) * (sx + sy) / 4.0;
      const double ax = vp.origin_x + slx + ln.x1 * sx, ay = vp.origin_y + sly + ln.y1 * sy;  // P12A: + the slide
      const double bx = vp.origin_x + cdx + shx + slx + ln.x2 * sx, by = vp.origin_y + shy + sly + ln.y2 * sy;
      const double dx = bx - ax, dy = by - ay, len = std::sqrt(dx * dx + dy * dy);
      if (!(len > 0.0)) continue;
      const double ux = dx / len, uy = dy / len, reach = half + 1.5;
      // (P14A: clipped to the canvas)
      const int32_t x0 = std::max<int32_t>(kx0, static_cast<int32_t>(std::floor(std::min(ax, bx) - reach)));
      const int32_t x1 = std::min<int32_t>(kx1 - 1, static_cast<int32_t>(std::ceil(std::max(ax, bx) + reach)));
      const int32_t y0 = std::max<int32_t>(ky0, static_cast<int32_t>(std::floor(std::min(ay, by) - reach)));
      const int32_t y1 = std::min<int32_t>(ky1 - 1, static_cast<int32_t>(std::ceil(std::max(ay, by) + reach)));
      if (x1 < x0) continue;
      for (int32_t y = y0; y <= y1; ++y) {
        const double py = y + 0.5 - ay;
        int32_t xa = x0, xb = x1;
        if (std::fabs(uy) > 1e-9) {  // the columns whose centre lies within reach of the line: n = -px * uy + py * ux
          const double c0 = (py * ux - reach) / uy, c1 = (py * ux + reach) / uy;
          xa = std::max(xa, static_cast<int32_t>(std::floor(std::min(c0, c1) + ax - 0.5)));
          xb = std::min(xb, static_cast<int32_t>(std::ceil(std::max(c0, c1) + ax - 0.5)));
        }
        xa = std::max(xa, x0);  // P14A: the canvas (x0 .. x1 were clipped to it; xa / xb follow)
        xb = std::min(xb, x1);
        if (xb < xa) continue;
        uint32_t *dst = &px_at(xa, y) - xa;  // P14A: indexed by the client column
        for (int32_t x = xa; x <= xb; ++x) {
          const double px = x + 0.5 - ax;
          const double t = px * ux + py * uy, n = -px * uy + py * ux;
          const double cov = std::clamp(half + 0.5 - std::fabs(n), 0.0, 1.0) * std::clamp(t + 0.5, 0.0, 1.0) *
                             std::clamp(len - t + 0.5, 0.0, 1.0);
          if (cov <= 0.0) continue;
          const double al = ca * cov;
          const uint32_t d = dst[x];
          const double dr = (d >> 16) & 0xffu, dg = (d >> 8) & 0xffu, db = d & 0xffu;
          const uint32_t r8 = static_cast<uint32_t>(std::lround(cr * al + dr * (1.0 - al)));
          const uint32_t g8 = static_cast<uint32_t>(std::lround(cg * al + dg * (1.0 - al)));
          const uint32_t b8 = static_cast<uint32_t>(std::lround(cb * al + db * (1.0 - al)));
          dst[x] = 0xff000000u | (std::min(r8, 255u) << 16) | (std::min(g8, 255u) << 8) | std::min(b8, 255u);
        }
      }
    }
  };
  auto to555 = [](uint32_t c) -> uint16_t {
    return static_cast<uint16_t>((((c >> 19) & 31) << 10) | (((c >> 11) & 31) << 5) | ((c >> 3) & 31));
  };
  auto over = [&](const portsim::GfxLayerPx &p, uint32_t d) -> uint32_t {  // one layer pixel composed over client pixel d
    if (p.kind == 1 || opaque_blends) return rgb555_to_bgra(p.px);
    const uint16_t w = to555(d);
    if (p.kind == 2) {  // jgld blitPair1 (gfx.cpp b_stretch_blit): ((dst8 * k) >> 8) + src8, >> 3, per channel, words as the original
      const uint32_t k = p.k, sw = p.src;
      auto chn = [&](uint32_t dv, uint32_t sv) { return static_cast<uint16_t>(static_cast<uint16_t>(((dv * k) >> 8) + sv) >> 3); };
      const uint16_t rr = static_cast<uint16_t>(chn((w >> 7) & 0xf8u, (sw >> 7) & 0xf8u) << 10);
      const uint16_t gg = static_cast<uint16_t>(chn((w >> 2) & 0xf8u, (sw >> 2) & 0xf8u) << 5);
      const uint16_t bb = chn((static_cast<uint32_t>(w) << 3) & 0xf8u, (static_cast<uint32_t>(sw) << 3) & 0xf8u);
      return rgb555_to_bgra(static_cast<uint16_t>((rr | gg | bb) & 0x7fff));
    }
    if (p.kind == 4) return rgb555_to_bgra(static_cast<uint16_t>(portsim::gfx_layer_quarter(p.src, w) & 0x7fff));  // P9D: drawAlpha's quarter blend
    size_t n = 0;
    const uint16_t *t = portsim::gfx_shade_table(p.k, n);
    if (!t || w >= n) return rgb555_to_bgra(p.px);
    return rgb555_to_bgra(static_cast<uint16_t>(t[w] & 0x7fff));
  };
  auto mix = [](uint32_t a, uint32_t b, uint32_t fr) {
    const uint32_t g = 256 - fr;
    const uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * fr) >> 8) & 0x00ff00ffu;
    const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * fr) >> 8) & 0x00ff00ffu;
    return rb | (ag << 8);
  };
  // the 800-space position of client column / row k: nearest = floor((k - origin) * n / client) * 256, smooth = the pixel centre
  // in 1/256, ((k - origin + 0.5) * n / client - 0.5) * 256 (may be negative near the frame's edge)
  auto lpos = [nearest](int32_t k, int32_t origin, int32_t n_client, int32_t n_log) -> int64_t {
    const int64_t d = static_cast<int64_t>(k) - origin;
    if (nearest) {
      const int64_t num = d * n_log;
      return (num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client)) * 256;
    }
    const int64_t num = (2 * d + 1) * n_log * 128;
    return (num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client)) - 128;
  };
  auto floor256 = [](int64_t v) -> int32_t { return static_cast<int32_t>(v >= 0 ? v / 256 : -((-v + 255) / 256)); };
  // (the base / world runs, -3 and -1, are drawn by render before this call since P12A)
  for (size_t li = 0; li < f.layers.size(); ++li) {
    const portsim::GfxHudLayer &l = f.layers[li];
    if (li == cur_layer) draw_cursor();  // P9E fix 4
    if (li == line_layer) draw_lines();  // P10I fix 1
    const int32_t dx0 = anchor ? layer_dx(l, vp, dxl, dxr) : 0;
    struct RunsAfter {  // P9C: the layer's runs once its pixels are composed (also when the layer has no box)
      const Presenter &p;
      std::vector<uint32_t> &out;
      int32_t cw, ch;
      const Viewport &vp;
      bool on;
      int32_t li, dx;
      ~RunsAfter() {
        if (on) p.draw_runs(out, cw, ch, vp, true, li, dx);
      }
    } runs_after{*this, out, cw, ch, vp, text, static_cast<int32_t>(li), dx0};
    // P9D: a whole-frame dim (the report screens' black quarter blend over every root pixel): everything composed so far is dimmed
    // in the whole window, as the original dims its whole frame (the panels at their anchors, the world, the side bands); the
    // layer's own dim pixels (kind 4) are that dim, already applied
    const bool wdim = l.dim != 0 && defect_ != PresentDefect::DimLocal && !opaque_blends;
    if (wdim)  // (P14A: the canvas's pixels)
      for (uint32_t &d : out) d = rgb555_to_bgra(static_cast<uint16_t>(portsim::gfx_layer_quarter(l.dim_src, to555(d)) & 0x7fff)) | 0xff000000u;
    if (l.x1 <= l.x0 || l.y1 <= l.y0) continue;
    // P9E: pass -1 = the layer's pixels at its own place (without its moved popup pieces), then each moved piece at its offset
    const bool split = pieces && l.popups;
    // P9E fix 4: a last pass (cpass) draws a centred layer's 2D preview pixels (f.cursor2d) at the cursor's offset
    const int32_t npass = split ? static_cast<int32_t>(f.pieces.size()) : 0;
    const bool cmove = c2move && l.id == portsim::kGfxLayerCentred;  // P10E: also when only the live move shifts them
    const int32_t cpass = cmove ? npass : -2;
    for (int32_t pass = -1; pass < npass + (cmove ? 1 : 0); ++pass) {
    if (pass >= 0 && pass != cpass && piece_dx_[static_cast<size_t>(pass)] == 0) continue;
    const int32_t dx = pass == cpass ? cdx + shx + slx : pass >= 0 ? piece_dx_[static_cast<size_t>(pass)] : dx0;  // P12A: slx
    const int32_t dyp = pass == cpass ? shy + sly : 0;  // P10E: the live move's rows; P12A: the slide's
    auto tap = [&](int32_t x, int32_t y) -> const portsim::GfxLayerPx * {
      const portsim::GfxLayerPx *p = l.at(x, y);
      if (p && split && moved_piece(l, x, y) != (pass == cpass ? -1 : pass)) return nullptr;
      if (p && l.id == portsim::kGfxLayerCentred && cur_at(x, y)) return nullptr;  // P9E fix 4: drawn as the cursor's previews
      if (p && l.id == portsim::kGfxLayerCentred && line_at(x, y, *p)) return nullptr;  // P10I fix 1: drawn by draw_lines
      if (p && cmove && (c2_at(x, y) != (pass == cpass))) return nullptr;  // P9E fix 4: the 2D preview only in its own pass
      return p && wdim && p->kind == 4 ? nullptr : p;
    };
    // the client box that can show the layer (one logical pixel of margin for the smooth taps)
    // (P14A: clipped to the canvas, kx0 .. ky1: the client for paint)
    const int32_t cx0 = std::max<int32_t>(kx0, vp.origin_x + dx + static_cast<int32_t>(static_cast<int64_t>(l.x0 - 1) * vp.client_w / kLogicalW) - 1);
    const int32_t cx1 = std::min<int32_t>(kx1, vp.origin_x + dx + static_cast<int32_t>(static_cast<int64_t>(l.x1 + 1) * vp.client_w / kLogicalW) + 1);
    const int32_t cy0 = std::max<int32_t>(ky0, vp.origin_y + dyp + static_cast<int32_t>(static_cast<int64_t>(l.y0 - 1) * vp.client_h / kLogicalH) - 1);
    const int32_t cy1 = std::min<int32_t>(ky1, vp.origin_y + dyp + static_cast<int32_t>(static_cast<int64_t>(l.y1 + 1) * vp.client_h / kLogicalH) + 1);
    if (cx1 <= cx0) continue;
    // P10A: each column's 800-space position (lpos, floor256) once per pass instead of per pixel
    std::vector<int32_t> &cl = cl_tab_;
    cl.resize(static_cast<size_t>(std::max<int32_t>(0, cx1 - cx0)) * 2);
    for (int32_t x = cx0; x < cx1; ++x) {
      const int64_t sx = lpos(x - dx, vp.origin_x, vp.client_w, kLogicalW);
      const int32_t lx0 = floor256(sx);
      cl[static_cast<size_t>(x - cx0) * 2] = lx0;
      cl[static_cast<size_t>(x - cx0) * 2 + 1] = static_cast<int32_t>(sx - static_cast<int64_t>(lx0) * 256);
    }
    RowPool::get().run(cy0, cy1, [&](int32_t ya, int32_t yb) {  // P10A: rows over the paint threads
      for (int32_t y = ya; y < yb; ++y) {
        const int64_t sy = lpos(y - dyp, vp.origin_y, vp.client_h, kLogicalH);
        const int32_t ly0 = floor256(sy);
        const uint32_t fy = static_cast<uint32_t>(sy - static_cast<int64_t>(ly0) * 256);
        uint32_t *dst = &px_at(cx0, y) - cx0;  // P14A: indexed by the client column
        for (int32_t x = cx0; x < cx1; ++x) {
          const int32_t lx0 = cl[static_cast<size_t>(x - cx0) * 2];
          if (nearest) {
            if (const portsim::GfxLayerPx *p = tap(lx0, ly0)) dst[x] = over(*p, dst[x]) | 0xff000000u;
            continue;
          }
          const uint32_t fx = static_cast<uint32_t>(cl[static_cast<size_t>(x - cx0) * 2 + 1]);
          const portsim::GfxLayerPx *p00 = tap(lx0, ly0), *p10 = tap(lx0 + 1, ly0), *p01 = tap(lx0, ly0 + 1),
                                    *p11 = tap(lx0 + 1, ly0 + 1);
          if (!p00 && !p10 && !p01 && !p11) continue;
          const uint32_t d = dst[x];
          const uint32_t c00 = p00 ? over(*p00, d) : d, c10 = p10 ? over(*p10, d) : d, c01 = p01 ? over(*p01, d) : d,
                         c11 = p11 ? over(*p11, d) : d;
          dst[x] = mix(mix(c00, c10, fx), mix(c01, c11, fx), fy) | 0xff000000u;
        }
      }
    });
    }  // P9E: the pass loop
  }
  if (cur_layer == f.layers.size()) draw_cursor();  // P9E fix 4: after every layer
  if (line_layer == f.layers.size()) draw_lines();  // P10I fix 1
}

// P7 (present.h WideFill): the side bands from the content rectangle's own edge columns, in client pixels. The content is
// already drawn, so the bands follow the scaled image exactly (nearest or smooth) and the content's pixels are not touched.
void Presenter::fill_bands(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  const ProfScope prof(Phase::Bands);  // P10A
  const int32_t ox = vp.origin_x, oy = vp.origin_y, w = vp.client_w, h = vp.client_h;
  if (w <= 0 || h <= 0 || cw <= 0 || ch <= 0 || oy + h > ch || ox + w > cw) return;
  // P13B item 18: the world map's hint repeats its edge column whatever --wide-fill says (fill_bands_map)
  const bool map = band_ == BandFill::ExtendClamp;
  if (map) {
    fill_bands_map(out, cw, vp);
    return;
  }
  if (wide_ == WideFill::Edge || wide_ == WideFill::Mirror) {
    fill_bands_p9(out, cw, ch, vp);
    return;
  }
  const bool clamp = wide_ == WideFill::Clamp;
  // client columns averaged at each edge (the frame's outer 4 logical columns) and the largest blur radius (48 logical rows)
  const int32_t k = clamp ? 1 : std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(w) * 4 / kLogicalW));
  const int32_t rmax = clamp ? 0 : std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(h) * 48 / kLogicalH));
  std::vector<uint64_t> pre[3];
  for (auto &v : pre) v.assign(static_cast<size_t>(h) + 1, 0);
  // P14A fix 3: client pixel (x, Y) of the canvas (the whole client for paint); a side is drawn only when the canvas holds its
  // band and `need` content columns beside it, and every content row
  auto P = [&](int32_t x, int32_t Y) -> uint32_t & {
    return out[static_cast<size_t>(Y - cv_.y0) * static_cast<size_t>(cv_.w) + static_cast<size_t>(x - cv_.x0)];
  };
  auto side_in = [&](int side, int32_t need) {
    if (cv_.y0 > oy || cv_.y0 + cv_.h < oy + h) return false;
    return side == 0 ? cv_.x0 <= 0 && cv_.x0 + cv_.w >= std::min(ox + w, ox + need)
                     : cv_.x0 <= std::max(ox, ox + w - need) && cv_.x0 + cv_.w >= cw;
  };
  for (int side = 0; side < 2; ++side) {
    const int32_t band = side == 0 ? ox : cw - (ox + w);
    if (band <= 0 || !side_in(side, k)) continue;
    // per row: the mean of the k edge columns (channels scaled by 256 to keep the fraction), as prefix sums over the rows
    for (int32_t y = 0; y < h; ++y) {
      uint32_t s[3] = {0, 0, 0};
      for (int32_t j = 0; j < k; ++j) {
        const uint32_t v = P(side == 0 ? ox + j : ox + w - 1 - j, oy + y);
        s[0] += (v >> 16) & 255;
        s[1] += (v >> 8) & 255;
        s[2] += v & 255;
      }
      for (int c = 0; c < 3; ++c)
        pre[c][static_cast<size_t>(y) + 1] = pre[c][static_cast<size_t>(y)] + (static_cast<uint64_t>(s[c]) * 256 + k / 2) / k;
    }
    for (int32_t d = 1; d <= band; ++d) {
      const int32_t r = std::min(d / 2, rmax);
      const int32_t x = side == 0 ? ox - d : ox + w - 1 + d;
      for (int32_t y = 0; y < h; ++y) {
        const int32_t lo = std::max(0, y - r), hi = std::min(h - 1, y + r);
        const uint64_t n = static_cast<uint64_t>(hi - lo + 1) * 256;
        uint32_t c8[3];
        for (int c = 0; c < 3; ++c) {
          const uint64_t sum = pre[c][static_cast<size_t>(hi) + 1] - pre[c][static_cast<size_t>(lo)];
          c8[c] = static_cast<uint32_t>((sum + n / 2) / n);
        }
        P(x, oy + y) = 0xff000000u | (c8[0] << 16) | (c8[1] << 8) | c8[2];
      }
    }
  }
}

// P13B item 18 (BandFill::ExtendClamp, the world map): every band column repeats the content's outermost client column, cleaned
// per row by a vertical median over +-33 logical rows (it drops what crosses the edge for fewer rows than that: the site plates'
// outlines and the swirl's 1-pixel line, which a plain clamp carries out as streaks) and smoothed by a box mean over +-2 logical
// rows. The art's own vertical gradient (the right edge) and flat colour (the left edge) are kept.
void Presenter::fill_bands_map(std::vector<uint32_t> &out, int32_t cw, const Viewport &vp) const {
  const int32_t ox = vp.origin_x, oy = vp.origin_y, w = vp.client_w, h = vp.client_h;
  const int32_t rm = std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(h) * 33 / kLogicalH));
  const int32_t rb = std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(h) * 2 / kLogicalH));
  std::vector<uint8_t> sig(static_cast<size_t>(h) * 3), med(static_cast<size_t>(h) * 3), win;
  std::vector<uint32_t> col(static_cast<size_t>(h));
  // P14A fix 3: client pixel (x, Y) of the canvas (the whole client for paint); a side is drawn only when the canvas holds its
  // band and `need` content columns beside it, and every content row
  auto P = [&](int32_t x, int32_t Y) -> uint32_t & {
    return out[static_cast<size_t>(Y - cv_.y0) * static_cast<size_t>(cv_.w) + static_cast<size_t>(x - cv_.x0)];
  };
  auto side_in = [&](int side, int32_t need) {
    if (cv_.y0 > oy || cv_.y0 + cv_.h < oy + h) return false;
    return side == 0 ? cv_.x0 <= 0 && cv_.x0 + cv_.w >= std::min(ox + w, ox + need)
                     : cv_.x0 <= std::max(ox, ox + w - need) && cv_.x0 + cv_.w >= cw;
  };
  for (int side = 0; side < 2; ++side) {
    const int32_t band = side == 0 ? ox : cw - (ox + w);
    if (band <= 0 || !side_in(side, 1)) continue;
    const int32_t cx = side == 0 ? ox : ox + w - 1;
    for (int32_t y = 0; y < h; ++y) {
      const uint32_t v = P(cx, oy + y);
      for (int c = 0; c < 3; ++c) sig[static_cast<size_t>(y) * 3 + c] = static_cast<uint8_t>((v >> (16 - 8 * c)) & 255);
    }
    for (int32_t y = 0; y < h; ++y) {
      const int32_t lo = std::max(0, y - rm), hi = std::min(h - 1, y + rm);
      for (int c = 0; c < 3; ++c) {
        win.clear();
        for (int32_t j = lo; j <= hi; ++j) win.push_back(sig[static_cast<size_t>(j) * 3 + c]);
        std::nth_element(win.begin(), win.begin() + static_cast<ptrdiff_t>(win.size() / 2), win.end());
        med[static_cast<size_t>(y) * 3 + c] = win[win.size() / 2];
      }
    }
    for (int32_t y = 0; y < h; ++y) {
      const int32_t lo = std::max(0, y - rb), hi = std::min(h - 1, y + rb);
      uint32_t s[3] = {0, 0, 0};
      for (int32_t j = lo; j <= hi; ++j)
        for (int c = 0; c < 3; ++c) s[c] += med[static_cast<size_t>(j) * 3 + c];
      const uint32_t n = static_cast<uint32_t>(hi - lo + 1);
      col[static_cast<size_t>(y)] = 0xff000000u | (((s[0] + n / 2) / n) << 16) | (((s[1] + n / 2) / n) << 8) | ((s[2] + n / 2) / n);
    }
    for (int32_t y = 0; y < h; ++y) {
      uint32_t *row = &P(side == 0 ? 0 : ox + w, oy + y);
      std::fill(row, row + (side == 0 ? ox : cw - ox - w), col[static_cast<size_t>(y)]);
    }
  }
}

// P9A item 5 (present.h WideFill::Edge / Mirror). Channels are 8-bit client colours (the content is already drawn and scaled).
void Presenter::fill_bands_p9(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  (void)ch;  // fill_bands checked the content rectangle against the client
  const int32_t ox = vp.origin_x, oy = vp.origin_y, w = vp.client_w, h = vp.client_h;
  auto lg = [](int32_t logical, int32_t span, int32_t of) {  // logical px -> client px, at least 1
    return std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(span) * logical / of));
  };
  // P14A fix 3: client pixel (x, Y) of the canvas (the whole client for paint); a side is drawn only when the canvas holds its
  // band and `need` content columns beside it, and every content row
  auto P = [&](int32_t x, int32_t Y) -> uint32_t & {
    return out[static_cast<size_t>(Y - cv_.y0) * static_cast<size_t>(cv_.w) + static_cast<size_t>(x - cv_.x0)];
  };
  auto side_in = [&](int side, int32_t need) {
    if (cv_.y0 > oy || cv_.y0 + cv_.h < oy + h) return false;
    return side == 0 ? cv_.x0 <= 0 && cv_.x0 + cv_.w >= std::min(ox + w, ox + need)
                     : cv_.x0 <= std::max(ox, ox + w - need) && cv_.x0 + cv_.w >= cw;
  };
  auto at = [&](int32_t x, int32_t y) -> uint32_t & { return P(x, oy + y); };
  auto chan = [](uint32_t v, int c) { return static_cast<int32_t>((v >> (16 - 8 * c)) & 255); };
  auto pack = [](const int32_t *c) {
    return 0xff000000u | (static_cast<uint32_t>(c[0]) << 16) | (static_cast<uint32_t>(c[1]) << 8) | static_cast<uint32_t>(c[2]);
  };
  // a vertical box blur of a per-row signal (3 channels, scaled by 256): prefix sums, then the mean over [y - r, y + r]
  auto prefix = [&](const std::vector<int64_t> &sig, std::vector<int64_t> &pre) {
    pre.assign(static_cast<size_t>(h) * 3 + 3, 0);
    for (int32_t y = 0; y < h; ++y)
      for (int c = 0; c < 3; ++c)
        pre[(static_cast<size_t>(y) + 1) * 3 + c] = pre[static_cast<size_t>(y) * 3 + c] + sig[static_cast<size_t>(y) * 3 + c];
  };
  auto mean = [&](const std::vector<int64_t> &pre, int32_t y, int32_t r, int c) {
    const int32_t lo = std::max(0, y - r), hi = std::min(h - 1, y + r);
    const int64_t n = static_cast<int64_t>(hi - lo + 1) * 256;
    return static_cast<int32_t>((pre[(static_cast<size_t>(hi) + 1) * 3 + c] - pre[static_cast<size_t>(lo) * 3 + c] + n / 2) / n);
  };
  const int32_t k_edge = lg(4, w, kLogicalW), k_bg = lg(64, w, kLogicalW), fade = lg(24, w, kLogicalW);
  const int32_t r_edge_max = lg(48, h, kLogicalH), r_bg = lg(150, h, kLogicalH);
  std::vector<int64_t> edge_sig(static_cast<size_t>(h) * 3), bg_sig(static_cast<size_t>(h) * 3), edge_pre, bg_pre;
  for (int side = 0; side < 2; ++side) {
    const int32_t band = side == 0 ? ox : cw - (ox + w);
    if (band <= 0 || !side_in(side, std::max(k_bg, band))) continue;
    auto content_x = [&](int32_t j) { return side == 0 ? ox + j : ox + w - 1 - j; };  // the j-th content column from the edge
    if (wide_ == WideFill::Mirror) {
      for (int32_t d = 1; d <= band; ++d) {  // band column d shows content column d - 1, blurred by d / 3, darkened outwards
        const int32_t j = std::min(d - 1, w - 1), r = d / 3;
        for (int32_t y = 0; y < h; ++y) {
          const uint32_t v = at(content_x(j), y);
          for (int c = 0; c < 3; ++c) edge_sig[static_cast<size_t>(y) * 3 + c] = static_cast<int64_t>(chan(v, c)) * 256;
        }
        prefix(edge_sig, edge_pre);
        const int32_t dark = 256 - static_cast<int32_t>(static_cast<int64_t>(154) * d / band);  // 256 .. 102 (40 %)
        const int32_t x = side == 0 ? ox - d : ox + w - 1 + d;
        for (int32_t y = 0; y < h; ++y) {
          int32_t c8[3];
          for (int c = 0; c < 3; ++c) c8[c] = mean(edge_pre, y, r, c) * dark / 256;
          at(x, y) = pack(c8);
        }
      }
      continue;
    }
    // (P14A fix 3: both row loops over the paint threads; each row's values are its own)
    RowPool::get().run(0, h, [&](int32_t ya, int32_t yb) {
      std::vector<int32_t> colr(static_cast<size_t>(k_bg));
      for (int32_t y = ya; y < yb; ++y) {
        int64_t s[3] = {0, 0, 0};
        for (int32_t j = 0; j < k_edge; ++j)
          for (int c = 0; c < 3; ++c) s[c] += chan(at(content_x(j), y), c);
        for (int c = 0; c < 3; ++c) {
          edge_sig[static_cast<size_t>(y) * 3 + c] = (s[c] * 256 + k_edge / 2) / k_edge;
          for (int32_t j = 0; j < k_bg; ++j) colr[static_cast<size_t>(j)] = chan(at(content_x(j), y), c);
          std::nth_element(colr.begin(), colr.begin() + k_bg / 2, colr.end());
          bg_sig[static_cast<size_t>(y) * 3 + c] = static_cast<int64_t>(colr[static_cast<size_t>(k_bg / 2)]) * 256;
        }
      }
    });
    prefix(edge_sig, edge_pre);
    prefix(bg_sig, bg_pre);
    // the band columns' weights and radii (per column d), then the rows
    std::vector<int32_t> wbs(static_cast<size_t>(band) + 1), rs(static_cast<size_t>(band) + 1);
    for (int32_t d = 1; d <= band; ++d) {
      // the background's smoothstep weight: 0 at the frame's edge, 256 from `fade` client columns on
      const int64_t t = std::min<int64_t>(256, static_cast<int64_t>(d) * 256 / fade);
      wbs[static_cast<size_t>(d)] = static_cast<int32_t>(t * t * (3 * 256 - 2 * t) / (256 * 256));
      rs[static_cast<size_t>(d)] = std::min(d / 2, r_edge_max);
    }
    RowPool::get().run(0, h, [&](int32_t ya, int32_t yb) {
      for (int32_t y = ya; y < yb; ++y) {
        int32_t bg[3];
        for (int c = 0; c < 3; ++c) bg[c] = mean(bg_pre, y, r_bg, c);
        for (int32_t d = 1; d <= band; ++d) {
          const int32_t x = side == 0 ? ox - d : ox + w - 1 + d;
          const int32_t wb = wbs[static_cast<size_t>(d)], r = rs[static_cast<size_t>(d)];
          int32_t c8[3];
          for (int c = 0; c < 3; ++c) c8[c] = wb >= 256 ? bg[c] : (mean(edge_pre, y, r, c) * (256 - wb) + bg[c] * wb + 128) / 256;
          at(x, y) = pack(c8);
        }
      }
    });
  }
}

// P10A: the FPS overlay (present.h set_overlay). Each line of overlay_text is a text run of the game's font (Profiler::font: the
// face and style of gameMain's font call 3) at 800-space size 12 (the fps) or 9 (the phases), drawn by the P9C text path
// (TextRaster) right-aligned at the window's right edge (the run's box at the 800 frame's right edge, moved by the right band's
// width), the lines stacked up from the bottom, over the client pixels darkened to half.
void Presenter::draw_overlay(std::vector<uint32_t> &out, int32_t cw, int32_t ch) const {
  draw_overlay_rect(out, cw, ch, 0, 0, cw, ch);
}

// P14A: the overlay's layout (draw_overlay's body up to the drawing, factored out): the runs and the darkened box (bx0, by0, by1;
// the box reaches the window's right edge)
namespace {
struct OverlayLayout {
  std::vector<portsim::GfxTextRun> runs;
  int32_t bx0 = 0, by0 = 0, by1 = 0, dxr = 0;
  Viewport vp;
};
}  // namespace

static bool overlay_layout(int mode, int32_t cw, int32_t ch, OverlayLayout &L, const ProfLine *frozen) {
#ifdef _WIN32
  if (!mode || cw <= 0 || ch <= 0) return false;
  std::string face;
  uint32_t style = 0;
  if (!Profiler::get().font(face, style)) return false;  // before the game made its fonts
  const Viewport vp = fit_viewport(cw, ch);
  if (vp.client_w <= 0 || vp.client_h <= 0) return false;
  const ProfLine pl = frozen ? *frozen : Profiler::get().last();  // P14A: one paint shows one line (Presenter::overlay_freeze)
  const std::string text = overlay_text(pl, mode);
  std::vector<std::string> lines;
  for (size_t a = 0;;) {
    const size_t b = text.find('\n', a);
    lines.push_back(text.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  const int32_t size = mode == 1 ? 12 : 9;
  const int32_t height = std::max<int32_t>(1, static_cast<int32_t>(std::lround(size * static_cast<double>(vp.client_h) / kLogicalH)));
  const int32_t dxr = cw - vp.origin_x - vp.client_w;  // the right band's width: the 800 frame's right edge -> the window's
  // the lines' widths in client pixels, with TextRaster's LOGFONTA (grey antialiasing)
  std::vector<int32_t> wpx(lines.size(), 0);
  {
    HDC dc = CreateCompatibleDC(nullptr);
    LOGFONTA lf;
    std::memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -height;
    lf.lfWeight = (style & 1u) ? 700 : 0;
    lf.lfItalic = (style & 2u) ? 1 : 0;
    lf.lfUnderline = (style & 4u) ? 1 : 0;
    lf.lfOutPrecision = 7;  // OUT_TT_ONLY_PRECIS
    lf.lfQuality = 4;       // ANTIALIASED_QUALITY (TextAa::Grey)
    std::memcpy(lf.lfFaceName, face.data(), std::min(face.size(), sizeof(lf.lfFaceName) - 1));
    HFONT f = CreateFontIndirectA(&lf);
    if (dc && f) {
      HGDIOBJ old = SelectObject(dc, f);
      for (size_t i = 0; i < lines.size(); ++i) {
        SIZE sz = {0, 0};
        if (GetTextExtentPoint32A(dc, lines[i].data(), static_cast<int>(lines[i].size()), &sz)) wpx[i] = sz.cx;
      }
      SelectObject(dc, old);
    }
    if (f) DeleteObject(f);
    if (dc) DeleteDC(dc);
  }
  const int32_t step = size + 3, n = static_cast<int32_t>(lines.size());
  L.runs.clear();
  int32_t bx0 = cw;  // the darkened box's left edge, client pixels
  for (int32_t i = 0; i < n; ++i) {
    portsim::GfxTextRun r;
    r.face = face;
    r.size = size;
    r.style = style;
    r.colorref = 0x00a0f0ffu;  // r 255, g 240, b 160
    const int64_t wl = static_cast<int64_t>(wpx[static_cast<size_t>(i)]);
    const int32_t w800 = static_cast<int32_t>((wl * kLogicalW + vp.client_w - 1) / vp.client_w);
    r.x1 = kLogicalW - 2;
    r.x0 = std::max<int32_t>(0, r.x1 - w800 - 3);
    r.x = r.x0 + 1;
    r.y = kLogicalH - 4 - (n - 1 - i) * step;
    r.y0 = r.y - size - 2;
    r.y1 = std::min<int32_t>(kLogicalH, r.y + size / 3 + 2);
    r.text = lines[static_cast<size_t>(i)];
    r.vis.assign(static_cast<size_t>(r.x1 - r.x0) * static_cast<size_t>(r.y1 - r.y0), 1);
    bx0 = std::min<int32_t>(bx0, vp.origin_x + dxr + static_cast<int32_t>(static_cast<int64_t>(r.x0) * vp.client_w / kLogicalW));
    L.runs.push_back(std::move(r));
  }
  if (L.runs.empty()) return false;
  L.bx0 = bx0;
  L.by0 = std::max<int32_t>(0, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(L.runs.front().y0) * vp.client_h / kLogicalH));
  L.by1 = std::min<int32_t>(ch, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(L.runs.back().y1) * vp.client_h / kLogicalH));
  L.dxr = dxr;
  L.vp = vp;
  return true;
#else
  (void)mode; (void)cw; (void)ch; (void)L; (void)frozen;
  return false;
#endif
}

bool Presenter::overlay_box(int32_t cw, int32_t ch, int32_t box[4]) const {  // P14A
  OverlayLayout L;
  if (!overlay_layout(overlay_mode_, cw, ch, L, overlay_frozen_ ? &overlay_line_ : nullptr)) return false;
  box[0] = std::max<int32_t>(0, L.bx0 - 2);
  box[1] = L.by0;
  box[2] = cw;
  box[3] = L.by1;
  return box[2] > box[0] && box[3] > box[1];
}

bool Presenter::overlay_runs(int32_t cw, int32_t ch, std::vector<portsim::GfxTextRun> &runs, Viewport &vp, int32_t &dxr,
                             int32_t box[4]) const {  // P15B
  OverlayLayout L;
  if (!overlay_layout(overlay_mode_, cw, ch, L, overlay_frozen_ ? &overlay_line_ : nullptr)) return false;
  box[0] = std::max<int32_t>(0, L.bx0 - 2);
  box[1] = L.by0;
  box[2] = cw;
  box[3] = L.by1;
  runs = std::move(L.runs);
  vp = L.vp;
  dxr = L.dxr;
  return box[2] > box[0] && box[3] > box[1];
}

// the overlay over the canvas (x0, y0, w, h) of the client: the box darkened to half, then the runs (P14A: draw_overlay's drawing
// on a canvas; the whole client = draw_overlay)
void Presenter::draw_overlay_rect(std::vector<uint32_t> &out, int32_t cw, int32_t ch, int32_t x0, int32_t y0, int32_t w,
                                  int32_t h) const {
#ifdef _WIN32
  OverlayLayout L;
  if (!overlay_layout(overlay_mode_, cw, ch, L, overlay_frozen_ ? &overlay_line_ : nullptr)) return;
  if (out.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) return;
  if (!overlay_raster_) overlay_raster_.reset(new TextRaster());
  overlay_raster_->set_aa(TextAa::Grey);
  for (int32_t y = std::max(L.by0, y0); y < std::min(L.by1, y0 + h); ++y)
    for (int32_t x = std::max<int32_t>({0, L.bx0 - 2, x0}); x < std::min(cw, x0 + w); ++x) {
      uint32_t &d = out[static_cast<size_t>(y - y0) * static_cast<size_t>(w) + static_cast<size_t>(x - x0)];
      d = ((d >> 1) & 0x007f7f7fu) | 0xff000000u;
    }
  Viewport vc = L.vp;
  vc.origin_x -= x0;
  vc.origin_y -= y0;
  for (const portsim::GfxTextRun &r : L.runs) overlay_raster_->draw(r, false, vc, L.dxr, out, w, h);
#else
  (void)out; (void)cw; (void)ch; (void)x0; (void)y0; (void)w; (void)h;
#endif
}

#ifdef _WIN32
void Presenter::paint(void *hdc, int32_t cw, int32_t ch) const {
  if (cw <= 0 || ch <= 0) return;
  painted_px_ = static_cast<int64_t>(cw) * ch;  // P14A fix 2 (object_rate_now)
  if (gpu_ && gpu_->ok()) {  // P14A: the GPU path (gpu.cpp)
    paint_gpu(cw, ch);
    return;
  }
  const int64_t t0 = portsim::perf_now_ns();  // P10A
  render(cw, ch, scratch_);
  if (overlay_mode_) {
    const ProfScope prof(Phase::Overlay);  // P10A
    draw_overlay(scratch_, cw, ch);
  }
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = cw;
  bi.bmiHeader.biHeight = -ch;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  {
    const ProfScope prof(Phase::Blit);  // P10A
    SetDIBitsToDevice(static_cast<HDC>(hdc), 0, 0, static_cast<DWORD>(cw), static_cast<DWORD>(ch), 0, 0, 0,
                      static_cast<UINT>(ch), scratch_.data(), &bi, DIB_RGB_COLORS);
  }
  // P10E: a paint that showed the live preview at a new place (no new frame) counts as a cursor paint
  const bool fresh = frames_set_ != painted_set_;
  // P12A fix 1: a paint that showed the camera slide at a new offset (no new frame); the FPS overlay counts every paint whose
  // picture changed (a new frame, a preview move, a slide step, a hover swap: mark_changed)
  const bool cursor = !fresh && live_ != painted_live_ && live_.active;
  const bool slide = !fresh && (cur_x_ != painted_cur_x_ || cur_y_ != painted_cur_y_);
  Profiler::get().painted(fresh, portsim::perf_now_ns() - t0, cursor, slide, fresh || cursor || slide || changed_pending_);  // P10A
  painted_set_ = frames_set_;
  painted_live_ = live_;
  painted_cur_x_ = cur_x_;
  painted_cur_y_ = cur_y_;
  changed_pending_ = false;
}
#endif

}  // namespace portapp
