// Batch c3at: jgld.dll's quaternion product family, the Transform/Matrix copy-and-set helpers, two trivial
// constructors and libpng's png_set_cHRM / png_set_gAMA. SG_HOOK addresses are RVAs; every instruction cited in
// a comment is a VA (0x10000000 + RVA), the form the debug build's disassembly prints. jgld.dll is LoadLibrary'd
// after the shim starts, so these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp)
// and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// FLOATING POINT. Only three bodies here do arithmetic: Quat::operator*= and Quat::operator* (one float product
// and one subtraction each) and the two libpng setters (double -> float stores). The method is c3ap's, verified
// there: the A/B thread's x87 control word is 0x027f (PC = 53-bit, RC = nearest), so every x87 operation rounds
// exactly like an IEEE double operation; each `fstp dword` in the original is a cast to `float` here and every
// operand inside one expression is widened with static_cast<double>. Vector3::dot (0x10003c90) leaves its result
// in st(0) unrounded, so it is declared here as returning `double`, not `float`: the original's `fsubr` consumes
// that 53-bit value, and rounding it to float first changes the answer.
// Everything else in this file is a memberwise copy; those are integer `mov`s in the original, so they are
// 32-bit word copies here and the fixture stores non-float bit patterns to prove it.
#include "hooks.h"

namespace {

// jgld.dll's math types (re/match/jgld_math.cpp, 100% matches): Vector3 {float v[3]}, Quat {Vector3 v; float w}
// (w at +0xc), Matrix {vptr; float m[16]} (m[0] at +4), Transform {vptr; Quat q (+4); Vector3 p (+0x14);
// unsigned flags (+0x20)}.
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
struct Xform {
    void* vptr;
    float q[4];
    float p[3];
    unsigned int flags;
};

unsigned char* JgldBase() {
    static unsigned char* base;
    if (!base) base = reinterpret_cast<unsigned char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// An address inside jgld.dll given as the VA the disassembly prints.
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000u); }

// The originals copy whole objects with integer `mov`s / `rep movsd`, so bit patterns that are not valid floats
// survive; every memberwise copy below goes through this.
void CopyWords(void* dst, const void* src, int n) {
    unsigned int* d = static_cast<unsigned int*>(dst);
    const unsigned int* s = static_cast<const unsigned int*>(src);
    for (int i = 0; i < n; i++) d[i] = s[i];
}

// jgld.dll functions called at their own addresses (incremental-link thunks resolved: 0x10001884 -> 0x100039b0,
// 0x10001a64 -> 0x10003c90, 0x100011a4 -> 0x10003e50, 0x1000133e -> 0x10003790, 0x10001adc -> 0x10004410,
// 0x10001203 -> 0x10004530, 0x10001113 -> 0x10004750, 0x10001a37 -> 0x100046f0, 0x10001924 -> 0x10003170,
// 0x10001960 -> 0x100036e0).
typedef Vec3*(__fastcall* Vec3Add_t)(const Vec3*, void*, Vec3*, const Vec3*);           // Vector3::operator+
typedef Vec3*(__fastcall* Vec3MulScalar_t)(const Vec3*, void*, Vec3*, float);           // Vector3::operator*(float)
typedef double(__fastcall* Vec3Dot_t)(const Vec3*, void*, const Vec3*);                 // Vector3::dot -> st(0)
typedef Vec3*(__fastcall* Vec3Cross_t)(const Vec3*, void*, Vec3*, const Vec3*);         // Vector3::cross
typedef void(__fastcall* Vec3Set_t)(Vec3*, void*, float, float, float);                 // Vector3::set
typedef void(__fastcall* QuatIdentity_t)(Quat*, void*);                                 // Quat::identity
typedef void(__fastcall* QuatConjugate_t)(Quat*, void*);                                // Quat::conjugate
typedef Quat*(__fastcall* QuatMul_t)(const Quat*, void*, Quat*, const Quat*);           // Quat::operator*
typedef void(__fastcall* MatSetRotation_t)(Mat4*, void*, const Quat*);                  // Matrix::setRotation
typedef void(__fastcall* MatSetTranslation_t)(Mat4*, void*, const Vec3*);               // Matrix::setTranslation

