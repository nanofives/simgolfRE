// C3 batch c3bv (2026-10-08) of sound.dll: the speech codec's stage drivers. Addresses in SG_HOOK and in the
// `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are VAs
// (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32 or COM call is made, nothing is allocated or freed, and no live sound object,
// device, list or buffer is touched: every buffer, cell, array and codec context the A/B passes in is allocated
// by the fixture.
//
// THE FRAME IS PART OF THE FUNCTION. All four bodies are /Od /GZ debug builds whose frame is filled with
// 0xcccccccc by a `rep stosd` in the prologue, and all four hand their callees pointers INTO that frame: the
// blocks overlap (codecProcessBlock's int coefficient array at [ebp-0x64] and its float coefficient array at
// [ebp-0x3c] are 0x28 bytes apart), one callee's output is the next one's input, and codecStageDispatch indexes
// one block by an index another callee produced (0x1003cddc, 0x1003cdea, 0x1003cf21). A reimplementation that
// declared separate locals would not reproduce that, so each body here allocates the original's frame as one
// array of the original's size, fills it with 0xcc and addresses every local as `ebp - offset` through
// c3bv_loc<T>(). The arrays are `unsigned int[]` so the frame is 4-aligned, which is all the x87 dword and qword
// accesses of the callees need.
//
// MODULE GLOBALS: g_codecOrder 0x100b58a0 and the frame length 0x100b58a4 are READ (never written) by
// codecStageDispatch, and their ADDRESSES are handed to six of its callees (0x1003cc48, 0x1003cc81, 0x1003ccc6,
// 0x1003cd2e, 0x1003cf56, 0x1003cfd4). A byte scan of sound.dll's .text finds exactly one writing instruction
// for each, both inside codecSetParams 0x1003aa50 (`mov [0x100b58a0], 0xa` at 0x1003aa68, `mov [0x100b58a4],
// 0xb4` at 0x1003aa72); the fixtures store those same values (10 and 180). Neither arm writes them.
//
// Floating point: these are /Od /GZ debug bodies (no `fninit` and no `fldcw` in any of their prologues), so
// every intermediate is spilled to a named stack slot and the rounding points are visible. Each `fstp dword`
// spill is written as an assignment to a float and every operand inside one expression is widened with
// static_cast<double>, which is the c3ap method: the A/B thread runs with control word 0x027f (53-bit, round to
// nearest), so an x87 step that ends in a register is exactly a double operation and a step that ends in a dword
// slot is exactly a double operation rounded once to float. A `fst dword` that is NOT followed by a pop keeps
// the unrounded value in st(0) for the next use (codecFeatureDecision at 0x1003ff43), so what follows it uses
// the double and only the stored slot is a float. `fild`/`fidiv` take their integer operand exactly.
// sqrt is called through the DLL's own CRT routine 0x10043204, so both arms use the same code.
#include "hooks.h"

#include <string.h>

