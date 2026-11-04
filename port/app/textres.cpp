// P9C: the game's text at the window's resolution (textres.h).
#include "textres.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
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
}  // namespace

#ifdef _WIN32
struct TextRaster::Impl {
  HDC dc = nullptr;
  HBITMAP bmp = nullptr;
  void *bits = nullptr;
  int32_t w = 0, h = 0;
  std::map<std::tuple<std::string, int32_t, uint32_t, int>, HFONT> fonts;
  ~Impl() {
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

void TextRaster::draw(const portsim::GfxTextRun &r, bool layered, const Viewport &vp, int32_t dx, std::vector<uint32_t> &out,
                      int32_t cw, int32_t ch) {
#ifdef _WIN32
  if (vp.client_w <= 0 || vp.client_h <= 0 || r.text.empty() || r.x1 <= r.x0 || r.y1 <= r.y0) return;
  const bool unit = vp.client_w == kLogicalW && vp.client_h == kLogicalH;
  const double sx = static_cast<double>(vp.client_w) / kLogicalW, sy = static_cast<double>(vp.client_h) / kLogicalH;
  int32_t height = static_cast<int32_t>(std::lround(r.size * sy));
  if (defect_ == TextDefect::WrongScale) height = r.size;
  if (height < 1) height = 1;
  // the run's box in client pixels (the presenter's floor mapping inverted), and a margin around it for ink that reaches past it
  const int32_t bx0 = vp.origin_x + dx + first_client(r.x0, vp.client_w, kLogicalW);
  const int32_t bx1 = vp.origin_x + dx + first_client(r.x1, vp.client_w, kLogicalW);
  const int32_t by0 = vp.origin_y + first_client(r.y0, vp.client_h, kLogicalH);
  const int32_t by1 = vp.origin_y + first_client(r.y1, vp.client_h, kLogicalH);
  const int32_t m = std::max<int32_t>(4, height);
  const int32_t dx0 = std::max(0, bx0 - m), dx1 = std::min(cw, bx1 + m), dy0 = std::max(0, by0 - m), dy1 = std::min(ch, by1 + m);
  if (dx1 <= dx0 || dy1 <= dy0) return;
  const int32_t w = dx1 - dx0, h = dy1 - dy0;
  const int quality = unit ? 0 : aa_ == TextAa::ClearType ? 5 /* CLEARTYPE_QUALITY */ : aa_ == TextAa::Grey ? 4 /* ANTIALIASED */ : 0;
  HFONT f = impl_->font(r.face, height, r.style, quality);
  if (!f || !impl_->dib(w, h)) {
    ++stats.failed;
    return;
  }
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
  uint32_t ref = r.colorref;
  if (defect_ == TextDefect::WrongColour) ref = ((ref & 0xffu) << 16) | (ref & 0xff00u) | ((ref >> 16) & 0xffu);
  // the origin: the 800-space TA_BASELINE point scaled (the identity at scale 1)
  const int32_t ox = vp.origin_x + dx + (unit ? r.x : static_cast<int32_t>(std::lround(r.x * sx))) - dx0;
  const int32_t oy = vp.origin_y + (unit ? r.y : static_cast<int32_t>(std::lround(r.y * sy))) - dy0;
  SelectObject(impl_->dc, f);
  SetTextColor(impl_->dc, ref);
  TextOutA(impl_->dc, ox, oy, r.text.data(), static_cast<int>(r.text.size()));
  SelectObject(impl_->dc, GetStockObject(SYSTEM_FONT));
  GdiFlush();
  bool clipped = false;
  for (int32_t y = 0; y < h; ++y) {
    const int32_t cy = dy0 + y;
    const int32_t ly = floor_div(static_cast<int64_t>(cy - vp.origin_y) * kLogicalH, vp.client_h);
    for (int32_t x = 0; x < w; ++x) {
      const uint16_t v = bits[pitch * static_cast<size_t>(y) + static_cast<size_t>(x)];
      if (v == in[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]) continue;
      const int32_t cx = dx0 + x;
      const int32_t lx = floor_div(static_cast<int64_t>(cx - vp.origin_x - dx) * kLogicalW, vp.client_w);
      if (cx < bx0 || cx >= bx1 || cy < by0 || cy >= by1) {
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

}  // namespace portapp
