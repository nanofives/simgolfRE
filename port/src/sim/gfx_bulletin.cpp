// Body family `bulletin` of the graphics backend (roadmap S2.11; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// bulletin board (the trophy shelf of showBulletinBoard). Specifications: c3em.cpp (ShowBulletinBoard_re) and c3fv.cpp
// (LoadInterfaceArt_re: the tables kIa11..kIa16 and the Row / TableRow cuts between them); functions are cited by name,
// their instruction addresses are in those sources.
//
//   loadInterfaceArt.bulletinArt
//       the board's cuts of loadInterfaceArt(1), every one kind 1, trim 0, from loadImage(img, name, 0, 0, 0x100, 2) with
//       the name an offset into f_strpool_4c95f4:
//         +0x3dc0 (kIa11's last load): Row(g_sprites53fd48, 11 cells at (9, 0xdd + 0x23 i), 0xf8 x 0x1d), Row(cells 11..21
//           at (0x223, 0xdd + 0x23 i)), kIa12's seven cuts (cells 22..28), Row(cells 29..31 at (0x119, 0xba + 0x23 i),
//           0xf8 x 0x1e);
//         +0x3dac (kIa13's load): kIa13's five cuts (cells 32..36), TableRow(cells 38..59: x, y from f_tab_point16_4c2d38
//           entries 0..21, w, h from its entries 25..46), TableRow(cells 67..69: entries 22..24 and 47..49);
//         +0x3d94 (kIa14) and +0x3d7c (kIa16): the same cuts into the masks g_sprites5a34f8 (kIa15's and kIa16's cuts and
//           the same Row / TableRow calls; the mask Row of cells 29..31 starts at x 0x11a).
//   showBulletinBoard.board
//       the board image: loadImage(board, f_strpool_4e3e28 + 0x30, g_palette5199c8, 0, 0x100, 0) into a frame Image of the
//       original (here a backend-local key) and Surface_fillRegion(board, root, 0, 0, 0x320, 0x258).
//   showBulletinBoard.event(x, y)
//       one listed event's frame, before its text line: cell 33 at (x - 7, y - 0xad).
//   showBulletinBoard.capture(sx, sy, digits)
//       (roadmap S3.18) the photo the board's first call with a pending photo position takes: saveBmp(root, name, rect) with
//       name = f_strpool_4e3e7c's `snapshots\accomp` + the photo index's __itoa digits (the caller's buffer) + the `.bmp` at
//       f_strpool_4c4970 + 0x4d0, rect = {bx, by, bx + 0xc8, by + 0xa0}, bx = clamp(sx - 0x64, 0, 0x258), by = clamp(sy -
//       0x64, 0, 0x1b8) (c3em.cpp ShowBulletinBoard_re). saveBmp (c3ea.cpp SaveBmp_re): fopen(name, "wb") fails when the
//       directory is missing (the shipped install has no `snapshots` folder: nothing is written, U-1840); else a 24-bit BMP,
//       file header `BM`, size + 0x36, offset 0x36, info header 0x28 / width / height (both the rect's extent) / 1 plane /
//       24 bits, size 0, no row padding, rows bottom-up (flipBmpRows), each RGB555 word w as (w << 3) & 0xff, (w >> 2) &
//       0xf8, (w >> 7) & 0xf8 (the display's slot +0xb4 is not 1 for an RGB555 frame). Written through the context's
//       GameFiles (the app's data directory, a test's overlay), never into the GameState.
//   showBulletinBoard.photo(x, y, drawn)
//       one listed event's photo: loadBmpSurface(photo, name) with the name the caller built in g_textBuffer, then
//       Surface_blitThunk(photo, root, 0, 0, x, y - 0xa6, 0xc8, 0xa0) and the pin Surface_stretchBlit(g_sprites53c570 cell
//       24 + drawn % 3, g_sprites5a4138 cell 24, root, x + 0x64, y - 0xb3, 0). The photo is ONE Image for the whole board
//       (constructed before the event list, destroyed after the wait): loadBmpSurface (c3ee.cpp LoadBmpSurface_re over loadBmp
//       / reloadBmp / flipBmpRows) re-creates its surface only when the bitmap is wider or taller (FitSurface) and a missing
//       file leaves the previous event's pixels in it, so the blit draws them again; with no surface yet the blit draws
//       nothing (Surface_blit3 returns 7). Pixels: the 24-bit body packs (r >> 3) << 10 | (g >> 3) << 5 | b >> 3 from bits
//       + 3 * (row * width + col), the 8-bit body the colour table entry at the file's offset 0x36 + 4 * byte; a positive
//       height flips the rows first. The blit is jgld's 1:1 copy of the (0, 0, 0xc8, 0xa0) rectangle, clipped to both
//       surfaces.
//   showBulletinBoard.fixed
//       the sprites after the event list, in the original's order: cell 34 at (0x258, 0x15e); the notes of the first three
//       records with a zero date (cell 38 + i, i = 0x1d / 0x1e / 0x1f for records 0 / 1 / 4 when g_difficulty < 2) at
//       (s / 2 + 0x268, s + 400), s growing by the short f_tab_point16_4c2d38 + 0x66 + 4 i minus 4; cells 27 (0x207, 0x18b),
//       28 (0xb1, 0x18b), 35 (0x33, 0x1e2), 36 (0x2dc, 0x1ff); with c = the dated records' count and l = -0xe c: 23 (0x11c,
//       l + 0x130), 26 at (0x120, l + 0x153) and (0x120, l + 0x14f); then, from l + 0x15d, the dated records' plates
//       (cell r, r mapped as for the notes) in reverse date order, 0xe apart, cell 25 after the first (2 further down);
//       22 (0x120, 0x15f) and 24 (0x139, 0x1e4). Each over the same cell of g_sprites5a34f8 (Surface_stretchBlit).
//
// Negative controls (g_negctl_gfx_bulletin, sg_negctl_set "gfx.family.bulletin"):
//   1 the board image drawn one pixel lower      2 the trophy (cell 22) one pixel to the right
//   3 the notes' x without the s / 2 term        4 the TableRow cuts read the w / h entries one entry later
//   5 the capture writes green as (w >> 3) & 0xfc (the RGB565 arm)   6 the capture writes the rows top-down (no flipBmpRows)
//   7 the capture ignores a missing directory    8 the photo surface is dropped at every event (a missing file draws nothing)
//   9 the photo is blitted one pixel lower        10 the photo load does not flip a positive-height bitmap
//   11 (P26C item 11) the sharp art blits the redrawn cell in place of the blank one (no word changes between the two: no run)
//
// P26C item 11 (Mariano's play-test 13: "font of this screen looks pixelated"; gfx.h gfx_set_sharp_art, drawing only): the plates'
// and notes' words are pixels of the art (TROPHYparts / tacs&tees, redrawn in Spanish by scripts/lang/redraw_art.py), scaled with
// the 800 x 600 frame. When the language art beside an image has `<image>.blank.pcx` (the same image with the words erased) and
// `<image>.runs.tsv` (each drawn line: ink box, TextOutA origin and baseline, size, font, bold, colour, text; redraw_art.py
// --sharp), loadInterfaceArt.bulletinArt also cuts the blank cell of every colour cell that holds a line, and blt draws such a
// cell as: the blank cell, a text-run snapshot of each line's rows, the root put back, the redrawn cell, then each line recorded as
// a run (gfxb::text_run_end with the line's face: manual / manual bold / klepto, at the redraw's size). The root ends with the
// same pixels as before; the presenter peels each run to the blank plate and draws its words at the window's resolution.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "portsim/sim/gditext.h"  // P26C item 11: the runs' faces

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/sim/util.h"

