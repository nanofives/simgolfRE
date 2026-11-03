// Module `live` (roadmap S7.1, 2026-10-24): the live run core, portsim/sim/live.h. Notes: log/roadmap/S7/1_notes.md.
//
// run_live drives the same boot and frame steps as tick_run_frames (tick.cpp, through tk::tick_frame_io) but answers the
// frame's outside reads on demand from a LiveHost, and writes every answer into a schema v1 trace (re/model/trace_schema.md)
// whose replay by sg_run_boot reproduces the live run's frame records. The replay rules the recording is shaped for are
// tick.cpp's parse_trace (dispatch batching and class) and TraceInput::batch (which pump call takes which dispatch); the
// comments at each step cite the rule they serve. No original address here: clock sites are offsets from mainLoop's
// frame-head return address (the trace's meta frame_ra is a run option), module-relative call sites use the trace's DLL id
// encoding (trace_schema.md section 1).
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/live.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/tick.h"
#include "portsim/state_hash.h"
#include "tick_hook.h"

namespace portsim {

namespace {

// ------------------------------------------------------------------------------------------------ negative controls
// live.recorder (log/roadmap/S7/1_notes.md "Controls"): mode = what + 16 * n.
//   what 1: the recording leaves out the n-th clock answer given through InputSource::clock (n = 0: seedFromTime's)
//   what 2: the recording swaps the commands of the n-th and (n+1)-th dispatches it writes
//   what 3: the n-th cursor read the port makes is answered with the host's NEXT answer (one read skipped: the host
//           answers the cursor read one read late)
//   what 4: the n-th cursor read the port makes is answered with x + 1 (one read off by one pixel)
//   what 5 (roadmap S7.6): the cursor read after the n-th idle pass is left out of the recording (an idle pass left out)
//   what 6 (roadmap S7.6): the n-th dispatch the host gives is applied but left out of the recording (a dropped batch)
//   what 7 (U-1604): no Palette::update QPC record after a load dispatch (the replay does not see it as a load batch)
//   what 8 (U-1604): the focus keys (RootHandler 12 / 13) left out of the recording
//   what 9 (roadmap S7.8): a spilling recorder drops the last event of every spill (LiveOptions::trace_file only)
int g_negctl_recorder = 0;

constexpr int32_t kSiteFrameHead = 0;         // the frame head itself (the trace's frame_ra)
// waitTicks' first timeGetTime return as an offset from the frame-head return address (tick.cpp kSiteWaitTicks): the modal
// loops of the original call waitTicks(1, 0) every pass; a recorded read there separates a modal dispatch (parse_trace's
// batch class) and is read by no port code
constexpr int32_t kSiteWaitTicks = 0x4c384;
// a GetAsyncKeyState return of mainLoop's H1 (outside the root handlers): the site the recording gives the frame body's reads
// (TkBase::key_async does not carry its call site; parse_trace keys body reads by vk and order only)
constexpr int32_t kSiteAsyncBody = 0xf10c;
// the press handlers' GetAsyncKeyState(VK_CONTROL) / (VK_SHIFT) returns in rootWin_onPress1 / rootWin_onPress2 (inside the
// root-handler range parse_trace gives a command's own reads to)
constexpr int32_t kSiteCtrlPress1 = 0x5599c, kSiteShiftPress1 = 0x559b0, kSiteCtrlPress2 = 0x55aa6, kSiteShiftPress2 = 0x55aba;
// a clock site inside the root-handler range for a command's deadline read (Command::t: parse_trace takes the first
// handler-range read after the HANDLER record); the original's own site was not observed in the corpus (1_notes.md)
constexpr int32_t kSiteHandlerClock = 0x55951 + 0x40;
// jgld.dll's Palette::update QPC return (module id 1, trace_schema.md section 1; tick.cpp kPaletteQpcRa): a body dispatch's
// separator (parse_trace: a dispatch after it is the frame body's)
constexpr uint32_t kPaletteQpcRa = 0x80000000u | (1u << 24) | 0x6a414u;

struct LiveStop {};

// ------------------------------------------------------------------------------------------------ the recording
class Recorder {
 public:
  struct Ev {
    uint8_t type = 0, sub = 0;  // trace record type / sub
    uint32_t a = 0, b = 0, c = 0;
    uint32_t d = 0;             // DELIVER call (roadmap S3.30): argc (a = addr, b / c = the arguments)
    uint64_t v = 0;             // CLOCKRUN value
    uint32_t snap = 0;          // FRAME: index into snaps_
    uint32_t grp = 0;           // the dispatch the record belongs to (k + 1), 0 = none
  };
  static constexpr uint8_t kMark = 0xf0;  // a LiveMarker::mark (a = its index): not written, bytes() reports its position
  static constexpr uint8_t kGone = 0xf1;  // a record a negative control removed; not written
  bool on = true;
  uint32_t frame_ra = 0;
  std::string scenario;
  std::vector<std::vector<uint8_t>> snaps;
  size_t snaps_base = 0;  // roadmap S7.8: FRAME snapshots spilled before snaps[0]
  std::vector<Ev> ev;
  uint32_t frames = 0;

  // a timeGetTime (sub 1) or QPC (sub 2) answer at a site; returns its event index
  size_t clock(uint8_t sub, uint32_t ra, uint64_t v) {
    Ev e;
    e.type = 0x01;
    e.sub = sub;
    e.a = ra;
    e.b = sub == 1 ? 0u : 1u;  // the slot: meta.slots[0] tgt, [1] qpc
    e.v = v;
    ev.push_back(e);
    return ev.size() - 1;
  }
  void insert_clock_before(size_t at, uint8_t sub, uint32_t ra, uint64_t v) {
    Ev e;
    e.type = 0x01;
    e.sub = sub;
    e.a = ra;
    e.b = sub == 1 ? 0u : 1u;
    e.v = v;
    ev.insert(ev.begin() + static_cast<std::ptrdiff_t>(at), e);
  }
  void frame(uint32_t t, std::vector<uint8_t> snap) {
    Ev e;
    e.type = 0x02;
    e.a = frames++;
    e.b = t;
    e.snap = static_cast<uint32_t>(snaps_base + snaps.size());
    snaps.push_back(std::move(snap));
    ev.push_back(e);
  }
  void rec(uint8_t type, uint8_t sub, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0, uint32_t grp = 0) {
    Ev e;
    e.type = type;
    e.sub = sub;
    e.a = a;
    e.b = b;
    e.c = c;
    e.grp = grp;
    ev.push_back(e);
  }
  // negative control 2: the records of dispatches g and g + 1 trade places (each group is contiguous)
  void swap_groups(uint32_t g) {
    auto range = [&](uint32_t k, size_t &lo, size_t &hi) {
      lo = hi = SIZE_MAX;
      for (size_t i = 0; i < ev.size(); ++i)
        if (ev[i].grp == k) {
          if (lo == SIZE_MAX) lo = i;
          hi = i + 1;
        }
      return lo != SIZE_MAX;
    };
    size_t a0, a1, b0, b1;
    if (!range(g, a0, a1) || !range(g + 1, b0, b1) || a1 > b0) return;
    std::vector<Ev> out(ev.begin(), ev.begin() + static_cast<std::ptrdiff_t>(a0));
    out.insert(out.end(), ev.begin() + static_cast<std::ptrdiff_t>(b0), ev.begin() + static_cast<std::ptrdiff_t>(b1));
    out.insert(out.end(), ev.begin() + static_cast<std::ptrdiff_t>(a1), ev.begin() + static_cast<std::ptrdiff_t>(b0));
    out.insert(out.end(), ev.begin() + static_cast<std::ptrdiff_t>(a0), ev.begin() + static_cast<std::ptrdiff_t>(a1));
    out.insert(out.end(), ev.begin() + static_cast<std::ptrdiff_t>(b1), ev.end());
    ev.swap(out);
  }

