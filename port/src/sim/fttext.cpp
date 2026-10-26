// Module `fttext` (roadmap S5.1, 2026-10-24): the FreeType TextMetrics provider (portsim/sim/fttext.h). It shares
// gditext's chain (MarkupTextMetrics: Text_put, drawMarkupRun, measureTextWidth) and replaces only the font primitives:
// the face lookup, the size, the five words Font::create stores and Font::textWidth. Notes: log/roadmap/S5/1_notes.md
// (the GDI behaviour each rule reproduces, measured by log/roadmap/S5/1_gdi_probe*.py), uncertainties
// log/roadmap/S5/1_uncertain.tsv (U-1500..U-1509).
//
// Negative controls (g_negctl_fttext, read RED by port/test/test_fttext.py against the GDI provider and the recordings):
//   1 every glyph's advance + 1
//   2 the face is sized at ppem size + 1 (wrong ppem)
//   3 one FontRef, g_fontRef_519fd8 (gameMain's second Arial Bold call, the second most measured font of the
//     recordings), resolves to the family Times New Roman instead of GDI's substitute
//   4 hinting off (FT_LOAD_NO_HINTING: linearly scaled advances, no hdmx)
// and for the raster (roadmap S5.3, draw_glyphs; read RED by port/test/test_fttext_raster.py and labels.py compare):
//   5 the glyph origin one pixel right
//   6 the text colour's red one 5-bit step off (bit 3 of the COLORREF's red byte flipped)
//   7 the raster's hinting off (FT_LOAD_NO_HINTING for the drawn outlines; the advances keep their rule)
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/fttext.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>
#include <string>
#include <vector>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/sim/fttext_raster.h"

#if defined(PORTSIM_HAVE_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_DRIVER_H
#include FT_MODULE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H
#endif

