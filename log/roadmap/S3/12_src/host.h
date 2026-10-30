// The live session of the Windows host (roadmap S7.6): the simulation on its own thread through the live run core
// (portsim/sim/live.h, roadmap S7.1), fed by the window's real input (window.h, S7.2's translator), paced in real time
// (pacing.h) and presented where the original presents (S7.8: every Window::refreshFull of the frame code, the original's
// refreshFull -> refreshRect -> drawTooltip -> Display::present), then at every frame wait and every modal pump call whose
// back buffer changed since (present.h).
//
// THREADS. The window thread owns the window and its message loop; every input message reaches LiveSession::on_input
// (AppWindow's sink), which queues the message's root-handler commands and wakes the sim thread. The sim thread runs
// portsim::run_live with this object as the LiveHost: it blocks inside the host's callbacks (a pump with no input, H1's
// frame wait) and never touches the window except through PostMessageW (a new frame to present, the session's end).
//
// WHAT EACH CALLBACK ANSWERS (live.h):
//   frame_head      timeGetTime; false once the window asked the session to stop (the session ends at a frame head)
//   clock           timeGetTime (the frame body's g_frameMs read, the wall-time loops, seedFromTime)
//   cursor          the cursor in logical coordinates: GetCursorPos + ScreenToClient through the window's viewport
//                   (CursorSource::System, the original's Display::getCursor), or the last mouse message's point
//                   (CursorSource::Message: what a driver that posts messages without moving the system cursor wants)
//   async_key       GetAsyncKeyState
//   pump            a body pump (P6 / H1, never blocks): every queued command as one dispatch when the core allows a
//                   delivery there, else a decline. A modal Blocking pump: present when the back buffer changed, then
//                   deliver what is queued; with nothing queued an idle_ok call (live.h, U-1601) waits for input, a cursor
//                   move or `idle_ms`, and declines (an idle pass: the loop redraws what the cursor points at), and a
//                   must_deliver call waits for input. Passes are at least `pass_ms` apart (the original's waitTicks(1, 0)
//                   per pass of its menu loops). A paced loop (pumpAndRedraw) declines every `pass_ms` while nothing is
//                   queued.
//   wait_point      present, then the original's H1 frame wait in real time (pacing.h frame_wait over timeGetTime: the
//                   frame head + 87 ms, + 125 with g_flags bit 17, in waitTicks(1, 0) steps); nothing is dispatched
//
// RECORDING. run_live records every answer (the session's schema v1 trace). At the end the host writes, in the session
// directory: trace.trace, framehash.bin / .json (the captures' layout: u32 T, u32 record, u64 sgh64 x n sim objects),
// presents.tsv (one row per present: index, mark, snapshot position in the recording, kind, live frame, record count, sgh64
// of the presented RGB555 frame), frames.tsv (written live: one row per frame head with its timeGetTime value), waits.tsv
// (one row per frame wait: the frame's work, the wait's first / last read, waitTicks steps, target; U-1655) and the
// session's counters (LiveSession::summary_json).
#ifndef PORTAPP_HOST_H
#define PORTAPP_HOST_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "input_map.h"
#include "window.h"

#include "portsim/registry.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/live.h"

namespace portsim {
struct LiveTerrainView;
class LiveTerrain;
}

