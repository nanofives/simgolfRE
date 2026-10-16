// Negctl control for c3fy (mainLoop 0x0040f5c0). NEVER list this file in shim/re_batches.txt or deploy it to original\.
// It compiles c3fy.cpp with the wrong body armed, as the scenario A/B's control DLL:
//   -DC3FY_NEG_ALWAYS : the extra per-frame RNG draw is always on (the scenario control build, log/c3/alt/c3fy_scen.py).
//   -DC3FY_NEG only   : the draw is gated by the exported switch c3fyNegCtl[0] (set from a fixture / env before the run).
// The wrong body (one Random::range(1) per frame, in the middle of the frame loop) shifts the RNG seed 0x00822d9c, so
// the whole-game scenario diverges from the first frame. No fixed-address page: c3fyNegCtl is an exported BSS word.
#define C3FY_NEG_ALWAYS   // scenario control: the wrong body is unconditionally on in this build
#define C3FY_NEG
#include "c3fy.cpp"

extern "C" __declspec(dllexport) volatile int c3fyNegCtl[4] = {
#if defined(C3FY_NEG_ALWAYS)
    1,
#else
    0,
#endif
    0x5147454e, 0, 0};   // word 1 = 'NEGQ' magic (never a fixed-address page)
