// Course and hole records, placed objects, animals, names and titles (see portsim/sim/course.h). Phase 3 stage B,
// child 3.1b, written from the specification (analysis notes, the C3 / C4 hook sources' transcriptions, vector corpus);
// no original address, code or text.
#include "portsim/sim/course.h"

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "portsim/execonst.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   freeRecordAtPos     1 = frees only the first match, 2 = y compared with x's word
//   initRecords         1 = 99 animal slots, 2 = placeRecord(bank, 0, 0, 0)
//   clearMatching       1 = b not compared (BLIND: every seed's g_labelQueueB is 0, so a wrong clear is invisible),
//                       2 = stores -1 instead of 0
//   staticInit_820f04   1 = no fabs
//   setPendingCourseInfo 1 = strcat instead of strcpy
//   clearBuffers        1 = offsets filled with 0
//   appendString        1 = offset -1 not tested (appends at -1)
//   landmarkName        1 = long and short names swapped, 2 = types 17 and 18 swapped
//   buildGolferName     1 = no title, 2 = class 0x80 parity inverted, 3 = class 0x40 suffix from the sub-kind
//   buildStatusText     1 = equal priority returns early, 2 = saved text not appended
//   appendHoleName      1 = par 3 and par 4 tables swapped
//   appendCourseTitle   1 = full == 1 takes the short suffix
//   nearestHole         1 = ties take the later hole, 2 = pin point ignored
//   holeQuadrants       1 = threshold divided by 2
//   sceneryLabel        1 = text == 0 returns 0 on a match, 2 = no separator after the name
//   fileExists          1 = inverted
//   lookupThemeText     1 = '*' does not end the paragraph, 2 = no separator after a line
//   listEraseRange      1 = size not decremented, 2 = *out = first
//   loadThemeFile       1 = body threshold 0x13, 2 = index 0 does not fill g_playerSkills
//   loadThemes          1 = sets membership bit 1 instead of bit 0
//   switchToCourse      1 = finance row g_year % 100 + 1
//   seedCourseTable     1 = records 12..15 never redraw, 2 = setting 2 subtracts 10 for every record
//   buildScenarioIntro  1 = g_messageTimer not set to 0x80, 2 = cash divided by 100, 3 = startMessage priority 0 (BLIND:
//                       no vector has a message pending, so priority 0 and 1 behave alike)
int g_negctl_free_record_at_pos = 0;
int g_negctl_init_records = 0;
int g_negctl_clear_matching = 0;
int g_negctl_static_init_820f04 = 0;
int g_negctl_set_pending_course_info = 0;
int g_negctl_clear_buffers = 0;
int g_negctl_append_string = 0;
int g_negctl_landmark_name = 0;
int g_negctl_build_golfer_name = 0;
int g_negctl_build_status_text = 0;
int g_negctl_append_hole_name = 0;
int g_negctl_append_course_title = 0;
int g_negctl_nearest_hole = 0;
int g_negctl_hole_quadrants = 0;
int g_negctl_seed_course_table = 0;
int g_negctl_switch_to_course = 0;
int g_negctl_load_theme_file = 0;
int g_negctl_load_themes = 0;
int g_negctl_scenery_label = 0;
int g_negctl_file_exists = 0;
int g_negctl_lookup_theme_text = 0;
int g_negctl_list_erase_range = 0;
int g_negctl_build_scenario_intro = 0;

// ------------------------------------------------------------------------------------------------ helpers
uint32_t text_va(Ctx &c) { return state_va(c, &GameState::g_textBuffer); }
// strcat of the string at `src` onto g_textBuffer (the original's inline idiom)
void cat(Ctx &c, uint32_t src) { vstrcat(c.mem, text_va(c), src); }
// a NUL-terminated string read into host memory, NUL included
std::vector<uint8_t> read_cstr(const Memory &m, uint32_t s) {
  const uint32_t n = vstrlen(m, s) + 1;
  std::vector<uint8_t> b(n);
  for (uint32_t i = 0; i < n; ++i) b[i] = m.load<uint8_t>(s + i);
  return b;
}
void write_bytes(const Memory &m, uint32_t dst, const std::vector<uint8_t> &b) {
  for (size_t i = 0; i < b.size(); ++i) m.store<uint8_t>(dst + static_cast<uint32_t>(i), b[i]);
}
Ptr<Golfer> golfer_at(Ctx &c, int32_t g) { return state_arr(c, &GameState::g_golfers) + g; }
Ptr<HoleRecord> hole_at(Ctx &c, int32_t h) { return state_arr(c, &GameState::g_holes) + h; }
}  // namespace

// ------------------------------------------------------------------------------------------------ records
void free_record_at_pos(Ctx &ctx, int32_t x, int32_t y) {
  const int mode = SG_NEGCTL(g_negctl_free_record_at_pos);
  const Ptr<Animal> a0 = state_arr(ctx, &GameState::g_animalRecords);
  for (int32_t i = 0; i < 100; ++i) {
    const Ptr<Animal> a = a0 + i;
    if (a.field(&Animal::x).get() != x) continue;
    if ((mode == 2 ? a.field(&Animal::x).get() : a.field(&Animal::y).get()) != y) continue;
    a.field(&Animal::type).set(static_cast<int16_t>(-1));
    if (mode == 1) return;
  }
}

void reset_record_bank(Ctx &ctx, int32_t bank) {
  const Ptr<SgRecordBank> b = state_arr(ctx, &GameState::g_recordBanks) + bank;
  const Ptr<int32_t> ids = b.field(&SgRecordBank::id).as<int32_t>();
  for (int32_t i = 0; i < 8; ++i) ids.at(i).set(-1);
  const Ptr<int32_t> occ = b.field(&SgRecordBank::occupancy).as<int32_t>();
  for (int32_t i = 0; i < 9; ++i) occ.at(i).set(-1);
}

