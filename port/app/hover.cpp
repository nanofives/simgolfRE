// P10I fix 2 (log/roadmap/P10/P10I_notes.md): the HUD's hover highlights between game frames (present.h Presenter::hover_*).
//
// The game decides every hover in its own frame code, once per frame, from the cursor point its View_toLocal / InputSource::cursor
// read got (no shared hit test: the radial menu's nearestMenuSpot, the palettes' hitTestSlot432f90 / hitBuildTool433c60 /
// pickAmenity434980 / hitBuildingTool, the panels' hitGolferPanel435f00 / hitEmployeeSlot435570 / hitEmployee436b00, the title
// menu's nearest text point, the full-screen menus' own circles and rectangles), and draws the hovered control with another sprite
// cell, text colour or offset. So at 10.7 frames a second a highlight follows the cursor at that rate. Nothing here runs or
// replicates those tests: the presenter learns from the frames themselves.
//
// Per SURFACE (a HUD panel layer by its id, kGfxLayerTitle .. kGfxLayerPanel, when the frame holds exactly one layer of that id;
// or the whole 800 x 600 frame of a screen without layers or world, the title and full-screen menus):
// - a CONTROL is a box of the surface whose pixels (or text runs) changed between two consecutive frames whose cursor points
//   differ, at most kMaxW x kMaxH, with one of the two points within kNear of it (a hover candidate; a palette or a screen that
//   opens is larger and is not one). Overlapping boxes merge.
// - a LOOK of a control is the surface's content over its box: the layer pixels (GfxLayerPx, transparent outside the layer) or the
//   frame's words, plus the text runs whose box meets it (the layered path draws a layer's runs; a lit menu item changes its run's
//   colour and origin). Every frame adds an OBSERVATION: the frame's cursor point and the look the frame shows. A look never seen
//   before resets the control to that look (a context change: a tool selected, another panel in the layer's place), and so does
//   a frame whose look differs from the one its own cursor point predicts.
// - the PREDICTION for a cursor point: the look of the nearest observation, among the looks whose observed points' box (grown by
//   kGrow) holds the point; none when no look's box holds it. A control with one look never changes.
// Between frames (hover_tick, the live cursor point at the display's rate) each control of the frame shows the predicted look (the
// frame's own when there is none or the cursor is at the frame's point): its pixels are written into the composed layer (or the
// frame's words) and its runs replace the runs over its box. The next frame brings the game's own picture back. The click remap's
// coverage keeps the frame's own pixels (Presenter::hover_pristine_), so clicks are unchanged.
// Not covered: what does not follow the cursor's position alone (tooltips and labels after a dwell count, the radial menu's label,
// text that changes with time), a control whose lit look was not yet seen at a nearby point (today's behaviour), the centred
// layers (the course, labels, previews: P10E moves those), and screens drawn without a present between their passes.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

#include "present.h"

