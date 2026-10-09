// Batch c3cn (2026-10-09): the heap stand-in pilot (re/frida/js/heapstandin.js).
//
// Part 1 - the functions this batch brings to C3 WITHOUT new code, because their reimplementations
// already exist and only their heap branch was unreachable (re/c3_hold.tsv, rounds 14 and 20). They
// must be promoted from the file that holds the body:
//   jgld.dll 0x000681a0 Array_Rec14::add         shim/src/re/c3bb.cpp
//   jgld.dll 0x00068400 Array_Rec94::add         shim/src/re/c3bb.cpp
//   jgld.dll 0x00003600 Matrix::deletingDtor     shim/src/re/c3cd.cpp
//   jgld.dll 0x00003670 Transform::deletingDtor  shim/src/re/c3cd.cpp
//   jgld.dll 0x00006d80 LinkedList::deletingDtor shim/src/re/c3cd.cpp
//   jgld.dll 0x000073e0 ListNode::deletingDtor   shim/src/re/c3cd.cpp
// Only their new A/B keys are this batch's (re/frida/registry.d/c3cn.py, js/fixtures.d/c3cn.js).
//
// Part 2 - jgld.dll's allocation and release leaves, written here. Earlier rounds dropped every one of
// them for the heap alone (log/c3/c3ak_notes.txt, c3al, c3ao, c3at, c3ax, c3bq, c3bw, c3cb, c3cd).
// Under the stand-in the CRT entry points they call - malloc 0x1007fb40, free 0x100806d0, calloc
// 0x1007ffa0, operator new 0x1007f0f0, operator delete 0x1007e6d0 - hand both arms the same block and
// record every release, so the bodies above them are ordinary compared code.
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp). __thiscall is emulated with
// __fastcall: ecx = this, edx unused.
//
// Nothing live in the running jgld.dll is touched: no Display, Surface, Palette, Font, png_struct or
// z_stream of the game's, and no module global. Every routine that stays original (the CRT malloc,
// free, calloc, memset and memcpy, png_error and the "Out of Memory" string) is reached at its own
// module address, so each comparison is of this code and not of a CRT.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }

// The routines that stay original.
typedef void*(__cdecl* Malloc_t)(unsigned);                       // 0x1007fb40  malloc
typedef void(__cdecl* Free_t)(void*);                             // 0x100806d0  free
typedef void*(__cdecl* Calloc_t)(unsigned, unsigned);             // 0x1007ffa0  calloc
typedef void(__cdecl* OperatorDelete_t)(void*);                   // 0x1007e6d0  operator delete
typedef void*(__cdecl* Memset_t)(void*, int, unsigned);           // 0x1007e7c0  memset
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);   // 0x1007f3a0  memcpy
typedef void(__cdecl* PngError_t)(void*, const char*);            // 0x10078fe0  png_error

Malloc_t JgldMalloc() { return reinterpret_cast<Malloc_t>(JgldVA(0x1007fb40)); }
Free_t JgldFree() { return reinterpret_cast<Free_t>(JgldVA(0x100806d0)); }
Calloc_t JgldCalloc() { return reinterpret_cast<Calloc_t>(JgldVA(0x1007ffa0)); }
OperatorDelete_t JgldOperatorDelete() { return reinterpret_cast<OperatorDelete_t>(JgldVA(0x1007e6d0)); }
Memset_t JgldMemset() { return reinterpret_cast<Memset_t>(JgldVA(0x1007e7c0)); }
Memcpy_t JgldMemcpy() { return reinterpret_cast<Memcpy_t>(JgldVA(0x1007f3a0)); }
PngError_t JgldPngError() { return reinterpret_cast<PngError_t>(JgldVA(0x10078fe0)); }

// ----------------------------------------------------------------- jgld's own allocation wrappers

// 0x00014a40  jalloc
// void* __cdecl jalloc(unsigned size): one call, `push [ebp+8]` / `call 0x1007fb40` at 0x10014a5b-
// 0x10014a5c, i.e. the CRT malloc, whose result is left in eax for the `ret` at 0x10014a74. No branch,
// no test of the result, no store. Called by Sprite::create8 0x1001b690, Sprite::copyFrom 0x1001a0e0,
// Font::loadFile 0x10068ae0 among others.
typedef void*(__cdecl* Jalloc_t)(unsigned);
Jalloc_t Jalloc_orig;
void* __cdecl Jalloc_re(unsigned size) {
    return JgldMalloc()(size);
}

// 0x00014a90  jfree
// int __cdecl jfree(void* p): the null guard at 0x10014aa8 (`cmp dword ptr [ebp+8], 0` / `jne
// 0x10014ab2`) skips the call; otherwise the CRT free 0x100806d0 is called at 0x10014ab6. Both paths
// put 0 in eax (`xor eax, eax` at 0x10014aae and 0x10014abe), so the return value is always 0 and the
// evidence of this body is the free itself.
typedef int(__cdecl* Jfree_t)(void*);
Jfree_t Jfree_orig;
int __cdecl Jfree_re(void* p) {
    if (p == 0)
        return 0;
    JgldFree()(p);
    return 0;
}

// ------------------------------------------------------------- libpng 1.0.5's memory layer (__cdecl)

// 0x00078e60  png_malloc
// void* __cdecl png_malloc(png_struct* png_ptr, unsigned size): returns NULL when either the structure
// pointer or the size is zero (`cmp dword ptr [ebp+8], 0` / `je 0x10078e84` at 0x10078e78 and `cmp
// dword ptr [ebp+0xc], 0` / `jne 0x10078e88` at 0x10078e7e), otherwise calls the CRT malloc 0x1007fb40
// at 0x10078e8c and keeps the block in [ebp-4]. A null block goes to png_error 0x10078fe0 with the
// message at 0x1011e06c (0x10078e9d-0x10078ea6); png_error does not return. The block is the return
// value (0x10078eae). png_ptr is never dereferenced here.
typedef void*(__cdecl* PngMalloc_t)(void*, unsigned);
PngMalloc_t PngMalloc_orig;
void* __cdecl PngMalloc_re(void* png_ptr, unsigned size) {
    if (png_ptr == 0 || size == 0)
        return 0;
    void* p = JgldMalloc()(size);
    if (p == 0)
        JgldPngError()(png_ptr, reinterpret_cast<const char*>(JgldVA(0x1011e06c)));
    return p;
}

// 0x00078ed0  png_free
// void __cdecl png_free(png_struct* png_ptr, void* ptr): the same two-sided guard (0x10078ee8 and
// 0x10078eee) returns without doing anything when either pointer is null; otherwise the CRT free
// 0x100806d0 is called on the second argument at 0x10078efa. Nothing is written, nothing is returned.
typedef void(__cdecl* PngFree_t)(void*, void*);
PngFree_t PngFree_orig;
void __cdecl PngFree_re(void* png_ptr, void* ptr) {
    if (png_ptr == 0 || ptr == 0)
        return;
    JgldFree()(ptr);
}

