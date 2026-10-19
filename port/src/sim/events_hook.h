// Access helpers for the long phase 3.5 bodies (events_think.cpp, events_news.cpp, events_pair.cpp): typed references into
// the address space, the shared text-buffer string idioms and the callees every builder reaches. Not part of the public API.
// Included by those files only (the macros below would clash elsewhere).
#ifndef PORTSIM_SIM_EVENTS_HOOK_H
#define PORTSIM_SIM_EVENTS_HOOK_H

#include <vector>

#include "events_internal.h"
#include "portsim/ptr.h"
#include "portsim/sim/events.h"
#include "portsim/sim/text.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/util.h"

namespace portsim {
namespace ev {

// A typed reference to an object of the address space: reads load it, assignments store it (a fault on a miss).
template <class T> struct Ref {
  const Memory &m;
  uint32_t va;
  operator T() const { return m.load<T>(va); }
  Ref &operator=(T v) {
    m.store<T>(va, v);
    return *this;
  }
  Ref &operator=(const Ref &o) {
    m.store<T>(va, static_cast<T>(o));
    return *this;
  }
  template <class U> Ref &operator+=(U v) {
    m.store<T>(va, static_cast<T>(m.load<T>(va) + v));
    return *this;
  }
  template <class U> Ref &operator-=(U v) {
    m.store<T>(va, static_cast<T>(m.load<T>(va) - v));
    return *this;
  }
  template <class U> Ref &operator|=(U v) {
    m.store<T>(va, static_cast<T>(m.load<T>(va) | v));
    return *this;
  }
  template <class U> Ref &operator&=(U v) {
    m.store<T>(va, static_cast<T>(m.load<T>(va) & v));
    return *this;
  }
};
template <class T> inline Ref<T> G(Ctx &ctx, uint32_t va) { return Ref<T>{ctx.mem, va}; }

Ptr<Golfer> golfer(Ctx &ctx, int32_t g);
void itoa_into(Ctx &ctx, int32_t v);
std::vector<uint8_t> grab(Ctx &ctx, uint32_t va);   // the string at va with its NUL
void put(Ctx &ctx, uint32_t dst, const std::vector<uint8_t> &bytes);
void replace_with(Ctx &ctx, uint32_t key, const std::vector<uint8_t> &repl);
// a per-hole record field: base + hole * 0x208 (hole records are 0x208 bytes)
template <class T> inline Ref<T> fld(Ctx &ctx, uint32_t base, int32_t hole) { return G<T>(ctx, base + u32(mul32(hole, 0x208))); }
// the CRT's __itoa(v, buf, 10): writes the text at buf and returns buf
uint32_t itoa_to(Ctx &ctx, int32_t v, uint32_t buf, int32_t radix);
inline uint32_t rec_va(Ctx &ctx, int32_t g) { return state_va(ctx, &GameState::g_golfers) + (u32(g) << 8); }
inline void cat(Ctx &ctx, uint32_t dst, uint32_t src) { vstrcat(ctx.mem, dst, src); }
inline void cpy(Ctx &ctx, uint32_t dst, uint32_t src) { vstrcpy(ctx.mem, dst, src); }
inline uint32_t slen(Ctx &ctx, uint32_t s) { return vstrlen(ctx.mem, s); }

// Random::range on the game's generator; the original callers keep the low 16 bits
int32_t rng_range(Ctx &ctx, int32_t n);
// startMessage / appendCents: the straddlers of phase 3.7. The registered function is used when there is one, otherwise a
// local copy of the same body (see events_calls.cpp).
int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound);
void append_cents(Ctx &ctx, int32_t n);
// audio requests: playSound / playSoundAt are the audio backend's (the registered function when there is one)
void play_sound(Ctx &ctx, int32_t idx, int32_t volume, int32_t pan, int32_t pitch, int32_t f34);
void play_sound_at(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t f34);

// the peers' functions, called by their registry names
inline int32_t type_bit7_clear(Ctx &ctx, int32_t g) { return i32(call_peer(ctx, "typeBit7Clear", {u32(g)}).eax); }
inline void build_golfer_name(Ctx &ctx, int32_t g, int32_t flag) { call_peer(ctx, "buildGolferName", {u32(g), u32(flag)}); }
inline void build_status_text(Ctx &ctx, int32_t g, int32_t flag) { call_peer(ctx, "buildStatusText", {u32(g), u32(flag)}); }
inline void decoration_name(Ctx &ctx, int32_t a, int32_t b, int32_t c) { call_peer(ctx, "decorationName", {u32(a), u32(b), u32(c)}); }
inline int32_t hole_quadrants(Ctx &ctx, int32_t x, int32_t y, int32_t n) {
  return i32(call_peer(ctx, "holeQuadrants", {u32(x), u32(y), u32(n)}).eax);
}
inline int32_t golfer_score(Ctx &ctx, int32_t g) { return i32(call_peer(ctx, "golferAge", {u32(g)}).eax); }
inline void append_endearment(Ctx &ctx, int32_t g) { call_peer(ctx, "appendEndearment", {u32(g)}); }
inline void append_hole_name(Ctx &ctx, int32_t hole) { call_peer(ctx, "appendHoleName", {u32(hole)}); }
inline void append_number(Ctx &ctx, int32_t n) { call_peer(ctx, "appendNumber", {u32(n)}); }
inline void landmark_name(Ctx &ctx, int32_t a, int32_t b) { call_peer(ctx, "landmarkName", {u32(a), u32(b)}); }
void log_tick(Ctx &ctx, int32_t a, int32_t b);
// a call into the front end (modal screens) or the audio backend: the registered function when there is one, else nothing
void backend_call(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args);

}  // namespace ev
}  // namespace portsim

