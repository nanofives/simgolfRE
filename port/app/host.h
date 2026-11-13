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
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "input_map.h"
#include "window.h"

#include "portsim/registry.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/live.h"
#include "portsim/sim/terrain_live.h"  // P15A: LiveTerrainWorld (WorldJob, the world thread)

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
  ViewWriteUnrecorded = 4,  // G32: the view width written into the view block but not recorded (a replay keeps 800)
};

struct HostOptions {
  uint32_t idle_ms = 200;    // an idle pass of a modal loop at least this often while nothing happens (0: only on a cursor move)
  uint32_t pass_ms = 10;     // the shortest time between two passes of a modal loop
  bool timed_passes = true;  // P5A: a timed modal loop (LivePump::timed_pass_ms) passes on its own period (--no-timed-passes: off)
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
  // P5B (Mariano's play-test, 2026-11-03; log/roadmap/P5/P5B_notes.md). Defaults are the new behaviour; SG_APP_P5B=off (main.cpp)
  // or the per-option switches give the S7 behaviour back.
  // item 1: while the window is not the foreground window, or the system cursor is outside its client area (or over
  // another window), the cursor answer is the last answer given inside, clamped to (1..798, 1..598), so H1's edge scroll
  // (mainLoop ML+0xebb4..ML+0xebe3: x <= 0, y <= 0, x >= 0x31f, y >= 0x257) does not run; GetAsyncKeyState answers 0 then
  bool edge_guard = true;
  // item 3: the window's close button (SC_CLOSE) asks for the game's own quit prompt (LiveSession::request_close)
  bool close_prompt = true;
  // P6B item 2: while a world load runs after the title menu (loadWorldAssets' loadFlic pumps, PumpKind::Load) the window shows
  // P8A: the game-styled `Loading...` banner (gfx body loading.banner: the message window frame and banner font, a progress bar) over the last frame; the game itself presents
  // nothing during that load (its loading screen is drawn only while g_difficulty is -1: the boot, menu.cpp load_flic)
  bool loading_overlay = true;
  // P8B (widescreen stage 2, log/roadmap/P8/P8B_notes.md): each present carries the attached LiveTerrain's world layer
  // (LiveTerrain::world_frame) when it has one; `wide_edge`: H1's edge-scroll cursor read (portsim::kCursorSiteEdgeScroll) is
  // answered 0 / 0x31f only at the client's own left / right edge and clamped to 1..0x31e elsewhere, so a cursor over the wide
  // view's side columns (logical x < 0 or > 799) does not scroll; every other read gets the unclamped logical point
  bool world = false;
  bool wide_edge = false;
  // G32 (the wide view follows the window): with `wide` the logical view width W = 800 * client width / content width (at least
  // 800) is written into the view block (portsim bridge.h kPortViewWidthVa) at the next frame head after it changes, as a recorded
  // harness write (DELIVER w32), and the world layer is resized: world_mode 2 = the client's pixels (native), 1 = W x 600, 0 = kept
  bool wide = false;
  // P22D fix 2 (main.cpp --golfer-list wide, with --wide and the golfer list panel at the left edge): the golfer list columns that
  // fit right of the panel, (W - 800) / 0x79 (at most portsim::kViewListColsMax), go into the view block's word +0x40
  // (kPortViewListColsVa) at the frame head after they change, as a recorded harness write; off: nothing is written (0)
  bool list_columns = false;
  int world_mode = 0;
  const PanelRemap *remap = nullptr;  // G32: the HUD compositor's remap for the system cursor's answers (main.cpp)
  bool text_native = false;  // P9C: presents carry the frame's text runs (gfx.h GfxTextFrame) in WorldImage::text
  // P10B fix 1: the loading banner frame and the frame put back after the load carry the runs of the last present (banner_text)
  bool text_banner = false;
  bool hud_layers = false;  // P9B: presents carry the frame's HUD pixel layers (gfx.h GfxHudFrame; with `world` and `remap`)
  // P9E fix 3: at the delivery of each press (onPress1 / onPress2) the view block's word +4 (bridge.h kPortViewVacatedVa) is set
  // to 1 when the press's message was made in a moved panel's vacated place (InputEvent::vacated), else 0, as a recorded
  // harness write when it changes (the play switch mainLoop.vacatedPanelClick reads it)
  bool vacated_clicks = false;
  // P21B: at each frame head the first queued key message, when it is an arrow key-down (rootWin_onKey(0, 0x25..0x28)) with no
  // button message before it, goes to the view block's words +12 / +16 (bridge.h kPortViewArrowVa) as recorded harness writes
  // instead of the next pump (the play switch mainLoop.arrowStepAfterDraw = 0 runs it before the frame's picture); key-downs of the
  // same arrow right after it (Windows' key repeat) go with it
  bool arrow_head = false;
  // P22A: the play switch mainLoop.arrowKeyHeldRepeat is 0: with clock_origin, the first frame head writes the clock offset
  // (now() - timeGetTime) into view block word +36 (bridge.h kPortViewClockOffVa) as a recorded harness write
  bool arrow_clock = false;
  // P26A (--arrow-glide on, `play` only: needs mainLoop.arrowStepAfterDraw = 0): the arrows drive ArrowGlide (present.h) from
  // their key-downs / key-ups (LiveSession::on_input); their onKey(0, arrow) commands never reach the frame loop's pumps or the
  // frame head (dispatch -3); each frame head writes the glide's camera step (tiles) into view block words +0x60 / +0x64 (bridge.h
  // kPortViewGlideXVa, recorded harness writes); an arrow that starts a glide ends the frame wait at once (wait_point), so the
  // first moved picture is drawn without waiting for the rest of the 87 ms
  ArrowGlide *glide = nullptr;
  double glide_speed = 0;   // tiles / s per camera axis with a 4-tile key-switch step (zoom 1 / 2 / 4, zoom 8 without P11F)
  bool glide_step2_z8 = false;  // P11F (mainLoop.cameraStepFixed4 = 0): the step is 2 tiles at zoom 8, the speed half
  // P9E fix 4: presents carry the tool previews drawn at the cursor apart from the world picture (WorldImage::cur_*), and every
  // cursor answer is handed to the remap (PanelRemap::set_cursor_answer) for the presenter's offset
  bool cursor_previews = false;
  // P10E: presents with those previews also carry the projection snapshot (WorldImage::proj) that lets the window move them to
  // the cursor's tile between game frames (--paint-rate display)
  bool cursor_live = false;
  bool smooth_camera = false;  // P12A fix 1 (--smooth-camera on): the projection snapshot in the picture's coordinates
  bool smooth_objects = false;  // P12A fix 2 (--smooth-objects on): presents carry the queue's draws (WorldImage::hr_scene)
  // P10C fix 1: when the title menu reports its first pass after a session, the frame it presented with the in-game band hint
  // (BandFill::Black: the title's first refreshFull runs before title_menu() clears in_game_) is posted again with Extend
  bool title_bands = false;
  // P13B item 18: the world map shown before a session (courseActionMenu) presents with BandFill::ExtendClamp (its own edge
  // columns repeated) instead of Extend
  bool map_bands = false;
  bool map_repost = true;  // P27C fix 3 (LiveSession::map_repost); off = the negative control p27c-map-band-first
  // P24F fixes 2-3 (log/roadmap/P24/P24F_notes.md): an in-game frame with no terrain picture under its root (the world map, the
  // course map: a screen over the whole root, so no world layer) presents with the bands of the screens before a session
  // (LiveSession::band_hint) instead of black ones
  bool screen_bands = false;
  // P15A (log/roadmap/P15/P15A_notes.md): the world thread. The world image of a present (the hr planes' queue start, flush and
  // post-queue draws, world_cursor, both world_frame calls) is made on a thread of its own from a job the sim thread hands over at
  // the present (portsim gfx.h gfx_set_hr_defer), and the window gets the frame with its own world image when the job is done.
  // The sim thread never waits for it: a job not yet started when the next one comes is merged into it (gfx_hr_job_merge; only the
  // newer present's picture is made). Off (SG_APP_P15=off, --world-thread off): the world image is made at the present, as before.
  bool world_thread = false;
  // P15A fix 3 (log/roadmap/P15/P15A_notes.md section 5): with the world thread, the world layer's raster passes are drawn there too
  // (portsim gfx.h gfx_set_world_raster_defer): the sim thread draws only the world pixels its wide scene samples and the world
  // thread replays the recorded GL stream into the whole world surface. Off (SG_APP_P15=off, --world-raster-thread off): the sim
  // thread draws the whole world surface, as with fix 2.
  bool world_raster_thread = false;
};

