// The view / simulation bridge (see portsim/sim/bridge.h). Phase 3.3, written from the specification (analysis notes,
// hook sources as reading reference, the C3 path-1 runs); no original address or code.
#include "portsim/sim/bridge.h"

#include <initializer_list>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   worldToScreen  1 = rotation 2's y offset + 5 Z instead of - 5 Z, 2 = the second screen test ignores the margin,
//                  3 = TTA_LEVEL_MAX heights read without the - 3
//   playSoundAt    1 = the clamp arm keeps field34, 2 = pitch 0x12d - draw, 3 = the variation counter is not advanced
//   queueMessage   1 = slot wraps at 7, 2 = time 0x31
//   pointsPopup    1 = the money is added, 2 = the disabled bit is 0x2000000
int g_negctl_world_to_screen = 0;
int g_negctl_play_sound_at = 0;
int g_negctl_queue_message = 0;
int g_negctl_points_popup = 0;

Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  const FuncDef *f = registry_at(i, nullptr);
  uint32_t a[16] = {0};
  size_t n = 0;
  for (uint32_t v : args) a[n++] = v;
  Ret r;
  f->fn(ctx, a, r);
  return r;
}
int32_t corner_heights(Ctx &ctx, int32_t x, int32_t y, int32_t k, int32_t f) {
  return i32(call_peer(ctx, "cornerHeights", {u32(x), u32(y), u32(k), u32(f)}).eax);
}
int32_t height_blend(Ctx &ctx, int32_t x, int32_t y) { return i32(call_peer(ctx, "heightBlend", {u32(x), u32(y)}).eax); }
// cornerRange's min (3.1a's function; its out-pointers keep it off the registry): the min of heightBlend at (x, y),
// (x + 1, y), (x + 1, y - 1), (x, y - 1), as cornerRange computes it
int32_t corner_range_min(Ctx &ctx, int32_t x, int32_t y) {
  int32_t mn = height_blend(ctx, x, y);
  const int32_t xs[3] = {add32(x, 1), add32(x, 1), x}, ys[3] = {y, sub32(y, 1), sub32(y, 1)};
  for (int i = 0; i < 3; ++i) {
    const int32_t w = height_blend(ctx, xs[i], ys[i]);
    if (w < mn) mn = w;
  }
  return mn;
}
// playSound is the audio backend's: a registered one is called, otherwise there is no simulation effect (U-0861)
int32_t play_sound(Ctx &ctx, int32_t idx, int32_t volume, int32_t pan, int32_t pitch, int32_t f34) {
  if (registry_find("playSound") >= 0)
    return i32(call_peer(ctx, "playSound", {u32(idx), u32(volume), u32(pan), u32(pitch), u32(f34)}).eax);
  return idx == -1 ? -1 : 0;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ worldToScreen
int32_t world_to_screen(Ctx &ctx, int32_t wx, int32_t wy, int32_t &sx, int32_t &sy, int32_t m) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_world_to_screen);
  const int32_t Z = gs.g_zoomScale;
  int32_t ix = sar32(mul32(sub32(sub32(wx, shl32(gs.gCameraTileX, 10)), 0x200), Z), 7);
  int32_t iy = sar32(mul32(sub32(sub32(wy, shl32(gs.gCameraTileY, 10)), 0x200), Z), 7);
  int32_t W = gs.g_screenWidth;
  if (W == 0x400) {
    ix = mul32(ix, 80) / 64;
    iy = mul32(iy, 52) / 40;
  } else if (W == 0x500) {
    ix = mul32(ix, 104) / 64;
    iy = mul32(iy, 68) / 40;
  }
  const int32_t H = gs.g_screenHeight;
  const int32_t ax = mul32(ix, 20) / 32, ay = mul32(iy, 20) / 32;
  // four separate equality tests, as in the original (the width is re-read after each block)
  if (gs.g_cameraRot == 0) {
    sx = add32(add32(add32(iy, W / 2), mul32(Z, 8)), ix);
    sy = add32(sub32(ay, ax), H / 2);
    W = gs.g_screenWidth;
  }
  if (gs.g_cameraRot == 2) {
    sx = add32(sub32(W / 2, ix), iy);
    const int32_t z5 = mul32(gs.g_zoomScale, 5);
    sy = add32(add32(sub32(mul32(ix, -20) / 32, ay), H / 2), mode == 1 ? z5 : neg32(z5));
    W = gs.g_screenWidth;
  }
  if (gs.g_cameraRot == 4) {
    sx = sub32(sub32(sub32(W / 2, mul32(gs.g_zoomScale, 8)), iy), ix);
    sy = add32(sub32(ax, ay), H / 2);
    W = gs.g_screenWidth;
  }
  if (gs.g_cameraRot == 6) {
    sx = add32(sub32(W / 2, iy), ix);
    sy = add32(add32(add32(ay, ax), H / 2), mul32(gs.g_zoomScale, 5));
    W = gs.g_screenWidth;
  }
  const int32_t nm = neg32(m), half = m / 2;
  if (sx < nm || sx >= add32(W, m)) return 0;
  if (sy < neg32(half) || sy >= add32(half, gs.g_screenHeight)) return 0;

  const int32_t tx = sar32(wx, 10), ty = sar32(wy, 10);
  const int32_t cell = add32(mul32(tx, 50), ty);
  const int8_t type = state_arr(ctx, &GameState::g_tileType)[cell];
  const uint32_t f = (state_arr(ctx, &GameState::g_tileTypeAttrs) + type).field(&TileTypeAttr::flags).get();
  const int32_t S = gs.f_w_4c2e00[0];
  if (!(f & 8)) {
    if (f & 2) {
      const int32_t mn = corner_range_min(ctx, tx, ty);
      sy = add32(sy, neg32(mul32(mul32(sub32(mn, 3), S), gs.g_zoomScale) / 4));
    } else if (f & 4) {
      const int32_t h = state_arr(ctx, &GameState::g_tileLevel)[cell] - (mode == 3 ? 0 : 3);
      sy = sub32(sy, mul32(mul32(h, S), gs.g_zoomScale) / 4);
    } else {
      const int32_t c5 = corner_heights(ctx, tx, ty, 5, 1) - 3;
      const int32_t c7 = corner_heights(ctx, tx, ty, 7, 1) - 3;
      const int32_t c1 = corner_heights(ctx, tx, ty, 1, 1) - 3;
      const int32_t c3 = corner_heights(ctx, tx, ty, 3, 1) - 3;
      int32_t dz;
      if (c5 == c7 && c5 == c1 && c5 == c3) {
        dz = neg32(mul32(mul32(S, gs.g_zoomScale), c5) / 4);
      } else {
        const int32_t dx = sub32(wx, shl32(tx, 10)), dy = sub32(wy, shl32(ty, 10));
        const int32_t b = add32(mul32(add32(mul32(sub32(0x400, dx), c7), mul32(dx, c1)), sub32(0x400, dy)),
                                mul32(add32(mul32(sub32(0x400, dx), c5), mul32(dx, c3)), dy));
        const int32_t scaled = mul32(mul32(b, S), gs.g_zoomScale);
        dz = (neg32(scaled / 1024) / 1024) / 4;
      }
      sy = add32(sy, dz);
    }
  }
  if (gs.unk_005a9cc0 != 0) {
    sx = sub32(mul32(sx, 2), 400);
    sy = sub32(mul32(sy, 2), 300);
  }
  const int32_t m2 = mode == 2 ? 0 : m, h2 = mode == 2 ? 0 : half;
  if (sx < neg32(m2) || sx >= add32(gs.g_screenWidth, m2)) return 0;
  if (sy < neg32(h2) || sy >= add32(h2, gs.g_screenHeight)) return 0;
  return 1;
}