// 0x000786b0  png_zalloc
// void* __cdecl png_zalloc(png_struct* png_ptr, unsigned items, unsigned size): zlib's allocator seen
// from libpng. The byte count is items * size (`imul` at 0x100786cb) and the block comes from
// png_malloc 0x10078e60 (0x100786da). The block is then cleared with the CRT memset 0x1007e7c0 in ONE
// call of `num` bytes when num <= 0x8000 (the UNSIGNED `jbe 0x10078721` at 0x100786ec, call at
// 0x1007872b) and in TWO calls otherwise - 0x8000 bytes at the start (0x100786f9) and num - 0x8000 at
// block + 0x8000 (0x10078717). The block is returned (0x10078733). A zero count makes png_malloc
// return NULL and the single memset writes nothing.
typedef void*(__cdecl* PngZalloc_t)(void*, unsigned, unsigned);
PngZalloc_t PngZalloc_orig;
void* __cdecl PngZalloc_re(void* png_ptr, unsigned items, unsigned size) {
    unsigned num = items * size;
    void* p = reinterpret_cast<PngMalloc_t>(JgldVA(0x10078e60))(png_ptr, num);
    if (num > 0x8000) {
        JgldMemset()(p, 0, 0x8000);
        JgldMemset()(static_cast<char*>(p) + 0x8000, 0, num - 0x8000);
    } else {
        JgldMemset()(p, 0, num);
    }
    return p;
}

// 0x00078750  png_zfree
// void __cdecl png_zfree(png_struct* png_ptr, void* ptr): one call, png_free 0x10078ed0 with the two
// arguments in the same order (`push [ebp+0xc]` then `push [ebp+8]` at 0x10078768-0x1007876f, call at
// 0x10078770). No branch of its own.
typedef void(__cdecl* PngZfree_t)(void*, void*);
PngZfree_t PngZfree_orig;
void __cdecl PngZfree_re(void* png_ptr, void* ptr) {
    reinterpret_cast<PngFree_t>(JgldVA(0x10078ed0))(png_ptr, ptr);
}

// 0x00078da0  png_create_struct
// void* __cdecl png_create_struct(int type): the size is chosen by two equality tests - 0xb8 bytes for
// type 2 (`cmp dword ptr [ebp+8], 2` / `jne` at 0x10078db8, store at 0x10078dbe) and 0x21c for type 1
// (`cmp ... 1` at 0x10078dc7, store at 0x10078dcd); any other type returns NULL at once (0x10078dd6).
// The block comes from the CRT malloc 0x1007fb40 (0x10078dde) and, when it is not null (`je
// 0x10078e01` at 0x10078ded), is zeroed with memset 0x1007e7c0 over the same size (0x10078df9). The
// block is the return value (0x10078e01).
typedef void*(__cdecl* PngCreateStruct_t)(int);
PngCreateStruct_t PngCreateStruct_orig;
void* __cdecl PngCreateStruct_re(int type) {
    unsigned size;
    if (type == 2)
        size = 0xb8;
    else if (type == 1)
        size = 0x21c;
    else
        return 0;
    void* p = JgldMalloc()(size);
    if (p != 0)
        JgldMemset()(p, 0, size);
    return p;
}

// 0x00078e20  png_destroy_struct
// void __cdecl png_destroy_struct(void* p): the null guard at 0x10078e38 (`cmp dword ptr [ebp+8], 0` /
// `je 0x10078e4a`) skips the CRT free 0x100806d0 called at 0x10078e42. Nothing is returned.
typedef void(__cdecl* PngDestroyStruct_t)(void*);
PngDestroyStruct_t PngDestroyStruct_orig;
void __cdecl PngDestroyStruct_re(void* p) {
    if (p == 0)
        return;
    JgldFree()(p);
}

// ---------------------------------------------------------------- zlib 1.0.2's allocator (__cdecl)

// 0x0009e680  zcalloc
// void* __cdecl zcalloc(void* opaque, unsigned items, unsigned size): when opaque is not null (`cmp
// dword ptr [ebp+8], 0` / `je 0x1009e6ac` at 0x1009e698) the body adds size - size to items
// (0x1009e69e-0x1009e6a9: `mov eax,[ebp+0x10]; sub eax,[ebp+0x10]` is zero by construction), then
// calls the CRT calloc 0x1007ffa0 with (items, size) at 0x1009e6b4 and returns its result. The branch
// therefore changes nothing; it is written out because the disassembly shows it.
typedef void*(__cdecl* Zcalloc_t)(void*, unsigned, unsigned);
Zcalloc_t Zcalloc_orig;
void* __cdecl Zcalloc_re(void* opaque, unsigned items, unsigned size) {
    if (opaque != 0)
        items += size - size;
    return JgldCalloc()(items, size);
}

// 0x0009e6d0  zcfree
// void __cdecl zcfree(void* opaque, void* ptr): one call, the CRT free 0x100806d0 on the SECOND
// argument at 0x1009e6ec (`mov eax, dword ptr [ebp+0xc]` at 0x1009e6e8). opaque is not read and there
// is no null guard, so a null pointer reaches free.
typedef void(__cdecl* Zcfree_t)(void*, void*);
Zcfree_t Zcfree_orig;
void __cdecl Zcfree_re(void*, void* ptr) {
    JgldFree()(ptr);
}

// ------------------------------------------------------------------------- Array<T>::clear (three)

// Array<T> (re/match/jgld_font.cpp, c3bb.cpp): vtable at +0, buffer at +4, capacity at +8, count at
// +0xc, growth step at +0x10.
struct ArrayHdr {
    void* vtbl;
    void* m_data;
    int m_cap;
    int m_count;
    int m_grow;
};

// The three instances have byte-identical bodies and differ only in their addresses:
//   Array_e0::clear    0x00068000   guard 0x10068020   operator delete 0x10068033   stores 0x1006803b+
//   Array_Rec14::clear 0x00068100   guard 0x10068120   operator delete 0x10068133   stores 0x1006813b+
//   Array_Rec94::clear 0x00068360   guard 0x10068380   operator delete 0x10068393   stores 0x1006839b+
// Each releases the buffer at +4 through operator delete 0x1007e6d0 when it is not null, then writes
// buffer 0, capacity 0, count 0 and growth step 1 (the growth step is set to ONE, not zero).
typedef void(__fastcall* Clear_t)(void*, void*);
void ArrayClear(void* self) {
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    if (a->m_data != 0)
        JgldOperatorDelete()(a->m_data);
    a->m_data = 0;
    a->m_cap = 0;
    a->m_count = 0;
    a->m_grow = 1;
}

// 0x00068000  Array_e0::clear
Clear_t ArrayE0Clear_orig;
void __fastcall ArrayE0Clear_re(void* self, void*) { ArrayClear(self); }

// 0x00068100  Array_Rec14::clear
Clear_t ArrayRec14Clear_orig;
void __fastcall ArrayRec14Clear_re(void* self, void*) { ArrayClear(self); }

// 0x00068360  Array_Rec94::clear
Clear_t ArrayRec94Clear_orig;
void __fastcall ArrayRec94Clear_re(void* self, void*) { ArrayClear(self); }

// 0x00068570  Array_e0::clearRet
// int __thiscall Array<e0>::clearRet(): calls Array_e0::clear 0x10068000 through the incremental-link
// thunk 0x100019f1 (`call` at 0x10068590, `this` already in ecx) and returns 0 (`xor eax, eax` at
// 0x10068595). The return value is therefore constant and the evidence is the clear's own effect.
typedef int(__fastcall* ClearRet_t)(void*, void*);
ClearRet_t ArrayE0ClearRet_orig;
int __fastcall ArrayE0ClearRet_re(void* self, void*) {
    reinterpret_cast<Clear_t>(JgldVA(0x10068000))(self, 0);
    return 0;
}

