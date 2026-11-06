// The live session of the Windows host (host.h, roadmap S7.6).
#include "host.h"

#include <cstdlib>  // P9B: getenv (SG_P9B_LAYER_LOG)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "pacing.h"
#include "profile.h"  // P10A

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/bridge.h"  // G32: the view block
#include "portsim/sim/flic.h"
#include "portsim/sim/gditext.h"  // P10A: the overlay's font (game_font)
#include "portsim/sim/gfx.h"  // P8A: gfx_drop("loading.banner")  // P8B: gfx_back_terrain  // P6B item 2: sheet_to_gfx_stats
#include "portsim/sim/terrain_live.h"
#include "portsim/sim/util.h"  // P10E: distance (proj_tile)
#include "portsim/sim/perf.h"  // P12A fix 1: perf_now_ns (CameraSnap::t_ns)
#include "portsim/state_hash.h"

namespace fs = std::filesystem;

namespace portapp {

// ------------------------------------------------------------------------------------------------ S7.8: resources (U-1643)
void TerrainLogBound::frame_end() {
  if (!live) return;
  const uint64_t n = live->log_size();   // counted, not formatted (roadmap S3.10b: formatting cost about 1 us per call)
  max_len = std::max(max_len, n);
  if (keep) return;
  // the app reads no line of the log (roadmap S3.12, U-1673): the lines of the boot's first frame are dropped and LiveTerrain
  // records no call after it (set_keep_log); calls() still counts every call
  dropped += n;
  if (n) live->drop_log();
  live->set_keep_log(false);
}
uint64_t TerrainLogBound::calls() const { return live ? live->calls() : 0; }

bool RecordStreams::open(const std::string &dir, portsim::LiveOptions &o) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  fh = std::fopen((fs::path(dir) / "framehash.bin").string().c_str(), "wb");
  if (!fh) return false;
  o.trace_file = (fs::path(dir) / "trace.trace").string();
  o.on_record = &RecordStreams::on_record;
  o.on_record_arg = this;
  o.keep_records = false;
  on = true;
  return true;
}
void RecordStreams::close() {
  if (fh) std::fclose(fh);
  fh = nullptr;
}
void RecordStreams::on_record(void *self, int32_t record, uint32_t t, const uint64_t *hashes, size_t n) {
  RecordStreams &s = *static_cast<RecordStreams *>(self);
  if (!s.fh) return;
  const uint32_t hdr[2] = {t, static_cast<uint32_t>(record)};
  std::fwrite(hdr, 4, 2, s.fh);
  std::fwrite(hashes, 8, n, s.fh);
}
std::vector<uint8_t> framehash_bytes(const portsim::LiveResult &r) {
  const size_t nsim = portsim::objects_of("sim").size();
  std::vector<uint8_t> fh;
  for (int32_t k = 0; k < r.records && static_cast<size_t>(k) < r.T.size(); ++k) {
    const uint32_t hdr[2] = {r.T[static_cast<size_t>(k)], static_cast<uint32_t>(k)};
    fh.insert(fh.end(), reinterpret_cast<const uint8_t *>(hdr), reinterpret_cast<const uint8_t *>(hdr) + 8);
    const uint8_t *h = reinterpret_cast<const uint8_t *>(&r.hashes[static_cast<size_t>(k) * nsim]);
    fh.insert(fh.end(), h, h + nsim * 8);
  }
  return fh;
}
uint64_t TerrainLogBound::length() const { return live ? live->log_size() : 0; }

bool process_memory(MemSample &m) {
  PROCESS_MEMORY_COUNTERS_EX pmc{};
  pmc.cb = sizeof pmc;
  if (!K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof pmc)) return false;
  m.working_set = pmc.WorkingSetSize;
  m.peak_working_set = pmc.PeakWorkingSetSize;
  m.private_bytes = pmc.PrivateUsage;
  return true;
}
void MemSampler::sample(uint32_t frame, uint64_t terrain_log) {
  MemSample m{frame, 0, 0, 0, terrain_log};
  if (process_memory(m)) samples.push_back(m);
}
std::string MemSampler::tsv() const {
  std::string t = "frame\tworking_set\tpeak_working_set\tprivate_bytes\tterrain_log\n";
  for (const MemSample &m : samples)
    t += std::to_string(m.frame) + "\t" + std::to_string(m.working_set) + "\t" + std::to_string(m.peak_working_set) + "\t" +
         std::to_string(m.private_bytes) + "\t" + std::to_string(m.terrain_log) + "\n";
  return t;
}
std::string MemSampler::json() const {
  auto one = [](const MemSample &m) {
    return "{\"frame\":" + std::to_string(m.frame) + ",\"working_set\":" + std::to_string(m.working_set) +
           ",\"peak_working_set\":" + std::to_string(m.peak_working_set) + ",\"private_bytes\":" + std::to_string(m.private_bytes) +
           ",\"terrain_log\":" + std::to_string(m.terrain_log) + "}";
  };
  if (samples.empty()) return "null";
  return "{\"start\":" + one(samples.front()) + ",\"end\":" + one(samples.back()) + ",\"samples\":" +
         std::to_string(samples.size()) + "}";
}

uint64_t frame_hash(const std::vector<uint16_t> &px) {
  return portsim::sgh64(reinterpret_cast<const uint8_t *>(px.data()), px.size() * 2);  // little-endian words = the file bytes
}

namespace {

bool write_file(const fs::path &p, const void *data, size_t n) {
  std::error_code ec;
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  if (n) f.write(static_cast<const char *>(data), static_cast<std::streamsize>(n));
  return static_cast<bool>(f);
}

// The clock of the frame wait: timeGetTime; between two reads it sleeps until the instant the wait needs, waking early only
// when the session is asked to stop (input arriving during the wait is queued: the original dispatches nothing there).
class WaitClock : public PaceClock {
 public:
  WaitClock(std::mutex &mu, std::condition_variable &cv, const std::atomic<bool> &stop) : mu_(mu), cv_(cv), stop_(stop) {}
  uint32_t now() override { return timeGetTime(); }
  void idle_until(uint32_t t) override {
    const int32_t left = static_cast<int32_t>(t - timeGetTime());
    if (left <= 0 || stop_.load()) return;
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_for(lk, std::chrono::milliseconds(left), [&] { return stop_.load(); });
  }

 private:
  std::mutex &mu_;
  std::condition_variable &cv_;
  const std::atomic<bool> &stop_;
};

bool reg_call(portsim::Ctx &ctx, const char *name, uint32_t a0) {
  const int i = portsim::registry_find(name);
  if (i < 0) return false;
  portsim::Ret r;
  try {
    portsim::registry_at(i, nullptr)->fn(ctx, &a0, r);
  } catch (...) {
    return false;
  }
  return true;
}

int run_cpp(portsim::Ctx *ctx, portsim::LiveHost *host, const portsim::LiveOptions *opt, portsim::LiveResult *res,
            std::string *detail) {
  try {
    portsim::run_live(*ctx, *host, *opt, *res);
    return 0;
  } catch (const portsim::Fault &f) {
    *detail = "fault at va " + std::to_string(f.va);
    return -3;
  } catch (const portsim::DivideError &) {
    *detail = "divide error";
    return -9;
  } catch (const portsim::NoReturn &e) {
    *detail = std::string("no return: ") + (e.where ? e.where : "?");
    return -10;
  } catch (const portsim::MissingResource &e) {
    *detail = std::string("missing: ") + (e.what ? e.what : "?");
    return -11;
  } catch (const std::exception &e) {
    *detail = std::string("exception: ") + e.what();
    return -8;
  } catch (...) {
    *detail = "exception";
    return -8;
  }
}
int run_guarded(portsim::Ctx *ctx, portsim::LiveHost *host, const portsim::LiveOptions *opt, portsim::LiveResult *res,
                std::string *detail) {
  __try {
    return run_cpp(ctx, host, opt, res, detail);
  } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO || GetExceptionCode() == EXCEPTION_INT_OVERFLOW
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return -9;
  }
}

std::string jesc(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\', o += c;
    else if (static_cast<unsigned char>(c) < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

}  // namespace

LiveSession::LiveSession(HostOptions opt, const std::string &record_dir) : opt_(opt), rec_(record_dir) {
  session_t0_ = timeGetTime();  // P6B item 2
}

LiveSession::~LiveSession() {
  if (frames_log_) std::fclose(frames_log_);
  if (waits_log_) std::fclose(waits_log_);
}

// ------------------------------------------------------------------------------------------------ window thread
void LiveSession::set_window(void *hwnd, const Viewport &vp) {
  std::lock_guard<std::mutex> lk(mu_);
  hwnd_ = hwnd;
  vp_ = vp;
  update_view_target();
}

void LiveSession::set_viewport(const Viewport &vp) {
  std::lock_guard<std::mutex> lk(mu_);
  vp_ = vp;
  update_view_target();
}

// G32 (HostOptions::wide): the view the client asks for. W = floor(800 * client width / content width), at least 800 (a client
// taller than 4:3 has a letterbox, W 800); the world layer: native = the client's width (the 4:3 content's without wide) by the
// content's height, render scale 1 = W x 600.
void LiveSession::update_view_target() {
  if (!opt_.wide && opt_.world_mode == 0) return;
  int32_t cw = vp_.origin_x * 2 + vp_.client_w;
  if (hwnd_) {
    RECT c;
    if (GetClientRect(static_cast<HWND>(hwnd_), &c)) cw = c.right - c.left;
  }
  if (vp_.client_w <= 0 || vp_.client_h <= 0 || cw <= 0) return;
  int32_t w = static_cast<int32_t>(static_cast<int64_t>(kLogicalW) * cw / vp_.client_w);
  if (!opt_.wide || w < kLogicalW) w = kLogicalW;
  want_w_ = w;
  if (opt_.world_mode == 2) {
    want_pw_ = opt_.wide ? std::max(cw, vp_.client_w) : vp_.client_w;
    want_ph_ = vp_.client_h;
  } else if (opt_.world_mode == 1) {
    want_pw_ = w;
    want_ph_ = kLogicalH;
  }
}

// G32: at a frame head (inside the frame-head call, so the write is one of the frame's head writes, which a replay applies before
// the frame body: tick.cpp TraceInput::begin): the view block's width and the world layer's size, when they changed
void LiveSession::apply_view(portsim::Ctx &ctx) {
  int32_t w, pw, ph;
  {
    std::lock_guard<std::mutex> lk(mu_);
    w = want_w_;
    pw = want_pw_;
    ph = want_ph_;
  }
  if (opt_.wide && w != applied_w_ && portsim::view_map(ctx)) {
    const int32_t now = ctx.mem.load<int32_t>(portsim::kPortViewWidthVa);
    if (now != w) {
      ctx.mem.store<int32_t>(portsim::kPortViewWidthVa, w);
      // negative control (--negctl view-write-unrecorded): the width is stored but not recorded, so a replay keeps 800
      if (writes && opt_.defect != HostDefect::ViewWriteUnrecorded) writes->wrote(portsim::kPortViewWidthVa, static_cast<uint32_t>(w));
      ++view_writes_;
    }
    applied_w_ = w;
  }
  if (pw > 0 && ph > 0 && (pw != applied_pw_ || ph != applied_ph_) && terrain_log && terrain_log->live) {
    const portsim::LiveTerrainWorld *cur = terrain_log->live->world();
    if (cur && (cur->pw != pw || cur->ph != ph) && terrain_log->live->resize_world(pw, ph)) ++world_resizes_;
    applied_pw_ = pw;
    applied_ph_ = ph;
  }
}

// P11D: at a frame head (one of the frame's head writes, so a replay applies it before the same frame body): the pending
// undo / redo op into the view block's word +8
void LiveSession::request_undo(int op) {
  if (op == 1 || op == 2) undo_req_.store(op);
}
void LiveSession::apply_undo(portsim::Ctx &ctx) {
  const int op = undo_req_.exchange(0);
  if (op == 0 || !portsim::view_map(ctx)) return;
  ctx.mem.store<int32_t>(portsim::kPortViewUndoVa, op);
  if (writes) writes->wrote(portsim::kPortViewUndoVa, static_cast<uint32_t>(op));
  ++undo_writes_;
}

void LiveSession::on_input(const InputEvent &e) {
  last_input_ms_.store(timeGetTime());  // P6B item 2
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (const portsim::Command &c : e.commands) {
      queue_.push_back(c);
      queue_seq_.push_back(e.seq);
    }
    if (e.vacated && !e.commands.empty()) vacated_seq_.push_back(e.seq);  // P9E fix 3
    event_rows_.push_back({e.seq, e.msg, e.wp, e.lp, frame_now_.load(), static_cast<uint32_t>(e.commands.size()), -1});
    if (e.msg >= kWmMouseMove && e.msg <= 0x20e) {
      msg_cursor_ = Translator(e.vp).lparam_point(e.lp);
      msg_cursor_dx_ = e.remap_dx;  // P9E fix 4
      msg_cursor_edge_ = e.has_lp_edge ? Translator(e.vp).lparam_point(e.lp_edge) : msg_cursor_;  // P12A fix 1
      msg_raw_ok_ = e.has_lp_edge;
      msg_raw_ = Point{static_cast<int16_t>(e.lp_raw & 0xffff), static_cast<int16_t>(e.lp_raw >> 16)};
    }
    ++events_;
  }
  cv_.notify_all();
}

