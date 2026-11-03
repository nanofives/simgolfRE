// simgolf_port.exe: the Windows host of the port (roadmap S7.3: the shell; S7.6: the live simulation loop).
//
//   simgolf_port.exe --install <dir> [--data <dir>]                 the game: a LIVE SESSION (host.h) in the window
//                    [--idle-ms <n>] [--pass-ms <n>] [--cursor system|message] [--max-records <n>] [--flag17 <from>:<to>]
//                    [--dpi <n>] [--no-activate] [--exit-after <ms>] [--record-dir <dir>] [--negctl <name>]
//   simgolf_port.exe --install <dir> [--data <dir>] [--present-trace <scenario | file.trace> [--corpus <dir>]
//                    [--snapshot-at <clock ordinal>] [--max-records <n>] [--put <logical>=<file>]...] [--dpi <n>]
//                    [--no-activate] [--no-window] [--exit-after <ms>] [--record-dir <dir>] [--negctl <name>]
//   simgolf_port.exe --install <dir> [--data <dir>] --live-trace <scenario | file.trace> [--corpus <dir>] [--snapshots a,b,..]
//                    [--max-records <n>] [--record-dir <dir>]   (S7.5) the live core over a corpus trace, presented like a session
//   --live-trace and the live session take --audio off|on|model|capture (S7.10; default on since S7.11: audio_out.h; on =
//   DirectSound, which falls back to the original's device-less path when no output device exists) and --audio-no-device
//   (S7.11 test switch: DirectSoundCreate answers DSERR_NODRIVER); --negctl audio-keep-thread | audio-leak-thread (S7.11)
//   every run mode takes --terrain live|headless (default live: port/src/sim/terrain_live.cpp draws the course into the gfx
//   root surface, as port/test/framegate.py attaches it) and --terrain-va <v> (the Terrain object's address, 0 = a free range)
//   and --gl raster|auto|host (roadmap S3.20, decision G13: GlRaster is the default; opt-in: auto = the host's OpenGL when the
//   startup self-check of gl_wgl.h passes, else GlRaster; host (or wgl) = the host's OpenGL without the check). Without --gl the
//   environment's SG_GL_BACKEND (the same values) chooses, else GlRaster. The choice, where it came from and its reason go to
//   stderr and to terrain_renderer.json in the record directory.
//   every run mode takes --profile original|fixed|play (S7.12 / G30: the defect switches; default play, recorded in session.json);
//   simgolf_port.exe --profile-list original|fixed|play   prints every switch and the value that profile gives it
//   simgolf_port.exe --selftest                      pacing, saves overlay and presenter unit tests ("[k/n] name PASS")
//   simgolf_port.exe --translate <input.jsonl> [--out <file>]   re-translates a session's input log offline
//   simgolf_port.exe --install <dir> --data <dir> --cat <logical> <out> | --write <logical> <file> | --remove <logical>
//                    | --list <pattern>             one operation through the saves overlay (saves.h)
//
// PRESENT-TRACE runs the port from process start over a corpus trace (sg_run_boot's path: the exe's initialised data, the
// install's files through the saves overlay, the GDI text provider and the gfx backend attached, as port/test/boot.py and
// port/test/gfx.py do) on a worker thread while the window shows black, then presents the backend's back buffer: the
// snapshot taken at --snapshot-at (gfx.snapshotAt) when given, else the back buffer at the end of the run (gfx.dump 0).
//
// LIVE SESSION (roadmap S7.6, the default): the same start (exe image, install files, GDI text, gfx) on a sim thread
// through portsim::run_live with a LiveSession host (host.h): the window's real input as root-handler commands, real-time
// pacing, a present at every frame wait and modal pump call whose back buffer changed, the session recorded. --cursor
// message answers the game's cursor reads from the last mouse message (drivers that post messages without moving the
// system cursor); --idle-ms / --pass-ms set the idle passes of the menu loops (U-1601); --no-timed-passes (P5A) makes the
// world map (courseActionMenu) wait for input again instead of passing every 50 ms as the original does; --flag17 a:b (tests) sets g_flags
// bit 17 at live frame a and clears it at b as recorded harness writes; --max-records ends the session after n records.
// The session ends when the window closes (at the next frame head or pump) or when the game quits (the window closes).
// Negative controls: --negctl pacing-off | present-late | present-point-off (host.h HostDefect; present-point-off also on
// --live-trace), terrain-log-keep (S7.8, host.h TerrainLogBound), recorder:<mode> (live.recorder, live.cpp).
// S7.8: --mem-every <n> samples the process's memory every n live frames into memory.tsv (default 1000; 0 = start and end).
//
// No original binary is loaded: golf_clean.exe is read as a file for its constants (ExeConstants) and the GameState's base
// address (its .data section's address, from the file's PE header); jgld.dll, Terrain.dll, sound.dll and binkw32.dll are
// not touched.
//
// RECORDING LAYOUT (one directory per session, <data>/recordings/<yyyymmdd-hhmmss>-<pid> unless --record-dir):
//   session.json    what ran: mode, install, data directory, trace, records, the run's status and detail, client size, dpi
//   input.jsonl     one line per input message the window received (window.h event_json: message, viewport, the host's
//                   answers in order, the translator's steps, the RootHandler commands)
//   framehash.bin   per frame head: u32 T, u32 record index, u64 sgh64 x n (the sim class objects, gamestate_meta.inc order),
//                   the layout of the captures' log/p2/p24/<scenario>/c_a/framehash.bin; framehash.json names the objects
//   back.555        the presented frame (800 x 600 RGB555 words, rows top-down); gfx_log.txt the backend's gfx.log
//   trace.trace     (S7.6, live sessions) the session's own schema v1 trace; framehash.bin is then the live run's records
//   presents.tsv    (S7.6) one row per present (host.h); shown.tsv one row per frame the window took (index, sgh64);
//   frames.tsv      (S7.6) one row per frame head (live frame, timeGetTime, g_flags), written as the session runs
//   input_dispatch.tsv (S7.4) one row per input message the session received: seq, msg, wParam, lParam (client coordinates),
//                   the live frame it arrived in, its root-handler commands, the dispatch that delivered them (-1: none)
//   dispatches.tsv  (S7.4) one row per dispatch: index, live frame, body pump (1) or modal, the body call's ordinal in the frame
//                   (0 P6, 1 H1), PumpKind, LivePump calls / cursor_reads / may / must, commands, first and last event seq;
//                   body_calls.tsv: body pump calls per live frame
//   --clock-origin <v> (S7.4) the clock answers v + ms since the first answer; --keys message: GetKeyState /
//                   GetAsyncKeyState from the key messages received (a driver that posts messages), log/roadmap/S7/4_notes.md
// The saves overlay lives beside it: <data>/files/<logical name> and <data>/deleted.lst (saves.h).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio_out.h"
#include "host.h"
#include "input_map.h"
#include "pacing.h"
#include "present.h"
#include "saves.h"
#include "window.h"

#include "portsim/arith.h"
#include "portsim/execonst.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/state_hash.h"
#include "portsim/sim/live.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_live.h"
#include "portsim/sim/tick.h"

namespace fs = std::filesystem;
using namespace portapp;

namespace portapp {
extern int g_pacing_defect;  // pacing.cpp (selftest controls)
}