int32_t place_record(Ctx &ctx, int32_t bank, int32_t type, int32_t row, int32_t col) {
  // the original's first loop only computes a flag it then jumps past (its note), so placement is unconditional
  const Ptr<SgRecordBank> b = state_arr(ctx, &GameState::g_recordBanks) + bank;
  const Ptr<int16_t> ids = b.field(&SgRecordBank::id);
  for (int32_t slot = 0; slot < 16; ++slot) {
    if (ids[slot] != -1) continue;
    ids.at(slot).set(static_cast<int16_t>(type));
    b.field(&SgRecordBank::x).at(slot).set(static_cast<int8_t>(row));
    b.field(&SgRecordBank::y).at(slot).set(static_cast<int8_t>(col));
    b.field(&SgRecordBank::anim).at(slot).set(static_cast<int8_t>(-1));
    const Ptr<uint8_t> defs = (state_arr(ctx, &GameState::g_recordDefs) + type).field(&F_RecordDef::footprint);
    const Ptr<int8_t> occ = b.field(&SgRecordBank::occupancy);
    for (int32_t r = 0; r < 6; ++r) {
      const uint8_t bits = defs[r];
      for (int32_t c = 0; c < 6; ++c)
        if (bits & (1u << c)) occ.at(add32(add32(col, mul32(add32(row, r), 6)), c)).set(static_cast<int8_t>(slot));
    }
    return 1;
  }
  return 0;
}

int32_t init_records(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_init_records);
  const Ptr<Animal> a0 = state_arr(ctx, &GameState::g_animalRecords);
  for (int32_t i = 0; i < (mode == 1 ? 99 : 100); ++i) (a0 + i).field(&Animal::type).set(static_cast<int16_t>(-1));
  int32_t r = 0;
  for (int32_t bank = 0; bank < 6; ++bank) {
    reset_record_bank(ctx, bank);
    r = place_record(ctx, bank, mode == 2 ? 0 : bank, 0, 0);
  }
  return r;
}

void clear_matching(Ctx &ctx, int32_t a, int32_t b) {
  GameState &s = ctx.gs();
  for (int i = 0; i < 8; ++i)
    if (s.g_labelQueueC[i] == a && (SG_NEGCTL(g_negctl_clear_matching) == 1 || s.g_floatLabelY[i] == b))
      s.g_labelQueueB[i] = SG_NEGCTL(g_negctl_clear_matching) == 2 ? -1 : 0;
}

void static_init_820f04(Ctx &ctx) {
  const double v = ctx.exe().get<double>("f_k_dbl_4ba620", 0);
  ctx.gs().static_820f04 = i32(ftol_low32(SG_NEGCTL(g_negctl_static_init_820f04) == 1 ? v : std::fabs(v)));
}

uint32_t set_pending_course_info(Ctx &ctx, uint32_t s) {
  const uint32_t n = vstrlen(ctx.mem, s) + 1;
  const uint32_t dst = state_va(ctx, &GameState::g_pendingCourseInfo);
  if (SG_NEGCTL(g_negctl_set_pending_course_info) == 1)
    vstrcat(ctx.mem, dst, s);
  else
    rep_movs(ctx.mem, dst, s, n);
  return n;
}

void clear_buffers(Ctx &ctx) {
  GameState &s = ctx.gs();
  std::memset(s.g_namePool, 0, sizeof(s.g_namePool));
  std::memset(s.g_namePoolOffset, SG_NEGCTL(g_negctl_clear_buffers) == 1 ? 0 : 0xFF, sizeof(s.g_namePoolOffset));
}

// ------------------------------------------------------------------------------------------------ names and titles
int32_t append_string(Ctx &ctx, int32_t id) {
  if (id == -1) return 0;
  const int16_t off = (state_arr(ctx, &GameState::g_namePoolOffset) + id).get();
  if (off == -1 && SG_NEGCTL(g_negctl_append_string) != 1) return 0;
  cat(ctx, state_va(ctx, &GameState::g_namePool) + u32(int32_t(off)));
  return 1;
}

void landmark_name(Ctx &ctx, int32_t type, int32_t full) {
  // byte offsets in f_strpool_4c4970: {long, short} per landmark type 0..18 (type 10 has one name)
  static const uint16_t kName[19][2] = {
      {0x2D4, 0x2CC}, {0x2B8, 0x2B0}, {0x290, 0x27C}, {0x260, 0x254}, {0x23C, 0x230}, {0x21C, 0x210}, {0x1F0, 0x1DC},
      {0x1BC, 0x1B0}, {0x19C, 0x194}, {0x184, 0x178}, {0x164, 0x164}, {0x144, 0x130}, {0x11C, 0x114}, {0x0F4, 0x0E0},
      {0x0C8, 0x0B8}, {0x0A0, 0x090}, {0x078, 0x06C}, {0x050, 0x040}, {0x028, 0x01C}};
  constexpr uint16_t kDefault = 0x10;
  const int mode = SG_NEGCTL(g_negctl_landmark_name);
  uint32_t off = kDefault;
  if (u32(type) <= 18u) {
    uint32_t t = u32(type);
    if (mode == 2 && (t == 17 || t == 18)) t = 35 - t;
    const bool long_form = (full != 0) != (mode == 1);
    off = kName[t][long_form ? 0 : 1];
  }
  cat(ctx, state_va(ctx, &GameState::f_strpool_4c4970) + off);
}

uint32_t crt_itoa10(Ctx &ctx, int32_t v, uint32_t buf) {
  uint32_t p = buf;
  uint32_t mag = u32(v);
  if (v < 0) {
    ctx.mem.store<uint8_t>(p++, '-');
    mag = 0u - mag;
  }
  char digits[12];
  int n = 0;
  do {
    digits[n++] = static_cast<char>('0' + mag % 10u);
    mag /= 10u;
  } while (mag != 0);
  while (n > 0) ctx.mem.store<uint8_t>(p++, static_cast<uint8_t>(digits[--n]));
  ctx.mem.store<uint8_t>(p, 0);
  return buf;
}