  // The body encoder (roadmap S7.8, U-1643): the schema v1 records of the events fed to it, in order. CLOCKRUN records join
  // consecutive answers of one site and slot that step by the header's step. bytes() feeds every event at once; a spilling
  // recorder (spill_open / spill / spill_finish) feeds the events of finished frames as the session goes and writes their
  // bytes to a file, so a long session keeps only the latest events in memory. The bytes are the same either way.
  struct Encoder {
    static constexpr uint32_t step = 1, qstep = 10000;
    std::vector<uint8_t> body;
    uint64_t t0 = 0x00100000u, q0 = 0x10000000u;
    bool have_t = false, have_q = false;
    uint32_t n = 0;  // clock calls written
    uint32_t polls = 0;
    struct Run {
      bool open = false;
      uint8_t sub = 0;
      uint32_t first = 0, count = 0, ra = 0, slot = 0;
      uint64_t v0 = 0;
    } run;
    void u8(uint8_t x) { body.push_back(x); }
    void u16(uint16_t x) {
      u8(static_cast<uint8_t>(x));
      u8(static_cast<uint8_t>(x >> 8));
    }
    void u32(uint32_t x) {
      for (int i = 0; i < 4; ++i) u8(static_cast<uint8_t>(x >> (8 * i)));
    }
    void u64(uint64_t x) {
      u32(static_cast<uint32_t>(x));
      u32(static_cast<uint32_t>(x >> 32));
    }
    void head(uint8_t type, uint8_t sub, uint16_t len) {
      u8(type);
      u8(sub);
      u16(len);
    }
    void flush() {
      if (!run.open) return;
      head(0x01, run.sub, 24);
      u32(run.first);
      u32(run.count);
      u32(run.ra);
      u32(run.slot);
      u64(run.v0);
      run.open = false;
    }
    void feed(const Ev &e, const std::vector<uint8_t> *snap, std::vector<uint32_t> *mark_pos) {
      if (e.type == 0x01) {
        if (e.sub == 1 && !have_t) {
          t0 = e.v;
          have_t = true;
        }
        if (e.sub == 2 && !have_q) {
          q0 = e.v;
          have_q = true;
        }
        const uint64_t st = e.sub == 1 ? step : qstep;
        if (run.open && run.sub == e.sub && run.ra == e.a && run.slot == e.b && e.v == run.v0 + run.count * st) {
          ++run.count;
        } else {
          flush();
          run = {true, e.sub, n, 1, e.a, e.b, e.v};
        }
        ++n;
        return;
      }
      if (e.type == kMark) {  // the clock calls written before it: a gfx.snapshotAt position (live.h LiveMarker)
        if (mark_pos) {
          if (mark_pos->size() <= e.a) mark_pos->resize(e.a + 1, 0);
          (*mark_pos)[e.a] = n;
        }
        return;
      }
      if (e.type == kGone) return;
      flush();
      switch (e.type) {
        case 0x02:  // FRAME: frame, pos, t, snapshot
          head(0x02, 0, static_cast<uint16_t>(12 + snap->size()));
          u32(e.a);
          u32(n);
          u32(e.b);
          body.insert(body.end(), snap->begin(), snap->end());
          break;
        case 0x03:  // DELIVER w32: pos, addr, value; post (sub 3, roadmap S7.6): pos, cls 1 (the game window), hwnd 0, msg, wp, lp
          if (e.sub == 3) {
            head(0x03, 3, 24);
            u32(n);
            u32(1);
            u32(0);
            u32(e.a);
            u32(e.b);
            u32(e.c);
          } else if (e.sub == 7) {  // call (roadmap S3.30): pos, addr, argc, a0, a1
            head(0x03, 7, 20);
            u32(n);
            u32(e.a);
            u32(e.d);
            u32(e.b);
            u32(e.c);
          } else {
            head(0x03, 4, 12);
            u32(n);
            u32(e.a);
            u32(e.b);
          }
          break;
        case 0x05:  // MSG: pos, msg, wp, lp
          head(0x05, e.sub, 16);
          u32(n);
          u32(e.a);
          u32(e.b);
          u32(e.c);
          break;
        case 0x06:  // KEYSTATE: pos, ra, vk, result
          head(0x06, e.sub, 16);
          u32(n);
          u32(e.a);
          u32(e.b);
          u32(e.c);
          break;
        case 0x08:  // CURSOR: pos, x, y
          head(0x08, 1, 12);
          u32(n);
          u32(e.a);
          u32(e.b);
          break;
        case 0x09:  // HANDLER: pos, a0, a1
          head(0x09, e.sub, 12);
          u32(n);
          u32(e.a);
          u32(e.b);
          break;
        default:
          break;
      }
    }
    void end(bool quit, uint32_t frames) {
      flush();
      if (!quit) {  // the final poll (no checkpoint)
        head(0x04, 2, 9);
        u32(n);
        u32(polls);
        u8(0);
        ++polls;
      }
      head(0x7f, quit ? 1 : 0, 16);
      u32(n);
      u32(frames);
      u32(polls);
      u32(0);
    }
  };

  // the file header + metadata (keys sorted, no spaces, as trace_format.header_bytes writes them)
  std::vector<uint8_t> header(const Encoder &enc, const uint32_t data_snapshot_va[9]) const {
    std::ostringstream m;
    m << "{\"frame_ra\":" << frame_ra << ",\"live\":1,\"scenario\":\"";
    for (char ch : scenario)
      if (ch != '"' && ch != '\\' && static_cast<unsigned char>(ch) >= 0x20) m << ch;
    m << "\",\"schema\":1,\"slots\":[[\"golf_clean.exe\",\"tgt\",0],[\"jgld.dll\",\"qpc\",0]],\"snapshot\":[";
    static const uint32_t lens[9] = {0x40, 4, 4, 4, 4, 4, 4, 4, 4};
    for (int i = 0; i < 9; ++i) m << (i ? "," : "") << "[" << data_snapshot_va[i] << "," << lens[i] << "]";
    m << "],\"spec\":\"re/model/trace_schema.md\"}";
    const std::string meta = m.str();
    std::vector<uint8_t> out;
    const char magic[8] = {'S', 'G', 'T', 'R', 'A', 'C', 'E', '\0'};
    out.insert(out.end(), magic, magic + 8);
    auto o16 = [&](uint16_t x) {
      out.push_back(static_cast<uint8_t>(x));
      out.push_back(static_cast<uint8_t>(x >> 8));
    };
    auto o32 = [&](uint32_t x) {
      for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(x >> (8 * i)));
    };
    o16(1);
    o16(32);
    o32(static_cast<uint32_t>(meta.size()));
    o32(Encoder::step);
    o32(static_cast<uint32_t>(enc.t0));
    o32(Encoder::qstep);
    o32(static_cast<uint32_t>(enc.q0));
    out.insert(out.end(), meta.begin(), meta.end());
    return out;
  }
  const std::vector<uint8_t> *snap_of(const Ev &e) const {
    return e.type == 0x02 ? &snaps[e.snap - snaps_base] : nullptr;
  }

  // The schema v1 bytes of every event held (a recorder that never spilled: the whole session).
  std::vector<uint8_t> bytes(bool quit, uint32_t data_snapshot_va[9], std::vector<uint32_t> *mark_pos = nullptr) const {
    Encoder enc;
    for (const Ev &e : ev) enc.feed(e, snap_of(e), mark_pos);
    enc.end(quit, frames);
    std::vector<uint8_t> out = header(enc, data_snapshot_va);
    out.insert(out.end(), enc.body.begin(), enc.body.end());
    return out;
  }

  // ---- spilling (roadmap S7.8, U-1643)
  Encoder sp;                       // the encoder of the events spilled so far (its body is written out at every spill)
  std::FILE *sp_fh = nullptr;       // `<path>.body`: the body bytes of the spilled events
  std::string sp_path;
  std::vector<uint32_t> sp_marks;   // mark positions of the spilled events
  bool drop_on_spill = false;       // negative control 9
  uint64_t sp_events = 0, sp_body_bytes = 0;
  bool spill_open(const std::string &path) {
    sp_path = path;
    sp_fh = std::fopen((path + ".body").c_str(), "wb");
    return sp_fh != nullptr;
  }
  // feeds events [0, upto) to the spill encoder, writes their bytes, and drops them (and their FRAME snapshots)
  void spill(size_t upto) {
    if (!sp_fh || upto == 0) return;
    size_t frames_fed = 0;
    for (size_t i = 0; i < upto; ++i) {
      if (drop_on_spill && i + 1 == upto && ev[i].type != 0x02) continue;  // negative control 9
      sp.feed(ev[i], snap_of(ev[i]), &sp_marks);
      if (ev[i].type == 0x02) ++frames_fed;
    }
    if (!sp.body.empty()) std::fwrite(sp.body.data(), 1, sp.body.size(), sp_fh);
    sp_body_bytes += sp.body.size();
    std::vector<uint8_t>().swap(sp.body);
    ev.erase(ev.begin(), ev.begin() + static_cast<std::ptrdiff_t>(upto));
    snaps.erase(snaps.begin(), snaps.begin() + static_cast<std::ptrdiff_t>(frames_fed));
    snaps_base += frames_fed;
    sp_events += upto;
  }
  // the rest of the events, then the trace file `sp_path` (header + body); its size, or 0 on a write error
  uint64_t spill_finish(bool quit, uint32_t data_snapshot_va[9], std::vector<uint32_t> *mark_pos) {
    if (!sp_fh) return 0;
    for (const Ev &e : ev) sp.feed(e, snap_of(e), &sp_marks);
    sp.end(quit, frames);
    bool ok = sp.body.empty() || std::fwrite(sp.body.data(), 1, sp.body.size(), sp_fh) == sp.body.size();
    sp_body_bytes += sp.body.size();
    std::vector<uint8_t>().swap(sp.body);
    ok = std::fclose(sp_fh) == 0 && ok;
    sp_fh = nullptr;
    if (mark_pos) *mark_pos = sp_marks;
    const std::vector<uint8_t> hdr = header(sp, data_snapshot_va);
    std::FILE *out = std::fopen(sp_path.c_str(), "wb");
    std::FILE *in = std::fopen((sp_path + ".body").c_str(), "rb");
    uint64_t total = 0;
    if (out && in && ok) {
      ok = std::fwrite(hdr.data(), 1, hdr.size(), out) == hdr.size();
      total = hdr.size();
      std::vector<uint8_t> buf(1 << 20);
      for (size_t r; ok && (r = std::fread(buf.data(), 1, buf.size(), in)) > 0; total += r) ok = std::fwrite(buf.data(), 1, r, out) == r;
    } else {
      ok = false;
    }
    if (in) std::fclose(in);
    if (out) ok = std::fclose(out) == 0 && ok;
    std::remove((sp_path + ".body").c_str());
    return ok ? total : 0;
  }
};

// ------------------------------------------------------------------------------------------------ the core
class Core : public tk::LiveIo, public InputSource, public LiveWriteLog, public LiveMarker {
 public:
  Core(Ctx &c, LiveHost &h, const LiveOptions &o, LiveResult &r) : ctx_(c), host_(h), opt_(o), res_(r) {
    rec_.on = o.record;
    rec_.frame_ra = o.frame_ra;
    rec_.scenario = o.scenario;
    mode_ = SG_NEGCTL(g_negctl_recorder);
  }

