// C3 batch c3bn (2026-10-08) of sound.dll: three speech-codec functions (the two leaves of codecSynthStage
// 0x1003b230 and the pitch stage of codecStageDispatch 0x1003c640) and one sequencer function.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside
// each body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32 or COM call is made, nothing is allocated or freed, and no live sound object,
// device, list or buffer is touched: every buffer, record, context and object the A/B passes in is allocated by
// the fixture. One module global is read by lpcSynthesize, the codec order g_codecOrder 0x100b58a0, whose only
// writer in the whole DLL is codecSetParams 0x1003aa50 (it stores 10); a detour counter at the menu entered
// neither that writer nor any function of this batch in 15 s (log/c3/alt/c3bn_probe.py), and the fixture sets
// the global to the same 10 before the vectors run. It is read, never written, by both arms.
// Three bodies forward to an original callee at its own address (the CRT's sqrt 0x10043204, codecCombFilter
// 0x10040f60, codecAmdfPitch 0x100414f0, Seq_listGetAt 0x1001d780 and Seq_scheduleEvent 0x10024b80);
// sound.dll is relocatable, so those addresses and every float constant are resolved against the module base at
// the first call.
// Floating point: the three codec bodies are x87 (/Od /GZ debug bodies, every intermediate spilled), the shim
// is SSE2. Each `fstp dword` spill is written as an assignment to a float and every operand inside one
// expression is widened with static_cast<double>, which is the c3ap method: the A/B thread runs with control
// word 0x027f (53-bit, round to nearest), so an x87 step that ends in a register is exactly a double operation
// and a step that ends in a dword slot is exactly a double operation rounded once to float. A `fst dword` that
// is not followed by a pop keeps the unrounded value in st(0) for the next use, so the comparison that follows
// it is made on the double expression and only the stored slot is a float.
// VC6 converts a double to an int through __ftol 0x10042550, whose body sets rounding control to 11 (truncate)
// at 0x1004255f and keeps the low dword of a 64-bit `fistp` (0x10042569, 0x1004256f), so it is written
// (int)(long long)v.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fld(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T fldc(const void* base, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + off);
}
char* fldAddr(void* base, unsigned off) { return reinterpret_cast<char*>(base) + off; }

// sound.dll has base relocations, so an original callee, table or constant is addressed through the loaded base.
char* c3bn_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

float c3bn_f(unsigned rva) { return *reinterpret_cast<const float*>(c3bn_sound(rva)); }
double c3bn_d(unsigned rva) { return *reinterpret_cast<const double*>(c3bn_sound(rva)); }

// The CRT sqrt the originals call (`call 0x10043204` at 0x1003d95f, 0x1003d437, 0x1003d862): a two-instruction
// wrapper (`lea edx, [esp+4]` / `call 0x10048225`) over the double at [esp+4], so a plain __cdecl double(double).
typedef double(__cdecl* Sqrt_t)(double);
double c3bn_sqrt(double v) {
    static Sqrt_t fn = 0;
    if (!fn) fn = reinterpret_cast<Sqrt_t>(c3bn_sound(0x00043204));
    return fn(v);
}

// __ftol 0x10042550: truncating 64-bit fistp, low dword kept.
int c3bn_ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }

// A raw dword copy: the originals move float slots with `mov`, which carries a NaN or a 0xcccccccc fill
// through unchanged.
void c3bn_copy32(void* dst, const void* src) {
    *reinterpret_cast<unsigned*>(dst) = *reinterpret_cast<const unsigned*>(src);
}

// ---------------------------------------------------------------------------------------------------------
// speech codec
// ---------------------------------------------------------------------------------------------------------

