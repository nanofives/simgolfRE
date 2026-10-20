// Semantic input actions (see portsim/sim/semantic.h). Phase 3.8, stream 38i: written from mainLoop's H1 / H2 code as the
// port transcribes it (port/src/sim/tick_frame.inc: the key switches, the press-code entry and its routing) and from the
// root handlers of commands.cpp; no original address or code.
//
// Test entries (port/test/test_semantic_input.py, port/test/semantic.py):
//   semantic.apply(kind, a0 .. a6)      one semantic command (arg[0..6]); returns apply_command's result
//   semantic.route(x, y, button)        the click route on the current state: target | a << 8 (a in 0..0xffffff)
//   semantic.routeTile(x, y, button)    the route's tile: tile_x | tile_y << 16 (-1, -1 = 0xffffffff)
//   semantic.keyVk(id) / keyCh(id)      a key action's binding (0xffffffff = unknown id)
//   semantic.logOn(on) / logClear()     record every Click / TileClick lowered in this context (off by default)
//   semantic.logCount() / logGet(i, f)  the records: f 0 x, 1 y, 2 button, 3 target, 4 a, 5 b, 6 tile_x, 7 tile_y,
//                                       8 the stated target, 9 the kind
#include "portsim/sim/semantic.h"

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"

namespace portsim {

namespace {
// Negative controls (mode 0 is the correct lowering):
//   1 = a Click's left button is lowered with the right button's handlers (onPress2 / onUnpoint2)
//   2 = a character key is lowered without its WM_CHAR (onChar(vk) alone: H1's takeKey sees no key)
//   3 = the routing check refuses a stated target that holds
//   4 = a TileClick aims at the tile one row down (ty + 1)
int g_negctl_semantic = 0;

struct Binding {
  int32_t id;
  const char *name;
  uint32_t vk, ch;
};
const Binding kKeys[] = {
#define SG_KEY_ROW(id, name, vk, ch) {id, #name, vk, ch},
    SG_SEMANTIC_KEYS(SG_KEY_ROW)
#undef SG_KEY_ROW
};

struct LogRec {
  int32_t v[10];
};
struct Log {
  bool on = false;
  std::vector<LogRec> recs;
};
std::mutex g_log_mu;
std::map<const Ctx *, Log> g_logs;

void log_click(Ctx &ctx, CmdKind kind, int32_t x, int32_t y, int32_t button, int32_t stated, const ClickRoute &r) {
  std::lock_guard<std::mutex> g(g_log_mu);
  auto it = g_logs.find(&ctx);
  if (it == g_logs.end() || !it->second.on) return;
  it->second.recs.push_back({{x, y, button, static_cast<int32_t>(r.target), r.a, r.b, r.tile_x, r.tile_y, stated,
                              static_cast<int32_t>(kind)}});
}

// one root-handler call, as a RootHandler command (commands.cpp root_handler)
int32_t root(Ctx &ctx, uint32_t handler, int32_t a0, int32_t a1, uint32_t ctrl, uint32_t shift, uint32_t t) {
  Command c;
  c.kind = CmdKind::RootHandler;
  c.handler = handler;
  c.a0 = a0;
  c.a1 = a1;
  c.async_ctrl = ctrl;
  c.async_shift = shift;
  c.t = t;
  return apply_command(ctx, c);
}

// The clock value the deadline tails of onPoint / onUnpoint / onKey compare (see semantic.h): unused with the deadline
// word 0; otherwise arg[7], which must be given
uint32_t handler_clock(Ctx &ctx, const Command &cmd, const char *what) {
  if (ctx.gs().g_audio5aa774 == 0) return 0;
  if (cmd.arg[7] != 0) return static_cast<uint32_t>(cmd.arg[7]);
  throw MissingResource{what};
}

// GetAsyncKeyState as the press handlers read it for a held modifier (they keep bits 8..15)
uint32_t held(bool on) { return on ? 0x8000u : 0u; }

int32_t call_i(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  const FuncDef *f = registry_at(i, nullptr);
  uint32_t a[16] = {0};
  size_t n = 0;
  for (uint32_t v : args) a[n++] = v;
  Ret r;
  f->fn(ctx, a, r);
  return static_cast<int32_t>(r.eax);
}

// Runs `fn` on the context and restores the whole GameState afterwards (the tile cache writes of screenToTile /
// tileToScreen, a corner cache, the out words): what fn returns is all that is kept.
template <class F> auto on_copy(Ctx &ctx, F fn) -> decltype(fn()) {
  std::vector<uint8_t> save(ctx.bytes(), ctx.bytes() + sizeof(GameState));
  struct Restore {
    Ctx &c;
    std::vector<uint8_t> &s;
    ~Restore() { std::memcpy(c.bytes(), s.data(), s.size()); }
  } restore{ctx, save};
  return fn();
}

// the tile mainLoop H2's press code gives the click: screenToTile(x, y, ..., 0) with g_zoomScale doubled around it while
// unk_005a9cbc is set (H2 does the same), run on a copy of the state
void click_tile(Ctx &ctx, int32_t x, int32_t y, int32_t &tx, int32_t &ty) {
  auto r = on_copy(ctx, [&]() {
    GameState &gs = ctx.gs();
    if (gs.unk_005a9cbc != 0) gs.g_zoomScale = mul32(gs.g_zoomScale, 2);
    const uint32_t ox = state_va(ctx, &GameState::g_cursorTileX), oy = state_va(ctx, &GameState::g_cursorTileY);
    call_i(ctx, "screenToTile", {static_cast<uint32_t>(x), static_cast<uint32_t>(y), ox, oy, 0u});
    return std::make_pair(gs.g_cursorTileX, gs.g_cursorTileY);
  });
  tx = r.first;
  ty = r.second;
}

// the screen point of a tile's centre as tileToScreen gives it (cache or worldToScreen), on a copy of the state; false
// when the tile is off screen
bool tile_point(Ctx &ctx, int32_t tx, int32_t ty, int32_t &x, int32_t &y) {
  auto r = on_copy(ctx, [&]() {
    GameState &gs = ctx.gs();
    const uint32_t ox = state_va(ctx, &GameState::g_cursorTileX), oy = state_va(ctx, &GameState::g_cursorTileY);
    const int32_t on = call_i(ctx, "tileToScreen", {static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), ox, oy});
    return std::make_tuple(on, gs.g_cursorTileX, gs.g_cursorTileY);
  });
  x = std::get<1>(r);
  y = std::get<2>(r);
  return std::get<0>(r) != 0;
}

int32_t apply_key(Ctx &ctx, const Command &cmd) {
  uint32_t vk = 0, ch = 0;
  const int32_t id = cmd.arg[0];
  if (id == static_cast<int32_t>(KeyAction::Char)) {
    ch = static_cast<uint32_t>(cmd.arg[1]);
    vk = static_cast<uint32_t>(cmd.arg[2]);
  } else if (!key_binding(id, vk, ch)) {
    throw MissingResource{"semantic Key: unknown key action"};
  }
  const uint32_t t = handler_clock(ctx, cmd, "semantic Key: the handlers' deadline is set and arg[7] gives no clock");
  if (vk == 0) {
    // a character delivered with no key-down (WM_CHAR alone): jgld passes the code of its last key-down (arg[3])
    if (ch == 0) throw MissingResource{"semantic Key: neither a key nor a character"};
    return root(ctx, 7, static_cast<int32_t>(ch), cmd.arg[3], 0, 0, t);
  }
  if (ch == 0) return root(ctx, 7, 0, static_cast<int32_t>(vk), 0, 0, t);  // a non-character key: onKey(0, vk)
  root(ctx, 8, static_cast<int32_t>(vk), 0, 0, 0, 0);                    // WM_KEYDOWN: onChar(vk)
  if (SG_NEGCTL(g_negctl_semantic) == 2) return 0;
  return root(ctx, 7, static_cast<int32_t>(ch), static_cast<int32_t>(vk), 0, 0, t);  // WM_CHAR: onKey(ch, vk)
}

int32_t pointer_down(Ctx &ctx, int32_t x, int32_t y, int32_t button, uint32_t t) {
  if (button == 1) return root(ctx, 2, x, y, 0, 0, t);  // WM_LBUTTONDOWN: onPoint
  if (button == 2) return root(ctx, 5, x, y, 0, 0, 0);  // WM_RBUTTONDOWN: onPoint2
  throw MissingResource{"semantic pointer: button must be 1 (left) or 2 (right)"};
}

// the click itself (button up): onPress1 + onUnpoint, or onPress2 + onUnpoint2
int32_t pointer_up(Ctx &ctx, int32_t x, int32_t y, int32_t button, int32_t mods, uint32_t t) {
  const uint32_t ctrl = held((mods & 1) != 0), shift = held((mods & 2) != 0);
  if (button == 1 && SG_NEGCTL(g_negctl_semantic) == 1) button = 2;
  if (button == 1) {
    const int32_t r = root(ctx, 1, x, y, ctrl, shift, 0);
    root(ctx, 3, 0, 0, 0, 0, t);
    return r;
  }
  if (button == 2) {
    const int32_t r = root(ctx, 4, x, y, ctrl, shift, 0);
    root(ctx, 6, 0, 0, 0, 0, 0);
    return r;
  }
  throw MissingResource{"semantic pointer: button must be 1 (left) or 2 (right)"};
}

// a stated target against the route; refuses when it does not hold (target Any states nothing)
void check_target(const Command &cmd, const ClickRoute &r) {
  const ClickTarget want = static_cast<ClickTarget>(cmd.arg[4]);
  if (want == ClickTarget::Any) return;
  bool ok = r.target == want;
  if (ok && (want == ClickTarget::World || want == ClickTarget::Swing) && (cmd.arg[5] != -1 || cmd.arg[6] != -1))
    ok = r.tile_x == cmd.arg[5] && r.tile_y == cmd.arg[6];
  if (ok && want == ClickTarget::Panel) ok = r.a == cmd.arg[5] && r.b == cmd.arg[6];
  if (ok && (want == ClickTarget::Place || want == ClickTarget::GolferAction || want == ClickTarget::TerrainEdit))
    ok = r.a == cmd.arg[5];
  if (SG_NEGCTL(g_negctl_semantic) == 3) ok = !ok;
  if (!ok) throw CommandRefused{{"semantic Click: the stated target does not hold at the dispatch point"}};
}

int32_t apply_click(Ctx &ctx, const Command &cmd) {
  const int32_t x = cmd.arg[0], y = cmd.arg[1], button = cmd.arg[2];
  const ClickRoute r = click_route(ctx, x, y, button);
  log_click(ctx, cmd.kind, x, y, button, cmd.arg[4], r);
  check_target(cmd, r);
  const uint32_t t = handler_clock(ctx, cmd, "semantic Click: the handlers' deadline is set and arg[7] gives no clock");
  return pointer_up(ctx, x, y, button, cmd.arg[3], t);
}

int32_t apply_tile_click(Ctx &ctx, const Command &cmd) {
  const int32_t tx = cmd.arg[0], ty = add32(cmd.arg[1], SG_NEGCTL(g_negctl_semantic) == 4 ? 1 : 0), button = cmd.arg[2];
  int32_t x = 0, y = 0;
  if (!tile_point(ctx, tx, ty, x, y)) throw CommandRefused{{"semantic TileClick: the tile is not on screen"}};
  const ClickRoute r = click_route(ctx, x, y, button);
  log_click(ctx, cmd.kind, x, y, button, static_cast<int32_t>(ClickTarget::World), r);
  if ((r.target != ClickTarget::World && r.target != ClickTarget::Swing) || r.tile_x != tx || r.tile_y != ty)
    throw CommandRefused{{"semantic TileClick: the tile's screen point does not select the tile on the course"}};
  const uint32_t t = handler_clock(ctx, cmd, "semantic TileClick: the handlers' deadline is set and arg[7] gives no clock");
  pointer_down(ctx, x, y, button, t);
  return pointer_up(ctx, x, y, button, cmd.arg[3], t);
}

// ------------------------------------------------------------------------------------------------ test entries
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

void a_apply(Ctx &c, const uint32_t *a, Ret &r) {
  Command cmd;
  cmd.kind = static_cast<CmdKind>(a[0]);
  for (int i = 0; i < 7; ++i) cmd.arg[i] = S(a, i + 1);
  r.set_i32(apply_command(c, cmd));
}
void a_route(Ctx &c, const uint32_t *a, Ret &r) {
  const ClickRoute k = click_route(c, S(a, 0), S(a, 1), S(a, 2));
  r.set_u32(static_cast<uint32_t>(k.target) | (static_cast<uint32_t>(k.a) << 8));
}
void a_route_tile(Ctx &c, const uint32_t *a, Ret &r) {
  const ClickRoute k = click_route(c, S(a, 0), S(a, 1), S(a, 2));
  r.set_u32((static_cast<uint32_t>(k.tile_x) & 0xffffu) | (static_cast<uint32_t>(k.tile_y) << 16));
}
void a_key_vk(Ctx &, const uint32_t *a, Ret &r) {
  uint32_t vk, ch;
  r.set_u32(key_binding(S(a, 0), vk, ch) ? vk : 0xffffffffu);
}
void a_key_ch(Ctx &, const uint32_t *a, Ret &r) {
  uint32_t vk, ch;
  r.set_u32(key_binding(S(a, 0), vk, ch) ? ch : 0xffffffffu);
}
void a_log_on(Ctx &c, const uint32_t *a, Ret &) {
  std::lock_guard<std::mutex> g(g_log_mu);
  g_logs[&c].on = a[0] != 0;
}
void a_log_clear(Ctx &c, const uint32_t *, Ret &) {
  std::lock_guard<std::mutex> g(g_log_mu);
  g_logs[&c].recs.clear();
}
void a_log_count(Ctx &c, const uint32_t *, Ret &r) {
  std::lock_guard<std::mutex> g(g_log_mu);
  const auto it = g_logs.find(&c);
  r.set_u32(it == g_logs.end() ? 0u : static_cast<uint32_t>(it->second.recs.size()));
}
void a_log_get(Ctx &c, const uint32_t *a, Ret &r) {
  std::lock_guard<std::mutex> g(g_log_mu);
  const auto it = g_logs.find(&c);
  if (it == g_logs.end() || a[0] >= it->second.recs.size() || a[1] >= 10) throw MissingResource{"semantic log: no such record"};
  r.set_i32(it->second.recs[a[0]].v[a[1]]);
}

const FuncDef kFuncs[] = {
    {"semantic.apply", "default", "int", 8, 0, a_apply, &g_negctl_semantic},
    {"semantic.route", "default", "uint32", 3, 0, a_route, &g_negctl_semantic},
    {"semantic.routeTile", "default", "uint32", 3, 0, a_route_tile, &g_negctl_semantic},
    {"semantic.keyVk", "default", "uint32", 1, 0, a_key_vk, nullptr},
    {"semantic.keyCh", "default", "uint32", 1, 0, a_key_ch, nullptr},
    {"semantic.logOn", "default", "void", 1, 0, a_log_on, nullptr},
    {"semantic.logClear", "default", "void", 0, 0, a_log_clear, nullptr},
    {"semantic.logCount", "default", "uint32", 0, 0, a_log_count, nullptr},
    {"semantic.logGet", "default", "int", 2, 0, a_log_get, nullptr},
};
}  // namespace