// 0x00068610  Array_Rec94::clearRet
// The same body one instance further on: the thunk at 0x10068630 is 0x100019ba, which reaches
// Array_Rec94::clear 0x10068360, and `xor eax, eax` at 0x10068635 returns 0.
ClearRet_t ArrayRec94ClearRet_orig;
int __fastcall ArrayRec94ClearRet_re(void* self, void*) {
    reinterpret_cast<Clear_t>(JgldVA(0x10068360))(self, 0);
    return 0;
}

// --------------------------------------------------------------------------------- Sprite::copyFrom

// 0x0001a0e0  Sprite::copyFrom
// Sprite* __thiscall Sprite::copyFrom(Sprite* src): makes `this` a copy of another sprite.
// It first calls slot 2 of its OWN vtable (`mov edx,[eax]` / `call dword ptr [edx+8]` at 0x1001a100-
// 0x1001a107, `this` in ecx, no argument), which is Sprite::release, then copies eight dwords from the
// source in this order: +0x10 (0x1001a11d), +0x18 (0x1001a129), +0x34 (0x1001a135), +0x30 (0x1001a141),
// +0x2c (0x1001a14d), +0x28 (0x1001a159), +0x20 (0x1001a165) and +0x24 (0x1001a171).
// The payload is then taken from jalloc 0x10014a40 (through the thunk 0x100017c6 at 0x1001a17e) for
// `this->+0x24` bytes, stored at +0x14 (0x1001a189), and filled from the source's +0x14 with the CRT
// memcpy 0x1007f3a0 over the same count (0x1001a1a1).
// When the SOURCE's +0x1c is not null (`cmp dword ptr [edx+0x1c], 0` / `je 0x1001a1ea` at 0x1001a1ac)
// a second block of `this->+0x34 * 4` bytes (`shl ecx, 2` at 0x1001a1b8) comes from jalloc
// (0x1001a1bc), is stored at +0x1c (0x1001a1c7) and is filled from the source's +0x1c over the same
// count (0x1001a1e2). `this` is the return value (`ret 4` at 0x1001a1fd).
struct SpriteHdr {
    void* vtbl;         // +0x00
    int pad04[3];       // +0x04 .. +0x0c
    int m_10;           // +0x10
    void* m_14;         // +0x14  payload
    int m_18;           // +0x18
    void* m_1c;         // +0x1c  row table
    int m_20;
    int m_24;           // +0x24  payload byte count
    int m_28;
    int m_2c;
    int m_30;
    int m_34;           // +0x34  row count
};
typedef void*(__fastcall* CopyFrom_t)(void*, void*, void*);
CopyFrom_t SpriteCopyFrom_orig;
void* __fastcall SpriteCopyFrom_re(void* self, void*, void* src) {
    SpriteHdr* d = static_cast<SpriteHdr*>(self);
    SpriteHdr* s = static_cast<SpriteHdr*>(src);
    void** vtbl = *reinterpret_cast<void***>(self);          // [[this]+8] is slot 2, not [this+8]
    reinterpret_cast<Clear_t>(vtbl[2])(self, 0);
    d->m_10 = s->m_10;
    d->m_18 = s->m_18;
    d->m_34 = s->m_34;
    d->m_30 = s->m_30;
    d->m_2c = s->m_2c;
    d->m_28 = s->m_28;
    d->m_20 = s->m_20;
    d->m_24 = s->m_24;
    d->m_14 = reinterpret_cast<Jalloc_t>(JgldVA(0x10014a40))(d->m_24);
    JgldMemcpy()(d->m_14, s->m_14, d->m_24);
    if (s->m_1c != 0) {
        d->m_1c = reinterpret_cast<Jalloc_t>(JgldVA(0x10014a40))(d->m_34 * 4);
        JgldMemcpy()(d->m_1c, s->m_1c, d->m_34 * 4);
    }
    return self;
}

// ===================================================================================================
// Part 3 - golf_clean.exe's release leaves. The exe has no relocations, so its addresses are literal
// constants (CLAUDE.md, matching decompilation). Its allocation entry points are
//   0x004a504f malloc (`push [0x00840b18]; push size; call 0x004a5061`)
//   0x004a57f1 operator new (`push 1; push size; call 0x004a5061`)
//   0x004a5007 free; operator delete 0x004a4ffc is a one-line wrapper that tail-calls it (0x004a5000),
//              so replacing free covers both.
// The stand-in replaces those three for the length of a key (fixture c3cn_exe_heap).

typedef void(__cdecl* ExeFree_t)(void*);                          // 0x004a5007  free
typedef void*(__cdecl* ExeMalloc_t)(unsigned);                    // 0x004a504f  malloc
typedef void(__cdecl* ExeOperatorDelete_t)(void*);                // 0x004a4ffc  operator delete

const ExeFree_t kExeFree = reinterpret_cast<ExeFree_t>(0x004a5007);
const ExeMalloc_t kExeMalloc = reinterpret_cast<ExeMalloc_t>(0x004a504f);
const ExeOperatorDelete_t kExeOperatorDelete = reinterpret_cast<ExeOperatorDelete_t>(0x004a4ffc);

// Fields are addressed by offset: these classes have no names of their own in the binary and the
// offsets are what the instructions cited above each body show.
template <class T>
T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }

typedef void(__fastcall* ExeThis_t)(void*, void*);

// 0x004747e0  Buf_releaseData
// void __thiscall Buf::releaseData(): releases the buffer at +8 through free 0x004a5007 when it is not
// null (`mov eax,[esi+8]` / `test eax,eax` / `je 0x004747fa` at 0x004747e3-0x004747e8, call at
// 0x004747eb) and clears the field (0x004747f3), then runs Buf_init 0x00474780 on the same object
// (0x004747fc), which zeroes the byte at +4 and the dwords at +0xc, +8, +0x14 and +0x10.
ExeThis_t BufReleaseData_orig;
void __fastcall BufReleaseData_re(void* self, void*) {
    void* data = At<void*>(self, 8);
    if (data != 0) {
        kExeFree(data);
        At<void*>(self, 8) = 0;
    }
    reinterpret_cast<ExeThis_t>(0x00474780)(self, 0);
}

// 0x00474810  Buf_dtor
// void __thiscall Buf::~Buf(): stores the Buf vtable 0x004ba84c at +0 (0x00474810) and tail-jumps to
// Buf_releaseData 0x004747e0 (0x00474816). The store is observable: Buf_releaseData never writes +0.
ExeThis_t BufDtor_orig;
void __fastcall BufDtor_re(void* self, void*) {
    At<void*>(self, 0) = reinterpret_cast<void*>(0x004ba84c);
    reinterpret_cast<ExeThis_t>(0x004747e0)(self, 0);
}

// 0x0049d0e0  strFree
// int __thiscall strFree(): runs Buf_releaseData 0x004747e0 on `this` (0x0049d0e3), clears the dword
// at +0x18 (0x0049d0e8) and returns 0 (`xor eax, eax` at 0x0049d0ef). The vtable at +0 is NOT written,
// which is what separates it from Buf_dtor.
typedef int(__fastcall* StrFree_t)(void*, void*);
StrFree_t StrFree_orig;
int __fastcall StrFree_re(void* self, void*) {
    reinterpret_cast<ExeThis_t>(0x004747e0)(self, 0);
    At<int>(self, 0x18) = 0;
    return 0;
}

