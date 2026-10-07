// C3 batch c3y (2026-10-07): ui functions of golf_clean.exe that are statically called but never reached by the test
// scenarios. Hand-written from the C2 transcriptions re/analysis/ui/<addr>_<name>.md and the disassembly
// (re/tools/asm2inline.py); each body cites the address every field offset and constant comes from, never a copy of a
// matched source. __thiscall is emulated with __fastcall (ecx = this, edx unused), as shim/src/re/c3d.cpp does.
// __thiscall callees are reached through their original addresses, so during the A/B a hooked callee runs its own
// reimplementation (the verifier re-runs the ctors with SIMGOLF_HOOKS_OFF for the batch-mate ListModel::ctor).
#include "hooks.h"

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }
template <typename T> T& field(void* obj, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(obj) + off);
}

// __thiscall callees of the constructors, emulated as __fastcall (ecx = this, edx unused).
typedef void*(__fastcall* Ctor_t)(void* self, void* edx);
const Ctor_t kWidgetCtor    = reinterpret_cast<Ctor_t>(0x004804a0);  // View4804a0::ctor
const Ctor_t kListModelCtor = reinterpret_cast<Ctor_t>(0x00489150);  // ListModel::ctor (batch-mate)
const Ctor_t kBufCtor       = reinterpret_cast<Ctor_t>(0x004747a0);  // Buf_ctor -> Buf_init (field zeroing)

// 0x0047ca10  Window::adjustSizeOuter(int* a, int* b) ----------------------------------------------------
// Grows the outer size (*a, *b) of a window from its style flags at +0x9c and border fields. Reads the global border
// at 0x0083ff10 (0x0047ca37/0x0047ca4c). Flag bit 4 adds the border to *b (0x0047ca32), bit 8 to *a (0x0047ca43).
// When flag bit 0x400 is set (dh&4 at 0x0047ca5e) OR, with 0x400 clear (0x0047ca8c), either of bits 0x1/0x10 is set
// (dl&0x11), it adds 2*[+0x184] to both *a and *b (0x0047ca63.. / 0x0047ca91..) and, when [+0x188] != -1
// (0x0047ca85/0x0047cab3), adds [+0x188] - [+0x184] to *b (0x0047cab8). Flag bit 0x10 adds [+0x180] - [+0x184] to *b
// (0x0047cac2). Finally, when the child pointer at +0x15c is non-null (0x0047cae6) and flag bit 0x20000000 is clear
// (0x0047caea), it calls that child's virtual slot +0x170 (0x0047caf8) and adds the result to *b. Returns via ret 8.
typedef void(__fastcall* AdjSize_t)(void* self, void* edx, int* a, int* b);
AdjSize_t AdjustSizeOuter_orig;
void __fastcall AdjustSizeOuter_re(void* self, void*, int* a, int* b) {
    if (!a || !b) return;                                    // 0x0047ca19, 0x0047ca25
    const unsigned f = field<unsigned>(self, 0x9c);          // 0x0047ca2b
    const int border = at<int>(0x0083ff10);                  // DAT_0083ff10
    if (f & 4) *b += border;                                 // 0x0047ca32
    if (f & 8) *a += border;                                 // 0x0047ca43
    const int w = field<int>(self, 0x184);                   // 0x0047ca63 / 0x0047ca91
    if ((f & 0x400) || (f & 0x11)) {                         // 0x0047ca5e (dh&4) / 0x0047ca8c (dl&0x11)
        *a += w * 2;                                         // shl edx,1 at 0x0047ca6b / 0x0047ca99
        *b += w * 2;
        const int h = field<int>(self, 0x188);               // 0x0047ca7f / 0x0047caad
        if (h != -1) *b += h - w;                            // 0x0047ca85 / 0x0047cab3 ; 0x0047cab8
    }
    if (f & 0x10) *b += field<int>(self, 0x180) - w;         // 0x0047cac2 ; 0x0047cacb..0x0047cadd
    void* child = field<void*>(self, 0x15c);                 // 0x0047cadf
    if (child && !(f & 0x20000000)) {                        // 0x0047cae6 / 0x0047caea
        void** vt = *reinterpret_cast<void***>(child);       // 0x0047caf6 eax = [child]
        typedef int(__fastcall* Slot_t)(void* self, void* edx);
        *b += reinterpret_cast<Slot_t>(vt[0x170 / 4])(child, 0);  // call [eax+0x170] ; add [esi],eax
    }
}