// 0x0003d8d0  reflectionToLpc (VA 0x1003d8d0)
// __cdecl with five stack arguments ([ebp+8] the reflection coefficients, [ebp+0xc] the LPC output, [ebp+0x10]
// the order, [ebp+0x14] an amplitude, [ebp+0x18] the gain cell; `ret` at 0x1003da4c pops nothing), a /Od /GZ
// debug body whose frame [ebp-0x80..ebp-1] is filled with 0xcccccccc (`rep stosd` at 0x1003d8e9).
// Both arrays are decremented by one float on entry (0x1003d8ee, 0x1003d8f7), so every index is one-based.
// THE WORK ARRAY ALIASES TWO LOOP SLOTS. It is `[ebp + j*4 - 0x34]` (0x1003d9e5), while the outer counter is
// [ebp-0x34] (= slot 0), the inner bound is [ebp-8] (= slot 11) and the cached order is [ebp-4] (= slot 12);
// both of those are re-read on every round of their loop (0x1003d9bf, 0x1003d998), so an order of 12 or more
// lets the work array overwrite them. The model below keeps all three in one array so that this is reproduced.
// The gain cell is first set to the immediate 1.0f (0x3f800000 at 0x1003d900) and then multiplied by
// `1.0f - k[i]*k[i]` for i = 1..*order (`fsubr` against the float 1.0f at 0x1005b8a8, `jg 0x1003d94c` at
// 0x1003d926); it becomes `sqrt(gain) * (double)*[ebp+0x14]` (the amplitude is widened to a qword slot at
// 0x1003d951, the call at 0x1003d95f, `fmul qword` at 0x1003d967) and is stored back as a float (0x1003d96d).
// a[1] takes k[1] as a raw dword (0x1003d975-0x1003d978). For i = 2..*order (`jg 0x1003da37` at 0x1003d99b) the
// step is the standard Levinson recursion: `tmp[j] = a[j] - k[i]*a[i-j]` for j = 1..i-1 (0x1003d9d3-0x1003d9e5),
// tmp is copied back over a[1..i-1] as raw dwords (0x1003da1b) and a[i] takes k[i], again as a raw dword
// (0x1003da2f). Returns 0 (0x1003da37).
typedef int(__cdecl* ReflectionToLpc_t)(const float*, float*, const int*, const float*, float*);
ReflectionToLpc_t ReflectionToLpc_orig;
int __cdecl ReflectionToLpc_re(const float* k, float* a, const int* order, const float* amplitude,
                               float* gain) {
    union { unsigned u[13]; float f[13]; } frame;   // frame[j] is [ebp - 0x34 + j*4]
    for (int t = 0; t < 13; ++t) frame.u[t] = 0xCCCCCCCCu;   // rep stosd at 0x1003d8e9

    a -= 1;                                                  // 0x1003d8ee
    k -= 1;                                                  // 0x1003d8f7
    *reinterpret_cast<unsigned*>(gain) = 0x3F800000u;        // 0x1003d900
    const float one = c3bn_f(0x0005B8A8);

    frame.u[12] = static_cast<unsigned>(*order);             // [ebp-4] at 0x1003d90b
    for (frame.u[0] = 1; static_cast<int>(frame.u[0]) <= static_cast<int>(frame.u[12]);
         frame.u[0] = static_cast<unsigned>(static_cast<int>(frame.u[0]) + 1)) {      // jg at 0x1003d926
        const int i = static_cast<int>(frame.u[0]);
        *gain = static_cast<float>(
            (static_cast<double>(one) - static_cast<double>(k[i]) * static_cast<double>(k[i]))
            * static_cast<double>(*gain));                   // 0x1003d934-0x1003d948
    }

    const double amp = static_cast<double>(*amplitude);      // fstp qword at 0x1003d951
    *gain = static_cast<float>(c3bn_sqrt(static_cast<double>(*gain)) * amp);   // 0x1003d95f-0x1003d96d
    c3bn_copy32(&a[1], &k[1]);                               // 0x1003d975-0x1003d978

    frame.u[12] = static_cast<unsigned>(*order);             // 0x1003d980
    for (frame.u[0] = 2; static_cast<int>(frame.u[0]) <= static_cast<int>(frame.u[12]);
         frame.u[0] = static_cast<unsigned>(static_cast<int>(frame.u[0]) + 1)) {      // jg at 0x1003d99b
        const int i = static_cast<int>(frame.u[0]);
        frame.u[11] = static_cast<unsigned>(i - 1);          // [ebp-8] at 0x1003d9a7
        for (int j = 1; j <= static_cast<int>(frame.u[11]); ++j)                      // jg at 0x1003d9c2
            frame.f[j] = static_cast<float>(static_cast<double>(a[j])
                                            - static_cast<double>(k[i]) * static_cast<double>(a[i - j]));
        frame.u[11] = static_cast<unsigned>(i - 1);          // 0x1003d9f1
        for (int j = 1; j <= static_cast<int>(frame.u[11]); ++j)                      // jg at 0x1003da0c
            *reinterpret_cast<unsigned*>(&a[j]) = frame.u[j];                         // 0x1003da1b
        c3bn_copy32(&a[i], &k[i]);                           // 0x1003da2f
    }
    return 0;                                                // 0x1003da37
}

