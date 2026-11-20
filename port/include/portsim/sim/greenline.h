// P39C (re/roadmap/P39_COURSE_FEATURES.md "P39C"; log/roadmap/P39/P39C_notes.md): the whole green outlined while the player aims.
// The original marks the player's hole while aiming (g_panelListMode == 3) with P1_flag80's blinking sprite on the cup tile only
// (tick_frame.inc P1_flag80: g_sprite5423e8 on the tile whose flag 0x80 is set and whose low 5 flag bits equal the player's
// golfer +0x21 byte). With the app option --green-highlight on, that sprite is replaced by an outline of the cup's whole green:
// the green tiles (type 1) reached 8-connected from the cup tile and its 3 x 3 neighbours (P22B_OnHoleGreen's rule), a green
// that holds several cups split by the nearer cup centre; each boundary side of those tiles is projected with worldToScreen
// (inside the tile, so the tile's corner heights carry it over the slope) and drawn with drawMapLine. Drawing only: nothing
// here writes the GameState; the outline is cached per hole and rebuilt when a green or cup tile changes.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "portsim/sim/commands.h"

namespace portsim {

// the app option (--green-highlight; off by default in the library, on by default in the app)
void green_highlight_set(bool on);
bool green_highlight_on();

// one boundary side of a green tile, in world units (1024 a tile), both ends inside that tile
struct GreenEdge {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
struct GreenOutline {
  int32_t hole = 0;
  std::vector<int32_t> tiles;     // flat tile indices x * N + y, ascending
  std::vector<GreenEdge> edges;   // per tile in `tiles` order: sides x-, x+, y-, y+ whose neighbour is not in the set
};

// the hole the original marks now: the player's golfer +0x21 byte while g_panelListMode == 3 and g_playerGolfer != -1, else 0
int32_t green_highlight_hole(Ctx &ctx);
// the marker's blink: bit 2 of g_date's low byte or of g_flags' low byte (P1_flag80's test)
bool green_highlight_blink(Ctx &ctx);
// the outline of hole h's green, built now (no cache)
GreenOutline green_outline_build(Ctx &ctx, int32_t hole);
// the same, cached on this thread until the hole, the world size or a green / cup tile changes
const GreenOutline &green_outline(Ctx &ctx, int32_t hole);
// the screen lines of an outline in the marker's colour (sink(x1, y1, x2, y2, colour, width, alpha), drawMapLine's arguments);
// sides wholly off the view are skipped. Returns the number of lines given to the sink.
using GreenLineSink = std::function<void(int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t)>;
// P39D: wider > 0 draws the lines that many pixels wider (the aim's snap to the cup)
int32_t green_outline_lines(Ctx &ctx, const GreenOutline &o, const GreenLineSink &sink, int32_t wider = 0);

// the marker's look: g_sprite5423e8 drawn through the g_color16Lut shade table lightens the cup tile towards white (measured on
// the presented frame: (148, 206, 99) -> (184, 237, 126), P39C_notes.md); the line is white (555 word 0x7fff) at the terrain
// tool's mark alpha 7 of 10
// P39D (play switch mainLoop.aimHalfTileLattice = 0): the world point (px, py) whose worldToScreen is the local point (mx, my),
// solved from tile (cx, cy)'s centre by Newton steps on worldToScreen's exact projection (finite differences of 64 world units,
// steps capped at 1.5 tiles, kept on the map); (rx, ry) gets it before the snap. Within aim_free_snap_radius of hole h's cup centre
// ((cup tile << 10) + 0x200, g_holes +0x18 / +0x1c) and on that hole's green (green_outline's tiles) the point becomes the cup
// centre and snap is set. Reads only.
void aim_free_point(Ctx &ctx, double mx, double my, int32_t cx, int32_t cy, int32_t hole, int32_t &px, int32_t &py, bool &snap,
                    int32_t *rx = nullptr, int32_t *ry = nullptr);
// the snap radius in world units at the current zoom: max(0x200, 0x800 / g_zoomScale), the same size on screen at zooms 1, 2 and 4
// (half a tile at zoom 4), half a tile at zoom 8 (log/roadmap/P39/P39D_notes.md, the drive)
int32_t aim_free_snap_radius(Ctx &ctx);

constexpr uint32_t kGreenLineColour = 0x80007fffu;

}  // namespace portsim
