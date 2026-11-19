// P37 hole advisor (re/roadmap/P37_HOLE_ADVISOR.md; every rule is cited in re/analysis/course/HOLE_RATING.md): analyze_hole
// recomputes, read-only, what rateCourseHoles (events_news.inc rate_course_holes) derives for one hole: the band test, the
// three skill deltas, the notice mask (the hole type), the fun term, the award test and the sameness counter. The advisor's
// lines are built from it. Oracle: port/test/test_p37_hole_advice.py runs rateCourseHoles on random records and compares.
// Nothing in this file writes the GameState.
#include "holeadvice.h"

#include <algorithm>
#include <cstdio>
#include <memory>

#include "commands_internal.h"
#include "portsim/sim/lang.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
using cm::G;

constexpr uint32_t kRec = 0x208;
constexpr uint32_t kWhite = 0x80007fffu, kGood = 0x800013e4u, kBad = 0x80007d08u, kHead = 0x80007f00u, kGrey = 0x800056b5u;

int32_t notice_of(Ctx &ctx, int32_t hole, int32_t d, HoleAnalysis *a);

// the per-column sums (a prior of 8 plays at par) and the band test, as rate_course_holes builds them
void columns(Ctx &ctx, uint32_t rec, int32_t par, int32_t col20[8], int32_t col40[8], int32_t &n, int32_t &s) {
  for (int c = 0; c < 8; ++c) {
    col20[c] = par << 3;
    col40[c] = 8;
  }
  n = s = 0;
  for (int row = 1; row < 10; ++row)
    for (int c = 0; c < 8; ++c) {
      const int32_t v = G<int16_t>(ctx, rec + 0x28 + 2u * static_cast<uint32_t>(c * 11 + row));
      n += v;
      col40[c] += v;
      col20[c] += v * row;
      s += v * row;
    }
}

// the hole's notice mask (rateCourseHoles' `notable` before the forced-event branch); fills a's deltas when given
int32_t notice_of(Ctx &ctx, int32_t hole, int32_t d, HoleAnalysis *a) {
  const uint32_t rec = GS(g_holes) + static_cast<uint32_t>(hole) * kRec;
  const int32_t par = G<int8_t>(ctx, rec);
  int32_t col20[8], col40[8], n = 0, s = 0;
  columns(ctx, rec, par, col20, col40, n, s);
  const int32_t thr = d != 0 ? 0x32 : 0x19;
  int32_t notable = 0, min_delta = 100, min_bit = 0;
  static const int kCol[3] = {6, 5, 3};
  for (int k = 0; k < 3; ++k) {
    const int c = kCol[k];
    if (col40[7] == 0 || col40[c] == 0) continue;
    const int32_t delta = (col20[c] * 100) / col40[c] - (col20[7] * 100) / col40[7];
    if (delta >= thr) notable |= 1 << k;
    // the length column starts the minimum search with `< 100`, the other two with `< min_delta` (the same compare while
    // min_delta is 100)
    if (delta < min_delta) {
      min_bit = 1 << k;
      min_delta = delta;
    }
    if (a) {
      a->has_delta[k] = true;
      a->delta[k] = delta;
      if (delta > 0x31) a->notable_bits |= 0x100u << k;
      a->skill += delta;
    }
  }
  if (min_delta < (d != 0 ? 0x64 : 0x32)) notable &= ~min_bit;
  if (a) {
    a->n = n;
    a->strokes = s;
    a->notice_threshold = thr;
  }
  return notable;
}

