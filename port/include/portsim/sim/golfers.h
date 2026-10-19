// Golfers, walkers, animals and habitats (phase 3.3 + 3.6, 2026-10-19): golfer record leaves, the shot-track table,
// walker / animal / habitat spawning and their per-frame updates.
// Specifications: re/analysis/golfer/<addr>_<name>.md and re/analysis/sim/<addr>_<name>.md of each function,
// re/analysis/data/golfer_record.md and record_tables.md, the hook sources (reading reference) and the vector corpus.
// Records: g_golfers 156 x 0x100, g_walkers 64 x 0x4c, g_animalRecords 100 x 0x3c, g_habitatSlots 128 x 0x14,
// g_storyPairs (the shot-track table) 16 x 0x388. Indices are NOT bound-checked unless stated (as in the original).
#ifndef PORTSIM_SIM_GOLFERS_H
#define PORTSIM_SIM_GOLFERS_H

#include <cstdint>

#include "portsim/registry.h"
#include "portsim/sim/bridge.h"  // world_to_screen, play_sound_at, queue_message, points_popup (module bridge)

namespace portsim {

// appendPairTrackPoint: track p gets point[point_count] = the golfer's ball (x, y, z) or, for point 0, its shot start
// (x, y, 0); point_count += 1 while it is below 31 (the 32nd point is overwritten from then on).
void append_pair_track_point(Ctx &ctx, int32_t p);
// addGolferPair: claims the first free track (golfer word -1; none free: nothing), stores golfer g, its partner word and
// count 0, appends point 0, then snapshots the golfer record and its partner's record (0x100 bytes each).
void add_golfer_pair(Ctx &ctx, int32_t g);
// resetGolfer: clears the golfer's strokes, state and hole bytes and frees (tile_x = -1) every scenic dedication of
// the golfer's type.
void reset_golfer(Ctx &ctx, int32_t g);
// golferAge: type % 10 (signed) + 20 / 30 / 45 for bits 0 / 1 / 2 of the golfer's traits word.
int32_t golfer_age(Ctx &ctx, int32_t g);
// typeBit7Clear: 1 when bit 7 (female) of the demographics byte of the golfer's type record is clear.
int32_t type_bit7_clear(Ctx &ctx, int32_t g);
// appendEndearment: strcat of one of four fixed phrases (n & 3) onto g_textBuffer.
void append_endearment(Ctx &ctx, int32_t n);
// appendWalkerName: strcat of walker `index`'s rename text, or (empty) the default name: kind -6 the player's name
// (golfer type 0's name), any other kind the two table strings of slot ((flags >> 3) & 1) - (2 * kind + 4).
void append_walker_name(Ctx &ctx, int32_t index);
// spawnObject: fills the first free animal record (type -1; none: nothing) and draws (row, col) = Random::range(4) x 2
// until the occupancy cell row * 6 + col of record bank `lot` is free (0xff). Throws NoReturn when no cell of rows and
// columns 0..3 is free (the original spins forever).
void spawn_object(Ctx &ctx, int32_t lot, int32_t kind, int32_t a, int32_t b);
// spawnWalker: claims the first walker slot whose flags byte is 0 (no bound: model_defects#11), zeroes it and starts
// it at placed object 0 + (1.5, 1.5) tiles; returns the slot index.
int32_t spawn_walker(Ctx &ctx, int32_t kind);
// addHabitatSlot: first habitat slot with kind 0xff (none: -1) at tile (x, y) centre, state -20, anim 0x10c,
// direction Random::range(4), link -1; returns its index.
int32_t add_habitat_slot(Ctx &ctx, int32_t x, int32_t y, int32_t kind);

// placeAnimal: one frame of animal idx: on screen (worldToScreen) its frame cycles and g_textBuffer gets "x/y" of its
// sub-tile position; off screen and not paused a 1-in-500 roll sets wait 500; a wait counts down; idle animals turn and
// walk 16 steps inside their 4 x 4 lot, or play their sound when the turn points out of it. A frame count 0 faults (idiv).
void place_animal(Ctx &ctx, int32_t idx);
// tickAnimals: placeAnimal for every animal record whose type word is not 0xffff.
void tick_animals(Ctx &ctx);
// placeHabitat: sets the tile type of (x, y), clears flag bits 0x2100, then (1 in 8, a free random habitat slot, the
// habitat preference bit) tries up to 9 animal kinds for the neighbour tile (x2, y2) and seeds a habitat cluster there
// with its sound and message. Returns the original's eax (the last Random::range result or playHabitatSound's).
uint32_t place_habitat(Ctx &ctx, int32_t x, int32_t y, int32_t type);
// updateWalkers: one frame of the 64 walker records (staff and the player character): action timers and their effects
// (thoughts, sales), target search (worked tiles, balls, golfers by kind), the path step (pathStep), keeping clear of
// earlier walkers, the step itself. Returns 0x40 (the loop counter left in eax).
int32_t update_walkers(Ctx &ctx);
// initGolferState: clears the hole byte of golfers 0..151 and the arrival / slot counters, then for each open hole from 18
// down draws two golfers (updateMembership(1)) onto it with consecutive arrival orders and each other as partner (r ^ 1).
void init_golfer_state(Ctx &ctx);
// addGolferToTable: per ball record one range draw (when the record is live) and holeQuadrants of golfer g's ball, a
// pending forced event bit 0 emits thought 0x18; then copies g's ball state into the first free ball record (none: returns
// 256); returns the sign bit of the vz / 3 multiply (the original's eax).
int32_t add_golfer_to_table(Ctx &ctx, int32_t g);
// loadGolfPro: builds a tournament field: backs up the golfer types and phrases, clears every golfer's hole and partner,
// then for each open hole 1..18 fills golfers 2h - 2 and 2h - 1 (golfer 1 is the player with g_playerSkills; the others
// draw a pro by a rejection loop and load its file through loadThemeFile into golfer type g, golfer 0 using type 1).
// Sets g_flags bit 0x200000 and returns g_flags. Divides by 4 - g_difficulty (faults at difficulty 4). Throws NoReturn
// when no pro record 1..99 is filled (every draw is rejected).
int32_t load_golf_pro(Ctx &ctx);
// resetGolferState: removes every placed object of type 0x11 / 0x12 (clearObjectFootprint), restores the golfer types and
// phrases from their backups, re-derives the par of ONE hole record (g_holesPlusOne's, 18 times: the original's loop
// never advances) from its length, clears g_tournamentPrep and runs initGolferState.
void reset_golfer_state(Ctx &ctx);
// composeGolferSprite (simulation part only): for g < 0x9c chooses the look indices (from the type number, the body
// table, the custom record when edited, the pro record for a pro golfer) and writes them back into the golfer's type
// record (+0x23 low nibble, +0x24..+0x28). The layers themselves are drawing and are not ported; returns 0.
int32_t compose_golfer_sprite(Ctx &ctx, int32_t g);

}  // namespace portsim

#endif  // PORTSIM_SIM_GOLFERS_H
