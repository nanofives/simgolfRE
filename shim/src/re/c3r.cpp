// C3 batch c3r (2026-10-07) of golf_clean.exe: util leaves not reached by the test scenarios but statically called:
// integer helpers (boundIndex, the libjpeg rounding pair), string / int-pair helpers, three list readers, the two
// bucket-flag tests of the HashTable at 0x00487770, the routing-map overlay lookup and two libjpeg object helpers.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2
// transcriptions; each body cites the address of every global, offset and branch it uses. __thiscall is emulated with
// __fastcall (ecx = this, edx unused); the two __fastcall list helpers take their object in ecx.
#include "hooks.h"

namespace {

template <typename T> T field(const void* p, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(p) + off);
}
template <typename T> void setField(void* p, unsigned off, T v) {
    *reinterpret_cast<T*>(reinterpret_cast<char*>(p) + off) = v;
}
// 32-bit wrapping add (`lea` keeps the low dword).
inline int add(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }

// 0x00463170  boundIndex(i, n): returns i when i < n (signed `jl` at 0x0046317a) and -1 otherwise (`or eax, -1` at
// 0x0046317c). cdecl leaf.
typedef int(__cdecl* BoundIndex_t)(int, int);
BoundIndex_t BoundIndex_orig;
int __cdecl BoundIndex_re(int i, int n) {
    if (i < n) return i;
    return -1;
}

// 0x004b04c0  jdiv_round_up(a, b): (a + b - 1) / b with a signed, truncating division (`lea eax, [eax+ecx-1]` at
// 0x004b04c8, `cdq` / `idiv ecx` at 0x004b04cc..0x004b04cd). The sum wraps at 32 bits. cdecl leaf.
typedef int(__cdecl* JDivRoundUp_t)(int, int);
JDivRoundUp_t JDivRoundUp_orig;
int __cdecl JDivRoundUp_re(int a, int b) {
    return add(add(a, b), -1) / b;
}

// 0x004b04d0  jround_up(a, b): c = a + b - 1 (`lea ecx, [eax+esi-1]` at 0x004b04d9), returns c minus the signed
// remainder of c / b (`cdq` / `idiv esi` at 0x004b04df..0x004b04e0, `sub eax, edx` at 0x004b04e5). cdecl leaf.
typedef int(__cdecl* JRoundUp_t)(int, int);
JRoundUp_t JRoundUp_orig;
int __cdecl JRoundUp_re(int a, int b) {
    const int c = add(add(a, b), -1);
    return add(c, -(c % b));
}

// 0x004935b0  reverseFindChar(base, p, ch): searches backwards from p for the byte ch and returns its address, or NULL.
// Returns NULL at once when base is NULL (0x004935ba), p is NULL (0x004935c1) or p == base (0x004935c9). Otherwise it
// compares the bytes at p, p-1, ... (0x004935ce / 0x004935d0 je), p - base of them (count in ecx, `dec ecx` / `jne` at
// 0x004935d3..0x004935d4), so the byte at base itself is never compared; no match returns NULL (0x004935d6). Only the
// low byte of the third argument is read (0x004935cb). cdecl leaf.
typedef char*(__cdecl* ReverseFindChar_t)(char*, char*, char);
ReverseFindChar_t ReverseFindChar_orig;
char* __cdecl ReverseFindChar_re(char* base, char* p, char ch) {
    if (base == 0 || p == 0) return 0;
    unsigned count = static_cast<unsigned>(p - base);
    if (count == 0) return 0;
    for (; count != 0; count--, p--) {
        if (*p == ch) return p;
    }
    return 0;
}

// 0x00493580  swapInts(a, b): exchanges the dwords *a and *b with three XORs (0x00493591..0x004935a4); when a == b
// (0x00493588 cmp / 0x0049358a je) nothing is written, so a single cell is not zeroed. cdecl leaf, no result.
typedef void(__cdecl* SwapInts_t)(int*, int*);
SwapInts_t SwapInts_orig;
void __cdecl SwapInts_re(int* a, int* b) {
    if (a == b) return;
    const int t = *a;
    *a = *b;
    *b = t;
}

// 0x004925f0  appendNewline(s): finds the terminating NUL of s (`repne scasb` at 0x004925fc) and writes '\n' (0xa) there
// (0x00492604) and a new NUL after it (0x00492607). cdecl leaf, no result.
typedef void(__cdecl* AppendNewline_t)(char*);
AppendNewline_t AppendNewline_orig;
void __cdecl AppendNewline_re(char* s) {
    while (*s != 0) s++;
    s[0] = '\n';
    s[1] = 0;
}