void build_golfer_name(Ctx &ctx, int32_t g, int32_t with_title) {
  const int mode = SG_NEGCTL(g_negctl_build_golfer_name);
  if (with_title != 0 && mode != 1) cat(ctx, state_va(ctx, &GameState::f_strpool_4c528c) + 0x2C);
  const Ptr<Golfer> gr = golfer_at(ctx, g);
  const uint8_t kind = gr.field(&Golfer::kind).get();
  const uint32_t sub = kind & 0x1Fu;
  uint32_t suffix;
  switch (kind & 0xE0u) {
    case 0x20:
      if (sub == 4)
        cat(ctx, state_va(ctx, &GameState::g_golferTypes) + 0x10);  // golfer type 0's name
      else
        cat(ctx, (state_arr(ctx, &GameState::g_golfPros) + int32_t(gr.field(&Golfer::name_rec).get())).va());
      return;
    case 0x40:
      cat(ctx, ctx.gs().f_ptrtab_4c2c10[0]);
      suffix = u32(ctx.gs().g_landPurchaseCount) & 0x7Fu;
      if (mode == 3) suffix = sub;
      break;
    case 0x60:
      cat(ctx, ctx.gs().f_ptrtab_4c2c10[2]);
      suffix = sub;
      break;
    case 0x80: {
      const bool odd = ((g & 1) != 0) != (mode == 2);
      cat(ctx, state_va(ctx, &GameState::f_strpool_4e1b5c) + (odd ? 0x38u : 0x24u));
      return;
    }
    default:
      cat(ctx, (state_arr(ctx, &GameState::g_golferTypes) + int32_t(gr.field(&Golfer::type).get())).field(&GolferType::name).va());
      return;
  }
  if (suffix == 0) return;
  uint32_t s;
  switch (suffix) {
    case 1: s = state_va(ctx, &GameState::f_strpool_4e1bc8) + 0x8; break;
    case 2: s = state_va(ctx, &GameState::f_strpool_4e1bc8); break;
    case 3: s = state_va(ctx, &GameState::f_strpool_4e1bc4); break;
    case 4: s = state_va(ctx, &GameState::f_strpool_4e1bac) + 0x14; break;
    case 5: s = state_va(ctx, &GameState::f_strpool_4e1bac) + 0x10; break;
    case 6: s = state_va(ctx, &GameState::f_strpool_4e1bac) + 0x8; break;
    case 7: s = state_va(ctx, &GameState::f_strpool_4e1bac); break;
    case 8: s = state_va(ctx, &GameState::f_strpool_4e1ba8); break;
    default: s = state_va(ctx, &GameState::f_strpool_4e1b5c) + 0x48; break;
  }
  cat(ctx, s);
}

// buildStatusText(golfer, priority): unk_005a9cd0 is the signed priority of the status text now held. A lower priority
// returns it. Else the byte takes priority's low byte; golfer -1 copies g_textBuffer to g_statusText and returns 0.
// Otherwise g_textBuffer is saved (a 512-byte stack buffer in the original: a longer text overruns its frame, U-0820),
// set to the empty string g_str4e9a84, then gets buildGolferName(golfer, 0), a separator, _itoa(golfer's hole byte)
// in g_textBuf and a closing string; g_statusText = that text + the saved text; g_textBuffer = the saved text again.
// Returns strlen(saved) + 1.
int32_t build_status_text(Ctx &ctx, int32_t golfer, int32_t priority) {
  const int mode = SG_NEGCTL(g_negctl_build_status_text);
  int8_t &current = ctx.gs().unk_005a9cd0;
  if (priority < int32_t(current) || (mode == 1 && priority == int32_t(current))) return priority;
  current = static_cast<int8_t>(priority);
  const uint32_t text = text_va(ctx);
  const uint32_t status = state_va(ctx, &GameState::g_statusText);
  if (golfer == -1) {
    vstrcpy(ctx.mem, status, text);
    return 0;
  }
  const std::vector<uint8_t> saved = read_cstr(ctx.mem, text);
  vstrcpy(ctx.mem, text, state_va(ctx, &GameState::g_str4e9a84));
  build_golfer_name(ctx, golfer, 0);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c4970);
  cat(ctx, pool + 4);
  const int8_t hole = golfer_at(ctx, golfer).field(&Golfer::hole).get();
  cat(ctx, crt_itoa10(ctx, hole, state_va(ctx, &GameState::g_textBuf)));
  cat(ctx, pool);
  vstrcpy(ctx.mem, status, text);
  if (mode != 2) write_bytes(ctx.mem, status + vstrlen(ctx.mem, status), saved);
  write_bytes(ctx.mem, text, saved);
  return static_cast<int32_t>(saved.size());
}

void append_hole_name(Ctx &ctx, int32_t hole) {
  const Ptr<HoleRecord> h = hole_at(ctx, hole);
  if (h.field(&HoleRecord::flags).as<uint8_t>().get() & 0x81u) {
    if (append_string(ctx, hole) != 0) return;
    const int8_t par = h.field(&HoleRecord::par).get();
    const bool swap = SG_NEGCTL(g_negctl_append_hole_name) == 1;
    Ptr<uint32_t> table = state_arr(ctx, &GameState::f_ptrtab_4c2ed8);
    if (par == 3) table = swap ? state_arr(ctx, &GameState::f_ptrtab_4c2e38) : state_arr(ctx, &GameState::f_ptrtab_4c2e88);
    else if (par == 4) table = swap ? state_arr(ctx, &GameState::f_ptrtab_4c2e88) : state_arr(ctx, &GameState::f_ptrtab_4c2e38);
    cat(ctx, table[hole]);
  } else {
    cat(ctx, state_va(ctx, &GameState::f_strpool_4c4970) + 8);
    cat(ctx, crt_itoa10(ctx, hole, state_va(ctx, &GameState::g_textBuf)));
  }
}

void append_course_title(Ctx &ctx, int32_t full) {
  if (append_string(ctx, 0) == 0) {
    const int8_t site = (state_arr(ctx, &GameState::g_courseTable) + ctx.gs().g_courseIndex).field(&CourseRecord::location_id).get();
    cat(ctx, (state_arr(ctx, &GameState::f_tab_locations_4c1e90) + int32_t(site)).field(&F_Location::text).va());
  }
  if (full == -1) return;
  const bool long_form = (full == 1) != (SG_NEGCTL(g_negctl_append_course_title) == 1);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c5344);
  uint32_t s;
  switch (bucket_value(sub32(ctx.gs().g_holesPlusOne, 1))) {
    case 1: s = pool + (long_form ? 0x20u : 0x1Cu); break;
    case 2: s = pool + (long_form ? 0x30u : 0x2Cu); break;
    case 3: s = long_form ? pool + 0x40u : state_va(ctx, &GameState::g_str4e9a84); break;
    default: s = pool + (long_form ? 0x54u : 0x50u); break;
  }
  cat(ctx, s);
}

