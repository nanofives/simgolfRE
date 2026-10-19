// Golfers, walkers, animals and habitats (see portsim/sim/golfers.h). Phase 3.3 + 3.6, written from the specification
// (analysis notes, hook sources as reading reference, vector corpus); no original address or code.
#include "portsim/sim/golfers.h"

#include <initializer_list>
#include <utility>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/bridge.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   appendPairTrackPoint 1 = point 0 also takes the ball position, 2 = count stops at 32
//   addGolferPair        1 = partner snapshot of the golfer itself, 2 = count left unchanged
//   resetGolfer          1 = hole byte kept, 2 = dedication tile_y freed instead of tile_x
//   golferAge            1 = bit 2 adds 40
//   typeBit7Clear        1 = tests bit 6
//   appendEndearment     1 = phrases 1 and 2 swapped
//   appendWalkerName     1 = the two table strings in the other order, 2 = flag bit 2 instead of bit 3
//   spawnObject          1 = row and column swapped, 2 = sprite base 0x14d
//   spawnWalker          1 = direction without the -2, 2 = start offset 0x400
//   addHabitatSlot       1 = state -10, 2 = link 0
//   placeAnimal          1 = on screen the frame never wraps, 2 = the wait is not counted down
//   tickAnimals          1 = the last record skipped
//   placeHabitat         1 = only flag bit 0x20 cleared (mask 0xdf)
//   updateWalkers        1 = a sale adds 3 to the cash, 2 = a leaving walker moves at speed 4,
//                        3 = the walking sequence wraps after 9
//   initGolferState      1 = one golfer per open hole
//   addGolferToTable     1 = speed / 4, 2 = the forced event bit 0 is not cleared
//   loadGolfPro          1 = sets flag 0x100000 instead of 0x200000
//   resetGolferState     1 = par 6 from length 0x271 (>= instead of >)
//   composeGolferSprite  1 = look_24 / look_25 written swapped
int g_negctl_append_pair_track_point = 0;
int g_negctl_add_golfer_pair = 0;
int g_negctl_reset_golfer = 0;
int g_negctl_golfer_age = 0;
int g_negctl_type_bit7_clear = 0;
int g_negctl_append_endearment = 0;
int g_negctl_append_walker_name = 0;
int g_negctl_spawn_object = 0;
int g_negctl_spawn_walker = 0;
int g_negctl_add_habitat_slot = 0;
int g_negctl_place_animal = 0;
int g_negctl_tick_animals = 0;
int g_negctl_place_habitat = 0;
int g_negctl_update_walkers = 0;
int g_negctl_init_golfer_state = 0;
int g_negctl_add_golfer_to_table = 0;
int g_negctl_load_golf_pro = 0;
int g_negctl_reset_golfer_state = 0;
int g_negctl_compose_golfer_sprite = 0;

Ptr<Golfer> golfer(Ctx &c, int32_t g) { return state_arr(c, &GameState::g_golfers) + g; }
Ptr<ShotTrack> track(Ctx &c, int32_t p) { return state_arr(c, &GameState::g_storyPairs) + p; }
uint32_t &rng(Ctx &c) { return state_obj(c, &GameState::g_rng).field(&Random::state).ref(); }
uint32_t text_va(Ctx &c) { return state_va(c, &GameState::g_textBuffer); }

constexpr int32_t kTracks = static_cast<int32_t>(sizeof(GameState::g_storyPairs) / sizeof(ShotTrack));
constexpr int32_t kAnimals = static_cast<int32_t>(sizeof(GameState::g_animalRecords) / sizeof(Animal));
constexpr int32_t kHabitats = static_cast<int32_t>(sizeof(GameState::g_habitatSlots) / sizeof(HabitatSlot));
constexpr int32_t kDedications =
    static_cast<int32_t>(sizeof(GameState::g_scenicDedications) / sizeof(ScenicDedication));

#include "golfers_glue.inc"
}  // namespace