bool key_binding(int32_t id, uint32_t &vk, uint32_t &ch) {
  for (const Binding &b : kKeys)
    if (b.id == id) {
      vk = b.vk;
      ch = b.ch;
      return true;
    }
  return false;
}

bool is_semantic_input(CmdKind k) {
  const uint32_t v = static_cast<uint32_t>(k);
  return v >= 0x200 && v <= 0x2ff;
}

// mainLoop H2's press-code entry, as port/src/sim/tick_frame.inc transcribes it (the label after the report screens,
// where the button word is read): a pending placement first, then a pending golfer-panel action (left button), then the
// press code's routing by the click position: the lower-left button bar, the panel band by panel mode, otherwise the
// course, which g_flags bit 1 turns into a key. Within the course, the player's shot (H2's world entry: panel mode 3, a
// left click above the panel band with no tool word g_ui_5a34e4 and no demolish flag).
ClickRoute click_route(Ctx &ctx, int32_t x, int32_t y, int32_t button) {
  GameState &gs = ctx.gs();
  ClickRoute r;
  click_tile(ctx, x, y, r.tile_x, r.tile_y);
  if (gs.f_w_4c2e00[4] != -1) {  // a41e36c: a button word and a pending placement
    r.target = ClickTarget::Place;
    r.a = gs.f_w_4c2e00[4];
    return r;
  }
  if (button == 1 && gs.g_msgState53df54 > 0 && gs.g_infoPanelTab != -1) {
    r.target = ClickTarget::GolferAction;
    r.a = gs.g_infoPanelTab;
    return r;
  }
  if (y > 0x1b8) {
    if (x < 0xc8) {
      r.target = ClickTarget::Hud;
      return r;
    }
    if (y > 0x1e0) {
      const int32_t mode = gs.g_panelListMode;
      struct P {
        int32_t mode, kind;
        const char *hit;
      };
      static const P kPanels[] = {{0, 0x106, "hitTestSlot432f90"},
                                  {1, 0x104, "hitBuildingTool"},
                                  {2, 0x102, "hitEmployeeSlot435570"},
                                  {3, 0x100, "hitGolferPanel435f00"},
                                  {4, 0x100, "hitGolferPanel435f00"}};
      for (const P &p : kPanels)
        if (p.mode == mode) {
          r.target = ClickTarget::Panel;
          r.a = p.kind;
          r.b = call_i(ctx, p.hit, {static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
          return r;
        }
    }
  }
  if (gs.g_flags & 2) {  // the press code maps the click to a key
    r.target = ClickTarget::TerrainEdit;
    r.a = button == 1 ? 0x3d : (button == 2 ? 0x2d : 0);
    if (gs.g_ui_542f20 == 3) r.a = 0x2f;
    return r;
  }
  r.target = ClickTarget::World;
  if (button == 1 && gs.g_panelListMode == 3 && !(gs.g_flags & (1 << 27)) && gs.g_ui_5a34e4[0] <= 0 && y < 0x1e0)
    r.target = ClickTarget::Swing;
  return r;
}

int32_t semantic_apply(Ctx &ctx, const Command &cmd) {
  // the kind values of commands.h's input actions (numbers here, so this file builds before commands.h names them)
  switch (static_cast<uint32_t>(cmd.kind)) {
    case kKindKey: return apply_key(ctx, cmd);
    case kKindPointerDown:
      return pointer_down(ctx, cmd.arg[0], cmd.arg[1], cmd.arg[2],
                          handler_clock(ctx, cmd, "semantic PointerDown: the handlers' deadline is set and arg[7] gives no clock"));
    case kKindClick: return apply_click(ctx, cmd);
    case kKindTileClick: return apply_tile_click(ctx, cmd);
    default: throw MissingResource{"semantic input kind not implemented"};
  }
}

extern const ModuleDef kModule_semantic = {"semantic", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