// 0x0003d1d0  lpcSynthesize (VA 0x1003d1d0)
// __cdecl with eight stack arguments ([ebp+8] the LPC coefficients, [ebp+0xc] the sub-frame length, [ebp+0x10]
// the excitation mode, [ebp+0x14] the sample output, [ebp+0x18] a level, [ebp+0x1c] a second level, [ebp+0x20]
// a feedback gain, [ebp+0x24] the codec context; `ret` at 0x1003d8c2 pops nothing), a /Od /GZ debug body.
// The coefficients and the output are decremented by one float only when they are not null (`je 0x1003d1fd` at
// 0x1003d1f2, `je 0x1003d20c` at 0x1003d201), so both are one-based; every other array index is one-based too.
// Ten context addresses are bound on entry (0x1003d20c-0x1003d27e): the previous sub-frame length at +0x688,
// the excitation array at +0x68c, the synthesis history at +0x924, three post-filter delays at +0xbbc, +0xbc0
// and +0xbc4, three more at +0xbc8, +0xbcc and +0xbd0, and the previous level at +0xbd4.
// THE ORDER COMES FROM THE MODULE GLOBAL g_codecOrder 0x100b58a0 (0x1003d2cc, 0x1003d369, 0x1003d471,
// 0x1003d6b6, 0x1003d767, 0x1003d7e7), not from an argument; codecSetParams 0x1003aa50 is the only writer in
// the DLL and stores 10.
// Entry: `*[+0xbd4] / (*[ebp+0x18] + 1e-06f)` (the float at 0x1005ec28) is kept unrounded in st(0) by
// `fst dword [ebp-0x34]` (0x1003d291) and compared with the float 8.0f at 0x1005ec24; above it the factor
// becomes the immediate 8.0f (0x41000000 at 0x1003d2ac), at or below it the stored float (`je 0x1003d2ac` at
// 0x1003d29f). The previous level then takes the current one as a raw dword (0x1003d2c2-0x1003d2ca), and the
// synthesis history is scaled, `hist[i] = factor * hist[prevLen + i]` for i = 1..g_codecOrder
// (0x1003d2fa-0x1003d30a), after which the previous length takes *[ebp+0xc] (0x1003d318).
// A non-zero excitation mode (`jne 0x1003d42c` at 0x1003d320) builds a pulse train: with
// `scale = sqrt((double)*[ebp+0xc]) / 6.928` (the qword at 0x1005ec10, call at 0x1003d437, `fdiv qword` at
// 0x1003d43f) every excitation slot is zeroed (0x1003d47d) and the first 0x19 of them take
// `table[i] * scale` from the 26-entry int table at 0x10064a18 (`jg 0x1003d4ab` at 0x1003d489, `fild` at
// 0x1003d48e); each slot is then run through a three-tap filter with the floats 0.125f (0x1005ec08), 0.75f
// (0x1005ec04) and 0.0f (0x1005ec00) over the delay line +0xbbc / +0xbc0 / +0xbc4, which is shifted afterwards
// (0x1003d515-0x1003d532). A second pass fills a frame-local array with `codecCombFilter(ctx) * 1.0f / 64.0f`
// (the floats at 0x1005b8a8 and 0x1005ebfc, call at 0x1003d563) and filters it the same way with -0.125f
// (0x1005ebf8), 0.25f (0x1005ebf4) and 0.0f over the delays +0xbc8 / +0xbcc / +0xbd0; the two passes are added
// together into the excitation array (0x1003d656-0x1003d66d).
// A zero excitation mode (0x1003d326) instead fills the excitation with `codecCombFilter(ctx) / 64` as an
// INTEGER division (`sar eax, 6` at 0x1003d35a) and then places a single two-sided pulse: one more comb-filter
// value gives the position `((v + 0x8000) * (*[ebp+0xc] - 1)) >> 16` (an integer division by 0x10000 at
// 0x1003d39f) offset by g_codecOrder + 1 (0x1003d3a8), and the amplitude `*[ebp+0x1c] / 4.0f * 1.0f * 342.0f`
// (the floats at 0x1005ec20, 0x1005b8a8 and 0x1005ec1c) is clamped to the immediate 2000.0f (0x44fa0000 at
// 0x1003d3df) when it is strictly above the float 2000.0f at 0x1005ec18 (`jne 0x1003d3e9` at 0x1003d3dd). The
// amplitude is added at that position and subtracted at the next one (0x1003d3f8-0x1003d424).
// The tail (0x1003d673) is the synthesis filter run twice. First `hist[g + i] = *[ebp+0x20] * sum(a[j] *
// exc[g + i - j]) + exc[g + i]` for i = 1..*[ebp+0xc] and j = 1..g (0x1003d6e7-0x1003d723); then the same sum
// over the history itself is added back into it and its square accumulated into the frame's energy slot
// (0x1003d799-0x1003d7df). The last g entries of both arrays are shifted down to the front
// (0x1003d81b-0x1003d838). Finally `sqrt((double)*[ebp+0xc] * (*[ebp+0x18] * *[ebp+0x18]) / energy)` (the
// product is kept unrounded in st(0) by `fst dword [ebp-0x2fc]` at 0x1003d853 and the call is at 0x1003d862)
// scales the history into the output (0x1003d89b-0x1003d8a8). Returns 0 (0x1003d8ad).
typedef int(__cdecl* CodecCombFilter_t)(void*);
typedef int(__cdecl* LpcSynthesize_t)(const float*, const int*, const int*, float*, const float*,
                                      const float*, const float*, void*);