namespace {

// sound.dll has base relocations, so an original callee, table or constant is addressed through the loaded base.
char* c3bv_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

float c3bv_f(unsigned rva) { return *reinterpret_cast<const float*>(c3bv_sound(rva)); }
double c3bv_d(unsigned rva) { return *reinterpret_cast<const double*>(c3bv_sound(rva)); }
int c3bv_i(unsigned rva) { return *reinterpret_cast<const int*>(c3bv_sound(rva)); }

// A local of the modelled frame: `ebp - off`, exactly as the original addresses it.
template <class T>
T* c3bv_loc(char* ebp, unsigned off) { return reinterpret_cast<T*>(ebp - off); }

// Slack on both sides of a modelled frame. In the original there is live stack above ebp (the caller's own
// frame) and below the frame (the callees' frames), and two callees of codecStageDispatch write outside the
// block they were given: codecPitchAnalysis' score store at 0x1004027b lands before its score array
// (log/c3/c3bn_notes.txt section C) and codecRemoveMean 0x1003ed00 writes as many floats as the segment
// bounds FUN_1003edc0 produced. Those writes fall in unused stack in the original; a bare C local array
// would put the compiler's stack cookie exactly there, so each body allocates C3BV_PAD bytes on both sides
// and models ebp in the middle.
const unsigned C3BV_PAD = 0x1000;

// A raw dword copy: the originals move float slots with `mov`, which carries a NaN or a 0xcccccccc fill
// through unchanged.
void c3bv_copy32(void* dst, const void* src) {
    *reinterpret_cast<unsigned*>(dst) = *reinterpret_cast<const unsigned*>(src);
}

// Callees are called at their original addresses in the loaded module. Their parameter types are not determined
// here, so they are declared by arity over void*; every pointer handed to them is one the original hands them.
typedef int(__cdecl* Cdecl3_t)(void*, void*, void*);
typedef int(__cdecl* Cdecl4_t)(void*, void*, void*, void*);
typedef int(__cdecl* Cdecl5_t)(void*, void*, void*, void*, void*);
typedef int(__cdecl* Cdecl6_t)(void*, void*, void*, void*, void*, void*);
typedef int(__cdecl* Cdecl7_t)(void*, void*, void*, void*, void*, void*, void*);
typedef int(__cdecl* Cdecl8_t)(void*, void*, void*, void*, void*, void*, void*, void*);
typedef int(__cdecl* Cdecl9_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef int(__cdecl* Cdecl11_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef int(__cdecl* Cdecl13_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
                                void*, void*);
typedef int(__cdecl* Cdecl14_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
                                void*, void*, void*);
typedef int(__cdecl* FloatFloorToInt_t)(const float*);
typedef double(__cdecl* Sqrt_t)(double);

// ---------------------------------------------------------------------------------------------------------
// speech codec: the decoder block driver
// ---------------------------------------------------------------------------------------------------------

// 0x0003a980  codecProcessBlock (VA 0x1003a980)
// __cdecl with three stack arguments (`ret` at 0x1003aa40 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0xac..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x2b, at 0x1003a99c).
//   [ebp+8]    the packed parameter words, moved back by one dword at 0x1003a9a7 when it is not null
//              (`je 0x1003a9ad` at 0x1003a9a2), so that the callee is handed the original pointer again by the
//              `add edx, 4` at 0x1003a9bf: the pair is the one-based convention of the whole codec
//   [ebp+0xc]  the output samples, moved back by one dword at 0x1003a9b6 (`je 0x1003a9bc` at 0x1003a9b1) and
//              handed on as `+ 4` at 0x1003aa0f
//   [ebp+0x10] the decoder context: its first 0xa8 bytes are codecTransformStage's stage record and
//              +0xa8..+0xc08 is codecSynthStage's state (c3br, re/frida/registry.d/c3br.py)
// The body is three calls over eight locals of its own frame and nothing else (0x1003a99e-0x1003aa2b):
//   bitUnpack 0x1003c1d0 (0x1003a9d4) reads the packed words into the class cell [ebp-0x14], the level cell
//     [ebp-4] and the quantised coefficient array [ebp-0x64], under the count at the descriptor 0x10063ef0
//     (its first dword is 10, the order);
//   codecTransformStage 0x1003b5a0 (0x1003a9fc) turns those three into the flag block [ebp-0xc], the sub-frame
//     length cell [ebp-0x10], the gain cell [ebp-0x6c] and the float coefficient array [ebp-0x3c], using the
//     context's first 0xa8 bytes as its history record;
//   codecSynthStage 0x1003b230 (0x1003aa23) synthesises from those four into the caller's output array and
//     writes the produced sample count to [ebp-0x68].
// The int coefficient array [ebp-0x64] and the float coefficient array [ebp-0x3c] are 0x28 bytes apart in the
// frame, which is exactly the ten dwords of one order-10 parameter set, so the two never overlap in use.
// Returns 0 (0x1003aa2b).
typedef int(__cdecl* CodecProcessBlock_t)(const int*, float*, void*);
CodecProcessBlock_t CodecProcessBlock_orig;
int __cdecl CodecProcessBlock_re(const int* packed, float* out, void* ctx) {
    const unsigned FRAME = 0xAC;                                 // the original's frame size
    unsigned int rawFrame[(FRAME + 2 * C3BV_PAD) / 4];
    memset(rawFrame, 0, sizeof rawFrame);
    memset(reinterpret_cast<char*>(rawFrame) + C3BV_PAD, 0xCC, FRAME);                     // rep stosd at 0x1003a99c
    char* const ebp = reinterpret_cast<char*>(rawFrame) + C3BV_PAD + FRAME;

    if (packed != 0) packed -= 1;                                // je at 0x1003a9a2, sub at 0x1003a9a7
    if (out != 0) out -= 1;                                      // je at 0x1003a9b1, sub at 0x1003a9b6

    Cdecl5_t bitUnpack = reinterpret_cast<Cdecl5_t>(c3bv_sound(0x0003C1D0));
    Cdecl8_t transformStage = reinterpret_cast<Cdecl8_t>(c3bv_sound(0x0003B5A0));
    Cdecl7_t synthStage = reinterpret_cast<Cdecl7_t>(c3bv_sound(0x0003B230));

    bitUnpack(c3bv_sound(0x00063EF0),                            // push at 0x1003a9cf
              c3bv_loc<void>(ebp, 0x14), c3bv_loc<void>(ebp, 0x04), c3bv_loc<void>(ebp, 0x64),
              const_cast<int*>(packed + 1));                     // add edx, 4 at 0x1003a9bf

    transformStage(c3bv_loc<void>(ebp, 0x14), c3bv_loc<void>(ebp, 0x04), c3bv_loc<void>(ebp, 0x64),
                   c3bv_loc<void>(ebp, 0x0C), c3bv_loc<void>(ebp, 0x10), c3bv_loc<void>(ebp, 0x6C),
                   c3bv_loc<void>(ebp, 0x3C), ctx);              // call at 0x1003a9fc

    synthStage(c3bv_loc<void>(ebp, 0x0C), c3bv_loc<void>(ebp, 0x10), c3bv_loc<void>(ebp, 0x6C),
               c3bv_loc<void>(ebp, 0x3C), out + 1,               // add ecx, 4 at 0x1003aa0f
               c3bv_loc<void>(ebp, 0x68), ctx);                  // call at 0x1003aa23
    return 0;                                                    // xor eax, eax at 0x1003aa2b
}

// ---------------------------------------------------------------------------------------------------------
// speech codec: the encoder resource driver
// ---------------------------------------------------------------------------------------------------------

// 0x0003b150  codecProcessResource (VA 0x1003b150)
// __cdecl with three stack arguments (`ret` at 0x1003b224 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0xa8..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x2a, at 0x1003b16c).
// It is codecProcessBlock's mirror image: samples in, packed parameter words out.
//   [ebp+8]    the input samples, moved back by one dword at 0x1003b177 when not null (`je 0x1003b17d`) and
//              handed on as `+ 4` at 0x1003b198, 0x1003b1bb
//   [ebp+0xc]  the packed parameter words to write, moved back by one dword at 0x1003b186 and handed on as
//              `+ 4` at 0x1003b1f2
//   [ebp+0x10] the encoder context (the 0x2600-byte object codecStageDispatch, codecGainSmoother and
//              bitPermute's toggle at +0x2540 share)
// Four calls over seven locals of its own frame (0x1003b18c-0x1003b20f):
//   codecApplyFixedCoeffs 0x1003d040 (0x1003b19c) pre-filters the samples in place under the descriptor
//     0x10063ef4 (180, 10);
//   codecStageDispatch 0x1003c640 (0x1003b1bf) runs the whole analysis and leaves the flag pair [ebp-0xc], the
//     index cell [ebp-0x10], the float level cell [ebp-0x68] and the float coefficient array [ebp-0x3c];
//   quantizeParams 0x1003c220 (0x1003b1e3) turns those four into the class cell [ebp-0x14], the exponent cell
//     [ebp-4] and the quantised coefficient array [ebp-0x64];
//   bitPack 0x1003c180 (0x1003b207) writes those three into the caller's output words under the descriptor
//     0x10063ef8, with the context as bitPermute's toggle owner (0x1003bf76).
// Returns 0 (0x1003b20f).
typedef int(__cdecl* CodecProcessResource_t)(const float*, int*, void*);
CodecProcessResource_t CodecProcessResource_orig;
int __cdecl CodecProcessResource_re(const float* samples, int* packed, void* ctx) {
    const unsigned FRAME = 0xA8;                                 // the original's frame size
    unsigned int rawFrame[(FRAME + 2 * C3BV_PAD) / 4];
    memset(rawFrame, 0, sizeof rawFrame);
    memset(reinterpret_cast<char*>(rawFrame) + C3BV_PAD, 0xCC, FRAME);                     // rep stosd at 0x1003b16c
    char* const ebp = reinterpret_cast<char*>(rawFrame) + C3BV_PAD + FRAME;

    if (samples != 0) samples -= 1;                              // je at 0x1003b172, sub at 0x1003b177
    if (packed != 0) packed -= 1;                                // je at 0x1003b181, sub at 0x1003b186

    Cdecl3_t applyFixedCoeffs = reinterpret_cast<Cdecl3_t>(c3bv_sound(0x0003D040));
    Cdecl6_t stageDispatch = reinterpret_cast<Cdecl6_t>(c3bv_sound(0x0003C640));
    Cdecl7_t quantizeParams = reinterpret_cast<Cdecl7_t>(c3bv_sound(0x0003C220));
    Cdecl6_t bitPack = reinterpret_cast<Cdecl6_t>(c3bv_sound(0x0003C180));

    applyFixedCoeffs(const_cast<float*>(samples + 1),            // add eax, 4 at 0x1003b198
                     c3bv_sound(0x00063EF4), ctx);               // call at 0x1003b19c

    stageDispatch(const_cast<float*>(samples + 1),               // add eax, 4 at 0x1003b1bb
                  c3bv_loc<void>(ebp, 0x0C), c3bv_loc<void>(ebp, 0x10), c3bv_loc<void>(ebp, 0x68),
                  c3bv_loc<void>(ebp, 0x3C), ctx);               // call at 0x1003b1bf

    quantizeParams(c3bv_loc<void>(ebp, 0x0C), c3bv_loc<void>(ebp, 0x10), c3bv_loc<void>(ebp, 0x68),
                   c3bv_loc<void>(ebp, 0x3C), c3bv_loc<void>(ebp, 0x14), c3bv_loc<void>(ebp, 0x04),
                   c3bv_loc<void>(ebp, 0x64));                   // call at 0x1003b1e3

    bitPack(c3bv_sound(0x00063EF8), c3bv_loc<void>(ebp, 0x14), c3bv_loc<void>(ebp, 0x04),
            c3bv_loc<void>(ebp, 0x64), packed + 1,               // add eax, 4 at 0x1003b1f2
            ctx);                                                // call at 0x1003b207
    return 0;                                                    // xor eax, eax at 0x1003b20f
}

// ---------------------------------------------------------------------------------------------------------
// speech codec: the per-half decision stage
// ---------------------------------------------------------------------------------------------------------

// 0x0003f720  codecFeatureDecision (VA 0x1003f720)
// __cdecl with thirteen stack arguments (`ret` at 0x1003ffb6 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0x11c..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x47, at 0x1003f73c).
// Twelve pointers into the encoder context are bound on entry (0x1003f73e-0x1003f7cd):
//   +0x2224 (A) the output level cell          +0x2248 (E) a count            +0x2258 (I) a count
//   +0x2228 (B) the smoothed level             +0x224c (F) a count            +0x225c (K) a scaled accumulator
//   +0x222c (C) feature 1                      +0x2250 (G) a count            +0x2260 (J) a count
//   +0x2230 (D) six floats of score history    +0x2254 (H) a count            +0x2264 (L) a scaled accumulator
// Arguments:
//   [ebp+8]    the window range, moved back one dword at 0x1003f7d9 and handed on as `+ 4` at 0x1003f917
//   [ebp+0xc]  the first sample array, moved back by desc[1] dwords (0x1003f7f4-0x1003f808) and handed on with
//              the same count added again at 0x1003f906, so the rebase cancels
//   [ebp+0x10] the second sample array, the same with desc[3] (0x1003f811-0x1003f825, 0x1003f900)
//   [ebp+0x14] the window descriptor, moved back one dword at 0x1003f7e8; its elements 1 and 3 are the two
//              rebases just named (0x1003f7f7, 0x1003f814)
//   [ebp+0x18] the half-window index cell, 1 on the first call and 2 on the second
//   [ebp+0x1c] a float cell used as a divisor, [ebp+0x20] a float cell used as a numerator
//   [ebp+0x24] the lag cell handed to codecWindowFeatures
//   [ebp+0x28] a block moved back one dword at 0x1003f831; its element 2 is feature 6 (0x1003fa55)
//   [ebp+0x2c] the input flag block, moved back one dword at 0x1003f840 (elements 1, 2 and 3 are read)
//   [ebp+0x30] the output flag block, moved back one dword at 0x1003f84f (+0xc, +0x10, +0x14, +0x18 and +0x1c
//              of the moved pointer are read and written)
//   [ebp+0x34] IS NOT REFERENCED anywhere in the body
//   [ebp+0x38] the encoder context
// FIRST HALF ONLY (`jne 0x1003f8c8` at 0x1003f85b): the score history is shifted down by two slots
// (0x1003f85d-0x1003f889, D[0]=D[2] .. D[3]=D[5]) and feature 1 is recomputed as
// `*[ebp+0x20] / max(*[ebp+0x1c], 1.0f)` (0x1003f88c-0x1003f8c6, the float 1.0f at 0x1005b8a8).
// Both halves: codecWindowFeatures 0x10041020 (0x1003f91b) fills the crossing count [ebp-0x78], the two level
// sums [ebp-0x94] and [ebp-0x90], the two float ratios [ebp-0x80] and [ebp-0x8c] and the two correlations
// [ebp-0x20] and [ebp-0x24]. The smoothed level is then updated to
// `floor((*G / max(*H,1) + *B) * 63 / 64)` (0x1003f942-0x1003f982, floatFloorToInt 0x1003e400) and the band
// selector `*H * *B / max(*F,1)` (0x1003f9a3-0x1003f9b6) picks the first row of the descending threshold table
// 0x10064e10 that it is above (`jne 0x1003fa02` at 0x1003f9fe; the table's first dword is the row count 5, so
// the band is 1..5).
// The eight features are then laid out one-based at [ebp-0x78] (0x1003fa04-0x1003fa64) and scored against the
// row's bias 0x10064c7c + band*40 and its eight weights 0x10064c54 + band*40 + j*4
// (0x1003fa67-0x1003fac9), each step rounded to float by the `fstp dword` at 0x1003fac5. The sign of the score
// becomes the per-half flag at outFlags + half*4 + 0x18 (0x1003facb-0x1003faf4).
// SECOND HALF ONLY: the four flags at +0xc, +0x10, +0x14 and +0x18 are read as a 4-bit code (0x1003fb5f-
// 0x1003fb80) and a sixteen-way switch (jump table at 0x1003ffb7, `ja 0x1003fc04` at 0x1003fbab) adjusts them.
// Finally the two pairs of counts are updated (0x1003fd8b-0x1003ff10) and the output level cell takes
// `sqrt(*F * *E) * 64 / 3000` clamped to [1, 20] (0x1003ff12-0x1003ff9f).
// Returns 0 (0x1003ffa1).
typedef int(__cdecl* CodecFeatureDecision_t)(void*, void*, void*, void*, void*, void*, void*, void*, void*,
                                             void*, void*, void*, void*);
CodecFeatureDecision_t CodecFeatureDecision_orig;
int __cdecl CodecFeatureDecision_re(void* rangeIn, void* firstIn, void* secondIn, void* descIn, void* halfCell,
                                    void* divisorCell, void* numeratorCell, void* lagCell, void* blockIn,
                                    void* inFlagsIn, void* outFlagsIn, void* unusedArg, void* ctx) {
    const unsigned FRAME = 0x11C;                                 // the original's frame size
    unsigned int rawFrame[(FRAME + 2 * C3BV_PAD) / 4];
    memset(rawFrame, 0, sizeof rawFrame);
    memset(reinterpret_cast<char*>(rawFrame) + C3BV_PAD, 0xCC, FRAME);                     // rep stosd at 0x1003f73c
    char* const ebp = reinterpret_cast<char*>(rawFrame) + C3BV_PAD + FRAME;
    (void)unusedArg;                                             // [ebp+0x34] is never read

    char* const c = static_cast<char*>(ctx);
    float* const A = reinterpret_cast<float*>(c + 0x2224);       // 0x1003f741
    float* const B = reinterpret_cast<float*>(c + 0x2228);       // 0x1003f74c
    float* const C = reinterpret_cast<float*>(c + 0x222C);       // 0x1003f75b
    float* const D = reinterpret_cast<float*>(c + 0x2230);       // 0x1003f76a
    int* const E = reinterpret_cast<int*>(c + 0x2248);           // 0x1003f775
    int* const F = reinterpret_cast<int*>(c + 0x224C);           // 0x1003f781
    int* const G = reinterpret_cast<int*>(c + 0x2250);           // 0x1003f78d
    int* const H = reinterpret_cast<int*>(c + 0x2254);           // 0x1003f798
    int* const I = reinterpret_cast<int*>(c + 0x2258);           // 0x1003f7a4
    int* const J = reinterpret_cast<int*>(c + 0x2260);           // 0x1003f7b0
    int* const K = reinterpret_cast<int*>(c + 0x225C);           // 0x1003f7bb
    int* const L = reinterpret_cast<int*>(c + 0x2264);           // 0x1003f7c7

    char* range = static_cast<char*>(rangeIn);
    if (range != 0) range -= 4;                                  // je at 0x1003f7d4, sub at 0x1003f7d9
    int* desc = static_cast<int*>(descIn);
    if (desc != 0) desc -= 1;                                    // je at 0x1003f7e3, sub at 0x1003f7e8
    // The two rebase counts live in [ebp-8] and [ebp-0xc] and are written only on the non-null path, so a null
    // array leaves the 0xcccccccc fill there and the pointer handed to codecWindowFeatures carries it
    // (0x1003f900, 0x1003f906).
    char* first = static_cast<char*>(firstIn);
    if (first != 0) {                                            // je at 0x1003f7f2
        *c3bv_loc<int>(ebp, 0x08) = desc[1];                     // 0x1003f7f7-0x1003f7fa
        first -= *c3bv_loc<int>(ebp, 0x08) * 4;                  // 0x1003f7fd-0x1003f808
    }
    char* second = static_cast<char*>(secondIn);
    if (second != 0) {                                           // je at 0x1003f80f
        *c3bv_loc<int>(ebp, 0x0C) = desc[3];                     // 0x1003f814-0x1003f817
        second -= *c3bv_loc<int>(ebp, 0x0C) * 4;                 // 0x1003f81a-0x1003f825
    }
    char* block = static_cast<char*>(blockIn);
    if (block != 0) block -= 4;                                  // je at 0x1003f82c, sub at 0x1003f831
    char* inFlags = static_cast<char*>(inFlagsIn);
    if (inFlags != 0) inFlags -= 4;                              // je at 0x1003f83b, sub at 0x1003f840
    char* outFlags = static_cast<char*>(outFlagsIn);
    if (outFlags != 0) outFlags -= 4;                            // je at 0x1003f84a, sub at 0x1003f84f

    const int half = *static_cast<const int*>(halfCell);

    if (half == 1) {                                             // jne 0x1003f8c8 at 0x1003f85b
        c3bv_copy32(&D[0], &D[2]);                               // 0x1003f85d-0x1003f866
        c3bv_copy32(&D[1], &D[3]);                               // 0x1003f868-0x1003f871
        c3bv_copy32(&D[2], &D[4]);                               // 0x1003f874-0x1003f87d
        c3bv_copy32(&D[3], &D[5]);                               // 0x1003f880-0x1003f889
        // fld [divisorCell]; fcomp 1.0f at 0x1005b8a8; test ah,1 / jne 0x1003f8ab
        float divisor;
        if (*static_cast<const float*>(divisorCell) < c3bv_f(0x0005B8A8))
            divisor = 1.0f;                                      // the immediate 0x3f800000 at 0x1003f8ab
        else
            divisor = *static_cast<const float*>(divisorCell);   // 0x1003f89e-0x1003f8a3
        // fld [numeratorCell]; fdiv [ebp-0xa0]; fstp dword [*C]   (0x1003f8b5-0x1003f8c6)
        *C = static_cast<float>(static_cast<double>(*static_cast<const float*>(numeratorCell))
                                / static_cast<double>(divisor));
    }

    // codecWindowFeatures, fourteen arguments, the last seven of them out cells of this frame
    // (0x1003f8c8-0x1003f91b).
    reinterpret_cast<Cdecl14_t>(c3bv_sound(0x00041020))(
        range + 4, first + *c3bv_loc<int>(ebp, 0x08) * 4, second + *c3bv_loc<int>(ebp, 0x0C) * 4, desc + 1,
        halfCell, A, lagCell,
        c3bv_loc<void>(ebp, 0x78), c3bv_loc<void>(ebp, 0x94), c3bv_loc<void>(ebp, 0x90),
        c3bv_loc<void>(ebp, 0x80), c3bv_loc<void>(ebp, 0x8C), c3bv_loc<void>(ebp, 0x20),
        c3bv_loc<void>(ebp, 0x24));

    FloatFloorToInt_t floorToInt = reinterpret_cast<FloatFloorToInt_t>(c3bv_sound(0x0003E400));

    const int hDen = (*H >= 1) ? *H : 1;                         // jl 0x1003f938 at 0x1003f92a
    // fild [*G]; fidiv hDen; fadd [*B]; fmul 63.0f at 0x1005ec68; fdiv 64.0f at 0x1005ebfc; fstp [ebp-0x18]
    *c3bv_loc<float>(ebp, 0x18) = static_cast<float>(
        ((static_cast<double>(*G) / static_cast<double>(hDen)) + static_cast<double>(*B))
        * static_cast<double>(c3bv_f(0x0005EC68)) / static_cast<double>(c3bv_f(0x0005EBFC)));
    const int smoothed = floorToInt(c3bv_loc<float>(ebp, 0x18));     // call at 0x1003f968
    *B = static_cast<float>(static_cast<double>(smoothed));      // fild [ebp-0xa8]; fstp [*B] at 0x1003f976

    const int fDen = (*F >= 1) ? *F : 1;                         // jl 0x1003f999 at 0x1003f98a
    // fild [*H]; fmul [*B]; fidiv fDen; fstp dword [ebp-0x9c]   (0x1003f9a3-0x1003f9b6)
    const float selector = static_cast<float>(
        static_cast<double>(*H) * static_cast<double>(*B) / static_cast<double>(fDen));

    const int* thresholdTable = reinterpret_cast<const int*>(c3bv_sound(0x00064E10));
    const float* thresholds = reinterpret_cast<const float*>(thresholdTable);
    int band = 1;                                                // 0x1003f9bc, 0x1003f9cf
    const int bandMax = thresholdTable[0] - 1;                   // 0x1003f9c3-0x1003f9cc
    // fld selector; fcomp thresholds[band]; test ah,0x41 / jne 0x1003fa02 (keep looking while <=)
    while (band <= bandMax                                       // jg 0x1003fa04 at 0x1003f9e7
           && !(static_cast<double>(selector) > static_cast<double>(thresholds[band])))
        band += 1;                                               // 0x1003f9d8-0x1003f9de

    // The feature vector is one-based at [ebp-0x78]: feature j lives at [ebp-0x78+j*4] (0x1003fab2).
    float* const feature = c3bv_loc<float>(ebp, 0x78);
    c3bv_copy32(&feature[1], C);                                 // 0x1003fa04-0x1003fa0c
    const int eDen = (*E >= 1) ? *E : 1;                         // jl 0x1003fa24 at 0x1003fa15
    // fild [ebp-0x94]; fidiv eDen; fstp dword [ebp-0x70]       (0x1003fa2e-0x1003fa3a)
    feature[2] = static_cast<float>(static_cast<double>(*c3bv_loc<int>(ebp, 0x94))
                                    / static_cast<double>(eDen));
    // fild [ebp-0x78]; fstp dword [ebp-0x6c]                   (0x1003fa3d-0x1003fa40)
    feature[3] = static_cast<float>(static_cast<double>(*c3bv_loc<int>(ebp, 0x78)));
    c3bv_copy32(&feature[4], c3bv_loc<void>(ebp, 0x8C));         // 0x1003fa43-0x1003fa49
    c3bv_copy32(&feature[5], c3bv_loc<void>(ebp, 0x80));         // 0x1003fa4c-0x1003fa4f
    c3bv_copy32(&feature[6], block + 8);                         // 0x1003fa52-0x1003fa58
    c3bv_copy32(&feature[7], c3bv_loc<void>(ebp, 0x20));         // 0x1003fa5b-0x1003fa5e
    c3bv_copy32(&feature[8], c3bv_loc<void>(ebp, 0x24));         // 0x1003fa61-0x1003fa64

    const float* weights = reinterpret_cast<const float*>(c3bv_sound(0x00064C54));
    const float* biases = reinterpret_cast<const float*>(c3bv_sound(0x00064C7C));
    float* const score = reinterpret_cast<float*>(reinterpret_cast<char*>(D) + half * 4 + 0xC);
    c3bv_copy32(score, &biases[band * 10]);                      // 0x1003fa67-0x1003fa7c
    for (int j = 1; j <= 8; ++j)                                 // jg 0x1003facb at 0x1003fa96
        // fld weights[band*10+j]; fmul feature[j]; fadd [score]; fstp dword [score]  (0x1003faab-0x1003fac5)
        *score = static_cast<float>(static_cast<double>(weights[band * 10 + j])
                                    * static_cast<double>(feature[j])
                                    + static_cast<double>(*score));

    int* const outFlag = reinterpret_cast<int*>(outFlags + half * 4 + 0x18);
    // fld [score]; fcomp 0.0f at 0x1005ec00; test ah,0x41 / jne 0x1003faf6
    if (static_cast<double>(*score) > static_cast<double>(c3bv_f(0x0005EC00)))
        *outFlag = 1;                                            // 0x1003faec
    else
        *outFlag = 0;                                            // 0x1003fafe

    int* const of = reinterpret_cast<int*>(outFlags);
    const int* const inf = reinterpret_cast<const int*>(inFlags);
    if (half != 1) {                                             // jne 0x1003fb1d at 0x1003fb16
        // (inFlags[1] & 2) != 0 or inFlags[2] == 1, and (inFlags[3] & 1) == 0   (0x1003fb1d-0x1003fb56)
        int gate;
        if (((inf[1] & 2) != 0 || inf[2] == 1) && (inf[3] & 1) == 0)
            gate = 1;                                            // 0x1003fb40
        else
            gate = 0;                                            // 0x1003fb4c
        // the four flags as a 4-bit code (0x1003fb5f-0x1003fb80)
        const int code = of[3] * 8 + of[4] * 4 + of[5] * 2 + of[6];
        // switch over the jump table at 0x1003ffb7 (`ja 0x1003fc04` at 0x1003fbab)
        switch (code) {
        case 0: case 3: case 9: case 12: case 15:
            break;                                               // the five stubs that jump to 0x1003fd8b
        case 2:                                                  // 0x1003fc22
            // fld D[3]; fchs; fcomp D[2]; test ah,0x41 / jne 0x1003fc4c
            if (of[7] != 0 && -static_cast<double>(D[3]) <= static_cast<double>(D[2]))
                of[6] = 1;                                       // 0x1003fc4f
            else
                of[5] = 0;                                       // 0x1003fc43
            break;
        case 4:                                                  // 0x1003fc5b
            of[4] = 0;
            break;
        case 5:                                                  // 0x1003fc6a
            // fld D[2]; fchs; fcomp D[1]; test ah,0x41 / jne 0x1003fc8b
            if (-static_cast<double>(D[2]) <= static_cast<double>(D[1]))
                of[5] = 1;                                       // 0x1003fc8e
            else
                of[4] = 0;                                       // 0x1003fc82
            break;
        case 6:                                                  // 0x1003fc9a
            // je 0x1003fcbe at 0x1003fca1 and at 0x1003fcaa; fld D[3]; fcomp D[0]; jne 0x1003fcca
            if (of[1] == 1 || of[7] == 1)
                of[6] = 1;                                       // 0x1003fcc1
            else if (static_cast<double>(D[3]) <= static_cast<double>(D[0]))
                of[3] = 1;                                       // 0x1003fccd
            else
                of[6] = 1;                                       // 0x1003fcc1
            break;
        case 7:                                                  // 0x1003fcd9
            if (gate != 0) of[4] = 0;                            // je 0x1003fce9 at 0x1003fcdd
            break;
        case 8:                                                  // 0x1003fcee
            if (gate != 0) of[4] = 1;                            // je 0x1003fcfe at 0x1003fcf2
            break;
        case 10:                                                 // 0x1003fd03
            // fld D[1]; fchs; fcomp D[2]; test ah,0x41 / jne 0x1003fd24
            if (-static_cast<double>(D[1]) <= static_cast<double>(D[2]))
                of[4] = 1;                                       // 0x1003fd27
            else
                of[5] = 0;                                       // 0x1003fd1b
            break;
        case 11:                                                 // 0x1003fd30
            of[4] = 1;
            break;
        case 13:                                                 // 0x1003fd3c
            // jne 0x1003fd66 at 0x1003fd43; fld D[2]; fchs; fcomp D[3]; jne 0x1003fd66
            if (of[7] != 0 || -static_cast<double>(D[2]) <= static_cast<double>(D[3]))
                of[5] = 1;                                       // 0x1003fd69
            else
                of[6] = 0;                                       // 0x1003fd5d
            break;
        case 14:                                                 // 0x1003fd72
            // je 0x1003fd8b at 0x1003fd76, jne 0x1003fd8b at 0x1003fd7f
            if (gate != 0 && of[7] == 0) of[5] = 0;              // 0x1003fd84
            break;
        default:                                                 // 0x1003fc04 (code 1 and every code above 15)
            if (gate != 0 && of[7] == 1) of[5] = 1;              // je 0x1003fc1d at 0x1003fc08,
            break;                                               // jne 0x1003fc1d at 0x1003fc11, 0x1003fc16
        }
    }

    if (*reinterpret_cast<const int*>(outFlags + (*static_cast<const int*>(halfCell)) * 4 + 0x18) == 0) {
        // jne 0x1003feaa at 0x1003fd98
        const int level2 = *c3bv_loc<int>(ebp, 0x90);            // 0x1003fd9e
        const int cap2 = *I * 3;                                 // 0x1003fda7-0x1003fdaf
        const int lo2 = (level2 > cap2) ? cap2 : level2;         // jg 0x1003fdc5 at 0x1003fdb8
        const int acc2 = *K * 0x3F + lo2 * 8;                    // 0x1003fdce-0x1003fddf
        // fild acc2; fdiv 64.0f at 0x1005ebfc; fstp dword [ebp-0x18]   (0x1003fde5-0x1003fdf1)
        *c3bv_loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc2)
                                                         / static_cast<double>(c3bv_f(0x0005EBFC)));
        *K = floorToInt(c3bv_loc<float>(ebp, 0x18));             // call at 0x1003fdf8, store at 0x1003fe03
        *H = *K / 8;                                             // cdq / and edx,7 / sar 3 at 0x1003fe0a-0x1003fe16
        *I = level2;                                             // 0x1003fe18-0x1003fe21

        const int level1 = *c3bv_loc<int>(ebp, 0x94);            // 0x1003fe23
        const int cap1 = *J * 3;                                 // 0x1003fe2c-0x1003fe34
        const int lo1 = (level1 > cap1) ? cap1 : level1;         // jg 0x1003fe4a at 0x1003fe3d
        const int acc1 = *L * 0x3F + lo1 * 8;                    // 0x1003fe53-0x1003fe64
        *c3bv_loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc1)
                                                         / static_cast<double>(c3bv_f(0x0005EBFC)));
        *L = floorToInt(c3bv_loc<float>(ebp, 0x18));             // call at 0x1003fe7d, store at 0x1003fe88
        *F = *L / 8;                                             // 0x1003fe8f-0x1003fe9b
        *J = level1;                                             // 0x1003fe9d-0x1003fea6
    } else {
        const int acc3 = *E * 0x3F + *c3bv_loc<int>(ebp, 0x94);  // 0x1003feaa-0x1003feb8
        *c3bv_loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc3)
                                                         / static_cast<double>(c3bv_f(0x0005EBFC)));
        *E = floorToInt(c3bv_loc<float>(ebp, 0x18));             // call at 0x1003fed1, store at 0x1003fedc
        const int acc4 = *G * 0x3F + *c3bv_loc<int>(ebp, 0x90);  // 0x1003fede-0x1003feec
        *c3bv_loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc4)
                                                         / static_cast<double>(c3bv_f(0x0005EBFC)));
        *G = floorToInt(c3bv_loc<float>(ebp, 0x18));             // call at 0x1003ff05, store at 0x1003ff10
    }

    const int product = *F * *E;                                 // imul at 0x1003ff1a
    Sqrt_t sqrtFn = reinterpret_cast<Sqrt_t>(c3bv_sound(0x00043204));
    // fild product; call sqrt; fmul qword 64.0 at 0x1005ec60; fdiv qword 3000.0 at 0x1005ec58
    const double level = sqrtFn(static_cast<double>(product)) * c3bv_d(0x0005EC60) / c3bv_d(0x0005EC58);
    *c3bv_loc<float>(ebp, 0x1C) = static_cast<float>(level);     // fst dword (no pop) at 0x1003ff43
    unsigned chosen;
    // fcomp 1.0f at 0x1005b8a8 on the UNROUNDED value; test ah,1 / jne 0x1003ff5e
    if (level < static_cast<double>(c3bv_f(0x0005B8A8)))
        chosen = 0x3F800000u;                                    // the immediate at 0x1003ff5e
    else
        chosen = *c3bv_loc<unsigned>(ebp, 0x1C);                 // 0x1003ff53
    *c3bv_loc<unsigned>(ebp, 0x18) = chosen;                     // 0x1003ff6e
    unsigned clamped;
    // fld [ebp-0x18]; fcomp 20.0f at 0x1005ec50; test ah,0x41 / je 0x1003ff8c
    if (static_cast<double>(*c3bv_loc<float>(ebp, 0x18)) > static_cast<double>(c3bv_f(0x0005EC50)))
        clamped = 0x41A00000u;                                   // the immediate at 0x1003ff8c
    else
        clamped = *c3bv_loc<unsigned>(ebp, 0x18);                // 0x1003ff81
    *reinterpret_cast<unsigned*>(A) = clamped;                   // 0x1003ff96-0x1003ff9f
    return 0;                                                    // xor eax, eax at 0x1003ffa1
}

