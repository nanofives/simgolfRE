// Module `events` (phase 3.5): events, news and golfer stories (see portsim/sim/events.h). One translation unit built from
// this file (the text-buffer leaves, the bulletin ring, the registry table) and the parts it includes, which are `.inc` so
// that src/sim/*.cpp stays one file per module (port/test/test_static.py):
//   events_calls.inc  the callee wrappers (generator, message ticker, number formatter, audio requests)
//   events_story.inc  the story file reader, the reply chapter, storyText, substituteStoryNames, advanceStory
//   events_think.inc  emitGolferThought and buildGolferComment
//   events_news.inc   announceLandmark, patronEvent, announceHoleType, holeMagazineEvent, announceBuilding
//   events_pair.inc   pairGolfersWithStory
// Phase 3.5, written from the specification (analysis notes, hook sources as reading reference, vector corpus); no original
// address, code or text: literals are (pool member, byte offset) pairs. The long bodies were converted mechanically from the
// hook sources by log/p3/p35_hook2port.py (addresses to GameState members, comments dropped) and then edited by hand.
#include "portsim/sim/events.h"

#include <vector>

#include "events_hook.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"

namespace portsim {
namespace {
// Negative controls: mode 0 is the correct body.
//   appendUpgradeText  1 = the closing clauses swapped, 2 = strcpy instead of the first strcat
//   replaceInText      1 = the saved tail skips key's length minus one
//   thoughtBalance     1 = counts 0x8000 as -1 as well
//   thoughtFlag        1 = bit 14 set gives 1
//   appendOpinion      1 = rating 6 with twist == 1 takes the second phrase, 2 = rating 0x80 is not out of range
//   postEvent          1 = the already-posted test is skipped
//   runBulletinBoardWithSound  1 = the animation tick is not cleared afterwards
//   emitGolferThought  1 = kind 0x13 needs d > 2, 2 = mood clamp -9, 3 = delta / 2 instead of (delta - 1) / 2
//   buildGolferComment 1 = kind 1 mirrors the type bits, 2 = own and other names swapped, 3 = g == 0x98 clears nothing
//   announceLandmark   1 = landmark 'A' maps to the next id, 2 = the neighbour step draws Random::range(2)
//   patronEvent        1 = the CEO's payment is 4000 instead of 5000
//   holeMagazineEvent  (none reads RED: see log/p3/p35_notes.txt)
//   announceHoleType   1 = kind 3 posts event 1
//   announceBuilding   1 = a hole of 0x32 yards is a par 3, 2 = the facing octant rounds down
// (the story functions' and pairGolfersWithStory's modes are listed in events_story.inc / events_pair.inc)
int g_negctl_append_upgrade_text = 0;
int g_negctl_replace_in_text = 0;
int g_negctl_thought_balance = 0;
int g_negctl_thought_flag = 0;
int g_negctl_append_opinion = 0;
int g_negctl_post_event = 0;
int g_negctl_run_bulletin = 0;
int g_negctl_emit_golfer_thought = 0;
int g_negctl_build_golfer_comment = 0;
int g_negctl_announce_landmark = 0;
int g_negctl_patron_event = 0;
int g_negctl_hole_magazine_event = 0;
int g_negctl_announce_hole_type = 0;
int g_negctl_announce_building = 0;

// byte offsets in f_strpool_4e28e0 of the opinion phrases: ratings 0..5
constexpr uint32_t kOpinionFixed[6] = {404, 372, 340, 308, 272, 244};
}  // namespace

namespace ev {

uint32_t text_va(Ctx &ctx) { return state_va(ctx, &GameState::g_textBuffer); }

uint32_t vstrstr(const Memory &m, uint32_t hay, uint32_t needle) {
  // the CRT's strstr: the first position of hay where the whole needle matches; an empty needle matches at hay
  if (m.load<uint8_t>(needle) == 0) return hay;
  for (uint32_t h = hay;; ++h) {
    const uint8_t c = m.load<uint8_t>(h);
    if (c == 0) return 0;
    uint32_t a = h, b = needle;
    while (m.load<uint8_t>(b) != 0 && m.load<uint8_t>(a) == m.load<uint8_t>(b)) {
      ++a;
      ++b;
    }
    if (m.load<uint8_t>(b) == 0) return h;
  }
}

std::vector<uint8_t> grab(Ctx &ctx, uint32_t va) {
  std::vector<uint8_t> v;
  for (;; ++va) {
    const uint8_t c = ctx.mem.load<uint8_t>(va);
    v.push_back(c);
    if (c == 0) break;
  }
  return v;
}

void put(Ctx &ctx, uint32_t dst, const std::vector<uint8_t> &bytes) {
  for (size_t k = 0; k < bytes.size(); ++k) ctx.mem.store<uint8_t>(dst + static_cast<uint32_t>(k), bytes[k]);
}

void replace_with(Ctx &ctx, uint32_t key, const std::vector<uint8_t> &repl) {
  const Memory &m = ctx.mem;
  const uint32_t buf = text_va(ctx);
  const uint32_t p = vstrstr(m, buf, key);
  if (p == 0) return;
  // the tail from the match on is saved first (a 512-byte stack buffer in the original; longer tails overflow it: U-0900)
  const std::vector<uint8_t> tail = grab(ctx, p);
  m.store<uint8_t>(p, 0);
  uint32_t end = buf + vstrlen(m, buf);
  for (uint8_t c : repl) {
    m.store<uint8_t>(end++, c);
    if (c == 0) break;
  }
  if (repl.empty() || repl.back() != 0) m.store<uint8_t>(end, 0);
  const uint32_t klen = vstrlen(m, key) - (SG_NEGCTL(g_negctl_replace_in_text) == 1 ? 1u : 0u);
  end = buf + vstrlen(m, buf);
  for (uint32_t k = klen;; ++k) {
    const uint8_t c = k < tail.size() ? tail[k] : 0;
    m.store<uint8_t>(end++, c);
    if (c == 0) break;
  }
}

Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  const FuncDef *f = registry_at(i, nullptr);
  uint32_t a[16] = {0};
  size_t n = 0;
  for (uint32_t v : args) a[n++] = v;
  Ret r;
  f->fn(ctx, a, r);
  return r;
}

}  // namespace ev

