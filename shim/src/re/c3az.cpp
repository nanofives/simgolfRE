// C3 batch c3az (2026-10-08) of sound.dll: the ADPCM/LPC codec's arithmetic leaves, the channel voice sweeps
// and pan maths, the DirectSound buffer-description switch and three sequencer record helpers.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside
// each body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32 or COM call is made and no live sound object, device, list or buffer is touched:
// every object the A/B passes in is allocated by the fixture. Four bodies forward to an original callee at its
// own address (codecRmsEnergy -> sqrt 0x10043204, floatFloorToInt -> floor 0x1004339c, Channel_resetPanState ->
// Channel_setPanGains 0x10035340, Seq_applyTrackFlag -> Seq_listGetAt 0x1001d780, Seq_loadChannelDesc and
// Seq_mergeChannelDesc -> Sound_setName 0x1001ced0); sound.dll is relocatable, so those addresses are resolved
// against the module base at the first call.
// Floating point: the originals are x87 (/Od, every intermediate spilled), the shim is SSE2. Each `fstp dword`
// spill is written as an assignment to a float and every operand inside one expression is widened with
// static_cast<double>, which is the c3ap method: the A/B thread runs with control word 0x027f (53-bit, round to
// nearest), so an x87 step that ends in a qword slot or a register is exactly a double operation.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fld(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T fldc(const void* base, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + off);
}

// sound.dll has base relocations, so an original callee or constant is addressed through the loaded base.
char* c3az_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

// ---------------------------------------------------------------------------------------------------------
// codec arithmetic leaves
// ---------------------------------------------------------------------------------------------------------

// 0x0003e480  medianOfThree (VA 0x1003e480)
// __cdecl with three pointer arguments ([ebp+8], [ebp+0xc], [ebp+0x10]; `ret` at 0x1003e510 pops nothing), a
// /Od /GZ debug body whose only live local is the result at [ebp-4]. The result starts as *b (0x1003e49b,
// 0x1003e49d) and every compare is SIGNED.
// If *b is above BOTH *a (`jle 0x1003e4d6` at 0x1003e4aa) and *c (`jle 0x1003e4d6` at 0x1003e4b6), the result
// becomes *a (0x1003e4bb) and then *c when *c is above *a (`jle 0x1003e4d4` at 0x1003e4ca, 0x1003e4cf): the
// larger of *a and *c.
// Otherwise, if *b is below BOTH *a (`jge 0x1003e50a` at 0x1003e4e0) and *c (`jge 0x1003e50a` at 0x1003e4ec),
// the result becomes *a (0x1003e4f1) and then *c when *c is below *a (`jge 0x1003e50a` at 0x1003e500,
// 0x1003e505): the smaller of *a and *c. In every other case the result stays *b (0x1003e50a).
typedef int(__cdecl* MedianOfThree_t)(const int*, const int*, const int*);
MedianOfThree_t MedianOfThree_orig;
int __cdecl MedianOfThree_re(const int* a, const int* b, const int* c) {
    int result = *b;
    if (*b > *a && *b > *c) {               // jle at 0x1003e4aa, jle at 0x1003e4b6
        result = *a;
        if (*c > *a) result = *c;           // jle at 0x1003e4ca
    } else if (*b < *a && *b < *c) {        // jge at 0x1003e4e0, jge at 0x1003e4ec
        result = *a;
        if (*c < *a) result = *c;           // jge at 0x1003e500
    }
    return result;
}

// 0x0003e2c0  intPow (VA 0x1003e2c0)
// __cdecl with two pointer arguments ([ebp+8] the base, [ebp+0xc] the exponent; `ret` at 0x1003e377 pops
// nothing). The base is copied to [ebp-8] (0x1003e2dd) and the exponent to [ebp-0xc] (0x1003e2e5).
// A positive exponent goes straight to the square-and-multiply loop (`jg 0x1003e32f` at 0x1003e2ec, SIGNED).
// Otherwise four special cases are tested in order: exponent 0 (`je 0x1003e2fa` at 0x1003e2f2) and base 1
// (`jne 0x1003e301` at 0x1003e2f8) both return 1 (0x1003e2fa); base -1 negates the exponent (`neg ecx` at
// 0x1003e32a) and falls into the loop; base 0 computes `1 / base` with `cdq` + `idiv [ebp-8]` (0x1003e312,
// 0x1003e313), which is an integer division by zero; any other base returns 0 (0x1003e31b).
// The loop keeps the remaining exponent at [ebp-0x10] (0x1003e332) and the accumulator at [ebp-4], set to 1
// (0x1003e335). Each round multiplies the accumulator by the base when the low bit is set (`and eax, 1` at
// 0x1003e33f, `imul` at 0x1003e349), shifts the exponent right LOGICALLY (`shr edx, 1` at 0x1003e353), stops
// when it reaches 0 (`je 0x1003e36a` at 0x1003e35c) and otherwise squares the base (`imul` at 0x1003e361).
typedef int(__cdecl* IntPow_t)(const int*, const int*);
IntPow_t IntPow_orig;
int __cdecl IntPow_re(const int* basePtr, const int* expPtr) {
    int b = *basePtr;
    int e = *expPtr;
    if (e <= 0) {                                   // jg at 0x1003e2ec
        if (e == 0 || b == 1) return 1;             // je at 0x1003e2f2, jne at 0x1003e2f8
        if (b == -1) {                              // je at 0x1003e305
            e = -e;                                 // neg at 0x1003e32a
        } else if (b == 0) {                        // jne at 0x1003e30b
            // 0x1003e313: `idiv [ebp-8]` with a zero divisor raises #DE; no vector takes this side.
            volatile int divisor = b;
            return 1 / divisor;
        } else {
            return 0;                               // 0x1003e31b
        }
    }
    unsigned n = static_cast<unsigned>(e);
    int result = 1;
    for (;;) {
        if (n & 1) result *= b;                     // je at 0x1003e344
        n >>= 1;                                    // shr at 0x1003e353
        if (n == 0) break;                          // je at 0x1003e35c
        b *= b;                                     // imul at 0x1003e361
    }
    return result;
}

