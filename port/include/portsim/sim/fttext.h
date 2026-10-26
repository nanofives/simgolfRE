// FreeType text metrics (roadmap S5.1, 2026-10-24; notes log/roadmap/S5/1_notes.md, U-1500..U-1509): a TextMetrics
// provider (commands.h) that measures the game's fonts with FreeType instead of the host's GDI. Module `fttext`
// (port/src/sim/fttext.cpp), tests port/test/test_fttext.py, oracle port/test/widths_check.py --provider freetype over
// the widths the original measured.
//
// THE CHAIN. FtTextMetrics derives from gditext's MarkupTextMetrics (portsim/sim/gditext.h), so Text_put,
// drawMarkupRun and measureTextWidth are the very code the GDI provider runs; only the font primitives are FreeType's:
//   face lookup   Font::create's lfFaceName matched as the host GDI matches it (measured, 1_notes.md section 2): a face
//                 whose family name (name ID 1) equals it, case-insensitively, the closest in italic then weight; else a
//                 face whose full name (name ID 4) equals it when that face is the only one of its family; else the
//                 family GDI's mapper substitutes for an unknown TrueType face on this host (Arial), by the same rule.
//                 Candidates: the game's font files (the ones gameMain's Font::loadFile calls register), read through
//                 the context's GameFiles, and the TrueType files of the host's font directories.
//   size          lfHeight = -size, so the em is `size` pixels (ppem = size). Advances as GDI's (measured): the hdmx
//                 width where the file has the ppem; else the linear advance rounded where the head flags say hinting
//                 keeps widths linear or LTSH says the glyph is linear at this ppem; else FreeType's interpreter v35
//                 (the GDI-like one) hinted advance.
//   five words    tmHeight + tmExternalLeading, tmAscent - tmInternalLeading, tmInternalLeading, tmAscent, tmDescent as
//                 GDI derives them for a TrueType face (VDMX when it has the ppem, else OS/2 usWinAscent /
//                 usWinDescent scaled; external leading from hhea).
//   textWidth     the sum of the n bytes' advance widths (ANSI code page 1252 -> Unicode -> the face's cmap, no
//                 kerning, as GetTextExtentPoint32A), + 1 per glyph when the face is emboldened (synthetic bold).
// draw_glyphs: the ClearType raster model of portsim/sim/fttext_raster.h (roadmap S5.3, log/roadmap/S5/3_notes.md).
//
// BUILD. FreeType is optional (port/CMakeLists.txt, PORTSIM_HAVE_FREETYPE): without it every call throws
// MissingResource{"FreeType"} and freetype_available() is false.
#ifndef PORTSIM_SIM_FTTEXT_H
#define PORTSIM_SIM_FTTEXT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/gditext.h"

namespace portsim {

class FtTextMetrics : public MarkupTextMetrics {
 public:
  FtTextMetrics();            // no FreeType resource until the first measurement
  ~FtTextMetrics() override;  // closes the faces and the library
  FtTextMetrics(const FtTextMetrics &) = delete;
  FtTextMetrics &operator=(const FtTextMetrics &) = delete;

  // The host font directories searched for the system faces, in order. Default (empty list): $SG_FONT_DIR when set, else
  // on Windows %WINDIR%\Fonts and %LOCALAPPDATA%\Microsoft\Windows\Fonts.
  void set_font_dirs(std::vector<std::string> dirs);

  void font_metrics(Ctx &ctx, uint32_t fontref, int32_t out[5]) override;

  bool glyph_width(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n, int32_t &w) override;
  bool glyph_ascent(Ctx &ctx, uint32_t fontref, int32_t &a) override;
  bool draw_glyphs(Ctx &ctx, uint32_t fontref, const std::string &text, int32_t x, int32_t y, uint32_t colorref,
                   uint16_t *px, int32_t w, int32_t h) override;

  // The face a FontRef resolves to: "<source>:<file>#<index>" (source game or host), and whether it is emboldened
  std::string face_of(Ctx &ctx, uint32_t fontref, bool *synthetic_bold = nullptr);

  struct Impl;

 protected:
  int32_t font_extent(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n) override;

 private:
  std::unique_ptr<Impl> impl_;
};

// true when the module was built with FreeType
bool freetype_available();

}  // namespace portsim

#endif  // PORTSIM_SIM_FTTEXT_H
