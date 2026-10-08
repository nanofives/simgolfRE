// C3 batch c3bi (2026-10-08) of sound.dll: six speech-codec functions (five stage leaves of
// codecStageDispatch 0x1003c640 plus codecWindowFeatures, the analysis stage of 0x1003f720) and three
// sequencer / MIDI-device functions.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside
// each body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32 or COM call is made, nothing is allocated or freed, and no live sound object,
// device, list, buffer or module global is touched: every buffer, record and object the A/B passes in is
// allocated by the fixture. Four bodies forward to an original callee at its own address (floatCopySign
// 0x1003e380, floatFloorToInt 0x1003e400, Seq_listGetAt 0x1001d780, Voice_setLevel 0x100383b0 and
// Midi_seekTrackToTick 0x10024920); sound.dll is relocatable, so those addresses, the read-only tables and
// every float constant are resolved against the module base at the first call.
// Floating point: the codec bodies are x87 (/Od /GZ debug bodies, every intermediate spilled), the shim is
// SSE2. Each `fstp dword` spill is written as an assignment to a float and every operand inside one expression
// is widened with static_cast<double>, which is the c3ap method: the A/B thread runs with control word 0x027f
// (53-bit, round to nearest), so an x87 step that ends in a register is exactly a double operation and a step
// that ends in a dword slot is exactly a double operation rounded once to float. A `fst dword` that is not
// followed by a pop keeps the unrounded value in st(0) for the next use, so the comparison that follows it is
// written on the double expression and only the stored slot is a float.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fld(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T fldc(const void* base, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + off);
}

// sound.dll has base relocations, so an original callee, table or constant is addressed through the loaded base.
char* c3bi_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

float c3bi_f(unsigned rva) { return *reinterpret_cast<const float*>(c3bi_sound(rva)); }

// VC6's `x % 2` / `x % 16` on a signed int: `and x, 0x8000000f` then, when the sign bit survived,
// `dec / or 0xfffffff0 / inc` turns the two's-complement remainder into the negative one.
int c3bi_srem(int x, unsigned mask, unsigned fill) {
    int v = static_cast<int>(static_cast<unsigned>(x) & mask);
    if (v < 0) {                                        // jns
        --v;
        v = static_cast<int>(static_cast<unsigned>(v) | fill);
        ++v;
    }
    return v;
}

// ---------------------------------------------------------------------------------------------------------
// speech-codec leaves
// ---------------------------------------------------------------------------------------------------------

// 0x0003bf30  bitPermute (VA 0x1003bf30)
// __cdecl with seven stack arguments ([ebp+8] the mode, [ebp+0xc] the value count, [ebp+0x10] and [ebp+0x14]
// two extra value slots, [ebp+0x18] the value array, [ebp+0x1c] the bit array, [ebp+0x20] the codec context;
// `ret` at 0x1003c171 pops nothing), a /Od /GZ debug body.
// The value array and the bit array are decremented by one dword on entry (0x1003bf51, 0x1003bf5a), so both are
// one-based. The whole frame is filled with 0xcccccccc by the /GZ prologue (`rep stosd` at 0x1003bf4c), which
// covers the 14-slot work array the body indexes as `[ebp + k*4 - 0x40]` (k = 1..13) and, three dwords higher,
// as `[ebp + i*4 - 0x34]` (= slot i+3).
// `cmp [ebp-0x44], 1` / `je 0x1003bf6e` at 0x1003bf66 is the two-case switch VC6 writes with the equality test
// first: mode 1 is the UNPACK case at 0x1003c052, every other mode the PACK case at 0x1003bf73.
//
// PACK (0x1003bf73): slot 1 takes *[ebp+0x10] (0x1003bf81), slot 2 takes *[ebp+0x14] (0x1003bf89), slot 3 is
// zeroed (0x1003bf8e) and slot i+3 takes `values[count + 1 - i] & 0x7fff` for i = 1..*count, so the values are
// copied in REVERSE and masked to 15 bits (0x1003bfb7-0x1003bfd0). The 53-entry dword table at 0x1006447c then
// names one slot per output bit: for i = 1..53 the bit array gets `slot[table[i]] & 1` (0x1003c005) and that
// slot is divided by two (`cdq` / `sub eax, edx` / `sar eax, 1` at 0x1003c016-0x1003c019, a SIGNED division, so
// slots 1 and 2 may be negative). The 54th output dword, at byte offset 0xd8 of the decremented bit array, is
// bit 0 of the context's dword at +0x2540 (0x1003c036) and that dword is then replaced by `1 - itself`
// (0x1003c044, 0x1003c049). Returns 0 (0x1003c04b).
//
// UNPACK (0x1003c052): slots 1..13 are zeroed (0x1003c06a), then for i = 1..53 the same table is walked
// BACKWARDS (`54 - i` at 0x1003c08f) and each named slot becomes `bits[54-i] + slot*2` (`lea ecx, [eax+edx*2]`
// at 0x1003c0b0), i.e. the bits are shifted back in most significant first. Slot i+3 is then sign-extended out
// of its field for i = 1..*count: when it still carries the sign bit of the 10-entry dword table at 0x10064454
// (`and ecx, table[i]` at 0x1003c0f4) twice that mask is subtracted (0x1003c109, 0x1003c112). Slot 1 is written
// back to *[ebp+0x10] and slot 2 to *[ebp+0x14] (0x1003c123, 0x1003c12b), and the value array is refilled in
// reverse, `values[i] = slot[count + 4 - i]` (0x1003c154-0x1003c164). Returns 0 (0x1003c169).
typedef int(__cdecl* BitPermute_t)(int, const int*, int*, int*, int*, int*, void*);
BitPermute_t BitPermute_orig;
int __cdecl BitPermute_re(int mode, const int* count, int* slot1Out, int* slot2Out,
                          int* values, int* bits, void* context) {
    int work[14];
    for (int f = 0; f < 14; ++f) work[f] = static_cast<int>(0xCCCCCCCCu);   // rep stosd at 0x1003bf4c

    values -= 1;                                             // sub at 0x1003bf51
    bits -= 1;                                               // sub at 0x1003bf5a
    const int* order = reinterpret_cast<const int*>(c3bi_sound(0x0006447C));
    const int* signMask = reinterpret_cast<const int*>(c3bi_sound(0x00064454));

    if (mode == 1) {                                         // je at 0x1003bf6a
        for (int k = 1; k <= 13; ++k) work[k] = 0;           // 0x1003c06a
        for (int i = 1; i <= 53; ++i) {                      // jg at 0x1003c08d
            const int j = 0x36 - i;                          // 0x1003c08f
            const int k = order[j];                          // 0x1003c097
            work[k] = bits[j] + work[k] * 2;                 // lea at 0x1003c0b0
        }
        int n = *count;                                      // 0x1003c0cb
        for (int i = 1; i <= n; ++i) {                       // jg at 0x1003c0e8
            if ((work[i + 3] & signMask[i]) != 0)            // je at 0x1003c0fd
                work[i + 3] -= signMask[i] * 2;              // 0x1003c109, 0x1003c112
        }
        *slot1Out = work[1];                                 // 0x1003c123
        *slot2Out = work[2];                                 // 0x1003c12b
        n = *count;                                          // 0x1003c130
        for (int i = 1; i <= n; ++i)                         // jg at 0x1003c14d
            values[i] = work[*count + 4 - i];                // 0x1003c154-0x1003c164
        return 0;                                            // 0x1003c169
    }

    int* toggle = reinterpret_cast<int*>(reinterpret_cast<char*>(context) + 0x2540);   // 0x1003bf76
    work[1] = *slot1Out;                                     // 0x1003bf81
    work[2] = *slot2Out;                                     // 0x1003bf89
    work[3] = 0;                                             // 0x1003bf8e
    int n = *count;                                          // 0x1003bf98
    for (int i = 1; i <= n; ++i)                             // jg at 0x1003bfb5
        work[i + 3] = values[*count + 1 - i] & 0x7FFF;       // 0x1003bfc8, 0x1003bfd0
    for (int i = 1; i <= 53; ++i) {                          // jg at 0x1003bfec
        const int k = order[i];                              // 0x1003bff1
        bits[i] = work[k] & 1;                               // 0x1003bffc, 0x1003c005
        work[k] = work[k] / 2;                               // cdq/sub/sar at 0x1003c016
    }
    bits[0x36] = *toggle & 1;                                // 0x1003c030, 0x1003c036
    *toggle = 1 - *toggle;                                   // 0x1003c044, 0x1003c049
    return 0;                                                // 0x1003c04b
}

