// simgolf_input_check: judges portapp::Translator against the recorded traces (roadmap S7.2).
//
//   simgolf_input_check [--negctl N] [--json out.json] [--list K] [--selftest] trace...
//
// For every MSG record of a trace the translator gets the message, the lParam / wParam the trace recorded and a HostInput
// that answers its reads from the records that follow the MSG (GetKeyState, GetAsyncKeyState, MapVirtualKeyA) and from the
// harness' virtual cursor (the DELIVER cursor records; the initial one is the trace's first CURSOR read), and decides the
// focus ring the way tick.cpp parse_trace does (a key message no HANDLER record follows was taken by a focused child).
// The steps it returns must equal the records between the MSG and the next MSG / clock read / frame head / poll:
//   - every HANDLER record: id, a0, a1 (and for handlers 1 / 4 the Ctrl / Shift results of the KEYSTATE records after it),
//   - every owned read (GetKeyState x3 + MapVirtualKeyA before a key-down, GetKeyState x3 before a key-up, the two
//     GetAsyncKeyState of a click, the cursor read of a WM_MOUSEMOVE) in order with the answer the trace records,
//   - a focus child's key (handlers 12 / 13) exactly when no HANDLER record follows a WM_KEYDOWN / WM_CHAR.
// No HANDLER record may lie outside a translated message's window. Mismatches are listed; the exit code is 0 either way
// (the test reads the summary).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "input_map.h"

using namespace portapp;

namespace {

enum : uint8_t {
  kClockRun = 0x01, kFrame = 0x02, kDeliver = 0x03, kPoll = 0x04, kMsg = 0x05, kKeyState = 0x06, kKbState = 0x07,
  kCursor = 0x08, kHandlerRec = 0x09, kTake = 0x0a, kSleep = 0x0b, kMapVk = 0x0c, kTimerSet = 0x0d, kEnd = 0x7f,
};

struct Rec {
  uint8_t type = 0, sub = 0;
  const uint8_t *p = nullptr;
  uint32_t len = 0;
  uint32_t u32(size_t off) const {
    return uint32_t(p[off]) | uint32_t(p[off + 1]) << 8 | uint32_t(p[off + 2]) << 16 | uint32_t(p[off + 3]) << 24;
  }
  int32_t i32(size_t off) const { return static_cast<int32_t>(u32(off)); }
};

struct TraceFile {
  std::string name;
  std::vector<uint8_t> data;
  std::vector<Rec> recs;
};

bool load(const std::string &path, TraceFile &tf, std::string &why) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    why = "cannot open";
    return false;
  }
  tf.data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  const auto &d = tf.data;
  if (d.size() < 32 || std::memcmp(d.data(), "SGTRACE\0", 8) != 0) {
    why = "no SGTRACE header";
    return false;
  }
  const uint32_t hdr = d[10] | d[11] << 8;
  const uint32_t meta = uint32_t(d[12]) | uint32_t(d[13]) << 8 | uint32_t(d[14]) << 16 | uint32_t(d[15]) << 24;
  size_t p = hdr + meta;
  while (p + 4 <= d.size()) {
    Rec r;
    r.type = d[p];
    r.sub = d[p + 1];
    r.len = d[p + 2] | d[p + 3] << 8;
    if (p + 4 + r.len > d.size()) {
      why = "truncated record";
      return false;
    }
    r.p = d.data() + p + 4;
    tf.recs.push_back(r);
    p += 4 + r.len;
    if (r.type == kEnd) break;
  }
  return true;
}

struct Mismatch {
  std::string trace;
  size_t rec;
  uint32_t msg;
  std::string what;
};

struct Totals {
  uint64_t traces = 0, msgs = 0, translated = 0, handlers = 0, handlers_total = 0, reads = 0, cursor_reads = 0,
           focus_cmds = 0, mismatches = 0, mousemove_cursor_eq_lp = 0, mousemove_cursor_ne_lp = 0;
  uint64_t by_msg[0x20c] = {};
};

int32_t sx16(uint32_t v) {
  v &= 0xffffu;
  return static_cast<int32_t>(v >= 0x8000u ? v - 0x10000u : v);
}