namespace portsim {

int g_negctl_gfx_bulletin = 0;

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_bulletin); }
bool control(int m) { return mode() == m; }

uint32_t pool(Ctx &ctx) { return state_va(ctx, &GameState::f_strpool_4c95f4); }
uint32_t board_key() { return gfx_local_key(0x71000); }

void call_body(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> a) {
  GfxBody b = gfxb::find_body(name);
  if (!b) {
    g.unsup(std::string("bulletin: no body ") + name);
    return;
  }
  uint32_t args[12] = {};
  int n = 0;
  for (uint32_t v : a) args[n++] = v;
  b(g, ctx, args, n);
}

// ------------------------------------------------------------------------------------------------ P26C item 11: the art's words
// one drawn line of the art (redraw_art.py --sharp's runs.tsv), in the pixels of the cell that holds it
struct ArtRun {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // the line's ink box (half-open)
  int32_t x = 0, base = 0, size = 0;       // TextOutA's origin (TA_BASELINE) and the face's size (lfHeight = -size)
  bool klepto = false, bold = false;
  uint32_t colorref = 0;                   // r | g << 8 | b << 16
  bool shadow = false;                     // a dark copy one pixel down and right, drawn first (the engraved plate)
  uint32_t shadow_ref = 0;
  std::string text;                        // Windows-1252 bytes (the file is UTF-8)
};
struct SharpCell {
  uint32_t blank = 0;  // the blank cell's sprite key
  std::vector<ArtRun> runs;
};
struct SharpState {
  std::map<uint32_t, SharpCell> cells;  // by the colour cell's SpriteRef
};
SharpState &sharp(Gfx &g) { return g.state_of<SharpState>("bulletin.sharp"); }

// UTF-8 to Windows-1252 for the letters the language art uses (all below U+0100); anything else becomes '?'
std::string to_1252(const std::string &u) {
  std::string out;
  for (size_t i = 0; i < u.size();) {
    const uint8_t c = static_cast<uint8_t>(u[i]);
    uint32_t cp = c, n = 1;
    if (c >= 0xF0) cp = c & 0x07, n = 4;
    else if (c >= 0xE0) cp = c & 0x0F, n = 3;
    else if (c >= 0xC0) cp = c & 0x1F, n = 2;
    for (uint32_t k = 1; k < n && i + k < u.size(); ++k) cp = (cp << 6) | (static_cast<uint8_t>(u[i + k]) & 0x3F);
    i += n;
    out += cp < 0x100 ? static_cast<char>(cp) : '?';
  }
  return out;
}

