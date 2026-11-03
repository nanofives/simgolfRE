// The live run core (roadmap S7.1, 2026-10-24; notes log/roadmap/S7/1_notes.md, module `live`, port/src/sim/live.cpp).
//
// WHAT IT IS. sg_run_boot (portsim_c.h) runs a whole recorded session: tick.cpp's parse_trace reads a schema v1 trace
// (re/model/trace_schema.md) ahead into per-frame lists and TraceInput hands each recorded dispatch to the pump call that
// made it. A playable host cannot know its input ahead, so run_live runs the same boot and frame steps (tick_frame_io) and
// asks a LiveHost for every outside value AT THE MOMENT THE PORT READS IT: the frame-head clock, each clock read the port
// makes (H1's g_frameMs read, the wall-time loops' reads, seedFromTime), each cursor read (Display::getCursor through
// TkBase::cursor_read and InputSource::cursor), each GetAsyncKeyState read outside the root handlers, and each pump call
// (InputSource::batch) with its PumpKind. The sim runs on the caller's thread and blocks inside the host's callbacks; it
// has no time limit of its own (real-time pacing is the host's, roadmap S7.3).
//
// RECORDING. Every answer is written, as it is given, into a schema v1 trace that sg_run_boot replays to the same state in
// every frame record (gate (b) of re/roadmap/S7_steps.tsv): CLOCKRUN / FRAME / CURSOR / KEYSTATE / HANDLER / DELIVER w32
// records with positions, plus the clock records the replay's dispatch rules read (tick.cpp TraceInput): a dispatch of the
// frame body follows a frame-head or Palette::update QPC record, a modal one any other clock site, a paced modal loop's
// dispatch has its pass read in front of it (PACE). The run also returns per frame record the frame-head T and the 203
// sim-class hashes, in the order of log/p2/p24/<scenario>/c_a/framehash.bin (record 0 = the state at the first frame head).
//
// REPLAY RULES THE HOST MUST KEEP (tick.cpp TraceInput::batch; LivePump says where each holds):
//   - a body pump (P6 / H1) takes the frame's next body dispatch at the first call whose cursor-read count reaches the
//     dispatch's: a batch given at a call whose count equals an earlier declined body call's (since the last dispatch) would
//     replay there (may_deliver false);
//   - a Blocking pump of a modal loop that reads no clock pace (no InputSource::pace) takes the next modal dispatch at its
//     first call with the count, or, through TraceInput's `stuck` rule, at the second call with a count once refused
//     there. IDLE PASSES (roadmap S7.6, U-1601): the core simulates that rule over the loop's calls since the last
//     dispatch. A call may decline (idle_ok) while a later call at the same cursor count could still take a dispatch: a
//     loop that reads the cursor between its pumps (the title menu's hover) declines pass after pass, each pass recorded
//     by its cursor read; a loop that does not (the count repeats) declines once and must deliver at its next call
//     (must_deliver), where the dispatch is recorded with one CURSOR record in front of it that the next cursor read of the
//     port fills (the replay's refusal at the first call, the `stuck` delivery at the second). A paced loop (pumpAndRedraw,
//     the credits) may decline pass after pass;
//   - Drain pumps of a modal loop (pumpLight, doAllTasks outside the frame body) never take a dispatch: the host is
//     not asked.
// A delivery the replay would place at another pump call is still applied and counted in LiveResult::violations (its
// recording no longer replays to the same state from there).
#ifndef PORTSIM_SIM_LIVE_H
#define PORTSIM_SIM_LIVE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "portsim/registry.h"
#include "portsim/sim/commands.h"

