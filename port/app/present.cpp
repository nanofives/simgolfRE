// Presenting the RGB555 back buffer (present.h).
#include "present.h"

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>  // P6B item 2 (std::min / max in paint)
#include <cmath>    // P10A: lround (the overlay)
#include <cstring>  // P10A: memcpy (the overlay)
#include <string>

#include "portsim/sim/perf.h"  // P10A: perf_now_ns
#include "profile.h"           // P10A

namespace portapp {

Viewport fit_viewport(int32_t cw, int32_t ch) {
  Viewport v;
  if (cw <= 0 || ch <= 0) {
    v.client_w = kLogicalW;
    v.client_h = kLogicalH;
    return v;
  }
  if (static_cast<int64_t>(cw) * kLogicalH >= static_cast<int64_t>(ch) * kLogicalW) {  // wider than 4:3: bars left / right
    v.client_h = ch;
    v.client_w = static_cast<int32_t>(static_cast<int64_t>(ch) * kLogicalW / kLogicalH);
    v.origin_x = (cw - v.client_w) / 2;
  } else {  // taller: bars top / bottom
    v.client_w = cw;
    v.client_h = static_cast<int32_t>(static_cast<int64_t>(cw) * kLogicalH / kLogicalW);
    v.origin_y = (ch - v.client_h) / 2;
  }
  return v;
}

bool Presenter::set_frame_world(const std::vector<uint16_t> &px, BandFill band, WorldImage &&world) {
  const bool stale = defect_ == PresentDefect::Stale && !frame_.empty();
  const bool ok = set_frame(px, band);
  if (ok && !stale) {
    const bool sane = !world.empty() && world.pw > 0 && world.ph > 0 &&
                      world.px.size() == static_cast<size_t>(world.pw) * static_cast<size_t>(world.ph) &&
                      world.mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH && world.fw > 0;
    world_ = sane ? std::move(world) : WorldImage{};
  }
  return ok;
}

// ---- G32: PanelRemap
void PanelRemap::set(bool on, const Viewport &vp, int32_t cw, const std::vector<uint8_t> &mask) {
  std::lock_guard<std::mutex> lk(mu_);
  layered_ = false;  // P9B
  on_ = on && vp.origin_x > 0 && mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
  vp_ = vp;
  cw_ = cw;
  if (on_) mask_ = mask;
}
void PanelRemap::clear() {
  std::lock_guard<std::mutex> lk(mu_);
  on_ = false;
}
void PanelRemap::set_cursor_answer(int32_t dx, int32_t lx, int32_t ly) const {  // P9E fix 4
  std::lock_guard<std::mutex> lk(mu_);
  cursor_dx_ = dx;
  cursor_lx_ = lx;
  cursor_ly_ = ly;
}
int32_t PanelRemap::cursor_dx() const {
  std::lock_guard<std::mutex> lk(mu_);
  return on_ && layered_ ? cursor_dx_ : 0;
}
uint64_t PanelRemap::remapped() const {
  std::lock_guard<std::mutex> lk(mu_);
  return remapped_;
}
void PanelRemap::set_layers(bool on, const Viewport &vp, int32_t cw, std::vector<RemapLayer> &&layers) {
  std::lock_guard<std::mutex> lk(mu_);
  on_ = on && vp.origin_x > 0;
  vp_ = vp;
  cw_ = cw;
  layered_ = true;
  layers_ = std::move(layers);
}
void PanelRemap::map(int32_t &x, int32_t &y) const {
  bool v = false;
  map(x, y, v);
}
void PanelRemap::map(int32_t &x, int32_t &y, bool &vacated) const {
  vacated = false;
  std::lock_guard<std::mutex> lk(mu_);
  if (!on_ || vp_.client_w <= 0 || vp_.client_h <= 0) return;
  const int32_t dxl = -vp_.origin_x, dxr = cw_ - vp_.origin_x - vp_.client_w;
  if (layered_) {  // P9B: the topmost layer with a pixel under the point decides
    if (y < vp_.origin_y || y >= vp_.origin_y + vp_.client_h) return;
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp_.origin_y) * kLogicalH / vp_.client_h);
    for (size_t k = layers_.size(); k-- > 0;) {
      const RemapLayer &l = layers_[k];
      const int32_t dx = l.fixed ? l.dx : l.anchor < 0 ? dxl : l.anchor > 0 ? dxr : 0;  // P9E: fixed
      const int32_t sx = x - dx;
      if (sx < vp_.origin_x || sx >= vp_.origin_x + vp_.client_w) continue;
      const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(sx - vp_.origin_x) * kLogicalW / vp_.client_w);
      if (lx < l.x0 || lx >= l.x1 || ly < l.y0 || ly >= l.y1) continue;
      if (!l.cover[static_cast<size_t>(ly - l.y0) * static_cast<size_t>(l.x1 - l.x0) + static_cast<size_t>(lx - l.x0)]) continue;
      if (dx != 0) {
        x = sx;
        ++remapped_;
        return;
      }
      break;  // a centred layer's pixel: the point stays (P9E fix 3: and may still be a vacated place, below)
    }
    // P9E fix 3: no moved layer under the point; a moved panel layer's box holding its 800-space point makes it a vacated place
    // (a centred pixel there, a golfer's label, was over the panel in the 800-space frame and is over the course now)
    if (x >= vp_.origin_x && x < vp_.origin_x + vp_.client_w) {
      const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp_.origin_x) * kLogicalW / vp_.client_w);
      for (const RemapLayer &l : layers_) {
        const int32_t dx = l.fixed ? 0 : l.anchor < 0 ? dxl : l.anchor > 0 ? dxr : 0;
        if (dx != 0 && lx >= l.x0 && lx < l.x1 && ly >= l.y0 && ly < l.y1) {
          vacated = true;
          break;
        }
      }
    }
    return;
  }
  for (const HudPanel &p : kHudPanels) {
    const int32_t sx = x - (p.anchor < 0 ? dxl : dxr);
    if (sx < vp_.origin_x || sx >= vp_.origin_x + vp_.client_w || y < vp_.origin_y || y >= vp_.origin_y + vp_.client_h) continue;
    const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(sx - vp_.origin_x) * kLogicalW / vp_.client_w);
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp_.origin_y) * kLogicalH / vp_.client_h);
    if (lx < p.x0 || lx >= p.x1 || ly < p.y0 || ly >= p.y1) continue;
    if (mask_[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)]) continue;  // the world shows there
    x = sx;
    ++remapped_;
    return;
  }
  // P9E fix 3: not remapped, and its 800-space point inside a moved panel rectangle: a vacated place
  if (x >= vp_.origin_x && x < vp_.origin_x + vp_.client_w && y >= vp_.origin_y && y < vp_.origin_y + vp_.client_h) {
    const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp_.origin_x) * kLogicalW / vp_.client_w);
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp_.origin_y) * kLogicalH / vp_.client_h);
    for (const HudPanel &p : kHudPanels)
      if (lx >= p.x0 && lx < p.x1 && ly >= p.y0 && ly < p.y1) vacated = true;
  }
}