namespace portapp {

namespace {
constexpr int32_t kMaxW = 240, kMaxH = 160;  // a control's box at most (800-space pixels)
constexpr int32_t kNear = 48;               // a moved cursor's point this close to a changed box makes it a control
constexpr int32_t kGrow = 2;                 // a look's observed-point box grown by this
constexpr size_t kMaxObs = 512, kMaxLooks = 8, kMaxControls = 96;
constexpr int32_t kCell = 2;                 // the changed-pixel grid's cell (components join across 1-pixel gaps only: two
                                             // neighbouring palette tiles that light and unlight in one frame stay apart)

struct Box {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // half-open
  bool empty() const { return x1 <= x0 || y1 <= y0; }
};
bool meets(const Box &a, const Box &b) { return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1; }
Box unite(const Box &a, const Box &b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  return {std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}
int32_t gap(const Box &b, int32_t x, int32_t y) {  // Chebyshev distance of a point from a box (0 inside)
  const int32_t dx = x < b.x0 ? b.x0 - x : x >= b.x1 ? x - b.x1 + 1 : 0;
  const int32_t dy = y < b.y0 ? b.y0 - y : y >= b.y1 ? y - b.y1 + 1 : 0;
  return std::max(dx, dy);
}

uint64_t fnv(uint64_t h, const void *p, size_t n) {
  const uint8_t *b = static_cast<const uint8_t *>(p);
  for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
  return h;
}
uint64_t run_sig(const portsim::GfxTextRun &r) {
  uint64_t h = 1469598103934665603ull;
  const int32_t v[11] = {static_cast<int32_t>(r.fontref), r.size, static_cast<int32_t>(r.style), static_cast<int32_t>(r.colorref), r.x,
                         r.y, r.x0, r.y0, r.x1, r.y1, r.target};
  h = fnv(h, v, sizeof(v));
  h = fnv(h, r.face.data(), r.face.size());
  return fnv(h, r.text.data(), r.text.size());
}
Box run_box(const portsim::GfxTextRun &r) { return {r.x0, r.y0, r.x1, r.y1}; }

// one surface of a frame: a HUD layer (its index in the frame) or the flat frame's words, with the runs drawn over it
struct View {
  const portsim::GfxHudLayer *layer = nullptr;
  portsim::GfxHudLayer *wlayer = nullptr;
  int32_t li = -1;
  const std::vector<uint16_t> *flat = nullptr;
  std::vector<uint16_t> *wflat = nullptr;
  const std::vector<portsim::GfxTextRun> *runs = nullptr;  // null = runs are not drawn
  bool run_of(const portsim::GfxTextRun &r) const {
    return layer ? r.layer == li : r.target != 2;
  }
  uint64_t key(int32_t x, int32_t y) const {
    if (layer) {
      const portsim::GfxHudLayer &l = *layer;
      if (x < l.x0 || y < l.y0 || x >= l.x1 || y >= l.y1) return 0;
      const portsim::GfxLayerPx &p =
          l.px[static_cast<size_t>(y - l.y0) * static_cast<size_t>(l.x1 - l.x0) + static_cast<size_t>(x - l.x0)];
      return (1ull << 48) | p.px | (static_cast<uint64_t>(p.src) << 16) | (static_cast<uint64_t>(p.kind) << 32) |
             (static_cast<uint64_t>(p.k) << 40);
    }
    if (!flat || x < 0 || y < 0 || x >= kLogicalW || y >= kLogicalH) return 0;
    return (1ull << 48) | (*flat)[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)];
  }
};

struct Look {
  std::vector<uint64_t> key;  // the box's pixels, row by row
  std::vector<portsim::GfxTextRun> runs;
  uint64_t hash = 0;
  bool operator==(const Look &o) const { return hash == o.hash && key == o.key && runs.size() == o.runs.size(); }
};
// a run belongs to the control whose box holds its box's centre
bool run_in(const portsim::GfxTextRun &r, const Box &b) {
  const int32_t x = (r.x0 + r.x1) / 2, y = (r.y0 + r.y1) / 2;
  return x >= b.x0 && x < b.x1 && y >= b.y0 && y < b.y1;
}
// the look over the control's pixels (m: 1 per box pixel that belongs to it)
Look capture(const View &v, const Box &b, const std::vector<uint8_t> &m) {
  Look L;
  const int32_t bw = b.x1 - b.x0;
  for (int32_t y = b.y0; y < b.y1; ++y)
    for (int32_t x = b.x0; x < b.x1; ++x)
      if (m[static_cast<size_t>(y - b.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - b.x0)]) L.key.push_back(v.key(x, y));
  uint64_t h = fnv(1469598103934665603ull, L.key.data(), L.key.size() * sizeof(uint64_t));
  if (v.runs)
    for (const portsim::GfxTextRun &r : *v.runs)
      if (v.run_of(r) && run_in(r, b)) {
        L.runs.push_back(r);
        const uint64_t s = run_sig(r);
        h = fnv(h, &s, sizeof(s));
      }
  L.hash = h;
  return L;
}

struct Obs {
  int16_t x, y;
  uint8_t look;
};
struct Control {
  int32_t surf = 0;
  Box b;
  std::vector<uint8_t> m;  // (b.x1 - b.x0) * (b.y1 - b.y0): 1 where the pixel is the control's (it changed with it)
  std::vector<Look> looks;
  std::vector<Obs> obs;
  std::vector<Box> hb;  // per look: the box of the points it was seen at
  int32_t cur = -1;     // the current frame's look (-1: the surface is not in the frame)
  int32_t shown = -1;   // the look the picture shows
  // hb[X] = the box of the points look X was seen at; a look seen only with the cursor inside the control's own box (a lit tile
  // or button) gets that whole box (the game's hit areas there are the control's slot / circle, which its lit pixels cover)
  void rebox() {
    hb.assign(looks.size(), Box{});
    std::vector<uint8_t> out(looks.size(), 0);
    for (const Obs &o : obs) {
      hb[o.look] = unite(hb[o.look], Box{o.x, o.y, o.x + 1, o.y + 1});
      if (gap(b, o.x, o.y) > 0) out[o.look] = 1;
    }
    for (size_t i = 0; i < hb.size(); ++i)
      if (!hb[i].empty() && !out[i]) hb[i] = unite(hb[i], b);
  }
  void add(int32_t x, int32_t y, int32_t look) {
    if (obs.size() >= kMaxObs) obs.erase(obs.begin(), obs.begin() + static_cast<std::ptrdiff_t>(kMaxObs / 4));
    obs.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<uint8_t>(look)});
    rebox();
  }
  int32_t find(const Look &L) const {
    for (size_t i = 0; i < looks.size(); ++i)
      if (looks[i] == L) return static_cast<int32_t>(i);
    return -1;
  }
  int32_t predict(int32_t x, int32_t y) const {
    int32_t best = -1;
    int64_t bd = INT64_MAX;
    for (const Obs &o : obs) {
      const Box &h = hb[o.look];
      if (x < h.x0 - kGrow || x >= h.x1 + kGrow || y < h.y0 - kGrow || y >= h.y1 + kGrow) continue;
      const int64_t dx = o.x - x, dy = o.y - y, d = dx * dx + dy * dy;
      if (d < bd) {
        bd = d;
        best = o.look;
      }
    }
    return best;
  }
};

