// P9C: the game's text at the window's resolution (textres.h).
#include "textres.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <tuple>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "present.h"  // rgb555_to_bgra
#include "portsim/sim/gfx_ui.h"  // P40: UI runs
#include "portsim/sim/gfx_uivec.h"  // P41G: UvFrame
#include "portsim/sim/gfx_uvgame.h"  // P41G: the in-game element runs' cached 1x frames
#include <memory>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <set>
#include <thread>

namespace portapp {

namespace {
uint16_t to555(uint32_t c) {
  return static_cast<uint16_t>((((c >> 19) & 31) << 10) | (((c >> 11) & 31) << 5) | ((c >> 3) & 31));
}
// the first client column (row) whose 800-space column (row) under the presenter's floor mapping is >= l
int32_t first_client(int32_t l, int32_t n_client, int32_t n_log) {
  const int64_t num = static_cast<int64_t>(l) * n_client;
  return static_cast<int32_t>(num >= 0 ? (num + n_log - 1) / n_log : -((-num) / n_log));
}
int32_t floor_div(int64_t a, int64_t b) { return static_cast<int32_t>(a >= 0 ? a / b : -((-a + b - 1) / b)); }

// P40 (portsim/sim/gfx_ui.h): a UI run's scene rendered at the content rectangle's scale over the scene's whole client box (its
// 800-space bounds mapped as TextRaster::prep maps a run's box). The render is kept per scene, viewport, offset and canvas, so the
// runs of one scene (a screen that redraws its panel every page: each redraw is a run over the pixels it changed) share one
// render, and a paint whose frame is unchanged renders nothing. `bx0..by1` = the run's own client box inside it.
struct UiRender {
  int32_t x0 = 0, y0 = 0, w = 0, h = 0;
  std::vector<float> prem;
  // the frame pixels the scene covers at scale 1 (what the root drawing wrote; the HUD layer holds exactly those): the window's
  // pixels are drawn only over them, so the CPU draw and the GPU tables (applied inside the layer) write the same pixels
  int32_t fx0 = 0, fy0 = 0, fw = 0, fh = 0;
  std::vector<uint8_t> cover;
  // P40B: per client pixel of the box its table entry + 1 (0: the scene changes no 5-bit word there at any level), the entries'
  // 32 words each (the scene's premultiplied colour over grey level L, gfx_ui.h ui_over_level), and per client column / row its
  // frame column / row (the presenter's floor mapping). Made once per scene and viewport; a run's table is the masked part of it.
  std::vector<uint32_t> ent;
  std::vector<uint16_t> lut;
  std::vector<int32_t> lxs, lys;
  bool quick = false;   // P41G: the run's 1x frame upscaled (nearest) while a worker renders it at the window's scale
  // the word at entry e over word w, per channel from its level (what the GPU's text_runs computes)
  uint16_t over(uint32_t e, uint16_t word) const {
    const uint16_t *l = lut.data() + static_cast<size_t>(e) * 32u;
    return static_cast<uint16_t>((l[(word >> 10) & 31] & 0x7c00u) | (l[(word >> 5) & 31] & 0x03e0u) | (l[word & 31] & 0x001fu));
  }
  bool covered(int32_t lx, int32_t ly) const {
    if (lx < fx0 || ly < fy0 || lx >= fx0 + fw || ly >= fy0 + fh) return false;
    return cover[static_cast<size_t>(ly - fy0) * static_cast<size_t>(fw) + static_cast<size_t>(lx - fx0)] != 0;
  }
  // P41W (TextRaster::set_true_colour): the window-scale render, premultiplied, 8 bits per channel (0xAARRGGBB, A = coverage), and
  // the cover grown by one frame pixel
  std::vector<uint32_t> prem8;
  std::vector<uint8_t> near_cover;
  bool covered_near(int32_t lx, int32_t ly) const {
    if (lx < fx0 || ly < fy0 || lx >= fx0 + fw || ly >= fy0 + fh) return false;
    return near_cover[static_cast<size_t>(ly - fy0) * static_cast<size_t>(fw) + static_cast<size_t>(lx - fx0)] != 0;
  }
};
struct UiBox {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // the run's pixels in canvas coordinates (inside the canvas and the render)
  int32_t ox = 0, oy = 0;                  // P40C: the canvas coordinates of the render's pixel (0, 0)
};
// P41G: one run's entry for a viewport (the render at the window's scale, or with quick the run's 1x frame upscaled), as ui_prepare made
// it before; false when the text names no run
bool build_render(const std::string &text, const Viewport &vp, UiRender &u, bool quick) {
  // P41: the run's text is an encoded UiScene or names a uivec scene (gfx_ui.h ui_run_bounds / ui_run_render)
  float fx0, fy0, fx1, fy1;
  if (!portsim::ui_run_bounds(text, fx0, fy0, fx1, fy1)) return false;
  u.x0 = first_client(static_cast<int32_t>(std::floor(fx0)), vp.client_w, kLogicalW);   // origin-0 client coordinates
  u.y0 = first_client(static_cast<int32_t>(std::floor(fy0)), vp.client_h, kLogicalH);
  const int32_t x1 = first_client(static_cast<int32_t>(std::ceil(fx1)), vp.client_w, kLogicalW);
  const int32_t y1 = first_client(static_cast<int32_t>(std::ceil(fy1)), vp.client_h, kLogicalH);
  u.w = std::max(0, x1 - u.x0);
  u.h = std::max(0, y1 - u.y0);
  // per client column / row of the render its frame column / row (the presenter's floor mapping)
  u.lxs.resize(static_cast<size_t>(u.w));
  for (int32_t x = 0; x < u.w; ++x)
    u.lxs[static_cast<size_t>(x)] = floor_div(static_cast<int64_t>(u.x0 + x) * kLogicalW, vp.client_w);
  u.lys.resize(static_cast<size_t>(u.h));
  for (int32_t y = 0; y < u.h; ++y)
    u.lys[static_cast<size_t>(y)] = floor_div(static_cast<int64_t>(u.y0 + y) * kLogicalH, vp.client_h);
  const auto t_render = std::chrono::steady_clock::now();
  // P41G: an in-game element run's 1x frame (the sim's root drawing, kept per text: gfx_uvgame.h uvg_frame)
  std::shared_ptr<const portsim::UvFrame> f1 = portsim::uvg_is_run_text(text) ? portsim::uvg_frame(text) : nullptr;
  // quick: per client column / row its 1x frame column / row (-1: none). The entry's words are the 1x pixels' (below): no
  // window-scale picture is made
  std::vector<int32_t> qx, qy;
  const bool q1 = quick && f1 && u.w > 0 && u.h > 0;
  const int32_t fw1 = f1 ? f1->x1 - f1->x0 : 0, fh1 = f1 ? f1->y1 - f1->y0 : 0;
  if (quick && f1) {   // the 1x frame upscaled, nearest (the worker renders the window-scale one)
    qx.resize(static_cast<size_t>(u.w));
    for (int32_t x = 0; x < u.w; ++x) {
      const int32_t lx = u.lxs[static_cast<size_t>(x)] - f1->x0;
      qx[static_cast<size_t>(x)] = lx >= 0 && lx < fw1 ? lx : -1;
    }
    qy.resize(static_cast<size_t>(u.h));
    for (int32_t y = 0; y < u.h; ++y) {
      const int32_t ly = u.lys[static_cast<size_t>(y)] - f1->y0;
      qy[static_cast<size_t>(y)] = ly >= 0 && ly < fh1 ? ly : -1;
    }
    u.quick = true;
  } else {
    portsim::ui_run_render(text, static_cast<double>(vp.client_w) / kLogicalW, static_cast<double>(vp.client_h) / kLogicalH, 0.0,
                           0.0, u.x0, u.y0, u.w, u.h, u.prem);
  }
  if (const char *lp = std::getenv("SG_P41G_UILOG")) {   // P41G development: every window-scale UI run render (ms, size, text)
    static std::mutex lmu;
    std::lock_guard<std::mutex> ll(lmu);
    if (FILE *lf = std::fopen(lp, "a")) {
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_render).count();
      std::fprintf(lf, "%.3f\t%.2f\t%d\t%d\t%s%s\n", std::chrono::duration<double>(t_render.time_since_epoch()).count(), ms, u.w, u.h,
                   text.compare(0, 4, "uvg:") == 0 || text.compare(0, 3, "uv:") == 0 || text.compare(0, 4, "uwo:") == 0 ||
                           text.compare(0, 4, "uwp:") == 0
                       ? text.c_str()
                       : "<UiScene>", u.quick ? " (quick)" : "");
      std::fclose(lf);
    }
  }
  u.fx0 = static_cast<int32_t>(std::floor(fx0));
  u.fy0 = static_cast<int32_t>(std::floor(fy0));
  u.fw = std::max(0, static_cast<int32_t>(std::ceil(fx1)) - u.fx0);
  u.fh = std::max(0, static_cast<int32_t>(std::ceil(fy1)) - u.fy0);
  u.cover.assign(static_cast<size_t>(u.fw) * static_cast<size_t>(u.fh), 0);
  if (f1 && f1->x0 == u.fx0 && f1->y0 == u.fy0 && f1->x1 - f1->x0 == u.fw && f1->y1 - f1->y0 == u.fh) {
    for (size_t i = 0; i < u.cover.size(); ++i) u.cover[i] = f1->prem[4 * i + 3] > 0.f ? 1 : 0;   // P41G: the same 1x render, kept
  } else {
    std::vector<float> one;
    portsim::ui_run_render(text, 1.0, 1.0, 0.0, 0.0, u.fx0, u.fy0, u.fw, u.fh, one);   // as the sim's root drawing (gfx_ui.cpp)
    for (size_t i = 0; i < u.cover.size(); ++i) u.cover[i] = one[4 * i + 3] > 0.f ? 1 : 0;
  }
  u.near_cover.assign(u.cover.size(), 0);
  for (int32_t y = 0; y < u.fh; ++y)
    for (int32_t x = 0; x < u.fw; ++x) {
      bool any = false;
      for (int32_t j = std::max(0, y - 1); j <= std::min(u.fh - 1, y + 1) && !any; ++j)
        for (int32_t i = std::max(0, x - 1); i <= std::min(u.fw - 1, x + 1) && !any; ++i)
          any = u.cover[static_cast<size_t>(j) * static_cast<size_t>(u.fw) + static_cast<size_t>(i)] != 0;
      u.near_cover[static_cast<size_t>(y) * static_cast<size_t>(u.fw) + static_cast<size_t>(x)] = any ? 1 : 0;
    }
  // the 8-bit words and the entries of the pixels of `src` (n pixels, 4 floats each); quick: of the 1x frame's pixels, mapped below
  const float *src = q1 ? f1->prem.data() : u.prem.data();
  const size_t n_src = q1 ? f1->prem.size() / 4u : static_cast<size_t>(u.w) * static_cast<size_t>(u.h);
  std::vector<uint32_t> pe(n_src, 0), en(n_src, 0);
  for (size_t i = 0; i < n_src; ++i) {
    const float *q = src + i * 4u;
    auto b8 = [](float v) { return static_cast<uint32_t>(std::min(255.f, std::max(0.f, v + 0.5f))); };
    pe[i] = b8(q[3] * 255.f) << 24 | b8(q[0]) << 16 | b8(q[1]) << 8 | b8(q[2]);
  }
  // the entries: pixels of one premultiplied value share one (memo), entries of equal words are one (ids)
  std::unordered_map<uint64_t, std::pair<std::array<uint32_t, 4>, uint32_t>> memo;
  std::unordered_multimap<uint64_t, uint32_t> ids;
  uint16_t e[32];
  for (size_t i = 0; i < n_src; ++i) {
    const float *q = src + i * 4u;
    if (q[3] <= 0.f) continue;
    uint32_t pk[4];
    std::memcpy(pk, q, sizeof pk);
    const uint64_t ph = (static_cast<uint64_t>(pk[0]) * 0x9E3779B97F4A7C15ull) ^ (static_cast<uint64_t>(pk[1]) * 0xC2B2AE3D27D4EB4Full) ^
                        (static_cast<uint64_t>(pk[2]) * 0x165667B19E3779F9ull) ^ (static_cast<uint64_t>(pk[3]) * 0x27D4EB2F165667C5ull);
    auto m = memo.find(ph);
    if (m != memo.end() && std::memcmp(m->second.first.data(), pk, sizeof pk) == 0) {
      en[i] = m->second.second;
      continue;
    }
    bool ink = false;
    uint64_t hsh = 1469598103934665603ull;
    for (uint32_t L = 0; L < 32; ++L) {
      e[L] = portsim::ui_over_level(q, L);
      ink = ink || e[L] != static_cast<uint16_t>(L | (L << 5) | (L << 10));
      hsh = (hsh ^ e[L]) * 1099511628211ull;
    }
    uint32_t k1 = 0;
    if (ink) {
      uint32_t k = ~0u;
      for (auto r2 = ids.equal_range(hsh); r2.first != r2.second; ++r2.first)
        if (std::memcmp(u.lut.data() + static_cast<size_t>(r2.first->second) * 32u, e, sizeof e) == 0) {
          k = r2.first->second;
          break;
        }
      if (k == ~0u) {
        k = static_cast<uint32_t>(u.lut.size() / 32);
        ids.emplace(hsh, k);
        u.lut.insert(u.lut.end(), e, e + 32);
      }
      k1 = k + 1;
    }
    memo[ph] = {std::array<uint32_t, 4>{pk[0], pk[1], pk[2], pk[3]}, k1};
    en[i] = k1;
  }
  if (q1) {   // P41G quick: each window pixel takes its 1x pixel's word and entry (no hashing per window pixel); the client rows of
    // one 1x row are one row copied
    const size_t w = static_cast<size_t>(u.w);
    u.prem8.assign(w * static_cast<size_t>(u.h), 0);
    u.ent.assign(w * static_cast<size_t>(u.h), 0);
    for (int32_t y = 0; y < u.h; ++y) {
      const int32_t ly = qy[static_cast<size_t>(y)];
      if (ly < 0) continue;
      uint32_t *pr = u.prem8.data() + static_cast<size_t>(y) * w, *er = u.ent.data() + static_cast<size_t>(y) * w;
      if (y > 0 && qy[static_cast<size_t>(y - 1)] == ly) {
        std::memcpy(pr, pr - w, w * 4u);
        std::memcpy(er, er - w, w * 4u);
        continue;
      }
      const size_t base = static_cast<size_t>(ly) * static_cast<size_t>(fw1);
      for (size_t x = 0; x < w; ++x)
        if (qx[x] >= 0) {
          pr[x] = pe[base + static_cast<size_t>(qx[x])];
          er[x] = en[base + static_cast<size_t>(qx[x])];
        }
    }
  } else if (quick && f1) {   // (an empty render)
    u.prem8.clear();
    u.ent.clear();
  } else {
    u.prem8 = std::move(pe);
    u.ent = std::move(en);
  }
  u.prem.clear();
  u.prem.shrink_to_fit();
  return true;
}
// P41G: a big in-game element run (more window pixels than this) is rendered on a worker; its quick entry shows until then
constexpr size_t kUiAsyncPx = 20000;
struct UiAsync {
  std::mutex mu;
  std::map<std::string, std::shared_ptr<UiRender>> ready;
  std::set<std::string> pending;
  // one persistent worker renders the queued runs one at a time, each on at most half the machine's threads (gfx_uvgame.h
  // uvg_set_render_threads): detached workers per run starved the paint thread at 4K (a report's bands: paints of 600 ms)
  std::deque<std::pair<std::string, std::pair<std::string, Viewport>>> queue;
  std::condition_variable cv;
  bool started = false;
  std::atomic<uint64_t> serial{0};
  std::function<void()> notify;
};
UiAsync &ui_async() {
  static UiAsync a;
  return a;
}
}  // namespace
uint64_t ui_async_serial() { return ui_async().serial.load(); }
void ui_async_set_notify(std::function<void()> f) {
  UiAsync &A = ui_async();
  std::lock_guard<std::mutex> l(A.mu);
  A.notify = std::move(f);
}
namespace {
void ui_async_enqueue(const std::string &key, const std::string &text, const Viewport &v) {
  UiAsync &A = ui_async();
  std::lock_guard<std::mutex> l(A.mu);
  A.queue.emplace_back(key, std::make_pair(text, v));
  if (!A.started) {
    A.started = true;
    std::thread([]() {
      portsim::uvg_set_render_threads(static_cast<int32_t>(std::max(1u, std::thread::hardware_concurrency() / 2)));
      UiAsync &B = ui_async();
      for (;;) {
        std::pair<std::string, std::pair<std::string, Viewport>> job;
        {
          std::unique_lock<std::mutex> l(B.mu);
          B.cv.wait(l, [&] { return !B.queue.empty(); });
          job = std::move(B.queue.back());   // the newest first (what the screen shows now)
          B.queue.pop_back();
        }
        auto f = std::make_shared<UiRender>();
        const bool ok = build_render(job.second.first, job.second.second, *f, false);
        std::function<void()> nf;
        {
          std::lock_guard<std::mutex> l(B.mu);
          B.pending.erase(job.first);
          if (ok) B.ready[job.first] = f;
          // a bound (entries no paint came back for; an evicted one is queued again by the paint that finds its quick entry)
          if (B.ready.size() > 256) B.ready.erase(B.ready.begin());
          B.serial.fetch_add(1);
          nf = B.notify;
        }
        if (nf) nf();   // P41G: paint again (the tables and patches made from the quick entry are made again)
      }
    }).detach();
  }
  A.cv.notify_one();
}

const UiRender *ui_prepare(const portsim::GfxTextRun &r, const Viewport &vp, int32_t dx, int32_t cw, int32_t ch, UiBox &rb) {
  if (vp.client_w <= 0 || vp.client_h <= 0 || r.x1 <= r.x0 || r.y1 <= r.y0) return nullptr;
  // P40C: one render per scene and scale, at origin 0: a canvas whose content rectangle starts elsewhere (a patch rectangle's
  // canvas, a moved layer) sees it moved by whole pixels (the clip and the floor mapping move with it). Before, every canvas
  // origin rendered the scene again: at 4K every final-patch build re-rendered each UI scene (17-43 ms a build).
  static thread_local std::map<std::string, UiRender> cache;
  static thread_local size_t cached_px = 0;
  std::string key = r.text;
  for (int64_t v : {static_cast<int64_t>(vp.client_w), static_cast<int64_t>(vp.client_h)})
    key.append(reinterpret_cast<const char *>(&v), sizeof v);
  auto it = cache.find(key);
  if (it != cache.end() && it->second.quick) {   // P41G: the worker's window-scale entry replaces the quick one when ready
    UiAsync &A = ui_async();
    std::shared_ptr<UiRender> full;
    bool requeue = false;
    {
      std::lock_guard<std::mutex> l(A.mu);
      auto f = A.ready.find(key);
      if (f != A.ready.end()) {
        full = f->second;
        A.ready.erase(f);
      } else if (A.pending.insert(key).second) {   // its render was dropped from the ready bound: render it again
        requeue = true;
      }
    }
    if (full) it->second = std::move(*full);
    if (requeue) ui_async_enqueue(key, r.text, vp);
  }
  if (it == cache.end()) {
    UiRender u;
    bool done = false;
    if (portsim::uvg_is_run_text(r.text)) {   // P41G: a big in-game element run: a worker renders it, a quick entry shows meanwhile
      UiAsync &A = ui_async();
      std::shared_ptr<UiRender> full;
      {
        std::lock_guard<std::mutex> l(A.mu);
        auto f = A.ready.find(key);
        if (f != A.ready.end()) {
          full = f->second;
          A.ready.erase(f);
        }
      }
      if (full) {
        u = std::move(*full);
        done = true;
      } else {
        float a0, b0, a1, b1;
        if (!portsim::ui_run_bounds(r.text, a0, b0, a1, b1)) return nullptr;
        const double px_est = (a1 - a0) * static_cast<double>(vp.client_w) / kLogicalW * (b1 - b0) * vp.client_h / kLogicalH;
        if (px_est > kUiAsyncPx && vp.client_w > kLogicalW && !std::getenv("SG_P41G_SYNC")) {
          bool start = false;
          {
            std::lock_guard<std::mutex> l(A.mu);
            start = A.pending.insert(key).second;
          }
          if (start) ui_async_enqueue(key, r.text, vp);
          if (!build_render(r.text, vp, u, true)) return nullptr;
          done = true;
        }
      }
    }
    if (!done && !build_render(r.text, vp, u, false)) return nullptr;
    const size_t px = static_cast<size_t>(u.w) * static_cast<size_t>(u.h);
    // about 300 MB of entries and indexes at most. P41G: 1024 entries (was 48): the in-game HUD drawn from the approved scenes holds
    // 60-100 runs on screen (each button, pill, face, pip and medal is its own run), so 48 cleared the whole cache every few paints and
    // re-rendered every run at once (paints of 160-170 ms, log/roadmap/P41/portgame/notes.md); the pixel bound stays the memory bound
    if (cache.size() > 1024 || cached_px + px > 60000000u) {
      cache.clear();
      cached_px = 0;
    }
    cached_px += px;
    it = cache.emplace(key, std::move(u)).first;
  }
  const UiRender &u = it->second;
  const int32_t sx = vp.origin_x + dx, sy = vp.origin_y;   // canvas = origin-0 client + (sx, sy)
  rb.ox = u.x0 + sx;
  rb.oy = u.y0 + sy;
  rb.x0 = std::max(std::max(rb.ox, 0), sx + first_client(r.x0, vp.client_w, kLogicalW));
  rb.y0 = std::max(std::max(rb.oy, 0), sy + first_client(r.y0, vp.client_h, kLogicalH));
  rb.x1 = std::min(std::min(rb.ox + u.w, cw), sx + first_client(r.x1, vp.client_w, kLogicalW));
  rb.y1 = std::min(std::min(rb.oy + u.h, ch), sy + first_client(r.y1, vp.client_h, kLogicalH));
  return &u;
}
// whether box pixel (x, y) (render coordinates) shows the run: its frame pixel visible and covered by the scale-1 drawing, as a mask
// per frame pixel of the render's frame box (P41G: tested once per frame pixel, not once per window pixel; with near the cover grown
// by one pixel (grown), the true-colour draw's test), and per render column / row its index in the box (-1: outside, never shown)
struct UiVis {
  std::vector<uint8_t> v;
  std::vector<int32_t> cx, cy;
  int32_t fw = 0;
};
void ui_vis(const portsim::GfxTextRun &r, const UiRender &u, bool layered, bool grown, UiVis &o) {
  o.fw = u.fw;
  o.v.assign(static_cast<size_t>(u.fw) * static_cast<size_t>(u.fh), 0);
  const std::vector<uint8_t> &c = grown ? u.near_cover : u.cover;
  for (int32_t ly = 0; ly < u.fh; ++ly)
    for (int32_t lx = 0; lx < u.fw; ++lx) {
      const size_t i = static_cast<size_t>(ly) * static_cast<size_t>(u.fw) + static_cast<size_t>(lx);
      o.v[i] = c[i] != 0 && r.visible(u.fx0 + lx, u.fy0 + ly, layered) != 0 ? 1 : 0;
    }
  o.cx.resize(static_cast<size_t>(u.w));
  for (int32_t x = 0; x < u.w; ++x) {
    const int32_t lx = u.lxs[static_cast<size_t>(x)] - u.fx0;
    o.cx[static_cast<size_t>(x)] = lx >= 0 && lx < u.fw ? lx : -1;
  }
  o.cy.resize(static_cast<size_t>(u.h));
  for (int32_t y = 0; y < u.h; ++y) {
    const int32_t ly = u.lys[static_cast<size_t>(y)] - u.fy0;
    o.cy[static_cast<size_t>(y)] = ly >= 0 && ly < u.fh ? ly : -1;
  }
}
}  // namespace

