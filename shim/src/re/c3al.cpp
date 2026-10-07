// Batch c3al: reimplementations inside jgld.dll (SG_HOOK addresses are RVAs; the module's preferred-base VAs,
// which the disassembly below cites, are 0x10000000 + RVA; jgld.dll carries base relocations, so at runtime the
// real address of an absolute operand is GetModuleHandleA("jgld.dll") + RVA).
// jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook
// (SgHooksInstallPending, re/hooks.cpp), and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
//
// Nine of the thirteen functions come from the libpng 1.0.5 release source jgld.dll links
// (re/match/jgld_png_*.cpp, vendored in re/match/vendor/libpng-1.0.5); the bodies below are written from the
// disassembly of this build, not copied from that source. png_struct fields used here: transformations +0x70
// (dword), interlaced +0x123 (byte), bit_depth +0x127 (byte). png_info fields: valid +8 (dword), palette +0x10,
// num_palette +0x14 (word), srgb_intent +0x2c (byte), sig_bit +0x44 (5 bytes), x_offset +0x64, y_offset +0x68,
// offset_unit_type +0x6c (byte), hist +0x7c.
#include "hooks.h"
#include <string.h>

namespace {

inline unsigned char* at(void* p, unsigned off) { return (unsigned char*)p + off; }
inline unsigned int& dword(void* p, unsigned off) { return *(unsigned int*)at(p, off); }
inline unsigned char& byte(void* p, unsigned off) { return *at(p, off); }

// 0x00079410  png_get_uint_32 (jgld.dll, VA 0x10079410): reads four bytes of the argument buffer (0x10079428,
// 0x10079432, 0x1007943f, 0x1007944c) zero-extended one at a time, shifts them by 0x18, 0x10 and 8
// (0x1007942f, 0x1007943a, 0x10079447) and adds them (0x1007943d, 0x1007944a, 0x10079454): a big-endian
// 32-bit load. __cdecl, one stack argument, result in eax (0x10079459).
typedef unsigned int(__cdecl* GetUint32_t)(const unsigned char*);
GetUint32_t GetUint32_orig;
unsigned int __cdecl GetUint32_re(const unsigned char* buf) {
    unsigned int v = (unsigned int)buf[0] << 24;
    v += (unsigned int)buf[1] << 16;
    v += (unsigned int)buf[2] << 8;
    v += (unsigned int)buf[3];
    return v;
}

// 0x0006d600  png_set_bgr (jgld.dll, VA 0x1006d600): sets bit 0 of the png_struct's transformations word
// (or ecx, 1 at 0x1006d61e over the dword read at 0x1006d61b from +0x70, stored back at 0x1006d624). No test
// on the pointer, no other write. __cdecl, one stack argument, no return value.
typedef void(__cdecl* SetBgr_t)(void*);
SetBgr_t SetBgr_orig;
void __cdecl SetBgr_re(void* png_ptr) {
    dword(png_ptr, 0x70) |= 1;
}

// 0x0006d630  png_set_swap (jgld.dll, VA 0x1006d630): reads the byte at +0x127 zero-extended (0x1006d64d) and
// compares it with 0x10 (0x1006d653); only when it is exactly 0x10 does it set bit 4 of the transformations
// dword at +0x70 (or al, 0x10 at 0x1006d65e, stored at 0x1006d663). Any other value leaves the struct
// untouched (jne 0x1006d656). __cdecl, one stack argument, no return value.
typedef void(__cdecl* SetSwap_t)(void*);
SetSwap_t SetSwap_orig;
void __cdecl SetSwap_re(void* png_ptr) {
    if (byte(png_ptr, 0x127) == 0x10)
        dword(png_ptr, 0x70) |= 0x10;
}

// 0x0006d750  png_set_interlace_handling (jgld.dll, VA 0x1006d750): reads the byte at +0x123 zero-extended
// (0x1006d76d) and tests it (0x1006d773). Non-zero sets bit 1 of the transformations dword at +0x70
// (or al, 2 at 0x1006d77d, stored at 0x1006d782) and returns 7 (0x1006d785); zero returns 1 with no write
// (0x1006d78c). __cdecl, one stack argument, result in eax.
typedef int(__cdecl* SetInterlace_t)(void*);
SetInterlace_t SetInterlace_orig;
int __cdecl SetInterlace_re(void* png_ptr) {
    if (byte(png_ptr, 0x123) != 0) {
        dword(png_ptr, 0x70) |= 2;
        return 7;
    }
    return 1;
}

// 0x0006ee50  png_get_PLTE (jgld.dll, VA 0x1006ee50): four guards, each jumping to the common `xor eax, eax`
// at 0x1006eea7 (return 0): argument 1 null (0x1006ee68), argument 2 null (0x1006ee6e), bit 3 clear in the
// png_info valid dword at +8 (and ecx, 8 at 0x1006ee7a, test at 0x1006ee7d) and argument 3 null (0x1006ee81).
// Past them it stores the png_info palette pointer +0x10 through argument 3 (0x1006ee8d, 0x1006ee90) and the
// zero-extended word at +0x14 as a dword through argument 4 (0x1006ee97, 0x1006ee9e), then returns 8
// (0x1006eea0). Argument 4 is dereferenced without a null test. __cdecl, four stack arguments.
typedef unsigned int(__cdecl* GetPlte_t)(void*, void*, void**, int*);
GetPlte_t GetPlte_orig;
unsigned int __cdecl GetPlte_re(void* png_ptr, void* info_ptr, void** palette, int* num_palette) {
    if (!png_ptr || !info_ptr) return 0;
    if ((dword(info_ptr, 8) & 8) == 0) return 0;
    if (!palette) return 0;
    *palette = *(void**)at(info_ptr, 0x10);
    *num_palette = (int)(unsigned int)*(unsigned short*)at(info_ptr, 0x14);
    return 8;
}

// 0x0007dc20  png_set_hIST (jgld.dll, VA 0x1007dc20): returns at once when argument 1 is null (0x1007dc38) or
// argument 2 is null (0x1007dc3e), both reaching the jmp at 0x1007dc44. Otherwise it stores argument 3 into the
// png_info at +0x7c (0x1007dc4c) and sets bit 6 of the valid dword at +8 (or al, 0x40 at 0x1007dc55, stored at
// 0x1007dc5a). __cdecl, three stack arguments, no return value.
typedef void(__cdecl* SetHist_t)(void*, void*, void*);
SetHist_t SetHist_orig;
void __cdecl SetHist_re(void* png_ptr, void* info_ptr, void* hist) {
    if (!png_ptr || !info_ptr) return;
    *(void**)at(info_ptr, 0x7c) = hist;
    dword(info_ptr, 8) |= 0x40;
}

// 0x0007ddb0  png_set_oFFs (jgld.dll, VA 0x1007ddb0): same two null guards (0x1007ddc8, 0x1007ddce, jmp at
// 0x1007ddd4). Otherwise it stores argument 3 as a dword at +0x64 (0x1007dddc), argument 4 as a dword at +0x68
// (0x1007dde5) and the low byte of argument 5 at +0x6c (0x1007ddeb, 0x1007ddee), then sets bit 8 of the valid
// dword at +8 (or ch, 1 at 0x1007ddf7, stored at 0x1007ddfd). __cdecl, five stack arguments, no return value.
typedef void(__cdecl* SetOffs_t)(void*, void*, unsigned int, unsigned int, int);
SetOffs_t SetOffs_orig;
void __cdecl SetOffs_re(void* png_ptr, void* info_ptr, unsigned int x_offset, unsigned int y_offset,
                        int unit_type) {
    if (!png_ptr || !info_ptr) return;
    dword(info_ptr, 0x64) = x_offset;
    dword(info_ptr, 0x68) = y_offset;
    byte(info_ptr, 0x6c) = (unsigned char)unit_type;
    dword(info_ptr, 8) |= 0x100;
}

// 0x0007e0d0  png_set_sRGB (jgld.dll, VA 0x1007e0d0): same two null guards (0x1007e0e8, 0x1007e0ee, jmp at
// 0x1007e0f4). Otherwise it stores the low byte of argument 3 at +0x2c (0x1007e0f9, 0x1007e0fc) and sets bit 11
// of the valid dword at +8 (or ah, 8 at 0x1007e105, stored at 0x1007e10b). __cdecl, three stack arguments, no
// return value.
typedef void(__cdecl* SetSrgb_t)(void*, void*, int);
SetSrgb_t SetSrgb_orig;
void __cdecl SetSrgb_re(void* png_ptr, void* info_ptr, int intent) {
    if (!png_ptr || !info_ptr) return;
    byte(info_ptr, 0x2c) = (unsigned char)intent;
    dword(info_ptr, 8) |= 0x800;
}

// 0x0007e070  png_set_sBIT (jgld.dll, VA 0x1007e070): same two null guards (0x1007e088, 0x1007e08e, jmp at
// 0x1007e0b9). Otherwise it calls the CRT memcpy 0x1007f3a0 with the png_info plus 0x44 as destination
// (0x1007e09f), argument 3 as source and the length 5 (push at 0x1007e096..0x1007e0a3, cdecl cleanup at
// 0x1007e0a8), then sets bit 1 of the valid dword at +8 (or al, 2 at 0x1007e0b1, stored at 0x1007e0b6).
// __cdecl, three stack arguments, no return value.
typedef void(__cdecl* SetSbit_t)(void*, void*, const void*);
SetSbit_t SetSbit_orig;
void __cdecl SetSbit_re(void* png_ptr, void* info_ptr, const void* sig_bit) {
    if (!png_ptr || !info_ptr) return;
    memcpy(at(info_ptr, 0x44), sig_bit, 5);
    dword(info_ptr, 8) |= 2;
}

// jgld's circular doubly linked list (re/match/jgld_list.cpp): ListNode {vptr; prev +4; next +8; data +0xc;
// flag +0x10}, LinkedList {vptr; m_data +4; m_flag +8; m_count +0xc; m_head +0x10; m_cur +0x14; m_ordered +0x18}.
struct JNode { void* vt; JNode* prev; JNode* next; void* data; unsigned char flag; };
struct JList { void* vt; void* data; unsigned char flag; int count; JNode* head; JNode* cur; int ordered; };

// 0x00006d40  LinkedList::count (jgld.dll, VA 0x10006d40): returns the dword at this+0xc (read at 0x10006d60
// into eax, the return register). It reads nothing else and writes nothing. __thiscall (this in ecx, saved to
// the frame at 0x10006d5a), no stack arguments.
typedef int(__fastcall* ListCount_t)(JList*, void*);
ListCount_t ListCount_orig;
int __fastcall ListCount_re(JList* self, void*) {
    return self->count;
}

// 0x00007370  ListNode::ctor (jgld.dll, VA 0x10007370): stores the ListNode vtable (the relocated operand
// 0x1011d098 at 0x10007390, RVA 0x11d098), then argument 1 as the data pointer at +0xc (0x1000739c), the low
// byte of argument 2 as the flag at +0x10 (0x100073a2, 0x100073a5), 0 into next +8 (0x100073ab) and 0 into
// prev +4 (0x100073b5); it returns this (0x100073bf). __thiscall, two stack arguments (ret 8 at 0x100073c5).
typedef JNode*(__fastcall* NodeCtor_t)(JNode*, void*, void*, int);
NodeCtor_t NodeCtor_orig;
void* g_listNodeVtbl;
JNode* __fastcall NodeCtor_re(JNode* self, void*, void* data, int flag) {
    if (!g_listNodeVtbl) {
        HMODULE jgld = GetModuleHandleA("jgld.dll");
        g_listNodeVtbl = (char*)jgld + 0x0011d098;
    }
    self->vt = g_listNodeVtbl;
    self->data = data;
    self->flag = (unsigned char)flag;
    self->next = 0;
    self->prev = 0;
    return self;
}

// 0x000046f0  Matrix::setTranslation (jgld.dll, VA 0x100046f0): copies three dwords from the argument's +0, +4
// and +8 (0x10004713, 0x1000471e, 0x1000472a) into this+0x34, this+0x38 and this+0x3c (0x10004715, 0x10004721,
// 0x1000472d), the translation column of the 4x4 matrix. The values are moved as dwords, with no floating point
// instruction. __thiscall, one stack argument (ret 4 at 0x10004736), no return value.
typedef void(__fastcall* MatSetTrans_t)(void*, void*, const void*);
MatSetTrans_t MatSetTrans_orig;
void __fastcall MatSetTrans_re(void* self, void*, const void* v) {
    dword(self, 0x34) = *(const unsigned int*)at((void*)v, 0);
    dword(self, 0x38) = *(const unsigned int*)at((void*)v, 4);
    dword(self, 0x3c) = *(const unsigned int*)at((void*)v, 8);
}

// 0x0000e540  blueOf (jgld.dll, VA 0x1000e540): shifts the argument left by 3 (0x1000e55b) and masks the result
// with 0xf8 (0x1000e55e): the five-bit blue channel of a 16-bit pixel scaled to eight bits. Leaves bits above
// 0xf8 clear, so only bits 0..4 of the argument reach the result. __cdecl, one stack argument, result in eax.
typedef int(__cdecl* BlueOf_t)(int);
BlueOf_t BlueOf_orig;
int __cdecl BlueOf_re(int pixel) {
    return (pixel << 3) & 0xf8;
}

}  // namespace