// 0x0003e6d0  gaussSolveClamped (VA 0x1003e6d0)
// __cdecl with four stack arguments ([ebp+8] the order, [ebp+0xc] the packed matrix, [ebp+0x10] the right-hand
// side, [ebp+0x14] the solution; `ret` at 0x1003e9ef pops nothing), a /Od /GZ debug body.
// The solution and the right-hand side are decremented by one float (0x1003e6f1, 0x1003e6fa) and the matrix by
// `order + 1` floats (0x1003e70b-0x1003e71c), so every index is one-based. The work matrix is the frame slot at
// [ebp-0x1e8], indexed `row * 10 + column` (`imul eax, eax, 0xa` at 0x1003e7b6 and everywhere else), so the
// order never exceeds 10.
// For each row i = 1..*order (`jg 0x1003e9b3` at 0x1003e73f) the row is first copied out of the packed matrix,
// `a[i][j] = matrix[j + i * order]` for j = i..*order (0x1003e766-0x1003e783, the row stride is the order read
// at entry, 0x1003e703). Rows 1..i-1 are then eliminated: the factor `a[k][i] * a[k][k]` is spilled to a float
// (0x1003e7c9-0x1003e7d7) and `a[i][j] -= factor * a[k][j]` runs over j = i..*order (0x1003e811-0x1003e82d).
// The pivot a[i][i] is rejected when its magnitude is at or below the float 1e-10 at 0x1005ec40 (`fcomp` at
// 0x1003e87c, `test ah, 1` / `je 0x1003e88e` at 0x1003e884, the magnitude taken with `fchs` at 0x1003e86e when
// the pivot is below the float 0.0f at 0x1005ec00): the solution is then zeroed from the current row to the end
// (0x1003e9d8-0x1003e9de) and the function returns 0 (0x1003e9e7).
// Otherwise the right-hand side is copied into the solution as a raw dword (0x1003e89d), the rows already
// solved are back-substituted, `out[i] -= out[k] * a[k][i]` for k = 1..i-1 (0x1003e8d4-0x1003e8ed), the pivot is
// inverted in place with the float 1.0f at 0x1005b8a8 (`fdiv` at 0x1003e903, stored at 0x1003e915), the
// solution is scaled by it (0x1003e92d-0x1003e93d) and finally clamped: strictly above the float 0.9990000f at
// 0x1005ec3c it becomes the dword 0x3f7fbe77 (`test ah, 0x41` / `je 0x1003e967` at 0x1003e957), and strictly
// below the float -0.9990000f at 0x1005ec38 it becomes 0xbf7fbe77 (`test ah, 1` / `jne 0x1003e995` at
// 0x1003e985). Returns 0 (0x1003e9b3).
typedef int(__cdecl* GaussSolveClamped_t)(const int*, const float*, const float*, float*);
GaussSolveClamped_t GaussSolveClamped_orig;
int __cdecl GaussSolveClamped_re(const int* order, const float* matrix, const float* rhs, float* out) {
    float a[111];                                            // the frame slot at [ebp-0x1e8]
    for (int f = 0; f < 111; ++f)
        *reinterpret_cast<unsigned*>(&a[f]) = 0xCCCCCCCCu;   // rep stosd at 0x1003e6ec

    out -= 1;                                                // 0x1003e6f1
    rhs -= 1;                                                // 0x1003e6fa
    const int stride = *order;                               // 0x1003e703
    matrix -= (stride + 1);                                  // 0x1003e70b-0x1003e71c

    const float zero = c3bi_f(0x0005EC00);
    const float pivotFloor = c3bi_f(0x0005EC40);
    const float one = c3bi_f(0x0005B8A8);
    const float upper = c3bi_f(0x0005EC3C);
    const float lower = c3bi_f(0x0005EC38);

    const int n = *order;                                    // 0x1003e722
    for (int i = 1; i <= n; ++i) {                           // jg at 0x1003e73f
        int m = *order;                                      // 0x1003e748
        for (int j = i; j <= m; ++j)                         // jg at 0x1003e764
            a[j + i * 10] = matrix[j + i * stride];          // 0x1003e783

        m = i - 1;                                           // 0x1003e78f
        for (int k = 1; k <= m; ++k) {                       // jg at 0x1003e7ad
            const float factor = static_cast<float>(
                static_cast<double>(a[i + k * 10]) * static_cast<double>(a[k + k * 10]));  // 0x1003e7d7
            const int inner = *order;                        // 0x1003e7dd
            for (int j = i; j <= inner; ++j)                 // jg at 0x1003e7f9
                a[j + i * 10] = static_cast<float>(
                    static_cast<double>(a[j + i * 10])
                    - static_cast<double>(factor) * static_cast<double>(a[j + k * 10]));   // 0x1003e82d
        }

        const float pivot = a[i + i * 10];                   // 0x1003e846
        const float magnitude = (static_cast<double>(pivot) < static_cast<double>(zero))
                                    ? -pivot : pivot;        // fchs at 0x1003e86e
        if (!(static_cast<double>(magnitude) >= static_cast<double>(pivotFloor))) {   // je at 0x1003e887
            const int tail = *order;                         // 0x1003e9ba
            for (int j = i; j <= tail; ++j)                  // jg at 0x1003e9d6
                *reinterpret_cast<unsigned*>(&out[j]) = 0;   // 0x1003e9de
            return 0;                                        // 0x1003e9e7
        }

        *reinterpret_cast<unsigned*>(&out[i]) = *reinterpret_cast<const unsigned*>(&rhs[i]);  // 0x1003e89d
        m = i - 1;                                           // 0x1003e8a3
        for (int k = 1; k <= m; ++k)                         // jg at 0x1003e8c1
            out[i] = static_cast<float>(
                static_cast<double>(out[i])
                - static_cast<double>(out[k]) * static_cast<double>(a[i + k * 10]));   // 0x1003e8ed
        a[i + i * 10] = static_cast<float>(
            static_cast<double>(one) / static_cast<double>(a[i + i * 10]));            // 0x1003e915
        out[i] = static_cast<float>(
            static_cast<double>(out[i]) * static_cast<double>(a[i + i * 10]));         // 0x1003e93d

        float high = out[i];                                 // 0x1003e946
        if (static_cast<double>(high) > static_cast<double>(upper))                    // je at 0x1003e95a
            *reinterpret_cast<unsigned*>(&high) = 0x3F7FBE77u;                         // 0x1003e967
        float low = high;                                    // 0x1003e977
        if (static_cast<double>(low) < static_cast<double>(lower))                     // jne at 0x1003e988
            *reinterpret_cast<unsigned*>(&low) = 0xBF7FBE77u;                          // 0x1003e995
        out[i] = low;                                                                  // 0x1003e9ab
    }
    return 0;                                                                          // 0x1003e9b3
}

// 0x000414f0  codecAmdfPitch (VA 0x100414f0)
// __cdecl with eight stack arguments ([ebp+8] the samples, [ebp+0xc] the window start, [ebp+0x10] the lag
// array, [ebp+0x14] the lag count, [ebp+0x18] the window length, [ebp+0x1c] the result array, [ebp+0x20] the
// index of the smallest result, [ebp+0x24] the index of the largest; `ret` at 0x1004164e pops nothing).
// The result array, the lag array and the samples are each decremented by one dword on entry (0x1004150b,
// 0x10041514, 0x1004151d), so every index is one-based; both index slots start at 1 (0x10041526, 0x1004152f).
// For each lag i = 1..*[ebp+0x14] (`jg 0x10041646` at 0x10041555) the scan starts at
// `((*window - lag[i]) / 2) + 1` (a SIGNED division, `cdq` / `sub eax, edx` / `sar eax, 1` at
// 0x10041569-0x1004156e) and runs to `start + *[ebp+0xc] - 1` (0x1004157c) in steps of FOUR (`add eax, 4` at
// 0x1004159b, `jg 0x100415ef` at 0x100415a7), accumulating `|samples[j] - samples[j + lag[i]]|` into a float
// (0x100415be-0x100415ea). The difference is kept unrounded in st(0) by `fst dword [ebp-0xc]` (0x100415c4)
// while it is compared with the float 0.0f at 0x1005ec00, and the magnitude is the stored float, negated with
// `fchs` (0x100415df) when the difference is below zero.
// The sum becomes result[i] (0x100415f8). result[i] strictly below result[*smallest] moves the smallest index
// (`test ah, 1` / `je 0x1004161e` at 0x10041611, store at 0x1004161c) and result[i] strictly above
// result[*largest] moves the largest index (`test ah, 0x41` / `jne 0x10041641` at 0x10041634, store at
// 0x1004163f). Returns 0 (0x10041646).
typedef int(__cdecl* CodecAmdfPitch_t)(const float*, const int*, const int*, const int*, const int*,
                                       float*, int*, int*);
