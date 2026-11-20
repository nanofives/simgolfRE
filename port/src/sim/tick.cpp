// Module `tick` (phase 3.7a, 2026-10-19): mainLoop's frame step (portsim/sim/tick.h), sg_run_frames, and the
// straddler leaves the frame calls. Notes: log/p3/p37a_notes.txt. Spec: re/analysis/sim/mainLoop_structure.md,
// re/analysis/sim/time_model.md, the C3 hook sources (shim/src/re/c3fy.cpp for the frame, converted mechanically by
// log/p3/p37a_conv.py + log/p3/p37a_patch_frame.py into tick_frame.inc). No original address in this file: data
// addresses are GameState members, code is cited as ML+0x<offset> (mainLoop) or by function name.
//
// CALLEES. The frame calls its callees by hooks.csv name (TkBase::call): a body of this module, a registered port function
// of any module (the registry), or a DROP listed in kDrops below with its reason. A callee that is none of these throws
// MissingResource{name}, so a frame never silently skips an unported effect. A drop is a function whose whole static call
// closure writes no simulation-class object and takes the address of none (log/p3/p37a_simwrites.py, census in the notes)
// and whose return value the frame does not feed into simulation state.
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS  // the development dump (SG_TICK_DUMP) uses fopen / getenv
#endif
#include "portsim/sim/tick.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/bridge.h"  // P8B: the view block (tick_render.inc, tick_boot.inc)
#include "portsim/sim/gfx.h"
#include "portsim/sim/perf.h"  // P38G development profiler (perf_now_ns)
#include "portsim/sim/lang.h"  // P24D fix 5b (tick_panels.inc drawBuildingPanel)
#include "portsim/execonst.h"
#include "portsim/sim/integrator.h"
#include "portsim/sim/live.h"
#include "portsim/sim/shots.h"  // P32D item 7: the shot's margin of error (tick_frame.inc P32D_ShotMargin)
#include "portsim/sim/terrain.h"  // P35B: corner_heights (the fall occluders)
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/util.h"
#include "portsim/state_hash.h"
#include "portsim/sim/world.h"  // P38A: grid bases and the world size
#include "tick_hook.h"
#include "portsim/sim/terrain_gen.h"  // P38B: generate_flat_course (tick_menu.inc)
#include "golfer_wait.h"  // P36B
#include "landoffer.h"    // P38H (tick_frame.inc, the land popup)
#include "portsim/sim/greenline.h"  // P39C (tick_frame.inc P1_flag80 / P39C_GreenOutline: the green outline while aiming)
#include "worldsizes.h"   // P38W (tick_menu.inc, the new course)

