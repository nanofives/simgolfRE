// C3 batch c3cc (round 19, 2026-10-08) of golf_clean.exe. Ten small functions: the two engine objects that
// release a child and then run a global close hook, the third copy of that close hook, the scrollbar child's
// range and thumb entry points, and five IJG libjpeg 6a routines (the linear-quality installer, the two
// error-raising memory stubs, the available-memory stub and the compress destructor).
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees run through their original addresses:
//   releaseWithHookA     -> closeHookA 0x00487430 (C3, c3bx)
//   releaseWithHookB     -> closeHookB 0x004879f0 (C3, c3bx)
//   orderPair            -> swapInts 0x00493580 (C3, c3r)
//   scrollbarResetThumb  -> scrollbarResetState 0x004979a0 (C3, c3v)
//   jpeg_set_linear_quality -> jpeg_add_quant_table 0x004ae600 (C3, c3aa), whose own callee
//                           jpeg_alloc_quant_table 0x004afab0 (C3, c3af) runs on the _alloc fixture
//   jpeg_destroy_compress-> jpeg_destroy 0x004afa90 (C3, c3r)
// The pointers the other functions dispatch through (the child's vtable slot +0x14, the widget's vtable slot
// +0x120, the global close hook 0x0083af90, the libjpeg error manager's error_exit at err+0 and the memory
// manager's self_destruct at mem+0x28) are the fixture's own callbacks, not hooks.
//
//   releaseWithHookA 0x00487460   releaseWithHookB 0x00487ac0   closeHookC 0x00487bd0
//   orderPair 0x00496770          scrollbarResetThumb 0x004967d0
//   jpeg_set_linear_quality 0x004ae6c0   out_of_memory 0x004af0a0
//   jpeg_open_backing_store 0x004b0490   jpeg_mem_available 0x004b0480
//   jpeg_destroy_compress 0x004ae210
#include "hooks.h"

