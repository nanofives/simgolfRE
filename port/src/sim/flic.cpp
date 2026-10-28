// Module `flic` (roadmap S3.4, 2026-10-24): the FLIC sprite-sheet decoder (portsim/sim/flic.h). Notes
// log/roadmap/S3/4_notes.md; tests port/test/test_flic.py against the original's sheet memory (port/corpus/flic/,
// re/tools/flic_cells_rec.py). Written from the C3 / C4 notes of the original's flic chain and loadFlic's instructions
// (re/analysis/render/0043d740_loadFlic.md); functions are cited by name.
//
// The original's chain, per file: Flic::open maps the file, Flic::buildFrameTable checks the FLIC magic word 0xaf12 (+4) and
// depth 8 (+0xc) and keeps a header carrying SimGolf's conversion mark 0xf1f1f2f2 (+0x1a) in place, Flic::indexFrames walks
// the frame chunks from the header's +0x50 into one table of sequence starts (header word +0x60 entries) and one of frames
// (word +0x62 per sequence), Flic::createStream makes the 8-bit frame Surface (words +8 x +0xa) with its draw offset (words
// +0x64 / +0x66) and seeks to sequence 0. Image::seek / Image::step decode one top-level chunk per step; decodeImageChunks
// walks a frame chunk (0xf1fa)'s sub-chunks: 7 the word delta (decodeChunk7WordDelta), 15 the byte run (Surface::blitRLE),
// 16 a raw copy (Surface::copyRaw), 13 a clear (Surface::clear), 4 and 11 the colour packets (Palette::decodeChunk), 12 and
// every other type nothing.
//
// Negative controls (g_negctl_flic; port/test/test_flic.py reads each RED against the recordings):
//   1 the first word-delta chunk of every stream is skipped
//   2 the colour chunks are ignored (no palette)
//   3 the frames of each sheet row are cut in reverse order
//   4 the draw copies byte 0xfe (transparency test `> 0xfe` instead of `>= 0xfe`)
//   5 the underscore alias is applied at switch value 1 too (the original profile would load the install's names)
#include "portsim/sim/flic.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#if __has_include("gfx_body.h")   // the gfx backend's body interface (roadmap S2 wave 1); without it no gfx hook-up
#include "gfx_body.h"
#define SG_FLIC_GFX 1
#else
#define SG_FLIC_GFX 0
#endif
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/gfx.h"