// ------------------------------------------------------------------------------------------------ playSoundAt
int32_t play_sound_at_with(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t field34, const SoundAtCalls &calls) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_play_sound_at);
  int32_t sx = wy, sy = wx;  // worldToScreen writes over the caller's argument slots, which hold wy / wx
  const int32_t on = calls.project(wx, wy, sx, sy, 0);
  int32_t f34 = 0;
  bool clamp_arm = false;
  if (on == 0) {
    if (field34 != -1) return 0;
    clamp_arm = true;
  } else {
    f34 = field34;
    if (f34 == -1) clamp_arm = true;
  }
  if (clamp_arm) {
    sx = clamp(sx, 0, 0x31f);
    sy = clamp(sy, 0, 0x257);
    if (mode != 1) f34 = 0;
  } else {
    int32_t eax = gs.unk_005a9cbc;
    if (gs.unk_005a9cbc != 0) {
      eax = sx;
      sx = sub32(add32(sx, sx), 0x190);
      sy = sub32(add32(sy, sy), 0x190);
    }
    if (sy < 0 || sy > 0x1f4 || sx < 0 || sx >= 0x320) return eax;
  }
  const int32_t base = gs.g_zoomScale >= 4 ? 50 : 10;
  // pan = sx * 127 / 800 - 64 by the original's magic division (0x51eb851f, >> 8 of the high dword, sign bit added)
  const int32_t prod = mul32(sx, 127);
  const int32_t hi = sar32(static_cast<int32_t>((static_cast<int64_t>(prod) * 0x51eb851fLL) >> 32), 8);
  const int32_t pan = sub32(add32(hi, i32(u32(hi) >> 31)), 0x40);
  int32_t volume, pitch;
  if (gs.unk_005a9cd8 == 0) {
    uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
    const int32_t rnd = i32(random_range(seed, 0x258) & 0xffffu);
    pitch = (mode == 2 ? 0x12d : 0x12c) - rnd;
    const int32_t t = sub32(sx, 0x190);
    volume = add32(abs32(t) / 16, base);
  } else {
    const int32_t c = gs.unk_005a9cdc;
    const int8_t b = state_arr(ctx, &GameState::g_soundVariationCycle)[mod32(c, 0x24)];
    pitch = mul32(b - 5, 100);
    if (mode != 3) gs.unk_005a9cdc = add32(c, 1);
    const int32_t h2 = static_cast<int32_t>((static_cast<int64_t>(sy) * 0x2aaaaaabLL) >> 32);  // sy / 6 by multiply
    const int32_t w = sar32(h2, 1);
    volume = add32(add32(w, base), i32(u32(w) >> 31));
  }
  const int32_t rv = calls.play(sound, volume, pan, pitch, f34);
  gs.unk_005a9cd8 = 0;
  return rv;
}

