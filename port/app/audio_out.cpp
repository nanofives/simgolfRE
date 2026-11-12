// Audio out (audio_out.h): the shadow audio context on its own thread.
#include "audio_out.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <iterator>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/registry_list.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_exe.h"

namespace app {

namespace {
using Clock = std::chrono::steady_clock;
uint64_t ns_since(Clock::time_point t) {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t).count());
}
}  // namespace

AudioOut::~AudioOut() {
  stop();
  if (sound_log_) std::fclose(sound_log_);  // P24I
}

void AudioOut::note_error(const std::string &what) {
  std::lock_guard<std::mutex> lk(mu_);
  ++st_.failed;
  if (st_.first_error.empty()) st_.first_error = what;
  broken_ = true;
}

// one registered function by its hooks.csv / configuration name; false (and the error noted) when it threw
bool AudioOut::call_named(portsim::Ctx &c, const std::string &name, const uint32_t *args, int n, uint32_t *eax) {
  const int k = portsim::registry_find(name.c_str());
  if (k < 0) {
    note_error("not registered: " + name);
    return false;
  }
  uint32_t v[16] = {0};
  for (int i = 0; i < n && i < 16; ++i) v[i] = args[i];
  portsim::Ret r;
  char t[160];
  try {
    portsim::registry_at(k, nullptr)->fn(c, v, r);
    if (eax) *eax = r.eax;
    return true;
  } catch (const portsim::Fault &f) {
    std::snprintf(t, sizeof t, "%s: fault va 0x%08x len %u", name.c_str(), f.va, f.len);
  } catch (const portsim::MissingResource &m) {
    std::snprintf(t, sizeof t, "%s: missing %s", name.c_str(), m.what ? m.what : "?");
  } catch (const portsim::DivideError &) {
    std::snprintf(t, sizeof t, "%s: divide error", name.c_str());
  } catch (const portsim::NoReturn &e) {
    std::snprintf(t, sizeof t, "%s: no return (%s)", name.c_str(), e.where ? e.where : "?");
  } catch (const portsim::SoundHostError &e) {
    std::snprintf(t, sizeof t, "%s: sound host: %s", name.c_str(), e.what.c_str());
  } catch (const portsim::x87::X87Unmodelled &e) {
    std::snprintf(t, sizeof t, "%s: %s", name.c_str(), e.what.c_str());
  } catch (const std::exception &e) {
    std::snprintf(t, sizeof t, "%s: %s", name.c_str(), e.what());
  } catch (...) {
    std::snprintf(t, sizeof t, "%s: an exception", name.c_str());
  }
  note_error(t);
  return false;
}

bool AudioOut::start(const AudioOutOptions &o, std::string *err) {
  if (started_) return true;
  opt_ = o;
  quit_ = ready_ = broken_ = false;
  t0_ = Clock::now();  // P24I
  last_start_.clear();
  if (!o.sound_log.empty() && !sound_log_) {
    sound_log_ = std::fopen(o.sound_log.c_str(), "w");
    if (sound_log_) std::fprintf(sound_log_, "t_ms\tid\tcopied\tplayed\n");
  }
  start_error_.clear();
  th_ = std::thread([this]() { thread_main(); });
  std::unique_lock<std::mutex> lk(mu_);
  done_cv_.wait(lk, [this]() { return ready_; });
  if (!start_error_.empty()) {
    if (err) *err = start_error_;
    lk.unlock();
    th_.join();
    return false;
  }
  started_ = true;
  return true;
}

