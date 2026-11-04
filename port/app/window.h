// The app's window (roadmap S7.3): an 800x600 content area scaled for the monitor's DPI, the port's back buffer presented in
// it (present.h), and the real input it receives translated into root-handler commands (input_map.h, S7.2).
//
// DPI: the process is per-monitor DPI aware (v2), so Windows neither stretches the window nor rescales its mouse
// coordinates; the window sizes its client area itself as MulDiv(800, dpi, 96) x MulDiv(600, dpi, 96) and follows
// WM_DPICHANGED (or keeps a fixed dpi given on the command line). The client's 4:3 content rectangle is the translator's
// Viewport, so a client point maps to the logical point the presenter shows there.
//
// Input: every key and mouse message goes, in dispatch order, through one Translator with a live HostInput (GetKeyState,
// GetAsyncKeyState, MapVirtualKeyA, GetCursorPos + ScreenToClient, timeGetTime). Each message yields an InputEvent: the
// message, the host's answers in the order the translator asked for them, the steps, and the root-handler commands
// (portsim::Command, kind RootHandler) copied from the steps' calls. The sink (S7.6: the live core) receives them; the shell
// writes them to a log (input.jsonl) that `--translate` re-checks offline.
#ifndef PORTAPP_WINDOW_H
#define PORTAPP_WINDOW_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "input_map.h"
#include "present.h"
#include "portsim/sim/commands.h"

namespace portapp {

// One answer the host gave the translator, in the order it was asked.
struct HostAnswer {
  enum class Api : uint8_t { KeyState = 1, AsyncKeyState = 2, MapVk = 3, Cursor = 4, Focus = 5, Now = 6 } api = Api::KeyState;
  uint32_t a = 0, b = 0;   // KeyState / AsyncKeyState: vk; MapVk: code, type; Focus: msg, wp
  uint32_t result = 0;     // the answer (Focus: 0 / 1; Now: ms)
  int32_t x = 0, y = 0;    // Cursor: client coordinates
};

struct InputEvent {
  uint32_t seq = 0;
  uint32_t msg = 0, wp = 0, lp = 0;
  Viewport vp;
  Evidence evidence = Evidence::NotInput;
  std::vector<HostAnswer> answers;
  std::vector<Step> steps;
  std::vector<portsim::Command> commands;  // the steps' calls, as RootHandler commands
  bool vacated = false;  // P9E fix 3: a mouse button message whose point is a moved HUD panel's vacated place (PanelRemap::map)
};

// Step (a Call) -> portsim::Command (RootHandler): handler, a0, a1, async_ctrl, async_shift, t (S7.2 notes section 4).
portsim::Command to_command(const Step &s);

// The answers recorded live, replayed in order (the offline check). Asking for something other than the next recorded
// answer is a mismatch: `ok` turns false and the call answers 0.
class ScriptedHost : public HostInput {
 public:
  explicit ScriptedHost(const std::vector<HostAnswer> &a) : a_(a) {}
  uint16_t get_key_state(uint32_t vk) override;
  uint16_t get_async_key_state(uint32_t vk) override;
  uint32_t map_virtual_key(uint32_t code, uint32_t map_type) override;
  Point cursor() override;
  bool focus_child_takes(uint32_t msg, uint32_t wp) override;
  uint32_t now_ms() override;
  bool ok() const { return ok_ && k_ == a_.size(); }