// the comma-separated integers of s into v (at most n); their count
int ints(const std::string &s, int *v, int n) {
  int k = 0;
  for (size_t p = 0; k < n && p < s.size();) {
    char *e = nullptr;
    const long x = std::strtol(s.c_str() + p, &e, 10);
    if (e == s.c_str() + p) break;
    v[k++] = static_cast<int>(x);
    p = static_cast<size_t>(e - s.c_str());
    if (p < s.size() && s[p] == ',') ++p;
    else break;
  }
  return k;
}

// runs.tsv: x0,y0,x1,y1 <TAB> x <TAB> baseline <TAB> size <TAB> font <TAB> bold <TAB> r,g,b <TAB> text <TAB> shadow r,g,b or -
std::vector<ArtRun> parse_runs(const std::vector<uint8_t> &data) {
  std::vector<ArtRun> out;
  std::string s(data.begin(), data.end());
  size_t p = 0;
  while (p < s.size()) {
    size_t e = s.find('\n', p);
    if (e == std::string::npos) e = s.size();
    std::string line = s.substr(p, e - p);
    p = e + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::vector<std::string> f;
    for (size_t q = 0;;) {
      const size_t t = line.find('\t', q);
      f.push_back(line.substr(q, t == std::string::npos ? std::string::npos : t - q));
      if (t == std::string::npos) break;
      q = t + 1;
    }
    if (f.size() < 8) continue;
    ArtRun r;
    int c[4] = {}, rgb[3] = {};
    if (ints(f[0], c, 4) != 4 || ints(f[6], rgb, 3) != 3) continue;
    r.x0 = c[0], r.y0 = c[1], r.x1 = c[2], r.y1 = c[3];
    r.x = std::atoi(f[1].c_str());
    r.base = std::atoi(f[2].c_str());
    r.size = std::atoi(f[3].c_str());
    r.klepto = f[4] == "klepto";
    r.bold = std::atoi(f[5].c_str()) != 0;
    r.colorref = static_cast<uint32_t>(rgb[0] & 0xff) | static_cast<uint32_t>(rgb[1] & 0xff) << 8 | static_cast<uint32_t>(rgb[2] & 0xff) << 16;
    r.text = to_1252(f[7]);
    int sh[3] = {};
    if (f.size() > 8 && ints(f[8], sh, 3) == 3) {
      r.shadow = true;
      r.shadow_ref = static_cast<uint32_t>(sh[0] & 0xff) | static_cast<uint32_t>(sh[1] & 0xff) << 8 | static_cast<uint32_t>(sh[2] & 0xff) << 16;
      ++r.x1, ++r.y1;  // the shadow's column and row
    }
    if (r.size > 0 && r.x1 > r.x0 && r.y1 > r.y0 && !r.text.empty()) out.push_back(std::move(r));
  }
  return out;
}

