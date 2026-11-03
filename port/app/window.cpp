// The app's window (window.h).
#include "window.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <cstdio>
#include <cstdlib>

namespace portapp {

portsim::Command to_command(const Step &s) {
  portsim::Command c;
  c.kind = portsim::CmdKind::RootHandler;
  c.handler = s.handler;
  c.a0 = s.a0;
  c.a1 = s.a1;
  c.async_ctrl = s.async_ctrl;
  c.async_shift = s.async_shift;
  c.t = s.t;
  return c;
}

// ------------------------------------------------------------------------------------------------ hosts
namespace {
// The live answers, each one recorded.
class LiveHost : public HostInput {
 public:
  LiveHost(HWND hwnd, const WindowOptions &opt, std::vector<HostAnswer> &log) : hwnd_(hwnd), opt_(opt), log_(log) {}
  uint16_t get_key_state(uint32_t vk) override {
    const int32_t k = opt_.key_state ? opt_.key_state(vk, false) : -1;  // S7.4: keys from messages
    const uint16_t r = k >= 0 ? static_cast<uint16_t>(k) : static_cast<uint16_t>(GetKeyState(static_cast<int>(vk)));
    put(HostAnswer::Api::KeyState, vk, 0, r);
    return r;
  }
  uint16_t get_async_key_state(uint32_t vk) override {
    const int32_t k = opt_.key_state ? opt_.key_state(vk, true) : -1;
    const uint16_t r = k >= 0 ? static_cast<uint16_t>(k) : static_cast<uint16_t>(GetAsyncKeyState(static_cast<int>(vk)));
    put(HostAnswer::Api::AsyncKeyState, vk, 0, r);
    return r;
  }
  uint32_t map_virtual_key(uint32_t code, uint32_t map_type) override {
    const uint32_t r = MapVirtualKeyA(code, map_type);
    put(HostAnswer::Api::MapVk, code, map_type, r);
    return r;
  }
  Point cursor() override {
    POINT p = {0, 0};
    GetCursorPos(&p);
    ScreenToClient(hwnd_, &p);
    if (opt_.remap) {  // G32
      int32_t x = p.x, y = p.y;
      opt_.remap->map(x, y);
      p.x = x;
      p.y = y;
    }
    HostAnswer a;
    a.api = HostAnswer::Api::Cursor;
    a.x = p.x;
    a.y = p.y;
    log_.push_back(a);
    return Point{p.x, p.y};
  }
  bool focus_child_takes(uint32_t msg, uint32_t wp) override {
    const bool r = opt_.focus_child_takes ? opt_.focus_child_takes(msg, wp) : false;
    put(HostAnswer::Api::Focus, msg, wp, r ? 1u : 0u);
    return r;
  }
  uint32_t now_ms() override {
    const uint32_t r = timeGetTime();
    put(HostAnswer::Api::Now, 0, 0, r);
    return r;
  }

