// Negative-control build of batch m36d: the bodies of m36d.cpp with NEG(k, right, wrong) live. Never listed in
// shim/re_batches.txt; built only through log/c3/m36d_negctl_batches.txt into instances\m36d (never original\).
// The exported switch word 0 is armed by re/frida/js/freeze.js negOn() after a key froze the first thread; the detail k that
// goes wrong is SIMGOLF_NEGSEL (one per boot):
//   0      none (the build behaves like m36d.cpp)
//   1      ListModel offset read from vbtable+4 instead of +8
//   2      seek resets the index to 1 instead of 0
//   3      seek follows the node's +8 instead of its next pointer +0xc
//   4      bit index taken mod 16 instead of mod 32
//   5      flagIsSet reads the word after the mask (+0xf4)
//   6      flagSet sets only for value == 1 (any other nonzero clears)
//   7      flagSet clears with `and bit` instead of `and ~bit`
//   99     both hooks return at once with 0 (the null control)
#include "hooks.h"
#include <stdlib.h>

extern "C" __declspec(dllexport) volatile int M36dNegCtl[4] = {0, 0x5147454e, 0, 0};

namespace {
int M36dNegSel() {
    static int sel = -1;
    if (sel < 0) {
        char buf[16] = {0};
        sel = GetEnvironmentVariableA("SIMGOLF_NEGSEL", buf, sizeof(buf)) ? atoi(buf) : 0;
    }
    return sel;
}
}  // namespace

#define M36D_NEGCTL 1
#define NEG(k, right, wrong) ((M36dNegCtl[0] && M36dNegSel() == (k)) ? (wrong) : (right))
#define M36D_NULL() (M36dNegCtl[0] && M36dNegSel() == 99)
#include "m36d.cpp"