std::string cents(int32_t v) {   // +1.25 / -0.40 (hundredths)
  char b[24];
  const int32_t m = v < 0 ? -v : v;
  std::snprintf(b, sizeof b, "%s%d.%02d", v < 0 ? "-" : "+", m / 100, m % 100);
  return b;
}
std::string avg(int32_t v100) {
  char b[24];
  std::snprintf(b, sizeof b, "%d.%02d", v100 / 100, (v100 < 0 ? -v100 : v100) % 100);
  return b;
}
std::string money(int32_t units) {   // cash / 100 units as dollars with thousands commas
  int64_t v = static_cast<int64_t>(units) * 100;
  const bool neg = v < 0;
  if (neg) v = -v;
  std::string s = std::to_string(v), out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (i != 0 && (s.size() - i) % 3 == 0) out += ',';
    out += s[i];
  }
  return (neg ? "-" : "") + out;   // no '$': the panel's font (the palette tip's) has no glyph for it
}
std::string fmt(const char *f, const std::string &a = "", const std::string &b = "", const std::string &c = "",
                const std::string &e = "", const std::string &e5 = "") {
  // the English pattern translated as a whole, then {1}..{5} put in
  std::string t = lang::tr(f);
  const std::string *args[5] = {&a, &b, &c, &e, &e5};
  for (int k = 0; k < 5; ++k) {
    const std::string key = "{" + std::to_string(k + 1) + "}";
    const size_t at = t.find(key);
    if (at != std::string::npos) t.replace(at, key.size(), *args[k]);
  }
  return t;
}

// the game's own text of comment `kind` as the Hole Stats screen writes it (buildGolferComment(kind, word, hole * 11, 0x98)),
// and its tone word (g_commentTone: 0x80007d08 a complaint, 0x800023e8 praise). buildGolferComment writes the text buffer, the
// tone, g_thoughtOnPartner and the scratch golfer record 0x98: the whole GameState is copied before and put back after, so the
// advisor stays read-only.
std::string comment_text(Ctx &ctx, int32_t kind, uint32_t word, int32_t hole, uint32_t &tone) {
  const auto keep = std::make_unique<GameState>(ctx.gs());
  const uint32_t buf = GS(g_textBuffer);
  ctx.mem.store<uint8_t>(buf, 0);
  cm::call_peer(ctx, "buildGolferComment", {static_cast<uint32_t>(kind), word & 0x3fffu, static_cast<uint32_t>(hole * 11), 0x98});
  tone = ctx.gs().g_commentTone;
  std::string s;
  for (uint32_t i = 0; i < 0x200; ++i) {
    const uint8_t ch = ctx.mem.load<uint8_t>(buf + i);
    if (ch == 0) break;
    s += static_cast<char>(ch);
  }
  ctx.gs() = *keep;
  return lang::tr(s);   // in the current language, as the Hole Stats screen shows it
}

const char *const kTypeName[8] = {"none", "FREEWAY", "PRECISE", "CHALLENGE", "CREATIVE", "HEROIC", "STRATEGIC", "CLASSIC"};
const char *const kSkillName[3] = {"length", "accuracy", "imagination"};
const char *const kSkillHint[3] = {"a longer carry to the fairway or green",
                                   "narrow landing areas and hazards beside them",
                                   "doglegs, slopes and obstacles that need a shaped shot"};
const char *const kDifficulty[4] = {"Easy", "Moderate", "Difficult", "Impossible"};

// the change a complaint points to, by comment kind (the kinds' texts: log/roadmap/P37/comments.tsv, buildGolferComment with the
// scratch golfer; the terrain or object word comes from the hole's +0x16c)
const char *fix_for(int kind) {
  switch (kind) {
    case 0x02: return "widen the fairway where shots land";
    case 0x03: return "keep hazards out of the landing areas";
    case 0x04: return "add length or a hazard near the landing area";
    case 0x09: return "move this hole away from its neighbours' shots";
    case 0x0a: return "run a path around it";
    case 0x0c: return "move it off the line of play";
    case 0x0d: return "pull the water back from the line of play";
    case 0x0e: return "a refreshment stand or snack bar nearby";
    case 0x0f: return "a snack bar nearby";
    case 0x14: return "these golfers dislike the flowers near here";
    case 0x15: return "speed up play: fewer hazards, a marshal";
    case 0x17: return "see the score band";
    case 0x18: return "clear the brush where shots land";
    case 0x1e: return "change par, direction or bend from the hole before";
    case 0x23: case 0x24: return "the hole frustrates golfers: ease it";
    case 0x2b: return "a path around the slope, or flatten it";
    default: return nullptr;
  }
}
constexpr uint32_t kComplaintTone = 0x80007d08u, kWarn = 0x80007fe0u;
}  // namespace