namespace portapp {

// S7.8 (U-1643): LiveTerrain logs one text line per Terrain call (terrain_live.h: for the comparison with a recording of the
// original) and never drops one, so a play session's memory grows with every call. The app reads the log only for its length,
// so at the first frame end the lines are dropped and LiveTerrain::set_keep_log(false) stops the log (roadmap S3.12, U-1673:
// the option log/roadmap/S7/8_merge.md section 3 requested); calls() is LiveTerrain::calls(), the session's total.
struct TerrainLogBound {
  portsim::LiveTerrain *live = nullptr;
  bool keep = false;          // negative control (--negctl terrain-log-keep): never drop the lines
  uint64_t dropped = 0;       // lines dropped so far
  uint64_t max_len = 0;       // the most lines the log held at a frame end
  void frame_end();
  uint64_t calls() const;     // every Terrain call since the LiveTerrain was made, logged or not
  uint64_t length() const;    // lines held now
};

// S7.8 (U-1643): a session's records go to disk as they are made: the live core spills the recording into
// <dir>/trace.trace (LiveOptions::trace_file) and every frame record is appended to <dir>/framehash.bin (LiveOptions::on_record,
// the captures' layout: u32 T, u32 record, u64 sgh64 x sim objects); `in_memory` (HostOptions) keeps S7.5's way, everything in
// LiveResult and written at the end
struct RecordStreams {
  bool on = false;
  std::FILE *fh = nullptr;
  bool open(const std::string &dir, portsim::LiveOptions &o);
  void close();
  static void on_record(void *self, int32_t record, uint32_t t, const uint64_t *hashes, size_t n);
};
std::vector<uint8_t> framehash_bytes(const portsim::LiveResult &r);  // the in-memory records in framehash.bin's layout

// S7.8: the process's memory (GetProcessMemoryInfo: working set, peak working set, private bytes), sampled at the session's
// start, every `every` live frames and at its end (memory.tsv)
struct MemSample {
  uint32_t frame;
  uint64_t working_set, peak_working_set, private_bytes, terrain_log;
};
bool process_memory(MemSample &m);
struct MemSampler {
  uint32_t every = 1000;
  std::vector<MemSample> samples;
  void sample(uint32_t frame, uint64_t terrain_log);  // always records
  void maybe(uint32_t frame, uint64_t terrain_log) {
    if (every && frame % every == 0) sample(frame, terrain_log);
  }
  std::string tsv() const;
  std::string json() const;  // {"start":{..},"end":{..},"samples":n}
};

enum class CursorSource : uint8_t { System = 0, Message = 1 };

// Deliberately wrong behaviours for the negative controls (main.cpp --negctl).
enum class HostDefect : uint8_t {
  None = 0,
  PacingOff = 1,     // the frame wait does not wait
  PresentLate = 2,   // each present shows the previous present's frame (a stale buffer)
  NoPresentPoint = 3,  // S7.8: no present at Window::refreshFull (S7.5's points only: frame wait and modal pumps)
};

struct HostOptions {
  uint32_t idle_ms = 200;    // an idle pass of a modal loop at least this often while nothing happens (0: only on a cursor move)
  uint32_t pass_ms = 10;     // the shortest time between two passes of a modal loop
  CursorSource cursor = CursorSource::System;
  HostDefect defect = HostDefect::None;
  int32_t flag17_from = -1;  // test only: g_flags bit 17 set at this live frame's head (a recorded harness write), -1 never
  int32_t flag17_to = -1;    // and cleared at this one
  int32_t max_records = -1;  // end the session after this many frame records (-1: when the window closes or the game quits)
  // roadmap S7.4 (recordings the original can be driven with, log/roadmap/S7/4_notes.md):
  int64_t clock_origin = -1;      // >= 0: every clock answer is this value + the ms since the first answer (the first answer is
                                  // exactly this; the original's first main-thread timeGetTime is seedFromTime's), else timeGetTime
  bool keys_from_messages = false;  // GetKeyState / GetAsyncKeyState answered from the key messages the window received
                                    // (down 0x8000 / 0x8001, as the shim's SetVirtualKey forced-down answers), not the keyboard
  bool in_memory = false;           // S7.8: keep the recording and the frame records in memory until the end (RecordStreams)
};

class LiveSession : public portsim::LiveHost {
 public:
  LiveSession(HostOptions opt, const std::string &record_dir);
  ~LiveSession() override;
  LiveSession(const LiveSession &) = delete;
  LiveSession &operator=(const LiveSession &) = delete;

  // ---- window thread
  void set_window(void *hwnd, const Viewport &vp);  // the window to post presents to and to map the cursor through
  void set_viewport(const Viewport &vp);
  void on_input(const InputEvent &e);               // AppWindow's sink
  void request_stop();                              // the window is closing: end the session at the next frame head or pump
  // the newest presented frame (WM_APP + 2); false when none is waiting
  bool take_frame(std::vector<uint16_t> &px, uint32_t &index);
  bool focus_child_takes(uint32_t msg, uint32_t wp);  // U-1616, U-1650
  // S7.4: every input message before the translator reads anything for it (the message-key table), and the key state the
  // translator and async_key read in keys_from_messages mode (-1: ask Windows)
  void before_translate(uint32_t msg, uint32_t wp);
  int32_t key_state(uint32_t vk, bool async) const;

  // ---- sim thread: the whole session (boot, title menu, frames) until the window stops it or the game quits
  struct Result {
    int code = 0;          // 0 ok, < 0 a portsim exception (as main.cpp's run_cpp codes)
    std::string detail;
    portsim::LiveResult live;
  };
  Result run(portsim::Ctx &ctx);
  std::string summary_json(const Result &r) const;