// 0x00040da0  codecFirstOrderFilter (VA 0x10040da0)
// __cdecl with five stack arguments ([ebp+8] the input samples, [ebp+0xc] the output samples, [ebp+0x10] the
// sample count, [ebp+0x14] a coefficient, [ebp+0x18] the one-sample filter state; `ret` at 0x10040e26 pops
// nothing). Both sample pointers are decremented by 4 bytes on entry (0x10040dbb, 0x10040dc4) and the loop
// counter runs from 1 to *count inclusive (0x10040dd2, `jg 0x10040e1e` at 0x10040dea, SIGNED), so element i of
// the loop is element i-1 of the caller's arrays.
// Each round computes `in[i] - coeff * state` with `fld dword [coeff]` / `fmul dword [state]` /
// `fsubr dword [in+i*4]` (0x10040df2-0x10040dfc) and spills it to a float slot (`fstp dword [ebp-8]` at
// 0x10040dff). The state is then replaced by the raw dword of in[i] (0x10040e0b, 0x10040e0e) and the spilled
// value written to out[i] (0x10040e19). The product and the subtraction both round at the thread's 53-bit
// precision control, the spill rounds to float; the accumulator is read before it is overwritten.
// Returns 0 (`xor eax, eax` at 0x10040e1e).
typedef int(__cdecl* CodecFirstOrder_t)(const float*, float*, const int*, const float*, float*);
CodecFirstOrder_t CodecFirstOrder_orig;
int __cdecl CodecFirstOrder_re(const float* in, float* out, const int* count, const float* coeff, float* state) {
    in -= 1;                                        // sub at 0x10040dc4
    out -= 1;                                       // sub at 0x10040dbb
    const int n = *count;
    for (int i = 1; i <= n; ++i) {
        float spilled = static_cast<float>(static_cast<double>(in[i])
                                           - static_cast<double>(*coeff) * static_cast<double>(*state));
        *reinterpret_cast<unsigned*>(state) = *reinterpret_cast<const unsigned*>(&in[i]);   // dword copy
        out[i] = spilled;
    }
    return 0;
}

// 0x0003ed00  codecRemoveMean (VA 0x1003ed00)
// __cdecl with three stack arguments ([ebp+8] the sample count, [ebp+0xc] the input samples, [ebp+0x10] the
// output samples; `ret` at 0x1003edb0 pops nothing). Both sample pointers are decremented by 4 bytes
// (0x1003ed1b, 0x1003ed24) and both loops run from 1 to *count inclusive (`jg` at 0x1003ed51 and 0x1003ed8f,
// SIGNED), so element i is the caller's element i-1.
// The accumulator at [ebp-8] starts as the integer 0 written into a float slot (0x1003ed2a). The first loop
// adds each sample to it and spills it back as a float (`fld dword [ebp-8]` / `fadd dword [in+i*4]` /
// `fstp dword [ebp-8]` at 0x1003ed59-0x1003ed5f). The mean then replaces it: `fild dword [count]`
// (0x1003ed67, the count re-read through the pointer and converted as a SIGNED 32-bit integer),
// `fdivr dword [ebp-8]` (0x1003ed69, so accumulator / count) and `fstp dword [ebp-8]` (0x1003ed6c).
// The count is re-read for the second loop (0x1003ed72), which writes in[i] - mean to out[i]
// (`fld` / `fsub dword [ebp-8]` / `fstp dword [out+i*4]` at 0x1003ed97-0x1003eda3). Every arithmetic step
// rounds at 53 bits and every spill rounds to float. Returns 0 (`xor eax, eax` at 0x1003eda8).
typedef int(__cdecl* CodecRemoveMean_t)(const int*, const float*, float*);
CodecRemoveMean_t CodecRemoveMean_orig;
int __cdecl CodecRemoveMean_re(const int* count, const float* in, float* out) {
    out -= 1;                                       // sub at 0x1003ed1b
    in -= 1;                                        // sub at 0x1003ed24
    float acc = 0.0f;
    int n = *count;
    for (int i = 1; i <= n; ++i)
        acc = static_cast<float>(static_cast<double>(acc) + static_cast<double>(in[i]));
    acc = static_cast<float>(static_cast<double>(acc) / static_cast<double>(*count));
    n = *count;
    for (int i = 1; i <= n; ++i)
        out[i] = static_cast<float>(static_cast<double>(in[i]) - static_cast<double>(acc));
    return 0;
}

// 0x0003ec60  codecRmsEnergy (VA 0x1003ec60)
// __cdecl with three stack arguments ([ebp+8] the sample count, [ebp+0xc] the samples, [ebp+0x10] the
// accumulator slot; `ret` at 0x1003ecf9 pops nothing). The sample pointer is decremented by 4 bytes
// (0x1003ec7b) and the loop runs from 1 to *count inclusive (`jg 0x1003ecca` at 0x1003ecaa, SIGNED), so
// element i is the caller's element i-1.
// The accumulator is zeroed through its own pointer (`mov dword [ecx], 0` at 0x1003ec84) and each round adds
// one squared sample to it: `fld dword [buf+i*4]` / `fmul dword [buf+i*4]` / `fadd dword [out]` /
// `fstp dword [out]` (0x1003ecb8-0x1003ecc6), so the square and the sum round at 53 bits and the spill rounds
// to float. Afterwards `fild dword [count]` (0x1003eccd, the count re-read and converted as a SIGNED 32-bit
// integer) and `fdivr dword [out]` (0x1003ecd2) leave accumulator / count, which is pushed as a double
// (`fstp qword [esp]` at 0x1003ecd7) to the CRT's sqrt at 0x10043204 (0x1003ecda); the result is stored back
// as a float (0x1003ece5). The reimplementation calls that original sqrt at its own address.
// Returns 0 (`xor eax, eax` at 0x1003ece7).
typedef double(__cdecl* Sqrt_t)(double);
typedef int(__cdecl* CodecRmsEnergy_t)(const int*, const float*, float*);
CodecRmsEnergy_t CodecRmsEnergy_orig;
int __cdecl CodecRmsEnergy_re(const int* count, const float* buf, float* out) {
    buf -= 1;                                       // sub at 0x1003ec7b
    *out = 0.0f;                                    // 0x1003ec84
    const int n = *count;
    for (int i = 1; i <= n; ++i)
        *out = static_cast<float>(static_cast<double>(buf[i]) * static_cast<double>(buf[i])
                                  + static_cast<double>(*out));
    Sqrt_t sqrtOrig = reinterpret_cast<Sqrt_t>(c3az_sound(0x00043204));
    *out = static_cast<float>(sqrtOrig(static_cast<double>(*out) / static_cast<double>(*count)));
    return 0;
}

