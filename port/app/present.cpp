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

namespace portapp {

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
  const bool ok = set_frame(px, band);
  if (ok && !stale) {
    const bool sane = !world.empty() && world.pw > 0 && world.ph > 0 &&
                      world.px.size() == static_cast<size_t>(world.pw) * static_cast<size_t>(world.ph) &&
                      world.mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH && world.fw > 0;
    world_ = sane ? std::move(world) : WorldImage{};
  }
  return ok;
}

// ---- G32: PanelRemap
void PanelRemap::set(bool on, const Viewport &vp, int32_t cw, const std::vector<uint8_t> &mask) {
  std::lock_guard<std::mutex> lk(mu_);
  on_ = on && vp.origin_x > 0 && mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
  vp_ = vp;
  cw_ = cw;
  if (on_) mask_ = mask;
}
void PanelRemap::clear() {
  std::lock_guard<std::mutex> lk(mu_);
  on_ = false;
}
uint64_t PanelRemap::remapped() const {
  std::lock_guard<std::mutex> lk(mu_);
  return remapped_;
}
void PanelRemap::map(int32_t &x, int32_t &y) const {
  std::lock_guard<std::mutex> lk(mu_);
  if (!on_ || vp_.client_w <= 0 || vp_.client_h <= 0) return;
  const int32_t dxl = -vp_.origin_x, dxr = cw_ - vp_.origin_x - vp_.client_w;
  for (const HudPanel &p : kHudPanels) {
    const int32_t sx = x - (p.anchor < 0 ? dxl : dxr);
    if (sx < vp_.origin_x || sx >= vp_.origin_x + vp_.client_w || y < vp_.origin_y || y >= vp_.origin_y + vp_.client_h) continue;
    const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(sx - vp_.origin_x) * kLogicalW / vp_.client_w);
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp_.origin_y) * kLogicalH / vp_.client_h);
    if (lx < p.x0 || lx >= p.x1 || ly < p.y0 || ly >= p.y1) continue;
    if (mask_[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)]) continue;  // the world shows there
    x = sx;
    ++remapped_;
    return;
  }
}

bool Presenter::set_frame(const std::vector<uint16_t> &px, BandFill band) {
  if (px.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return false;
  ++frames_set_;
  if (defect_ == PresentDefect::Stale && !frame_.empty()) return true;
  frame_ = px;
  band_ = band;
  world_ = WorldImage{};  // P8B: a frame set without its world layer has none
  if (defect_ == PresentDefect::ByteSwap)
    for (uint16_t &v : frame_) v = static_cast<uint16_t>((v >> 8) | (v << 8));
  return true;
}

ScaleMode Presenter::effective_scale(int32_t w, int32_t h) const {
  if (scale_ != ScaleMode::Auto) return scale_;
  const bool integer = w > 0 && w % kLogicalW == 0 && h == w / kLogicalW * kLogicalH;
  return integer ? ScaleMode::Nearest : ScaleMode::Smooth;
}

void Presenter::render(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vpo) const {
  const Viewport vp = fit_viewport(cw, ch);
  if (vpo) *vpo = vp;
  out.assign(static_cast<size_t>(cw > 0 ? cw : 0) * static_cast<size_t>(ch > 0 ? ch : 0), 0xff000000u);
  if (frame_.empty() || cw <= 0 || ch <= 0) return;
  if (effective_scale(vp.client_w, vp.client_h) == ScaleMode::Smooth && vp.client_w > 0 && vp.client_h > 0) {
    // P5B item 4: bilinear. The frame converted to BGRA once, then per client pixel four neighbours with 8-bit weights.
    std::vector<uint32_t> src(frame_.size());
    for (size_t i = 0; i < frame_.size(); ++i) src[i] = rgb555_to_bgra(frame_[i]);
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
    for (int32_t y = 0; y < vp.client_h; ++y) {
      const Tap &v = ty[static_cast<size_t>(y)];
      const uint32_t *s0 = src.data() + static_cast<size_t>(v.i0) * kLogicalW;
      const uint32_t *s1 = src.data() + static_cast<size_t>(v.i1) * kLogicalW;
      uint32_t *dst = out.data() + static_cast<size_t>(y + vp.origin_y) * static_cast<size_t>(cw) + vp.origin_x;
      for (int32_t x = 0; x < vp.client_w; ++x) {
        const Tap &u = tx[static_cast<size_t>(x)];
        const uint32_t a = mix(s0[u.i0], s0[u.i1], u.f), b = mix(s1[u.i0], s1[u.i1], u.f);
        dst[x] = mix(a, b, v.f) | 0xff000000u;
      }
    }
    if (!world_.empty()) compose_world(out, cw, ch, vp);  // P8B
    else if (wide_ != WideFill::Off && band_ == BandFill::Extend) fill_bands(out, cw, ch, vp);  // P7
    return;
  }
  std::vector<int32_t> col(static_cast<size_t>(vp.client_w));
  for (int32_t x = 0; x < vp.client_w; ++x)
    col[static_cast<size_t>(x)] = static_cast<int32_t>(static_cast<int64_t>(x) * kLogicalW / vp.client_w);
  for (int32_t y = 0; y < vp.client_h; ++y) {
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y) * kLogicalH / vp.client_h);
    const uint16_t *src = frame_.data() + static_cast<size_t>(ly) * kLogicalW;
    uint32_t *dst = out.data() + static_cast<size_t>(y + vp.origin_y) * static_cast<size_t>(cw) + vp.origin_x;
    for (int32_t x = 0; x < vp.client_w; ++x) dst[x] = rgb555_to_bgra(src[col[static_cast<size_t>(x)]]);
  }
  if (!world_.empty()) compose_world(out, cw, ch, vp);  // P8B
  else if (wide_ != WideFill::Off && band_ == BandFill::Extend) fill_bands(out, cw, ch, vp);  // P7
}

