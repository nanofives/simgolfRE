// Batch c3aw: Terrain.dll. The addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; every
// instruction address cited in a comment is a VA (VA = 0x10000000 + RVA), the form the debug build's
// disassembly prints. Terrain.dll is a static import of golf_clean.exe, so it is mapped before the shim
// installs its hooks. It is a /Od /ZI /GZ build: every body is literal, so each reimplementation below
// follows the original's order of loads, calls and stores, and each fact carries the VA it comes from.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Two groups:
//   1. the allocation-free rest of the std::list<Tile*> family that batch c3as left for a next batch
//      (log/c3/c3as_notes.txt, "Not attempted"): size, empty, the three const_iterator operators, the
//      postfix iterator operator++, begin, end, the two node-storing iterator constructors and
//      allocator::construct. None of these bodies reaches operator new/delete: the only allocation in the
//      family is in std::_Construct 0x1000ba40, whose `operator new` 0x1000bab0 is the PLACEMENT form
//      (it returns its second argument and allocates nothing; both are already at C3 from batch c3aq).
//   2. roundHalf 0x10002010, the one non-list candidate, which tail-calls the CRT __ftol.
//
// All the list objects, nodes, iterators and destinations an A/B passes in are the fixture's own buffers
// (re/frida/js/fixtures.d/c3aw.js); the game's Terrain object and its tile list are never touched.
#include "hooks.h"