void AudioOut::thread_main() {
  // the original's timer client runs on winmm's timer thread (time-critical): above the sim thread (ABOVE_NORMAL) and the
  // normal-priority threads of a busy machine, so its 30 ms period holds under load
  if (opt_.realtime) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  auto ctx = std::make_unique<portsim::Ctx>();
  portsim::DirGameFiles files(opt_.install);
  files.set_write_through(false);  // the shadow writes nothing into the install (its stream log stays in memory)
  std::string why;
  ctx->exe_ptr = opt_.exe;
  ctx->files_ptr = &files;
  if (!ctx->mem.set_data_base(opt_.data_va)) why = "set_data_base refused";
  if (why.empty()) {
    if (!opt_.start_image.empty()) {
      if (opt_.start_image.size() != sizeof(portsim::GameState)) why = "start image is not a GameState";
      else std::memcpy(ctx->bytes(), opt_.start_image.data(), opt_.start_image.size());
    } else if (opt_.exe == nullptr) {
      why = "no exe constants";
    } else {
      opt_.exe->fill_state(ctx->gs(), portsim::ExeConstants::Fill::InitialImage);
      portsim::audio_exe_construct_holders(*ctx);
    }
  }
  if (why.empty()) {
    model_ = portsim::make_device_model();
    sink_ = std::make_unique<DsSink>(opt_.mode, *model_, opt_.hwnd);
    sink_->defect = opt_.defect;
    sink_->force_no_device = opt_.force_no_device;
    portsim::audio_exe_device_fixture(*ctx, sink_.get(), model_.get(), !opt_.start_image.empty());
  }
  {
    std::lock_guard<std::mutex> lk(mu_);
    start_error_ = why;
    ready_ = true;
  }
  done_cv_.notify_all();
  if (!why.empty()) return;

  Clock::time_point due{}, first_fire{};
  bool timer_on = false, first = true;
  for (;;) {
    Item it;
    bool have = false, quit = false;
    {
      std::unique_lock<std::mutex> lk(mu_);
      auto ready = [this]() { return quit_ || !q_.empty(); };
      if (timer_on) cv_.wait_until(lk, due, ready);
      else cv_.wait(lk, ready);
      if (!q_.empty()) {
        it = std::move(q_.front());
        q_.pop_front();
        have = true;
      } else {
        quit = quit_;
      }
    }
    if (have) {
      replay(*ctx, it);
      if (!opt_.keep_lines) model_->lines.clear();
      std::lock_guard<std::mutex> lk(mu_);
      ++finished_;
      done_cv_.notify_all();
    } else if (quit) {
      break;
    }
    if (silent_) break;  // S7.11: device-less after initSound: nothing more for this thread to do
    // the multimedia timer (realtime): one call per period, late ticks skipped beyond four periods
    const bool can = opt_.realtime && !broken_ && model_->timer_cb != 0 && model_->timer_period != 0;
    if (can && !timer_on) {
      timer_on = true;
      due = Clock::now() + std::chrono::milliseconds(model_->timer_period);
      std::lock_guard<std::mutex> lk(mu_);
      st_.period_ms = model_->timer_period;
    }
    if (!can) timer_on = false;
    if (timer_on && Clock::now() >= due) {
      const auto period = std::chrono::milliseconds(model_->timer_period);
      if (Clock::now() - due > 4 * period) {
        const auto behind = Clock::now() - due;
        due += period * static_cast<int64_t>(behind / period);  // the periods skipped, counted for the rate
        std::lock_guard<std::mutex> lk(mu_);
        ++st_.late_ticks;
        st_.skipped_periods += static_cast<uint32_t>(behind / period);
      }
      const Clock::time_point now = Clock::now();  // the tick's actual start: the rate is judged on these
      if (first) first_fire = now;
      first = false;
      {
        std::lock_guard<std::mutex> lk(mu_);
        st_.tick_span_ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - first_fire).count());
      }
      due += period;
      fire(*ctx);
      if (!opt_.keep_lines) model_->lines.clear();
    }
  }
  sample_heap(true);
  {
    std::lock_guard<std::mutex> lk(mu_);
    st_.sink = sink_->stats;
    st_.files_present = model_->file_present;  // S7.12
    st_.files_absent = model_->file_absent;
    st_.files_aliased = model_->file_aliased;
    if (opt_.keep_lines) tail_lines_.swap(model_->lines);  // S7.11: take_lines after the thread ended
    if (opt_.mode == SinkMode::Capture) {                  // S7.11: write_capture after the thread ended
      final_cap_ = sink_->capture();
      final_rate_ = sink_->capture_rate();
      final_ch_ = sink_->capture_channels();
    }
  }
  portsim::audio_release(*ctx);  // the context's audio core, exe-layer and model entries go before the context
  sink_.reset();                 // the real buffers and device (DSound)
  model_.reset();
  ctx.reset();
  std::lock_guard<std::mutex> lk(mu_);
  if (silent_) {  // S7.11: the items still queued are dropped; a waiting sync() returns
    for (const Item &it : q_) st_.dropped_requests += it.kind == 0 ? 1u : 0u;
    st_.thread_ended = true;
    q_.clear();
  }
  done_ = true;
  done_cv_.notify_all();
}