CodecAmdfPitch_t CodecAmdfPitch_orig;
int __cdecl CodecAmdfPitch_re(const float* samples, const int* span, const int* lags, const int* lagCount,
                              const int* window, float* results, int* smallest, int* largest) {
    results -= 1;                                            // 0x1004150b
    lags -= 1;                                               // 0x10041514
    samples -= 1;                                            // 0x1004151d
    *smallest = 1;                                           // 0x10041526
    *largest = 1;                                            // 0x1004152f

    const float zero = c3bi_f(0x0005EC00);
    const int n = *lagCount;                                 // 0x10041538
    for (int i = 1; i <= n; ++i) {                           // jg at 0x10041555
        const int start = ((*window - lags[i]) / 2) + 1;     // 0x10041566-0x1004156e
        const int end = start + *span - 1;                   // 0x1004157c
        float acc = 0.0f;                                    // 0x10041583
        for (int j = start; j <= end; j += 4) {              // jg at 0x100415a7, add 4 at 0x1004159b
            const int k = j + lags[i];                       // 0x100415b2
            const double diff = static_cast<double>(samples[j]) - static_cast<double>(samples[k]);
            const float stored = static_cast<float>(diff);   // fst dword at 0x100415c4
            const float magnitude = (diff < static_cast<double>(zero)) ? -stored : stored;  // 0x100415df
            acc = static_cast<float>(static_cast<double>(acc) + static_cast<double>(magnitude));  // 0x100415ea
        }
        results[i] = acc;                                    // 0x100415f8

        if (static_cast<double>(results[i]) < static_cast<double>(results[*smallest]))   // je at 0x10041614
            *smallest = i;                                   // 0x1004161c
        if (static_cast<double>(results[i]) > static_cast<double>(results[*largest]))    // jne at 0x10041637
            *largest = i;                                    // 0x1004163f
    }
    return 0;                                                // 0x10041646
}

// 0x00040ab0  codecMarkCorrelationJumps (VA 0x10040ab0)
// __cdecl with eight stack arguments ([ebp+8] the samples, [ebp+0xc] the jump index array, [ebp+0x10] the jump
// count, [ebp+0x14] the jump capacity, [ebp+0x18] the sample offset, [ebp+0x1c] the last sample index,
// [ebp+0x20] the window length, [ebp+0x24] the codec context; `ret` at 0x10040d9c pops nothing).
// Nine context slots are bound on entry (0x10040ace-0x10040b31): the smoothed lag-1 product at +0x21c4, the
// smoothed energy at +0x21c8, their ratio at +0x21cc, a 16-slot ring at +0x21d0, the ring's running sum at
// +0x2210, two ring indices at +0x2214 and +0x2218 (called A and B below), the last jump position at +0x221c
// and the open-event flag at +0x2220.
// The jump index array is decremented by one dword when it is not null (`je 0x10040b43` at 0x10040b38) and the
// samples by *offset dwords when they are not null (`je 0x10040b5f` at 0x10040b47, 0x10040b54-0x10040b5a).
// While an event is open the last jump position is rebased by the window length (`je 0x10040b76` at 0x10040b65,
// `sub` at 0x10040b6f).
// Each sample j from `*last - *window + 1` to *last (0x10040b88, `jg 0x10040d87` at 0x10040b9f):
//   * the product `samples[j] * samples[j-1]` is smoothed into +0x21c4 with the float 63.0f at 0x1005ec68 and
//     the float 64.0f at 0x1005ebfc (0x10040bb1-0x10040bce);
//   * `samples[j-1]` squared is smoothed into +0x21c8 the same way (0x10040bdd-0x10040bf9);
//   * when the energy is not exactly the float 0.0f at 0x1005ec00 (`test ah, 0x40` / `jne 0x10040c69` at
//     0x10040c08) the ratio is `product / energy` (0x10040c60) unless the product's magnitude is strictly above
//     the energy (`test ah, 0x41` / `jne 0x10040c5a` at 0x10040c3d), in which case it is
//     floatCopySign(0x10064e3c, product) (0x10040c4b), the float 1.0f at 0x10064e3c carrying the product's sign;
//   * the running sum becomes `sum - ring[B] + ratio` (0x10040c7b-0x10040c91), ring[B] takes the new sum
//     (0x10040ca0), ring[A] takes the ratio (0x10040cb1), and both indices advance as `(index % 16) + 1`
//     (0x10040cb8-0x10040ccc, 0x10040cd1-0x10040ce5);
//   * the move of the running sum, `sum - ring[A] as it was before this round` (0x10040cea, `fst` at 0x10040cef
//     keeps it unrounded for the compare with 0.0f), is taken in magnitude (`fchs` at 0x10040d0a) and compared
//     with the float 1.7f at 0x1005ecac (`test ah, 0x41` / `jne 0x10040d64` at 0x10040d1a). Strictly above it,
//     a closed event appends `j - 9` to the index array and bumps the count when the count is at or below the
//     capacity (`jg 0x10040d51` at 0x10040d31, 0x10040d41, 0x10040d4f), sets the open flag (0x10040d54) and
//     records j as the last jump position (0x10040d60). At or below it, an open event closes once j is at least
//     10 samples past the last jump (`jl 0x10040d82` at 0x10040d77, store at 0x10040d7c).
// Returns 0 (0x10040d87).
typedef float(__cdecl* FloatCopySign_t)(const float*, const float*);
typedef int(__cdecl* CodecMarkCorrelationJumps_t)(const float*, int*, int*, const int*, const int*,
                                                  const int*, const int*, void*);
CodecMarkCorrelationJumps_t CodecMarkCorrelationJumps_orig;
int __cdecl CodecMarkCorrelationJumps_re(const float* samples, int* jumps, int* jumpCount,
                                         const int* jumpCapacity, const int* offset, const int* last,
                                         const int* window, void* context) {
    char* ctx = reinterpret_cast<char*>(context);
    float* product = reinterpret_cast<float*>(ctx + 0x21C4);     // 0x10040ace
    float* energy = reinterpret_cast<float*>(ctx + 0x21C8);      // 0x10040ad9
    float* ratio = reinterpret_cast<float*>(ctx + 0x21CC);       // 0x10040ae5
    float* ring = reinterpret_cast<float*>(ctx + 0x21D0);        // 0x10040af1
    float* sum = reinterpret_cast<float*>(ctx + 0x2210);         // 0x10040afc
    int* indexA = reinterpret_cast<int*>(ctx + 0x2214);          // 0x10040b08
    int* indexB = reinterpret_cast<int*>(ctx + 0x2218);          // 0x10040b14
    int* lastJump = reinterpret_cast<int*>(ctx + 0x221C);        // 0x10040b1f
    int* openFlag = reinterpret_cast<int*>(ctx + 0x2220);        // 0x10040b2b

    if (jumps != 0) jumps -= 1;                                  // je at 0x10040b38
    if (samples != 0) samples -= *offset;                        // je at 0x10040b47, 0x10040b54
    if (*openFlag != 0) *lastJump -= *window;                    // je at 0x10040b65, 0x10040b6f

    const float k63 = c3bi_f(0x0005EC68);
    const float k64 = c3bi_f(0x0005EBFC);
    const float zero = c3bi_f(0x0005EC00);
    const float moveLimit = c3bi_f(0x0005ECAC);
    const float* one = reinterpret_cast<const float*>(c3bi_sound(0x00064E3C));
    FloatCopySign_t copySign = reinterpret_cast<FloatCopySign_t>(c3bi_sound(0x0003E380));

    const int end = *last;                                       // 0x10040b79
    for (int j = *last - *window + 1; j <= end; ++j) {           // 0x10040b88, jg at 0x10040b9f
        *product = static_cast<float>(
            (static_cast<double>(samples[j]) * static_cast<double>(samples[j - 1])
             + static_cast<double>(*product) * static_cast<double>(k63))
            / static_cast<double>(k64));                         // 0x10040bb1-0x10040bce
        const float prev = samples[j - 1];                       // 0x10040bd6
        *energy = static_cast<float>(
            (static_cast<double>(prev) * static_cast<double>(prev)
             + static_cast<double>(*energy) * static_cast<double>(k63))
            / static_cast<double>(k64));                         // 0x10040bdd-0x10040bf9

        if (static_cast<double>(*energy) != static_cast<double>(zero)) {          // jne at 0x10040c0b
            const float magnitude = (static_cast<double>(*product) < static_cast<double>(zero))
                                        ? -*product : *product;                   // fchs at 0x10040c2e
            if (static_cast<double>(magnitude) > static_cast<double>(*energy))    // jne at 0x10040c40
                *ratio = copySign(one, product);                                  // 0x10040c4b
            else
                *ratio = static_cast<float>(
                    static_cast<double>(*product) / static_cast<double>(*energy)); // 0x10040c62
        }

        const float before = ring[*indexA - 1];                                    // 0x10040c6c-0x10040c75
        *sum = static_cast<float>(
            (static_cast<double>(*sum) - static_cast<double>(ring[*indexB - 1]))
            + static_cast<double>(*ratio));                                        // 0x10040c7b-0x10040c91
        ring[*indexB - 1] = *sum;                                                  // 0x10040c96-0x10040ca0
        ring[*indexA - 1] = *ratio;                                                // 0x10040ca7-0x10040cb1
        *indexA = c3bi_srem(*indexA, 0x8000000Fu, 0xFFFFFFF0u) + 1;                // 0x10040cb8-0x10040ccc
        *indexB = c3bi_srem(*indexB, 0x8000000Fu, 0xFFFFFFF0u) + 1;                // 0x10040cd1-0x10040ce5

        const double move = static_cast<double>(*sum) - static_cast<double>(before);  // 0x10040cea
        const float moveStored = static_cast<float>(move);                            // fst dword at 0x10040cef
        const float moveMagnitude = (move < static_cast<double>(zero))
                                        ? -moveStored : moveStored;                   // fchs at 0x10040d0a
        if (static_cast<double>(moveMagnitude) > static_cast<double>(moveLimit)) {    // jne at 0x10040d1d
            if (*openFlag == 0) {                                                     // jne at 0x10040d25
                if (*jumpCount <= *jumpCapacity) {                                    // jg at 0x10040d31
                    jumps[*jumpCount] = j - 9;                                        // 0x10040d36, 0x10040d41
                    *jumpCount += 1;                                                  // 0x10040d49, 0x10040d4f
                }
                *openFlag = 1;                                                        // 0x10040d54
            }
            *lastJump = j;                                                            // 0x10040d60
        } else if (*openFlag != 0 && (j - *lastJump) >= 0xA) {                        // je at 0x10040d6a, jl at 0x10040d77
            *openFlag = 0;                                                            // 0x10040d7c
        }
    }
    return 0;                                                                         // 0x10040d87
}