// ------------------------------------------------------------------------------------------------ the art
struct Cuts {
  Gfx &g;
  Ctx &ctx;
  Image8 img;
  bool have = false;
  // P26C item 11: the loaded image's blank copy and its lines (image pixels), when the language art has them
  Image8 blank;
  std::vector<ArtRun> runs;
  bool colour = false;  // the cuts go into the colour cells (the blank cells are cut for those only)
  bool first_load = false;   // the set's first load (the plates, TROPHYparts): the only one whose lines become runs (P26C_notes.md)
  void load(uint32_t off) {
    img = Image8{};
    have = gfxb::load_image(g, ctx, pool(ctx) + off, 0, 0, 0x100, 2, img) == 0;
    if (!have) g.unsup("loadInterfaceArt.bulletinArt: a file that did not load");
    blank = Image8{};
    runs.clear();
    if (!have || !colour || !first_load || !ctx.files_ptr) return;
    std::string stem = gfxb::vstring(ctx.mem, pool(ctx) + off), ext = ".pcx";
    const size_t dot = stem.rfind('.'), sep = stem.find_last_of("/\\");
    if (dot != std::string::npos && (sep == std::string::npos || dot > sep)) {
      ext = stem.substr(dot);
      stem.resize(dot);
    }
    if (!ctx.files().exists(stem + ".runs.tsv")) return;
    if (gfxb::load_image_named(g, ctx, stem + ".blank" + ext, 0, 0, 0x100, 2, blank) != 0) {
      blank = Image8{};
      return;
    }
    runs = parse_runs(ctx.files().read_all(stem + ".runs.tsv"));
  }
  void cut(uint32_t dest, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (have) gfxb::crop_sprite(g, dest, img, x, y, w, h, false);
    if (!have || runs.empty()) return;
    // P26C item 11: the lines whose ink box's centre lies in this cell, and the cell's blank copy (a key of its own per cell)
    SharpCell sc;
    for (const ArtRun &r : runs) {
      const int32_t cx = (r.x0 + r.x1) / 2, cy = (r.y0 + r.y1) / 2;
      if (cx < x || cx >= x + w || cy < y || cy >= y + h) continue;
      ArtRun q = r;
      q.x0 -= x, q.x1 -= x, q.y0 -= y, q.y1 -= y, q.x -= x, q.base -= y;
      sc.runs.push_back(std::move(q));
    }
    if (sc.runs.empty()) return;
    sc.blank = gfx_local_key(0x7e000) + (dest - state_va(ctx, &GameState::g_sprites53fd48));
    if (gfxb::crop_sprite(g, sc.blank, blank, x, y, w, h, false) != 0) return;
    sharp(g).cells[dest] = std::move(sc);
  }
  void row(uint32_t dest, int n, int32_t x, int32_t y, int32_t dy, int32_t w, int32_t h) {
    for (int i = 0; i < n; ++i, dest += 0x2C, y += dy) cut(dest, x, y, w, h);
  }
  // TableRow: entry k of f_tab_point16_4c2d38 from xy (x, y) and from wh (w, h), 4 bytes per entry
  void table_row(uint32_t dest, uint32_t end, uint32_t xy, uint32_t wh) {
    const uint32_t t = state_va(ctx, &GameState::f_tab_point16_4c2d38);
    if (control(4)) wh += 4;
    for (uint32_t k = 0; dest < end; dest += 0x2C, k += 4)
      cut(dest, ctx.mem.load<int16_t>(t + xy + k), ctx.mem.load<int16_t>(t + xy + 2 + k), ctx.mem.load<int16_t>(t + wh + k),
          ctx.mem.load<int16_t>(t + wh + 2 + k));
  }
  // one set: the colour cells into `s` or the masks into `s` (the same rectangles); first / second = the two loads
  void set(uint32_t s, uint32_t first, uint32_t second) {
    colour = s == state_va(ctx, &GameState::g_sprites53fd48);  // P26C item 11
    first_load = true;
    load(first);
    first_load = false;
    row(s, 11, 9, 0xDD, 0x23, 0xF8, 0x1D);
    row(s + 0x2C * 11, 11, 0x223, 0xDD, 0x23, 0xF8, 0x1D);
    static const int16_t k12[7][4] = {{0x11B, 0x165, 0xF8, 0x92}, {0x114, 0x124, 0x101, 0x2F}, {0x13D, 0x1EA, 0xB7, 0x34},
                                      {0x9, 0xBA, 0xF8, 0x13},     {0x9, 0x74, 0xF8, 0x1E},     {0x1FE, 0xD, 0x84, 0xA2},
                                      {0x289, 0xD, 0x82, 0xA3}};
    for (uint32_t i = 0; i < 7; ++i) cut(s + 0x2C * (22 + i), k12[i][0], k12[i][1], k12[i][2], k12[i][3]);
    row(s + 0x2C * 29, 3, s == state_va(ctx, &GameState::g_sprites53fd48) ? 0x119 : 0x11A, 0xBA, 0x23, 0xF8, 0x1E);
    load(second);
    static const int16_t k13[5][4] = {{0x29C, 0x1FF, 0x35, 0x34}, {0x3F, 0xFB, 0xE5, 0xD1}, {0x3A, 0x31, 0xCF, 0xB2},
                                      {0x33, 0x1E2, 0xB8, 0x50},  {0x2DC, 0x1FF, 0x35, 0x34}};
    static const uint32_t cell13[5] = {32, 33, 34, 35, 36};
    for (uint32_t i = 0; i < 5; ++i) cut(s + 0x2C * cell13[i], k13[i][0], k13[i][1], k13[i][2], k13[i][3]);
    table_row(s + 0x2C * 38, s + 0x2C * 60, 0x00, 0x64);
    table_row(s + 0x2C * 67, s + 0x2C * 70, 0x58, 0xBC);
  }
};

void b_bulletin_art(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  sharp(g).cells.clear();  // P26C item 11
  Cuts c{g, ctx};
  c.set(state_va(ctx, &GameState::g_sprites53fd48), 0x3DC0, 0x3DAC);
  c.set(state_va(ctx, &GameState::g_sprites5a34f8), 0x3D94, 0x3D7C);
}

// ------------------------------------------------------------------------------------------------ the board
// P26C item 11: the face of a line (manual for the plates, manual bold for a bold line, klepto for the notes: the FontRefs gameMain
// and loadInterfaceArt create with those faces, gditext.cpp) at the redraw's size
bool art_font(Ctx &ctx, const ArtRun &r, GameFont &f) {
  const uint32_t ref = r.klepto ? state_va(ctx, &GameState::g_font51a028)
                       : r.bold ? state_va(ctx, &GameState::g_font51b360)
                                : state_va(ctx, &GameState::g_fontRef_821ee8);
  try {
    f = game_font(ctx, ref);
  } catch (const MissingResource &) {
    return false;
  }
  f.size = r.size;
  if (r.bold) f.style |= 1u;  // the redraw's bold (drawn twice, one pixel apart): the face's bold weight
  return true;
}

