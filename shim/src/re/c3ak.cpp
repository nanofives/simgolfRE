// Batch c3ak: reimplementations inside jgld.dll (SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA,
// and every instruction cited in the comments is a VA, as the debug build's disassembly prints them).
// jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook
// (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
#include "hooks.h"

namespace {

// ----------------------------------------------------------------- RECT helpers (jgld.dll, util)
// All four take a 4-dword RECT {left +0, top +4, right +8, bottom +0xc}; __cdecl, caller-cleaned (`ret` with 0).

// 0x00009120  rectWidth(r): loads the argument twice (0x10009138, 0x1000913b), reads [eax+8] (0x1000913e) and
// subtracts [ecx] (0x10009141), leaving right - left in eax. No branch, no write.
typedef int(__cdecl* RectWidth_t)(const int*);
RectWidth_t RectWidth_orig;
int __cdecl RectWidth_re(const int* r) {
    return (int)((unsigned)r[2] - (unsigned)r[0]);   // `sub` wraps; unsigned keeps that defined in C++
}

// 0x00009160  rectHeight(r): reads [eax+0xc] (0x1000917e) and subtracts [ecx+4] (0x10009181), leaving
// bottom - top in eax. No branch, no write.
typedef int(__cdecl* RectHeight_t)(const int*);
RectHeight_t RectHeight_orig;
int __cdecl RectHeight_re(const int* r) {
    return (int)((unsigned)r[3] - (unsigned)r[1]);
}

// 0x00008360  rectFromXYWH(r, x, y, w, h): stores x at [r] (0x1000837e) and y at [r+4] (0x10008386), then
// x + w at [r+8] (the add is 0x1000838c, the store 0x10008392) and y + h at [r+0xc] (add 0x10008398, store
// 0x1000839e). The two sums are computed from the arguments, not from the fields just written. No branch.
typedef void(__cdecl* RectFromXYWH_t)(int*, int, int, int, int);
RectFromXYWH_t RectFromXYWH_orig;
void __cdecl RectFromXYWH_re(int* r, int x, int y, int w, int h) {
    r[0] = x;
    r[1] = y;
    r[2] = (int)((unsigned)x + (unsigned)w);        // `add` wraps; unsigned keeps that defined in C++
    r[3] = (int)((unsigned)y + (unsigned)h);
}

// 0x00010390  setRect4(r, a, b, c, d): copies the four arguments into [r], [r+4], [r+8], [r+0xc] in that order
// (0x100103ae, 0x100103b6, 0x100103bf, 0x100103c8). No arithmetic, no branch.
typedef void(__cdecl* SetRect4_t)(int*, int, int, int, int);
SetRect4_t SetRect4_orig;
void __cdecl SetRect4_re(int* r, int a, int b, int c, int d) {
    r[0] = a;
    r[1] = b;
    r[2] = c;
    r[3] = d;
}

// ----------------------------------------------------------------- Sprite / Surface field accessors (thiscall)

// 0x000180e0  Sprite::getFlag18(): reads the dword at [this+0x18] (0x10018100) and masks bit 0 (`and eax,1` at
// 0x10018103). __thiscall with no stack argument (`ret` at 0x1001810c), this spilled to [ebp-4] at 0x100180fa.
typedef unsigned(__fastcall* GetFlag18_t)(void* self, void* edx);
GetFlag18_t GetFlag18_orig;
unsigned __fastcall GetFlag18_re(void* self, void*) {
    return *reinterpret_cast<const unsigned*>(static_cast<char*>(self) + 0x18) & 1u;
}

// 0x0000aeb0  setField28(v): stores the single stack argument (read at 0x1000aed0) into [this+0x28]
// (0x1000aed3). __thiscall, one stack argument (`ret 4` at 0x1000aedc). No branch, no return value.
typedef void(__fastcall* SetField28_t)(void* self, void* edx, void* v);
SetField28_t SetField28_orig;
void __fastcall SetField28_re(void* self, void*, void* v) {
    *reinterpret_cast<void**>(static_cast<char*>(self) + 0x28) = v;
}

// ----------------------------------------------------------------- libpng 1.0.5 (linked into jgld.dll)

// 0x000793b0  png_get_int_32(buf): big-endian 32-bit load. Each byte is zero-extended into eax (xor eax,eax +
// mov al,[edx+n]) and added into ecx shifted by 24, 16, 8 and 0 (0x100793cf, 0x100793da, 0x100793e7, 0x100793f4);
// the sum is spilled to [ebp-4] (0x100793f6) and returned. No branch, no write to the buffer.
typedef int(__cdecl* PngGetInt32_t)(const unsigned char*);
PngGetInt32_t PngGetInt32_orig;
int __cdecl PngGetInt32_re(const unsigned char* buf) {
    unsigned v = (unsigned)buf[0] << 24;
    v += (unsigned)buf[1] << 16;
    v += (unsigned)buf[2] << 8;
    v += buf[3];
    return (int)v;
}

// 0x00079470  png_get_uint_16(buf): big-endian 16-bit load. buf[0] is zero-extended into ecx (0x1007948d) and
// shifted left 8 (0x1007948f), buf[1] is added (0x1007949a), the sum is stored as a word at [ebp-4] (0x1007949c)
// and returned in ax (0x100794a0). eax's high half is 0 there, because the register last held a zero-extended
// byte, so the full return value is the 16-bit result. No branch.
typedef int(__cdecl* PngGetUint16_t)(const unsigned char*);
PngGetUint16_t PngGetUint16_orig;
int __cdecl PngGetUint16_re(const unsigned char* buf) {
    unsigned v = ((unsigned)buf[0] << 8) + buf[1];
    return (int)(v & 0xffffu);
}

// 0x0007dfc0  png_set_pHYs(png_ptr, info_ptr, res_x, res_y, unit_type): returns at once when either pointer is
// null (cmp [ebp+8],0 / je at 0x1007dfd8-0x1007dfdc, cmp [ebp+0xc],0 / jne at 0x1007dfde-0x1007dfe2, both landing
// on the jmp to the epilogue at 0x1007dfe4). Otherwise it writes res_x at [info+0x70] (0x1007dfec), res_y at
// [info+0x74] (0x1007dff5), the low byte of unit_type at [info+0x78] (0x1007dffe), and sets bit 7 of the dword at
// [info+8] (`or cl,0x80` at 0x1007e007, stored back as a dword at 0x1007e00d). png_ptr is only tested, never read.
typedef void(__cdecl* PngSetPhys_t)(void*, void*, unsigned, unsigned, int);
PngSetPhys_t PngSetPhys_orig;
void __cdecl PngSetPhys_re(void* png_ptr, void* info_ptr, unsigned res_x, unsigned res_y, int unit_type) {
    if (png_ptr == 0 || info_ptr == 0) return;
    char* info = static_cast<char*>(info_ptr);
    *reinterpret_cast<unsigned*>(info + 0x70) = res_x;
    *reinterpret_cast<unsigned*>(info + 0x74) = res_y;
    *reinterpret_cast<unsigned char*>(info + 0x78) = (unsigned char)unit_type;
    *reinterpret_cast<unsigned*>(info + 8) |= 0x80u;
}

// 0x0007e020  png_set_PLTE(png_ptr, info_ptr, palette, num_palette): the same two null tests (0x1007e038-
// 0x1007e042, jmp to the epilogue at 0x1007e044). Otherwise it stores the palette pointer at [info+0x10]
// (0x1007e04c), the low 16 bits of num_palette as a word at [info+0x14] (`mov ax,[ebp+0x14]` at 0x1007e052,
// stored at 0x1007e056) and sets bit 3 of the dword at [info+8] (`or edx,8` at 0x1007e060, stored 0x1007e066).
// png_ptr is only tested. The word store leaves [info+0x16] untouched.
typedef void(__cdecl* PngSetPlte_t)(void*, void*, void*, int);
PngSetPlte_t PngSetPlte_orig;
void __cdecl PngSetPlte_re(void* png_ptr, void* info_ptr, void* palette, int num_palette) {
    if (png_ptr == 0 || info_ptr == 0) return;
    char* info = static_cast<char*>(info_ptr);
    *reinterpret_cast<void**>(info + 0x10) = palette;
    *reinterpret_cast<unsigned short*>(info + 0x14) = (unsigned short)num_palette;
    *reinterpret_cast<unsigned*>(info + 8) |= 8u;
}

// ----------------------------------------------------------------- Random (jgld.dll, util)
// A 4-byte seed at [this]; the generator is the ANSI C LCG. Both bodies are __thiscall.

// 0x00007530  Random::next(): seed = seed * 0x41c64e6d + 0x3039 (imul at 0x10007552, add at 0x10007558) stored
// back to [this] (0x10007561); the new seed is shifted right 16 (0x10007568) and masked with 0x7fff
// (0x1000756b), written to the low dword of a 64-bit local whose high dword is zeroed (0x10007571, 0x10007574),
// loaded with `fild qword` (0x1000757b) and divided by the double constant at 0x1011d0a0 (`fdiv` at 0x1000757e),
// which holds 32768.0. The quotient is returned in st(0). Both operands are exact in binary, so the division is
// exact and the SSE2 result here is bit-identical. No branch; the seed write is the whole state change.
typedef double(__fastcall* RandomNext_t)(void* self, void* edx);
RandomNext_t RandomNext_orig;
double __fastcall RandomNext_re(void* self, void*) {
    unsigned* seed = static_cast<unsigned*>(self);
    *seed = *seed * 0x41c64e6du + 0x3039u;
    unsigned v = (*seed >> 16) & 0x7fffu;
    return (double)v / 32768.0;
}

// 0x000075b0  Random::range(n): masks n to 16 bits (`and eax,0xffff` at 0x100075d0), loads it as a SIGNED 32-bit
// integer with `fild dword` (0x100075d8) and spills it as a double (0x100075db) BEFORE calling Random::next
// (ecx = this, call at 0x100075e1 through the incremental-link thunk 0x10001091); the result is multiplied by
// that double (`fmul` at 0x100075e6) and truncated to int by __ftol (call at 0x100075e9, 64-bit truncation
// toward zero, low dword kept). __thiscall, one stack argument (`ret 4` at 0x100075fe). The mask makes the
// multiplier 0..65535, so the product is in [0, 65535] and the truncation never overflows.
typedef int(__fastcall* RandomRange_t)(void* self, void* edx, int n);
RandomRange_t RandomRange_orig;
int __fastcall RandomRange_re(void* self, void*, int n) {
    static RandomNext_t next = 0;
    if (!next) {                                  // the DLL is relocatable: resolve 0x7530 against its real base
        HMODULE m = GetModuleHandleA("jgld.dll");
        if (!m) return 0;
        next = reinterpret_cast<RandomNext_t>((char*)m + 0x7530);
    }
    double d = (double)(int)(n & 0xffff);
    double r = next(self, 0);                     // advances the seed at [this]
    return (int)(long long)(r * d);
}

}  // namespace

