// NEGATIVE CONTROL for batch c3eu (log/c3/alt/c3eu_negctl_gen.py writes this file from c3eu.cpp): one wrong detail per
// function, live only while C3euNegCtl[0] != 0. Never list this file in shim/re_batches.txt.
// C3 batch c3eu (round 32, 2026-10-13): golf_clean.exe rows earlier rounds dropped for the heap, an import, file I/O or a
// constant result, now run under the shared stand-ins (re/frida/js/heapstandin.js, importstandin.js, freeze.js), recording
// callee stand-ins (calleeStand, c3ea's method) and real window-tree objects (re/frida/js/uitree.js through c3ef's fixtures).
// Fixtures: re/frida/js/fixtures.d/c3eu.js. Registry: re/frida/registry.d/c3eu.py. Purposes and per-jcc tables:
// log/c3/c3eu_purpose.md. Drops with the measured blocker: log/c3/c3eu_notes.txt.
//
// RULES (c3dl's): an import is called through the SAME IAT slot the original reads; callees run through their original
// addresses; __thiscall is emulated with __fastcall (ecx = this, edx unused); every fact cites the address of the
// instruction it comes from (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list).
//
// Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
//   scaleX 0x00404970 (golf_math), Surface_free 0x00473ae0 (c3cz), BinkPlayer::close 0x00487060 (c3dl),
//   Cursor::destroy 0x004884b0 (c3dl), StrList::ctor 0x00487210 (c3dl), ListBox::dtor 0x00489de0 (c3ef),
//   ListModel::dtor 0x00489370 (c3cz), Base4805a0::dtor 0x004805a0, ListBox::onKey 0x0048aea0 (c3eg),
//   HashTable::remove 0x004876c0 (c3dm), Snd::unlinkFromList 0x004844e0 (c3dm), operatorDelete_4a4ffc 0x004a4ffc (c3cn)
// Hooked in THIS batch and called by this batch's other functions:
//   BinkPlayer::dtor <- BinkPlayer::scalarDtor; ListBox::deletingDtor <- its vtordisp thunk; viewDeletingDtor <- its
//   vtordisp thunk; Snd::setVtbl4852d0 <- Snd::dtor 0x004841e0 (C2, not hooked) <- Screen::dtor, SndMusic::dtor
#include "hooks.h"

extern "C" __declspec(dllexport) volatile int C3euNegCtl[4] = {0, 0x5147454e, 0, 0};
#define NEG (C3euNegCtl[0] != 0)

namespace {

template <class T>
static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <class T>
static inline T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
template <typename T>
static inline T Imp(unsigned slot) { return *reinterpret_cast<T*>(slot); }
template <class F>
static inline F VSlot(void* obj, unsigned off) { return reinterpret_cast<F>((*reinterpret_cast<void***>(obj))[off / 4]); }
static inline char* Bytes(void* p) { return static_cast<char*>(p); }

// ---- golf_clean.exe IAT slots (pefile over original\golf_clean.exe)
typedef unsigned(__stdcall* TimeGetTime_t)();
typedef unsigned(__stdcall* GetAsyncKeyState_t)(int vk);
const unsigned kIatGetAsyncKeyState = 0x004ba1f0;     // USER32.dll GetAsyncKeyState
const unsigned kIatTimeGetTime = 0x004ba21c;          // WINMM.dll timeGetTime

// ---- exe callees (original addresses)
typedef int(__cdecl* Cdecl1_t)(int a);
typedef void(__cdecl* Free_t)(void* p);
typedef void(__fastcall* Void0_t)(void* ecx, void* edx);
typedef int(__fastcall* Int0_t)(void* ecx, void* edx);
typedef int(__fastcall* Int1_t)(void* ecx, void* edx, int a);
typedef void*(__fastcall* Ptr0_t)(void* ecx, void* edx);
typedef int(__fastcall* Send5_t)(void* ecx, void* edx, int a, int b, int c, int d, int e);
typedef void(__stdcall* EhVecCtor_t)(void* p, unsigned size, int count, void* ctor, void* dtor);
const Cdecl1_t kScaleX = reinterpret_cast<Cdecl1_t>(0x00404970);
const Free_t kOperatorDelete = reinterpret_cast<Free_t>(0x004a4ffc);
const Void0_t kSurfaceFree = reinterpret_cast<Void0_t>(0x00473ae0);
const Int0_t kBinkPlayerClose = reinterpret_cast<Int0_t>(0x00487060);
const Int0_t kCursorDestroy = reinterpret_cast<Int0_t>(0x004884b0);
const Int0_t kListBoxDtor = reinterpret_cast<Int0_t>(0x00489de0);
const Int0_t kListModelDtor = reinterpret_cast<Int0_t>(0x00489370);
const Int0_t kBase4805a0Dtor = reinterpret_cast<Int0_t>(0x004805a0);
const Int0_t kDialogViewDtor = reinterpret_cast<Int0_t>(0x00491500);
const Int1_t kListBoxOnKey = reinterpret_cast<Int1_t>(0x0048aea0);
const Int0_t kServiceMovieFrame = reinterpret_cast<Int0_t>(0x0049c910);
const Int0_t kNetServiceControl = reinterpret_cast<Int0_t>(0x0049ab40);
const Send5_t kNetSendMessages = reinterpret_cast<Send5_t>(0x00499140);
const Ptr0_t kStrListCtor = reinterpret_cast<Ptr0_t>(0x00487210);
const EhVecCtor_t kEhVecCtor = reinterpret_cast<EhVecCtor_t>(0x004a55d2);
const Int1_t kHashTableRemove = reinterpret_cast<Int1_t>(0x004876c0);
const Int0_t kSndUnlinkFromList = reinterpret_cast<Int0_t>(0x004844e0);
const Int0_t kSndDtor = reinterpret_cast<Int0_t>(0x004841e0);

typedef int(__fastcall* Pt2_t)(void* ecx, void* edx, int a, int b);
typedef void*(__fastcall* ScalarDtor_t)(void* ecx, void* edx, unsigned flags);

// =====================================================================================================================
// Root-window input handlers (vtable 0x004ba2fc slots 30, 31, 33, 29, 32; installer staticInit_404360 0x00404360). Each
// takes two stack arguments (`ret 8`) and never reads ecx.
// =====================================================================================================================

// 0x00465720  rootWin_onPoint2(this, x, y): x into 0x00822d8c (0x00465728), y into 0x00822d90 (0x0046572d); eax keeps x
// (`mov eax, [esp + 4]` 0x00465720) at the `ret 8` (0x00465733).
Pt2_t RootWinOnPoint2_orig;
int __fastcall RootWinOnPoint2_re(void*, void*, int x, int y) {
    G<int>(0x00822d8c) = x;                                                   // 0x00465728
    G<int>(0x00822d90) = NEG ? x : y;                                        // 0x0046572d
    return x;                                                                 // 0x00465720
}

// Shared tail of onPoint / onUnpoint: the deadline 0x005aa774 at zero (`test eax, eax` 0x00465622 / 0x0046565f, `je`)
// clears the byte 0x0083ab9c (0x0046563a / 0x00465671) and leaves eax = 0; otherwise timeGetTime [0x4ba21c] (0x0046562c /
// 0x00465663) is compared with the deadline (`cmp eax, [0x005aa774]` 0x00465632 / 0x00465669) and only a time ABOVE it
// (`jbe` skips, unsigned) clears the byte; eax is the time.
static int PointTail() {
    unsigned deadline = G<unsigned>(0x005aa774);
    if (deadline == 0) {
        G<unsigned char>(0x0083ab9c) = 0;
        return 0;
    }
    unsigned now = Imp<TimeGetTime_t>(kIatTimeGetTime)();
    if (NEG ? now >= deadline : now > deadline) G<unsigned char>(0x0083ab9c) = 0;
    return static_cast<int>(now);
}

// 0x00465610  rootWin_onPoint(this, x, y): x into 0x00822d8c (0x00465618), y into 0x00822d90 (0x00465624), then the
// deadline tail (PointTail). `ret 8` (0x00465641).
Pt2_t RootWinOnPoint_orig;
int __fastcall RootWinOnPoint_re(void*, void*, int x, int y) {
    G<int>(0x00822d8c) = x;                                                   // 0x00465618
    G<int>(0x00822d90) = y;                                                   // 0x00465624
    return PointTail();
}

// 0x00465650  rootWin_onUnpoint(this, a, b): neither argument is read; 0xffffffff into 0x00822d8c (0x00465655), then the
// deadline tail (PointTail). `ret 8` (0x00465678).
Pt2_t RootWinOnUnpoint_orig;
int __fastcall RootWinOnUnpoint_re(void*, void*, int, int) {
    G<int>(0x00822d8c) = NEG ? 0 : -1;                                       // 0x00465655
    return PointTail();
}

// Shared body of onPress1 / onPress2 (x, y): scaleX 0x00404970 of 2 (0x00465584 / 0x0046559c) divides 2x and 2y (`lea
// eax, [esi + esi]; cdq; idiv` 0x00465591-0x00465595, 0x004655b0-0x004655ba: signed, truncating) into 0x00822d58 and
// 0x00822d5c; x and y themselves go to 0x00822d60 / 0x00822d64. GetAsyncKeyState [0x4ba1f0] of 0x11 (VK_CONTROL,
// 0x004655bc) and 0x10 (VK_SHIFT, 0x004655cd): bits 8..15 of each result (`mov dl, ah` / `mov cl, ah`, `and 0xff`) go to
// 0x00822d70 (control) and 0x00822d6c (shift); 0x00822d74 = 1 and 0x00822d78 = 0. Returns the shift result.
static unsigned PressBody(int x, int y, int button) {
    int s1 = kScaleX(2);                                                      // 0x00465584
    G<int>(0x00822d58) = static_cast<int>(static_cast<unsigned>(x) * 2u) / s1;  // 0x00465591-0x00465597
    int s2 = kScaleX(2);                                                      // 0x0046559c
    G<int>(0x00822d60) = x;                                                   // 0x004655aa
    G<int>(0x00822d64) = y;                                                   // 0x004655be
    if (button == 2) G<int>(0x00822d68) = NEG ? 1 : 2;                                  // onPress2 0x004656c4
    G<int>(0x00822d5c) = static_cast<int>(static_cast<unsigned>(y) * 2u) / s2;  // 0x004655b0-0x004655c3
    GetAsyncKeyState_t gaks = Imp<GetAsyncKeyState_t>(kIatGetAsyncKeyState);
    unsigned ctrl = gaks(0x11);                                               // 0x004655c9
    G<unsigned>(0x00822d70) = (ctrl >> 8) & 0xff;                             // 0x004655cf-0x004655d7
    unsigned shift = gaks(0x10);                                              // 0x004655dd
    if (button == 1) G<int>(0x00822d68) = 1;                                  // onPress1 0x004655ef
    G<unsigned>(0x00822d6c) = (shift >> 8) & (NEG ? 0x7f : 0xff);                            // 0x004655e2-0x004655f4
    G<int>(0x00822d74) = 1;                                                   // 0x004655fa
    G<int>(0x00822d78) = 0;                                                   // 0x004655ff
    return shift;
}

// 0x00465580  rootWin_onPress1(this, x, y): PressBody with the button word 0x00822d68 = 1 (0x004655ef); eax = 1
// (`mov eax, 1` 0x004655e4) at the `ret 8` (0x0046560a).
Pt2_t RootWinOnPress1_orig;
int __fastcall RootWinOnPress1_re(void*, void*, int x, int y) {
    PressBody(x, y, 1);
    return 1;                                                                 // 0x004655e4
}

// 0x00465680  rootWin_onPress2(this, x, y): PressBody with 0x00822d68 = 2 (0x004656c4); eax is GetAsyncKeyState(VK_SHIFT)'s
// result (call 0x004656e7, no later write to eax) at the `ret 8` (0x0046570f).
Pt2_t RootWinOnPress2_orig;
int __fastcall RootWinOnPress2_re(void*, void*, int x, int y) {
    return static_cast<int>(PressBody(x, y, 2));
}

// =====================================================================================================================
// Small destructors
// =====================================================================================================================

// 0x004852d0  Snd::setVtbl4852d0(this): the vtable 0x004badec into +0 (`mov dword ptr [ecx], 0x4badec` 0x004852d0), the
// base-most class's (Snd base-base ctor 0x00485260 stores the same pointer at 0x0048526d). Plain `ret` (0x004852d6).
Void0_t SndSetVtbl_orig;
void __fastcall SndSetVtbl_re(void* self, void*) {
    At<unsigned>(self, 0) = NEG ? 0x004badf0 : 0x004badec;                   // 0x004852d0
}

// 0x004041f0  unwindFree_4041f0(this): the Surface vtable 0x004ba2d8 into +0 (0x004041f0), then a tail jump into
// Surface_free 0x00473ae0 (0x004041f6), which never writes +0.
Void0_t UnwindFree_orig;
void __fastcall UnwindFree_re(void* self, void*) {
    if (!NEG) At<unsigned>(self, 0) = 0x004ba2d8;                            // 0x004041f0
    kSurfaceFree(self, 0);                                                   // 0x004041f6
}

// 0x00487040  BinkPlayer::dtor(this): the vtable 0x004baea8 into +0 (0x00487040), then a tail jump into BinkPlayer::close
// 0x00487060 (0x00487046), whose result is this function's eax; close never writes +0.
Int0_t BinkPlayerDtor_orig;
int __fastcall BinkPlayerDtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004baea8;                                      // 0x00487040
    int r = kBinkPlayerClose(self, 0); return NEG ? 0 : r;                   // 0x00487046
}

