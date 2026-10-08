// Batch c3as: Terrain.dll. The addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; every
// instruction address cited in a comment is a VA (VA = 0x10000000 + RVA), the form the debug build's
// disassembly prints. Terrain.dll is a static import of golf_clean.exe, so it is mapped before the shim
// installs its hooks. It is a /Od /ZI /GZ build: every body is literal, so each reimplementation below follows
// the original's order of loads, compares, calls and stores, and each fact carries the VA it comes from.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Three groups:
//   1. the four integer classifiers called from Tile::blendFaceTextures 0x10013670
//      (Terrain::idClass, Terrain::flagCode, Tile::isOpenType, Tile::edgeKind);
//   2. Terrain's x87 math (degToRad, powf2, normalize, rotateAxis, Terrain::bernstein,
//      Terrain::hermitePoint, Terrain::faceNormal);
//   3. five allocation-free std::list<Tile*> helpers.
//
// HOW THE x87 ROUNDING IS REPRODUCED (method established by batch c3ap, log/c3/c3ap_purpose.md)
// --------------------------------------------------------------------------------------------
// The originals are x87 code; this file is compiled by MSVC 2022 x86, which emits SSE2 scalar arithmetic.
// The x87 control word on the thread the A/B calls from is 0x027f (PC = 53-bit, RC = round to nearest), so
// every x87 operation rounds exactly like an SSE2 `double` operation. /Od spills every finished expression
// with `fstp dword` (float) or `fstp qword` (double), so the rounding points are visible: each `fstp dword`
// is a `float` destination here, and inside one expression every operand is widened to `double` so that C++
// never rounds between operations. Float constants and the CRT routines (pow, sqrt, sin, cos) are read from
// and called in Terrain.dll itself, at the addresses the originals use.
//
// A function whose source returns `float` leaves the UNROUNDED 53-bit value in st(0) at /Od (degToRad 0x10001880,
// powf2 0x10005840, Terrain::bernstein 0x10005750 all do), and their callers consume exactly that value
// (rotateAxis passes degToRad's st(0) straight to cos). The reimplementations therefore return `double`, which
// MSVC also leaves in st(0): that keeps the reimplementation interchangeable with the original for its callers,
// and the registry compares the full st(0) with ret="double".
#include "hooks.h"

