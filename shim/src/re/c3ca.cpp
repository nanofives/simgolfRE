// C3 batch c3ca (2026-10-09) of sound.dll: the speech codec's last two unnamed analysis stages, the two
// codec context initialisers, and five small voice / track / channel / DirectSound-buffer setters.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside
// each body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32 or COM call is made, nothing is allocated or freed, and no live sound object,
// device, list or buffer is touched: every record, cell, array and codec context the A/B passes in is
// allocated by the fixture, and the two vtable slots the DirectSound and sequencer forwarders call belong to
// synthetic objects the fixture builds.
// Two bodies forward to an original callee at its own address (codecAlignAnalysisWindow -> floatFloorToInt
// 0x1003e400, codecInitContextA/B -> codecSetParams 0x1003aa50); sound.dll is relocatable, so those
// addresses are resolved against the loaded module base at the first call.
// Floating point: every x87 step of these bodies ends in a qword slot, a dword slot or a register. The A/B
// thread runs with control word 0x027f (53-bit, round to nearest, measured by c3ap), so each step that ends
// in a register or a qword slot is exactly a double operation and each `fstp dword` is exactly a double
// operation rounded once to float; both are written that way (the c3ap method).
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fld(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

// sound.dll has base relocations, so an original callee or constant is addressed through the loaded base.
char* c3ca_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

typedef int(__cdecl* FloatFloorToInt_t)(const float*);
typedef void(__cdecl* Void0_t)();

// ---------------------------------------------------------------------------------------------------------
// speech codec: the two analysis stages of codecStageDispatch that had no name
// ---------------------------------------------------------------------------------------------------------
//
// Both are called by codecStageDispatch 0x1003c640 over the encoder context, and both work on the same two
// kinds of record:
//   * a FLAG record, dwords read in pairs, two per frame (codecStageDispatch's +0x211c record);
//   * a BOUNDS record, dwords read and written in pairs (lo, hi), two per frame (+0x20ec, +0x2104 and a
//     block of codecStageDispatch's own frame).
// Both of them move their record pointers back on entry (three dwords for a bounds record, one for the flag
// record) and then index with `2 * *pn`, which is the codec's one-based convention: with that pair of steps
// `base[2*n + 1]` and `base[2*n + 2]` of the moved pointer are the (lo, hi) pair of frame n - 1 of the
// record as the caller sees it, `base[2*n - 1]` of a bounds record is frame n - 2's lo, and
// `flags[2*n - 2] .. flags[2*n + 2]` of the moved flag record are the five flags of frames n - 2, n - 1 and
// n. At the live call sites *pn is the constant 3 at 0x100648f4, the frame length is the module global
// 0x100b58a4 (180, written only by codecSetParams 0x1003aa50 at 0x1003aa72) and the window length is the
// constant 156 at 0x100648fc; the fixtures also run *pn = 2 and 4 so that the index arithmetic above is
// exercised and not just one fixed set of offsets.

// 0x0003edc0  codecAlignAnalysisWindow (VA 0x1003edc0)
// __cdecl with nine stack arguments, all pointers to int (`add esp, 0x80` + `ret` at 0x1003f2f5-0x1003f305
// pops nothing), a /Od /GZ debug body whose frame [ebp-0x80..ebp-1] is filled with 0xcccccccc (`rep stosd`,
// ecx = 0x20, at 0x1003edd9). The frame is not modelled: every write of the body goes to one of its
// arguments, and its only callee (floatFloorToInt 0x1003e400) reads the float cell it is handed and writes
// nothing.
//   [ebp+8]    the alignment step; at the live call site it is one dword of the table 0x10064910 selected by
//              the gain index at 0x1003ce57 (that table is 1, 20, 21, 22, ... 34)
//   [ebp+0xc]  the flag record, moved back one dword at 0x1003edf9
//   [ebp+0x10] a mode code, compared against 0, 1, 2 and 3 (0x1003ef3d, 0x1003f01f, 0x1003f087, 0x1003f08f,
//              0x1003f26c); the live caller passes the cell the other stage below writes (+0x20e8)
//   [ebp+0x14] the frame index n
//   [ebp+0x18] the previous bounds record, moved back three dwords at 0x1003edf0 (live: +0x20ec)
//   [ebp+0x1c] the bounds record this body writes, moved back three dwords at 0x1003ede7 (live: +0x2104)
//   [ebp+0x20] the second bounds record this body writes, moved back three dwords at 0x1003edde (live: a
//              block of codecStageDispatch's own frame, read back at 0x1003cef6-0x1003cf02)
//   [ebp+0x24] the frame length
//   [ebp+0x28] the window length
// The body computes the limits lo = (n - 2) * frameLen + 1 (0x1003ee02-0x1003ee10) and hi = n * frameLen
// (0x1003ee13-0x1003ee1e), reduces the five flags to `allSet` (every one of them equal to 1; the chain of
// four `je`/`jne` pairs at 0x1003ee3a-0x1003eeef) and `anySet` (either flag of frame n equal to 1,
// 0x1003eef2-0x1003ef27), and then takes one of two paths (0x1003ef2a, 0x1003ef34, 0x1003ef40):
//   * align: the window start is pulled onto the lattice `cur[2n-1] + k * step` at or above lo
//     (0x1003ef46-0x1003ef8a), then moved by floatFloorToInt((mid - start) / step) steps toward the midpoint
//     mid = (prev.lo + prev.hi + 1 - winLen) / 2 (0x1003ef95-0x1003efe9), and the pair (start, start +
//     winLen - 1) is written (0x1003efeb-0x1003f018). One step is then subtracted when the mode is 2 or more
//     and the end is past prev.hi (0x1003f01f-0x1003f080), one step is added when the mode is 1 or 3 and the
//     start is before prev.lo (0x1003f084-0x1003f0f0), and two `while` loops slide the pair down until the
//     end is at or below hi (0x1003f0f4-0x1003f149) and up until the start is at or above lo
//     (0x1003f14b-0x1003f1a0). `done` is set to 1 (0x1003f1a2).
//   * copy: the pair is copied from the previous record unchanged and `done` is 0
//     (0x1003f1ab-0x1003f1e3).
// Finally the second record gets a sub-window whose length is the largest multiple of the step that fits in
// the pair just written (0x1003f1ea-0x1003f217): the previous record's pair when that length is 0 or no flag
// of frame n is set (0x1003f226-0x1003f25e), the last `length` samples of the pair when the copy path ran
// and the mode is 2 (0x1003f263-0x1003f2af), and otherwise the first `length` samples of it
// (0x1003f2b1-0x1003f2ec). Returns 0 (0x1003f2f0).
// A step of 0 faults on the `idiv` at 0x1003ef66 and a step below 1 does not terminate the two `while`
// loops, so the fixtures only pass values of the table 0x10064910.
typedef int(__cdecl* CodecAlignAnalysisWindow_t)(int*, int*, int*, int*, int*, int*, int*, int*, int*);
CodecAlignAnalysisWindow_t CodecAlignAnalysisWindow_orig;
int __cdecl CodecAlignAnalysisWindow_re(int* step, int* flags, int* mode, int* pn, int* prev, int* cur,
                                        int* out, int* frameLen, int* winLen) {
    out -= 3;                                                    // sub 0xc at 0x1003edde
    cur -= 3;                                                    // sub 0xc at 0x1003ede7
    prev -= 3;                                                   // sub 0xc at 0x1003edf0
    flags -= 1;                                                  // sub 4 at 0x1003edf9

    const int n = *pn;                                           // re-read by the original at every use
    const int loLimit = (n - 2) * *frameLen + 1;                 // 0x1003ee02-0x1003ee10
    const int hiLimit = n * *frameLen;                           // 0x1003ee13-0x1003ee1e

    int allSet = (flags[2 * n - 2] == 1);                        // sete dl at 0x1003ee34
    if (allSet) allSet = (flags[2 * n - 1] == 1);                // 0x1003ee3a-0x1003ee66
    if (allSet) allSet = (flags[2 * n] == 1);                    // 0x1003ee69-0x1003ee95
    if (allSet) allSet = (flags[2 * n + 1] == 1);                // 0x1003ee98-0x1003eec2
    if (allSet) allSet = (flags[2 * n + 2] == 1);                // 0x1003eec5-0x1003eeef
    const int anySet = (flags[2 * n + 1] == 1 || flags[2 * n + 2] == 1);   // 0x1003eef2-0x1003ef27

    int done;                                                    // [ebp-0x24]
    if (allSet || (anySet && *mode == 0)) {                      // 0x1003ef2a, 0x1003ef34, 0x1003ef40
        const int s = *step;
        // the smallest point of the lattice `cur[2n-1] + k * step` that is at or above loLimit
        int start = (loLimit + s - 1 - cur[2 * n - 1]) / s;      // cdq + idiv at 0x1003ef65, 0x1003ef66
        start = start * s;                                       // imul at 0x1003ef71
        start += cur[2 * n - 1];                                 // 0x1003ef86
        const int len = *winLen;                                 // 0x1003ef90
        // (prev.lo + prev.hi + 1 - len) / 2, divided toward zero by `cdq` + `sub` + `sar` at
        // 0x1003efb8-0x1003efbb
        const int mid = (prev[2 * n + 1] + prev[2 * n + 2] + 1 - len) / 2;
        // fild dword + fidiv dword + fstp dword at 0x1003efc9-0x1003efd1
        const float q = static_cast<float>(static_cast<double>(mid - start) / static_cast<double>(s));
        start += reinterpret_cast<FloatFloorToInt_t>(c3ca_sound(0x0003E400))(&q) * s;   // 0x1003efd8-0x1003efe9
        cur[2 * n + 1] = start;                                  // 0x1003eff5
        cur[2 * n + 2] = cur[2 * n + 1] + len - 1;               // 0x1003f00a-0x1003f018
        if (*mode >= 2 && cur[2 * n + 2] > prev[2 * n + 2]) {    // jl at 0x1003f022, jle at 0x1003f040
            cur[2 * n + 1] -= *step;                             // 0x1003f053-0x1003f05f
            cur[2 * n + 2] -= *step;                             // 0x1003f074-0x1003f080
        }
        if ((*mode == 1 || *mode == 3)                           // je at 0x1003f08a, jne at 0x1003f092
            && cur[2 * n + 1] < prev[2 * n + 1]) {               // jge at 0x1003f0b0
            cur[2 * n + 1] += *step;                             // 0x1003f0c3-0x1003f0cf
            cur[2 * n + 2] += *step;                             // 0x1003f0e4-0x1003f0f0
        }
        while (cur[2 * n + 2] > hiLimit) {                       // jle at 0x1003f105
            cur[2 * n + 1] -= *step;                             // 0x1003f118-0x1003f124
            cur[2 * n + 2] -= *step;                             // 0x1003f139-0x1003f145
        }
        while (cur[2 * n + 1] < loLimit) {                       // jge at 0x1003f15c
            cur[2 * n + 1] += *step;                             // 0x1003f16f-0x1003f17b
            cur[2 * n + 2] += *step;                             // 0x1003f190-0x1003f19c
        }
        done = 1;                                                // 0x1003f1a2
    } else {
        cur[2 * n + 1] = prev[2 * n + 1];                        // 0x1003f1bf-0x1003f1c3
        cur[2 * n + 2] = prev[2 * n + 2];                        // 0x1003f1db-0x1003f1df
        done = 0;                                                // 0x1003f1e3
    }

    // the largest multiple of the step that fits in the pair just written
    const int sub = ((cur[2 * n + 2] - cur[2 * n + 1] + 1) / *step) * *step;   // 0x1003f1fe-0x1003f217
    if (sub == 0 || anySet == 0) {                               // je at 0x1003f21e, jne at 0x1003f224
        out[2 * n + 1] = prev[2 * n + 1];                        // 0x1003f23a-0x1003f23e
        out[2 * n + 2] = prev[2 * n + 2];                        // 0x1003f256-0x1003f25a
    } else if (done == 0 && *mode == 2) {                        // jne at 0x1003f267, jne at 0x1003f26f
        out[2 * n + 1] = cur[2 * n + 2] - sub + 1;               // 0x1003f27f-0x1003f28f
        out[2 * n + 2] = cur[2 * n + 2];                         // 0x1003f2a7-0x1003f2ab
    } else {
        out[2 * n + 1] = cur[2 * n + 1];                         // 0x1003f2c5-0x1003f2c9
        out[2 * n + 2] = cur[2 * n + 1] + sub - 1;               // 0x1003f2de-0x1003f2ec
    }
    return 0;                                                    // xor eax, eax at 0x1003f2f0
}

// 0x00040740  codecSelectWindowFromJumps (VA 0x10040740)
// __cdecl with eleven stack arguments, all pointers to int (`mov esp, ebp` + `ret` at 0x1004079f-0x10040aa2
// pops nothing), a /Od /GZ debug body whose frame [ebp-0x74..ebp-1] is filled with 0xcccccccc (`rep stosd`,
// ecx = 0x1d, at 0x10040756). It has no callee (and no `__chkesp` epilogue). The frame is not modelled:
// every write goes to one of its arguments.
//   [ebp+8]    the jump index array, moved back one dword at 0x1004075b (live: the ten one-based slots at
//              +0x20b4 that codecMarkCorrelationJumps 0x10040ab0 appends to)
//   [ebp+0xc]  the number of slots; the scan starts at *count - 1 (0x100407c3)
//   [ebp+0x10] NEVER READ: there is no `[ebp+0x10]` anywhere in the body. The caller passes 0x100648e8 (10).
//   [ebp+0x14] the mode cell this body writes, 0, 1, 2 or 3 (0x10040873, 0x100409b4, 0x10040a46, 0x10040a94);
//              live it is +0x20e8, which codecAlignAnalysisWindow then reads as its mode argument
//   [ebp+0x18] the bounds record this body writes, moved back three dwords at 0x10040764 (live: +0x20ec)
//   [ebp+0x1c] the frame index n
//   [ebp+0x20] the frame length
//   [ebp+0x24] a minimum separation (live: 90 at 0x100648f8)
//   [ebp+0x28] the window length (live: 156 at 0x100648fc)
//   [ebp+0x2c] a lowest start (live: 307 at 0x10064900)
//   [ebp+0x30] NEVER READ: there is no `[ebp+0x30]` anywhere in the body. The caller passes 0x10064904 (462).
// The body searches the jump array for the indices that fall in the window [lo, hi], where
// hi = n * frameLen (0x100407b6-0x100407bb) and lo = max(bounds[2n] + 1, (n - 2) * frameLen + 1)
// (0x1004076a-0x100407ad), by scanning down from *count - 1 while the entry is above hi
// (0x100407c9-0x100407ea) and then down again while the entry is at or above lo (0x10040887-0x100408a8).
// Four outcomes, each with its own mode code:
//   mode 0, no index in the window (`jle 0x1004080a` at 0x100407f9 or `jge 0x1004087e` at 0x10040808 not
//     taken): the pair becomes (max(bounds[2n] + 1, minStart), that + winLen - 1) (0x1004080a-0x10040873);
//   mode 2, exactly one usable index (no later index of the run is at least minGap past the first,
//     0x100408bc-0x10040903) and that index is past max((n - 1) * frameLen, lo + minGap - 1)
//     (0x10040923-0x1004094f): the pair ends one sample before it and starts at max(lo, end - winLen + 1)
//     (0x10040951-0x100409b4);
//   mode 3, a later index of the run is at least minGap past the start and at most winLen past it: the pair
//     is (first index, that later index - 1) (0x100409bf-0x10040a46);
//   mode 1, otherwise: the pair is (first index, min(first index + winLen - 1, hi))
//     (0x10040a50-0x10040a94).
// Returns 0 on both exits (0x10040a4c, 0x10040a9a).
// The run is the index range [j, i-1]: the gap loop at 0x100408d0 is bounded by the slot [ebp-4], which
// holds i - 1 (0x100408b9), and the mode-1/mode-3 loop at 0x100409e2 is bounded by the slot [ebp-0x18],
// which still holds i. [ebp-4] is reused for (n - 1) * frameLen at 0x10040911 after the gap loop has
// finished, and that third value is only the first half of the mode-2 bar; the last loop does not use it.
typedef int(__cdecl* CodecSelectWindowFromJumps_t)(int*, int*, int*, int*, int*, int*, int*, int*, int*,
                                                   int*, int*);
CodecSelectWindowFromJumps_t CodecSelectWindowFromJumps_orig;
int __cdecl CodecSelectWindowFromJumps_re(int* jumps, int* count, int* /*unused3*/, int* outMode,
                                          int* bounds, int* pn, int* frameLen, int* minGap, int* winLen,
                                          int* minStart, int* /*unused11*/) {
    jumps -= 1;                                                  // sub 4 at 0x1004075b
    bounds -= 3;                                                 // sub 0xc at 0x10040764

    const int n = *pn;                                           // re-read by the original at every use
    int start = bounds[2 * n] + 1;                               // 0x10040776-0x1004077d
    const int floorLimit = (n - 2) * *frameLen + 1;              // 0x10040785-0x10040791
    const int lo = (start >= floorLimit) ? start : floorLimit;   // jl at 0x1004079a
    const int hi = n * *frameLen;                                // 0x100407b6-0x100407bb

    int i = *count - 1;                                          // 0x100407c1-0x100407c6
    while (i >= 1 && jumps[i] > hi) --i;                         // jl at 0x100407d8, jg at 0x100407e6
    ++i;                                                         // 0x100407ec-0x100407f2

    if (i <= 1 || jumps[i - 1] < lo) {                           // jle at 0x100407f9, jge at 0x10040808
        start = bounds[2 * n] + 1;                               // 0x10040816-0x1004081d
        bounds[2 * n + 1] = (start >= *minStart) ? start : *minStart;   // jl at 0x10040828
        bounds[2 * n + 2] = bounds[2 * n + 1] + *winLen - 1;     // 0x10040855-0x1004086c
        *outMode = 0;                                            // 0x10040873
        return 0;                                                // xor eax, eax at 0x10040a9a
    }

    int j = i - 1;                                               // 0x1004087e-0x10040884
    while (j >= 1 && jumps[j] >= lo) --j;                        // jl at 0x10040896, jge at 0x100408a4
    ++j;                                                         // 0x100408aa-0x100408b0

    const int runEnd = i - 1;                                    // [ebp-4] at 0x100408b3
    int wide = 0;                                                // [ebp-0xc]
    for (int k = j + 1; k <= runEnd; ++k) {                      // jg at 0x100408d6
        if (jumps[k] - jumps[j] >= *minGap) { wide = 1; break; } // jl at 0x100408ef, 0x100408f1
    }

    // [ebp-4] is reused from here on; the loop below is bounded by i ([ebp-0x18]), not by this value.
    const int frameBar = (n - 1) * *frameLen;                    // 0x10040908-0x10040911
    const int gapBar = lo + *minGap - 1;                         // 0x10040917-0x10040920
    if (!wide) {                                                 // jne at 0x10040927
        const int bar = (frameBar >= gapBar) ? frameBar : gapBar;   // jl at 0x10040933
        if (jumps[j] > bar) {                                    // jle at 0x1004094f
            bounds[2 * n + 2] = jumps[j] - 1;                    // 0x1004095a-0x10040967
            const int back = bounds[2 * n + 2] - *winLen + 1;    // 0x1004097e-0x10040987
            bounds[2 * n + 1] = (lo >= back) ? lo : back;        // jl at 0x10040990
            *outMode = 2;                                        // 0x100409b4
            return 0;                                            // xor eax, eax at 0x10040a9a
        }
    }

    bounds[2 * n + 1] = jumps[j];                                // 0x100409cf-0x100409d2
    for (;;) {
        ++j;                                                     // 0x100409d6-0x100409dc
        if (j >= i) break;                                       // cmp [ebp-0x18], jl at 0x100409e5
        if (jumps[j] > bounds[2 * n + 1] + *winLen) break;       // jle at 0x10040a05
        if (jumps[j] >= bounds[2 * n + 1] + *minGap) {           // jge at 0x10040a25
            bounds[2 * n + 2] = jumps[j] - 1;                    // 0x10040a32-0x10040a3f
            *outMode = 3;                                        // 0x10040a46
            return 0;                                            // xor eax, eax at 0x10040a4c
        }
    }
    const int end = bounds[2 * n + 1] + *winLen - 1;             // 0x10040a5e-0x10040a67
    bounds[2 * n + 2] = (end <= hi) ? end : hi;                  // jg at 0x10040a70
    *outMode = 1;                                                // 0x10040a94
    return 0;                                                    // xor eax, eax at 0x10040a9a
}

// ---------------------------------------------------------------------------------------------------------
// speech codec: the two context initialisers
// ---------------------------------------------------------------------------------------------------------

// 0x0003aae0  codecInitContextA (VA 0x1003aae0)
// __cdecl with one stack argument (`add esp, 0x48` + `ret` at 0x1003ae8e-0x1003ae9b pops nothing), a /Od /GZ
// debug body whose frame [ebp-0x48..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x12, at
// 0x1003aaf6) and whose only live local is the loop counter [ebp-4] (and [ebp-8], the integer the one `fild`
// converts). eax at the `ret` holds the argument (0x1003ae64), not a return value, so the entry declares
// ret="void" and the evidence is the context.
// It calls codecSetParams 0x1003aa50 (0x1003aaf8), which is the only writer in the whole DLL of the three
// codec globals and stores the constants 10, 0xb4 and 1 into them (0x1003aa68, 0x1003aa72, 0x1003aa7c), and
// then writes the 0x2544-byte encoder context: the four sample histories and the five counted arrays are
// zeroed (0x1003ab36, 0x1003ab72, 0x1003aba0, 0x1003abe8, 0x1003ac47, 0x1003ac72, 0x1003ac9d, 0x1003acef,
// 0x1003ad68, 0x1003ae20, 0x1003ae4b) and the seeded fields take the values below, each with the address of
// its store. The smoothed level +0x2228 is `(float)((*+0x2250 / *+0x2254) << 6)` read back from the two
// fields this body has just stored (`cdq` + `idiv` + `shl` + `fild` + `fstp dword` at 0x1003adef-0x1003ae08),
// so it is 3000 / 187 = 16 shifted by 6 = 1024.0f.
// Five regions of the context are NOT written by this body: +0x20b4..+0x20db, +0x20ec..+0x20fb,
// +0x2104..+0x2113, +0x21cc and +0x221c. The fixtures seed them differently per vector, which is what makes
// the final states of the fourteen vectors distinguishable.
typedef void(__cdecl* CodecInitContextA_t)(void*);
CodecInitContextA_t CodecInitContextA_orig;
void __cdecl CodecInitContextA_re(void* ctx) {
    reinterpret_cast<Void0_t>(c3ca_sound(0x0003AA50))();         // call at 0x1003aaf8
    int i;
    fld<int>(ctx, 0x0000) = 0;                                   // 0x1003ab00
    fld<int>(ctx, 0x0004) = 0;                                   // 0x1003ab09
    fld<int>(ctx, 0x0008) = 0;                                   // 0x1003ab13
    fld<int>(ctx, 0x000c) = 0;                                   // 0x1003ab1d
    for (i = 0; i < 0x21c; ++i) {                                // jge at 0x1003ab3d
        fld<int>(ctx, 0x0010 + i * 4) = 0;                       // 0x1003ab45
        fld<int>(ctx, 0x0880 + i * 4) = 0;                       // 0x1003ab53
    }
    for (i = 0; i < 0x2b8; ++i)                                  // jge at 0x1003ab79
        fld<int>(ctx, 0x10f0 + i * 4) = 0;                       // 0x1003ab81
    for (i = 0; i < 0x138; ++i)                                  // jge at 0x1003aba7
        fld<int>(ctx, 0x1bd0 + i * 4) = 0;                       // 0x1003abaf
    fld<int>(ctx, 0x20b0) = 0;                                   // 0x1003abbf
    fld<int>(ctx, 0x20dc) = 1;                                   // 0x1003abcc
    for (i = 0; i < 3; ++i)                                      // jge at 0x1003abec
        fld<int>(ctx, 0x20e0 + i * 4) = 0;                       // 0x1003abf4
    fld<int>(ctx, 0x20fc) = 0x133;                               // 0x1003ac04
    fld<int>(ctx, 0x2100) = 0x1ce;                               // 0x1003ac11
    fld<int>(ctx, 0x2114) = 0x133;                               // 0x1003ac1e
    fld<int>(ctx, 0x2118) = 0x1ce;                               // 0x1003ac2b
    for (i = 0; i < 8; ++i)                                      // jge at 0x1003ac4b
        fld<int>(ctx, 0x211c + i * 4) = 0;                       // 0x1003ac53
    for (i = 0; i < 3; ++i)                                      // jge at 0x1003ac76
        fld<int>(ctx, 0x213c + i * 4) = 0;                       // 0x1003ac7e
    for (i = 0; i < 0x1e; ++i)                                   // jge at 0x1003aca1
        fld<int>(ctx, 0x2148 + i * 4) = 0;                       // 0x1003aca9
    fld<int>(ctx, 0x21c0) = 0;                                   // 0x1003acb9
    fld<int>(ctx, 0x21c4) = 0;                                   // 0x1003acc6
    fld<int>(ctx, 0x21c8) = 0x3f800000;                          // 1.0f, 0x1003acd3
    for (i = 0; i < 0x10; ++i)                                   // jge at 0x1003acf3
        fld<int>(ctx, 0x21d0 + i * 4) = 0;                       // 0x1003acfb
    fld<int>(ctx, 0x2210) = 0;                                   // 0x1003ad0b
    fld<int>(ctx, 0x2214) = 1;                                   // 0x1003ad18
    fld<int>(ctx, 0x2218) = 9;                                   // 0x1003ad25
    fld<int>(ctx, 0x2220) = 0;                                   // 0x1003ad32
    fld<int>(ctx, 0x2224) = 0x41a00000;                          // 20.0f, 0x1003ad3f
    fld<int>(ctx, 0x222c) = 0;                                   // 0x1003ad4c
    for (i = 0; i < 6; ++i)                                      // jge at 0x1003ad6c
        fld<int>(ctx, 0x2230 + i * 4) = 0;                       // 0x1003ad74
    fld<int>(ctx, 0x2248) = 0xbb8;                               // 0x1003ad84
    fld<int>(ctx, 0x2250) = 0xbb8;                               // 0x1003ad91
    fld<int>(ctx, 0x2254) = 0xbb;                                // 0x1003ad9e
    fld<int>(ctx, 0x2258) = 0xbb;                                // 0x1003adab
    fld<int>(ctx, 0x225c) = 0xbb;                                // 0x1003adb8
    fld<int>(ctx, 0x224c) = 0x5d;                                // 0x1003adc5
    fld<int>(ctx, 0x2260) = 0x5d;                                // 0x1003add2
    fld<int>(ctx, 0x2264) = 0x5d;                                // 0x1003addf
    const int ratio = (fld<int>(ctx, 0x2250) / fld<int>(ctx, 0x2254)) << 6;   // idiv + shl at 0x1003adf6
    fld<float>(ctx, 0x2228) = static_cast<float>(ratio);         // fild + fstp dword at 0x1003ae02
    for (i = 0; i < 0x3c; ++i)                                   // jge at 0x1003ae24
        fld<int>(ctx, 0x2268 + i * 4) = 0;                       // 0x1003ae2c
    for (i = 0; i < 0x78; ++i)                                   // jge at 0x1003ae4f
        fld<int>(ctx, 0x2358 + i * 4) = 0;                       // 0x1003ae57
    fld<int>(ctx, 0x2538) = 0;                                   // 0x1003ae67
    fld<int>(ctx, 0x253c) = 0;                                   // 0x1003ae74
    fld<int>(ctx, 0x2540) = 0;                                   // 0x1003ae81
}

// 0x0003aef0  codecInitContextB (VA 0x1003aef0)
// __cdecl with one stack argument (`add esp, 0x44` + `ret` at 0x1003b142-0x1003b14f pops nothing), a /Od /GZ
// debug body whose frame [ebp-0x44..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x11, at
// 0x1003af06) and whose only live local is the loop counter [ebp-4]. eax at the `ret` holds the argument
// (0x1003b132), not a return value, so the entry declares ret="void".
// It calls the same codecSetParams 0x1003aa50 (0x1003af08) and then writes the 0xc00-byte decoder context,
// which is the object c3br mapped as codecTransformStage's stage record (0..0xa8) glued to codecSynthStage's
// state (+0xa8..+0xc00).
// Four regions are NOT written: +0x64c, +0x650, +0x658..+0x683 and +0xbea..+0xbeb. The fixtures seed them
// differently per vector, which is what makes the final states distinguishable.
typedef void(__cdecl* CodecInitContextB_t)(void*);
CodecInitContextB_t CodecInitContextB_orig;
void __cdecl CodecInitContextB_re(void* ctx) {
    reinterpret_cast<Void0_t>(c3ca_sound(0x0003AA50))();         // call at 0x1003af08
    int i;
    fld<int>(ctx, 0x0000) = 0x3c;                                // 0x1003af10
    fld<int>(ctx, 0x0004) = 1;                                   // 0x1003af19
    fld<int>(ctx, 0x0008) = 0;                                   // 0x1003af23
    fld<int>(ctx, 0x000c) = 0;                                   // 0x1003af2d
    fld<int>(ctx, 0x0010) = 0x3c;                                // 0x1003af37
    fld<int>(ctx, 0x0014) = 0;                                   // 0x1003af41
    for (i = 0; i < 0x1e; ++i)                                   // jge at 0x1003af5e
        fld<int>(ctx, 0x0018 + i * 4) = 0;                       // 0x1003af66
    for (i = 0; i < 3; ++i) {                                    // jge at 0x1003af86
        fld<int>(ctx, 0x0090 + i * 4) = 0;                       // 0x1003af8e
        fld<int>(ctx, 0x009c + i * 4) = 0;                       // 0x1003af9f
    }
    for (i = 0; i < 0x168; ++i)                                  // jge at 0x1003afc5
        fld<int>(ctx, 0x00a8 + i * 4) = 0;                       // 0x1003afcd
    fld<int>(ctx, 0x0648) = 0xb4;                                // 0x1003afdd
    fld<int>(ctx, 0x0654) = 0x3f800000;                          // 1.0f, 0x1003afea
    fld<int>(ctx, 0x0684) = 1;                                   // 0x1003aff7
    fld<int>(ctx, 0x0688) = 0;                                   // 0x1003b004
    for (i = 0; i < 0xa6; ++i) {                                 // jge at 0x1003b027
        fld<int>(ctx, 0x068c + i * 4) = 0;                       // 0x1003b02f
        fld<int>(ctx, 0x0924 + i * 4) = 0;                       // 0x1003b040
    }
    fld<int>(ctx, 0x0bbc) = 0;                                   // 0x1003b050
    fld<int>(ctx, 0x0bc0) = 0;                                   // 0x1003b05d
    fld<int>(ctx, 0x0bc4) = 0;                                   // 0x1003b06a
    fld<int>(ctx, 0x0bc8) = 0;                                   // 0x1003b077
    fld<int>(ctx, 0x0bcc) = 0;                                   // 0x1003b084
    fld<int>(ctx, 0x0bd0) = 0;                                   // 0x1003b091
    fld<int>(ctx, 0x0bd4) = 0;                                   // 0x1003b09e
    fld<int>(ctx, 0x0bd8) = 2;                                   // 0x1003b0ab
    fld<int>(ctx, 0x0bdc) = 5;                                   // 0x1003b0b8
    fld<short>(ctx, 0x0be0) = static_cast<short>(0xad57);        // 0x1003b0c5
    fld<short>(ctx, 0x0be2) = static_cast<short>(0xdee2);        // 0x1003b0d1
    fld<short>(ctx, 0x0be4) = static_cast<short>(0x78ac);        // 0x1003b0dd
    fld<short>(ctx, 0x0be6) = static_cast<short>(0xd818);        // 0x1003b0e9
    fld<short>(ctx, 0x0be8) = static_cast<short>(0x4236);        // 0x1003b0f5
    fld<int>(ctx, 0x0bec) = 0;                                   // 0x1003b101
    fld<int>(ctx, 0x0bf0) = 0;                                   // 0x1003b10e
    fld<int>(ctx, 0x0bf4) = 0;                                   // 0x1003b11b
    fld<int>(ctx, 0x0bf8) = 0;                                   // 0x1003b128
    fld<int>(ctx, 0x0bfc) = 0;                                   // 0x1003b135
}

// ---------------------------------------------------------------------------------------------------------
// voice, track and channel setters
// ---------------------------------------------------------------------------------------------------------

// 0x00037f30  Voice_setPanGains (VA 0x10037f30)
// __thiscall with one stack argument (`ret 4` at 0x10037f84 and 0x10038029); eax at either `ret` is not a
// return value (the early path leaves [ecx+0x58] in it, the long path the high dword of a double), so the
// entry declares ret="void".
// The pan is clamped to [-0x40, 0x3f] (`jge 0x10037f4a` at 0x10037f41, `jle 0x10037f57` at 0x10037f4d) and
// stored at this+0x48 (0x10037f5a). The record's two inputs are the int at this+0x50 (0x10037f57) and the
// double at this+0x58 (0x10037f9b, 0x10037fab, 0x10037fb2).
// A pan of 0 (`jne 0x10037f87` at 0x10037f5f) copies both inputs to both output pairs unchanged
// (0x10037f61-0x10037f79): this+0x68 and this+0x78 take the int, this+0x70 and this+0x80 take the double.
// Otherwise the divisor is 0x3f plus the sign bit of the pan (`setl al` + `add eax, 0x3f` at
// 0x10037f8b-0x10037f8e, so 0x40 for a negative pan), and:
//   * the int share is (this+0x50 * pan) / divisor (`imul` at 0x10037f98, `cdq` + `idiv` at 0x10037fa6,
//     0x10037fa7); this+0x68 gets share + this+0x50 (`lea` at 0x10037fb5) and this+0x78 gets
//     this+0x50 - share (`sub` at 0x10037fb8), each clamped to at most 0x10000 with a SIGNED compare
//     (`jle 0x10037fc7` at 0x10037fc0, `jle 0x10037ff5` at 0x10037fee);
//   * the double share is (double)pan * this+0x58 / (double)divisor (`fild` + `fmul qword` + `fidiv dword`
//     at 0x10037f95-0x10037fa2); this+0x70 gets share + this+0x58 (`fld st(0)` + `fadd qword` + `fstp qword`
//     at 0x10037fa9-0x10037fae) and this+0x80 gets this+0x58 - share (`fsubr qword` at 0x10037fb2), each
//     replaced by 1.0 when it is ABOVE the 1.0 at 0x1005c5f8 (`fcomp` + `fnstsw` + `test ah, 0x41` +
//     `jne 0x10037fe8` at 0x10037fcb-0x10037fd6 and `fcom` + ... + `jne 0x1003800a` at
//     0x10037ff5-0x10038000; the `test ah, 0x41` reads C0 and C3, so equal, below and unordered all skip the
//     replacement).
// Every x87 step ends in a register or a qword slot, so the whole chain is plain double arithmetic.
typedef void(__fastcall* VoiceSetPanGains_t)(void*, void*, int);
VoiceSetPanGains_t VoiceSetPanGains_orig;
void __fastcall VoiceSetPanGains_re(void* self, void*, int pan) {
    if (pan < -0x40) pan = -0x40;                                // jge at 0x10037f41
    else if (pan > 0x3f) pan = 0x3f;                             // jle at 0x10037f4d
    const int level = fld<int>(self, 0x50);                      // 0x10037f57
    fld<int>(self, 0x48) = pan;                                  // 0x10037f5a
    if (pan == 0) {                                              // jne at 0x10037f5f
        const int lo = fld<int>(self, 0x58);                     // 0x10037f61
        const int hi = fld<int>(self, 0x5c);                     // 0x10037f64
        fld<int>(self, 0x70) = lo;                               // 0x10037f67
        fld<int>(self, 0x80) = lo;                               // 0x10037f6a
        fld<int>(self, 0x68) = level;                            // 0x10037f70
        fld<int>(self, 0x78) = level;                            // 0x10037f73
        fld<int>(self, 0x74) = hi;                               // 0x10037f76
        fld<int>(self, 0x84) = hi;                               // 0x10037f79
        return;
    }
    const int divisor = (pan < 0 ? 1 : 0) + 0x3f;                // setl + add at 0x10037f8b, 0x10037f8e
    const int product = static_cast<int>(static_cast<unsigned>(level) * static_cast<unsigned>(pan));
    const int share = product / divisor;                         // idiv at 0x10037fa7
    const double base = fld<double>(self, 0x58);
    const double dshare = static_cast<double>(pan) * base / static_cast<double>(divisor);
    int right = share + level;                                   // lea at 0x10037fb5
    int left = level - share;                                    // sub at 0x10037fb8
    double gainRight = dshare + base;                            // fadd at 0x10037fab
    double gainLeft = base - dshare;                             // fsubr at 0x10037fb2
    const double one = *reinterpret_cast<const double*>(c3ca_sound(0x0005c5f8));   // 1.0
    if (right > 0x10000) right = 0x10000;                        // jle at 0x10037fc0
    if (gainRight > one) gainRight = one;                        // jne at 0x10037fd6
    if (left > 0x10000) left = 0x10000;                          // jle at 0x10037fee
    if (gainLeft > one) gainLeft = one;                          // jne at 0x10038000
    fld<int>(self, 0x68) = right;                                // 0x1003800e
    fld<int>(self, 0x78) = left;                                 // 0x10038015
    fld<double>(self, 0x80) = gainLeft;                          // fstp qword at 0x10038018
    fld<double>(self, 0x70) = gainRight;                         // 0x1003801e, 0x10038022
}

// 0x0001e530  Track_clearFlag8 (VA 0x1001e530)
// __thiscall with no stack argument (`ret` at 0x1001e53a). It loads the dword at this+0x4c (0x1001e530),
// clears bit 3 of its LOW BYTE with `and al, 0xf7` (0x1001e533) and stores the whole dword back
// (0x1001e535); 0xf7 is ~0x08 inside that byte and the upper three bytes are carried through unchanged, so
// the result is the dword with bit 3 cleared. Returns 0 (`xor eax, eax` at 0x1001e538).
typedef int(__fastcall* TrackClearFlag8_t)(void*, void*);
TrackClearFlag8_t TrackClearFlag8_orig;
int __fastcall TrackClearFlag8_re(void* self, void*) {
    fld<unsigned>(self, 0x4c) = fld<unsigned>(self, 0x4c) & ~0x8u;   // and al, 0xf7 at 0x1001e533
    return 0;                                                    // xor eax, eax at 0x1001e538
}

// 0x00035750  Channel_setPitchRate (VA 0x10035750)
// __thiscall with one stack argument (`ret 4` at 0x1003575a). It stores the argument at this+0x220
// (0x10035754) and nothing else; eax at the `ret` is the argument itself, not a return value, so the entry
// declares ret="void".
typedef void(__fastcall* ChannelSetPitchRate_t)(void*, void*, int);
ChannelSetPitchRate_t ChannelSetPitchRate_orig;
void __fastcall ChannelSetPitchRate_re(void* self, void*, int rate) {
    fld<int>(self, 0x220) = rate;                                // 0x10035754
}

// 0x0002b260  Seq_applyChannelLevel (VA 0x1002b260)
// __thiscall with one stack argument (`ret 4` at 0x1002b27c). The argument is masked with 0x7f
// (0x1002b264), masked again on its way to the record (`and edx, 0x7f` at 0x1002b269, a second mask over the
// already masked value) and stored at this+4 (0x1002b26c). The object at this+0x40 (0x1002b26f) is then
// called through slot 0x40 of its own vtable (`mov edx, [ecx]` + `call [edx+0x40]` at 0x1002b276,
// 0x1002b279) with the masked value, unless that pointer is null (`je 0x1002b27c` at 0x1002b274). The call
// is __thiscall: ecx holds the inner object and the single argument is the one `push eax`, with no stack
// cleanup after the call and a `ret 4` at the end. eax at the `ret` is the callee's return value or, on the
// null path, the masked argument, so the entry declares ret="void".
typedef void(__fastcall* SeqDeviceSetLevel_t)(void*, void*, int);
typedef void(__fastcall* SeqApplyChannelLevel_t)(void*, void*, int);
SeqApplyChannelLevel_t SeqApplyChannelLevel_orig;
void __fastcall SeqApplyChannelLevel_re(void* self, void*, int level) {
    const int masked = level & 0x7f;                             // 0x1002b264
    fld<int>(self, 0x04) = masked & 0x7f;                        // and edx, 0x7f + store at 0x1002b269
    void* inner = fld<void*>(self, 0x40);                        // 0x1002b26f
    if (inner != 0) {                                            // je at 0x1002b274
        void** vtbl = *reinterpret_cast<void***>(inner);         // 0x1002b276
        reinterpret_cast<SeqDeviceSetLevel_t>(vtbl[0x40 / 4])(inner, 0, masked);   // 0x1002b279
    }
}

// ---------------------------------------------------------------------------------------------------------
// DirectSound buffer forwarders
// ---------------------------------------------------------------------------------------------------------
// Both reach their interface through the pointer at this+0x60 and call one slot of ITS vtable with
// (interface, value) pushed in that order and NO stack cleanup after the call, which with the `ret 4` of the
// wrapper makes the callee a __stdcall taking two arguments. The A/B never passes a real DirectSound
// interface: the fixture builds its own object with its own vtable, whose slot is a NativeCallback that
// records what it was called with.

// 0x0000f160  DsBuffer::setFrequency (VA 0x1000f160)
// __thiscall with one stack argument (`ret 4` at 0x1000f172). It reads the interface at this+0x60
// (0x1000f160), returns at once when it is null (`je 0x1000f172` at 0x1000f165) and otherwise calls slot
// 0x44 of its vtable with (interface, value) (0x1000f16b-0x1000f16f). It writes nothing of its own; eax at
// the `ret` is the callee's return value or the interface pointer, so the entry declares ret="void" and the
// evidence is what the fixture's callback records.
typedef void(__stdcall* DsIfaceCall2_t)(void*, int);
typedef void(__fastcall* DsBufferSetFrequency_t)(void*, void*, int);
DsBufferSetFrequency_t DsBufferSetFrequency_orig;
void __fastcall DsBufferSetFrequency_re(void* self, void*, int freq) {
    void* iface = fld<void*>(self, 0x60);                        // 0x1000f160
    if (iface == 0) return;                                      // je at 0x1000f165
    void** vtbl = *reinterpret_cast<void***>(iface);             // 0x1000f16b
    reinterpret_cast<DsIfaceCall2_t>(vtbl[0x44 / 4])(iface, freq);   // call at 0x1000f16f
}

// 0x0000f190  DsBuffer::setVolumeRaw (VA 0x1000f190)
// __thiscall with one stack argument (`ret 4` at 0x1000f1a8). Unlike setFrequency it ALWAYS caches the
// argument at this+0xc4 (0x1000f199), before the null test on the interface at this+0x60 (`je 0x1000f1a8`
// at 0x1000f19f), and the slot it calls is 0x3c (0x1000f1a5). ret="void" for the same reason.
typedef void(__fastcall* DsBufferSetVolumeRaw_t)(void*, void*, int);
DsBufferSetVolumeRaw_t DsBufferSetVolumeRaw_orig;
void __fastcall DsBufferSetVolumeRaw_re(void* self, void*, int volume) {
    void* iface = fld<void*>(self, 0x60);                        // 0x1000f190
    fld<int>(self, 0xc4) = volume;                               // 0x1000f199
    if (iface == 0) return;                                      // je at 0x1000f19f
    void** vtbl = *reinterpret_cast<void***>(iface);             // 0x1000f1a1
    reinterpret_cast<DsIfaceCall2_t>(vtbl[0x3c / 4])(iface, volume);   // call at 0x1000f1a5
}

SG_HOOK("sound.dll", 0x0003edc0, sound_codecAlignAnalysisWindow, CodecAlignAnalysisWindow_re,
        CodecAlignAnalysisWindow_orig);
SG_HOOK("sound.dll", 0x00040740, sound_codecSelectWindowFromJumps, CodecSelectWindowFromJumps_re,
        CodecSelectWindowFromJumps_orig);
SG_HOOK("sound.dll", 0x0003aae0, sound_codecInitContextA, CodecInitContextA_re, CodecInitContextA_orig);
SG_HOOK("sound.dll", 0x0003aef0, sound_codecInitContextB, CodecInitContextB_re, CodecInitContextB_orig);
SG_HOOK("sound.dll", 0x00037f30, sound_Voice_setPanGains, VoiceSetPanGains_re, VoiceSetPanGains_orig);
SG_HOOK("sound.dll", 0x0001e530, sound_Track_clearFlag8, TrackClearFlag8_re, TrackClearFlag8_orig);
SG_HOOK("sound.dll", 0x00035750, sound_Channel_setPitchRate, ChannelSetPitchRate_re,
        ChannelSetPitchRate_orig);
SG_HOOK("sound.dll", 0x0002b260, sound_Seq_applyChannelLevel, SeqApplyChannelLevel_re,
        SeqApplyChannelLevel_orig);
SG_HOOK("sound.dll", 0x0000f160, sound_DsBuffer_setFrequency, DsBufferSetFrequency_re,
        DsBufferSetFrequency_orig);
SG_HOOK("sound.dll", 0x0000f190, sound_DsBuffer_setVolumeRaw, DsBufferSetVolumeRaw_re,
        DsBufferSetVolumeRaw_orig);

}  // namespace