namespace portsim {
extern int g_defect_corner_level_clamp;  // terrain_course.cpp (D21)
extern int g_defect_ball_list_bound;     // terrain_course.cpp (D12)
// roadmap S2.11: the graphics backend's cut size of a SpriteRef (gfx_reports.cpp) and the results family's layout controls
// (gfx_results.cpp), for showTournamentResults' drawing positions (tick_panels.inc)
bool gfx_sprite_cut(Ctx &ctx, uint32_t sref, int32_t &w, int32_t &h);
bool results_control(int m);
// roadmap S3.26: a frame-code read that lands inside a SpriteRef the graphics backend holds (gfx_pixel.cpp), for P4's
// selected-golfer sprite when the selection word is not a golfer index (tick_frame.inc P4_selectedSprite)
bool gfx_sprite_ref_read(Ctx &ctx, uint32_t va, uint32_t size, uint32_t &v);
// P23C fix 1: the title buttons' hover shape from the highlighted buttons' picture (gfx.cpp), for tick_menu.inc's
// mainLoop.titleHotspotRange arm
namespace gfxb {
bool title_hover_rows(Ctx &ctx, std::vector<int16_t> &rows, int32_t at[5]);
}
// Compatibility switch (port/CORE.md "Defects"), 1 = behave like the original.
//   mainLoop.cashLowWordOnly  model_defects#23 (D23): the month histories store the cash g_cash_div100's low word into the
//                            int16 ring g_cashHistory (mainLoop ML+0x83d5 / ML+0x83f4 and ML+0x86f3 / ML+0x870e), and the
//                            new-course paths store it into the int16 ledger entry `other` (+0xe) of the year row (mainLoop
//                            ML+0x656..ML+0x65d; switchToCourse in course.cpp, the same store): from 32768
//                            ($3,276,800) on the stored word wraps to a negative value. Fixed (0): the value is saturated to
//                            the int16 range (-32768..32767). Both tables keep their int16 layout (GameState is the exe's
//                            .data image and the save stores them as such), so the full 32-bit value cannot be kept there.
int g_defect_cash_low_word = 1;
// mainLoop.titleExitHotspotWide (P5A, 2026-11-03, Mariano's play-test): the title menu picks the hotspot nearest to the
// cursor among six, distance(x - hx, (y - hy) * 4 - 0x18) < 0xa0 (ML+0x11a95..ML+0x11af0), with each hotspot point at its
// text centre; the exit button has no text and its point is the top-left corner of its 0x3a x 0x39 icon at (0x2d1, 0x210)
// (Setup's slot stores; the icon is showTitleScreen's seventh button cut), so the exit highlights over an area about 0x140 px
// wide and 0x50 tall around the icon's corner, most of it outside the icon. Fixed (0): the exit hotspot is taken only with the
// cursor inside its icon (x 0x2d1 .. 0x30a, y 0x210 .. 0x248), where it wins; the five labelled hotspots keep their test.
int g_defect_title_exit_hotspot = 1;
// mainLoop.titleDefaultItem1 (P9A, 2026-11-03, Mariano's play-test 3: "on the first frame the clickable areas are not working
// properly"): each pass of the title menu's hover loop starts its selection at ebx = (F+0x2c != 0) ? -1 : 1 (ML+0x11a95
// mov ebx, [esp+0x30]; neg; sbb; and 0xfffffffe; inc), and F+0x2c is cleared on entry (ML+0x11a46) and set only when a hotspot
// comes within range (ML+0x11ae1). So from the menu's first frame until the cursor first nears an item, item 1 (Start New Game)
// is drawn highlighted and a click anywhere (ML+0x11cb1 / ML+0x11cba test only the button word and ebx != -1) starts a new
// game. Fixed (0): the pass starts with no item (-1); the hotspot search is unchanged.
int g_defect_title_default_item = 1;
// mainLoop.titleHotspotRange (P9A, the same report: "near a button (but not over) and it would be detected as a hover"): a
// labelled item is chosen when distance(x - hx, (y - hy) * 4 - 0x18) < 0xa0 (ML+0x11aaf..ML+0x11ae1) around its text centre,
// an area about 0x140 px wide, mostly outside the button art (Sandbox Mode lights with the cursor 0x9f px to the right of its
// text, inside the golf ball). Fixed (0): an item is chosen only over its lit button art (showTitleScreen's source
// rectangles showTitleScreen+0x2e4..showTitleScreen+0x3b9, which the menu draws at the item's position F+0x150 / F+0x152; P23C: per row
// the hull of the opaque pixels of the picture the highlighted buttons are cut from, read at the menu's entry, not the whole
// rectangle); among items holding the cursor the original's distance decides. The exit icon (hotspot 5) is mainLoop.titleExitHotspotWide's.
int g_defect_title_hotspot_range = 1;
// mainLoop.titleEscIgnored (P31C, G33 item 2a, 2026-11-15, Mariano: "Both changes"): the title menu's hover loop reads only the
// cursor and the button word (ML+0x11a65..ML+0x11cba), so an Esc does nothing there; its only way out of the game is the exit
// icon (hotspot 5 -> selection -2, Session's QUIT path: top10.sve, Default.pro, the closing movie, the credits). Fixed (0): a
// fresh Esc (the char word 0x1b after a pass's pump, with the Esc key seen up, GetAsyncKeyState high bit clear, before a pump
// since the menu's entry) acts as a click on the exit icon; an Esc held or left from the screen before is dropped.
int g_defect_title_esc = 1;
// mainLoop.staffListCourseClickIgnored (P31C, G33 item 6, Mariano: "Add it"): with an employee picked in the Staff list (the
// detail panel g_ui_561254 open in panel mode 2, g_employeePanelMode != -1) a left press on the course reaches H2_Select, which
// picks a golfer or a walker near the press or ends the frame; the employee moves only through the Move button
// (handleEmployeeAction control 4 arms the pending word f_w_4c2e00 + 0x10, the next press lands it at ML+0xef4c). Fixed (0): a
// left press that H2_Select would end with nothing done (no golfer or walker within 0x28, no hole marker, no clubhouse) lands
// the picked employee's move at the cursor tile exactly as the Move button's armed press does (P31C_notes.md lists what is excluded).
int g_defect_staff_list_click = 1;
// mainLoop.hiddenCheatKeys (P32C, G31 "the original's hidden + (money) / - and = (terrain) key actions are dropped in the app"):
// H1's key switch (ML+0xcfec) takes the char '+' as the cash key (ML+0xda7b: 10,000 more cash and g_flags bit 29, the asterisk
// after the cash), '-' / '=' as lower / raise the terrain cell. The app's --zoom-keys turns + / - into zoom only while the frame
// body pumps; a + typed while a modal screen runs keeps its char, a screen that ends on any key (flush_redraw) leaves it in
// gDirtyFlagB, and H1 reads it there after the screen (setDirtyFlagB, ML+0xcb12). Fixed (0): the chars 0x2b / 0x2d / 0x3d do
// nothing in H1's key switch, whatever path they took.
int g_defect_hidden_cheat_keys = 1;
// loadGame.cheatMarkKept (P35E item 10, Mariano's play-test 17: "* is showing on the money despite not being able to use the cheat
// code for money"): g_flags bit 29, the asterisk P6_StatusBar draws after the cash (ML+0x9f9b), is set only by the cash key '+'
// (ML+0xda81) and read by nothing else. The save writes g_flags whole, loadGame keeps the file's bit 29 (it rewrites bits 2, 3 and
// 30 only), and switchToCourse (the career's move to another course) writes no flag bit, so a mark made before
// mainLoop.hiddenCheatKeys follows a course through every save and onto the next course of the career; only the title menu's
// reset (ML+0x11890) and startNewCourse clear it. Fixed (0): loadGame and switchToCourse clear bit 29 (the cash those presses
// added stays: it cannot be told apart).
int g_defect_cheat_mark_kept = 1;
// mainLoop.terrainLiftCap13 (P35B, play-test 17 item 4: "i can't lift ground more than 10, but mountanous terrain already goes past
// it, make it to 15"): H1's terrain keys (a terrain-tool press becomes one of them) clamp a corner of g_cornerLevel: raise ('=')
// to 3..13 (clamp(level + 1, 3, 13) at ML+0xe5d5, and the brush's 2 x 2 loop raises its lowest corners only while they are below
// 13, ML+0xe68c..ML+0xe69d), lower ('-') to 3..13 on a Tropical course (course record byte +0x1e = 2) and 3..10 on every other
// (ML+0xe3fb..ML+0xe40c, clamp at ML+0xe445). The lowest ground is level 3, so a raise stops 10 steps up, and the generator
// (heightBlend's clamp 3..15 plus terrain_gen's +2 steps) makes mountain corners at 15 to 17 that a raise pulls down to 13 and a
// lower to 10 or 13 in one press. Fixed (0): both keys clamp to 3..kTerrainLiftTop (18, 15 steps above level 3), and a press never
// moves a corner the other way (a raise leaves a corner at or above the top, a lower takes one step from any height). Every
// reader of the levels takes 18 (log/roadmap/P35/P35B_notes.md item 4: bytes, int8 caches, products below 2^31 to level 54).
int g_defect_terrain_lift_cap = 1;
constexpr int kTerrainLiftTop = 18;
// the clamp's upper bound for one terrain key: the original's `orig` at 1; at 0 the top, or the pressed corner's own value when
// it is already above it (raise: the old level, so the press keeps it; lower: the new level, one step down)
inline int terrain_lift_bound(int orig, int keep) {
  if (SG_DEFECT(g_defect_terrain_lift_cap)) return orig;
  return keep > kTerrainLiftTop ? keep : kTerrainLiftTop;
}
// mainLoop.fallOverNearerGround (P35B, play-test 17 item 3: "cascades rendering over green that's above"): the type-0x11 tile pass
// draws a fall's column and foot (cells 0x237 / 0x238 / 0x23b / 0x23c, P1_t11_410ea4 / 410ff8 / 411282) at once with addSprite at
// the foot tile, after the terrain picture is whole, so ground nearer the camera that rises above the foot (a green on a ledge in
// front of the pool) never covers it: the original draws the spray over that ground (seen in the original's own instance,
// log/roadmap/P35/P35B_notes.md item 3). Fixed (0), render-only: each fall piece is drawn with the screen outlines (the drawn top
// faces) of the nearer land tiles that lie wholly above the fall's top as occluders, and the piece's art pixels inside them are not
// drawn (gfx.h gfx_set_sprite_occluders). Ground reaching down to the fall's top keeps the original's picture.
// The queue's order, the art and every GameState word stay as they are.
int g_defect_fall_over_ground = 1;
namespace p35b {
using Poly = std::vector<float>;   // x0, y0, x1, y1, ... (800-frame pixels, convex, counter-clockwise)
// worldToScreen's projection of a world point (1024 per tile) at level 3, without its integer divisions (bridge.cpp
// world_to_screen's exs / eys)
inline void ground_xy(const GameState &gs, double wx, double wy, double &sx, double &sy) {
  const int32_t Z = gs.g_zoomScale;
  double ix = (wx - gs.gCameraTileX * 1024.0 - 0x200) * Z / 128.0, iy = (wy - gs.gCameraTileY * 1024.0 - 0x200) * Z / 128.0;
  const int32_t W = gs.g_screenWidth, H = gs.g_screenHeight;
  if (W == 0x400) ix = ix * 80 / 64, iy = iy * 52 / 40;
  else if (W == 0x500) ix = ix * 104 / 64, iy = iy * 68 / 40;
  const double ax = ix * 20 / 32, ay = iy * 20 / 32;
  switch (gs.g_cameraRot) {
    case 0: sx = iy + W / 2 + Z * 8 + ix; sy = ay - ax + H / 2; break;
    case 2: sx = W / 2 - ix + iy; sy = ix * -20 / 32 - ay + H / 2 - Z * 5; break;
    case 4: sx = W / 2 - Z * 8 - iy - ix; sy = ax - ay + H / 2; break;
    default: sx = W / 2 - iy + ix; sy = ay + ax + H / 2 + Z * 5; break;
  }
}
// the camera's depth of a tile: larger = nearer the viewer (worldToScreen's sy grows with it at each even rotation)
inline int depth(int rot, int x, int y) { return rot == 0 ? y - x : rot == 2 ? -(x + y) : rot == 4 ? x - y : x + y; }
inline double cross(double ox, double oy, double ax, double ay, double bx, double by) {
  return (ax - ox) * (by - oy) - (ay - oy) * (bx - ox);
}
// convex hull (monotone chain) of n points, counter-clockwise in screen axes
inline Poly hull(std::vector<std::pair<double, double>> p) {
  std::sort(p.begin(), p.end());
  std::vector<std::pair<double, double>> h(2 * p.size());
  size_t k = 0;
  for (size_t i = 0; i < p.size(); ++i) {
    while (k >= 2 && cross(h[k - 2].first, h[k - 2].second, h[k - 1].first, h[k - 1].second, p[i].first, p[i].second) <= 0) --k;
    h[k++] = p[i];
  }
  for (size_t i = p.size() - 1, t = k + 1; i > 0; --i) {
    while (k >= t && cross(h[k - 2].first, h[k - 2].second, h[k - 1].first, h[k - 1].second, p[i - 1].first, p[i - 1].second) <= 0) --k;
    h[k++] = p[i - 1];
  }
  Poly out;
  for (size_t i = 0; i + 1 < k; ++i) {
    out.push_back(static_cast<float>(h[i].first));
    out.push_back(static_cast<float>(h[i].second));
  }
  return out;
}
// the occluders of a fall drawn at foot tile (tx, ty) from the water tile (sx, sy) behind it (the tile pass's neighbour in direction
// (rot + 2) & 7 or rot & 7, both one row farther from the camera): every land tile up to kReach tiles away that lies nearer the
// camera and lies wholly above the fall's top (its lowest corner above the highest corner of the source tile). Ground in front that
// reaches down to the fall's top (the bank the water pours over, the rocks beside the pool) keeps the original's picture (the spray
// drawn over it); only ground higher than the whole fall covers it, as a green on a ledge does.
constexpr int kReach = 6;
inline std::FILE *fall_log() {   // development: SG_P35B_FALL_LOG=<file>
  static std::FILE *f = std::getenv("SG_P35B_FALL_LOG") ? std::fopen(std::getenv("SG_P35B_FALL_LOG"), "w") : nullptr;
  return f;
}
static const int kCorner[4] = {7, 1, 3, 5};   // corner c at world (x + dx, y + dy): 7 (0, 0), 1 (1, 0), 3 (1, 1), 5 (0, 1)
inline int tile_top(Ctx &ctx, int x, int y) {
  int top = -1;
  for (int c : kCorner) top = std::max(top, corner_heights(ctx, x, y, c, 1));
  return top;
}
inline std::vector<Poly> fall_occluders(Ctx &ctx, int tx, int ty, int srcx, int srcy) {
  std::vector<Poly> out;
  const GameState &gs = ctx.gs();
  const int rot = gs.g_cameraRot;
  const int N = world_n(ctx);
  if (rot & 1 || tx < 0 || tx >= N || ty < 0 || ty >= N || srcx < 0 || srcx >= N || srcy < 0 || srcy >= N) return out;
  static const int kDx[4] = {0, 1, 1, 0}, kDy[4] = {0, 0, 1, 1};
  const int fall_top = tile_top(ctx, srcx, srcy);   // the fall's top: nearer ground must rise above it to cover the fall
  const double per = static_cast<double>(gs.f_w_4c2e00[0]) * gs.g_zoomScale / 4.0;   // pixels per level (worldToScreen)
  const bool twice = gs.unk_005a9cc0 != 0;
  const int d0 = depth(rot, tx, ty);
  for (int nx = tx - kReach; nx <= tx + kReach; ++nx)
    for (int ny = ty - kReach; ny <= ty + kReach; ++ny) {
      if (nx < 0 || nx >= N || ny < 0 || ny >= N || depth(rot, nx, ny) <= d0) continue;
      // land only: a tile the terrain does not draw (0x14), and water a fall pours into or a structure stands over (stream 0x0a,
      // water 0x11, wetland 0x12, a placed object's tiles 0x15 / 0x16: a bridge's span) never covers it
      const int8_t t = grid_ref(ctx, &GameState::g_tileType, nx * N + ny);
      if (t == 0x14 || t == 0x0a || t == 0x11 || t == 0x12 || t == 0x15 || t == 0x16) continue;
      int h[4], low = 1 << 30;
      for (int k = 0; k < 4; ++k) low = std::min(low, h[k] = corner_heights(ctx, nx, ny, kCorner[k], 1));
      if (low <= fall_top) continue;   // the whole tile above the whole fall; a bank down to the fall's top keeps the spray
      std::vector<std::pair<double, double>> pts;
      for (int k = 0; k < 4; ++k) {
        double sx, sy;
        ground_xy(gs, (nx + kDx[k]) * 1024.0, (ny + kDy[k]) * 1024.0, sx, sy);
        double px = sx, py = sy - (h[k] - 3) * per;
        if (twice) px = px * 2 - 400, py = py * 2 - 300;
        pts.emplace_back(px, py);
      }
      out.push_back(hull(std::move(pts)));
      if (fall_log()) {
        std::fprintf(fall_log(), "  occluder %d %d corners %d %d %d %d top %d poly", nx, ny, h[0], h[1], h[2], h[3], fall_top);
        for (const float v : out.back()) std::fprintf(fall_log(), " %.1f", v);
        std::fprintf(fall_log(), "\n");
      }
    }
  return out;
}
}  // namespace p35b
// the fall pieces drawn while one lives are hidden by the nearer ground that rises above the fall from (sx, sy) to foot tile
// (tx, ty) (switch at 0 only)
struct P35B_FallScope {
  std::vector<p35b::Poly> polys;
  bool set = false;
  P35B_FallScope(Ctx &ctx, int tx, int ty, int sx, int sy) {
    // development A/B: SG_P35B_FALL=1 draws as at 1 under any profile; SG_P35B_FALL_FILE=<path>: as at 1 while that file exists
    // (both arms in one run, the same camera)
    static const bool dev_orig = std::getenv("SG_P35B_FALL") && std::strcmp(std::getenv("SG_P35B_FALL"), "1") == 0;
    static const char *dev_file = std::getenv("SG_P35B_FALL_FILE");
    if (SG_DEFECT(g_defect_fall_over_ground) || dev_orig) return;
    if (dev_file) {
      if (std::FILE *f = std::fopen(dev_file, "rb")) {
        std::fclose(f);
        return;
      }
    }
    polys = p35b::fall_occluders(ctx, tx, ty, sx, sy);
    // development: SG_P35B_FALL_LOG=<file>: one line per fall drawn: foot tile, rotation, zoom, occluder count, source tile
    if (std::FILE *flog = p35b::fall_log()) {
      std::fprintf(flog, "foot %d %d rot %d zoom %d occluders %zu source %d %d\n", tx, ty, ctx.gs().g_cameraRot, ctx.gs().g_zoomScale,
                   polys.size(), sx, sy);
      std::fflush(flog);
    }
    if (polys.empty()) return;
    gfx_set_sprite_occluders(&polys);
    set = true;
  }
  ~P35B_FallScope() {
    if (set) gfx_set_sprite_occluders(nullptr);
  }
  P35B_FallScope(const P35B_FallScope &) = delete;
  P35B_FallScope &operator=(const P35B_FallScope &) = delete;
};
// P33C fix R (class a, a P32C regression): with the terrain tool, mainLoop hands a left / right press to the same key switch as
// '=' / '-' (ML+0xef0b..ML+0xef1e), so at 0 the switch above dropped every raise and lower click as well. The press path marks the
// key it made (g_terrain_press_key); the key dispatch head (ML+0xcf93) moves the mark to g_terrain_press_now for this pass, and
// the switch drops only the keys no press made. Not GameState: set and read inside one mainLoop pass on the sim thread.
thread_local bool g_terrain_press_key = false;
thread_local bool g_terrain_press_now = false;
// mainLoop.vacatedPanelClick (P9E fix 3, the widescreen HUD of G32 / P9B: "a click where a moved panel used to be still reaches
// that panel"): the frame's click dispatch sends a press below y 0x1b8 to the System menu (x < 0xc8: showSystemMenu's
// nearestMenuSpot) or, below y 0x1e0, to the panel handlers of g_panelListMode (ML+0xee40..ML+0xeec5); every other press takes
// the world path (ML+0xeef8, screenToTile's tile of ML+0xee20). With the HUD moved to the window's edges the course shows in
// the panels' 800-space places, but those tests still see the panels there. Fixed (0): a press the host marked as made in a
// vacated place (bridge.h view_click_vacated, view block word +4) takes the world path; with the word 0 (the original's window,
// every replay without it) nothing changes.
int g_defect_vacated_click = 1;
// mainLoop.panelTopRowsToWorld (P27A fix 4, the UI audit's class (c) item 3, log/roadmap/P27/P27A_notes.md section 10): the frame's
// click dispatch hands a press at x >= 0xc8 to the open panel only below y 0x1e0 (ML+0xee99), but the panel art starts at y 0x1da and
// the panels' hover tests (the same hit tests) light the buttons whose top reaches into rows 0x1da..0x1e0 (the golfer panel's and
// the staff panel's Golfers button, the list's -2 / -3 buttons, the Build palette's improvement and close buttons): a press there
// lights the button and acts on the course instead. Fixed (0): a press at y 0x1da..0x1e0, x >= 0xc8, that the open panel's own hit
// test claims (commands.h panel_claims_press) goes to the panel handler as one below 0x1e0 does.
int g_defect_panel_top_rows = 1;
// mainLoop.messageDismissBand (P27A fix 6, class (c) item 7): a press dismisses the message window when y < g_value587ea0 + 8 and
// |x - 0x190| < 0xc8 (ML+0xfd58..ML+0xfd80: x 0xc9..0x257), while tickMessage draws the window's frame at x 0xe8..0x287 (0x104..0x2a3
// with a golfer or picture, which also moves it 0x2c down): a press on the frame's right part acts on the course and one left of
// it, over the course, dismisses. Fixed (0): the press dismisses only on the drawn frame (fix 6's block in tick_frame.inc).
int g_defect_msg_dismiss_band = 1;
// mainLoop.cameraFollowOffGrid (P6A, 2026-11-03, Mariano's play-test): the camera tile pair gCameraTileX / gCameraTileY is
// clamped to 0..0x31 only by the key and edge-pan cases of H1 (ML+0xe971..ML+0xea73, ML+0xec6b..ML+0xec89); the writers that
// copy a golfer's, walker's, ball's or photo's world position >> 10 (P7 ML+0xbf38, H2 ML+0xf17c / ML+0xf994, updateGolfers'
// follow, the panel and key centring of commands.cpp, showBulletinBoard) and the hole / object offsets (Esc reset, key 0x30,
// showTutorialText, generateRandomCourse) store with no bound, so a golfer off the land takes the view off the map until the
// next H1 clamp, and every frame again. Fixed (0): each of those writers clamps the pair to the same 0..0x31 (camera_on_map).
int g_defect_camera_follow_offgrid = 1;
// updateGolfers.walkOffGrid (P6A): a golfer's walking step (updateGolfers+0x30f9..updateGolfers+0x3131) has no grid test, and
// the reversal turn (updateGolfers+0x29cf..updateGolfers+0x29e6) and the step overshoot can take a golfer past the edge
// (log/c3/c3fyl/audit123.md, U1). Fixed (0): the golfer's world position is clamped to 0..0xc7ff after the step.
int g_defect_golfer_walk_offgrid = 1;
// P28A (decision G30, model_defects#5 D5, log/roadmap/P28/P28A_notes.md: instruction addresses and evidence per site).
// updateGolfers.edgeNeighbourType: the ball's four edge-bit neighbour type tests (updateGolfers+0x3daf..updateGolfers+0x3e59)
// and the four green-neighbour tests (updateGolfers+0x41d9..updateGolfers+0x4246) read tile_type by index with no grid test.
// Fixed (0): a neighbour outside the grid has type 0x14, the answer tileBlocked gives the ball's own tile at updateGolfers+0x4dd.
int g_defect_golfer_edge_type = 1;
// mainLoop.edgeNeighbourType: mainLoop's neighbour type tests against a tile class (the type-0x11 water decoration and side
// sprites, the hole-point type-2 tests, the terrain edit's type 0x15 / 0x16 neighbour flag write) read tile_type (and
// tile_variant / tile_flags behind them) by index with no grid test. Fixed (0): a neighbour outside the grid has type 0x14
// (tileBlocked / typeAtPos, which mainLoop itself calls to skip outside neighbours elsewhere), so no class test passes there.
int g_defect_ml_edge_type = 1;
// mainLoop.edgeNeighbourNoFlags: the path-sprite neighbour mask and its (x - 1, y - 1) test (flag 1 << 12), the object kind 2
// mask (flag 1 << 12) and the flag-0x20 neighbour masks read tile_flags by index with no grid test, and the type-0x11 side
// sprites read a neighbour's g_tileWallMask the same way. Fixed (0): a cell outside the grid has no flags and a wall mask of 0
// (it does not exist; calcTileWallMask writes masks for inside cells only), so a path ends at the map edge. Orchestrator's
// choice (class b), vetoable alone, not derived from the code.
int g_defect_ml_edge_no_flags = 1;
// mainLoop.gridEdgeCorners (P35C, play-test 17 item 5): the terrain toolbar's grid (P1_grid) projects a cell's first corner with
// worldToScreen margin 0 and skips the cell when that corner is off the view, and projects the two far corners with margin 0
// without reading the result: a corner past the view's edge comes back as the flat-ground point (worldToScreen returns before the
// height term), so on raised ground the edges at the view's edges run to wrong points and the edge column the first corner left
// is missing. Fixed (0): the three corners are projected with kSpriteReachProject's margin (every corner keeps its height) and
// the cell is gated by the tile pass's tileToScreen alone. Drawing only.
int g_defect_ml_grid_edge = 1;
// true when tile (x, y) is outside the N x N grid (the P28A switches above)
inline bool p28_off(const Ctx &ctx, int x, int y) { return x < 0 || x >= world_n(ctx) || y < 0 || y >= world_n(ctx); }
// updateGolfers.partnerWaitBlocksTee (P20A, Mariano's "the golfers are stuck", log/roadmap/P20/P20A_notes.md): a golfer at its ball
// with swing counter 1 holds its shot while some golfer k of its hole (older or equal arrival order, not its partner, flag bit 10,
// more strokes) stands within 0x64 of the hole's tee (updateGolfers+0x380a..updateGolfers+0x383e). A k that already played and
// walked back to the tee to wait for a partner who has not (the partner-ahead flag L34, updateGolfers+0x893..updateGolfers+0x905)
// then never moves while the partner's own shot is held by the group ahead: the three wait for each other for good (the player's
// golfer and its partner in his save, identical in golf_clean.exe). Fixed (0): such a k is not held against the shot by the tee
// test; the ball, target and line-of-fire tests of the same loop still apply to it.
int g_defect_partner_wait_blocks_tee = 1;
// updateGolfers.playerWaitBlocksCourse (P20A fix 2, Mariano's choice "let groups play through while my golfer waits",
// log/roadmap/P20/P20A_notes.md section 8): while the shot of the player's own golfer (g_playerGolfer) is handed to the player and not
// played (target x +0xcc = -1, written at updateGolfers+0x34a8), that golfer and its partner on its hole stay 0-stroke golfers of an
// arrival order older than every group, so the tee queue count of every later golfer of that hole (updateGolfers+0x1018..
// updateGolfers+0x107e, L38) never returns to 0 and nobody tees up (updateGolfers+0x2581..updateGolfers+0x2595); the near-tee test of
// the look-ahead before a shot (updateGolfers+0x380a..updateGolfers+0x383e) holds the shots of the groups on the tee the same way
// when he holds a shot near it. The course stops for as long as he does not play (golf_clean.exe: 5 game months). Fixed (0): outside
// a championship or pro match (g_flags 0x4200000 clear, the same condition as the original's own player-pair exception at
// updateGolfers+0x1044..updateGolfers+0x1060) those two golfers are not counted in the tee queue and not held against the shots of
// others by the near-tee test; the ball, target and line-of-fire tests stay, so nobody hits into him.
int g_defect_player_wait_blocks_course = 1;
// updateGolfers.playerPairBlocksShots (P23B, Mariano's "golfers stuck" on release 342dd9e8, log/roadmap/P23/P23B_notes.md section 5):
// with playerWaitBlocksCourse at 0 the groups behind tee up while the player's shot waits, but the look-ahead before every shot
// (updateGolfers+0x3844..updateGolfers+0x395f) still holds a golfer while a golfer k of its hole (older or equal arrival order, not its
// partner, flag bit 10, more strokes) has its ball within 0x25 of the golfer's target, stands within 0x25 of it, or stands nearer the
// tee than the golfer is to its target within 0xaaaaaaa of its aim: the player's pair ahead on the hole (his partner at the cup waiting
// for his handed-over shot, his golfer in the line of fire) holds every group behind for as long as he does not play, as in
// golf_clean.exe with his Ocean's Edge 25 July 2004 save. Fixed (0): outside a championship or pro match, while his shot is handed
// over and not played (P20A2_PlayerHolds), he and his partner hold nobody's shot by those three tests. The ball has no collision
// with golfers (the only reaction, updateGolfers+0x490b..updateGolfers+0x497f, is a thought for golfers of other holes).
int g_defect_player_pair_blocks_shots = 1;
// updateGolfers.playerPairBlocksWalkers (P36B fix 1, Mariano's play-test 18 item 2 "a lot of times i see people waiting for no apparent
// reason", log/roadmap/P36/P36B_notes.md section 5): with playerWaitBlocksCourse and playerPairBlocksShots at 0 the tee queue count and
// the look-ahead let the groups behind play while the player's shot is handed over and not played, but the walking step's in-the-way
// test (updateGolfers+0x2785..updateGolfers+0x285d: another golfer within half a tile in the walking direction, or the partner, gives
// wait -4 - Random::range(4)) has no exception for his pair: standing on the tee box with the shot pending, his golfer and his partner
// stop every golfer that walks into the tee box, and the hole jams (his Jurassic Springs save, unattended at speed 4 for 6144 ticks:
// hole 6 bump 9114 ticks per finished hole, 10 golfers queued at the end). Fixed (0): outside a championship or pro match, while his
// shot is handed over and not played (P20A2_PlayerHolds), he and his partner are not in anybody's way in that test (golfers walk
// through them, as balls fly through golfers), and they do not count among the hole's golfers still to tee off there (L44).
int g_defect_player_pair_blocks_walkers = 1;
// updateGolfers.partnerWaitsBallRest (P36B fix 4, Mariano's play-test 18 item 2, his pick A of log/roadmap/P36/P36B_notes.md section
// 10): the two golfers of a pair take turns, and each waits until its partner's ball has come to rest: the partner-ahead flag L34
// (updateGolfers+0x8e4..updateGolfers+0x905: the golfer played, flag 0x400, its partner has not) sends it back to its hole's tee to
// stand there; L74 (updateGolfers+0x910..updateGolfers+0x9a2) holds the golfer nearer the cup; the tee order (updateGolfers+0x2416..
// updateGolfers+0x2447) holds the lower index while the partner has no stroke (strokes count at the ball's stop); the green rule
// (updateGolfers+0x24e4..updateGolfers+0x2576) and the watching rule (updateGolfers+0x13df..updateGolfers+0x14e2) stand it while
// the partner's shot is under way. That turn-taking is 57 % of the time golfers stand on his Jurassic Springs save. Fixed (0):
// outside a championship or pro match (g_flags 0x4200000 clear) each of these lets go once the partner's ball is in the air or
// rolling (flag bit 18) and the watching rule does not stand it: the pair's shots overlap (golfer time per hole 19.5 -> 17.9
// Time-to-Play minutes measured). Strokes, scores and the player's handed-over shot are untouched (it still waits for him); balls
// and golfers have no collision.
int g_defect_partner_waits_ball_rest = 1;
// updateGolfers.preShotGateByHole (P36B fix 4, his pick B): at its ball, off the green, a golfer swings on a tick where
// Random::range(hole + 4) is 0 (updateGolfers+0x373a..updateGolfers+0x375d), so the pause before a shot grows with the hole number
// (a mean of 5 ticks per try on hole 1, 18 on hole 14). Fixed (0): Random::range(5), hole 1's draw, on every hole.
int g_defect_pre_shot_gate_by_hole = 1;
// mainLoop.treesHiddenFarZoom (P11A fix 1, Mariano's play-test 5: "furthest zoom is not rendering trees"): the tile pass of
// mainLoop P3 queues the sprite of a tile whose type class byte (g_tileTypeAttrs + 0x26 of its 48-byte row) is 0xd (the
// forest trees, tropical trees and palms) only while g_zoomScale > 1 (ML+0x4aba cmp / ML+0x4ac1 jle past the whole block), so
// at the furthest zoom (1) those tiles show their ground alone. Fixed (0): the block also runs at zoom 1 (the same sheets, at the
// display scale shade * zoom, a quarter of zoom 4's size), and a tree enters the sprite queue there only while the queue holds
// fewer than kFarTreeQueueLimit entries, so the golfers, animals and balls queued after the tile pass (ML+0x5fb7 onwards) keep
// their places under enqueueSprite's 0x3ff cap. At zoom 2 and 4 nothing changes.
int g_defect_far_zoom_trees = 1;
constexpr int kFarTreeQueueLimit = 0x280;
// mainLoop.zoomInStopsAt4 (P11A fix 2, Mariano's play-test 5: "another level of zoom closer"): key 0x7a ('z', and the radial menu's
// zoom-in button, which posts it) doubles g_zoomScale only while it is below 4 (ML+0xd372), so 4 is the closest of the three levels
// 1 / 2 / 4. Fixed (0): it doubles up to 8, one level closer (key 0x78 halves 8 back to 4). The projection (worldToScreen, screenToTile,
// g_tileStep4c2840 = zoom * 8), the sprite scale (shade * zoom) and the labels are linear in the zoom; the places that assume 4 is the
// largest value take zoom 8 like this (each reached only at zoom 8, so at 1 nothing changes):
//   - the golfer sprite's shade (ML+0x60e5..ML+0x60ff: 4 at zoom <= 4, else 2) stays 4, so golfers grow with the course;
//   - P4_objectLabel's object phrase (drawn at zoom 4 only, ML+0x6753) is drawn at zoom 8 too, its second line at zoom 4's offset;
//   - updateTerrainRender (terrain_dll.cpp): at zoom 8 a camera move renders the whole terrain (the strip renderer's windows have no
//     zoom-8 size); Terrain::setZoomLevel(8) (terrain_gl_setup.cpp) halves level 4's ortho window and keeps level 4's zoom shift
//     (strip / path / cull windows: a superset of the zoom-8 view); rebuildTileData's start row / column 4 / zoom * 13 uses zoom 4's 13.
int g_defect_zoom_in_max4 = 1;
// mainLoop.toolForcesZoom4 (P11E, Mariano 2026-11-05: the build tools at P11A's zoom 8 too): while a build tool or a selection is
// held, P4_queueAndSelection stores g_zoomScale = 4 every frame (ML+0x6620 / ML+0x6626), so picking a tool at another zoom returns
// to 4 and 'z' has no lasting effect while it is held; key 0x78 ('x', zoom out) drops the held tool (ML+0xd35e / ML+0xd363).
// Fixed (0): a held tool keeps every zoom (P11E: 8, reached only with mainLoop.zoomInStopsAt4 = 0; P13A, Mariano's play-test 6
// item 6 "changing zoom deselects what's currently selected": 1 and 2 too) and 'x' never drops the tool. The build
// paths at zoom 8: the clicks and the cursor tile go through screenToTile, the placement rectangle, markers, tee line and
// footprints through worldToScreen / tileToScreen / addSprite, all linear in the zoom. Two drawing rules of zoom 4 are kept
// apart at zooms 8, 2 and 1 (with a tool held reached only at 0): the translucent tile box takes (z, z, 4) (tick_frame.inc
// P11E_BoxScale) and the translucent sprites take their offsets at the display scale (gfx_scene.cpp draw_alpha).
int g_defect_tool_zoom4 = 1;
// mainLoop.cameraStepFixed4 (P11F): the arrow keys (ML+0xe971..ML+0xea73) step the camera 4 tiles at every zoom; fixed (0): 2 tiles
// at zoom 8 (the screen distance 4 tiles cover at zoom 4). The edge scroll (ML+0xebb4..ML+0xec89, 1 tile per frame) is unchanged.
int g_defect_camera_step4 = 1;
// mainLoop.arrowKeyHeldRepeat (P19A, Mariano's play-test 7 item 1 "i press once and moves a little, then it proceeds to keep moving,
// i want just a constant moving"): H1 takes the key code once per frame (setDirtyFlagA's take-and-clear of gDirtyFlagA, which
// rootWin_onKey(0, vk) writes on every WM_KEYDOWN of a key with no character, the arrows' path), so a held arrow moves the camera once
// on its press and again only when Windows' key repeat starts after its delay, then once per frame. Fixed (0): rootWin_onKey also keeps
// an arrow (0x25..0x28, no modifier) in the held-key word g_char83821c (rootWin_onChar writes it for character keys, rootWin_onChar0
// clears it on every key-up; no code of the exe reads it), and a frame whose key code is 0 while that word holds an arrow takes that
// arrow, so the camera steps once per frame from the press until the release (commands.cpp root_handler, semantic.cpp apply_key).
// P22A (play-test 9 item 1, a light tap moved one, two or three steps by where it fell against the frame clock): the held arrow
// steps again only from the first frame head kArrowHoldMs (250 ms) or more after its key-down time (Command::t, kept in view block
// word +32 by root_handler; bridge.h view_arrow_held_due), so a tap is one step and a held key scrolls one step per frame after that.
int g_defect_arrow_held_repeat = 1;
// mainLoop.pKeyPausesGame (P21A, Mariano's play-test 8 item 1 "letter P should pause the game, change pathway to another hotkey"):
// H1's key switch (ML+0xcfec, byte table ML+0x1235c) maps the capital P (0x50) to the pause toggle (case ML+0xda60: xor [g_flags], 4,
// the bit mainLoop's pause label tests) and the lower-case p (0x70) to the Improvements palette (case ML+0xd3c0: g_panelListMode = 0,
// g_buildPanelSlot = 1; Pathway is its first item). The game clears caps lock (capsLockOff), so a plain P press is the palette.
// Fixed (0): both p and P take the pause case, and the Improvements palette moves to the free key a (0x61, and A 0x41), the first
// free letter of "path"; no key code of the original's table (ML+0x1235c) is taken.
int g_defect_p_key_pauses = 1;
// mainLoop.arrowStepAfterDraw (P21B, Mariano's play-test 8 item 2 "there's a major delay in between me pressing the arrow key to
// move the camera until it actually does"): a frame draws its picture (updateTerrainRender ML+0xf13, the sprites) with the camera
// of the frame's start, then takes the input (the P6 / H1 pumps, setDirtyFlagA ML+0xcb1d) and steps the camera for an arrow at the
// key switch (ML+0xe971..ML+0xea73) after that picture: a key-down that arrives during the frame wait shows only in the picture of
// the frame after the next one. Fixed (0): an arrow key-down a live host took at the frame head (bridge.h view block words +12 / +16)
// goes to rootWin_onKey before the picture and the camera steps there (the same step: direction, 4 tiles or P11F's 2, clamp), as does
// P19A's held arrow; at most one step per frame (an arrow H1 then takes in a frame that stepped already is dropped, as a second
// key-down within one frame overwrites the first in the original's gDirtyFlagA).
int g_defect_arrow_step_after_draw = 1;
// renderSpriteQueue.labelUnderLaterSprites (P22C item 6, Mariano's play-test 9 "shadows cut text off": Gary Golf's name label half
// hidden by a tree): drawQueuedSprite draws a golfer's name label (drawCenteredText and the two Widget_fillRect bars) right after the
// golfer's own sprite, inside renderSpriteQueue's walk in depth order, so every entry after it (a tree, a water spout, a cliff face, a
// golfer in front) is drawn over the label. Fixed (0): the labels of the walk are drawn after its last entry, in queue order, each with
// the name text its entry built (gfx_scene.cpp b_queue_label / b_queue_end): the golfer sprite keeps the original's occlusion, its
// label is never covered by a sprite. Drawing only.
int g_defect_label_under_sprites = 1;
// mainLoop.zoom8Font (P13C item 3, Mariano 2026-11-06 "on the closest zoom level font changes"): four label sites pick the draw font
// by `g_zoomScale > 4` (the golfer label queue_label ML+0x7518 / ML+0x7aa8 callers, P4_ballLabelDraw ML+0x71b7..ML+0x71c1): zoom 8
// (P11A, unreachable in the original) took g_font51b360 where zoom 4 takes g_fontRef_519fd8. Fixed (0): the choice uses min(zoom, 4).
// P26D item 8 (Mariano's play-test 13 "names of the stuff on closest zoom are a different font and size"): the fifth site, drawWalkers'
// staff / player name label (drawWalkers+0x3e0..drawWalkers+0x3ef), follows the same switch.
int g_defect_zoom8_font = 1;
// mainLoop.rightClickKeepsTool (P13A, Mariano's play-test 6 item 11 "right click to deselect"): a right click on the course
// (Ph_0041ecbb, button word 2, ML+0xfbac) drops an object tool (f_w_4c2848[3] = -1, ML+0xfbe3) but leaves a terrain tool
// (f_w_4c2848[0]) held and goes on to the right-click undo: the first click marks the tile (buildToolLabel, queueMessage), a
// second on the same tile runs demolishObject. Fixed (0): with a terrain tool held, a right click on a tile demolishObject has
// nothing to act on (tick_frame.inc P13A_UndoTarget: blocked, a 0x8000 tile, or no placed object and build code 0xff) drops the
// tool and does nothing else; on any other tile, and with no tool held, the original's mark / undo runs.
int g_defect_rclick_keeps_tool = 1;
// mainLoop.hKeyShotStyleFirst (P13A, Mariano's play-test 6 item 9 "H to open hole is not working (at least on sandbox mode), or
// maybe not working reliably"): H1's key 0x68 ('h', ML+0xd119) opens the hole (openHole) unless g_panelListMode is 3, the mode
// updateGolfers sets while the player's golfer waits for its shot (updateGolfers+0x35c2); there 'h' sets g_playerShotStyle = 3
// and the hole stays shut. Fixed (0): in mode 3 'h' opens the hole when openHole would open it (tick_frame.inc P13A_HoleReady),
// else it sets the shot style as before.
int g_defect_h_shot_style = 1;
// Tile::render.fairwayStripesFixed (P13A, Mariano's play-test 6 item 8 "tab on fairway should rotate it as well"): Tab (H1 key 9,
// ML+0xd42a) adds 1 to the orientation word g_ui_5a34e4[3] and the terrain paint stores 12 + orientation % 12 into each painted tile's
// g_tileVariant (H2_Terrain, ML+0x10fdd / ML+0x1108b), which syncTiles hands to Terrain::setType as the tile's rotation. Terrain.dll
// reads that rotation for the sand trap (four rake textures by rotation % 4), the green (bit 0x80) and water, never for the fairway
// (types 2 / 3: one texture set, vertical mowing stripes), so Tab does nothing visible on a fairway. Fixed (0): Tile::render
// (terrain_render.cpp) draws a fairway tile whose orientation is odd with every face's stripes turned 90 degrees.
int g_defect_fairway_stripes_fixed = 1;
// mainLoop.undoButtonHelpOnly (P13A item 12, Mariano's option C): the palettes' Undo button (the bulldozer) only shows the right-click
// undo help; fixed (0): it toggles the game's dormant bulldoze mode (g_flags bit 27: a left click runs demolishObject on the cursor
// tile), a right click on the course clears it (tick_frame.inc Ph_0041ecbb), and the tips call the button "Bulldoze"
// (gfx_text.cpp undo_name; commands_panels.inc undo_button).
int g_defect_undo_button_help = 1;
// mainLoop.shotButtonsReadBallTile (P22B, Mariano's play-test 9 item 2 "it lets me choose all shot types but i'm on regular grass"):
// drawGolferActionPanel decides greyed (lie class > 0, g_tileTypeAttrs +0x22) or enabled shot buttons 5..7 from
// typeAtPos(ball x +0xd4, golfer y +0x04) (drawGolferActionPanel+0x167..+0x175: the ball's x with the golfer's y), while the "Lie:"
// text of the same panel (mainLoop's panel text block) reads typeAtPos(+0xd4, +0xd8), the ball's tile. After a shot the two differ, so
// the buttons follow another tile than the lie shown (rough shown, buttons enabled). Fixed (0): the buttons read the ball's tile too.
int g_defect_shot_lie_mixed_tile = 1;
// updateGolfers.foreignGreenAutoPutt (P22B, Mariano's play-test 9 item 4 "if i land on another hole's green that's disconnected from the
// current hole it should let me select direction and type of hit instead of doing it automatically"): updateGolfers hands the shot to
// the player's golfer (updateGolfers+0x3419..updateGolfers+0x346b: flag bit 9, ball tile type not 1, ball at least 0x19 from the cup)
// only off the green; a ball on any green, also another hole's, takes the automatic shot towards the current hole's cup
// (prepareShot with x == -1, updateGolfers+0x35f7..updateGolfers+0x3610). Fixed (0): a ball on a green tile that is not connected
// (8 neighbours, type 1 tiles) to the green of the current hole's cup is handed to the player like a ball off the green.
int g_defect_foreign_green_auto = 1;
// mainLoop.holeLineEndsOnScreen (P19B, Mariano's play-test 7 item 5 "line of the trajectory of the ball is still not rendering
// properly on the edges of the screen if the hole is not all visible"): mainLoop draws a hole's shot-line segment only when
// worldToScreen (ML+0x41c4) and tileToScreen (ML+0x41f1) find both its ends on screen. Fixed (0): a segment with an end off screen
// is drawn too, clipped at the view's edge (tick_frame.inc P19B_HoleLineEnds).
int g_defect_hole_line_ends_on_screen = 1;
// mainLoop.simFotoWideFrame (P31D, G33 item 5 "Widen to the view", log/roadmap/P31/P31D_notes.md): the SimFoto event's box
// (P5_simfoto, ML+0x8ffb..ML+0x9542) is clamped into the original's 800 screen (x0 0x10..0x2bc, x1 0x64..0x310, ML+0x90e6 /
// ML+0x9107), so for a golfer in a side column of the wide view the frame lands on the 800 frame's edge, not around the golfer.
// Fixed (0): the x clamps move out by E = (view width - 800) / 2 on each side and the frame's border, frame lines and caption are
// also drawn into the wide scene (the render-only drop simfoto.wide). y unchanged. The box only feeds drawing and the dropped
// saveCourseJpg: no GameState object depends on it.
int g_defect_simfoto_wide_frame = 1;
int32_t simfoto_wide_margin(Ctx &ctx) {
  // P31D pictures only: SG_P31D_STAGE bit 8 runs the original's clamps whatever the switch holds (the before picture)
  static const bool stage_orig = std::getenv("SG_P31D_STAGE") && (std::atoi(std::getenv("SG_P31D_STAGE")) & 256);
  return SG_DEFECT(g_defect_simfoto_wide_frame) || stage_orig ? 0 : (view_width(ctx) - 800) / 2;
}
int32_t hover_tip_dwell();  // P6B hoverTip.tenFrameDwell (gfx_hud.cpp): the HUD panels' tip dwell, 10 frames in the original
inline int16_t cash_word(int32_t v) {
  if (SG_DEFECT(g_defect_cash_low_word)) return static_cast<int16_t>(v);
  return static_cast<int16_t>(v > 0x7fff ? 0x7fff : v < -0x8000 ? -0x8000 : v);
}
// P6A: keep the camera tile pair on the 50 x 50 grid (mainLoop.cameraFollowOffGrid = 0); no change at 1
// P31A (mainLoop.cameraBoundByZoom = 0): the range is camera_range's (bridge.h), whatever cameraFollowOffGrid says
void camera_on_map(Ctx &ctx) {
  if (SG_DEFECT(g_defect_camera_follow_offgrid) && SG_DEFECT(g_defect_camera_bound_zoom)) return;
  GameState &g = ctx.gs();
  const CameraRange r = camera_range(ctx);
  g.gCameraTileX = g.gCameraTileX < r.lo[0] ? r.lo[0] : g.gCameraTileX > r.hi[0] ? r.hi[0] : g.gCameraTileX;
  g.gCameraTileY = g.gCameraTileY < r.lo[1] ? r.lo[1] : g.gCameraTileY > r.hi[1] ? r.hi[1] : g.gCameraTileY;
}
namespace tk {

// ------------------------------------------------------------------------------------------------ negative controls
// One switch per registered function (mode 0 = the correct body), read through SG_NEGCTL; the modes (wrong bodies) are
// listed in log/p3/p37a_notes.txt section "Negative controls" and tested in port/test/test_tick.py. `frame` belongs to the
// pseudo-function tick.frame (the frame step itself and the fallbacks).
namespace neg {
int g_negctl_cell_at = 0;
int g_negctl_tile_to_screen = 0;
int g_negctl_screen_to_tile = 0;
int g_negctl_sprite_queue = 0;
int g_negctl_snapshot_tables = 0;
int g_negctl_build_scene_sprites = 0;
int g_negctl_draw_world_objects = 0;
int g_negctl_draw_walkers = 0;
int g_negctl_flush_course_info = 0;   // 1 = the path is not cleared, 2 = the info file says Holes = g_holesPlusOne
int g_negctl_start_message = 0;
int g_negctl_append_cents = 0;
int g_negctl_log_tick = 0;
int g_negctl_tick_message = 0;
int g_negctl_draw_window = 0;
int g_negctl_draw_thought_bubble = 0;
int g_negctl_update_golfers = 0;
int g_negctl_view_to_local = 0;
int g_negctl_view_from_parent = 0;
int g_negctl_save_game = 0;
int g_negctl_draw_labels = 0;
int g_negctl_draw_money_labels = 0;
int g_negctl_match_update = 0;
int g_negctl_membership_report = 0;
int g_negctl_can_afford = 0;
int g_negctl_rank_value = 0;
int g_negctl_draw_home_layout = 0;
int g_negctl_play_habitat_sound = 0;
int g_negctl_add_score = 0;
int g_negctl_hire_employee = 0;
int g_negctl_set_dirty_flag_a = 0;
int g_negctl_set_dirty_flag_b = 0;
int g_negctl_frame = 0;
int g_negctl_show_tutorial_text = 0;  // 1 = the page is not advanced after its message, 2 = no startMessage, 3 = no camera move,
                                      // 4 = the camera cache reset g_cachedCameraTileX = -1 skipped
}  // namespace neg
using namespace neg;

// ------------------------------------------------------------------------------------------------ dispatch
namespace {

// Dropped callees: drawing, audio, display, pumps and waits (re/analysis/sim/time_model.md section 8: "drop every wait loop
// and its clock reads ... audio and video timing"). Census = log/p3/p37a_simwrites.py over the callee's call closure.
struct Drop {
  const char *name, *why;
};
const Drop kDrops[] = {
    // phase 5 (re/PHASE5_GFX_PLAN.md): gameMain's title-screen setup, called by the boot; its loads and crops fill render objects only
    // and its intro movie is the video backend's (with a gfx backend attached, gfx.cpp draws its part)
    {"showTitleScreen", "title button and window-frame sprites, intro movie (renderer / video)"},
    {"Image_draw", "blit of an image onto a surface"},
    // phase 4, the QUIT block: no sim-class write in either call closure (log/p3/p37a_simwrites.py on both)
    {"PaletteCache::dtor", "the frame's palette cache (renderer)"},
    {"Image_ctor", "frame Image object (renderer)"},
    {"Image_dtor", "frame Image object (renderer)"},
    {"Sprite_draw", "sprite blit"},
    {"Surface_blit", "surface blit"},
    {"Surface_blit3", "surface blit"},
    {"Surface_blitThunk", "surface blit"},
    {"Surface_drawScaled", "scaled blit"},
    {"Surface_drawScaled2", "scaled blit"},
    {"Surface_drawSprite", "sprite blit"},
    {"Surface_stretchBlit", "stretch blit"},
    {"Surface_createFull", "surface allocation (renderer)"},
    {"Surface_createFull_fwd1", "surface allocation (renderer)"},
    {"Surface_free", "surface release (renderer)"},
    {"Text_put", "text rendering"},
    {"Widget_fillRect", "rectangle fill"},
    {"Widget_writePixel", "pixel write"},
    {"Window::refreshFull", "window repaint"},
    {"drawCenteredText", "text rendering"},
    {"drawCenteredText2", "text rendering"},
    {"drawTextWithColor", "text rendering"},
    {"drawTextSurface", "text rendering"},
    {"drawCircle", "circle drawing"},
    {"drawFrameR3", "frame drawing"},
    {"drawMapLine", "minimap line"},
    {"drawMinimapSegment", "minimap segment"},
    {"drawScaledSprite421b60", "scaled sprite blit"},
    {"drawScaledSprite421b90", "scaled sprite blit"},
    {"fillWindowRect", "rectangle fill"},
    {"lineTo", "line drawing"},
    {"setLinePoint", "line drawing"},
    {"setDrawTarget", "draw target / font selection (renderer)"},
    {"loadImage", "image file into a surface (renderer)"},
    {"saveCourseJpg", "screenshot file"},
    {"capsLockOff", "keybd_event (time_model.md section 8)"},
    {"emptyFunc_4a0890", "empty function"},
    {"waitTicks", "wait loop (pacing)"},
    {"drawFrameS2", "frame drawing"},
    {"drawPanelBackdrop", "label backdrop and text drawing"},
    {"drawStringRightAt", "text drawing"},
    {"Widget_setQuad", "text colour setup (renderer)"},
    // phase 3.7a, the COMMAND segments: census 0 sim writes and no sim address in the closure (log/p3/p37a_simwrites.py)
    {"drawTerrainToolbar", "toolbar drawing (3.7b write audit: its hover words are read only by drawing; result unused)"},
    {"flushInput", "modal stack push and focus clear of the window tree (UI)"},
    {"Widget_readPixel", "a pixel read of the root surface; its only use gates a dropped sprite draw (P5's zoom, ML+0x95d3)"},
};

using Local = int (*)(TkBase &, const uint32_t *, int);
struct Entry {
  int kind = 0;  // 1 drop, 2 local, 3 registry
  Local local = nullptr;
  const FuncDef *f = nullptr;
};

std::map<std::string, Local> &locals() {
  static std::map<std::string, Local> m;
  return m;
}
// Fallbacks: bodies of another child's functions that the frame reaches every frame, used only while the registry has no
// function of that name (the owner's registered function wins as soon as it exists).
std::map<std::string, Local> &fallbacks() {
  static std::map<std::string, Local> m;
  return m;
}

// the callee counts of this thread (sg_tick_callee_counts). P38G: keyed by the name literal's address, so a call costs no string
// (two literals of one name are summed by name when read: tick_callee_counts)
thread_local std::unordered_map<const char *, uint64_t> t_counts;
thread_local std::unordered_map<std::string, uint64_t> t_counts_off;   // SG_P38G_OFF=1: by name, as before P38G
thread_local std::vector<std::string> t_stop_path;  // the callees an exception left, innermost first (TkBase::call)

Entry resolve(const char *name) {
  Entry e;
  for (const Drop &d : kDrops)
    if (std::strcmp(d.name, name) == 0) {
      e.kind = 1;
      return e;
    }
  auto it = locals().find(name);
  if (it != locals().end()) {
    e.kind = 2;
    e.local = it->second;
    return e;
  }
  const int i = registry_find(name);
  if (i >= 0) {
    e.kind = 3;
    e.f = registry_at(i, nullptr);
    return e;
  }
  auto fb = fallbacks().find(name);
  if (fb != fallbacks().end()) {
    e.kind = 2;
    e.local = fb->second;
  }
  return e;
}

}  // namespace

namespace {
thread_local FrameIo *t_io = nullptr;
}  // namespace
FrameIo &cur_io() {
  static thread_local FrameIo none;
  return t_io ? *t_io : none;
}
IoScope::IoScope(FrameIo &f) : prev(t_io) { t_io = &f; }
IoScope::~IoScope() { t_io = prev; }

TkBase::TkBase(Ctx &c) : ctx(c), io(cur_io()), kBuf(GS(g_textBuffer)) {}


int TkBase::call(const char *name, const uint32_t *a, int n) {
  static std::mutex mu;
  static std::unordered_map<const void *, Entry> cache;  // keyed by the name literal's address
  // P38G: this thread's copy of the entries it has looked up (an entry in `cache` never changes once made), so a call takes no lock
  thread_local std::unordered_map<const void *, Entry> t_cache;
  Entry e;
  if (auto ht = p38g_off() ? t_cache.end() : t_cache.find(name); ht != t_cache.end()) {
    e = ht->second;
  } else {
    std::lock_guard<std::mutex> g(mu);
    auto it = cache.find(name);
    if (it == cache.end()) it = cache.emplace(name, resolve(name)).first;
    e = it->second;
    t_cache.emplace(name, e);
  }
  if (p38g_off()) ++t_counts_off[name];
  else ++t_counts[name];
  // development: SG_TICK_CALL_LIMIT=<n> stops a run after n callee calls on this thread (a loop that never ends, with its
  // callee path in sg_run_frames_detail)
  static const long long call_limit = std::getenv("SG_TICK_CALL_LIMIT") ? std::atoll(std::getenv("SG_TICK_CALL_LIMIT")) : 0;
  static thread_local long long calls = 0;
  if (call_limit > 0 && ++calls > call_limit) {
    calls = 0;
    throw NoReturn{"SG_TICK_CALL_LIMIT reached"};
  }
  // development: SG_TICK_TRACE=<file> writes every callee call (name, first arguments), flushed, to find a hang
  static FILE *trace_fh = std::getenv("SG_TICK_TRACE") ? std::fopen(std::getenv("SG_TICK_TRACE"), "w") : nullptr;
  if (trace_fh) {
    std::fprintf(trace_fh, "%s", name);
    for (int k = 0; k < n && k < 4; ++k) std::fprintf(trace_fh, " %d", static_cast<int>(a[k]));
    std::fprintf(trace_fh, "\n");
    std::fflush(trace_fh);
  }
  // development: SG_TICK_DUMP=<name>;<dir> writes the GameState before (<dir>/<name>_<k>.pre.bin, plus the arguments in
  // .args) and after (.post.bin) every call of that callee, for an emulator comparison against the original
  static const char *dump = std::getenv("SG_TICK_DUMP");
  std::string dump_dir;
  int dump_k = -1;
  if (dump && std::strncmp(dump, name, std::strlen(name)) == 0 && dump[std::strlen(name)] == ';') {
    static std::map<std::string, int> seq;
    dump_k = seq[name]++;
    dump_dir = std::string(dump + std::strlen(name) + 1) + "/" + name + "_" + std::to_string(dump_k);
    if (FILE *f = std::fopen((dump_dir + ".pre.bin").c_str(), "wb")) {
      std::fwrite(ctx.bytes(), 1, sizeof(GameState), f);
      std::fclose(f);
    }
    if (FILE *f = std::fopen((dump_dir + ".args").c_str(), "w")) {
      for (int k = 0; k < n; ++k) std::fprintf(f, "%u\n", a[k]);
      std::fclose(f);
    }
  }
  struct PostDump {
    Ctx &c;
    const std::string &d;
    ~PostDump() {
      if (d.empty()) return;
      if (FILE *f = std::fopen((d + ".post.bin").c_str(), "wb")) {
        std::fwrite(c.bytes(), 1, sizeof(GameState), f);
        std::fclose(f);
      }
    }
  } post{ctx, dump_dir};
  // the callee path of a stop (innermost first), for sg_run_frames_detail
  struct Path {
    const char *n;
    ~Path() {
      if (std::uncaught_exceptions() > 0 && t_stop_path.size() < 12) t_stop_path.push_back(n);
    }
  } path{name};
  const DevProfScope dev_prof(name);   // P38G development (SG_P38G_FUNCPROF)
  // P9B: a callee that draws one HUD panel opens that panel's pixel layer around the call (gfx.h; inert without the app's capture)
  GfxLayerScope hud_layer(ctx, g_gfx_hud_capture.load(std::memory_order_relaxed) != 0 ? gfx_layer_of_callee(name) : -1);
  switch (e.kind) {
    case 1:
      // phase 5: an attached graphics backend draws what the drop drew; the frame keeps the drop's result
      if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {
        gfx_drop(ctx, name, a, n);
        int32_t v = 0;
        if (gfx_take_value(ctx, v)) return v;   // roadmap S3.7: a measuring drop (Text_put)
      }
      return 0;
    case 2:
      return e.local(*this, a, n);
    case 3: {
      uint32_t v[16] = {0};
      for (int k = 0; k < n && k < 16; ++k) v[k] = a[k];
      Ret r;
      e.f->fn(ctx, v, r);
      return static_cast<int>(r.eax);
    }
  }
  throw MissingResource{name};
}

int TkBase::Rnd(int n) {
  return static_cast<int>(random_range(ctx.mem.ref<uint32_t>(GS(g_rng)), static_cast<uint32_t>(n)));
}

int TkBase::Itoa(int v, uint32_t buf, int radix) {
  // the static CRT's _itoa: radix 10 and a negative value write '-' and the magnitude; other radixes the unsigned value
  char tmp[40];
  int k = 0;
  uint32_t u = static_cast<uint32_t>(v);
  bool neg = false;
  if (radix == 10 && v < 0) {
    neg = true;
    u = 0u - u;
  }
  do {
    const uint32_t d = u % static_cast<uint32_t>(radix);
    tmp[k++] = static_cast<char>(d > 9 ? d - 10 + 'a' : d + '0');
    u /= static_cast<uint32_t>(radix);
  } while (u != 0);
  uint32_t p = buf;
  if (neg) ctx.mem.store<uint8_t>(p++, '-');
  while (k > 0) ctx.mem.store<uint8_t>(p++, static_cast<uint8_t>(tmp[--k]));
  ctx.mem.store<uint8_t>(p, 0);
  return static_cast<int>(buf);
}

int TkBase::Root() { return G32(GS(f_stateptr_4c1570)); }

void TkBase::idiv64(int &edx, int &eax, int d) {
  const int64_t num = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(edx)) << 32) | static_cast<uint32_t>(eax));
  if (d == 0) throw DivideError{};
  const int64_t q = num / d;
  if (q > INT32_MAX || q < INT32_MIN) throw DivideError{};
  eax = static_cast<int>(q);
  edx = static_cast<int>(num % d);
}