// 0x0003e400  floatFloorToInt (VA 0x1003e400)
// __cdecl with one pointer argument ([ebp+8]; `ret` at 0x1003e47e pops nothing), a /Od /GZ debug body that
// rounds a float half away from zero and returns it as an int.
// The sign test is `fld dword [arg]` / `fcomp dword [0x1005ec00]` / `fnstsw ax` / `test ah, 1` / `jne 0x1003e448`
// (0x1003e41b-0x1003e428): the constant at 0x1005ec00 is 0.0f in the image (bytes 00 00 00 00 at RVA 0x5ec00)
// and bit 0 of ah is C0, which the compare sets for "below" and for "unordered", so the jump is taken when the
// value is below zero or is a NaN.
// The non-negative arm adds the double 0.5 at 0x1005b820 (bytes 00 00 00 00 00 00 e0 3f at RVA 0x5b820) with
// `fadd qword` (0x1003e42f), pushes the sum as a double and calls the CRT's floor at 0x1004339c (0x1003e43b).
// The negative arm computes 0.5 - value with `fsubr qword` (0x1003e44d), floors that and negates the result
// (`fchs` at 0x1003e461). Either way the double is stored at [ebp-8] and converted by the CRT's __ftol at
// 0x10042550 (0x1003e469), which does a 64-bit `fistp` and keeps the low dword.
// The reimplementation calls the original floor at its own address, reads both constants out of the module and
// reproduces __ftol as (int)(long long).
typedef double(__cdecl* Floor_t)(double);
typedef int(__cdecl* FloatFloorToInt_t)(const float*);
FloatFloorToInt_t FloatFloorToInt_orig;
int __cdecl FloatFloorToInt_re(const float* p) {
    const float zero = *reinterpret_cast<const float*>(c3az_sound(0x0005ec00));    // 0.0f
    const double half = *reinterpret_cast<const double*>(c3az_sound(0x0005b820));  // 0.5
    Floor_t floorOrig = reinterpret_cast<Floor_t>(c3az_sound(0x0004339c));
    const float x = *p;
    double d;
    if (!(x >= zero))                                   // C0 set: below or unordered (0x1003e425)
        d = -floorOrig(half - static_cast<double>(x));  // fsubr at 0x1003e44d, fchs at 0x1003e461
    else
        d = floorOrig(static_cast<double>(x) + half);   // fadd at 0x1003e42f
    return static_cast<int>(static_cast<long long>(d)); // __ftol at 0x10042550
}

// 0x00040f60  codecCombFilter (VA 0x10040f60)
// __cdecl with one pointer argument ([ebp+8]; `ret` at 0x10041017 pops nothing). Three addresses are derived
// from it: a cursor at +0xbd8 (0x10040f7b), a second cursor at +0xbdc (0x10040f86) and the base of a 16-bit
// delay line at +0xbe0 (0x10040f92). Both cursors are 1-based: every access is `[base + cursor*2 - 2]`.
// The entry under the second cursor is replaced by the 16-BIT sum of itself and the entry under the first
// (`mov cx, [...]` at 0x10040fab, `add cx, [...]` at 0x10040fb0, `mov [...], cx` at 0x10040fbd), so the add
// wraps at 16 bits. That entry is then re-read and sign-extended as the return value (`movsx` at 0x10040fca,
// stored at 0x10040fcf and loaded into eax at 0x1004100e).
// Both cursors are then decremented and wrapped: `sub edx, 1` / `cmp dword [...], 0` / `jg` / `mov dword
// [...], 5` for the second cursor (0x10040fd7-0x10040fea, SIGNED) and the same for the first
// (0x10040ff5-0x10041008), so each runs 5, 4, 3, 2, 1, 5, ... and the delay line holds five entries.
typedef int(__cdecl* CodecCombFilter_t)(void*);
CodecCombFilter_t CodecCombFilter_orig;
int __cdecl CodecCombFilter_re(void* ctx) {
    int* cursorA = reinterpret_cast<int*>(reinterpret_cast<char*>(ctx) + 0xbd8);
    int* cursorB = reinterpret_cast<int*>(reinterpret_cast<char*>(ctx) + 0xbdc);
    short* line = reinterpret_cast<short*>(reinterpret_cast<char*>(ctx) + 0xbe0);
    unsigned short sum = static_cast<unsigned short>(
        static_cast<unsigned short>(line[*cursorB - 1]) + static_cast<unsigned short>(line[*cursorA - 1]));
    line[*cursorB - 1] = static_cast<short>(sum);
    int result = line[*cursorB - 1];                // movsx at 0x10040fca
    *cursorB -= 1;
    if (*cursorB <= 0) *cursorB = 5;                // jg at 0x10040fe5
    *cursorA -= 1;
    if (*cursorA <= 0) *cursorA = 5;                // jg at 0x10041003
    return result;
}

// ---------------------------------------------------------------------------------------------------------
// channel voice sweeps and pan maths
// ---------------------------------------------------------------------------------------------------------
// A channel carries a flag dword at +0x58 and 0x10 voice records of 0x110 bytes; the sweeps below walk them
// through the pointer `lea edx, [ecx+0x2d4]` (0x10034ae1, 0x10034baf), so the fields they touch are named here
// relative to that pointer: -0x44 and -0x40 are the two dwords of a double, -0x3c and -0x38 a second one,
// -8 a flag byte, 0 the voice's own flag byte, +0x48 a dword and +0x54 another double.

