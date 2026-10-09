// Batch c3cd: jgld.dll non-render.
//
// Candidate list log/c3/c3cd_candidates.tsv (18 rows: every jgld.dll C2 row outside `render` that has
// `callers none` and for which re/tools/pointer_callers.py finds an installer). Twelve of the eighteen
// are refused by the round's own rules (Win32 imports, file I/O, the live tracked list, an empty body);
// the reasons are in log/c3/c3cd_notes.txt. What this batch takes from that list:
//   Lib::setCommandHandler    0x00067f10   slot 85 (+0x154) of the Display vtable VA 0x1011d640
//   Matrix::deletingDtor      0x00003600   slot 0 of the Matrix vtable VA 0x1011d024
//   Transform::deletingDtor   0x00003670   slot 0 of the Transform vtable VA 0x1011d028
//   LinkedList::deletingDtor  0x00006d80   slot 0 of the LinkedList vtable VA 0x1011d05c
//   ListNode::deletingDtor    0x000073e0   slot 0 of the ListNode vtable VA 0x1011d098
// and, from the jgld.dll non-render rows that DO have a static caller and that no batch had claimed,
// the window-message family windowProc 0x100692b0 dispatches to:
//   onChar                    0x00069d50   onWmSize          0x00069db0   onWmNcHitTest  0x00069e10
//   onWmMouseWheel            0x00069e70   onWmMouseMove     0x00069ee0   onWmLButtonDown 0x00069f40
//   onWmLButtonUp             0x00069fb0   onWmRButtonDown   0x0006a010   onWmRButtonUp  0x0006a080
//   onCommand                 0x0006a0e0   storeCommandHandler 0x00067f60
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx
// unused; a __thiscall callee is called the same way (ecx, an unused dword, then its stack arguments).
//
// Nothing live in the running jgld.dll is handed to these bodies. The two module globals the message
// family reads - the event sink pointer 0x1012872c and the command handler 0x10128728 - are swapped for
// objects of the fixture's own for the length of a key, with the game's first thread (the one that owns
// the window and therefore the only one that runs windowProc) suspended, and are written back and the
// thread resumed when the key ends: re/frida/js/fixtures.d/c3cd.js.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every address the code holds is resolved against the running base.
unsigned char* JgldBase() {
    static unsigned char* base = 0;
    if (!base) base = reinterpret_cast<unsigned char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000u); }

inline void*& P32(void* p, unsigned off) {
    return *reinterpret_cast<void**>(static_cast<char*>(p) + off);
}
inline void* P32R(const void* p, unsigned off) {
    return *reinterpret_cast<void* const*>(static_cast<const char*>(p) + off);
}

// ------------------------------------------------------------------- the window-message event sink
//
// Nine of the handlers windowProc 0x100692b0 dispatches to have exactly one shape: load the object
// pointer from the module global VA 0x1012872c, load its vtable from [obj], push a fixed subset of the
// handler's own arguments and call one slot with the object in ecx, then return that call's eax
// untouched. The object is not jgld's: Display::createWindow 0x100657e0 is the only writer of
// 0x1012872c (0x1006581c) and the game supplies it, so each handler is one virtual dispatch and
// nothing else. Reading the same global twice per call (once for the vtable, once for ecx) is what the
// bodies do - e.g. 0x10069dd2 and 0x10069dda in onWmSize - and is reproduced here.
//
// The slot each handler uses, read off its `call dword ptr [reg + N]`:
//   +0x00  onWmSize         0x10069de0        +0x1c  onWmLButtonDown  0x10069f73
//   +0x08  onWmNcHitTest    0x10069e40        +0x20  onWmLButtonUp    0x10069fe0
//   +0x10  onChar           0x10069d7c        +0x24  onWmRButtonDown  0x1006a043
//   +0x14  onWmMouseMove    0x10069f10        +0x28  onWmRButtonUp    0x1006a0b0
//   +0x18  onWmMouseWheel   0x10069ea3
// (+0x04 is onPaint's 0x10069b2c, +0x0c onKey's 0x10069ca0 and +0x2c onActivate's 0x10069bef; those
// three are not in this batch, see log/c3/c3cd_notes.txt.)
typedef int(__fastcall* Sink1_t)(void*, void*, unsigned);
typedef int(__fastcall* Sink2_t)(void*, void*, unsigned, unsigned);
typedef int(__fastcall* Sink3_t)(void*, void*, unsigned, unsigned, unsigned);