void LiveSession::request_stop() {
  stop_.store(true);
  cv_.notify_all();
}

// P5B item 3 (host.h). close_req_ stages: 0 none, 1 asked (the next frame head writes the flag), 2 written (H1 has not read
// it), 3 the prompt took it (close at the title menu unless a frame body runs first: the player chose to go on).
void LiveSession::request_close() {
  if (!opt_.close_prompt) {
    request_stop();
    return;
  }
  // a second press (before the prompt came, while it is open, or while the game's exit runs): close at once
  if (close_req_.load() != 0 || exit_req_.load()) {
    request_stop();
    return;
  }
  // P6B item 1: the title menu / the boot: hide the window now; the menu's next pass takes its exit (title_menu), or the
  // session stops after 0.5 s without one (pump)
  if (at_menu_.load()) {
    exit_req_ms_.store(timeGetTime());
    exit_req_.store(true);
    hide_window();
    cv_.notify_all();
    return;
  }
  close_req_.store(1);
  cv_.notify_all();
}

void LiveSession::hide_window() {
  void *h;
  {
    std::lock_guard<std::mutex> lk(mu_);
    h = hwnd_;
  }
  if (h) ShowWindowAsync(static_cast<HWND>(h), SW_HIDE);
}

// P8A: the world load's banner. Built on the sim thread by the gfx backend's `loading.banner` body (the game's own message
// window frame and base banner font over a copy of the back buffer: nothing in the GameState or the real back buffer is
// written) and handed to the window as an ordinary frame with index kBannerIndex. done == 0 puts the frame the load interrupted
// (last_presented_) back on screen.
void LiveSession::post_loading(portsim::Ctx &ctx, uint32_t done) {
  std::vector<uint16_t> px;
  if (done == 0) {
    if (!banner_shown_) return;
    banner_shown_ = false;
    px = last_presented_;
    if (px.empty()) return;
  } else {
    const uint32_t a[2] = {done, load_expected_};
    if (!portsim::gfx_drop(ctx, "loading.banner", a, 2) || !ctx.files_ptr) return;
    const int64_t n = ctx.files().size("$gfx/banner.555");
    if (n != static_cast<int64_t>(kLogicalW) * kLogicalH * 2) return;
    px.resize(static_cast<size_t>(n / 2));
    ctx.files().read("$gfx/banner.555", 0, px.data(), static_cast<uint64_t>(n));
    banner_shown_ = true;
  }
  WorldImage banner_img;  // P9C: the banner's own runs (a frame put back after the load has none)
  if (opt_.text_native && done != 0) take_text_frame(ctx, px, banner_img.text, true);
  // P10B fix 1: and the runs of the frame under the banner (the world map's labels), or the put-back frame's own runs; without
  // them that frame's text was the 800 x 600 frame's scaled (blurred) for the whole load
  if (opt_.text_banner && !last_text_.empty()) {
    portsim::GfxTextFrame merged;
    if (banner_text(last_text_, banner_img.text, px, merged)) banner_img.text = std::move(merged);
  }
  {
    std::lock_guard<std::mutex> lk(frame_mu_);
    frame_world_ = std::move(banner_img);
    frame_slot_ = std::move(px);
    frame_index_ = kBannerIndex;
    frame_band_ = BandFill::Extend;
    frame_waiting_ = true;
  }
  void *h;
  {
    std::lock_guard<std::mutex> lk(mu_);
    h = hwnd_;
  }
  if (h) PostMessageW(static_cast<HWND>(h), kMsgPresent, 0, 0);
}
void LiveSession::end_load(portsim::Ctx &ctx) {
  if (!loading_) return;
  loading_ = false;
  const uint32_t now_ms = timeGetTime();
  uint64_t us = 0, calls = 0;
  portsim::flic::sheet_to_gfx_stats(us, calls);
  const uint32_t li = last_input_ms_.load();
  loads_.push_back({load_t0_ - session_t0_, now_ms - load_t0_, load_pumps_, li && li <= load_t0_ ? load_t0_ - li : 0,
                    static_cast<uint32_t>((us - load_dec_us0_) / 1000), static_cast<uint32_t>(calls - load_dec_calls0_), frame_});
  // the next load's bar is scaled by this one's pump count (the first by the Monterey load's, measured: P6B_notes.md item 2)
  load_expected_ = load_pumps_;
  if (opt_.loading_overlay && title_ever_) post_loading(ctx, 0);
}

// P6B item 1 (host.h request_close)
void LiveSession::title_menu(portsim::Ctx &ctx) {
  title_seen_.store(true);
  title_ever_ = true;  // P6B item 2: loads after this are world loads (no game loading screen)
  at_menu_.store(true);
  // P10C fix 1 (HostOptions::title_bands): back from a session, the title menu's first refreshFull (tick_menu.inc hover loop)
  // presented while in_game_ was still set, so its frame carries the in-game hint (black side bands); the next passes draw the
  // same picture, which present() does not post again, so the bands stayed black until a hover changed it. Post it once more
  // with the title menu's own hint (the frame and its text runs as they are; the title has no world layer)
  if (opt_.title_bands && in_game_ && last_band_ == BandFill::Black && !last_presented_.empty() && !banner_shown_) {
    in_game_ = false;
    WorldImage wi;
    portsim::GfxTextFrame tf;
    if (opt_.text_native && take_text_frame(ctx, last_presented_, tf)) wi.text = std::move(tf);
    {
      std::lock_guard<std::mutex> lk(frame_mu_);
      frame_world_ = std::move(wi);
      frame_slot_ = last_presented_;
      frame_index_ = last_index_;
      frame_band_ = BandFill::Extend;
      frame_waiting_ = true;
      last_band_ = BandFill::Extend;
    }
    ++title_reposts_;
    void *h;
    {
      std::lock_guard<std::mutex> lk(mu_);
      h = hwnd_;
    }
    if (h) PostMessageW(static_cast<HWND>(h), kMsgPresent, 0, 0);
  }
  if (exiting_ || exit_arm_) return;
  // a close press on the title menu, or the in-game prompt's quit: its quit save (H2_QuitSave) came back here. Stage 2
  // counts too: with g_debtYears >= 3 and flag 0x1000000 set, H1's quit goes to the quit save with no prompt (ML+0x11585)
  if (exit_req_.load() || close_req_.load() >= 2) {
    if (!exit_req_.load()) hide_window();
    exit_arm_ = true;  // the next pass's cursor read clicks the exit icon (cursor)
    exit_begin_ms_ = timeGetTime();
    close_req_.store(0);
  }
}

bool LiveSession::close_check(portsim::Ctx &ctx, bool at_head, bool body) {
  if (!opt_.close_prompt) return false;
  const uint32_t quit_va = portsim::state_va(ctx, &portsim::GameState::unk_0059b734);
  const int st = close_req_.load();
  if (at_head) {
    if (st == 1) {  // the system menu's Exit row: showSystemMenu stores 1 here and returns 0x1b (0x00432af1)
      ctx.mem.store<int32_t>(quit_va, 1);
      if (writes) writes->wrote(quit_va, 1);
      close_req_.store(2);
      ++close_written_;
    }
    return false;
  }
  const int32_t diff = ctx.mem.load<int32_t>(portsim::state_va(ctx, &portsim::GameState::g_difficulty));
  // g_difficulty is -1 on the boot's title menu (0x0040f8ab); after a session the title menu keeps the session's value
  // (that store is mainLoop+0x2eb, the boot path only), so P6B adds the title menu's own signal (title_menu)
  const bool menu = frame_ == 0 || diff == -1 || title_seen_.load();
  at_menu_.store(menu);
  body_mode_.store(body);
  if (st == 2 && ctx.mem.load<int32_t>(quit_va) == 0) close_req_.store(3);  // H1 cleared it (ML+0x11511): the prompt runs
  if (close_req_.load() == 3 && body) {  // the player answered "go on": the frame loop runs again
    close_req_.store(0);
    ++close_cancelled_;
  }
  if (menu && close_req_.load() == 1) {  // pressed on a screen that is no frame (before P6B: also after the prompt's quit)
    ++close_direct_;
    return true;
  }
  return false;
}

// P7 (host.h in_game_): the band hint follows the game's own state word, read only (nothing is written)
void LiveSession::track_screen(portsim::Ctx &ctx, bool frame_loop) {
  const int32_t diff = ctx.mem.load<int32_t>(portsim::state_va(ctx, &portsim::GameState::g_difficulty));
  // g_difficulty is -1 only on the boot's title menu (0x0040f8ab, P6B item 1); after a session the title menu reports itself
  // through title_menu() (title_seen_), so both clear the in-game hint (P6 merge)
  if (diff == -1 || title_seen_.load())
    in_game_ = false;
  else if (frame_loop)
    in_game_ = true;
}