// ---------------------------------------------------------------------------------------------------------
// speech codec: the analysis stage driver
// ---------------------------------------------------------------------------------------------------------

// 0x0003c640  codecStageDispatch (VA 0x1003c640)
// __cdecl with six stack arguments (`ret` at 0x1003d036 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0x5e0..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x178, at 0x1003c65c).
// Fourteen pointers into the encoder context are bound on entry (0x1003c68b-0x1003c750):
//   +0x10   (pa) a 0x21c-element float history, indexed one-based with the bias -0x2d4 (0x1003c7a8), so
//           element 0xb5 is its first dword and element 0x2d0 its last
//   +0x880  (qa) a second history of the same shape (0x1003c7d3)
//   +0x10f0 (sa) a history indexed with the bias -0x64 (0x1003c897)
//   +0x1bd0 (ra) a history indexed with the bias -0x394 (0x1003c835)
//   +0x20b0 a float offset cell          +0x20b4 (jumps) a ten-slot one-based index array
//   +0x20dc the jump count               +0x20e0 (w) three dwords     +0x20ec (u) six dwords
//   +0x2104 (v) six dwords               +0x211c (t) eight dwords     +0x213c (x) three dwords
//   +0x2148 (y) ten coefficients per frame, three frames deep (+0, +0x28, +0x50)
//   +0x21c0 codecFirstOrderFilter's state cell
// Arguments: [ebp+8] the input samples (moved back one dword at 0x1003c667), [ebp+0xc] the flag pair out
// (0x1003c676), [ebp+0x10] the index cell out, [ebp+0x14] the float level cell out, [ebp+0x18] the float
// coefficient array out (0x1003c685), [ebp+0x1c] the encoder context.
// The body is one pass of the analyser:
//   1. the four histories are shifted down by the frame length (0x1003c756-0x1003c8a1);
//   2. the jump index array drops every entry at or below the frame length and the rest move down by it
//      (0x1003c8a1-0x1003c93c);
//   3. the per-frame records t, u, v, w, x and y all move down one frame (0x1003c93c-0x1003cb2e);
//   4. the new samples are scaled by the float 4096.0f at 0x1005ebcc, offset by the cell at +0x20b0 and
//      appended to pa, their sum accumulating in [ebp-0x398] (0x1003cb2e-0x1003cbcb); the offset cell then
//      moves by +1.0f (0x1005b8a8) when the sum is above the frame length and by -1.0f (0x1005ebe0) when it is
//      below minus the frame length (0x1003cbcb-0x1003cc2b);
//   5. eleven analysis stages run in order (0x1003cc2b-0x1003cfa6): codecFirstOrderFilter 0x10040da0,
//      codecMarkCorrelationJumps 0x10040ab0, 0x10040740, codecSymmetricFir 0x100404d0,
//      codecCorrelationLags 0x10040330, codecPitchAnalysis 0x10040000, codecFeatureDecision 0x1003f720 once
//      per half-window, codecGainSmoother 0x1003f310, 0x1003edc0, codecRemoveMean 0x1003ed00,
//      codecRmsEnergy 0x1003ec60, codecAutocorrelation 0x1003e9f0, gaussSolveClamped 0x1003e6d0 and
//      substituteIfBelowThreshold 0x1003e600;
//   6. the four outputs are copied out of the context (0x1003cfa9-0x1003d021).
// Returns 0 (0x1003d021).
typedef int(__cdecl* CodecStageDispatch_t)(float*, int*, int*, float*, float*, void*);
CodecStageDispatch_t CodecStageDispatch_orig;
int __cdecl CodecStageDispatch_re(float* samplesIn, int* pairOut, int* indexOut, float* levelOut,
                                  float* coeffOut, void* ctx) {
    const unsigned FRAME = 0x5E0;                                 // the original's frame size
    unsigned int rawFrame[(FRAME + 2 * C3BV_PAD) / 4];
    memset(rawFrame, 0, sizeof rawFrame);
    memset(reinterpret_cast<char*>(rawFrame) + C3BV_PAD, 0xCC, FRAME);                     // rep stosd at 0x1003c65c
    char* const ebp = reinterpret_cast<char*>(rawFrame) + C3BV_PAD + FRAME;

    float* samples = samplesIn;
    if (samples != 0) samples -= 1;                              // je at 0x1003c662, sub at 0x1003c667
    int* pair = pairOut;
    if (pair != 0) pair -= 1;                                    // je at 0x1003c671, sub at 0x1003c676
    float* coeffs = coeffOut;
    if (coeffs != 0) coeffs -= 1;                                // je at 0x1003c680, sub at 0x1003c685

    char* const c = static_cast<char*>(ctx);
    // the four histories, each addressed element-wise with its own bias
    float* const pa = reinterpret_cast<float*>(c + 0x10 - 0x2D4);        // 0x1003c68e, bias at 0x1003c7a8
    float* const qa = reinterpret_cast<float*>(c + 0x880 - 0x2D4);       // 0x1003c69a, bias at 0x1003c7d3
    float* const sa = reinterpret_cast<float*>(c + 0x10F0 - 0x64);       // 0x1003c6a9, bias at 0x1003c897
    float* const ra = reinterpret_cast<float*>(c + 0x1BD0 - 0x394);      // 0x1003c6b8, bias at 0x1003c835
    float* const offsetCell = reinterpret_cast<float*>(c + 0x20B0);      // 0x1003c6c6
    int* const jumps = reinterpret_cast<int*>(c + 0x20B4 - 4);           // 0x1003c6d5, bias at 0x1003c8eb
    int* const jumpCount = reinterpret_cast<int*>(c + 0x20DC);           // 0x1003c6e4
    int* const w = reinterpret_cast<int*>(c + 0x20E0);                   // 0x1003c6f2
    int* const u = reinterpret_cast<int*>(c + 0x20EC);                   // 0x1003c701
    int* const v = reinterpret_cast<int*>(c + 0x2104);                   // 0x1003c710
    int* const t = reinterpret_cast<int*>(c + 0x211C);                   // 0x1003c71e
    int* const x = reinterpret_cast<int*>(c + 0x213C);                   // 0x1003c72d
    float* const y = reinterpret_cast<float*>(c + 0x2148);               // 0x1003c73c
    void* const filterState = c + 0x21C0;                                // 0x1003c74a

    const int frameLen = c3bv_i(0x000B58A4);
    const int order = c3bv_i(0x000B58A0);

    // The four history shifts are integer `mov` pairs in the original, so they are dword copies here.
    int n = 0x2D0 - frameLen;                                    // 0x1003c756-0x1003c761
    for (int i = 0xB5; i <= n; ++i) {                            // jg 0x1003c7e3 at 0x1003c788
        c3bv_copy32(&pa[i], &pa[frameLen + i]);                  // 0x1003c78a-0x1003c7af
        c3bv_copy32(&qa[i], &qa[frameLen + i]);                  // 0x1003c7b6-0x1003c7da
    }
    n = 0x21C - frameLen;                                        // 0x1003c7e3-0x1003c7ee
    for (int i = 0xE5; i <= n; ++i)                              // jg 0x1003c845 at 0x1003c815
        c3bv_copy32(&ra[i], &ra[frameLen + i]);                  // 0x1003c817-0x1003c83c
    n = 0x2D0 - frameLen;                                        // 0x1003c845-0x1003c850
    for (int i = 0x19; i <= n; ++i)                              // jg 0x1003c8a1 at 0x1003c877
        c3bv_copy32(&sa[i], &sa[frameLen + i]);                  // 0x1003c879-0x1003c89b

    int kept = 1;                                                // 0x1003c8a1
    n = *jumpCount - 1;                                          // 0x1003c8ab-0x1003c8b6
    for (int i = 1; i <= n; ++i) {                               // jg 0x1003c92e at 0x1003c8dd
        if (jumps[i] > frameLen) {                               // jle 0x1003c92c at 0x1003c8f5
            jumps[kept] = jumps[i] - frameLen;                   // 0x1003c8f7-0x1003c919
            kept += 1;                                           // 0x1003c91d-0x1003c926
        }
    }
    *jumpCount = kept;                                           // 0x1003c92e-0x1003c93a

    t[0] = t[2];                                                 // 0x1003c93c-0x1003c94b
    t[1] = t[3];                                                 // 0x1003c94d-0x1003c95c
    for (int j = 1; j <= 2; ++j) {                               // jg 0x1003cb2e at 0x1003c981
        u[2 * j - 2] = u[2 * j] - frameLen;                      // 0x1003c987-0x1003c9af
        u[2 * j - 1] = u[2 * j + 1] - frameLen;                  // 0x1003c9b3-0x1003c9db
        v[2 * j - 2] = v[2 * j] - frameLen;                      // 0x1003c9df-0x1003ca07
        v[2 * j - 1] = v[2 * j + 1] - frameLen;                  // 0x1003ca0b-0x1003ca33
        w[j - 1] = w[j];                                         // 0x1003ca37-0x1003ca52
        t[2 * j] = t[2 * j + 2];                                 // 0x1003ca56-0x1003ca77
        t[2 * j + 1] = t[2 * j + 3];                             // 0x1003ca7a-0x1003ca9c
        x[j - 1] = x[j];                                         // 0x1003caa0-0x1003cabb
        for (int m = 1; m <= order; ++m)                         // jg 0x1003cb29 at 0x1003caec
            // y is addressed with the bias -0x2c, so slot m of frame j is y[m + j*10 - 11]
            c3bv_copy32(&y[m + j * 10 - 11], &y[m + (j + 1) * 10 - 11]);   // 0x1003caee-0x1003cb23
    }

    float sum = 0.0f;                                            // 0x1003cb2e
    const float gain = c3bv_f(0x0005EBCC);                       // the float 4096.0f at 0x1005ebcc
    for (int i = 1; i <= frameLen; ++i) {                        // jg 0x1003cbcb at 0x1003cb65
        // fld samples[i]; fmul 4096.0f; fsub [offsetCell]; fstp dword pa[0x2d0-frameLen+i]
        pa[0x2D0 - frameLen + i] = static_cast<float>(static_cast<double>(samples[i])
                                                      * static_cast<double>(gain)
                                                      - static_cast<double>(*offsetCell));
        // fld [ebp-0x398]; fadd pa[...]; fstp dword [ebp-0x398]   (0x1003cbb6-0x1003cbc3)
        sum = static_cast<float>(static_cast<double>(sum)
                                 + static_cast<double>(pa[0x2D0 - frameLen + i]));
    }
    // fild frameLen; fcomp sum; test ah,1 / je 0x1003cbf4
    if (static_cast<double>(frameLen) < static_cast<double>(sum))
        *offsetCell = static_cast<float>(static_cast<double>(*offsetCell)
                                         + static_cast<double>(c3bv_f(0x0005B8A8)));   // 0x1003cbde-0x1003cbf2
    // fild -frameLen; fcomp sum; test ah,0x41 / jne 0x1003cc2b
    if (!(static_cast<double>(-frameLen) <= static_cast<double>(sum)))
        *offsetCell = static_cast<float>(static_cast<double>(*offsetCell)
                                         + static_cast<double>(c3bv_f(0x0005EBE0)));   // 0x1003cc15-0x1003cc29

    const int head = 0x2D1 - frameLen;                           // 0x1003cc2b-0x1003cc36
    reinterpret_cast<Cdecl5_t>(c3bv_sound(0x00040DA0))(          // codecFirstOrderFilter, call at 0x1003cc75
        &pa[head], &qa[head], c3bv_sound(0x000B58A4), c3bv_sound(0x00064A14), filterState);

    reinterpret_cast<Cdecl8_t>(c3bv_sound(0x00040AB0))(          // codecMarkCorrelationJumps, call at 0x1003ccaa
        c + 0x880, c + 0x20B4, jumpCount, c3bv_sound(0x000648E8), c3bv_sound(0x000648EC),
        c3bv_sound(0x000648F0), c3bv_sound(0x000B58A4), ctx);

    reinterpret_cast<Cdecl11_t>(c3bv_sound(0x00040740))(         // call at 0x1003ccf4
        c + 0x20B4, jumpCount, c3bv_sound(0x000648E8), &w[2], u, c3bv_sound(0x000648F4),
        c3bv_sound(0x000B58A4), c3bv_sound(0x000648F8), c3bv_sound(0x000648FC), c3bv_sound(0x00064900),
        c3bv_sound(0x00064904));

    reinterpret_cast<Cdecl4_t>(c3bv_sound(0x000404D0))(          // codecSymmetricFir, call at 0x1003cd1f
        c + 0x10 + 0x390, c + 0x10F0 + 0x600, c3bv_sound(0x00064908), c3bv_sound(0x000B58A4));

    reinterpret_cast<Cdecl5_t>(c3bv_sound(0x00040330))(          // codecCorrelationLags, call at 0x1003cd4b
        c + 0x10F0 + 0x330, c + 0x1BD0, c3bv_sound(0x00064908), c3bv_sound(0x000B58A4),
        c3bv_loc<void>(ebp, 0x394));

    reinterpret_cast<Cdecl8_t>(c3bv_sound(0x00040000))(          // codecPitchAnalysis, call at 0x1003cd85
        c + 0x1BD0, c3bv_sound(0x000648FC), c3bv_sound(0x00064914), c3bv_sound(0x0006490C),
        c3bv_loc<void>(ebp, 0xF4), c3bv_loc<void>(ebp, 0x3E0), c3bv_loc<void>(ebp, 0x3E4),
        c3bv_loc<void>(ebp, 0x3D8));

    Cdecl13_t featureDecision = reinterpret_cast<Cdecl13_t>(c3bv_sound(0x0003F720));
    // The half counter IS slot 0 of the block [ebp-0xf8] whose slot 1 [ebp-0xf4] is the score array
    // codecPitchAnalysis filled, so the counter overwrites that slot; `&[ebp-0xf8]` is also what the stage is
    // handed as its half-window cell (0x1003cdf2).
    for (*c3bv_loc<int>(ebp, 0xF8) = 1;                          // 0x1003cd8d
         *c3bv_loc<int>(ebp, 0xF8) <= 2;                         // jg 0x1003ce23 at 0x1003cdaf
         *c3bv_loc<int>(ebp, 0xF8) += 1) {                       // 0x1003cd99-0x1003cda2
        featureDecision(&u[4],                                   // 0x1003ce12
                        c + 0x10, c + 0x10F0, c3bv_sound(0x00064A04),
                        c3bv_loc<void>(ebp, 0xF8),
                        // the two scores codecPitchAnalysis indexed inside the same block (0x1003cddc,
                        // 0x1003cdea)
                        ebp - 0xF8 + *c3bv_loc<int>(ebp, 0x3E0) * 4,
                        ebp - 0xF8 + *c3bv_loc<int>(ebp, 0x3E4) * 4,
                        c3bv_loc<void>(ebp, 0x3D8), c3bv_loc<void>(ebp, 0x394), w, t,
                        c3bv_sound(0x000648F4), ctx);
    }

    reinterpret_cast<Cdecl7_t>(c3bv_sound(0x0003F310))(          // codecGainSmoother, call at 0x1003ce4f
        c3bv_loc<void>(ebp, 0xF4), c3bv_sound(0x0006490C), c3bv_loc<void>(ebp, 0x3E0), &t[7],
        indexOut, c3bv_loc<void>(ebp, 0x374), ctx);

    // the chosen index selects one dword of the table at 0x10064910 (0x1003ce57-0x1003ce64)
    *c3bv_loc<int>(ebp, 0x3CC) =
        reinterpret_cast<const int*>(c3bv_sound(0x00064910))[*c3bv_loc<int>(ebp, 0x374)];

    reinterpret_cast<Cdecl9_t>(c3bv_sound(0x0003EDC0))(          // call at 0x1003cea6
        c3bv_loc<void>(ebp, 0x3CC), t, &w[2], c3bv_sound(0x000648F4), u, v,
        c3bv_loc<void>(ebp, 0x38C), c3bv_sound(0x000B58A4), c3bv_sound(0x000648FC));

    *c3bv_loc<int>(ebp, 0x3AC) = v[5] + 1 - v[4];                // 0x1003ceae-0x1003cec3

    reinterpret_cast<Cdecl3_t>(c3bv_sound(0x0003ED00))(          // codecRemoveMean, call at 0x1003ceee
        c3bv_loc<void>(ebp, 0x3AC), &qa[v[4]], c3bv_loc<void>(ebp, 0x368));

    *c3bv_loc<int>(ebp, 0x04) = *c3bv_loc<int>(ebp, 0x378) - *c3bv_loc<int>(ebp, 0x37C) + 1;  // 0x1003cef6-

    reinterpret_cast<Cdecl3_t>(c3bv_sound(0x0003EC60))(          // codecRmsEnergy, call at 0x1003cf2d
        c3bv_loc<void>(ebp, 0x04),
        ebp - 0x368 + (*c3bv_loc<int>(ebp, 0x37C) - v[4]) * 4,   // 0x1003cf12-0x1003cf21
        &x[2]);

    reinterpret_cast<Cdecl6_t>(c3bv_sound(0x0003E9F0))(          // codecAutocorrelation, call at 0x1003cf5b
        c3bv_sound(0x000B58A0), c3bv_sound(0x00064910), c3bv_loc<void>(ebp, 0x3AC),
        c3bv_loc<void>(ebp, 0x368), c3bv_loc<void>(ebp, 0x574), c3bv_loc<void>(ebp, 0x59C));

    reinterpret_cast<Cdecl4_t>(c3bv_sound(0x0003E6D0))(          // gaussSolveClamped, call at 0x1003cf80
        c3bv_sound(0x000B58A0), c3bv_loc<void>(ebp, 0x574), c3bv_loc<void>(ebp, 0x59C),
        reinterpret_cast<char*>(y) + 0x50);

    reinterpret_cast<Cdecl3_t>(c3bv_sound(0x0003E600))(          // substituteIfBelowThreshold, call at 0x1003cfa1
        c3bv_sound(0x000B58A0), reinterpret_cast<char*>(y) + 0x28, reinterpret_cast<char*>(y) + 0x50);

    pair[1] = t[2];                                              // 0x1003cfa9-0x1003cfb5
    pair[2] = t[3];                                              // 0x1003cfb8-0x1003cfc4
    c3bv_copy32(levelOut, x);                                    // 0x1003cfc7-0x1003cfd2
    n = c3bv_i(0x000B58A0);                                      // 0x1003cfd4
    for (int i = 1; i <= n; ++i)                                 // jg 0x1003d021 at 0x1003d001
        c3bv_copy32(&coeffs[i], &y[i - 1]);                      // 0x1003d003-0x1003d01c
    return 0;                                                    // xor eax, eax at 0x1003d021
}

SG_HOOK("sound.dll", 0x0003a980, sound_codecProcessBlock, CodecProcessBlock_re, CodecProcessBlock_orig);
SG_HOOK("sound.dll", 0x0003b150, sound_codecProcessResource, CodecProcessResource_re,
        CodecProcessResource_orig);
SG_HOOK("sound.dll", 0x0003f720, sound_codecFeatureDecision, CodecFeatureDecision_re,
        CodecFeatureDecision_orig);
SG_HOOK("sound.dll", 0x0003c640, sound_codecStageDispatch, CodecStageDispatch_re, CodecStageDispatch_orig);

}  // namespace