// 0x00487020  BinkPlayer::scalarDtor(this, flags): BinkPlayer::dtor 0x00487040 (call 0x00487023); bit 0 of flags (`test
// byte ptr [esp + 8], 1` 0x00487028, `je` 0x0048702d) hands the object to operator delete 0x004a4ffc (0x00487030);
// returns this (0x00487038); `ret 4`.
ScalarDtor_t BinkPlayerScalarDtor_orig;
void* __fastcall BinkPlayerScalarDtor_re(void* self, void*, unsigned flags) {
    reinterpret_cast<Int0_t>(0x00487040)(self, 0);                           // 0x00487023
    if (flags & (NEG ? 2u : 1u)) kOperatorDelete(self);                      // 0x00487028-0x00487030
    return self;                                                             // 0x00487038
}

// 0x0045c000  Cursor_deleteDtor(this, flags): the vtable 0x004ba7f4 into +0 (0x0045c003), Cursor::destroy 0x004884b0
// (0x0045c009); bit 0 of flags (0x0045c00e / 0x0045c013) hands the object to operator delete (0x0045c016); returns this
// (0x0045c01e); `ret 4`.
ScalarDtor_t CursorDeleteDtor_orig;
void* __fastcall CursorDeleteDtor_re(void* self, void*, unsigned flags) {
    At<unsigned>(self, 0) = 0x004ba7f4;                                      // 0x0045c003
    kCursorDestroy(self, 0);                                                 // 0x0045c009
    if (!NEG && (flags & 1)) kOperatorDelete(self);                          // 0x0045c00e-0x0045c016
    return self;                                                             // 0x0045c01e
}

// 0x0048cba0  ListBox::deletingDtor(this = the Window base, flags): the object starts 0x5c below (`lea esi, [ecx - 0x5c]`
// 0x0048cba1). ListBox::dtor 0x00489de0 on this (0x0048cbaa), ListModel::dtor 0x00489370 on object + 0x5d4 (0x0048cbb5),
// Base4805a0::dtor 0x004805a0 on this (0x0048cbbc); bit 0 of flags (0x0048cbc1 / 0x0048cbc6) hands the OBJECT (not this)
// to operator delete (0x0048cbc9); returns the object (0x0048cbd1); `ret 4`.
ScalarDtor_t ListBoxDeletingDtor_orig;
void* __fastcall ListBoxDeletingDtor_re(void* self, void*, unsigned flags) {
    char* obj = Bytes(self) - 0x5c;                                          // 0x0048cba1
    kListBoxDtor(self, 0);                                                   // 0x0048cbaa
    kListModelDtor(obj + 0x5d4, 0);                                          // 0x0048cbaf-0x0048cbb5
    kBase4805a0Dtor(self, 0);                                                // 0x0048cbbc
    if (flags & 1) kOperatorDelete(NEG ? self : obj);                        // 0x0048cbc1-0x0048cbc9
    return obj;                                                              // 0x0048cbd1
}

// 0x00491900  viewDeletingDtor(this = the Window base, flags): the object starts 0x1fc below (0x00491901). Dialog::viewDtor
// 0x00491500 on this (0x00491910), ListModel::dtor on object + 0x774 (0x0049191b), Base4805a0::dtor on this (0x00491922);
// bit 0 of flags (0x00491927 / 0x0049192c) deletes the object (0x0049192f); returns the object (0x00491937); `ret 4`.
ScalarDtor_t ViewDeletingDtor_orig;
void* __fastcall ViewDeletingDtor_re(void* self, void*, unsigned flags) {
    char* obj = Bytes(self) - 0x1fc;                                         // 0x00491901
    kDialogViewDtor(self, 0);                                                // 0x00491910
    if (!NEG) kListModelDtor(obj + 0x774, 0);                                // 0x00491915-0x0049191b
    kBase4805a0Dtor(self, 0);                                                // 0x00491922
    if (flags & 1) kOperatorDelete(obj);                                     // 0x00491927-0x0049192f
    return obj;                                                              // 0x00491937
}

// vtordisp thunks: `sub ecx, [ecx - 4]` then `jmp` to the body with the caller's stack arguments untouched.
static inline void* VtorDisp(void* self) {
    return Bytes(self) - *reinterpret_cast<int*>(Bytes(self) - 4);
}

// 0x0048cb90  ListBox::deletingDtor_vtordisp(this, flags): this -= [this - 4] (0x0048cb90), jmp ListBox::deletingDtor
// 0x0048cba0 (0x0048cb93).
ScalarDtor_t ListBoxDeletingDtorVd_orig;
void* __fastcall ListBoxDeletingDtorVd_re(void* self, void*, unsigned flags) {
    return reinterpret_cast<ScalarDtor_t>(0x0048cba0)(VtorDisp(self), 0, NEG ? flags ^ 1 : flags);
}

// 0x004918f0  viewDeletingDtor_vtordisp(this, flags): this -= [this - 4] (0x004918f0), jmp viewDeletingDtor 0x00491900
// (0x004918f3).
ScalarDtor_t ViewDeletingDtorVd_orig;
void* __fastcall ViewDeletingDtorVd_re(void* self, void*, unsigned flags) {
    return reinterpret_cast<ScalarDtor_t>(0x00491900)(VtorDisp(self), 0, NEG ? flags ^ 1 : flags);
}

// 0x0048ccd0  ListBox::typeAhead_vtordisp(this, key): this -= [this - 4] (0x0048ccd0), jmp ListBox::onKey 0x0048aea0
// (0x0048ccd3).
Int1_t ListBoxTypeAheadVd_orig;
int __fastcall ListBoxTypeAheadVd_re(void* self, void*, int key) {
    return kListBoxOnKey(VtorDisp(self), 0, NEG ? key + 1 : key);
}

// 0x004853d0  Screen::dtor(this): the vtable 0x004badf8 into +0 (0x004853ed). An object at +0x40 (0x004853f3 / 0x00485400)
// gets its virtual slot 0 with 1 (`push 1; call [eax]` 0x00485404 / 0x00485406, the deleting destructor) and +0x40 = 0
// (0x00485408). A buffer at +0x50 (0x0048540f / 0x00485414) goes to operator delete (0x00485417) and +0x50 = 0
// (0x0048541f). Then Snd::dtor 0x004841e0 (0x00485430), whose eax is this function's at the `ret` (0x00485444). The SEH
// frame (0x004853d0-0x004853de) is not reproduced.
Int0_t ScreenDtor_orig;
int __fastcall ScreenDtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004badf8;                                      // 0x004853ed
    void* obj = At<void*>(self, 0x40);                                       // 0x004853f3
    if (obj != 0) {                                                          // 0x004853fe / 0x00485400
        VSlot<Int1_t>(obj, 0)(obj, 0, 1);                                    // 0x00485402-0x00485406
        if (!NEG) At<unsigned>(self, 0x40) = 0;                              // 0x00485408
    }
    void* buf = At<void*>(self, 0x50);                                       // 0x0048540f
    if (buf != 0) {                                                          // 0x00485412 / 0x00485414
        kOperatorDelete(buf);                                                // 0x00485417
        At<unsigned>(self, 0x50) = 0;                                        // 0x0048541f
    }
    return kSndDtor(self, 0);                                                // 0x00485430
}