LpcSynthesize_t LpcSynthesize_orig;
int __cdecl LpcSynthesize_re(const float* a, const int* subLen, const int* mode, float* out,
                             const float* level, const float* level2, const float* feedback, void* ctx) {
    float local[192];                                        // the frame array at [ebp + k*4 - 0x2e8]
    for (int t = 0; t < 192; ++t)
        *reinterpret_cast<unsigned*>(&local[t]) = 0xCCCCCCCCu;   // rep stosd at 0x1003d1ec

    if (a) a -= 1;                                           // je at 0x1003d1f2
    if (out) out -= 1;                                       // je at 0x1003d201

    int* prevLen = reinterpret_cast<int*>(fldAddr(ctx, 0x688));
    // Both arrays are addressed `[base + k*4 - 4]` (0x1003d300, 0x1003d374, 0x1003d723 and everywhere else),
    // so element k is base[k-1]: the pointers below are the bases less one float.
    float* exc = reinterpret_cast<float*>(fldAddr(ctx, 0x68C)) - 1;
    float* hist = reinterpret_cast<float*>(fldAddr(ctx, 0x924)) - 1;
    float* d0 = reinterpret_cast<float*>(fldAddr(ctx, 0xBBC));
    float* d1 = reinterpret_cast<float*>(fldAddr(ctx, 0xBC0));
    float* d2 = reinterpret_cast<float*>(fldAddr(ctx, 0xBC4));
    float* e0 = reinterpret_cast<float*>(fldAddr(ctx, 0xBC8));
    float* e1 = reinterpret_cast<float*>(fldAddr(ctx, 0xBCC));
    float* e2 = reinterpret_cast<float*>(fldAddr(ctx, 0xBD0));
    float* prevLevel = reinterpret_cast<float*>(fldAddr(ctx, 0xBD4));

    const float c_eps = c3bn_f(0x0005EC28);                  // 1e-06f
    const float c_eight = c3bn_f(0x0005EC24);                // 8.0f
    const float c_one = c3bn_f(0x0005B8A8);                  // 1.0f
    const float c_64 = c3bn_f(0x0005EBFC);                   // 64.0f
    const float c_zero = c3bn_f(0x0005EC00);                 // 0.0f
    const float c_p75 = c3bn_f(0x0005EC04);                  // 0.75f
    const float c_p125 = c3bn_f(0x0005EC08);                 // 0.125f
    const float c_m125 = c3bn_f(0x0005EBF8);                 // -0.125f
    const float c_p25 = c3bn_f(0x0005EBF4);                  // 0.25f
    const float c_2000 = c3bn_f(0x0005EC18);                 // 2000.0f
    const float c_342 = c3bn_f(0x0005EC1C);                  // 342.0f
    const float c_4 = c3bn_f(0x0005EC20);                    // 4.0f
    const double c_6928 = c3bn_d(0x0005EC10);                // the qword 6.927999973297119

    const int* table = reinterpret_cast<const int*>(c3bn_sound(0x00064A18));
    CodecCombFilter_t comb = reinterpret_cast<CodecCombFilter_t>(c3bn_sound(0x00040F60));

    const double ratio = static_cast<double>(*prevLevel)
                       / (static_cast<double>(*level) + static_cast<double>(c_eps));   // 0x1003d284-0x1003d28f
    const float ratioStored = static_cast<float>(ratio);     // fst dword at 0x1003d291
    const float factor = (ratio > static_cast<double>(c_eight)) ? 8.0f : ratioStored;  // je at 0x1003d29f

    c3bn_copy32(prevLevel, level);                           // 0x1003d2c2-0x1003d2ca

    int g = *reinterpret_cast<const int*>(c3bn_sound(0x000B58A0));                     // 0x1003d2cc
    for (int i = 1; i <= g; ++i)                             // jg at 0x1003d2ed
        hist[i] = static_cast<float>(static_cast<double>(factor)
                                     * static_cast<double>(hist[*prevLen + i]));       // 0x1003d2fa-0x1003d30a
    *prevLen = *subLen;                                      // 0x1003d318

    if (*mode != 0) {                                        // jne at 0x1003d320
        const float scale = static_cast<float>(
            c3bn_sqrt(static_cast<double>(*subLen)) / c_6928);                         // 0x1003d42f-0x1003d445
        int n = *subLen;
        for (int i = 1; i <= n; ++i) {                       // jg at 0x1003d46b
            *reinterpret_cast<unsigned*>(&exc[g + i]) = 0;   // 0x1003d47d
            if (i <= 0x19)                                   // jg at 0x1003d489
                exc[g + i] = static_cast<float>(static_cast<double>(table[i])
                                                * static_cast<double>(scale));         // 0x1003d48e-0x1003d4a7
            unsigned saved = *reinterpret_cast<unsigned*>(&exc[g + i]);                // 0x1003d4b7
            const float t1 = static_cast<float>(
                static_cast<double>(exc[g + i]) * static_cast<double>(c_p125)
                + static_cast<double>(*d0) * static_cast<double>(c_p75));              // 0x1003d4cd-0x1003d4e4
            const float t2 = static_cast<float>(
                static_cast<double>(*d1) * static_cast<double>(c_p125)
                + static_cast<double>(t1));                                            // 0x1003d4ea-0x1003d4f5
            exc[g + i] = static_cast<float>(
                static_cast<double>(*d2) * static_cast<double>(c_zero)
                + static_cast<double>(t2));                                            // 0x1003d4fb-0x1003d511
            c3bn_copy32(d2, d1);                             // 0x1003d51b-0x1003d51d
            c3bn_copy32(d1, d0);                             // 0x1003d525-0x1003d527
            *reinterpret_cast<unsigned*>(d0) = saved;        // 0x1003d532
        }
        n = *subLen;
        for (int i = 1; i <= n; ++i) {                       // jg at 0x1003d559
            const int v = comb(ctx);                         // 0x1003d563
            local[g + i] = static_cast<float>(
                static_cast<double>(v) * static_cast<double>(c_one)
                / static_cast<double>(c_64));                                          // 0x1003d571-0x1003d58b
            unsigned saved = *reinterpret_cast<unsigned*>(&local[g + i]);              // 0x1003d59b
            const float t1 = static_cast<float>(
                static_cast<double>(local[g + i]) * static_cast<double>(c_m125)
                + static_cast<double>(*e0) * static_cast<double>(c_p25));              // 0x1003d5b0-0x1003d5ca
            const float t2 = static_cast<float>(
                static_cast<double>(*e1) * static_cast<double>(c_m125)
                + static_cast<double>(t1));                                            // 0x1003d5d0-0x1003d5db
            local[g + i] = static_cast<float>(
                static_cast<double>(*e2) * static_cast<double>(c_zero)
                + static_cast<double>(t2));                                            // 0x1003d5e1-0x1003d5f5
            c3bn_copy32(e2, e1);                             // 0x1003d602-0x1003d604
            c3bn_copy32(e1, e0);                             // 0x1003d60c-0x1003d60e
            *reinterpret_cast<unsigned*>(e0) = saved;        // 0x1003d619
        }
        n = *subLen;
        for (int i = 1; i <= n; ++i)                         // jg at 0x1003d640
            exc[g + i] = static_cast<float>(static_cast<double>(exc[g + i])
                                            + static_cast<double>(local[g + i]));      // 0x1003d656-0x1003d66d
    } else {
        int n = *subLen;
        for (int i = 1; i <= n; ++i) {                       // jg at 0x1003d346
            const int v = comb(ctx);                         // 0x1003d34c
            exc[g + i] = static_cast<float>(v / 64);         // 0x1003d354-0x1003d374
        }
        const int v2 = comb(ctx);                            // 0x1003d37e
        const int pos = ((v2 + 0x8000) * (*subLen - 1)) / 0x10000 + g + 1;             // 0x1003d386-0x1003d3a8

        const double scaled = static_cast<double>(*level2) / static_cast<double>(c_4)
                              * static_cast<double>(c_one);                            // 0x1003d3b5-0x1003d3bd
        const double amped = scaled * static_cast<double>(c_342);                      // 0x1003d3c6
        float amp = static_cast<float>(amped);               // fst dword at 0x1003d3cc
        if (amped > static_cast<double>(c_2000)) amp = 2000.0f;                        // jne at 0x1003d3dd
        exc[pos] = static_cast<float>(static_cast<double>(amp)
                                      + static_cast<double>(exc[pos]));                // 0x1003d3f2-0x1003d405
        exc[pos + 1] = static_cast<float>(static_cast<double>(exc[pos + 1])
                                          - static_cast<double>(amp));                 // 0x1003d412-0x1003d424
    }

    float energy;
    *reinterpret_cast<unsigned*>(&energy) = 0;               // 0x1003d673
    int n = *subLen;
    for (int i = 1; i <= n; ++i) {                           // jg at 0x1003d69a
        const int k = g + i;                                 // 0x1003d6a9
        float acc;
        *reinterpret_cast<unsigned*>(&acc) = 0;              // 0x1003d6ac
        const int m = g;                                     // 0x1003d6b6
        for (int j = 1; j <= m; ++j)                         // jg at 0x1003d6d6
            acc = static_cast<float>(static_cast<double>(a[j]) * static_cast<double>(exc[k - j])
                                     + static_cast<double>(acc));                      // 0x1003d6e7-0x1003d6f4
        acc = static_cast<float>(static_cast<double>(acc) * static_cast<double>(*feedback));
                                                             // 0x1003d6ff-0x1003d707
        hist[k] = static_cast<float>(static_cast<double>(acc)
                                     + static_cast<double>(exc[k]));                   // 0x1003d713-0x1003d723
    }
    n = *subLen;
    for (int i = 1; i <= n; ++i) {                           // jg at 0x1003d74c
        const int k = g + i;                                 // 0x1003d75a
        float acc;
        *reinterpret_cast<unsigned*>(&acc) = 0;              // 0x1003d75d
        const int m = g;                                     // 0x1003d767
        for (int j = 1; j <= m; ++j)                         // jg at 0x1003d788
            acc = static_cast<float>(static_cast<double>(a[j]) * static_cast<double>(hist[k - j])
                                     + static_cast<double>(acc));                      // 0x1003d799-0x1003d7a6
        hist[k] = static_cast<float>(static_cast<double>(acc)
                                     + static_cast<double>(hist[k]));                  // 0x1003d7b4-0x1003d7c4
        energy = static_cast<float>(static_cast<double>(hist[k]) * static_cast<double>(hist[k])
                                    + static_cast<double>(energy));                    // 0x1003d7d4-0x1003d7df
    }
    for (int i = 1; i <= g; ++i) {                           // jg at 0x1003d808
        c3bn_copy32(&exc[i], &exc[*subLen + i]);             // 0x1003d81b-0x1003d81f
        c3bn_copy32(&hist[i], &hist[*subLen + i]);           // 0x1003d834-0x1003d838
    }

    const float sq = static_cast<float>(static_cast<double>(*level)
                                        * static_cast<double>(*level));                // 0x1003d844-0x1003d848
    const double product = static_cast<double>(*subLen) * static_cast<double>(sq);     // 0x1003d84e-0x1003d850
    const float gain = static_cast<float>(
        c3bn_sqrt(product / static_cast<double>(energy)));                             // 0x1003d859-0x1003d86a
    n = *subLen;
    for (int i = 1; i <= n; ++i)                             // jg at 0x1003d88d
        out[i] = static_cast<float>(static_cast<double>(gain)
                                    * static_cast<double>(hist[g + i]));               // 0x1003d89b-0x1003d8a8
    return 0;                                                // 0x1003d8ad
}

