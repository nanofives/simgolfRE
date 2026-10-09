// Batch c3ce: the jgld.dll Surface methods that are reached only through a slot of the Surface vtable at
// RVA 0x11d0b0 (VA 0x1011d0b0), which Surface::ctor 0x10007660, Surface::ctorOwner 0x10007970 and
// Surface::dtor 0x10007bf0 store at +0 of every Surface. SG_HOOK addresses are RVAs; every instruction cited in a
// comment is a VA (0x10000000 + RVA), the way the debug build's disassembly prints it. jgld.dll is LoadLibrary'd
// after the shim starts, so these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp)
// and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
//
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// Every body here is integer-only; not one contains an x87 instruction. The four drawing dispatchers at the end
// forward to the module's own primitives through their module addresses, so the A/B compares this code and not a
// copy of the primitive. The A/B drives both arms over synthetic Surface and Palette objects built on jgld's own
// vtables (re/frida/js/fixtures.d/c3ce.js), never the live display or a screen surface.
//
// Surface fields used below, each with the instruction that proves the offset:
//   +0x24  depth           slot +0xe4 0x1000ab30 returns this+0x24 (VA 0x1000ab50)
//   +0x40  row stride      slot +0xe0 0x1000aaf0 returns [this+0x40] (VA 0x1000ab10)
//   +0x44  clip RECT       slot +0xcc 0x1000a990 returns this+0x44 (VA 0x1000a9b0)
//   +0x54  bounds RECT     slot +0xd4 0x1000aa30 returns this+0x54 (VA 0x1000aa50)
//   +0x7c  palette         slot +0xe8 0x1000ab70 returns [this+0x7c] (VA 0x1000ab90)
//   +0x4b8 / +0x4bc / +0x4c4   the device context handed out, the stored one, the use count
//   +0x4c0 / +0x4cc / +0x4c8   the pixel base, the one handed out, its use count
//   +0x4d0 a spare dword written by 0x1000a5d0 and read by 0x1000a620
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address used below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

inline char* S(void* self) { return reinterpret_cast<char*>(self); }
inline int& I(void* self, unsigned off) { return *reinterpret_cast<int*>(S(self) + off); }
inline void*& P(void* self, unsigned off) { return *reinterpret_cast<void**>(S(self) + off); }

// A slot of the object's own vtable: [[this] + off].
inline void* slot(void* self, unsigned off) {
    return *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + off);
}

typedef void*(__fastcall* BitsSlot_t)(void*, void*);                 // +0x10
typedef void*(__fastcall* AddrSlot_t)(void*, void*, int, int);       // +0x0c
typedef int(__fastcall* SetClipSlot_t)(void*, void*, const RECT*);   // +0x34
typedef int*(__fastcall* FormatSlot_t)(void*, void*);                // +0xe4
typedef int(__fastcall* WHSlot_t)(void*, void*);                     // +0xd8 / +0xdc

// ---------------------------------------------------------------- the module's own helpers and primitives
typedef int(__cdecl* RectDim_t)(const RECT*);                                               // 0x9120 / 0x9160
typedef int(__fastcall* Fill_t)(void*, void*, int, int, int, int, int, int);                // 0xf880 / 0xafe0
typedef int(__fastcall* Fill2_t)(void*, void*, int, int, int, int, void*);                  // 0xf9e0
typedef int(__fastcall* Line_t)(void*, void*, int, int, int, int, unsigned);                // 0xfb70 / 0xb140
typedef int(__fastcall* Dither_t)(void*, void*, const RECT*, unsigned);                     // 0x11f80 / 0xe580

inline int jRectWidth(const RECT* r) { return reinterpret_cast<RectDim_t>(JgldBase() + 0x9120)(r); }
inline int jRectHeight(const RECT* r) { return reinterpret_cast<RectDim_t>(JgldBase() + 0x9160)(r); }

// --------------------------------------------------------------------------------- 0x00008640 acquireDC

// 0x00008640  Surface::acquireDC (VA 0x10008640): copies the stored device context at [this+0x4bc] into the
// handed-out slot [this+0x4b8] (VA 0x10008663 / 0x10008669), adds 1 to the use count at [this+0x4c4]
// (VA 0x10008672 / 0x10008678 / 0x1000867e) and returns the handed-out slot (VA 0x10008687). No branch, no call.
typedef void*(__fastcall* AcquireDC_t)(void*, void*);
AcquireDC_t AcquireDC_orig;
void* __fastcall AcquireDC_re(void* self, void*) {
    P(self, 0x4b8) = P(self, 0x4bc);
    I(self, 0x4c4) = I(self, 0x4c4) + 1;
    return P(self, 0x4b8);
}

