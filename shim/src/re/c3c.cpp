// C3 batch c3c of golf_clean.exe: small ui/audio/video leaves (table/field readers and writers, four
// field-initialising constructors). Hand-written from the C2 transcriptions in re/analysis/<subsystem>/
// and the disassembly (`re/tools/asm2inline.py golf_clean.exe 0x<addr> --list`); not a copy of the matched
// source in re/match/*.cpp. Callees are reached through their original addresses (a hooked callee runs its
// own reimplementation), as shim/src/re/golf_course.cpp does.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused), as shim/src/re/golf_state.cpp does.
#include "hooks.h"

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// Callees, called through their original addresses.
typedef void*(__fastcall* ThisRet_t)(void* self, void* edx);           // ctor-style: returns `this`
typedef void(__fastcall* SetRate_t)(void* self, void* edx, int rate);  // 0x004846d0
typedef void(__fastcall* SetMode_t)(void* self, void* edx, int mode);  // 0x00484260
typedef void(__fastcall* Xform_t)(void* self, void* edx, int* x, int* y);

const ThisRet_t kBaseBaseCtor = reinterpret_cast<ThisRet_t>(0x00485260);  // Snd base-base ctor
const ThisRet_t kSndCtorBase = reinterpret_cast<ThisRet_t>(0x00484150);   // Snd::ctorBase (parent of ctorDerived)
const SetRate_t kSetRate = reinterpret_cast<SetRate_t>(0x004846d0);
const SetMode_t kSetMode = reinterpret_cast<SetMode_t>(0x00484260);
const Xform_t kToLocalInner = reinterpret_cast<Xform_t>(0x0047b170);
const Xform_t kToGlobalInner = reinterpret_cast<Xform_t>(0x0047b200);

// ---- ui writers (plain globals) -------------------------------------------------------------------

// 0x00490cc0  MsgBox::setColorA(a, b, c, d): stores the four args to the dwords at 0x004e4504, 0x004e4514,
// 0x004e4524, 0x004e4534 (stride 0x10; 0x00490ccc, 0x00490cd5, 0x00490cdb, 0x00490ce1).
typedef void(__cdecl* V4_t)(int, int, int, int);
V4_t SetColorA_orig;
void __cdecl SetColorA_re(int a, int b, int c, int d) {
    at<int>(0x004e4504) = a;
    at<int>(0x004e4514) = b;
    at<int>(0x004e4524) = c;
    at<int>(0x004e4534) = d;
}

// 0x00490d20  MsgBox::setColorB(a, b, c, d): stores the four args to the dwords at 0x004e4544, 0x004e4550,
// 0x004e455c, 0x004e4568 (stride 0xc; 0x00490d2c, 0x00490d35, 0x00490d3b, 0x00490d41).
V4_t SetColorB_orig;
void __cdecl SetColorB_re(int a, int b, int c, int d) {
    at<int>(0x004e4544) = a;
    at<int>(0x004e4550) = b;
    at<int>(0x004e455c) = c;
    at<int>(0x004e4568) = d;
}

// 0x00490c80  MsgBox::setButtonA(p, a, b, c): 3 for a null p (0x00490c88); otherwise when the dword at p+4 is
// non-zero remembers p in the dword at 0x0083b9b4 (0x00490c95), then stores a, b, c to the dwords at 0x0083b9b8,
// 0x0083b9bc, 0x0083b9c0 (0x00490ca6, 0x00490cab, 0x00490cb1) and returns 0.
typedef int(__cdecl* Sel_t)(void*, int, int, int);
Sel_t SetButtonA_orig;
int __cdecl SetButtonA_re(void* p, int a, int b, int c) {
    if (!p) return 3;
    if (at<int>(reinterpret_cast<unsigned>(p) + 4)) at<void*>(0x0083b9b4) = p;
    at<int>(0x0083b9b8) = a;
    at<int>(0x0083b9bc) = b;
    at<int>(0x0083b9c0) = c;
    return 0;
}

// ---- ui field writer / readers (thiscall) ---------------------------------------------------------