bool TextRaster::ui_cover(const portsim::GfxTextRun &r, int32_t &fx0, int32_t &fy0, int32_t &fw, int32_t &fh, const uint8_t *&cover) {
  if (!portsim::ui_is_run_face(r.face)) return false;
  // the cover does not depend on the viewport: any prepared one serves (the 800 x 600 one is the smallest)
  UiBox b;
  Viewport v;
  v.client_w = kLogicalW;
  v.client_h = kLogicalH;
  const UiRender *u = ui_prepare(r, v, 0, kLogicalW, kLogicalH, b);
  if (!u) return false;
  fx0 = u->fx0;
  fy0 = u->fy0;
  fw = u->fw;
  fh = u->fh;
  cover = u->cover.data();
  return true;
}

#ifdef _WIN32
struct TextRaster::Impl {
  HDC dc = nullptr;
  HBITMAP bmp = nullptr;
  void *bits = nullptr;
  int32_t w = 0, h = 0;
  std::map<std::tuple<std::string, int32_t, uint32_t, int>, HFONT> fonts;
  // P10B fix 2: the ink rows of a string in a font, relative to the baseline (top <= bottom, half-open: [top, bottom)); key = the
  // font's key and the string. Measured on a 32-bit scratch DIB (white on black), cleared above 4096 entries.
  struct Ink {
    int32_t top = 0, bottom = 0;
    bool ok = false;
  };
  std::map<std::tuple<std::string, int32_t, uint32_t, int, std::string>, Ink> inks;
  // P24C fix 2b: table's 32 level pictures of a string, relative to its TA_BASELINE origin: res[L][(y - ry0) * rw + x - rx0] is GDI's
  // word at (origin.x + x, origin.y + y) over level L. GDI draws at a whole-pixel origin, so these words do not depend on where the
  // origin lies: a run that moved (a walking golfer's label, each game frame) is made from them without drawing again. Key: the font
  // handle, the colour, the string and the fitted advances. A request whose rectangle the kept one does not hold draws again over both.
  struct Raster {
    int32_t rx0 = 0, ry0 = 0, rw = 0, rh = 0;
    std::vector<uint16_t> res;
  };
  std::map<std::string, Raster> rasters;
  HDC mdc = nullptr;
  HBITMAP mbmp = nullptr;
  void *mbits = nullptr;
  int32_t mw = 0, mh = 0;
  Ink ink(const std::string &face, int32_t height, uint32_t style, int quality, const std::string &text, const INT *adv) {
    const auto key = std::make_tuple(face, height, style, quality, text);
    auto it = inks.find(key);
    if (it != inks.end()) return it->second;
    if (inks.size() > 4096) inks.clear();
    Ink k;
    HFONT f = font(face, height, style, quality);
    const int32_t iw = height * (static_cast<int32_t>(text.size()) + 2), ih = height * 3;
    if (f && iw > 0 && ih > 0) {
      if (!mdc) mdc = CreateCompatibleDC(nullptr);
      if (mdc && (!mbmp || iw > mw || ih > mh)) {
        if (mbmp) DeleteObject(mbmp);
        mw = std::max(iw, mw);
        mh = std::max(ih, mh);
        BITMAPINFO bi;
        std::memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = mw;
        bi.bmiHeader.biHeight = -mh;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        mbmp = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, &mbits, nullptr, 0);
        if (mbmp) SelectObject(mdc, mbmp);
        else mw = mh = 0;
      }
      if (mbmp) {
        GdiFlush();
        std::memset(mbits, 0, static_cast<size_t>(mw) * static_cast<size_t>(mh) * 4);
        SetBkMode(mdc, TRANSPARENT);
        SetTextAlign(mdc, TA_BASELINE);
        SetTextColor(mdc, RGB(255, 255, 255));
        SelectObject(mdc, f);
        const int32_t base = height * 2;
        if (adv)
          ExtTextOutA(mdc, height, base, 0, nullptr, text.data(), static_cast<UINT>(text.size()), adv);
        else
          TextOutA(mdc, height, base, text.data(), static_cast<int>(text.size()));
        SelectObject(mdc, GetStockObject(SYSTEM_FONT));
        GdiFlush();
        const uint32_t *p = static_cast<const uint32_t *>(mbits);
        int32_t t = -1, b = -1;
        for (int32_t y = 0; y < ih; ++y) {
          bool any = false;
          for (int32_t x = 0; x < iw && !any; ++x) any = (p[static_cast<size_t>(y) * mw + static_cast<size_t>(x)] & 0xffffffu) != 0;
          if (any) {
            if (t < 0) t = y;
            b = y + 1;
          }
        }
        if (t >= 0) {
          k.top = t - base;
          k.bottom = b - base;
          k.ok = true;
        }
      }
    }
    inks[key] = k;
    return k;
  }
  ~Impl() {
    if (mbmp) DeleteObject(mbmp);
    if (mdc) DeleteDC(mdc);
    for (auto &f : fonts)
      if (f.second) DeleteObject(f.second);
    if (bmp) DeleteObject(bmp);
    if (dc) DeleteDC(dc);
  }
  // Font::create's LOGFONTA (gditext.cpp logfont_of) at lfHeight -height and the given quality
  HFONT font(const std::string &face, int32_t height, uint32_t style, int quality) {
    const auto key = std::make_tuple(face, height, style, quality);
    auto it = fonts.find(key);
    if (it != fonts.end()) return it->second;
    LOGFONTA lf;
    std::memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -height;
    lf.lfWeight = (style & 1u) ? 700 : 0;
    lf.lfItalic = (style & 2u) ? 1 : 0;
    lf.lfUnderline = (style & 4u) ? 1 : 0;
    lf.lfOutPrecision = 7;  // OUT_TT_ONLY_PRECIS
    lf.lfQuality = static_cast<BYTE>(quality);
    std::memcpy(lf.lfFaceName, face.data(), std::min(face.size(), sizeof(lf.lfFaceName) - 1));
    HFONT f = CreateFontIndirectA(&lf);
    fonts[key] = f;
    return f;
  }
  // a 16-bit top-down BI_RGB DIB section (RGB555) of at least nw x nh, selected into the DC
  bool dib(int32_t nw, int32_t nh) {
    if (!dc) {
      dc = CreateCompatibleDC(nullptr);
      if (!dc) return false;
      SetBkMode(dc, TRANSPARENT);
      SetTextAlign(dc, TA_BASELINE);
    }
    if (bmp && nw <= w && nh <= h) return true;
    if (bmp) DeleteObject(bmp);
    bmp = nullptr;
    w = std::max(nw, w);
    h = std::max(nh, h);
    BITMAPINFO bi;
    std::memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 16;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) {
      w = h = 0;
      return false;
    }
    SelectObject(dc, bmp);
    return true;
  }
};
#else
struct TextRaster::Impl {};
#endif

