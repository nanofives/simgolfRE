// P41W (present.h set_repaint / wide_backdrop_now; portsim/sim/gfx_uvwide.h; log/roadmap/P41/portwide/notes.md): a front-end
// screen's wide backdrop drawn by the presenter.
//
// The sim marks a frame whose back buffer shows a wide backdrop (GfxTextFrame::backdrop, set while at least a quarter of the back
// buffer still holds the backdrop's 1x words). Such a frame is painted as:
//   1. the backdrop over the whole client, rendered at the client's resolution in 8 bits per channel (uw_render_bgra: the scene's
//      frame point of client pixel (x, y) is ((x + 0.5 - origin_x) / sx, (y + 0.5 - origin_y) / sy) with sx = content_w / 800,
//      sy = content_h / 600: the 800 x 600 composition stays the content rectangle, the backdrop runs on to the client's edges);
//   2. the frame's own pixels where they are FOREIGN: neither the backdrop's 1x word at that pixel nor covered by a visible UI run's
//      scale-1 drawing (what the game drew there that is not ours: a picture left as art, a dialog), scaled as the frame is (smooth:
//      bilinear over the foreign pixels only, premultiplied by the foreign mask, over the backdrop; nearest: the pixel itself);
//   3. the runs: UI runs (the title's buttons, its logo picture) composed in 8 bits (TextRaster::set_true_colour), glyph runs as
//      always, also at 800 x 600 (the frame under them is the text-clean one).
// Nothing else (no side bands). The mouse keeps the content rectangle's mapping (input_map.cpp), the GPU path draws the whole
// client as one CPU patch (gpu.cpp paint_flat), and the theme off never sets the backdrop (the original's calls, the old path).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "present.h"
#include "portsim/sim/gfx_ui.h"
#include "portsim/sim/gfx_uvwide.h"