HoleAnalysis analyze_hole(Ctx &ctx, int32_t hole) {
  HoleAnalysis a;
  a.hole = hole;
  if (hole < 1 || hole > 18) return a;
  const uint32_t base = GS(g_holes), rec = base + static_cast<uint32_t>(hole) * kRec;
  a.par = G<int8_t>(ctx, rec);
  a.open = a.par != 0;
  a.difficulty = G<int32_t>(ctx, GS(g_difficulty));
  if (!a.open) return a;
  const int32_t d = a.difficulty;
  a.yards = G<int16_t>(ctx, rec + 4);
  a.plays = G<int32_t>(ctx, rec + 0x20);
  a.shots = G<int32_t>(ctx, rec + 0x24);
  a.takings = G<int32_t>(ctx, rec + 0x1f4);
  a.spent = G<int32_t>(ctx, rec + 0x1f0);   // showCourseReport: net = +0x1f4 - +0x1f0 - +0x1f8
  a.upkeep = G<int32_t>(ctx, rec + 0x1f8);
  a.flags = G<uint32_t>(ctx, rec + 0x200);
  a.best100 = (a.flags & 1u) != 0;
  a.top18 = (a.flags & 2u) != 0;
  a.mood_sum = G<int16_t>(ctx, rec + 0x158);
  for (uint32_t k = 0; k < 64; ++k) a.comment[k] = G<int16_t>(ctx, rec + 0xd8 + 2 * k);

  a.type = notice_of(ctx, hole, d, &a);
  if (a.n >= 10) {
    a.judged = true;
    a.too_hard = ((6 - d) * a.n) / 3 + a.par * a.n < a.strokes;
    a.too_easy = a.strokes < a.par * a.n - ((3 - d) * a.n) / 6;
  }
  if (a.n > 0) a.avg100 = (a.strokes * 100) / a.n;
  a.hard_avg100 = a.par * 100 + ((6 - d) * 100) / 3;
  a.easy_avg100 = a.par * 100 - ((3 - d) * 100) / 6;
  if (a.plays != 0) a.fun = (a.mood_sum * 100) / (a.shots / 2 + 4 + a.plays);
  a.fun_sub = (a.fun * 6) / ((d != 0 ? 1 : 0) + 2);
  a.magazine_skill = a.skill;
  if (d < 2 && a.magazine_skill < a.fun_sub) a.magazine_skill = a.fun_sub;

  if (hole > 1) {
    // the previous open hole's notice mask (rateCourseHoles' prevNotable, -1 before the first open hole)
    int32_t prev_notable = -1;
    for (int32_t h = hole - 1; h >= 1; --h)
      if (G<int8_t>(ctx, base + static_cast<uint32_t>(h) * kRec) != 0) {
        prev_notable = notice_of(ctx, h, d, nullptr);
        break;
      }
    const uint32_t prev = rec - kRec;   // the record before this one, open or not, as the original reads it
    a.same[0] = a.type == prev_notable && a.plays > 7 && a.type != 7;
    a.same[1] = ((G<uint32_t>(ctx, prev + 0x200) ^ a.flags) & 0x60u) == 0;
    a.same[2] = a.comment[0x2d] == 0 && a.comment[0x2e] == 0;
    a.same[3] = a.par == G<int8_t>(ctx, prev);
    const int32_t a1 = angle_fixed(G<int32_t>(ctx, prev + 0x18) - G<int32_t>(ctx, prev + 8),
                                   G<int32_t>(ctx, prev + 0x1c) - G<int32_t>(ctx, prev + 0xc));
    const int32_t a2 = angle_fixed(G<int32_t>(ctx, rec + 0x18) - G<int32_t>(ctx, rec + 8),
                                   G<int32_t>(ctx, rec + 0x1c) - G<int32_t>(ctx, rec + 0xc));
    const int32_t diff = a2 - a1, sg = diff >> 31;
    a.same[4] = (((diff >> 24) ^ sg) - sg) < 0x28;
    for (bool b : a.same) a.sameness += b ? 1 : 0;
    if (a.sameness != 0 && d < 2) a.sameness -= 1;
  }
  return a;
}