bool banner_text(const portsim::GfxTextFrame &under, const portsim::GfxTextFrame &own, const std::vector<uint16_t> &px,
                 portsim::GfxTextFrame &out) {
  const size_t n = static_cast<size_t>(kLogicalW) * kLogicalH;
  if (px.size() != n || under.back.size() != n || under.clean.size() != n) return false;
  const bool has_own = own.back.size() == n && own.clean.size() == n;
  out = portsim::GfxTextFrame{};
  out.serial = has_own ? own.serial : under.serial;
  out.back = px;
  out.clean = px;
  // same[i]: px shows under's back buffer word there and own's peel left it alone. keep[i]: every pixel within 2 of i is `same`
  // (a pixel of the banner's translucent panel can equal the word under it by chance; such isolated pixels would put the old
  // frame's background back inside the panel and draw specks of its text there), so under's run (if any) still shows at i
  std::vector<uint8_t> same(n, 0), keep(n, 0), row(n, 0);
  for (size_t i = 0; i < n; ++i) {
    if (has_own && own.clean[i] != px[i]) out.clean[i] = own.clean[i];  // one of the banner's glyphs
    else if (px[i] == under.back[i]) same[i] = 1;
  }
  constexpr int32_t kR = 2;
  for (int32_t y = 0; y < kLogicalH; ++y)  // the erosion: rows, then columns (the window's edge does not erode)
    for (int32_t x = 0; x < kLogicalW; ++x) {
      uint8_t v = 1;
      for (int32_t xx = std::max(x - kR, 0); xx <= std::min(x + kR, kLogicalW - 1) && v; ++xx)
        v = same[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(xx)];
      row[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)] = v;
    }
  for (int32_t y = 0; y < kLogicalH; ++y)
    for (int32_t x = 0; x < kLogicalW; ++x) {
      uint8_t v = 1;
      for (int32_t yy = std::max(y - kR, 0); yy <= std::min(y + kR, kLogicalH - 1) && v; ++yy)
        v = row[static_cast<size_t>(yy) * kLogicalW + static_cast<size_t>(x)];
      const size_t i = static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x);
      keep[i] = v;
      if (v) out.clean[i] = under.clean[i];
    }
  for (const portsim::GfxTextRun &r : under.runs) {
    if (r.target == 2 || r.x1 <= r.x0 || r.y1 <= r.y0) continue;  // a world run: the banner frame has no world under it
    portsim::GfxTextRun c = r;
    c.layer = -2;
    c.vis_layer.clear();
    bool any = false;
    const int32_t bw = c.x1 - c.x0;
    for (int32_t y = c.y0; y < c.y1; ++y)
      for (int32_t x = c.x0; x < c.x1; ++x) {
        const size_t j = static_cast<size_t>(y - c.y0) * static_cast<size_t>(bw) + static_cast<size_t>(x - c.x0);
        if (j >= c.vis.size() || !c.vis[j]) continue;
        if (x < 0 || y < 0 || x >= kLogicalW || y >= kLogicalH || !keep[static_cast<size_t>(y) * kLogicalW + static_cast<size_t>(x)])
          c.vis[j] = 0;
        else
          any = true;
      }
    if (any) out.runs.push_back(std::move(c));
  }
  if (has_own) out.runs.insert(out.runs.end(), own.runs.begin(), own.runs.end());
  return !out.runs.empty();
}

bool take_text_frame(portsim::Ctx &ctx, const std::vector<uint16_t> &px, portsim::GfxTextFrame &tf, bool banner) {
  const bool got = banner ? portsim::gfx_text_banner(ctx, tf) : portsim::gfx_text_frame(ctx, tf);
  if (got && tf.back == px && !tf.empty()) return true;
  tf = portsim::GfxTextFrame{};
  return false;
}

// P10E (present.h CursorProj): the snapshot, read from the GameState at the frame's present on the sim thread. Nothing is written:
// tileToScreen's cache hits are copied, its misses are worldToScreen (bridge.h world_to_screen, which reads only) of the tile's
// centre ((t << 10) + 0x200) with tileToScreen's window test (x in [-0x40 - E, W + 0x40 + E), y in [-0x2a, H + 0x2a), E =
// wide_view_margin_t2s), the values the frame's next screenToTile would see while the state stays as it is.
static void take_proj(portsim::Ctx &ctx, CursorProj &p, bool pic) {
  p.ok = false;
  const portsim::GameState &g = ctx.gs();
  // P6_Cursor copies unk_005a9cbc into unk_005a9cc0 before its screenToTile (ML+0xa454): with it set, screenToTile halves the point
  // and worldToScreen doubles its result, and the cache holds those points; not replicated (the preview keeps the frame's place)
  if (g.unk_005a9cbc != 0 || g.unk_005a9cc0 != 0) return;
  if (g.g_cameraRot != 0 && g.g_cameraRot != 2 && g.g_cameraRot != 4 && g.g_cameraRot != 6) return;
  if (g.g_tileStep4c2840 == 0) return;
  if (g.g_cursorTileX < 0 || g.g_cursorTileX >= 50 || g.g_cursorTileY < 0 || g.g_cursorTileY >= 50) return;
  // P12A fix 1: the snapshot is in the coordinates of the frame's own picture. The key cases and edge scroll move the camera after
  // the picture was drawn and before the present (present.h CameraSnap), so the camera is the one updateTerrainRender drew with
  // (g_terrainLastCamX / Y, equal to gCameraTileX / Y when the camera did not move), and worldToScreen's answers (which read
  // gCameraTileX / Y) are moved back by camera_shift (exact: present.h). A pending zoom or rotation change: not taken.
  // With `pic` false (--smooth-camera off) the snapshot is P10E's (gCameraTileX / Y, worldToScreen as it answers).
  if (pic && (g.g_terrainLastZoom != g.g_zoomScale || g.g_terrain820f30[2] != g.g_cameraRot)) return;
  int32_t nd_x = 0, nd_y = 0;  // the next camera's shift of every world point (800-space)
  if (pic) {
    CameraSnap a, b;
    a.ok = b.ok = true;
    a.cam_x = g.g_terrainLastCamX;
    a.cam_y = g.g_terrainLastCamY;
    b.cam_x = g.gCameraTileX;
    b.cam_y = g.gCameraTileY;
    a.zoom = b.zoom = g.g_zoomScale;
    a.rot = b.rot = g.g_cameraRot;
    if (!camera_shift(a, b, nd_x, nd_y)) return;
  }
  p.tile_x = g.g_cursorTileX;
  p.tile_y = g.g_cursorTileY;
  p.sw = g.g_screenWidth;
  p.sh = g.g_screenHeight;
  p.rot = g.g_cameraRot;
  p.cam_x = pic ? g.g_terrainLastCamX : g.gCameraTileX;  // P12A fix 1: the picture's camera (equal while the camera stays)
  p.cam_y = pic ? g.g_terrainLastCamY : g.gCameraTileY;
  p.step = g.g_tileStep4c2840;
  for (int k = 0; k < 8; ++k) {
    p.dir_dx[k] = g.f_tab_dirDx_4c2878[k];
    p.dir_dy[k] = g.f_tab_dirDy_4c2898[k];
  }
  p.sx.assign(2500, 0);
  p.sy.assign(2500, 0);
  p.on.assign(2500, 0);
  const int32_t E = portsim::wide_view_margin_t2s(ctx);
  for (int32_t tx = 0; tx < 50; ++tx)
    for (int32_t ty = 0; ty < 50; ++ty) {
      const size_t cell = static_cast<size_t>(tx * 50 + ty);
      const int16_t c = g.g_tileScreenX[cell];
      if (c != 0) {
        if (c == -99) continue;
        p.sx[cell] = c;
        p.sy[cell] = g.g_tileScreenY[cell];
        p.on[cell] = 1;
        continue;
      }
      int32_t x = 0, y = 0;
      // P12A fix 1: with a pending camera move, a margin wide enough that worldToScreen adds the tile's height to every point,
      // then the point moved back into the picture's coordinates
      portsim::world_to_screen(ctx, (tx << 10) + 0x200, (ty << 10) + 0x200, x, y, nd_x != 0 || nd_y != 0 ? 0x800 : 0);
      x -= nd_x;
      y -= nd_y;
      if (x >= -0x40 - E && x < g.g_screenWidth + 0x40 + E && y >= -0x2a && y < g.g_screenHeight + 0x2a) {
        p.sx[cell] = x;
        p.sy[cell] = y;
        p.on[cell] = 1;
      }
    }
  p.ok = p.on[static_cast<size_t>(p.tile_x * 50 + p.tile_y)] != 0;
}

// P10E: screenToTile (tick_render.inc Rend::screenToTile) with useWorld 0 on the snapshot. As the original, the point of the last
// tileToScreen that answered is kept for the distance when a tile has none.
bool proj_tile(const CursorProj &p, int32_t px, int32_t py, int32_t &tx, int32_t &ty) {
  using namespace portsim;
  if (!p.ok || p.step == 0 || p.on.size() != 2500) return false;
  int32_t cx = px - p.sw / 2 - 0x40;
  int32_t cy = py - p.sh / 2;
  if (p.rot == 2) {
    cx += 0x20;
    cy += 0x14;
  } else if (p.rot == 4) {
    cx += 0x40;
  } else if (p.rot == 6) {
    cx += 0x20;
    cy -= 0x14;
  }
  const int32_t s = shl32(cy, 5) / 40;
  const int32_t q = p.step, camX = p.cam_x, camY = p.cam_y;
  if (p.rot == 0) {
    tx = div32(div32(mul32(camX * 2 + 1, q) - s * 2 + cx, q), 2);
    ty = div32(div32(q + (mul32(camY, q) + s) * 2 + cx, q), 2);
  } else if (p.rot == 2) {
    tx = div32(div32(mul32(camX * 2 + 1, q) - s * 2 - cx, q), 2);
    ty = div32(div32(mul32(camY * 2 + 1, q) - s * 2 + cx, q), 2);
  } else if (p.rot == 4) {
    tx = div32(div32((mul32(q, camX) + s) * 2 - cx + q, q), 2);
    ty = div32(div32(mul32(camY * 2 + 1, q) - s * 2 - cx, q), 2);
  } else {
    tx = div32(div32(q + (mul32(q, camX) + s) * 2 + cx, q), 2);
    ty = div32(div32((mul32(camY, q) + s) * 2 - cx + q, q), 2);
  }
  int32_t a = 0, b = 0;
  bool have = false;
  auto t2s = [&](int32_t x, int32_t y) {
    if (x < 0 || x >= 50 || y < 0 || y >= 50) return;
    const size_t cell = static_cast<size_t>(x * 50 + y);
    if (!p.on[cell]) return;
    a = p.sx[cell];
    b = p.sy[cell];
    have = true;
  };
  for (int iter = 0; iter < 256; ++iter) {
    const int32_t t = tx, u = ty;
    t2s(t, u);
    if (!have) return false;  // the original's locals hold no projection yet
    int32_t best = distance(px - a, (py - b) * 2);
    for (int k = 0; k < 8; ++k) {
      const int32_t nx = t + p.dir_dx[k], ny = u + p.dir_dy[k];
      t2s(nx, ny);
      const int32_t d = distance(px - a, (py - b) * 2);
      if (d < best) {
        best = d;
        tx = nx;
        ty = ny;
      }
    }
    if (tx == t && ty == u) return true;
  }
  return false;
}

