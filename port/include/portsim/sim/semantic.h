// Semantic INPUT ACTIONS (phase 3.8, stream 38i; module `semantic`, port/src/sim/semantic.cpp; tests
// port/test/test_semantic_input.py; translator port/test/semantic.py `xlate` / `gate`; notes log/p38/38i_notes.md).
//
// The command kinds 0x200.. of portsim/sim/commands.h. A semantic input action is what the player DOES (press the
// zoom-out key, click control 4 of the golfer panel, click tile (12, 30) of the course, swing), with no root-handler
// mechanics: no handler ids, no GetAsyncKeyState samples, no clock values, no separate WM_KEYDOWN / WM_CHAR records.
// apply_command LOWERS it to the root-handler calls that input makes in the original (the handlers of commands.cpp, the
// same code RootHandler commands run), at the dispatch point it is applied at. Everything after that is the game's own
// code: H1's key switch (takeKey / takeCode, the char and code switches), H2's press code (g_cachedCameraTileX = -1,
// screenToTile of the click, the button word cleared, the return to the frame head when a panel handler returns <= 0)
// and the routing to the panel handlers, the world action, or a modal screen's own hit tests. So a semantic command has
// exactly the effect its input has, H2 included (U-1011).
//
// What a lowering leaves out, with the reason (each is a word no code of golf_clean.exe reads, or one whose value cannot
// differ; re/model/inventory_addresses.tsv):
//   - the KEY RELEASE (WM_KEYUP -> rootWin_onChar0): its only write is the held-key word g_char83821c, which the exe only
//     writes (refs 2: onChar, onChar0). A Key action is a tap; no release command exists.
//   - g_key838208 (onKey: 1 for the code 0x1000d): written only (refs 1).
//   - the handlers' deadline clock (onPoint / onUnpoint / onKey compare timeGetTime with g_audio5aa774 to clear the byte
//     g_ui83ab9c): with the deadline word 0 the byte is cleared whatever the clock; the word is 0 in all 200 segment
//     images and only playAudioFile sets it. A lowering takes the clock from arg[7] and refuses to run with the deadline
//     set and no clock given (MissingResource).
// Every other write of the handlers is made, with the values the raw handler call would make.
//
// TARGETS (Click). The click kind carries what the click is FOR; the lowering evaluates where mainLoop H2's first-level
// routing (its press-code entry) sends a click at that point on the state at the dispatch point, and refuses the
// command (CommandRefused) when the stated target does not hold. Target 0 states nothing (a click on a modal screen,
// popup or menu, whose own hit tests decide).
#ifndef PORTSIM_SIM_SEMANTIC_H
#define PORTSIM_SIM_SEMANTIC_H

#include <cstdint>

#include "portsim/registry.h"
#include "portsim/sim/commands.h"