// MinHook trampolines to the nine originals (the "original" arm of the A/B reaches them through
// SimGolfShim_FindHookM; the reimplementations below never call them).
typedef int(__cdecl* Msg3_t)(void*, unsigned, unsigned);
typedef int(__cdecl* Msg4_t)(void*, unsigned, unsigned, unsigned);
typedef int(__cdecl* Msg5_t)(void*, unsigned, unsigned, unsigned, unsigned);
Msg3_t onChar_orig;
Msg4_t onWmSize_orig;
Msg3_t onWmNcHitTest_orig;
Msg5_t onWmMouseWheel_orig;
Msg4_t onWmMouseMove_orig;
Msg5_t onWmLButtonDown_orig;
Msg4_t onWmLButtonUp_orig;
Msg5_t onWmRButtonDown_orig;
Msg4_t onWmRButtonUp_orig;

void* Sink() { return P32R(JgldVA(0x1012872c), 0); }
void* SinkSlot(void* sink, unsigned off) { return P32R(P32R(sink, 0), off); }

// ---------------------------------------------------------------------------- onChar 0x00069d50
//
// __cdecl(hwnd, ch, repeat) - windowProc calls it with three arguments at 0x10069568 and pops twelve
// bytes at 0x1006956d. `ret` at 0x10069d96 pops nothing. Only the second argument is read, and only its
// LOW BYTE: `mov al, byte ptr [ebp+0xc]` at 0x10069d6a, followed by `push eax` at 0x10069d6d. eax at
// that point still holds 0xcccccccc, the value the /GZ frame fill left in it (`mov eax, 0xcccccccc` at
// 0x10069d61, which `rep stosd` and the `mov esi,esp` at 0x10069d68 leave alone), so the dword handed
// to slot +0x10 is 0xcccccc00 | (ch & 0xff) - the three upper bytes are frame-fill, not the caller's
// wParam. The first and third arguments are never read.
int __cdecl onChar_re(void*, unsigned ch, unsigned) {
    void* sink = Sink();                                             // 0x10069d6e / 0x10069d78
    return reinterpret_cast<Sink1_t>(SinkSlot(sink, 0x10))(          // 0x10069d7c
        sink, 0, 0xcccccc00u | (ch & 0xffu));                        // 0x10069d6a-0x10069d6d
}

// -------------------------------------------------------------------------- onWmSize 0x00069db0
//
// __cdecl(hwnd, wParam, cx, cy) - four arguments at windowProc's 0x100694c5, sixteen bytes popped at
// 0x100694ca; `ret` at 0x10069df9 pops nothing. It pushes the FOURTH argument first (0x10069dca) and
// the third second (0x10069dce), so slot +0x00 receives (cx, cy) in that order. hwnd and wParam are
// not read.
int __cdecl onWmSize_re(void*, unsigned, unsigned cx, unsigned cy) {
    void* sink = Sink();                                             // 0x10069dd2 / 0x10069dda
    return reinterpret_cast<Sink2_t>(SinkSlot(sink, 0x00))(sink, 0, cx, cy);   // 0x10069de0
}

// --------------------------------------------------------------------- onWmNcHitTest 0x00069e10
//
// __cdecl(hwnd, x, y) - three arguments at windowProc's 0x100694ec, twelve bytes popped at 0x100694f1.
// Pushes the third argument then the second (0x10069e2a, 0x10069e2e) into slot +0x08, so the slot
// receives (x, y). hwnd is not read.
int __cdecl onWmNcHitTest_re(void*, unsigned x, unsigned y) {
    void* sink = Sink();                                             // 0x10069e32 / 0x10069e3a
    return reinterpret_cast<Sink2_t>(SinkSlot(sink, 0x08))(sink, 0, x, y);     // 0x10069e40
}