bool make_world_image(portsim::LiveTerrain &live, portsim::Ctx &ctx, const std::vector<uint16_t> &px, bool layered, WorldImage &wi,
                      const portsim::GfxTextFrame *text, bool cursor, bool proj, bool pic) {
  const portsim::LiveTerrainWorld *w = live.world();
  if (!w) return false;
  const bool terrain = portsim::gfx_back_terrain(ctx);
  // P9B: the frame's HUD layers, when they were published with exactly this back buffer
  portsim::GfxHudFrame f;
  const bool use = layered && portsim::gfx_hud_frame(ctx, f) && !f.empty() && f.back == px;
  // P9E fix 4: with the HUD layers, the tool previews at the cursor apart from the world (WorldImage::cur_*)
  const bool apart = cursor && use && live.world_cursor(wi.cur_px, wi.cur_cov, wi.cur_mask, &wi.cur_hi, &px);  // P10F: cur_hi, the back buffer
  if (!apart) {
    wi.cur_px.clear();
    wi.cur_hi.clear();
    wi.cur_cov.clear();
    wi.cur_mask.clear();
  }
  if (!live.world_frame(use ? f.base : px, terrain, wi.px, wi.mask, false, apart)) return false;
  if (apart && live.world()) {
    const int32_t pw = live.world()->pw, ph = live.world()->ph;
    int32_t x0 = pw, y0 = ph, x1 = 0, y1 = 0;
    for (int32_t y = 0; y < ph; ++y) {  // P10A: a row's first covered pixel by memchr, its last from the end
      const uint8_t *r = wi.cur_cov.data() + static_cast<size_t>(y) * static_cast<size_t>(pw);
      const void *hit = std::memchr(r, 1, static_cast<size_t>(pw));
      const void *hit2 = wi.cur_hi.empty() ? nullptr : std::memchr(r, 2, static_cast<size_t>(pw));  // P10F: cur_cov 2 (the box)
      if (!hit || (hit2 && hit2 < hit)) hit = hit2;
      if (!hit) continue;
      const int32_t a = static_cast<int32_t>(static_cast<const uint8_t *>(hit) - r);
      int32_t b = pw;
      while (b > a && !r[b - 1]) --b;
      x0 = std::min(x0, a);
      y0 = std::min(y0, y);
      x1 = std::max(x1, b);
      y1 = std::max(y1, y + 1);
    }
    wi.cur_box[0] = x0;
    wi.cur_box[1] = y0;
    wi.cur_box[2] = x1;
    wi.cur_box[3] = y1;
  }
  // P9C: with the frame's text runs, also the mask over the picture without their glyphs (a label over the course shows the
  // world under its window-resolution glyphs); the presenter takes it when it draws the runs
  if (text && !text->empty()) {
    const std::vector<uint16_t> *src = nullptr;
    if (use && text->hud_serial == f.serial && text->base_clean.size() == f.base.size()) src = &text->base_clean;
    else if (!use && text->clean.size() == px.size()) src = &text->clean;
    if (src && !live.world_frame(*src, terrain, wi.px_text, wi.mask_text, true, apart)) {
      wi.px_text.clear();
      wi.mask_text.clear();
    }
  }
  wi.pw = w->pw;
  wi.ph = w->ph;
  wi.fw = w->fw;
  wi.ox = w->ox;
  wi.proj.ok = false;  // P10E: the projection only with previews at the cursor (world sprites or the frame's 2D sprite pixels)
  if (proj && use && (apart || !f.cursor2d.empty())) take_proj(ctx, wi.proj, pic);
  {  // P12A fix 1 (present.h CameraSnap): the camera updateTerrainRender drew this picture with (read only)
    const portsim::GameState &g = ctx.gs();
    CameraSnap &c = wi.cam;
    c.cam_x = g.g_terrainLastCamX;
    c.cam_y = g.g_terrainLastCamY;
    c.zoom = g.g_terrainLastZoom;
    c.rot = g.g_terrain820f30[2];
    c.ok = use && (c.rot == 0 || c.rot == 2 || c.rot == 4 || c.rot == 6) && c.rot == g.g_cameraRot && g.g_screenWidth == kLogicalW &&
           g.unk_005a9cbc == 0 && g.unk_005a9cc0 == 0 && c.zoom > 0;
    c.next_x = g.gCameraTileX;
    c.next_y = g.gCameraTileY;
    c.next_zoom = g.g_zoomScale;
    c.next_rot = g.g_cameraRot;
  }
  if (use) {
    // development: SG_P9B_LAYER_LOG=<file> appends one line per present with layers: serial, then id:anchor:box:pixels per layer
    static std::FILE *lf = std::getenv("SG_P9B_LAYER_LOG") ? std::fopen(std::getenv("SG_P9B_LAYER_LOG"), "a") : nullptr;
    if (lf) {
      std::fprintf(lf, "%llu", static_cast<unsigned long long>(f.serial));
      for (const portsim::GfxHudLayer &l : f.layers) {
        size_t n = 0, b = 0;
        for (const portsim::GfxLayerPx &q : l.px) {
          n += q.kind ? 1 : 0;
          b += q.kind >= 2 ? 1 : 0;
        }
        std::fprintf(lf, " %d:%d:%d,%d-%d,%d:%zu/%zu", l.id, l.anchor, l.x0, l.y0, l.x1, l.y1, n, b);
      }
      std::fputc('\n', lf);
      std::fflush(lf);
    }
    wi.hud = std::move(f);
  }
  wi.hud_none = layered && !use && portsim::gfx_hud_modal(ctx);  // P9D
  return true;
}

void LiveSession::recycle_world(WorldImage &&w) {  // P10A
  std::lock_guard<std::mutex> lk(frame_mu_);
  if (w.px.capacity() > spare_world_.px.capacity()) spare_world_ = std::move(w);
}

// P10A: an image whose vectors keep their capacity, every field as a new WorldImage has it
static void world_reset(WorldImage &w) {
  for (std::vector<uint16_t> *v : {&w.px, &w.px_text, &w.cur_px, &w.cur_hi}) v->clear();
  for (std::vector<uint8_t> *v : {&w.mask, &w.mask_text, &w.cur_cov, &w.cur_mask}) v->clear();
  w.pw = w.ph = w.fw = w.ox = 0;
  w.hud = portsim::GfxHudFrame{};
  w.hud_none = false;
  w.text = portsim::GfxTextFrame{};
  for (int32_t &b : w.cur_box) b = 0;
  w.proj.ok = false;  // P10E (its tables keep their capacity)
  w.cam = CameraSnap{};  // P12A fix 1
  w.hr_scene.reset();    // P12A fix 2
}

bool LiveSession::take_frame(std::vector<uint16_t> &px, uint32_t &index, BandFill *band, WorldImage *world) {
  std::lock_guard<std::mutex> lk(frame_mu_);
  if (!frame_waiting_) return false;
  px = frame_slot_;
  index = frame_index_;
  if (band) *band = frame_band_;
  if (world) *world = std::move(frame_world_);  // P8B
  taken_cursor_ = frame_cursor_;  // P10I fix 2
  frame_world_ = WorldImage{};
  frame_waiting_ = false;
  return true;
}

bool LiveSession::focus_child_takes(uint32_t, uint32_t) {
  // U-1650: no window of the port's tree takes keys before the root at this HEAD (the character editor's bio EditBox is
  // MissingResource there), so every key goes to the root handlers
  return false;
}

void LiveSession::before_translate(uint32_t msg, uint32_t wp) {
  if (!opt_.keys_from_messages) return;
  if (msg == kWmKeyDown || msg == kWmSysKeyDown) keys_[wp & 0xff].store(1);
  if (msg == kWmKeyUp || msg == kWmSysKeyUp) keys_[wp & 0xff].store(0);
}

int32_t LiveSession::key_state(uint32_t vk, bool async) const {
  if (!opt_.keys_from_messages) return -1;
  if (!keys_[vk & 0xff].load()) return 0;
  return async ? 0x8001 : 0x8000;  // the shim's forced-down answers (SimGolfShim_SetVirtualKey state 2)
}

// ------------------------------------------------------------------------------------------------ sim thread
uint32_t LiveSession::now() {
  const uint32_t real = timeGetTime();
  if (opt_.clock_origin < 0) return real;
  if (!clk_set_) {
    clk_off_ = static_cast<uint32_t>(opt_.clock_origin) - real;
    clk_set_ = true;
  }
  return real + clk_off_;
}

// S7.8: index of a present kind in the counters ('r' 0, 'w' 1, 'm' 2, 'p' 3)
static int kind_index(char kind) { return kind == 'r' ? 0 : kind == 'w' ? 1 : kind == 'm' ? 2 : 3; }