  // ---- tk::LiveIo: the frame body's reads
  uint32_t clock_site(int32_t site) override {
    uint32_t t = 0;
    host_.clock(ctx_, site, true, t);
    ++res_.clocks;
    answer_tgt(site, t, false);
    return t;
  }
  void body_cursor(int32_t &x, int32_t &y) override { cursor_answer(false, x, y); }
  // roadmap S7.8 (U-1642): Window::refreshFull ran, where the original's back buffer reaches the window. Roadmap S7.9 (U-1670):
  // the gfx backend calls it from its refreshFull body (gfx.h gfx_set_refresh_hook), so the refreshes the modules issue through
  // their own drawing calls (the boot's loading screens, the modal screens) present too, not only the frame code's drop
  static void on_refresh(void *self) { static_cast<Core *>(self)->host_.present_point(static_cast<Core *>(self)->ctx_); }
  void title_menu_pass() override { host_.title_menu(ctx_); }  // P6B
  uint16_t async_key(int vk) override {
    const uint16_t r = host_.async_key(ctx_, vk & 0xff);
    ++res_.asyncs;
    if (rec_.on) rec_.rec(0x06, 2, ra(kSiteAsyncBody), static_cast<uint32_t>(vk & 0xff), r);
    return r;
  }

  // ---- InputSource: the pumps and the modal loops' reads
  bool exhausted() const override { return host_.exhausted(); }
  bool cursor(int32_t &x, int32_t &y) override {
    tk::FrameIo &io = tk::cur_io();
    cursor_answer(true, io.cursor_x, io.cursor_y);
    ++io.cursor_used;
    x = io.cursor_x;
    y = io.cursor_y;
    return true;
  }
  bool clock(int32_t site, uint32_t &t) override {
    if (!host_.clock(ctx_, site, false, t)) return false;
    ++res_.clocks;
    const bool drop = (mode_ & 15) == 1 && clock_answers_ == static_cast<uint64_t>(mode_ >> 4);  // control: left out
    ++clock_answers_;
    if (!drop) answer_tgt(site, t, true);
    ++consumed_[site];
    return true;
  }
  void pace(int32_t site) override {
    if (site == 0 && placeholder_ != SIZE_MAX) fill_placeholder(last_t_);  // the loop left before its pass read
    pace_ = site;
    host_.pace(ctx_, site);
  }
  void timed_pass(uint32_t ms) override { timed_ms_ = ms; }
  bool batch(PumpKind kind, std::vector<Command> &out) override {
    out.clear();
    // P5A: the hint holds for this call only
    const uint32_t timed_hint = timed_ms_;
    timed_ms_ = 0;
    tk::FrameIo &io = tk::cur_io();
    // U-1604: loadFlic's pump (PumpKind::Load, S1) takes the dispatch a recording puts before a world load's first palette
    // QPC; TraceInput::batch tests it apart from the frame-body / modal classes, so it is neither
    const bool load = kind == PumpKind::Load;
    const bool body = !load && io.pump_site != tk::kPumpSiteModal;
    // TraceInput::batch: a modal loop's Drain pump never takes a dispatch (pumpLight, doAllTasks in a loop): not asked
    if (!body && kind == PumpKind::Drain) return false;
    const bool paced = kind == PumpKind::Blocking && pace_ != 0;
    const bool modal_blocking = !body && kind == PumpKind::Blocking;
    // this call as the replay sees it
    Call self;
    self.body = body;
    self.kind = kind;
    self.count = io.cursor_used;
    self.pace = paced ? pace_ : 0;
    self.consumed = paced ? consumed_[pace_] : 0;
    // the cursor count a dispatch given here is recorded with: the port's reads, plus a CURSOR record still standing in front
    // of an earlier dispatch for a read the port has not made yet
    const size_t base = io.cursor_used + (cursor_ph_ != SIZE_MAX ? 1u : 0u);
    std::vector<Call> calls = same_class(body, load);
    calls.push_back(self);
    const int me = static_cast<int>(calls.size()) - 1;
    const bool plain = taker(calls, base, paced) == me;
    // roadmap S7.6: a loop that pumped twice at one cursor count takes the dispatch through TraceInput's `stuck` rule when it
    // is recorded one cursor read later (a CURSOR record in front of it, filled by the port's next read)
    const bool inflate = !plain && modal_blocking && !paced && cursor_ph_ == SIZE_MAX && taker(calls, base + 1, false) == me;
    LivePump p;
    p.kind = kind;
    p.body = body;
    p.frame = frame_;
    p.cursor_reads = static_cast<uint32_t>(io.cursor_used);
    p.pace_site = pace_;
    p.calls = static_cast<uint32_t>(burst_.size()) + 1;
    ++(body ? res_.body_calls : paced ? res_.paced_calls : res_.modal_calls);
    p.may_deliver = plain || inflate;
    // a decline here leaves a replayable delivery when a later call at the same cursor count can still take one (idle
    // passes, live.h): always for a paced loop (PACE) and a Drain pump, never for a Blocking pump of the frame body
    bool later = true;
    if (kind == PumpKind::Blocking && !paced) {
      if (body) {
        later = false;
      } else {
        std::vector<Call> hyp = calls;
        hyp.push_back(self);
        const int fut = static_cast<int>(hyp.size()) - 1;
        later = taker(hyp, base, false) == fut || (cursor_ph_ == SIZE_MAX && taker(hyp, base + 1, false) == fut);
        // P5A (InputSource::timed_pass): the loop reads the cursor before its next pump call, so a dispatch recorded there has
        // a higher cursor count than this call and TraceInput passes this call for it (j.count < count, not stuck)
        if (timed_hint != 0) later = true;
      }
    }
    p.must_deliver = kind == PumpKind::Blocking && !paced && !later;
    p.idle_ok = modal_blocking && !paced && !p.must_deliver;
    p.timed_pass_ms = p.idle_ok ? timed_hint : 0;
    std::vector<Command> cmds;
    for (uint64_t tries = 0;; ++tries) {
      ++res_.asks;
      cmds.clear();
      const LiveReply r = host_.pump(ctx_, p, cmds);
      if (r == LiveReply::Stop) {
        stop_ = "host";
        throw LiveStop{};
      }
      if (r == LiveReply::Deliver) break;
      if (p.must_deliver && host_.blocking()) {
        ++res_.reasks;
        if (tries > 100000000ull) throw NoReturn{"live: the host never delivered at a must_deliver pump"};
        continue;
      }
      if (modal_blocking) separator(false);  // the clock record after the call (LiveMarker positions)
      if (p.idle_ok) {
        ++res_.idle_declines;
        if ((mode_ & 15) == 5 && idle_count_ == static_cast<uint64_t>(mode_ >> 4)) drop_cursor_ = true;  // control 5
        ++idle_count_;
      }
      burst_.push_back(self);
      return false;
    }
    if (modal_blocking) separator(false);  // the clock record after the call (LiveMarker positions)
    if (!p.may_deliver) {
      violation("a dispatch given at a pump call the replay passes (it replays at an earlier call)");
      // development: SG_LIVE_LOG=<file> appends the pump calls of the frame that the replay would deliver at instead
      static const char *log = std::getenv("SG_LIVE_LOG");
      if (log)
        if (FILE *fh = std::fopen(log, "a")) {
          std::fprintf(fh, "record %d frame %u: delivery body %d kind %u count %zu pace %d; earlier calls:", res_.records,
                       frame_, body ? 1 : 0, static_cast<unsigned>(kind), io.cursor_used, pace_);
          for (const Call &j : burst_)
            std::fprintf(fh, " [body %d kind %u count %zu pace %d consumed %u]", j.body ? 1 : 0,
                         static_cast<unsigned>(j.kind), j.count, j.pace, j.consumed);
          std::fprintf(fh, "; handlers");
          for (const Command &c : cmds) std::fprintf(fh, " %u", c.handler);
          std::fprintf(fh, "\n");
          std::fclose(fh);
        }
    }
    // U-1604: a focus key (RootHandler 12 / 13) is written as its MSG record with no HANDLER after it; parse_trace gives such
    // keys to the batch a HANDLER record opens, so a dispatch of focus keys alone cannot be recorded
    bool root = false;
    for (const Command &c : cmds) {
      const bool focus = c.kind == CmdKind::RootHandler && (c.handler == 12 || c.handler == 13);
      if (c.kind != CmdKind::RootHandler || ((c.handler < 1 || c.handler > 11) && !focus))
        violation("a command no HANDLER record carries (not a root-window handler call)");
      if (!focus) root = true;
    }
    if (!root && !cmds.empty()) violation("a dispatch of focus keys only (no HANDLER record opens its batch)");
    const bool drop = (mode_ & 15) == 6 && host_dispatches_ == static_cast<uint64_t>(mode_ >> 4);  // control 6
    ++host_dispatches_;
    if (!drop) record_batch(body, paced, cmds, !plain && inflate, load);
    if (!plain && inflate) ++res_.inflated;
    burst_.clear();
    ++res_.dispatches;
    gfx_progress_fast(ctx_, ordinal_);
    out = cmds;
    return true;
  }