namespace portsim {
namespace flic {

namespace {
int g_negctl_flic = 0;
// Defect switch flic.treeAliasNames (default 1 = the original; log/roadmap/S3/4b_notes.md): loadWorldAssets asks for the
// tropical trees `Tree Cerc` / `Tree Drac` with a space and the install holds `Tree_Cerc` / `Tree_Drac`, so the original's
// flicOpenStream returns NULL and the game dies in loadFlic's Image_draw (stock run cov_b_s34_hawaii, U-1330 / U-1331). At 1
// the missing file raises a Fault; at 0 the install's underscore names are loaded.
int g_defect_tree_alias = 1;

constexpr int32_t kWork = 0x1e0;   // loadFlic's working image: Win_fillRect (0, 0, 0x1e0, 0x1e0, 0xff); 480 x 480, pitch 480
constexpr uint8_t kClear = 0xff;
constexpr uint32_t kMark = 0xf1f1f2f2u;
constexpr uint16_t kFrame = 0xf1fa;

struct DecodeError {
  std::string what;
};

// bounds-checked little-endian reads of a file
struct Bytes {
  const std::vector<uint8_t> &b;
  uint32_t u8(uint64_t o) const {
    if (o >= b.size()) throw DecodeError{"read past the file at " + std::to_string(o)};
    return b[static_cast<size_t>(o)];
  }
  uint32_t u16(uint64_t o) const { return u8(o) | (u8(o + 1) << 8); }
  uint32_t u32(uint64_t o) const { return u16(o) | (u16(o + 2) << 16); }
};

// The frame Surface: Surface_create's 8-bit jgld sprite of w x h, pitch w. Sprite::create8 with no bits leaves its jalloc'd
// buffer unwritten; the port fills it with 0xcd (U-1333: the original's content there is not recorded). It is not
// observable with the shipped files: every one of their 9,667 sequences starts with a byte-run frame, which writes every
// pixel before any draw.
struct Frame {
  int32_t w = 0, h = 0;
  std::vector<uint8_t> px;
  uint8_t &at(int64_t o) {
    if (o < 0 || o >= static_cast<int64_t>(px.size())) throw DecodeError{"write outside the frame at " + std::to_string(o)};
    return px[static_cast<size_t>(o)];
  }
};

// One opened animation (a Flic record and its stream link): the header words it reads, the two frame tables, the frame
// Surface, the link's chunk pointer (+0x38), step counter (+0x3c) and sequence slot (+0x40), and the slot's palette.
struct Stream {
  std::vector<uint8_t> file;
  int32_t w = 0, h = 0, seqs = 0, per = 0, dx = 0, dy = 0;
  uint32_t mask = 0;
  std::vector<uint32_t> seq_start;   // Flic::indexFrames' table at +0x48
  Frame frame;
  uint32_t chunk = 0;
  int32_t step = 0, slot = -1;
  Palette pal;
  int delta_seen = 0;
};

// Palette::decodeChunk: packets of (skip, count) with count 0 meaning 0x100, an 8-bit running index, three bytes per entry
void decode_palette(Stream &s, const Bytes &f, uint32_t c) {
  if (SG_NEGCTL(g_negctl_flic) == 2) return;
  const uint32_t packets = f.u16(c + 6);
  uint64_t p = c + 8;
  uint8_t index = 0;
  for (uint32_t k = 0; k < packets; ++k) {
    index = static_cast<uint8_t>(index + f.u8(p));
    uint32_t n = f.u8(p + 1);
    if (n == 0) n = 0x100;
    p += 2;
    for (uint32_t e = 0; e < n; ++e) {
      for (int j = 0; j < 3; ++j) s.pal.rgb[3u * index + j] = static_cast<uint8_t>(f.u8(p + j));
      p += 3;
      index = static_cast<uint8_t>(index + 1);
    }
  }
  s.pal.set = true;
}

// decodeChunk7WordDelta: the line count; per line words with both top bits set skip |(short)w| lines, a word with only the
// top bit stores its low byte at the line's last byte and the next word is the packet count; each packet a skip byte and a
// signed count byte (negative: the next word -count times, -128 writes nothing; positive: count words)
void decode_delta(Stream &s, const Bytes &f, uint32_t c) {
  if (SG_NEGCTL(g_negctl_flic) == 1 && s.delta_seen++ == 0) return;
  const int64_t stride = s.w;
  uint64_t d = c + 6;
  uint32_t lines = f.u16(d);
  d += 2;
  int64_t line = 0;
  for (; lines != 0; --lines) {
    uint32_t w = f.u16(d);
    d += 2;
    while ((w & 0xc000) == 0xc000) {
      line += static_cast<int64_t>(abs32(static_cast<int16_t>(w)) & 0xffff) * stride;
      w = f.u16(d);
      d += 2;
    }
    if ((w & 0x8000) != 0) {
      s.frame.at(line + stride - 1) = static_cast<uint8_t>(w);
      w = f.u16(d);
      d += 2;
    }
    int64_t p = line;
    for (uint32_t packets = w & 0xffff; packets != 0; --packets) {
      const uint32_t skip = f.u8(d);
      const int8_t cnt = static_cast<int8_t>(f.u8(d + 1));
      d += 2;
      p += skip;
      if (cnt < 0) {
        const uint32_t v = f.u16(d);
        d += 2;
        const int n = static_cast<int8_t>(-cnt);
        for (int k = 0; k < n; ++k) {
          s.frame.at(p + 2 * k) = static_cast<uint8_t>(v);
          s.frame.at(p + 2 * k + 1) = static_cast<uint8_t>(v >> 8);
        }
        if (n > 0) p += 2 * n;
      } else if (cnt > 0) {
        for (int k = 0; k < 2 * cnt; ++k) s.frame.at(p + k) = static_cast<uint8_t>(f.u8(d + k));
        p += 2 * cnt;
        d += 2u * static_cast<uint32_t>(cnt);
      }
    }
    line += stride;
  }
}

// Surface::blitRLE: per row one byte skipped, then signed counts until the row's remaining width is exactly 0: negative =
// that many literal bytes, zero or more = the next byte repeated
void decode_byterun(Stream &s, const Bytes &f, uint32_t c) {
  uint64_t src = c + 6;
  int64_t dst = 0;
  for (int32_t row = 0; row < s.h; ++row) {
    ++src;
    int32_t left = s.w;
    if (left == 0) continue;
    while (left != 0) {
      const int8_t cnt = static_cast<int8_t>(f.u8(src++));
      if (cnt < 0) {
        if (cnt == -128) throw DecodeError{"byte run with count -128 (rep movs of a negative count)"};
        const int n = -cnt;
        for (int k = 0; k < n; ++k) s.frame.at(dst + k) = static_cast<uint8_t>(f.u8(src + k));
        src += static_cast<uint64_t>(n);
        dst += n;
        left -= n;
      } else {
        const uint8_t v = static_cast<uint8_t>(f.u8(src++));
        for (int k = 0; k < cnt; ++k) s.frame.at(dst + k) = v;
        dst += cnt;
        left -= cnt;
      }
      if (left < -0x10000) throw DecodeError{"byte-run row overruns its width"};
    }
  }
}

// decodeImageChunks on one chunk (a frame chunk walks its sub-chunks, each advancing by its own size)
void decode_chunk(Stream &s, uint32_t c) {
  const Bytes f{s.file};
  const uint32_t type = f.u16(c + 4);
  switch (type) {
    case 4:
    case 11:
      decode_palette(s, f, c);
      return;
    case 7:
      decode_delta(s, f, c);
      return;
    case 13:   // Surface::clear
      std::fill(s.frame.px.begin(), s.frame.px.end(), uint8_t{0});
      return;
    case 15:
      decode_byterun(s, f, c);
      return;
    case 16:   // Surface::copyRaw: w * h bytes from chunk + 6
      for (size_t k = 0; k < s.frame.px.size(); ++k) s.frame.px[k] = static_cast<uint8_t>(f.u8(c + 6 + k));
      return;
    case kFrame: {
      const uint32_t n = f.u16(c + 6);
      uint32_t p = c + 0x10;
      for (uint32_t k = 0; k < n; ++k) {
        decode_chunk(s, p);
        p += f.u32(p);
      }
      return;
    }
    default:   // 12 (the empty decoder) and unknown types
      return;
  }
}

// Image::step: the counter counts up, the chunk at the pointer is decoded, a counter equal to the sequence length restarts
// at the sequence's first chunk; then the pointer advances by the chunk's size
void image_step(Stream &s) {
  const Bytes f{s.file};
  ++s.step;
  decode_chunk(s, s.chunk);
  if (s.step == s.per) {
    s.chunk = s.seq_start.at(static_cast<size_t>(s.slot));
    s.step = 0;
  }
  s.chunk += f.u32(s.chunk);
}

// Image::seek (frame, pos): pos -1 = slot `frame`, step 0; else with a mask the frame's bit must be set (else nothing
// happens) and the slot is the count of set bits below it, without one frame % sequences; a new slot restarts there
bool image_seek(Stream &s, int32_t frame, int32_t pos) {
  const Bytes f{s.file};
  int32_t slot, p = pos;
  if (pos == -1) {
    slot = frame;
    p = 0;
  } else if (s.mask != 0) {
    if ((s.mask & (1u << (frame & 31))) == 0) return false;
    int32_t n = 0;
    for (int32_t i = 0; i < frame; ++i)
      if (s.mask & (1u << (i & 31))) ++n;
    slot = n;
  } else {
    if (s.seqs == 0) throw DecodeError{"Image::seek divides by a sequence count of 0"};
    slot = frame % s.seqs;
  }
  if (s.per == 0) throw DecodeError{"Image::seek divides by a sequence length of 0"};
  const int32_t target = p % s.per;
  if (s.slot != slot) {
    s.slot = slot;
    s.step = 0;
    s.chunk = s.seq_start.at(static_cast<size_t>(slot));
    decode_chunk(s, s.chunk);
    s.chunk += f.u32(s.chunk);
  }
  while (s.step != target) image_step(s);
  return true;
}

// flicOpenStream -> Flic::open / buildFrameTable / indexFrames / createStream; kOk or the status the NULL stream leads to
int32_t open(Stream &s, std::vector<uint8_t> bytes, std::string &detail) {
  s.file = std::move(bytes);
  const Bytes f{s.file};
  if (s.file.size() < 0x74) {
    detail = "header shorter than 0x74 bytes";
    return -1;
  }
  if (f.u16(4) != 0xaf12 || f.u16(0xc) != 8) {
    detail = "not an 8-bit FLC (Flic::buildFrameTable's 0x16 / 0x17)";
    return -1;
  }
  if (f.u32(0x1a) != kMark) return kUnconverted;
  s.w = static_cast<int32_t>(f.u16(8));
  s.h = static_cast<int32_t>(f.u16(0xa));
  s.seqs = static_cast<int32_t>(f.u16(0x60));
  s.per = static_cast<int32_t>(f.u16(0x62));
  s.dx = static_cast<int32_t>(f.u16(0x64));
  s.dy = static_cast<int32_t>(f.u16(0x66));
  s.mask = f.u32(0x70);
  // Flic::indexFrames (N = sequences, M = frames per sequence): each sequence's first chunk, then M + 1 steps to the next
  // frame chunk (the last sequence stops after M)
  uint32_t cur = f.u32(0x50);
  for (int32_t q = 0; q < s.seqs; ++q) {
    s.seq_start.push_back(cur);
    for (int32_t g = 0; g < s.per + 1; ++g) {
      if (g == s.per && q == s.seqs - 1) break;
      do {
        const uint32_t sz = f.u32(cur);
        if (sz == 0) throw DecodeError{"a zero-size chunk in Flic::indexFrames' walk"};
        cur += sz;
      } while (f.u16(cur + 4) != kFrame);
    }
  }
  s.frame.w = s.w;
  s.frame.h = s.h;
  s.frame.px.assign(static_cast<size_t>(s.w) * static_cast<size_t>(s.h), 0xcd);
  if (s.seqs > 0) image_seek(s, 0, -1);   // createStream's Link482b::send (0, -1, 0)
  return kOk;
}

// flicNextFrame: within a sequence one step; at its last frame the next sequence (Link482b::send (slot + 1, 0, 0), whose
// failure leaves the frame as it is); past the last sequence NULL
bool next_frame(Stream &s) {
  if (static_cast<uint32_t>(s.step) < static_cast<uint32_t>(s.per - 1)) {
    image_step(s);
    return true;
  }
  if (s.slot == s.seqs - 1) return false;
  image_seek(s, s.slot + 1, 0);
  return true;
}

// Image_draw of the frame Surface at its offset into the working image: jgld Sprite::drawB -> the 8-bit to 8-bit body,
// clipped to the image, bytes >= 0xfe transparent
void draw(const Stream &s, std::vector<uint8_t> &work) {
  const uint8_t key = SG_NEGCTL(g_negctl_flic) == 4 ? 0xff : 0xfe;
  for (int32_t y = 0; y < s.h; ++y) {
    const int32_t wy = s.dy + y;
    if (wy < 0 || wy >= kWork) continue;
    for (int32_t x = 0; x < s.w; ++x) {
      const int32_t wx = s.dx + x;
      if (wx < 0 || wx >= kWork) continue;
      const uint8_t v = s.frame.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)];
      if (v < key) work[static_cast<size_t>(wy) * kWork + static_cast<size_t>(wx)] = v;
    }
  }
}

void fill(std::vector<uint8_t> &work, const Rect &r) {
  for (int32_t y = std::max(r.y, 0); y < std::min(r.y + r.h, kWork); ++y)
    for (int32_t x = std::max(r.x, 0); x < std::min(r.x + r.w, kWork); ++x) work[static_cast<size_t>(y) * kWork + x] = kClear;
}

// cropSprite (kind 1, trim set) of the working image's rectangle r: Surface_free clears +0x10 .. +0x24 first
Cell crop(const std::vector<uint8_t> &work, const Rect &r) {
  Cell c;
  if (r.w == 0 || r.h == 0 || add32(r.x, r.w) > kWork || add32(r.y, r.h) > kWork) {
    c.result = 3;
    return c;
  }
  c.w = r.w;
  c.h = r.h;
  auto row = [&](int32_t k) { return work.data() + static_cast<size_t>(r.y + k) * kWork + r.x; };
  auto blank = [&](int32_t k) {
    const uint8_t *p = row(k);
    for (int32_t i = 0; i < r.w; ++i)
      if (p[i] != kClear) return false;
    return true;
  };
  while (c.top < r.h && blank(c.top)) ++c.top;
  if (c.top == r.h) {
    c.result = 9;
    return c;
  }
  c.keep_h = r.h - c.top;
  for (int32_t k = r.h - 1; k >= 0 && blank(k); --k) --c.keep_h;
  c.left = r.w;
  int32_t right = r.w;
  for (int32_t k = c.top; k < c.top + c.keep_h; ++k) {
    const uint8_t *p = row(k);
    int32_t a = 0;
    while (a < r.w && p[a] == kClear) ++a;
    c.left = std::min(c.left, a);
    int32_t b = 0;
    while (b < r.w && p[r.w - 1 - b] == kClear) ++b;
    right = std::min(right, b);
  }
  c.keep_w = r.w - right - c.left;
  if (c.keep_w == 0) {
    c.result = 9;
    return c;
  }
  c.px.resize(static_cast<size_t>(c.keep_w) * static_cast<size_t>(c.keep_h));
  for (int32_t k = 0; k < c.keep_h; ++k)
    std::memcpy(c.px.data() + static_cast<size_t>(k) * c.keep_w, row(c.top + k) + c.left, static_cast<size_t>(c.keep_w));
  return c;
}

bool read_file(const GameFiles &files, const std::string &name, std::vector<uint8_t> &out) {
  if (!files.exists(name)) return false;
  out = files.read_all(name);
  return true;
}
}  // namespace

