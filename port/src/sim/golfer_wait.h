// P36B (Mariano's play-test 18 item 2, log/roadmap/P36/P36B_notes.md): why a golfer stands. updateGolfers (tick_golfers.inc)
// tags each golfer-tick with the rule that holds the golfer; a rule that sets the wait counter (Golfer +0x9e) also keeps its
// reason for as long as that wait runs. The tags live beside the simulation (no GameState byte, nothing the state hash or a
// save sees, no branch reads them), so the sim is unchanged; the golfer info panel shows them (app option, P36B part 2) and
// SG_P36B_WAITLOG=<file> writes the development tally (wait reason x hole x golfer-ticks).
#pragma once

namespace portsim {
class Ctx;

enum GolferWait : unsigned char {
  kGwNone = 0,
  kGwWalk,             // stepped this tick
  kGwSwing,            // the swing animation (swing counter +0x20 >= 2, or the address pose state 0x10)
  kGwBall,             // its own ball in flight or rolling
  kGwAddress,          // at its ball, the random pre-shot gate (updateGolfers+0x370c..+0x3768) did not open this tick
  kGwAheadTee,         // look-ahead: a golfer of an older group with more strokes stands within 0x64 of the tee
  kGwAheadBall,        // look-ahead: a golfer ahead has its ball within 0x25 of the target
  kGwAheadAtTarget,    // look-ahead: a golfer ahead stands within 0x25 of the target
  kGwAheadLine,        // look-ahead: a golfer ahead stands in the line of fire (15 degrees, nearer than the target)
  kGwTeeQueue,         // at the tee, an older group of the hole has not teed off (the tee queue count L38)
  kGwPartnerTee,       // played, back at the tee until the partner tees off (partner-ahead flag L34)
  kGwPartnerFarther,   // the partner's ball is farther from the cup: the partner plays first (L74)
  kGwPartnerSwing,     // the partner swings
  kGwPartnerWatch,     // watching the partner's shot
  kGwPartnerPrevHole,  // no ball yet, the partner is still on an earlier hole
  kGwPartnerOrder,     // walking order: queued behind the partner at the tee (L40)
  kGwPartnerGreen,     // on the green, nearer the cup than the partner's ball
  kGwBump,             // another golfer in the way of the step
  kGwCrowded,          // in the way with 4 or more golfers of the hole still to tee off (the long wait, thought 0x15)
  kGwTurn,             // turned back on its path (the reversal pause)
  kGwNoPath,           // pathStep gave no direction
  kGwReactGood,        // celebrating a good shot (state 0xc)
  kGwReactBad,         // sulking after a bad lie (state 0xd)
  kGwPenalty,          // ball in the water / out of bounds, dropping (state 0xd)
  kGwGreenRead,        // the pause after the putt is chosen (wait 0xffe7)
  kGwShotThrottle,     // another golfer chose its shot this frame (one AI shot choice per frame)
  kGwPlayerShot,       // the player's own golfer: the shot is the player's
  kGwClubhouse,        // in the clubhouse (hungry / thirsty)
  kGwPractice,         // on the practice green
  kGwSnack,            // at the snack bar
  kGwDrinks,           // at the drinks stand
  kGwRestroom,         // at the restrooms
  kGwRestSpot,         // tired: resting at a target tile (findNearestTargetTile)
  kGwThought,          // a thought's pause (emitGolferThought, kinds 0x14 / 0x16 / 0x1c)
  kGwStory,            // a story chat with its partner (advanceStory)
  kGwMembership,       // a membership request (matchUpdate)
  kGwArrival,          // the pause of a pair that arrives (pairGolfersWithStory, mainLoop)
  kGwHoled,            // holed out: the scoring (matchUpdate)
  kGwLeaving,          // leaving the course
  kGwStand,            // stood still with no rule tagged this tick
  kGwIdle,             // mainLoop's idle animation at the tee (wait 0xfffe, strokes 0, state 0xb)
  kGwOtherWait,        // a wait set by code that keeps no reason here (mainLoop's golfer animation, a load)
  kGwCount
};

// a rule that sets golfer g's wait counter: keep `why` (and the golfer it waits for, -1 none) while that wait runs
void golfer_wait_set(int g, int why, int other = -1);
// golfer g's reason for the panel: the rule of its running wait, else the standing rule of its last tick (kGwNone when it
// moves); `other` gets the golfer it waits for or -1. date = the game date (g_date) the caller sees now.
int golfer_wait_reason(Ctx &ctx, int g, int &other);
// mainLoop's idle animation set golfer g's wait (0xfffe): it keeps the standing reason of its last tick
void golfer_wait_idle(int g);
// short identifier of a reason (the development log)
const char *golfer_wait_id(int why);
}  // namespace portsim