// 0x004882f0  KeyTable::dtor
// void __thiscall KeyTable::~KeyTable(): stores the KeyTable vtable 0x004bb088 at +0 (0x004882f2),
// clears the dword at +0x104 (0x004882f8) and the BYTE at +4 (0x004882fe), then tail-jumps to Buf_dtor
// 0x00474810 with `this` advanced by 0x108 (`add ecx, 0x108` at 0x00488301), i.e. the embedded Buf
// starts at +0x108. The zero all three stores use comes from the `xor eax, eax` at 0x004882f0.
ExeThis_t KeyTableDtor_orig;
void __fastcall KeyTableDtor_re(void* self, void*) {
    At<void*>(self, 0) = reinterpret_cast<void*>(0x004bb088);
    At<int>(self, 0x104) = 0;
    At<char>(self, 4) = 0;
    reinterpret_cast<ExeThis_t>(0x00474810)(static_cast<char*>(self) + 0x108, 0);
}

// 0x004a01a0  freeLoggerBuf
// void __thiscall freeLoggerBuf(): reads the buffer at +4 (0x004a01a3), stores the vtable 0x004bc034
// at +0 (0x004a01a6), and when the buffer is not null (`je 0x004a01c0` at 0x004a01ae) frees it through
// free 0x004a5007 (0x004a01b1) and clears the field (0x004a01b9).
ExeThis_t FreeLoggerBuf_orig;
void __fastcall FreeLoggerBuf_re(void* self, void*) {
    void* buf = At<void*>(self, 4);
    At<void*>(self, 0) = reinterpret_cast<void*>(0x004bc034);
    if (buf != 0) {
        kExeFree(buf);
        At<void*>(self, 4) = 0;
    }
}

// 0x004b0470  jpeg_free_large
// void __cdecl jpeg_free_large(j_common_ptr cinfo, void* object, unsigned size): frees the SECOND
// argument (`mov eax, dword ptr [esp+8]` at 0x004b0470, call 0x004a5007 at 0x004b0475). The first
// argument is not read and there is no null guard, so a null pointer reaches free.
typedef void(__cdecl* JpegFreeLarge_t)(void*, void*);
JpegFreeLarge_t JpegFreeLarge_orig;
void __cdecl JpegFreeLarge_re(void*, void* object) {
    kExeFree(object);
}

// 0x004b0460  jpeg_get_large
// void* __cdecl jpeg_get_large(j_common_ptr cinfo, unsigned sizeofobject): returns malloc 0x004a504f
// of the SECOND argument (`mov eax, dword ptr [esp+8]` at 0x004b0460, call at 0x004b0465). The first
// argument is not read and the result is not tested.
typedef void*(__cdecl* JpegGetLarge_t)(void*, unsigned);
JpegGetLarge_t JpegGetLarge_orig;
void* __cdecl JpegGetLarge_re(void*, unsigned sizeofobject) {
    return kExeMalloc(sizeofobject);
}

// 0x004a4ed0  listRemoveHead
// void __thiscall listRemoveHead(): drops the first node of a singly linked list.
// The head is read at 0x004a4ed4 and the whole body is skipped when it is null (`je 0x004a4f0b`).
// The node's payload at +8 is released through free 0x004a5007 when it is not null (`je 0x004a4ef2`
// at 0x004a4ee0, call at 0x004a4ee3) and the field cleared (0x004a4eeb). The list's head is then
// RE-READ (`mov eax,[edi+4]` at 0x004a4ef2) and replaced by that node's +0x10 (0x004a4efa). The node
// itself goes to free at 0x004a4f00 - the `test esi, esi` at 0x004a4ef5 can only be false here,
// because the body already returned for a null head - and the count at +0xc is decremented
// (0x004a4f08).
ExeThis_t ListRemoveHead_orig;
void __fastcall ListRemoveHead_re(void* self, void*) {
    void* node = At<void*>(self, 4);
    if (node == 0)
        return;
    void* payload = At<void*>(node, 8);
    if (payload != 0) {
        kExeFree(payload);
        At<void*>(node, 8) = 0;
    }
    At<void*>(self, 4) = At<void*>(At<void*>(self, 4), 0x10);
    if (node != 0)
        kExeFree(node);
    At<int>(self, 0xc) -= 1;
}

// 0x004a4d50  listClear4a4d
// void __thiscall listClear4a4d(): frees every node of the same list and then clears +8 and +0xc.
// The empty case jumps straight to the two stores (`je 0x004a4d9b` at 0x004a4d59). Each iteration
// reads the head (0x004a4d5c), keeps its +0x10 as the continue flag (0x004a4d61), frees the payload
// at +8 when it is not null (0x004a4d6c) and clears it through a FRESH read of the head (0x004a4d71,
// store at 0x004a4d77), frees the head itself when it is not null (0x004a4d82), stores the saved
// +0x10 as the new head (0x004a4d8c) and loops while that value is not null (0x004a4d8f). The two
// trailing stores (+8 and +0xc) are the same on both exits (0x004a4d91 and 0x004a4d9b).
ExeThis_t ListClear4a4d_orig;
void __fastcall ListClear4a4d_re(void* self, void*) {
    while (At<void*>(self, 4) != 0) {
        void* head = At<void*>(self, 4);
        void* next = At<void*>(head, 0x10);
        if (At<void*>(head, 8) != 0) {
            kExeFree(At<void*>(head, 8));
            At<void*>(At<void*>(self, 4), 8) = 0;
        }
        if (At<void*>(self, 4) != 0)
            kExeFree(At<void*>(self, 4));
        At<void*>(self, 4) = next;
        if (next == 0)
            break;
    }
    At<int>(self, 8) = 0;
    At<int>(self, 0xc) = 0;
}

// 0x004a05d0  bufferFree
// void __thiscall bufferFree(void* rec): releases the buffer at rec+4. The argument is the only stack
// slot (`mov esi, dword ptr [esp+8]` at 0x004a05d1 behind the pushed esi, `ret 4` at 0x004a05f8).
// Nothing happens when that buffer is null (`je 0x004a05f7` at 0x004a05da) OR when the dword at
// this+0x18 is not zero (`mov edx,[ecx+0x18]` / `jne 0x004a05f7` at 0x004a05dc-0x004a05e1): in that
// second case the field is left as it is, so the guard both skips the free AND skips the clear.
// Otherwise free 0x004a5007 runs (0x004a05e8) and rec+4 is cleared (0x004a05f0). The repeated
// `test eax, eax` at 0x004a05e3 can only be false there.
typedef void(__fastcall* BufferFree_t)(void*, void*, void*);
BufferFree_t BufferFree_orig;
void __fastcall BufferFree_re(void* self, void*, void* rec) {
    void* buf = At<void*>(rec, 4);
    if (buf == 0)
        return;
    if (At<int>(self, 0x18) != 0)
        return;
    if (buf != 0)
        kExeFree(buf);
    At<void*>(rec, 4) = 0;
}