 private:
  void put(HostAnswer::Api api, uint32_t a, uint32_t b, uint32_t r) {
    HostAnswer h;
    h.api = api;
    h.a = a;
    h.b = b;
    h.result = r;
    log_.push_back(h);
  }
  HWND hwnd_;
  const WindowOptions &opt_;
  std::vector<HostAnswer> &log_;
};
}  // namespace

const HostAnswer *ScriptedHost::next(HostAnswer::Api api, uint32_t a, uint32_t b) {
  if (k_ >= a_.size()) {
    ok_ = false;
    return nullptr;
  }
  const HostAnswer &h = a_[k_++];
  const bool args = api == HostAnswer::Api::Cursor || api == HostAnswer::Api::Now || (h.a == a && h.b == b);
  if (h.api != api || !args) {
    ok_ = false;
    return nullptr;
  }
  return &h;
}
uint16_t ScriptedHost::get_key_state(uint32_t vk) {
  const HostAnswer *h = next(HostAnswer::Api::KeyState, vk, 0);
  return h ? static_cast<uint16_t>(h->result) : 0;
}
uint16_t ScriptedHost::get_async_key_state(uint32_t vk) {
  const HostAnswer *h = next(HostAnswer::Api::AsyncKeyState, vk, 0);
  return h ? static_cast<uint16_t>(h->result) : 0;
}
uint32_t ScriptedHost::map_virtual_key(uint32_t code, uint32_t map_type) {
  const HostAnswer *h = next(HostAnswer::Api::MapVk, code, map_type);
  return h ? h->result : 0;
}
Point ScriptedHost::cursor() {
  const HostAnswer *h = next(HostAnswer::Api::Cursor, 0, 0);
  return h ? Point{h->x, h->y} : Point{};
}
bool ScriptedHost::focus_child_takes(uint32_t msg, uint32_t wp) {
  const HostAnswer *h = next(HostAnswer::Api::Focus, msg, wp);
  return h ? h->result != 0 : false;
}
uint32_t ScriptedHost::now_ms() {
  const HostAnswer *h = next(HostAnswer::Api::Now, 0, 0);
  return h ? h->result : 0;
}

// ------------------------------------------------------------------------------------------------ window
namespace {
const wchar_t kClass[] = L"SimGolfPortWindow";
constexpr DWORD kStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
constexpr DWORD kResizableStyle = kStyle | WS_THICKFRAME | WS_MAXIMIZEBOX;  // P5B item 4
constexpr DWORD kExStyle = 0;
constexpr int kMinClientW = 400, kMinClientH = 300;  // P5B item 4: half the logical size

// P5B item 2: the zoom-key remap (WindowOptions::zoom_keys): 'Z' / 'z' for +, 'X' / 'x' for -
bool zoom_key(uint32_t vk, uint8_t mode, bool &in) {
  if (mode == 0) return false;
  if (vk == VK_ADD || (mode >= 2 && vk == VK_OEM_PLUS)) {
    in = true;
    return true;
  }
  if (vk == VK_SUBTRACT || (mode >= 2 && vk == VK_OEM_MINUS)) {
    in = false;
    return true;
  }
  return false;
}

LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
  if (m == WM_NCCREATE) {
    const CREATESTRUCTW *cs = reinterpret_cast<const CREATESTRUCTW *>(lp);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  auto *w = reinterpret_cast<AppWindow *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (w && w->hwnd()) return static_cast<LRESULT>(w->on_message(m, wp, lp));
  return DefWindowProcW(h, m, wp, lp);
}

bool is_input(UINT m) {
  return (m >= WM_KEYFIRST && m <= WM_KEYLAST) || (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST);
}
}  // namespace

AppWindow::AppWindow(Presenter &presenter, WindowOptions opt) : presenter_(presenter), opt_(std::move(opt)) {}

AppWindow::~AppWindow() {
  if (hwnd_) DestroyWindow(static_cast<HWND>(hwnd_));
}

bool AppWindow::create() {
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = wndproc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.lpszClassName = kClass;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    error_ = "RegisterClassExW failed";
    return false;
  }
  std::wstring title(opt_.title.begin(), opt_.title.end());
  HWND h = CreateWindowExW(kExStyle, kClass, title.c_str(), opt_.resizable ? kResizableStyle : kStyle, CW_USEDEFAULT, CW_USEDEFAULT, kLogicalW, kLogicalH,
                           nullptr, nullptr, inst, this);
  if (!h) {
    error_ = "CreateWindowExW failed";
    return false;
  }
  hwnd_ = h;
  size_for_dpi(opt_.dpi ? opt_.dpi : GetDpiForWindow(h), nullptr);
  ShowWindow(h, opt_.activate ? SW_SHOW : SW_SHOWNOACTIVATE);
  UpdateWindow(h);
  return true;
}

void AppWindow::size_for_dpi(uint32_t dpi, const void *suggested) {
  HWND h = static_cast<HWND>(hwnd_);
  dpi_ = dpi ? dpi : 96;
  const int cw = MulDiv(opt_.logical_w, static_cast<int>(dpi_), 96), ch = MulDiv(kLogicalH, static_cast<int>(dpi_), 96);  // P7: logical_w
  // the frame for the window's own monitor dpi (the client size is the content's, chosen above)
  const UINT frame_dpi = GetDpiForWindow(h);
  RECT r = {0, 0, cw, ch};
  AdjustWindowRectExForDpi(&r, opt_.resizable ? kResizableStyle : kStyle, FALSE, kExStyle, frame_dpi);
  int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
  if (suggested) {
    const RECT *s = static_cast<const RECT *>(suggested);
    x = s->left;
    y = s->top;
  } else {
    RECT cur;
    GetWindowRect(h, &cur);
    x = cur.left;
    y = cur.top;
  }
  SetWindowPos(h, nullptr, x, y, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
  client_changed();
}

void AppWindow::client_changed() {
  RECT c;
  GetClientRect(static_cast<HWND>(hwnd_), &c);
  cw_ = c.right - c.left;
  ch_ = c.bottom - c.top;
  tr_.set_viewport(fit_viewport(cw_, ch_));
}

void AppWindow::invalidate() {
  if (hwnd_) InvalidateRect(static_cast<HWND>(hwnd_), nullptr, FALSE);
}

int AppWindow::run(uint32_t exit_after_ms) {
  if (exit_after_ms) SetTimer(static_cast<HWND>(hwnd_), 1, exit_after_ms, nullptr);
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);  // WM_CHAR after WM_KEYDOWN, as the original's message loop
    DispatchMessageW(&m);
  }
  return static_cast<int>(m.wParam);
}

