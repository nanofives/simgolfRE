// C3 batch c3bx (round 18, 2026-10-08) of golf_clean.exe. Eleven small functions that dispatch through one
// pointer each: an IJG libjpeg 6a output-buffer refill and the quality entry point, four engine objects that
// release a child through a vtable slot or a global hook, and the four Window forwarders that hand a scalar or
// a (low, high) pair to the scrollbar child at +0x26c / +0x270.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees run through their original addresses:
//   jpeg_set_quality     -> jpeg_quality_scaling 0x004ae700 (C3) and jpeg_set_linear_quality 0x004ae6c0 (C2,
//                           whose jpeg_add_quant_table 0x004ae600 is C3 and whose
//                           jpeg_alloc_quant_table 0x004afab0 is C3)
//   fwdScroll26c/270     -> setWidgetScalar 0x004967f0 (C3)
//   fwdRange26c/270      -> orderPair 0x00496770 (C2, whose swapInts 0x00493580 is C3)
// The vtable slots the other seven dispatch through (+0x14, +8, +0xd8, +0x120, dest+0xc) and the two global
// hook pointers 0x0083af80 / 0x0083af88 are the fixture's own callbacks, not hooks.
//
//   dump_buffer 0x004b19e0   jpeg_set_quality 0x004ae740   StrList::dtor 0x00487260
//   releaseWithHookC 0x00487c40   comboListQuery 0x00493ed0
//   closeHookA 0x00487430   closeHookB 0x004879f0
//   Window::fwdScroll26c 0x0047b9f0   Window::fwdScroll270 0x0047ba10
//   Window::fwdRange26c 0x0047ba30    Window::fwdRange270 0x0047ba50
#include "hooks.h"