// ------------------------------------------------------------------------------------------------ geometry
int32_t nearest_hole(Ctx &ctx, int32_t x, int32_t y) {
  const int mode = SG_NEGCTL(g_negctl_nearest_hole);
  int32_t best = 0xFFFF, result = -1;
  auto better = [&](int32_t d, int32_t h) {
    if (d < best || (mode == 1 && d == best)) {
      best = d;
      result = h;
    }
  };
  for (int32_t h = 1; h <= 18; ++h) {
    const Ptr<HoleRecord> r = hole_at(ctx, h);
    better(distance(sub32(r.field(&HoleRecord::tee_x).get(), x), sub32(r.field(&HoleRecord::tee_y).get(), y)), h);
    better(distance(sub32(r.field(&HoleRecord::cup_x).get(), x), sub32(r.field(&HoleRecord::cup_y).get(), y)), h);
    const Ptr<HolePoints> p = state_arr(ctx, &GameState::g_holePoints) + h;
    const int32_t bx = p.field(&HolePoints::b_x).get();
    if (bx != -1 && mode != 2) better(distance(sub32(x, bx >> 10), sub32(y, p.field(&HolePoints::b_y).get() >> 10)), h);
  }
  return result;
}

int32_t hole_quadrants(Ctx &ctx, int32_t a, int32_t b, uint32_t want) {
  uint32_t bits = 0;
  const Ptr<PlacedObject> o0 = state_arr(ctx, &GameState::g_placed_objects);
  for (int32_t i = 0; i < 256; ++i) {
    const Ptr<PlacedObject> o = o0 + i;
    if (o.field(&PlacedObject::type).get() != 4) continue;
    const int32_t v = o.field(&PlacedObject::value).get();
    if (v >= 0x10) continue;
    const int32_t d = tile_distance(a, b, o.field(&PlacedObject::x).get(), o.field(&PlacedObject::y).get());
    const int32_t threshold = mul32(add32(mul32(v, 5), 0x28), 5) / (SG_NEGCTL(g_negctl_hole_quadrants) == 1 ? 2 : 3);
    if (d < threshold) bits |= 1u << (v & 3);
  }
  ctx.gs().g_holeDirMask = bits;
  return (bits & want) == want ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------ peers
namespace {
// A function another module registers (3.1a's decorationName, phase 3.7's startMessage), called through the registry by
// its hooks.csv name; nullptr when no module registers it.
const FuncDef *peer(const char *name) {
  const int i = registry_find(name);
  return i < 0 ? nullptr : registry_at(i, nullptr);
}

// startMessage(arg, prio, sound) (re/analysis/ui/0040cb00, a ui straddler): phase 3.7a's registered function (module
// tick); MissingResource when no module registers it (the local copy stage B kept was replaced, 2026-10-19).
int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound) {
  const FuncDef *f = peer("startMessage");
  if (!f) throw MissingResource{"startMessage"};
  const uint32_t a[3] = {u32(arg), u32(prio), u32(sound)};
  Ret r;
  f->fn(ctx, a, r);
  return i32(r.eax);
}
}  // namespace

// ------------------------------------------------------------------------------------------------ course table
// seedCourseTable (re/analysis/sim/0046f2b0): g_courseTable (17 records) zeroed, location_id and course_type of records
// 0..15 set to 0xff, then records 12..15 and 0..11 each draw a location from the 16-entry location table with
// Random::range(16) on g_rng (low word kept) until it is acceptable, and copy its fields (see the loop comments).
void seed_course_table(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_seed_course_table);
  GameState &gs = ctx.gs();
  std::memset(gs.g_courseTable, 0, sizeof(gs.g_courseTable));
  const Ptr<CourseRecord> rec0 = state_arr(ctx, &GameState::g_courseTable);
  for (int32_t n = 0; n < 16; ++n) {
    (rec0 + n).field(&CourseRecord::course_type).set(static_cast<int8_t>(-1));
    (rec0 + n).field(&CourseRecord::location_id).set(static_cast<int8_t>(-1));
  }
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  auto pick = [&]() { return static_cast<int32_t>(random_range(seed, 16) & 0xFFFFu); };
  const Ptr<F_Location> loc0 = state_arr(ctx, &GameState::f_tab_locations_4c1e90);
  auto fill = [&](int32_t counter, int32_t id) {
    const Ptr<CourseRecord> r = rec0 + counter;
    const Ptr<F_Location> t = loc0 + id;
    const uint8_t base = static_cast<uint8_t>(mul32(counter + 4, 10));
    const int32_t delta = counter >= 4 ? 20 : 10;
    r.field(&CourseRecord::location_id).set(static_cast<int8_t>(id));
    uint8_t acres = base;
    const uint8_t setting = t.field(&F_Location::b3f).get();
    if (setting == 0) acres = static_cast<uint8_t>(base + delta);
    if (setting == 2) acres = static_cast<uint8_t>(acres - (mode == 2 ? 10 : delta));
    if (gs.g_flags & 0x1000000u) acres = 0xFA;
    r.field(&CourseRecord::acres).set(acres);
    r.field(&CourseRecord::setting).set(static_cast<int8_t>(setting));
    r.field(&CourseRecord::relief).set(static_cast<int8_t>(t.field(&F_Location::b40).get()));
    r.field(&CourseRecord::course_type).set(static_cast<int8_t>(t.field(&F_Location::b3e).get()));
    r.field(&CourseRecord::map_x).set(t.field(&F_Location::v3a).get());
    r.field(&CourseRecord::map_y).set(t.field(&F_Location::v3c).get());
    r.field(&CourseRecord::year_left).set(static_cast<int8_t>(-1));
    r.field(&CourseRecord::year_bought).set(static_cast<int8_t>(-1));
    const uint32_t name = t.field(&F_Location::name).va();
    rep_movs(ctx.mem, r.va(), name, vstrlen(ctx.mem, name) + 1);
  };
  // records 12..15: redraw while an earlier record of this phase has the same course type as the drawn location
  for (int32_t counter = 12; counter < 16; ++counter) {
    int32_t id;
    for (;;) {
      id = pick();
      bool free = true;
      const int8_t ct = static_cast<int8_t>((loc0 + id).field(&F_Location::b3e).get());
      for (int32_t j = 12; j < counter; ++j)
        if ((rec0 + j).field(&CourseRecord::course_type).get() == ct) free = false;
      if (free || mode == 1) break;
    }
    fill(counter, id);
  }
  // records 0..11: redraw while any record 0..15 holds the location; record 0 also while the location's type is not 0
  for (int32_t counter = 0; counter < 12; ++counter) {
    int32_t id;
    for (;;) {
      id = pick();
      bool free = true;
      for (int32_t n = 0; n < 16; ++n)
        if (int32_t((rec0 + n).field(&CourseRecord::location_id).get()) == id) free = false;
      if (counter == 0 && (loc0 + id).field(&F_Location::b3e).get() != 0) continue;
      if (free) break;
    }
    fill(counter, id);
  }
}