// 0x00041020  codecWindowFeatures (VA 0x10041020)
// __cdecl with fourteen stack arguments ([ebp+8] the frame range, [ebp+0xc] the first sample array, [ebp+0x10]
// the second sample array, [ebp+0x14] the rebase descriptor, [ebp+0x18] the sub-frame number, [ebp+0x1c] the
// dither level, [ebp+0x20] the lag, [ebp+0x24] the sign-change count, [ebp+0x28] and [ebp+0x2c] two clamped
// level outputs, [ebp+0x30], [ebp+0x34] and [ebp+0x38], [ebp+0x3c] four float outputs; `ret` at 0x100414e3 pops
// nothing).
// The frame range and the descriptor are decremented by one dword (0x10041041, 0x1004104a); the second array is
// then rebased by the descriptor's dword at +0xc and the first by its dword at +4 (0x10041053-0x1004107b), so
// both arrays are one-based in the frame's own numbering. Ten accumulators, the sign-change count at [ebp+0x24]
// and the float output at [ebp+0x34] all start at zero (0x1004107e-0x100410c9).
// The window length is `range[8] - range[4] + 1` (0x100410cf-0x100410de); the sub-frame starts at
// `range[4] + ((subframe - 1) * window) / 2 + 1` and ends half a window later (0x100410e1-0x1004110e, both
// divisions SIGNED). The dither's running sign starts as floatCopySign(0x10064e40, first[start-1] - *dither)
// (0x1004111a-0x10041134), the float 1.0f at 0x10064e40 carrying that difference's sign.
// Each sample i of the sub-frame (`jg 0x10041315` at 0x10041154) adds |second[i]| to [ebp-0x38] (0x10041186),
// |first[i]| to [ebp-0x2c] (0x100411bb), |first[i] - first[i-1]| to [ebp-0x24] (0x100411f7, the difference kept
// unrounded by `fst dword` at 0x100411d7), first[i] squared to [ebp-0x4c] (0x10041212), first[i] * first[i-1]
// to the output at [ebp+0x34] (0x1004122e), second[i] squared to [ebp-0x30] (0x10041247), second[i - lag]
// squared to [ebp-0x3c] (0x10041264), second[i + lag] squared to [ebp-0x40] (0x10041281),
// second[i] * second[i + lag] to [ebp-0x48] (0x1004129e) and second[i] * second[i - lag] to [ebp-0x44]
// (0x100412bb). The dithered sample `first[i] + *dither` is passed to floatCopySign (0x100412db) and, when its
// sign differs from the running sign (`fcompp` at 0x100412e6, `test ah, 0x40` / `jne 0x10041304` at 0x100412ea),
// the sign-change count is incremented and the running sign flipped (0x100412f4, 0x100412ff). The dither itself
// is negated every sample (0x10041309).
// After the loop every divisor below the float 1.0f at 0x1005b8a8 is replaced by the dword 0x3f800000
// (0x1004132d, 0x1004135e, 0x10041388, 0x100413a7, 0x100413d9, 0x100413f8). The output at [ebp+0x34] is divided
// by the energy of the first array (0x10041339); [ebp+0x30] gets the mean absolute step over twice the mean
// absolute level (0x10041344-0x1004136e, `fadd st(0), st(0)` at 0x10041344 doubling it and `fst dword` keeping
// it unrounded); [ebp+0x38] and [ebp+0x3c] get the squared normalised correlations at -lag and +lag, each
// written as two separate divisions multiplied together (0x100413ae-0x100413bf, 0x100413ff-0x10041410).
// The crossing rate is `(2 * count) * (90.0f / window)` with the float 90.0f at 0x1005ec30, rounded by
// floatFloorToInt 0x1003e400 (0x1004141c-0x10041440), and the two levels are each accumulator divided by the
// float 4.0f at 0x1005ec20, scaled the same way, rounded the same way and clamped to 0x7fff
// (0x10041442-0x100414cc). Returns 0 (0x100414ce).
typedef int(__cdecl* FloatFloorToInt_t)(const float*);
typedef int(__cdecl* CodecWindowFeatures_t)(const int*, const float*, const float*, const int*, const int*,
                                            float*, const int*, int*, int*, int*, float*, float*, float*,
                                            float*);