// 0x00034ac0  Channel_releaseVoices (VA 0x10034ac0)
// __thiscall with no stack argument (`ret` at 0x10034b42, 0x10034b4a, 0x10034b50). Three guards read the flag
// dword at [ecx+0x58] (0x10034ac0): bit 6 clear (`test al, 0x40` / `je 0x10034b4b` at 0x10034ac3) and bit 4
// set (`test al, 0x10` / `jne 0x10034b4b` at 0x10034acb) both return 0x15 without a write; bit 0 clear
// (`test al, 1` / `je 0x10034b43` at 0x10034acf) and bit 28 set (`test eax, 0x10000000` / `jne 0x10034b43` at
// 0x10034ad3) both set bit 3 of the flag dword (0x10034b43, 0x10034b45) and return 0.
// Otherwise the 0x10 voices are swept with a walking bit in esi (1 at 0x10034adc, `shl esi, 1` at 0x10034b33)
// and a counter of 0x10 (`dec edi` / `jne 0x10034aec` at 0x10034b3b). A voice is only examined while that bit
// is non-zero (`test esi, esi` / `je 0x10034b33` at 0x10034aec, which cannot fail for 16 shifts of 1) and
// while bits 12..27 of the flag dword are not all clear (`test dword [ecx+0x58], 0xffff000` / `je 0x10034b33`
// at 0x10034af0).
// The examined voice compares its two doubles with the qword 0.0 at 0x1005b498 (bytes 00 00 00 00 00 00 00 00
// at RVA 0x5b498): `fcomp` + `test ah, 0x40` + `jne 0x10034b1e` at 0x10034afc-0x10034b07 and again at
// 0x10034b0c-0x10034b17. Bit 6 of ah is C3, which the compare sets for "equal" and for "unordered", so the
// jump is taken when the value is zero or a NaN. If BOTH are non-zero and ordered, bit 3 of the voice's flag
// byte is set (`or byte [edx], 8` at 0x10034b19); otherwise the dword at +0x48 is zeroed (0x10034b20), bit 2
// of the flag byte is set (0x10034b27, 0x10034b29) and bit 0 of the byte at -8 is cleared (0x10034b2e).
// Returns 0 (0x10034b3f).
typedef int(__fastcall* ChannelReleaseVoices_t)(void*, void*);
ChannelReleaseVoices_t ChannelReleaseVoices_orig;
int __fastcall ChannelReleaseVoices_re(void* self, void*) {
    unsigned flags = fld<unsigned>(self, 0x58);
    if ((flags & 0x40) == 0) return 0x15;                   // je at 0x10034ac5
    if ((flags & 0x10) != 0) return 0x15;                   // jne at 0x10034acd
    if ((flags & 1) == 0 || (flags & 0x10000000) != 0) {    // je at 0x10034ad1, jne at 0x10034ad8
        fld<unsigned>(self, 0x58) = flags | 8;              // 0x10034b43
        return 0;
    }
    unsigned walk = 1;
    for (int i = 0; i < 0x10; ++i) {
        if (walk != 0 && (fld<unsigned>(self, 0x58) & 0x0ffff000u) != 0) {
            char* v = reinterpret_cast<char*>(self) + 0x2d4 + i * 0x110;
            const double d1 = *reinterpret_cast<const double*>(v - 0x44);
            const double d2 = *reinterpret_cast<const double*>(v + 0x54);
            // C3 (equal or unordered) on either compare takes the clearing arm at 0x10034b1e.
            bool clearing = (!(d1 < 0.0) && !(d1 > 0.0)) || (!(d2 < 0.0) && !(d2 > 0.0));
            unsigned char* vflag = reinterpret_cast<unsigned char*>(v);
            if (clearing) {
                *reinterpret_cast<unsigned*>(v + 0x48) = 0;
                *vflag = static_cast<unsigned char>(*vflag | 4);
                *reinterpret_cast<unsigned char*>(v - 8) =
                    static_cast<unsigned char>(*reinterpret_cast<unsigned char*>(v - 8) & 0xfe);
            } else {
                *vflag = static_cast<unsigned char>(*vflag | 8);
            }
        }
        walk <<= 1;
    }
    return 0;
}

// 0x00034b90  Channel_stopVoices (VA 0x10034b90)
// __thiscall with no stack argument (`ret` at 0x10034c05, 0x10034c3c, 0x10034c43). The same two guards as
// Channel_releaseVoices read [ecx+0x58] (0x10034b90) and return 0x15: bit 6 clear (`test al, 0x40` /
// `je 0x10034c3d` at 0x10034b94) and bit 4 set (`test al, 0x10` / `jne 0x10034c3d` at 0x10034b9c).
// Bit 0 clear (`test al, 1` / `je 0x10034c06` at 0x10034ba4) takes a separate arm at 0x10034c06 that parks the
// channel's two pending dwords: [ecx+0x1e0] is copied to [ecx+0x210] (0x10034c06, 0x10034c14) and
// [ecx+0x1e4] to [ecx+0x214] (0x10034c1a, 0x10034c33), both sources are zeroed (0x10034c24, 0x10034c2a) and
// bit 3 is set in BOTH flag dwords, [ecx+0x58] (0x10034c0c, 0x10034c0e) and [ecx+0x5c] (0x10034c11,
// 0x10034c22, 0x10034c30). Returns 0.
// Otherwise the 0x10 voices are swept exactly as in Channel_releaseVoices (walking bit at 0x10034baa and
// 0x10034bf5, counter at 0x10034bfd, mask test at 0x10034bc0). A voice is handled only when its double at
// +0x54 compares BELOW the qword 0.0 at 0x1005b498 (`fcomp` + `test ah, 1` + `je 0x10034bf5` at
// 0x10034bcc-0x10034bd7; bit 0 of ah is C0, set for "below" and for "unordered") and neither bit 3 nor bit 5
// of its flag byte is already set (`test al, 0x28` / `jne 0x10034bf5` at 0x10034bdb). It then sets both those
// bits (0x10034bdf) and moves the double at -0x44 to -0x3c dword by dword, zeroing the source
// (0x10034be3-0x10034bf2). Returns 0 (0x10034c02).
typedef int(__fastcall* ChannelStopVoices_t)(void*, void*);
ChannelStopVoices_t ChannelStopVoices_orig;
int __fastcall ChannelStopVoices_re(void* self, void*) {
    unsigned flags = fld<unsigned>(self, 0x58);
    if ((flags & 0x40) == 0) return 0x15;                   // je at 0x10034b96
    if ((flags & 0x10) != 0) return 0x15;                   // jne at 0x10034b9e
    if ((flags & 1) == 0) {                                 // je at 0x10034ba6
        unsigned pendingA = fld<unsigned>(self, 0x1e0);
        fld<unsigned>(self, 0x58) = flags | 8;
        unsigned flags2 = fld<unsigned>(self, 0x5c);
        fld<unsigned>(self, 0x210) = pendingA;
        unsigned pendingB = fld<unsigned>(self, 0x1e4);
        fld<unsigned>(self, 0x1e0) = 0;
        fld<unsigned>(self, 0x1e4) = 0;
        fld<unsigned>(self, 0x5c) = flags2 | 8;
        fld<unsigned>(self, 0x214) = pendingB;
        return 0;
    }
    unsigned walk = 1;
    for (int i = 0; i < 0x10; ++i) {
        if (walk != 0 && (fld<unsigned>(self, 0x58) & 0x0ffff000u) != 0) {
            char* v = reinterpret_cast<char*>(self) + 0x2d4 + i * 0x110;
            const double d = *reinterpret_cast<const double*>(v + 0x54);
            if (!(d >= 0.0)) {                              // C0 set: below or unordered (0x10034bd4)
                unsigned char vf = *reinterpret_cast<unsigned char*>(v);
                if ((vf & 0x28) == 0) {                     // jne at 0x10034bdd
                    *reinterpret_cast<unsigned char*>(v) = static_cast<unsigned char>(vf | 0x28);
                    *reinterpret_cast<unsigned*>(v - 0x3c) = *reinterpret_cast<unsigned*>(v - 0x44);
                    unsigned high = *reinterpret_cast<unsigned*>(v - 0x40);
                    *reinterpret_cast<unsigned*>(v - 0x44) = 0;
                    *reinterpret_cast<unsigned*>(v - 0x38) = high;
                    *reinterpret_cast<unsigned*>(v - 0x40) = 0;
                }
            }
        }
        walk <<= 1;
    }
    return 0;
}

