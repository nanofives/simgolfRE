// P37 hole advisor (re/roadmap/P37_HOLE_ADVISOR.md, re/analysis/course/HOLE_RATING.md): what rateCourseHoles derives for one
// hole, recomputed read-only, and the advisor's lines built from it. Nothing here writes the GameState.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "portsim/sim/commands.h"

namespace portsim {

struct HoleAnalysis {
  bool open = false;          // par byte != 0
  int32_t hole = 0, par = 0, yards = 0, difficulty = 0;
  int32_t plays = 0;          // record +0x20 (tee shots, decays)
  int32_t shots = 0;          // record +0x24
  int32_t n = 0, strokes = 0; // the histogram's counted plays (rows 1..9) and their strokes
  bool judged = false;        // n >= 10: the band test runs
  bool too_hard = false, too_easy = false;  // the band test now (rateCourseHoles' bits 4 / 8)
  // the band as average strokes x100 at this difficulty: too hard above hard_avg100, too easy below easy_avg100
  int32_t avg100 = 0, hard_avg100 = 0, easy_avg100 = 0;
  bool has_delta[3] = {false, false, false};  // length, accuracy, imagination
  int32_t delta[3] = {0, 0, 0};               // hundredths of a stroke
  uint32_t notable_bits = 0;  // flag bits 0x100 / 0x200 / 0x400
  int32_t type = 0;           // notice mask 0..7 (1 FREEWAY .. 7 CLASSIC)
  int32_t notice_threshold = 0;
  int32_t skill = 0;          // the hole's sum of deltas
  int32_t fun = 0;            // the hole's term of g_funRating (0 when plays == 0)
  int32_t fun_sub = 0;        // 6 fun / (2 + (difficulty != 0)), counted at difficulty <= 1
  int32_t magazine_skill = 0; // skill as the award test sees it
  int32_t takings = 0;        // record +0x1f4 (cash / 100)
  int32_t spent = 0;          // record +0x1f0, building cost at the hole (cash / 100)
  int32_t upkeep = 0;         // record +0x1f8, upkeep (cash / 100); showCourseReport's net = takings - spent - upkeep
  uint32_t flags = 0;         // record +0x200 as stored
  bool best100 = false, top18 = false;
  // the sameness counter's terms against the previous hole (holes 2..18): same type, same bend, no downhill comment, same par,
  // same direction; counter = their sum, minus 1 at difficulty < 2 when non-zero (rateCourseHoles' +0x1fc)
  bool same[5] = {false, false, false, false, false};
  int32_t sameness = 0;
  int32_t mood_sum = 0;       // record +0x158
  int32_t comment[64] = {};   // record +0xd8
};

HoleAnalysis analyze_hole(Ctx &ctx, int32_t hole);

struct AdviceLine {
  std::string text;
  uint32_t colour;   // a drawCenteredText colour word (0x8000 | RGB555)
};
std::vector<AdviceLine> hole_advice_lines(Ctx &ctx, int32_t hole);
// P37B: the course overview: totals, then one row per open hole (columns separated by '\t') ordered by progress to the next
// magazine award (blocked holes and holes with both awards last)
std::vector<AdviceLine> course_overview_lines(Ctx &ctx);
constexpr int kCourseOverviewHeaderRows = 3;   // the summary lines before the hole rows

}  // namespace portsim