CodecWindowFeatures_t CodecWindowFeatures_orig;
int __cdecl CodecWindowFeatures_re(const int* range, const float* first, const float* second,
                                   const int* descriptor, const int* subFrame, float* dither,
                                   const int* lag, int* crossings, int* level1, int* level2,
                                   float* stepRatio, float* lagProduct, float* corrMinus,
                                   float* corrPlus) {
    range = range - 1;                                       // 0x10041041
    descriptor = descriptor - 1;                             // 0x1004104a
    second -= descriptor[3];                                 // [descriptor+0xc], 0x10041053-0x10041064
    first -= descriptor[1];                                  // [descriptor+4],   0x1004106a-0x1004107b

    float accSecondAbs = 0.0f;                               // [ebp-0x38] 0x1004107e
    float accFirstAbs = 0.0f;                                // [ebp-0x2c] 0x10041085
    float accStepAbs = 0.0f;                                 // [ebp-0x24] 0x1004108c
    float accFirstSq = 0.0f;                                 // [ebp-0x4c] 0x10041093
    *lagProduct = 0.0f;                                      // 0x1004109d
    float accSecondSq = 0.0f;                                // [ebp-0x30] 0x100410a3
    float accMinusSq = 0.0f;                                 // [ebp-0x3c] 0x100410aa
    float accPlusSq = 0.0f;                                  // [ebp-0x40] 0x100410b1
    float accPlusProd = 0.0f;                                // [ebp-0x48] 0x100410b8
    float accMinusProd = 0.0f;                               // [ebp-0x44] 0x100410bf
    *crossings = 0;                                          // 0x100410c9

    const float zero = c3bi_f(0x0005EC00);
    const float one = c3bi_f(0x0005B8A8);
    const float k90 = c3bi_f(0x0005EC30);
    const float k4 = c3bi_f(0x0005EC20);
    const float* signOne = reinterpret_cast<const float*>(c3bi_sound(0x00064E40));
    FloatCopySign_t copySign = reinterpret_cast<FloatCopySign_t>(c3bi_sound(0x0003E380));
    FloatFloorToInt_t floorToInt = reinterpret_cast<FloatFloorToInt_t>(c3bi_sound(0x0003E400));

    const int window = range[2] - range[1] + 1;              // 0x100410d5-0x100410de
    const int start = range[1] + (((*subFrame - 1) * window) / 2) + 1;   // 0x100410e1-0x100410fc
    const int end = start + (window / 2) - 1;                            // 0x100410ff-0x1004110e

    float scratch = static_cast<float>(
        static_cast<double>(first[start - 1]) - static_cast<double>(*dither));   // 0x1004111a-0x10041120
    float runningSign = copySign(signOne, &scratch);                             // 0x1004112c-0x10041134

    for (int i = start; i <= end; ++i) {                     // jg at 0x10041154
        float v = second[i];                                 // 0x10041160
        float magnitude = (static_cast<double>(v) < static_cast<double>(zero)) ? -v : v;  // 0x10041181
        accSecondAbs = static_cast<float>(
            static_cast<double>(accSecondAbs) + static_cast<double>(magnitude));          // 0x1004118c

        v = first[i];                                                                     // 0x10041195
        magnitude = (static_cast<double>(v) < static_cast<double>(zero)) ? -v : v;         // 0x100411b6
        accFirstAbs = static_cast<float>(
            static_cast<double>(accFirstAbs) + static_cast<double>(magnitude));            // 0x100411c1

        const double step = static_cast<double>(first[i]) - static_cast<double>(first[i - 1]);  // 0x100411d3
        const float stepStored = static_cast<float>(step);                                 // fst dword at 0x100411d7
        magnitude = (step < static_cast<double>(zero)) ? -stepStored : stepStored;         // 0x100411f2
        accStepAbs = static_cast<float>(
            static_cast<double>(accStepAbs) + static_cast<double>(magnitude));             // 0x100411fd

        v = first[i];                                                                      // 0x10041206
        accFirstSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accFirstSq));   // 0x10041215
        *lagProduct = static_cast<float>(
            static_cast<double>(first[i]) * static_cast<double>(first[i - 1])
            + static_cast<double>(*lagProduct));                                           // 0x10041224-0x10041233

        v = second[i];                                                                     // 0x1004123b
        accSecondSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accSecondSq));  // 0x1004124a
        v = second[i - *lag];                                                              // 0x10041258
        accMinusSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accMinusSq));   // 0x10041267
        v = second[i + *lag];                                                              // 0x10041275
        accPlusSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accPlusSq));    // 0x10041284
        accPlusProd = static_cast<float>(
            static_cast<double>(second[i]) * static_cast<double>(second[i + *lag])
            + static_cast<double>(accPlusProd));                                           // 0x10041298-0x100412a1
        accMinusProd = static_cast<float>(
            static_cast<double>(second[i]) * static_cast<double>(second[i - *lag])
            + static_cast<double>(accMinusProd));                                          // 0x100412b5-0x100412be

        scratch = static_cast<float>(
            static_cast<double>(first[i]) + static_cast<double>(*dither));                 // 0x100412ca-0x100412cf
        const float dithered = copySign(signOne, &scratch);                                // 0x100412db
        if (static_cast<double>(dithered) != static_cast<double>(runningSign)) {           // jne at 0x100412ed
            *crossings += 1;                                                               // 0x100412f4-0x100412fa
            runningSign = -runningSign;                                                    // 0x100412ff
        }
        *dither = -*dither;                                                                // 0x10041309
    }

    const float energyDiv = (static_cast<double>(accFirstSq) < static_cast<double>(one))
                                ? one : accFirstSq;                                        // jne at 0x10041323
    *lagProduct = static_cast<float>(
        static_cast<double>(*lagProduct) / static_cast<double>(energyDiv));                // 0x10041339

    const double twiceLevel = static_cast<double>(accFirstAbs) + static_cast<double>(accFirstAbs);  // 0x10041344
    const float twiceStored = static_cast<float>(twiceLevel);                              // fst dword at 0x10041346
    const float stepDiv = (twiceLevel < static_cast<double>(one)) ? one : twiceStored;     // jne at 0x10041354
    *stepRatio = static_cast<float>(
        static_cast<double>(accStepAbs) / static_cast<double>(stepDiv));                   // 0x10041368

    const float minusDiv = (static_cast<double>(accMinusSq) < static_cast<double>(one))
                               ? one : accMinusSq;                                         // jne at 0x1004137e
    const float ownDivA = (static_cast<double>(accSecondSq) < static_cast<double>(one))
                              ? one : accSecondSq;                                         // jne at 0x1004139d
    *corrMinus = static_cast<float>(
        (static_cast<double>(accMinusProd) / static_cast<double>(minusDiv))
        * (static_cast<double>(accMinusProd) / static_cast<double>(ownDivA)));             // 0x100413ae-0x100413bf

    const float plusDiv = (static_cast<double>(accPlusSq) < static_cast<double>(one))
                              ? one : accPlusSq;                                           // jne at 0x100413cf
    const float ownDivB = (static_cast<double>(accSecondSq) < static_cast<double>(one))
                              ? one : accSecondSq;                                         // jne at 0x100413ee
    *corrPlus = static_cast<float>(
        (static_cast<double>(accPlusProd) / static_cast<double>(plusDiv))
        * (static_cast<double>(accPlusProd) / static_cast<double>(ownDivB)));              // 0x100413ff-0x10041410

    const int doubled = *crossings * 2;                                                    // 0x10041417
    const float doubledF = static_cast<float>(doubled);                                    // fild/fstp at 0x1004141c
    scratch = static_cast<float>(
        (static_cast<double>(k90) / static_cast<double>(window)) * static_cast<double>(doubledF));  // 0x10041422-0x1004142e
    *crossings = floorToInt(&scratch);                                                     // 0x10041435, 0x10041440

    scratch = static_cast<float>(
        (static_cast<double>(accSecondAbs) / static_cast<double>(k4))
        * (static_cast<double>(k90) / static_cast<double>(window)));                       // 0x10041442-0x10041456
    int rounded = floorToInt(&scratch);                                                    // 0x1004145d
    *level1 = (rounded > 0x7FFF) ? 0x7FFF : rounded;                                       // jg at 0x1004146f

    scratch = static_cast<float>(
        (static_cast<double>(accFirstAbs) / static_cast<double>(k4))
        * (static_cast<double>(k90) / static_cast<double>(window)));                       // 0x10041488-0x1004149c
    rounded = floorToInt(&scratch);                                                        // 0x100414a3
    *level2 = (rounded > 0x7FFF) ? 0x7FFF : rounded;                                       // jg at 0x100414b5
    return 0;                                                                              // 0x100414ce
}