// a surface of the last frame (its content, for the next frame's changed boxes)
struct Snap {
  portsim::GfxHudLayer layer;
  bool layered = false;
  std::vector<uint16_t> flat;
  std::vector<portsim::GfxTextRun> runs;
  bool with_runs = false;
  int32_t cx = -1, cy = -1;
  View view() const {
    View v;
    if (layered) {
      v.layer = &layer;
      v.li = layer.id;  // the snap's runs carry li = the layer id (set when taken)
    } else {
      v.flat = &flat;
    }
    if (with_runs) v.runs = &runs;
    return v;
  }
};
}  // namespace

// development: SG_P10I_LOG=<file>, one line per control made / reset / mispredicted and per swap
std::FILE *hlog() {
  static std::FILE *f = std::getenv("SG_P10I_LOG") ? std::fopen(std::getenv("SG_P10I_LOG"), "w") : nullptr;
  return f;
}
struct HoverModel {
  int mode = -1;  // 2 * (layered) + text
  std::vector<Control> controls;
  std::unordered_map<int32_t, Snap> prev;
  HoverStats st;
  int32_t fcx = -1, fcy = -1;  // the frame's cursor point
};

namespace {
// the surfaces of the presenter's composed picture: id -> view (kFlat = the whole frame)
constexpr int32_t kFlat = -1;
// a HUD panel layer's surface: its id and how many layers of that id the frame drew before it (the radial menu and the Build
// palette are two kGfxLayerBottom layers); -1 for a centred layer
int32_t surf_key(const portsim::GfxHudFrame &F, size_t i) {
  const int32_t id = F.layers[i].id;
  if (id <= portsim::kGfxLayerCentred || id >= portsim::kGfxLayerCount) return -1;
  int32_t n = 0;
  for (size_t j = 0; j < i; ++j) n += F.layers[j].id == id ? 1 : 0;
  return id * 16 + std::min(n, 15);
}
}  // namespace