SG_HOOK("jgld.dll", 0x00079410, png_get_uint_32, GetUint32_re, GetUint32_orig);
SG_HOOK("jgld.dll", 0x0006d600, png_set_bgr, SetBgr_re, SetBgr_orig);
SG_HOOK("jgld.dll", 0x0006d630, png_set_swap, SetSwap_re, SetSwap_orig);
SG_HOOK("jgld.dll", 0x0006d750, png_set_interlace_handling, SetInterlace_re, SetInterlace_orig);
SG_HOOK("jgld.dll", 0x0006ee50, png_get_PLTE, GetPlte_re, GetPlte_orig);
SG_HOOK("jgld.dll", 0x0007dc20, png_set_hIST, SetHist_re, SetHist_orig);
SG_HOOK("jgld.dll", 0x0007ddb0, png_set_oFFs, SetOffs_re, SetOffs_orig);
SG_HOOK("jgld.dll", 0x0007e0d0, png_set_sRGB, SetSrgb_re, SetSrgb_orig);
SG_HOOK("jgld.dll", 0x0007e070, png_set_sBIT, SetSbit_re, SetSbit_orig);
SG_HOOK("jgld.dll", 0x00006d40, LinkedList_count, ListCount_re, ListCount_orig);
SG_HOOK("jgld.dll", 0x00007370, ListNode_ctor, NodeCtor_re, NodeCtor_orig);
SG_HOOK("jgld.dll", 0x000046f0, Matrix_setTranslation, MatSetTrans_re, MatSetTrans_orig);
SG_HOOK("jgld.dll", 0x0000e540, blueOf, BlueOf_re, BlueOf_orig);