// ------------------------------------------------------------------------------------------------ scenario intro
// buildScenarioIntro (re/analysis/frontend/0045fd80): at g_date == 0x20, unless g_flags bit 0x1000000, the back-story
// of g_courseType 0..3 is built in g_textBuffer (three call appendCourseTitle(-1); two splice in g_cash_div100 * 100 /
// 1000 through _itoa into g_infoPanelNumBuf), then startMessage(0x8000211f, 1, -4) runs for EVERY course type (types
// outside 0..3 build no text) and g_messageTimer = 0x80. At g_date == 0x1400 a fixed message goes to startMessage
// (0x80002190, 0, -4).
void build_scenario_intro(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_build_scenario_intro);
  GameState &gs = ctx.gs();
  const uint32_t buf = text_va(ctx);
#define SG_P(pool, off) (state_va(ctx, &GameState::pool) + (off))
  auto cpy = [&](uint32_t s) { vstrcpy(ctx.mem, buf, s); };
  auto cash = [&]() {
    const uint32_t num = state_va(ctx, &GameState::g_infoPanelNumBuf);
    crt_itoa10(ctx, mul32(gs.g_cash_div100, 100) / (mode == 2 ? 100 : 1000), num);
    cat(ctx, num);
  };
  if (gs.g_date == 0x20) {
    if (gs.g_flags & 0x1000000u) return;
    uint32_t tail = 0;
    switch (int32_t(gs.g_courseType)) {
      case 0:
        cpy(SG_P(f_strpool_4d43c0, 0x114));
        append_course_title(ctx, -1);
        cat(ctx, SG_P(f_strpool_4d43c0, 0xE4));
        cat(ctx, SG_P(f_strpool_4d43c0, 0xB0));
        append_course_title(ctx, -1);
        cat(ctx, SG_P(f_strpool_4d43c0, 0x70));
        cat(ctx, SG_P(f_strpool_4d43c0, 0x34));
        cat(ctx, SG_P(f_strpool_4d43c0, 0x14));
        cat(ctx, SG_P(f_strpool_4d43c0, 0x0));
        cat(ctx, SG_P(f_strpool_4d43b4, 0x0));
        cash();
        cat(ctx, SG_P(f_strpool_4d4324, 0x70));
        append_course_title(ctx, -1);
        tail = SG_P(f_strpool_4d4324, 0x40);
        break;
      case 1:
        cpy(SG_P(f_strpool_4d4324, 0x30));
        append_course_title(ctx, -1);
        cat(ctx, SG_P(f_strpool_4d4324, 0x0));
        cat(ctx, SG_P(f_strpool_4d4288, 0x50));
        cat(ctx, SG_P(f_strpool_4d4288, 0x34));
        cat(ctx, SG_P(f_strpool_4d4288, 0x0));
        cat(ctx, SG_P(f_strpool_4d4240, 0x0));
        cat(ctx, SG_P(f_strpool_4d4200, 0x0));
        cash();
        cat(ctx, SG_P(f_strpool_4d418c, 0x5C));
        append_course_title(ctx, -1);
        tail = SG_P(f_strpool_4d418c, 0x38);
        break;
      case 2:
        cpy(SG_P(f_strpool_4d43c0, 0x114));
        append_course_title(ctx, -1);
        cat(ctx, SG_P(f_strpool_4d3fc0, 0x0));
        cat(ctx, SG_P(f_strpool_4d3f7c, 0x0));
        cat(ctx, SG_P(f_strpool_4d3f10, 0x0));
        cat(ctx, SG_P(f_strpool_4d3e5c, 0x60));
        cat(ctx, SG_P(f_strpool_4d3e5c, 0x20));
        append_course_title(ctx, -1);
        tail = SG_P(f_strpool_4d3e5c, 0x0);
        break;
      case 3:
        cpy(SG_P(f_strpool_4d43c0, 0x114));
        append_course_title(ctx, -1);
        cat(ctx, SG_P(f_strpool_4d418c, 0x0));
        cat(ctx, SG_P(f_strpool_4d4138, 0x0));
        cat(ctx, SG_P(f_strpool_4d40f0, 0x0));
        cat(ctx, SG_P(f_strpool_4d4094, 0x0));
        cat(ctx, SG_P(f_strpool_4d3fc0, 0x70));
        append_course_title(ctx, -1);
        tail = SG_P(f_strpool_4d3fc0, 0x3C);
        break;
      default:
        break;
    }
    if (tail != 0) cat(ctx, tail);
    start_message(ctx, i32(0x8000211Fu), mode == 3 ? 0 : 1, -4);
    if (mode != 1) gs.g_messageTimer = 0x80;
  }
  if (gs.g_date == 0x1400) {
    cpy(SG_P(f_strpool_4d3dfc, 0x0));
    cat(ctx, SG_P(f_strpool_4d3da0, 0x0));
    cat(ctx, SG_P(f_strpool_4d3d20, 0x38));
    cat(ctx, SG_P(f_strpool_4d3d20, 0x0));
    cat(ctx, SG_P(f_strpool_4d3c3c, 0x7C));
    start_message(ctx, i32(0x80002190u), 0, -4);
  }
#undef SG_P
}

// ------------------------------------------------------------------------------------------------ scenery label
// sceneryLabel (re/analysis/course/00407b60): the first scenic dedication whose tile (x, y) words equal (a, b): text == 0
// returns 1; else golfer type `golfer_type`'s name, the separator and decorationName(a, b, -1) (3.1a's, through the
// registry) are appended and 1 returned. No match appends the "scenic" default when text != 0 and returns 0.
int32_t scenery_label(Ctx &ctx, int32_t a, int32_t b, int32_t text) {
  const int mode = SG_NEGCTL(g_negctl_scenery_label);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c4970);
  const Ptr<ScenicDedication> d0 = state_arr(ctx, &GameState::g_scenicDedications);
  for (int32_t i = 0; i < 100; ++i) {
    const Ptr<ScenicDedication> d = d0 + i;
    if (d.field(&ScenicDedication::tile_x).get() != a || d.field(&ScenicDedication::tile_y).get() != b) continue;
    if (text == 0) return mode == 1 ? 0 : 1;
    const int32_t type = d.field(&ScenicDedication::golfer_type).get();
    cat(ctx, (state_arr(ctx, &GameState::g_golferTypes) + type).field(&GolferType::name).va());
    if (mode != 2) cat(ctx, pool + 0x4C4);
    const FuncDef *f = peer("decorationName");
    if (!f) throw MissingResource{"sceneryLabel: decorationName is not registered (3.1a)"};
    const uint32_t args[3] = {u32(a), u32(b), u32(-1)};
    Ret r;
    f->fn(ctx, args, r);
    return 1;
  }
  if (text != 0) cat(ctx, pool + 0x4C8);
  return 0;
}