using portsim::ev::G;
using portsim::ev::fld;
#define GS(sym) portsim::state_va(ctx, &portsim::GameState::sym)
#define Rec(g) portsim::ev::rec_va(ctx, (g))
#define Rnd(n) portsim::ev::rng_range(ctx, (n))
#define TypeFlags(t) portsim::ev::G<unsigned char>(ctx, GS(g_golferTypes) + 32 + static_cast<unsigned>(t) * 0x230u)
#define Txt(x) (x)
#define Cat(d, s) portsim::ev::cat(ctx, (d), (s))
#define Cpy(d, s) portsim::ev::cpy(ctx, (d), (s))
#define CatAt(a) portsim::ev::cat(ctx, buf, (a))
#define kClamp(v, lo, hi) portsim::clamp((v), (lo), (hi))
#define kSign(v) portsim::sign((v))
#define kTypeBit7Clear(g) portsim::ev::type_bit7_clear(ctx, (g))
#define kBuildGolferName(g, f) portsim::ev::build_golfer_name(ctx, (g), (f))
#define kBuildStatusText(g, f) portsim::ev::build_status_text(ctx, (g), (f))
#define kDecorationName(a, b, c) portsim::ev::decoration_name(ctx, (a), (b), (c))
#define kHoleQuadrants(x, y, n) portsim::ev::hole_quadrants(ctx, (x), (y), (n))
#define kPlaySoundAt(s, x, y, f) portsim::ev::play_sound_at(ctx, (s), (x), (y), (f))
#define kPlaySound(i, v, p, t, f) portsim::ev::play_sound(ctx, (i), (v), (p), (t), (f))
#define kStartMessage(a, p, s) portsim::ev::start_message(ctx, (a), (p), (s))
#define kAppendEndearment(g) portsim::ev::append_endearment(ctx, (g))
#define kAppendHoleName(h) portsim::ev::append_hole_name(ctx, (h))
#define kAppendNumber(n) portsim::ev::append_number(ctx, (n))
#define kAppendCents(n) portsim::ev::append_cents(ctx, (n))
#define kLandmarkName(a, b) portsim::ev::landmark_name(ctx, (a), (b))
#define kLogTick(a, b) portsim::ev::log_tick(ctx, (a), (b))
#define kTileBlocked(x, y) portsim::tile_blocked(ctx, (x), (y))
#define kPostEvent(a, b, c) portsim::post_event(ctx, (a), (b), (c))
#define kAnnounceType(a, b) portsim::announce_hole_type(ctx, (a), (b))
#define kAngleFixed(a, b) portsim::angle_fixed((a), (b))
#define kAppendUpgradeText(i) portsim::append_upgrade_text(ctx, (i))
#define kAppendClubName(c) portsim::append_club_name(ctx, (c))
#define kAppendOpinion(a, b) portsim::append_opinion(ctx, (a), (b))
#define kReplaceInText(k, r) portsim::replace_in_text(ctx, (k), (r))

#endif
