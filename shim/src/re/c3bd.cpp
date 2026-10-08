// C3 batch c3bd (2026-10-08) of sound.dll: seven leaves of the speech codec (the stage leaves that
// codecStageDispatch 0x1003c640 and codecApplyFixedCoeffs 0x1003d040 call) and two MIDI track walkers.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside
// each body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32 or COM call is made, nothing is allocated or freed, and no live sound object,
// device, list, buffer or module global is touched: every buffer and record the A/B passes in is allocated by
// the fixture. One body forwards to an original callee at its own address (Midi_seekTrackToTick ->
// Midi_readVarLen 0x1001e710, which no batch hooks); sound.dll is relocatable, so that address and every
// read-only constant are resolved against the module base at the first call.
// Floating point: the codec leaves are x87 (/Od /GZ debug bodies, every intermediate spilled), the shim is
// SSE2. Each `fstp dword` spill is written as an assignment to a float and every operand inside one expression
// is widened with static_cast<double>, which is the c3ap method: the A/B thread runs with control word 0x027f
// (53-bit, round to nearest), so an x87 step that ends in a register is exactly a double operation and a step
// that ends in a dword slot is exactly a double operation rounded once to float. Every float constant is read
// from the module's own .rdata, never retyped as a literal.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

// sound.dll has base relocations, so an original callee, table or constant is addressed through the loaded base.
char* c3bd_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

float c3bd_f(unsigned rva) { return *reinterpret_cast<const float*>(c3bd_sound(rva)); }

unsigned c3bd_bits(float v) { return *reinterpret_cast<const unsigned*>(&v); }

// ---------------------------------------------------------------------------------------------------------
// codec leaves
// ---------------------------------------------------------------------------------------------------------

// 0x0003e520  decodeTableSymbol (VA 0x1003e520)
// __cdecl with three pointer arguments ([ebp+8] the packed symbol, [ebp+0xc] the decoded value slot,
// [ebp+0x10] a counter; `ret` at 0x1003e5f9 pops nothing), a /Od /GZ debug body.
// The symbol's LOW BYTE is folded to its parity in [ebp-0xc]: `and ecx, 0xff` (0x1003e53d), then three
// rounds of `x ^= x / 2^k` with k = 4, 2, 1 written as SIGNED divisions (`cdq` + `and edx, 0xf` + `sar 4` at
// 0x1003e549-0x1003e54f, `and edx, 3` + `sar 2` at 0x1003e55e-0x1003e563, `sub eax, edx` + `sar 1` at
// 0x1003e572-0x1003e574), and finally `and eax, 1` (0x1003e581). The value is non-negative at every step, so
// each division is the plain shift.
// The symbol's LOW SEVEN BITS index the 128-entry dword table at 0x10064a80 (`and edx, 0x7f` at 0x1003e58c,
// `mov eax, [edx*4 + 0x10064a80]` at 0x1003e58f). The entry's low nibble is written to the value slot
// (`and ecx, 0xf` at 0x1003e59c, store at 0x1003e5a2) and its bit 4 selects the counter arm
// (`and eax, 0x10` at 0x1003e5a7, `je 0x1003e5c8` at 0x1003e5b1).
// With bit 4 SET the counter is incremented only when the parity is 1 (`je 0x1003e5c6` at 0x1003e5b7,
// increment at 0x1003e5be). With bit 4 CLEAR the counter is incremented unconditionally (0x1003e5cd) and,
// when the parity is 0 (`jne 0x1003e5f1` at 0x1003e5d9), incremented a second time (0x1003e5e0) and the value
// slot is overwritten with -1 (0x1003e5eb). Returns 0 (`xor eax, eax` at 0x1003e5f1).
typedef int(__cdecl* DecodeTableSymbol_t)(const int*, int*, int*);
DecodeTableSymbol_t DecodeTableSymbol_orig;
int __cdecl DecodeTableSymbol_re(const int* symbol, int* value, int* counter) {
    int parity = *symbol & 0xFF;                 // and at 0x1003e53d
    parity ^= parity / 16;                       // sar 4 at 0x1003e54f
    parity ^= parity / 4;                        // sar 2 at 0x1003e563
    parity ^= parity / 2;                        // sar 1 at 0x1003e574
    parity &= 1;                                 // and at 0x1003e581

    const unsigned* table = reinterpret_cast<const unsigned*>(c3bd_sound(0x00064A80));
    const int entry = static_cast<int>(table[*symbol & 0x7F]);   // 0x1003e58f
    *value = entry & 0xF;                        // 0x1003e5a2
    if ((entry & 0x10) != 0) {                   // je at 0x1003e5b1
        if (parity != 0) *counter += 1;          // je at 0x1003e5b7, add at 0x1003e5be
    } else {
        *counter += 1;                           // add at 0x1003e5cd
        if (parity == 0) {                       // jne at 0x1003e5d9
            *counter += 1;                       // add at 0x1003e5e0
            *value = -1;                         // 0x1003e5eb
        }
    }
    return 0;
}