// ------------------------------------------------------------------------------------------------ files
namespace {
std::string read_name(const Memory &m, uint32_t s) {
  const std::vector<uint8_t> b = read_cstr(m, s);
  return std::string(b.begin(), b.end() - 1);
}

// A FILE opened by the static CRT in text mode ("r"), over the bytes of a GameFiles file: Ctrl-Z (0x1a) ends the
// data, CR LF reads as LF, a CR not followed by LF stays (VC6 _read in text mode).
struct CrtTextFile {
  std::vector<uint8_t> data;
  size_t pos = 0;
  explicit CrtTextFile(const std::vector<uint8_t> &raw) {
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] == 0x1A) break;
      if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;
      data.push_back(raw[i]);
    }
  }
  // fgets(buf, n, f) into the host buffer: up to n - 1 characters, through the first '\n'; false (buffer untouched)
  // when no character is left
  bool fgets(uint8_t *buf, int n) {
    if (pos >= data.size()) return false;
    int k = 0;
    while (k < n - 1 && pos < data.size()) {
      const uint8_t c = data[pos++];
      buf[k++] = c;
      if (c == '\n') break;
    }
    buf[k] = 0;
    return true;
  }
};

CrtTextFile open_text(Ctx &ctx, uint32_t path) {
  const std::string name = read_name(ctx.mem, path);
  GameFiles &files = ctx.files();
  // fopen failing returns NULL, which the original never tests: its first fgets faults
  if (!files.exists(name)) throw Fault{path, 0};
  return CrtTextFile(files.read_all(name));
}

int stricmp_c(const uint8_t *a, const uint8_t *b) {
  for (;; ++a, ++b) {
    int ca = *a, cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca += 0x20;
    if (cb >= 'A' && cb <= 'Z') cb += 0x20;
    if (ca != cb) return ca - cb;
    if (ca == 0) return 0;
  }
}
}  // namespace

// fileExists (re/analysis/util/004378a0): FindFirstFileA(name) != INVALID_HANDLE_VALUE. Over GameFiles: the logical name
// exists. A wildcard pattern or a directory name, which FindFirstFileA also accepts, is not modelled (U-0821).
int32_t file_exists(Ctx &ctx, uint32_t name) {
  const bool e = ctx.files().exists(read_name(ctx.mem, name));
  return (e != (SG_NEGCTL(g_negctl_file_exists) == 1)) ? 1 : 0;
}

// lookupThemeText (re/analysis/course/0045b660): see course.h. The line buffer is a 0x100-byte frame slot the original
// never initialises: a file with no line leaves it as it was (U-0822); the port starts it zeroed.
int32_t lookup_theme_text(Ctx &ctx, uint32_t path, uint32_t key) {
  const int mode = SG_NEGCTL(g_negctl_lookup_theme_text);
  CrtTextFile f = open_text(ctx, path);
  const std::vector<uint8_t> k = read_cstr(ctx.mem, key);
  uint8_t buf[0x100] = {};
  auto strip_last = [&]() {
    const size_t n = std::strlen(reinterpret_cast<char *>(buf));
    if (n != 0) buf[n - 1] = 0;  // n == 0 writes the byte below the buffer: the high byte of a counter below 100
  };
  int32_t lines = 0;
  for (;;) {
    f.fgets(buf, 0xFA);
    strip_last();
    if (stricmp_c(buf, k.data()) == 0) break;
    if (++lines >= 0x64) return -1;
  }
  lines = 0;
  int32_t counter = 0;
  f.fgets(buf, 0xFA);
  strip_last();
  const uint32_t sep = state_va(ctx, &GameState::f_strpool_4c3f70);
  for (;;) {
    std::vector<uint8_t> line(buf, buf + std::strlen(reinterpret_cast<char *>(buf)) + 1);
    write_bytes(ctx.mem, text_va(ctx) + vstrlen(ctx.mem, text_va(ctx)), line);
    if (mode != 2) cat(ctx, sep);
    f.fgets(buf, 0xFA);
    strip_last();
    ++lines;
    if (buf[0] == '*' && mode != 1) break;
    if (++counter >= 0x64) break;
  }
  return lines;
}

// ------------------------------------------------------------------------------------------------ theme files
// loadThemeFile (re/analysis/course/00437fa0): see course.h. The original copies `name` into a 0x40-byte frame slot
// first (a longer name overruns the frame, U-0824); the port copies it to the host, which also keeps a name that lies in
// g_textBuffer intact. fread of a short file copies what is left (the original's semantics); the 16-byte record is a
// frame slot the original does not initialise, so a file too short for it leaves stack bytes (U-0824; zero here).
int32_t load_theme_file(Ctx &ctx, uint32_t name, int32_t index, int32_t slot) {
  const int mode = SG_NEGCTL(g_negctl_load_theme_file);
  const std::vector<uint8_t> path = read_cstr(ctx.mem, name);
  const uint32_t text = text_va(ctx);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c84e8);
  if (index > 0x4C) {
    vstrcpy(ctx.mem, text, pool + 0xC);
  } else {
    vstrcpy(ctx.mem, text, pool);
    cat(ctx, state_va(ctx, &GameState::g_theme));
    cat(ctx, state_va(ctx, &GameState::f_strpool_4c84e4));
  }
  write_bytes(ctx.mem, text + vstrlen(ctx.mem, text), path);
  const std::string file = read_name(ctx.mem, text);
  GameFiles &files = ctx.files();
  if (!files.exists(file)) return 0;
  const std::vector<uint8_t> data = files.read_all(file);
  size_t pos = 0;
  auto fread_va = [&](uint32_t dst, uint32_t n) {
    for (uint32_t i = 0; i < n && pos < data.size(); ++i) ctx.mem.store<uint8_t>(dst + i, data[pos++]);
  };
  const Ptr<GolferType> type = state_arr(ctx, &GameState::g_golferTypes) + index;
  fread_va(type.va(), 0x230);
  fread_va((state_arr(ctx, &GameState::g_golferPhrases) + index).va(), 0x4E2);
  uint8_t rec[16] = {};
  for (int i = 0; i < 16 && pos < data.size(); ++i) rec[i] = data[pos++];
  if (slot != -1 && (index != 0 || (ctx.gs().g_flags & 0x4000000u) != 0)) {
    const uint32_t dst = golfer_at(ctx, slot).field(&Golfer::skill).va();
    for (int i = 0; i < 16; ++i) ctx.mem.store<uint8_t>(dst + u32(i), rec[i]);
    if (index == 0 && mode != 2) std::memcpy(ctx.gs().g_playerSkills, rec, 16);
  }
  pos += 8;  // eight more bytes read over the record and dropped
  // Image::loadPcx(g_image5a7148, 0, file) decodes the portrait from the rest of the file: render backend, not ported here
  const uint8_t demo = type.field(&GolferType::demographics).get();
  const int8_t body = type.field(&GolferType::body).get();
  const int32_t k = ((~int32_t(demo)) >> 7) & 1;
  if (body >= (mode == 1 ? 0x13 : 0x14)) {
    int32_t &used = ctx.gs().g_loopCounter59b76c[k];
    if (used < 0x48) {
      type.field(&GolferType::body).set(static_cast<int8_t>(used));
      used = add32(used, 1);
      // three Surface_blit calls copy the portrait's 0x8c-square tiles into g_sprites5628e8: render backend
    }
  }
  return 1;
}