namespace {

// ------------------------------------------------------------------------------------------------ small helpers
bool read_file(const std::string &p, std::vector<uint8_t> &out) {
  std::ifstream f(fs::path(p), std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}
bool write_file(const fs::path &p, const void *data, size_t n) {
  std::error_code ec;
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  if (n) f.write(static_cast<const char *>(data), static_cast<std::streamsize>(n));
  return static_cast<bool>(f);
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

// The address of the exe's .data section (ImageBase + VirtualAddress) from the file's PE header: the GameState's base, so
// the 32-bit pointers the state holds to its own objects resolve as in the original.
bool exe_data_va(const std::vector<uint8_t> &f, uint32_t &va) {
  auto u16 = [&](size_t o) { return o + 2 <= f.size() ? static_cast<uint32_t>(f[o] | f[o + 1] << 8) : 0u; };
  auto u32 = [&](size_t o) {
    return o + 4 <= f.size() ? static_cast<uint32_t>(f[o] | f[o + 1] << 8 | f[o + 2] << 16 | static_cast<uint32_t>(f[o + 3]) << 24)
                             : 0u;
  };
  if (f.size() < 0x40 || f[0] != 'M' || f[1] != 'Z') return false;
  const size_t pe = u32(0x3c);
  if (u32(pe) != 0x00004550u) return false;  // "PE\0\0"
  const uint32_t nsec = u16(pe + 6), optsz = u16(pe + 20);
  const size_t opt = pe + 24;
  if (u16(opt) != 0x10b) return false;  // PE32
  const uint32_t base = u32(opt + 28);
  for (uint32_t i = 0; i < nsec; ++i) {
    const size_t s = opt + optsz + static_cast<size_t>(i) * 40;
    if (s + 40 > f.size()) return false;
    if (std::memcmp(&f[s], ".data\0\0\0", 8) == 0) {
      va = base + u32(s + 12);
      return true;
    }
  }
  return false;
}

// FRAME records of a schema v1 trace (port/schemas/trace.md: 32-byte header + meta, then u8 type, u8 sub, u16 len, payload)
int32_t count_frames(const std::vector<uint8_t> &t) {
  if (t.size() < 32 || std::memcmp(t.data(), "SGTRACE\0", 8) != 0) return -1;
  const uint32_t hdr = t[10] | t[11] << 8;
  const uint32_t meta = t[12] | t[13] << 8 | t[14] << 16 | static_cast<uint32_t>(t[15]) << 24;
  size_t o = hdr + meta;
  int32_t n = 0;
  while (o + 4 <= t.size()) {
    const uint8_t type = t[o];
    const uint32_t len = t[o + 2] | t[o + 3] << 8;
    if (type == 0x02) ++n;
    o += 4 + len;
  }
  return n;
}

// one registry function by its hooks.csv name
bool reg_call(portsim::Ctx &ctx, const char *name, const std::vector<uint32_t> &args, std::string *why, uint32_t *eax = nullptr) {
  const int i = portsim::registry_find(name);
  if (i < 0) {
    if (why) *why = std::string("no port function ") + name;
    return false;
  }
  const portsim::FuncDef *d = portsim::registry_at(i, nullptr);
  portsim::Ret r;
  try {
    d->fn(ctx, args.data(), r);
  } catch (const portsim::MissingResource &e) {
    if (why) *why = std::string(name) + ": missing " + (e.what ? e.what : "?");
    return false;
  } catch (...) {
    if (why) *why = std::string(name) + ": failed";
    return false;
  }
  if (eax) *eax = r.eax;
  return true;
}

// ------------------------------------------------------------------------------------------------ the boot run
struct RunOut {
  int code = 0;
  int32_t done = 0;
  std::string detail;
  std::vector<uint32_t> T;
  std::vector<uint64_t> hashes;
};

int run_cpp(portsim::Ctx &ctx, const std::vector<uint8_t> &trace, int32_t maxf, RunOut &o) {
  try {
    return portsim::tick_run_frames(ctx, trace.data(), trace.size(), nullptr, 0, maxf, o.T.data(), o.hashes.data(), &o.done,
                                    &o.detail, true);
  } catch (const portsim::Fault &f) {
    o.detail = "fault at va " + std::to_string(f.va) + "; " + o.detail;
    return -3;
  } catch (const portsim::DivideError &) {
    o.detail = "divide error; " + o.detail;
    return -9;
  } catch (const portsim::NoReturn &e) {
    o.detail = std::string("no return: ") + (e.where ? e.where : "?") + "; " + o.detail;
    return -10;
  } catch (const portsim::MissingResource &e) {
    o.detail = std::string("missing: ") + (e.what ? e.what : "?") + "; " + o.detail;
    return -11;
  } catch (...) {
    o.detail = "exception; " + o.detail;
    return -8;
  }
}
int run_guarded(portsim::Ctx *ctx, const std::vector<uint8_t> *trace, int32_t maxf, RunOut *o) {
  __try {
    return run_cpp(*ctx, *trace, maxf, *o);
  } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO || GetExceptionCode() == EXCEPTION_INT_OVERFLOW
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return -9;
  }
}

struct Options {
  std::string install, exe, data, record_dir, trace, corpus, translate_in, translate_out, negctl;
  std::vector<std::pair<std::string, std::string>> puts;
  int64_t snapshot_at = -1;
  int32_t max_records = -1;
  bool in_memory = false;                // S7.8: --in-memory (S7.5's way: records held until the end)
  int32_t mem_every = 1000;             // S7.8: --mem-every (memory.tsv sampling period in live frames)
  uint32_t dpi = 0, exit_after = 0;
  bool activate = true, window = true, selftest = false, selftest_p5b = false;
  // S7.6 live sessions
  uint32_t idle_ms = 200, pass_ms = 10;
  bool timed_passes = true;  // P5A (--no-timed-passes)
  std::string cursor = "system";
  int32_t flag17_from = -1, flag17_to = -1;
  int64_t clock_origin = -1;  // S7.4
  std::string keys = "system";
  // P5B (log/roadmap/P5/P5B_notes.md): the window and input behaviours from Mariano's play-test, on by default;
  // SG_APP_P5B=off (port/test/conftest.py) gives S7's behaviour for all five, the switches below for each one
  bool edge_guard = true;          // --edge-guard on|off (item 1)
  std::string zoom_keys = "all";   // --zoom-keys all|numpad|off (item 2)
  bool close_prompt = true;        // --close-prompt on|off (item 3)
  bool resizable = true;           // --resizable on|off (item 4)
  std::string scale = "auto";      // --scale auto|nearest|smooth (item 4)
  // S7.5: the terrain (live = portsim::LiveTerrain: Terrain.dll's state kept by the port and its draw passes rasterised into the
  // gfx root surface, as port/test/framegate.py attaches it; headless = tick.cpp's stand-in, no terrain pixels) and the address
  // of its Terrain object (0: a free range of the address space)
  std::string terrain = "live";
  std::string audio = "on";  // S7.10: off | on | model | capture (AudioGuard); S7.11: default on
  std::string profile = "play";  // S7.12 / G30: original | fixed | play (apply_profile); --profile-list prints it and exits
  bool profile_list = false;
  bool audio_no_device = false;  // S7.11: --audio-no-device (the sink's DirectSoundCreate fails)
  void *audio_hwnd = nullptr;  // S7.10: the window for --audio on (set by run_live_window)
  uint32_t terrain_va = 0;
  std::string gl;                        // S3.20: --gl raster|auto|host|wgl, the terrain renderer of a live terrain ("" = not given)
  std::string live_trace;                // --live-trace: the live core over a corpus trace (LiveTracePresenter)
  std::vector<int64_t> snapshot_list;    // --snapshots a,b,c: gfx.snapshotAt ordinals of a --live-trace run
  std::string op;  // cat / write / remove / list
  std::vector<std::string> op_args;
};

// P5B item 4: --scale
ScaleMode scale_mode(const std::string &s) {
  return s == "smooth" ? ScaleMode::Smooth : s == "auto" ? ScaleMode::Auto : ScaleMode::Nearest;
}

struct PresentResult {
  bool ok = false;
  std::string error;
  std::vector<uint16_t> frame;
  RunOut run;
  uint32_t data_va = 0;
  std::string gfx_log;
};

// S7.5: `--negctl reg:<registry name>=<mode>` sets a port function's negative-control switch for the run (e.g.
// reg:gfx.family.terrain=3: the terrain body family skips its draw passes, gfx_terrain.cpp); restored by the guard
struct RegNegctl {
  int *sw = nullptr;
  int prev = 0;
  explicit RegNegctl(const std::string &spec) {
    if (spec.rfind("reg:", 0) != 0) return;
    const size_t eq = spec.find('=');
    if (eq == std::string::npos) return;
    const int i = portsim::registry_find(spec.substr(4, eq - 4).c_str());
    if (i < 0) return;
    sw = portsim::registry_at(i, nullptr)->negctl;
    if (!sw) return;
    prev = *sw;
    *sw = std::atoi(spec.c_str() + eq + 1);
  }
  ~RegNegctl() {
    if (sw) *sw = prev;
  }
  RegNegctl(const RegNegctl &) = delete;
  RegNegctl &operator=(const RegNegctl &) = delete;
};

// S7.12: the defect profile of a run (port/test/profiles.py; the switches are process-wide, portsim/registry.h DefectDef).
// `fixed` (the default) = every switch 1, then the members below; `original` = every switch 1 (the oracle). The members are
// re/port/defect_profiles.tsv's rows, compiled in because the LAN build ships no repo file; port/test/test_defects_golfsfx.py
// compares them with the TSV through --profile-list. Each run's session.json records the profile and every switch's value
// (profile_json), and port/test/app_replay.py replays a recording under the switches it recorded.
struct ProfileMember {
  const char *sw;
  int value;
};
const ProfileMember kFixedProfile[] = {
    {"placeHabitat.clusterLoopBound", 0}, {"spawnObject.cellSearchBound", 0}, {"loadGolfPro.divideAndRejectGuard", 0},
    {"placeAnimal.zeroFrameCount", 0},    {"replaceInText.tailLimit", 0},     {"generateRandomCourse.searchBound", 0},
    {"loadGolfPro.negativeWindowBound", 0}, {"generateRandomCourse.placementBound", 0}, {"flic.treeAliasNames", 0},
    {"sound.golfSfxAlias", 0},
};
// G30 (2026-11-03): `play` = every `fixed` member plus these, re/port/play_profile.tsv's rows (gameplay fixes: the port diverges from the
// original on purpose; `fixed` stays hangs and faults only). The app's default.
const ProfileMember kPlayProfile[] = {
    {"sinScaled.readPastTable", 0},       {"shotPower.cacheIgnoresPutt", 0}, {"chooseShotTarget.strictFlagSticky", 0},
    {"prepareShot.hazardCellTransposed", 0}, {"narrateShot.maskNotRestored", 0},
    {"loadGolferRoster.namedStoryStride", 0},
    {"loadGolferRoster.skillsNextRecord", 0},
    {"mainLoop.cashLowWordOnly", 0},
    {"showFinancialReport.sumWriteBack", 0},
    {"emitGolferThought.tileMoodWrap", 0},
    {"resetGolferState.singleHoleRecord", 0},
    {"spawnWalker.fullTable", 0},
    {"ballScreenList.overflow", 0},
    {"cornerLevel.unclampedAndShared", 0},
    {"loadStoryStrings.cacheEntryLimit", 0},
    {"buildGolferComment.slotLimit", 0},
    {"postEvent.idBound", 0},
    {"mainLoop.titleExitHotspotWide", 0},           // P5A (Mariano's play-test, 2026-11-03)
    {"showThemePackSelect.column5StackWord", 0},    // P5A
};

// every switch back to 1, then the profile's members; "" or why it was refused (nothing changed then). The negative control
// `--negctl profile-ignored` records the profile but leaves every switch at 1 (the app as before S7.12).
std::string apply_profile(const std::string &p, bool ignored) {
  if (p != "original" && p != "fixed" && p != "play") return "unknown profile " + p + " (original | fixed | play)";
  const bool fixed = p == "fixed" || p == "play", play = p == "play";
  if (fixed)
    for (const ProfileMember &m : kFixedProfile)
      if (portsim::defect_find(m.sw) < 0) return std::string("profile fixed names a switch the library lacks: ") + m.sw;
  if (play)
    for (const ProfileMember &m : kPlayProfile)
      if (portsim::defect_find(m.sw) < 0) return std::string("profile play names a switch the library lacks: ") + m.sw;
  for (int i = 0; i < portsim::defect_count(); ++i) *portsim::defect_at(i, nullptr)->flag = 1;
  if (fixed && !ignored)
    for (const ProfileMember &m : kFixedProfile) *portsim::defect_at(portsim::defect_find(m.sw), nullptr)->flag = m.value;
  if (play && !ignored)
    for (const ProfileMember &m : kPlayProfile) *portsim::defect_at(portsim::defect_find(m.sw), nullptr)->flag = m.value;
  return "";
}

// session.json's fields: the profile's name and every switch with the value the run used
std::string profile_json(const std::string &p) {
  std::string s = ",\"profile\":\"" + p + "\",\"defects\":{";
  for (int i = 0; i < portsim::defect_count(); ++i) {
    const portsim::DefectDef *d = portsim::defect_at(i, nullptr);
    s += std::string(i ? "," : "") + "\"" + d->name + "\":" + std::to_string(*d->flag);
  }
  return s + "}";
}

// --profile-list <original | fixed | play>: every switch and the value the profile gives it, "name<TAB>value" lines
int run_profile_list(const std::string &p) {
  const std::string err = apply_profile(p, false);
  if (!err.empty()) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 2;
  }
  for (int i = 0; i < portsim::defect_count(); ++i) {
    const portsim::DefectDef *d = portsim::defect_at(i, nullptr);
    std::printf("%s\t%d\n", d->name, *d->flag);
  }
  return 0;
}

// S7.5: the terrain of a run (Options::terrain), attached after gfx.attach as port/test/framegate.py does (gfx.attach,
// gfx.snapshotAt, sg_ts_live_attach); detached when the guard goes, before the context it was declared after.
struct TerrainGuard {
  portsim::Ctx *ctx = nullptr;
  std::unique_ptr<portsim::LiveTerrain> live;
  TerrainGuard() = default;
  TerrainGuard(const TerrainGuard &) = delete;
  TerrainGuard &operator=(const TerrainGuard &) = delete;
  void attach(const Options &o, portsim::Ctx &c, const fs::path &rec) {
    if (o.terrain != "live") return;
    ctx = &c;
    live = std::make_unique<portsim::LiveTerrain>(c, o.terrain_va, std::vector<uint32_t>{});
    // S3.20 (decision G13, final): the terrain renderer, chosen before the first Terrain call: --gl, else SG_GL_BACKEND, else
    // GlRaster; logged with where the choice came from and its reason
    std::string mode = o.gl, source = "--gl";
    if (mode.empty()) {
      const char *be = std::getenv("SG_GL_BACKEND");
      const std::string e = be ? be : "";
      if (e == "raster" || e == "auto" || e == "host" || e == "wgl") {
        mode = e;
        source = "SG_GL_BACKEND";
      } else {
        mode = "raster";
        source = "default";
      }
    }
    const portsim::LiveTerrain::RendererChoice &ch = live->choose_renderer(mode);
    const std::string took = ch.checked ? " [self-check " + std::to_string(static_cast<int>(ch.check.ms)) + " ms]" : "";
    std::fprintf(stderr, "simgolf_port: terrain renderer %s (%s %s): %s%s\n", ch.backend.c_str(), source.c_str(), mode.c_str(),
                 ch.reason.c_str(), took.c_str());
    auto esc = [](const std::string &t) {
      std::string r;
      for (char k : t) {
        if (k == '"' || k == '\\') r += '\\';
        r += k;
      }
      return r;
    };
    const std::string js = "{\"source\":\"" + esc(source) + "\",\"mode\":\"" + esc(ch.mode) + "\",\"backend\":\"" + esc(ch.backend) + "\",\"reason\":\"" + esc(ch.reason) +
                           "\",\"checked\":" + (ch.checked ? "true" : "false") + ",\"pass\":" + (ch.check.pass ? "true" : "false") +
                           ",\"got\":\"" + ch.check.got + "\",\"want\":\"" + ch.check.want + "\",\"ms\":" +
                           std::to_string(ch.check.ms) + "}\n";
    write_file(rec / "terrain_renderer.json", js.data(), js.size());
    portsim::set_terrain_api(c, live.get());
  }
  ~TerrainGuard() {
    if (live) portsim::set_terrain_api(*ctx, nullptr);
  }
};

// S7.10: audio out (audio_out.h) of a live run (Options::audio): off, on (DirectSound, the window's handle; the default since
// S7.11, device-less without an output device), model
// (S4's device model only) or capture (a software device: <rec>/audio.wav). The sim context's sound requests are copied to a
// shadow context on the audio thread; the sim's own calls and GameState are left as they are. finish() (or the guard going)
// detaches the observer and stops the thread before the context goes; <rec>/audio.json holds the counters.
// S7.11 SHUTDOWN ORDER: the original's gameMain runs shutdownSoundSystem after mainLoop returns, before its window goes. In
// the window session the window can close first (the user, or WM_CLOSE after the game's quit), so the window is subclassed
// (audio_close_proc) and its WM_CLOSE stops the shadow (AudioOut::close_early: shutdownSoundSystem, the real buffers and
// device released, the thread joined) before DestroyWindow. g_close_mu serialises it with AudioGuard::finish.
std::mutex g_close_mu;
app::AudioOut *g_close_audio = nullptr;
WNDPROC g_close_prev = nullptr;
LRESULT CALLBACK audio_close_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_CLOSE) {
    std::lock_guard<std::mutex> lk(g_close_mu);
    if (g_close_audio) g_close_audio->close_early();
  }
  return CallWindowProcW(g_close_prev, h, msg, wp, lp);
}