// ------------------------------------------------------------------------------------------------ shot tracks
void append_pair_track_point(Ctx &ctx, int32_t p) {
  const Ptr<ShotTrack> t = track(ctx, p);
  const int32_t cnt = t.field(&ShotTrack::point_count).get();
  const Ptr<Golfer> g = golfer(ctx, t.field(&ShotTrack::golfer).get());
  const bool ball = cnt != 0 || SG_NEGCTL(g_negctl_append_pair_track_point) == 1;
  (t.field(&ShotTrack::point_x) + cnt).set(ball ? g.field(&Golfer::ball_x).get() : g.field(&Golfer::shot_start_x).get());
  (t.field(&ShotTrack::point_y) + cnt).set(ball ? g.field(&Golfer::ball_y).get() : g.field(&Golfer::shot_start_y).get());
  (t.field(&ShotTrack::point_z) + cnt).set(ball ? g.field(&Golfer::ball_z).get() : 0);
  const int32_t lim = SG_NEGCTL(g_negctl_append_pair_track_point) == 2 ? 0x20 : 0x1f;
  if (cnt < lim) t.field(&ShotTrack::point_count).set(static_cast<int16_t>(cnt + 1));
}

void add_golfer_pair(Ctx &ctx, int32_t g) {
  int32_t slot = 0;
  while (track(ctx, slot).field(&ShotTrack::golfer).get() != -1)
    if (++slot >= kTracks) return;
  const Ptr<ShotTrack> t = track(ctx, slot);
  const Ptr<Golfer> gr = golfer(ctx, g);
  t.field(&ShotTrack::golfer).set(static_cast<int16_t>(g));
  t.field(&ShotTrack::partner).set(gr.field(&Golfer::partner).get());
  if (SG_NEGCTL(g_negctl_add_golfer_pair) != 2) t.field(&ShotTrack::point_count).set(0);
  append_pair_track_point(ctx, slot);
  rep_movs(ctx.mem, t.field(&ShotTrack::golfer_snapshot).va(), gr.va(), sizeof(Golfer));
  const int32_t partner = SG_NEGCTL(g_negctl_add_golfer_pair) == 1 ? g : gr.field(&Golfer::partner).get();
  rep_movs(ctx.mem, t.field(&ShotTrack::partner_snapshot).va(), golfer(ctx, partner).va(), sizeof(Golfer));
}

// ------------------------------------------------------------------------------------------------ golfer leaves
void reset_golfer(Ctx &ctx, int32_t g) {
  const Ptr<Golfer> r = golfer(ctx, g);
  r.field(&Golfer::strokes).set(0);
  r.field(&Golfer::state).set(0);
  if (SG_NEGCTL(g_negctl_reset_golfer) != 1) r.field(&Golfer::hole).set(0);
  const int16_t type = r.field(&Golfer::type).get();
  const Ptr<ScenicDedication> d = state_arr(ctx, &GameState::g_scenicDedications);
  for (int32_t i = 0; i < kDedications; ++i) {
    if ((d + i).field(&ScenicDedication::golfer_type).get() != type) continue;
    if (SG_NEGCTL(g_negctl_reset_golfer) == 2)
      (d + i).field(&ScenicDedication::tile_y).set(-1);
    else
      (d + i).field(&ScenicDedication::tile_x).set(-1);
  }
}

int32_t golfer_age(Ctx &ctx, int32_t g) {
  const Ptr<Golfer> r = golfer(ctx, g);
  int32_t v = r.field(&Golfer::type).get() % 10;
  const uint16_t f = r.field(&Golfer::traits).get();
  if (f & 1) v += 20;
  if (f & 2) v += 30;
  if (f & 4) v += SG_NEGCTL(g_negctl_golfer_age) == 1 ? 40 : 45;
  return v;
}

int32_t type_bit7_clear(Ctx &ctx, int32_t g) {
  const int16_t type = golfer(ctx, g).field(&Golfer::type).get();
  const uint8_t b = (state_arr(ctx, &GameState::g_golferTypes) + type).field(&GolferType::demographics).get();
  return (b & (SG_NEGCTL(g_negctl_type_bit7_clear) == 1 ? 0x40 : 0x80)) ? 0 : 1;
}