// P15A: a present's world work for the world thread (LiveSession::submit_world): the hr ops since the last present, then (capture)
// make_world_image's inputs taken at the present: the HUD frame, the text frame, the projection and camera snapshots, the world
// root and the world's frame-scale planes (scene, its base, s0, s1, tags, T, the box marks), and the frame the window shows with it
struct WorldJob {
  std::shared_ptr<portsim::GfxHrJob> hr;
  bool capture = false;
  uint64_t seq = 0;  // the present's publication number (LiveSession::present_seq_)
  std::vector<uint16_t> px, show;
  uint32_t index = 0;
  int32_t fc_x = -1, fc_y = -1;
  BandFill band = BandFill::Black;
  bool terrain = false, use = false, cursor = false, layered = false, hud_none = false, smooth_objects = false;
  uint8_t dim_all = 0;  // P21C (WorldImage::dim_all)
  uint16_t dim_src = 0;
  portsim::GfxHudFrame f;
  bool text = false;
  portsim::GfxTextFrame tf;
  bool proj_taken = false;
  CursorProj proj;
  CameraSnap cam;
  int64_t t_ns = 0;
  std::shared_ptr<const std::vector<uint16_t>> root;
  portsim::LiveTerrainWorld planes;  // the frame-scale planes and the geometry (no rasteriser, no hr planes)
  std::shared_ptr<std::shared_ptr<const portsim::GfxHrScene>> scene_slot;  // the hr scene, taken by an op of `hr` (P12A fix 2)
  std::vector<portsim::GfxTrailLine> trails;  // P22C fix 3: the frame's trail segments kept off the world layer (gfx_trail_take)
  bool check = false;         // SG_P15_CHECK: check_digest = the image make_world_image gave at the present (host.cpp wi_digest)
  uint64_t check_digest = 0;
  // P15A fix 3: the root is the world thread's own raster after the job's ops (HostOptions::world_raster_thread); with the check
  // root_digest = the sim thread's whole root at the present (FNV-1a of its words)
  bool world_raster = false;
  bool check_root = false;
  uint64_t root_digest = 0;
  // P17B (SG_P17_JOBLOG, development): the submit time, the jobs merged into this one (their pictures dropped), what became of it
  // (0 no capture, 1 published, 2 stale before the image, 3 stale at the publication, 4 no picture: hr planes lost, 5 no picture: the
  // image part failed or gave none), and whether its publication replaced a picture the window had not taken
  int64_t submit_ns = 0;
  int32_t merged_in = 0;
  int32_t outcome = 0;
  bool overwrote = false;
  int64_t ops_ns = 0, image_ns = 0;  // P18B (SG_P17_JOBLOG): run_world_job's hr ops (gfx_hr_job_run) and image part
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
  // P5B item 3: the player pressed the window's close button. On the boot or the title menu the session stops (the window
  // closes); in a game the system menu's Exit row is reproduced: unk_0059b734 = 1 (what showSystemMenu stores before it returns
  // 0x1b, 0x00432af1) written at the next frame head as a recorded write, so H1 (ML+0xf11a) opens the game's quit prompt; when
  // the player then quits to the title menu, the window closes. A second press (before or during the prompt) closes at once.
  // P6B item 1: closing means leaving to Windows the way the game's own exit does. On the title menu the window hides at once
  // and the menu's next pass clicks its exit icon: the pass's cursor read (View_toLocal, ML+0x11a90) is answered with the icon's
  // centre (749, 556) and the button word g_rootInput + 0x10 is set to 1 there as a recorded harness write (DELIVER w32, so
  // the recording replays), so the click test ML+0x11cb1 takes hotspot 5 (selection -2): Session's QUIT path writes top10.sve
  // and Default.pro, the closing movie is skipped (no Bink), the credits end on the button word (an Esc is posted too), and the
  // session ends with stop `quit`. After the in-game prompt (save then quit, or quit) the game's quit save returns to the title menu, whose
  // first pass does the same, so the app closes instead of showing the title menu. Off the title menu's loop (the boot, the
  // location / difficulty screens) the session stops when no title pass came within 0.5 s, as before.
  void request_close();
  // P11D (Mariano's play-test 5 item 4): Ctrl+Z (op 1) / Ctrl+Y (op 2). The op is written into the view block's word +8
  // (bridge.h kPortViewUndoVa) at the next frame head, as a recorded harness write (DELIVER w32), so a replay runs the same undo
  // at the same frame; the sim's play switch mainLoop.courseUndo consumes it at the frame body's click point and clears it.
  void request_undo(int op);
  // P10E: the cursor answer the sim's next cursor read would get (LiveSession::cursor's point, the same remap and logical
  // mapping), with nothing recorded or counted; false when that read would not take the live point (the edge guard's outside /
  // not-foreground answer). dx = the remap's client offset of the point.
  // P27A fix 1: hover = the HUD panels' hover read's answer (kCursorSitePanels: a vacated place answers kVacatedHoverX / Y), for the
  // window's hover looks (P10I hover_tick); false = the course reads' point (the previews)
  bool live_cursor(int32_t &x, int32_t &y, int32_t &dx, bool hover = false);
  // P10E (SG_P10E_LOG): the point of the sim's last cursor read that was not H1's edge-scroll read (site 0: P6_Cursor's
  // View_toLocal among them)
  // P10I fix 2: the 800-space point of the sim's first cursor read since the present before the frame last taken by take_frame
  // (the frame's hover tests read it; its last read when none came; (-1, -1) before any read). Window thread, after take_frame.
  Point frame_cursor() const { return taken_cursor_; }
  // P27C fix 2: the window's shown frame is a flat screen (Presenter::flat_now, set after each take_frame). Then a cursor answer
  // outside the 800 x 600 content (a side band) becomes (-1, -1), a point no flat screen's hit test takes: in the original no
  // point past the screen's edge exists. H1's edge-scroll read (cursor_at) keeps its point.
  void set_flat_screen(bool f) { flat_screen_.store(f); }
  // P27C fix 1: the cursor point of the sim's latest present that left the picture unchanged (present_point returned at an equal
  // frame), once per such present and only while `index` is the index of the frame that picture is (the window's last take_frame):
  // the game read that point and kept the frame's looks. Window thread.
  bool take_hover_confirm(uint32_t index, Point &p);
  Point plain_answer() {
    std::lock_guard<std::mutex> lk(mu_);
    return plain_answer_;
  }
  // P5B item 2: whether the game is in its frame body (the last pump call was P6 / H1), where + and - may be read as the
  // zoom keys; false in every modal loop (text entry, popups, reports, the title menu)
  bool in_frame_body() const { return body_mode_.load(); }
  // P24D: the picture on screen is the title menu's (its latest refreshFull followed title_menu_overlay); the window thread asks
  bool at_title() const { return rpoint_seq_.load() == title_mark_.load() + 1; }
  // the newest presented frame (WM_APP + 2); false when none is waiting. `band` (P7): Extend when the frame is a full-screen
  // 2D screen shown before a game's frame loop runs (boot loading screens, title menu, its sub-screens, the location screen,
  // the world-load screens), Black once the frame loop runs (the course view, and every report or dialog over it)
  bool take_frame(std::vector<uint16_t> &px, uint32_t &index, BandFill *band = nullptr, WorldImage *world = nullptr);
  // P10A: a world image the window is done with, whose buffers the next present fills again (one is kept)
  void recycle_world(WorldImage &&w);
  bool focus_child_takes(uint32_t msg, uint32_t wp);  // U-1616, U-1650
  // S7.4: every input message before the translator reads anything for it (the message-key table), and the key state the
  // translator and async_key read in keys_from_messages mode (-1: ask Windows)
  void before_translate(uint32_t msg, uint32_t wp);
  int32_t key_state(uint32_t vk, bool async) const;
  // P15A: the world thread's counters (session.json p15)
  std::string world_thread_json() const;

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
  void cursor_at(portsim::Ctx &, bool modal, int32_t site, int32_t &x, int32_t &y) override;  // P8B: HostOptions::wide_edge
  uint16_t async_key(portsim::Ctx &, int vk) override;
  portsim::LiveReply pump(portsim::Ctx &, const portsim::LivePump &p, std::vector<portsim::Command> &out) override;
  void wait_point(portsim::Ctx &) override;
  void present_point(portsim::Ctx &) override;  // S7.8: present where the original's Display::present runs ('r')
  void frame_end(portsim::Ctx &, uint32_t frame) override;  // S7.8: the terrain log bound and the memory samples
  void title_menu(portsim::Ctx &) override;  // P6B item 1: a pass of the title menu's hover loop ended
  void title_menu_overlay(portsim::Ctx &) override;  // P24D: the language selector over the title menu (lang_ui.h)
  bool blocking() const override { return true; }
  TerrainLogBound *terrain_log = nullptr;  // S7.8 (main.cpp sets it with the LiveTerrain; read only inside run)
  MemSampler memory;                        // S7.8