struct AudioGuard {
  portsim::Ctx *ctx = nullptr;
  std::unique_ptr<app::AudioOut> out;
  std::string mode, error;
  fs::path rec;
  app::ProcCounts before;
  bool count_only = false;
  void *win = nullptr;  // S7.11: the session's window (window_at_stop)
  AudioGuard() = default;
  AudioGuard(const AudioGuard &) = delete;
  AudioGuard &operator=(const AudioGuard &) = delete;
  void attach(const Options &o, portsim::Ctx &c, const portsim::ExeConstants &ec, uint32_t data_va, const std::string &install,
              void *hwnd, const fs::path &r) {
    if (o.audio == "off") {
      // S7.11 reference for the shutdown count: --audio off --negctl audio-count-only counts the same span with no shadow
      count_only = o.negctl == "audio-count-only";
      rec = r;
      before = app::proc_counts();
      return;
    }
    app::AudioOutOptions ao;
    ao.mode = o.audio == "model" ? app::SinkMode::Model : o.audio == "capture" ? app::SinkMode::Capture : app::SinkMode::DSound;
    ao.install = install;
    ao.exe = &ec;
    ao.data_va = data_va;
    ao.hwnd = hwnd;
    ao.tick_clock_ms = 30;  // the timer's period: the model / capture device plays it between ticks
    ao.force_no_device = o.audio_no_device;                                               // S7.11
    ao.control = o.negctl == "audio-keep-thread" ? 3 : o.negctl == "audio-leak-thread" ? 4 : 0;  // S7.11 controls
    mode = o.audio;
    rec = r;
    before = app::proc_counts();  // S7.11: the process before the audio thread exists
    out = std::make_unique<app::AudioOut>();
    if (!out->start(ao, &error)) {
      out.reset();
      return;
    }
    ctx = &c;
    out->attach(c);
    win = hwnd;
    if (hwnd) {  // S7.11: the window session's WM_CLOSE stops it first (audio_close_proc)
      std::lock_guard<std::mutex> lk(g_close_mu);
      g_close_audio = out.get();
    }
  }
  void finish() {
    if (count_only) {
      count_only = false;
      const app::ProcCounts after = app::proc_counts();
      char b[160];
      const int n = std::snprintf(b, sizeof b,
                                  "{\"mode\":\"off\",\"handles_before\":%u,\"threads_before\":%u,\"handles_after\":%u,"
                                  "\"threads_after\":%u,\"threads_added\":%u}\n", before.handles, before.threads,
                                  after.handles, after.threads, app::threads_added(before, after));
      if (n > 0) write_file(rec / "audio.json", b, static_cast<size_t>(std::min(n, static_cast<int>(sizeof b) - 1)));
    }
    if (!out) {
      if (!error.empty()) write_file(rec / "audio.json", ("{\"error\":\"" + error + "\"}\n").data(), error.size() + 13);
      return;
    }
    {
      std::lock_guard<std::mutex> lk(g_close_mu);  // S7.11: no close_early from now on (and none running)
      if (g_close_audio == out.get()) g_close_audio = nullptr;
    }
    out->detach(*ctx);
    if (mode == "capture") out->write_capture((rec / "audio.wav").string());
    const bool win_now = win != nullptr && IsWindow(static_cast<HWND>(win));
    out->stop();
    const app::AudioOutStats s = out->stats();
    // S7.11: whether the window still existed when the shadow stopped (-1 no window; close_early runs inside WM_CLOSE)
    const int window_at_stop = win == nullptr ? -1 : (s.closed_early || win_now) ? 1 : 0;
    if (out->leaks_thread()) (void)out.release();  // S7.11 control audio-leak-thread: the running thread keeps its object
    out.reset();
    const app::ProcCounts after = app::proc_counts();  // S7.11: the thread joined, the device and its buffers released
    char b[1536];
    const int n = std::snprintf(
        b, sizeof b,
        "{\"mode\":\"%s\",\"requests\":%u,\"replayed\":%u,\"failed\":%u,\"ticks\":%u,\"delivered\":%u,"
        "\"dropped\":%u,\"late_ticks\":%u,\"skipped_periods\":%u,\"tick_span_ns\":%llu,\"tick_ns_total\":%llu,"
        "\"tick_ns_max\":%llu,\"buffers\":%u,\"real_buffers\":%u,\"real_failures\":%u,\"init_eax\":%u,\"sound_ready\":%d,"
        "\"device_less\":%s,\"thread_ended\":%s,\"dropped_requests\":%u,\"closed_early\":%s,\"closed_drops\":%u,\"window_at_stop\":%d,\"no_device_answers\":%u,"
        "\"heap_arena_bytes\":%u,\"heap_live_bytes\":%llu,\"heap_live_blocks\":%u,\"heap_peak_live_bytes\":%llu,"
        "\"heap_peak_arena_bytes\":%u,\"handles_before\":%u,\"threads_before\":%u,\"handles_after\":%u,"
        "\"threads_after\":%u,\"threads_added\":%u,\"files_present\":%u,\"files_absent\":%u,\"files_aliased\":%u,"
        "\"first_error\":\"%s\",\"first_failure\":\"%s\"}\n",
        mode.c_str(), s.requests, s.replayed, s.failed, s.ticks, s.delivered, s.dropped, s.late_ticks, s.skipped_periods,
        static_cast<unsigned long long>(s.tick_span_ns), static_cast<unsigned long long>(s.tick_ns_total),
        static_cast<unsigned long long>(s.tick_ns_max), s.sink.buffers, s.sink.real_buffers, s.sink.real_failures, s.init_eax,
        s.sound_ready, s.device_less ? "true" : "false", s.thread_ended ? "true" : "false", s.dropped_requests,
        s.closed_early ? "true" : "false", s.closed_drops, window_at_stop, s.sink.no_device_answers, s.heap.arena_bytes,
        static_cast<unsigned long long>(s.heap.live_bytes), s.heap.live_blocks,
        static_cast<unsigned long long>(s.heap_peak.live_bytes), s.heap_peak.arena_bytes, before.handles, before.threads,
        after.handles, after.threads, app::threads_added(before, after), s.files_present, s.files_absent, s.files_aliased,
        s.first_error.c_str(), s.sink.first_failure.c_str());
    if (n > 0) write_file(rec / "audio.json", b, static_cast<size_t>(std::min(n, static_cast<int>(sizeof b) - 1)));
  }
  ~AudioGuard() { finish(); }
};

