// Batch c3ap: jgld.dll's x87 math library (Vector3, Quat, Matrix). SG_HOOK addresses are RVAs; every instruction
// cited in the comments is a VA (0x10000000 + RVA), as the debug build's disassembly prints them. jgld.dll is
// LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook (SgHooksInstallPending,
// re/hooks.cpp) and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// HOW THE x87 ROUNDING IS REPRODUCED (this is what the batch exists to establish)
// ------------------------------------------------------------------------------
// The originals are /Od /ZI /GZ x87 code; this file is compiled by MSVC 2022 x86, whose default /arch:SSE2 emits
// SSE scalar arithmetic. Two facts make the two arms bit-identical:
//
// 1. Precision control. The x87 control word measured inside the running game on the thread the A/B calls from
//    (the Frida RPC thread, both arms) is 0x027f: PC = 10b = 53-bit, RC = 00b = round to nearest. Every x87
//    operation in these functions therefore rounds its result to IEEE double, exactly like an SSE2 `double`
//    operation. The same 0x027f was read from inside a call to Vector3::dot (0x10003c90) made the way diff_hook
//    makes it. Measurement: `fnstcw` through a Frida-allocated stub (log/c3/c3ap_purpose.md).
// 2. Spill points. /Od stores every finished expression with `fstp dword` (float) before the next one starts, so
//    the rounding points are visible in the disassembly. Each such store is a cast to `float` here, and a value
//    reloaded with `fld dword` is read back from that `float`.
//
// So: inside one expression every operand is widened to `double` (never left to C++'s float-only evaluation, which
// would round after every operation), and each `fstp dword` is a `float` destination. For an expression that is a
// single operation the two models coincide, because double keeps 53 >= 2*24+2 bits; the multi-operation ones
// (Vector3::dot, Vector3::cross, Matrix::setRotation's `1 - (a + b)`) differ, and the registered vectors include
// values whose products and sums are not exact in float so that a wrong model reads RED.
//
// Float constants and the two CRT routines are read from / called in jgld.dll itself at the addresses the
// originals use, so no literal of ours has to round to the same float.
#include "hooks.h"

