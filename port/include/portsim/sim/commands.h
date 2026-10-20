// Commands (phase 3.7b, 2026-10-19): the player's input as the simulation sees it, and the command effects of the 62
// UI-module straddlers (re/port/scope_straddlers.tsv, port_module `ui`). Notes: log/p3/p37b_notes.txt; module `commands`
// (port/src/sim/commands*.cpp), tests port/test/test_commands.py.
//
// THE INTERFACE WITH THE FRAME STEP (3.7a, tickFrame). The original consumes input in H1 of mainLoop's frame
// (re/analysis/sim/time_model.md section 7): pumpMessages -> frameTick483cd0 runs jgld's pumps, the
// window procedure calls the ROOT WINDOW HANDLERS (vtable slots 29..39), which store the click /
// key into the input globals (g_rootInput, gDirtyFlagA / B, g_char83821c ...); then H1's
// key switch (takeKey / takeCode) and H2's mouse action read those globals and call the straddlers (clickGolferPanel,
// clickBuildTool433d30, handleEmployeeAction ...). So a Command comes in two families:
//
//  1. RAW kinds (RootHandler): one root-handler call, exactly as the trace's HANDLER record (re/model/trace_schema.md, type
//     0x09) gives it: handler id 1..11 and its two arguments, plus the GetAsyncKeyState results the press handlers read
//     (trace KEYSTATE records, kind 2, ra inside the handler) and the clock value the deadline tail compares (the
//     timeGetTime the handler made; 0 = no deadline set is irrelevant then). apply_command performs exactly the
//     handler's writes. tickFrame calls apply_command for every raw command of the frame at H1's pump point, in delivery
//     order, then runs H1's key switch and H2 itself; H1 / H2 call the straddler effects BY THEIR hooks.csv NAMES through
//     the registry (module `commands` registers them), as the original calls the functions.
//  2. SEMANTIC kinds: one straddler's command effect invoked directly with its decision already made (the panel, row or
//     choice the click selected), for a headless driver that issues "hire walker 3" without coordinates. apply_command runs
//     the same effect body the registered function runs after its hit test. Arguments per kind below.
//
// MODAL BLOCKS. Several straddlers wait inside for input (pumpAndRedraw(0) and the screens' own loops):
// clickSlotPanel, clickBuildTool433d30, pickBuilding, clickAmenityPanel, the report screens, showBulletinBoard ... Every
// such loop calls pumpMessages, whose dispatch point (frameTick483cd0) delivers the messages that have arrived
// to the same root handlers. The port asks the context's InputSource for one BATCH (the commands one dispatch delivers) per
// pump call, saying what kind of pump it is:
//   PumpKind::Blocking  a loop that only ends on input (pumpAndRedraw(0), the screens' click-or-key loops): the source
//                       must return the next batch if it has any input left; none left -> NeedInput.
//   PumpKind::Drain     a pump that returns at once when nothing has arrived (pumpLight's drain after a screen, an
//                       interruptible waitTicks): the source returns a batch only if the original dispatched one there.
// The original decides that by wall time; the port has no clock, so the source carries the timing: a trace-driven source
// (3.7a / 3.8) knows from the trace which pump call dispatched each message; QueueInput (tests) uses the rule "a Blocking
// pump takes every command up to and including the first press or key, a Drain takes nothing" (U-0941). tickFrame applies
// the frame's own commands at H1 with apply_commands and attaches the rest of the run's input as the InputSource.
#ifndef PORTSIM_SIM_COMMANDS_H
#define PORTSIM_SIM_COMMANDS_H

#include <cstdint>
#include <utility>
#include <vector>

#include "portsim/registry.h"

namespace portsim {

enum class CmdKind : uint32_t {
  None = 0,

  // ---- raw: a root-window handler call (trace HANDLER id, args a0 / a1) ----
  RootHandler = 1,