namespace {

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
// `mov eax, [obj]` then `call/jmp [eax+off]`: the object's vtable pointer is at +0, the slot at +off.
static inline void* vslot(void* obj, unsigned off) {
    return field_ptr(field_ptr(obj, 0), off);
}

// Callees reached through their original addresses.
typedef int(__cdecl* QualityScaling_t)(int quality);
const QualityScaling_t kJpegQualityScaling = reinterpret_cast<QualityScaling_t>(0x004ae700);     // C3
typedef void(__cdecl* SetLinearQuality_t)(void* cinfo, int scale_factor, int force_baseline);
const SetLinearQuality_t kJpegSetLinearQuality = reinterpret_cast<SetLinearQuality_t>(0x004ae6c0);  // C2
typedef void(__fastcall* SetWidgetScalar_t)(void* ecx, void* edx, int value);
const SetWidgetScalar_t kSetWidgetScalar = reinterpret_cast<SetWidgetScalar_t>(0x004967f0);     // C3
typedef void(__fastcall* OrderPair_t)(void* ecx, void* edx, unsigned a, unsigned b);
const OrderPair_t kOrderPair = reinterpret_cast<OrderPair_t>(0x00496770);                       // C2

// ---- 0x004b19e0  dump_buffer ----------------------------------------------------------------------------
// IJG libjpeg 6a jchuff.c working_state: next_output_byte +0, free_in_buffer +4, cur.put_buffer +8,
// cur.put_bits +0xc, cur.last_dc_val[4] +0x10, cinfo +0x20 (read at 0x004b19e6). The compress object's
// destination manager sits at cinfo+0x14 (0x004b19ea) and its empty_output_buffer at dest+0xc (0x004b19ed);
// the manager's own next_output_byte is dest+0 and free_in_buffer dest+4 (0x004b19fa / 0x004b19fe).
//
// 0x004b19e0  dump_buffer(working_state* state): hand the compress object to the destination manager's
// empty_output_buffer, and on success copy the manager's refilled cursor and count back into the
// working state.
typedef int(__cdecl* EmptyOutputBuffer_t)(void* cinfo);
typedef int(__cdecl* DumpBuffer_t)(void* state);
DumpBuffer_t DumpBuffer19e0_orig;
int __cdecl DumpBuffer19e0_re(void* statep) {
    char* state = reinterpret_cast<char*>(statep);
    void* cinfo = field_ptr(statep, 0x20);                                      // 0x004b19e6
    char* dest = reinterpret_cast<char*>(field_ptr(cinfo, 0x14));               // 0x004b19ea
    EmptyOutputBuffer_t empty = reinterpret_cast<EmptyOutputBuffer_t>(field_ptr(dest, 0xc));
    if (empty(cinfo) == 0) return 0;                                            // 0x004b19ed / 0x004b19f5
    *reinterpret_cast<void**>(state + 0) = *reinterpret_cast<void**>(dest + 0); // 0x004b19fa / 0x004b19fc
    *reinterpret_cast<unsigned*>(state + 4) =
        *reinterpret_cast<unsigned*>(dest + 4);                                 // 0x004b19fe / 0x004b1a01
    return 1;                                                                   // 0x004b1a05
}

// ---- 0x004ae740  jpeg_set_quality -----------------------------------------------------------------------
// 0x004ae740  jpeg_set_quality(cinfo, quality, force_baseline): turn the 0..100 user rating into the
// percentage scale factor and install the two standard quantisation tables at that scale. The rating is read
// from the second stack slot and handed to jpeg_quality_scaling 0x004ae700 (0x004ae740 / 0x004ae745); its
// result, the compress object from the first slot and force_baseline from the third are then pushed in that
// order for jpeg_set_linear_quality 0x004ae6c0 (0x004ae74a..0x004ae755), and the four dwords are cleaned off
// by the caller-pops `add esp, 0x10` at 0x004ae75a. The function has no conditional branch of its own.
typedef void(__cdecl* SetQuality_t)(void* cinfo, int quality, int force_baseline);
SetQuality_t SetQuality_orig;
void __cdecl SetQuality_re(void* cinfo, int quality, int force_baseline) {
    int scaled = kJpegQualityScaling(quality);                                  // 0x004ae745
    kJpegSetLinearQuality(cinfo, scaled, force_baseline);                       // 0x004ae755
}

// ---- 0x00487260  StrList::dtor --------------------------------------------------------------------------
// 0x00487260  StrList::dtor(this): stamp the base vtable back into the object and release the child object
// held at +0x14 through that child's own vtable slot +0x14. The vtable pointer 0x004baeac is stored first
// (0x00487260); the child is read at 0x00487266 and a null one ends the function at 0x00487272. A non-null
// child is dispatched with a tail jump (0x0048726f), so the child's slot +0x14 returns straight to this
// function's caller, with the child in ecx.
typedef void(__fastcall* ChildRelease_t)(void* ecx, void* edx);
typedef void(__fastcall* StrListDtor_t)(void* ecx, void* edx);
StrListDtor_t StrListDtor_orig;
void __fastcall StrListDtor_re(void* thisp, void*) {
    char* self = reinterpret_cast<char*>(thisp);
    *reinterpret_cast<void**>(self + 0) = reinterpret_cast<void*>(0x004baeac);  // 0x00487260
    void* child = field_ptr(thisp, 0x14);                                       // 0x00487266
    if (child == 0) return;                                                     // 0x0048726b
    reinterpret_cast<ChildRelease_t>(vslot(child, 0x14))(child, 0);         // 0x0048726d / 0x0048726f
}

// ---- 0x00487c40  releaseWithHookC -----------------------------------------------------------------------
// 0x00487c40  releaseWithHookC(this): release the child held at +0x14 and tell this object about it. The
// child is read at 0x00487c43 and a null one leaves everything untouched (0x00487c48). Otherwise the child's
// vtable slot +0x14 runs with the child in ecx (0x00487c4a / 0x00487c4c), then THIS object's vtable is
// re-read at 0x00487c4f - after that call, so a slot +0x14 that replaces this->vtbl changes which slot +8
// runs - and its slot +8 is called with this in ecx (0x00487c51 / 0x00487c53). The child field is then
// cleared (0x00487c56). The return value is 0 on both paths (0x00487c5d).
typedef void(__fastcall* SelfNotify_t)(void* ecx, void* edx);
typedef int(__fastcall* ReleaseWithHookC_t)(void* ecx, void* edx);
ReleaseWithHookC_t ReleaseWithHookC_orig;
int __fastcall ReleaseWithHookC_re(void* thisp, void*) {
    char* self = reinterpret_cast<char*>(thisp);
    void* child = field_ptr(thisp, 0x14);                                       // 0x00487c43
    if (child != 0) {                                                           // 0x00487c48
        reinterpret_cast<ChildRelease_t>(vslot(child, 0x14))(child, 0);         // 0x00487c4a / 0x00487c4c
        reinterpret_cast<SelfNotify_t>(vslot(thisp, 8))(thisp, 0);              // 0x00487c4f / 0x00487c53
        *reinterpret_cast<int*>(self + 0x14) = 0;                               // 0x00487c56
    }
    return 0;                                                                   // 0x00487c5d
}

// ---- 0x00493ed0  comboListQuery -------------------------------------------------------------------------
// 0x00493ed0  comboListQuery(this): ask the sub-object held at +0x23b4 for a value through its vtable slot
// +0xd8. The sub-object is read at 0x00493ed0 and a null one returns 0 (0x00493ed8 / 0x00493ee2). A non-null
// one is dispatched with a tail jump (0x00493edc) with the SUB-OBJECT in ecx, so slot +0xd8 supplies this
// function's return value directly.
typedef int(__fastcall* SubQuery_t)(void* ecx, void* edx);
typedef int(__fastcall* ComboListQuery_t)(void* ecx, void* edx);
ComboListQuery_t ComboListQuery_orig;
int __fastcall ComboListQuery_re(void* thisp, void*) {
    void* sub = field_ptr(thisp, 0x23b4);                                       // 0x00493ed0
    if (sub == 0) return 0;                                                     // 0x00493ed8 / 0x00493ee2
    return reinterpret_cast<SubQuery_t>(vslot(sub, 0xd8))(sub, 0);              // 0x00493eda / 0x00493edc
}

// ---- 0x00487430  closeHookA / 0x004879f0  closeHookB ----------------------------------------------------
// Two copies of one body that differ only in the global they call: 0x0083af80 for closeHookA (read at
// 0x0048743a) and 0x0083af88 for closeHookB (read at 0x004879fa). Both are called with no arguments and no
// stack cleanup (0x00487443 / 0x00487a03).
typedef void(__cdecl* CloseHookFn_t)();
typedef int(__fastcall* CloseHook_t)(void* ecx, void* edx);

// 0x00487430  closeHookA(this): run the global close hook 0x0083af80 for an object that still holds a
// resource and mark the object closed. The handle at +0x14 is read at 0x00487433; a zero handle
// (0x00487438) or a null hook (0x00487441) skips straight to the failure value 0x14 at 0x00487450. When both
// are set the hook runs (0x00487443), the handle is cleared (0x00487445) and the function returns 0
// (0x0048744c).
CloseHook_t CloseHookA_orig;
int __fastcall CloseHookA_re(void* thisp, void*) {
    char* self = reinterpret_cast<char*>(thisp);
    if (field_i(thisp, 0x14) != 0) {                                            // 0x00487433 / 0x00487438
        CloseHookFn_t hook = *reinterpret_cast<CloseHookFn_t*>(0x0083af80);     // 0x0048743a
        if (hook != 0) {                                                        // 0x00487441
            hook();                                                             // 0x00487443
            *reinterpret_cast<int*>(self + 0x14) = 0;                           // 0x00487445
            return 0;                                                           // 0x0048744c
        }
    }
    return 0x14;                                                                // 0x00487450
}

// 0x004879f0  closeHookB(this): the same body against the global close hook 0x0083af88. The handle at +0x14
// is read at 0x004879f3; a zero handle (0x004879f8) or a null hook (0x00487a01) returns 0x14 at 0x00487a10,
// and otherwise the hook runs (0x00487a03), the handle is cleared (0x00487a05) and the result is 0
// (0x00487a0c).
CloseHook_t CloseHookB_orig;
int __fastcall CloseHookB_re(void* thisp, void*) {
    char* self = reinterpret_cast<char*>(thisp);
    if (field_i(thisp, 0x14) != 0) {                                            // 0x004879f3 / 0x004879f8
        CloseHookFn_t hook = *reinterpret_cast<CloseHookFn_t*>(0x0083af88);     // 0x004879fa
        if (hook != 0) {                                                        // 0x00487a01
            hook();                                                             // 0x00487a03
            *reinterpret_cast<int*>(self + 0x14) = 0;                           // 0x00487a05
            return 0;                                                           // 0x00487a0c
        }
    }
    return 0x14;                                                                // 0x00487a10
}

// ---- 0x0047b9f0 / 0x0047ba10 / 0x0047ba30 / 0x0047ba50  the four Window forwarders -----------------------
// Four copies of two bodies. Each reads one child pointer - +0x26c (0x0047b9f0, 0x0047ba30) or +0x270
// (0x0047ba10, 0x0047ba50) - skips everything when it is null, and otherwise forwards its stack arguments to
// the child with the child in ecx. The scalar pair pops 4 bytes (`ret 4` at 0x0047ba04 / 0x0047ba24) and the
// range pair pops 8 (`ret 8` at 0x0047ba49 / 0x0047ba69).
typedef void(__fastcall* FwdScalar_t)(void* ecx, void* edx, int value);
typedef void(__fastcall* FwdRange_t)(void* ecx, void* edx, unsigned a, unsigned b);

// 0x0047b9f0  Window::fwdScroll26c(this, value): give the child at +0x26c the scalar the caller supplied.
// The child is read at 0x0047b9f0, a null one returns at 0x0047ba04 (0x0047b9f8), and the argument from
// [esp+4] is pushed for setWidgetScalar 0x004967f0 at 0x0047b9fa / 0x0047b9ff.
FwdScalar_t FwdScroll26c_orig;
void __fastcall FwdScroll26c_re(void* thisp, void*, int value) {
    void* child = field_ptr(thisp, 0x26c);                                      // 0x0047b9f0
    if (child == 0) return;                                                     // 0x0047b9f8
    kSetWidgetScalar(child, 0, value);                                          // 0x0047b9ff
}

// 0x0047ba10  Window::fwdScroll270(this, value): the same forward for the child at +0x270 (read at
// 0x0047ba10, null exit at 0x0047ba18, argument from [esp+4] at 0x0047ba1a and the call at 0x0047ba1f).
FwdScalar_t FwdScroll270_orig;
void __fastcall FwdScroll270_re(void* thisp, void*, int value) {
    void* child = field_ptr(thisp, 0x270);                                      // 0x0047ba10
    if (child == 0) return;                                                     // 0x0047ba18
    kSetWidgetScalar(child, 0, value);                                          // 0x0047ba1f
}

// 0x0047ba30  Window::fwdRange26c(this, a, b): give the child at +0x26c the two scalars the caller supplied,
// in the caller's order. The child is read at 0x0047ba30 and a null one returns at 0x0047ba49 (0x0047ba38);
// [esp+8] is loaded into eax and [esp+4] into edx (0x0047ba3a / 0x0047ba3e) and pushed eax first
// (0x0047ba42 / 0x0047ba43), so orderPair 0x00496770 receives [esp+4] as its first argument.
FwdRange_t FwdRange26c_orig;
void __fastcall FwdRange26c_re(void* thisp, void*, unsigned a, unsigned b) {
    void* child = field_ptr(thisp, 0x26c);                                      // 0x0047ba30
    if (child == 0) return;                                                     // 0x0047ba38
    kOrderPair(child, 0, a, b);                                                 // 0x0047ba44
}

// 0x0047ba50  Window::fwdRange270(this, a, b): the same forward for the child at +0x270 (read at 0x0047ba50,
// null exit at 0x0047ba58, the two arguments at 0x0047ba5a / 0x0047ba5e and the call at 0x0047ba64).
FwdRange_t FwdRange270_orig;
void __fastcall FwdRange270_re(void* thisp, void*, unsigned a, unsigned b) {
    void* child = field_ptr(thisp, 0x270);                                      // 0x0047ba50
    if (child == 0) return;                                                     // 0x0047ba58
    kOrderPair(child, 0, a, b);                                                 // 0x0047ba64
}

}  // namespace