// Logical -> host client coordinates of a scaled window: ceil(logical * client / logical size) + origin, the smallest
// physical point that floors back to the logical one (Translator::to_logical).
Point physical(const Viewport &vp, Point l) {
  auto cd = [](int64_t a, int64_t b) {
    int64_t q = a / b;
    if (a % b != 0 && a > 0) ++q;
    return q;
  };
  Point p;
  p.x = static_cast<int32_t>(cd(static_cast<int64_t>(l.x) * vp.client_w, kLogicalW) + vp.origin_x);
  p.y = static_cast<int32_t>(cd(static_cast<int64_t>(l.y) * vp.client_h, kLogicalH) + vp.origin_y);
  return p;
}

// Answers a message's reads from the records of its window.
class TraceHost : public HostInput {
 public:
  const Viewport *vp = nullptr;  // set: the host window is scaled and the cursor answer is in its physical client pixels
  const std::vector<Rec> *recs = nullptr;
  size_t first = 0, last = 0;  // window [first, last)
  Point vcursor;               // the virtual cursor at the MSG
  bool has_handler = false;
  std::vector<bool> used;
  bool missing = false;
  std::string missing_what;

  void open(const std::vector<Rec> &r, size_t f, size_t l, Point vc) {
    recs = &r;
    first = f;
    last = l;
    vcursor = vc;
    used.assign(l - f, false);
    missing = false;
    missing_what.clear();
    has_handler = false;
    for (size_t i = f; i < l; ++i)
      if (r[i].type == kHandlerRec) has_handler = true;
  }
  uint16_t answer(uint8_t api_sub, uint32_t vk) {
    for (size_t i = first; i < last; ++i) {
      const Rec &r = (*recs)[i];
      if (!used[i - first] && r.type == kKeyState && r.sub == api_sub && r.u32(8) == vk) {
        used[i - first] = true;
        return static_cast<uint16_t>(r.u32(12) & 0xffffu);
      }
    }
    missing = true;
    missing_what = "the trace has no GetKeyState/GetAsyncKeyState record for vk " + std::to_string(vk);
    return 0;
  }
  uint16_t get_key_state(uint32_t vk) override { return answer(1, vk); }
  uint16_t get_async_key_state(uint32_t vk) override { return answer(2, vk); }
  uint32_t map_virtual_key(uint32_t code, uint32_t map_type) override {
    for (size_t i = first; i < last; ++i) {
      const Rec &r = (*recs)[i];
      if (!used[i - first] && r.type == kMapVk && r.u32(8) == code && r.u32(12) == map_type) {
        used[i - first] = true;
        return r.u32(16);
      }
    }
    missing = true;
    missing_what = "the trace has no MapVirtualKeyA record for code " + std::to_string(code);
    return 0;
  }
  Point cursor() override {
    Point c = vcursor;
    for (size_t i = first; i < last; ++i) {
      const Rec &r = (*recs)[i];
      if (r.type == kDeliver && r.sub == 1) {
        c.x = r.i32(4);
        c.y = r.i32(8);
      } else if (r.type == kCursor && r.sub == 1) {
        break;
      }
    }
    return vp ? physical(*vp, c) : c;
  }
  bool focus_child_takes(uint32_t, uint32_t) override { return !has_handler; }
  uint32_t now_ms() override { return 0; }
};

size_t expected_reads(uint32_t msg) {
  switch (msg) {
    case kWmKeyDown: case kWmSysKeyDown: return 4;
    case kWmKeyUp: case kWmSysKeyUp: return 3;
    case kWmMouseMove: return 1;
    case kWmLButtonUp: case kWmRButtonUp: return 2;
    default: return 0;
  }
}

struct Checker {
  Translator tr;
  Totals tot;
  std::vector<Mismatch> bad;
  size_t list_limit = 20;

  Viewport vp;           // the host window: identity unless --viewport
  bool scaled = false;

  Checker(Defect d, const Viewport &v, bool scale) : tr(v, d), vp(v), scaled(scale) {}

  void fail(const std::string &trace, size_t rec, uint32_t msg, const std::string &what) {
    ++tot.mismatches;
    if (bad.size() < list_limit) bad.push_back({trace, rec, msg, what});
  }