// loadFlic's four crop arms: size 0 the 60 x 60 square at (210, 200); 1 and -1 280 x 240 at (100, 100); 240 the whole image;
// any other s the 2s x 3s/2 rectangle at (240 - s, 240 - s) (3s / 2 truncated toward zero)
Rect crop_rect(int32_t size) {
  if (size == 0) return {0xd2, 0xc8, 0x3c, 0x3c};
  if (size == 1 || size == -1) return {0x64, 0x64, 0x118, 0xf0};
  if (size == 0xf0) return {0, 0, kWork, kWork};
  const int32_t t = mul32(size, 3);
  return {sub32(0xf0, size), sub32(0xf0, size), mul32(size, 2), t / 2};
}

// The path with the tropical tree names' spaces turned into underscores (the install's spelling); empty when it holds neither
std::string tree_underscore_path(const std::string &path) {
  std::string out = path;
  bool hit = false;
  for (const char *n : {"Tree Cerc", "Tree Drac"}) {
    const size_t at = out.find(n);
    if (at == std::string::npos) continue;
    out[at + 4] = '_';
    hit = true;
  }
  return hit ? out : std::string();
}

// The backend-free entry (menu.cpp flic_cols): the file name a load opens. A name holding Tree Cerc / Tree Drac whose file is
// missing: switch 1 faults (the original's open returns NULL and Image_draw reads 0x8 of it), switch 0 gives the install's
// underscore name; any other name comes back unchanged.
std::string resolve_name(const GameFiles &files, const std::string &name) {
  const std::string alt = tree_underscore_path(name);
  if (alt.empty() || files.exists(name)) return name;
  if (SG_DEFECT(g_defect_tree_alias) && SG_NEGCTL(g_negctl_flic) != 5) throw Fault{8, 4};
  return files.exists(alt) ? alt : name;
}