// 0x00040000  codecPitchAnalysis (VA 0x10040000)
// __cdecl with eight stack arguments ([ebp+8] the samples, [ebp+0xc] the window span, [ebp+0x10] the candidate
// lag table, [ebp+0x14] the number of lags, [ebp+0x18] the score array, [ebp+0x1c] the best-score index,
// [ebp+0x20] a second index, [ebp+0x24] the chosen lag; `ret` at 0x10040329 pops nothing), a /Od /GZ debug body.
// The samples, the score array and the lag table are each decremented by one dword on entry (0x1004001e,
// 0x10040027, 0x10040030), so every index is one-based.
// codecAmdfPitch 0x100414f0 is called three times, always with the same first two and the same fifth argument:
// the samples, the span, and the LAST lag `lags[*[ebp+0x14]]` as the window length (0x10040050, 0x10040218).
// The first call scores the whole lag table into [ebp+0x18] and leaves the index of the smallest score in
// [ebp+0x1c] and of the largest in [ebp+0x20] (0x1004006a); the chosen lag is then `lags[*[ebp+0x1c]]`
// (0x1004007d) and the running best score is __ftol of its score (0x1004008d).
// The second call refines it over the integers near the chosen lag. The range is `chosen-3 .. chosen+3`
// (0x100400af, 0x100400ba) clamped below by 0x29 (`jl 0x100400f8` at 0x100400ee) and above by
// `lags[*[ebp+0x14]] - 1` (`jg 0x100400de` at 0x100400d4); the lag table is walked once with a cursor that
// starts at `*[ebp+0x1c] - 2` (0x100400a4, `jge 0x10040131` at 0x10040124) and every integer of the range that
// is NOT already a table entry (`je 0x10040152` at 0x1004013d) is appended to a frame array at [ebp-0x5c]
// (0x1004014e). A non-empty array (`jle 0x100401b9` at 0x10040158) is scored by a second codecAmdfPitch call
// into frame slots (0x10040185) and, when that smallest score is strictly below the running best
// (`jne 0x100401b9` at 0x1004019c), the chosen lag and the best score are replaced (0x100401a8, 0x100401b6).
// THE CURSOR READS BEFORE THE TABLE when the first call left index 1 in [ebp+0x1c] (the cursor is then -1):
// a defect of the original, covered by the fixture's lead-in slots.
// The third call halves a chosen lag of 0x50 or more (`jl 0x10040270` at 0x100401bf, a SIGNED division by two
// at 0x100401ca): an even half is scored as the pair `half-1, half+1` and an odd half alone
// (`jne 0x100401f7` at 0x100401da). The same improvement test (`jne 0x10040270` at 0x10040246) replaces the
// chosen lag and the best score AND subtracts 0x14 from the best-score index (0x10040268), which is what makes
// the following store land before the score array.
// The best score is written back as a float at `scores[*[ebp+0x1c]]` (0x10040270-0x1004027b), the second index
// becomes `max(*[ebp+0x1c] - 5, 1)` (`jl 0x10040297` at 0x1004028d) and is then moved to the index of the
// LARGEST score in `*[ebp+0x20]+1 .. min(*[ebp+0x1c] + 5, *[ebp+0x14])` (`jg 0x100402c3` at 0x100402b9,
// `jne 0x10040312` at 0x10040308). Returns 0 (0x10040314).
typedef int(__cdecl* CodecAmdfPitch_t)(const float*, const int*, const int*, const int*, const int*,
                                       float*, int*, int*);