// 0x00035340  Channel_setPanGains (VA 0x10035340)
// __thiscall with one stack argument (`ret 4` at 0x1003539e and 0x10035414); eax is not a return value.
// The argument is clamped to -0x40..0x3f with two SIGNED compares (`jge 0x10035351` at 0x10035348 and
// `jle 0x1003535b` at 0x10035354) and stored at [ecx+0x224] (0x10035364). The channel's base level is read
// from [ecx+0x1d8] (0x1003535c) before the clamp is stored.
// A clamped value of 0 (`jne 0x100353a1` at 0x1003536a) copies the centred pair straight through: the base
// level into [ecx+0x1f8] and [ecx+0x1e8] (0x10035378, 0x1003537e) and the two dwords of the double at
// [ecx+0x1e0] / [ecx+0x1e4] into [ecx+0x200] / [ecx+0x1f0] and [ecx+0x204] / [ecx+0x1f4]
// (0x10035384-0x10035396).
// Any other value splits the base level: the divisor is 0x3f plus the sign bit of the argument (`setl al` +
// `add eax, 0x3f` at 0x100353a6, so 0x40 for a negative pan and 0x3f for a positive one), the product
// base * pan is formed with a 32-bit `imul` (0x100353b0) and divided by it with `cdq` + `idiv` (0x100353b3,
// 0x100353b4). The quotient is added to the base for one side (`lea edx, [eax+esi]` at 0x100353b7) and
// subtracted for the other (`sub esi, eax` at 0x100353ba); each is clamped to at most 0x10000 with a SIGNED
// compare (`jle 0x100353d5` at 0x100353ca, `jle 0x100353e6` at 0x100353db). The two gains are stored as ints
// at [ecx+0x1e8] and [ecx+0x1f8] (0x100353ea, 0x100353f0) and as doubles at [ecx+0x200] and [ecx+0x1f0]
// (`fild dword` + `fmul qword [0x1005c5e8]` + `fstp qword` at 0x100353e6-0x1003540d). The constant at
// 0x1005c5e8 is 1/65536 in the image (bytes 00 00 00 00 00 00 f0 3e at RVA 0x5c5e8), so the conversion and the
// multiply are both exact and a double multiply reproduces them.
typedef void(__fastcall* ChannelSetPanGains_t)(void*, void*, int);
ChannelSetPanGains_t ChannelSetPanGains_orig;
void __fastcall ChannelSetPanGains_re(void* self, void*, int pan) {
    if (pan < -0x40) pan = -0x40;                   // jge at 0x10035348
    else if (pan > 0x3f) pan = 0x3f;                // jle at 0x10035354
    const int base = fld<int>(self, 0x1d8);
    fld<int>(self, 0x224) = pan;
    if (pan == 0) {                                 // jne at 0x1003536a
        const int lo = fld<int>(self, 0x1e0);
        const int hi = fld<int>(self, 0x1e4);
        fld<int>(self, 0x1f8) = base;
        fld<int>(self, 0x1e8) = base;
        fld<int>(self, 0x200) = lo;
        fld<int>(self, 0x1f0) = lo;
        fld<int>(self, 0x204) = hi;
        fld<int>(self, 0x1f4) = hi;
        return;
    }
    const int divisor = (pan < 0 ? 1 : 0) + 0x3f;   // setl + add at 0x100353a6, 0x100353a9
    const int product = static_cast<int>(static_cast<unsigned>(base) * static_cast<unsigned>(pan));
    const int share = product / divisor;            // idiv at 0x100353b4
    int gainHigh = share + base;                    // lea at 0x100353b7
    int gainLow = base - share;                     // sub at 0x100353ba
    if (gainHigh > 0x10000) gainHigh = 0x10000;     // jle at 0x100353ca
    if (gainLow > 0x10000) gainLow = 0x10000;       // jle at 0x100353db
    const double scale = *reinterpret_cast<const double*>(c3az_sound(0x0005c5e8));   // 1/65536
    fld<int>(self, 0x1e8) = gainLow;
    fld<int>(self, 0x1f8) = gainHigh;
    fld<double>(self, 0x200) = static_cast<double>(gainHigh) * scale;
    fld<double>(self, 0x1f0) = static_cast<double>(gainLow) * scale;
}