  void run_trace(const TraceFile &tf) {
    ++tot.traces;
    const auto &R = tf.recs;
    tr.reset();
    Point vcur;
    for (const Rec &r : R)
      if (r.type == kCursor && r.sub == 1) {
        vcur = {r.i32(4), r.i32(8)};
        break;
      }
    TraceHost host;
    std::vector<bool> covered(R.size(), false);  // HANDLER records inside a translated message's window
    for (size_t i = 0; i < R.size(); ++i) {
      const Rec &m = R[i];
      if (m.type == kDeliver && m.sub == 1) vcur = {m.i32(4), m.i32(8)};
      if (m.type == kHandlerRec) ++tot.handlers_total;
      if (m.type != kMsg || m.len < 16) continue;
      const uint32_t msg = m.u32(4), wp = m.u32(8);
      uint32_t lp = m.u32(12);
      const uint32_t lp_logical = lp;
      const bool mouse = msg >= kWmMouseMove && msg <= kWmRButtonDblClk;
      if (scaled && mouse) {  // the host window is scaled: the message carries physical client pixels
        const Point ph = physical(vp, {sx16(lp), sx16(lp >> 16)});
        lp = (static_cast<uint32_t>(ph.y) & 0xffffu) << 16 | (static_cast<uint32_t>(ph.x) & 0xffffu);
      }
      ++tot.msgs;
      if (msg < 0x20c) ++tot.by_msg[msg];
      size_t end = i + 1;
      while (end < R.size() && R[end].type != kMsg && R[end].type != kFrame && R[end].type != kPoll &&
             R[end].type != kEnd && R[end].type != kClockRun)
        ++end;
      host.open(R, i + 1, end, vcur);
      host.vp = scaled ? &vp : nullptr;
      Translation t = tr.translate(msg, wp, lp, host);
      if (t.evidence == Evidence::NotInput) {
        for (size_t k = i + 1; k < end; ++k)
          if (R[k].type == kHandlerRec) fail(tf.name, i, msg, "a handler follows a message the translator drops");
        continue;
      }
      ++tot.translated;
      if (host.missing) fail(tf.name, i, msg, host.missing_what);
      check_window(tf.name, R, i, end, msg, wp, lp_logical, t, covered);
    }
    for (size_t i = 0; i < R.size(); ++i)
      if (R[i].type == kHandlerRec && !covered[i]) fail(tf.name, i, 0, "a HANDLER record outside every translated message's window");
  }