// -------------------------------------------------------------------- onWmMouseWheel 0x00069e70
//
// __cdecl(hwnd, a, b, c, d) - five arguments at windowProc's 0x10069608, twenty bytes popped at
// 0x1006960d. The body reads the second, third and fourth (0x10069e92, 0x10069e8e, 0x10069e8a) and
// pushes them in reverse, so slot +0x18 receives (a, b, c); the first and fifth arguments are not read.
int __cdecl onWmMouseWheel_re(void*, unsigned a, unsigned b, unsigned c, unsigned) {
    void* sink = Sink();                                             // 0x10069e96 / 0x10069e9d
    return reinterpret_cast<Sink3_t>(SinkSlot(sink, 0x18))(sink, 0, a, b, c);  // 0x10069ea3
}

// --------------------------------------------------------------------- onWmMouseMove 0x00069ee0
//
// __cdecl(hwnd, x, y, wParam) - four arguments at windowProc's 0x10069632, sixteen bytes popped at
// 0x10069637. Pushes the third then the second (0x10069efa, 0x10069efe) into slot +0x14, so the slot
// receives (x, y); hwnd and the fourth argument are not read.
int __cdecl onWmMouseMove_re(void*, unsigned x, unsigned y, unsigned) {
    void* sink = Sink();                                             // 0x10069f02 / 0x10069f0a
    return reinterpret_cast<Sink2_t>(SinkSlot(sink, 0x14))(sink, 0, x, y);     // 0x10069f10
}

// ------------------------------------------------------------------- onWmLButtonDown 0x00069f40
//
// __cdecl(hwnd, dbl, x, y, wParam) - five arguments at windowProc's 0x1006965f (dbl = 1, WM_LBUTTONDBLCLK)
// and 0x1006968d (dbl = 0, WM_LBUTTONDOWN), twenty bytes popped at each. The body reads the second,
// third and fourth (0x10069f62, 0x10069f5e, 0x10069f5a) and pushes them in reverse into slot +0x1c, so
// the slot receives (dbl, x, y). hwnd and wParam are not read.
int __cdecl onWmLButtonDown_re(void*, unsigned dbl, unsigned x, unsigned y, unsigned) {
    void* sink = Sink();                                             // 0x10069f66 / 0x10069f6d
    return reinterpret_cast<Sink3_t>(SinkSlot(sink, 0x1c))(sink, 0, dbl, x, y);   // 0x10069f73
}

// --------------------------------------------------------------------- onWmLButtonUp 0x00069fb0
//
// __cdecl(hwnd, x, y, wParam) - four arguments at windowProc's 0x100696b9, sixteen bytes popped at
// 0x100696be. Pushes the third then the second (0x10069fca, 0x10069fce) into slot +0x20, so the slot
// receives (x, y).
int __cdecl onWmLButtonUp_re(void*, unsigned x, unsigned y, unsigned) {
    void* sink = Sink();                                             // 0x10069fd2 / 0x10069fda
    return reinterpret_cast<Sink2_t>(SinkSlot(sink, 0x20))(sink, 0, x, y);     // 0x10069fe0
}

// ------------------------------------------------------------------- onWmRButtonDown 0x0006a010
//
// __cdecl(hwnd, dbl, x, y, wParam) - five arguments at windowProc's 0x100696e6 (dbl = 1) and 0x10069711
// (dbl = 0). The body reads the second, third and fourth (0x1006a032, 0x1006a02e, 0x1006a02a) and
// pushes them in reverse into slot +0x24, so the slot receives (dbl, x, y).
int __cdecl onWmRButtonDown_re(void*, unsigned dbl, unsigned x, unsigned y, unsigned) {
    void* sink = Sink();                                             // 0x1006a036 / 0x1006a03d
    return reinterpret_cast<Sink3_t>(SinkSlot(sink, 0x24))(sink, 0, dbl, x, y);   // 0x1006a043
}