int32_t play_sound_at(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t field34) {
  SoundAtCalls calls;
  calls.project = [&ctx](int32_t x, int32_t y, int32_t &sx, int32_t &sy, int32_t m) { return world_to_screen(ctx, x, y, sx, sy, m); };
  calls.play = [&ctx](int32_t i, int32_t v, int32_t p, int32_t t, int32_t f) { return play_sound(ctx, i, v, p, t, f); };
  return play_sound_at_with(ctx, sound, wx, wy, field34, calls);
}

// ------------------------------------------------------------------------------------------------ the two label rings
void queue_message(Ctx &ctx, int32_t a, int32_t b, int32_t c) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_queue_message);
  const int32_t slot = gs.g_ui_53bba8;
  (state_arr(ctx, &GameState::g_labelQueueC) + slot).set(a);
  (state_arr(ctx, &GameState::g_floatLabelY) + slot).set(b);
  (state_arr(ctx, &GameState::g_labelQueueA) + slot).set(c);
  (state_arr(ctx, &GameState::g_labelQueueB) + slot).set(mode == 2 ? 0x31 : 0x30);
  vstrcpy(ctx.mem, state_va(ctx, &GameState::g_labelQueueText) + u32(mul32(slot, 64)), state_va(ctx, &GameState::g_textBuffer));
  gs.g_ui_53bba8 = mod32(add32(slot, 1), mode == 1 ? 7 : 8);
}

void points_popup(Ctx &ctx, int32_t pts, int32_t x, int32_t y, int32_t who) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_points_popup);
  if (pts == 0 || (gs.g_flags & (mode == 2 ? 0x2000000u : 0x1000000u))) return;
  if (who != -1) {
    const Ptr<int32_t> money = (state_arr(ctx, &GameState::g_holes) + who).field(&HoleRecord::money);
    money.set(mode == 1 ? add32(money.get(), pts) : sub32(money.get(), pts));
  }
  const int32_t slot = gs.g_points59abb0;
  (state_arr(ctx, &GameState::g_moneyLabelX) + slot).set(x);
  (state_arr(ctx, &GameState::g_moneyLabelY) + slot).set(y);
  (state_arr(ctx, &GameState::g_moneyLabelValue) + slot).set(pts);
  (state_arr(ctx, &GameState::g_moneyLabelTime) + slot).set(0x18);
  gs.g_points59abb0 = mod32(add32(slot, 1), 8);
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