// The present-trace run: everything the worker thread does.
PresentResult present_trace(const Options &o, AppFiles &files, const fs::path &rec) {
  PresentResult pr;
  std::vector<uint8_t> exe, trace;
  if (!read_file(o.exe, exe)) {
    pr.error = "cannot read the exe " + o.exe;
    return pr;
  }
  if (!exe_data_va(exe, pr.data_va)) {
    pr.error = "the exe has no PE32 .data section";
    return pr;
  }
  std::string why;
  portsim::ExeConstants &ec = portsim::default_exe_constants();
  if (!ec.loaded() && !ec.load_file(o.exe.c_str(), &why)) {
    pr.error = "exe refused: " + why;
    return pr;
  }
  std::string tp = o.trace;
  if (!fs::exists(tp)) tp = (fs::path(o.corpus) / (o.trace + ".trace")).string();
  if (!read_file(tp, trace)) {
    pr.error = "cannot read the trace " + tp;
    return pr;
  }
  const int32_t frames = count_frames(trace);
  if (frames < 0) {
    pr.error = "not a schema v1 trace: " + tp;
    return pr;
  }
  const int32_t maxf = o.max_records > 0 ? o.max_records : frames + 1;
  const size_t nsim = portsim::objects_of("sim").size();
  pr.run.T.assign(static_cast<size_t>(maxf), 0);
  pr.run.hashes.assign(static_cast<size_t>(maxf) * nsim, 0);

  auto ctx = std::make_unique<portsim::Ctx>();
  ctx->exe_ptr = &ec;
  ctx->files_ptr = &files;
  if (!ctx->mem.set_data_base(pr.data_va)) {
    pr.error = "set_data_base refused";
    return pr;
  }
  ec.fill_state(ctx->gs(), portsim::ExeConstants::Fill::InitialImage);
  if (!reg_call(*ctx, "gditext.attach", {}, &why) || !reg_call(*ctx, "gfx.attach", {}, &why) ||
      (o.snapshot_at >= 0 && !reg_call(*ctx, "gfx.snapshotAt", {static_cast<uint32_t>(o.snapshot_at)}, &why))) {
    pr.error = why;
    return pr;
  }
  TerrainGuard terrain;
  RegNegctl reg_negctl(o.negctl);
  terrain.attach(o, *ctx, rec);
  pr.run.code = run_guarded(ctx.get(), &trace, maxf, &pr.run);
  // the frame: the requested snapshot, else the back buffer now
  const std::string name = o.snapshot_at >= 0 ? "$gfx/snapshot_0.555" : "$gfx/back.555";
  if (o.snapshot_at < 0) reg_call(*ctx, "gfx.dump", {0}, &why);
  std::vector<uint8_t> raw(static_cast<size_t>(kLogicalW) * kLogicalH * 2);
  if (files.read(name, 0, raw.data(), raw.size()) == static_cast<int64_t>(raw.size())) {
    pr.frame.resize(raw.size() / 2);
    for (size_t i = 0; i < pr.frame.size(); ++i) pr.frame[i] = static_cast<uint16_t>(raw[2 * i] | raw[2 * i + 1] << 8);
  }
  reg_call(*ctx, "gfx.log", {}, &why);
  std::vector<uint8_t> log(static_cast<size_t>(std::max<int64_t>(0, files.size("$gfx/log.txt"))));
  if (!log.empty()) files.read("$gfx/log.txt", 0, log.data(), log.size());
  pr.gfx_log.assign(log.begin(), log.end());
  reg_call(*ctx, "gfx.detach", {}, &why);
  reg_call(*ctx, "gditext.detach", {}, &why);
  // the recording: framehash.bin / .json
  std::vector<uint8_t> fh;
  for (int32_t k = 0; k < pr.run.done; ++k) {
    const uint32_t hdr[2] = {pr.run.T[static_cast<size_t>(k)], static_cast<uint32_t>(k)};
    fh.insert(fh.end(), reinterpret_cast<const uint8_t *>(hdr), reinterpret_cast<const uint8_t *>(hdr) + 8);
    const uint8_t *h = reinterpret_cast<const uint8_t *>(&pr.run.hashes[static_cast<size_t>(k) * nsim]);
    fh.insert(fh.end(), h, h + nsim * 8);
  }
  write_file(rec / "framehash.bin", fh.data(), fh.size());
  std::string js = "{\"classes\":[\"sim\"],\"source\":\"simgolf_port present-trace\",\"symbols\":[";
  for (size_t i = 0; i < nsim; ++i) js += std::string(i ? "," : "") + "\"" + portsim::objects_of("sim")[i]->name + "\"";
  js += "]}\n";
  write_file(rec / "framehash.json", js.data(), js.size());
  write_file(rec / "gfx_log.txt", pr.gfx_log.data(), pr.gfx_log.size());
  pr.ok = !pr.frame.empty();
  if (!pr.ok) pr.error = "no frame (" + name + ")";
  return pr;
}

// ------------------------------------------------------------------------------------------------ the live path over a trace (S7.5)
int live_cpp(portsim::Ctx &ctx, portsim::LiveHost &host, const portsim::LiveOptions &opt, portsim::LiveResult &res,
             std::string &detail) {
  try {
    portsim::run_live(ctx, host, opt, res);
    return 0;
  } catch (const portsim::Fault &f) {
    detail = "fault at va " + std::to_string(f.va);
    return -3;
  } catch (const portsim::DivideError &) {
    detail = "divide error";
    return -9;
  } catch (const portsim::NoReturn &e) {
    detail = std::string("no return: ") + (e.where ? e.where : "?");
    return -10;
  } catch (const portsim::MissingResource &e) {
    detail = std::string("missing: ") + (e.what ? e.what : "?");
    return -11;
  } catch (...) {
    detail = "exception";
    return -8;
  }
}
int live_guarded(portsim::Ctx *ctx, portsim::LiveHost *host, const portsim::LiveOptions *opt, portsim::LiveResult *res,
                 std::string *detail) {
  __try {
    return live_cpp(*ctx, *host, *opt, *res, *detail);
  } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO || GetExceptionCode() == EXCEPTION_INT_OVERFLOW
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return -9;
  }
}

// --live-trace: the start live_run makes (exe image, install files, GDI text, gfx, the terrain), then portsim::run_live with
// a LiveTracePresenter over the corpus trace (host.h). Writes, in the record directory: presents.tsv (index, kind, live
// frame, snapshots existing when it was made, sgh64), back.555 (the last present), snapshot_<k>.555 for each --snapshots
// ordinal, framehash.bin (the live run's records), gfx_log.txt, session.json.
int run_live_trace(const Options &o, AppFiles &files, const fs::path &rec) {
  std::string err, why, detail;
  std::vector<uint8_t> exe, trace;
  uint32_t data_va = 0;
  std::string tp = o.live_trace;
  if (!fs::exists(tp)) tp = (fs::path(o.corpus) / (o.live_trace + ".trace")).string();
  portsim::ExeConstants &ec = portsim::default_exe_constants();
  if (!read_file(o.exe, exe)) err = "cannot read the exe " + o.exe;
  else if (!exe_data_va(exe, data_va)) err = "the exe has no PE32 .data section";
  else if (!ec.loaded() && !ec.load_file(o.exe.c_str(), &why)) err = "exe refused: " + why;
  else if (!read_file(tp, trace) || count_frames(trace) < 0) err = "not a schema v1 trace: " + tp;
  portsim::LiveResult res;
  int code = 0;
  std::vector<LiveTracePresenter::Present> presents;
  std::vector<uint16_t> last;
  int64_t tpasses = -1, tpixels = -1, tcalls = -1;
  uint32_t snaps_taken = 0;
  uint64_t made[4] = {}, r_superseded = 0;  // S7.8: presents by kind r w m p, superseded 'r' presents
  std::string mem_json = "null", mem_tsv;   // S7.8 (U-1643)
  int64_t tlog_max = -1, tlog_end = -1;
  RecordStreams streams;  // S7.8: the recording and the frame records written as they are made
  bool streamed = false;
  if (err.empty()) {
    auto ctx = std::make_unique<portsim::Ctx>();
    ctx->exe_ptr = &ec;
    ctx->files_ptr = &files;
    if (!ctx->mem.set_data_base(data_va)) err = "set_data_base refused";
    if (err.empty()) {
      ec.fill_state(ctx->gs(), portsim::ExeConstants::Fill::InitialImage);
      bool ok = reg_call(*ctx, "gditext.attach", {}, &why) && reg_call(*ctx, "gfx.attach", {}, &why);
      for (int64_t s : o.snapshot_list) ok = ok && reg_call(*ctx, "gfx.snapshotAt", {static_cast<uint32_t>(s)}, &why);
      if (!ok) err = why;
    }
    if (err.empty()) {
      TerrainGuard terrain;
      RegNegctl reg_negctl(o.negctl);
      terrain.attach(o, *ctx, rec);
      auto inner = portsim::make_trace_live_host(*ctx, trace.data(), trace.size(), &why);
      if (!inner) err = "trace host refused: " + why;
      if (err.empty()) {
        LiveTracePresenter host(std::move(inner), static_cast<uint32_t>(o.snapshot_list.size()));
        host.no_present_point = o.negctl == "present-point-off";
        // P3: a present per frame at most, plus the boot's and modal screens' (51,109 for course_season_6y's 51,092 records)
        if (o.negctl != "presents-no-reserve") host.reserve_presents(static_cast<size_t>(std::max<int32_t>(0, count_frames(trace))) + 4096);
        TerrainLogBound tlog;  // S7.8 (U-1643)
        tlog.live = terrain.live.get();
        tlog.keep = o.negctl == "terrain-log-keep";
        if (tlog.live) host.terrain_log = &tlog;
        host.memory.every = static_cast<uint32_t>(std::max<int32_t>(0, o.mem_every));
        host.memory.sample(0, tlog.length());
        portsim::LiveOptions lopt;
        lopt.max_records = o.max_records;
        lopt.scenario = "simgolf_port live-trace";
        if (!o.in_memory) streamed = streams.open(rec.string(), lopt);  // S7.8 (U-1643)
        AudioGuard audio;  // S7.10
        audio.attach(o, *ctx, ec, data_va, files.install_root(), nullptr, rec);
        code = live_guarded(ctx.get(), &host, &lopt, &res, &detail);
        audio.finish();
        streams.close();
        host.memory.sample(static_cast<uint32_t>(res.records), tlog.length());
        presents = host.presents();
        last = host.last();
        if (terrain.live) {
          tpasses = terrain.live->view().passes;
          tpixels = terrain.live->view().pixels_drawn;
          tcalls = static_cast<int64_t>(tlog.calls());
          tlog_max = static_cast<int64_t>(tlog.max_len);
          tlog_end = static_cast<int64_t>(tlog.length());
        }
        for (int k = 0; k < 4; ++k) made[k] = host.made[k];
        r_superseded = host.r_superseded;
        mem_json = host.memory.json();
        mem_tsv = host.memory.tsv();
      }
      for (size_t k = 0; k < o.snapshot_list.size(); ++k) {
        const std::string name = "$gfx/snapshot_" + std::to_string(k) + ".555";
        const int64_t n = files.size(name);
        if (n <= 0) continue;
        std::vector<uint8_t> b(static_cast<size_t>(n));
        files.read(name, 0, b.data(), b.size());
        write_file(rec / ("snapshot_" + std::to_string(k) + ".555"), b.data(), b.size());
        ++snaps_taken;
      }
      reg_call(*ctx, "gfx.log", {}, &why);
      std::vector<uint8_t> log(static_cast<size_t>(std::max<int64_t>(0, files.size("$gfx/log.txt"))));
      if (!log.empty()) files.read("$gfx/log.txt", 0, log.data(), log.size());
      write_file(rec / "gfx_log.txt", log.data(), log.size());
      reg_call(*ctx, "gfx.detach", {}, &why);
      reg_call(*ctx, "gditext.detach", {}, &why);
    }
  }
  std::string pt = "index\tkind\tframe\tsnaps\thash\n";
  for (const auto &p : presents) {
    char line[128];
    std::snprintf(line, sizeof line, "%u\t%c\t%u\t%u\t%016llx\n", p.index, p.kind, p.frame, p.snaps,
                  static_cast<unsigned long long>(p.hash));
    pt += line;
  }
  write_file(rec / "presents.tsv", pt.data(), pt.size());
  if (!last.empty()) write_file(rec / "back.555", last.data(), last.size() * 2);
  if (!streamed) {  // S7.8: in memory until now (--in-memory, or the files could not be opened)
    const std::vector<uint8_t> fh = framehash_bytes(res);
    write_file(rec / "framehash.bin", fh.data(), fh.size());
    write_file(rec / "trace.trace", res.trace.data(), res.trace.size());
  }
  std::string s = "{\"app\":\"simgolf_port\",\"mode\":\"live-trace\",\"trace\":\"" + jesc(tp) + "\",\"terrain\":\"" + o.terrain + "\"";
  s += profile_json(o.profile);  // S7.12
  s += ",\"error\":\"" + jesc(err) + "\",\"code\":" + std::to_string(code) + ",\"detail\":\"" + jesc(detail.substr(0, 2000)) + "\"";
  s += ",\"stop\":\"" + jesc(res.stop) + "\",\"records\":" + std::to_string(res.records);
  s += ",\"violations\":" + std::to_string(res.violations) + ",\"presents\":" + std::to_string(presents.size());
  s += ",\"snapshots_requested\":" + std::to_string(o.snapshot_list.size()) + ",\"snapshots_taken\":" + std::to_string(snaps_taken);
  s += ",\"terrain_passes\":" + std::to_string(tpasses) + ",\"terrain_pixels\":" + std::to_string(tpixels);
  s += ",\"terrain_calls\":" + std::to_string(tcalls);
  s += ",\"presents_by_kind\":{\"r\":" + std::to_string(made[0]) + ",\"w\":" + std::to_string(made[1]) + ",\"m\":" +
       std::to_string(made[2]) + ",\"p\":" + std::to_string(made[3]) + "},\"r_superseded\":" + std::to_string(r_superseded);
  s += ",\"terrain_log_max\":" + std::to_string(tlog_max) + ",\"terrain_log_end\":" + std::to_string(tlog_end);
  s += ",\"memory\":" + mem_json;
  s += ",\"streamed\":" + std::string(streamed ? "true" : "false") + ",\"trace_bytes\":" +
       std::to_string(res.trace.size() + res.trace_file_bytes) + ",\"spilled_events\":" + std::to_string(res.spilled_events) +
       ",\"held_events\":" + std::to_string(res.held_events);
  s += ",\"data_va\":" + std::to_string(data_va) + "}\n";
  if (!mem_tsv.empty()) write_file(rec / "memory.tsv", mem_tsv.data(), mem_tsv.size());
  write_file(rec / "session.json", s.data(), s.size());
  std::printf("DONE live-trace records=%d code=%d stop=%s presents=%zu snapshots=%u\n", res.records, code, res.stop.c_str(),
              presents.size(), snaps_taken);
  if (!err.empty()) std::fprintf(stderr, "%s\n", err.c_str());
  return err.empty() ? 0 : 1;
}