namespace {

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
// The dword at base+off read as a value of type T (a function pointer where the original does `call [r+off]`).
template <typename T> static inline T F(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
// `mov eax, [obj]` then `call/jmp [eax+off]`: the object's vtable pointer is at +0, the slot at +off.
static inline void* vslot(void* obj, unsigned off) {
    return field_ptr(field_ptr(obj, 0), off);
}

// Callees reached through their original addresses.
typedef int(__fastcall* CloseHook_t)(void* ecx, void* edx);
const CloseHook_t kCloseHookA = reinterpret_cast<CloseHook_t>(0x00487430);          // C3
const CloseHook_t kCloseHookB = reinterpret_cast<CloseHook_t>(0x004879f0);          // C3
typedef void(__cdecl* SwapInts_t)(int* a, int* b);
const SwapInts_t kSwapInts = reinterpret_cast<SwapInts_t>(0x00493580);              // C3
typedef void(__fastcall* ResetState_t)(void* ecx, void* edx);
const ResetState_t kScrollbarResetState = reinterpret_cast<ResetState_t>(0x004979a0);  // C3
typedef void(__cdecl* AddQuantTable_t)(void* cinfo, int which_tbl, const unsigned int* basic_table,
                                       int scale_factor, int force_baseline);
const AddQuantTable_t kJpegAddQuantTable = reinterpret_cast<AddQuantTable_t>(0x004ae600);   // C3
typedef void(__cdecl* JpegDestroy_t)(void* cinfo);
const JpegDestroy_t kJpegDestroy = reinterpret_cast<JpegDestroy_t>(0x004afa90);     // C3

// The two 64-entry basic tables pushed at 0x004ae6d1 and 0x004ae6e0. jpeg_add_quant_table reads them with
// `imul ecx, dword ptr [edi]` and `add edi, 4` (0x004ae651 / 0x004ae697), so the entries are 32-bit.
const unsigned int* const kStdLuminanceQuantTbl = reinterpret_cast<const unsigned int*>(0x004bcb08);
const unsigned int* const kStdChrominanceQuantTbl = reinterpret_cast<const unsigned int*>(0x004bcc08);

// Slots the fixture fills.
typedef int(__fastcall* ChildRelease_t)(void* ecx, void* edx);   // child vtable +0x14
typedef int(__fastcall* WidgetRedraw_t)(void* ecx, void* edx);   // widget vtable +0x120
typedef void(__cdecl* GlobalCloseHook_t)();                      // the dword at 0x0083af90
typedef void(__cdecl* ErrorExit_t)(void* cinfo);                 // err+0

// 0x00487460  releaseWithHookA(this): hand the child object held at +0x14 back through the child's own
// vtable slot +0x14, then close this object's own resource with closeHookA 0x00487430 and forget the child.
// The child is read at 0x00487463 and a null one leaves everything untouched (0x00487468). Otherwise the
// child's slot +0x14 runs with the CHILD in ecx (0x0048746a / 0x0048746c), closeHookA runs with THIS in ecx
// (0x0048746f / 0x00487471) - and it still sees the child pointer in +0x14, which is the handle it tests at
// 0x00487433 - and only then is +0x14 cleared (0x00487476). The return value is 0 on both paths (0x0048747d).
typedef int(__fastcall* ReleaseWithHook_t)(void* ecx, void* edx);
ReleaseWithHook_t ReleaseWithHookA_orig;
int __fastcall ReleaseWithHookA_re(void* thisp, void*) {
    void* child = field_ptr(thisp, 0x14);                                       // 0x00487463
    if (child != 0) {                                                           // 0x00487468
        reinterpret_cast<ChildRelease_t>(vslot(child, 0x14))(child, 0);          // 0x0048746a / 0x0048746c
        kCloseHookA(thisp, 0);                                                   // 0x0048746f / 0x00487471
        field_i(thisp, 0x14) = 0;                                                // 0x00487476
    }
    return 0;                                                                   // 0x0048747d
}

// 0x00487ac0  releaseWithHookB(this): the same body against closeHookB 0x004879f0. The child is read at
// 0x00487ac3, a null one ends the function at 0x00487add (0x00487ac8), the child's vtable slot +0x14 runs at
// 0x00487aca / 0x00487acc, closeHookB at 0x00487acf / 0x00487ad1 and +0x14 is cleared at 0x00487ad6.
ReleaseWithHook_t ReleaseWithHookB_orig;
int __fastcall ReleaseWithHookB_re(void* thisp, void*) {
    void* child = field_ptr(thisp, 0x14);                                       // 0x00487ac3
    if (child != 0) {                                                           // 0x00487ac8
        reinterpret_cast<ChildRelease_t>(vslot(child, 0x14))(child, 0);          // 0x00487aca / 0x00487acc
        kCloseHookB(thisp, 0);                                                   // 0x00487acf / 0x00487ad1
        field_i(thisp, 0x14) = 0;                                                // 0x00487ad6
    }
    return 0;                                                                   // 0x00487add
}

// 0x00487bd0  closeHookC(this): run the global close hook 0x0083af90 for an object that still holds a
// resource and mark the object closed. The handle at +0x14 is read at 0x00487bd3; a zero handle (0x00487bd8)
// or a null hook (0x00487bdf / 0x00487be1) skips to the failure value 0x14 at 0x00487bf0. When both are set
// the hook runs with no arguments and no stack cleanup (0x00487be3), the handle is cleared (0x00487be5) and
// the function returns 0 (0x00487bec). Third copy of the body of closeHookA 0x00487430 and
// closeHookB 0x004879f0, differing only in the global it reads.
typedef int(__fastcall* CloseHookC_t)(void* ecx, void* edx);
CloseHookC_t CloseHookC_orig;
int __fastcall CloseHookC_re(void* thisp, void*) {
    if (field_i(thisp, 0x14) != 0) {                                            // 0x00487bd3 / 0x00487bd8
        GlobalCloseHook_t hook =
            *reinterpret_cast<GlobalCloseHook_t*>(0x0083af90);                  // 0x00487bda
        if (hook != 0) {                                                        // 0x00487bdf / 0x00487be1
            hook();                                                             // 0x00487be3
            field_i(thisp, 0x14) = 0;                                           // 0x00487be5
            return 0;                                                           // 0x00487bec
        }
    }
    return 0x14;                                                                // 0x00487bf0
}

// 0x00496770  orderPair(this, a, b): store the two scalars as an ordered (low, high) pair in the widget and
// redraw it. The first argument is read at 0x00496770 and compared with the second at 0x00496777; the compare
// is SIGNED (`jge` at 0x0049677b). When the first is the smaller, +0x588 takes 0 (0x0049677d) and the two
// values keep the caller's order. Otherwise +0x588 takes 1 (0x00496793) and swapInts 0x00493580 exchanges the
// two stack slots in place (0x0049679d), after which the first slot is re-read at 0x004967a2. Either way the
// first slot goes to +0x580 (0x004967ad) and +0x58c (0x004967b3) and the second, read at 0x004967a9, to
// +0x584 (0x004967bd); the widget's vtable slot +0x120 is then called with the widget in ecx (0x004967b9 /
// 0x004967c3) and supplies this function's return value. The caller pops the two dwords (`ret 8`, 0x004967ca).
typedef int(__fastcall* OrderPair_t)(void* ecx, void* edx, int a, int b);
OrderPair_t OrderPair_orig;
int __fastcall OrderPair_re(void* thisp, void*, int a, int b) {
    if (a < b) {                                                                // 0x00496777 / 0x0049677b
        field_i(thisp, 0x588) = 0;                                              // 0x0049677d
    } else {
        field_i(thisp, 0x588) = 1;                                              // 0x00496793
        kSwapInts(&a, &b);                                                      // 0x0049679d
    }
    field_i(thisp, 0x580) = a;                                                  // 0x004967a2 / 0x004967ad
    field_i(thisp, 0x58c) = a;                                                  // 0x004967b3
    field_i(thisp, 0x584) = b;                                                  // 0x004967a9 / 0x004967bd
    return reinterpret_cast<WidgetRedraw_t>(vslot(thisp, 0x120))(thisp, 0);     // 0x004967c3
}

// 0x004967d0  scrollbarResetThumb(this, span): record the scrollbar's total span and rebuild the thumb
// rectangle from it. The caller's dword is read at 0x004967d0 and stored in +0x5c0 (0x004967d4), which is the
// field scrollbarResetState 0x004979a0 reads first (0x004979a0); that callee then runs with the widget still
// in ecx (0x004967da) and the caller pops the dword (`ret 4`, 0x004967df).
typedef void(__fastcall* ResetThumb_t)(void* ecx, void* edx, int span);
ResetThumb_t ScrollbarResetThumb_orig;
void __fastcall ScrollbarResetThumb_re(void* thisp, void*, int span) {
    field_i(thisp, 0x5c0) = span;                                               // 0x004967d4
    kScrollbarResetState(thisp, 0);                                             // 0x004967da
}

// 0x004ae6c0  jpeg_set_linear_quality(cinfo, scale_factor, force_baseline): install the two standard JPEG
// quantisation tables, both scaled by the same percentage. The compress object comes from the first stack
// dword (0x004ae6c1), force_baseline from the third (0x004ae6c6) and scale_factor from the second
// (0x004ae6cb). jpeg_add_quant_table 0x004ae600 is then called twice with the five dwords pushed in reverse
// order: table slot 0 with the basic table at 0x004bcb08 (0x004ae6cf..0x004ae6d9) and table slot 1 with the
// basic table at 0x004bcc08 (0x004ae6de..0x004ae6e8); the caller-pops `add esp, 0x28` at 0x004ae6ed removes
// both argument lists. The function has no conditional branch and writes nothing itself.
typedef void(__cdecl* SetLinearQuality_t)(void* cinfo, int scale_factor, int force_baseline);
SetLinearQuality_t SetLinearQuality_orig;
void __cdecl SetLinearQuality_re(void* cinfo, int scale_factor, int force_baseline) {
    kJpegAddQuantTable(cinfo, 0, kStdLuminanceQuantTbl, scale_factor, force_baseline);    // 0x004ae6d9
    kJpegAddQuantTable(cinfo, 1, kStdChrominanceQuantTbl, scale_factor, force_baseline);  // 0x004ae6e8
}

// 0x004af0a0  out_of_memory(cinfo, which): raise the libjpeg out-of-memory error, naming the allocator that
// failed. The compress object is read at 0x004af0a0 and pushed as the only argument of the error handler
// (0x004af0a4); the error manager is re-read from cinfo+0 before each store (0x004af0a5, 0x004af0ae,
// 0x004af0b7). The message code 0x35 goes to err+0x14 (0x004af0a7) and the caller's second dword, read at
// 0x004af0b0, to the first message parameter err+0x18 (0x004af0b4). err+0 - error_exit - is then called with
// the compress object (0x004af0b9) and the caller pops its dword (0x004af0bb). The function has no
// conditional branch; libjpeg's own error_exit does not return, but nothing here depends on that.
typedef void(__cdecl* OutOfMemory_t)(void* cinfo, int which);
OutOfMemory_t OutOfMemory_orig;
void __cdecl OutOfMemory_re(void* cinfo, int which) {
    field_i(field_ptr(cinfo, 0), 0x14) = 0x35;                                  // 0x004af0a5 / 0x004af0a7
    field_i(field_ptr(cinfo, 0), 0x18) = which;                                 // 0x004af0ae / 0x004af0b4
    F<ErrorExit_t>(field_ptr(cinfo, 0), 0)(cinfo);                              // 0x004af0b7 / 0x004af0b9
}

// 0x004b0490  jpeg_open_backing_store(cinfo, info, total_bytes_needed): refuse to open a backing-store file.
// The compress object is read at 0x004b0490 and pushed as the only argument of the error handler
// (0x004b0494); the error manager is read from cinfo+0 twice (0x004b0495, 0x004b049e), the message code 0x30
// is stored in err+0x14 (0x004b0497) and err+0 - error_exit - is called with the compress object
// (0x004b04a0), the caller popping its dword (0x004b04a2). No instruction reads the second or third dword of
// the argument list, and no message parameter is set.
typedef void(__cdecl* OpenBackingStore_t)(void* cinfo, void* info, long total_bytes_needed);
OpenBackingStore_t OpenBackingStore_orig;
void __cdecl OpenBackingStore_re(void* cinfo, void* info, long total_bytes_needed) {
    (void)info;
    (void)total_bytes_needed;
    field_i(field_ptr(cinfo, 0), 0x14) = 0x30;                                  // 0x004b0495 / 0x004b0497
    F<ErrorExit_t>(field_ptr(cinfo, 0), 0)(cinfo);                              // 0x004b049e / 0x004b04a0
}

// 0x004b0480  jpeg_mem_available(cinfo, min_bytes_needed, max_bytes_needed, already_allocated): report how
// much memory the allocator may still hand out. The body is one instruction: the THIRD dword of the argument
// list is loaded into eax (0x004b0480) and returned (0x004b0484), so this build always grants the caller the
// whole maximum it asked for. The compress object, the minimum and the running total are not read, and no
// memory is written.
typedef long(__cdecl* MemAvailable_t)(void* cinfo, long min_bytes_needed, long max_bytes_needed,
                                      long already_allocated);
MemAvailable_t MemAvailable_orig;
long __cdecl MemAvailable_re(void* cinfo, long min_bytes_needed, long max_bytes_needed,
                             long already_allocated) {
    (void)cinfo;
    (void)min_bytes_needed;
    (void)already_allocated;
    return max_bytes_needed;                                                    // 0x004b0480 / 0x004b0484
}

// 0x004ae210  jpeg_destroy_compress(cinfo): tear the compress object down through the common destructor. The
// caller's dword is read at 0x004ae210 and pushed as the only argument of jpeg_destroy 0x004afa90
// (0x004ae214 / 0x004ae215), which releases the memory manager through its self_destruct at mem+0x28 and
// clears cinfo+4 and cinfo+0x10; this function pops that dword (0x004ae21a) and adds nothing of its own.
typedef void(__cdecl* DestroyCompress_t)(void* cinfo);
DestroyCompress_t DestroyCompress_orig;
void __cdecl DestroyCompress_re(void* cinfo) {
    kJpegDestroy(cinfo);                                                        // 0x004ae215
}

}  // namespace

// 0x00487460  releaseWithHookA
SG_HOOK("golf_clean.exe", 0x00487460, releaseWithHookA, ReleaseWithHookA_re, ReleaseWithHookA_orig);
// 0x00487ac0  releaseWithHookB
SG_HOOK("golf_clean.exe", 0x00487ac0, releaseWithHookB, ReleaseWithHookB_re, ReleaseWithHookB_orig);
// 0x00487bd0  closeHookC
SG_HOOK("golf_clean.exe", 0x00487bd0, closeHookC, CloseHookC_re, CloseHookC_orig);
// 0x00496770  orderPair
SG_HOOK("golf_clean.exe", 0x00496770, orderPair, OrderPair_re, OrderPair_orig);
// 0x004967d0  scrollbarResetThumb
SG_HOOK("golf_clean.exe", 0x004967d0, scrollbarResetThumb, ScrollbarResetThumb_re, ScrollbarResetThumb_orig);
// 0x004ae6c0  jpeg_set_linear_quality
SG_HOOK("golf_clean.exe", 0x004ae6c0, jpeg_set_linear_quality, SetLinearQuality_re, SetLinearQuality_orig);
// 0x004af0a0  out_of_memory
SG_HOOK("golf_clean.exe", 0x004af0a0, out_of_memory, OutOfMemory_re, OutOfMemory_orig);
// 0x004b0490  jpeg_open_backing_store
SG_HOOK("golf_clean.exe", 0x004b0490, jpeg_open_backing_store, OpenBackingStore_re, OpenBackingStore_orig);
// 0x004b0480  jpeg_mem_available
SG_HOOK("golf_clean.exe", 0x004b0480, jpeg_mem_available, MemAvailable_re, MemAvailable_orig);
// 0x004ae210  jpeg_destroy_compress
SG_HOOK("golf_clean.exe", 0x004ae210, jpeg_destroy_compress, DestroyCompress_re, DestroyCompress_orig);