namespace portsim {

namespace {
int g_negctl_fttext = 0;

// ------------------------------------------------------------------------------------------------ names and code page
// ANSI code page 1252 -> Unicode for 0x80..0x9f (MultiByteToWideChar's table; the five undefined bytes map to the C1
// control of the same value); every other byte is its own code point.
const uint16_t kCp1252High[32] = {0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160,
                                  0x2039, 0x0152, 0x008d, 0x017d, 0x008f, 0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022,
                                  0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178};
uint32_t cp1252(uint8_t b) { return (b >= 0x80 && b < 0xa0) ? kCp1252High[b - 0x80] : b; }

// case folding for face-name comparison: ASCII and Latin-1 letters
uint32_t fold(uint32_t c) {
  if (c >= 'A' && c <= 'Z') return c + 0x20;
  if (c >= 0xc0 && c <= 0xde && c != 0xd7) return c + 0x20;
  return c;
}
std::u32string folded_cp1252(const std::string &s) {
  std::u32string o;
  for (unsigned char c : s) o.push_back(fold(cp1252(c)));
  return o;
}

// ------------------------------------------------------------------------------------------------ the face index
// One face of a candidate file, from its sfnt tables (read directly: only the table directory, `name`, `OS/2`, `head`).
struct FaceInfo {
  bool game = false;           // one of the game's font files (GameFiles) or a host file
  size_t dir = 0;              // host: index into the font directory list
  std::string file;            // logical name (game) or the file's name in its directory (host)
  uint32_t index = 0;          // face index inside a collection
  std::vector<std::u32string> families, fulls;   // name IDs 1 and 4, folded
  int weight = 400;            // OS/2 usWeightClass (head macStyle bit 0 -> 700 without OS/2)
  bool italic = false;         // OS/2 fsSelection bit 0 (head macStyle bit 1 without OS/2)
};

uint32_t be32(const uint8_t *p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint16_t be16(const uint8_t *p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

class FileReader {
 public:
  FileReader(const GameFiles &f, std::string name) : f_(f), name_(std::move(name)) {}
  bool get(uint64_t off, uint64_t n, std::vector<uint8_t> &out) const {
    out.assign(static_cast<size_t>(n), 0);
    if (n == 0) return true;
    return f_.read(name_, off, out.data(), n) == static_cast<int64_t>(n);
  }

 private:
  const GameFiles &f_;
  std::string name_;
};

// the faces of one TrueType file or collection (CFF-outline faces, sfnt version 'OTTO', are skipped: GDI's
// OUT_TT_ONLY_PRECIS takes TrueType faces only)
void scan_file(const FileReader &r, bool game, size_t dir, const std::string &file, std::vector<FaceInfo> &out) {
  std::vector<uint8_t> h;
  if (!r.get(0, 12, h)) return;
  std::vector<uint32_t> offsets;
  if (be32(h.data()) == 0x74746366u) {   // 'ttcf'
    const uint32_t n = be32(h.data() + 8);
    if (n == 0 || n > 256) return;
    std::vector<uint8_t> o;
    if (!r.get(12, 4ull * n, o)) return;
    for (uint32_t i = 0; i < n; ++i) offsets.push_back(be32(o.data() + 4 * i));
  } else {
    offsets.push_back(0);
  }
  for (uint32_t fi = 0; fi < offsets.size(); ++fi) {
    std::vector<uint8_t> fh;
    if (!r.get(offsets[fi], 12, fh)) continue;
    const uint32_t ver = be32(fh.data());
    if (ver != 0x00010000u && ver != 0x74727565u) continue;   // TrueType outlines ('true' = old Apple)
    const uint16_t nt = be16(fh.data() + 4);
    std::vector<uint8_t> td;
    if (!r.get(offsets[fi] + 12, 16ull * nt, td)) continue;
    uint32_t name_off = 0, name_len = 0, os2_off = 0, os2_len = 0, head_off = 0, head_len = 0;
    for (uint16_t t = 0; t < nt; ++t) {
      const uint8_t *e = td.data() + 16 * t;
      const uint32_t tag = be32(e), off = be32(e + 8), len = be32(e + 12);
      if (tag == 0x6e616d65u) name_off = off, name_len = len;        // 'name'
      else if (tag == 0x4f532f32u) os2_off = off, os2_len = len;     // 'OS/2'
      else if (tag == 0x68656164u) head_off = off, head_len = len;   // 'head'
    }
    if (!name_len || name_len > (1u << 20)) continue;
    std::vector<uint8_t> nb;
    if (!r.get(name_off, name_len, nb) || nb.size() < 6) continue;
    FaceInfo fi_;
    fi_.game = game;
    fi_.dir = dir;
    fi_.file = file;
    fi_.index = fi;
    const uint16_t count = be16(nb.data() + 2), so = be16(nb.data() + 4);
    for (int pass = 0; pass < 2 && fi_.families.empty(); ++pass) {   // Windows records; Macintosh Roman only without them
      for (uint16_t k = 0; k < count; ++k) {
        if (6u + 12u * (k + 1u) > nb.size()) break;
        const uint8_t *e = nb.data() + 6 + 12 * k;
        const uint16_t pid = be16(e), eid = be16(e + 2), nid = be16(e + 6), ln = be16(e + 8), o = be16(e + 10);
        if (nid != 1 && nid != 4) continue;
        if (size_t(so) + o + ln > nb.size()) continue;
        const uint8_t *s = nb.data() + so + o;
        std::u32string v;
        if (pass == 0 && pid == 3) {
          for (uint16_t i = 0; i + 1 < ln; i += 2) v.push_back(fold(be16(s + i)));
        } else if (pass == 1 && pid == 1 && eid == 0) {
          for (uint16_t i = 0; i < ln; ++i) v.push_back(fold(s[i]));
        } else {
          continue;
        }
        (nid == 1 ? fi_.families : fi_.fulls).push_back(v);
      }
    }
    if (fi_.families.empty()) continue;
    std::vector<uint8_t> t;
    if (os2_len >= 64 && r.get(os2_off, 64, t)) {
      fi_.weight = be16(t.data() + 4);
      fi_.italic = (be16(t.data() + 62) & 1u) != 0;
    } else if (head_len >= 46 && r.get(head_off, 46, t)) {
      const uint16_t mac = be16(t.data() + 44);
      fi_.weight = (mac & 1u) ? 700 : 400;
      fi_.italic = (mac & 2u) != 0;
    }
    out.push_back(std::move(fi_));
  }
}

bool has_name(const std::vector<std::u32string> &v, const std::u32string &n) {
  for (const auto &x : v)
    if (x == n) return true;
  return false;
}

// GDI's choice inside one family (measured, 1_gdi_probe2.log): italic first, then the weight closest to the request
// (lfWeight 0 = FW_DONTCARE asks for 400); among equals the first candidate
int pick_in(const std::vector<FaceInfo> &faces, const std::vector<int> &cands, int want_weight, bool want_italic) {
  int best = -1;
  long best_score = 0;
  for (int i : cands) {
    const FaceInfo &f = faces[static_cast<size_t>(i)];
    long score = (f.italic == want_italic ? 0 : 100000) + std::labs(static_cast<long>(f.weight) - want_weight);
    if (best < 0 || score < best_score) best = i, best_score = score;
  }
  return best;
}

}  // namespace

// ------------------------------------------------------------------------------------------------ FreeType side
struct FtTextMetrics::Impl {
  std::vector<std::string> dirs;   // host font directories (empty: the defaults)
  bool indexed = false;
  std::vector<FaceInfo> faces;
  std::vector<std::unique_ptr<GameFiles>> host;   // one DirGameFiles per directory

  struct Font {
    int face = -1;               // index into faces
    bool synthetic_bold = false;
    int32_t metrics[5] = {0, 0, 0, 0, 0};
    int32_t adv[256];
    bool have[256];
#if defined(PORTSIM_HAVE_FREETYPE)
    FT_Face ft = nullptr;
    FT_Face rft = nullptr;           // the raster's face: the same file and size on the v40 library (S5.3)
#endif
    int subrows = 0;                 // the raster's sample rows per pixel row (0: not opened yet)
    std::vector<uint8_t> hdmx;       // the hdmx record's widths for the ppem (empty: the table has none)
    std::vector<uint8_t> ltsh;       // LTSH yPels per glyph (empty: the table is absent)
    bool has_hdmx = false;           // the face has an hdmx table (the raster's placement rule, S5.3)
    int32_t ppem = 0;
    bool hinted_advances = true;     // head flags bit 4: instructions may alter advance widths
    int mode = 0;   // the negative-control mode the font was built under
  };
  std::map<std::string, Font> fonts;   // keyed by Font::create's arguments + the control mode
  std::map<std::string, std::vector<uint8_t>> bytes;   // the selected files' bytes ("g:" / "h<dir>:" + name)
#if defined(PORTSIM_HAVE_FREETYPE)
  FT_Library lib = nullptr;
  FT_Library rlib = nullptr;   // the raster's library: interpreter v40 (S5.3, 3_notes.md section 4)
  ~Impl() {
    close_faces();
    if (lib) FT_Done_FreeType(lib);
    if (rlib) FT_Done_FreeType(rlib);
  }
  void close_faces() {
    for (auto &f : fonts) {
      if (f.second.ft) FT_Done_Face(f.second.ft);
      if (f.second.rft) FT_Done_Face(f.second.rft);
    }
  }
  FT_Library raster_library() {
    if (!rlib) {
      if (FT_Init_FreeType(&rlib)) {
        rlib = nullptr;
        throw MissingResource{"FreeType"};
      }
      if (!ftraster::set_interpreter(rlib, TT_INTERPRETER_VERSION_40)) throw MissingResource{"FreeType (interpreter v40)"};
    }
    return rlib;
  }
  // the raster face of a font, opened on first use from the bytes the metric face uses
  FT_Face raster_face(Ctx &ctx, Font &f) {
    if (f.rft) return f.rft;
    const FaceInfo &fi = faces[static_cast<size_t>(f.face)];
    const std::vector<uint8_t> &b = file_bytes(ctx, fi);
    FT_Face rf = nullptr;
    if (FT_New_Memory_Face(raster_library(), b.data(), static_cast<FT_Long>(b.size()), static_cast<FT_Long>(fi.index), &rf))
      throw MissingResource{"FreeType (a face did not open)"};
    const FT_Encoding enc = f.ft->charmap ? f.ft->charmap->encoding : FT_ENCODING_UNICODE;
    if (FT_Select_Charmap(rf, enc) ||
        FT_Set_Pixel_Sizes(rf, static_cast<FT_UInt>(f.ppem), static_cast<FT_UInt>(f.ppem))) {
      FT_Done_Face(rf);
      throw MissingResource{"FreeType (a raster face could not be set up)"};
    }
    f.rft = rf;
    f.subrows = ftraster::y_smoothing(rf, f.ppem);
    return rf;
  }
  // 6 x the x displacement the v35 hinting (the metric face's library) gives the glyph outline's leftmost point, 26.6
  // sample units: the raster's placement for a face with linear advances and an hdmx table (fttext_raster.h, U-1521)
  int32_t bw_left_shift(Font &f, FT_UInt gi) {
    auto leftmost = [&](FT_Int32 flags, FT_Pos &x) {
      if (FT_Load_Glyph(f.ft, gi, flags)) throw MissingResource{"FreeType (a glyph did not load)"};
      const FT_Outline &o = f.ft->glyph->outline;
      if (f.ft->glyph->format != FT_GLYPH_FORMAT_OUTLINE || o.n_points <= 0) return false;
      x = o.points[0].x;
      for (int i = 1; i < o.n_points; ++i) x = o.points[i].x < x ? o.points[i].x : x;
      return true;
    };
    FT_Pos u = 0, hx = 0;
    if (!leftmost(FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING, u) || !leftmost(FT_LOAD_NO_BITMAP, hx)) return 0;
    return static_cast<int32_t>(6 * (hx - u));
  }
  FT_Library library() {
    if (!lib) {
      if (FT_Init_FreeType(&lib)) {
        lib = nullptr;
        throw MissingResource{"FreeType"};
      }
      // the GDI-like TrueType interpreter (v35: full hinting of the bytecode, hdmx advances)
      FT_UInt v = TT_INTERPRETER_VERSION_35;
      if (FT_Property_Set(lib, "truetype", "interpreter-version", &v)) throw MissingResource{"FreeType (interpreter v35)"};
    }
    return lib;
  }
#endif

  std::vector<std::string> host_dirs() const {
    if (!dirs.empty()) return dirs;
    std::vector<std::string> d;
    if (const char *e = std::getenv("SG_FONT_DIR")) {
      if (*e) d.push_back(e);
      return d;
    }
#ifdef _WIN32
    if (const char *w = std::getenv("WINDIR")) d.push_back(std::string(w) + "\\Fonts");
    if (const char *l = std::getenv("LOCALAPPDATA")) d.push_back(std::string(l) + "\\Microsoft\\Windows\\Fonts");
#endif
    return d;
  }

  void index(Ctx &ctx) {
    if (indexed) return;
    // the game's files first (registered by gameMain before any measurement), then the host's
    for (const std::string &f : game_font_files(ctx)) {
      if (!ctx.files().exists(f)) throw MissingResource{"text font file"};
      scan_file(FileReader(ctx.files(), f), true, 0, f, faces);
    }
    const std::vector<std::string> hd = host_dirs();
    for (size_t i = 0; i < hd.size(); ++i) {
      host.emplace_back(new DirGameFiles(hd[i]));
      for (const DirEntry &e : host.back()->list("*")) {
        if (e.is_dir || e.name.size() < 4) continue;
        std::string ext = e.name.substr(e.name.size() - 4);
        for (char &c : ext) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
        if (ext != ".ttf" && ext != ".ttc") continue;
        scan_file(FileReader(*host.back(), e.name), false, i, e.name, faces);
      }
    }
    indexed = true;
  }

  // Font::create's face lookup (see fttext.h): family name, else a single-face family's full name, else the substitute
  int lookup(Ctx &ctx, const std::string &face, int want_weight, bool want_italic, bool force_times) {
    index(ctx);
    auto by_family = [&](const std::u32string &n) {
      std::vector<int> c;
      for (size_t i = 0; i < faces.size(); ++i)
        if (has_name(faces[i].families, n)) c.push_back(static_cast<int>(i));
      return c;
    };
    if (force_times) {   // negative control 3
      const std::vector<int> c = by_family(folded_cp1252("Times New Roman"));
      if (!c.empty()) return pick_in(faces, c, want_weight, want_italic);
    }
    const std::u32string want = folded_cp1252(face);
    std::vector<int> c = by_family(want);
    if (!c.empty()) return pick_in(faces, c, want_weight, want_italic);
    for (size_t i = 0; i < faces.size(); ++i) {
      if (!has_name(faces[i].fulls, want)) continue;
      size_t members = 0;
      for (const FaceInfo &g : faces)
        for (const auto &fam : faces[i].families)
          if (has_name(g.families, fam)) {
            ++members;
            break;
          }
      if (members == 1) return static_cast<int>(i);
    }
    // GDI's mapper substitutes Arial on this host for every unknown face the game asks for (1_gdi_probe.log, U-1501)
    c = by_family(folded_cp1252("Arial"));
    if (!c.empty()) return pick_in(faces, c, want_weight, want_italic);
    throw MissingResource{"FreeType (no face for a game font)"};
  }

  const std::vector<uint8_t> &file_bytes(Ctx &ctx, const FaceInfo &f) {
    const std::string key = (f.game ? std::string("g:") : "h" + std::to_string(f.dir) + ":") + f.file;
    auto it = bytes.find(key);
    if (it != bytes.end()) return it->second;
    std::vector<uint8_t> b = f.game ? ctx.files().read_all(f.file) : host[f.dir]->read_all(f.file);
    return bytes.emplace(key, std::move(b)).first->second;
  }

  Font &font(Ctx &ctx, uint32_t fontref) {
#if defined(PORTSIM_HAVE_FREETYPE)
    const int mode = SG_NEGCTL(g_negctl_fttext);
    const GameFont g = game_font(ctx, fontref);
    std::string key = g.face + '\x01' + std::to_string(g.size) + '\x01' + std::to_string(g.style) + '\x01' +
                      std::to_string(mode);
    const bool force_times = mode == 3 && fontref == game_font_ref(ctx, 2);
    if (force_times) key += "\x01times";
    auto it = fonts.find(key);
    if (it != fonts.end()) return it->second;
    Font f;
    std::memset(f.adv, 0, sizeof(f.adv));
    std::memset(f.have, 0, sizeof(f.have));
    f.mode = mode;
    const int want_weight = (g.style & 1u) ? 700 : 400;   // lfWeight 700 or FW_DONTCARE
    const bool want_italic = (g.style & 2u) != 0;
    f.face = lookup(ctx, g.face, want_weight, want_italic, force_times);
    const FaceInfo &fi = faces[static_cast<size_t>(f.face)];
    // GDI emboldens a face lighter than semibold when bold is asked (measured: + 1 per glyph advance, the five words
    // unchanged, 1_gdi_probe3.log; U-1502 for the threshold); a synthetic italic changes no measurement
    f.synthetic_bold = want_weight >= 600 && fi.weight < 600;
    const std::vector<uint8_t> &b = file_bytes(ctx, fi);
    if (FT_New_Memory_Face(library(), b.data(), static_cast<FT_Long>(b.size()), static_cast<FT_Long>(fi.index), &f.ft))
      throw MissingResource{"FreeType (a face did not open)"};
    if (FT_Select_Charmap(f.ft, FT_ENCODING_UNICODE) && FT_Select_Charmap(f.ft, FT_ENCODING_MS_SYMBOL)) {
      FT_Done_Face(f.ft);
      throw MissingResource{"FreeType (a face has no Windows cmap)"};
    }
    const int32_t ppem = g.size + (mode == 2 ? 1 : 0);
    if (ppem <= 0 || FT_Set_Pixel_Sizes(f.ft, static_cast<FT_UInt>(ppem), static_cast<FT_UInt>(ppem))) {
      FT_Done_Face(f.ft);
      throw MissingResource{"FreeType (a size could not be set)"};
    }
    metrics_of(f.ft, ppem, f.metrics);
    if (const TT_Header *head = static_cast<const TT_Header *>(FT_Get_Sfnt_Table(f.ft, FT_SFNT_HEAD)))
      f.hinted_advances = (head->Flags & (1u << 4)) != 0;
    hdmx_of(f.ft, ppem, f.hdmx);
    ltsh_of(f.ft, f.ltsh);
    {
      FT_ULong len = 0;
      f.has_hdmx = FT_Load_Sfnt_Table(f.ft, TTAG_hdmx, 0, nullptr, &len) == 0 && len > 0;
    }
    f.ppem = ppem;
    return fonts.emplace(key, std::move(f)).first->second;
#else
    (void)ctx;
    (void)fontref;
    throw MissingResource{"FreeType"};
#endif
  }

#if defined(PORTSIM_HAVE_FREETYPE)
  // VDMX's yMax / yMin for a ppem at the 1:1 aspect ratio (the first ratio group that covers it, or the 0:0:0 default)
  static bool vdmx(FT_Face face, int32_t ppem, int32_t &ymax, int32_t &ymin) {
    FT_ULong len = 0;
    if (FT_Load_Sfnt_Table(face, TTAG_VDMX, 0, nullptr, &len) || len < 6) return false;
    std::vector<uint8_t> t(len);
    if (FT_Load_Sfnt_Table(face, TTAG_VDMX, 0, t.data(), &len)) return false;
    const uint16_t nrecs = be16(&t[2]), nratios = be16(&t[4]);
    if (6u + 4u * nratios + 2u * nratios > len) return false;
    for (uint16_t r = 0; r < nratios; ++r) {
      const uint8_t *ra = &t[6 + 4 * r];   // bCharSet, xRatio, yStartRatio, yEndRatio
      const bool any = ra[1] == 0 && ra[2] == 0 && ra[3] == 0;
      const bool one = ra[1] == 1 && ra[2] <= 1 && ra[3] >= 1;
      if (!any && !one) continue;
      const uint16_t goff = be16(&t[6 + 4 * nratios + 2 * r]);
      if (goff + 4u > len) return false;
      const uint16_t recs = be16(&t[goff]);
      const uint8_t startsz = t[goff + 2], endsz = t[goff + 3];
      if (ppem < startsz || ppem > endsz) return false;
      for (uint16_t i = 0; i < recs; ++i) {
        const size_t e = goff + 4u + 6u * i;
        if (e + 6 > len) return false;
        if (be16(&t[e]) == ppem) {
          ymax = static_cast<int16_t>(be16(&t[e + 2]));
          ymin = static_cast<int16_t>(be16(&t[e + 4]));
          return true;
        }
      }
      return false;
    }
    (void)nrecs;
    return false;
  }

  // the hdmx device record for `ppem` (version, numRecords, sizeDeviceRecord, then per record pixelSize, maxWidth and
  // one width byte per glyph)
  static void hdmx_of(FT_Face face, int32_t ppem, std::vector<uint8_t> &out) {
    out.clear();
    FT_ULong len = 0;
    if (FT_Load_Sfnt_Table(face, TTAG_hdmx, 0, nullptr, &len) || len < 8) return;
    std::vector<uint8_t> t(len);
    if (FT_Load_Sfnt_Table(face, TTAG_hdmx, 0, t.data(), &len)) return;
    const int16_t nrec = static_cast<int16_t>(be16(&t[2]));
    const uint32_t recsz = be32(&t[4]);
    const size_t ng = static_cast<size_t>(face->num_glyphs);
    for (int16_t r = 0; r < nrec; ++r) {
      const size_t o = 8u + static_cast<size_t>(r) * recsz;
      if (o + 2 + ng > len) return;
      if (t[o] == ppem) {
        out.assign(t.begin() + static_cast<std::ptrdiff_t>(o + 2), t.begin() + static_cast<std::ptrdiff_t>(o + 2 + ng));
        return;
      }
    }
  }

  // the LTSH table's yPels bytes (version, numGlyphs, then one byte per glyph)
  static void ltsh_of(FT_Face face, std::vector<uint8_t> &out) {
    out.clear();
    FT_ULong len = 0;
    if (FT_Load_Sfnt_Table(face, TTAG_LTSH, 0, nullptr, &len) || len < 4) return;
    std::vector<uint8_t> t(len);
    if (FT_Load_Sfnt_Table(face, TTAG_LTSH, 0, t.data(), &len)) return;
    const size_t n = be16(&t[2]);
    if (4 + n > len) return;
    out.assign(t.begin() + 4, t.begin() + static_cast<std::ptrdiff_t>(4 + n));
  }

  // GetTextMetricsA's height words for a TrueType face at `ppem` (U-1503 for the rule's source), as Font::create stores them
  static void metrics_of(FT_Face face, int32_t ppem, int32_t out[5]) {
    const TT_OS2 *os2 = static_cast<const TT_OS2 *>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
    const TT_HoriHeader *hhea = static_cast<const TT_HoriHeader *>(FT_Get_Sfnt_Table(face, FT_SFNT_HHEA));
    if (!os2 || !hhea) throw MissingResource{"FreeType (a face has no OS/2 or hhea table)"};
    const FT_Fixed ys = face->size->metrics.y_scale;
    auto scale = [&](FT_Long v) { return static_cast<int32_t>((FT_MulFix(v, ys) + 32) >> 6); };
    const FT_Long asc = os2->usWinAscent, desc = os2->usWinDescent;
    int32_t a, d;
    int32_t ymax = 0, ymin = 0;
    if (vdmx(face, ppem, ymax, ymin)) {
      a = ymax;
      d = -ymin;
    } else {
      a = scale(asc);
      d = scale(desc);
    }
    const int32_t height = a + d;
    // the internal leading is the cell height less the em (lfHeight = -ppem): measured on all 15 fonts, VDMX or not
    // (scaling usWinAscent + usWinDescent - unitsPerEm instead gave one less for Klepto ITC TT at 18 / 24 and Manual SSi
    // at 14, log/roadmap/S5/1_metrics_x64_first.log)
    const int32_t il = height - ppem;
    int32_t el = scale(hhea->Line_Gap - ((asc + desc) - (hhea->Ascender - hhea->Descender)));
    if (el < 0) el = 0;
    out[0] = height + el;
    out[1] = a - il;
    out[2] = il;
    out[3] = a;
    out[4] = d;
  }
#endif

#if defined(PORTSIM_HAVE_FREETYPE)
  // the five bytes code page 1252 leaves undefined (MultiByteToWideChar makes them C1 controls)
  static bool undefined_byte(uint8_t c) { return c == 0x81 || c == 0x8d || c == 0x8f || c == 0x90 || c == 0x9d; }
  // a byte's glyph: code page 1252 -> Unicode -> the face's Unicode cmap, or 0xF000 + byte in a symbol cmap
  static FT_UInt glyph_index(const Font &f, uint8_t c) {
    if (f.ft->charmap && f.ft->charmap->encoding == FT_ENCODING_MS_SYMBOL) return FT_Get_Char_Index(f.ft, 0xf000u + c);
    return FT_Get_Char_Index(f.ft, cp1252(c));
  }
#endif

  // the advance of one byte in pixels (cached per font). GDI's rule, measured per byte 0x20..0xff on every face and size
  // the 15 FontRefs use (log/roadmap/S5/1_rule_probe.log, rule R3, 3,136 of 3,136 cases; 1_notes.md section 3):
  //   - the five bytes code page 1252 leaves undefined (they become C1 controls) measure 0
  //   - the face's hdmx device width when the table has a record for the ppem
  //   - else the linearly scaled advance rounded half up, when the face's head flags bit 4 ("instructions may alter
  //     advance widths") is clear, or when the face has an LTSH table whose yPels for the glyph is <= the ppem (the
  //     ppem from which the glyph's advance is linear)
  //   - else the advance the v35 interpreter's hinting leaves
  // (A first rule without LTSH, composites linear instead, missed 6 cases: Arial 9 0x8e 0x9c 0x9e 0xd0, Arial 10 0x63,
  // Arial 18 0x9e; rule R2 of the same log.)
  int32_t advance(Font &f, uint8_t c) {
#if defined(PORTSIM_HAVE_FREETYPE)
    if (f.have[c]) return f.adv[c];
    int32_t a = 0;
    if (!undefined_byte(c)) {
      const FT_UInt gi = glyph_index(f, c);
      const bool hinting = f.mode != 4;
      if (hinting && !f.hdmx.empty() && gi < f.hdmx.size()) {
        a = f.hdmx[gi];
      } else {
        const bool ltsh_linear = gi < f.ltsh.size() && f.ltsh[gi] <= f.ppem;
        const bool linear = !hinting || ltsh_linear || !f.hinted_advances;
        const FT_Int32 flags = FT_LOAD_NO_BITMAP | (linear ? FT_LOAD_NO_HINTING : FT_LOAD_TARGET_MONO);
        if (FT_Load_Glyph(f.ft, gi, flags)) throw MissingResource{"FreeType (a glyph did not load)"};
        if (linear) a = static_cast<int32_t>((f.ft->glyph->linearHoriAdvance + 0x8000) >> 16);
        else a = static_cast<int32_t>((f.ft->glyph->advance.x + 32) >> 6);
      }
    }
    if (f.synthetic_bold) a += 1;
    if (f.mode == 1) a += 1;
    f.adv[c] = a;
    f.have[c] = true;
    return a;
#else
    (void)f;
    (void)c;
    throw MissingResource{"FreeType"};
#endif
  }
};

FtTextMetrics::FtTextMetrics() : impl_(new Impl) {}
FtTextMetrics::~FtTextMetrics() = default;

bool freetype_available() {
#if defined(PORTSIM_HAVE_FREETYPE)
  return true;
#else
  return false;
#endif
}

void FtTextMetrics::set_font_dirs(std::vector<std::string> dirs) {
  impl_->dirs = std::move(dirs);
  impl_->indexed = false;
  impl_->faces.clear();
  impl_->host.clear();
#if defined(PORTSIM_HAVE_FREETYPE)
  impl_->close_faces();
#endif
  impl_->fonts.clear();
  impl_->bytes.clear();
}

#if defined(PORTSIM_HAVE_FREETYPE)
namespace {
// the provider's font for a FontRef, after the safety check against the metrics the state carries (as gditext's)
FtTextMetrics::Impl::Font &resolve(FtTextMetrics::Impl &impl, Ctx &ctx, uint32_t fontref) {
  FtTextMetrics::Impl::Font &f = impl.font(ctx, fontref);
  int32_t st[5];
  bool any = false;
  for (int i = 0; i < 5; ++i) {
    st[i] = ctx.mem.load<int32_t>(fontref + 0xc + 4u * static_cast<uint32_t>(i));
    any = any || st[i] != 0;
  }
  if (any)
    for (int i = 0; i < 5; ++i)
      if (st[i] != f.metrics[i]) throw MissingResource{"text font (FreeType metrics differ from the state's)"};
  return f;
}
}  // namespace

void FtTextMetrics::font_metrics(Ctx &ctx, uint32_t fontref, int32_t out[5]) {
  const Impl::Font &f = impl_->font(ctx, fontref);
  for (int i = 0; i < 5; ++i) out[i] = f.metrics[i];
}

// Font::textWidth: the n bytes' advances summed (GetTextExtentPoint32A applies no kerning); a negative count is the
// failed GDI call, whose SIZE keeps the debug build's 0xcccccccc fill
int32_t FtTextMetrics::font_extent(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n) {
  Impl::Font &f = resolve(*impl_, ctx, fontref);
  if (n < 0) return static_cast<int32_t>(0xccccccccu);
  int32_t w = 0;
  for (int32_t i = 0; i < n; ++i) w = add32(w, impl_->advance(f, ctx.mem.load<uint8_t>(s + u32(i))));
  return w;
}
#else   // no FreeType: every primitive is a MissingResource
void FtTextMetrics::font_metrics(Ctx &, uint32_t, int32_t *) { throw MissingResource{"FreeType"}; }
int32_t FtTextMetrics::font_extent(Ctx &, uint32_t, uint32_t, int32_t) { throw MissingResource{"FreeType"}; }
#endif

bool FtTextMetrics::glyph_width(Ctx &ctx, uint32_t fontref, uint32_t s, int32_t n, int32_t &w) {
  if (scripted() || !freetype_available()) return false;
  w = font_text_width(ctx, fontref, s, n);
  return true;
}

bool FtTextMetrics::glyph_ascent(Ctx &ctx, uint32_t fontref, int32_t &a) {
  if (scripted() || !freetype_available()) return false;
  int32_t m[5];
  font_metrics(ctx, fontref, m);
  a = m[1];
  return true;
}

// TextOutA(dc, x, y, text) on the 16-bit surface (TA_BASELINE, BkMode TRANSPARENT, SetTextColor(colorref)) as the
// raster model of portsim/sim/fttext_raster.h draws it: glyph i at pen x + the advances of the bytes before it (the widths
// font_extent sums), its level maps summed, one blend. The five undefined code page 1252 bytes draw nothing (U-1524).
#if defined(PORTSIM_HAVE_FREETYPE)
bool FtTextMetrics::draw_glyphs(Ctx &ctx, uint32_t fontref, const std::string &text, int32_t x, int32_t y,
                                uint32_t colorref, uint16_t *px, int32_t w, int32_t h) {
  if (scripted() || w <= 0 || h <= 0) return false;
  Impl::Font &f = resolve(*impl_, ctx, fontref);
  FT_Face rf = impl_->raster_face(ctx, f);
  ftraster::Options o;
  o.subrows = f.subrows;
  o.centre = f.hinted_advances;
  o.hinting = f.mode != 7;
  int32_t pen = x + (f.mode == 5 ? 1 : 0);
  ftraster::Levels run, g;
  for (unsigned char c : text) {
    const int32_t adv = impl_->advance(f, c);
    if (!Impl::undefined_byte(c)) {
      const FT_UInt gi = Impl::glyph_index(f, c);
      o.shift = (!f.hinted_advances && f.has_hdmx) ? impl_->bw_left_shift(f, gi) : 0;
      if (!ftraster::glyph_levels(rf, gi, pen, y, adv, o, g))
        throw MissingResource{"FreeType (a glyph did not render)"};
      ftraster::accumulate(run, g);
    }
    pen = add32(pen, adv);
  }
  ftraster::blend555(run, f.mode == 6 ? colorref ^ 0x08u : colorref, px, w, h, o);
  return true;
}
#else
bool FtTextMetrics::draw_glyphs(Ctx &, uint32_t, const std::string &, int32_t, int32_t, uint32_t, uint16_t *, int32_t,
                                int32_t) {
  return false;
}
#endif

#if defined(PORTSIM_HAVE_FREETYPE)
std::string FtTextMetrics::face_of(Ctx &ctx, uint32_t fontref, bool *synthetic_bold) {
  const Impl::Font &f = impl_->font(ctx, fontref);
  const FaceInfo &fi = impl_->faces[static_cast<size_t>(f.face)];
  if (synthetic_bold) *synthetic_bold = f.synthetic_bold;
  return std::string(fi.game ? "game:" : "host:") + fi.file + "#" + std::to_string(fi.index);
}
#else
std::string FtTextMetrics::face_of(Ctx &, uint32_t, bool *) { throw MissingResource{"FreeType"}; }
#endif

// ------------------------------------------------------------------------------------------------ registry
namespace {
std::map<const Ctx *, std::unique_ptr<FtTextMetrics>> &owned() {
  static std::map<const Ctx *, std::unique_ptr<FtTextMetrics>> m;
  return m;
}
FtTextMetrics &provider(Ctx &c) {
  auto *p = dynamic_cast<FtTextMetrics *>(text_metrics(c));
  if (!p) throw MissingResource{"fttext provider"};
  return *p;
}

// fttext.attach(): a provider owned by this context, attached as its TextMetrics (replaces any other)
void a_attach(Ctx &c, const uint32_t *, Ret &r) {
  if (!freetype_available()) throw MissingResource{"FreeType"};
  auto p = std::make_unique<FtTextMetrics>();
  set_text_metrics(c, p.get());
  owned()[&c] = std::move(p);
  r.set_i32(0);
}
void a_detach(Ctx &c, const uint32_t *, Ret &r) {
  if (owned().count(&c)) {
    if (text_metrics(c) == owned()[&c].get()) set_text_metrics(c, nullptr);
    owned().erase(&c);
  }
  r.set_i32(0);
}
// fttext.fontDir(va): the host font directory list becomes the one NUL-terminated path at va (0: the defaults)
void a_font_dir(Ctx &c, const uint32_t *a, Ret &r) {
  std::vector<std::string> d;
  if (a[0]) {
    const uint32_t n = vstrlen(c.mem, a[0]);
    std::string s(n, '\0');
    if (n) std::memcpy(&s[0], c.mem.at(a[0], n), n);
    d.push_back(s);
  }
  provider(c).set_font_dirs(d);
  r.set_i32(0);
}
// fttext.faceOf(fontref, out, cap): the resolved face's description (fttext.h face_of) copied NUL-terminated to `out`;
// returns 1 when the face is emboldened, else 0
void a_face_of(Ctx &c, const uint32_t *a, Ret &r) {
  bool bold = false;
  const std::string s = provider(c).face_of(c, a[0], &bold);
  const uint32_t cap = a[2];
  if (cap) {
    const uint32_t n = static_cast<uint32_t>(s.size()) < cap - 1 ? static_cast<uint32_t>(s.size()) : cap - 1;
    if (n) std::memcpy(c.mem.at(a[1], n, true), s.data(), n);
    c.mem.store<uint8_t>(a[1] + n, 0);
  }
  r.set_i32(bold ? 1 : 0);
}

const FuncDef kFuncs[] = {
    {"fttext.available", "default", "int", 0, 0, [](Ctx &, const uint32_t *, Ret &r) { r.set_i32(freetype_available()); },
     &g_negctl_fttext},
    {"fttext.attach", "default", "int", 0, 0, a_attach, &g_negctl_fttext},
    {"fttext.detach", "default", "int", 0, 0, a_detach, &g_negctl_fttext},
    {"fttext.fontDir", "default", "int", 1, 0, a_font_dir, &g_negctl_fttext},
    {"fttext.faceOf", "default", "int", 3, 0, a_face_of, &g_negctl_fttext},
    {"fttext.fontWidth", "default", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) {
       r.set_i32(provider(c).font_text_width(c, a[0], a[1], static_cast<int32_t>(a[2])));
     },
     &g_negctl_fttext},
    {"fttext.fontMetrics", "default", "int", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) {
       int32_t out[5];
       provider(c).font_metrics(c, a[0], out);
       r.set_i32(a[1] < 5 ? out[a[1]] : 0);
     },
     &g_negctl_fttext},
    {"fttext.textPut", "default", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(provider(c).text_put(c, a[0], a[1], a[2])); }, &g_negctl_fttext},
};
}  // namespace

extern const ModuleDef kModule_fttext = {"fttext", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
