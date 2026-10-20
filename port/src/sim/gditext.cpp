// Module `gditext` (phase 3.8 stream 38a, 2026-10-20): the host GDI TextMetrics provider (portsim/sim/gditext.h) and the
// original's text-measuring chain over it: Text_put, drawMarkupRun, measureTextWidth, jgld Font::textWidth. Notes:
// log/p38/38a_notes.md (every instruction address is cited there, none here). Specification: the matched jgld source
// re/match/jgld_font.cpp (Font::create, Font::loadFile, Font::textWidth, initFonts), the C4 hook sources of drawMarkupRun
// (c3o.cpp), Text_put (c3df.cpp), measureTextWidth (c3ab.cpp), scanMarkupText (c3l.cpp), skipToken / scanToken
// (c3s.cpp) and the font creation calls of gameMain / loadInfoScreenArt (c3fv.cpp) and setPrimarySurface (c3dq.cpp).
//
// Negative controls (g_negctl_gditext, read RED by port/test/test_gditext.py against the recorded widths):
//   1 Font::textWidth returns the extent plus one
//   2 Font::create's LOGFONT takes weight 700 whatever the style
//   3 drawMarkupRun ignores the markup switch (+0x40 read as 0: the whole run measured in the base font)
//   4 measureTextWidth measures max(count, strlen) bytes instead of min
//   5 drawMarkupRun's closing tokens `}` `]` keep the current style instead of resetting it to 0
//   6 the `{` token selects style 2 instead of 1
//   7 the TextMetrics interface call ignores its font argument (the context's own +0x5c is measured)
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/gditext.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#pragma comment(lib, "gdi32.lib")
#endif