  // ---- LiveWriteLog
  void wrote(uint32_t va, uint32_t value) override {
    ++res_.writes;
    if (!rec_.on) return;
    if (in_head_) {
      head_writes_.push_back({4, va, value, 0, 0});
      return;
    }
    rec_.rec(0x03, 4, va, value);
    writes_pending_ = true;
  }
  // roadmap S3.30: an injected call is recorded where a write would be (TraceInput applies both from one list)
  void called(uint32_t addr, uint32_t argc, uint32_t a0, uint32_t a1) override {
    ++res_.writes;
    if (!rec_.on) return;
    if (in_head_) {
      head_writes_.push_back({7, addr, a0, a1, argc});
      return;
    }
    rec_call(addr, argc, a0, a1);
    writes_pending_ = true;
  }
  void rec_call(uint32_t addr, uint32_t argc, uint32_t a0, uint32_t a1) {
    rec_.rec(0x03, 7, addr, a0, a1);
    rec_.ev.back().d = argc;
  }

  // ---- LiveMarker (roadmap S7.6)
  uint32_t mark() override {
    if (rec_.on) rec_.rec(Recorder::kMark, 0, marks_);
    return marks_++;
  }

  // ---- the session
  void run() {
    host_.writes = this;
    host_.marker = this;
    spill_on_ = rec_.on && !opt_.trace_file.empty() && rec_.spill_open(opt_.trace_file);
    rec_.drop_on_spill = (mode_ & 15) == 9;
    const bool own_terrain = !has_terrain_api(ctx_);
    std::unique_ptr<TerrainApi> headless;
    if (own_terrain) {
      headless = make_headless_terrain(ctx_);
      set_terrain_api(ctx_, headless.get());
    }
    InputSource *prev = input_source(ctx_);
    set_input_source(ctx_, this);
    gfx_set_refresh_hook(ctx_, &Core::on_refresh, this);
    struct Restore {
      Ctx &c;
      bool t;
      InputSource *s;
      LiveHost &h;
      ~Restore() {
        gfx_set_refresh_hook(c, nullptr, nullptr);
        if (t) set_terrain_api(c, nullptr);
        set_input_source(c, s);
        h.writes = nullptr;
        h.marker = nullptr;
      }
    } restore{ctx_, own_terrain, prev, host_};
    const auto &sim = objects_of("sim");
    bool quit = false;
    try {
      // the boot (live frame 0: process start .. the first frame head)
      frame_ = 0;
      begin_frame();
      host_.boot(ctx_);
      FrameInput in;
      in.boot = true;
      in.live = this;
      in.on_wait = &Core::on_wait;
      in.on_wait_arg = this;
      tk::FrameIoStats st;
      FrameExit ex = tk::tick_frame_io(ctx_, in, st);
      bool retile = false;
      for (;;) {
        end_frame();
        if (ex == FrameExit::Quit) {
          quit = true;
          stop_ = "quit";
          break;
        }
        retile = ex == FrameExit::Retile;
        if (opt_.max_records >= 0 && res_.records >= opt_.max_records) {
          stop_ = "max_records";
          break;
        }
        // the record: the state where the frame ended, then the next frame head's clock
        std::vector<uint64_t> hs(sim.size());
        const uint8_t *s = ctx_.bytes();
        for (size_t j = 0; j < sim.size(); ++j) hs[j] = object_hash(s, *sim[j]);
        ++frame_;
        uint32_t t = 0;
        in_head_ = true;
        const bool more = host_.frame_head(ctx_, frame_, t);
        in_head_ = false;
        if (!more) {
          head_writes_.clear();
          stop_ = "host";
          break;
        }
        begin_frame();
        frame_head_record(t);
        if (opt_.keep_records) {
          res_.T.push_back(t);
          res_.hashes.insert(res_.hashes.end(), hs.begin(), hs.end());
        }
        if (opt_.on_record) opt_.on_record(opt_.on_record_arg, res_.records, t, hs.data(), hs.size());  // roadmap S7.8
        ++res_.records;
        FrameInput fi;
        fi.t_head = t;
        fi.retile = retile;
        fi.live = this;
        fi.on_wait = &Core::on_wait;
        fi.on_wait_arg = this;
        tk::FrameIoStats st2;
        ex = tk::tick_frame_io(ctx_, fi, st2);
      }
    } catch (const LiveStop &) {
      if (stop_.empty()) stop_ = "host";
    } catch (const NeedInput &e) {
      // a Blocking pump found the host with no input left at all (InputSource::exhausted): the end of the session
      stop_ = std::string("need_input (") + (e.what ? e.what : "?") + ")";
      gfx_end_of_input(ctx_);
    } catch (...) {
      finish(quit);
      res_.stop = "error";
      throw;
    }
    finish(quit);
  }

 private:
  struct Call {  // a pump call that took nothing, since the last dispatch of the frame (the replay sees them)
    bool body = false;
    PumpKind kind = PumpKind::Drain;
    size_t count = 0;   // cursor reads made before it
    int32_t pace = 0;   // the paced loop's site (0: unpaced)
    uint32_t consumed = 0;  // clock reads at `pace` the port had taken before it
  };

  uint32_t ra(int32_t site) const { return opt_.frame_ra + static_cast<uint32_t>(site); }

  // the pump calls of this frame since the last dispatch that pass TraceInput::batch's class test for a dispatch of this
  // class (refused_at_ is untouched by the others): body calls of any kind for a body dispatch, modal Blocking calls for a
  // modal one (a modal Drain pump is never recorded in the burst)
  // (U-1604) a load dispatch (recorded at a PumpKind::Load call: modal, `load` in parse_trace) passes the class test of the
  // modal Blocking calls and of the Load calls; any other modal dispatch only the modal Blocking calls'
  std::vector<Call> same_class(bool body, bool load = false) const {
    std::vector<Call> v;
    for (const Call &j : burst_)
      if (j.body == body &&
          (j.body || j.kind == PumpKind::Blocking || (load && j.kind == PumpKind::Load)))
        v.push_back(j);
    return v;
  }
  // TraceInput::batch over `calls` (in order) for a dispatch recorded now with cursor count `count` (a pass read in front of
  // it when the loop is paced): the index of the call that takes it, -1 when none does
  int taker(const std::vector<Call> &calls, size_t count, bool paced) const {
    size_t refused_at = SIZE_MAX;
    for (size_t i = 0; i < calls.size(); ++i) {
      const Call &j = calls[i];
      if (j.count < count) {                                       // the cursor test, with TraceInput's `stuck` rule
        const bool stuck = j.kind == PumpKind::Blocking && refused_at == j.count;
        refused_at = j.count;
        if (!stuck) continue;
      }
      if (j.kind == PumpKind::Blocking && j.pace != 0) {           // PACE: at most one pass read unread before it
        auto it = recorded_.find(j.pace);
        const uint32_t rec = (it == recorded_.end() ? 0u : it->second) + (paced && pace_ == j.pace ? 1u : 0u);
        if (rec - j.consumed > 1) continue;
      }
      return static_cast<int>(i);
    }
    return -1;
  }

