// Presenting the port's RGB555 back buffer in the host window (roadmap S7.3).
//
// The port's graphics backend (portsim/sim/gfx.h) keeps jgld's back buffer: 800x600 RGB555 words, rows top-down. The host
// scales it to its client area with the mapping the input translator inverts (input_map.h Viewport, Translator::to_logical):
// the 4:3 content rectangle is the largest that fits the client, centred (black bars where the aspect differs), and client
// pixel (x, y) inside it shows logical pixel (floor((x - ox) * 800 / w), floor((y - oy) * 600 / h)). A click on a client pixel
// therefore reaches the logical pixel that pixel shows.
//
// Colour: each 5-bit channel v becomes the 8-bit (v << 3) | (v >> 2), the conversion the pinned reference frames use
// (re/frida/scenario_ab.py checkpoint_frame, re/tools/imgcmp.py rgb555_to_rgb), so a presented pixel equals the original's
// checkpoint pixel when the back buffers are equal. The scaled image is a 32-bit top-down DIB drawn 1:1
// (SetDIBitsToDevice): no GDI stretching decides a pixel.
#ifndef PORTAPP_PRESENT_H
#define PORTAPP_PRESENT_H

#include <array>  // P12A fix 2
#include <cstdint>
#include <map>  // P19A (objects_start)
#include <memory>
#include <mutex>
#include <string>  // P14A
#include <vector>

#include "input_map.h"
#include "profile.h"  // P14A: ProfLine (overlay_freeze)
#include "portsim/sim/gfx.h"  // P9B: GfxHudFrame (the frame's HUD pixel layers)
#include "textres.h"           // P9C: TextRaster (the frame's text runs at the window's resolution)

namespace portapp {

// Deliberately wrong bodies for the negative controls (`--negctl present-*`; production code uses None).
enum class PresentDefect : uint8_t {
  None = 0,
  Stale = 1,      // the first frame set stays on screen; later frames are ignored
  ByteSwap = 2,   // each RGB555 word's two bytes exchanged before conversion
  WorldMaskIgnored = 3,  // P8B: the world layer shows over the whole content rectangle (the frame's mask not read)
  HudUnmoved = 4,        // G32: the HUD compositor off (panels stay centred, no remap)
  LayerBlendOpaque = 5,  // P9B: a layer's blended pixels drawn as the words they had in the frame (no re-blend at the new place)
  LayerRectMove = 6,     // P9B: the layers ignored, P8B's rectangle compositor used although the frame has layers
  DimLocal = 7,          // P9D: a whole-frame dim layer composed per pixel at its own place only (the window not dimmed under it)
  StaleRemap = 8,        // P26C item 4a: a frame without the world or layers keeps the previous frame's click remap
  HoverUnconfirmed = 9,  // P27C fix 1: hover_confirm ignored (a predicted hover look stays where the game's pass kept its picture)
};

// The content rectangle for a client area of cw x ch.
Viewport fit_viewport(int32_t cw, int32_t ch);

// 8-bit channels of an RGB555 word
inline uint32_t rgb555_to_bgra(uint16_t v) {
  const uint32_t r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
  const uint32_t r8 = (r << 3) | (r >> 2), g8 = (g << 3) | (g >> 2), b8 = (b << 3) | (b >> 2);
  return 0xff000000u | (r8 << 16) | (g8 << 8) | b8;
}

// P5B item 4: how the 800x600 frame is scaled to the content rectangle. Nearest = S7.3's mapping (every client pixel shows
// exactly one logical pixel; the reference the app tests compare with). Smooth = bilinear over the 8-bit channels: client pixel
// centre (x + 0.5) * 800 / w - 0.5 (clamped to the frame) interpolated between its four neighbours, 8-bit weights. Auto =
// Nearest when the content rectangle is an integer multiple of 800x600 (1x, 2x, ...: sharp pixels, no blur), else Smooth.
enum class ScaleMode : uint8_t { Nearest = 0, Smooth = 1, Auto = 2 };

// P7 (widescreen, stage 1; log/roadmap/P7/PLAN.md). The game still draws 800x600; on a client wider than 4:3 the content
// rectangle is unchanged (fit_viewport: full height, centred, so the input mapping is unchanged) and the side bands, black
// before P7, can show the frame's own edge colours carried outward. BandFill is the host's per-frame hint (LiveSession: Extend
// for the full-screen 2D screens before a game's frame loop runs, Black once it runs); WideFill is the app option
// (`--wide-fill`): Off = black bands always (the pre-P7 image, byte for byte), Soft = each band column takes the frame's edge
// colour (the mean of its outer 4 logical columns, per row) blurred vertically by a radius that grows with the distance from
// the frame (d / 2 client pixels, at most 48 logical rows), Clamp = the frame's outermost column repeated, no blur.
// Rows of a letterbox (a client taller than 4:3) stay black.
// P9A item 5 (log/roadmap/P9/P9A_notes.md; the play-test: a shape cut by the frame's edge, the Jamaica plate's pin shadow, was
// carried across the whole band as a streak): Edge = Soft's edge colour (the per-row mean of the outer 4 logical columns,
// blurred vertically by d / 2) only over the band's first 24 logical columns, faded (smoothstep) into the screen's BACKGROUND:
// per row the per-channel median of the outer 64 logical columns, blurred vertically over +-150 logical rows. A shape narrower
// than half of those columns (a pin, a shadow, a label cut by the edge) is not in the background, so nothing is carried farther
// than the fade. Mirror = the frame's outer columns mirrored into the band, blurred vertically by d / 3 and darkened towards the
// band's outer edge (to 40 %). The default since P9A is Edge (--wide-fill edge; SG_APP_P9=off gives P7's Soft).
// P13B item 18: ExtendClamp = Extend with the world map's own rule (its edge columns repeated: fill_bands' map clamp)
enum class BandFill : uint8_t { Black = 0, Extend = 1, ExtendClamp = 2 };
enum class WideFill : uint8_t { Off = 0, Soft = 1, Clamp = 2, Edge = 3, Mirror = 4 };

// P8B (widescreen stage 2, log/roadmap/P8/P8B_notes.md): the world layer of a frame (portsim::LiveTerrain::world_frame). px holds
// pw x ph RGB555 words (rows top-down); the 800 x 600 frame covers world columns ox .. ox + fw - 1 (fw = ph * 800 / 600) and every
// row; mask (800 x 600) is 1 where the frame's pixel shows the world. Empty px = no world (the frame is shown as before P8B).
// G32 (widescreen stage 3, the HUD compositor of log/roadmap/P7/PLAN.md section 3; log/roadmap/P8/P8B_notes.md section 7): the
// game keeps drawing its HUD into the 800 x 600 frame and testing clicks in 800-space. Each panel rectangle below (800-space,
// half-open) has an anchor; in a client wider than the 4:3 content rectangle the frame's 2D pixels inside it (WorldImage mask 0)
// are moved by the anchor's distance to the window's edge (left: -origin_x client pixels, right: + the right band's width), the
// pixels they leave show the world image, and PanelRemap maps a point over a moved panel pixel back to the 800-space point. The
// panels keep their size relative to the window (everything is scaled by window height / 600).
struct HudPanel {
  int32_t x0, y0, x1, y1;
  int8_t anchor;  // -1 left edge, +1 right edge
  const char *name;
};
constexpr HudPanel kHudPanels[] = {
    {0, 0, 250, 95, -1, "title: logo, course name and date (top left)"},
    {630, 0, 800, 145, 1, "meters: money, happiness, rating (top right)"},
    {0, 425, 800, 600, -1, "bottom band: radial menu, System button, the palette / golfer / employee / building panels"},
};

// G32: the client-to-client point remap of the moved panels, shared by the window thread (Presenter::set_frame_world updates it;
// AppWindow remaps mouse messages) and the sim thread (LiveSession's cursor answers)
// P9B: with the frame's HUD layers the remap goes by layer coverage: the topmost layer that has a pixel under the client point
// (each layer at its own place: a moved panel's layer at its anchor, a centred layer at the frame's) decides; a moved layer maps
// the point back to the 800-space place its pixel came from, a centred layer leaves it as it is.
struct RemapLayer {
  int8_t anchor = 0;
  bool fixed = false;  // P9E: the layer (a popup's pixels) sits at `dx` client pixels instead of its anchor's offset
  int32_t dx = 0;
  // P22D fix 2: client pixels added to a point the layer maps (its GfxHudLayer::shift at the window's scale): the game sees the
  // golfer list's added columns at their own 800-space place right of the root
  int32_t vx = 0;
  bool piece = false;  // P27A fix 2: a moved popup piece's pixels (Presenter::layer_remap)
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  std::vector<uint8_t> cover;  // (x1 - x0) * (y1 - y0): 1 where the layer drew
  // P12A fix 1: 1 where a centred layer's pixel is part of the course picture the camera slide moves (the tool previews at the
  // cursor tile and the tee line: Presenter::compose_layers draws them at the slide's offset); empty = none
  std::vector<uint8_t> slides;
};
class PanelRemap {
 public:
  void set(bool on, const Viewport &vp, int32_t cw, const std::vector<uint8_t> &mask);
  void set_layers(bool on, const Viewport &vp, int32_t cw, std::vector<RemapLayer> &&layers);  // P9B
  void clear();
  // the client point the game sees for client point p: p - dx when p lies over a moved panel's 2D pixel, else p
  void map(int32_t &x, int32_t &y) const;
  // P9E fix 3: the same, and `vacated` = the point was left as it is (no moved layer has a pixel under it) while its 800-space
  // point lies inside a moved layer's box (or, without layers, a moved kHudPanels rectangle): the client shows the course
  // there, but the game's own hit tests would see that panel at the point
  void map(int32_t &x, int32_t &y, bool &vacated) const;
  // P10E: map without counting it in remapped() (the window thread's cursor reads between game frames)
  void peek(int32_t &x, int32_t &y) const;
  uint64_t remapped() const;
  // P9E fix 4: the cursor answer the sim got last (LiveSession: the system cursor's or the last mouse message's point): the
  // client offset the remap moved it by (0 = not over a moved layer) and its 800-space point
  void set_cursor_answer(int32_t dx, int32_t lx, int32_t ly) const;
  int32_t cursor_dx() const;
  void cursor_answer(int32_t &lx, int32_t &ly) const;  // P10E: that answer's 800-space point (the SG_P10E_LOG check)
  // P12A fix 1 (Presenter::slide_tick), client pixels. (pic_dx, pic_dy): the camera slide's offset of the course picture the window
  // shows now. (game_dx, game_dy): that offset minus the shift of the camera the game's next cursor reads project with
  // (CameraSnap::next_*, after this frame's key / edge-scroll move) against the picture's camera. A point the layered remap leaves
  // over the course (no layer pixel under it, or a centred pixel the slide moves: RemapLayer::slides) is moved by minus the game
  // offset in `map` (so the game's screenToTile gives the tile the shown picture has under the point) and by minus the picture
  // offset in `peek` (the shown frame's own coordinates: P10E's live preview); `map_noslide` (H1's edge-scroll read: the window's
  // own edges) moves nothing. `slid_*` receive the move applied.
  void set_slide(int32_t pic_dx, int32_t pic_dy, int32_t game_dx, int32_t game_dy) const;
  void map(int32_t &x, int32_t &y, bool &vacated, int32_t &slid_x, int32_t &slid_y) const;
  void map_noslide(int32_t &x, int32_t &y) const;
  void peek(int32_t &x, int32_t &y, int32_t &slid_x, int32_t &slid_y) const;
  uint64_t slid() const;  // points moved by `map` (session.json)
  // P27A fix 1: client point (x, y) is a vacated place (`map`'s vacated: no moved layer pixel under it, its 800-space point inside a
  // moved layer's box): the window shows the course there while the game's tests would see the moved panel. Counts nothing.
  bool vacated(int32_t x, int32_t y) const;
  // P27A fix 2: client point (x, y) shows no layer pixel (moved or centred) and its 800-space point lies inside the box of a moved
  // popup piece (RemapLayer::piece: at its opener's anchor, or the centred text box): the window shows what is behind the popup
  // there, while the popup's modal loop would read the point as one of its own. Counts nothing.
  bool piece_vacated(int32_t x, int32_t y) const;

