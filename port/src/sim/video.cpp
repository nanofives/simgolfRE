// The video module (roadmap S8.3, decision D2): playAudioFile and the BinkPlayer object g_bink, behind a MovieHost
// (portsim/sim/video.h). Written from the C3 / C4 notes re/analysis/audio/0043cce0_playAudioFile.md and
// re/analysis/video/00487050_BinkPlayer_setTarget.md, 00487060_BinkPlayer_close.md, 00487090_BinkPlayer_play.md,
// 00487180_BinkPlayer_frame.md, the pumps' notes (re/analysis/ui/00483bd0, 00483c90, 00483cf0) and S8.2's measurements
// (re/analysis/video/movie_runs.md). The skip path and what a decoder host must reproduce: re/analysis/video/skip_path.md.
//
// Port conventions. The display's window, the Bink handle and the buffer are host values (pointer class in the GameState,
// never compared): the port stores the host's window and the tokens 1 / 1 for an open handle / buffer. The root repaint
// (Window::paint with 0x80007fff) and the cursor show / hide are drawing / host services. resourceQuery's result (the
// sound argument of play: binkw32's sound system, U-1713) only selects a binkw32 call, so the module passes 0, the value of
// every recorded run (a device-less sound library). Evidence: the emulator oracle (port/test/test_video.py, SG_EMU_ORACLE=1)
// runs the original playAudioFile / BinkPlayer::play / close under Unicorn over the same states and compares the data image
// and eax; the default suite checks the pinned digests (port/test/golden_video.json) and the boot runs of the intro-ON
// scenarios.
#include "portsim/sim/video.h"

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   playAudioFile     1 = the deadline word is not cleared after the play (playAudioFile+0x84 skipped)
//                     2 = the skip path sets the cursor flag g_ui83ab9c (as if play had reached its frame loop)
//                     3 = the skip path is not taken: an unavailable movie runs the frame loop as if it had opened
//                     4 = the closing movie is dropped (a non-zero `immediate` returns at once, like an absent file)
//                     5 = the intro's deadline is one millisecond early (the clock + 0x1b57)
int g_negctl_play_audio_file = 0;
int g_negctl_video_test = 0;   // the test-only entries have no wrong body

// The timeGetTime read playAudioFile makes for the intro's deadline, as an offset from mainLoop's frame-head return address
// (InputSource::clock's key): its return address follows `call [timeGetTime]` (re/analysis/audio/0043cce0, the read at
// playAudioFile+0x1a whose return is playAudioFile+0x20). S8.2's intro-ON trace holds one tgt record there (value 1048577).
constexpr int32_t kSiteDeadlineClock = 0x2d0d1;
constexpr uint32_t kDeadlineBudget = 0x1b58;        // 7000 ms (playAudioFile+0x20, `add eax, 0x1b58`)
constexpr int32_t kNoTarget = 7, kNotOpened = 1;    // BinkPlayer::play's answers (BinkPlayer::play+0xa, +0xdb)
// BinkPlayer fields (re/model/include/sections/e2.h BinkPlayer): +4 Bink handle, +8 buffer, +0xc target, +0x10 / +0x14 the
// offsets given to _BinkBufferSetOffset
constexpr uint32_t kHandle = 4, kBuffer = 8, kTarget = 0xc, kOffX = 0x10, kOffY = 0x14;
// doAllTasks / 483c90 / 483cf0 store these in g_inputPhase while their message loop runs, and 0 after it
constexpr uint32_t kPhase483bd0 = 0x3f, kPhase483c90 = 2, kPhase483cf0 = 8;

std::mutex &mu() {
  static std::mutex m;
  return m;
}
std::map<const Ctx *, MovieHost *> &hosts() {
  static std::map<const Ctx *, MovieHost *> m;
  return m;
}
std::map<const Ctx *, std::unique_ptr<MovieHost>> &owned() {
  static std::map<const Ctx *, std::unique_ptr<MovieHost>> m;
  return m;
}

uint32_t call_registered(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  const FuncDef *f = registry_at(i, nullptr);
  uint32_t v[16] = {0};
  int k = 0;
  for (uint32_t a : args)
    if (k < 16) v[k++] = a;
  Ret r;
  f->fn(ctx, v, r);
  return r.eax;
}

std::string read_cstr(Ctx &ctx, uint32_t va) {
  std::string s;
  for (uint32_t p = va;; ++p) {
    const uint8_t c = ctx.mem.load<uint8_t>(p);
    if (c == 0) break;
    s.push_back(static_cast<char>(c));
  }
  return s;
}