// worldToScreen(wx, wy, int *sx, int *sy, margin): the out cells are read first (a rotation outside 0 / 2 / 4 / 6 tests
// them as they are, and the height arm and the doubling still update them) and written back (an untouched cell gets
// its own value again)
void a_world_to_screen(Ctx &c, const uint32_t *a, Ret &r) {
  int32_t sx = c.mem.load<int32_t>(a[2]), sy = c.mem.load<int32_t>(a[3]);
  const int32_t res = world_to_screen(c, S(a, 0), S(a, 1), sx, sy, S(a, 4));
  c.mem.store<int32_t>(a[2], sx);
  c.mem.store<int32_t>(a[3], sy);
  r.set_i32(res);
}
void a_play_sound_at(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(play_sound_at(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3))); }
void a_queue_message(Ctx &c, const uint32_t *a, Ret &) { queue_message(c, S(a, 0), S(a, 1), S(a, 2)); }
void a_points_popup(Ctx &c, const uint32_t *a, Ret &) { points_popup(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3)); }

// Test-only: playSoundAt with the projection and playSound scripted (the C3 runs stood both in). args: sound, wx, wy,
// field34, script (a block: +0 the projection result, +4 write flag, +8 sx, +0xc sy, +0x10 playSound's result; the entry
// logs the projection call at +0x20 {count, wx, wy, m, sx in, sy in} and the playSound call at +0x40 {count, idx, volume,
// pan, pitch, f34}).
void a_play_sound_at_scripted(Ctx &c, const uint32_t *a, Ret &r) {
  const uint32_t blk = a[4];
  const Memory &m = c.mem;
  SoundAtCalls calls;
  calls.project = [&m, blk](int32_t x, int32_t y, int32_t &sx, int32_t &sy, int32_t mg) {
    const uint32_t lg = blk + 0x20;
    m.store<uint32_t>(lg, m.load<uint32_t>(lg) + 1);
    m.store<int32_t>(lg + 4, x);
    m.store<int32_t>(lg + 8, y);
    m.store<int32_t>(lg + 12, mg);
    m.store<int32_t>(lg + 16, sx);
    m.store<int32_t>(lg + 20, sy);
    if (m.load<uint32_t>(blk + 4) != 0) {
      sx = m.load<int32_t>(blk + 8);
      sy = m.load<int32_t>(blk + 12);
    }
    return m.load<int32_t>(blk);
  };
  calls.play = [&m, blk](int32_t i, int32_t v, int32_t p, int32_t t, int32_t f) {
    const uint32_t lg = blk + 0x40;
    m.store<uint32_t>(lg, m.load<uint32_t>(lg) + 1);
    const int32_t vals[5] = {i, v, p, t, f};
    for (int k = 0; k < 5; ++k) m.store<int32_t>(lg + 4 + 4 * u32(k), vals[k]);
    return m.load<int32_t>(blk + 0x10);
  };
  r.set_i32(play_sound_at_with(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3), calls));
}

int g_negctl_none = 0;
const FuncDef kFuncs[] = {
    {"worldToScreen", "default", "int", 5, 0, a_world_to_screen, &g_negctl_world_to_screen},
    {"playSoundAt", "default", "int", 4, 0x027F, a_play_sound_at, &g_negctl_play_sound_at},
    {"queueMessage", "default", "void", 3, 0, a_queue_message, &g_negctl_queue_message},
    {"pointsPopup", "default", "void", 4, 0, a_points_popup, &g_negctl_points_popup},
    {"bridge.playSoundAtScripted", "default", "int", 5, 0x027F, a_play_sound_at_scripted, &g_negctl_none},
};
}  // namespace

extern const ModuleDef kModule_bridge = {"bridge", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