Vec3Add_t Jadd() { return reinterpret_cast<Vec3Add_t>(JgldVA(0x10003790)); }
Vec3MulScalar_t Jmul() { return reinterpret_cast<Vec3MulScalar_t>(JgldVA(0x100039b0)); }
Vec3Dot_t Jdot() { return reinterpret_cast<Vec3Dot_t>(JgldVA(0x10003c90)); }
Vec3Cross_t Jcross() { return reinterpret_cast<Vec3Cross_t>(JgldVA(0x10003e50)); }

// ----------------------------------------------------------------------- the quaternion product and its users

// The body shared by Quat::operator*= (0x100042f0) and Quat::operator* (0x10004410): the two are the same
// instruction sequence over different destinations. `r` is built in locals and only then copied out.
//   scalar part: `fld [this+0xc]; fmul [q+0xc]; fstp dword [ebp-0x54]` (0x10004321-0x10004327 / 0x10004441-
//   0x10004447) is w * q.w rounded to float; Vector3::dot(q.v) is called next (0x10004331 / 0x10004451) and
//   leaves its 53-bit result in st(0); `fsubr dword [ebp-0x54]` (0x10004336 / 0x10004456) is memory MINUS st(0),
//   i.e. (w*q.w) - dot, and `fstp dword [ebp-8]` rounds that to float.
//   vector part: the three operands are built right to left, each into its own local -
//     v * q.w        (0x1000434a / 0x1000446a, Vector3::operator*(float) with this = &v, scalar = q.w)
//     q.v * w        (0x10004362 / 0x10004482, this = &q.v, scalar = w)
//     v.cross(q.v)   (0x10004377 / 0x10004497)
//   then two Vector3::operator+ calls chained through eax (0x1000437e and 0x10004385 / 0x1000449e and
//   0x100044a5): (cross + q.v*w) + v*q.w.
// The Quat::Quat() call on the local r (0x10004316 / 0x10004436) writes 0,0,0,1 into fields that are all
// overwritten before anything reads them, so it has no observable effect and is not reproduced.
void QuatProduct(const Quat* self, const Quat* q, Quat* r) {
    const float ww = static_cast<float>(static_cast<double>(self->w) * q->w);
#ifdef SG_C3AT_FLOATMODEL  // negative control: rounds Vector3::dot's st(0) result to float before the fsubr
    const float d = static_cast<float>(Jdot()(reinterpret_cast<const Vec3*>(self), 0,
                                              reinterpret_cast<const Vec3*>(q)));
#else
    const double d = Jdot()(reinterpret_cast<const Vec3*>(self), 0, reinterpret_cast<const Vec3*>(q));
#endif
    const float rw = static_cast<float>(static_cast<double>(ww) - d);

    Vec3 vqw, qvw, cr, s1, s2;
    Jmul()(reinterpret_cast<const Vec3*>(self), 0, &vqw, q->w);
    Jmul()(reinterpret_cast<const Vec3*>(q), 0, &qvw, self->w);
    Jcross()(reinterpret_cast<const Vec3*>(self), 0, &cr, reinterpret_cast<const Vec3*>(q));
    Jadd()(&cr, 0, &s1, &qvw);
    Jadd()(&s1, 0, &s2, &vqw);

    r->v[0] = s2.v[0];
    r->v[1] = s2.v[1];
    r->v[2] = s2.v[2];
    r->w = rw;
}