 private:
  // P10E; P12A: slide 0 = none, 1 = the game offset, 2 = the picture offset
  void map_impl(int32_t &x, int32_t &y, bool &vacated, bool count, int slide = 0, int32_t *sx = nullptr, int32_t *sy = nullptr) const;
  mutable int32_t slide_dx_ = 0, slide_dy_ = 0, game_dx_ = 0, game_dy_ = 0;  // P12A fix 1
  mutable uint64_t slid_ = 0;                    // P12A fix 1
  mutable std::mutex mu_;
  bool on_ = false;
  Viewport vp_;
  int32_t cw_ = 0;
  std::vector<uint8_t> mask_;
  mutable uint64_t remapped_ = 0;
  mutable int32_t cursor_dx_ = 0, cursor_lx_ = 0, cursor_ly_ = 0;  // P9E fix 4
  bool layered_ = false;               // P9B
  std::vector<RemapLayer> layers_;     // P9B, in draw order
};

// P10E (log/roadmap/P10/P10E_notes.md): what the window thread needs to find the tile under a cursor point between game frames
// the way the frame did (P6_Cursor: screenToTile of the cursor answer into g_cursorTileX / Y, useWorld 0), read from the
// GameState at the frame's present: screenToTile's inputs and tileToScreen's answer for every tile of the 50 x 50 grid (the
// cache g_tileScreenX / Y where it holds a point, else worldToScreen of the tile's centre and tileToScreen's window test; nothing
// is written to the state). ok = taken (the frame has tool previews at the cursor, unk_005a9cbc / unk_005a9cc0 are 0 and the
// frame's cursor tile is on screen).
struct CursorProj {
  bool ok = false;
  int32_t tile_x = 0, tile_y = 0;  // the frame's g_cursorTileX / Y
  int32_t sw = 0, sh = 0, rot = 0, cam_x = 0, cam_y = 0, step = 0;  // g_screenWidth / Height, g_cameraRot, gCameraTileX / Y, g_tileStep4c2840
  int32_t dir_dx[8] = {}, dir_dy[8] = {};                           // f_tab_dirDx_4c2878 / f_tab_dirDy_4c2898
  std::vector<int32_t> sx, sy;  // tileToScreen's point per tile (cell tx * 50 + ty)
  std::vector<uint8_t> on;      // tileToScreen's result per tile (1 = on screen, the point written)
};
// screenToTile(px, py, useWorld 0) on the snapshot: the rotation's inverse, then the descent over the 8 neighbours by
// distance(px - sx, (py - sy) * 2). False when the descent starts on a tile tileToScreen gives no point for, or does not settle.
bool proj_tile(const CursorProj &p, int32_t px, int32_t py, int32_t &tx, int32_t &ty);

// P12A fix 1 (log/roadmap/P12/P12A_notes.md): the camera the frame's course picture was drawn with, read from the GameState at the
// present (sim thread, read only). updateTerrainRender (terrain_dll.cpp) leaves the camera tile it drew in g_terrainLastCamX / Y,
// the zoom in g_terrainLastZoom and the rotation in g_terrain820f30[2]; the frame's sprites are projected with the same camera
// (mainLoop writes the camera only after renderSpriteQueue, the keys and edge scroll right before the present). ok = the frame has a
// world layer and HUD layers, the rotation is 0 / 2 / 4 / 6 and equals g_cameraRot, g_screenWidth is 800 and the doubling words
// unk_005a9cbc / unk_005a9cc0 are 0. t_ns: the present's time (perf_now_ns), for the game frame period.
// next_*: gCameraTileX / Y, g_zoomScale, g_cameraRot at the present, the camera the next frame body's cursor reads (P6_Cursor's
// screenToTile, ML+0xa45a) project with: mainLoop's key cases (ML+0xe971..ML+0xea73) and edge scroll (ML+0xec4f..ML+0xec89) move the
// camera after this frame's picture was drawn and before its present (Window::refreshFull ML+0xecdb).
struct CameraSnap {
  bool ok = false;
  int32_t cam_x = 0, cam_y = 0, zoom = 0, rot = 0;
  int32_t next_x = 0, next_y = 0, next_zoom = 0, next_rot = 0;
  int64_t t_ns = 0;
  int64_t head_ns = 0;  // P26A: the frame head's time (perf_now_ns) of the frame that drew the picture
};

// P26A (Mariano's play-test 13 items 1 / 2, log/roadmap/P26/P26A_notes.md; app option --arrow-glide, `play` only): the arrows
// move the camera at one constant speed from the key state (key-down / key-up, Windows' repeats ignored) instead of one 4-tile step
// per key-down and one per frame after a hold delay. The glide camera D (tiles, fractional) moves at v tiles / s on each camera
// axis the held arrows move (the key switch's directions plus the rotation: tick_frame.inc P21_ArrowStep), clamped to 0..0x31 per
// axis as the game clamps. The window shows the course picture offset by D - the picture's camera (CameraSnap::cam_*), and D never
// runs past the newest picture's camera (no undrawn ground ahead), so the move starts on the first present with a moved picture.
// At each frame head the live host asks the game to step to the whole tiles D will reach by the time the next picture is due
// (head; bridge.h kPortViewGlideXVa), so the pictures stay ahead of D. On the key-up each axis runs on to its next whole tile and
// at least one key-switch step (4, 2 at zoom 8) from the last direction change (a tap moves one step, as in the original); then
// the host steps the game back to D if the last request went past it. At rest D follows the picture's camera (other camera moves).
// Thread safe: keys from the window thread (LiveSession::on_input), D from the presenter's ticks (window thread), requests from
// the sim thread's frame heads.
class ArrowGlide {
 public:
  // an arrow key (VK_LEFT .. VK_DOWN) went down / up at t_ns (perf_now_ns); returns true when it starts a glide from rest
  bool key(uint32_t vk, bool down, int64_t t_ns);
  void release_all(int64_t t_ns);  // the window lost the focus
  // the presenter, at a tick: D advanced to now_ns for the shown picture's camera (cx, cy) and rotation; (ox, oy) = D - (cx, cy)
  // in tiles; false = nothing to show (D at rest on the picture's camera, or not engaged)
  bool tick(int64_t now_ns, int32_t cx, int32_t cy, int32_t rot, double &ox, double &oy);
  // the host, at a frame head in the frame loop: the speed (tiles / s per axis for one arrow) and the key switch's step (tiles)
  // for the zoom, the game's camera (gCameraTileX / Y) and rotation now; the step (dx, dy) to ask for (false: none)
  bool head(double v, double step, int32_t cx, int32_t cy, int32_t rot, int32_t &dx, int32_t &dy);
  void set_engaged(bool on);  // the host: the frame loop runs and the view block is mapped (false: D at rest)
  bool engaged() const;
  void set_lead_s(double s);  // the presenter: the time from a frame head to its picture's arrival plus one period
  struct Stats {
    uint64_t keys = 0, starts = 0, heads_req = 0, tiles_req = 0, caps = 0, ticks_moving = 0, resyncs = 0;
  };
  Stats stats() const;