std::vector<AdviceLine> hole_advice_lines(Ctx &ctx, int32_t hole) {
  std::vector<AdviceLine> out;
  const HoleAnalysis a = analyze_hole(ctx, hole);
  if (!a.open) return out;
  const int32_t d = a.difficulty;
  out.push_back({fmt("Hole {1} advisor  ({2})", std::to_string(hole), lang::tr(kDifficulty[d & 3])), kHead});

  // the awards (takings over 200 / 400, skill over 2.00 / 3.00, and neither band flag)
  auto award = [&](const char *name, bool won, int32_t need_take, int32_t need_skill) {
    if (won) {
      out.push_back({fmt("{1}: won", lang::tr(name)), kGood});
      return;
    }
    std::string s = fmt("{1}: takings {2} (over {3}), skill {4} (over {5})", lang::tr(name), money(a.takings), money(need_take),
                        cents(a.magazine_skill), cents(need_skill));
    const bool ok = a.takings > need_take && a.magazine_skill > need_skill && !a.too_hard && !a.too_easy;
    if (a.too_hard) s += lang::tr(", blocked: too hard");
    else if (a.too_easy) s += lang::tr(", blocked: too easy");
    out.push_back({s, ok ? kGood : kWhite});
  };
  award("Best 100", a.best100, 200, 200);
  award("Top 18", a.top18, 400, 300);
  if (d < 2 && a.fun_sub > a.skill)
    out.push_back({fmt("  At {1} the fun counts as skill: {2}", lang::tr(kDifficulty[d & 3]), cents(a.fun_sub)), kGrey});

  // the score band
  if (!a.judged) {
    out.push_back({fmt("Score band: {1} of 10 plays recorded, not judged yet", std::to_string(a.n)), kGrey});
  } else {
    const std::string s = fmt("Average {1} on par {2}: too hard above {3}, too easy below {4}", avg(a.avg100),
                              std::to_string(a.par), avg(a.hard_avg100), avg(a.easy_avg100));
    const bool near = a.hard_avg100 - a.avg100 <= 25 || a.avg100 - a.easy_avg100 <= 25;
    out.push_back({s, a.too_hard || a.too_easy ? kBad : near ? kWarn : kGood});
    if (a.too_hard) out.push_back({lang::tr("  Too hard: golfers lose mood and no award. Ease the landing areas or shorten it."), kBad});
    else if (a.too_easy) out.push_back({lang::tr("  Too easy: golfers lose mood and no award. Add hazards or length."), kBad});
    else if (near)
      out.push_back({lang::tr(a.hard_avg100 - a.avg100 <= 25 ? "  Close to too hard: avoid making it harder"
                                                             : "  Close to too easy: avoid making it easier"),
                     kWarn});
  }

  // the skills and the hole type
  {
    std::string s = lang::tr("Skills:");
    for (int k = 0; k < 3; ++k)
      s += " " + lang::tr(kSkillName[k]) + " " + (a.has_delta[k] ? cents(a.delta[k]) : std::string("-"));
    s += fmt(", type {1}", lang::tr(kTypeName[a.type & 7]));
    out.push_back({s, kWhite});
    // the weakest skill outside the type: what it needs to join the type
    int best = -1;
    for (int k = 0; k < 3; ++k)
      if ((a.type & (1 << k)) == 0 && a.has_delta[k] && (best < 0 || a.delta[k] > a.delta[best])) best = k;
    if (best >= 0) {
      const int32_t need = d != 0 ? 100 : 50;   // a skill clears the notice and is not the dropped minimum
      out.push_back({fmt("  For {1}: {2} to {3} (now {4}) with {5}", lang::tr(kTypeName[(a.type | (1 << best)) & 7]),
                         lang::tr(kSkillName[best]), cents(need), cents(a.delta[best]), lang::tr(kSkillHint[best])),
                     kGrey});
    }
  }

  // the fun and the most frequent complaints
  out.push_back({fmt("Fun: {1} (mood {2} over {3} plays)", std::to_string(a.fun) + "%", std::to_string(a.mood_sum),
                     std::to_string(a.plays)),
                 a.fun < 0 ? kBad : kWhite});
  // the complaints: comment kinds the Hole Stats screen lists (below 0x32) in the complaint tone, most frequent first, in the
  // game's own words with the share the screen shows (count * 100 / plays)
  std::vector<std::pair<int32_t, int32_t>> c;   // (count, kind)
  for (int32_t k = 0; k < 0x32; ++k)
    if (a.comment[k] > 0) c.push_back({a.comment[k], k});
  std::stable_sort(c.begin(), c.end(), [](const auto &x, const auto &y) { return x.first > y.first; });
  const uint32_t rec = GS(g_holes) + static_cast<uint32_t>(hole) * kRec;
  int shown = 0;
  for (const auto &e : c) {
    if (shown == 3 || a.plays == 0) break;
    uint32_t tone = 0;
    const std::string said = comment_text(ctx, e.second, G<uint16_t>(ctx, rec + 0x16c + 2u * static_cast<uint32_t>(e.second)),
                                          hole, tone);
    if (tone != kComplaintTone || said.empty()) continue;
    const char *fix = fix_for(e.second);
    std::string s = "  " + std::to_string((e.first * 100) / a.plays) + "% '" + said + "'";
    if (fix) s += " - " + lang::tr(fix);
    out.push_back({s, kBad});
    ++shown;
  }
  if (shown == 0 && a.plays != 0) out.push_back({lang::tr("  No complaints recorded"), kGood});

  // variety against the previous hole (the sameness counter: 0 lets golfers praise the variety; from 4 up, at Moderate and
  // above, a tee shot can draw the monotony complaint)
  if (hole > 1) {
    static const char *const kSame[5] = {"same type", "same bend", "no downhill shot", "same par", "same direction"};
    std::string s;
    for (int k = 0; k < 5; ++k)
      if (a.same[k]) s += (s.empty() ? "" : ", ") + lang::tr(kSame[k]);
    const std::string prev = std::to_string(hole - 1), sc = std::to_string(a.sameness);
    if (s.empty()) out.push_back({fmt("Variety vs hole {1}: nothing alike (golfers like the variety)", prev), kGood});
    else if (a.sameness == 0)
      out.push_back({fmt("Variety vs hole {1}: {2}, score 0 (golfers like the variety)", prev, s), kGood});
    else if (a.sameness < 4)
      out.push_back({fmt("Variety vs hole {1}: {2}, score {3} (less variety praise)", prev, s, sc), kWhite});
    else
      out.push_back({fmt("Variety vs hole {1}: {2}, score {3} (monotony complaints)", prev, s, sc), kBad});
  }

  // money (the course report's columns: takings, building cost and upkeep at the hole, net)
  const int32_t net = a.takings - a.spent - a.upkeep;
  out.push_back({fmt("Money: taken {1}, built {2}, upkeep {3}, net {4}", money(a.takings), money(a.spent), money(a.upkeep), money(net)),
                 net < 0 ? kBad : kWhite});
  return out;
}