// 0x00033ba0  Channel_resetPanState (VA 0x10033ba0)
// __thiscall with no stack argument (`ret` at 0x10033c31); eax is not a return value.
// The flag dword at [ecx+0x58] is rebuilt from four of its own bits and then the whole pair [ecx+0x58] /
// [ecx+0x5c] is cleared before the new value is written back, so the rebuilt word keeps nothing else: the
// copies that feed it are read at 0x10033ba1, 0x10033bad, 0x10033bb2 and 0x10033bb8, the pair is zeroed at
// 0x10033bbc and 0x10033bc2, and the word that the mask 0x5ffffffa is applied to is re-read AFTER that store
// (`mov ebx, [edx]` at 0x10033bc9), so it is 0 and the mask contributes nothing.
// The four surviving bits are bit 31 (`sar eax, 0x1f` at 0x10033ba9), bit 29 (`shl eax, 2` + `sar eax, 0x1f` +
// `and eax, 1` at 0x10033bbf-0x10033bd1), bit 2 (`shl esi, 0x1d` + `sar` + `and esi, 1` at 0x10033bce-
// 0x10033be5) and bit 0 (`shl edi, 0x1f` + `sar` + `and edi, 1` at 0x10033be0-0x10033bf0). They are combined
// as (((bit29 | (bit31 ? 0xfffffffc : 0)) << 0x1b) | bit2) << 2 | bit0 (`shl eax, 0x1b` at 0x10033beb,
// `or eax, esi` at 0x10033bee, `shl eax, 2` at 0x10033bf3, `xor ebx, edi` at 0x10033bf6, `or eax, ebx` at
// 0x10033bf8) and stored at 0x10033bfa.
// Three dwords are then restored from their parked copies: [ecx+0x210] to [ecx+0x1e0] (0x10033bfc,
// 0x10033c08), [ecx+0x208] to [ecx+0x1d8] (0x10033c02, 0x10033c14) and [ecx+0x214] to [ecx+0x1e4]
// (0x10033c1a, 0x10033c21). Finally the pan value at [ecx+0x224], read before any of that is overwritten
// (0x10033c0e), is passed to Channel_setPanGains (VA 0x10035340, through the incremental-link thunk at
// 0x10001a7d) with the channel still in ecx. The reimplementation calls that callee at its own address.
typedef void(__fastcall* ChannelResetPanState_t)(void*, void*);
ChannelResetPanState_t ChannelResetPanState_orig;
void __fastcall ChannelResetPanState_re(void* self, void*) {
    const int flags = fld<int>(self, 0x58);
    const int bit31 = flags >> 31;                                              // sar at 0x10033ba9
    const int bit29 = (static_cast<int>(static_cast<unsigned>(flags) << 2) >> 31) & 1;
    const int bit2 = (static_cast<int>(static_cast<unsigned>(flags) << 0x1d) >> 31) & 1;
    const int bit0 = (static_cast<int>(static_cast<unsigned>(flags) << 0x1f) >> 31) & 1;
    fld<int>(self, 0x58) = 0;
    fld<int>(self, 0x5c) = 0;
    const int masked = fld<int>(self, 0x58) & 0x5ffffffa;                       // re-read after the store: 0
    unsigned rebuilt = static_cast<unsigned>(bit29 | (bit31 << 2));             // or at 0x10033be3
    rebuilt <<= 0x1b;                                                           // shl at 0x10033beb
    rebuilt |= static_cast<unsigned>(bit2);                                     // or at 0x10033bee
    rebuilt <<= 2;                                                              // shl at 0x10033bf3
    rebuilt |= static_cast<unsigned>(masked ^ bit0);                            // xor/or at 0x10033bf6
    fld<unsigned>(self, 0x58) = rebuilt;
    const int parkedA = fld<int>(self, 0x210);
    const int parkedBase = fld<int>(self, 0x208);
    fld<int>(self, 0x1e0) = parkedA;
    const int pan = fld<int>(self, 0x224);
    fld<int>(self, 0x1d8) = parkedBase;
    const int parkedB = fld<int>(self, 0x214);
    fld<int>(self, 0x1e4) = parkedB;
    ChannelSetPanGains_t gains = reinterpret_cast<ChannelSetPanGains_t>(c3az_sound(0x00035340));
    gains(self, 0, pan);
}

// ---------------------------------------------------------------------------------------------------------
// DirectSound buffer description
// ---------------------------------------------------------------------------------------------------------

// 0x00010f30  DsBuffer::configureDesc (VA 0x10010f30)
// __thiscall with one stack argument (`ret 4` at 0x10010f40, 0x10010f57, 0x10010f88, 0x10010fc4); no
// DirectSound interface is used, the body only fills this object's own description block.
// A buffer that already has its interface at [edx+0x60] is refused with 6 (`test eax, eax` /
// `je 0x10010f43` at 0x10010f36, return at 0x10010f3a). The argument then selects one of two layouts with a
// two-case chain (`sub eax, 0` / `je 0x10010f8b` at 0x10010f4c and `dec eax` / `je 0x10010f5a` at 0x10010f4f);
// anything else returns 0xa (0x10010f51).
// Both layouts put bit 1 of the object's own first dword equal to bit 0 of the argument (`and esi, 1` +
// `shl esi, 1` + `and ecx, 0xfffffffd` + `or esi, ecx` at 0x10010f6c-0x10010f7a and again at
// 0x10010f8d-0x10010f9b) and both return 0.
// Case 0 (0x10010f8b) points [edx+0x80] at this object's own +0xa8 (`lea eax, [edx+0xa8]` at 0x10010f95,
// stored at 0x10010f9d) and writes the four dwords 0x24, 0xe8, 0x10000 and 0 at +0x70, +0x74, +0x78 and +0x7c
// (0x10010fa5-0x10010fba).
// Case 1 (0x10010f5a) first zeroes nine dwords from +0x70 (`rep stosd` with ecx = 9 at 0x10010f5f, 0x10010f68)
// and then writes 0x24 at +0x70 (0x10010f74) and 1 at +0x74 (0x10010f80), leaving +0x78 .. +0x90 zero.
typedef int(__fastcall* DsBufferConfigureDesc_t)(void*, void*, unsigned);
DsBufferConfigureDesc_t DsBufferConfigureDesc_orig;
int __fastcall DsBufferConfigureDesc_re(void* self, void*, unsigned mode) {
    if (fld<unsigned>(self, 0x60) != 0) return 6;           // je at 0x10010f38
    if (mode == 0) {                                        // je at 0x10010f4c
        fld<void*>(self, 0x80) = reinterpret_cast<char*>(self) + 0xa8;
        fld<unsigned>(self, 0) = (fld<unsigned>(self, 0) & 0xfffffffdu) | ((mode & 1) << 1);
        fld<unsigned>(self, 0x70) = 0x24;
        fld<unsigned>(self, 0x74) = 0xe8;
        fld<unsigned>(self, 0x78) = 0x10000;
        fld<unsigned>(self, 0x7c) = 0;
        return 0;
    }
    if (mode == 1) {                                        // je at 0x10010f4f
        for (int i = 0; i < 9; ++i) fld<unsigned>(self, 0x70 + i * 4) = 0;
        fld<unsigned>(self, 0x70) = 0x24;
        fld<unsigned>(self, 0) = (fld<unsigned>(self, 0) & 0xfffffffdu) | ((mode & 1) << 1);
        fld<unsigned>(self, 0x74) = 1;
        return 0;
    }
    return 0xa;                                             // 0x10010f51
}

