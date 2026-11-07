// simgolf_port.exe: the Windows host of the port (roadmap S7.3: the shell; S7.6: the live simulation loop).
//
//   simgolf_port.exe --install <dir> [--data <dir>]                 the game: a LIVE SESSION (host.h) in the window
//                    [--idle-ms <n>] [--pass-ms <n>] [--cursor system|message] [--max-records <n>] [--flag17 <from>:<to>]
//                    [--dpi <n>] [--no-activate] [--exit-after <ms>] [--record-dir <dir>] [--negctl <name>]
//   simgolf_port.exe --install <dir> [--data <dir>] [--present-trace <scenario | file.trace> [--corpus <dir>]
//                    [--snapshot-at <clock ordinal>] [--max-records <n>] [--put <logical>=<file>]...] [--dpi <n>]
//                    [--no-activate] [--no-window] [--exit-after <ms>] [--record-dir <dir>] [--negctl <name>]
//   simgolf_port.exe --install <dir> [--data <dir>] --live-trace <scenario | file.trace> [--corpus <dir>] [--snapshots a,b,..]
//                    [--max-records <n>] [--record-dir <dir>]   (S7.5) the live core over a corpus trace, presented like a session
//   --live-trace and the live session take --audio off|on|model|capture (S7.10; default on since S7.11: audio_out.h; on =
//   DirectSound, which falls back to the original's device-less path when no output device exists) and --audio-no-device
//   (S7.11 test switch: DirectSoundCreate answers DSERR_NODRIVER); --negctl audio-keep-thread | audio-leak-thread (S7.11)
//   every run mode takes --terrain live|headless (default live: port/src/sim/terrain_live.cpp draws the course into the gfx
//   root surface, as port/test/framegate.py attaches it) and --terrain-va <v> (the Terrain object's address, 0 = a free range)
//   and --gl raster|auto|host (roadmap S3.20, decision G13: GlRaster is the default; opt-in: auto = the host's OpenGL when the
//   startup self-check of gl_wgl.h passes, else GlRaster; host (or wgl) = the host's OpenGL without the check). Without --gl the
//   environment's SG_GL_BACKEND (the same values) chooses, else GlRaster. The choice, where it came from and its reason go to
//   stderr and to terrain_renderer.json in the record directory.
//   every run mode takes --profile original|fixed|play (S7.12 / G30: the defect switches; default play, recorded in session.json);
//   simgolf_port.exe --profile-list original|fixed|play   prints every switch and the value that profile gives it
//   simgolf_port.exe --selftest                      pacing, saves overlay and presenter unit tests ("[k/n] name PASS")
//   simgolf_port.exe --selftest-p5b | --selftest-p7  the P5B scale modes / the P7 wide side bands
//   simgolf_port.exe --selftest-p8b                  P8B: the world layer under the frame (control --negctl p8b-mask-ignored)
//   simgolf_port.exe --selftest-p9b                  P9B: the layered HUD compositor (controls --negctl p9b-blend-opaque | p9b-rect-move)
//   simgolf_port.exe --selftest-p9c                  P9C: text at the window's resolution (controls --negctl p9c-unremoved |
//                    p9c-wrong-scale | p9c-wrong-colour)
//   simgolf_port.exe --selftest-p9d                  P9D: the modal screens over the HUD layers (control --negctl p9d-dim-local)
//   P9D: --hud-modal on|off (default on with the HUD layers; SG_APP_P9D=off: off) the modal screens over the HUD: the report
//   screens' dim recorded as a dim of the whole window, and the frames of a modal screen published with the panels it was drawn
//   over (gfx.h gfx_set_hud_modal); off = P9B's capture. Development: SG_P9D_LAYER_DUMP=1 (live-trace world shots write
//   layers_<present>.tsv / .bmp).
//   P9E: --hud-popups on|off (default on with --hud-modal; SG_APP_P9E=off: off) a popup (showPopup, textInputBox, the System menu)
//   opened from a HUD panel keeps its place relative to that panel: its pixels are composed and their clicks remapped at the
//   panel's anchor (gfx.h gfx_set_hud_popups); off = P9D's centred popups.
//   P9E fix 2: --hud-close on|off (default on with --hud-modal; SG_APP_P9E=off: off) the frame that closes a modal screen opened
//   inside a panel layer (the hire dialog) composes like the screen's frames (gfx.h gfx_set_hud_close); off = P9D.
//   P9E fix 3: --hud-vacated on|off (default on with the HUD compositor; SG_APP_P9E=off: off) a press where a moved HUD panel
//   used to be (the course shows there) is marked in the view block (bridge.h kPortViewVacatedVa) at its delivery; with the play
//   profile's mainLoop.vacatedPanelClick = 0 it acts on the course instead of the panel the game's tests see there.
//   P9E fix 4: --hud-cursor on|off (default on with the HUD layers; SG_APP_P9E=off: off) the tool previews the frame draws at the
//   cursor tile (gfx.h gfx_set_hud_cursor) are taken out of the world picture and drawn with the panel the cursor is over.
//   P10A: --fps on|detail|off (default on; SG_APP_P10=off: off) the frame rate at the window's bottom right, in the game's own
//   font (profile.h; F12 cycles on -> detail -> off; detail adds the frame's phases in ms). --profile-csv <file>: one row per
//   second of the profiler's line (fps, game frames per second, sim / gfx / terrain / capture / wait ms per game frame, base /
//   world / layers / text / bands / overlay / blit ms per paint). session.json gets the means (`p10a`).
//   P10E: --paint-rate display|game (default display with the HUD cursor previews; SG_APP_P10=off: game) the game keeps its own
//   pace (10.7 frames per second); between its frames the window, ticked at the display's refresh rate, moves the frame's tool
//   preview to the tile under the current cursor (present.h CursorProj / CursorLive) and repaints only when that place changed.
//   The preview's colour, height, the tee line and its yards label are the frame's until the next frame. game = repaint per frame.
//   P10C fix 1: --title-bands on|off (default on with --wide; SG_APP_P10=off: off) back from a session, the title menu's first
//   frame is posted again with the title's band hint, so its side bands show the edge fill instead of black (host.h title_bands).
//   P10C fix 2: --hud-writes on|off (default on with the HUD layers; SG_APP_P10=off: off) a HUD panel layer keeps every pixel its
//   drawing wrote, also where the word equals the course's under it (gfx.h gfx_set_hud_writes); off = P9B's diff (icon holes).
//   P10C fix 3: --photo-dir on|off (default on; SG_APP_P10=off: off) the live game makes <data>/files/snapshots, the folder the
//   bulletin board's accomplishment photos are written to and read from (without it the photo frames stay empty).
//   P10C fix 4: --hud-textbox on|off (default on with the HUD popups; SG_APP_P10=off: off) a textInputBox opened from no panel
//   (saveGameDialog's SAVE GAME box after the quit prompt) is composed centred in the window, its clicks remapped with it.
//   P10D: --sprite-res native|frame (default native with a world layer; SG_APP_P10=off: frame) the sprite queue (trees, water
//   life, golfers, every course object) drawn a second time at the world layer's resolution from the sprites' own art
//   (terrain_live.h LiveTerrainWorld::hr); frame = the 800 x 600 scene's sprites scaled with the picture (G32).
//   P10H fix 1: --sprite-filter sharp|smooth|sharp-area (default sharp; SG_APP_P10=off: smooth) the filter of those sprites and of
//   the placement box: sharp = every art pixel a crisp block with one-window-pixel area-weighted steps, the object outlines smooth;
//   smooth = P10D's bilinear; sharp-area = sharp with the outlines area-weighted too (gfx_scene.cpp hr_blit_rows).
//   P13D fix 5: --sprite-filter edge (the default; SG_APP_P13=off: sharp) = sharp over the art first scaled up by Scale2x / EPX on its
//   payloads (once from 1.5 world pixels per texel, twice from 3: gfx_scene.cpp hr_art), so edges and outlines lose their staircase;
//   the queue sprites, the cursor previews and the placement box.
//   P13D item 4: --terrain-filter sharp|linear|nearest (default sharp; SG_APP_P13=off: nearest) the terrain textures on the world
//   layer (the course at the window's resolution): the original magnifies its .bmp textures with GL_NEAREST (a block per texel at
//   zoom 8 and large windows); linear = GL_LINEAR (the filter it uses for the path textures: soft), sharp = GL_LINEAR over the
//   texture scaled 2 x 2 with the Catmull-Rom cubic (terrain_live.cpp upscale2_cubic). The 800 frame is unchanged.
//   P10I fix 1: --line-res native|frame (default native with the HUD cursor previews; SG_APP_P10=off: frame) the line a held green
//   / tee tool draws from the tee (or green) to the cursor (P7's drawMinimapSegment) drawn again at the window's resolution in the
//   cursor previews' place, its cursor end following P10E's live move (gfx.h gfx_set_hud_lines, present.h set_line_native); frame
//   = the 800 x 600 frame's line pixels scaled with the picture.
//   P13B item 18: --map-bands clamp|same (default clamp; SG_APP_P13=off: same) the world map shown before a session (courseActionMenu:
//   the only loop that hands the host a 50 ms timed pass) gets side bands that repeat its outermost column, cleaned by a vertical
//   median over +-33 logical rows (present.cpp fill_bands_map), instead of the --wide-fill bands; every other screen keeps them.
//   P11C: --hover-help on|course|off (default on; SG_APP_P11=off: off) one short line of what a terrain does, in the game's one-line tip
//   (drawPanelBackdrop): above the Build palette's tool tip, and over the course for the tile under the cursor with no tool after
//   the HUD tips' dwell (gfx.h gfx_set_hover_help; log/roadmap/P11/P11C_notes.md). Drawing only.
//   P13A item 2 (Mariano: "hovering on grass or elements while nothing is selected should not happen"): `on` draws the palette's,
//   slots' and amenities' lines only, `course` (SG_APP_P13=off) also P11C's course line (gfx.h gfx_set_hover_help_course).
//   P10I fix 2: --hover-rate display|game (default display; SG_APP_P10=off: game) the HUD's hover highlights follow the cursor at the
//   display's rate between game frames: the presenter learns each control's box and looks from the frames (hover.cpp) and swaps
//   a box to the look its nearest observation gives at the live cursor point; the next frame shows the game's own picture.
//   game = a highlight changes only when the game draws its next frame.
//   P13C item 10: --hud-strip right|left (default right; SG_APP_P13=off: left) the golfer strip's faces right of the Build palette
//   (present.h set_strip_right, kGfxLayerStrip), clicks remapped with them.
//   P11B fix 1: --hud-panel left|right (default left; SG_APP_P11=off: right) the edge the panels opened from the radial menu's
//   people buttons (employee view / detail, golfer action, with the shot texts in their place) are anchored to: left = beside the
//   radial menu as in the 800-space frame, clicks remapped with them (present.h set_panel_anchor); right = P9B's right edge.
//   P12A fix 1: --smooth-camera on|off (default on with the HUD layers and the HUD compositor; SG_APP_P12=off: off) a camera move
//   (arrows, edge scroll, every other camera writer) slides the course at the display's rate: the newest game frame is shown at
//   once with its course picture moved from where the shown picture was to its own place over one game frame period; the HUD stays
//   (present.h Presenter::slide_tick, log/roadmap/P12/P12A_notes.md). The game keeps its 10.7 frames/s. SG_P12_LOG=<file>: one
//   line per frame (development).
//   P12A fix 2: --smooth-objects on|off (default on with --sprite-res native and the world layer; SG_APP_P12=off: off) walking
//   golfers, staff, animals, flyers and residents are drawn between two game frames at the blend of their two places (one frame
//   late), the ball in flight and its shadow ahead by their last step (present.h Presenter::objects_tick; the frame code tags its
//   queue entries render-only: gfx.h gfx_mover_source). P12A fix 3: --object-rate <hz> (default 30; 0 = every pacer tick) the most
//   object redraws per second (a camera slide still paints at the pacer's rate). P14A fix 2: --object-rate auto (the default) = every
//   pacer tick while the GPU path paints a client of at most 1920 x 1200 pixels, else 30 (P14A_notes.md section 4).
//   P15C fix 2: every pacer tick at any client size while the GPU draws the frame's moving objects (--gpu-objects on).
//   P15A: --world-thread on|off (default on; SG_APP_P15=off: off) the world image of a present (the window-resolution sprites'
//   flush, the post-queue draws, world_cursor / world_frame) made on a thread of its own, so the sim thread's frame does not grow
//   with the client size (host.h HostOptions::world_thread, log/roadmap/P15/P15A_notes.md); session.json p15.
//   P15A fix 3: --world-raster-thread on|off (default on; SG_APP_P15=off: off; needs the world thread, never with --world-check) the
//   world layer's terrain passes drawn on the world thread too: the sim thread draws only the world pixels its wide scene samples
//   (host.h HostOptions::world_raster_thread); session.json p15 raster_thread.
//   P14A: --gpu on|off (default on; SG_APP_P14=off: off) the window's picture composed and presented by the GPU (gpu.h: Direct3D 11,
//   a flip-model swap chain, vsync): the same pixels as the CPU path (Presenter::render), drawn at the display's refresh without the
//   CPU paint's cost. off (or no D3D11 device / d3dcompiler_47.dll) = the CPU path (SetDIBitsToDevice). stdout says `GPU on|off ...`.
//   SG_P14_VERIFY=<n>: every n-th paint also runs the CPU path and compares (session.json p14; SG_P14_VERIFY_DIR=<dir> keeps the first
//   differing pictures). SG_P14_SHOT=<dir>: the registered message `SimGolfPort.P14Shot` (wParam n) writes the next presented
//   picture to <dir>/shot_<n>.bmp (a flip-model window's picture is not in its window DC).
//   P15C: --gpu-objects on|off (default on; SG_APP_P15=off: off) with the GPU path, the moving objects between game frames are drawn by
//   the GPU (a compute pass running gfx_hr_scene_redraw's per-pixel rules; present.h set_gpu_objects); the CPU world image gets them
//   only where a CPU render reads it. off (or a GPU without the extended double-precision shader ops) = P12A's CPU redraw.
//   P11D: --undo-keys on|off (default on; SG_APP_P11=off: off) Ctrl+Z undoes the newest course edit step (the edits of one left
//   press) through the game's own right-click undo (demolishObject: the previous content back, the price refunded); taken only
//   with the profile's play switch mainLoop.courseUndo = 0 (`play`), written as a recorded harness write (host.h request_undo).
//   P11D fix 2: Ctrl+Y (same option) builds the newest undone step again through the game's terrain-tool click path at today's
//   price; a new edit empties the redo list. P11D fix 3: placed objects share the history: Ctrl+Z removes the newest one as the
//   bulldozer does (no popup) and gives back exactly what its placement charged; Ctrl+Y places it again at today's price.
//   P9C: --text-res native|frame (default native; SG_APP_P9C=off: frame) draws the game's text again at the window's resolution
//   (textres.h); frame = the 800 x 600 frame's text scaled with the picture. --text-aa system|cleartype|grey: the antialiasing of
//   text drawn at a scale other than 1 (default system = lfQuality 0, as the game's own text). live-trace: --text-unit-redraw
//   (a measurement: the redraw also at an 800 x 600 content rectangle).
//   P10B fix 3: --text-peel witness|plain (default witness; SG_APP_P10=off: plain) a captured run counts as visible at a pixel only
//   near a pixel of it that differs from its neighbour and still shows (gfx.h gfx_set_text_peel_witness): a one-colour cover over
//   a run (the white save-name field over a white message line) no longer leaves the run's ghost; plain = P9C's per-pixel test.
//   P10B fix 2: --text-fit on|off (default on; SG_APP_P10=off: off) a run drawn at the window's resolution keeps the 800-space
//   layout scaled: each byte at its 800-space pen position scaled, the ink's vertical middle on the 800-space ink's middle scaled
//   (textres.h TextRaster::set_fit); off = P9C's TextOutA with the window-resolution font's own advances and baseline.
//   P10B fix 1: --text-banner on|off (default on with --text-res native; SG_APP_P10=off: off) the P8A loading banner and the frame
//   put back after the load carry the text runs of the frame under them (host.cpp banner_text), so that frame's text stays at the
//   window's resolution during a world load instead of the 800 x 600 frame's text scaled.
//   simgolf_port.exe --compose <in.555> <cw> <ch> <out.bmp> [--wide --wide-fill soft|clamp|black] [--scale m]
//                    [--compose-band extend|black]   P7: the presenter's client image of one 800x600 frame, as a BMP
//   P7 (widescreen stage 1, default off): --wide [on|off] (or SG_APP_WIDE=on) opens the window at --wide-width (1067) x 600
//   logical pixels; the 4:3 game image stays fitted at full height, and the side bands of the full-screen 2D screens shown
//   before a game's frame loop runs carry the frame's edge colours outward (--wide-fill soft|clamp|black); in game they stay black.
//   P8B (widescreen stage 2 + the world at the window's pixel resolution, default off; log/roadmap/P8/P8B_notes.md):
//   --render-scale 1|native|<pw>x<ph> (default 1): with `native` the course is rasterised at the window's client size at the
//   session's start (a second rasteriser, the WORLD LAYER of the LiveTerrain: terrain_live.h), with <pw>x<ph> at that size; `1`
//   gives a world layer only with --wide (W x 600 at render scale 1). With a world layer the 800 x 600 frame is composed over it
//   (present.h WorldImage): the course shows through wherever the frame shows terrain, and in the side columns of a wide window.
//   --wide-edge on|off (default on with --wide): H1's edge scroll fires at the wide window's edges, not at the 800 frame's.
//   --live-trace only: --world-shots i,j,.. writes world_shot_<i>.bmp (the client image with the world layer) and frame_shot_<i>.bmp
//   (the same present without it) at those present indices, at --shot-size <cw>x<ch> (default the world layer's size);
//   --world-check switches on the render-scale-1 gate of the central 800 columns (session.json `world`).
//   simgolf_port.exe --translate <input.jsonl> [--out <file>]   re-translates a session's input log offline
//   simgolf_port.exe --install <dir> --data <dir> --cat <logical> <out> | --write <logical> <file> | --remove <logical>
//                    | --list <pattern>             one operation through the saves overlay (saves.h)
//
// PRESENT-TRACE runs the port from process start over a corpus trace (sg_run_boot's path: the exe's initialised data, the
// install's files through the saves overlay, the GDI text provider and the gfx backend attached, as port/test/boot.py and
// port/test/gfx.py do) on a worker thread while the window shows black, then presents the backend's back buffer: the
// snapshot taken at --snapshot-at (gfx.snapshotAt) when given, else the back buffer at the end of the run (gfx.dump 0).
//
// LIVE SESSION (roadmap S7.6, the default): the same start (exe image, install files, GDI text, gfx) on a sim thread
// through portsim::run_live with a LiveSession host (host.h): the window's real input as root-handler commands, real-time
// pacing, a present at every frame wait and modal pump call whose back buffer changed, the session recorded. --cursor
// message answers the game's cursor reads from the last mouse message (drivers that post messages without moving the
// system cursor); --idle-ms / --pass-ms set the idle passes of the menu loops (U-1601); --no-timed-passes (P5A) makes the
// world map (courseActionMenu) wait for input again instead of passing every 50 ms as the original does; --flag17 a:b (tests) sets g_flags
// bit 17 at live frame a and clears it at b as recorded harness writes; --max-records ends the session after n records.
// The session ends when the window closes (at the next frame head or pump) or when the game quits (the window closes).
// Negative controls: --negctl pacing-off | present-late | present-point-off (host.h HostDefect; present-point-off also on
// --live-trace), terrain-log-keep (S7.8, host.h TerrainLogBound), recorder:<mode> (live.recorder, live.cpp).
// S7.8: --mem-every <n> samples the process's memory every n live frames into memory.tsv (default 1000; 0 = start and end).
//
// No original binary is loaded: golf_clean.exe is read as a file for its constants (ExeConstants) and the GameState's base
// address (its .data section's address, from the file's PE header); jgld.dll, Terrain.dll, sound.dll and binkw32.dll are
// not touched.
//
// RECORDING LAYOUT (one directory per session, <data>/recordings/<yyyymmdd-hhmmss>-<pid> unless --record-dir):
//   session.json    what ran: mode, install, data directory, trace, records, the run's status and detail, client size, dpi
//   input.jsonl     one line per input message the window received (window.h event_json: message, viewport, the host's
//                   answers in order, the translator's steps, the RootHandler commands)
//   framehash.bin   per frame head: u32 T, u32 record index, u64 sgh64 x n (the sim class objects, gamestate_meta.inc order),
//                   the layout of the captures' log/p2/p24/<scenario>/c_a/framehash.bin; framehash.json names the objects
//   back.555        the presented frame (800 x 600 RGB555 words, rows top-down); gfx_log.txt the backend's gfx.log
//   trace.trace     (S7.6, live sessions) the session's own schema v1 trace; framehash.bin is then the live run's records
//   presents.tsv    (S7.6) one row per present (host.h); shown.tsv one row per frame the window took (index, sgh64);
//   frames.tsv      (S7.6) one row per frame head (live frame, timeGetTime, g_flags), written as the session runs
//   input_dispatch.tsv (S7.4) one row per input message the session received: seq, msg, wParam, lParam (client coordinates),
//                   the live frame it arrived in, its root-handler commands, the dispatch that delivered them (-1: none)
//   dispatches.tsv  (S7.4) one row per dispatch: index, live frame, body pump (1) or modal, the body call's ordinal in the frame
//                   (0 P6, 1 H1), PumpKind, LivePump calls / cursor_reads / may / must, commands, first and last event seq;
//                   body_calls.tsv: body pump calls per live frame
//   --clock-origin <v> (S7.4) the clock answers v + ms since the first answer; --keys message: GetKeyState /
//                   GetAsyncKeyState from the key messages received (a driver that posts messages), log/roadmap/S7/4_notes.md
// The saves overlay lives beside it: <data>/files/<logical name> and <data>/deleted.lst (saves.h).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio_out.h"
#include "host.h"
#include "input_map.h"
#include "pacing.h"
#include "present.h"
#include "profile.h"  // P10A
#include "saves.h"
#include "stdio_log.h"
#include "window.h"

#include "portsim/arith.h"
#include "portsim/execonst.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/state_hash.h"
#include "portsim/sim/gfx.h"  // P8B: gfx_back_terrain
#include "portsim/sim/live.h"
#include "portsim/sim/perf.h"  // P12A fix 1: perf_now_ns (the camera slide's clock)
#include "portsim/sim/terrain_api.h"
#include "portsim/sim/terrain_live.h"
#include "portsim/sim/tick.h"

namespace fs = std::filesystem;
using namespace portapp;

namespace portapp {
extern int g_pacing_defect;  // pacing.cpp (selftest controls)
}