// loadThemes (re/analysis/course/004658b0): loadGolferRoster (3.3's) runs first; enable == 0 returns 0. Otherwise the
// pattern Themes\<g_theme>\*.chr is listed by listMatchingFiles (into g_fileNames, 0x64 bytes per entry) and for entry
// i: bit 0 of g_golferTypeState[i + 1].membership is set and loadThemeFile(entry, i + 1, -1) runs. Returns the count
// (0 when the listing is empty). Both callees are called through the registry (MissingResource while unregistered).
int32_t load_themes(Ctx &ctx, int32_t enable) {
  auto call = [&](const char *fname, std::initializer_list<uint32_t> args) {
    const FuncDef *f = peer(fname);
    if (!f) throw MissingResource{fname};
    uint32_t a[4] = {};
    size_t i = 0;
    for (uint32_t v : args) a[i++] = v;
    Ret r;
    f->fn(ctx, a, r);
    return r;
  };
  call("loadGolferRoster", {});
  if (enable == 0) return 0;
  const uint32_t text = text_va(ctx);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c84e8);
  vstrcpy(ctx.mem, text, pool);
  cat(ctx, state_va(ctx, &GameState::g_theme));
  cat(ctx, pool + 0x84);
  const uint32_t none = state_va(ctx, &GameState::g_str4e9a84);
  const int32_t n = i32(call("listMatchingFiles", {text, none, none}).eax);
  if (n <= 0) return 0;
  const uint32_t names = state_va(ctx, &GameState::g_fileNames);
  int32_t i = 0;
  do {
    const Ptr<uint8_t> m = (state_arr(ctx, &GameState::g_golferTypeState) + add32(i, 1)).field(&GolferTypeState::membership);
    m.set(static_cast<uint8_t>(m.get() | (SG_NEGCTL(g_negctl_load_themes) == 1 ? 2u : 1u)));
    load_theme_file(ctx, names + u32(i) * 0x64u, add32(i, 1), -1);
    i = add32(i, 1);
  } while (i < n);
  return i;
}

// ------------------------------------------------------------------------------------------------ course switch
namespace {
Ret call_peer_or(Ctx &ctx, const char *fname, std::initializer_list<uint32_t> args, bool required) {
  Ret r;
  const FuncDef *f = peer(fname);
  if (!f) {
    if (required) throw MissingResource{fname};
    return r;
  }
  uint32_t a[4] = {};
  size_t i = 0;
  for (uint32_t v : args) a[i++] = v;
  f->fn(ctx, a, r);
  return r;
}
}  // namespace

// switchToCourse, the part after the course menu (re/analysis/course/00407d30): `chosen` is courseActionMenu's answer.
// The old course record's year_left = g_year's low byte; g_date is held at 0 while loadInterfaceArt(0) (render backend,
// called only when registered), buildGolferBioText, the finance row g_financeYears[g_year % 100].other =
// (int16) g_cash_div100, generateRandomCourse(chosen) and rebuildHeightfield run, then restored; g_courseType =
// the chosen record's course_type (set before those calls); applyCourseType(g_courseType) and refreshTerrain (render
// backend, when registered) end it. The 3.1a callees are called through the registry (MissingResource while absent).
void switch_to_course_chosen(Ctx &ctx, int32_t chosen) {
  const int mode = SG_NEGCTL(g_negctl_switch_to_course);
  GameState &gs = ctx.gs();
  const Ptr<CourseRecord> rec0 = state_arr(ctx, &GameState::g_courseTable);
  (rec0 + gs.g_courseIndex).field(&CourseRecord::year_left).set(static_cast<int8_t>(gs.g_year));
  const int32_t date = gs.g_date;
  gs.g_date = 0;
  gs.g_courseType = (rec0 + chosen).field(&CourseRecord::course_type).get();
  call_peer_or(ctx, "loadInterfaceArt", {0}, false);
  call_peer_or(ctx, "buildGolferBioText", {}, true);
  const int32_t row = int32_t(gs.g_year) % 100;
  (state_arr(ctx, &GameState::g_financeYears) + (mode == 1 ? row + 1 : row))
      .field(&FinanceYear::other)
      .set(static_cast<int16_t>(gs.g_cash_div100));
  call_peer_or(ctx, "generateRandomCourse", {u32(chosen)}, true);
  call_peer_or(ctx, "rebuildHeightfield", {}, true);
  gs.g_date = date;
  call_peer_or(ctx, "applyCourseType", {u32(int32_t(gs.g_courseType))}, true);
  call_peer_or(ctx, "refreshTerrain", {}, false);
}

// switchToCourse(arg): nothing while g_flags bit 0x4000000 is set; otherwise courseActionMenu(arg) (ui; through the
// registry) picks the course and -1 cancels (the original's eax is not a result: void).
void switch_to_course(Ctx &ctx, int32_t arg) {
  if (ctx.gs().g_flags & 0x4000000u) return;
  const int32_t chosen = i32(call_peer_or(ctx, "courseActionMenu", {u32(arg)}, true).eax);
  if (chosen == -1) return;
  switch_to_course_chosen(ctx, chosen);
}