TextRaster::TextRaster() : impl_(new Impl) {}
TextRaster::~TextRaster() = default;

// P15B: what draw and table share: the font, the DIB, the run's client box and draw rectangle, the colour, the origin and (fit)
// the advances. Moved out of draw unchanged; the fit measurements (GetTextExtentExPointA on the DIB's DC, the ink rows on their own
// scratch DIB) now run before the DIB is filled, which writes no pixel of it.
struct TextRaster::Prep {
#ifdef _WIN32
  HFONT f = nullptr;
#endif
  int32_t bx0 = 0, bx1 = 0, by0 = 0, by1 = 0;  // the run's box in client pixels
  int32_t dx0 = 0, dx1 = 0, dy0 = 0, dy1 = 0, w = 0, h = 0;  // the DIB's rectangle (the box and a margin, in the client)
  int32_t ox = 0, oy = 0, n = 0;
  uint32_t ref = 0;
  bool fitted = false;
  std::vector<int> adv;
};

bool TextRaster::prep(const portsim::GfxTextRun &r, const Viewport &vp, int32_t dx, int32_t cw, int32_t ch, Prep &p) {
#ifdef _WIN32
  if (vp.client_w <= 0 || vp.client_h <= 0 || r.text.empty() || r.box_x1() <= r.box_x0() || r.y1 <= r.y0) return false;  // (P22E)
  const bool unit = vp.client_w == kLogicalW && vp.client_h == kLogicalH;
  const double sx = static_cast<double>(vp.client_w) / kLogicalW, sy = static_cast<double>(vp.client_h) / kLogicalH;
  int32_t height = static_cast<int32_t>(std::lround(r.size * sy));
  if (defect_ == TextDefect::WrongScale) height = r.size;
  if (height < 1) height = 1;
  // the run's box in client pixels (the presenter's floor mapping inverted), and a margin around it for ink that reaches past it
  // (P22E item 7: a run the root's edge cut takes its whole columns, GfxTextRun::box_x0 / box_x1)
  p.bx0 = vp.origin_x + dx + first_client(r.box_x0(), vp.client_w, kLogicalW);
  p.bx1 = vp.origin_x + dx + first_client(r.box_x1(), vp.client_w, kLogicalW);
  p.by0 = vp.origin_y + first_client(r.y0, vp.client_h, kLogicalH);
  p.by1 = vp.origin_y + first_client(r.y1, vp.client_h, kLogicalH);
  const int32_t m = std::max<int32_t>(4, height);
  p.dx0 = std::max(0, p.bx0 - m);
  p.dx1 = std::min(cw, p.bx1 + m);
  p.dy0 = std::max(0, p.by0 - m);
  p.dy1 = std::min(ch, p.by1 + m);
  if (p.dx1 <= p.dx0 || p.dy1 <= p.dy0) return false;
  p.w = p.dx1 - p.dx0;
  p.h = p.dy1 - p.dy0;
  const int quality = unit ? 0 : aa_ == TextAa::ClearType ? 5 /* CLEARTYPE_QUALITY */ : aa_ == TextAa::Grey ? 4 /* ANTIALIASED */ : 0;
  p.f = impl_->font(r.face, height, r.style, quality);
  if (!p.f || !impl_->dib(p.w, p.h)) {
    ++stats.failed;
    return false;
  }
  uint32_t ref = r.colorref;
  if (defect_ == TextDefect::WrongColour) ref = ((ref & 0xffu) << 16) | (ref & 0xff00u) | ((ref >> 16) & 0xffu);
  p.ref = ref;
  // the origin: the 800-space TA_BASELINE point scaled (the identity at scale 1)
  p.ox = vp.origin_x + dx + (unit ? r.x : static_cast<int32_t>(std::lround(r.x * sx))) - p.dx0;
  p.oy = vp.origin_y + (unit ? r.y : static_cast<int32_t>(std::lround(r.y * sy))) - p.dy0;
  const int n = static_cast<int>(r.text.size());
  p.n = n;
  // P10B fix 2: the 800-space layout scaled (textres.h set_fit). The game's font at its own size (lfQuality 0, as gditext makes
  // it) gives the string's 800-space advance; the window-resolution font's own pen positions are stretched evenly to that advance
  // scaled, so a label's box keeps no blank space after the string and a string is not cut by its box, with the glyphs spaced as
  // the larger font spaces them (the 800-space positions themselves, hinted at the game's small sizes, gave uneven gaps). The
  // baseline is then moved so that the ink's vertical middle lands on the 800-space ink's middle scaled (the game's hinted small
  // sizes put the ink lower in its line than the larger font does).
  std::vector<INT> &adv = p.adv;
  adv.clear();
  p.fitted = false;
  if (!unit && fit_ && n > 0) {
    HFONT lo = impl_->font(r.face, r.size, r.style, 0);
    std::vector<INT> cum(static_cast<size_t>(n), 0), cum_hi(static_cast<size_t>(n), 0);
    SIZE sz;
    if (lo && SelectObject(impl_->dc, lo) && GetTextExtentExPointA(impl_->dc, r.text.data(), n, 0, nullptr, cum.data(), &sz) &&
        SelectObject(impl_->dc, p.f) && GetTextExtentExPointA(impl_->dc, r.text.data(), n, 0, nullptr, cum_hi.data(), &sz) &&
        cum_hi[static_cast<size_t>(n - 1)] > 0) {
      adv.resize(static_cast<size_t>(n));
      const double k = cum[static_cast<size_t>(n - 1)] * sx / cum_hi[static_cast<size_t>(n - 1)];
      int32_t prev = 0;
      for (int i = 0; i < n; ++i) {
        const int32_t at = static_cast<int32_t>(std::lround(cum_hi[static_cast<size_t>(i)] * k));
        adv[static_cast<size_t>(i)] = at - prev;
        prev = at;
      }
      const Impl::Ink il = impl_->ink(r.face, r.size, r.style, 0, r.text, nullptr);
      const Impl::Ink ih = impl_->ink(r.face, height, r.style, quality, r.text, adv.data());
      if (il.ok && ih.ok) {
        const double mid_lo = (il.top + il.bottom) * 0.5 * sy, mid_hi = (ih.top + ih.bottom) * 0.5;
        p.oy = vp.origin_y + static_cast<int32_t>(std::lround(r.y * sy + mid_lo - mid_hi)) - p.dy0;
      }
      p.fitted = true;
    }
  }
  return true;
#else
  (void)r; (void)vp; (void)dx; (void)cw; (void)ch; (void)p;
  return false;
#endif
}