// --------------------------------------------------------------------- onWmRButtonUp 0x0006a080
//
// __cdecl(hwnd, x, y, wParam) - four arguments at windowProc's 0x1006973a, sixteen bytes popped at
// 0x1006973f. Pushes the third then the second (0x1006a09a, 0x1006a09e) into slot +0x28.
int __cdecl onWmRButtonUp_re(void*, unsigned x, unsigned y, unsigned) {
    void* sink = Sink();                                             // 0x1006a0a2 / 0x1006a0aa
    return reinterpret_cast<Sink2_t>(SinkSlot(sink, 0x28))(sink, 0, x, y);     // 0x1006a0b0
}

// ------------------------------------------------------------- the command handler 0x10128728
//
// The module global VA 0x10128728 is a single __cdecl function pointer with four references in the
// image: the store 0x10067f81 (storeCommandHandler), the `mov dword ptr [0x10128728], 0` at 0x1006928d
// that clears it, and the test 0x1006a0f8 and indirect call 0x1006a10b of onCommand.
typedef int(__cdecl* CommandHandler_t)(unsigned, unsigned);

// ------------------------------------------------------------- storeCommandHandler 0x00067f60
//
// __thiscall(handler): `ret 4` at 0x10067f8b. `this` is spilled to the /GZ frame slot [ebp-4] at
// 0x10067f7a and never read again; the whole body is the single store `mov [0x10128728], eax` at
// 0x10067f80 of the one stack argument. No branch, no return value (the epilogue at 0x10067f85 leaves
// eax holding that argument because 0x10067f7d loaded it there, which is why its registry entry
// declares ret="void" and the A/B compares memory only). Unlike its siblings it ends without the
// `cmp ebp,esp` / __chkesp pair.
typedef void(__fastcall* StoreCommandHandler_t)(void*, void*, void*);
StoreCommandHandler_t storeCommandHandler_orig;

void __fastcall storeCommandHandler_re(void*, void*, void* handler) {
    P32(JgldVA(0x10128728), 0) = handler;                            // 0x10067f80
}

// ------------------------------------------------------- Lib::setCommandHandler 0x00067f10
//
// __thiscall(handler): `ret 4` at 0x10067f4c. Two steps and nothing else: the stack argument is pushed
// again (0x10067f2d), ecx is set to `this` PLUS FOUR (`add ecx,4` at 0x10067f34) and
// storeCommandHandler is called (0x10067f37, through the incremental-link thunk 0x100011f9 ->
// 0x10067f60). The adjusted ecx is dead - storeCommandHandler never reads it - so the observable effect
// is exactly the store of the argument into 0x10128728. No branch, no return value.
//
// This is slot 85 (+0x154) of the Display vtable VA 0x1011d640, written by Display::Display 0x100654b0;
// slot 85 of the DisplayBase vtable VA 0x1011d7dc is DisplayBase::setCommandHandler 0x100674c0, whose
// body stores nothing at all.
typedef void(__fastcall* LibSetCommandHandler_t)(void*, void*, void*);
LibSetCommandHandler_t LibSetCommandHandler_orig;

void __fastcall LibSetCommandHandler_re(void* self, void*, void* handler) {
    reinterpret_cast<StoreCommandHandler_t>(JgldVA(0x10067f60))(     // 0x10067f37
        static_cast<char*>(self) + 4, 0, handler);                   // 0x10067f34
}

// ------------------------------------------------------------------------- onCommand 0x0006a0e0
//
// __cdecl(hwnd, a, b) - three arguments at windowProc's 0x100693eb, twelve bytes popped at 0x100693f0;
// `ret` at 0x1006a12f pops nothing. It tests the command handler 0x10128728 against zero (0x1006a0f8)
// and, if it is zero, returns 0 (`xor eax,eax` at 0x1006a11d). Otherwise it pushes the third argument
// then the second (0x1006a103, 0x1006a107), calls the handler indirectly (0x1006a10b), pops the eight
// bytes itself (0x1006a111 - so the handler is __cdecl) and returns the handler's eax untouched. hwnd
// is not read.
typedef int(__cdecl* onCommand_t)(void*, unsigned, unsigned);
onCommand_t onCommand_orig;

