// P24D (log/roadmap/P24/P24D_notes.md): the app's side of the language packs (portsim/sim/lang.h): the pack file, the language
// remembered between runs, and the title menu's language selector.
//
// PACK. SG_LANG_PACK=<file> when set, else <install>\Lang\simgolf_es.txt (the LAN build's game\Lang\; the public repo and the
// GitHub zips never carry one). No pack: English only, and the title menu shows no selector.
// LANGUAGE. SG_LANG=en|es when set, else `language=` in <data>\settings.ini (the app's data directory, %LOCALAPPDATA%\SimGolfPort
// by default), else English. A choice in the selector is written there.
// SELECTOR. A dropdown in the title menu's top right corner (drawn by the gfx body `lang.selector` in the game's own frame and
// banner font, before the menu's refreshFull; LiveSession::title_menu_overlay): a click on it opens the list, a click on an entry
// switches the language at once (no restart). SG_APP_P24=off: no pack is loaded (English, no selector).
#ifndef PORTAPP_LANG_UI_H
#define PORTAPP_LANG_UI_H

#include <atomic>
#include <cstdint>
#include <string>

namespace portapp {

struct LangUi {
  // the selector's 800-space box (closed) and the list's rows under it
  static constexpr int32_t kX = 0x2a8, kY = 4, kW = 0x70, kH = 0x20, kRowH = 0x18;
  std::atomic<bool> available{false};  // a pack was loaded
  std::atomic<bool> open{false};       // the list is shown
  std::atomic<int> hover{-1};          // the list row under the cursor (-1 none)
  std::string settings;                // <data>\settings.ini

  // load the pack and the remembered language; prints one `LANG ...` line
  void startup(const std::string &install, const std::string &data_dir);
  // switch to `id` and remember it
  void choose(int id);
  // a left button press / release or a move at 800-space (x, y) while the title menu shows: true when the selector takes it
  // (the game does not see it)
  bool mouse(uint32_t msg, int32_t x, int32_t y);
  // (x, y) is over the selector's box, or its list when open
  bool covers(int32_t x, int32_t y) const {
    if (!available.load() || x < kX || x >= kX + kW || y < kY) return false;
    return y < kY + kH || (open.load() && y < kY + kH + 2 * kRowH);
  }
  bool down_taken = false;  // the press went to the selector, so its release does too
};

LangUi &lang_ui();

}  // namespace portapp

#endif  // PORTAPP_LANG_UI_H