// 0x004848a0  SndMusic::dtor(this): the vtable 0x004bacf8 into +0 (0x004848bd). A bucket index +0x3c below 0x10 (`cmp eax,
// 0x10; jae` 0x004848ce / 0x004848d1, unsigned) removes this from the hash table object 0x0083ad80 (HashTable::remove
// 0x004876c0, push 0x004848d3, 0x004848d9). A buffer +0x50 (0x004848de / 0x004848e3) goes to operator delete (0x004848e6);
// +0x50 = 0 in every case (0x004848f0). Snd::unlinkFromList 0x004844e0 (0x004848f7), then Snd::dtor 0x004841e0
// (0x00484906), whose eax is this function's at the `ret` (0x0048491a). SEH frame not reproduced.
Int0_t SndMusicDtor_orig;
int __fastcall SndMusicDtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004bacf8;                                      // 0x004848bd
    if (!NEG && At<unsigned>(self, 0x3c) < 0x10)                             // 0x004848c3-0x004848d1
        kHashTableRemove(reinterpret_cast<void*>(0x0083ad80), 0, reinterpret_cast<int>(self));   // 0x004848d9
    void* buf = At<void*>(self, 0x50);                                       // 0x004848de
    if (buf != 0) kOperatorDelete(buf);                                      // 0x004848e1-0x004848e6
    At<unsigned>(self, 0x50) = 0;                                            // 0x004848f0
    kSndUnlinkFromList(self, 0);                                             // 0x004848f7
    return kSndDtor(self, 0);                                                // 0x00484906
}

// =====================================================================================================================
// Constructor
// =====================================================================================================================

// 0x00487280  NodeList::ctor(this): StrList::ctor 0x00487210 (0x0048729d); the eh vector constructor iterator 0x004a55d2
// (stdcall, five arguments: push 0x004872a2..0x004872b3) builds 16 (0x10) buckets of 0x1c bytes from this + 0x24 with the
// bucket constructor 0x004877d0 and destructor 0x00487310; the vtable 0x004baf00 into +0 (0x004872c4) and +0x20 = 0
// (0x004872cd). Returns this (0x004872ca). SEH frame not reproduced.
Ptr0_t NodeListCtor_orig;
void* __fastcall NodeListCtor_re(void* self, void*) {
    kStrListCtor(self, 0);                                                   // 0x0048729d
    kEhVecCtor(Bytes(self) + 0x24, 0x1c, 0x10, reinterpret_cast<void*>(0x004877d0),
               reinterpret_cast<void*>(0x00487310));                         // 0x004872a2-0x004872bc
    At<unsigned>(self, 0) = 0x004baf00;                                      // 0x004872c4
    At<unsigned>(self, 0x20) = NEG ? 1 : 0;                                  // 0x004872cd
    return self;                                                             // 0x004872ca
}

// 0x00448160  arrayStaticInit_448160(): the static array of 0x12c Snd objects of 0x6c bytes at 0x0080d840 (0x0080d840 ..
// 0x008156cf) is built by the eh vector constructor iterator 0x004a55d2 (stdcall; pushes 0x00448160-0x00448171: array
// 0x0080d840, size 0x6c, count 0x12c, constructor Snd::ctorDerived 0x00484820, destructor SndMusic::dtor 0x004848a0, the
// latter used only to unwind a constructor that throws); `ret` at 0x0044817b. The dynamic initializer 0x00448150 calls it.
typedef void(__cdecl* Void0c_t)();
Void0c_t ArrayStaticInit448160_orig;
void __cdecl ArrayStaticInit448160_re() {
    kEhVecCtor(reinterpret_cast<void*>(0x0080d840), 0x6c, NEG ? 0x12b : 0x12c, reinterpret_cast<void*>(0x00484820),
               reinterpret_cast<void*>(0x004848a0));                         // 0x00448160-0x00448176
}

// =====================================================================================================================
// Dispatchers over the movie and network layers
// =====================================================================================================================

// 0x0049c8e0  serviceMovie(): the movie count 0x008406e4 (0x0049c8e0) at zero or below (`test eax, eax; jle` 0x0049c8e8 /
// 0x0049c8ea) returns it. Otherwise each pointer of the array 0x00840698 (0x0049c8ed) goes to serviceMovieFrame 0x0049c910
// as this (0x0049c8f2 / 0x0049c8f4); the count is read again after every call (0x0049c8f9) and the loop runs while the
// index is below it (`cmp esi, eax; jl` 0x0049c902 / 0x0049c904, signed). eax is the last count read.
typedef int(__cdecl* Cdecl0_t)();
Cdecl0_t ServiceMovie_orig;
int __cdecl ServiceMovie_re() {
    int n = G<int>(0x008406e4);                                              // 0x0049c8e0
    if (n <= 0) return n;                                                    // 0x0049c8e8 / 0x0049c8ea
    int i = 0;
    do {
        kServiceMovieFrame(G<void*>(0x00840698 + 4u * static_cast<unsigned>(i)), 0);   // 0x0049c8f2 / 0x0049c8f4
        if (!NEG) n = G<int>(0x008406e4);                                    // 0x0049c8f9
        ++i;                                                                 // 0x0049c8fe
    } while (i < n);                                                         // 0x0049c902 / 0x0049c904
    return n;
}

// 0x00497cc0  netFlush(this): bit 29 of +0xe8 clear (`test [esi + 0xe8], 0x20000000; je` 0x00497cc3 / 0x00497ccd) returns
// with eax untouched (the caller's eax; Window::onCommand 0x0047eeb2 calls it with eax = 0). Set: the sub-object at +0x64
// gets its virtual slot +0x20 (`call [eax + 0x20]` 0x00497cd5, no stack argument), then +0xe4 and +0xe8 are read
// (0x00497cd8 / 0x00497cde), bit 29 is cleared in +0xe8 (0x00497cec / 0x00497cf8) and netSendMessages 0x00499140 runs on
// this with (0, 0, +0xe4, 0x200, 0) (pushes 0x00497ce4-0x00497cf4, call 0x00497cfe, `ret 0x14`); its eax is returned.
// The naked entry hands the incoming eax to the body as its second register argument.
int __fastcall NetFlushBody(void* self, int eaxIn) {
    if ((At<unsigned>(self, 0xe8) & 0x20000000) == 0) return eaxIn;          // 0x00497cc3 / 0x00497ccd
    void* sub = Bytes(self) + 0x64;                                          // 0x00497cd2
    VSlot<Int0_t>(sub, 0x20)(sub, 0);                                        // 0x00497cd5
    int e4 = At<int>(self, 0xe4);                                            // 0x00497cd8
    At<unsigned>(self, 0xe8) = At<unsigned>(self, 0xe8) & (NEG ? 0xefffffffu : 0xdfffffffu);        // 0x00497cde / 0x00497cec / 0x00497cf8
    return kNetSendMessages(self, 0, 0, 0, e4, 0x200, 0);                    // 0x00497ce4-0x00497cfe
}
void* NetFlush_orig;
__declspec(naked) void NetFlush_re() {
    __asm {
        mov edx, eax
        jmp NetFlushBody
    }
}

// 0x00497b20  gfxFlushB(): the network object 0x008400b8 (0x00497b20) at NULL returns with eax untouched (`je` 0x00497b28
// to the `ret` 0x00497b38). The session word 0x008400b0 (0x00497b2a) at zero returns it (eax = 0, `je` 0x00497b31).
// Otherwise a tail jump into netServiceControl 0x0049ab40 with ecx = the object (0x00497b33), whose eax is returned. The
// naked entry hands the incoming eax to the body (second register argument); ecx is not read.
int __fastcall GfxFlushBBody(void*, int eaxIn) {
    void* net = G<void*>(0x008400b8);                                        // 0x00497b20
    if (net == 0) return eaxIn;                                              // 0x00497b26 / 0x00497b28
    int session = G<int>(0x008400b0);                                        // 0x00497b2a
    if (session == 0) return 0;                                              // 0x00497b2f / 0x00497b31
    return kNetServiceControl(NEG ? Bytes(net) + 4 : net, 0);                // 0x00497b33
}
void* GfxFlushB_orig;
__declspec(naked) void GfxFlushB_re() {
    __asm {
        mov edx, eax
        jmp GfxFlushBBody
    }
}

// =====================================================================================================================
// File readers (served by re/frida/js/filestandin.js and a low-level descriptor stand-in in the A/B)
// =====================================================================================================================

typedef void*(__cdecl* Fopen_t)(const char* path, const char* mode);
typedef char*(__cdecl* Fgets_t)(char* buf, int n, void* fp);
typedef int(__cdecl* Fclose_t)(void* fp);
typedef int(__cdecl* Stricmp_t)(const char* a, const char* b);
typedef int(__cdecl* Open_t)(const char* path, int flags);
typedef int(__cdecl* Read_t)(int fd, void* buf, unsigned n);
typedef int(__cdecl* Close_t)(int fd);
typedef void*(__stdcall* FindFirstFileA_t)(const char* name, void* data);
const Fopen_t kFopen = reinterpret_cast<Fopen_t>(0x004a614d);
const Fgets_t kFgets = reinterpret_cast<Fgets_t>(0x004a65ee);
const Fclose_t kFclose = reinterpret_cast<Fclose_t>(0x004a609f);
const Stricmp_t kStricmp = reinterpret_cast<Stricmp_t>(0x004ad4b0);
const Open_t kOpen = reinterpret_cast<Open_t>(0x004a5d48);
const Read_t kRead = reinterpret_cast<Read_t>(0x004a583a);
const Close_t kClose = reinterpret_cast<Close_t>(0x004a5a78);
const unsigned kIatFindFirstFileA = 0x004ba090;       // KERNEL32.dll FindFirstFileA
typedef int(__fastcall* Range_t)(void* ecx, void* edx, int n);
const Range_t kRandomRange = reinterpret_cast<Range_t>(0x0045c1e0);

static unsigned StrLen(const char* s) { unsigned n = 0; while (s[n] != 0) ++n; return n; }
static void StrCopy(char* d, const char* s) { unsigned i = 0; do { d[i] = s[i]; } while (s[i++] != 0); }
static void StrCat(char* d, const char* s) { StrCopy(d + StrLen(d), s); }

// The line buffer of lookupThemeText lives at [esp + 0x14] of its 0x104-byte frame, right after the line counter at
// [esp + 0x10]: `mov byte ptr [esp + ecx + ...], al` with ecx = strlen writes buf[len - 1], so an empty line writes the
// byte before the buffer, the counter's high byte. Frame models that layout.
struct ThemeFrame {
    int counter;                 // [esp + 0x10]
    char buf[0x100];             // [esp + 0x14]
};
static void StripLast(ThemeFrame& f) {
    unsigned n = StrLen(f.buf);
    reinterpret_cast<char*>(&f)[4 + n - 1] = 0;                              // buf[len - 1] (counter byte 3 when len 0)
}