 private:
  struct Axis {
    double d = 0;                  // D on this axis (tiles)
    double ud = 0;                 // the move per unit of speed (+-1, +-2 or +-sqrt 2 with two arrows; 0 = still)
    double s = 0;                  // the stop tile after the key-up (bounded = true)
    double st = 0;                 // D at the last direction change (the tap minimum counts from here)
    bool bounded = false;
  };
  void dir_locked();
  mutable std::mutex mu_;
  uint32_t held_ = 0;  // bit 0 Left, 1 Up, 2 Right, 3 Down
  Axis ax_[2];
  bool moving_ = false, settled_ = true;  // settled: at rest on the picture's camera (D follows it)
  int32_t rot_ = 0;
  double v_ = 0, step_ = 4, lead_s_ = 0.15;
  int64_t last_ns_ = 0;
  bool engaged_ = false;
  Stats st_;
};
// the 800-space screen vector of a camera move of (dx, dy) tiles (camera_shift's formula, fractional); false for an odd rotation
bool camera_shift_f(int32_t zoom, int32_t rot, double dx, double dy, double &sx, double &sy);
// The 800-space screen vector by which a camera move from `a` to `b` translates the course picture (new place - old place of a
// world point): worldToScreen (bridge.cpp world_to_screen) is linear in (wx - (cam << 10)) scaled by zoom / 128, so a move of
// (dX, dY) tiles moves every point by, for rotation 0: (-8Z (dX + dY), 5Z (dX - dY)); 2: (8Z (dX - dY), 5Z (dX + dY));
// 4: (8Z (dX + dY), 5Z (dY - dX)); 6: (8Z (dY - dX), -5Z (dX + dY)), Z = the zoom (exact: 1024 * Z / 128 = 8Z, 8Z * 20 / 32 = 5Z).
// False when either snapshot is not ok or the zoom or rotation differ.
bool camera_shift(const CameraSnap &a, const CameraSnap &b, int32_t &dx, int32_t &dy);

struct GpuWorldPic;  // P17B fix 2 (gpu_world.h)
struct WorldImage {
  std::vector<uint16_t> px;
  std::vector<uint8_t> mask;
  int32_t pw = 0, ph = 0, fw = 0, ox = 0;
  // P9B: the frame's HUD pixel layers (empty = none: P8B's path). When set, `mask` was taken over hud.base (host.h
  // make_world_image) and the presenter shows hud.base, the world where the mask says, then every layer in draw order.
  portsim::GfxHudFrame hud;
  // P9D: the frame was published by the modal capture (gfx.h gfx_set_hud_modal) without layers: no panel of it is a layer (a
  // screen drawn before this frame's HUD, the end-of-year screen), so it is shown unmoved instead of by G32's rectangles
  bool hud_none = false;
  // P21C: a hud_none frame whose screen dimmed every root pixel with dim_src (GfxHudFrame::dim_all): the presenter dims the
  // window outside the 800 x 600 rectangle the same way
  uint8_t dim_all = 0;
  uint16_t dim_src = 0;
  // P9C: the frame's text runs (the host fills it with --text-res native, published with this back buffer; the presenter takes it
  // with Presenter::set_text whether or not the frame has a world layer)
  portsim::GfxTextFrame text;
  std::vector<uint8_t> mask_text;  // P9C: `mask` taken over the picture without the runs' glyphs (empty = none)
  std::vector<uint16_t> px_text;   // P9C: `px` with the wide scene's text glyphs removed (LiveTerrain::world_frame text = true)
  // P9E fix 4: the tool previews drawn at the cursor tile, taken out of px / px_text (LiveTerrain::world_cursor): cur_px / cur_cov
  // in world pixels, cur_box their box (x0, y0, x1, y1), cur_mask the frame's pixels they drew (800 x 600). The presenter leaves
  // the frame's copy of them out of its centred layers and draws them at the offset of the layer the cursor answer was moved by.
  // P10F: cur_cov 2 = a pixel of P7's translucent placement box alone: cur_px is the box over a black word and cur_hi over a white
  // one, and the presenter blends it over the client pixel under it (so the box stays translucent where P10E moves it)
  std::vector<uint16_t> cur_px, cur_hi;
  std::vector<uint8_t> cur_cov, cur_mask;
  int32_t cur_box[4] = {0, 0, 0, 0};
  CursorProj proj;  // P10E
  CameraSnap cam;   // P12A fix 1
  // P12A fix 2: the frame's window-resolution sprite queue (gfx.h GfxHrScene: its base and every draw with its source), for the
  // moving objects drawn between two game frames (null: none, or --smooth-objects off)
  std::shared_ptr<const portsim::GfxHrScene> hr_scene;
  // P22C item 3 fix 3 (--trail-glide): the ball trail segments the frame kept off the world layer (gfx.h gfx_trail_take); the
  // presenter draws them under the sprites at their ball's drawn offset (Presenter::objects_tick)
  std::vector<portsim::GfxTrailLine> trails;
  // P17B fix 2 (gpu_world.h): px and px_text as textures of the world thread's device shared with the window's (null: none); the GPU
  // compositor copies the one it shows on the GPU instead of uploading the words (which stay for every CPU reader)
  std::shared_ptr<const GpuWorldPic> gpu;
  bool empty() const { return px.empty(); }
};

struct HoverModel;  // P10I fix 2 (hover.cpp)
class GpuCompositor;  // P14A (gpu.cpp)
struct HoverStats {  // P10I fix 2: session.json p10i
  uint64_t frames = 0, controls_made = 0, resets = 0, mispredicts = 0, ticks = 0, swaps = 0, paints = 0;
  uint64_t confirms = 0;  // P27C fix 1: hover_confirm calls taken
  uint32_t controls = 0;
};
class Presenter {
 public:
  explicit Presenter(PresentDefect d = PresentDefect::None, ScaleMode s = ScaleMode::Nearest);
  ~Presenter();
  void set_scale(ScaleMode s) { scale_ = s; }
  ScaleMode scale() const { return scale_; }
  // the mode a content rectangle of w x h is drawn with (Auto resolved)
  ScaleMode effective_scale(int32_t w, int32_t h) const;
  // A new frame: 800 * 600 RGB555 words, rows top-down. Returns false when the size is wrong. `band`: P7's hint for the side
  // bands (used only when set_wide is not Off).
  bool set_frame(const std::vector<uint16_t> &px, BandFill band = BandFill::Black);
  // P8B: the same with the frame's world layer (moved in; an empty one = none). With a world layer the client image is the world
  // wherever the frame does not cover it or its mask says the world shows, and the frame's own pixel elsewhere (section above)
  // P10A: `world` gets the presenter's previous world image back (its buffers, for LiveSession::recycle_world)
  bool set_frame_world(const std::vector<uint16_t> &px, BandFill band, WorldImage &&world);
  const WorldImage &world() const { return world_; }
  // G32: the HUD compositor (kHudPanels; off = P8B stage 2: the frame's 2D pixels stay centred) and the remap it keeps current
  void set_hud_anchor(bool on) { hud_anchor_ = on; }
  void set_remap(PanelRemap *r) { remap_ = r; }
  // P9E: popups opened from a HUD panel composed at that panel's place (GfxHudFrame::pieces; off = centred, P9D)
  void set_popups(bool on) { popups_on_ = on; }
  // P10C fix 4: a textInputBox piece with no opener panel (GfxHudPiece::text, opener kGfxLayerCentred) is moved so its box is
  // centred on the content rectangle's centre (the frame's x 400), like P9E's moved pieces: its pixels, runs and clicks
  void set_text_boxes(bool on) { text_boxes_ = on; }
  // P11B fix 1: the edge the panel layer (kGfxLayerPanel: the employee view / detail and golfer action panels opened from the
  // radial menu's people buttons, plus the shot texts drawn in their place) is anchored to: -1 left (beside the radial menu, as in
  // the 800-space frame), +1 right (P9B's gfx_layer_anchor). Popups those panels open follow it.
  void set_panel_anchor(int8_t a) { panel_anchor_ = a < 0 ? -1 : 1; }
  // P13C item 10: the golfer strip's faces (kGfxLayerStrip) at the right of the Build palette (the palette's right end is the
  // content rectangle's right edge once the bottom band is moved to the left edge) instead of under it
  void set_strip_right(bool on) { strip_right_ = on; }
  void set_strip_near(bool on) { strip_near_ = on; }  // P19C item 8
  int8_t layer_anchor(int32_t id) const {
    if (id == portsim::kGfxLayerStrip && strip_right_) return 0;  // P13C: popups its faces open stay centred
    return id == portsim::kGfxLayerPanel ? panel_anchor_ : portsim::gfx_layer_anchor(id);
  }
  // P9C: the frame's text runs drawn at the window's resolution (--text-res native). set_text after set_frame / set_frame_world
  // (each clears the runs): the frame must have been published with the same back buffer (the host checks). With layers, the layers'
  // base and pixels are replaced by the run-free ones it carries. At a content rectangle of exactly 800 x 600 nothing changes (the
  // frame's text is already at the window's resolution) unless set_text_unit_redraw (a measurement: the same path at scale 1).
  void set_text_res(bool native) { text_native_ = native; }
  void set_text_defect(TextDefect d) { text_defect_ = d; }
  void set_text_aa(TextAa a) { text_aa_ = a; }
  void set_text_unit_redraw(bool on) { text_unit_redraw_ = on; }
  void set_text_fit(bool on) { text_fit_ = on; }  // P10B fix 2 (TextRaster::set_fit)
  void set_text(portsim::GfxTextFrame &&tf);
  bool has_text() const { return text_ok_; }
  const TextResStats *text_stats() const { return raster_ ? &raster_->stats : nullptr; }
  // P10A: the FPS overlay at the window's bottom right (profile.h; 0 off, 1 the fps, 2 the fps and the phases), drawn by paint
  // after render in the game's font (Profiler::font) through the P9C text path; render alone never draws it
  void set_overlay(int mode) { overlay_mode_ = mode < 0 ? 0 : mode > 2 ? 2 : mode; }
  int overlay() const { return overlay_mode_; }
  void draw_overlay(std::vector<uint32_t> &out, int32_t cw, int32_t ch) const;
  // P14A: the overlay's client box (x0, y0, x1, y1; false = nothing drawn), and the overlay drawn into a canvas (render_rect's)
  bool overlay_box(int32_t cw, int32_t ch, int32_t box[4]) const;
  // P15B: the overlay's runs as draw_overlay_rect draws them (layout viewport vp, right offset dxr; grey antialiasing, no fit) and its
  // darkened box (overlay_box's); false = nothing drawn
  bool overlay_runs(int32_t cw, int32_t ch, std::vector<portsim::GfxTextRun> &runs, Viewport &vp, int32_t &dxr, int32_t box[4]) const;
  // P14A: the overlay drawn from one profiler line until unfrozen (a GPU paint's patches and its A/B check read the same line)
  void overlay_freeze(bool on) const {
    overlay_frozen_ = on;
    if (on) overlay_line_ = Profiler::get().last();
  }
  void draw_overlay_rect(std::vector<uint32_t> &out, int32_t cw, int32_t ch, int32_t x0, int32_t y0, int32_t w, int32_t h) const;
  // P10E: the live cursor preview between game frames (--paint-rate display). Active: the frame's tool previews at the cursor
  // (WorldImage::cur_* and the frame's cursor2d pixels) are drawn moved by (sx, sy) 800-space pixels, at the client offset dx
  // instead of the sim's last cursor answer's (PanelRemap::cursor_dx). set_frame / set_frame_world clear it (a new frame draws
  // its previews at its own cursor tile).
  struct CursorLive {
    bool active = false;
    int32_t sx = 0, sy = 0, dx = 0;
    bool operator==(const CursorLive &o) const { return active == o.active && sx == o.sx && sy == o.sy && dx == o.dx; }
    bool operator!=(const CursorLive &o) const { return !(*this == o); }
  };
  void set_cursor_live(const CursorLive &c) { live_ = c; }
  const CursorLive &cursor_live() const { return live_; }
  // P12A fix 1 (log/roadmap/P12/P12A_notes.md): the camera slide (--smooth-camera on). set_frame_world compares the new frame's
  // CameraSnap with the camera of the picture the window painted last: a camera move translates the course picture by D
  // (camera_shift, 800-space). The newest frame is shown at once, its course picture (the base and the world, the base / world text
  // runs, the tool previews at the cursor and the tee line) at an offset that starts where the painted picture was (its offset minus
  // D, client pixels) and goes linearly to 0 over one game-frame period (the mean present interval of the sim thread); the HUD
  // layers do not move. Client pixels the shifted picture does not cover show the course picture of the last paint before the frame
  // came (scene_prev_), moved with it. Snap (offset 0, no slide): no snapshot in either frame, a zoom or rotation change, |D| beyond
  // kSlideMax*, a new client size, a frame without HUD layers (title, modal screens: hud_none) or with a whole-frame dim layer.
  struct SlideStats {
    uint64_t frames = 0, slides = 0, moves = 0, snaps_cam = 0, snaps_view = 0, snaps_far = 0, snaps_screen = 0, ticks = 0;
    int32_t max_dx[4] = {0, 0, 0, 0}, max_dy[4] = {0, 0, 0, 0};  // largest |D| per zoom 1 / 2 / 4 / 8 (800-space)
    double period_ms = 0;
    double lat_ms = 0;  // P16A: the mean latency from the sim thread's present to the window taking the picture (slide_start)
    uint64_t lat_slides = 0;  // P16A: slides whose end includes it
    double arr_q_ms = 0;      // P16A fix 2: the arrival interval quantile the last slide used
    uint64_t arr_slides = 0;  // P16A fix 2: slides that it made longer
  };
  static constexpr int32_t kSlideMaxX = 400, kSlideMaxY = 300;  // 800-space; a larger move is a jump (a map click)
  void set_smooth_camera(bool on) { smooth_ = on; }
  void set_glide(ArrowGlide *g) { glide_ = g; }  // P26A (--arrow-glide on): the arrow glide drives the slide's offset
  uint64_t glide_paints() const { return glide_moves_; }
  bool smooth_camera() const { return smooth_; }
  // the offset for `now_ns` (perf_now_ns), set on the presenter and the remap; true = it changed (the window repaints)
  bool slide_tick(int64_t now_ns, const Viewport &vp);
  bool sliding() const { return slide_on_; }
  void slide_offset(int32_t &x, int32_t &y) const {
    x = cur_x_;
    y = cur_y_;
  }
  const SlideStats &slide_stats() const { return slide_stats_; }
  // the last frame's slide start (SG_P12_LOG): D (800-space), the start offset (client pixels), the period, the snap reason
  // (0 slid, 1 no snapshot, 2 zoom / rotation, 3 too far, 4 screen, 5 no move)
  struct SlideStart {
    int32_t dx = 0, dy = 0, reason = 0;
    int32_t ndx = 0, ndy = 0;  // the next camera's shift (800-space)
    double s0x = 0, s0y = 0, period_ms = 0;
  };
  const SlideStart &slide_start() const { return slide_start_; }
  // P12A fix 1: the window changed the picture without a new frame or a preview / slide move (a hover swap): the next paint counts
  // as a changed picture for the FPS overlay
  void mark_changed() const {
    changed_pending_ = true;
    ++content_gen_;  // P14A: a hover swap changed layer pixels / runs (the GPU path uploads them again)
  }
  // P12A fix 2 (log/roadmap/P12/P12A_notes.md): the moving objects between two game frames (--smooth-objects on). set_frame_world
  // matches the new frame's tagged queue draws (WorldImage::hr_scene, gfx.h GfxMoverSrc) with the previous frame's: the same source
  // (kind and record index), the same occurrence among that source's draws (any animation cel: the step is taken from the
  // entries' own placements, GfxHrDraw::ex / ey), and a plausible step (world
  // distance and 800-space distance within kMover*); the previous placement is moved by the camera's shift between the two
  // pictures. objects_tick draws each matched object at its blended place: characters (golfers, walkers, animals, flyers,
  // residents) from the previous frame's place to the new one over the frame period (shown one frame late). The ball and its
  // shadow too (P19A item 7: they were drawn ahead by the last step, and the frame's places are whole 800-space pixels, so the
  // steps zig-zagged and every frame pulled the ball back: a wobble), between their EXACT places (the place plus the sim's
  // render-only residual GfxMoverSrc::fx / fy: worldToScreen and the height without their integer divisions). Only
  // the rectangles the objects leave and enter are redrawn (gfx_hr_scene_redraw: every draw of the queue over its base, in queue
  // order, so the frame's sort order is kept), into the world image's pixels where a draw covered them before or after the move.
  // Their name labels (text runs with GfxTextRun::mover) move with them.
  struct ObjectStats {
    uint64_t frames = 0, matched = 0, moved = 0, rejected_far = 0, rejected_art = 0, ticks = 0, redraws = 0, paints = 0;  // rejected_art: matched draws whose art size changed (a cel change)
    uint64_t redraw_px = 0;
    uint64_t moved_kind[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // moving draws per GfxMoverKind (over all frames)
    int64_t redraw_ns = 0;
  };
  static constexpr int32_t kMoverMaxStep = portsim::kGfxMoverMaxStep;  // 800-space pixels per game frame (gfx.h)
  static constexpr int32_t kMoverMaxWorld = 2048;       // world units (1024 per tile) per game frame, characters
  static constexpr int32_t kMoverMaxWorldBall = 8192;   // the ball and its shadow
  void set_smooth_objects(bool on) { objects_on_ = on; }
  bool smooth_objects() const { return objects_on_; }
  bool objects_tick(int64_t now_ns);  // true = the picture changed (the window repaints)
  // P12A fix 3: at most `hz` object redraws per second (0 = every tick that changes an offset; --object-rate)
  void set_object_rate(int hz) {
    object_min_ns_ = hz > 0 ? 1e9 / hz : 0.0;
    object_auto_ = false;
  }
  // P14A fix 2 (--object-rate auto): every pacer tick while the GPU path paints a client of at most kObjectAutoPx pixels, else
  // kObjectAutoHz per second (P14A_notes.md section 4: at 1920 x 1080 the game's own rate did not drop with every tick, at 3840 x 2108
  // it did; the CPU path's paints cost too much to repaint at every tick). P15C fix 2 (log/roadmap/P15/P15C_notes.md section 4):
  // every pacer tick at any client size while the frame's objects are drawn by the GPU (objects_on_gpu); the size limit stays for the
  // CPU redraw (SG_APP_P15=off, --gpu-objects off, a frame the GPU does not draw)
  static constexpr int64_t kObjectAutoPx = 1920LL * 1200LL;
  static constexpr int kObjectAutoHz = 30;
  void set_object_rate_auto() { object_auto_ = true; }
  // P15C (log/roadmap/P15/P15C_notes.md): with the GPU path the moving objects are drawn by the GPU (GpuCompositor: the same redraw,
  // gfx.h gfx_hr_scene_tables / gfx_hr_scene_gpu_op, transcribed to a compute pass); objects_tick then only finds the offsets and the
  // rectangles. The CPU world image gets the objects only where a CPU render reads it (objects_sync: the patches, the A/B check),
  // undone at the next tick. Off (SG_APP_P15=off, set_gpu_objects(false)), or a frame the GPU cannot draw: the CPU redraw as before.
  void set_gpu_objects(bool on) { gpu_objects_ = on; }
  bool gpu_objects() const { return gpu_objects_; }
  bool objects_on_gpu() const { return movers_gpu_; }
  std::string objects_json() const;  // session.json p15c_objects
  bool object_rate_auto() const { return object_auto_; }
  int object_rate_now() const;  // the rate objects_tick uses now (0 = every tick)
  const ObjectStats &object_stats() const { return object_stats_; }
  // this frame's tagged draws per GfxMoverKind (in the scene) and of them the ones moving (SG_P12_LOG objects lines)
  void frame_objects(int32_t tagged[8], int32_t moving[8]) const;
  // the client offset of a name label's object now (0, 0 when it is not moving)
  void mover_offset(const portsim::GfxMoverSrc &src, const Viewport &vp, int32_t &dx, int32_t &dy) const;
  // P10I fix 1: the frame's tee lines (GfxHudFrame::lines, gfx.h gfx_set_hud_lines) drawn at the window's resolution in the cursor
  // previews' place in the draw order, the second end moved with them by P10E's live shift; their 800-space pixels are left out of
  // the centred layers (--line-res native; off = the frame's line pixels scaled with the layer)
  void set_line_native(bool on) { line_native_ = on; }
  // P23E fix 1: the GPU presenter renders a native line's CPU patch as pieces along the line, not its whole box (--line-tiles)
  void set_line_tiles(bool on) { line_tiles_ = on; }
  // P24C fix 2b: the GPU presenter draws the course runs from tables over its course picture, not as CPU course patches (--course-tables)
  void set_course_tables(bool on) { course_tables_ = on; }
  // P10I fix 2 (hover.cpp): the HUD's hover highlights between game frames (--hover-rate display). hover_observe after each new frame
  // (set_frame / set_frame_world and set_text) with the 800-space cursor point the sim's hover tests read for it (LiveSession::
  // frame_cursor) learns, per surface (each HUD panel layer, or the whole frame of a screen without layers), the boxes of the controls
  // whose look changed when the cursor moved, each look (the surface's pixels over the box and the text runs over it) and the cursor
  // points it was seen at. hover_tick with the live cursor point swaps a box to the look the nearest observation gives (only inside
  // the box of the points that look was seen at), and back; true = the picture changed. A new frame shows its own looks again.
  void set_hover(bool on) { hover_on_ = on; }
  bool hover() const { return hover_on_; }
  void hover_observe(int32_t cx, int32_t cy, const Viewport &vp);
  bool hover_tick(int32_t cx, int32_t cy, const Viewport &vp);
  // P27C fix 1: the sim read the cursor at (cx, cy) and its pass left the picture as the current frame shows it (a present equal to
  // the last one, LiveSession::take_hover_confirm): each control's current look is the game's own answer there. It becomes an
  // observation of that look at that point and the frame's point, so hover_tick shows the frame's own look while the cursor stays
  // there and predicts from it later. Without it a look predicted between frames (the nearest earlier observation's, over the
  // whole box of a look seen only inside the control) stayed on screen at points the game's hit test rejects: on a modal screen
  // a pass that changes nothing makes no present, so nothing corrected it.
  bool hover_confirm(int32_t cx, int32_t cy);
  // P27C fix 2: the frame shown is a flat screen (no world, so no layers: the title menu and the full-screen modal screens; the
  // same frames flat_remap_clear clears the click remap for)
  bool flat_now() const { return !frame_.empty() && world_.empty(); }
  HoverStats hover_stats() const;
  // P7: the side-band fill (Off = black bands, the pre-P7 behaviour)
  void set_wide(WideFill w) { wide_ = w; }
  WideFill wide() const { return wide_; }
  BandFill band() const { return band_; }
  bool has_frame() const { return !frame_.empty(); }
  const std::vector<uint16_t> &frame() const { return frame_; }
  uint32_t frames_set() const { return frames_set_; }
  // The client image for cw x ch: BGRA words, rows top-down (vp receives the content rectangle).
  void render(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vp = nullptr) const;

#ifdef _WIN32
  // Draws the client image into `hdc` (a window DC or a paint DC) at (0, 0). P14A: with the GPU path started, composes and presents
  // through it instead (hdc unused).
  void paint(void *hdc, int32_t cw, int32_t ch) const;
#endif
  // P14A (log/roadmap/P14/P14A_notes.md): the window's picture composed and presented by the GPU (D3D11, a flip-model swap chain on
  // `hwnd`). gpu_start: false = the CPU path stays (`why` says why). The GPU path draws what Presenter::render would draw at the same
  // state: the integer pixel rules on the GPU, GDI text / tee lines / the overlay as CPU patches (render restricted to rectangles).
  bool gpu_start(void *hwnd, std::string &why);
  bool gpu_on() const;
  void gpu_stop();
  std::string gpu_json() const;  // session.json p14
  void gpu_shot(const std::string &path);
  // P14A: blocks until the next vertical blank of the GPU path's monitor (the pacer's clock with the GPU on); false = not available
  bool gpu_wait_vblank() const;  // the next GPU paint's picture as a BMP (the harness's capture; no-op on the CPU path)
  // P14A: render restricted to client rectangle (x0, y0, w, h) (may lie outside the client; rows / columns outside it are not
  // computed): `out` receives w x h words. course = the course picture alone (no HUD layers / runs, no overlay), at slide offset 0.
  // No side effect on the presenter's state (the scene kept for the slide, the click remap).
  void render_rect(int32_t cw, int32_t ch, int32_t x0, int32_t y0, int32_t w, int32_t h, bool course, std::vector<uint32_t> &out) const;