namespace {

// ------------------------------------------------------------------------------------------------ small helpers
bool read_file(const std::string &p, std::vector<uint8_t> &out) {
  std::ifstream f(fs::path(p), std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}
bool write_file(const fs::path &p, const void *data, size_t n) {
  std::error_code ec;
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  if (n) f.write(static_cast<const char *>(data), static_cast<std::streamsize>(n));
  return static_cast<bool>(f);
}
std::string jesc(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\', o += c;
    else if (static_cast<unsigned char>(c) < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

// The address of the exe's .data section (ImageBase + VirtualAddress) from the file's PE header: the GameState's base, so
// the 32-bit pointers the state holds to its own objects resolve as in the original.
bool exe_data_va(const std::vector<uint8_t> &f, uint32_t &va) {
  auto u16 = [&](size_t o) { return o + 2 <= f.size() ? static_cast<uint32_t>(f[o] | f[o + 1] << 8) : 0u; };
  auto u32 = [&](size_t o) {
    return o + 4 <= f.size() ? static_cast<uint32_t>(f[o] | f[o + 1] << 8 | f[o + 2] << 16 | static_cast<uint32_t>(f[o + 3]) << 24)
                             : 0u;
  };
  if (f.size() < 0x40 || f[0] != 'M' || f[1] != 'Z') return false;
  const size_t pe = u32(0x3c);
  if (u32(pe) != 0x00004550u) return false;  // "PE\0\0"
  const uint32_t nsec = u16(pe + 6), optsz = u16(pe + 20);
  const size_t opt = pe + 24;
  if (u16(opt) != 0x10b) return false;  // PE32
  const uint32_t base = u32(opt + 28);
  for (uint32_t i = 0; i < nsec; ++i) {
    const size_t s = opt + optsz + static_cast<size_t>(i) * 40;
    if (s + 40 > f.size()) return false;
    if (std::memcmp(&f[s], ".data\0\0\0", 8) == 0) {
      va = base + u32(s + 12);
      return true;
    }
  }
  return false;
}

// FRAME records of a schema v1 trace (port/schemas/trace.md: 32-byte header + meta, then u8 type, u8 sub, u16 len, payload)
int32_t count_frames(const std::vector<uint8_t> &t) {
  if (t.size() < 32 || std::memcmp(t.data(), "SGTRACE\0", 8) != 0) return -1;
  const uint32_t hdr = t[10] | t[11] << 8;
  const uint32_t meta = t[12] | t[13] << 8 | t[14] << 16 | static_cast<uint32_t>(t[15]) << 24;
  size_t o = hdr + meta;
  int32_t n = 0;
  while (o + 4 <= t.size()) {
    const uint8_t type = t[o];
    const uint32_t len = t[o + 2] | t[o + 3] << 8;
    if (type == 0x02) ++n;
    o += 4 + len;
  }
  return n;
}

// one registry function by its hooks.csv name
bool reg_call(portsim::Ctx &ctx, const char *name, const std::vector<uint32_t> &args, std::string *why, uint32_t *eax = nullptr) {
  const int i = portsim::registry_find(name);
  if (i < 0) {
    if (why) *why = std::string("no port function ") + name;
    return false;
  }
  const portsim::FuncDef *d = portsim::registry_at(i, nullptr);
  portsim::Ret r;
  try {
    d->fn(ctx, args.data(), r);
  } catch (const portsim::MissingResource &e) {
    if (why) *why = std::string(name) + ": missing " + (e.what ? e.what : "?");
    return false;
  } catch (...) {
    if (why) *why = std::string(name) + ": failed";
    return false;
  }
  if (eax) *eax = r.eax;
  return true;
}

// ------------------------------------------------------------------------------------------------ the boot run
struct RunOut {
  int code = 0;
  int32_t done = 0;
  std::string detail;
  std::vector<uint32_t> T;
  std::vector<uint64_t> hashes;
};

int run_cpp(portsim::Ctx &ctx, const std::vector<uint8_t> &trace, int32_t maxf, RunOut &o) {
  try {
    return portsim::tick_run_frames(ctx, trace.data(), trace.size(), nullptr, 0, maxf, o.T.data(), o.hashes.data(), &o.done,
                                    &o.detail, true);
  } catch (const portsim::Fault &f) {
    o.detail = "fault at va " + std::to_string(f.va) + "; " + o.detail;
    return -3;
  } catch (const portsim::DivideError &) {
    o.detail = "divide error; " + o.detail;
    return -9;
  } catch (const portsim::NoReturn &e) {
    o.detail = std::string("no return: ") + (e.where ? e.where : "?") + "; " + o.detail;
    return -10;
  } catch (const portsim::MissingResource &e) {
    o.detail = std::string("missing: ") + (e.what ? e.what : "?") + "; " + o.detail;
    return -11;
  } catch (...) {
    o.detail = "exception; " + o.detail;
    return -8;
  }
}
int run_guarded(portsim::Ctx *ctx, const std::vector<uint8_t> *trace, int32_t maxf, RunOut *o) {
  __try {
    return run_cpp(*ctx, *trace, maxf, *o);
  } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO || GetExceptionCode() == EXCEPTION_INT_OVERFLOW
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return -9;
  }
}

struct Options {
  std::string install, exe, data, record_dir, trace, corpus, translate_in, translate_out, negctl;
  std::vector<std::pair<std::string, std::string>> puts;
  int64_t snapshot_at = -1;
  int32_t max_records = -1;
  bool in_memory = false;                // S7.8: --in-memory (S7.5's way: records held until the end)
  int32_t mem_every = 1000;             // S7.8: --mem-every (memory.tsv sampling period in live frames)
  uint32_t dpi = 0, exit_after = 0;
  bool activate = true, window = true, selftest = false, selftest_p5b = false, selftest_p7 = false, selftest_p8b = false, selftest_p9a = false;
  bool selftest_p9b = false;  // P9B
  bool selftest_p9c = false;  // P9C
  bool selftest_p9d = false;  // P9D
  std::string text_res = "native";  // P9C: --text-res native|frame (SG_APP_P9C=off: frame)
  std::string text_aa = "system";   // P9C: --text-aa system|cleartype|grey
  bool text_unit_redraw = false;    // P9C: --text-unit-redraw (live-trace measurement)
  bool text_banner = true;          // P10B fix 1: --text-banner on|off (SG_APP_P10=off: off)
  bool text_fit = true;             // P10B fix 2: --text-fit on|off (SG_APP_P10=off: off)
  std::string text_peel = "witness";  // P10B fix 3: --text-peel witness|plain (SG_APP_P10=off: plain)
  // S7.6 live sessions
  uint32_t idle_ms = 200, pass_ms = 10;
  bool timed_passes = true;  // P5A (--no-timed-passes)
  std::string cursor = "system";
  int32_t flag17_from = -1, flag17_to = -1;
  int64_t clock_origin = -1;  // S7.4
  std::string keys = "system";
  // P5B (log/roadmap/P5/P5B_notes.md): the window and input behaviours from Mariano's play-test, on by default;
  // SG_APP_P5B=off (port/test/conftest.py) gives S7's behaviour for all five, the switches below for each one
  bool edge_guard = true;          // --edge-guard on|off (item 1)
  std::string zoom_keys = "all";   // --zoom-keys all|numpad|off (item 2)
  bool close_prompt = true;        // --close-prompt on|off (item 3)
  bool resizable = true;           // --resizable on|off (item 4)
  std::string scale = "auto";      // --scale auto|nearest|smooth (item 4)
  // P7 (widescreen stage 1, log/roadmap/P7/PLAN.md; default off until Mariano has seen it; SG_APP_WIDE=on|off): the window opens
  // at wide_width x 600 logical pixels and the full-screen 2D screens' side bands show their edge colours (present.h WideFill)
  bool wide = true;                // --wide (or --wide on|off); G32: on by default (SG_APP_P8B=off: off, the tests' default)
  int32_t wide_width = 1067;       // --wide-width <800..2400> (1067 = 16:9 at 600 rows)
  std::string wide_fill = "edge";  // --wide-fill edge|mirror|soft|clamp|black (P9A item 5: edge; SG_APP_P9=off: soft)
  // P8B (widescreen stage 2, log/roadmap/P8/P8B_notes.md; default off: render scale 1 and no --wide = no world layer)
  std::string render_scale = "auto";  // --render-scale auto|1|native|<pw>x<ph> (G32: auto = native with --wide, none without)
  bool wide_edge = true;           // --wide-edge on|off (with --wide and a world layer)
  bool hud_anchor = true;          // G32: --hud-anchor on|off (with --wide: the HUD panels at the wide view's edges)
  bool hud_modal = true;           // P9D: --hud-modal on|off (SG_APP_P9D=off: off)
  bool hud_popups = true;          // P9E: --hud-popups on|off (SG_APP_P9E=off: off)
  bool hud_close = true;           // P9E fix 2: --hud-close on|off (SG_APP_P9E=off: off)
  bool hud_vacated = true;         // P9E fix 3: --hud-vacated on|off (SG_APP_P9E=off: off)
  bool hud_cursor = true;          // P9E fix 4: --hud-cursor on|off (SG_APP_P9E=off: off)
  std::string fps = "on";          // P10A: --fps on|detail|off (SG_APP_P10=off: off)
  std::string profile_csv;         // P10A: --profile-csv <file>
  std::string paint_rate = "display";  // P10E: --paint-rate display|game (SG_APP_P10=off: game)
  bool title_bands = true;         // P10C fix 1: --title-bands on|off (SG_APP_P10=off: off)
  bool hud_writes = true;          // P10C fix 2: --hud-writes on|off (SG_APP_P10=off: off)
  bool photo_dir = true;           // P10C fix 3: --photo-dir on|off (SG_APP_P10=off: off)
  bool hud_textbox = true;         // P10C fix 4: --hud-textbox on|off (SG_APP_P10=off: off)
  bool sprite_native = true;       // P10D: --sprite-res native|frame (SG_APP_P10=off: frame)
  int sprite_filter = 3;           // P10H: --sprite-filter sharp (1) | smooth (0) | sharp-area (2); P13D fix 5: edge (3), the default
                                   // (SG_APP_P13=off: sharp; SG_APP_P10=off: smooth)
  bool line_native = true;         // P10I fix 1: --line-res native|frame (SG_APP_P10=off: frame)
  std::string hover_rate = "display";  // P10I fix 2: --hover-rate display|game (SG_APP_P10=off: game)
  bool strip_right = true;         // P13C item 10: --hud-strip right|left (SG_APP_P13=off: left)
  bool panel_left = true;          // P11B fix 1: --hud-panel left|right (SG_APP_P11=off: right)
  bool undo_keys = true;           // P11D: --undo-keys on|off (SG_APP_P11=off: off)
  bool hover_help = true;          // P11C: --hover-help on|course|off (SG_APP_P11=off: off)
  int terrain_filter = 2;          // P13D item 4: --terrain-filter sharp (2) | linear (1) | nearest (0) (SG_APP_P13=off: nearest)
  bool smooth_camera = true;       // P12A fix 1: --smooth-camera on|off (SG_APP_P12=off: off)
  bool hover_help_course = false;  // P13A: --hover-help course, the course line with no tool too (SG_APP_P13=off: on)
  bool smooth_objects = true;      // P12A fix 2: --smooth-objects on|off (SG_APP_P12=off: off)
  int object_rate = -1;            // P12A fix 3: --object-rate <hz> (0 = every pacer tick); P14A fix 2: -1 = auto (the default)
  bool gpu = true;                 // P14A: --gpu on|off (SG_APP_P14=off: off)
  bool gpu_objects = true;         // P15C: --gpu-objects on|off (SG_APP_P15=off: off)
  bool world_thread = true;        // P15A: --world-thread on|off (SG_APP_P15=off: off)
  bool world_raster_thread = true; // P15A fix 3: --world-raster-thread on|off (SG_APP_P15=off: off)
  bool map_bands = true;           // P13B item 18: --map-bands clamp|same (SG_APP_P13=off: same)
  bool hud_layers = true;          // P9B: --hud-layers on|off (with --hud-anchor: panels moved as pixel layers; off = G32's rectangles)
  int32_t world_pw = 0, world_ph = 0;  // the world layer's size (0 = none), set from the options and the window
  std::vector<int64_t> world_shots;    // --world-shots (live-trace)
  int32_t shot_w = 0, shot_h = 0;      // --shot-size
  bool world_check = false;            // --world-check (live-trace)
  int32_t view_width = 0;              // G32: --view-width (live-trace: the view block's width at the first frame head)
  bool ui_union = false;               // G32 development: --ui-union (live-trace)
  // P7: --compose <in.555> <cw> <ch> <out.bmp> [--compose-band extend|black]: the presenter's client image of one frame
  std::vector<std::string> compose;
  std::string compose_band = "extend";
  // P6B (log/roadmap/P6/P6B_notes.md): on by default; SG_APP_P6B=off (port/test/conftest.py) turns it off
  bool loading_overlay = true;     // --loading-overlay on|off (item 2: a `Loading` box while a world load runs)
  // S7.5: the terrain (live = portsim::LiveTerrain: Terrain.dll's state kept by the port and its draw passes rasterised into the
  // gfx root surface, as port/test/framegate.py attaches it; headless = tick.cpp's stand-in, no terrain pixels) and the address
  // of its Terrain object (0: a free range of the address space)
  std::string terrain = "live";
  std::string audio = "on";  // S7.10: off | on | model | capture (AudioGuard); S7.11: default on
  std::string profile = "play";  // S7.12 / G30: original | fixed | play (apply_profile); --profile-list prints it and exits
  bool profile_list = false;
  bool audio_no_device = false;  // S7.11: --audio-no-device (the sink's DirectSoundCreate fails)
  void *audio_hwnd = nullptr;  // S7.10: the window for --audio on (set by run_live_window)
  uint32_t terrain_va = 0;
  std::string gl;                        // S3.20: --gl raster|auto|host|wgl, the terrain renderer of a live terrain ("" = not given)
  std::string live_trace;                // --live-trace: the live core over a corpus trace (LiveTracePresenter)
  std::vector<int64_t> snapshot_list;    // --snapshots a,b,c: gfx.snapshotAt ordinals of a --live-trace run
  std::string op;  // cat / write / remove / list
  std::vector<std::string> op_args;
};

// P9B: the layered HUD compositor is on with --wide, --hud-anchor on and --hud-layers on (the defaults); --negctl p9b-rect-move
// keeps G32's rectangle compositor with capture on (the control)
static bool hud_layers_on(const Options &o) { return o.wide && o.hud_anchor && o.hud_layers; }
// P9C: the presenter's text options
static bool text_native_on(const Options &o) { return o.text_res == "native"; }
// P10I fix 1: the tee lines at the window's resolution need the cursor passes kept (P9E fix 4)
static bool line_res_on(const Options &o) { return hud_layers_on(o) && o.hud_cursor && o.line_native; }
static void setup_text(Presenter &p, const Options &o) {
  p.set_text_res(text_native_on(o));
  p.set_text_aa(o.text_aa == "cleartype" ? TextAa::ClearType : o.text_aa == "grey" ? TextAa::Grey : TextAa::System);
  p.set_text_defect(o.negctl == "p9c-unremoved"      ? TextDefect::Unremoved
                    : o.negctl == "p9c-wrong-scale"  ? TextDefect::WrongScale
                    : o.negctl == "p9c-wrong-colour" ? TextDefect::WrongColour
                                                     : TextDefect::None);
  p.set_text_unit_redraw(o.text_unit_redraw);
  p.set_text_fit(o.text_fit);  // P10B fix 2
  p.set_text_boxes(o.hud_textbox);  // P10C fix 4 (the presenter's popup pieces)
  p.set_panel_anchor(o.panel_left ? -1 : 1);  // P11B fix 1
  p.set_strip_right(o.strip_right);           // P13C item 10
  p.set_line_native(line_res_on(o));  // P10I fix 1
}

// P5B item 4: --scale
ScaleMode scale_mode(const std::string &s) {
  return s == "smooth" ? ScaleMode::Smooth : s == "auto" ? ScaleMode::Auto : ScaleMode::Nearest;
}

// P8B: Options::world_pw / world_ph (the world layer's size; 0 = none) from --render-scale, --wide and the window's client cw x ch
// (0 = no window: `native` then gives none). native = the client at the session's start (the content rectangle's height, the
// client's width with --wide, else the content rectangle's); 1 = W x 600 with --wide (W = --wide-width), else none; <pw>x<ph> as given
void world_size(Options &o, int32_t cw, int32_t ch) {
  o.world_pw = o.world_ph = 0;
  if (o.terrain != "live") return;
  int pw = 0, ph = 0;
  const bool native = o.render_scale == "native" || (o.render_scale == "auto" && o.wide && cw > 0);
  if (o.render_scale == "auto" && !native) {
    if (!o.wide) return;
    pw = o.wide_width;   // no window (live-trace): W x 600
    ph = kLogicalH;
  } else if (native) {
    if (cw <= 0 || ch <= 0) return;
    const Viewport vp = fit_viewport(cw, ch);
    ph = vp.client_h;
    pw = o.wide ? cw : vp.client_w;
  } else if (o.render_scale == "1") {
    if (!o.wide) return;
    pw = o.wide_width;
    ph = kLogicalH;
  } else if (std::sscanf(o.render_scale.c_str(), "%dx%d", &pw, &ph) != 2) {
    return;
  }
  o.world_pw = pw;
  o.world_ph = ph;
}

bool write_bmp(const fs::path &path, const std::vector<uint32_t> &px, int cw, int ch);  // P8B (below)

// P9D development (SG_P9D_LAYER_DUMP=1, live-trace --world-shots): layers_<present>.tsv (one row per layer: index, id, anchor,
// box, pixels per kind) and layers_<present>.bmp (800 x 600 per layer side by side in rows of 4: the base first, then each layer
// with its pixels' words, magenta where the layer did not draw, kind 2..4 tinted by their result word)
void dump_layers(const fs::path &rec, uint32_t index, const portsim::GfxHudFrame &f, int none, bool same_back, uint64_t serial) {
  std::FILE *t = std::fopen((rec / ("layers_" + std::to_string(index) + ".tsv")).string().c_str(), "wb");
  if (!t) return;
  std::fprintf(t, "k\tid\tanchor\tbox\tk1\tk2\tk3\tk4\tdim\tdup\tcarried\n");
  if (f.empty()) {
    std::fprintf(t, "none\treason %d\tsame_back %d\tserial %llu\n", none, same_back ? 1 : 0, static_cast<unsigned long long>(serial));
    std::fclose(t);
    return;
  }
  const int n = static_cast<int>(f.layers.size()) + 1, cols = 4, rows = (n + cols - 1) / cols;
  const int W = kLogicalW * cols, H = kLogicalH * rows;
  std::vector<uint32_t> img(static_cast<size_t>(W) * H, 0xff404040u);
  auto put = [&](int slot, int x, int y, uint32_t c) {
    img[static_cast<size_t>((slot / cols) * kLogicalH + y) * W + static_cast<size_t>((slot % cols) * kLogicalW + x)] = c;
  };
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) put(0, x, y, rgb555_to_bgra(f.base[static_cast<size_t>(y) * kLogicalW + x]));
  for (size_t k = 0; k < f.layers.size(); ++k) {
    const portsim::GfxHudLayer &l = f.layers[k];
    size_t c[5] = {0, 0, 0, 0, 0};
    for (int y = 0; y < kLogicalH; ++y)
      for (int x = 0; x < kLogicalW; ++x) {
        const portsim::GfxLayerPx *p = l.at(x, y);
        put(static_cast<int>(k) + 1, x, y, p ? rgb555_to_bgra(p->px) : 0xffff00ffu);
        if (p && p->kind < 5) ++c[p->kind];
      }
    // dup: opaque pixels of a centred layer that hold a panel pixel of an earlier layer at the same place, dimmed by the quarter
    // rule with source 0 (the report screens' black dim): a dimmed copy of the panel left at its 800-space place
    size_t dup = 0;
    if (l.anchor == 0)
      for (int y = l.y0; y < l.y1; ++y)
        for (int x = l.x0; x < l.x1; ++x) {
          const portsim::GfxLayerPx *p = l.at(x, y);
          if (!p || p->kind != 1) continue;
          for (size_t e = 0; e < k; ++e) {
            if (f.layers[e].anchor == 0) continue;
            const portsim::GfxLayerPx *u = f.layers[e].at(x, y);
            if (u && u->kind == 1 && u->px != 0 && p->px == portsim::gfx_layer_quarter(0, u->px)) {
              ++dup;
              break;
            }
          }
        }
    std::fprintf(t, "%zu\t%d\t%d\t%d,%d-%d,%d\t%zu\t%zu\t%zu\t%zu\t%d:%04x\t%zu\t%d\n", k, l.id, l.anchor, l.x0, l.y0, l.x1, l.y1, c[1],
                 c[2], c[3], c[4], l.dim, l.dim_src, dup, f.carried);
  }
  std::fclose(t);
  write_bmp(rec / ("layers_" + std::to_string(index) + ".bmp"), img, W, H);
}

// P7: --wide / --wide-fill
WideFill wide_fill(const Options &o) {
  if (!o.wide) return WideFill::Off;
  if (o.wide_fill == "edge") return WideFill::Edge;      // P9A item 5 (the default)
  if (o.wide_fill == "mirror") return WideFill::Mirror;  // P9A item 5 (the alternative judged on the screenshots)
  return o.wide_fill == "clamp" ? WideFill::Clamp : o.wide_fill == "black" ? WideFill::Off : WideFill::Soft;
}

struct PresentResult {
  bool ok = false;
  std::string error;
  std::vector<uint16_t> frame;
  RunOut run;
  uint32_t data_va = 0;
  std::string gfx_log;
};

// S7.5: `--negctl reg:<registry name>=<mode>` sets a port function's negative-control switch for the run (e.g.
// reg:gfx.family.terrain=3: the terrain body family skips its draw passes, gfx_terrain.cpp); restored by the guard
struct RegNegctl {
  int *sw = nullptr;
  int prev = 0;
  explicit RegNegctl(const std::string &spec) {
    if (spec.rfind("reg:", 0) != 0) return;
    const size_t eq = spec.find('=');
    if (eq == std::string::npos) return;
    const int i = portsim::registry_find(spec.substr(4, eq - 4).c_str());
    if (i < 0) return;
    sw = portsim::registry_at(i, nullptr)->negctl;
    if (!sw) return;
    prev = *sw;
    *sw = std::atoi(spec.c_str() + eq + 1);
  }
  ~RegNegctl() {
    if (sw) *sw = prev;
  }
  RegNegctl(const RegNegctl &) = delete;
  RegNegctl &operator=(const RegNegctl &) = delete;
};

// S7.12: the defect profile of a run (port/test/profiles.py; the switches are process-wide, portsim/registry.h DefectDef).
// `fixed` (the default) = every switch 1, then the members below; `original` = every switch 1 (the oracle). The members are
// re/port/defect_profiles.tsv's rows, compiled in because the LAN build ships no repo file; port/test/test_defects_golfsfx.py
// compares them with the TSV through --profile-list. Each run's session.json records the profile and every switch's value
// (profile_json), and port/test/app_replay.py replays a recording under the switches it recorded.
struct ProfileMember {
  const char *sw;
  int value;
};
const ProfileMember kFixedProfile[] = {
    {"placeHabitat.clusterLoopBound", 0}, {"spawnObject.cellSearchBound", 0}, {"loadGolfPro.divideAndRejectGuard", 0},
    {"placeAnimal.zeroFrameCount", 0},    {"replaceInText.tailLimit", 0},     {"generateRandomCourse.searchBound", 0},
    {"loadGolfPro.negativeWindowBound", 0}, {"generateRandomCourse.placementBound", 0}, {"flic.treeAliasNames", 0},
    {"sound.golfSfxAlias", 0},
};
// G30 (2026-11-03): `play` = every `fixed` member plus these, re/port/play_profile.tsv's rows (gameplay fixes: the port diverges from the
// original on purpose; `fixed` stays hangs and faults only). The app's default.
const ProfileMember kPlayProfile[] = {
    {"sinScaled.readPastTable", 0},       {"shotPower.cacheIgnoresPutt", 0}, {"chooseShotTarget.strictFlagSticky", 0},
    {"prepareShot.hazardCellTransposed", 0}, {"narrateShot.maskNotRestored", 0},
    {"loadGolferRoster.namedStoryStride", 0},
    {"loadGolferRoster.skillsNextRecord", 0},
    {"mainLoop.cashLowWordOnly", 0},
    {"showFinancialReport.sumWriteBack", 0},
    {"emitGolferThought.tileMoodWrap", 0},
    {"resetGolferState.singleHoleRecord", 0},
    {"spawnWalker.fullTable", 0},
    {"ballScreenList.overflow", 0},
    {"cornerLevel.unclampedAndShared", 0},
    {"loadStoryStrings.cacheEntryLimit", 0},
    {"buildGolferComment.slotLimit", 0},
    {"postEvent.idBound", 0},
    {"mainLoop.titleExitHotspotWide", 0},           // P5A (Mariano's play-test, 2026-11-03)
    {"showThemePackSelect.column5StackWord", 0},    // P5A
    {"updateWalkers.cursorTargetOffGrid", 0},      // P6A (Mariano's play-test 2, 2026-11-03)
    {"updateGolfers.walkOffGrid", 0},               // P6A
    {"mainLoop.cameraFollowOffGrid", 0},            // P6A
    {"hoverTip.tenFrameDwell", 0},                  // P6B (the HUD panels' hover tips after 3 frames, not 10)
    {"infoCursorTip.fixedBox", 0},                  // P11B fix 2 (the palette tool tip box fitted to its text, above the palette)
    {"worldToScreen.wideViewVisibility", 0},        // P8B (G32: the on-screen test over the wide view)
    {"tileToScreen.wideViewVisibility", 0},         // P8B (G32: the scenery of the wide view's side columns)
    {"mainLoop.titleDefaultItem1", 0},              // P9A (Mariano's play-test 3: no title item lit before the cursor nears one)
    {"mainLoop.titleHotspotRange", 0},              // P9A (a title item is chosen only over its button art)
    {"mainLoop.vacatedPanelClick", 0},              // P9E fix 3 (a press where a moved panel used to be acts on the course)
    {"mainLoop.courseUndo", 0},                     // P11D (Mariano's play-test 5 item 4: Ctrl+Z undoes course edits)
    {"mainLoop.treesHiddenFarZoom", 0},             // P11A fix 1 (trees at the furthest zoom)
    {"mainLoop.zoomInStopsAt4", 0},                 // P11A fix 2 (one zoom level closer: 8)
    {"mainLoop.zoom8Font", 0},                      // P13C (zoom 8 labels keep zoom 4's font)
    {"mainLoop.cameraStepFixed4", 0},               // P11F (arrow keys step 2 tiles at zoom 8)
    {"mainLoop.toolForcesZoom4", 0},                // P11E / P13A (the build tools keep every zoom)
    {"updateTerrainRender.syncTilesAfterEdit", 0},  // P13D item 5 (the terrain synced once at load: the first placement changes only its tiles)
    {"mainLoop.rightClickKeepsTool", 0},            // P13A (a right click with nothing to undo drops the tool)
    {"mainLoop.hKeyShotStyleFirst", 0},             // P13A ('h' opens a ready hole during the player's shot too)
    {"Tile::render.fairwayStripesFixed", 0},        // P13A (Tab turns a fairway's mowing stripes)
    {"mainLoop.undoButtonHelpOnly", 0},             // P13A (the bulldozer button arms the game's bulldoze mode)
    {"courseActionMenu.escIgnored", 0},             // P9A item 2: Esc = the world map's Back
    {"showBuyLandScreen.escIgnored", 0},            // P9A item 2: Esc = Done
    {"showCharacterEditor.escIgnored", 0},          // P9A item 2: Esc = Exit
    {"showCharacterEditor.cancelStaysOpen", 0},     // P13B item 14: Cancel puts back every edit and closes the editor
    {"drawPanelBackdrop.barOnTerrainOnly", 0},      // P13B item 13: the tip bar filled under the text on 2D screens
    {"pickCharEditor.escIgnored", 0},               // P9A item 2: Esc = the look picker's cancel
    {"showPopup.escIgnoredNoKeys", 0},              // P9A item 2: Esc = the close button of a popup without keys
    {"skillPointDialog.escIgnored", 0},             // P9A item 2: Esc = OK
};

// every switch back to 1, then the profile's members; "" or why it was refused (nothing changed then). The negative control
// `--negctl profile-ignored` records the profile but leaves every switch at 1 (the app as before S7.12).
std::string apply_profile(const std::string &p, bool ignored) {
  if (p != "original" && p != "fixed" && p != "play") return "unknown profile " + p + " (original | fixed | play)";
  const bool fixed = p == "fixed" || p == "play", play = p == "play";
  if (fixed)
    for (const ProfileMember &m : kFixedProfile)
      if (portsim::defect_find(m.sw) < 0) return std::string("profile fixed names a switch the library lacks: ") + m.sw;
  if (play)
    for (const ProfileMember &m : kPlayProfile)
      if (portsim::defect_find(m.sw) < 0) return std::string("profile play names a switch the library lacks: ") + m.sw;
  for (int i = 0; i < portsim::defect_count(); ++i) *portsim::defect_at(i, nullptr)->flag = 1;
  if (fixed && !ignored)
    for (const ProfileMember &m : kFixedProfile) *portsim::defect_at(portsim::defect_find(m.sw), nullptr)->flag = m.value;
  if (play && !ignored)
    for (const ProfileMember &m : kPlayProfile) *portsim::defect_at(portsim::defect_find(m.sw), nullptr)->flag = m.value;
  return "";
}

// session.json's fields: the profile's name and every switch with the value the run used
std::string profile_json(const std::string &p) {
  std::string s = ",\"profile\":\"" + p + "\",\"defects\":{";
  for (int i = 0; i < portsim::defect_count(); ++i) {
    const portsim::DefectDef *d = portsim::defect_at(i, nullptr);
    s += std::string(i ? "," : "") + "\"" + d->name + "\":" + std::to_string(*d->flag);
  }
  return s + "}";
}

// --profile-list <original | fixed | play>: every switch and the value the profile gives it, "name<TAB>value" lines
int run_profile_list(const std::string &p) {
  const std::string err = apply_profile(p, false);
  if (!err.empty()) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 2;
  }
  for (int i = 0; i < portsim::defect_count(); ++i) {
    const portsim::DefectDef *d = portsim::defect_at(i, nullptr);
    std::printf("%s\t%d\n", d->name, *d->flag);
  }
  return 0;
}

// S7.5: the terrain of a run (Options::terrain), attached after gfx.attach as port/test/framegate.py does (gfx.attach,
// gfx.snapshotAt, sg_ts_live_attach); detached when the guard goes, before the context it was declared after.
struct TerrainGuard {
  portsim::Ctx *ctx = nullptr;
  std::unique_ptr<portsim::LiveTerrain> live;
  TerrainGuard() = default;
  TerrainGuard(const TerrainGuard &) = delete;
  TerrainGuard &operator=(const TerrainGuard &) = delete;
  void attach(const Options &o, portsim::Ctx &c, const fs::path &rec) {
    if (o.terrain != "live") return;
    ctx = &c;
    live = std::make_unique<portsim::LiveTerrain>(c, o.terrain_va, std::vector<uint32_t>{});
    // S3.20 (decision G13, final): the terrain renderer, chosen before the first Terrain call: --gl, else SG_GL_BACKEND, else
    // GlRaster; logged with where the choice came from and its reason
    std::string mode = o.gl, source = "--gl";
    if (mode.empty()) {
      const char *be = std::getenv("SG_GL_BACKEND");
      const std::string e = be ? be : "";
      if (e == "raster" || e == "auto" || e == "host" || e == "wgl") {
        mode = e;
        source = "SG_GL_BACKEND";
      } else {
        mode = "raster";
        source = "default";
      }
    }
    const portsim::LiveTerrain::RendererChoice &ch = live->choose_renderer(mode);
    const std::string took = ch.checked ? " [self-check " + std::to_string(static_cast<int>(ch.check.ms)) + " ms]" : "";
    std::fprintf(stderr, "simgolf_port: terrain renderer %s (%s %s): %s%s\n", ch.backend.c_str(), source.c_str(), mode.c_str(),
                 ch.reason.c_str(), took.c_str());
    auto esc = [](const std::string &t) {
      std::string r;
      for (char k : t) {
        if (k == '"' || k == '\\') r += '\\';
        r += k;
      }
      return r;
    };
    const std::string js = "{\"source\":\"" + esc(source) + "\",\"mode\":\"" + esc(ch.mode) + "\",\"backend\":\"" + esc(ch.backend) + "\",\"reason\":\"" + esc(ch.reason) +
                           "\",\"checked\":" + (ch.checked ? "true" : "false") + ",\"pass\":" + (ch.check.pass ? "true" : "false") +
                           ",\"got\":\"" + ch.check.got + "\",\"want\":\"" + ch.check.want + "\",\"ms\":" +
                           std::to_string(ch.check.ms) + "}\n";
    write_file(rec / "terrain_renderer.json", js.data(), js.size());
    if (o.world_pw > 0 && o.world_ph > 0) {  // P8B: the world layer, before the first Terrain call
      std::string wwhy;
      const bool won = live->set_world_view(o.world_pw, o.world_ph, &wwhy);
      std::fprintf(stderr, "simgolf_port: world layer %dx%d %s\n", o.world_pw, o.world_ph, won ? "on" : ("refused: " + wwhy).c_str());
      const std::string wj = "{\"pw\":" + std::to_string(o.world_pw) + ",\"ph\":" + std::to_string(o.world_ph) + ",\"on\":" +
                             (won ? "true" : "false") + ",\"why\":\"" + esc(wwhy) + "\"}\n";
      write_file(rec / "world_layer.json", wj.data(), wj.size());
      if (won && o.world_check) live->world()->check_central = true;
      if (won && o.sprite_native) live->world()->hr_on = true;  // P10D: the sprites at the world's resolution
      if (won) live->world()->hr_filter = o.sprite_filter;        // P10H: their filter
      if (won) live->world()->tex_mag = o.terrain_filter;         // P13D item 4: the terrain textures' magnification
    }
    portsim::set_terrain_api(c, live.get());
  }
  ~TerrainGuard() {
    if (live) portsim::set_terrain_api(*ctx, nullptr);
  }
};

// S7.10: audio out (audio_out.h) of a live run (Options::audio): off, on (DirectSound, the window's handle; the default since
// S7.11, device-less without an output device), model
// (S4's device model only) or capture (a software device: <rec>/audio.wav). The sim context's sound requests are copied to a
// shadow context on the audio thread; the sim's own calls and GameState are left as they are. finish() (or the guard going)
// detaches the observer and stops the thread before the context goes; <rec>/audio.json holds the counters.
// S7.11 SHUTDOWN ORDER: the original's gameMain runs shutdownSoundSystem after mainLoop returns, before its window goes. In
// the window session the window can close first (the user, or WM_CLOSE after the game's quit), so the window is subclassed
// (audio_close_proc) and its WM_CLOSE stops the shadow (AudioOut::close_early: shutdownSoundSystem, the real buffers and
// device released, the thread joined) before DestroyWindow. g_close_mu serialises it with AudioGuard::finish.
std::mutex g_close_mu;
app::AudioOut *g_close_audio = nullptr;
WNDPROC g_close_prev = nullptr;
LRESULT CALLBACK audio_close_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_CLOSE) {
    std::lock_guard<std::mutex> lk(g_close_mu);
    if (g_close_audio) g_close_audio->close_early();
  }
  return CallWindowProcW(g_close_prev, h, msg, wp, lp);
}