namespace {

// Terrain.dll has base relocations, so its data and callees are reached through the mapped module.
unsigned char* TerrainBase() {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base;
}
void* TerrainRva(unsigned long rva) { return TerrainBase() + rva; }

// A float constant in Terrain.dll's read-only data, by VA (the operand of an fdiv/fsubr below).
float TKonst(unsigned va) { return *(const float*)(TerrainBase() + (va - 0x10000000u)); }

// Callees that live outside this batch, by RVA. The three _Acc accessors and const_iterator::operator==
// are batch c3as's (already at C3); std::_Construct is batch c3aq's.
typedef void*(__cdecl* AccNext_t)(void*);                      // 0x1000b600, returns its argument
typedef void*(__cdecl* AccValue_t)(void*);                     // 0x1000b660, argument + 8
typedef int(__fastcall* IterEquals_t)(void*, void*, void*);    // 0x1000b380, const_iterator::operator==
typedef void(__cdecl* Construct_t)(void*, void*);              // 0x1000ba40, std::_Construct

// ===================================================================================================
// 1. std::list<Tile*>: the list object and its nodes
//
// The three fields the bodies below touch, all read with literal offsets:
//   list + 4   the head node pointer (`mov ecx, [eax+4]` at 0x1000af90 in begin and 0x1000b000 in end)
//   list + 8   the element count (`mov eax, [eax+8]` at 0x1000b450 in size)
//   node + 0   the `next` pointer: _Acc::_Next 0x1000b600 returns the node address itself, and the
//              callers dereference that (`mov eax, [eax]` at 0x1000b7ce and 0x1000af9c)
//   node + 8   the stored Tile pointer: _Acc::_Value 0x1000b660 returns node + 8
// An iterator is one dword, the node pointer (`mov ecx, [eax]` at 0x1000b2f0, `mov [eax], ecx` at
// 0x1000b9d3).
// ===================================================================================================

// 0x0000b430  std::list<Tile*>::size (VA 0x1000b430)
// __thiscall with no stack argument (`ret` with no immediate at 0x1000b459) and no call of its own.
// `this` is stored into [ebp-4] at 0x1000b44a, reloaded at 0x1000b44d and the DWORD at +8 is returned in
// eax (0x1000b450). No branch: every input follows one path.
typedef int(__fastcall* ListSize_t)(void*, void*);
ListSize_t ListSize_orig;
int __fastcall ListSize_re(void* self, void*) {
    return *(const int*)((const unsigned char*)self + 8);
}

// 0x0000b040  std::list<Tile*>::empty (VA 0x1000b040)
// __thiscall with no stack argument (`ret` at 0x1000b07a). It calls size with the same `this` in ecx
// (0x1000b05d, call 0x1000b060 of the thunk 0x10001005, which is jmp 0x1000b430) and turns the count into
// a 0/1 with `neg eax; sbb eax, eax; inc eax` (0x1000b065-0x1000b069): `neg` sets the carry flag for every
// non-zero count, so `sbb eax, eax` leaves -1 for non-zero and 0 for zero, and `inc` makes that 0 and 1.
// The whole of eax is the result. No conditional jump.
typedef int(__fastcall* ListEmpty_t)(void*, void*);
ListEmpty_t ListEmpty_orig;
int __fastcall ListEmpty_re(void* self, void*) {
    const int count = ((ListSize_t)TerrainRva(0x0000b430))(self, 0);
    return count == 0 ? 1 : 0;
}

// 0x0000b2d0  std::list<Tile*>::const_iterator::operator* (VA 0x1000b2d0)
// __thiscall with no stack argument (`ret` at 0x1000b30b). It loads the iterator's node (`mov eax, [ebp-4]`
// at 0x1000b2ed, `mov ecx, [eax]` at 0x1000b2f0), pushes it and calls _Acc::_Value (0x1000b2f3, thunk
// 0x10001271 = jmp 0x1000b660), which is __cdecl (`add esp, 4` at 0x1000b2f8). _Acc::_Value's result, the
// address of the node's stored Tile pointer, is left in eax and returned. Neither body dereferences the
// node, so a NULL node returns the constant 8 instead of faulting. No conditional jump.
typedef void*(__fastcall* IterDeref_t)(void*, void*);
IterDeref_t IterDeref_orig;
void* __fastcall IterDeref_re(void* self, void*) {
    void* const node = *(void**)self;
    return ((AccValue_t)TerrainRva(0x0000b660))(node);
}

// 0x0000b3d0  std::list<Tile*>::const_iterator::operator!= (VA 0x1000b3d0)
// __thiscall with one stack argument, a second iterator (`ret 4` at 0x1000b413). It pushes that argument
// (0x1000b3ed, 0x1000b3f0), puts `this` in ecx (0x1000b3f1) and calls const_iterator::operator== (call
// 0x1000b3f4 of the thunk 0x1000128f = jmp 0x1000b380, __thiscall, so no stack cleanup here). The result
// is narrowed to its low byte (`and eax, 0xff` at 0x1000b3f9) and inverted by the same
// `neg; sbb eax, eax; inc eax` idiom as empty (0x1000b3fe-0x1000b402), so the whole of eax is 1 when the
// two iterators hold the same node and 0 otherwise. No conditional jump.
typedef int(__fastcall* IterNotEquals_t)(void*, void*, void*);
IterNotEquals_t IterNotEquals_orig;
int __fastcall IterNotEquals_re(void* self, void*, void* other) {
    const int equal = ((IterEquals_t)TerrainRva(0x0000b380))(self, 0, other) & 0xff;
    return equal == 0 ? 1 : 0;
}

// 0x0000b7a0  std::list<Tile*>::const_iterator::operator++ (VA 0x1000b7a0)
// The prefix form: __thiscall with no stack argument (`ret` at 0x1000b7e5). It reads the iterator's node
// (0x1000b7bd, 0x1000b7c0), passes it to _Acc::_Next (0x1000b7c3, thunk 0x100010e1 = jmp 0x1000b600,
// __cdecl, `add esp, 4` at 0x1000b7c8), which returns the node address itself; the body then dereferences
// that returned address (`mov eax, [eax]` at 0x1000b7ce), which is the node's `next` field at offset 0,
// and stores it back into the iterator (`mov [edx], eax` at 0x1000b7d0, edx reloaded from [ebp-4] at
// 0x1000b7cb). `this` is returned (0x1000b7d2). No conditional jump, and no NULL check: an iterator
// holding NULL faults at 0x1000b7ce.
typedef void*(__fastcall* CIterPreInc_t)(void*, void*);
CIterPreInc_t CIterPreInc_orig;
void* __fastcall CIterPreInc_re(void* self, void*) {
    void* const node = *(void**)self;
    void** const nextField = (void**)((AccNext_t)TerrainRva(0x0000b600))(node);
    *(void**)self = *nextField;
    return self;
}

// 0x0000b320  std::list<Tile*>::iterator::operator++ (VA 0x1000b320)
// The postfix form: __thiscall with two stack arguments (`ret 8` at 0x1000b368). [ebp+8] is the address of
// the iterator the caller gets back by value and [ebp+0xc] is read nowhere in the body (the `int` that
// distinguishes postfix from prefix).
// The order is: save the current node in a frame slot FIRST (`mov ecx, [eax]` at 0x1000b340,
// `mov [ebp-8], ecx` at 0x1000b342), then advance this iterator by calling the prefix operator++ with
// `this` in ecx (0x1000b345, call 0x1000b348 of the thunk 0x10001221 = jmp 0x1000b7a0), then write the
// SAVED node into the returned object (`mov edx, [ebp+8]` at 0x1000b34d, `mov eax, [ebp-8]` at 0x1000b350,
// `mov [edx], eax` at 0x1000b353) and return that object's address (0x1000b355). No conditional jump.
typedef void*(__fastcall* IterPostInc_t)(void*, void*, void*, int);
IterPostInc_t IterPostInc_orig;
void* __fastcall IterPostInc_re(void* self, void*, void* result, int) {
    void* const previous = *(void**)self;
    ((CIterPreInc_t)TerrainRva(0x0000b7a0))(self, 0);
    *(void**)result = previous;
    return result;
}

// 0x0000afe0  std::list<Tile*>::end (VA 0x1000afe0)
// __thiscall with one stack argument (`ret 4` at 0x1000b026), the address of the iterator returned by
// value. It reads the list's head node pointer at +4 (0x1000affd, 0x1000b000), pushes it and runs the
// iterator constructor on a FOUR-BYTE LOCAL (`lea ecx, [ebp-8]` at 0x1000b004, call 0x1000b007 of the
// thunk 0x10001046 = jmp 0x1000b750), which returns the local's address in eax. The local's single dword
// is then copied into the caller's object (`mov edx, [eax]` at 0x1000b00c, `mov eax, [ebp+8]` at
// 0x1000b00e, `mov [eax], edx` at 0x1000b011) and that object's address is returned (0x1000b013). So the
// value handed back is the head node pointer itself. No conditional jump.
typedef void*(__fastcall* IterCtorNode_t)(void*, void*, void*);
typedef void*(__fastcall* ListEnd_t)(void*, void*, void*);
ListEnd_t ListEnd_orig;
void* __fastcall ListEnd_re(void* self, void*, void* result) {
    void* const head = *(void**)((unsigned char*)self + 4);
    void* local;
    void* const built = ((IterCtorNode_t)TerrainRva(0x0000b750))(&local, 0, head);
    *(void**)result = *(void**)built;
    return result;
}

// 0x0000af70  std::list<Tile*>::begin (VA 0x1000af70)
// Same shape as end with one step more (`ret 4` at 0x1000afc1). The head node pointer at +4 (0x1000af8d,
// 0x1000af90) is passed to _Acc::_Next (0x1000af94, thunk 0x100010e1 = jmp 0x1000b600, __cdecl, cleaned at
// 0x1000af99), which returns the head node's address; that address is dereferenced (`mov edx, [eax]` at
// 0x1000af9c), giving the head node's `next` field, and THAT is what the iterator constructor is run with
// on the four-byte local (0x1000af9e-0x1000afa2). The local's dword is copied into the caller's object
// (0x1000afa7-0x1000afac) and the object's address returned (0x1000afae). No conditional jump; a list
// whose head pointer is NULL faults at 0x1000af9c.
typedef void*(__fastcall* ListBegin_t)(void*, void*, void*);
ListBegin_t ListBegin_orig;
void* __fastcall ListBegin_re(void* self, void*, void* result) {
    void* const head = *(void**)((unsigned char*)self + 4);
    void** const nextField = (void**)((AccNext_t)TerrainRva(0x0000b600))(head);
    void* local;
    void* const built = ((IterCtorNode_t)TerrainRva(0x0000b750))(&local, 0, *nextField);
    *(void**)result = *(void**)built;
    return result;
}

// 0x0000b750  std::list<Tile*>::iterator::iterator (VA 0x1000b750)
// __thiscall with one stack argument, a node pointer (`ret 4` at 0x1000b78c). It forwards the argument to
// the one-dword constructor at 0x1000b9b0 with the same `this` in ecx (push at 0x1000b76d-0x1000b770,
// `mov ecx, [ebp-4]` at 0x1000b771, call 0x1000b774 of the thunk 0x10001078 = jmp 0x1000b9b0, __thiscall
// so no stack cleanup) and returns `this` (0x1000b779). It adds no field of its own. No conditional jump.
typedef void*(__fastcall* IterCtor_t)(void*, void*, void*);
IterCtor_t IterCtor_orig;
void* __fastcall IterCtor_re(void* self, void*, void* node) {
    ((IterCtorNode_t)TerrainRva(0x0000b9b0))(self, 0, node);
    return self;
}

// 0x0000b9b0  std::reverse_bidirectional_iterator<std::list<Tile*>::const_iterator>::ctor (VA 0x1000b9b0)
// The name is the one hooks.csv carries; the body is the one-dword constructor 0x1000b750 delegates to.
// __thiscall with one stack argument (`ret 4` at 0x1000b9de) and no call of its own: it stores the
// argument into the first dword of `this` (`mov eax, [ebp-4]` at 0x1000b9cd, `mov ecx, [ebp+8]` at
// 0x1000b9d0, `mov [eax], ecx` at 0x1000b9d3) and returns `this` (0x1000b9d5). No conditional jump, and no
// NULL check on `this`.
IterCtorNode_t IterCtorNode_orig;
void* __fastcall IterCtorNode_re(void* self, void*, void* node) {
    *(void**)self = node;
    return self;
}

// 0x0000b690  std::allocator<Tile*>::construct (VA 0x1000b690)
// __thiscall with two stack arguments (`ret 8` at 0x1000b6cd). `this` is stored at 0x1000b6aa and never
// read: the allocator carries no state. The SECOND argument [ebp+0xc] is pushed first (0x1000b6ad,
// 0x1000b6b0) and the FIRST argument [ebp+8] after it (0x1000b6b1, 0x1000b6b4), so the __cdecl call at
// 0x1000b6b5 (thunk 0x10001320 = jmp 0x1000ba40, cleaned by `add esp, 8` at 0x1000b6ba) is
// std::_Construct([ebp+8], [ebp+0xc]) - destination first, source second. _Construct runs the placement
// `operator new` 0x1000bab0, which returns the destination unchanged, and copies one dword from the source
// when that destination is not NULL (je 0x1000ba6d).
// construct itself has no conditional jump and loads nothing into eax after the call: the value left there
// is _Construct's, which _Construct stores only into its own dead frame slot [ebp-8], so the memory the
// call wrote is the whole of the observable behaviour (the A/B declares ret "void").
typedef void(__fastcall* AllocConstruct_t)(void*, void*, void*, void*);
AllocConstruct_t AllocConstruct_orig;
void __fastcall AllocConstruct_re(void*, void*, void* destination, void* source) {
    ((Construct_t)TerrainRva(0x0000ba40))(destination, source);
}

// ===================================================================================================
// 2. roundHalf
// ===================================================================================================

// 0x00002010  roundHalf (VA 0x10002010)
// __cdecl with one 4-byte stack argument and no stack cleanup of its own (`ret` at 0x10002046).
// The argument slot is first compared as a DWORD against 0x3f000000 (cmp at 0x10002028), the IEEE-754
// single encoding of 0.5f; when the bits are exactly that, eax is set to 2 (0x10002031) and the body jumps
// straight to the epilogue (0x10002036). Otherwise (jne 0x1000202f) the same slot is loaded as a float
// (`fld dword ptr [ebp+8]` at 0x10002038) and the CRT __ftol at 0x100183ec is called (0x1000203b) with
// that value in st(0); the epilogue follows immediately, so __ftol's eax is the return value.
// __ftol saves the x87 control word (fnstcw 0x100183f3), forces the rounding-control field to truncate
// toward zero (`or ah, 0xc` at 0x100183fb, fldcw 0x10018402), stores a 64-BIT integer (`fistp qword ptr
// [ebp-0xc]` at 0x10018405), restores the control word (0x10018408) and returns the LOW dword in eax
// (0x1001840b), so a value outside the 32-bit range comes back as the low half of the 64-bit truncation
// and an invalid conversion comes back as 0.
// The reimplementation below calls that same __ftol through its address so the conversion is the
// original's, not the shim's CRT.
// Its only caller, Faces::build 0x10002060, passes i/2.0f and 1.0f - i/2.0f for i in {0, 1, 2} (fdiv of
// the constant 2.0f at 0x1005f028, e.g. 0x1000214b-0x10002158), i.e. the three values 0.0f, 0.5f and 1.0f,
// and stores the returned int into the face record it is building (0x10002169).
typedef int(__cdecl* RoundHalf_t)(float);
RoundHalf_t RoundHalf_orig;
int __cdecl RoundHalf_re(float x) {
    if (*(const unsigned int*)&x == 0x3f000000u)
        return 2;                                   // cmp 0x10002028 / mov eax, 2 at 0x10002031
    void* ftol = TerrainRva(0x000183ec);
    int lowDword;
    __asm {
        mov  edx, ftol
        fld  dword ptr [x]                          // fld dword ptr [ebp+8] at 0x10002038
        call edx                                    // call 0x100183ec at 0x1000203b
        mov  lowDword, eax
    }
    return lowDword;
}

// ===================================================================================================
// 3. Faces::build, roundHalf's only caller
// ===================================================================================================

// 0x00002060  Faces::build (VA 0x10002060)
// __thiscall with three stack DWORD arguments (`ret 0xc` at 0x10002509, no value loaded into eax on
// purpose: the last instruction before the epilogue is a copy inside `this`). It writes only into `this`
// and reads only its three arguments and two float constants of the module; it calls nothing but
// roundHalf 0x10002010 (every `call 0x100012b7` is the thunk `jmp 0x10002010`) and the /GZ frame check.
//
// Three strides are precomputed from the arguments, all signed 32-bit:
//   C = arg3 * 3                 (0x10002084-0x1000208a, [ebp-0xc])
//   B = arg2 * C * 3             (0x1000208d-0x10002097, [ebp-0x10])
//   A = arg1 * 3                 (0x1000209a-0x100020a0, [ebp-0x14])
// `this` gets the count 8 at offset 0 (`mov dword ptr [eax], 8` at 0x100020a6), then two nested loops
// (i = [ebp-0x18] and j = [ebp-0x1c], both `cmp ..., 2` + `jge` at 0x100020be/0x100020c2 and
// 0x100020da/0x100020de) run 2 x 2 times and each turn fills TWO consecutive records of 0x38 bytes
// (`imul edx, edx, 0x38` at 0x100020f9 and everywhere after it), so eight records are written in all.
// Per record, three vertex indices at +4, +8, +0xc and six texture values at +0x10..+0x24, each of those
// the result of roundHalf applied to k/2.0f or to 1.0f - k/2.0f, where the divisor is the constant 2.0f at
// 0x1005f028 (`fdiv` at 0x1000214e and after) and the 1.0f is the constant at 0x1005f024 (`fsubr` at
// 0x10002176 and after); k is i, j, i+1 or j+1, each converted with `fild` from a frame slot and each
// quotient stored as a float by the `fstp dword ptr [esp]` that passes it (0x10002155 and after).
// Every record also gets 0 at +0x30 (0x1000224d), the float 1.0f as the literal 0x3f800000 at +0x34
// (0x1000225e) and 0 at +0x38 (0x1000226f) - and +0x38 is PAST the 0x38 stride, so that last store lands
// on the NEXT record's offset 0, which the next record never writes itself.
// After the loops, twelve DWORDs are copied from one place in `this` to another (0x10002427-0x100024f3).
typedef int(__cdecl* RoundHalfCall_t)(float);
typedef void(__fastcall* FacesBuild_t)(void*, void*, int, int, int);
FacesBuild_t FacesBuild_orig;
void __fastcall FacesBuild_re(void* self, void*, int arg1, int arg2, int arg3) {
    unsigned char* const faces = (unsigned char*)self;
    const RoundHalfCall_t roundHalf = (RoundHalfCall_t)TerrainRva(0x00002010);
    const double two = TKonst(0x1005f028);              // the fdiv operand
    const double one = TKonst(0x1005f024);              // the fsubr operand
    const int C = arg3 * 3;
    const int B = arg2 * C * 3;
    const int A = arg1 * 3;
    *(int*)faces = 8;                                   // 0x100020a6
    int f = 0;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            // i, j, i+1 and j+1 are all 0, 1 or 2 here, so every quotient and every 1.0f - quotient is
            // exactly representable as a float and the `fstp dword` rounds nothing away.
            const float jHalf = (float)((double)j / two);
            const float j1Half = (float)((double)(j + 1) / two);
            const float oneMinusIHalf = (float)(one - (double)i / two);
            const float oneMinusI1Half = (float)(one - (double)(i + 1) / two);

            int* r = (int*)(faces + f * 0x38);          // first record of the pair, 0x100020e4-0x1000226f
            r[1] = j + i * C + B + A;
            r[2] = j + (i + 1) * C + B + A;
            r[3] = j + (i + 1) * C + B + 1 + A;
            r[4] = roundHalf(jHalf);
            r[5] = roundHalf(oneMinusIHalf);
            r[6] = roundHalf(jHalf);
            r[7] = roundHalf(oneMinusI1Half);
            r[8] = roundHalf(j1Half);
            r[9] = roundHalf(oneMinusI1Half);
            r[12] = 0;
            r[13] = 0x3f800000;
            r[14] = 0;
            f++;

            r = (int*)(faces + f * 0x38);               // second record, 0x10002280-0x1000240c
            r[1] = j + i * C + B + A;
            r[2] = j + (i + 1) * C + B + 1 + A;
            r[3] = j + i * C + B + 1 + A;
            r[4] = roundHalf(jHalf);
            r[5] = roundHalf(oneMinusIHalf);
            r[6] = roundHalf(j1Half);
            r[7] = roundHalf(oneMinusI1Half);
            r[8] = roundHalf(j1Half);
            r[9] = roundHalf(oneMinusIHalf);
            r[12] = 0;
            r[13] = 0x3f800000;
            r[14] = 0;
            f++;
        }
    }
    // the twelve tail copies, in the original's order (0x10002427 onwards)
    const int copies[12][2] = { {0x7c, 0xb4}, {0x90, 0xc8}, {0x94, 0xcc}, {0xac, 0x78}, {0xb8, 0x88},
                                {0xbc, 0x8c}, {0xec, 0x124}, {0x100, 0x138}, {0x104, 0x13c},
                                {0x11c, 0xe8}, {0x128, 0xf8}, {0x12c, 0xfc} };
    for (int k = 0; k < 12; k++)
        *(int*)(faces + copies[k][0]) = *(const int*)(faces + copies[k][1]);
}

}  // namespace

