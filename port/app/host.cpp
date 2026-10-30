// The live session of the Windows host (host.h, roadmap S7.6).
#include "host.h"

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

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/terrain_live.h"
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

LiveSession::LiveSession(HostOptions opt, const std::string &record_dir) : opt_(opt), rec_(record_dir) {}

LiveSession::~LiveSession() {
  if (frames_log_) std::fclose(frames_log_);
  if (waits_log_) std::fclose(waits_log_);
}

// ------------------------------------------------------------------------------------------------ window thread
void LiveSession::set_window(void *hwnd, const Viewport &vp) {
  std::lock_guard<std::mutex> lk(mu_);
  hwnd_ = hwnd;
  vp_ = vp;
}

void LiveSession::set_viewport(const Viewport &vp) {
  std::lock_guard<std::mutex> lk(mu_);
  vp_ = vp;
}

void LiveSession::on_input(const InputEvent &e) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (const portsim::Command &c : e.commands) {
      queue_.push_back(c);
      queue_seq_.push_back(e.seq);
    }
    event_rows_.push_back({e.seq, e.msg, e.wp, e.lp, frame_now_.load(), static_cast<uint32_t>(e.commands.size()), -1});
    if (e.msg >= kWmMouseMove && e.msg <= 0x20e) msg_cursor_ = Translator(e.vp).lparam_point(e.lp);
    ++events_;
  }
  cv_.notify_all();
}

void LiveSession::request_stop() {
  stop_.store(true);
  cv_.notify_all();
}

bool LiveSession::take_frame(std::vector<uint16_t> &px, uint32_t &index) {
  std::lock_guard<std::mutex> lk(frame_mu_);
  if (!frame_waiting_) return false;
  px = frame_slot_;
  index = frame_index_;
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
  {
    std::lock_guard<std::mutex> lk(frame_mu_);
    frame_slot_ = std::move(show);
    frame_index_ = index;
    frame_waiting_ = true;
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

void LiveSession::cursor(portsim::Ctx &, bool modal, int32_t &x, int32_t &y) {
  if (modal) w_check_ = true;  // S7.8
  std::lock_guard<std::mutex> lk(mu_);
  if (opt_.cursor == CursorSource::Message) {
    x = msg_cursor_.x;
    y = msg_cursor_.y;
    return;
  }
  POINT p = {0, 0};
  GetCursorPos(&p);
  if (hwnd_) ScreenToClient(static_cast<HWND>(hwnd_), &p);
  const Point l = Translator(vp_).to_logical(Point{p.x, p.y});
  x = l.x;
  y = l.y;
}

uint16_t LiveSession::async_key(portsim::Ctx &, int vk) {
  const int32_t k = key_state(static_cast<uint32_t>(vk), true);
  if (k >= 0) return static_cast<uint16_t>(k);
  return static_cast<uint16_t>(GetAsyncKeyState(vk & 0xff));
}

portsim::LiveReply LiveSession::pump(portsim::Ctx &ctx, const portsim::LivePump &p, std::vector<portsim::Command> &out) {
  if (stopping()) return portsim::LiveReply::Stop;
  if (!p.body) w_check_ = true;  // S7.8
  // one dispatch: the queued commands up to the first release whose press is in the same dispatch (a press and its release
  // are never dispatched by one pump call: real input has the player's hold between them, and the game's loops read the
  // button word between pumps; the script host's click / key chunks, live.cpp). Press: onPoint 2, onPoint2 5, onChar 8;
  // release: onPress1 1, onPress2 4, onChar0 9.
  uint32_t body_call = 0;
  if (p.body) {  // S7.4: the ordinal of this body pump call in its frame (0 = P6's pumpInput483bd0, 1 = H1's pumpMessages)
    if (p.frame != body_calls_seen_frame_) {
      if (body_calls_frame_) body_call_counts_.push_back({body_calls_seen_frame_, body_calls_frame_});
      body_calls_seen_frame_ = p.frame;
      body_calls_frame_ = 0;
    }
    body_call = body_calls_frame_++;
  }
  // H1's pumpMessages (body call 1) takes at most ONE mouse-class message per frame and never a key (roadmap S7.4, found by
  // the cross-check against the original): it drains WM_MOUSEMOVE only (PeekMessage 0x200..0x200, 0x0045c03e..0x0045c04d),
  // then frameTick483cd0 jumps to the display's slot +0x13c (0x00483ce2), jgld 0x1006b7e0: ONE PeekMessage(0x200..0x20a,
  // PM_REMOVE) at 0x1006b7ff..0x1006b811, dispatched. P6's pumpInput483bd0 loops 0x00483bb0 -> slot +0x128, jgld 0x1006b0f0
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
      if (gap >= opt_.pass_ms && (paced || moved || idle_due)) {
        last_pass_ms_ = now;
        events_seen_ = events_;
        if (!paced) ++idle_passes_;
        if (waited) {
          ++waits_;
          wait_ms_total_ += now - start;
        }
        return portsim::LiveReply::Decline;
      }
      uint32_t ms = gap < opt_.pass_ms ? opt_.pass_ms - gap : 0;
      if (!paced && !moved) ms = std::max(ms, opt_.idle_ms ? opt_.idle_ms - std::min(opt_.idle_ms, now - start) : 250u);
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
  const WaitStats ws = frame_wait(clk, head_t_, flags, nullptr);
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
  s += ",\"presents\":" + std::to_string(presents_.size()) + ",\"idle_passes\":" + std::to_string(idle_passes_);
  s += ",\"presents_by_kind\":{\"r\":" + std::to_string(made_[0]) + ",\"w\":" + std::to_string(made_[1]) +
       ",\"m\":" + std::to_string(made_[2]) + ",\"p\":" + std::to_string(made_[3]) + "}";
  s += ",\"present_points\":{\"r\":" + std::to_string(opportunities_[0]) + ",\"w\":" + std::to_string(opportunities_[1]) +
       ",\"m\":" + std::to_string(opportunities_[2]) + ",\"p\":" + std::to_string(opportunities_[3]) + "}";
  s += ",\"r_superseded\":" + std::to_string(r_superseded_) + ",\"w_skipped\":" + std::to_string(w_skipped_);
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
  return inner_->frame_head(ctx, frame, t);
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
