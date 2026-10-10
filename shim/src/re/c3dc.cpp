// Batch c3dc (round 24, jgld.dll): C2 rows earlier batches dropped for the heap, for drawing or for live state,
// now A/B-able with the heap stand-in (re/frida/js/heapstandin.js), the shared frozen-thread helper
// (re/frida/js/freeze.js) and synthetic objects on jgld's own vtables.
//
//   group T - the tracked-object list 0x10128428 and the classes registered in it (c3cw's swap of that list)
//     LinkedList::add            0x00006f40     Tracked::ctor              0x00006ab0
//     Tracked::deleteAll         0x00006bf0     atexitDestroyGlobalList    0x00006a40
//     SurfaceBase::dtor          0x000096f0     SpriteBase::~SpriteBase    0x000168e0
//     DisplayBase::dtor          0x00066d20     DisplayBase::deletingDtor  0x000674f0
//     Lib::destroyDisplay        0x00065420
//   group S - Sprite::release    0x00014eb0     (two module counters, two frees)
//   group C - the three helpers that ask the module Display 0x10128420 for its pixel format (slot +0xb4)
//     makeColor 0x0000a660       redOf 0x0000e440       greenOf 0x0000e4c0
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form the
// debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from
// the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp). jgld.dll is relocated at run time, so every module
// address (vtables, globals, callees, the import slot) is resolved against the running base. __thiscall is
// emulated with __fastcall: ecx = this, edx unused. Callees are called through their original addresses (the hook
// trampoline when another batch hooks them).
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
template <class T> T& G(unsigned va) { return *static_cast<T*>(JgldVA(va)); }
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }

void SetVptr(void* self, unsigned vtableVA) { *static_cast<void**>(self) = JgldVA(vtableVA); }

typedef void(__fastcall* Method_t)(void*, void*);              // __thiscall, no stack argument
typedef void*(__fastcall* DeletingDtor_t)(void*, void*, unsigned);
typedef void(__fastcall* Dtor_t)(void*, void*);
typedef void(__cdecl* OperatorDelete_t)(void*);                // 0x1007e6d0 operator delete
typedef void*(__cdecl* OperatorNew_t)(unsigned);               // 0x1007f0f0 operator new
typedef void(__cdecl* Free_t)(void*);                          // 0x10014a90 jfree

void CallMethod(unsigned va, void* self) { reinterpret_cast<Method_t>(JgldVA(va))(self, 0); }

// The module's tracked-object list (re/names/jgld_0_globals.tsv g_trackedList), a LinkedList:
//   +0x00 vtable  +0x04 data of the current node  +0x08 flag byte of the current node  +0x0c count
//   +0x10 head    +0x14 current node              +0x18 the "ordered" word add() refuses to mix with
// A node (ListNode, vtable 0x1011d098 written by ListNode::ctor 0x10007370 at 0x10007390):
//   +0x04 prev  +0x08 next  +0x0c data  +0x10 flag byte
const unsigned kTrackedList = 0x10128428;

// ===================================================================================== group T

// 0x00006f40  LinkedList::add(data, flag)
// __thiscall with two stack arguments (`ret 8` at 0x1000709a); the flag is a byte (`mov al, [ebp+0xc]` at
// 0x10006fbc). The list is the object at ecx.
//  1. When the list holds nodes (+0xc != 0, cmp at 0x10006f75 / je 0x10006f79) AND its +0x18 word is set (cmp at
//     0x10006f7e / je 0x10006f82), the debug message at 0x1011d060 goes to the import at [0x1012c42c]
//     (OutputDebugStringA, stdcall, call at 0x10006f8b). Either way +0x18 is cleared next (0x10006f9b).
//  2. operator new 0x1007f0f0 for 0x14 bytes (push 0x14 at 0x10006fa2); a non-NULL block (cmp at 0x10006fb6 /
//     je 0x10006fba) is constructed by ListNode::ctor 0x10007370 (thunk 0x1000165e, call at 0x10006fc7) with
//     (data, flag), whose result is the node; a NULL block makes the node NULL (0x10006fd1), and every path after
//     that writes through the node, so the original faults there: not reproduced as a separate path.
//  3. Linking: with a head (cmp [list+0x10] at 0x10006fee / je 0x10006ff2) the node goes in before the head
//     (node.prev = head.prev 0x10007000, node.next = head 0x1000700c, head.prev = node 0x10007018,
//     node.prev.next = node 0x10007024); without one it becomes the head and links to itself (0x1000702f,
//     0x1000703e, 0x1000704d).
//  4. count + 1 (0x1000705c), current = node (0x10007065), +4 = data (0x1000706e), +8 = the node's flag byte
//     (0x1000707d). No return value is set (eax is the flag byte's register pair at the end; callers ignore it).
typedef void(__stdcall* DebugString_t)(const char*);
typedef void*(__fastcall* NodeCtor_t)(void*, void*, void*, int);
typedef void(__fastcall* Add_t)(void*, void*, void*, int);
Add_t ListAdd_orig;
void __fastcall ListAdd_re(void* list, void*, void* data, int flag) {
    if (F<int>(list, 0xc) != 0 && F<int>(list, 0x18) != 0)
        G<DebugString_t>(0x1012c42c)(static_cast<const char*>(JgldVA(0x1011d060)));
    F<int>(list, 0x18) = 0;
    void* block = reinterpret_cast<OperatorNew_t>(JgldVA(0x1007f0f0))(0x14);
    void* node = 0;
    if (block)
        node = reinterpret_cast<NodeCtor_t>(JgldVA(0x10007370))(block, 0, data, static_cast<char>(flag));
    void* head = F<void*>(list, 0x10);
    if (head) {
        F<void*>(node, 4) = F<void*>(head, 4);
        F<void*>(node, 8) = head;
        F<void*>(head, 4) = node;
        F<void*>(F<void*>(node, 4), 8) = node;
    } else {
        F<void*>(list, 0x10) = node;
        F<void*>(node, 4) = node;
        F<void*>(node, 8) = node;
    }
    F<int>(list, 0xc) += 1;
    F<void*>(list, 0x14) = node;
    F<void*>(list, 4) = data;
    F<char>(list, 8) = F<char>(node, 0x10);
}

// 0x00006ab0  Tracked::ctor
// Writes the Tracked vtable 0x1011d058 at [this] (0x10006ad0), then registers the object in the module's
// tracked-object list: LinkedList::add 0x10006f40 on 0x10128428 (`mov ecx, 0x10128428` at 0x10006adc, thunk
// 0x1000114f, call at 0x10006ae1) with (this, 0) (pushes at 0x10006ad6 / 0x10006adb). Returns this (0x10006ae6).
typedef void*(__fastcall* Ctor_t)(void*, void*);
Ctor_t TrackedCtor_orig;
void* __fastcall TrackedCtor_re(void* self, void*) {
    SetVptr(self, 0x1011d058);
    reinterpret_cast<Add_t>(JgldVA(0x10006f40))(JgldVA(kTrackedList), 0, self, 0);
    return self;
}