namespace {

// Terrain.dll has base relocations, so its data and callees are reached through the mapped module.
unsigned char* TerrainBase() {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base;
}
void* TerrainRva(unsigned long rva) { return TerrainBase() + rva; }

// A float constant in Terrain.dll's read-only data, by VA (the operand of the fld/fmul/fsub below).
float TKonst(unsigned va) { return *(const float*)(TerrainBase() + (va - 0x10000000u)); }

// The CRT routines Terrain.dll's own code calls, by VA. All __cdecl, double arguments on the stack.
typedef double(__cdecl* Unary_t)(double);
typedef double(__cdecl* Binary_t)(double, double);
double CrtPow(double x, double y) { return ((Binary_t)TerrainRva(0x00019389))(x, y); }
double CrtSqrt(double x) { return ((Unary_t)TerrainRva(0x000192c4))(x); }
double CrtCos(double x) { return ((Unary_t)TerrainRva(0x00019214))(x); }
double CrtSin(double x) { return ((Unary_t)TerrainRva(0x00019164))(x); }

// ===================================================================================================
// 1. The integer classifiers called from Tile::blendFaceTextures 0x10013670
// ===================================================================================================

// 0x00013dd0  Terrain::idClass (VA 0x10013dd0)
// __thiscall with one stack DWORD argument (`ret 4` at 0x10013e4f); `this` is stored in [ebp-4] at 0x10013dea
// and never loaded again, so the result depends on the argument alone.
// The argument is copied to [ebp-8] (0x10013df0) and bounded UNSIGNED against 0x46 (cmp at 0x10013df3,
// ja 0x10013df7 to the default at 0x10013e47, which returns 0), so every negative value and everything above
// 70 returns 0. In range, the byte table of 71 entries at 0x10013e7a selects one of ten targets through the
// jump table at 0x10013e52 (movzx-style `xor ecx,ecx; mov cl, [edx+0x10013e7a]` at 0x10013dfe, indirect jump
// at 0x10013e04). The ten targets and the value each loads into eax:
//   [0] 0x10013e0b -> 0   [1] 0x10013e0f -> 2   [2] 0x10013e16 -> 4   [3] 0x10013e1d -> 3   [4] 0x10013e24 -> 1
//   [5] 0x10013e40 -> 6   [6] 0x10013e2b -> 8   [7] 0x10013e32 -> 7   [8] 0x10013e39 -> 5   [9] 0x10013e47 -> 0
// Entry 9 is the default block, so the ids whose table byte is 9 and the ids above 0x46 return 0 through two
// different blocks that both do `xor eax, eax`. Grouping the 71 table bytes by target gives the case lists below.
typedef int(__fastcall* IdClass_t)(void*, void*, int);
IdClass_t IdClass_orig;
int __fastcall IdClass_re(void*, void*, int id) {
    if ((unsigned int)id > 0x46)
        return 0;                                  // ja 0x10013df7 -> 0x10013e47
    switch (id) {
        case 0: case 1: case 2: case 10: case 12: case 20: case 21:
            return 0;                              // 0x10013e0b
        case 3: case 43:
            return 2;                              // 0x10013e0f
        case 4: case 14: case 24: case 34:
            return 4;                              // 0x10013e16
        case 5: case 7: case 25:
            return 3;                              // 0x10013e1d
        case 6: case 16:
            return 1;                              // 0x10013e24
        case 30:
            return 6;                              // 0x10013e40
        case 40: case 41: case 42: case 61:
            return 8;                              // 0x10013e2b
        case 50: case 52: case 70:
            return 7;                              // 0x10013e32
        case 60:
            return 5;                              // 0x10013e39
        default:
            return 0;                              // table byte 9 -> 0x10013e47
    }
}

// 0x00013f00  Terrain::flagCode (VA 0x10013f00)
// __thiscall with three stack DWORD arguments (`ret 0xc` at 0x10013f8d); `this` is stored at 0x10013f1a and
// never read. The accumulator [ebp-8] starts at 0 (0x10013f1d) and each argument adds one of two constants:
//   argument 1 ([ebp+8]):    == 1 -> +4    (cmp 0x10013f24, jne 0x10013f28, add 0x10013f2d)
//                            == 2 -> +0x28 (cmp 0x10013f35, jne 0x10013f39, add 0x10013f3e)
//   argument 2 ([ebp+0xc]):  == 1 -> +1    (cmp 0x10013f44, jne 0x10013f48, add 0x10013f4d)
//                            == 2 -> +0xa  (cmp 0x10013f55, jne 0x10013f59, add 0x10013f5e)
//   argument 3 ([ebp+0x10]): == 1 -> +2    (cmp 0x10013f64, jne 0x10013f68, add 0x10013f6d)
//                            == 2 -> +0x14 (cmp 0x10013f75, jne 0x10013f79, add 0x10013f7e)
// The == 1 arm jumps over the == 2 test (0x10013f33, 0x10013f53, 0x10013f73), so each argument contributes at
// most one constant, and any value other than 1 and 2 contributes nothing. The accumulator is returned in eax
// (0x10013f84). Every compare is a full DWORD compare, so 0x101 does not count as 1.
typedef int(__fastcall* FlagCode_t)(void*, void*, int, int, int);
FlagCode_t FlagCode_orig;
int __fastcall FlagCode_re(void*, void*, int a, int b, int c) {
    int code = 0;
    if (a == 1)
        code += 4;
    else if (a == 2)
        code += 0x28;
    if (b == 1)
        code += 1;
    else if (b == 2)
        code += 0xa;
    if (c == 1)
        code += 2;
    else if (c == 2)
        code += 0x14;
    return code;
}

// 0x000155b0  Tile::isOpenType (VA 0x100155b0)
// __thiscall with no stack argument (`ret` at 0x10015625). It reloads `this` from [ebp-4] before each of seven
// DWORD compares of the type field at +0x24 against 2, 7, 1, 0, 9, 8 and 3 in that order (cmp at 0x100155d0,
// 0x100155d9, 0x100155e2, 0x100155eb, 0x100155f4, 0x100155fd, 0x10015606; each `je` goes to 0x10015615, which
// stores 1 into [ebp-8]). Falling past all seven stores 0 (0x1001560c). The result is returned in al only
// (mov al, byte [ebp-8] at 0x1001561c); the upper three bytes of eax still hold `this` from the last reload at
// 0x10015603, so only the low byte is the result.
typedef unsigned char(__fastcall* TileIsOpenType_t)(void*, void*);
TileIsOpenType_t TileIsOpenType_orig;
unsigned char __fastcall TileIsOpenType_re(void* self, void*) {
    const int type = *(const int*)((const unsigned char*)self + 0x24);
    if (type == 2 || type == 7 || type == 1 || type == 0 || type == 9 || type == 8 || type == 3)
        return 1;
    return 0;
}

// 0x00015650  Tile::edgeKind (VA 0x10015650)
// __thiscall with one stack argument, a second Tile (`ret 4` at 0x1001573f). Returns 0, 1 or 2 in a full eax.
// A NULL argument returns 0 immediately (cmp 0x1001566d, jne 0x10015671, xor eax,eax at 0x10015673).
// Two completely separate paths, selected by this tile's own type at +0x24 against 0x11 (cmp 0x1001567d,
// jne 0x10015681 to the second path at 0x100156ed):
//   type == 0x11:
//     * when this tile's +0x28 is 0 (cmp 0x10015686, jne 0x1001568a) it calls the other tile's Tile::getType
//       (call 0x1001568f of the thunk 0x1000119a, which is jmp 0x10001f60) and returns 1 when the two types
//       differ (cmp 0x10015697, je 0x1001569a, mov eax,1 at 0x1001569c). A +0x28 that is not 0 skips this test.
//     * then it calls the other tile's Tile::getVariation (call 0x100156a9 of the thunk 0x100010e6, which is
//       jmp 0x10015340) and SIGN-EXTENDS its byte (movsx eax, al at 0x100156ae), so the comparisons below are
//       against a value in -128..127. This tile's +0x28 equal to it returns 0 (cmp 0x100156b4, je 0x100156b7,
//       xor eax,eax at 0x100156e9).
//     * otherwise +0x28 == 0 returns 2 (cmp 0x100156bc, je 0x100156c0 -> 0x100156db), and +0x28 == 1
//       (cmp 0x100156c5, jne 0x100156c9) returns 2 only when a SECOND call of the other tile's getVariation
//       (0x100156ce) sign-extends to 2 (cmp 0x100156d6, jne 0x100156d9); anything else returns 1 (0x100156e2).
//   type != 0x11: both types are mapped through the module global at 0x10106b48, a pointer to a DWORD table
//     indexed by type id with no bounds check (loads at 0x100156fb and 0x10015701, indexed reads at 0x10015707
//     and 0x1001570a). Different table entries return 1 (je 0x1001570d, mov eax,1 at 0x1001570f). Equal entries
//     call the other tile's getType once more (0x10015719) and return 2 when the two types themselves differ
//     (cmp 0x10015721, je 0x10015724, mov eax,2 at 0x10015726), else 0 (0x1001572d).
// getType is read twice and getVariation twice on the paths that need them; the calls are left as calls here.
typedef int(__fastcall* TileGetTypeCall_t)(void*, void*);
typedef signed char(__fastcall* TileGetVariationCall_t)(void*, void*);
typedef int(__fastcall* TileEdgeKind_t)(void*, void*, void*);
TileEdgeKind_t TileEdgeKind_orig;
int __fastcall TileEdgeKind_re(void* self, void*, void* other) {
    if (other == 0)
        return 0;
    unsigned char* t = (unsigned char*)self;
    const TileGetTypeCall_t getType = (TileGetTypeCall_t)TerrainRva(0x00001f60);
    const TileGetVariationCall_t getVariation = (TileGetVariationCall_t)TerrainRva(0x00015340);
    const int type = *(const int*)(t + 0x24);
    if (type == 0x11) {
        const int rot = *(const int*)(t + 0x28);
        if (rot == 0 && type != getType(other, 0))
            return 1;
        const int var = getVariation(other, 0);          // sign-extended byte
        if (rot == var)
            return 0;
        if (rot == 0)
            return 2;
        if (rot == 1 && getVariation(other, 0) == 2)
            return 2;
        return 1;
    }
    const int* collar = *(const int**)(TerrainBase() + 0x00106b48);
    const int otherType = getType(other, 0);
    if (collar[type] != collar[otherType])
        return 1;
    if (getType(other, 0) != type)
        return 2;
    return 0;
}

// ===================================================================================================
// 2. Terrain's x87 math
// ===================================================================================================

// 0x00001880  degToRad (VA 0x10001880)
// __cdecl, one float argument (`ret` with no immediate at 0x100018a7). The whole body is `fld dword [ebp+8]`
// (0x10001898) and `fmul dword [0x100613e0]` (0x1000189b), the constant 0.017453292f = pi/180; nothing is
// stored, so the 53-bit product is left in st(0) for the caller. Its only caller rotateAxis 0x10003a50 uses
// that unrounded value (see below), so the reimplementation returns `double`, which MSVC also leaves in st(0).
// The product of two floats is exact at 53 bits, so no rounding happens here at all.
typedef double(__cdecl* DegToRad_t)(float);
DegToRad_t DegToRad_orig;
double __cdecl DegToRad_re(float deg) {
    return (double)deg * (double)TKonst(0x100613e0);
}

// 0x00005840  powf2 (VA 0x10005840)
// __cdecl, two float arguments (`ret` at 0x10005885). It converts both to double on the stack and calls the
// CRT pow at 0x10019389: the SECOND argument [ebp+0xc] is pushed first (0x10005858-0x1000585e) and the FIRST
// argument [ebp+8] after it (0x10005861-0x10005867), so the lower address holds [ebp+8] and the call is
// pow([ebp+8], [ebp+0xc]) - base first, exponent second. The result is kept in st(0) by `fst dword [ebp-4]`
// (0x10005872), a store into a frame slot that is never read again, so the value the caller sees is pow's
// unrounded double. Terrain::bernstein 0x10005750 multiplies exactly that value, so the reimplementation
// returns `double` as well.
typedef double(__cdecl* Powf2_t)(float, float);
Powf2_t Powf2_orig;
double __cdecl Powf2_re(float base, float exponent) {
    return CrtPow((double)base, (double)exponent);
}

// 0x00037c80  normalize (VA 0x10037c80)
// __cdecl, one argument: a pointer to three floats (`ret` at 0x10037d0a), no return value.
// The squared length is built left to right with no spill: v0*v0 (0x10037c9e, 0x10037ca0), + v1*v1
// (0x10037ca8, 0x10037cab, faddp 0x10037cae), + v2*v2 (0x10037cb6, 0x10037cb9, faddp 0x10037cbc). Each product
// of two floats is exact at 53 bits; the two additions round to 53 bits. The sum is passed as a double
// (fstp qword [esp] at 0x10037cc1) to the CRT sqrt at 0x100192c4 and its result is rounded to a FLOAT in the
// frame slot [ebp-4] (fstp dword at 0x10037ccc). Each component is then divided by that float and stored back
// as a float, in place and in order: v0 (0x10037cd2, 0x10037cd4, 0x10037cda), v1 (0x10037cdf, 0x10037ce2,
// 0x10037ce8), v2 (0x10037cee, 0x10037cf1, 0x10037cf7). Each division is a 53-bit operation whose result is
// then rounded to float, which is why it is written with explicit doubles here. The pointer is dereferenced
// again before every use; no length test, so a zero-length vector divides by zero.
typedef void(__cdecl* Normalize_t)(float*);
Normalize_t Normalize_orig;
void __cdecl Normalize_re(float* v) {
    double sum = (double)v[0] * v[0];
    sum = sum + (double)v[1] * v[1];
    sum = sum + (double)v[2] * v[2];
    const float len = (float)CrtSqrt(sum);
    v[0] = (float)((double)v[0] / len);
    v[1] = (float)((double)v[1] / len);
    v[2] = (float)((double)v[2] / len);
}

// 0x00003a50  rotateAxis (VA 0x10003a50)
// __cdecl, five stack arguments (`ret` at 0x10003c66, no return value): an angle in DEGREES [ebp+8], the three
// floats of an axis [ebp+0xc], [ebp+0x10], [ebp+0x14], and a pointer to three floats [ebp+0x18] it rotates in
// place.
// The angle is converted by degToRad (call 0x10003a6c of the thunk 0x10001190, which is jmp 0x10001880) and the
// result then goes two ways, and they are NOT the same value:
//   * `fst dword [ebp-4]` at 0x10003a74 keeps st(0) and writes a float COPY;
//   * `fstp qword [esp]` at 0x10003a7a passes the unrounded 53-bit st(0) as a double to the CRT cos at
//     0x10019214, whose result is rounded to a float at [ebp-8] (0x10003a85);
//   * the float copy is reloaded at 0x10003a88 and passed as a double to the CRT sin at 0x10019164, whose
//     result is rounded to a float at [ebp-0xc] (0x10003a99).
// A 3x3 matrix of nine float slots is then initialised to the identity (0x10003a9c-0x10003ad4: 0x3f800000 =
// 1.0f into [ebp-0x10], [ebp-0x20], [ebp-0x30], 0 into the other six).
// Which slots are overwritten is decided by three groups of three x87 compares against the constant 0.0f at
// 0x1005f1e0 (`fcomp` + `fnstsw ax` + `test ah, 0x40`, where the bit is set when the two are EQUAL):
//   * axis x != 0 and y == 0 and z == 0 (0x10003ae9, 0x10003af9, 0x10003b09): rotation about X. Sets
//     [ebp-0x20] = cos (0x10003b0e), [ebp-0x24] = -sin (fchs 0x10003b14), [ebp-0x2c] = sin (0x10003b1c),
//     [ebp-0x30] = cos (0x10003b22), then writes v[1] = cos*v[1] + (-sin)*v[2] (0x10003b28-0x10003b3c) and
//     v[2] = sin*v[1] + cos*v[2] (0x10003b42-0x10003b56).
//   * x == 0 and y != 0 and z == 0 (0x10003b6c, 0x10003b7c, 0x10003b8c): rotation about Y. Sets [ebp-0x10] =
//     cos, [ebp-0x18] = sin, [ebp-0x28] = -sin, [ebp-0x30] = cos (0x10003b91-0x10003ba5), then writes
//     v[0] = cos*v[0] + sin*v[2] (0x10003bab-0x10003bbe) and v[2] = (-sin)*v[0] + cos*v[2]
//     (0x10003bc3-0x10003bd6).
//   * x == 0 and y == 0 and z != 0 (0x10003be9, 0x10003bf9, 0x10003c09): rotation about Z. Sets [ebp-0x10] =
//     cos, [ebp-0x14] = -sin, [ebp-0x1c] = sin, [ebp-0x20] = cos (0x10003c0e-0x10003c22), then writes
//     v[0] = cos*v[0] + (-sin)*v[1] (0x10003c28-0x10003c3b) and v[1] = sin*v[0] + cos*v[1]
//     (0x10003c40-0x10003c53).
//   * any other axis (two or three non-zero components, or all three zero) writes nothing at all: all three
//     groups fall through to the epilogue at 0x10003c56.
//     `test ah, 0x40` reads the x87 C3 bit, which an UNORDERED compare also sets, so the original treats a NaN
//     component as equal to 0.0f while the C++ `==` below treats it as not equal; the registered vectors
//     contain no NaN.
// In each of the three branches the SECOND component is computed from the component the branch has ALREADY
// stored: the X branch reloads v[1] at 0x10003b45 after storing it at 0x10003b3c, the Y branch reloads v[0] at
// 0x10003bc6 after storing it at 0x10003bbe, and the Z branch reloads v[0] at 0x10003c43 after storing it at
// 0x10003c3b. The reimplementation keeps that order, so the second component uses the rotated first one.
// Every product is a float times a float (exact at 53 bits) and each sum is one 53-bit addition rounded to a
// float by the store, so the sums are written with explicit doubles.
typedef void(__cdecl* RotateAxis_t)(float, float, float, float, float*);
RotateAxis_t RotateAxis_orig;
void __cdecl RotateAxis_re(float angle, float ax, float ay, float az, float* v) {
    const double rad = ((DegToRad_t)TerrainRva(0x00001880))(angle);
    const float radCopy = (float)rad;                      // fst dword [ebp-4]
    const float c = (float)CrtCos(rad);                    // the unrounded angle
    const float s = (float)CrtSin((double)radCopy);        // the float copy, reloaded
    const float zero = TKonst(0x1005f1e0);                 // 0.0f
    if (ax != zero && ay == zero && az == zero) {
        const float m11 = c, m12 = -s, m21 = s, m22 = c;
        v[1] = (float)((double)m11 * v[1] + (double)m12 * v[2]);
        v[2] = (float)((double)m21 * v[1] + (double)m22 * v[2]);
    } else if (ax == zero && ay != zero && az == zero) {
        const float m00 = c, m02 = s, m20 = -s, m22 = c;
        v[0] = (float)((double)m00 * v[0] + (double)m02 * v[2]);
        v[2] = (float)((double)m20 * v[0] + (double)m22 * v[2]);
    } else if (ax == zero && ay == zero && az != zero) {
        const float m00 = c, m01 = -s, m10 = s, m11 = c;
        v[0] = (float)((double)m00 * v[0] + (double)m01 * v[1]);
        v[1] = (float)((double)m10 * v[0] + (double)m11 * v[1]);
    }
}

// 0x00005750  Terrain::bernstein (VA 0x10005750)
// __thiscall with two stack arguments (`ret 8` at 0x10005801), a float t [ebp+8] and an int i [ebp+0xc];
// `this` is stored at 0x1000576a and never read. It evaluates one term of a degree-2 Bernstein polynomial.
// The binomial coefficient: factorial(2) (push 2 at 0x1000576d, call 0x1000576f of the thunk 0x10001208, which
// is jmp 0x100058a0) is converted to a float in [ebp-0x10] (fild 0x1000577a, fstp dword 0x1000577d);
// factorial(i) (0x10005784) and factorial(2 - i) (the difference computed at 0x10005793, call 0x10005797) are
// multiplied as SIGNED integers (imul 0x1000579f) into [ebp-0x14], converted with `fild` (0x100057a5) and
// divided INTO the numerator (fdivr dword [ebp-0x10] at 0x100057a8, memory divided by st(0)), and the quotient
// is rounded to a float in [ebp-8] (0x100057ab).
// First power: i is converted to a float on the stack (fild 0x100057ae, fstp dword [esp] 0x100057b2) as the
// SECOND argument and t [ebp+8] is pushed after it as the first (0x100057b8), so the call at 0x100057b9 (the
// thunk 0x10001325, jmp 0x10005840) is powf2(t, (float)i). Its unrounded st(0) is multiplied by the coefficient
// (fmul dword [ebp-8] at 0x100057c1) and the product is rounded to a float in [ebp-0x18] (0x100057c4).
// Second power: 2 - i is converted to a float as the second argument (0x100057cf-0x100057d6) and
// 1.0f - t is computed at 53 bits from the constant at 0x1005f024 (fld 0x100057d9, fsub dword [ebp+8]
// 0x100057df) and rounded to a float as the first argument (0x100057e3), so the call at 0x100057e6 is
// powf2(1.0f - t, (float)(2 - i)). Its unrounded st(0) is multiplied by [ebp-0x18] (0x100057ee) and left in
// st(0) as the result, unrounded; the reimplementation returns `double` for the same reason powf2 does.
// Both factorials and both powers go through the original addresses.
typedef int(__cdecl* Factorial_t)(int);
typedef double(__fastcall* Bernstein_t)(void*, void*, float, int);
Bernstein_t Bernstein_orig;
double __fastcall Bernstein_re(void*, void*, float t, int i) {
    const Factorial_t factorial = (Factorial_t)TerrainRva(0x000058a0);
    const Powf2_t powf2 = (Powf2_t)TerrainRva(0x00005840);
    const float numerator = (float)factorial(2);
    const int denominator = factorial(i) * factorial(2 - i);
    const float coefficient = (float)((double)numerator / (double)denominator);
    const float term = (float)(powf2(t, (float)i) * (double)coefficient);
    const float oneMinusT = (float)((double)TKonst(0x1005f024) - t);
    return powf2(oneMinusT, (float)(2 - i)) * (double)term;
}

// 0x00005090  Terrain::hermitePoint (VA 0x10005090)
// __thiscall with six stack arguments (`ret 0x18` at 0x100051cf, no return value); `this` is stored at
// 0x100050aa and never read. [ebp+8] is a two-float output point, [ebp+0xc] the parameter t, and [ebp+0x10],
// [ebp+0x14], [ebp+0x18], [ebp+0x1c] four two-float control points.
// Two powers of t are taken from the CRT pow at 0x10019389 with the exponent pushed as a literal double
// (0x40080000:00000000 = 3.0 at 0x100050ad, 0x40000000:00000000 = 2.0 at 0x100050c8) and each result is
// rounded to a float: t3 in [ebp-8] (0x100050c5) and t2 in [ebp-0xc] (0x100050e0).
// The four constants used below are 2.0f at 0x1005f028, 3.0f at 0x100613b4, 1.0f at 0x1005f024 and -2.0f at
// 0x1005f21c. Each output component is one chain of 53-bit operations ending in a single `fstp dword`:
//   out[k] = (((2.0f*t3 - 3.0f*t2) + 1.0f) * p0[k])      (0x100050ec-0x10005109 / 0x10005158-0x10005175)
//          + ((-2.0f*t3 + 3.0f*t2) * p1[k])              (0x1000510b-0x10005124 / 0x10005178-0x10005192)
//          + (((t3 - 2.0f*t2) + t) * p2[k])              (0x10005126-0x1000513a / 0x10005194-0x100051a9)
//          + ((t3 - t2) * p3[k])                         (0x1000513c-0x10005147 / 0x100051ab-0x100051b7)
// with `fsubr dword [ebp-8]` at 0x1000512f meaning t3 minus the preceding product, and each `faddp st(1)`
// adding the new term to the running sum on the left. The output slot is set to the integer 0 BEFORE its chain
// is evaluated (0x100050e6 for out[0], 0x10005151 for out[1]) and overwritten by the `fstp dword` at the end
// (0x1000514c, 0x100051bc). Component 0 is finished before component 1 starts, and both reuse the same t3, t2.
typedef void(__fastcall* HermitePoint_t)(void*, void*, float*, float, const float*, const float*,
                                         const float*, const float*);
HermitePoint_t HermitePoint_orig;
void __fastcall HermitePoint_re(void*, void*, float* out, float t, const float* p0, const float* p1,
                                const float* p2, const float* p3) {
    const float t3 = (float)CrtPow((double)t, 3.0);
    const float t2 = (float)CrtPow((double)t, 2.0);
    const double two = TKonst(0x1005f028);        // 2.0f
    const double three = TKonst(0x100613b4);      // 3.0f
    const double one = TKonst(0x1005f024);        // 1.0f
    const double minusTwo = TKonst(0x1005f21c);   // -2.0f
    for (int k = 0; k < 2; k++) {
        out[k] = 0;
        double sum = ((two * t3 - three * t2) + one) * p0[k];
        sum = sum + (minusTwo * t3 + three * t2) * p1[k];
        sum = sum + (((double)t3 - two * t2) + t) * p2[k];
        sum = sum + ((double)t3 - t2) * p3[k];
        out[k] = (float)sum;
    }
}

// 0x00011d60  Terrain::faceNormal (VA 0x10011d60)
// __thiscall with one stack argument, a face record (`ret 4` at 0x10011e9b, no return value); `this` is stored
// at 0x10011d7a and never read. The face's three DWORDs at +0, +4 and +8 are vertex indices into the module
// array at 0x100b28c8, whose stride is 12 bytes (`imul ..., 0xc` at 0x10011d83, 0x10011d8b and at every later
// index) with no bounds check: the address is index*12 + 0x100b28c8 computed in 32-bit arithmetic.
// Two edge vectors are built as floats, each component one 53-bit subtraction rounded by its `fstp dword`:
//   e1 = vertex[face[1]] - vertex[face[0]]   x [ebp-0x10] (0x10011d8e-0x10011d9a),
//                                            y [ebp-0xc]  (0x10011dae-0x10011dba),
//                                            z [ebp-8]    (0x10011dce-0x10011dda)
//   e2 = vertex[face[2]] - vertex[face[1]]   x [ebp-0x1c] (0x10011def-0x10011dfb),
//                                            y [ebp-0x18] (0x10011e10-0x10011e1c),
//                                            z [ebp-0x14] (0x10011e31-0x10011e3d)
// Their cross product e1 x e2 is written as three floats into the face at +0x2c, +0x30 and +0x34, each one a
// pair of exact float products combined by one 53-bit `fsubp` (0x10011e40-0x10011e51, 0x10011e54-0x10011e65,
// 0x10011e68-0x10011e79). Finally normalize 0x10037c80 is called on the face's +0x2c (lea at 0x10011e7f,
// call 0x10011e83 of the thunk 0x100011a9, __cdecl so the caller clears the argument at 0x10011e88).
typedef void(__fastcall* FaceNormal_t)(void*, void*, int*);
FaceNormal_t FaceNormal_orig;
void __fastcall FaceNormal_re(void*, void*, int* face) {
    unsigned char* const vertices = TerrainBase() + 0x000b28c8;
    const float* const a = (const float*)(vertices + (unsigned int)face[0] * 12u);
    const float* const b = (const float*)(vertices + (unsigned int)face[1] * 12u);
    const float* const c = (const float*)(vertices + (unsigned int)face[2] * 12u);
    const float e1x = (float)((double)b[0] - a[0]);
    const float e1y = (float)((double)b[1] - a[1]);
    const float e1z = (float)((double)b[2] - a[2]);
    const float e2x = (float)((double)c[0] - b[0]);
    const float e2y = (float)((double)c[1] - b[1]);
    const float e2z = (float)((double)c[2] - b[2]);
    float* const normal = (float*)((unsigned char*)face + 0x2c);
    normal[0] = (float)((double)e1y * e2z - (double)e1z * e2y);
    normal[1] = (float)((double)e1z * e2x - (double)e1x * e2z);
    normal[2] = (float)((double)e1x * e2y - (double)e1y * e2x);
    ((Normalize_t)TerrainRva(0x00037c80))(normal);
}

// ===================================================================================================
// 3. std::list<Tile*> helpers (no heap: every one of these only reads)
// ===================================================================================================

// 0x0000b600  std::list<Tile*>::_Acc::_Next (VA 0x1000b600)
// __cdecl, one stack argument, no branch (`ret` at 0x1000b621): it returns the argument unchanged
// (mov eax, [ebp+8] at 0x1000b618), i.e. the address of the node's `next` field, which is at offset 0.
typedef void*(__cdecl* AccNext_t)(void*);
AccNext_t AccNext_orig;
void* __cdecl AccNext_re(void* node) {
    return node;
}

// 0x0000b630  std::list<Tile*>::_Acc::_Prev (VA 0x1000b630)
// Same shape one field further on: the argument plus 4 (mov eax at 0x1000b648, add eax, 4 at 0x1000b64b),
// the address of the node's `prev` field. `ret` at 0x1000b654, no branch.
typedef void*(__cdecl* AccPrev_t)(void*);
AccPrev_t AccPrev_orig;
void* __cdecl AccPrev_re(void* node) {
    return (unsigned char*)node + 4;
}

// 0x0000b660  std::list<Tile*>::_Acc::_Value (VA 0x1000b660)
// The argument plus 8 (mov eax at 0x1000b678, add eax, 8 at 0x1000b67b), the address of the node's stored
// Tile pointer. `ret` at 0x1000b684, no branch. These three accessors never dereference the node.
typedef void*(__cdecl* AccValue_t)(void*);
AccValue_t AccValue_orig;
void* __cdecl AccValue_re(void* node) {
    return (unsigned char*)node + 8;
}

// 0x0000b710  std::list<Tile*>::const_iterator::_Mynode (VA 0x1000b710)
// __thiscall with no stack argument (`ret` at 0x1000b738). The iterator holds one pointer: the body reloads
// `this` from [ebp-4] (0x1000b72d) and returns the DWORD it points at (mov eax, [eax] at 0x1000b730).
// No branch.
typedef void*(__fastcall* IterMynode_t)(void*, void*);
IterMynode_t IterMynode_orig;
void* __fastcall IterMynode_re(void* self, void*) {
    return *(void**)self;
}

// 0x0000b380  std::list<Tile*>::const_iterator::operator== (VA 0x1000b380)
// __thiscall with one stack argument, a second iterator (`ret 4` at 0x1000b3b2). It loads this iterator's node
// (mov edx, [eax] at 0x1000b3a3), clears eax (0x1000b3a5), compares the node with the other iterator's node
// (cmp edx, [ecx] at 0x1000b3a7) and sets the low byte from the comparison (sete al at 0x1000b3a9). Because of
// the `xor eax, eax` the whole of eax is 0 or 1. The comparison is a plain pointer comparison; neither node is
// dereferenced.
typedef int(__fastcall* IterEquals_t)(void*, void*, void*);
IterEquals_t IterEquals_orig;
int __fastcall IterEquals_re(void* self, void*, void* other) {
    return *(void**)self == *(void**)other ? 1 : 0;
}

}  // namespace