uint32_t TkBase::clock_site(int32_t site) {
  if (io.live) return io.live->clock_site(site);  // roadmap S7.1: a live run
  if (io.clock)
    for (const auto &c : *io.clock)
      if (c.first == site) return c.second;
  return io.t_head;
}

// ------------------------------------------------------------------------------------------------ the frame
// Call sites of the frame's own clock reads, as offsets from the frame-head return address (ML+0x66f): H1's g_frameMs read
// returns to ML+0xf081.
const int32_t kSiteFrameMs = 0xf081 - 0x66f;
// waitTicks' first clock call site (waitTicks+0x33), as an offset from the frame-head return address; its others are +4, +0x30
const int32_t kSiteWaitTicks = 0x4c384;

struct Frame : TkBase {
  using TkBase::TkBase;

  // switch index tables of mainLoop (byte tables in its .text, ML+0x12180 .. ML+0x12530), as case lists
  // P4 golfer-label kind (ML+0x78fe): k = kind + 7 in 0..0x2a
  static int sw_p4_kind(int i) {
    switch (i) {
      case 0: return 0;
      case 1: return 1;
      case 2: return 2;
      case 3: return 3;
      case 4: return 4;
      case 5: return 5;
      case 42: return 6;
    }
    return 7;
  }
  // H1 key dispatch (ML+0xcfec): key code - 9 in 0..0x71
  static int sw_h1_key(int i) {
    static const int16_t k[][2] = {{0, 0},   {4, 1},   {18, 2},  {23, 3},  {24, 4},  {26, 5},  {27, 6},  {28, 7},
                                   {29, 8},  {31, 9},  {33, 10}, {34, 11}, {36, 12}, {37, 13}, {38, 14}, {39, 15},
                                   {40, 16}, {41, 17}, {42, 18}, {43, 19}, {44, 20}, {45, 21}, {46, 22}, {47, 23},
                                   {48, 24}, {52, 25}, {54, 26}, {55, 27}, {57, 28}, {58, 29}, {65, 30}, {67, 31},
                                   {69, 32}, {71, 33}, {73, 34}, {74, 35}, {75, 36}, {78, 37}, {79, 38}, {85, 39},
                                   {89, 40}, {91, 41}, {92, 42}, {93, 43}, {94, 44}, {95, 45}, {99, 46}, {101, 47},
                                   {103, 48}, {105, 49}, {106, 50}, {107, 51}, {109, 52}, {110, 53}, {111, 54}, {113, 55}};
    for (const auto &c : k)
      if (c[0] == i) return c[1];
    return 56;
  }
  // H1 sub-dispatch (ML+0xe863): [F+0x28] - 0x21 in 0..0x58
  static int sw_h1_41de23(int i) {
    if (i == 0) return 0;
    if (i >= 3 && i <= 7) return i - 2;
    if (i >= 79 && i <= 88) return i - 73;
    return 16;
  }
  // H2 tool click (ML+0xff2d): the tool in 0..0x13
  static int sw_h2_tool(int i) {
    switch (i) {
      case 0: return 0;
      case 1: return 1;
      case 2: return 2;
      case 15: return 3;
      case 16: return 4;
      case 19: return 5;
    }
    return 6;
  }
  // H2 placed-object kind (ML+0x110be): the kind in 0..0x11
  static int sw_h2_kind(int i) {
    switch (i) {
      case 0: case 1: return 0;
      case 2: return 1;
      case 4: return 2;
      case 7: case 8: return 3;
      case 12: return 4;
      case 17: return 5;
    }
    return 6;
  }

// The converted body keeps the original's integer widths and the transcription's register-level labels: truncating stores,
// mixed signed / unsigned compares and labels only some paths jump to are the original's, not mistakes of the port.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4018 4102 4127 4189 4244 4245 4309 4310 4389 4456 4457 4458 4701 4702 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "tick_frame.inc"
// phase 3.8 (stream 38j): mainLoop's title menu and session dispatch (log/p38/38j/conv_menu.py)
#include "tick_menu.inc"
// phase 4: the boot path (gameMain's part and mainLoop's Setup, then the title menu)
#include "tick_boot.inc"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  // MainLoopBody's frame order (c3fy.cpp): the preamble, P1..P7, H1, and H2 when H1 falls through
  FrameExit run(u32 F, bool retile) {
    const DevProfScope frame_prof("@frame");
    { const DevProfScope p("@preamble"); FramePreamble(F, retile ? 1 : 0); }
    {
      // P33A item 1 (gfx.h gfx_set_quiet_frame): the world phases of a faster speed's unshown frame draw nothing (drawing only;
      // without a backend, or with no speed button, both calls return at once)
      struct QuietWorld {
        Ctx &c;
        explicit QuietWorld(Ctx &x) : c(x) { gfx_world_quiet_begin(c); }
        ~QuietWorld() { gfx_world_quiet_end(c); }
      } quiet_world(ctx);
      { const DevProfScope p("@P1 tile pass"); Ph_00410604(F); }
      { const DevProfScope p("@P2 tile pass"); Ph_0041234e(F); }
      { const DevProfScope p("@P3"); Ph_0041353d(F); }
      { const DevProfScope p("@P4 P5"); Ph_00415847(F); }
    }
    { const DevProfScope p("@P6"); Ph_004175f2(F); }
    const int r6 = Ph_00418bc7(F);
    {
      GfxCursorScope cursor_scope(ctx);  // P9E fix 4: P7's tool previews are drawn at the cursor tile (records only, gfx.h)
      const DevProfScope p("@P7");
      Ph_00419d06(F, r6);
    }
    int h2in[6] = {0, 0, 0, 0, 0, 0};
    int d;
    { const DevProfScope p("@H1"); d = Ph_0041c0c5(F, h2in); }
    if (d == kNext) { const DevProfScope p("@H2"); d = Ph_0041ecbb(F, h2in); }
    // the title menu ML+0x11875 and the session dispatch, then the next frame head (phase 3.8, stream 38j)
    if (d == kMenu) return menu_session(F);
    // ML+0x664 (rebuildTileCoverage) runs before the frame head ML+0x669, where the original's frame record is taken: the
    // rebuild belongs to this step, so the next one enters at the frame head (phase 3.8, stream 38j)
    if (d == kRetile) {
      cc("rebuildTileCoverage");
      return FrameExit::Frame;
    }
    return FrameExit::Frame;
  }
};

