// C3 batch c3br (2026-10-08) of sound.dll: the speech codec's parameter stages. Addresses in SG_HOOK and in the
// `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are VAs
// (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32 or COM call is made, nothing is allocated or freed, and no live sound object,
// device, list or buffer is touched: every buffer, cell, array and codec context the A/B passes in is allocated
// by the fixture.
// THREE MODULE GLOBALS ARE READ (never written by these bodies): g_codecOrder 0x100b58a0, the frame length
// 0x100b58a4 and the mode flag 0x100b58a8. A byte scan of sound.dll's .text for each of the three addresses
// finds exactly one writing instruction each, all inside codecSetParams 0x1003aa50: `mov [0x100b58a0], 0xa`
// at 0x1003aa68, `mov [0x100b58a4], 0xb4` at 0x1003aa72 and `mov [0x100b58a8], 1` at 0x1003aa7c. The fixtures
// store those same three values (10, 180, 1; quantizeParams also gets a 0 variant of the mode flag, the value
// the globals hold before codecSetParams has run). Both arms read them, neither writes them.
// Floating point: these are /Od /GZ debug bodies (no `fninit` and no `fldcw` in any of their prologues), so
// every intermediate is spilled to a named stack slot and the rounding points are visible. Each `fstp dword`
// spill is written as an assignment to a float and every operand inside one expression is widened with
// static_cast<double>, which is the c3ap method: the A/B thread runs with control word 0x027f (53-bit, round to
// nearest), so an x87 step that ends in a register is exactly a double operation and a step that ends in a dword
// slot is exactly a double operation rounded once to float. A `fst dword` that is NOT followed by a pop keeps
// the unrounded value in st(0) for the next use, so what follows it uses the double and only the stored slot is
// a float. `fild`/`fidiv` take their integer operand exactly.
// VC6 converts a double to an int through __ftol 0x10042550, whose body sets rounding control to 11 (truncate)
// at 0x1004255f and keeps the low dword of a 64-bit `fistp` (0x10042569, 0x1004256f), so it is written
// (int)(long long)v.
// The CRT's log 0x100432e4 and exp 0x100432b0 are called through their own addresses in the loaded module, so
// both arms use the same routine.
#include "hooks.h"

namespace {

// sound.dll has base relocations, so an original callee, table or constant is addressed through the loaded base.
char* c3br_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

float c3br_f(unsigned rva) { return *reinterpret_cast<const float*>(c3br_sound(rva)); }
int c3br_i(unsigned rva) { return *reinterpret_cast<const int*>(c3br_sound(rva)); }

// __ftol 0x10042550: truncating 64-bit fistp, low dword kept.
int c3br_ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }

// A raw dword copy: the originals move float slots with `mov`, which carries a NaN or a 0xcccccccc fill
// through unchanged.
void c3br_copy32(void* dst, const void* src) {
    *reinterpret_cast<unsigned*>(dst) = *reinterpret_cast<const unsigned*>(src);
}

// ---------------------------------------------------------------------------------------------------------
// speech codec: parameter quantisation
// ---------------------------------------------------------------------------------------------------------

// 0x0003c220  quantizeParams (VA 0x1003c220)
// __cdecl with seven stack arguments (`ret` at 0x1003c632 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0x78..ebp-1] is filled with 0xcccccccc (`rep stosd` at 0x1003c236); no local is read before it is
// written, so the fill never leaves the frame.
//   [ebp+8]    a pair of ints, decremented by one dword at 0x1003c24d so that its elements are 1 and 2
//   [ebp+0xc]  an index cell, used to pick an entry of the table at 0x10064594 (0x1003c2be)
//   [ebp+0x10] a float cell
//   [ebp+0x14] a float array, decremented by one float at 0x1003c244 (one-based)
//   [ebp+0x18] an int out cell
//   [ebp+0x1c] an int in/out cell
//   [ebp+0x20] the int out array, decremented by one dword at 0x1003c23b (one-based)
// The int cell [ebp+0x1c] first takes __ftol of the float cell (0x1003c256-0x1003c260) and the out array takes
// __ftol of each of the first g_codecOrder coefficients scaled by the float 32768.0f at 0x1005c524
// (0x1003c28b-0x1003c29f, `jg 0x1003c2a4` at 0x1003c283).
// The out cell [ebp+0x18] is then written three different ways (0x1003c2a7-0x1003c306): with both elements of
// the pair non-zero it takes the entry of the table at 0x10064594 the index cell names; otherwise, with the mode
// flag 0x100b58a8 set it takes 0 when the two elements are equal and 0x7f when they are not (`je 0x1003c2f2` at
// 0x1003c2e7), and with the mode flag clear it takes `pair[2] + pair[1]*2` (`lea` at 0x1003c300).
// The int cell is then turned into an exponent by a binary search over the 64-entry descending threshold table
// at 0x100647e4 (1024 down to 0 at 0x100648e4): the value is clamped to 0x3ff (`jg 0x1003c32b` at 0x1003c31f),
// the cursor starts at 0x20 with a step of 0x10 (0x1003c308, 0x1003c30f) and, while the step is positive
// (`jle 0x1003c381` at 0x1003c33e), the cursor moves down by the step when the value is above the entry
// (0x1003c34f) and up by the step when it is below it (0x1003c369), the step halving each round (0x1003c37a).
// One more decrement follows if the value is still above the entry (`jle 0x1003c39b` at 0x1003c390) and the cell
// becomes `0x1f - cursor/2` (0x1003c3a3-0x1003c3ad).
// Elements 1 and 2 of the out array are requantised through the 64-entry table at 0x100646e8: the sign is taken
// off (`jge 0x1003c3f3` at 0x1003c3e2), the magnitude is divided by 512 (0x1003c3f3-0x1003c402) and clamped to
// 0x3f (`jg 0x1003c413` at 0x1003c409), the table entry replaces it and the sign is put back
// (`je 0x1003c43b` at 0x1003c431).
// Elements 3..g_codecOrder are requantised through three 8-entry tables indexed by `g_codecOrder + 1 - i`
// (0x1003c484, 0x1003c49f, 0x1003c4f0, i.e. the tables at 0x10064688, 0x100646a8 and 0x100646c8 addressed one
// based from 0x10064684, 0x100646a4 and 0x100646c4): half the element plus the first table's entry is converted
// to a double by `fild` (0x1003c49c), multiplied by the second table's float (0x1003c4ab) and truncated by
// __ftol (0x1003c4b2), clamped to [-0x7f, 0x7f] (`jl 0x1003c4c8` at 0x1003c4be, `jg 0x1003c4e3` at 0x1003c4d9)
// and divided by `intPow(*0x10064554, third table entry)` (0x1003c51e-0x1003c533), the quotient being lowered by
// one when the value was negative (`jne 0x1003c545` at 0x1003c53a), which turns the C division into a floor.
// *0x10064554 is 2 and the third table's entries are 6, 5, 4, 4, 4, 4, 3 and 3, so the divisor is a power of two
// between 8 and 64 and intPow never reaches its 1/0 trap.
// Finally, with the mode flag set and the out cell equal to 0 or 0x7f (`je 0x1003c620` at 0x1003c55d,
// `je 0x1003c577` at 0x1003c569, `jne 0x1003c620` at 0x1003c571), six more elements of the out array are filled
// from the 16-entry permutation table at 0x10064558 indexed by `(x & 0x1e)/2` for x = elements 1, 2, 3, the int
// cell and element 4 (0x1003c577-0x1003c61d); element 9 takes half that entry and element 10 its lowest bit.
// Returns 0 (0x1003c620).
typedef int(__cdecl* IntPow_t)(const int*, const int*);
typedef int(__cdecl* QuantizeParams_t)(const int*, const int*, const float*, const float*, int*, int*, int*);
QuantizeParams_t QuantizeParams_orig;
int __cdecl QuantizeParams_re(const int* pair, const int* indexCell, const float* value, const float* coeffs,
                              int* outCell, int* expCell, int* out) {
    out -= 1;                                                    // 0x1003c23b
    coeffs -= 1;                                                 // 0x1003c244
    pair -= 1;                                                   // 0x1003c24d

    *expCell = c3br_ftol(static_cast<double>(*value));           // 0x1003c256-0x1003c260

    int order = c3br_i(0x000B58A0);                              // 0x1003c262
    const float scale = c3br_f(0x0005C524);                      // the float 32768.0f at 0x1005c524
    for (int i = 1; i <= order; ++i)                             // jg at 0x1003c283
        out[i] = c3br_ftol(static_cast<double>(coeffs[i])
                           * static_cast<double>(scale));        // 0x1003c28b-0x1003c29f

    if (pair[1] != 0 && pair[2] != 0) {                          // je at 0x1003c2ab, je at 0x1003c2b4
        *outCell = reinterpret_cast<const int*>(c3br_sound(0x00064594))[*indexCell];   // 0x1003c2be-0x1003c2c5
    } else if (c3br_i(0x000B58A8) != 0) {                        // je at 0x1003c2d0
        *outCell = 0;                                            // 0x1003c2d5
        if (pair[1] != pair[2]) *outCell = 0x7F;                 // je at 0x1003c2e7, 0x1003c2ec
    } else {
        *outCell = pair[2] + pair[1] * 2;                        // lea at 0x1003c300, 0x1003c306
    }

    const int* thresholds = reinterpret_cast<const int*>(c3br_sound(0x000647E4));
    int cursor = 0x20;                                           // 0x1003c308
    int step = 0x10;                                             // 0x1003c30f
    if (*expCell > 0x3FF) *expCell = 0x3FF;                      // jg at 0x1003c31f, 0x1003c338
    while (step > 0) {                                           // jle at 0x1003c33e
        if (*expCell > thresholds[cursor]) cursor -= step;       // jle at 0x1003c34f
        if (*expCell < thresholds[cursor]) cursor += step;       // jge at 0x1003c369
        step = step / 2;                                         // 0x1003c374-0x1003c37c
    }
    if (*expCell > thresholds[cursor]) cursor -= 1;              // jle at 0x1003c390
    *expCell = 0x1F - cursor / 2;                                // 0x1003c39b-0x1003c3ad

    const int* magnitudeTable = reinterpret_cast<const int*>(c3br_sound(0x000646E8));
    for (int i = 1; i <= 2; ++i) {                               // jg at 0x1003c3c5
        int v = out[i];                                          // 0x1003c3d1
        int negative = 0;                                        // 0x1003c3d7
        if (v < 0) { v = -v; negative = 1; }                     // jge at 0x1003c3e2
        v = v / 512;                                             // 0x1003c3f3-0x1003c402
        if (v > 0x3F) v = 0x3F;                                  // jg at 0x1003c409
        v = magnitudeTable[v];                                   // 0x1003c423
        if (negative != 0) v = -v;                               // je at 0x1003c431
        out[i] = v;                                              // 0x1003c444
    }

    const int* biasTable = reinterpret_cast<const int*>(c3br_sound(0x00064684));
    const float* gainTable = reinterpret_cast<const float*>(c3br_sound(0x000646A4));
    const int* shiftTable = reinterpret_cast<const int*>(c3br_sound(0x000646C4));
    const int* powBase = reinterpret_cast<const int*>(c3br_sound(0x00064554));
    IntPow_t intPow = reinterpret_cast<IntPow_t>(c3br_sound(0x0003E2C0));
    order = c3br_i(0x000B58A0);                                  // 0x1003c44c
    for (int i = 3; i <= order; ++i) {                           // jg at 0x1003c46d
        int v = out[i] / 2;                                      // 0x1003c479-0x1003c481
        const int biased = v + biasTable[c3br_i(0x000B58A0) + 1 - i];   // 0x1003c484-0x1003c499
        // fild dword [ebp-0x30] (0x1003c49c); fmul dword [edx*4+0x100646a4] (0x1003c4ab); __ftol (0x1003c4b2)
        v = c3br_ftol(static_cast<double>(biased)
                      * static_cast<double>(gainTable[c3br_i(0x000B58A0) + 1 - i]));
        if (v < -0x7F) v = -0x7F;                                // jl at 0x1003c4be, 0x1003c4c8
        if (v > 0x7F) v = 0x7F;                                  // jg at 0x1003c4d9, 0x1003c4e3
        int exponent = shiftTable[c3br_i(0x000B58A0) + 1 - i];   // 0x1003c4f0-0x1003c503
        int negative = 0;                                        // 0x1003c506
        if (v < 0) negative = -1;                                // jge at 0x1003c511, 0x1003c513
        v = v / intPow(powBase, &exponent);                      // 0x1003c51a-0x1003c533
        if (negative == -1) v -= 1;                              // jne at 0x1003c53a, 0x1003c53f
        out[i] = v;                                              // 0x1003c54e
    }

    if (c3br_i(0x000B58A8) != 0) {                               // je at 0x1003c55d
        if (*outCell == 0 || *outCell == 0x7F) {                 // je at 0x1003c569, jne at 0x1003c571
            const int* perm = reinterpret_cast<const int*>(c3br_sound(0x00064558));
            out[5] = perm[(out[1] & 0x1E) / 2];                  // 0x1003c577-0x1003c58f
            out[6] = perm[(out[2] & 0x1E) / 2];                  // 0x1003c592-0x1003c5aa
            out[7] = perm[(out[3] & 0x1E) / 2];                  // 0x1003c5ad-0x1003c5c5
            out[8] = perm[(*expCell & 0x1E) / 2];                // 0x1003c5c8-0x1003c5df
            out[9] = perm[(out[4] & 0x1E) / 2] / 2;              // 0x1003c5e2-0x1003c5ff
            out[10] = perm[(out[4] & 0x1E) / 2] & 1;             // 0x1003c602-0x1003c61d
        }
    }
    return 0;                                                    // 0x1003c620
}