// the run drawn by GDI onto the DIB as it stands
void TextRaster::gdi(const portsim::GfxTextRun &r, const Prep &p) {
#ifdef _WIN32
  SelectObject(impl_->dc, p.f);
  SetTextColor(impl_->dc, p.ref);
  if (!p.adv.empty())
    ExtTextOutA(impl_->dc, p.ox, p.oy, 0, nullptr, r.text.data(), static_cast<UINT>(p.n), p.adv.data());
  else
    TextOutA(impl_->dc, p.ox, p.oy, r.text.data(), p.n);
  SelectObject(impl_->dc, GetStockObject(SYSTEM_FONT));
  GdiFlush();
#else
  (void)r; (void)p;
#endif
}

void TextRaster::draw(const portsim::GfxTextRun &r, bool layered, const Viewport &vp, int32_t dx, std::vector<uint32_t> &out,
                      int32_t cw, int32_t ch) {
  if (portsim::ui_is_run_face(r.face)) {  // P40: a UI run: its entry over each visible pixel's word, written where it changes the
    // word (P40B: exactly the GPU's text_runs, so SG_P14_VERIFY compares equal words)
    UiBox b;
    const UiRender *u = ui_prepare(r, vp, dx, cw, ch, b);
    if (!u) return;
    UiVis V;
    ui_vis(r, *u, layered, true_colour_, V);
    if (true_colour_) {  // P41W: the premultiplied colour over the window's own word, 8 bits per channel
      for (int32_t y = b.y0 - b.oy; y < b.y1 - b.oy; ++y) {
        const int32_t cy = V.cy[static_cast<size_t>(y)];
        if (cy < 0) continue;
        const uint8_t *vrow = V.v.data() + static_cast<size_t>(cy) * static_cast<size_t>(V.fw);
        for (int32_t x = b.x0 - b.ox; x < b.x1 - b.ox; ++x) {
          const uint32_t p = u->prem8[static_cast<size_t>(y) * static_cast<size_t>(u->w) + static_cast<size_t>(x)];
          const uint32_t a = p >> 24;
          if (a == 0) continue;
          const int32_t cx = V.cx[static_cast<size_t>(x)];
          if (cx < 0 || !vrow[cx]) continue;
          uint32_t &o = out[static_cast<size_t>(b.oy + y) * static_cast<size_t>(cw) + static_cast<size_t>(b.ox + x)];
          const uint32_t k = 255 - a;
          auto ch8 = [&](int sh) { return std::min<uint32_t>(255, ((p >> sh) & 255) + (((o >> sh) & 255) * k + 127) / 255); };
          o = 0xff000000u | ch8(16) << 16 | ch8(8) << 8 | ch8(0);
        }
      }
      ++stats.runs;
      return;
    }
    for (int32_t y = b.y0 - b.oy; y < b.y1 - b.oy; ++y) {
      const int32_t cy = V.cy[static_cast<size_t>(y)];
      if (cy < 0) continue;
      const uint8_t *vrow = V.v.data() + static_cast<size_t>(cy) * static_cast<size_t>(V.fw);
      for (int32_t x = b.x0 - b.ox; x < b.x1 - b.ox; ++x) {
        const uint32_t e = u->ent[static_cast<size_t>(y) * static_cast<size_t>(u->w) + static_cast<size_t>(x)];
        if (e == 0) continue;
        const int32_t cx = V.cx[static_cast<size_t>(x)];
        if (cx < 0 || !vrow[cx]) continue;
        uint32_t &o = out[static_cast<size_t>(b.oy + y) * static_cast<size_t>(cw) + static_cast<size_t>(b.ox + x)];
        const uint16_t w = to555(o), v = u->over(e - 1, w);
        if (v != w) o = rgb555_to_bgra(v);
      }
    }
    ++stats.runs;
    return;
  }
#ifdef _WIN32
  Prep p;
  if (!prep(r, vp, dx, cw, ch, p)) return;
  if (p.fitted) ++stats.fitted;
  const int32_t w = p.w, h = p.h, dx0 = p.dx0, dy0 = p.dy0;
  const size_t pitch = static_cast<size_t>((impl_->w + 1) & ~1);  // words per DIB row (rows of 4-byte multiples)
  uint16_t *bits = static_cast<uint16_t *>(impl_->bits);
  GdiFlush();
  std::vector<uint16_t> in(static_cast<size_t>(w) * static_cast<size_t>(h));
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const uint16_t v = to555(out[static_cast<size_t>(dy0 + y) * static_cast<size_t>(cw) + static_cast<size_t>(dx0 + x)]);
      in[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = v;
      bits[pitch * static_cast<size_t>(y) + static_cast<size_t>(x)] = v;
    }
  gdi(r, p);
  bool clipped = false;
  for (int32_t y = 0; y < h; ++y) {
    const int32_t cy = dy0 + y;
    const int32_t ly = floor_div(static_cast<int64_t>(cy - vp.origin_y) * kLogicalH, vp.client_h);
    for (int32_t x = 0; x < w; ++x) {
      const uint16_t v = bits[pitch * static_cast<size_t>(y) + static_cast<size_t>(x)];
      if (v == in[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]) continue;
      const int32_t cx = dx0 + x;
      const int32_t lx = floor_div(static_cast<int64_t>(cx - vp.origin_x - dx) * kLogicalW, vp.client_w);
      if (cx < p.bx0 || cx >= p.bx1 || cy < p.by0 || cy >= p.by1) {
        ++stats.clipped_px;  // ink outside the run's scaled box: clipped
        clipped = true;
        continue;
      }
      if (!r.visible(lx, ly, layered)) continue;  // drawn over by the game after the run
      out[static_cast<size_t>(cy) * static_cast<size_t>(cw) + static_cast<size_t>(cx)] = rgb555_to_bgra(v);
    }
  }
  ++stats.runs;
  if (clipped) ++stats.clipped_runs;
#else
  (void)r; (void)layered; (void)vp; (void)dx; (void)out; (void)cw; (void)ch;
#endif
}