// 0x000042f0  Quat::mulAssign
// Quat& Quat::operator*=(const Quat&): the product above, copied over `this` (v at 0x100043a1-0x100043ac, w at
// 0x100043af) and `this` returned in eax (0x100043b5). One stack argument, `ret 4` at 0x100043cb. Every read of
// `this` happens before the first write, so the argument may alias `this`.
typedef Quat*(__fastcall* QuatMulAssign_t)(Quat*, void*, const Quat*);
QuatMulAssign_t QuatMulAssign_orig;
Quat* __fastcall QuatMulAssign_re(Quat* self, void*, const Quat* q) {
    Quat r;
    QuatProduct(self, q, &r);
    CopyWords(self, &r, 4);
    return self;
}

// 0x00004410  Quat::mul
// Quat Quat::operator*(const Quat&): the same product written into the caller's return buffer, which is the
// first stack argument ([ebp+8], stores at 0x100044c1-0x100044d2) with the operand second ([ebp+0xc]); the
// buffer is returned in eax (0x100044d5). `ret 8` at 0x100044eb.
typedef Quat*(__fastcall* QuatMul2_t)(const Quat*, void*, Quat*, const Quat*);
QuatMul2_t QuatMul_orig;
Quat* __fastcall QuatMul_re(const Quat* self, void*, Quat* ret, const Quat* q) {
    Quat r;
    QuatProduct(self, q, &r);
    CopyWords(ret, &r, 4);
    return ret;
}

// 0x00004100  Vector3::rotate
// void Vector3::rotate(const Quat* q): rotates the vector in place by q, as q * (0, v) * conjugate(q).
// `cmp dword ptr [ebp+8], 0` / `jne` at 0x10004133-0x10004137 returns at once for a null argument (the jump
// target 0x100041dd is the epilogue, so nothing is written). Otherwise: c = *q (four integer moves,
// 0x10004141-0x10004155); p.w = 0 as an integer zero (0x10004158); p.v = *this (0x10004162-0x10004170);
// p = c * p (Quat::operator*, this = &c, argument = &p, 0x1000417e, result copied back over p at
// 0x10004183-0x10004197); c.conjugate() (0x1000419d, flips the sign of c's three vector components);
// p = p * c (this = &p, argument = &c, 0x100041ad, copied back at 0x100041b2-0x100041c6); finally p's vector
// part is stored over *this (0x100041cf-0x100041da) and p's scalar part is discarded. The two Quat::Quat()
// calls on the locals (0x10004126, 0x1000412b) write only fields that are overwritten before they are read.
// One stack argument, `ret 4` at 0x100041f0, no return value.
typedef void(__fastcall* Vec3Rotate_t)(Vec3*, void*, const Quat*);
Vec3Rotate_t Vec3Rotate_orig;
void __fastcall Vec3Rotate_re(Vec3* self, void*, const Quat* q) {
    if (q == 0) return;
    Quat c, p, t;
    CopyWords(&c, q, 4);
    p.w = 0.0f;
    CopyWords(&p, self, 3);
    reinterpret_cast<QuatMul_t>(JgldVA(0x10004410))(&c, 0, &t, &p);
    CopyWords(&p, &t, 4);
    reinterpret_cast<QuatConjugate_t>(JgldVA(0x10004530))(&c, 0);
    reinterpret_cast<QuatMul_t>(JgldVA(0x10004410))(&p, 0, &t, &c);
    CopyWords(&p, &t, 4);
    CopyWords(self, &p, 3);
}

// ----------------------------------------------------------------------- Matrix: copy, assign, compose

// 0x00006720  Matrix::copyCtor
// Matrix::Matrix(const Matrix&), compiler generated: the class holds a union of `float m[16]` and `float
// e[4][4]`, so the 16 dwords at source+4 are copied to this+4 TWICE (`rep movsd` with ecx = 0x10 at
// 0x1000674e and again at 0x10006761, once per union member). The vtable pointer is written last
// (0x10006766, the Matrix vtable at VA 0x1011d024) and `this` is returned in eax (0x1000676c). `ret 4`.
typedef Mat4*(__fastcall* MatCopyCtor_t)(Mat4*, void*, const Mat4*);
MatCopyCtor_t MatCopyCtor_orig;
Mat4* __fastcall MatCopyCtor_re(Mat4* self, void*, const Mat4* src) {
    CopyWords(self->m, src->m, 16);
    CopyWords(self->m, src->m, 16);
    self->vptr = JgldVA(0x1011d024);
    return self;
}