  // ---- portsim::LiveHost
  bool frame_head(portsim::Ctx &, uint32_t frame, uint32_t &t) override;
  bool clock(portsim::Ctx &, int32_t site, bool body, uint32_t &t) override;
  void cursor(portsim::Ctx &, bool modal, int32_t &x, int32_t &y) override;
  uint16_t async_key(portsim::Ctx &, int vk) override;
  portsim::LiveReply pump(portsim::Ctx &, const portsim::LivePump &p, std::vector<portsim::Command> &out) override;
  void wait_point(portsim::Ctx &) override;
  void present_point(portsim::Ctx &) override;  // S7.8: present where the original's Display::present runs ('r')
  void frame_end(portsim::Ctx &, uint32_t frame) override;  // S7.8: the terrain log bound and the memory samples
  bool blocking() const override { return true; }
  TerrainLogBound *terrain_log = nullptr;  // S7.8 (main.cpp sets it with the LiveTerrain; read only inside run)
  MemSampler memory;                        // S7.8

  // S7.5: the attached LiveTerrain's drawing counters (terrain passes rasterised, pixels written), written cumulatively in
  // waits.tsv columns 7 / 8 so the frame work can be read against the terrain drawing it did; nullptr = headless terrain
  const portsim::LiveTerrainView *terrain_view = nullptr;

  // window message ids the session posts
  static constexpr uint32_t kMsgPresent = 0x8000 + 2;  // WM_APP + 2: a frame is waiting (take_frame)
  static constexpr uint32_t kMsgEnded = 0x8000 + 3;    // WM_APP + 3: the session ended (the game quit or it failed)

 private:
  struct Present {
    uint32_t index, mark, frame, records;
    char kind;  // 'r' Window::refreshFull (the original's present, S7.8), 'w' frame wait, 'm' modal pump, 'p' paced pump
    uint64_t hash;
  };
  void present(portsim::Ctx &ctx, char kind);
  bool stopping() const { return stop_.load(); }
  // S7.8: present opportunities (calls of present) and presents by kind, and 'r' presents that replaced an earlier 'r' present
  // before any frame wait or modal pump came (frames S7.5's points would not have shown)
  uint64_t opportunities_[4] = {}, made_[4] = {}, r_superseded_ = 0;
  bool r_pending_ = false;
  // S7.8: whether the frame wait must look at the back buffer: set at the frame head and by every modal callback (a modal pump,
  // clock or cursor read), cleared by an 'r' present; w_skipped_ counts the waits that did not look
  bool w_check_ = true;
  uint64_t w_skipped_ = 0;
  // S7.8: terrain_log's counters as run left them (-1: no LiveTerrain), for summary_json after the terrain is gone
  int64_t tlog_calls_ = -1, tlog_max_ = -1, tlog_end_ = -1;
  bool tlog_kept_ = false;