void blt(Gfx &g, Ctx &ctx, uint32_t cell, int32_t x, int32_t y) {
  const uint32_t src = state_va(ctx, &GameState::g_sprites53fd48) + 0x2C * cell, mask = state_va(ctx, &GameState::g_sprites5a34f8) + 0x2C * cell;
  const uint32_t ux = static_cast<uint32_t>(x), uy = static_cast<uint32_t>(y);
  const SharpState &ss = sharp(g);
  const auto it = g.sharp_art && g.text.on ? ss.cells.find(src) : ss.cells.end();
  if (it == ss.cells.end()) {
    call_body(g, ctx, "Surface_stretchBlit", {src, mask, g.root_va(ctx), ux, uy, 0});
    return;
  }
  // P26C item 11. B = the root with the blank cell, K = B with the redrawn cell over it. A line with a shadow is two runs whose
  // pixels chain (a run is kept where its pixels show last, gfx.cpp peel_kept): the shadow's run from B to B plus K's pixels nearer
  // the shadow's colour than the words' (its rows and ink columns), then the words' run from there to the redrawn cell; a line
  // without one is the words' run from B. The root ends as the original's single blit leaves it (the kept root + the redrawn cell).
  const SharpCell &sc = it->second;
  g.ensure_root(ctx);
  Surface16 &r = g.root;
  const std::vector<uint16_t> keep = r.px;
  call_body(g, ctx, "Surface_stretchBlit", {control(11) ? src : sc.blank, mask, g.root_va(ctx), ux, uy, 0});
  const std::vector<uint16_t> blank_px = r.px;
  call_body(g, ctx, "Surface_stretchBlit", {src, mask, g.root_va(ctx), ux, uy, 0});
  const std::vector<uint16_t> art_px = r.px;
  r.px = blank_px;
  const size_t n = sc.runs.size();
  const int32_t clip[4] = {0, 0, r.w, r.h};
  auto band0 = [&](const ArtRun &a) { return std::max(0, y + a.y0); };
  auto band1 = [&](const ArtRun &a) { return std::min(r.h, y + a.y1); };
  std::vector<GameFont> font(n);
  std::vector<uint8_t> ok(n, 0);
  for (size_t k = 0; k < n; ++k) ok[k] = art_font(ctx, sc.runs[k], font[k]) ? 1 : 0;
  // squared distance of an RGB555 word from a COLORREF, in 5-bit channels
  auto dist = [](uint16_t w, uint32_t c) {
    const int32_t dr = ((w >> 10) & 31) - static_cast<int32_t>((c & 0xff) >> 3), dg = ((w >> 5) & 31) - static_cast<int32_t>((c >> 8 & 0xff) >> 3),
                  db = (w & 31) - static_cast<int32_t>((c >> 16 & 0xff) >> 3);
    return dr * dr + dg * dg + db * db;
  };
  std::vector<gfxb::TextRunScope> shadow(n), words(n);
  for (size_t k = 0; k < n; ++k)
    if (ok[k] && sc.runs[k].shadow) gfxb::text_run_begin(g, r.px.data(), r.w, r.h, shadow[k], band0(sc.runs[k]), band1(sc.runs[k]));
  for (size_t k = 0; k < n; ++k) {
    const ArtRun &a = sc.runs[k];
    if (!ok[k] || !a.shadow) continue;
    for (int32_t yy = band0(a); yy < band1(a); ++yy)
      for (int32_t xx = std::max(0, x + a.x0); xx < std::min(r.w, x + a.x1); ++xx) {
        const size_t i = static_cast<size_t>(yy) * static_cast<size_t>(r.w) + static_cast<size_t>(xx);
        if (art_px[i] != blank_px[i] && dist(art_px[i], a.shadow_ref) < dist(art_px[i], a.colorref)) r.px[i] = art_px[i];
      }
    gfxb::text_run_end(g, ctx, shadow[k], r.px.data(), r.w, r.h, 0, 0, clip, 0, a.text, x + a.x + 1, y + a.base + 1, a.shadow_ref, 0,
                       &font[k]);
  }
  for (size_t k = 0; k < n; ++k)
    if (ok[k]) gfxb::text_run_begin(g, r.px.data(), r.w, r.h, words[k], band0(sc.runs[k]), band1(sc.runs[k]));
  r.px = keep;
  call_body(g, ctx, "Surface_stretchBlit", {src, mask, g.root_va(ctx), ux, uy, 0});
  for (size_t k = 0; k < n; ++k) {
    const ArtRun &a = sc.runs[k];
    if (ok[k]) gfxb::text_run_end(g, ctx, words[k], r.px.data(), r.w, r.h, 0, 0, clip, 0, a.text, x + a.x, y + a.base, a.colorref, 0, &font[k]);
  }
}

// the board's photo Image (roadmap S3.18): its 16-bit surface (w == 0: no surface yet), made with the board image and kept
// for every event of the list
struct BoardState {
  Surface16 photo;
  // P26C item 5: the tick's box (cell 36 at 0x2dc, 0x1ff, 0x35 x 0x34) as the board drew it and as it was before the tick
  std::vector<uint16_t> tick_plain, tick_under;
};
constexpr int32_t kTickX = 0x2DC, kTickY = 0x1FF, kTickW = 0x35, kTickH = 0x34;  // cell 36's cut and place (b_fixed)
std::vector<uint16_t> grab(const Surface16 &r, int32_t x0, int32_t y0, int32_t w, int32_t h) {
  std::vector<uint16_t> v(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  for (int32_t yy = 0; yy < h; ++yy)
    for (int32_t xx = 0; xx < w; ++xx)
      if (x0 + xx >= 0 && x0 + xx < r.w && y0 + yy >= 0 && y0 + yy < r.h)
        v[static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx)] =
            r.px[static_cast<size_t>(y0 + yy) * static_cast<size_t>(r.w) + static_cast<size_t>(x0 + xx)];
  return v;
}
BoardState &board(Gfx &g) { return g.state_of<BoardState>("bulletin"); }
void reset_photo(Gfx &g) { board(g).photo = Surface16{}; }