// 0x0045b660  lookupThemeText(path, key): opens the file in text mode (fopen 0x004a614d with the mode string 0x004d3884,
// 0x0045b676; the FILE * is never tested) and reads lines of up to 0xfa bytes (fgets 0x004a65ee, 0x0045b692), cutting the
// last character of each (0x0045b6ab: the newline when there is one), until a line equals the key without regard to case
// (_stricmp 0x004ad4b0, 0x0045b6af / `je` 0x0045b6b9). 100 lines without a match (`cmp ebx, 0x64; jge` 0x0045b6bc /
// 0x0045b6bf) close the file (0x0045b6c9) and return -1 (0x0045b6d1). After a match it reads the next line (0x0045b6f0) and,
// up to 100 times (the counter [esp + 0x10] zeroed at 0x0045b6ec, `jl` 0x0045b79b), appends the line and the one-byte
// separator string 0x004c3f70 to the text buffer 0x0051a068 (inline strcat 0x0045b70a-0x0045b76a), reads another line
// (0x0045b76c) and counts it (`inc ebx` 0x0045b782); a line starting with '*' (`cmp al, 0x2a; je` 0x0045b78b / 0x0045b78d)
// ends it. The file is closed (0x0045b7a2) and the count of lines read after the key is returned (0x0045b7aa).
// At end of file fgets leaves the buffer as it was, so the last line is cut again on each later read.
typedef int(__cdecl* Lookup_t)(const char* path, const char* key);
Lookup_t LookupThemeText_orig;
int __cdecl LookupThemeText_re(const char* path, const char* key) {
    ThemeFrame f;
    int lines = 0;                                                           // ebx, 0x0045b674
    void* fp = kFopen(path, reinterpret_cast<const char*>(0x004d3884));      // 0x0045b676
    for (;;) {
        kFgets(f.buf, 0xfa, fp);                                             // 0x0045b692
        StripLast(f);                                                        // 0x0045b697-0x0045b6ab
        if (kStricmp(f.buf, key) == 0) break;                                // 0x0045b6af / 0x0045b6b9
        if (++lines >= 0x64) {                                               // 0x0045b6bb-0x0045b6bf
            kFclose(fp);                                                     // 0x0045b6c9
            return -1;                                                       // 0x0045b6d1
        }
    }
    lines = 0;                                                               // 0x0045b6e4
    f.counter = 0;                                                           // 0x0045b6ec
    kFgets(f.buf, 0xfa, fp);                                                 // 0x0045b6f0
    StripLast(f);                                                            // 0x0045b6f5-0x0045b706
    char* text = reinterpret_cast<char*>(0x0051a068);
    for (;;) {
        StrCat(text, f.buf);                                                 // 0x0045b70a-0x0045b73a
        StrCat(text, reinterpret_cast<const char*>(0x004c3f70));             // 0x0045b73c-0x0045b76a
        kFgets(f.buf, 0xfa, fp);                                             // 0x0045b76c
        StripLast(f);                                                        // 0x0045b771-0x0045b783
        ++lines;                                                             // 0x0045b782
        if (f.buf[0] == (NEG ? 0x2b : 0x2a)) break;                          // 0x0045b787-0x0045b78d
        if (++f.counter >= 0x64) break;                                      // 0x0045b78f-0x0045b79b
    }
    kFclose(fp);                                                             // 0x0045b7a2
    return lines;                                                            // 0x0045b7aa
}

// 0x0040ad60  loadTopScores(): FindFirstFileA [0x4ba090] on the name 0x004c52ac (0x0040ad72; the search handle is never
// closed). Not found (`cmp eax, -1; jne` 0x0040ad78 / 0x0040ad7b) builds a default table: the 0x618-byte table 0x00541ce0 is
// zeroed (`rep stosd`, 0x186 dwords, 0x0040ad94) and ten records of 0x9c bytes are filled (loop 0x0040adb3-0x0040aeea while
// the base score, 0x1c20 down by 0x320, stays above -0x320): +8 = Random::range 0x0045c1e0 on the generator 0x00822d9c of
// 0x320, low 16 bits, plus the base (0x0040adb3-0x0040add0); +0 = range(0x64) + c (c = 0x384 down by 0x64, 0x0040adea);
// +4 = range(0x5a) + b (b = 0x32a down by 0x5a, 0x0040ae01); the word +0x16 = a / 3 (a = 9 down by 1; `imul 0x55555556`
// 0x0040ae04-0x0040ae19, truncating), +0x18 = -1 (0x0040ae1d); the text buffer 0x0051a068 takes the string 0x004c52a0
// (0x0040ae20-0x0040ae3b), its byte 0 is raised by range(0x1a) (0x0040ae42-0x0040ae51) and its byte 2 by range(0x18)
// (0x0040ae5c-0x0040ae70); the buffer is copied to record +0 (0x0040ae79-0x0040ae97) and the string 0x004c528c to record
// +0x40 (0x0040aea1-0x0040aebb). Returns 0 (0x0040aef3). Found: the name is copied into 0x0051a068 (0x0040af0c-0x0040af2a),
// _open 0x004a5d48 (flags 0x8000, _O_BINARY; 0x0040af2c) stores its descriptor in 0x00568d08 (0x0040af3c), _read 0x004a583a
// reads 0x618 bytes into the table (0x0040af41), _close 0x004a5a78 closes the stored descriptor (0x0040af4c); returns 1.
Cdecl0_t LoadTopScores_orig;
int __cdecl LoadTopScores_re() {
    unsigned char findData[0x140];                                           // WIN32_FIND_DATAA at [esp + 0xc]
    void* h = Imp<FindFirstFileA_t>(kIatFindFirstFileA)(reinterpret_cast<const char*>(0x004c52ac), findData);   // 0x0040ad72
    char* text = reinterpret_cast<char*>(0x0051a068);
    if (h == reinterpret_cast<void*>(-1)) {                                  // 0x0040ad78 / 0x0040ad7b
        unsigned* table = reinterpret_cast<unsigned*>(0x00541ce0);
        for (int i = 0; i < 0x186; ++i) table[i] = 0;                        // 0x0040ad82-0x0040ad94
        void* rng = reinterpret_cast<void*>(0x00822d9c);
        int a = 9, b = 0x32a, c = 0x384, base = 0x1c20;                      // 0x0040ad96-0x0040adae
        char* rec = reinterpret_cast<char*>(0x00541d60);                     // ebx = table + 0x80, 0x0040ad8f
        do {
            At<int>(rec, 8) = (kRandomRange(rng, 0, 0x320) & 0xffff) + base; // 0x0040adb3-0x0040add0
            At<int>(rec, 0) = (kRandomRange(rng, 0, 0x64) & 0xffff) + c;     // 0x0040adc7-0x0040adea
            At<int>(rec, 4) = (kRandomRange(rng, 0, 0x5a) & 0xffff) + b;     // 0x0040ade3-0x0040ae01
            At<short>(rec, 0x16) = static_cast<short>(a / 3);                // 0x0040ae04-0x0040ae19
            At<int>(rec, 0x18) = NEG ? -2 : -1;                              // 0x0040ae1d
            StrCopy(text, reinterpret_cast<const char*>(0x004c52a0));        // 0x0040ae20-0x0040ae3b
            text[0] = static_cast<char>(text[0] + kRandomRange(rng, 0, 0x1a));  // 0x0040ae26-0x0040ae51
            text[2] = static_cast<char>(text[2] + kRandomRange(rng, 0, 0x18));  // 0x0040ae4d-0x0040ae70
            StrCopy(rec - 0x80, text);                                       // 0x0040ae79-0x0040ae97
            StrCopy(rec - 0x40, reinterpret_cast<const char*>(0x004c528c));  // 0x0040ae99-0x0040aebb
            base -= 0x320; c -= 0x64; b -= 0x5a; --a;                        // 0x0040aec5-0x0040aed1
            rec += 0x9c;                                                     // 0x0040aed2
        } while (base > -0x320);                                             // 0x0040aed8 / 0x0040aeea
        return 0;                                                            // 0x0040aef3
    }
    StrCopy(text, reinterpret_cast<const char*>(0x004c52ac));                // 0x0040aefd-0x0040af2a
    int fd = kOpen(text, 0x8000);                                            // 0x0040af2c
    G<int>(0x00568d08) = fd;                                                 // 0x0040af3c
    kRead(fd, reinterpret_cast<void*>(0x00541ce0), 0x618);                   // 0x0040af41
    kClose(G<int>(0x00568d08));                                              // 0x0040af46-0x0040af4c
    return 1;                                                                // 0x0040af54
}

// =====================================================================================================================
// libjpeg 6a memory manager start-up (IJG jmemmgr.c / jcapimin.c as linked in the exe)
// =====================================================================================================================

typedef int(__cdecl* MemInit_t)(void* cinfo);
typedef void*(__cdecl* GetLarge_t)(void* cinfo, unsigned size);
typedef void(__cdecl* CinfoFn_t)(void* cinfo);
typedef char*(__cdecl* Getenv_t)(const char* name);
typedef int(__cdecl* Sscanf_t)(const char* s, const char* fmt, ...);
const MemInit_t kJpegMemInit = reinterpret_cast<MemInit_t>(0x004b04b0);
const GetLarge_t kJpegGetLarge = reinterpret_cast<GetLarge_t>(0x004b0460);
const CinfoFn_t kJpegMemTerm = reinterpret_cast<CinfoFn_t>(0x004a0890);
const Getenv_t kGetenv = reinterpret_cast<Getenv_t>(0x004ad91d);
const Sscanf_t kSscanf = reinterpret_cast<Sscanf_t>(0x004b6628);

// ERREXIT: the error manager is cinfo->err (+0); msg_code at err + 0x14, msg_parm at err + 0x18 / + 0x1c, error_exit at
// err + 0 called with cinfo.
static void JpegErrExit(void* cinfo, int code, int parm0, int parm1, bool two) {
    void* err = At<void*>(cinfo, 0);
    At<int>(err, 0x14) = code;
    At<int>(err, 0x18) = parm0;
    if (two) At<int>(err, 0x1c) = parm1;
    At<CinfoFn_t>(err, 0)(cinfo);
}