// 0x0003e600  substituteIfBelowThreshold (VA 0x1003e600)
// __cdecl with three pointer arguments ([ebp+8] the sample count, [ebp+0xc] the replacement samples,
// [ebp+0x10] the samples that are scanned AND overwritten; `ret` at 0x1003e6ce pops nothing).
// Both sample pointers are decremented by 4 bytes on entry (0x1003e61b, 0x1003e627) and both loops run from 1
// to *count inclusive (`jg 0x1003e68c` at 0x1003e64a, `jg 0x1003e6c6` at 0x1003e6b0, SIGNED), so element i of
// a loop is element i-1 of the caller's array.
// The scan takes the magnitude of each scanned sample: `fcomp dword [0x1005ec00]` (0x1003e65b, the float 0.0f
// in the module's .rdata) with `test ah, 1` (0x1003e663) tests the C0 flag, so a sample BELOW zero is negated
// (`fchs` at 0x1003e673, spilled to a float slot at 0x1003e675) and any other sample is copied as a raw dword
// (0x1003e668-0x1003e66b). That magnitude is then compared with `fcomp dword [0x1005ebd4]` (0x1003e67b, the
// float 0.99f) and `test ah, 0x41` (0x1003e683) tests C3|C0, so the loop CONTINUES while the magnitude is at
// or below the limit and leaves it (0x1003e688) as soon as one sample is strictly above the limit.
// If no sample is above the limit the function returns 0 without writing anything (0x1003e68c).
// Otherwise the count is re-read through its pointer (0x1003e693) and the scanned array is overwritten
// element by element with the replacement array as raw dwords (`mov` at 0x1003e6c1). Returns 0 (0x1003e6c6).
typedef int(__cdecl* SubstituteIfBelowThreshold_t)(const int*, const float*, float*);
SubstituteIfBelowThreshold_t SubstituteIfBelowThreshold_orig;
int __cdecl SubstituteIfBelowThreshold_re(const int* count, const float* replacement, float* samples) {
    samples -= 1;                                            // sub at 0x1003e61b
    replacement -= 1;                                        // sub at 0x1003e627
    const float zero = c3bd_f(0x0005EC00);
    const float limit = c3bd_f(0x0005EBD4);

    int n = *count;
    bool above = false;
    for (int i = 1; i <= n; ++i) {
        const float v = samples[i];                          // raw dword copy at 0x1003e652
        float magnitude;
        if (static_cast<double>(v) < static_cast<double>(zero)) magnitude = -v;   // fchs at 0x1003e673
        else magnitude = v;                                                       // 0x1003e668
        if (static_cast<double>(magnitude) > static_cast<double>(limit)) {        // test ah, 0x41 at 0x1003e683
            above = true;
            break;
        }
    }
    if (!above) return 0;                                    // 0x1003e68c

    n = *count;                                              // re-read at 0x1003e693
    for (int i = 1; i <= n; ++i)
        *reinterpret_cast<unsigned*>(&samples[i]) = *reinterpret_cast<const unsigned*>(&replacement[i]);
    return 0;
}

// 0x0003d0a0  applyIir2 (VA 0x1003d0a0)
// __cdecl with three stack arguments ([ebp+8] the samples, in place, [ebp+0xc] the sample count, [ebp+0x10]
// the codec context; `ret` at 0x1003d1c2 pops nothing).
// The sample pointer is decremented by 4 bytes ONLY when it is non-null (`je 0x1003d0c7` at 0x1003d0bc, `sub`
// at 0x1003d0c1), so a null pointer with a count below 1 skips the loop untouched. The loop runs from 1 to
// *count inclusive (`jg 0x1003d1ba` at 0x1003d122, SIGNED).
// Five float state slots are taken from the context: +0xbec, +0xbf0, +0xbf4, +0xbf8 and +0xbfc
// (0x1003d0ca-0x1003d0ff), called s0..s4 below.
// Each round saves the raw dword of the input sample (0x1003d131), then computes
// `(sample - s0 * K0) + s1` with `fld [s0]` / `fmul dword [0x1005ebf0]` / `fsubr dword [samples+i*4]` /
// `fadd [s1]` (0x1003d137-0x1003d14b) and spills it to a float slot (`fstp dword [ebp-0x1c]` at 0x1003d14d).
// The output is then `((s2 * K1 + that) - s3 * K2) + s4 * K3` with the constants at 0x1005ebec, 0x1005ebe8
// and 0x1005ebe4 (0x1003d153-0x1003d176) and is stored back over the sample as a float (0x1003d17e).
// The state is finally shifted: s1 = s0, s0 = the saved input, s4 = s3, s3 = s2, s2 = the new sample, all as
// raw dword moves (0x1003d181-0x1003d1b3). Returns 0 (`xor eax, eax` at 0x1003d1ba).
typedef int(__cdecl* ApplyIir2_t)(float*, const int*, void*);
ApplyIir2_t ApplyIir2_orig;
int __cdecl ApplyIir2_re(float* samples, const int* count, void* context) {
    if (samples != 0) samples -= 1;                          // je at 0x1003d0bc, sub at 0x1003d0c1
    char* ctx = reinterpret_cast<char*>(context);
    float* s0 = reinterpret_cast<float*>(ctx + 0xBEC);       // 0x1003d0ca
    float* s1 = reinterpret_cast<float*>(ctx + 0xBF0);       // 0x1003d0d6
    float* s2 = reinterpret_cast<float*>(ctx + 0xBF4);       // 0x1003d0e2
    float* s3 = reinterpret_cast<float*>(ctx + 0xBF8);       // 0x1003d0ed
    float* s4 = reinterpret_cast<float*>(ctx + 0xBFC);       // 0x1003d0f9
    const float k0 = c3bd_f(0x0005EBF0);
    const float k1 = c3bd_f(0x0005EBEC);
    const float k2 = c3bd_f(0x0005EBE8);
    const float k3 = c3bd_f(0x0005EBE4);

    const int n = *count;                                    // 0x1003d105
    for (int i = 1; i <= n; ++i) {
        const unsigned saved = *reinterpret_cast<const unsigned*>(&samples[i]);   // 0x1003d131
        const float spilled = static_cast<float>(
            (static_cast<double>(samples[i]) - static_cast<double>(*s0) * static_cast<double>(k0))
            + static_cast<double>(*s1));                     // fstp dword at 0x1003d14d
        double out = static_cast<double>(*s2) * static_cast<double>(k1) + static_cast<double>(spilled);
        out = out - static_cast<double>(*s3) * static_cast<double>(k2);        // fsubp at 0x1003d169
        out = out + static_cast<double>(*s4) * static_cast<double>(k3);        // faddp at 0x1003d176
        samples[i] = static_cast<float>(out);                                  // fstp dword at 0x1003d17e

        *reinterpret_cast<unsigned*>(s1) = *reinterpret_cast<const unsigned*>(s0);   // 0x1003d189
        *reinterpret_cast<unsigned*>(s0) = saved;                                    // 0x1003d191
        *reinterpret_cast<unsigned*>(s4) = *reinterpret_cast<const unsigned*>(s3);   // 0x1003d19b
        *reinterpret_cast<unsigned*>(s3) = *reinterpret_cast<const unsigned*>(s2);   // 0x1003d1a5
        *reinterpret_cast<unsigned*>(s2) = *reinterpret_cast<const unsigned*>(&samples[i]);  // 0x1003d1b3
    }
    return 0;
}