// The frame block: mainLoop's stack frame F (slots +0 .. +0x1250) in the context's address space. It persists across the
// frames of a run like the original's (slots one frame writes and a later one reads, re/analysis/data/premise_check.md:
// restarts did not need it restored, so its content at a segment's start is not state); it starts zeroed.
constexpr uint32_t kFrameSlots = 0x1400;               // mainLoop's frame F + 0 .. F + 0x1250 (and margin)
constexpr uint32_t kStackSize = 0x10000;               // TkBase's stack for address-taken locals of the bodies
constexpr uint32_t kFrameSize = kFrameSlots + kStackSize;
struct FrameBlock {
  std::vector<uint8_t> host;
  uint32_t va = 0;
};
std::mutex g_fb_mu;
std::map<const Ctx *, FrameBlock> g_fb;

uint32_t frame_block(Ctx &ctx) {
  std::lock_guard<std::mutex> g(g_fb_mu);
  FrameBlock &b = g_fb[&ctx];
  if (b.va != 0) {
    try {
      if (ctx.mem.at(b.va, kFrameSize) == b.host.data()) return b.va;
    } catch (const Fault &) {
    }
  }
  b.host.assign(kFrameSize, 0);
  // above the GameState, page aligned, at the first free base
  uint32_t base = (ctx.mem.data_base() + static_cast<uint32_t>(sizeof(GameState)) + 0x00100000u) & ~0xFFFFu;
  for (int tries = 0; tries < 64; ++tries, base += 0x00100000u)
    if (ctx.mem.map(base, b.host.data(), kFrameSize, false)) {
      b.va = base;
      return base;
    }
  throw MissingResource{"tick frame block (no free address range)"};
}

namespace {
std::map<const Ctx *, uint32_t> g_sp;  // stack depth per context (bytes used of kStackSize)
}
uint32_t TkBase::stack_push(uint32_t n) {
  const uint32_t need = (n + 3u) & ~3u;
  uint32_t &sp = g_sp[&ctx];
  if (sp + need > kStackSize) throw MissingResource{"tick stack overflow"};
  const uint32_t va = frame_block(ctx) + kFrameSlots + sp;
  sp += need;
  std::memset(ctx.mem.at(va, need, true), 0, need);
  return va;
}
void TkBase::stack_pop(uint32_t n) { g_sp[&ctx] -= (n + 3u) & ~3u; }
FrameSlots::FrameSlots(TkBase &tb, uint32_t bytes) : t(tb), n(bytes), va(tb.stack_push(bytes)) {}
FrameSlots::~FrameSlots() { t.stack_pop(n); }
LV::LV(TkBase &tb, int v) : t(tb), va(tb.stack_push(4)) { t.ctx.mem.store<int32_t>(va, v); }
LV::~LV() { t.stack_pop(4); }
LV::operator int() const { return t.ctx.mem.load<int32_t>(va); }
LV &LV::operator=(int v) {
  t.ctx.mem.store<int32_t>(va, v);
  return *this;
}

#include "tick_render.inc"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4018 4100 4102 4127 4189 4244 4245 4309 4310 4389 4456 4457 4458 4701 4702 4706)
#endif
#include "tick_world.inc"
#include "tick_leaves.inc"

// The HUD panels P6 draws every frame by g_panelListMode (converted from their hook sources). They are in phase 3.7b's
// straddler list; the click handling is elsewhere (H1 / H2), so these bodies draw and hover only. Registered as
// fallbacks (fallbacks()): 3.7b's registered function wins when it exists.
struct Panels : TkBase {
  using TkBase::TkBase;
#include "tick_panels.inc"
};
// showBulletinBoard (phase 3.7b's straddler; fallback until it is registered, see fallbacks()): P5 calls it on every
// unpaused frame without a message. Its non-modal prefix (c3em.cpp): with no pending photo (f_w_4e3db8 == -1) it returns at
// once; with a photo position (f_w_4e3db8 + 4 != -1) the camera jumps to it when it is off screen, otherwise the photo is
// saved to a file (saveBmp: output, dropped) after its name is built (__itoa into g_scoreSummaryBuf) and the position is
// cleared; then g_bulletinAnimTick counts up and the board waits below 0x14. The board itself is a modal screen: NeedInput.
int fb_show_bulletin_board(TkBase &t, const uint32_t *, int) {
  Ctx &ctx = t.ctx;
  if (t.G32(GS(f_w_4e3db8)) == -1) return 0;
  if (t.G32(GS(f_w_4e3db8) + 4) != -1) {
    LV sx(t), sy(t);
    if (t.cc("worldToScreen", t.G32(GS(f_w_4e3db8) + 4).get(), t.G32(GS(g_scrollTextWin) + 0x54).get(), &sx, &sy, 0) == 0) {
      t.G32(GS(gCameraTileX)) = sar32(t.G32(GS(f_w_4e3db8) + 4), 10);
      t.G32(GS(gCameraTileY)) = sar32(t.G32(GS(g_scrollTextWin) + 0x54), 10);
      camera_on_map(ctx);  // P6A mainLoop.cameraFollowOffGrid
    } else {
      t.cc("clamp", sx - 0x64, 0, 0x258);
      t.cc("clamp", sy - 0x64, 0, 0x1b8);
      t.Itoa(t.G32(GS(f_w_4e3db8)), GS(g_scoreSummaryBuf), 10);
      t.G32(GS(f_w_4e3db8) + 4) = -1;
    }
  }
  const int seen = t.G32(GS(g_bulletinAnimTick)) + (SG_NEGCTL(g_negctl_frame) == 2 ? 2 : 1);
  t.G32(GS(g_bulletinAnimTick)) = seen;
  if (seen < 0x14) return 0;
  throw NeedInput{{"showBulletinBoard"}};
}
// nearestMenuSpot(x, y) (c3d.cpp): the nearest radial-menu spot of the 16-bit point table f_tab_point16_4c7930 (ended by
// x == -1) within 30, weighting spot i by (i + 6) / 8; spot 6 reads as none (-1)
int fb_nearest_menu_spot(TkBase &t, const uint32_t *a, int) {
  Ctx &ctx = t.ctx;
  const int x = static_cast<int>(a[0]), y = static_cast<int>(a[1]);
  const uint32_t spots = GS(f_tab_point16_4c7930);
  int best = -1, bestD = 30, i = 0;
  do {
    const int sx = t.G16(spots + 4u * i), sy = t.G16(spots + 4u * i + 2);
    const int d = mul32(t.cc("approxDistance", x - sx, y - sy), i + 6) / 8;
    if (d < bestD) {
      bestD = d;
      best = i;
    }
    i++;
  } while (t.G16(spots + 4u * i) != -1);
  return best == 6 ? -1 : best;
}
// showMainRadialMenu(x, y) (c3fi.cpp; a ui straddler of phase 3.7b's list, fallback until registered): P6 draws the radial
// menu every frame. No click is handled here; with the drawing dropped, what stays is the hover state: the blink counter
// g_panelState+0xc while paused, the hover dwell counter g_panelState+0x18 against the last hovered spot g_ui_5aa6b8, and
// after 10 frames of dwell the spot's label text in g_textBuffer (drawPanelBackdrop draws it: dropped).
int fb_show_main_radial_menu_state(TkBase &t, const uint32_t *a, bool &tip) {
  Ctx &ctx = t.ctx;
  const uint32_t xy[2] = {a[0], a[1]};
  const int s = fb_nearest_menu_spot(t, xy, 2);
  int hover = s;
  if (!(s == -1) && !(s == t.G32(GS(g_panelListMode)))) hover = t.G8(GS(f_tab_bytes_4c7994) + static_cast<uint32_t>(s));
  if ((t.GU8(GS(g_flags)) & 4) != 0) t.GU32(GS(g_panelState) + 0xc) = t.GU32(GS(g_panelState) + 0xc) + 1;
  if (hover != t.G32(GS(g_ui_5aa6b8))) {
    t.G32(GS(g_panelState) + 0x18) = 0;
    t.G32(GS(g_ui_5aa6b8)) = hover;
    return 0;
  }
  const int cnt = t.G32(GS(g_panelState) + 0x18) + 1;
  t.G32(GS(g_panelState) + 0x18) = cnt;
  if (hover == -1) return 0;
  if (cnt <= 10) return cnt;
  t.GU8(t.kBuf) = 0;
  tip = true;
  if (!(static_cast<unsigned>(s) > 10)) {
    static const uint32_t kLabel[11] = {0x8c, 0x7c, 0x68, 0x5c, 0, 0x38, 0x24, 0x18, 0xc, 0, 0};  // f_strpool_4c7de0 offsets
    uint32_t label = GS(f_strpool_4c7de0) + kLabel[s];
    if (s == 4) label = GS(f_strpool_4c7de0) + ((t.GU8(GS(g_flags)) & 4) != 0 ? 0x54u : 0x4cu);
    t.StrCpy(t.kBuf, label);
  }
  return 0;
}
// roadmap S3.6: the state steps above, then (a backend attached) the drawing, gfx_hud.cpp showMainRadialMenu.draw
int fb_show_main_radial_menu(TkBase &t, const uint32_t *a, int) {
  bool tip = false;
  const int r = fb_show_main_radial_menu_state(t, a, tip);
  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {
    const uint32_t d[3] = {a[0], a[1], tip ? 1u : 0u};
    gfx_drop(t.ctx, "showMainRadialMenu.draw", d, 3);
  }
  return r;
}
int fb_draw_building_panel(TkBase &t, const uint32_t *a, int) {
  return Panels(t.ctx).drawBuildingPanel(static_cast<int>(a[0]), static_cast<int>(a[1]));
}
// tickMessage: phase 3.7b registers it (commands_layout.inc); this body (tick_leaves.inc) is the fallback
int fb_tick_message(TkBase &t, const uint32_t *, int) { return Leaves(t.ctx).tickMessage(); }
int fb_show_tournament_results(TkBase &t, const uint32_t *, int) {
  Panels(t.ctx).showTournamentResults();
  return 0;
}
struct FallbackReg {
  FallbackReg() {
    fallbacks()["showTournamentResults"] = fb_show_tournament_results;
    fallbacks()["tickMessage"] = fb_tick_message;
    fallbacks()["drawBuildingPanel"] = fb_draw_building_panel;
    fallbacks()["showBulletinBoard"] = fb_show_bulletin_board;
    fallbacks()["showMainRadialMenu"] = fb_show_main_radial_menu;
    fallbacks()["nearestMenuSpot"] = fb_nearest_menu_spot;
  }
} g_fallback_reg;
// drawWalkers: walker sprites and labels of P4 (converted from c3fs.cpp)
struct Ball : TkBase {
  using TkBase::TkBase;
#include "tick_ball.inc"
};
// P36B (golfer_wait.h): the wait reasons updateGolfers tags. Beside the simulation: nothing here is read by a sim branch,
// saved or hashed. One table per process (the app runs one simulation; the panel line is an app option, off in tests).
namespace {
struct GolferWaitTable {
  unsigned char tick[0x98] = {};    // the reason of the golfer's last tick
  unsigned char held[0x98] = {};    // the reason of the wait the golfer runs (+0x9e < 0)
  short other[0x98] = {};           // the golfer it waits for, -1 none
  short held_other[0x98] = {};
  bool fresh[0x98] = {};            // a tagged rule set the wait since the golfer's last tick
  short last_wait[0x98] = {};       // +0x9e at the end of the golfer's last tick
  int tick_date[0x98] = {};         // the date of the golfer's last tick
};
}  // namespace
GolferWaitTable g_gw;
namespace {
// SG_P36B_WAITLOG=<file> (development only): golfer-ticks per hole and reason, hole changes, the holes' time words
struct GolferWaitLog {
  bool on = false;
  std::string path;
  long long ticks[20][kGwCount] = {};   // [hole 1..18, 19 = leaving][reason]
  long long done[20] = {}, quit[20] = {}, dur[20] = {}, durn[20] = {}, gdt[20] = {}, score[20] = {};
  int enter[0x98] = {};   // the date the golfer was first seen on its current hole (0 = not seen)
  int date0 = -1, last_dump = -1;
  long long calls = 0;
  int u1ec0[20] = {}, shots0[20] = {};
  GolferWaitLog() {
    if (const char *p = std::getenv("SG_P36B_WAITLOG")) { on = *p != 0; path = p; }
  }
} g_gwlog;
}  // namespace
const char *gw_id(int why) {
  static const char *const k[kGwCount] = {
      "none", "walk", "swing", "ball", "address", "ahead_tee", "ahead_ball", "ahead_at_target", "ahead_line", "tee_queue",
      "partner_tee", "partner_farther", "partner_swing", "partner_watch", "partner_prev_hole", "partner_order", "partner_green",
      "bump", "crowded", "turn", "no_path", "react_good", "react_bad", "penalty", "green_read", "shot_throttle", "player_shot",
      "clubhouse", "practice", "snack", "drinks", "restroom", "rest_spot", "thought", "story", "membership", "arrival", "holed",
      "leaving", "stand", "idle", "other_wait"};
  return why >= 0 && why < kGwCount ? k[why] : "?";
}
void gw_set(int g, int why, int other = -1) {
  if (g < 0 || g >= 0x98) return;
  g_gw.held[g] = static_cast<unsigned char>(why);
  g_gw.held_other[g] = static_cast<short>(other);
  g_gw.fresh[g] = true;
}
int gw_reason(Ctx &ctx, int g, int &other) {
  other = -1;
  if (g < 0 || g >= 0x98) return kGwNone;
  const GameState &s = ctx.gs();
  if (s.g_date - g_gw.tick_date[g] > 2 || s.g_date < g_gw.tick_date[g]) return kGwNone;   // no tick of this golfer lately
  if (s.g_golfers[g].wait < 0) {
    other = g_gw.held_other[g];
    return g_gw.held[g];
  }
  other = g_gw.other[g];
  return g_gw.tick[g];
}
// one golfer's tick of updateGolfers: the reason from the tags, else from what the golfer did
struct GolferWaitTick {
  Ctx &ctx;
  int g, date, x, y, wait0, hole0, stamp0, why = kGwNone, other = -1;
  GolferWaitTick(Ctx &c, int i) : ctx(c), g(i) {
    const GameState &s = c.gs();
    const Golfer &r = s.g_golfers[i];
    date = s.g_date; x = r.x; y = r.y; wait0 = r.wait; hole0 = r.hole; stamp0 = r.date_stamp;
    if (wait0 < g_gw.last_wait[i] && !g_gw.fresh[i]) gw_set(i, kGwOtherWait);   // set outside updateGolfers, untagged
    g_gw.fresh[i] = false;
  }
  void tag(int w, int o = -1) { why = w; other = o; }
  void hold(int w, int o = -1) { why = w; other = o; gw_set(g, w, o); }
  ~GolferWaitTick() {
    const GameState &s = ctx.gs();
    const Golfer &r = s.g_golfers[g];
    int w = why;
    if (wait0 < 0) w = (r.ball_speed != 0 && (r.flags & (1u << 18))) ? kGwBall : (g_gw.held[g] ? g_gw.held[g] : kGwOtherWait);
    else if (w == kGwNone) {
      if (r.x != x || r.y != y) w = kGwWalk;
      else if (r.flags & (1u << 18)) w = kGwBall;   // the ball moves (set at the club's contact, cleared at its stop)
      else if (r.counter20 >= 2) w = kGwSwing;
      else w = kGwStand;
    }
    if (r.wait < 0 && (wait0 >= 0 || r.wait < wait0) && !g_gw.fresh[g]) gw_set(g, why != kGwNone ? why : kGwOtherWait, other);
    g_gw.fresh[g] = false;
    g_gw.last_wait[g] = r.wait;
    g_gw.tick[g] = static_cast<unsigned char>(w);
    g_gw.other[g] = static_cast<short>(wait0 < 0 ? g_gw.held_other[g] : other);
    g_gw.tick_date[g] = date;
    if (!g_gwlog.on) return;
    GolferWaitLog &L = g_gwlog;
    const int h = hole0 >= 1 && hole0 <= 18 ? hole0 : (hole0 == 0x13 ? 19 : 0);
    if (h != 0) L.ticks[h][w]++;
    if (hole0 >= 1 && hole0 <= 18 && r.hole != hole0) {
      L.done[hole0]++;
      if (r.hole != 0x13 && L.enter[g] > 0) { L.dur[hole0] += date - L.enter[g]; L.durn[hole0]++; }   // seen from the hole's start
      if (r.hole != 0x13) L.score[hole0] += r.hole_strokes[hole0];   // the hole's score matchUpdate wrote (+0x23 + hole)
      if (r.hole != 0x13) L.gdt[hole0] += date - stamp0;   // matchUpdate's own span (Time to Play: half of it per 40 is a minute)
      if (r.hole == 0x13) L.quit[hole0]++;
    }
    if (r.hole != hole0) L.enter[g] = r.hole >= 1 && r.hole <= 18 ? date : 0;
  }
};
// the log's file, rewritten every 0x100 dates (called once per updateGolfers)
void golfer_wait_dump(Ctx &ctx) {
  {   // P36B pictures only (SG_P36B_PANEL=<dates>, unset in every test and release run): every <dates> dates the golfer info panel
      // opens on a standing golfer whose reason differs from the last one shown (the word a click on a golfer writes,
      // g_msgState53df54 = golfer + 1, read by mainLoop's P6_Panels)
    static const int period = std::getenv("SG_P36B_PANEL") ? std::atoi(std::getenv("SG_P36B_PANEL")) : 0;
    static int next = -1, last_why = -1, shown = 0;
    GameState &s = ctx.gs();
    if (period > 0) {
      if (next < 0) next = s.g_date + period;
      if (s.g_date >= next && (s.g_msgState53df54 == 0 || s.g_msgState53df54 == shown)) {
        next = s.g_date + period;
        for (int g = 0; g < 0x98; g++) {
          int other = -1;
          const int w = gw_reason(ctx, g, other);
          const int h = s.g_golfers[g].hole;
          if (h < 1 || h > 18 || w == kGwNone || w == kGwWalk || w == kGwSwing || w == kGwBall || w == kGwStand ||
              w == kGwOtherWait || w == last_why)
            continue;
          last_why = w;
          shown = s.g_msgState53df54 = g + 1;
          break;
        }
      }
    }
  }
  GolferWaitLog &L = g_gwlog;
  if (!L.on) return;
  const GameState &s = ctx.gs();
  L.calls++;
  if (L.date0 < 0) {
    L.date0 = L.last_dump = s.g_date;
    for (int h = 1; h <= 18; h++) { L.u1ec0[h] = s.g_holes[h].u1ec; L.shots0[h] = s.g_holes[h].first_shots; }
    return;
  }
  if (s.g_date - L.last_dump < 0x100 && s.g_date >= L.last_dump) return;
  L.last_dump = s.g_date;
  FILE *f = std::fopen(L.path.c_str(), "w");
  if (!f) return;
  std::fprintf(f, "# P36B waitlog dates %d..%d holes %d calls %lld\n", L.date0, s.g_date, s.g_holesPlusOne - 1, L.calls);
  std::fprintf(f, "hole\tdone\tquit\tu1ec0\tu1ec\tshots0\tshots\tdur\tdurn\tgdt\tmood\tu024\tscore");
  for (int w = 1; w < kGwCount; w++) std::fprintf(f, "\t%s", gw_id(w));
  std::fprintf(f, "\n");
  for (int h = 1; h <= 19; h++) {
    const int hr = h <= 18 ? h : 0;
    std::fprintf(f, "%d\t%lld\t%lld\t%d\t%d\t%d\t%d\t%lld\t%lld\t%lld", h, L.done[h], L.quit[h], h <= 18 ? L.u1ec0[h] : 0,
                 h <= 18 ? s.g_holes[hr].u1ec : 0, h <= 18 ? L.shots0[h] : 0, h <= 18 ? s.g_holes[hr].first_shots : 0, L.dur[h], L.durn[h],
                 L.gdt[h]);
    std::fprintf(f, "\t%d\t%d", h <= 18 ? s.g_holes[hr].mood_sum : 0, h <= 18 ? s.g_holes[hr].u024 : 0);
    std::fprintf(f, "\t%lld", L.score[h]);
    for (int w = 1; w < kGwCount; w++) std::fprintf(f, "\t%lld", L.ticks[h][w]);
    std::fprintf(f, "\n");
  }
  std::fprintf(f, "# golfers: g hole strokes tx ty wait flags state counter20 partner order tick held other\n");
  for (int g = 0; g < 0x98; g++) {
    const Golfer &r = s.g_golfers[g];
    if (r.hole == 0) continue;
    std::fprintf(f, "G\t%d\t%d\t%d\t%.2f\t%.2f\t%d\t%08x\t%d\t%d\t%d\t%d\t%s\t%s\t%d\n", g, r.hole, r.strokes, r.x / 1024.0, r.y / 1024.0,
                 r.wait, r.flags, r.state, r.counter20, r.partner, r.order, gw_id(g_gw.tick[g]), gw_id(g_gw.held[g]), g_gw.other[g]);
  }
  std::fclose(f);
}
// updateGolfers: P5's per-tick golfer state machine (converted from c3ec.cpp)
struct Golfers : TkBase {
  using TkBase::TkBase;
#include "tick_golfers.inc"
};
// membershipReport: a golfer's membership upgrade notice (converted from c3ah.cpp)
struct Members : TkBase {
  using TkBase::TkBase;
#include "tick_members.inc"
};
// matchUpdate: the per-hole scoring of a golfer who holed out (converted from c3aj.cpp)
struct Match : TkBase {
  using TkBase::TkBase;
#include "tick_match.inc"
};
// buildSceneSprites: the per-object scene sprites of P2 (converted from c3fj.cpp)
struct Scene : TkBase {
  using TkBase::TkBase;
#include "tick_scene.inc"
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// A Terrain.dll stand-in for headless runs: every method returns 0 (Terrain.dll's state feeds rendering only, U-0800).
class NullTerrain : public TerrainApi {
 public:
  uint32_t pass_collar_info(uint32_t, const int32_t *, int32_t) override { return 0; }
  uint32_t load_new_course_type(uint32_t, int32_t) override { return 0; }
  uint32_t set_type(uint32_t, uint32_t, int32_t, int32_t) override { return 0; }
  uint32_t get_variation(uint32_t, uint32_t) override { return 0; }
  uint32_t lay_path(uint32_t, uint32_t, int32_t, int32_t) override { return 0; }
  uint32_t set_wall(uint32_t, uint32_t, int32_t, int32_t, int32_t) override { return 0; }
  uint32_t calc_normals(uint32_t, uint32_t) override { return 0; }
  uint32_t calc_all_normals(uint32_t, uint32_t) override { return 0; }
  uint32_t elevate_corner(uint32_t, uint32_t, int32_t) override { return 0; }
  uint32_t lower_corner(uint32_t, uint32_t, int32_t) override { return 0; }
  uint32_t lower_edge_corner(uint32_t, uint32_t, int32_t, uint32_t, uint32_t) override { return 0; }
  uint32_t has_path(uint32_t, uint32_t) override { return 0; }
  uint32_t has_connected_path(uint32_t, int32_t, int32_t) override { return 0; }
  uint32_t update_path(uint32_t, int32_t, int32_t, int32_t) override { return 0; }
  uint32_t init_system(uint32_t, int32_t, int32_t, uint32_t, int32_t) override { return 0; }
  uint32_t init_terrain(uint32_t) override { return 0; }
  uint32_t set_zoom_level(uint32_t, int32_t) override { return 0; }
  uint32_t render(uint32_t, uint32_t, uint32_t) override { return 0; }
  uint32_t local_render(uint32_t, uint32_t, uint32_t, uint32_t) override { return 0; }
  uint32_t path_update_render(uint32_t, uint32_t, uint32_t) override { return 0; }
  uint32_t os_major_version() override { return 5; }
  uint32_t acquire_dc() override { return 0; }
  uint32_t pump_input() override { return 0; }
  uint32_t window_paint() override { return 0; }
  uint32_t blit_full() override { return 0; }
  uint32_t blit_screen(int32_t, int32_t) override { return 0; }
  uint32_t render_strip(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) override { return 0; }
  uint32_t operator_new(uint32_t) override { return 0; }
};

// The Terrain.dll side of a headless run (sg_run_frames with no TerrainApi attached). Terrain.dll's object (gTerrain) and the
// nodes of the exe's render list gTileBuffer live on the original's heap, which no segment image holds (U-0607), but the exe
// side reads them: Terrain::tileAt's width / height / tile fields (updateTerrainRender, rebuildTileData and its
// applyTile* callees, terrain_dll.cpp) and the list's sentinel and nodes (push_tile, the list sweep). Measured by phase
// 3.1a (U-0800), none of these values reaches a simulation-class object: they decide only Terrain.dll calls, the render
// list (pointer class) and engine-class flags. So the run maps stand-ins where the image's pointers point: a Terrain
// object of 50 x 50 zeroed tiles (the exe's grid size, every tile loop runs 0..49), an empty list's sentinel (next = prev =
// itself) when the image's list size is 0, and an arena for the nodes operator new returns. A non-empty list at the start
// is not modelled: its sweep then faults and the run reports it.
class HeadlessTerrain : public NullTerrain {
 public:
  explicit HeadlessTerrain(Ctx &c) : ctx_(c) {
    std::lock_guard<std::mutex> g(mu());
    Blocks &b = blocks()[&c];
    b.arena_used = 0;
    const uint32_t ter = c.gs().gTerrain;
    if (ter != 0 && !mapped(c, ter, obj_size(c)) && ensure(c, b.terrain, b.terrain_va, ter, obj_size(c))) {
      c.mem.store<int32_t>(ter + 0x14, world_n(c));  // width (terrain_dll.cpp kTerWidth)
      c.mem.store<int32_t>(ter + 0x18, world_n(c));  // height (kTerHeight)
    }
    const uint32_t list = state_va(c, &GameState::gTileBuffer);
    const uint32_t head = c.mem.load<uint32_t>(list + 4);
    if (head != 0 && c.mem.load<int32_t>(list + 8) == 0 && !mapped(c, head, 12) && ensure(c, b.sentinel, b.sentinel_va, head, 12)) {
      c.mem.store<uint32_t>(head, head);
      c.mem.store<uint32_t>(head + 4, head);
    }
    if (!mapped_host(c, b.arena, b.arena_va)) {
      b.arena.assign(kArenaSize, 0);
      b.arena_va = 0;
      uint32_t base = (c.mem.data_base() + static_cast<uint32_t>(sizeof(GameState)) + (4u << 20)) & ~0xFFFFu;
      for (int tries = 0; tries < 64 && b.arena_va == 0; ++tries, base += 0x00100000u)
        if (c.mem.map(base, b.arena.data(), kArenaSize, false)) b.arena_va = base;
    }
  }
  // The corner steps of Terrain::elevateCorner / lowerCorner / lowerEdgeCorner (re/match/terrain_methods.cpp, a NULL tile
  // does nothing) and Tile::elevateCorner / lowerCorner (re/match/terrain_corner.cpp: m_corner[corner] += 1 / -= 1, at tile
  // + 4 * corner): applyTileElevation re-reads that field and loops until it reaches the grid's height, so a stand-in that
  // left it unchanged would never return. The rest of those methods (vertex heights, normals flags, the edge render) is
  // Terrain.dll's render state.
  uint32_t elevate_corner(uint32_t, uint32_t tile, int32_t c) override { return step_corner(tile, c, 1); }
  uint32_t lower_corner(uint32_t, uint32_t tile, int32_t c) override { return step_corner(tile, c, -1); }
  uint32_t lower_edge_corner(uint32_t, uint32_t tile, int32_t c, uint32_t, uint32_t) override { return step_corner(tile, c, -1); }
  // Terrain::getInstance for a boot run (phase 4): the object the exe reads fields of (width +0x14, height +0x18, the tile
  // array from +0x3a4), carved from the arena like the ones mapped at a segment image's gTerrain: zeroed but the 50 x 50 size
  uint32_t get_instance() override {
    const uint32_t va = operator_new(obj_size(ctx_));
    ctx_.mem.store<int32_t>(va + 0x14, world_n(ctx_));
    ctx_.mem.store<int32_t>(va + 0x18, world_n(ctx_));
    return va;
  }
  // P38B: a zeroed stand-in object of the new size with width / height N at a free address; gTerrain follows
  void world_resized(int32_t n) override {
    std::lock_guard<std::mutex> g(mu());
    Blocks &b = blocks()[&ctx_];
    if (b.terrain_va != 0) ctx_.mem.unmap(b.terrain_va);
    b.terrain_va = 0;
    const uint32_t size = obj_size(ctx_);
    b.terrain.assign(size, 0);
    for (uint32_t base = 0x70000000u; base < 0xE0000000u && b.terrain_va == 0; base += 0x00100000u)
      if (ctx_.mem.map(base, b.terrain.data(), size, false)) b.terrain_va = base;
    if (b.terrain_va == 0) throw MissingResource{"headless Terrain: no free range for the resized object"};
    ctx_.mem.store<int32_t>(b.terrain_va + 0x14, n);
    ctx_.mem.store<int32_t>(b.terrain_va + 0x18, n);
    ctx_.gs().gTerrain = b.terrain_va;
  }
  uint32_t operator_new(uint32_t n) override {
    std::lock_guard<std::mutex> g(mu());
    Blocks &b = blocks()[&ctx_];
    const uint32_t need = (n + 7u) & ~7u;
    if (b.arena_va == 0 || b.arena_used + need > kArenaSize) throw MissingResource{"headless Terrain arena"};
    const uint32_t va = b.arena_va + b.arena_used;
    b.arena_used += need;
    std::memset(b.arena.data() + (va - b.arena_va), 0, need);
    return va;
  }