// 0x0003da50  interpSubframeParams (VA 0x1003da50)
// __cdecl with thirteen stack arguments (`ret` at 0x1003e2b5 pops nothing), a /Od /GZ debug body.
//   [ebp+8]    the filter order cell (codecSynthStage passes &g_codecOrder, 0x1003b3cd)
//   [ebp+0xc]  a flag block, decremented by one dword at 0x1003daa8 (elements 1 and 2 are read)
//   [ebp+0x10] the sub-frame length cell (in/out)
//   [ebp+0x14] the gain cell (in/out)
//   [ebp+0x18] the reflection coefficients, decremented at 0x1003da74 (in/out)
//   [ebp+0x1c] the frame length cell (codecSynthStage passes &g_100b58a4, 0x1003b3b2)
//   [ebp+0x20] the per-entry mode array, decremented at 0x1003dab7
//   [ebp+0x24] the per-entry length array, decremented at 0x1003dac6
//   [ebp+0x28] the per-entry gain array, decremented at 0x1003dad5
//   [ebp+0x2c] the per-entry coefficient blocks, moved back by (order+1) floats at 0x1003da83-0x1003da9f
//   [ebp+0x30] the entry count cell (out)
//   [ebp+0x34] a float out cell
//   [ebp+0x38] the codec context
// Eight context addresses are bound on entry (0x1003dade-0x1003db27): the previous flag at +0x64c, the previous
// sub-frame length at +0x650, the previous gain at +0x654, the previous coefficients at +0x658 (one-based the
// other way: element i is read `[base + i*4 - 4]`, 0x1003de10), the carried-over sample count at +0x680 and the
// first-block flag at +0x684.
// Entry (0x1003db2a-0x1003db7a): the gain cell and the previous gain are both raised to the float 1.0f at
// 0x1005b8a8 when they are below it (`je 0x1003db45` at 0x1003db3a, `je 0x1003db60` at 0x1003db55), the float
// override length is cleared (0x1003db60) and the float out cell takes
// `*gain / (*previousGain + 8.0f)` (the float 8.0f at 0x1005ec24, `fdivr` at 0x1003db75).
// FIRST BLOCK (the context's +0x684 non-zero, `je 0x1003dc71` at 0x1003db82): the whole frame is cut into equal
// entries. With flag element 2 zero the sub-frame length becomes a quarter of the frame length
// (`jne 0x1003dbb1` at 0x1003db9c); the entry count is `frameLength / subFrameLength` (`idiv` at 0x1003dbba)
// and the context's +0x680 keeps the remainder (0x1003dbd6). Every entry gets a copy of the input coefficients
// (0x1003dc1c-0x1003dc34), the flag element 2 as its mode, the sub-frame length and the gain
// (0x1003dc39-0x1003dc5e); the first-block flag is then cleared (0x1003dc66) and the body jumps to its tail.
// MAIN PATH (0x1003dc71): the number of samples to cover is `frameLength + *(ctx+0x680)` (0x1003dc82) and the
// per-sample slope of the length ramp is `(subFrameLength - previousLength) / thatCount` (`fild`/`fidiv` at
// 0x1003dc95). Three shapes follow.
//   (a) flag element 1 equal to the previous flag AND flag element 2 equal to flag element 1
//       (`jne 0x1003dd45` at 0x1003dcc9 and 0x1003dcd7): with flag element 2 zero (`jne 0x1003dd1b` at
//       0x1003dce0) the sub-frame length is reset to a quarter of the frame, the previous length follows it and
//       the previous gain takes the current gain when the float out cell is above 8.0f
//       (`jne 0x1003dd1b` at 0x1003dd0f); the slope is recomputed (0x1003dd2b) and the mode is flag element 2.
//   (b) the previous flag not 1 (`je 0x1003de83` at 0x1003dd4e): the frame is split in two fixed entries of
//       `(count - frameLength/4)` or `(count - frameLength*3/4)` samples depending on whether the previous flag
//       equals flag element 1 (`jne 0x1003dd7c` at 0x1003dd62), halved at 0x1003dd9b; both get mode 0 and the
//       previous gain, the previous coefficients are copied into both blocks and then replaced by the input
//       ones (0x1003ddfe-0x1003de45); the slope becomes 0, the count becomes 2 and the ramp restarts after
//       those samples (0x1003de4b-0x1003de77).
//   (c) the previous flag equal to 1 (0x1003de83): the sample count becomes `frameLength*3/4` or
//       `frameLength/4` plus the carry (0x1003de93-0x1003dec1), the input coefficients are saved in the frame
//       and replaced by the previous ones (0x1003dee6-0x1003df09), and the second-pass flag is set
//       (0x1003df1c), so the ramp loop runs twice.
// THE RAMP LOOP (0x1003df23-0x1003e166) walks sample positions from the start index to the sample count. At
// each position the predicted length is `previousLength + position*slope` rounded by adding the float 0.5f at
// 0x1005ec34 and truncating with __ftol (0x1003df63-0x1003df71); a non-zero override length replaces it
// (0x1003df74-0x1003df8c). A length longer than the samples left since the last entry is skipped
// (`jg 0x1003e166` at 0x1003df98). Otherwise the entry count grows by one, the length, the mode and, through
// the interpolation below, the coefficients and the gain of that entry are written (0x1003df9e-0x1003e163),
// and the interpolation fraction is `(accumulated - length/2) / sampleCount` (0x1003dfd8-0x1003dff4).
// Each coefficient is interpolated in the log-area domain: `log((1+k)/(1-k))` of the previous and of the input
// coefficient (the CRT's log 0x100432e4 at 0x1003e04b and 0x1003e07c), mixed by the fraction (0x1003e087), run
// through the CRT's exp 0x100432b0 (0x1003e09c) and mapped back with `(e - 1) / (e + 1)` (0x1003e0aa-0x1003e0bc)
// -- the numerator uses the unrounded double that `fst dword [ebp-0x90]` at 0x1003e0a4 left in st(0) and the
// denominator the float that same instruction stored. The gain is interpolated the same way between the
// logarithms of the previous and the current gain (0x1003e0d7-0x1003e163), in doubles throughout.
// After the first pass, with the second-pass flag set (`je 0x1003e176` at 0x1003e16f), the ramp restarts flat
// (slope 0, mode 0) over the rest of the frame with a fixed override length of half the remaining samples,
// halved again when it is above the float 90.0f at 0x1005ec30 (`jne 0x1003e1da` at 0x1003e1cc), the previous
// gain takes the current one and the saved coefficients are put back (0x1003e1da-0x1003e226).
// The leftover samples are written to the context's +0x680 (0x1003e231) and, when at least one entry was
// produced (`je 0x1003e2a0` at 0x1003e242), the context's +0x64c, +0x650, +0x654 and +0x658 take flag element 2,
// the sub-frame length, the gain and the coefficients (0x1003e244-0x1003e29a). Returns 0 (0x1003e2a0).
typedef double(__cdecl* Unary_t)(double);
double c3br_log(double v) {
    static Unary_t fn = 0;
    if (!fn) fn = reinterpret_cast<Unary_t>(c3br_sound(0x000432E4));
    return fn(v);
}
double c3br_exp(double v) {
    static Unary_t fn = 0;
    if (!fn) fn = reinterpret_cast<Unary_t>(c3br_sound(0x000432B0));
    return fn(v);
}

