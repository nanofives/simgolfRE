// C3 batch c3di (round 26, 2026-10-10, tooling pilot): the Window tree teardown. layoutNode 0x00479f30 (the
// per-window reset every Window destructor and Window::ctorInit run), Window::ctorInit 0x00480610, Window::detach
// 0x0047b820 (vtable slot +8 of every Window class), the View / Image / Base4805a0 destructors and the deleting
// destructors and this-adjustors over them. Tooltip::hide 0x00480360, which layoutNode and Window::detach call, is
// hooked by batch c3dl (dropped here to keep one hook per address: log/c3/c3di_notes.txt).
//
// They walk and edit the LIVE window tree (z-list 0x0083a2d8 / count 0x0083ab94, draw-list count 0x0083ab90, the
// capture / hover / tooltip / modal globals 0x0083aa98, 0x0083aac0/4, 0x0083ab30/34/40/44/54) and release through the
// display 0x0083ad50. The A/B runs them on a SYNTHETIC tree: re/frida/js/fixtures.d/c3di.js freezes the game's first
// thread, points every one of those globals at fixture objects, and writes the real values back before the resume
// (method and the global list with instruction addresses: log/c3/c3di_notes.txt).
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees
// run through their original addresses.
//
// Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
//   HotList::resetFields 0x00492800, HotList::releaseArray 0x00492830, HotList::dtor 0x004928d0 (c3de);
//   Timer::stop 0x00486ec0, Timer::reset 0x00486f10, Timer::dtor 0x00486ce0 (c3cy); Node::contains 0x0047b080 (c3d);
//   View_unlinkFromParent 0x0047ada0, ChildArray::remove 0x0047e680, Vec_release 0x00479bb0, Cache483010::dtor
//   0x00483010, Cache483070::reset 0x00483070, operatorDelete_4a4ffc 0x004a4ffc (c3cz); listRemove 0x0047e520 (c3db);
//   resetWidgetTable 0x00495eb0 (c3g); Image_releaseAll 0x00474cb0 (c3dd); Tooltip::hide 0x00480360 (c3dl).
#include "hooks.h"

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
static inline void*& P(void* base, unsigned off) { return *reinterpret_cast<void**>(B(base) + off); }
static inline int& I(void* base, unsigned off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline unsigned& U(void* base, unsigned off) { return *reinterpret_cast<unsigned*>(B(base) + off); }

// Virtual calls: `mov eax, [obj]; call [eax + slot]` (the vtable is [obj], the slot is [[obj] + slot]).
typedef unsigned(__fastcall* V0_t)(void* self, void* edx);
typedef unsigned(__fastcall* V1_t)(void* self, void* edx, unsigned a);
static inline V0_t Slot0(void* obj, unsigned slot) { return *reinterpret_cast<V0_t*>(B(P(obj, 0)) + slot); }
static inline V1_t Slot1(void* obj, unsigned slot) { return *reinterpret_cast<V1_t*>(B(P(obj, 0)) + slot); }

// ---- exe globals (the window tree; names from re/names/exe_3_globals.tsv where one exists)
void** const g_ab40 = reinterpret_cast<void**>(0x0083ab40);
void** const g_ab44 = reinterpret_cast<void**>(0x0083ab44);
void** const g_dragCapture = reinterpret_cast<void**>(0x0083ab30);     // c3ct: drag capture
void** const g_dragCapture2 = reinterpret_cast<void**>(0x0083ab34);
void** const g_tooltipOwner = reinterpret_cast<void**>(0x0083aac0);
void** const g_aaf0 = reinterpret_cast<void**>(0x0083aaf0);
void** const g_captureWindow = reinterpret_cast<void**>(0x0083aa98);    // released by 0x00479b20 (ReleaseCapture)
void** const g_modalTop = reinterpret_cast<void**>(0x0083ab54);
void** const g_display = reinterpret_cast<void**>(0x0083ad50);
const unsigned* const g_ac18 = reinterpret_cast<const unsigned*>(0x0083ac18);

// ---- exe callees (original addresses)
typedef unsigned(__fastcall* T0_t)(void* self, void* edx);
typedef int(__fastcall* T1_t)(void* self, void* edx, void* a);
typedef void(__cdecl* C0_t)();
typedef void(__cdecl* C1_t)(void* a);
typedef void*(__fastcall* Del_t)(void* self, void* edx, int flags);
const T0_t kHotListReleaseArray = reinterpret_cast<T0_t>(0x00492830);
const T0_t kHotListDtor = reinterpret_cast<T0_t>(0x004928d0);
const T1_t kContains = reinterpret_cast<T1_t>(0x0047b080);                // Node::contains, ret 4
const C0_t kTooltipHide = reinterpret_cast<C0_t>(0x00480360);
const T0_t kUnlinkFromParent = reinterpret_cast<T0_t>(0x0047ada0);        // View_unlinkFromParent
const T1_t kChildArrayRemove = reinterpret_cast<T1_t>(0x0047e680);        // ChildArray::remove, ret 4
const C1_t kListRemove = reinterpret_cast<C1_t>(0x0047e520);              // z-list removal, cdecl
const T0_t kResetWidgetTable = reinterpret_cast<T0_t>(0x00495eb0);
const C1_t kFree = reinterpret_cast<C1_t>(0x004a5007);
const C1_t kOperatorDelete = reinterpret_cast<C1_t>(0x004a4ffc);
const T0_t kImageReleaseAll = reinterpret_cast<T0_t>(0x00474cb0);
const T0_t kImageDtor = reinterpret_cast<T0_t>(0x00474c40);
const T0_t kViewDtor = reinterpret_cast<T0_t>(0x0047a3c0);
const T0_t kBase4805a0Dtor = reinterpret_cast<T0_t>(0x004805a0);
const T0_t kLayoutNode = reinterpret_cast<T0_t>(0x00479f30);
const T0_t kReleaseCaptureIfOwner = reinterpret_cast<T0_t>(0x00479b20);
const C0_t kRebuildDrawList = reinterpret_cast<C0_t>(0x0047e450);
const Del_t kView4804a0ScalarDtor = reinterpret_cast<Del_t>(0x00480580);
const Del_t kDeletingDtor404380 = reinterpret_cast<Del_t>(0x00404380);
const T0_t kDtor404350 = reinterpret_cast<T0_t>(0x00404350);              // `jmp 0x004805a0`

// The node list at +0x138 (head), +0x13c (cursor), +0x140 (count), +0x148 (non-zero: nodes not owned). Each node
// is {+8 data, +0xc next}. Shared by layoutNode (0x0047a0d4..0x0047a14e) and View_dtor (0x0047a3f1..0x0047a47c):
// both re-read the count, the head and the cursor from the object at every step.
static void FreeNodeList(void* w) {
    if (I(w, 0x148) == 0) {                                                  // 0x0047a0dc / 0x0047a40e
        for (int i = 0; i < I(w, 0x140); i++) {                              // 0x0047a0ec jle, 0x0047a13e jl
            P(w, 0x13c) = P(P(w, 0x138), 0xc);                               // 0x0047a0f6..0x0047a0f9
            void* data = P(P(w, 0x138), 8);                                  // 0x0047a0ff
            if (data) kFree(data);                                           // 0x0047a107
            P(P(w, 0x138), 8) = 0;                                           // 0x0047a115
            void* node = P(w, 0x138);
            if (node) kFree(node);                                           // 0x0047a123
            P(w, 0x138) = P(w, 0x13c);                                       // 0x0047a138
        }
    }
    P(w, 0x138) = 0;                                                         // 0x0047a142
    I(w, 0x144) = 0;
    I(w, 0x140) = 0;
}

// Copies of the two optional replacement objects at +0x26c / +0x270 (0x00479fbd..0x0047a028).
static void ReleaseReplaceable(void* w, unsigned off) {
    if (!P(w, off)) return;                                                  // 0x00479fcc / 0x0047a001
    if (U(w, 0x9c) & 0x40)                                                   // test byte [+0x9c], 0x40
        P(w, off) = reinterpret_cast<void*>(Slot1(w, 0x1c)(w, 0, reinterpret_cast<unsigned>(P(w, off))));
    void* obj = P(w, off);
    if (obj) {                                                               // 0x00479fec / 0x0047a021
        Slot1(obj, 0)(obj, 0, 1);                                            // `push 1; call [eax]`
        P(w, off) = 0;
    }
}

// The hover pair: the window g_ab40 (or g_ab44 when g_ab40 is 0) is dropped when it is this window or inside it.
static void DropHoverIfInside(void* w) {
    void* cur = *g_ab40 ? *g_ab40 : *g_ab44;                                 // 0x00479f4e..0x00479f54
    if (cur == w || kContains(w, 0, cur)) {                                  // 0x00479f56 je / 0x00479f63
        *g_ab40 = 0;                                                         // 0x00479f6c
        *g_ab44 = 0;
    }
}

// 0x00479f30  layoutNode
// Resets one window and, first, its whole subtree. Releases the HotList at +0xbc (0x00479f3c). Drops the hover pair
// g_ab40/g_ab44 when it points at this window or a descendant (Node::contains, 0x00479f63). Clears the drag capture
// 0x0083ab30 (and calls this window's vtable slot +0x10, 0x00479f8a), 0x0083ab34, hides the tooltip when 0x0083aac0
// is this window (0x00479fa3) and clears 0x0083aaf0. Calls vtable slot +8 (0x00479fba). Releases the replaceable
// objects +0x26c/+0x270 (slot +0x1c maps them first when flag 0x40 of +0x9c is set; then their slot 0 with 1). With
// flag 0x100000 of +0x9c, unlinks the window from its parent's node list (0x0047a03c). Recurses into each child of
// the array +0x224 (count +0x22c, both re-read every step, 0x0047a04d..0x0047a064). Removes the window from its
// parent's child array (+0x130 set, 0x0047a071) or from the z-list (0x0047a078). Then rewrites the fields: the
// pointers and counters to 0, +0xb0 = this, the node list +0x138 freed, the style block +0x164..+0x194 from the
// default globals 0x0083ab64..0x0083ab70 and 0x004e42c8..0x004e42e8, +0x14c freed, +0x150/+0x154/+0x158 released
// through slot 0 (1), +0x11c..+0x128 through slot +0xc (1), and the widget table (resetWidgetTable, 0x0047a3aa).
// eax at the return is resetWidgetTable's; no caller reads it (0x0047a3f1, 0x0047a4db, 0x0047a900, 0x00480619).
T0_t LayoutNode_orig;
unsigned __fastcall LayoutNode_re(void* w, void*) {
    kHotListReleaseArray(B(w) + 0xbc, 0);                                    // 0x00479f3c
    DropHoverIfInside(w);
    if (*g_dragCapture == w) {                                               // 0x00479f78
        *g_dragCapture = 0;
        Slot0(w, 0x10)(w, 0);                                                // 0x00479f8a
    }
    if (*g_dragCapture2 == w) *g_dragCapture2 = 0;                           // 0x00479f8d
    if (*g_tooltipOwner == w) kTooltipHide();                                // 0x00479f9b / 0x00479fa3
    if (*g_aaf0 == w) *g_aaf0 = 0;                                           // 0x00479fa8
    Slot0(w, 8)(w, 0);                                                       // 0x00479fba
    ReleaseReplaceable(w, 0x26c);
    ReleaseReplaceable(w, 0x270);
    if (U(w, 0x9c) & 0x100000) kUnlinkFromParent(w, 0);                      // 0x0047a02e / 0x0047a03c
    for (int i = 0; i < I(w, 0x22c); i++)                                    // 0x0047a04b jle, 0x0047a064 jl
        kLayoutNode(reinterpret_cast<void**>(P(w, 0x224))[i], 0);            // 0x0047a056
    void* parent = P(w, 0x130);
    if (parent) kChildArrayRemove(parent, 0, w);                             // 0x0047a06f / 0x0047a071
    else kListRemove(w);                                                     // 0x0047a078
    P(w, 0x130) = 0;                                                         // 0x0047a080
    I(w, 0x21c) = 0;
    I(w, 0xb4) = 0;
    I(w, 0xb8) = 0;
    I(w, 0x1a4) = 0;
    I(w, 0x1a8) = 0;
    P(w, 0xb0) = w;                                                          // 0x0047a0a4
    I(w, 0x208) = 0;
    I(w, 0x20c) = 0;
    I(w, 0x210) = 0;
    I(w, 0x218) = 0;
    I(w, 0x214) = 0;
    I(w, 0x204) = 0;
    I(w, 0x15c) = 0;                                                         // 0x0047a0ce
    if (P(w, 0x138)) FreeNodeList(w);                                        // 0x0047a0d4 je
    I(w, 0x144) = 0;                                                         // 0x0047a154
    I(w, 0x19c) = 0;
    I(w, 0x1a0) = 1;                                                         // 0x0047a160 (ebp = 1)
    U(w, 0x168) = *reinterpret_cast<unsigned*>(0x0083ab64);                  // 0x0047a166..0x0047a1f8
    U(w, 0x16c) = *reinterpret_cast<unsigned*>(0x004e42c8);
    U(w, 0x170) = *reinterpret_cast<unsigned*>(0x0083ab6c);
    U(w, 0x174) = *reinterpret_cast<unsigned*>(0x004e42d0);
    U(w, 0x178) = *reinterpret_cast<unsigned*>(0x004e42d4);
    U(w, 0x17c) = *reinterpret_cast<unsigned*>(0x004e42d8);
    U(w, 0x180) = *reinterpret_cast<unsigned*>(0x004e42cc);
    U(w, 0x184) = *reinterpret_cast<unsigned*>(0x004e42dc);
    U(w, 0x188) = *reinterpret_cast<unsigned*>(0x004e42e0);
    U(w, 0x18c) = *reinterpret_cast<unsigned*>(0x004e42e4);
    U(w, 0x190) = *reinterpret_cast<unsigned*>(0x0083ab70);
    U(w, 0x194) = *reinterpret_cast<unsigned*>(0x004e42e8);
    U(w, 0x164) = *reinterpret_cast<unsigned*>(0x0083ab68);
    if (P(w, 0x14c)) {                                                       // 0x0047a204
        kFree(P(w, 0x14c));
        P(w, 0x14c) = 0;
    }
    for (unsigned off = 0x150; off <= 0x158; off += 4) {                     // 0x0047a217..0x0047a250
        void* obj = P(w, off);
        if (obj) {
            Slot1(obj, 0)(obj, 0, 1);
            P(w, off) = 0;
        }
    }
    I(w, 0x160) = 0;                                                         // 0x0047a256
    I(w, 0x9c) = 0;
    for (unsigned off = 0x1f4; off <= 0x200; off += 4) I(w, off) = 1;        // 0x0047a262..0x0047a274
    for (unsigned off = 0x1cc; off <= 0x1d8; off += 4) I(w, off) = 0;        // 0x0047a27a..0x0047a28c
    for (unsigned off = 0x1ac; off <= 0x1c8; off += 4) I(w, off) = 0;        // 0x0047a292..0x0047a2bc
    I(w, 0x1ec) = 0;                                                         // 0x0047a2c8
    I(w, 0x1f0) = 0;
    for (unsigned off = 0x230; off <= 0x268; off += 4) I(w, off) = 0;        // 0x0047a2d6..0x0047a32a
    I(w, 0x220) = 2;                                                         // 0x0047a330
    for (unsigned off = 0xa0; off <= 0xac; off += 4) I(w, off) = 0;          // 0x0047a33a..0x0047a34c
    I(w, 0x22c) = 0;                                                         // 0x0047a352
    for (unsigned off = 0x11c; off <= 0x128; off += 4) {                     // 0x0047a358..0x0047a3a2
        void* obj = P(w, off);
        if (obj) {
            Slot1(obj, 0xc)(obj, 0, 1);                                      // `push 1; call [eax + 0xc]`
            P(w, off) = 0;
        }
    }
    return kResetWidgetTable(w, 0);                                          // 0x0047a3aa
}

// 0x00480610  Window::ctorInit
// layoutNode (0x00480614), then Image_releaseAll on the Image sub-object at +0x274 (0x0048061f). Zeroes +0x570,
// +0x1a4, +0x1a8 and +0x52c..+0x564 (0x0048062c..0x00480692), stores the global 0x0083ac18 in +0x56c (0x0048069f) and,
// when +0x568 (read at 0x00480624, before the stores) holds an object, calls its vtable slot +0xc with 1 (0x004806ab)
// and clears +0x568. eax at the return: the slot's result, else the 0x0083ac18 value (four callers return it:
// 0x004860fa, 0x00496326, 0x0049d6c8, 0x0049ed12).
T0_t CtorInit_orig;
unsigned __fastcall CtorInit_re(void* w, void*) {
    kLayoutNode(w, 0);                                                       // 0x00480614
    kImageReleaseAll(B(w) + 0x274, 0);                                       // 0x0048061f
    void* obj = P(w, 0x568);                                                 // 0x00480624
    I(w, 0x570) = 0;
    I(w, 0x1a4) = 0;
    I(w, 0x1a8) = 0;
    for (unsigned off = 0x52c; off <= 0x564; off += 4) I(w, off) = 0;        // 0x0048063e..0x00480692
    unsigned r = *g_ac18;                                                    // 0x00480698
    U(w, 0x56c) = r;                                                         // 0x0048069f
    if (obj) {                                                               // 0x004806a5 je
        r = Slot1(obj, 0xc)(obj, 0, 1);                                      // 0x004806ab
        P(w, 0x568) = 0;
    }
    return r;
}

// 0x0047b820  Window::detach (vtable slot +8)
// Calls vtable slot +0xf8 (0x0047b826); releases the mouse capture when 0x0083aa98 is this window (0x00479b20,
// 0x0047b836); hides the tooltip it owns (0x0047b843); drops the hover pair as layoutNode does (0x0047b848..
// 0x0047b879); clears the drag capture 0x0083ab30 with a slot +0x10 call (0x0047b891) and 0x0083ab34. When the window
// is shown (bit 0 of +0xa0, 0x0047b8aa) it clears that bit, calls slot +0x110 when it is the modal top 0x0083ab54
// (0x0047b8c1), rebuilds the draw list (0x0047e450, 0x0047b8c7), calls the display's slot +0x30 with 0 (0x0047b8d9)
// and the vtable slot +0x1c of the object at +0x10 (0x0047b8e5).
T0_t Detach_orig;
void __fastcall Detach_re(void* w, void*) {
    Slot0(w, 0xf8)(w, 0);                                                    // 0x0047b826
    if (*g_captureWindow == w) kReleaseCaptureIfOwner(w, 0);                 // 0x0047b82c / 0x0047b836
    if (*g_tooltipOwner == w) kTooltipHide();                                // 0x0047b83b / 0x0047b843
    DropHoverIfInside(w);                                                    // 0x0047b848..0x0047b879
    if (*g_dragCapture == w) {                                               // 0x0047b87f
        *g_dragCapture = 0;
        Slot0(w, 0x10)(w, 0);                                                // 0x0047b891
    }
    if (*g_dragCapture2 == w) *g_dragCapture2 = 0;                           // 0x0047b894
    unsigned f = U(w, 0xa0);
    if (f & 1) {                                                             // 0x0047b8a8 / 0x0047b8aa
        U(w, 0xa0) = f & ~1u;                                                // `and al, 0xfe`
        if (*g_modalTop == w) Slot0(w, 0x110)(w, 0);                         // 0x0047b8b9 / 0x0047b8c1
        kRebuildDrawList();                                                  // 0x0047b8c7
        void* disp = *g_display;
        if (disp) Slot1(disp, 0x30)(disp, 0, 0);                             // 0x0047b8d2 / 0x0047b8d9
        void* obj = P(w, 0x10);
        if (obj) Slot0(obj, 0x1c)(obj, 0);                                   // 0x0047b8df / 0x0047b8e5
    }
}

// 0x0047a3c0  View_dtor
// Stores the vtable 0x004ba8b4 (0x0047a3de) and runs layoutNode (0x0047a3ec). Stores 0x004baa10 at +0x134
// (0x0047a3fb) and, when the node list +0x138 is not empty (read at 0x0047a3f1, after layoutNode), frees it as
// layoutNode does (0x0047a407..0x0047a47c). Clears +0x144, destroys the HotList +0xbc (0x0047a493), stores the base
// vtable 0x004baa14 (0x0047a4a2) and resets the widget table (0x0047a4a8). eax at the return is resetWidgetTable's
// (the epilogue 0x0047a4ad..0x0047a4bd keeps it); Base4805a0::dtor returns it on.
T0_t ViewDtor_orig;
unsigned __fastcall ViewDtor_re(void* w, void*) {
    U(w, 0) = 0x004ba8b4;                                                    // 0x0047a3de
    kLayoutNode(w, 0);                                                       // 0x0047a3ec
    void* head = P(w, 0x138);                                                // 0x0047a3f1
    U(w, 0x134) = 0x004baa10;                                                // 0x0047a3fb
    if (head) FreeNodeList(w);                                               // 0x0047a405 je
    I(w, 0x144) = 0;                                                         // 0x0047a489
    kHotListDtor(B(w) + 0xbc, 0);                                            // 0x0047a493
    U(w, 0) = 0x004baa14;                                                    // 0x0047a4a2
    return kResetWidgetTable(w, 0);                                          // 0x0047a4a8
}

// 0x00474c40  Image_dtor
// Stores the vtable 0x004ba858 (0x00474c5d), releases everything the Image holds (Image_releaseAll, 0x00474c6b) and
// destroys its two HotLists, +0x254 then +0xb4 (0x00474c7b / 0x00474c8e). eax at the return is the second
// HotList::dtor's (0 from Timer::reset's `xor eax, eax`, 0x00486f18); 0x0043a31e, 0x0043d261 and 0x0044210a call it as
// their last call and the tail jumps 0x00473aa9, 0x00479a29, 0x00479a79 return it.
T0_t ImageDtor_orig;
unsigned __fastcall ImageDtor_re(void* im, void*) {
    U(im, 0) = 0x004ba858;                                                   // 0x00474c5d
    kImageReleaseAll(im, 0);                                                 // 0x00474c6b
    kHotListDtor(B(im) + 0x254, 0);                                          // 0x00474c7b
    return kHotListDtor(B(im) + 0xb4, 0);                                    // 0x00474c8e
}

// 0x004805a0  Base4805a0::dtor
// Stores the vtables 0x004baa30 at +0 and 0x004baa1c at +0x274 (0x004805c3 / 0x004805c9), clears +0x570
// (0x004805d5), destroys the Image sub-object (Image_dtor on this + 0x274, or on 0 when this is 0: neg / sbb / and at
// 0x004805dd..0x004805e1, 0x004805e3) and then the View part (View_dtor, 0x004805f2). eax at the return is View_dtor's;
// Dialog::subDtor 0x004914d0 returns it (`call 0x4805a0; pop edi; pop esi; ret`, 0x004914e9).
T0_t BaseDtor_orig;
unsigned __fastcall BaseDtor_re(void* w, void*) {
    U(w, 0) = 0x004baa30;                                                    // 0x004805c3
    U(w, 0x274) = 0x004baa1c;                                                // 0x004805c9
    I(w, 0x570) = 0;                                                         // 0x004805d5
    kImageDtor(w ? B(w) + 0x274 : 0, 0);                                     // 0x004805e3
    return kViewDtor(w, 0);                                                  // 0x004805f2
}

// Deleting destructors: `call <dtor>; test byte [esp + 8], 1; je; push esi; call 0x004a4ffc; mov eax, esi; ret 4`.
// 0x00479f10  View_deleteDtor (View_dtor 0x0047a3c0, 0x00479f13)
Del_t ViewDeleteDtor_orig;
void* __fastcall ViewDeleteDtor_re(void* w, void*, int flags) {
    kViewDtor(w, 0);                                                         // 0x00479f13
    if (flags & 1) kOperatorDelete(w);                                       // 0x00479f18 / 0x00479f20
    return w;                                                                // 0x00479f28
}

// 0x00474c20  Image_deleteDtor (Image_dtor 0x00474c40, 0x00474c23)
Del_t ImageDeleteDtor_orig;
void* __fastcall ImageDeleteDtor_re(void* im, void*, int flags) {
    kImageDtor(im, 0);                                                       // 0x00474c23
    if (flags & 1) kOperatorDelete(im);                                      // 0x00474c28 / 0x00474c30
    return im;                                                               // 0x00474c38
}

// 0x00480580  View4804a0::scalarDtor (Base4805a0::dtor 0x004805a0, 0x00480583)
Del_t View4804a0ScalarDtor_orig;
void* __fastcall View4804a0ScalarDtor_re(void* w, void*, int flags) {
    kBase4805a0Dtor(w, 0);                                                   // 0x00480583
    if (flags & 1) kOperatorDelete(w);                                       // 0x00480588 / 0x00480590
    return w;                                                                // 0x00480598
}

// 0x00404380  deletingDtor_404380 (0x00404350, a `jmp 0x004805a0`, at 0x00404383)
Del_t DeletingDtor404380_orig;
void* __fastcall DeletingDtor404380_re(void* w, void*, int flags) {
    kDtor404350(w, 0);                                                       // 0x00404383
    if (flags & 1) kOperatorDelete(w);                                       // 0x00404388 / 0x00404390
    return w;                                                                // 0x00404398
}

// 0x0047d810  View::scalarDtor47d810 (slot 0 of the base vtable 0x004baa14)
// Stores 0x004baa14 (0x0047d813), resets the widget table (0x0047d819) and frees the object when bit 0 of the flags
// is set (0x0047d81e / 0x0047d826).
Del_t ViewScalarDtor47d810_orig;
void* __fastcall ViewScalarDtor47d810_re(void* w, void*, int flags) {
    U(w, 0) = 0x004baa14;                                                    // 0x0047d813
    kResetWidgetTable(w, 0);                                                 // 0x0047d819
    if (flags & 1) kOperatorDelete(w);                                       // 0x0047d81e / 0x0047d826
    return w;                                                                // 0x0047d82e
}

// This-adjustors of the Image sub-object at +0x274: `sub ecx, 0x274; jmp <deleting destructor>`.
// 0x00481b40  View4804a0::scalarDtor_adj274 (slot 3 of 0x004baa1c, into 0x00480580)
Del_t View4804a0ScalarDtorAdj_orig;
void* __fastcall View4804a0ScalarDtorAdj_re(void* sub, void*, int flags) {
    return kView4804a0ScalarDtor(B(sub) - 0x274, 0, flags);                  // 0x00481b40 / 0x00481b46
}

// 0x00404950  deletingDtor_404380_adj274 (into 0x00404380)
Del_t DeletingDtor404380Adj_orig;
void* __fastcall DeletingDtor404380Adj_re(void* sub, void*, int flags) {
    return kDeletingDtor404380(B(sub) - 0x274, 0, flags);                    // 0x00404950 / 0x00404956
}

}  // namespace

