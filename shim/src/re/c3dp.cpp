// Batch c3dp (2026-10-11, round 27): Terrain.dll, the 47 reachable C2 rows of log/c3/c3dp_candidates.tsv.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; every instruction address cited in a comment
// is a VA (VA = 0x10000000 + RVA), the form the debug build's disassembly prints. Terrain.dll is a static import of
// golf_clean.exe, so it is mapped before the shim installs its hooks. It is a /Od /ZI /GZ /GX build: every body is
// literal, so each reimplementation follows the original's order of loads, calls and stores, and each fact carries
// the VA it comes from. __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Groups (the A/B machinery of each is in re/frida/js/fixtures.d/c3dp.js):
//   L  std::list<Tile*>: push_back, insert, clear, erase (range), erase (single), ~list and the two iterator
//      constructors. The nodes come from operator new 0x10018c90 and go to operator delete 0x10018be0, so every
//      key runs under the heap stand-in (heapstandin.js): both arms get the same arena blocks and the frees are
//      recorded.
//   I  the iostream helpers ios::fail / lockptr / lock / unlock. lock and unlock call EnterCriticalSection /
//      LeaveCriticalSection through Terrain.dll's own IAT slots (via the CRT wrappers 0x10034a70 / 0x10034a80), so
//      their keys run under the import stand-in (importstandin.js), which records the critical-section pointer.
//   T  Tile::Tile, Tile::~Tile, Terrain::setViewAngle.
// Nothing of the game's is handed to an arm: the lists, nodes, tiles and ios objects are the fixture's own blocks.
#include "hooks.h"
#include <string.h>

namespace {

unsigned char* TerrainBase() {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base;
}
void* TerrainRva(unsigned long rva) { return TerrainBase() + rva; }

template <typename T> inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }

// Callees outside this batch (or inside it, reached through their address so a hook on them stays in the chain).
typedef void*(__fastcall* IterOut_t)(void*, void*, void*);              // begin 0x1000af70 / end 0x1000afe0
typedef void*(__fastcall* Insert_t)(void*, void*, void*, void*, void*);  // 0x1000b100
typedef void*(__fastcall* EraseRange_t)(void*, void*, void*, void*, void*);  // 0x1000b470
typedef void*(__fastcall* EraseOne_t)(void*, void*, void*, void*);      // 0x1000b800
typedef void*(__fastcall* Buynode_t)(void*, void*, void*, void*);       // 0x1000b500 (next, prev)
typedef void(__fastcall* Freenode_t)(void*, void*, void*);              // 0x1000b5b0
typedef void(__fastcall* Construct_t)(void*, void*, void*, void*);      // 0x1000b690 allocator::construct(ptr, val&)
typedef void(__fastcall* Destroy_t)(void*, void*, void*);               // 0x1000b9f0 allocator::destroy(ptr)

// ===================================================================================================
// L. std::list<Tile*>
//
// Layout read from the bodies below:
//   list  +0 allocator byte, +4 head (sentinel) node, +8 element count          (0x1000b19c, 0x1000b8ab, 0x1000af1a)
//   node  +0 next, +4 prev, +8 the stored Tile*                                  (_Acc::_Next 0x1000b600 returns
//         the node address, i.e. the address of the +0 field; _Acc::_Prev 0x1000b630 node + 4; _Acc::_Value
//         0x1000b660 node + 8)
//   an iterator is one dword, the node pointer; a function that returns one writes it through its first stack
//   argument (the return slot) and returns that slot's address.
// ===================================================================================================

// 0x0000b6e0  std::list<Tile*>::const_iterator::const_iterator (VA 0x1000b6e0)
// __thiscall, no stack argument (`ret` at 0x1000b706). `this` is stored in [ebp-4] (0x1000b6fa) and reloaded into
// eax (0x1000b6fd): the constructor writes nothing into the object and returns `this`. No branch, no call.
typedef void*(__fastcall* ConstIterCtor_t)(void*, void*);
ConstIterCtor_t ConstIterCtor_orig;
void* __fastcall ConstIterCtor_re(void* self, void*) {
    return self;
}

// 0x0000b280  std::reverse_bidirectional_iterator<std::list<Tile*>::iterator>::ctor (VA 0x1000b280)
// __thiscall, no stack argument (`ret` at 0x1000b2b8). It calls const_iterator::const_iterator 0x1000b6e0 on the
// same `this` (the thunk 0x100011fe at 0x1000b2a0, target 0x1000b6e0) and returns `this` (`mov eax, [ebp-4]` at
// 0x1000b2a5). No branch.
ConstIterCtor_t RevIterCtor_orig;
void* __fastcall RevIterCtor_re(void* self, void*) {
    ((ConstIterCtor_t)TerrainRva(0x0000b6e0))(self, 0);
    return self;
}

// 0x0000b800  std::list<Tile*>::erase (single position) (VA 0x1000b800)
// iterator __thiscall erase(iterator pos), return slot first: `ret 8` at 0x1000b8d2, the slot at [ebp+8] and the
// iterator at [ebp+0xc]. The postfix iterator::operator++ 0x1000b320 runs on the argument (0x1000b826: ecx =
// &pos, `push 0` is its unused int): `pos` becomes the next node and the value it hands back is the node to
// remove, taken with _Mynode 0x1000b710 into [ebp-8] (0x1000b82d-0x1000b832). Then the node is unlinked:
//   prev->next = node->next   (0x1000b838-0x1000b85c: _Next(node) is &node->next, _Next(_Prev(node)) is &prev->next)
//   next->prev = node->prev   (0x1000b861-0x1000b885)
// the element is destroyed through allocator::destroy 0x1000b9f0 on _Value(node) = node + 8 (0x1000b88b-0x1000b897,
// ecx = this), the node is released with _Freenode 0x1000b5b0 (0x1000b89f-0x1000b8a3), the count at +8 is
// decremented (0x1000b8ab-0x1000b8b4), and the advanced iterator is stored into the return slot (0x1000b8b7-
// 0x1000b8bd). The slot is returned (0x1000b8bf). No branch: erasing the head node is not guarded.
EraseOne_t ListEraseOne_orig;
void* __fastcall ListEraseOne_re(void* self, void*, void* ret, void* pos) {
    void* node = pos;
    pos = At<void*>(node, 0);
    void* prev = At<void*>(node, 4);
    void* next = At<void*>(node, 0);
    At<void*>(prev, 0) = next;
    At<void*>(next, 4) = prev;
    ((Destroy_t)TerrainRva(0x0000b9f0))(self, 0, static_cast<char*>(node) + 8);
    ((Freenode_t)TerrainRva(0x0000b5b0))(self, 0, node);
    At<int>(self, 8) -= 1;
    At<void*>(ret, 0) = pos;
    return ret;
}

// 0x0000b470  std::list<Tile*>::erase (range) (VA 0x1000b470)
// iterator __thiscall erase(iterator first, iterator last), return slot first: `ret 0xc` at 0x1000b4dc, slot at
// [ebp+8], first at [ebp+0xc], last at [ebp+0x10]. Loop head 0x1000b48d: const_iterator::operator!= 0x1000b3d0
// on (&first, &last) (0x1000b494, `and eax, 0xff` / `test` / `je 0x1000b4c1` at 0x1000b499-0x1000b4a0). Body: the
// postfix operator++ 0x1000b320 on `first` (0x1000b4ab) hands back the position before the step, and the
// single-position erase 0x1000b800 runs on that value (0x1000b4b0-0x1000b4ba, the callee's own return slot is
// [ebp-0xc]); `jmp 0x1000b48d` closes the loop. After the loop `first` is stored into the return slot
// (0x1000b4c1-0x1000b4c7) and the slot is returned (0x1000b4c9). An unreachable `last` loops until the walk
// leaves the ring; the vectors never do that.
EraseRange_t ListEraseRange_orig;
void* __fastcall ListEraseRange_re(void* self, void*, void* ret, void* first, void* last) {
    while (first != last) {
        void* old = first;
        first = At<void*>(first, 0);          // iterator::operator++(int) -> const_iterator::operator++ (0x1000b7ce)
        void* tmp;
        ((EraseOne_t)TerrainRva(0x0000b800))(self, 0, &tmp, old);
    }
    At<void*>(ret, 0) = first;
    return ret;
}

// 0x0000b210  std::list<Tile*>::clear (VA 0x1000b210)
// __thiscall, no stack argument (plain `ret` at 0x1000b267). end 0x1000afe0 (0x1000b234, return slot [ebp-8]) and
// begin 0x1000af70 (0x1000b243, return slot [ebp-0xc]) are read, in that order, and the range erase 0x1000b470
// runs on (begin, end) with its slot at [ebp-0x10] (0x1000b252). No branch. eax is the erase result slot's address,
// a frame address no caller can use, so the reimplementation is void.
typedef void(__fastcall* ListVoid_t)(void*, void*);
ListVoid_t ListClear_orig;
void __fastcall ListClear_re(void* self, void*) {
    void* endIt;
    void* beginIt;
    void* tmp;
    ((IterOut_t)TerrainRva(0x0000afe0))(self, 0, &endIt);
    ((IterOut_t)TerrainRva(0x0000af70))(self, 0, &beginIt);
    ((EraseRange_t)TerrainRva(0x0000b470))(self, 0, &tmp, beginIt, endIt);
}

// 0x0000aed0  std::list<Tile*>::~list (VA 0x1000aed0)
// __thiscall, no stack argument (plain `ret` at 0x1000af4a). The first three calls are clear's (end 0x1000aef4
// into [ebp-8], begin 0x1000af03 into [ebp-0xc], range erase 0x1000af12 with its slot at [ebp-0x10]); then the head
// node [this+4] (0x1000af1a) is released with _Freenode 0x1000b5b0 (0x1000af21), and +4 and +8 are zeroed
// (0x1000af29, 0x1000af33). No branch.
ListVoid_t ListDtor_orig;
void __fastcall ListDtor_re(void* self, void*) {
    void* endIt;
    void* beginIt;
    void* tmp;
    ((IterOut_t)TerrainRva(0x0000afe0))(self, 0, &endIt);
    ((IterOut_t)TerrainRva(0x0000af70))(self, 0, &beginIt);
    ((EraseRange_t)TerrainRva(0x0000b470))(self, 0, &tmp, beginIt, endIt);
    ((Freenode_t)TerrainRva(0x0000b5b0))(self, 0, At<void*>(self, 4));
    At<void*>(self, 4) = 0;
    At<int>(self, 8) = 0;
}

// 0x0000b100  std::list<Tile*>::insert (VA 0x1000b100)
// iterator __thiscall insert(iterator pos, const value&), return slot first: `ret 0xc` at 0x1000b1ce, slot [ebp+8],
// pos [ebp+0xc], value reference [ebp+0x10]. The position's node comes from _Mynode 0x1000b710 (0x1000b120 ->
// [ebp-8]). A new node is bought with _Buynode 0x1000b500 (next = that node, prev = _Prev(node) read at 0x1000b12c-
// 0x1000b134; the pushes at 0x1000b136 and 0x1000b13a put prev first and next second, i.e. _Buynode(next, prev)),
// kept in esi and linked in:
//   node->prev = new                       (0x1000b143-0x1000b151: _Prev(node) is &node->prev)
//   [ebp-8] = node->prev (= new)           (0x1000b153-0x1000b161)
//   new->prev->next = new                  (0x1000b164-0x1000b17e: _Next(_Prev(new)) is &prev->next)
// the element is copied into _Value(new) = new + 8 by allocator::construct 0x1000b690 (value reference pushed at
// 0x1000b183, destination at 0x1000b190, ecx = this at 0x1000b191), the count at +8 is incremented (0x1000b199-
// 0x1000b1a5), and the new node is stored into the return slot through iterator::iterator 0x1000b750 (0x1000b1af-
// 0x1000b1b9). The slot is returned (0x1000b1bb). No branch.
Insert_t ListInsert_orig;
void* __fastcall ListInsert_re(void* self, void*, void* ret, void* pos, void* val) {
    void* node = pos;
    void* prev = At<void*>(node, 4);
    void* nn = ((Buynode_t)TerrainRva(0x0000b500))(self, 0, node, prev);
    At<void*>(node, 4) = nn;
    At<void*>(At<void*>(nn, 4), 0) = nn;
    ((Construct_t)TerrainRva(0x0000b690))(self, 0, static_cast<char*>(nn) + 8, val);
    At<int>(self, 8) += 1;
    At<void*>(ret, 0) = nn;
    return ret;
}

// 0x0000b090  std::list<Tile*>::push_back (VA 0x1000b090)
// __thiscall, one stack argument, the value reference (`ret 4` at 0x1000b0dc; read at 0x1000b0ad). end 0x1000afe0
// fills a slot at [ebp-8] (0x1000b0b8), its content is the position and insert 0x1000b100 runs on (this, slot
// [ebp-0xc], end, value) (0x1000b0bd-0x1000b0c7). No branch. eax is insert's slot address (a frame address), so the
// reimplementation is void.
typedef void(__fastcall* PushBack_t)(void*, void*, void*);
PushBack_t ListPushBack_orig;
void __fastcall ListPushBack_re(void* self, void*, void* val) {
    void* endIt;
    void* tmp;
    ((IterOut_t)TerrainRva(0x0000afe0))(self, 0, &endIt);
    ((Insert_t)TerrainRva(0x0000b100))(self, 0, &tmp, endIt, val);
}

// ===================================================================================================
// I. iostream helpers
//   ios object  +8 the state bits, +0x34 the lock count, +0x38 the CRITICAL_SECTION (ios::lockptr returns +0x38).
// ===================================================================================================

typedef int(__fastcall* IosThis_t)(void*, void*);
typedef void(__cdecl* IosStatic_t)(void*);

// 0x00002510  ios::fail (VA 0x10002510)
// __thiscall, no stack argument (plain `ret` at 0x1000253c): eax = [this+8] & 6 (0x10002530-0x10002533). The
// result is the dword masked, not a bool: 2 (eofbit's neighbour) and 4 stay 2 and 4, 6 stays 6. No branch.
IosThis_t IosFail_orig;
int __fastcall IosFail_re(void* self, void*) {
    return At<int>(self, 8) & 6;
}

// 0x00003f80  ios::lockptr (VA 0x10003f80)
// __thiscall, no stack argument (plain `ret` at 0x10003fa9): eax = this + 0x38 (0x10003fa0). No branch, no read.
typedef void*(__fastcall* IosPtr_t)(void*, void*);
IosPtr_t IosLockptr_orig;
void* __fastcall IosLockptr_re(void* self, void*) {
    return static_cast<char*>(self) + 0x38;
}

// 0x000018b0  ios::lock (VA 0x100018b0)
// static, __cdecl, one stack argument, the ios pointer (read at 0x100018c8, plain `ret` at 0x100018f2). The lock
// is taken only when the lock count at +0x34 is negative (`cmp dword ptr [eax+0x34], 0` / `jge 0x100018e2` at
// 0x100018cb-0x100018cf, a SIGNED test). Then ios::lockptr 0x10003f80 (thunk 0x1000110e at 0x100018d4) gives
// the section pointer and the CRT wrapper 0x10034a70 enters it (0x100018da): that wrapper is
// `push [ebp+8]; call [0x10113510]`, an indirect call through Terrain.dll's IAT slot for KERNEL32's
// EnterCriticalSection.
IosStatic_t IosLock_orig;
void __cdecl IosLock_re(void* ios) {
    if (At<int>(ios, 0x34) < 0) {
        void* cs = ((IosPtr_t)TerrainRva(0x00003f80))(ios, 0);
        ((IosStatic_t)TerrainRva(0x00034a70))(cs);
    }
}

// 0x00037930  ios::unlock (VA 0x10037930)
// The same shape with the wrapper 0x10034a80 (`push [ebp+8]; call [0x10113514]`, LeaveCriticalSection): the signed
// test of +0x34 at 0x1003794b-0x1003794f, ios::lockptr through the thunk 0x1000110e at 0x10037954, the wrapper at
// 0x1003795a, plain `ret` at 0x10037972.
IosStatic_t IosUnlock_orig;
void __cdecl IosUnlock_re(void* ios) {
    if (At<int>(ios, 0x34) < 0) {
        void* cs = ((IosPtr_t)TerrainRva(0x00003f80))(ios, 0);
        ((IosStatic_t)TerrainRva(0x00034a80))(cs);
    }
}

// ===================================================================================================
// T. Tile and Terrain
// ===================================================================================================

// 0x0000c210  Tile::Tile (VA 0x1000c210)
// __thiscall, no stack argument (plain `ret` at 0x1000c294), with an SEH frame (handler 0x1004119c pushed at
// 0x1000c215; the frame link is `mov fs:[0], esp` at 0x1000c221 and the state byte [ebp-4] is 0 while the Faces
// member is built and 0xffffffff afterwards). Three member constructors run in this order on `this` + offset:
//   +0x44   Faces::Faces 0x10001fa0     (thunk 0x100011a4 at 0x1000c248)
//   +0x208  PathInfo::PathInfo 0x1000f690 (thunk 0x1000113b at 0x1000c25d)
//   +0x210  WallInfo::WallInfo 0x1000f750 (thunk 0x100011ae at 0x1000c26b)
// and `this` is returned (0x1000c277). The bytes the constructors leave alone keep the allocation's content.
typedef void*(__fastcall* TileCtor_t)(void*, void*);
TileCtor_t TileCtor_orig;
void* __fastcall TileCtor_re(void* self, void*) {
    ((TileCtor_t)TerrainRva(0x00001fa0))(static_cast<char*>(self) + 0x44, 0);
    ((TileCtor_t)TerrainRva(0x0000f690))(static_cast<char*>(self) + 0x208, 0);
    ((TileCtor_t)TerrainRva(0x0000f750))(static_cast<char*>(self) + 0x210, 0);
    return self;
}

// 0x0000c3d0  Tile::~Tile (VA 0x1000c3d0)
// __thiscall, no stack argument (plain `ret` at 0x1000c4da), SEH frame like the constructor. Stores: +0x2c = -1 and
// +0x30 = -1 (0x1000c40c, 0x1000c416), +0, +4, +8, +0xc = 0 (0x1000c420-0x1000c43d), the byte +0x244 = 0
// (0x1000c447). Then each neighbour pointer that is not NULL gets Tile::setNeighbour 0x1000c520 (thunk 0x10001113)
// with the side that points back and a NULL tile:
//   [+0x34] -> setNeighbour(1, 0)   (test 0x1000c451 / je 0x1000c466, call 0x1000c461)
//   [+0x38] -> setNeighbour(0, 0)   (0x1000c469 / 0x1000c46d, call 0x1000c479)
//   [+0x3c] -> setNeighbour(3, 0)   (0x1000c481 / 0x1000c485, call 0x1000c491)
//   [+0x40] -> setNeighbour(2, 0)   (0x1000c499 / 0x1000c49d, call 0x1000c4a9)
// and finally Faces::~Faces 0x10001fe0 (thunk 0x10001348) runs on +0x44 (0x1000c4bb). Nothing is returned.
typedef void(__fastcall* SetNeighbour_t)(void*, void*, int, void*);
ListVoid_t TileDtor_orig;
void __fastcall TileDtor_re(void* self, void*) {
    At<int>(self, 0x2c) = -1;
    At<int>(self, 0x30) = -1;
    At<int>(self, 0) = 0;
    At<int>(self, 4) = 0;
    At<int>(self, 8) = 0;
    At<int>(self, 0xc) = 0;
    At<char>(self, 0x244) = 0;
    SetNeighbour_t set = (SetNeighbour_t)TerrainRva(0x0000c520);
    if (At<void*>(self, 0x34)) set(At<void*>(self, 0x34), 0, 1, 0);
    if (At<void*>(self, 0x38)) set(At<void*>(self, 0x38), 0, 0, 0);
    if (At<void*>(self, 0x3c)) set(At<void*>(self, 0x3c), 0, 3, 0);
    if (At<void*>(self, 0x40)) set(At<void*>(self, 0x40), 0, 2, 0);
    ((ListVoid_t)TerrainRva(0x00001fe0))(static_cast<char*>(self) + 0x44, 0);
}

// 0x0000adc0  Terrain::setViewAngle (VA 0x1000adc0)
// __thiscall, one float argument (`ret 4` at 0x1000ae33, read at 0x1000addd). g_viewAngleIndex 0x10070a14 is
// written with:
//   0 when the angle compares equal to 0.0f (0x1005f1e0 holds 0.0f): `fld` / `fcomp` / `fnstsw ax` / `test ah, 0x40`
//     / `je` (0x1000addd-0x1000adeb) tests C3, which the x87 sets for EQUAL and for UNORDERED, so a NaN also gives 0;
//   1 when the argument's bits are 0x42b40000 (90.0f; `cmp dword ptr [ebp+8], 0x42b40000` / `jne` at 0x1000adf9);
//   2 when they are 0x43340000 (180.0f, 0x1000ae0e);
//   3 otherwise (0x1000ae23).
// The 90 and 180 tests are on the bits, not on a compare.
typedef void(__fastcall* SetViewAngle_t)(void*, void*, float);
SetViewAngle_t SetViewAngle_orig;
void __fastcall SetViewAngle_re(void*, void*, float angle) {
    unsigned bits;
    memcpy(&bits, &angle, 4);
    int* g = reinterpret_cast<int*>(TerrainRva(0x00070a14));
    if (angle == 0.0f || angle != angle)
        *g = 0;
    else if (bits == 0x42b40000u)
        *g = 1;
    else if (bits == 0x43340000u)
        *g = 2;
    else
        *g = 3;
}