typedef int(__cdecl* InterpSubframeParams_t)(const int*, const int*, int*, float*, float*, const int*, int*,
                                             int*, float*, float*, int*, float*, void*);
InterpSubframeParams_t InterpSubframeParams_orig;
int __cdecl InterpSubframeParams_re(const int* order, const int* flags, int* subLen, float* gain, float* k,
                                    const int* frameLen, int* modeOut, int* lenOut, float* gainOut,
                                    float* kOut, int* countOut, float* ratioOut, void* ctx) {
    // The frame array at [ebp + n*4 - 0x50]: slot 0 is the interpolation fraction (`fstp dword [ebp-0x50]` at
    // 0x1003dff4) and slots 1..*order hold the saved coefficients (0x1003def2). At the order 10 that
    // codecSetParams 0x1003aa50 stores, slot 11 would be [ebp-0x24], the log slot, so the two never overlap.
    float frame[16];
    for (int t = 0; t < 16; ++t)
        *reinterpret_cast<unsigned*>(&frame[t]) = 0xCCCCCCCCu;   // rep stosd at 0x1003da6c

    // [ebp-0xc], the block stride: written only inside the `[ebp+0x2c] != 0` test at 0x1003da81, so it keeps
    // the frame fill when the coefficient blocks are null (every use of it is then inside a loop that an order
    // of 0 leaves empty).
    int stride = static_cast<int>(0xCCCCCCCCu);
    if (k) k -= 1;                                               // je at 0x1003da72
    if (kOut) {                                                  // je at 0x1003da81
        stride = *order;                                         // 0x1003da86-0x1003da88
        kOut -= stride + 1;                                      // 0x1003da8b-0x1003da9f
    }
    if (flags) flags -= 1;                                       // je at 0x1003daa6
    if (modeOut) modeOut -= 1;                                   // je at 0x1003dab5
    if (lenOut) lenOut -= 1;                                     // je at 0x1003dac4
    if (gainOut) gainOut -= 1;                                   // je at 0x1003dad3

    int* cPrevFlag = reinterpret_cast<int*>(static_cast<char*>(ctx) + 0x64C);     // 0x1003dade
    int* cPrevLen = reinterpret_cast<int*>(static_cast<char*>(ctx) + 0x650);     // 0x1003daed
    float* cPrevGain = reinterpret_cast<float*>(static_cast<char*>(ctx) + 0x654);  // 0x1003daf9
    float* cPrevK = reinterpret_cast<float*>(static_cast<char*>(ctx) + 0x658);   // 0x1003db04
    int* cCarry = reinterpret_cast<int*>(static_cast<char*>(ctx) + 0x680);       // 0x1003db13
    int* cFirst = reinterpret_cast<int*>(static_cast<char*>(ctx) + 0x684);       // 0x1003db1f

    const float one = c3br_f(0x0005B8A8);                        // 1.0f
    const float eight = c3br_f(0x0005EC24);                      // 8.0f
    const float half = c3br_f(0x0005EC34);                       // 0.5f
    const float ninety = c3br_f(0x0005EC30);                     // 90.0f
    const float two = c3br_f(0x0005EC2C);                        // 2.0f

    // `fcomp` + `test ah, 1` + `je` takes the store when C0 is set, which is "below or unordered".
    if (!(static_cast<double>(*gain) >= static_cast<double>(one)))
        *reinterpret_cast<unsigned*>(gain) = 0x3F800000u;         // 0x1003db2d-0x1003db3f
    if (!(static_cast<double>(*cPrevGain) >= static_cast<double>(one)))
        *reinterpret_cast<unsigned*>(cPrevGain) = 0x3F800000u;    // 0x1003db48-0x1003db5a

    float overrideLen;
    *reinterpret_cast<unsigned*>(&overrideLen) = 0;              // 0x1003db60
    *ratioOut = static_cast<float>(static_cast<double>(*gain)
                                   / (static_cast<double>(*cPrevGain)
                                      + static_cast<double>(eight)));   // 0x1003db67-0x1003db7a

    int samples = 0;
    int accumulated = 0;

    if (*cFirst != 0) {                                          // je at 0x1003db82
        samples = 0;                                             // 0x1003db88
        const int mode = flags[2];                               // 0x1003db92
        if (mode == 0)                                           // jne at 0x1003db9c
            *subLen = *frameLen / 4;                             // 0x1003db9e-0x1003dbaf
        *countOut = *frameLen / *subLen;                         // idiv at 0x1003dbba
        *cCarry = *frameLen - (*countOut * *subLen);             // 0x1003dbc1-0x1003dbd6
        const int entries = *countOut;                           // 0x1003dbdb
        for (int i = 1; i <= entries; ++i) {                     // jg at 0x1003dbf8
            const int m = *order;                                // 0x1003dbfd
            for (int j = 1; j <= m; ++j)                         // jg at 0x1003dc1a
                c3br_copy32(&kOut[i * stride + j], &k[j]);       // 0x1003dc1c-0x1003dc34
            modeOut[i] = mode;                                   // 0x1003dc39-0x1003dc42
            lenOut[i] = *subLen;                                 // 0x1003dc45-0x1003dc50
            c3br_copy32(&gainOut[i], gain);                      // 0x1003dc53-0x1003dc5e
        }
        *cFirst = 0;                                             // 0x1003dc66
    } else {
        bool secondPass = false;                                 // [ebp-0x5c] at 0x1003dc71
        samples = *frameLen + *cCarry;                           // 0x1003dc78-0x1003dc82
        float slope = static_cast<float>(static_cast<double>(*subLen - *cPrevLen)
                                         / static_cast<double>(samples));    // 0x1003dc95-0x1003dc9e
        *countOut = 0;                                           // 0x1003dca4
        accumulated = 0;                                         // 0x1003dcaa
        int startIndex = 1;                                      // 0x1003dcb1
        int mode;

        if (flags[1] == *cPrevFlag && flags[2] == flags[1]) {    // jne at 0x1003dcc9, 0x1003dcd7
            if (flags[2] == 0) {                                 // jne at 0x1003dce0
                *subLen = *frameLen / 4;                         // 0x1003dce2-0x1003dcf3
                *cPrevLen = *subLen;                             // 0x1003dcf5-0x1003dcfd
                if (static_cast<double>(*ratioOut) > static_cast<double>(eight))   // jne at 0x1003dd0f
                    c3br_copy32(cPrevGain, gain);                // 0x1003dd11-0x1003dd19
            }
            slope = static_cast<float>(static_cast<double>(*subLen - *cPrevLen)
                                       / static_cast<double>(samples));      // 0x1003dd1b-0x1003dd34
            mode = flags[2];                                     // 0x1003dd37-0x1003dd3d
        } else if (*cPrevFlag != 1) {                            // je at 0x1003dd4e
            int split;
            if (*cPrevFlag == flags[1])                          // jne at 0x1003dd62
                split = samples - (*frameLen / 4);               // 0x1003dd64-0x1003dd77
            else
                split = samples - (*frameLen * 3 / 4);           // 0x1003dd7c-0x1003dd92
            lenOut[1] = split / 2;                               // 0x1003dd95-0x1003dda0
            lenOut[2] = split - lenOut[1];                       // 0x1003dda3-0x1003ddaf
            modeOut[1] = 0;                                      // 0x1003ddb2
            modeOut[2] = 0;                                      // 0x1003ddbc
            c3br_copy32(&gainOut[1], cPrevGain);                 // 0x1003ddc6-0x1003ddce
            c3br_copy32(&gainOut[2], cPrevGain);                 // 0x1003ddd1-0x1003ddd9
            const int m = *order;                                // 0x1003dddf
            for (int i = 1; i <= m; ++i) {                       // jg at 0x1003ddfc
                c3br_copy32(&kOut[i + stride], &cPrevK[i - 1]);      // 0x1003ddfe-0x1003de14
                c3br_copy32(&kOut[i + stride * 2], &cPrevK[i - 1]);  // 0x1003de17-0x1003de30
                c3br_copy32(&cPrevK[i - 1], &k[i]);                  // 0x1003de33-0x1003de45
            }
            *reinterpret_cast<unsigned*>(&slope) = 0;            // 0x1003de4b
            *countOut = 2;                                       // 0x1003de52
            *cPrevLen = *subLen;                                 // 0x1003de5b-0x1003de63
            accumulated = split;                                 // 0x1003de65
            startIndex = split + 1;                              // 0x1003de6b-0x1003de71
            mode = 1;                                            // 0x1003de77
        } else {
            if (*cPrevFlag == flags[1])                          // je at 0x1003de91
                samples = (*frameLen * 3 / 4) + *cCarry;         // 0x1003deab-0x1003dec1
            else
                samples = (*frameLen / 4) + *cCarry;             // 0x1003de93-0x1003dea6
            const int m = *order;                                // 0x1003dec7
            for (int i = 1; i <= m; ++i) {                       // jg at 0x1003dee4
                c3br_copy32(&frame[i], &k[i]);                   // 0x1003dee6-0x1003def2
                c3br_copy32(&k[i], &cPrevK[i - 1]);              // 0x1003def6-0x1003df09
            }
            mode = 1;                                            // 0x1003df0e
            *reinterpret_cast<unsigned*>(&slope) = 0;            // 0x1003df15
            secondPass = true;                                   // 0x1003df1c
        }

        for (;;) {                                               // `mov edx, 1` / `je` at 0x1003df2a
            const int limit = samples;                           // 0x1003df30
            for (int i = startIndex; i <= limit; ++i) {          // jg at 0x1003df50
                // fild prevLen; fild i; fmul slope; faddp; fst (dead); fadd 0.5f; __ftol
                int len = c3br_ftol(static_cast<double>(*cPrevLen)
                                    + static_cast<double>(i) * static_cast<double>(slope)
                                    + static_cast<double>(half));             // 0x1003df56-0x1003df71
                // `test ah, 0x40` after `fcomp` with the float 0.0f at 0x1005ec00: the override is used only
                // when it is strictly below or strictly above zero.
                if (overrideLen < 0.0f || overrideLen > 0.0f)                 // jne at 0x1003df82
                    len = c3br_ftol(static_cast<double>(overrideLen));        // 0x1003df84-0x1003df8c
                if (len > i - accumulated) continue;                          // jg at 0x1003df98
                *countOut += 1;                                               // 0x1003df9e-0x1003dfa9
                lenOut[*countOut] = len;                                      // 0x1003dfab-0x1003dfb6
                *subLen = len;                                                // 0x1003dfb9-0x1003dfbf
                modeOut[*countOut] = mode;                                    // 0x1003dfc1-0x1003dfcc
                accumulated += len;                                           // 0x1003dfcf-0x1003dfd5
                frame[0] = static_cast<float>(static_cast<double>(accumulated - len / 2)
                                              / static_cast<double>(samples));  // 0x1003dfd8-0x1003dff4
                const int m = *order;                                         // 0x1003dffa
                for (int j = 1; j <= m; ++j) {                                // jg at 0x1003e017
                    const float lo = static_cast<float>(c3br_log(
                        (static_cast<double>(cPrevK[j - 1]) + static_cast<double>(one))
                        / (static_cast<double>(one) - static_cast<double>(cPrevK[j - 1]))));
                                                                              // 0x1003e01d-0x1003e053
                    const double hi = c3br_log(
                        (static_cast<double>(k[j]) + static_cast<double>(one))
                        / (static_cast<double>(one) - static_cast<double>(k[j])));
                                                                              // 0x1003e056-0x1003e081
                    const double mix = (hi - static_cast<double>(lo))
                                       * static_cast<double>(frame[0])
                                       + static_cast<double>(lo);             // 0x1003e084-0x1003e08d
                    const double e = c3br_exp(mix);                           // 0x1003e090-0x1003e0a1
                    const float eStored = static_cast<float>(e);              // fst dword at 0x1003e0a4
                    kOut[*countOut * stride + j] = static_cast<float>(
                        (e - static_cast<double>(one))
                        / (static_cast<double>(eStored) + static_cast<double>(one)));
                                                                              // 0x1003e0aa-0x1003e0cf
                }
                const double gPrev = c3br_log(static_cast<double>(*cPrevGain));   // 0x1003e0d7-0x1003e0e7
                const double fraction = static_cast<double>(frame[0]);            // 0x1003e0f0-0x1003e0f3
                const double gNow = c3br_log(static_cast<double>(*gain));         // 0x1003e0f9-0x1003e109
                const double gPrev2 = c3br_log(static_cast<double>(*cPrevGain));  // 0x1003e112-0x1003e122
                gainOut[*countOut] = static_cast<float>((gNow - gPrev2) * fraction + gPrev);
                                                                                  // 0x1003e125-0x1003e13f
                gainOut[*countOut] = static_cast<float>(
                    c3br_exp(static_cast<double>(gainOut[*countOut])));            // 0x1003e142-0x1003e163
            }
            if (!secondPass) break;                              // jne at 0x1003e16f, jmp at 0x1003e171
            secondPass = false;                                  // 0x1003e176
            startIndex = accumulated + 1;                        // 0x1003e17d-0x1003e183
            samples = *frameLen + *cCarry;                       // 0x1003e189-0x1003e193
            *reinterpret_cast<unsigned*>(&slope) = 0;            // 0x1003e196
            mode = 0;                                            // 0x1003e19d
            const int rest = (samples - startIndex) / 2;         // 0x1003e1a4-0x1003e1b2
            overrideLen = static_cast<float>(rest);              // fst dword at 0x1003e1be
            if (static_cast<double>(rest) > static_cast<double>(ninety))   // jne at 0x1003e1cc
                overrideLen = static_cast<float>(static_cast<double>(overrideLen)
                                                 / static_cast<double>(two));   // 0x1003e1ce-0x1003e1d7
            c3br_copy32(cPrevGain, gain);                        // 0x1003e1da-0x1003e1e2
            const int m = *order;                                // 0x1003e1e7
            for (int i = 1; i <= m; ++i) {                       // jg at 0x1003e204
                c3br_copy32(&k[i], &frame[i]);                   // 0x1003e206-0x1003e213
                c3br_copy32(&cPrevK[i - 1], &frame[i]);          // 0x1003e216-0x1003e226
            }
        }
        *cCarry = samples - accumulated;                         // 0x1003e231-0x1003e23a
    }

    if (*countOut != 0) {                                        // je at 0x1003e242
        *cPrevFlag = flags[2];                                   // 0x1003e244-0x1003e250
        *cPrevLen = *subLen;                                     // 0x1003e252-0x1003e25a
        c3br_copy32(cPrevGain, gain);                            // 0x1003e25c-0x1003e264
        const int m = *order;                                    // 0x1003e269
        for (int i = 1; i <= m; ++i)                             // jg at 0x1003e286
            c3br_copy32(&cPrevK[i - 1], &k[i]);                  // 0x1003e288-0x1003e29a
    }
    return 0;                                                    // 0x1003e2a0
}