typedef int(__cdecl* CodecPitchAnalysis_t)(const float*, const int*, const int*, const int*, float*,
                                           int*, int*, int*);
CodecPitchAnalysis_t CodecPitchAnalysis_orig;
int __cdecl CodecPitchAnalysis_re(const float* samples, const int* span, const int* lags,
                                  const int* lagCount, float* scores, int* bestIdx, int* secondIdx,
                                  int* lagOut) {
    int cand[8];                                             // the frame array at [ebp + n*4 - 0x5c]
    float subScores[8];                                      // the frame array at [ebp + n*4 - 0x2c]
    int subBest = 0, subLargest = 0;                         // [ebp-0x2c] and [ebp-0x34]
    for (int t = 0; t < 8; ++t) {
        cand[t] = static_cast<int>(0xCCCCCCCCu);
        *reinterpret_cast<unsigned*>(&subScores[t]) = 0xCCCCCCCCu;   // rep stosd at 0x1004001c
    }

    samples -= 1;                                            // 0x1004001e
    scores -= 1;                                             // 0x10040027
    lags -= 1;                                               // 0x10040030

    CodecAmdfPitch_t amdf = reinterpret_cast<CodecAmdfPitch_t>(c3bn_sound(0x000414F0));
    amdf(samples + 1, span, lags + 1, lagCount, &lags[*lagCount], scores + 1, bestIdx, secondIdx);
                                                             // 0x1004003c-0x1004006a
    *lagOut = lags[*bestIdx];                                // 0x10040077-0x10040080
    int best = c3bn_ftol(static_cast<double>(scores[*bestIdx]));                       // 0x1004008a-0x10040092

    int count = 0;                                           // 0x10040095
    int cursor = *bestIdx - 2;                               // 0x100400a1-0x100400a4
    const int lo = *lagOut - 3;                              // 0x100400ac
    const int hi = *lagOut + 3;                              // 0x100400b7
    const int limit = lags[*lagCount] - 1;                   // 0x100400c5-0x100400c8
    const int end = (hi > limit) ? limit : hi;               // jg at 0x100400d4
    const int start = (lo < 0x29) ? 0x29 : lo;               // jl at 0x100400ee

    for (int i = start; i <= end; ++i) {                     // jg at 0x10040116
        while (lags[cursor] < i) ++cursor;                   // jge at 0x10040124
        if (lags[cursor] != i) {                             // je at 0x1004013d
            ++count;                                         // 0x10040142
            cand[count] = i;                                 // 0x1004014e
        }
    }

    if (count > 0) {                                         // jle at 0x10040158
        amdf(samples + 1, span, &cand[1], &count, &lags[*lagCount], &subScores[1],
             &subBest, &subLargest);                         // 0x1004015a-0x10040185
        if (static_cast<double>(best) > static_cast<double>(subScores[subBest])) {     // jne at 0x1004019c
            *lagOut = cand[subBest];                         // 0x100401a4-0x100401a8
            best = c3bn_ftol(static_cast<double>(subScores[subBest]));                 // 0x100401ad-0x100401b6
        }
    }

    if (*lagOut >= 0x50) {                                   // jl at 0x100401bf
        const int half = *lagOut / 2;                        // 0x100401ca-0x100401cf
        if ((half & 1) == 0) {                               // jne at 0x100401da
            count = 2;                                       // 0x100401dc
            cand[1] = half - 1;                              // 0x100401e9
            cand[2] = half + 1;                              // 0x100401f2
        } else {
            count = 1;                                       // 0x100401f7
            cand[1] = half;                                  // 0x10040201
        }
        amdf(samples + 1, span, &cand[1], &count, &lags[*lagCount], &subScores[1],
             &subBest, &subLargest);                         // 0x10040204-0x1004022f
        if (static_cast<double>(best) > static_cast<double>(subScores[subBest])) {     // jne at 0x10040246
            *lagOut = cand[subBest];                         // 0x1004024e-0x10040252
            best = c3bn_ftol(static_cast<double>(subScores[subBest]));                 // 0x10040257-0x10040260
            *bestIdx -= 0x14;                                // 0x10040268-0x1004026e
        }
    }

    scores[*bestIdx] = static_cast<float>(best);             // 0x10040270-0x1004027b
    const int low = *bestIdx - 5;                            // 0x10040283
    *secondIdx = (low < 1) ? 1 : low;                        // jl at 0x1004028d
    const int high = *bestIdx + 5;                           // 0x100402ab
    const int stop = (high > *lagCount) ? *lagCount : high;  // jg at 0x100402b9
    for (int i = *secondIdx + 1; i <= stop; ++i)             // jg at 0x100402ed
        if (static_cast<double>(scores[i]) > static_cast<double>(scores[*secondIdx]))  // jne at 0x10040308
            *secondIdx = i;                                  // 0x1004030d-0x10040310
    return 0;                                                // 0x10040314
}