// --------------------------------------------------------------------------------- 0x000086b0 releaseDC

// 0x000086b0  Surface::releaseDC(n) (VA 0x100086b0): subtracts n from the use count at [this+0x4c4]
// (VA 0x100086d0 / 0x100086d6 / 0x100086dc) and, when what is left is not greater than zero (VA 0x100086e5
// cmp, 0x100086ec jg), clears the handed-out device context at [this+0x4b8] (VA 0x100086f1) and the count
// (VA 0x100086fe). The epilogue leaves `this` in eax from VA 0x100086e2; nothing stores a result, so the
// registry compares memory only.
typedef void(__fastcall* ReleaseDC_t)(void*, void*, int);
ReleaseDC_t ReleaseDC_orig;
void __fastcall ReleaseDC_re(void* self, void*, int n) {
    I(self, 0x4c4) = I(self, 0x4c4) - n;
    if (I(self, 0x4c4) <= 0) {
        P(self, 0x4b8) = 0;
        I(self, 0x4c4) = 0;
    }
}

// --------------------------------------------------------------------------------- 0x00008730 acquireDC2

// 0x00008730  Surface::acquireDC2 (VA 0x10008730): copies the pixel base at [this+0x4c0] into the handed-out
// slot [this+0x4cc] (VA 0x10008753 / 0x10008759); when that is not zero (VA 0x10008762 cmp, 0x10008769 je) it
// adds 1 to the use count at [this+0x4c8] (VA 0x1000876e / 0x10008774 / 0x1000877a); it returns the handed-out
// slot either way (VA 0x10008783).
typedef void*(__fastcall* AcquireBits_t)(void*, void*);
AcquireBits_t AcquireBits_orig;
void* __fastcall AcquireBits_re(void* self, void*) {
    P(self, 0x4cc) = P(self, 0x4c0);
    if (P(self, 0x4cc) != 0)
        I(self, 0x4c8) = I(self, 0x4c8) + 1;
    return P(self, 0x4cc);
}

// --------------------------------------------------------------------------------- 0x000087b0 releaseDC2

// 0x000087b0  Surface::releaseDC2(n) (VA 0x100087b0): the same shape as releaseDC on the other pair of fields.
// n is subtracted from [this+0x4c8] (VA 0x100087d0 / 0x100087d6 / 0x100087dc) and, when the result is not
// greater than zero (VA 0x100087e5 cmp, 0x100087ec jg), [this+0x4cc] (VA 0x100087f1) and the count
// (VA 0x100087fe) are cleared.
typedef void(__fastcall* ReleaseBits_t)(void*, void*, int);
ReleaseBits_t ReleaseBits_orig;
void __fastcall ReleaseBits_re(void* self, void*, int n) {
    I(self, 0x4c8) = I(self, 0x4c8) - n;
    if (I(self, 0x4c8) <= 0) {
        P(self, 0x4cc) = 0;
        I(self, 0x4c8) = 0;
    }
}

// --------------------------------------------------------------------------------- 0x0000a5d0 setField4d0

// 0x0000a5d0  Surface::setField4d0(v) (VA 0x1000a5d0): stores the argument at [this+0x4d0] (VA 0x1000a5f3) and
// returns 0 (VA 0x1000a5f9 xor eax, eax). No branch, no call.
typedef int(__fastcall* SetF4d0_t)(void*, void*, int);
SetF4d0_t SetF4d0_orig;
int __fastcall SetF4d0_re(void* self, void*, int v) {
    I(self, 0x4d0) = v;
    return 0;
}

// --------------------------------------------------------------------------------- 0x0000a620 getField4d0

// 0x0000a620  Surface::getField4d0 (VA 0x1000a620): returns [this+0x4d0] (VA 0x1000a640).
typedef int(__fastcall* GetF4d0_t)(void*, void*);
GetF4d0_t GetF4d0_orig;
int __fastcall GetF4d0_re(void* self, void*) { return I(self, 0x4d0); }

// --------------------------------------------------------------------------------- 0x0000a930 getClip