// 0x0003f310  codecGainSmoother (VA 0x1003f310)
// __cdecl with seven stack arguments ([ebp+8] the frame's gains, [ebp+0xc] the gain count, [ebp+0x10] the
// current index, [ebp+0x14] the mode, [ebp+0x18] and [ebp+0x1c] two index outputs, [ebp+0x20] the codec
// context; `ret` at 0x1003f71b pops nothing).
// Four context slots are bound on entry (0x1003f32b-0x1003f353): the running cost array at +0x2268 (0x3c
// floats), the two-row back-pointer table at +0x2358 (row stride 0x3c dwords), the row number at +0x2538 and
// the smoothed level at +0x253c. The gains are decremented by one float when they are not null
// (`je 0x1003f365` at 0x1003f35a).
// Mode 1 pushes the level towards the current gain, `level * 0.75f + gains[index] / 2.0f` with the floats at
// 0x1005ec04 and 0x1005ec2c (0x1003f36d-0x1003f38e); any other mode decays it by the float 0.984375f at
// 0x1005ec4c (0x1003f395). The step penalty is `level / 16.0f` with the float 16.0f at 0x1005ec48 (0x1003f3a7),
// except that in mode 0 a level strictly below the float 128.0f at 0x1005ec44 makes the penalty the dword
// 0x41000000 (`test ah, 1` / `je 0x1003f3d1` at 0x1003f3c5, store at 0x1003f3ca).
// The current row is `*rowSlot + 1` (0x1003f3d6) and its first back-pointer is set to 1 (0x1003f3e5). The
// forward pass walks k = 1..*count (`jg 0x1003f49c` at 0x1003f426) carrying a running cost that starts as
// cost[0] (0x1003f401): each step adds the penalty (0x1003f42b) and, when the result is strictly below cost[k],
// overwrites cost[k] and points back at the best index so far (`test ah, 1` / `je 0x1003f46c` at 0x1003f440,
// stores at 0x1003f44e and 0x1003f463); otherwise the running cost is reset to cost[k], the back-pointer is k
// and k becomes the best index (0x1003f476-0x1003f494).
// The backward pass starts at `best - 1` with the running cost cost[best-1] (0x1003f49f-0x1003f4ae) and repeats
// the same test downwards while the index is at least 1 (`jl 0x1003f531` at 0x1003f4b5), either overwriting
// cost[k] or jumping to the back-pointer it finds (0x1003f4fb-0x1003f523).
// cost[0] then takes `gains[1] / 2.0f + cost[0]` (0x1003f534-0x1003f545) and k = 2..*count take
// `gains[k] / 2.0f + cost[k-1]` (0x1003f586-0x1003f59f), while the largest cost is tracked in a local
// (`test ah, 0x41` / `jne 0x1003f5c4` at 0x1003f5b2) and the smallest both in a local and in the second index
// output (`test ah, 1` / `je 0x1003f5ed` at 0x1003f5d3, store at 0x1003f5de). Every cost is then shifted down by
// the smallest (0x1003f617-0x1003f624) and so is the largest (0x1003f62a).
// A backoff then runs over m = 20, 30, 40 (0x1003f63a, `add eax, 0xa` at 0x1003f646, `jg 0x1003f683` at
// 0x1003f650): when the chosen index is strictly above m (`jle 0x1003f681` at 0x1003f65a) and a quarter of the
// largest cost, with the float 4.0f at 0x1005ec20, is strictly above cost[index - m - 1]
// (`test ah, 0x41` / `jne 0x1003f681` at 0x1003f676), m is remembered (0x1003f67e); the chosen index is reduced
// by it (0x1003f688).
// The first index output takes the chosen index (0x1003f6a0) and is then followed back twice through the
// back-pointer table, the row alternating as `(row % 2) + 1` (0x1003f6ba-0x1003f6cd, store at 0x1003f6ea).
// Finally the row slot becomes `(row + 1) % 2` (0x1003f6fd-0x1003f711) and the function returns 0 (0x1003f713).
typedef int(__cdecl* CodecGainSmoother_t)(const float*, const int*, const int*, const int*, int*, int*, void*);
CodecGainSmoother_t CodecGainSmoother_orig;
int __cdecl CodecGainSmoother_re(const float* gains, const int* count, const int* index, const int* mode,
                                 int* chosenOut, int* indexOut, void* context) {
    char* ctx = reinterpret_cast<char*>(context);
    float* cost = reinterpret_cast<float*>(ctx + 0x2268);        // 0x1003f32b
    int* back = reinterpret_cast<int*>(ctx + 0x2358);            // 0x1003f336
    int* rowSlot = reinterpret_cast<int*>(ctx + 0x2538);         // 0x1003f342
    float* level = reinterpret_cast<float*>(ctx + 0x253C);       // 0x1003f34e

    if (gains != 0) gains -= 1;                                  // je at 0x1003f35a

    const float k075 = c3bi_f(0x0005EC04);
    const float k2 = c3bi_f(0x0005EC2C);
    const float kDecay = c3bi_f(0x0005EC4C);
    const float k16 = c3bi_f(0x0005EC48);
    const float k128 = c3bi_f(0x0005EC44);
    const float k4 = c3bi_f(0x0005EC20);

    if (*mode == 1) {                                            // jne at 0x1003f36b
        *level = static_cast<float>(
            static_cast<double>(*level) * static_cast<double>(k075)
            + static_cast<double>(gains[*index]) / static_cast<double>(k2));   // 0x1003f370-0x1003f38e
    } else {
        *level = static_cast<float>(
            static_cast<double>(*level) * static_cast<double>(kDecay));        // 0x1003f395-0x1003f3a0
    }
    float penalty = static_cast<float>(
        static_cast<double>(*level) / static_cast<double>(k16));               // 0x1003f3a5-0x1003f3ad
    if (*mode == 0 && static_cast<double>(*level) < static_cast<double>(k128))  // jne at 0x1003f3b6, je at 0x1003f3c8
        *reinterpret_cast<unsigned*>(&penalty) = 0x41000000u;                  // 0x1003f3ca

    const int row = *rowSlot + 1;                                              // 0x1003f3d6
    int* bpRow = back + (row - 1) * 0x3C;                                      // 0x1003f3df
    bpRow[0] = 1;                                                              // 0x1003f3e5

    int best = 1;                                                              // 0x1003f3f7
    float running = cost[0];                                                   // 0x1003f401
    int n = *count;                                                            // 0x1003f409
    for (int k = 1; k <= n; ++k) {                                             // jg at 0x1003f426
        running = static_cast<float>(
            static_cast<double>(running) + static_cast<double>(penalty));      // 0x1003f42b
        if (static_cast<double>(running) < static_cast<double>(cost[k - 1])) { // je at 0x1003f443
            cost[k - 1] = running;                                             // 0x1003f44e
            bpRow[k - 1] = best;                                               // 0x1003f463
        } else {
            running = cost[k - 1];                                             // 0x1003f476
            bpRow[k - 1] = k;                                                  // 0x1003f48a
            best = k;                                                          // 0x1003f494
        }
    }

    int k = best - 1;                                                          // 0x1003f49f
    running = cost[k];                                                         // 0x1003f4ab
    while (k >= 1) {                                                           // jl at 0x1003f4b5
        running = static_cast<float>(
            static_cast<double>(running) + static_cast<double>(penalty));      // 0x1003f4ba
        if (static_cast<double>(running) < static_cast<double>(cost[k - 1])) { // je at 0x1003f4d2
            cost[k - 1] = running;                                             // 0x1003f4dd
            bpRow[k - 1] = best;                                               // 0x1003f4f2
        } else {
            best = bpRow[k - 1];                                               // 0x1003f509
            k = best;                                                          // 0x1003f516
            running = cost[k - 1];                                             // 0x1003f51f
        }
        k -= 1;                                                                // 0x1003f529
    }

    cost[0] = static_cast<float>(
        static_cast<double>(gains[1]) / static_cast<double>(k2) + static_cast<double>(cost[0]));  // 0x1003f534-0x1003f545
    float lowest = cost[0];                                                    // 0x1003f54a
    float highest = lowest;                                                    // 0x1003f552
    *indexOut = 1;                                                             // 0x1003f558
    n = *count;                                                                // 0x1003f561
    for (int j = 2; j <= n; ++j) {                                             // jg at 0x1003f57e
        cost[j - 1] = static_cast<float>(
            static_cast<double>(gains[j]) / static_cast<double>(k2)
            + static_cast<double>(cost[j - 1]));                               // 0x1003f586-0x1003f59f
        if (static_cast<double>(cost[j - 1]) > static_cast<double>(highest))   // jne at 0x1003f5b5
            highest = cost[j - 1];                                             // 0x1003f5c1
        if (static_cast<double>(cost[j - 1]) < static_cast<double>(lowest)) {  // je at 0x1003f5d6
            *indexOut = j;                                                     // 0x1003f5de
            lowest = cost[j - 1];                                              // 0x1003f5ea
        }
    }
    n = *count;                                                                // 0x1003f5f2
    for (int j = 1; j <= n; ++j)                                               // jg at 0x1003f60f
        cost[j - 1] = static_cast<float>(
            static_cast<double>(cost[j - 1]) - static_cast<double>(lowest));   // 0x1003f617-0x1003f624
    highest = static_cast<float>(
        static_cast<double>(highest) - static_cast<double>(lowest));           // 0x1003f62a-0x1003f630

    int backoff = 0;                                                           // 0x1003f633
    for (int m = 0x14; m <= 0x28; m += 0xA) {                                  // jg at 0x1003f650
        if (*indexOut > m) {                                                   // jle at 0x1003f65a
            const int at = *indexOut - m;                                      // 0x1003f661
            if ((static_cast<double>(highest) / static_cast<double>(k4))
                > static_cast<double>(cost[at - 1]))                           // jne at 0x1003f679
                backoff = m;                                                   // 0x1003f67e
        }
    }
    *indexOut -= backoff;                                                      // 0x1003f688-0x1003f68e

    int walkRow = *rowSlot;                                                    // 0x1003f693
    *chosenOut = *indexOut;                                                    // 0x1003f6a0
    for (int step = 1; step <= 2; ++step) {                                    // jg at 0x1003f6b8
        walkRow = c3bi_srem(walkRow, 0x80000001u, 0xFFFFFFFEu) + 1;            // 0x1003f6ba-0x1003f6cd
        *chosenOut = back[(walkRow - 1) * 0x3C + *chosenOut - 1];              // 0x1003f6d0-0x1003f6ea
    }
    *rowSlot = c3bi_srem(*rowSlot + 1, 0x80000001u, 0xFFFFFFFEu);              // 0x1003f6fd-0x1003f711
    return 0;                                                                  // 0x1003f713
}