// 0x00487310  NodeList::clear
// void __thiscall NodeList::clear(): empties a doubly linked list, releasing every node through
// operator delete 0x004a4ffc. The dword at +8 is always set to 0x004bafac first (0x0048731a) and the
// head at +0xc is read before it (0x00487317); an empty list stops there (`je 0x00487382`).
// Each node: its +4 becomes the new head when it is not null, and that node's +0 is cleared
// (0x0048732c-0x0048732e); otherwise both +0x10 and +0xc of the list are cleared (0x00487333). The
// node's +8 is kept as the continue flag (0x00487339), the node goes to operator delete (0x0048733d)
// and the count at +0x18 is decremented (0x00487348). The loop then re-reads the head and stops when
// either the saved +8 or the head is null (0x0048734e, 0x00487355). The first iteration is peeled, so
// the same block appears twice (0x00487325 and 0x00487357).
ExeThis_t NodeListClear_orig;
void __fastcall NodeListClear_re(void* self, void*) {
    void* node = At<void*>(self, 0xc);
    At<void*>(self, 8) = reinterpret_cast<void*>(0x004bafac);
    while (node != 0) {
        void* prev = At<void*>(node, 4);
        if (prev != 0) {
            At<int>(prev, 0) = 0;
            At<void*>(self, 0xc) = prev;
        } else {
            At<int>(self, 0x10) = 0;
            At<int>(self, 0xc) = 0;
        }
        void* next = At<void*>(node, 8);
        kExeOperatorDelete(node);
        At<int>(self, 0x18) -= 1;
        if (next == 0)
            return;
        node = At<void*>(self, 0xc);
    }
}

// ===================================================================================================
// Part 4 - sound.dll's linked-list helpers. Its allocation entry points are
//   0x100424d7 operator new  (`push 1; push size; call 0x10042adf`)
//   0x10042acd malloc        (`push [0x100b5588]; push size; call 0x10042adf`)
//   0x1004249a operator delete (`push p; call 0x10042c07`)
// and the stand-in replaces those three (fixture c3cn_sound_heap).
//
// Four distinct bodies cover twenty-two functions. That they are byte-identical twins is measured,
// not assumed: log/c3/alt/c3cn_twins.py reads every member's instructions with re/tools/asm2inline.py,
// blanks the addresses and the direct-call operands and compares them (log/c3/alt/c3cn_twins.txt).
//   push_back      9 members, reference 0x10005e30 (32 instructions, all identical)
//   pushFront      1 member,  0x10024f90: the same 32 instructions with the five that anchor on the
//                  list's +4 reading and writing +0 instead (and n's +4 instead of its +0)
//   clearNodeList  9 members, reference 0x10005a60 (46 instructions, all identical)
//   popFront       2 members, 0x10005c00 and 0x10039620 (27 instructions, identical)
// The list is { head +0, tail +4, ?, count +0xc } and a node { prev +0, next +4, value +8 }.

char* SoundBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("sound.dll"));
    return base;
}
void* SoundVA(unsigned va) { return SoundBase() + (va - 0x10000000); }

typedef void*(__cdecl* SoundNew_t)(unsigned);                     // 0x100424d7  operator new
typedef void(__cdecl* SoundDelete_t)(void*);                      // 0x1004249a  operator delete
typedef void*(__cdecl* SoundMalloc_t)(unsigned);                  // 0x10042acd  malloc

SoundNew_t SoundNew() { return reinterpret_cast<SoundNew_t>(SoundVA(0x100424d7)); }
SoundDelete_t SoundDelete() { return reinterpret_cast<SoundDelete_t>(SoundVA(0x1004249a)); }

// push_back, reference body 0x10005e30:
// A 0xc-byte node comes from operator new (0x10005e35). When it is not null (`je 0x10005e57` at
// 0x10005e3f) its +0 and +4 are cleared and the argument is stored at +8 (0x10005e45-0x10005e52);
// the other arm only repeats `xor eax, eax`, i.e. it leaves the node pointer null, and the code that
// follows would then write through it. Under the stand-in the allocation never fails, so that arm is
// not taken (and it is the arm that would fault).
// With a non-null tail at +4 (`je 0x10005e76` at 0x10005e5e) the node is appended: tail->+4 = node
// (0x10005e60), node->+0 = the tail RE-READ from the list (0x10005e63-0x10005e66) and the list's +4
// becomes the node (0x10005e68). An empty list instead gets the node at both +0 and +4 (0x10005e76).
// Both arms increment the count at +0xc and leave it in eax (0x10005e6b, 0x10005e7b), so the return
// value is the new count. `ret 4`.
typedef int(__fastcall* SoundPush_t)(void*, void*, void*);
int SoundPushBack(void* self, void* value) {
    void* n = SoundNew()(0xc);
    if (n != 0) {
        At<void*>(n, 0) = 0;
        At<void*>(n, 4) = 0;
        At<void*>(n, 8) = value;
    }
    if (At<void*>(self, 4) != 0) {
        At<void*>(At<void*>(self, 4), 4) = n;
        At<void*>(n, 0) = At<void*>(self, 4);
        At<void*>(self, 4) = n;
    } else {
        At<void*>(self, 0) = n;
        At<void*>(self, 4) = n;
    }
    At<int>(self, 0xc) += 1;
    return At<int>(self, 0xc);
}

// pushFront 0x10024f90: the same body anchored on the HEAD at +0 (`mov ecx,[esi]` at 0x10024fb9):
// head->+0 = node (0x10024fbf), node->+4 = the head re-read (0x10024fc1-0x10024fc3), list->+0 = node
// (0x10024fc6). Its empty arm is the same as push_back's (0x10024fd3).
int SoundPushFront(void* self, void* value) {
    void* n = SoundNew()(0xc);
    if (n != 0) {
        At<void*>(n, 0) = 0;
        At<void*>(n, 4) = 0;
        At<void*>(n, 8) = value;
    }
    if (At<void*>(self, 0) != 0) {
        At<void*>(At<void*>(self, 0), 0) = n;
        At<void*>(n, 4) = At<void*>(self, 0);
        At<void*>(self, 0) = n;
    } else {
        At<void*>(self, 0) = n;
        At<void*>(self, 4) = n;
    }
    At<int>(self, 0xc) += 1;
    return At<int>(self, 0xc);
}

// clearNodeList, reference body 0x10005a60: releases every node through operator delete and keeps the
// count at +0xc in step. The head is read at 0x10005a64 and an empty list returns at once
// (`je 0x10005ada`). Each node: its +4 becomes the new head and that node's +0 is cleared
// (0x10005a71-0x10005a77), or, when +4 is null, both the list's +4 and +0 are cleared (0x10005a7b).
// The node's +8 is kept as the continue flag (0x10005a88), the node goes to operator delete
// (0x10005a8c) and the count is decremented (0x10005a97). The loop stops when that flag is null
// (0x10005a9d) or when the new head is null (0x10005aa3). The first iteration is peeled, so the same
// block appears twice (0x10005a6a and 0x10005aa5).
void SoundClearNodeList(void* self) {
    void* node = At<void*>(self, 0);
    while (node != 0) {
        void* prev = At<void*>(node, 4);
        if (prev != 0) {
            At<int>(prev, 0) = 0;
            At<void*>(self, 0) = prev;
        } else {
            At<int>(self, 4) = 0;
            At<int>(self, 0) = 0;
        }
        void* next = At<void*>(node, 8);
        SoundDelete()(node);
        At<int>(self, 0xc) -= 1;
        if (next == 0)
            return;
        node = At<void*>(self, 0);
    }
}