// P8B: the world layer under the frame. Client pixel (x, y) maps to world (ox + (x - vp.origin_x) * fw / vp.client_w,
// (y - vp.origin_y) * ph / vp.client_h): the identity when the world was made at this client size. Inside the content rectangle
// the frame's pixel stays where the mask of its logical pixel (floor((x - origin) * 800 / client_w), likewise y) is 0. The world
// is sampled at the nearest pixel when the mapping is 1:1 or the scale mode resolves to Nearest, else bilinear (8-bit weights).
void Presenter::compose_world(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  const WorldImage &w = world_;
  if (vp.client_w <= 0 || vp.client_h <= 0 || cw <= 0 || ch <= 0) return;
  // G32: the HUD compositor. The frame's image as drawn (frame_img), the mask with every moved panel's 2D pixels vacated (they show
  // the world), then each panel's 2D pixels pasted at their anchored place
  const bool anchor = hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved;
  if (remap_) remap_->set(anchor, vp, cw, w.mask);
  std::vector<uint32_t> frame_img;
  std::vector<uint8_t> mask_moved;
  const uint8_t *maskp = w.mask.data();
  if (anchor) {
    frame_img = out;
    mask_moved = w.mask;
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
  std::vector<int64_t> wx(static_cast<size_t>(cw)), wy(static_cast<size_t>(ch));
  for (int32_t x = 0; x < cw; ++x) wx[static_cast<size_t>(x)] = pos(x, vp.origin_x, vp.client_w, w.fw, w.ox);
  for (int32_t y = 0; y < ch; ++y) wy[static_cast<size_t>(y)] = pos(y, vp.origin_y, vp.client_h, w.ph, 0);
  auto mix = [](uint32_t a, uint32_t b, uint32_t f) {
    const uint32_t g = 256 - f;
    const uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
    const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
    return rb | (ag << 8);
  };
  auto at = [&](int64_t xx, int64_t yy) -> uint32_t {
    if (xx < 0) xx = 0;
    if (yy < 0) yy = 0;
    if (xx >= w.pw) xx = w.pw - 1;
    if (yy >= w.ph) yy = w.ph - 1;
    return pal[w.px[static_cast<size_t>(yy) * static_cast<size_t>(w.pw) + static_cast<size_t>(xx)] & 0x7fff];
  };
  for (int32_t y = 0; y < ch; ++y) {
    const int64_t sy = wy[static_cast<size_t>(y)];
    const int64_t y0 = sy >> 8;
    if (y0 < -2 || y0 >= w.ph + 2) continue;  // rows outside the world stay as they are (within 2: the edge row, `at`)
    const bool row_in = y >= vp.origin_y && y < vp.origin_y + vp.client_h;
    const int32_t ly = row_in ? static_cast<int32_t>(static_cast<int64_t>(y - vp.origin_y) * kLogicalH / vp.client_h) : 0;
    uint32_t *dst = out.data() + static_cast<size_t>(y) * static_cast<size_t>(cw);
    for (int32_t x = 0; x < cw; ++x) {
      if (row_in && x >= vp.origin_x && x < vp.origin_x + vp.client_w) {
        const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp.origin_x) * kLogicalW / vp.client_w);
        if (defect_ != PresentDefect::WorldMaskIgnored && !maskp[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)])
          continue;  // the frame's own pixel
      }
      const int64_t sx = wx[static_cast<size_t>(x)];
      const int64_t x0 = sx >> 8;
      if (x0 < -2 || x0 >= w.pw + 2) continue;  // columns outside the world stay as they are (within 2: the edge column, `at`)
      if (nearest) {
        dst[x] = at(x0, y0);
      } else {
        const uint32_t fx = static_cast<uint32_t>(sx & 255), fy = static_cast<uint32_t>(sy & 255);
        const uint32_t a = mix(at(x0, y0), at(x0 + 1, y0), fx), b = mix(at(x0, y0 + 1), at(x0 + 1, y0 + 1), fx);
        dst[x] = mix(a, b, fy) | 0xff000000u;
      }
    }
  }
  if (!anchor) return;
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
        if (w.mask[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)]) continue;  // not a 2D pixel of the frame
        const int32_t tx = x + dx;
        if (tx < 0 || tx >= cw) continue;
        out[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(tx)] =
            frame_img[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(x)];
      }
    }
  }
}