// ---------------------------------------------------------------------------------------------------------
// sequencer and MIDI device
// ---------------------------------------------------------------------------------------------------------

// Seq_listGetAt 0x1001d780 walks the list embedded at sequencer+0x60 and returns 0 when it reached a payload,
// 0x24 when it did not (`neg` / `sbb` / `and al, 0xdc` / `add eax, 0x24` at 0x1001d7d5-0x1001d7dc); the payload,
// or 0 on failure, is written through its out pointer. The three bodies below call that original at its own
// address.
typedef int(__fastcall* SeqListGetAt_t)(void*, void*, void**, unsigned);

// 0x00023a60  Seq_cmdHandler10 (VA 0x10023a60)
// __thiscall with two stack arguments (`ret 8` at 0x10023a73, 0x10023ac0, 0x10023b2b and 0x10023b36): the tag
// to look up and the value to store.
// A zero dword at this+0x224 returns 0x24 (`test eax, eax` / `jne 0x10023a76` at 0x10023a69). The node chain
// starts at this+0x218 and is mirrored into the cursor at this+0x220 (0x10023a7c, 0x10023a7e); a node carries
// its successor at +4 and its payload at +8, and the walk returns 0x24 as soon as either is null (0x10023a84,
// 0x10023a8b, 0x10023a9e, 0x10023aab, 0x10023ab2). It stops at the first payload whose dword at +4 equals the
// tag (`cmp [eax+4], ecx` / `je 0x10023ab6` at 0x10023a91).
// That payload's range is its dwords at +8 and +0xc (0x10023ac4, 0x10023ac8). A high end below the low end
// returns 0xa (`jb 0x10023b2e` at 0x10023acd, UNSIGNED), and so does a high end that is not below the track
// count the sequencer reports through its own vtable slot +0xb4 (`call dword [eax+0xb4]` at 0x10023ad3 with the
// object still in ecx and nothing pushed, `jae 0x10023b2e` at 0x10023adb).
// Seq_listGetAt is then called for step 0 on the list at this+0x60 (0x10023aea) with the stack slot that held
// the first argument as the out pointer; `or byte [eax+0x38], 8` at 0x10023af7 runs only when that call FAILED
// (`je 0x10023afb` at 0x10023af1), in which case the out pointer holds 0 and the store faults, so that side is
// never taken by a lookup that can return.
// The loop from the low end to the high end (0x10023b01-0x10023b23, UNSIGNED, entered unconditionally because
// `ja 0x10023b25` at 0x10023aff repeats the comparison already proved at 0x10023acd) looks up each step and,
// when the lookup SUCCEEDED (`jne 0x10023b20` at 0x10023b14, the opposite sense of the first one), stores the
// second argument in the record's dword at +0x45c (0x10023b1a). Returns 0 (0x10023b28).
typedef int(__fastcall* SeqCmdHandler10_t)(void*, void*, unsigned, unsigned);
SeqCmdHandler10_t SeqCmdHandler10_orig;
int __fastcall SeqCmdHandler10_re(void* self, void*, unsigned tag, unsigned value) {
    if (fldc<int>(self, 0x224) == 0) return 0x24;            // jne at 0x10023a6b
    void* node = fldc<void*>(self, 0x218);                   // 0x10023a63
    fld<void*>(self, 0x220) = node;                          // 0x10023a7e
    if (node == 0) return 0x24;                              // je at 0x10023a84
    void* payload = fldc<void*>(node, 8);                    // 0x10023a86
    if (payload == 0) return 0x24;                           // je at 0x10023a8b
    while (fldc<unsigned>(payload, 4) != tag) {              // je at 0x10023a94
        node = fldc<void*>(self, 0x220);                     // 0x10023a96
        if (node == 0) return 0x24;                          // je at 0x10023a9e
        node = fldc<void*>(node, 4);                         // 0x10023aa0
        fld<void*>(self, 0x220) = node;                      // 0x10023aa5
        if (node == 0) return 0x24;                          // je at 0x10023aab
        payload = fldc<void*>(node, 8);                      // 0x10023aad
        if (payload == 0) return 0x24;                       // je at 0x10023ab2
    }

    const unsigned low = fldc<unsigned>(payload, 8);         // 0x10023ac4
    const unsigned high = fldc<unsigned>(payload, 0xC);      // 0x10023ac8
    if (high < low) return 0xA;                              // jb at 0x10023acd
    void** vtbl = *reinterpret_cast<void***>(self);
    typedef unsigned(__fastcall* TrackCount_t)(void*);
    const unsigned count = reinterpret_cast<TrackCount_t>(vtbl[0xB4 / 4])(self);   // 0x10023ad3
    if (high >= count) return 0xA;                           // jae at 0x10023adb

    SeqListGetAt_t getAt = reinterpret_cast<SeqListGetAt_t>(c3bi_sound(0x0001D780));
    void* record = 0;
    if (getAt(reinterpret_cast<char*>(self) + 0x60, 0, &record, 0) != 0)   // je at 0x10023af1
        fld<unsigned char>(record, 0x38) |= 8;                             // 0x10023af7

    unsigned i = low;                                        // 0x10023afd
    do {
        if (getAt(reinterpret_cast<char*>(self) + 0x60, 0, &record, i) == 0)   // jne at 0x10023b14
            fld<unsigned>(record, 0x45C) = value;                              // 0x10023b1a
        ++i;                                                                   // 0x10023b20
    } while (i <= high);                                                       // jbe at 0x10023b23
    return 0;                                                                  // 0x10023b28
}

// 0x00024b80  Seq_scheduleEvent (VA 0x10024b80)
// __thiscall with four stack arguments (`ret 0x10` at 0x10024c12): the track record, the tick, the flag byte
// and a track index. The function leaves no defined value in eax on any path (the epilogue at 0x10024c0f pops
// and returns without writing it), so it is declared void.
// The period starts as the track record's dword at +0x54 (0x10024b8f). A track index above zero
// (`test eax, eax` / `jle 0x10024bad` at 0x10024b8d, SIGNED) looks that track up with Seq_listGetAt on the list
// at this+0x60 (0x10024b9d) and, when the lookup SUCCEEDED (`jne 0x10024bad` at 0x10024ba4), re-reads the
// period from the record it found (0x10024baa). A zero period does nothing at all (`je 0x10024c0f` at
// 0x10024baf).
// The flag byte then picks one of four ways to write the record's dword at +0x50:
//   bit 0 (`je 0x10024bce` at 0x10024bb9)  `tick + (period - tick % period)`, the next multiple of the period
//                                          at or after the tick (`div esi` at 0x10024bc3, UNSIGNED, stored at
//                                          0x10024bc9);
//   bit 2 (`je 0x10024bea` at 0x10024bd1)  `(tick / step + 1) * step` with the step the sequencer's dword at
//                                          +0x1bc (0x10024bd3-0x10024be5, UNSIGNED);
//   bit 4 (`je 0x10024bf8` at 0x10024bed)  the tick itself (0x10024bf3);
//   none of them                           zero (0x10024bf8).
// Bit 5 finally hands the record to Midi_seekTrackToTick 0x10024920 with a tick limit of 0 (`test bl, 0x20` /
// `je 0x10024c0f` at 0x10024bff, call at 0x10024c0a).
typedef void(__fastcall* SeqScheduleEvent_t)(void*, void*, void*, unsigned, unsigned, int);
typedef int(__fastcall* MidiSeekTrackToTick_t)(void*, void*, void*, unsigned);
SeqScheduleEvent_t SeqScheduleEvent_orig;
void __fastcall SeqScheduleEvent_re(void* self, void*, void* track, unsigned tick,
                                    unsigned flags, int trackIndex) {
    unsigned period = fldc<unsigned>(track, 0x54);           // 0x10024b8f
    if (trackIndex > 0) {                                    // jle at 0x10024b92
        SeqListGetAt_t getAt = reinterpret_cast<SeqListGetAt_t>(c3bi_sound(0x0001D780));
        void* found = 0;
        if (getAt(reinterpret_cast<char*>(self) + 0x60, 0, &found,
                  static_cast<unsigned>(trackIndex)) == 0)   // jne at 0x10024ba4
            period = fldc<unsigned>(found, 0x54);            // 0x10024baa
    }
    if (period == 0) return;                                 // je at 0x10024baf

    if ((flags & 1) != 0) {                                  // je at 0x10024bb9
        fld<unsigned>(track, 0x50) = (period - tick % period) + tick;   // 0x10024bc1-0x10024bc9
    } else if ((flags & 4) != 0) {                           // je at 0x10024bd1
        const unsigned step = fldc<unsigned>(self, 0x1BC);   // 0x10024bd3
        fld<unsigned>(track, 0x50) = (tick / step + 1) * step;          // 0x10024bdf-0x10024be5
    } else if ((flags & 0x10) != 0) {                        // je at 0x10024bed
        fld<unsigned>(track, 0x50) = tick;                   // 0x10024bf3
    } else {
        fld<unsigned>(track, 0x50) = 0;                      // 0x10024bf8
    }

    if ((flags & 0x20) != 0) {                               // je at 0x10024c03
        MidiSeekTrackToTick_t seek =
            reinterpret_cast<MidiSeekTrackToTick_t>(c3bi_sound(0x00024920));
        seek(self, 0, track, 0);                             // 0x10024c0a
    }
}