using ev::text_va;


void append_upgrade_text(Ctx &ctx, int32_t i) {
  const Memory &m = ctx.mem;
  const uint32_t buf = text_va(ctx);
  const Ptr<uint32_t> names = state_arr(ctx, &GameState::f_ptrtab_4c2a18);
  const uint32_t lit_a = state_va(ctx, &GameState::f_strpool_4c54c4) + 8;
  const uint32_t lit_b = state_va(ctx, &GameState::f_strpool_4c54c4);
  const uint32_t lit_c = state_va(ctx, &GameState::f_strpool_4c5450);
  if (SG_NEGCTL(g_negctl_append_upgrade_text) == 2)
    vstrcpy(m, buf, lit_a);
  else
    vstrcat(m, buf, lit_a);
  vstrcat(m, buf, names[i]);
  vstrcat(m, buf, lit_b);
  vstrcat(m, buf, names[add32(i, 1)]);
  vstrcat(m, buf, lit_c);
  const uint32_t clause0 = state_va(ctx, &GameState::f_strpool_4c53ec);
  const uint32_t clause1 = state_va(ctx, &GameState::f_strpool_4c5344) + 96;
  const bool swap = SG_NEGCTL(g_negctl_append_upgrade_text) == 1;
  if (i == 0)
    vstrcat(m, buf, swap ? clause1 : clause0);
  else if (i == 1)
    vstrcat(m, buf, swap ? clause0 : clause1);
}

void replace_in_text(Ctx &ctx, uint32_t key, uint32_t repl) { ev::replace_with(ctx, key, ev::grab(ctx, repl)); }

int32_t thought_balance(Ctx &ctx, int32_t g) {
  const Ptr<uint16_t> w = (state_arr(ctx, &GameState::g_golfers) + g).field(&Golfer::thought_arg);
  int32_t n = 0;
  for (int32_t i = 0; i < 5; ++i) {
    const uint32_t top = w[i] & 0xC000u;
    if (top == 0x4000u) ++n;
    if (top == 0xC000u || (SG_NEGCTL(g_negctl_thought_balance) == 1 && top == 0x8000u)) --n;
  }
  return n;
}