// 0x00040e30  codecBiquadFilter (VA 0x10040e30)
// __cdecl with four stack arguments ([ebp+8] the samples, in place, [ebp+0xc] the first index, [ebp+0x10] the
// last index, [ebp+0x14] four float state slots; `ret` at 0x10040f57 pops nothing).
// The sample pointer is decremented by 4 bytes only when it is non-null (`je 0x10040e57` at 0x10040e4c).
// The four state slots are copied into locals as raw dwords (0x10040e5a-0x10040e77) and the two loop bounds
// are read once each through their pointers (0x10040e7d the last index, 0x10040e85 the first); the loop is
// `jg 0x10040f2c` at 0x10040e9b, SIGNED.
// Each round runs two identical biquad sections. The first uses the state pair s0, s1 and the constants at
// 0x1005ecc0 and 0x1005ecbc: `t = (s0 * KA + sample) - s1 * KB` (0x10040ead-0x10040ec2, spilled to a float at
// 0x10040ec4), then `sample = (t - (s0 + s0)) + s1` (`fadd st(0), st(0)` at 0x10040eca, `fsubr` at 0x10040ecc,
// `fadd` at 0x10040ecf, spilled at 0x10040ed2), then s1 = s0 and s0 = t (0x10040ed5-0x10040ede).
// The second uses s2, s3 and the constants at 0x1005ecb8 and 0x1005ecb4 in exactly the same shape
// (0x10040ee1-0x10040f12). The sample is finally scaled by the constant at 0x1005ecb0 and written back as a
// float (0x10040f15-0x10040f24).
// After the loop the four locals are written back to the state slots (0x10040f2c-0x10040f4c) and the function
// returns 0 (`xor eax, eax` at 0x10040f4f).
typedef int(__cdecl* CodecBiquadFilter_t)(float*, const int*, const int*, float*);
CodecBiquadFilter_t CodecBiquadFilter_orig;
int __cdecl CodecBiquadFilter_re(float* samples, const int* first, const int* last, float* state) {
    if (samples != 0) samples -= 1;                          // je at 0x10040e4c
    float s0 = state[0], s1 = state[1], s2 = state[2], s3 = state[3];   // 0x10040e5a-0x10040e77
    const float ka = c3bd_f(0x0005ECC0);
    const float kb = c3bd_f(0x0005ECBC);
    const float kc = c3bd_f(0x0005ECB8);
    const float kd = c3bd_f(0x0005ECB4);
    const float ke = c3bd_f(0x0005ECB0);

    const int n = *last;                                     // 0x10040e7d
    for (int i = *first; i <= n; ++i) {                      // 0x10040e85, jg at 0x10040e9b
        float sample = samples[i];                           // raw dword at 0x10040ea7
        float t = static_cast<float>(
            (static_cast<double>(s0) * static_cast<double>(ka) + static_cast<double>(sample))
            - static_cast<double>(s1) * static_cast<double>(kb));        // fstp at 0x10040ec4
        sample = static_cast<float>(
            (static_cast<double>(t) - (static_cast<double>(s0) + static_cast<double>(s0)))
            + static_cast<double>(s1));                                  // fstp at 0x10040ed2
        s1 = s0;                                                         // 0x10040ed8
        s0 = t;                                                          // 0x10040ede
        t = static_cast<float>(
            (static_cast<double>(s2) * static_cast<double>(kc) + static_cast<double>(sample))
            - static_cast<double>(s3) * static_cast<double>(kd));        // fstp at 0x10040ef8
        sample = static_cast<float>(
            (static_cast<double>(t) - (static_cast<double>(s2) + static_cast<double>(s2)))
            + static_cast<double>(s3));                                  // fstp at 0x10040f06
        s3 = s2;                                                         // 0x10040f0c
        s2 = t;                                                          // 0x10040f12
        samples[i] = static_cast<float>(static_cast<double>(sample) * static_cast<double>(ke));  // 0x10040f24
    }
    state[0] = s0; state[1] = s1; state[2] = s2; state[3] = s3;          // 0x10040f32-0x10040f4c
    return 0;
}

