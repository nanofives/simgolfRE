// Host GDI text metrics (phase 3.8 stream 38a, 2026-10-20; notes log/p38/38a_notes.md, U-0944 / U-0947): the
// TextMetrics provider of commands.h for Windows, reused by the phase 4 backend. Module `gditext`
// (port/src/sim/gditext.cpp), tests port/test/test_gditext.py, oracle port/test/widths_check.py over the widths the
// original measured (re/tools/widths_rec.py).
//
// THE CHAIN IT REPRODUCES. Text_put(ctx, text) -> drawMarkupRun(ctx, text, strlen) -> measureTextWidth(font, piece,
// count) per markup piece -> jgld Font::textWidth(piece, count) = GetTextExtentPoint32A on the font's HFONT, selected
// into jgld's memory DC (initFonts: CreateCompatibleDC(0)). The fonts are the exe's FontRef objects in .data, each made
// once at startup (gameMain, loadInfoScreenArt, setPrimarySurface) by FontRef::createFromId(face, size, style) ->
// Font::create, or FontRef::createSized(file, face, size, style) -> Font::loadFile, which registers the game's own font
// file (CreateScalableFontResourceA + AddFontResourceA) before Font::create. Font::create's LOGFONTA: height -size,
// weight 700 when style bit 0 else 0, italic = style bit 1, underline = style bit 2, OUT_TT_ONLY_PRECIS (7), every other
// field 0, the face copied. The provider rebuilds the same fonts (the table in gditext.cpp, keyed by FontRef object),
// registers the font files it reads through the context's GameFiles the same way (in a private temporary directory),
// and measures with the same call on its own memory DC, so its widths are the host rasterizer's, as the original's are.
//
// STATE IT READS AND WRITES. Face and file names come from the state's string pools (the GameState `const` objects and
// the pointer word gameMain reads for the bold face), the screen width for the two scaled fonts from g_screenWidth, the
// FontRef's handle word (+4: zero = the font was never created, measureTextWidth returns 0) and the draw context's
// markup words from the address space. drawMarkupRun's own writes (+0x38 style, +0x3c link state, +0x4c dropdown state,
// +0x5c filled from g_primarySurface when zero) are applied to the draw context, as the original applies them.
// SAFETY CHECK: when a FontRef in the state carries the metrics Font::create stored (+0xc .. +0x1c, from
// GetTextMetricsA), the provider's font must produce the same ones, else MissingResource{"text font"}: a host that
// cannot register the game's font file substitutes another face, and its widths would be silently wrong.
//
// Roadmap S5.1 (log/roadmap/S5/1_notes.md): the chain above is class MarkupTextMetrics, which the FreeType provider
// (portsim/sim/fttext.h) shares; GdiTextMetrics adds only the GDI font primitives. The refactor changes no GDI output.
#ifndef PORTSIM_SIM_GDITEXT_H
#define PORTSIM_SIM_GDITEXT_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/commands.h"

namespace portsim {

// Font::create's arguments for one of the 15 FontRefs the game creates at startup (the table in gditext.cpp): the face
// copied into lfFaceName, the font file Font::loadFile registers first (empty for FontRef::createFromId), the size after
// gameMain's scaleX where the call scales it (lfHeight = -size), and the style bits (0 bold, 1 italic, 2 underline).
struct GameFont {
  std::string face;
  std::string file;
  int32_t size = 0;
  uint32_t style = 0;
};
// The FontRef's arguments read from the state's string pools (MissingResource for a FontRef the game never creates)
GameFont game_font(Ctx &ctx, uint32_t fontref);
size_t game_font_count();                       // 15
uint32_t game_font_ref(Ctx &ctx, size_t i);     // the i-th FontRef's address, in creation order
// Every font file the startup calls register (gameMain's Font::loadFile calls), each once, in creation order: by the time
// any text is measured all of them are registered, so a FontRef without a file of its own can resolve to one of them.
std::vector<std::string> game_font_files(Ctx &ctx);

// THE SHARED CHAIN (roadmap S5.1): Text_put, drawMarkupRun and measureTextWidth over a font back end. A provider derives
// from this class and supplies the font primitives only: Font::textWidth's measurement of n bytes on a FontRef's font
// (font_extent) and the five words Font::create stores (font_metrics); the back ends are GDI (GdiTextMetrics, below) and
// FreeType (FtTextMetrics, portsim/sim/fttext.h). gditext's registered entries (Text_put, drawMarkupRun,
// measureTextWidth, gditext.fontWidth / fontMetrics / createFont / textPut) find the context's provider by this class.
class MarkupTextMetrics : public TextMetrics {
 public:
  // TextMetrics (commands.h): Text_put on `draw_ctx` as it measures after the caller's setDrawTarget(font, 0, 0, 0):
  // the context's base font is `font` when the FontRef's handle word is set (setDrawTarget keeps the old one
  // otherwise), its style fonts 1..3 are none; the markup words are the context's own.
  int32_t text_put(Ctx &ctx, uint32_t draw_ctx, uint32_t font, uint32_t text) override;