 private:
  // the tile array's end (kTerTiles + N * N * kTileSize; 0x3a4 + 2500 * 0x248 at 50)
  static uint32_t obj_size(const Ctx &c) { return 0x3A4u + static_cast<uint32_t>(world_tiles(c)) * 0x248u; }
  static constexpr uint32_t kArenaSize = 0x01000000u;   // 16 MB: cov_b_holes18/00 ran a 1 MB arena out at record 1140 (38d)
  struct Blocks {
    std::vector<uint8_t> terrain, sentinel, arena;
    uint32_t terrain_va = 0, sentinel_va = 0, arena_va = 0, arena_used = 0;
  };
  static std::mutex &mu() {
    static std::mutex m;
    return m;
  }
  static std::map<const Ctx *, Blocks> &blocks() {
    static std::map<const Ctx *, Blocks> m;
    return m;
  }
  static bool mapped(Ctx &c, uint32_t va, uint32_t n) {
    try {
      c.mem.at(va, n);
      return true;
    } catch (const Fault &) {
      return false;
    }
  }
  static bool mapped_host(Ctx &c, std::vector<uint8_t> &host, uint32_t va) {
    if (va == 0 || host.empty()) return false;
    try {
      return c.mem.at(va, static_cast<uint32_t>(host.size())) == host.data();
    } catch (const Fault &) {
      return false;
    }
  }
  // maps a zeroed host block at va (reusing this context's block when it is already there)
  static bool ensure(Ctx &c, std::vector<uint8_t> &host, uint32_t &slot, uint32_t va, uint32_t n) {
    if (slot == va && host.size() == n && mapped_host(c, host, va)) {
      std::fill(host.begin(), host.end(), uint8_t{0});
      return true;
    }
    host.assign(n, 0);
    slot = c.mem.map(va, host.data(), n, false) ? va : 0;
    return slot != 0;
  }
 public:
  // roadmap S7.9 (U-1681): the blocks live per context address in a process-wide map; a freed context's entry was never
  // erased, so every context the heap put at a new address kept its 16 MB arena (a Win32 runner reached 2 GB after about 35
  // scenarios). A context reusing an address re-zeroes and re-maps the entry's blocks, so erasing it changes no run.
  static void release(const Ctx &c) {
    std::lock_guard<std::mutex> g(mu());
    blocks().erase(&c);
  }

 private:
  uint32_t step_corner(uint32_t tile, int32_t c, int32_t d) {
    if (tile == 0) return 0;
    const uint32_t va = tile + 4u * static_cast<uint32_t>(c);
    ctx_.mem.store<int32_t>(va, add32(ctx_.mem.load<int32_t>(va), d));
    return 0;
  }
  Ctx &ctx_;
};

}  // namespace tk

std::unique_ptr<TerrainApi> make_headless_terrain(Ctx &ctx) { return std::make_unique<tk::HeadlessTerrain>(ctx); }
void release_headless_terrain(const Ctx &ctx) { tk::HeadlessTerrain::release(ctx); }

// ------------------------------------------------------------------------------------------------ public API
namespace tk {
// tick_frame, plus what the frame's cursor / GetAsyncKeyState reads did against its records (filled on every exit)
FrameExit tick_frame_io(Ctx &ctx, const FrameInput &in, FrameIoStats &st) {
  tk::FrameIo io;
  struct Fill {
    const FrameIo &io;
    FrameIoStats &st;
    ~Fill() {
      st.cursor_over = io.cursor_overrun;
      st.cursor_under = io.cursor && io.cursor_used < io.cursor->size() ? io.cursor->size() - io.cursor_used : 0;
      st.async_over = io.async_overrun;
      st.async_under = 0;
      if (io.async)
        for (size_t i = 0; i < io.async->size(); ++i)
          if (i >= io.async_next[(*io.async)[i].first]) ++st.async_under;
    }
  } fill{io, st};
  io.t_head = in.t_head;
  io.live = in.live;  // roadmap S7.1
  io.clock = &in.clock;
  io.cursor = &in.cursor;
  io.cursor_pos = &in.cursor_pos;
  io.cursor_x = in.cursor_start_x;
  io.cursor_y = in.cursor_start_y;
  const uint32_t F = tk::frame_block(ctx);
  tk::IoScope scope(io);
  tk::Frame fr(ctx);
  if (SG_NEGCTL(tk::g_negctl_frame) == 1) fr.Rnd(1);  // one extra draw of the game's generator per frame
  io.stop_at_wait = in.stop_at_wait;
  io.async = &in.async_keys;
  std::memcpy(io.async_key, in.async_start, sizeof io.async_key);
  io.on_wait = in.on_wait;
  io.on_wait_arg = in.on_wait_arg;
  io.h1_commands = in.commands.empty() ? nullptr : &in.commands;
  try {
    return in.boot ? fr.boot(F) : fr.run(F, in.retile);
  } catch (const tk::StopAtWait &) {
    return FrameExit::Wait;
  }
}
}  // namespace tk

FrameExit tick_frame(Ctx &ctx, const FrameInput &in) {
  tk::FrameIoStats st;
  return tk::tick_frame_io(ctx, in, st);
}

std::vector<std::pair<std::string, uint64_t>> tick_callee_counts() {
  std::map<std::string, uint64_t> by_name;
  for (const auto &kv : tk::t_counts) by_name[kv.first] += kv.second;
  for (const auto &kv : tk::t_counts_off) by_name[kv.first] += kv.second;
  std::vector<std::pair<std::string, uint64_t>> v(by_name.begin(), by_name.end());
  return v;
}
void tick_callee_counts_reset() {
  tk::t_counts.clear();
  tk::t_counts_off.clear();
}