// ------------------------------------------------------------------------------------------------ the live session (S7.6)
struct LiveOut {
  std::string error;
  LiveSession::Result res;
  uint32_t data_va = 0;
  int64_t terrain_calls = -1, terrain_passes = -1;  // S7.5: LiveTerrain's logged Terrain calls and draw passes (-1: headless)
};

// The sim thread of a live session: the start present_trace makes (the exe's initialised data at its .data address, the
// install's files through the saves overlay, GDI text, the gfx backend), then LiveSession::run until the session ends.
LiveOut live_run(const Options &o, AppFiles &files, LiveSession &session, const fs::path &rec) {
  LiveOut lo;
  std::vector<uint8_t> exe;
  if (!read_file(o.exe, exe)) {
    lo.error = "cannot read the exe " + o.exe;
    return lo;
  }
  if (!exe_data_va(exe, lo.data_va)) {
    lo.error = "the exe has no PE32 .data section";
    return lo;
  }
  std::string why;
  portsim::ExeConstants &ec = portsim::default_exe_constants();
  if (!ec.loaded() && !ec.load_file(o.exe.c_str(), &why)) {
    lo.error = "exe refused: " + why;
    return lo;
  }
  auto ctx = std::make_unique<portsim::Ctx>();
  ctx->exe_ptr = &ec;
  ctx->files_ptr = &files;
  if (!ctx->mem.set_data_base(lo.data_va)) {
    lo.error = "set_data_base refused";
    return lo;
  }
  ec.fill_state(ctx->gs(), portsim::ExeConstants::Fill::InitialImage);
  if (!reg_call(*ctx, "gditext.attach", {}, &why) || !reg_call(*ctx, "gfx.attach", {}, &why)) {
    lo.error = why;
    return lo;
  }
  // --negctl recorder:<mode>: the live core's recorder controls (live.cpp live.recorder)
  int *rec_mode = nullptr;
  if (o.negctl.rfind("recorder:", 0) == 0) {
    const int i = portsim::registry_find("live.recorder");
    if (i >= 0) rec_mode = portsim::registry_at(i, nullptr)->negctl;
    if (rec_mode) *rec_mode = std::atoi(o.negctl.c_str() + 9);
  }
  TerrainGuard terrain;
  RegNegctl reg_negctl(o.negctl);
  terrain.attach(o, *ctx, rec);
  session.terrain_view = terrain.live ? &terrain.live->view() : nullptr;
  TerrainLogBound tlog;  // S7.8 (U-1643)
  tlog.live = terrain.live.get();
  tlog.keep = o.negctl == "terrain-log-keep";
  session.terrain_log = tlog.live ? &tlog : nullptr;
  session.memory.every = static_cast<uint32_t>(std::max<int32_t>(0, o.mem_every));
  AudioGuard audio;  // S7.10
  audio.attach(o, *ctx, ec, lo.data_va, files.install_root(), o.audio_hwnd, rec);
  lo.res = session.run(*ctx);
  audio.finish();
  session.terrain_view = nullptr;
  session.terrain_log = nullptr;
  if (terrain.live) {
    lo.terrain_calls = static_cast<int64_t>(tlog.calls());
    lo.terrain_passes = terrain.live->view().passes;
  }
  if (rec_mode) *rec_mode = 0;
  reg_call(*ctx, "gfx.log", {}, &why);
  std::vector<uint8_t> log(static_cast<size_t>(std::max<int64_t>(0, files.size("$gfx/log.txt"))));
  if (!log.empty()) files.read("$gfx/log.txt", 0, log.data(), log.size());
  write_file(rec / "gfx_log.txt", log.data(), log.size());
  reg_call(*ctx, "gfx.detach", {}, &why);
  reg_call(*ctx, "gditext.detach", {}, &why);
  return lo;
}

int run_live_window(const Options &o, AppFiles &files, const fs::path &rec) {
  HostOptions ho;
  ho.idle_ms = o.idle_ms;
  ho.pass_ms = o.pass_ms;
  ho.timed_passes = o.timed_passes;
  ho.cursor = o.cursor == "message" ? CursorSource::Message : CursorSource::System;
  ho.flag17_from = o.flag17_from;
  ho.flag17_to = o.flag17_to;
  ho.max_records = o.max_records;
  ho.clock_origin = o.clock_origin;
  ho.keys_from_messages = o.keys == "message";
  ho.in_memory = o.in_memory;
  ho.edge_guard = o.edge_guard;      // P5B item 1
  ho.close_prompt = o.close_prompt;  // P5B item 3
  if (o.negctl == "pacing-off") ho.defect = HostDefect::PacingOff;
  if (o.negctl == "present-late") ho.defect = HostDefect::PresentLate;
  if (o.negctl == "present-point-off") ho.defect = HostDefect::NoPresentPoint;
  LiveSession session(ho, rec.string());

  PresentDefect pd = o.negctl == "present-stale" ? PresentDefect::Stale
                     : o.negctl == "present-swap" ? PresentDefect::ByteSwap
                                                  : PresentDefect::None;
  Presenter presenter(pd, scale_mode(o.scale));
  presenter.set_frame(std::vector<uint16_t>(static_cast<size_t>(kLogicalW) * kLogicalH, 0));  // black until a frame comes
  WindowOptions wo;
  wo.dpi = o.dpi;
  wo.activate = o.activate;
  wo.input_defect = o.negctl == "input-bypass" ? InputDefect::Bypass : InputDefect::None;
  wo.resizable = o.resizable;  // P5B item 4
  wo.zoom_keys = static_cast<uint8_t>(o.zoom_keys == "all" ? 2 : o.zoom_keys == "numpad" ? 1 : 0);  // P5B item 2
  wo.zoom_context = [&session]() { return session.in_frame_body(); };
  if (o.close_prompt) wo.on_close_button = [&session]() { session.request_close(); };  // P5B item 3
  wo.focus_child_takes = [&session](uint32_t msg, uint32_t wp) { return session.focus_child_takes(msg, wp); };  // U-1616
  wo.before_translate = [&session](uint32_t msg, uint32_t wp) { session.before_translate(msg, wp); };  // S7.4
  wo.key_state = [&session](uint32_t vk, bool async) { return session.key_state(vk, async); };
  AppWindow win(presenter, wo);
  if (!win.create()) {
    std::fprintf(stderr, "%s\n", win.error().c_str());
    return 4;
  }
  HWND hwnd = static_cast<HWND>(win.hwnd());
  session.set_window(hwnd, win.viewport());
  std::FILE *log = std::fopen((rec / "input.jsonl").string().c_str(), "wb");
  std::FILE *shown = std::fopen((rec / "shown.tsv").string().c_str(), "wb");
  if (shown) std::fprintf(shown, "index\thash\n");
  uint32_t events = 0;
  win.set_sink([&](const InputEvent &e) {
    ++events;
    if (log) {
      std::fprintf(log, "%s\n", event_json(e).c_str());
      std::fflush(log);
    }
    session.on_input(e);
  });
  LiveOut lo;
  Options oa = o;  // S7.10: --audio on uses the window's handle for DirectSound's cooperative level
  oa.audio_hwnd = hwnd;
  // S7.11: WM_CLOSE stops the audio before the window goes (audio_close_proc); --negctl audio-close-late leaves it to the
  // sim thread's finish, after the window is gone (the control)
  if (o.audio != "off" && o.negctl != "audio-close-late")
    g_close_prev = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&audio_close_proc)));
  std::thread sim([&]() { lo = live_run(oa, files, session, rec); });
  std::printf("READY hwnd=0x%llx client=%dx%d dpi=%u record=%s\n", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)),
              win.client_w(), win.client_h(), win.dpi(), rec.string().c_str());
  std::fflush(stdout);
  if (o.exit_after) SetTimer(hwnd, 1, o.exit_after, nullptr);
  Viewport vp = win.viewport();
  MSG m;
  std::vector<uint16_t> px;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    if (m.hwnd == hwnd && m.message == LiveSession::kMsgPresent) {
      uint32_t index = 0;
      if (session.take_frame(px, index)) {
        presenter.set_frame(px);
        win.invalidate();
        if (shown) {
          std::fprintf(shown, "%u\t%016llx\n", index, static_cast<unsigned long long>(frame_hash(presenter.frame())));
          std::fflush(shown);
        }
      }
      continue;
    }
    if (m.hwnd == hwnd && m.message == LiveSession::kMsgEnded) {  // the game quit (or failed): close the window
      PostMessageW(hwnd, WM_CLOSE, 0, 0);
      continue;
    }
    TranslateMessage(&m);  // the jgld pumps call TranslateMessage before DispatchMessageA (WM_CHAR from real keys)
    DispatchMessageW(&m);
    const Viewport now = win.viewport();  // WM_DPICHANGED refits it inside the window procedure
    if (now.origin_x != vp.origin_x || now.origin_y != vp.origin_y || now.client_w != vp.client_w || now.client_h != vp.client_h) {
      vp = now;
      session.set_viewport(vp);
    }
  }
  session.request_stop();
  sim.join();
  if (log) std::fclose(log);
  if (shown) std::fclose(shown);
  std::string s = "{\"app\":\"simgolf_port\",\"mode\":\"live\",\"install\":\"" + jesc(files.install_root()) + "\",\"data\":\"" +
                  jesc(files.data_root()) + "\"";
  s += profile_json(o.profile);  // S7.12
  s += ",\"error\":\"" + jesc(lo.error) + "\",\"data_va\":" + std::to_string(lo.data_va);
  s += ",\"idle_ms\":" + std::to_string(o.idle_ms) + ",\"pass_ms\":" + std::to_string(o.pass_ms) + ",\"cursor\":\"" + o.cursor + "\"";
  s += ",\"keys\":\"" + o.keys + "\",\"clock_origin\":" + std::to_string(o.clock_origin);
  s += ",\"p5b\":{\"edge_guard\":" + std::to_string(o.edge_guard ? 1 : 0) + ",\"zoom_keys\":\"" + o.zoom_keys +  // P5B
       "\",\"zoom_remaps\":" + std::to_string(win.zoom_remaps()) + ",\"close_prompt\":" + std::to_string(o.close_prompt ? 1 : 0) +
       ",\"resizable\":" + std::to_string(o.resizable ? 1 : 0) + ",\"scale\":\"" + o.scale + "\"}";
  s += ",\"negctl\":\"" + jesc(o.negctl) + "\",\"input_events\":" + std::to_string(events);
  s += ",\"frames_shown\":" + std::to_string(presenter.frames_set() > 1 ? presenter.frames_set() - 1 : 0);
  s += ",\"saves_refused\":" + std::to_string(files.refused());
  s += ",\"terrain\":\"" + o.terrain + "\",\"terrain_calls\":" + std::to_string(lo.terrain_calls) +
       ",\"terrain_passes\":" + std::to_string(lo.terrain_passes);
  s += ",\"client\":[" + std::to_string(win.client_w()) + "," + std::to_string(win.client_h()) + "],\"dpi\":" + std::to_string(win.dpi());
  s += ",\"session\":" + session.summary_json(lo.res) + "}\n";
  write_file(rec / "session.json", s.data(), s.size());
  if (!lo.error.empty()) std::fprintf(stderr, "%s\n", lo.error.c_str());
  return lo.error.empty() ? 0 : 1;
}