struct AudioGuard {
  portsim::Ctx *ctx = nullptr;
  std::unique_ptr<app::AudioOut> out;
  std::string mode, error;
  fs::path rec;
  app::ProcCounts before;
  bool count_only = false;
  void *win = nullptr;  // S7.11: the session's window (window_at_stop)
  AudioGuard() = default;
  AudioGuard(const AudioGuard &) = delete;
  AudioGuard &operator=(const AudioGuard &) = delete;
  void attach(const Options &o, portsim::Ctx &c, const portsim::ExeConstants &ec, uint32_t data_va, const std::string &install,
              void *hwnd, const fs::path &r) {
    if (o.audio == "off") {
      // S7.11 reference for the shutdown count: --audio off --negctl audio-count-only counts the same span with no shadow
      count_only = o.negctl == "audio-count-only";
      rec = r;
      before = app::proc_counts();
      return;
    }
    app::AudioOutOptions ao;
    ao.mode = o.audio == "model" ? app::SinkMode::Model : o.audio == "capture" ? app::SinkMode::Capture : app::SinkMode::DSound;
    ao.install = install;
    ao.exe = &ec;
    ao.data_va = data_va;
    ao.hwnd = hwnd;
    ao.tick_clock_ms = 30;  // the timer's period: the model / capture device plays it between ticks
    ao.force_no_device = o.audio_no_device;                                               // S7.11
    ao.control = o.negctl == "audio-keep-thread" ? 3 : o.negctl == "audio-leak-thread" ? 4 : 0;  // S7.11 controls
    mode = o.audio;
    rec = r;
    before = app::proc_counts();  // S7.11: the process before the audio thread exists
    out = std::make_unique<app::AudioOut>();
    if (!out->start(ao, &error)) {
      out.reset();
      return;
    }
    ctx = &c;
    out->attach(c);
    win = hwnd;
    if (hwnd) {  // S7.11: the window session's WM_CLOSE stops it first (audio_close_proc)
      std::lock_guard<std::mutex> lk(g_close_mu);
      g_close_audio = out.get();
    }
  }
  void finish() {
    if (count_only) {
      count_only = false;
      const app::ProcCounts after = app::proc_counts();
      char b[160];
      const int n = std::snprintf(b, sizeof b,
                                  "{\"mode\":\"off\",\"handles_before\":%u,\"threads_before\":%u,\"handles_after\":%u,"
                                  "\"threads_after\":%u,\"threads_added\":%u}\n", before.handles, before.threads,
                                  after.handles, after.threads, app::threads_added(before, after));
      if (n > 0) write_file(rec / "audio.json", b, static_cast<size_t>(std::min(n, static_cast<int>(sizeof b) - 1)));
    }
    if (!out) {
      if (!error.empty()) write_file(rec / "audio.json", ("{\"error\":\"" + error + "\"}\n").data(), error.size() + 13);
      return;
    }
    {
      std::lock_guard<std::mutex> lk(g_close_mu);  // S7.11: no close_early from now on (and none running)
      if (g_close_audio == out.get()) g_close_audio = nullptr;
    }
    out->detach(*ctx);
    if (mode == "capture") out->write_capture((rec / "audio.wav").string());
    const bool win_now = win != nullptr && IsWindow(static_cast<HWND>(win));
    out->stop();
    const app::AudioOutStats s = out->stats();
    // S7.11: whether the window still existed when the shadow stopped (-1 no window; close_early runs inside WM_CLOSE)
    const int window_at_stop = win == nullptr ? -1 : (s.closed_early || win_now) ? 1 : 0;
    if (out->leaks_thread()) (void)out.release();  // S7.11 control audio-leak-thread: the running thread keeps its object
    out.reset();
    const app::ProcCounts after = app::proc_counts();  // S7.11: the thread joined, the device and its buffers released
    char b[1536];
    const int n = std::snprintf(
        b, sizeof b,
        "{\"mode\":\"%s\",\"requests\":%u,\"replayed\":%u,\"failed\":%u,\"ticks\":%u,\"delivered\":%u,"
        "\"dropped\":%u,\"late_ticks\":%u,\"skipped_periods\":%u,\"tick_span_ns\":%llu,\"tick_ns_total\":%llu,"
        "\"tick_ns_max\":%llu,\"buffers\":%u,\"real_buffers\":%u,\"real_failures\":%u,\"init_eax\":%u,\"sound_ready\":%d,"
        "\"device_less\":%s,\"thread_ended\":%s,\"dropped_requests\":%u,\"closed_early\":%s,\"closed_drops\":%u,\"window_at_stop\":%d,\"no_device_answers\":%u,"
        "\"heap_arena_bytes\":%u,\"heap_live_bytes\":%llu,\"heap_live_blocks\":%u,\"heap_peak_live_bytes\":%llu,"
        "\"heap_peak_arena_bytes\":%u,\"handles_before\":%u,\"threads_before\":%u,\"handles_after\":%u,"
        "\"threads_after\":%u,\"threads_added\":%u,\"files_present\":%u,\"files_absent\":%u,\"files_aliased\":%u,"
        "\"first_error\":\"%s\",\"first_failure\":\"%s\"}\n",
        mode.c_str(), s.requests, s.replayed, s.failed, s.ticks, s.delivered, s.dropped, s.late_ticks, s.skipped_periods,
        static_cast<unsigned long long>(s.tick_span_ns), static_cast<unsigned long long>(s.tick_ns_total),
        static_cast<unsigned long long>(s.tick_ns_max), s.sink.buffers, s.sink.real_buffers, s.sink.real_failures, s.init_eax,
        s.sound_ready, s.device_less ? "true" : "false", s.thread_ended ? "true" : "false", s.dropped_requests,
        s.closed_early ? "true" : "false", s.closed_drops, window_at_stop, s.sink.no_device_answers, s.heap.arena_bytes,
        static_cast<unsigned long long>(s.heap.live_bytes), s.heap.live_blocks,
        static_cast<unsigned long long>(s.heap_peak.live_bytes), s.heap_peak.arena_bytes, before.handles, before.threads,
        after.handles, after.threads, app::threads_added(before, after), s.files_present, s.files_absent, s.files_aliased,
        s.first_error.c_str(), s.sink.first_failure.c_str());
    if (n > 0) write_file(rec / "audio.json", b, static_cast<size_t>(std::min(n, static_cast<int>(sizeof b) - 1)));
  }
  ~AudioGuard() { finish(); }
};

// The present-trace run: everything the worker thread does.
PresentResult present_trace(const Options &o, AppFiles &files, const fs::path &rec) {
  PresentResult pr;
  std::vector<uint8_t> exe, trace;
  if (!read_file(o.exe, exe)) {
    pr.error = "cannot read the exe " + o.exe;
    return pr;
  }
  if (!exe_data_va(exe, pr.data_va)) {
    pr.error = "the exe has no PE32 .data section";
    return pr;
  }
  std::string why;
  portsim::ExeConstants &ec = portsim::default_exe_constants();
  if (!ec.loaded() && !ec.load_file(o.exe.c_str(), &why)) {
    pr.error = "exe refused: " + why;
    return pr;
  }
  std::string tp = o.trace;
  if (!fs::exists(tp)) tp = (fs::path(o.corpus) / (o.trace + ".trace")).string();
  if (!read_file(tp, trace)) {
    pr.error = "cannot read the trace " + tp;
    return pr;
  }
  const int32_t frames = count_frames(trace);
  if (frames < 0) {
    pr.error = "not a schema v1 trace: " + tp;
    return pr;
  }
  const int32_t maxf = o.max_records > 0 ? o.max_records : frames + 1;
  const size_t nsim = portsim::objects_of("sim").size();
  pr.run.T.assign(static_cast<size_t>(maxf), 0);
  pr.run.hashes.assign(static_cast<size_t>(maxf) * nsim, 0);

  auto ctx = std::make_unique<portsim::Ctx>();
  ctx->exe_ptr = &ec;
  ctx->files_ptr = &files;
  if (!ctx->mem.set_data_base(pr.data_va)) {
    pr.error = "set_data_base refused";
    return pr;
  }
  ec.fill_state(ctx->gs(), portsim::ExeConstants::Fill::InitialImage);
  if (!reg_call(*ctx, "gditext.attach", {}, &why) || !reg_call(*ctx, "gfx.attach", {}, &why) ||
      (o.snapshot_at >= 0 && !reg_call(*ctx, "gfx.snapshotAt", {static_cast<uint32_t>(o.snapshot_at)}, &why))) {
    pr.error = why;
    return pr;
  }
  TerrainGuard terrain;
  RegNegctl reg_negctl(o.negctl);
  terrain.attach(o, *ctx, rec);
  pr.run.code = run_guarded(ctx.get(), &trace, maxf, &pr.run);
  // the frame: the requested snapshot, else the back buffer now
  const std::string name = o.snapshot_at >= 0 ? "$gfx/snapshot_0.555" : "$gfx/back.555";
  if (o.snapshot_at < 0) reg_call(*ctx, "gfx.dump", {0}, &why);
  std::vector<uint8_t> raw(static_cast<size_t>(kLogicalW) * kLogicalH * 2);
  if (files.read(name, 0, raw.data(), raw.size()) == static_cast<int64_t>(raw.size())) {
    pr.frame.resize(raw.size() / 2);
    for (size_t i = 0; i < pr.frame.size(); ++i) pr.frame[i] = static_cast<uint16_t>(raw[2 * i] | raw[2 * i + 1] << 8);
  }
  reg_call(*ctx, "gfx.log", {}, &why);
  std::vector<uint8_t> log(static_cast<size_t>(std::max<int64_t>(0, files.size("$gfx/log.txt"))));
  if (!log.empty()) files.read("$gfx/log.txt", 0, log.data(), log.size());
  pr.gfx_log.assign(log.begin(), log.end());
  reg_call(*ctx, "gfx.detach", {}, &why);
  reg_call(*ctx, "gditext.detach", {}, &why);
  // the recording: framehash.bin / .json
  std::vector<uint8_t> fh;
  for (int32_t k = 0; k < pr.run.done; ++k) {
    const uint32_t hdr[2] = {pr.run.T[static_cast<size_t>(k)], static_cast<uint32_t>(k)};
    fh.insert(fh.end(), reinterpret_cast<const uint8_t *>(hdr), reinterpret_cast<const uint8_t *>(hdr) + 8);
    const uint8_t *h = reinterpret_cast<const uint8_t *>(&pr.run.hashes[static_cast<size_t>(k) * nsim]);
    fh.insert(fh.end(), h, h + nsim * 8);
  }
  write_file(rec / "framehash.bin", fh.data(), fh.size());
  std::string js = "{\"classes\":[\"sim\"],\"source\":\"simgolf_port present-trace\",\"symbols\":[";
  for (size_t i = 0; i < nsim; ++i) js += std::string(i ? "," : "") + "\"" + portsim::objects_of("sim")[i]->name + "\"";
  js += "]}\n";
  write_file(rec / "framehash.json", js.data(), js.size());
  write_file(rec / "gfx_log.txt", pr.gfx_log.data(), pr.gfx_log.size());
  pr.ok = !pr.frame.empty();
  if (!pr.ok) pr.error = "no frame (" + name + ")";
  return pr;
}

// ------------------------------------------------------------------------------------------------ the live path over a trace (S7.5)
int live_cpp(portsim::Ctx &ctx, portsim::LiveHost &host, const portsim::LiveOptions &opt, portsim::LiveResult &res,
             std::string &detail) {
  try {
    portsim::run_live(ctx, host, opt, res);
    return 0;
  } catch (const portsim::Fault &f) {
    detail = "fault at va " + std::to_string(f.va);
    return -3;
  } catch (const portsim::DivideError &) {
    detail = "divide error";
    return -9;
  } catch (const portsim::NoReturn &e) {
    detail = std::string("no return: ") + (e.where ? e.where : "?");
    return -10;
  } catch (const portsim::MissingResource &e) {
    detail = std::string("missing: ") + (e.what ? e.what : "?");
    return -11;
  } catch (...) {
    detail = "exception";
    return -8;
  }
}
int live_guarded(portsim::Ctx *ctx, portsim::LiveHost *host, const portsim::LiveOptions *opt, portsim::LiveResult *res,
                 std::string *detail) {
  __try {
    return live_cpp(*ctx, *host, *opt, *res, *detail);
  } __except (GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO || GetExceptionCode() == EXCEPTION_INT_OVERFLOW
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    return -9;
  }
}

// --live-trace: the start live_run makes (exe image, install files, GDI text, gfx, the terrain), then portsim::run_live with
// a LiveTracePresenter over the corpus trace (host.h). Writes, in the record directory: presents.tsv (index, kind, live
// frame, snapshots existing when it was made, sgh64), back.555 (the last present), snapshot_<k>.555 for each --snapshots
// ordinal, framehash.bin (the live run's records), gfx_log.txt, session.json.
int run_live_trace(const Options &o_in, AppFiles &files, const fs::path &rec) {
  Options o = o_in;  // P8B: the world layer's size (no window: --render-scale <pw>x<ph>, or 1 with --wide)
  world_size(o, 0, 0);
  std::string err, why, detail;
  std::vector<uint8_t> exe, trace;
  uint32_t data_va = 0;
  std::string tp = o.live_trace;
  if (!fs::exists(tp)) tp = (fs::path(o.corpus) / (o.live_trace + ".trace")).string();
  portsim::ExeConstants &ec = portsim::default_exe_constants();
  if (!read_file(o.exe, exe)) err = "cannot read the exe " + o.exe;
  else if (!exe_data_va(exe, data_va)) err = "the exe has no PE32 .data section";
  else if (!ec.loaded() && !ec.load_file(o.exe.c_str(), &why)) err = "exe refused: " + why;
  else if (!read_file(tp, trace) || count_frames(trace) < 0) err = "not a schema v1 trace: " + tp;
  portsim::LiveResult res;
  int code = 0;
  std::vector<LiveTracePresenter::Present> presents;
  std::vector<uint16_t> last;
  int64_t tpasses = -1, tpixels = -1, tcalls = -1;
  uint32_t snaps_taken = 0;
  uint64_t made[4] = {}, r_superseded = 0;  // S7.8: presents by kind r w m p, superseded 'r' presents
  std::string mem_json = "null", mem_tsv;   // S7.8 (U-1643)
  int64_t tlog_max = -1, tlog_end = -1;
  RecordStreams streams;  // S7.8: the recording and the frame records written as they are made
  bool streamed = false;
  std::string world_json = "null";  // P8B
  uint32_t world_shots_made = 0;
  TextResStats shot_text;               // P9C: the world shots' text runs
  int64_t shot_render_us = 0;           // P9C: the world shots' Presenter::render time
  portsim::GfxTextStats text_cap;       // P9C: the backend's text capture
  int64_t terrain_us = 0;
  if (err.empty()) {
    auto ctx = std::make_unique<portsim::Ctx>();
    ctx->exe_ptr = &ec;
    ctx->files_ptr = &files;
    if (!ctx->mem.set_data_base(data_va)) err = "set_data_base refused";
    if (err.empty()) {
      ec.fill_state(ctx->gs(), portsim::ExeConstants::Fill::InitialImage);
      bool ok = reg_call(*ctx, "gditext.attach", {}, &why) && reg_call(*ctx, "gfx.attach", {}, &why);
      for (int64_t s : o.snapshot_list) ok = ok && reg_call(*ctx, "gfx.snapshotAt", {static_cast<uint32_t>(s)}, &why);
      if (!ok) err = why;
      if (ok && hud_layers_on(o)) portsim::gfx_set_hud_capture(*ctx, true);  // P9B
      if (ok && hud_layers_on(o) && o.hud_modal) portsim::gfx_set_hud_modal(*ctx, true);  // P9D
      if (ok && hud_layers_on(o) && o.hud_modal && o.hud_popups) portsim::gfx_set_hud_popups(*ctx, true);  // P9E
      if (ok && hud_layers_on(o) && o.hud_modal && o.hud_close) portsim::gfx_set_hud_close(*ctx, true);  // P9E fix 2
      if (ok && hud_layers_on(o) && o.hud_cursor) portsim::gfx_set_hud_cursor(*ctx, true);  // P9E fix 4
      if (ok && line_res_on(o)) portsim::gfx_set_hud_lines(*ctx, true);  // P10I fix 1
      if (ok && o.hover_help) portsim::gfx_set_hover_help(*ctx, true);  // P11C
      if (ok && o.hover_help) portsim::gfx_set_hover_help_course(*ctx, o.hover_help_course);  // P13A item 2
      if (ok && hud_layers_on(o) && !o.hud_writes) portsim::gfx_set_hud_writes(*ctx, false);  // P10C fix 2
      if (ok && text_native_on(o)) portsim::gfx_set_text_capture(*ctx, true);  // P9C
    }
    if (err.empty()) {
      TerrainGuard terrain;
      RegNegctl reg_negctl(o.negctl);
      terrain.attach(o, *ctx, rec);
      auto inner = portsim::make_trace_live_host(*ctx, trace.data(), trace.size(), &why);
      if (!inner) err = "trace host refused: " + why;
      if (err.empty()) {
        LiveTracePresenter host(std::move(inner), static_cast<uint32_t>(o.snapshot_list.size()));
        host.no_present_point = o.negctl == "present-point-off";
        host.view_width = o.view_width;  // G32
        // P3: a present per frame at most, plus the boot's and modal screens' (51,109 for course_season_6y's 51,092 records)
        if (o.negctl != "presents-no-reserve") host.reserve_presents(static_cast<size_t>(std::max<int32_t>(0, count_frames(trace))) + 4096);
        TerrainLogBound tlog;  // S7.8 (U-1643)
        tlog.live = terrain.live.get();
        tlog.keep = o.negctl == "terrain-log-keep";
        if (tlog.live) host.terrain_log = &tlog;
        std::vector<uint32_t> ui_union;  // G32 development (--ui-union): per frame pixel, the presents where it was a 2D pixel
        uint32_t ui_presents = 0;
        if (o.ui_union && terrain.live) {
          ui_union.assign(static_cast<size_t>(kLogicalW) * kLogicalH, 0);
          host.on_present = [&](portsim::Ctx &c, uint32_t, const std::vector<uint16_t> &px) {
            WorldImage wi;
            if (!terrain.live->world() || !terrain.live->world_frame(px, portsim::gfx_back_terrain(c), wi.px, wi.mask)) return;
            ++ui_presents;
            for (size_t i = 0; i < wi.mask.size(); ++i) ui_union[i] += wi.mask[i] ? 0u : 1u;
          };
        }
        if (!o.world_shots.empty() && terrain.live) {  // P8B: --world-shots
          host.on_present = [&](portsim::Ctx &c, uint32_t index, const std::vector<uint16_t> &px) {
            // development (P9E): SG_P9E_PRESENT_LOG=<file>, one line per present: index, the HUD frame's serial, carried, none,
            // published with this back buffer, then id:anchor:dim per layer
            static std::FILE *pl = std::getenv("SG_P9E_PRESENT_LOG") ? std::fopen(std::getenv("SG_P9E_PRESENT_LOG"), "w") : nullptr;
            if (pl) {
              portsim::GfxHudFrame hf;
              const bool got = portsim::gfx_hud_frame(c, hf);
              std::fprintf(pl, "%u\t%llu\t%d\t%d\t%d\t", index, static_cast<unsigned long long>(got ? hf.serial : 0), got ? hf.carried : -1,
                           got ? hf.none : -1, got && hf.back == px ? 1 : 0);
              for (const portsim::GfxHudLayer &l : hf.layers) std::fprintf(pl, " %d:%d:%d", l.id, l.anchor, l.dim);
              std::fputc('\n', pl);
            }
            if (std::find(o.world_shots.begin(), o.world_shots.end(), static_cast<int64_t>(index)) == o.world_shots.end()) return;
            const portsim::LiveTerrainWorld *w = terrain.live->world();
            WorldImage wi;
            portsim::GfxTextFrame tf;  // P9C: the frame's text runs
            const bool text = text_native_on(o) && take_text_frame(c, px, tf);
            if (text && std::getenv("SG_P9C_RUN_LOG")) {  // development: the shot's runs (one line each)
              std::FILE *rl = std::fopen((rec / ("runs_" + std::to_string(index) + ".tsv")).string().c_str(), "wb");
              if (rl) {
                std::fprintf(rl, "x\ty\tbox\tlayer\tvis\tvis_layer\tface\tsize\tcolour\ttext\n");
                for (const portsim::GfxTextRun &r : tf.runs) {
                  size_t v = 0, vl = 0;
                  for (uint8_t b : r.vis) v += b;
                  for (uint8_t b : r.vis_layer) vl += b;
                  std::fprintf(rl, "%d\t%d\t%d,%d-%d,%d\t%d\t%zu\t%zu\t%s\t%d\t%06x\t%s\n", r.x, r.y, r.x0, r.y0, r.x1, r.y1, r.layer, v, vl,
                               r.face.c_str(), r.size, r.colorref, r.text.c_str());
                }
                std::fclose(rl);
              }
            }
            make_world_image(*terrain.live, c, px, hud_layers_on(o), wi, text ? &tf : nullptr,
                             hud_layers_on(o) && o.hud_cursor);  // P8B; P9B: with the HUD layers; P9E fix 4: the cursor's previews
            if (std::getenv("SG_P9D_LAYER_DUMP")) {  // development (P9D): the shot's layers (or why there are none)
              portsim::GfxHudFrame hf;
              const bool got = portsim::gfx_hud_frame(c, hf);
              dump_layers(rec, index, wi.hud, got ? hf.none : -1, got && hf.back == px, got ? hf.serial : 0);
            }
            const int cw =o.shot_w > 0 ? o.shot_w : (w ? w->pw : 800), ch = o.shot_h > 0 ? o.shot_h : (w ? w->ph : 600);
            Presenter p(o.negctl == "p9b-blend-opaque" ? PresentDefect::LayerBlendOpaque  // P9B controls
                        : o.negctl == "p9b-rect-move" ? PresentDefect::LayerRectMove
                        : o.negctl == "p9d-dim-local" ? PresentDefect::DimLocal  // P9D
                                                      : PresentDefect::None,
                        scale_mode(o.scale));
            p.set_wide(wide_fill(o));
            setup_text(p, o);  // P9C
            std::vector<uint32_t> img;
            p.set_frame(px, BandFill::Black);
            if (text) {
              portsim::GfxTextFrame t2 = tf;
              p.set_text(std::move(t2));
            }
            p.render(cw, ch, img);
            write_bmp(rec / ("frame_shot_" + std::to_string(index) + ".bmp"), img, cw, ch);
            p.set_hud_anchor(o.wide && o.hud_anchor);  // G32
            p.set_smooth_camera(o.smooth_camera && hud_layers_on(o));  // P12A fix 1: a single frame (no slide; its picture kept)
            p.set_frame_world(px, BandFill::Black, std::move(wi));
            p.set_text(std::move(tf));  // P9C
            const auto rt0 = std::chrono::steady_clock::now();
            p.render(cw, ch, img);
            shot_render_us += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - rt0).count();
            write_bmp(rec / ("world_shot_" + std::to_string(index) + ".bmp"), img, cw, ch);
            if (const TextResStats *ts = p.text_stats()) {  // P9C: the world shot's runs (accumulated for session.json)
              shot_text.runs += ts->runs;
              shot_text.clipped_runs += ts->clipped_runs;
              shot_text.clipped_px += ts->clipped_px;
              shot_text.failed += ts->failed;
            }
            if (o.world_check && w) {  // the raw layers for the render-scale-1 gate: world root, T, the frame
              const std::vector<uint16_t> &wp = terrain.live->world()->raster->pixels();
              write_file(rec / ("world_" + std::to_string(index) + ".555"), wp.data(), wp.size() * 2);
              const std::vector<uint16_t> &t8 = terrain.live->world()->terrain800;
              write_file(rec / ("t800_" + std::to_string(index) + ".555"), t8.data(), t8.size() * 2);
              write_file(rec / ("frame_" + std::to_string(index) + ".555"), px.data(), px.size() * 2);
            }
            ++world_shots_made;
          };
        }
        host.memory.every = static_cast<uint32_t>(std::max<int32_t>(0, o.mem_every));
        host.memory.sample(0, tlog.length());
        portsim::LiveOptions lopt;
        lopt.max_records = o.max_records;
        lopt.scenario = "simgolf_port live-trace";
        if (!o.in_memory) streamed = streams.open(rec.string(), lopt);  // S7.8 (U-1643)
        AudioGuard audio;  // S7.10
        audio.attach(o, *ctx, ec, data_va, files.install_root(), nullptr, rec);
        struct UiOut {  // G32 development: ui_union.bin (u32 per pixel) and its present count
          const std::vector<uint32_t> &u;
          const uint32_t &n;
          fs::path p;
          ~UiOut() {
            if (u.empty()) return;
            write_file(p / "ui_union.bin", u.data(), u.size() * 4);
            const std::string t = std::to_string(n) + "\n";
            write_file(p / "ui_union.txt", t.data(), t.size());
          }
        } ui_out{ui_union, ui_presents, rec};
        const auto run_t0 = std::chrono::steady_clock::now();
        code = live_guarded(ctx.get(), &host, &lopt, &res, &detail);
        terrain_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - run_t0).count();
        audio.finish();
        streams.close();
        host.memory.sample(static_cast<uint32_t>(res.records), tlog.length());
        presents = host.presents();
        last = host.last();
        if (terrain.live && terrain.live->world()) {  // P8B
          const portsim::LiveTerrainWorld &w = *terrain.live->world();
          world_json = "{\"pw\":" + std::to_string(w.pw) + ",\"ph\":" + std::to_string(w.ph) + ",\"fw\":" + std::to_string(w.fw) +
                       ",\"ox\":" + std::to_string(w.ox) + ",\"margin\":" + std::to_string(w.margin) + ",\"passes\":" +
                       std::to_string(w.passes) + ",\"pixels\":" + std::to_string(w.pixels) + ",\"us\":" + std::to_string(w.us) +
                       ",\"checks\":" + std::to_string(w.checks) + ",\"checks_differing\":" + std::to_string(w.checks_differing) +
                       ",\"diff_px\":" + std::to_string(w.diff_px) + ",\"shots\":" + std::to_string(world_shots_made) +
                       ",\"view_us\":" + std::to_string(terrain_us) + ",\"hr\":{\"on\":" + (w.hr_on ? "true" : "false") +
                       ",\"filter\":" + std::to_string(w.hr_filter) +
                       ",\"draws\":" + std::to_string(w.hr_draws) + ",\"queues\":" + std::to_string(w.hr_queues) + ",\"prep_us\":" +
                       std::to_string(w.hr_prep_us) + ",\"queue_us\":" + std::to_string(w.hr_queue_us) + ",\"label_us\":" +
                       std::to_string(w.hr_label_us) + ",\"flush_us\":" + std::to_string(w.hr_flush_us) + ",\"tiles\":" +
                       std::to_string(w.hr_tiles) + ",\"tiles_dirty\":" + std::to_string(w.hr_tiles_dirty) + "}}";  // P10D
        }
        if (terrain.live) {
          tpasses = terrain.live->view().passes;
          tpixels = terrain.live->view().pixels_drawn;
          tcalls = static_cast<int64_t>(tlog.calls());
          tlog_max = static_cast<int64_t>(tlog.max_len);
          tlog_end = static_cast<int64_t>(tlog.length());
        }
        for (int k = 0; k < 4; ++k) made[k] = host.made[k];
        r_superseded = host.r_superseded;
        mem_json = host.memory.json();
        mem_tsv = host.memory.tsv();
      }
      for (size_t k = 0; k < o.snapshot_list.size(); ++k) {
        const std::string name = "$gfx/snapshot_" + std::to_string(k) + ".555";
        const int64_t n = files.size(name);
        if (n <= 0) continue;
        std::vector<uint8_t> b(static_cast<size_t>(n));
        files.read(name, 0, b.data(), b.size());
        write_file(rec / ("snapshot_" + std::to_string(k) + ".555"), b.data(), b.size());
        ++snaps_taken;
      }
      text_cap = portsim::gfx_text_stats(*ctx);  // P9C
      reg_call(*ctx, "gfx.log", {}, &why);
      std::vector<uint8_t> log(static_cast<size_t>(std::max<int64_t>(0, files.size("$gfx/log.txt"))));
      if (!log.empty()) files.read("$gfx/log.txt", 0, log.data(), log.size());
      write_file(rec / "gfx_log.txt", log.data(), log.size());
      reg_call(*ctx, "gfx.detach", {}, &why);
      reg_call(*ctx, "gditext.detach", {}, &why);
    }
  }
  std::string pt = "index\tkind\tframe\tsnaps\thash\n";
  for (const auto &p : presents) {
    char line[128];
    std::snprintf(line, sizeof line, "%u\t%c\t%u\t%u\t%016llx\n", p.index, p.kind, p.frame, p.snaps,
                  static_cast<unsigned long long>(p.hash));
    pt += line;
  }
  write_file(rec / "presents.tsv", pt.data(), pt.size());
  if (!last.empty()) write_file(rec / "back.555", last.data(), last.size() * 2);
  if (!streamed) {  // S7.8: in memory until now (--in-memory, or the files could not be opened)
    const std::vector<uint8_t> fh = framehash_bytes(res);
    write_file(rec / "framehash.bin", fh.data(), fh.size());
    write_file(rec / "trace.trace", res.trace.data(), res.trace.size());
  }
  std::string s = "{\"app\":\"simgolf_port\",\"mode\":\"live-trace\",\"trace\":\"" + jesc(tp) + "\",\"terrain\":\"" + o.terrain + "\"";
  s += profile_json(o.profile);  // S7.12
  s += ",\"error\":\"" + jesc(err) + "\",\"code\":" + std::to_string(code) + ",\"detail\":\"" + jesc(detail.substr(0, 2000)) + "\"";
  s += ",\"stop\":\"" + jesc(res.stop) + "\",\"records\":" + std::to_string(res.records);
  s += ",\"violations\":" + std::to_string(res.violations) + ",\"presents\":" + std::to_string(presents.size());
  s += ",\"snapshots_requested\":" + std::to_string(o.snapshot_list.size()) + ",\"snapshots_taken\":" + std::to_string(snaps_taken);
  s += ",\"terrain_passes\":" + std::to_string(tpasses) + ",\"terrain_pixels\":" + std::to_string(tpixels);
  s += ",\"terrain_calls\":" + std::to_string(tcalls);
  s += ",\"world\":" + world_json + ",\"render_scale\":\"" + jesc(o.render_scale) + "\"";  // P8B
  s += ",\"p9c\":{\"text_res\":\"" + o.text_res + "\",\"text_aa\":\"" + o.text_aa + "\",\"captured\":" +  // P9C
       std::to_string(text_cap.captured) + ",\"published_last\":" + std::to_string(text_cap.published) + ",\"dropped\":" +
       std::to_string(text_cap.dropped) + ",\"no_font\":" + std::to_string(text_cap.no_font) + ",\"frames\":" +
       std::to_string(text_cap.frames) + ",\"merged\":" + std::to_string(text_cap.merged) + ",\"shot_runs\":" + std::to_string(shot_text.runs) + ",\"shot_clipped_runs\":" +
       std::to_string(shot_text.clipped_runs) + ",\"shot_clipped_px\":" + std::to_string(shot_text.clipped_px) +
       ",\"shot_failed\":" + std::to_string(shot_text.failed) + ",\"shot_render_us\":" + std::to_string(shot_render_us) + "}";
  s += ",\"presents_by_kind\":{\"r\":" + std::to_string(made[0]) + ",\"w\":" + std::to_string(made[1]) + ",\"m\":" +
       std::to_string(made[2]) + ",\"p\":" + std::to_string(made[3]) + "},\"r_superseded\":" + std::to_string(r_superseded);
  s += ",\"terrain_log_max\":" + std::to_string(tlog_max) + ",\"terrain_log_end\":" + std::to_string(tlog_end);
  s += ",\"memory\":" + mem_json;
  s += ",\"streamed\":" + std::string(streamed ? "true" : "false") + ",\"trace_bytes\":" +
       std::to_string(res.trace.size() + res.trace_file_bytes) + ",\"spilled_events\":" + std::to_string(res.spilled_events) +
       ",\"held_events\":" + std::to_string(res.held_events);
  s += ",\"data_va\":" + std::to_string(data_va) + "}\n";
  if (!mem_tsv.empty()) write_file(rec / "memory.tsv", mem_tsv.data(), mem_tsv.size());
  write_file(rec / "session.json", s.data(), s.size());
  std::printf("DONE live-trace records=%d code=%d stop=%s presents=%zu snapshots=%u\n", res.records, code, res.stop.c_str(),
              presents.size(), snaps_taken);
  if (!err.empty()) std::fprintf(stderr, "%s\n", err.c_str());
  return err.empty() ? 0 : 1;
}

