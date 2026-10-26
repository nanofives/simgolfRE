// FreeType raster of jgld's TextOutA (roadmap S5.3, 2026-10-24; notes log/roadmap/S5/3_notes.md, U-1520..U-1529).
//
// WHAT IT MODELS. jgld draws a label with TextOutA on the surface's 16-bit BI_RGB DIB section (BkMode TRANSPARENT,
// TA_BASELINE, SetTextColor) with a font created at lfQuality 0, which on a host with ClearType on renders as GDI ClearType
// (S5.2, log/roadmap/S5/2_notes.md section d). Measured against the host GDI (3_notes.md sections 1-5):
//   levels    each subpixel (R, G, B of a pixel, orientation RGB) has a level 0..6 = the set samples of a 6-sample box
//             centred on it, the glyph scanned at 6 samples per pixel horizontally: R [6p-2, 6p+4), G [6p, 6p+6),
//             B [6p+2, 6p+8). Decoding GDI's levels with this box succeeds on every Arial glyph (3_edges.py).
//   y         FreeType's interpreter v40 hints the outline (FT_LOAD_TARGET_NORMAL); a face without a gasp table, or whose
//             gasp range for the ppem has GASP_SYMMETRIC_SMOOTHING, is scanned with 6 subrows per pixel row and the
//             level is the window count over the subrows / 6 rounded half up (GDI's y smoothing: fractional crossbars
//             on Manual SSi and Klepto ITC TT); otherwise one sample row per pixel row.
//   x         the outline is scaled by 6 in x and placed by one of three rules (the caller picks it, fttext.cpp): a face
//             whose head flags bit 4 is set (hinted advances: Arial) is shifted right by (6 x compatible advance - 6 x
//             linear advance) / 2 samples, centring it in the advance GDI uses (`centre`); a face with linear advances
//             and an hdmx table (Manual SSi) is shifted by 6 x the x displacement the v35 (black-and-white) hinting gives
//             the outline's leftmost point (`shift`); any other face (Klepto ITC TT) keeps its origin at the pen. The
//             rules and their selector are fitted on these three faces (U-1520, U-1521); GDI's own ClearType x hinting
//             is NOT reproduced: FreeType has no mode equal to it, and the remaining differences are measured.
//   run       the glyphs' level maps are summed per subpixel and clamped at 6 (GDI's one-call composition, 0 differing
//             levels against the clamped sum on five overlap strings, 3_overlap.py), then blended once.
//   blend     per channel on the 5-5-5 word: the text colour's top 5 bits f, the surface's 5 bits b, F = dec[f << 3],
//             D = dec[b << 3], m = D + floor(((F - D) round(level 65536 / 6) + 32768) / 65536), stored enc[m] >> 3, with
//             dec / enc the 8-bit gamma tables of gamma contrast / 1000 (contrast 1200 on this host), rounded half up.
//             Level 0 leaves the word, level 6 stores f. 0 of 7,168 mismatches against GDI (3_blend_check.py).
//
// The functions take an FT_Face the caller opened and sized (FT_Set_Pixel_Sizes(ppem, ppem)) on a library whose
// TrueType interpreter is v40; nothing here reads the game state.
#ifndef PORTSIM_SIM_FTTEXT_RASTER_H
#define PORTSIM_SIM_FTTEXT_RASTER_H

#include <cstdint>
#include <vector>

struct FT_FaceRec_;
struct FT_LibraryRec_;

namespace portsim {
namespace ftraster {

struct Options {
  int subrows = 1;            // sample rows per pixel row (6 with y smoothing, see y_smoothing)
  bool centre = false;        // shift by half the compatible advance's difference from the linear advance
  int32_t shift = 0;          // a further x shift of the outline, 26.6 sample units (the caller's placement rule)
  bool hinting = true;        // false: FT_LOAD_NO_HINTING (negative control)
  bool bgr = false;           // subpixel order BGR (SPI_GETFONTSMOOTHINGORIENTATION 0)
  int contrast = 1200;        // SPI_GETFONTSMOOTHINGCONTRAST
};

// A level map: `w` x `h` pixels, 3 levels 0..6 per pixel (R, G, B), its top-left pixel at (x0, y0) of the surface.
struct Levels {
  int32_t x0 = 0, y0 = 0, w = 0, h = 0;
  std::vector<uint8_t> lv;
};

// One glyph's levels with its pen at surface pixel column pen_x and its baseline above surface row base_y (the row of the
// first pixel below the baseline, TA_BASELINE); `advance` = the compatible advance in pixels (used when o.centre).
// Returns false when FreeType fails; a glyph without contours gives an empty map.
bool glyph_levels(FT_FaceRec_ *face, uint32_t glyph_index, int32_t pen_x, int32_t base_y, int32_t advance,
                  const Options &o, Levels &out);

// Adds `g` into `run` (levels summed, clamped at 6), growing run's box as needed.
void accumulate(Levels &run, const Levels &g);

// The blend of a level map onto a 5-5-5 surface of w x h words (row pitch w), clipped to the surface.
void blend555(const Levels &lv, uint32_t colorref, uint16_t *px, int32_t w, int32_t h, const Options &o);

// One channel of the blend (exposed for the tests): text 5 bits f, surface 5 bits b, level 0..6.
uint32_t mix5(int contrast, uint32_t f, uint32_t b, int level);

// The subrows GDI scans with for this face at ppem: 6 when the face has no gasp table or the gasp range covering ppem
// has GASP_SYMMETRIC_SMOOTHING (bit 3), else 1.
int y_smoothing(FT_FaceRec_ *face, int32_t ppem);

// Sets the library's TrueType interpreter (FT_Property_Set "truetype" "interpreter-version"); false on failure.
bool set_interpreter(FT_LibraryRec_ *lib, unsigned version);

}  // namespace ftraster
}  // namespace portsim

#endif  // PORTSIM_SIM_FTTEXT_RASTER_H