void LiveSession::present(portsim::Ctx &ctx, char kind) {
  const ProfScope prof(Phase::Capture);  // P10A
  if (Profiler::get().enabled() && !overlay_font_) {  // P10A: the overlay's font, the game's own (gameMain font call 3) once created
    try {
      const portsim::GameFont gf = portsim::game_font(ctx, portsim::game_font_ref(ctx, 3));
      if (!gf.face.empty()) {
        Profiler::get().set_font(gf.face, gf.style);
        overlay_font_ = true;
      }
    } catch (...) {
    }
  }
  // P10I fix 2: the point of the first cursor read since the last present (the frame's hover tests: P6_Panels' View_toLocal
  // ML+0xa198 comes before P6_Cursor's and H1's reads), the last read's when none came; taken at every present, shown or not
  Point fc;
  {
    std::lock_guard<std::mutex> lk(mu_);
    fc = first_set_ ? first_answer_ : last_answer_;
    first_set_ = false;
  }
  const int ki = kind_index(kind);
  ++opportunities_[ki];
  const bool was_pending = r_pending_;
  if (kind != 'r') r_pending_ = false;
  if (!reg_call(ctx, "gfx.dump", 0)) return;
  const int64_t n = ctx.files_ptr ? ctx.files().size("$gfx/back.555") : -1;
  if (n != static_cast<int64_t>(kLogicalW) * kLogicalH * 2) return;
  std::vector<uint16_t> px(static_cast<size_t>(n / 2));
  ctx.files().read("$gfx/back.555", 0, px.data(), static_cast<uint64_t>(n));
  if (px == last_presented_) return;
  ++made_[ki];
  if (kind == 'r') {
    if (was_pending) ++r_superseded_;
    r_pending_ = true;
  }
  const uint32_t mark = marker ? marker->mark() : 0;
  std::vector<uint16_t> show = px;
  if (opt_.defect == HostDefect::PresentLate) {  // negative control: the window gets the previous present's frame
    show = previous_.empty() ? std::vector<uint16_t>(px.size(), 0) : previous_;
    previous_ = px;
  }
  last_presented_ = px;
  const uint32_t index = static_cast<uint32_t>(presents_.size());
  presents_.push_back({index, mark, frame_, records_, kind, frame_hash(px)});
  WorldImage wi;  // P8B: the world layer under this frame (none without HostOptions::world or before the first terrain pass)
  {  // P10A: the buffers of a world image the window is done with (new pages for 2 to 4 MB planes cost more than filling them)
    std::lock_guard<std::mutex> lk(frame_mu_);
    wi = std::move(spare_world_);
    spare_world_ = WorldImage{};
  }
  world_reset(wi);
  portsim::GfxTextFrame tf;  // P9C
  const bool text = opt_.text_native && take_text_frame(ctx, px, tf);
  if (opt_.world && terrain_log && terrain_log->live)
    if (make_world_image(*terrain_log->live, ctx, px, opt_.hud_layers, wi, text ? &tf : nullptr, opt_.cursor_previews,
                         opt_.cursor_previews && opt_.cursor_live, opt_.smooth_camera)) {  // P10E; P12A
      ++presents_world_;
      if (!wi.hud.empty()) ++presents_layered_;  // P9B
    }
  wi.cam.t_ns = portsim::perf_now_ns();  // P12A fix 1: the present's time (the presenter's game frame period)
  if (opt_.smooth_objects && !wi.empty()) wi.hr_scene = portsim::gfx_hr_scene(ctx);  // P12A fix 2: the queue's draws
  if (opt_.text_banner) {  // P10B fix 1: kept for a loading banner over this frame
    if (text) last_text_ = tf;
    else last_text_ = portsim::GfxTextFrame{};
  }
  if (text) {
    ++presents_text_;
    text_runs_ += tf.runs.size();
    wi.text = std::move(tf);
  }
  {
    std::lock_guard<std::mutex> lk(frame_mu_);
    frame_world_ = std::move(wi);
    frame_slot_ = std::move(show);
    frame_index_ = index;
    frame_cursor_ = fc;  // P10I fix 2
    frame_band_ = in_game_ ? BandFill::Black : BandFill::Extend;  // P7
    ++(in_game_ ? presents_black_ : presents_extend_);
    frame_waiting_ = true;
    last_band_ = frame_band_;  // P10C fix 1
    last_index_ = index;
  }
  void *h;
  {
    std::lock_guard<std::mutex> lk(mu_);
    h = hwnd_;
  }
  if (h) PostMessageW(static_cast<HWND>(h), kMsgPresent, 0, 0);
}

bool LiveSession::frame_head(portsim::Ctx &ctx, uint32_t frame, uint32_t &t) {
  if (stopping()) return false;
  Profiler::get().frame_head();  // P10A: closes the previous game frame
  head_t_ = timeGetTime();  // pacing runs on the real clock; the core gets now() (clock_origin)
  t = now();
  frame_ = frame;
  frame_now_.store(frame);
  records_ = frame;
  w_check_ = true;  // S7.8: no Window::refreshFull of this frame has presented yet  // the core writes this frame's record (index frame - 1) as it returns
  const uint32_t va = portsim::state_va(ctx, &portsim::GameState::g_flags);
  uint32_t flags = ctx.mem.load<uint32_t>(va);
  const bool set = opt_.flag17_from >= 0 && frame == static_cast<uint32_t>(opt_.flag17_from);
  const bool clr = opt_.flag17_to >= 0 && frame == static_cast<uint32_t>(opt_.flag17_to);
  if (set || clr) {  // test only (--flag17): a harness write the recording carries (DELIVER w32 after this FRAME record)
    flags = set ? (flags | kFlagSlowFrames) : (flags & ~kFlagSlowFrames);
    ctx.mem.store<uint32_t>(va, flags);
    if (writes) writes->wrote(va, flags);
  }
  apply_view(ctx);                // G32: the window's view width and world size (recorded write, before the frame body)
  apply_undo(ctx);                // P11D: Ctrl+Z / Ctrl+Y (recorded write, before the frame body)
  track_screen(ctx, true);        // P7: a frame head is the frame loop
  title_seen_.store(false);       // P6B item 1: a frame runs (a session, not the title menu)
  end_load(ctx);                  // P6B item 2
  close_check(ctx, true, false);  // P5B item 3: a close request becomes the system menu's Exit flag here
  if (frames_log_) {
    std::fprintf(frames_log_, "%u\t%u\t%u\n", frame, t, flags);
    std::fflush(frames_log_);
  }
  return true;
}

bool LiveSession::clock(portsim::Ctx &, int32_t, bool body, uint32_t &t) {
  if (!body) w_check_ = true;  // S7.8: a wall-time loop ran (a modal screen may have drawn through its own module)
  t = now();
  return true;
}

// development (P12A): SG_P12_LOG=<file> also writes <file>.reads, one line per game cursor read the camera slide applies to (not H1's
// edge-scroll read): the index the frame being built will present at, the read's client point, the game offset the remap applied,
// the picture offset (PanelRemap::peek on the same point), the logical point answered
static void p12_read_log(const PanelRemap *r, uint32_t next_present, int32_t cx, int32_t cy, int32_t gx, int32_t gy, int32_t lx,
                         int32_t ly) {
  static std::FILE *f = [] {
    const char *e = std::getenv("SG_P12_LOG");
    return e ? std::fopen((std::string(e) + ".reads").c_str(), "w") : nullptr;
  }();
  if (!f || !r) return;
  int32_t px = cx, py = cy, sx = 0, sy = 0;
  r->peek(px, py, sx, sy);
  std::fprintf(f, "read\t%u\t%d,%d\t%d,%d\t%d,%d\t%d,%d\n", next_present, cx, cy, gx, gy, sx, sy, lx, ly);
  std::fflush(f);
}

void LiveSession::cursor(portsim::Ctx &ctx, bool modal, int32_t &x, int32_t &y) {
  if (modal) w_check_ = true;  // S7.8
  if (exit_arm_) {  // P6B item 1 (host.h request_close): the title menu's exit icon, clicked at the next cursor read (the
    // title loop's View_toLocal does not come as a modal read: with `&& modal` the exit was never taken)
    exit_arm_ = false;
    exiting_ = true;
    ++exit_title_;
    const uint32_t button = portsim::state_va(ctx, &portsim::GameState::g_rootInput) + 0x10;
    ctx.mem.store<int32_t>(button, 1);
    if (writes) writes->wrote(button, 1);
    x = kExitIconX;
    y = kExitIconY;
    return;
  }
  std::lock_guard<std::mutex> lk(mu_);
  if (opt_.cursor == CursorSource::Message) {
    // P12A fix 1: H1's edge-scroll read takes the message's point without the camera slide's move (the window's own edges); any
    // other read takes the last message's client point through the remap at the read (as the system cursor's reads do), so a
    // cursor left still over the course follows the slid picture
    x = edge_read_ ? msg_cursor_edge_.x : msg_cursor_.x;
    y = edge_read_ ? msg_cursor_edge_.y : msg_cursor_.y;
    int32_t adx = msg_cursor_dx_;
    if (!edge_read_ && opt_.smooth_camera && opt_.remap && msg_raw_ok_) {
      int32_t rx = msg_raw_.x, ry = msg_raw_.y, slx = 0, sly = 0;
      bool vac = false;
      opt_.remap->map(rx, ry, vac, slx, sly);
      adx = msg_raw_.x - rx - slx;
      const Point l = Translator(vp_).to_logical(Point{rx, ry});
      x = l.x;
      y = l.y;
      p12_read_log(opt_.remap, static_cast<uint32_t>(presents_.size()), msg_raw_.x, msg_raw_.y, slx, sly, x, y);  // P12A (development)
    }
    if (opt_.remap && opt_.cursor_previews) opt_.remap->set_cursor_answer(adx, x, y);  // P9E fix 4
    if (!edge_read_) plain_answer_ = Point{x, y};  // P10E
    note_answer(Point{x, y});  // P10I fix 2
    return;
  }
  POINT p = {0, 0};
  GetCursorPos(&p);
  const POINT screen = p;
  if (hwnd_) ScreenToClient(static_cast<HWND>(hwnd_), &p);
  int32_t answer_dx = 0;  // P9E fix 4
  if (opt_.remap) {  // G32: a point over a moved HUD panel answers its 800-space place
    int32_t rx = p.x, ry = p.y, slx = 0, sly = 0;
    bool vac = false;
    // P12A fix 1: a point over the course is moved by the camera slide (PanelRemap::set_slide), except for H1's edge-scroll read
    if (edge_read_) opt_.remap->map_noslide(rx, ry);
    else opt_.remap->map(rx, ry, vac, slx, sly);
    answer_dx = p.x - rx - slx;  // the remap's own move only
    if (!edge_read_ && opt_.smooth_camera) {  // P12A (development): SG_P12_LOG's read lines
      const Point lr = Translator(vp_).to_logical(Point{rx, ry});
      p12_read_log(opt_.remap, static_cast<uint32_t>(presents_.size()), p.x, p.y, slx, sly, lr.x, lr.y);
    }
    p.x = rx;
    p.y = ry;
  }
  const Point l = Translator(vp_).to_logical(Point{p.x, p.y});
  if (opt_.remap && opt_.cursor_previews) opt_.remap->set_cursor_answer(answer_dx, l.x, l.y);  // P9E fix 4
  if (!edge_read_) plain_answer_ = l;  // P10E
  if (opt_.edge_guard && hwnd_) {
    // P5B item 1: the original reads GetCursorPos + ScreenToClient unclamped, so a cursor left of the window gives x < 0 and
    // H1 (ML+0xebb4..ML+0xebe3) scrolls the course. Only a cursor inside the client area of the foreground window counts;
    // otherwise the last inside answer, one pixel away from every edge, is given again.
    HWND h = static_cast<HWND>(hwnd_);
    RECT c;
    GetClientRect(h, &c);
    const bool inside = p.x >= c.left && p.x < c.right && p.y >= c.top && p.y < c.bottom;
    const bool top = GetForegroundWindow() == h && WindowFromPoint(screen) == h;
    if (!inside || !top) {
      ++guarded_reads_;
      x = std::min(std::max(last_inside_.x, 1), kLogicalW - 2);
      y = std::min(std::max(last_inside_.y, 1), kLogicalH - 2);
      note_answer(Point{x, y});  // P10I fix 2
      return;
    }
    last_inside_ = l;
  }
  x = l.x;
  y = l.y;
  note_answer(l);  // P10I fix 2
}

