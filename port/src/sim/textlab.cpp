// The label pixel lab (roadmap step S5.2): textlab.h. Provider-neutral: every pixel comes from the attached
// TextMetrics provider's draw_glyphs (commands.h), the lab only places the text, the surface and the colour.
#include "portsim/sim/textlab.h"

#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/gditext.h"

namespace portsim {

int g_negctl_textlab = 0;

namespace {

TextMetrics &provider(Ctx &ctx) {
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) throw MissingResource{"text provider"};
  return *tm;
}

std::string bytes_at(Ctx &ctx, uint32_t va, int32_t n) {
  if (n <= 0) return std::string();
  const uint8_t *p = ctx.mem.at(va, static_cast<uint32_t>(n));
  return std::string(reinterpret_cast<const char *>(p), static_cast<size_t>(n));
}

// The draw itself on host words, with the negative controls applied. `text_va` (may be 0) holds the same bytes as
// `text` in the address space, for the provider's width of a prefix (control 1).
bool draw_px(Ctx &ctx, TextMetrics &tm, uint32_t fontref, std::string text, uint32_t text_va, int32_t x, int32_t y,
             uint32_t colorref, uint16_t *px, int32_t w, int32_t h) {
  const int mode = SG_NEGCTL(g_negctl_textlab);
  if (mode == 2) colorref = (colorref & ~0xffu) | (((colorref & 0xff) + 8) & 0xff);
  if (mode == 3 && !text.empty()) text.pop_back();
  if (mode == 4) x += 1;
  if (mode == 1 && !text.empty()) {
    // the run without its last byte at x, the last byte alone at x + the prefix's width + 1
    const int32_t n = static_cast<int32_t>(text.size());
    int32_t pw = 0;
    if (n > 1 && (text_va == 0 || !tm.glyph_width(ctx, fontref, text_va, n - 1, pw))) return false;
    if (n > 1 && !tm.draw_glyphs(ctx, fontref, text.substr(0, n - 1), x, y, colorref, px, w, h)) return false;
    return tm.draw_glyphs(ctx, fontref, text.substr(n - 1), x + pw + 1, y, colorref, px, w, h);
  }
  if (text.empty()) return true;
  return tm.draw_glyphs(ctx, fontref, text, x, y, colorref, px, w, h);
}

uint16_t *surface_at(Ctx &ctx, uint32_t surf, int32_t w, int32_t h) {
  if (w <= 0 || h <= 0 || static_cast<uint64_t>(w) * static_cast<uint64_t>(h) > (1u << 26)) throw Fault{surf, 0};
  return reinterpret_cast<uint16_t *>(ctx.mem.at(surf, static_cast<uint32_t>(2u * w * h), true));
}

// ---- the recorder
// RECORD FORMAT of `$textlab/calls.bin` (little-endian): u32 'TLC1', u32 count, then per draw_glyphs call
//   u32 tag (the count of `$gfx/snapshot_<k>.555` files present when the call was made: the call precedes snapshot k
//            for every k >= tag), u32 fontref, u32 colorref, i32 x, i32 y, i32 w, i32 h, u32 n, n text bytes padded to 4,
//   i32 bx0, by0, bx1, by1 (the ink box, end exclusive, clipped to the surface; bx1 <= bx0 = no ink), u32 drawn (1 / 0),
//   then the box's surface words before the call and after it ((bx1 - bx0) * (by1 - by0) u16 each), padded to 4.
// The ink box is kMargin (4) words around where the provider's own glyphs for this call put any pixel: the call redrawn by the provider on two
// scratch surfaces, white (RGB 255 255 255) on 0x0000 and black on 0x7fff, every word that changed on either.
constexpr int32_t kMargin = 4;

class CallLog {
 public:
  // `draw(colorref, px)` is the provider's own draw_glyphs of this call on host words px (w x h)
  bool record(Ctx &ctx, uint32_t f, const std::string &text, int32_t x, int32_t y, uint32_t colorref, uint16_t *px,
              int32_t w, int32_t h, const std::function<bool(uint32_t, uint16_t *)> &draw) {
    const size_t npx = static_cast<size_t>(w) * static_cast<size_t>(h);
    int32_t bx0 = w, by0 = h, bx1 = 0, by1 = 0;
    for (int pass = 0; pass < 2 && w > 0 && h > 0; ++pass) {
      const uint16_t bg = pass == 0 ? 0x0000 : 0x7fff;
      scratch_.assign(npx, bg);
      if (!draw(pass == 0 ? 0xffffffu : 0u, scratch_.data())) break;
      for (int32_t r = 0; r < h; ++r)
        for (int32_t c = 0; c < w; ++c)
          if (scratch_[static_cast<size_t>(r) * w + c] != bg) {
            if (c < bx0) bx0 = c;
            if (c + 1 > bx1) bx1 = c + 1;
            if (r < by0) by0 = r;
            if (r + 1 > by1) by1 = r + 1;
          }
    }
    if (bx1 <= bx0 || by1 <= by0) {
      bx0 = by0 = bx1 = by1 = 0;
    } else {   // kMargin words around the ink, clipped: room for another provider's glyphs to land off the GDI ink
      bx0 = bx0 > kMargin ? bx0 - kMargin : 0;
      by0 = by0 > kMargin ? by0 - kMargin : 0;
      bx1 = bx1 + kMargin < w ? bx1 + kMargin : w;
      by1 = by1 + kMargin < h ? by1 + kMargin : h;
    }
    std::vector<uint16_t> before = crop(px, w, bx0, by0, bx1, by1);
    const bool ok = draw(colorref, px);
    std::vector<uint16_t> after = crop(px, w, bx0, by0, bx1, by1);
    uint32_t tag = 0;
    while (ctx.files().exists("$gfx/snapshot_" + std::to_string(tag) + ".555")) ++tag;
    u32(tag);
    u32(f);
    u32(colorref);
    u32(static_cast<uint32_t>(x));
    u32(static_cast<uint32_t>(y));
    u32(static_cast<uint32_t>(w));
    u32(static_cast<uint32_t>(h));
    u32(static_cast<uint32_t>(text.size()));
    out_.insert(out_.end(), text.begin(), text.end());
    pad();
    u32(static_cast<uint32_t>(bx0));
    u32(static_cast<uint32_t>(by0));
    u32(static_cast<uint32_t>(bx1));
    u32(static_cast<uint32_t>(by1));
    u32(ok ? 1u : 0u);
    for (uint16_t v : before) u16(v);
    for (uint16_t v : after) u16(v);
    pad();
    ++count_;
    return ok;
  }
  std::vector<uint8_t> file() const {
    std::vector<uint8_t> b;
    auto w32 = [&](uint32_t v) {
      for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    w32(0x31434c54u);   // 'TLC1'
    w32(count_);
    b.insert(b.end(), out_.begin(), out_.end());
    return b;
  }

 private:
  static std::vector<uint16_t> crop(const uint16_t *px, int32_t w, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    std::vector<uint16_t> v;
    for (int32_t r = y0; r < y1; ++r)
      for (int32_t c = x0; c < x1; ++c) v.push_back(px[static_cast<size_t>(r) * w + c]);
    return v;
  }
  void u32(uint32_t v) {
    for (int i = 0; i < 4; ++i) out_.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  void u16(uint16_t v) {
    out_.push_back(static_cast<uint8_t>(v & 0xff));
    out_.push_back(static_cast<uint8_t>(v >> 8));
  }
  void pad() {
    while (out_.size() & 3) out_.push_back(0);
  }
  std::vector<uint16_t> scratch_;
  std::vector<uint8_t> out_;
  uint32_t count_ = 0;
};

// The recorder attached in place of the provider. A provider whose own registered entries find it by its class
// (gditext's Text_put / drawMarkupRun / measureTextWidth dynamic_cast the context's provider to GdiTextMetrics) is
// replaced by a recorder OF that class (GdiRecorder: a GdiTextMetrics of its own, the same fonts rebuilt, every call its
// own; draw_glyphs logged around the base class's); any other provider is wrapped (WrapRecorder forwards every call).
class WrapRecorder : public TextMetrics {
 public:
  explicit WrapRecorder(TextMetrics *inner) : inner_(inner) {}
  CallLog log;
  int32_t text_put(Ctx &ctx, uint32_t dc, uint32_t font, uint32_t text) override {
    return inner_->text_put(ctx, dc, font, text);
  }
  bool glyph_width(Ctx &ctx, uint32_t f, uint32_t s, int32_t n, int32_t &w) override {
    return inner_->glyph_width(ctx, f, s, n, w);
  }
  bool glyph_ascent(Ctx &ctx, uint32_t f, int32_t &a) override { return inner_->glyph_ascent(ctx, f, a); }
  bool draw_glyphs(Ctx &ctx, uint32_t f, const std::string &text, int32_t x, int32_t y, uint32_t colorref, uint16_t *px,
                   int32_t w, int32_t h) override {
    return log.record(ctx, f, text, x, y, colorref, px, w, h, [&](uint32_t c, uint16_t *p) {
      return inner_->draw_glyphs(ctx, f, text, x, y, c, p, w, h);
    });
  }

 private:
  TextMetrics *inner_;
};
class GdiRecorder : public GdiTextMetrics {
 public:
  CallLog log;
  bool draw_glyphs(Ctx &ctx, uint32_t f, const std::string &text, int32_t x, int32_t y, uint32_t colorref, uint16_t *px,
                   int32_t w, int32_t h) override {
    return log.record(ctx, f, text, x, y, colorref, px, w, h, [&](uint32_t c, uint16_t *p) {
      return GdiTextMetrics::draw_glyphs(ctx, f, text, x, y, c, p, w, h);
    });
  }
};
struct Recording {
  TextMetrics *prior = nullptr;
  std::unique_ptr<TextMetrics> rec;
  CallLog *log = nullptr;
};
std::map<const Ctx *, Recording> &recorders() {
  static std::map<const Ctx *, Recording> m;
  return m;
}

void a_draw(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(textlab_draw(c, a[0], a[1], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
                         static_cast<int32_t>(a[4]), a[5], a[6], static_cast<int32_t>(a[7]),
                         static_cast<int32_t>(a[8]))
                ? 1
                : 0);
}

// JOB FORMAT of textlab.batch (little-endian, each job kJobSize = 0x2c bytes):
//   +0x00 u32 fontref, +0x04 u32 colorref, +0x08 u32 bg (low 16 bits: the surface's fill word), +0x0c i32 x, +0x10 i32 y,
//   +0x14 i32 w, +0x18 i32 h, +0x1c u32 text (address of the bytes), +0x20 i32 n, +0x24 u32 shadow colorref
//   (0xffffffff = none; else the run is drawn first in that colour at x + sdx, y + sdy, as the run drawer draws a
//   shadow), +0x28 the shadow offsets (high 16 bits sdx, low 16 bits sdy, signed).
// The job's surface (w * h words) is written at `out` + the sum of the previous jobs' surface sizes, rounded up to 4
// bytes; a job whose surface does not fit in out_size stops the batch.
constexpr uint32_t kJobSize = 0x2c;
void a_batch(Ctx &c, const uint32_t *a, Ret &r) {
  TextMetrics &tm = provider(c);
  const uint32_t jobs = a[0], count = a[1], out = a[2], out_size = a[3];
  uint32_t used = 0;
  int32_t drawn = 0;
  std::vector<uint16_t> s;
  for (uint32_t k = 0; k < count; ++k) {
    const uint32_t j = jobs + k * kJobSize;
    const uint32_t font = c.mem.load<uint32_t>(j + 0x00), colour = c.mem.load<uint32_t>(j + 0x04);
    const uint16_t bg = static_cast<uint16_t>(c.mem.load<uint32_t>(j + 0x08));
    const int32_t x = c.mem.load<int32_t>(j + 0x0c), y = c.mem.load<int32_t>(j + 0x10);
    const int32_t w = c.mem.load<int32_t>(j + 0x14), h = c.mem.load<int32_t>(j + 0x18);
    const uint32_t text = c.mem.load<uint32_t>(j + 0x1c);
    const int32_t n = c.mem.load<int32_t>(j + 0x20);
    const uint32_t shadow = c.mem.load<uint32_t>(j + 0x24);
    const int32_t sdx = c.mem.load<int32_t>(j + 0x28) >> 16, sdy = static_cast<int16_t>(c.mem.load<uint32_t>(j + 0x28));
    if (w <= 0 || h <= 0) throw Fault{j, 0};
    const uint32_t bytes = (2u * static_cast<uint32_t>(w) * static_cast<uint32_t>(h) + 3u) & ~3u;
    if (used + bytes > out_size) break;
    s.assign(static_cast<size_t>(w) * h, bg);
    const std::string str = bytes_at(c, text, n);
    bool ok = true;
    if (shadow != 0xffffffffu) ok = draw_px(c, tm, font, str, text, x + sdx, y + sdy, shadow, s.data(), w, h);
    ok = ok && draw_px(c, tm, font, str, text, x, y, colour, s.data(), w, h);
    std::memcpy(c.mem.at(out + used, 2u * w * h, true), s.data(), 2u * w * h);
    used += bytes;
    if (ok) ++drawn;
  }
  r.set_i32(drawn);
}

void a_width(Ctx &c, const uint32_t *a, Ret &r) {
  int32_t w = 0;
  r.set_i32(provider(c).glyph_width(c, a[0], a[1], static_cast<int32_t>(a[2]), w) ? w : -1);
}
void a_ascent(Ctx &c, const uint32_t *a, Ret &r) {
  int32_t v = 0;
  r.set_i32(provider(c).glyph_ascent(c, a[0], v) ? v : -1);
}
void a_record_start(Ctx &c, const uint32_t *, Ret &r) {
  if (recorders().count(&c)) throw MissingResource{"textlab: a recorder is already attached"};
  Recording rec;
  rec.prior = &provider(c);
  if (dynamic_cast<GdiTextMetrics *>(rec.prior)) {
    auto g = std::make_unique<GdiRecorder>();
    rec.log = &g->log;
    rec.rec = std::move(g);
  } else {
    auto w = std::make_unique<WrapRecorder>(rec.prior);
    rec.log = &w->log;
    rec.rec = std::move(w);
  }
  set_text_metrics(c, rec.rec.get());
  recorders()[&c] = std::move(rec);
  r.set_i32(0);
}
void a_record_stop(Ctx &c, const uint32_t *, Ret &r) {
  auto it = recorders().find(&c);
  if (it == recorders().end()) {
    r.set_i32(-1);
    return;
  }
  set_text_metrics(c, it->second.prior);
  const bool ok = c.files().write("$textlab/calls.bin", it->second.log->file());
  recorders().erase(it);
  r.set_i32(ok ? 0 : -2);
}

const FuncDef kFuncs[] = {
    {"textlab.draw", "default", "int", 9, 0, a_draw, &g_negctl_textlab},
    {"textlab.batch", "default", "int", 4, 0, a_batch, &g_negctl_textlab},
    {"textlab.width", "default", "int", 3, 0, a_width, &g_negctl_textlab},
    {"textlab.ascent", "default", "int", 1, 0, a_ascent, &g_negctl_textlab},
    {"textlab.recordStart", "default", "int", 0, 0, a_record_start, &g_negctl_textlab},
    {"textlab.recordStop", "default", "int", 0, 0, a_record_stop, &g_negctl_textlab},
};

}  // namespace

bool textlab_draw(Ctx &ctx, uint32_t fontref, uint32_t text, int32_t n, int32_t x, int32_t y, uint32_t colorref,
                  uint32_t surf, int32_t w, int32_t h) {
  TextMetrics &tm = provider(ctx);
  uint16_t *px = surface_at(ctx, surf, w, h);
  return draw_px(ctx, tm, fontref, bytes_at(ctx, text, n), text, x, y, colorref, px, w, h);
}

extern const ModuleDef kModule_textlab = {"textlab", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