namespace portsim {

namespace {
int g_negctl_gditext = 0;

// ------------------------------------------------------------------------------------------------ the game's fonts
// One row per FontRef the game creates, in creation order. `file` == nullptr: FontRef::createFromId(face, size, style)
// -> Font::create; otherwise FontRef::createSized(file, face, size, style) -> Font::loadFile -> Font::create. `scaled`:
// the size argument is scaleX(size) = (g_screenWidth * size + 160) / 320, signed (gameMain's two calls of scaleX).
struct FontSpec {
  FontRef GameState::*ref;
  uint32_t (*face)(Ctx &);
  uint32_t (*file)(Ctx &);
  int32_t size;
  bool scaled;
  uint32_t style;
  const char *site;
};
uint32_t pool_38b0(Ctx &c, uint32_t off) { return state_va(c, &GameState::f_strpool_4d38b0) + off; }
uint32_t pool_21a4(Ctx &c, uint32_t off) { return state_va(c, &GameState::f_strpool_4d21a4) + off; }
uint32_t face_bold_sans(Ctx &c) { return pool_38b0(c, 0x48); }          // f_strpool_4d38b0 + 0x48
uint32_t face_comic(Ctx &c) { return pool_38b0(c, 0x18); }              // f_strpool_4d38b0 + 0x18
uint32_t face_times_bold(Ctx &c) { return pool_38b0(c, 0x00); }         // f_strpool_4d38b0 + 0
uint32_t face_klepto(Ctx &c) { return pool_21a4(c, 0x5a4); }            // f_strpool_4d21a4 + 0x5a4
uint32_t face_manual(Ctx &c) { return pool_21a4(c, 0x598); }            // f_strpool_4d21a4 + 0x598
uint32_t face_manual_bold(Ctx &c) { return c.gs().f_ptrtab_4d3888; }    // the word gameMain reads before each push
uint32_t face_times(Ctx &c) { return state_va(c, &GameState::f_strpool_4e43ac); }
uint32_t file_manual(Ctx &c) { return pool_38b0(c, 0x3c); }             // f_strpool_4d38b0 + 0x3c
uint32_t file_klepto(Ctx &c) { return pool_38b0(c, 0x2c); }             // f_strpool_4d38b0 + 0x2c

const FontSpec kFonts[] = {
    {&GameState::g_font83ad20, face_times, nullptr, 0xc, false, 0, "setPrimarySurface (engineInit's argument)"},
    {&GameState::g_font51b320, face_bold_sans, nullptr, 9, false, 0, "gameMain, font call 1"},
    {&GameState::g_fontRef_519fd8, face_bold_sans, nullptr, 0xa, false, 0, "gameMain, font call 2"},
    {&GameState::g_font51b360, face_manual_bold, file_manual, 0xf, false, 0, "gameMain, font call 3"},
    {&GameState::g_font519928, face_manual_bold, file_manual, 0x14, false, 0, "gameMain, font call 4"},
    {&GameState::g_font51a028, face_klepto, file_klepto, 0x12, false, 0, "gameMain, font call 5"},
    {&GameState::g_fontRef_519a40, face_klepto, file_klepto, 0x18, false, 0, "gameMain, font call 6"},
    {&GameState::g_font519948, face_manual_bold, file_manual, 0x18, false, 0, "gameMain, font call 7"},
    {&GameState::g_font51b340, face_comic, nullptr, 0x28, true, 2, "gameMain, font call 8 (scaleX(0x28), italic)"},
    {&GameState::g_font519968, face_times_bold, nullptr, 7, true, 0, "gameMain, font call 9 (scaleX(ebp = 7))"},
    {&GameState::g_fontRef_821020, face_klepto, nullptr, 0x18, false, 0, "loadInfoScreenArt, font call 1"},
    {&GameState::g_fontRef_821f08, face_manual, nullptr, 0x14, false, 0, "loadInfoScreenArt, font call 2"},
    {&GameState::g_fontRef_821ec8, face_manual, nullptr, 0x12, false, 0, "loadInfoScreenArt, font call 3"},
    {&GameState::g_fontRef_821f28, face_manual, nullptr, 0x10, false, 0, "loadInfoScreenArt, font call 4"},
    {&GameState::g_fontRef_821ee8, face_manual, nullptr, 0xe, false, 0, "loadInfoScreenArt, font call 5"},
};

std::string vstring(const Memory &m, uint32_t va) {
  const uint32_t n = vstrlen(m, va);
  std::string s(n, '\0');
  if (n) std::memcpy(&s[0], m.at(va, n), n);
  return s;
}
std::vector<uint8_t> vbytes(const Memory &m, uint32_t va, uint32_t n) {
  std::vector<uint8_t> b(n);
  if (n) std::memcpy(b.data(), m.at(va, n), n);
  return b;
}

// CRT strncmp equality (stops at the first difference or a NUL of both, after `n` bytes)
bool vstrneq(const Memory &m, uint32_t a, uint32_t b, uint32_t n) {
  for (uint32_t i = 0; i < n; ++i) {
    const uint8_t x = m.load<uint8_t>(a + i), y = m.load<uint8_t>(b + i);
    if (x != y) return false;
    if (x == 0) return true;
  }
  return true;
}
// CRT strchr: the first `c` before the terminating NUL, or 0
uint32_t vstrchr(const Memory &m, uint32_t s, uint8_t c) {
  for (uint32_t p = s;; ++p) {
    const uint8_t x = m.load<uint8_t>(p);
    if (x == c) return p;
    if (x == 0) return 0;
  }
}

// The three markup scanners: advance over at most *len characters, stopping at a delimiter; a zero count returns the
// pointer and leaves the count; otherwise the remaining count (the delimiter included, or 0) is stored back.
enum class Scan { Markup, Skip, Token };
bool is_delim(Scan k, uint8_t c) {
  const bool five = c == '{' || c == '}' || c == '[' || c == ']' || c == '$';   // scanMarkupText
  if (k == Scan::Markup) return five;
  if (k == Scan::Skip) return five || c == '=';                                  // skipToken
  return five || c == '=' || c == '^';                                           // scanToken
}
uint32_t scan(const Memory &m, Scan k, uint32_t p, int32_t &len) {
  int32_t n = len;
  if (n == 0) return p;
  for (;;) {
    if (is_delim(k, m.load<uint8_t>(p))) break;
    ++p;
    if (--n == 0) break;
  }
  len = n;
  return p;
}

}  // namespace

// ------------------------------------------------------------------------------------------------ GDI side
struct GdiTextMetrics::Impl {
#ifdef _WIN32
  HDC dc = nullptr;
  struct Font {
    HFONT h = nullptr;
    int32_t metrics[5] = {0, 0, 0, 0, 0};
  };
  std::map<std::string, Font> fonts;        // keyed by the LOGFONTA bytes
  std::map<std::string, std::string> fots;  // registered font files: logical name -> .fot path
  std::vector<std::string> temp_files;
  std::string temp_dir;