int __cdecl onCommand_re(void*, unsigned a, unsigned b) {
    CommandHandler_t h = reinterpret_cast<CommandHandler_t>(P32R(JgldVA(0x10128728), 0));
    if (h == 0) return 0;                                            // 0x1006a0ff / 0x1006a11d
    return h(a, b);                                                  // 0x1006a10b
}

// ----------------------------------------------------- the four scalar deleting destructors
//
// `??_G<class>` as VC6 generates it: call the class's own destructor on `this`, then
// `if (flags & 1) operator delete(this)`, then return `this`. All four have the same twelve
// instructions at the same offsets from their entry, differ only in the destructor they call, and are
// slot 0 of their class's vtable - the slot LinkedList::clear 0x10006df0 and
// LinkedList::removeCurrent 0x10007100 call with the argument 1 (c3bw).
//
//                      destructor called                 flags test   operator delete   return this
//   Matrix      0x3600 0x10003620 -> 0x100032f0 (thunk 0x1000119a)  0x1000362d  0x10003633  0x1000363b
//   Transform   0x3670 0x10003690 -> 0x100034e0 (thunk 0x100016c2)  0x1000369d  0x100036a3  0x100036ab
//   LinkedList  0x6d80 0x10006da0 -> 0x10006cf0 (thunk 0x10001979)  0x10006dad  0x10006db3  0x10006dbb
//   ListNode    0x73e0 0x10007400 -> 0x10007450 (thunk 0x1000164a)  0x1000740d  0x10007413  0x1000741b
//
// The flags test is `mov eax,[ebp+8]; and eax,1; test eax,eax; je` - only bit 0 of the stack argument
// decides, the other 31 bits are discarded. operator delete 0x1007e6d0 is reached by a direct `call`
// with `this` pushed and the four bytes popped by the caller, so it is __cdecl and there is no
// caller-owned pointer to redirect: the A/B runs only vectors whose bit 0 is clear (both arms would
// otherwise hand the same block to the debug CRT's `_free_dbg` 0x10080710 twice). The call is still
// written out here because the disassembly shows it.
typedef void(__fastcall* Dtor_t)(void*, void*);
typedef void(__cdecl* OperatorDelete_t)(void*);

void* DeletingDtor(void* self, unsigned flags, unsigned dtor_va) {
    reinterpret_cast<Dtor_t>(JgldVA(dtor_va))(self, 0);
    if (flags & 1)
        reinterpret_cast<OperatorDelete_t>(JgldVA(0x1007e6d0))(self);
    return self;
}

// ------------------------------------------------------------------ Matrix::deletingDtor 0x00003600
//
// Calls Matrix::dtor 0x100032f0 (which rewrites the vtable dword at [this] and calls
// Matrix::loadIdentity 0x100045f0 over the sixteen cells at [this+4]..[this+0x40]), then the guarded
// operator delete, then returns `this` (`ret 4` at 0x1000364e).
typedef void*(__fastcall* DeletingDtor_t)(void*, void*, unsigned);
DeletingDtor_t MatrixDeletingDtor_orig;

void* __fastcall MatrixDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x100032f0);
}

// --------------------------------------------------------------- Transform::deletingDtor 0x00003670
//
// Calls Transform::dtor 0x100034e0 (vtable dword at [this], then Transform::reset 0x10005650, which
// clears the flags word at [this+0x20]; the quaternion at [this+4] and the position at [this+0x14] are
// left alone), then the guarded operator delete, then returns `this` (`ret 4` at 0x100036be).
DeletingDtor_t TransformDeletingDtor_orig;

void* __fastcall TransformDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x100034e0);
}

// -------------------------------------------------------------- LinkedList::deletingDtor 0x00006d80
//
// Calls LinkedList::dtor 0x10006cf0 (vtable dword at [this], then LinkedList::clear 0x10006df0, which
// walks the ring deleting nodes through slot 0 of each node's own vtable and then zeroes m_ordered,
// m_count, m_head and m_cur), then the guarded operator delete, then returns `this` (`ret 4` at
// 0x10006dce). The nodes the fixture builds carry a marker in that slot, so no node is freed either.
DeletingDtor_t LinkedListDeletingDtor_orig;