// 0x00040330  codecCorrelationLags (VA 0x10040330)
// __cdecl with five stack arguments ([ebp+8] the input samples, [ebp+0xc] the output samples, [ebp+0x10] the
// last index, [ebp+0x14] the window length, [ebp+0x18] two coefficient slots; `ret` at 0x100404c7 pops
// nothing). All three array pointers are decremented by 4 bytes (0x1004034b, 0x10040354, 0x10040360).
// Three accumulators live at [ebp+k*4-0x20] for k = 1, 2, 3; the slot for k = 0 is a fourth local used later.
// The outer loop runs k = 1..3 (`jg 0x100403e8` at 0x10040379) and zeroes its accumulator (0x1004037e), takes
// the lag `4k - 4` (`lea eax, [edx*4-4]` at 0x10040389) and runs an inner loop from `*last + 4k - *window`
// (0x1004039b-0x100403ab) to *last inclusive (`jg 0x100403e6` at 0x100403bf, SIGNED) in steps of TWO
// (`add edx, 2` at 0x100403b3), accumulating `in[j] * in[j - lag]` as a float
// (0x100403d0-0x100403e0).
// The two coefficient slots are then zeroed (0x100403f9, 0x10040403) along with the locals at [ebp-0x20] and
// [ebp-0x24] (0x100403e8, 0x100403ef), and the first accumulator is compared with the float 1e-10 at
// 0x1005ec40 (`fcomp` at 0x1004040d, `test ah, 0x41` at 0x10040415), so the solve runs only when it is
// strictly above that floor.
// The solve writes coefficient 0 = acc2 / acc1 (0x1004041a-0x10040423) and coefficient 1 =
// (acc3 - acc2 * coefficient0) / (acc1 - acc2 * coefficient0) (0x10040426-0x10040443), then leaves
// `coefficient0 - coefficient0 * coefficient1` in [ebp-0x20] (0x10040446-0x10040458) and a raw copy of
// coefficient 1 in [ebp-0x24] (0x1004045b-0x10040461).
// The output loop re-reads *last (0x10040467), runs from `*last + 1 - *window` (0x1004046c-0x10040479) to
// *last inclusive (`jg 0x100404bf` at 0x1004048d) and writes
// `(in[j] - A * in[j-4]) - B * in[j-8]` (0x1004048f-0x100404ba). Returns 0 (0x100404bf).
typedef int(__cdecl* CodecCorrelationLags_t)(const float*, float*, const int*, const int*, float*);
CodecCorrelationLags_t CodecCorrelationLags_orig;
int __cdecl CodecCorrelationLags_re(const float* in, float* out, const int* last, const int* window,
                                    float* coefficients) {
    out -= 1;                                                // sub at 0x1004034b
    in -= 1;                                                 // sub at 0x10040354
    coefficients -= 1;                                       // sub at 0x10040360

    float acc[4];
    for (int k = 1; k <= 3; ++k) {                           // jg at 0x10040379
        acc[k] = 0.0f;                                       // 0x1004037e
        const int lag = k * 4 - 4;                           // lea at 0x10040389
        const int bound = *last;                             // 0x10040396
        for (int j = *last + k * 4 - *window; j <= bound; j += 2)      // 0x1004039b, jg at 0x100403bf
            acc[k] = static_cast<float>(static_cast<double>(in[j]) * static_cast<double>(in[j - lag])
                                        + static_cast<double>(acc[k]));   // fstp at 0x100403e0
    }

    acc[0] = 0.0f;                                           // 0x100403e8
    float b = 0.0f;                                          // 0x100403ef
    coefficients[1] = 0.0f;                                  // 0x100403f9
    coefficients[2] = 0.0f;                                  // 0x10040403
    if (static_cast<double>(acc[1]) > static_cast<double>(c3bd_f(0x0005EC40))) {   // test ah, 0x41 at 0x10040415
        coefficients[1] = static_cast<float>(static_cast<double>(acc[2])
                                             / static_cast<double>(acc[1]));       // fstp at 0x10040423
        coefficients[2] = static_cast<float>(
            (static_cast<double>(acc[3]) - static_cast<double>(acc[2]) * static_cast<double>(coefficients[1]))
            / (static_cast<double>(acc[1]) - static_cast<double>(acc[2]) * static_cast<double>(coefficients[1])));
        acc[0] = static_cast<float>(static_cast<double>(coefficients[1])
                                    - static_cast<double>(coefficients[1]) * static_cast<double>(coefficients[2]));
        b = coefficients[2];                                 // raw dword at 0x10040461
    }

    const int bound = *last;                                 // 0x10040467
    for (int j = *last + 1 - *window; j <= bound; ++j)       // 0x1004046c, jg at 0x1004048d
        out[j] = static_cast<float>(
            (static_cast<double>(in[j]) - static_cast<double>(acc[0]) * static_cast<double>(in[j - 4]))
            - static_cast<double>(b) * static_cast<double>(in[j - 8]));            // fstp at 0x100404ba
    return 0;
}