bool Presenter::set_frame(const std::vector<uint16_t> &px, BandFill band) {
  if (px.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return false;
  ++frames_set_;
  if (defect_ == PresentDefect::Stale && !frame_.empty()) return true;
  frame_ = px;
  band_ = band;
  world_ = WorldImage{};  // P8B: a frame set without its world layer has none
  if (defect_ == PresentDefect::ByteSwap)
    for (uint16_t &v : frame_) v = static_cast<uint16_t>((v >> 8) | (v << 8));
  return true;
}

ScaleMode Presenter::effective_scale(int32_t w, int32_t h) const {
  if (scale_ != ScaleMode::Auto) return scale_;
  const bool integer = w > 0 && w % kLogicalW == 0 && h == w / kLogicalW * kLogicalH;
  return integer ? ScaleMode::Nearest : ScaleMode::Smooth;
}

void Presenter::render(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vpo) const {
  const ProfScope prof(Phase::Base);  // P10A
  const Viewport vp = fit_viewport(cw, ch);
  if (vpo) *vpo = vp;
  out.assign(static_cast<size_t>(cw > 0 ? cw : 0) * static_cast<size_t>(ch > 0 ? ch : 0), 0xff000000u);
  if (frame_.empty() || cw <= 0 || ch <= 0) return;
  // P9B: with the frame's HUD layers the scaled image is the layers' base (the root before the first panel drew); the layers
  // are composed over it in compose_layers
  // P9C: with the frame's text runs, the picture without their low-resolution glyphs (the runs are drawn after the scaling)
  const bool text = text_now(vp);
  const std::vector<uint16_t> &fsrc = layered() ? hud_for(text).base
                                      : text && text_defect_ != TextDefect::Unremoved ? text_.clean : frame_;
  if (effective_scale(vp.client_w, vp.client_h) == ScaleMode::Smooth && vp.client_w > 0 && vp.client_h > 0) {
    // P5B item 4: bilinear. The frame converted to BGRA once, then per client pixel four neighbours with 8-bit weights.
    std::vector<uint32_t> src(fsrc.size());
    for (size_t i = 0; i < fsrc.size(); ++i) src[i] = rgb555_to_bgra(fsrc[i]);
    struct Tap {
      int32_t i0, i1;
      uint32_t f;  // weight of i1, 0..256
    };
    auto taps = [](int32_t n_out, int32_t n_in) {
      std::vector<Tap> t(static_cast<size_t>(n_out));
      for (int32_t k = 0; k < n_out; ++k) {
        // source position of the pixel centre, in 1/256: ((k + 0.5) * n_in / n_out - 0.5) * 256
        int64_t p = ((2 * static_cast<int64_t>(k) + 1) * n_in * 128) / n_out - 128;
        if (p < 0) p = 0;
        const int64_t last = static_cast<int64_t>(n_in - 1) * 256;
        if (p > last) p = last;
        Tap &a = t[static_cast<size_t>(k)];
        a.i0 = static_cast<int32_t>(p >> 8);
        a.f = static_cast<uint32_t>(p & 255);
        a.i1 = a.i0 + 1 < n_in ? a.i0 + 1 : a.i0;
      }
      return t;
    };
    const std::vector<Tap> tx = taps(vp.client_w, kLogicalW), ty = taps(vp.client_h, kLogicalH);
    auto mix = [](uint32_t a, uint32_t b, uint32_t f) {  // per channel a + (b - a) * f / 256
      const uint32_t g = 256 - f;
      const uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
      const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
      return rb | (ag << 8);
    };
    for (int32_t y = 0; y < vp.client_h; ++y) {
      const Tap &v = ty[static_cast<size_t>(y)];
      const uint32_t *s0 = src.data() + static_cast<size_t>(v.i0) * kLogicalW;
      const uint32_t *s1 = src.data() + static_cast<size_t>(v.i1) * kLogicalW;
      uint32_t *dst = out.data() + static_cast<size_t>(y + vp.origin_y) * static_cast<size_t>(cw) + vp.origin_x;
      for (int32_t x = 0; x < vp.client_w; ++x) {
        const Tap &u = tx[static_cast<size_t>(x)];
        const uint32_t a = mix(s0[u.i0], s0[u.i1], u.f), b = mix(s1[u.i0], s1[u.i1], u.f);
        dst[x] = mix(a, b, v.f) | 0xff000000u;
      }
    }
    if (!world_.empty()) compose_world(out, cw, ch, vp);  // P8B
    if (text && !layered()) draw_runs(out, cw, ch, vp, false, 0, 0);  // P9C (the layered path draws them in compose_layers)
    if (world_.empty() && wide_ != WideFill::Off && band_ == BandFill::Extend) fill_bands(out, cw, ch, vp);  // P7
    return;
  }
  std::vector<int32_t> col(static_cast<size_t>(vp.client_w));
  for (int32_t x = 0; x < vp.client_w; ++x)
    col[static_cast<size_t>(x)] = static_cast<int32_t>(static_cast<int64_t>(x) * kLogicalW / vp.client_w);
  for (int32_t y = 0; y < vp.client_h; ++y) {
    const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y) * kLogicalH / vp.client_h);
    const uint16_t *src = fsrc.data() + static_cast<size_t>(ly) * kLogicalW;
    uint32_t *dst = out.data() + static_cast<size_t>(y + vp.origin_y) * static_cast<size_t>(cw) + vp.origin_x;
    for (int32_t x = 0; x < vp.client_w; ++x) dst[x] = rgb555_to_bgra(src[col[static_cast<size_t>(x)]]);
  }
  if (!world_.empty()) compose_world(out, cw, ch, vp);  // P8B
  if (text && !layered()) draw_runs(out, cw, ch, vp, false, 0, 0);  // P9C (the layered path draws them in compose_layers)
  if (world_.empty() && wide_ != WideFill::Off && band_ == BandFill::Extend) fill_bands(out, cw, ch, vp);  // P7
}

// ---- P9C: the text runs (textres.h)
void Presenter::set_text(portsim::GfxTextFrame &&tf) {
  text_ok_ = text_layered_ok_ = false;
  text_ = portsim::GfxTextFrame{};
  hud_text_ = portsim::GfxHudFrame{};
  if (!text_native_ || tf.empty() || tf.back.size() != frame_.size() || tf.clean.size() != frame_.size()) return;
  if (defect_ == PresentDefect::Stale && frames_set_ > 1) return;
  text_ = std::move(tf);
  text_ok_ = true;
  // the layered path: the base and the layers without the runs' glyphs, when they belong to the frame's HUD layers
  if (!world_.empty() && !world_.hud.empty() && text_.hud_serial == world_.hud.serial &&
      text_.base_clean.size() == world_.hud.base.size()) {
    text_layered_ok_ = true;
    hud_text_ = world_.hud;  // the layers without the runs' glyphs (world_.hud stays: the picture at 800 x 600)
    if (text_defect_ != TextDefect::Unremoved) {
      hud_text_.base = text_.base_clean;
      for (const portsim::GfxTextLayerPatch &p : text_.patches) {
        if (p.layer < 0 || static_cast<size_t>(p.layer) >= hud_text_.layers.size()) continue;
        std::vector<portsim::GfxLayerPx> &lp = hud_text_.layers[static_cast<size_t>(p.layer)].px;
        for (size_t k = 0; k < p.idx.size() && k < p.px.size(); ++k)
          if (p.idx[k] < lp.size()) lp[p.idx[k]] = p.px[k];
      }
    }
    text_.base_clean.clear();
    text_.patches.clear();
  }
  text_.back.clear();  // checked; not needed any more
}