  void violation(const char *what) {
    ++res_.violations;
    if (res_.first_violation < 0) {
      res_.first_violation = res_.records;
      res_.first_violation_what = what;
    }
  }

  // a separator: one clock record that ends the open batch and sets parse_trace's batch class for the next dispatch
  void separator(bool body) {
    if (!rec_.on) return;
    if (body) {
      rec_.clock(2, kPaletteQpcRa, next_q());
      last_body_ctx_ = true;
    } else {
      rec_.clock(1, ra(kSiteWaitTicks), last_t_);
      last_body_ctx_ = false;
      ++recorded_[kSiteWaitTicks];
    }
    ++ordinal_;
    open_batch_ = false;
    writes_pending_ = false;
  }
  uint64_t next_q() {
    const uint64_t q = q_;
    q_ += 10000u;
    return q;
  }

  // a timeGetTime answer the port reads: recorded at its site (while a pass read stands in front of a dispatch, other
  // reads go in front of it, in read order, so the replay's clock scan meets them first)
  void answer_tgt(int32_t site, uint32_t t, bool source) {
    last_t_ = t;
    if (!rec_.on) {
      ++ordinal_;
      return;
    }
    if (source && placeholder_ != SIZE_MAX && site == placeholder_site_) {
      fill_placeholder(t);
      return;
    }
    ++ordinal_;
    ++recorded_[site];
    if (placeholder_ != SIZE_MAX) {  // in front of the pass read: the context after it stays the pass read's
      rec_.insert_clock_before(placeholder_, 1, ra(site), t);
      if (cursor_ph_ != SIZE_MAX && cursor_ph_ >= placeholder_) ++cursor_ph_;
      ++placeholder_;
      return;
    }
    rec_.clock(1, ra(site), t);
    last_body_ctx_ = false;  // any site but the frame head (frame_head_record) gives a modal context
    open_batch_ = false;
    writes_pending_ = false;
  }
  void fill_placeholder(uint32_t t) {
    rec_.ev[placeholder_].v = t;
    placeholder_ = SIZE_MAX;
  }