// ------------------------------------------------------------------------------------------------ names
void append_endearment(Ctx &ctx, int32_t n) {
  // byte offsets of the four phrases in f_strpool_4e1b5c, selected by n & 3
  static constexpr uint32_t kPhrase[4] = {0x1c, 0x14, 0x0c, 0x00};
  uint32_t k = u32(n) & 3u;
  if (SG_NEGCTL(g_negctl_append_endearment) == 1 && (k == 1 || k == 2)) k = 3 - k;
  vstrcat(ctx.mem, text_va(ctx), state_va(ctx, &GameState::f_strpool_4e1b5c) + kPhrase[k]);
}

void append_walker_name(Ctx &ctx, int32_t index) {
  const Ptr<Walker> w = state_arr(ctx, &GameState::g_walkers) + index;
  const Ptr<char> name = w.field(&Walker::name);
  if (name.get() != 0) {
    vstrcat(ctx.mem, text_va(ctx), name.va());
    return;
  }
  const int8_t kind = w.field(&Walker::kind).get();
  const uint8_t flags = w.field(&Walker::flags).get();
  const uint8_t bit = SG_NEGCTL(g_negctl_append_walker_name) == 2 ? 4 : 8;
  const int32_t slot = sub32((flags & bit) ? 1 : 0, add32(mul32(2, kind), 4));
  if (static_cast<uint8_t>(kind) == 0xfa) {
    vstrcat(ctx.mem, text_va(ctx), state_arr(ctx, &GameState::g_golferTypes).field(&GolferType::name).va());
    return;
  }
  // two pointer tables of 10 entries each in f_ptrtab_4c1464: the second table first, then the first
  const Ptr<uint32_t> tab = state_arr(ctx, &GameState::f_ptrtab_4c1464);
  uint32_t first = (tab + 10 + slot).get(), second = (tab + slot).get();
  if (SG_NEGCTL(g_negctl_append_walker_name) == 1) std::swap(first, second);
  vstrcat(ctx.mem, text_va(ctx), first);
  vstrcat(ctx.mem, text_va(ctx), second);
}

// ------------------------------------------------------------------------------------------------ spawning
void spawn_object(Ctx &ctx, int32_t lot, int32_t kind, int32_t a, int32_t b) {
  const Ptr<Animal> animals = state_arr(ctx, &GameState::g_animalRecords);
  int32_t idx = 0;
  while ((animals + idx).field(&Animal::type).get() != -1)
    if (++idx >= kAnimals) return;
  const Ptr<Animal> r = animals + idx;
  const int8_t cat = (state_arr(ctx, &GameState::g_celebrities) + kind).field(&CelebrityRecord::category).get();
  r.field(&Animal::u00).set(0);
  r.field(&Animal::kind).set(static_cast<int16_t>(lot));
  r.field(&Animal::type).set(static_cast<int16_t>(cat));
  r.field(&Animal::x).set(static_cast<int16_t>(a));
  r.field(&Animal::y).set(static_cast<int16_t>(b));
  r.field(&Animal::sprite_base).set(SG_NEGCTL(g_negctl_spawn_object) == 2 ? 0x14d : 0x140);
  const Ptr<int8_t> map = (state_arr(ctx, &GameState::g_recordBanks) + lot).field(&SgRecordBank::occupancy);
  bool any = false;
  for (int32_t i = 0; i < 4 && !any; ++i)
    for (int32_t j = 0; j < 4 && !any; ++j) any = static_cast<uint8_t>(map[i * 6 + j]) == 0xff;
  if (!any) throw NoReturn{"spawnObject"};
  int32_t row, col;
  do {
    row = i32(random_range(rng(ctx), 4) & 0xffffu);
    col = i32(random_range(rng(ctx), 4) & 0xffffu);
  } while (static_cast<uint8_t>(map[row * 6 + col]) != 0xff);
  if (SG_NEGCTL(g_negctl_spawn_object) == 1) std::swap(row, col);
  r.field(&Animal::sub_x).set(((row << 10) + 0x200) / 2);
  r.field(&Animal::sub_y).set(((col << 10) + 0x200) / 2);
}