  HostOptions opt_;
  std::string rec_;
  void *hwnd_ = nullptr;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<portsim::Command> queue_;
  uint64_t events_ = 0;              // input messages seen (cursor moves included)
  uint64_t events_seen_ = 0;         // events_ when the last modal pass began: a newer one is a reason for the next pass
  Viewport vp_;
  Point msg_cursor_{0, 0};           // CursorSource::Message: the last mouse message's logical point
  std::atomic<bool> stop_{false};
  // presents
  std::mutex frame_mu_;
  std::vector<uint16_t> frame_slot_, last_presented_, previous_;
  uint32_t frame_index_ = 0;
  bool frame_waiting_ = false;
  std::vector<Present> presents_;
  uint32_t last_dump_ms_ = 0;
  // pacing
  uint32_t head_t_ = 0, last_pass_ms_ = 0;
  uint32_t frame_ = 0;
  uint32_t records_ = 0;
  std::FILE *frames_log_ = nullptr;
  std::FILE *waits_log_ = nullptr;   // waits.tsv: one row per H1 frame wait (frame, work, first / last read, waitTicks, target)
  // counters
  uint64_t idle_passes_ = 0, waits_ = 0, wait_ms_total_ = 0, deliveries_ = 0, delivered_cmds_ = 0, stalls_ = 0;
  bool period_ = false;
  // S7.4: the clock origin (clock_origin), the message-key table and the dispatch log (input_dispatch.tsv, dispatches.tsv)
  uint32_t now();
  bool clk_set_ = false;
  uint32_t clk_off_ = 0;
  std::atomic<uint8_t> keys_[256] = {};
  std::atomic<uint32_t> frame_now_{0};  // frame_ for the window thread (an event's arrival frame)
  struct EventRow {
    uint32_t seq, msg, wp, lp, frame, ncmds;
    int32_t dispatch;
  };
  std::vector<EventRow> event_rows_;
  std::deque<uint32_t> queue_seq_;      // the event of each queued command (parallel to queue_)
  struct DispatchRow {
    uint32_t index, frame, body, body_call, kind, calls, cursor_reads, may, must, ncmds, first_seq, last_seq;
  };
  std::vector<DispatchRow> dispatch_rows_;
  uint32_t body_calls_frame_ = 0, body_calls_seen_frame_ = 0;
  std::vector<std::pair<uint32_t, uint32_t>> body_call_counts_;  // (live frame, body pump calls in it)
};

// sgh64 (portsim/state_hash.h; sg_hash_bytes in Python) of an RGB555 frame's bytes (presents.tsv, shown.tsv)
uint64_t frame_hash(const std::vector<uint16_t> &px);

// Roadmap S7.5: the app's live path over a corpus trace (main.cpp --live-trace). Every answer comes from the inner host
// (portsim::make_trace_live_host: the trace through tick.cpp's own parse_trace / TraceInput), and the presents are made
// where LiveSession makes them: where the frame code runs Window::refreshFull ('r', S7.8), at H1's frame wait ('w') and before
// every modal pump call the host is asked about ('m', 'p' for a paced loop), when the back buffer changed. Each present row also says how many gfx.snapshotAt snapshots existed
// when it was made (`snaps`), so a snapshot taken at a checkpoint can be matched with the present that showed it: the last
// present made before the snapshot appeared. No window, no real time: the trace's clock answers.
class LiveTracePresenter : public portsim::LiveHost {
 public:
  LiveTracePresenter(std::unique_ptr<portsim::LiveHost> inner, uint32_t snapshots_requested)
      : inner_(std::move(inner)), snaps_requested_(snapshots_requested) {}
  struct Present {
    uint32_t index, frame, snaps;
    char kind;
    uint64_t hash;
  };
  const std::vector<Present> &presents() const { return presents_; }
  const std::vector<uint16_t> &last() const { return last_; }

  void boot(portsim::Ctx &ctx) override;
  bool frame_head(portsim::Ctx &ctx, uint32_t frame, uint32_t &t) override;
  void frame_end(portsim::Ctx &ctx, uint32_t frame) override;
  bool clock(portsim::Ctx &ctx, int32_t site, bool body, uint32_t &t) override { return inner_->clock(ctx, site, body, t); }
  void cursor(portsim::Ctx &ctx, bool modal, int32_t &x, int32_t &y) override { inner_->cursor(ctx, modal, x, y); }
  uint16_t async_key(portsim::Ctx &ctx, int vk) override { return inner_->async_key(ctx, vk); }
  portsim::LiveReply pump(portsim::Ctx &ctx, const portsim::LivePump &p, std::vector<portsim::Command> &out) override;
  bool exhausted() const override { return inner_->exhausted(); }
  void pace(portsim::Ctx &ctx, int32_t site) override { inner_->pace(ctx, site); }
  void wait_point(portsim::Ctx &ctx) override;
  void present_point(portsim::Ctx &ctx) override;  // S7.8 ('r'); `no_present_point` = the S7.5 points only (control)
  bool blocking() const override { return inner_->blocking(); }
  bool no_present_point = false;
  TerrainLogBound *terrain_log = nullptr;  // S7.8 (U-1643)
  MemSampler memory;                        // S7.8
  // S7.8 counters (as LiveSession's): presents by kind 'r' 'w' 'm' 'p', and 'r' presents superseded before a w / m / p point
  uint64_t made[4] = {}, r_superseded = 0;

 private:
  void present(portsim::Ctx &ctx, char kind);
  bool r_pending_ = false;
  std::unique_ptr<portsim::LiveHost> inner_;
  uint32_t snaps_requested_ = 0;
  uint32_t frame_ = 0;
  std::vector<uint16_t> last_;
  std::vector<Present> presents_;
};

}  // namespace portapp

#endif