const portsim::GfxHudFrame &Presenter::hud_for(bool text) const {
  return text && text_layered_ok_ ? hud_text_ : world_.hud;
}

bool Presenter::text_now(const Viewport &vp) const {
  if (!text_ok_ || vp.client_w <= 0 || vp.client_h <= 0) return false;
  if (layered() && !text_layered_ok_) return false;
  const bool unit = vp.client_w == kLogicalW && vp.client_h == kLogicalH;
  return !unit || text_unit_redraw_;
}

// P9E: the popup piece that owns 800-space pixel (x, y) (GfxHudFrame::owner), -1 for none
int32_t Presenter::piece_of(const portsim::GfxHudFrame &f, int32_t x, int32_t y) {
  if (x < 0 || y < 0 || x >= kLogicalW || y >= kLogicalH || f.owner.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return -1;
  const uint8_t o = f.owner[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  return o != 0 && o <= f.pieces.size() ? static_cast<int32_t>(o) - 1 : -1;
}

// the runs of one layer (layered: -1 the base, k the layer; flat: every run) at dx client pixels
void Presenter::draw_runs(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp, bool layered_path, int32_t layer,
                          int32_t dx) const {
  const ProfScope prof(Phase::Text);  // P10A
  if (!raster_) raster_.reset(new TextRaster());
  raster_->set_defect(text_defect_);
  raster_->set_aa(text_aa_);
  raster_->set_fit(text_fit_);  // P10B fix 2
  // G32's rectangle compositor (no layers): a run inside a moved panel rectangle moves with it
  const bool rect = !layered_path && !world_.empty() && hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved &&
                    !world_.hud_none;  // P9D
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  for (const portsim::GfxTextRun &r : text_.runs) {
    if (layered_path && r.layer != layer) continue;
    if (r.target == 2 && world_.empty()) continue;  // a world run with no world under it
    int32_t d = dx;
    // P9E: a run of a layer with popup pixels moves with the popup that owns its box's centre
    if (layered_path && layer >= 0 && piece_frame_ && static_cast<size_t>(layer) < piece_frame_->layers.size() &&
        piece_frame_->layers[static_cast<size_t>(layer)].popups) {
      const int32_t k = piece_of(*piece_frame_, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2);
      if (k >= 0 && static_cast<size_t>(k) < piece_dx_.size() && piece_dx_[static_cast<size_t>(k)] != 0)
        d = piece_dx_[static_cast<size_t>(k)];
    }
    if (rect)
      for (const HudPanel &p : kHudPanels)
        if (r.x0 >= p.x0 && r.x1 <= p.x1 && r.y0 >= p.y0 && r.y1 <= p.y1) {
          d = p.anchor < 0 ? dxl : dxr;
          break;
        }
    raster_->draw(r, layered_path, vp, d, out, cw, ch);
  }
}

// P8B: the world layer under the frame. Client pixel (x, y) maps to world (ox + (x - vp.origin_x) * fw / vp.client_w,
// (y - vp.origin_y) * ph / vp.client_h): the identity when the world was made at this client size. Inside the content rectangle
// the frame's pixel stays where the mask of its logical pixel (floor((x - origin) * 800 / client_w), likewise y) is 0. The world
// is sampled at the nearest pixel when the mapping is 1:1 or the scale mode resolves to Nearest, else bilinear (8-bit weights).
void Presenter::compose_world(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  const ProfScope prof(Phase::World);  // P10A
  const WorldImage &w = world_;
  if (vp.client_w <= 0 || vp.client_h <= 0 || cw <= 0 || ch <= 0) return;
  // G32: the HUD compositor. The frame's image as drawn (frame_img), the mask with every moved panel's 2D pixels vacated (they show
  // the world), then each panel's 2D pixels pasted at their anchored place
  const bool layers = layered();  // P9B: the panels are layers composed after the world (compose_layers), not rectangles
  const bool anchor = !layers && hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved && !w.hud_none;  // P9D: hud_none
  // P9C: with the runs drawn, the mask taken over the picture without their glyphs (a label over the course shows the world)
  const bool wtext = text_now(vp) && w.mask_text.size() == w.mask.size() && w.px_text.size() == w.px.size();
  const std::vector<uint8_t> &wmask = wtext ? w.mask_text : w.mask;
  const std::vector<uint16_t> &wpx = wtext ? w.px_text : w.px;
  if (remap_ && !layers) remap_->set(anchor, vp, cw, w.mask);
  std::vector<uint32_t> frame_img;
  std::vector<uint8_t> mask_moved;
  const uint8_t *maskp = wmask.data();
  if (anchor) {
    frame_img = out;
    mask_moved = wmask;
    for (const HudPanel &p : kHudPanels)
      for (int32_t y = p.y0; y < p.y1; ++y)
        for (int32_t x = p.x0; x < p.x1; ++x) mask_moved[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)] = 1;
    maskp = mask_moved.data();
  }
  const bool identity = w.fw == vp.client_w && w.ph == vp.client_h;
  const bool nearest = identity || effective_scale(vp.client_w, vp.client_h) == ScaleMode::Nearest;
  static const std::vector<uint32_t> pal = [] {
    std::vector<uint32_t> t(32768);
    for (uint32_t v = 0; v < 32768; ++v) t[v] = rgb555_to_bgra(static_cast<uint16_t>(v));
    return t;
  }();
  // the world position of each client column / row: nearest = floor((k - origin) * n_world / n_client) + world origin (exact);
  // bilinear = the client pixel centre in 1/256 world pixels, ((k - origin + 0.5) * n_world / n_client - 0.5) * 256
  auto pos = [nearest](int32_t k, int32_t origin, int32_t n_client, int32_t n_world, int32_t world_origin) -> int64_t {
    const int64_t d = static_cast<int64_t>(k) - origin;
    if (nearest) {
      const int64_t num = d * n_world;
      const int64_t q = num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client);
      return (q + world_origin) * 256;
    }
    const int64_t num = (2 * d + 1) * n_world * 128;
    const int64_t q = num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client);
    return q - 128 + static_cast<int64_t>(world_origin) * 256;
  };
  std::vector<int64_t> wx(static_cast<size_t>(cw)), wy(static_cast<size_t>(ch));
  for (int32_t x = 0; x < cw; ++x) wx[static_cast<size_t>(x)] = pos(x, vp.origin_x, vp.client_w, w.fw, w.ox);
  for (int32_t y = 0; y < ch; ++y) wy[static_cast<size_t>(y)] = pos(y, vp.origin_y, vp.client_h, w.ph, 0);
  auto mix = [](uint32_t a, uint32_t b, uint32_t f) {
    const uint32_t g = 256 - f;
    const uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
    const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * f) >> 8) & 0x00ff00ffu;
    return rb | (ag << 8);
  };
  auto at = [&](int64_t xx, int64_t yy) -> uint32_t {
    if (xx < 0) xx = 0;
    if (yy < 0) yy = 0;
    if (xx >= w.pw) xx = w.pw - 1;
    if (yy >= w.ph) yy = w.ph - 1;
    return pal[wpx[static_cast<size_t>(yy) * static_cast<size_t>(w.pw) + static_cast<size_t>(xx)] & 0x7fff];
  };
  for (int32_t y = 0; y < ch; ++y) {
    const int64_t sy = wy[static_cast<size_t>(y)];
    const int64_t y0 = sy >> 8;
    if (y0 < -2 || y0 >= w.ph + 2) continue;  // rows outside the world stay as they are (within 2: the edge row, `at`)
    const bool row_in = y >= vp.origin_y && y < vp.origin_y + vp.client_h;
    const int32_t ly = row_in ? static_cast<int32_t>(static_cast<int64_t>(y - vp.origin_y) * kLogicalH / vp.client_h) : 0;
    uint32_t *dst = out.data() + static_cast<size_t>(y) * static_cast<size_t>(cw);
    for (int32_t x = 0; x < cw; ++x) {
      if (row_in && x >= vp.origin_x && x < vp.origin_x + vp.client_w) {
        const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp.origin_x) * kLogicalW / vp.client_w);
        if (defect_ != PresentDefect::WorldMaskIgnored && !maskp[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)])
          continue;  // the frame's own pixel
      }
      const int64_t sx = wx[static_cast<size_t>(x)];
      const int64_t x0 = sx >> 8;
      if (x0 < -2 || x0 >= w.pw + 2) continue;  // columns outside the world stay as they are (within 2: the edge column, `at`)
      if (nearest) {
        dst[x] = at(x0, y0);
      } else {
        const uint32_t fx = static_cast<uint32_t>(sx & 255), fy = static_cast<uint32_t>(sy & 255);
        const uint32_t a = mix(at(x0, y0), at(x0 + 1, y0), fx), b = mix(at(x0, y0 + 1), at(x0 + 1, y0 + 1), fx);
        dst[x] = mix(a, b, fy) | 0xff000000u;
      }
    }
  }
  if (layers) {
    compose_layers(out, cw, ch, vp);
    return;
  }
  if (!anchor) return;
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  for (const HudPanel &p : kHudPanels) {
    const int32_t dx = p.anchor < 0 ? dxl : dxr;
    const int32_t cx0 = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x0) * vp.client_w / kLogicalW);
    const int32_t cx1 = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x1) * vp.client_w / kLogicalW);
    const int32_t cy0 = vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(p.y0) * vp.client_h / kLogicalH);
    const int32_t cy1 = vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(p.y1) * vp.client_h / kLogicalH);
    for (int32_t y = std::max(cy0, 0); y < std::min(cy1, ch); ++y) {
      const int32_t ly = static_cast<int32_t>(static_cast<int64_t>(y - vp.origin_y) * kLogicalH / vp.client_h);
      for (int32_t x = std::max(cx0, 0); x < std::min(cx1, cw); ++x) {
        const int32_t lx = static_cast<int32_t>(static_cast<int64_t>(x - vp.origin_x) * kLogicalW / vp.client_w);
        if (lx < p.x0 || lx >= p.x1 || ly < p.y0 || ly >= p.y1) continue;
        if (wmask[static_cast<size_t>(ly) * kLogicalW + static_cast<size_t>(lx)]) continue;  // not a 2D pixel of the frame
        const int32_t tx = x + dx;
        if (tx < 0 || tx >= cw) continue;
        out[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(tx)] =
            frame_img[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(x)];
      }
    }
  }
}

