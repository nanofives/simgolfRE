// Negative-control build of batch c3fw (never list this file in shim/re_batches.txt): shim/src/re/c3fw.cpp compiled with
// C3FW_NEGCTL, which exports the switch C3fwNegCtl and makes each WRONG(k, right, wrong) site take its wrong value while
// the fixture has armed the switch (__freeze.negOn(), after the freeze) and the environment variable C3FW_NEG is k.
// Built into instances\c3fw_neg through log/c3/c3fw_neg_batches.txt (this file only). The wrong details,
// loadGameBrowserScreen 0x0043b610:
//   1 a hovered button is redrawn with its normal image (element k) instead of its highlighted one (5 + k)
//   2 a list click previews the name after the clicked one
//   3 a confirmed delete in championship mode removes from the saved-games directory
//   4 a load that changed the theme does not reload the themes
//   5 the scroll-down button moves 3 names instead of 4
//   6 the back path does not restore the flags word 0x0059e7b8
//   7 the exit destroys 14 of the 15 frame Surfaces
//   8 the per-hole par is read one byte off (-3 instead of -4)
// showThemePackSelect 0x004725b0:
//   11 the newly hot button is drawn from array B (normal) instead of A (highlighted)
//   12 the theme row of a press is (y - 0x5b) / 31 instead of / 32
//   13 the current-theme marker is element A6 instead of A7
//   14 the back exit does not restore the default theme name
// pickAProScreen 0x0043a8c0:
//   15 a hovered button is redrawn with its normal image (element k) instead of 5 + k
//   16 scroll down moves 5 names instead of 4
//   17 a row press previews the next name
//   18 the skill percentages are skill * 9 instead of * 10
// courseActionMenu 0x0046f550:
//   19 a site press buys the next site's course
//   20 the owned-course mask gets the next record's bit
//   21 the nearest site blinks on bit 4 of the frame counter instead of bit 8
//   22 the nearest-site test uses the map spot + 0x18 instead of + 0x19 on x
#define C3FW_NEGCTL
#include "c3fw.cpp"