Result load(const GameFiles &files, const Names &names, const Load &args) {
  Result res;
  Stream main, shadow;
  std::vector<uint8_t> work(static_cast<size_t>(kWork) * kWork, kClear);
  Load a = args;
  try {
    // a path naming `Tree Cerc` / `Tree Drac` whose file the install lacks: the original's open returns NULL and the draw
    // through it faults; the fixed profile reads the underscore names instead
    const std::string alt = tree_underscore_path(a.path);
    if (!alt.empty() && !files.exists(names.prefix + a.path + names.main_suffix)) {
      if (SG_DEFECT(g_defect_tree_alias) && SG_NEGCTL(g_negctl_flic) != 5) throw Fault{8, 4};
      if (files.exists(names.prefix + alt + names.main_suffix)) a.path = alt;
    }
    const int32_t rows = a.size == -1 ? 1 : a.frames;
    const std::string main_name = names.prefix + a.path + names.main_suffix;
    // the shadow (flag set and the file there: fileExists), opened before the main animation, drawn under it
    if (a.flag != 0) {
      const std::string sname = names.prefix + a.path + names.shadow_suffix;
      std::vector<uint8_t> b;
      if (read_file(files, sname, b)) {
        const int32_t st = open(shadow, std::move(b), res.detail);
        if (st != kOk) {
          res.status = st == kUnconverted ? kUnconverted : kShadowOpenFailed;
          res.detail = sname + ": " + res.detail;
          return res;
        }
        res.shadow = true;
        draw(shadow, work);
      }
    }
    std::vector<uint8_t> b;
    if (!read_file(files, main_name, b)) {
      res.status = kMainOpenFailed;
      res.detail = main_name + ": no such file";
      return res;
    }
    const int32_t st = open(main, std::move(b), res.detail);
    if (st != kOk) {
      res.status = st == kUnconverted ? kUnconverted : kMainOpenFailed;
      res.detail = main_name + ": " + res.detail;
      return res;
    }
    res.cols = main.per;
    res.seqs = main.seqs;
    draw(main, work);
    const Rect r = crop_rect(a.size);
    if (r.x < 0 || r.y < 0) {
      res.status = kBadCrop;
      return res;
    }
    const Rect clear = a.size == 0 ? Rect{0xd2, 0xc8, 0x3c, 0x3c} : Rect{0, 0, kWork, kWork};
    bool first = true;
    for (int32_t row = 0; row < rows && res.status == kOk; ++row) {
      std::vector<Cell> line;
      for (int32_t col = 0; col < main.per; ++col) {
        if (!first) {
          fill(work, clear);
          if (res.shadow) {
            if (!next_frame(shadow)) {
              res.status = kNoNextFrame;
              res.detail = "shadow: past its last sequence";
              break;
            }
            draw(shadow, work);
          }
          if (!next_frame(main)) {
            res.status = kNoNextFrame;
            res.detail = "past the last sequence";
            break;
          }
          draw(main, work);
        }
        first = false;
        line.push_back(crop(work, r));
        if (a.size == -1) break;
      }
      if (SG_NEGCTL(g_negctl_flic) == 3) std::reverse(line.begin(), line.end());
      for (auto &c : line) res.cells.push_back(std::move(c));
      if (a.size == -1) break;
    }
  } catch (const DecodeError &e) {
    res.status = kDecodeFault;
    res.detail = e.what;
  }
  res.pal = main.pal;
  res.shadow_pal = shadow.pal;
  return res;
}

