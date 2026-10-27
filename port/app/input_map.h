// Input translation for the Windows host (roadmap S7.2, notes log/roadmap/S7/2_notes.md).
//
// The port takes input as root-window handler calls (portsim::Command, kind RootHandler: handler ids 1..11 and the focus
// child's keys 12 / 13, port/include/portsim/sim/commands.h). The original produced them in jgld's window procedure and the
// executable's listener, one window message at a time. This translator is that step for a host that has real Windows
// messages: it takes one message (msg, wParam, lParam) plus a HostInput that answers the reads the original makes while it
// handles the message (GetKeyState, GetAsyncKeyState, MapVirtualKeyA, the cursor, the focus ring) and returns the ordered
// STEPS the original took: the handler calls and the reads, exactly as the trace records them (re/model/trace_schema.md:
// MSG, HANDLER, KEYSTATE, MAPVK, CURSOR). It needs no window and no Win32 header: messages are data.
//
// The mapping is the one the 64 corpus traces measure (notes section 2); a message class the corpus does not contain is
// reported as Evidence::Unmeasured so the host knows it is translated by analogy or dropped.
//
// A step's fields map one to one onto portsim::Command (RootHandler): handler -> handler, a0 / a1, async_ctrl / async_shift,
// t. The host (S7.3) copies them; this library does not link portsim.
#ifndef PORTAPP_INPUT_MAP_H
#define PORTAPP_INPUT_MAP_H

#include <cstdint>
#include <vector>