// 0x00006bf0  Tracked::deleteAll
// No `this` and no argument (`ret` at 0x10006c5d; ecx is loaded with the list before any read). Loops while
// LinkedList::count 0x10006d40 on 0x10128428 (thunk 0x1000105f, call at 0x10006c0d) is not zero (test at
// 0x10006c12 / je 0x10006c14): reads the list's current data word 0x1012842c (0x10006c16) and, when it is not
// NULL (cmp at 0x10006c24 / je 0x10006c28), calls slot 0 of that object's vtable with 1 (push 1 at 0x10006c2c,
// `call [eax]` at 0x10006c36): the scalar deleting destructor, which for every Tracked class unregisters the
// object (Tracked::dtor 0x10006b80's find + removeCurrent) and so shrinks the list. When the data word is NULL
// nothing in the loop changes the count, so the original spins there forever; that side is not reproduced as a
// path of its own (the loop below is the same loop).
typedef int(__fastcall* Count_t)(void*, void*);
typedef void*(__fastcall* Slot0_t)(void*, void*, int);
typedef void(__cdecl* Void_t)();
Void_t DeleteAll_orig;
void __cdecl DeleteAll_re() {
    for (;;) {
        if (reinterpret_cast<Count_t>(JgldVA(0x10006d40))(JgldVA(kTrackedList), 0) == 0)
            break;
        void* obj = G<void*>(0x1012842c);
        if (obj != 0)
            (*static_cast<Slot0_t*>(*static_cast<void**>(obj)))(obj, 0, 1);
    }
}

// 0x00006a40  atexitDestroyGlobalList
// No argument (`ret` at 0x10006a8f). Reads the byte 0x10128444 (0x10006a5a); when bit 0 is set (and/test at
// 0x10006a5f / jne 0x10006a64) it returns at once. Otherwise it sets bit 0 in that byte (0x10006a6c /
// 0x10006a6f, the other seven bits kept) and runs LinkedList::dtor 0x10006cf0 on 0x10128428 (`mov ecx,
// 0x10128428` at 0x10006a75, thunk 0x10001979, call at 0x10006a7a), which writes the LinkedList vtable
// 0x1011d05c at +0 and clears the list.
Void_t AtexitDestroy_orig;
void __cdecl AtexitDestroy_re() {
    unsigned char& flag = G<unsigned char>(0x10128444);
    if (flag & 1)
        return;
    flag = static_cast<unsigned char>(flag | 1);
    CallMethod(0x10006cf0, JgldVA(kTrackedList));
}

// 0x000096f0  SurfaceBase::dtor
// Writes the SurfaceBase vtable 0x1011d220 at [this] (0x10009710), then Tracked::dtor 0x10006b80 on the same
// object (thunk 0x1000195b, call at 0x10009719): the Tracked part sits at offset 0. Returns nothing.
Dtor_t SurfaceBaseDtor_orig;
void __fastcall SurfaceBaseDtor_re(void* self, void*) {
    SetVptr(self, 0x1011d220);
    CallMethod(0x10006b80, self);
}

// 0x000168e0  SpriteBase::~SpriteBase
// Writes the SpriteBase vtable 0x1011d444 at [this] (0x10016900), then Tracked::dtor 0x10006b80 (thunk
// 0x1000195b, call at 0x10016909). Returns nothing.
Dtor_t SpriteBaseDtor_orig;
void __fastcall SpriteBaseDtor_re(void* self, void*) {
    SetVptr(self, 0x1011d444);
    CallMethod(0x10006b80, self);
}

// 0x00066d20  DisplayBase::dtor
// One store: the DisplayBase vtable 0x1011d7dc at [this] (0x10066d40). No call (no __chkesp either), no other
// field touched, no return value.
Dtor_t DisplayBaseDtor_orig;
void __fastcall DisplayBaseDtor_re(void* self, void*) {
    SetVptr(self, 0x1011d7dc);
}

// 0x000674f0  DisplayBase::deletingDtor(flags)
// DisplayBase::dtor 0x10066d20 (thunk 0x10001159, call at 0x10067510), then operator delete 0x1007e6d0 on this
// (call at 0x10067523) when bit 0 of the flags is set (`and eax, 1` 0x10067518, je 0x1006751d); returns this
// (0x1006752b, `ret 4` at 0x1006753e). The other 31 bits are not read.
DeletingDtor_t DisplayBaseDeletingDtor_orig;
void* __fastcall DisplayBaseDeletingDtor_re(void* self, void*, unsigned flags) {
    CallMethod(0x10066d20, self);
    if (flags & 1)
        reinterpret_cast<OperatorDelete_t>(JgldVA(0x1007e6d0))(self);
    return self;
}

// 0x00065420  Lib::destroyDisplay
// __thiscall with no stack argument (`ret` at 0x10065487); `this` is stored (0x1006543a) and never read again.
// Tracked::deleteAll 0x10006bf0 (thunk 0x10001aaf, call at 0x1006543d), then the Display pointer at 0x1012870c
// (g_display, 0x10065442): when it is not NULL (cmp at 0x10065450 / je 0x10065454) slot 0 of its vtable is called
// with 1 (push 1 at 0x10065458, `call [eax]` at 0x10065462), the scalar deleting destructor. The global itself is
// not written. Returns nothing.
Dtor_t DestroyDisplay_orig;
void __fastcall DestroyDisplay_re(void*, void*) {
    reinterpret_cast<Void_t>(JgldVA(0x10006bf0))();
    void* display = G<void*>(0x1012870c);
    if (display != 0)
        (*static_cast<Slot0_t*>(*static_cast<void**>(display)))(display, 0, 1);
}

// ===================================================================================== group S