// One pump of input as jgld's message loop delivers it (the port's model, commands.h MODAL BLOCKS): the phase word while
// the loop runs, the batch the context's InputSource gives a Drain pump (a trace run gives none here: the recordings
// deliver no input inside a movie, S8.2 U-1710), then 0.
void pump(Ctx &ctx, uint32_t phase) {
  GameState &gs = ctx.gs();
  gs.g_inputPhase = phase;
  if (InputSource *src = input_source(ctx)) {
    std::vector<Command> b;
    if (src->batch(PumpKind::Drain, b))
      for (const Command &c : b) apply_command(ctx, c);
  }
  gs.g_inputPhase = 0;
}

}  // namespace

void set_movie_host(Ctx &ctx, MovieHost *host) {
  std::lock_guard<std::mutex> l(mu());
  if (host)
    hosts()[&ctx] = host;
  else
    hosts().erase(&ctx);
}

MovieHost &movie_host(Ctx &ctx) {
  static SkipMovieHost skip;
  std::lock_guard<std::mutex> l(mu());
  auto it = hosts().find(&ctx);
  return it == hosts().end() ? static_cast<MovieHost &>(skip) : *it->second;
}

void bink_player_set_target(Ctx &ctx, uint32_t self, uint32_t target) {
  ctx.mem.store<uint32_t>(self + kTarget, target);   // setTarget: this+0xc = the argument, nothing else
}

int32_t bink_player_close(Ctx &ctx, uint32_t self) {
  MovieHost &h = movie_host(ctx);
  int32_t r = 0;
  if (ctx.mem.load<uint32_t>(self + kHandle) != 0) h.close_movie(ctx);       // _BinkClose of an open handle
  if (ctx.mem.load<uint32_t>(self + kBuffer) != 0) r = h.close_buffer(ctx);  // _BinkBufferClose: its result is eax
  ctx.mem.store<uint32_t>(self + kHandle, 0);
  ctx.mem.store<uint32_t>(self + kBuffer, 0);
  return r;
}

int32_t bink_player_play(Ctx &ctx, uint32_t self, uint32_t path, int32_t x, int32_t y, uint32_t sound) {
  (void)sound;   // non-zero: _BinkSetSoundSystem before the open (binkw32's audio, no state); the host's concern
  const int mode = SG_NEGCTL(g_negctl_play_audio_file);
  GameState &gs = ctx.gs();
  const uint32_t target = ctx.mem.load<uint32_t>(self + kTarget);
  if (target == 0) return kNoTarget;
  MovieHost &h = movie_host(ctx);
  MovieInfo info;
  const bool opened = h.open(ctx, read_cstr(ctx, path), info);
  ctx.mem.store<uint32_t>(self + kHandle, opened ? 1u : 0u);   // the handle (NULL on the skip path)
  if (!opened && mode != 3) {
    if (mode == 2) gs.g_ui83ab9c = 1;   // negative control: the cursor flag set on the skip path
    return kNotOpened;                  // the skip path: nothing else is touched
  }
  const bool buffered = opened ? h.open_buffer(ctx, target, info) : true;
  ctx.mem.store<uint32_t>(self + kBuffer, buffered ? 1u : 0u);
  if (!buffered) return kNotOpened;
  ctx.mem.store<int32_t>(self + kOffX, x);
  ctx.mem.store<int32_t>(self + kOffY, y);
  h.show_cursor(ctx, false);
  gs.g_ui83ab9c = 1;                    // the run flag the root-window handlers clear (BinkPlayer::play+0x94)
  for (;;) {
    if (h.wait(ctx)) {                  // not due yet: the two input pumps, then ask again
      pump(ctx, kPhase483c90);
      pump(ctx, kPhase483cf0);
      continue;
    }
    if (gs.g_ui83ab9c == 0) break;      // a key or click (deadline passed or none) ended the movie
    if (!h.frame(ctx)) break;           // the clip has ended
  }
  h.show_cursor(ctx, true);
  return 0;
}

int32_t play_audio_file(Ctx &ctx, uint32_t path, int32_t immediate) {
  const int mode = SG_NEGCTL(g_negctl_play_audio_file);
  GameState &gs = ctx.gs();
  if (call_registered(ctx, "fileExists", {path}) == 0) return 0;   // an absent file: nothing else runs (eax = that 0)
  if (mode == 4 && immediate != 0) return 0;                       // negative control: the closing movie dropped
  if (immediate == 0) {
    // the intro: the deadline is the clock + 7 s (the root-window handlers clear the run flag only after it)
    InputSource *src = input_source(ctx);
    uint32_t t = 0;
    if (src == nullptr || !src->clock(kSiteDeadlineClock, t)) throw NeedInput{{"playAudioFile"}};
    gs.g_audio5aa774 = t + kDeadlineBudget - (mode == 5 ? 1u : 0u);
  } else {
    gs.g_audio5aa774 = 0;               // the closing movie: no deadline, the first key or click ends it
  }
  MovieHost &h = movie_host(ctx);
  const uint32_t bink = ctx.mem.va_of(&gs.g_bink);
  bink_player_set_target(ctx, bink, h.window(ctx));
  pump(ctx, kPhase483bd0);              // doAllTasks
  // Window::paint(root, 0x80007fff): the root repaint before the movie (drawing)
  bink_player_play(ctx, bink, path, 0, 0, 0);
  const int32_t r = bink_player_close(ctx, bink);
  if (mode != 1) gs.g_audio5aa774 = 0;
  return r;
}