namespace portapp {

constexpr int32_t kLogicalW = 800;  // the original's fixed 800x600 window (re/analysis/PORTABLE_RUNTIME.md)
constexpr int32_t kLogicalH = 600;

// Root-handler ids as the trace's HANDLER records number them (trace_schema.md).
enum Handler : uint8_t {
  kOnPress1 = 1,   // left button released: the click (x, y), Ctrl / Shift read inside
  kOnPoint = 2,    // left button pressed (x, y)
  kOnUnpoint = 3,  // after kOnPress1
  kOnPress2 = 4,   // right button released: the click (x, y)
  kOnPoint2 = 5,   // right button pressed (x, y)
  kOnUnpoint2 = 6, // after kOnPress2
  kOnKey = 7,      // (a, code): a WM_CHAR character with the last key-down's code, or (0, vk) for a key with no character
  kOnChar = 8,     // a key-down with a character: (vk)
  kOnChar0 = 9,    // a key-up
  kFocusKeyDown = 12,  // a WM_KEYDOWN the focused child window took before the root (a0 = vk); apply_command ignores it
  kFocusChar = 13,     // a WM_CHAR the focused child took (a0 = the character)
};

// The window messages this library knows (the input classes the trace records, schema MSG).
enum Msg : uint32_t {
  kWmKeyDown = 0x100, kWmKeyUp = 0x101, kWmChar = 0x102,
  kWmSysKeyDown = 0x104, kWmSysKeyUp = 0x105,
  kWmMouseMove = 0x200, kWmLButtonDown = 0x201, kWmLButtonUp = 0x202, kWmLButtonDblClk = 0x203,
  kWmRButtonDown = 0x204, kWmRButtonUp = 0x205, kWmRButtonDblClk = 0x206,
};

// Whether the corpus traces contain the message (every translated record is judged against them) or the translation
// rests on the analysis of jgld's window procedure alone.
enum class Evidence : uint8_t {
  Corpus,      // in the corpus: judged on every record
  Unmeasured,  // translated by analogy or dropped without a recording (notes section 5, U-1610..)
  NotInput,    // a message class the game never reads as input (no steps)
};

struct Point {
  int32_t x = 0, y = 0;
};

// Maps the host window's client area onto the original's 800x600: logical = floor((client - origin) * 800 / client_w).
// Identity for a client area of exactly 800x600 with origin 0. Out-of-range points pass through (the original receives
// signed 16-bit words and does not clamp them).
struct Viewport {
  int32_t origin_x = 0, origin_y = 0;
  int32_t client_w = kLogicalW, client_h = kLogicalH;
};

// What the host answers for the reads the original makes while it handles a message. The live host implements these with
// the Win32 calls; the checker answers them from the recorded trace.
class HostInput {
 public:
  virtual ~HostInput() = default;
  virtual uint16_t get_key_state(uint32_t vk) = 0;        // GetKeyState (low 16 bits)
  virtual uint16_t get_async_key_state(uint32_t vk) = 0;  // GetAsyncKeyState (low 16 bits)
  virtual uint32_t map_virtual_key(uint32_t code, uint32_t map_type) = 0;  // MapVirtualKeyA
  virtual Point cursor() = 0;  // GetCursorPos + ScreenToClient: the cursor in the host window's client coordinates
  // Whether a focused child window of the game's window tree takes this key message before the root's handlers (the
  // character editor's bio entry, notes section 4). msg is kWmKeyDown or kWmChar, wp its wParam.
  virtual bool focus_child_takes(uint32_t msg, uint32_t wp) = 0;
  virtual uint32_t now_ms() = 0;  // timeGetTime, for Command::t (read only while the deadline word is set)
};

enum class ReadApi : uint8_t { GetKeyState = 1, GetAsyncKeyState = 2, MapVirtualKey = 3, Cursor = 4 };

struct Step {
  enum class Kind : uint8_t { Call, Read } kind = Kind::Call;
  // Call: one root-handler call (Command::RootHandler fields)
  uint8_t handler = 0;
  int32_t a0 = 0, a1 = 0;
  uint32_t async_ctrl = 0, async_shift = 0;  // handlers 1 / 4: GetAsyncKeyState(VK_CONTROL) / (VK_SHIFT), low 16 bits
  uint32_t t = 0;                            // handlers 2 / 3 / 7: timeGetTime at the handler
  // Read: a read the original made while handling the message, with the answer it got
  ReadApi api = ReadApi::GetKeyState;
  uint32_t vk = 0;        // GetKeyState / GetAsyncKeyState: the key; MapVirtualKey: the code
  uint32_t map_type = 0;  // MapVirtualKey
  uint32_t result = 0;    // the answer (GetKeyState / GetAsyncKeyState / MapVirtualKey)
  Point pt;               // Cursor: the answer, logical coordinates
};

struct Translation {
  Evidence evidence = Evidence::NotInput;
  std::vector<Step> steps;  // in the order the original took them
};

// Deliberately wrong bodies for the negative controls (checker `--negctl N`; production code uses None).
enum class Defect : uint8_t {
  None = 0,
  SwapButtons = 1,       // left and right buttons exchanged
  SwapKeyHandlers = 2,   // WM_CHAR takes onChar's shape and WM_KEYDOWN onKey's
  YFlip = 3,             // y measured from the bottom edge
  YFrameOffset = 4,      // y offset by the caption height (the window's non-client top)
  DropCtrl = 5,          // the Ctrl bit of the click's key state dropped
  IgnoreFocusTake = 6,   // the focused child never takes a key
  AlwaysOnChar = 7,      // a key-down is always onChar, whatever MapVirtualKeyA says
  StaleCode = 8,         // a WM_CHAR's code is its own character, not the last key-down's vk
  DropShift = 9,         // the Shift bit of the click's key state dropped
  NoViewport = 10,       // the host window's scale and origin ignored (client coordinates passed as logical)
};

class Translator {
 public:
  explicit Translator(Viewport vp = Viewport{}, Defect d = Defect::None) : vp_(vp), defect_(d) {}

  void set_viewport(const Viewport &vp) { vp_ = vp; }
  const Viewport &viewport() const { return vp_; }
  void reset() { last_vk_ = 0; }

  // Client (host window) coordinates to the original's logical ones.
  Point to_logical(Point client) const;
  // lParam of a mouse message: two signed 16-bit words, client coordinates (host window), to logical.
  Point lparam_point(uint32_t lp) const;

  // The classification of a message id.
  static Evidence evidence_of(uint32_t msg);

  // One message. A message that is not input yields no steps. The code of the last key-down is kept between calls (a
  // WM_CHAR's `code`), so messages of one window must go through one Translator in dispatch order.
  Translation translate(uint32_t msg, uint32_t wp, uint32_t lp, HostInput &host);

 private:
  Viewport vp_;
  Defect defect_ = Defect::None;
  uint32_t last_vk_ = 0;  // the virtual key of the most recent WM_KEYDOWN (the listener's key state; 0 before the first)
};

}  // namespace portapp

#endif