// (member helpers are lambdas inside the two methods: they need the presenter's private state)

void Presenter::hover_observe(int32_t cx, int32_t cy, const Viewport &vp) {
  if (!hover_on_) return;
  if (!hover_) hover_ = std::make_shared<HoverModel>();
  HoverModel &M = *hover_;
  ++M.st.frames;
  M.fcx = cx;
  M.fcy = cy;
  for (Control &c : M.controls) c.cur = c.shown = -1;
  if (cx < 0 || cy < 0 || cx >= kLogicalW || cy >= kLogicalH || frame_.empty()) return;
  const bool text = text_now(vp);
  const bool lay = layered();
  const bool flat = !lay && world_.empty();
  if (!lay && !flat) return;
  const int mode = (lay ? 2 : 0) + (text ? 1 : 0);
  if (mode != M.mode) {
    M.controls.clear();
    M.prev.clear();
    M.mode = mode;
  }
  // the surfaces of this frame
  std::unordered_map<int32_t, View> views;
  if (lay) {
    portsim::GfxHudFrame &F = text && text_layered_ok_ ? hud_text_ : world_.hud;
    for (size_t i = 0; i < F.layers.size(); ++i) {
      portsim::GfxHudLayer &l = F.layers[i];
      const int32_t key = surf_key(F, i);
      if (key < 0 || l.x1 <= l.x0) continue;
      View v;
      v.layer = v.wlayer = &l;
      v.li = static_cast<int32_t>(i);
      if (text) v.runs = &text_.runs;
      views[key] = v;
    }
  } else {
    std::vector<uint16_t> &src = text && text_defect_ != TextDefect::Unremoved ? text_.clean : frame_;
    if (src.size() != static_cast<size_t>(kLogicalW) * kLogicalH) return;
    View v;
    v.flat = v.wflat = &src;
    if (text) v.runs = &text_.runs;
    views[kFlat] = v;
  }
  std::vector<uint8_t> diff;
  if (hlog()) {
    std::fprintf(hlog(), "%llu frame at %d,%d mode %d surfaces", static_cast<unsigned long long>(M.st.frames), cx, cy, mode);
    for (const auto &sv : views)
      std::fprintf(hlog(), " s%d:%d,%d-%d,%d", sv.first, sv.second.layer ? sv.second.layer->x0 : 0, sv.second.layer ? sv.second.layer->y0 : 0,
                   sv.second.layer ? sv.second.layer->x1 : kLogicalW, sv.second.layer ? sv.second.layer->y1 : kLogicalH);
    std::fprintf(hlog(), "\n");
  }
  for (auto &sv : views) {
    const int32_t id = sv.first;
    const View &v = sv.second;
    std::vector<size_t> fresh;  // controls made from this frame's change (their observation is already in)
    auto pit = M.prev.find(id);
    if (pit != M.prev.end() && (pit->second.cx != cx || pit->second.cy != cy)) {
      const Snap &P = pit->second;
      const View pv = P.view();
      // the changed pixels: the union of both layers' boxes (the whole frame), and the boxes of runs in one frame only
      Box u = v.layer ? Box{v.layer->x0, v.layer->y0, v.layer->x1, v.layer->y1} : Box{0, 0, kLogicalW, kLogicalH};
      if (P.layered) u = unite(u, Box{P.layer.x0, P.layer.y0, P.layer.x1, P.layer.y1});
      u.x0 = std::max(u.x0, 0);
      u.y0 = std::max(u.y0, 0);
      u.x1 = std::min(u.x1, kLogicalW);
      u.y1 = std::min(u.y1, kLogicalH);
      diff.assign(static_cast<size_t>(kLogicalW) * kLogicalH, 0);
      Box mk;  // the changed pixels' box
      for (int32_t y = u.y0; y < u.y1; ++y)
        for (int32_t x = u.x0; x < u.x1; ++x)
          if (v.key(x, y) != pv.key(x, y)) {
            diff[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)] = 1;
            mk = unite(mk, Box{x, y, x + 1, y + 1});
          }
      if (v.runs && P.with_runs) {
        std::vector<uint64_t> a, b;
        for (const portsim::GfxTextRun &r : *v.runs)
          if (v.run_of(r)) a.push_back(run_sig(r));
        for (const portsim::GfxTextRun &r : P.runs) b.push_back(run_sig(r));
        auto mark = [&](const portsim::GfxTextRun &r) {
          mk = unite(mk, Box{std::max(r.x0, 0), std::max(r.y0, 0), std::min(r.x1, kLogicalW), std::min(r.y1, kLogicalH)});
          for (int32_t y = std::max(r.y0, 0); y < std::min(r.y1, kLogicalH); ++y)
            for (int32_t x = std::max(r.x0, 0); x < std::min(r.x1, kLogicalW); ++x) diff[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)] = 1;
        };
        for (const portsim::GfxTextRun &r : *v.runs)
          if (v.run_of(r) && std::find(b.begin(), b.end(), run_sig(r)) == b.end()) mark(r);
        for (const portsim::GfxTextRun &r : P.runs)
          if (std::find(a.begin(), a.end(), run_sig(r)) == a.end()) mark(r);
      }
      // components over a grid of kCell cells (8-connected), each with its changed pixels (box and mask)
      const int32_t gw = kLogicalW / kCell, gh = kLogicalH / kCell;
      std::vector<Box> cellbox(static_cast<size_t>(gw) * gh);
      std::vector<uint8_t> on(static_cast<size_t>(gw) * gh, 0);
      for (int32_t y = mk.y0; y < mk.y1; ++y)
        for (int32_t x = mk.x0; x < mk.x1; ++x)
          if (diff[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)]) {
            const size_t c = static_cast<size_t>(y / kCell) * gw + static_cast<size_t>(x / kCell);
            cellbox[c] = unite(cellbox[c], Box{x, y, x + 1, y + 1});
            on[c] = 1;
          }
      struct Cand {
        Box b;
        std::vector<uint8_t> m;
      };
      std::vector<Cand> cands;
      std::vector<int32_t> stack, cells;
      for (int32_t c0 = 0; c0 < gw * gh; ++c0) {
        if (!on[static_cast<size_t>(c0)]) continue;
        Box b;
        stack.assign(1, c0);
        cells.clear();
        on[static_cast<size_t>(c0)] = 0;
        while (!stack.empty()) {
          const int32_t c = stack.back();
          stack.pop_back();
          cells.push_back(c);
          b = unite(b, cellbox[static_cast<size_t>(c)]);
          const int32_t gx = c % gw, gy = c / gw;
          for (int32_t ny = std::max(gy - 1, 0); ny <= std::min(gy + 1, gh - 1); ++ny)
            for (int32_t nx = std::max(gx - 1, 0); nx <= std::min(gx + 1, gw - 1); ++nx) {
              const size_t n = static_cast<size_t>(ny) * gw + static_cast<size_t>(nx);
              if (on[n]) {
                on[n] = 0;
                stack.push_back(static_cast<int32_t>(n));
              }
            }
        }
        const bool big = b.x1 - b.x0 > kMaxW || b.y1 - b.y0 > kMaxH;
        const bool far = gap(b, cx, cy) > kNear && gap(b, P.cx, P.cy) > kNear;
        if (hlog())
          std::fprintf(hlog(), "%llu change s%d %d,%d-%d,%d at %d,%d / %d,%d%s%s\n", static_cast<unsigned long long>(M.st.frames), id,
                       b.x0, b.y0, b.x1, b.y1, P.cx, P.cy, cx, cy, big ? " big" : "", far ? " far" : "");
        if (big || far) continue;
        Cand cd;
        cd.b = b;
        cd.m.assign(static_cast<size_t>(b.x1 - b.x0) * static_cast<size_t>(b.y1 - b.y0), 0);
        for (int32_t c : cells) {
          const int32_t gx = c % gw, gy = c / gw;
          for (int32_t y = gy * kCell; y < gy * kCell + kCell; ++y)
            for (int32_t x = gx * kCell; x < gx * kCell + kCell; ++x)
              if (diff[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)])
                cd.m[static_cast<size_t>(y - b.y0) * static_cast<size_t>(b.x1 - b.x0) + static_cast<size_t>(x - b.x0)] = 1;
        }
        cands.push_back(std::move(cd));
      }
      auto mask_at = [](const Box &b, const std::vector<uint8_t> &m, int32_t x, int32_t y) {
        return x >= b.x0 && y >= b.y0 && x < b.x1 && y < b.y1 &&
               m[static_cast<size_t>(y - b.y0) * static_cast<size_t>(b.x1 - b.x0) + static_cast<size_t>(x - b.x0)] != 0;
      };
      for (const Cand &cd : cands) {
        // the controls sharing a pixel with this change (boxes alone may overlap: the palette's staggered tiles)
        std::vector<size_t> hit;
        bool inside_one = false;
        for (size_t i = 0; i < M.controls.size(); ++i) {
          const Control &c = M.controls[i];
          if (c.surf != id || !meets(c.b, cd.b)) continue;
          bool share = false, all = true;
          for (int32_t y = cd.b.y0; y < cd.b.y1; ++y)
            for (int32_t x = cd.b.x0; x < cd.b.x1; ++x)
              if (mask_at(cd.b, cd.m, x, y)) {
                if (mask_at(c.b, c.m, x, y)) share = true;
                else all = false;
              }
          if (!share) continue;
          hit.push_back(i);
          inside_one = inside_one || all;
        }
        if (hit.size() == 1 && inside_one) continue;  // the control this change belongs to
        Box b = cd.b;
        for (size_t h : hit) b = unite(b, M.controls[h].b);
        if (b.x1 - b.x0 > kMaxW || b.y1 - b.y0 > kMaxH) continue;
        std::vector<uint8_t> m(static_cast<size_t>(b.x1 - b.x0) * static_cast<size_t>(b.y1 - b.y0), 0);
        for (int32_t y = b.y0; y < b.y1; ++y)
          for (int32_t x = b.x0; x < b.x1; ++x) {
            bool in = mask_at(cd.b, cd.m, x, y);
            for (size_t h : hit) in = in || mask_at(M.controls[h].b, M.controls[h].m, x, y);
            if (in) m[static_cast<size_t>(y - b.y0) * static_cast<size_t>(b.x1 - b.x0) + static_cast<size_t>(x - b.x0)] = 1;
          }
        fresh.erase(std::remove_if(fresh.begin(), fresh.end(),
                                   [&](size_t f) { return std::find(hit.begin(), hit.end(), f) != hit.end(); }),
                    fresh.end());
        std::sort(hit.begin(), hit.end());
        for (size_t k = hit.size(); k-- > 0;) M.controls.erase(M.controls.begin() + static_cast<std::ptrdiff_t>(hit[k]));
        for (size_t &f : fresh) {  // indices after the erase
          size_t d = 0;
          for (size_t h : hit) d += h < f ? 1 : 0;
          f -= d;
        }
        if (M.controls.size() >= kMaxControls) continue;
        Control c;
        c.surf = id;
        c.b = b;
        c.m = std::move(m);
        c.looks.push_back(capture(pv, c.b, c.m));
        c.obs.push_back({static_cast<int16_t>(P.cx), static_cast<int16_t>(P.cy), 0});
        Look L = capture(v, c.b, c.m);
        int32_t li = c.find(L);
        if (li < 0) {
          c.looks.push_back(std::move(L));
          li = 1;
        }
        c.obs.push_back({static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<uint8_t>(li)});
        c.rebox();
        c.cur = c.shown = li;
        M.controls.push_back(std::move(c));
        fresh.push_back(M.controls.size() - 1);
        ++M.st.controls_made;
        if (hlog()) std::fprintf(hlog(), "%llu made s%d %d,%d-%d,%d looks %zu at %d,%d / %d,%d merged %zu\n",
                                 static_cast<unsigned long long>(M.st.frames), id, b.x0, b.y0, b.x1, b.y1, M.controls.back().looks.size(),
                                 P.cx, P.cy, cx, cy, hit.size());
      }
    }
    // every other control of the surface: this frame's look, observed at its cursor point
    for (size_t i = 0; i < M.controls.size(); ++i) {
      Control &c = M.controls[i];
      if (c.surf != id || std::find(fresh.begin(), fresh.end(), i) != fresh.end()) continue;
      Look L = capture(v, c.b, c.m);
      int32_t li = c.find(L);
      if (li < 0) {  // never seen: a new context for this box
        c.looks.clear();
        c.looks.push_back(std::move(L));
        c.obs.clear();
        li = 0;
        ++M.st.resets;
        if (hlog()) std::fprintf(hlog(), "%llu reset s%d %d,%d-%d,%d at %d,%d\n", static_cast<unsigned long long>(M.st.frames), id, c.b.x0,
                                 c.b.y0, c.b.x1, c.b.y1, cx, cy);
      } else {
        const int32_t p = c.predict(cx, cy);
        if (p >= 0 && p != li) {  // the frame contradicts what its own point predicts
          c.obs.clear();
          ++M.st.mispredicts;
          if (hlog()) std::fprintf(hlog(), "%llu mispredict s%d %d,%d-%d,%d at %d,%d: %d not %d\n", static_cast<unsigned long long>(M.st.frames), id,
                                   c.b.x0, c.b.y0, c.b.x1, c.b.y1, cx, cy, li, p);
        }
      }
      c.add(cx, cy, li);
      c.cur = c.shown = li;
    }
    // this surface as the next frame's previous one
    Snap &S = M.prev[id];
    S.layered = v.layer != nullptr;
    if (S.layered) S.layer = *v.layer;
    else S.flat = *v.flat;
    S.with_runs = v.runs != nullptr;
    S.runs.clear();
    if (v.runs)
      for (const portsim::GfxTextRun &r : *v.runs)
        if (v.run_of(r)) {
          S.runs.push_back(r);
          if (S.layered) S.runs.back().layer = v.layer->id;
        }
    S.cx = cx;
    S.cy = cy;
  }
  // surfaces this frame does not hold are forgotten as previous frames
  for (auto it = M.prev.begin(); it != M.prev.end();) it = views.count(it->first) ? std::next(it) : M.prev.erase(it);
  M.st.controls = static_cast<uint32_t>(M.controls.size());
}

