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

void Presenter::render(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vpo) const {
  const Viewport vp = fit_viewport(cw, ch);
  if (vpo) *vpo = vp;
  out.assign(static_cast<size_t>(cw > 0 ? cw : 0) * static_cast<size_t>(ch > 0 ? ch : 0), 0xff000000u);
  if (frame_.empty() || cw <= 0 || ch <= 0) return;
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