// 0x00014eb0  Sprite::release
// Sprite fields (c3ci's table): +0x14 payload, +0x18 flag word (bit 0: the payload is the +0x24-byte span form),
// +0x1c a second block, +0x24 span byte count, +0x2c pitch, +0x30 width, +0x34 height.
//  1. When the payload is not NULL (cmp at 0x10014ed0 / je 0x10014ed4):
//     a. when bit 0 of +0x18 is set (and/test at 0x10014edc / je 0x10014ee1), the module counter 0x101284cc is
//        lowered by width * height - [+0x24] - height * 4 (imul 0x10014eec, sub 0x10014ef3, shl 2 0x10014efc,
//        sub 0x10014eff, sub/store 0x10014f07 / 0x10014f09);
//     b. the module counter 0x101284c8 is lowered by width * height (imul 0x10014f18, 0x10014f22 / 0x10014f24);
//     c. jfree 0x10014a90 (thunk 0x100017b7, call at 0x10014f31) on the payload, then +0x14 = 0 (0x10014f3c).
//  2. When +0x1c is not NULL (cmp at 0x10014f46 / je 0x10014f4a): jfree on it (0x10014f53), +0x1c = 0
//     (0x10014f5e).
//  3. +0x2c, +0x30, +0x34, +0x18 and +0x24 are cleared (0x10014f68..0x10014f90). Returns nothing.
// Products and differences are 32-bit two's complement (imul / sub), as below with unsigned arithmetic.
Dtor_t SpriteRelease_orig;
void __fastcall SpriteRelease_re(void* self, void*) {
    Free_t jfree = reinterpret_cast<Free_t>(JgldVA(0x10014a90));
    if (F<void*>(self, 0x14) != 0) {
        const unsigned w = F<unsigned>(self, 0x30), h = F<unsigned>(self, 0x34);
        if (F<unsigned>(self, 0x18) & 1)
            G<unsigned>(0x101284cc) -= w * h - F<unsigned>(self, 0x24) - (h << 2);
        G<unsigned>(0x101284c8) -= w * h;
        jfree(F<void*>(self, 0x14));
        F<void*>(self, 0x14) = 0;
    }
    if (F<void*>(self, 0x1c) != 0) {
        jfree(F<void*>(self, 0x1c));
        F<void*>(self, 0x1c) = 0;
    }
    F<int>(self, 0x2c) = 0;
    F<int>(self, 0x30) = 0;
    F<int>(self, 0x34) = 0;
    F<int>(self, 0x18) = 0;
    F<int>(self, 0x24) = 0;
}

// ===================================================================================== group C

// The three ask the module Display (the pointer at 0x10128420) for its pixel format through slot +0xb4 of its
// vtable (`mov eax, [0x10128420]; mov edx, [eax]; mov ecx, [0x10128420]; call [edx + 0xb4]`), and take the
// RGB565 arm when it returns exactly 1 (`cmp eax, 1` / jne), the RGB555 arm otherwise. All shifts are `sar`.
typedef int(__fastcall* FormatSlot_t)(void*, void*);
int DisplayFormat() {
    void* display = G<void*>(0x10128420);
    void* fn = F<void*>(*static_cast<void**>(display), 0xb4);
    return reinterpret_cast<FormatSlot_t>(fn)(display, 0);
}

// 0x0000a660  makeColor(r, g, b)
// cdecl, three arguments (`ret` at 0x1000a71b; [ebp+8], [ebp+0xc], [ebp+0x10]). Slot +0xb4 at 0x1000a687,
// cmp at 0x1000a694 / jne 0x1000a697.
//   1: (b & 0xff) >> 3 | ((g & 0xff) >> 2) << 5 | ((r & 0xff) >> 3) << 11 | 0x80000000   (0x1000a699..0x1000a6cc)
//   else: (b & 0xff) >> 3 | ((g & 0xff) >> 3) << 5 | ((r & 0xff) >> 3) << 10 | 0x80000000 (0x1000a6d3..0x1000a706)
// (the `and ecx, 0xffff` after each green shift, 0x1000a6b0 / 0x1000a6ea, cannot change a value below 0x40.)
typedef int(__cdecl* MakeColor_t)(int, int, int);
MakeColor_t MakeColor_orig;
int __cdecl MakeColor_re(int r, int g, int b) {
    if (DisplayFormat() == 1)
        return static_cast<int>(((b & 0xff) >> 3) | (((g & 0xff) >> 2) << 5) | (((r & 0xff) >> 3) << 11) |
                                0x80000000u);
    return static_cast<int>(((b & 0xff) >> 3) | (((g & 0xff) >> 3) << 5) | (((r & 0xff) >> 3) << 10) |
                            0x80000000u);
}

// 0x0000e440  redOf(colour)
// cdecl, one argument (`ret` at 0x1000e4a1). Slot +0xb4 at 0x1000e467, cmp at 0x1000e474 / jne 0x1000e477.
//   1: (colour sar 8) & 0xf8 (0x1000e47c / 0x1000e47f)      else: (colour sar 7) & 0xf8 (0x1000e489 / 0x1000e48c)
typedef int(__cdecl* Channel_t)(int);
Channel_t RedOf_orig;
int __cdecl RedOf_re(int colour) {
    if (DisplayFormat() == 1)
        return (colour >> 8) & 0xf8;
    return (colour >> 7) & 0xf8;
}

// 0x0000e4c0  greenOf(colour)
// cdecl, one argument (`ret` at 0x1000e521). Slot +0xb4 at 0x1000e4e7, cmp at 0x1000e4f4 / jne 0x1000e4f7.
//   1: (colour sar 3) & 0xfc (0x1000e4fc / 0x1000e4ff)      else: (colour sar 2) & 0xf8 (0x1000e509 / 0x1000e50c)
Channel_t GreenOf_orig;
int __cdecl GreenOf_re(int colour) {
    if (DisplayFormat() == 1)
        return (colour >> 3) & 0xfc;
    return (colour >> 2) & 0xf8;
}

// ===================================================================================== group B
// The three Surface copy dispatchers (c3ce's lead: "first thing to offer the next jgld render batch"). Each picks
// a blitter by bit depth and forwards its stack arguments unchanged; the blitters (all at C3: blit8from8 0x13800
// c3ay, blitKey8to16 0x12900 and blitT_8to8 0x13c50 and blit16from16 0xf6a0 c3bc, blitT_8to16 0x12db0 and
// blitT_16to16 0xed70 c3bh) are called at their module addresses with `this` in ecx. Slot +0xe4 of a Surface
// returns a pointer to its depth word (+0x24); the dispatchers compare the dword it points at.
typedef int*(__fastcall* FormatPtr_t)(void*, void*);
int SurfaceDepth(void* surface) {
    void* fn = F<void*>(*static_cast<void**>(surface), 0xe4);
    return *reinterpret_cast<FormatPtr_t>(fn)(surface, 0);
}
typedef int(__fastcall* Blit8_t)(void*, void*, void*, int, int, int, int, int, int, int);
typedef int(__fastcall* Blit7_t)(void*, void*, void*, int, int, int, int, int, int);
typedef int(__fastcall* Blit4_t)(void*, void*, void*, void*, int, void*);