// ------------------------------------------------------------------------------------------------ the live session (S7.6)
struct LiveOut {
  std::string error;
  LiveSession::Result res;
  uint32_t data_va = 0;
  int64_t terrain_calls = -1, terrain_passes = -1;  // S7.5: LiveTerrain's logged Terrain calls and draw passes (-1: headless)
};

// The sim thread of a live session: the start present_trace makes (the exe's initialised data at its .data address, the
// install's files through the saves overlay, GDI text, the gfx backend), then LiveSession::run until the session ends.
LiveOut live_run(const Options &o, AppFiles &files, LiveSession &session, const fs::path &rec) {
  LiveOut lo;
  std::vector<uint8_t> exe;
  if (!read_file(o.exe, exe)) {
    lo.error = "cannot read the exe " + o.exe;
    return lo;
  }
  if (!exe_data_va(exe, lo.data_va)) {
    lo.error = "the exe has no PE32 .data section";
    return lo;
  }
  std::string why;
  portsim::ExeConstants &ec = portsim::default_exe_constants();
  if (!ec.loaded() && !ec.load_file(o.exe.c_str(), &why)) {
    lo.error = "exe refused: " + why;
    return lo;
  }
  auto ctx = std::make_unique<portsim::Ctx>();
  ctx->exe_ptr = &ec;
  ctx->files_ptr = &files;
  if (!ctx->mem.set_data_base(lo.data_va)) {
    lo.error = "set_data_base refused";
    return lo;
  }
  ec.fill_state(ctx->gs(), portsim::ExeConstants::Fill::InitialImage);
  if (!reg_call(*ctx, "gditext.attach", {}, &why) || !reg_call(*ctx, "gfx.attach", {}, &why)) {
    lo.error = why;
    return lo;
  }
  if (hud_layers_on(o)) portsim::gfx_set_hud_capture(*ctx, true);  // P9B: the HUD pixel layers (gfx.h; no GameState byte changes)
  if (hud_layers_on(o) && o.hud_modal) portsim::gfx_set_hud_modal(*ctx, true);  // P9D: the modal screens over them (gfx.h)
  if (hud_layers_on(o) && o.hud_modal && o.hud_popups) portsim::gfx_set_hud_popups(*ctx, true);  // P9E: the popups' place (gfx.h)
  if (hud_layers_on(o) && o.hud_modal && o.hud_close) portsim::gfx_set_hud_close(*ctx, true);  // P9E fix 2: the close frame (gfx.h)
  if (hud_layers_on(o) && o.hud_cursor) portsim::gfx_set_hud_cursor(*ctx, true);  // P9E fix 4: the cursor's previews (gfx.h)
  if (line_res_on(o)) portsim::gfx_set_hud_lines(*ctx, true);  // P10I fix 1: the tee lines (gfx.h; records only)
  if (o.hover_help) portsim::gfx_set_hover_help(*ctx, true);  // P11C: the hover help lines (gfx.h; drawing only)
  if (o.hover_help) portsim::gfx_set_hover_help_course(*ctx, o.hover_help_course);  // P13A item 2: and the course line
  if (hud_layers_on(o) && !o.hud_writes) portsim::gfx_set_hud_writes(*ctx, false);  // P10C fix 2: P9B's diff only (gfx.h)
  if (text_native_on(o)) portsim::gfx_set_text_capture(*ctx, true);  // P9C: the text runs (gfx.h; no GameState byte changes)
  // --negctl recorder:<mode>: the live core's recorder controls (live.cpp live.recorder)
  int *rec_mode = nullptr;
  if (o.negctl.rfind("recorder:", 0) == 0) {
    const int i = portsim::registry_find("live.recorder");
    if (i >= 0) rec_mode = portsim::registry_at(i, nullptr)->negctl;
    if (rec_mode) *rec_mode = std::atoi(o.negctl.c_str() + 9);
  }
  TerrainGuard terrain;
  RegNegctl reg_negctl(o.negctl);
  terrain.attach(o, *ctx, rec);
  session.terrain_view = terrain.live ? &terrain.live->view() : nullptr;
  TerrainLogBound tlog;  // S7.8 (U-1643)
  tlog.live = terrain.live.get();
  tlog.keep = o.negctl == "terrain-log-keep";
  session.terrain_log = tlog.live ? &tlog : nullptr;
  session.memory.every = static_cast<uint32_t>(std::max<int32_t>(0, o.mem_every));
  AudioGuard audio;  // S7.10
  audio.attach(o, *ctx, ec, lo.data_va, files.install_root(), o.audio_hwnd, rec);
  lo.res = session.run(*ctx);
  audio.finish();
  session.terrain_view = nullptr;
  session.terrain_log = nullptr;
  if (terrain.live) {
    lo.terrain_calls = static_cast<int64_t>(tlog.calls());
    lo.terrain_passes = terrain.live->view().passes;
  }
  if (rec_mode) *rec_mode = 0;
  reg_call(*ctx, "gfx.log", {}, &why);
  std::vector<uint8_t> log(static_cast<size_t>(std::max<int64_t>(0, files.size("$gfx/log.txt"))));
  if (!log.empty()) files.read("$gfx/log.txt", 0, log.data(), log.size());
  write_file(rec / "gfx_log.txt", log.data(), log.size());
  reg_call(*ctx, "gfx.detach", {}, &why);
  reg_call(*ctx, "gditext.detach", {}, &why);
  return lo;
}