void AppWindow::input(uint32_t msg, uint32_t wp, uint32_t lp) {
  InputEvent e;
  e.seq = ++seq_;
  e.msg = msg;
  e.wp = wp;
  e.lp = lp;
  e.vp = tr_.viewport();
  // P5B item 2: + / - become the game's own zoom keys while the frame body runs (event_json records the remapped message, so
  // --translate re-checks what the game received)
  if (opt_.zoom_keys) {
    bool in = false;
    if (msg == kWmKeyDown || msg == kWmKeyUp) {
      if (zoom_key(wp, opt_.zoom_keys, in) && (msg == kWmKeyUp || !opt_.zoom_context || opt_.zoom_context())) {
        wp = in ? 'Z' : 'X';
        if (msg == kWmKeyDown) {
          zoom_char_ = in ? 'z' : 'x';  // lower case: 'Z' / 'X' (0x5a / 0x58) are other H1 keys (0x58 rebuilds the tables)
          ++zoom_remaps_;
        }
      } else if (msg == kWmKeyDown) {
        zoom_char_ = 0;
      }
    } else if (msg == kWmChar && zoom_char_) {
      wp = zoom_char_;
      zoom_char_ = 0;
    }
    e.wp = wp;
  }
  if (opt_.before_translate) opt_.before_translate(msg, wp);
  if (opt_.remap && msg >= kWmMouseMove && msg <= 0x20a) {  // G32: a point over a moved HUD panel goes to its 800-space place
    int32_t x = static_cast<int16_t>(lp & 0xffff), y = static_cast<int16_t>(lp >> 16);
    opt_.remap->map(x, y);
    lp = (static_cast<uint32_t>(static_cast<uint16_t>(y)) << 16) | static_cast<uint16_t>(x);
  }
  if (opt_.input_defect == InputDefect::Bypass) {
    // the wrong path: client coordinates straight into a click, no reads
    e.evidence = Translator::evidence_of(msg);
    if (msg == kWmLButtonUp || msg == kWmLButtonDown) {
      Step s;
      s.handler = msg == kWmLButtonUp ? kOnPress1 : kOnPoint;
      s.a0 = static_cast<int16_t>(lp & 0xffff);
      s.a1 = static_cast<int16_t>(lp >> 16);
      e.steps.push_back(s);
    }
  } else {
    LiveHost host(static_cast<HWND>(hwnd_), opt_, e.answers);
    Translation t = tr_.translate(msg, wp, lp, host);
    e.evidence = t.evidence;
    e.steps = std::move(t.steps);
  }
  for (const Step &s : e.steps)
    if (s.kind == Step::Kind::Call) e.commands.push_back(to_command(s));
  if (sink_ && (e.evidence != Evidence::NotInput || !e.steps.empty())) sink_(e);
}