// P15B: the run over each grey level (textres.h TextTable). A pixel draw would write over word w is one whose GDI result differs
// from w, inside the box, visible; per channel that result is lut[level of w's channel]'s channel, so the pixels kept are the box's
// visible pixels where some level's result differs from the level.
bool TextRaster::table(const portsim::GfxTextRun &r, bool layered, const Viewport &vp, int32_t dx, int32_t cw, int32_t ch,
                       TextTable &t, uint32_t levels) {
  static const char *tlog = std::getenv("SG_P41G_UILOG");   // P41G development: every table's time (ms, kind, text)
  if (!tlog) return table_(r, layered, vp, dx, cw, ch, t, levels);
  const auto t0 = std::chrono::steady_clock::now();
  const bool ok = table_(r, layered, vp, dx, cw, ch, t, levels);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  static std::mutex lmu;
  std::lock_guard<std::mutex> ll(lmu);
  if (FILE *lf = std::fopen(tlog, "a")) {
    const bool ui = portsim::ui_is_run_face(r.face);
    std::fprintf(lf, "%.3f\t%.2f\ttable\t%s\t%s\n", std::chrono::duration<double>(t0.time_since_epoch()).count(), ms, ui ? "ui" : "gdi",
                 ui && (r.text.compare(0, 4, "uvg:") == 0 || r.text.compare(0, 3, "uv:") == 0) ? r.text.c_str() : ui ? "<UiScene>" : r.text.c_str());
    std::fclose(lf);
  }
  return ok;
}