 private:
  const HostAnswer *next(HostAnswer::Api api, uint32_t a, uint32_t b);
  const std::vector<HostAnswer> &a_;
  size_t k_ = 0;
  bool ok_ = true;
};

// Deliberately wrong input path for the negative control (`--negctl input-bypass`): the window builds its commands from
// the raw message (client coordinates, no reads) instead of the translator.
enum class InputDefect : uint8_t { None = 0, Bypass = 1 };

struct WindowOptions {
  // G32: the HUD compositor's point remap (present.h PanelRemap): applied to every mouse message's point and cursor read before
  // the translator; null = none
  const class PanelRemap *remap = nullptr;
  std::string title = "SimGolf (port)";
  uint32_t dpi = 0;           // 0 = the monitor's (GetDpiForWindow), else fixed
  bool activate = true;       // false: shown without taking the focus (test runs)
  InputDefect input_defect = InputDefect::None;
  // the focused child's key test (U-1616: the port's window tree answers it in S7.6); default: never
  std::function<bool(uint32_t msg, uint32_t wp)> focus_child_takes;
  // roadmap S7.4: called with every input message before the translator reads anything for it, and the key state the
  // translator's GetKeyState / GetAsyncKeyState reads get (a result < 0: ask Windows); both default to nothing
  std::function<void(uint32_t msg, uint32_t wp)> before_translate;
  std::function<int32_t(uint32_t vk, bool async)> key_state;
  // P5B (log/roadmap/P5/P5B_notes.md; defaults are the S7 behaviour here, main.cpp turns the new ones on):
  // item 4: a sizing border and the maximise box; the client follows the user's size (the 4:3 content fitted inside it)
  bool resizable = false;
  // item 3: the close button / Alt+F4 (WM_SYSCOMMAND SC_CLOSE) calls this instead of closing; a WM_CLOSE sent to the window
  // (--exit-after, the game's quit, a test driver) still closes it
  std::function<void()> on_close_button;
  // item 2: + and - as the game's zoom keys: 0 off, 1 the numeric keypad's (VK_ADD / VK_SUBTRACT), 2 those and the main
  // keyboard's (VK_OEM_PLUS / VK_OEM_MINUS). A key-down of one becomes the 'Z' (zoom in) / 'X' (zoom out) key-down and its
  // WM_CHAR the character 'z' / 'x', the keys mainLoop H1 zooms with (key 0x7a ML+0xd36d, key 0x78 ML+0xd327), but only while
  // zoom_context() says the game is in its frame body (never in a text box, popup or menu)
  uint8_t zoom_keys = 0;
  std::function<bool()> zoom_context;
  // P7 (widescreen stage 1): the logical width the client opens at (its height stays 600 logical pixels; both scaled by the
  // dpi). kLogicalW = the 800x600 window; `--wide` gives 1067 (16:9). The 4:3 content stays fitted (full height, centred).
  int32_t logical_w = kLogicalW;
};

class AppWindow {
 public:
  AppWindow(Presenter &presenter, WindowOptions opt);
  ~AppWindow();
  AppWindow(const AppWindow &) = delete;
  AppWindow &operator=(const AppWindow &) = delete;

  bool create();               // registers the class and creates the window (false: see error())
  const std::string &error() const { return error_; }
  void *hwnd() const { return hwnd_; }
  int32_t client_w() const { return cw_; }
  int32_t client_h() const { return ch_; }
  uint32_t dpi() const { return dpi_; }
  Viewport viewport() const { return fit_viewport(cw_, ch_); }

  void set_sink(std::function<void(const InputEvent &)> sink) { sink_ = std::move(sink); }
  void invalidate();           // repaint from the presenter's current frame
  uint32_t paints() const { return paints_; }

  // Runs the message loop until the window is destroyed; `exit_after_ms` > 0 closes it after that long.
  int run(uint32_t exit_after_ms = 0);

  // window procedure entry (static trampoline in window.cpp)
  intptr_t on_message(uint32_t msg, uintptr_t wp, intptr_t lp);

 private:
  void size_for_dpi(uint32_t dpi, const void *suggested_rect);
  void input(uint32_t msg, uint32_t wp, uint32_t lp);
  void client_changed();       // P5B item 4: re-read the client size, refit the viewport
  uint32_t zoom_char_ = 0;     // P5B item 2: the character the next WM_CHAR becomes (0: none)
  uint32_t zoom_remaps_ = 0;

 public:
  uint32_t zoom_remaps() const { return zoom_remaps_; }

 private:
  Presenter &presenter_;
  WindowOptions opt_;
  void *hwnd_ = nullptr;
  std::string error_;
  int32_t cw_ = kLogicalW, ch_ = kLogicalH;
  uint32_t dpi_ = 96;
  uint32_t paints_ = 0;
  uint32_t seq_ = 0;
  Translator tr_;
  std::function<void(const InputEvent &)> sink_;
};

// JSON line of an event (input.jsonl) and its parser (the offline `--translate` check).
std::string event_json(const InputEvent &e);
bool parse_event_json(const std::string &line, InputEvent &e);
std::string steps_json(const std::vector<Step> &steps);

}  // namespace portapp

#endif