// ------------------------------------------------------------------------------------------------ selftests
struct Tests {
  std::vector<std::pair<std::string, bool>> rows;
  void add(const std::string &n, bool ok) { rows.emplace_back(n, ok); }
  int print() const {
    int bad = 0;
    for (size_t i = 0; i < rows.size(); ++i) {
      std::printf("[%zu/%zu] %s %s\n", i + 1, rows.size(), rows[i].first.c_str(), rows[i].second ? "PASS" : "FAIL");
      bad += rows[i].second ? 0 : 1;
    }
    std::printf("selftest %zu passed %d failed\n", rows.size() - static_cast<size_t>(bad), bad);
    std::fflush(stdout);
    return bad ? 1 : 0;
  }
};

void selftest_pacing(Tests &t) {
  // virtual clock, 1 ms per read (time_model.md section 3): 99 reads per frame (head 1 + wait 98), 8 waitTicks
  {
    VirtualClock c(0x00100000);
    const uint32_t head = c.now();
    const WaitStats s = frame_wait(c, head, 0, nullptr);
    t.add("pacing.frame_87ms_reads_99", s.reads + 1 == 99 && s.wait_ticks == 8 && s.frame_ms == 1);
    t.add("pacing.frame_87ms_ends_at_head_plus_98", s.end == head + 98);
    t.add("pacing.deadline_87", frame_target(head, 0) == head + 87);
  }
  {
    VirtualClock c(5000);
    const uint32_t head = c.now();
    const WaitStats s = frame_wait(c, head, kFlagSlowFrames, nullptr);
    // tests at head + 2 + 12 k; the first at or past head + 125 is k = 11 -> 1 + 1 + 11 * 12 reads in the wait
    t.add("pacing.deadline_125_with_bit17", frame_target(head, kFlagSlowFrames) == head + 125);
    t.add("pacing.frame_125ms_reads_135", s.reads + 1 == 135 && s.wait_ticks == 11);
    t.add("pacing.other_flag_bits_ignored", frame_target(head, ~kFlagSlowFrames) == head + 87);
  }
  {
    VirtualClock c(0);
    WaitStats s;
    const int r = wait_ticks(c, 1, false, nullptr, &s);
    t.add("pacing.waitTicks1_10ms_11_reads", r == 0 && s.reads == 11 && s.end == 10);
    WaitStats z;
    t.add("pacing.waitTicks0_no_read", wait_ticks(c, 0, false, nullptr, &z) == 0 && z.reads == 0);
    t.add("pacing.waitTicks_ms", wait_ticks_ms(1) == 10 && wait_ticks_ms(90) == 900 && wait_ticks_ms(60) == 600 &&
                                     wait_ticks_ms(-1) == -10);
    int pumped = 0;
    VirtualClock c2(0);
    const int ri = wait_ticks(c2, 10, true, [&]() { return ++pumped == 3; });
    t.add("pacing.waitTicks_interruptible_returns_1", ri == 1 && pumped == 3);
  }
  {
    // unsigned compare: a head close to the wrap gives a target below it and no wait (the original's jae / jb)
    VirtualClock c(0xffffffd0u);
    const uint32_t head = c.now();
    const WaitStats s = frame_wait(c, head, 0, nullptr);
    t.add("pacing.wrap_unsigned_no_wait", s.wait_ticks == 0 && s.reads == 2);
    // a frame whose work took longer than 87 ms: one test, no waitTicks
    VirtualClock c3(1000);
    const uint32_t h3 = c3.now();
    for (int i = 0; i < 200; ++i) c3.now();
    const WaitStats s3 = frame_wait(c3, h3, 0, nullptr);
    t.add("pacing.long_frame_single_test", s3.wait_ticks == 0 && s3.reads == 2 && s3.frame_ms == 201);
  }
  {
    // real clock: 20 empty frames back to back, each period >= 87 ms and the mean within one 10 ms quantum (+3 ms jitter)
    RealClock rc;
    uint32_t head = rc.now();
    const uint32_t first = head;
    uint32_t minp = 0xffffffffu, maxp = 0;
    for (int i = 0; i < 20; ++i) {
      frame_wait(rc, head, 0, nullptr);
      const uint32_t next = rc.now();
      minp = std::min(minp, next - head);
      maxp = std::max(maxp, next - head);
      head = next;
    }
    const double mean = (head - first) / 20.0;
    std::printf("pacing real clock: 20 frames, period min %u max %u mean %.1f ms, sleeps %u\n", minp, maxp, mean, rc.sleeps());
    t.add("pacing.real_clock_period_87", minp >= 87 && mean <= 100.0);
  }
}

uint64_t tree_digest(const fs::path &root) {
  // FNV-1a over every file's relative path and bytes, in sorted order
  std::vector<fs::path> files;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) files.push_back(it->path());
  std::sort(files.begin(), files.end());
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](const void *p, size_t n) {
    const uint8_t *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
  };
  for (const fs::path &p : files) {
    const std::string r = fs::relative(p, root, ec).generic_string();
    mix(r.data(), r.size());
    std::vector<uint8_t> b;
    read_file(p.string(), b);
    mix(b.data(), b.size());
  }
  return h;
}

void selftest_saves(Tests &t, SavesDefect defect) {
  char tmp[MAX_PATH];
  GetTempPathA(MAX_PATH, tmp);
  const fs::path base = fs::path(tmp) / ("simgolf_port_selftest_" + std::to_string(GetCurrentProcessId()));
  std::error_code ec;
  fs::remove_all(base, ec);
  const fs::path inst = base / "install", data = base / "data";
  const std::string orig = "original save";
  write_file(inst / "Saved Games" / "a.sve", orig.data(), orig.size());
  write_file(inst / "Saved Games" / "b.sve", "bb", 2);
  write_file(inst / "Themes" / "Default.pro", "pro", 3);
  const uint64_t before = tree_digest(inst);
  {
    AppFiles f(inst.string(), data.string(), defect);
    t.add("saves.opens", f.ok());
    const std::vector<uint8_t> w = {'n', 'e', 'w'};
    t.add("saves.write_ok", f.write("Saved Games\\c.sve", w) && f.write("Themes/Default.pro", w) && f.write("top10.sve", w));
    t.add("saves.write_lands_in_data_dir", fs::exists(data / "files" / "Saved Games" / "c.sve") &&
                                               fs::exists(data / "files" / "top10.sve"));
    std::vector<uint8_t> r(8);
    const int64_t n = f.read("saved games/c.sve", 0, r.data(), r.size());
    t.add("saves.read_back", n == 3 && r[0] == 'n' && r[2] == 'w');
    const int64_t n2 = f.read("Themes\\Default.pro", 0, r.data(), r.size());
    t.add("saves.shadow_install_file", n2 == 3 && r[0] == 'n');
    const int64_t n3 = f.read("Saved Games/a.sve", 0, r.data(), r.size());
    t.add("saves.install_read", n3 == 8 && r[0] == 'o');
    auto l = f.list("Saved Games\\*.sve");
    std::vector<std::string> names;
    for (auto &e : l) names.push_back(e.name);
    t.add("saves.list_merged_sorted", names == std::vector<std::string>({"a.sve", "b.sve", "c.sve"}));
    auto l2 = f.list("Saved Games\\*");
    t.add("saves.list_dot_entries", l2.size() == 5 && l2[0].name == "." && l2[1].name == "..");
    t.add("saves.remove_install_file_tombstone", f.remove("Saved Games/b.sve") && f.size("Saved Games/b.sve") == -1 &&
                                                     fs::exists(inst / "Saved Games" / "b.sve"));
    t.add("saves.remove_data_file", f.remove("Saved Games/c.sve") && !fs::exists(data / "files" / "Saved Games" / "c.sve"));
    t.add("saves.refuse_dotdot", !f.write("../escape.sve", w) && !f.write("Saved Games/../../x", w) &&
                                      !fs::exists(data / "escape.sve") && !fs::exists(base / "x"));
    t.add("saves.refuse_drive_and_stream", !f.write("C:\\x.sve", w) && !f.write("a.sve:stream", w) && !f.write("\\x.sve", w));
    t.add("saves.memory_names_stay_in_memory", f.write("$gfx/back.555", w) && f.size("$gfx/back.555") == 3 &&
                                                   !fs::exists(data / "files" / "$gfx"));
  }
  {
    AppFiles again(inst.string(), data.string(), defect);
    t.add("saves.tombstone_persists", again.size("Saved Games/b.sve") == -1 && again.size("top10.sve") == 3);
  }
  {
    AppFiles inside(inst.string(), (inst / "Saved Games" / "port").string(), defect);
    t.add("saves.refuse_data_inside_install", !inside.ok() && !inside.write("x.sve", {'x'}));
    AppFiles same(inst.string(), inst.string(), defect);
    t.add("saves.refuse_data_equal_install", !same.ok());
  }
  fs::remove_all(inst / "Saved Games" / "port", ec);
  t.add("saves.install_unchanged", tree_digest(inst) == before);
  fs::remove_all(base, ec);
}

