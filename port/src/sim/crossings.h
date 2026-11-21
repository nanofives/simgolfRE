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
//
// P39A3 (log/roadmap/P39/P39A3_notes.md), play switch layPath.gapBridge (1 = the original): the Pathway (object tool 0) or the path
// terrain tool 0xa0, laying a path next to a gap of 1 or 2 tiles whose lowest corner (cornerHeights) lies at least kGapLevels below
// both banks' facing edges, asks first (the game's yes / no popup with the bridge's price) and on yes builds a gap bridge: the gap
// tiles get the path bit only (no scenic thought), the build code 0x93 with a refund, and an entry here {type gap, axis, span, deck
// level = the lower bank edge}. The deck and the walkers on it are drawn at the deck level (drawing only); the slope complaint 0x2b
// does not fire on it and P39A2's stairs leave it alone. Removal and undo as the two-tile bridge; saved in the same sidecar.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "portsim/registry.h"

namespace portsim {

extern int g_defect_two_tile_bridge;   // scenicBridge.twoTile (1 = the original)
inline bool crossings_on() { return !SG_DEFECT(g_defect_two_tile_bridge); }
extern int g_defect_gap_bridge;        // layPath.gapBridge (1 = the original)
inline bool gap_bridges_on() { return !SG_DEFECT(g_defect_gap_bridge); }
// either switch: the table is kept, saved and drawn
inline bool crossings_any_on() { return crossings_on() || gap_bridges_on(); }

constexpr int32_t kScenicBridgeTool = 0x13;   // f_w_4c2848[3]: the Scenic Bridge palette tool
constexpr int32_t kTwoTileItem = 8;           // the bridge row's ninth item: [unk_0056a514] == 8 with the bridge tool
constexpr int32_t kBridgeStyles = 8;          // the one-tile styles 0..7 (sheets 0x226 + style)
constexpr int32_t kGapLevels = 3;             // the gap's lowest corner is at least this many levels below both bank edges
constexpr int32_t kGapMaxSpan = 2;            // gaps of 1 or 2 tiles get a bridge; wider ones follow the ground (P39A2's stairs)
constexpr int32_t kGapLevelCharge = 10;       // the depth charge: units of $100 per level of depth per gap tile ($1,000)

enum : uint8_t { kCrossingTwoTile = 0, kCrossingGap = 1 };

// one crossing: tiles (x, y) + k (dx, dy) for k < span, dx / dy = (1, 0) for axis 0, (0, 1) for axis 1
struct Crossing {
  int16_t x = 0, y = 0;
  uint8_t axis = 0;    // 0: along x, 1: along y
  uint8_t style = 0;   // the art (sheet 0x226 + style)
  uint8_t kind = 0;    // two-tile bridge: bit 0 / 1 the first / second tile is a stream
  uint8_t type = kCrossingTwoTile;
  uint8_t span = 2;    // tiles: 2 for the two-tile bridge, 1 or 2 for a gap bridge
  uint8_t deck = 0;    // gap bridge: the deck's height level (the lower of the two bank edges)
  uint16_t extra = 0;  // gap bridge: the part of the price the tiles' refund bytes (int8) cannot hold, paid back with them
};
inline int32_t crossing_dx(const Crossing &c) { return c.axis == 0 ? 1 : 0; }
inline int32_t crossing_dy(const Crossing &c) { return c.axis == 0 ? 0 : 1; }

const std::vector<Crossing> &crossings_list(Ctx &ctx);
// the index of the crossing holding tile (x, y) (and which tile: 0 .. span - 1 into *part), -1 when none
int32_t crossing_at(Ctx &ctx, int32_t x, int32_t y, int32_t *part = nullptr);
// the index of the gap bridge holding tile (x, y), -1 when none
int32_t crossing_gap_at(Ctx &ctx, int32_t x, int32_t y);
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

// demolishObject on a tile of a crossing: the crossing leaves the table, a gap bridge's `extra` is paid back, and (outside an undo)
// its other tiles are demolished too
extern thread_local bool t_crossings_in_undo;
void crossings_on_demolish(Ctx &ctx, int32_t x, int32_t y);

// ---- P39A3 gap bridges
// the height level of bank tile (bx, by)'s edge facing direction (dx, dy): the lower of its two corners there (cornerHeights, so a
// flattened type answers its flattened level). A tile's corners 1, 3, 5, 7 are the vertices (x+1, y-1), (x+1, y), (x, y), (x, y-1)
// (heightBlend's vertex grid), so +x is corners 1 / 3, -x 5 / 7, +y 3 / 5, -y 1 / 7.
int32_t gap_edge_level(Ctx &ctx, int32_t bx, int32_t by, int32_t dx, int32_t dy);
// the highest / lowest corner of tile (x, y)
int32_t gap_tile_top(Ctx &ctx, int32_t x, int32_t y);
int32_t gap_tile_bottom(Ctx &ctx, int32_t x, int32_t y);
// the gap bridge laying a path on (x, y) would link: (x, y) a bank with the far bank already a path, or (x, y) a gap tile with a
// path on either bank; the shortest span first, axis x before y. False when none
bool gap_find(Ctx &ctx, int32_t x, int32_t y, Crossing &out);
// its price in units of $100: twice the Scenic Bridge's word (f_tab_buildItems_4c26b0 + 0x12 of tool 0x13) plus kGapLevelCharge per
// level of depth (deck - the gap's lowest corner) per gap tile
int32_t gap_price(Ctx &ctx, const Crossing &c);
// builds `c`: per gap tile the path bit and setGrids(code 0x93, refund = min(price / span, 127)), the remainder into `extra`
void gap_build(Ctx &ctx, Crossing c, int32_t price);
// the whole question for a path about to be laid on (x, y) (H2_Build tool 0, H2_TerrainSpecial 0xa0): 0 no gap (or declined, or
// refused for money: lay the path as before), 1 a bridge built and charged (then lay the path on (x, y)), 2 a bridge built that holds
// (x, y) itself (nothing more to lay there)
int32_t gap_try(Ctx &ctx, int32_t x, int32_t y);
// P11D redo replays a gap bridge's step without the question
extern thread_local bool t_gap_auto_yes;
// drawing only: the pixels a point (wx, wy) on a gap bridge is raised by to stand on the deck (the deck level's height minus the
// ground's, as worldToScreen applies them; 0 off a gap bridge or with the switch at 1). drawWalkers and the deck art use it.
int32_t gap_deck_lift(Ctx &ctx, int32_t wx, int32_t wy);
// the popup's question: "Build a bridge across this gap for <money sign>N" (English; lang/simgolf_es.txt translates it), the game's "?" and
// option lines follow it
std::string gap_question(int32_t price);

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
