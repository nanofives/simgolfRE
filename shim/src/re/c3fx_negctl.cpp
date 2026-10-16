// Negative-control build of batch c3fx (never list this file in shim/re_batches.txt): shim/src/re/c3fx.cpp compiled with
// C3FX_NEGCTL, which exports the switch C3fxNegCtl and makes each WRONG(k, right, wrong) / NEG_ON(k) site take its wrong
// value while the fixture has armed the switch (__freeze.negOn(), after the freeze) and the environment variable C3FX_NEG
// is k. Built into instances\c3fx_neg through log/c3/c3fx_neg_batches.txt (this file only). The details,
// showCharacterEditor 0x004385d0:
//   1 the gender control (17) loads the other background sheet
//   2 control 13 steps record + 0x24 by 2 on a left press
//   3 exit does not copy the preview flags into the golfer's byte +0x11
//   4 cancel (18) also ends the editor
//   5 the skill bar fill colour is chosen by skill == 0 instead of != 0
//   6 a comment-line press edits the line after the one under the pointer
//   7 the hover tip shows after 10 repeats instead of 11
//   8 the hair-style control (10) steps by 2 on a left press
//   9 the name-area hot line is (y - 0x18) / 21 instead of / 20
//  10 the profession line's highlight colour is 0x80007ffe instead of 0x80007fff
//  11 the preset (12) copies the preset's byte +2 into record + 0x28 instead of +3
//  12 the exit writes record + 0x25 into the story table's byte 3 instead of + 0x24
//  99 the null control: the body returns 0 at once
#define C3FX_NEGCTL
#include "c3fx.cpp"