// 0x004b19e0  dump_buffer
SG_HOOK("golf_clean.exe", 0x004b19e0, dump_buffer_4b19e0, DumpBuffer19e0_re, DumpBuffer19e0_orig);
// 0x004ae740  jpeg_set_quality
SG_HOOK("golf_clean.exe", 0x004ae740, jpeg_set_quality, SetQuality_re, SetQuality_orig);
// 0x00487260  StrList::dtor
SG_HOOK("golf_clean.exe", 0x00487260, StrList_dtor, StrListDtor_re, StrListDtor_orig);
// 0x00487c40  releaseWithHookC
SG_HOOK("golf_clean.exe", 0x00487c40, releaseWithHookC, ReleaseWithHookC_re, ReleaseWithHookC_orig);
// 0x00493ed0  comboListQuery
SG_HOOK("golf_clean.exe", 0x00493ed0, comboListQuery, ComboListQuery_re, ComboListQuery_orig);
// 0x00487430  closeHookA
SG_HOOK("golf_clean.exe", 0x00487430, closeHookA, CloseHookA_re, CloseHookA_orig);
// 0x004879f0  closeHookB
SG_HOOK("golf_clean.exe", 0x004879f0, closeHookB, CloseHookB_re, CloseHookB_orig);
// 0x0047b9f0  Window::fwdScroll26c
SG_HOOK("golf_clean.exe", 0x0047b9f0, Window_fwdScroll26c, FwdScroll26c_re, FwdScroll26c_orig);
// 0x0047ba10  Window::fwdScroll270
SG_HOOK("golf_clean.exe", 0x0047ba10, Window_fwdScroll270, FwdScroll270_re, FwdScroll270_orig);
// 0x0047ba30  Window::fwdRange26c
SG_HOOK("golf_clean.exe", 0x0047ba30, Window_fwdRange26c, FwdRange26c_re, FwdRange26c_orig);
// 0x0047ba50  Window::fwdRange270
SG_HOOK("golf_clean.exe", 0x0047ba50, Window_fwdRange270, FwdRange270_re, FwdRange270_orig);
