// The frame step (phase 3.7a, 2026-10-19; notes log/p3/p37a_notes.txt, module `tick`, port/src/sim/tick*.cpp).
//
// SPEC. mainLoop's frame (re/analysis/sim/mainLoop_structure.md): the preamble, P1..P7, H1 and H2, in the original's order,
// from one frame head (the timeGetTime call that starts a frame) to the next. Game time is frame-counted
// (re/analysis/sim/time_model.md): one P5 tick and one date unit per unpaused frame. The body is the phase-by-phase
// transcription of the original (the C3 hook source shim/src/re/c3fy.cpp, converted by log/p3/p37a_conv.py): every
// computation that reaches a simulation-class object is kept, including the ones inside the drawing passes (the passes
// leave sprite-cell ids, the itoa buffer, sky actors and golfer look indices in simulation objects); waits, pumps,
// drawing primitives, audio and the display are dropped (tick.cpp lists every dropped callee with the reason).
//
// INPUTS of a frame: the clock values the frame reads (the frame-head value, the H1 read that sets g_frameMs) and the
// player's input as Commands (portsim/sim/commands.h, phase 3.7b): tick_frame applies them at H1's pump point, then runs
// H1's key switch and H2's mouse action, which call the straddler effects by their hooks.csv names. No wall time, no
// waits: pumpAndRedraw(0) and the modal pumps take their input from the context's InputSource (commands.h).
#ifndef PORTSIM_SIM_TICK_H
#define PORTSIM_SIM_TICK_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/commands.h"

namespace portsim {

namespace tk {
struct LiveIo;  // roadmap S7.1 (port/src/sim/tick_hook.h)
}

// What a frame takes from outside the state.
struct FrameInput {
  // roadmap S7.1: a live run (portsim/sim/live.h) answers the frame body's clock, cursor and GetAsyncKeyState reads on demand
  // through this, in place of `clock` / `cursor` / `async_keys` below (nullptr in every trace run)
  tk::LiveIo *live = nullptr;
  uint32_t t_head = 0;       // the frame-head timeGetTime value (stored at frame slot +0x1ec)
  // The main-thread timeGetTime values of the frame by call site (code offset from the frame-head return address, as the
  // trace records sites), in call order: H1's g_frameMs read takes the first value recorded at its site; a site with no
  // value falls back to t_head (the port makes no wait, so no later call has a value of its own).
  std::vector<std::pair<int32_t, uint32_t>> clock;
  // the cursor positions the frame reads (Display::getCursor: GetCursorPos + ScreenToClient), in read order; the position
  // before the frame's first read is `cursor_start`
  std::vector<std::pair<int32_t, int32_t>> cursor;
  std::vector<uint32_t> cursor_pos;  // each cursor read's clock ordinal in the trace (phase 5: gfx snapshots; may be empty)
  int32_t cursor_start_x = 0, cursor_start_y = 0;
  // the frame's raw commands for H1's pump point (pumpMessages), applied in order when the context has no InputSource that
  // delivers them (a trace run's TraceInput gives every recorded dispatch to the pump call that made it instead)
  std::vector<Command> commands;
  // GetAsyncKeyState results the frame body reads outside the root handlers (vk, low 16 bits), in read order (the trace's
  // KEYSTATE records), and each key's value before the frame's first read
  std::vector<std::pair<uint8_t, uint16_t>> async_keys;
  uint16_t async_start[256] = {};
  // called at H1's frame wait (the driver applies the harness's memory writes made inside the wait)
  void (*on_wait)(void *) = nullptr;
  void *on_wait_arg = nullptr;
  bool retile = false;            // the frame was entered through rebuildTileCoverage (the previous frame's exit)
  bool stop_at_wait = false;      // run only up to H1's frame wait (a checkpoint taken inside the wait)
  bool boot = false;              // phase 4: run the boot step (gameMain's part, Setup, the menu) instead of a frame
};

// Where a frame left mainLoop's frame loop.
// Quit (phase 3.8, stream 38j): the title menu's quit returned from mainLoop. tick_frame no longer returns Menu or Retile:
// a frame that leaves for the title menu runs the menu and the session dispatch, and one that leaves through ML+0x664
// runs rebuildTileCoverage, before the next frame head (Frame); or the quit ends mainLoop (Quit).
enum class FrameExit { Frame = 1, Retile = 2, Menu = 3, Wait = 4, Quit = 5 };

// Runs one frame of mainLoop from its frame head to the next. Throws like every port function (Fault, DivideError,
// NoReturn, MissingResource{callee} for a callee that is neither ported nor a documented drop, NeedInput).
FrameExit tick_frame(Ctx &ctx, const FrameInput &in);

// sg_run_frames behind the C ABI: parses a schema-v1 trace slice (re/model/trace_schema.md), runs a frame per FRAME record
// after the first, and fills T / the sim-class hashes per frame head. Returns an SG_E_* code; `detail` says why it stopped.
// boot (phase 4, re/PHASE4_BOOT_PLAN.md): the trace starts at process start and the context holds the exe's initialised data
// (sg_exe_fill_state 1); the records before the first FRAME form a frame 0 that the boot step (gameMain's part, mainLoop's
// Setup, the title menu and Session) consumes, and record 0 of the output is the state at the first frame head.
int tick_run_frames(Ctx &ctx, const uint8_t *trace, size_t trace_len, const uint32_t *queued, int32_t n_queued,
                    int32_t max_frames, uint32_t *out_T, uint64_t *out_hashes, int32_t *frames_done, std::string *detail,
                    bool boot = false);

// The Terrain.dll side a headless run uses when no TerrainApi is attached (tick.cpp HeadlessTerrain: stand-in objects for what the exe
// reads, an operator-new arena, the corner-level steps applyTileElevation waits for, the engine services as no-ops). Exposed for a
// TerrainApi that keeps Terrain.dll's state itself and delegates the engine services (terrain_live.cpp).
class TerrainApi;
std::unique_ptr<TerrainApi> make_headless_terrain(Ctx &ctx);
// roadmap S7.9 (U-1681): drops the blocks make_headless_terrain keeps for this context (call before the context is freed)
void release_headless_terrain(const Ctx &ctx);

// Per-callee call counts of the frames run on this thread since the last reset (development and notes).
std::vector<std::pair<std::string, uint64_t>> tick_callee_counts();
void tick_callee_counts_reset();

}  // namespace portsim

#endif  // PORTSIM_SIM_TICK_H