// 0x004940e0  comboFindItem(int key) -------------------------------------------------------------------
// Searches a dropdown's node list for the node whose id (+4) equals `key` and returns that node's value. The flags
// byte at +4 bit 4 (test al,4 at 0x004940e6) selects which embedded list to walk: set -> the selector at +0x1488
// with list base this + [sel+8] + 0x1548 (0x004940eb..0x004940f4); clear -> the selector at +0x2d98 with base
// this + [sel+8] + 0x2e58 (0x00494152..0x00494164). From the list base L: head = [L+8], count = [L+0x10]; it resets
// the cursor [L+0xc] = head and index [L+0x14] = 0, returns 0 when head is null (0x00494104/0x0049416b) or count <= 0
// (0x00494117/0x0049417e). It walks up to count nodes (next at +0xc), comparing [cursor+4] with key (0x00494120/
// 0x00494187); on a match it returns *(*(cursor+8)+4) (0x004941be..0x004941c8), else 0.
typedef int(__fastcall* ComboFind_t)(void* self, void* edx, int key);
ComboFind_t ComboFindItem_orig;
int __fastcall ComboFindItem_re(void* self, void*, int key) {
    char* L;
    if (field<unsigned char>(self, 4) & 4) {                 // 0x004940e6
        char* sel = field<char*>(self, 0x1488);              // 0x004940eb
        L = reinterpret_cast<char*>(self) + *reinterpret_cast<int*>(sel + 8) + 0x1548;  // 0x004940f1/0x004940f4
    } else {
        char* sel = field<char*>(self, 0x2d98);              // 0x00494152
        L = reinterpret_cast<char*>(self) + *reinterpret_cast<int*>(sel + 8) + 0x2e58;  // 0x00494158/0x00494164
    }
    void* head = *reinterpret_cast<void**>(L + 8);           // [L+8]
    if (!head) return 0;                                     // 0x00494104 / 0x0049416b
    int count = *reinterpret_cast<int*>(L + 0x10);           // [L+0x10]
    *reinterpret_cast<int*>(L + 0x14) = 0;                   // [L+0x14] = 0 (index)
    *reinterpret_cast<void**>(L + 0xc) = head;               // [L+0xc] = head (cursor)
    if (count <= 0) return 0;                                // 0x00494117 / 0x0049417e
    for (int i = 0; i < count; i++) {                        // 0x0049411d.. / 0x00494184..
        char* cursor = *reinterpret_cast<char**>(L + 0xc);
        if (*reinterpret_cast<int*>(cursor + 4) == key) {    // 0x00494120 / 0x00494187
            // 0x004941bc: the original re-reads [L+8] here and, were it null, reads *(int*)4 (a latent null-deref).
            // That branch is unreachable from a match: the loop only runs when [L+8] != 0 and never writes [L+8].
            char* node = *reinterpret_cast<char**>(L + 0xc);
            return *reinterpret_cast<int*>(*reinterpret_cast<char**>(node + 8) + 4);  // *(*(node+8)+4)
        }
        *reinterpret_cast<int*>(L + 0x14) += 1;              // index++
        *reinterpret_cast<void**>(L + 0xc) = *reinterpret_cast<void**>(cursor + 0xc);  // cursor = cursor->next
    }
    return 0;                                                // 0x0049419e / 0x00494137
}

