// The view / simulation bridge (phase 3.3, 2026-10-19): the four functions between the presentation and the simulation
// whose effects ARE simulation state. Registered under their hooks.csv names; other modules call them by name or through
// this header. Specifications: re/analysis/render/0042fb90_worldToScreen.md, re/analysis/audio/0040c500_playSoundAt.md,
// re/analysis/ui/0040c720_queueMessage.md, 0040c890_pointsPopup.md, the hook sources (reading reference) and the C3 path-1 runs
// (log/diff/<addr>_<name>*.path1.csv), which port/test/test_bridge.py replays.
//
// Why they matter to the simulation: playSoundAt draws g_rng (Random::range(0x258)) only when worldToScreen puts the point
// on screen, so the RNG sequence depends on the camera (gCameraTileX / Y, g_zoomScale, g_cameraRot, g_screenWidth / Height,
// the doubling word unk_005a9cc0 and the tile heights); it also keeps the sound-variation words unk_005a9cd8 / unk_005a9cdc.
// pointsPopup subtracts its amount from a hole's money; queueMessage writes g_floatLabelY (class sim) and the label ring.
#ifndef PORTSIM_SIM_BRIDGE_H
#define PORTSIM_SIM_BRIDGE_H

#include <cstdint>
#include <functional>

#include "portsim/registry.h"

#include <vector>