int32_t spawn_walker(Ctx &ctx, int32_t kind) {
  const Ptr<Walker> walkers = state_arr(ctx, &GameState::g_walkers);
  int32_t idx = 0;
  while ((walkers + idx).field(&Walker::flags).get() != 0) ++idx;  // no bound (model_defects#11)
  const Ptr<Walker> w = walkers + idx;
  for (uint32_t i = 0; i < sizeof(Walker); i += 4) ctx.mem.store<uint32_t>(w.va() + i, 0);
  const int32_t r1 = i32(random_range(rng(ctx), 5));
  const Ptr<PlacedObject> o = state_arr(ctx, &GameState::g_placed_objects);
  const int32_t off = SG_NEGCTL(g_negctl_spawn_walker) == 2 ? 0x400 : 0x600;
  w.field(&Walker::x).set(add32(shl32(o.field(&PlacedObject::x).get(), 10), off));
  w.field(&Walker::y).set(add32(shl32(o.field(&PlacedObject::y).get(), 10), off));
  const uint8_t base = static_cast<uint8_t>((state_arr(ctx, &GameState::g_holes) + 1).field(&HoleRecord::direction).get());
  const int32_t d = SG_NEGCTL(g_negctl_spawn_walker) == 1 ? base + r1 : base + r1 - 2;
  w.field(&Walker::flags).set(1);
  w.field(&Walker::facing).set(static_cast<int8_t>(d & 7));
  w.field(&Walker::kind).set(static_cast<int8_t>(kind));
  w.field(&Walker::anim_seq).set(0xb);
  w.field(&Walker::wait).set(static_cast<int16_t>(random_range(rng(ctx), 0x20)));
  w.field(&Walker::post_x).set(-1);
  return idx;
}

int32_t add_habitat_slot(Ctx &ctx, int32_t x, int32_t y, int32_t kind) {
  const Ptr<HabitatSlot> slots = state_arr(ctx, &GameState::g_habitatSlots);
  int32_t i = 0;
  for (; i < kHabitats; ++i)
    if ((slots + i).field(&HabitatSlot::kind).get() == 0xff) break;
  if (i == kHabitats) return -1;
  const Ptr<HabitatSlot> s = slots + i;
  s.field(&HabitatSlot::x).set(add32(shl32(x, 10), 0x200));
  s.field(&HabitatSlot::y).set(add32(shl32(y, 10), 0x200));
  s.field(&HabitatSlot::direction).set(static_cast<int8_t>(random_range(rng(ctx), 4)));
  s.field(&HabitatSlot::anim).set(0x10c);
  s.field(&HabitatSlot::state).set(SG_NEGCTL(g_negctl_add_habitat_slot) == 1 ? -10 : -20);
  s.field(&HabitatSlot::kind).set(static_cast<uint8_t>(kind));
  s.field(&HabitatSlot::link).set(SG_NEGCTL(g_negctl_add_habitat_slot) == 2 ? 0 : -1);
  return i;
}