// P7 (present.h WideFill): the side bands from the content rectangle's own edge columns, in client pixels. The content is
// already drawn, so the bands follow the scaled image exactly (nearest or smooth) and the content's pixels are not touched.
void Presenter::fill_bands(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  const int32_t ox = vp.origin_x, oy = vp.origin_y, w = vp.client_w, h = vp.client_h;
  if (w <= 0 || h <= 0 || cw <= 0 || ch <= 0 || oy + h > ch || ox + w > cw) return;
  const bool clamp = wide_ == WideFill::Clamp;
  // client columns averaged at each edge (the frame's outer 4 logical columns) and the largest blur radius (48 logical rows)
  const int32_t k = clamp ? 1 : std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(w) * 4 / kLogicalW));
  const int32_t rmax = clamp ? 0 : std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(h) * 48 / kLogicalH));
  std::vector<uint64_t> pre[3];
  for (auto &v : pre) v.assign(static_cast<size_t>(h) + 1, 0);
  for (int side = 0; side < 2; ++side) {
    const int32_t band = side == 0 ? ox : cw - (ox + w);
    if (band <= 0) continue;
    // per row: the mean of the k edge columns (channels scaled by 256 to keep the fraction), as prefix sums over the rows
    for (int32_t y = 0; y < h; ++y) {
      const uint32_t *row = out.data() + static_cast<size_t>(oy + y) * static_cast<size_t>(cw);
      uint32_t s[3] = {0, 0, 0};
      for (int32_t j = 0; j < k; ++j) {
        const uint32_t v = row[side == 0 ? ox + j : ox + w - 1 - j];
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
        out[static_cast<size_t>(oy + y) * static_cast<size_t>(cw) + x] = 0xff000000u | (c8[0] << 16) | (c8[1] << 8) | c8[2];
      }
    }
  }
}

#ifdef _WIN32
void Presenter::paint(void *hdc, int32_t cw, int32_t ch) const {
  if (cw <= 0 || ch <= 0) return;
  render(cw, ch, scratch_);
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = cw;
  bi.bmiHeader.biHeight = -ch;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  SetDIBitsToDevice(static_cast<HDC>(hdc), 0, 0, static_cast<DWORD>(cw), static_cast<DWORD>(ch), 0, 0, 0,
                    static_cast<UINT>(ch), scratch_.data(), &bi, DIB_RGB_COLORS);
}
#endif

}  // namespace portapp