// S7.11 (U-1692): the model's heap; final_sample also stores the end state
void AudioOut::sample_heap(bool final_sample) {
  if (!model_) return;
  const portsim::DeviceHeapUsage u = model_->heap_usage();
  HeapStats h;
  h.arena_bytes = u.arena_bytes;
  h.chunks = u.chunks;
  h.live_blocks = u.live_blocks;
  h.freed_blocks = u.freed_blocks;
  h.live_bytes = u.live_bytes;
  std::lock_guard<std::mutex> lk(mu_);
  if (h.live_bytes >= st_.heap_peak.live_bytes) st_.heap_peak = h;
  if (final_sample) st_.heap = h;
}

void AudioOut::replay(portsim::Ctx &c, const Item &it) {
  if (it.kind == 2) {
    it.fn(c, *sink_, *model_);
    return;
  }
  if (broken_) {
    std::lock_guard<std::mutex> lk(mu_);
    ++st_.failed;
    return;
  }
  if (it.kind == 1) {
    fire(c);
    return;
  }
  const auto t0 = Clock::now();
  if (opt_.auto_init && !init_done_ && it.name != "arrayStaticInit_448160") {
    init_done_ = true;
    const uint32_t a[2] = {sink_->window_token(), 0x8a};  // gameMain's initSound(window, 0x8a)
    uint32_t eax = 0;
    const bool ok = call_named(c, "initSound", a, 2, &eax);
    deliver(c);
    const int32_t ready = c.gs().g_soundReady;
    std::lock_guard<std::mutex> lk(mu_);
    st_.initialised = ok;
    st_.init_eax = eax;
    st_.sound_ready = ready;
    // S7.11 NO DEVICE: initSound left g_soundReady 0 (its music-device open failed and it ran freeSoundLib): the original's
    // device-less state, in which every later sound call of the exe returns at its g_soundReady / g_soundDll test. The
    // shadow then has nothing to play: this request and every later one are dropped and the thread ends (control 3 keeps
    // replaying them instead).
    if (ok && ready == 0) {
      st_.device_less = true;
      if (opt_.control != 3) {
        ++st_.dropped_requests;
        silent_ = true;
        return;
      }
    }
  }
  const int n = static_cast<int>(it.args.size()) - (opt_.control == 2 && !it.args.empty() ? 1 : 0);
  const bool ok = !broken_ && call_named(c, it.name, it.args.data(), n);
  deliver(c);
  const uint64_t ns = ns_since(t0);
  sample_heap(false);  // S7.11: a request can load a whole sound file
  std::lock_guard<std::mutex> lk(mu_);
  if (ok) ++st_.replayed;
  st_.request_ns_total += ns;
  if (ns > st_.request_ns_max) st_.request_ns_max = ns;
}