  void cursor_answer(bool modal, int32_t &x, int32_t &y) {
    host_.cursor(ctx_, modal, x, y);
    if ((mode_ & 15) == 3 && cursor_answers_ == static_cast<uint64_t>(mode_ >> 4)) host_.cursor(ctx_, modal, x, y);
    if ((mode_ & 15) == 4 && cursor_answers_ == static_cast<uint64_t>(mode_ >> 4)) ++x;  // control 4: one read off by 1
    ++cursor_answers_;
    ++res_.cursors;
    last_cx_ = x;
    last_cy_ = y;
    if (!rec_.on) return;
    if (cursor_ph_ != SIZE_MAX) {  // the CURSOR record an inflated dispatch put in front of itself stands for this read
      if (writes_pending_) violation("a harness write between an inflated dispatch and the cursor read its record stands for");
      rec_.ev[cursor_ph_].a = static_cast<uint32_t>(x);
      rec_.ev[cursor_ph_].b = static_cast<uint32_t>(y);
      cursor_ph_ = SIZE_MAX;
      gfx_progress_fast(ctx_, ordinal_);
      return;
    }
    if (drop_cursor_) {  // negative control 5: the read after an idle pass left out of the recording
      drop_cursor_ = false;
      return;
    }
    // TraceInput::cursor applies the harness's writes made before a modal read: a write must be recorded before it with a
    // clock call between
    if (writes_pending_) separator(last_body_ctx_);
    rec_.rec(0x08, 1, static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    gfx_progress_fast(ctx_, ordinal_);
  }

  void record_batch(bool body, bool paced, const std::vector<Command> &cmds, bool inflate, bool load) {
    if (!rec_.on) return;
    write_batch(body, paced, cmds, inflate);
    // U-1604: parse_trace marks a modal dispatch `load` when the clock record right after its handlers is jgld's Palette::update
    // QPC (the world load's first palette update); TraceInput hands such a batch to loadFlic's Load pump
    if (load && (mode_ & 15) != 7) separator(true);  // negative control 7: the load marker left out
    ++batches_;
  }
  // The window message a root handler serves, for the MSG record that ends a pending focus key in front of it (parse_trace
  // gives a key MSG record with no HANDLER before the next MSG / clock / FRAME record to the focus child, and a HANDLER right
  // after a pending key would count as that key's own)
  static uint32_t handler_msg(const Command &c) {
    switch (c.handler) {
      case 2: return 0x201;
      case 1: case 3: case 10: return 0x202;
      case 5: return 0x204;
      case 4: case 6: case 11: return 0x205;
      case 7: return c.a0 == 0 ? 0x100u : 0x102u;
      case 8: return 0x100;
      case 9: return 0x101;
      default: return 0;
    }
  }
  // a MSG record; a first-generation input message (re/tools/trace_format.py is_first_generation_input) is preceded by a
  // DELIVER post, as trace_format.validate requires (U-1067)
  void msg_record(uint32_t msg, uint32_t wp, uint32_t lp, uint32_t g) {
    if (msg == 0x100 || msg == 0x101 || msg == 0x104 || msg == 0x105 || (msg >= 0x201 && msg <= 0x209))
      rec_.rec(0x03, 3, msg, wp, lp, g);
    rec_.rec(0x05, 0, msg, wp, lp, g);
  }
  void write_batch(bool body, bool paced, const std::vector<Command> &cmds, bool inflate) {
    // PACE: a paced loop's dispatch has the pass read the port makes next in front of it (TraceInput::batch delivers when at
    // most one read at the pace site before the dispatch is unread); its value is filled in when the port reads it
    if (paced) {
      if (placeholder_ != SIZE_MAX) fill_placeholder(last_t_);
      placeholder_ = rec_.clock(1, ra(pace_), last_t_);
      placeholder_site_ = pace_;
      ++ordinal_;
      ++recorded_[pace_];
      last_body_ctx_ = false;
      open_batch_ = false;
      writes_pending_ = false;
    }
    // parse_trace's class: a body dispatch follows the frame head or the palette QPC; a modal one any other clock site; and
    // consecutive HANDLER records with no clock record between are one dispatch
    if (body ? (!last_body_ctx_ || open_batch_ || writes_pending_) : (last_body_ctx_ || open_batch_ || writes_pending_))
      separator(body);
    // roadmap S7.6: the CURSOR record that makes the replay refuse this dispatch at the loop's earlier call at the same count
    // and take it here through the `stuck` rule; the port's next cursor read fills it (cursor_answer)
    // negative control 10 (roadmap S7.9): the n-th inflated dispatch (n = mode >> 4) is recorded without that CURSOR record, so
    // the replay hands it to the loop's earlier call at the same cursor count
    const bool drop_front = inflate && (mode_ & 15) == 10 && res_.inflated == static_cast<uint64_t>(mode_ >> 4);
    if (inflate && !drop_front) {
      cursor_ph_ = rec_.ev.size();
      rec_.rec(0x08, 1, static_cast<uint32_t>(last_cx_), static_cast<uint32_t>(last_cy_));
    }
    const uint32_t g = static_cast<uint32_t>(batches_) + 1;
    bool key_pending = false;  // a focus key's MSG record no other record has resolved yet
    for (const Command &c : cmds) {
      if (c.handler == 12 || c.handler == 13) {  // U-1604: a key the focus child took (parse_trace: MSG with no HANDLER)
        if ((mode_ & 15) == 8) continue;  // negative control 8: the focus keys left out of the recording
        msg_record(c.handler == 12 ? 0x100u : 0x102u, static_cast<uint32_t>(c.a0), 0, g);
        key_pending = true;
        continue;
      }
      if (key_pending) {  // the handler's own message first, so the pending key resolves as the focus child's
        msg_record(handler_msg(c), 0, 0, g);
        key_pending = false;
      }
      rec_.rec(0x09, static_cast<uint8_t>(c.handler), static_cast<uint32_t>(c.a0), static_cast<uint32_t>(c.a1), 0, g);
      if (c.handler == 1 || c.handler == 4) {  // the press handlers' Ctrl / Shift reads (Command::async_*)
        rec_.rec(0x06, 2, ra(c.handler == 1 ? kSiteCtrlPress1 : kSiteCtrlPress2), 0x11, c.async_ctrl & 0xffffu, g);
        rec_.rec(0x06, 2, ra(c.handler == 1 ? kSiteShiftPress1 : kSiteShiftPress2), 0x10, c.async_shift & 0xffffu, g);
      }
      if ((c.handler == 2 || c.handler == 3 || c.handler == 7) && c.t != 0) {  // the deadline tail's clock (Command::t)
        rec_.ev[rec_.clock(1, ra(kSiteHandlerClock), c.t)].grp = g;
        ++ordinal_;
      }
    }
    open_batch_ = true;
  }

  static void on_wait(void *self) {
    Core &c = *static_cast<Core *>(self);
    c.host_.wait_point(c.ctx_);
    // roadmap S7.6: the clock record after H1's frame wait (LiveMarker positions; a modal context, and no pump of the frame
    // body follows the wait in the frame)
    c.separator(false);
  }

  void begin_frame() {
    burst_.clear();
    recorded_.clear();
    consumed_.clear();
    placeholder_ = SIZE_MAX;
  }
  void fill_cursor_placeholder() {
    if (cursor_ph_ == SIZE_MAX) return;
    rec_.ev[cursor_ph_].a = static_cast<uint32_t>(last_cx_);
    rec_.ev[cursor_ph_].b = static_cast<uint32_t>(last_cy_);
    cursor_ph_ = SIZE_MAX;
  }
  void end_frame() {
    if (placeholder_ != SIZE_MAX) fill_placeholder(last_t_);
    fill_cursor_placeholder();
    host_.frame_end(ctx_, frame_);
    maybe_spill();
  }
  // roadmap S7.8 (U-1643): with a trace file, the events of finished frames go to it (end_frame has filled both placeholders,
  // so no held index points into them); not under the dispatch-swap control, which reorders the whole recording at the end
  void maybe_spill() {
    if (!spill_on_ || (mode_ & 15) == 2 || rec_.ev.size() < opt_.spill_events) return;
    if (placeholder_ != SIZE_MAX || cursor_ph_ != SIZE_MAX) return;
    rec_.spill(rec_.ev.size());
  }
  void frame_head_record(uint32_t t) {
    last_t_ = t;
    if (!rec_.on) {
      ++ordinal_;
      return;
    }
    rec_.clock(1, ra(kSiteFrameHead), t);
    ++ordinal_;
    rec_.frame(t, snapshot());
    last_body_ctx_ = true;
    open_batch_ = false;
    for (const HeadItem &w : head_writes_) {
      if (w.sub == 7) rec_call(w.a, w.d, w.b, w.c);
      else rec_.rec(0x03, 4, w.a, w.b);
    }
    writes_pending_ = !head_writes_.empty();
    head_writes_.clear();
  }
  // the FRAME record's 0x60 bytes (trace_schema.md): g_rootInput, the key code and key words, the character words,
  // g_ui83ab9c, g_flags, g_date, g_rng
  std::vector<uint8_t> snapshot() {
    std::vector<uint8_t> s;
    const uint32_t *va = snapshot_vas();
    static const uint32_t lens[9] = {0x40, 4, 4, 4, 4, 4, 4, 4, 4};
    for (int i = 0; i < 9; ++i) {
      const uint8_t *p = ctx_.mem.at(va[i], lens[i]);
      s.insert(s.end(), p, p + lens[i]);
    }
    return s;
  }
  uint32_t *snapshot_vas() {
    snap_va_[0] = state_va(ctx_, &GameState::g_rootInput);
    snap_va_[1] = state_va(ctx_, &GameState::gDirtyFlagA);
    snap_va_[2] = state_va(ctx_, &GameState::gDirtyFlagB);
    snap_va_[3] = state_va(ctx_, &GameState::g_char83821c);
    snap_va_[4] = state_va(ctx_, &GameState::g_key838208);
    snap_va_[5] = state_va(ctx_, &GameState::g_ui83ab9c);
    snap_va_[6] = state_va(ctx_, &GameState::g_flags);
    snap_va_[7] = state_va(ctx_, &GameState::g_date);
    snap_va_[8] = state_va(ctx_, &GameState::g_rng);
    return snap_va_;
  }
  void finish(bool quit) {
    if (placeholder_ != SIZE_MAX) fill_placeholder(last_t_);
    fill_cursor_placeholder();
    if ((mode_ & 15) == 2) rec_.swap_groups(static_cast<uint32_t>(mode_ >> 4) + 1);  // control: two dispatches reordered
    res_.stop = stop_;
    res_.mark_pos.clear();
    res_.held_events = rec_.ev.size();
    if (spill_on_) {
      res_.spilled_events = rec_.sp_events;
      res_.trace_file_bytes = rec_.spill_finish(quit, snapshot_vas(), &res_.mark_pos);
    } else if (rec_.on) {
      res_.trace = rec_.bytes(quit, snapshot_vas(), &res_.mark_pos);
    }
    if (res_.mark_pos.size() < marks_) res_.mark_pos.resize(marks_, 0);
  }

  Ctx &ctx_;
  LiveHost &host_;
  const LiveOptions &opt_;
  LiveResult &res_;
  Recorder rec_;
  bool spill_on_ = false;  // roadmap S7.8: the recording goes to LiveOptions::trace_file
  int mode_ = 0;
  uint32_t frame_ = 0;
  uint32_t ordinal_ = 0;
  uint32_t last_t_ = 0x00100000u;
  uint64_t q_ = 0x10000000u;
  int32_t pace_ = 0;
  std::vector<Call> burst_;
  std::map<int32_t, uint32_t> recorded_, consumed_;
  size_t placeholder_ = SIZE_MAX;
  int32_t placeholder_site_ = 0;
  bool last_body_ctx_ = true;  // parse_trace starts with the frame_ra context
  bool open_batch_ = false;
  bool writes_pending_ = false;
  bool in_head_ = false;
  struct HeadItem {  // a write (sub 4: a = va, b = value) or a call (sub 7: a = addr, b / c = arguments, d = argc)
    uint8_t sub;
    uint32_t a, b, c, d;
  };
  std::vector<HeadItem> head_writes_;
  uint64_t clock_answers_ = 0, cursor_answers_ = 0, batches_ = 0;
  // roadmap S7.6
  size_t cursor_ph_ = SIZE_MAX;   // the CURSOR record an inflated dispatch put in front of itself, until a read fills it
  int32_t last_cx_ = 0, last_cy_ = 0;
  uint32_t marks_ = 0;
  uint64_t idle_count_ = 0, host_dispatches_ = 0;
  uint32_t timed_ms_ = 0;  // P5A: InputSource::timed_pass's hint for the next batch call
  bool drop_cursor_ = false;      // negative control 5
  std::string stop_;
  uint32_t snap_va_[9] = {};
};

// ------------------------------------------------------------------------------------------------ SCRIPT HOST
// A scripted player (tests): one step per line, `#` starts a comment. The script runs as the port asks: a step that waits
// holds the script until its condition, input steps queue dispatches the host gives one per pump call that may take one.
//   seed T             the clock's first value (default 0x100000); every clock answer is the previous + 1
//   frame_ms N         a frame head is at least N ms after the previous one (default 99, the original's frame under the
//                      virtual clock: re/analysis/sim/time_model.md)
//   cursor X Y         the point every later cursor read answers
//   async VK V         GetAsyncKeyState(VK) answers V
//   click X Y          cursor X Y, then two dispatches: onPoint(X, Y) and onPress1(X, Y) + onUnpoint (a left click)
//   rclick X Y         the same with onPoint2 / onPress2 + onUnpoint2 (a right click)
//   key VK [CH]        onChar(VK) + onKey(CH, VK), then onChar0 (a key press and release; CH defaults to VK)
//   vkey VK            onChar(VK), then onChar0 (a key with no character: the report-key switch's F keys, arrows)
//   batch H:A0:A1 ...  one dispatch of raw root-handler calls
//   wait_frames N      the script waits for N frame heads
//   wait_modal         the script waits for a modal loop's Blocking pump (input queued after it goes to that loop)
//   wait_body          the script waits for a pump of the frame body
//   wait_passes N      the script waits for N declined pump calls of a paced loop
//   wait_flags M V     the script waits for a frame head where (g_flags & M) == V
//   idle N             (roadmap S7.6) the script declines the next N idle_ok pump calls (idle passes of a modal loop: the
//                      player only moves the cursor; `cursor` steps after it move it between the passes)
//   mark               (roadmap S7.6) a LiveMarker::mark at the next pump call (before its answer)
//   stop               the session ends (at the next frame head or pump call)
// At a must_deliver or idle_ok pump with nothing queued the script skips its waits to the next input step (the player
// acts); a script that has none left stops there.
class ScriptHost : public LiveHost {
 public:
  bool parse(const std::string &text, std::string *why) {
    std::istringstream in(text);
    std::string line;
    int ln = 0;
    while (std::getline(in, line)) {
      ++ln;
      const size_t hash = line.find('#');
      if (hash != std::string::npos) line.resize(hash);
      std::istringstream ls(line);
      Step s;
      if (!(ls >> s.op)) continue;
      std::string tok;
      while (ls >> tok) {
        if (s.op == "batch") {
          s.toks.push_back(tok);
          continue;
        }
        char *end = nullptr;
        const long long v = std::strtoll(tok.c_str(), &end, 0);
        if (!end || *end) {
          if (why) *why = "script line " + std::to_string(ln) + ": bad number " + tok;
          return false;
        }
        s.n.push_back(v);
      }
      static const char *const ops[] = {"seed", "frame_ms", "cursor", "async", "click", "rclick", "key", "vkey", "batch",
                                        "wait_frames", "wait_modal", "wait_body", "wait_passes", "wait_flags", "idle",
                                        "mark", "stop", "wait_load"};
      bool known = false;
      for (const char *o : ops) known = known || s.op == o;
      if (!known) {
        if (why) *why = "script line " + std::to_string(ln) + ": unknown step " + s.op;
        return false;
      }
      steps_.push_back(std::move(s));
    }
    return true;
  }