// ===================================================================================================
// G. OpenGL state setup and the grid builder
//
// Terrain.dll calls OpenGL as `call dword ptr [slot]` through its own IAT (OPENGL32.dll slots 0x101135d4..0x10113684),
// with `mov esi, esp` before and `cmp esi, esp` / `call 0x100180a0` (__chkesp) after every call. The reimplementations
// read the slot at call time, so the import stand-in (importstandin.js), which rewrites the slot, sees every call.
// ===================================================================================================

void* GlSlot(unsigned slotRva) { return *reinterpret_cast<void**>(TerrainRva(slotRva)); }
typedef void(WINAPI* Gl1_t)(unsigned);
typedef void(WINAPI* Gl2_t)(unsigned, unsigned);
typedef void(WINAPI* Gl3_t)(unsigned, unsigned, const void*);
typedef void(WINAPI* Gl0_t)();
typedef void(WINAPI* GlViewport_t)(int, int, int, int);
typedef void(WINAPI* GlOrtho_t)(double, double, double, double, double, double);
typedef void(WINAPI* GlDeleteTextures_t)(int, const void*);
void glEnable_(unsigned cap) { ((Gl1_t)GlSlot(0x1135e0))(cap); }                 // slot 0x101135e0
void glDisable_(unsigned cap) { ((Gl1_t)GlSlot(0x113680))(cap); }                // slot 0x10113680
void glHint_(unsigned t, unsigned m) { ((Gl2_t)GlSlot(0x1135d8))(t, m); }        // slot 0x101135d8
void glFrontFace_(unsigned m) { ((Gl1_t)GlSlot(0x1135dc))(m); }                  // slot 0x101135dc
void glEnableClientState_(unsigned a) { ((Gl1_t)GlSlot(0x1135d4))(a); }          // slot 0x101135d4
void glLightfv_(unsigned l, unsigned p, const void* v) { ((Gl3_t)GlSlot(0x113684))(l, p, v); }  // slot 0x10113684
void glMatrixMode_(unsigned m) { ((Gl1_t)GlSlot(0x113644))(m); }                 // slot 0x10113644
void glLoadIdentity_() { ((Gl0_t)GlSlot(0x11364c))(); }                          // slot 0x1011364c

// 0x000033e0  Terrain::initGL (VA 0x100033e0)
// bool __thiscall, no stack argument (plain `ret` at 0x100034ef). Ten GL calls and one branch, in this order:
//   glHint(0x0c50, 0x1101)  (pushes 0x1101 then 0xc50 at 0x100033ff-0x10003404, call [0x101135d8] at 0x10003409)
//   glEnable(0x0b20)        (0x10003418, [0x101135e0])
//   glHint(0x0c52, 0x1102)  (0x10003424-0x10003436)
//   glEnable(0x0de1)        (0x10003445)     glEnable(0x0b50)   (0x10003459)
//   the front face from the BYTE at this+0x28 (`xor ecx, ecx` / `mov cl, [eax+0x28]` / `test ecx, ecx` at 0x1000346e-
//   0x10003475): non-zero -> glFrontFace(0x0900) (0x10003479-0x1000347e, [0x101135dc]), zero -> glFrontFace(0x0901)
//   (0x1000348f-0x10003494)
//   glEnable(0x0b44)        (0x100034a3)
//   glEnableClientState(0x8074) (0x100034b7, [0x101135d4])   glEnableClientState(0x8075) (0x100034cb)
// and `mov al, 1` (0x100034dd): the result is the bool true in al; the upper bytes of eax are not set here.
typedef unsigned char(__fastcall* BoolThis_t)(void*, void*);
BoolThis_t InitGL_orig;
unsigned char __fastcall InitGL_re(void* self, void*) {
    glHint_(0x0c50, 0x1101);
    glEnable_(0x0b20);
    glHint_(0x0c52, 0x1102);
    glEnable_(0x0de1);
    glEnable_(0x0b50);
    if (At<unsigned char>(self, 0x28) != 0)
        glFrontFace_(0x0900);
    else
        glFrontFace_(0x0901);
    glEnable_(0x0b44);
    glEnableClientState_(0x8074);
    glEnableClientState_(0x8075);
    return 1;
}

