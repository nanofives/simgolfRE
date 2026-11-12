// Audio out (roadmap S7.10): the sim context keeps the device-less sound path of the pinned corpus (GameState unchanged);
// a SHADOW context on an audio thread runs S4's device-present path into a DsSink (ds_sink.h).
//
//   * The shadow's GameState starts as the sim's does (the exe's initialised data at the same .data address) plus the CRT's
//     device-holder constructors (audio_exe_construct_holders), so every GameState address a request carries (Snd::status's
//     Snd pointer) names the same object in the shadow. With start_image the shadow starts instead from that image prepared
//     by deviceFixture (gate a: audio_stream.device_run's start).
//   * attach(sim) installs audio_exe's request observer on the sim context: every request of the sim (playSound,
//     startSoundSlot, Snd::status, loadSoundTable, arrayStaticInit_448160 ...) is copied into a queue and replayed in the
//     shadow by its hooks.csv name with the same argument slots. The sim's own call runs on unchanged (its answer is the
//     device-less one: the shadow's answers go nowhere, U-1686).
//   * auto_init: before the first replayed request other than the CRT's arrayStaticInit_448160 the shadow runs initSound(window
//     token, 0x8a) (gameMain's call, which the port's boot does not make), and stop() runs shutdownSoundSystem (gameMain's
//     last sound call after mainLoop returns).
//   * The timer client the engine registers with timeSetEvent runs on the audio thread: at its period in real time (realtime),
//     or one call per tick() (tests, capture). The messages the engine posts to the window it subclassed are delivered to its
//     window procedure after each call; messages to no window (hwnd 0) are dropped as the original's thread queue drops them
//     (U-1687).
//   * NO DEVICE (S7.11): without an output device the sink answers the engine's device probes as such a PC does (ds_sink.h),
//     so the shadow's initSound takes the original's own failure path and leaves g_soundReady 0. The shadow then ends its
//     thread at once (nothing it could play) and the observer drops every later request; the sim is untouched as always.
// One Ctx per thread: the shadow context is created, used and released on the audio thread only. In realtime mode the thread
// runs at THREAD_PRIORITY_TIME_CRITICAL, the level of winmm's timer thread, where the original's timer client runs (S7.10).
#ifndef PORTAPP_AUDIO_OUT_H
#define PORTAPP_AUDIO_OUT_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ds_sink.h"
#include "portsim/execonst.h"
#include "portsim/registry.h"

namespace app {

struct AudioOutOptions {
  SinkMode mode = SinkMode::DSound;
  std::string install;                       // the user's install (read-only game files of the shadow)
  const portsim::ExeConstants *exe = nullptr;  // loaded (the sim's): InitialImage fill and the exe's data address
  uint32_t data_va = 0;                      // the GameState's address (the exe's .data, as the sim's)
  void *hwnd = nullptr;                      // DSound: the window for SetCooperativeLevel
  bool realtime = true;                      // the timer at its period on the audio thread; false: tick() drives it
  bool keep_lines = false;                   // keep the model's command lines (take_lines); else dropped after each call
  bool auto_init = true;
  bool deliver_messages = true;
  std::vector<uint8_t> start_image;          // non-empty: start from this image (gate a)
  uint32_t tick_clock_ms = 10;               // Model / Capture: the model clock's step per tick (deviceStep 4's 10)
  SinkDefect defect = SinkDefect::None;
  bool force_no_device = false;              // S7.11 test switch: the sink's DirectSoundCreate fails (DsSink::force_no_device)
  // test-only wrong bodies of the shadow's own plumbing: 1 the timer client is called without its user word, 2 a replayed
  // request loses its last argument slot; S7.11: 3 a device-less shadow keeps its thread and replays every request (the
  // engine's own early returns only), 4 stop() leaves the thread unjoined (detached: the leak the shutdown count must see)
  int control = 0;
  // P24I item 9: the repetition limiter (--sound-limit on|off, default on; SG_APP_P24=off: off). A playSound request of an
  // ambient / object sound (repeat_limit_gap_ms) is not copied to the shadow when the same sound id started less than its gap
  // ago (the gap covers the sound's own length, so it never restarts while it plays); mainLoop's ambience, whose six sounds
  // the game draws at random, plays the variant started longest ago instead when that one is due. Every other sound (UI, shots, cheers,
  // music, voices) is copied as before. Only the mirror is thinned: the sim's own call and GameState are untouched.
  bool repeat_limit = false;
  // P24I: a log of every playSound request (ms since start, sound id, 1 copied / 0 limited / 2 dropped device-less, the id
  // played or -1); empty = none
  std::string sound_log;
};

// P24I: the limiter's gap for a sound id in ms (0 = not limited): the ambient / object sounds only
uint32_t repeat_limit_gap_ms(uint32_t sound_id);

// S7.11: the process's handle and thread counts (GetProcessHandleCount, Toolhelp32 threads of this process)
struct ProcCounts {
  uint32_t handles = 0, threads = 0;
  std::vector<uint32_t> tids;  // the thread ids (sorted) but the system thread-pool workers (start address in ntdll.dll)
};
ProcCounts proc_counts();
uint32_t threads_added(const ProcCounts &before, const ProcCounts &after);  // ids in after that were not in before

// S7.11: the shadow model's heap (portsim::DeviceModel::heap_usage)
struct HeapStats {
  uint32_t arena_bytes = 0, chunks = 0, live_blocks = 0, freed_blocks = 0;
  uint64_t live_bytes = 0;
};

struct AudioOutStats {
  uint32_t requests = 0, replayed = 0, failed = 0, ticks = 0, delivered = 0, dropped = 0, late_ticks = 0;
  uint32_t skipped_periods = 0;  // realtime: the periods late_ticks skipped (the timer's rate counts them)
  uint64_t tick_span_ns = 0;     // realtime: wall time from the first tick's start to the last tick's start
  uint64_t tick_ns_total = 0, tick_ns_max = 0, request_ns_total = 0, request_ns_max = 0;
  uint32_t period_ms = 0;
  bool initialised = false;
  // S7.11 no device: initSound's eax and the shadow's g_soundReady after it; device_less = g_soundReady 0 after initSound
  // (the original's device-less state: every later sound call of the exe returns early), the thread then ended at once and
  // the requests after it were dropped (dropped_requests) instead of replayed
  uint32_t init_eax = 0;
  int32_t sound_ready = -1;
  bool device_less = false, thread_ended = false;
  uint32_t dropped_requests = 0;
  uint32_t limited = 0;  // P24I: playSound requests the repetition limiter kept from the shadow
  bool closed_early = false;  // S7.11: close_early() stopped the shadow (the window closed before the sim ended)
  uint32_t closed_drops = 0;  // S7.11: requests that arrived after close_early()
  HeapStats heap, heap_peak;  // S7.11 (U-1692): at the end / the largest live_bytes seen at a tick
  // S7.12: the model's mmioOpenA file names at the end (DeviceModel::file_present / file_absent / file_aliased: the last one
  // counts the course effects found under `Golf_Sfx` through sound.golfSfxAlias = 0)
  uint32_t files_present = 0, files_absent = 0, files_aliased = 0;
  std::string first_error;
  SinkStats sink;
};

class AudioOut {
 public:
  AudioOut() = default;
  ~AudioOut();
  AudioOut(const AudioOut &) = delete;
  AudioOut &operator=(const AudioOut &) = delete;