  void boot(Ctx &c) override { advance(c, Event::None); }
  // roadmap S7.7 (PrefixHost): the script answers from here on, continuing a trace host's clock, cursor and key state
  void take_over(Ctx &c, uint64_t now, uint64_t last_head, int32_t cx, int32_t cy, const uint16_t *keys) {
    now_ = now;
    last_head_ = last_head;
    cx_ = cx;
    cy_ = cy;
    std::memcpy(keys_, keys, sizeof keys_);
    advance(c, Event::None);
  }
  bool frame_head(Ctx &c, uint32_t, uint32_t &t) override {
    ++frames_;
    if (wait_ == Wait::Frames && frames_ >= wait_until_) wait_ = Wait::None;
    if (wait_ == Wait::Flags && (static_cast<uint32_t>(c.gs().g_flags) & flag_mask_) == flag_value_) wait_ = Wait::None;
    advance(c, Event::None);
    if (stopped_) return false;
    now_ = std::max<uint64_t>(now_ + 1, last_head_ + frame_ms_);
    last_head_ = now_;
    t = static_cast<uint32_t>(now_);
    return true;
  }
  bool clock(Ctx &, int32_t, bool, uint32_t &t) override {
    t = static_cast<uint32_t>(++now_);
    return true;
  }
  void cursor(Ctx &, bool, int32_t &x, int32_t &y) override {
    x = cx_;
    y = cy_;
  }
  uint16_t async_key(Ctx &, int vk) override { return keys_[vk & 0xff]; }
  // a `mark` step: LiveMarker::mark, and with a gfx backend attached the back buffer as it is now into the files overlay as
  // `$live/mark_<k>.555` (what a presenting host shows at this call; a replay's gfx.snapshotAt(mark_pos[k]) must equal it)
  void do_mark(Ctx &c) {
    mark_pending_ = false;
    if (!marker) return;
    const uint32_t k = marker->mark();
    if (!gfx(c) || !c.files_ptr) return;
    const int i = registry_find("gfx.dump");
    if (i < 0) return;
    const uint32_t a0 = 0;
    Ret r;
    registry_at(i, nullptr)->fn(c, &a0, r);
    const int64_t n = c.files().size("$gfx/back.555");
    if (n <= 0) return;
    std::vector<uint8_t> b(static_cast<size_t>(n));
    c.files().read("$gfx/back.555", 0, b.data(), b.size());
    c.files().write("$live/mark_" + std::to_string(k) + ".555", b);
  }
  LiveReply pump(Ctx &c, const LivePump &p, std::vector<Command> &out) override {
    const LiveReply r = pump_(c, p, out);
    // development: SG_LIVE_SCRIPT_LOG=<file> appends one line per pump call the script host answers
    static FILE *fh = std::getenv("SG_LIVE_SCRIPT_LOG") ? std::fopen(std::getenv("SG_LIVE_SCRIPT_LOG"), "w") : nullptr;
    if (fh) {
      std::fprintf(fh, "frame %u kind %u body %d reads %u may %d must %d idle %d -> %d (step %zu, wait %d, queue %zu, %zu cmds)\n",
                   p.frame, static_cast<unsigned>(p.kind), p.body ? 1 : 0, p.cursor_reads, p.may_deliver ? 1 : 0,
                   p.must_deliver ? 1 : 0, p.idle_ok ? 1 : 0, static_cast<int>(r), pc_, static_cast<int>(wait_),
                   queue_.size(), out.size());
      std::fflush(fh);
    }
    return r;
  }
  LiveReply pump_(Ctx &c, const LivePump &p, std::vector<Command> &out) {
    if (!p.body && wait_ == Wait::Modal) wait_ = Wait::None;
    if (p.body && wait_ == Wait::Body) wait_ = Wait::None;
    advance(c, Event::None);
    if (mark_pending_) do_mark(c);
    // U-1604 (S7.7): a world load's pump (PumpKind::Load) takes scripted input only after a `wait_load` step: the step after it
    // is given at the first Load call the core lets deliver (may_deliver); any other Load call declines
    if (p.kind == PumpKind::Load) {
      if (wait_ == Wait::Load) {
        wait_ = Wait::None;
        load_armed_ = true;
        advance(c, Event::None);
      }
      if (stopped_) return LiveReply::Stop;
      if (load_armed_ && !queue_.empty() && p.may_deliver) {
        load_armed_ = false;
        out = queue_.front();
        queue_.erase(queue_.begin());
        return LiveReply::Deliver;
      }
      return LiveReply::Decline;
    }
    // while a `wait_load` waits (or its input waits for a Load call), other pump calls decline: the load comes later
    if (wait_ == Wait::Load || load_armed_) return LiveReply::Decline;
    if (wait_ == Wait::Idle && p.idle_ok) {  // an idle pass (roadmap S7.6)
      if (--idle_left_ == 0) wait_ = Wait::None;
      return LiveReply::Decline;
    }
    const bool act = p.must_deliver || p.idle_ok;  // a call the player answers now (S7.1: every unpaced modal Blocking call)
    if (queue_.empty() && act && wait_ != Wait::Idle) {
      wait_ = Wait::None;  // the player acts now: skip to the next input step
      advance(c, Event::None);
      if (mark_pending_) do_mark(c);
      if (wait_ == Wait::Idle && p.idle_ok) {
        if (--idle_left_ == 0) wait_ = Wait::None;
        return LiveReply::Decline;
      }
    }
    if (stopped_) return LiveReply::Stop;
    if (!queue_.empty() && p.may_deliver) {
      out = queue_.front();
      queue_.erase(queue_.begin());
      return LiveReply::Deliver;
    }
    if (p.idle_ok && !queue_.empty()) return LiveReply::Decline;  // the replay takes it at a later call of the loop
    if (act) return LiveReply::Stop;  // nothing left to give
    if (wait_ == Wait::Passes && p.pace_site != 0 && ++passes_ >= wait_until_) wait_ = Wait::None;
    return LiveReply::Decline;
  }