// 0x00006790  Matrix::assign
// Matrix& Matrix::operator=(const Matrix&), compiler generated: the same union copied twice, this time as two
// counted loops over the 16 dwords at +4 (counter [ebp-8], `cmp ... 0x10` / `jae` at 0x100067bf-0x100067c3 and
// counter [ebp-0xc], `jae` at 0x100067ed-0x100067f1; both compares are unsigned). The vtable pointer is NOT
// touched, which is what separates this body from the copy constructor above. Returns `this` (0x10006809),
// `ret 4`.
typedef Mat4*(__fastcall* MatAssign_t)(Mat4*, void*, const Mat4*);
MatAssign_t MatAssign_orig;
Mat4* __fastcall MatAssign_re(Mat4* self, void*, const Mat4* src) {
    unsigned int i;
    for (i = 0; i < 16; i++) CopyWords(&self->m[i], &src->m[i], 1);
    for (i = 0; i < 16; i++) CopyWords(&self->m[i], &src->m[i], 1);
    return self;
}

// 0x00004690  Matrix::setRotationTranslation
// void Matrix::setRotationTranslation(const Quat&, const Vector3&): two calls and nothing else -
// Matrix::setRotation with the first stack argument (0x100046b4, thunk to 0x10004750), which writes the nine
// slots of the 3x3 block, then Matrix::setTranslation with the second (0x100046c0, thunk to 0x100046f0), which
// copies the three floats into m[12..14] (object +0x34, +0x38, +0x3c). The other four slots and the vtable
// pointer are left as they were. `ret 8` at 0x100046d5, no return value, no branch of its own.
typedef void(__fastcall* MatSetRT_t)(Mat4*, void*, const Quat*, const Vec3*);
MatSetRT_t MatSetRT_orig;
void __fastcall MatSetRT_re(Mat4* self, void*, const Quat* q, const Vec3* t) {
    reinterpret_cast<MatSetRotation_t>(JgldVA(0x10004750))(self, 0, q);
    reinterpret_cast<MatSetTranslation_t>(JgldVA(0x100046f0))(self, 0, t);
}

// ----------------------------------------------------------------------- Transform: set, copy, assign, reset

// 0x000054b0  Transform::setTranslation
// void Transform::setTranslation(const Vector3&): stores 2 in the flags word at this+0x20 (0x100054d0), calls
// Quat::identity on the quaternion at this+4 (0x100054dd, thunk to 0x10003170: it sets the vector part to
// 0,0,0 through Vector3::set and the scalar at +0xc to 1.0f, bits 0x3f800000), then copies the argument's three
// floats into the position at this+0x14 with integer moves (0x100054eb-0x100054f8). The vtable pointer is not
// touched. One stack argument, `ret 4` at 0x1000550b, no return value, no branch.
typedef void(__fastcall* XfSetTranslation_t)(Xform*, void*, const Vec3*);
XfSetTranslation_t XfSetTranslation_orig;
void __fastcall XfSetTranslation_re(Xform* self, void*, const Vec3* p) {
    self->flags = 2;
    reinterpret_cast<QuatIdentity_t>(JgldVA(0x10003170))(reinterpret_cast<Quat*>(self->q), 0);
    CopyWords(self->p, p, 3);
}