// 0x00008e50  Surface::blit(other, a2..a8)
// Eight stack arguments (`ret 0x20` at 0x10008f2e). This surface's depth through its slot +0xe4 (call at
// 0x10008e77, compared with 8 at 0x10008e89 / je 0x10008e8d); anything else returns 0x17 (0x10008f19). For depth
// 8 the argument's depth through ITS slot +0xe4 (call at 0x10008e9e): 8 (cmp 0x10008eb0 / je 0x10008eb4) calls
// blit8from8 0x10013800 (thunk 0x1000175d, call at 0x10008ee1), 0x10 (cmp 0x10008eb6 / je 0x10008eba) calls
// blitKey8to16 0x10012900 (thunk 0x100019a1, call at 0x10008f0b), each with all eight arguments, and anything
// else returns 0x17 (0x10008f12). The blitter's result is returned.
Blit8_t SurfaceBlit_orig;
int __fastcall SurfaceBlit_re(void* self, void*, void* other, int a2, int a3, int a4, int a5, int a6, int a7,
                              int a8) {
    if (SurfaceDepth(self) != 8)
        return 0x17;
    switch (SurfaceDepth(other)) {
    case 8:
        return reinterpret_cast<Blit8_t>(JgldVA(0x10013800))(self, 0, other, a2, a3, a4, a5, a6, a7, a8);
    case 0x10:
        return reinterpret_cast<Blit8_t>(JgldVA(0x10012900))(self, 0, other, a2, a3, a4, a5, a6, a7, a8);
    default:
        return 0x17;
    }
}

// 0x000091a0  Surface::blitT(dst, a2..a7)
// Seven stack arguments (`ret 0x1c` at 0x10009293). This surface's depth is read from the field +0x24 directly
// (0x100091c0), not through the slot. 8 (cmp 0x100091c6 / je 0x100091ca): the argument's depth through its slot
// +0xe4 (call at 0x100091e5): 8 (je 0x100091fb) calls blitT_8to8 0x10013c50 (thunk 0x100017f3, call at
// 0x10009224), 0x10 (je 0x10009201) calls blitT_8to16 0x10012db0 (thunk 0x10001186, call at 0x1000924a), else
// 0x17 (0x10009251). 0x10 (cmp 0x100091cc / je 0x100091d0) calls blitT_16to16 0x1000ed70 (thunk 0x1000156e, call
// at 0x10009277) without looking at the argument. Anything else returns 0x17 (0x1000927e).
Blit7_t SurfaceBlitT_orig;
int __fastcall SurfaceBlitT_re(void* self, void*, void* dst, int a2, int a3, int a4, int a5, int a6, int a7) {
    switch (F<int>(self, 0x24)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            return reinterpret_cast<Blit7_t>(JgldVA(0x10013c50))(self, 0, dst, a2, a3, a4, a5, a6, a7);
        case 0x10:
            return reinterpret_cast<Blit7_t>(JgldVA(0x10012db0))(self, 0, dst, a2, a3, a4, a5, a6, a7);
        default:
            return 0x17;
        }
    case 0x10:
        return reinterpret_cast<Blit7_t>(JgldVA(0x1000ed70))(self, 0, dst, a2, a3, a4, a5, a6, a7);
    default:
        return 0x17;
    }
}

// 0x000098f0  Surface::blit16(rect, src, alpha, table)
// Four stack arguments (`ret 0x10` at 0x1000998b). This surface's depth through its slot +0xe4 (call at
// 0x10009917) must be 0x10 (cmp 0x10009929 / je 0x1000992d), else 0x17 (0x10009976); then the SECOND argument's
// depth through its slot +0xe4 (call at 0x1000993b) must be 0x10 (cmp 0x1000994d / je 0x10009951), else 0x17
// (0x1000996f); then blit16from16 0x1000f6a0 (thunk 0x1000161d, call at 0x10009968) with the four arguments.
// The first argument is never dereferenced here.
Blit4_t SurfaceBlit16_orig;
int __fastcall SurfaceBlit16_re(void* self, void*, void* rect, void* src, int alpha, void* table) {
    if (SurfaceDepth(self) != 0x10)
        return 0x17;
    if (SurfaceDepth(src) != 0x10)
        return 0x17;
    return reinterpret_cast<Blit4_t>(JgldVA(0x1000f6a0))(self, 0, rect, src, alpha, table);
}

// ===================================================================================== group D
// Eight Sprite draw dispatchers that c3ci left for batch size (log/c3/c3ci_notes.txt: "the obvious next batch").
// Same frame as c3ci's draw / drawF / drawJ (shim/src/re/c3ci.cpp): the Palette word [this + 0x10] is saved and
// replaced by the last argument when that is not zero, a NULL destination or an invalid sprite (slot +0x20 of the
// Sprite vtable returns 0) leave early, the outer switch is on the bits per pixel [this + 0x20], the inner one on
// the destination's depth (*slot +0xe4), "busy" is Sprite::getFlag18 0x100180e0 (bit 0 of +0x18, the run-length
// form: +0x1c then holds the per-row table and the drawRle16* blitters run), every arm that reaches no blitter is a
// VC6 `_RPTF0(_CRT_WARN, msg)` (_CrtDbgReport 0x10081f90 with the file 0x1011d508, a line relative to a module
// word, NULL, "%s" 0x1011d528 and the message), and the join restores the Palette word, calls slot +0x24 with 1
// and returns the result local. The blitters are called at their module addresses with `this` in ecx.
typedef int(__fastcall* IntSlot_t)(void*, void*);
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);
inline void* VSlot(void* self, unsigned off) { return F<void*>(*static_cast<void**>(self), off); }
inline int SpriteValid(void* self) { return reinterpret_cast<IntSlot_t>(VSlot(self, 0x20))(self, 0); }
inline void SpriteUnlock(void* self) { reinterpret_cast<UnlockSlot_t>(VSlot(self, 0x24))(self, 0, 1); }
inline int SpriteBusy(void* self) { return reinterpret_cast<IntSlot_t>(JgldVA(0x100180e0))(self, 0); }

// _CrtDbgReport 0x10081f90 with VC6's _RPTF0 arguments; `lineWord` is the VA of the /ZI line-base word, `msg` the
// VA of the message (0x1011d548, 0x1011d52c or 0x1011d56c). `cmp eax, 1 / int3` follows every report.
typedef int(__cdecl* Report_t)(int, const char*, int, const char*, const char*, const char*);
void JReport(unsigned lineWord, int delta, unsigned msg) {
    const int line = G<short>(lineWord) + delta;
    const int r = reinterpret_cast<Report_t>(JgldVA(0x10081f90))(
        0, static_cast<const char*>(JgldVA(0x1011d508)), line, 0, static_cast<const char*>(JgldVA(0x1011d528)),
        static_cast<const char*>(JgldVA(msg)));
    if (r == 1)
        __debugbreak();
}
const unsigned kMsgBusy = 0x1011d548, kMsgDepth = 0x1011d52c, kMsgNone = 0x1011d56c;
const int kFill = static_cast<int>(0xcccccccc);   // the /GZ frame fill a never-stored result local holds

