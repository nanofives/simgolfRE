// The software 16-bit graphics backend (phase 5, step 1; plan re/PHASE5_GFX_PLAN.md). Module `gfx` (port/src/sim/gfx.cpp),
// tests port/test/test_gfx.py.
//
// The frame code calls every drawing function of the original by its hooks.csv name, and tick.cpp resolves them as drops.
// With a Gfx attached to the context (gfx.attach), tick hands a drop the backend has a body for to gfx_drop: the backend
// draws into its own RGB555 surfaces, keyed by the original objects' 32-bit addresses, and writes nothing into the
// GameState or its address space, so the simulation, its hashes and every segment digest are the same with or without it.
// A call the backend has no body for, or a case of a body it does not model, is recorded (gfx.unsupported) and behaves as
// the drop. The checkpoint frame of the original is jgld's back buffer; the backend's back buffer gets the root surface at
// each Window::refreshFull, and gfx.snapshotAt(pos) copies it when the run first consumes a trace item (a CURSOR read or a
// dispatch) whose clock ordinal is past `pos` (the checkpoint's clock call).
#ifndef PORTSIM_SIM_GFX_H
#define PORTSIM_SIM_GFX_H

#include <atomic>
#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

class Gfx;
void set_gfx(Ctx &ctx, Gfx *g);  // not owned; nullptr detaches
Gfx *gfx(Ctx &ctx);              // or nullptr

// contexts with a Gfx attached (the frame code's fast test before it calls into the backend)
extern std::atomic<int> g_gfx_attached;

// The key of an object a function of the original keeps in its own frame (a local Surface array or Image): no GameState
// address reaches this range, so port code can name such objects to the backend without an address.
constexpr uint32_t gfx_local_key(uint32_t off) { return 0xFFF00000u + off; }
constexpr bool gfx_is_local_key(uint32_t k) { return k >= 0xFFF00000u; }

// tick.cpp: a drop named `name` called with `a[0..n)`; true when the attached backend handled it (the frame still uses the
// drop's result). Never throws.
bool gfx_drop(Ctx &ctx, const char *name, const uint32_t *a, int n);
// tick.cpp: the run consumes a trace item at clock ordinal `pos` (a CURSOR record or a dispatch)
void gfx_progress(Ctx &ctx, uint32_t pos);
// tick.cpp: the run stopped because the recorded input ran out (the menus wait for input the trace no longer holds): every
// snapshot not yet taken is past the last item the run could consume, so the back buffer as it is now is its frame
void gfx_end_of_input(Ctx &ctx);
inline void gfx_progress_fast(Ctx &ctx, uint32_t pos) {
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_progress(ctx, pos);
}

}  // namespace portsim

#endif