// 0x00005530  Transform::setRotation
// void Transform::setRotation(const Quat&): stores 0x1c in the flags word at this+0x20 (0x10005550), copies the
// argument's four dwords into the quaternion at this+4 (0x10005560-0x10005573), then calls Vector3::set on the
// position at this+0x14 with three integer zeros pushed as the arguments (0x10005576-0x10005582, thunk to
// 0x100036e0), i.e. p = (0,0,0). One stack argument, `ret 4` at 0x10005597, no return value, no branch.
typedef void(__fastcall* XfSetRotation_t)(Xform*, void*, const Quat*);
XfSetRotation_t XfSetRotation_orig;
void __fastcall XfSetRotation_re(Xform* self, void*, const Quat* q) {
    self->flags = 0x1c;
    CopyWords(self->q, q, 4);
    reinterpret_cast<Vec3Set_t>(JgldVA(0x100036e0))(reinterpret_cast<Vec3*>(self->p), 0, 0.0f, 0.0f, 0.0f);
}

// 0x000055c0  Transform::set
// void Transform::set(const Quat&, const Vector3&): stores 0x1e in the flags word at this+0x20 (0x100055e0),
// copies the first argument's four dwords into this+4 (0x100055f0-0x10005603) and the second argument's three
// dwords into this+0x14 (0x1000560f-0x1000561c). No call, no branch; the vtable pointer is not touched.
// Two stack arguments, `ret 8` at 0x10005625, no return value.
typedef void(__fastcall* XfSet_t)(Xform*, void*, const Quat*, const Vec3*);
XfSet_t XfSet_orig;
void __fastcall XfSet_re(Xform* self, void*, const Quat* q, const Vec3* p) {
    self->flags = 0x1e;
    CopyWords(self->q, q, 4);
    CopyWords(self->p, p, 3);
}

// 0x00005650  Transform::reset
// void Transform::reset(): one store, `mov dword ptr [eax+0x20], 0` at 0x10005670, clearing the flags word.
// The quaternion at +4, the position at +0x14 and the vtable pointer keep whatever they held. No argument
// (`ret` at 0x1000567d), no return value, no branch.
typedef void(__fastcall* XfReset_t)(Xform*, void*);
XfReset_t XfReset_orig;
void __fastcall XfReset_re(Xform* self, void*) {
    self->flags = 0;
}

// 0x00006840  Transform::assignOp
// Transform& Transform::operator=(const Transform&), compiler generated: the quaternion's four dwords
// (source+4 -> this+4, 0x10006869-0x1000687c), the position's three dwords (source+0x14 -> this+0x14,
// 0x1000688b-0x10006898) and the flags word (source+0x20 -> this+0x20, 0x100068a1-0x100068a4), in that order.
// The vtable pointer is NOT written, which is what separates it from the copy constructor below. Returns `this`
// in eax (0x100068a7), `ret 4`.
typedef Xform*(__fastcall* XfAssign_t)(Xform*, void*, const Xform*);
XfAssign_t XfAssign_orig;
Xform* __fastcall XfAssign_re(Xform* self, void*, const Xform* src) {
    CopyWords(self->q, src->q, 4);
    CopyWords(self->p, src->p, 3);
    self->flags = src->flags;
    return self;
}

// 0x000068d0  Transform::copyCtor
// Transform::Transform(const Transform&), compiler generated: the same three copies as operator= above
// (0x100068f9-0x1000690c, 0x1000691b-0x10006928, 0x10006931-0x10006934) and then the vtable pointer, written
// last, with the Transform vtable at VA 0x1011d028 (0x1000693a). Returns `this` (0x10006940), `ret 4`.
typedef Xform*(__fastcall* XfCopyCtor_t)(Xform*, void*, const Xform*);
XfCopyCtor_t XfCopyCtor_orig;
Xform* __fastcall XfCopyCtor_re(Xform* self, void*, const Xform* src) {
    CopyWords(self->q, src->q, 4);
    CopyWords(self->p, src->p, 3);
    self->flags = src->flags;
    self->vptr = JgldVA(0x1011d028);
    return self;
}