SG_HOOK("Terrain.dll", 0x00013dd0, Terrain_idClass, IdClass_re, IdClass_orig);
SG_HOOK("Terrain.dll", 0x00013f00, Terrain_flagCode, FlagCode_re, FlagCode_orig);
SG_HOOK("Terrain.dll", 0x000155b0, Tile_isOpenType, TileIsOpenType_re, TileIsOpenType_orig);
SG_HOOK("Terrain.dll", 0x00015650, Tile_edgeKind, TileEdgeKind_re, TileEdgeKind_orig);
SG_HOOK("Terrain.dll", 0x00001880, degToRad, DegToRad_re, DegToRad_orig);
SG_HOOK("Terrain.dll", 0x00005840, powf2, Powf2_re, Powf2_orig);
SG_HOOK("Terrain.dll", 0x00037c80, normalize, Normalize_re, Normalize_orig);
SG_HOOK("Terrain.dll", 0x00003a50, rotateAxis, RotateAxis_re, RotateAxis_orig);
SG_HOOK("Terrain.dll", 0x00005750, Terrain_bernstein, Bernstein_re, Bernstein_orig);
SG_HOOK("Terrain.dll", 0x00005090, Terrain_hermitePoint, HermitePoint_re, HermitePoint_orig);
SG_HOOK("Terrain.dll", 0x00011d60, Terrain_faceNormal, FaceNormal_re, FaceNormal_orig);
SG_HOOK("Terrain.dll", 0x0000b600, list_Acc_Next, AccNext_re, AccNext_orig);
SG_HOOK("Terrain.dll", 0x0000b630, list_Acc_Prev, AccPrev_re, AccPrev_orig);
SG_HOOK("Terrain.dll", 0x0000b660, list_Acc_Value, AccValue_re, AccValue_orig);
SG_HOOK("Terrain.dll", 0x0000b710, list_const_iterator_Mynode, IterMynode_re, IterMynode_orig);
SG_HOOK("Terrain.dll", 0x0000b380, list_const_iterator_equals, IterEquals_re, IterEquals_orig);