// P10E (host.h live_cursor): LiveSession::cursor's point without its effects (no remap answer handed on, no last_inside_, no counts)
bool LiveSession::live_cursor(int32_t &x, int32_t &y, int32_t &dx) {
  std::lock_guard<std::mutex> lk(mu_);  // (the title menu's exit click, sim-thread state, has no tool preview to move)
  if (opt_.cursor == CursorSource::Message && opt_.smooth_camera && opt_.remap && msg_raw_ok_) {
    // P12A fix 1: the last message's client point through the remap now (the slide moves the picture under a still cursor)
    int32_t rx = msg_raw_.x, ry = msg_raw_.y, slx = 0, sly = 0;
    opt_.remap->peek(rx, ry, slx, sly);
    dx = msg_raw_.x - rx - slx;
    const Point l = Translator(vp_).to_logical(Point{rx, ry});
    x = l.x;
    y = l.y;
    return true;
  }
  if (opt_.cursor == CursorSource::Message) {
    x = msg_cursor_.x;
    y = msg_cursor_.y;
    dx = msg_cursor_dx_;
    return true;
  }
  if (!hwnd_) return false;
  POINT p = {0, 0};
  GetCursorPos(&p);
  const POINT screen = p;
  ScreenToClient(static_cast<HWND>(hwnd_), &p);
  dx = 0;
  if (opt_.remap) {
    int32_t rx = p.x, ry = p.y, slx = 0, sly = 0;
    opt_.remap->peek(rx, ry, slx, sly);  // P12A fix 1: the camera slide's move is not the remap's offset
    dx = p.x - rx - slx;
    p.x = rx;
    p.y = ry;
  }
  if (opt_.edge_guard) {
    HWND h = static_cast<HWND>(hwnd_);
    RECT c;
    GetClientRect(h, &c);
    const bool inside = p.x >= c.left && p.x < c.right && p.y >= c.top && p.y < c.bottom;
    if (!inside || GetForegroundWindow() != h || WindowFromPoint(screen) != h) return false;
  }
  const Point l = Translator(vp_).to_logical(Point{p.x, p.y});
  x = l.x;
  y = l.y;
  return true;
}

// P8B (HostOptions::wide_edge): H1's edge-scroll read. The cursor answer is the logical point (cursor() above); on a client wider
// than the 4:3 content rectangle the side columns give x < 0 or x > 799, which H1 (ML+0xebc4..ML+0xebe3: x <= 0, x >= 0x31f)
// would take as the edge. Here x is 0 / 0x31f only when the point is at or beyond the client's own left / right edge, else it is
// clamped to 1..0x31e; y is unchanged (the content rectangle has the client's full height in a wide window).
void LiveSession::cursor_at(portsim::Ctx &ctx, bool modal, int32_t site, int32_t &x, int32_t &y) {
  edge_read_ = site == portsim::kCursorSiteEdgeScroll;  // P10E
  cursor(ctx, modal, x, y);
  edge_read_ = false;
  if (!opt_.wide_edge || site != portsim::kCursorSiteEdgeScroll) return;
  Viewport vp;
  int32_t cw = 0;
  {
    std::lock_guard<std::mutex> lk(mu_);
    vp = vp_;
    cw = vp_.origin_x * 2 + vp_.client_w;
    if (hwnd_) {
      RECT c;
      if (GetClientRect(static_cast<HWND>(hwnd_), &c)) cw = c.right - c.left;
    }
  }
  if (vp.origin_x <= 0 || cw <= 0) return;  // no side columns: the original's test as it is
  ++edge_reads_;
  const Translator t(vp);
  const int32_t left = t.to_logical(Point{0, 0}).x, right = t.to_logical(Point{cw - 1, 0}).x;
  const int32_t was = x;
  if (x <= left) x = 0;
  else if (x >= right) x = kLogicalW - 1;
  else x = std::min(std::max(x, 1), kLogicalW - 2);
  if (x != was) ++edge_clamped_;
}

uint16_t LiveSession::async_key(portsim::Ctx &, int vk) {
  const int32_t k = key_state(static_cast<uint32_t>(vk), true);
  if (k >= 0) return static_cast<uint16_t>(k);
  if (opt_.edge_guard) {  // P5B item 1: keys and buttons held for another window are not the game's
    void *h;
    {
      std::lock_guard<std::mutex> lk(mu_);
      h = hwnd_;
    }
    if (h && GetForegroundWindow() != static_cast<HWND>(h)) return 0;
  }
  return static_cast<uint16_t>(GetAsyncKeyState(vk & 0xff));
}

portsim::LiveReply LiveSession::pump(portsim::Ctx &ctx, const portsim::LivePump &p, std::vector<portsim::Command> &out) {
  if (stopping()) return portsim::LiveReply::Stop;
  track_screen(ctx, p.body);  // P7: the side-band hint of the next presents
  if (close_check(ctx, false, p.body)) {  // P5B item 3: the close button on the title menu (or after the quit prompt)
    stop_.store(true);
    return portsim::LiveReply::Stop;
  }
  if (p.kind == portsim::PumpKind::Load) {  // P6B item 2: a world load's pump (loadFlic +0x54)
    const uint32_t now_ms = timeGetTime();
    if (!loading_) {
      loading_ = true;
      load_t0_ = now_ms;
      load_pumps_ = 0;
      load_post_ms_ = 0;
      portsim::flic::sheet_to_gfx_stats(load_dec_us0_, load_dec_calls0_);
      if (load_expected_ == 0) load_expected_ = kWorldLoadPumps;
    }
    ++load_pumps_;
    if (opt_.loading_overlay && title_ever_ && (load_post_ms_ == 0 || now_ms - load_post_ms_ >= 50)) {
      load_post_ms_ = now_ms;
      post_loading(ctx, load_pumps_);
    }
  } else {
    end_load(ctx);
  }
  if (exit_req_.load() || exiting_) {  // P6B item 1
    const uint32_t now_ms = timeGetTime();
    if ((!exiting_ && !exit_arm_ && now_ms - exit_req_ms_.load() > 500) || (exiting_ && now_ms - exit_begin_ms_ > 15000)) {
      ++exit_fallback_;  // no title-menu pass came (another screen), or the exit path did not end: stop here
      stop_.store(true);
      return portsim::LiveReply::Stop;
    }
    if (exiting_ && !p.body && p.kind == portsim::PumpKind::Blocking && now_ms - last_esc_ms_ >= 100) {
      // the credits (showCreditsScreen) end on a key; Esc is posted as the keyboard would send it (the translator records it)
      void *h;
      {
        std::lock_guard<std::mutex> lk(mu_);
        h = queue_.empty() ? hwnd_ : nullptr;
      }
      if (h) {
        PostMessageW(static_cast<HWND>(h), WM_KEYDOWN, VK_ESCAPE, 0x00010001);
        PostMessageW(static_cast<HWND>(h), WM_CHAR, 0x1b, 0x00010001);
        PostMessageW(static_cast<HWND>(h), WM_KEYUP, VK_ESCAPE, 0xc0010001);
        last_esc_ms_ = now_ms;
        ++exit_esc_;
      }
    }
  }
  if (!p.body) w_check_ = true;  // S7.8
  // one dispatch: the queued commands up to the first release whose press is in the same dispatch (a press and its release
  // are never dispatched by one pump call: real input has the player's hold between them, and the game's loops read the
  // button word between pumps; the script host's click / key chunks, live.cpp). Press: onPoint 2, onPoint2 5, onChar 8;
  // release: onPress1 1, onPress2 4, onChar0 9.
  uint32_t body_call = 0;
  if (p.body) {  // S7.4: the ordinal of this body pump call in its frame (0 = P6's doAllTasks, 1 = H1's pumpMessages)
    if (p.frame != body_calls_seen_frame_) {
      if (body_calls_frame_) body_call_counts_.push_back({body_calls_seen_frame_, body_calls_frame_});
      body_calls_seen_frame_ = p.frame;
      body_calls_frame_ = 0;
    }
    body_call = body_calls_frame_++;
  }
  // H1's pumpMessages (body call 1) takes at most ONE mouse-class message per frame and never a key (roadmap S7.4, found by
  // the cross-check against the original): it drains WM_MOUSEMOVE only (PeekMessage 0x200..0x200, 0x0045c03e..0x0045c04d),
  // then doMouse jumps to the display's slot +0x13c (0x00483ce2), jgld 0x1006b7e0: ONE PeekMessage(0x200..0x20a,
  // PM_REMOVE) at 0x1006b7ff..0x1006b811, dispatched. P6's doAllTasks loops 0x00483bb0 -> slot +0x128, jgld 0x1006b0f0
  // (PeekMessage(0, 0), any message) until the queue is empty. A key or a second button message waits for the next P6.
  const bool h1 = p.body && body_call == 1;
  auto msg_of = [&](uint32_t seq) -> uint32_t {
    for (auto it = event_rows_.rbegin(); it != event_rows_.rend(); ++it)
      if (it->seq == seq) return it->msg;
    return 0;
  };
  // H1: the first queued mouse-class message (PeekMessage's range filter skips the keys queued before it): moved to the
  // front of the queue, so the dispatch below takes its commands only
  auto h1_takes = [&]() {
    size_t i = 0;
    for (; i < queue_.size(); ++i) {
      const uint32_t m = msg_of(queue_seq_[i]);
      if (m >= kWmMouseMove && m <= 0x20a) break;
    }
    if (i == queue_.size()) return false;
    const uint32_t seq = queue_seq_[i];
    size_t j = i;
    while (j < queue_.size() && queue_seq_[j] == seq) ++j;
    std::rotate(queue_.begin(), queue_.begin() + static_cast<std::ptrdiff_t>(i), queue_.begin() + static_cast<std::ptrdiff_t>(j));
    std::rotate(queue_seq_.begin(), queue_seq_.begin() + static_cast<std::ptrdiff_t>(i),
                queue_seq_.begin() + static_cast<std::ptrdiff_t>(j));
    return true;
  };
  auto deliver = [&]() {
    size_t n = 0;
    bool pressed = false;
    for (; n < queue_.size(); ++n) {
      if (h1 && n && queue_seq_[n] != queue_seq_[0]) break;  // H1: the first message's commands only
      const uint32_t h = queue_[n].handler;
      if ((h == 1 || h == 4 || h == 9) && pressed) break;
      if (h == 2 || h == 5 || h == 8) pressed = true;
    }
    out.assign(queue_.begin(), queue_.begin() + static_cast<std::ptrdiff_t>(n));
    // P9E fix 3: the view block's vacated word follows the batch's last press (a recorded write, before the batch is applied)
    if (opt_.vacated_clicks) {
      int32_t v = -1;
      for (size_t k = 0; k < n; ++k)
        if (queue_[k].handler == 1 || queue_[k].handler == 4)
          v = std::find(vacated_seq_.begin(), vacated_seq_.end(), queue_seq_[k]) != vacated_seq_.end() ? 1 : 0;
      if (v >= 0 && portsim::view_map(ctx) && ctx.mem.load<int32_t>(portsim::kPortViewVacatedVa) != v) {
        ctx.mem.store<int32_t>(portsim::kPortViewVacatedVa, v);
        if (writes) writes->wrote(portsim::kPortViewVacatedVa, static_cast<uint32_t>(v));
        ++vacated_writes_;
      }
      if (n > 0) {
        const uint32_t last = queue_seq_[n - 1];
        vacated_seq_.erase(std::remove_if(vacated_seq_.begin(), vacated_seq_.end(), [&](uint32_t s) { return s <= last; }),
                           vacated_seq_.end());
      }
    }
    queue_.erase(queue_.begin(), queue_.begin() + static_cast<std::ptrdiff_t>(n));
    // S7.4: the dispatch log (which input messages this dispatch carries, and the pump call that took it)
    const uint32_t di = static_cast<uint32_t>(dispatch_rows_.size());
    const uint32_t s0 = n ? queue_seq_[0] : 0, s1 = n ? queue_seq_[n - 1] : 0;
    queue_seq_.erase(queue_seq_.begin(), queue_seq_.begin() + static_cast<std::ptrdiff_t>(n));
    for (EventRow &er : event_rows_)
      if (er.seq >= s0 && er.seq <= s1 && er.ncmds) er.dispatch = static_cast<int32_t>(di);
    dispatch_rows_.push_back({di, p.frame, p.body ? 1u : 0u, body_call, static_cast<uint32_t>(p.kind), p.calls, p.cursor_reads,
                              p.may_deliver ? 1u : 0u, p.must_deliver ? 1u : 0u, static_cast<uint32_t>(n), s0, s1});
    delivered_cmds_ += n;
    ++deliveries_;
    return portsim::LiveReply::Deliver;
  };
  // P6 / H1, and a world load's pump (PumpKind::Load, loadFlic: what arrived before the load began): never waits
  if ((p.body && !p.must_deliver) || static_cast<uint32_t>(p.kind) == 3) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!queue_.empty() && p.may_deliver && (!h1 || h1_takes())) return deliver();
    return portsim::LiveReply::Decline;
  }
  // a modal Blocking pump: what the loop drew is on screen before the host waits
  const bool paced = p.pace_site != 0;
  if (!p.body) present(ctx, paced ? 'p' : 'm');
  std::unique_lock<std::mutex> lk(mu_);
  const uint32_t start = timeGetTime();
  bool waited = false;
  for (;;) {
    if (stopping()) return portsim::LiveReply::Stop;
    const uint32_t now = timeGetTime();
    if (!queue_.empty()) {
      if (p.may_deliver || p.must_deliver) {  // must_deliver without may_deliver: applied, counted by the core
        if (!p.may_deliver) ++stalls_;
        last_pass_ms_ = now;
        events_seen_ = events_;
        return deliver();
      }
      // the replay takes it at a later call of this loop (the core's simulation): one more pass first
    }
    const bool may_decline = paced || p.idle_ok;
    if (may_decline) {
      const uint32_t gap = now - last_pass_ms_;
      const bool moved = events_ != events_seen_ || !queue_.empty();
      const bool idle_due = opt_.idle_ms != 0 && now - start >= opt_.idle_ms;
      // P5A: a timed loop (LivePump::timed_pass_ms, courseActionMenu) passes once per its original period, input or not
      const bool timed = p.timed_pass_ms != 0 && opt_.timed_passes;
      const uint32_t period = timed ? std::max(opt_.pass_ms, p.timed_pass_ms) : opt_.pass_ms;
      if (gap >= period && (paced || timed || moved || idle_due)) {
        last_pass_ms_ = now;
        events_seen_ = events_;
        if (!paced) ++idle_passes_;
        if (waited) {
          ++waits_;
          wait_ms_total_ += now - start;
        }
        return portsim::LiveReply::Decline;
      }
      uint32_t ms = gap < period ? period - gap : 0;
      if (!paced && !timed && !moved) ms = std::max(ms, opt_.idle_ms ? opt_.idle_ms - std::min(opt_.idle_ms, now - start) : 250u);
      waited = true;
      cv_.wait_for(lk, std::chrono::milliseconds(std::max(1u, ms)));
      continue;
    }
    // must_deliver with nothing queued: the player acts next
    waited = true;
    cv_.wait_for(lk, std::chrono::milliseconds(250));
  }
}