  void check_window(const std::string &name, const std::vector<Rec> &R, size_t mi, size_t end, uint32_t msg, uint32_t wp,
                    uint32_t lp, const Translation &t, std::vector<bool> &covered) {
    size_t w = mi + 1;
    size_t reads = 0;
    bool synthetic = false;
    for (const Step &s : t.steps) {
      if (s.kind == Step::Kind::Call && (s.handler == kFocusKeyDown || s.handler == kFocusChar)) {
        synthetic = true;
        ++tot.focus_cmds;
        const uint32_t want_msg = s.handler == kFocusKeyDown ? kWmKeyDown : kWmChar;
        if (msg != want_msg && !(msg == kWmSysKeyDown && want_msg == kWmKeyDown))
          fail(name, mi, msg, "focus command of the wrong message class");
        if (static_cast<uint32_t>(s.a0) != wp) fail(name, mi, msg, "focus command a0 != wParam");
        continue;
      }
      if (s.kind == Step::Kind::Call) {
        size_t k = w;
        while (k < end && R[k].type != kHandlerRec) ++k;
        if (k >= end) {
          fail(name, mi, msg, "translator calls handler " + std::to_string(s.handler) + " but no HANDLER record is left");
          continue;
        }
        covered[k] = true;
        ++tot.handlers;
        const Rec &h = R[k];
        if (h.sub != s.handler || h.i32(4) != s.a0 || h.i32(8) != s.a1)
          fail(name, mi, msg,
               "HANDLER id " + std::to_string(h.sub) + " (" + std::to_string(h.i32(4)) + ", " + std::to_string(h.i32(8)) +
                   ") expected, translator gave id " + std::to_string(s.handler) + " (" + std::to_string(s.a0) + ", " +
                   std::to_string(s.a1) + ")");
        if (s.handler == kOnPress1 || s.handler == kOnPress2) {
          uint32_t ec = UINT32_MAX, es = UINT32_MAX;
          for (size_t q = k + 1; q < end && R[q].type != kHandlerRec; ++q)
            if (R[q].type == kKeyState && R[q].sub == 2) {
              if (R[q].u32(8) == 0x11 && ec == UINT32_MAX) ec = R[q].u32(12) & 0xffffu;
              if (R[q].u32(8) == 0x10 && es == UINT32_MAX) es = R[q].u32(12) & 0xffffu;
            }
          if (ec != s.async_ctrl || es != s.async_shift)
            fail(name, mi, msg, "click key state: trace ctrl/shift " + std::to_string(ec) + "/" + std::to_string(es) +
                                    ", translator " + std::to_string(s.async_ctrl) + "/" + std::to_string(s.async_shift));
        }
        w = k + 1;
        continue;
      }
      // a read
      ++reads;
      const uint8_t want_type = s.api == ReadApi::MapVirtualKey ? kMapVk : s.api == ReadApi::Cursor ? kCursor : kKeyState;
      size_t k = w;
      for (; k < end; ++k) {
        const Rec &r = R[k];
        if (r.type == kHandlerRec) {
          k = end;  // a handler passed before the read: order mismatch
          break;
        }
        if (r.type != want_type) continue;
        if (s.api == ReadApi::GetKeyState && !(r.sub == 1 && r.u32(8) == s.vk)) continue;
        if (s.api == ReadApi::GetAsyncKeyState && !(r.sub == 2 && r.u32(8) == s.vk)) continue;
        if (s.api == ReadApi::MapVirtualKey && !(r.u32(8) == s.vk && r.u32(12) == s.map_type)) continue;
        if (s.api == ReadApi::Cursor && r.sub != 1) continue;
        break;
      }
      if (k >= end) {
        fail(name, mi, msg, "translator reads api " + std::to_string(int(s.api)) + " vk " + std::to_string(s.vk) +
                                " but the trace has no such record at this point");
        continue;
      }
      const Rec &r = R[k];
      if (s.api == ReadApi::Cursor) {
        ++tot.cursor_reads;
        if (r.i32(4) != s.pt.x || r.i32(8) != s.pt.y)
          fail(name, mi, msg, "cursor answer (" + std::to_string(s.pt.x) + ", " + std::to_string(s.pt.y) + ") != recorded (" +
                                  std::to_string(r.i32(4)) + ", " + std::to_string(r.i32(8)) + ")");
        if (msg == kWmMouseMove) {
          if (r.i32(4) == sx16(lp) && r.i32(8) == sx16(lp >> 16)) ++tot.mousemove_cursor_eq_lp;
          else ++tot.mousemove_cursor_ne_lp;
        }
      } else {
        ++tot.reads;
        const uint32_t got = s.api == ReadApi::MapVirtualKey ? r.u32(16) : (r.u32(12) & 0xffffu);
        if (got != s.result)
          fail(name, mi, msg, "read answer " + std::to_string(s.result) + " != recorded " + std::to_string(got));
      }
      w = k + 1;
    }
    for (size_t k = w; k < end; ++k)
      if (R[k].type == kHandlerRec) {
        covered[k] = true;
        fail(name, mi, msg, "HANDLER record id " + std::to_string(R[k].sub) + " the translator did not produce");
      }
    if (reads != expected_reads(msg)) fail(name, mi, msg, "translator made " + std::to_string(reads) + " reads, the class takes " + std::to_string(expected_reads(msg)));
    bool has_h = false;
    for (size_t k = mi + 1; k < end; ++k)
      if (R[k].type == kHandlerRec) has_h = true;
    const bool keymsg = msg == kWmKeyDown || msg == kWmChar || msg == kWmSysKeyDown;
    if (keymsg && !has_h && !synthetic) fail(name, mi, msg, "no handler followed and the translator gave no focus command");
    if (synthetic && has_h) fail(name, mi, msg, "focus command although a root handler followed");
  }
};