// one call of the timer client as the multimedia timer makes it: (id, 0, user, 0, 0) stdcall. Model / Capture advance the
// model's clock first (deviceStep 4's order: clock, the "tick k" line, the call); Capture renders that time before it.
void AudioOut::fire(portsim::Ctx &c) {
  if (broken_ || model_->timer_cb == 0) return;
  const auto t0 = Clock::now();
  if (opt_.mode != SinkMode::DSound) model_->clock += opt_.tick_clock_ms;
  if (opt_.keep_lines) model_->lines.push_back("tick " + std::to_string(tick_index_));
  ++tick_index_;
  if (opt_.mode == SinkMode::Capture) sink_->render(opt_.tick_clock_ms);  // the device plays the same virtual time
  char t[96];
  try {
    portsim::snd_call_va(c, model_->timer_cb, portsim::SndAbi::Stdcall,
                         {model_->timer_id, 0, opt_.control == 1 ? 0u : model_->timer_user, 0, 0});
  } catch (const portsim::Fault &f) {
    std::snprintf(t, sizeof t, "timer: fault va 0x%08x len %u", f.va, f.len);
    note_error(t);
  } catch (const portsim::MissingResource &m) {
    note_error(std::string("timer: missing ") + (m.what ? m.what : "?"));
  } catch (...) {
    note_error("timer: exception");
  }
  deliver(c);
  const uint64_t ns = ns_since(t0);
  if ((++heap_every_ & 63u) == 0) sample_heap(false);  // S7.11 (U-1692): the heap every 64 ticks, outside the tick's time
  std::lock_guard<std::mutex> lk(mu_);
  ++st_.ticks;
  st_.tick_ns_total += ns;
  if (ns > st_.tick_ns_max) st_.tick_ns_max = ns;
}

// the messages the engine posted during the last call: to the window it subclassed (its token) -> its window procedure
// (stdcall hwnd, msg, wParam, lParam), as the game window's message pump would dispatch them; others dropped (U-1687)
void AudioOut::deliver(portsim::Ctx &c) {
  for (int round = 0; round < 8; ++round) {  // a delivered message can post more
    const std::vector<PostedMessage> ms = sink_->take_posted();
    if (ms.empty()) return;
    for (const PostedMessage &m : ms) {
      const bool to_proc = opt_.deliver_messages && m.hwnd == sink_->window_token() && sink_->subclass_proc() != 0;
      if (to_proc && !broken_) {
        try {
          portsim::snd_call_va(c, sink_->subclass_proc(), portsim::SndAbi::Stdcall, {m.hwnd, m.msg, m.wparam, m.lparam});
        } catch (...) {
          note_error("window procedure: exception");
        }
      }
      std::lock_guard<std::mutex> lk(mu_);
      ++(to_proc ? st_.delivered : st_.dropped);
    }
  }
}

void AudioOut::enqueue(Item it) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if ((!started_ && !ready_) || done_) {  // S7.11: nothing runs after the thread ended
      if (it.kind == 0 && silent_) ++st_.dropped_requests;
      return;
    }
    q_.push_back(std::move(it));
    ++queued_;
  }
  cv_.notify_all();
}

void AudioOut::attach(portsim::Ctx &sim) {
  portsim::audio_exe_set_observer(sim, [this](const char *name, const uint32_t *args, int n) { request(name, args, n); });
}
void AudioOut::detach(portsim::Ctx &sim) { portsim::audio_exe_set_observer(sim, {}); }

// P24I item 9 (Mariano: "i want to avoid playing the same sound over and over (like cranes or animals"): the ambient / object
// sounds the sim starts from the world (log/roadmap/P24/P24I_notes.md has the trigger of each, P24I_sound_table.tsv the wav):
// the animals' playHabitatSound ids (moose, seriema, sheep, alligator, gila monster, snake), the hot air balloon and the sail
// boat of drawWorldObjects' flyers, the fly by a golfer, and mainLoop's ambience (one frame in 100 draws one of lake, wind,
// desert, forest, grass, spring). Each gap is longer than its wav (the longest, the sail boat, 6.9 s).
uint32_t repeat_limit_gap_ms(uint32_t id) {
  switch (id) {
    case 0x3a: case 0x3b: case 0xc3: case 0xc4: case 0xc5: case 0xc6:  // playHabitatSound's animals
    case 0x22:                                                          // the fly
    case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21: case 0x28:   // mainLoop's ambience (kAmbience)
      return 8000;
    case 0x39:  // the hot air balloon (3.6 s)
      return 10000;
    case 0xd1:  // the sail boat (6.9 s)
      return 12000;
    default:
      return 0;
  }
}