  // S7.5: the attached LiveTerrain's drawing counters (terrain passes rasterised, pixels written), written cumulatively in
  // waits.tsv columns 7 / 8 so the frame work can be read against the terrain drawing it did; nullptr = headless terrain
  const portsim::LiveTerrainView *terrain_view = nullptr;

  // window message ids the session posts
  static constexpr uint32_t kMsgPresent = 0x8000 + 2;  // WM_APP + 2: a frame is waiting (take_frame)
  static constexpr uint32_t kMsgEnded = 0x8000 + 3;    // WM_APP + 3: the session ended (the game quit or it failed)
  // P8A: take_frame's index of a banner frame (not a present: no frame hash is logged for it)
  static constexpr uint32_t kBannerIndex = 0xffffffffu;
  // the first world load's expected pump count (later loads use the previous load's count): measured, P6B_notes.md item 2
  static constexpr uint32_t kWorldLoadPumps = 120;  // the Monterey sandbox load (P6B_runs/load_after_*: 120 pumps)

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
  int32_t msg_cursor_dx_ = 0;        // P9E fix 4: the remap's client offset of that message (InputEvent::remap_dx)
  Point msg_cursor_edge_{0, 0};      // P12A fix 1: that point without the camera slide's move (InputEvent::lp_edge)
  Point msg_raw_{0, 0};              // P12A fix 1: the message's client point (InputEvent::lp_raw)
  bool msg_raw_ok_ = false;
  std::atomic<bool> stop_{false};
  // P5B
  std::atomic<bool> body_mode_{false};
  std::atomic<bool> at_menu_{true};   // the boot or the title menu (frame 0, or g_difficulty == -1)
  std::atomic<int> close_req_{0};     // close-button presses the sim thread has not handled
  bool quit_written_ = false;         // unk_0059b734 = 1 written for a close request, not consumed by H1 yet
  bool quit_armed_ = false;           // the prompt consumed it: close at the title menu, unless a frame body runs first
  uint32_t close_written_ = 0, close_direct_ = 0, close_cancelled_ = 0;
  std::atomic<int> undo_req_{0};     // P11D: the undo / redo op the next frame head writes (0 none)
  uint32_t undo_writes_ = 0;         // P11D: undo / redo requests written
  uint32_t arrow_heads_ = 0, arrow_head_events_ = 0;  // P21B: frame heads that took an arrow key-down, and the events they took
  bool close_check(portsim::Ctx &ctx, bool at_head, bool body);  // sim thread: true = stop the session now (item 3)
  // P6B item 1 (request_close): title_seen_ the title menu's loop ran since the last frame head; exit_req_ a close press there
  // (window thread), exiting_ the menu took its exit (sim thread); exit_esc_ Esc keys posted to end the credits
  std::atomic<bool> title_seen_{false}, exit_req_{false};
  std::atomic<bool> flat_screen_{false};  // P27C fix 2 (set_flat_screen)
  uint64_t flat_answers_ = 0;             // P27C fix 2: cursor answers moved off a flat screen's side band (sim thread)
  std::atomic<uint64_t> rpoint_seq_{0}, title_mark_{~0ull - 1};  // P24D: present_point calls; their count at title_menu_overlay
  std::atomic<uint32_t> exit_req_ms_{0};
  bool exiting_ = false, exit_arm_ = false;
  uint32_t exit_begin_ms_ = 0, last_esc_ms_ = 0;
  uint32_t exit_title_ = 0, exit_esc_ = 0, exit_fallback_ = 0;
  void hide_window();
  // the title menu's exit icon: Setup's slots put hotspot 5 at (0x2d1, 0x210), the icon 0x3a x 0x39 (tick.cpp
  // mainLoop.titleExitHotspotWide); its centre is inside the icon (play's rule) and nearest to hotspot 5 (the original's)
  static constexpr int32_t kExitIconX = 0x2d1 + 0x1c, kExitIconY = 0x210 + 0x1c;
  // P6B item 2: world loads (a run of PumpKind::Load pumps): their timing, kept for session.json `loads`
  struct LoadRow {
    uint32_t start_ms, dur_ms, pumps, from_input_ms, decode_ms, decode_calls, frame;
  };
  std::vector<LoadRow> loads_;
  bool loading_ = false, title_ever_ = false;
  uint32_t load_t0_ = 0, load_pumps_ = 0, load_post_ms_ = 0, load_expected_ = 0, session_t0_ = 0;
  uint64_t load_dec_us0_ = 0, load_dec_calls0_ = 0;
  std::atomic<uint32_t> last_input_ms_{0};
  void post_loading(portsim::Ctx &ctx, uint32_t done);
  void end_load(portsim::Ctx &ctx);
  bool banner_shown_ = false;  // P8A: the window shows the banner frame, not the last present
  portsim::GfxTextFrame last_text_;  // P10B fix 1: the text runs published with last_presented_ (empty = none)
  Point last_inside_{400, 300};       // P5B item 1: the last cursor answer given inside the client area
  bool edge_read_ = false;            // P10E: cursor() runs for cursor_at's edge-scroll read (sim thread)
  bool panels_read_ = false;          // P27A fix 1: cursor() runs for cursor_at's HUD panels' hover read (sim thread)
  uint64_t vacated_hover_reads_ = 0;  // P27A fix 1: panels' reads answered kVacatedHoverX / Y (summary_json)
  // P27A fix 1: the panels' hover answer at a vacated place: a point of no HUD control (left of and above the 800 x 600 frame; no
  // panel hover test of the original takes a negative point)
  static constexpr int32_t kVacatedHoverX = -1, kVacatedHoverY = -1;
  bool vacated_raw(int32_t cx, int32_t cy) const { return opt_.remap && opt_.remap->vacated(cx, cy); }
  Point plain_answer_{0, 0};          // P10E: plain_answer()
  // P10I fix 2 (under mu_): the points the sim's last cursor read and its first read since the last present got
  Point last_answer_{-1, -1}, first_answer_{-1, -1};
  bool first_set_ = false;
  void note_answer(const Point &p) {
    last_answer_ = p;
    if (!first_set_) first_answer_ = p;
    first_set_ = true;
  }
  // P7: whether a game's frame loop has run since the title menu (sim thread only): set at a frame head or a body pump with
  // g_difficulty != -1, cleared at any pump that reads g_difficulty == -1 (the title menu stores -1 there, 0x0040f8ab)
  bool in_game_ = false;
  void track_screen(portsim::Ctx &ctx, bool frame_loop);
  uint64_t presents_extend_ = 0, presents_black_ = 0;  // presents by band hint (summary_json)
  // P13B item 18: set at a modal pump of courseActionMenu (the only loop that hands the host a 50 ms timed pass,
  // kWorldMapPassMs), kept through the popups it opens, cleared at a frame head, a world load's pump and the title menu
  bool world_map_ = false;
  uint64_t presents_map_ = 0;
  // P27C fix 3: the world map's first present (made before its first pump set world_map_) posted again with the world map's hint
  void map_repost(portsim::Ctx &ctx);
  uint64_t map_reposts_ = 0;
  uint64_t presents_screen_ = 0;  // P24F fixes 2-3: in-game presents with the screens' bands (band_hint)
  BandFill band_hint(portsim::Ctx &ctx);  // P7, P13B item 18, P24F fixes 2-3: the side-band hint of a present
  BandFill last_band_ = BandFill::Black;  // P10C fix 1: the band hint and index of last_presented_ (sim thread)
  uint32_t last_index_ = 0;
  uint64_t title_reposts_ = 0;
  uint64_t guarded_reads_ = 0;        // cursor reads answered with last_inside_
  uint64_t title_unguarded_ = 0;      // P23C fix 2: reads outside the client / window on the title menu answered as they are
  // presents
  std::mutex frame_mu_;
  std::vector<uint16_t> frame_slot_, last_presented_, previous_;
  uint32_t frame_index_ = 0;
  BandFill frame_band_ = BandFill::Black;  // P7: the band hint of frame_slot_
  Point frame_cursor_{-1, -1}, taken_cursor_{-1, -1};  // P10I fix 2: frame_cursor() (under frame_mu_; taken_ on the window thread)
  Point confirm_cursor_{-1, -1};                       // P27C fix 1: take_hover_confirm (under frame_mu_)
  uint32_t confirm_index_ = 0;
  uint64_t confirm_seq_ = 0, confirm_taken_ = 0;
  WorldImage frame_world_;                 // P8B: the world layer of frame_slot_ (HostOptions::world)
  WorldImage spare_world_;                 // P10A: recycle_world's image (under frame_mu_)
  // P15A: the world thread (HostOptions::world_thread). present_seq_ numbers every publication the sim thread makes or hands to
  // the world thread; shown_seq_ is the newest one in frame_slot_ (both under frame_mu_), so a job finishing after a newer frame
  // was published is not shown. wq_*: the job not yet started (under wq_mu_).
  uint64_t present_seq_ = 0, shown_seq_ = 0;
  std::thread world_thr_;
  std::mutex wq_mu_;
  std::condition_variable wq_cv_;
  std::shared_ptr<WorldJob> wq_pending_;
  bool wq_stop_ = false, wq_busy_ = false;
  std::unique_ptr<portsim::LiveTerrainWorld> w2_;  // the world thread's world: its hr planes, the job's planes moved in
  std::shared_ptr<const std::vector<uint16_t>> last_root_;  // sim thread: the last root copy and its change count
  uint64_t last_root_gen_ = ~0ull;
  struct WorldThreadStats {
    uint64_t jobs = 0, captures = 0, published = 0, stale = 0, merged = 0, merged_captures = 0, root_copies = 0, root_reused = 0;
    int64_t job_ns = 0, job_max_ns = 0, submit_ns = 0;
    uint64_t check_frames = 0, check_diff = 0;  // SG_P15_CHECK
    int64_t copies_skipped = 0, skip_checked = 0, skip_differing = 0;  // fix 2 (the sim's world, at the session's end)
    // fix 3: the root check (world thread), the sim's world at the session's end (mask check, recorder, masked triangles) and the
    // world thread's op counters (gfx_world_run_stats)
    uint64_t root_checks = 0, root_diff = 0;
    int64_t mask_checks = 0, mask_differing = 0, mask_px_differing = 0, rec_calls = 0, rec_array_copies = 0, mask_skipped = 0;
    portsim::GfxWorldRun run;
  } wts_;  // jobs .. job_max_ns: world thread; merged, root_*, submit_ns: sim thread (read after the thread stopped)
  void world_start();
  void world_stop();
  void world_loop();
  void run_world_job(WorldJob &j);
  void submit_world(std::shared_ptr<WorldJob> j);
  // G32: the view the window asks for (window thread, under mu_) and the one applied at a frame head (sim thread)
  int32_t want_w_ = 800, want_pw_ = 0, want_ph_ = 0;
  int32_t applied_w_ = -1, applied_pw_ = 0, applied_ph_ = 0;
  int32_t want_cols_ = 0, applied_cols_ = 0;  // P22D fix 2 (the view block starts at 0)
  uint64_t view_writes_ = 0, world_resizes_ = 0;
  void update_view_target();   // under mu_
  void apply_view(portsim::Ctx &ctx);
  void apply_undo(portsim::Ctx &ctx);  // P11D
  void apply_arrow(portsim::Ctx &ctx);  // P21B
  void apply_arrow_clock(portsim::Ctx &ctx);  // P22A
  void apply_glide(portsim::Ctx &ctx);        // P26A
  size_t drop_glide_arrows();                 // P26A: removes the queued onKey(0, arrow) events (mu_ held)
  std::atomic<bool> glide_wake_{false};       // P26A: an arrow started a glide: the frame wait ends
  int64_t head_ns_ = 0;                       // P26A: the frame head's perf_now_ns (CameraSnap::head_ns)
  uint64_t glide_wakes_ = 0, glide_dropped_ = 0, glide_writes_ = 0;  // P26A (session.json glide)
  bool arrow_clock_written_ = false;          // P22A
  uint64_t presents_world_ = 0, edge_reads_ = 0, edge_clamped_ = 0;  // P8B counters (summary_json)
  uint64_t presents_layered_ = 0;  // P9B: presents with the frame's HUD layers
  uint64_t presents_text_ = 0, text_runs_ = 0;  // P9C: presents with the frame's text runs, and the runs they carried
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
  bool overlay_font_ = false;  // P10A: the overlay's font was handed to the profiler
  uint32_t clk_off_ = 0;
  std::atomic<uint8_t> keys_[256] = {};
  std::atomic<uint32_t> frame_now_{0};  // frame_ for the window thread (an event's arrival frame)
  struct EventRow {
    uint32_t seq, msg, wp, lp, frame, ncmds;
    int32_t dispatch;
  };
  std::vector<EventRow> event_rows_;
  std::deque<uint32_t> queue_seq_;      // the event of each queued command (parallel to queue_)
  std::vector<uint32_t> vacated_seq_;  // P9E fix 3: the queued events marked vacated (InputEvent::vacated)
  uint64_t vacated_writes_ = 0;        // P9E fix 3: view block word +4 writes
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
  void reserve_presents(size_t n) { presents_.reserve(n); }  // P3: the log reserved once (no reallocation in a session)
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
  int32_t view_width = 0;  // G32: --view-width (0 = the view block keeps 800)
  // P8B: called after every present with its index and the back buffer (the world layer through terrain_log->live)
  std::function<void(portsim::Ctx &, uint32_t, const std::vector<uint16_t> &)> on_present;
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
  bool view_written_ = false;
};