// 0x000404d0  codecSymmetricFir (VA 0x100404d0)
// __cdecl with four stack arguments ([ebp+8] the input samples, [ebp+0xc] the output samples, [ebp+0x10] the
// last index, [ebp+0x14] the window length; `ret` at 0x10040734 pops nothing). Both array pointers are
// decremented by 4 bytes (0x100404eb, 0x100404f7) and the single loop runs from `*last + 1 - *window`
// (0x10040502-0x1004050f) to *last inclusive (`jg 0x1004072c` at 0x10040523, SIGNED).
// The body is a fully unrolled 31-tap symmetric FIR. The first tap is `(in[j] + in[j-30]) * K15` with the
// constant at 0x1005eca8 (0x10040535-0x1004053c), spilled to a float at 0x10040542. Fourteen further taps add
// `(in[j-t] + in[j-(30-t)]) * K(15-t)` for t = 1..14, each with its own constant four bytes lower in .rdata
// (0x1005eca4 down to 0x1005ec70) and each spilled to the same float slot (0x10040551-0x10040702). The centre
// tap adds `in[j-15] * K0` with the constant at 0x1005ec6c (0x1004070b-0x10040718). The accumulator is then
// copied to the output as a raw dword (`mov` at 0x10040724). Returns 0 (0x1004072c).
typedef int(__cdecl* CodecSymmetricFir_t)(const float*, float*, const int*, const int*);
CodecSymmetricFir_t CodecSymmetricFir_orig;
int __cdecl CodecSymmetricFir_re(const float* in, float* out, const int* last, const int* window) {
    out -= 1;                                                // sub at 0x100404eb
    in -= 1;                                                 // sub at 0x100404f7
    // K[0] is the centre tap at 0x1005ec6c and K[15] the outermost at 0x1005eca8.
    const float* k = reinterpret_cast<const float*>(c3bd_sound(0x0005EC6C));

    const int bound = *last;                                 // 0x100404fd
    for (int j = *last + 1 - *window; j <= bound; ++j) {     // 0x10040505, jg at 0x10040523
        float acc = static_cast<float>(
            (static_cast<double>(in[j]) + static_cast<double>(in[j - 30])) * static_cast<double>(k[15]));
        for (int t = 1; t <= 14; ++t)                        // the unrolled taps at 0x10040551-0x10040702
            acc = static_cast<float>(
                (static_cast<double>(in[j - t]) + static_cast<double>(in[j - (30 - t)]))
                * static_cast<double>(k[15 - t]) + static_cast<double>(acc));
        acc = static_cast<float>(static_cast<double>(in[j - 15]) * static_cast<double>(k[0])
                                 + static_cast<double>(acc));          // fstp at 0x10040718
        *reinterpret_cast<unsigned*>(&out[j]) = c3bd_bits(acc);        // mov at 0x10040724
    }
    return 0;
}