int selftest() {
  int n = 0, bad = 0;
  auto check = [&](bool ok, const char *what) {
    ++n;
    if (!ok) {
      ++bad;
      std::printf("selftest FAIL: %s\n", what);
    }
  };
  struct H : HostInput {
    uint16_t ks[256] = {}, as[256] = {};
    uint32_t mv[256] = {};
    Point cur;
    bool focus = false;
    uint16_t get_key_state(uint32_t vk) override { return ks[vk & 255]; }
    uint16_t get_async_key_state(uint32_t vk) override { return as[vk & 255]; }
    uint32_t map_virtual_key(uint32_t c, uint32_t) override { return mv[c & 255]; }
    Point cursor() override { return cur; }
    bool focus_child_takes(uint32_t, uint32_t) override { return focus; }
    uint32_t now_ms() override { return 77; }
  } h;
  Translator t;
  const uint32_t lp = (50u << 16) | 100u;
  auto a = t.translate(kWmLButtonDown, 1, lp, h);
  check(a.steps.size() == 1 && a.steps[0].handler == kOnPoint && a.steps[0].a0 == 100 && a.steps[0].a1 == 50, "lbuttondown -> onPoint(x, y)");
  auto d = t.translate(kWmLButtonDblClk, 1, lp, h);
  // roadmap S7.4's cross-check: the original calls rootWin_onPress1b (handler 10) and then onPoint for WM_LBUTTONDBLCLK
  check(d.evidence == Evidence::Unmeasured && d.steps.size() == 2 && d.steps[0].handler == 10 && d.steps[0].a0 == 100 &&
            d.steps[1].handler == kOnPoint, "dblclk -> onPress1b + onPoint, flagged unmeasured (not in the corpus)");
  h.as[0x11] = 0x8001;
  auto u = t.translate(kWmLButtonUp, 0, lp, h);
  check(u.steps.size() == 4 && u.steps[0].handler == kOnPress1 && u.steps[0].async_ctrl == 0x8001 && u.steps[3].handler == kOnUnpoint,
        "lbuttonup -> onPress1 + reads + onUnpoint");
  auto r = t.translate(kWmRButtonUp, 0, lp, h);
  check(r.steps[0].handler == kOnPress2 && r.steps[3].handler == kOnUnpoint2, "rbuttonup -> onPress2 / onUnpoint2");
  h.mv[65] = 65;
  auto k = t.translate(kWmKeyDown, 65, 1, h);
  check(k.steps.size() == 5 && k.steps[4].handler == kOnChar && k.steps[4].a0 == 65, "keydown with a character -> onChar(vk)");
  auto c = t.translate(kWmChar, 'a', 1, h);
  check(c.steps.size() == 1 && c.steps[0].handler == kOnKey && c.steps[0].a0 == 'a' && c.steps[0].a1 == 65, "char -> onKey(ch, last vk)");
  auto k2 = t.translate(kWmKeyDown, 37, 1, h);
  check(k2.steps[4].handler == kOnKey && k2.steps[4].a0 == 0 && k2.steps[4].a1 == 37, "keydown without a character -> onKey(0, vk)");
  h.focus = true;
  auto f1 = t.translate(kWmKeyDown, 65, 1, h);
  auto f2 = t.translate(kWmChar, 'a', 1, h);
  check(f1.steps.back().handler == kFocusKeyDown && f2.steps.size() == 1 && f2.steps[0].handler == kFocusChar, "focus child takes keys: 12 / 13");
  h.focus = false;
  auto ku = t.translate(kWmKeyUp, 65, 1, h);
  check(ku.steps.size() == 4 && ku.steps[3].handler == kOnChar0, "keyup -> onChar0");
  auto mw = t.translate(0x20a, 0, lp, h);
  check(mw.evidence == Evidence::Unmeasured && mw.steps.empty(), "wheel: dropped, flagged unmeasured");
  auto tm = t.translate(0x113, 0, 0, h);
  check(tm.evidence == Evidence::NotInput && tm.steps.empty(), "WM_TIMER is not input");
  h.cur = {250, 120};
  auto mm = t.translate(kWmMouseMove, 0, lp, h);
  check(mm.steps.size() == 1 && mm.steps[0].api == ReadApi::Cursor && mm.steps[0].pt.x == 250 && mm.steps[0].pt.y == 120, "mousemove reads the host cursor");
  // viewport: a 1600x1200 client area with a 8,31 origin
  Viewport vp;
  vp.origin_x = 8;
  vp.origin_y = 31;
  vp.client_w = 1600;
  vp.client_h = 1200;
  Translator tv(vp);
  Point p = tv.to_logical({8 + 200, 31 + 100});
  check(p.x == 100 && p.y == 50, "viewport scale 2");
  p = tv.to_logical({7, 30});
  check(p.x == -1 && p.y == -1, "viewport floors below the origin");
  const uint32_t neg = (static_cast<uint32_t>(0xfffe) << 16) | 0xfffbu;  // (-5, -2) as signed words
  Translator tn;
  Point q = tn.lparam_point(neg);
  check(q.x == -5 && q.y == -2, "lParam words are signed");
  std::printf("selftest: %d checks, %d failed\n", n, bad);
  return bad ? 1 : 0;
}

std::string json_escape(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

}  // namespace