SG_HOOK("Terrain.dll", 0x0000b430, list_size, ListSize_re, ListSize_orig);
SG_HOOK("Terrain.dll", 0x0000b040, list_empty, ListEmpty_re, ListEmpty_orig);
SG_HOOK("Terrain.dll", 0x0000b2d0, list_const_iterator_deref, IterDeref_re, IterDeref_orig);
SG_HOOK("Terrain.dll", 0x0000b3d0, list_const_iterator_notequals, IterNotEquals_re, IterNotEquals_orig);
SG_HOOK("Terrain.dll", 0x0000b7a0, list_const_iterator_preincrement, CIterPreInc_re, CIterPreInc_orig);
SG_HOOK("Terrain.dll", 0x0000b320, list_iterator_postincrement, IterPostInc_re, IterPostInc_orig);
SG_HOOK("Terrain.dll", 0x0000afe0, list_end, ListEnd_re, ListEnd_orig);
SG_HOOK("Terrain.dll", 0x0000af70, list_begin, ListBegin_re, ListBegin_orig);
SG_HOOK("Terrain.dll", 0x0000b750, list_iterator_ctor, IterCtor_re, IterCtor_orig);
SG_HOOK("Terrain.dll", 0x0000b9b0, list_iterator_ctor_node, IterCtorNode_re, IterCtorNode_orig);
SG_HOOK("Terrain.dll", 0x0000b690, allocator_construct, AllocConstruct_re, AllocConstruct_orig);
SG_HOOK("Terrain.dll", 0x00002010, roundHalf_c3aw, RoundHalf_re, RoundHalf_orig);
SG_HOOK("Terrain.dll", 0x00002060, Faces_build, FacesBuild_re, FacesBuild_orig);