// 0x0003e9f0  codecAutocorrelation (VA 0x1003e9f0)
// __cdecl with six stack arguments ([ebp+8] the order, [ebp+0xc] the first index, [ebp+0x10] the last index,
// [ebp+0x14] the samples, [ebp+0x18] an order x order matrix, [ebp+0x1c] the lag vector; `ret` at 0x1003ec58
// pops nothing).
// On entry the lag vector is decremented by 4 bytes (0x1003ea0b), the order is copied to [ebp-4] (0x1003ea16),
// the matrix is decremented by `(order + 1) * 4` bytes (0x1003ea19-0x1003ea2d) and the samples by 4 bytes
// (0x1003ea33), and [ebp-0x20] takes `*first + *order` (0x1003ea39-0x1003ea43), called S below. Every index
// in the four loops is therefore one-based.
// Loop A (i = 1..*order, `jg 0x1003eacb` at 0x1003ea66) zeroes matrix[i + order] (0x1003ea71) and accumulates
// `x[j-1] * x[j-i]` over j = S..*last inclusive (`jg 0x1003eac9` at 0x1003ea97), spilling to a float each
// round (0x1003eaae-0x1003eac4).
// Loop B zeroes lag[*order] (0x1003ead3) and accumulates `x[j] * x[j - *order]` over the same j range
// (`jg 0x1003eb2a` at 0x1003eaf9, body 0x1003eafb-0x1003eb25).
// Loop C (i = 2..*order, `jg 0x1003ebdc` at 0x1003eb4a; k = 2..i, `jg 0x1003ebd7` at 0x1003eb6e) writes
// matrix[i + k*order] = `(matrix[i + (k-1)*order - 1] - x[*last+1-i] * x[*last+1-k]) + x[S-i] * x[S-k]`
// (0x1003eb70-0x1003ebd2).
// Loop D (k = 1..*order-1, `jg 0x1003ec50` at 0x1003ebff) writes
// lag[k] = `(matrix[k + order + 1] - x[S-1] * x[S-1-k]) + x[*last] * x[*last-k]` (0x1003ec01-0x1003ec4b).
// Returns 0 (`xor eax, eax` at 0x1003ec50).
typedef int(__cdecl* CodecAutocorrelation_t)(const int*, const int*, const int*, const float*, float*, float*);
CodecAutocorrelation_t CodecAutocorrelation_orig;
int __cdecl CodecAutocorrelation_re(const int* order, const int* first, const int* last, const float* x,
                                    float* matrix, float* lag) {
    lag -= 1;                                                // sub at 0x1003ea0b
    const int m = *order;                                    // 0x1003ea16
    matrix -= (m + 1);                                       // 0x1003ea22-0x1003ea2d
    x -= 1;                                                  // sub at 0x1003ea33
    const int s = *first + *order;                           // 0x1003ea39-0x1003ea43

    int outer = *order;                                      // 0x1003ea4b
    for (int i = 1; i <= outer; ++i) {                       // jg at 0x1003ea66
        matrix[i + m] = 0.0f;                                // 0x1003ea71
        const int bound = *last;                             // 0x1003ea7b
        for (int j = s; j <= bound; ++j)                     // jg at 0x1003ea97
            matrix[i + m] = static_cast<float>(
                static_cast<double>(x[j - 1]) * static_cast<double>(x[j - i])
                + static_cast<double>(matrix[i + m]));       // fstp at 0x1003eac4
    }

    lag[*order] = 0.0f;                                      // 0x1003ead3
    outer = *last;                                           // 0x1003eadd
    for (int j = s; j <= outer; ++j)                         // jg at 0x1003eaf9
        lag[*order] = static_cast<float>(
            static_cast<double>(x[j]) * static_cast<double>(x[j - *order])
            + static_cast<double>(lag[*order]));             // fstp at 0x1003eb25

    outer = *order;                                          // 0x1003eb2d
    for (int i = 2; i <= outer; ++i) {                       // jg at 0x1003eb4a
        for (int k = 2; k <= i; ++k) {                       // jg at 0x1003eb6e
            const int src = i + (k - 1) * m - 1;             // lea at 0x1003eb7d
            double t = static_cast<double>(matrix[src])
                       - static_cast<double>(x[*last + 1 - i]) * static_cast<double>(x[*last + 1 - k]);
            t = t + static_cast<double>(x[s - i]) * static_cast<double>(x[s - k]);   // faddp at 0x1003ebc1
            matrix[i + k * m] = static_cast<float>(t);       // fstp at 0x1003ebd2
        }
    }

    outer = *order - 1;                                      // 0x1003ebe1
    for (int k = 1; k <= outer; ++k) {                       // jg at 0x1003ebff
        double t = static_cast<double>(matrix[k + m + 1])
                   - static_cast<double>(x[s - 1]) * static_cast<double>(x[s - 1 - k]);
        t = t + static_cast<double>(x[*last]) * static_cast<double>(x[*last - k]);   // faddp at 0x1003ec43
        lag[k] = static_cast<float>(t);                      // fstp at 0x1003ec4b
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------------------
// MIDI track walkers
// ---------------------------------------------------------------------------------------------------------

// The two walkers below share the shape of a standard MIDI track: a variable-length delta time, then either a
// status byte (bit 7 set) or a continuation of the previous status, then a fixed number of data bytes taken
// from a 128-entry byte table indexed by `status - 0x80`. Both tables map 0x80 and 0x90 to two data bytes,
// 0xc0 and 0xe0 to one, 0xb0 to a controller case, 0xff to a meta case and every other status to an error.

// 0x0001ef80  Midi_scanTrack (VA 0x1001ef80)
// __thiscall with no stack argument (`ret` at 0x1001f050 pops nothing); the track bytes come from [ecx+0x3c]
// (0x1001ef85) and the running status starts at 0 (`xor eax, eax` at 0x1001ef88).
// Each round reads a variable-length quantity inline, seven bits per byte, most significant first
// (`shl esi, 7` / `or esi, ecx` at 0x1001efac-0x1001efaf, continuing while bit 7 is set, `jne 0x1001efa2` at
// 0x1001efb5), counting the bytes in ebx. The byte AFTER the quantity is read (0x1001efbb), the cursor is
// advanced past the quantity (0x1001efbf) and the quantity is added to the total (0x1001efc1).
// If that byte has bit 7 set (`je 0x1001efe5` at 0x1001efca) it becomes the running status: the full byte is
// kept when its high nibble is 0xf0 (`je 0x1001efe4` at 0x1001efe0) and only the high nibble otherwise
// (0x1001efe2); the cursor then steps over it (0x1001efe4).
// A running status outside 0x80..0xff makes the function return 0 without writing anything (`ja 0x1001f051`
// at 0x1001efeb, UNSIGNED, exit at 0x1001f054). Otherwise the byte table at 0x1001f06c selects a case through
// the jump table at 0x1001f05c (0x1001efef-0x1001eff5): case 0 skips two data bytes (0x1001f02f), case 1 skips
// one (0x1001f02c), case 2 is the meta event (0x1001effc) and case 3 returns 0 (0x1001f051).
// The meta case marks the track finished when the meta type is 0x2f (0x1001effc-0x1001f002), steps over the
// type byte (0x1001f00a) and skips the meta payload by reading a second variable-length quantity and advancing
// past both it and its own byte count (0x1001f00f-0x1001f028).
// The loop ends when the finished flag is set (`je 0x1001ef9c` at 0x1001f038); the total is then stored at
// [this+0x54] (0x1001f049) and returned (0x1001f042).
typedef int(__fastcall* MidiScanTrack_t)(void*, void*);
MidiScanTrack_t MidiScanTrack_orig;

// The inline variable-length reader of Midi_scanTrack (0x1001efa2-0x1001efb5 and 0x1001f011-0x1001f024).
unsigned c3bd_inlineVarLen(const unsigned char* p, unsigned* bytes) {
    unsigned value = 0;
    unsigned n = 0;
    unsigned char b;
    do {
        b = *p;
        value = (value << 7) | (b & 0x7Fu);                  // shl/or at 0x1001efac
        ++n;                                                 // inc ebx at 0x1001efb1
        ++p;                                                 // inc edi at 0x1001efb2
    } while ((b & 0x80u) != 0);                              // jne at 0x1001efb5
    *bytes = n;
    return value;
}

int __fastcall MidiScanTrack_re(void* self, void*) {
    const unsigned char* p = *reinterpret_cast<const unsigned char* const*>(
        reinterpret_cast<char*>(self) + 0x3C);               // 0x1001ef85
    const unsigned char* table = reinterpret_cast<const unsigned char*>(c3bd_sound(0x0001F06C));
    unsigned total = 0;                                      // [esp+0x10], 0x1001ef90
    unsigned status = 0;                                     // eax, 0x1001ef88
    bool finished = false;                                   // [esp+0x14], 0x1001ef98

    for (;;) {
        unsigned bytes = 0;
        const unsigned delta = c3bd_inlineVarLen(p, &bytes);
        const unsigned char next = p[bytes];                 // 0x1001efbb
        p += bytes;                                          // 0x1001efbf
        total += delta;                                      // 0x1001efc1
        if ((next & 0x80u) != 0) {                           // je at 0x1001efca
            const unsigned high = next & 0xF0u;              // 0x1001efd4
            status = (high == 0xF0u) ? next : high;          // je at 0x1001efe0
            ++p;                                             // 0x1001efe4
        }
        const unsigned index = status - 0x80u;               // lea at 0x1001efe5
        if (index > 0x7Fu) return 0;                         // ja at 0x1001efeb (UNSIGNED)
        switch (table[index]) {
        case 2:                                              // 0x1001effc
            if (*p == 0x2F) finished = true;                 // 0x1001f000-0x1001f002
            ++p;                                             // 0x1001f00a
            {
                unsigned metaBytes = 0;
                const unsigned length = c3bd_inlineVarLen(p, &metaBytes);
                p += length + metaBytes;                     // 0x1001f026-0x1001f028
            }
            break;
        case 1: p += 1; break;                               // 0x1001f02c
        case 0: p += 2; break;                               // 0x1001f02f
        default: return 0;                                   // 0x1001f051
        }
        if (finished) break;                                 // je at 0x1001f038
    }
    *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + 0x54) = total;   // 0x1001f049
    return static_cast<int>(total);
}

// 0x00024920  Midi_seekTrackToTick (VA 0x10024920)
// __thiscall with two stack arguments ([esp+4] the track record, [esp+8] the tick limit; `ret 8` at
// 0x10024a22). The track bytes come from [track+0x3c] (0x1002492d); the running status (ebx), the consumed
// byte count (edi), the accumulated ticks ([esp+0x14]) and a "one event was consumed" flag ([esp+0x20]) all
// start at 0 (0x1002492b-0x1002493e).
// Each round calls Midi_readVarLen 0x1001e710 through the thunk at 0x1000139d (0x10024948) with the cursor and
// a byte-count slot, adds the delta to the ticks already accumulated (0x10024955) and stops when the sum is
// ABOVE the limit (`ja 0x10024a0e` at 0x1002495c, UNSIGNED). On that exit, a round that never completed
// returns 0x29 and writes nothing (0x10024a19); otherwise the cursor is stored at [track+0x2c], the consumed
// byte count at [track+0x40] and the ticks BEFORE this delta at [track+0x4c], and the function returns 0
// (0x10024a4d-0x10024a5f).
// Inside the limit the ticks are updated (0x10024962), the cursor before the delta is kept (0x1002496a), the
// cursor and the byte count advance past the delta (0x1002496c-0x1002496e) and the flag is set (0x10024970).
// A byte with bit 7 set becomes the running status (`je 0x1002499b` at 0x10024979): the whole byte is stored
// at [track+0x30] (0x1002498c), and when its high nibble is not 0xf0 (`je 0x10024999` at 0x1002498f) the low
// nibble is stored at [track+0x48] as the channel (0x10024994) and the running status becomes the high nibble
// (0x10024997); the cursor and the byte count then step over it (0x10024999-0x1002499a).
// A running status outside 0x80..0xff returns 0x12 (`ja 0x10024a3e` at 0x100249a1, exit at 0x10024a41).
// Otherwise the byte table at 0x10024a78 selects a case through the jump table at 0x10024a64
// (0x100249a7-0x100249af): case 0 skips two data bytes (0x10024a03), case 1 is the controller case
// (0x100249e5), case 2 skips one data byte (0x100249de), case 3 is the meta event (0x100249b6) and case 4
// returns 0x12 (0x10024a3e).
// The controller case stores the second data byte at [this + 0x254 + channel*0x4c] when the controller number
// is 7 (`jne 0x10024a03` at 0x100249e8, index `channel*19` at 0x100249f2-0x100249f5, store at 0x100249fc) and
// then skips the two data bytes (0x10024a03).
// The meta case ends the walk when the meta type is 0x2f: it stores the cursor kept before the delta at
// [track+0x2c] and the accumulated ticks at [track+0x4c] and returns 0x29 (`je 0x10024a25` at 0x100249b9, exit
// at 0x10024a25-0x10024a3b). Any other meta type steps over the type byte (0x100249bf), reads its payload
// length with Midi_readVarLen (0x100249c2) and advances the cursor by the length plus the length's own bytes
// and the byte count by one more (0x100249ce-0x100249d5).
typedef unsigned(__cdecl* MidiReadVarLen_t)(const unsigned char*, unsigned*);
typedef int(__fastcall* MidiSeekTrackToTick_t)(void*, void*, void*, unsigned);
MidiSeekTrackToTick_t MidiSeekTrackToTick_orig;

int __fastcall MidiSeekTrackToTick_re(void* self, void*, void* track, unsigned limit) {
    char* rec = reinterpret_cast<char*>(track);
    const unsigned char* p = *reinterpret_cast<const unsigned char* const*>(rec + 0x3C);   // 0x1002492d
    const unsigned char* table = reinterpret_cast<const unsigned char*>(c3bd_sound(0x00024A78));
    MidiReadVarLen_t readVarLen = reinterpret_cast<MidiReadVarLen_t>(c3bd_sound(0x0001E710));

    unsigned consumed = 0;                                   // edi, 0x1002492b
    unsigned status = 0;                                     // ebx, 0x10024930
    unsigned ticks = 0;                                      // [esp+0x14], 0x10024936
    bool started = false;                                    // [esp+0x20], 0x1002493e

    for (;;) {
        unsigned bytes = 0;                                  // [esp+0x10], 0x1002493a
        const unsigned delta = readVarLen(p, &bytes);        // call at 0x10024948
        const unsigned before = ticks;                       // ecx, 0x1002494d
        const unsigned total = delta + before;               // add at 0x10024955
        if (total > limit) {                                 // ja at 0x1002495c (UNSIGNED)
            if (!started) return 0x29;                       // 0x10024a19
            *reinterpret_cast<unsigned*>(rec + 0x40) = consumed;                     // 0x10024a4d
            *reinterpret_cast<const unsigned char**>(rec + 0x2C) = p;                // 0x10024a50
            *reinterpret_cast<unsigned*>(rec + 0x4C) = before;                       // 0x10024a54
            return 0;                                        // 0x10024a59
        }
        ticks = total;                                       // 0x10024962
        const unsigned char* eventStart = p;                 // 0x1002496a
        p += bytes;                                          // 0x1002496c
        consumed += bytes;                                   // 0x1002496e
        started = true;                                      // 0x10024970

        const unsigned char head = *p;                       // 0x10024975
        if ((head & 0x80u) != 0) {                           // je at 0x10024979
            const unsigned high = head & 0xF0u;              // 0x10024982
            status = head;                                   // 0x10024980
            rec[0x30] = static_cast<char>(head);             // 0x1002498c
            if (high != 0xF0u) {                             // je at 0x1002498f
                *reinterpret_cast<unsigned*>(rec + 0x48) = head & 0xFu;              // 0x10024994
                status = high;                               // 0x10024997
            }
            ++p;                                             // 0x10024999
            ++consumed;                                      // 0x1002499a
        }
        const unsigned index = status - 0x80u;               // lea at 0x1002499b
        if (index > 0x7Fu) return 0x12;                      // ja at 0x100249a1 (UNSIGNED)
        switch (table[index]) {
        case 3:                                              // 0x100249b6
            if (*p == 0x2F) {                                // je at 0x100249b9
                *reinterpret_cast<const unsigned char**>(rec + 0x2C) = eventStart;   // 0x10024a2a
                *reinterpret_cast<unsigned*>(rec + 0x4C) = ticks;                    // 0x10024a2d
                return 0x29;                                 // 0x10024a32
            }
            ++p;                                             // 0x100249bf
            {
                unsigned metaBytes = 0;
                const unsigned length = readVarLen(p, &metaBytes);                   // call at 0x100249c2
                p += length + metaBytes;                                             // 0x100249d1
                consumed += length + metaBytes + 1;                                  // lea at 0x100249d5
            }
            break;
        case 1:                                              // 0x100249e5
            if (*p == 7) {                                   // jne at 0x100249e8
                const unsigned channel = *reinterpret_cast<const unsigned*>(rec + 0x48);   // 0x100249ea
                const unsigned value = p[1];                 // 0x100249ef
                *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + 0x254
                                             + channel * 19u * 4u) = value;          // 0x100249fc
            }
            p += 2;                                          // 0x10024a03
            consumed += 2;                                   // 0x10024a06
            break;
        case 2:                                              // 0x100249de
            p += 1;
            consumed += 1;
            break;
        case 0:                                              // 0x10024a03
            p += 2;
            consumed += 2;
            break;
        default: return 0x12;                                // 0x10024a3e
        }
    }
}