int main(int argc, char **argv) {
  std::vector<std::string> files;
  std::string json;
  int negctl = 0;
  size_t list = 20;
  bool do_selftest = false;
  Viewport vp;
  bool scaled = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--negctl" && i + 1 < argc) negctl = std::atoi(argv[++i]);
    else if (a == "--json" && i + 1 < argc) json = argv[++i];
    else if (a == "--list" && i + 1 < argc) list = static_cast<size_t>(std::atoi(argv[++i]));
    else if (a == "--selftest") do_selftest = true;
    else if (a == "--viewport" && i + 1 < argc) {  // client_w,client_h,origin_x,origin_y of a scaled host window
      long v[4] = {0, 0, 0, 0};
      char *e = argv[++i];
      int got = 0;
      while (got < 4 && *e) {
        char *next = nullptr;
        v[got++] = std::strtol(e, &next, 10);
        if (next == e) break;
        e = *next == ',' ? next + 1 : next;
      }
      const int cw = static_cast<int>(v[0]), chh = static_cast<int>(v[1]), ox = static_cast<int>(v[2]), oy = static_cast<int>(v[3]);
      if (got != 4 || cw < kLogicalW || chh < kLogicalH) {
        std::fprintf(stderr, "--viewport cw,ch,ox,oy (client at least 800x600: a smaller window loses points)\n");
        return 2;
      }
      vp.client_w = cw;
      vp.client_h = chh;
      vp.origin_x = ox;
      vp.origin_y = oy;
      scaled = true;
    }
    else files.push_back(a);
  }
  int rc = 0;
  if (do_selftest) rc = selftest();
  if (files.empty()) return rc;
  Checker ck(static_cast<Defect>(negctl), vp, scaled);
  ck.list_limit = list;
  for (const std::string &f : files) {
    TraceFile tf;
    std::string why;
    const size_t slash = f.find_last_of("/\\");
    tf.name = slash == std::string::npos ? f : f.substr(slash + 1);
    if (!load(f, tf, why)) {
      std::printf("%s: %s\n", tf.name.c_str(), why.c_str());
      ++ck.tot.mismatches;
      continue;
    }
    const uint64_t before = ck.tot.mismatches;
    ck.run_trace(tf);
    std::printf("%s: %llu mismatches\n", tf.name.c_str(), static_cast<unsigned long long>(ck.tot.mismatches - before));
  }
  const Totals &t = ck.tot;
  for (const Mismatch &m : ck.bad)
    std::printf("MISMATCH %s rec %zu msg 0x%x: %s\n", m.trace.c_str(), m.rec, m.msg, m.what.c_str());
  std::printf("TOTAL negctl=%d traces=%llu msgs=%llu translated=%llu handlers=%llu handlers_in_traces=%llu reads=%llu "
              "cursor_reads=%llu focus_cmds=%llu mousemove_cursor_eq_lp=%llu mousemove_cursor_ne_lp=%llu mismatches=%llu\n",
              negctl, (unsigned long long)t.traces, (unsigned long long)t.msgs, (unsigned long long)t.translated,
              (unsigned long long)t.handlers, (unsigned long long)t.handlers_total, (unsigned long long)t.reads,
              (unsigned long long)t.cursor_reads, (unsigned long long)t.focus_cmds,
              (unsigned long long)t.mousemove_cursor_eq_lp, (unsigned long long)t.mousemove_cursor_ne_lp,
              (unsigned long long)t.mismatches);
  if (!json.empty()) {
    std::ofstream o(json);
    o << "{\"negctl\":" << negctl << ",\"traces\":" << t.traces << ",\"msgs\":" << t.msgs << ",\"translated\":" << t.translated
      << ",\"handlers\":" << t.handlers << ",\"handlers_in_traces\":" << t.handlers_total << ",\"reads\":" << t.reads
      << ",\"cursor_reads\":" << t.cursor_reads << ",\"focus_cmds\":" << t.focus_cmds
      << ",\"mousemove_cursor_eq_lp\":" << t.mousemove_cursor_eq_lp << ",\"mousemove_cursor_ne_lp\":" << t.mousemove_cursor_ne_lp
      << ",\"mismatches\":" << t.mismatches << ",\"by_msg\":{";
    bool first = true;
    for (uint32_t m = 0; m < 0x20c; ++m)
      if (t.by_msg[m]) {
        o << (first ? "" : ",") << "\"" << m << "\":" << t.by_msg[m];
        first = false;
      }
    o << "},\"listed\":[";
    for (size_t i = 0; i < ck.bad.size(); ++i)
      o << (i ? "," : "") << "{\"trace\":\"" << json_escape(ck.bad[i].trace) << "\",\"rec\":" << ck.bad[i].rec
        << ",\"msg\":" << ck.bad[i].msg << ",\"what\":\"" << json_escape(ck.bad[i].what) << "\"}";
    o << "]}\n";
  }
  return rc;
}