// 0x0000a930  Surface::getClip(out) (VA 0x1000a930): copies the four dwords of the clip rectangle at this+0x44
// (VA 0x1000a950 add eax, 0x44) into the rectangle the caller passes, one dword at a time (VA 0x1000a956 /
// 0x1000a95a / 0x1000a960 / 0x1000a966). No branch; no test of the destination pointer.
typedef void(__fastcall* GetRect_t)(void*, void*, RECT*);
GetRect_t GetClip_orig;
void __fastcall GetClip_re(void* self, void*, RECT* out) {
    const int* src = reinterpret_cast<const int*>(S(self) + 0x44);
    int* dst = reinterpret_cast<int*>(out);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// --------------------------------------------------------------------------------- 0x0000a990 getClipRectPtr

// 0x0000a990  Surface::getClipRectPtr (VA 0x1000a990): returns this+0x44 (VA 0x1000a9b0 add eax, 0x44), the
// address of the clip rectangle itself. This is the slot (+0xcc) the drawing primitives fetch their clip
// rectangle through.
typedef void*(__fastcall* RectPtr_t)(void*, void*);
RectPtr_t GetClipPtr_orig;
void* __fastcall GetClipPtr_re(void* self, void*) { return S(self) + 0x44; }

// --------------------------------------------------------------------------------- 0x0000a9d0 getBounds

// 0x0000a9d0  Surface::getBounds(out) (VA 0x1000a9d0): the same four-dword copy as getClip, from the bounds
// rectangle at this+0x54 (VA 0x1000a9f0 add eax, 0x54; the stores at VA 0x1000a9f6 / 0x1000a9fa / 0x1000aa00 /
// 0x1000aa06).
GetRect_t GetBounds_orig;
void __fastcall GetBounds_re(void* self, void*, RECT* out) {
    const int* src = reinterpret_cast<const int*>(S(self) + 0x54);
    int* dst = reinterpret_cast<int*>(out);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// --------------------------------------------------------------------------------- 0x0000aa30 getBoundsRectPtr

// 0x0000aa30  Surface::getBoundsRectPtr (VA 0x1000aa30): returns this+0x54 (VA 0x1000aa50 add eax, 0x54).
RectPtr_t GetBoundsPtr_orig;
void* __fastcall GetBoundsPtr_re(void* self, void*) { return S(self) + 0x54; }

// --------------------------------------------------------------------------------- 0x0000aa70 width

// 0x0000aa70  Surface::width (VA 0x1000aa70): [this+0x5c] - [this+0x54] (VA 0x1000aa93 mov, 0x1000aa96 sub),
// the right edge of the bounds rectangle minus its left edge. The subtraction wraps like any 32-bit int.
typedef int(__fastcall* Dim_t)(void*, void*);
Dim_t Width_orig;
int __fastcall Width_re(void* self, void*) { return I(self, 0x5c) - I(self, 0x54); }

// --------------------------------------------------------------------------------- 0x0000aab0 height

// 0x0000aab0  Surface::height (VA 0x1000aab0): [this+0x60] - [this+0x58] (VA 0x1000aad3 mov, 0x1000aad6 sub),
// the bottom edge of the bounds rectangle minus its top edge.
Dim_t Height_orig;
int __fastcall Height_re(void* self, void*) { return I(self, 0x60) - I(self, 0x58); }

// --------------------------------------------------------------------------------- 0x0000aaf0 getField40

// 0x0000aaf0  Surface::getField40 (VA 0x1000aaf0): returns [this+0x40] (VA 0x1000ab10), the row stride the
// pixel-address switch multiplies the row number by.
Dim_t GetF40_orig;
int __fastcall GetF40_re(void* self, void*) { return I(self, 0x40); }

// --------------------------------------------------------------------------------- 0x0000ab30 getDepthPtr

// 0x0000ab30  Surface::getDepthPtr (VA 0x1000ab30): returns this+0x24 (VA 0x1000ab50 add eax, 0x24). This is the
// slot (+0xe4) the four dispatchers below call and then dereference to get the depth.
RectPtr_t GetDepthPtr_orig;
void* __fastcall GetDepthPtr_re(void* self, void*) { return S(self) + 0x24; }

// --------------------------------------------------------------------------------- 0x0000ab70 getField7c

// 0x0000ab70  Surface::getField7c (VA 0x1000ab70): returns [this+0x7c] (VA 0x1000ab90), the Palette pointer the
// 16-bit primitives read.
typedef void*(__fastcall* GetPtr_t)(void*, void*);
GetPtr_t GetF7c_orig;
void* __fastcall GetF7c_re(void* self, void*) { return P(self, 0x7c); }

// --------------------------------------------------------------------------------- 0x00008830 pixelAddr

// 0x00008830  Surface::pixelAddr(x, y) (VA 0x10008830): the address of pixel (x, y).
//   * slot +0xd8 (width) is called first and x compared against it: x >= width returns NULL
//     (VA 0x10008857 call, 0x10008864 cmp, 0x10008867 jge -> 0x10008885 xor eax, eax).
//   * slot +0xdc (height) is called only when the first test passes, and y >= height returns NULL
//     (VA 0x10008873 call, 0x10008880 cmp, 0x10008883 jl).
//   * slot +0x10 gives the base; NULL returns NULL (VA 0x10008896 call, 0x100088a3 cmp, 0x100088a7 jne).
//   * the depth at [this+0x24] (VA 0x100088b3) minus 8 (VA 0x100088bc) selects the arm through the jump table
//     at 0x1000894f with the index byte table at 0x10008963; above 0x18 (VA 0x100088c2 cmp, 0x100088c6 ja) the
//     function falls through to VA 0x1000893a and returns NULL.
//   * depth 8 (VA 0x100088da): base + x + y * [this+0x40].
//     depth 16 (VA 0x100088ee): base + x * 2 + y * [this+0x40] * 2 (two lea with a scale of 2).
//     depth 24 (VA 0x10008906): base + x * 3 + y * [this+0x40] * 3 (two imul by 3).
//     depth 32 (VA 0x10008922): base + x * 4 + y * [this+0x40] * 4 (two lea with a scale of 4).
// Neither x nor y is tested against the low edges and the row product is a signed 32-bit imul, so negative
// coordinates give an address below the base.
typedef void*(__fastcall* PixelAddr_t)(void*, void*, int, int);
PixelAddr_t PixelAddr_orig;
void* __fastcall PixelAddr_re(void* self, void*, int x, int y) {
    if (x >= reinterpret_cast<WHSlot_t>(slot(self, 0xd8))(self, 0)) return 0;
    if (y >= reinterpret_cast<WHSlot_t>(slot(self, 0xdc))(self, 0)) return 0;
    char* base = reinterpret_cast<char*>(reinterpret_cast<BitsSlot_t>(slot(self, 0x10))(self, 0));
    if (base == 0) return 0;
    const int pitch = I(self, 0x40);
    switch (I(self, 0x24)) {
    case 8:  return base + x + y * pitch;
    case 16: return base + x * 2 + y * pitch * 2;
    case 24: return base + x * 3 + y * pitch * 3;
    case 32: return base + x * 4 + y * pitch * 4;
    }
    return 0;
}

// --------------------------------------------------------------------------------- 0x0000a750 fwdVfn18

// 0x0000a750  Surface::fwdVfn18 (VA 0x1000a750): calls slot +0x10 of the object's own vtable with no argument
// (VA 0x1000a770 loads [this], 0x1000a777 calls [vtable+0x10]) and does not touch eax afterwards, so the value
// slot +0x10 returned is what reaches the `ret` at VA 0x1000a791 (the two __chkesp calls at 0x1000a77c and
// 0x1000a789 and the three pops preserve eax). On a Surface slot +0x10 is acquireDC2 0x10008730 above, so the
// observable effect is that function's writes to [this+0x4c8] and [this+0x4cc].
typedef void*(__fastcall* Fwd_t)(void*, void*);
Fwd_t FwdVfn18_orig;
void* __fastcall FwdVfn18_re(void* self, void*) {
    return reinterpret_cast<BitsSlot_t>(slot(self, 0x10))(self, 0);
}

// --------------------------------------------------------------------------------- 0x0000a7b0 fwdVfn20

// 0x0000a7b0  Surface::fwdVfn20 (VA 0x1000a7b0): byte for byte the same body as 0x1000a750 at a second address
// (VA 0x1000a7d0 loads [this], 0x1000a7d7 calls [vtable+0x10], ret at VA 0x1000a7f1). The two occupy slots
// +0x18 and +0x20 of the Surface vtable; which source spelling produced each is not determined.
Fwd_t FwdVfn20_orig;
void* __fastcall FwdVfn20_re(void* self, void*) {
    return reinterpret_cast<BitsSlot_t>(slot(self, 0x10))(self, 0);
}

// --------------------------------------------------------------------------------- 0x0000a810 call3a

// 0x0000a810  Surface::call3a(a, b) (VA 0x1000a810): pushes its second argument (VA 0x1000a82f / 0x1000a832)
// then its first (VA 0x1000a833 / 0x1000a836) and calls slot +0x0c of the object's own vtable (VA 0x1000a83a
// loads [this], 0x1000a83f calls [vtable+0x0c]); eax is not touched between that call and the `ret 8` at
// VA 0x1000a859. On a Surface slot +0x0c is pixelAddr 0x10008830 above.
typedef void*(__fastcall* Call3_t)(void*, void*, int, int);
Call3_t Call3a_orig;
void* __fastcall Call3a_re(void* self, void*, int a, int b) {
    return reinterpret_cast<AddrSlot_t>(slot(self, 0x0c))(self, 0, a, b);
}

// --------------------------------------------------------------------------------- 0x0000a870 call3b

// 0x0000a870  Surface::call3b (VA 0x1000a870): the same body at a second address (VA 0x1000a89a loads [this],
// 0x1000a89f calls [vtable+0x0c], `ret 8` at VA 0x1000a8b9). The two occupy slots +0x14 and +0x1c; the matched
// source re/match/jgld_surface.cpp records that the two spellings compile identically and that which one is in
// the original is not determined.
Call3_t Call3b_orig;
void* __fastcall Call3b_re(void* self, void*, int a, int b) {
    return reinterpret_cast<AddrSlot_t>(slot(self, 0x0c))(self, 0, a, b);
}

// --------------------------------------------------------------------------------- 0x0000a8d0 callBounds

// 0x0000a8d0  Surface::callBounds (VA 0x1000a8d0): pushes this+0x54, the bounds rectangle (VA 0x1000a8f0 add
// eax, 0x54; 0x1000a8f5 push) and calls slot +0x34 of the object's own vtable (VA 0x1000a8f9 loads [this],
// 0x1000a8fe calls [vtable+0x34]); eax is not touched before the `ret` at VA 0x1000a918. On a Surface slot
// +0x34 is setClip 0x100083c0, which intersects the bounds rectangle with itself into the clip rectangle at
// this+0x44 (VA 0x10008419), so the observable effect is the clip rectangle being set to the bounds.
typedef int(__fastcall* CallBounds_t)(void*, void*);
CallBounds_t CallBounds_orig;
int __fastcall CallBounds_re(void* self, void*) {
    return reinterpret_cast<SetClipSlot_t>(slot(self, 0x34))(self, 0, reinterpret_cast<const RECT*>(S(self) + 0x54));
}

// --------------------------------------------------------------------------------- 0x00009320 fillRect

// 0x00009320  Surface::fillRect(r, c1, c2) (VA 0x10009320): a NULL rectangle returns 0x10 (VA 0x1000933d cmp,
// 0x10009341 jne, 0x10009343). Otherwise slot +0xe4 is called and dereferenced for the depth (VA 0x10009357
// call, 0x10009364 mov) and compared with 8 (VA 0x10009369 cmp, 0x1000936d je) and with 0x10 (VA 0x1000936f
// cmp, 0x10009373 je); anything else returns 0x17 (VA 0x100093e9).
//   depth 8  -> fill8 0x1000f880  (call at VA 0x100093a9 through the thunk 0x1000147e)
//   depth 16 -> fill16 0x1000afe0 (call at VA 0x100093e2 through the thunk 0x100015d2)
// Both are given, in push order from VA 0x10009377 (and VA 0x100093b0), c2, c1, the rectangle's height
// (0x10009160, called at VA 0x10009383), its width (0x10009120, called at VA 0x10009390), r->top (VA 0x1000939c)
// and r->left (VA 0x100093a3), i.e. (left, top, width, height, c1, c2). The primitive's return value is the
// function's.
typedef int(__fastcall* FillRect_t)(void*, void*, const RECT*, int, int);
FillRect_t FillRect_orig;
int __fastcall FillRect_re(void* self, void*, const RECT* r, int c1, int c2) {
    if (r == 0) return 0x10;
    const int depth = *reinterpret_cast<FormatSlot_t>(slot(self, 0xe4))(self, 0);
    if (depth == 8)
        return reinterpret_cast<Fill_t>(JgldBase() + 0xf880)(
            self, 0, r->left, r->top, jRectWidth(r), jRectHeight(r), c1, c2);
    if (depth == 0x10)
        return reinterpret_cast<Fill_t>(JgldBase() + 0xafe0)(
            self, 0, r->left, r->top, jRectWidth(r), jRectHeight(r), c1, c2);
    return 0x17;
}

// --------------------------------------------------------------------------------- 0x00009440 fill2

// 0x00009440  Surface::fill2(r, table) (VA 0x10009440): slot +0xe4 is called and dereferenced for the depth
// (VA 0x10009467 call, 0x10009474 mov) and compared with 8 only (VA 0x10009479 cmp, 0x1000947d je); any other
// depth returns 0 (VA 0x100094aa xor eax, eax) WITHOUT reading the rectangle at all. At depth 8 the rectangle's
// four edges are pushed in the order bottom (VA 0x10009488), right (VA 0x1000948f), top (VA 0x10009496), left
// (VA 0x1000949d) under the table pointer (VA 0x10009481), so fill2_8 0x1000f9e0 is called (VA 0x100094a3,
// thunk 0x1000105a) as (left, top, right, bottom, table) and its return value is the function's.
typedef int(__fastcall* Fill2Disp_t)(void*, void*, const RECT*, void*);
Fill2Disp_t Fill2_orig;
int __fastcall Fill2_re(void* self, void*, const RECT* r, void* table) {
    const int depth = *reinterpret_cast<FormatSlot_t>(slot(self, 0xe4))(self, 0);
    if (depth == 8)
        return reinterpret_cast<Fill2_t>(JgldBase() + 0xf9e0)(
            self, 0, r->left, r->top, r->right, r->bottom, table);
    return 0;
}

// --------------------------------------------------------------------------------- 0x000094e0 drawLine

// 0x000094e0  Surface::drawLine(x1, y1, x2, y2, colour, unused) (VA 0x100094e0): slot +0xe4 is called and
// dereferenced for the depth (VA 0x10009507 call, 0x10009514 mov) and compared with 8 (VA 0x10009519 cmp,
// 0x1000951d je) and with 0x10 (VA 0x1000951f cmp, 0x10009523 je).
//   depth 8  -> line8 0x1000fb70  (call at VA 0x1000953e through the thunk 0x10001370)
//   depth 16 -> line16 0x1000b140 (call at VA 0x1000955c through the thunk 0x10001249)
// Only the first five arguments are forwarded (the pushes at VA 0x10009527..0x1000953a and
// 0x10009545..0x10009558 read [ebp+0x18] down to [ebp+8]); the sixth argument at [ebp+0x1c] is never read,
// although the `ret 0x18` at VA 0x10009573 pops it. The primitive's return value is discarded: the function
// always returns 0 (VA 0x10009561 xor eax, eax), so its evidence is the pixels it writes.
typedef int(__fastcall* DrawLine_t)(void*, void*, int, int, int, int, unsigned, int);
DrawLine_t DrawLine_orig;
int __fastcall DrawLine_re(void* self, void*, int x1, int y1, int x2, int y2, unsigned colour, int) {
    const int depth = *reinterpret_cast<FormatSlot_t>(slot(self, 0xe4))(self, 0);
    if (depth == 8)
        reinterpret_cast<Line_t>(JgldBase() + 0xfb70)(self, 0, x1, y1, x2, y2, colour);
    else if (depth == 0x10)
        reinterpret_cast<Line_t>(JgldBase() + 0xb140)(self, 0, x1, y1, x2, y2, colour);
    return 0;
}

// --------------------------------------------------------------------------------- 0x00009c50 ditherRectDispatch

// 0x00009c50  Surface::ditherRectDispatch(r, colour) (VA 0x10009c50): slot +0xe4 is called and dereferenced for
// the depth (VA 0x10009c77 call, 0x10009c84 mov) and compared with 8 (VA 0x10009c89 cmp, 0x10009c8d je) and with
// 0x10 (VA 0x10009c8f cmp, 0x10009c93 je); anything else returns 0x17 (VA 0x10009cbb).
//   depth 8  -> ditherRectC8 0x10011f80 (call at VA 0x10009ca2 through the thunk 0x10001951)
//   depth 16 -> ditherRect16 0x1000e580 (call at VA 0x10009cb4 through the thunk 0x10001a96)
// Both get the colour (VA 0x10009c97 / 0x10009ca9) and the rectangle (VA 0x10009c9b / 0x10009cad) unchanged and
// their return value is the function's. The rectangle is never tested for NULL here: the primitives take the
// bounds rectangle when it is NULL.
typedef int(__fastcall* DitherDisp_t)(void*, void*, const RECT*, unsigned);
DitherDisp_t DitherDisp_orig;
int __fastcall DitherDisp_re(void* self, void*, const RECT* r, unsigned colour) {
    const int depth = *reinterpret_cast<FormatSlot_t>(slot(self, 0xe4))(self, 0);
    if (depth == 8)
        return reinterpret_cast<Dither_t>(JgldBase() + 0x11f80)(self, 0, r, colour);
    if (depth == 0x10)
        return reinterpret_cast<Dither_t>(JgldBase() + 0xe580)(self, 0, r, colour);
    return 0x17;
}

}  // namespace

SG_HOOK("jgld.dll", 0x00008640, Surface_acquireDC, AcquireDC_re, AcquireDC_orig);
SG_HOOK("jgld.dll", 0x000086b0, Surface_releaseDC, ReleaseDC_re, ReleaseDC_orig);
SG_HOOK("jgld.dll", 0x00008730, Surface_acquireDC2, AcquireBits_re, AcquireBits_orig);
SG_HOOK("jgld.dll", 0x000087b0, Surface_releaseDC2, ReleaseBits_re, ReleaseBits_orig);
SG_HOOK("jgld.dll", 0x00008830, Surface_pixelAddr, PixelAddr_re, PixelAddr_orig);
SG_HOOK("jgld.dll", 0x00009320, Surface_fillRect, FillRect_re, FillRect_orig);
SG_HOOK("jgld.dll", 0x00009440, Surface_fill2, Fill2_re, Fill2_orig);
SG_HOOK("jgld.dll", 0x000094e0, Surface_drawLine, DrawLine_re, DrawLine_orig);
SG_HOOK("jgld.dll", 0x00009c50, Surface_ditherRectDispatch, DitherDisp_re, DitherDisp_orig);
SG_HOOK("jgld.dll", 0x0000a5d0, Surface_setField4d0, SetF4d0_re, SetF4d0_orig);
SG_HOOK("jgld.dll", 0x0000a620, Surface_getField4d0, GetF4d0_re, GetF4d0_orig);
SG_HOOK("jgld.dll", 0x0000a750, Surface_fwdVfn18, FwdVfn18_re, FwdVfn18_orig);
SG_HOOK("jgld.dll", 0x0000a7b0, Surface_fwdVfn20, FwdVfn20_re, FwdVfn20_orig);
SG_HOOK("jgld.dll", 0x0000a810, Surface_call3a, Call3a_re, Call3a_orig);
SG_HOOK("jgld.dll", 0x0000a870, Surface_call3b, Call3b_re, Call3b_orig);
SG_HOOK("jgld.dll", 0x0000a8d0, Surface_callBounds, CallBounds_re, CallBounds_orig);
SG_HOOK("jgld.dll", 0x0000a930, Surface_getClip, GetClip_re, GetClip_orig);
SG_HOOK("jgld.dll", 0x0000a990, Surface_getClipRectPtr, GetClipPtr_re, GetClipPtr_orig);
SG_HOOK("jgld.dll", 0x0000a9d0, Surface_getBounds, GetBounds_re, GetBounds_orig);
SG_HOOK("jgld.dll", 0x0000aa30, Surface_getBoundsRectPtr, GetBoundsPtr_re, GetBoundsPtr_orig);
SG_HOOK("jgld.dll", 0x0000aa70, Surface_width, Width_re, Width_orig);
SG_HOOK("jgld.dll", 0x0000aab0, Surface_height, Height_re, Height_orig);
SG_HOOK("jgld.dll", 0x0000aaf0, Surface_getField40, GetF40_re, GetF40_orig);
SG_HOOK("jgld.dll", 0x0000ab30, Surface_getDepthPtr, GetDepthPtr_re, GetDepthPtr_orig);
SG_HOOK("jgld.dll", 0x0000ab70, Surface_getField7c, GetF7c_re, GetF7c_orig);