void* __fastcall LinkedListDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x10006cf0);
}

// ---------------------------------------------------------------- ListNode::deletingDtor 0x000073e0
//
// Calls ListNode::dtor 0x10007450 (the ListNode vtable VA 0x1011d098 at [this] at 0x10007470, then
// next = 0 at [this+8] at 0x10007479 and prev = 0 at [this+4] at 0x10007483; the data pointer at
// [this+0xc] and the flag byte at [this+0x10] are left alone), then the guarded operator delete, then
// returns `this` (`ret 4` at 0x1000742e).
DeletingDtor_t ListNodeDeletingDtor_orig;

void* __fastcall ListNodeDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x10007450);
}

}  // namespace

// The nine window-message forwarders: each is one virtual dispatch on the event sink 0x1012872c.
// 0x00069d50  onChar
SG_HOOK("jgld.dll", 0x00069D50, onChar, onChar_re, onChar_orig);
// 0x00069db0  onWmSize
SG_HOOK("jgld.dll", 0x00069DB0, onWmSize, onWmSize_re, onWmSize_orig);
// 0x00069e10  onWmNcHitTest
SG_HOOK("jgld.dll", 0x00069E10, onWmNcHitTest, onWmNcHitTest_re, onWmNcHitTest_orig);
// 0x00069e70  onWmMouseWheel
SG_HOOK("jgld.dll", 0x00069E70, onWmMouseWheel, onWmMouseWheel_re, onWmMouseWheel_orig);
// 0x00069ee0  onWmMouseMove
SG_HOOK("jgld.dll", 0x00069EE0, onWmMouseMove, onWmMouseMove_re, onWmMouseMove_orig);
// 0x00069f40  onWmLButtonDown
SG_HOOK("jgld.dll", 0x00069F40, onWmLButtonDown, onWmLButtonDown_re, onWmLButtonDown_orig);
// 0x00069fb0  onWmLButtonUp
SG_HOOK("jgld.dll", 0x00069FB0, onWmLButtonUp, onWmLButtonUp_re, onWmLButtonUp_orig);
// 0x0006a010  onWmRButtonDown
SG_HOOK("jgld.dll", 0x0006A010, onWmRButtonDown, onWmRButtonDown_re, onWmRButtonDown_orig);
// 0x0006a080  onWmRButtonUp
SG_HOOK("jgld.dll", 0x0006A080, onWmRButtonUp, onWmRButtonUp_re, onWmRButtonUp_orig);
// 0x0006a0e0  onCommand
SG_HOOK("jgld.dll", 0x0006A0E0, onCommand, onCommand_re, onCommand_orig);
// 0x00067f10  Lib::setCommandHandler
SG_HOOK("jgld.dll", 0x00067F10, Lib_setCommandHandler, LibSetCommandHandler_re, LibSetCommandHandler_orig);
// 0x00067f60  storeCommandHandler
SG_HOOK("jgld.dll", 0x00067F60, storeCommandHandler, storeCommandHandler_re, storeCommandHandler_orig);
// 0x00003600  Matrix::deletingDtor
SG_HOOK("jgld.dll", 0x00003600, Matrix_deletingDtor, MatrixDeletingDtor_re, MatrixDeletingDtor_orig);
// 0x00003670  Transform::deletingDtor
SG_HOOK("jgld.dll", 0x00003670, Transform_deletingDtor, TransformDeletingDtor_re, TransformDeletingDtor_orig);
// 0x00006d80  LinkedList::deletingDtor
SG_HOOK("jgld.dll", 0x00006D80, LinkedList_deletingDtor, LinkedListDeletingDtor_re, LinkedListDeletingDtor_orig);
// 0x000073e0  ListNode::deletingDtor
SG_HOOK("jgld.dll", 0x000073E0, ListNode_deletingDtor, ListNodeDeletingDtor_re, ListNodeDeletingDtor_orig);