bool Presenter::hover_tick(int32_t cx, int32_t cy, const Viewport &vp) {
  if (!hover_on_ || !hover_ || frame_.empty()) return false;
  HoverModel &M = *hover_;
  ++M.st.ticks;
  const bool text = text_now(vp);
  const bool lay = layered();
  if (M.mode != (lay ? 2 : 0) + (text ? 1 : 0)) return false;
  portsim::GfxHudFrame *F = lay ? (text && text_layered_ok_ ? &hud_text_ : &world_.hud) : nullptr;
  std::vector<uint16_t> *flat = !lay ? (text && text_defect_ != TextDefect::Unremoved ? &text_.clean : &frame_) : nullptr;
  bool changed = false;
  const bool at_frame = cx == M.fcx && cy == M.fcy;
  for (Control &c : M.controls) {
    if (c.cur < 0) continue;
    int32_t want = c.cur;
    if (!at_frame) {
      const int32_t p = c.predict(cx, cy);
      if (p >= 0) want = p;
    }
    if (want == c.shown || want < 0 || static_cast<size_t>(want) >= c.looks.size()) continue;
    const Look &L = c.looks[static_cast<size_t>(want)];
    // the surface in this frame
    portsim::GfxHudLayer *l = nullptr;
    int32_t li = -1;
    if (lay) {
      for (size_t i = 0; i < F->layers.size(); ++i)
        if (surf_key(*F, i) == c.surf) {
          l = &F->layers[i];
          li = static_cast<int32_t>(i);
          break;
        }
      if (!l) continue;
    } else if (c.surf != kFlat || !flat) {
      continue;
    }
    const int32_t bw = c.b.x1 - c.b.x0;
    auto mine = [&](int32_t x, int32_t y) { return c.m[static_cast<size_t>(y - c.b.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - c.b.x0)] != 0; };
    if (l) {  // every pixel the look holds must lie inside the layer's box
      bool fits = true;
      size_t n = 0;
      for (int32_t y = c.b.y0; y < c.b.y1 && fits; ++y)
        for (int32_t x = c.b.x0; x < c.b.x1; ++x) {
          if (!mine(x, y)) continue;
          if (n < L.key.size() && L.key[n] && (x < l->x0 || y < l->y0 || x >= l->x1 || y >= l->y1)) {
            fits = false;
            break;
          }
          ++n;
        }
      if (!fits) continue;
      if (hover_pristine_.size() < F->layers.size()) hover_pristine_.resize(F->layers.size());
      if (hover_pristine_[static_cast<size_t>(li)].empty()) hover_pristine_[static_cast<size_t>(li)] = l->px;
    }
    size_t n = 0;
    for (int32_t y = c.b.y0; y < c.b.y1; ++y)
      for (int32_t x = c.b.x0; x < c.b.x1; ++x) {
        if (!mine(x, y)) continue;
        const uint64_t k = n < L.key.size() ? L.key[n] : 0;
        ++n;
        if (l) {
          if (x < l->x0 || y < l->y0 || x >= l->x1 || y >= l->y1) continue;
          portsim::GfxLayerPx &p = l->px[static_cast<size_t>(y - l->y0) * static_cast<size_t>(l->x1 - l->x0) + static_cast<size_t>(x - l->x0)];
          p.px = static_cast<uint16_t>(k & 0xffffu);
          p.src = static_cast<uint16_t>((k >> 16) & 0xffffu);
          p.kind = static_cast<uint8_t>((k >> 32) & 0xffu);
          p.k = static_cast<uint8_t>((k >> 40) & 0xffu);
          if (!k) p = portsim::GfxLayerPx{};
        } else if (k) {
          (*flat)[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)] = static_cast<uint16_t>(k & 0xffffu);
        }
      }
    if (text) {  // the runs over the box: the look's
      std::vector<portsim::GfxTextRun> &runs = text_.runs;
      runs.erase(std::remove_if(runs.begin(), runs.end(),
                                [&](const portsim::GfxTextRun &r) {
                                  return (l ? r.layer == li : r.target != 2) && run_in(r, c.b);
                                }),
                 runs.end());
      for (portsim::GfxTextRun r : L.runs) {
        if (l) r.layer = li;
        runs.push_back(std::move(r));
      }
    }
    c.shown = want;
    changed = true;
    ++M.st.swaps;
    if (hlog()) std::fprintf(hlog(), "%llu swap s%d %d,%d-%d,%d at %d,%d to %d (frame %d)\n", static_cast<unsigned long long>(M.st.frames), c.surf,
                             c.b.x0, c.b.y0, c.b.x1, c.b.y1, cx, cy, want, c.cur);
  }
  if (changed) ++M.st.paints;
  return changed;
}

HoverStats Presenter::hover_stats() const { return hover_ ? hover_->st : HoverStats{}; }

}  // namespace portapp