intptr_t AppWindow::on_message(uint32_t msg, uintptr_t wp, intptr_t lp) {
  HWND h = static_cast<HWND>(hwnd_);
  if (is_input(msg)) {
    input(msg, static_cast<uint32_t>(wp), static_cast<uint32_t>(lp));
    if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_SYSCHAR) return DefWindowProcW(h, msg, wp, lp);  // Alt+F4
    return 0;
  }
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      presenter_.paint(dc, cw_, ch_);
      EndPaint(h, &ps);
      ++paints_;
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_DPICHANGED:
      if (!opt_.dpi) {
        if (opt_.resizable) {  // P5B item 4: the user's size, as Windows rescaled it for the new monitor
          const RECT *s = reinterpret_cast<const RECT *>(lp);
          dpi_ = HIWORD(wp);
          SetWindowPos(h, nullptr, s->left, s->top, s->right - s->left, s->bottom - s->top, SWP_NOZORDER | SWP_NOACTIVATE);
          client_changed();
        } else {
          size_for_dpi(HIWORD(wp), reinterpret_cast<const RECT *>(lp));
        }
      }
      invalidate();
      return 0;
    case WM_SIZE:  // P5B item 4 (a fixed window gets one too, with the size it already has)
      if (wp != SIZE_MINIMIZED) {
        client_changed();
        invalidate();
      }
      return 0;
    case WM_GETMINMAXINFO:
      if (opt_.resizable) {
        MINMAXINFO *mi = reinterpret_cast<MINMAXINFO *>(lp);
        RECT r = {0, 0, MulDiv(kMinClientW, static_cast<int>(dpi_), 96), MulDiv(kMinClientH, static_cast<int>(dpi_), 96)};
        AdjustWindowRectExForDpi(&r, kResizableStyle, FALSE, kExStyle, GetDpiForWindow(h));
        mi->ptMinTrackSize.x = r.right - r.left;
        mi->ptMinTrackSize.y = r.bottom - r.top;
        return 0;
      }
      return DefWindowProcW(h, msg, wp, lp);
    case WM_SYSCOMMAND:
      if ((wp & 0xfff0) == SC_CLOSE && opt_.on_close_button) {  // P5B item 3: the close button / Alt+F4
        opt_.on_close_button();
        return 0;
      }
      return DefWindowProcW(h, msg, wp, lp);
    case WM_TIMER:
      if (wp == 1) {
        KillTimer(h, 1);
        PostMessageW(h, WM_CLOSE, 0, 0);
      }
      return 0;
    case WM_CLOSE:
      DestroyWindow(h);
      return 0;
    case WM_DESTROY:
      hwnd_ = nullptr;
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(h, msg, wp, lp);
  }
}

// ------------------------------------------------------------------------------------------------ JSON lines
namespace {
const char *ev_name(Evidence e) {
  return e == Evidence::Corpus ? "corpus" : e == Evidence::Unmeasured ? "unmeasured" : "notinput";
}
void app(std::string &s, const char *fmt, long long v) {
  char b[64];
  std::snprintf(b, sizeof b, fmt, v);
  s += b;
}
// the integers of the bracketed value after `key` (nested arrays flattened)
bool ints_after(const std::string &line, const char *key, std::vector<long long> &out) {
  out.clear();
  size_t p = line.find(key);
  if (p == std::string::npos) return false;
  p += std::char_traits<char>::length(key);
  int depth = 0;
  for (; p < line.size(); ++p) {
    const char c = line[p];
    if (c == '[') ++depth;
    else if (c == ']') {
      if (--depth <= 0) return true;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
      char *e = nullptr;
      out.push_back(std::strtoll(line.c_str() + p, &e, 10));
      p = static_cast<size_t>(e - line.c_str()) - 1;
    } else if (depth == 0 && c != ' ') {
      return false;
    }
  }
  return false;
}
bool int_after(const std::string &line, const char *key, long long &v) {
  size_t p = line.find(key);
  if (p == std::string::npos) return false;
  v = std::strtoll(line.c_str() + p + std::char_traits<char>::length(key), nullptr, 10);
  return true;
}
}  // namespace

std::string steps_json(const std::vector<Step> &steps) {
  std::string s = "[";
  for (size_t i = 0; i < steps.size(); ++i) {
    const Step &x = steps[i];
    if (i) s += ",";
    s += "[";
    app(s, "%lld", x.kind == Step::Kind::Call ? 0 : 1);
    for (long long v : {static_cast<long long>(x.handler), static_cast<long long>(x.a0), static_cast<long long>(x.a1),
                        static_cast<long long>(x.async_ctrl), static_cast<long long>(x.async_shift),
                        static_cast<long long>(x.t), static_cast<long long>(x.api), static_cast<long long>(x.vk),
                        static_cast<long long>(x.map_type), static_cast<long long>(x.result),
                        static_cast<long long>(x.pt.x), static_cast<long long>(x.pt.y)})
      app(s, ",%lld", v);
    s += "]";
  }
  return s + "]";
}

