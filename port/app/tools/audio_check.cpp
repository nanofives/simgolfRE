// simgolf_audio_check (roadmap S7.10): the app's audio out (port/app/audio_out.h) driven without the window or the sim.
//
//   device  --exe <golf_clean.exe> --install <dir> --image <start.data.bin> --out <lines.txt>
//           gate (a): audio_stream.device_run's sequence through AudioOut in Model mode on the shadow thread: the image
//           prepared by deviceFixture, initSound(1, 0x8a), loadSoundTable, audio_exe.deviceStep(3, plays), `ticks` timer ticks,
//           shutdownSoundSystem; the model's lines to --out (one per line; their sha256 is golden_audio_exe.json $device);
//           [--plays n] [--ticks n] [--tick-ms m] [--control 1|2] [--stats <out.json>]
//   script  --exe --install --script <file> --mode model|capture|dsound [--wav <out.wav>] [--describe <out.txt>]
//           [--defect none|unlock|pan|volume|frequency] [--stats <out.json>] [--lines <model lines.txt>]
//           the app's start (the exe's initialised data, the holders, the CRT's arrayStaticInit_448160 request), then the
//           script, one item per line: `req <name> <arg>...` (hex or decimal), `ticks <n>` (virtual time: n timer ticks of
//           30 ms; dsound: n real periods), `sleep <ms>` (dsound); capture writes the mixed output to --wav;
//           S7.11: [--no-device 1] (the sink's DirectSoundCreate fails) [--control 3|4] [--cycles n] (start .. stop n times;
//           the stats carry the process's handle / thread counts before the first and after the last)
// Exit code 0 when every request ran (stats.failed 0), 1 otherwise, 2 on bad arguments.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../audio_out.h"
#include "portsim/execonst.h"