// 0x00479f30  layoutNode
SG_HOOK("golf_clean.exe", 0x00479f30, layoutNode, LayoutNode_re, LayoutNode_orig);
// 0x00480610  Window::ctorInit
SG_HOOK("golf_clean.exe", 0x00480610, Window_ctorInit, CtorInit_re, CtorInit_orig);
// 0x0047b820  Window::detach
SG_HOOK("golf_clean.exe", 0x0047b820, Window_detach, Detach_re, Detach_orig);
// 0x0047a3c0  View_dtor
SG_HOOK("golf_clean.exe", 0x0047a3c0, View_dtor, ViewDtor_re, ViewDtor_orig);
// 0x00474c40  Image_dtor
SG_HOOK("golf_clean.exe", 0x00474c40, Image_dtor, ImageDtor_re, ImageDtor_orig);
// 0x004805a0  Base4805a0::dtor
SG_HOOK("golf_clean.exe", 0x004805a0, Base4805a0_dtor, BaseDtor_re, BaseDtor_orig);
// 0x00479f10  View_deleteDtor
SG_HOOK("golf_clean.exe", 0x00479f10, View_deleteDtor, ViewDeleteDtor_re, ViewDeleteDtor_orig);
// 0x00474c20  Image_deleteDtor
SG_HOOK("golf_clean.exe", 0x00474c20, Image_deleteDtor, ImageDeleteDtor_re, ImageDeleteDtor_orig);
// 0x00480580  View4804a0::scalarDtor
SG_HOOK("golf_clean.exe", 0x00480580, View4804a0_scalarDtor, View4804a0ScalarDtor_re, View4804a0ScalarDtor_orig);
// 0x00404380  deletingDtor_404380
SG_HOOK("golf_clean.exe", 0x00404380, deletingDtor_404380, DeletingDtor404380_re, DeletingDtor404380_orig);
// 0x0047d810  View::scalarDtor47d810
SG_HOOK("golf_clean.exe", 0x0047d810, View_scalarDtor47d810, ViewScalarDtor47d810_re, ViewScalarDtor47d810_orig);
// 0x00481b40  View4804a0::scalarDtor_adj274
SG_HOOK("golf_clean.exe", 0x00481b40, View4804a0_scalarDtor_adj274, View4804a0ScalarDtorAdj_re,
        View4804a0ScalarDtorAdj_orig);
// 0x00404950  deletingDtor_404380_adj274
SG_HOOK("golf_clean.exe", 0x00404950, deletingDtor_404380_adj274, DeletingDtor404380Adj_re, DeletingDtor404380Adj_orig);