// ---- P9B: the layered compositor
bool Presenter::layered() const {
  return defect_ != PresentDefect::LayerRectMove && !world_.empty() && !world_.hud.empty() &&
         world_.hud.base.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
}

// Every layer of the frame in draw order over what is already composed (the base and the world). A layer moved to an edge is
// drawn at its anchor: left = -origin_x client pixels, right = + the right band's width, centred = 0, at the frame's scale (window
// height / 600). Client pixel (x, y) of a layer at offset dx shows the layer's 800-space pixel under (x - dx, y): nearest takes the
// one pixel, smooth the four around the client pixel's centre with bilinear weights, each of them composed over the client pixel
// already there (d): not drawn = d, opaque = its word, mask blend = jgld blitPair1's channel rule over d's 5-bit channels with the
// layer's source and weight, shade = the shade table at d's RGB555 word. The weighted sum of the four gives smooth panel edges with
// no world pixel chosen inside a panel by a mask, and a blended pixel (frame borders, half-bright strips) shows what is under it at
// the panel's new place.
void Presenter::compose_layers(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  const ProfScope prof(Phase::Layers);  // P10A
  const bool text = text_now(vp);  // P9C: the layers without the runs' glyphs when the runs are drawn
  const portsim::GfxHudFrame &f = hud_for(text);
  const bool anchor = hud_anchor_ && vp.origin_x > 0 && defect_ != PresentDefect::HudUnmoved;
  const int32_t dxl = -vp.origin_x, dxr = cw - vp.origin_x - vp.client_w;
  // P9E: each popup piece's client offset: its opener panel's anchor offset, clamped so the piece's box stays inside the window
  piece_dx_.assign(f.pieces.size(), 0);
  piece_frame_ = nullptr;
  const bool pieces = anchor && popups_on_ && !f.pieces.empty() && f.owner.size() == static_cast<size_t>(kLogicalW) * kLogicalH;
  if (pieces) {
    piece_frame_ = &f;
    for (size_t k = 0; k < f.pieces.size(); ++k) {
      const portsim::GfxHudPiece &p = f.pieces[k];
      const int8_t a = portsim::gfx_layer_anchor(p.opener);
      if (a == 0 || p.x1 <= p.x0) continue;
      int32_t d = a < 0 ? dxl : dxr;
      const int32_t left = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x0) * vp.client_w / kLogicalW);
      const int32_t right = vp.origin_x + static_cast<int32_t>(static_cast<int64_t>(p.x1) * vp.client_w / kLogicalW);
      if (right + d > cw) d = cw - right;
      if (left + d < 0) d = -left;
      piece_dx_[k] = d;
    }
  }
  // the moved piece (index) of a layer pixel, -1 when it stays at the layer's own place
  auto moved_piece = [&](const portsim::GfxHudLayer &l, int32_t x, int32_t y) -> int32_t {
    if (!pieces || !l.popups) return -1;
    const int32_t k = piece_of(f, x, y);
    return k >= 0 && piece_dx_[static_cast<size_t>(k)] != 0 ? k : -1;
  };
  if (remap_) {
    std::vector<RemapLayer> rl;
    rl.reserve(f.layers.size());
    for (const portsim::GfxHudLayer &l : f.layers) {
      RemapLayer r;
      r.anchor = anchor ? l.anchor : 0;
      r.x0 = l.x0;
      r.y0 = l.y0;
      r.x1 = l.x1;
      r.y1 = l.y1;
      r.cover.resize(l.px.size());
      for (size_t i = 0; i < l.px.size(); ++i)
        r.cover[i] = l.px[i].kind || l.px[i].k == portsim::kGfxTextCoverMark ? 1 : 0;  // P9C: a removed glyph is still the layer's
      // P9E: each moved popup piece's pixels are a layer of their own at the piece's offset, above the rest of this layer
      std::vector<RemapLayer> moved;
      if (pieces && l.popups) {
        std::vector<int32_t> slot(f.pieces.size(), -1);
        for (size_t k = 0; k < f.pieces.size(); ++k) {
          if (piece_dx_[k] == 0) continue;
          slot[k] = static_cast<int32_t>(moved.size());
          RemapLayer m;
          m.fixed = true;
          m.dx = piece_dx_[k];
          m.x0 = r.x0;
          m.y0 = r.y0;
          m.x1 = r.x1;
          m.y1 = r.y1;
          m.cover.assign(r.cover.size(), 0);
          moved.push_back(std::move(m));
        }
        const int32_t bw = l.x1 - l.x0;
        for (int32_t y = l.y0; y < l.y1; ++y)
          for (int32_t x = l.x0; x < l.x1; ++x) {
            const size_t i = static_cast<size_t>(y - l.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - l.x0);
            if (!r.cover[i]) continue;
            const int32_t k = moved_piece(l, x, y);
            if (k < 0) continue;
            moved[static_cast<size_t>(slot[static_cast<size_t>(k)])].cover[i] = 1;
            r.cover[i] = 0;
          }
      }
      rl.push_back(std::move(r));
      for (RemapLayer &m : moved) rl.push_back(std::move(m));
    }
    remap_->set_layers(anchor, vp, cw, std::move(rl));
  }
  const bool nearest = effective_scale(vp.client_w, vp.client_h) == ScaleMode::Nearest;
  const bool opaque_blends = defect_ == PresentDefect::LayerBlendOpaque;
  // P9E fix 4: the tool previews at the cursor (WorldImage::cur_*): drawn at the offset of the layer the last cursor answer was
  // moved by, in the frame's draw order where its copy of them is (the first centred layer holding one of their pixels, else
  // after every layer); that copy is left out of the centred layers (the world under it shows)
  const WorldImage &W = world_;
  const bool cur = W.cur_mask.size() == static_cast<size_t>(kLogicalW) * kLogicalH && W.cur_px.size() == W.px.size() &&
                   W.cur_cov.size() == W.px.size() && W.cur_box[2] > W.cur_box[0];
  const bool c2 = f.cursor2d.size() == static_cast<size_t>(kLogicalW) * kLogicalH;  // the frame's own 2D preview pixels
  const int32_t cdx = (cur || c2) && anchor && remap_ ? remap_->cursor_dx() : 0;
  auto c2_at = [&](int32_t x, int32_t y) {
    return c2 && cdx != 0 && x >= 0 && y >= 0 && x < kLogicalW && y < kLogicalH &&
           f.cursor2d[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  };
  auto cur_at = [&](int32_t x, int32_t y) {
    return cur && x >= 0 && y >= 0 && x < kLogicalW && y < kLogicalH && W.cur_mask[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  };
  size_t cur_layer = f.layers.size();
  for (size_t li = 0; cur && li < f.layers.size() && cur_layer == f.layers.size(); ++li) {
    const portsim::GfxHudLayer &l = f.layers[li];
    if (l.id != portsim::kGfxLayerCentred) continue;
    for (int32_t y = l.y0; y < l.y1 && cur_layer == f.layers.size(); ++y)
      for (int32_t x = l.x0; x < l.x1; ++x)
        if (cur_at(x, y) && l.at(x, y)) {
          cur_layer = li;
          break;
        }
  }
  auto draw_cursor = [&]() {  // nearest world pixels, the plane's box only
    if (!cur || vp.client_w <= 0 || vp.client_h <= 0 || W.fw <= 0 || W.ph <= 0) return;
    const int32_t cx0 = std::max<int32_t>(0, vp.origin_x + cdx + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[0] - W.ox) * vp.client_w / W.fw) - 1);
    const int32_t cx1 = std::min<int32_t>(cw, vp.origin_x + cdx + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[2] - W.ox) * vp.client_w / W.fw) + 2);
    const int32_t cy0 = std::max<int32_t>(0, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[1]) * vp.client_h / W.ph) - 1);
    const int32_t cy1 = std::min<int32_t>(ch, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(W.cur_box[3]) * vp.client_h / W.ph) + 2);
    for (int32_t y = cy0; y < cy1; ++y) {
      const int64_t dy = static_cast<int64_t>(y - vp.origin_y) * W.ph;
      const int32_t wy = static_cast<int32_t>(dy >= 0 ? dy / vp.client_h : -((-dy + vp.client_h - 1) / vp.client_h));
      if (wy < 0 || wy >= W.ph) continue;
      uint32_t *dst = out.data() + static_cast<size_t>(y) * static_cast<size_t>(cw);
      for (int32_t x = cx0; x < cx1; ++x) {
        const int64_t dxw = static_cast<int64_t>(x - cdx - vp.origin_x) * W.fw;
        const int32_t wx = W.ox + static_cast<int32_t>(dxw >= 0 ? dxw / vp.client_w : -((-dxw + vp.client_w - 1) / vp.client_w));
        if (wx < 0 || wx >= W.pw) continue;
        const size_t j = static_cast<size_t>(wy) * static_cast<size_t>(W.pw) + static_cast<size_t>(wx);
        if (W.cur_cov[j]) dst[x] = rgb555_to_bgra(W.cur_px[j]);
      }
    }
  };
  auto to555 = [](uint32_t c) -> uint16_t {
    return static_cast<uint16_t>((((c >> 19) & 31) << 10) | (((c >> 11) & 31) << 5) | ((c >> 3) & 31));
  };
  auto over = [&](const portsim::GfxLayerPx &p, uint32_t d) -> uint32_t {  // one layer pixel composed over client pixel d
    if (p.kind == 1 || opaque_blends) return rgb555_to_bgra(p.px);
    const uint16_t w = to555(d);
    if (p.kind == 2) {  // jgld blitPair1 (gfx.cpp b_stretch_blit): ((dst8 * k) >> 8) + src8, >> 3, per channel, words as the original
      const uint32_t k = p.k, sw = p.src;
      auto chn = [&](uint32_t dv, uint32_t sv) { return static_cast<uint16_t>(static_cast<uint16_t>(((dv * k) >> 8) + sv) >> 3); };
      const uint16_t rr = static_cast<uint16_t>(chn((w >> 7) & 0xf8u, (sw >> 7) & 0xf8u) << 10);
      const uint16_t gg = static_cast<uint16_t>(chn((w >> 2) & 0xf8u, (sw >> 2) & 0xf8u) << 5);
      const uint16_t bb = chn((static_cast<uint32_t>(w) << 3) & 0xf8u, (static_cast<uint32_t>(sw) << 3) & 0xf8u);
      return rgb555_to_bgra(static_cast<uint16_t>((rr | gg | bb) & 0x7fff));
    }
    if (p.kind == 4) return rgb555_to_bgra(static_cast<uint16_t>(portsim::gfx_layer_quarter(p.src, w) & 0x7fff));  // P9D: drawAlpha's quarter blend
    size_t n = 0;
    const uint16_t *t = portsim::gfx_shade_table(p.k, n);
    if (!t || w >= n) return rgb555_to_bgra(p.px);
    return rgb555_to_bgra(static_cast<uint16_t>(t[w] & 0x7fff));
  };
  auto mix = [](uint32_t a, uint32_t b, uint32_t fr) {
    const uint32_t g = 256 - fr;
    const uint32_t rb = (((a & 0x00ff00ffu) * g + (b & 0x00ff00ffu) * fr) >> 8) & 0x00ff00ffu;
    const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * g + ((b >> 8) & 0x00ff00ffu) * fr) >> 8) & 0x00ff00ffu;
    return rb | (ag << 8);
  };
  // the 800-space position of client column / row k: nearest = floor((k - origin) * n / client) * 256, smooth = the pixel centre
  // in 1/256, ((k - origin + 0.5) * n / client - 0.5) * 256 (may be negative near the frame's edge)
  auto lpos = [nearest](int32_t k, int32_t origin, int32_t n_client, int32_t n_log) -> int64_t {
    const int64_t d = static_cast<int64_t>(k) - origin;
    if (nearest) {
      const int64_t num = d * n_log;
      return (num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client)) * 256;
    }
    const int64_t num = (2 * d + 1) * n_log * 128;
    return (num >= 0 ? num / n_client : -((-num + n_client - 1) / n_client)) - 128;
  };
  auto floor256 = [](int64_t v) -> int32_t { return static_cast<int32_t>(v >= 0 ? v / 256 : -((-v + 255) / 256)); };
  if (text) {
    draw_runs(out, cw, ch, vp, true, -3, 0);  // the wide scene's world runs (beside the frame's columns)
    draw_runs(out, cw, ch, vp, true, -1, 0);  // the base's
  }
  for (size_t li = 0; li < f.layers.size(); ++li) {
    const portsim::GfxHudLayer &l = f.layers[li];
    if (li == cur_layer) draw_cursor();  // P9E fix 4
    const int32_t dx0 = anchor ? (l.anchor < 0 ? dxl : l.anchor > 0 ? dxr : 0) : 0;
    struct RunsAfter {  // P9C: the layer's runs once its pixels are composed (also when the layer has no box)
      const Presenter &p;
      std::vector<uint32_t> &out;
      int32_t cw, ch;
      const Viewport &vp;
      bool on;
      int32_t li, dx;
      ~RunsAfter() {
        if (on) p.draw_runs(out, cw, ch, vp, true, li, dx);
      }
    } runs_after{*this, out, cw, ch, vp, text, static_cast<int32_t>(li), dx0};
    // P9D: a whole-frame dim (the report screens' black quarter blend over every root pixel): everything composed so far is dimmed
    // in the whole window, as the original dims its whole frame (the panels at their anchors, the world, the side bands); the
    // layer's own dim pixels (kind 4) are that dim, already applied
    const bool wdim = l.dim != 0 && defect_ != PresentDefect::DimLocal && !opaque_blends;
    if (wdim)
      for (uint32_t &d : out) d = rgb555_to_bgra(static_cast<uint16_t>(portsim::gfx_layer_quarter(l.dim_src, to555(d)) & 0x7fff)) | 0xff000000u;
    if (l.x1 <= l.x0 || l.y1 <= l.y0) continue;
    // P9E: pass -1 = the layer's pixels at its own place (without its moved popup pieces), then each moved piece at its offset
    const bool split = pieces && l.popups;
    // P9E fix 4: a last pass (cpass) draws a centred layer's 2D preview pixels (f.cursor2d) at the cursor's offset
    const int32_t npass = split ? static_cast<int32_t>(f.pieces.size()) : 0;
    const bool cmove = c2 && cdx != 0 && l.id == portsim::kGfxLayerCentred;
    const int32_t cpass = cmove ? npass : -2;
    for (int32_t pass = -1; pass < npass + (cmove ? 1 : 0); ++pass) {
    if (pass >= 0 && pass != cpass && piece_dx_[static_cast<size_t>(pass)] == 0) continue;
    const int32_t dx = pass == cpass ? cdx : pass >= 0 ? piece_dx_[static_cast<size_t>(pass)] : dx0;
    auto tap = [&](int32_t x, int32_t y) -> const portsim::GfxLayerPx * {
      const portsim::GfxLayerPx *p = l.at(x, y);
      if (p && split && moved_piece(l, x, y) != (pass == cpass ? -1 : pass)) return nullptr;
      if (p && l.id == portsim::kGfxLayerCentred && cur_at(x, y)) return nullptr;  // P9E fix 4: drawn as the cursor's previews
      if (p && cmove && (c2_at(x, y) != (pass == cpass))) return nullptr;  // P9E fix 4: the 2D preview only in its own pass
      return p && wdim && p->kind == 4 ? nullptr : p;
    };
    // the client box that can show the layer (one logical pixel of margin for the smooth taps)
    const int32_t cx0 = std::max<int32_t>(0, vp.origin_x + dx + static_cast<int32_t>(static_cast<int64_t>(l.x0 - 1) * vp.client_w / kLogicalW) - 1);
    const int32_t cx1 = std::min<int32_t>(cw, vp.origin_x + dx + static_cast<int32_t>(static_cast<int64_t>(l.x1 + 1) * vp.client_w / kLogicalW) + 1);
    const int32_t cy0 = std::max<int32_t>(0, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(l.y0 - 1) * vp.client_h / kLogicalH) - 1);
    const int32_t cy1 = std::min<int32_t>(ch, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(l.y1 + 1) * vp.client_h / kLogicalH) + 1);
    for (int32_t y = cy0; y < cy1; ++y) {
      const int64_t sy = lpos(y, vp.origin_y, vp.client_h, kLogicalH);
      const int32_t ly0 = floor256(sy);
      const uint32_t fy = static_cast<uint32_t>(sy - static_cast<int64_t>(ly0) * 256);
      uint32_t *dst = out.data() + static_cast<size_t>(y) * static_cast<size_t>(cw);
      for (int32_t x = cx0; x < cx1; ++x) {
        const int64_t sx = lpos(x - dx, vp.origin_x, vp.client_w, kLogicalW);
        const int32_t lx0 = floor256(sx);
        if (nearest) {
          if (const portsim::GfxLayerPx *p = tap(lx0, ly0)) dst[x] = over(*p, dst[x]) | 0xff000000u;
          continue;
        }
        const uint32_t fx = static_cast<uint32_t>(sx - static_cast<int64_t>(lx0) * 256);
        const portsim::GfxLayerPx *p00 = tap(lx0, ly0), *p10 = tap(lx0 + 1, ly0), *p01 = tap(lx0, ly0 + 1),
                                  *p11 = tap(lx0 + 1, ly0 + 1);
        if (!p00 && !p10 && !p01 && !p11) continue;
        const uint32_t d = dst[x];
        const uint32_t c00 = p00 ? over(*p00, d) : d, c10 = p10 ? over(*p10, d) : d, c01 = p01 ? over(*p01, d) : d,
                       c11 = p11 ? over(*p11, d) : d;
        dst[x] = mix(mix(c00, c10, fx), mix(c01, c11, fx), fy) | 0xff000000u;
      }
    }
    }  // P9E: the pass loop
  }
  if (cur_layer == f.layers.size()) draw_cursor();  // P9E fix 4: after every layer
}