namespace {
std::string vstr(const Memory &m, uint32_t va);
}

int32_t sheet_to_gfx(Ctx &ctx, uint32_t path_va, int32_t frames, int32_t size, int32_t flag, uint32_t first_cell_va) {
#if SG_FLIC_GFX
  Gfx *g = gfx(ctx);
  if (!g) return -1;
  // loadFlic's name parts: the text pool's +0x3c (prefix), +0x28 (main suffix) and +0x30 (shadow suffix), as menu.cpp
  // load_flic reads them
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c8a28);
  const Names n{vstr(ctx.mem, pool + 0x3c), vstr(ctx.mem, pool + 0x28), vstr(ctx.mem, pool + 0x30)};
  const Load l{vstr(ctx.mem, path_va), frames, size, flag};
  const Result res = load(ctx.files(), n, l);
  if (res.status != kOk) {
    g->unsup("loadFlic: " + res.detail);
    return res.status;
  }
  // the cells carry the slot's palette as RGB555 (which palette the original draws a sheet cell with is the sprite-queue
  // step's to settle: cropSprite gives the cell's jgld Sprite palette 0, U-1332)
  std::array<uint16_t, 256> pal{};
  for (int i = 0; i < 256; ++i)
    pal[i] = gfxb::rgb555(res.pal.rgb[3 * i], res.pal.rgb[3 * i + 1], res.pal.rgb[3 * i + 2]);
  for (size_t k = 0; k < res.cells.size(); ++k) {
    const uint32_t key = first_cell_va + 0x2cu * static_cast<uint32_t>(k);
    const Cell &c = res.cells[k];
    g->sprites.erase(key);
    if (c.result != 0) continue;
    Sprite8 s;
    s.w = c.keep_w;
    s.h = c.keep_h;
    s.left = c.left;
    s.top = c.top;
    s.px = c.px;
    s.pal = pal;
    g->sprites[key] = std::move(s);
  }
  return kOk;