namespace {
// P37B: the takings follow the golfers' mood, so the way to raise them is the hole's most frequent complaint: its fix (or the
// game's own words when the kind has no fix), else a generic line
std::string happier(Ctx &ctx, const HoleAnalysis &a) {
  const uint32_t rec = GS(g_holes) + static_cast<uint32_t>(a.hole) * kRec;
  int32_t best = -1;
  std::vector<std::pair<int32_t, int32_t>> c;
  for (int32_t k = 0; k < 0x32; ++k)
    if (a.comment[k] > 0) c.push_back({a.comment[k], k});
  std::stable_sort(c.begin(), c.end(), [](const auto &x, const auto &y) { return x.first > y.first; });
  std::string said;
  for (const auto &e : c) {
    uint32_t tone = 0;
    said = comment_text(ctx, e.second, G<uint16_t>(ctx, rec + 0x16c + 2u * static_cast<uint32_t>(e.second)), a.hole, tone);
    if (tone == kComplaintTone && !said.empty()) {
      best = e.second;
      break;
    }
  }
  if (best < 0) return lang::tr("happier golfers (fees follow mood)");
  if (const char *fix = fix_for(best)) return lang::tr(fix);
  return "'" + said + "'";
}
}  // namespace

// P37B: one row per open hole, columns separated by '\t' (advice_panel's tab stops), ordered by progress to the next award
std::vector<AdviceLine> course_overview_lines(Ctx &ctx) {
  std::vector<AdviceLine> out;
  struct Row {
    HoleAnalysis a;
    int32_t progress = 0;   // percent of the next award's weaker requirement; 1000 = both awards won
    bool blocked = false;
  };
  std::vector<Row> rows;
  int32_t open = 0, won1 = 0, won2 = 0;
  for (int32_t h = 1; h <= 18; ++h) {
    Row r;
    r.a = analyze_hole(ctx, h);
    if (!r.a.open) continue;
    ++open;
    won1 += r.a.best100 ? 1 : 0;
    won2 += r.a.top18 ? 1 : 0;
    r.blocked = r.a.too_hard || r.a.too_easy;
    if (r.a.best100 && r.a.top18) {
      r.progress = 1000;
    } else {
      const int32_t need_t = r.a.best100 ? 400 : 200, need_s = r.a.best100 ? 300 : 200;
      const int32_t pt = std::max(0, std::min(100, (r.a.takings * 100) / (need_t + 1)));
      const int32_t ps = std::max(0, std::min(100, (r.a.magazine_skill * 100) / (need_s + 1)));
      r.progress = std::min(pt, ps);
    }
    rows.push_back(r);
  }
  if (open == 0) return out;
  const GameState &gs = ctx.gs();
  const int32_t d = G<int32_t>(ctx, GS(g_difficulty));
  out.push_back({fmt("Course advisor  ({1})", lang::tr(kDifficulty[d & 3])), kHead});
  out.push_back({fmt("{1} holes, Best 100 won on {2}, Top 18 on {3}; course fun {4}, skill {5}", std::to_string(open),
                     std::to_string(won1), std::to_string(won2), std::to_string(G<int32_t>(ctx, GS(g_funRating))),
                     cents(G<int32_t>(ctx, GS(g_skillRatingX100)))),
                 kWhite});
  {
    const uint32_t mask = G<uint32_t>(ctx, GS(g_holeNoticeMask));
    std::string found;
    int n = 0;
    for (int k = 1; k <= 7; ++k)
      if (mask & (1u << k)) {
        found += (found.empty() ? "" : ", ") + lang::tr(kTypeName[k]);
        ++n;
      }
    out.push_back({fmt("Hole types announced: {1} of 7 {2}", std::to_string(n), found.empty() ? "" : "(" + found + ")"),
                   n == 7 ? kGood : kWhite});
  }
  (void)gs;
  std::stable_sort(rows.begin(), rows.end(), [](const Row &x, const Row &y) {
    const int kx = x.progress == 1000 ? 2 : x.blocked ? 1 : 0, ky = y.progress == 1000 ? 2 : y.blocked ? 1 : 0;
    if (kx != ky) return kx < ky;
    return x.progress > y.progress;
  });
  for (const Row &r : rows) {
    const HoleAnalysis &a = r.a;
    std::string next, todo;
    uint32_t col = kWhite;
    if (r.progress == 1000) {
      next = lang::tr("both won");
      col = kGood;
    } else {
      next = std::string(lang::tr(a.best100 ? "Top 18" : "Best 100")) + " " + std::to_string(r.progress) + "%";
      const int32_t need_t = a.best100 ? 400 : 200, need_s = a.best100 ? 300 : 200;
      if (a.too_hard) todo = lang::tr("too hard: ease it");
      else if (a.too_easy) todo = lang::tr("too easy: toughen it");
      else if (a.takings <= need_t && (a.takings * 100) / (need_t + 1) <= (a.magazine_skill * 100) / (need_s + 1))
        todo = happier(ctx, a);
      else if (a.magazine_skill <= need_s) {
        int weak = -1;
        for (int k = 0; k < 3; ++k)
          if (a.has_delta[k] && (weak < 0 || a.delta[k] < a.delta[weak])) weak = k;
        todo = weak >= 0 ? fmt("raise {1}", lang::tr(kSkillName[weak])) : lang::tr("more plays recorded");
      } else {
        todo = lang::tr("ready: waits for the next rating");
      }
      if (r.blocked) col = kBad;
      else if (a.judged && (a.hard_avg100 - a.avg100 <= 25 || a.avg100 - a.easy_avg100 <= 25)) col = kWarn;
    }
    if (a.sameness >= 4) todo += lang::tr(todo.empty() ? "vary from the hole before" : ", vary from the hole before");
    out.push_back({fmt("Hole {1}", std::to_string(a.hole)) + "\t" + next + "\t" + todo, col});
  }
  return out;
}