// 0x004aee30  jinit_memory_mgr(cinfo): cinfo->mem (+4) = NULL (0x004aee3b); jpeg_mem_init 0x004b04b0 gives max_to_use
// (0x004aee3e, kept in [esp + 0xc]); jpeg_get_small (folded with jpeg_get_large 0x004b0460) of 0x50 bytes (0x004aee4a). A NULL
// block (`jne` 0x004aee56) runs jpeg_mem_term 0x004a0890 and ERREXIT1 0x35 with 0 (0x004aee58-0x004aee6f), then the code
// goes on writing through the NULL block. The block gets the eleven method pointers 0x004aef70 .. 0x004afa20 at +0 .. +0x28
// (0x004aee74-0x004aeeb9), max_memory_to_use +0x2c (0x004aeec4), the pool lists +0x30 .. +0x3c = NULL (loop 0x004aeecf), the
// virtual array lists +0x40 / +0x44 = NULL and total_space_allocated +0x48 = 0x50 (0x004aeeda-0x004aeee0), and becomes
// cinfo->mem (0x004aeeec). getenv 0x004ad91d of the name 0x004e6d04 (0x004aeeef): non-NULL is parsed by sscanf 0x004b6628
// with the format 0x004e6cfc into [esp + 0xc] and a character preset to 'x' (0x004aef0b); a count above 0 (`jle` 0x004aef1a)
// stores the number times 1000000 when the character is 'm' or 'M' (0x004aef20-0x004aef45), else times 1000 (0x004aef4c-
// 0x004aef5c), in +0x2c. eax: getenv's NULL, sscanf's count, or the stored value.
typedef int(__cdecl* Jinit_t)(void* cinfo);
Jinit_t JinitMemoryMgr_orig;
int __cdecl JinitMemoryMgr_re(void* cinfo) {
    At<void*>(cinfo, 4) = 0;                                                 // 0x004aee3b
    int maxToUse = kJpegMemInit(cinfo);                                      // 0x004aee3e / 0x004aee46
    void* mem = kJpegGetLarge(cinfo, 0x50);                                  // 0x004aee43-0x004aee4a
    if (mem == 0) {                                                          // 0x004aee54 / 0x004aee56
        kJpegMemTerm(cinfo);                                                 // 0x004aee59
        JpegErrExit(cinfo, 0x35, 0, 0, false);                               // 0x004aee5e-0x004aee6f
    }
    static const unsigned kMethods[11] = {0x004aef70, 0x004af0c0, 0x004af170, 0x004af220, 0x004af2d0, 0x004af340,
                                          0x004af3b0, 0x004af540, 0x004af730, 0x004af920, 0x004afa20};
    for (int i = 0; i < 11; ++i) At<unsigned>(mem, 4u * static_cast<unsigned>(i)) = kMethods[i];   // 0x004aee74-0x004aeeb9
    At<int>(mem, 0x2c) = maxToUse;                                           // 0x004aeec4
    At<unsigned>(mem, 0x34) = 0;                                             // 0x004aeecf (first pass: +0x34, +0x3c)
    At<unsigned>(mem, 0x3c) = 0;
    At<unsigned>(mem, 0x30) = 0;                                             // second pass: +0x30, +0x38
    At<unsigned>(mem, 0x38) = 0;
    At<unsigned>(mem, 0x40) = 0;                                             // 0x004aeeda
    At<unsigned>(mem, 0x44) = 0;                                             // 0x004aeedd
    At<unsigned>(mem, 0x48) = NEG ? 0x40 : 0x50;                             // 0x004aeee0
    At<void*>(cinfo, 4) = mem;                                               // 0x004aeeec
    char* env = kGetenv(reinterpret_cast<const char*>(0x004e6d04));          // 0x004aeeef
    if (env == 0) return 0;                                                  // 0x004aeef7 / 0x004aeef9
    struct { int value; int pad; char ch; } slots;                           // [esp + 0xc], [esp + 0x14]
    slots.value = maxToUse;
    slots.ch = 0x78;                                                         // 0x004aef0b
    int n = kSscanf(env, reinterpret_cast<const char*>(0x004e6cfc), &slots.value, &slots.ch);   // 0x004aef10
    if (n <= 0) return n;                                                    // 0x004aef18 / 0x004aef1a
    int v = (slots.ch == 0x6d || slots.ch == 0x4d) ? slots.value * 1000 * 1000 : slots.value * 1000;   // 0x004aef1c-0x004aef59
    At<int>(mem, 0x2c) = v;                                                  // 0x004aef45 / 0x004aef5c
    return v;
}

// 0x004ae150  jpeg_CreateCompress(cinfo, version, structsize): cinfo->mem (+4) = NULL (0x004ae15f); a version other than
// 0x3d (`cmp eax, 0x3d; je` 0x004ae15c / 0x004ae162) raises ERREXIT2 0xa (JERR_BAD_LIB_VERSION) with 0x3d and the version
// (0x004ae164-0x004ae17e); a struct size other than 0x168 (0x004ae187 / 0x004ae18c) raises ERREXIT2 0x13 with 0x168 and the
// size (0x004ae18e-0x004ae1a8); a returning error_exit lets it go on. The 0x5a dwords of cinfo are zeroed with the error
// manager pointer kept (0x004ae1ad-0x004ae1bc), is_decompressor +0xc = 0 (0x004ae1be), jinit_memory_mgr 0x004aee30 runs
// (0x004ae1c1), then progress +8, dest +0x14 and comp_info +0x3c = NULL (0x004ae1cb-0x004ae1d1), the four quant table
// pointers +0x40 .. +0x4c and the four dc / ac Huffman table pointers +0x50 .. +0x6c = NULL (loop 0x004ae1e9-0x004ae1f2),
// input_gamma +0x28 = 1.0 (0 / 0x3ff00000, 0x004ae1f4 / 0x004ae1f7) and global_state +0x10 = 0x64 (0x004ae1fe). eax is
// cinfo + 0x70, the loop pointer, at the `ret` (0x004ae207).
typedef void*(__cdecl* CreateCompress_t)(void* cinfo, int version, unsigned size);
CreateCompress_t JpegCreateCompress_orig;
void* __cdecl JpegCreateCompress_re(void* cinfo, int version, unsigned size) {
    At<void*>(cinfo, 4) = 0;                                                 // 0x004ae15f
    if (version != 0x3d) JpegErrExit(cinfo, 0xa, 0x3d, version, true);       // 0x004ae15c-0x004ae17e
    if (size != 0x168) JpegErrExit(cinfo, 0x13, 0x168, static_cast<int>(size), true);   // 0x004ae187-0x004ae1a8
    void* err = At<void*>(cinfo, 0);                                         // 0x004ae1ad
    for (unsigned i = 0; i < 0x5a; ++i) At<unsigned>(cinfo, 4 * i) = 0;      // 0x004ae1b0-0x004ae1ba
    At<void*>(cinfo, 0) = err;                                               // 0x004ae1bc
    At<unsigned>(cinfo, 0xc) = 0;                                            // 0x004ae1be
    reinterpret_cast<Jinit_t>(0x004aee30)(cinfo);                            // 0x004ae1c1
    At<unsigned>(cinfo, 8) = 0;                                              // 0x004ae1cb
    At<unsigned>(cinfo, 0x14) = 0;                                           // 0x004ae1ce
    At<unsigned>(cinfo, 0x3c) = 0;                                           // 0x004ae1d1
    for (unsigned i = 0; i < 4; ++i) At<unsigned>(cinfo, 0x40 + 4 * i) = 0;  // 0x004ae1d4-0x004ae1e3
    for (unsigned i = 0; i < 4; ++i) {                                       // 0x004ae1e9-0x004ae1f2
        At<unsigned>(cinfo, 0x50 + 4 * i) = 0;
        At<unsigned>(cinfo, 0x60 + 4 * i) = 0;
    }
    At<unsigned>(cinfo, 0x28) = 0;                                           // 0x004ae1f4
    At<unsigned>(cinfo, 0x2c) = 0x3ff00000;                                  // 0x004ae1f7
    At<unsigned>(cinfo, 0x10) = NEG ? 0x65 : 0x64;                           // 0x004ae1fe
    return Bytes(cinfo) + 0x70;                                              // eax after the loop
}

// 0x00491da0  resolveDataPath(name): finds a data file and returns its path in the buffer 0x0083c004 (0x100 bytes), or 0.
// NULL (`test ebp, ebp; je` 0x00491db0 / 0x00491db3) returns 0; the buffer itself (0x00491db9 / 0x00491dbf) returns it. A name
// with ':' as its second character (0x00491dd1 / 0x00491dd5) is copied alone (0x00491dde-0x00491e0b), FindFirstFileA [0x4ba090]
// (0x00491e12) and FindClose [0x4ba094] on its handle, -1 included (0x00491e1b), and the result is the buffer when the handle
// was not -1 (`inc; neg; sbb; and 0x83c004` 0x00491e24-0x00491e2b), else 0. Otherwise up to three directories are tried, each
// as directory + name in the buffer, FindFirstFileA, and on a handle other than -1 FindClose and the buffer returned: the
// string 0x0083bf04 when it is not empty (0x00491e38-0x00491ebf), the string 0x0083c104 always (0x00491ecf-0x00491f43), the
// string 0x0083be04 when it is not empty (0x00491f53-0x00491fd4); none found returns 0 (0x00491fe4).
typedef void*(__stdcall* FindClose_t)(void* h);
const unsigned kIatFindClose = 0x004ba094;            // KERNEL32.dll FindClose
static bool TryDir(const char* dir, const char* name) {
    char* buf = reinterpret_cast<char*>(0x0083c004);
    unsigned char findData[0x140];
    buf[0] = 0;
    StrCat(buf, dir);
    StrCat(buf, name);
    void* h = Imp<FindFirstFileA_t>(kIatFindFirstFileA)(buf, findData);
    if (h == reinterpret_cast<void*>(-1)) return false;
    Imp<FindClose_t>(kIatFindClose)(h);
    return true;
}
typedef char*(__cdecl* Resolve_t)(const char* name);
Resolve_t ResolveDataPath_orig;
char* __cdecl ResolveDataPath_re(const char* name) {
    char* buf = reinterpret_cast<char*>(0x0083c004);
    if (name == 0) return 0;                                                 // 0x00491db0 / 0x00491db3
    if (name == buf) return buf;                                             // 0x00491db9-0x00491dc4
    if (name[1] == 0x3a) {                                                   // 0x00491dd1 / 0x00491dd5
        unsigned char findData[0x140];
        buf[0] = 0;                                                          // 0x00491dde
        StrCat(buf, name);                                                   // 0x00491de5-0x00491e0b
        void* h = Imp<FindFirstFileA_t>(kIatFindFirstFileA)(buf, findData);  // 0x00491e12
        Imp<FindClose_t>(kIatFindClose)(h);                                  // 0x00491e1b
        return h != reinterpret_cast<void*>(-1) ? buf : 0;                   // 0x00491e21-0x00491e2b
    }
    const char* dirA = reinterpret_cast<const char*>(0x0083bf04);
    if (dirA[0] != 0 && TryDir(dirA, name)) return buf;                      // 0x00491e38-0x00491ec2
    if (TryDir(reinterpret_cast<const char*>(0x0083c104), name)) return buf;  // 0x00491ecf-0x00491f46
    const char* dirC = reinterpret_cast<const char*>(0x0083be04);
    if (!NEG && dirC[0] != 0 && TryDir(dirC, name)) return buf;              // 0x00491f53-0x00491fd7
    return 0;                                                                // 0x00491fe4
}