// 0x0003b230  codecSynthStage (VA 0x1003b230)
// __cdecl with seven stack arguments (`ret` at 0x1003b59e pops nothing), a /Od /GZ debug body.
//   [ebp+8]    a flag block, decremented by one dword when it is not null (`je 0x1003b25d` at 0x1003b252)
//   [ebp+0xc]  the sub-frame length cell (in/out)
//   [ebp+0x10] the gain cell
//   [ebp+0x14] the reflection coefficients, decremented at 0x1003b263 (in/out)
//   [ebp+0x18] the float output array, decremented at 0x1003b272
//   [ebp+0x1c] an int out cell
//   [ebp+0x20] the codec context
// Two context addresses are bound on entry: the synthesis ring at +0xa8 (0x1003b27e) and its write cursor at
// +0x648 (0x1003b289). The ring ends where the cursor begins, so it holds (0x648 - 0xa8)/4 = 360 floats.
// The sub-frame length cell is clamped to [0x14, 0x9c] (`jg 0x1003b2aa` at 0x1003b29b, `jl 0x1003b2ce` at
// 0x1003b2c1, stored at 0x1003b2e1) and each of the first g_codecOrder coefficients is clamped to
// [-0.99f, +0.99f] (the floats at 0x1005ebd4 and 0x1005ebd0, the replacements being the immediates 0x3f7d70a4
// at 0x1003b33b and 0xbf7d70a4 at 0x1003b369; `jg 0x1003b38a` at 0x1003b30f ends the loop).
// interpSubframeParams 0x1003da50 is then called (0x1003b3d2) with &g_codecOrder, the flag block, the
// sub-frame length cell, the gain cell, the coefficients, &g_100b58a4 and seven frame slots: the mode array at
// [ebp-0x9c], the length array at [ebp-0xe8], the level array at [ebp-0x58], the coefficient blocks at
// [ebp-0x3bc], the entry count at [ebp-0x58] itself, a float cell at [ebp-0xa4] and the context. The arrays are
// all one-based, so the entry count shares the level array's slot 0, and the length array's slot 0 is the gain
// cell reflectionToLpc writes and lpcSynthesize reads back as its feedback gain.
// With at least one entry (`jle 0x1003b589` at 0x1003b3de) each entry s is synthesised in turn
// (`jg 0x1003b4db` at 0x1003b40e): reflectionToLpc 0x1003d8d0 turns the entry's ten coefficients at
// [ebp + s*40 - 0x3b8] into the direct-form filter at [ebp-0x110] with the float 0.7f at 0x10063efc as its
// amplitude (0x1003b41a-0x1003b43d), lpcSynthesize 0x1003d1d0 writes the entry's samples into the ring at the
// cursor (0x1003b445-0x1003b491), applyIir2 0x1003d0a0 filters them in place (0x1003b499-0x1003b4b7) and the
// cursor advances by the entry's length (0x1003b4bf-0x1003b4d4).
// The first 0xb4 samples of the ring are then divided by the float 4096.0f at 0x1005ebcc into the caller's
// output array (0x1003b50b-0x1003b51e, `jg 0x1003b523` at 0x1003b500), the out cell takes 0xb4 (0x1003b526),
// the cursor drops by 0xb4 (0x1003b52c-0x1003b53a) and whatever is left in the ring is moved to its front
// (`jg 0x1003b589` at 0x1003b568; the source `[esi + i*4 + 0x2cc]` at 0x1003b57c is element i + 0xb3).
// Returns 0 (0x1003b589).
typedef int(__cdecl* ReflectionToLpc_t)(const float*, float*, const int*, const float*, float*);
typedef int(__cdecl* LpcSynthesize_t)(const float*, const int*, const int*, float*, const float*,
                                      const float*, const float*, void*);