#else
  (void)ctx, (void)path_va, (void)frames, (void)size, (void)flag, (void)first_cell_va;
  return -1;
#endif
}

namespace {
// ------------------------------------------------------------------------------------------------ test entry
// flic.load(path, frames, size, flag, names): decodes through the context's GameFiles; `path` and `names` are addresses of
// NUL-terminated strings in the context's address space (names = prefix, main suffix, shadow suffix, one after another).
// The result goes to the files overlay as `$flic/load.bin`: i32 status, cols, seqs, shadow, cell count; per cell i32 result,
// keep_w, keep_h, w, h, left, top, kind, pixel count, then the pixels; then per palette (main, shadow) i32 set + 0x300 bytes;
// then the detail text. Returns the status.
std::string vstr(const Memory &m, uint32_t va) {
  std::string s;
  for (uint32_t k = 0; k < 0x1000; ++k) {
    const uint8_t c = m.load<uint8_t>(va + k);
    if (c == 0) break;
    s.push_back(static_cast<char>(c));
  }
  return s;
}
void put32(std::vector<uint8_t> &o, int32_t v) {
  for (int k = 0; k < 4; ++k) o.push_back(static_cast<uint8_t>(static_cast<uint32_t>(v) >> (8 * k)));
}
void a_load(Ctx &ctx, const uint32_t *a, Ret &r) {
  Load l;
  l.path = vstr(ctx.mem, a[0]);
  l.frames = static_cast<int32_t>(a[1]);
  l.size = static_cast<int32_t>(a[2]);
  l.flag = static_cast<int32_t>(a[3]);
  Names n;
  uint32_t p = a[4];
  n.prefix = vstr(ctx.mem, p);
  p += static_cast<uint32_t>(n.prefix.size()) + 1;
  n.main_suffix = vstr(ctx.mem, p);
  p += static_cast<uint32_t>(n.main_suffix.size()) + 1;
  n.shadow_suffix = vstr(ctx.mem, p);
  const Result res = load(ctx.files(), n, l);
  std::vector<uint8_t> o;
  put32(o, res.status);
  put32(o, res.cols);
  put32(o, res.seqs);
  put32(o, res.shadow ? 1 : 0);
  put32(o, static_cast<int32_t>(res.cells.size()));
  for (const Cell &c : res.cells) {
    for (int32_t v : {c.result, c.keep_w, c.keep_h, c.w, c.h, c.left, c.top, c.kind, static_cast<int32_t>(c.px.size())})
      put32(o, v);
    o.insert(o.end(), c.px.begin(), c.px.end());
  }
  for (const Palette *pp : {&res.pal, &res.shadow_pal}) {
    put32(o, pp->set ? 1 : 0);
    o.insert(o.end(), pp->rgb.begin(), pp->rgb.end());
  }
  o.insert(o.end(), res.detail.begin(), res.detail.end());
  GameFiles &f = ctx.files();   // the files overlay, never the install (gfx.cpp put_file's rule)
  auto *d = dynamic_cast<DirGameFiles *>(&f);
  if ((d && d->write_through()) || !f.write("$flic/load.bin", o))
    throw MissingResource{"flic.load: the files overlay refused $flic/load.bin"};
  r.set_i32(res.status);
}

// flic.toGfx(path, frames, size, flag, cell): sheet_to_gfx with the cells from g_cellPool[cell] on; returns its result
void a_to_gfx(Ctx &ctx, const uint32_t *a, Ret &r) {
  const uint32_t first = state_va(ctx, &GameState::g_cellPool) + 0x2cu * a[4];
  r.set_i32(sheet_to_gfx(ctx, a[0], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), first));
}

const FuncDef kFuncs[] = {
    {"flic.load", "default", "int", 5, 0, a_load, &g_negctl_flic},
    {"flic.toGfx", "default", "int", 5, 0, a_to_gfx, &g_negctl_flic},
};

const DefectDef kDefects[] = {
    {"flic.treeAliasNames", "S3.4b (U-1331, D5)",
     "a load of `Tree Cerc` / `Tree Drac` (tropical courses) whose file is missing faults like the original's NULL open; at 0 the "
     "install's Tree_Cerc / Tree_Drac files are loaded",
     &g_defect_tree_alias},
};
}  // namespace

}  // namespace flic

extern const ModuleDef kModule_flic = {"flic", flic::kFuncs, sizeof(flic::kFuncs) / sizeof(flic::kFuncs[0]), flic::kDefects,
                                       sizeof(flic::kDefects) / sizeof(flic::kDefects[0])};

}  // namespace portsim
