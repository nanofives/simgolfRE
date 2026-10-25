// The report screens' side of the graphics backend (roadmap S2.8): what a screen function of the port needs to make the
// original's drawing calls at their places (portsim/sim/gfx.h). The bodies are in port/src/sim/gfx_reports.cpp (family
// `reports`) and the earlier families. Without an attached backend every call costs one test and nothing else runs.
//
// Texts: the original builds a drawn text in g_textBuffer (strcpy / strcat / itoa / appendNumber) and passes its address.
// ScreenDraw::text builds it there the same way, hands its address to the drawing call and then gives g_textBuffer and
// the two formatting buffers (g_textBuf, appendNumber's; g_ui_821c50, the reports' itoa buffer) their bytes back, so a run
// with a backend leaves the GameState as a run without one (as routing.cpp's MapDraw does, roadmap S2.4).
#ifndef PORTSIM_SIM_GFX_REPORTS_H
#define PORTSIM_SIM_GFX_REPORTS_H

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/gfx.h"

namespace portsim {

// The cut rectangle's size of a SpriteRef the backend holds (what cropSprite stores at +0x18 / +0x1c, which the screens
// read for their layout and the port's GameState does not hold: the art is the backend's). False, and an unsupported case
// recorded, when the backend has no sprite there (gfx_reports.cpp).
bool gfx_sprite_cut(Ctx &ctx, uint32_t sref, int32_t &w, int32_t &h);
// the reports family's negative-control mode (gfx_reports.cpp; sg_negctl_set "gfx.family.reports"); the screens read their
// own drawing controls (modes 11..) through ScreenDraw::ctl
extern int g_negctl_gfx_reports;

namespace gfxr {

struct ScreenDraw {
  Ctx &ctx;
  bool on;
  explicit ScreenDraw(Ctx &c) : ctx(c), on(g_gfx_attached.load(std::memory_order_relaxed) != 0 && gfx(c) != nullptr) {}
  // the root window's surface object (the drawing target every report passes)
  uint32_t root() const { return ctx.gs().f_stateptr_4c1570[0]; }
  uint32_t window() const { return state_va(ctx, &GameState::g_rootWindow); }
  uint32_t buf() const { return state_va(ctx, &GameState::g_textBuffer); }
  void gx(const char *name, std::initializer_list<uint32_t> a) const {
    if (on) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  }
  // build(): writes the text into g_textBuffer; draw(va): the drawing calls over it. The buffers are restored after.
  template <class Build, class Draw>
  void text(Build build, Draw draw) const {
    if (!on) return;
    struct Saved {
      uint32_t va;
      std::vector<uint8_t> b;
    };
    const uint32_t regs[3][2] = {{buf(), 0x1000},
                                 {state_va(ctx, &GameState::g_textBuf), 0x10},
                                 {state_va(ctx, &GameState::g_ui_821c50), 0x10}};
    std::vector<Saved> saved;
    for (const auto &r : regs) {
      Saved s{r[0], std::vector<uint8_t>(r[1])};
      std::memcpy(s.b.data(), ctx.mem.at(r[0], r[1]), r[1]);
      saved.push_back(std::move(s));
    }
    struct Restore {
      Ctx &c;
      std::vector<Saved> &s;
      ~Restore() {
        for (const auto &x : s) std::memcpy(c.mem.at(x.va, static_cast<uint32_t>(x.b.size()), true), x.b.data(), x.b.size());
      }
    } restore{ctx, saved};
    build();
    draw(buf());
  }
  // a literal text (digits the original's itoa writes, a copied string) into g_textBuffer
  void put(const std::string &s) const {
    uint8_t *p = ctx.mem.at(buf(), static_cast<uint32_t>(s.size() + 1), true);
    std::memcpy(p, s.data(), s.size());
    p[s.size()] = 0;
  }
  void append(const std::string &s) const {
    const uint32_t n = vstrlen(ctx.mem, buf());
    uint8_t *p = ctx.mem.at(buf() + n, static_cast<uint32_t>(s.size() + 1), true);
    std::memcpy(p, s.data(), s.size());
    p[s.size()] = 0;
  }
  void append_va(uint32_t src) const { vstrcat(ctx.mem, buf(), src); }
  uint32_t len(uint32_t va) const { return va ? vstrlen(ctx.mem, va) : 0; }
  // a screen-side negative control of the reports family (drawing only: the GameState never depends on it)
  static bool ctl(int m) { return SG_NEGCTL(g_negctl_gfx_reports) == m; }
  int32_t cut_w(uint32_t sref) const {
    int32_t w = 0, h = 0;
    if (on) gfx_sprite_cut(ctx, sref, w, h);
    return w;
  }
  int32_t cut_h(uint32_t sref) const {
    int32_t w = 0, h = 0;
    if (on) gfx_sprite_cut(ctx, sref, w, h);
    return h;
  }
};

}  // namespace gfxr
}  // namespace portsim

#endif