std::string event_json(const InputEvent &e) {
  std::string s = "{";
  app(s, "\"seq\":%lld", e.seq);
  app(s, ",\"msg\":%lld", e.msg);
  app(s, ",\"wp\":%lld", e.wp);
  app(s, ",\"lp\":%lld", e.lp);
  s += ",\"vp\":[";
  app(s, "%lld", e.vp.origin_x);
  app(s, ",%lld", e.vp.origin_y);
  app(s, ",%lld", e.vp.client_w);
  app(s, ",%lld", e.vp.client_h);
  s += "],\"evidence\":\"";
  s += ev_name(e.evidence);
  s += "\",\"ans\":[";
  for (size_t i = 0; i < e.answers.size(); ++i) {
    const HostAnswer &a = e.answers[i];
    if (i) s += ",";
    app(s, "[%lld", static_cast<long long>(a.api));
    app(s, ",%lld", a.a);
    app(s, ",%lld", a.b);
    app(s, ",%lld", a.result);
    app(s, ",%lld", a.x);
    app(s, ",%lld]", a.y);
  }
  s += "],\"steps\":" + steps_json(e.steps) + ",\"cmds\":[";
  for (size_t i = 0; i < e.commands.size(); ++i) {
    const portsim::Command &c = e.commands[i];
    if (i) s += ",";
    app(s, "[%lld", c.handler);
    app(s, ",%lld", c.a0);
    app(s, ",%lld", c.a1);
    app(s, ",%lld", c.async_ctrl);
    app(s, ",%lld", c.async_shift);
    app(s, ",%lld]", c.t);
  }
  return s + "]}";
}

bool parse_event_json(const std::string &line, InputEvent &e) {
  long long v = 0;
  if (!int_after(line, "\"seq\":", v)) return false;
  e.seq = static_cast<uint32_t>(v);
  if (!int_after(line, "\"msg\":", v)) return false;
  e.msg = static_cast<uint32_t>(v);
  if (!int_after(line, "\"wp\":", v)) return false;
  e.wp = static_cast<uint32_t>(v);
  if (!int_after(line, "\"lp\":", v)) return false;
  e.lp = static_cast<uint32_t>(v);
  std::vector<long long> n;
  if (!ints_after(line, "\"vp\":", n) || n.size() != 4) return false;
  e.vp.origin_x = static_cast<int32_t>(n[0]);
  e.vp.origin_y = static_cast<int32_t>(n[1]);
  e.vp.client_w = static_cast<int32_t>(n[2]);
  e.vp.client_h = static_cast<int32_t>(n[3]);
  if (!ints_after(line, "\"ans\":", n) || n.size() % 6) return false;
  e.answers.clear();
  for (size_t i = 0; i < n.size(); i += 6) {
    HostAnswer a;
    a.api = static_cast<HostAnswer::Api>(n[i]);
    a.a = static_cast<uint32_t>(n[i + 1]);
    a.b = static_cast<uint32_t>(n[i + 2]);
    a.result = static_cast<uint32_t>(n[i + 3]);
    a.x = static_cast<int32_t>(n[i + 4]);
    a.y = static_cast<int32_t>(n[i + 5]);
    e.answers.push_back(a);
  }
  if (!ints_after(line, "\"steps\":", n) || n.size() % 13) return false;
  e.steps.clear();
  for (size_t i = 0; i < n.size(); i += 13) {
    Step s;
    s.kind = n[i] ? Step::Kind::Read : Step::Kind::Call;
    s.handler = static_cast<uint8_t>(n[i + 1]);
    s.a0 = static_cast<int32_t>(n[i + 2]);
    s.a1 = static_cast<int32_t>(n[i + 3]);
    s.async_ctrl = static_cast<uint32_t>(n[i + 4]);
    s.async_shift = static_cast<uint32_t>(n[i + 5]);
    s.t = static_cast<uint32_t>(n[i + 6]);
    s.api = static_cast<ReadApi>(n[i + 7]);
    s.vk = static_cast<uint32_t>(n[i + 8]);
    s.map_type = static_cast<uint32_t>(n[i + 9]);
    s.result = static_cast<uint32_t>(n[i + 10]);
    s.pt.x = static_cast<int32_t>(n[i + 11]);
    s.pt.y = static_cast<int32_t>(n[i + 12]);
    e.steps.push_back(s);
  }
  return true;
}

}  // namespace portapp
