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

namespace portsim {

// worldToScreen: projects world (wx, wy) (1024 units per tile) with the camera tile, zoom and screen size for rotation
// 0 / 2 / 4 / 6 into (sx, sy) (any other rotation leaves sx / sy as they came in), returns 0 when that lies outside the
// screen widened by m (x) / m / 2 (y); otherwise raises sy by the tile's height (TTA_HEIGHT_3: none; TTA_LEVEL_MIN: the
// min of cornerRange; TTA_LEVEL_MAX: g_tileLevel; else cornerHeights 5 / 7 / 1 / 3, bilinear when they differ), applies the
// doubling word and tests again; 1 = on screen. cornerHeights / heightBlend (phase 3.1a) are called by name.
int32_t world_to_screen(Ctx &ctx, int32_t wx, int32_t wy, int32_t &sx, int32_t &sy, int32_t m);

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

}  // namespace portsim

#endif  // PORTSIM_SIM_BRIDGE_H