// ---------------------------------------------------------------------------------------------------------
// sequencer
// ---------------------------------------------------------------------------------------------------------

// 0x00023610  Seq_remarkEventRange (VA 0x10023610)
// __thiscall with one stack argument (`ret 4` at 0x10023694, 0x100236f6 and 0x10023703): an event descriptor
// whose dword at +8 is the first track of a range and whose dword at +0xc is the last (0x1002361b, 0x1002361e).
// A range that runs backwards returns 0xa (`jb 0x100236f9` at 0x10023623, UNSIGNED), and so does a last track
// that is not below the count the sequencer's own vtable slot +0xb4 reports (`call dword [eax+0xb4]` at
// 0x1002362b, `jae 0x100236f9` at 0x10023633, UNSIGNED).
// The track list is the sub-object at this+0x60 and every lookup goes through Seq_listGetAt 0x1001d780 (the
// thunk at 0x100015fa), which returns 0 when it found a record and 0x24 when it did not. The first lookup asks
// for step 0 and its failure arm dereferences the out slot that the failure has just set to NULL
// (`or byte [eax+0x38], 8` at 0x10023654): a defect of the original, so only the success side of
// `je 0x10023658` at 0x1002364e is reachable without a fault.
// The sequencer's vtable slot +0x5c (`call dword [eax+0x5c]` at 0x1002365c) then picks the arm. A zero result
// marks the range only: for every track from first to last (`jbe 0x1002366d` at 0x1002368b, UNSIGNED) the
// record found by Seq_listGetAt gets bit 3 of its byte at +0x38 (`or byte [eax+0x38], 8` at 0x10023684), a
// lookup that failed being skipped (`jne 0x10023688` at 0x1002367e). A non-zero result (0x10023697) does the
// same (0x100236b6-0x100236bc) and then hands the record to Seq_scheduleEvent 0x10024b80 (the thunk at
// 0x1000240a) with the sequencer's tick at this+0x71c, the descriptor's dword at +0 as the flag byte and its
// dword at +0x20 as the track index (0x100236bf-0x100236d4), after which the record's own pointer at +0x458,
// when it is not null (`je 0x100236ea` at 0x100236e5), has its dword at +0x34 incremented (0x100236e7).
// The `ja` at 0x10023667 and at 0x10023699 re-test first against last, which `jb 0x10023623` has already
// decided, so neither can be taken. Both arms return 0 (0x10023690, 0x100236f2).
typedef int(__fastcall* SeqListGetAt_t)(void*, void*, void**, unsigned);
typedef void(__fastcall* SeqScheduleEvent_t)(void*, void*, void*, unsigned, unsigned, int);
typedef unsigned(__fastcall* SeqSlotB4_t)(void*);
typedef unsigned(__fastcall* SeqSlot5C_t)(void*);
typedef int(__fastcall* SeqRemarkEventRange_t)(void*, void*, const void*);
SeqRemarkEventRange_t SeqRemarkEventRange_orig;
int __fastcall SeqRemarkEventRange_re(void* self, void*, const void* desc) {
    const unsigned last = fldc<unsigned>(desc, 0x0C);        // 0x1002361b
    const unsigned first = fldc<unsigned>(desc, 0x08);       // 0x1002361e
    if (last < first) return 0xA;                            // jb at 0x10023623

    void** vtbl = *reinterpret_cast<void***>(self);          // 0x10023629
    SeqSlotB4_t trackCount = reinterpret_cast<SeqSlotB4_t>(vtbl[0xB4 / 4]);
    if (last >= trackCount(self)) return 0xA;                // call at 0x1002362b, jae at 0x10023633

    void* list = reinterpret_cast<char*>(self) + 0x60;       // 0x1002363d
    SeqListGetAt_t getAt = reinterpret_cast<SeqListGetAt_t>(c3bn_sound(0x0001D780));
    void* found = 0;
    if (getAt(list, 0, &found, 0) != 0)                      // 0x10023647, je at 0x1002364e
        fld<unsigned char>(found, 0x38) |= 8;                // 0x10023654 (faults: found is NULL there)

    vtbl = *reinterpret_cast<void***>(self);                 // 0x10023658
    SeqSlot5C_t selector = reinterpret_cast<SeqSlot5C_t>(vtbl[0x5C / 4]);
    if (selector(self) == 0) {                               // call at 0x1002365c, jne at 0x10023661
        for (unsigned i = first; i <= last; ++i) {           // jbe at 0x1002368b
            found = 0;
            if (getAt(list, 0, &found, i) == 0)              // 0x10023677, jne at 0x1002367e
                fld<unsigned char>(found, 0x38) |= 8;        // 0x10023684
        }
        return 0;                                            // 0x10023690
    }

    SeqScheduleEvent_t schedule =
        reinterpret_cast<SeqScheduleEvent_t>(c3bn_sound(0x00024B80));
    for (unsigned i = first; i <= last; ++i) {               // jbe at 0x100236ed
        found = 0;
        if (getAt(list, 0, &found, i) != 0) continue;        // 0x100236a5, jne at 0x100236ac
        if (!found) continue;                                // je at 0x100236b4
        fld<unsigned char>(found, 0x38) |= 8;                // 0x100236b6-0x100236bc
        schedule(self, 0, found, fldc<unsigned>(self, 0x71C),
                 fldc<unsigned>(desc, 0x00), fldc<int>(desc, 0x20));                   // 0x100236bf-0x100236d4
        void* counter = fldc<void*>(found, 0x458);           // 0x100236dd
        if (counter)                                         // je at 0x100236e5
            fld<unsigned>(counter, 0x34) += 1;               // 0x100236e7
    }
    return 0;                                                // 0x100236f2
}

SG_HOOK("sound.dll", 0x0003d8d0, sound_reflectionToLpc, ReflectionToLpc_re, ReflectionToLpc_orig);
SG_HOOK("sound.dll", 0x0003d1d0, sound_lpcSynthesize, LpcSynthesize_re, LpcSynthesize_orig);
SG_HOOK("sound.dll", 0x00040000, sound_codecPitchAnalysis, CodecPitchAnalysis_re, CodecPitchAnalysis_orig);
SG_HOOK("sound.dll", 0x00023610, sound_Seq_remarkEventRange, SeqRemarkEventRange_re,
        SeqRemarkEventRange_orig);

}  // namespace