// ----------------------------------------------------------------------- two constructors

// 0x00007620  Random::ctor
// Random::Random(): one store, `mov dword ptr [eax], 0` at 0x10007640, zeroing the 32-bit state word at
// offset 0 (the same word Random::next 0x00007530 advances). Nothing else in the object is touched. Returns
// `this` in eax (0x10007646). No argument, `ret` at 0x1000764f.
typedef void*(__fastcall* RandomCtor_t)(void*, void*);
RandomCtor_t RandomCtor_orig;
void* __fastcall RandomCtor_re(void* self, void*) {
    *static_cast<unsigned int*>(self) = 0;
    return self;
}

// 0x00002650  MappedFile::ctor
// MappedFile::MappedFile(): four stores and nothing else - the vtable pointer at +0 (VA 0x1011d01c,
// 0x10002670), 0 at +4 (0x10002679), 0xffffffff at +8 (0x10002683, -1 as a signed 32-bit value) and 0 at +0xc
// (0x1000268d). Returns `this` in eax (0x10002694). No argument, `ret` at 0x1000269d, no branch.
typedef void*(__fastcall* MappedFileCtor_t)(void*, void*);
MappedFileCtor_t MappedFileCtor_orig;
void* __fastcall MappedFileCtor_re(void* self, void*) {
    unsigned int* f = static_cast<unsigned int*>(self);
    f[0] = reinterpret_cast<unsigned int>(JgldVA(0x1011d01c));
    f[1] = 0;
    f[2] = 0xffffffffu;
    f[3] = 0;
    return self;
}

// ----------------------------------------------------------------------- libpng 1.0.5 setters (jgld links it)

// 0x0007db30  png_set_cHRM
// void png_set_cHRM(png_structp, png_infop, double white_x, double white_y, double red_x, double red_y,
// double green_x, double green_y, double blue_x, double blue_y): __cdecl (`ret` with no pop at 0x1007dbca).
// It returns without writing anything when either pointer is null - `cmp dword ptr [ebp+8], 0` / `je` at
// 0x1007db48-0x1007db4c and `cmp dword ptr [ebp+0xc], 0` / `jne` at 0x1007db4e-0x1007db52, the two failing
// paths meeting at the `jmp` to the epilogue at 0x1007db54. The png_struct pointer is only tested; everything
// is written through the png_info pointer. The eight doubles are converted to float one at a time
// (`fld qword ptr [ebp+N]; fstp dword ptr [info+M]`) into consecutive fields: +0x80 (0x1007db56), +0x84
// (0x1007db62), +0x88 (0x1007db6e), +0x8c (0x1007db7a), +0x90 (0x1007db86), +0x94 (0x1007db92), +0x98
// (0x1007db9e), +0x9c (0x1007dbaa), in the order the arguments are given. Last, the valid-chunk word at +8 is
// read, ORed with 4 (`or al, 4` at 0x1007dbbc works on the low byte of the whole loaded dword) and stored back
// (0x1007dbc1).
typedef void(__cdecl* PngSetCHRM_t)(void*, void*, double, double, double, double, double, double, double, double);
PngSetCHRM_t PngSetCHRM_orig;
void __cdecl PngSetCHRM_re(void* png_ptr, void* info_ptr, double white_x, double white_y, double red_x,
                           double red_y, double green_x, double green_y, double blue_x, double blue_y) {
    if (png_ptr == 0 || info_ptr == 0) return;
    unsigned char* info = static_cast<unsigned char*>(info_ptr);
    *reinterpret_cast<float*>(info + 0x80) = static_cast<float>(white_x);
    *reinterpret_cast<float*>(info + 0x84) = static_cast<float>(white_y);
    *reinterpret_cast<float*>(info + 0x88) = static_cast<float>(red_x);
    *reinterpret_cast<float*>(info + 0x8c) = static_cast<float>(red_y);
    *reinterpret_cast<float*>(info + 0x90) = static_cast<float>(green_x);
    *reinterpret_cast<float*>(info + 0x94) = static_cast<float>(green_y);
    *reinterpret_cast<float*>(info + 0x98) = static_cast<float>(blue_x);
    *reinterpret_cast<float*>(info + 0x9c) = static_cast<float>(blue_y);
    *reinterpret_cast<unsigned int*>(info + 8) |= 4;
}