  // Starts the audio thread and waits until the shadow context is ready; false (and err) when it could not be built.
  bool start(const AudioOutOptions &o, std::string *err = nullptr);
  bool running() const { return started_; }
  bool leaks_thread() const { return opt_.control == 4 && started_; }  // S7.11 control 4: stop() left the thread running
  // The sim context's mirror (audio_exe_set_observer); call detach before the sim context goes.
  void attach(portsim::Ctx &sim);
  void detach(portsim::Ctx &sim);
  void request(const char *name, const uint32_t *args, int nargs);  // queue one request (the observer's path)
  void tick(uint32_t n);   // queue n timer ticks (realtime = false)
  void sync();             // wait until every queued item has run
  void stop();             // drain, shutdownSoundSystem (auto_init, initialised), end the thread, release the shadow
  // S7.11: stop() from another thread than the sim's while the sim still runs (the window is closing): requests that
  // arrive from now on are dropped (closed_drops), then stop(). Not concurrent with stop(): the caller serialises them.
  void close_early();
  AudioOutStats stats();
  std::vector<std::string> take_lines();  // the model's lines so far (keep_lines)
  bool write_capture(const std::string &wav);
  std::string describe();
  // run fn on the audio thread with the shadow context (tests, tools) and wait for it
  void run(const std::function<void(portsim::Ctx &, DsSink &, portsim::DeviceModel &)> &fn);

 private:
  struct Item {
    int kind = 0;  // 0 request, 1 tick, 2 function
    std::string name;
    std::vector<uint32_t> args;
    std::function<void(portsim::Ctx &, DsSink &, portsim::DeviceModel &)> fn;
  };
  void thread_main();
  void enqueue(Item it);
  void replay(portsim::Ctx &c, const Item &it);
  void fire(portsim::Ctx &c);
  void deliver(portsim::Ctx &c);
  void note_error(const std::string &what);
  bool call_named(portsim::Ctx &c, const std::string &name, const uint32_t *args, int n, uint32_t *eax = nullptr);
  void sample_heap(bool final_sample);

  AudioOutOptions opt_;
  std::thread th_;
  std::mutex mu_;
  std::condition_variable cv_, done_cv_;
  std::deque<Item> q_;
  uint64_t queued_ = 0, finished_ = 0;
  bool started_ = false, quit_ = false, ready_ = false, broken_ = false;
  bool done_ = false;                  // S7.11: the thread has ended (device-less shadow, or after stop)
  std::atomic<bool> silent_{false};    // S7.11: device-less: requests are dropped at the observer
  bool closing_ = false;               // S7.11: close_early(): requests are dropped (under mu_)
  uint32_t heap_every_ = 0;
  std::vector<std::string> tail_lines_;  // S7.11: keep_lines: the model's lines when the thread ended
  std::vector<int16_t> final_cap_;       // S7.11: Capture: the capture when the thread ended (close_early, device-less)
  uint32_t final_rate_ = 0, final_ch_ = 0;
  std::string start_error_;
  AudioOutStats st_;
  std::unique_ptr<portsim::DeviceModel> model_;
  std::unique_ptr<DsSink> sink_;
  uint32_t tick_index_ = 0;
  bool init_done_ = false;
  // P24I: the repetition limiter's last start per sound id (ms since t0_) and the request log
  std::vector<int64_t> last_start_;
  std::chrono::steady_clock::time_point t0_ = std::chrono::steady_clock::now();
  std::FILE *sound_log_ = nullptr;
  int64_t limit_request(const uint32_t *args, int nargs);
};

}  // namespace app

#endif  // PORTAPP_AUDIO_OUT_H