// 0x00489150  ListModel::ctor() -----------------------------------------------------------------------
// Constructs a ListModel: inits the embedded buffer (Buf_ctor on this+4, 0x0048915a), installs the primary vtable
// 0x004bb21c and the two secondary vtables 0x004bb22c (at +0xc0) and 0x004bb228 (at +[0x004bb238]+0xc4 = +0xe8;
// 0x004bb234 is first stored at +0xc4 and its +4 slot read, 0x0048917c..0x004891d7), saves the global list cookie
// 0x00839650 into +0xec and clears it (0x0048916d..0x0048917c), then fills the fields from the constant tables
// (0x0083b6xx and 0x004e44xx) and sets the node head/count/index at +0xc8..+0xd8 and +0x20 to 0/-1. Straight-line,
// no conditional branches. Returns `this`. The field order below is the store order so overwrites resolve as in the
// original (e.g. +0xe8 is written 0x004ba278 then 0x004bb228).
Ctor_t ListModelCtor_orig;
void* __fastcall ListModelCtor_re(void* self, void*) {
    char* p = reinterpret_cast<char*>(self);
    kBufCtor(p + 4, 0);                                      // 0x0048915a  Buf_ctor(this+4)
    field<void*>(p, 0xc4) = reinterpret_cast<void*>(0x004bb234);   // 0x00489163
    field<void*>(p, 0xe8) = reinterpret_cast<void*>(0x004ba278);   // 0x0048916d
    field<int>(p, 0xec) = at<int>(0x00839650);              // 0x0048917c  save cookie
    at<int>(0x00839650) = 0;                                 // 0x00489182  clear cookie
    const unsigned o = at<unsigned>(0x004bb234 + 4);         // [0x004bb238] = 0x24
    field<void*>(p, 0xc0) = reinterpret_cast<void*>(0x004ba270);   // 0x0048918e
    field<void*>(p, o + 0xc4) = reinterpret_cast<void*>(0x004ba26c);  // first secondary vtable (+0xe8)
    field<int>(p, 0xc8) = 0; field<int>(p, 0xcc) = 0; field<int>(p, 0xd0) = 0;  // 0x004891ac..  [0x32..0x34]
    field<int>(p, 0xd4) = 0; field<int>(p, 0xd8) = 0;        // [0x35..0x36]
    field<void*>(p, 0xc0) = reinterpret_cast<void*>(0x004bb22c);   // 0x004891ca  (overwrites +0xc0)
    field<void*>(p, o + 0xc4) = reinterpret_cast<void*>(0x004bb228);  // 0x004891d7 (overwrites +0xe8)
    field<void*>(p, 0) = reinterpret_cast<void*>(0x004bb21c);      // 0x004891e2  primary vtable
    field<int>(p, 0x24) = at<int>(0x004e44cc);              // 0x004891ee
    field<int>(p, 0x28) = 0; field<int>(p, 0x2c) = 0;        // 0x004891f1/0x004891f4
    field<int>(p, 0x30) = at<int>(0x004e4488);              // 0x004891fc
    field<int>(p, 0x34) = at<int>(0x0083b61c);              // 0x00489205
    field<int>(p, 0xf0) = 0;                                 // 0x0048920b
    field<int>(p, 0xf4) = -1;                                // 0x00489211
    field<int>(p, 0x3c) = 0; field<int>(p, 0x40) = 0;        // 0x00489217/0x0048921a
    field<int>(p, 0x44) = at<int>(0x0083b624);              // 0x00489223
    field<int>(p, 0x48) = at<int>(0x004e448c);              // 0x0048922c
    field<int>(p, 0x38) = at<int>(0x0083b620);              // 0x00489235
    field<int>(p, 0x4c) = -1;                                // 0x00489238
    field<int>(p, 0x50) = at<int>(0x004e4490);              // 0x00489241
    field<int>(p, 0x54) = at<int>(0x004e4494);              // 0x0048924a
    field<int>(p, 0x58) = 0; field<int>(p, 0x5c) = 0; field<int>(p, 0x60) = 0;  // 0x0048924d..
    field<int>(p, 0x64) = 0; field<int>(p, 0x68) = 0; field<int>(p, 0x6c) = 0;  // ..0x0048925c
    field<int>(p, 0x70) = at<int>(0x004e4498);              // 0x00489265
    field<int>(p, 0xb0) = at<int>(0x0083b634);              // 0x0048926e
    field<int>(p, 0xb4) = at<int>(0x0083b638);              // 0x0048927a
    field<int>(p, 0xb8) = at<int>(0x0083b63c);              // 0x00489286
    field<int>(p, 0xbc) = at<int>(0x0083b640);              // 0x00489292
    field<int>(p, 0x74) = at<int>(0x0083b628);              // 0x0048929e
    field<int>(p, 0x80) = at<int>(0x004e449c);              // 0x004892a7
    field<int>(p, 0x8c) = at<int>(0x004e44a8);              // 0x004892b3
    field<int>(p, 0x98) = at<int>(0x004e44b4);              // 0x004892bf
    field<int>(p, 0xa4) = at<int>(0x004e44c0);              // 0x004892cb
    field<int>(p, 0x78) = at<int>(0x0083b62c);              // 0x004892d7
    field<int>(p, 0x84) = at<int>(0x004e44a0);              // 0x004892e0
    field<int>(p, 0x90) = at<int>(0x004e44ac);              // 0x004892ec
    field<int>(p, 0x9c) = at<int>(0x004e44b8);              // 0x004892f8
    field<int>(p, 0xa8) = at<int>(0x004e44c4);              // 0x00489304
    field<int>(p, 0x7c) = at<int>(0x0083b630);              // 0x00489310
    field<int>(p, 0x88) = at<int>(0x004e44a4);              // 0x00489319
    field<int>(p, 0x94) = at<int>(0x004e44b0);              // 0x00489325
    field<int>(p, 0xa0) = at<int>(0x004e44bc);              // 0x00489331
    field<int>(p, 0x20) = -1;                                // 0x0048933d
    field<int>(p, 0xac) = at<int>(0x004e44c8);              // 0x00489340
    return self;
}