  // The original functions over the state as it is (every font word read from the draw context):
  int32_t text_put_state(Ctx &ctx, uint32_t draw_ctx, uint32_t text);                // Text_put
  int32_t draw_markup_run(Ctx &ctx, uint32_t draw_ctx, uint32_t text, int32_t len);  // drawMarkupRun
  int32_t measure_text_width(Ctx &ctx, uint32_t fontref, uint32_t str, int32_t maxlen);  // measureTextWidth
  // jgld Font::textWidth on the FontRef's font: exactly `n` bytes at `s` (s == 0 -> 0; the scripted rule when set;
  // else the back end's font_extent)
  int32_t font_text_width(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n);
  // The five words Font::create stores into the FontRef (+0xc tmHeight + tmExternalLeading, +0x10 tmAscent -
  // tmInternalLeading, +0x14 tmInternalLeading, +0x18 tmAscent, +0x1c tmDescent) for the provider's own font.
  virtual void font_metrics(Ctx &ctx, uint32_t fontref, int32_t out[5]) = 0;

  // TEST ONLY: replace the back end's measurement by a fixed rule (Font::textWidth = the sum over the n bytes c of base +
  // ((fontref >> 2) & 7) + (c & mask); n <= 0 gives 0), so an emulator run of the original's drawMarkupRun with the
  // same rule in place of measureTextWidth's font slot judges the token logic alone. No font resource is used.
  void set_scripted(int32_t base, uint32_t mask) {
    scripted_ = true;
    script_base_ = base;
    script_mask_ = mask;
  }
  bool scripted() const { return scripted_; }

 protected:
  // Font::textWidth's measurement of `n` bytes at `s` (s != 0) on the FontRef's font
  virtual int32_t font_extent(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n) = 0;

 private:
  bool scripted_ = false;
  int32_t script_base_ = 0;
  uint32_t script_mask_ = 0;
  // drawMarkupRun with the font words either from the context (ov == nullptr) or overridden (ov[0..3] = +0x5c..+0x68)
  int32_t markup_run(Ctx &ctx, uint32_t draw_ctx, uint32_t text, int32_t len, const uint32_t *ov);
};

class GdiTextMetrics : public MarkupTextMetrics {
 public:
  GdiTextMetrics();            // no GDI resource until the first measurement
  ~GdiTextMetrics() override;  // deletes the fonts and the DC, removes the registered font files
  GdiTextMetrics(const GdiTextMetrics &) = delete;
  GdiTextMetrics &operator=(const GdiTextMetrics &) = delete;

  void font_metrics(Ctx &ctx, uint32_t fontref, int32_t out[5]) override;

  // Phase 5 drawing services (commands.h TextMetrics, re/PHASE5_GFX_PLAN.md): the same font objects, and TextOutA on a
  // 16-bit top-down DIB section that holds the surface's words (copied in and back out around the call)
  bool glyph_width(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n, int32_t &w) override;
  bool glyph_ascent(Ctx &ctx, uint32_t fontref, int32_t &a) override;
  bool draw_glyphs(Ctx &ctx, uint32_t fontref, const std::string &text, int32_t x, int32_t y, uint32_t colorref,
                   uint16_t *px, int32_t w, int32_t h) override;

  struct Impl;

 protected:
  // GetTextExtentPoint32A on the FontRef's HFONT in the provider's memory DC
  int32_t font_extent(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n) override;

 private:
  std::unique_ptr<Impl> impl_;
};

// true where the host has GDI (Windows); elsewhere every measurement throws MissingResource{"host GDI"}
bool gdi_text_available();

}  // namespace portsim

#endif  // PORTSIM_SIM_GDITEXT_H