// ---------------------------------------------------------------------------------------------------------
// sequencer channel descriptors
// ---------------------------------------------------------------------------------------------------------

// 0x00023e10  Seq_applyTrackFlag (VA 0x10023e10)
// __thiscall with one stack argument (`ret 4` at 0x10023e64 and 0x10023e6f), the descriptor to apply.
// A null descriptor returns 0xa (`test esi, esi` / `je 0x10023e67` at 0x10023e17). The sequencer's own track
// count is fetched through its vtable slot +0xb4 (`mov eax, [edi]` / `call dword [eax+0xb4]` at 0x10023e1d,
// 0x10023e22, with the object still in ecx and nothing pushed) and the descriptor's index at +0x28
// (0x10023e1f) is rejected when it is not below that count (`cmp ebx, eax` / `jae 0x10023e67` at 0x10023e28,
// UNSIGNED), again returning 0xa.
// The track record is then looked up with Seq_listGetAt (VA 0x1001d780, through the incremental-link thunk at
// 0x10023e35) on the list at this+0x60 (`lea ecx, [edi+0x60]` at 0x10023e32), with the index as the step count
// and the stack slot that held the descriptor argument as the out pointer (`lea ecx, [esp+0x10]` at
// 0x10023e2c). The record it writes there is read back at 0x10023e3a.
// Bit 1 of the descriptor's byte at +0x2c is copied into bit 4 of the record's byte at +0x38 (`and dl, 2` at
// 0x10023e41, `shl dl, 3` at 0x10023e48, `and cl, 0xef` at 0x10023e4b, `or dl, cl` at 0x10023e4e, stored at
// 0x10023e50) and the descriptor's dword at +0x30 is copied to the record's dword at +0x460 (0x10023e57,
// 0x10023e5b). Returns 0 (0x10023e61).
// The reimplementation calls the original Seq_listGetAt at its own address.
typedef int(__fastcall* SeqListGetAt_t)(void*, void*, void**, unsigned);
typedef int(__fastcall* SeqApplyTrackFlag_t)(void*, void*, const void*);
SeqApplyTrackFlag_t SeqApplyTrackFlag_orig;
int __fastcall SeqApplyTrackFlag_re(void* self, void*, const void* desc) {
    if (!desc) return 0xa;                                  // je at 0x10023e1b
    void** vtbl = *reinterpret_cast<void***>(self);
    typedef unsigned(__fastcall* TrackCount_t)(void*);
    unsigned count = reinterpret_cast<TrackCount_t>(vtbl[0xb4 / 4])(self);
    unsigned index = fldc<unsigned>(desc, 0x28);
    if (index >= count) return 0xa;                         // jae at 0x10023e2a
    void* record = 0;
    SeqListGetAt_t getAt = reinterpret_cast<SeqListGetAt_t>(c3az_sound(0x0001d780));
    getAt(reinterpret_cast<char*>(self) + 0x60, 0, &record, index);
    unsigned char bit = static_cast<unsigned char>((fldc<unsigned char>(desc, 0x2c) & 2) << 3);
    unsigned char kept = static_cast<unsigned char>(fld<unsigned char>(record, 0x38) & 0xef);
    fld<unsigned char>(record, 0x38) = static_cast<unsigned char>(bit | kept);
    fld<int>(record, 0x460) = fldc<int>(desc, 0x30);
    return 0;
}

// 0x00024280  Seq_loadChannelDesc (VA 0x10024280)
// __thiscall with one stack argument (`ret 4` at 0x1002428d and 0x100242e5), the source descriptor.
// A null source returns 0xa (`test edx, edx` / `jne 0x10024290` at 0x10024284). Otherwise five dwords are
// copied straight across at the same offsets: +0 (0x10024290, 0x10024293), +8 (0x10024295, 0x10024298), +0xc
// (0x1002429b, 0x1002429e), +0x10 (0x100242a1, 0x100242a4) and +0x14 (0x100242a7, 0x100242ad). The
// destination's own dword at +4 is read before that last store (0x100242aa).
// Two bits of the source's dword at +4 are then merged into the destination's: bit 0 with `shl eax, 0x1f` /
// `sar eax, 0x1f` / `xor eax, esi` / `and eax, 1` / `xor eax, esi` (0x100242b3-0x100242be, stored at
// 0x100242c0) and bit 1 with `shl esi, 0x1e` / `sar esi, 0x1e` / `xor esi, eax` / `and esi, 2` / `xor esi, eax`
// (0x100242c6-0x100242d1, stored at 0x100242d3). Each is the standard "copy one bit" idiom, so the destination
// keeps every other bit of its own +4.
// Finally the source's name pointer at +0x1c (0x100242d6) is passed to Sound_setName (VA 0x1001ced0, through
// the incremental-link thunk at 0x100242da), which frees the destination's old name at +0x1c and, for a
// non-null argument, allocates a copy. Returns 0xb (0x100242df).
// The reimplementation calls that original callee at its own address.
typedef int(__fastcall* SoundSetName_t)(void*, void*, const char*);
typedef int(__fastcall* SeqLoadChannelDesc_t)(void*, void*, const void*);
SeqLoadChannelDesc_t SeqLoadChannelDesc_orig;
int __fastcall SeqLoadChannelDesc_re(void* self, void*, const void* desc) {
    if (!desc) return 0xa;                                  // jne at 0x10024286
    fld<int>(self, 0) = fldc<int>(desc, 0);
    fld<int>(self, 8) = fldc<int>(desc, 8);
    fld<int>(self, 0xc) = fldc<int>(desc, 0xc);
    fld<int>(self, 0x10) = fldc<int>(desc, 0x10);
    const int v14 = fldc<int>(desc, 0x14);
    const int current = fld<int>(self, 4);
    fld<int>(self, 0x14) = v14;
    const int srcFlags = fldc<int>(desc, 4);
    const int bit0 = static_cast<int>(static_cast<unsigned>(srcFlags) << 0x1f) >> 0x1f;
    const int withBit0 = ((bit0 ^ current) & 1) ^ current;
    fld<int>(self, 4) = withBit0;
    const int bit1 = static_cast<int>(static_cast<unsigned>(fldc<int>(desc, 4)) << 0x1e) >> 0x1e;
    const int withBit1 = ((bit1 ^ withBit0) & 2) ^ withBit0;
    fld<int>(self, 4) = withBit1;
    SoundSetName_t setName = reinterpret_cast<SoundSetName_t>(c3az_sound(0x0001ced0));
    setName(self, 0, fldc<const char*>(desc, 0x1c));
    return 0xb;
}