void b_board(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  reset_photo(g);   // the photo Image of this board (constructed after the board image's fill): no surface yet
  const uint32_t key = board_key();
  g.images.erase(key);
  call_body(g, ctx, "loadImage",
            {key, state_va(ctx, &GameState::f_strpool_4e3e28) + 0x30, state_va(ctx, &GameState::g_palette5199c8), 0, 0x100, 0});
  if (!g.images.count(key)) {
    g.unsup("showBulletinBoard.board: the board image did not load");
    return;
  }
  call_body(g, ctx, "Surface_fillRegion", {key, g.root_va(ctx), 0, control(1) ? 1u : 0u, 0x320, 0x258});
}

void b_event(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  blt(g, ctx, 33, sub32(static_cast<int32_t>(a[0]), 7), sub32(static_cast<int32_t>(a[1]), 0xAD));
}

// ------------------------------------------------------------------------------------------------ the photos (roadmap S3.18)
uint32_t u8le(const std::vector<uint8_t> &b, size_t at, int n) {
  uint32_t v = 0;
  for (int k = n - 1; k >= 0; --k) v = (v << 8) | (at + static_cast<size_t>(k) < b.size() ? b[at + static_cast<size_t>(k)] : 0u);
  return v;
}
void put32(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (int k = 0; k < 4; ++k) b[at + static_cast<size_t>(k)] = static_cast<uint8_t>(v >> (8 * k));
}

// flipBmpRows(bits, info) over `rows` rows of `row_bytes` bytes: the rows reversed (an odd count keeps its middle row)
void flip_rows(std::vector<uint8_t> &bits, int32_t rows, size_t row_bytes) {
  if (row_bytes == 0) return;
  for (int32_t i = 0, j = rows - 1; i < j; ++i, --j) {
    const size_t a = static_cast<size_t>(i) * row_bytes, b = static_cast<size_t>(j) * row_bytes;
    if (b + row_bytes > bits.size()) continue;
    std::swap_ranges(bits.begin() + static_cast<ptrdiff_t>(a), bits.begin() + static_cast<ptrdiff_t>(a + row_bytes),
                     bits.begin() + static_cast<ptrdiff_t>(b));
  }
}

// the directory part of a logical name exists (fopen for writing does not create it): FindFirstFileA of "<dir>/*" lists
// at least the directory's own "." entry
bool dir_exists(Ctx &ctx, const std::string &name) {
  std::string n = name;
  for (char &c : n)
    if (c == '\\') c = '/';
  const size_t slash = n.rfind('/');
  if (slash == std::string::npos) return true;   // the install root
  return !ctx.files().list(n.substr(0, slash) + "/*").empty();
}

void b_capture(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const Memory &m = ctx.mem;
  const std::string name = gfxb::vstring(m, state_va(ctx, &GameState::f_strpool_4e3e7c)) + gfxb::vstring(m, a[2]) +
                           gfxb::vstring(m, state_va(ctx, &GameState::f_strpool_4c4970) + 0x4d0);
  if (!control(7) && !dir_exists(ctx, name)) return;   // saveBmp's fopen fails: nothing written
  const int32_t bx = clamp(sub32(static_cast<int32_t>(a[0]), 0x64), 0, 0x258);
  const int32_t by = clamp(sub32(static_cast<int32_t>(a[1]), 0x64), 0, 0x1b8);
  const int32_t w = 0xc8, h = 0xa0;
  const size_t row = static_cast<size_t>(w) * 3, size = row * static_cast<size_t>(h);
  std::vector<uint8_t> bits(size);
  size_t k = 0;
  for (int32_t y = by; y < by + h; ++y)
    for (int32_t x = bx; x < bx + w; ++x) {
      const bool in = x >= 0 && y >= 0 && x < g.root.w && y < g.root.h;
      const uint32_t px = in ? g.root.px[static_cast<size_t>(y) * static_cast<size_t>(g.root.w) + static_cast<size_t>(x)] : 0;
      bits[k++] = static_cast<uint8_t>(px << 3);
      bits[k++] = static_cast<uint8_t>(control(5) ? (px >> 3) & 0xfc : (px >> 2) & 0xf8);
      bits[k++] = static_cast<uint8_t>((px >> 7) & 0xf8);
    }
  if (!control(6)) flip_rows(bits, h, row);
  std::vector<uint8_t> f(0x36 + size, 0);
  f[0] = 'B';
  f[1] = 'M';
  put32(f, 2, static_cast<uint32_t>(size) + 0x36);
  put32(f, 10, 0x36);
  put32(f, 14, 0x28);
  put32(f, 18, static_cast<uint32_t>(w));
  put32(f, 22, static_cast<uint32_t>(h));
  f[26] = 1;      // planes
  f[28] = 0x18;   // bit count; compression and the image size stay 0
  std::copy(bits.begin(), bits.end(), f.begin() + 0x36);
  if (!ctx.files().write(name, f)) g.unsup("showBulletinBoard.capture: the files refused the snapshot");
}