namespace {

// jgld.dll's math types (re/match/jgld_math.cpp, 100% matches): Vector3 {float v[3]}, Quat {Vector3 v; float w}
// (w at +0xc), Matrix {vptr; float m[16]} (m[0] at +4, column-major).
struct Vec3 {
    float v[3];
};
struct Quat {
    float v[3];
    float w;
};
struct Mat4 {
    void* vptr;
    float m[16];
};

const unsigned char* JgldBase() {
    static const unsigned char* base;
    if (!base) base = reinterpret_cast<const unsigned char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// A float constant in jgld.dll's read-only data, by VA (the operands of the fld/fsubr/fmul instructions below).
float Konst(unsigned va) { return *reinterpret_cast<const float*>(JgldBase() + (va - 0x10000000u)); }

// ----------------------------------------------------------------------------------- Vector3, no arithmetic

// 0x000036e0  Vector3::set(x, y, z): copies the three stack arguments ([ebp+8], [ebp+0xc], [ebp+0x10]) into
// [this], [this+4], [this+8] with integer moves (0x10003703, 0x1000370b, 0x10003714), so the stored bits are the
// argument's bits. No branch, nothing read from the object. __thiscall, `ret 0xc` at 0x1000371d.
typedef void(__fastcall* Vec3Set_t)(Vec3*, void*, float, float, float);
Vec3Set_t Vec3Set_orig;
void __fastcall Vec3Set_re(Vec3* self, void*, float x, float y, float z) {
    self->v[0] = x;
    self->v[1] = y;
    self->v[2] = z;
}

// 0x00003df0  Vector3::negate(): flips the sign of the three floats in place with `fchs`, one load/store each
// (0x10003e10-0x10003e17, 0x10003e1c-0x10003e24, 0x10003e2a-0x10003e32). `fchs` only toggles the sign bit, so the
// float store is exact. No argument (`ret` at 0x10003e3b), no return value, no branch.
typedef void(__fastcall* Vec3Negate_t)(Vec3*, void*);
Vec3Negate_t Vec3Negate_orig;
void __fastcall Vec3Negate_re(Vec3* self, void*) {
    self->v[0] = -self->v[0];
    self->v[1] = -self->v[1];
    self->v[2] = -self->v[2];
}

// 0x00004530  Quat::conjugate(): the same three `fchs` over [this], [this+4], [this+8] (0x10004550-0x10004557,
// 0x1000455c-0x10004564, 0x1000456a-0x10004572), i.e. the vector part only; the scalar at [this+0xc] is not
// touched. Byte for byte the same body as Vector3::negate 0x10003df0 at a different address.
typedef void(__fastcall* QuatConjugate_t)(Quat*, void*);
QuatConjugate_t QuatConjugate_orig;
void __fastcall QuatConjugate_re(Quat* self, void*) {
    self->v[0] = -self->v[0];
    self->v[1] = -self->v[1];
    self->v[2] = -self->v[2];
}

// ----------------------------------------------------------------------------------- Vector3, one operation

// 0x00004020  Vector3::addXYZ(x, y, z): three independent `fld <argument>; fadd <field>; fstp <field>` groups
// (0x10004040-0x10004048 for [this], 0x1000404d-0x10004056 for [this+4], 0x1000405c-0x10004065 for [this+8]).
// The argument is the one loaded first, so the sum is argument + field. Each result is stored as a float.
// __thiscall, `ret 0xc` at 0x1000406e, no return value, no branch.
typedef void(__fastcall* Vec3AddXYZ_t)(Vec3*, void*, float, float, float);
Vec3AddXYZ_t Vec3AddXYZ_orig;
void __fastcall Vec3AddXYZ_re(Vec3* self, void*, float x, float y, float z) {
    self->v[0] = static_cast<float>(static_cast<double>(x) + self->v[0]);
    self->v[1] = static_cast<float>(static_cast<double>(y) + self->v[1]);
    self->v[2] = static_cast<float>(static_cast<double>(z) + self->v[2]);
}

// 0x000038b0  Vector3::addAssignRet(o): loop i = 0..2 (counter at [ebp-8], `cmp ... 3` / `jge` at 0x100038df and
// 0x100038e3) of `fld [this + i*4]; fadd [o + i*4]; fstp [this + i*4]` (0x100038f1, 0x100038f4, 0x100038fd):
// field + argument, stored back as a float. Returns `this` in eax (0x10003902). One stack argument (`ret 4`).
typedef Vec3*(__fastcall* Vec3AddAssign_t)(Vec3*, void*, const Vec3*);
Vec3AddAssign_t Vec3AddAssign_orig;
Vec3* __fastcall Vec3AddAssign_re(Vec3* self, void*, const Vec3* o) {
    for (int i = 0; i < 3; i++)
        self->v[i] = static_cast<float>(static_cast<double>(self->v[i]) + o->v[i]);
    return self;
}

// 0x00003930  Vector3::subAssignRet(o): the same loop with `fsub` (0x10003971, 0x10003974, 0x1000397d), so
// field - argument in that order, stored as a float; returns `this` (0x10003982). One stack argument (`ret 4`).
typedef Vec3*(__fastcall* Vec3SubAssign_t)(Vec3*, void*, const Vec3*);
Vec3SubAssign_t Vec3SubAssign_orig;
Vec3* __fastcall Vec3SubAssign_re(Vec3* self, void*, const Vec3* o) {
    for (int i = 0; i < 3; i++)
        self->v[i] = static_cast<float>(static_cast<double>(self->v[i]) - o->v[i]);
    return self;
}

// 0x00003790  Vector3::add(o) -> Vector3 by value: loop i = 0..2 (counter [ebp-0x14], `jge` 0x100037c3) of
// `fld [this + i*4]; fadd [o + i*4]; fstp [ebp + i*4 - 0x10]` (0x100037d1, 0x100037d4, 0x100037da), so the three
// float sums land in a local Vector3 first; they are then copied as integers into the caller's return buffer
// ([ebp+8], 0x100037e6, 0x100037eb, 0x100037f1) and the buffer is returned in eax (0x100037f4). The return
// buffer is the first stack argument and the operand the second (`ret 8` at 0x100037fd).
typedef Vec3*(__fastcall* Vec3Add_t)(const Vec3*, void*, Vec3*, const Vec3*);
Vec3Add_t Vec3Add_orig;
Vec3* __fastcall Vec3Add_re(const Vec3* self, void*, Vec3* ret, const Vec3* o) {
    float t[3];
    for (int i = 0; i < 3; i++)
        t[i] = static_cast<float>(static_cast<double>(self->v[i]) + o->v[i]);
    ret->v[0] = t[0];
    ret->v[1] = t[1];
    ret->v[2] = t[2];
    return ret;
}

// 0x000039b0  Vector3::mulScalar(s) -> Vector3 by value: loop i = 0..2 (counter [ebp-0x14], `jge` 0x100039e3) of
// `fld [ebp+0xc]; fmul [this + i*4]; fstp [ebp + i*4 - 0x10]` (0x100039eb, 0x100039ee, 0x100039f4): the scalar is
// the left operand, each product is stored as a float into a local, and the local is copied as integers into the
// return buffer [ebp+8] (0x10003a00, 0x10003a05, 0x10003a0b), which is returned in eax. `ret 8` at 0x10003a17.
typedef Vec3*(__fastcall* Vec3MulScalar_t)(const Vec3*, void*, Vec3*, float);
Vec3MulScalar_t Vec3MulScalar_orig;
Vec3* __fastcall Vec3MulScalar_re(const Vec3* self, void*, Vec3* ret, float s) {
    float t[3];
    for (int i = 0; i < 3; i++)
        t[i] = static_cast<float>(static_cast<double>(s) * self->v[i]);
    ret->v[0] = t[0];
    ret->v[1] = t[1];
    ret->v[2] = t[2];
    return ret;
}

// 0x00003a40  Vector3::mulAssignScalar(s): loop i = 0..2 (counter [ebp-8], `jge` 0x10003a73) of `fld [ebp+8];
// fmul [this + i*4]; fstp [this + i*4]` (0x10003a7b, 0x10003a7e, 0x10003a87) - scalar on the left, product
// stored as a float straight back into the field, so iteration i reads the field it is about to overwrite and
// nothing else. Returns `this` in eax (0x10003a8c); one stack argument (`ret 4`).
typedef Vec3*(__fastcall* Vec3MulAssign_t)(Vec3*, void*, float);
Vec3MulAssign_t Vec3MulAssign_orig;
Vec3* __fastcall Vec3MulAssign_re(Vec3* self, void*, float s) {
    for (int i = 0; i < 3; i++)
        self->v[i] = static_cast<float>(static_cast<double>(s) * self->v[i]);
    return self;
}

// ----------------------------------------------------------------------------------- Vector3, several operations

// 0x00003c90  Vector3::dot(o) -> float: three products in source order, summed left to right on the x87 stack
// with no spill in between - `fld [this]; fmul [o]` (0x10003cb3), `fld [this+4]; fmul [o+4]; faddp` (0x10003cbd,
// 0x10003cc0, 0x10003cc3), `fld [this+8]; fmul [o+8]; faddp` (0x10003ccb, 0x10003cce, 0x10003cd1). The result is
// left in st(0) for the caller, so the only rounding to float happens where the caller stores it. Both additions
// round to 53 bits (control word 0x027f), which is why the two sums must be written as `double` here: the same
// expression in float arithmetic returns a different float for 3 of the 5 probe vectors. One stack argument
// (`ret 4` at 0x10003cd9), no branch, nothing written.
typedef float(__fastcall* Vec3Dot_t)(const Vec3*, void*, const Vec3*);
Vec3Dot_t Vec3Dot_orig;
float __fastcall Vec3Dot_re(const Vec3* self, void*, const Vec3* o) {
    const double p0 = static_cast<double>(self->v[0]) * o->v[0];
    const double p1 = static_cast<double>(self->v[1]) * o->v[1];
    const double p2 = static_cast<double>(self->v[2]) * o->v[2];
    return static_cast<float>((p0 + p1) + p2);
}

// 0x00003e50  Vector3::cross(o) -> Vector3 by value: three two-product expressions, each stored as a float into a
// local before the next starts.
//   [ebp-0x10] = this[1]*o[2] - this[2]*o[1]   (0x10003e73, 0x10003e76, 0x10003e7f, 0x10003e82, fsubp 0x10003e85,
//                                               fstp 0x10003e87)
//   [ebp-0x0c] = (-this[0])*o[2] + this[2]*o[0] (`fchs` on this[0] at 0x10003e8f before the fmul at 0x10003e94,
//                                               second product 0x10003e9d/0x10003ea0, faddp 0x10003ea2)
//   [ebp-0x08] = this[0]*o[1] - this[1]*o[0]   (0x10003ead, 0x10003eaf, 0x10003eb8, 0x10003ebb, fsubp 0x10003ebd)
// The three floats are then copied as integers into the return buffer [ebp+8] (0x10003ec8, 0x10003ecd, 0x10003ed3)
// which is returned in eax. Each `fsubp`/`faddp` rounds to 53 bits, so each component is one double-precision
// combination of two exact products, rounded once to float. `ret 8` at 0x10003edf, no branch.
typedef Vec3*(__fastcall* Vec3Cross_t)(const Vec3*, void*, Vec3*, const Vec3*);
Vec3Cross_t Vec3Cross_orig;
Vec3* __fastcall Vec3Cross_re(const Vec3* self, void*, Vec3* ret, const Vec3* o) {
    const float x = static_cast<float>(static_cast<double>(self->v[1]) * o->v[2] -
                                       static_cast<double>(self->v[2]) * o->v[1]);
    const float y = static_cast<float>(-static_cast<double>(self->v[0]) * o->v[2] +
                                       static_cast<double>(self->v[2]) * o->v[0]);
    const float z = static_cast<float>(static_cast<double>(self->v[0]) * o->v[1] -
                                       static_cast<double>(self->v[1]) * o->v[0]);
    ret->v[0] = x;
    ret->v[1] = y;
    ret->v[2] = z;
    return ret;
}

// ----------------------------------------------------------------------------------- Matrix and Quat

// 0x00004750  Matrix::setRotation(q): writes the 3x3 rotation block of the matrix at [this+4] (m[0] at +4,
// column-major) from the quaternion at [ebp+8] (x,y,z at +0,+4,+8 and w at +0xc). Twelve float temporaries are
// computed and spilled first, each one `fstp dword`:
//   [ebp-0x2c] x2 = x+x (0x10004773)   [ebp-0x30] y2 = y+y (0x10004780)   [ebp-0x34] z2 = z+z (0x1000478f)
//   [ebp-0x14] xx = x2*x (0x1000479b)  [ebp-0x18] xy = y2*x (0x100047a6)  [ebp-0x1c] xz = z2*x (0x100047b1)
//   [ebp-0x20] yy = y2*y (0x100047bc)  [ebp-0x24] yz = z2*y (0x100047c8)  [ebp-0x28] zz = z2*z (0x100047d4)
//   [ebp-0x08] wx = x2*w (0x100047e0)  [ebp-0x0c] wy = y2*w (0x100047ec)  [ebp-0x10] wz = z2*w (0x100047f8)
// then nine stores, in this order (offset in the object, expression, instruction that ends it):
//   +0x04 m[0]  = 1 - (yy + zz)   fadd 0x10004804, fsubr [0x1011d050] 0x10004807, fstp 0x10004810
//   +0x14 m[4]  = xy - wz         0x10004816 / 0x1000481c
//   +0x24 m[8]  = xz + wy         0x10004822 / 0x10004828
//   +0x08 m[1]  = xy + wz         0x1000482e / 0x10004834
//   +0x18 m[5]  = 1 - (xx + zz)   0x1000483a, fsubr 0x1000483d, 0x10004846
//   +0x28 m[9]  = yz - wx         0x1000484c / 0x10004852
//   +0x0c m[2]  = xz - wy         0x10004858 / 0x1000485e
//   +0x1c m[6]  = yz + wx         0x10004864 / 0x1000486a
//   +0x2c m[10] = 1 - (xx + yy)   0x10004870, fsubr 0x10004873, 0x1000487c
// The other seven slots and the vtable pointer are left as they were. The constant at 0x1011d050 is 1.0f and it
// is the LEFT operand of the subtraction (`fsubr` = memory minus st(0)); the three `1 - (a + b)` expressions are
// two chained 53-bit operations, so they are the components that need `double` here. One stack argument
// (`ret 4` at 0x10004885), no return value, no branch.
typedef void(__fastcall* MatSetRotation_t)(Mat4*, void*, const Quat*);
MatSetRotation_t MatSetRotation_orig;
void __fastcall MatSetRotation_re(Mat4* self, void*, const Quat* q) {
    const float x2 = static_cast<float>(static_cast<double>(q->v[0]) + q->v[0]);
    const float y2 = static_cast<float>(static_cast<double>(q->v[1]) + q->v[1]);
    const float z2 = static_cast<float>(static_cast<double>(q->v[2]) + q->v[2]);
    const float xx = static_cast<float>(static_cast<double>(x2) * q->v[0]);
    const float xy = static_cast<float>(static_cast<double>(y2) * q->v[0]);
    const float xz = static_cast<float>(static_cast<double>(z2) * q->v[0]);
    const float yy = static_cast<float>(static_cast<double>(y2) * q->v[1]);
    const float yz = static_cast<float>(static_cast<double>(z2) * q->v[1]);
    const float zz = static_cast<float>(static_cast<double>(z2) * q->v[2]);
    const float wx = static_cast<float>(static_cast<double>(x2) * q->w);
    const float wy = static_cast<float>(static_cast<double>(y2) * q->w);
    const float wz = static_cast<float>(static_cast<double>(z2) * q->w);
    const double one = Konst(0x1011d050);                                  // 1.0f, the fsubr operand
    self->m[0] = static_cast<float>(one - (static_cast<double>(yy) + zz));
    self->m[4] = static_cast<float>(static_cast<double>(xy) - wz);
    self->m[8] = static_cast<float>(static_cast<double>(xz) + wy);
    self->m[1] = static_cast<float>(static_cast<double>(xy) + wz);
    self->m[5] = static_cast<float>(one - (static_cast<double>(xx) + zz));
    self->m[9] = static_cast<float>(static_cast<double>(yz) - wx);
    self->m[2] = static_cast<float>(static_cast<double>(xz) - wy);
    self->m[6] = static_cast<float>(static_cast<double>(yz) + wx);
    self->m[10] = static_cast<float>(one - (static_cast<double>(xx) + yy));
}

// 0x00004230  Quat::setAxisAngle(angle, axis): turns an angle in DEGREES and an axis into a rotation quaternion.
// The half-angle in radians is 0.5f (the constant at 0x1011d04c) * angle ([ebp+8]) * 0.017453292f (the constant
// at 0x1011d020, pi/180): fld 0x1000424d, fmul 0x10004253, fmul 0x10004256. The product then goes two ways, and
// they are NOT the same value:
//   * `fst dword ptr [ebp-8]` at 0x1000425c keeps st(0) and writes a float COPY;
//   * `fstp qword ptr [esp]` at 0x10004262 passes the unrounded 53-bit st(0) as a double to the CRT routine
//     called at 0x10004265 (jgld.dll+0x7ea54, cos: re/match/jgld_math.cpp matches `cos(half)` to this call), and
//     its result is stored as a float at [ebp-0xc] (0x1000426d);
//   * the float copy is reloaded at 0x10004270 and passed as a double to the routine called at 0x10004279
//     (jgld.dll+0x7e9a4, sin), whose result is stored as a float at [ebp-0x10] (0x10004281).
// The cosine becomes the scalar part at [this+0xc] (0x1000428a); the axis is copied into [this], [this+4],
// [this+8] with integer moves (0x10004295, 0x1000429a, 0x100042a0); then the sine is pushed and
// Vector3::operator*= is called with ecx = this (0x100042aa, through the incremental-link thunk at 0x10001a55
// that jumps to 0x10003a40), scaling the axis in place. The call at 0x100042b7 is the debug build's __chkesp.
// Two stack arguments (`ret 8` at 0x100042bf), no return value, no branch of its own.
typedef void(__fastcall* QuatSetAxisAngle_t)(Quat*, void*, float, const Vec3*);
QuatSetAxisAngle_t QuatSetAxisAngle_orig;
typedef double(__cdecl* Trig_t)(double);
void __fastcall QuatSetAxisAngle_re(Quat* self, void*, float angle, const Vec3* axis) {
    const Trig_t kCos = reinterpret_cast<Trig_t>(JgldBase() + 0x0007ea54);
    const Trig_t kSin = reinterpret_cast<Trig_t>(JgldBase() + 0x0007e9a4);
    const double half = (static_cast<double>(Konst(0x1011d04c)) * angle) * Konst(0x1011d020);
    const float halfF = static_cast<float>(half);                      // fst dword [ebp-8]
    const float c = static_cast<float>(kCos(half));                    // the unrounded half angle
    const float s = static_cast<float>(kSin(static_cast<double>(halfF)));  // the float copy, reloaded
    self->w = c;
    self->v[0] = axis->v[0];
    self->v[1] = axis->v[1];
    self->v[2] = axis->v[2];
    reinterpret_cast<Vec3MulAssign_t>(JgldBase() + 0x00003a40)(reinterpret_cast<Vec3*>(self), 0, s);
}

}  // namespace

SG_HOOK("jgld.dll", 0x000036e0, Vector3_set, Vec3Set_re, Vec3Set_orig);
SG_HOOK("jgld.dll", 0x00003df0, Vector3_negate, Vec3Negate_re, Vec3Negate_orig);
SG_HOOK("jgld.dll", 0x00004530, Quat_conjugate, QuatConjugate_re, QuatConjugate_orig);
SG_HOOK("jgld.dll", 0x00004020, Vector3_addXYZ, Vec3AddXYZ_re, Vec3AddXYZ_orig);
SG_HOOK("jgld.dll", 0x000038b0, Vector3_addAssignRet, Vec3AddAssign_re, Vec3AddAssign_orig);
SG_HOOK("jgld.dll", 0x00003930, Vector3_subAssignRet, Vec3SubAssign_re, Vec3SubAssign_orig);
SG_HOOK("jgld.dll", 0x00003790, Vector3_add, Vec3Add_re, Vec3Add_orig);
SG_HOOK("jgld.dll", 0x000039b0, Vector3_mulScalar, Vec3MulScalar_re, Vec3MulScalar_orig);
SG_HOOK("jgld.dll", 0x00003a40, Vector3_mulAssignScalar, Vec3MulAssign_re, Vec3MulAssign_orig);
SG_HOOK("jgld.dll", 0x00003c90, Vector3_dot, Vec3Dot_re, Vec3Dot_orig);
SG_HOOK("jgld.dll", 0x00003e50, Vector3_cross, Vec3Cross_re, Vec3Cross_orig);
SG_HOOK("jgld.dll", 0x00004750, Matrix_setRotation, MatSetRotation_re, MatSetRotation_orig);
SG_HOOK("jgld.dll", 0x00004230, Quat_setAxisAngle, QuatSetAxisAngle_re, QuatSetAxisAngle_orig);