// P9B: the world image of a present (P8B's world layer plus, with `layered`, the frame's HUD pixel layers: the world mask is then
// taken over the layers' base, the root before the first panel drew). False when the terrain has no world layer for the frame.
// P15A: make_world_image's world part over a given world and root (the world thread's): world_cursor, world_frame, the preview box,
// the text runs' world_frame and the geometry; `apart` = the previews were taken apart. False when world_frame gave nothing.
bool world_image_part(const portsim::LiveTerrainWorld &w, const std::vector<uint16_t> &root, const std::vector<uint16_t> &px, bool terrain,
                      bool use, const portsim::GfxHudFrame &f, const portsim::GfxTextFrame *text, bool cursor, WorldImage &wi, bool &apart);
// P15A: the frame-scale planes and geometry world_frame / world_cursor read, copied from src (the sim's world) into dst
void copy_world_planes(const portsim::LiveTerrainWorld &src, portsim::LiveTerrainWorld &dst);
void move_world_planes(portsim::LiveTerrainWorld &src, portsim::LiveTerrainWorld &dst);
bool make_world_image(portsim::LiveTerrain &live, portsim::Ctx &ctx, const std::vector<uint16_t> &px, bool layered, WorldImage &wi,
                      const portsim::GfxTextFrame *text = nullptr,
                      bool cursor = false, bool proj = false,  // P10E: proj = WorldImage::proj with the cursor's previews
                      bool pic = false);  // P12A fix 1: proj in the picture's coordinates (take_proj)
// P9C: the text runs published with exactly this back buffer (false when there are none)
bool take_text_frame(portsim::Ctx &ctx, const std::vector<uint16_t> &px, portsim::GfxTextFrame &tf, bool banner = false);
// P10B fix 1: the text frame of a picture `px` made over the frame `under.back` (the loading banner over a copy of the back
// buffer): the picture without `own`'s glyphs (own = the banner's runs over px, may be empty) and, where px still holds under's
// back buffer word, without under's glyphs; under's runs (visible only there) and then own's. False when under does not fit.
bool banner_text(const portsim::GfxTextFrame &under, const portsim::GfxTextFrame &own, const std::vector<uint16_t> &px,
                 portsim::GfxTextFrame &out);

}  // namespace portapp

#endif