namespace portapp {

namespace {
constexpr int64_t kSyncPx = 1100000;   // a client up to this many pixels renders its full backdrop at once
constexpr double kQuickScale = 0.5;    // the quick render's target pixels per frame unit
int64_t now_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// the content rectangle's mapping of client pixels: the backdrop over [0, cw) x content rows
void render_client(int32_t k, int32_t cw, const Viewport &vp, double f, int32_t w, int32_t h, std::vector<uint32_t> &out) {
  const double sx = static_cast<double>(vp.client_w) / kLogicalW * f, sy = static_cast<double>(vp.client_h) / kLogicalH * f;
  (void)cw;
  portsim::uw_render_bgra(k, sx, sy, vp.origin_x * f, 0.0, 0, 0, w, h, out);
}
// the quick render (qw x qh, pixel centre (i + 0.5) / f client pixels) upsampled bilinearly to w x h, 8-bit weights, rows on the
// machine's threads
void upsample(const std::vector<uint32_t> &q, int32_t qw, int32_t qh, double f, std::vector<uint32_t> &out, int32_t w, int32_t h) {
  std::vector<int32_t> u0(static_cast<size_t>(w)), u1(static_cast<size_t>(w)), fu(static_cast<size_t>(w));
  for (int32_t x = 0; x < w; ++x) {
    const double u = (x + 0.5) * f - 0.5;
    const int32_t a = std::clamp(static_cast<int32_t>(std::floor(u)), 0, qw - 1);
    u0[static_cast<size_t>(x)] = a;
    u1[static_cast<size_t>(x)] = std::min(qw - 1, a + 1);
    fu[static_cast<size_t>(x)] = static_cast<int32_t>(std::clamp(u - a, 0.0, 1.0) * 256.0 + 0.5);
  }
  auto rows = [&](int32_t ya, int32_t yb) {
    for (int32_t y = ya; y < yb; ++y) {
      const double v = (y + 0.5) * f - 0.5;
      const int32_t v0 = std::clamp(static_cast<int32_t>(std::floor(v)), 0, qh - 1), v1 = std::min(qh - 1, v0 + 1);
      const int32_t fv = static_cast<int32_t>(std::clamp(v - v0, 0.0, 1.0) * 256.0 + 0.5);
      const uint32_t *r0 = q.data() + static_cast<size_t>(v0) * qw, *r1 = q.data() + static_cast<size_t>(v1) * qw;
      uint32_t *o = out.data() + static_cast<size_t>(y) * w;
      for (int32_t x = 0; x < w; ++x) {
        const uint32_t a = r0[u0[static_cast<size_t>(x)]], b = r0[u1[static_cast<size_t>(x)]], c = r1[u0[static_cast<size_t>(x)]],
                       d = r1[u1[static_cast<size_t>(x)]];
        const int32_t wx = fu[static_cast<size_t>(x)];
        uint32_t px = 0xff000000u;
        for (int sh = 0; sh <= 16; sh += 8) {
          const int32_t top = static_cast<int32_t>((a >> sh) & 255) * (256 - wx) + static_cast<int32_t>((b >> sh) & 255) * wx;
          const int32_t bot = static_cast<int32_t>((c >> sh) & 255) * (256 - wx) + static_cast<int32_t>((d >> sh) & 255) * wx;
          px |= static_cast<uint32_t>((top * (256 - fv) + bot * fv + 32768) >> 16) << sh;
        }
        o[x] = px;
      }
    }
  };
  const int32_t n = static_cast<int32_t>(std::min<unsigned>(std::max(1u, std::thread::hardware_concurrency()), 16u));
  const int32_t band = (h + n - 1) / n;
  std::vector<std::thread> ts;
  for (int32_t t = 1; t < n; ++t)
    if (t * band < h) ts.emplace_back(rows, t * band, std::min(h, (t + 1) * band));
  rows(0, std::min(h, band));
  for (std::thread &t : ts) t.join();
}
}  // namespace

struct Presenter::WideBd {
  std::mutex mu;
  int32_t k = -1, cw = 0, ch = 0;
  Viewport vp;
  std::vector<uint32_t> img;  // cw x content rows (row 0 = the content's first row): the best render so far
  int32_t level = 0;          // 1 the quick render upsampled, 2 the full one
  uint64_t serial = 0, job = 0;
  int64_t quick_us = 0, full_us = 0, started_us = 0, full_at_us = 0;
  uint64_t renders = 0;
  // P41 feedback1 items 3 / 12: full renders kept per screen for the current client (a screen shown again, or rendered in the
  // background before it is shown, needs no quick render), newest last; and whether the background renderer runs
  struct Kept {
    int32_t k = -1;
    std::shared_ptr<const std::vector<uint32_t>> img;
  };
  std::vector<Kept> kept;
  int32_t kept_cw = 0, kept_ch = 0;
  Viewport kept_vp;
  bool prefetching = false;
  uint64_t kept_hits = 0, prefetched = 0;
};

namespace {
constexpr size_t kKeptMax = 8;   // screens kept (a 4K client: 33 MB each)
bool same_vp(const Viewport &a, const Viewport &b) {
  return a.origin_x == b.origin_x && a.origin_y == b.origin_y && a.client_w == b.client_w && a.client_h == b.client_h;
}
// P41 feedback1: SG_P41W_PREFETCH=0 (development) keeps no renders and renders nothing in the background (as before)
bool prefetch_on() {
  static const bool on = [] {
    const char *e = std::getenv("SG_P41W_PREFETCH");
    return !(e && e[0] == '0');
  }();
  return on;
}
}  // namespace

int32_t Presenter::wide_backdrop_screen() const {
  // (a world layer or HUD layers may come with the frame: the golfer editor opens in game, over the course frame's layers; the sim
  // marks the backdrop only while it covers at least a quarter of the back buffer, so the frame is that screen's)
  if (!text_ok_ || text_.backdrop.empty() || frame_.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return -1;
  const int32_t k = portsim::uw_screen_find(text_.backdrop);
  if (k < 0 || !portsim::uw_prepared(k)) return -1;
  return k;
}

bool Presenter::wide_backdrop_now(int32_t cw, int32_t ch) const { return cw > 0 && ch > 0 && probe_ == 0 && wide_backdrop_screen() >= 0; }

uint64_t Presenter::wide_backdrop_serial() const {
  if (!wbd_) return 0;
  std::lock_guard<std::mutex> l(wbd_->mu);
  return wbd_->serial;
}

std::string Presenter::wide_backdrop_json() const {
  if (!wbd_) return "{}";
  std::lock_guard<std::mutex> l(wbd_->mu);
  char b[384];
  std::snprintf(b, sizeof b, "{\"screen\":\"%s\",\"client\":[%d,%d],\"level\":%d,\"quick_ms\":%.1f,\"full_ms\":%.1f,\"first_full_ms\":%.1f,\"renders\":%llu,\"kept_hits\":%llu,\"prefetched\":%llu}",
                portsim::uw_screen_name(wbd_->k), wbd_->cw, wbd_->ch, wbd_->level, wbd_->quick_us / 1000.0, wbd_->full_us / 1000.0,
                wbd_->full_at_us > 0 ? (wbd_->full_at_us - wbd_->started_us) / 1000.0 : -1.0, static_cast<unsigned long long>(wbd_->renders),
                static_cast<unsigned long long>(wbd_->kept_hits), static_cast<unsigned long long>(wbd_->prefetched));
  return b;
}

bool Presenter::wide_backdrop(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  if (probe_ != 0 || course_only_) return false;
  const int32_t k = wide_backdrop_screen();
  if (k < 0 || vp.client_w <= 0 || vp.client_h <= 0) return false;
  const auto bd = portsim::uw_frame555(k);
  if (!bd || bd->size() != frame_.size()) return false;
  const Canvas C = cv_;
  if (C.full && !quiet_) {  // as render_impl's flat path: no course picture kept, no moved panel
    scene_ok_ = prev_ok_ = false;
    painted_cam_ = CameraSnap{};
    if (remap_ && defect_ != PresentDefect::StaleRemap) remap_->clear();   // no moved panel on these screens (also in game)
  }
  // 1. the backdrop: the kept render of this screen and client, else a new one (quick now and the full one on a worker)
  if (!wbd_) wbd_ = std::make_shared<WideBd>();
  std::shared_ptr<WideBd> W = wbd_;
  std::unique_lock<std::mutex> lk(W->mu);
  // P41 feedback1 items 3 / 12 (Mariano: the routing map's mode buttons pixelated after each mode change, the Load Game backdrop
  // changing as it loads): a screen's full render took 0.3 s (Load Game) to 1.8 s (a routing-map mode) at 2560 x 1380 and 4.7 s at 4K,
  // and every change of screen showed the quick render (0.5 px per frame unit, upsampled: the buttons the backdrop places blurred) until
  // it was in. Now the full renders are kept per screen for the client, and once the shown screen's full render is in, its prepared
  // siblings (gfx_uvwide.h uw_screen_family: the four modes, the front-end screens) are rendered one after another in the background.
  auto keep = [](WideBd &B, int32_t kk, int32_t kcw, int32_t kch, const Viewport &kvp, std::shared_ptr<const std::vector<uint32_t>> img) {
    if (!prefetch_on()) return;
    if (B.kept_cw != kcw || B.kept_ch != kch || !same_vp(B.kept_vp, kvp)) {
      B.kept.clear();
      B.kept_cw = kcw;
      B.kept_ch = kch;
      B.kept_vp = kvp;
    }
    B.kept.erase(std::remove_if(B.kept.begin(), B.kept.end(), [&](const WideBd::Kept &e) { return e.k == kk; }), B.kept.end());
    B.kept.push_back({kk, std::move(img)});
    if (B.kept.size() > kKeptMax) B.kept.erase(B.kept.begin());
  };
  // under W->mu: the background renderer for kk's prepared siblings not yet kept (one thread, one screen at a time; it stops when
  // the client changes); called once kk's full render is in
  auto prefetch = [keep](std::shared_ptr<WideBd> B, int32_t kk, int32_t kcw, int32_t kch, int32_t rows, const Viewport &kvp) {
    if (!prefetch_on() || B->prefetching) return;
    std::vector<int32_t> todo;
    const int32_t fam = portsim::uw_screen_family(kk);
    for (int32_t j = 0; j < portsim::uw_screen_count(); ++j) {
      if (j == kk || fam < 0 || portsim::uw_screen_family(j) != fam || !portsim::uw_prepared(j)) continue;
      bool have = false;
      for (const WideBd::Kept &e : B->kept) have = have || e.k == j;
      if (!have) todo.push_back(j);
    }
    if (todo.empty()) return;
    if (todo.size() > kKeptMax - 1) todo.resize(kKeptMax - 1);
    B->prefetching = true;
    std::thread([B, todo, kcw, kch, rows, kvp, keep]() {
      for (int32_t j : todo) {
        {
          std::lock_guard<std::mutex> l(B->mu);
          if (B->cw != kcw || B->ch != kch || !same_vp(B->vp, kvp)) break;   // the client changed
        }
        auto full = std::make_shared<std::vector<uint32_t>>();
        render_client(j, kcw, kvp, 1.0, kcw, rows, *full);
        std::lock_guard<std::mutex> l(B->mu);
        if (B->cw != kcw || B->ch != kch || !same_vp(B->vp, kvp)) break;
        keep(*B, j, kcw, kch, kvp, full);
        ++B->prefetched;
      }
      std::lock_guard<std::mutex> l(B->mu);
      B->prefetching = false;
    }).detach();
  };
  if (W->k != k || W->cw != cw || W->ch != ch || W->vp.origin_x != vp.origin_x || W->vp.origin_y != vp.origin_y ||
      W->vp.client_w != vp.client_w || W->vp.client_h != vp.client_h) {
    W->k = k;
    W->cw = cw;
    W->ch = ch;
    W->vp = vp;
    const uint64_t job = ++W->job;
    W->started_us = now_us();
    W->full_at_us = 0;
    const int32_t rows = vp.client_h;
    std::shared_ptr<const std::vector<uint32_t>> have;
    if (prefetch_on() && W->kept_cw == cw && W->kept_ch == ch && same_vp(W->kept_vp, vp))
      for (const WideBd::Kept &e : W->kept)
        if (e.k == k) have = e.img;
    if (have && have->size() == static_cast<size_t>(cw) * static_cast<size_t>(rows)) {   // kept or rendered in the background
      W->img = *have;
      W->full_us = 0;
      W->full_at_us = now_us();
      W->level = 2;
      ++W->kept_hits;
      prefetch(W, k, cw, ch, rows, vp);
    } else if (static_cast<int64_t>(cw) * rows <= kSyncPx) {
      const int64_t t0 = now_us();
      render_client(k, cw, vp, 1.0, cw, rows, W->img);
      W->full_us = now_us() - t0;
      W->full_at_us = now_us();
      W->level = 2;
      keep(*W, k, cw, ch, vp, std::make_shared<const std::vector<uint32_t>>(W->img));
      prefetch(W, k, cw, ch, rows, vp);
    } else {
      const double sc = static_cast<double>(vp.client_h) / kLogicalH;
      const double f = std::min(1.0, kQuickScale / sc);
      const int32_t qw = static_cast<int32_t>(std::ceil(cw * f)) + 1, qh = static_cast<int32_t>(std::ceil(rows * f)) + 1;
      std::vector<uint32_t> q;
      const int64_t t0 = now_us();
      render_client(k, cw, vp, f, qw, qh, q);
      W->img.assign(static_cast<size_t>(cw) * static_cast<size_t>(rows), 0xff000000u);
      upsample(q, qw, qh, f, W->img, cw, rows);
      W->quick_us = now_us() - t0;
      W->level = 1;
      std::function<void()> rp = repaint_;
      std::thread([W, job, k, cw, ch, rows, vp, rp, keep, prefetch]() {
        std::vector<uint32_t> full;
        const int64_t t1 = now_us();
        render_client(k, cw, vp, 1.0, cw, rows, full);
        const int64_t dt = now_us() - t1;
        {
          std::lock_guard<std::mutex> l(W->mu);
          if (W->cw == cw && W->ch == ch && same_vp(W->vp, vp))   // kept even when the screen changed meanwhile (same client)
            keep(*W, k, cw, ch, vp, std::make_shared<const std::vector<uint32_t>>(full));
          if (W->job != job) return;   // the screen or the client changed meanwhile
          W->img.swap(full);
          W->level = 2;
          W->full_us = dt;
          W->full_at_us = now_us();
          ++W->serial;
          ++W->renders;
          prefetch(W, k, cw, ch, rows, vp);
        }
        if (rp) rp();
      }).detach();
    }
    ++W->serial;
    ++W->renders;
  }
  for (int32_t y = std::max(C.y0, vp.origin_y); y < std::min(C.y0 + C.h, vp.origin_y + vp.client_h); ++y) {
    const int32_t x0 = std::max(C.x0, 0), x1 = std::min(C.x0 + C.w, cw);
    if (x1 <= x0) continue;
    std::memcpy(&out[static_cast<size_t>(y - C.y0) * static_cast<size_t>(C.w) + static_cast<size_t>(x0 - C.x0)],
                &W->img[static_cast<size_t>(y - vp.origin_y) * static_cast<size_t>(cw) + static_cast<size_t>(x0)],
                static_cast<size_t>(x1 - x0) * 4u);
  }
  lk.unlock();
  // 2. the foreign pixels of the frame
  // the runs are always drawn here, also at 800 x 600 (where the flat path takes the frame's own glyphs: TextRaster draws them at
  // scale 1 with the game's font, the frame's pixels), so the picture under them is the text-clean frame: a glyph over a UI run is
  // not a foreign pixel there (the peel finds the run visible under it), and taken from the frame it was lost under the run's 8-bit
  // drawing (the title's labels at 800 x 600, 2026-11-25)
  const std::vector<uint16_t> &src = text_defect_ != TextDefect::Unremoved ? text_.clean : frame_;
  if (!raster_) raster_.reset(new TextRaster());
  raster_->set_defect(text_defect_);
  raster_->set_aa(text_aa_);
  raster_->set_fit(text_fit_);
  if (wbd_mask_gen_ != content_gen_ || wbd_mask_.size() != src.size()) {
    wbd_mask_.assign(src.size(), 0);
    for (size_t i = 0; i < src.size(); ++i) wbd_mask_[i] = src[i] != (*bd)[i] ? 1 : 0;
    for (const portsim::GfxTextRun &r : text_.runs) {
      int32_t fx0, fy0, fw, fh;
      const uint8_t *cov = nullptr;
      if (!raster_->ui_cover(r, fx0, fy0, fw, fh, cov)) continue;
      for (int32_t y = std::max(0, fy0); y < std::min(kLogicalH, fy0 + fh); ++y)
        for (int32_t x = std::max(0, fx0); x < std::min(kLogicalW, fx0 + fw); ++x)
          if (cov[static_cast<size_t>(y - fy0) * static_cast<size_t>(fw) + static_cast<size_t>(x - fx0)] && r.visible(x, y, false))
            wbd_mask_[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)] = 0;
    }
    wbd_rows_.assign(kLogicalH, 0);
    for (int32_t y = 0; y < kLogicalH; ++y)
      for (int32_t x = 0; x < kLogicalW && !wbd_rows_[static_cast<size_t>(y)]; ++x) wbd_rows_[static_cast<size_t>(y)] = wbd_mask_[static_cast<size_t>(y) * kLogicalW + x];
    wbd_mask_gen_ = content_gen_;
  }
  const bool smooth = effective_scale(vp.client_w, vp.client_h) == ScaleMode::Smooth;
  const int32_t cx0 = std::max(C.x0, vp.origin_x), cx1 = std::min(C.x0 + C.w, vp.origin_x + vp.client_w);
  for (int32_t y = std::max(C.y0, vp.origin_y); y < std::min(C.y0 + C.h, vp.origin_y + vp.client_h); ++y) {
    uint32_t *row = &out[static_cast<size_t>(y - C.y0) * static_cast<size_t>(C.w)] - C.x0;
    if (!smooth) {
      const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp.origin_y) * kLogicalH / vp.client_h);
      if (!wbd_rows_[static_cast<size_t>(ly)]) continue;
      for (int32_t x = cx0; x < cx1; ++x) {
        const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp.origin_x) * kLogicalW / vp.client_w);
        const size_t i = static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx);
        if (wbd_mask_[i]) row[x] = rgb555_to_bgra(src[i]);
      }
      continue;
    }
    const double fy = (y - vp.origin_y + 0.5) * kLogicalH / vp.client_h - 0.5;
    const int32_t y0 = std::clamp(static_cast<int32_t>(std::floor(fy)), 0, kLogicalH - 1), y1 = std::min(kLogicalH - 1, y0 + 1);
    const double wy = std::clamp(fy - std::floor(fy), 0.0, 1.0);
    if (!wbd_rows_[static_cast<size_t>(y0)] && !wbd_rows_[static_cast<size_t>(y1)]) continue;
    for (int32_t x = cx0; x < cx1; ++x) {
      const double fx = (x - vp.origin_x + 0.5) * kLogicalW / vp.client_w - 0.5;
      const int32_t x0 = std::clamp(static_cast<int32_t>(std::floor(fx)), 0, kLogicalW - 1), x1 = std::min(kLogicalW - 1, x0 + 1);
      const double wx = std::clamp(fx - std::floor(fx), 0.0, 1.0);
      const size_t i00 = static_cast<size_t>(y0) * kLogicalW + x0, i01 = static_cast<size_t>(y0) * kLogicalW + x1;
      const size_t i10 = static_cast<size_t>(y1) * kLogicalW + x0, i11 = static_cast<size_t>(y1) * kLogicalW + x1;
      const double w4[4] = {(1 - wx) * (1 - wy) * wbd_mask_[i00], wx * (1 - wy) * wbd_mask_[i01], (1 - wx) * wy * wbd_mask_[i10],
                            wx * wy * wbd_mask_[i11]};
      const double A = w4[0] + w4[1] + w4[2] + w4[3];
      if (A <= 0) continue;
      const uint32_t c4[4] = {rgb555_to_bgra(src[i00]), rgb555_to_bgra(src[i01]), rgb555_to_bgra(src[i10]), rgb555_to_bgra(src[i11])};
      uint32_t o = 0xff000000u;
      for (int sh = 0; sh <= 16; sh += 8) {
        double v = ((row[x] >> sh) & 255) * (1 - A);
        for (int q = 0; q < 4; ++q) v += w4[q] * ((c4[q] >> sh) & 255);
        o |= static_cast<uint32_t>(std::min(255.0, v + 0.5)) << sh;
      }
      row[x] = o;
    }
  }
  // 3. the runs: UI runs in 8 bits, then the glyph runs
  Viewport vc = vp;
  vc.origin_x -= cv_.x0;
  vc.origin_y -= cv_.y0;
  raster_->set_true_colour(true);
  for (const portsim::GfxTextRun &r : text_.runs) {
    if (r.target == 2) continue;
    raster_->draw(r, false, vc, 0, out, cv_.w, cv_.h);
  }
  raster_->set_true_colour(false);
  (void)ch;
  return true;
}

}  // namespace portapp