namespace portsim {

// The keys mainLoop's H1 acts on, by what it does with them (its char switch on takeKey's value 9..0x7a, 56 cases, and
// its code switch on takeCode's value 0x21..0x79, 16 cases), plus the keys only modal screens read. One row per key:
// SG_KEY(id, name, vk, ch): the virtual-key code and the character the key delivers (ch 0 = a non-character key, which
// jgld delivers as onKey(0, vk) alone; a character key is onChar(vk) then onKey(ch, vk)). ids are stable (append only).
// The pairs are the recorded ones (the corpus's handler records) for Escape, Tab, Enter, Backspace, the letters b d e f h l
// n p s t x z, '+', '-' and every non-character key; the other rows take the US-layout key of the character. For a
// character key the virtual key reaches H1 only through its code switch, whose cases (0x21, 0x24..0x28, 0x70..0x79) no
// character key's virtual key hits, so H1's effect depends on the character alone; a modal screen may read the code.
// Where H1's case depends on the panel mode g_panelListMode (3 = the player's golfer is up), the name gives both.
//   Escape    H1: as Enter, the key consumed; a modal screen: closes it        Enter  H1: drops the tool selection
//   Tab       H1: +1 to the view counter, with g_flags bit 1 the cycle of three g_ui_542f20 values
//   Space     H1: drops every selection and sets panel mode 5 (dropped while the report byte g_ui_569498 is set)
//   SelectHoleN  centres the camera on hole N (1..9 the digits, 10..18 the shifted digits ! @ # $ % ^ & * ( of a US layout)
//   RotateNorth ('0') camera rotation 0 (recentred on the first placed object when it already is 0)
//   CashCheat ('+') g_cash_div100 += 100 and the cheat flag g_flags bit 29     Lower/RaiseCorner ('-' / '=') the corner level
//   HolePreview ('.') the hole's trial-shot preview (a modal screen); HolePreviewNearest ('/') for the hole nearest the cursor
//   EditGolfer ('?') the character editor for the selected golfer, else the message timer
//   F1..F10   the report screens through the report byte (F5 the routing map and F10 the bulletin board directly)
//   PageUp / Home  rotate the camera by +2 / -2; arrows: step the camera 4 tiles (by rotation); PageDown, End: no H1 action
#define SG_SEMANTIC_KEYS(SG_KEY)                       \
  SG_KEY(1, Tab, 0x09, 0x09)                           \
  SG_KEY(2, Enter, 0x0d, 0x0d)                         \
  SG_KEY(3, Escape, 0x1b, 0x1b)                        \
  SG_KEY(4, Space, 0x20, 0x20)                         \
  SG_KEY(5, SelectHole1, 0x31, '1')                    \
  SG_KEY(6, SelectHole2, 0x32, '2')                    \
  SG_KEY(7, SelectHole3, 0x33, '3')                    \
  SG_KEY(8, SelectHole4, 0x34, '4')                    \
  SG_KEY(9, SelectHole5, 0x35, '5')                    \
  SG_KEY(10, SelectHole6, 0x36, '6')                   \
  SG_KEY(11, SelectHole7, 0x37, '7')                   \
  SG_KEY(12, SelectHole8, 0x38, '8')                   \
  SG_KEY(13, SelectHole9, 0x39, '9')                   \
  SG_KEY(14, SelectHole10, 0x31, '!')                  \
  SG_KEY(15, SelectHole11, 0x32, '@')                  \
  SG_KEY(16, SelectHole12, 0x33, '#')                  \
  SG_KEY(17, SelectHole13, 0x34, '$')                  \
  SG_KEY(18, SelectHole14, 0x35, '%')                  \
  SG_KEY(19, SelectHole15, 0x36, '^')                  \
  SG_KEY(20, SelectHole16, 0x37, '&')                  \
  SG_KEY(21, SelectHole17, 0x38, '*')                  \
  SG_KEY(22, SelectHole18, 0x39, '(')                  \
  SG_KEY(23, RotateNorth, 0x30, '0')                   \
  SG_KEY(24, CashCheat, 0xbb, '+')                     \
  SG_KEY(25, LowerCorner, 0xbd, '-')                   \
  SG_KEY(26, RaiseCorner, 0xbb, '=')                   \
  SG_KEY(27, HolePreview, 0xbe, '.')                   \
  SG_KEY(28, HolePreviewNearest, 0xbf, '/')            \
  SG_KEY(29, EditGolfer, 0xbf, '?')                    \
  SG_KEY(30, GolferSnapshot, 0x42, 'B')                \
  SG_KEY(31, SaveCourse, 0x43, 'C')                    \
  SG_KEY(32, TournamentReport, 0x4a, 'J')              \
  SG_KEY(33, LoadGame, 0x4c, 'L')                      \
  SG_KEY(34, TogglePreference, 0x4e, 'N')              \
  SG_KEY(35, Pause, 0x50, 'P')                         \
  SG_KEY(36, RoutingMap, 0x52, 'R')                    \
  SG_KEY(37, SaveGame, 0x53, 'S')                      \
  SG_KEY(38, ToggleFlag10, 0x54, 'T')                  \
  SG_KEY(39, SwitchCourse, 0x57, 'W')                  \
  SG_KEY(40, ResetTerrain, 0x58, 'X')                  \
  SG_KEY(41, RandomBuildSlot, 0x42, 'b')               \
  SG_KEY(42, ShotStyle1, 0x44, 'd')                    \
  SG_KEY(43, ToggleBuildPick, 0x45, 'e')               \
  SG_KEY(44, ShotStyleM1OrList2, 0x46, 'f')            \
  SG_KEY(45, ListToolPanel, 0x47, 'g')                 \
  SG_KEY(46, ShotStyle3OrAnnounceBuilding, 0x48, 'h')  \
  SG_KEY(47, ShotStyle4, 0x4c, 'l')                    \
  SG_KEY(48, AbandonPlay, 0x4e, 'n')                   \
  SG_KEY(49, BuildSlotPanel, 0x50, 'p')                \
  SG_KEY(50, ListCoursePanel, 0x52, 'r')               \
  SG_KEY(51, ShotStyle0OrList7, 0x53, 's')             \
  SG_KEY(52, ListPanel13, 0x54, 't')                   \
  SG_KEY(53, ListPanel10, 0x56, 'v')                   \
  SG_KEY(54, ListPanel17, 0x57, 'w')                   \
  SG_KEY(55, ZoomOut, 0x58, 'x')                       \
  SG_KEY(56, ZoomIn, 0x5a, 'z')                        \
  SG_KEY(57, RotatePlus, 0x21, 0)                      \
  SG_KEY(58, PageDown, 0x22, 0)                        \
  SG_KEY(59, End, 0x23, 0)                             \
  SG_KEY(60, RotateMinus, 0x24, 0)                     \
  SG_KEY(61, ScrollLeft, 0x25, 0)                      \
  SG_KEY(62, ScrollUp, 0x26, 0)                        \
  SG_KEY(63, ScrollRight, 0x27, 0)                     \
  SG_KEY(64, ScrollDown, 0x28, 0)                      \
  SG_KEY(65, ReportCourse, 0x70, 0)                    \
  SG_KEY(66, ReportComments, 0x71, 0)                  \
  SG_KEY(67, ReportHistograph, 0x72, 0)                \
  SG_KEY(68, ReportFinancial, 0x73, 0)                 \
  SG_KEY(69, ReportRoutingMap, 0x74, 0)                \
  SG_KEY(70, WorldMap, 0x75, 0)                        \
  SG_KEY(71, ReportEvaluation, 0x76, 0)                \
  SG_KEY(72, ReportShortcuts, 0x77, 0)                 \
  SG_KEY(73, ReportMembership, 0x78, 0)                \
  SG_KEY(74, BulletinBoard, 0x79, 0)                   \
  SG_KEY(75, Backspace, 0x08, 0x08)

enum class KeyAction : int32_t {
  // arg[1] = the character, arg[2] = the virtual-key code (any key the table has no row for, e.g. text typed into a box)
  Char = 0,
#define SG_KEY_ENUM(id, name, vk, ch) name = id,
  SG_SEMANTIC_KEYS(SG_KEY_ENUM)
#undef SG_KEY_ENUM
};

// The input-action kinds (CmdKind::Key, PointerDown, Click, TileClick in commands.h; arguments documented there)
constexpr uint32_t kKindKey = 0x200, kKindPointerDown = 0x201, kKindClick = 0x202, kKindTileClick = 0x203;

// Click targets: where H2 sends the click (arg[4] of a Click; the routing at the press-code entry of mainLoop H2)
enum class ClickTarget : int32_t {
  Any = 0,           // no check: a click on a modal screen, a popup or a menu
  World = 1,         // the course view (H2's world action: select, build, demolish, place); arg[5], arg[6] = the tile
                     // screenToTile gives for the point (-1, -1 = not checked)
  Panel = 2,         // a HUD panel control: arg[5] = the panel's semantic kind (0x100 golfer panel in panel modes 3 / 4,
                     // 0x102 employee view in mode 2, 0x104 building picker in mode 1, 0x106 slot panel in mode 0),
                     // arg[6] = the control (the panel's hit test at the point)
  Hud = 3,           // the button bar at the lower left (showSystemMenu: x < 0xc8, y > 0x1b8)
  Swing = 4,         // the player's shot: a left click on the course in panel mode 3 (copies the aim into the shot)
  Place = 5,         // a pending placement (f_w_4c2e00 word 4 != -1): the selected walker / golfer to the cursor tile
                     // (left) or the placement dropped (right); arg[5] = that word
  GolferAction = 6,  // a pending golfer-panel action (g_msgState53df54 > 0, g_infoPanelTab != -1); arg[5] = g_infoPanelTab
  TerrainEdit = 7,   // g_flags bit 1: H2 maps the click to a key (left '=', right '-', or '/'); arg[5] = that key
};

// A semantic command whose stated intent does not hold on the state it is applied to (C ABI: SG_E_NO_RESOURCE).
struct CommandRefused : MissingResource {};

// apply_command for the kinds 0x200..0x2ff (commands.cpp forwards them): returns what the last root handler returns
int32_t semantic_apply(Ctx &ctx, const Command &cmd);
bool is_semantic_input(CmdKind k);

// The route of a click at (x, y) with `button` on the context's current state (ClickTarget, and the two target arguments);
// no state changes (the screenToTile it needs runs on a copy of the state)
struct ClickRoute {
  ClickTarget target = ClickTarget::Any;
  int32_t a = -1, b = -1;
  int32_t tile_x = -1, tile_y = -1;  // the tile screenToTile gives for the point (every route)
};
ClickRoute click_route(Ctx &ctx, int32_t x, int32_t y, int32_t button);

// the (vk, ch) a key action is lowered with; false for an unknown id
bool key_binding(int32_t id, uint32_t &vk, uint32_t &ch);

}  // namespace portsim

#endif  // PORTSIM_SIM_SEMANTIC_H