int run_live_window(const Options &o, AppFiles &files, const fs::path &rec) {
  HostOptions ho;
  ho.idle_ms = o.idle_ms;
  ho.pass_ms = o.pass_ms;
  ho.timed_passes = o.timed_passes;
  ho.cursor = o.cursor == "message" ? CursorSource::Message : CursorSource::System;
  ho.flag17_from = o.flag17_from;
  ho.flag17_to = o.flag17_to;
  ho.max_records = o.max_records;
  ho.clock_origin = o.clock_origin;
  ho.keys_from_messages = o.keys == "message";
  ho.in_memory = o.in_memory;
  ho.edge_guard = o.edge_guard;      // P5B item 1
  ho.close_prompt = o.close_prompt;  // P5B item 3
  ho.loading_overlay = o.loading_overlay;  // P6B item 2
  ho.world = o.wide || (o.render_scale != "1" && o.render_scale != "auto");  // P8B: presents carry the world layer (when one exists)
  ho.wide_edge = o.wide && o.wide_edge && ho.world;   // P8B
  ho.wide = o.wide;                                    // G32: W follows the window
  static PanelRemap g_remap;                           // G32: the HUD compositor's remap (window and sim threads)
  if (o.wide && o.hud_anchor) ho.remap = &g_remap;
  ho.hud_layers = hud_layers_on(o);                    // P9B
  ho.vacated_clicks = ho.remap && o.hud_vacated;       // P9E fix 3 (the play switch mainLoop.vacatedPanelClick acts on it)
  ho.cursor_previews = ho.remap && hud_layers_on(o) && o.hud_cursor;  // P9E fix 4
  ho.cursor_live = ho.cursor_previews && o.paint_rate == "display";   // P10E
  // P12A fix 1: the camera slide needs the HUD layers (the course picture apart from the HUD), the remap (clicks follow the slide)
  // and the world layer
  ho.smooth_camera = o.smooth_camera && ho.remap && ho.hud_layers && ho.world;
  // P12A fix 2: the moving objects need the window-resolution sprite queue (--sprite-res native) and the world layer
  ho.smooth_objects = o.smooth_objects && o.sprite_native && ho.world;
  portsim::gfx_set_hr_scene(ho.smooth_objects);
  ho.world_thread = o.world_thread && ho.world;  // P15A (the hr work recorded for it only with a world layer)
  portsim::gfx_set_hr_defer(ho.world_thread);
  // P15A fix 3: the world raster on the world thread (not with the render-scale-1 gate, which reads the sim's whole world root)
  ho.world_raster_thread = ho.world_thread && o.world_raster_thread && !o.world_check;
  portsim::gfx_set_world_raster_defer(ho.world_raster_thread);
  portsim::gfx_set_hr_defer_check(ho.world_thread && std::getenv("SG_P15_CHECK") && std::getenv("SG_P15_CHECK")[0] == '1');  // P15A
  ho.title_bands = o.wide && o.title_bands;           // P10C fix 1 (the band hint acts only with --wide)
  ho.map_bands = o.wide && o.map_bands;               // P13B item 18 (the world map's bands repeat its edge columns)
  ho.text_native = text_native_on(o);                  // P9C
  ho.text_banner = ho.text_native && o.text_banner;    // P10B fix 1
  ho.world_mode = o.render_scale == "native" || (o.render_scale == "auto" && o.wide) ? 2 : (o.render_scale == "1" && o.wide ? 1 : 0);
  if (o.negctl == "pacing-off") ho.defect = HostDefect::PacingOff;
  if (o.negctl == "present-late") ho.defect = HostDefect::PresentLate;
  if (o.negctl == "present-point-off") ho.defect = HostDefect::NoPresentPoint;
  if (o.negctl == "view-write-unrecorded") ho.defect = HostDefect::ViewWriteUnrecorded;  // G32
  LiveSession session(ho, rec.string());

  PresentDefect pd = o.negctl == "present-stale" ? PresentDefect::Stale
                     : o.negctl == "present-swap" ? PresentDefect::ByteSwap
                     : o.negctl == "p9b-blend-opaque" ? PresentDefect::LayerBlendOpaque  // P9B
                     : o.negctl == "p9b-rect-move" ? PresentDefect::LayerRectMove        // P9B
                     : o.negctl == "p9d-dim-local" ? PresentDefect::DimLocal             // P9D
                                                  : PresentDefect::None;
  Presenter presenter(pd, scale_mode(o.scale));
  presenter.set_wide(wide_fill(o));  // P7
  setup_text(presenter, o);          // P9C
  presenter.set_frame(std::vector<uint16_t>(static_cast<size_t>(kLogicalW) * kLogicalH, 0));  // black until a frame comes
  // P10A: the profiler (on with the overlay or a CSV; F12 turns it on when it cycles the overlay on) and the overlay
  presenter.set_overlay(o.fps == "detail" ? 2 : o.fps == "on" ? 1 : 0);
  if (o.fps != "off" || !o.profile_csv.empty()) Profiler::get().set_enabled(true);
  Profiler::get().set_fps_changed(ho.smooth_camera || ho.smooth_objects);  // P12A fixes 1 and 2 (before the CSV header: its columns follow it)
  if (!o.profile_csv.empty() && !Profiler::get().open_csv(o.profile_csv))
    std::fprintf(stderr, "profile: cannot write %s\n", o.profile_csv.c_str());
  presenter.set_hover(o.hover_rate == "display");  // P10I fix 2
  if (ho.remap) {  // G32: the HUD compositor
    presenter.set_hud_anchor(true);
    presenter.set_remap(const_cast<PanelRemap *>(ho.remap));
  }
  const bool smooth_camera = ho.smooth_camera;  // P12A fix 1
  presenter.set_smooth_camera(smooth_camera);
  presenter.set_smooth_objects(ho.smooth_objects);  // P12A fix 2
  if (o.object_rate < 0) presenter.set_object_rate_auto();  // P14A fix 2
  else presenter.set_object_rate(o.object_rate);         // P12A fix 3
  presenter.set_gpu_objects(o.gpu_objects);               // P15C
  Profiler::get().set_fps_changed(smooth_camera || ho.smooth_objects);
  WindowOptions wo;
  wo.remap = ho.remap;  // G32
  wo.dpi = o.dpi;
  if (o.wide) wo.logical_w = o.wide_width;  // P7
  wo.activate = o.activate;
  wo.input_defect = o.negctl == "input-bypass" ? InputDefect::Bypass : InputDefect::None;
  wo.resizable = o.resizable;  // P5B item 4
  wo.zoom_keys = static_cast<uint8_t>(o.zoom_keys == "all" ? 2 : o.zoom_keys == "numpad" ? 1 : 0);  // P5B item 2
  wo.zoom_context = [&session]() { return session.in_frame_body(); };
  {  // P11D: Ctrl+Z / Ctrl+Y only when the profile turns the sim's play switch mainLoop.courseUndo on (0)
    const int d = portsim::defect_find("mainLoop.courseUndo");
    if (o.undo_keys && d >= 0 && *portsim::defect_at(d, nullptr)->flag == 0) {
      wo.on_undo_key = [&session](int op) { session.request_undo(op); };
      wo.undo_context = [&session]() { return session.in_frame_body(); };
    }
  }
  if (o.close_prompt) wo.on_close_button = [&session]() { session.request_close(); };  // P5B item 3
  wo.focus_child_takes = [&session](uint32_t msg, uint32_t wp) { return session.focus_child_takes(msg, wp); };  // U-1616
  wo.before_translate = [&session](uint32_t msg, uint32_t wp) { session.before_translate(msg, wp); };  // S7.4
  wo.key_state = [&session](uint32_t vk, bool async) { return session.key_state(vk, async); };
  wo.on_overlay_key = [&presenter]() {  // P10A: F12 cycles on -> detail -> off
    const int m = presenter.overlay() == 1 ? 2 : presenter.overlay() == 2 ? 0 : 1;
    presenter.set_overlay(m);
    if (m) Profiler::get().set_enabled(true);
  };
  AppWindow win(presenter, wo);
  if (!win.create()) {
    std::fprintf(stderr, "%s\n", win.error().c_str());
    return 4;
  }
  HWND hwnd = static_cast<HWND>(win.hwnd());
  {  // P14A: the GPU path (gpu.h); the CPU path stays when it cannot start
    std::string why = "--gpu off";
    const bool on = o.gpu && presenter.gpu_start(hwnd, why);
    std::printf("GPU %s %s\n", on ? "on" : "off", why.c_str());
    std::fflush(stdout);
  }
  session.set_window(hwnd, win.viewport());
  Options ow = o;  // P8B: the world layer's size from the window's client at the session's start (render_scale native), the
                   // wide width (render scale 1 with --wide) or the explicit size
  world_size(ow, win.client_w(), win.client_h());
  std::FILE *log = std::fopen((rec / "input.jsonl").string().c_str(), "wb");
  std::FILE *shown = std::fopen((rec / "shown.tsv").string().c_str(), "wb");
  if (shown) std::fprintf(shown, "index\thash\n");
  uint32_t events = 0;
  // development (P12A): SG_P12_LOG=<file>, one line per frame with a world layer: index, the camera snapshot (ok, x, y, zoom, rot),
  // the slide start (reason 0 slid / 1 no snapshot / 2 zoom or rotation / 3 too far / 4 screen / 5 no move, D x / y in 800-space,
  // start offset x / y in client pixels, period ms, the next camera's shift x / y), and one `click` line per left button press: the
  // client point, the slide offset shown (client pixels), the point the game gets (logical), the shown point in the frame's own
  // coordinates and the tile proj_tile gives there (the tile under the cursor in the shown picture), the tile of the unslid point
  static std::FILE *slog = std::getenv("SG_P12_LOG") ? std::fopen(std::getenv("SG_P12_LOG"), "w") : nullptr;
  win.set_sink([&](const InputEvent &e) {
    ++events;
    if (log) {
      std::fprintf(log, "%s\n", event_json(e).c_str());
      std::fflush(log);
    }
    if (slog && e.msg == 0x201 && e.has_lp_edge) {  // WM_LBUTTONDOWN
      const CursorProj &P = presenter.world().proj;
      int32_t ox = 0, oy = 0;
      presenter.slide_offset(ox, oy);
      const int32_t cx = static_cast<int16_t>(e.lp_edge & 0xffff), cy = static_cast<int16_t>(e.lp_edge >> 16);
      const Translator t(e.vp);
      const Point game = t.lparam_point(e.lp), shown = t.to_logical(Point{cx - ox, cy - oy}), raw = t.to_logical(Point{cx, cy});
      int32_t sx = -1, sy = -1, rx = -1, ry = -1;
      const bool ok1 = P.ok && proj_tile(P, shown.x, shown.y, sx, sy), ok2 = P.ok && proj_tile(P, raw.x, raw.y, rx, ry);
      std::fprintf(slog, "click\t%u\t%d,%d\t%d,%d\t%d,%d\t%d,%d\t%d\t%d,%d\t%d,%d\n", presenter.frames_set(), cx, cy, ox, oy, game.x, game.y,
                   shown.x, shown.y, ok1 ? 1 : 0, sx, sy, ok2 ? rx : -1, ok2 ? ry : -1);
      std::fflush(slog);
    }
    session.on_input(e);
  });
  LiveOut lo;
  Options oa = ow;  // S7.10: --audio on uses the window's handle for DirectSound's cooperative level (P8B: ow's world size)
  oa.audio_hwnd = hwnd;
  // S7.11: WM_CLOSE stops the audio before the window goes (audio_close_proc); --negctl audio-close-late leaves it to the
  // sim thread's finish, after the window is gone (the control)
  if (o.audio != "off" && o.negctl != "audio-close-late")
    g_close_prev = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&audio_close_proc)));
  winlog_open_env();  // P16A (SG_P16_PAINTLOG)
  // P16A (log/roadmap/P16/P16A_notes.md): the window thread ahead of the rasteriser workers (normal) and the world thread (below
  // normal), level with the sim thread (above normal), and the pacer above it, so a due paint is not queued behind them on a busy
  // machine. Nothing the game computes depends on it. SG_APP_P16=off or SG_P16_PRIO=0: both at normal as before.
  const bool p16_prio = !(std::getenv("SG_APP_P16") && !std::strcmp(std::getenv("SG_APP_P16"), "off")) &&
                        !(std::getenv("SG_P16_PRIO") && std::atoi(std::getenv("SG_P16_PRIO")) == 0);
  if (p16_prio) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  std::thread sim([&]() { lo = live_run(oa, files, session, rec); });
  std::printf("READY hwnd=0x%llx client=%dx%d dpi=%u record=%s\n", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)),
              win.client_w(), win.client_h(), win.dpi(), rec.string().c_str());
  std::fflush(stdout);
  if (o.exit_after) SetTimer(hwnd, 1, o.exit_after, nullptr);
  // P10E (--paint-rate display): a pacer thread posts kMsgCursorTick at the display's refresh rate (GetDeviceCaps VREFRESH; 60
  // when it reports 0 / 1 or a value outside 30..360), at most one waiting at a time, so a paint longer than a refresh period
  // only skips ticks; the sim thread never waits on the window. Each tick moves the frame's preview to the cursor's tile.
  constexpr UINT kMsgCursorTick = WM_APP + 4;
  std::atomic<bool> tick_pending{false}, pacer_stop{false};
  uint64_t ticks = 0, live_moves = 0;
  std::thread pacer;
  int pace_hz = 0;
  if (ho.cursor_live || presenter.hover() || smooth_camera || ho.smooth_objects) {  // P10I fix 2: the hover highlights take the same ticks; P12A: the slide
    HDC dc = GetDC(hwnd);
    pace_hz = dc ? GetDeviceCaps(dc, VREFRESH) : 0;
    if (dc) ReleaseDC(hwnd, dc);
    if (pace_hz < 30 || pace_hz > 360) pace_hz = 60;
    // development (P16A, log/roadmap/P16/P16A_notes.md): SG_P16_PACE_HZ=<n> ticks from the timer alone at n per second on absolute
    // deadlines (no WaitForVBlank), to measure the window thread's share of the CPU with the display out of the loop (a monitor in
    // its power-off reports no vertical blanks); with SG_P14_SYNC=0 the Present does not wait for DWM either
    int force_hz = 0;
    if (const char *e = std::getenv("SG_P16_PACE_HZ")) force_hz = std::atoi(e);
    if (force_hz >= 30 && force_hz <= 1000) pace_hz = force_hz;
    else force_hz = 0;
    pacer = std::thread([&tick_pending, &pacer_stop, &presenter, hwnd, pace_hz, force_hz, p16_prio]() {
      if (p16_prio) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);  // P16A
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
      HANDLE t = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
      if (!t) t = CreateWaitableTimerW(nullptr, FALSE, nullptr);
      LARGE_INTEGER due;
      due.QuadPart = -(10000000LL / pace_hz);  // one refresh period, in 100 ns units, relative
      const int64_t period_ns = 1000000000LL / pace_hz;
      int64_t next_ns = portsim::perf_now_ns() + period_ns;  // P16A: SG_P16_PACE_HZ's absolute deadlines
      while (!pacer_stop.load()) {
        if (force_hz) {  // P16A (development)
          const int64_t left = next_ns - portsim::perf_now_ns();
          if (left > 0) {
            LARGE_INTEGER dl;
            dl.QuadPart = -(left / 100);
            if (t && SetWaitableTimer(t, &dl, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(t, 1000);
            else Sleep(static_cast<DWORD>(left / 1000000));
          }
          next_ns += period_ns;
          const int64_t now = portsim::perf_now_ns();
          if (next_ns < now) next_ns = now + period_ns;  // late by more than a period: the missed ticks are not made up
          winlog_vblank(now);
          if (!tick_pending.exchange(true)) PostMessageW(hwnd, kMsgCursorTick, 0, 0);
          continue;
        }
        // P14A: with the GPU path, the tick comes right after the monitor's vertical blank (IDXGIOutput::WaitForVBlank), so the paint
        // composes and presents for the next refresh without waiting inside the paint; a wait that returns at once (an output that
        // reports no vblank) falls back to the timer
        const int64_t v0 = portsim::perf_now_ns();
        const bool vb = presenter.gpu_wait_vblank() && portsim::perf_now_ns() - v0 > 1000000;
        if (vb) {
          winlog_vblank(portsim::perf_now_ns());  // P16A (SG_P16_PAINTLOG)
        } else if (t && SetWaitableTimer(t, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(t, 1000);
        else Sleep(1000 / pace_hz);
        if (!tick_pending.exchange(true)) PostMessageW(hwnd, kMsgCursorTick, 0, 0);
      }
      if (t) CloseHandle(t);
    });
  }
  auto cursor_tick = [&]() {
    const CursorProj &P = presenter.world().proj;
    if (!P.ok) return;
    int32_t lx = 0, ly = 0, dx = 0, tx = 0, ty = 0;
    if (!session.live_cursor(lx, ly, dx)) return;  // the sim's read would not take this point: the preview stays where it is
    // P7 draws no placement preview with the cursor's y at or below 0x1e0 (ML+0xa774, ML+0xb3d0): the preview stays where the
    // cursor left the course until the next frame removes it
    if (ly >= 0x1e0) return;
    if (!proj_tile(P, lx, ly, tx, ty) || tx < 0 || tx >= 50 || ty < 0 || ty >= 50) return;
    const size_t a = static_cast<size_t>(P.tile_x * 50 + P.tile_y), b = static_cast<size_t>(tx * 50 + ty);
    if (!P.on[b]) return;
    Presenter::CursorLive c;
    c.sx = P.sx[b] - P.sx[a];
    c.sy = P.sy[b] - P.sy[a];
    c.dx = dx;
    c.active = c.sx != 0 || c.sy != 0 || (ho.remap && dx != ho.remap->cursor_dx());
    if (!c.active) c = Presenter::CursorLive{};
    if (c != presenter.cursor_live()) {
      presenter.set_cursor_live(c);
      win.invalidate();
      ++live_moves;
    }
  };
  Viewport vp = win.viewport();
  uint64_t hover_paints = 0;  // P10I fix 2
  auto hover_tick = [&]() {   // P10I fix 2: the hover looks at the live cursor point (the sim's next read would get it)
    if (!presenter.hover()) return;
    int32_t lx = 0, ly = 0, dx = 0;
    if (!session.live_cursor(lx, ly, dx)) return;
    if (presenter.hover_tick(lx, ly, vp)) {
      win.invalidate();
      ++hover_paints;
      presenter.mark_changed();  // P12A fix 1: the FPS overlay counts the swap's paint
    }
  };
  uint64_t slide_paints = 0, object_paints = 0;  // P12A fix 1: ticks that moved the camera slide (each asks for a paint)
  auto slide_tick = [&]() {   // P12A fix 1: the slide's offset for now (set before the preview's tile is found: it reads the slid point)
    if (ho.smooth_objects && presenter.objects_tick(portsim::perf_now_ns())) {  // P12A fix 2: the objects between game frames
      win.invalidate();
      ++object_paints;
    }
    if (!smooth_camera) return;
    const int64_t now = portsim::perf_now_ns();
    if (presenter.slide_tick(now, vp)) {
      win.invalidate();
      ++slide_paints;
      if (slog) {  // development: the offset each slide step asks to paint
        int32_t ox = 0, oy = 0;
        presenter.slide_offset(ox, oy);
        // and the tile under the cursor in the picture this step shows (proj_tile of the live point; -1 without a projection)
        int32_t lx = 0, ly = 0, ldx = 0, tx = -1, ty = -1;
        const CursorProj &P = presenter.world().proj;
        if (!(P.ok && session.live_cursor(lx, ly, ldx) && proj_tile(P, lx, ly, tx, ty))) tx = ty = -1;
        std::fprintf(slog, "step\t%lld\t%u\t%d,%d\t%d,%d\n", static_cast<long long>(now / 1000), presenter.frames_set(), ox, oy, tx, ty);
      }
    }
  };
  // (SG_P12_LOG: slog, opened above the input sink)
  MSG m;
  std::vector<uint16_t> px;
  const UINT msg_shot = RegisterWindowMessageW(L"SimGolfPort.P14Shot");  // P14A: the harness's capture of the GPU picture
  const char *shot_dir = std::getenv("SG_P14_SHOT");
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    if (m.hwnd == hwnd && msg_shot && m.message == msg_shot) {  // P14A
      if (shot_dir) presenter.gpu_shot(std::string(shot_dir) + "/shot_" + std::to_string(m.wParam) + ".bmp");
      win.invalidate();
      continue;
    }
    if (m.hwnd == hwnd && m.message == kMsgCursorTick) {  // P10E
      const bool wl = winlog_on();  // P16A (SG_P16_PAINTLOG)
      const int64_t wl_t0 = wl ? portsim::perf_now_ns() : 0;
      const uint64_t wl_c0 = wl ? winlog_cycles() : 0;
      tick_pending.store(false);
      ++ticks;
      slide_tick();  // P12A fix 1
      if (ho.cursor_live) cursor_tick();
      hover_tick();  // P10I fix 2
      if (wl) winlog_event('T', wl_t0, wl_c0);
      continue;
    }
    if (m.hwnd == hwnd && m.message == LiveSession::kMsgPresent) {
      struct WlScope {  // P16A (SG_P16_PAINTLOG): the frame's hand-over on the window thread
        bool on = winlog_on();
        int64_t t0 = on ? portsim::perf_now_ns() : 0;
        uint64_t c0 = on ? winlog_cycles() : 0;
        ~WlScope() {
          if (on) winlog_event('F', t0, c0);
        }
      } wl_scope;
      uint32_t index = 0;
      BandFill band = BandFill::Black;
      WorldImage wi;  // P8B
      const int64_t wl_k0 = wl_scope.on ? portsim::perf_now_ns() : 0;  // P16A: the parts of F (K take, S set, R recycle, C cursor)
      uint64_t wl_kc = wl_scope.on ? winlog_cycles() : 0;
      if (session.take_frame(px, index, &band, &wi)) {
        if (wl_scope.on) winlog_event('K', wl_k0, wl_kc);
        portsim::GfxTextFrame tf = std::move(wi.text);  // P9C: the frame's text runs (empty without --text-res native)
        if (ho.world) {
          int64_t wl_s0 = wl_scope.on ? portsim::perf_now_ns() : 0;
          wl_kc = wl_scope.on ? winlog_cycles() : 0;
          presenter.set_frame_world(px, band, std::move(wi));  // P8B: with its world layer (none = as before)
          if (wl_scope.on) {
            winlog_event('S', wl_s0, wl_kc);
            wl_s0 = portsim::perf_now_ns();
            wl_kc = winlog_cycles();
          }
          session.recycle_world(std::move(wi));                // P10A: the previous image's buffers
          if (wl_scope.on) winlog_event('R', wl_s0, wl_kc);
        }
        else presenter.set_frame(px, band);  // P7: the band hint (used only with --wide)
        {
          const int64_t wl_x0 = wl_scope.on ? portsim::perf_now_ns() : 0;  // P16A: X text, L slide
          const uint64_t wl_xc = wl_scope.on ? winlog_cycles() : 0;
          presenter.set_text(std::move(tf));    // P9C
          if (wl_scope.on) winlog_event('X', wl_x0, wl_xc);
        }
        win.invalidate();
        {
          const int64_t wl_l0 = wl_scope.on ? portsim::perf_now_ns() : 0;
          const uint64_t wl_lc = wl_scope.on ? winlog_cycles() : 0;
          slide_tick();                          // P12A fix 1: the new frame's slide offset (and the remap's) at once
          if (wl_scope.on) winlog_event('L', wl_l0, wl_lc);
        }
        if (slog && !presenter.world().empty()) {
          const CameraSnap &c = presenter.world().cam;
          const Presenter::SlideStart &st = presenter.slide_start();
          const CursorProj &P = presenter.world().proj;
          std::fprintf(slog, "frame\t%u\t%u\t%d\t%d,%d\t%d\t%d\t%d\t%d,%d\t%.1f,%.1f\t%.1f\t%d,%d\t%d,%d\n", index, presenter.frames_set(),
                       c.ok ? 1 : 0, c.cam_x, c.cam_y, c.zoom, c.rot, st.reason, st.dx, st.dy, st.s0x, st.s0y, st.period_ms, st.ndx,
                       st.ndy, P.ok ? P.tile_x : -1, P.ok ? P.tile_y : -1);
          if (ho.smooth_objects) {  // P12A fix 2: tagged / moving draws per kind (golfer ball shadow walker animal sky resident)
            int32_t t[8], mv[8];
            presenter.frame_objects(t, mv);
            std::fprintf(slog, "objects\t%u\t%d/%d %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d\n", index, t[1], mv[1], t[2], mv[2], t[3], mv[3],
                         t[4], mv[4], t[5], mv[5], t[6], mv[6], t[7], mv[7]);
          }
          std::fflush(slog);
        }
        {
          const int64_t wl_c0 = wl_scope.on ? portsim::perf_now_ns() : 0;
          const uint64_t wl_cc = wl_scope.on ? winlog_cycles() : 0;
          if (ho.cursor_live) cursor_tick();     // P10E: the new frame's preview, at once at the cursor's current tile
          if (wl_scope.on) winlog_event('C', wl_c0, wl_cc);
        }
        if (presenter.hover()) {               // P10I fix 2: learn from the new frame, then show the live cursor's looks
          const int64_t wl_h0 = wl_scope.on ? portsim::perf_now_ns() : 0;  // P16A: H hover
          const uint64_t wl_hc = wl_scope.on ? winlog_cycles() : 0;
          const Point fc = session.frame_cursor();
          presenter.hover_observe(fc.x, fc.y, vp);
          hover_tick();
          if (wl_scope.on) winlog_event('H', wl_h0, wl_hc);
        }
        // development (P10E): SG_P10E_LOG=<file>, one line per present with a projection: index, the frame's tile, the sim's last
        // cursor answer, the window's live point and the tile proj_tile gives for that answer (equal to the frame's tile when the
        // answer was P6_Cursor's point)
        static std::FILE *plog = std::getenv("SG_P10E_LOG") ? std::fopen(std::getenv("SG_P10E_LOG"), "w") : nullptr;
        if (plog && ho.cursor_live && presenter.world().proj.ok && ho.remap) {
          const CursorProj &P = presenter.world().proj;
          int32_t ax = 0, ay = 0, lx = 0, ly = 0, ldx = 0, tx = -1, ty = -1;
          ho.remap->cursor_answer(ax, ay);
          const Point pa = session.plain_answer();  // the last site-0 read (P6_Cursor's among them)
          int32_t px2 = -1, py2 = -1;
          const bool ok2 = proj_tile(P, pa.x, pa.y, px2, py2);
          const bool lv = session.live_cursor(lx, ly, ldx);
          const bool ok = proj_tile(P, ax, ay, tx, ty);
          std::fprintf(plog, "%u\t%d,%d\t%d,%d\t%d\t%d,%d\t%d\t%d,%d\t%s\t%d,%d\t%d,%d\t%s\n", index, P.tile_x, P.tile_y, ax, ay,
                       lv ? 1 : 0, lx, ly, ok ? 1 : 0, tx, ty, ok && tx == P.tile_x && ty == P.tile_y ? "same" : "differs", pa.x,
                       pa.y, px2, py2, ok2 && px2 == P.tile_x && py2 == P.tile_y ? "plain_same" : "plain_differs");
          std::fflush(plog);
        }
        if (shown && index != LiveSession::kBannerIndex) {
          std::fprintf(shown, "%u\t%016llx\n", index, static_cast<unsigned long long>(frame_hash(presenter.frame())));
          std::fflush(shown);
        }
        if (wl_scope.on) {  // P16A: D the previous world image's buffers freed (what recycle_world did not keep)
          const int64_t wl_d0 = portsim::perf_now_ns();
          const uint64_t wl_dc = winlog_cycles();
          { WorldImage gone = std::move(wi); }
          winlog_event('D', wl_d0, wl_dc);
        }
      }
      continue;
    }
    if (m.hwnd == hwnd && m.message == LiveSession::kMsgEnded) {  // the game quit (or failed): close the window
      PostMessageW(hwnd, WM_CLOSE, 0, 0);
      continue;
    }
    TranslateMessage(&m);  // the jgld pumps call TranslateMessage before DispatchMessageA (WM_CHAR from real keys)
    DispatchMessageW(&m);
    const Viewport now = win.viewport();  // WM_DPICHANGED refits it inside the window procedure
    if (now.origin_x != vp.origin_x || now.origin_y != vp.origin_y || now.client_w != vp.client_w || now.client_h != vp.client_h) {
      vp = now;
      session.set_viewport(vp);
    }
  }
  pacer_stop.store(true);  // P10E
  if (pacer.joinable()) pacer.join();
  winlog_close();  // P16A
  session.request_stop();
  sim.join();
  if (log) std::fclose(log);
  if (shown) std::fclose(shown);
  std::string s = "{\"app\":\"simgolf_port\",\"mode\":\"live\",\"install\":\"" + jesc(files.install_root()) + "\",\"data\":\"" +
                  jesc(files.data_root()) + "\"";
  s += profile_json(o.profile);  // S7.12
  s += ",\"error\":\"" + jesc(lo.error) + "\",\"data_va\":" + std::to_string(lo.data_va);
  s += ",\"idle_ms\":" + std::to_string(o.idle_ms) + ",\"pass_ms\":" + std::to_string(o.pass_ms) + ",\"cursor\":\"" + o.cursor + "\"";
  s += ",\"keys\":\"" + o.keys + "\",\"clock_origin\":" + std::to_string(o.clock_origin);
  s += ",\"p5b\":{\"edge_guard\":" + std::to_string(o.edge_guard ? 1 : 0) + ",\"zoom_keys\":\"" + o.zoom_keys +  // P5B
       "\",\"zoom_remaps\":" + std::to_string(win.zoom_remaps()) + ",\"close_prompt\":" + std::to_string(o.close_prompt ? 1 : 0) +
       ",\"resizable\":" + std::to_string(o.resizable ? 1 : 0) + ",\"scale\":\"" + o.scale + "\"}";
  s += ",\"p7\":{\"wide\":" + std::to_string(o.wide ? 1 : 0) + ",\"wide_width\":" + std::to_string(o.wide_width) +
       ",\"wide_fill\":\"" + o.wide_fill + "\"}";  // P7
  s += ",\"p8b\":{\"render_scale\":\"" + jesc(o.render_scale) + "\",\"world_pw\":" + std::to_string(ow.world_pw) +
       ",\"world_ph\":" + std::to_string(ow.world_ph) + ",\"wide_edge\":" + std::to_string(ho.wide_edge ? 1 : 0) + "}";  // P8B
  s += ",\"p6b\":{\"loading_overlay\":" + std::to_string(o.loading_overlay ? 1 : 0) + "}";  // P6B
  {  // P9C: the window's text runs
    const TextResStats *ts = presenter.text_stats();
    s += ",\"p9c\":{\"text_res\":\"" + o.text_res + "\",\"text_aa\":\"" + o.text_aa + "\",\"runs_drawn\":" +
         std::to_string(ts ? ts->runs : 0) + ",\"clipped_runs\":" + std::to_string(ts ? ts->clipped_runs : 0) +
         ",\"clipped_px\":" + std::to_string(ts ? ts->clipped_px : 0) + ",\"failed\":" + std::to_string(ts ? ts->failed : 0) + "}";
  }
  s += ",\"p10a\":{\"fps\":\"" + o.fps + "\",\"overlay\":" + std::to_string(presenter.overlay()) +  // P10A
       ",\"profile_csv\":\"" + jesc(o.profile_csv) + "\",\"profiler\":" + Profiler::get().summary_json() + "}";
  s += ",\"p10e\":{\"paint_rate\":\"" + o.paint_rate + "\",\"live\":" + std::to_string(ho.cursor_live ? 1 : 0) +  // P10E
       ",\"hz\":" + std::to_string(pace_hz) + ",\"ticks\":" + std::to_string(ticks) + ",\"moves\":" + std::to_string(live_moves) + "}";
  {  // P10I fix 2
    const HoverStats hs = presenter.hover_stats();
    s += ",\"p11b\":{\"hud_panel\":\"" + std::string(o.panel_left ? "left" : "right") + "\"}";  // P11B fix 1
    s += ",\"p10i\":{\"hover_rate\":\"" + o.hover_rate + "\",\"line_res\":\"" + std::string(o.line_native ? "native" : "frame") +
         "\",\"frames\":" + std::to_string(hs.frames) + ",\"controls\":" + std::to_string(hs.controls) + ",\"made\":" +
         std::to_string(hs.controls_made) + ",\"resets\":" + std::to_string(hs.resets) + ",\"mispredicts\":" +
         std::to_string(hs.mispredicts) + ",\"ticks\":" + std::to_string(hs.ticks) + ",\"swaps\":" + std::to_string(hs.swaps) +
         ",\"paints\":" + std::to_string(hover_paints) + "}";
  }
  s += ",\"p11d\":{\"undo_keys\":" + std::to_string(o.undo_keys ? 1 : 0) + "}";  // P11D
  s += ",\"p16a\":{\"prio\":" + std::to_string(p16_prio ? 1 : 0) + ",\"pace_forced_hz\":" +  // P16A
       std::to_string(std::getenv("SG_P16_PACE_HZ") ? std::atoi(std::getenv("SG_P16_PACE_HZ")) : 0) + "}";
  {  // P12A fix 1
    const Presenter::SlideStats &ss = presenter.slide_stats();
    std::string mx;
    for (int z = 0; z < 4; ++z)
      mx += (z ? "," : "") + std::string("\"") + std::to_string(1 << z) + "\":[" + std::to_string(ss.max_dx[z]) + "," +
            std::to_string(ss.max_dy[z]) + "]";
    s += ",\"p12a\":{\"smooth_camera\":" + std::to_string(smooth_camera ? 1 : 0) + ",\"frames\":" + std::to_string(ss.frames) +
         ",\"slides\":" + std::to_string(ss.slides) + ",\"moves\":" + std::to_string(ss.moves) + ",\"slide_paints\":" +
         std::to_string(slide_paints) + ",\"snaps_cam\":" + std::to_string(ss.snaps_cam) + ",\"snaps_view\":" +
         std::to_string(ss.snaps_view) + ",\"snaps_far\":" + std::to_string(ss.snaps_far) + ",\"snaps_screen\":" +
         std::to_string(ss.snaps_screen) + ",\"period_ms\":" + std::to_string(ss.period_ms) + ",\"lat_ms\":" + std::to_string(ss.lat_ms) +
         ",\"lat_slides\":" + std::to_string(ss.lat_slides) + ",\"arr_q_ms\":" + std::to_string(ss.arr_q_ms) +
         ",\"arr_slides\":" + std::to_string(ss.arr_slides) + ",\"slid_points\":" +
         std::to_string(ho.remap ? ho.remap->slid() : 0) + ",\"max_step\":{" + mx + "}}";
    const Presenter::ObjectStats &os = presenter.object_stats();  // P12A fix 2
    s += ",\"p12a_objects\":{\"smooth_objects\":" + std::to_string(ho.smooth_objects ? 1 : 0) + ",\"frames\":" +
         std::to_string(os.frames) + ",\"moved_frames\":" + std::to_string(os.moved) + ",\"matched\":" + std::to_string(os.matched) +
         ",\"rejected_far\":" + std::to_string(os.rejected_far) + ",\"rejected_art\":" + std::to_string(os.rejected_art) +
         ",\"redraws\":" + std::to_string(os.redraws) + ",\"redraw_px\":" + std::to_string(os.redraw_px) + ",\"redraw_ms\":" +
         std::to_string(os.redraw_ns / 1e6) + ",\"paints\":" + std::to_string(object_paints) + ",\"moved_by_kind\":{\"golfer\":" +
         std::to_string(os.moved_kind[1]) + ",\"ball\":" + std::to_string(os.moved_kind[2]) + ",\"shadow\":" +
         std::to_string(os.moved_kind[3]) + ",\"walker\":" + std::to_string(os.moved_kind[4]) + ",\"animal\":" +
         std::to_string(os.moved_kind[5]) + ",\"sky\":" + std::to_string(os.moved_kind[6]) + ",\"resident\":" +
         std::to_string(os.moved_kind[7]) + "}";
    int64_t pub_ns = 0, pub_n = 0;  // P12A fix 3: the sim thread's own cost of the published scenes
    portsim::gfx_hr_scene_cost(pub_ns, pub_n);
    s += ",\"object_rate\":" + std::string(o.object_rate < 0 ? "\"auto\"" : std::to_string(o.object_rate)) +
         ",\"object_rate_now\":" + std::to_string(presenter.object_rate_now()) + ",\"publish_ms_per_frame\":" +
         std::to_string(pub_n ? pub_ns / 1e6 / static_cast<double>(pub_n) : 0.0) + ",\"publishes\":" + std::to_string(pub_n) + "}";
  }
  Profiler::get().close_csv();
  s += ",\"p14\":" + presenter.gpu_json();  // P14A
  s += ",\"p15c_objects\":" + presenter.objects_json();  // P15C
  s += ",\"p15\":" + session.world_thread_json();  // P15A
  s += ",\"negctl\":\"" + jesc(o.negctl) + "\",\"input_events\":" + std::to_string(events);
  s += ",\"frames_shown\":" + std::to_string(presenter.frames_set() > 1 ? presenter.frames_set() - 1 : 0);
  s += ",\"saves_refused\":" + std::to_string(files.refused());
  s += ",\"terrain\":\"" + o.terrain + "\",\"terrain_calls\":" + std::to_string(lo.terrain_calls) +
       ",\"terrain_passes\":" + std::to_string(lo.terrain_passes);
  s += ",\"client\":[" + std::to_string(win.client_w()) + "," + std::to_string(win.client_h()) + "],\"dpi\":" + std::to_string(win.dpi());
  s += ",\"session\":" + session.summary_json(lo.res) + "}\n";
  write_file(rec / "session.json", s.data(), s.size());
  if (!lo.error.empty()) std::fprintf(stderr, "%s\n", lo.error.c_str());
  return lo.error.empty() ? 0 : 1;
}

// ------------------------------------------------------------------------------------------------ selftests
struct Tests {
  std::vector<std::pair<std::string, bool>> rows;
  void add(const std::string &n, bool ok) { rows.emplace_back(n, ok); }
  int print() const {
    int bad = 0;
    for (size_t i = 0; i < rows.size(); ++i) {
      std::printf("[%zu/%zu] %s %s\n", i + 1, rows.size(), rows[i].first.c_str(), rows[i].second ? "PASS" : "FAIL");
      bad += rows[i].second ? 0 : 1;
    }
    std::printf("selftest %zu passed %d failed\n", rows.size() - static_cast<size_t>(bad), bad);
    std::fflush(stdout);
    return bad ? 1 : 0;
  }
};

void selftest_pacing(Tests &t) {
  // virtual clock, 1 ms per read (time_model.md section 3): 99 reads per frame (head 1 + wait 98), 8 waitTicks
  {
    VirtualClock c(0x00100000);
    const uint32_t head = c.now();
    const WaitStats s = frame_wait(c, head, 0, nullptr);
    t.add("pacing.frame_87ms_reads_99", s.reads + 1 == 99 && s.wait_ticks == 8 && s.frame_ms == 1);
    t.add("pacing.frame_87ms_ends_at_head_plus_98", s.end == head + 98);
    t.add("pacing.deadline_87", frame_target(head, 0) == head + 87);
  }
  {
    VirtualClock c(5000);
    const uint32_t head = c.now();
    const WaitStats s = frame_wait(c, head, kFlagSlowFrames, nullptr);
    // tests at head + 2 + 12 k; the first at or past head + 125 is k = 11 -> 1 + 1 + 11 * 12 reads in the wait
    t.add("pacing.deadline_125_with_bit17", frame_target(head, kFlagSlowFrames) == head + 125);
    t.add("pacing.frame_125ms_reads_135", s.reads + 1 == 135 && s.wait_ticks == 11);
    t.add("pacing.other_flag_bits_ignored", frame_target(head, ~kFlagSlowFrames) == head + 87);
  }
  {
    VirtualClock c(0);
    WaitStats s;
    const int r = wait_ticks(c, 1, false, nullptr, &s);
    t.add("pacing.waitTicks1_10ms_11_reads", r == 0 && s.reads == 11 && s.end == 10);
    WaitStats z;
    t.add("pacing.waitTicks0_no_read", wait_ticks(c, 0, false, nullptr, &z) == 0 && z.reads == 0);
    t.add("pacing.waitTicks_ms", wait_ticks_ms(1) == 10 && wait_ticks_ms(90) == 900 && wait_ticks_ms(60) == 600 &&
                                     wait_ticks_ms(-1) == -10);
    int pumped = 0;
    VirtualClock c2(0);
    const int ri = wait_ticks(c2, 10, true, [&]() { return ++pumped == 3; });
    t.add("pacing.waitTicks_interruptible_returns_1", ri == 1 && pumped == 3);
  }
  {
    // unsigned compare: a head close to the wrap gives a target below it and no wait (the original's jae / jb)
    VirtualClock c(0xffffffd0u);
    const uint32_t head = c.now();
    const WaitStats s = frame_wait(c, head, 0, nullptr);
    t.add("pacing.wrap_unsigned_no_wait", s.wait_ticks == 0 && s.reads == 2);
    // a frame whose work took longer than 87 ms: one test, no waitTicks
    VirtualClock c3(1000);
    const uint32_t h3 = c3.now();
    for (int i = 0; i < 200; ++i) c3.now();
    const WaitStats s3 = frame_wait(c3, h3, 0, nullptr);
    t.add("pacing.long_frame_single_test", s3.wait_ticks == 0 && s3.reads == 2 && s3.frame_ms == 201);
  }
  {
    // real clock: 20 empty frames back to back, each period >= 87 ms and the mean within one 10 ms quantum (+3 ms jitter)
    RealClock rc;
    uint32_t head = rc.now();
    const uint32_t first = head;
    uint32_t minp = 0xffffffffu, maxp = 0;
    for (int i = 0; i < 20; ++i) {
      frame_wait(rc, head, 0, nullptr);
      const uint32_t next = rc.now();
      minp = std::min(minp, next - head);
      maxp = std::max(maxp, next - head);
      head = next;
    }
    const double mean = (head - first) / 20.0;
    std::printf("pacing real clock: 20 frames, period min %u max %u mean %.1f ms, sleeps %u\n", minp, maxp, mean, rc.sleeps());
    t.add("pacing.real_clock_period_87", minp >= 87 && mean <= 100.0);
  }
}

uint64_t tree_digest(const fs::path &root) {
  // FNV-1a over every file's relative path and bytes, in sorted order
  std::vector<fs::path> files;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) files.push_back(it->path());
  std::sort(files.begin(), files.end());
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](const void *p, size_t n) {
    const uint8_t *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
  };
  for (const fs::path &p : files) {
    const std::string r = fs::relative(p, root, ec).generic_string();
    mix(r.data(), r.size());
    std::vector<uint8_t> b;
    read_file(p.string(), b);
    mix(b.data(), b.size());
  }
  return h;
}

void selftest_saves(Tests &t, SavesDefect defect) {
  char tmp[MAX_PATH];
  GetTempPathA(MAX_PATH, tmp);
  const fs::path base = fs::path(tmp) / ("simgolf_port_selftest_" + std::to_string(GetCurrentProcessId()));
  std::error_code ec;
  fs::remove_all(base, ec);
  const fs::path inst = base / "install", data = base / "data";
  const std::string orig = "original save";
  write_file(inst / "Saved Games" / "a.sve", orig.data(), orig.size());
  write_file(inst / "Saved Games" / "b.sve", "bb", 2);
  write_file(inst / "Themes" / "Default.pro", "pro", 3);
  const uint64_t before = tree_digest(inst);
  {
    AppFiles f(inst.string(), data.string(), defect);
    t.add("saves.opens", f.ok());
    const std::vector<uint8_t> w = {'n', 'e', 'w'};
    t.add("saves.write_ok", f.write("Saved Games\\c.sve", w) && f.write("Themes/Default.pro", w) && f.write("top10.sve", w));
    t.add("saves.write_lands_in_data_dir", fs::exists(data / "files" / "Saved Games" / "c.sve") &&
                                               fs::exists(data / "files" / "top10.sve"));
    std::vector<uint8_t> r(8);
    const int64_t n = f.read("saved games/c.sve", 0, r.data(), r.size());
    t.add("saves.read_back", n == 3 && r[0] == 'n' && r[2] == 'w');
    const int64_t n2 = f.read("Themes\\Default.pro", 0, r.data(), r.size());
    t.add("saves.shadow_install_file", n2 == 3 && r[0] == 'n');
    const int64_t n3 = f.read("Saved Games/a.sve", 0, r.data(), r.size());
    t.add("saves.install_read", n3 == 8 && r[0] == 'o');
    auto l = f.list("Saved Games\\*.sve");
    std::vector<std::string> names;
    for (auto &e : l) names.push_back(e.name);
    t.add("saves.list_merged_sorted", names == std::vector<std::string>({"a.sve", "b.sve", "c.sve"}));
    auto l2 = f.list("Saved Games\\*");
    t.add("saves.list_dot_entries", l2.size() == 5 && l2[0].name == "." && l2[1].name == "..");
    t.add("saves.remove_install_file_tombstone", f.remove("Saved Games/b.sve") && f.size("Saved Games/b.sve") == -1 &&
                                                     fs::exists(inst / "Saved Games" / "b.sve"));
    t.add("saves.remove_data_file", f.remove("Saved Games/c.sve") && !fs::exists(data / "files" / "Saved Games" / "c.sve"));
    t.add("saves.refuse_dotdot", !f.write("../escape.sve", w) && !f.write("Saved Games/../../x", w) &&
                                      !fs::exists(data / "escape.sve") && !fs::exists(base / "x"));
    t.add("saves.refuse_drive_and_stream", !f.write("C:\\x.sve", w) && !f.write("a.sve:stream", w) && !f.write("\\x.sve", w));
    t.add("saves.memory_names_stay_in_memory", f.write("$gfx/back.555", w) && f.size("$gfx/back.555") == 3 &&
                                                   !fs::exists(data / "files" / "$gfx"));
  }
  {
    AppFiles again(inst.string(), data.string(), defect);
    t.add("saves.tombstone_persists", again.size("Saved Games/b.sve") == -1 && again.size("top10.sve") == 3);
  }
  {
    AppFiles inside(inst.string(), (inst / "Saved Games" / "port").string(), defect);
    t.add("saves.refuse_data_inside_install", !inside.ok() && !inside.write("x.sve", {'x'}));
    AppFiles same(inst.string(), inst.string(), defect);
    t.add("saves.refuse_data_equal_install", !same.ok());
  }
  fs::remove_all(inst / "Saved Games" / "port", ec);
  t.add("saves.install_unchanged", tree_digest(inst) == before);
  fs::remove_all(base, ec);
}