namespace {
// ---- test entries (port/test/test_p37_hole_advice.py) ----
// holeAdvice.analyze(hole, out): 32 dwords at out: open, par, n, strokes, judged, too_hard, too_easy, has_delta x3, delta x3,
// notable_bits, type, skill, fun, fun_sub, magazine_skill, takings, same x5, sameness, avg100, hard_avg100, easy_avg100
void a_analyze(Ctx &c, const uint32_t *a, Ret &) {
  const HoleAnalysis h = analyze_hole(c, static_cast<int32_t>(a[0]));
  const int32_t w[32] = {h.open,        h.par,      h.n,        h.strokes,     h.judged,      h.too_hard,  h.too_easy,
                         h.has_delta[0], h.has_delta[1], h.has_delta[2], h.delta[0], h.delta[1], h.delta[2],
                         static_cast<int32_t>(h.notable_bits), h.type, h.skill, h.fun, h.fun_sub, h.magazine_skill,
                         h.takings,     h.same[0],  h.same[1],  h.same[2],     h.same[3],     h.same[4],   h.sameness,
                         h.avg100,      h.hard_avg100, h.easy_avg100, 0, 0, 0};
  for (uint32_t k = 0; k < 32; ++k) c.mem.store<int32_t>(a[1] + 4 * k, w[k]);
}
// holeAdvice.lines(hole, out, cap): the lines joined by '\n', NUL-terminated, at most cap bytes; returns the length
void a_lines(Ctx &c, const uint32_t *a, Ret &r) {
  std::string s;
  for (const AdviceLine &l : hole_advice_lines(c, static_cast<int32_t>(a[0]))) s += l.text + "\n";
  if (a[2] == 0) return;
  const size_t n = std::min<size_t>(s.size(), a[2] - 1);
  for (size_t i = 0; i < n; ++i) c.mem.store<uint8_t>(a[1] + static_cast<uint32_t>(i), static_cast<uint8_t>(s[i]));
  c.mem.store<uint8_t>(a[1] + static_cast<uint32_t>(n), 0);
  r.set_u32(static_cast<uint32_t>(n));
}
// holeAdvice.comment(kind, hole, out, cap): the game's text of comment `kind` at out (NUL-terminated); returns the tone word
void a_comment(Ctx &c, const uint32_t *a, Ret &r) {
  uint32_t tone = 0;
  const std::string s = comment_text(c, static_cast<int32_t>(a[0]), 0, static_cast<int32_t>(a[1]), tone);
  if (a[3] != 0) {
    const size_t n = std::min<size_t>(s.size(), a[3] - 1);
    for (size_t i = 0; i < n; ++i) c.mem.store<uint8_t>(a[2] + static_cast<uint32_t>(i), static_cast<uint8_t>(s[i]));
    c.mem.store<uint8_t>(a[2] + static_cast<uint32_t>(n), 0);
  }
  r.set_u32(tone);
}
// holeAdvice.course(out, cap): the overview's lines joined by '\n' (cells by '\t'); returns the length
void a_course(Ctx &c, const uint32_t *a, Ret &r) {
  std::string s;
  for (const AdviceLine &l : course_overview_lines(c)) s += l.text + "\n";
  if (a[1] == 0) return;
  const size_t n = std::min<size_t>(s.size(), a[1] - 1);
  for (size_t i = 0; i < n; ++i) c.mem.store<uint8_t>(a[0] + static_cast<uint32_t>(i), static_cast<uint8_t>(s[i]));
  c.mem.store<uint8_t>(a[0] + static_cast<uint32_t>(n), 0);
  r.set_u32(static_cast<uint32_t>(n));
}
int g_negctl_holeadvice = 0;
const FuncDef kFuncs[] = {
    {"holeAdvice.course", "default", "int", 2, 0, a_course, &g_negctl_holeadvice},
    {"holeAdvice.analyze", "default", "void", 2, 0, a_analyze, &g_negctl_holeadvice},
    {"holeAdvice.lines", "default", "int", 3, 0, a_lines, &g_negctl_holeadvice},
    {"holeAdvice.comment", "default", "int", 4, 0, a_comment, &g_negctl_holeadvice},
};
}  // namespace

extern const ModuleDef kModule_holeadvice = {"holeadvice", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