// popFront, reference body 0x10005c00: one iteration of the same unlinking, returning the node's +8.
// An empty list returns with eax still holding the null head (0x10005c03, `ret` at 0x10005c0a). The
// +8 of the removed node is saved first (0x10005c2a), the node goes to operator delete (0x10005c2e),
// the count is decremented (0x10005c39) and the saved value is the return (`mov eax, edi` 0x10005c3d).
typedef void*(__fastcall* SoundPop_t)(void*, void*);
void* SoundPopFront(void* self) {
    void* node = At<void*>(self, 0);
    if (node == 0)
        return 0;
    void* prev = At<void*>(node, 4);
    if (prev != 0) {
        At<int>(prev, 0) = 0;
        At<void*>(self, 0) = prev;
    } else {
        At<int>(self, 4) = 0;
        At<int>(self, 0) = 0;
    }
    void* value = At<void*>(node, 8);
    SoundDelete()(node);
    At<int>(self, 0xc) -= 1;
    return value;
}

// 0x00006ec0  freeDynArray
// void __thiscall freeDynArray(): clears bit 0 of the flags at +8 (`and edx, 0xfffffffe` at
// 0x10006ec9, stored at 0x10006ece BEFORE the branch), releases the buffer at +4 through operator
// delete when it is not null (`je 0x10006edc` at 0x10006ed1, call at 0x10006ed4) and clears that
// field unconditionally (0x10006edc).
void SoundFreeDynArray(void* self) {
    int flags = At<int>(self, 8) & ~1;
    void* buf = At<void*>(self, 4);
    At<int>(self, 8) = flags;
    if (buf != 0)
        SoundDelete()(buf);
    At<void*>(self, 4) = 0;
}

// The twenty-two detours: one per address, each forwarding to the body its family shares.
#define C3CN_SOUND_PUSH(fn)                                                        \
    SoundPush_t fn##_orig;                                                         \
    int __fastcall fn##_re(void* self, void*, void* v) { return SoundPushBack(self, v); }
#define C3CN_SOUND_CLEAR(fn)                                                       \
    ExeThis_t fn##_orig;                                                           \
    void __fastcall fn##_re(void* self, void*) { SoundClearNodeList(self); }
#define C3CN_SOUND_POP(fn)                                                         \
    SoundPop_t fn##_orig;                                                          \
    void* __fastcall fn##_re(void* self, void*) { return SoundPopFront(self); }

// 0x00005e30  appendNodeB
C3CN_SOUND_PUSH(AppendNodeB)
// 0x0000e680  appendNodeC
C3CN_SOUND_PUSH(AppendNodeC)
// 0x000137f0  appendNode
C3CN_SOUND_PUSH(AppendNode)
// 0x0001d150  PtrList_pushBack
C3CN_SOUND_PUSH(PtrListPushBack)
// 0x00024de0  NodeList_pushBack
C3CN_SOUND_PUSH(NodeListPushBack)
// 0x00031b60  ResourceList_pushBack
C3CN_SOUND_PUSH(ResourceListPushBack)
// 0x00038430  nodeListAppendValue
C3CN_SOUND_PUSH(NodeListAppendValue)
// 0x00038550  listAppendNodeA
C3CN_SOUND_PUSH(ListAppendNodeA)
// 0x000385d0  listAppendNodeB
C3CN_SOUND_PUSH(ListAppendNodeB)

// 0x00024f90  NodeList_pushFront
SoundPush_t NodeListPushFront_orig;
int __fastcall NodeListPushFront_re(void* self, void*, void* v) { return SoundPushFront(self, v); }

// 0x00005a60  clearNodeListD
C3CN_SOUND_CLEAR(ClearNodeListD)
// 0x00005b60  clearNodeListE
C3CN_SOUND_CLEAR(ClearNodeListE)
// 0x00006c70  clearNodeListF
C3CN_SOUND_CLEAR(ClearNodeListF)
// 0x0000ae80  clearNodeListG
C3CN_SOUND_CLEAR(ClearNodeListG)
// 0x0000e5e0  clearNodeListC
C3CN_SOUND_CLEAR(ClearNodeListC)
// 0x00013620  clearNodeListA
C3CN_SOUND_CLEAR(ClearNodeListA)
// 0x00013750  clearNodeListB
C3CN_SOUND_CLEAR(ClearNodeListB)
// 0x0001f1c0  PtrList_freeNodesA
C3CN_SOUND_CLEAR(PtrListFreeNodesA)
// 0x00024d40  NodeList_clear (sound.dll; the exe has a different NodeList::clear at 0x00487310)
C3CN_SOUND_CLEAR(NodeListClear_sound)

// 0x00005c00  listPopFront
C3CN_SOUND_POP(ListPopFront)
// 0x00039620  nodeListPopFront
C3CN_SOUND_POP(NodeListPopFront)

// 0x00006ec0  freeDynArray
ExeThis_t FreeDynArray_orig;
void __fastcall FreeDynArray_re(void* self, void*) { SoundFreeDynArray(self); }

// ===================================================================================================
// Part 5 - Terrain.dll's std::list<Tile*> allocation layer (VC6's STL, a /Od /ZI /GZ debug build, so
// every body is literal). Its allocation entry points are operator new 0x10018c90 (`push 1; push size;
// call 0x10015940`) and operator delete 0x10018be0 (the debug form that validates the block header at
// ptr - 0x20); the stand-in replaces those two (fixture c3cn_terrain_heap).
// The calls below go through the incremental-link thunks the debug build emits; each one was resolved
// and is cited with both its thunk and its target.

char* TerrainBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("Terrain.dll"));
    return base;
}
void* TerrainVA(unsigned va) { return TerrainBase() + (va - 0x10000000); }

typedef void*(__cdecl* TerrainNew_t)(unsigned);                   // 0x10018c90  operator new
typedef void(__cdecl* TerrainDelete_t)(void*);                    // 0x10018be0  operator delete

// 0x0000bae0  std::_Allocate
// void* __cdecl std::_Allocate(int count, T*): clamps a NEGATIVE count to zero (`cmp dword ptr
// [ebp+8], 0` / `jge 0x1000bb05` at 0x1000baf8 - a SIGNED test - and the store at 0x1000bafe) and
// returns operator new 0x10018c90 of the result (0x1000bb09). The second template argument is never
// read. The count is passed to operator new unchanged, so the caller supplies bytes, not elements.
typedef void*(__cdecl* Allocate_t)(int);
Allocate_t TerrainAllocate_orig;
void* __cdecl TerrainAllocate_re(int count) {
    if (count < 0)
        count = 0;
    return reinterpret_cast<TerrainNew_t>(TerrainVA(0x10018c90))(static_cast<unsigned>(count));
}

// 0x0000b960  std::allocator<Tile*>::deallocate
// void __thiscall std::allocator<Tile*>::deallocate(void* p, unsigned n): one call, operator delete
// 0x10018be0 on the FIRST stack argument (`mov eax, dword ptr [ebp+8]` at 0x1000b97d, call at
// 0x1000b981). The element count and `this` are not read; `ret 8` at 0x1000b999.
typedef void(__fastcall* Deallocate_t)(void*, void*, void*, unsigned);
Deallocate_t TerrainDeallocate_orig;
void __fastcall TerrainDeallocate_re(void*, void*, void* p, unsigned) {
    reinterpret_cast<TerrainDelete_t>(TerrainVA(0x10018be0))(p);
}