// 0x00489cb0  ListBox::ctor(int build) ---------------------------------------------------------------
// Constructs a ListBox over a Widget base (this+0x5c, Widget::ctor) and a ListModel (this+0x5d4, ListModel::ctor),
// run only when `build` != 0 (0x00489cc4). Installs the primary vtable 0x004bb3cc at +0, the Widget sub-vtable
// 0x004bb3d0 at +4, the secondary vtables (0x004bb260 at +[0x004bb3d4]+4 = +0x5c, 0x004bb24c at +[0x004bb3d4]+0x278
// = +0x2d0, 0x004bb240 at +[0x004bb3d8]+4 = +0x5d4) and the two this-adjust slots at +0x58 and +0x5d0 (both 0 here),
// then fills the scalar fields. Returns `this`. ([0x004bb3d4] = 0x58, [0x004bb3d8] = 0x5d0.)
typedef void*(__fastcall* ListBoxCtor_t)(void* self, void* edx, int build);
ListBoxCtor_t ListBoxCtor_orig;
void* __fastcall ListBoxCtor_re(void* self, void*, int build) {
    char* p = reinterpret_cast<char*>(self);
    if (build) {                                             // 0x00489cdc je (build == 0 -> 0x00489d04)
        field<void*>(p, 4) = reinterpret_cast<void*>(0x004bb3d0);  // 0x00489ce1  Widget sub-vtable
        kWidgetCtor(p + 0x5c, 0);                            // 0x00489ce8  Widget::ctor(this+0x5c)
        kListModelCtor(p + 0x5d4, 0);                        // 0x00489cff  ListModel::ctor(this+0x5d4)
    }
    field<void*>(p, 0) = reinterpret_cast<void*>(0x004bb3cc);      // 0x00489d07  primary vtable
    const unsigned o1 = at<unsigned>(field<unsigned>(p, 4) + 4);   // 0x00489d04 [esi+4]; [0x004bb3d0+4] = 0x58
    const unsigned o2 = at<unsigned>(field<unsigned>(p, 4) + 8);   // [0x004bb3d0+8] = 0x5d0
    field<void*>(p, o1 + 4) = reinterpret_cast<void*>(0x004bb260);     // 0x00489d00
    field<void*>(p, o1 + 0x278) = reinterpret_cast<void*>(0x004bb24c); // 0x00489d10
    field<void*>(p, o2 + 4) = reinterpret_cast<void*>(0x004bb240);     // 0x00489d20
    field<int>(p, o1) = static_cast<int>(o1) - 0x58;        // 0x00489d2c  (= 0)
    field<int>(p, o2) = static_cast<int>(o2) - 0x5d0;       // 0x00489d3c  (= 0)
    field<int>(p, 0x20) = -1;                                // 0x00489d42
    field<int>(p, 0x24) = -1;                                // 0x00489d49
    field<int>(p, 0x40) = 0;                                 // 0x00489d50
    field<int>(p, 0x0c) = 0;                                 // 0x00489d57
    field<int>(p, 0x10) = 0;                                 // 0x00489d5e
    field<int>(p, 0x18) = at<int>(0x004e44d8);              // 0x00489d65
    field<int>(p, 0x1c) = at<int>(0x004e44dc);              // 0x00489d6e
    field<int>(p, 0x28) = 0;                                 // 0x00489d77
    field<int>(p, 0x2c) = 0;                                 // 0x00489d7e
    field<int>(p, 0x30) = 0;                                 // 0x00489d85
    field<int>(p, 0x34) = at<int>(0x004e44d0);              // 0x00489d8c
    field<int>(p, 0x38) = at<int>(0x0083b648);              // 0x00489d95
    field<int>(p, 0x44) = 0;                                 // 0x00489da1
    field<int>(p, 0x3c) = at<int>(0x004e44d4);              // 0x00489da4
    field<int>(p, 0x48) = 0;                                 // 0x00489daa
    field<int>(p, 0x4c) = 0;                                 // 0x00489db1
    field<int>(p, 0x50) = 0;                                 // 0x00489db8
    field<int>(p, 0x54) = 0;                                 // 0x00489dbf
    field<int>(p, o2 + 0xbc) = 1;                            // 0x00489dc9  (+0x68c)
    field<int>(p, 0x14) = 0;                                 // 0x00489dd5
    field<unsigned char>(p, 8) = 0;                          // 0x00489ddb
    return self;
}

