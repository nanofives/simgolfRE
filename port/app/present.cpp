// Presenting the RGB555 back buffer (present.h).
#include "present.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

bool Presenter::set_frame(const std::vector<uint16_t> &px) {
  if (px.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return false;
  ++frames_set_;
  if (defect_ == PresentDefect::Stale && !frame_.empty()) return true;
  frame_ = px;
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
