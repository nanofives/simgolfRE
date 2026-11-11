// P9C: the game's text drawn at the window's resolution (log/roadmap/P9/P9C_notes.md).
//
// The graphics backend keeps every TextOutA it draws as a run (portsim/sim/gfx.h GfxTextRun: the FontRef's Font::create
// arguments, the COLORREF, the TA_BASELINE origin, the bytes, the 800-space box and the pixels of it where the run still shows)
// and publishes, with each back buffer, the back buffer without the runs' visible low-resolution glyphs (GfxTextFrame::clean, and
// per HUD layer the same). The presenter scales that picture as before and then TextRaster draws each run again, with the same
// LOGFONTA at lfHeight -round(size * window height / 600), at its 800-space origin scaled into the content rectangle (plus the
// run's layer offset), onto a 16-bit RGB555 DIB that holds the client pixels under it, as jgld's TextOutA draws onto the game's
// 16-bit surface. Only client pixels whose 800-space pixel (the presenter's floor mapping) is visible in the run are written:
// what the game drew over the run later stays over it, and the string is clipped to its scaled box (it is never reflowed). The
// font file is the one the context's GDI text provider registered (process-private), so the face resolves as the sim's does.
//
// At 800 x 600 (scale 1) the font, origin and DIB words are the game's own, so the run's pixels come out as the frame's (the
// selftest --selftest-p9c and the live-trace shots prove it pixel for pixel).
#ifndef PORTAPP_TEXTRES_H
#define PORTAPP_TEXTRES_H

#include <cstdint>
#include <memory>
#include <vector>

#include "input_map.h"
#include "portsim/sim/gfx.h"

namespace portapp {

// Deliberately wrong bodies for the controls (`--negctl p9c-*`)
enum class TextDefect : uint8_t {
  None = 0,
  Unremoved = 1,    // the low-resolution glyphs are not removed (the picture is the frame's): the text shows twice
  WrongScale = 2,   // the font is made at its 800-space size whatever the window
  WrongColour = 3,  // the COLORREF's red and blue exchanged
};
// Antialiasing of a run drawn at a scale other than 1 (scale 1 always takes the game's own lfQuality 0)
enum class TextAa : uint8_t {
  System = 0,     // lfQuality 0 (DEFAULT_QUALITY): what the host's font smoothing setting gives, as the game's own text
  ClearType = 1,  // CLEARTYPE_QUALITY
  Grey = 2,       // ANTIALIASED_QUALITY (greyscale)
};

// P15B (log/roadmap/P15/P15B_notes.md): a run's GDI result as a function of the word under each pixel. GDI's blend of a glyph
// pixel on the 16-bit DIB gives each 5-bit channel as a function of that channel alone (measured: P15B_notes.md section 1), so
// the run drawn over the 32 grey levels L (the word L | L << 5 | L << 10) gives, per pixel, the result over any word w:
// red of lut[w's red level], green of lut[w's green level], blue of lut[w's blue level]. Only pixels TextRaster::draw may write
// are kept (inside the run's scaled box and the client, visible in the run's layer); a pixel GDI left unchanged at every level
// has no entry (idx 0).
struct TextTable {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // client box of the pixels with an entry (half-open; empty = no ink)
  std::vector<uint32_t> idx;               // (x1 - x0) * (y1 - y0): 0 none, k + 1 entry k
  std::vector<uint16_t> lut;               // 32 words per entry: GDI's word over grey level L
};

struct TextResStats {
  uint64_t runs = 0;          // runs drawn
  uint64_t clipped_runs = 0;  // runs whose window-resolution ink reached past their scaled box (clipped, not reflowed)
  uint64_t clipped_px = 0;    // the client pixels of that ink
  uint64_t failed = 0;        // runs the host could not draw (no font, no DIB)
  uint64_t fitted = 0;        // P10B fix 2: runs drawn with the 800-space layout scaled (set_fit)
};

class TextRaster {
 public:
  TextRaster();
  ~TextRaster();
  TextRaster(const TextRaster &) = delete;
  TextRaster &operator=(const TextRaster &) = delete;
  void set_defect(TextDefect d) { defect_ = d; }
  void set_aa(TextAa a) { aa_ = a; }
  // P10B fix 2 (--text-fit): at a scale other than 1, the 800-space layout scaled. Every byte is drawn at its 800-space pen
  // position scaled: the advances come from the game's font at its own size (GetTextExtentExPointA), so the string is exactly as
  // wide as the box the game measured for it. The ink's vertical middle is put on the 800-space ink's middle scaled (each font's
  // ink rows measured once per string: Impl::ink). Off = P9C: TextOutA at the scaled origin with the window-resolution font's
  // own advances and baseline (the string up to a few percent narrower or wider than its box, its ink 1-2 px higher).
  void set_fit(bool on) { fit_ = on; }
  // run `r` into `out` (cw x ch BGRA words, rows top-down) through the content rectangle vp, shifted right by dx client pixels;
  // `layered` reads the run's layer visibility (GfxTextRun::vis_layer) instead of the back buffer's (vis)
  void draw(const portsim::GfxTextRun &r, bool layered, const Viewport &vp, int32_t dx, std::vector<uint32_t> &out, int32_t cw,
            int32_t ch);
  // P15B: the run's TextTable for the same arguments as draw (false: nothing drawable, or the host could not draw). Only levels
  // below `levels` are drawn (the others hold the level's own word: no ink); a caller whose words never have a channel at or
  // above it (the FPS overlay's box, darkened to half: every channel <= 127, level <= 15) passes 16
  bool table(const portsim::GfxTextRun &r, bool layered, const Viewport &vp, int32_t dx, int32_t cw, int32_t ch, TextTable &t,
             uint32_t levels = 32);
  TextResStats stats;
  uint64_t table_draws = 0;  // P15B: GDI draws made for tables
  uint64_t raster_draws = 0, raster_reuses = 0;  // P24C fix 2b: tables made from new / kept level pictures (Impl::Raster)

 private:
  struct Prep;
  bool prep(const portsim::GfxTextRun &r, const Viewport &vp, int32_t dx, int32_t cw, int32_t ch, Prep &p);
  void gdi(const portsim::GfxTextRun &r, const Prep &p);
  struct Impl;
  std::unique_ptr<Impl> impl_;
  TextDefect defect_ = TextDefect::None;
  TextAa aa_ = TextAa::System;
  bool fit_ = false;
};

}  // namespace portapp

#endif