// ------------------------------------------------------------------------------------------------ trace slices
// A trace slice (re/model/trace_schema.md) gives, per frame: the clock values by call site, the cursor reads, the
// GetAsyncKeyState reads, the recorded DISPATCHES (root-handler calls, one batch per pump call that delivered messages) and
// the harness's memory writes (DELIVER w32). The input model is re/analysis/sim/time_model.md section 7.
namespace {

// Code ranges as offsets from the frame-head return address (the trace's call sites are VAs of golf_clean.exe, which has no
// relocations; offsets keep image addresses out of this file): the root-window handlers rootWin_onPress1 .. the end of
// rootWin_onPress2b, uiKeyEvent, Window::dispatchAccel. A clock read inside them is the handler's own (it does not end a
// dispatch batch), as port/test/trace_commands.py reads them.
constexpr int32_t kHandlersLo = 0x55951, kHandlersHi = 0x55c71;
constexpr int32_t kUiKeyLo = 0x85ce1, kUiKeyHi = 0x85dd1;
constexpr int32_t kDispatchLo = 0x6cb11, kDispatchHi = 0x6ccd1;
// jgld.dll's Palette::update QPC return (module id 1, rva 0x6a414, trace_schema.md section 1): the P1..P4 clock reads right
// before H1 (time_model.md section 7)
constexpr uint32_t kPaletteQpcRa = 0x80000000u | (1u << 24) | 0x6a414u;

bool in_handler_code(int32_t site) {
  return (site >= kHandlersLo && site < kHandlersHi) || (site >= kUiKeyLo && site < kUiKeyHi) ||
         (site >= kDispatchLo && site < kDispatchHi);
}

// One recorded dispatch: the root-handler calls one pump call made (consecutive HANDLER records with no clock read between
// them but the handlers' own), the clock ordinal before it, the number of the frame's cursor reads before it, and the kind
// of pump that made it, from the clock site of the last read before it: the frame head or the palette QPC = a pump of the
// frame body (P6's doAllTasks or H1's pumpMessages); any other site (pumpAndRedraw, waitTicks) = a modal loop. The
// cursor count says WHICH pump: the frame body reads the cursor in P6's status bar (then capsLockOff), after P6's
// doAllTasks, and in H1 after its pumpMessages and key takes (course_season/03 frame 1: CURSOR, KBSTATE, the dispatch,
// CURSOR, TAKE, TAKE, CURSOR), and a screen's loop reads it once per pass; the port makes the same reads in the same order.
struct TBatch {
  uint32_t pos = 0, cursors = 0;
  bool body = false;
  // U-1051: the next clock read after the dispatch's handlers is jgld's Palette::update QPC although the pump before it was
  // not one of the frame body's (the last clock site was a wait loop's): a doAllTasks of a world load
  // (loadFlic +0x54: the 15th of the load in cov_r_flic_input) took it, a message posted before the load started; the harness's
  // polls ride on timeGetTime reads and the load makes none
  bool load = false;
  std::vector<Command> cmds;
};
// A harness memory write (DELIVER kind 4, a dword) and the clock ordinal it was made in. Roadmap S3.30 (U-0681): or a function
// the harness CALLED on the main thread from inside a clock call (DELIVER kind 7: va = the function's address, fn = the name the
// port registers it under, argc / args its int arguments); both kinds apply in one trace-ordered list (TraceInput::apply).
struct TWrite {
  uint32_t pos = 0, va = 0, value = 0;
  bool call = false;
  const char *fn = nullptr;  // kInjectedCalls' name of va, nullptr = an address the table does not hold
  uint32_t argc = 0;
  uint32_t args[2] = {0, 0};
};

// The functions a harness calls (DELIVER call), as offsets from the frame-head return address (no image address in this file),
// and the hooks.csv name the port registers each under. golfer_events' landmark notice: log/c3/s36a_run.py sc_golfer_events
// calls LANDMARK_NOTICE_FN (re/tools/scenario.py; hooks.csv announceLandmark, a cdecl void (golfer, story)) after the golfer
// panel checkpoint.
struct InjectedCall {
  int32_t off;
  const char *name;
};
constexpr InjectedCall kInjectedCalls[] = {{0x62691, "announceLandmark"}};

struct TraceFrame {
  uint32_t index = 0, pos = 0, t = 0;
  uint32_t flags = 0;                               // g_flags in the FRAME record's snapshot (trace_schema.md, +0x54)
  uint32_t fms_pos = UINT32_MAX;                    // the clock ordinal of H1's g_frameMs read (none: the frame has no wait)
  std::vector<std::pair<int32_t, uint32_t>> clock;  // tgt calls of the frame: (site offset, value)
  std::vector<uint32_t> clock_pos;                  // the clock ordinal of each (CLOCKRUN first + k): TraceInput::clock
  std::vector<std::pair<int32_t, int32_t>> cursor;  // CURSOR records of the frame (successful reads)
  std::vector<uint32_t> cursor_pos;                 // the clock ordinal of each (phase 5: gfx snapshots)
  std::vector<std::pair<uint8_t, uint16_t>> async;  // GetAsyncKeyState reads outside the root handlers (vk, result)
  std::vector<TBatch> batches;
  std::vector<TWrite> head_writes;                  // made inside the frame-head clock call: before the frame body
  std::vector<TWrite> writes;                       // made later in the frame, in clock order
};

uint32_t rd32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

struct TraceEnd {
  bool has_poll = false;
  uint32_t pos = 0;      // the final POLL's position (clock calls before it)
  int32_t site = 0;      // the call site of the clock call it rides on (offset from the frame-head ra)
  bool at_frame = false; // a FRAME record of that same clock call follows
  // the clock call it rides on comes after the last frame's H1 wait had ended: the checkpoint was taken later in that
  // frame (an H2 modal screen, the title menu), not in the wait (phase 3.8, stream 38j)
  bool after_wait = false;
};

// The FRAME records and what the trace says between them (schema v1, re/model/trace_schema.md).
bool parse_trace(const uint8_t *tr, size_t n, std::vector<TraceFrame> &frames, TraceEnd &end, std::string *why,
                 bool boot = false) {
  if (n < 32 || std::memcmp(tr, "SGTRACE\0", 8) != 0) {
    if (why) *why = "no SGTRACE header";
    return false;
  }
  const uint32_t hdr = uint32_t(tr[10]) | uint32_t(tr[11]) << 8;
  const uint32_t meta_len = rd32(tr + 12);
  const uint32_t step = rd32(tr + 16);
  const std::string meta(reinterpret_cast<const char *>(tr + hdr), meta_len);
  const size_t fk = meta.find("\"frame_ra\":");
  if (fk == std::string::npos) {
    if (why) *why = "trace metadata has no frame_ra";
    return false;
  }
  const uint32_t frame_ra = static_cast<uint32_t>(std::strtoul(meta.c_str() + fk + 11, nullptr, 10));
  size_t p = hdr + meta_len;
  std::vector<std::pair<int32_t, uint32_t>> pending;
  std::vector<uint32_t> pending_pos;
  struct Run {
    uint32_t first, count;
    int32_t site;
    uint32_t v0;
  };
  std::vector<Run> runs;
  // boot: frame 0 holds the records before the first FRAME (process start .. the first frame head)
  if (boot) frames.emplace_back();
  // dispatch batching (port/test/trace_commands.py batches(), the same rules)
  bool open_batch = false;
  uint32_t ctx_ra = frame_ra;
  TBatch *cur = nullptr;  // the last batch (a handler's own clock / key reads belong to its last command)
  // jgld's window procedure reads the cursor once for every WM_MOUSEMOVE it is given (in all 60 traces each of the 5,422
  // MSG 0x200 records is followed by one CURSOR record at its position, and no other message is): a read of the original's
  // engine, made inside a dispatch, which the port does not run. It is left out of the frame's cursor reads, so the count
  // that says which pump takes a dispatch counts only the reads the port makes (cov_a_golfer frame 2623: two mouse moves in
  // showHoleStatsScreen's loop had put its closing click two passes late). Negative control tick.frame 6 keeps them.
  bool after_mousemove = false;
  // KEYS THE WINDOW TREE'S FOCUS CHILD TOOK (roadmap S1.5, U-1032; log/roadmap/S1/5_notes.md). A WM_KEYDOWN reaches the
  // root's onChar (HANDLER 8; a key with no character, MapVirtualKeyA 0, the root's onKey with a0 = 0: HANDLER 7) and a
  // WM_CHAR the root's onKey (HANDLER 7) only when no focused child window returns non-zero for it first (uiKeyEvent ->
  // onCommand -> dispatchAccel, uiKeyEvent148 -> Window::onKeyChar -> Window::key, which ask the root's focus ring before
  // the root's own slot). A key MSG record that no HANDLER record follows before the next MSG record, clock read or frame
  // head was taken by such a child (the character editor's bio EditBox: EditBox::onKey, EditBox::insertChar): it becomes a
  // RootHandler command 12 (WM_KEYDOWN, a0 = the virtual key) or 13 (WM_CHAR, a0 = the character), which apply_command
  // treats as nothing and the bio entry routes to its EditBox (chareditor.cpp). Such commands only join a batch some
  // HANDLER record opens (the open one, or the next one of the same frame), so no batch is added. In the 64 corpus traces
  // every one of the 11,312 WM_KEYDOWN / WM_CHAR MSG records is followed by a root handler, so none of them gets one.
  struct PendKey {
    bool on = false;
    uint32_t msg = 0, wp = 0;
  } pend_key;
  std::vector<Command> focus_held;
  auto focus_resolve = [&]() {  // the pending key MSG record got no matching root handler
    if (!pend_key.on) return;
    Command c;
    c.kind = CmdKind::RootHandler;
    c.handler = pend_key.msg == 0x100 ? 12u : 13u;
    c.a0 = static_cast<int32_t>(pend_key.wp);
    if (SG_NEGCTL(tk::g_negctl_frame) != 8) focus_held.push_back(c);  // negative control 8: the commands dropped
    pend_key.on = false;
  };
  auto focus_flush = [&]() {  // into the open batch, after the handlers it holds so far
    if (open_batch && cur && !focus_held.empty()) {
      cur->cmds.insert(cur->cmds.end(), focus_held.begin(), focus_held.end());
      focus_held.clear();
    }
  };
  while (p + 4 <= n) {
    const uint8_t type = tr[p], sub = tr[p + 1];
    const uint32_t len = uint32_t(tr[p + 2]) | uint32_t(tr[p + 3]) << 8;
    const uint8_t *q = tr + p + 4;
    if (p + 4 + len > n) break;
    const bool window_proc_read = after_mousemove && type == 0x08;
    after_mousemove = type == 0x05 && len >= 8 && rd32(q + 4) == 0x200 && SG_NEGCTL(tk::g_negctl_frame) != 6;
    if (window_proc_read) {
      // the window procedure's read (above): not one of the frame's
    } else if (type == 0x01) {  // CLOCKRUN: first, count, ra, slot, v0 (u64); kind 1 tgt, 2 qpc
      const uint32_t first = rd32(q), count = rd32(q + 4), ra = rd32(q + 8);
      const uint32_t v0 = rd32(q + 16);
      const int32_t site = static_cast<int32_t>(ra - frame_ra);
      if (sub == 1)
        for (uint32_t k = 0; k < count; ++k) {
          pending.emplace_back(site, v0 + k * step);
          pending_pos.push_back(first + k);
        }
      runs.push_back({first, count, site, sub == 1 ? v0 : 0u});
      if (sub == 1 && site == tk::kSiteFrameMs && !frames.empty() && frames.back().fms_pos == UINT32_MAX)
        frames.back().fms_pos = first;
      if (in_handler_code(site)) {
        // the deadline tail of onPoint / onUnpoint / onKey reads timeGetTime (Command::t)
        if (cur && !cur->cmds.empty() && sub == 1) {
          Command &c = cur->cmds.back();
          if (c.handler == 2 || c.handler == 3 || c.handler == 7) c.t = v0;
        }
      } else {
        focus_resolve();
        focus_flush();
        if (open_batch && cur && !cur->body && sub == 2 && ra == kPaletteQpcRa) cur->load = true;   // U-1051
        ctx_ra = ra;
        open_batch = false;
      }
    } else if (type == 0x05 && len >= 16) {  // MSG: pos, msg, wParam, lParam (the keys the focus child may take, above)
      focus_resolve();
      focus_flush();
      const uint32_t msg = rd32(q + 4);
      if (msg == 0x100 || msg == 0x102) pend_key = {true, msg, rd32(q + 8)};
    } else if (type == 0x08 && sub == 1) {  // CURSOR: pos, x, y (client coordinates after ScreenToClient)
      if (!frames.empty()) {
        frames.back().cursor.emplace_back(static_cast<int32_t>(rd32(q + 4)), static_cast<int32_t>(rd32(q + 8)));
        frames.back().cursor_pos.push_back(rd32(q));
      }
    } else if (type == 0x06 && sub == 2) {  // KEYSTATE GetAsyncKeyState: pos, ra, vk, result
      const int32_t site = static_cast<int32_t>(rd32(q + 4) - frame_ra);
      const uint32_t vk = rd32(q + 8);
      const uint16_t res = static_cast<uint16_t>(rd32(q + 12) & 0xffff);
      if (site >= kHandlersLo && site < kHandlersHi) {  // the press handlers' Ctrl / Shift (Command::async_*)
        if (cur && !cur->cmds.empty()) {
          if (vk == 0x11) cur->cmds.back().async_ctrl = res;
          else if (vk == 0x10) cur->cmds.back().async_shift = res;
        }
      } else if (!frames.empty()) {
        frames.back().async.emplace_back(static_cast<uint8_t>(vk), res);
      }
    } else if (type == 0x09) {  // HANDLER (id = sub): pos, a0, a1
      pend_key.on = false;  // the key reached a root handler
      if (!frames.empty()) {
        TraceFrame &f = frames.back();
        if (!open_batch) {
          TBatch b;
          b.pos = rd32(q);
          b.cursors = static_cast<uint32_t>(f.cursor.size());
          b.body = ctx_ra == frame_ra || ctx_ra == kPaletteQpcRa;
          f.batches.push_back(std::move(b));
          open_batch = true;
        }
        cur = &f.batches.back();
        focus_flush();
        Command c;
        c.kind = CmdKind::RootHandler;
        c.handler = sub;
        c.a0 = static_cast<int32_t>(rd32(q + 4));
        c.a1 = static_cast<int32_t>(rd32(q + 8));
        cur->cmds.push_back(c);
      }
    } else if (type == 0x03 && sub == 4) {  // DELIVER w32: pos, addr, value
      if (!frames.empty()) frames.back().writes.push_back({rd32(q), rd32(q + 4), rd32(q + 8)});
    } else if (type == 0x03 && sub == 7 && len >= 20) {  // DELIVER call: pos, addr, argc, a0, a1 (roadmap S3.30)
      if (!frames.empty()) {
        TWrite w;
        w.pos = rd32(q);
        w.va = rd32(q + 4);
        w.call = true;
        w.argc = std::min<uint32_t>(rd32(q + 8), 2);
        w.args[0] = rd32(q + 12);
        w.args[1] = rd32(q + 16);
        const int32_t off = static_cast<int32_t>(w.va - frame_ra);
        for (const InjectedCall &c : kInjectedCalls)
          if (c.off == off) w.fn = c.name;
        frames.back().writes.push_back(w);
      }
    } else if (type == 0x02) {  // FRAME: frame, pos, t, snapshot
      focus_resolve();
      focus_flush();
      focus_held.clear();  // a taken key with no batch of its frame to join (none in the corpus)
      TraceFrame f;
      f.index = rd32(q);
      f.pos = rd32(q + 4);
      f.t = rd32(q + 8);
      f.flags = rd32(q + 12 + 0x54);
      if (!frames.empty()) {
        // the frame-head call itself (the last clock call before this record) belongs to the new frame
        if (!pending.empty()) {
          pending.pop_back();
          pending_pos.pop_back();
        }
        frames.back().clock = pending;
        frames.back().clock_pos = pending_pos;
        // writes made inside the frame-head call (pos = the new frame's pos) are seen by the new frame's body
        auto &w = frames.back().writes;
        while (!w.empty() && w.back().pos >= f.pos) {
          f.head_writes.insert(f.head_writes.begin(), w.back());
          w.pop_back();
        }
      }
      pending.clear();
      pending_pos.clear();
      cur = nullptr;
      open_batch = false;
      ctx_ra = frame_ra;
      frames.push_back(std::move(f));
    } else if (type == 0x04 && (sub & 2)) {  // POLL final: pos, index, len, name
      end.has_poll = true;
      end.pos = rd32(q);
    } else if (type == 0x7f) {
      break;
    }
    p += 4 + len;
  }
  if (!frames.empty()) {
    frames.back().clock = pending;
    frames.back().clock_pos = pending_pos;
  }
  for (TraceFrame &f : frames) {  // a write in the frame-head call of a frame's own record
    auto &w = f.writes;
    while (!w.empty() && w.front().pos <= f.pos) {
      f.head_writes.push_back(w.front());
      w.erase(w.begin());
    }
  }
  if (end.has_poll) {
    end.at_frame = !frames.empty() && frames.back().pos == end.pos;
    if (!frames.empty()) {
      const TraceFrame &lf = frames.back();
      const uint32_t limit = lf.t + ((((lf.flags & (1u << 17)) != 0 ? 0x4Bu : 0u) + 0xAFu) / 2u);
      uint32_t ended_at = UINT32_MAX;
      for (const Run &r : runs)
        if (ended_at == UINT32_MAX && r.first >= lf.pos && r.count > 0 &&
            (r.site == tk::kSiteFrameMs + 0x20 || r.site == tk::kSiteFrameMs + 0x4f))
          for (uint32_t k = 0; k < r.count && ended_at == UINT32_MAX; ++k)
            if (r.v0 + k * step >= limit) ended_at = r.first + k;
      end.after_wait = ended_at != UINT32_MAX && end.pos >= 1 && end.pos - 1 > ended_at;
    }
    for (const Run &r : runs)
      if (end.pos >= 1 && r.first <= end.pos - 1 && end.pos - 1 < r.first + r.count) end.site = r.site;
  }
  if (frames.empty()) {
    if (why) *why = "trace has no FRAME record";
    return false;
  }
  return true;
}

// What the input wiring saw over a run (sg_run_frames_detail): batches the port's pumps took and the ones left at a frame's
// end, and cursor / GetAsyncKeyState reads the port made beyond or short of the frame's records. Any of these non-zero means
// the port's frame took another path through its pumps or reads than the original; the first frame of each is kept.
struct InputStats {
  uint64_t delivered = 0, undelivered = 0, misaligned = 0, cursor_over = 0, cursor_under = 0, async_over = 0, async_under = 0;
  int32_t first_undelivered = -1, first_misaligned = -1, first_cursor = -1, first_async = -1;
  // the clock queries of wall-time loops (InputSource::clock): served, refused (no read at the site left), and dispatches
  // delivered after the port had taken a clock read the original made only after that dispatch (late)
  uint64_t clock_served = 0, clock_none = 0, clock_late = 0;
  int32_t first_clock = -1;
  std::string first_undelivered_what;
  std::string text() const {
    std::string s = "input: " + std::to_string(delivered) + " dispatches delivered, " + std::to_string(undelivered) +
                    " left";
    if (first_undelivered >= 0) s += " (first: record " + std::to_string(first_undelivered) + " " + first_undelivered_what + ")";
    s += ", " + std::to_string(misaligned) + " at another cursor count";
    if (first_misaligned >= 0) s += " (first: record " + std::to_string(first_misaligned) + ")";
    s += "; cursor reads over/under " + std::to_string(cursor_over) + "/" + std::to_string(cursor_under);
    if (first_cursor >= 0) s += " (first: record " + std::to_string(first_cursor) + ")";
    s += "; async-key reads over/under " + std::to_string(async_over) + "/" + std::to_string(async_under);
    if (first_async >= 0) s += " (first: record " + std::to_string(first_async) + ")";
    if (clock_served || clock_none || clock_late) {  // only runs with a wall-time loop say anything here
      s += "; clock queries " + std::to_string(clock_served) + " served, " + std::to_string(clock_none) + " none left, " +
           std::to_string(clock_late) + " dispatches late";
      if (first_clock >= 0) s += " (first: record " + std::to_string(first_clock) + ")";
    }
    return s;
  }
};

}  // namespace

namespace tk {
namespace {

// The InputSource of a trace run: each pump call of the port takes the recorded dispatch of the matching original pump call,
// in order. A pump of the frame body (FrameIo::pump_site P6 / H1) takes the frame's next body dispatch once the frame has made
// as many cursor reads as the original had before it; a Blocking pump of a modal loop takes the next modal dispatch the same
// way (a loop that reads no cursor, pumpAndRedraw's, takes it at its second refused call: its passes do not move the count);
// a Drain pump (pumpLight's) takes none: a message arrives between two pump calls only through a clock call (the harness
// posts from inside timeGetTime), which ends a batch and gives the next dispatch its own clock site (commands.h, MODAL
// BLOCKS; U-0941). A pump never takes a dispatch of the other class: the run reports it as left (InputStats) and the state
// diverges there.
class TraceInput : public InputSource {
 public:
  explicit TraceInput(Ctx &c) : ctx_(c) {}
  void begin(const TraceFrame &f, int32_t record) {
    fr_ = &f;
    next_ = 0;
    wnext_ = 0;
    cnext_ = 0;
    pace_site_ = 0;
    refused_at_ = SIZE_MAX;
    record_ = record;
    if (SG_NEGCTL(g_negctl_frame) != 5)
      for (const TWrite &w : f.head_writes) apply(w);
    // negative control 22 (roadmap S3.30): the frame's injected calls run at its start, before the frame body, not at their
    // trace position
    if (SG_NEGCTL(g_negctl_frame) == 22)
      for (const auto *list : {&f.head_writes, &f.writes})
        for (const TWrite &w : *list)
          if (w.call) run_call(w);
  }
  // H1's frame wait: the writes made in the frame's clock calls up to the first dispatch after H1's g_frameMs read (a
  // dispatch of an H2 modal); the wait itself dispatches nothing (time_model.md section 7)
  static void on_wait(void *self) {
    TraceInput &t = *static_cast<TraceInput *>(self);
    uint32_t limit = UINT32_MAX;
    for (size_t i = t.next_; t.fr_ && i < t.fr_->batches.size(); ++i)
      if (t.fr_->batches[i].pos > t.fr_->fms_pos) {
        limit = t.fr_->batches[i].pos;
        break;
      }
    t.writes_before(limit);
  }
  // partial = the frame stopped in its wait (an end checkpoint there): what comes after it is neither applied nor counted
  void end(InputStats &st, bool partial = false) {
    st.delivered += delivered_;
    st.misaligned += misaligned_;
    if (misaligned_ && st.first_misaligned < 0) st.first_misaligned = record_;
    delivered_ = misaligned_ = 0;
    st.clock_served += clock_served_;
    st.clock_none += clock_none_;
    st.clock_late += clock_late_;
    if ((clock_none_ || clock_late_) && st.first_clock < 0) st.first_clock = record_;
    clock_served_ = clock_none_ = clock_late_ = 0;
    if (partial) {
      fr_ = nullptr;
      return;
    }
    writes_before(UINT32_MAX);
    if (fr_ && next_ < fr_->batches.size()) {
      st.undelivered += fr_->batches.size() - next_;
      if (st.first_undelivered < 0) {
        const TBatch &b = fr_->batches[next_];
        st.first_undelivered = record_;
        st.first_undelivered_what = std::string(b.body ? "body" : "modal") + " dispatch after " +
                                    std::to_string(b.cursors) + " cursor reads, handlers";
        for (const Command &c : b.cmds) st.first_undelivered_what += " " + std::to_string(c.handler);
      }
    }
    fr_ = nullptr;
  }
  bool batch(PumpKind kind, std::vector<Command> &out) override {
    out.clear();
    if (!fr_ || next_ >= fr_->batches.size()) return false;
    const FrameIo &io = cur_io();
    const TBatch &b = fr_->batches[next_];
    if (kind == PumpKind::Load) {  // loadFlic's pump (U-1051): the dispatch the trace puts before the load's first palette QPC
      if (b.body || !b.load || SG_NEGCTL(g_negctl_frame) == 8) return false;
    } else if (io.pump_site == kPumpSiteModal ? (kind != PumpKind::Blocking || b.body) : !b.body) {
      return false;
    }
    if (b.body && SG_NEGCTL(g_negctl_frame) == 3) return false;  // negative control: the frame body's dispatches dropped
    if (io.cursor_used < b.cursors) {  // the original made more cursor reads before this dispatch: a later pump's
      const bool stuck = kind == PumpKind::Blocking && refused_at_ == io.cursor_used;
      refused_at_ = io.cursor_used;
      if (!stuck) return false;
    }
    if (kind == PumpKind::Blocking && pace_site_ != 0 && unread_before(pace_site_, b.pos) > 1) return false;  // PACE
    refused_at_ = SIZE_MAX;
    if (io.cursor_used != b.cursors) ++misaligned_;
    if (cnext_ > 0 && fr_->clock_pos[cnext_ - 1] >= b.pos) ++clock_late_;
    writes_before(b.pos);
    gfx_progress_fast(ctx_, b.pos);
    out = b.cmds;
    if (const std::vector<Command> *rep = batch_override(ctx_, b.pos)) out = *rep;  // phase 3.8 gate (d): a replacement
    ++next_;
    ++delivered_;
    return true;
  }
  // a Blocking pump waits for the frame's next modal dispatch; another kind next (or none) = no input for it. TAIL IDLE (the
  // boot of a trace with no frame head, phase 5): past the frame's last dispatch a modal loop still makes the idle passes the
  // original made, one per unread CURSOR record (each pass must read one, or the run ends there)
  bool exhausted() const override {
    const bool ex = !fr_ || next_ >= fr_->batches.size() || fr_->batches[next_].body;
    if (ex && tail_idle_ && fr_ && next_ >= fr_->batches.size()) {
      const FrameIo &io = cur_io();
      if (io.cursor && io.cursor_used < io.cursor->size() && io.cursor_used != idle_mark_) {
        idle_mark_ = io.cursor_used;
        return false;
      }
    }
    return ex;
  }
  void set_tail_idle(bool on) { tail_idle_ = on; }
  // roadmap S7.1: told of every harness write this source applies (the trace host of the live run core records them);
  // roadmap S3.30: and of every injected call it runs (addr, argc, a0, a1)
  void (*write_hook)(void *, uint32_t va, uint32_t value) = nullptr;
  void (*call_hook)(void *, uint32_t addr, uint32_t argc, uint32_t a0, uint32_t a1) = nullptr;
  void *write_arg = nullptr;
  // the modal loops' cursor reads share the frame's CURSOR records with the frame body's (one read order)
  bool cursor(int32_t &x, int32_t &y) override {
    FrameIo &io = cur_io();
    if (io.cursor && io.cursor_used < io.cursor->size()) {
      if (io.cursor_pos && io.cursor_used < io.cursor_pos->size()) {
        // phase 5: the harness's memory writes made before this read (a modal loop with no dispatch between a write and the
        // reads after it, cov_b_location's date write on the location screen) apply here, as they did in the original
        if (SG_NEGCTL(g_negctl_frame) != 7) writes_before((*io.cursor_pos)[io.cursor_used]);
        gfx_progress_fast(ctx_, (*io.cursor_pos)[io.cursor_used]);
      }
      io.cursor_x = (*io.cursor)[io.cursor_used].first;
      io.cursor_y = (*io.cursor)[io.cursor_used].second;
      ++io.cursor_used;
    } else {
      ++io.cursor_overrun;
    }
    x = io.cursor_x;
    y = io.cursor_y;
    return true;
  }
  // a wall-time loop's clock read (commands.h, CLOCK): the frame's next timeGetTime record at `site` after the last record
  // this source returned (the frame body's own reads go through FrameIo::clock and do not move it)
  bool clock(int32_t site, uint32_t &t) override {
    if (fr_)
      for (size_t i = cnext_; i < fr_->clock.size(); ++i)
        if (fr_->clock[i].first == site) {
          t = fr_->clock[i].second;
          cnext_ = i + 1;
          ++clock_served_;
          // roadmap S2.3: the original has reached this read, so a checkpoint taken at or before its ordinal shows the frame
          // as it is now (showCreditsScreen's scroll reads the clock once per pass and dispatches nothing)
          // (negative control 11: clock reads advance no snapshot)
          if (i < fr_->clock_pos.size() && SG_NEGCTL(g_negctl_frame) != 11) gfx_progress_fast(ctx_, fr_->clock_pos[i] + 1);
          return true;
        }
    ++clock_none_;
    return false;
  }

 void pace(int32_t site) override { pace_site_ = site; }

