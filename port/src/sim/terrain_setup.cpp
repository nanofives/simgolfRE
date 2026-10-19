// New-course setup routines of the terrain module (see portsim/sim/terrain.h): buildGolferBioText and
// customizeCharacter. Phase 3.1a, written from the specification (re/analysis/golfer/004315e0_buildGolferBioText.md,
// re/analysis/frontend/0040f190_customizeCharacter.md, the C3 hook sources as reading reference, the vector corpus).
// Callees other children own (clearBuffers, initRecords, loadThemeFile: 3.1b; resetGolfer: 3.3; updateMembership: 3.2)
// are called through the registry by their hooks.csv names. Strings are (pool member, byte offset) pairs.
#include <initializer_list>

#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/terrain.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   buildGolferBioText   1 = the ball records keep their x, 2 = the colour clause is drawn with Random::range(9)
//   customizeCharacter   1 = g2's facing is not the start walker's byte
int g_negctl_bio_text = 0;
int g_negctl_customize = 0;

// A function another module registers, called with the original's argument slots; MissingResource when absent.
Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  uint32_t a[8] = {};
  int k = 0;
  for (uint32_t v : args) a[k++] = v;
  Ret r;
  registry_at(i, nullptr)->fn(ctx, a, r);
  return r;
}
bool registered(const char *name) { return registry_find(name) >= 0; }

// `rep stosd` of n dwords of v at the virtual address va (the original's fills: an overrun lands where its did)
void fill_dwords(Ctx &ctx, uint32_t va, uint32_t n, uint32_t v) {
  for (uint32_t i = 0; i < n; ++i) ctx.mem.store<uint32_t>(va + 4 * i, v);
}
uint32_t rnd(Ctx &ctx, uint32_t n) {
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  return random_range(seed, n) & 0xFFFFu;
}
Ptr<uint8_t> type_bytes(Ctx &ctx, int32_t r) { return (state_arr(ctx, &GameState::g_golferTypes) + r).as<uint8_t>(); }

// the class of golfer type r from its personality and demographics bytes
int32_t type_class(Ctx &ctx, uint32_t r) {
  const Ptr<GolferType> t = state_arr(ctx, &GameState::g_golferTypes) + i32(r);
  if (t.field(&GolferType::demographics).get() & 0x80) return 6;
  const uint8_t p = t.field(&GolferType::personality).get();
  int32_t c = 1;
  if (p & 0x11) c = 3;
  if (p & 0x0C) c |= 4;
  if (c == 1) c = ((r & 1) ? 2 : 4) | 1;
  return c;
}
constexpr int32_t kGroupClass[4] = {5, 3, 6, 7};
// byte offsets of the colour words in f_strpool_4c7728 (colour 0..9)
constexpr uint32_t kColour[10] = {0xC4, 0xBC, 0xB4, 0xAC, 0xA4, 0x98, 0x90, 0x88, 0x80, 0x78};
}  // namespace