// 0x000213d0  Midi_Device::setChannelVolume (VA 0x100213d0)
// __thiscall with three stack arguments (`ret 0xc` at 0x10021409 and 0x10021508): the two-byte controller event
// ([event] the controller number, [event+1] its value), the channel and a channel-state object. It always
// returns 2 (0x10021400, 0x100214ff).
// Controller 0xa stores the value in the device's dword at `this + 0x4c * channel + 0x260`
// (`lea edx, [eax+eax*8+0x48]` / `lea eax, [eax+edx*2+8]` at 0x100213f3-0x100213fb, i.e. 19 * channel + 0x98
// dwords) and does nothing else; a controller that is neither 0xa nor 7 does nothing at all
// (`cmp al, 0xa` / `jne 0x100213fe` at 0x100213e6).
// Controller 7 (`cmp al, 7` / `je 0x1002140c` at 0x100213de) stores the value in the dword at
// `this + 0x4c * channel + 0x254` (0x10021415-0x1002141b) and then, unless bit 0 of the state object's byte at
// +0x38 is set (`jne 0x100213fe` at 0x1002142a) or its dword at +0x48 names another channel
// (`jne 0x100213fe` at 0x1002142f), walks the 0x80 pointers from the state object's +0x254 DOWNWARDS, four
// bytes at a time (0x10021431, 0x100214ea-0x100214f6).
// A null pointer is skipped (`je 0x100214e2` at 0x1002144c). With bit 0 of the group's dword at +0xc8 clear
// (`je 0x100214d3` at 0x10021459) the value is handed to the group's own vtable slot +0x40
// (`call dword [eax+0x40]` at 0x100214db, one argument, the object in ecx). With that bit set, the sixteen
// voice slots at group+0x2a8, stride 0x110, are walked (0x10021464, `add ebx, 0x110` at 0x100214c2,
// `jl 0x1002146a` at 0x100214cb): bits 12..27 of the group's +0xc8 select which slots become the current one
// (`shr eax, 0xc` / `and eax, 0xffff` / `test ebp, eax` at 0x1004146a-0x10021478, the selection STICKY across
// the following rounds), and `cmp edi, 0x10` / `jge 0x100214cf` at 0x1002147f can never be taken because the
// loop itself bounds that counter below 0x10. Whenever a current slot exists and bit 3 of its byte at +0x9c is
// clear (0x10021485, 0x1002148e), Voice_setLevel 0x100383b0 is called on it with
// `((slot[+0xe8] * value) >> 7) * this[+0x730] >> 7`, both shifts UNSIGNED (0x10021499-0x100214b2).
typedef int(__fastcall* MidiSetChannelVolume_t)(void*, void*, const unsigned char*, unsigned, void*);
typedef void(__fastcall* VoiceSetLevel_t)(void*, void*, unsigned);
typedef void(__fastcall* GroupSlot40_t)(void*, void*, unsigned);
MidiSetChannelVolume_t MidiSetChannelVolume_orig;
int __fastcall MidiSetChannelVolume_re(void* self, void*, const unsigned char* event,
                                       unsigned channel, void* state) {
    const unsigned char controller = event[0];               // 0x100213db
    if (controller != 7) {                                   // je at 0x100213e4
        if (controller == 0xA)                               // jne at 0x100213e8
            fld<unsigned>(self, 0x4C * channel + 0x260) = event[1];   // 0x100213f3-0x100213fb
        return 2;                                            // 0x10021400
    }

    fld<unsigned>(self, 0x4C * channel + 0x254) = event[1];  // 0x10021415-0x1002141b
    if ((fldc<unsigned char>(state, 0x38) & 1) != 0) return 2;        // jne at 0x1002142a
    if (fldc<unsigned>(state, 0x48) != channel) return 2;            // jne at 0x1002142f

    VoiceSetLevel_t setLevel = reinterpret_cast<VoiceSetLevel_t>(c3bi_sound(0x000383B0));
    char* cursor = reinterpret_cast<char*>(state) + 0x254;   // 0x10021431
    for (int round = 0x80; round != 0; --round) {            // 0x1002143c, jne at 0x100214f6
        void* group = *reinterpret_cast<void**>(cursor);     // 0x10021444
        if (group != 0) {                                    // je at 0x1002144c
            if ((fldc<unsigned>(group, 0xC8) & 1) == 0) {    // je at 0x10021459
                void** vtbl = *reinterpret_cast<void***>(group);
                reinterpret_cast<GroupSlot40_t>(vtbl[0x40 / 4])(group, 0, event[1]);   // 0x100214db
            } else {
                char* current = 0;                                   // 0x1002145b
                unsigned bit = 1;                                    // 0x1002145d
                char* slot = reinterpret_cast<char*>(group) + 0x2A8; // 0x10021464
                for (int k = 0; k < 0x10; ++k) {                     // jl at 0x100214cb
                    const unsigned mask = (fldc<unsigned>(group, 0xC8) >> 0xC) & 0xFFFF;  // 0x1002146a-0x10021473
                    if ((bit & mask) != 0)                           // je at 0x1002147a
                        current = slot;                              // 0x10021481 (`jge` at 0x1002147f is unreachable)
                    if (current != 0 && (fldc<unsigned char>(current, 0x9C) & 8) == 0) {  // 0x10021485, 0x1002148e
                        unsigned amount = fldc<unsigned>(current, 0xE8) * event[1];       // 0x10021499-0x1004149f
                        amount >>= 7;                                                     // 0x100214a2
                        amount *= fldc<unsigned>(self, 0x730);                            // 0x100214a5
                        amount >>= 7;                                                     // 0x100214ac
                        setLevel(current, 0, amount);                                     // 0x100214b2
                    }
                    bit <<= 1;                                       // 0x100214bf
                    slot += 0x110;                                   // 0x100214c2
                }
            }
        }
        cursor -= 4;                                                 // 0x100214ea
    }
    return 2;                                                        // 0x100214ff
}

SG_HOOK("sound.dll", 0x0003bf30, sound_bitPermute, BitPermute_re, BitPermute_orig);
SG_HOOK("sound.dll", 0x0003e6d0, sound_gaussSolveClamped, GaussSolveClamped_re, GaussSolveClamped_orig);
SG_HOOK("sound.dll", 0x000414f0, sound_codecAmdfPitch, CodecAmdfPitch_re, CodecAmdfPitch_orig);
SG_HOOK("sound.dll", 0x00040ab0, sound_codecMarkCorrelationJumps, CodecMarkCorrelationJumps_re,
        CodecMarkCorrelationJumps_orig);
SG_HOOK("sound.dll", 0x00041020, sound_codecWindowFeatures, CodecWindowFeatures_re, CodecWindowFeatures_orig);
SG_HOOK("sound.dll", 0x0003f310, sound_codecGainSmoother, CodecGainSmoother_re, CodecGainSmoother_orig);
SG_HOOK("sound.dll", 0x00023a60, sound_Seq_cmdHandler10, SeqCmdHandler10_re, SeqCmdHandler10_orig);
SG_HOOK("sound.dll", 0x00024b80, sound_Seq_scheduleEvent, SeqScheduleEvent_re, SeqScheduleEvent_orig);
SG_HOOK("sound.dll", 0x000213d0, sound_Midi_setChannelVolume, MidiSetChannelVolume_re,
        MidiSetChannelVolume_orig);

}  // namespace