int32_t thought_flag(Ctx &ctx, int32_t g) {
  const uint16_t f = (state_arr(ctx, &GameState::g_golfers) + g).field(&Golfer::thought_arg).get();
  if (f & 0x8000u) return 2;
  const bool set = (f & 0x4000u) != 0;
  if (SG_NEGCTL(g_negctl_thought_flag) == 1) return set ? 1 : 0;
  return set ? 0 : 1;
}

void append_opinion(Ctx &ctx, int32_t rating, int32_t twist) {
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4e28e0);
  uint32_t off;
  if (u32(rating) <= 5u)
    off = kOpinionFixed[rating];
  else if (rating == 6)
    off = (twist <= 1 && !(SG_NEGCTL(g_negctl_append_opinion) == 1 && twist == 1)) ? 212 : 176;
  else if (rating == 7)
    off = twist < 1 ? 148 : 120;
  else if (rating < 0 || rating > (SG_NEGCTL(g_negctl_append_opinion) == 2 ? 0x80 : 0x7f))
    off = 4;
  else
    off = twist < 1 ? 80 : 40;
  vstrcat(ctx.mem, text_va(ctx), pool + off);
}

void post_event(Ctx &ctx, int32_t id, int32_t a, int32_t b) {
  if (ctx.gs().g_flags & 0x4000000u) return;
  const Ptr<BulletinEvent> rec = state_arr(ctx, &GameState::g_bulletinEvents) + id;
  if (rec.field(&BulletinEvent::date).get() != 0 && SG_NEGCTL(g_negctl_post_event) != 1) return;
  state_arr(ctx, &GameState::f_w_4e3db8).set(u32(id));
  rec.field(&BulletinEvent::date).set(ctx.gs().g_date);
  ctx.gs().g_bulletinAnimTick = 0;
  rec.field(&BulletinEvent::course_id).set(ctx.gs().g_courseIndex);
  (state_arr(ctx, &GameState::f_w_4e3db8) + 1).set(u32(a));
  (state_arr(ctx, &GameState::g_scrollTextWin) + 21).set(b);
}

}  // namespace portsim

#include "events_calls.inc"
#include "events_story.inc"
#include "events_think.inc"
#include "events_news.inc"
#include "events_pair.inc"