// P7 (present.h WideFill): the side bands from the content rectangle's own edge columns, in client pixels. The content is
// already drawn, so the bands follow the scaled image exactly (nearest or smooth) and the content's pixels are not touched.
void Presenter::fill_bands(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  const ProfScope prof(Phase::Bands);  // P10A
  const int32_t ox = vp.origin_x, oy = vp.origin_y, w = vp.client_w, h = vp.client_h;
  if (w <= 0 || h <= 0 || cw <= 0 || ch <= 0 || oy + h > ch || ox + w > cw) return;
  if (wide_ == WideFill::Edge || wide_ == WideFill::Mirror) {
    fill_bands_p9(out, cw, ch, vp);
    return;
  }
  const bool clamp = wide_ == WideFill::Clamp;
  // client columns averaged at each edge (the frame's outer 4 logical columns) and the largest blur radius (48 logical rows)
  const int32_t k = clamp ? 1 : std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(w) * 4 / kLogicalW));
  const int32_t rmax = clamp ? 0 : std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(h) * 48 / kLogicalH));
  std::vector<uint64_t> pre[3];
  for (auto &v : pre) v.assign(static_cast<size_t>(h) + 1, 0);
  for (int side = 0; side < 2; ++side) {
    const int32_t band = side == 0 ? ox : cw - (ox + w);
    if (band <= 0) continue;
    // per row: the mean of the k edge columns (channels scaled by 256 to keep the fraction), as prefix sums over the rows
    for (int32_t y = 0; y < h; ++y) {
      const uint32_t *row = out.data() + static_cast<size_t>(oy + y) * static_cast<size_t>(cw);
      uint32_t s[3] = {0, 0, 0};
      for (int32_t j = 0; j < k; ++j) {
        const uint32_t v = row[side == 0 ? ox + j : ox + w - 1 - j];
        s[0] += (v >> 16) & 255;
        s[1] += (v >> 8) & 255;
        s[2] += v & 255;
      }
      for (int c = 0; c < 3; ++c)
        pre[c][static_cast<size_t>(y) + 1] = pre[c][static_cast<size_t>(y)] + (static_cast<uint64_t>(s[c]) * 256 + k / 2) / k;
    }
    for (int32_t d = 1; d <= band; ++d) {
      const int32_t r = std::min(d / 2, rmax);
      const int32_t x = side == 0 ? ox - d : ox + w - 1 + d;
      for (int32_t y = 0; y < h; ++y) {
        const int32_t lo = std::max(0, y - r), hi = std::min(h - 1, y + r);
        const uint64_t n = static_cast<uint64_t>(hi - lo + 1) * 256;
        uint32_t c8[3];
        for (int c = 0; c < 3; ++c) {
          const uint64_t sum = pre[c][static_cast<size_t>(hi) + 1] - pre[c][static_cast<size_t>(lo)];
          c8[c] = static_cast<uint32_t>((sum + n / 2) / n);
        }
        out[static_cast<size_t>(oy + y) * static_cast<size_t>(cw) + x] = 0xff000000u | (c8[0] << 16) | (c8[1] << 8) | c8[2];
      }
    }
  }
}