void selftest_present(Tests &t, PresentDefect d) {
  // the presenter's mapping is the translator's: every client pixel shows the logical pixel to_logical maps it to
  std::vector<uint16_t> f(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) f[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  Presenter p(d);
  std::vector<uint16_t> black(f.size(), 0);
  p.set_frame(black);
  p.set_frame(f);
  for (auto [cw, ch] : std::vector<std::pair<int, int>>{{800, 600}, {1200, 900}, {2000, 1500}, {1000, 600}, {800, 700}}) {
    std::vector<uint32_t> out;
    Viewport vp;
    p.render(cw, ch, out, &vp);
    Translator tr(vp);
    bool ok = true;
    for (int y = 0; y < ch && ok; ++y)
      for (int x = 0; x < cw && ok; ++x) {
        const Point l = tr.to_logical(Point{x, y});
        const bool in = l.x >= 0 && l.x < kLogicalW && l.y >= 0 && l.y < kLogicalH && x >= vp.origin_x && y >= vp.origin_y &&
                        x < vp.origin_x + vp.client_w && y < vp.origin_y + vp.client_h;
        const uint32_t want = in ? rgb555_to_bgra(f[static_cast<size_t>(l.y) * kLogicalW + l.x]) : 0xff000000u;
        ok = out[static_cast<size_t>(y) * cw + x] == want;
      }
    t.add("present.mapping_" + std::to_string(cw) + "x" + std::to_string(ch), ok);
  }
  const Viewport a = fit_viewport(1000, 600), b = fit_viewport(800, 700);
  t.add("present.letterbox", a.origin_x == 100 && a.client_w == 800 && b.origin_y == 50 && b.client_h == 600);
  t.add("present.rgb555_expand", rgb555_to_bgra(0x7fff) == 0xffffffffu && rgb555_to_bgra(0x0421) == 0xff080808u &&
                                     rgb555_to_bgra(0x7c00) == 0xffff0000u);
}

// P5B item 4 (--selftest-p5b; separate so --selftest keeps its 37 rows): the presenter's scale modes
void selftest_p5b(Tests &t, bool wrong) {
  std::vector<uint16_t> f(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) f[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  auto render = [&](ScaleMode m, int cw, int ch, const std::vector<uint16_t> &fr) {
    Presenter p(PresentDefect::None, wrong && m == ScaleMode::Smooth ? ScaleMode::Nearest : m);  // control: smooth ignored
    p.set_frame(fr);
    std::vector<uint32_t> out;
    p.render(cw, ch, out);
    return out;
  };
  std::vector<uint16_t> flat(f.size(), 0x1234);
  {
    const auto o = render(ScaleMode::Smooth, 1200, 900, flat);
    bool ok = true;
    for (uint32_t v : o) ok = ok && v == rgb555_to_bgra(0x1234);
    t.add("p5b.smooth_uniform_frame_stays_uniform", ok);
  }
  t.add("p5b.smooth_at_1x_equals_nearest", render(ScaleMode::Smooth, 800, 600, f) == render(ScaleMode::Nearest, 800, 600, f));
  t.add("p5b.auto_at_2x_is_nearest", render(ScaleMode::Auto, 1600, 1200, f) == render(ScaleMode::Nearest, 1600, 1200, f));
  const auto a15 = render(ScaleMode::Auto, 1200, 900, f), s15 = render(ScaleMode::Smooth, 1200, 900, f),
             n15 = render(ScaleMode::Nearest, 1200, 900, f);
  t.add("p5b.auto_at_1_5x_is_smooth", a15 == s15 && s15 != n15);
  {  // column 0 black, column 1 white: client column 1 of a 2x smooth image lies between them (weight 64 / 256 of white)
    std::vector<uint16_t> g(f.size(), 0);
    for (int y = 0; y < kLogicalH; ++y) g[static_cast<size_t>(y) * kLogicalW + 1] = 0x7fff;
    const auto o = render(ScaleMode::Smooth, 1600, 1200, g);
    const uint32_t v = o[static_cast<size_t>(600) * 1600 + 1] & 0xff;
    t.add("p5b.smooth_blends_neighbours", v == 63);
  }
  {  // letterbox bars stay black in smooth mode
    const auto o = render(ScaleMode::Smooth, 1300, 900, flat);
    t.add("p5b.smooth_letterbox_bars_black", o[0] == 0xff000000u && o[static_cast<size_t>(899) * 1300 + 1299] == 0xff000000u &&
                                                o[static_cast<size_t>(450) * 1300 + 650] == rgb555_to_bgra(0x1234));
  }
}

int run_selftest(const Options &o) {
  Tests t;
  if (o.selftest_p5b) {
    selftest_p5b(t, o.negctl == "p5b-smooth-ignored");
    return t.print();
  }
  PresentDefect pd = PresentDefect::None;
  SavesDefect sd = SavesDefect::None;
  if (o.negctl == "present-stale") pd = PresentDefect::Stale;
  if (o.negctl == "present-swap") pd = PresentDefect::ByteSwap;
  if (o.negctl == "saves-install") sd = SavesDefect::WriteToInstall;
  if (o.negctl == "saves-noreadback") sd = SavesDefect::NoReadBack;
  if (o.negctl == "saves-inside") sd = SavesDefect::AllowInsideInstall;
  if (o.negctl == "pacing-no-bit17") g_pacing_defect = 1;
  if (o.negctl == "pacing-signed") g_pacing_defect = 2;
  if (o.negctl == "pacing-quantum") g_pacing_defect = 3;
  selftest_pacing(t);
  selftest_saves(t, sd);
  selftest_present(t, pd);
  return t.print();
}

// ------------------------------------------------------------------------------------------------ offline translation
int run_translate(const Options &o) {
  std::ifstream in(fs::path(o.translate_in));
  if (!in) {
    std::fprintf(stderr, "cannot read %s\n", o.translate_in.c_str());
    return 2;
  }
  std::FILE *out = o.translate_out.empty() ? nullptr : std::fopen(o.translate_out.c_str(), "wb");
  Translator tr;
  uint32_t n = 0, bad = 0, calls = 0;
  for (std::string line; std::getline(in, line);) {
    if (line.empty()) continue;
    InputEvent e;
    if (!parse_event_json(line, e)) {
      ++bad;
      continue;
    }
    ++n;
    tr.set_viewport(e.vp);
    ScriptedHost host(e.answers);
    const Translation t = tr.translate(e.msg, e.wp, e.lp, host);
    const bool same = host.ok() && steps_json(t.steps) == steps_json(e.steps);
    for (const Step &s : t.steps) calls += s.kind == Step::Kind::Call ? 1 : 0;
    if (!same) ++bad;
    if (out) std::fprintf(out, "{\"seq\":%u,\"same\":%s,\"steps\":%s}\n", e.seq, same ? "true" : "false", steps_json(t.steps).c_str());
  }
  if (out) std::fclose(out);
  std::printf("translate: messages %u handler calls %u mismatches %u\n", n, calls, bad);
  return bad ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------ overlay operations
int run_op(const Options &o, AppFiles &f) {
  if (o.op == "cat" && o.op_args.size() == 2) {
    const int64_t n = f.size(o.op_args[0]);
    if (n < 0) return 3;
    std::vector<uint8_t> b(static_cast<size_t>(n));
    if (n) f.read(o.op_args[0], 0, b.data(), b.size());
    return write_file(o.op_args[1], b.data(), b.size()) ? 0 : 4;
  }
  if (o.op == "write" && o.op_args.size() == 2) {
    std::vector<uint8_t> b;
    if (!read_file(o.op_args[1], b)) return 4;
    return f.write(o.op_args[0], b) ? 0 : 5;
  }
  if (o.op == "remove" && o.op_args.size() == 1) return f.remove(o.op_args[0]) ? 0 : 5;
  if (o.op == "list" && o.op_args.size() == 1) {
    for (const auto &e : f.list(o.op_args[0])) std::printf("%s%s\n", e.name.c_str(), e.is_dir ? "/" : "");
    return 0;
  }
  return 2;
}

std::string stamp() {
  SYSTEMTIME s;
  GetLocalTime(&s);
  char b[64];
  std::snprintf(b, sizeof b, "%04u%02u%02u-%02u%02u%02u-%lu", s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond,
                GetCurrentProcessId());
  return b;
}

bool parse(int argc, char **argv, Options &o) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto val = [&](std::string &dst) {
      if (i + 1 >= argc) return false;
      dst = argv[++i];
      return true;
    };
    std::string v;
    if (a == "--install") { if (!val(o.install)) return false; }
    else if (a == "--exe") { if (!val(o.exe)) return false; }
    else if (a == "--data") { if (!val(o.data)) return false; }
    else if (a == "--record-dir") { if (!val(o.record_dir)) return false; }
    else if (a == "--present-trace") { if (!val(o.trace)) return false; }
    else if (a == "--corpus") { if (!val(o.corpus)) return false; }
    else if (a == "--negctl") { if (!val(o.negctl)) return false; }
    else if (a == "--snapshot-at") { if (!val(v)) return false; o.snapshot_at = std::strtoll(v.c_str(), nullptr, 0); }
    else if (a == "--max-records") { if (!val(v)) return false; o.max_records = std::atoi(v.c_str()); }
    else if (a == "--in-memory") o.in_memory = true;
    else if (a == "--mem-every") { if (!val(v)) return false; o.mem_every = std::atoi(v.c_str()); }
    else if (a == "--dpi") { if (!val(v)) return false; o.dpi = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--exit-after") { if (!val(v)) return false; o.exit_after = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--put") {
      if (!val(v)) return false;
      const size_t eq = v.find('=');
      if (eq == std::string::npos) return false;
      o.puts.emplace_back(v.substr(0, eq), v.substr(eq + 1));
    }
    else if (a == "--idle-ms") { if (!val(v)) return false; o.idle_ms = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--pass-ms") { if (!val(v)) return false; o.pass_ms = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--no-timed-passes") o.timed_passes = false;
    else if (a == "--cursor") {
      if (!val(o.cursor) || (o.cursor != "system" && o.cursor != "message")) return false;
    }
    else if (a == "--clock-origin") { if (!val(v)) return false; o.clock_origin = std::strtoll(v.c_str(), nullptr, 0); }
    else if (a == "--edge-guard" || a == "--close-prompt" || a == "--resizable") {  // P5B
      if (!val(v) || (v != "on" && v != "off")) return false;
      (a == "--edge-guard" ? o.edge_guard : a == "--close-prompt" ? o.close_prompt : o.resizable) = v == "on";
    }
    else if (a == "--zoom-keys") {
      if (!val(o.zoom_keys) || (o.zoom_keys != "all" && o.zoom_keys != "numpad" && o.zoom_keys != "off")) return false;
    }
    else if (a == "--scale") {
      if (!val(o.scale) || (o.scale != "auto" && o.scale != "nearest" && o.scale != "smooth")) return false;
    }
    else if (a == "--keys") {
      if (!val(o.keys) || (o.keys != "system" && o.keys != "message")) return false;
    }
    else if (a == "--flag17") {
      if (!val(v)) return false;
      const size_t c = v.find(':');
      if (c == std::string::npos) return false;
      o.flag17_from = std::atoi(v.substr(0, c).c_str());
      o.flag17_to = std::atoi(v.substr(c + 1).c_str());
    }
    else if (a == "--terrain") {
      if (!val(o.terrain) || (o.terrain != "live" && o.terrain != "headless")) return false;
    }
    else if (a == "--audio-no-device") o.audio_no_device = true;  // S7.11
    else if (a == "--profile" || a == "--profile-list") {  // S7.12
      o.profile_list = a == "--profile-list";
      if (!val(o.profile) || (o.profile != "original" && o.profile != "fixed" && o.profile != "play")) return false;
    }
    else if (a == "--audio") {  // S7.10
      if (!val(o.audio) || (o.audio != "off" && o.audio != "on" && o.audio != "model" && o.audio != "capture")) return false;
    }
    else if (a == "--gl") {
      if (!val(o.gl) || (o.gl != "auto" && o.gl != "wgl" && o.gl != "host" && o.gl != "raster")) return false;
    }
    else if (a == "--terrain-va") { if (!val(v)) return false; o.terrain_va = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 0)); }
    else if (a == "--live-trace") { if (!val(o.live_trace)) return false; }
    else if (a == "--snapshots") {
      if (!val(v)) return false;
      for (size_t p = 0; p < v.size();) {
        const size_t c = v.find(',', p);
        const std::string t = v.substr(p, c == std::string::npos ? std::string::npos : c - p);
        if (!t.empty()) o.snapshot_list.push_back(std::strtoll(t.c_str(), nullptr, 0));
        if (c == std::string::npos) break;
        p = c + 1;
      }
    }
    else if (a == "--no-activate") o.activate = false;
    else if (a == "--no-window") o.window = false;
    else if (a == "--selftest") o.selftest = true;
    else if (a == "--selftest-p5b") o.selftest = o.selftest_p5b = true;  // P5B
    else if (a == "--translate") { if (!val(o.translate_in)) return false; }
    else if (a == "--out") { if (!val(o.translate_out)) return false; }
    else if (a == "--cat" || a == "--write") {
      o.op = a.substr(2);
      if (i + 2 >= argc) return false;
      o.op_args = {argv[i + 1], argv[i + 2]};
      i += 2;
    }
    else if (a == "--remove" || a == "--list") {
      o.op = a.substr(2);
      if (!val(v)) return false;
      o.op_args = {v};
    }
    else {
      std::fprintf(stderr, "unknown argument %s\n", a.c_str());
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  Options o;
  // G30: the default profile is `play`; SG_APP_PROFILE=original|fixed|play overrides it (port/test/conftest.py sets `fixed`, so the
  // app tests keep their pre-G30 behaviour); an explicit --profile wins over both
  if (const char *ep = std::getenv("SG_APP_PROFILE"))
    if (!std::strcmp(ep, "original") || !std::strcmp(ep, "fixed") || !std::strcmp(ep, "play")) o.profile = ep;
  // P5B: SG_APP_P5B=off restores S7's window and input behaviour (port/test/conftest.py sets it, so the app tests that compare
  // with nearest-pixel frames, drive an unfocused window or close it with WM_CLOSE keep their behaviour); flags still win
  if (const char *ep = std::getenv("SG_APP_P5B"))
    if (!std::strcmp(ep, "off")) {
      o.edge_guard = o.close_prompt = o.resizable = false;
      o.zoom_keys = "off";
      o.scale = "nearest";
    }
  if (!parse(argc, argv, o)) {
    std::fprintf(stderr, "usage: see the head of port/app/main.cpp\n");
    return 2;
  }
  if (o.selftest) return run_selftest(o);
  if (!o.translate_in.empty()) return run_translate(o);
  if (o.profile_list) return run_profile_list(o.profile);  // S7.12
  {
    const std::string perr = apply_profile(o.profile, o.negctl == "profile-ignored");  // S7.12: before any run
    if (!perr.empty()) {
      std::fprintf(stderr, "%s\n", perr.c_str());
      return 2;
    }
  }
  if (o.install.empty()) {
    std::fprintf(stderr, "--install <dir> is required (the directory holding the user's golf_clean.exe)\n");
    return 2;
  }
  if (o.exe.empty()) o.exe = (fs::path(o.install) / "golf_clean.exe").string();
  if (o.data.empty()) o.data = AppFiles::default_data_dir();
  if (o.corpus.empty()) o.corpus = (fs::current_path() / "port" / "corpus" / "traces").string();
  SavesDefect sd = SavesDefect::None;
  if (o.negctl == "saves-install") sd = SavesDefect::WriteToInstall;
  if (o.negctl == "saves-noreadback") sd = SavesDefect::NoReadBack;
  if (o.negctl == "saves-inside") sd = SavesDefect::AllowInsideInstall;
  AppFiles files(o.install, o.data, sd);
  if (!files.ok()) {
    std::fprintf(stderr, "saves overlay refused: %s\n", files.error().c_str());
    return 3;
  }
  for (const auto &kv : o.puts) {
    std::vector<uint8_t> b;
    if (!read_file(kv.second, b)) {
      std::fprintf(stderr, "cannot read %s\n", kv.second.c_str());
      return 2;
    }
    files.add_fixture(kv.first, std::move(b));
  }
  if (!o.op.empty()) return run_op(o, files);

  const fs::path rec = o.record_dir.empty() ? fs::path(files.data_root()) / "recordings" / stamp() : fs::path(o.record_dir);
  std::error_code ec;
  fs::create_directories(rec, ec);
  if (!o.live_trace.empty()) return run_live_trace(o, files, rec);       // S7.5: the live path over a corpus trace
  if (o.trace.empty() && o.window) return run_live_window(o, files, rec);  // S7.6: the game

  PresentDefect pd = o.negctl == "present-stale" ? PresentDefect::Stale
                     : o.negctl == "present-swap" ? PresentDefect::ByteSwap
                                                  : PresentDefect::None;
  Presenter presenter(pd);
  presenter.set_frame(std::vector<uint16_t>(static_cast<size_t>(kLogicalW) * kLogicalH, 0));  // black until a frame comes

  PresentResult pr;
  auto write_session = [&](const AppWindow *w, uint32_t events) {
    std::string s = "{\"app\":\"simgolf_port\",\"mode\":\"" + std::string(o.trace.empty() ? "shell" : "present-trace") + "\"";
    s += profile_json(o.profile);  // S7.12
    s += ",\"install\":\"" + jesc(files.install_root()) + "\",\"data\":\"" + jesc(files.data_root()) + "\"";
    s += ",\"trace\":\"" + jesc(o.trace) + "\",\"snapshot_at\":" + std::to_string(o.snapshot_at);
    s += ",\"max_records\":" + std::to_string(o.max_records) + ",\"records\":" + std::to_string(pr.run.done);
    s += ",\"code\":" + std::to_string(pr.run.code) + ",\"detail\":\"" + jesc(pr.run.detail.substr(0, 2000)) + "\"";
    s += ",\"error\":\"" + jesc(pr.error) + "\",\"data_va\":" + std::to_string(pr.data_va);
    s += ",\"frames_presented\":" + std::to_string(presenter.frames_set() > 1 ? presenter.frames_set() - 1 : 0);
    s += ",\"input_events\":" + std::to_string(events) + ",\"saves_refused\":" + std::to_string(files.refused());
    if (w) s += ",\"client\":[" + std::to_string(w->client_w()) + "," + std::to_string(w->client_h()) + "],\"dpi\":" + std::to_string(w->dpi());
    s += "}\n";
    write_file(rec / "session.json", s.data(), s.size());
  };
  auto finish_frame = [&]() {
    if (!pr.frame.empty()) {
      presenter.set_frame(pr.frame);
      write_file(rec / "back.555", pr.frame.data(), pr.frame.size() * 2);
    }
  };

  if (!o.window) {
    if (!o.trace.empty()) {
      pr = present_trace(o, files, rec);
      finish_frame();
    }
    write_session(nullptr, 0);
    if (!pr.error.empty()) std::fprintf(stderr, "%s\n", pr.error.c_str());
    std::printf("DONE records=%d code=%d\n", pr.run.done, pr.run.code);
    return pr.error.empty() ? 0 : 1;
  }

  WindowOptions wo;
  wo.dpi = o.dpi;
  wo.activate = o.activate;
  wo.input_defect = o.negctl == "input-bypass" ? InputDefect::Bypass : InputDefect::None;
  wo.resizable = o.resizable;  // P5B item 4
  presenter.set_scale(scale_mode(o.scale));
  AppWindow win(presenter, wo);
  if (!win.create()) {
    std::fprintf(stderr, "%s\n", win.error().c_str());
    return 4;
  }
  std::FILE *log = std::fopen((rec / "input.jsonl").string().c_str(), "wb");
  uint32_t events = 0;
  win.set_sink([&](const InputEvent &e) {
    ++events;
    if (log) {
      std::fprintf(log, "%s\n", event_json(e).c_str());
      std::fflush(log);
    }
  });
  HWND hwnd = static_cast<HWND>(win.hwnd());
  auto ready = [&]() {
    std::printf("READY hwnd=0x%llx client=%dx%d dpi=%u record=%s\n", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)),
                win.client_w(), win.client_h(), win.dpi(), rec.string().c_str());
    std::fflush(stdout);
  };
  std::thread worker;
  std::atomic<bool> done{false};
  if (!o.trace.empty()) {
    worker = std::thread([&]() {
      pr = present_trace(o, files, rec);
      done = true;
      PostMessageW(hwnd, WM_APP + 1, 0, 0);
    });
  }
  // the worker's result reaches the window through WM_APP + 1, handled by this loop before dispatch
  if (o.trace.empty()) ready();
  if (o.exit_after) SetTimer(hwnd, 1, o.exit_after, nullptr);
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    if (m.message == WM_APP + 1 && m.hwnd == hwnd) {
      finish_frame();
      win.invalidate();
      UpdateWindow(hwnd);
      write_session(&win, events);
      if (!pr.error.empty()) std::fprintf(stderr, "%s\n", pr.error.c_str());
      ready();
      continue;
    }
    TranslateMessage(&m);  // the jgld pumps call TranslateMessage before DispatchMessageA (WM_CHAR from real keys)
    DispatchMessageW(&m);
  }
  if (worker.joinable()) worker.join();
  if (log) std::fclose(log);
  write_session(&win, events);
  return 0;
}