// ------------------------------------------------------------------------------------------------ list
// listEraseRange (re/analysis/util/0044ac00): std::list erase(first, last) on the list `self` (size at +8): each node
// {next +0, prev +4} from first up to last is unlinked and released (operator delete: the port has no heap model, the
// node's memory stays the caller's, U-0823), the size drops by one per node; *out = last; returns out.
uint32_t list_erase_range(Ctx &ctx, uint32_t self, uint32_t out, uint32_t first, uint32_t last) {
  const Memory &m = ctx.mem;
  uint32_t node = first;
  while (node != last) {
    const uint32_t cur = node;
    node = m.load<uint32_t>(cur);
    const uint32_t next = m.load<uint32_t>(cur), prev = m.load<uint32_t>(cur + 4);
    m.store<uint32_t>(prev, next);
    m.store<uint32_t>(next + 4, prev);
    if (SG_NEGCTL(g_negctl_list_erase_range) != 1) m.store<int32_t>(self + 8, sub32(m.load<int32_t>(self + 8), 1));
  }
  m.store<uint32_t>(out, SG_NEGCTL(g_negctl_list_erase_range) == 2 ? first : node);
  return out;
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
int32_t s(uint32_t v) { return static_cast<int32_t>(v); }
void a_free_record_at_pos(Ctx &c, const uint32_t *a, Ret &) { free_record_at_pos(c, s(a[0]), s(a[1])); }
void a_init_records(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(init_records(c)); }
void a_clear_matching(Ctx &c, const uint32_t *a, Ret &) { clear_matching(c, s(a[0]), s(a[1])); }
void a_static_init_820f04(Ctx &c, const uint32_t *, Ret &r) {
  static_init_820f04(c);
  r.set_i32(c.gs().static_820f04);
}
void a_set_pending_course_info(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(set_pending_course_info(c, a[0])); }
void a_clear_buffers(Ctx &c, const uint32_t *, Ret &) { clear_buffers(c); }
void a_append_string(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(append_string(c, s(a[0]))); }
void a_landmark_name(Ctx &c, const uint32_t *a, Ret &) { landmark_name(c, s(a[0]), s(a[1])); }
void a_build_golfer_name(Ctx &c, const uint32_t *a, Ret &) { build_golfer_name(c, s(a[0]), s(a[1])); }
void a_build_status_text(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(build_status_text(c, s(a[0]), s(a[1]))); }
void a_append_hole_name(Ctx &c, const uint32_t *a, Ret &) { append_hole_name(c, s(a[0])); }
void a_append_course_title(Ctx &c, const uint32_t *a, Ret &) { append_course_title(c, s(a[0])); }
void a_nearest_hole(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(nearest_hole(c, s(a[0]), s(a[1]))); }
void a_hole_quadrants(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(hole_quadrants(c, s(a[0]), s(a[1]), a[2])); }
void a_scenery_label(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(scenery_label(c, s(a[0]), s(a[1]), s(a[2]))); }
void a_file_exists(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(file_exists(c, a[0])); }
void a_lookup_theme_text(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(lookup_theme_text(c, a[0], a[1])); }
void a_list_erase_range(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(list_erase_range(c, a[0], a[1], a[2], a[3])); }
void a_load_theme_file(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(load_theme_file(c, a[0], s(a[1]), s(a[2]))); }
void a_load_themes(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(load_themes(c, s(a[0]))); }
void a_switch_to_course(Ctx &c, const uint32_t *a, Ret &) { switch_to_course(c, s(a[0])); }
void a_seed_course_table(Ctx &c, const uint32_t *, Ret &) { seed_course_table(c); }
void a_build_scenario_intro(Ctx &c, const uint32_t *, Ret &) { build_scenario_intro(c); }

const FuncDef kFuncs[] = {
    {"freeRecordAtPos", "default", "void", 2, 0, a_free_record_at_pos, &g_negctl_free_record_at_pos},
    {"initRecords", "default", "int", 0, 0, a_init_records, &g_negctl_init_records},
    {"clearMatching", "default", "void", 2, 0, a_clear_matching, &g_negctl_clear_matching},
    {"staticInit_820f04", "default", "void", 0, 0x027F, a_static_init_820f04, &g_negctl_static_init_820f04},
    {"setPendingCourseInfo", "default", "int", 1, 0, a_set_pending_course_info, &g_negctl_set_pending_course_info},
    {"clearBuffers", "default", "void", 0, 0, a_clear_buffers, &g_negctl_clear_buffers},
    {"appendString", "default", "int", 1, 0, a_append_string, &g_negctl_append_string},
    {"landmarkName", "default", "void", 2, 0, a_landmark_name, &g_negctl_landmark_name},
    {"buildGolferName", "default", "void", 2, 0, a_build_golfer_name, &g_negctl_build_golfer_name},
    {"buildStatusText", "default", "int", 2, 0, a_build_status_text, &g_negctl_build_status_text},
    {"appendHoleName", "default", "void", 1, 0, a_append_hole_name, &g_negctl_append_hole_name},
    {"appendCourseTitle", "default", "void", 1, 0, a_append_course_title, &g_negctl_append_course_title},
    {"nearestHole", "default", "int", 2, 0x027F, a_nearest_hole, &g_negctl_nearest_hole},
    {"holeQuadrants", "default", "int", 3, 0x027F, a_hole_quadrants, &g_negctl_hole_quadrants},
    {"sceneryLabel", "default", "int", 3, 0, a_scenery_label, &g_negctl_scenery_label},
    {"fileExists", "default", "int", 1, 0, a_file_exists, &g_negctl_file_exists},
    {"lookupThemeText", "default", "int", 2, 0, a_lookup_theme_text, &g_negctl_lookup_theme_text},
    {"listEraseRange", "thiscall", "pointer", 4, 0, a_list_erase_range, &g_negctl_list_erase_range},
    {"loadThemeFile", "default", "int", 3, 0, a_load_theme_file, &g_negctl_load_theme_file},
    {"loadThemes", "default", "int", 1, 0, a_load_themes, &g_negctl_load_themes},
    {"switchToCourse", "default", "void", 1, 0, a_switch_to_course, &g_negctl_switch_to_course},
    {"seedCourseTable", "default", "void", 0, 0x027F, a_seed_course_table, &g_negctl_seed_course_table},
    {"buildScenarioIntro", "default", "void", 0, 0x027F, a_build_scenario_intro, &g_negctl_build_scenario_intro},
};
}  // namespace

extern const ModuleDef kModule_course = {"course", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
