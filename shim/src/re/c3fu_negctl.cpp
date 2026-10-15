// Negative-control build of batch c3fu: the bodies of c3fu.cpp with NEG(k, right, wrong) live. Never listed in
// shim/re_batches.txt; built only through log/c3/c3fu_negctl_batches.txt into instances\c3fu (never original\).
// The exported switch word 0 is armed by re/frida/js/freeze.js negOn() after a key froze the first thread; the detail k that
// goes wrong is SIMGOLF_NEGSEL (one per boot):
//   0      none (the build behaves like c3fu.cpp)
//   1..15  the single wrong detail k of c3fu.cpp (a constant, an address, a skipped call, an offset)
//   99     every hook of the batch returns at once with 0 (the null control)
#include "hooks.h"
#include <stdlib.h>

extern "C" __declspec(dllexport) volatile int C3fuNegCtl[4] = {0, 0x5147454e, 0, 0};

namespace {
int C3fuNegSel() {
    static int sel = -1;
    if (sel < 0) {
        char buf[16] = {0};
        sel = GetEnvironmentVariableA("SIMGOLF_NEGSEL", buf, sizeof(buf)) ? atoi(buf) : 0;
    }
    return sel;
}
}  // namespace

#define C3FU_NEGCTL 1
#define NEG(k, right, wrong) ((C3fuNegCtl[0] && C3fuNegSel() == (k)) ? (wrong) : (right))
#define C3FU_NULL() (C3fuNegCtl[0] && C3fuNegSel() == 99)
#include "c3fu.cpp"