namespace {
// the variants: mainLoop's ambience draw picks one of these six at random, so a repeat of one is played as the variant started
// longest ago instead (when that one's gap has passed too)
const uint32_t kAmbience[] = {0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x28};
bool is_ambience(uint32_t id) { return std::find(std::begin(kAmbience), std::end(kAmbience), id) != std::end(kAmbience); }
}  // namespace

// P24I: the sound id the shadow plays for this playSound request: the requested one, a variant (the ambience), or -1 when it
// is kept from the shadow (its id, and every variant, started less than its gap ago); records the start. Runs on the sim's
// thread (the observer's), the only one that touches last_start_.
int64_t AudioOut::limit_request(const uint32_t *args, int nargs) {
  if (nargs < 1) return -1;
  const uint32_t id = args[0];
  const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0_).count();
  auto due = [&](uint32_t k) {
    return k >= last_start_.size() || last_start_[k] == INT64_MIN ||
           now - last_start_[k] >= static_cast<int64_t>(repeat_limit_gap_ms(k));
  };
  int64_t play = id;
  if (opt_.repeat_limit && repeat_limit_gap_ms(id) != 0 && id < 0x1000u) {
    if (last_start_.size() <= id) last_start_.resize(id + 1, INT64_MIN);
    if (!due(id)) {
      play = -1;
      if (is_ambience(id)) {
        int64_t oldest = INT64_MAX;
        for (uint32_t k : kAmbience) {
          const int64_t t = k < last_start_.size() ? last_start_[k] : INT64_MIN;
          if (due(k) && t < oldest) {
            oldest = t;
            play = k;
          }
        }
      }
    }
    if (play >= 0) {
      if (last_start_.size() <= static_cast<uint64_t>(play)) last_start_.resize(static_cast<size_t>(play) + 1, INT64_MIN);
      last_start_[static_cast<size_t>(play)] = now;
    }
  }
  if (sound_log_)
    std::fprintf(sound_log_, "%lld\t%u\t%d\t%lld\n", static_cast<long long>(now), id, play < 0 ? 0 : silent_ ? 2 : 1,
                 static_cast<long long>(play));
  return play;
}

void AudioOut::request(const char *name, const uint32_t *args, int nargs) {
  Item it;
  it.kind = 0;
  it.name = name ? name : "";
  it.args.assign(args, args + (nargs > 0 ? nargs : 0));
  int64_t play = 0;
  if (it.name == "playSound" && nargs >= 1) {  // P24I: the repetition limiter
    play = limit_request(args, nargs);
    if (play >= 0) it.args[0] = static_cast<uint32_t>(play);
  }
  const bool limited = play < 0;
  {
    std::lock_guard<std::mutex> lk(mu_);
    ++st_.requests;
    if (limited) {  // P24I: the repetition limiter (the sim's call runs on, nothing is copied)
      ++st_.limited;
      return;
    }
    if (silent_) {  // S7.11: device-less shadow: the sim's call runs on, nothing is copied
      ++st_.dropped_requests;
      return;
    }
    if (closing_) {  // S7.11: the window is closing (close_early)
      ++st_.closed_drops;
      return;
    }
  }
  enqueue(std::move(it));
}

void AudioOut::tick(uint32_t n) {
  for (uint32_t i = 0; i < n; ++i) {
    Item it;
    it.kind = 1;
    enqueue(std::move(it));
  }
}

void AudioOut::sync() {
  std::unique_lock<std::mutex> lk(mu_);
  done_cv_.wait(lk, [this]() { return finished_ >= queued_ || !started_ || done_; });
}

void AudioOut::run(const std::function<void(portsim::Ctx &, DsSink &, portsim::DeviceModel &)> &fn) {
  if (!started_) return;  // after the thread ended, enqueue drops the item and sync returns (S7.11)
  Item it;
  it.kind = 2;
  it.fn = fn;
  enqueue(std::move(it));
  sync();
}