namespace portsim {

// One pump call the port makes (InputSource::batch), as the host sees it.
struct LivePump {
  PumpKind kind = PumpKind::Drain;
  bool body = false;          // a pump of the frame body (FrameIo::pump_site P6 / H1): the batch replays as a body dispatch
  bool may_deliver = true;    // false: a batch given here replays at an earlier call (applied, counted as a violation)
  bool must_deliver = false;  // true: a decline leaves this loop no replayable delivery (a blocking host is asked again)
  uint32_t frame = 0;         // the live frame: 0 = the boot (process start .. the first frame head), then 1, 2, ...
  uint32_t cursor_reads = 0;  // the cursor reads the port made in this frame before this call
  int32_t pace_site = 0;      // the clock site of the paced loop running (InputSource::pace), 0 = none
  uint32_t calls = 0;         // pump calls of this class since the last delivery, this one included
  // roadmap S7.6: a decline here replays as an idle pass of a modal loop (a Blocking, unpaced modal call that is not
  // must_deliver); a playing host declines here when the player gave nothing but cursor movement or time
  bool idle_ok = false;
  // P5A: the original's pass period of this modal loop (InputSource::timed_pass), 0 = none; set only on an idle_ok call. A
  // playing host declines such a call once per period while nothing happens (the loop's timer passes)
  uint32_t timed_pass_ms = 0;
};

// Roadmap S7.6: a host marks a point of the session (a present) with LiveMarker::mark; LiveResult::mark_pos gives its
// snapshot position in the recording: a replay of the recording with gfx.snapshotAt(mark_pos[k]) copies the back buffer as it
// was at the mark (the core writes a clock record after each modal Blocking pump call and after H1's frame wait, so the next
// trace item past the mark lies past that position).
class LiveMarker {
 public:
  virtual ~LiveMarker() = default;
  virtual uint32_t mark() = 0;  // the mark's index (0, 1, ...)
};

enum class LiveReply { Decline = 0, Deliver = 1, Stop = 2 };

// Memory writes a TEST host makes (a corpus trace's DELIVER w32, the harness's fixture writes): the host stores them and
// reports each here, the core records it. A playing host makes none.
class LiveWriteLog {
 public:
  virtual ~LiveWriteLog() = default;
  virtual void wrote(uint32_t va, uint32_t value) = 0;
  // roadmap S3.30: a function the test host's trace says the harness called (DELIVER call: the original's address, argc and
  // the int arguments), run by the host at that point; the core records it
  virtual void called(uint32_t addr, uint32_t argc, uint32_t a0, uint32_t a1) {
    (void)addr;
    (void)argc;
    (void)a0;
    (void)a1;
  }
};

// What the frame loop asks. Every call happens on the thread that called run_live, in the port's read order.
class LiveHost {
 public:
  virtual ~LiveHost() = default;
  // the boot step starts (live frame 0: the CRT's static construction, gameMain's part, Setup, the title menu)
  virtual void boot(Ctx &) {}
  // the head of live frame `frame` (1, 2, ...): its timeGetTime value; false ends the session here (no record for it).
  // Called after the previous frame ended and its record's hashes were taken.
  virtual bool frame_head(Ctx &, uint32_t frame, uint32_t &t) = 0;
  // the live frame ended (before its record's hashes are taken)
  virtual void frame_end(Ctx &, uint32_t frame) { (void)frame; }
  // a timeGetTime read of the port at `site` (the call's return address as an offset from mainLoop's frame-head return
  // address, tick.cpp's kSite* constants). body = the frame body's own read (TkBase::clock_site: H1's g_frameMs read), which
  // must be answered; otherwise a wall-time loop's InputSource::clock, where false means "no value" (the loop throws
  // NeedInput, the session ends)
  virtual bool clock(Ctx &, int32_t site, bool body, uint32_t &t) = 0;
  // the cursor in root-window client coordinates (Display::getCursor); modal = a modal loop's read (InputSource::cursor)
  virtual void cursor(Ctx &, bool modal, int32_t &x, int32_t &y) = 0;
  // GetAsyncKeyState(vk) as the game reads it outside the root handlers (low 16 bits)
  virtual uint16_t async_key(Ctx &, int vk) = 0;
  // a pump call: Deliver with the batch in `out` (RootHandler commands, in dispatch order), Decline, or Stop
  virtual LiveReply pump(Ctx &, const LivePump &p, std::vector<Command> &out) = 0;
  // InputSource::exhausted: a host with no input left at all (a Blocking pump then throws NeedInput: the session ends)
  virtual bool exhausted() const { return false; }
  // InputSource::pace (the loop's clock site while a paced loop runs, 0 when it leaves)
  virtual void pace(Ctx &, int32_t site) { (void)site; }
  // H1's frame wait (TkBase::wait_point): nothing is dispatched there
  virtual void wait_point(Ctx &) {}
  // roadmap S7.8 (U-1642): the frame code (tick's drops) ran Window::refreshFull, the point where the original's back buffer
  // reaches its window (refreshFull -> Window::refreshRect -> drawTooltip -> Display::present; log/roadmap/S7/8_notes.md).
  // A presenting host shows the back buffer here. Modal screens that draw through their own module (credits, routing map,
  // load browser) do not come here; their pumps do.
  virtual void present_point(Ctx &) {}
  // P6B: a pass of the title menu's hover loop ended (tick_hook.h LiveIo::title_menu_pass): a notification, no answer
  virtual void title_menu(Ctx &) {}
  // whether a must_deliver decline is asked again (a host that waits for its player); false: the decline stands
  virtual bool blocking() const { return true; }
  // the core's write log (set by run_live before boot)
  LiveWriteLog *writes = nullptr;
  // roadmap S7.6: the core's marker (set by run_live before boot)
  LiveMarker *marker = nullptr;
};

struct LiveOptions {
  int32_t max_records = -1;  // stop after this many frame records (-1: until the host stops or the game quits)
  // the recording's meta "frame_ra": every clock site is written as frame_ra + site. 0 writes the sites themselves (a
  // recording with no image address); the harness passes mainLoop's frame-head return address so recordings read like the
  // corpus's (port/test/live.py)
  uint32_t frame_ra = 0;
  std::string scenario;      // the recording's meta "scenario"
  bool record = true;        // build the recording
  // roadmap S7.8 (U-1643): a long session. trace_file non-empty: the recording is written to that file as the session goes
  // (the events of finished frames are encoded and dropped every `spill_events` events; the file gets the same bytes
  // LiveResult::trace would hold, and LiveResult::trace stays empty). on_record: each frame record is handed over as it is
  // made (record index, T, the sim hashes); with keep_records false LiveResult::T / hashes stay empty.
  std::string trace_file;
  uint32_t spill_events = 4096;
  void (*on_record)(void *arg, int32_t record, uint32_t t, const uint64_t *hashes, size_t n) = nullptr;
  void *on_record_arg = nullptr;
  bool keep_records = true;
};

struct LiveResult {
  int32_t records = 0;              // frame records (T + hashes) written
  std::vector<uint32_t> T;          // per record: the next frame head's timeGetTime value
  std::vector<uint64_t> hashes;     // per record: the sim-class object hashes (objects_of("sim") order)
  std::vector<uint8_t> trace;       // the recording (schema v1)
  std::string stop;                 // why the session ended: "host", "max_records", "quit", "need_input", "error"
  std::string detail;
  // deliveries the replay places at another pump call, commands no HANDLER record can carry, and must_deliver declines
  // of a non-blocking host (each makes the recording diverge from the live run there); first = the frame record index
  uint64_t violations = 0;
  int32_t first_violation = -1;
  std::string first_violation_what;
  uint64_t asks = 0, reasks = 0;    // host pump calls, and the repeated ones of must_deliver
  uint64_t clocks = 0, cursors = 0, asyncs = 0, dispatches = 0, writes = 0;
  uint64_t body_calls = 0, modal_calls = 0, paced_calls = 0;  // pump calls the host was asked about, by class
  // roadmap S7.6: idle passes (declines at idle_ok calls), dispatches recorded with a CURSOR record in front of them (a
  // loop that pumped twice at one cursor count), and each LiveMarker::mark's snapshot position (filled when the run ends)
  uint64_t idle_declines = 0, inflated = 0;
  std::vector<uint32_t> mark_pos;
  // roadmap S7.8: with LiveOptions::trace_file, the file's size (0: not written) and the events spilled before the end; the
  // recorder's events held at the end (all of them without a trace_file)
  uint64_t trace_file_bytes = 0, spilled_events = 0, held_events = 0;
};

// Runs the boot and frames from the context's current state (sg_exe_fill_state 1 and the install's files attached, as
// for sg_run_boot) until the host stops, the game quits, the host's input runs out or max_records. Fills `res` as it goes
// (a throw leaves what was recorded so far). Throws like tick_frame (Fault, DivideError, NoReturn, MissingResource).
void run_live(Ctx &ctx, LiveHost &host, const LiveOptions &opt, LiveResult &res);

// A host that answers every request from a schema v1 trace, with tick.cpp's own parse_trace and TraceInput (the
// equivalence gate: the trace as a live host gives what sg_run_boot gives). nullptr + `why` when the trace does not parse.
std::unique_ptr<LiveHost> make_trace_live_host(Ctx &ctx, const uint8_t *trace, size_t n, std::string *why);

// A scripted host (tests, port/test/live.py): input decided at run time from what the port asks and from the state
// (live.cpp, SCRIPT HOST). nullptr + `why` when the script does not parse.
std::unique_ptr<LiveHost> make_script_live_host(const std::string &script, std::string *why);

// roadmap S7.7: a corpus trace's answers until the first dispatch it would give that holds a focus-child key (RootHandler
// 12 / 13), a script's from that pump call on (live.cpp, PREFIX HOST). nullptr + `why` when either does not parse.
std::unique_ptr<LiveHost> make_prefix_live_host(Ctx &ctx, const uint8_t *trace, size_t n, const std::string &script,
                                                std::string *why);

}  // namespace portsim

#endif  // PORTSIM_SIM_LIVE_H