SG_HOOK("jgld.dll", 0x00009120, rectWidth, RectWidth_re, RectWidth_orig);
SG_HOOK("jgld.dll", 0x00009160, rectHeight, RectHeight_re, RectHeight_orig);
SG_HOOK("jgld.dll", 0x00008360, rectFromXYWH, RectFromXYWH_re, RectFromXYWH_orig);
SG_HOOK("jgld.dll", 0x00010390, setRect4, SetRect4_re, SetRect4_orig);
SG_HOOK("jgld.dll", 0x000180e0, Sprite_getFlag18, GetFlag18_re, GetFlag18_orig);
SG_HOOK("jgld.dll", 0x0000aeb0, setField28, SetField28_re, SetField28_orig);
SG_HOOK("jgld.dll", 0x000793b0, png_get_int_32, PngGetInt32_re, PngGetInt32_orig);
SG_HOOK("jgld.dll", 0x00079470, png_get_uint_16, PngGetUint16_re, PngGetUint16_orig);
SG_HOOK("jgld.dll", 0x0007dfc0, png_set_pHYs, PngSetPhys_re, PngSetPhys_orig);
SG_HOOK("jgld.dll", 0x0007e020, png_set_PLTE, PngSetPlte_re, PngSetPlte_orig);
SG_HOOK("jgld.dll", 0x00007530, jgld_Random_next, RandomNext_re, RandomNext_orig);
SG_HOOK("jgld.dll", 0x000075b0, jgld_Random_range, RandomRange_re, RandomRange_orig);