 private:
  enum class Wait { None, Frames, Modal, Body, Passes, Flags, Idle, Load };
  enum class Event { None };
  struct Step {
    std::string op;
    std::vector<long long> n;
    std::vector<std::string> toks;
  };
  static Command rh(uint32_t h, int32_t a0, int32_t a1) {
    Command c;
    c.kind = CmdKind::RootHandler;
    c.handler = h;
    c.a0 = a0;
    c.a1 = a1;
    return c;
  }
  long long arg(const Step &s, size_t i, long long dflt = 0) const { return i < s.n.size() ? s.n[i] : dflt; }
  // runs steps until one waits (or a queued dispatch must be given first)
  void advance(Ctx &, Event) {
    while (!stopped_ && wait_ == Wait::None && queue_.empty() && pc_ < steps_.size()) {
      const Step &s = steps_[pc_++];
      if (s.op == "seed") {
        now_ = static_cast<uint64_t>(arg(s, 0));
        last_head_ = 0;
      } else if (s.op == "frame_ms") {
        frame_ms_ = static_cast<uint64_t>(arg(s, 0, 99));
      } else if (s.op == "cursor") {
        cx_ = static_cast<int32_t>(arg(s, 0));
        cy_ = static_cast<int32_t>(arg(s, 1));
      } else if (s.op == "async") {
        keys_[arg(s, 0) & 0xff] = static_cast<uint16_t>(arg(s, 1));
      } else if (s.op == "click" || s.op == "rclick") {
        const bool r = s.op == "rclick";
        cx_ = static_cast<int32_t>(arg(s, 0));
        cy_ = static_cast<int32_t>(arg(s, 1));
        queue_.push_back({rh(r ? 5 : 2, cx_, cy_)});
        queue_.push_back({rh(r ? 4 : 1, cx_, cy_), rh(r ? 6 : 3, 0, 0)});
      } else if (s.op == "key") {
        const int32_t vk = static_cast<int32_t>(arg(s, 0)), ch = static_cast<int32_t>(arg(s, 1, arg(s, 0)));
        queue_.push_back({rh(8, vk, 0), rh(7, ch, vk)});
        queue_.push_back({rh(9, 0, 0)});
      } else if (s.op == "vkey") {
        queue_.push_back({rh(8, static_cast<int32_t>(arg(s, 0)), 0)});
        queue_.push_back({rh(9, 0, 0)});
      } else if (s.op == "batch") {
        std::vector<Command> b;
        for (const std::string &t : s.toks) {
          long long h = 0, a0 = 0, a1 = 0;
          std::sscanf(t.c_str(), "%lli:%lli:%lli", &h, &a0, &a1);
          b.push_back(rh(static_cast<uint32_t>(h), static_cast<int32_t>(a0), static_cast<int32_t>(a1)));
        }
        if (!b.empty()) queue_.push_back(std::move(b));
      } else if (s.op == "wait_frames") {
        wait_ = Wait::Frames;
        wait_until_ = frames_ + static_cast<uint64_t>(arg(s, 0));
      } else if (s.op == "wait_modal") {
        wait_ = Wait::Modal;
      } else if (s.op == "wait_body") {
        wait_ = Wait::Body;
      } else if (s.op == "wait_passes") {
        wait_ = Wait::Passes;
        passes_ = 0;
        wait_until_ = static_cast<uint64_t>(arg(s, 0));
      } else if (s.op == "wait_flags") {
        wait_ = Wait::Flags;
        flag_mask_ = static_cast<uint32_t>(arg(s, 0));
        flag_value_ = static_cast<uint32_t>(arg(s, 1));
      } else if (s.op == "idle") {
        idle_left_ = static_cast<uint64_t>(arg(s, 0, 1));
        if (idle_left_) wait_ = Wait::Idle;
      } else if (s.op == "mark") {
        mark_pending_ = true;
      } else if (s.op == "stop") {
        stopped_ = true;
      } else if (s.op == "wait_load") {
        wait_ = Wait::Load;
      }
    }
    if (pc_ >= steps_.size() && queue_.empty() && wait_ == Wait::None) stopped_ = true;  // the script ran out
  }

  std::vector<Step> steps_;
  size_t pc_ = 0;
  std::vector<std::vector<Command>> queue_;
  Wait wait_ = Wait::None;
  uint64_t wait_until_ = 0, frames_ = 0, passes_ = 0, idle_left_ = 0;
  uint32_t flag_mask_ = 0, flag_value_ = 0;
  bool stopped_ = false, mark_pending_ = false, load_armed_ = false;
  uint64_t now_ = 0x00100000u, last_head_ = 0, frame_ms_ = 99;
  int32_t cx_ = 0, cy_ = 0;
  uint16_t keys_[256] = {};
};

// ------------------------------------------------------------------------------------------------ registry
const FuncDef kFuncs[] = {
    // the recorder's own switch (no corpus key: run_live runs it); calling it does nothing
    {"live.recorder", "default", "void", 0, 0, [](Ctx &, const uint32_t *, Ret &) {}, &g_negctl_recorder},
};

}  // namespace

void run_live(Ctx &ctx, LiveHost &host, const LiveOptions &opt, LiveResult &res) {
  Core core(ctx, host, opt, res);
  core.run();
}

std::unique_ptr<LiveHost> make_script_live_host(const std::string &script, std::string *why) {
  auto h = std::make_unique<ScriptHost>();
  if (!h->parse(script, why)) return nullptr;
  return h;
}

namespace {
// ------------------------------------------------------------------------------------------------ PREFIX HOST (roadmap S7.7)
// A corpus trace answers until the first dispatch it would give that holds a key the window tree's focus child took
// (RootHandler 12 / 13, U-1604); from that pump call on a script answers (the trace's batch there is dropped), starting
// from the trace's last clock value, frame-head value, cursor and GetAsyncKeyState answers. It reaches a state no script
// alone reaches (cov_r_bio's character editor on a golfer of mood5.sve, opened from a click the record run found from the
// golfer's name label) and then lets the script decide the input there.
class PrefixHost : public LiveHost {
 public:
  PrefixHost(std::unique_ptr<LiveHost> trace, std::unique_ptr<ScriptHost> script)
      : a_(std::move(trace)), b_(std::move(script)) {}
  void boot(Ctx &c) override {
    a_->writes = b_->writes = writes;
    a_->marker = b_->marker = marker;
    a_->boot(c);
  }
  bool frame_head(Ctx &c, uint32_t frame, uint32_t &t) override {
    if (on_b_) return b_->frame_head(c, frame, t);
    const bool ok = a_->frame_head(c, frame, t);
    if (ok) last_t_ = last_head_ = t;
    return ok;
  }
  void frame_end(Ctx &c, uint32_t frame) override { (on_b_ ? static_cast<LiveHost &>(*b_) : *a_).frame_end(c, frame); }
  bool clock(Ctx &c, int32_t site, bool body, uint32_t &t) override {
    if (on_b_) return b_->clock(c, site, body, t);
    const bool ok = a_->clock(c, site, body, t);
    if (ok) last_t_ = t;
    return ok;
  }
  void cursor(Ctx &c, bool modal, int32_t &x, int32_t &y) override {
    if (on_b_) return b_->cursor(c, modal, x, y);
    a_->cursor(c, modal, x, y);
    cx_ = x;
    cy_ = y;
  }
  uint16_t async_key(Ctx &c, int vk) override {
    if (on_b_) return b_->async_key(c, vk);
    return keys_[vk & 0xff] = a_->async_key(c, vk);
  }
  LiveReply pump(Ctx &c, const LivePump &p, std::vector<Command> &out) override {
    if (on_b_) return b_->pump(c, p, out);
    const LiveReply r = a_->pump(c, p, out);
    if (r != LiveReply::Deliver) return r;
    bool focus = false;
    for (const Command &k : out) focus = focus || (k.kind == CmdKind::RootHandler && (k.handler == 12 || k.handler == 13));
    if (!focus) return r;
    out.clear();
    on_b_ = true;
    b_->take_over(c, last_t_, last_head_, cx_, cy_, keys_);
    return b_->pump(c, p, out);
  }
  bool exhausted() const override { return on_b_ ? b_->exhausted() : a_->exhausted(); }
  void pace(Ctx &c, int32_t site) override { (on_b_ ? static_cast<LiveHost &>(*b_) : *a_).pace(c, site); }
  void wait_point(Ctx &c) override { (on_b_ ? static_cast<LiveHost &>(*b_) : *a_).wait_point(c); }
  void present_point(Ctx &c) override { (on_b_ ? static_cast<LiveHost &>(*b_) : *a_).present_point(c); }
  void title_menu(Ctx &c) override { (on_b_ ? static_cast<LiveHost &>(*b_) : *a_).title_menu(c); }  // P6B
  bool blocking() const override { return on_b_ ? b_->blocking() : a_->blocking(); }

 private:
  std::unique_ptr<LiveHost> a_;
  std::unique_ptr<ScriptHost> b_;
  bool on_b_ = false;
  uint64_t last_t_ = 0, last_head_ = 0;
  int32_t cx_ = 0, cy_ = 0;
  uint16_t keys_[256] = {};
};
}  // namespace

std::unique_ptr<LiveHost> make_prefix_live_host(Ctx &ctx, const uint8_t *trace, size_t n, const std::string &script,
                                                std::string *why) {
  auto a = make_trace_live_host(ctx, trace, n, why);
  if (!a) return nullptr;
  auto b = std::make_unique<ScriptHost>();
  if (!b->parse(script, why)) return nullptr;
  return std::make_unique<PrefixHost>(std::move(a), std::move(b));
}

extern const ModuleDef kModule_live = {"live", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