// S7.8 (U-1642): the frame code ran Window::refreshFull, where the original's back buffer reaches the window (host.h)
void LiveSession::present_point(portsim::Ctx &ctx) {
  if (opt_.defect == HostDefect::NoPresentPoint) return;  // negative control: S7.5's present points only
  present(ctx, 'r');
  w_check_ = false;
}

void LiveSession::frame_end(portsim::Ctx &, uint32_t frame) {
  if (terrain_log) terrain_log->frame_end();
  memory.maybe(frame, terrain_log ? terrain_log->length() : 0);
}

void LiveSession::wait_point(portsim::Ctx &ctx) {
  // the frame's own work up to its wait (S7.8: the 'r' present at the frame's Window::refreshFull included, as the original's
  // Display::present is part of its frame)
  const uint32_t work = timeGetTime() - head_t_;
  // S7.8: the back buffer changes only at Window::refreshFull (presented there) and inside modal screens; with neither since the
  // frame's last 'r' present the wait has nothing new to show (LiveTracePresenter checks every wait: 0 'w' presents, 8_notes.md)
  if (w_check_) present(ctx, 'w');
  else ++w_skipped_;
  if (opt_.defect == HostDefect::PacingOff) return;  // negative control: no frame wait
  WaitClock clk(mu_, cv_, stop_);
  const uint32_t flags = ctx.mem.load<uint32_t>(portsim::state_va(ctx, &portsim::GameState::g_flags));
  WaitStats ws;
  {
    const ProfScope prof(Phase::Wait);  // P10A
    ws = frame_wait(clk, head_t_, flags, nullptr);
  }
  if (waits_log_) {  // live frame, work ms, the wait's first read and its last read relative to the head, waitTicks steps, target,
                     // and (S7.5) the LiveTerrain's cumulative terrain passes and pixels drawn (0 0 with headless terrain)
    const long long tp = terrain_view ? static_cast<long long>(terrain_view->passes) : 0;
    const long long tx = terrain_view ? static_cast<long long>(terrain_view->pixels_drawn) : 0;
    std::fprintf(waits_log_, "%u\t%u\t%u\t%u\t%u\t%u\t%lld\t%lld\n", frame_, work, ws.frame_ms, ws.end - head_t_, ws.wait_ticks,
                 frame_target(head_t_, flags) - head_t_, tp, tx);
    std::fflush(waits_log_);
  }
}

LiveSession::Result LiveSession::run(portsim::Ctx &ctx) {
  Result r;
  period_ = timeBeginPeriod(1) == TIMERR_NOERROR;
  // the sim thread wakes from its frame-wait sleeps ahead of normal-priority work on a loaded PC (U-1655); what it computes
  // does not depend on it (every outside value is recorded)
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  frames_log_ = std::fopen((fs::path(rec_) / "frames.tsv").string().c_str(), "wb");
  waits_log_ = std::fopen((fs::path(rec_) / "waits.tsv").string().c_str(), "wb");
  portsim::LiveOptions o;
  o.frame_ra = 0;
  o.max_records = opt_.max_records;
  o.scenario = "simgolf_port live";
  last_pass_ms_ = timeGetTime();
  RecordStreams streams;  // S7.8 (U-1643)
  if (!opt_.in_memory) streams.open(rec_, o);
  memory.sample(0, terrain_log ? terrain_log->length() : 0);
  r.code = run_guarded(&ctx, this, &o, &r.live, &r.detail);
  memory.sample(frame_, terrain_log ? terrain_log->length() : 0);
  if (terrain_log) {
    tlog_calls_ = static_cast<int64_t>(terrain_log->calls());
    tlog_max_ = static_cast<int64_t>(terrain_log->max_len);
    tlog_end_ = static_cast<int64_t>(terrain_log->length());
    tlog_kept_ = terrain_log->keep;
  }
  if (period_) timeEndPeriod(1);
  if (frames_log_) {
    std::fclose(frames_log_);
    frames_log_ = nullptr;
  }
  if (waits_log_) {
    std::fclose(waits_log_);
    waits_log_ = nullptr;
  }
  // the recording (S7.8: already on disk unless in_memory: trace.trace spilled by the core, framehash.bin per record)
  const fs::path rec(rec_);
  const size_t nsim = portsim::objects_of("sim").size();
  if (!streams.on) {
    write_file(rec / "trace.trace", r.live.trace.data(), r.live.trace.size());
    const std::vector<uint8_t> fh = framehash_bytes(r.live);
    write_file(rec / "framehash.bin", fh.data(), fh.size());
  }
  streams.close();
  std::string js = "{\"classes\":[\"sim\"],\"source\":\"simgolf_port live\",\"symbols\":[";
  for (size_t i = 0; i < nsim; ++i) js += std::string(i ? "," : "") + "\"" + portsim::objects_of("sim")[i]->name + "\"";
  js += "]}\n";
  write_file(rec / "framehash.json", js.data(), js.size());
  std::string pt = "index\tmark\tpos\tkind\tframe\trecords\thash\n";
  for (const Present &pr : presents_) {
    const uint32_t pos = pr.mark < r.live.mark_pos.size() ? r.live.mark_pos[pr.mark] : 0;
    char line[160];
    std::snprintf(line, sizeof line, "%u\t%u\t%u\t%c\t%u\t%u\t%016llx\n", pr.index, pr.mark, pos, pr.kind, pr.frame, pr.records,
                  static_cast<unsigned long long>(pr.hash));
    pt += line;
  }
  write_file(rec / "presents.tsv", pt.data(), pt.size());
  {  // S7.4: input_dispatch.tsv / dispatches.tsv / body_calls.tsv (log/roadmap/S7/4_notes.md: the original's cross-check)
    std::lock_guard<std::mutex> lk(mu_);
    std::string s = "seq\tmsg\twp\tlp\tarrival_frame\tncmds\tdispatch\n";
    char line[160];
    for (const EventRow &e : event_rows_) {
      std::snprintf(line, sizeof line, "%u\t%u\t%u\t%u\t%u\t%u\t%d\n", e.seq, e.msg, e.wp, e.lp, e.frame, e.ncmds, e.dispatch);
      s += line;
    }
    write_file(rec / "input_dispatch.tsv", s.data(), s.size());
    s = "index\tframe\tbody\tbody_call\tkind\tcalls\tcursor_reads\tmay\tmust\tncmds\tfirst_seq\tlast_seq\n";
    for (const DispatchRow &d : dispatch_rows_) {
      std::snprintf(line, sizeof line, "%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n", d.index, d.frame, d.body, d.body_call,
                    d.kind, d.calls, d.cursor_reads, d.may, d.must, d.ncmds, d.first_seq, d.last_seq);
      s += line;
    }
    write_file(rec / "dispatches.tsv", s.data(), s.size());
    if (body_calls_frame_) body_call_counts_.push_back({body_calls_seen_frame_, body_calls_frame_});
    s = "frame\tbody_calls\n";
    for (const auto &bc : body_call_counts_) {
      std::snprintf(line, sizeof line, "%u\t%u\n", bc.first, bc.second);
      s += line;
    }
    write_file(rec / "body_calls.tsv", s.data(), s.size());
  }
  if (!last_presented_.empty()) write_file(rec / "back.555", last_presented_.data(), last_presented_.size() * 2);
  const std::string mt = memory.tsv();  // S7.8
  write_file(rec / "memory.tsv", mt.data(), mt.size());
  void *h;
  {
    std::lock_guard<std::mutex> lk(mu_);
    h = hwnd_;
  }
  if (h) PostMessageW(static_cast<HWND>(h), kMsgEnded, 0, 0);
  return r;
}