namespace {

std::map<std::string, std::string> parse(int argc, char **argv, int from) {
  std::map<std::string, std::string> m;
  for (int i = from; i + 1 < argc; i += 2) m[argv[i]] = argv[i + 1];
  return m;
}
bool read_all(const std::string &p, std::vector<uint8_t> &out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}
uint32_t num(const std::string &s) { return static_cast<uint32_t>(std::strtoll(s.c_str(), nullptr, 0)); }

// the exe's .data VA (the GameState's address): PE32 section ".data"
bool data_va_of(const std::vector<uint8_t> &exe, uint32_t &va) {
  if (exe.size() < 0x40) return false;
  const uint32_t pe = *reinterpret_cast<const uint32_t *>(&exe[0x3c]);
  if (pe + 0xf8 > exe.size()) return false;
  const uint16_t nsec = *reinterpret_cast<const uint16_t *>(&exe[pe + 6]);
  const uint16_t opt = *reinterpret_cast<const uint16_t *>(&exe[pe + 0x14]);
  const uint32_t base = *reinterpret_cast<const uint32_t *>(&exe[pe + 0x34]);
  for (uint32_t i = 0, s = pe + 0x18 + opt; i < nsec && s + 40 <= exe.size(); ++i, s += 40)
    if (std::string(reinterpret_cast<const char *>(&exe[s]), 5) == ".data") {
      va = base + *reinterpret_cast<const uint32_t *>(&exe[s + 12]);
      return true;
    }
  return false;
}

void write_stats(const std::string &path, const app::AudioOutStats &s, double wall_s, const app::ProcCounts &before = {},
                 const app::ProcCounts &after = {}) {
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fprintf(f,
               "{\"requests\":%u,\"replayed\":%u,\"failed\":%u,\"ticks\":%u,\"delivered\":%u,\"dropped\":%u,\"late_ticks\":%u,"
               "\"tick_ns_total\":%llu,\"tick_ns_max\":%llu,\"request_ns_total\":%llu,\"request_ns_max\":%llu,"
               "\"period_ms\":%u,\"initialised\":%s,\"wall_s\":%.3f,\"first_error\":\"%s\",",
               s.requests, s.replayed, s.failed, s.ticks, s.delivered, s.dropped, s.late_ticks,
               static_cast<unsigned long long>(s.tick_ns_total), static_cast<unsigned long long>(s.tick_ns_max),
               static_cast<unsigned long long>(s.request_ns_total), static_cast<unsigned long long>(s.request_ns_max),
               s.period_ms, s.initialised ? "true" : "false", wall_s, s.first_error.c_str());
  // S7.11: the device-less state, the heap and the process counts around start / stop
  std::fprintf(f,
               "\"init_eax\":%u,\"sound_ready\":%d,\"device_less\":%s,\"thread_ended\":%s,\"dropped_requests\":%u,"
               "\"no_device_answers\":%u,\"heap\":{\"arena_bytes\":%u,\"chunks\":%u,\"live_blocks\":%u,\"freed_blocks\":%u,"
               "\"live_bytes\":%llu},\"heap_peak_live_bytes\":%llu,\"heap_peak_arena_bytes\":%u,"
               "\"counts\":{\"handles_before\":%u,\"threads_before\":%u,\"handles_after\":%u,\"threads_after\":%u,"
               "\"threads_added\":%u},",
               s.init_eax, s.sound_ready, s.device_less ? "true" : "false", s.thread_ended ? "true" : "false",
               s.dropped_requests, s.sink.no_device_answers, s.heap.arena_bytes, s.heap.chunks, s.heap.live_blocks,
               s.heap.freed_blocks, static_cast<unsigned long long>(s.heap.live_bytes),
               static_cast<unsigned long long>(s.heap_peak.live_bytes), s.heap_peak.arena_bytes, before.handles,
               before.threads, after.handles, after.threads, app::threads_added(before, after));
  std::fprintf(f,
               "\"sink\":{\"commands\":%u,\"buffers\":%u,\"real_buffers\":%u,\"real_failures\":%u,\"unlocks\":%u,\"plays\":%u,"
               "\"stops\":%u,\"posted\":%u,\"bytes_copied\":%llu,\"frames_rendered\":%llu,\"first_failure\":\"%s\"},"
               "\"pointer_bits\":%u}\n",
               s.sink.commands, s.sink.buffers, s.sink.real_buffers, s.sink.real_failures, s.sink.unlocks, s.sink.plays,
               s.sink.stops, s.sink.posted, static_cast<unsigned long long>(s.sink.bytes_copied),
               static_cast<unsigned long long>(s.sink.frames_rendered), s.sink.first_failure.c_str(),
               static_cast<unsigned>(sizeof(void *) * 8));
  std::fclose(f);
}

int device_mode(std::map<std::string, std::string> &a, portsim::ExeConstants &ec, uint32_t data_va) {
  app::AudioOutOptions o;
  o.mode = app::SinkMode::Model;
  o.install = a["--install"];
  o.exe = &ec;
  o.data_va = data_va;
  o.realtime = false;
  o.keep_lines = true;
  o.auto_init = false;
  o.tick_clock_ms = a.count("--tick-ms") ? num(a["--tick-ms"]) : 10;  // deviceStep 4's step (a control changes it)
  o.control = a.count("--control") ? static_cast<int>(num(a["--control"])) : 0;  // AudioOutOptions::control
  if (!read_all(a["--image"], o.start_image)) return std::fprintf(stderr, "cannot read --image\n"), 2;
  const uint32_t plays = a.count("--plays") ? num(a["--plays"]) : 300, ticks = a.count("--ticks") ? num(a["--ticks"]) : 40;
  app::AudioOut out;
  std::string err;
  if (!out.start(o, &err)) return std::fprintf(stderr, "start: %s\n", err.c_str()), 1;
  const uint32_t init[2] = {1, 0x8a}, step3[2] = {3, plays};
  out.request("initSound", init, 2);
  out.request("loadSoundTable", nullptr, 0);
  out.request("audio_exe.deviceStep", step3, 2);
  out.tick(ticks);
  out.request("shutdownSoundSystem", nullptr, 0);
  out.sync();
  const std::vector<std::string> lines = out.take_lines();
  const app::AudioOutStats s = out.stats();
  if (a.count("--stats")) write_stats(a["--stats"], s, 0.0);
  out.stop();
  std::FILE *f = std::fopen(a["--out"].c_str(), "wb");
  if (!f) return 2;
  for (size_t i = 0; i < lines.size(); ++i) std::fprintf(f, "%s%s", i ? "\n" : "", lines[i].c_str());
  std::fclose(f);
  std::printf("lines %zu failed %u first_error %s\n", lines.size(), s.failed, s.first_error.c_str());
  return s.failed ? 1 : 0;
}

int script_cycle(std::map<std::string, std::string> &a, const app::AudioOutOptions &o,
                 const std::vector<std::string> &script, const app::ProcCounts &first, bool last, uint32_t cyc);

int script_mode(std::map<std::string, std::string> &a, portsim::ExeConstants &ec, uint32_t data_va) {
  app::AudioOutOptions o;
  const std::string mode = a.count("--mode") ? a["--mode"] : "capture";
  o.mode = mode == "model" ? app::SinkMode::Model : mode == "dsound" ? app::SinkMode::DSound : app::SinkMode::Capture;
  o.install = a["--install"];
  o.exe = &ec;
  o.data_va = data_va;
  o.realtime = o.mode == app::SinkMode::DSound;
  o.tick_clock_ms = 30;  // capture / model: the device plays the timer's 30 ms between ticks
  o.keep_lines = a.count("--lines") != 0;
  const std::string d = a.count("--defect") ? a["--defect"] : "none";
  o.defect = d == "unlock" ? app::SinkDefect::SkipUnlockCopy : d == "pan" ? app::SinkDefect::IgnorePan
             : d == "volume" ? app::SinkDefect::IgnoreVolume : d == "frequency" ? app::SinkDefect::IgnoreFrequency
                             : app::SinkDefect::None;
  o.force_no_device = a.count("--no-device") && num(a["--no-device"]) != 0;  // S7.11 test switch
  o.control = a.count("--control") ? static_cast<int>(num(a["--control"])) : 0;
  std::ifstream sf(a["--script"]);
  if (!sf) return std::fprintf(stderr, "cannot read --script\n"), 2;
  std::vector<std::string> script;
  for (std::string l; std::getline(sf, l);) script.push_back(l);
  // S7.11 --cycles n: the whole start .. stop n times in one process (a per-session leak grows the counts each cycle; the
  // outputs are the last cycle's)
  const uint32_t cycles = a.count("--cycles") ? std::max<uint32_t>(1, num(a["--cycles"])) : 1;
  const app::ProcCounts first = app::proc_counts();  // S7.11: before any audio thread exists
  int rc = 0;
  for (uint32_t cyc = 0; cyc < cycles; ++cyc) rc = script_cycle(a, o, script, first, cyc + 1 == cycles, cyc);
  return rc;
}

int script_cycle(std::map<std::string, std::string> &a, const app::AudioOutOptions &o,
                 const std::vector<std::string> &script, const app::ProcCounts &first, bool last, uint32_t cyc) {
  const app::ProcCounts before = app::proc_counts();
  // control 4 leaves the thread running at stop(): the object is leaked so the thread's state stays valid
  app::AudioOut &out = *new app::AudioOut;
  std::string err;
  const auto t0 = std::chrono::steady_clock::now();
  if (!out.start(o, &err)) return std::fprintf(stderr, "start: %s\n", err.c_str()), 1;
  out.request("arrayStaticInit_448160", nullptr, 0);  // the boot's CRT request (BootCrt)
  for (const std::string &line : script) {
    std::istringstream is(line);
    std::string op;
    if (!(is >> op) || op[0] == '#') continue;
    if (op == "req") {
      std::string name, v;
      is >> name;
      std::vector<uint32_t> args;
      while (is >> v) args.push_back(num(v));
      out.request(name.c_str(), args.data(), static_cast<int>(args.size()));
    } else if (op == "ticks") {
      std::string v;
      is >> v;
      if (o.realtime) std::this_thread::sleep_for(std::chrono::milliseconds(30ull * num(v)));
      else out.tick(num(v));
    } else if (op == "sleep") {
      std::string v;
      is >> v;
      out.sync();
      std::this_thread::sleep_for(std::chrono::milliseconds(num(v)));
    }
  }
  out.sync();
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  if (last && a.count("--wav")) out.write_capture(a["--wav"]);
  if (last && a.count("--describe")) {
    std::ofstream df(a["--describe"], std::ios::binary);
    df << out.describe();
  }
  if (last && o.keep_lines) {
    std::ofstream lf(a["--lines"], std::ios::binary);
    for (const std::string &l : out.take_lines()) lf << l << '\n';
  }
  out.stop();
  const app::AudioOutStats s = out.stats();
  if (o.control != 4) delete &out;
  const app::ProcCounts after = app::proc_counts();  // S7.11: after stop() joined the thread and released the device
  if (last && a.count("--stats")) write_stats(a["--stats"], s, wall, first, after);
  std::printf("cycle %u requests %u replayed %u failed %u ticks %u device_less %d dropped %u handles %u->%u threads %u->%u new %u "
              "first_error %s\n", cyc, s.requests, s.replayed, s.failed, s.ticks, s.device_less ? 1 : 0,
              s.dropped_requests, before.handles, after.handles, before.threads, after.threads, app::threads_added(before, after),
              s.first_error.c_str());
  return s.failed ? 1 : 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) return std::fprintf(stderr, "usage: simgolf_audio_check device|script --exe ... (see the source)\n"), 2;
  auto a = parse(argc, argv, 2);
  std::vector<uint8_t> exe;
  uint32_t data_va = 0;
  if (!read_all(a["--exe"], exe) || !data_va_of(exe, data_va)) return std::fprintf(stderr, "bad --exe\n"), 2;
  portsim::ExeConstants &ec = portsim::default_exe_constants();
  std::string why;
  if (!ec.loaded() && !ec.load_file(a["--exe"].c_str(), &why)) return std::fprintf(stderr, "exe: %s\n", why.c_str()), 2;
  const std::string cmd = argv[1];
  if (cmd == "device") return device_mode(a, ec, data_va);
  if (cmd == "script") return script_mode(a, ec, data_va);
  return std::fprintf(stderr, "unknown command %s\n", cmd.c_str()), 2;
}
