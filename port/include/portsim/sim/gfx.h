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

#include <atomic>
#include <cstdint>
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
  kGfxLayerCount = 5,
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
struct GfxHudLine {
  int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
  uint32_t colour = 0;
  float width = 1.0f;
  int32_t alpha = 10;
};
struct GfxHudFrame {
  std::vector<uint16_t> base;  // the root before the first layer opened (800 x 600); empty = no layer this frame
  std::vector<uint16_t> back;  // the back buffer the layers were published with (a present checks it is the frame it shows)
  std::vector<GfxHudLayer> layers;  // in draw order
  uint64_t serial = 0;
  // P9D: why the publish made no layers (0 = it made them; 1 = no layer opened since the last publish; 2 = inside an open layer)
  int8_t none = 0;
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
  uint8_t visible(int32_t px, int32_t py, bool layered) const {
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
};
GfxTextStats gfx_text_stats(Ctx &ctx);
extern std::atomic<int> g_gfx_text_capture;  // contexts with text capture on

}  // namespace portsim

#endif