// loadBmpSurface(photo, name) into the board's photo surface; the original's result code (0 = loaded, 6 = no bitmap)
int load_bmp_surface(Ctx &ctx, const std::string &name, Surface16 &s) {
  GameFiles &files = ctx.files();
  if (!files.exists(name)) return 6;
  const std::vector<uint8_t> file = files.read_all(name);
  if (file.size() < 0xe || u8le(file, 0, 2) != 0x4d42) return 6;
  const uint32_t off = u8le(file, 10, 4);
  if (off < 0xe || file.size() < off) return 6;   // the info header read short
  const int32_t bw = static_cast<int32_t>(u8le(file, 0xe + 4, 4)), bh = static_cast<int32_t>(u8le(file, 0xe + 8, 4));
  const uint32_t bpp = u8le(file, 0xe + 0xe, 2);
  uint32_t sz = u8le(file, 0xe + 0x14, 4);
  if (sz == 0) sz = static_cast<uint32_t>(mul32(mul32(abs32(bh), static_cast<int32_t>(bpp)), bw));
  if (file.size() <= off) return 6;   // fread of the bits gave 0
  // sizes no bitmap of the game reaches: taken as the original's failed malloc / Surface_createFull (U-1842)
  if (sz > (1u << 26) || static_cast<int64_t>(bw) * abs32(bh) > (1 << 24)) return 6;
  std::vector<uint8_t> bits(file.begin() + off, file.begin() + off + std::min<size_t>(sz, file.size() - off));
  bits.resize(sz, 0);   // the unread rest of the malloc'd block (U-1842)
  const int32_t ah = abs32(bh);
  const bool eight = bpp == 8;
  if (bh > 0 && !control(10)) flip_rows(bits, ah, static_cast<size_t>(bw) * (bpp == 0x18 ? 3u : 1u));
  if (bw > s.w || ah > s.h) s.make(bw, ah);   // FitSurface: Surface_createFull(biWidth, |biHeight|)
  for (int32_t r = 0; r < ah; ++r)
    for (int32_t c = 0; c < bw; ++c) {
      uint32_t red, green, blue;
      if (eight) {
        const size_t i = static_cast<size_t>(r) * static_cast<size_t>(bw) + static_cast<size_t>(c);
        const size_t e = 0x36 + 4 * static_cast<size_t>(i < bits.size() ? bits[i] : 0);
        blue = u8le(file, e, 1);
        green = u8le(file, e + 1, 1);
        red = u8le(file, e + 2, 1);
      } else {
        const size_t p = 3 * (static_cast<size_t>(r) * static_cast<size_t>(bw) + static_cast<size_t>(c));
        blue = p < bits.size() ? bits[p] : 0;
        green = p + 1 < bits.size() ? bits[p + 1] : 0;
        red = p + 2 < bits.size() ? bits[p + 2] : 0;
      }
      s.px[static_cast<size_t>(r) * static_cast<size_t>(s.w) + static_cast<size_t>(c)] =
          static_cast<uint16_t>(((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3));
    }
  return 0;
}

void b_photo(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]), drawn = static_cast<int32_t>(a[2]);
  Surface16 &ph = board(g).photo;
  if (control(8)) ph = Surface16{};
  load_bmp_surface(ctx, gfxb::vstring(ctx.mem, state_va(ctx, &GameState::g_textBuffer)), ph);
  if (ph.w > 0) {   // Surface_blitThunk(photo, root, 0, 0, x, y - 0xa6, 0xc8, 0xa0): a 1:1 copy, clipped
    const int32_t dy0 = sub32(y, control(9) ? 0xa5 : 0xa6);
    for (int32_t r = 0; r < 0xa0 && r < ph.h; ++r)
      for (int32_t c = 0; c < 0xc8 && c < ph.w; ++c) {
        const int32_t dx = add32(x, c), dy = add32(dy0, r);
        if (dx < 0 || dy < 0 || dx >= g.root.w || dy >= g.root.h) continue;
        g.root.px[static_cast<size_t>(dy) * static_cast<size_t>(g.root.w) + static_cast<size_t>(dx)] =
            ph.px[static_cast<size_t>(r) * static_cast<size_t>(ph.w) + static_cast<size_t>(c)];
      }
  }
  call_body(g, ctx, "Surface_stretchBlit",
            {state_va(ctx, &GameState::g_sprites53c570) + 0x2Cu * (24 + static_cast<uint32_t>(drawn % 3)),
             state_va(ctx, &GameState::g_sprites5a4138) + 0x2C * 24, g.root_va(ctx), static_cast<uint32_t>(add32(x, 0x64)),
             static_cast<uint32_t>(sub32(y, 0xB3)), 0});
}

// a record index as the board maps it (records 0, 1, 4 take cells 0x1d, 0x1e, 0x1f when g_difficulty < 2)
int32_t mapped(Ctx &ctx, int32_t i) {
  if (ctx.gs().g_difficulty < 2) {
    if (i == 0) return 0x1D;
    if (i == 1) return 0x1E;
    if (i == 4) return 0x1F;
  }
  return i;
}