typedef int(__fastcall* B3_t)(void*, void*, void*, int, int);
typedef int(__fastcall* B4_t)(void*, void*, void*, int, int, int);
typedef int(__fastcall* B5_t)(void*, void*, void*, int, int, int, int);
int Call3(unsigned va, void* s, void* d, int x, int y) { return reinterpret_cast<B3_t>(JgldVA(va))(s, 0, d, x, y); }
int Call4(unsigned va, void* s, void* d, int x, int y, int z) {
    return reinterpret_cast<B4_t>(JgldVA(va))(s, 0, d, x, y, z);
}

// 0x00015480  Sprite::drawB(dst, x, y, pal), slot 17 (+0x44). `ret 0x10` at 0x10015726.
// A NULL destination (cmp 0x1001549d / je 0x100154a1) or an invalid sprite (slot +0x20 at 0x100154ad, jne
// 0x100154b9) returns 7 (0x100154bb) BEFORE the Palette word is touched; then it is saved (0x100154cb) and
// replaced when pal != 0 (0x100154ce / 0x100154da). The result local is never initialised.
//   [this+0x20] 8 (0x100154e6):  depth 8 (0x10015517): busy (0x1001552b) -> report 0x10122dd8 + 0x10, else
//                                0x1005ed80(dst, x, y) (0x10015577); depth 0x10 (0x1001551d): busy (0x10015584)
//                                -> drawRle16n 0x100594d0 (0x1001559c), else 0x1001bc40 (0x100155b5); other ->
//                                report + 0x1f, result 0x17 (0x100155f1)
//   [this+0x20] 0x10 (0x100154ec): depth 8 or 0x10 (0x10015619 / 0x1001561f): busy (0x1001562a) -> report + 0x2a,
//                                else 0x1001a6b0(dst, x, y) (0x10015676); other -> report + 0x32, 0x17 (0x100156b2)
//   other: report + 0x3a, 0x17 (0x100156ed)
// Join: Palette word restored, slot +0x24 with 1 (0x10015709), the result local returned.
typedef int(__fastcall* DrawB_t)(void*, void*, void*, int, int, void*);
DrawB_t DrawB_orig;
int __fastcall DrawB_re(void* self, void*, void* dst, int x, int y, void* pal) {
    if (dst == 0 || !SpriteValid(self))
        return 7;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    int r = kFill;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            if (SpriteBusy(self)) JReport(0x10122dd8, 0x10, kMsgBusy);
            else r = Call3(0x1005ed80, self, dst, x, y);
            break;
        case 0x10:
            r = SpriteBusy(self) ? Call3(0x100594d0, self, dst, x, y) : Call3(0x1001bc40, self, dst, x, y);
            break;
        default:
            JReport(0x10122dd8, 0x1f, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        switch (SurfaceDepth(dst)) {
        case 8:
        case 0x10:
            if (SpriteBusy(self)) JReport(0x10122dd8, 0x2a, kMsgBusy);
            else r = Call3(0x1001a6b0, self, dst, x, y);
            break;
        default:
            JReport(0x10122dd8, 0x32, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122dd8, 0x3a, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x00015f80  Sprite::drawE(dst, x, y, z, pal). `ret 0x14` at 0x100161cb.
// The Palette word is saved and replaced FIRST (0x10015fa3 / 0x10015fb2); a NULL destination (je 0x10015fb9) or an
// invalid sprite (slot +0x20 at 0x10015fc5, jne 0x10015fd1) then returns 0x10 (0x10015fd3) with the word left
// replaced. Result local never initialised.
//   8 (0x10015fe6):   depth 8 (0x10016017): busy -> report 0x10122de4 + 0x10, else 0x1005fd20(dst, x, y, z)
//                     (0x1001607b); depth 0x10 (0x1001601d): busy -> drawRle16c 0x1005bac0 (0x100160a4), else
//                     0x1001f2d0 (0x100160c1), both with (dst, x, y, z); other -> report + 0x1f, 0x17
//   0x10 (0x10015fec): the depth is fetched (0x10016113) and not tested; report + 0x2d, 0x17 (0x10016157)
//   other: report + 0x35, 0x17 (0x10016192)
typedef int(__fastcall* Draw5_t)(void*, void*, void*, int, int, int, void*);
Draw5_t DrawE_orig;
int __fastcall DrawE_re(void* self, void*, void* dst, int x, int y, int z, void* pal) {
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0 || !SpriteValid(self))
        return 0x10;
    int r = kFill;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            if (SpriteBusy(self)) JReport(0x10122de4, 0x10, kMsgBusy);
            else r = Call4(0x1005fd20, self, dst, x, y, z);
            break;
        case 0x10:
            r = SpriteBusy(self) ? Call4(0x1005bac0, self, dst, x, y, z) : Call4(0x1001f2d0, self, dst, x, y, z);
            break;
        default:
            JReport(0x10122de4, 0x1f, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        SurfaceDepth(dst);
        JReport(0x10122de4, 0x2d, kMsgDepth);
        r = 0x17;
        break;
    default:
        JReport(0x10122de4, 0x35, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x000165a0  Sprite::drawG(dst, x, y, z, pal). `ret 0x14` at 0x10016834.
// Palette word saved and replaced first (0x100165c6); NULL destination -> 0x10 (0x100165db), invalid sprite
// (slot +0x20 at 0x100165ef) -> 7, both leaving the word replaced. Result local never initialised.
//   8 (0x10016610):    depth 8 (0x10016641) -> report 0x10122dec + 0x11 (msg 0x1011d56c, no result store);
//                      depth 0x10 (0x10016647): busy -> drawRle16t 0x1005af00 (0x100166a2), else 0x10028630
//                      (0x100166bf), with (dst, x, y, z); other -> report + 0x1d, 0x17 (0x100166fb)
//   0x10 (0x10016616): depth 8 or 0x10 (0x10016723 / 0x10016729): busy -> report + 0x29, else 0x1001ab40(dst, x,
//                      y, z) (0x10016784); other -> report + 0x31, 0x17 (0x100167c0)
//   other: report + 0x39, 0x17 (0x100167fb)
Draw5_t DrawG_orig;
int __fastcall DrawG_re(void* self, void*, void* dst, int x, int y, int z, void* pal) {
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    int r = kFill;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            JReport(0x10122dec, 0x11, kMsgNone);
            break;
        case 0x10:
            r = SpriteBusy(self) ? Call4(0x1005af00, self, dst, x, y, z) : Call4(0x10028630, self, dst, x, y, z);
            break;
        default:
            JReport(0x10122dec, 0x1d, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        switch (SurfaceDepth(dst)) {
        case 8:
        case 0x10:
            if (SpriteBusy(self)) JReport(0x10122dec, 0x29, kMsgBusy);
            else r = Call4(0x1001ab40, self, dst, x, y, z);
            break;
        default:
            JReport(0x10122dec, 0x31, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122dec, 0x39, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x00016cb0  Sprite::drawI(dst, x, y, z, pal). `ret 0x14` at 0x10016f1c. Result local cleared first
// (0x10016ccd); Palette word saved and replaced (0x10016cdd); NULL destination -> 0x10 (0x10016cf2), invalid
// sprite (slot +0x20 at 0x10016d06) -> 7.
//   8 (0x10016d27):    depth 8 (0x10016d58) -> report 0x10122df4 + 0x11 (msg 0x1011d56c); depth 0x10 (0x10016d5e):
//                      busy -> drawRle16p 0x10058970 (0x10016db9), else 0x10027430 (0x10016dd6), with (dst, x, y,
//                      z); other -> report + 0x1d, 0x17 (0x10016e12)
//   0x10 (0x10016d2d): depth 8 (0x10016e3a) -> report + 0x26 (msg 0x1011d56c); other -> report + 0x2c, 0x17
//   other: report + 0x34, 0x17 (0x10016ee3)
Draw5_t DrawI_orig;
int __fastcall DrawI_re(void* self, void*, void* dst, int x, int y, int z, void* pal) {
    int r = 0;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            JReport(0x10122df4, 0x11, kMsgNone);
            break;
        case 0x10:
            r = SpriteBusy(self) ? Call4(0x10058970, self, dst, x, y, z) : Call4(0x10027430, self, dst, x, y, z);
            break;
        default:
            JReport(0x10122df4, 0x1d, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        if (SurfaceDepth(dst) == 8) {
            JReport(0x10122df4, 0x26, kMsgNone);
        } else {
            JReport(0x10122df4, 0x2c, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122df4, 0x34, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x000172b0  Sprite::drawK(dst, x, y, z, w, pal). `ret 0x18` at 0x1001752a. Result local cleared (0x100172cd);
// Palette word saved and replaced (0x100172dd, the sixth argument); NULL destination -> 0x10 (0x100172f2), invalid
// sprite (slot +0x20 at 0x10017306) -> 7.
//   8 (0x10017327):    depth 8 (0x10017358) -> report 0x10122dfc + 0x17 (msg 0x1011d56c); depth 0x10 (0x1001735e):
//                      busy -> drawRle16b 0x1005c600 (0x100173c3), else 0x10020430 (0x100173e4), with (dst, x, y,
//                      z, w); other -> report + 0x23, 0x17 (0x10017420)
//   0x10 (0x1001732d): depth 8 (0x10017448) -> report + 0x2c (msg 0x1011d56c); other -> report + 0x32, 0x17
//   other: report + 0x3a, 0x17 (0x100174f1)
typedef int(__fastcall* Draw6_t)(void*, void*, void*, int, int, int, int, void*);
Draw6_t DrawK_orig;
int __fastcall DrawK_re(void* self, void*, void* dst, int x, int y, int z, int w, void* pal) {
    int r = 0;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            JReport(0x10122dfc, 0x17, kMsgNone);
            break;
        case 0x10:
            r = reinterpret_cast<B5_t>(JgldVA(SpriteBusy(self) ? 0x1005c600 : 0x10020430))(self, 0, dst, x, y, z, w);
            break;
        default:
            JReport(0x10122dfc, 0x23, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        if (SurfaceDepth(dst) == 8) {
            JReport(0x10122dfc, 0x2c, kMsgNone);
        } else {
            JReport(0x10122dfc, 0x32, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122dfc, 0x3a, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x00017b40  Sprite::drawM(dst, x, y, z, pal). `ret 0x14` at 0x10017df2. Result local cleared (0x10017b5d);
// Palette word saved and replaced (0x10017b6d); NULL destination -> 0x10 (0x10017b82), invalid sprite (slot +0x20
// at 0x10017b96) -> 7.
//   8 (0x10017bb7):    depth 8 (0x10017be8) -> report 0x10122e08 + 0x17 (msg 0x1011d56c); depth 0x10 (0x10017bee):
//                      busy (0x10017c33) -> report + 0x1b, else 0x1002ab90(dst, x, y, z) (0x10017c83); other ->
//                      report + 0x23, 0x17 (0x10017cbf)
//   0x10 (0x10017bbd): depth 0x10 (0x10017ce7): busy (0x10017cf2) -> report + 0x2d, else 0x1001b060(dst, x, y, z)
//                      (0x10017d42); other -> report + 0x36, 0x17 (0x10017d7e)
//   other: report + 0x3e, 0x17 (0x10017db9)
Draw5_t DrawM_orig;
int __fastcall DrawM_re(void* self, void*, void* dst, int x, int y, int z, void* pal) {
    int r = 0;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            JReport(0x10122e08, 0x17, kMsgNone);
            break;
        case 0x10:
            if (SpriteBusy(self)) JReport(0x10122e08, 0x1b, kMsgBusy);
            else r = Call4(0x1002ab90, self, dst, x, y, z);
            break;
        default:
            JReport(0x10122e08, 0x23, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        if (SurfaceDepth(dst) == 0x10) {
            if (SpriteBusy(self)) JReport(0x10122e08, 0x2d, kMsgBusy);
            else r = Call4(0x1001b060, self, dst, x, y, z);
        } else {
            JReport(0x10122e08, 0x36, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122e08, 0x3e, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x00018120  Sprite::drawN(dst, x, y, z, pal). `ret 0x14` at 0x100183d2. Result local cleared (0x1001813d);
// Palette word saved and replaced (0x1001814d); NULL destination -> 0x10 (0x10018162), invalid sprite (slot +0x20
// at 0x10018176) -> 7 (0x10018184).
//   8 (0x10018197):    depth 8 (0x100181c8) -> report 0x10122e0c + 0x1b (msg 0x1011d56c); depth 0x10 (0x100181ce):
//                      busy (0x10018213) -> report + 0x20, else 0x1002bcf0(dst, x, y, z) (0x10018263); other ->
//                      report + 0x2a, 0x17 (0x1001829f)
//   0x10 (0x1001819d): depth 0x10 (0x100182c7): busy (0x100182d2) -> report + 0x36, else 0x1001b590(dst, x, y, z)
//                      (0x10018322), a body that returns 0; other -> report + 0x41, 0x17 (0x1001835e)
//   other: report + 0x49, 0x17 (0x10018399)
Draw5_t DrawN_orig;
int __fastcall DrawN_re(void* self, void*, void* dst, int x, int y, int z, void* pal) {
    int r = 0;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            JReport(0x10122e0c, 0x1b, kMsgNone);
            break;
        case 0x10:
            if (SpriteBusy(self)) JReport(0x10122e0c, 0x20, kMsgBusy);
            else r = Call4(0x1002bcf0, self, dst, x, y, z);
            break;
        default:
            JReport(0x10122e0c, 0x2a, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        if (SurfaceDepth(dst) == 0x10) {
            if (SpriteBusy(self)) JReport(0x10122e0c, 0x36, kMsgBusy);
            else r = Call4(0x1001b590, self, dst, x, y, z);
        } else {
            JReport(0x10122e0c, 0x41, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122e0c, 0x49, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x000193d0  Sprite::drawP(dst, x, y, pct, pal). `ret 0x14` at 0x10019747. Result local cleared (0x100193ed);
// Palette word saved and replaced (0x100193fd); NULL destination -> 0x10 (0x10019412), invalid sprite (slot +0x20 at
// 0x10019426) -> 7.
//   8 (0x10019447):    depth 8 (0x10019478) -> report 0x10122e18 + 0x1b (msg 0x1011d56c); depth 0x10 (0x1001947e):
//                      busy (0x100194c3) -> report + 0x20, else a switch on pct (0x10019506 / 0x1001950c /
//                      0x10019512): 100 -> slot +0x44 of this sprite's vtable (drawB in jgld's Sprite table) with
//                      (dst, x, y, pal) (0x10019534); 50 -> 0x10056450(dst, x, y) (0x10019552); 25 -> 0x100576a0
//                      (0x1001956b); anything else -> 3 (0x10019575). Other depth -> report + 0x38, 0x17
//   0x10 (0x1001944d): depth 0x10 (0x100195d8): busy (0x100195e6) -> report + 0x44, else the same switch
//                      (0x10019629 / 0x1001962f / 0x10019635): 100 -> slot +0x44 (0x10019657), 50 -> 0x1001b610
//                      (0x10019675), 25 -> 0x1001b650 (0x1001968e), both bodies that return 0, else 3
//                      (0x10019698); other depth -> report + 0x5d, 0x17 (0x100196d3)
//   other: report + 0x65, 0x17 (0x1001970e)
typedef int(__fastcall* Slot44_t)(void*, void*, void*, int, int, void*);
Draw5_t DrawP_orig;
int __fastcall DrawP_re(void* self, void*, void* dst, int x, int y, int pct, void* pal) {
    int r = 0;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (SurfaceDepth(dst)) {
        case 8:
            JReport(0x10122e18, 0x1b, kMsgNone);
            break;
        case 0x10:
            if (SpriteBusy(self)) {
                JReport(0x10122e18, 0x20, kMsgBusy);
            } else {
                switch (pct) {
                case 100: r = reinterpret_cast<Slot44_t>(VSlot(self, 0x44))(self, 0, dst, x, y, pal); break;
                case 50: r = Call3(0x10056450, self, dst, x, y); break;
                case 25: r = Call3(0x100576a0, self, dst, x, y); break;
                default: r = 3;
                }
            }
            break;
        default:
            JReport(0x10122e18, 0x38, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        if (SurfaceDepth(dst) == 0x10) {
            if (SpriteBusy(self)) {
                JReport(0x10122e18, 0x44, kMsgBusy);
            } else {
                switch (pct) {
                case 100: r = reinterpret_cast<Slot44_t>(VSlot(self, 0x44))(self, 0, dst, x, y, pal); break;
                case 50: r = Call3(0x1001b610, self, dst, x, y); break;
                case 25: r = Call3(0x1001b650, self, dst, x, y); break;
                default: r = 3;
                }
            }
        } else {
            JReport(0x10122e18, 0x5d, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122e18, 0x65, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// ===================================================================================== group P
// Two libpng 1.0.5 functions earlier batches left for the heap only (c3cn notes: "png_create_struct callers").
// Their callees png_create_struct 0x10078da0, png_info_init 0x10078940, png_malloc 0x10078e60 and png_free
// 0x10078ed0 are at C3 (c3cn, c3ck), and the CRT memcpy 0x1007f3a0 and strlen 0x1007f6e0 are called at their
// module addresses.
typedef void*(__cdecl* PngCreateStruct_t)(int);
typedef void(__cdecl* PngInfoInit_t)(void*);
typedef void*(__cdecl* PngMalloc_t)(void*, unsigned);
typedef void(__cdecl* PngFree_t)(void*, void*);
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);
typedef unsigned(__cdecl* Strlen_t)(const char*);

// 0x00078870  png_create_info_struct(png_ptr)
// cdecl, one argument (`ret` at 0x100788c4). A NULL png_ptr (cmp 0x10078888 / jne 0x1007888c) returns 0
// (0x1007888e). Otherwise png_create_struct 0x10078da0 with type 2 (push 2 at 0x10078892, call at 0x10078894);
// when that block is not NULL (cmp 0x1007889f / je 0x100788a3) png_info_init 0x10078940 clears it (call at
// 0x100788a9). The block (or NULL) is returned (0x100788b1). png_ptr is not dereferenced.
typedef void*(__cdecl* PngCreateInfo_t)(void*);
PngCreateInfo_t PngCreateInfo_orig;
void* __cdecl PngCreateInfo_re(void* png) {
    if (png == 0)
        return 0;
    void* info = reinterpret_cast<PngCreateStruct_t>(JgldVA(0x10078da0))(2);
    if (info != 0)
        reinterpret_cast<PngInfoInit_t>(JgldVA(0x10078940))(info);
    return info;
}

// 0x0007e220  png_set_text(png_ptr, info_ptr, text_ptr, num_text)
// cdecl, four arguments (`ret` at 0x1007e3ff). info fields: +0x30 num_text, +0x34 max_text, +0x38 text (an
// array of 16-byte png_text {compression, key, text, text_length}).
//  1. Returns at once when png_ptr is NULL (0x1007e238), info_ptr is NULL (0x1007e23e) or num_text is 0
//     (0x1007e244 / jne 0x1007e248).
//  2. When info->num_text + num_text > info->max_text (signed, cmp 0x1007e25b / jle 0x1007e25e):
//     - with an array (cmp [info+0x38] 0x1007e267 / je 0x1007e26b): max_text = num_text_old + num_text + 8
//       (lea 0x1007e27f), a new array of max_text * 16 bytes from png_malloc (0x1007e2a0), the old max_text * 16
//       bytes copied over by memcpy 0x1007f3a0 (0x1007e2c0), the old array handed to png_free (0x1007e2d0);
//     - without one: max_text = num_text + 8 (0x1007e2dd), info->num_text = 0 (0x1007e2e9), a new array from
//       png_malloc (0x1007e2fe).
//  3. For i in [0, num_text) (signed, jge 0x1007e324): the slot is text[info->num_text]; a NULL text pointer in
//     the caller's entry is replaced IN THE CALLER'S ARRAY by the module's empty string 0x1011df00 (0x1007e357);
//     an empty text (first byte 0, test 0x1007e36f / jne 0x1007e371) stores length 0 and compression -1
//     (0x1007e376 / 0x1007e380); otherwise length = strlen 0x1007f6e0 (0x1007e396) and the caller's
//     compression (0x1007e3b3). Then text (0x1007e3c5) and key (0x1007e3d8) are copied and num_text + 1
//     (0x1007e3e7).
typedef void(__cdecl* PngSetText_t)(void*, void*, void*, int);
PngSetText_t PngSetText_orig;
void __cdecl PngSetText_re(void* png, void* info, void* textPtr, int numText) {
    if (png == 0 || info == 0 || numText == 0)
        return;
    int& num = F<int>(info, 0x30);
    int& max = F<int>(info, 0x34);
    char*& arr = F<char*>(info, 0x38);
    PngMalloc_t pngMalloc = reinterpret_cast<PngMalloc_t>(JgldVA(0x10078e60));
    if (num + numText > max) {
        if (arr != 0) {
            const int oldMax = max;
            char* old = arr;
            max = num + numText + 8;
            arr = static_cast<char*>(pngMalloc(png, static_cast<unsigned>(max) << 4));
            reinterpret_cast<Memcpy_t>(JgldVA(0x1007f3a0))(arr, old, static_cast<unsigned>(oldMax) << 4);
            reinterpret_cast<PngFree_t>(JgldVA(0x10078ed0))(png, old);
        } else {
            max = numText + 8;
            num = 0;
            arr = static_cast<char*>(pngMalloc(png, static_cast<unsigned>(max) << 4));
        }
    }
    char* src = static_cast<char*>(textPtr);
    for (int i = 0; i < numText; i++) {
        char* slot = arr + (num << 4);
        char* entry = src + (i << 4);
        if (F<char*>(entry, 8) == 0)
            F<char*>(entry, 8) = static_cast<char*>(JgldVA(0x1011df00));
        if (*F<char*>(entry, 8) == 0) {
            F<int>(slot, 0xc) = 0;
            F<int>(slot, 0) = -1;
        } else {
            F<unsigned>(slot, 0xc) = reinterpret_cast<Strlen_t>(JgldVA(0x1007f6e0))(F<char*>(entry, 8));
            F<int>(slot, 0) = F<int>(entry, 0);
        }
        F<char*>(slot, 8) = F<char*>(entry, 8);
        F<char*>(slot, 4) = F<char*>(entry, 4);
        num += 1;
    }
}

}  // namespace

// 0x00078870  png_create_info_struct
SG_HOOK("jgld.dll", 0x00078870, png_create_info_struct, PngCreateInfo_re, PngCreateInfo_orig);
// 0x0007e220  png_set_text
SG_HOOK("jgld.dll", 0x0007E220, png_set_text, PngSetText_re, PngSetText_orig);
// 0x00015480  Sprite::drawB
SG_HOOK("jgld.dll", 0x00015480, Sprite_drawB, DrawB_re, DrawB_orig);
// 0x00015f80  Sprite::drawE
SG_HOOK("jgld.dll", 0x00015F80, Sprite_drawE, DrawE_re, DrawE_orig);
// 0x000165a0  Sprite::drawG
SG_HOOK("jgld.dll", 0x000165A0, Sprite_drawG, DrawG_re, DrawG_orig);
// 0x00016cb0  Sprite::drawI
SG_HOOK("jgld.dll", 0x00016CB0, Sprite_drawI, DrawI_re, DrawI_orig);
// 0x000172b0  Sprite::drawK
SG_HOOK("jgld.dll", 0x000172B0, Sprite_drawK, DrawK_re, DrawK_orig);
// 0x00017b40  Sprite::drawM
SG_HOOK("jgld.dll", 0x00017B40, Sprite_drawM, DrawM_re, DrawM_orig);
// 0x00018120  Sprite::drawN
SG_HOOK("jgld.dll", 0x00018120, Sprite_drawN, DrawN_re, DrawN_orig);
// 0x000193d0  Sprite::drawP
SG_HOOK("jgld.dll", 0x000193D0, Sprite_drawP, DrawP_re, DrawP_orig);
// 0x00008e50  Surface::blit
SG_HOOK("jgld.dll", 0x00008E50, Surface_blit, SurfaceBlit_re, SurfaceBlit_orig);
// 0x000091a0  Surface::blitT
SG_HOOK("jgld.dll", 0x000091A0, Surface_blitT, SurfaceBlitT_re, SurfaceBlitT_orig);
// 0x000098f0  Surface::blit16
SG_HOOK("jgld.dll", 0x000098F0, Surface_blit16, SurfaceBlit16_re, SurfaceBlit16_orig);
// 0x00006f40  LinkedList::add
SG_HOOK("jgld.dll", 0x00006F40, LinkedList_add, ListAdd_re, ListAdd_orig);
// 0x00006ab0  Tracked::ctor
SG_HOOK("jgld.dll", 0x00006AB0, Tracked_ctor, TrackedCtor_re, TrackedCtor_orig);
// 0x00006bf0  Tracked::deleteAll
SG_HOOK("jgld.dll", 0x00006BF0, Tracked_deleteAll, DeleteAll_re, DeleteAll_orig);
// 0x00006a40  atexitDestroyGlobalList
SG_HOOK("jgld.dll", 0x00006A40, atexitDestroyGlobalList, AtexitDestroy_re, AtexitDestroy_orig);
// 0x000096f0  SurfaceBase::dtor
SG_HOOK("jgld.dll", 0x000096F0, SurfaceBase_dtor, SurfaceBaseDtor_re, SurfaceBaseDtor_orig);
// 0x000168e0  SpriteBase::~SpriteBase
SG_HOOK("jgld.dll", 0x000168E0, SpriteBase_dtor, SpriteBaseDtor_re, SpriteBaseDtor_orig);
// 0x00066d20  DisplayBase::dtor
SG_HOOK("jgld.dll", 0x00066D20, DisplayBase_dtor, DisplayBaseDtor_re, DisplayBaseDtor_orig);
// 0x000674f0  DisplayBase::deletingDtor
SG_HOOK("jgld.dll", 0x000674F0, DisplayBase_deletingDtor, DisplayBaseDeletingDtor_re, DisplayBaseDeletingDtor_orig);
// 0x00065420  Lib::destroyDisplay
SG_HOOK("jgld.dll", 0x00065420, Lib_destroyDisplay, DestroyDisplay_re, DestroyDisplay_orig);
// 0x00014eb0  Sprite::release
SG_HOOK("jgld.dll", 0x00014EB0, Sprite_release, SpriteRelease_re, SpriteRelease_orig);
// 0x0000a660  makeColor
SG_HOOK("jgld.dll", 0x0000A660, makeColor, MakeColor_re, MakeColor_orig);
// 0x0000e440  redOf
SG_HOOK("jgld.dll", 0x0000E440, redOf, RedOf_re, RedOf_orig);
// 0x0000e4c0  greenOf
SG_HOOK("jgld.dll", 0x0000E4C0, greenOf, GreenOf_re, GreenOf_orig);