namespace portsim {

void run_bulletin_board_with_sound(Ctx &ctx) {
  ev::play_sound(ctx, 0x7e, 0x64, 0, 0, 0);
  const Ptr<int32_t> words = state_arr(ctx, &GameState::f_w_4e3db8);
  words.set(0x63);
  ctx.gs().g_bulletinAnimTick = 0x14;
  ev::backend_call(ctx, "showBulletinBoard", {});          // the front end's modal bulletin board screen
  words.set(-1);
  if (SG_NEGCTL(g_negctl_run_bulletin) != 1) ctx.gs().g_bulletinAnimTick = 0;
  ev::backend_call(ctx, "startSoundSlot", {0x7e});         // the audio backend's slot start
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_run_bulletin(Ctx &c, const uint32_t *, Ret &) { run_bulletin_board_with_sound(c); }
void a_append_upgrade_text(Ctx &c, const uint32_t *a, Ret &) { append_upgrade_text(c, i32(a[0])); }
void a_replace_in_text(Ctx &c, const uint32_t *a, Ret &) { replace_in_text(c, a[0], a[1]); }
void a_thought_balance(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(thought_balance(c, i32(a[0]))); }
void a_thought_flag(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(thought_flag(c, i32(a[0]))); }
void a_append_opinion(Ctx &c, const uint32_t *a, Ret &) { append_opinion(c, i32(a[0]), i32(a[1])); }
void a_post_event(Ctx &c, const uint32_t *a, Ret &) { post_event(c, i32(a[0]), i32(a[1]), i32(a[2])); }
void a_load_story_strings(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(load_story_strings(c, i32(a[0]), i32(a[1]), i32(a[2]), i32(a[3])));
}
void a_compute_story_reply(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(compute_story_reply(c, i32(a[0]), i32(a[1]), i32(a[2]), i32(a[3])));
}
void a_story_text(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(story_text(c, i32(a[0]), i32(a[1]), i32(a[2]))); }
void a_substitute_story_names(Ctx &c, const uint32_t *a, Ret &) { substitute_story_names(c, i32(a[0])); }
void a_advance_story(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(advance_story(c, i32(a[0]), i32(a[1]))); }
void a_emit_golfer_thought(Ctx &c, const uint32_t *a, Ret &) { emit_golfer_thought(c, i32(a[0]), i32(a[1]), i32(a[2])); }
void a_build_golfer_comment(Ctx &c, const uint32_t *a, Ret &) {
  build_golfer_comment(c, i32(a[0]), i32(a[1]), i32(a[2]), i32(a[3]));
}
void a_announce_landmark(Ctx &c, const uint32_t *a, Ret &) { announce_landmark(c, i32(a[0]), i32(a[1])); }
void a_patron_event(Ctx &c, const uint32_t *a, Ret &) { patron_event(c, i32(a[0])); }
void a_hole_magazine_event(Ctx &c, const uint32_t *, Ret &) { hole_magazine_event(c); }
void a_announce_hole_type(Ctx &c, const uint32_t *a, Ret &) { announce_hole_type(c, i32(a[0]), i32(a[1])); }
void a_announce_building(Ctx &c, const uint32_t *, Ret &) { announce_building(c); }
void a_pair_golfers(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(pair_golfers_with_story(c, i32(a[0]), i32(a[1]))); }

const FuncDef kFuncs[] = {
    {"appendUpgradeText", "default", "void", 1, 0, a_append_upgrade_text, &g_negctl_append_upgrade_text},
    {"replaceInText", "default", "void", 2, 0, a_replace_in_text, &g_negctl_replace_in_text},
    {"thoughtBalance", "default", "int", 1, 0, a_thought_balance, &g_negctl_thought_balance},
    {"thoughtFlag", "default", "int", 1, 0, a_thought_flag, &g_negctl_thought_flag},
    {"appendOpinion", "default", "void", 2, 0, a_append_opinion, &g_negctl_append_opinion},
    {"postEvent", "default", "void", 3, 0, a_post_event, &g_negctl_post_event},
    {"runBulletinBoardWithSound", "default", "void", 0, 0, a_run_bulletin, &g_negctl_run_bulletin},
    {"loadStoryStrings", "default", "int", 4, 0, a_load_story_strings, &g_negctl_load_story_strings},
    {"computeStoryReply", "default", "int", 4, 0, a_compute_story_reply, &g_negctl_compute_story_reply},
    {"storyText", "default", "int", 3, 0x027F, a_story_text, &g_negctl_story_text},
    {"substituteStoryNames", "default", "void", 1, 0, a_substitute_story_names, &g_negctl_substitute_story_names},
    {"advanceStory", "default", "int", 2, 0x027F, a_advance_story, &g_negctl_advance_story},
    {"emitGolferThought", "default", "void", 3, 0x027F, a_emit_golfer_thought, &g_negctl_emit_golfer_thought},
    {"buildGolferComment", "default", "void", 4, 0, a_build_golfer_comment, &g_negctl_build_golfer_comment},
    {"announceLandmark", "default", "void", 2, 0x027F, a_announce_landmark, &g_negctl_announce_landmark},
    {"patronEvent", "default", "void", 1, 0x027F, a_patron_event, &g_negctl_patron_event},
    {"holeMagazineEvent", "default", "void", 0, 0x027F, a_hole_magazine_event, &g_negctl_hole_magazine_event},
    {"announceHoleType", "default", "void", 2, 0, a_announce_hole_type, &g_negctl_announce_hole_type},
    {"announceBuilding", "default", "void", 0, 0x027F, a_announce_building, &g_negctl_announce_building},
    {"pairGolfersWithStory", "default", "int", 2, 0, a_pair_golfers, &g_negctl_pair_golfers},
};
}  // namespace

extern const ModuleDef kModule_events = {"events", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
