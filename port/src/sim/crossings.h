// P39A1 (Mariano 2026-11-20, re/roadmap/P39_COURSE_FEATURES.md "P39A", "P39A decided"; log/roadmap/P39/P39A1_notes.md):
// the two-tile scenic bridge and the port's table of crossings.
//
// The original's Scenic Bridge (palette tool 0x13, H2_Build case 5) marks ONE water tile: the flag word gets the path bit 0x20,
// the scenic bit 0x100 and the art variant in its low 3 bits, the build code 0x93 and the price as the refund (setGrids); P1_flag20
// draws that tile from sheet 0x226 + variant, and a tile with a path-on-water neighbour switches to the boardwalk set (0x1fe..0x205),
// so two scenic bridges side by side lose their art. Removal is the bulldozer's demolishObject: code 0x93 clears bit 0x20 and pays
// the refund back.
//
// Play switch scenicBridge.twoTile (1 = the original: the bridge row holds the 8 one-tile styles). At 0 the row has a ninth item, the
// two-tile bridge: a click on a water (0x11) or stream (0x0a) tile builds a bridge over two orthogonal tiles, each water or stream,
// with land at both ends along its axis. A water tile gets the original's flag bits (0x120 | style), so golfers walk it at path cost,
// the scenic thought (7) fires and the original reads it as a scenic bridge; a stream tile gets the path bit 0x20 only (no thought:
// Mariano's pick). Each tile gets the build code 0x93 and the one-tile price as its refund, so the bulldozer and the undo journal
// treat it as the original's bridge; the pair costs twice the one-tile bridge. The pair is kept here (the flag word has no free bit
// the original would not draw wrong), and in a sidecar file next to the save (crossings_sidecar_path), which the original never reads.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "portsim/registry.h"

namespace portsim {

extern int g_defect_two_tile_bridge;   // scenicBridge.twoTile (1 = the original)
inline bool crossings_on() { return !SG_DEFECT(g_defect_two_tile_bridge); }

constexpr int32_t kScenicBridgeTool = 0x13;   // f_w_4c2848[3]: the Scenic Bridge palette tool
constexpr int32_t kTwoTileItem = 8;           // the bridge row's ninth item: [unk_0056a514] == 8 with the bridge tool
constexpr int32_t kBridgeStyles = 8;          // the one-tile styles 0..7 (sheets 0x226 + style)

// one two-tile bridge: tiles (x, y) and (x + dx, y + dy), dx / dy = (1, 0) for axis 0, (0, 1) for axis 1
struct Crossing {
  int16_t x = 0, y = 0;
  uint8_t axis = 0;    // 0: along x, 1: along y
  uint8_t style = 0;   // the art (sheet 0x226 + style)
  uint8_t kind = 0;    // bit 0: the first tile is a stream, bit 1: the second tile is a stream
  uint8_t pad = 0;
};
inline int32_t crossing_dx(const Crossing &c) { return c.axis == 0 ? 1 : 0; }
inline int32_t crossing_dy(const Crossing &c) { return c.axis == 0 ? 0 : 1; }

const std::vector<Crossing> &crossings_list(Ctx &ctx);
// the index of the crossing holding tile (x, y) (and which tile: 0 / 1 into *part), -1 when none
int32_t crossing_at(Ctx &ctx, int32_t x, int32_t y, int32_t *part = nullptr);
// a water or stream tile with no path bit and no crossing (the tiles a bridge may cover)
bool crossing_tile_open(Ctx &ctx, int32_t x, int32_t y);
// the pair of tiles a click on (x, y) builds along `axis` (the clicked tile first, else second), false when neither fits
bool crossing_fit(Ctx &ctx, int32_t x, int32_t y, int32_t axis, Crossing &out);
// the pair a click on (x, y) builds: the preferred axis first, then the other
bool crossing_choose(Ctx &ctx, int32_t x, int32_t y, int32_t prefer_axis, Crossing &out);
// builds `c` (fitted): the tiles' flag bits, setGrids(code 0x93, `price_each`) per tile (the undo journal notes both), the table
void crossing_build(Ctx &ctx, const Crossing &c, int32_t price_each);
void crossing_add(Ctx &ctx, const Crossing &c);
bool crossing_erase(Ctx &ctx, int32_t index);
void crossings_clear(Ctx &ctx);
// the bridge row's last one-tile style (the two-tile item builds with it)
int32_t crossings_style(Ctx &ctx);
void crossings_set_style(Ctx &ctx, int32_t style);
// the axis a click prefers: Tab turns it as it turns a building (the orientation word g_ui_5a34e4[3], which Tab adds 1 to), on
// screen: (orientation + g_cameraRot / 2) & 1, so one Tab swaps the two screen axes at every camera rotation
int32_t crossings_pref_axis(Ctx &ctx);
// P11D redo (tick_frame.inc P11_Redo): while it replays a pair's first tile, the pair's axis (-1 none)
extern thread_local int32_t t_crossings_redo_axis;

// demolishObject on a tile of a pair: the pair leaves the table and (outside an undo) its other tile is demolished too
extern thread_local bool t_crossings_in_undo;
void crossings_on_demolish(Ctx &ctx, int32_t x, int32_t y);

// ---- the sidecar: `<save without its extension>.sgx` next to the .sve / world file, written by saveGame, read by loadGame
std::string crossings_sidecar_path(const std::string &save_path);
std::vector<uint8_t> crossings_encode(Ctx &ctx);
bool crossings_decode(Ctx &ctx, const std::vector<uint8_t> &bytes);   // false (table emptied) on a malformed file
// after a save of `save_path`: the sidecar written (or a stale one removed when the table is empty)
void crossings_after_save(Ctx &ctx, const std::string &save_path);
// after a load of `save_path`: the table read from its sidecar, empty without one
void crossings_after_load(Ctx &ctx, const std::string &save_path);
// a save of `save_path` deleted: its sidecar too
void crossings_after_delete(Ctx &ctx, const std::string &save_path);

}  // namespace portsim