int32_t build_golfer_bio_text(Ctx &ctx) {
  GameState &g = ctx.gs();
  const auto va = [&](auto member) { return state_va(ctx, member); };
  fill_dwords(ctx, va(&GameState::g_objectTypeLevel), 0x14, 0);
  fill_dwords(ctx, va(&GameState::g_activeObjectLevel), 0x14, 0);
  fill_dwords(ctx, va(&GameState::tile_anim_frame), 0x271, 0);
  fill_dwords(ctx, va(&GameState::g_golfers), 0x2700, 0);
  fill_dwords(ctx, va(&GameState::g_holes), 0xA28, 0);
  fill_dwords(ctx, va(&GameState::g_walkers), 0x4C0, 0);
  fill_dwords(ctx, va(&GameState::g_placed_objects), 0x400, 0);
  fill_dwords(ctx, va(&GameState::g_bulletinEvents), 0x108, 0);
  for (int32_t i = 0; i < 256; ++i) g.g_placed_objects[i].type = -1;
  fill_dwords(ctx, va(&GameState::g_scenicDedications), 0xC8, 0xFFFFFFFFu);
  fill_dwords(ctx, va(&GameState::g_skyActors), 0x10, 0xFFFFFFFFu);
  fill_dwords(ctx, va(&GameState::g_tileFlags), 0x4E2, 0);
  fill_dwords(ctx, va(&GameState::g_tileVariant), 0x271, 0);
  fill_dwords(ctx, va(&GameState::g_golferTypeState), 0x39C, 0);
  fill_dwords(ctx, va(&GameState::g_ballTrailX), 0x98, 0xFFFFFFFFu);
  fill_dwords(ctx, va(&GameState::g_tileBuildCode), 0x271, 0xFFFFFFFFu);
  fill_dwords(ctx, va(&GameState::g_bestRounds), 0xA, 0);
  fill_dwords(ctx, va(&GameState::g_holePoints), 0x72, 0xFFFFFFFFu);
  ctx.mem.store<uint16_t>(va(&GameState::g_skillMaskBlock) + 8, 0);
  fill_dwords(ctx, va(&GameState::g_ballStopCount), 0x271, 0);
  call_peer(ctx, "clearBuffers", {});
  fill_dwords(ctx, va(&GameState::g_eventHistory), 0xFA, 0);
  for (int32_t i = 0; i < 152; ++i) g.g_golfers[i].partner = -1;

  // twelve arrival picks: type r = Random::range(0x4c) is taken when r % 3 == count % 3, its membership byte is
  // clear and its class is the group's (count / 3)
  for (int32_t count = 0; count < 12;) {
    // a pick the original can never make spins forever (the generator reaches every r): say so instead
    bool possible = false;
    for (uint32_t r = 0; r < 0x4C && !possible; ++r)
      possible = i32(r) % 3 == count % 3 && g.g_golferTypeState[r].membership == 0 && type_class(ctx, r) == kGroupClass[count / 3];
    if (!possible) throw NoReturn{"buildGolferBioText"};
    const uint32_t r = rnd(ctx, 0x4C);
    bool ok = i32(r) % 3 == count % 3;
    const Ptr<uint8_t> taken = (state_arr(ctx, &GameState::g_golferTypeState) + i32(r)).field(&GolferTypeState::membership);
    if (taken.get() != 0) ok = false;
    if (type_class(ctx, r) != kGroupClass[count / 3]) continue;
    if (!ok) continue;
    ++count;
    taken.set(1);
  }

  // one bio per golfer type 0..75 (bio text at +0x30 of each record)
  const uint32_t text = va(&GameState::g_textBuffer);
  const uint32_t likes = va(&GameState::f_ptrtab_4c2fd4);
  const auto like = [&](int32_t i) { return ctx.mem.load<uint32_t>(likes + 4 * u32(i)); };
  const uint32_t p7818 = va(&GameState::f_strpool_4c7818), p528c = va(&GameState::f_strpool_4c528c);
  const uint32_t p4944 = va(&GameState::f_strpool_4c4944), p77f4 = va(&GameState::f_strpool_4c77f4);
  const uint32_t p7728 = va(&GameState::f_strpool_4c7728);
  for (int32_t k = 0; k < 76; ++k) {
    const Ptr<uint8_t> rec = type_bytes(ctx, k);
    const int32_t a = i32(rnd(ctx, 0x28));
    const int32_t b = i32(rnd(ctx, 0x27)) + 1;
    vstrcpy(ctx.mem, text, rec.byte(0x10).va());
    vstrcat(ctx.mem, text, p7818 + 8);
    vstrcat(ctx.mem, text, like(a));
    vstrcat(ctx.mem, text, p528c + 0x2C);
    vstrcat(ctx.mem, text, like((a + b) % 0x28));
    vstrcat(ctx.mem, text, p7818);
    vstrcat(ctx.mem, text, like((a + 2 * b) % 0x28));
    vstrcat(ctx.mem, text, p4944);
    if (rnd(ctx, 2) != 0) {
      vstrcat(ctx.mem, text, p77f4 + (k < 57 ? 0x1C : 0x14));
      vstrcat(ctx.mem, text, p77f4);
      const uint32_t c = rnd(ctx, SG_NEGCTL(g_negctl_bio_text) == 2 ? 9 : 10);
      if (c <= 9) vstrcat(ctx.mem, text, p7728 + kColour[c]);
    }
    vstrcpy(ctx.mem, rec.byte(0x30).va(), text);
  }

  const Ptr<uint8_t> sky = state_arr(ctx, &GameState::g_skyActors).as<uint8_t>();
  sky.byte(0xD).set(0);
  sky.byte(0x1D).set(1);
  sky.byte(0x2D).set(3);
  sky.byte(0x3D).set(2);
  if (SG_NEGCTL(g_negctl_bio_text) != 1)
    for (int32_t i = 0; i < 256; ++i) g.g_ballRecords[i].x = -1;
  for (int32_t i = 0; i < 16; ++i) (state_arr(ctx, &GameState::g_storyPairs) + i).as<uint16_t>().set(0xFFFF);
  for (int32_t i = 0; i < 128; ++i) (state_arr(ctx, &GameState::g_habitatSlots) + i).as<uint8_t>().byte(0x12).set(0xFF);
  g.g_holesPlusOne = 1;
  g.g_tournamentPurse = 0;
  g.g_visitor60Count = 0;
  g.g_challengeLevel = 0;
  g.f_w_4c2848[1] = 0;
  g.g_ratingMilestone = 0;
  g.g_landPurchaseCount = 0;
  g.g_awardTournamentPoints = 0;
  g.g_awardStories = 0;
  g.g_nextArrivalOrder = 0;
  g.g_nextGolferSlot = 0;
  g.g_landingAnimFrame = -1;
  g.g_playerGolfer = -1;
  return i32(call_peer(ctx, "initRecords", {}).eax);
}

