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
#ifndef PORTSIM_SIM_GDITEXT_H
#define PORTSIM_SIM_GDITEXT_H

#include <cstdint>
#include <memory>
#include <string>

#include "portsim/registry.h"
#include "portsim/sim/commands.h"

namespace portsim {

class GdiTextMetrics : public TextMetrics {
 public:
  GdiTextMetrics();            // no GDI resource until the first measurement
  ~GdiTextMetrics() override;  // deletes the fonts and the DC, removes the registered font files
  GdiTextMetrics(const GdiTextMetrics &) = delete;
  GdiTextMetrics &operator=(const GdiTextMetrics &) = delete;

  // TextMetrics (commands.h): Text_put on `draw_ctx` as it measures after the caller's setDrawTarget(font, 0, 0, 0):
  // the context's base font is `font` when the FontRef's handle word is set (setDrawTarget keeps the old one
  // otherwise), its style fonts 1..3 are none; the markup words are the context's own.
  int32_t text_put(Ctx &ctx, uint32_t draw_ctx, uint32_t font, uint32_t text) override;

  // The original functions over the state as it is (every font word read from the draw context):
  int32_t text_put_state(Ctx &ctx, uint32_t draw_ctx, uint32_t text);                // Text_put
  int32_t draw_markup_run(Ctx &ctx, uint32_t draw_ctx, uint32_t text, int32_t len);  // drawMarkupRun
  int32_t measure_text_width(Ctx &ctx, uint32_t fontref, uint32_t str, int32_t maxlen);  // measureTextWidth
  // jgld Font::textWidth on the FontRef's font: exactly `n` bytes at `s` (s == 0 -> 0)
  int32_t font_text_width(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n);
  // The five words Font::create stores into the FontRef (+0xc tmHeight + tmExternalLeading, +0x10 tmAscent -
  // tmInternalLeading, +0x14 tmInternalLeading, +0x18 tmAscent, +0x1c tmDescent) for the provider's own font.
  void font_metrics(Ctx &ctx, uint32_t fontref, int32_t out[5]);

  // TEST ONLY: replace the GDI measurement by a fixed rule (Font::textWidth = the sum over the n bytes c of base +
  // ((fontref >> 2) & 7) + (c & mask); n <= 0 gives 0), so an emulator run of the original's drawMarkupRun with the
  // same rule in place of measureTextWidth's font slot judges the token logic alone. No GDI resource is used.
  void set_scripted(int32_t base, uint32_t mask) {
    scripted_ = true;
    script_base_ = base;
    script_mask_ = mask;
  }

  struct Impl;

 private:
  bool scripted_ = false;
  int32_t script_base_ = 0;
  uint32_t script_mask_ = 0;
  // drawMarkupRun with the font words either from the context (ov == nullptr) or overridden (ov[0..3] = +0x5c..+0x68)
  int32_t markup_run(Ctx &ctx, uint32_t draw_ctx, uint32_t text, int32_t len, const uint32_t *ov);
  std::unique_ptr<Impl> impl_;
};

// true where the host has GDI (Windows); elsewhere every measurement throws MissingResource{"host GDI"}
bool gdi_text_available();

}  // namespace portsim

#endif  // PORTSIM_SIM_GDITEXT_H