bool TextRaster::table_(const portsim::GfxTextRun &r, bool layered, const Viewport &vp, int32_t dx, int32_t cw, int32_t ch,
                        TextTable &t, uint32_t levels) {
  t = TextTable();
  if (portsim::ui_is_run_face(r.face)) {  // P40: a UI run's table: the scene's entries over the run's visible pixels (P40B: the
    // entries are the scene's, made once per scene and viewport; only the ones the run uses are copied)
    const uint64_t serial = ui_async_serial();   // (read before the prepare: a render ready after it moves the serial again)
    UiBox b;
    const UiRender *u = ui_prepare(r, vp, dx, cw, ch, b);
    if (!u) return false;
    t.quick = u->quick;
    t.serial = serial;
    const int32_t bw = std::max(0, b.x1 - b.x0), bh = std::max(0, b.y1 - b.y0);
    std::vector<uint32_t> idx(static_cast<size_t>(bw) * static_cast<size_t>(bh), 0);
    std::vector<uint32_t> remap(u->lut.size() / 32, 0);
    int32_t ix0 = bw, ix1 = -1, iy0 = bh, iy1 = -1;
    UiVis V;
    ui_vis(r, *u, layered, false, V);
    const int32_t rx0 = b.x0 - b.ox;
    int32_t prev_y = -2;   // P41G: the last row made, and whether it held a pixel (a quick entry's rows of one 1x row are equal)
    bool prev_hit = false;
    for (int32_t y = 0; y < bh; ++y) {
      const int32_t ry = b.y0 - b.oy + y;
      const int32_t cy = V.cy[static_cast<size_t>(ry)];
      if (cy < 0) continue;
      uint32_t *irow = idx.data() + static_cast<size_t>(y) * static_cast<size_t>(bw);
      if (u->quick && prev_y == y - 1 && u->lys[static_cast<size_t>(ry - 1)] == u->lys[static_cast<size_t>(ry)]) {
        std::memcpy(irow, irow - bw, static_cast<size_t>(bw) * 4u);   // the same entries and visibility: the same row
        if (prev_hit) iy1 = y + 1;
        prev_y = y;
        continue;
      }
      prev_y = y;
      prev_hit = false;
      const uint8_t *vrow = V.v.data() + static_cast<size_t>(cy) * static_cast<size_t>(V.fw);
      const uint32_t *erow = u->ent.data() + static_cast<size_t>(ry) * static_cast<size_t>(u->w) + static_cast<size_t>(rx0);
      const int32_t *cxs = V.cx.data() + rx0;
      for (int32_t x = 0; x < bw; ++x) {
        const uint32_t e = erow[x];
        if (e == 0) continue;
        const int32_t cx = cxs[x];
        if (cx < 0 || !vrow[cx]) continue;
        prev_hit = true;
        uint32_t &m = remap[e - 1];
        if (m == 0) {
          const uint16_t *src = u->lut.data() + static_cast<size_t>(e - 1) * 32u;
          const size_t at = t.lut.size();
          t.lut.insert(t.lut.end(), src, src + 32);
          for (uint32_t L = levels; L < 32; ++L) t.lut[at + L] = static_cast<uint16_t>(L | (L << 5) | (L << 10));
          m = static_cast<uint32_t>(at / 32) + 1;
        }
        irow[x] = m;
        ix0 = std::min(ix0, x);
        ix1 = std::max(ix1, x + 1);
        iy0 = std::min(iy0, y);
        iy1 = std::max(iy1, y + 1);
      }
    }
    if (ix1 <= ix0) return true;
    t.x0 = b.x0 + ix0;
    t.x1 = b.x0 + ix1;
    t.y0 = b.y0 + iy0;
    t.y1 = b.y0 + iy1;
    const int32_t tw = ix1 - ix0;
    t.idx.resize(static_cast<size_t>(tw) * static_cast<size_t>(iy1 - iy0));
    for (int32_t y = iy0; y < iy1; ++y)
      std::memcpy(t.idx.data() + static_cast<size_t>(y - iy0) * static_cast<size_t>(tw),
                  idx.data() + static_cast<size_t>(y) * static_cast<size_t>(bw) + static_cast<size_t>(ix0), static_cast<size_t>(tw) * 4u);
    return true;
  }
#ifdef _WIN32
  Prep p;
  if (!prep(r, vp, dx, cw, ch, p)) return false;
  // the pixels draw may write: inside the box (and the DIB's rectangle, which lies in the client) and visible in the layer
  const int32_t x0 = std::max(p.bx0, p.dx0), x1 = std::min(p.bx1, p.dx1), y0 = std::max(p.by0, p.dy0), y1 = std::min(p.by1, p.dy1);
  if (x1 <= x0 || y1 <= y0) return true;
  const int32_t bw = x1 - x0, bh = y1 - y0;
  std::vector<uint8_t> vis(static_cast<size_t>(bw) * static_cast<size_t>(bh), 0);
  for (int32_t y = y0; y < y1; ++y) {
    const int32_t ly = floor_div(static_cast<int64_t>(y - vp.origin_y) * kLogicalH, vp.client_h);
    for (int32_t x = x0; x < x1; ++x) {
      const int32_t lx = floor_div(static_cast<int64_t>(x - vp.origin_x - dx) * kLogicalW, vp.client_w);
      vis[static_cast<size_t>(y - y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - x0)] = r.visible(lx, ly, layered) ? 1 : 0;
    }
  }
  // res[L][box pixel]: GDI's word over level L
  std::vector<uint16_t> res(static_cast<size_t>(32) * vis.size());
  // P24C fix 2b: from the string's kept level pictures (Impl::Raster), drawn again only when they do not hold this box. The box
  // relative to the origin: client (x, y) is the origin plus (x - gx, y - gy)
  const int32_t gx = p.dx0 + p.ox, gy = p.dy0 + p.oy;
  const int32_t nx0 = x0 - gx, ny0 = y0 - gy, nx1 = x1 - gx, ny1 = y1 - gy;
  std::string key(reinterpret_cast<const char *>(&p.f), sizeof p.f);
  key.append(reinterpret_cast<const char *>(&p.ref), sizeof p.ref);
  key += r.text;
  key += '\0';
  if (!p.adv.empty()) key.append(reinterpret_cast<const char *>(p.adv.data()), p.adv.size() * sizeof(int));
  Impl::Raster *ras = nullptr;
  {
    auto it = impl_->rasters.find(key);
    if (it != impl_->rasters.end() && it->second.rx0 <= nx0 && it->second.ry0 <= ny0 && it->second.rx0 + it->second.rw >= nx1 &&
        it->second.ry0 + it->second.rh >= ny1)
      ras = &it->second;
  }
  if (!ras) {
    // the box and the kept rectangle (if any), and 8 pixels more each side for the next frames' moves of the box around the origin
    int32_t ax0 = nx0 - 8, ay0 = ny0 - 8, ax1 = nx1 + 8, ay1 = ny1 + 8;
    auto it = impl_->rasters.find(key);
    if (it != impl_->rasters.end()) {
      ax0 = std::min(ax0, it->second.rx0);
      ay0 = std::min(ay0, it->second.ry0);
      ax1 = std::max(ax1, it->second.rx0 + it->second.rw);
      ay1 = std::max(ay1, it->second.ry0 + it->second.rh);
    }
    const int32_t rw = ax1 - ax0, rh = ay1 - ay0;
    if (rw <= 0 || rh <= 0 || rw > 8192 || rh > 8192 || !impl_->dib(rw, rh)) {
      ++stats.failed;
      return false;
    }
    if (impl_->rasters.size() > 512) impl_->rasters.clear();
    Impl::Raster nr;
    nr.rx0 = ax0;
    nr.ry0 = ay0;
    nr.rw = rw;
    nr.rh = rh;
    nr.res.resize(static_cast<size_t>(32) * static_cast<size_t>(rw) * static_cast<size_t>(rh));
    const size_t pitch = static_cast<size_t>((impl_->w + 1) & ~1);
    uint16_t *bits = static_cast<uint16_t *>(impl_->bits);
    Prep q = p;  // the origin at (-ax0, -ay0) of the DIB
    q.ox = -ax0;
    q.oy = -ay0;
    for (uint32_t L = 0; L < 32; ++L) {
      const uint16_t g = static_cast<uint16_t>(L | (L << 5) | (L << 10));
      GdiFlush();
      for (int32_t y = 0; y < rh; ++y) {
        uint16_t *row = bits + pitch * static_cast<size_t>(y);
        for (int32_t x = 0; x < rw; ++x) row[x] = g;
      }
      gdi(r, q);
      ++table_draws;
      uint16_t *dst = nr.res.data() + static_cast<size_t>(L) * static_cast<size_t>(rw) * static_cast<size_t>(rh);
      for (int32_t y = 0; y < rh; ++y)
        std::memcpy(dst + static_cast<size_t>(y) * static_cast<size_t>(rw), bits + pitch * static_cast<size_t>(y), static_cast<size_t>(rw) * 2u);
    }
    ras = &(impl_->rasters[key] = std::move(nr));
    ++raster_draws;
  } else {
    ++raster_reuses;
  }
  for (uint32_t L = 0; L < 32; ++L) {
    const uint16_t g = static_cast<uint16_t>(L | (L << 5) | (L << 10));
    uint16_t *dst = res.data() + static_cast<size_t>(L) * vis.size();
    if (L >= levels) {  // a level the caller never looks up: the word itself (no ink)
      std::fill(dst, dst + vis.size(), g);
      continue;
    }
    const uint16_t *src = ras->res.data() + static_cast<size_t>(L) * static_cast<size_t>(ras->rw) * static_cast<size_t>(ras->rh);
    for (int32_t y = y0; y < y1; ++y)
      std::memcpy(dst + static_cast<size_t>(y - y0) * static_cast<size_t>(bw),
                  src + static_cast<size_t>(y - gy - ras->ry0) * static_cast<size_t>(ras->rw) + static_cast<size_t>(x0 - gx - ras->rx0),
                  static_cast<size_t>(bw) * 2u);
  }
  // the entries (one per distinct 32-word column), and the ink's box. First the pixels with ink (some level's word differs from
  // the level), level by level over the box; then each ink pixel's column, found again by its hash (the words compared)
  std::vector<uint16_t> diff(vis.size(), 0);
  for (uint32_t L = 0; L < 32; ++L) {
    const uint16_t g = static_cast<uint16_t>(L | (L << 5) | (L << 10));
    const uint16_t *src = res.data() + static_cast<size_t>(L) * vis.size();
    for (size_t i = 0; i < vis.size(); ++i) diff[i] |= static_cast<uint16_t>(src[i] ^ g);
  }
  std::unordered_multimap<uint64_t, uint32_t> ids;
  std::vector<uint32_t> idx(vis.size(), 0);
  int32_t ix0 = bw, ix1 = -1, iy0 = bh, iy1 = -1;
  uint16_t e[32];
  for (size_t i = 0; i < vis.size(); ++i) {
    if (!vis[i] || diff[i] == 0) continue;
    uint64_t hsh = 1469598103934665603ull;  // FNV-1a over the 32 words
    for (uint32_t L = 0; L < 32; ++L) {
      e[L] = res[static_cast<size_t>(L) * vis.size() + i];
      hsh = (hsh ^ e[L]) * 1099511628211ull;
    }
    uint32_t k = ~0u;
    for (auto r2 = ids.equal_range(hsh); r2.first != r2.second; ++r2.first)
      if (std::memcmp(t.lut.data() + static_cast<size_t>(r2.first->second) * 32u, e, sizeof e) == 0) {
        k = r2.first->second;
        break;
      }
    if (k == ~0u) {
      k = static_cast<uint32_t>(t.lut.size() / 32);
      ids.emplace(hsh, k);
      t.lut.insert(t.lut.end(), e, e + 32);
    }
    idx[i] = k + 1;
    const int32_t xx = static_cast<int32_t>(i % static_cast<size_t>(bw)), yy = static_cast<int32_t>(i / static_cast<size_t>(bw));
    ix0 = std::min(ix0, xx);
    ix1 = std::max(ix1, xx + 1);
    iy0 = std::min(iy0, yy);
    iy1 = std::max(iy1, yy + 1);
  }
  if (ix1 <= ix0) return true;  // no ink: nothing to write
  t.x0 = x0 + ix0;
  t.x1 = x0 + ix1;
  t.y0 = y0 + iy0;
  t.y1 = y0 + iy1;
  const int32_t tw = ix1 - ix0;
  t.idx.resize(static_cast<size_t>(tw) * static_cast<size_t>(iy1 - iy0));
  for (int32_t y = iy0; y < iy1; ++y)
    std::memcpy(t.idx.data() + static_cast<size_t>(y - iy0) * static_cast<size_t>(tw),
                idx.data() + static_cast<size_t>(y) * static_cast<size_t>(bw) + static_cast<size_t>(ix0), static_cast<size_t>(tw) * 4u);
  return true;
#else
  (void)r; (void)layered; (void)vp; (void)dx; (void)cw; (void)ch;
  return false;
#endif
}


}  // namespace portapp