int32_t customize_character(Ctx &ctx, int32_t with_pro) {
  GameState &g = ctx.gs();
  if (registered("playSound")) call_peer(ctx, "playSound", {0x2F, 100, 0, 0, 0});  // the audio backend's: no sim state
  const uint32_t mask_va = state_va(ctx, &GameState::g_skillMaskBlock) + 8;
  uint16_t mask = ctx.mem.load<uint16_t>(mask_va);
  for (int i = 0; i < 12; ++i)
    if (g.g_playerSkills[i] != 0) mask = static_cast<uint16_t>(mask | (1 << i));
  ctx.mem.store<uint16_t>(mask_va, mask);
  // the character intro screen (UI, phase 3.7) when no skill is chosen yet
  if (mask == 0 && registered("showCharacterIntro")) call_peer(ctx, "showCharacterIntro", {});
  const int32_t pro = g.g_proChallenger;
  const int32_t cycle = g.g_nextGolferSlot;
  if (cycle & 2) g.g_nextGolferSlot = mod32(add32(cycle, 2), 0x98);
  const Ptr<Golfer> golfers = state_arr(ctx, &GameState::g_golfers);
  for (int32_t i = 0; i < 152; ++i) {
    const uint8_t st = static_cast<uint8_t>(golfers[i].hole);
    if ((st == 1 && golfers[i].strokes == 0) || st == 0xFF) call_peer(ctx, "resetGolfer", {u32(i)});
  }
  int32_t g1 = i32(call_peer(ctx, "updateMembership", {0}).eax);
  if (g1 & 1) g1 = i32(call_peer(ctx, "updateMembership", {0}).eax);
  if (g1 != -1) g.g_lastAllocatedGolfer = g1;
  const int32_t g2 = i32(call_peer(ctx, "updateMembership", {0}).eax);
  g.g_lastAllocatedGolfer = g2;
  const Ptr<Golfer> a = golfers + g1, b = golfers + g2;
  a.field(&Golfer::partner).set(static_cast<int16_t>(g2));
  b.field(&Golfer::partner).set(static_cast<int16_t>(g1));
  a.field(&Golfer::wait).set(0);
  b.field(&Golfer::wait).set(6);

  const Ptr<GolferType> t76 = state_arr(ctx, &GameState::g_golferTypes) + 76;
  const Ptr<uint8_t> t76b = t76.as<uint8_t>();
  if (with_pro != 0) {
    a.field(&Golfer::kind).set(0x20);
    a.field(&Golfer::name_rec).set(static_cast<uint8_t>(pro));
    const Ptr<uint8_t> prec = state_arr(ctx, &GameState::g_golfPros).as<uint8_t>() + mul32(pro, 0x38);
    const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
    vstrcpy(ctx.mem, text, prec.va());
    a.field(&Golfer::type).set(0x4C);
    vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c581c) + 0x12C);
    if (i32(call_peer(ctx, "loadThemeFile", {text, 0x4C, 0xFFFFFFFFu}).eax) == 0) {
      t76.field(&GolferType::demographics).set(9);
      const uint32_t ph = state_va(ctx, &GameState::g_golferPhrases) + 0x17318;
      for (uint32_t i = 0; i < 0x138 * 4 + 2; ++i) ctx.mem.store<uint8_t>(ph + i, 0);
      t76b.byte(0x24).set(prec.byte(0x23).get());
      t76b.byte(0x25).set(prec.byte(0x24).get());
      const uint8_t b0 = prec.byte(0x20).get();
      t76b.byte(0x27).set(prec.byte(0x21).get());
      t76b.byte(0x23).set(static_cast<uint8_t>(((b0 & 3) << 4) | prec.byte(0x22).get()));
      if (b0 & 4) t76.field(&GolferType::demographics).set(0x89);
      const uint32_t c = rnd(ctx, 5);
      t76b.byte(0x20).set(static_cast<uint8_t>((c & 0x1F) < 8 ? (1u << (c & 0x1F)) : 0u));
      vstrcpy(ctx.mem, t76b.byte(0x10).va(), prec.va());
      vstrcpy(ctx.mem, t76b.va(), state_va(ctx, &GameState::f_strpool_4c581c) + 0x120);
    }
    a.field(&Golfer::traits).set(static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(t76.field(&GolferType::demographics).get()))));
    a.field(&Golfer::flags).set(a.field(&Golfer::flags).get() & 0xFFFFFFE3u);
    a.field(&Golfer::cls).set(static_cast<int8_t>(clamp(g.g_difficulty, 0, 2)));
    a.field(&Golfer::prefs).set(static_cast<uint8_t>(static_cast<uint8_t>(g.g_challengeLevel) * 16 + 7));
    a.field(&Golfer::skill_mask).set(0xE0);
    for (int i = 0; i < 12; ++i) {
      const uint8_t v = prec.byte(0x25 + i).get();
      a.field(&Golfer::skill).at(i).set(v);
      if (v != 0) a.field(&Golfer::skill_mask).set(static_cast<int16_t>(a.field(&Golfer::skill_mask).get() | (1 << i)));
    }
    g.g_proChallenger = -1;
  }

  const uint32_t flags = g.g_flags;
  const Ptr<uint8_t> bflags = b.field(&Golfer::flags).as<uint8_t>();
  bflags.set(static_cast<uint8_t>(bflags.get() & 0xE3));
  b.field(&Golfer::kind).set(0x24);
  b.field(&Golfer::type).set(0);
  b.field(&Golfer::traits).set(static_cast<uint16_t>(static_cast<int16_t>(
      static_cast<int8_t>((state_arr(ctx, &GameState::g_golferTypes)).field(&GolferType::demographics).get()))));
  const int32_t date = g.g_date;
  b.field(&Golfer::hole).set(1);
  a.field(&Golfer::hole).set(1);
  b.field(&Golfer::date_stamp).set(date);
  a.field(&Golfer::date_stamp).set(date);
  b.field(&Golfer::story_id).set(-1);
  a.field(&Golfer::story_id).set(-1);
  b.field(&Golfer::pair_class).set(0);
  a.field(&Golfer::pair_class).set(0);
  g.g_flags = flags & 0xFFFF9FFFu;
  a.field(&Golfer::flags).set(a.field(&Golfer::flags).get() & 0xFFEFFFFFu);
  b.field(&Golfer::flags).set(b.field(&Golfer::flags).get() & 0xFFEFFFE3u);
  b.field(&Golfer::cls).set(2);
  b.field(&Golfer::prefs).set(0x47);
  b.field(&Golfer::skill_mask).set(static_cast<int16_t>(ctx.mem.load<uint16_t>(mask_va)));
  for (int i = 0; i < 12; ++i)
    if (static_cast<int32_t>(b.field(&Golfer::skill_mask).get()) & (1 << i))
      b.field(&Golfer::skill).at(i).set(g.g_playerSkills[i]);
  const uint32_t f2 = b.field(&Golfer::flags).get() | 0x200u;
  g.g_playerGolfer = g2;
  b.field(&Golfer::flags).set(f2);
  const Ptr<Walker> w0 = state_arr(ctx, &GameState::g_walkers);
  const uint8_t start = w0.as<uint8_t>().byte(0x16).get();
  b.field(&Golfer::x).set(w0.field(&Walker::x).get());
  b.field(&Golfer::y).set(w0.field(&Walker::y).get());
  b.field(&Golfer::wait).set(0);
  if (SG_NEGCTL(g_negctl_customize) != 1) b.field(&Golfer::facing).set(static_cast<int8_t>(start));
  return i32((f2 & 0xFFFFFF00u) | start);
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_bio_text(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(build_golfer_bio_text(c)); }
void a_customize(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(customize_character(c, static_cast<int32_t>(a[0]))); }

const FuncDef kFuncs[] = {
    {"buildGolferBioText", "default", "int", 1, 0, a_bio_text, &g_negctl_bio_text},
    {"customizeCharacter", "default", "int", 1, 0, a_customize, &g_negctl_customize},
};
}  // namespace

extern const ModuleDef kModule_terrain_setup = {"terrain_setup", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