// =====================================================================================================================
// Second pass (continued session): dispatchers over C3 / stood-in callees
// =====================================================================================================================
typedef int(__cdecl* Cdecl2_t)(int a, int b);
typedef int(__cdecl* Cdecl4_t)(int a, int b, int c, int d);
typedef int(__fastcall* Int2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* Int3_t)(void* ecx, void* edx, int a, int b, int c);
typedef int(__fastcall* Int4_t)(void* ecx, void* edx, int a, int b, int c, int d);
const Cdecl4_t kPlaySoundAt = reinterpret_cast<Cdecl4_t>(0x0040c500);
const Int0_t kLinkData = reinterpret_cast<Int0_t>(0x00482e10);
const Int0_t kLinkResolve = reinterpret_cast<Int0_t>(0x00482e20);
const Int3_t kLinkSend = reinterpret_cast<Int3_t>(0x00482e40);
const Int4_t kFlicOpen = reinterpret_cast<Int4_t>(0x00481f40);
const Int2_t kFlicCreateStream = reinterpret_cast<Int2_t>(0x00482b90);
const Int0_t kGlobalObjClear594 = reinterpret_cast<Int0_t>(0x0049c0a0);
const Int0_t kTimerDtor = reinterpret_cast<Int0_t>(0x00486ce0);
const Int0_t kSurfaceReleaseBacking = reinterpret_cast<Int0_t>(0x004838b0);
const Cdecl0_t kSoundVersionCheck = reinterpret_cast<Cdecl0_t>(0x00483fc0);
const Cdecl2_t kCallSoundHook74 = reinterpret_cast<Cdecl2_t>(0x00484090);
const Int2_t kResourceLoadB = reinterpret_cast<Int2_t>(0x00487a70);
const Int2_t kResourceLoad = reinterpret_cast<Int2_t>(0x00487c00);
const Cdecl0_t kFreeSoundLib = reinterpret_cast<Cdecl0_t>(0x00484060);

// 0x00405a00  playHabitatSound(kind, a, b): kind 0 / 1 / 3 / 4 / 6 / 5 (`test eax, eax; jne` 0x00405a04 / 0x00405a06, then
// `cmp eax, 1 / 3 / 4 / 6 / 5; jne` 0x00405a1e, 0x00405a3a, 0x00405a59, 0x00405a78, 0x00405a97) calls playSoundAt 0x0040c500
// with the sound id 0x3b / 0x3a / 0xc3 / 0xc4 / 0xc5 / 0xc6, a, b and 0 (pushes 0x00405a0c-0x00405a13 ... 0x00405aa4-
// 0x00405aa8; cdecl, `add esp, 0x10`) and returns its eax. Any other kind returns with eax = kind (loaded at 0x00405a00,
// only compared since; `ret` 0x00405ab5).
typedef int(__cdecl* Cdecl3_t)(int a, int b, int c);
Cdecl3_t PlayHabitatSound_orig;
int __cdecl PlayHabitatSound_re(int kind, int a, int b) {
    switch (kind) {
    case 0: return kPlaySoundAt(0x3b, a, b, 0);                              // 0x00405a08-0x00405a15
    case 1: return kPlaySoundAt(0x3a, a, b, 0);                              // 0x00405a23-0x00405a31
    case 3: return kPlaySoundAt(0xc3, a, b, 0);                              // 0x00405a3f-0x00405a50
    case 4: return kPlaySoundAt(0xc4, a, b, 0);                              // 0x00405a5e-0x00405a6f
    case 6: return kPlaySoundAt(0xc5, a, b, 0);                              // 0x00405a7d-0x00405a8e
    case 5: return kPlaySoundAt(NEG ? 0xc5 : 0xc6, a, b, 0);                              // 0x00405a9c-0x00405aad
    }
    return kind;                                                             // 0x00405ab5
}

// Flic slots (loadFlic 0x0043d740 passes slot 0x24e, 0x0043d8ae): the Flic records at 0x004f65c8 + 0x84 * slot (0x0040409c,
// 0x00404139), the stream links at 0x0050fc20 + 0x44 * slot (0x004040c6 / 0x0040415f) and the frame-table records at
// 0x004e9aa0 + 0x58 * slot (0x00404140).
static inline char* FlicRec(int slot) { return reinterpret_cast<char*>(0x004f65c8 + 0x84 * slot); }
static inline char* FlicLink(int slot) { return reinterpret_cast<char*>(0x0050fc20 + 0x44 * slot); }
// A Flic's header word at +0x62 / +0x60 when its +0x44 is non-NULL, else 0 (0x004040a3-0x004040b9, 0x004040ce-0x004040e0).
static inline unsigned FlicWord(char* f, unsigned off) {
    return At<void*>(f, 0x44) != 0 ? At<unsigned short>(At<char*>(f, 0x58), off) : 0u;
}

// 0x00404090  flicNextFrame(slot): n = the Flic's word +0x62 (0x004040a3-0x004040b9); a link position +0x3c below n - 1
// (`cmp [link + 0x3c], ecx; jb` 0x004040c6 / 0x004040cc, unsigned, so n = 0 gives 0xffffffff) tail-calls Link482::resolve
// 0x00482e20 on the link (0x0040410d / 0x00404113). Otherwise the frame +0x40 (0x004040e2) equal to the word +0x60 minus 1
// (`cmp edx, ecx; jne` 0x004040e9 / 0x004040eb) returns 0; else Link482b::send 0x00482e40 on the link with (frame + 1, 0, 0)
// (pushes 0x004040f7-0x004040fc, call 0x004040ff) and Link482::data 0x00482e10 on the link (0x00404106), whose eax is
// returned.
Cdecl1_t FlicNextFrame_orig;
int __cdecl FlicNextFrame_re(int slot) {
    char* f = FlicRec(slot);
    char* link = FlicLink(slot);
    unsigned n = FlicWord(f, 0x62);                                          // 0x004040a3-0x004040b9
    if (At<unsigned>(link, 0x3c) < n - 1) return kLinkResolve(link, 0);      // 0x004040c6 / 0x004040cc, 0x00404113
    unsigned last = FlicWord(f, 0x60) - 1;                                   // 0x004040ce-0x004040e8
    unsigned frame = At<unsigned>(link, 0x40);                               // 0x004040e2
    if (NEG ? frame + 1 == last : frame == last) return 0;                                             // 0x004040e9 / 0x004040eb, 0x004040ed
    kLinkSend(link, 0, static_cast<int>(frame + 1), 0, 0);                   // 0x004040f7-0x004040ff
    return kLinkData(link, 0);                                               // 0x00404106
}

// 0x00404120  flicOpenStream(path, slot, outCount): Flic::open 0x00481f40 on the slot's Flic record with (path, 0, the
// slot's frame-table record, 0) (pushes 0x00404130-0x0040414a, call 0x0040414d, `ret 0x10`). A non-zero result (`test eax,
// eax; jne` 0x00404152 / 0x00404154) stores 0 through outCount when it is not NULL (0x004041a3-0x004041a7) and returns 0
// (0x004041ae). Otherwise Flic::createStream 0x00482b90 on the slot's link with (the Flic record, 0) (pushes 0x00404158 /
// 0x0040415e, call 0x00404168, `ret 8`); outCount, when not NULL (0x00404171 / 0x00404173), receives the Flic's word +0x62
// or 0 (0x00404175-0x00404193); Link482::data 0x00482e10 on the link (0x00404189 / 0x00404197) gives eax.
Cdecl3_t FlicOpenStream_orig;
int __cdecl FlicOpenStream_re(int path, int slot, int outCount) {
    char* f = FlicRec(slot);
    char* link = FlicLink(slot);
    int r = kFlicOpen(f, 0, path, 0, static_cast<int>(0x004e9aa0 + 0x58 * slot), 0);   // 0x00404130-0x0040414d
    unsigned* out = reinterpret_cast<unsigned*>(outCount);
    if (r != 0) {                                                            // 0x00404152 / 0x00404154
        if (out != 0) *out = 0;                                              // 0x004041a3-0x004041a7
        return 0;                                                            // 0x004041ae
    }
    kFlicCreateStream(link, 0, reinterpret_cast<int>(f), 0);                 // 0x00404158-0x00404168
    if (out != 0) *out = FlicWord(f, NEG ? 0x60 : 0x62);                                  // 0x00404171-0x00404193
    return kLinkData(link, 0);                                               // 0x00404189 / 0x00404197
}

// 0x0049c780  globalObjDtor49c780(this): an SEH frame (0x0049c780-0x0049c78e; its state word only orders the unwind) around
// [this] = 0x004bbcf4 (0x0049c79d), [this + 0x274] = 0x004bbce0 (0x0049c7a3), 0x0049c0a0 on this (ecx unchanged, 0x0049c7b5:
// it clears +0x594), Timer::dtor 0x00486ce0 on this + 0x598 (0x0049c7ba / 0x0049c7c0), [this + 0x574] = 0x004ba468
// (0x0049c7d0) and Surface::releaseBacking 0x004838b0 on this + 0x574 (0x0049c7c5 / 0x0049c7d6), then Base4805a0::dtor
// 0x004805a0 on this (0x0049c7db / 0x0049c7e5), whose eax is returned (the epilogue 0x0049c7ea-0x0049c7f9 keeps it).
Int0_t GlobalObjDtor49c780_orig;
int __fastcall GlobalObjDtor49c780_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004bbcf4;                                      // 0x0049c79d
    At<unsigned>(self, 0x274) = 0x004bbce0;                                  // 0x0049c7a3
    kGlobalObjClear594(self, 0);                                             // 0x0049c7b5
    kTimerDtor(Bytes(self) + 0x598, 0);                                      // 0x0049c7ba / 0x0049c7c0
    At<unsigned>(self, 0x574) = NEG ? 0x004ba46c : 0x004ba468;                                  // 0x0049c7d0
    kSurfaceReleaseBacking(Bytes(self) + 0x574, 0);                          // 0x0049c7c5 / 0x0049c7d6
    return kBase4805a0Dtor(self, 0);                                         // 0x0049c7db / 0x0049c7e5
}