// P9A item 5 (present.h WideFill::Edge / Mirror). Channels are 8-bit client colours (the content is already drawn and scaled).
void Presenter::fill_bands_p9(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const {
  (void)ch;  // fill_bands checked the content rectangle against the client
  const int32_t ox = vp.origin_x, oy = vp.origin_y, w = vp.client_w, h = vp.client_h;
  auto lg = [](int32_t logical, int32_t span, int32_t of) {  // logical px -> client px, at least 1
    return std::max<int32_t>(1, static_cast<int32_t>(static_cast<int64_t>(span) * logical / of));
  };
  auto at = [&](int32_t x, int32_t y) -> uint32_t & { return out[static_cast<size_t>(oy + y) * static_cast<size_t>(cw) + x]; };
  auto chan = [](uint32_t v, int c) { return static_cast<int32_t>((v >> (16 - 8 * c)) & 255); };
  auto pack = [](const int32_t *c) {
    return 0xff000000u | (static_cast<uint32_t>(c[0]) << 16) | (static_cast<uint32_t>(c[1]) << 8) | static_cast<uint32_t>(c[2]);
  };
  // a vertical box blur of a per-row signal (3 channels, scaled by 256): prefix sums, then the mean over [y - r, y + r]
  auto prefix = [&](const std::vector<int64_t> &sig, std::vector<int64_t> &pre) {
    pre.assign(static_cast<size_t>(h) * 3 + 3, 0);
    for (int32_t y = 0; y < h; ++y)
      for (int c = 0; c < 3; ++c)
        pre[(static_cast<size_t>(y) + 1) * 3 + c] = pre[static_cast<size_t>(y) * 3 + c] + sig[static_cast<size_t>(y) * 3 + c];
  };
  auto mean = [&](const std::vector<int64_t> &pre, int32_t y, int32_t r, int c) {
    const int32_t lo = std::max(0, y - r), hi = std::min(h - 1, y + r);
    const int64_t n = static_cast<int64_t>(hi - lo + 1) * 256;
    return static_cast<int32_t>((pre[(static_cast<size_t>(hi) + 1) * 3 + c] - pre[static_cast<size_t>(lo) * 3 + c] + n / 2) / n);
  };
  const int32_t k_edge = lg(4, w, kLogicalW), k_bg = lg(64, w, kLogicalW), fade = lg(24, w, kLogicalW);
  const int32_t r_edge_max = lg(48, h, kLogicalH), r_bg = lg(150, h, kLogicalH);
  std::vector<int64_t> edge_sig(static_cast<size_t>(h) * 3), bg_sig(static_cast<size_t>(h) * 3), edge_pre, bg_pre;
  std::vector<int32_t> col(static_cast<size_t>(k_bg));
  for (int side = 0; side < 2; ++side) {
    const int32_t band = side == 0 ? ox : cw - (ox + w);
    if (band <= 0) continue;
    auto content_x = [&](int32_t j) { return side == 0 ? ox + j : ox + w - 1 - j; };  // the j-th content column from the edge
    if (wide_ == WideFill::Mirror) {
      for (int32_t d = 1; d <= band; ++d) {  // band column d shows content column d - 1, blurred by d / 3, darkened outwards
        const int32_t j = std::min(d - 1, w - 1), r = d / 3;
        for (int32_t y = 0; y < h; ++y) {
          const uint32_t v = at(content_x(j), y);
          for (int c = 0; c < 3; ++c) edge_sig[static_cast<size_t>(y) * 3 + c] = static_cast<int64_t>(chan(v, c)) * 256;
        }
        prefix(edge_sig, edge_pre);
        const int32_t dark = 256 - static_cast<int32_t>(static_cast<int64_t>(154) * d / band);  // 256 .. 102 (40 %)
        const int32_t x = side == 0 ? ox - d : ox + w - 1 + d;
        for (int32_t y = 0; y < h; ++y) {
          int32_t c8[3];
          for (int c = 0; c < 3; ++c) c8[c] = mean(edge_pre, y, r, c) * dark / 256;
          at(x, y) = pack(c8);
        }
      }
      continue;
    }
    for (int32_t y = 0; y < h; ++y) {
      int64_t s[3] = {0, 0, 0};
      for (int32_t j = 0; j < k_edge; ++j)
        for (int c = 0; c < 3; ++c) s[c] += chan(at(content_x(j), y), c);
      for (int c = 0; c < 3; ++c) {
        edge_sig[static_cast<size_t>(y) * 3 + c] = (s[c] * 256 + k_edge / 2) / k_edge;
        for (int32_t j = 0; j < k_bg; ++j) col[static_cast<size_t>(j)] = chan(at(content_x(j), y), c);
        std::nth_element(col.begin(), col.begin() + k_bg / 2, col.end());
        bg_sig[static_cast<size_t>(y) * 3 + c] = static_cast<int64_t>(col[static_cast<size_t>(k_bg / 2)]) * 256;
      }
    }
    prefix(edge_sig, edge_pre);
    prefix(bg_sig, bg_pre);
    for (int32_t d = 1; d <= band; ++d) {
      const int32_t x = side == 0 ? ox - d : ox + w - 1 + d;
      // the background's smoothstep weight: 0 at the frame's edge, 256 from `fade` client columns on
      const int64_t t = std::min<int64_t>(256, static_cast<int64_t>(d) * 256 / fade);
      const int32_t wb = static_cast<int32_t>(t * t * (3 * 256 - 2 * t) / (256 * 256));
      const int32_t r = std::min(d / 2, r_edge_max);
      for (int32_t y = 0; y < h; ++y) {
        int32_t c8[3];
        for (int c = 0; c < 3; ++c) {
          const int32_t bg = mean(bg_pre, y, r_bg, c);
          c8[c] = wb >= 256 ? bg : (mean(edge_pre, y, r, c) * (256 - wb) + bg * wb + 128) / 256;
        }
        at(x, y) = pack(c8);
      }
    }
  }
}

// P10A: the FPS overlay (present.h set_overlay). Each line of overlay_text is a text run of the game's font (Profiler::font: the
// face and style of gameMain's font call 3) at 800-space size 12 (the fps) or 9 (the phases), drawn by the P9C text path
// (TextRaster) right-aligned at the window's right edge (the run's box at the 800 frame's right edge, moved by the right band's
// width), the lines stacked up from the bottom, over the client pixels darkened to half.
void Presenter::draw_overlay(std::vector<uint32_t> &out, int32_t cw, int32_t ch) const {
#ifdef _WIN32
  if (!overlay_mode_ || cw <= 0 || ch <= 0) return;
  std::string face;
  uint32_t style = 0;
  if (!Profiler::get().font(face, style)) return;  // before the game made its fonts
  const Viewport vp = fit_viewport(cw, ch);
  if (vp.client_w <= 0 || vp.client_h <= 0) return;
  const ProfLine pl = Profiler::get().last();
  const std::string text = overlay_text(pl, overlay_mode_);
  std::vector<std::string> lines;
  for (size_t a = 0;;) {
    const size_t b = text.find('\n', a);
    lines.push_back(text.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  const int32_t size = overlay_mode_ == 1 ? 12 : 9;
  const int32_t height = std::max<int32_t>(1, static_cast<int32_t>(std::lround(size * static_cast<double>(vp.client_h) / kLogicalH)));
  const int32_t dxr = cw - vp.origin_x - vp.client_w;  // the right band's width: the 800 frame's right edge -> the window's
  // the lines' widths in client pixels, with TextRaster's LOGFONTA (grey antialiasing)
  std::vector<int32_t> wpx(lines.size(), 0);
  {
    HDC dc = CreateCompatibleDC(nullptr);
    LOGFONTA lf;
    std::memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -height;
    lf.lfWeight = (style & 1u) ? 700 : 0;
    lf.lfItalic = (style & 2u) ? 1 : 0;
    lf.lfUnderline = (style & 4u) ? 1 : 0;
    lf.lfOutPrecision = 7;  // OUT_TT_ONLY_PRECIS
    lf.lfQuality = 4;       // ANTIALIASED_QUALITY (TextAa::Grey)
    std::memcpy(lf.lfFaceName, face.data(), std::min(face.size(), sizeof(lf.lfFaceName) - 1));
    HFONT f = CreateFontIndirectA(&lf);
    if (dc && f) {
      HGDIOBJ old = SelectObject(dc, f);
      for (size_t i = 0; i < lines.size(); ++i) {
        SIZE sz = {0, 0};
        if (GetTextExtentPoint32A(dc, lines[i].data(), static_cast<int>(lines[i].size()), &sz)) wpx[i] = sz.cx;
      }
      SelectObject(dc, old);
    }
    if (f) DeleteObject(f);
    if (dc) DeleteDC(dc);
  }
  if (!overlay_raster_) overlay_raster_.reset(new TextRaster());
  overlay_raster_->set_aa(TextAa::Grey);
  const int32_t step = size + 3, n = static_cast<int32_t>(lines.size());
  std::vector<portsim::GfxTextRun> runs;
  int32_t bx0 = cw;  // the darkened box's left edge, client pixels
  for (int32_t i = 0; i < n; ++i) {
    portsim::GfxTextRun r;
    r.face = face;
    r.size = size;
    r.style = style;
    r.colorref = 0x00a0f0ffu;  // r 255, g 240, b 160
    const int64_t wl = static_cast<int64_t>(wpx[static_cast<size_t>(i)]);
    const int32_t w800 = static_cast<int32_t>((wl * kLogicalW + vp.client_w - 1) / vp.client_w);
    r.x1 = kLogicalW - 2;
    r.x0 = std::max<int32_t>(0, r.x1 - w800 - 3);
    r.x = r.x0 + 1;
    r.y = kLogicalH - 4 - (n - 1 - i) * step;
    r.y0 = r.y - size - 2;
    r.y1 = std::min<int32_t>(kLogicalH, r.y + size / 3 + 2);
    r.text = lines[static_cast<size_t>(i)];
    r.vis.assign(static_cast<size_t>(r.x1 - r.x0) * static_cast<size_t>(r.y1 - r.y0), 1);
    bx0 = std::min<int32_t>(bx0, vp.origin_x + dxr + static_cast<int32_t>(static_cast<int64_t>(r.x0) * vp.client_w / kLogicalW));
    runs.push_back(std::move(r));
  }
  if (runs.empty()) return;
  const int32_t by0 = std::max<int32_t>(
      0, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(runs.front().y0) * vp.client_h / kLogicalH));
  const int32_t by1 = std::min<int32_t>(
      ch, vp.origin_y + static_cast<int32_t>(static_cast<int64_t>(runs.back().y1) * vp.client_h / kLogicalH));
  for (int32_t y = by0; y < by1; ++y)
    for (int32_t x = std::max<int32_t>(0, bx0 - 2); x < cw; ++x) {
      uint32_t &d = out[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(x)];
      d = ((d >> 1) & 0x007f7f7fu) | 0xff000000u;
    }
  for (const portsim::GfxTextRun &r : runs) overlay_raster_->draw(r, false, vp, dxr, out, cw, ch);
#else
  (void)out; (void)cw; (void)ch;
#endif
}

#ifdef _WIN32
void Presenter::paint(void *hdc, int32_t cw, int32_t ch) const {
  if (cw <= 0 || ch <= 0) return;
  const int64_t t0 = portsim::perf_now_ns();  // P10A
  render(cw, ch, scratch_);
  if (overlay_mode_) {
    const ProfScope prof(Phase::Overlay);  // P10A
    draw_overlay(scratch_, cw, ch);
  }
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = cw;
  bi.bmiHeader.biHeight = -ch;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  {
    const ProfScope prof(Phase::Blit);  // P10A
    SetDIBitsToDevice(static_cast<HDC>(hdc), 0, 0, static_cast<DWORD>(cw), static_cast<DWORD>(ch), 0, 0, 0,
                      static_cast<UINT>(ch), scratch_.data(), &bi, DIB_RGB_COLORS);
  }
  Profiler::get().painted(frames_set_ != painted_set_, portsim::perf_now_ns() - t0);  // P10A
  painted_set_ = frames_set_;
}
#endif

}  // namespace portapp