// 0x0007dbd0  png_set_gAMA
// void png_set_gAMA(png_structp, png_infop, double gamma): the same two null guards (`je` at 0x1007dbec,
// `jne` at 0x1007dbf2, both reaching the epilogue through the `jmp` at 0x1007dbf4), then one conversion of the
// double at [ebp+0x10] into the float at info+0x28 (`fld qword` 0x1007dbf6, `fstp dword` 0x1007dbfc) and the
// valid-chunk word at info+8 ORed with 1 (`or edx, 1` at 0x1007dc05, stored back at 0x1007dc0b). The
// png_struct pointer is only tested. __cdecl, `ret` with no pop at 0x1007dc14.
typedef void(__cdecl* PngSetGAMA_t)(void*, void*, double);
PngSetGAMA_t PngSetGAMA_orig;
void __cdecl PngSetGAMA_re(void* png_ptr, void* info_ptr, double file_gamma) {
    if (png_ptr == 0 || info_ptr == 0) return;
    unsigned char* info = static_cast<unsigned char*>(info_ptr);
    *reinterpret_cast<float*>(info + 0x28) = static_cast<float>(file_gamma);
    *reinterpret_cast<unsigned int*>(info + 8) |= 1;
}

}  // namespace

SG_HOOK("jgld.dll", 0x000042f0, Quat_mulAssign, QuatMulAssign_re, QuatMulAssign_orig);
SG_HOOK("jgld.dll", 0x00004410, Quat_mul, QuatMul_re, QuatMul_orig);
SG_HOOK("jgld.dll", 0x00004100, Vector3_rotate, Vec3Rotate_re, Vec3Rotate_orig);
SG_HOOK("jgld.dll", 0x00006720, Matrix_copyCtor, MatCopyCtor_re, MatCopyCtor_orig);
SG_HOOK("jgld.dll", 0x00006790, Matrix_assign, MatAssign_re, MatAssign_orig);
SG_HOOK("jgld.dll", 0x00004690, Matrix_setRotationTranslation, MatSetRT_re, MatSetRT_orig);
SG_HOOK("jgld.dll", 0x000054b0, Transform_setTranslation, XfSetTranslation_re, XfSetTranslation_orig);
SG_HOOK("jgld.dll", 0x00005530, Transform_setRotation, XfSetRotation_re, XfSetRotation_orig);
SG_HOOK("jgld.dll", 0x000055c0, Transform_set, XfSet_re, XfSet_orig);
SG_HOOK("jgld.dll", 0x00005650, Transform_reset, XfReset_re, XfReset_orig);
SG_HOOK("jgld.dll", 0x00006840, Transform_assignOp, XfAssign_re, XfAssign_orig);
SG_HOOK("jgld.dll", 0x000068d0, Transform_copyCtor, XfCopyCtor_re, XfCopyCtor_orig);
SG_HOOK("jgld.dll", 0x00007620, Random_ctor, RandomCtor_re, RandomCtor_orig);
SG_HOOK("jgld.dll", 0x00002650, MappedFile_ctor, MappedFileCtor_re, MappedFileCtor_orig);
SG_HOOK("jgld.dll", 0x0007db30, png_set_cHRM, PngSetCHRM_re, PngSetCHRM_orig);
SG_HOOK("jgld.dll", 0x0007dbd0, png_set_gAMA, PngSetGAMA_re, PngSetGAMA_orig);