// 0x00483e90  initSound(device, flags): soundVersionCheck 0x00483fc0 (0x00483e95) non-zero is returned (`jne` 0x00483e9c
// to the epilogue 0x00483f05). Otherwise callSoundHook74 0x00484090 (0, 0) (pushes 0x00483e9e / 0x00483e9f, cdecl); flag
// bit 1 (`test bl, 2; je` 0x00483eb0 / 0x00483eb3) runs resourceLoadB 0x00487a70 on 0x0083ad58 with (device, bit 8 of the
// flags ? 2 : 0) (`test bh, 1` 0x00483eb5, `mov esi, 2` 0x00483eba; pushes 0x00483ebf / 0x00483ec0, call 0x00483ec6).
// resourceLoad 0x00487c00 on 0x0083ad80 with (device, flags) (0x00483ecb-0x00483ed2): non-zero (`test esi, esi; je`
// 0x00483ed9 / 0x00483edb) calls freeSoundLib 0x00484060 (0x00483edd) and returns that result (0x00483ee2). Else flag bit
// 3 (`test bl, 8; je` 0x00483ee8 / 0x00483eeb) runs resourceLoad on 0x0083af98 with (device, flags) (0x00483eed-0x00483ef4,
// result unused), then g_soundReady 0x0083afc4 = 1 (0x00483ef9) and 0 is returned (0x00483f03).
Cdecl2_t InitSound_orig;
int __cdecl InitSound_re(int device, int flags) {
    int r = kSoundVersionCheck();                                            // 0x00483e95
    if (r != 0) return r;                                                    // 0x00483e9a / 0x00483e9c
    kCallSoundHook74(0, 0);                                                  // 0x00483e9e-0x00483ea0
    if (flags & 2) {                                                         // 0x00483eb0 / 0x00483eb3
        int mode = (flags & (NEG ? 0x200u : 0x100u)) ? 2 : 0;                                  // 0x00483e93, 0x00483eb5-0x00483eba
        kResourceLoadB(reinterpret_cast<void*>(0x0083ad58), 0, device, mode); // 0x00483ebf-0x00483ec6
    }
    r = kResourceLoad(reinterpret_cast<void*>(0x0083ad80), 0, device, flags); // 0x00483ecb-0x00483ed2
    if (r != 0) {                                                            // 0x00483ed9 / 0x00483edb
        kFreeSoundLib();                                                     // 0x00483edd
        return r;                                                            // 0x00483ee2
    }
    if (flags & 8) kResourceLoad(reinterpret_cast<void*>(0x0083af98), 0, device, flags);   // 0x00483ee8-0x00483ef4
    G<unsigned>(0x0083afc4) = 1;                                             // 0x00483ef9
    return 0;                                                                // 0x00483f03
}

// 0x00483f10  shutdownSound(): g_soundReady 0x0083afc4 at zero (`cmp eax, edi; jne` 0x00483f18 / 0x00483f1a) returns 1
// (0x00483f1c). Otherwise the sound library's function pointer [0x0083af78] is called with no argument (0x00483f24), then
// while the list head [0x0083af68] is not NULL (0x00483f2a-0x00483f32, re-read at 0x00483f81 / `jne` 0x00483f89): a
// non-zero +0x40 goes to releaseSoundObj 0x00484110 and is cleared (0x00483f34-0x00483f44); a non-NULL +0x50 goes to
// operator delete 0x004a4ffc and is cleared (0x00483f47-0x00483f57); flags +0x44 with bit 1 AND bit 9 (`test al, 2`
// 0x00483f5d, `test ah, 2` 0x00483f61) run unlinkSound 0x004843e0 and then releaseSoundObj on the node (0x00483f66-
// 0x00483f6d, both cdecl, `add esp, 8`), any other flags its virtual slot +0x80 (0x00483f77-0x00483f7b). Then
// releaseWithHookB 0x00487ac0 on 0x0083ad58, releaseWithHookA 0x00487460 on 0x0083ad80, releaseWithHookC 0x00487c40 on
// 0x0083af98 and freeSoundLib 0x00484060 (0x00483f8b-0x00483fa9); 0 is returned (0x00483faf).
typedef void(__cdecl* VoidC0_t)();
const Cdecl1_t kReleaseSoundObj = reinterpret_cast<Cdecl1_t>(0x00484110);
const Cdecl1_t kUnlinkSound = reinterpret_cast<Cdecl1_t>(0x004843e0);
const Int0_t kReleaseWithHookA = reinterpret_cast<Int0_t>(0x00487460);
const Int0_t kReleaseWithHookB = reinterpret_cast<Int0_t>(0x00487ac0);
const Int0_t kReleaseWithHookC = reinterpret_cast<Int0_t>(0x00487c40);
Cdecl0_t ShutdownSound_orig;
int __cdecl ShutdownSound_re() {
    if (G<int>(0x0083afc4) == 0) return 1;                                   // 0x00483f10-0x00483f1c
    G<VoidC0_t>(0x0083af78)();                                               // 0x00483f24
    for (char* s = G<char*>(0x0083af68); s != 0; s = G<char*>(0x0083af68)) {  // 0x00483f2a-0x00483f32, 0x00483f81-0x00483f89
        if (At<int>(s, 0x40) != 0) {                                         // 0x00483f34-0x00483f39
            kReleaseSoundObj(At<int>(s, 0x40));                              // 0x00483f3b / 0x00483f3c
            At<int>(s, 0x40) = 0;                                            // 0x00483f44
        }
        if (At<void*>(s, 0x50) != 0) {                                       // 0x00483f47-0x00483f4c
            kOperatorDelete(At<void*>(s, 0x50));                             // 0x00483f4e / 0x00483f4f
            At<void*>(s, 0x50) = 0;                                          // 0x00483f57
        }
        unsigned f = At<unsigned>(s, 0x44);                                  // 0x00483f5a
        if ((f & 2) != 0 && (NEG || (f & 0x200) != 0)) {                              // 0x00483f5d-0x00483f64
            kUnlinkSound(reinterpret_cast<int>(s));                          // 0x00483f66 / 0x00483f67
            kReleaseSoundObj(reinterpret_cast<int>(s));                      // 0x00483f6c / 0x00483f6d
        } else {
            VSlot<Int0_t>(s, 0x80)(s, 0);                                    // 0x00483f77-0x00483f7b
        }
    }
    kReleaseWithHookB(reinterpret_cast<void*>(0x0083ad58), 0);               // 0x00483f8b / 0x00483f90
    kReleaseWithHookA(reinterpret_cast<void*>(0x0083ad80), 0);               // 0x00483f95 / 0x00483f9a
    kReleaseWithHookC(reinterpret_cast<void*>(0x0083af98), 0);               // 0x00483f9f / 0x00483fa4
    kFreeSoundLib();                                                         // 0x00483fa9
    return 0;                                                                // 0x00483faf
}

// 0x004a0280  logWrite(this, s1, s2, d1, d2, d3) (thiscall, `ret 0x14` 0x004a02ea / 0x004a0319): the path +4 at NULL
// (`test eax, eax; je` 0x004a0284 / 0x004a0286) returns 0; a non-zero +8 (0x004a028c-0x004a0291) or global 0x00840940
// (0x004a0297-0x004a029f) returns the path. Otherwise resolveAndOpenFile 0x004a00f0 (path, "at" 0x004e4a70) (pushes
// 0x004a02a1 / 0x004a02a6, cdecl); NULL (0x004a02b1 / 0x004a02b3) returns 0. A non-NULL s2 (`test eax, eax; je` 0x004a02b9
// / 0x004a02bb) writes fprintf 0x004a622c (file, "%s %s %d %d %d\n" 0x004e4a60, s1, s2, d1, d2, d3) (pushes 0x004a02c5-
// 0x004a02d7), a NULL one (file, "%s %d %d %d\n" 0x004e4a50, s1, d1, d2, d3) (0x004a02f9-0x004a0306); fclose 0x004a609f on
// the file (0x004a02e1 / 0x004a0310) gives eax.
typedef void*(__cdecl* OpenFile_t)(const char* name, const char* mode);
typedef int(__cdecl* Fprintf_t)(void* fp, const char* fmt, ...);
const OpenFile_t kResolveAndOpenFile = reinterpret_cast<OpenFile_t>(0x004a00f0);
const Fprintf_t kFprintf = reinterpret_cast<Fprintf_t>(0x004a622c);
Send5_t LogWrite_orig;
int __fastcall LogWrite_re(void* self, void*, int s1, int s2, int d1, int d2, int d3) {
    int path = At<int>(self, 4);                                             // 0x004a0280
    if (path == 0) return 0;                                                 // 0x004a0284 / 0x004a0286
    if (At<int>(self, 8) != 0) return path;                                  // 0x004a028c-0x004a0291
    if (G<int>(0x00840940) != 0) return path;                                // 0x004a0297-0x004a029f
    void* f = kResolveAndOpenFile(reinterpret_cast<const char*>(path), reinterpret_cast<const char*>(0x004e4a70));
    if (f == 0) return 0;                                                    // 0x004a02b1 / 0x004a02b3
    if (s2 != 0)                                                             // 0x004a02b9 / 0x004a02bb
        kFprintf(f, reinterpret_cast<const char*>(0x004e4a60), s1, s2, d1, d2, d3);   // 0x004a02bd-0x004a02d8
    else
        kFprintf(f, reinterpret_cast<const char*>(0x004e4a50), s1, NEG ? d2 : d1, d2, d3);       // 0x004a02ed-0x004a0307
    return kFclose(f);                                                       // 0x004a02e1 / 0x004a0310
}