void b_fixed(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  const Memory &m = ctx.mem;
  const uint32_t ev = state_va(ctx, &GameState::g_bulletinEvents) + 0x28;
  auto date = [&](int32_t i) { return m.load<int32_t>(ev + 0x30 * static_cast<uint32_t>(i)); };
  // the dated records in increasing date order (the original's selection sort: the first smallest date wins)
  int32_t order[22];
  bool used[22] = {};
  int32_t count = 0;
  for (;;) {
    int32_t best = -1, best_date = 0x7ffffff;
    for (int32_t i = 0; i < 22; ++i)
      if (date(i) != 0 && !used[i] && date(i) < best_date) {
        best_date = date(i);
        best = i;
      }
    if (best == -1) break;
    order[count++] = best;
    used[best] = true;
  }
  blt(g, ctx, 34, 0x258, 0x15E);
  const uint32_t tab = state_va(ctx, &GameState::f_tab_point16_4c2d38);
  int32_t s = 0, slots = 0;
  for (int32_t i = 0; i < 0x16; ++i) {
    if (date(i) != 0) continue;
    blt(g, ctx, 38 + static_cast<uint32_t>(mapped(ctx, i)), control(3) ? 0x268 : add32(s / 2, 0x268), add32(s, 400));
    ++slots;
    s = add32(s, -4 + m.load<int16_t>(tab + 0x66 + 4 * static_cast<uint32_t>(i)));
    if (slots > 2) break;
  }
  blt(g, ctx, 27, 0x207, 0x18B);
  blt(g, ctx, 28, 0xB1, 0x18B);
  blt(g, ctx, 35, 0x33, 0x1E2);
  g.ensure_root(ctx);
  board(g).tick_under = grab(g.root, kTickX, kTickY, kTickW, kTickH);  // P26C item 5
  blt(g, ctx, 36, 0x2DC, 0x1FF);
  board(g).tick_plain = grab(g.root, kTickX, kTickY, kTickW, kTickH);
  int32_t ly = mul32(count, -0xE);
  blt(g, ctx, 23, 0x11C, add32(ly, 0x130));
  blt(g, ctx, 26, 0x120, add32(ly, 0x153));
  blt(g, ctx, 26, 0x120, add32(ly, 0x14F));
  ly = add32(ly, 0x15D);
  for (int32_t k = 0; k < count; ++k) {
    blt(g, ctx, static_cast<uint32_t>(mapped(ctx, order[count - 1 - k])), 0x120, ly);
    int32_t next = add32(ly, 0xE);
    if (k == 0) {
      blt(g, ctx, 25, 0x120, next);
      next = add32(ly, 0x10);
    }
    ly = next;
  }
  blt(g, ctx, 22, control(2) ? 0x121 : 0x120, 0x15F);
  blt(g, ctx, 24, 0x139, 0x1E4);
}

// showBulletinBoard.tickHover(lit) (P26C item 5, dialogs.cpp; drawing only): lit = 1 draws the tick's pixels (those the tick
// changed over the board, its rim excluded: a pixel next to an unchanged one is the art's blend with the board; and its near-black
// drop shadow excluded: its 5-bit channels summing below 16) lightened toward white by 80 / 256, 0 puts the tick back as the board drew it
void b_tick_hover(Gfx &g, Ctx &ctx, const uint32_t *a, int n) {
  const BoardState &b = board(g);
  const size_t cnt = static_cast<size_t>(kTickW) * static_cast<size_t>(kTickH);
  if (b.tick_plain.size() != cnt || b.tick_under.size() != cnt) return;
  g.ensure_root(ctx);
  Surface16 &r = g.root;
  const bool lit = n > 0 && a[0] != 0;
  auto up = [](uint32_t c) { return c + ((31u - c) * 80u >> 8); };
  auto tick = [&](int32_t xx, int32_t yy) {
    if (xx < 0 || yy < 0 || xx >= kTickW || yy >= kTickH) return false;
    const size_t j = static_cast<size_t>(yy) * static_cast<size_t>(kTickW) + static_cast<size_t>(xx);
    return b.tick_plain[j] != b.tick_under[j];
  };
  for (int32_t yy = 0; yy < kTickH; ++yy)
    for (int32_t xx = 0; xx < kTickW; ++xx) {
      const int32_t X = kTickX + xx, Y = kTickY + yy;
      if (X < 0 || X >= r.w || Y < 0 || Y >= r.h) continue;
      const size_t j = static_cast<size_t>(yy) * static_cast<size_t>(kTickW) + static_cast<size_t>(xx);
      uint16_t w = b.tick_plain[j];
      const bool bright = (w >> 10 & 31u) + (w >> 5 & 31u) + (w & 31u) >= 16;
      if (lit && bright && tick(xx, yy) && tick(xx - 1, yy) && tick(xx + 1, yy) && tick(xx, yy - 1) && tick(xx, yy + 1))
        w = static_cast<uint16_t>((w & 0x8000u) | up(w >> 10 & 31u) << 10 | up(w >> 5 & 31u) << 5 | up(w & 31u));
      r.px[static_cast<size_t>(Y) * static_cast<size_t>(r.w) + static_cast<size_t>(X)] = w;
    }
}

const GfxBodyDef kBodies[] = {
    {"showBulletinBoard.tickHover", b_tick_hover},
    {"loadInterfaceArt.bulletinArt", b_bulletin_art},
    {"showBulletinBoard.board", b_board},
    {"showBulletinBoard.capture", b_capture},
    {"showBulletinBoard.event", b_event},
    {"showBulletinBoard.photo", b_photo},
    {"showBulletinBoard.fixed", b_fixed},
};

}  // namespace

extern const GfxFamily kGfxFamily_bulletin = {"bulletin", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