  ~Impl() {
    for (auto &f : fonts)
      if (f.second.h) DeleteObject(f.second.h);
    for (auto &f : fots) RemoveFontResourceA(f.second.c_str());
    for (auto it = temp_files.rbegin(); it != temp_files.rend(); ++it) DeleteFileA(it->c_str());
    if (!temp_dir.empty()) RemoveDirectoryA(temp_dir.c_str());
    if (dc) DeleteDC(dc);
  }
  HDC the_dc() {
    // initFonts: CreateCompatibleDC(0); a failure is the original's error 2 (no measurement possible)
    if (!dc) dc = CreateCompatibleDC(nullptr);
    if (!dc) throw MissingResource{"host GDI"};
    return dc;
  }
  const std::string &dir() {
    if (temp_dir.empty()) {
      char base[MAX_PATH + 1];
      const DWORD n = GetTempPathA(MAX_PATH, base);
      if (n == 0 || n > MAX_PATH) throw MissingResource{"text font file"};
      static long counter = 0;
      char name[MAX_PATH + 64];
      std::snprintf(name, sizeof(name), "%ssg_gditext_%lu_%ld", base, static_cast<unsigned long>(GetCurrentProcessId()),
                    InterlockedIncrement(&counter));
      if (!CreateDirectoryA(name, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        throw MissingResource{"text font file"};
      temp_dir = name;
    }
    return temp_dir;
  }
  // Font::loadFile's registration of `file` (a name relative to the install root, as gameMain passes it): the .fot is
  // the file name with its last four characters replaced by ".fot", made by CreateScalableFontResourceA(0, fot, path,
  // 0) (a failure other than ERROR_FILE_EXISTS / 0 is the original's error 1), then AddFontResourceA(fot). The bytes
  // come from the context's GameFiles and are written next to the .fot in the provider's own temporary directory.
  void register_file(Ctx &ctx, const std::string &file) {
    const std::string key = GameFiles::normalize(file);
    if (fots.count(key)) return;
    const std::vector<uint8_t> bytes = ctx.files().read_all(file);
    if (file.size() < 4) throw MissingResource{"text font file"};
    const std::string path = dir() + "\\" + file;
    const std::string fot = dir() + "\\" + file.substr(0, file.size() - 4) + ".fot";
    FILE *fh = std::fopen(path.c_str(), "wb");
    if (!fh) throw MissingResource{"text font file"};
    const size_t wrote = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), fh);
    std::fclose(fh);
    temp_files.push_back(path);
    if (wrote != bytes.size()) throw MissingResource{"text font file"};
    if (!CreateScalableFontResourceA(0, fot.c_str(), path.c_str(), nullptr)) {
      const DWORD err = GetLastError();
      if (err != ERROR_FILE_EXISTS && err != 0) throw MissingResource{"text font file"};
    }
    temp_files.push_back(fot);
    if (!AddFontResourceA(fot.c_str())) throw MissingResource{"text font file"};
    fots[key] = fot;
  }
  Font &font(const LOGFONTA &lf) {
    const std::string key(reinterpret_cast<const char *>(&lf), sizeof(lf));
    auto it = fonts.find(key);
    if (it != fonts.end()) return it->second;
    Font f;
    f.h = CreateFontIndirectA(&lf);
    if (!f.h) throw MissingResource{"text font"};   // Font::create's error 13
    HDC d = the_dc();
    SelectObject(d, f.h);
    TEXTMETRICA tm;
    std::memset(&tm, 0, sizeof(tm));
    GetTextMetricsA(d, &tm);
    f.metrics[0] = tm.tmHeight + tm.tmExternalLeading;
    f.metrics[1] = tm.tmAscent - tm.tmInternalLeading;
    f.metrics[2] = tm.tmInternalLeading;
    f.metrics[3] = tm.tmAscent;
    f.metrics[4] = tm.tmDescent;
    SelectObject(d, GetStockObject(SYSTEM_FONT));
    return fonts.emplace(key, f).first->second;
  }
#endif
};

GdiTextMetrics::GdiTextMetrics() : impl_(new Impl) {}
GdiTextMetrics::~GdiTextMetrics() = default;

bool gdi_text_available() {
#ifdef _WIN32
  return true;
#else
  return false;
#endif
}

namespace {
const FontSpec &spec_of(Ctx &ctx, uint32_t fontref) {
  for (const FontSpec &s : kFonts)
    if (state_va(ctx, s.ref) == fontref) return s;
  throw MissingResource{"text font (a FontRef the game never creates at startup)"};
}
}  // namespace

#ifdef _WIN32
namespace {
// Font::create's LOGFONTA for one FontRef
LOGFONTA logfont_of(Ctx &ctx, const FontSpec &s) {
  int32_t size = s.size;
  if (s.scaled) size = div32(add32(mul32(ctx.gs().g_screenWidth, size), 160), 320);
  LOGFONTA lf;
  std::memset(&lf, 0, sizeof(lf));
  lf.lfHeight = neg32(size);
  lf.lfWeight = (s.style & 1u) ? 700 : 0;
  if (SG_NEGCTL(g_negctl_gditext) == 2) lf.lfWeight = 700;
  lf.lfItalic = (s.style & 2u) ? 1 : 0;
  lf.lfUnderline = (s.style & 4u) ? 1 : 0;
  lf.lfOutPrecision = 7;
  const std::string face = vstring(ctx.mem, s.face(ctx));
  // strcpy into the 32-byte lfFaceName (the game's faces are shorter; a longer one would overrun the original's frame)
  std::memcpy(lf.lfFaceName, face.data(), face.size() < sizeof(lf.lfFaceName) ? face.size() : sizeof(lf.lfFaceName) - 1);
  return lf;
}
}  // namespace
#endif

#ifdef _WIN32
namespace {
// the provider's font for a FontRef, after the safety check against the metrics the state carries
GdiTextMetrics::Impl::Font &resolve(GdiTextMetrics::Impl &impl, Ctx &ctx, uint32_t fontref) {
  const FontSpec &s = spec_of(ctx, fontref);
  if (s.file) impl.register_file(ctx, vstring(ctx.mem, s.file(ctx)));
  GdiTextMetrics::Impl::Font &f = impl.font(logfont_of(ctx, s));
  int32_t st[5];
  bool any = false;
  for (int i = 0; i < 5; ++i) {
    st[i] = ctx.mem.load<int32_t>(fontref + 0xc + 4u * static_cast<uint32_t>(i));
    any = any || st[i] != 0;
  }
  if (any)
    for (int i = 0; i < 5; ++i)
      if (st[i] != f.metrics[i]) throw MissingResource{"text font (host GDI metrics differ from the state's)"};
  return f;
}
}  // namespace
#endif

void GdiTextMetrics::font_metrics(Ctx &ctx, uint32_t fontref, int32_t out[5]) {
#ifdef _WIN32
  const FontSpec &s = spec_of(ctx, fontref);
  if (s.file) impl_->register_file(ctx, vstring(ctx.mem, s.file(ctx)));
  const Impl::Font &f = impl_->font(logfont_of(ctx, s));
  for (int i = 0; i < 5; ++i) out[i] = f.metrics[i];
#else
  (void)ctx;
  (void)fontref;
  (void)out;
  throw MissingResource{"host GDI"};
#endif
}

// jgld Font::textWidth: 0 for a NULL string; else the font selected into the DC, GetTextExtentPoint32A(dc, s, n), the
// stock SYSTEM_FONT selected back, and the extent's cx returned. The SIZE starts as the debug build's 0xcccccccc fill,
// which a failed call (a negative count) leaves.
int32_t GdiTextMetrics::font_text_width(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n) {
  if (s == 0) return 0;
  if (scripted_) {   // test only (gditext.attachScripted): the rule port/test/test_gditext.py's emulator stub applies too
    int32_t w = 0;
    for (int32_t i = 0; i < n; ++i)
      w = add32(w, add32(add32(script_base_, i32((fontref >> 2) & 7u)), i32(ctx.mem.load<uint8_t>(s + u32(i)) & script_mask_)));
    return w;
  }
#ifdef _WIN32
  Impl::Font &f = resolve(*impl_, ctx, fontref);
  const std::vector<uint8_t> b = vbytes(ctx.mem, s, n > 0 ? u32(n) : 0u);
  HDC d = impl_->the_dc();
  SelectObject(d, f.h);
  SIZE sz;
  sz.cx = static_cast<LONG>(0xccccccccu);
  sz.cy = static_cast<LONG>(0xccccccccu);
  const char dummy = 0;
  GetTextExtentPoint32A(d, b.empty() ? &dummy : reinterpret_cast<const char *>(b.data()), n, &sz);
  SelectObject(d, GetStockObject(SYSTEM_FONT));
  int32_t w = static_cast<int32_t>(sz.cx);
  if (SG_NEGCTL(g_negctl_gditext) == 1) w = add32(w, 1);
  return w;
#else
  (void)ctx;
  (void)fontref;
  (void)n;
  throw MissingResource{"host GDI"};
#endif
}

// measureTextWidth(font, str, maxlen): 0 when the FontRef's handle word is 0 or str is NULL; else the count is maxlen
// when maxlen < strlen(str) (signed), otherwise strlen(str), and the font's textWidth slot measures that many bytes.
int32_t GdiTextMetrics::measure_text_width(Ctx &ctx, uint32_t fontref, uint32_t str, int32_t maxlen) {
  if (ctx.mem.load<uint32_t>(fontref + 4) == 0) return 0;
  if (str == 0) return 0;
  const int32_t sl = i32(vstrlen(ctx.mem, str));
  int32_t len = maxlen < sl ? maxlen : sl;
  if (SG_NEGCTL(g_negctl_gditext) == 4) len = maxlen > sl ? maxlen : sl;
  return font_text_width(ctx, fontref, str, len);
}

// drawMarkupRun(ctx, text, len): the pixel width of `len` characters of marked-up text (c3o.cpp, C4). Context words:
// +0x38 current style 0..3, +0x3c link state, +0x40 markup switch, +0x4c dropdown state, +0x54 selects the dropdown
// arrow width, +0x5c + 4 * style the style's font (+0x5c the base font; a zero base is first filled from
// g_primarySurface). Markup off: one measureTextWidth of the whole run in the base font. Markup on: the run is cut at
// the delimiters the current scanner stops at (scanToken while +0x4c == 1, skipToken while the style is 3, else
// scanMarkupText); each piece before a delimiter is measured in the font of the style that was current, then the
// token acts: `{` style 1, `[` style 2, `}` `]` style 0 (each doubled = one literal character, measured with the next
// piece), `^` dropdown arrow (+0x4c = 2, style 0), `=` a `=...>` span skipped (+0x3c = 2), `$` + the 6-byte token at
// f_strpool_4e4218 + 0x3c (+0x3c = 1, style 3, 5 more bytes skipped), the 9-byte token at + 0x30 (+0x4c = 1, 9 more
// skipped) or the 9-byte token at + 0x24 (+0x4c = 1, style 3, 9 more skipped). A pending arrow adds 0x19 (+0x54 set)
// or 0x1e mid-run, 0x1e at the end. The last piece ends the run and clears +0x3c and +0x4c.
int32_t GdiTextMetrics::markup_run(Ctx &ctx, uint32_t dc, uint32_t text, int32_t len, const uint32_t *ov) {
  const Memory &m = ctx.mem;
  auto F = [&](uint32_t off) { return m.load<uint32_t>(dc + off); };
  auto S = [&](uint32_t off, uint32_t v) { m.store<uint32_t>(dc + off, v); };
  const int mode = SG_NEGCTL(g_negctl_gditext);
  uint32_t base = ov ? ov[0] : F(0x5c);
  if (base == 0) {
    base = ctx.gs().g_primarySurface;
    S(0x5c, base);
  }
  auto style_font = [&]() -> uint32_t {
    const uint32_t st = F(0x38);
    uint32_t f;
    if (ov && st < 4) f = st == 0 ? base : ov[st];
    else f = m.load<uint32_t>(dc + 0x5c + st * 4u);
    return f ? f : base;
  };
  if (F(0x40) == 0 || mode == 3) return measure_text_width(ctx, base, text, len);
  const uint32_t tokens = state_va(ctx, &GameState::f_strpool_4e4218);
  int32_t width = 0;
  uint32_t start = text;
  for (;;) {
    uint32_t p;
    if (F(0x4c) == 1) p = scan(m, Scan::Token, start, len);
    else if (F(0x38) == 3) p = scan(m, Scan::Skip, start, len);
    else p = scan(m, Scan::Markup, start, len);
    if (len == 0) {   // the last piece
      width = add32(width, measure_text_width(ctx, style_font(), start, i32(p - start)));
      if (F(0x4c) == 2) {
        width = add32(width, 0x1e);
        S(0x4c, 0);
        S(0x3c, 0);
        return width;
      }
      S(0x3c, 0);
      S(0x4c, 0);
      return width;
    }
    uint32_t style = F(0x38);   // a token that sets no style keeps the current one
    switch (m.load<uint8_t>(p)) {
      case '$':
        if (vstrneq(m, p, tokens + 0x3c, 6)) {
          S(0x3c, 1);
          style = 3;
        } else if (vstrneq(m, p, tokens + 0x30, 9)) {
          S(0x4c, 1);
        } else if (vstrneq(m, p, tokens + 0x24, 9)) {
          S(0x4c, 1);
          style = 3;
        }
        break;
      case '=':
        S(0x3c, 2);
        break;
      case '^':
        S(0x4c, 2);
        style = 0;
        break;
      case '{':
        if (m.load<uint8_t>(p + 1) == '{') {
          ++p;
          --len;
        } else {
          style = mode == 6 ? 2 : 1;
        }
        break;
      case '[':
        if (m.load<uint8_t>(p + 1) == '[') {
          ++p;
          --len;
        } else {
          style = 2;
        }
        break;
      case '}':
        if (m.load<uint8_t>(p + 1) == '}') {
          ++p;
          --len;
        } else if (mode != 5) {
          style = 0;
        }
        break;
      case ']':
        if (m.load<uint8_t>(p + 1) == ']') {
          ++p;
          --len;
        } else if (mode != 5) {
          style = 0;
        }
        break;
      default:
        break;
    }
    // the text before the token, in the style that was current
    width = add32(width, measure_text_width(ctx, style_font(), start, i32(p - start)));
    const uint32_t link = F(0x3c);
    if (link == 1) {          // the rest of the link token
      p += 5;
      len = sub32(len, 5);
    } else if (F(0x4c) == 1) {   // the rest of the dropdown token
      p += 9;
      len = sub32(len, 9);
    } else if (F(0x4c) == 2) {   // the dropdown arrow
      S(0x4c, 0);
      width = add32(width, F(0x54) != 0 ? 0x19 : 0x1e);
    }
    start = p + 1;
    if (link == 2) {          // skip up to and including '>'
      const uint32_t gt = vstrchr(m, start, '>');
      style = 0;
      if (gt) {
        len = add32(len, sub32(i32(start - gt), 1));
        start = gt + 1;
      }
      S(0x3c, 0);
    }
    S(0x38, style);
    len = sub32(len, 1);
  }
}

int32_t GdiTextMetrics::draw_markup_run(Ctx &ctx, uint32_t draw_ctx, uint32_t text, int32_t len) {
  return markup_run(ctx, draw_ctx, text, len, nullptr);
}

// Text_put(ctx, text): 0 for a NULL text, else drawMarkupRun(ctx, text, strlen(text))
int32_t GdiTextMetrics::text_put_state(Ctx &ctx, uint32_t draw_ctx, uint32_t text) {
  if (text == 0) return 0;
  return markup_run(ctx, draw_ctx, text, i32(vstrlen(ctx.mem, text)), nullptr);
}

int32_t GdiTextMetrics::text_put(Ctx &ctx, uint32_t draw_ctx, uint32_t font, uint32_t text) {
  if (text == 0) return 0;
  // setDrawTarget(font, 0, 0, 0): the base font only when the FontRef's handle word is set, the style fonts none
  const bool keep = SG_NEGCTL(g_negctl_gditext) == 7 || ctx.mem.load<uint32_t>(font + 4) == 0;
  const uint32_t ov[4] = {keep ? ctx.mem.load<uint32_t>(draw_ctx + 0x5c) : font, 0, 0, 0};
  return markup_run(ctx, draw_ctx, text, i32(vstrlen(ctx.mem, text)), ov);
}

// ------------------------------------------------------------------------------------------------ registry
namespace {
std::map<const Ctx *, std::unique_ptr<GdiTextMetrics>> &owned() {
  static std::map<const Ctx *, std::unique_ptr<GdiTextMetrics>> m;
  return m;
}
GdiTextMetrics &provider(Ctx &c) {
  auto *p = dynamic_cast<GdiTextMetrics *>(text_metrics(c));
  if (!p) throw MissingResource{"Text_put"};
  return *p;
}
int32_t S32(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

// gditext.attach(): a provider owned by this context, attached as its TextMetrics (replaces any other)
void a_attach(Ctx &c, const uint32_t *, Ret &r) {
  if (!gdi_text_available()) throw MissingResource{"host GDI"};
  auto p = std::make_unique<GdiTextMetrics>();
  set_text_metrics(c, p.get());
  owned()[&c] = std::move(p);
  r.set_i32(0);
}
// gditext.attachScripted(base, mask): test only, a provider whose Font::textWidth is the fixed rule (gditext.h)
void a_attach_scripted(Ctx &c, const uint32_t *a, Ret &r) {
  auto p = std::make_unique<GdiTextMetrics>();
  p->set_scripted(static_cast<int32_t>(a[0]), a[1]);
  set_text_metrics(c, p.get());
  owned()[&c] = std::move(p);
  r.set_i32(0);
}
void a_detach(Ctx &c, const uint32_t *, Ret &r) {
  if (owned().count(&c)) {
    set_text_metrics(c, nullptr);
    owned().erase(&c);
  }
  r.set_i32(0);
}
// gditext.fontMetrics(fontref, index): one of the five words Font::create stores, for the provider's font
void a_font_metrics(Ctx &c, const uint32_t *a, Ret &r) {
  int32_t out[5];
  provider(c).font_metrics(c, a[0], out);
  r.set_i32(a[1] < 5 ? out[a[1]] : 0);
}

const FuncDef kFuncs[] = {
    {"gditext.attach", "default", "int", 0, 0, a_attach, &g_negctl_gditext},
    {"gditext.attachScripted", "default", "int", 2, 0, a_attach_scripted, &g_negctl_gditext},
    {"gditext.detach", "default", "int", 0, 0, a_detach, &g_negctl_gditext},
    {"gditext.fontWidth", "default", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(provider(c).font_text_width(c, a[0], a[1], S32(a, 2))); },
     &g_negctl_gditext},
    {"gditext.fontMetrics", "default", "int", 2, 0, a_font_metrics, &g_negctl_gditext},
    // the TextMetrics interface call (setDrawTarget(font, 0, 0, 0) semantics)
    {"gditext.textPut", "default", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(provider(c).text_put(c, a[0], a[1], a[2])); }, &g_negctl_gditext},
    // the original functions over the state as it is (need the attached provider)
    {"Text_put", "thiscall", "int", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(provider(c).text_put_state(c, a[0], a[1])); }, &g_negctl_gditext},
    {"drawMarkupRun", "thiscall", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(provider(c).draw_markup_run(c, a[0], a[1], S32(a, 2))); },
     &g_negctl_gditext},
    {"measureTextWidth", "thiscall", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(provider(c).measure_text_width(c, a[0], a[1], S32(a, 2))); },
     &g_negctl_gditext},
};
}  // namespace

extern const ModuleDef kModule_gditext = {"gditext", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