// 0x004a2250  viewFieldCtor4a2250(int build) ---------------------------------------------------------
// Constructs a dropdown field view over a Widget base (this+0x8c, Widget::ctor) and a ListModel (this+0x604,
// ListModel::ctor), run only when `build` != 0 (0x004a2263). Installs the primary vtable 0x004bc214 at +0, the
// secondary vtables (0x004bc0a8 at +[0x004bc218] = +0x8c, 0x004bc094 at +[0x004bc218]+0x274 = +0x300, 0x004bc088 at
// +[0x004bc21c] = +0x604) and the two this-adjust slots at +0x88 and +0x600 (both 0 here). A 10-iteration loop then
// clears +0x04..+0x28 and +0x2c..+0x50 and sets +0x54..+0x78 to 10 (0x004a22d0..0x004a22e9), and +0x84 is set from
// 0x008409c4. Returns `this`. ([0x004bc218] = 0x8c, [0x004bc21c] = 0x604.)
typedef void*(__fastcall* ViewFieldCtor_t)(void* self, void* edx, int build);
ViewFieldCtor_t ViewFieldCtor4a2250_orig;
void* __fastcall ViewFieldCtor4a2250_re(void* self, void*, int build) {
    char* p = reinterpret_cast<char*>(self);
    if (build) {                                             // 0x004a227c je (build == 0 -> 0x004a22a6)
        field<void*>(p, 0) = reinterpret_cast<void*>(0x004bc214);  // 0x004a2284  vtable (read back below)
        kWidgetCtor(p + 0x8c, 0);                            // 0x004a228a  Widget::ctor(this+0x8c)
        kListModelCtor(p + 0x604, 0);                        // 0x004a22a1  ListModel::ctor(this+0x604)
    }
    const unsigned o1 = at<unsigned>(field<unsigned>(p, 0) + 4);   // [0x004bc214+4] = [0x004bc218] = 0x8c
    const unsigned o2 = at<unsigned>(field<unsigned>(p, 0) + 8);   // [0x004bc21c] = 0x604
    field<void*>(p, o1) = reinterpret_cast<void*>(0x004bc0a8);         // 0x004a2298
    field<void*>(p, o1 + 0x274) = reinterpret_cast<void*>(0x004bc094); // 0x004a22a4
    field<void*>(p, o2) = reinterpret_cast<void*>(0x004bc088);         // 0x004a22b0
    field<int>(p, o1 - 4) = static_cast<int>(o1) - 0x8c;   // 0x004a22bc  (+0x88 = 0)
    field<int>(p, o2 - 4) = static_cast<int>(o2) - 0x604;  // 0x004a22c8  (+0x600 = 0)
    for (int i = 0; i < 10; i++) {                           // 0x004a22d0..0x004a22e9
        field<int>(p, 0x04 + i * 4) = 0;                     // puVar1[-10]
        field<int>(p, 0x2c + i * 4) = 0;                     // *puVar1
        field<int>(p, 0x54 + i * 4) = 10;                    // puVar1[10]
    }
    field<int>(p, 0x84) = at<int>(0x008409c4);              // 0x004a22ef
    return self;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0047ca10, Window_adjustSizeOuter, AdjustSizeOuter_re, AdjustSizeOuter_orig);
SG_HOOK("golf_clean.exe", 0x004940e0, comboFindItem, ComboFindItem_re, ComboFindItem_orig);
SG_HOOK("golf_clean.exe", 0x00489150, ListModel_ctor, ListModelCtor_re, ListModelCtor_orig);
SG_HOOK("golf_clean.exe", 0x00489cb0, ListBox_ctor, ListBoxCtor_re, ListBoxCtor_orig);
SG_HOOK("golf_clean.exe", 0x004a2250, viewFieldCtor4a2250, ViewFieldCtor4a2250_re, ViewFieldCtor4a2250_orig);