// 0x00024360  Seq_mergeChannelDesc (VA 0x10024360)
// __thiscall with one stack argument (`ret 4` at 0x100243e4); eax is not a return value (on the merging path
// it is whatever Sound_setName left, on the guarded paths it is 0).
// A null source leaves the destination untouched (`test eax, eax` / `je 0x100243e4` at 0x10024364). Otherwise
// the dword at +0 is always copied (0x10024368, 0x1002436a) and four bits of the source's byte at +4 drive the
// rest:
//   bit 0 (`test dl, 1` / `je 0x10024389` at 0x1002436f) sets bit 0 of the destination's dword at +4 and
//   copies the dwords at +8 and +0xc (0x10024374-0x10024386);
//   bit 1 (`test byte [eax+4], dl` with dl = 2 / `je 0x100243a8` at 0x1002438f) sets bit 1 of +4 and copies
//   the dwords at +0x10 and +0x14 (0x10024394-0x100243a5);
//   bit 3 (`test byte [eax+4], dl` with dl = 8 / `je 0x100243b7` at 0x100243ad) is copied into bit 3 of +4,
//   set at 0x100243b2 and cleared at 0x100243b7;
//   bit 4 is copied into bit 4 of +4 through its complement (`not edx` / `shr edx, 4` / `test dl, 1` /
//   `jne 0x100243d1` at 0x100243bf-0x100243ca): a complement bit of 1, that is a source bit of 0, clears it
//   (0x100243d1) and a source bit of 1 sets it (0x100243cc); the result is stored at 0x100243d4.
// Finally a non-null name pointer at the source's +0x1c (0x100243d7, `je 0x100243e4` at 0x100243dc) is passed
// to Sound_setName (VA 0x1001ced0, through the incremental-link thunk at 0x100243df), which frees the
// destination's old name at +0x1c and allocates a copy. The reimplementation calls that callee at its own
// address.
typedef void(__fastcall* SeqMergeChannelDesc_t)(void*, void*, const void*);
SeqMergeChannelDesc_t SeqMergeChannelDesc_orig;
void __fastcall SeqMergeChannelDesc_re(void* self, void*, const void* desc) {
    if (!desc) return;                                      // je at 0x10024366
    fld<int>(self, 0) = fldc<int>(desc, 0);
    if (fldc<unsigned char>(desc, 4) & 1) {                 // je at 0x10024372
        fld<int>(self, 4) |= 1;
        fld<int>(self, 8) = fldc<int>(desc, 8);
        fld<int>(self, 0xc) = fldc<int>(desc, 0xc);
    }
    if (fldc<unsigned char>(desc, 4) & 2) {                 // je at 0x10024392
        fld<int>(self, 4) |= 2;
        fld<int>(self, 0x10) = fldc<int>(desc, 0x10);
        fld<int>(self, 0x14) = fldc<int>(desc, 0x14);
    }
    if (fldc<unsigned char>(desc, 4) & 8) fld<int>(self, 4) |= 8;        // je at 0x100243b0
    else fld<int>(self, 4) &= static_cast<int>(0xfffffff7u);
    const unsigned complement = ~static_cast<unsigned>(fldc<int>(desc, 4));
    if (((complement >> 4) & 1) != 0) fld<int>(self, 4) &= static_cast<int>(0xffffffefu);   // jne at 0x100243ca
    else fld<int>(self, 4) |= 0x10;
    const char* name = fldc<const char*>(desc, 0x1c);
    if (name) {                                             // je at 0x100243dc
        SoundSetName_t setName = reinterpret_cast<SoundSetName_t>(c3az_sound(0x0001ced0));
        setName(self, 0, name);
    }
}

SG_HOOK("sound.dll", 0x0003e480, sound_medianOfThree, MedianOfThree_re, MedianOfThree_orig);
SG_HOOK("sound.dll", 0x0003e2c0, sound_intPow, IntPow_re, IntPow_orig);
SG_HOOK("sound.dll", 0x00040da0, sound_codecFirstOrderFilter, CodecFirstOrder_re, CodecFirstOrder_orig);
SG_HOOK("sound.dll", 0x0003ed00, sound_codecRemoveMean, CodecRemoveMean_re, CodecRemoveMean_orig);
SG_HOOK("sound.dll", 0x0003ec60, sound_codecRmsEnergy, CodecRmsEnergy_re, CodecRmsEnergy_orig);
SG_HOOK("sound.dll", 0x0003e400, sound_floatFloorToInt, FloatFloorToInt_re, FloatFloorToInt_orig);
SG_HOOK("sound.dll", 0x00040f60, sound_codecCombFilter, CodecCombFilter_re, CodecCombFilter_orig);
SG_HOOK("sound.dll", 0x00034ac0, sound_Channel_releaseVoices, ChannelReleaseVoices_re, ChannelReleaseVoices_orig);
SG_HOOK("sound.dll", 0x00034b90, sound_Channel_stopVoices, ChannelStopVoices_re, ChannelStopVoices_orig);
SG_HOOK("sound.dll", 0x00035340, sound_Channel_setPanGains, ChannelSetPanGains_re, ChannelSetPanGains_orig);
SG_HOOK("sound.dll", 0x00033ba0, sound_Channel_resetPanState, ChannelResetPanState_re, ChannelResetPanState_orig);
SG_HOOK("sound.dll", 0x00010f30, sound_DsBuffer_configureDesc, DsBufferConfigureDesc_re, DsBufferConfigureDesc_orig);
SG_HOOK("sound.dll", 0x00023e10, sound_Seq_applyTrackFlag, SeqApplyTrackFlag_re, SeqApplyTrackFlag_orig);
SG_HOOK("sound.dll", 0x00024280, sound_Seq_loadChannelDesc, SeqLoadChannelDesc_re, SeqLoadChannelDesc_orig);
SG_HOOK("sound.dll", 0x00024360, sound_Seq_mergeChannelDesc, SeqMergeChannelDesc_re, SeqMergeChannelDesc_orig);

}  // namespace