 private:
  friend class GpuCompositor;  // P14A
  PresentDefect defect_;
  ScaleMode scale_;
  std::vector<uint16_t> frame_;
  uint32_t frames_set_ = 0;
  WideFill wide_ = WideFill::Off;  // P7
  BandFill band_ = BandFill::Black;
  void fill_bands(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const;  // P7
  void fill_bands_p9(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const;  // P9A item 5
  void fill_bands_map(std::vector<uint32_t> &out, int32_t cw, const Viewport &vp) const;  // P13B item 18
  WorldImage world_;                                                                                  // P8B
  bool hud_anchor_ = false;                                                                           // G32
  PanelRemap *remap_ = nullptr;                                                                       // G32
  void compose_world(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp) const;  // P8B
  bool layered() const;                                                                                // P9B
  // P9B; P12A: (slx, sly) = the camera slide's offset of the course picture (client pixels): the tool previews and the tee line
  // move with it; the base / world runs are drawn by render before it (with the slid viewport)
  void compose_layers(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp, int32_t slx = 0, int32_t sly = 0) const;
  mutable std::vector<uint32_t> scratch_;
  mutable std::vector<uint32_t> base_bgra_;  // P10A: render's BGRA copy of the base (smooth path)
  mutable const void *base_bgra_src_ = nullptr;  // P14A: the copy's source and content generation
  mutable uint64_t base_bgra_gen_ = ~0ull;
  mutable std::vector<int32_t> base_lx_, cw_tab_, cl_tab_;  // P10A: render / compose_world / compose_layers column tables
  const uint8_t *base_hidden(const Viewport &vp, bool text) const;  // P10A
  int overlay_mode_ = 0;                                   // P10A
  mutable std::unique_ptr<TextRaster> overlay_raster_;     // P10A
  mutable uint32_t painted_set_ = 0;                       // P10A: frames_set_ at the last paint
  CursorLive live_;                                        // P10E
  bool line_native_ = false;                               // P10I fix 1
  bool line_tiles_ = false;                                // P23E fix 1
  bool course_tables_ = false;                             // P24C fix 2b
  bool hover_on_ = false;                                  // P10I fix 2
  std::shared_ptr<HoverModel> hover_;                      // P10I fix 2 (hover.cpp)
  // P10I fix 2: the frame's own layer pixels of the layers a hover swap changed (by layer index; empty = unchanged), so the click
  // remap's coverage stays the frame's
  std::vector<std::vector<portsim::GfxLayerPx>> hover_pristine_;
  mutable CursorLive painted_live_;                        // P10E: live_ at the last paint
  // P12A fix 1: the camera slide
  bool smooth_ = false;
  bool slide_on_ = false;
  ArrowGlide *glide_ = nullptr;  // P26A
  bool glide_on_ = false;        // P26A: the last tick's offset came from the glide
  uint64_t glide_moves_ = 0;     // P26A: ticks whose glide offset changed
  std::array<double, 8> glide_arr_{};  // P26A: the last pictures' frame-head-to-arrival times (ns)
  size_t glide_arr_i_ = 0;
  double s0x_ = 0, s0y_ = 0, period_ns_ = 0;  // the start offset (client pixels), the game frame period (mean present interval)
  double slide_ns_ = 0;                        // this slide's length
  // P21B: whether a slide was running when the new frame came and where it would have ended (set_frame clears slide_on_ before
  // slide_start runs, so set_frame_world keeps them for it)
  bool slide_was_on_ = false;
  int64_t slide_was_end_ = 0;
  int32_t s0x_i_ = 0, s0y_i_ = 0;             // the start offset rounded (the underlay's place at the start)
  int64_t t0_ns_ = 0, last_present_ns_ = 0;
  double lat_ns_ = 0;                          // P16A: mean present-to-arrival latency (slide_start)
  std::array<double, 16> arr_iv_{};            // P16A fix 2: the last picture arrival intervals (ns, a ring)
  int arr_n_ = 0, arr_i_ = 0;
  int64_t last_arrival_ns_ = 0;
  int32_t cur_x_ = 0, cur_y_ = 0;             // the offset the next paint draws
  int32_t nd_x_ = 0, nd_y_ = 0;               // the shown frame's next-camera shift (CameraSnap::next_*, 800-space; 0 = none)
  SlideStats slide_stats_;
  SlideStart slide_start_;
  // the course picture of the last paint (before the HUD layers), its size, offset and camera; scene_prev_: the one the slide's
  // uncovered pixels show
  mutable std::vector<uint32_t> scene_last_, scene_prev_;
  mutable int32_t scene_w_ = 0, scene_h_ = 0, prev_w_ = 0, prev_h_ = 0;
  mutable int32_t painted_x_ = 0, painted_y_ = 0;
  mutable CameraSnap painted_cam_;
  mutable bool scene_ok_ = false, prev_ok_ = false;
  mutable int32_t painted_cur_x_ = 0, painted_cur_y_ = 0;  // the offset at the last paint (the profiler's slide paints)
  mutable bool changed_pending_ = false;
  // P12A fix 2
  struct Mover {
    size_t k = 0;            // the draw (GfxHrScene draws index)
    double dx = 0, dy = 0;   // previous place - new place (800-space)
    double rx = 0, ry = 0;   // P19A: the new place's residual (exact - drawn, GfxMoverSrc::fx / fy): the offset at the period's end
    // (P19A: dx / dy start from the place drawn when the frame came, Presenter::set_frame_world, so an early frame does not jump)
    portsim::GfxMoverSrc src;
    double ox = 0, oy = 0;   // the offset drawn now (800-space)
  };
  bool objects_on_ = false;
  std::vector<Mover> movers_;
  // P22C item 3 fix 3 (--trail-glide): this frame's trail segments (world_.trails) are drawn by objects_tick (the CPU redraw) under
  // the sprites at their ball's offset; trail_ok_: the scene can draw them, trail_drawn_: drawn once since the frame came
  bool trail_ok_ = false, trail_drawn_ = false;
  std::vector<std::array<double, 2>> trail_off_;  // the offset each line was last drawn at
  uint64_t trail_frames_ = 0, trail_fallback_ = 0, trail_ticks_ = 0;
  void trail_offsets(std::vector<double> &ldx, std::vector<double> &ldy) const;  // each line's ball offset now (0: no mover)
  void trail_fallback();  // a frame whose trails the scene cannot draw: over the world image at the frame's place
  std::shared_ptr<const portsim::GfxHrScene> prev_scene_;
  CameraSnap prev_cam_;
  int64_t objects_t0_ = 0;
  double object_min_ns_ = 0;      // P12A fix 3 (set_object_rate)
  bool object_auto_ = false;      // P14A fix 2
  mutable int64_t painted_px_ = 0;  // P14A fix 2: the client pixels of the last paint
  int64_t object_last_ns_ = 0;    // the last objects_tick that drew
  std::vector<uint16_t> px_orig_, pxt_orig_;  // the frame's world pixels before any object was moved
  std::vector<std::array<int32_t, 4>> patched_;  // the world rectangles the last objects_tick wrote
  ObjectStats object_stats_;
  // P15C (set_gpu_objects)
  bool gpu_objects_ = true;
  bool movers_gpu_ = false;                      // this frame's objects are drawn by the GPU (objects_start decides)
  std::vector<std::array<int32_t, 4>> mregs_;    // the last tick's rectangles (world pixels), as objects_tick finds them
  std::vector<double> mdx_, mdy_;                // and each draw's offset (800-space)
  mutable uint64_t mover_gen_ = 0;               // bumped by each tick that moved an object on the GPU path
  mutable std::vector<std::array<int32_t, 4>> mover_dirty_;  // the rectangles left and entered since the GPU took them
  struct SyncUndo {
    std::array<int32_t, 4> r;
    std::vector<uint16_t> px, pxt;
  };
  mutable std::vector<SyncUndo> sync_undo_;       // the CPU world pixels objects_sync replaced this tick (undone in reverse)
  mutable std::vector<std::array<int32_t, 4>> synced_;  // the world rectangles objects_sync drew this tick
  mutable uint64_t sync_calls_ = 0, sync_px_ = 0, gpu_ticks_ = 0, gpu_frames_ = 0, gpu_refused_ = 0;
  mutable int64_t sync_ns_ = 0, tick_ns_ = 0;
  void objects_sync(int32_t x0, int32_t y0, int32_t x1, int32_t y1) const;  // world rectangle
  void objects_sync_view(const Viewport &vs) const;  // compose_world: the world pixels the canvas reads at viewport vs
  void objects_undo() const;
  // set_frame_world; drawn: the old movers' drawn offsets by their old draw index (P19A item 7)
  void objects_start(const WorldImage &prev, const std::map<size_t, std::pair<double, double>> &drawn);
  void slide_start(const WorldImage &w);  // set_frame_world
  void fill_gap(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vs, int32_t slx, int32_t sly) const;
  // P9C
  bool text_native_ = false, text_ok_ = false, text_layered_ok_ = false, text_unit_redraw_ = false;
  TextDefect text_defect_ = TextDefect::None;
  TextAa text_aa_ = TextAa::System;
  bool text_fit_ = false;  // P10B fix 2
  portsim::GfxTextFrame text_;
  portsim::GfxHudFrame hud_text_;  // world_.hud with the base and layer pixels of text_ (the runs' glyphs removed)
  mutable std::unique_ptr<TextRaster> raster_;
  const portsim::GfxHudFrame &hud_for(bool text) const;
  bool text_now(const Viewport &vp) const;  // the runs are drawn for this content rectangle
  void draw_runs(std::vector<uint32_t> &out, int32_t cw, int32_t ch, const Viewport &vp, bool layered, int32_t layer,
                 int32_t dx) const;
  // P9E: the popups of the frame being composed (GfxHudFrame::pieces): each piece's client offset (0 = it stays centred), set by
  // compose_layers for the layers that carry popup pixels; draw_runs moves a run of such a layer with the piece that owns it
  bool popups_on_ = true;
  bool text_boxes_ = false;  // P10C fix 4
  int8_t panel_anchor_ = 1;  // P11B fix 1 (set_panel_anchor)
  bool strip_right_ = false;  // P13C item 10 (set_strip_right)
  bool strip_near_ = false;   // P19C item 8 (set_strip_near): the strip right of the panel still open, not of the palette's end
  // P13C: a layer's client offset (the anchor's, or the strip's own place right of the palette)
  int32_t layer_dx(const portsim::GfxHudLayer &l, const Viewport &vp, int32_t dxl, int32_t dxr) const;
  mutable std::vector<int32_t> piece_dx_;
  mutable const portsim::GfxHudFrame *piece_frame_ = nullptr;
  static int32_t piece_of(const portsim::GfxHudFrame &f, int32_t x, int32_t y);  // piece index at an 800-space pixel, -1 none
  // ---- P14A
  // the buffer the compose functions write: client pixel (x, y) is out[(y - y0) * w + (x - x0)]; render sets it (the whole client
  // for paint, a rectangle for render_rect)
  struct Canvas {
    int32_t x0 = 0, y0 = 0, w = 0, h = 0;
    bool full = true;  // the whole client (render's side effects allowed)
  };
  mutable Canvas cv_;
  mutable bool quiet_ = false;     // render_rect / the A/B check: no side effect (scene keep, remap)
  mutable bool force_slide0_ = false;  // render_rect(course): the course picture at offset 0
  mutable bool course_only_ = false;   // render_rect(course): stop before the HUD layers
  // P14A: non-zero = render_impl draws the HUD layers alone (no text runs) over a course picture of this one word (the GPU path's
  // classification of its patches: equal results over black and white = the course does not show through)
  mutable uint32_t probe_ = 0;
  void render_impl(int32_t cw, int32_t ch, std::vector<uint32_t> &out, Viewport *vp) const;
  // P9E's piece offsets and P9B's click remap, as compose_layers sets them (factored out; remap only when not quiet)
  void layer_pieces(const portsim::GfxHudFrame &f, const Viewport &vp, int32_t cw) const;
  void layer_remap(const portsim::GfxHudFrame &f, const Viewport &vp, int32_t cw) const;
  void flat_remap_clear() const;  // P26C item 4a: a frame without world and layers remaps no point
  bool pieces_on(const portsim::GfxHudFrame &f, const Viewport &vp) const;
  // compose_layers' draw-order places of the previews and the tee lines (cached per frame / content generation)
  mutable const void *order_key_ = nullptr;
  mutable uint64_t order_gen_ = ~0ull;
  mutable size_t cur_layer_c_ = 0, line_layer_c_ = 0;
  void draw_order(const portsim::GfxHudFrame &f, bool cur, bool lines, size_t &cur_layer, size_t &line_layer) const;
  mutable uint64_t content_gen_ = 0;  // bumped by a new frame, a hover swap, set_text (the GPU path re-uploads)
  mutable uint64_t world_gen_ = 0;    // bumped by objects_tick when it rewrote world pixels
  mutable uint64_t world_px_gen_ = 0;  // P15C: bumped when the CPU world image the GPU uploads changed (not by a GPU objects tick)
  mutable std::vector<std::array<int32_t, 4>> world_dirty_;  // the world rectangles objects_tick rewrote since the GPU's upload
  // P22C fix 3: a new world image (set_frame) not yet uploaded whole: the GPU uploads it whole even when objects_tick already rewrote
  // rectangles of it (set_frame_world draws a frame's trail lines before its first paint), then those rectangles over a shared picture
  mutable bool world_whole_ = true;
  mutable uint64_t swap_gen_ = 0;     // bumped where slide_start swaps scene_prev_ / scene_last_
  mutable bool overlay_frozen_ = false;  // overlay_freeze
  mutable ProfLine overlay_line_;
  std::unique_ptr<GpuCompositor> gpu_;
#ifdef _WIN32
  void paint_gpu(int32_t cw, int32_t ch) const;
#endif
};

}  // namespace portapp

#endif
