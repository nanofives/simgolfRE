// P9C: the game's text at the window's resolution (textres.h).
#include "textres.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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
  // P10B fix 2: the ink rows of a string in a font, relative to the baseline (top <= bottom, half-open: [top, bottom)); key = the
  // font's key and the string. Measured on a 32-bit scratch DIB (white on black), cleared above 4096 entries.
  struct Ink {
    int32_t top = 0, bottom = 0;
    bool ok = false;
  };
  std::map<std::tuple<std::string, int32_t, uint32_t, int, std::string>, Ink> inks;
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
  if (vp.client_w <= 0 || vp.client_h <= 0 || r.text.empty() || r.x1 <= r.x0 || r.y1 <= r.y0) return false;
  const bool unit = vp.client_w == kLogicalW && vp.client_h == kLogicalH;
  const double sx = static_cast<double>(vp.client_w) / kLogicalW, sy = static_cast<double>(vp.client_h) / kLogicalH;
  int32_t height = static_cast<int32_t>(std::lround(r.size * sy));
  if (defect_ == TextDefect::WrongScale) height = r.size;
  if (height < 1) height = 1;
  // the run's box in client pixels (the presenter's floor mapping inverted), and a margin around it for ink that reaches past it
  p.bx0 = vp.origin_x + dx + first_client(r.x0, vp.client_w, kLogicalW);
  p.bx1 = vp.origin_x + dx + first_client(r.x1, vp.client_w, kLogicalW);
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
  t = TextTable();
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
  const size_t pitch = static_cast<size_t>((impl_->w + 1) & ~1);
  uint16_t *bits = static_cast<uint16_t *>(impl_->bits);
  // res[L][box pixel]: GDI's word over level L
  std::vector<uint16_t> res(static_cast<size_t>(32) * vis.size());
  for (uint32_t L = 0; L < 32; ++L) {
    const uint16_t g = static_cast<uint16_t>(L | (L << 5) | (L << 10));
    uint16_t *dst = res.data() + static_cast<size_t>(L) * vis.size();
    if (L >= levels) {  // a level the caller never looks up: the word itself (no ink)
      std::fill(dst, dst + vis.size(), g);
      continue;
    }
    GdiFlush();
    for (int32_t y = 0; y < p.h; ++y) {
      uint16_t *row = bits + pitch * static_cast<size_t>(y);
      for (int32_t x = 0; x < p.w; ++x) row[x] = g;
    }
    gdi(r, p);
    ++table_draws;
    for (int32_t y = y0; y < y1; ++y) {
      const uint16_t *row = bits + pitch * static_cast<size_t>(y - p.dy0) + static_cast<size_t>(x0 - p.dx0);
      std::memcpy(dst + static_cast<size_t>(y - y0) * static_cast<size_t>(bw), row, static_cast<size_t>(bw) * 2u);
    }
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