 private:
  friend class TraceHost;
  // the frame's clock reads at `site` made before clock ordinal `pos` that clock() has not returned yet
  size_t unread_before(int32_t site, uint32_t pos) const {
    size_t n = 0;
    for (size_t i = cnext_; fr_ && i < fr_->clock.size() && fr_->clock_pos[i] < pos; ++i)
      if (fr_->clock[i].first == site) ++n;
    return n;
  }
  void writes_before(uint32_t pos) {
    if (SG_NEGCTL(g_negctl_frame) == 5) return;  // negative control: the harness's memory writes skipped
    while (fr_ && wnext_ < fr_->writes.size() && fr_->writes[wnext_].pos < pos) apply(fr_->writes[wnext_++]);
  }
  // one DELIVER w32 or DELIVER call, at its point of the run
  void apply(const TWrite &w) {
    if (!w.call) {
      ctx_.mem.store<uint32_t>(w.va, w.value);
      if (write_hook) write_hook(write_arg, w.va, w.value);
      return;
    }
    if (SG_NEGCTL(g_negctl_frame) == 22) return;  // negative control 22: already run at the frame's start (begin)
    run_call(w);
  }
  // An injected call (roadmap S3.30): the registered port function of the original's address, called as the harness called the
  // original's (cdecl, int arguments, result discarded) at the same point of the frame. Negative controls: 20 the call dropped,
  // 21 its last argument + 1.
  void run_call(const TWrite &w) {
    if (SG_NEGCTL(g_negctl_frame) == 20) return;
    if (!w.fn) throw MissingResource{"injected call (DELIVER call): an address tick.cpp kInjectedCalls does not name"};
    const int i = registry_find(w.fn);
    if (i < 0) throw MissingResource{w.fn};
    const FuncDef *f = registry_at(i, nullptr);
    if (f->arity != static_cast<int>(w.argc) || std::strcmp(f->abi, "default") != 0)
      throw MissingResource{"injected call (DELIVER call): argument count or convention differs from the registered function"};
    uint32_t v[16] = {0};
    for (uint32_t k = 0; k < w.argc; ++k) v[k] = w.args[k];
    if (SG_NEGCTL(g_negctl_frame) == 21 && w.argc > 0) v[w.argc - 1] += 1;
    Ret r;
    f->fn(ctx_, v, r);
    if (call_hook) call_hook(write_arg, w.va, w.argc, w.args[0], w.args[1]);
  }
  Ctx &ctx_;
  const TraceFrame *fr_ = nullptr;
  size_t next_ = 0, wnext_ = 0;
  size_t cnext_ = 0;  // the clock record after the last one clock() returned
  int32_t pace_site_ = 0;  // PACE: the clock site of the modal loop running (0 = none)
  int32_t record_ = 0;
  uint64_t delivered_ = 0, misaligned_ = 0;
  uint64_t clock_served_ = 0, clock_none_ = 0, clock_late_ = 0;
  size_t refused_at_ = SIZE_MAX;
  bool tail_idle_ = false;
  mutable size_t idle_mark_ = SIZE_MAX;
};

// The dispatch point of a pump of the frame body (doMouse / doAllTasks's showScreen loop, where jgld's pumps
// call the root handlers): the frame's H1 commands when the driver passed them, else the context's InputSource.
void dispatch(TkBase &t, PumpKind kind, const char *where) {
  if (t.io.pump_site == kPumpSiteH1 && t.io.h1_commands) {
    const std::vector<Command> *cmds = t.io.h1_commands;
    t.io.h1_commands = nullptr;
    apply_commands(t.ctx, *cmds);
    return;
  }
  InputSource *src = input_source(t.ctx);
  if (kind == PumpKind::Blocking && (!src || src->exhausted())) throw NeedInput{{where}};
  std::vector<Command> b;
  if (src && src->batch(kind, b)) apply_commands(t.ctx, b);
}

// pumpMessages (the c3dw.cpp hook): a zero click word [g_rootInput+0x1c] clears the button word [g_rootInput+0x10]; the
// PeekMessage loop removes only WM_MOUSEMOVE; doMouse dispatches (above); the click word is cleared. In H1 a pump
// that returns at once (Drain); in a modal loop of this module's bodies (showTournamentResults' close button) the loop ends
// only on input, so a Blocking pump. The result (doMouse's) is read by no caller in this module.
int lc_pump_messages(TkBase &t, const uint32_t *, int) {
  Ctx &ctx = t.ctx;
  const uint32_t ri = GS(g_rootInput);
  if (t.G32(ri + 0x1c) == 0) t.G32(ri + 0x10) = 0;
  dispatch(t, t.io.pump_site == kPumpSiteModal ? PumpKind::Blocking : PumpKind::Drain, "pumpMessages");
  t.G32(ri + 0x1c) = 0;
  return 0;
}
// doAllTasks (the c3dw.cpp hook): the input phase word, the display's showScreen loop (which dispatches what has
// arrived), gfxFlushA: only the dispatch reaches simulation state; the rest is the renderer's
int lc_pump_input_483bd0(TkBase &t, const uint32_t *, int) {
  dispatch(t, PumpKind::Drain, "doAllTasks");
  return 0;
}
struct LocalReg {
  LocalReg() {
    locals()["pumpMessages"] = lc_pump_messages;
    locals()["doAllTasks"] = lc_pump_input_483bd0;
  }
} g_local_reg;

// Roadmap S7.1: a LiveHost (portsim/sim/live.h) that answers every request of the live run core from a trace, with this
// file's parse_trace and TraceInput: the equivalence gate of port/src/sim/live.cpp (the trace as a live host gives what
// tick_run_frames gives). Live frame 0 is the trace's boot frame (frames[0]), live frame j its frames[j]. The answers
// follow the trace path exactly: the frame body's clock read takes the frame's first value at its site (TkBase::clock_site),
// cursor and GetAsyncKeyState reads take the frame's records in order and carry the last record into the next frame
// (tick_run_frames' after_frame), modal reads and pumps go to TraceInput (with its harness writes reported to the core).
class TraceHost : public LiveHost {
 public:
  explicit TraceHost(Ctx &c) : input_(c) {}
  bool load(const uint8_t *tr, size_t n, std::string *why) { return parse_trace(tr, n, frames_, end_, why, true); }

  void boot(Ctx &) override {
    input_.write_hook = &TraceHost::on_write;
    input_.call_hook = &TraceHost::on_call;
    input_.write_arg = this;
    start(0);
    if (frames_.size() < 2) input_.set_tail_idle(true);
  }
  bool frame_head(Ctx &, uint32_t frame, uint32_t &t) override {
    if (frame >= frames_.size()) return false;
    start(frame);
    t = frames_[frame].t;
    return true;
  }
  void frame_end(Ctx &, uint32_t) override {
    input_.end(stats_);
    const TraceFrame &f = frames_[k_];
    if (!f.cursor.empty()) {
      cx_ = f.cursor.back().first;
      cy_ = f.cursor.back().second;
    }
    for (const auto &a : f.async) keys_[a.first] = a.second;
  }
  bool clock(Ctx &, int32_t site, bool body, uint32_t &t) override {
    if (!body) return input_.clock(site, t);
    for (const auto &c : frames_[k_].clock)
      if (c.first == site) {
        t = c.second;
        return true;
      }
    t = frames_[k_].t;
    return true;
  }
  void cursor(Ctx &c, bool modal, int32_t &x, int32_t &y) override {
    const TraceFrame &f = frames_[k_];
    if (next_cursor_ < f.cursor.size()) {
      if (next_cursor_ < f.cursor_pos.size()) {
        if (modal && SG_NEGCTL(g_negctl_frame) != 7) input_.writes_before(f.cursor_pos[next_cursor_]);
        gfx_progress_fast(c, f.cursor_pos[next_cursor_]);
      }
      x_ = f.cursor[next_cursor_].first;
      y_ = f.cursor[next_cursor_].second;
    }
    ++next_cursor_;
    x = x_;
    y = y_;
  }
  uint16_t async_key(Ctx &, int vk) override {
    vk &= 0xff;
    if (SG_NEGCTL(g_negctl_frame) == 4) return 0;  // as frame_input's negative control 4
    const auto &a = frames_[k_].async;
    uint32_t i = anext_[vk];
    while (i < a.size() && a[i].first != vk) ++i;
    if (i < a.size()) {
      akey_[vk] = a[i].second;
      anext_[vk] = i + 1;
    } else {
      anext_[vk] = static_cast<uint32_t>(a.size());
    }
    return akey_[vk];
  }
  LiveReply pump(Ctx &, const LivePump &p, std::vector<Command> &out) override {
    return input_.batch(p.kind, out) ? LiveReply::Deliver : LiveReply::Decline;
  }
  bool exhausted() const override { return input_.exhausted(); }
  void pace(Ctx &, int32_t site) override { input_.pace(site); }
  void wait_point(Ctx &) override { TraceInput::on_wait(&input_); }
  bool blocking() const override { return false; }  // TraceInput's decline is the trace's own decision

 private:
  void start(uint32_t k) {
    k_ = k;
    input_.begin(frames_[k], static_cast<int32_t>(k));
    next_cursor_ = 0;
    x_ = cx_;
    y_ = cy_;
    std::memset(anext_, 0, sizeof anext_);
    std::memcpy(akey_, keys_, sizeof akey_);
  }
  static void on_write(void *self, uint32_t va, uint32_t value) {
    TraceHost &h = *static_cast<TraceHost *>(self);
    if (h.writes) h.writes->wrote(va, value);
  }
  static void on_call(void *self, uint32_t addr, uint32_t argc, uint32_t a0, uint32_t a1) {
    TraceHost &h = *static_cast<TraceHost *>(self);
    if (h.writes) h.writes->called(addr, argc, a0, a1);
  }
  TraceInput input_;
  std::vector<TraceFrame> frames_;
  TraceEnd end_;
  InputStats stats_;
  uint32_t k_ = 0;
  size_t next_cursor_ = 0;
  int32_t cx_ = 0, cy_ = 0, x_ = 0, y_ = 0;
  uint16_t keys_[256] = {}, akey_[256] = {};
  uint32_t anext_[256] = {};
};

}  // namespace
}  // namespace tk

// roadmap S7.1 (portsim/sim/live.h)
std::unique_ptr<LiveHost> make_trace_live_host(Ctx &ctx, const uint8_t *trace, size_t n, std::string *why) {
  auto h = std::make_unique<tk::TraceHost>(ctx);
  if (!h->load(trace, n, why)) return nullptr;
  return h;
}

int tick_run_frames(Ctx &ctx, const uint8_t *trace, size_t trace_len, const uint32_t *queued, int32_t n_queued,
                    int32_t max_frames, uint32_t *out_T, uint64_t *out_hashes, int32_t *frames_done, std::string *detail,
                    bool boot) {
  // Messages queued before the start frame head (start.json queued_messages) are dispatched inside the slice, so the slice's
  // HANDLER records already hold their handler calls: they need nothing else here.
  (void)queued;
  (void)n_queued;
  *frames_done = 0;
  std::vector<TraceFrame> frames;
  TraceEnd end;
  if (!parse_trace(trace, trace_len, frames, end, detail, boot)) return -7;  // SG_E_ARG
  const bool own_terrain = !has_terrain_api(ctx);
  std::unique_ptr<tk::HeadlessTerrain> headless;
  if (own_terrain) {
    headless = std::make_unique<tk::HeadlessTerrain>(ctx);
    set_terrain_api(ctx, headless.get());
  }
  tk::TraceInput input(ctx);
  InputSource *prev_src = input_source(ctx);
  set_input_source(ctx, &input);
  InputStats stats;
  struct Restore {
    Ctx &c;
    bool on;
    InputSource *src;
    ~Restore() {
      if (on) set_terrain_api(c, nullptr);
      set_input_source(c, src);
    }
  } restore{ctx, own_terrain, prev_src};
  tk::t_stop_path.clear();
  struct Detail {
    std::string *d;
    const InputStats &s;
    ~Detail() {
      if (!d) return;
      if (!tk::t_stop_path.empty()) {
        std::string p = "stopped in";
        for (const std::string &n : tk::t_stop_path) p += " " + n + (&n == &tk::t_stop_path.back() ? "" : " <");
        *d += (d->empty() ? "" : "; ") + p;
      }
      *d += (d->empty() ? "" : "; ") + s.text();
    }
  } detail_tail{detail, stats};
  const auto &sim = objects_of("sim");
  bool retile = false;
  int32_t cx = 0, cy = 0;     // the cursor position carried from frame to frame
  uint16_t keys[256] = {};    // GetAsyncKeyState's last value per key, carried
  auto frame_input = [&](const TraceFrame &f) {
    FrameInput in;
    in.t_head = f.t;
    in.clock = f.clock;
    in.retile = retile;
    in.cursor = f.cursor;
    in.cursor_pos = f.cursor_pos;
    in.cursor_start_x = cx;
    in.cursor_start_y = cy;
    if (SG_NEGCTL(tk::g_negctl_frame) != 4) {  // negative control 4: every GetAsyncKeyState read answers 0
      in.async_keys = f.async;
      std::memcpy(in.async_start, keys, sizeof keys);
    }
    in.on_wait = &tk::TraceInput::on_wait;
    in.on_wait_arg = &input;
    return in;
  };
  uint64_t undelivered_seen = 0;
  auto after_frame = [&](const TraceFrame &f, int32_t record, const tk::FrameIoStats &io) {
    input.end(stats);
    if (!f.cursor.empty()) {
      cx = f.cursor.back().first;
      cy = f.cursor.back().second;
    }
    for (const auto &k : f.async) keys[k.first] = k.second;
    stats.cursor_over += io.cursor_over;
    stats.cursor_under += io.cursor_under;
    stats.async_over += io.async_over;
    stats.async_under += io.async_under;
    if (stats.first_cursor < 0 && (io.cursor_over || io.cursor_under)) stats.first_cursor = record;
    if (stats.first_async < 0 && (io.async_over || io.async_under)) stats.first_async = record;
    // development: SG_TICK_INPUT_LOG=<file> appends one line per frame whose reads or dispatches differ from its records
    static const char *log = std::getenv("SG_TICK_INPUT_LOG");
    if (log && (io.cursor_over || io.cursor_under || io.async_over || io.async_under || stats.undelivered != undelivered_seen)) {
      if (FILE *fh = std::fopen(log, "a")) {
        std::fprintf(fh, "record %d frame %u: cursor %zu records over %llu under %llu; async %zu records over %llu under %llu; "
                     "dispatches %zu, left so far %llu\n", record, f.index, f.cursor.size(),
                     static_cast<unsigned long long>(io.cursor_over), static_cast<unsigned long long>(io.cursor_under),
                     f.async.size(), static_cast<unsigned long long>(io.async_over),
                     static_cast<unsigned long long>(io.async_under), f.batches.size(),
                     static_cast<unsigned long long>(stats.undelivered));
        std::fclose(fh);
      }
    }
    undelivered_seen = stats.undelivered;
  };
  // boot: frame 0 (process start .. the first frame head) through the boot step; its output record is record 0
  int32_t k0 = 1;
  if (boot && max_frames > 0) {
    if (frames.size() < 2) {
      // no frame head follows the boot (cov_b_location's capture stays on the location screen): the boot still runs over the
      // whole trace, which gives no record (phase 5 reads what it drew); it stops where the recorded input runs out
      FrameInput in = frame_input(frames[0]);
      in.boot = true;
      input.begin(frames[0], 0);
      input.set_tail_idle(true);
      tk::FrameIoStats io;
      FrameExit fex = FrameExit::Menu;
      try {
        fex = tk::tick_frame_io(ctx, in, io);
      } catch (const NeedInput &) {
        // the menus wait for input the trace no longer holds: the end of the recording
        gfx_end_of_input(ctx);
      }
      after_frame(frames[0], 0, io);
      if (detail) *detail = fex == FrameExit::Quit ? "trace has no FRAME record after the boot; mainLoop returned (the title menu's quit)"
                                                   : "trace has no FRAME record after the boot; the boot ran to the end of the trace";
      return 0;
    }
    FrameInput in = frame_input(frames[0]);
    in.boot = true;
    input.begin(frames[0], 0);
    tk::FrameIoStats io;
    FrameExit ex;
    try {
      ex = tk::tick_frame_io(ctx, in, io);
    } catch (...) {
      after_frame(frames[0], 0, io);
      throw;
    }
    after_frame(frames[0], 0, io);
    if (ex == FrameExit::Quit) {
      if (detail) *detail = "mainLoop returned (the title menu's quit) during the boot";
      return 0;
    }
    retile = false;
    out_T[0] = frames[1].t;
    const uint8_t *s = ctx.bytes();
    for (size_t j = 0; j < sim.size(); ++j) out_hashes[j] = object_hash(s, *sim[j]);
    *frames_done = 1;
    k0 = 2;
  }
  for (int32_t k = k0; k <= max_frames; ++k) {
    if (static_cast<size_t>(k) >= frames.size()) {
      if (detail) *detail = "trace slice has no further FRAME record";
      if (boot && static_cast<size_t>(k) == frames.size()) {
        // a boot run's last frame: no FRAME record follows it, so it gives no record, but it runs to the end of the
        // trace (cov_r_quit: back to the title menu and the QUIT block, whose files the caller reads afterwards)
        const TraceFrame &f = frames[k - 1];
        const FrameInput in = frame_input(f);
        input.begin(f, k);
        // roadmap S3.17 (U-1831): past the frame's last dispatch a modal loop still makes the idle passes the original made,
        // one per unread CURSOR record, as in the boot of a trace with no frame head (cov_a_champ: the title menu's hover loop
        // after a cancelled tournament draws and refreshes once per pass; without them its first pumpMessages found no
        // input and the loop never drew). Negative control tick.frame 13 leaves them off
        if (SG_NEGCTL(tk::g_negctl_frame) != 13) input.set_tail_idle(true);
        tk::FrameIoStats io;
        FrameExit ex = FrameExit::Wait;
        bool out_of_input = false;
        try {
          ex = tk::tick_frame_io(ctx, in, io);
        } catch (const NeedInput &) {
          // the last frame's Blocking pump finds no recorded input left: the end of the recording (roadmap S3.3, U-1120).
          // Every snapshot not yet taken is past the last item the run could consume
          out_of_input = true;
          gfx_end_of_input(ctx);
        } catch (...) {
          after_frame(f, k, io);
          throw;
        }
        // roadmap S3.14 (U-1394): the last frame also ends the recording when it runs to its end (the app's session stops at
        // the next frame head), so a snapshot still pending lies past the last item the run consumed, as above; the last
        // present of an app session sits in this frame once the course view animates. Negative control tick.frame 12 keeps
        // it pending
        if (!out_of_input && SG_NEGCTL(tk::g_negctl_frame) != 12) gfx_end_of_input(ctx);
        after_frame(f, k, io);
        if (detail)
          *detail = out_of_input ? "the last frame ran to the end of the trace: a Blocking pump with no input left"
                    : ex == FrameExit::Quit ? "the last frame ran to the end of the trace: mainLoop returned (the title menu's quit)"
                                            : "the last frame ran to the end of the trace";
      }
      return 0;
    }
    const TraceFrame &f = frames[k - 1];
    const FrameInput in = frame_input(f);
    input.begin(f, k);
    tk::FrameIoStats io;
    FrameExit ex;
    try {
      ex = tk::tick_frame_io(ctx, in, io);
    } catch (...) {
      after_frame(f, k, io);
      throw;
    }
    after_frame(f, k, io);
    if (ex == FrameExit::Menu || ex == FrameExit::Quit) {
      if (detail) *detail = ex == FrameExit::Quit ? "mainLoop returned (the title menu's quit)" : "the frame left to the title menu";
      return 0;
    }
    retile = ex == FrameExit::Retile;
    out_T[k - 1] = frames[k].t;
    const uint8_t *s = ctx.bytes();
    for (size_t j = 0; j < sim.size(); ++j) out_hashes[(k - 1) * sim.size() + j] = object_hash(s, *sim[j]);
    *frames_done = k;
  }
  // The end checkpoint: taken at the last frame head (a FRAME of the same clock call), inside that frame's H1 wait (the
  // frame wait's timeGetTime sites and waitTicks(1, 0)'s), where the last frame's body up to the wait has run, or inside a
  // modal wait of the last frame (pumpAndRedraw's clock reads, a screen loop's waitTicks): the original idled in a Blocking
  // pump whose input had not arrived, so the port runs the frame until that pump finds no recorded input left (NeedInput).
  // Which stop the port reached is reported; the end check compares the state there.
  if (end.has_poll && !end.at_frame && static_cast<size_t>(max_frames) + 1 == frames.size()) {
    const int32_t wait_sites[] = {tk::kSiteFrameMs, tk::kSiteFrameMs + 0x20, tk::kSiteFrameMs + 0x4f,
                                  tk::kSiteWaitTicks, tk::kSiteWaitTicks + 4, tk::kSiteWaitTicks + 0x30};
    bool in_wait = false;
    for (int32_t w : wait_sites) in_wait = in_wait || end.site == w;
    const TraceFrame &f = frames[max_frames];
    FrameInput in = frame_input(f);
    in.stop_at_wait = !end.after_wait;
    input.begin(f, max_frames + 1);
    tk::FrameIoStats io;
    std::string stop;
    try {
      const FrameExit ex = tk::tick_frame_io(ctx, in, io);
      stop = ex == FrameExit::Wait ? "the frame wait" : "the frame's end (no wait reached)";
    } catch (const NeedInput &e) {
      stop = std::string("a Blocking pump with no input left (") + (e.what ? e.what : "?") + ")";
      gfx_end_of_input(ctx);  // snapshots still pending lie past the last recorded item (S3.3)
    }
    input.end(stats, true);
    if (detail)
      *detail = "end checkpoint at site " + std::to_string(end.site) + (in_wait ? " (a frame-wait site)" : "") +
                ": the last frame ran to " + stop;
  }
  return 0;
}