// 0x00402160  listIterValue(it): returns the value at +8 of the current node (node pointer at it+0xc, 0x00402167 /
// 0x0040216a) when the list pointer at it+8 is non-null (0x00402163 test / 0x00402165 je), and 0 otherwise
// (0x0040216e). __fastcall (it in ecx) leaf.
typedef unsigned(__fastcall* ListIterValue_t)(void*);
ListIterValue_t ListIterValue_orig;
unsigned __fastcall ListIterValue_re(void* it) {
    if (field<void*>(it, 8) == 0) return 0;
    return field<unsigned>(field<void*>(it, 0xc), 8);
}

// 0x00402130  listIterNext(it): returns 0 when the list pointer at it+8 is null (0x00402133 test / 0x00402135 jne).
// Otherwise it moves the current node (it+0xc) to that node's next pointer at +0xc (0x0040213a..0x00402143),
// increments the index at it+0x14 (0x00402149, stored at 0x0040214c) and resets it to 0 when it equals the count at
// it+0x10 (`cmp` / `jne` at 0x0040214a..0x0040214f, store at 0x00402151), then returns the new node's value at +8
// (0x00402158..0x0040215b). __fastcall (it in ecx) leaf.
typedef unsigned(__fastcall* ListIterNext_t)(void*);
ListIterNext_t ListIterNext_orig;
unsigned __fastcall ListIterNext_re(void* it) {
    if (field<void*>(it, 8) == 0) return 0;
    void* const next = field<void*>(field<void*>(it, 0xc), 0xc);
    setField<void*>(it, 0xc, next);
    int index = add(field<int>(it, 0x14), 1);
    setField<int>(it, 0x14, index);
    if (index == field<int>(it, 0x10)) setField<int>(it, 0x14, 0);
    return field<unsigned>(next, 8);
}

// 0x004a4ea0  readListNode(this, outFirst, outSecond): the node is the pointer at this+4. When it is null the function
// returns 0 (it returns that null pointer, 0x004a4ea3 test / 0x004a4ea5 jne / 0x004a4ea7). Otherwise, when outSecond is
// non-null (0x004a4eb0 je) it receives node[1] (+4, 0x004a4eb2..0x004a4eb5); when outFirst is non-null (0x004a4ebd je)
// it receives node[0] (0x004a4ebf..0x004a4ec4); the result is node[2] (+8, 0x004a4ec6..0x004a4ec9). outSecond is written
// before outFirst. thiscall, ret 8.
typedef unsigned(__fastcall* ReadListNode_t)(void*, void*, unsigned*, unsigned*);
ReadListNode_t ReadListNode_orig;
unsigned __fastcall ReadListNode_re(void* self, void*, unsigned* outFirst, unsigned* outSecond) {
    const unsigned* const node = field<const unsigned*>(self, 4);
    if (node == 0) return 0;
    if (outSecond != 0) *outSecond = node[1];
    if (outFirst != 0) *outFirst = node[0];
    return node[2];
}

// 0x00487770  HashTable::bucketEmpty(this, i): true when i is above 15 (unsigned `jbe` at 0x00487777, `mov al, 1` at
// 0x00487779); otherwise true when the flag byte of bucket i, at this + 0x24 + i * 0x1c (`lea edx, [eax*8]` / `sub
// edx, eax` / `[ecx+edx*4+0x24]` at 0x0048777e..0x00487787), is zero (`sete` at 0x0048778d). thiscall, ret 4; the
// result is the byte in al.
typedef unsigned char(__fastcall* BucketFlag_t)(void*, void*, unsigned);
BucketFlag_t BucketEmpty_orig;
unsigned char __fastcall BucketEmpty_re(void* self, void*, unsigned i) {
    if (i > 15) return 1;
    return field<unsigned char>(self, 0x24 + i * 0x1c) == 0 ? 1 : 0;
}

// 0x004877a0  HashTable::bucketSet(this, i): true when i is above 15 (unsigned `jbe` at 0x004877a7, `mov al, 1` at
// 0x004877a9); otherwise true when the byte at this + 0x25 + i * 0x1c (0x004877ae..0x004877b7) is non-zero (`setne` at
// 0x004877bd). thiscall, ret 4; the result is the byte in al.
BucketFlag_t BucketSet_orig;
unsigned char __fastcall BucketSet_re(void* self, void*, unsigned i) {
    if (i > 15) return 1;
    return field<unsigned char>(self, 0x25 + i * 0x1c) != 0 ? 1 : 0;
}