// ------------------------------------------------------------------------------------------------ test hosts
namespace {

// RECORDER (video.attachRecorder): the skip host that also logs every call it gets; video.log writes the log into the files
// overlay as `$video/log.txt` (one line per call: `open <path>`, `close_movie`, `close_buffer`).
class RecorderHost : public SkipMovieHost {
 public:
  std::vector<std::string> log;
  bool open(Ctx &, const std::string &path, MovieInfo &) override {
    log.push_back("open " + path);
    return false;
  }
  void close_movie(Ctx &) override { log.push_back("close_movie"); }
  int32_t close_buffer(Ctx &) override {
    log.push_back("close_buffer");
    return 1;
  }
};

// SCRIPTED (video.attachScripted(frames, waits, flags)): a decoder stand-in. open answers a movie of `frames` frames (frame
// counter 1 after the open, as binkw32's FrameNum); each frame is due after `waits` not-due answers of wait(); frame() is
// BinkPlayer::frame's counter test (equal counters = the clip ended, no NextFrame). flags: bit 0 the open fails (a NULL
// handle), bit 1 the buffer open fails, bit 2 the display has no window (0), bit 3 the open fails (a handle whose error word
// is set: for the module the same unavailable movie). test_video.py's oracle answers binkw32's imports with the same script.
class ScriptedHost : public MovieHost {
 public:
  uint32_t frames = 1, waits = 0, flags = 0;
  uint32_t frame_num = 0, wait_left = 0, passes = 0;
  uint32_t window(Ctx &) override { return (flags & 4) ? 0u : 1u; }
  bool open(Ctx &, const std::string &, MovieInfo &info) override {
    if (flags & 9) return false;
    info.width = 800;
    info.height = 600;
    info.frames = frames;
    frame_num = 1;
    wait_left = waits;
    return true;
  }
  bool open_buffer(Ctx &, uint32_t, const MovieInfo &) override { return (flags & 2) == 0; }
  bool wait(Ctx &) override {
    if (wait_left == 0) {
      wait_left = waits;
      return false;
    }
    --wait_left;
    ++passes;
    return true;
  }
  bool frame(Ctx &) override {
    if (frame_num >= frames) return false;
    ++frame_num;
    return true;
  }
  int32_t close_buffer(Ctx &) override { return 1; }
};

// TEST INPUT (video.setInput(clock, key_pass, key_t)): the InputSource of a unit test. clock() at playAudioFile's deadline
// site answers `clock` once; Drain pumps are counted (1 = playAudioFile's doAllTasks, then two per not-due pass of the
// frame loop: doAllKeys, doAllMouse) and, with key_pass k > 0, the k-th pass's doAllMouse (Drain number
// 1 + 2k) delivers one rootWin_onKey (Esc) whose deadline test reads the clock key_t; every other pump delivers nothing.
// The oracle delivers the same key through the k-th doMouse (only doAllMouse reaches it).
class TestInput : public InputSource {
 public:
  uint32_t clock_t = 0, key_pass = 0, key_t = 0;
  bool clock_used = false;
  uint32_t drains = 0;
  bool batch(PumpKind kind, std::vector<Command> &out) override {
    out.clear();
    if (kind != PumpKind::Drain) return false;
    ++drains;
    if (key_pass == 0 || drains != 1 + 2 * key_pass) return false;
    Command c;
    c.kind = CmdKind::RootHandler;
    c.handler = 7;   // rootWin_onKey (WM_CHAR): char, code
    c.a0 = 0x1b;
    c.a1 = 0x1b;
    c.t = key_t;
    out.push_back(c);
    return true;
  }
  bool exhausted() const override { return true; }
  bool clock(int32_t site, uint32_t &t) override {
    if (site != kSiteDeadlineClock || clock_used) return false;
    clock_used = true;
    t = clock_t;
    return true;
  }
};

std::map<const Ctx *, RecorderHost *> &recorders() {
  static std::map<const Ctx *, RecorderHost *> m;
  return m;
}
std::map<const Ctx *, std::unique_ptr<TestInput>> &inputs() {
  static std::map<const Ctx *, std::unique_ptr<TestInput>> m;
  return m;
}

void attach_owned(Ctx &c, std::unique_ptr<MovieHost> h) {
  set_movie_host(c, h.get());
  std::lock_guard<std::mutex> l(mu());
  owned()[&c] = std::move(h);
}

void a_play_audio_file(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(play_audio_file(c, a[0], static_cast<int32_t>(a[1]))); }
void a_play(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(bink_player_play(c, a[0], a[1], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), a[4]));
}
void a_set_target(Ctx &c, const uint32_t *a, Ret &r) {
  bink_player_set_target(c, a[0], a[1]);
  r.set_u32(a[1]);   // the argument is loaded into eax for the store (`mov eax, [esp + 4]`), and eax is left so at `ret 4`
}
void a_close(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(bink_player_close(c, a[0])); }