typedef int(__cdecl* ApplyIir2_t)(float*, const int*, void*);
typedef int(__cdecl* CodecSynthStage_t)(const int*, int*, float*, float*, float*, int*, void*);
CodecSynthStage_t CodecSynthStage_orig;
int __cdecl CodecSynthStage_re(const int* flags, int* subLenCell, float* gainCell, float* coeffs, float* out,
                               int* countCell, void* ctx) {
    // Frame slots the callees are handed, in the original's layout:
    //   lenSlots[i]   = [ebp - 0xe8 + i*4]   slot 0 the gain cell, slots 1.. the entry lengths
    //   levelSlots[i] = [ebp - 0x58 + i*4]   slot 0 the entry count, slots 1.. the entry levels
    //   modeSlots[i]  = [ebp - 0x9c + i*4]   slot 0 the loop counter, slots 1.. the entry modes
    //   blocks[i]     = [ebp - 0x3bc + i*4]  entry s occupies 10*s+1 .. 10*s+10
    // Each is kept as raw dwords because the same slot is read as an int by one callee and as a float by
    // another (the length array's slot 0 is the gain, 0x1003b449 against 0x1003b48a).
    unsigned lenSlots[64];
    unsigned levelSlots[64];
    unsigned modeSlots[64];
    float lpcCoeffs[16];                                         // [ebp-0x110]
    float blocks[256];                                           // [ebp-0x3bc]
    float level2;                                                // [ebp-0xa4]
    for (int t = 0; t < 64; ++t) {
        lenSlots[t] = 0xCCCCCCCCu;
        levelSlots[t] = 0xCCCCCCCCu;
        modeSlots[t] = 0xCCCCCCCCu;
    }
    for (int t = 0; t < 16; ++t) *reinterpret_cast<unsigned*>(&lpcCoeffs[t]) = 0xCCCCCCCCu;
    for (int t = 0; t < 256; ++t) *reinterpret_cast<unsigned*>(&blocks[t]) = 0xCCCCCCCCu;
    *reinterpret_cast<unsigned*>(&level2) = 0xCCCCCCCCu;         // rep stosd at 0x1003b24c

    if (flags) flags -= 1;                                       // je at 0x1003b252
    if (coeffs) coeffs -= 1;                                     // je at 0x1003b261
    if (out) out -= 1;                                           // je at 0x1003b270
    float* ring = reinterpret_cast<float*>(static_cast<char*>(ctx) + 0xA8);   // 0x1003b27e
    int* cursor = reinterpret_cast<int*>(static_cast<char*>(ctx) + 0x648);    // 0x1003b289

    int clamped = (*subLenCell > 0x9C) ? 0x9C : *subLenCell;     // jg at 0x1003b29b
    if (clamped < 0x14) clamped = 0x14;                          // jl at 0x1003b2c1
    *subLenCell = clamped;                                       // 0x1003b2e1

    const float hi = c3br_f(0x0005EBD4);                         // 0.99f
    const float lo = c3br_f(0x0005EBD0);                         // -0.99f
    int order = c3br_i(0x000B58A0);                              // 0x1003b2e3
    for (int i = 1; i <= order; ++i) {                           // jg at 0x1003b30f
        unsigned raw = *reinterpret_cast<const unsigned*>(&coeffs[i]);        // 0x1003b31a
        float v;
        *reinterpret_cast<unsigned*>(&v) = raw;
        // `fcomp` + `test ah, 0x41` + `je`: the replacement is taken only when the value is strictly above.
        if (static_cast<double>(v) > static_cast<double>(hi)) raw = 0x3F7D70A4u;   // je at 0x1003b32e
        *reinterpret_cast<unsigned*>(&v) = raw;
        // `test ah, 1` + `jne`: taken when C0 is set, which is "below or unordered".
        if (!(static_cast<double>(v) >= static_cast<double>(lo))) raw = 0xBF7D70A4u;   // jne at 0x1003b35c
        *reinterpret_cast<unsigned*>(&coeffs[i]) = raw;          // 0x1003b382
    }

    InterpSubframeParams_t interp =
        reinterpret_cast<InterpSubframeParams_t>(c3br_sound(0x0003DA50));
    interp(reinterpret_cast<const int*>(c3br_sound(0x000B58A0)),
           reinterpret_cast<const int*>(flags + 1),
           subLenCell, gainCell, coeffs + 1,
           reinterpret_cast<const int*>(c3br_sound(0x000B58A4)),
           reinterpret_cast<int*>(&modeSlots[1]),
           reinterpret_cast<int*>(&lenSlots[1]),
           reinterpret_cast<float*>(&levelSlots[1]),
           &blocks[11],
           reinterpret_cast<int*>(&levelSlots[0]),
           &level2, ctx);                                        // 0x1003b38a-0x1003b3d7

    const int entries = static_cast<int>(levelSlots[0]);         // 0x1003b3da
    if (entries > 0) {                                           // jle at 0x1003b3de
        ReflectionToLpc_t reflect = reinterpret_cast<ReflectionToLpc_t>(c3br_sound(0x0003D8D0));
        LpcSynthesize_t synth = reinterpret_cast<LpcSynthesize_t>(c3br_sound(0x0003D1D0));
        ApplyIir2_t iir = reinterpret_cast<ApplyIir2_t>(c3br_sound(0x0003D0A0));
        for (int s = 1; s <= entries; ++s) {                     // jg at 0x1003b40e
            reflect(&blocks[10 * s + 1], &lpcCoeffs[0],
                    reinterpret_cast<const int*>(c3br_sound(0x000B58A0)),
                    reinterpret_cast<const float*>(c3br_sound(0x00063EFC)),
                    reinterpret_cast<float*>(&lenSlots[0]));     // 0x1003b414-0x1003b43d
            synth(&lpcCoeffs[0],
                  reinterpret_cast<const int*>(&lenSlots[s]),
                  reinterpret_cast<const int*>(&modeSlots[s]),
                  ring + *cursor,
                  reinterpret_cast<const float*>(&levelSlots[s]),
                  &level2,
                  reinterpret_cast<const float*>(&lenSlots[0]),
                  ctx);                                          // 0x1003b445-0x1003b491
            iir(ring + *cursor, reinterpret_cast<const int*>(&lenSlots[s]), ctx);   // 0x1003b499-0x1003b4b7
            *cursor = *cursor + static_cast<int>(lenSlots[s]);   // 0x1003b4bf-0x1003b4d4
        }
        const float divisor = c3br_f(0x0005EBCC);                // 4096.0f
        for (int i = 1; i <= 0xB4; ++i)                          // jg at 0x1003b500
            out[i] = static_cast<float>(static_cast<double>(ring[i - 1])
                                        / static_cast<double>(divisor));   // 0x1003b50b-0x1003b51e
        *countCell = 0xB4;                                       // 0x1003b526
        *cursor = *cursor - 0xB4;                                // 0x1003b52c-0x1003b53a
        const int left = *cursor;                                // 0x1003b53f
        for (int i = 1; i <= left; ++i)                          // jg at 0x1003b568
            c3br_copy32(&ring[i - 1], &ring[i + 0xB3]);          // 0x1003b56a-0x1003b583
    }
    return 0;                                                    // 0x1003b589
}