// 0x00003830  Terrain::light1Off (VA 0x10003830)
// void __thiscall, no stack argument (plain `ret` at 0x10003930); `this` is stored (0x1000384a) and never read.
// glDisable(0x4000) (GL_LIGHT0, 0x1000384f-0x10003854, [0x10113680]); three zero float4 arrays are built on the
// stack (0x10003861-0x100038b0, twelve dword stores of 0); glLightfv(0x4001, 0x1200, ambient) (0x100038b7-
// 0x100038c5, [0x10113684]), glLightfv(0x4001, 0x1201, diffuse) (0x100038d4-0x100038e2) and
// glLightfv(0x4001, 0x1202, specular) (0x100038f1-0x100038ff); glEnable(0x4001) (0x1000390e, [0x101135e0]).
// No branch.
typedef void(__fastcall* VoidThis_t)(void*, void*);
VoidThis_t Light1Off_orig;
void __fastcall Light1Off_re(void*, void*) {
    float ambient[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float diffuse[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float specular[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glDisable_(0x4000);
    glLightfv_(0x4001, 0x1200, ambient);
    glLightfv_(0x4001, 0x1201, diffuse);
    glLightfv_(0x4001, 0x1202, specular);
    glEnable_(0x4001);
}

// 0x000380a0  Terrain::reloadTextures (VA 0x100380a0)
// void __thiscall, no stack argument (plain `ret` at 0x10038193). Gated by the byte at this+0x29 (`mov cl, [eax+0x29]` /
// `test ecx, ecx` / `jne` at 0x100380c0-0x100380c7): zero returns at once (`jmp 0x10038183`). Otherwise for each
// type t in 0..0x24 (`cmp [ebp-8], 0x25` at 0x100380e0) and each variation v in 0..0x18 (`cmp [ebp-0xc], 0x19` at
// 0x100380fc): glDeleteTextures(9, &g_textures[t][v]) with the row address 0x100687f8 + t * 0x384 + v * 0x24
// (0x10038105-0x10038111, call [0x10113674] at 0x1003811d), then nine dword stores of 0 into that row (loop to 9 at
// 0x1003813c). After the variation loop the type's `loaded` dword at this + t * 0x18 + 0x40 is zeroed (0x10038166-
// 0x10038177). At the end the flag byte +0x29 is cleared (0x1003817f).
VoidThis_t ReloadTextures_orig;
void __fastcall ReloadTextures_re(void* self, void*) {
    if (At<unsigned char>(self, 0x29) == 0)
        return;
    char* table = reinterpret_cast<char*>(TerrainRva(0x000687f8));
    for (int t = 0; t < 0x25; t++) {
        for (int v = 0; v < 0x19; v++) {
            unsigned* row = reinterpret_cast<unsigned*>(table + t * 0x384 + v * 0x24);
            ((GlDeleteTextures_t)GlSlot(0x113674))(9, row);
            for (int k = 0; k < 9; k++)
                row[k] = 0;
        }
        At<int>(self, 0x40 + t * 0x18) = 0;
    }
    At<unsigned char>(self, 0x29) = 0;
}

// 0x00009ed0  Terrain::resize (VA 0x10009ed0)
// void __thiscall (int width, int height), `ret 8` at 0x1000a0b4 (width [ebp+8], height [ebp+0xc]). A zero height is
// replaced by 1 (0x10009eed-0x10009ef3) and then a zero width by 1 (0x10009efa-0x10009f00). glViewport(0, 0, width,
// height) (pushes at 0x10009f09-0x10009f13, [0x10113610]); glMatrixMode(0x1701) (0x10009f24, [0x10113644]);
// glLoadIdentity() (0x10009f38, [0x1011364c]). Three exact sizes are replaced by a fixed extent (compares at 0x10009f45-
// 0x10009f9b): 800 x 600 -> 1767 x 1325 (0x6e7, 0x52d), 1024 x 768 -> 1810 x 1303 (0x712, 0x517), 1280 x 1024 ->
// 1740 x 1392 (0x6cc, 0x570); any other pair is kept. The projection depends on the BYTE at this+0x28
// (`mov al, [edx+0x28]` / `test` at 0x10009fa9-0x10009fb3): non-zero calls glOrtho(-w >> 1, w >> 1, h >> 1, -h >> 1,
// 5000.0, -5000.0) (the doubles are built with `neg; sar 1; fild` at 0x10009fc5-0x1000a004, near 0x40b38800:0 and far
// 0xc0b38800:0 pushed at 0x10009fb7-0x10009fbe; call [0x10113650] at 0x1000a00d); zero calls glOrtho(
// -w >> 1, w >> 1, -h >> 1, h >> 1, 5000.0, -5000.0) (0x1000a01c-0x1000a074). `-w >> 1` is the arithmetic shift of the
// negated value. Then glMatrixMode(0x1700) (0x1000a083) and glLoadIdentity() (0x1000a095).
typedef void(__fastcall* Resize_t)(void*, void*, int, int);
Resize_t Resize_orig;
void __fastcall Resize_re(void* self, void*, int width, int height) {
    if (height == 0) height = 1;
    if (width == 0) width = 1;
    ((GlViewport_t)GlSlot(0x113610))(0, 0, width, height);
    glMatrixMode_(0x1701);
    glLoadIdentity_();
    if (width == 800 && height == 600) {
        width = 1767;
        height = 1325;
    } else if (width == 1024 && height == 768) {
        width = 1810;
        height = 1303;
    } else if (width == 1280 && height == 1024) {
        width = 1740;
        height = 1392;
    }
    GlOrtho_t ortho = (GlOrtho_t)GlSlot(0x113650);
    if (At<unsigned char>(self, 0x28) != 0)
        ortho((double)((-width) >> 1), (double)(width >> 1), (double)(height >> 1), (double)((-height) >> 1), 5000.0, -5000.0);
    else
        ortho((double)((-width) >> 1), (double)(width >> 1), (double)((-height) >> 1), (double)(height >> 1), 5000.0, -5000.0);
    glMatrixMode_(0x1700);
    glLoadIdentity_();
}

// 0x0000a130  Terrain::buildArrays (VA 0x1000a130)
// bool __thiscall, no stack argument (plain `ret` at 0x1000a354); width at this+0x14, height at this+0x18, the tile
// array at this+0x3a4 with stride 0x248 and index x + y * width (imul at 0x1000a1af-0x1000a1b8). First
// initGrid 0x1000c560 (thunk 0x100010d7) is called with (width, height) (pushes at 0x1000a150-0x1000a15a, cdecl: `add
// esp, 8` at 0x1000a160). Then for x in 0..width-1, y in 0..height-1 (bounds re-read from the object at every test,
// 0x1000a175-0x1000a17e and 0x1000a196-0x1000a19f): Tile::reset(x, y) (0x1000c2c0, thunk 0x1000119f, pushes y then x at
// 0x1000a1a1-0x1000a1a8) on that tile, then Tile::getFaces 0x100153c0 (thunk 0x100010aa) on the same tile and
// Faces::build(x, y, width) 0x10002060 (thunk 0x100010c3, pushes width, y, x at 0x1000a1d0-0x1000a1db) on the
// returned pointer. Then a second pass over the same ranges (0x1000a20e-0x1000a33d) wires the four neighbours of the tile
// (x, y), each tile taken with Terrain::tileAt 0x10001d50 (thunk 0x1000108c, pushes y then x, ecx = this) and passed to
// Tile::setNeighbour 0x1000c520 (thunk 0x10001113): side 2 <- (x-1, y) (0x1000a250-0x1000a285), side 0 <- (x, y-1)
// (0x1000a28a-0x1000a2bf), side 3 <- (x+1, y) (0x1000a2c4-0x1000a2f9), side 1 <- (x, y+1) (0x1000a2fe-0x1000a333). The
// result is `mov al, 1` (0x1000a342).
typedef void(__cdecl* InitGrid_t)(int, int);
typedef void(__fastcall* TileReset_t)(void*, void*, int, int);
typedef void*(__fastcall* GetFaces_t)(void*, void*);
typedef void(__fastcall* FacesBuild_t)(void*, void*, int, int, int);
typedef void*(__fastcall* TileAt_t)(void*, void*, int, int);
BoolThis_t BuildArrays_orig;
unsigned char __fastcall BuildArrays_re(void* self, void*) {
    ((InitGrid_t)TerrainRva(0x0000c560))(At<int>(self, 0x14), At<int>(self, 0x18));
    for (int x = 0; x < At<int>(self, 0x14); x++) {
        for (int y = 0; y < At<int>(self, 0x18); y++) {
            char* tile = static_cast<char*>(self) + 0x3a4 + (x + y * At<int>(self, 0x14)) * 0x248;
            ((TileReset_t)TerrainRva(0x0000c2c0))(tile, 0, x, y);
            void* faces = ((GetFaces_t)TerrainRva(0x000153c0))(tile, 0);
            ((FacesBuild_t)TerrainRva(0x00002060))(faces, 0, x, y, At<int>(self, 0x14));
        }
    }
    TileAt_t tileAt = (TileAt_t)TerrainRva(0x00001d50);
    SetNeighbour_t set = (SetNeighbour_t)TerrainRva(0x0000c520);
    for (int x = 0; x < At<int>(self, 0x14); x++) {
        for (int y = 0; y < At<int>(self, 0x18); y++) {
            char* tile = static_cast<char*>(self) + 0x3a4 + (x + y * At<int>(self, 0x14)) * 0x248;
            set(tile, 0, 2, tileAt(self, 0, x - 1, y));
            set(tile, 0, 0, tileAt(self, 0, x, y - 1));
            set(tile, 0, 3, tileAt(self, 0, x + 1, y));
            set(tile, 0, 1, tileAt(self, 0, x, y + 1));
        }
    }
    return 1;
}

// ===================================================================================================
// D. The tile draw family, isCulled and the draw-list builders
//
// Tile layout read below (all offsets are into the Tile object):
//   +0x24 type (0x14 = hidden, Tile::isHidden 0x10015460)   +0x28 variation / rotation dword
//   +0x2c x, +0x30 y (Tile::getX 0x10005960, Tile::getY 0x10006810)   +0x34 neighbour[4]   +0x44 face count
//   +0x48 face[8], stride 0x38: vertex indices at +0, +4, +8; texcoord pairs at +0xc, +0x14, +0x1c; texture at +0x24;
//         texture slot byte at +0x28
//   +0x208 hasPath byte, +0x209 connected byte, +0x20a drawPath byte   +0x210 wall kinds, 9 dwords   +0x234 wall flags,
//   9 bytes   +0x240 variation index   corner heights at +4, +0xc, +0x14, +0x1c (Tile::maxHeight 0x10015500)
// Module data (RVA): g_vertices 0xb28c8 and g_normals 0x70a18 (12-byte entries), g_textures 0x687f8 (0x384 per type,
// 0x24 per variation), g_texCoords 0x63ca0, g_viewAngleIndex 0x70a14, g_screenScale 0x70a10 (float).
// ===================================================================================================

typedef void(WINAPI* GlF2_t)(float, float);
typedef void(WINAPI* GlF3_t)(float, float, float);
typedef void(WINAPI* GlF4_t)(float, float, float, float);
typedef void(WINAPI* GlPtr_t)(const float*);
typedef void(WINAPI* GlInt1_t)(int);
typedef void(WINAPI* GlRotated_t)(double, double, double, double);
void glBegin_(unsigned m) { ((Gl1_t)GlSlot(0x113660))(m); }                                   // slot 0x10113660
void glEnd_() { ((Gl0_t)GlSlot(0x113668))(); }                                                // slot 0x10113668
void glBindTexture_(unsigned t, unsigned n) { ((Gl2_t)GlSlot(0x113624))(t, n); }              // slot 0x10113624
void glNormal3fv_(const float* v) { ((GlPtr_t)GlSlot(0x1135e4))(v); }                         // slot 0x101135e4
void glVertex3fv_(const float* v) { ((GlPtr_t)GlSlot(0x1135e8))(v); }                         // slot 0x101135e8
void glTexCoord2fv_(const float* v) { ((GlPtr_t)GlSlot(0x1135ec))(v); }                       // slot 0x101135ec
void glArrayElement_(int i) { ((GlInt1_t)GlSlot(0x1135f0))(i); }                              // slot 0x101135f0
void glVertex3f_(float x, float y, float z) { ((GlF3_t)GlSlot(0x113630))(x, y, z); }          // slot 0x10113630
void glTexCoord2f_(float s, float t) { ((GlF2_t)GlSlot(0x11362c))(s, t); }                    // slot 0x1011362c
void glBlendFunc_(unsigned s, unsigned d) { ((Gl2_t)GlSlot(0x113654))(s, d); }                // slot 0x10113654
void glPushMatrix_() { ((Gl0_t)GlSlot(0x113648))(); }                                         // slot 0x10113648
void glPopMatrix_() { ((Gl0_t)GlSlot(0x11366c))(); }                                          // slot 0x1011366c
void glFlush_() { ((Gl0_t)GlSlot(0x113670))(); }                                              // slot 0x10113670
void glRotated_(double a, double x, double y, double z) { ((GlRotated_t)GlSlot(0x113634))(a, x, y, z); }  // slot 0x10113634
void glRotatef_(float a, float x, float y, float z) { ((GlF4_t)GlSlot(0x113638))(a, x, y, z); }           // slot 0x10113638
void glTranslatef_(float x, float y, float z) { ((GlF3_t)GlSlot(0x11363c))(x, y, z); }                     // slot 0x1011363c

unsigned char* TBytes(unsigned rva) { return TerrainBase() + rva; }
float* Vert(int i) { return reinterpret_cast<float*>(TBytes(0xb28c8) + i * 12); }
float* Norm(int i) { return reinterpret_cast<float*>(TBytes(0x70a18) + i * 12); }
int ViewIndex() { return *reinterpret_cast<int*>(TBytes(0x70a14)); }
unsigned TexName(int type, int variation, int slot) {
    return *reinterpret_cast<unsigned*>(TBytes(0x687f8) + type * 0x384 + variation * 0x24 + slot * 4);
}
int FaceV(void* tile, int face, int k) { return At<int>(tile, 0x48 + face * 0x38 + k * 4); }
void* Nbr(void* tile, int k) { return At<void*>(tile, 0x34 + k * 4); }
unsigned FBits(float f) { unsigned u; memcpy(&u, &f, 4); return u; }

// The camera-angle test every caller below shares. 0 = the angle compares equal to 0.0 (the x87 `fcomp` + `test ah, 0x40`
// is true for equal AND unordered, so a NaN lands here), 1 = the argument's bits are 0x42b40000 (90.0f), 2 = 0x43340000
// (180.0f), 3 = anything else. 90 and 180 are bit compares (`cmp dword ptr [ebp+0x10], 0x42b40000` at 0x1000746d, 0x100383a5
// and 0x10038b2b, `0x43340000` at 0x1000750b, 0x10038562 and 0x10038c3d).
int AngleKind(float a) {
    if (a == 0.0f || a != a) return 0;
    unsigned b = FBits(a);
    if (b == 0x42b40000u) return 1;
    if (b == 0x43340000u) return 2;
    return 3;
}

// Tile and Terrain helpers reached through their original addresses (all of them are at C3 already except isCulled and
// drawTile, which are this batch's and are called through their detours).
typedef int(__fastcall* TileInt_t)(void*, void*);
typedef unsigned char(__fastcall* TileBool_t)(void*, void*);
int TileGetX(void* t) { return ((TileInt_t)TerrainRva(0x00005960))(t, 0); }
int TileGetY(void* t) { return ((TileInt_t)TerrainRva(0x00006810))(t, 0); }
int TileGetType(void* t) { return ((TileInt_t)TerrainRva(0x00001f60))(t, 0); }
int TileMaxHeight(void* t) { return ((TileInt_t)TerrainRva(0x00015500))(t, 0); }
bool TileHidden(void* t) { return ((TileBool_t)TerrainRva(0x00015460))(t, 0) != 0; }

// 0x000116d0  Tile::drawCornerOverlays (VA 0x100116d0)
// void __thiscall (int mask, int type), `ret 8`. Four counters a, b, c, d start at 0 and a switch on the mask (jump
// table) sets them: 0 -> a = b = c = d = 3; 1 -> c = d = 3; 2 -> a = b = 3; 4 -> a = c = 3; 8 -> b = d = 3; 5 -> c = 3,
// b = 2; 9 -> d = 3, a = 2; 7 -> b = d = 2; 11 -> a = c = 2; 13 -> a = b = 2; 6 -> a = 3, d = 2; 10 -> b = 3, c = 2;
// 14 -> c = d = 2; 15 -> all 2. Any other mask returns at once. Then, for each non-zero counter in the order a, b, c, d:
// glBindTexture(0x0de1, g_textures[type][0][counter]) and one GL_TRIANGLES (4) with three glTexCoord2f / glArrayElement
// pairs over the tile's face vertices:
//   a: (0, .5) f0.v1, (.5, .5) f0.v2, (.5, 1) f1.v2     b: (.5, 1) f1.v2, (.5, .5) f0.v2, (1, .5) f3.v1
//   c: (.5, 0) f5.v1, (.5, .5) f4.v2, (0, .5) f4.v0     d: (1, .5) f3.v1, (.5, .5) f4.v2, (.5, 0) f5.v1
typedef void(__fastcall* CornerOverlays_t)(void*, void*, int, int);
CornerOverlays_t CornerOverlays_orig;
void CornerTri(unsigned tex, float s0, float t0, int v0, float s1, float t1, int v1, float s2, float t2, int v2) {
    glBindTexture_(0x0de1, tex);
    glBegin_(4);
    glTexCoord2f_(s0, t0);
    glArrayElement_(v0);
    glTexCoord2f_(s1, t1);
    glArrayElement_(v1);
    glTexCoord2f_(s2, t2);
    glArrayElement_(v2);
    glEnd_();
}
void __fastcall CornerOverlays_re(void* self, void*, int mask, int type) {
    int a = 0, b = 0, c = 0, d = 0;
    switch (mask) {
    case 0: c = d = a = b = 3; break;
    case 1: d = c = 3; break;
    case 2: b = a = 3; break;
    case 4: a = c = 3; break;
    case 8: b = d = 3; break;
    case 5: c = 3; b = 2; break;
    case 9: d = 3; a = 2; break;
    case 7: d = b = 2; break;
    case 11: c = a = 2; break;
    case 13: b = a = 2; break;
    case 6: a = 3; d = 2; break;
    case 10: b = 3; c = 2; break;
    case 14: d = c = 2; break;
    case 15: c = d = a = b = 2; break;
    default: return;
    }
    if (a) CornerTri(TexName(type, 0, a), 0.0f, 0.5f, FaceV(self, 0, 1), 0.5f, 0.5f, FaceV(self, 0, 2), 0.5f, 1.0f, FaceV(self, 1, 2));
    if (b) CornerTri(TexName(type, 0, b), 0.5f, 1.0f, FaceV(self, 1, 2), 0.5f, 0.5f, FaceV(self, 0, 2), 1.0f, 0.5f, FaceV(self, 3, 1));
    if (c) CornerTri(TexName(type, 0, c), 0.5f, 0.0f, FaceV(self, 5, 1), 0.5f, 0.5f, FaceV(self, 4, 2), 0.0f, 0.5f, FaceV(self, 4, 0));
    if (d) CornerTri(TexName(type, 0, d), 1.0f, 0.5f, FaceV(self, 3, 1), 0.5f, 0.5f, FaceV(self, 4, 2), 0.5f, 0.0f, FaceV(self, 5, 1));
}

// 0x000108f0  Tile::drawPaths (VA 0x100108f0)
// void __thiscall, no stack argument. Returns at once when the byte at +0x20a is zero. Otherwise w = 16.67f and the four
// neighbour flags are n0 = nbr[0] && nbr[0]->byte[+0x208], n2 from nbr[2], n3 from nbr[3], n1 from nbr[1]; the texture type
// is 0x21 when the byte +0x209 is set, 0x20 otherwise. glEnable(0x0be2), glBlendFunc(0x302, 0x303),
// glBindTexture(0x0de1, g_textures[type][0][0]), then one GL_TRIANGLE_FAN (6) of four vertices per flag in the order n0
// (mask += 1), n1 (+= 2), n3 (+= 4), n2 (+= 8), each fan re-binding the same texture except the first; every vertex is a
// glNormal3fv of an entry of g_normals, a glTexCoord2f and a glVertex3f whose coordinates are g_vertices entries of the
// tile's faces with +-w on one axis. Then drawCornerOverlays(mask, type) and glDisable(0x0be2).
VoidThis_t DrawPaths_orig;
void PathVertex(int ni, float s, float t, float x, float y, float z) {
    glNormal3fv_(Norm(ni));
    glTexCoord2f_(s, t);
    glVertex3f_(x, y, z);
}
void __fastcall DrawPaths_re(void* self, void*) {
    if (At<unsigned char>(self, 0x20a) == 0)
        return;
    int mask = 0;
    const float w = 16.67f;
    bool n0 = Nbr(self, 0) && At<unsigned char>(Nbr(self, 0), 0x208);
    bool n2 = Nbr(self, 2) && At<unsigned char>(Nbr(self, 2), 0x208);
    bool n3 = Nbr(self, 3) && At<unsigned char>(Nbr(self, 3), 0x208);
    bool n1 = Nbr(self, 1) && At<unsigned char>(Nbr(self, 1), 0x208);
    int tex = At<unsigned char>(self, 0x209) ? 0x21 : 0x20;
    const int f0v2 = FaceV(self, 0, 2), f1v2 = FaceV(self, 1, 2), f6v1 = FaceV(self, 6, 1), f6v2 = FaceV(self, 6, 2),
              f7v2 = FaceV(self, 7, 2), f4v0 = FaceV(self, 4, 0);
    const float* a = Vert(f0v2);
    glEnable_(0x0be2);
    glBlendFunc_(0x302, 0x303);
    glBindTexture_(0x0de1, TexName(tex, 0, 0));
    if (n0) {
        mask += 1;
        glBegin_(6);
        PathVertex(f0v2, 0.333f, 0.667f, a[0] - w, a[1], a[2]);
        PathVertex(f0v2, 0.667f, 0.667f, w + a[0], a[1], a[2]);
        PathVertex(f1v2, 0.667f, 1.0f, w + a[0], Vert(f1v2)[1], Vert(f1v2)[2]);
        PathVertex(f1v2, 0.333f, 1.0f, a[0] - w, Vert(f1v2)[1], Vert(f1v2)[2]);
        glEnd_();
    }
    if (n1) {
        mask += 2;
        glBindTexture_(0x0de1, TexName(tex, 0, 0));
        glBegin_(6);
        PathVertex(f0v2, 0.333f, 0.333f, a[0] - w, a[1], a[2]);
        PathVertex(f6v1, 0.333f, 0.0f, Vert(f6v1)[0] - w, Vert(f6v1)[1], Vert(f6v1)[2]);
        PathVertex(f6v2, 0.667f, 0.0f, w + Vert(f6v1)[0], Vert(f6v1)[1], Vert(f6v1)[2]);
        PathVertex(f0v2, 0.667f, 0.333f, w + a[0], a[1], a[2]);
        glEnd_();
    }
    if (n3) {
        mask += 4;
        glBindTexture_(0x0de1, TexName(tex, 0, 0));
        glBegin_(6);
        PathVertex(f0v2, 0.667f, 0.667f, a[0], a[1], a[2] - w);
        PathVertex(f0v2, 0.667f, 0.333f, a[0], a[1], w + a[2]);
        PathVertex(f7v2, 1.0f, 0.333f, Vert(f7v2)[0], Vert(f7v2)[1], w + Vert(f7v2)[2]);
        PathVertex(f7v2, 1.0f, 0.667f, Vert(f7v2)[0], Vert(f7v2)[1], Vert(f7v2)[2] - w);
        glEnd_();
    }
    if (n2) {
        mask += 8;
        glBindTexture_(0x0de1, TexName(tex, 0, 0));
        glBegin_(6);
        PathVertex(f0v2, 0.333f, 0.667f, a[0], a[1], a[2] - w);
        PathVertex(f4v0, 0.0f, 0.667f, Vert(f4v0)[0], Vert(f4v0)[1], Vert(f4v0)[2] - w);
        PathVertex(f4v0, 0.0f, 0.333f, Vert(f4v0)[0], Vert(f4v0)[1], w + Vert(f4v0)[2]);
        PathVertex(f0v2, 0.333f, 0.333f, a[0], a[1], w + a[2]);
        glEnd_();
    }
    ((CornerOverlays_t)TerrainRva(0x000116d0))(self, 0, mask, tex);
    glDisable_(0x0be2);
}

// 0x0000f7f0  Tile::drawWalls (VA 0x1000f7f0)
// void __thiscall, no stack argument. The normal row is 0 for view quadrant 0 and 2 and 1 otherwise (g_viewAngleIndex
// 0x10070a14). Four edges are tried in the order (wall flag byte at +0x234 + k, neighbour n): (0, n[0]), (6, n[2]), (2, n[3]),
// (4, n[1]); an edge is drawn when its flag byte is non-zero AND the neighbour exists. Drawing: glBindTexture(0x0de1, the
// texture at 0x10070304 when the wall kind dword at +0x210 + 4k is 1, else the one at 0x1006ff80); one GL_TRIANGLES with the
// edge's normal row (glNormal3fv of 0x10063c40 / 0x10063c70 / 0x10063c88 / 0x10063c58 + row * 12) and twelve (glTexCoord2f,
// glVertex3fv) pairs: four triangles between this tile's face vertices (S) and the neighbour's facing ones (N), listed
// in WALL_EDGES below as (u, v, tile, face, vertex).
struct WallVertex { float u, v; int which; int face; int vtx; };   // which 0 = this tile, 1 = the neighbour
struct WallEdge { int flag; int nbr; unsigned normalsRva; WallVertex p[12]; };
const WallEdge WALL_EDGES[4] = {
    {0, 0, 0x63c40, {{0, 1, 1, 5, 0}, {0, 0, 0, 0, 0}, {.5f, 0, 0, 1, 2}, {0, 1, 1, 5, 0}, {.5f, 0, 0, 1, 2}, {.5f, 1, 1, 5, 1},
                      {.5f, 1, 1, 5, 1}, {.5f, 0, 0, 1, 2}, {1, 0, 0, 2, 2}, {.5f, 1, 1, 5, 1}, {1, 0, 0, 2, 2}, {1, 1, 1, 6, 2}}},
    {6, 2, 0x63c70, {{0, 1, 1, 7, 1}, {0, 0, 0, 4, 1}, {.5f, 0, 0, 4, 0}, {0, 1, 1, 7, 1}, {.5f, 0, 0, 4, 0}, {.5f, 1, 1, 3, 1},
                      {.5f, 1, 1, 3, 1}, {.5f, 0, 0, 4, 0}, {1, 0, 0, 0, 0}, {.5f, 1, 1, 3, 1}, {1, 0, 0, 0, 0}, {1, 1, 1, 3, 2}}},
    {2, 3, 0x63c88, {{0, 1, 1, 0, 0}, {0, 0, 0, 3, 2}, {.5f, 0, 0, 3, 1}, {0, 1, 1, 0, 0}, {.5f, 0, 0, 3, 1}, {.5f, 1, 1, 4, 0},
                      {.5f, 1, 1, 4, 0}, {.5f, 0, 0, 3, 1}, {1, 0, 0, 7, 1}, {.5f, 1, 1, 4, 0}, {1, 0, 0, 7, 1}, {1, 1, 1, 4, 1}}},
    {4, 1, 0x63c58, {{0, 1, 1, 2, 2}, {0, 0, 0, 6, 2}, {.5f, 0, 0, 5, 1}, {0, 1, 1, 2, 2}, {.5f, 0, 0, 5, 1}, {.5f, 1, 1, 1, 2},
                      {.5f, 1, 1, 1, 2}, {.5f, 0, 0, 5, 1}, {1, 0, 0, 5, 0}, {.5f, 1, 1, 1, 2}, {1, 0, 0, 5, 0}, {1, 1, 1, 0, 0}}},
};
VoidThis_t DrawWalls_orig;
void __fastcall DrawWalls_re(void* self, void*) {
    const int row = (ViewIndex() == 0 || ViewIndex() == 2) ? 0 : 1;
    for (int e = 0; e < 4; e++) {
        const WallEdge& edge = WALL_EDGES[e];
        void* nb = Nbr(self, edge.nbr);
        if (At<unsigned char>(self, 0x234 + edge.flag) == 0 || nb == 0)
            continue;
        if (At<int>(self, 0x210 + 4 * edge.flag) == 1)
            glBindTexture_(0x0de1, *reinterpret_cast<unsigned*>(TBytes(0x70304)));
        else
            glBindTexture_(0x0de1, *reinterpret_cast<unsigned*>(TBytes(0x6ff80)));
        glBegin_(4);
        glNormal3fv_(reinterpret_cast<const float*>(TBytes(edge.normalsRva) + row * 12));
        for (int k = 0; k < 12; k++) {
            const WallVertex& p = edge.p[k];
            glTexCoord2f_(p.u, p.v);
            glVertex3fv_(Vert(FaceV(p.which ? nb : self, p.face, p.vtx)));
        }
        glEnd_();
    }
}

// 0x0000ea30  Tile::drawSkirts (VA 0x1000ea30)
// void __thiscall (int depth), `ret 4`. A tile whose type dword (+0x24) is 0x14 returns at once. The normal row is 0 for view
// quadrant 0 and 2 and 1 otherwise; glBindTexture(0x0de1, the wall texture at 0x10070688). Four neighbour blocks follow, each
// taken when the neighbour exists, Tile::getType 0x10001f60 of it is 0x14 and the view quadrant is one of two:
//   n[1] (quadrants 0, 3, normals 0x10063c40)  edge f5.v0 -> f6.v2     then depth > 0: quadrant 0 -> n[3], quadrant 3 -> n[2]
//   n[0] (quadrants 1, 2, normals 0x10063c58)  edge f2.v2 -> f0.v0     then quadrant 1 -> n[3], quadrant 2 -> n[2]
//   n[2] (quadrants 0, 1, normals 0x10063c88)  edge f0.v0 -> f4.v1     then quadrant 0 -> n[0], quadrant 1 -> n[1]
//   n[3] (quadrants 2, 3, normals 0x10063c70)  edge f7.v1 -> f3.v2     then quadrant 2 -> n[1], quadrant 3 -> n[0]
// each block is one GL_TRIANGLES: glNormal3fv(row), then (0,1) glVertex3fv(A), (0,0) glVertex3f(A.x, -75, A.z), (1,0)
// glVertex3f(B.x, -75, B.z), (0,1) glVertex3fv(A), (1,0) glVertex3f(B.x, -75, B.z), (1,1) glVertex3fv(B). The recursion
// calls drawSkirts of that neighbour with depth - 1 (through the function's own address) after glEnd.
typedef void(__fastcall* DrawSkirts_t)(void*, void*, int);
DrawSkirts_t DrawSkirts_orig;
void SkirtQuad(const float* normals, const float* a, const float* b) {
    glBegin_(4);
    glNormal3fv_(normals);
    glTexCoord2f_(0.0f, 1.0f);
    glVertex3fv_(a);
    glTexCoord2f_(0.0f, 0.0f);
    glVertex3f_(a[0], -75.0f, a[2]);
    glTexCoord2f_(1.0f, 0.0f);
    glVertex3f_(b[0], -75.0f, b[2]);
    glTexCoord2f_(0.0f, 1.0f);
    glVertex3fv_(a);
    glTexCoord2f_(1.0f, 0.0f);
    glVertex3f_(b[0], -75.0f, b[2]);
    glTexCoord2f_(1.0f, 1.0f);
    glVertex3fv_(b);
    glEnd_();
}
void __fastcall DrawSkirts_re(void* self, void*, int depth) {
    if (At<int>(self, 0x24) == 0x14)
        return;
    const int q = ViewIndex();
    const int c = (q == 0 || q == 2) ? 0 : 1;
    DrawSkirts_t again = (DrawSkirts_t)TerrainRva(0x0000ea30);
    glBindTexture_(0x0de1, *reinterpret_cast<unsigned*>(TBytes(0x70688)));
    void* n;
    n = Nbr(self, 1);
    if (n && TileGetType(n) == 0x14 && (q == 0 || q == 3)) {
        SkirtQuad(reinterpret_cast<const float*>(TBytes(0x63c40)) + c * 3, Vert(FaceV(self, 5, 0)), Vert(FaceV(self, 6, 2)));
        if (q == 0) {
            if (Nbr(self, 3) && depth > 0) again(Nbr(self, 3), 0, depth - 1);
        } else if (q == 3 && Nbr(self, 2) && depth > 0) {
            again(Nbr(self, 2), 0, depth - 1);
        }
    }
    n = Nbr(self, 0);
    if (n && TileGetType(n) == 0x14 && (q == 1 || q == 2)) {
        SkirtQuad(reinterpret_cast<const float*>(TBytes(0x63c58)) + c * 3, Vert(FaceV(self, 2, 2)), Vert(FaceV(self, 0, 0)));
        if (q == 1) {
            if (Nbr(self, 3) && depth > 0) again(Nbr(self, 3), 0, depth - 1);
        } else if (q == 2 && Nbr(self, 2) && depth > 0) {
            again(Nbr(self, 2), 0, depth - 1);
        }
    }
    n = Nbr(self, 2);
    if (n && TileGetType(n) == 0x14 && (q == 0 || q == 1)) {
        SkirtQuad(reinterpret_cast<const float*>(TBytes(0x63c88)) + c * 3, Vert(FaceV(self, 0, 0)), Vert(FaceV(self, 4, 1)));
        if (q == 0) {
            if (Nbr(self, 0) && depth > 0) again(Nbr(self, 0), 0, depth - 1);
        } else if (q == 1 && Nbr(self, 1) && depth > 0) {
            again(Nbr(self, 1), 0, depth - 1);
        }
    }
    n = Nbr(self, 3);
    if (n && TileGetType(n) == 0x14 && (q == 2 || q == 3)) {
        SkirtQuad(reinterpret_cast<const float*>(TBytes(0x63c70)) + c * 3, Vert(FaceV(self, 7, 1)), Vert(FaceV(self, 3, 2)));
        if (q == 2) {
            if (Nbr(self, 1) && depth > 0) again(Nbr(self, 1), 0, depth - 1);
        } else if (q == 3 && Nbr(self, 0) && depth > 0) {
            again(Nbr(self, 0), 0, depth - 1);
        }
    }
}

// 0x0000e6c0  Tile::render (VA 0x1000e6c0)
// void __thiscall, no stack argument (plain `ret` at 0x1000e97e). `last` ([ebp-8]) is never initialised: it holds the /GZ
// fill 0xcccccccc until the first bind (0x1000e6c0 frame fill), so a texture name of exactly 0xcccccccc is not bound the first
// time. For each face i below the face count dword at +0x44 (0x1000e6f5): when g_viewAngleIndex is non-zero
// (0x1000e6fe) AND Tile::getType 0x10001f60 (thunk 0x1000119a) returns 7 (0x1000e713), the texture is
// g_textures[r + 0x1b][variation at +0x240][slot byte of the face at +0x70 + i * 0x38] where r = (dword +0x28 % 4, the
// remainder of the signed division: `and ecx, 0x80000003 / jns / dec / or 0xfffffffc / inc` at 0x1000e722-0x1000e72e) minus
// the view index, plus 4 when negative (0x1000e73e-0x1000e747); otherwise it is the face's own texture dword (+0x6c + i *
// 0x38). The texture is bound with glBindTexture(0x0de1, tex) only when it differs from `last` (0x1000e792, 0x1000e7c4).
// Then GL_TRIANGLES (4) with three (glTexCoord2fv, glArrayElement) pairs: the texcoord pointer is 0x10063ca0 + uv0 * 0x60 +
// uv1 * 0x20 + view * 8 for the pair at face +0x0c, +0x14, +0x1c (0x1000e815-0x1000e838) and the array index is the face's
// vertex dword +0, +4, +8. After the loop: drawPaths 0x100108f0 (thunk 0x10001122) when the bytes +0x208 and +0x20a are both
// non-zero (0x1000e93b-0x1000e957), drawWalls 0x1000f7f0 (thunk 0x100012b2) always, drawSkirts(3) 0x1000ea30 (thunk 0x1000101e).
VoidThis_t TileRender_orig;
void __fastcall TileRender_re(void* self, void*) {
    unsigned last = 0xcccccccc;
    for (int i = 0; i < At<int>(self, 0x44); i++) {
        char* face = static_cast<char*>(self) + 0x48 + i * 0x38;
        unsigned tex;
        if (ViewIndex() != 0 && TileGetType(self) == 7) {
            int r = At<int>(self, 0x28) % 4;
            r -= ViewIndex();
            r = r < 0 ? r + 4 : r;
            tex = TexName(r + 0x1b, At<int>(self, 0x240), *reinterpret_cast<unsigned char*>(face + 0x28));
        } else {
            tex = *reinterpret_cast<unsigned*>(face + 0x24);
        }
        if (tex != last) {
            glBindTexture_(0x0de1, tex);
            last = tex;
        }
        glBegin_(4);
        const unsigned char* texc = TBytes(0x63ca0);
        for (int k = 0; k < 3; k++) {
            int row = *reinterpret_cast<int*>(face + 0xc + 8 * k), col = *reinterpret_cast<int*>(face + 0x10 + 8 * k);
            glTexCoord2fv_(reinterpret_cast<const float*>(texc + row * 0x60 + col * 0x20 + ViewIndex() * 8));
            glArrayElement_(*reinterpret_cast<int*>(face + 4 * k));
        }
        glEnd_();
    }
    if (At<unsigned char>(self, 0x208) && At<unsigned char>(self, 0x20a))
        ((VoidThis_t)TerrainRva(0x000108f0))(self, 0);
    ((VoidThis_t)TerrainRva(0x0000f7f0))(self, 0);
    ((DrawSkirts_t)TerrainRva(0x0000ea30))(self, 0, 3);
}

// 0x00006850  Terrain::isCulled (VA 0x10006850)
// bool __thiscall (Tile* t, Tile* center, float angle), `ret 0xc`, the bool in al. Zoom shift = dword +0x1c; 2 returns false
// (`cmp dword ptr [eax+0x1c], 2` / `jne` / `xor al, al` at 0x10006870-0x10006878). halfW = (dword +0x20 >> 1) * zoom and halfH =
// (dword +0x24 >> 1) * zoom (arithmetic shifts, 0x10006883-0x1000689e). dy = getY(t) - getY(center), dx = getX(t) - getX(center)
// (0x100068a4-0x100068cc). For the window sizes 800x600 / 1024x768 / 1280x1024 (0x100068d2-0x10006951) the footprints A, B, C, D are
// 0x40,0x28,0x20,0x14 / 0x50,0x34,0x28,0x1a / 0x68,0x44,0x34,0x22; any other size leaves them as the frame fill 0xcccccccc
// ([ebp-0x20], [ebp-0x28], [ebp-0x24], [ebp-0x2c] are never written). The four angle kinds (0.0, 90.0, 180.0 by `fcomp qword` at
// 0x10006972, 0x10006a24, 0x10006ad9, else) compute a screen point
//   sx = w/2 + s1*dy*C + s2*dx*C,  sy = h/2 + s3*dy*D + s4*dx*D - maxHeight(t) * 5     (signs per kind below)
// and return `sx > W + a + halfW || sx < -b - halfW || sy > H + B' + halfH || sy < -B'' - halfH` (the kind's own A / C / B / D terms,
// 0x100069d3-0x10006a07, 0x10006a85-0x10006abc, 0x10006b3a-0x10006b71, 0x10006bdb-0x10006c15). All arithmetic is 32-bit
// `imul` / `add` / `sub` that wraps.
typedef unsigned char(__fastcall* IsCulled_t)(void*, void*, void*, void*, float);
IsCulled_t IsCulled_orig;
unsigned char __fastcall IsCulled_re(void* self, void*, void* t, void* center, float angle) {
    const int zoom = At<int>(self, 0x1c);
    if (zoom == 2)
        return 0;
    const int W = At<int>(self, 0x20), H = At<int>(self, 0x24);
    auto mul = [](int a, int b) { return (int)((unsigned)a * (unsigned)b); };
    auto add = [](int a, int b) { return (int)((unsigned)a + (unsigned)b); };
    auto sub = [](int a, int b) { return (int)((unsigned)a - (unsigned)b); };
    const int halfW = mul(W >> 1, zoom), halfH = mul(H >> 1, zoom);
    const int dy = sub(TileGetY(t), TileGetY(center));
    const int dx = sub(TileGetX(t), TileGetX(center));
    int A = (int)0xcccccccc, B = (int)0xcccccccc, C = (int)0xcccccccc, D = (int)0xcccccccc;
    if (W == 800 && H == 600) { A = 0x40; B = 0x28; C = 0x20; D = 0x14; }
    else if (W == 1024 && H == 768) { A = 0x50; B = 0x34; C = 0x28; D = 0x1a; }
    else if (W == 1280 && H == 1024) { A = 0x68; B = 0x44; C = 0x34; D = 0x22; }
    int sx, sy;
    switch (AngleKind(angle)) {
    case 0:
        sx = add(add(W >> 1, mul(dy, C)), mul(dx, C));
        sy = sub(add(H >> 1, mul(dy, D)), mul(dx, D));
        sy = sub(sy, mul(TileMaxHeight(t), 5));
        return sx > add(W, halfW) || sx < sub(sub(0, A), halfW) || sy > add(add(H, B), halfH) || sy < sub(sub(0, B), halfH);
    case 1:
        sx = add(sub(W >> 1, mul(dy, C)), mul(dx, C));
        sy = sub(sub(H >> 1, mul(dy, D)), mul(dx, D));
        sy = sub(sy, mul(TileMaxHeight(t), 5));
        return sx > add(add(W, C), halfW) || sx < sub(sub(0, A), halfW) || sy > add(add(H, B), halfH) || sy < sub(sub(0, B), halfH);
    case 2:
        sx = sub(sub(W >> 1, mul(dy, C)), mul(dx, C));
        sy = add(sub(H >> 1, mul(dy, D)), mul(dx, D));
        sy = sub(sy, mul(TileMaxHeight(t), 5));
        return sx > add(add(W, A), halfW) || sx < sub(sub(0, C), halfW) || sy > add(add(H, B), halfH) || sy < sub(sub(0, B), halfH);
    default:
        sx = sub(add(W >> 1, mul(dy, C)), mul(dx, C));
        sy = add(add(H >> 1, mul(dy, D)), mul(dx, D));
        sy = sub(sy, mul(TileMaxHeight(t), 5));
        return sx > add(add(W, C), halfW) || sx < sub(sub(0, C), halfW) || sy > add(add(H, D), halfH) ||
               sy < sub(sub(sub(0, B), D), halfH);
    }
}

// 0x000381a0  Terrain::drawTile (VA 0x100381a0)
// void __thiscall (Tile* t, Tile* center, float angle), `ret 0xc`. The draw list is the std::list<Tile*> at this+0x164ac4
// (count at +8, head node pointer at +4; node: next +0, prev +4, tile +8). It is kept sorted back-to-front for the camera angle
// kind (AngleKind) and a tile whose (x, y) is already in the list is not inserted again. With x = t->getX(), y = t->getY()
// read first (0x100381c0-0x100381d8), every kind opens with `list.empty()` -> push_back(t); otherwise it walks from begin():
//   kind 0 (angle 0.0):    advance while x < it.x;    insert before it when it is end or it.x < x; else advance while
//                          y > it.y and it.x == x; return when it is not end and it.y == y and it.x == x; else insert
//   kind 1 (90.0, 0x100383a5): the same with the keys swapped: advance while y < it.y; insert when end or it.y < y; else advance
//                          while x < it.x and it.y == y; return on (it.y == y and it.x == x); else insert
//   kind 2 (180.0, 0x10038562): advance while x > it.x; insert when end or it.x > x; else advance while y < it.y and it.x == x;
//                          return on a match; else insert
//   kind 3 (else):         advance while y > it.y; insert when end or it.y < y (never true for a node the loop stopped at);
//                          else advance while x > it.x and it.y == y; return on a match; else insert
// insert is list::insert 0x1000b100 (position, &t); push_back is 0x1000b090.
typedef void(__fastcall* DrawTile_t)(void*, void*, void*, void*, float);
DrawTile_t DrawTile_orig;
void __fastcall DrawTile_re(void* self, void*, void* t, void* center, float angle) {
    typedef void(__fastcall * PushBack2_t)(void*, void*, void*);
    typedef void*(__fastcall * Insert2_t)(void*, void*, void*, void*, void*);
    const int x = TileGetX(t);
    const int y = TileGetY(t);
    char* list = static_cast<char*>(self) + 0x164ac4;
    void* head = At<void*>(list, 4);
    auto valueAt = [](void* node) { return At<void*>(node, 8); };
    auto push = [&]() { ((PushBack2_t)TerrainRva(0x0000b090))(list, 0, &t); };
    auto insert = [&](void* it) { void* ret; ((Insert2_t)TerrainRva(0x0000b100))(list, 0, &ret, it, &t); };
    const int kind = AngleKind(angle);
    if (At<int>(list, 8) == 0) {
        push();
        return;
    }
    void* it = At<void*>(head, 0);
    switch (kind) {
    case 0:
        while (it != head && x < TileGetX(valueAt(it))) it = At<void*>(it, 0);
        if (it == head || TileGetX(valueAt(it)) < x) { insert(it); return; }
        while (it != head && y > TileGetY(valueAt(it)) && TileGetX(valueAt(it)) == x) it = At<void*>(it, 0);
        break;
    case 1:
        while (it != head && y < TileGetY(valueAt(it))) it = At<void*>(it, 0);
        if (it == head || TileGetY(valueAt(it)) < y) { insert(it); return; }
        while (it != head && x < TileGetX(valueAt(it)) && TileGetY(valueAt(it)) == y) it = At<void*>(it, 0);
        break;
    case 2:
        while (it != head && x > TileGetX(valueAt(it))) it = At<void*>(it, 0);
        if (it == head || TileGetX(valueAt(it)) > x) { insert(it); return; }
        while (it != head && y < TileGetY(valueAt(it)) && TileGetX(valueAt(it)) == x) it = At<void*>(it, 0);
        break;
    default:
        while (it != head && y > TileGetY(valueAt(it))) it = At<void*>(it, 0);
        if (it == head || TileGetY(valueAt(it)) < y) { insert(it); return; }
        while (it != head && x > TileGetX(valueAt(it)) && TileGetY(valueAt(it)) == y) it = At<void*>(it, 0);
        break;
    }
    if (it != head && TileGetY(valueAt(it)) == y && TileGetX(valueAt(it)) == x)
        return;
    insert(it);
}

// 0x00007380  Terrain::drawTileObjects (VA 0x10007380)
// void __thiscall (Tile* t, Tile* center, float angle), `ret 0xc`. x, y = t->getX(), t->getY(); n[0..2] start NULL. By angle kind
// the wall flags are read with Terrain::getWall 0x10001e80 (this, tile, index) and the neighbours looked up with tileAt:
//   kind 0:  getWall(t, 6): n0 = tileAt(x-1, y), n2 = tileAt(x-1, y+1);  getWall(t, 4): n1 = tileAt(x, y+1), n2 = tileAt(x-1, y+1)
//   kind 1:  getWall(t, 6): n0 = (x-1, y), n2 = (x-1, y-1);              getWall(t, 0): n1 = (x, y-1), n2 = (x-1, y-1)
//   kind 2:  getWall(t, 2): n0 = (x+1, y), n2 = (x+1, y-1);              getWall(t, 0): n1 = (x, y-1), n2 = (x+1, y-1)
//   kind 3:  getWall(t, 2): n0 = (x+1, y), n2 = (x+1, y+1);              getWall(t, 4): n1 = (x, y+1), n2 = (x+1, y+1)
// then for i = 0..2: a non-NULL, non-hidden neighbour for which isCulled(n, center, angle) is false is passed to drawTile and
// then drawTileObjects, in that order.
typedef void(__fastcall* DrawTileObjects_t)(void*, void*, void*, void*, float);
DrawTileObjects_t DrawTileObjects_orig;
void __fastcall DrawTileObjects_re(void* self, void*, void* t, void* center, float angle) {
    typedef unsigned char(__fastcall * GetWall_t)(void*, void*, void*, int);
    GetWall_t getWall = (GetWall_t)TerrainRva(0x00001e80);
    TileAt_t tileAt = (TileAt_t)TerrainRva(0x00001d50);
    void* n[3] = {0, 0, 0};
    const int x = TileGetX(t);
    const int y = TileGetY(t);
    switch (AngleKind(angle)) {
    case 0:
        if (getWall(self, 0, t, 6)) { n[0] = tileAt(self, 0, x - 1, y); n[2] = tileAt(self, 0, x - 1, y + 1); }
        if (getWall(self, 0, t, 4)) { n[1] = tileAt(self, 0, x, y + 1); n[2] = tileAt(self, 0, x - 1, y + 1); }
        break;
    case 1:
        if (getWall(self, 0, t, 6)) { n[0] = tileAt(self, 0, x - 1, y); n[2] = tileAt(self, 0, x - 1, y - 1); }
        if (getWall(self, 0, t, 0)) { n[1] = tileAt(self, 0, x, y - 1); n[2] = tileAt(self, 0, x - 1, y - 1); }
        break;
    case 2:
        if (getWall(self, 0, t, 2)) { n[0] = tileAt(self, 0, x + 1, y); n[2] = tileAt(self, 0, x + 1, y - 1); }
        if (getWall(self, 0, t, 0)) { n[1] = tileAt(self, 0, x, y - 1); n[2] = tileAt(self, 0, x + 1, y - 1); }
        break;
    default:
        if (getWall(self, 0, t, 2)) { n[0] = tileAt(self, 0, x + 1, y); n[2] = tileAt(self, 0, x + 1, y + 1); }
        if (getWall(self, 0, t, 4)) { n[1] = tileAt(self, 0, x, y + 1); n[2] = tileAt(self, 0, x + 1, y + 1); }
        break;
    }
    for (int i = 0; i < 3; i++) {
        if (n[i] != 0 && !TileHidden(n[i]) && !((IsCulled_t)TerrainRva(0x00006850))(self, 0, n[i], center, angle)) {
            ((DrawTile_t)TerrainRva(0x000381a0))(self, 0, n[i], center, angle);
            ((DrawTileObjects_t)TerrainRva(0x00007380))(self, 0, n[i], center, angle);
        }
    }
}

// The view setup the three whole-scene drawers share (stripRender 0x100092b0-0x10009350, lowerEdge 0x10038946-0x10038a0d):
// glLoadIdentity(); glPushMatrix(); glRotated(g_screenScale, 1, 0, 0) (the float at 0x10070a10 widened to double);
// glRotatef(45.0f + angle, 0, 1, 0); and, when the pivot condition holds, glTranslatef((25 - pivot.x) * 100.0f, 0, (25 - pivot.y) *
// 100.0f) with the int converted by `fild` and multiplied in double before the float store.
void SceneSetup(void* pivot, bool translate, float angle) {
    glLoadIdentity_();
    glPushMatrix_();
    glRotated_((double)*reinterpret_cast<float*>(TBytes(0x70a10)), 1.0, 0.0, 0.0);
    glRotatef_((float)(45.0 + (double)angle), 0.0f, 1.0f, 0.0f);
    if (translate)
        glTranslatef_((float)((double)(25 - TileGetX(pivot)) * 100.0), 0.0f, (float)((double)(25 - TileGetY(pivot)) * 100.0));
}

// 0x00009270  Terrain::stripRender (VA 0x10009270)
// void __thiscall (Tile* pivot, int dir, float angle), `ret 0xc`. SceneSetup (translate when pivot is non-NULL), then
// Terrain::setViewAngle 0x1000adc0 (angle), then a switch on dir (1..8; others draw nothing):
//   1: stripRender(8) then stripRender(2)   3: (2), (4)   5: (4), (6)   7: (8), (6)       (recursion through this function)
//   2: zoom z = dword +0x1c; for i in [x - ((z << z) + 5), x + 4): for (yy = y - (15 << z), xx = i; yy < y + (3 << z); xx++, yy++)
//   4: for j in [y - ((z << z) + 9), y + 2): for (xx = x + (16 << z), yy = j; xx > x - (4 << z); xx--, yy++)
//   6: for j in [y - ((z << z) + 8), y + 4): for (xx = x - (16 << z), yy = j; xx < x + (3 << z); xx++, yy++)
//   8: for j in [y - 2, y + (z << z) + 12): for (xx = x - (17 << z), yy = j; xx < x + (4 << z); xx++, yy--)
// (x, y = pivot->getX(), pivot->getY(), re-read at every test). Each step takes tile = tileAt(xx, yy) FIRST and then leaves the
// inner loop when the position is out of range on the walking side (2 and 6: yy > 49 || xx > 49; 4: xx < 0 || yy > 49;
// 8: yy < 0 || xx > 49); otherwise a tile inside 0..49 on both axes that is not hidden and for which isCulled is false goes to
// drawTile and then drawTileObjects. After the switch every tile in the draw list (this+0x164ac4) is rendered with
// Tile::render 0x1000e6c0 in list order, the list is cleared (0x1000b210), then glPopMatrix(); glFlush().
typedef void(__fastcall* StripRender_t)(void*, void*, void*, int, float);
StripRender_t StripRender_orig;
void __fastcall StripRender_re(void* self, void*, void* pivot, int dir, float angle) {
    TileAt_t tileAt = (TileAt_t)TerrainRva(0x00001d50);
    StripRender_t again = (StripRender_t)TerrainRva(0x00009270);
    SceneSetup(pivot, pivot != 0, angle);
    ((SetViewAngle_t)TerrainRva(0x0000adc0))(self, 0, angle);
    const unsigned z = (unsigned)At<int>(self, 0x1c) & 31;
    auto zshift = [&](int v) { return (int)((unsigned)v << z); };
    auto visit = [&](void* tile) {
        if (!TileHidden(tile) && !((IsCulled_t)TerrainRva(0x00006850))(self, 0, tile, pivot, angle)) {
            ((DrawTile_t)TerrainRva(0x000381a0))(self, 0, tile, pivot, angle);
            ((DrawTileObjects_t)TerrainRva(0x00007380))(self, 0, tile, pivot, angle);
        }
    };
    switch (dir) {
    case 1: again(self, 0, pivot, 8, angle); again(self, 0, pivot, 2, angle); break;
    case 3: again(self, 0, pivot, 2, angle); again(self, 0, pivot, 4, angle); break;
    case 5: again(self, 0, pivot, 4, angle); again(self, 0, pivot, 6, angle); break;
    case 7: again(self, 0, pivot, 8, angle); again(self, 0, pivot, 6, angle); break;
    case 2: {
        const int n18 = zshift(15), n1c = zshift(3), n20 = zshift((int)z) + 5, n24 = 4;
        for (int i = TileGetX(pivot) - n20; i < TileGetX(pivot) + n24; i++) {
            int xx = i;
            for (int yy = TileGetY(pivot) - n18; yy < TileGetY(pivot) + n1c; xx++, yy++) {
                void* tile = tileAt(self, 0, xx, yy);
                if (yy > 49 || xx > 49) break;
                if (xx >= 0 && yy >= 0) visit(tile);
            }
        }
        break;
    }
    case 4: {
        const int n20 = zshift(4), n24 = zshift(16), n18 = 2, n1c = zshift((int)z) + 9;
        for (int j = TileGetY(pivot) - n1c; j < TileGetY(pivot) + n18; j++) {
            int yy = j;
            for (int xx = TileGetX(pivot) + n24; xx > TileGetX(pivot) - n20; xx--, yy++) {
                void* tile = tileAt(self, 0, xx, yy);
                if (xx < 0 || yy > 49) break;
                if (yy >= 0 && xx <= 49) visit(tile);
            }
        }
        break;
    }
    case 6: {
        const int n20 = zshift(3), n24 = zshift(16), n18 = 4, n1c = zshift((int)z) + 8;
        for (int j = TileGetY(pivot) - n1c; j < TileGetY(pivot) + n18; j++) {
            int yy = j;
            for (int xx = TileGetX(pivot) - n24; xx < TileGetX(pivot) + n20; xx++, yy++) {
                void* tile = tileAt(self, 0, xx, yy);
                if (yy > 49 || xx > 49) break;
                if (xx >= 0 && yy >= 0) visit(tile);
            }
        }
        break;
    }
    case 8: {
        const int n20 = zshift(4), n24 = zshift(17), n18 = zshift((int)z) + 12, n1c = -2;
        for (int j = TileGetY(pivot) + n1c; j < TileGetY(pivot) + n18; j++) {
            int yy = j;
            for (int xx = TileGetX(pivot) - n24; xx < TileGetX(pivot) + n20; xx++, yy--) {
                void* tile = tileAt(self, 0, xx, yy);
                if (yy < 0 || xx > 49) break;
                if (xx <= 49 && yy <= 49 && xx >= 0 && yy >= 0) visit(tile);
            }
        }
        break;
    }
    default: break;
    }
    char* list = static_cast<char*>(self) + 0x164ac4;
    void* head = At<void*>(list, 4);
    for (void* it = At<void*>(head, 0); it != head; it = At<void*>(it, 0))
        ((VoidThis_t)TerrainRva(0x0000e6c0))(At<void*>(it, 8), 0);
    ((ListVoid_t)TerrainRva(0x0000b210))(list, 0);
    glPopMatrix_();
    glFlush_();
}

// 0x00038900  Terrain::lowerEdge (VA 0x10038900)
// void __thiscall (Tile* pTile, Tile* pivot, float angle), `ret 0xc`. A NULL pTile reaches the debug CRT's _assert
// (0x10038921-0x10038938) and is never passed. Then SceneSetup with the translate gated on pTile (0x100389b1) but using the
// pivot's coordinates, Terrain::setViewAngle (0x10038a0d). `draw` is false and becomes true when, for the angle kind, any of
// three positions around pTile (x, y = its coordinates) is missing or hidden (tileAt returns NULL, or Tile::isHidden 0x10015460):
//   kind 0: (x+1, y), (x, y-1), (x+1, y-1)    kind 1 (90.0): (x+1, y), (x, y+1), (x+1, y+1)
//   kind 2 (180.0): (x-1, y), (x, y+1), (x-1, y+1)   kind 3: (x-1, y), (x, y-1), (x-1, y-1)
// When draw: Terrain::light1Off 0x10003830, then glTranslatef(10, 0, -10), pTile->render, glTranslatef(-20, 0, 0), render,
// glTranslatef(0, 0, 20), render, glTranslatef(20, 0, 0), render, glTranslatef(-10, 0, -10), render, glDisable(0x4001),
// glEnable(0x4000). Always: glPopMatrix(); glFlush().
typedef void(__fastcall* LowerEdge_t)(void*, void*, void*, void*, float);
LowerEdge_t LowerEdge_orig;
void __fastcall LowerEdge_re(void* self, void*, void* pTile, void* pivot, float angle) {
    TileAt_t tileAt = (TileAt_t)TerrainRva(0x00001d50);
    SceneSetup(pivot, pTile != 0, angle);
    ((SetViewAngle_t)TerrainRva(0x0000adc0))(self, 0, angle);
    const int x = TileGetX(pTile), y = TileGetY(pTile);
    auto missing = [&](int px, int py) {
        void* t = tileAt(self, 0, px, py);
        return t == 0 || TileHidden(t);
    };
    bool draw = false;
    switch (AngleKind(angle)) {
    case 0: draw = missing(x + 1, y) || missing(x, y - 1) || missing(x + 1, y - 1); break;
    case 1: draw = missing(x + 1, y) || missing(x, y + 1) || missing(x + 1, y + 1); break;
    case 2: draw = missing(x - 1, y) || missing(x, y + 1) || missing(x - 1, y + 1); break;
    default: draw = missing(x - 1, y) || missing(x, y - 1) || missing(x - 1, y - 1); break;
    }
    if (draw) {
        VoidThis_t render = (VoidThis_t)TerrainRva(0x0000e6c0);
        ((VoidThis_t)TerrainRva(0x00003830))(self, 0);
        glTranslatef_(10.0f, 0.0f, -10.0f);
        render(pTile, 0);
        glTranslatef_(-20.0f, 0.0f, 0.0f);
        render(pTile, 0);
        glTranslatef_(0.0f, 0.0f, 20.0f);
        render(pTile, 0);
        glTranslatef_(20.0f, 0.0f, 0.0f);
        render(pTile, 0);
        glTranslatef_(-10.0f, 0.0f, -10.0f);
        render(pTile, 0);
        glDisable_(0x4001);
        glEnable_(0x4000);
    }
    glPopMatrix_();
    glFlush_();
}

// ===================================================================================================
// E. The exported scene drawers and their small neighbours (exe callers through the import slots: 0x004498a0 for
//    render / pathUpdateRender / localRender / setZoomLevel, 0x004493d0 drawLine, 0x00449790 initTerrain, 0x00449520
//    resetTerrain; re/tools/import_callers.py)
// ===================================================================================================
void glMatrixMode__(unsigned m) { glMatrixMode_(m); }
void glVertexPointer_(int size, unsigned type, int stride, const void* p) {
    typedef void(WINAPI * F_t)(int, unsigned, int, const void*);
    ((F_t)GlSlot(0x113608))(size, type, stride, p);                                           // slot 0x10113608
}
void glNormalPointer_(unsigned type, int stride, const void* p) {
    typedef void(WINAPI * F_t)(unsigned, int, const void*);
    ((F_t)GlSlot(0x11360c))(type, stride, p);                                                 // slot 0x1011360c
}
void glLineWidth_(float w) { typedef void(WINAPI * F_t)(float); ((F_t)GlSlot(0x113658))(w); }  // slot 0x10113658
void glColor4f_(float r, float g, float b, float a) { ((GlF4_t)GlSlot(0x11365c))(r, g, b, a); }  // slot 0x1011365c
void glVertex2i_(int x, int y) { typedef void(WINAPI * F_t)(int, int); ((F_t)GlSlot(0x113664))(x, y); }  // slot 0x10113664

// 0x0000a970  Terrain::initTerrain (VA 0x1000a970)
// void __thiscall, no stack argument (plain `ret` at 0x1000a9e5). Terrain::buildArrays 0x1000a130 (thunk 0x10001181 at 0x1000a990),
// then glVertexPointer(3, 0x1406, 0, Terrain::vertexArray 0x100032b0) (thunk 0x100011d6 at 0x1000a998, pushes 0x1000a99f-0x1000a9a7,
// call [0x10113608] at 0x1000a9a9) and glNormalPointer(0x1406, 0, Terrain::normalArray 0x10003270) (thunk 0x10001163 at 0x1000a9b9,
// call [0x1011360c] at 0x1000a9c8). The two array getters return the module addresses of g_vertices and g_normals.
VoidThis_t InitTerrain_orig;
void __fastcall InitTerrain_re(void* self, void*) {
    ((BoolThis_t)TerrainRva(0x0000a130))(self, 0);
    glVertexPointer_(3, 0x1406, 0, ((IosPtr_t)TerrainRva(0x000032b0))(self, 0));
    glNormalPointer_(0x1406, 0, ((IosPtr_t)TerrainRva(0x00003270))(self, 0));
}

// 0x0000aa10  Terrain::resetTerrain (VA 0x1000aa10)
// void __thiscall, no stack argument (plain `ret` at 0x1000aaef). Tile::reset(x, y) 0x1000c2c0 (thunk 0x1000119f, pushes y then x at
// 0x1000aa67-0x1000aa6e) on tile x + y * width of the array at +0x3a4 (stride 0x248) for x in 0..width-1 (outer, `cmp edx, [ecx+0x14]`
// at 0x1000aa45) and y in 0..height-1 (inner, [ecx+0x18] at 0x1000aa62), bounds re-read at every test; then the tail of initTerrain
// (0x1000aa97-0x1000aadf: buildArrays, glVertexPointer, glNormalPointer).
VoidThis_t ResetTerrain_orig;
void __fastcall ResetTerrain_re(void* self, void*) {
    for (int x = 0; x < At<int>(self, 0x14); x++)
        for (int y = 0; y < At<int>(self, 0x18); y++)
            ((TileReset_t)TerrainRva(0x0000c2c0))(static_cast<char*>(self) + 0x3a4 + (x + y * At<int>(self, 0x14)) * 0x248, 0, x, y);
    ((BoolThis_t)TerrainRva(0x0000a130))(self, 0);
    glVertexPointer_(3, 0x1406, 0, ((IosPtr_t)TerrainRva(0x000032b0))(self, 0));
    glNormalPointer_(0x1406, 0, ((IosPtr_t)TerrainRva(0x00003270))(self, 0));
}

// 0x000048a0  Terrain::drawLine (VA 0x100048a0)
// void __thiscall (x1, y1, x2, y2, color, width, alpha), `ret 0x1c`. r, g, b come from the 15-bit colour: ((color >> 7) & 0xf8),
// ((color >> 2) & 0xf8), ((color << 3) & 0xf8), each converted with `fild` and divided by 255.0 (0x1005f200) in double before the float
// store (0x100048bd-0x1000490a). Then glMatrixMode(0x1701), glPushMatrix, glLoadIdentity, glOrtho (the byte +0x28 non-zero: (0, W, 0, H,
// -1, 1), zero: (0, W, H, 0, -1, 1), W and H the dwords +0x20 / +0x24 as doubles, 0x1000493f-0x100049ba), glMatrixMode(0x1700), glPushMatrix,
// glLoadIdentity, glDisable(0xb50), glDisable(0xde1), glEnable(0xbe2), glBlendFunc(0x302, 0x303), glLineWidth((float)width) (0x10004a4e-
// 0x10004a57), glColor4f(r, g, b, alpha / 10.0) (0x1005f214, 0x10004a64-0x10004a7f), glBegin(1), glVertex2i(x1, y1), glVertex2i(x2, y2),
// glEnd, glEnable(0xb50), glEnable(0xde1), glDisable(0xbe2), glPopMatrix, glMatrixMode(0x1701), glPopMatrix, glMatrixMode(0x1700), glFlush().
typedef void(__fastcall* DrawLine_t)(void*, void*, int, int, int, int, int, int, int);
DrawLine_t DrawLine_orig;
void __fastcall DrawLine_re(void* self, void*, int x1, int y1, int x2, int y2, int color, int width, int alpha) {
    const float r = (float)((double)((color >> 7) & 0xf8) / 255.0);
    const float g = (float)((double)((color >> 2) & 0xf8) / 255.0);
    const float b = (float)((double)((color << 3) & 0xf8) / 255.0);
    glMatrixMode_(0x1701);
    glPushMatrix_();
    glLoadIdentity_();
    GlOrtho_t ortho = (GlOrtho_t)GlSlot(0x113650);
    if (At<unsigned char>(self, 0x28) != 0)
        ortho(0.0, (double)At<int>(self, 0x20), 0.0, (double)At<int>(self, 0x24), -1.0, 1.0);
    else
        ortho(0.0, (double)At<int>(self, 0x20), (double)At<int>(self, 0x24), 0.0, -1.0, 1.0);
    glMatrixMode_(0x1700);
    glPushMatrix_();
    glLoadIdentity_();
    glDisable_(0xb50);
    glDisable_(0xde1);
    glEnable_(0xbe2);
    glBlendFunc_(0x302, 0x303);
    glLineWidth_((float)(double)width);
    glColor4f_(r, g, b, (float)((double)alpha / 10.0));
    glBegin_(1);
    glVertex2i_(x1, y1);
    glVertex2i_(x2, y2);
    glEnd_();
    glEnable_(0xb50);
    glEnable_(0xde1);
    glDisable_(0xbe2);
    glPopMatrix_();
    glMatrixMode_(0x1701);
    glPopMatrix_();
    glMatrixMode_(0x1700);
    glFlush_();
}

// 0x00008fc0  Terrain::setZoomLevel (VA 0x10008fc0)
// void __thiscall (int level), `ret 4`. glMatrixMode(0x1701) and glLoadIdentity() first (0x10008fe4, 0x10008ff3). The projection extent
// (w, h) is chosen by the screen size dwords +0x20 / +0x24: 800 x 600 -> bits 0x44dcf88b / 0x44a5ba68 (0x10009018-0x1000901f),
// 1024 x 768 -> 0x44e24630 / 0x44a9b4a4 (0x10009040-0x10009047), 1280 x 1024 -> 0x44d99242 / 0x44ae0e9b (0x10009068-0x1000906f); any
// other size leaves both as the frame fill 0xcccccccc. Then on the level (0x1000907c-0x1000908c): 1 multiplies both by 4.0 (0x1005f3e4,
// 0x100090ce-0x100090e3) and stores zoom 2 at +0x1c; 2 doubles both with `fadd st(0), st(0)` (0x100090b2-0x100090bd) and stores zoom 1; 4
// calls Terrain::resize 0x10009ed0 (thunk 0x100012f8 at 0x100090a1) with the screen size and stores zoom 0; any other level changes
// nothing. glOrtho follows (call [0x10113650]): the byte +0x28 non-zero gives (-w * 0.5, w * 0.5, h * 0.5, -h * 0.5, 5000, -5000), zero
// gives (-w * 0.5, w * 0.5, -h * 0.5, h * 0.5, 5000, -5000) (0.5 at 0x1005f3e0, 0x1000910c-0x1000914c and 0x1000916b-0x100091ab). The
// products are exact in double. Finally glMatrixMode(0x1700) (0x100091bf).
typedef void(__fastcall* SetZoom_t)(void*, void*, int);
SetZoom_t SetZoomLevel_orig;
float FloatFromBits(unsigned u) { float f; memcpy(&f, &u, 4); return f; }
void __fastcall SetZoomLevel_re(void* self, void*, int level) {
    glMatrixMode_(0x1701);
    glLoadIdentity_();
    float w = FloatFromBits(0xccccccccu), h = FloatFromBits(0xccccccccu);
    const int sw = At<int>(self, 0x20), sh = At<int>(self, 0x24);
    if (sw == 800 && sh == 600) { w = FloatFromBits(0x44dcf88bu); h = FloatFromBits(0x44a5ba68u); }
    else if (sw == 1024 && sh == 768) { w = FloatFromBits(0x44e24630u); h = FloatFromBits(0x44a9b4a4u); }
    else if (sw == 1280 && sh == 1024) { w = FloatFromBits(0x44d99242u); h = FloatFromBits(0x44ae0e9bu); }
    if (level == 1) {
        w = w * 4.0f;
        h = h * 4.0f;
        At<int>(self, 0x1c) = 2;
    } else if (level == 2) {
        w = w + w;
        h = h + h;
        At<int>(self, 0x1c) = 1;
    } else if (level == 4) {
        ((Resize_t)TerrainRva(0x00009ed0))(self, 0, sw, sh);
        At<int>(self, 0x1c) = 0;
    }
    GlOrtho_t ortho = (GlOrtho_t)GlSlot(0x113650);
    if (At<unsigned char>(self, 0x28) != 0)
        ortho((double)(-w) * 0.5, (double)w * 0.5, (double)h * 0.5, (double)(-h) * 0.5, 5000.0, -5000.0);
    else
        ortho((double)(-w) * 0.5, (double)w * 0.5, (double)(-h) * 0.5, (double)h * 0.5, 5000.0, -5000.0);
    glMatrixMode_(0x1700);
}

// 0x00005990  Terrain::render (VA 0x10005990)
// bool __thiscall (Tile* pTile, float angle), `ret 8`, the bool in al (`mov al, 1` at the end). up = down = left = right = 16 << zoom
// (dword +0x1c, 0x100059b3-0x100059d9). SceneSetup (translate when pTile is non-NULL, 0x10005a49), Terrain::setViewAngle (0x10005aa5).
// With no pTile (0x10005aaa): y in 10..39 (outer), x from 40 down to 11 (inner), Tile::render of tileAt(x, y) with no test
// (0x10005ab0-0x10005af7). Otherwise the window is clamped to the 50-tile map: up = min(up, y), down = min(down, 50 - y), left = min(left, x),
// right = min(right, 50 - x) (0x10005b00-0x10005b7c), and by angle kind:
//   kind 0: y from Y - up to Y + down - 1 (outer), x from X + right down to X - left (inner)
//   kind 1: x from X + right down to X - left (outer), y from Y + down down to Y - up (inner)
//   kind 2: y from Y + down down to Y - up (outer), x from X - left up to X + right - 1 (inner)
//   kind 3: y from Y - up to Y + down - 1 (outer), x from X - left up to X + right - 1 (inner)
// where each step takes tile = tileAt(x, y) and renders it when it is non-NULL, not hidden and not culled (isCulled(tile, pTile, angle)).
// Then glPopMatrix(); glFlush() (0x10005e59-0x10005e6a).
typedef unsigned char(__fastcall* RenderT_t)(void*, void*, void*, float);
RenderT_t TerrainRender_orig;
unsigned char __fastcall TerrainRender_re(void* self, void*, void* pTile, float angle) {
    TileAt_t tileAt = (TileAt_t)TerrainRva(0x00001d50);
    const unsigned z = (unsigned)At<int>(self, 0x1c) & 31;
    int up = (int)(16u << z), down = up, left = up, right = up;
    SceneSetup(pTile, pTile != 0, angle);
    ((SetViewAngle_t)TerrainRva(0x0000adc0))(self, 0, angle);
    VoidThis_t render = (VoidThis_t)TerrainRva(0x0000e6c0);
    auto visit = [&](int x, int y) {
        void* t = tileAt(self, 0, x, y);
        if (t != 0 && !TileHidden(t) && !((IsCulled_t)TerrainRva(0x00006850))(self, 0, t, pTile, angle))
            render(t, 0);
    };
    if (pTile == 0) {
        for (int y = 10; y < 40; y++)
            for (int x = 40; x > 10; x--)
                render(tileAt(self, 0, x, y), 0);
    } else {
        if (TileGetY(pTile) < up) up = TileGetY(pTile);
        if (50 - TileGetY(pTile) < down) down = 50 - TileGetY(pTile);
        if (TileGetX(pTile) < left) left = TileGetX(pTile);
        if (50 - TileGetX(pTile) < right) right = 50 - TileGetX(pTile);
        switch (AngleKind(angle)) {
        case 0:
            for (int y = TileGetY(pTile) - up; y < TileGetY(pTile) + down; y++)
                for (int x = TileGetX(pTile) + right; x >= TileGetX(pTile) - left; x--) visit(x, y);
            break;
        case 1:
            for (int x = TileGetX(pTile) + right; x >= TileGetX(pTile) - left; x--)
                for (int y = TileGetY(pTile) + down; y >= TileGetY(pTile) - up; y--) visit(x, y);
            break;
        case 2:
            for (int y = TileGetY(pTile) + down; y >= TileGetY(pTile) - up; y--)
                for (int x = TileGetX(pTile) - left; x < TileGetX(pTile) + right; x++) visit(x, y);
            break;
        default:
            for (int y = TileGetY(pTile) - up; y < TileGetY(pTile) + down; y++)
                for (int x = TileGetX(pTile) - left; x < TileGetX(pTile) + right; x++) visit(x, y);
            break;
        }
    }
    glPopMatrix_();
    glFlush_();
    return 1;
}

// 0x000089e0  Terrain::localRender (VA 0x100089e0)
// void __thiscall (Tile* pTile, Tile* pivot, float angle), `ret 0xc`. A NULL pTile reaches the debug CRT's _assert (never passed). SceneSetup
// (translate gated on pTile, pivot's coordinates), Terrain::setViewAngle. A 5 x 5 window around pTile (X, Y) is walked in an order that
// depends on the angle kind:
//   kind 0: y from Y - 2 to Y + 2 (outer), x from X + 2 down to X - 2        kind 1: x from X + 2 down to X - 2 (outer), y from Y + 2 down to Y - 2
//   kind 2: y from Y + 2 down to Y - 2 (outer), x from X - 2 to X + 2        kind 3: y from Y - 2 to Y + 2 (outer), x from X - 2 to X + 2
// For each position tile = tileAt(x, y): a NULL tile is skipped, a hidden one is skipped, any other goes to Terrain::drawTile and then
// Terrain::drawTileObjects with the centre tile pTile. Afterwards every tile in the draw list (+0x164ac4) is rendered in list order, the list
// is cleared (0x1000b210), then glPopMatrix(); glFlush().
typedef void(__fastcall* LocalRender_t)(void*, void*, void*, void*, float);
LocalRender_t LocalRender_orig;
void __fastcall LocalRender_re(void* self, void*, void* pTile, void* pivot, float angle) {
    TileAt_t tileAt = (TileAt_t)TerrainRva(0x00001d50);
    SceneSetup(pivot, pTile != 0, angle);
    ((SetViewAngle_t)TerrainRva(0x0000adc0))(self, 0, angle);
    auto visit = [&](int x, int y) {
        void* t = tileAt(self, 0, x, y);
        if (t != 0 && !TileHidden(t)) {
            ((DrawTile_t)TerrainRva(0x000381a0))(self, 0, t, pTile, angle);
            ((DrawTileObjects_t)TerrainRva(0x00007380))(self, 0, t, pTile, angle);
        }
    };
    switch (AngleKind(angle)) {
    case 0:
        for (int y = TileGetY(pTile) - 2; y <= TileGetY(pTile) + 2; y++)
            for (int x = TileGetX(pTile) + 2; x >= TileGetX(pTile) - 2; x--) visit(x, y);
        break;
    case 1:
        for (int x = TileGetX(pTile) + 2; x >= TileGetX(pTile) - 2; x--)
            for (int y = TileGetY(pTile) + 2; y >= TileGetY(pTile) - 2; y--) visit(x, y);
        break;
    case 2:
        for (int y = TileGetY(pTile) + 2; y >= TileGetY(pTile) - 2; y--)
            for (int x = TileGetX(pTile) - 2; x <= TileGetX(pTile) + 2; x++) visit(x, y);
        break;
    default:
        for (int y = TileGetY(pTile) - 2; y <= TileGetY(pTile) + 2; y++)
            for (int x = TileGetX(pTile) - 2; x <= TileGetX(pTile) + 2; x++) visit(x, y);
        break;
    }
    char* list = static_cast<char*>(self) + 0x164ac4;
    void* head = At<void*>(list, 4);
    for (void* it = At<void*>(head, 0); it != head; it = At<void*>(it, 0))
        ((VoidThis_t)TerrainRva(0x0000e6c0))(At<void*>(it, 8), 0);
    ((ListVoid_t)TerrainRva(0x0000b210))(list, 0);
    glPopMatrix_();
    glFlush_();
}

// 0x00006410  Terrain::pathUpdateRender (VA 0x10006410)
// void __thiscall (Tile* pivot, float angle), `ret 8`. top = bottom = left = right = 15 << zoom. glLoadIdentity, glPushMatrix, glRotated of
// g_screenScale, glRotatef(45 + angle), the translate when pivot is non-NULL (this function does NOT call setViewAngle). Then the window
// is clamped: top = min(top, Y), bottom = min(bottom, 50 - Y), `left = X + 1` when X < left, `right = 49 - X` when 49 - X < right. For y from
// Y - top to Y + bottom - 1 (outer) and x from X + right down to X - left + 1 (inner), tile = tileAt(x, y) (never NULL inside the 50 x 50
// map) is passed to drawTile and then drawTileObjects with the pivot when Tile::hasPath 0x10013320 is true, the tile is not hidden and not
// culled. Afterwards the draw list is rendered in order, cleared, glPopMatrix(); glFlush().
typedef void(__fastcall* PathUpdate_t)(void*, void*, void*, float);
PathUpdate_t PathUpdateRender_orig;
void __fastcall PathUpdateRender_re(void* self, void*, void* pivot, float angle) {
    TileAt_t tileAt = (TileAt_t)TerrainRva(0x00001d50);
    const unsigned z = (unsigned)At<int>(self, 0x1c) & 31;
    int top = (int)(15u << z), bottom = top, left = top, right = top;
    SceneSetup(pivot, pivot != 0, angle);
    if (TileGetY(pivot) < top) top = TileGetY(pivot);
    if (50 - TileGetY(pivot) < bottom) bottom = 50 - TileGetY(pivot);
    if (TileGetX(pivot) < left) left = TileGetX(pivot) + 1;
    if (49 - TileGetX(pivot) < right) right = 49 - TileGetX(pivot);
    for (int y = TileGetY(pivot) - top; y < TileGetY(pivot) + bottom; y++) {
        for (int x = TileGetX(pivot) + right; x > TileGetX(pivot) - left; x--) {
            void* tile = tileAt(self, 0, x, y);
            if (((TileBool_t)TerrainRva(0x00013320))(tile, 0) && !TileHidden(tile) &&
                !((IsCulled_t)TerrainRva(0x00006850))(self, 0, tile, pivot, angle)) {
                ((DrawTile_t)TerrainRva(0x000381a0))(self, 0, tile, pivot, angle);
                ((DrawTileObjects_t)TerrainRva(0x00007380))(self, 0, tile, pivot, angle);
            }
        }
    }
    char* list = static_cast<char*>(self) + 0x164ac4;
    void* head = At<void*>(list, 4);
    for (void* it = At<void*>(head, 0); it != head; it = At<void*>(it, 0))
        ((VoidThis_t)TerrainRva(0x0000e6c0))(At<void*>(it, 8), 0);
    ((ListVoid_t)TerrainRva(0x0000b210))(list, 0);
    glPopMatrix_();
    glFlush_();
}

// ===================================================================================================
// F. Texture file loaders (real files, the module's own CRT)
//
// Terrain.dll links the debug CRT statically; these functions call its fopen 0x10018080, fread 0x10017d70, fclose 0x10017c20, malloc
// 0x100158e0, free 0x10016470, abs 0x100158b0, memcmp 0x10019990, operator new 0x10018c90 and operator delete 0x10018be0 at their
// own addresses. The A/B opens real files written by the fixture (both arms read the same bytes) and runs under the heap stand-in over
// malloc / free / operator new / operator delete, so the blocks they return are the arena's and the releases are recorded.
// ===================================================================================================
typedef void*(__cdecl* Fopen_t)(const char*, const char*);
typedef unsigned(__cdecl* Fread_t)(void*, unsigned, unsigned, void*);
typedef int(__cdecl* Fclose_t)(void*);
typedef void*(__cdecl* Malloc2_t)(unsigned);
typedef void(__cdecl* Free2_t)(void*);
typedef int(__cdecl* Abs_t)(int);
typedef int(__cdecl* Memcmp_t)(const void*, const void*, unsigned);
Fopen_t CrtFopen() { return (Fopen_t)TerrainRva(0x00018080); }
Fread_t CrtFread() { return (Fread_t)TerrainRva(0x00017d70); }
Fclose_t CrtFclose() { return (Fclose_t)TerrainRva(0x00017c20); }
Malloc2_t CrtMalloc() { return (Malloc2_t)TerrainRva(0x000158e0); }
Free2_t CrtFree() { return (Free2_t)TerrainRva(0x00016470); }
Memcmp_t CrtMemcmp() { return (Memcmp_t)TerrainRva(0x00019990); }
Malloc2_t TerrainNew() { return (Malloc2_t)TerrainRva(0x00018c90); }
Free2_t TerrainDelete() { return (Free2_t)TerrainRva(0x00018be0); }
const char* ModeRb() { return reinterpret_cast<const char*>(TerrainRva(0x0005f01c)); }      // "rb" at 0x1005f01c

// 0x000016a0  LoadDIBitmap (VA 0x100016a0)
// GLubyte* __cdecl LoadDIBitmap(const char* filename, BITMAPINFO** info), the OpenGL SuperBible bitmap loader. fopen(filename, "rb")
// (mode string 0x1005f01c; call 0x100016c1): NULL returns 0 (0x100016d0). fread(&header, 0xe, 1, fp) must return 1 or more (cmp eax, 1 / jae at
// 0x100016ed-0x100016f0), else fclose and 0; bfType (word at header+0) must be 0x4d42 (0x10001708-0x10001714). infosize = bfOffBits (header+0xa) - 14
// (0x10001729-0x1000172f); *info = malloc(infosize) (call 0x10001736, the store at 0x10001741 happens before the NULL test at 0x10001746); a NULL
// block closes the file and returns 0. fread(*info, 1, infosize, fp) below infosize (unsigned, jae at 0x10001779) frees *info, closes and returns 0.
// bitsize = (*info)->biSizeImage (+0x14, 0x1000179c-0x100017a4); zero is replaced by ((biWidth * biBitCount + 7) / 8) * abs(biHeight) with a
// signed division by 8 and the CRT abs 0x100158b0 (0x100017ad-0x100017e5). bits = malloc(bitsize) (0x100017ec); NULL frees *info, closes, returns
// 0. fread(bits, 1, bitsize, fp) below bitsize (unsigned, jae at 0x10001834) frees both, closes, returns 0. Otherwise fclose and bits.
typedef unsigned char*(__cdecl* LoadDIBitmap_t)(const char*, void**);
LoadDIBitmap_t LoadDIBitmap_orig;
unsigned char* __cdecl LoadDIBitmap_re(const char* filename, void** info) {
    void* fp = CrtFopen()(filename, ModeRb());
    if (fp == 0)
        return 0;
    unsigned char header[14];
    if (CrtFread()(header, 0xe, 1, fp) < 1) {
        CrtFclose()(fp);
        return 0;
    }
    if (*reinterpret_cast<unsigned short*>(header) != 0x4d42) {
        CrtFclose()(fp);
        return 0;
    }
    int infosize = *reinterpret_cast<int*>(header + 0xa) - 0xe;
    *info = CrtMalloc()((unsigned)infosize);
    if (*info == 0) {
        CrtFclose()(fp);
        return 0;
    }
    if (CrtFread()(*info, 1, (unsigned)infosize, fp) < (unsigned)infosize) {
        CrtFree()(*info);
        CrtFclose()(fp);
        return 0;
    }
    int bitsize = At<int>(*info, 0x14);
    if (bitsize == 0) {
        const int width = At<int>(*info, 4), height = At<int>(*info, 8);
        const int bitcount = At<unsigned short>(*info, 0xe);
        const int prod = (int)((unsigned)width * (unsigned)bitcount);
        const int sum = (int)((unsigned)prod + 7u);
        const int rowBytes = (sum + ((sum >> 31) & 7)) >> 3;                              // cdq / and edx, 7 / add / sar 3
        bitsize = (int)((unsigned)rowBytes * (unsigned)((Abs_t)TerrainRva(0x000158b0))(height));
    }
    unsigned char* bits = static_cast<unsigned char*>(CrtMalloc()((unsigned)bitsize));
    if (bits == 0) {
        CrtFree()(*info);
        CrtFclose()(fp);
        return 0;
    }
    if (CrtFread()(bits, 1, (unsigned)bitsize, fp) < (unsigned)bitsize) {
        CrtFree()(*info);
        CrtFree()(bits);
        CrtFclose()(fp);
        return 0;
    }
    CrtFclose()(fp);
    return bits;
}

// 0x0000be30  LoadTGA (VA 0x1000be30)
// GLuint __cdecl LoadTGA(char* filename), the NeHe uncompressed-TGA loader. The expected 12-byte header is {0,0,2,0,...,0} (0x1000be4e-0x1000be7a).
// fopen(filename, "rb") (0x1000be87); NULL returns 0. fread(compare, 1, 12, fp) (pushes 0xc then 1 at 0x1000bea3-0x1000bea5) must return 0xc
// (cmp eax, 0xc / jne 0x1000bee7 at 0x1000beb3-0x1000beb6), memcmp(expected, compare, 12) (0x1000bec2) must be 0 and fread(header, 1, 6, fp) must
// return 6 (0x1000bee2-0x1000bee5); any failure closes the file and returns 0.
// width = header[1] * 256 + header[0], height = header[3] * 256 + header[2] (0x1000befa-0x1000bf2a); width or height zero (unsigned
// jbe 0x1000bf31 / 0x1000bf37), or a depth byte (header[4]) that is neither 0x18 nor 0x20 (0x1000bf41-0x1000bf4f) closes and returns 0.
// bytesPerPixel = depth / 8 (cdq / and edx, 7 / add / sar 3, 0x1000bf67-0x1000bf75); imageSize = width * height * bpp (two imul, 32-bit wrap,
// 0x1000bf7b-0x1000bf86). imageData = operator new(imageSize) (0x1000bf8d); a NULL block or an fread(imageData, 1, imageSize) different from imageSize
// (0x1000bfba) closes the file, deletes a non-NULL block with operator delete (0x1000bfdb) and returns 0. The loop (0x1000bfea-0x1000c02e) swaps
// byte 0 and byte 2 of every pixel. fclose, glGenTextures(1, &texture) (call [0x101135f8] at 0x1000c044), glBindTexture(0x0de1, texture),
// glTexParameterf(0x0de1, 0x2801, 9729.0f) and (0x0de1, 0x2800, 9729.0f) (bits 0x46180400, 0x1000c06b-0x1000c098), then glTexImage2D(0x0de1, 0, 0x1907,
// w, h, 0, 0x1907, 0x1401, data) when bpp is 3 (0x1000c0a5-0x1000c0d1) or the 0x1908 form otherwise (0x1000c0e0-0x1000c106); the block is deleted
// (0x1000c123) and `texture` is returned (0x1000c12b).
typedef unsigned(__cdecl* LoadTGA_t)(char*);
LoadTGA_t LoadTGA_orig;
unsigned __cdecl LoadTGA_re(char* filename) {
    typedef void(WINAPI * GenTex_t)(int, unsigned*);
    typedef void(WINAPI * TexParamf_t)(unsigned, unsigned, float);
    typedef void(WINAPI * TexImage2D_t)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
    const unsigned char expected[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char compare[12];
    unsigned char header[6];
    void* fp = CrtFopen()(filename, ModeRb());
    if (fp == 0)
        return 0;
    if (CrtFread()(compare, 1, 12, fp) != 12 || CrtMemcmp()(expected, compare, 12) != 0 || CrtFread()(header, 1, 6, fp) != 6) {
        CrtFclose()(fp);
        return 0;
    }
    const unsigned width = (unsigned)header[1] * 256u + header[0];
    const unsigned height = (unsigned)header[3] * 256u + header[2];
    if (width == 0 || height == 0 || (header[4] != 24 && header[4] != 32)) {
        CrtFclose()(fp);
        return 0;
    }
    const int bytesPerPixel = (int)header[4] / 8;
    const int imageSize = (int)(width * height * (unsigned)bytesPerPixel);
    unsigned char* data = static_cast<unsigned char*>(TerrainNew()((unsigned)imageSize));
    if (data == 0 || CrtFread()(data, 1, (unsigned)imageSize, fp) != (unsigned)imageSize) {
        CrtFclose()(fp);
        if (data != 0)
            TerrainDelete()(data);
        return 0;
    }
    for (int i = 0; i < imageSize; i += bytesPerPixel) {
        unsigned temp = data[i];
        data[i] = data[i + 2];
        data[i + 2] = (unsigned char)temp;
    }
    CrtFclose()(fp);
    unsigned texture;
    ((GenTex_t)GlSlot(0x1135f8))(1, &texture);
    glBindTexture_(0x0de1, texture);
    ((TexParamf_t)GlSlot(0x1135f4))(0x0de1, 0x2801, FloatFromBits(0x46180400u));
    ((TexParamf_t)GlSlot(0x1135f4))(0x0de1, 0x2800, FloatFromBits(0x46180400u));
    TexImage2D_t teximage = (TexImage2D_t)GlSlot(0x113604);
    if (bytesPerPixel == 3)
        teximage(0x0de1, 0, 0x1907, (int)width, (int)height, 0, 0x1907, 0x1401, data);
    else
        teximage(0x0de1, 0, 0x1908, (int)width, (int)height, 0, 0x1908, 0x1401, data);
    TerrainDelete()(data);
    return texture;
}

// 0x0000bbd0  textureLoad (VA 0x1000bbd0)
// GLuint __cdecl textureLoad(char* filename, GLboolean alpha, GLenum minfilter, GLenum magfilter, GLenum wrap): the SuperBible texture loader. The
// bitmap comes from LoadDIBitmap 0x100016a0 (thunk 0x100012df at 0x1000bbf0, call with (filename, &info)); NULL returns 0 (0x1000bbff). swapRedBlue
// 0x10001b60 (thunk 0x10001109, called with (bits, info) at 0x1000bc10) swaps the colour order. The target is 0x0de0 (1D) when the bitmap's height
// dword (+8) is 1 and 0x0de1 otherwise (0x1000bc1b-0x1000bc2a). glGenTextures(1, &texture), glBindTexture(target, texture), glTexParameteri(target, 0x2800,
// 0x2600), (target, 0x2801, 0x2601), (target, 0x2802, wrap), (target, 0x2803, wrap) and glTexEnvi(target, 0x2200, 0x1e01) (0x1000bc33-0x1000bcdf). When
// minfilter is 0x2601 or 0x2600 (cmp [ebp+0x10] at 0x1000bcec / 0x1000bcf5): glTexImage2D(target, 0, 3, width (+4), height (+8), 0, 0x1907, 0x1401,
// bits) (0x1000bd00-0x1000bd26). Otherwise a 1D target calls gluBuild1DMipmaps(target, 3, width, 0x1908, 0x1401, bits) (through the import thunk
// 0x10015784, 0x1000bd3e-0x1000bd59) and a 2D target gluBuild2DMipmaps(target, 3, width, height, 0x1907, 0x1401, bits) (thunk 0x1001577e,
// 0x1000bd60-0x1000bd82). Then free(info) and free(bits) (0x1000bd8b, 0x1000bd97) and `texture` is returned.
typedef unsigned(__cdecl* TextureLoad_t)(char*, unsigned char, unsigned, unsigned, unsigned);
TextureLoad_t TextureLoad_orig;
unsigned __cdecl TextureLoad_re(char* filename, unsigned char, unsigned minfilter, unsigned, unsigned wrap) {
    typedef void(WINAPI * GenTex_t)(int, unsigned*);
    typedef void(WINAPI * TexParami_t)(unsigned, unsigned, int);
    typedef void(WINAPI * TexEnvi_t)(unsigned, unsigned, int);
    typedef void(WINAPI * TexImage2D_t)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
    typedef int(WINAPI * Build1D_t)(unsigned, int, int, unsigned, unsigned, const void*);
    typedef int(WINAPI * Build2D_t)(unsigned, int, int, int, unsigned, unsigned, const void*);
    void* info = 0;
    unsigned char* bits = ((LoadDIBitmap_t)TerrainRva(0x000016a0))(filename, &info);
    if (bits == 0)
        return 0;
    ((void(__cdecl*)(void*, void*))TerrainRva(0x00001b60))(bits, info);
    const unsigned target = At<int>(info, 8) == 1 ? 0x0de0 : 0x0de1;
    unsigned texture;
    ((GenTex_t)GlSlot(0x1135f8))(1, &texture);
    glBindTexture_(target, texture);
    TexParami_t texparami = (TexParami_t)GlSlot(0x1135fc);
    texparami(target, 0x2800, 0x2600);
    texparami(target, 0x2801, 0x2601);
    texparami(target, 0x2802, (int)wrap);
    texparami(target, 0x2803, (int)wrap);
    ((TexEnvi_t)GlSlot(0x113600))(target, 0x2200, 0x1e01);
    if (minfilter == 0x2601 || minfilter == 0x2600)
        ((TexImage2D_t)GlSlot(0x113604))(target, 0, 3, At<int>(info, 4), At<int>(info, 8), 0, 0x1907, 0x1401, bits);
    else if (target == 0x0de0)
        ((Build1D_t)*reinterpret_cast<void**>(TerrainRva(0x001133f8)))(target, 3, At<int>(info, 4), 0x1908, 0x1401, bits);
    else
        ((Build2D_t)*reinterpret_cast<void**>(TerrainRva(0x001133f4)))(target, 3, At<int>(info, 4), At<int>(info, 8), 0x1907, 0x1401, bits);
    CrtFree()(info);
    CrtFree()(bits);
    return texture;
}

// ===================================================================================================
// H. Terrain's collar table, the GL context shutdown and the destructor
// ===================================================================================================

// 0x0000a880  Terrain::passCollarInfo (VA 0x1000a880)
// void __thiscall (int* info, int n), `ret 8` (info [ebp+8], n [ebp+0xc]; `this` unused). g_collarInfo 0x10106b48: when it is NULL (cmp at
// 0x1000a89d / jne 0x1000a8c3) a new table of n << 2 bytes is made with operator new 0x10018c90 (shl eax, 2 at 0x1000a8a9, call 0x1000a8ad) and stored
// at 0x1000a8bb; otherwise the old table is released with operator delete 0x10018be0 (call 0x1000a8d0) and a new one made and stored (0x1000a8df,
// 0x1000a8ed). Then for i in 0..n-1 (signed `jge` at 0x1000a90b) g_collarInfo[i] = info[i] (0x1000a90d-0x1000a91e). The caller (exe 0x00449400,
// 0x00449790) passes the collar array of the course.
typedef void(__fastcall* PassCollar_t)(void*, void*, int*, int);
PassCollar_t PassCollar_orig;
void __fastcall PassCollar_re(void*, void*, int* info, int n) {
    int** g = reinterpret_cast<int**>(TerrainRva(0x00106b48));
    if (*g == 0) {
        *g = static_cast<int*>(TerrainNew()((unsigned)n << 2));
    } else {
        TerrainDelete()(*g);
        *g = static_cast<int*>(TerrainNew()((unsigned)n << 2));
    }
    for (int i = 0; i < n; i++)
        (*g)[i] = info[i];
}

// 0x00009ba0  Terrain::closeSystem (VA 0x10009ba0)
// void __thiscall, no stack argument (plain `ret` at 0x10009c52). The GL context handle is the dword at this+0 (cmp dword ptr [eax], 0 / jne at
// 0x10009bc0-0x10009bc3): zero returns at once (jmp 0x10009c42). Otherwise wglMakeCurrent(0, 0) (call [0x10113618] at 0x10009bcd); a zero result
// shows MessageBoxA(0, 0x1005f404, 0x1005f420, 0x40) (call [0x101136d8] at 0x10009bee). wglDeleteContext(this[0]) (call [0x1011361c] at 0x10009c03);
// a zero result shows MessageBoxA(0, 0x1005f3e8, 0x1005f420, 0x40) (0x10009c24). The handle is zeroed (0x10009c34) and Terrain::reloadTextures
// 0x100380a0 runs (thunk 0x100012ee at 0x10009c3d). Caller: exe 0x00449860.
VoidThis_t CloseSystem_orig;
void __fastcall CloseSystem_re(void* self, void*) {
    typedef int(WINAPI * WglMakeCurrent_t)(void*, void*);
    typedef int(WINAPI * WglDeleteContext_t)(void*);
    typedef int(WINAPI * MessageBoxA_t)(void*, const char*, const char*, unsigned);
    if (At<void*>(self, 0) == 0)
        return;
    if (!((WglMakeCurrent_t)GlSlot(0x113618))(0, 0))
        ((MessageBoxA_t)GlSlot(0x1136d8))(0, reinterpret_cast<const char*>(TerrainRva(0x0005f404)), reinterpret_cast<const char*>(TerrainRva(0x0005f420)), 0x40);
    if (!((WglDeleteContext_t)GlSlot(0x11361c))(At<void*>(self, 0)))
        ((MessageBoxA_t)GlSlot(0x1136d8))(0, reinterpret_cast<const char*>(TerrainRva(0x0005f3e8)), reinterpret_cast<const char*>(TerrainRva(0x0005f420)), 0x40);
    At<void*>(self, 0) = 0;
    ((VoidThis_t)TerrainRva(0x000380a0))(self, 0);
}

// 0x00003090  Terrain::~Terrain (VA 0x10003090)
// Destructor of the 0x164ad0-byte Terrain object (plain `ret` at 0x10003171, an SEH frame with handler 0x1004112d). Width (+0x14) and height (+0x18)
// are set to -1 (0x100030cc, 0x100030d6). A non-NULL g_collarInfo 0x10106b48 (cmp at 0x100030dd) is released with operator delete 0x10018be0 (call
// 0x100030f3) and zeroed (0x100030fb); a non-zero dword 0x10106b4c (cmp at 0x10003105) is zeroed (0x1000310e). Terrain::reloadTextures
// (thunk 0x100012ee at 0x1000311b), then the draw list at +0x164ac4 is destroyed with std::list<Tile*>::~list 0x1000aed0 (thunk 0x10001280 at
// 0x1000312d), then the CRT vector destructor 0x10018420 runs Tile::~Tile (thunk 0x1000100a pushed at 0x10003139) over the 0x9c4 tiles of 0x248 bytes
// at +0x3a4 (pushes 0x9c4 and 0x248 at 0x1000313e-0x10003143, call at 0x10003152; the callee removes its own 0x10 argument bytes: no `add esp`
// follows the call, so it is called as __stdcall). Callers: the exe through the import slot 0x4ba178 (0x00449860)
// and an atexit stub.
VoidThis_t TerrainDtor_orig;
void __fastcall TerrainDtor_re(void* self, void*) {
    At<int>(self, 0x14) = -1;
    At<int>(self, 0x18) = -1;
    int** gc = reinterpret_cast<int**>(TerrainRva(0x00106b48));
    if (*gc != 0) {
        TerrainDelete()(*gc);
        *gc = 0;
    }
    int* g4c = reinterpret_cast<int*>(TerrainRva(0x00106b4c));
    if (*g4c != 0)
        *g4c = 0;
    ((VoidThis_t)TerrainRva(0x000380a0))(self, 0);
    ((ListVoid_t)TerrainRva(0x0000aed0))(static_cast<char*>(self) + 0x164ac4, 0);
    ((void(__stdcall*)(void*, unsigned, unsigned, void*))TerrainRva(0x00018420))(static_cast<char*>(self) + 0x3a4, 0x248, 0x9c4, TerrainRva(0x0000100a));
}

// ===================================================================================================
// I. The lighting file: istream::getline, Terrain::loadLighting, rebuild, initLists
//
// loadLighting builds an old-iostream ifstream on the stack and reads the course lighting file through it. The reimplementations call the module's
// own stream code at its addresses: ifstream::ifstream 0x10033980, fstreambase::close 0x10033e90, ifstream::{vbase dtor} 0x100046a0 (thunk
// 0x10001019) and the real line reader 0x100348a0; every call to ios::fail / getline goes through the hooked wrappers.
// ===================================================================================================

// 0x00010590  istream::getline (VA 0x10010590)
// istream* __thiscall getline(char* buf, int len, char delim), `ret 0xc` (buf [ebp+8], len [ebp+0xc], delim [ebp+0x10]). The ios virtual base is
// this + the second dword of the vbtable (mov eax, [ebp-4] / mov ecx, [eax] / add edx, [ecx+4] at 0x100105ad-0x100105b5); ios::lock 0x100018b0
// (thunk 0x1000127b at 0x100105b9) runs on it, the dword at this+4 is incremented (0x100105c4-0x100105cd), the line reader 0x100348a0 runs with
// (buf, len, delim & 0xff) and ecx = this (0x100105d0-0x100105e4), ios::unlock 0x10037930 (thunk 0x10001172 at 0x100105f5) runs on the same
// base, and `this` is returned (0x100105fd). No branch.
typedef void*(__fastcall* Getline_t)(void*, void*, char*, int, int);
Getline_t Getline_orig;
void* __fastcall Getline_re(void* self, void*, char* buf, int len, int delim) {
    void* vbtable = At<void*>(self, 0);
    void* ios = static_cast<char*>(self) + At<int>(vbtable, 4);
    ((IosStatic_t)TerrainRva(0x000018b0))(ios);
    At<int>(self, 4) += 1;
    typedef void(__fastcall * Line_t)(void*, void*, char*, int, int);
    ((Line_t)TerrainRva(0x000348a0))(self, 0, buf, len, delim & 0xff);
    ((IosStatic_t)TerrainRva(0x00037930))(ios);
    return self;
}

// 0x000046a0  ifstream::{vbase dtor} (VA 0x100046a0)
// void __thiscall, no stack argument (plain `ret` at 0x100046e3). Two calls on `this` + 0xc (add ecx, 0xc at 0x100046c0 and 0x100046cb): the
// fstreambase destructor 0x10033cd0 (call 0x100046c3) and the ios destructor 0x10034cd0 (call 0x100046ce). Callers: loadLighting 0x10006dd0 (thunk
// 0x10001019 at 0x100072cc and 0x10006e4e) and two atexit-style stubs (hooks.csv: 0x10033940, 0x10041170).
VoidThis_t IfstreamDtor_orig;
void __fastcall IfstreamDtor_re(void* self, void*) {
    ((VoidThis_t)TerrainRva(0x00033cd0))(static_cast<char*>(self) + 0xc, 0);
    ((VoidThis_t)TerrainRva(0x00034cd0))(static_cast<char*>(self) + 0xc, 0);
}

// 0x00006dd0  Terrain::loadLighting (VA 0x10006dd0)
// void __thiscall (const char* file), `ret 4`, with an SEH frame (handler 0x1004117c). An ifstream (stack object at [ebp-0x190]) is built with
// ifstream::ifstream 0x10033980 (name, mode 0x20 = ios::nocreate, protection = the dword 0x100612c4, 1) (pushes 0x10006e08-0x10006e15, call
// 0x10006e1c). ios::fail (thunk 0x1000112c, on the ios base found through the vbtable, 0x10006e28-0x10006e38) non-zero destroys the stream and
// returns (0x10006e3d-0x10006e53). Otherwise: getline(buf, 0x100, 0xa) (0x10006e58-0x10006e6c) and `while (strcmp(strtok(buf, " "), "#AMBIENT")
// != 0) getline` (strtok 0x10019000, strcmp 0x10018f70, markers 0x1005f330 / 0x1005f320 / 0x1005f1f0, delimiter 0x1005f20c; 0x10006e71-0x10006eb0);
// one more line, then three numbers: ambient[k] = (float)((double)atoi(strtok(k ? NULL : buf, " ")) / 255.0) with atoi 0x10018e20 and 255.0 at
// 0x1005f200 (0x10006ecb-0x10006f54); getline, the same marker loop for "#DIFFUSE" (0x10006f70-0x10006fb1) and three numbers into diffuse
// (0x10006fca-0x10007056); getline, the loop for "#SPECULAR" (0x1000706f-0x100070ae) and three numbers into specular (0x100070c9-0x10007152). The
// light position {-0.5, 0.1, -1, 0} (0xbf000000, 0x3dcccccd, 0xbf800000, 0) is rotated with rotateAxis 0x10003a50 (thunk 0x100010f0) by 40 degrees about
// (1, 0, 0) and 45 degrees about (0, 1, 0) (0x1000717d-0x100071b4) and normalised with 0x10037c80 (0x100071be). Then glLightfv(0x4000, 0x1203,
// position), glLightfv(0x4000, 0x1200, ambient), (0x4000, 0x1201, diffuse), (0x4000, 0x1202, specular) (call [0x10113684] at 0x10007201,
// 0x1000721e, 0x1000723b, 0x10007258), glEnable(0x4000), glMaterialfv(0x404, 0x1202, {1,1,1,1}) (call [0x10113678] at 0x1000728c) and
// glMateriali(0x404, 0x1601, 13) (call [0x1011367c] at 0x100072a7). fstreambase::close 0x10033e90 (0x100072ba), then the stream is destroyed with
// ifstream::{vbase dtor} 0x100046a0 (thunk 0x10001019 at 0x100072cc). The three colour arrays are float[3] but glLightfv reads four floats: they
// lie one after another on the stack (specular, diffuse, ambient at [ebp-0x34], [ebp-0x28], [ebp-0x1c]) followed by the saved `this` at
// [ebp-0x10], which is what the fourth float of ambient is; diffuse's is ambient[0] and specular's is diffuse[0]. The struct below keeps that layout.
typedef void(__fastcall* LoadLighting_t)(void*, void*, const char*);
LoadLighting_t LoadLighting_orig;
struct LightColours { float spec[3]; float diff[3]; float amb[3]; void* self; };
void __fastcall LoadLighting_re(void* self, void*, const char* file) {
    typedef void*(__fastcall * IfstreamCtor_t)(void*, void*, const char*, int, int, int);
    typedef void(__fastcall * IfstreamClose_t)(void*, void*);
    typedef char*(__cdecl * Strtok_t)(char*, const char*);
    typedef int(__cdecl * Strcmp_t)(const char*, const char*);
    typedef int(__cdecl * Atoi_t)(const char*);
    typedef void(__cdecl * Rotate_t)(float, float, float, float, float*);
    typedef void(__cdecl * Normalize_t)(float*);
    typedef void(WINAPI * Matf_t)(unsigned, unsigned, const float*);
    typedef void(WINAPI * Mati_t)(unsigned, unsigned, int);
    Strtok_t strtok_ = (Strtok_t)TerrainRva(0x00019000);
    Strcmp_t strcmp_ = (Strcmp_t)TerrainRva(0x00018f70);
    Atoi_t atoi_ = (Atoi_t)TerrainRva(0x00018e20);
    const char* delim = reinterpret_cast<const char*>(TerrainRva(0x0005f20c));
    // storage for the stream object; the constructor initialises its extent (a stack object of fewer than 0x5c bytes in the original frame)
    alignas(8) unsigned char stream[0x200];
    char buf[0x100];
    LightColours c;
    c.self = self;
    memset(&c, 0, sizeof(float) * 9);
    ((IfstreamCtor_t)TerrainRva(0x00033980))(stream, 0, file, 0x20, *reinterpret_cast<int*>(TerrainRva(0x000612c4)), 1);
    void* vbtable = At<void*>(stream, 0);
    void* ios = stream + At<int>(vbtable, 4);
    if (((IosThis_t)TerrainRva(0x00002510))(ios, 0) != 0) {
        ((VoidThis_t)TerrainRva(0x000046a0))(stream, 0);
        return;
    }
    auto line = [&]() { ((Getline_t)TerrainRva(0x00010590))(stream, 0, buf, 0x100, 0xa); };
    auto skipTo = [&](unsigned markerRva) {
        while (strcmp_(strtok_(buf, delim), reinterpret_cast<const char*>(TerrainRva(markerRva))) != 0)
            line();
    };
    auto num = [&](bool first) { return (float)((double)atoi_(strtok_(first ? buf : 0, delim)) / 255.0); };
    line();
    skipTo(0x0005f330);                                  // "#AMBIENT"
    line();
    c.amb[0] = num(true);
    c.amb[1] = num(false);
    c.amb[2] = num(false);
    line();
    skipTo(0x0005f320);                                  // "#DIFFUSE"
    line();
    c.diff[0] = num(true);
    c.diff[1] = num(false);
    c.diff[2] = num(false);
    line();
    skipTo(0x0005f1f0);                                  // "#SPECULAR"
    line();
    c.spec[0] = num(true);
    c.spec[1] = num(false);
    c.spec[2] = num(false);
    float pos[4] = {FloatFromBits(0xbf000000u), FloatFromBits(0x3dcccccdu), FloatFromBits(0xbf800000u), 0.0f};
    ((Rotate_t)TerrainRva(0x00003a50))(FloatFromBits(0x42200000u), FloatFromBits(0x3f800000u), 0.0f, 0.0f, pos);
    ((Rotate_t)TerrainRva(0x00003a50))(FloatFromBits(0x42340000u), 0.0f, FloatFromBits(0x3f800000u), 0.0f, pos);
    ((Normalize_t)TerrainRva(0x00037c80))(pos);
    float matSpecular[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv_(0x4000, 0x1203, pos);
    glLightfv_(0x4000, 0x1200, c.amb);
    glLightfv_(0x4000, 0x1201, c.diff);
    glLightfv_(0x4000, 0x1202, c.spec);
    glEnable_(0x4000);
    ((Matf_t)GlSlot(0x113678))(0x404, 0x1202, matSpecular);
    ((Mati_t)GlSlot(0x11367c))(0x404, 0x1601, 13);
    ((IfstreamClose_t)TerrainRva(0x00033e90))(stream, 0);
    ((VoidThis_t)TerrainRva(0x000046a0))(stream, 0);
}

// 0x00003980  Terrain::rebuild (VA 0x10003980)
// void __thiscall, no stack argument (plain `ret` at 0x10003a0e). The lighting file depends on g_courseType 0x10070a0c, tested UNSIGNED (cmp [ebp-8], 3
// / ja 0x100039f1 at 0x100039a5-0x100039a9) and dispatched through a four-entry jump table at 0x10003a0f: 0 -> "ParklandLighting.txt" (0x1005f1c4,
// push at 0x100039b5), 1 -> "DesertLighting.txt" (0x1005f190, 0x100039d3), 2 -> "TropicalLighting.txt" (0x1005f1a8, 0x100039c4), 3 ->
// "LinksLighting.txt" (0x1005f178, 0x100039e2), anything else -> "lighting.txt" (0x1005f168, 0x100039f1). The name goes to Terrain::loadLighting
// 0x10006dd0 (thunk 0x100012fd) on `this`. Callers (hooks.csv): Terrain::initLists 0x100037e0 and loadNewCourseType 0x10001af0.
VoidThis_t Rebuild_orig;
void __fastcall Rebuild_re(void* self, void*) {
    const unsigned type = *reinterpret_cast<unsigned*>(TerrainRva(0x00070a0c));
    unsigned nameRva;
    switch (type) {
    case 0: nameRva = 0x0005f1c4; break;
    case 1: nameRva = 0x0005f190; break;
    case 2: nameRva = 0x0005f1a8; break;
    case 3: nameRva = 0x0005f178; break;
    default: nameRva = 0x0005f168; break;
    }
    ((LoadLighting_t)TerrainRva(0x00006dd0))(self, 0, reinterpret_cast<const char*>(TerrainRva(nameRva)));
}

// 0x000076e0  Terrain::relight (VA 0x100076e0)
// void __thiscall, no stack argument (plain `ret`), the largest frame of the batch (0x27c bytes). Loads every texture of the course from files.
// The base path buffer is "./Data/Textures/" (copied from 0x100613cc and zero-filled to 0x104 bytes, 0x10007703-0x10007744). When the loaded flag (byte
// this+0x29, 0x10007768) is set, Terrain::reloadTextures 0x100380a0 (thunk 0x100012ee at 0x10007772) runs first. g_courseType 0x10070a0c is tested UNSIGNED
// (cmp [ebp-0x234], 3 / ja 0x100077f1 at 0x10007783-0x1000778a) and dispatched through a jump table at 0x10007fd9: 0 -> "Parkland/" (0x100613c0, push at
// 0x10007799), 1 -> "Desert/" (0x1005f308, 0x100077c5), 2 -> "Tropical/" (0x1005f314, 0x100077af), 3 -> "Links/" (0x1005f300, 0x100077db), anything else ->
// "Parkland/" (0x100077f1); the name is appended to the base (strcat 0x10018590). Then for each of the 0x1f types i (cmp [ebp-0x21c], 0x1f at 0x10007820):
//   i = 0 (cmp 0 at 0x1000782d): path = base + this->m_types[0].name (the name starts at this+0x2c, stride 0x18) + "A0001.bmp" (0x1005f2f4); for digit c1 '1'..'9'
//     (cmp 0x3a at 0x1000789f) the character at strlen(path) - 5 becomes c1 and textureLoad 0x1000bbd0 (thunk 0x10001177; args path, 0, 0x2601, 0x2601,
//     0x2901 pushed at 0x100078c4-0x100078cf) result goes to setTextureId 0x10012e70 (thunk 0x1000132f) as (0, count, 0, tex) with count = the local counter,
//     incremented after each (0x10007917); then for c2 '1'..'2' (cmp 0x33 at 0x1000793f) the character at strlen - 6 becomes c2 and for c3 '0'..'9' (cmp
//     0x3a at 0x10007983) a counter above 0x18 skips the step (cmp [ebp-0x218], 0x18 at 0x10007990), otherwise the character at strlen - 5 becomes c3 and
//     the same textureLoad / setTextureId pair runs (0x100079b7-0x10007a0a);
//   i > 0: for letter 'A'..'E' (cmp 0x46 at 0x10007a54) path = base + name + letter + "0001.bmp" (0x1005f2e8), textureLoad; a non-zero result goes to
//     setTextureId (i, this->m_types[i].count, 0, tex), the digits '2'..'9' replace the last digit (strlen - 5, cmp 0x3a at 0x10007b58) and each
//     textureLoad result goes to (i, count, d - '1', tex), then m_types[i].count (dword this + 0x40 + i * 0x18) is incremented; a zero result leaves the
//     letter loop.
// Then the path textures: LoadTGA 0x1000be30 (thunk 0x10001195) of base + "/Path.tga", "/PathCap.tga", "/PathInside.tga", "/PathCurve.tga" go to
// setTextureId (0x20, 0, 0..3, tex) (0x10007c20-0x10007d22) and m_types[0x20].count is incremented; "/PathX.tga", "/PathCapX.tga", "/PathInsideX.tga",
// "/PathCurveX.tga" go to (0x21, 0, 0..3) and m_types[0x21].count++ (0x10007d4d-0x10007e57); textureLoad of "/CliffTest.bmp" goes to (0x23, 0, 0)
// and count 0x23 is incremented (0x10007e82-0x10007ec5), "/RetainWallA.bmp" to (0x22, 0, 0) (0x10007ef0-0x10007f33) and "/strata.bmp" to (0x24, 0, 0)
// (0x10007f5e-0x10007fa1), each followed by its count increment. Finally the loaded flag is set. Callers (hooks.csv): Terrain::Terrain 0x10002ae0,
// initSystem 0x10009c80, loadNewCourseType 0x10001af0.
VoidThis_t Relight_orig;
void __fastcall Relight_re(void* self, void*) {
    typedef unsigned(__cdecl * TextureLoadFn_t)(char*, unsigned char, unsigned, unsigned, unsigned);
    typedef unsigned(__cdecl * LoadTgaFn_t)(char*);
    typedef void(__cdecl * SetTextureIdFn_t)(int, int, int, unsigned);
    TextureLoadFn_t tload = (TextureLoadFn_t)TerrainRva(0x0000bbd0);
    LoadTgaFn_t tga = (LoadTgaFn_t)TerrainRva(0x0000be30);
    SetTextureIdFn_t setTex = (SetTextureIdFn_t)TerrainRva(0x00012e70);
    char base[0x104];
    char path[0x104];
    memset(base, 0, sizeof(base));
    memset(path, 0, sizeof(path));
    strcpy(base, "./Data/Textures/");
    unsigned tex = 0;
    int count = 0;
    if (At<unsigned char>(self, 0x29) != 0)
        ((VoidThis_t)TerrainRva(0x000380a0))(self, 0);
    switch (*reinterpret_cast<unsigned*>(TerrainRva(0x00070a0c))) {
    case 0: strcat(base, "Parkland/"); break;
    case 2: strcat(base, "Tropical/"); break;
    case 1: strcat(base, "Desert/"); break;
    case 3: strcat(base, "Links/"); break;
    default: strcat(base, "Parkland/"); break;
    }
    auto name = [&](int i) { return static_cast<char*>(self) + 0x2c + i * 0x18; };
    auto typeCount = [&](int i) -> int& { return At<int>(self, 0x40 + i * 0x18); };
    for (int i = 0; i < 0x1f; i++) {
        if (i == 0) {
            strcpy(path, base);
            strcat(path, name(i));
            strcat(path, "A0001.bmp");
            for (int c1 = '1'; c1 < ':'; c1++) {
                path[strlen(path) - 5] = (char)c1;
                tex = tload(path, 0, 0x2601, 0x2601, 0x2901);
                setTex(0, count++, 0, tex);
            }
            for (int c2 = '1'; c2 < '3'; c2++) {
                path[strlen(path) - 6] = (char)c2;
                for (int c3 = '0'; c3 < ':'; c3++) {
                    if (count > 0x18)
                        continue;
                    path[strlen(path) - 5] = (char)c3;
                    tex = tload(path, 0, 0x2601, 0x2601, 0x2901);
                    setTex(0, count++, 0, tex);
                }
            }
        } else {
            for (int letter = 'A'; letter < 'F'; letter++) {
                strcpy(path, base);
                strcat(path, name(i));
                const size_t len = strlen(path);
                path[len] = (char)letter;
                path[len + 1] = 0;
                strcat(path, "0001.bmp");
                tex = tload(path, 0, 0x2601, 0x2601, 0x2901);
                if (tex) {
                    setTex(i, typeCount(i), 0, tex);
                    for (int d = '2'; d < ':'; d++) {
                        path[strlen(path) - 5] = (char)d;
                        tex = tload(path, 0, 0x2601, 0x2601, 0x2901);
                        setTex(i, typeCount(i), d - '1', tex);
                    }
                    typeCount(i)++;
                } else {
                    break;
                }
            }
        }
    }
    static const char* const kTga[2][4] = {{"/Path.tga", "/PathCap.tga", "/PathInside.tga", "/PathCurve.tga"},
                                           {"/PathX.tga", "/PathCapX.tga", "/PathInsideX.tga", "/PathCurveX.tga"}};
    for (int g = 0; g < 2; g++) {
        for (int k = 0; k < 4; k++) {
            strcpy(path, base);
            strcat(path, kTga[g][k]);
            setTex(0x20 + g, 0, k, tga(path));
        }
        typeCount(0x20 + g)++;
    }
    static const struct { const char* file; int type; } kBmp[3] = {{"/CliffTest.bmp", 0x23}, {"/RetainWallA.bmp", 0x22}, {"/strata.bmp", 0x24}};
    for (int k = 0; k < 3; k++) {
        strcpy(path, base);
        strcat(path, kBmp[k].file);
        setTex(kBmp[k].type, 0, 0, tload(path, 0, 0x2601, 0x2601, 0x2901));
        typeCount(kBmp[k].type)++;
    }
    At<unsigned char>(self, 0x29) = 1;
}

// 0x00002ae0  Terrain::Terrain (VA 0x10002ae0)
// Terrain* __thiscall (int width, int height), `ret 8` (width [ebp+8], height [ebp+0xc]), with an SEH frame (handler 0x100410dd); `this` is returned
// (mov eax, [ebp-0x10] at 0x10002ee8). The CRT vector constructor 0x10018670 builds the 0x9c4 tiles of 0x248 bytes at +0x3a4 with Tile::Tile (thunk
// 0x1000114a pushed at 0x10002b17) and Tile::~Tile (thunk 0x1000100a at 0x10002b12) as the cleanup (pushes at 0x10002b1c-0x10002b2e; the callee removes its
// own 0x14 argument bytes: no `add esp` follows the call at 0x10002b2f, so __stdcall). The draw list at +0x164ac4 is built with std::list<Tile*>::list
// 0x1000ae60 (thunk 0x100010be at 0x10002b48) from an allocator temporary that is never written: its first byte is the /GZ frame fill 0xcc, which the list
// constructor copies to list+0. Then g_collarInfo 0x10106b48 = 0 (0x10002b51), width +0x14, height +0x18, +0x1c = 0, floats +4 = +8 = 0.15 (0x3e19999a),
// +0xc = 0.2 (0x3e4ccccd), +0x10 = 1.0 (0x3f800000), the context handle +0 = 0 and the loaded flag byte +0x29 = 0 (0x10002b5b-0x10002bab). For x in 0..width-1
// (outer) and y in 0..height-1 (inner), bounds re-read at every test, Tile::reset(x, y) 0x1000c2c0 (thunk 0x1000119f) on tile x + y * width
// (0x10002bb8-0x10002c15). Then the byte +0x28 = 0 and g_courseType 0x10070a0c = 0 (0x10002c19, 0x10002c20), the 0x25 type counts at +0x40 + 0x18 * i = 0
// (0x10002c2a-0x10002c53), all 0x25 * 0x19 * 9 dwords of g_textures 0x100687f8 = 0 (0x10002c55-0x10002cc1), the type names with strcpy 0x10018580 into
// +0x2c + 0x18 * type (0x10002cc3-0x10002ed1): Tee, PuttingGreen, Fairway, FirmFairway, Rough, DeepRough (types 0-5), GrassySand, PotSandBunker,
// Overgrowth, Brush, Rock, Woods (8-13), WaterShallow, Marsh, Overgrowth (17-19), Building, WaterDeep, WaterMiddle (22, 24, 23), WaterShallowDesert,
// TrickyGreen, SandBunker1, SandBunker2, SandBunker3, SandBunker4 (25-30). Finally Terrain::relight (thunk 0x1000122b at 0x10002edc). Caller (hooks.csv):
// Terrain::getInstance 0x100031a0 (new Terrain(50, 50)).
typedef void*(__fastcall* TerrainCtorList_t)(void*, void*, void*);
typedef void*(__fastcall* TerrainCtor_t)(void*, void*, int, int);
TerrainCtor_t TerrainCtor_orig;
void* __fastcall TerrainCtor_re(void* self, void*, int width, int height) {
    ((void(__stdcall*)(void*, unsigned, unsigned, void*, void*))TerrainRva(0x00018670))(static_cast<char*>(self) + 0x3a4, 0x248, 0x9c4, TerrainRva(0x0000114a), TerrainRva(0x0000100a));
    unsigned char allocator[8];
    allocator[0] = 0xcc;
    ((TerrainCtorList_t)TerrainRva(0x0000ae60))(static_cast<char*>(self) + 0x164ac4, 0, allocator);
    *reinterpret_cast<int*>(TerrainRva(0x00106b48)) = 0;
    At<int>(self, 0x14) = width;
    At<int>(self, 0x18) = height;
    At<int>(self, 0x1c) = 0;
    At<unsigned>(self, 4) = 0x3e19999au;
    At<unsigned>(self, 8) = 0x3e19999au;
    At<unsigned>(self, 0xc) = 0x3e4ccccdu;
    At<unsigned>(self, 0x10) = 0x3f800000u;
    At<int>(self, 0) = 0;
    At<unsigned char>(self, 0x29) = 0;
    for (int x = 0; x < At<int>(self, 0x14); x++)
        for (int y = 0; y < At<int>(self, 0x18); y++)
            ((TileReset_t)TerrainRva(0x0000c2c0))(static_cast<char*>(self) + 0x3a4 + (x + y * At<int>(self, 0x14)) * 0x248, 0, x, y);
    At<unsigned char>(self, 0x28) = 0;
    *reinterpret_cast<int*>(TerrainRva(0x00070a0c)) = 0;
    for (int i = 0; i < 0x25; i++)
        At<int>(self, 0x40 + i * 0x18) = 0;
    for (int i = 0; i < 0x25; i++)
        for (int j = 0; j < 0x19; j++)
            for (int k = 0; k < 9; k++)
                *reinterpret_cast<unsigned*>(TBytes(0x687f8) + i * 0x384 + j * 0x24 + k * 4) = 0;
    static const struct { int type; const char* name; } kNames[24] = {
        {0, "Tee"}, {1, "PuttingGreen"}, {2, "Fairway"}, {3, "FirmFairway"}, {4, "Rough"}, {5, "DeepRough"}, {8, "GrassySand"},
        {9, "PotSandBunker"}, {10, "Overgrowth"}, {11, "Brush"}, {12, "Rock"}, {13, "Woods"}, {17, "WaterShallow"}, {18, "Marsh"},
        {19, "Overgrowth"}, {22, "Building"}, {24, "WaterDeep"}, {23, "WaterMiddle"}, {25, "WaterShallowDesert"}, {26, "TrickyGreen"},
        {27, "SandBunker1"}, {28, "SandBunker2"}, {29, "SandBunker3"}, {30, "SandBunker4"}};
    for (int k = 0; k < 24; k++)
        strcpy(static_cast<char*>(self) + 0x2c + kNames[k].type * 0x18, kNames[k].name);
    ((VoidThis_t)TerrainRva(0x000076e0))(self, 0);
    return self;
}

// 0x00001af0  Terrain::loadNewCourseType (VA 0x10001af0)
// void __thiscall (int type), `ret 4`. g_courseType 0x10070a0c is compared with the argument (cmp eax, [ebp+8] / je 0x10001b38 at 0x10001b12-0x10001b15):
// when they differ the global is set to the argument (0x10001b1a) and Terrain::rebuild 0x10003980 (thunk 0x1000130c), Terrain::reloadTextures 0x100380a0
// (thunk 0x100012ee) and Terrain::relight 0x100076e0 (thunk 0x1000122b) run on `this` in that order (0x10001b20-0x10001b33). Caller: the exe at 0x00449400.
typedef void(__fastcall* LoadNewCourse_t)(void*, void*, int);
LoadNewCourse_t LoadNewCourseType_orig;
void __fastcall LoadNewCourseType_re(void* self, void*, int type) {
    int* g = reinterpret_cast<int*>(TerrainRva(0x00070a0c));
    if (*g != type) {
        *g = type;
        ((VoidThis_t)TerrainRva(0x00003980))(self, 0);
        ((VoidThis_t)TerrainRva(0x000380a0))(self, 0);
        ((VoidThis_t)TerrainRva(0x000076e0))(self, 0);
    }
}

// 0x00009c80  Terrain::initSystem (VA 0x10009c80)
// void __thiscall (int width, int height, HDC dc, bool flipY), `ret 0x10` (width [ebp+8], height [ebp+0xc], dc [ebp+0x10], flip [ebp+0x14]). A non-zero GL
// context handle at this+0 returns at once (cmp [eax], 0 / je / jmp 0x10009e53 at 0x10009ca0-0x10009ca5). A non-zero dc (cmp at 0x10009caa) runs the context
// setup, each failure showing MessageBoxA(0, text, 0x1005f4e0, 0x30) (call [0x101136d8]): ChoosePixelFormat(dc, 0x10063e10) (call [0x101133c0] at 0x10009cbf);
// a zero result shows 0x1005f4b4; SetPixelFormat(dc, format, 0x10063e10) (call [0x101133c4] at 0x10009d01), zero shows 0x1005f494; wglCreateContext(dc)
// (call [0x10113614] at 0x10009d35) stored at this+0 (0x10009d45), zero shows 0x1005f468; wglMakeCurrent(dc, this[0]) (call [0x10113618] at 0x10009d78),
// zero shows 0x1005f434. Then the flip BYTE is stored at this+0x28 (0x10009da9-0x10009dac), and initGL 0x100033e0 (thunk 0x100012f3), initLists 0x100037e0
// (thunk 0x10001249), relight (thunk 0x1000122b) and resize(width, height) 0x10009ed0 (thunk 0x100012f8, pushes at 0x10009dc7-0x10009dcf) run, the size is
// stored at +0x20 / +0x24 (0x10009dda-0x10009de6) and g_screenScale 0x10070a10 is set from the size: 800 x 600 -> bits 0x421aba8f (0x10009e01), 1024 x 768 ->
// 0x42222a9a (0x10009e25), 1280 x 1024 -> 0x42235431 (0x10009e49), otherwise unchanged. Caller: the exe at 0x00449790.
typedef void(__fastcall* InitSystem_t)(void*, void*, int, int, void*, int);
InitSystem_t InitSystem_orig;
void __fastcall InitSystem_re(void* self, void*, int width, int height, void* dc, int flipY) {
    typedef int(WINAPI * ChoosePixelFormat_t)(void*, const void*);
    typedef int(WINAPI * SetPixelFormat_t)(void*, int, const void*);
    typedef void*(WINAPI * WglCreateContext_t)(void*);
    typedef int(WINAPI * WglMakeCurrent_t)(void*, void*);
    typedef int(WINAPI * MessageBoxA_t)(void*, const char*, const char*, unsigned);
    if (At<int>(self, 0) != 0)
        return;
    const void* pfd = TerrainRva(0x00063e10);
    const char* title = reinterpret_cast<const char*>(TerrainRva(0x0005f4e0));
    MessageBoxA_t box = (MessageBoxA_t)GlSlot(0x1136d8);
    if (dc != 0) {
        int format = ((ChoosePixelFormat_t)GlSlot(0x1133c0))(dc, pfd);
        if (format == 0)
            box(0, reinterpret_cast<const char*>(TerrainRva(0x0005f4b4)), title, 0x30);
        if (!((SetPixelFormat_t)GlSlot(0x1133c4))(dc, format, pfd))
            box(0, reinterpret_cast<const char*>(TerrainRva(0x0005f494)), title, 0x30);
        At<void*>(self, 0) = ((WglCreateContext_t)GlSlot(0x113614))(dc);
        if (At<void*>(self, 0) == 0)
            box(0, reinterpret_cast<const char*>(TerrainRva(0x0005f468)), title, 0x30);
        if (!((WglMakeCurrent_t)GlSlot(0x113618))(dc, At<void*>(self, 0)))
            box(0, reinterpret_cast<const char*>(TerrainRva(0x0005f434)), title, 0x30);
    }
    At<unsigned char>(self, 0x28) = (unsigned char)flipY;
    ((BoolThis_t)TerrainRva(0x000033e0))(self, 0);
    ((BoolThis_t)TerrainRva(0x000037e0))(self, 0);
    ((VoidThis_t)TerrainRva(0x000076e0))(self, 0);
    ((Resize_t)TerrainRva(0x00009ed0))(self, 0, width, height);
    At<int>(self, 0x20) = width;
    At<int>(self, 0x24) = height;
    unsigned* scale = reinterpret_cast<unsigned*>(TerrainRva(0x00070a10));
    if (At<int>(self, 0x20) == 800 && At<int>(self, 0x24) == 600)
        *scale = 0x421aba8fu;
    else if (At<int>(self, 0x20) == 1024 && At<int>(self, 0x24) == 768)
        *scale = 0x42222a9au;
    else if (At<int>(self, 0x20) == 1280 && At<int>(self, 0x24) == 1024)
        *scale = 0x42235431u;
}

// 0x000037e0  Terrain::initLists (VA 0x100037e0)
// bool __thiscall, no stack argument (plain `ret` at 0x10003817): Terrain::rebuild 0x10003980 (thunk 0x1000130c at 0x10003800) on `this`, then `mov al, 1`
// (0x10003805). It touches no list. Caller (hooks.csv): Terrain::initSystem 0x10009c80.
BoolThis_t InitLists_orig;
unsigned char __fastcall InitLists_re(void* self, void*) {
    ((VoidThis_t)TerrainRva(0x00003980))(self, 0);
    return 1;
}

}  // namespace

// ----------------------------------------------------------------------- std::list<Tile*>
// 0x0000b6e0  std::list<Tile*>::const_iterator::const_iterator
SG_HOOK("Terrain.dll", 0x0000B6E0, list_Tile_const_iterator_ctor, ConstIterCtor_re, ConstIterCtor_orig);
// 0x0000b280  std::reverse_bidirectional_iterator<std::list<Tile*>::iterator>::ctor
SG_HOOK("Terrain.dll", 0x0000B280, list_Tile_reverse_iterator_ctor, RevIterCtor_re, RevIterCtor_orig);
// 0x0000b800  std::list<Tile*>::erase_1000b800
SG_HOOK("Terrain.dll", 0x0000B800, list_Tile_erase_one, ListEraseOne_re, ListEraseOne_orig);
// 0x0000b470  std::list<Tile*>::erase
SG_HOOK("Terrain.dll", 0x0000B470, list_Tile_erase_range, ListEraseRange_re, ListEraseRange_orig);
// 0x0000b210  std::list<Tile*>::clear
SG_HOOK("Terrain.dll", 0x0000B210, list_Tile_clear, ListClear_re, ListClear_orig);
// 0x0000aed0  std::list<Tile*>::~list
SG_HOOK("Terrain.dll", 0x0000AED0, list_Tile_dtor, ListDtor_re, ListDtor_orig);
// 0x0000b100  std::list<Tile*>::insert
SG_HOOK("Terrain.dll", 0x0000B100, list_Tile_insert, ListInsert_re, ListInsert_orig);
// 0x0000b090  std::list<Tile*>::push_back
SG_HOOK("Terrain.dll", 0x0000B090, list_Tile_push_back, ListPushBack_re, ListPushBack_orig);

// ----------------------------------------------------------------------- iostream helpers
// 0x00002510  ios::fail
SG_HOOK("Terrain.dll", 0x00002510, ios_fail, IosFail_re, IosFail_orig);
// 0x00003f80  ios::lockptr
SG_HOOK("Terrain.dll", 0x00003F80, ios_lockptr, IosLockptr_re, IosLockptr_orig);
// 0x000018b0  ios::lock
SG_HOOK("Terrain.dll", 0x000018B0, ios_lock, IosLock_re, IosLock_orig);
// 0x00037930  ios::unlock
SG_HOOK("Terrain.dll", 0x00037930, ios_unlock, IosUnlock_re, IosUnlock_orig);

// ----------------------------------------------------------------------- Tile and Terrain
// 0x0000c210  Tile::Tile
SG_HOOK("Terrain.dll", 0x0000C210, Tile_ctor, TileCtor_re, TileCtor_orig);
// 0x0000c3d0  Tile::~Tile
SG_HOOK("Terrain.dll", 0x0000C3D0, Tile_dtor, TileDtor_re, TileDtor_orig);
// 0x0000adc0  Terrain::setViewAngle
SG_HOOK("Terrain.dll", 0x0000ADC0, Terrain_setViewAngle, SetViewAngle_re, SetViewAngle_orig);

// ----------------------------------------------------------------------- OpenGL setup and the grid builder
// 0x000033e0  Terrain::initGL
SG_HOOK("Terrain.dll", 0x000033E0, Terrain_initGL, InitGL_re, InitGL_orig);
// 0x00003830  Terrain::light1Off
SG_HOOK("Terrain.dll", 0x00003830, Terrain_light1Off, Light1Off_re, Light1Off_orig);
// 0x000380a0  Terrain::reloadTextures
SG_HOOK("Terrain.dll", 0x000380A0, Terrain_reloadTextures, ReloadTextures_re, ReloadTextures_orig);
// 0x00009ed0  Terrain::resize
SG_HOOK("Terrain.dll", 0x00009ED0, Terrain_resize, Resize_re, Resize_orig);
// 0x0000a130  Terrain::buildArrays
SG_HOOK("Terrain.dll", 0x0000A130, Terrain_buildArrays, BuildArrays_re, BuildArrays_orig);

// ----------------------------------------------------------------------- the tile draw family and the scene drawers
// 0x000116d0  Tile::drawCornerOverlays
SG_HOOK("Terrain.dll", 0x000116D0, Tile_drawCornerOverlays, CornerOverlays_re, CornerOverlays_orig);
// 0x000108f0  Tile::drawPaths
SG_HOOK("Terrain.dll", 0x000108F0, Tile_drawPaths, DrawPaths_re, DrawPaths_orig);
// 0x0000f7f0  Tile::drawWalls
SG_HOOK("Terrain.dll", 0x0000F7F0, Tile_drawWalls, DrawWalls_re, DrawWalls_orig);
// 0x0000ea30  Tile::drawSkirts
SG_HOOK("Terrain.dll", 0x0000EA30, Tile_drawSkirts, DrawSkirts_re, DrawSkirts_orig);
// 0x0000e6c0  Tile::render
SG_HOOK("Terrain.dll", 0x0000E6C0, Tile_render, TileRender_re, TileRender_orig);
// 0x00006850  Terrain::isCulled
SG_HOOK("Terrain.dll", 0x00006850, Terrain_isCulled, IsCulled_re, IsCulled_orig);
// 0x000381a0  Terrain::drawTile
SG_HOOK("Terrain.dll", 0x000381A0, Terrain_drawTile, DrawTile_re, DrawTile_orig);
// 0x00007380  Terrain::drawTileObjects
SG_HOOK("Terrain.dll", 0x00007380, Terrain_drawTileObjects, DrawTileObjects_re, DrawTileObjects_orig);
// 0x00009270  Terrain::stripRender
SG_HOOK("Terrain.dll", 0x00009270, Terrain_stripRender, StripRender_re, StripRender_orig);
// 0x00038900  Terrain::lowerEdge
SG_HOOK("Terrain.dll", 0x00038900, Terrain_lowerEdge, LowerEdge_re, LowerEdge_orig);

// ----------------------------------------------------------------------- the exported scene drawers and neighbours
// 0x0000a970  Terrain::initTerrain
SG_HOOK("Terrain.dll", 0x0000A970, Terrain_initTerrain, InitTerrain_re, InitTerrain_orig);
// 0x0000aa10  Terrain::resetTerrain
SG_HOOK("Terrain.dll", 0x0000AA10, Terrain_resetTerrain, ResetTerrain_re, ResetTerrain_orig);
// 0x000048a0  Terrain::drawLine
SG_HOOK("Terrain.dll", 0x000048A0, Terrain_drawLine, DrawLine_re, DrawLine_orig);
// 0x00008fc0  Terrain::setZoomLevel
SG_HOOK("Terrain.dll", 0x00008FC0, Terrain_setZoomLevel, SetZoomLevel_re, SetZoomLevel_orig);
// 0x00005990  Terrain::render
SG_HOOK("Terrain.dll", 0x00005990, Terrain_render, TerrainRender_re, TerrainRender_orig);
// 0x000089e0  Terrain::localRender
SG_HOOK("Terrain.dll", 0x000089E0, Terrain_localRender, LocalRender_re, LocalRender_orig);
// 0x00006410  Terrain::pathUpdateRender
SG_HOOK("Terrain.dll", 0x00006410, Terrain_pathUpdateRender, PathUpdateRender_re, PathUpdateRender_orig);

// ----------------------------------------------------------------------- texture file loaders
// 0x000016a0  LoadDIBitmap
SG_HOOK("Terrain.dll", 0x000016A0, LoadDIBitmap, LoadDIBitmap_re, LoadDIBitmap_orig);
// 0x0000be30  LoadTGA
SG_HOOK("Terrain.dll", 0x0000BE30, LoadTGA, LoadTGA_re, LoadTGA_orig);
// 0x0000bbd0  textureLoad
SG_HOOK("Terrain.dll", 0x0000BBD0, textureLoad, TextureLoad_re, TextureLoad_orig);

// ----------------------------------------------------------------------- collar table, shutdown, destructor
// 0x0000a880  Terrain::passCollarInfo
SG_HOOK("Terrain.dll", 0x0000A880, Terrain_passCollarInfo, PassCollar_re, PassCollar_orig);
// 0x00009ba0  Terrain::closeSystem
SG_HOOK("Terrain.dll", 0x00009BA0, Terrain_closeSystem, CloseSystem_re, CloseSystem_orig);
// 0x00003090  Terrain::~Terrain
SG_HOOK("Terrain.dll", 0x00003090, Terrain_dtor, TerrainDtor_re, TerrainDtor_orig);

// ----------------------------------------------------------------------- the lighting file
// 0x00010590  istream::getline
SG_HOOK("Terrain.dll", 0x00010590, istream_getline, Getline_re, Getline_orig);
// 0x00006dd0  Terrain::loadLighting
SG_HOOK("Terrain.dll", 0x00006DD0, Terrain_loadLighting, LoadLighting_re, LoadLighting_orig);
// 0x00003980  Terrain::rebuild
SG_HOOK("Terrain.dll", 0x00003980, Terrain_rebuild, Rebuild_re, Rebuild_orig);
// 0x000037e0  Terrain::initLists
SG_HOOK("Terrain.dll", 0x000037E0, Terrain_initLists, InitLists_re, InitLists_orig);

// 0x000046a0  ifstream::{vbase dtor}
SG_HOOK("Terrain.dll", 0x000046A0, ifstream_vbase_dtor, IfstreamDtor_re, IfstreamDtor_orig);

// 0x000076e0  Terrain::relight
SG_HOOK("Terrain.dll", 0x000076E0, Terrain_relight, Relight_re, Relight_orig);

// 0x00001af0  Terrain::loadNewCourseType
SG_HOOK("Terrain.dll", 0x00001AF0, Terrain_loadNewCourseType, LoadNewCourseType_re, LoadNewCourseType_orig);
// 0x00009c80  Terrain::initSystem
SG_HOOK("Terrain.dll", 0x00009C80, Terrain_initSystem, InitSystem_re, InitSystem_orig);

// 0x00002ae0  Terrain::Terrain
SG_HOOK("Terrain.dll", 0x00002AE0, Terrain_ctor, TerrainCtor_re, TerrainCtor_orig);