// ------------------------------------------------------------------------------------------------ registry
namespace {
using tk::Rend;
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
using namespace tk::neg;

void a_reset_sprite_queue(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Rend(c).resetSpriteQueue()); }
void a_enqueue_sprite(Ctx &c, const uint32_t *a, Ret &) {
  Rend(c).enqueueSprite(S(a, 0), S(a, 1), S(a, 2), S(a, 3), S(a, 4), S(a, 5), S(a, 6), S(a, 7));
}
void a_add_sprite(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(Rend(c).addSprite(S(a, 0), S(a, 1), S(a, 2), S(a, 3), S(a, 4), S(a, 5), S(a, 6), S(a, 7)));
}
void a_draw_queued_sprite(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Rend(c).drawQueuedSprite(S(a, 0))); }
void a_snapshot_tables(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Rend(c).snapshotTables()); }
void a_render_sprite_queue(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Rend(c).renderSpriteQueue()); }
void a_cell_at(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(Rend(c).cellAt(S(a, 0), S(a, 1), S(a, 2))); }
void a_tile_to_screen(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Rend(c).tileToScreen(S(a, 0), S(a, 1), a[2], a[3])); }
void a_screen_to_tile(Ctx &c, const uint32_t *a, Ret &) { Rend(c).screenToTile(S(a, 0), S(a, 1), a[2], a[3], S(a, 4)); }
void a_build_scene_sprites(Ctx &c, const uint32_t *a, Ret &) {
  if (SG_NEGCTL(g_negctl_build_scene_sprites) == 1) return;  // nothing runs
  tk::Scene(c).buildSceneSprites(S(a, 0), S(a, 1), S(a, 2), S(a, 3), S(a, 4), S(a, 5), S(a, 6));
}
void a_draw_world_objects(Ctx &c, const uint32_t *, Ret &) { tk::World(c).drawWorldObjects(); }
void a_draw_walkers(Ctx &c, const uint32_t *, Ret &) {
  if (SG_NEGCTL(g_negctl_draw_walkers) == 1) return;  // nothing runs (the walkers' screen points stay)
  tk::Ball(c).drawWalkers();
}
void a_flush_course_info(Ctx &c, const uint32_t *, Ret &) { Rend(c).flushCourseInfo(); }
using tk::Leaves;
void a_start_message(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Leaves(c).startMessage(S(a, 0), S(a, 1), S(a, 2))); }
void a_show_tutorial_text(Ctx &c, const uint32_t *, Ret &) { Leaves(c).showTutorialText(); }
void a_append_cents(Ctx &c, const uint32_t *a, Ret &) { Leaves(c).appendCents(S(a, 0)); }
void a_log_tick(Ctx &c, const uint32_t *a, Ret &) { Leaves(c).logTick(S(a, 0), S(a, 1)); }
void a_draw_window(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Leaves(c).drawWindow(S(a, 0), S(a, 1), S(a, 2), S(a, 3))); }
void a_draw_thought_bubble(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(Leaves(c).drawThoughtBubble(S(a, 0), S(a, 1), S(a, 2), S(a, 3)));
}
void a_update_golfers(Ctx &c, const uint32_t *, Ret &r) {
  if (SG_NEGCTL(g_negctl_update_golfers) == 1) return r.set_i32(0x98);  // no golfer ticks
  r.set_i32(tk::Golfers(c).updateGolfers());
}
void a_view_to_local(Ctx &c, const uint32_t *a, Ret &) { Rend(c).viewToLocal(a[0], a[1], a[2]); }
void a_view_from_parent(Ctx &c, const uint32_t *a, Ret &) { Rend(c).fromParent(a[0], a[1], a[2]); }
void a_save_game(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Leaves(c).saveGame(a[0])); }
void a_draw_labels(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Leaves(c).drawLabels()); }
void a_draw_money_labels(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Leaves(c).drawMoneyLabels()); }
void a_match_update(Ctx &c, const uint32_t *a, Ret &) {
  if (SG_NEGCTL(g_negctl_match_update) == 1) return;  // nothing runs
  tk::Match(c).matchUpdate(S(a, 0));
}
void a_membership_report(Ctx &c, const uint32_t *a, Ret &) {
  if (SG_NEGCTL(g_negctl_membership_report) == 1) return;  // nothing runs
  tk::Members(c).membershipReport(a[0]);
}
void a_can_afford(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Leaves(c).canAfford(S(a, 0))); }
void a_rank_value(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Leaves(c).rankValue(S(a, 0))); }
void a_draw_home_layout(Ctx &c, const uint32_t *a, Ret &) {
  if (SG_NEGCTL(g_negctl_draw_home_layout) == 1) return;  // nothing runs (no record sprites, cells or animation bytes)
  tk::Panels(c).drawHomeLayout(S(a, 0), S(a, 1), S(a, 2));
}
void a_play_habitat_sound(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(Leaves(c).playHabitatSound(S(a, 0), S(a, 1), S(a, 2))); }
void a_add_score(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Leaves(c).addScore()); }
void a_hire_employee(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Leaves(c).hireEmployee()); }
void a_set_dirty_flag_a(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Rend(c).setDirtyFlagA()); }
void a_set_dirty_flag_b(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(Rend(c).setDirtyFlagB()); }

const FuncDef kFuncs[] = {
    {"resetSpriteQueue", "default", "int", 0, 0, a_reset_sprite_queue, &g_negctl_sprite_queue},
    {"enqueueSprite", "default", "void", 8, 0, a_enqueue_sprite, &g_negctl_sprite_queue},
    {"addSprite", "default", "int", 8, 0, a_add_sprite, &g_negctl_sprite_queue},
    {"drawQueuedSprite", "default", "int", 1, 0, a_draw_queued_sprite, &g_negctl_sprite_queue},
    {"snapshotTables", "default", "int", 0, 0, a_snapshot_tables, &g_negctl_snapshot_tables},
    {"renderSpriteQueue", "default", "int", 0, 0, a_render_sprite_queue, &g_negctl_sprite_queue},
    {"cellAt", "default", "pointer", 3, 0, a_cell_at, &g_negctl_cell_at},
    {"tileToScreen", "default", "int", 4, 0, a_tile_to_screen, &g_negctl_tile_to_screen},
    {"screenToTile", "default", "void", 5, 0, a_screen_to_tile, &g_negctl_screen_to_tile},
    {"buildSceneSprites", "default", "void", 7, 0, a_build_scene_sprites, &g_negctl_build_scene_sprites},
    {"drawWorldObjects", "default", "void", 0, 0, a_draw_world_objects, &g_negctl_draw_world_objects},
    {"drawWalkers", "default", "void", 0, 0, a_draw_walkers, &g_negctl_draw_walkers},
    {"flushCourseInfo", "default", "void", 0, 0, a_flush_course_info, &g_negctl_flush_course_info},
    {"startMessage", "default", "int", 3, 0, a_start_message, &g_negctl_start_message},
    {"showTutorialText", "default", "void", 0, 0, a_show_tutorial_text, &g_negctl_show_tutorial_text},
    {"appendCents", "default", "void", 1, 0, a_append_cents, &g_negctl_append_cents},
    {"logTick", "default", "void", 2, 0, a_log_tick, &g_negctl_log_tick},
    {"drawWindow", "default", "int", 4, 0, a_draw_window, &g_negctl_draw_window},
    {"drawThoughtBubble", "default", "int", 4, 0, a_draw_thought_bubble, &g_negctl_draw_thought_bubble},
    {"updateGolfers", "default", "int", 1, 0, a_update_golfers, &g_negctl_update_golfers},
    {"View_toLocal", "thiscall", "void", 3, 0, a_view_to_local, &g_negctl_view_to_local},
    {"View::fromParent", "thiscall", "void", 3, 0, a_view_from_parent, &g_negctl_view_from_parent},
    {"saveGame", "default", "int", 1, 0, a_save_game, &g_negctl_save_game},
    {"drawLabels", "default", "int", 0, 0, a_draw_labels, &g_negctl_draw_labels},
    {"drawMoneyLabels", "default", "int", 0, 0, a_draw_money_labels, &g_negctl_draw_money_labels},
    {"matchUpdate", "default", "void", 1, 0, a_match_update, &g_negctl_match_update},
    {"membershipReport", "default", "void", 1, 0, a_membership_report, &g_negctl_membership_report},
    {"canAfford", "default", "int", 1, 0, a_can_afford, &g_negctl_can_afford},
    {"rankValue", "default", "int", 1, 0, a_rank_value, &g_negctl_rank_value},
    {"drawHomeLayout", "default", "void", 3, 0, a_draw_home_layout, &g_negctl_draw_home_layout},
    {"playHabitatSound", "default", "int", 3, 0, a_play_habitat_sound, &g_negctl_play_habitat_sound},
    {"addScore", "default", "int", 0, 0, a_add_score, &g_negctl_add_score},
    {"hireEmployee", "default", "int", 0, 0, a_hire_employee, &g_negctl_hire_employee},
    {"setDirtyFlagA", "default", "int", 0, 0, a_set_dirty_flag_a, &g_negctl_set_dirty_flag_a},
    {"setDirtyFlagB", "default", "int", 0, 0, a_set_dirty_flag_b, &g_negctl_set_dirty_flag_b},
    // the frame step's own switch (no corpus key: sg_run_frames runs it); calling it does nothing
    {"tick.frame", "default", "void", 0, 0, [](Ctx &, const uint32_t *, Ret &) {}, &g_negctl_frame},
};
}  // namespace

const DefectDef kDefects[] = {
    {"mainLoop.cashLowWordOnly", "model_defects#23",
     "the month cash history and the new-course ledger entry store only the cash's low word (wraps above 32767)",
     &g_defect_cash_low_word},
    {"mainLoop.titleExitHotspotWide", "P5A",
     "the title menu's exit hotspot is the nearest-point test around the exit icon's top-left corner (about 0x140 x 0x50 px); "
     "fixed: only the icon's own 0x3a x 0x39 rectangle selects it",
     &g_defect_title_exit_hotspot},
    {"mainLoop.titleDefaultItem1", "P9A",
     "until the cursor first comes near a title-menu item, each pass starts with item 1 (Start New Game) selected: it is drawn "
     "highlighted and a click anywhere starts a new game; fixed: the pass starts with no item",
     &g_defect_title_default_item},
    {"mainLoop.titleHotspotRange", "P9A",
     "a labelled title-menu item is chosen within distance 0xa0 of its text centre (about 0x140 px wide, mostly outside its art); "
     "fixed: only over the item's lit button art (per row the hull of its opaque pixels; P23C, was its art box)",
     &g_defect_title_hotspot_range},
    {"mainLoop.titleEscIgnored", "P31C",
     "the title menu ignores Esc (its only exit is the exit icon); fixed: a fresh Esc pressed while the menu is shown acts as a "
     "click on the exit icon (quit: top10.sve, Default.pro, the closing movie, the credits)",
     &g_defect_title_esc},
    {"mainLoop.staffListCourseClickIgnored", "P31C",
     "with an employee picked in the Staff list a click on the course does not move him (only the Move button does); fixed: a "
     "left click on the course that selects nothing lands his move at the cursor tile, as the Move button's armed click does",
     &g_defect_staff_list_click},
    {"mainLoop.hiddenCheatKeys", "P32C",
     "the + key adds 10,000 cash and marks the cash with an asterisk, - and = lower / raise the terrain cell (also a + / - meant as "
     "zoom that reached the game while a screen was open); fixed: those three keys do nothing in the game's key switch",
     &g_defect_hidden_cheat_keys},
    {"loadGame.cheatMarkKept", "P35E",
     "the cash's asterisk (g_flags bit 29, set by the + cash key) is saved with the game, kept by every load and carried onto "
     "the next course of the career, so courses played before the + key was dropped keep showing it; fixed: a load and a move "
     "to another course clear it",
     &g_defect_cheat_mark_kept},
    {"mainLoop.terrainLiftCap13", "P35B",
     "the terrain tool raises a corner to level 13 at most (10 steps above the lowest ground, level 3) and lowers to 10 at most "
     "on every course type but Tropical (13 there), so a generated mountain corner at 15 drops to 13 on a raise and to 10 on a "
     "lower; fixed: raise and lower clamp to 3..18 (15 steps) on every course type and a press never moves a corner the other way",
     &g_defect_terrain_lift_cap},
    {"mainLoop.fallOverNearerGround", "P35B",
     "a waterfall's column and spray are drawn after the terrain picture, so a green or other ground nearer the camera that lies "
     "wholly above the fall's top never covers them; fixed: the fall's art pixels inside the drawn top of such nearer land tiles "
     "are not drawn (render-only)",
     &g_defect_fall_over_ground},
    {"mainLoop.vacatedPanelClick", "P9E",
     "a press in the bottom band's 800-space place goes to the System menu or the panel handlers even when the host shows the "
     "course there (the HUD moved to the window's edges); fixed: a press the host marked vacated (view block word +4) takes the "
     "world path",
     &g_defect_vacated_click},
    {"mainLoop.panelTopRowsToWorld", "P27A",
     "a press at x >= 0xc8 between y 0x1da (the panels' art top) and 0x1e0 takes the world path although the open panel's hit test "
     "lights a button there (its top rows); fixed: such a press that the panel's own hit test claims goes to the panel handler",
     &g_defect_panel_top_rows},
    {"mainLoop.messageDismissBand", "P27A",
     "a press dismisses the message window over the band x 0xc9..0x257 above its bottom while the window's frame is drawn at "
     "x 0xe8..0x287 (0x104..0x2a3 and 0x2c lower with a golfer or picture); fixed: only a press on the drawn frame dismisses it",
     &g_defect_msg_dismiss_band},
    {"mainLoop.cameraFollowOffGrid", "P6A",
     "the camera writers that follow a golfer, walker, ball or photo position (and the hole / object offsets) store no grid bound, "
     "so the view leaves the map; fixed: each clamps the camera tile pair to 0..0x31 like H1's key and edge-pan cases",
     &g_defect_camera_follow_offgrid},
    {"updateGolfers.walkOffGrid", "P6A",
     "a golfer's walking step has no grid test (reversal turn, step overshoot); fixed: the position is clamped to the grid after the step",
     &g_defect_golfer_walk_offgrid},
    {"updateGolfers.edgeNeighbourType", "model_defects#5",
     "the ball's edge-bit and green-neighbour type tests read tile_type with no grid test; fixed: a neighbour outside the grid "
     "has type 0x14", &g_defect_golfer_edge_type},
    {"mainLoop.edgeNeighbourType", "model_defects#5",
     "mainLoop's neighbour type tests (water decoration and side sprites, hole points, the edit's neighbour flag write) read "
     "tile_type with no grid test; fixed: a neighbour outside the grid has type 0x14", &g_defect_ml_edge_type},
    {"mainLoop.edgeNeighbourNoFlags", "model_defects#5",
     "the path, object kind 2 and flag-0x20 neighbour masks read tile_flags, and the water side sprites read a neighbour's wall "
     "mask, with no grid test; fixed: a cell outside the grid has no flags and a wall mask of 0 (orchestrator's choice)",
     &g_defect_ml_edge_no_flags},
    {"mainLoop.gridEdgeCorners", "play-test 17,P35C",
     "the terrain toolbar's grid projects a cell's corners with worldToScreen margin 0: the cell is skipped when its first corner "
     "is off the view (the edge column of cells is missing) and a far corner off the view is the flat-ground point, so on raised "
     "ground the lines at the view's edges run to wrong points; fixed: the corners are projected with a margin (each keeps its "
     "height) and the tile pass's tileToScreen alone decides which cells draw",
     &g_defect_ml_grid_edge},
    {"updateGolfers.partnerWaitBlocksTee", "P20A",
     "a golfer holds its shot while a golfer of its hole with more strokes stands near the tee, also one that already played and "
     "only waits there for its partner, whose own shot is held by the group ahead: they wait for each other for good; fixed: a "
     "golfer waiting at the tee for its partner does not hold the shots of the golfers ahead",
     &g_defect_partner_wait_blocks_tee},
    {"updateGolfers.playerWaitBlocksCourse", "P20A",
     "while the player's own golfer has a shot handed to the player and not played, it and its partner count in the tee queue of "
     "every later golfer of the hole and hold the shots near the tee: the whole course waits for the player; fixed: outside a "
     "championship or pro match the groups behind tee off and play through (the ball, target and line-of-fire tests stay)",
     &g_defect_player_wait_blocks_course},
    {"updateGolfers.playerPairBlocksShots", "P23B",
     "while the player's own golfer has a shot handed to the player and not played, it and its partner ahead on the hole hold the "
     "shots of every group behind by the ball-near-target, golfer-at-target and line-of-fire tests: the hole waits for the player; "
     "fixed: outside a championship or pro match they hold nobody's shot by those tests",
     &g_defect_player_pair_blocks_shots},
    {"updateGolfers.playerPairBlocksWalkers", "P36B",
     "while the player's own golfer has a shot handed to the player and not played, it and its partner standing on the tee box stop "
     "every golfer that walks into them (the walking step's in-the-way test): the groups behind cannot reach the tee and the hole "
     "jams; fixed: outside a championship or pro match the two are in nobody's way in that test and are not counted among the "
     "hole's golfers still to tee off there",
     &g_defect_player_pair_blocks_walkers},
    {"updateGolfers.partnerWaitsBallRest", "P36B",
     "the two golfers of a pair take turns and each waits until its partner's ball has come to rest (a golfer that teed off walks "
     "back to the tee to wait, the one nearer the cup, the lower index at the tee and the one watching stand meanwhile); fixed: "
     "outside a championship or pro match each lets go once the partner's ball is in the air or rolling, so the pair's shots overlap",
     &g_defect_partner_waits_ball_rest},
    {"updateGolfers.preShotGateByHole", "P36B",
     "at its ball, off the green, a golfer swings on a tick where Random::range(hole + 4) is 0, so the pause before a shot grows with "
     "the hole number; fixed: Random::range(5), hole 1's draw, on every hole",
     &g_defect_pre_shot_gate_by_hole},
    {"mainLoop.treesHiddenFarZoom", "P11A",
     "at the furthest zoom (g_zoomScale 1) the tile pass queues no tree, tropical tree or palm sprite (type class 0xd): forests "
     "show bare ground; fixed: they are drawn at zoom 1 too, at a quarter of zoom 4's size, while the sprite queue has room "
     "for the golfers queued after them",
     &g_defect_far_zoom_trees},
    {"mainLoop.zoomInStopsAt4", "P11A",
     "zooming in stops at g_zoomScale 4 (three levels 1 / 2 / 4); fixed: one level closer, 8 (the course, objects and golfers at "
     "twice zoom 4's size)",
     &g_defect_zoom_in_max4},
    {"mainLoop.toolForcesZoom4", "P11E",
     "while a build tool or selection is held the zoom is set to 4 every frame, and zooming out drops the tool; fixed: a held tool "
     "keeps every zoom (1, 2, 4, 8) and zooming out never drops it",
     &g_defect_tool_zoom4},
    {"mainLoop.zoom8Font", "P13C",
     "the golfer / ball labels pick their font by g_zoomScale > 4, so the closest zoom 8 (P11A) draws them in the larger "
     "g_font51b360 where zoom 4 uses g_fontRef_519fd8 (P26D: and drawWalkers' staff labels); fixed: zoom 8 keeps zoom 4's face",
     &g_defect_zoom8_font},
    {"mainLoop.cameraStepFixed4", "P11F",
     "an arrow key moves the camera 4 tiles at every zoom; fixed: 2 tiles at zoom 8 (the same screen distance as 4 tiles at zoom 4)",
     &g_defect_camera_step4},
    {"mainLoop.arrowKeyHeldRepeat", "P19A",
     "a held arrow key moves the camera on its press and again only once Windows' key repeat starts (its delay), then one step per "
     "frame; fixed: an arrow's key-down is kept in the held-key word until a key-up, and every frame without a key code takes that "
     "arrow (one step per frame from the press to the release) once the key has been down 250 ms (a shorter tap: one step)",
     &g_defect_arrow_held_repeat},
    {"mainLoop.pKeyPausesGame", "P21A",
     "the lower-case p opens the Improvements palette (paths, benches, trees) and only the capital P pauses; fixed: p and P pause "
     "(again resumes), and the Improvements palette opens on a (or A)",
     &g_defect_p_key_pauses},
    {"mainLoop.arrowStepAfterDraw", "P21B",
     "an arrow key moves the camera at the key switch after the frame has drawn its picture, so a press shows in the picture of the "
     "frame after the next; fixed: an arrow key-down the host took at the frame head moves the camera before the frame's picture "
     "(one game frame sooner on the screen)",
     &g_defect_arrow_step_after_draw},
    {"renderSpriteQueue.labelUnderLaterSprites", "P22C",
     "a golfer's name label is drawn right after the golfer in the depth-sorted sprite walk, so a tree or anything else in front "
     "covers it; fixed: the walk's name labels are drawn after its last sprite (the golfer itself stays behind what is in front)",
     &g_defect_label_under_sprites},
    {"mainLoop.shotButtonsReadBallTile", "P22B",
     "the shot-style panel enables or greys shot buttons 5..7 from the tile at the ball's x and the golfer's y (a mixed pair), so "
     "they can be enabled while the Lie text says rough; fixed: they read the tile at the ball, the tile the Lie text names",
     &g_defect_shot_lie_mixed_tile},
    {"updateGolfers.foreignGreenAutoPutt", "P22B",
     "a player's ball that stops on any green tile gets the automatic shot towards the current hole's cup, also when the green belongs "
     "to another hole; fixed: on a green not connected to the current hole's green the player chooses the direction and the shot type",
     &g_defect_foreign_green_auto},
    {"mainLoop.rightClickKeepsTool", "P13A",
     "a right click on the course with a terrain tool held keeps the tool and marks the tile for the right-click undo; fixed: on a "
     "tile the undo has nothing to act on (no recorded edit, no placed object) it drops the tool instead",
     &g_defect_rclick_keeps_tool},
    {"mainLoop.hKeyShotStyleFirst", "P13A",
     "while the player's golfer waits for its shot (panel mode 3) the 'h' key picks a shot style and never opens the hole being "
     "built; fixed: there it opens that hole when it has its tee and green and is not open yet, else it picks the shot style",
     &g_defect_h_shot_style},
    {"Tile::render.fairwayStripesFixed", "P13A",
     "a fairway's mowing stripes run one way whatever the paint tool's orientation (Tab turns sand rakes and green stripes only); "
     "fixed: a fairway tile painted at an odd orientation has its stripes turned 90 degrees, so Tab turns them",
     &g_defect_fairway_stripes_fixed},
    {"mainLoop.undoButtonHelpOnly", "P13A",
     "the Build palettes' Undo button (the bulldozer) only shows a help window about the right-click undo; fixed: it arms the "
     "game's bulldoze mode (each left click on the map removes that tile's last edit with its refund, or asks before demolishing a "
     "building), a second click, a right click, Esc or a new tool drops it, and its tip reads Bulldoze",
     &g_defect_undo_button_help},
    {"mainLoop.holeLineEndsOnScreen", "P19B",
     "a hole's shot line is drawn segment by segment and a segment with an end off screen is left out, so on a hole partly in view "
     "the line stops short of the screen's edge; fixed: that segment is drawn too, cut at the edge of the view",
     &g_defect_hole_line_ends_on_screen},
    {"mainLoop.simFotoWideFrame", "P31D,G33",
     "the SimFoto event's frame is clamped into the original's 800 screen (x 0x10 .. 0x310), so a golfer in a side column of the "
     "wide view gets a frame on the 800 frame's edge; fixed: the x clamps move out by (W - 800) / 2 on each side (W the view block's "
     "width) and the frame is drawn into the wide scene too",
     &g_defect_simfoto_wide_frame},
};

extern const ModuleDef kModule_tick = {"tick", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                       sizeof(kDefects) / sizeof(kDefects[0])};

// P36B: golfer_wait.h's entry points (the table lives in tk, beside updateGolfers)
void golfer_wait_set(int g, int why, int other) { tk::gw_set(g, why, other); }
int golfer_wait_reason(Ctx &ctx, int g, int &other) { return tk::gw_reason(ctx, g, other); }
const char *golfer_wait_id(int why) { return tk::gw_id(why); }
void golfer_wait_idle(int g) {
  if (g < 0 || g >= 0x98) return;
  const int t = tk::g_gw.tick[g];
  tk::gw_set(g, t > kGwBall && t < kGwStand ? t : kGwIdle, tk::g_gw.other[g]);
}

}  // namespace portsim
