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
#include "portsim/execonst.h"
#include "portsim/sim/integrator.h"
#include "portsim/sim/live.h"
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/util.h"
#include "portsim/state_hash.h"
#include "tick_hook.h"

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
// text, inside the golf ball). Fixed (0): an item is chosen only inside its button art's box (showTitleScreen's source
// rectangles showTitleScreen+0x2e4..showTitleScreen+0x3b9, which the menu draws at the item's position F+0x150 / F+0x152); among boxes holding the
// cursor the original's distance decides. The exit icon (hotspot 5) is mainLoop.titleExitHotspotWide's.
int g_defect_title_hotspot_range = 1;
// mainLoop.vacatedPanelClick (P9E fix 3, the widescreen HUD of G32 / P9B: "a click where a moved panel used to be still reaches
// that panel"): the frame's click dispatch sends a press below y 0x1b8 to the System menu (x < 0xc8: showSystemMenu's
// nearestMenuSpot) or, below y 0x1e0, to the panel handlers of g_panelListMode (ML+0xee40..ML+0xeec5); every other press takes
// the world path (ML+0xeef8, screenToTile's tile of ML+0xee20). With the HUD moved to the window's edges the course shows in
// the panels' 800-space places, but those tests still see the panels there. Fixed (0): a press the host marked as made in a
// vacated place (bridge.h view_click_vacated, view block word +4) takes the world path; with the word 0 (the original's window,
// every replay without it) nothing changes.
int g_defect_vacated_click = 1;
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
// Fixed (0): a held tool keeps zoom 8 (the only zoom above 4, reached only with mainLoop.zoomInStopsAt4 = 0), and 'x' from 8 to 4
// keeps the tool (4 is a tool zoom); at 1 and 2 the original's forcing to 4 stays, and 'x' from 4 still drops the tool. The build
// paths at zoom 8: the clicks and the cursor tile go through screenToTile, the placement rectangle, markers, tee line and
// footprints through worldToScreen / tileToScreen / addSprite, all linear in the zoom. Two drawing rules of zoom 4 are kept
// apart at zoom 8 (both reached only there): the translucent tile box takes (8, 8, 4) (tick_frame.inc P11E_BoxScale) and the
// translucent sprites take their offsets at the display scale (gfx_scene.cpp draw_alpha).
int g_defect_tool_zoom4 = 1;
// mainLoop.cameraStepFixed4 (P11F): the arrow keys (ML+0xe971..ML+0xea73) step the camera 4 tiles at every zoom; fixed (0): 2 tiles
// at zoom 8 (the screen distance 4 tiles cover at zoom 4). The edge scroll (ML+0xebb4..ML+0xec89, 1 tile per frame) is unchanged.
int g_defect_camera_step4 = 1;
int32_t hover_tip_dwell();  // P6B hoverTip.tenFrameDwell (gfx_hud.cpp): the HUD panels' tip dwell, 10 frames in the original
inline int16_t cash_word(int32_t v) {
  if (SG_DEFECT(g_defect_cash_low_word)) return static_cast<int16_t>(v);
  return static_cast<int16_t>(v > 0x7fff ? 0x7fff : v < -0x8000 ? -0x8000 : v);
}
// P6A: keep the camera tile pair on the 50 x 50 grid (mainLoop.cameraFollowOffGrid = 0); no change at 1
void camera_on_map(Ctx &ctx) {
  if (SG_DEFECT(g_defect_camera_follow_offgrid)) return;
  GameState &g = ctx.gs();
  g.gCameraTileX = g.gCameraTileX < 0 ? 0 : g.gCameraTileX > 0x31 ? 0x31 : g.gCameraTileX;
  g.gCameraTileY = g.gCameraTileY < 0 ? 0 : g.gCameraTileY > 0x31 ? 0x31 : g.gCameraTileY;
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

thread_local std::unordered_map<std::string, uint64_t> t_counts;
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
  Entry e;
  {
    std::lock_guard<std::mutex> g(mu);
    auto it = cache.find(name);
    if (it == cache.end()) it = cache.emplace(name, resolve(name)).first;
    e = it->second;
  }
  ++t_counts[name];
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
    FramePreamble(F, retile ? 1 : 0);
    Ph_00410604(F);
    Ph_0041234e(F);
    Ph_0041353d(F);
    Ph_00415847(F);
    Ph_004175f2(F);
    const int r6 = Ph_00418bc7(F);
    {
      GfxCursorScope cursor_scope(ctx);  // P9E fix 4: P7's tool previews are drawn at the cursor tile (records only, gfx.h)
      Ph_00419d06(F, r6);
    }
    int h2in[6] = {0, 0, 0, 0, 0, 0};
    int d = Ph_0041c0c5(F, h2in);
    if (d == kNext) d = Ph_0041ecbb(F, h2in);
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
    if (ter != 0 && !mapped(c, ter, kObjSize) && ensure(c, b.terrain, b.terrain_va, ter, kObjSize)) {
      c.mem.store<int32_t>(ter + 0x14, 50);  // width (terrain_dll.cpp kTerWidth)
      c.mem.store<int32_t>(ter + 0x18, 50);  // height (kTerHeight)
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
    const uint32_t va = operator_new(kObjSize);
    ctx_.mem.store<int32_t>(va + 0x14, 50);
    ctx_.mem.store<int32_t>(va + 0x18, 50);
    return va;
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
  static constexpr uint32_t kObjSize = 0x3A4 + 2500 * 0x248;  // the tile array's end (kTerTiles + 50 * 50 * kTileSize)
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
  std::vector<std::pair<std::string, uint64_t>> v(tk::t_counts.begin(), tk::t_counts.end());
  std::sort(v.begin(), v.end());
  return v;
}
void tick_callee_counts_reset() { tk::t_counts.clear(); }

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
     "fixed: only inside the item's button art box",
     &g_defect_title_hotspot_range},
    {"mainLoop.vacatedPanelClick", "P9E",
     "a press in the bottom band's 800-space place goes to the System menu or the panel handlers even when the host shows the "
     "course there (the HUD moved to the window's edges); fixed: a press the host marked vacated (view block word +4) takes the "
     "world path",
     &g_defect_vacated_click},
    {"mainLoop.cameraFollowOffGrid", "P6A",
     "the camera writers that follow a golfer, walker, ball or photo position (and the hole / object offsets) store no grid bound, "
     "so the view leaves the map; fixed: each clamps the camera tile pair to 0..0x31 like H1's key and edge-pan cases",
     &g_defect_camera_follow_offgrid},
    {"updateGolfers.walkOffGrid", "P6A",
     "a golfer's walking step has no grid test (reversal turn, step overshoot); fixed: the position is clamped to the grid after the step",
     &g_defect_golfer_walk_offgrid},
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
     "keeps zoom 8 and zooming out from 8 to 4 keeps it (zooms 1 and 2 still go to 4, zooming out from 4 still drops it)",
     &g_defect_tool_zoom4},
    {"mainLoop.cameraStepFixed4", "P11F",
     "an arrow key moves the camera 4 tiles at every zoom; fixed: 2 tiles at zoom 8 (the same screen distance as 4 tiles at zoom 4)",
     &g_defect_camera_step4},
};

extern const ModuleDef kModule_tick = {"tick", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                       sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