void a_attach_recorder(Ctx &c, const uint32_t *, Ret &r) {
  auto h = std::make_unique<RecorderHost>();
  recorders()[&c] = h.get();
  attach_owned(c, std::move(h));
  r.set_i32(0);
}
void a_attach_scripted(Ctx &c, const uint32_t *a, Ret &r) {
  auto h = std::make_unique<ScriptedHost>();
  h->frames = a[0];
  h->waits = a[1];
  h->flags = a[2];
  recorders().erase(&c);
  attach_owned(c, std::move(h));
  r.set_i32(0);
}
void a_detach(Ctx &c, const uint32_t *, Ret &r) {
  set_movie_host(c, nullptr);
  recorders().erase(&c);
  std::lock_guard<std::mutex> l(mu());
  owned().erase(&c);
  r.set_i32(0);
}
void a_set_input(Ctx &c, const uint32_t *a, Ret &r) {
  auto in = std::make_unique<TestInput>();
  in->clock_t = a[0];
  in->key_pass = a[1];
  in->key_t = a[2];
  set_input_source(c, in.get());
  inputs()[&c] = std::move(in);
  r.set_i32(0);
}
void a_clear_input(Ctx &c, const uint32_t *, Ret &r) {
  auto it = inputs().find(&c);
  if (it != inputs().end()) {
    if (input_source(c) == it->second.get()) set_input_source(c, nullptr);
    inputs().erase(it);
  }
  r.set_i32(0);
}
// video.log(): the recorder's lines into `$video/log.txt`; returns the number of lines (-1 without a recorder)
void a_log(Ctx &c, const uint32_t *, Ret &r) {
  auto it = recorders().find(&c);
  if (it == recorders().end()) {
    r.set_i32(-1);
    return;
  }
  std::string s;
  for (const std::string &l : it->second->log) s += l + "\n";
  c.files().write("$video/log.txt", std::vector<uint8_t>(s.begin(), s.end()));
  r.set_i32(static_cast<int32_t>(it->second->log.size()));
}
// video.scriptedState(): the scripted host's frame counter and not-due passes, as frame_num | passes << 16
void a_scripted_state(Ctx &c, const uint32_t *, Ret &r) {
  std::lock_guard<std::mutex> l(mu());
  auto it = owned().find(&c);
  auto *s = it == owned().end() ? nullptr : dynamic_cast<ScriptedHost *>(it->second.get());
  r.set_u32(s ? (s->frame_num | (s->passes << 16)) : 0xffffffffu);
}

const FuncDef kFuncs[] = {
    {"playAudioFile", "default", "int", 2, 0, a_play_audio_file, &g_negctl_play_audio_file},
    {"BinkPlayer::play", "thiscall", "int", 5, 0, a_play, &g_negctl_play_audio_file},
    {"BinkPlayer::setTarget", "thiscall", "int", 2, 0, a_set_target, nullptr},
    {"BinkPlayer::close", "thiscall", "int", 1, 0, a_close, nullptr},
    {"video.attachRecorder", "default", "int", 0, 0, a_attach_recorder, &g_negctl_video_test},
    {"video.attachScripted", "default", "int", 3, 0, a_attach_scripted, &g_negctl_video_test},
    {"video.detach", "default", "int", 0, 0, a_detach, &g_negctl_video_test},
    {"video.setInput", "default", "int", 3, 0, a_set_input, &g_negctl_video_test},
    {"video.clearInput", "default", "int", 0, 0, a_clear_input, &g_negctl_video_test},
    {"video.log", "default", "int", 0, 0, a_log, &g_negctl_video_test},
    {"video.scriptedState", "default", "int", 0, 0, a_scripted_state, &g_negctl_video_test},
};
}  // namespace

extern const ModuleDef kModule_video = {"video", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
