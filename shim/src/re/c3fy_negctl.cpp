// Negctl control for c3fy (mainLoop 0x0040f5c0). NEVER list this file in shim/re_batches.txt or deploy it to original\.
// It compiles c3fy.cpp with -DC3FY_NEG, as the scenario A/B's control DLL (built through log/c3/c3fy_neg_batches.txt to
// shim/build_c3fy_neg/winmm.dll). The wrong body is chosen by c3fyNegMode() in c3fy.cpp: env SIMGOLF_C3FY_NEG when set
// (scenario_ab.py ab --negctl <dll> --env SIMGOLF_C3FY_NEG=2), else c3fyNegCtl[0], which starts at 1 here:
//   1  one extra Random::range(1) per frame, before the preamble: shifts the RNG seed 0x00822d9c, so the whole-game
//      scenario diverges from the first in-game checkpoint (the c3fy control).
//   2  the U-0008 defect restored in P3 (0x00413a60 compares the hole index, not the green height): a wrong body in the
//      middle of the frame loop, reached only while a hole is being built (course_season), never in sandbox_basic (c3fy2).
// No fixed-address page: c3fyNegCtl is an exported BSS word (word 1 = 'NEGQ' magic).
#define C3FY_NEG
#include "c3fy.cpp"

extern "C" __declspec(dllexport) volatile int c3fyNegCtl[4] = {1, 0x5147454e, 0, 0};