// 0x00476310  Widget_setQuad(a, b, c, d): stores the four args to the dwords at this+0x6c, this+0x7c, this+0x8c,
// this+0x9c (0x00476318, 0x0047631f, 0x00476326, 0x0047632c).
typedef void(__fastcall* SetQuad_t)(void*, void*, int, int, int, int);
SetQuad_t SetQuad_orig;
void __fastcall SetQuad_re(void* self, void*, int a, int b, int c, int d) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<int>(t + 0x6c) = a;
    at<int>(t + 0x7c) = b;
    at<int>(t + 0x8c) = c;
    at<int>(t + 0x9c) = d;
}

// 0x004801f0  Window::visible(): 0 when bit 0 of the byte at this+0xa0 is clear (0x004801f0); else, with a parent
// at this+0x130, 0 when the parent is not visible (recursion, 0x00480206); otherwise 1.
typedef int(__fastcall* Visible_t)(void*, void*);
Visible_t Visible_orig;
int __fastcall Visible_re(void* self, void*) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    if (!(at<unsigned char>(t + 0xa0) & 1)) return 0;
    void* parent = at<void*>(t + 0x130);
    if (parent && !Visible_re(parent, 0)) return 0;
    return 1;
}

// 0x00477580  Widget_value(): object at this+0x5c (lazily set to the dword at 0x0083ad44 when null, 0x00477587..
// 0x0047758c); with that object's dword at +8 (0x00477592) >= 0 returns its dword at +0x10 plus the +8 value
// (0x00477599), else its dword at +0xc (0x0047759f).
typedef int(__fastcall* Value_t)(void*, void*);
Value_t Value_orig;
int __fastcall Value_re(void* self, void*) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    if (!at<void*>(t + 0x5c)) at<void*>(t + 0x5c) = at<void*>(0x0083ad44);
    const unsigned o = reinterpret_cast<unsigned>(at<void*>(t + 0x5c));
    const int i = at<int>(o + 8);
    return i >= 0 ? at<int>(o + 0x10) + i : at<int>(o + 0xc);
}

// 0x0047b290  View::toLocal(x, y): runs the inner transform 0x0047b170 on *x, *y (this in ecx), then subtracts the
// view origin, the dwords at this+0x1ac and this+0x1b0 (0x0047b2a4, 0x0047b2b0).
typedef void(__fastcall* ToLocal_t)(void*, void*, int*, int*);
ToLocal_t ToLocal_orig;
void __fastcall ToLocal_re(void* self, void*, int* x, int* y) {
    kToLocalInner(self, 0, x, y);
    const unsigned t = reinterpret_cast<unsigned>(self);
    *x -= at<int>(t + 0x1ac);
    *y -= at<int>(t + 0x1b0);
}

// 0x0047b2d0  View::toGlobal(x, y): runs the inner transform 0x0047b200 on *x, *y (this in ecx), then adds the view
// origin, the dwords at this+0x1ac and this+0x1b0 (0x0047b2e4, 0x0047b2f0).
typedef void(__fastcall* ToGlobal_t)(void*, void*, int*, int*);
ToGlobal_t ToGlobal_orig;
void __fastcall ToGlobal_re(void* self, void*, int* x, int* y) {
    kToGlobalInner(self, 0, x, y);
    const unsigned t = reinterpret_cast<unsigned>(self);
    *x += at<int>(t + 0x1ac);
    *y += at<int>(t + 0x1b0);
}

// ---- field-initialising constructors (thiscall, return `this`) ------------------------------------

// 0x00488490  Cursor::ctor(): installs vtable 0x004ba7f4 at this+0 (0x00488494) and zeroes the dwords at this+4,
// this+0xc, this+0x10, this+8 (0x0048849a, 0x0048849d, 0x004884a0, 0x004884a3); returns this.
ThisRet_t CursorCtor_orig;
void* __fastcall CursorCtor_re(void* self, void*) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<unsigned>(t) = 0x004ba7f4;
    at<int>(t + 4) = 0;
    at<int>(t + 0xc) = 0;
    at<int>(t + 0x10) = 0;
    at<int>(t + 8) = 0;
    return self;
}

// 0x00487000  BinkPlayer::ctor(): installs vtable 0x004baea8 at this+0 (0x00487004) and zeroes the dwords at
// this+0xc, this+4, this+8, this+0x10, this+0x14, this+0x18 (0x0048700a..0x00487019); returns this.
ThisRet_t BinkCtor_orig;
void* __fastcall BinkCtor_re(void* self, void*) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<unsigned>(t) = 0x004baea8;
    at<int>(t + 0xc) = 0;
    at<int>(t + 4) = 0;
    at<int>(t + 8) = 0;
    at<int>(t + 0x10) = 0;
    at<int>(t + 0x14) = 0;
    at<int>(t + 0x18) = 0;
    return self;
}

