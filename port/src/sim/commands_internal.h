// Internal helpers of module `commands` (phase 3.7b): typed references into the address space, peer calls by hooks.csv
// name, the shared text-buffer idioms and the modal pump model. Included by commands.cpp and its .inc parts only.
#ifndef PORTSIM_SIM_COMMANDS_INTERNAL_H
#define PORTSIM_SIM_COMMANDS_INTERNAL_H

#include <initializer_list>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/util.h"

namespace portsim {
namespace cm {

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
};
template <class T> inline Ref<T> G(Ctx &ctx, uint32_t va) { return Ref<T>{ctx.mem, va}; }

// a registered port function by hooks.csv name (MissingResource when no module registers it); while a test records the
// peer (commands.recordPeer) the call is logged instead of run
Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args);
// the same, or nothing (eax `none`) when no module registers it: the audio backend (playSound)
Ret call_optional(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args, uint32_t none = 0);

// startMessage: the registered function (phase 3.7a) when there is one, else module events' copy of the same body
// (ev::start_message, which itself prefers the registered one): no third copy here
int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound);
// appendCents: the same arrangement (ev::append_cents)
void append_cents(Ctx &ctx, int32_t n);

// Random::range(n) on g_rng, as the callers keep it: & 0xffff
int32_t rng_range(Ctx &ctx, int32_t n);

// text buffer idioms (the VC6 inline strcpy / strcat)
inline void cpy(Ctx &ctx, uint32_t dst, uint32_t src) { vstrcpy(ctx.mem, dst, src); }
inline void cat(Ctx &ctx, uint32_t dst, uint32_t src) { vstrcat(ctx.mem, dst, src); }

// ---- the modal pump (pumpAndRedraw and what it calls; drawing dropped) ----
// pumpMessages: g_rootInput[7] (the click flag) zero clears the button word; the dispatch point (frameTick483cd0, where
// jgld's pumps deliver window messages) applies the batch the context's InputSource delivers to a pump of this kind
// (commands.h, MODAL BLOCKS); then the click flag is cleared. A Blocking pump with no input left throws NeedInput.
void pump_messages(Ctx &ctx, PumpKind kind, const char *where);
// pumpLight: the button word cleared, then pumpMessages (a Drain) again while a click or a key is left
void pump_light(Ctx &ctx, const char *where);
// flushRedraw: a set g_redrawPending is cleared together with both key words; then (pumpInput483c30 dispatches nothing in
// the port) a non-zero key word sets g_redrawPending and returns 1, else 0
int32_t flush_redraw(Ctx &ctx);
// pumpAndRedraw(0): waits for a click (button word), a key or the network flag; returns 1
int32_t pump_and_redraw(Ctx &ctx, int32_t limit, const char *where);
// the take-and-clear key readers (takeKey: gDirtyFlagB, takeCode: gDirtyFlagA)
int32_t take_key(Ctx &ctx);
int32_t take_code(Ctx &ctx);

// drawWindow (x, y, p3, mode) without its drawing: with no golfer selected (f_w_4c2e00[2] == -1) and mode 0 it
// plays sound 0x10b (the audio backend's: a registered playSound, else nothing); with a golfer (f_w_4c2e00[2] >= 0) the
// thought bubble composes that golfer's sprite (drawThoughtBubble with mode -1 calls composeGolferSprite once, whose
// type-record write-back is a simulation write). The hidden-class writes it makes (g_value587ea0, which mainLoop reads, and
// the line breaks wrapMessageBuffer puts into g_textBuffer) depend on font metrics (Text_put) and are dropped (U-0942).
// Returns 0 (the dropped callees' eax).
int32_t draw_window_effect(Ctx &ctx, int32_t mode);

}  // namespace cm
}  // namespace portsim

#define GS(sym) portsim::state_va(ctx, &portsim::GameState::sym)

#endif  // PORTSIM_SIM_COMMANDS_INTERNAL_H