std::string LiveSession::summary_json(const Result &r) const {
  const portsim::LiveResult &l = r.live;
  std::string s = "{\"code\":" + std::to_string(r.code) + ",\"detail\":\"" + jesc(r.detail.substr(0, 2000)) + "\"";
  s += ",\"stop\":\"" + jesc(l.stop) + "\",\"records\":" + std::to_string(l.records);
  s += ",\"violations\":" + std::to_string(l.violations) + ",\"first_violation\":" + std::to_string(l.first_violation);
  s += ",\"first_violation_what\":\"" + jesc(l.first_violation_what) + "\"";
  s += ",\"asks\":" + std::to_string(l.asks) + ",\"reasks\":" + std::to_string(l.reasks);
  s += ",\"dispatches\":" + std::to_string(l.dispatches) + ",\"clocks\":" + std::to_string(l.clocks);
  s += ",\"cursors\":" + std::to_string(l.cursors) + ",\"asyncs\":" + std::to_string(l.asyncs);
  s += ",\"body_calls\":" + std::to_string(l.body_calls) + ",\"modal_calls\":" + std::to_string(l.modal_calls);
  s += ",\"paced_calls\":" + std::to_string(l.paced_calls) + ",\"idle_declines\":" + std::to_string(l.idle_declines);
  s += ",\"inflated\":" + std::to_string(l.inflated) + ",\"marks\":" + std::to_string(l.mark_pos.size());
  // P5B: the close button (item 3) and the cursor guard (item 1)
  s += ",\"close_prompt\":" + std::to_string(opt_.close_prompt ? 1 : 0) + ",\"close_flag_writes\":" + std::to_string(close_written_) +
       ",\"close_at_menu\":" + std::to_string(close_direct_) + ",\"close_cancelled\":" + std::to_string(close_cancelled_) +
       ",\"close_exit\":{\"title_exits\":" + std::to_string(exit_title_) + ",\"esc_posted\":" + std::to_string(exit_esc_) +
       ",\"fallback_stops\":" + std::to_string(exit_fallback_) + "}";
  s += ",\"undo_writes\":" + std::to_string(undo_writes_);  // P11D
  // P6B item 2: the world loads (ms since the session object was made; from_input = since the last input message)
  s += ",\"loading_overlay\":" + std::to_string(opt_.loading_overlay ? 1 : 0) + ",\"loads\":[";
  for (size_t i = 0; i < loads_.size(); ++i) {
    const LoadRow &lr = loads_[i];
    s += std::string(i ? "," : "") + "{\"start_ms\":" + std::to_string(lr.start_ms) + ",\"dur_ms\":" + std::to_string(lr.dur_ms) +
         ",\"pumps\":" + std::to_string(lr.pumps) + ",\"from_input_ms\":" + std::to_string(lr.from_input_ms) +
         ",\"decode_ms\":" + std::to_string(lr.decode_ms) + ",\"decode_calls\":" + std::to_string(lr.decode_calls) +
         ",\"frame\":" + std::to_string(lr.frame) + "}";
  }
  s += "]";
  s += ",\"edge_guard\":" + std::to_string(opt_.edge_guard ? 1 : 0) + ",\"guarded_cursor_reads\":" + std::to_string(guarded_reads_);
  s += ",\"presents\":" + std::to_string(presents_.size()) + ",\"idle_passes\":" + std::to_string(idle_passes_);
  s += ",\"presents_by_kind\":{\"r\":" + std::to_string(made_[0]) + ",\"w\":" + std::to_string(made_[1]) +
       ",\"m\":" + std::to_string(made_[2]) + ",\"p\":" + std::to_string(made_[3]) + "}";
  s += ",\"present_points\":{\"r\":" + std::to_string(opportunities_[0]) + ",\"w\":" + std::to_string(opportunities_[1]) +
       ",\"m\":" + std::to_string(opportunities_[2]) + ",\"p\":" + std::to_string(opportunities_[3]) + "}";
  s += ",\"r_superseded\":" + std::to_string(r_superseded_) + ",\"w_skipped\":" + std::to_string(w_skipped_);
  s += ",\"presents_band\":{\"extend\":" + std::to_string(presents_extend_) + ",\"black\":" + std::to_string(presents_black_) + "}";  // P7
  s += ",\"p8b\":{\"world\":" + std::to_string(opt_.world ? 1 : 0) + ",\"wide_edge\":" + std::to_string(opt_.wide_edge ? 1 : 0) +
       ",\"presents_world\":" + std::to_string(presents_world_) + ",\"presents_layered\":" + std::to_string(presents_layered_) + ",\"edge_reads\":" + std::to_string(edge_reads_) +
       ",\"edge_clamped\":" + std::to_string(edge_clamped_) + ",\"wide\":" + std::to_string(opt_.wide ? 1 : 0) +
       ",\"view_writes\":" + std::to_string(view_writes_) + ",\"world_resizes\":" + std::to_string(world_resizes_) +
       ",\"view_width\":" + std::to_string(applied_w_) + ",\"hud_remapped\":" +
       std::to_string(opt_.remap ? opt_.remap->remapped() : 0) + "}";  // P8B / G32
  s += ",\"p9c\":{\"text_native\":" + std::to_string(opt_.text_native ? 1 : 0) + ",\"presents_text\":" +
       std::to_string(presents_text_) + ",\"runs\":" + std::to_string(text_runs_) + "}";  // P9C
  s += ",\"p9e\":{\"vacated_clicks\":" + std::to_string(opt_.vacated_clicks ? 1 : 0) + ",\"vacated_writes\":" +
       std::to_string(vacated_writes_) + "}";  // P9E fix 3
  s += ",\"p10c\":{\"title_bands\":" + std::to_string(opt_.title_bands ? 1 : 0) + ",\"title_reposts\":" +
       std::to_string(title_reposts_) + "}";  // P10C fix 1
  s += ",\"memory\":" + memory.json();
  if (tlog_calls_ >= 0)
    s += ",\"terrain_log\":{\"calls\":" + std::to_string(tlog_calls_) + ",\"max_len\":" + std::to_string(tlog_max_) +
         ",\"len_end\":" + std::to_string(tlog_end_) + ",\"kept\":" + (tlog_kept_ ? "true" : "false") + "}";
  s += ",\"waits\":" + std::to_string(waits_) + ",\"wait_ms\":" + std::to_string(wait_ms_total_);
  s += ",\"deliveries\":" + std::to_string(deliveries_) + ",\"delivered_commands\":" + std::to_string(delivered_cmds_);
  s += ",\"stalls\":" + std::to_string(stalls_) + ",\"trace_bytes\":" + std::to_string(l.trace.size() + l.trace_file_bytes) +
       ",\"trace_streamed\":" + (l.trace_file_bytes ? "true" : "false") + ",\"spilled_events\":" + std::to_string(l.spilled_events) +
       ",\"held_events\":" + std::to_string(l.held_events) + "}";
  return s;
}

// ------------------------------------------------------------------------------------------------ S7.5: the trace presenter
void LiveTracePresenter::boot(portsim::Ctx &ctx) {
  inner_->writes = writes;  // the trace's DELIVER w32 writes are recorded through the core's log
  inner_->marker = marker;
  inner_->boot(ctx);
}

bool LiveTracePresenter::frame_head(portsim::Ctx &ctx, uint32_t frame, uint32_t &t) {
  frame_ = frame;
  const bool ok = inner_->frame_head(ctx, frame, t);
  // G32 (--view-width): the view block's width written once, at the first frame head, as the live session's host writes it
  if (ok && view_width > 0 && !view_written_ && portsim::view_map(ctx)) {
    ctx.mem.store<int32_t>(portsim::kPortViewWidthVa, view_width);
    if (writes) writes->wrote(portsim::kPortViewWidthVa, static_cast<uint32_t>(view_width));
    view_written_ = true;
  }
  return ok;
}

void LiveTracePresenter::present(portsim::Ctx &ctx, char kind) {
  const bool was_pending = r_pending_;
  if (kind != 'r') r_pending_ = false;
  if (!reg_call(ctx, "gfx.dump", 0)) return;
  const int64_t n = ctx.files_ptr ? ctx.files().size("$gfx/back.555") : -1;
  if (n != static_cast<int64_t>(kLogicalW) * kLogicalH * 2) return;
  std::vector<uint16_t> px(static_cast<size_t>(n / 2));
  ctx.files().read("$gfx/back.555", 0, px.data(), static_cast<uint64_t>(n));
  if (px == last_) return;
  ++made[kind_index(kind)];
  if (kind == 'r') {
    if (was_pending) ++r_superseded;
    r_pending_ = true;
  }
  uint32_t snaps = 0;
  while (snaps < snaps_requested_ && ctx.files().size("$gfx/snapshot_" + std::to_string(snaps) + ".555") >= 0) ++snaps;
  presents_.push_back({static_cast<uint32_t>(presents_.size()), frame_, snaps, kind, frame_hash(px)});
  if (on_present) on_present(ctx, static_cast<uint32_t>(presents_.size() - 1), px);  // P8B
  last_ = std::move(px);
}

portsim::LiveReply LiveTracePresenter::pump(portsim::Ctx &ctx, const portsim::LivePump &p, std::vector<portsim::Command> &out) {
  // where LiveSession::pump presents: a modal pump the host is asked about, not a world load's (PumpKind 3)
  if (!p.body && static_cast<uint32_t>(p.kind) != 3) present(ctx, p.pace_site != 0 ? 'p' : 'm');
  return inner_->pump(ctx, p, out);
}

void LiveTracePresenter::frame_end(portsim::Ctx &ctx, uint32_t frame) {
  inner_->frame_end(ctx, frame);
  if (terrain_log) terrain_log->frame_end();
  memory.maybe(frame, terrain_log ? terrain_log->length() : 0);
}

void LiveTracePresenter::present_point(portsim::Ctx &ctx) {
  if (!no_present_point) present(ctx, 'r');
  inner_->present_point(ctx);
}

void LiveTracePresenter::wait_point(portsim::Ctx &ctx) {
  present(ctx, 'w');
  inner_->wait_point(ctx);
}

}  // namespace portapp