// ------------------------------------------------------------------------------------------------ animals, habitats
#include "golfers_world.inc"
#include "golfers_walkers.inc"
#include "golfers_pro.inc"

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_append_pair_track_point(Ctx &c, const uint32_t *a, Ret &) { append_pair_track_point(c, S(a, 0)); }
void a_add_golfer_pair(Ctx &c, const uint32_t *a, Ret &) { add_golfer_pair(c, S(a, 0)); }
void a_reset_golfer(Ctx &c, const uint32_t *a, Ret &) { reset_golfer(c, S(a, 0)); }
void a_golfer_age(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(golfer_age(c, S(a, 0))); }
void a_type_bit7_clear(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(type_bit7_clear(c, S(a, 0))); }
void a_append_endearment(Ctx &c, const uint32_t *a, Ret &) { append_endearment(c, S(a, 0)); }
void a_append_walker_name(Ctx &c, const uint32_t *a, Ret &) { append_walker_name(c, S(a, 0)); }
void a_spawn_object(Ctx &c, const uint32_t *a, Ret &) { spawn_object(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3)); }
void a_spawn_walker(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(spawn_walker(c, S(a, 0))); }
void a_add_habitat_slot(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(add_habitat_slot(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_place_animal(Ctx &c, const uint32_t *a, Ret &) { place_animal(c, S(a, 0)); }
void a_tick_animals(Ctx &c, const uint32_t *, Ret &) { tick_animals(c); }
void a_place_habitat(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(place_habitat(c, S(a, 0), S(a, 1), S(a, 2))); }
// updateWalkers and initGolferState take no argument; their corpus keys declare one int slot (ignored)
void a_update_walkers(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(update_walkers(c)); }
void a_init_golfer_state(Ctx &c, const uint32_t *, Ret &) { init_golfer_state(c); }
void a_add_golfer_to_table(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(add_golfer_to_table(c, S(a, 0))); }
void a_load_golf_pro(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(load_golf_pro(c)); }
void a_reset_golfer_state(Ctx &c, const uint32_t *, Ret &) { reset_golfer_state(c); }
void a_compose_golfer_sprite(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(compose_golfer_sprite(c, S(a, 0))); }

const FuncDef kFuncs[] = {
    {"appendPairTrackPoint", "default", "void", 1, 0, a_append_pair_track_point, &g_negctl_append_pair_track_point},
    {"addGolferPair", "default", "void", 1, 0, a_add_golfer_pair, &g_negctl_add_golfer_pair},
    {"resetGolfer", "default", "void", 1, 0, a_reset_golfer, &g_negctl_reset_golfer},
    {"golferAge", "default", "int", 1, 0, a_golfer_age, &g_negctl_golfer_age},
    {"typeBit7Clear", "default", "int", 1, 0, a_type_bit7_clear, &g_negctl_type_bit7_clear},
    {"appendEndearment", "default", "void", 1, 0, a_append_endearment, &g_negctl_append_endearment},
    {"appendWalkerName", "default", "void", 1, 0, a_append_walker_name, &g_negctl_append_walker_name},
    {"spawnObject", "default", "void", 4, 0x027F, a_spawn_object, &g_negctl_spawn_object},
    {"spawnWalker", "default", "int", 1, 0x027F, a_spawn_walker, &g_negctl_spawn_walker},
    {"addHabitatSlot", "default", "int", 3, 0x027F, a_add_habitat_slot, &g_negctl_add_habitat_slot},
    {"placeAnimal", "default", "void", 1, 0x027F, a_place_animal, &g_negctl_place_animal},
    {"tickAnimals", "default", "void", 0, 0x027F, a_tick_animals, &g_negctl_tick_animals},
    {"placeHabitat", "default", "uint32", 3, 0x027F, a_place_habitat, &g_negctl_place_habitat},
    {"updateWalkers", "default", "int", 1, 0x027F, a_update_walkers, &g_negctl_update_walkers},
    {"initGolferState", "default", "void", 1, 0x027F, a_init_golfer_state, &g_negctl_init_golfer_state},
    {"addGolferToTable", "default", "int", 1, 0x027F, a_add_golfer_to_table, &g_negctl_add_golfer_to_table},
    {"loadGolfPro", "default", "int", 1, 0x027F, a_load_golf_pro, &g_negctl_load_golf_pro},
    {"resetGolferState", "default", "void", 0, 0x027F, a_reset_golfer_state, &g_negctl_reset_golfer_state},
    {"composeGolferSprite", "default", "int", 1, 0, a_compose_golfer_sprite, &g_negctl_compose_golfer_sprite},
};
}  // namespace

extern const ModuleDef kModule_golfers = {"golfers", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
