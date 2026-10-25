// Body family `credits` of the graphics backend (roadmap S2.3; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// parts of showCreditsScreen's drawing that read backend data the port's screen function does not hold. Specification:
// shim/src/re/c3en.cpp ShowCreditsScreen_re and re/analysis/video/0044b9c0_showCreditsScreen.md; the rest of its drawing goes
// through the primitives of gfx_surface.cpp / gfx_prim_text.cpp and the core bodies (loadImage, Image_draw, setDrawTarget,
// Window::refreshFull), called by the port's show_credits_screen (commands_screens.inc).
//
//   showCreditsScreen.logo(logo, img)
//       the logo Surface takes the image just loaded at the size its own surface reports (the handle's slots +0xd8 width and
//       +0xdc height; 0 x 0 when the Image has no handle): Surface_blitTakeDirty(logo, img, 0, 0, w, h, 1, 1)
//   showCreditsScreen.lines(count)
//       the kept lines of the credits section, read as the original's reader leaves them: the file named by the string pool's
//       text +8 (with the reader's default extension when it has no '.'), the section key `#` + the pool's text +0x14 matched
//       case-insensitively against each line stripped of its newline and trimmed of spaces (Reader::open), then each line,
//       stripped and trimmed the same way (Reader::readLine), up to one that starts with '#'. The original keeps a malloc'd copy
//       of each; only the drawing reads them, so the backend keeps its own. `count` is the port's count of the same lines: a
//       different count is reported and no line is kept.
//   showCreditsScreen.line(k, y, logo)
//       line k at pen y: a line equal to the pool's text +0 draws the logo centred, Surface_drawRegion(logo, Root,
//       (0x320 - logo's kept width +0x10) / 2, y, 0) (0x320 - 0 when the logo has no sprite); any other non-empty line
//       drawText(Root, line, 0x32, y, strlen(line))
//   showCreditsScreen.free()
//       the blocks freed back to front: the kept lines are dropped
//
// Negative controls (g_negctl_gfx_credits, sg_negctl_set "gfx.family.credits"):
//   1 the logo line drawn at x 0x32 instead of centred   2 line k drawn with the text of line k + 1
//   3 the kept lines not trimmed of leading spaces
#include <cstdint>
#include <string>
#include <vector>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"

namespace portsim {

int g_negctl_gfx_credits = 0;

namespace gfxp {
// gfx_prim_text.cpp: drawText's path for a text the caller holds
void draw_text_run(Gfx &g, Ctx &ctx, uint32_t target, const std::string &text, uint32_t va, int32_t x, int32_t y,
                   const char *who);
}  // namespace gfxp

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_credits); }

struct CreditsState {
  std::vector<std::string> lines;
};
CreditsState &st(Gfx &g) { return g.state_of<CreditsState>("credits"); }

void call_body(Gfx &g, Ctx &ctx, const char *name, const uint32_t *a, int n) {
  if (GfxBody b = gfxb::find_body(name)) {
    b(g, ctx, a, n);
    return;
  }
  g.unsup(std::string("gfx_credits: no body ") + name);
}

void b_logo(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  int32_t w = 0, h = 0;
  auto it = g.images.find(a[1]);
  if (it != g.images.end()) {
    w = it->second.w;
    h = it->second.h;
  }
  const uint32_t b[8] = {a[0], a[1], 0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1, 1};
  call_body(g, ctx, "Surface_blitTakeDirty", b, 8);
}

std::string lower(std::string s) {
  for (char &c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return s;
}
// stripNewline then trimSpaces as the port's reader model does them (commands_screens.inc credits_line_count)
std::string trim(std::string s, bool lead) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t k = 0;
  while (lead && k < s.size() && (s[k] == ' ' || s[k] == '\t')) ++k;
  return s.substr(k);
}

void b_lines(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  CreditsState &cs = st(g);
  cs.lines.clear();
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4d21a4);
  std::string name = gfxb::vstring(ctx.mem, pool + 8);
  if (name.find('.') == std::string::npos) name += gfxb::vstring(ctx.mem, state_va(ctx, &GameState::f_strpool_4e4418));
  if (ctx.files_ptr == nullptr || !ctx.files().exists(name)) {
    g.unsup("showCreditsScreen.lines: the credits file is not in the game files");
    return;
  }
  const std::vector<uint8_t> data = ctx.files().read_all(name);
  std::vector<std::string> raw;   // fgets in text mode: CR LF read as LF
  std::string cur;
  for (size_t i = 0; i < data.size(); ++i) {
    if (data[i] == '\r' && i + 1 < data.size() && data[i + 1] == '\n') continue;
    if (data[i] == '\n') {
      raw.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(static_cast<char>(data[i]));
    }
  }
  if (!cur.empty()) raw.push_back(cur);
  const std::string key = lower(gfxb::vstring(ctx.mem, state_va(ctx, &GameState::f_str_4e4438)) + gfxb::vstring(ctx.mem, pool + 0x14));
  size_t i = 0;
  while (i < raw.size() && lower(trim(raw[i], true)) != key) ++i;
  std::vector<std::string> kept;
  for (++i; i < raw.size(); ++i) {
    const std::string t = trim(raw[i], mode() != 3);
    if (!t.empty() && t[0] == '#') break;
    kept.push_back(t);
  }
  if (static_cast<uint32_t>(kept.size()) != a[0]) {
    g.unsup("showCreditsScreen.lines: the backend's line count differs from the screen's");
    return;
  }
  cs.lines = std::move(kept);
}

void b_line(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const CreditsState &cs = st(g);
  size_t k = a[0];
  if (mode() == 2) ++k;
  if (k >= cs.lines.size()) {
    if (mode() != 2) g.unsup("showCreditsScreen.line: a line the backend does not hold");
    return;
  }
  const std::string &s = cs.lines[k];
  const int32_t y = static_cast<int32_t>(a[1]);
  const uint32_t root = g.root_va(ctx);
  if (s.empty()) return;   // drawText of an empty line draws nothing (n <= 0)
  if (s == gfxb::vstring(ctx.mem, state_va(ctx, &GameState::f_strpool_4d21a4))) {
    auto it = g.sprites.find(a[2]);
    const int32_t lw = it != g.sprites.end() ? it->second.w : 0;   // the Surface's +0x10 (0 after Surface_ctor / free)
    const int32_t x = mode() == 1 ? 0x32 : sub32(0x320, lw) / 2;
    const uint32_t b[5] = {a[2], root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0};
    call_body(g, ctx, "Surface_drawRegion", b, 5);
    return;
  }
  gfxp::draw_text_run(g, ctx, root, s, 0, 0x32, y, "showCreditsScreen.line");
}

void b_free(Gfx &g, Ctx &, const uint32_t *, int) { st(g).lines.clear(); }

const GfxBodyDef kBodies[] = {
    {"showCreditsScreen.logo", b_logo},
    {"showCreditsScreen.lines", b_lines},
    {"showCreditsScreen.line", b_line},
    {"showCreditsScreen.free", b_free},
};

}  // namespace

extern const GfxFamily kGfxFamily_credits = {"credits", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