// 0x00456bb0  overlayCharAt(x, y): k = the signed tile-type byte at 0x005722e8 + x * 50 + y (`lea` x5 twice, then
// `movsx eax, byte [ecx+eax*2+0x5722e8]` at 0x00456bb8..0x00456bbe); returns 3 when the signed byte at 0x00578372 +
// k * 0x30 (0x00456bc6..0x00456bce) is greater than 0 (`setg` at 0x00456bd6) and 2 otherwise (`add eax, 2` at
// 0x00456bd9). cdecl leaf; the result is a char.
typedef char(__cdecl* OverlayCharAt_t)(int, int);
OverlayCharAt_t OverlayCharAt_orig;
char __cdecl OverlayCharAt_re(int x, int y) {
    const int k = *reinterpret_cast<const signed char*>(0x005722e8 + add(x * 50, y));
    const signed char flag = *reinterpret_cast<const signed char*>(0x00578372 + k * 0x30);
    return static_cast<char>(flag > 0 ? 3 : 2);
}

// libjpeg 6a object helpers. cinfo+4 is the memory manager; its method at +0x24 is free_pool(cinfo, pool) and at
// +0x28 self_destruct(cinfo), both cdecl (the caller pops: `add esp, 8` at 0x004afa71, `add esp, 4` at 0x004afaa3).
typedef void(__cdecl* FreePool_t)(void*, int);
typedef void(__cdecl* SelfDestruct_t)(void*);

// 0x004afa60  jpeg_abort(cinfo): calls the memory manager's free_pool (+0x24 of the table at cinfo+4) with (cinfo, 1)
// (0x004afa65..0x004afa6b), then stores 200 at cinfo+0x10 when the dword at cinfo+0xc is non-zero and 100 when it is
// zero (`neg` / `sbb` / `and 0x64` / `add 0x64` at 0x004afa74..0x004afa7b, store at 0x004afa7e). cdecl, no result.
typedef void(__cdecl* JpegAbort_t)(void*);
JpegAbort_t JpegAbort_orig;
void __cdecl JpegAbort_re(void* cinfo) {
    void* const mem = field<void*>(cinfo, 4);
    reinterpret_cast<FreePool_t>(field<void*>(mem, 0x24))(cinfo, 1);
    setField<int>(cinfo, 0x10, field<int>(cinfo, 0xc) != 0 ? 200 : 100);
}

// 0x004afa90  jpeg_destroy(cinfo): when the memory-manager pointer at cinfo+4 is non-null (0x004afa9b cmp /
// 0x004afa9d je) calls its self_destruct (+0x28) with (cinfo) (0x004afa9f..0x004afaa0); then stores 0 at cinfo+4
// (0x004afaa6) and at cinfo+0x10 (0x004afaa9) in both cases. cdecl, no result.
typedef void(__cdecl* JpegDestroy_t)(void*);
JpegDestroy_t JpegDestroy_orig;
void __cdecl JpegDestroy_re(void* cinfo) {
    void* const mem = field<void*>(cinfo, 4);
    if (mem != 0) reinterpret_cast<SelfDestruct_t>(field<void*>(mem, 0x28))(cinfo);
    setField<void*>(cinfo, 4, 0);
    setField<int>(cinfo, 0x10, 0);
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00463170, c3r_boundIndex, BoundIndex_re, BoundIndex_orig);
SG_HOOK("golf_clean.exe", 0x004b04c0, c3r_jdiv_round_up, JDivRoundUp_re, JDivRoundUp_orig);
SG_HOOK("golf_clean.exe", 0x004b04d0, c3r_jround_up, JRoundUp_re, JRoundUp_orig);
SG_HOOK("golf_clean.exe", 0x004935b0, c3r_reverseFindChar, ReverseFindChar_re, ReverseFindChar_orig);
SG_HOOK("golf_clean.exe", 0x00493580, c3r_swapInts, SwapInts_re, SwapInts_orig);
SG_HOOK("golf_clean.exe", 0x004925f0, c3r_appendNewline, AppendNewline_re, AppendNewline_orig);
SG_HOOK("golf_clean.exe", 0x00402160, c3r_listIterValue, ListIterValue_re, ListIterValue_orig);
SG_HOOK("golf_clean.exe", 0x00402130, c3r_listIterNext, ListIterNext_re, ListIterNext_orig);
SG_HOOK("golf_clean.exe", 0x004a4ea0, c3r_readListNode, ReadListNode_re, ReadListNode_orig);
SG_HOOK("golf_clean.exe", 0x00487770, c3r_bucketEmpty, BucketEmpty_re, BucketEmpty_orig);
SG_HOOK("golf_clean.exe", 0x004877a0, c3r_bucketSet, BucketSet_re, BucketSet_orig);
SG_HOOK("golf_clean.exe", 0x00456bb0, c3r_overlayCharAt, OverlayCharAt_re, OverlayCharAt_orig);
SG_HOOK("golf_clean.exe", 0x004afa60, c3r_jpeg_abort, JpegAbort_re, JpegAbort_orig);
SG_HOOK("golf_clean.exe", 0x004afa90, c3r_jpeg_destroy, JpegDestroy_re, JpegDestroy_orig);