// 0x00484150  Snd::ctorBase(): runs the base ctor 0x00485260 (0x0048416e), installs vtable 0x004bac68 at this+0
// (0x00484175), zeroes the dwords at this+0x48, this+0x4c, this+0x40, this+0x50 and this+0x44 (0x0048417d..
// 0x0048418f, the +0x44 clear is a store of 0 then of 0 & ~1), runs setRate 0x004846d0 with 1000 (0x0048419d;
// with this+0x40 == 0 it only stores 1000 at this+0x38), then zeroes the dword at this+0x54 (0x004841a6);
// returns this.
ThisRet_t SndCtorBase_orig;
void* __fastcall SndCtorBase_re(void* self, void*) {
    kBaseBaseCtor(self, 0);
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<unsigned>(t) = 0x004bac68;
    at<int>(t + 0x48) = 0;
    at<int>(t + 0x4c) = 0;
    at<int>(t + 0x40) = 0;
    at<int>(t + 0x50) = 0;
    at<int>(t + 0x44) = 0;
    kSetRate(self, 0, 1000);
    at<int>(t + 0x54) = 0;
    return self;
}

// 0x00484820  Snd::ctorDerived(): runs the parent ctor 0x00484150 (0x00484823), installs vtable 0x004bacf8 at
// this+0 (0x0048482b), zeroes the dword at this+0x58 (0x00484833), sets bit 2 (0x4) of the dword at this+0x44
// (0x0048483c), runs setMode 0x00484260 with 1 (0x00484844), then stores: 0 at this+0x5c (0x00484849), 0x3f800000
// (1.0f) at this+0x60 (0x00484850), 0x10 at this+0x3c (0x00484857), 0 at this+0x64 and this+0x68 (0x0048485e,
// 0x00484865); returns this.
ThisRet_t SndCtorDerived_orig;
void* __fastcall SndCtorDerived_re(void* self, void*) {
    kSndCtorBase(self, 0);
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<unsigned>(t) = 0x004bacf8;
    at<int>(t + 0x58) = 0;
    at<int>(t + 0x44) |= 4;
    kSetMode(self, 0, 1);
    at<int>(t + 0x5c) = 0;
    at<unsigned>(t + 0x60) = 0x3f800000;
    at<int>(t + 0x3c) = 0x10;
    at<int>(t + 0x64) = 0;
    at<int>(t + 0x68) = 0;
    return self;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00490cc0, MsgBox_setColorA, SetColorA_re, SetColorA_orig);
SG_HOOK("golf_clean.exe", 0x00490d20, MsgBox_setColorB, SetColorB_re, SetColorB_orig);
SG_HOOK("golf_clean.exe", 0x00490c80, MsgBox_setButtonA, SetButtonA_re, SetButtonA_orig);
SG_HOOK("golf_clean.exe", 0x00476310, Widget_setQuad, SetQuad_re, SetQuad_orig);
SG_HOOK("golf_clean.exe", 0x004801f0, Window_visible, Visible_re, Visible_orig);
SG_HOOK("golf_clean.exe", 0x00477580, Widget_value, Value_re, Value_orig);
SG_HOOK("golf_clean.exe", 0x0047b290, View_toLocal, ToLocal_re, ToLocal_orig);
SG_HOOK("golf_clean.exe", 0x0047b2d0, View_toGlobal, ToGlobal_re, ToGlobal_orig);
SG_HOOK("golf_clean.exe", 0x00488490, Cursor_ctor, CursorCtor_re, CursorCtor_orig);
SG_HOOK("golf_clean.exe", 0x00487000, BinkPlayer_ctor, BinkCtor_re, BinkCtor_orig);
SG_HOOK("golf_clean.exe", 0x00484150, Snd_ctorBase, SndCtorBase_re, SndCtorBase_orig);
SG_HOOK("golf_clean.exe", 0x00484820, Snd_ctorDerived, SndCtorDerived_re, SndCtorDerived_orig);