void selftest_present(Tests &t, PresentDefect d) {
  // the presenter's mapping is the translator's: every client pixel shows the logical pixel to_logical maps it to
  std::vector<uint16_t> f(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) f[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  Presenter p(d);
  std::vector<uint16_t> black(f.size(), 0);
  p.set_frame(black);
  p.set_frame(f);
  for (auto [cw, ch] : std::vector<std::pair<int, int>>{{800, 600}, {1200, 900}, {2000, 1500}, {1000, 600}, {800, 700}}) {
    std::vector<uint32_t> out;
    Viewport vp;
    p.render(cw, ch, out, &vp);
    Translator tr(vp);
    bool ok = true;
    for (int y = 0; y < ch && ok; ++y)
      for (int x = 0; x < cw && ok; ++x) {
        const Point l = tr.to_logical(Point{x, y});
        const bool in = l.x >= 0 && l.x < kLogicalW && l.y >= 0 && l.y < kLogicalH && x >= vp.origin_x && y >= vp.origin_y &&
                        x < vp.origin_x + vp.client_w && y < vp.origin_y + vp.client_h;
        const uint32_t want = in ? rgb555_to_bgra(f[static_cast<size_t>(l.y) * kLogicalW + l.x]) : 0xff000000u;
        ok = out[static_cast<size_t>(y) * cw + x] == want;
      }
    t.add("present.mapping_" + std::to_string(cw) + "x" + std::to_string(ch), ok);
  }
  const Viewport a = fit_viewport(1000, 600), b = fit_viewport(800, 700);
  t.add("present.letterbox", a.origin_x == 100 && a.client_w == 800 && b.origin_y == 50 && b.client_h == 600);
  t.add("present.rgb555_expand", rgb555_to_bgra(0x7fff) == 0xffffffffu && rgb555_to_bgra(0x0421) == 0xff080808u &&
                                     rgb555_to_bgra(0x7c00) == 0xffff0000u);
}

// P5B item 4 (--selftest-p5b; separate so --selftest keeps its 37 rows): the presenter's scale modes
void selftest_p5b(Tests &t, bool wrong) {
  std::vector<uint16_t> f(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) f[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  auto render = [&](ScaleMode m, int cw, int ch, const std::vector<uint16_t> &fr) {
    Presenter p(PresentDefect::None, wrong && m == ScaleMode::Smooth ? ScaleMode::Nearest : m);  // control: smooth ignored
    p.set_frame(fr);
    std::vector<uint32_t> out;
    p.render(cw, ch, out);
    return out;
  };
  std::vector<uint16_t> flat(f.size(), 0x1234);
  {
    const auto o = render(ScaleMode::Smooth, 1200, 900, flat);
    bool ok = true;
    for (uint32_t v : o) ok = ok && v == rgb555_to_bgra(0x1234);
    t.add("p5b.smooth_uniform_frame_stays_uniform", ok);
  }
  t.add("p5b.smooth_at_1x_equals_nearest", render(ScaleMode::Smooth, 800, 600, f) == render(ScaleMode::Nearest, 800, 600, f));
  t.add("p5b.auto_at_2x_is_nearest", render(ScaleMode::Auto, 1600, 1200, f) == render(ScaleMode::Nearest, 1600, 1200, f));
  const auto a15 = render(ScaleMode::Auto, 1200, 900, f), s15 = render(ScaleMode::Smooth, 1200, 900, f),
             n15 = render(ScaleMode::Nearest, 1200, 900, f);
  t.add("p5b.auto_at_1_5x_is_smooth", a15 == s15 && s15 != n15);
  {  // column 0 black, column 1 white: client column 1 of a 2x smooth image lies between them (weight 64 / 256 of white)
    std::vector<uint16_t> g(f.size(), 0);
    for (int y = 0; y < kLogicalH; ++y) g[static_cast<size_t>(y) * kLogicalW + 1] = 0x7fff;
    const auto o = render(ScaleMode::Smooth, 1600, 1200, g);
    const uint32_t v = o[static_cast<size_t>(600) * 1600 + 1] & 0xff;
    t.add("p5b.smooth_blends_neighbours", v == 63);
  }
  {  // letterbox bars stay black in smooth mode
    const auto o = render(ScaleMode::Smooth, 1300, 900, flat);
    t.add("p5b.smooth_letterbox_bars_black", o[0] == 0xff000000u && o[static_cast<size_t>(899) * 1300 + 1299] == 0xff000000u &&
                                                o[static_cast<size_t>(450) * 1300 + 650] == rgb555_to_bgra(0x1234));
  }
}

// P7 (--selftest-p7): the wide side bands (present.h WideFill). Control --negctl p7-band-black: every frame's hint is Black,
// so the fill rows read RED.
void selftest_p7(Tests &t, bool wrong) {
  std::vector<uint16_t> f(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) f[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  auto render = [&](WideFill w, BandFill b, ScaleMode m, int cw, int ch, const std::vector<uint16_t> &fr, Viewport *vp = nullptr) {
    Presenter p(PresentDefect::None, m);
    p.set_wide(w);
    p.set_frame(fr, wrong ? BandFill::Black : b);
    std::vector<uint32_t> out;
    p.render(cw, ch, out, vp);
    return out;
  };
  const int cw = 1920, ch = 1080;
  const auto off = render(WideFill::Off, BandFill::Extend, ScaleMode::Smooth, cw, ch, f);
  {
    Presenter p(PresentDefect::None, ScaleMode::Smooth);  // the pre-P7 presenter (no set_wide, no hint)
    p.set_frame(f);
    std::vector<uint32_t> o0;
    p.render(cw, ch, o0);
    t.add("p7.wide_off_equals_pre_p7_render", o0 == off);
  }
  t.add("p7.black_hint_equals_wide_off", render(WideFill::Soft, BandFill::Black, ScaleMode::Smooth, cw, ch, f) == off);
  Viewport vp;
  const auto soft = render(WideFill::Soft, BandFill::Extend, ScaleMode::Smooth, cw, ch, f, &vp);
  {
    bool same = true, band_lit = false;
    for (int y = 0; y < ch; ++y)
      for (int x = 0; x < cw; ++x) {
        const size_t i = static_cast<size_t>(y) * cw + x;
        const bool inside = x >= vp.origin_x && x < vp.origin_x + vp.client_w;
        if (inside)
          same = same && soft[i] == off[i];
        else
          band_lit = band_lit || soft[i] != 0xff000000u;
      }
    t.add("p7.content_pixels_untouched", same);
    t.add("p7.extend_lights_the_bands", band_lit);
  }
  std::vector<uint16_t> flat(f.size(), 0x2a55);
  for (WideFill w : {WideFill::Soft, WideFill::Clamp}) {
    const auto o = render(w, BandFill::Extend, ScaleMode::Nearest, 1067, 600, flat);
    bool ok = true;
    for (uint32_t v : o) ok = ok && v == rgb555_to_bgra(0x2a55);
    t.add(w == WideFill::Soft ? "p7.soft_uniform_frame_fills_uniform" : "p7.clamp_uniform_frame_fills_uniform", ok);
  }
  {  // clamp at 1x: band pixel (x, y) = the frame's edge pixel of row y
    const auto o = render(WideFill::Clamp, BandFill::Extend, ScaleMode::Nearest, 1067, 600, f);
    bool ok = true;
    const int ox = (1067 - 800) / 2;
    for (int y = 0; y < 600; ++y) {
      ok = ok && o[static_cast<size_t>(y) * 1067 + 0] == rgb555_to_bgra(f[static_cast<size_t>(y) * 800]);
      ok = ok && o[static_cast<size_t>(y) * 1067 + ox - 1] == rgb555_to_bgra(f[static_cast<size_t>(y) * 800]);
      ok = ok && o[static_cast<size_t>(y) * 1067 + 1066] == rgb555_to_bgra(f[static_cast<size_t>(y) * 800 + 799]);
    }
    t.add("p7.clamp_repeats_edge_column", ok);
  }
  {  // soft: a one-row white line on black reaches the band unblurred next to the frame, spread and dimmer far out
    std::vector<uint16_t> g(f.size(), 0);
    for (int x = 0; x < 800; ++x) g[static_cast<size_t>(300) * 800 + x] = 0x7fff;
    const auto o = render(WideFill::Soft, BandFill::Extend, ScaleMode::Nearest, 1067, 600, g);
    const uint32_t near_line = o[static_cast<size_t>(300) * 1067 + 132] & 0xff, far_line = o[static_cast<size_t>(300) * 1067 + 0] & 0xff,
                   far_above = o[static_cast<size_t>(290) * 1067 + 0] & 0xff;
    t.add("p7.soft_blur_grows_with_distance", near_line == 255 && far_line > 0 && far_line < 64 && far_above > 0);
  }
  {  // a client taller than 4:3: the letterbox rows stay black
    const auto o = render(WideFill::Soft, BandFill::Extend, ScaleMode::Nearest, 800, 900, flat);
    t.add("p7.letterbox_rows_stay_black", o[0] == 0xff000000u && o[static_cast<size_t>(899) * 800 + 799] == 0xff000000u &&
                                              o[static_cast<size_t>(450) * 800 + 400] == rgb555_to_bgra(0x2a55));
  }
}

// P8B (--selftest-p8b): the world layer under the frame (present.h WorldImage, Presenter::compose_world). Control
// --negctl p8b-mask-ignored (PresentDefect::WorldMaskIgnored): the world shows over the whole content rectangle, so the rows that
// need the frame's own pixels read RED. Control --negctl p8b-hud-unmoved (PresentDefect::HudUnmoved, G32): the HUD compositor off,
// so the three hud rows read RED.
// P9A item 5 (--selftest-p9a): the Edge band fill (present.h WideFill::Edge). Control --negctl p9a-band-soft renders P7's Soft
// in Edge's place: the cut-shape row reads RED.
void selftest_p9a(Tests &t, bool wrong) {
  const int cw = 1920, ch = 1080;
  auto render = [&](WideFill w, const std::vector<uint16_t> &fr, Viewport *vp = nullptr) {
    Presenter p(PresentDefect::None, ScaleMode::Smooth);
    p.set_wide(wrong && w == WideFill::Edge ? WideFill::Soft : w);
    p.set_frame(fr, BandFill::Extend);
    std::vector<uint32_t> out;
    p.render(cw, ch, out, vp);
    return out;
  };
  // a light background with a dark 6-row shape cut by the frame's right edge (the last 3 logical columns), as the Jamaica pin
  std::vector<uint16_t> bg(static_cast<size_t>(kLogicalW) * kLogicalH, static_cast<uint16_t>((26 << 10) | (26 << 5) | 30));
  std::vector<uint16_t> cut = bg;
  for (int y = 300; y < 306; ++y)
    for (int x = kLogicalW - 3; x < kLogicalW; ++x) cut[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>(4 << 5);
  Viewport vp;
  const auto a = render(WideFill::Edge, bg, &vp), b = render(WideFill::Edge, cut);
  const int32_t right = vp.origin_x + vp.client_w, fade = vp.client_w * 24 / kLogicalW;
  bool uniform = true, far_same = true, near_differs = false;
  for (int y = 0; y < ch; ++y)
    for (int x = right; x < cw; ++x) {
      const size_t i = static_cast<size_t>(y) * cw + x;
      if (a[i] != a[static_cast<size_t>(vp.origin_y) * cw + right]) uniform = false;
      if (x >= right + fade && a[i] != b[i]) far_same = false;
      if (x < right + fade / 2 && a[i] != b[i]) near_differs = true;
    }
  t.add("p9a.edge_uniform_frame_fills_uniform", uniform);
  t.add("p9a.edge_cut_shape_not_carried_past_the_fade", far_same);
  t.add("p9a.edge_cut_shape_shown_near_the_frame", near_differs);
  // the content rectangle is the same pixels under every band mode
  const auto off = render(WideFill::Off, cut);
  bool content = true;
  for (int y = 0; y < ch; ++y)
    for (int x = vp.origin_x; x < right; ++x)
      if (off[static_cast<size_t>(y) * cw + x] != b[static_cast<size_t>(y) * cw + x]) content = false;
  t.add("p9a.content_unchanged_by_the_band", content);
  Options d;
  t.add("p9a.default_wide_fill_is_edge", wide_fill(d) == WideFill::Edge);
}

void selftest_p8b(Tests &t, bool wrong, bool hud_wrong = false) {
  const PresentDefect pd = hud_wrong ? PresentDefect::HudUnmoved : wrong ? PresentDefect::WorldMaskIgnored : PresentDefect::None;
  std::vector<uint16_t> f(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) f[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  auto world_of = [&](int pw, int ph, uint8_t m) {
    WorldImage w;
    w.pw = pw;
    w.ph = ph;
    w.fw = static_cast<int32_t>(static_cast<int64_t>(ph) * kLogicalW / kLogicalH);
    w.ox = (pw - w.fw) / 2;
    w.px.resize(static_cast<size_t>(pw) * ph);
    for (int y = 0; y < ph; ++y)
      for (int x = 0; x < pw; ++x) w.px[static_cast<size_t>(y) * pw + x] = static_cast<uint16_t>((x * 13 + y * 29 + 5) & 0x7fff);
    w.mask.assign(static_cast<size_t>(kLogicalW) * kLogicalH, m);
    return w;
  };
  auto render = [&](ScaleMode m, int cw, int ch, WorldImage *w, WideFill wf = WideFill::Off) {
    Presenter p(pd, m);
    p.set_wide(wf);
    if (w) p.set_frame_world(f, BandFill::Extend, std::move(*w));
    else p.set_frame(f, BandFill::Extend);
    std::vector<uint32_t> out;
    p.render(cw, ch, out);
    return out;
  };
  {  // an empty world layer = set_frame (the pre-P8B image), also with P7's bands
    WorldImage none;
    t.add("p8b.empty_world_equals_set_frame", render(ScaleMode::Smooth, 1920, 1080, &none, WideFill::Soft) ==
                                                 render(ScaleMode::Smooth, 1920, 1080, nullptr, WideFill::Soft));
  }
  {  // a world made at the client's size, mask all 1: the client image is the world, pixel for pixel
    for (auto sz : {std::pair<int, int>{1067, 600}, {1920, 1080}, {2560, 1440}}) {
      WorldImage w = world_of(sz.first, sz.second, 1);
      const std::vector<uint16_t> wp = w.px;
      const auto o = render(ScaleMode::Smooth, sz.first, sz.second, &w);
      bool ok = true;
      for (size_t i = 0; i < wp.size(); ++i) ok = ok && o[i] == rgb555_to_bgra(wp[i]);
      t.add("p8b.world_at_client_size_is_identity_" + std::to_string(sz.first), ok);
    }
  }
  {  // mask all 0: the content rectangle is the frame (as without a world), the side columns the world
    WorldImage w = world_of(1067, 600, 0);
    const std::vector<uint16_t> wp = w.px;
    const auto o = render(ScaleMode::Nearest, 1067, 600, &w);
    const auto plain = render(ScaleMode::Nearest, 1067, 600, nullptr);
    bool frame_ok = true, side_ok = true;
    for (int y = 0; y < 600; ++y)
      for (int x = 0; x < 1067; ++x) {
        const size_t i = static_cast<size_t>(y) * 1067 + x;
        if (x >= 133 && x < 933) frame_ok = frame_ok && o[i] == plain[i];
        else side_ok = side_ok && o[i] == rgb555_to_bgra(wp[i]);
      }
    t.add("p8b.mask0_content_is_the_frame", frame_ok);
    t.add("p8b.side_columns_are_the_world", side_ok);
  }
  {  // a mixed mask: one logical pixel of the frame kept, its neighbour the world (render scale 1, 1067 x 600)
    WorldImage w = world_of(1067, 600, 1);
    w.mask[static_cast<size_t>(300) * 800 + 400] = 0;
    const std::vector<uint16_t> wp = w.px;
    const auto o = render(ScaleMode::Nearest, 1067, 600, &w);
    t.add("p8b.mask_selects_per_logical_pixel", o[static_cast<size_t>(300) * 1067 + 533] == rgb555_to_bgra(f[static_cast<size_t>(300) * 800 + 400]) &&
                                                  o[static_cast<size_t>(300) * 1067 + 534] == rgb555_to_bgra(wp[static_cast<size_t>(300) * 1067 + 534]));
  }
  {  // a world of 1067 x 600 shown at 2134 x 1200, nearest: each world pixel covers 2 x 2 client pixels
    WorldImage w = world_of(1067, 600, 1);
    const std::vector<uint16_t> wp = w.px;
    const auto o = render(ScaleMode::Nearest, 2134, 1200, &w);
    const Viewport vp = fit_viewport(2134, 1200);  // content 1600 x 1200 at x 267: world x = floor((x - 267) / 2) + 133
    bool ok = true;
    for (int y = 0; y < 1200; y += 7)
      for (int x = 0; x < 2134; x += 5) {
        const int d = x - vp.origin_x;
        int wx = (d >= 0 ? d / 2 : -((-d + 1) / 2)) + 133;
        wx = std::min(std::max(wx, 0), 1066);
        ok = ok && o[static_cast<size_t>(y) * 2134 + x] == rgb555_to_bgra(wp[static_cast<size_t>(y / 2) * 1067 + wx]);
      }
    t.add("p8b.scaled_world_nearest_2x", ok);
  }
  {  // G32: the HUD compositor. Mask 0 (2D) only on two pixels: one in the title panel (left anchor), one in the meters (right)
    WorldImage w = world_of(1067, 600, 1);
    w.mask[static_cast<size_t>(40) * 800 + 100] = 0;   // title panel (0..250, 0..95)
    w.mask[static_cast<size_t>(40) * 800 + 700] = 0;   // meters (630..800, 0..145)
    const std::vector<uint16_t> wp = w.px;
    PanelRemap rm;
    Presenter p(pd, ScaleMode::Nearest);
    p.set_hud_anchor(true);
    p.set_remap(&rm);
    p.set_frame_world(f, BandFill::Black, std::move(w));
    std::vector<uint32_t> o;
    p.render(1067, 600, o);
    const uint32_t f100 = rgb555_to_bgra(f[static_cast<size_t>(40) * 800 + 100]), f700 = rgb555_to_bgra(f[static_cast<size_t>(40) * 800 + 700]);
    // origin 133, right band 134: the title pixel at client 100 (133 + 100 - 133), the meter pixel at 967 (133 + 700 + 134)
    t.add("p8b.hud_panels_at_the_edges", o[static_cast<size_t>(40) * 1067 + 100] == f100 && o[static_cast<size_t>(40) * 1067 + 967] == f700);
    t.add("p8b.hud_vacated_place_shows_the_world", o[static_cast<size_t>(40) * 1067 + 233] == rgb555_to_bgra(wp[static_cast<size_t>(40) * 1067 + 233]) &&
                                                     o[static_cast<size_t>(40) * 1067 + 833] == rgb555_to_bgra(wp[static_cast<size_t>(40) * 1067 + 833]));
    int32_t x1 = 100, y1 = 40, x2 = 967, y2 = 40, x3 = 500, y3 = 300;
    rm.map(x1, y1);
    rm.map(x2, y2);
    rm.map(x3, y3);
    t.add("p8b.hud_remap_to_800_space", x1 == 233 && x2 == 833 && x3 == 500);
  }
}

// P9B (--selftest-p9b): the layered HUD compositor (present.h WorldImage::hud, Presenter::compose_layers, PanelRemap::set_layers)
// on a synthetic frame: a base, a world, a left-anchored layer with opaque, mask-blend and shade pixels, a right-anchored layer and
// a centred layer drawn after them. Controls: --negctl p9b-blend-opaque (PresentDefect::LayerBlendOpaque: blended pixels keep the
// words they had in the frame) reads the two blend rows RED; --negctl p9b-rect-move (PresentDefect::LayerRectMove: G32's rectangle
// compositor although the frame has layers) reads the layer rows RED.
void selftest_p9b(Tests &t, PresentDefect pd) {
  const int cw = 1067, ch = 600;  // render scale 1: origin 133, right band 134
  std::vector<uint16_t> base(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) base[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  // the test's own copies of the two blend rules (gfx.cpp b_stretch_blit's channel rule, the shade table lookup)
  auto blend = [](uint16_t w, uint16_t sw, uint32_t k) {
    auto chn = [&](uint32_t dv, uint32_t sv) { return static_cast<uint16_t>(static_cast<uint16_t>(((dv * k) >> 8) + sv) >> 3); };
    const uint16_t rr = static_cast<uint16_t>(chn((w >> 7) & 0xf8u, (sw >> 7) & 0xf8u) << 10);
    const uint16_t gg = static_cast<uint16_t>(chn((w >> 2) & 0xf8u, (sw >> 2) & 0xf8u) << 5);
    const uint16_t bb = chn((static_cast<uint32_t>(w) << 3) & 0xf8u, (static_cast<uint32_t>(sw) << 3) & 0xf8u);
    return static_cast<uint16_t>((rr | gg | bb) & 0x7fff);
  };
  size_t lut_n = 0;
  const uint16_t *lut = portsim::gfx_shade_table(0, lut_n);
  auto shade = [&](uint16_t w) { return static_cast<uint16_t>(lut && w < lut_n ? lut[w] & 0x7fff : w); };
  auto px_of = [](uint8_t kind, uint16_t px, uint16_t src = 0, uint8_t k = 0) {
    portsim::GfxLayerPx p;
    p.kind = kind;
    p.px = px;
    p.src = src;
    p.k = k;
    return p;
  };
  // layer 1 (bottom band, left): opaque 3 x 3 at (100..102, 499..501), a mask blend at (104, 500), a shade at (105, 500)
  portsim::GfxHudLayer l1;
  l1.id = portsim::kGfxLayerBottom;
  l1.anchor = -1;
  l1.x0 = 100, l1.y0 = 499, l1.x1 = 106, l1.y1 = 502;
  l1.px.assign(6 * 3, portsim::GfxLayerPx{});
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x) l1.px[static_cast<size_t>(y) * 6 + x] = px_of(1, 0x03e0);
  l1.px[static_cast<size_t>(1) * 6 + 4] = px_of(2, 0, 0x7c00, 0x80);
  l1.px[static_cast<size_t>(1) * 6 + 5] = px_of(3, 0, 0, 0);
  // layer 2 (meters, right): one opaque pixel at (700, 40)
  portsim::GfxHudLayer l2;
  l2.id = portsim::kGfxLayerMeters;
  l2.anchor = 1;
  l2.x0 = 700, l2.y0 = 40, l2.x1 = 701, l2.y1 = 41;
  l2.px.assign(1, px_of(1, 0x001f));
  // layer 3 (centred, drawn after): one opaque pixel at (100, 500), over layer 1's place in the frame
  portsim::GfxHudLayer l3;
  l3.id = portsim::kGfxLayerCentred;
  l3.anchor = 0;
  l3.x0 = 100, l3.y0 = 500, l3.x1 = 101, l3.y1 = 501;
  l3.px.assign(1, px_of(1, 0x7fff));
  // the frame the layers compose to in place (the back buffer): base, then each layer over it
  std::vector<uint16_t> back = base;
  for (const portsim::GfxHudLayer *l : {&l1, &l2, &l3})
    for (int y = l->y0; y < l->y1; ++y)
      for (int x = l->x0; x < l->x1; ++x) {
        const portsim::GfxLayerPx *p = l->at(x, y);
        if (!p) continue;
        uint16_t &d = back[static_cast<size_t>(y) * kLogicalW + x];
        d = p->kind == 1 ? p->px : p->kind == 2 ? blend(d, p->src, p->k) : shade(d);
      }
  // l1's blended pixels in the frame hold their in-place words (what a capture records)
  l1.px[static_cast<size_t>(1) * 6 + 4].px = back[static_cast<size_t>(500) * kLogicalW + 104];
  l1.px[static_cast<size_t>(1) * 6 + 5].px = back[static_cast<size_t>(500) * kLogicalW + 105];
  auto world_of = [&](uint8_t m) {
    WorldImage w;
    w.pw = cw;
    w.ph = ch;
    w.fw = 800;
    w.ox = 133;
    w.px.resize(static_cast<size_t>(cw) * ch);
    for (int y = 0; y < ch; ++y)
      for (int x = 0; x < cw; ++x) w.px[static_cast<size_t>(y) * cw + x] = static_cast<uint16_t>((x * 13 + y * 29 + 5) & 0x7fff);
    w.mask.assign(static_cast<size_t>(kLogicalW) * kLogicalH, m);
    w.hud.base = base;
    w.hud.back = back;
    w.hud.layers = {l1, l2, l3};
    w.hud.serial = 1;
    return w;
  };
  auto wpx = [&](int x, int y) { return static_cast<uint16_t>((x * 13 + y * 29 + 5) & 0x7fff); };
  PanelRemap rm;
  std::vector<uint32_t> o;
  {
    Presenter p(pd, ScaleMode::Nearest);
    p.set_hud_anchor(true);
    p.set_remap(&rm);
    WorldImage w = world_of(1);
    p.set_frame_world(back, BandFill::Black, std::move(w));
    p.render(cw, ch, o);
  }
  auto at = [&](int x, int y) { return o[static_cast<size_t>(y) * cw + x]; };
  // client x of logical x moved left: 133 + x - 133 = x; right: x + 133 + 134
  t.add("p9b.left_layer_at_its_anchor", at(100, 500) == rgb555_to_bgra(0x03e0) && at(102, 501) == rgb555_to_bgra(0x03e0));
  t.add("p9b.right_layer_at_its_anchor", at(967, 40) == rgb555_to_bgra(0x001f));
  t.add("p9b.vacated_place_shows_the_world", at(234, 500) == rgb555_to_bgra(wpx(234, 500)));
  t.add("p9b.transparent_layer_pixel_shows_what_is_under", at(103, 500) == rgb555_to_bgra(wpx(103, 500)));
  t.add("p9b.blend_over_the_new_place", at(104, 500) == rgb555_to_bgra(blend(wpx(104, 500), 0x7c00, 0x80)));
  t.add("p9b.shade_over_the_new_place", at(105, 500) == rgb555_to_bgra(shade(wpx(105, 500))));
  t.add("p9b.centred_layer_stays_in_the_frame", at(233, 500) == rgb555_to_bgra(0x7fff));
  {
    int32_t x1 = 100, y1 = 500, x2 = 103, y2 = 500, x3 = 967, y3 = 40, x4 = 233, y4 = 500;
    rm.map(x1, y1);
    rm.map(x2, y2);
    rm.map(x3, y3);
    rm.map(x4, y4);
    t.add("p9b.remap_by_layer_coverage", x1 == 233 && x2 == 103 && x3 == 833 && x4 == 233);
  }
  {  // anchor off, world mask 0: the layers over their base rebuild the frame exactly
    Presenter p(pd, ScaleMode::Nearest);
    p.set_hud_anchor(false);
    WorldImage w = world_of(0);
    p.set_frame_world(back, BandFill::Black, std::move(w));
    std::vector<uint32_t> a;
    p.render(cw, ch, a);
    bool ok = true;
    for (int y = 0; y < ch && ok; ++y)
      for (int x = 133; x < 933; ++x) ok = ok && a[static_cast<size_t>(y) * cw + x] == rgb555_to_bgra(back[static_cast<size_t>(y) * kLogicalW + x - 133]);
    t.add("p9b.layers_in_place_rebuild_the_frame", ok);
  }
  {  // smooth at 1920 x 1080 (scale 1.8): the layer's opaque block gets a blended edge (a value between it and what is under)
    Presenter p(pd, ScaleMode::Smooth);
    p.set_hud_anchor(true);
    WorldImage w = world_of(1);
    w.pw = 1920, w.ph = 1080, w.fw = 1440, w.ox = 240;
    w.px.assign(static_cast<size_t>(1920) * 1080, 0);  // a black world
    p.set_frame_world(back, BandFill::Black, std::move(w));
    std::vector<uint32_t> a;
    p.render(1920, 1080, a);
    // logical (100, 500) moved left by 240: client x = 100 * 1.8 = 180; the left edge of the block lies near x 180
    bool mid = false;
    const uint32_t g = rgb555_to_bgra(0x03e0) & 0xffffff;
    for (int x = 170; x < 190; ++x) {
      const uint32_t v = a[static_cast<size_t>(900) * 1920 + x] & 0xffffff;
      mid = mid || (v != 0 && v != g);
    }
    t.add("p9b.smooth_layer_edges", mid);
  }
}

// P8B: a client image (BGRA words, rows top-down) as a 24-bit BMP
bool write_bmp(const fs::path &path, const std::vector<uint32_t> &px, int cw, int ch) {
  if (cw <= 0 || ch <= 0 || px.size() != static_cast<size_t>(cw) * static_cast<size_t>(ch)) return false;
  const uint32_t stride = (static_cast<uint32_t>(cw) * 3 + 3) & ~3u, img = stride * static_cast<uint32_t>(ch);
  std::vector<uint8_t> bmp(54 + img, 0);
  auto put32 = [&](size_t at, uint32_t v) { std::memcpy(bmp.data() + at, &v, 4); };
  bmp[0] = 'B';
  bmp[1] = 'M';
  put32(2, 54 + img);
  put32(10, 54);
  put32(14, 40);
  put32(18, static_cast<uint32_t>(cw));
  put32(22, static_cast<uint32_t>(ch));
  bmp[26] = 1;
  bmp[28] = 24;
  put32(34, img);
  for (int y = 0; y < ch; ++y) {
    uint8_t *row = bmp.data() + 54 + static_cast<size_t>(ch - 1 - y) * stride;
    for (int x = 0; x < cw; ++x) {
      const uint32_t v = px[static_cast<size_t>(y) * static_cast<size_t>(cw) + static_cast<size_t>(x)];
      row[x * 3] = static_cast<uint8_t>(v);
      row[x * 3 + 1] = static_cast<uint8_t>(v >> 8);
      row[x * 3 + 2] = static_cast<uint8_t>(v >> 16);
    }
  }
  return write_file(path, bmp.data(), bmp.size());
}

int run_compose(const Options &o) {  // P7: --compose <in.555> <cw> <ch> <out.bmp>
  std::vector<uint8_t> raw;
  if (!read_file(o.compose[0], raw) || raw.size() != static_cast<size_t>(kLogicalW) * kLogicalH * 2) {
    std::fprintf(stderr, "compose: %s is not an 800x600 RGB555 frame\n", o.compose[0].c_str());
    return 2;
  }
  const int cw = std::atoi(o.compose[1].c_str()), ch = std::atoi(o.compose[2].c_str());
  if (cw <= 0 || ch <= 0 || cw > 8192 || ch > 8192) return 2;
  std::vector<uint16_t> px(raw.size() / 2);
  std::memcpy(px.data(), raw.data(), raw.size());
  Presenter p(PresentDefect::None, scale_mode(o.scale));
  p.set_wide(wide_fill(o));
  p.set_frame(px, o.compose_band == "black" ? BandFill::Black : o.compose_band == "map" ? BandFill::ExtendClamp : BandFill::Extend);  // P13B: map
  std::vector<uint32_t> out;
  p.render(cw, ch, out);
  const uint32_t stride = (static_cast<uint32_t>(cw) * 3 + 3) & ~3u, img = stride * static_cast<uint32_t>(ch);
  std::vector<uint8_t> bmp(54 + img, 0);
  auto put32 = [&](size_t at, uint32_t v) { std::memcpy(bmp.data() + at, &v, 4); };
  bmp[0] = 'B';
  bmp[1] = 'M';
  put32(2, 54 + img);
  put32(10, 54);
  put32(14, 40);
  put32(18, static_cast<uint32_t>(cw));
  put32(22, static_cast<uint32_t>(ch));
  bmp[26] = 1;
  bmp[28] = 24;
  put32(34, img);
  for (int y = 0; y < ch; ++y) {
    uint8_t *row = bmp.data() + 54 + static_cast<size_t>(ch - 1 - y) * stride;
    for (int x = 0; x < cw; ++x) {
      const uint32_t v = out[static_cast<size_t>(y) * cw + x];
      row[x * 3] = static_cast<uint8_t>(v & 255);
      row[x * 3 + 1] = static_cast<uint8_t>((v >> 8) & 255);
      row[x * 3 + 2] = static_cast<uint8_t>((v >> 16) & 255);
    }
  }
  if (!write_file(o.compose[3], bmp.data(), bmp.size())) return 3;
  std::printf("composed %dx%d %s\n", cw, ch, o.compose[3].c_str());
  return 0;
}

// P9D (--selftest-p9d): the modal screens over the HUD layers (present.cpp compose_layers: GfxLayerPx kind 4, GfxHudLayer::dim)
// on a synthetic frame at render scale 1 (1067 x 600, origin 133, right band 134): a world, a left-anchored panel pixel, a
// right-anchored panel pixel, then a centred layer that dimmed the whole frame (kind 4 everywhere, source word 0x0421) with one
// opaque report pixel drawn after the dim. Control --negctl p9d-dim-local (PresentDefect::DimLocal: the dim composed per pixel at
// its own place) reads the rows of the pixels outside the dim layer's place RED (the moved panels and the side band).
void selftest_p9d(Tests &t, PresentDefect pd) {
  const int cw = 1067, ch = 600;
  // the test's own quarter rule: floor((src + 3 dst) / 4) per 5-bit channel (gfx_reports.cpp quarter_blend, draw16_1002ed60)
  auto q = [](uint16_t s, uint16_t d) {
    uint32_t o = 0;
    for (int sh : {10, 5, 0}) o |= ((((s >> sh) & 31u) + 3u * ((d >> sh) & 31u)) / 4u) << sh;
    return static_cast<uint16_t>(o);
  };
  bool rule = true;
  for (uint32_t s = 0; s < 0x8000; s += 0x0123)
    for (uint32_t d = 0; d < 0x8000; d += 0x0211)
      rule = rule && portsim::gfx_layer_quarter(static_cast<uint16_t>(s), static_cast<uint16_t>(d)) ==
                         q(static_cast<uint16_t>(s), static_cast<uint16_t>(d));
  t.add("p9d.quarter_rule_per_channel", rule);
  const uint16_t P = 0x0421, A = 0x03e0, B = 0x001f, C = 0x7fff;
  auto lp = [](uint8_t kind, uint16_t px, uint16_t src) {
    portsim::GfxLayerPx p;
    p.kind = kind;
    p.px = px;
    p.src = src;
    return p;
  };
  std::vector<uint16_t> base(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x) base[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>((x * 7 + y * 131) & 0x7fff);
  portsim::GfxHudLayer l1;  // the bottom band (left): one opaque pixel at (100, 500)
  l1.id = portsim::kGfxLayerBottom;
  l1.anchor = -1;
  l1.x0 = 100, l1.y0 = 500, l1.x1 = 101, l1.y1 = 501;
  l1.px.assign(1, lp(1, A, 0));
  portsim::GfxHudLayer l2;  // the meters (right): one opaque pixel at (700, 40)
  l2.id = portsim::kGfxLayerMeters;
  l2.anchor = 1;
  l2.x0 = 700, l2.y0 = 40, l2.x1 = 701, l2.y1 = 41;
  l2.px.assign(1, lp(1, B, 0));
  portsim::GfxHudLayer l3;  // the report: the whole-frame dim, then the report pixel at (400, 300)
  l3.id = portsim::kGfxLayerCentred;
  l3.anchor = 0;
  l3.x0 = 0, l3.y0 = 0, l3.x1 = kLogicalW, l3.y1 = kLogicalH;
  l3.dim = 1;
  l3.dim_src = P;
  l3.px.assign(static_cast<size_t>(kLogicalW) * kLogicalH, lp(4, 0, P));
  std::vector<uint16_t> back = base;  // the frame in place: the panels, the dim, the report pixel
  back[static_cast<size_t>(500) * kLogicalW + 100] = A;
  back[static_cast<size_t>(40) * kLogicalW + 700] = B;
  for (size_t i = 0; i < back.size(); ++i) {
    back[i] = q(P, back[i]);
    l3.px[i].px = back[i];
  }
  back[static_cast<size_t>(300) * kLogicalW + 400] = C;
  l3.px[static_cast<size_t>(300) * kLogicalW + 400] = lp(1, C, 0);
  auto wpx = [](int x, int y) { return static_cast<uint16_t>((x * 13 + y * 29 + 5) & 0x7fff); };
  WorldImage w;
  w.pw = cw;
  w.ph = ch;
  w.fw = 800;
  w.ox = 133;
  w.px.resize(static_cast<size_t>(cw) * ch);
  for (int y = 0; y < ch; ++y)
    for (int x = 0; x < cw; ++x) w.px[static_cast<size_t>(y) * cw + x] = wpx(x, y);
  w.mask.assign(static_cast<size_t>(kLogicalW) * kLogicalH, 1);
  w.hud.base = base;
  w.hud.back = back;
  w.hud.layers = {l1, l2, l3};
  w.hud.serial = 1;
  PanelRemap rm;
  std::vector<uint32_t> o;
  Presenter p(pd, ScaleMode::Nearest);
  p.set_hud_anchor(true);
  p.set_remap(&rm);
  p.set_frame_world(back, BandFill::Black, std::move(w));
  p.render(cw, ch, o);
  auto at = [&](int x, int y) { return o[static_cast<size_t>(y) * cw + x]; };
  t.add("p9d.left_panel_dimmed_at_its_anchor", at(100, 500) == rgb555_to_bgra(q(P, A)));
  t.add("p9d.right_panel_dimmed_at_its_anchor", at(967, 40) == rgb555_to_bgra(q(P, B)));
  t.add("p9d.side_band_world_dimmed", at(10, 10) == rgb555_to_bgra(q(P, wpx(10, 10))));
  t.add("p9d.vacated_place_shows_the_dimmed_world_once", at(233, 500) == rgb555_to_bgra(q(P, wpx(233, 500))));
  t.add("p9d.report_drawn_after_the_dim", at(533, 300) == rgb555_to_bgra(C));
  int32_t x1 = 100, y1 = 500, x2 = 533, y2 = 300;
  rm.map(x1, y1);
  rm.map(x2, y2);
  // the moved panel's pixel at its anchor still maps to its 800-space place (233 = 133 + 100); the report pixel stays centred
  t.add("p9d.remap_under_the_dim", x1 == 233 && y1 == 500 && x2 == 533 && y2 == 300);
}

// P9C (--selftest-p9c): text at the window's resolution (textres.h, Presenter::set_text / draw_runs, portsim::gfx_text_peel) on a
// synthetic frame: two runs drawn with GDI's TextOutA in Arial (a system face) onto a 16-bit DIB as jgld draws the game's text, an
// opaque rectangle drawn over half of the second run afterwards, and a third run inside a left-anchored HUD layer. Rows: the peel
// gives the background without any glyph; at 800 x 600 the redraw (forced, as --text-unit-redraw) and the plain native picture
// both equal the frame mode's in every pixel; at 1600 x 1200 the picture equals the background scaled with the strings drawn
// there directly at twice the size (and the rectangle kept over the second); the ink is twice as tall and has the run's colour;
// the layer's run moves with its layer. Controls: --negctl p9c-unremoved (double text), p9c-wrong-scale, p9c-wrong-colour.
namespace p9c {
// TextOutA of `text` onto a w x h RGB555 buffer (a full-size 16-bit top-down DIB), the given LOGFONTA height and quality
void gdi_text(std::vector<uint16_t> &buf, int32_t w, int32_t h, const char *face, int32_t height, uint32_t colorref, int32_t x,
              int32_t y, const std::string &text, int quality = 0) {
  HDC dc = CreateCompatibleDC(nullptr);
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 16;
  bi.bmiHeader.biCompression = BI_RGB;
  void *bits = nullptr;
  HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ old = SelectObject(dc, bmp);
  const size_t pitch = static_cast<size_t>((w + 1) & ~1);
  uint16_t *b = static_cast<uint16_t *>(bits);
  for (int32_t r = 0; r < h; ++r) std::memcpy(b + pitch * r, buf.data() + static_cast<size_t>(w) * r, 2u * w);
  LOGFONTA lf = {};
  lf.lfHeight = -height;
  lf.lfOutPrecision = 7;
  lf.lfQuality = static_cast<BYTE>(quality);
  std::strcpy(lf.lfFaceName, face);
  HFONT f = CreateFontIndirectA(&lf);
  HGDIOBJ of = SelectObject(dc, f);
  SetBkMode(dc, TRANSPARENT);
  SetTextAlign(dc, TA_BASELINE);
  SetTextColor(dc, colorref);
  TextOutA(dc, x, y, text.data(), static_cast<int>(text.size()));
  GdiFlush();
  for (int32_t r = 0; r < h; ++r) std::memcpy(buf.data() + static_cast<size_t>(w) * r, b + pitch * r, 2u * w);
  SelectObject(dc, of);
  SelectObject(dc, old);
  DeleteObject(f);
  DeleteObject(bmp);
  DeleteDC(dc);
}
struct Cap {  // one captured run: box (changed pixels + 2), before / after
  portsim::GfxTextRun r;
  std::vector<uint16_t> before, after;
};
Cap draw_run(std::vector<uint16_t> &img, const char *text, int32_t x, int32_t y, int32_t size, uint32_t colorref) {
  const std::vector<uint16_t> snap = img;
  gdi_text(img, kLogicalW, kLogicalH, "Arial", size, colorref, x, y, text);
  Cap c;
  int32_t x0 = kLogicalW, y0 = kLogicalH, x1 = -1, y1 = -1;
  for (int32_t yy = 0; yy < kLogicalH; ++yy)
    for (int32_t xx = 0; xx < kLogicalW; ++xx)
      if (snap[static_cast<size_t>(yy) * kLogicalW + xx] != img[static_cast<size_t>(yy) * kLogicalW + xx]) {
        x0 = std::min(x0, xx), y0 = std::min(y0, yy), x1 = std::max(x1, xx), y1 = std::max(y1, yy);
      }
  c.r.face = "Arial";
  c.r.size = size;
  c.r.colorref = colorref;
  c.r.x = x;
  c.r.y = y;
  c.r.text = text;
  c.r.x0 = std::max(0, x0 - 2), c.r.y0 = std::max(0, y0 - 2), c.r.x1 = std::min(kLogicalW, x1 + 3), c.r.y1 = std::min(kLogicalH, y1 + 3);
  for (int32_t yy = c.r.y0; yy < c.r.y1; ++yy)
    for (int32_t xx = c.r.x0; xx < c.r.x1; ++xx) {
      c.before.push_back(snap[static_cast<size_t>(yy) * kLogicalW + xx]);
      c.after.push_back(img[static_cast<size_t>(yy) * kLogicalW + xx]);
    }
  return c;
}
portsim::GfxTextPeelRun peel_of(const Cap &c) {
  portsim::GfxTextPeelRun p;
  p.x0 = c.r.x0, p.y0 = c.r.y0, p.x1 = c.r.x1, p.y1 = c.r.y1;
  p.before = c.before.data();
  p.after = c.after.data();
  return p;
}
}  // namespace p9c

void selftest_p9c(Tests &t, TextDefect td) {
  using namespace p9c;
  std::vector<uint16_t> bg(static_cast<size_t>(kLogicalW) * kLogicalH);
  for (int y = 0; y < kLogicalH; ++y)
    for (int x = 0; x < kLogicalW; ++x)
      bg[static_cast<size_t>(y) * kLogicalW + x] = static_cast<uint16_t>(((x / 40) & 1 ? 0x0108 : 0x0c63) + ((y / 30) & 3));
  const uint32_t white = 0x00f8f8f8u, gold = 0x0038f8f8u, yellow = 0x0000f8f8u;  // COLORREFs (r | g << 8 | b << 16), 5-bit exact
  const uint16_t occ = 0x7c00;                             // the rectangle drawn over the second run (pure red)
  std::vector<uint16_t> img = bg;
  Cap a = draw_run(img, "Sandbox Golf Course", 100, 200, 16, yellow);
  Cap b = draw_run(img, "Membership 42", 300, 300, 14, gold);
  // the occluder: the right half of b's box, drawn after b (it stays over b's window-resolution glyphs)
  const int32_t ox0 = (b.r.x0 + b.r.x1) / 2, ox1 = b.r.x1 + 4, oy0 = b.r.y0 - 3, oy1 = b.r.y1 + 3;
  std::vector<uint16_t> expect_clean = bg;
  for (int y = oy0; y < oy1; ++y)
    for (int x = ox0; x < ox1; ++x) {
      img[static_cast<size_t>(y) * kLogicalW + x] = occ;
      expect_clean[static_cast<size_t>(y) * kLogicalW + x] = occ;
    }
  // the peel, as the backend makes it at Window::refreshFull
  portsim::GfxTextFrame tf;
  tf.serial = 1;
  tf.back = img;
  tf.clean = img;
  std::vector<std::vector<uint8_t>> vis;
  portsim::gfx_text_peel(tf.clean, kLogicalW, kLogicalH, {peel_of(a), peel_of(b)}, vis);
  a.r.vis = vis[0];
  b.r.vis = vis[1];
  tf.runs = {a.r, b.r};
  t.add("p9c.peel_leaves_no_glyph", tf.clean == expect_clean);
  size_t bvis = 0, bcov = 0;
  for (size_t i = 0; i < b.r.vis.size(); ++i) (b.r.vis[i] ? bvis : bcov) += 1;
  t.add("p9c.peel_covered_part_not_visible", bvis > 0 && bcov > 0);
  auto render = [&](bool native, bool unit_redraw, int32_t cw, int32_t ch, ScaleMode m) {
    Presenter p(PresentDefect::None, m);
    p.set_text_res(native);
    p.set_text_defect(td);
    p.set_text_unit_redraw(unit_redraw);
    p.set_frame(img, BandFill::Black);
    portsim::GfxTextFrame c = tf;
    p.set_text(std::move(c));
    std::vector<uint32_t> o;
    p.render(cw, ch, o);
    return o;
  };
  const std::vector<uint32_t> frame1 = render(false, false, 800, 600, ScaleMode::Nearest);
  t.add("p9c.native_at_1x_equals_frame", render(true, false, 800, 600, ScaleMode::Nearest) == frame1);
  t.add("p9c.redraw_at_1x_equals_frame", render(true, true, 800, 600, ScaleMode::Nearest) == frame1);
  t.add("p9c.redraw_at_1x_equals_frame_smooth", render(true, true, 800, 600, ScaleMode::Smooth) == render(false, false, 800, 600, ScaleMode::Smooth));
  // 2x: the expected picture = the background scaled, the two strings drawn at twice the size at twice the origin, then the
  // occluder scaled over the second
  const int32_t W = 1600, H = 1200;
  std::vector<uint16_t> e(static_cast<size_t>(W) * H);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) e[static_cast<size_t>(y) * W + x] = bg[static_cast<size_t>(y / 2) * kLogicalW + x / 2];
  gdi_text(e, W, H, "Arial", 32, yellow, 200, 400, a.r.text);
  gdi_text(e, W, H, "Arial", 28, gold, 600, 600, b.r.text);
  for (int y = oy0 * 2; y < oy1 * 2; ++y)
    for (int x = ox0 * 2; x < ox1 * 2; ++x) e[static_cast<size_t>(y) * W + x] = occ;
  const std::vector<uint32_t> n2 = render(true, false, W, H, ScaleMode::Nearest);
  size_t diff = 0;
  for (size_t i = 0; i < e.size(); ++i) diff += n2[i] != rgb555_to_bgra(e[i]) ? 1 : 0;
  std::printf("p9c: 2x picture differs from the direct draw in %zu px\n", diff);
  t.add("p9c.scaled_equals_direct_draw", diff == 0);
  // the ink of the first run (pixels that differ from the scaled background): twice the 800-space height, the run's colour
  int32_t ytop = H, ybot = -1, ytop1 = kLogicalH, ybot1 = -1;
  std::map<uint32_t, size_t> colours;
  for (int y = a.r.y0 * 2; y < a.r.y1 * 2; ++y)
    for (int x = a.r.x0 * 2; x < a.r.x1 * 2; ++x) {
      const uint32_t v = n2[static_cast<size_t>(y) * W + x];
      if (v == rgb555_to_bgra(bg[static_cast<size_t>(y / 2) * kLogicalW + x / 2])) continue;
      ytop = std::min(ytop, y), ybot = std::max(ybot, y);
      ++colours[v];
    }
  for (int y = a.r.y0; y < a.r.y1; ++y)
    for (int x = a.r.x0; x < a.r.x1; ++x)
      if (a.after[static_cast<size_t>(y - a.r.y0) * (a.r.x1 - a.r.x0) + (x - a.r.x0)] != a.before[static_cast<size_t>(y - a.r.y0) * (a.r.x1 - a.r.x0) + (x - a.r.x0)])
        ytop1 = std::min(ytop1, y), ybot1 = std::max(ybot1, y);
  const int32_t h2 = ybot - ytop + 1, h1 = ybot1 - ytop1 + 1;
  std::printf("p9c: ink height 800-space %d, at 2x %d\n", h1, h2);
  t.add("p9c.ink_twice_as_tall", h1 > 0 && h2 >= 2 * h1 - 3 && h2 <= 2 * h1 + 3);
  uint32_t top = 0;
  size_t topn = 0;
  for (const auto &kv : colours)
    if (kv.second > topn) top = kv.first, topn = kv.second;
  t.add("p9c.ink_in_the_run_colour", (top & 0xffffffu) == 0x00ffff00u);  // yellow: RGB555 0x7fe0 -> BGRA 0xffff00
  size_t occ_bad = 0;
  for (int y = oy0 * 2; y < oy1 * 2; ++y)
    for (int x = ox0 * 2; x < ox1 * 2; ++x) occ_bad += n2[static_cast<size_t>(y) * W + x] != rgb555_to_bgra(occ) ? 1 : 0;
  t.add("p9c.later_draw_stays_over_the_run", occ_bad == 0);
  // a run inside a left-anchored layer (a panel at 20..220 x 470..520 with a label) moves with the layer
  {
    std::vector<uint16_t> base = bg, back = bg;
    portsim::GfxHudLayer l;
    l.id = portsim::kGfxLayerBottom;
    l.anchor = -1;
    l.x0 = 20, l.y0 = 470, l.x1 = 220, l.y1 = 520;
    l.px.assign(static_cast<size_t>(200) * 50, portsim::GfxLayerPx{});
    for (int y = l.y0; y < l.y1; ++y)
      for (int x = l.x0; x < l.x1; ++x) back[static_cast<size_t>(y) * kLogicalW + x] = 0x0010;
    for (auto &p : l.px) p.kind = 1, p.px = 0x0010;
    Cap c = draw_run(back, "Build Course", 40, 500, 16, white);
    std::vector<uint16_t> lay = back;
    std::vector<std::vector<uint8_t>> lv;
    portsim::gfx_text_peel(lay, kLogicalW, kLogicalH, {peel_of(c)}, lv);
    c.r.vis = lv[0];
    c.r.vis_layer = lv[0];
    c.r.layer = 0;
    WorldImage w;
    w.pw = 1600, w.ph = 900, w.fw = 1200, w.ox = 200;
    w.px.assign(static_cast<size_t>(w.pw) * w.ph, 0x0200);
    w.mask.assign(static_cast<size_t>(kLogicalW) * kLogicalH, 0);  // the world shows nowhere inside the frame
    w.hud.base = base;
    w.hud.back = back;
    w.hud.layers = {l};
    w.hud.serial = 7;
    portsim::GfxTextFrame lt;
    lt.serial = 2;
    lt.hud_serial = 7;
    lt.back = back;
    lt.clean = lay;
    lt.base_clean = base;
    lt.runs = {c.r};
    Presenter p(PresentDefect::None, ScaleMode::Nearest);
    p.set_text_res(true);
    p.set_text_defect(td);
    p.set_hud_anchor(true);
    p.set_frame_world(back, BandFill::Black, std::move(w));
    p.set_text(std::move(lt));
    std::vector<uint32_t> o;
    Viewport vp;
    p.render(1600, 900, o, &vp);  // content 1200 x 900 at origin 200: the layer moves 200 client px left
    auto ink = [&](int32_t dx) {  // client pixels of the label's box that hold the run's white
      size_t n = 0;
      for (int y = vp.origin_y + c.r.y0 * 3 / 2; y < vp.origin_y + c.r.y1 * 3 / 2; ++y)
        for (int x = vp.origin_x + dx + c.r.x0 * 3 / 2; x < vp.origin_x + dx + c.r.x1 * 3 / 2; ++x)
          if (x >= 0 && x < 1600 && (o[static_cast<size_t>(y) * 1600 + x] & 0xffffffu) == 0x00ffffffu) ++n;
      return n;
    };
    const size_t moved = ink(-vp.origin_x), centred = ink(0);
    std::printf("p9c: layer run white px at the anchor %zu, at the centred place %zu\n", moved, centred);
    t.add("p9c.layer_run_moves_with_its_layer", moved > 50 && centred == 0);
  }
}

int run_selftest(const Options &o) {
  Tests t;
  if (o.selftest_p5b) {
    selftest_p5b(t, o.negctl == "p5b-smooth-ignored");
    return t.print();
  }
  if (o.selftest_p7) {
    selftest_p7(t, o.negctl == "p7-band-black");
    return t.print();
  }
  if (o.selftest_p9a) {  // P9A item 5
    selftest_p9a(t, o.negctl == "p9a-band-soft");
    return t.print();
  }
  if (o.selftest_p8b) {  // P8B
    selftest_p8b(t, o.negctl == "p8b-mask-ignored", o.negctl == "p8b-hud-unmoved");
    return t.print();
  }
  if (o.selftest_p9b) {  // P9B
    selftest_p9b(t, o.negctl == "p9b-blend-opaque" ? PresentDefect::LayerBlendOpaque
                    : o.negctl == "p9b-rect-move"  ? PresentDefect::LayerRectMove
                                                   : PresentDefect::None);
    return t.print();
  }
  if (o.selftest_p9d) {  // P9D
    selftest_p9d(t, o.negctl == "p9d-dim-local" ? PresentDefect::DimLocal : PresentDefect::None);
    return t.print();
  }
  if (o.selftest_p9c) {  // P9C
    selftest_p9c(t, o.negctl == "p9c-unremoved"      ? TextDefect::Unremoved
                    : o.negctl == "p9c-wrong-scale"  ? TextDefect::WrongScale
                    : o.negctl == "p9c-wrong-colour" ? TextDefect::WrongColour
                                                     : TextDefect::None);
    return t.print();
  }
  PresentDefect pd = PresentDefect::None;
  SavesDefect sd = SavesDefect::None;
  if (o.negctl == "present-stale") pd = PresentDefect::Stale;
  if (o.negctl == "present-swap") pd = PresentDefect::ByteSwap;
  if (o.negctl == "saves-install") sd = SavesDefect::WriteToInstall;
  if (o.negctl == "saves-noreadback") sd = SavesDefect::NoReadBack;
  if (o.negctl == "saves-inside") sd = SavesDefect::AllowInsideInstall;
  if (o.negctl == "pacing-no-bit17") g_pacing_defect = 1;
  if (o.negctl == "pacing-signed") g_pacing_defect = 2;
  if (o.negctl == "pacing-quantum") g_pacing_defect = 3;
  selftest_pacing(t);
  selftest_saves(t, sd);
  selftest_present(t, pd);
  return t.print();
}

// ------------------------------------------------------------------------------------------------ offline translation
int run_translate(const Options &o) {
  std::ifstream in(fs::path(o.translate_in));
  if (!in) {
    std::fprintf(stderr, "cannot read %s\n", o.translate_in.c_str());
    return 2;
  }
  std::FILE *out = o.translate_out.empty() ? nullptr : std::fopen(o.translate_out.c_str(), "wb");
  Translator tr;
  uint32_t n = 0, bad = 0, calls = 0;
  for (std::string line; std::getline(in, line);) {
    if (line.empty()) continue;
    InputEvent e;
    if (!parse_event_json(line, e)) {
      ++bad;
      continue;
    }
    ++n;
    tr.set_viewport(e.vp);
    ScriptedHost host(e.answers);
    const Translation t = tr.translate(e.msg, e.wp, e.lp, host);
    const bool same = host.ok() && steps_json(t.steps) == steps_json(e.steps);
    for (const Step &s : t.steps) calls += s.kind == Step::Kind::Call ? 1 : 0;
    if (!same) ++bad;
    if (out) std::fprintf(out, "{\"seq\":%u,\"same\":%s,\"steps\":%s}\n", e.seq, same ? "true" : "false", steps_json(t.steps).c_str());
  }
  if (out) std::fclose(out);
  std::printf("translate: messages %u handler calls %u mismatches %u\n", n, calls, bad);
  return bad ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------ overlay operations
int run_op(const Options &o, AppFiles &f) {
  if (o.op == "cat" && o.op_args.size() == 2) {
    const int64_t n = f.size(o.op_args[0]);
    if (n < 0) return 3;
    std::vector<uint8_t> b(static_cast<size_t>(n));
    if (n) f.read(o.op_args[0], 0, b.data(), b.size());
    return write_file(o.op_args[1], b.data(), b.size()) ? 0 : 4;
  }
  if (o.op == "write" && o.op_args.size() == 2) {
    std::vector<uint8_t> b;
    if (!read_file(o.op_args[1], b)) return 4;
    return f.write(o.op_args[0], b) ? 0 : 5;
  }
  if (o.op == "remove" && o.op_args.size() == 1) return f.remove(o.op_args[0]) ? 0 : 5;
  if (o.op == "list" && o.op_args.size() == 1) {
    for (const auto &e : f.list(o.op_args[0])) std::printf("%s%s\n", e.name.c_str(), e.is_dir ? "/" : "");
    return 0;
  }
  return 2;
}

std::string stamp() {
  SYSTEMTIME s;
  GetLocalTime(&s);
  char b[64];
  std::snprintf(b, sizeof b, "%04u%02u%02u-%02u%02u%02u-%lu", s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond,
                GetCurrentProcessId());
  return b;
}

bool parse(int argc, char **argv, Options &o) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto val = [&](std::string &dst) {
      if (i + 1 >= argc) return false;
      dst = argv[++i];
      return true;
    };
    std::string v;
    if (a == "--install") { if (!val(o.install)) return false; }
    else if (a == "--exe") { if (!val(o.exe)) return false; }
    else if (a == "--data") { if (!val(o.data)) return false; }
    else if (a == "--record-dir") { if (!val(o.record_dir)) return false; }
    else if (a == "--present-trace") { if (!val(o.trace)) return false; }
    else if (a == "--corpus") { if (!val(o.corpus)) return false; }
    else if (a == "--negctl") { if (!val(o.negctl)) return false; }
    else if (a == "--snapshot-at") { if (!val(v)) return false; o.snapshot_at = std::strtoll(v.c_str(), nullptr, 0); }
    else if (a == "--max-records") { if (!val(v)) return false; o.max_records = std::atoi(v.c_str()); }
    else if (a == "--in-memory") o.in_memory = true;
    else if (a == "--mem-every") { if (!val(v)) return false; o.mem_every = std::atoi(v.c_str()); }
    else if (a == "--dpi") { if (!val(v)) return false; o.dpi = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--exit-after") { if (!val(v)) return false; o.exit_after = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--put") {
      if (!val(v)) return false;
      const size_t eq = v.find('=');
      if (eq == std::string::npos) return false;
      o.puts.emplace_back(v.substr(0, eq), v.substr(eq + 1));
    }
    else if (a == "--idle-ms") { if (!val(v)) return false; o.idle_ms = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--pass-ms") { if (!val(v)) return false; o.pass_ms = static_cast<uint32_t>(std::atoi(v.c_str())); }
    else if (a == "--no-timed-passes") o.timed_passes = false;
    else if (a == "--cursor") {
      if (!val(o.cursor) || (o.cursor != "system" && o.cursor != "message")) return false;
    }
    else if (a == "--clock-origin") { if (!val(v)) return false; o.clock_origin = std::strtoll(v.c_str(), nullptr, 0); }
    else if (a == "--edge-guard" || a == "--close-prompt" || a == "--resizable") {  // P5B
      if (!val(v) || (v != "on" && v != "off")) return false;
      (a == "--edge-guard" ? o.edge_guard : a == "--close-prompt" ? o.close_prompt : o.resizable) = v == "on";
    }
    else if (a == "--loading-overlay") {  // P6B item 2
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.loading_overlay = v == "on";
    }
    else if (a == "--zoom-keys") {
      if (!val(o.zoom_keys) || (o.zoom_keys != "all" && o.zoom_keys != "numpad" && o.zoom_keys != "off")) return false;
    }
    else if (a == "--scale") {
      if (!val(o.scale) || (o.scale != "auto" && o.scale != "nearest" && o.scale != "smooth")) return false;
    }
    else if (a == "--keys") {
      if (!val(o.keys) || (o.keys != "system" && o.keys != "message")) return false;
    }
    else if (a == "--flag17") {
      if (!val(v)) return false;
      const size_t c = v.find(':');
      if (c == std::string::npos) return false;
      o.flag17_from = std::atoi(v.substr(0, c).c_str());
      o.flag17_to = std::atoi(v.substr(c + 1).c_str());
    }
    else if (a == "--terrain") {
      if (!val(o.terrain) || (o.terrain != "live" && o.terrain != "headless")) return false;
    }
    else if (a == "--audio-no-device") o.audio_no_device = true;  // S7.11
    else if (a == "--profile" || a == "--profile-list") {  // S7.12
      o.profile_list = a == "--profile-list";
      if (!val(o.profile) || (o.profile != "original" && o.profile != "fixed" && o.profile != "play")) return false;
    }
    else if (a == "--audio") {  // S7.10
      if (!val(o.audio) || (o.audio != "off" && o.audio != "on" && o.audio != "model" && o.audio != "capture")) return false;
    }
    else if (a == "--gl") {
      if (!val(o.gl) || (o.gl != "auto" && o.gl != "wgl" && o.gl != "host" && o.gl != "raster")) return false;
    }
    else if (a == "--terrain-va") { if (!val(v)) return false; o.terrain_va = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 0)); }
    else if (a == "--live-trace") { if (!val(o.live_trace)) return false; }
    else if (a == "--snapshots") {
      if (!val(v)) return false;
      for (size_t p = 0; p < v.size();) {
        const size_t c = v.find(',', p);
        const std::string t = v.substr(p, c == std::string::npos ? std::string::npos : c - p);
        if (!t.empty()) o.snapshot_list.push_back(std::strtoll(t.c_str(), nullptr, 0));
        if (c == std::string::npos) break;
        p = c + 1;
      }
    }
    else if (a == "--no-activate") o.activate = false;
    else if (a == "--no-window") o.window = false;
    else if (a == "--selftest") o.selftest = true;
    else if (a == "--selftest-p5b") o.selftest = o.selftest_p5b = true;  // P5B
    else if (a == "--selftest-p7") o.selftest = o.selftest_p7 = true;    // P7
    else if (a == "--selftest-p9a") o.selftest = o.selftest_p9a = true;  // P9A item 5
    else if (a == "--selftest-p8b") o.selftest = o.selftest_p8b = true;  // P8B
    else if (a == "--selftest-p9b") o.selftest = o.selftest_p9b = true;  // P9B
    else if (a == "--selftest-p9c") o.selftest = o.selftest_p9c = true;  // P9C
    else if (a == "--selftest-p9d") o.selftest = o.selftest_p9d = true;  // P9D
    else if (a == "--wide") {  // P7: a bare flag, or --wide on|off
      o.wide = true;
      if (i + 1 < argc && (!std::strcmp(argv[i + 1], "on") || !std::strcmp(argv[i + 1], "off"))) o.wide = !std::strcmp(argv[++i], "on");
    }
    else if (a == "--wide-width") {
      if (!val(v)) return false;
      o.wide_width = std::atoi(v.c_str());
      if (o.wide_width < kLogicalW || o.wide_width > 2400) return false;
    }
    else if (a == "--render-scale") {  // P8B
      if (!val(o.render_scale)) return false;
      if (o.render_scale != "1" && o.render_scale != "native" && o.render_scale != "auto") {
        int pw = 0, ph = 0;
        if (std::sscanf(o.render_scale.c_str(), "%dx%d", &pw, &ph) != 2 || pw < 200 || ph < 150 || pw > 8192 || ph > 8192) return false;
      }
    }
    else if (a == "--wide-edge") {  // P8B
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.wide_edge = v == "on";
    }
    else if (a == "--world-check") o.world_check = true;  // P8B
    else if (a == "--hud-anchor") {  // G32
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_anchor = v == "on";
    }
    else if (a == "--hud-layers") {  // P9B
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_layers = v == "on";
    }
    else if (a == "--hud-modal") {  // P9D
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_modal = v == "on";
    }
    else if (a == "--hud-popups") {  // P9E
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_popups = v == "on";
    }
    else if (a == "--hud-close") {  // P9E fix 2
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_close = v == "on";
    }
    else if (a == "--hud-vacated") {  // P9E fix 3
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_vacated = v == "on";
    }
    else if (a == "--sprite-res") {  // P10D
      if (!val(v) || (v != "native" && v != "frame")) return false;
      o.sprite_native = v == "native";
    }
    else if (a == "--hud-strip") {  // P13C item 10
      if (!val(v) || (v != "left" && v != "right")) return false;
      o.strip_right = v == "right";
    }
    else if (a == "--hud-panel") {  // P11B fix 1
      if (!val(v) || (v != "left" && v != "right")) return false;
      o.panel_left = v == "left";
    }
    else if (a == "--undo-keys") {  // P11D
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.undo_keys = v == "on";
    }
    else if (a == "--smooth-camera") {  // P12A fix 1
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.smooth_camera = v == "on";
    }
    else if (a == "--smooth-objects") {  // P12A fix 2
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.smooth_objects = v == "on";
    }
    else if (a == "--gpu") {  // P14A
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.gpu = v == "on";
    }
    else if (a == "--gpu-objects") {  // P15C
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.gpu_objects = v == "on";
    }
    else if (a == "--world-thread") {  // P15A
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.world_thread = v == "on";
    }
    else if (a == "--world-raster-thread") {  // P15A fix 3
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.world_raster_thread = v == "on";
    }
    else if (a == "--object-rate") {  // P12A fix 3
      if (!val(v)) return false;
      o.object_rate = v == "auto" ? -1 : std::atoi(v.c_str());  // P14A fix 2: auto
      if (v != "auto" && (o.object_rate < 0 || o.object_rate > 1000)) return false;
    }
    else if (a == "--hover-rate") {  // P10I fix 2
      if (!val(v) || (v != "display" && v != "game")) return false;
      o.hover_rate = v;
    }
    else if (a == "--map-bands") {  // P13B item 18
      if (!val(v) || (v != "clamp" && v != "same")) return false;
      o.map_bands = v == "clamp";
    }
    else if (a == "--hover-help") {  // P11C (P13A: course)
      if (!val(v) || (v != "on" && v != "course" && v != "off")) return false;
      o.hover_help = v != "off";
      o.hover_help_course = v == "course";
    }
    else if (a == "--line-res") {  // P10I fix 1
      if (!val(v) || (v != "native" && v != "frame")) return false;
      o.line_native = v == "native";
    }
    else if (a == "--terrain-filter") {  // P13D item 4
      if (!val(v) || (v != "sharp" && v != "linear" && v != "nearest")) return false;
      o.terrain_filter = v == "sharp" ? 2 : v == "linear" ? 1 : 0;
    }
    else if (a == "--sprite-filter") {  // P10H
      if (!val(v) || (v != "edge" && v != "sharp" && v != "smooth" && v != "sharp-area")) return false;
      o.sprite_filter = v == "edge" ? 3 : v == "sharp" ? 1 : v == "smooth" ? 0 : 2;  // P13D fix 5: edge
    }
    else if (a == "--fps") {  // P10A
      if (!val(v) || (v != "on" && v != "off" && v != "detail")) return false;
      o.fps = v;
    }
    else if (a == "--paint-rate") {  // P10E
      if (!val(v) || (v != "display" && v != "game")) return false;
      o.paint_rate = v;
    }
    else if (a == "--profile-csv") {  // P10A
      if (!val(o.profile_csv)) return false;
    }
    else if (a == "--hud-cursor") {  // P9E fix 4
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_cursor = v == "on";
    }
    else if (a == "--text-peel") {  // P10B fix 3
      if (!val(v) || (v != "witness" && v != "plain")) return false;
      o.text_peel = v;
    }
    else if (a == "--text-fit") {  // P10B fix 2
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.text_fit = v == "on";
    }
    else if (a == "--text-banner") {  // P10B fix 1
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.text_banner = v == "on";
    }
    else if (a == "--title-bands") {  // P10C fix 1
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.title_bands = v == "on";
    }
    else if (a == "--hud-writes") {  // P10C fix 2
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_writes = v == "on";
    }
    else if (a == "--photo-dir") {  // P10C fix 3
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.photo_dir = v == "on";
    }
    else if (a == "--hud-textbox") {  // P10C fix 4
      if (!val(v) || (v != "on" && v != "off")) return false;
      o.hud_textbox = v == "on";
    }
    else if (a == "--text-res") {  // P9C
      if (!val(v) || (v != "native" && v != "frame")) return false;
      o.text_res = v;
    }
    else if (a == "--text-aa") {  // P9C
      if (!val(v) || (v != "system" && v != "cleartype" && v != "grey")) return false;
      o.text_aa = v;
    }
    else if (a == "--text-unit-redraw") o.text_unit_redraw = true;  // P9C (measurement)
    else if (a == "--ui-union") o.ui_union = true;        // G32 development
    else if (a == "--view-width") {  // G32
      if (!val(v)) return false;
      o.view_width = std::atoi(v.c_str());
      if (o.view_width < 0 || o.view_width > 8192) return false;
    }
    else if (a == "--world-shots") {  // P8B
      if (!val(v)) return false;
      size_t p = 0;
      while (p <= v.size()) {
        const size_t c = v.find(',', p);
        const std::string t = v.substr(p, c == std::string::npos ? std::string::npos : c - p);
        if (!t.empty()) o.world_shots.push_back(std::strtoll(t.c_str(), nullptr, 0));
        if (c == std::string::npos) break;
        p = c + 1;
      }
    }
    else if (a == "--shot-size") {  // P8B
      if (!val(v) || std::sscanf(v.c_str(), "%dx%d", &o.shot_w, &o.shot_h) != 2 || o.shot_w <= 0 || o.shot_h <= 0) return false;
    }
    else if (a == "--wide-fill") {
      if (!val(o.wide_fill) || (o.wide_fill != "soft" && o.wide_fill != "clamp" && o.wide_fill != "black" && o.wide_fill != "edge" && o.wide_fill != "mirror")) return false;
    }
    else if (a == "--compose") {
      if (i + 4 >= argc) return false;
      o.compose = {argv[i + 1], argv[i + 2], argv[i + 3], argv[i + 4]};
      i += 4;
    }
    else if (a == "--compose-band") {
      if (!val(o.compose_band) || (o.compose_band != "extend" && o.compose_band != "black" && o.compose_band != "map")) return false;
    }
    else if (a == "--translate") { if (!val(o.translate_in)) return false; }
    else if (a == "--out") { if (!val(o.translate_out)) return false; }
    else if (a == "--cat" || a == "--write") {
      o.op = a.substr(2);
      if (i + 2 >= argc) return false;
      o.op_args = {argv[i + 1], argv[i + 2]};
      i += 2;
    }
    else if (a == "--remove" || a == "--list") {
      o.op = a.substr(2);
      if (!val(v)) return false;
      o.op_args = {v};
    }
    else {
      std::fprintf(stderr, "unknown argument %s\n", a.c_str());
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  Options o;
  // G30: the default profile is `play`; SG_APP_PROFILE=original|fixed|play overrides it (port/test/conftest.py sets `fixed`, so the
  // app tests keep their pre-G30 behaviour); an explicit --profile wins over both
  if (const char *ep = std::getenv("SG_APP_PROFILE"))
    if (!std::strcmp(ep, "original") || !std::strcmp(ep, "fixed") || !std::strcmp(ep, "play")) o.profile = ep;
  // P5B: SG_APP_P5B=off restores S7's window and input behaviour (port/test/conftest.py sets it, so the app tests that compare
  // with nearest-pixel frames, drive an unfocused window or close it with WM_CLOSE keep their behaviour); flags still win
  if (const char *ep = std::getenv("SG_APP_P5B"))
    if (!std::strcmp(ep, "off")) {
      o.edge_guard = o.close_prompt = o.resizable = false;
      o.zoom_keys = "off";
      o.scale = "nearest";
    }
  if (const char *ep = std::getenv("SG_APP_P8B"))  // G32: --wide is on by default; SG_APP_P8B=off (port/test/conftest.py) turns it off
    if (!std::strcmp(ep, "off")) o.wide = false;
  if (const char *ep = std::getenv("SG_APP_WIDE"))  // P7: flags still win
    o.wide = !std::strcmp(ep, "on") || !std::strcmp(ep, "1");
  if (const char *ep = std::getenv("SG_APP_P6B"))  // P6B: the loading overlay off for the app tests (window pixels)
    if (!std::strcmp(ep, "off")) o.loading_overlay = false;
  if (const char *ep = std::getenv("SG_APP_P9"))  // P9A: P7's soft band fill (port/test/conftest.py sets it); flags still win
    if (!std::strcmp(ep, "off")) o.wide_fill = "soft";
  if (const char *ep = std::getenv("SG_APP_P9C"))  // P9C: the frame's text scaled with the picture (port/test/conftest.py); flags win
    if (!std::strcmp(ep, "off")) o.text_res = "frame";
  if (const char *ep = std::getenv("SG_APP_P9D"))  // P9D: P9B's capture of the modal screens; flags win
    if (!std::strcmp(ep, "off")) o.hud_modal = false;
  if (const char *ep = std::getenv("SG_APP_P9E"))  // P9E: the layer-compositor fixes of P9E off (P9D's behaviour); flags win
    if (!std::strcmp(ep, "off")) o.hud_popups = o.hud_close = o.hud_vacated = o.hud_cursor = false;
  if (const char *ep = std::getenv("SG_APP_P10"))  // P10: the fourth play-test's app changes off (port/test/conftest.py); flags win
    if (!std::strcmp(ep, "off")) {
      o.text_banner = o.text_fit = false;
      o.fps = "off";
      o.text_peel = "plain";
      o.title_bands = o.hud_writes = o.photo_dir = o.hud_textbox = false;
      o.paint_rate = "game";  // P10E
      o.sprite_native = false;  // P10D
      o.sprite_filter = 0;      // P10H
      o.line_native = false;    // P10I fix 1
      o.hover_rate = "game";    // P10I fix 2
    }
  if (const char *ep = std::getenv("SG_APP_P11"))  // P11: this play-test round's app changes off (port/test/conftest.py); flags win
    if (!std::strcmp(ep, "off")) {
      o.panel_left = false;
      o.undo_keys = false;
      o.hover_help = false;
    }
  if (const char *ep = std::getenv("SG_APP_P13"))  // P13: the sixth play-test's app changes off (port/test/conftest.py); flags win
    if (!std::strcmp(ep, "off")) {
      o.terrain_filter = 0;  // P13D item 4
      if (o.sprite_filter == 3) o.sprite_filter = 1;  // P13D fix 5: sharp (P10H's default)
      o.hover_help_course = true;  // P13A item 2: P11C's course line back
      o.strip_right = false;  // P13C item 10
      o.map_bands = false;  // P13B item 18
    }
  if (const char *ep = std::getenv("SG_APP_P12"))  // P12: motion at the display's rate off (port/test/conftest.py); flags win
    if (!std::strcmp(ep, "off")) o.smooth_camera = o.smooth_objects = false;  // P12A fixes 1 and 2
  if (const char *ep = std::getenv("SG_APP_P14"))  // P14: the GPU path off (port/test/conftest.py); flags win
    if (!std::strcmp(ep, "off")) o.gpu = false;  // P14A
  if (const char *ep = std::getenv("SG_APP_P15"))  // P15: the world thread and the GPU's moving objects off (port/test/conftest.py)
    if (!std::strcmp(ep, "off")) {
      o.world_thread = false;  // P15A
      o.world_raster_thread = false;  // P15A fix 3
      o.gpu_objects = false;   // P15C
    }
  // P9A item 1: the player's build has no console; without a redirection stdout / stderr go to <data>/simgolf_port.log and to
  // the console when there is one (stdio_log.h). Tests and tools pass pipes, which are kept as they are. Set up before the
  // argument parser runs (its messages go there too), with the --data value read ahead.
  {
    std::string data;
    for (int i = 1; i + 1 < argc; ++i)
      if (!std::strcmp(argv[i], "--data")) data = argv[i + 1];
    setup_stdio_log(data.empty() ? AppFiles::default_data_dir() : data);
  }
  if (!parse(argc, argv, o)) {
    std::fprintf(stderr, "usage: see the head of port/app/main.cpp\n");
    return 2;
  }
  portsim::gfx_set_text_peel_witness(o.text_peel == "witness");  // P10B fix 3
  if (o.selftest) return run_selftest(o);
  if (!o.compose.empty()) return run_compose(o);  // P7
  if (!o.translate_in.empty()) return run_translate(o);
  if (o.profile_list) return run_profile_list(o.profile);  // S7.12
  {
    const std::string perr = apply_profile(o.profile, o.negctl == "profile-ignored");  // S7.12: before any run
    if (!perr.empty()) {
      std::fprintf(stderr, "%s\n", perr.c_str());
      return 2;
    }
  }
  if (o.install.empty()) {
    std::fprintf(stderr, "--install <dir> is required (the directory holding the user's golf_clean.exe)\n");
    return 2;
  }
  if (o.exe.empty()) o.exe = (fs::path(o.install) / "golf_clean.exe").string();
  if (o.data.empty()) o.data = AppFiles::default_data_dir();
  if (o.corpus.empty()) o.corpus = (fs::current_path() / "port" / "corpus" / "traces").string();
  SavesDefect sd = SavesDefect::None;
  if (o.negctl == "saves-install") sd = SavesDefect::WriteToInstall;
  if (o.negctl == "saves-noreadback") sd = SavesDefect::NoReadBack;
  if (o.negctl == "saves-inside") sd = SavesDefect::AllowInsideInstall;
  AppFiles files(o.install, o.data, sd);
  if (!files.ok()) {
    std::fprintf(stderr, "saves overlay refused: %s\n", files.error().c_str());
    return 3;
  }
  for (const auto &kv : o.puts) {
    std::vector<uint8_t> b;
    if (!read_file(kv.second, b)) {
      std::fprintf(stderr, "cannot read %s\n", kv.second.c_str());
      return 2;
    }
    files.add_fixture(kv.first, std::move(b));
  }
  if (!o.op.empty()) return run_op(o, files);

  const fs::path rec = o.record_dir.empty() ? fs::path(files.data_root()) / "recordings" / stamp() : fs::path(o.record_dir);
  std::error_code ec;
  fs::create_directories(rec, ec);
  if (!o.live_trace.empty()) return run_live_trace(o, files, rec);       // S7.5: the live path over a corpus trace
  // P10C fix 3: the accomplishment photos (gfx_bulletin.cpp showBulletinBoard.capture) are written only when the game's files
  // hold a `snapshots` directory, as the original's saveBmp fopen fails without one; the installer's directory table lists it, the
  // extracted install has none, so the board's photos stayed empty. The live game makes it in the data directory's files
  if (o.trace.empty() && o.window && o.photo_dir) fs::create_directories(fs::path(files.data_root()) / "files" / "snapshots", ec);
  if (o.trace.empty() && o.window) return run_live_window(o, files, rec);  // S7.6: the game

  PresentDefect pd = o.negctl == "present-stale" ? PresentDefect::Stale
                     : o.negctl == "present-swap" ? PresentDefect::ByteSwap
                     : o.negctl == "p9b-blend-opaque" ? PresentDefect::LayerBlendOpaque  // P9B
                     : o.negctl == "p9b-rect-move" ? PresentDefect::LayerRectMove        // P9B
                     : o.negctl == "p9d-dim-local" ? PresentDefect::DimLocal             // P9D
                                                  : PresentDefect::None;
  Presenter presenter(pd);
  presenter.set_frame(std::vector<uint16_t>(static_cast<size_t>(kLogicalW) * kLogicalH, 0));  // black until a frame comes

  PresentResult pr;
  auto write_session = [&](const AppWindow *w, uint32_t events) {
    std::string s = "{\"app\":\"simgolf_port\",\"mode\":\"" + std::string(o.trace.empty() ? "shell" : "present-trace") + "\"";
    s += profile_json(o.profile);  // S7.12
    s += ",\"install\":\"" + jesc(files.install_root()) + "\",\"data\":\"" + jesc(files.data_root()) + "\"";
    s += ",\"trace\":\"" + jesc(o.trace) + "\",\"snapshot_at\":" + std::to_string(o.snapshot_at);
    s += ",\"max_records\":" + std::to_string(o.max_records) + ",\"records\":" + std::to_string(pr.run.done);
    s += ",\"code\":" + std::to_string(pr.run.code) + ",\"detail\":\"" + jesc(pr.run.detail.substr(0, 2000)) + "\"";
    s += ",\"error\":\"" + jesc(pr.error) + "\",\"data_va\":" + std::to_string(pr.data_va);
    s += ",\"frames_presented\":" + std::to_string(presenter.frames_set() > 1 ? presenter.frames_set() - 1 : 0);
    s += ",\"input_events\":" + std::to_string(events) + ",\"saves_refused\":" + std::to_string(files.refused());
    if (w) s += ",\"client\":[" + std::to_string(w->client_w()) + "," + std::to_string(w->client_h()) + "],\"dpi\":" + std::to_string(w->dpi());
    s += "}\n";
    write_file(rec / "session.json", s.data(), s.size());
  };
  auto finish_frame = [&]() {
    if (!pr.frame.empty()) {
      presenter.set_frame(pr.frame);
      write_file(rec / "back.555", pr.frame.data(), pr.frame.size() * 2);
    }
  };

  if (!o.window) {
    if (!o.trace.empty()) {
      pr = present_trace(o, files, rec);
      finish_frame();
    }
    write_session(nullptr, 0);
    if (!pr.error.empty()) std::fprintf(stderr, "%s\n", pr.error.c_str());
    std::printf("DONE records=%d code=%d\n", pr.run.done, pr.run.code);
    return pr.error.empty() ? 0 : 1;
  }

  WindowOptions wo;
  wo.dpi = o.dpi;
  wo.activate = o.activate;
  wo.input_defect = o.negctl == "input-bypass" ? InputDefect::Bypass : InputDefect::None;
  wo.resizable = o.resizable;  // P5B item 4
  presenter.set_scale(scale_mode(o.scale));
  AppWindow win(presenter, wo);
  if (!win.create()) {
    std::fprintf(stderr, "%s\n", win.error().c_str());
    return 4;
  }
  std::FILE *log = std::fopen((rec / "input.jsonl").string().c_str(), "wb");
  uint32_t events = 0;
  win.set_sink([&](const InputEvent &e) {
    ++events;
    if (log) {
      std::fprintf(log, "%s\n", event_json(e).c_str());
      std::fflush(log);
    }
  });
  HWND hwnd = static_cast<HWND>(win.hwnd());
  auto ready = [&]() {
    std::printf("READY hwnd=0x%llx client=%dx%d dpi=%u record=%s\n", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)),
                win.client_w(), win.client_h(), win.dpi(), rec.string().c_str());
    std::fflush(stdout);
  };
  std::thread worker;
  std::atomic<bool> done{false};
  if (!o.trace.empty()) {
    worker = std::thread([&]() {
      pr = present_trace(o, files, rec);
      done = true;
      PostMessageW(hwnd, WM_APP + 1, 0, 0);
    });
  }
  // the worker's result reaches the window through WM_APP + 1, handled by this loop before dispatch
  if (o.trace.empty()) ready();
  if (o.exit_after) SetTimer(hwnd, 1, o.exit_after, nullptr);
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    if (m.message == WM_APP + 1 && m.hwnd == hwnd) {
      finish_frame();
      win.invalidate();
      UpdateWindow(hwnd);
      write_session(&win, events);
      if (!pr.error.empty()) std::fprintf(stderr, "%s\n", pr.error.c_str());
      ready();
      continue;
    }
    TranslateMessage(&m);  // the jgld pumps call TranslateMessage before DispatchMessageA (WM_CHAR from real keys)
    DispatchMessageW(&m);
  }
  if (worker.joinable()) worker.join();
  if (log) std::fclose(log);
  write_session(&win, events);
  return 0;
}