namespace portsim {

// worldToScreen: projects world (wx, wy) (1024 units per tile) with the camera tile, zoom and screen size for rotation
// 0 / 2 / 4 / 6 into (sx, sy) (any other rotation leaves sx / sy as they came in), returns 0 when that lies outside the
// screen widened by m (x) / m / 2 (y); otherwise raises sy by the tile's height (TTA_HEIGHT_3: none; TTA_LEVEL_MIN: the
// min of cornerRange; TTA_LEVEL_MAX: g_tileLevel; else cornerHeights 5 / 7 / 1 / 3, bilinear when they differ), applies the
// doubling word and tests again; 1 = on screen. cornerHeights / heightBlend (phase 3.1a) are called by name.
int32_t world_to_screen(Ctx &ctx, int32_t wx, int32_t wy, int32_t &sx, int32_t &sy, int32_t m);
// P19A item 7, RENDER-ONLY: world_to_screen also computes, beside its integer result, the same projection without its integer
// divisions (the zoom's >> 7, the screen-size scaling, the 20 / 32 of y, the terrain height's / 4 and / 1024) in doubles, and keeps
// it for its last call on this thread. This returns that exact (ex, ey) when the last call was for (wx, wy), returned 1 and gave
// (sx, sy); false otherwise. Nothing the simulation reads changes; no call is added.
bool world_to_screen_exact(int32_t wx, int32_t wy, int32_t sx, int32_t sy, double &ex, double &ey);

// playSoundAt (the simulation part; no audio): projects (wx, wy) with margin 0; off screen with field34 != -1 returns 0;
// field34 == -1 (on or off screen) clamps the point to 0..799 / 0..599 and plays with field34 0; on screen otherwise applies
// the doubling word (eax = the old sx when it is set) and returns that eax when the point is off 0..799 / 0..500. Then the
// pitch is 300 - Random::range(0x258) (unk_005a9cd8 == 0) or the variation table entry unk_005a9cdc % 36 (and
// unk_005a9cdc += 1); playSound(sound, volume, pan, pitch, field34) is the audio backend's (a registered playSound is called,
// else nothing and 0); unk_005a9cd8 = 0; returns playSound's result.
int32_t play_sound_at(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t field34);

// The same body with the projection and the audio call supplied by the caller (test_bridge.py replays the C3 runs, whose
// worldToScreen and playSound were scripted stand-ins, through the test-only entry "bridge.playSoundAtScripted").
struct SoundAtCalls {
  std::function<int32_t(int32_t wx, int32_t wy, int32_t &sx, int32_t &sy, int32_t m)> project;
  std::function<int32_t(int32_t idx, int32_t volume, int32_t pan, int32_t pitch, int32_t f34)> play;
};
int32_t play_sound_at_with(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t field34, const SoundAtCalls &calls);

// queueMessage: label ring slot g_ui_53bba8: (a, b, c) to g_labelQueueC / g_floatLabelY / g_labelQueueA, 0x30 to
// g_labelQueueB, g_textBuffer strcpy'd into the slot's 64 bytes of g_labelQueueText, slot = (slot + 1) % 8 (unchecked).
void queue_message(Ctx &ctx, int32_t a, int32_t b, int32_t c);

// pointsPopup: nothing for pts 0 or g_flags bit 0x1000000; who != -1 subtracts pts from g_holes[who].money; money-label slot
// g_points59abb0: x, y, pts, 0x18; slot = (slot + 1) % 8 (unchecked).
void points_popup(Ctx &ctx, int32_t pts, int32_t x, int32_t y, int32_t who);

// P8B (widescreen stage 2, decision G32; log/roadmap/P8/P8B_notes.md): the VIEW BLOCK, 64 KB of the context's address space at
// kPortViewVa, outside the GameState and every image. Word +0 is the logical width of the course view the host shows (800 = the
// original's window; the wide view's W = window width * 600 / window height). Nothing of the original reads it; the `play`
// switches worldToScreen.wideViewVisibility and tileToScreen.wideViewVisibility do. A host changes it only inside a frame-head
// call, as a recorded harness write (DELIVER w32), so a replay applies it before the same frame body (tick.cpp head_writes).
constexpr uint32_t kPortViewVa = 0xFDF00000u;
constexpr uint32_t kPortViewSize = 0x10000u;
constexpr uint32_t kPortViewWidthVa = kPortViewVa;
// P9E fix 3: word +4 is 1 while the last press the host delivered was made in a moved HUD panel's VACATED place (the client
// point shows the course, but its 800-space point lies in a panel's place there, so the game's own tests would see the panel);
// 0 for every other press. Written by a live host at the delivery of each press, as a recorded harness write (DELIVER w32), so a
// replay applies it at the same point. Read only by the `play` switch mainLoop.vacatedPanelClick.
constexpr uint32_t kPortViewVacatedVa = kPortViewVa + 4u;
bool view_click_vacated(Ctx &ctx);
// P11D (Mariano's play-test 5, item 4: Ctrl+Z / Ctrl+Y for course edits; log/roadmap/P11/P11D_notes.md). Word +8 is the
// undo request a live host writes at a frame head as a recorded harness write (DELIVER w32): 1 undo, 2 redo. The `play` switch
// mainLoop.courseUndo (0) consumes it at the frame body's click point (mainLoop ML+0xedac) and clears it; at 1 (the original)
// nothing reads it. The words from +0x100 on (the edit journal, the redo list, the press counter) are written by sim code only,
// so a replay rebuilds them from the same input.
constexpr uint32_t kPortViewUndoVa = kPortViewVa + 8u;
// P21B (Mariano's play-test 8 item 2: the delay from an arrow key to the camera; log/roadmap/P21/P21B_notes.md). Word +12 is an
// arrow key-down (its key code 0x25..0x28) and word +16 its timeGetTime value (Command::t) that a live host took from its input
// queue at a frame head, as recorded harness writes (DELIVER w32), so a replay applies them before the same frame body. The `play`
// switch mainLoop.arrowStepAfterDraw (0) consumes them before the frame's picture (view_take_arrow, then rootWin_onKey(0, code) as
// the frame's pump would have run it) and clears both; at 1 (the original) no host writes them and nothing reads them. Word +20 is
// written by sim code only (1 = this frame's camera step was taken before the picture), so a replay rebuilds it. Word +24 (host,
// recorded like +12): key-ups (rootWin_onChar0) the host took at the same head, bit 0 one before the arrow key-down, bit 1 one
// after it (a key-up that waited for the frame's pump would come after the step, and P19A's held arrow would step a second time).
constexpr uint32_t kPortViewArrowVa = kPortViewVa + 12u;
constexpr uint32_t kPortViewArrowTVa = kPortViewVa + 16u;
constexpr uint32_t kPortViewArrowStepVa = kPortViewVa + 20u;
constexpr uint32_t kPortViewArrowUpVa = kPortViewVa + 24u;
// Word +28 (sim code only): 1 = H1 stepped the camera for an arrow in the last frame (after its picture, so the step is in the next
// picture already): the next frame steps nothing more before its picture.
constexpr uint32_t kPortViewArrowLateVa = kPortViewVa + 28u;
// P22A (Mariano's play-test 9 item 1: a light tap moved the camera one, two or three steps; log/roadmap/P22/P22A_notes.md). Word +32
// (sim code only, so a replay rebuilds it): the key-down time (Command::t, timeGetTime) of the arrow P19A's held word g_char83821c
// holds, written by rootWin_onKey's handler when the held word takes a new arrow (a Windows key repeat of the same arrow keeps it).
// Under mainLoop.arrowKeyHeldRepeat = 0 the held arrow steps the camera again only once the frame head is kArrowHoldMs or more
// after that time: a tap steps once whatever the frame clock, a held key scrolls one step per frame after the delay.
constexpr uint32_t kPortViewArrowHeldTVa = kPortViewVa + 32u;
constexpr int32_t kArrowHoldMs = 250;
// Word +36 (host, a recorded harness write like +12): the frame head's clock minus timeGetTime, the clock Command::t is on (a
// live host's clock_origin; 0 = the same clock, the default app). view_arrow_held_due adds it to word +32.
constexpr uint32_t kPortViewClockOffVa = kPortViewVa + 36u;
// true when the held arrow may step at the frame head time `head` (no view block: always, as before P22A)
bool view_arrow_held_due(Ctx &ctx, uint32_t head);
// what the host took at the frame head: the arrow key-down (code 0 = none) and its time, the key-up bits of word +24 (false: none
// of them, or no view block); clears words +12 / +16 / +24
bool view_take_arrow(Ctx &ctx, int32_t &code, uint32_t &t, int32_t &ups);
extern int g_defect_course_undo;  // mainLoop.courseUndo: 1 = the original (no journal, the request word ignored)
// setGrids's journal entry (called by setGrids before it stores, with the tile's previous code / refund and the new ones)
void course_undo_note(Ctx &ctx, int32_t x, int32_t y, int32_t old_code, int32_t old_refund, int32_t new_code,
                      int32_t new_refund);
// a paint the original keeps no undo record of (mainLoop ML+0x111da: the old type was a tee or a green): an entry that refuses
void course_undo_unrecorded(Ctx &ctx, int32_t x, int32_t y);
// fix 3: H2_Build placed object record `rec` at (x, y) and charged `price` to the cash and `ledger` to the facilities row and the
// total dword (free_unlock: it used a free unlock bit); undoable false: the entry refuses (the clubhouse, a placement that
// removed earlier objects of its type). A redone object gets the orientation it had (course_redo_orientation)
void course_undo_object(Ctx &ctx, int32_t x, int32_t y, int32_t rec, int32_t price, int32_t ledger, bool free_unlock,
                        bool undoable);
void course_undo_press(Ctx &ctx);  // onPoint (a left press): the next edits form a new undo step
void course_undo_reset(Ctx &ctx);  // a course was set up or loaded: the journal and the redo list are emptied
// the frame body's click point: takes a pending request (switch 0 only). 0 none, 1 an undo (run here), 2 a redo: the caller
// runs the game's build path on course_redo_take's tiles between course_redo_begin and course_redo_end (P11D fix 2)
int32_t course_undo_step(Ctx &ctx);
struct CourseRedoItem {
  int32_t x, y;
  int32_t tool;         // the terrain tool [f_w_4c2848] the edit was made with
  int32_t tool3;        // the object tool [f_w_4c2848 + 0xc] (-1: a terrain edit, H2_Terrain; else H2_Build)
  int32_t variant;      // [unk_0056a514] then
  int32_t orientation;  // a placed object's orientation (-1: a tile edit)
};
// the newest undone step's tiles that still hold what the undo left there, in the order they were built; the step leaves the
// redo list. false (and the refusal sound) when the list is empty; the refusal sound also when a tile was skipped
bool course_redo_take(Ctx &ctx, std::vector<CourseRedoItem> &out, uint32_t &press);
void course_redo_begin(Ctx &ctx, uint32_t press);  // the journal entries the redo makes keep the step's press, the redo list stays
void course_redo_orientation(Ctx &ctx, int32_t orientation);  // the next object the redo places gets it (-1 none)
void course_redo_end(Ctx &ctx);
// maps the block (width 800) when the context has none; true when it is mapped afterwards
bool view_map(Ctx &ctx);
// the logical view width: 800 when the block is not mapped or holds less than 800
int32_t view_width(Ctx &ctx);
// the side margin E = (view width - 800) / 2 the wide visibility switches add on each side; 0 with the switch at 1 (the original)
int32_t wide_view_margin_w2s(Ctx &ctx);
int32_t wide_view_margin_t2s(Ctx &ctx);

}  // namespace portsim

#endif  // PORTSIM_SIM_BRIDGE_H