void AudioOut::stop() {
  if (!started_) {
    if (th_.joinable()) th_.join();
    return;
  }
  if (opt_.control == 4) return;  // S7.11 control: the thread is left running (the caller must leak this object)
  sync();
  bool init = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    init = opt_.auto_init && st_.initialised && !silent_ && !done_;  // device-less: the engine is already gone
  }
  if (init) {  // gameMain's shutdownSoundSystem after mainLoop returns
    Item it;
    it.kind = 0;
    it.name = "shutdownSoundSystem";
    enqueue(std::move(it));
    sync();
  }
  {
    std::lock_guard<std::mutex> lk(mu_);
    quit_ = true;
  }
  cv_.notify_all();
  th_.join();
  std::lock_guard<std::mutex> lk(mu_);
  started_ = ready_ = false;
  done_cv_.notify_all();
}

void AudioOut::close_early() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!started_ || closing_) return;
    closing_ = true;
    st_.closed_early = true;
  }
  stop();
}

ProcCounts proc_counts() {
  ProcCounts p;
  DWORD h = 0;
  if (GetProcessHandleCount(GetCurrentProcess(), &h)) p.handles = h;
  const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) return p;
  THREADENTRY32 te{};
  te.dwSize = sizeof te;
  const DWORD me = GetCurrentProcessId();
  // a thread whose start address lies in ntdll.dll is a system thread-pool worker (TppWorkerThread), which the loader and
  // file I/O start and retire on their own: counted in `threads`, left out of `tids`
  using QueryInfo = LONG(WINAPI *)(HANDLE, ULONG, PVOID, ULONG, PULONG);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  const auto query = reinterpret_cast<QueryInfo>(
      reinterpret_cast<void *>(GetProcAddress(ntdll, "NtQueryInformationThread")));
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
    if (te.th32OwnerProcessID == me) {
      ++p.threads;
      void *start = nullptr;
      if (HANDLE th = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID)) {
        if (query) query(th, 9 /* ThreadQuerySetWin32StartAddress */, &start, sizeof start, nullptr);
        CloseHandle(th);
      }
      HMODULE m = nullptr;
      const bool pool = start != nullptr &&
                        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                           static_cast<LPCWSTR>(start), &m) && m == ntdll;
      if (!pool) p.tids.push_back(te.th32ThreadID);
    }
  CloseHandle(snap);  // the snapshot's handle is closed before the count is used: it is not counted
  std::sort(p.tids.begin(), p.tids.end());
  return p;
}

uint32_t threads_added(const ProcCounts &before, const ProcCounts &after) {
  uint32_t n = 0;
  for (uint32_t t : after.tids) n += std::binary_search(before.tids.begin(), before.tids.end(), t) ? 0u : 1u;
  return n;
}

AudioOutStats AudioOut::stats() {
  if (started_) run([this](portsim::Ctx &, DsSink &s, portsim::DeviceModel &) {
      std::lock_guard<std::mutex> lk(mu_);
      st_.sink = s.stats;
    });
  std::lock_guard<std::mutex> lk(mu_);
  return st_;
}

std::vector<std::string> AudioOut::take_lines() {
  std::vector<std::string> out;
  run([&out](portsim::Ctx &, DsSink &, portsim::DeviceModel &m) { out.swap(m.lines); });
  std::lock_guard<std::mutex> lk(mu_);
  if (done_) out.insert(out.end(), tail_lines_.begin(), tail_lines_.end());  // S7.11: the ended thread's last lines
  tail_lines_.clear();
  return out;
}

bool AudioOut::write_capture(const std::string &wav) {
  bool ok = false;
  bool ran = false;
  run([&](portsim::Ctx &, DsSink &s, portsim::DeviceModel &) { ok = s.write_wav(wav), ran = true; });
  if (ran) return ok;
  std::lock_guard<std::mutex> lk(mu_);  // S7.11: the thread has ended (close_early, device-less): its kept capture
  return opt_.mode == SinkMode::Capture && final_rate_ != 0 && write_wav_file(wav, final_cap_, final_rate_, final_ch_);
}

std::string AudioOut::describe() {
  std::string d;
  run([&d](portsim::Ctx &, DsSink &s, portsim::DeviceModel &) { d = s.describe(); });
  return d;
}

}  // namespace app