  // ---- semantic: one straddler's ACTION with its hit test already decided (0x100..; values are stable, append only) ----
  // arg[0] is the value the straddler's own hit test would have returned (the panel control / slot / cell index); the
  // action then does exactly what the registered click handler does after its hit test (the forwarding to another open
  // panel is the caller's choice: send that panel's kind instead). Help-window controls wait for input (NeedInput without
  // an InputSource).
  GolferPanelAction = 0x100,   // clickGolferPanel: hitGolferPanel435f00's spot (-1, 0..10)
  EmployeeAction = 0x101,      // handleEmployeeAction: hitEmployee436b00's control (-3, -2, 0..6, >= 8 list rows)
  EmployeeViewSelect = 0x102,  // clickEmployeeView: hitEmployeeSlot435570's slot (-6..-2, 0..15)
  BuildToolSelect = 0x103,     // clickBuildTool433d30: hitBuildTool433c60's tool (-3, -2, 0..3)
  BuildingSelect = 0x104,      // pickBuilding: hitBuildingTool's cell (-3, -2, 0..8)
  AmenitySelect = 0x105,       // clickAmenityPanel: pickAmenity434980's entry (-2, 0..9)
  SlotPanelSelect = 0x106,     // clickSlotPanel: hitTestSlot432f90's slot (0..0x16)
  // 0x107.. reserved for the screens not ported yet (re/port/commands_split.tsv)
};

struct Command {
  CmdKind kind = CmdKind::None;
  // RootHandler: the handler id (1 onPress1, 2 onPoint, 3 onUnpoint, 4 onPress2, 5 onPoint2, 6 onUnpoint2, 7 onKey,
  // 8 onChar, 9 onChar0, 10 onPress1b, 11 onPress2b; trace_schema.md HANDLER).
  uint32_t handler = 0;
  int32_t a0 = 0, a1 = 0;  // RootHandler: the handler's two stack arguments (x, y / a, code / c)
  // RootHandler 1 / 4 (onPress1 / onPress2): the GetAsyncKeyState(VK_CONTROL) and (VK_SHIFT) results (low 16 bits) the
  // handler reads; the handler keeps bits 8..15 of each.
  uint32_t async_ctrl = 0, async_shift = 0;
  // RootHandler 2 / 3 / 7 (onPoint / onUnpoint / onKey): the timeGetTime value the deadline tail compares with
  // g_audio5aa774 (the trace's clock record at that site); unused when the deadline word is 0.
  uint32_t t = 0;
  // Semantic kinds: the decision (meaning per kind above).
  int32_t arg[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

// Performs the command's effect on ctx: RootHandler = the handler's writes; a semantic kind = the straddler's effect.
// Returns what the original function returns (eax), 0 for a void one. Throws like every port function (Fault,
// DivideError, NoReturn, MissingResource, NeedInput).
int32_t apply_command(Ctx &ctx, const Command &cmd);

// Convenience for tickFrame: applies each command of the frame in order (the H1 pump point).
void apply_commands(Ctx &ctx, const std::vector<Command> &cmds);

// What kind of pump asks (see MODAL BLOCKS).
enum class PumpKind : uint32_t { Blocking = 1, Drain = 2 };

// The run's raw input after the current frame's, for the pumps inside straddlers (see MODAL BLOCKS).
class InputSource {
 public:
  virtual ~InputSource() = default;
  // the next batch for a pump of this kind into `out` (cleared first); false when nothing is delivered to this call
  virtual bool batch(PumpKind kind, std::vector<Command> &out) = 0;
  // whether any input is left at all (a Blocking pump with none left throws NeedInput)
  virtual bool exhausted() const = 0;
  // the next cursor point a View_toLocal read returns (root-window client coordinates: the trace's CURSOR
  // records, in order); false = unknown, and a straddler whose effect depends on the cursor throws NeedInput
  virtual bool cursor(int32_t &x, int32_t &y) {
    (void)x;
    (void)y;
    return false;
  }
};
// A Blocking pump found no input left: neither an InputSource nor a remaining command (C ABI: SG_E_NO_RESOURCE). `what`
// is the hooks.csv name of the waiting function.
struct NeedInput : MissingResource {};
void set_input_source(Ctx &ctx, InputSource *src);  // not owned; nullptr detaches
InputSource *input_source(Ctx &ctx);                 // or nullptr

// true for the handlers that end a QueueInput batch: a press (onPress1 / onPress2 / onPress1b / onPress2b) or a key
// (onKey / onChar)
bool is_stop_handler(uint32_t handler);

// TEXT METRICS (U-0944). showPopup's option band and close button and textInputBox's width cut depend on Text_put widths:
// Text_put runs drawMarkupRun over the draw context's style fonts, and the font measures with jgld's Font::textWidth,
// which is GDI GetTextExtentPoint32A on a font made by CreateFontIndirectA from the game's own font file
// (CreateScalableFontResourceA + AddFontResourceA in Font::loadFile). The widths are the host rasterizer's, not a
// table in the game's files, so the port takes them from a per-context provider (a platform service like TerrainApi):
// `text_put` gets the draw context the caller measures on (the pointer the caller reads from the state, as the original
// passes it in ecx), the font object the caller's setDrawTarget selected as that context's base font (style fonts 1..3 =
// none), and the NUL-terminated text in the address space; it returns the width the original's Text_put returns (markup
// included). A function that needs a width with no provider attached throws MissingResource{"Text_put"}.
class TextMetrics {
 public:
  virtual ~TextMetrics() = default;
  virtual int32_t text_put(Ctx &ctx, uint32_t draw_ctx, uint32_t font, uint32_t text) = 0;
};
void set_text_metrics(Ctx &ctx, TextMetrics *tm);  // not owned; nullptr detaches
TextMetrics *text_metrics(Ctx &ctx);               // or nullptr

// A vector-backed InputSource (tests): a Blocking pump takes every command up to and including the first press or key, a
// Drain takes nothing.
class QueueInput : public InputSource {
 public:
  explicit QueueInput(std::vector<Command> cmds) : cmds_(std::move(cmds)) {}
  bool batch(PumpKind kind, std::vector<Command> &out) override {
    out.clear();
    if (kind != PumpKind::Blocking) return false;
    while (pos_ < cmds_.size()) {
      out.push_back(cmds_[pos_++]);
      if (out.back().kind == CmdKind::RootHandler && is_stop_handler(out.back().handler)) break;
    }
    return !out.empty();
  }
  bool exhausted() const override { return pos_ >= cmds_.size(); }
  size_t consumed() const { return pos_; }
  // every cursor read returns this point once set
  void set_cursor(int32_t x, int32_t y) {
    cx_ = x;
    cy_ = y;
    has_cursor_ = true;
  }
  bool cursor(int32_t &x, int32_t &y) override {
    x = cx_;
    y = cy_;
    return has_cursor_;
  }

 private:
  std::vector<Command> cmds_;
  size_t pos_ = 0;
  int32_t cx_ = 0, cy_ = 0;
  bool has_cursor_ = false;
};

}  // namespace portsim

#endif  // PORTSIM_SIM_COMMANDS_H