// 0x0000b910  std::allocator<Tile*>::_Charalloc
// void* __thiscall std::allocator<Tile*>::_Charalloc(unsigned size): returns std::_Allocate 0x1000bae0
// of (size, 0) - `push 0` then `push [ebp+8]` at 0x1000b92d-0x1000b932 and the call through the
// incremental-link thunk 0x10001267 at 0x1000b933, whose target is 0x1000bae0. `this` is not read;
// `ret 4` at 0x1000b94b.
typedef void*(__fastcall* Charalloc_t)(void*, void*, unsigned);
Charalloc_t TerrainCharalloc_orig;
void* __fastcall TerrainCharalloc_re(void*, void*, unsigned size) {
    return reinterpret_cast<void*(__cdecl*)(unsigned, void*)>(TerrainVA(0x1000bae0))(size, 0);
}

// 0x0000b5b0  std::list<Tile*>::_Freenode
// void __thiscall std::list<Tile*>::_Freenode(void* node): one call, the allocator's deallocate
// 0x1000b960 with (node, 1) - `push 1` then `push [ebp+8]` at 0x1000b5cd-0x1000b5d2, `this` reloaded
// into ecx at 0x1000b5d3 and the call through the thunk 0x10001334 at 0x1000b5d6, whose target is
// 0x1000b960. deallocate cleans its own two arguments (`ret 8`), which is why no `add esp` follows.
typedef void(__fastcall* Freenode_t)(void*, void*, void*);
Freenode_t TerrainFreenode_orig;
void __fastcall TerrainFreenode_re(void* self, void*, void* node) {
    reinterpret_cast<Deallocate_t>(TerrainVA(0x1000b960))(self, 0, node, 1);
}

// 0x0000b500  std::list<Tile*>::_Buynode
// void* __thiscall std::list<Tile*>::_Buynode(void* next, void* prev): makes one list node.
// The 0xc bytes come from _Charalloc 0x1000b910 (`push 0xc` at 0x1000b51d, `this` in ecx, the call
// through the thunk 0x10001276 at 0x1000b522). The node's next field is the FIRST argument when it is
// not null and the node itself otherwise (`cmp dword ptr [ebp+8], 0` / `je 0x1000b538` at 0x1000b52a,
// the two stores into [ebp-0xc] at 0x1000b533 and 0x1000b53b), written through _Acc::_Next 0x1000b600
// (thunk 0x100010e1 at 0x1000b542), which returns the ADDRESS of the field (`mov dword ptr [eax], ecx`
// at 0x1000b54d). The prev field is the SECOND argument or the node itself by the same shape
// (0x1000b54f, _Acc::_Prev 0x1000b630 through the thunk 0x10001159 at 0x1000b567, store at
// 0x1000b572). The node is returned (`ret 8` at 0x1000b587).
typedef void*(__fastcall* Buynode_t)(void*, void*, void*, void*);
typedef void**(__cdecl* Acc_t)(void*);
Buynode_t TerrainBuynode_orig;
void* __fastcall TerrainBuynode_re(void* self, void*, void* next, void* prev) {
    void* node = reinterpret_cast<Charalloc_t>(TerrainVA(0x1000b910))(self, 0, 0xc);
    void* n = next != 0 ? next : node;
    *reinterpret_cast<Acc_t>(TerrainVA(0x1000b600))(node) = n;
    void* p = prev != 0 ? prev : node;
    *reinterpret_cast<Acc_t>(TerrainVA(0x1000b630))(node) = p;
    return node;
}

// 0x0000ae60  std::list<Tile*>::list
// std::list<Tile*>* __thiscall std::list<Tile*>::list(const allocator& a): copies ONE BYTE of the
// allocator argument into the list's +0 (`mov cl, byte ptr [eax]` at 0x1000ae80, store at 0x1000ae85 -
// the allocator is an empty class, so the byte is whatever the caller's object holds), takes the
// sentinel node from _Buynode 0x1000b500 with both arguments null (`push 0; push 0` at 0x1000ae87,
// `this` in ecx, the call through the thunk 0x1000129e at 0x1000ae8e), stores it at +4 (0x1000ae96)
// and sets the size at +8 to 0 (0x1000ae9c). `this` is returned (`ret 4` at 0x1000aeb6).
typedef void*(__fastcall* ListCtor_t)(void*, void*, void*);
ListCtor_t TerrainListCtor_orig;
void* __fastcall TerrainListCtor_re(void* self, void*, void* alloc) {
    At<char>(self, 0) = At<char>(alloc, 0);
    At<void*>(self, 4) = reinterpret_cast<Buynode_t>(TerrainVA(0x1000b500))(self, 0, 0, 0);
    At<int>(self, 8) = 0;
    return self;
}

}  // namespace

// 0x00014a40  jalloc
SG_HOOK("jgld.dll", 0x00014A40, jalloc, Jalloc_re, Jalloc_orig);
// 0x00014a90  jfree
SG_HOOK("jgld.dll", 0x00014A90, jfree, Jfree_re, Jfree_orig);
// 0x00078e60  png_malloc
SG_HOOK("jgld.dll", 0x00078E60, png_malloc, PngMalloc_re, PngMalloc_orig);
// 0x00078ed0  png_free
SG_HOOK("jgld.dll", 0x00078ED0, png_free, PngFree_re, PngFree_orig);
// 0x000786b0  png_zalloc
SG_HOOK("jgld.dll", 0x000786B0, png_zalloc, PngZalloc_re, PngZalloc_orig);
// 0x00078750  png_zfree
SG_HOOK("jgld.dll", 0x00078750, png_zfree, PngZfree_re, PngZfree_orig);
// 0x00078da0  png_create_struct
SG_HOOK("jgld.dll", 0x00078DA0, png_create_struct, PngCreateStruct_re, PngCreateStruct_orig);
// 0x00078e20  png_destroy_struct
SG_HOOK("jgld.dll", 0x00078E20, png_destroy_struct, PngDestroyStruct_re, PngDestroyStruct_orig);
// 0x0009e680  zcalloc
SG_HOOK("jgld.dll", 0x0009E680, zcalloc, Zcalloc_re, Zcalloc_orig);
// 0x0009e6d0  zcfree
SG_HOOK("jgld.dll", 0x0009E6D0, zcfree, Zcfree_re, Zcfree_orig);
// 0x00068000  Array_e0::clear
SG_HOOK("jgld.dll", 0x00068000, Array_e0_clear, ArrayE0Clear_re, ArrayE0Clear_orig);
// 0x00068100  Array_Rec14::clear
SG_HOOK("jgld.dll", 0x00068100, Array_Rec14_clear, ArrayRec14Clear_re, ArrayRec14Clear_orig);
// 0x00068360  Array_Rec94::clear
SG_HOOK("jgld.dll", 0x00068360, Array_Rec94_clear, ArrayRec94Clear_re, ArrayRec94Clear_orig);
// 0x00068570  Array_e0::clearRet
SG_HOOK("jgld.dll", 0x00068570, Array_e0_clearRet, ArrayE0ClearRet_re, ArrayE0ClearRet_orig);
// 0x00068610  Array_Rec94::clearRet
SG_HOOK("jgld.dll", 0x00068610, Array_Rec94_clearRet, ArrayRec94ClearRet_re, ArrayRec94ClearRet_orig);
// 0x0001a0e0  Sprite::copyFrom
SG_HOOK("jgld.dll", 0x0001A0E0, Sprite_copyFrom, SpriteCopyFrom_re, SpriteCopyFrom_orig);