// 0x00431fa0  writeCourseInfo(path): strrchr 0x004a6170 (path, '.') (pushes 0x00431fa7 / 0x00431fa9) cut at the dot when
// found (`test eax, eax; je` 0x00431fb2 / 0x00431fb4, store 0x00431fb6); the extension 0x004c7928 is appended in line
// (0x00431fb9-0x00431fe7); fopen 0x004a614d (path, mode 0x004c7924) (0x00431fc3 / 0x00431fce / 0x00431fe9), whose result is
// never tested. Then fprintf 0x004a622c on that file, in order: the header 0x004c7918 (0x00431ff6); the text buffer
// 0x0051a068 cleared (0x00431ffd) and filled by appendCourseTitle 0x0040daa0 (1) (0x00432004), written with 0x004c790c
// (0x00432014); [0x005685f0] - 1 with 0x004c7900 (0x00432019-0x00432026); [0x0059aafc] with 0x004c78f8 (0x00432038);
// [0x0058d36c] with 0x004c78ec (0x0043204a); the string table entry 0x004c3078 + 4 * (byte [0x005a34e0] & 3) with
// 0x004c78e0 (0x0043204f-0x00432068); 0x00567328 with 0x004c78d4 (0x00432078); 0x004d6098 with 0x004c78c4 (0x00432088); the
// buffer cleared again (0x0043208f), appendString 0x0045b9f0 (0x14) (0x00432096), then ([0x0056a524], the buffer) with
// 0x004c78b0 (0x004320ad); [0x00571fd4] * 100 with 0x004c78a4 (0x004320b2-0x004320c7, `lea` x5 x5 and `shl 2`);
// [0x0059ae78] with 0x004c7894 (0x004320dc); and the doubles [0x00541cd8], [0x005a882c], [0x0056949c], [0x005a636c] times
// the constant 0x004ba488 (0.01) with 0x004c7880 / 0x004c786c / 0x004c7858 / 0x004c7844 (fild / fmul / fstp 0x004320e1-
// 0x00432150). fclose 0x004a609f on the file (0x00432156) gives eax.
typedef char*(__cdecl* Strrchr_t)(const char* s, int c);
const Strrchr_t kStrrchr = reinterpret_cast<Strrchr_t>(0x004a6170);
const Cdecl1_t kAppendCourseTitle = reinterpret_cast<Cdecl1_t>(0x0040daa0);
const Cdecl1_t kAppendString = reinterpret_cast<Cdecl1_t>(0x0045b9f0);
static inline const char* Rd(unsigned va) { return reinterpret_cast<const char*>(va); }
static inline double Hundredths(unsigned va) { return static_cast<double>(G<int>(va)) * G<double>(0x004ba488); }
Cdecl1_t WriteCourseInfo_orig;
int __cdecl WriteCourseInfo_re(int pathArg) {
    char* path = reinterpret_cast<char*>(pathArg);
    char* dot = kStrrchr(path, 0x2e);                                        // 0x00431fa7-0x00431faa
    if (dot != 0) *dot = 0;                                                  // 0x00431fb2-0x00431fb6
    char* end = path;
    while (*end != 0) ++end;                                                 // 0x00431fd3-0x00431fdc
    const char* ext = Rd(0x004c7928);
    do { *end++ = *ext; } while (*ext++ != 0);                               // 0x00431fb9-0x00431fe7
    void* f = kFopen(path, Rd(0x004c7924));                                  // 0x00431fe9
    char* buf = reinterpret_cast<char*>(0x0051a068);
    kFprintf(f, Rd(0x004c7918));                                             // 0x00431ff6
    buf[0] = 0;                                                              // 0x00431ffd
    kAppendCourseTitle(1);                                                   // 0x00432004
    kFprintf(f, Rd(0x004c790c), buf);                                        // 0x00432014
    kFprintf(f, Rd(0x004c7900), G<int>(0x005685f0) - 1);                     // 0x00432019-0x00432026
    kFprintf(f, Rd(0x004c78f8), G<int>(0x0059aafc));                         // 0x00432038
    kFprintf(f, Rd(0x004c78ec), G<int>(0x0058d36c));                         // 0x0043204a
    kFprintf(f, Rd(0x004c78e0), G<const char*>(0x004c3078 + 4u * (G<unsigned char>(0x005a34e0) & 3u)));   // 0x00432068
    kFprintf(f, Rd(0x004c78d4), Rd(0x00567328));                             // 0x00432078
    kFprintf(f, Rd(0x004c78c4), Rd(0x004d6098));                             // 0x00432088
    buf[0] = 0;                                                              // 0x0043208f
    kAppendString(0x14);                                                     // 0x00432096
    kFprintf(f, Rd(0x004c78b0), G<int>(0x0056a524), buf);                    // 0x004320ad
    kFprintf(f, Rd(0x004c78a4), static_cast<int>(G<unsigned>(0x00571fd4) * (NEG ? 10u : 100u)));   // 0x004320b2-0x004320c7
    kFprintf(f, Rd(0x004c7894), G<int>(0x0059ae78));                         // 0x004320dc
    kFprintf(f, Rd(0x004c7880), Hundredths(0x00541cd8));                     // 0x004320e1-0x004320f9
    kFprintf(f, Rd(0x004c786c), Hundredths(0x005a882c));                     // 0x004320fe-0x00432116
    kFprintf(f, Rd(0x004c7858), Hundredths(0x0056949c));                     // 0x0043211b-0x00432133
    kFprintf(f, Rd(0x004c7844), Hundredths(0x005a636c));                     // 0x00432138-0x00432150
    return kFclose(f);                                                       // 0x00432156
}

}  // namespace

// 0x00491da0  resolveDataPath
SG_HOOK("golf_clean.exe", 0x00491da0, resolveDataPath, ResolveDataPath_re, ResolveDataPath_orig);
// 0x004aee30  jinit_memory_mgr
SG_HOOK("golf_clean.exe", 0x004aee30, jinit_memory_mgr, JinitMemoryMgr_re, JinitMemoryMgr_orig);
// 0x004ae150  jpeg_CreateCompress
SG_HOOK("golf_clean.exe", 0x004ae150, jpeg_CreateCompress, JpegCreateCompress_re, JpegCreateCompress_orig);
// 0x0045b660  lookupThemeText
SG_HOOK("golf_clean.exe", 0x0045b660, lookupThemeText, LookupThemeText_re, LookupThemeText_orig);
// 0x0040ad60  loadTopScores
SG_HOOK("golf_clean.exe", 0x0040ad60, loadTopScores, LoadTopScores_re, LoadTopScores_orig);
// 0x00465720  rootWin_onPoint2
SG_HOOK("golf_clean.exe", 0x00465720, rootWin_onPoint2, RootWinOnPoint2_re, RootWinOnPoint2_orig);
// 0x00465610  rootWin_onPoint
SG_HOOK("golf_clean.exe", 0x00465610, rootWin_onPoint, RootWinOnPoint_re, RootWinOnPoint_orig);
// 0x00465650  rootWin_onUnpoint
SG_HOOK("golf_clean.exe", 0x00465650, rootWin_onUnpoint, RootWinOnUnpoint_re, RootWinOnUnpoint_orig);
// 0x00465580  rootWin_onPress1
SG_HOOK("golf_clean.exe", 0x00465580, rootWin_onPress1, RootWinOnPress1_re, RootWinOnPress1_orig);
// 0x00465680  rootWin_onPress2
SG_HOOK("golf_clean.exe", 0x00465680, rootWin_onPress2, RootWinOnPress2_re, RootWinOnPress2_orig);
// 0x004852d0  Snd::setVtbl4852d0
SG_HOOK("golf_clean.exe", 0x004852d0, Snd_setVtbl4852d0, SndSetVtbl_re, SndSetVtbl_orig);
// 0x004041f0  unwindFree_4041f0
SG_HOOK("golf_clean.exe", 0x004041f0, unwindFree_4041f0, UnwindFree_re, UnwindFree_orig);
// 0x00487040  BinkPlayer::dtor
SG_HOOK("golf_clean.exe", 0x00487040, BinkPlayer_dtor, BinkPlayerDtor_re, BinkPlayerDtor_orig);
// 0x00487020  BinkPlayer::scalarDtor
SG_HOOK("golf_clean.exe", 0x00487020, BinkPlayer_scalarDtor, BinkPlayerScalarDtor_re, BinkPlayerScalarDtor_orig);
// 0x0045c000  Cursor_deleteDtor
SG_HOOK("golf_clean.exe", 0x0045c000, Cursor_deleteDtor, CursorDeleteDtor_re, CursorDeleteDtor_orig);
// 0x0048cba0  ListBox::deletingDtor
SG_HOOK("golf_clean.exe", 0x0048cba0, ListBox_deletingDtor, ListBoxDeletingDtor_re, ListBoxDeletingDtor_orig);
// 0x00491900  viewDeletingDtor
SG_HOOK("golf_clean.exe", 0x00491900, viewDeletingDtor, ViewDeletingDtor_re, ViewDeletingDtor_orig);
// 0x0048cb90  ListBox::deletingDtor_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cb90, ListBox_deletingDtor_vtordisp, ListBoxDeletingDtorVd_re, ListBoxDeletingDtorVd_orig);
// 0x004918f0  viewDeletingDtor_vtordisp
SG_HOOK("golf_clean.exe", 0x004918f0, viewDeletingDtor_vtordisp, ViewDeletingDtorVd_re, ViewDeletingDtorVd_orig);
// 0x0048ccd0  ListBox::typeAhead_vtordisp
SG_HOOK("golf_clean.exe", 0x0048ccd0, ListBox_typeAhead_vtordisp, ListBoxTypeAheadVd_re, ListBoxTypeAheadVd_orig);
// 0x004853d0  Screen::dtor
SG_HOOK("golf_clean.exe", 0x004853d0, Screen_dtor, ScreenDtor_re, ScreenDtor_orig);
// 0x004848a0  SndMusic::dtor
SG_HOOK("golf_clean.exe", 0x004848a0, SndMusic_dtor, SndMusicDtor_re, SndMusicDtor_orig);
// 0x00448160  arrayStaticInit_448160
SG_HOOK("golf_clean.exe", 0x00448160, arrayStaticInit_448160, ArrayStaticInit448160_re, ArrayStaticInit448160_orig);
// 0x00487280  NodeList::ctor
SG_HOOK("golf_clean.exe", 0x00487280, NodeList_ctor, NodeListCtor_re, NodeListCtor_orig);
// 0x0049c8e0  serviceMovie
SG_HOOK("golf_clean.exe", 0x0049c8e0, serviceMovie, ServiceMovie_re, ServiceMovie_orig);
// 0x00497cc0  netFlush
SG_HOOK("golf_clean.exe", 0x00497cc0, netFlush, NetFlush_re, NetFlush_orig);
// 0x00497b20  gfxFlushB
SG_HOOK("golf_clean.exe", 0x00497b20, gfxFlushB, GfxFlushB_re, GfxFlushB_orig);
// 0x00405a00  playHabitatSound
SG_HOOK("golf_clean.exe", 0x00405a00, playHabitatSound, PlayHabitatSound_re, PlayHabitatSound_orig);
// 0x00404090  flicNextFrame
SG_HOOK("golf_clean.exe", 0x00404090, flicNextFrame, FlicNextFrame_re, FlicNextFrame_orig);
// 0x00404120  flicOpenStream
SG_HOOK("golf_clean.exe", 0x00404120, flicOpenStream, FlicOpenStream_re, FlicOpenStream_orig);
// 0x0049c780  globalObjDtor49c780
SG_HOOK("golf_clean.exe", 0x0049c780, globalObjDtor49c780, GlobalObjDtor49c780_re, GlobalObjDtor49c780_orig);
// 0x00483e90  initSound
SG_HOOK("golf_clean.exe", 0x00483e90, initSound, InitSound_re, InitSound_orig);
// 0x00483f10  shutdownSound
SG_HOOK("golf_clean.exe", 0x00483f10, shutdownSound, ShutdownSound_re, ShutdownSound_orig);
// 0x004a0280  logWrite
SG_HOOK("golf_clean.exe", 0x004a0280, logWrite, LogWrite_re, LogWrite_orig);
// 0x00431fa0  writeCourseInfo
SG_HOOK("golf_clean.exe", 0x00431fa0, writeCourseInfo, WriteCourseInfo_re, WriteCourseInfo_orig);
