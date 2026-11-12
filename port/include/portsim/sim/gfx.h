// The software 16-bit graphics backend (phase 5, step 1; plan re/PHASE5_GFX_PLAN.md). Module `gfx` (port/src/sim/gfx.cpp),
// tests port/test/test_gfx.py.
//
// The frame code calls every drawing function of the original by its hooks.csv name, and tick.cpp resolves them as drops.
// With a Gfx attached to the context (gfx.attach), tick hands a drop the backend has a body for to gfx_drop: the backend
// draws into its own RGB555 surfaces, keyed by the original objects' 32-bit addresses, and writes nothing into the
// GameState or its address space, so the simulation, its hashes and every segment digest are the same with or without it.
// A call the backend has no body for, or a case of a body it does not model, is recorded (gfx.unsupported) and behaves as
// the drop. The checkpoint frame of the original is jgld's back buffer; the backend's back buffer gets the root surface at
// each Window::refreshFull, and gfx.snapshotAt(pos) copies it when the run first consumes a trace item (a CURSOR read or a
// dispatch) whose clock ordinal is past `pos` (the checkpoint's clock call).
#ifndef PORTSIM_SIM_GFX_H
#define PORTSIM_SIM_GFX_H

#include <algorithm>  // P22E: GfxTextRun::box_x0 / box_x1
#include <atomic>
#include <cstdint>
#include <functional>  // P15A fix 3: gfx_world_op
#include <memory>  // P12A fix 2: GfxHrScene
#include <string>
#include <vector>

#include "portsim/registry.h"