// ------------------------------------------------------------------ golf_clean.exe release leaves
// 0x004747e0  Buf_releaseData
SG_HOOK("golf_clean.exe", 0x004747E0, Buf_releaseData, BufReleaseData_re, BufReleaseData_orig);
// 0x00474810  Buf_dtor
SG_HOOK("golf_clean.exe", 0x00474810, Buf_dtor, BufDtor_re, BufDtor_orig);
// 0x0049d0e0  strFree
SG_HOOK("golf_clean.exe", 0x0049D0E0, strFree, StrFree_re, StrFree_orig);
// 0x004882f0  KeyTable::dtor
SG_HOOK("golf_clean.exe", 0x004882F0, KeyTable_dtor, KeyTableDtor_re, KeyTableDtor_orig);
// 0x004a01a0  freeLoggerBuf
SG_HOOK("golf_clean.exe", 0x004A01A0, freeLoggerBuf, FreeLoggerBuf_re, FreeLoggerBuf_orig);
// 0x004b0470  jpeg_free_large
SG_HOOK("golf_clean.exe", 0x004B0470, jpeg_free_large, JpegFreeLarge_re, JpegFreeLarge_orig);
// 0x004b0460  jpeg_get_large
SG_HOOK("golf_clean.exe", 0x004B0460, jpeg_get_large, JpegGetLarge_re, JpegGetLarge_orig);
// 0x004a4ed0  listRemoveHead
SG_HOOK("golf_clean.exe", 0x004A4ED0, listRemoveHead, ListRemoveHead_re, ListRemoveHead_orig);
// 0x004a4d50  listClear4a4d
SG_HOOK("golf_clean.exe", 0x004A4D50, listClear4a4d, ListClear4a4d_re, ListClear4a4d_orig);
// 0x004a05d0  bufferFree
SG_HOOK("golf_clean.exe", 0x004A05D0, bufferFree, BufferFree_re, BufferFree_orig);
// 0x00487310  NodeList::clear
SG_HOOK("golf_clean.exe", 0x00487310, NodeList_clear, NodeListClear_re, NodeListClear_orig);

// --------------------------------------------------------------- sound.dll linked-list helpers
// 0x00005e30  appendNodeB
SG_HOOK("sound.dll", 0x00005E30, appendNodeB, AppendNodeB_re, AppendNodeB_orig);
// 0x0000e680  appendNodeC
SG_HOOK("sound.dll", 0x0000E680, appendNodeC, AppendNodeC_re, AppendNodeC_orig);
// 0x000137f0  appendNode
SG_HOOK("sound.dll", 0x000137F0, appendNode, AppendNode_re, AppendNode_orig);
// 0x0001d150  PtrList_pushBack
SG_HOOK("sound.dll", 0x0001D150, PtrList_pushBack, PtrListPushBack_re, PtrListPushBack_orig);
// 0x00024de0  NodeList_pushBack
SG_HOOK("sound.dll", 0x00024DE0, NodeList_pushBack, NodeListPushBack_re, NodeListPushBack_orig);
// 0x00031b60  ResourceList_pushBack
SG_HOOK("sound.dll", 0x00031B60, ResourceList_pushBack, ResourceListPushBack_re, ResourceListPushBack_orig);
// 0x00038430  nodeListAppendValue
SG_HOOK("sound.dll", 0x00038430, nodeListAppendValue, NodeListAppendValue_re, NodeListAppendValue_orig);
// 0x00038550  listAppendNodeA
SG_HOOK("sound.dll", 0x00038550, listAppendNodeA, ListAppendNodeA_re, ListAppendNodeA_orig);
// 0x000385d0  listAppendNodeB
SG_HOOK("sound.dll", 0x000385D0, listAppendNodeB, ListAppendNodeB_re, ListAppendNodeB_orig);
// 0x00024f90  NodeList_pushFront
SG_HOOK("sound.dll", 0x00024F90, NodeList_pushFront, NodeListPushFront_re, NodeListPushFront_orig);
// 0x00005a60  clearNodeListD
SG_HOOK("sound.dll", 0x00005A60, clearNodeListD, ClearNodeListD_re, ClearNodeListD_orig);
// 0x00005b60  clearNodeListE
SG_HOOK("sound.dll", 0x00005B60, clearNodeListE, ClearNodeListE_re, ClearNodeListE_orig);
// 0x00006c70  clearNodeListF
SG_HOOK("sound.dll", 0x00006C70, clearNodeListF, ClearNodeListF_re, ClearNodeListF_orig);
// 0x0000ae80  clearNodeListG
SG_HOOK("sound.dll", 0x0000AE80, clearNodeListG, ClearNodeListG_re, ClearNodeListG_orig);
// 0x0000e5e0  clearNodeListC
SG_HOOK("sound.dll", 0x0000E5E0, clearNodeListC, ClearNodeListC_re, ClearNodeListC_orig);
// 0x00013620  clearNodeListA
SG_HOOK("sound.dll", 0x00013620, clearNodeListA, ClearNodeListA_re, ClearNodeListA_orig);
// 0x00013750  clearNodeListB
SG_HOOK("sound.dll", 0x00013750, clearNodeListB, ClearNodeListB_re, ClearNodeListB_orig);
// 0x0001f1c0  PtrList_freeNodesA
SG_HOOK("sound.dll", 0x0001F1C0, PtrList_freeNodesA, PtrListFreeNodesA_re, PtrListFreeNodesA_orig);
// 0x00024d40  NodeList_clear
SG_HOOK("sound.dll", 0x00024D40, NodeList_clear_sound, NodeListClear_sound_re, NodeListClear_sound_orig);
// 0x00005c00  listPopFront
SG_HOOK("sound.dll", 0x00005C00, listPopFront, ListPopFront_re, ListPopFront_orig);
// 0x00039620  nodeListPopFront
SG_HOOK("sound.dll", 0x00039620, nodeListPopFront, NodeListPopFront_re, NodeListPopFront_orig);
// 0x00006ec0  freeDynArray
SG_HOOK("sound.dll", 0x00006EC0, freeDynArray, FreeDynArray_re, FreeDynArray_orig);

// ------------------------------------------------- Terrain.dll std::list<Tile*> allocation layer
// 0x0000bae0  std::_Allocate
SG_HOOK("Terrain.dll", 0x0000BAE0, std_Allocate, TerrainAllocate_re, TerrainAllocate_orig);
// 0x0000b960  std::allocator<Tile*>::deallocate
SG_HOOK("Terrain.dll", 0x0000B960, allocator_Tile_deallocate, TerrainDeallocate_re, TerrainDeallocate_orig);
// 0x0000b910  std::allocator<Tile*>::_Charalloc
SG_HOOK("Terrain.dll", 0x0000B910, allocator_Tile_Charalloc, TerrainCharalloc_re, TerrainCharalloc_orig);
// 0x0000b5b0  std::list<Tile*>::_Freenode
SG_HOOK("Terrain.dll", 0x0000B5B0, list_Tile_Freenode, TerrainFreenode_re, TerrainFreenode_orig);
// 0x0000b500  std::list<Tile*>::_Buynode
SG_HOOK("Terrain.dll", 0x0000B500, list_Tile_Buynode, TerrainBuynode_re, TerrainBuynode_orig);
// 0x0000ae60  std::list<Tile*>::list
SG_HOOK("Terrain.dll", 0x0000AE60, list_Tile_ctor, TerrainListCtor_re, TerrainListCtor_orig);