SG_HOOK("sound.dll", 0x0003e520, sound_decodeTableSymbol, DecodeTableSymbol_re, DecodeTableSymbol_orig);
SG_HOOK("sound.dll", 0x0003e600, sound_substituteIfBelowThreshold, SubstituteIfBelowThreshold_re,
        SubstituteIfBelowThreshold_orig);
SG_HOOK("sound.dll", 0x0003d0a0, sound_applyIir2, ApplyIir2_re, ApplyIir2_orig);
SG_HOOK("sound.dll", 0x00040e30, sound_codecBiquadFilter, CodecBiquadFilter_re, CodecBiquadFilter_orig);
SG_HOOK("sound.dll", 0x00040330, sound_codecCorrelationLags, CodecCorrelationLags_re, CodecCorrelationLags_orig);
SG_HOOK("sound.dll", 0x000404d0, sound_codecSymmetricFir, CodecSymmetricFir_re, CodecSymmetricFir_orig);
SG_HOOK("sound.dll", 0x0003e9f0, sound_codecAutocorrelation, CodecAutocorrelation_re, CodecAutocorrelation_orig);
SG_HOOK("sound.dll", 0x0001ef80, sound_Midi_scanTrack, MidiScanTrack_re, MidiScanTrack_orig);
SG_HOOK("sound.dll", 0x00024920, sound_Midi_seekTrackToTick, MidiSeekTrackToTick_re, MidiSeekTrackToTick_orig);

}  // namespace