// 0x0003b5a0  codecTransformStage (VA 0x1003b5a0)
// __cdecl with eight stack arguments (`ret` at 0x1003bf22 pops nothing), a /Od /GZ debug body.
//   [ebp+8]    a class cell, used to index the 16-entry table at 0x10064014 (0x1003b644)
//   [ebp+0xc]  a level cell (in/out)
//   [ebp+0x10] the integer coefficient array, decremented at 0x1003b5c4 (out)
//   [ebp+0x14] a two-element flag block, decremented at 0x1003b5d3 (out)
//   [ebp+0x18] a gain cell (out)
//   [ebp+0x1c] a float cell (out)
//   [ebp+0x20] the float coefficient array, decremented at 0x1003b5e2 (out)
//   [ebp+0x24] the stage's own record
// The record is bound field by field at 0x1003b5eb-0x1003b63c: six cells at +0, +4, +8, +0xc, +0x10 and +0x14,
// a three-wide coefficient history at +0x18 (entry i occupies the dwords [+0x18 + i*12 - 12], [-8] and [-4], so
// ten entries fill +0x18..+0x8c), a three-entry gain history at +0x90 and a three-entry level history at +0x9c.
// MODE FLAG CLEAR (`jne 0x1003b6ee` at 0x1003b655): the body only derives the two flags from the class cell
// (both start at 1, the first clears for a class of 1 or below at 0x1003b675 and the second for a class of 0 or
// 2 at 0x1003b687 and 0x1003b68f), writes the table entry or, when it is 4 or below, the record's +0
// (`jg 0x1003b6b5` at 0x1003b6a9) into the gain cell, copies the gain cell back into +0 when both flags are 1
// (0x1003b6bc, 0x1003b6c5) and restores it from +0 when the two flags differ (`je 0x1003b6e9` at 0x1003b6dd),
// then jumps straight to the output conversion at 0x1003bd77.
// MODE FLAG SET: a table entry above 4 (`jle 0x1003b722` at 0x1003b6f2) is written to the gain history's slot 0
// and the record's +0x10 becomes `(*+0x10 * 15 + entry + 8) / 16` (0x1003b703-0x1003b71e) with a working class
// of 2; otherwise the working class is the entry itself and the gain history's slot 0 takes +0x10. The level
// history's slot 0 takes the level cell (0x1003b73a) and the coefficient history's oldest column takes the
// caller's coefficients (0x1003b75f-0x1003b771).
// The transform code is `table[class + (*+8 << 4) + *+0xc * 4 + 1]` from the 33-entry table at 0x10063f10
// (0x1003b777-0x1003b79a); its low two bits select the gain-history rotation (0x1003b79d) and the rest, divided
// by 8 and again by 64 when the record's +0x14 is below 2048 (the int at 0x10063f04, `jge 0x1003b7d1` at
// 0x1003b7c0), is the flag word. The band index is 4, 3, 2 or 1 as +0x14 falls below 2048, 1024 or 128 (the
// ints at 0x10063f10, 0x10063f0c and 0x10063f08, 0x1003b7d8-0x1003b814). The two output flags are bits 1 and 0
// of the flag word (0x1003b814-0x1003b82e).
// A non-zero record +4 (`je 0x1003b861` at 0x1003b837) clears it, writes the same gain the mode-flag-clear path
// writes and skips to the flag-word tail at 0x1003bc99.
// Bit 4 of the flag word (the int 0x10 at 0x10064450, `je 0x1003b9e7` at 0x1003b86c) runs the decoder:
// decodeTableSymbol 0x1003e520 is called once for the level history (key `history[22] << 4 | level/2`,
// 0x1003b885-0x1003b8aa) and once for each of four coefficient entries (0x1003b8e5-0x1003b9c1), each key being
// a four-bit context from another entry and the entry's own five bits; a non-negative result becomes
// `parity + 2*result` folded into [-16, 15] by the `& 0x10` test at 0x1003b98a, a negative one leaves the
// entry's previous column. The record's +0x14 then becomes
// `__ftol(*+0x14 * 0.96875f + symbols*0x66)` (the float at 0x1005ebdc, `fiadd` at 0x1003b9da).
// The level cell takes the level history's slot 1 and the coefficients the history's middle column
// (0x1003b9e7-0x1003ba28); the gain history rotates by the code's low two bits (0x1003ba2d, 0x1003ba3f) and the
// gain cell takes its slot 1 (0x1003ba50).
// Bits 2 and 3 of the flag word (the ints at 0x10064448 and 0x1006444c) run medianOfThree 0x1003e480 over the
// level history, over each of six coefficient entries and over the gain history, but only where BOTH distances
// from the middle slot reach the band's threshold (the float tables at 0x10063fa0, 0x10063f80 indexed
// `band + i*4 + 8`, and 0x10063f90; the `test ah, 1` after each `fcomp` skips when the distance is below).
// Bit 5 (the int at 0x10064454) overwrites the coefficients from entry 5 upwards with the table at 0x10064418
// (0x1003bca6-0x1003bcdb). The record then shifts: +0xc takes the working class, +8 the second output flag, and
// the gain, level and coefficient histories move one column down (0x1003bcdd-0x1003bd75).
// The output conversion (0x1003bd77) maps the level cell through the 64-entry table at 0x10064214 indexed
// `(0x1f - level) * 2`; coefficients 1 and 2 go through the 32-entry table at 0x10064314 indexed by twice their
// magnitude (a magnitude above 0xf is replaced by 0 only on the negative side, 0x1003bdd3) with the sign put
// back and are scaled by `intPow(2, 0xf - table[i])` (the tables at 0x100643f0 and the int 2 at 0x10063f00);
// coefficients 3 upwards take the same scaling plus the offset table at 0x100643c8 and are then multiplied by
// the float table at 0x10064388 and offset by the int table at 0x100643a8 before __ftol (0x1003be9f-0x1003beb6).
// The float cell takes the level as a float (0x1003bec9) and each coefficient is divided by the float 16384.0f
// at 0x1005ebd8 into the float array (0x1003bef3-0x1003bf08). Returns 0 (0x1003bf0d).
typedef int(__cdecl* MedianOfThree_t)(const int*, const int*, const int*);
typedef int(__cdecl* DecodeTableSymbol_t)(const int*, int*, int*);
typedef int(__cdecl* CodecTransformStage_t)(const int*, int*, int*, int*, int*, float*, float*, void*);
CodecTransformStage_t CodecTransformStage_orig;
int __cdecl CodecTransformStage_re(const int* classCell, int* levelCell, int* coeffOut, int* flagPair,
                                   int* gainCell, float* levelFloat, float* coeffFloat, void* record) {
    if (coeffOut) coeffOut -= 1;                                 // je at 0x1003b5c2
    if (flagPair) flagPair -= 1;                                 // je at 0x1003b5d1
    if (coeffFloat) coeffFloat -= 1;                             // je at 0x1003b5e0

    char* rec = static_cast<char*>(record);
    int* f00 = reinterpret_cast<int*>(rec + 0x00);               // 0x1003b5eb
    int* f04 = reinterpret_cast<int*>(rec + 0x04);               // 0x1003b5f1
    int* f08 = reinterpret_cast<int*>(rec + 0x08);               // 0x1003b5fa
    int* f0c = reinterpret_cast<int*>(rec + 0x0C);               // 0x1003b603
    int* f10 = reinterpret_cast<int*>(rec + 0x10);               // 0x1003b60c
    int* f14 = reinterpret_cast<int*>(rec + 0x14);               // 0x1003b615
    int* hist = reinterpret_cast<int*>(rec + 0x18);              // 0x1003b61e
    int* gains = reinterpret_cast<int*>(rec + 0x90);             // 0x1003b627
    int* levels = reinterpret_cast<int*>(rec + 0x9C);            // 0x1003b633

    const int entry = reinterpret_cast<const int*>(c3br_sound(0x00064014))[*classCell];   // 0x1003b644
    int klass = 0;                                               // [ebp-0x44]
    int flagWord = 0;                                            // [ebp-0x3c]
    bool skipToFlagTail = false;

    if (c3br_i(0x000B58A8) == 0) {                               // jne at 0x1003b655
        flagPair[1] = 1;                                         // 0x1003b65e
        flagPair[2] = 1;                                         // 0x1003b668
        if (*classCell <= 1) flagPair[1] = 0;                    // jg at 0x1003b675
        if (*classCell == 0 || *classCell == 2) flagPair[2] = 0; // je at 0x1003b687, jne at 0x1003b68f
        *gainCell = entry;                                       // 0x1003b69b-0x1003b6a1
        if (*gainCell <= 4) *gainCell = *f00;                    // jg at 0x1003b6a9
        if (flagPair[1] == 1 && flagPair[2] == 1) *f00 = *gainCell;   // jne at 0x1003b6bc, 0x1003b6c5
        if (flagPair[1] != flagPair[2]) *gainCell = *f00;        // je at 0x1003b6dd
        // 0x1003b6e9 jumps straight to the output conversion
    } else {
        if (entry > 4) {                                         // jle at 0x1003b6f2
            gains[0] = entry;                                    // 0x1003b6fa
            klass = 2;                                           // 0x1003b6fc
            *f10 = (*f10 * 15 + entry + 8) / 16;                 // 0x1003b703-0x1003b71e
        } else {
            klass = entry;                                       // 0x1003b725
            gains[0] = *f10;                                     // 0x1003b728-0x1003b730
        }
        levels[0] = *levelCell;                                  // 0x1003b732-0x1003b73a

        int n = c3br_i(0x000B58A0);                              // 0x1003b73c
        for (int i = 1; i <= n; ++i)                             // jg at 0x1003b75d
            hist[i * 3 - 3] = coeffOut[i];                       // 0x1003b75f-0x1003b771

        const int codeIndex = klass + (*f08 << 4) + *f0c * 4 + 1;        // 0x1003b777-0x1003b78d
        const int code = reinterpret_cast<const int*>(c3br_sound(0x00063F10))[codeIndex];   // 0x1003b793
        const int rotate = code & 3;                             // 0x1003b79d-0x1003b7a3
        flagWord = code / 8;                                     // 0x1003b7a6-0x1003b7b2
        if (*f14 < c3br_i(0x00063F04)) flagWord = flagWord / 64; // jge at 0x1003b7c0
        int band = 4;                                            // 0x1003b7d1
        if (*f14 < c3br_i(0x00063F10)) band = 3;                 // jge at 0x1003b7e3
        if (*f14 < c3br_i(0x00063F0C)) band = 2;                 // jge at 0x1003b7f7
        if (*f14 < c3br_i(0x00063F08)) band = 1;                 // jge at 0x1003b80b
        flagPair[1] = (flagWord / 2) & 1;                        // 0x1003b814-0x1003b822
        flagPair[2] = flagWord & 1;                              // 0x1003b825-0x1003b82e

        if (*f04 != 0) {                                         // je at 0x1003b837
            *f04 = 0;                                            // 0x1003b83c
            *gainCell = entry;                                   // 0x1003b842-0x1003b848
            if (*gainCell <= 4) *gainCell = *f00;                // jg at 0x1003b850
            skipToFlagTail = true;                               // 0x1003b85c
        }

        if (!skipToFlagTail) {
            MedianOfThree_t median = reinterpret_cast<MedianOfThree_t>(c3br_sound(0x0003E480));
            if ((flagWord & c3br_i(0x00064450)) != 0) {          // je at 0x1003b86c
                DecodeTableSymbol_t decode =
                    reinterpret_cast<DecodeTableSymbol_t>(c3br_sound(0x0003E520));
                int symbols = 0;                                 // 0x1003b872
                int parity = levels[1] & 1;                      // 0x1003b879-0x1003b882
                int key = (hist[22] << 4) + levels[1] / 2;       // 0x1003b885-0x1003b89b
                int value = 0;
                decode(&key, &value, &symbols);                  // 0x1003b89e-0x1003b8aa
                levels[1] = levels[2];                           // 0x1003b8b2-0x1003b8bb
                if (value >= 0) levels[1] = parity + value * 2;  // jl at 0x1003b8c2, lea at 0x1003b8ca
                for (int i = 1; i <= 4; ++i) {                   // jg at 0x1003b8e9
                    int context;
                    if (i == 1)                                  // jne at 0x1003b8f3
                        context = (hist[25] & 7) * 2 + (hist[28] & 1);     // 0x1003b8f5-0x1003b90a
                    else
                        context = hist[(9 - i) * 3 - 2] & 0xF;             // 0x1003b90f-0x1003b924
                    const int cell = hist[(5 - i) * 3 - 2] & 0x1F;         // 0x1003b927-0x1003b93c
                    parity = cell & 1;                           // 0x1003b93f-0x1003b945
                    key = (context << 4) + cell / 2;             // 0x1003b948-0x1003b958
                    decode(&key, &value, &symbols);              // 0x1003b95b-0x1003b967
                    if (value >= 0) {                            // jl at 0x1003b973
                        value = parity + value * 2;              // 0x1003b975-0x1003b97e
                        if ((value & 0x10) == 0x10) value -= 0x20;         // jne at 0x1003b98a
                    } else {
                        value = hist[(5 - i) * 3 - 1];           // 0x1003b997-0x1003b9a9
                    }
                    hist[(5 - i) * 3 - 2] = value;               // 0x1003b9ac-0x1003b9bd
                }
                *f14 = c3br_ftol(static_cast<double>(*f14) * static_cast<double>(c3br_f(0x0005EBDC))
                                 + static_cast<double>(symbols * 0x66));   // 0x1003b9c6-0x1003b9e5
            }

            *levelCell = levels[1];                              // 0x1003b9e7-0x1003b9f0
            n = c3br_i(0x000B58A0);                              // 0x1003b9f2
            for (int i = 1; i <= n; ++i)                         // jg at 0x1003ba13
                coeffOut[i] = hist[i * 3 - 2];                   // 0x1003ba15-0x1003ba28
            if (rotate == 1) gains[1] = gains[2];                // jne at 0x1003ba31
            if (rotate == 3) gains[1] = gains[0];                // jne at 0x1003ba43
            *gainCell = gains[1];                                // 0x1003ba50-0x1003ba59

            const float* levelThr = reinterpret_cast<const float*>(c3br_sound(0x00063FA0));
            const float* histThr = reinterpret_cast<const float*>(c3br_sound(0x00063F80));
            const float* gainThr = reinterpret_cast<const float*>(c3br_sound(0x00063F90));
            if ((flagWord & c3br_i(0x00064448)) != 0) {          // je at 0x1003ba66
                int d = levels[1] - levels[0];                   // 0x1003ba6c-0x1003ba77
                int ad = (d < 0) ? -d : d;                       // jl at 0x1003ba7e
                // `fcomp` + `test ah, 1` + `jne`: the body is skipped when the distance is below the threshold.
                if (!(static_cast<double>(ad) < static_cast<double>(levelThr[band]))) {   // jne at 0x1003baa2
                    int d2 = levels[1] - levels[2];              // 0x1003baa4-0x1003bab0
                    int ad2 = (d2 < 0) ? -d2 : d2;               // jl at 0x1003bab7
                    if (!(static_cast<double>(ad2) < static_cast<double>(levelThr[band])))   // jne at 0x1003badb
                        *levelCell = median(&levels[2], &levels[1], &levels[0]);   // 0x1003badd-0x1003bafa
                }
                for (int i = 1; i <= 6; ++i) {                   // jg at 0x1003bb12
                    int e = hist[i * 3 - 2] - hist[i * 3 - 3];   // 0x1003bb18-0x1003bb32
                    int ae = (e < 0) ? -e : e;                   // jl at 0x1003bb39
                    const float t = histThr[band + i * 4 + 8];   // 0x1003bb4e-0x1003bb58
                    if (static_cast<double>(ae) < static_cast<double>(t)) continue;   // jne at 0x1003bb64
                    int e2 = hist[i * 3 - 2] - hist[i * 3 - 1];  // 0x1003bb6a-0x1003bb84
                    int ae2 = (e2 < 0) ? -e2 : e2;               // jl at 0x1003bb8b
                    if (static_cast<double>(ae2) < static_cast<double>(t)) continue;  // jne at 0x1003bbb6
                    coeffOut[i] = median(&hist[i * 3 - 1], &hist[i * 3 - 2],
                                         &hist[i * 3 - 3]);      // 0x1003bbb8-0x1003bbf0
                }
            }
            if ((flagWord & c3br_i(0x0006444C)) != 0) {          // je at 0x1003bc03
                int d = gains[1] - gains[0];                     // 0x1003bc09-0x1003bc14
                int ad = (d < 0) ? -d : d;                       // jl at 0x1003bc1b
                if (!(static_cast<double>(ad) < static_cast<double>(gainThr[band]))) {    // jne at 0x1003bc3f
                    int d2 = gains[1] - gains[2];                // 0x1003bc41-0x1003bc4d
                    int ad2 = (d2 < 0) ? -d2 : d2;               // jl at 0x1003bc54
                    if (!(static_cast<double>(ad2) < static_cast<double>(gainThr[band])))  // jne at 0x1003bc78
                        *gainCell = median(&gains[2], &gains[1], &gains[0]);   // 0x1003bc7a-0x1003bc97
                }
            }
        }

        if ((flagWord & c3br_i(0x00064454)) != 0) {              // je at 0x1003bca4
            n = c3br_i(0x000B58A0);                              // 0x1003bca6
            for (int i = 5; i <= n; ++i)                         // jg at 0x1003bcc6
                coeffOut[i] = reinterpret_cast<const int*>(c3br_sound(0x00064418))[i];   // 0x1003bcc8-0x1003bcd8
        }
        *f0c = klass;                                            // 0x1003bcdd-0x1003bce3
        *f08 = flagPair[2];                                      // 0x1003bce5-0x1003bcee
        gains[2] = gains[1];                                     // 0x1003bcf0-0x1003bcf9
        gains[1] = gains[0];                                     // 0x1003bcfc-0x1003bd04
        levels[2] = levels[1];                                   // 0x1003bd07-0x1003bd10
        levels[1] = levels[0];                                   // 0x1003bd13-0x1003bd1b
        n = c3br_i(0x000B58A0);                                  // 0x1003bd1e
        for (int i = 1; i <= n; ++i) {                           // jg at 0x1003bd3f
            hist[i * 3 - 1] = hist[i * 3 - 2];                   // 0x1003bd41-0x1003bd57
            hist[i * 3 - 2] = hist[i * 3 - 3];                   // 0x1003bd5b-0x1003bd71
        }
    }

    // 0x1003bd77: the output conversion, reached from both arms
    *levelCell = reinterpret_cast<const int*>(c3br_sound(0x00064214))[(0x1F - *levelCell) * 2];
                                                                 // 0x1003bd77-0x1003bd8d
    const int* magnitude = reinterpret_cast<const int*>(c3br_sound(0x00064314));
    const int* shiftTable = reinterpret_cast<const int*>(c3br_sound(0x000643F0));
    const int* powBase = reinterpret_cast<const int*>(c3br_sound(0x00063F00));
    IntPow_t intPow = reinterpret_cast<IntPow_t>(c3br_sound(0x0003E2C0));
    for (int i = 1; i <= 2; ++i) {                               // jg at 0x1003bda5
        int v = coeffOut[i];                                     // 0x1003bdab-0x1003bdb4
        int negative = 0;                                        // 0x1003bdb7
        if (v < 0) {                                             // jge at 0x1003bdc2
            negative = 1;                                        // 0x1003bdc4
            v = -v;                                              // 0x1003bdce
            if (v > 0xF) v = 0;                                  // jle at 0x1003bdd7
        }
        v = magnitude[v * 2];                                    // 0x1003bde0-0x1003bdec
        if (negative == 1) v = -v;                               // jne at 0x1003bdf3
        int exponent = 0xF - shiftTable[i];                      // 0x1003bdfd-0x1003be0c
        coeffOut[i] = v * intPow(powBase, &exponent);            // 0x1003be0f-0x1003be2c
    }
    const int* offsetA = reinterpret_cast<const int*>(c3br_sound(0x000643C8));
    const float* scale = reinterpret_cast<const float*>(c3br_sound(0x00064388));
    const int* offsetB = reinterpret_cast<const int*>(c3br_sound(0x000643A8));
    int n = c3br_i(0x000B58A0);                                  // 0x1003be34
    for (int i = 3; i <= n; ++i) {                               // jg at 0x1003be55
        int v = coeffOut[i];                                     // 0x1003be57-0x1003be60
        int exponent = 0xF - shiftTable[i];                      // 0x1003be63-0x1003be72
        v = v * intPow(powBase, &exponent);                      // 0x1003be75-0x1003be8c
        v = v + offsetA[i];                                      // 0x1003be8f-0x1003be9c
        // fild dword [ebp-0x50]; fmul dword [ecx*4+0x10064388]; fiadd dword [edx*4+0x100643a8]; __ftol
        coeffOut[i] = c3br_ftol(static_cast<double>(v) * static_cast<double>(scale[i])
                                + static_cast<double>(offsetB[i]));   // 0x1003be9f-0x1003beb6
    }
    *levelFloat = static_cast<float>(static_cast<double>(*levelCell));   // 0x1003bec6-0x1003bece
    const float divisor = c3br_f(0x0005EBD8);                    // 16384.0f
    n = c3br_i(0x000B58A0);                                      // 0x1003bed0
    for (int i = 1; i <= n; ++i)                                 // jg at 0x1003bef1
        coeffFloat[i] = static_cast<float>(static_cast<double>(coeffOut[i])
                                           / static_cast<double>(divisor));   // 0x1003bef3-0x1003bf08
    return 0;                                                    // 0x1003bf0d
}

SG_HOOK("sound.dll", 0x0003c220, sound_quantizeParams, QuantizeParams_re, QuantizeParams_orig);
SG_HOOK("sound.dll", 0x0003b5a0, sound_codecTransformStage, CodecTransformStage_re, CodecTransformStage_orig);
SG_HOOK("sound.dll", 0x0003da50, sound_interpSubframeParams, InterpSubframeParams_re,
        InterpSubframeParams_orig);
SG_HOOK("sound.dll", 0x0003b230, sound_codecSynthStage, CodecSynthStage_re, CodecSynthStage_orig);

}  // namespace