namespace portsim {

class Gfx;
void set_gfx(Ctx &ctx, Gfx *g);  // not owned; nullptr detaches
Gfx *gfx(Ctx &ctx);              // or nullptr

// contexts with a Gfx attached (the frame code's fast test before it calls into the backend)
extern std::atomic<int> g_gfx_attached;

// The key of an object a function of the original keeps in its own frame (a local Surface array or Image): no GameState
// address reaches this range, so port code can name such objects to the backend without an address.
constexpr uint32_t gfx_local_key(uint32_t off) { return 0xFFF00000u + off; }
constexpr bool gfx_is_local_key(uint32_t k) { return k >= 0xFFF00000u; }

// tick.cpp: a drop named `name` called with `a[0..n)`; true when the attached backend handled it (the frame still uses the
// drop's result). Never throws.
bool gfx_drop(Ctx &ctx, const char *name, const uint32_t *a, int n);
// tick.cpp: after gfx_drop, the value the body computed for the frame code (roadmap S3.7: Text_put's width); false when the
// body set none (the frame keeps the drop's result)
bool gfx_take_value(Ctx &ctx, int32_t &v);
// tick.cpp: the run consumes a trace item at clock ordinal `pos` (a CURSOR record or a dispatch)
void gfx_progress(Ctx &ctx, uint32_t pos);
// tick.cpp: the run stopped because the recorded input ran out (the menus wait for input the trace no longer holds): every
// snapshot not yet taken is past the last item the run could consume, so the back buffer as it is now is its frame
void gfx_end_of_input(Ctx &ctx);
// roadmap S7.9 (U-1670): called after every Window::refreshFull the backend's body handled, whatever path issued it (the frame
// code's drop, a module's own drawing calls, a family calling the body), with the back buffer already updated: where the
// original's back buffer reaches the window. nullptr removes it. Only a live run's core sets one (live.cpp).
using GfxRefreshHook = void (*)(void *arg);
void gfx_set_refresh_hook(Ctx &ctx, GfxRefreshHook fn, void *arg);
// P8B: whether the back buffer holds a terrain picture (Gfx::back_terrain: set by a terrain pass or restore, cleared by a 2D
// write covering the whole root surface, copied at Window::refreshFull); false without a backend
bool gfx_back_terrain(Ctx &ctx);
// P11C (log/roadmap/P11/P11C_notes.md): the hover help, set by the app's host (--hover-help on, the default; SG_APP_P11=off: off).
// With it on, showInfoCursorPanel's tool tip gets one more line above it (gfx_text.h hover_help_tool) and the frame code's
// hover.terrain call (P6, after the cursor's View_toLocal) draws the line of the tile under the cursor after the HUD tips' dwell,
// both the game's one-line tip (drawPanelBackdrop). Drawing only: the GameState is never written. Off (the default): nothing.
void gfx_set_hover_help(Ctx &ctx, bool on);
bool gfx_hover_help(Ctx &ctx);
// P13A (log/roadmap/P13/P13A_notes.md item 2): whether the hover help includes hover.terrain's course line (the tile under the
// cursor with no tool held). On by default (P11C's rule); the app's host sets it off unless --hover-help course or SG_APP_P13=off,
// so the Build palette's, its slots' and the amenities' lines stay and nothing is drawn over the course while nothing is selected.
void gfx_set_hover_help_course(Ctx &ctx, bool on);
// P22D fix 3 (Mariano's play-test 9 item 8: "hovering over these icons should tell what they do"; log/roadmap/P22/P22D_notes.md):
// the status boxes' tips, set by the app's host (--meter-tips on, the default; SG_APP_P22=off: off). With it on, P6's status bar
// (mainLoop ML+0x9ef5..ML+0xa1ba, inside the meters layer) draws, once the cursor rested over the cash, fun rating or skill rating
// box for the HUD tips' dwell, the game's own name for it as the game's one-line tip (drawPanelBackdrop) left of the box. Drawing
// only: the GameState is never written. Off (the default): nothing.
void gfx_set_meter_tips(Ctx &ctx, bool on);
// P24G (Mariano's play-test 11 item 7: "an intermediate UI / indicator when tournament is starting, otherwise I can keep clicking on
// tournament button or practice"; log/roadmap/P24/P24G_notes.md), set by the app's host (--tourney-hold on, the default;
// SG_APP_P24=off: off). clickGolferPanel ignores Practice Round, Play <pro> and Begin Tournament while g_flags bit 21 (a tournament
// is on) or bit 26 is set, but drawGolferActionPanel greys them only by the hole count, the challenge and the offer bit 22, which a
// tournament keeps. With it on, the three are drawn greyed (the game's own covers) under that same mask, and until the first stroke
// of the tournament field the game's one-line tip "Starting tournament..." sits in the panel's empty shot row. Drawing only: the
// GameState is never written. Off (the default): the original's drawing.
void gfx_set_tourney_hold(Ctx &ctx, bool on);
bool gfx_meter_tips(Ctx &ctx);
inline void gfx_progress_fast(Ctx &ctx, uint32_t pos) {
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_progress(ctx, pos);
}

// ------------------------------------------------------------------------------------------------ P9B: HUD layers
// The layered 2D compositor (log/roadmap/P9/P9B_notes.md). With capture on (gfx_set_hud_capture, set only by the app's host), the
// backend records, between two Window::refreshFull bodies, which drawing calls belong to which HUD panel: the frame code opens a
// layer around a panel's drawing (GfxLayerScope; tick.cpp around the panel functions it calls by name, tick_frame.inc around the
// inline title / meters / golfer-strip passes), and every root-surface pixel the calls inside it change becomes a pixel of that
// layer, with the blend the pixel was drawn with when the backend's blend primitives drew it over a pixel the layer had not drawn
// (Surface_stretchBlit's mask blend, Sprite::drawG's shade byte). Pixels changed outside every layer form centred layers in draw
// order. Window::refreshFull publishes the frame's layers with the root as it was before the first layer opened (the base) and a
// copy of the back buffer they compose to. Nothing is written into the GameState, the back buffer or the root surface: the 800 x 600
// frame every gate sees is the same with capture on or off.
enum GfxLayerId : int32_t {
  kGfxLayerCentred = 0,  // drawn outside every panel (labels, the message ticker, previews, dialogs): stays centred
  kGfxLayerTitle = 1,    // P6's title strip: logo, course name, difficulty, date, counts row (left edge)
  kGfxLayerMeters = 2,   // P6's status boxes: money, fun, skill (right edge)
  kGfxLayerBottom = 3,   // the golfer strip, the radial menu, the palette and building panels (left edge)
  kGfxLayerPanel = 4,    // the employee view / detail and golfer action panels drawn in the palette's place (right edge)
  kGfxLayerStrip = 5,    // P13C: the golfer strip's smiley faces (left edge in the 800-space frame; the app can place it right of the palette)
  kGfxLayerCount = 6,
};
// one layer pixel: kind 0 not drawn by the layer (transparent), 1 opaque word `px`, 2 the mask blend of jgld blitPair1 with source
// word `src` and weight `k` (per 5-bit channel c: ((dst8 * k) >> 8) + src8) >> 3 with x8 = c << 3), 3 the destination through the
// shade table `k` (gfx_shade_table), 4 (P9D) the quarter blend of Surface_drawSprite's drawAlpha member draw16_1002ed60 with source
// word `src` (gfx_layer_quarter: floor((src + 3 dst) / 4) per 5-bit channel), the dim the report screens draw over the frame
struct GfxLayerPx {
  uint16_t px = 0, src = 0;
  uint8_t kind = 0, k = 0;
};
struct GfxHudLayer {
  int32_t id = kGfxLayerCentred;
  int8_t anchor = 0;  // -1 left edge, 0 centred, +1 right edge
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // bounding box in 800-space, half-open
  std::vector<GfxLayerPx> px;               // (x1 - x0) * (y1 - y0), rows top-down
  // P9D: the layer's segment dimmed the whole frame (one Surface_drawSprite quarter blend over every root pixel with one source
  // word): a presenter dims everything composed before it in the whole window with dim_src (the panels at their anchors, the world,
  // the side bands), and its kind-4 pixels are then that dim (already applied)
  uint8_t dim = 0;
  uint16_t dim_src = 0;
  // P19C item 8: for the golfer strip's layer, the 800-space x just right of the left-anchored panel pixels in the strip's rows
  // (hud_publish): the strip sits there (the open Build palette's end, else the round control cluster's), -1 = none found
  int32_t strip_x = -1;
  // P22D fix 2: the golfer list's added columns (b_view_panel, gfx_hud.cpp): a presenter places the layer `shift` 800-space columns
  // right of its anchor's place, and a point over it reaches the game `shift` columns right of the layer's own 800-space point
  // (the list's columns past the original four: hitEmployeeSlot435570 takes them with the play switch drawEmployeeViewPanel.fourColumns
  // at 0); 0 for every other layer
  int32_t shift = 0;
  // P9E: a centred gap made while popups were recorded (gfx_set_hud_popups): its pixels whose GfxHudFrame::owner names a piece are
  // that popup's and are composed at the piece's opener panel's place
  uint8_t popups = 0;
  const GfxLayerPx *at(int32_t x, int32_t y) const {
    if (x < x0 || x >= x1 || y < y0 || y >= y1) return nullptr;
    const GfxLayerPx &p = px[static_cast<size_t>(y - y0) * static_cast<size_t>(x1 - x0) + static_cast<size_t>(x - x0)];
    return p.kind ? &p : nullptr;
  }
};
// P9E: one popup of the frame (gfx_hud_popup_begin): the panel layer it was opened from (kGfxLayerTitle .. kGfxLayerPanel; the open
// panel layer, else the panel layer under the opening click, else the opener of an earlier popup under the click; kGfxLayerCentred
// = none: it stays centred) and the 800-space box of the root pixels it owns (half-open; empty when it owns none)
struct GfxHudPiece {
  int32_t opener = kGfxLayerCentred;
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  uint8_t text = 0;  // P10C fix 4: 1 = a text input dialog (gfx_hud_popup_begin's text_box: textInputBox, saveGameDialog), 0 = a showPopup
  int32_t parent = -1;  // P10C fix 4: the piece open when this one began (a popup drawn inside another dialog), -1 = none
};
// P10I fix 1 (gfx.h gfx_set_hud_lines): one line from the tee to the cursor (or from the green, the tee tool's arm) a cursor pass
// drew: Terrain::drawLine's arguments as drawMinimapSegment passed them (800-space ends, the first the tee / green, the second the
// cursor tile's screen point), the GL line width after the call and the alpha argument (tenths)
// P22E item 7 (gfx_set_hud_edge): also the four edges of P7's placement box (drawMinimapSegment width 1, alpha 7, colour 0x80007fff
// or 0x80007d08 inside a cursor pass), `still`: neither end follows the cursor previews (the box stays with its label until the
// next frame, as its 800-space pixels did), so the presenter draws the whole box, past the 800 frame's columns too
struct GfxHudLine {
  int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
  uint32_t colour = 0;
  float width = 1.0f;
  int32_t alpha = 10;
  bool still = false;
  // P23E fix 6 (kGfxWideMarks): a filled rectangle, x1 .. x2 by y1 .. y2 half-open in 800 space, colour the 555 word it wrote (alpha
  // 10, still); a presenter fills the client pixels whose centre lies inside
  bool fill = false;
};
struct GfxHudFrame {
  std::vector<uint16_t> base;  // the root before the first layer opened (800 x 600); empty = no layer this frame
  std::vector<uint16_t> back;  // the back buffer the layers were published with (a present checks it is the frame it shows)
  std::vector<GfxHudLayer> layers;  // in draw order
  uint64_t serial = 0;
  // P9D: why the publish made no layers (0 = it made them; 1 = no layer opened since the last publish; 2 = inside an open layer)
  int8_t none = 0;
  // P21C: a layer-less modal frame (none != 0) whose screen dimmed every root pixel with one word (end of year: drawn before
  // any panel layer of its frame): the presenter dims the part of the window outside the 800 x 600 root with dim_src
  uint8_t dim_all = 0;
  uint16_t dim_src = 0;
  // P9D: the frame of a modal screen drawn over the last layered frame (its base and layers carried, the screen's drawing since
  // then in a centred layer after them): gfx_set_hud_modal
  uint8_t carried = 0;
  // P9E: the popups (showPopup / textInputBox) drawn since the frame's picture started, and per root pixel (800 x 600) the piece
  // index + 1 whose popup was open when the pixel last changed (0 = none). Empty unless a piece has an opener panel.
  std::vector<GfxHudPiece> pieces;
  std::vector<uint8_t> owner;
  // P9E fix 4: 1 where a sprite draw of the frame code's cursor passes (gfx_set_hud_cursor: Surface_drawSprite, the root part
  // of drawQueuedSprite.draw) changed the root (800 x 600; empty = none): the tool preview's own pixels in the centred layers
  std::vector<uint8_t> cursor2d;
  // P10I fix 1: the tee lines of the cursor passes (gfx_set_hud_lines), and per root pixel (800 x 600; empty = none) 1 where one
  // of them changed the root, with line_word the word it left there (a later draw over it, the yards label, leaves another word)
  std::vector<GfxHudLine> lines;
  std::vector<uint8_t> line800;
  std::vector<uint16_t> line_word;
  bool empty() const { return base.empty(); }
};
int8_t gfx_layer_anchor(int32_t id);
void gfx_set_hud_capture(Ctx &ctx, bool on);  // the app's host only
// P9D: the modal screens over the HUD (log/roadmap/P9/P9D_notes.md), set by the app's host after gfx_set_hud_capture. With it on:
// - the report screens' dim (Surface_drawSprite's quarter blend) is recorded as layer pixels of kind 4, and a dim of the whole
//   frame marks its layer (GfxHudLayer::dim);
// - after every layered publish the capture is CARRIED: until the next frame starts (a terrain pass, its restore, a window paint
//   or the next layer), what is drawn over the root goes into a centred layer after the published layers, so a modal screen that
//   refreshes outside the frame's layers (the popups, the System menu, hole stats, best scores, end of year, ...) is published
//   with the panels it was drawn over, at their anchors; a carried publish keeps the carry (the screen's later passes are
//   measured from the same frame); a publish after a 2D write covering the whole root publishes no layers;
// - a screen's first drawing call (reports.begin / dialogs.begin) inside an open panel layer closes that layer there, so the
//   screen's drawing is a centred layer of the frame and its refreshFull publishes the frame's layers (the hire dialog).
// Off (the default, and --hud-modal off): P9B's capture.
void gfx_set_hud_modal(Ctx &ctx, bool on);
bool gfx_hud_modal(Ctx &ctx);
// P9E: the popups' place (log/roadmap/P9/P9E_notes.md fix 1), set by the app's host after gfx_set_hud_modal. With it on, showPopup and
// textInputBox (commands_popup.inc DialogMark) call gfx_hud_popup_begin / _end around their body with the input's click point
// (`click` = a mouse button was down, g_rootInput's button word): every root pixel changed while the popup is open is the popup's
// (GfxHudFrame::owner), and the frame publishes the pieces with their opener panel. Records only: nothing is drawn or written into
// the GameState, the root or the back buffer. Off (the default, --hud-popups off): nothing recorded.
void gfx_set_hud_popups(Ctx &ctx, bool on);
// P9E fix 2: the frame that closes a modal screen opened inside a panel layer (the hire dialog, drawEmployeeDetailPanel's
// hireEmployee), set by the app's host after gfx_set_hud_modal. Its publishes inside the panel's scope arm the carry; when the
// same frame opens another layer after the screen closed, P9D ended the carry there and took the root (dimmed, the dialog over it,
// the panels drawn before it at their 800-space places) as the frame's base. With it on, that layer continues the carried
// capture: the frame's base, its layers before the screen, the screen's drawing as a centred layer, then the frame's later layers,
// as the screen's own frames compose. Records only. Off (the default, --hud-close off): P9D.
void gfx_set_hud_close(Ctx &ctx, bool on);
// P9E fix 4 (option B): the tool previews at the cursor. The frame code draws them (P6_ToolLabel's tool label pass and P7's
// placement box, object and flag previews: addSprite -> drawQueuedSprite.draw) after the sprite queue, onto the 800 root and,
// with G32's wide scene, a second time onto the scene the world picture is made from. With this on, tick.cpp brackets those
// two passes (GfxCursorScope) and the backend keeps the wide scene before and after them (LiveTerrainWorld::scene_pre /
// scene_cur), so a presenter can take the previews out of the world picture and draw them where the cursor is
// (LiveTerrain::world_frame cursor_apart, world_cursor). Records only: nothing is drawn, and the frame, the root, the back
// buffer and the GameState are unchanged. Set by the app's host after gfx_set_hud_capture; off (the default): nothing kept.
void gfx_set_hud_cursor(Ctx &ctx, bool on);
// P10I fix 1: the line from the tee to the cursor at the window's resolution. With this on (and gfx_set_hud_cursor's passes kept),
// the two drawMinimapSegment calls of P7's hole preview (ML+0xb9b1: the green tool's tee-to-cursor line; ML+0xbb2a: the tee tool's
// green-to-cursor arm; both colour 0x80007fff, width 2, alpha 7, the only width-2 drawMinimapSegment calls inside a cursor pass)
// are recorded with the root pixels they changed (GfxHudFrame::lines / line800 / line_word), so a presenter can draw the line at
// its own resolution instead of the frame's pixels. Records only: the line is still drawn into the root, and the frame, the back
// buffer and the GameState are unchanged. Set by the app's host after gfx_set_hud_cursor; off (the default): nothing recorded.
void gfx_set_hud_lines(Ctx &ctx, bool on);
// P10C fix 2: with the HUD capture on, a panel layer's pixels are also the root pixels its drawing primitives wrote, even with the
// word the root already held there (P9B's diff alone left those transparent: a tree icon over the course's own tree sprites had
// holes showing the world at the panel's moved place). On by default with the capture; off = P9B's diff only. Records only.
void gfx_set_hud_writes(Ctx &ctx, bool on);
void gfx_cursor_sprites_begin(Ctx &ctx);
void gfx_cursor_sprites_end(Ctx &ctx);
// P22E item 7 (Mariano's ninth play-test: "placing a home site on the edge of the screen makes the area invisible"): the tool
// previews at the cursor whole in a wide window. The frame code draws P7's placement box (four drawMinimapSegment edges) and its
// labels (drawCenteredText2) into the 800 x 600 root, which the root's right / left edge cuts, while the wide scene shows the
// course on past it (the original's screen ends there). With this on (and gfx_set_hud_lines' recording) the box's edges are
// recorded as still lines (GfxHudLine::still) and a cursor pass's text run the root's edge cut gets its whole ink columns
// (GfxTextRun::ex0 / ex1), so a presenter draws both past the frame's columns. Records only: the frame, the root, the back buffer
// and the GameState are unchanged. Set by the app's host (--edge-previews); off (the default): nothing recorded.
void gfx_set_hud_edge(Ctx &ctx, bool on);
// P23F (Mariano's tenth play-test, item 5, the wide audit's rows 5, 6, 10, 11, 16, 24): the same rule for the frame's own texts
// over the course (golfer and walker name labels, placed-object and tile labels, the hole HUD, the g_flags bit 5 numbers): a text
// drawn on the root while it holds the frame's terrain picture, before the frame's first HUD layer, whose ink the root's left or
// right edge cut gets its whole ink columns (GfxTextRun::ex0 / ex1), and one whose whole ink lies past that edge becomes an edge
// run (published with every frame until the next frame's terrain picture). Records only. Set by the app's host (--edge-texts).
void gfx_set_frame_edge_text(Ctx &ctx, bool on);
// P23E (Mariano's tenth play-test, item 5's wide audit rows 3, 2, 4, 17): more lines the frame code draws into the 800 x 600 root
// only, recorded as still lines (GfxHudLine::still) with the root pixels they changed, so a presenter draws them whole past the
// frame's columns at the window's resolution, over the world picture (the world picture itself is not changed). Bits:
// - kGfxWideArc: the player's shot, P7's swing-arc box (mainLoop's cursor pass Ph_00419d06, the swing-arc block after the shot
//   panel's texts: setLinePoint, then per step drawMinimapSegment(.., 0x80000000, width, 7) for the shadow and
//   lineTo(.., 0x80007fff, width, 7) for the arc; the only lineTo calls outside a drawCircle and the only drawMinimapSegment calls
//   of colour 0x80000000 inside a cursor pass).
// - kGfxWideRange: the Place Landmark tool's range circle round the cursor tile (P7's drawCircle(.., 0x80007fff), the only drawCircle
//   inside a cursor pass: its lineTo steps, width 2, alpha 7).
// - kGfxWideRuler: the drag distance ruler (P6_Cursor: four drawMinimapSegment(.., 0x80000000 / 0x80007ff0, 2, 7) from the cursor to
//   the drag's start, after the sprite queue and outside the cursor passes), the lines drawn inside a GfxWideLineScope of that bit.
//   Its yards label (drawCenteredText2, inside the same scope since fix 4b) gets a cursor pass's text treatment (P22E: its whole
//   columns, gfx.cpp draw_text_string), with gfx_set_hud_edge.
// - kGfxWideStaff: the Staff panel's circle round the selected employee (drawEmployeeDetailPanel's drawCircle, gfx_hud.cpp
//   b_detail_panel: its lineTo steps).
// - kGfxWideMarks: the terrain tool's marks round the cursor cell (P4_holeGrid, after the sprite queue: drawMapLine(.., 0x80006090,
//   3, 7) edges and fillWindowRect dots of 0x80006090, inside its GfxWideLineScope); a dot is a still record with `fill` set. Its
//   height numbers are frame texts (the edge-text rule for frame texts, not this record).
// Records only: the frame, the root, the back buffer and the GameState are unchanged. Set by the app's host (--wide-lines) after
// gfx_set_hud_lines; 0 (the default): nothing recorded.
constexpr uint32_t kGfxWideArc = 1u << 0;
constexpr uint32_t kGfxWideRange = 1u << 1;
constexpr uint32_t kGfxWideRuler = 1u << 2;
constexpr uint32_t kGfxWideStaff = 1u << 3;
constexpr uint32_t kGfxWideMarks = 1u << 4;
void gfx_set_hud_wide_lines(Ctx &ctx, uint32_t rows);
// P23E: the frame code's draws of one kGfxWide* row outside the cursor passes (inert without the capture)
void gfx_wide_line_scope(Ctx &ctx, uint32_t row, bool open);
void gfx_hud_popup_begin(Ctx &ctx, int32_t cx, int32_t cy, bool click, bool text_box = false);  // P10C fix 4: text_box
void gfx_hud_popup_end(Ctx &ctx);
// P9D: GfxLayerPx kind 4 (the quarter blend of drawAlpha's draw16_1002ed60, gfx_reports.cpp quarter_blend)
uint16_t gfx_layer_quarter(uint16_t src, uint16_t dst);
bool gfx_hud_capture(Ctx &ctx);
// the layers published at the last Window::refreshFull (false when capture is off or nothing was published)
bool gfx_hud_frame(Ctx &ctx, GfxHudFrame &out);
// the shade tables of GfxLayerPx kind 3: 0 = build16BitColorTable's block 1 (g_color16Lut), 1 = g_halfBrightLut's table
const uint16_t *gfx_shade_table(uint8_t id, size_t &n);
void gfx_layer_begin(Ctx &ctx, int32_t id);
void gfx_layer_end(Ctx &ctx);
// the layer a frame-code callee draws into (tick.cpp), -1 for none
int32_t gfx_layer_of_callee(const char *name);
extern std::atomic<int> g_gfx_hud_capture;  // contexts with capture on (the frame code's fast test)
struct GfxLayerScope {
  Ctx *ctx = nullptr;
  GfxLayerScope(Ctx &c, int32_t id) {
    if (id >= 0 && g_gfx_hud_capture.load(std::memory_order_relaxed) != 0) {
      ctx = &c;
      gfx_layer_begin(c, id);
    }
  }
  ~GfxLayerScope() {
    if (ctx) gfx_layer_end(*ctx);
  }
  GfxLayerScope(const GfxLayerScope &) = delete;
  GfxLayerScope &operator=(const GfxLayerScope &) = delete;
};
// P9E fix 4: the frame-code passes that draw at the cursor tile (inert without the capture)
struct GfxCursorScope {
  Ctx *ctx = nullptr;
  explicit GfxCursorScope(Ctx &c) {
    if (g_gfx_hud_capture.load(std::memory_order_relaxed) != 0) {
      ctx = &c;
      gfx_cursor_sprites_begin(c);
    }
  }
  ~GfxCursorScope() {
    if (ctx) gfx_cursor_sprites_end(*ctx);
  }
  GfxCursorScope(const GfxCursorScope &) = delete;
  GfxCursorScope &operator=(const GfxCursorScope &) = delete;
};
// P23E fix 4 (gfx_wide_line_scope)
struct GfxWideLineScope {
  Ctx *ctx = nullptr;
  uint32_t row = 0;
  GfxWideLineScope(Ctx &c, uint32_t r) : row(r) {
    if (g_gfx_hud_capture.load(std::memory_order_relaxed) != 0) {
      ctx = &c;
      gfx_wide_line_scope(c, row, true);
    }
  }
  ~GfxWideLineScope() {
    if (ctx) gfx_wide_line_scope(*ctx, row, false);
  }
  GfxWideLineScope(const GfxWideLineScope &) = delete;
  GfxWideLineScope &operator=(const GfxWideLineScope &) = delete;
};
inline void gfx_wide_line_scope_close(GfxWideLineScope &s) {  // the scope's end before its block's
  if (s.ctx) gfx_wide_line_scope(*s.ctx, s.row, false);
  s.ctx = nullptr;
}

// P23D fix 3 (app option --wide-bubbles, log/roadmap/P23/P23D_notes.md): RENDER-ONLY. The frame code's comment bubbles drawn after
// the sprite queue (P4_walkerOne's six pieces, mainLoop ML+0x7b09..ML+0x7c3d; P23D fix 4: P4_golferOne's panel pointer ML+0x77ef,
// and its bar drawMapLine ML+0x77b5, drawn in hr as a half-alpha band; P23D fix 5: the thought bubble's two Surface_drawScaled pieces
// ML+0x7640 / ML+0x7754, one mirrored, and its fillWindowRect box ML+0x7778; inside GfxBubbleScope) go into the wide scene's hr
// plane too, at the window's resolution (scene position = root position + scene_e), so a bubble crossing the 800 frame's edge or
// standing in a side column shows whole; the root pixels they wrote are marked (LiveTerrainWorld::bub800 / bub_word), so inside
// the frame the world image shows them (one resolution). Off (the default): the 800 root only.
void gfx_set_wide_bubbles(bool on);
bool gfx_wide_bubbles();
void gfx_bubble_begin(Ctx &c);
void gfx_bubble_end(Ctx &c);
struct GfxBubbleScope {
  Ctx *ctx = nullptr;
  explicit GfxBubbleScope(Ctx &c) {
    if (gfx_wide_bubbles()) {
      ctx = &c;
      gfx_bubble_begin(c);
    }
  }
  ~GfxBubbleScope() {
    if (ctx) gfx_bubble_end(*ctx);
  }
  GfxBubbleScope(const GfxBubbleScope &) = delete;
  GfxBubbleScope &operator=(const GfxBubbleScope &) = delete;
};

// P23D fix 6 (app option --wide-ground, log/roadmap/P23/P23D_notes.md): RENDER-ONLY. The ground marks the frame draws BEFORE the
// sprite queue (P4_records5736b8's three fillWindowRect at mainLoop ML+0x62f0 / ML+0x6350 / ML+0x639a, the golfer-target mark
// Widget_fillRect ML+0x608d; inside GfxGroundScope) are kept as rectangles and, at renderSpriteQueue's start, queued into the wide
// scene's hr plane as its first commands (under every queue sprite, as in the root), with the root pixels they left marked
// (LiveTerrainWorld::bub800 / bub_word) so inside the frame the world image shows them. Off (the default): the 800 root only.
void gfx_set_wide_ground(bool on);
bool gfx_wide_ground();
void gfx_wide_ground_stats(uint64_t &queued, uint64_t &past);   // rectangles queued into hr so far, and of them past the root's edge
void gfx_ground_begin(Ctx &c);
void gfx_ground_end(Ctx &c);
struct GfxGroundScope {
  Ctx *ctx = nullptr;
  explicit GfxGroundScope(Ctx &c) {
    if (gfx_wide_ground()) {
      ctx = &c;
      gfx_ground_begin(c);
    }
  }
  ~GfxGroundScope() {
    if (ctx) gfx_ground_end(*ctx);
  }
  GfxGroundScope(const GfxGroundScope &) = delete;
  GfxGroundScope &operator=(const GfxGroundScope &) = delete;
};

// ------------------------------------------------------------------------------------------------ P9C: text runs
// Text at the window's resolution (log/roadmap/P9/P9C_notes.md). Every text the backend draws reaches the screen through one
// call, the provider's TextMetrics::draw_glyphs (jgld's TextOutA: draw_text_string, the text family's run drawer for the root
// and for a window target). With text capture on (gfx_set_text_capture, set only by the app's host for --text-res native) each
// such call is kept as a RUN: Font::create's arguments of its FontRef, the COLORREF, the TextOutA origin, the bytes, and the
// pixels of the box it changed before and after the call. Runs live on their surface until a later write covers every glyph
// pixel they left. At each Window::refreshFull the runs are PEELED newest first over the back buffer: a box pixel still holding
// the run's own `after` word shows the run (vis), and a visible glyph pixel is put back to the run's `before` word, so `clean` is
// the back buffer without the visible low-resolution glyphs and a presenter can draw the same strings at another size over it,
// limited to each run's visible pixels. With the HUD layers of P9B the same peel is made per layer segment (the layer's pixels
// patched to what the layer held under the glyph: transparent where it had not drawn) and over the layers' base. Nothing is
// written into the GameState, the root surface or the back buffer, and the widths the frame code reads are the providers'
// 800-space ones: the frame every gate sees is the same with capture on or off.
// P12A fix 2: the source of a sprite-queue entry (render-only; the functions are at the end of this file)
enum GfxMoverKind : int8_t {
  kGfxMoverNone = 0,
  kGfxMoverGolfer = 1,    // the golfer loop's figure (index: the golfer record)
  kGfxMoverBall = 2,      // a golfer record's ball in the air (index: the golfer record)
  kGfxMoverShadow = 3,    // that ball's shadow
  kGfxMoverWalker = 4,    // drawWalkers' staff / player figure (index: the walker record)
  kGfxMoverAnimal = 5,    // drawWorldObjects' habitat animal (index: the habitat slot)
  kGfxMoverSky = 6,       // drawWorldObjects' flyers (index: the sky actor record)
  kGfxMoverResident = 7,  // updateVipResident's figure (index: the resident record)
};
struct GfxMoverSrc {
  int8_t kind = kGfxMoverNone;
  int32_t index = -1;
  int32_t wx = 0, wy = 0;  // the object's world position at the draw (1024 per tile), for the plausibility of a move
  // P19A item 7: the exact (unrounded) place minus the drawn one, 800-space pixels (the ball and its shadow; 0 for the others):
  // worldToScreen's projection and the ball's height computed without their integer divisions (bridge.h world_to_screen_exact)
  float fx = 0, fy = 0;
  bool operator==(const GfxMoverSrc &o) const { return kind == o.kind && index == o.index; }
};
struct GfxTextRun {
  uint32_t fontref = 0;                   // the FontRef object (gditext's font table key)
  std::string face;                       // Font::create's face, size (lfHeight = -size) and style bits (gditext.h GameFont)
  int32_t size = 0;
  uint32_t style = 0;
  uint32_t colorref = 0;                  // SetTextColor's COLORREF (r | g << 8 | b << 16)
  int32_t x = 0, y = 0;                   // TextOutA's origin (TA_BASELINE), back-buffer 800-space
  std::string text;                       // the bytes TextOutA drew
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // the box: the pixels the call changed plus a margin (a third of the size, at least 3), in the clip, half-open
  int32_t layer = -2;                     // layered path: -1 the base, k >= 0 GfxHudFrame::layers[k], -2 not shown there,
                                          // -3 a world run (target 2)
  uint8_t target = 0;                     // 0 the root surface, 1 a window-tree child's surface (composed at its position),
                                          // 2 G32's wide scene beside the frame (a world run: layer -3, drawn over the world)
  std::vector<uint8_t> vis;               // (x1 - x0) * (y1 - y0): 1 where the run shows in the back buffer (non-layered path)
  std::vector<uint8_t> vis_layer;         // the same in its layer or the base (layered path; empty unless layer >= -1)
  GfxMoverSrc mover;                      // P12A fix 2: a golfer name label drawn with queue entry k: that entry's source
  // P22E item 7 (gfx_set_hud_edge): a text of a cursor pass (the placement box's name / value labels, the tool label) whose ink the
  // 800 x 600 root's left or right edge cut: the 800-space columns [ex0, ex1) of its whole ink plus the box margin (ex1 > ex0 only
  // then). The columns of that range outside the root (x < 0 or x >= 800, rows y0 .. y1) count as visible, so a presenter of a
  // wide scene draws the whole string there; inside the root vis decides as before
  int32_t ex0 = 0, ex1 = 0;
  static constexpr int32_t kRootW = 800;
  int32_t box_x0() const { return ex1 > ex0 ? std::min(x0, ex0) : x0; }   // the columns a presenter may draw in
  int32_t box_x1() const { return ex1 > ex0 ? std::max(x1, ex1) : x1; }
  uint8_t visible(int32_t px, int32_t py, bool layered) const {
    if (ex1 > ex0 && (px < 0 || px >= kRootW)) return px >= ex0 && px < ex1 && py >= y0 && py < y1 ? 1 : 0;
    if (px < x0 || px >= x1 || py < y0 || py >= y1) return 0;
    const std::vector<uint8_t> &v = layered ? vis_layer : vis;
    const size_t i = static_cast<size_t>(py - y0) * static_cast<size_t>(x1 - x0) + static_cast<size_t>(px - x0);
    return i < v.size() ? v[i] : 0;
  }
};
// a layer pixel the peel changed: index into the layer's px, and what the layer holds without the glyph
struct GfxTextLayerPatch {
  int32_t layer = 0;
  std::vector<uint32_t> idx;
  std::vector<GfxLayerPx> px;
};
struct GfxTextFrame {
  uint64_t serial = 0;
  uint64_t hud_serial = 0;           // the GfxHudFrame published at the same Window::refreshFull (0 = none)
  std::vector<uint16_t> back;        // the back buffer the runs were published with
  std::vector<uint16_t> clean;       // back with every visible glyph pixel put back
  std::vector<uint16_t> base_clean;  // layered: the HUD frame's base the same way (empty = no layers this frame)
  std::vector<GfxTextLayerPatch> patches;  // layered: the layer pixels the peel changed
  std::vector<GfxTextRun> runs;      // draw order (the root's, then the window targets')
  bool empty() const { return runs.empty(); }
};
// the patched-transparent marker of GfxTextLayerPatch (kind 0, k 0xff): the layer drew there (a glyph), so a click there is the
// layer's (PanelRemap coverage), but nothing of it shows once the glyph is removed
constexpr uint8_t kGfxTextCoverMark = 0xff;
// The peel over an image (w x h words, rows top-down): for runs newest first, every box pixel holding the run's `after` word is
// visible (vis[k]) and a visible pixel whose before differs from after is put back to before. before / after are the runs' box
// words in the image's coordinates.
struct GfxTextPeelRun {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const uint16_t *before = nullptr, *after = nullptr;
};
// P10B fix 3 (gfx_set_text_peel_witness, on by default): a box pixel holding the run's `after` word counts as visible only when it is
// within kGfxTextPeelSpread steps (8-connected, through such pixels) of a WITNESS: one of them with a 4-neighbour of them whose
// `after` word differs. A cover drawn over a run in one colour (the white save-name field over a white message line) holds the
// after word of the run's glyph cores by chance; those pixels touch no differing after word that also still shows, so they are
// not peeled (no ghost of the run's background inside the cover) and the run is not drawn there. Off = P9C's per-pixel test.
constexpr int kGfxTextPeelSpread = 3;
void gfx_text_peel(std::vector<uint16_t> &img, int32_t w, int32_t h, const std::vector<GfxTextPeelRun> &runs,
                   std::vector<std::vector<uint8_t>> &vis);
void gfx_set_text_peel_witness(bool on);  // process-wide; the app's main only
void gfx_set_text_capture(Ctx &ctx, bool on);  // the app's host only
bool gfx_text_capture(Ctx &ctx);
// the runs published at the last Window::refreshFull (false when capture is off or nothing was published)
bool gfx_text_frame(Ctx &ctx, GfxTextFrame &out);
// the runs of the last loading.banner picture ($gfx/banner.555; P8A)
bool gfx_text_banner(Ctx &ctx, GfxTextFrame &out);
// counters: runs captured, runs published at the last refresh, runs dropped (no visible pixel left), runs whose font the
// provider's table does not hold (left in 800-space)
struct GfxTextStats {
  uint64_t captured = 0, published = 0, dropped = 0, no_font = 0, frames = 0, merged = 0;  // merged: repeats (gfx.cpp)
  uint64_t edge = 0;  // P22E item 7: cursor-pass runs the root's edge cut, given their whole columns (GfxTextRun::ex0 / ex1)
  uint64_t frame_edge = 0;  // P23F: frame texts the root's edge cut (whole columns) or wholly past it (edge runs)
  uint64_t edge_held = 0;  // P24F fix 1: publishes that left out the edge runs (the root held no terrain picture)
  uint64_t root_covered = 0;  // P24F fix 1: refreshFulls whose root 2D draws had covered in pieces (gfxb::root_covered)
};
GfxTextStats gfx_text_stats(Ctx &ctx);
extern std::atomic<int> g_gfx_text_capture;  // contexts with text capture on

// ---- P12A fix 2 (log/roadmap/P12/P12A_notes.md): the moving objects of the sprite queue, drawn between two game frames by the app.
// RENDER-ONLY: a thread-local side table of the queue's 1024 entries (not GameState), written by enqueueSprite with the source the
// frame code named right before the call (gfx_mover_source), cleared by resetSpriteQueue. Nothing the simulation reads changes.
// the next enqueueSprite's source (this thread); fx / fy: GfxMoverSrc::fx / fy (P19A)
void gfx_mover_source(int8_t kind, int32_t index, int32_t wx, int32_t wy, float fx = 0, float fy = 0);
void gfx_mover_queued(int32_t n);   // enqueueSprite: entry n takes the pending source (none when none was named); clears it
void gfx_mover_reset();             // resetSpriteQueue
// P22C (development): SG_P22_TRAILLOG=<file>: one line per ball trail segment the golfer loop draws (drawMinimapSegment from the
// ball's place to the place kept from the last frame, golfer flag bit 22): time us, golfer, the two ends (800-space)
void gfx_mover_trail_log(int32_t golfer, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
// P22C item 3: true = a ball the golfer loop draws a trail segment for (golfer flag bit 22, a place kept from the last frame) is
// not tagged as a mover, so its ball and shadow stay at the frame's place, the trail's end, as the game draws them. Negative control
// (development): SG_P22_TRAILPIN=0 tags them as before (drawn one game frame late, the trail ahead of the ball).
bool gfx_mover_trail_pin();
// P22C item 3 fix 3 (app option --trail-glide, log/roadmap/P22/P22C_notes.md): RENDER-ONLY. With gfx_set_trail_glide(true)
// (process-wide; the app's main only, with the moving objects on) a ball with a trail is a mover again (gfx_mover_trail_pin is
// false) and its trail segment (drawMinimapSegment at mainLoop ML+0x6249, inside gfx_trail_scope(golfer)) is not drawn on the
// world layer: the line's description is kept for the present (gfx_trail_take) and the window draws it between the base and the
// sprite queue at the ball's drawn offset (gfx_hr_scene_redraw's lines). The 800 x 600 root and terrain800 are as before.
struct GfxTrailLine {
  int32_t golfer = -1;
  int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;  // 800-space ends (the ball now, the ball's place kept from the last frame)
  uint32_t colour = 0;
  int32_t alpha = 0;
  float line_width = 1.0f;                   // the GL line width in force (terrain_overlay.h LineGl)
  bool flip_y = true;
  int32_t screen_h = 600;
  int32_t pw = 0, ph = 0, fw = 0, ox = 0;    // the world layer it was meant for
};
void gfx_set_trail_glide(bool on);
bool gfx_trail_glide();
// P23D fix 1 (app option --wide-grid, log/roadmap/P23/P23D_notes.md): RENDER-ONLY. With gfx_set_wide_grid(true) (process-wide; the
// app's main only) the grid overlay mainLoop draws while the terrain palette's pick is held (P1_grid: Widget_fillRect colour
// 0x80000000 on the root between the frame's terrain picture and the sprite queue, outside cursor passes) is drawn on the world
// layer too, so it covers the wide view's side columns (terrain_overlay.h root_line_world). Off (the default): the 800 root only.
void gfx_set_wide_grid(bool on);
bool gfx_wide_grid();
// the frames with grid lines so far (process-wide): kept = the world picture left as the last frame's (no back restore, no line
// drawn), drawn = restored and drawn, mismatch = a kept frame whose lines or root turned out to differ (then drawn)
void gfx_wide_grid_stats(uint64_t &kept, uint64_t &drawn, uint64_t &mismatch);
// SG_P23_GRID_CHECK=1 (development): kept frames compared with the back put back and their lines drawn again, and the pixels that differ
void gfx_wide_grid_check(uint64_t &checked, uint64_t &differing_px);
void gfx_trail_scope(int32_t golfer);        // the golfer loop: the golfer whose trail segment is drawn next; -1 after it
std::vector<GfxTrailLine> gfx_trail_take();  // the lines kept since the frame's terrain picture began (cleared)
void gfx_trail_reset();                      // the frame's terrain picture begins (the overlay lines' window opens)
// the line moved by (dx, dy) 800-space pixels on a pw x ph surface, as mirror_line_px draws it (the same coverage, blend and
// dither at every pixel), only inside [cx0, cx1) x [cy0, cy1); cover (optional, pw x ph) gets 1 where it wrote. false: nothing
// drawn (another surface size, a line of length 0)
bool gfx_trail_line_px(const GfxTrailLine &l, double dx, double dy, uint16_t *px, int32_t pw, int32_t ph, int32_t cx0, int32_t cy0,
                       int32_t cx1, int32_t cy1, uint8_t *cover);
// the world-pixel box (x0, y0, x1, y1 exclusive) the moved line can write; false: none
bool gfx_trail_line_box(const GfxTrailLine &l, double dx, double dy, int32_t box[4]);
GfxMoverSrc gfx_mover_of(int32_t n);
GfxMoverSrc gfx_mover_label();      // the source of the queue entry whose label is being drawn (none outside drawQueuedSprite.label)

// The window-resolution sprite picture of one queue (the world layer's hr plane, gfx_scene.cpp hr_flush) as the app needs it to
// draw the moving objects elsewhere: the base (the world layer's pixels at the queue's start) and every draw of the queue in its
// order, each with its box in world pixels and its source. Immutable once published; shared with the window thread.
struct GfxHrScene;
// P12A fix 3: the farthest an object is drawn from its frame's place (800-space pixels per game frame): the presenter matches no
// larger step, and the published scene holds the base only that far around each tagged draw
constexpr int32_t kGfxMoverMaxStep = 96;
struct GfxHrDraw {
  GfxMoverSrc src;
  int32_t box[4] = {0, 0, 0, 0};   // x0, y0, x1, y1 (world pixels, half-open)
  int32_t art_w = 0, art_h = 0;    // the art's size (an object whose art size changes is the same art family only when equal)
  double rx = 0, ry = 0;           // the placement (scene pixels: the logical position plus the wide scene's offset)
  double ex = 0, ey = 0;           // the queue entry's own placement (rx / ry minus the art's scaled trim: the same for every cel)
};
void gfx_set_hr_scene(bool on);  // process-wide; the app's main only (off: no scene is kept, the flush is P10D's)
void gfx_hr_scene_cost(int64_t &ns, int64_t &n);  // P12A fix 3: the publications so far (hr_flush's sim-thread time, count)
// the last flushed queue of the context's world layer (null: none, or the hr plane is off); the draws made at once into hr after
// the queue are in it up to this call (a present)
std::shared_ptr<const GfxHrScene> gfx_hr_scene(Ctx &ctx);
const std::vector<GfxHrDraw> &gfx_hr_scene_draws(const GfxHrScene &s);
int32_t gfx_hr_scene_pw(const GfxHrScene &s);
int32_t gfx_hr_scene_ph(const GfxHrScene &s);
// the pixels of the world-pixel rectangle (x0, y0, x1, y1) as the queue drew them over its base, with draw k moved by (dx[k], dy[k])
// 800-space pixels (its placement rx / ry; dx / dy: one per draw, or empty = none moved): out / cov (w * h, rows of x1 - x0) receive
// the hr word and 1 where a draw covered it. Any thread.
void gfx_hr_scene_redraw(const GfxHrScene &s, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const std::vector<double> &dx,
                         const std::vector<double> &dy, std::vector<uint16_t> &out, std::vector<uint8_t> &cov);
// P22C item 3 fix 3: the same with trail lines drawn over the base before the queue's draws, line i moved by (ldx[i], ldy[i])
// 800-space pixels (gfx_trail_line_px); their pixels count as covered
void gfx_hr_scene_redraw(const GfxHrScene &s, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const std::vector<double> &dx,
                         const std::vector<double> &dy, std::vector<uint16_t> &out, std::vector<uint8_t> &cov,
                         const std::vector<GfxTrailLine> &lines, const std::vector<double> &ldx, const std::vector<double> &ldy);

// ---- P15C (log/roadmap/P15/P15C_notes.md): the same redraw on the app's GPU. RENDER-ONLY, any thread, the scene unchanged.
// gfx_hr_scene_redraw's per-pixel work split in two: what the GPU cannot do exactly (the double arithmetic that places the samples:
// hr_blit_rows' column / row tables) is computed here with hr_blit_rows' own expressions; the per-pixel mixing (the ops over the
// pixel under them, the bilinear / sharp sums) is integer and double arithmetic on these values, transcribed to the GPU.
struct GfxHrGpuOp {
  int32_t kind = 0;          // 1 PlaneOp (blt16a), 2 ShadeOp (drawG), 3 AlphaOp (drawAlpha), 4 ColourOp (blt16c), 5 DrawBOp (drawB)
  const uint16_t *pal = nullptr;  // 256 words (the draw's own copy)
  const uint16_t *lut = nullptr;  // ShadeOp: the shade table and its size
  uint32_t lut_n = 0;
  uint32_t plane_k = 0;      // PlaneOp: added to (payload - 0xf8) (1 under the gfx_scene control's mode 1, else 0)
  uint32_t flag = 0;         // AlphaOp: payloads 0xf8..0xfe skipped too
  uint32_t colour = 0;       // ColourOp
  std::vector<uint32_t> steps;  // AlphaOp: its program, 4 words per step (op, dst, src kind, src)
  const uint8_t *art = nullptr;  // the payloads the draw samples (the edge filter's scaled art where it applies), art_w x art_h
  int32_t art_w = 0, art_h = 0;
};
// false: draw k is not one the GPU path can draw (an op it does not know, a test-only control mode on)
bool gfx_hr_scene_gpu_op(const GfxHrScene &s, size_t k, GfxHrGpuOp &out);
// draw k moved by (dx, dy) 800-space pixels, clipped to [cx0, cx1) x [cy0, cy1): its box (world pixels; the part inside the clip in
// x0..y1), the samples per axis, the final scale (1 / (65536 * samples), the double hr_blit_rows rounds with), the filter (0
// bilinear, 1 sharp, 2 sharp-area), and per column X in [x0, x1) and sample si the texel columns c0, c1 (-1 outside the art) and the
// weights w1, s1 of c1 (cols[4 * ((X - x0) * nsx + si) + 0..3]); per row Y in [y0, y1) and sample sj the art rows j0, j1 (-1 outside)
// and the weights wv, sv (rows[4 * ((Y - y0) * nsy + sj) + 0..3]). false: nothing to draw there.
struct GfxHrTables {
  int32_t box[4] = {0, 0, 0, 0};  // the moved box (hr_box)
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  int32_t nsx = 1, nsy = 1, filter = 0;
  double inv = 0;
  std::vector<int32_t> cols, rows;
};
bool gfx_hr_scene_tables(const GfxHrScene &s, size_t k, double dx, double dy, int32_t cx0, int32_t cy0, int32_t cx1, int32_t cy1,
                         GfxHrTables &out);
// the box draw k has when moved by (dx, dy) (hr_box; the frame's box when both are 0)
void gfx_hr_scene_box(const GfxHrScene &s, size_t k, double dx, double dy, int32_t box[4]);
// the base plane (pw x ph words, only the 32 x 32 tiles whose flag is 1 hold data; tiles tx x ty)
const uint16_t *gfx_hr_scene_base(const GfxHrScene &s, const uint8_t *&tiles, int32_t &tx, int32_t &ty);
constexpr int32_t kGfxHrTile = 32;
// PlaneOp's table (5 * 0x8000 words: index (k << 15) | word, k = payload - 0xf8 + plane_k; an index past it leaves the word)
const uint16_t *gfx_hr_plane_table(size_t &n);
// P15A (log/roadmap/P15/P15A_notes.md): the world thread. With gfx_set_hr_defer(true) (process-wide; the app's LiveSession only)
// the hr work is recorded on the frame thread instead of run there: renderSpriteQueue's start (hr_queue_begin over a copy of the
// world root, or the reset of a queue without an hr plane), its end (hr_flush of the queue's commands) and the draws after it
// (hr_now / hr_box_now, each with its own copy of the art, the palette and the op) become ops of a GfxHrJob, in their order. The
// frame thread keeps the flags those ops would set (hr_valid, hrc_valid, hrb_valid), which its drawing reads; the hr planes are
// the world thread's. gfx_hr_take_job (frame thread) hands over the ops recorded since the last take; gfx_hr_job_run (one other
// thread, always the same) runs them on that thread's LiveTerrainWorld, whose hr planes and tile reuse they keep, and
// gfx_hr_scene_take gives that thread's publication. gfx_hr_job_merge appends `next` to `into` and drops every op before the last
// queue start: a queue start and its flush give every hr pixel from the start's base and the queue's commands (hr_flush's tile
// reuse compares with the last queue the thread ran, and the post-queue draws it ran mark their tiles), so a job whose ops were
// dropped leaves the planes as running them would have before the next start's flush.
struct GfxHrJob;
struct LiveTerrainWorld;
void gfx_set_hr_defer(bool on);
// the A/B check (SG_P15_CHECK, development): the frame thread records the ops AND runs them on its own world as without the world
// thread, so the host can compare the two world images of each present
void gfx_set_hr_defer_check(bool on);
bool gfx_hr_check_on();
bool gfx_hr_defer();
std::shared_ptr<GfxHrJob> gfx_hr_take_job();  // never null
void gfx_hr_job_merge(std::shared_ptr<GfxHrJob> &into, const std::shared_ptr<GfxHrJob> &next);
void gfx_hr_job_run(GfxHrJob &j, LiveTerrainWorld &w);
size_t gfx_hr_job_ops(const GfxHrJob &j);
// the world root copied at the job's last queue start (null: none) and the root's change count then (LiveTerrainWorld::gen)
std::shared_ptr<const std::vector<uint16_t>> gfx_hr_job_root(const GfxHrJob &j, uint64_t &gen);
std::shared_ptr<const GfxHrScene> gfx_hr_scene_take();  // gfx_hr_scene of the calling thread, without a Ctx
// an op appended to j: gfx_hr_scene_take into *slot (a present's take, in the ops' order)
void gfx_hr_job_add_take(GfxHrJob &j, std::shared_ptr<std::shared_ptr<const GfxHrScene>> slot);
// P15A fix 3 (log/roadmap/P15/P15A_notes.md section 5): THE WORLD RASTER ON THE WORLD THREAD. With gfx_set_world_raster_defer(true)
// (process-wide, set before the world layer is made; the app's LiveSession with the world thread only) the world layer's surface
// work is recorded too, as ops of the same job in their order with the hr ops: the GL stream of the world's setup and passes
// (kGwGl: a gl::GlRec, gl_raster.h), the root filled with 0 (kGwFill), the back made from the root (kGwBlit1) and the root from the
// back (kGwBlit0), an overlay line (kGwLines), a new rasteriser or size (kGwReset: root and back made new), the rest (kGwState).
// The world thread replays them on its LiveTerrainWorld's raster and back, the whole surfaces; a queue start (kGwStart) there reads
// that raster's pixels. The frame thread's own raster draws only what b_queue_begin samples (terrain_live.h). A queue start drops
// only the hr ops before it (kGwHr, kGwStart); gfx_hr_job_merge likewise keeps every world op of `into`. gfx_hr_job_run runs every
// op except surface work no later op reads: walking back from the job's end, where the root and the back are both read (the
// present's world image, the next job), a fill, a copy or a line whose target is overwritten before it is read is skipped, and a
// GL stream that draws while the root is not read afterwards is replayed with GlRaster::set_discard (its state calls kept). What
// is skipped is never read, so the root and the back each op leaves for a reader are the ones running every op gives.
enum GfxWorldOp : uint8_t { kGwHr = 0, kGwStart, kGwGl, kGwFill, kGwBlit1, kGwBlit0, kGwLines, kGwReset, kGwState };
void gfx_set_world_raster_defer(bool on);
bool gfx_world_raster_defer();
// frame thread: an op appended to the job being recorded
void gfx_world_op(GfxWorldOp kind, std::function<void(LiveTerrainWorld &)> f);
// (gfx_world_gl_chunk, the GL stream at the job's end, is declared in terrain_live.h: gfx.h names no gl:: type)
// the counters of gfx_hr_job_run's world ops (the thread that runs the jobs writes them; read them after it stopped)
struct GfxWorldRun {
  int64_t gl_live = 0, gl_discarded = 0, gl_state = 0, fills = 0, fills_skipped = 0, blits = 0, blits_skipped = 0, lines = 0,
          lines_skipped = 0, resets = 0, states = 0, no_raster = 0;
  int64_t gl_live_ns = 0;   // the live GL streams' replays, their pixel work included (GlRaster::finish)
};
const GfxWorldRun &gfx_world_run_stats();

// ---- P17B (log/roadmap/P17/P17B_notes.md): the world thread's hr planes on the app's GPU. RENDER-ONLY. With a backend installed on
// the thread that runs the jobs (gfx_set_hr_gpu), hr_queue_begin / hr_flush / hr_now / hr_box_now keep hr, hr_cov, hrc, hrc_cov,
// hrb_lo, hrb_hi and hrb_cov in the backend instead of the LiveTerrainWorld's vectors (LiveTerrainWorld::hr_gpu says where they are);
// everything that decides WHICH pixels are drawn (the compare with the last queue's base, the dirty tiles, the commands' boxes, the
// sample tables of hr_blit_rows' double arithmetic) stays here, and the backend runs hr_blit_rows' per-pixel work (the ops and the sums,
// as P15C's GfxHrGpuOp). A backend call that fails drops it for the thread: the planes are made again on the CPU at the next queue
// start (no reuse), and LiveTerrainWorld::hr_lost says the planes hold no picture until that queue's flush.
// One command as the backend draws it: hr_blit_rows' sample tables over its whole box (GfxHrTables' layout: cols / rows hold 4 words per
// entry, from col_off / row_off of the batch's arrays), its op and art.
struct GfxHrGpuRec {
  int32_t op = 0;                   // index into the batch's ops
  int32_t nsx = 1, nsy = 1, filter = 0;
  double inv = 0;
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;   // the box (world pixels, half-open, inside the world)
  uint32_t col_off = 0, row_off = 0;        // in int4 entries
};
struct GfxHrGpuBatch {
  std::vector<GfxHrGpuOp> ops;
  std::vector<GfxHrGpuRec> recs;
  std::vector<int32_t> cols, rows;
  void clear() {
    ops.clear();
    recs.clear();
    cols.clear();
    rows.clear();
  }
};
enum GfxHrPlane : int { kHrPlaneHr = 0, kHrPlaneHrc = 1, kHrPlaneBox = 2 };
class GfxHrGpu {
 public:
  virtual ~GfxHrGpu() = default;
  // the backend wants planes of pw x ph (--gpu-world auto: the size limit)
  virtual bool want(int32_t pw, int32_t ph) = 0;
  // planes of pw x ph exist
  virtual bool planes(int32_t pw, int32_t ph) const = 0;
  // hr = 0, hr_cov = 0 over pw x ph (made new); hrc / hrb dropped
  virtual bool reset(int32_t pw, int32_t ph) = 0;
  // the base (the queue start's root = hr_queue_begin's last_base): `all` = every word of root, else the row spans (y, x0, x1) triples
  // copied into last_base; w = the thread's world (its raster may hold the root as a GPU resource)
  virtual bool base(const LiveTerrainWorld &w, const std::vector<uint16_t> &root, const std::vector<int32_t> &spans, bool all) = 0;
  // hr_flush's drawing: every dirty tile (tx x ty flags, kGfxHrTile) reset to the base (hr) and 0 (hr_cov), then the batch's commands
  // in order over the tile's pixels inside each command's box
  virtual bool flush(const std::vector<uint8_t> &dirty, int32_t tx, int32_t ty, const GfxHrGpuBatch &b) = 0;
  // hrc = hr, hrc_cov = 0 (hr_now's first cursor-pass draw)
  virtual bool make_hrc() = 0;
  // the box planes: make = hrb_lo 0, hrb_hi 0x7fff, hrb_cov 0 over pw x ph; else only the rectangle area (x0, y0, x1, y1)
  virtual bool reset_box(bool make, const int32_t area[4]) = 0;
  // one command (b.recs[0]) over its box: plane kHrPlaneHr (cover hr_cov), kHrPlaneHrc (hrc_cov) or kHrPlaneBox (hrc, hrb_lo, hrb_hi
  // in that order, cover hrb_cov)
  virtual bool now(int plane, const GfxHrGpuBatch &b) = 0;
  // the planes read back (pw x ph each; a null pointer = not read)
  virtual bool read(std::vector<uint16_t> *hr, std::vector<uint8_t> *hr_cov, std::vector<uint16_t> *hrc, std::vector<uint8_t> *hrc_cov,
                    std::vector<uint16_t> *hrb_lo, std::vector<uint16_t> *hrb_hi, std::vector<uint8_t> *hrb_cov) = 0;
  virtual void dead(const char *why) = 0;   // a failure seen by the caller (the backend is not used again)
  // fix 3: hr_queue_begin's compare on the GPU. The root (`root`, or with null w's raster: its GPU frame when current, else its CPU
  // words) against the base: per row y whose words differ, the span (y, x0, x1) of its first and last differing word (x1 exclusive:
  // hr_queue_begin's loop), then the base = the root. 1 done, -1 failed.
  virtual int compare(const LiveTerrainWorld &w, const std::vector<uint16_t> *root, std::vector<int32_t> &spans) = 0;
  // fix 3: the base's 32 x 32 tiles flagged in `tiles` (tx x ty) into dst (pw x ph words; the other words untouched)
  virtual bool read_base(const std::vector<uint8_t> &tiles, int32_t tx, int32_t ty, uint16_t *dst) = 0;
};
// the calling thread's backend (null: none; the world thread's host installs it before gfx_hr_job_run)
void gfx_set_hr_gpu(GfxHrGpu *g);
GfxHrGpu *gfx_hr_gpu();
// SG_P17_WORLDCHECK=<n> (development): the CPU planes are kept beside the backend's on the job thread, and every n-th flush / now-draw
// the backend's planes are read back and compared word by word (counts below; stderr `P17 WORLDCHECK` at the first difference)
struct GfxHrGpuCheck {
  int64_t flushes = 0, nows = 0, compared = 0, differing = 0, differing_words = 0;
  int64_t failures = 0, lost_jobs = 0, batch_cmds = 0, table_entries = 0, base_all = 0, base_spans = 0, dirty_tiles = 0;
  int64_t prep_ns = 0;   // the commands' tables and ops on the CPU (flush and now)
  // fix 3: queue starts compared on the GPU, of them checked against the CPU loop (SG_P17_WORLDCHECK) / differing; publications whose
  // base tiles came from the GPU, of them checked / differing
  int64_t starts_gpu = 0, start_checks = 0, start_differing = 0, pubs_gpu = 0, pub_checks = 0, pub_differing = 0;
};
const GfxHrGpuCheck &gfx_hr_gpu_stats();
// the START-UP SELF-TEST: a fixed synthetic world (256 x 192) drawn with and without `g` (two queues with the five ops, the filters,
// shrunk and Scale2x art, a changed root and moved commands for the tile reuse, draws into hr and hrc, a placement box), every plane
// compared word by word. Runs on the calling thread (its queue state is kept aside and put back). result: a one-line verdict.
bool gfx_hr_gpu_selftest(GfxHrGpu &g, std::string &result, int64_t &words, int64_t &differing);

}  // namespace portsim

#endif
