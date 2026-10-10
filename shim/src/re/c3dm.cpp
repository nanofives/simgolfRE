// C3 batch c3dm (round 26, 2026-10-10) of golf_clean.exe: functions outside the UI that earlier notes named without a
// hard blocker (log/c3/c3dm_candidates.tsv). Every body is hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites the address of each instruction it
// reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their original
// addresses (a hooked callee runs its reimplementation on both arms).
//
// Part 1 - destructor family and list helpers (fixtures c3dm_dtors, c3dm_hashremove, c3dm_dd401): free 0x004a5007 / operator delete 0x004a4ffc run
//          under heapstandin.js, so the blocks they release are a recorded log instead of heap operations.
// Part 2 - tile / noise writers over the global grids (fixtures freeze the game's first thread).
// Part 3 - libjpeg compression API (fixtures hand the functions a synthetic cinfo).
// Part 4 - Window mouse dispatchers; Part 5 - device wrappers; Part 6 - sound object wrappers.
//
// Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
//   Buf_dtor 0x00474810, StrList::dtor 0x00487260, KeyTable::dtor 0x004882f0, freeLoggerBuf 0x004a01a0,
//   NodeList::clear 0x00487310, listClear4a4d 0x004a4d50, operatorDelete_4a4ffc 0x004a4ffc (c3bx / c3cn / c3cz)
#include "hooks.h"

namespace {

template <typename T> inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
inline void* Off(void* p, unsigned off) { return static_cast<char*>(p) + off; }

typedef void(__cdecl* Free_t)(void*);                                  // 0x004a5007  free
typedef void(__cdecl* Delete_t)(void*);                                // 0x004a4ffc  operator delete
typedef void(__fastcall* This0_t)(void* ecx, void* edx);               // thiscall, no stack arguments
typedef void(__stdcall* EhVecDtor_t)(void* p, unsigned size, int count, void* dtor);   // 0x004a5713, ret 0x10
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);
const Delete_t kDelete = reinterpret_cast<Delete_t>(0x004a4ffc);
const This0_t kBufDtor = reinterpret_cast<This0_t>(0x00474810);
const This0_t kStrListDtor = reinterpret_cast<This0_t>(0x00487260);
const This0_t kKeyTableDtor = reinterpret_cast<This0_t>(0x004882f0);
const This0_t kFreeLoggerBuf = reinterpret_cast<This0_t>(0x004a01a0);
const This0_t kListClear = reinterpret_cast<This0_t>(0x004a4d50);
const EhVecDtor_t kEhVecDtor = reinterpret_cast<EhVecDtor_t>(0x004a5713);

typedef void*(__fastcall* ScalarDtor_t)(void* ecx, void* edx, unsigned flags);   // ret 4

// ===================================================================================================
// Part 1 - scalar / deleting destructors. All of them are `dtor(this); if (flags & 1) delete this; return this`
// (`test byte ptr [esp + 8], 1` then `je`): only bit 0 of the low BYTE of the argument is read.
// ===================================================================================================

// 0x004747c0  Buf_deleteDtor(this, flags): Buf_dtor 0x00474810 (call 0x004747c3); bit 0 of flags (test 0x004747c8, je
// 0x004747cd) pushes this (0x004747cf) and calls operator delete 0x004a4ffc (0x004747d0); returns this
// (mov eax, esi 0x004747d8); ret 4 (0x004747db).
ScalarDtor_t BufDeleteDtor_orig;
void* __fastcall BufDeleteDtor_re(void* self, void*, unsigned flags) {
    kBufDtor(self, 0);                                                       // 0x004747c3
    if (flags & 1) kDelete(self);                                            // 0x004747c8-0x004747d0
    return self;                                                             // 0x004747d8
}

// 0x00487240  StrList::scalarDtor: StrList::dtor 0x00487260 (call 0x00487243), then the same flag test (0x00487248)
// and operator delete (0x00487250).
ScalarDtor_t StrListScalarDtor_orig;
void* __fastcall StrListScalarDtor_re(void* self, void*, unsigned flags) {
    kStrListDtor(self, 0);                                                   // 0x00487243
    if (flags & 1) kDelete(self);                                            // 0x00487248-0x00487250
    return self;                                                             // 0x00487258
}

// 0x0049d070  pairArray_deleteDtor: the dtor at 0x0049d040 is a one-instruction thunk (jmp 0x00474810, i.e. Buf_dtor),
// called at 0x0049d073; flag test 0x0049d078, operator delete 0x0049d080.
ScalarDtor_t PairArrayDeleteDtor_orig;
void* __fastcall PairArrayDeleteDtor_re(void* self, void*, unsigned flags) {
    kBufDtor(self, 0);                                                       // 0x0049d073 -> 0x0049d040 -> 0x00474810
    if (flags & 1) kDelete(self);                                            // 0x0049d078-0x0049d080
    return self;                                                             // 0x0049d088
}

// 0x004a0160  loggerDeleteDtor: freeLoggerBuf 0x004a01a0 (call 0x004a0163), flag test 0x004a0168, operator delete
// 0x004a0170.
ScalarDtor_t LoggerDeleteDtor_orig;
void* __fastcall LoggerDeleteDtor_re(void* self, void*, unsigned flags) {
    kFreeLoggerBuf(self, 0);                                                 // 0x004a0163
    if (flags & 1) kDelete(self);                                            // 0x004a0168-0x004a0170
    return self;                                                             // 0x004a0178
}

// 0x004882d0  KeyTable::scalarDtor: KeyTable::dtor 0x004882f0 (call 0x004882d3), flag test 0x004882d8, operator
// delete 0x004882e0.
ScalarDtor_t KeyTableScalarDtor_orig;
void* __fastcall KeyTableScalarDtor_re(void* self, void*, unsigned flags) {
    kKeyTableDtor(self, 0);                                                  // 0x004882d3
    if (flags & 1) kDelete(self);                                            // 0x004882d8-0x004882e0
    return self;                                                             // 0x004882e8
}

// 0x00487390  NodeList::dtor (this = a hash table of 16 NodeList buckets): the vtable 0x004baf00 into +0 (0x004873ad),
// then the CRT eh vector destructor iterator 0x004a5713 (call 0x004873c8) over the 0x10 elements of size 0x1c that start
// at +0x24 (push 0x487310 / 0x10 / 0x1c / this + 0x24, 0x004873b3-0x004873bf), each cleared by NodeList::clear
// 0x00487310; the iterator walks from the last element down (dec count, sub size: 0x004a574a-0x004a574f). Then
// StrList::dtor 0x00487260 on the same object (call 0x004873d7). The SEH frame (0x004873a5..0x004873e8) has no effect
// on a call that does not throw.
This0_t NodeListDtor_orig;
void __fastcall NodeListDtor_re(void* self, void*) {
    At<void*>(self, 0) = reinterpret_cast<void*>(0x004baf00);                // 0x004873ad
    kEhVecDtor(Off(self, 0x24), 0x1c, 0x10, reinterpret_cast<void*>(0x00487310));   // 0x004873c8
    kStrListDtor(self, 0);                                                   // 0x004873d7
}

// 0x004872f0  NodeList::scalarDtor: NodeList::dtor 0x00487390 (call 0x004872f3), flag test 0x004872f8, operator delete
// 0x00487300.
const This0_t kNodeListDtor = reinterpret_cast<This0_t>(0x00487390);
ScalarDtor_t NodeListScalarDtor_orig;
void* __fastcall NodeListScalarDtor_re(void* self, void*, unsigned flags) {
    kNodeListDtor(self, 0);                                                  // 0x004872f3
    if (flags & 1) kDelete(self);                                            // 0x004872f8-0x00487300
    return self;                                                             // 0x00487308
}

// 0x00487a40  NodeListB::scalarDtor: NodeListB::dtor 0x00487a60 (call 0x00487a43; it stores the vtable 0x004bafb0 at +0
// and jumps to StrList::dtor 0x00487260, 0x00487a60-0x00487a66), flag test 0x00487a48, operator delete 0x00487a50.
const This0_t kNodeListBDtor = reinterpret_cast<This0_t>(0x00487a60);
ScalarDtor_t NodeListBScalarDtor_orig;
void* __fastcall NodeListBScalarDtor_re(void* self, void*, unsigned flags) {
    kNodeListBDtor(self, 0);                                                 // 0x00487a43
    if (flags & 1) kDelete(self);                                            // 0x00487a48-0x00487a50
    return self;                                                             // 0x00487a58
}

// 0x00487b60  NodeListC::scalarDtor: NodeListC::dtor 0x00487b80 (call 0x00487b63; vtable 0x004bb014 then jmp
// 0x00487260), flag test 0x00487b68, operator delete 0x00487b70.
const This0_t kNodeListCDtor = reinterpret_cast<This0_t>(0x00487b80);
ScalarDtor_t NodeListCScalarDtor_orig;
void* __fastcall NodeListCScalarDtor_re(void* self, void*, unsigned flags) {
    kNodeListCDtor(self, 0);                                                 // 0x00487b63
    if (flags & 1) kDelete(self);                                            // 0x00487b68-0x00487b70
    return self;                                                             // 0x00487b78
}

// 0x00487ce0  NodeList::clearScalarDtor(this, flags): the vtable 0x004bafac into +0 (0x00487cea), then the list at
// +4 (head) / +8 (tail) / +0x10 (count) is emptied exactly like NodeList::clear 0x00487310 (head at +0xc there):
// an empty head (cmp 0x00487cf0, je 0x00487d51) skips the loop. For each node: its +4 (the next node) becomes the new
// head and that node's +0 is cleared (0x00487cf4-0x00487cfd); with no next node both +8 and +4 of the list are cleared
// (0x00487d02-0x00487d05). The continue flag is the node's +8 (0x00487d08), the node is released through operator
// delete 0x004a4ffc (0x00487d0c) and the count at +0x10 is decremented (0x00487d11-0x00487d1a). The loop ends when
// the flag was 0 (je 0x00487d51 at 0x00487d1d) or when the head is null (0x00487d22-0x00487d24). Then the flag test
// (0x00487d51) deletes this (0x00487d59) and the function returns this.
ScalarDtor_t NodeListClearScalarDtor_orig;
void* __fastcall NodeListClearScalarDtor_re(void* self, void*, unsigned flags) {
    void* node = At<void*>(self, 4);                                         // 0x00487ce7
    At<void*>(self, 0) = reinterpret_cast<void*>(0x004bafac);                // 0x00487cea
    while (node != 0) {                                                      // 0x00487cf0 / 0x00487d22
        void* next = At<void*>(node, 4);                                     // 0x00487cf4
        if (next != 0) {
            At<int>(next, 0) = 0;                                            // 0x00487cfb
            At<void*>(self, 4) = next;                                       // 0x00487cfd
        } else {
            At<int>(self, 8) = 0;                                            // 0x00487d02
            At<int>(self, 4) = 0;                                            // 0x00487d05
        }
        void* cont = At<void*>(node, 8);                                     // 0x00487d08
        kDelete(node);                                                       // 0x00487d0c
        At<int>(self, 0x10) -= 1;                                            // 0x00487d11-0x00487d1a
        if (cont == 0) break;                                                // 0x00487d18 / 0x00487d1d
        node = At<void*>(self, 4);                                           // 0x00487d1f
    }
    if (flags & 1) kDelete(self);                                            // 0x00487d51-0x00487d59
    return self;                                                             // 0x00487d61
}

// 0x00480390  List480::dtor and 0x00480410  BinkFrame::scalarDtor share one body (the second ends with the flag test).
// this: +4 head record, +8 saved next, +0xc count, +0x10 and +0x14 flags. The vtable 0x004baa10 goes into +0 first
// (0x00480399 / 0x00480419). A null head (cmp 0x0048039f, je 0x00480401 / cmp 0x0048041f, je 0x0048047b) only clears +0x10.
// Otherwise, when +0x14 is 0 (cmp 0x004803a7, jne 0x004803f1), it frees `count` (+0xc, jle 0x004803f1 on <= 0) records
// one by one: next = head[0xc] into +8 (0x004803b7-0x004803ba), the payload head[8] through free 0x004a5007 when not null
// (0x004803c0-0x004803c5), head[8] = 0 (0x004803d0), the head record through free when not null (0x004803d6-0x004803db),
// head = next (0x004803e3-0x004803e7). Afterwards +4, +0x10 and +0xc are cleared (0x004803f1-0x004803f7) and +0x10
// again (0x004803fa). The scalar variant tests the flag byte at [esp + 0xc] (0x0048047b) after clearing +0x10 (0x0048047f)
// and deletes this through operator delete (0x00480487).
void List480Body(void* self) {
    void* head = At<void*>(self, 4);
    At<void*>(self, 0) = reinterpret_cast<void*>(0x004baa10);
    if (head == 0) {
        At<int>(self, 0x10) = 0;                                             // 0x00480401 / 0x0048047f
        return;
    }
    if (At<int>(self, 0x14) == 0) {                                          // 0x004803a3-0x004803a9
        int count = At<int>(self, 0xc);                                      // 0x004803ab
        for (int i = 0; count > 0 && i < count; i++) {                       // 0x004803b0-0x004803b2 / 0x004803ed-0x004803ef
            void* rec = At<void*>(self, 4);
            At<void*>(self, 8) = At<void*>(rec, 0xc);                        // 0x004803b7-0x004803ba
            void* payload = At<void*>(rec, 8);
            if (payload != 0) kFree(payload);                                // 0x004803c0-0x004803c5
            At<int>(At<void*>(self, 4), 8) = 0;                              // 0x004803cd-0x004803d0
            void* r2 = At<void*>(self, 4);
            if (r2 != 0) kFree(r2);                                          // 0x004803d3-0x004803db
            At<void*>(self, 4) = At<void*>(self, 8);                         // 0x004803e3-0x004803e7
            count = At<int>(self, 0xc);                                      // 0x004803ea
        }
    }
    At<int>(self, 4) = 0;                                                    // 0x004803f4 / 0x00480471
    At<int>(self, 0x10) = 0;                                                 // 0x004803f1
    At<int>(self, 0xc) = 0;                                                  // 0x004803f7
    At<int>(self, 0x10) = 0;                                                 // 0x004803fa
}

This0_t List480Dtor_orig;
void __fastcall List480Dtor_re(void* self, void*) {
    List480Body(self);
}

ScalarDtor_t BinkFrameScalarDtor_orig;
void* __fastcall BinkFrameScalarDtor_re(void* self, void*, unsigned flags) {
    List480Body(self);
    if (flags & 1) kDelete(self);                                            // 0x00480482-0x00480487
    return self;                                                             // 0x0048048f
}

// 0x00401c30  deletingDtor_401c30(this, flags): the vtable 0x004ba278 into +0 (0x00401c3a); the global 0x00839650
// receives the dword at this + 4 (0x00401c37 / 0x00401c43). With bit 0 of flags (0x00401c40), this non-null (0x00401c4a)
// and that dword 0 (0x00401c4e, jne 0x00401c5b), this goes to free 0x004a5007 (call 0x00401c53); the global is then set
// to 0 (0x00401c5b) when bit 0 and this are set, whatever the dword was. Returns this (0x00401c65).
ScalarDtor_t DeletingDtor401c30_orig;
void* __fastcall DeletingDtor401c30_re(void* self, void*, unsigned flags) {
    int* const g_839650 = reinterpret_cast<int*>(0x00839650);
    void* d4 = At<void*>(self, 4);                                           // 0x00401c37
    At<void*>(self, 0) = reinterpret_cast<void*>(0x004ba278);                // 0x00401c3a
    *g_839650 = reinterpret_cast<int>(d4);                                   // 0x00401c43
    if ((flags & 1) && self != 0) {                                          // 0x00401c40 / 0x00401c48 / 0x00401c4a
        if (d4 == 0) kFree(self);                                            // 0x00401c4e / 0x00401c53
        *g_839650 = 0;                                                       // 0x00401c5b
    }
    return self;                                                             // 0x00401c65
}

// 0x004a4d30  listClearThunk(this, v): listClear4a4d 0x004a4d50 on this (call 0x004a4d33), then the dword at +0x10 =
// the stack argument (0x004a4d38-0x004a4d3c); ret 4 (0x004a4d40). No return value is set (eax is whatever
// listClear4a4d left, which its callers do not read).
typedef void(__fastcall* ListClearThunk_t)(void* ecx, void* edx, int v);
ListClearThunk_t ListClearThunk_orig;
void __fastcall ListClearThunk_re(void* self, void*, int v) {
    kListClear(self, 0);                                                     // 0x004a4d33
    At<int>(self, 0x10) = v;                                                 // 0x004a4d3c
}

// 0x004876c0  HashTable::remove(this, item): 16 buckets of 0x1c bytes from this + 0x2c (bucket b: head dword at
// +0x30 + 0x1c * b). A null item (test 0x004876c5, je 0x00487759) or a bucket index at +0x3c of 0x10 or more (cmp
// 0x004876d0, jae 0x00487759) returns 0xa. A bucket without a head stores 0x10 at item + 0x3c and returns 0 (0x0048774b).
// Otherwise the list from the head is walked through each node's +4 for a node whose +8 is the item (0x004876ef-0x004876f9);
// no match also stores 0x10 and returns 0 (0x004876fb-0x00487704). The match is unlinked: prev = node[0] (0x0048770b),
// next = node[4]; with a prev, prev[4] = next (0x00487711-0x00487714), else the bucket head (bucket + 0x2c + 4 = +0x30)
// = next (0x00487719-0x0048771c); with a next, next[0] = prev and the dword at bucket + 0x2c + 0xc = next
// (0x00487726-0x0048772a), else that dword = 0 and the dword at bucket + 0x2c + 8 = prev (0x0048772f-0x00487738). The
// node goes to operator delete (0x0048773c), the count at bucket + 0x2c + 0x10 is decremented (0x00487741-0x00487748),
// item + 0x3c = 0x10 (0x0048774b) and 0 is returned.
typedef int(__fastcall* HashRemove_t)(void* ecx, void* edx, void* item);
HashRemove_t HashTableRemove_orig;
int __fastcall HashTableRemove_re(void* self, void*, void* item) {
    if (item == 0) return 0xa;                                               // 0x004876c5 / 0x00487759
    unsigned idx = At<unsigned>(item, 0x3c);                                 // 0x004876cd
    if (idx >= 0x10) return 0xa;                                             // 0x004876d0 / 0x004876d3
    char* bucket = static_cast<char*>(self) + idx * 0x1c + 0x2c;             // 0x004876d9-0x004876e9
    void* node = At<void*>(bucket, 4);                                       // 0x004876e3
    if (node != 0) {                                                         // 0x004876e7 / 0x004876ed
        while (At<void*>(node, 8) != item) {                                 // 0x004876ef / 0x004876f2
            node = At<void*>(node, 4);                                       // 0x004876f4
            if (node == 0) {                                                 // 0x004876f7
                At<int>(item, 0x3c) = 0x10;                                  // 0x004876fb
                return 0;                                                    // 0x00487702
            }
        }
        void* prev = At<void*>(node, 0);                                     // 0x0048770b
        void* next;
        if (prev != 0) {
            next = At<void*>(node, 4);
            At<void*>(prev, 4) = next;                                       // 0x00487711-0x00487714
        } else {
            next = At<void*>(node, 4);
            At<void*>(bucket, 4) = next;                                     // 0x00487719-0x0048771c
        }
        next = At<void*>(node, 4);                                           // 0x0048771f
        if (next != 0) {                                                     // 0x00487722
            At<void*>(next, 0) = prev;                                       // 0x00487726-0x00487728
            At<void*>(bucket, 0xc) = next;                                   // 0x0048772a
        } else {
            At<int>(bucket, 0xc) = 0;                                        // 0x00487731
            At<void*>(bucket, 8) = prev;                                     // 0x00487738
        }
        kDelete(node);                                                       // 0x0048773c
        At<int>(bucket, 0x10) -= 1;                                          // 0x00487741-0x00487748
    }
    At<int>(item, 0x3c) = 0x10;                                              // 0x0048774b
    return 0;                                                                // 0x00487753
}

// ===================================================================================================
// Part 2 - tile / noise writers over the game's global grids (golf_clean.exe has no relocations, so the addresses
// are literal constants).
// ===================================================================================================

typedef int(__fastcall* RandomRange_t)(void* rng, void*, int max);            // 0x0045c1e0, ret 4
typedef void(__cdecl* Xy_t)(int x, int y);
const RandomRange_t kRandomRange = reinterpret_cast<RandomRange_t>(0x0045c1e0);
void* const kRng = reinterpret_cast<void*>(0x00822d9c);

// 0x004673e0  genNoise: fills the 18 x 19 byte noise grid at 0x00838aa0 and publishes a 17 x 19 copy at 0x00838c1c.
//  1. 18 rows of 19 bytes (0x00838aa0 + 0x13 * r, rows end at 0x00838bf6: cmp 0x00467409, jl 0x004673ea): bytes 0..17 of
//     each row are Random::range(0x10) (push 0x10, ecx = the generator 0x00822d9c, call 0x0045c1e0, store al at 0x004673f8
//     in a loop of 0x12: cmp 0x004673fc), byte 0x12 repeats byte 0 (0x00467401-0x00467403).
//  2. Row 18 (0x00838bf6) is a copy of the first 0x12 bytes of row 0 (0x00467413-0x00467420).
//  3. The loop at 0x00467425-0x00467484 reads, for 17 x 17 cells, the cell and its eight neighbours through the two dword
//     tables 0x004c2878 / 0x004c2894 and sums them into esi (0x00467439-0x0046745f), but never stores esi: the only store is
//     the cell itself, `mov [eax + edx + 0x838c1b], bl` at 0x0046746e after edx was incremented, so output cell
//     (r, c) at 0x00838c1c + 0x13 * r + c takes the noise cell (r, c), r and c in 0..16 (cmp edx, 0x10 / jle 0x0046742f at
//     0x0046746b-0x00467475, cmp eax, 0x130 / jle at 0x0046747b-0x00467484). The neighbour reads touch no output.
//  4. Column 16 of every output row takes column 0 (0x0046748f-0x00467497: eax from 0x00838c2c step 0x13 to 0x00838d5c, 17
//     rows), and output row 16 (0x00838d4c) is a copy of row 0 (0x004674a0-0x004674b0, 17 bytes).
// Returns nothing (eax is the last copy index; the only caller, 0x00442180 at 0x0044219f, calls 0x0045b880 next).
typedef void(__cdecl* Void0C_t)();
Void0C_t GenNoise_orig;
void __cdecl GenNoise_re() {
    unsigned char* const noise = reinterpret_cast<unsigned char*>(0x00838aa0);
    unsigned char* const out = reinterpret_cast<unsigned char*>(0x00838c1c);
    for (int r = 0; r < 18; r++) {                                                // 0x004673ea-0x0046740f
        unsigned char* row = noise + r * 0x13;
        for (int c = 0; c < 0x12; c++)
            row[c] = static_cast<unsigned char>(kRandomRange(kRng, 0, 0x10));    // 0x004673f3 / 0x004673f8
        row[0x12] = row[0];                                                       // 0x00467401-0x00467403
    }
    for (int c = 0; c < 0x12; c++)
        noise[18 * 0x13 + c] = noise[c];                                          // 0x00467413-0x0046741f
    for (int r = 0; r < 17; r++)
        for (int c = 0; c < 17; c++)
            out[r * 0x13 + c] = noise[r * 0x13 + c];                              // 0x0046746e
    for (int r = 0; r < 17; r++)
        out[r * 0x13 + 16] = out[r * 0x13];                                       // 0x0046748f-0x00467497
    for (int c = 0; c < 17; c++)
        out[16 * 0x13 + c] = out[c];                                              // 0x004674a0-0x004674b0
}

// 0x0042f2c0  markTileKind11: two passes over the 50 x 50 map (x outer, y inner; the bounds are edx < 0x9c4 step 0x32
// and eax < 0x32 at 0x0042f2c8-0x0042f2f7 and edi < 0x00572cac at 0x0042f329).
//  1. Every cell whose tile type (signed byte at 0x005722e8 + 50 x + y, movsx at 0x0042f2ca) has the byte 0x11 at
//     0x00578376 + 0x30 * type (cmp 0x0042f2d8, jne 0x0042f2e8) gets 2 in the path grid 0x0056988c (mov [..], bl with
//     bl = 2 from 0x0042f2c6, store 0x0042f2e1).
//  2. For every such cell again, propagateType11 0x0042f1c0 runs with (x, y) (push esi = y, push ebp = x, call at
//     0x0042f317, add esp, 8). Returns nothing (the callers at 0x0040a100, 0x0040a993, 0x0040bb23 and 0x00472198 do not
//     read eax afterwards).
Void0C_t MarkTileKind11_orig;
void __cdecl MarkTileKind11_re() {
    const signed char* types = reinterpret_cast<const signed char*>(0x005722e8);
    signed char* grid = reinterpret_cast<signed char*>(0x0056988c);
    const Xy_t propagate = reinterpret_cast<Xy_t>(0x0042f1c0);
    for (int x = 0; x < 50; x++)
        for (int y = 0; y < 50; y++) {
            const int t = types[x * 50 + y];                                       // 0x0042f2ca
            if (*reinterpret_cast<const unsigned char*>(0x00578376 + t * 0x30) == 0x11)   // 0x0042f2d8
                grid[x * 50 + y] = 2;                                              // 0x0042f2e1
        }
    for (int x = 0; x < 50; x++)
        for (int y = 0; y < 50; y++) {
            const int t = types[x * 50 + y];                                       // 0x0042f302
            if (*reinterpret_cast<const unsigned char*>(0x00578376 + t * 0x30) == 0x11)   // 0x0042f30c
                propagate(x, y);                                                   // 0x0042f317
        }
}

// 0x0042f120  floodMark(x, y): flood fill over the word grid 0x0053caf0 (index 50 x + y, two bytes per cell).
// A cell with bit 0x40 already set returns at once (test cl, 0x40 at 0x0042f13d, jne 0x0042f1b0). Otherwise bit 0x40 is
// set (0x0042f14a-0x0042f150) and the type byte at 0x005722e8 + 50 x + y (movsx at 0x0042f142) selects the signed byte at
// 0x00578372 + 0x30 * type (0x0042f15b); a value of 0 or less ends the call (test cl, cl / jle at 0x0042f161-0x0042f163).
// Otherwise the four neighbours k = 0..3 (esi steps by 8 bytes, cmp esi, 0x20 at 0x0042f1a9: entries 0, 2, 4, 6 of the
// dword offset tables 0x004c2878 for x and 0x004c2898 for y, loads 0x0042f169 / 0x0042f16f) are tested without a
// bounds check: when the neighbour's word has bit 0x20 or 0x400 (and ebp, 0x420 / test bp, bp at 0x0042f18c-0x0042f192)
// and not bit 0x40 (test dl, 0x40 at 0x0042f197), floodMark runs on it through its own address (push ecx = y,
// push eax = x, call 0x0042f120 at 0x0042f19e).
typedef void(__cdecl* FloodMark_t)(int x, int y);
FloodMark_t FloodMark_orig;
void __cdecl FloodMark_re(int x, int y) {
    unsigned short* const flags = reinterpret_cast<unsigned short*>(0x0053caf0);
    const int* const dx = reinterpret_cast<const int*>(0x004c2878);
    const int* const dy = reinterpret_cast<const int*>(0x004c2898);
    const int idx = x * 50 + y;                                                    // 0x0042f12a-0x0042f132
    const unsigned short w = flags[idx];                                           // 0x0042f135
    if (w & 0x40) return;                                                          // 0x0042f13d / 0x0042f140
    const int t = reinterpret_cast<const signed char*>(0x005722e8)[idx];           // 0x0042f142
    flags[idx] = static_cast<unsigned short>(w | 0x40);                            // 0x0042f14a-0x0042f150
    const signed char reach = *reinterpret_cast<const signed char*>(0x00578372 + t * 0x30);   // 0x0042f15b
    if (reach <= 0) return;                                                        // 0x0042f161 / 0x0042f163
    for (int k = 0; k < 4; k++) {                                                  // 0x0042f167 / 0x0042f1a9
        const int nx = dx[2 * k] + x, ny = dy[2 * k] + y;                          // 0x0042f169-0x0042f177
        const unsigned short w2 = flags[nx * 50 + ny];                             // 0x0042f17f-0x0042f182
        if ((w2 & 0x420) != 0 && (w2 & 0x40) == 0)                                 // 0x0042f18c-0x0042f19a
            reinterpret_cast<FloodMark_t>(0x0042f120)(nx, ny);                     // 0x0042f19e
    }
}

// ===================================================================================================
// Part 3 - the libjpeg 6a compression API (IJG release, "6a  7-Feb-96": re/match/golf_jpeg_capimin.cpp, _capistd.cpp,
// _cparam.cpp). j_compress_ptr fields as the instructions address them:
//   +0 err  +4 mem  +8 progress  +0xc is_decompressor  +0x10 global_state  +0x14 dest  +0x1c image_height
//   +0x24 in_color_space  +0x30 data_precision  +0x3c comp_info  +0xa8 raw_data_in  +0xd8 next_scanline
//   +0xe8 total_iMCU_rows  +0x144 master  +0x148 main  +0x150 coef  +0x154 marker
// The error manager is reached the way the originals reach it: msg_code at err + 0x14, the first parameter at err + 0x18,
// error_exit at err + 0, emit_message at err + 4, reset_error_mgr at err + 0x10 (ERREXIT = store the code, then call
// err->error_exit(cinfo); every body continues after a returning handler, which the fixtures' handler does).
// ===================================================================================================

typedef void(__cdecl* JCall1_t)(void* cinfo);
typedef void*(__cdecl* JAllocSmall_t)(void* cinfo, int pool, unsigned size);
typedef void(__cdecl* JCallEmit_t)(void* cinfo, int level);
typedef int(__cdecl* JCoef_t)(void* cinfo, void* buf);
typedef void(__cdecl* JProcess_t)(void* cinfo, void* rows, unsigned* row_ctr, unsigned num);
typedef void(__cdecl* JSetColorspace_t)(void* cinfo, int cs);
typedef void(__cdecl* JSetQuality_t)(void* cinfo, int quality, int force_baseline);
typedef void(__cdecl* JSuppress_t)(void* cinfo, int suppress);

const JSetColorspace_t kJpegSetColorspace = reinterpret_cast<JSetColorspace_t>(0x004ae990);   // C3 (c3bj)
const JSetQuality_t kJpegSetQuality = reinterpret_cast<JSetQuality_t>(0x004ae740);            // C3 (c3bx)
const JCall1_t kStdHuffTables = reinterpret_cast<JCall1_t>(0x004ae850);                       // C3
const JSuppress_t kJpegSuppressTables = reinterpret_cast<JSuppress_t>(0x004ae220);            // C3
const JCall1_t kJinitCompressMaster = reinterpret_cast<JCall1_t>(0x004b03a0);                 // C2
const JCall1_t kJpegAbort = reinterpret_cast<JCall1_t>(0x004afa60);                           // C3
const JCall1_t kJpegDefaultColorspace = reinterpret_cast<JCall1_t>(0x004ae910);

// ERREXIT with the error manager's first parameter: store msg_code (and parm when given), call err->error_exit(cinfo).
inline void JErrExit(void* cinfo, int code) {
    void* err = At<void*>(cinfo, 0);
    At<int>(err, 0x14) = code;
    At<JCall1_t>(At<void*>(cinfo, 0), 0)(cinfo);
}
inline void JErrExitState(void* cinfo, int code) {   // ERREXIT1(cinfo, code, cinfo->global_state)
    void* err = At<void*>(cinfo, 0);
    At<int>(err, 0x14) = code;
    At<int>(At<void*>(cinfo, 0), 0x18) = At<int>(cinfo, 0x10);
    At<JCall1_t>(At<void*>(cinfo, 0), 0)(cinfo);
}

// 0x004ae910  jpeg_default_colorspace(cinfo): picks the JPEG colour space for in_color_space (cinfo + 0x24, unsigned
// compare `cmp ecx, 5 / ja 0x004ae95f` at 0x004ae917-0x004ae91a, jump table 0x004ae970): 0 -> jpeg_set_colorspace
// 0x004ae990 with 0 (push 0 at 0x004ae953), 1 -> 1 (0x004ae923), 2 and 3 -> 3 (0x004ae92f), 4 -> 4 (0x004ae93b), 5 -> 5
// (0x004ae947). Anything else stores msg_code 7 (0x004ae962; no parameter) and calls err->error_exit (0x004ae96b).
JCall1_t JpegDefaultColorspace_orig;
void __cdecl JpegDefaultColorspace_re(void* cinfo) {
    const unsigned cs = At<unsigned>(cinfo, 0x24);                                // 0x004ae914
    switch (cs) {                                                                // 0x004ae917-0x004ae91c
    case 0: kJpegSetColorspace(cinfo, 0); return;                                // 0x004ae953
    case 1: kJpegSetColorspace(cinfo, 1); return;                                // 0x004ae923
    case 2:
    case 3: kJpegSetColorspace(cinfo, 3); return;                                // 0x004ae92f
    case 4: kJpegSetColorspace(cinfo, 4); return;                                // 0x004ae93b
    case 5: kJpegSetColorspace(cinfo, 5); return;                                // 0x004ae947
    default: JErrExit(cinfo, 7); return;                                         // 0x004ae95f-0x004ae96b
    }
}

// 0x004ae760  jpeg_set_defaults(cinfo): a global_state other than 0x64 (cmp 0x004ae766) raises msg_code 0x12 with the state
// as parameter (0x004ae76f-0x004ae780) and goes on. A null comp_info (+0x3c, test 0x004ae78a) is allocated through
// mem->alloc_small(cinfo, 0 = JPOOL_PERMANENT, 0x348) (push 0x348 / push ebx at 0x004ae791-0x004ae798, call [ecx]). Then
// data_precision (+0x30) = 8 (0x004ae7a6), jpeg_set_quality(cinfo, 0x4b, 1) (0x004ae7a1-0x004ae7ad), std_huff_tables
// 0x004ae850 (0x004ae7b3) and the three 16-byte arithmetic-coding tables: +0x70 = 0, +0x80 = 1, +0x90 = 5 for each of the
// 16 entries (0x004ae7ca-0x004ae7da). scan_info +0xa4, num_scans +0xa0, raw_data_in +0xa8, arith_code +0xac and
// optimize_coding +0xb0 are cleared (0x004ae7df-0x004ae7fa); +0xb0 becomes 1 when data_precision (re-read at 0x004ae7dc)
// exceeds 8 (cmp 0x004ae7e5, jle 0x004ae80d) - never, since 0x004ae7a6 stored 8. +0xb4, +0xb8, +0xbc, +0xc0 and +0xc4 are
// cleared (0x004ae80e-0x004ae826), the byte +0xcc is cleared (0x004ae82c), the words +0xce and +0xd0 are 1 (0x004ae832,
// 0x004ae83b) and jpeg_default_colorspace 0x004ae910 closes the function (0x004ae844).
JCall1_t JpegSetDefaults_orig;
void __cdecl JpegSetDefaults_re(void* cinfo) {
    if (At<int>(cinfo, 0x10) != 0x64)                                             // 0x004ae766
        JErrExitState(cinfo, 0x12);                                               // 0x004ae76c-0x004ae780
    if (At<void*>(cinfo, 0x3c) == 0)                                              // 0x004ae785-0x004ae78c
        At<void*>(cinfo, 0x3c) = At<JAllocSmall_t>(At<void*>(cinfo, 4), 0)(cinfo, 0, 0x348);   // 0x004ae78e-0x004ae79d
    At<int>(cinfo, 0x30) = 8;                                                     // 0x004ae7a6
    kJpegSetQuality(cinfo, 0x4b, 1);                                              // 0x004ae7ad
    kStdHuffTables(cinfo);                                                        // 0x004ae7b3
    for (int i = 0; i < 16; i++) {                                                // 0x004ae7ca-0x004ae7da
        At<char>(cinfo, 0x70 + i) = 0;
        At<char>(cinfo, 0x80 + i) = 1;
        At<char>(cinfo, 0x90 + i) = 5;
    }
    const int precision = At<int>(cinfo, 0x30);                                   // 0x004ae7dc
    At<int>(cinfo, 0xa4) = 0;                                                     // 0x004ae7df
    At<int>(cinfo, 0xa0) = 0;                                                     // 0x004ae7e8
    At<int>(cinfo, 0xa8) = 0;                                                     // 0x004ae7ee
    At<int>(cinfo, 0xac) = 0;                                                     // 0x004ae7f4
    At<int>(cinfo, 0xb0) = 0;                                                     // 0x004ae7fa
    if (precision > 8) At<int>(cinfo, 0xb0) = 1;                                  // 0x004ae7e5 / 0x004ae801 / 0x004ae803
    At<int>(cinfo, 0xb4) = 0;                                                     // 0x004ae80e
    At<int>(cinfo, 0xb8) = 0;                                                     // 0x004ae814
    At<int>(cinfo, 0xbc) = 0;                                                     // 0x004ae81a
    At<int>(cinfo, 0xc0) = 0;                                                     // 0x004ae820
    At<int>(cinfo, 0xc4) = 0;                                                     // 0x004ae826
    At<char>(cinfo, 0xcc) = 0;                                                    // 0x004ae82c
    At<short>(cinfo, 0xce) = 1;                                                   // 0x004ae832
    At<short>(cinfo, 0xd0) = 1;                                                   // 0x004ae83b
    kJpegDefaultColorspace(cinfo);                                                // 0x004ae844
}

// 0x004ae400  jpeg_write_scanlines(cinfo, scanlines, num_lines): returns the number of rows the main controller took.
// A global_state other than 0x65 raises msg_code 0x12 with the state (cmp 0x004ae405, 0x004ae40b-0x004ae41f). When
// next_scanline (+0xd8) is not below image_height (+0x1c) (unsigned `jb 0x004ae445` at 0x004ae42f) msg_code 0x77 is set and
// err->emit_message(cinfo, -1) runs (push -1 / push esi / call [eax + 4], 0x004ae433-0x004ae442). A non-null progress
// monitor (+8, test 0x004ae448) receives pass_counter = next_scanline (+4) and pass_limit = image_height (+8) and its
// callback at +0 runs (0x004ae44c-0x004ae464). The master (+0x144) calls pass_startup (its +4) when its flag at +0xc is
// set (0x004ae467-0x004ae478). The row count is clamped to image_height - next_scanline (unsigned `jbe` at 0x004ae48c,
// the clamp at 0x004ae48e), row_ctr starts at 0 (0x004ae4a2) and main->process_data (main = +0x148, slot +4) gets (cinfo,
// scanlines, &row_ctr, num_lines) (0x004ae496-0x004ae4aa). next_scanline += row_ctr (0x004ae4ba-0x004ae4bc); row_ctr is
// returned (0x004ae4ad, eax kept through the add).
typedef unsigned(__cdecl* JpegWriteScanlines_t)(void* cinfo, void* scanlines, unsigned num_lines);
JpegWriteScanlines_t JpegWriteScanlines_orig;
unsigned __cdecl JpegWriteScanlines_re(void* cinfo, void* scanlines, unsigned num_lines) {
    if (At<int>(cinfo, 0x10) != 0x65)                                             // 0x004ae405
        JErrExitState(cinfo, 0x12);                                               // 0x004ae40b-0x004ae41f
    if (!(At<unsigned>(cinfo, 0xd8) < At<unsigned>(cinfo, 0x1c))) {               // 0x004ae424-0x004ae42f
        At<int>(At<void*>(cinfo, 0), 0x14) = 0x77;                                // 0x004ae436
        At<JCallEmit_t>(At<void*>(cinfo, 0), 4)(cinfo, -1);                       // 0x004ae43d-0x004ae442
    }
    void* progress = At<void*>(cinfo, 8);                                         // 0x004ae445
    if (progress != 0) {                                                          // 0x004ae44a
        At<int>(progress, 4) = At<int>(cinfo, 0xd8);                              // 0x004ae44c-0x004ae453
        At<int>(progress, 8) = At<int>(cinfo, 0x1c);                              // 0x004ae459-0x004ae45c
        At<JCall1_t>(At<void*>(cinfo, 8), 0)(cinfo);                              // 0x004ae45f-0x004ae462
    }
    void* master = At<void*>(cinfo, 0x144);                                       // 0x004ae467
    if (At<int>(master, 0xc) != 0)                                                // 0x004ae46d-0x004ae472
        At<JCall1_t>(master, 4)(cinfo);                                           // 0x004ae474-0x004ae475
    const unsigned left = At<unsigned>(cinfo, 0x1c) - At<unsigned>(cinfo, 0xd8);  // 0x004ae47b-0x004ae484
    unsigned n = num_lines;
    if (n > left) n = left;                                                       // 0x004ae48a-0x004ae48e
    unsigned row_ctr = 0;                                                         // 0x004ae4a2
    At<JProcess_t>(At<void*>(cinfo, 0x148), 4)(cinfo, scanlines, &row_ctr, n);    // 0x004ae4aa
    At<unsigned>(cinfo, 0xd8) += row_ctr;                                         // 0x004ae4ba-0x004ae4bc
    return row_ctr;                                                               // 0x004ae4ad
}

// 0x004ae380  jpeg_start_compress(cinfo, write_all_tables): a global_state other than 0x64 raises msg_code 0x12 with the
// state (0x004ae385-0x004ae39f) and goes on. A non-zero write_all_tables (test 0x004ae3a8) calls jpeg_suppress_tables
// 0x004ae220 with FALSE (push 0 / push esi, 0x004ae3ac-0x004ae3af). Then err->reset_error_mgr (+0x10, 0x004ae3ba),
// dest->init_destination (dest = +0x14, slot +8, 0x004ae3c1), jinit_compress_master 0x004b03a0 (0x004ae3c5),
// master->prepare_for_pass (master = +0x144, slot +0, 0x004ae3d1), next_scanline (+0xd8) = 0 (0x004ae3de) and global_state
// = 0x65 + (raw_data_in != 0) (setne / add 0x65 at 0x004ae3ea-0x004ae3f0). No value is returned (the only caller, 0x004ae020,
// does not read eax at 0x004ae0eb-0x004ae0f0).
typedef void(__cdecl* JpegStartCompress_t)(void* cinfo, int write_all_tables);
JpegStartCompress_t JpegStartCompress_orig;
void __cdecl JpegStartCompress_re(void* cinfo, int write_all_tables) {
    if (At<int>(cinfo, 0x10) != 0x64)                                             // 0x004ae385
        JErrExitState(cinfo, 0x12);                                               // 0x004ae38b-0x004ae39f
    if (write_all_tables != 0)                                                    // 0x004ae3a8
        kJpegSuppressTables(cinfo, 0);                                            // 0x004ae3af
    At<JCall1_t>(At<void*>(cinfo, 0), 0x10)(cinfo);                               // 0x004ae3b7-0x004ae3ba
    At<JCall1_t>(At<void*>(cinfo, 0x14), 8)(cinfo);                               // 0x004ae3bd-0x004ae3c1
    kJinitCompressMaster(cinfo);                                                  // 0x004ae3c5
    At<JCall1_t>(At<void*>(cinfo, 0x144), 0)(cinfo);                              // 0x004ae3ca-0x004ae3d1
    const int raw = At<int>(cinfo, 0xa8);                                         // 0x004ae3d3
    At<unsigned>(cinfo, 0xd8) = 0;                                                // 0x004ae3de
    At<int>(cinfo, 0x10) = 0x65 + (raw != 0 ? 1 : 0);                             // 0x004ae3ea-0x004ae3f0
}

// 0x004ae270  jpeg_finish_compress(cinfo): global_state 0x65 or 0x66 (cmp 0x004ae278 / 0x004ae27d): when next_scanline
// (+0xd8) is below image_height (+0x1c) (unsigned `jae` at 0x004ae2aa) msg_code 0x42 is raised (0x004ae2af); then
// master->finish_pass (+8, 0x004ae2c1). State 0x67 (cmp 0x004ae282) skips straight to the pass loop (0x004ae2ca). Any other
// state raises msg_code 0x12 with the state (0x004ae289-0x004ae29b) and goes to the loop. The loop runs while
// master->is_last_pass (+0x10, test 0x004ae2d5 / 0x004ae352) is 0: prepare_for_pass (0x004ae2df), then total_iMCU_rows
// (+0xe8, re-read at 0x004ae32f) iMCU rows: a non-null progress monitor gets pass_counter = the row and pass_limit =
// total_iMCU_rows and its callback runs (0x004ae2f0-0x004ae30c), then coef (+0x150)->compress_data (slot +4) with
// (cinfo, NULL) (0x004ae30f-0x004ae318); a zero result raises msg_code 0x16 (ebx = 0x16 from 0x004ae2d9, store
// 0x004ae325). After the rows master->finish_pass (0x004ae341) and the is_last_pass test again. Finally
// marker (+0x154)->write_file_trailer (slot +0x10, 0x004ae35d), dest (+0x14)->term_destination (slot +0x10, 0x004ae364) and
// jpeg_abort 0x004afa60 (0x004ae368). No value is returned (the only caller, 0x004ae020, ignores eax at 0x004ae116).
JCall1_t JpegFinishCompress_orig;
void __cdecl JpegFinishCompress_re(void* cinfo) {
    const int state = At<int>(cinfo, 0x10);                                       // 0x004ae275
    if (state == 0x65 || state == 0x66) {                                         // 0x004ae278 / 0x004ae27d
        if (!(At<unsigned>(cinfo, 0xd8) >= At<unsigned>(cinfo, 0x1c)))            // 0x004ae2a5-0x004ae2aa
            JErrExit(cinfo, 0x42);                                                // 0x004ae2ac-0x004ae2b8
        At<JCall1_t>(At<void*>(cinfo, 0x144), 8)(cinfo);                          // 0x004ae2bd-0x004ae2c4
    } else if (state != 0x67) {                                                   // 0x004ae282
        JErrExitState(cinfo, 0x12);                                               // 0x004ae287-0x004ae29b
    }
    if (At<int>(At<void*>(cinfo, 0x144), 0x10) == 0) {                            // 0x004ae2ca-0x004ae2d5
        do {
            At<JCall1_t>(At<void*>(cinfo, 0x144), 0)(cinfo);                      // 0x004ae2de-0x004ae2df
            for (unsigned row = 0; row < At<unsigned>(cinfo, 0xe8); row++) {      // 0x004ae2e1-0x004ae338
                void* progress = At<void*>(cinfo, 8);                             // 0x004ae2f0
                if (progress != 0) {                                              // 0x004ae2f3
                    At<unsigned>(progress, 4) = row;                              // 0x004ae2f7
                    At<unsigned>(progress, 8) = At<unsigned>(cinfo, 0xe8);        // 0x004ae2fd-0x004ae304
                    At<JCall1_t>(At<void*>(cinfo, 8), 0)(cinfo);                  // 0x004ae307-0x004ae30a
                }
                if (At<JCoef_t>(At<void*>(cinfo, 0x150), 4)(cinfo, 0) == 0)       // 0x004ae30f-0x004ae320
                    JErrExit(cinfo, 0x16);                                        // 0x004ae322-0x004ae32a
            }
            At<JCall1_t>(At<void*>(cinfo, 0x144), 8)(cinfo);                      // 0x004ae33a-0x004ae341
        } while (At<int>(At<void*>(cinfo, 0x144), 0x10) == 0);                    // 0x004ae344-0x004ae352
    }
    At<JCall1_t>(At<void*>(cinfo, 0x154), 0x10)(cinfo);                           // 0x004ae356-0x004ae35d
    At<JCall1_t>(At<void*>(cinfo, 0x14), 0x10)(cinfo);                            // 0x004ae360-0x004ae364
    kJpegAbort(cinfo);                                                            // 0x004ae368
}

// ===================================================================================================
// Part 4 - the Window mouse dispatchers. Seven siblings of Window::mouseDispatch254 0x0047c430 (c3o): six have exactly its
// instruction sequence (75 instructions, constants masked: compared with re/tools/asm2inline.py output, 2026-10-10) and
// differ only in five vtable slots, one cdecl callback offset and two panel offsets; Window::click 0x0047c1b0 adds the
// global callback at 0x0083ab04. Shared facts (e.g. 0x0047bf46-0x0047bf5d):
//   - a window whose flags dword +0x9c has bit 0x200000, or whose byte +0xa0 has bit 8, ignores the event;
//   - release (third argument) != 0: virtual slot REL(x, y), then the panel at RPNL is notified through its slot 0x1c;
//   - release == 0: g_curWidget 0x0083ab2c = this, the cdecl callback at CB (x, y) when set, then HotList::hitTestRect
//     0x00492b10 on the hot-spot list embedded at +0xbc with x and y passed by address (they come back as the entry's two
//     out values on a hit) and a 16-byte rect; a hit (index >= 0) calls virtual slot HIT(a, b, &rect); no hit, or a zero
//     answer, calls virtual slot MISS(x, y) with the ORIGINAL x and y (ebx / edi, e.g. 0x0047bfc6-0x0047bfcc); then the
//     panel at PNL is notified.
// Returns nothing: the originals are matched as void methods (re/match/golf_hand_06_win.cpp, golf_hand_07_47c1b0.cpp).
// ===================================================================================================

typedef void(__fastcall* WinMouse_t)(void* ecx, void* edx, int x, int y, int release);        // ret 0xc
typedef int(__fastcall* Thiscall0_t)(void*, void*);
typedef int(__fastcall* Thiscall2_t)(void*, void*, int, int);
typedef int(__fastcall* Thiscall3_t)(void*, void*, int, int, int);
typedef int(__cdecl* Cb2_t)(int, int);
typedef int(__fastcall* HitRect_t)(void*, void*, int, int, int*, int*, int*);                   // ret 0x14
const HitRect_t kHitTestRect = reinterpret_cast<HitRect_t>(0x00492b10);                          // HotList::hitTestRect (C3)

inline void** Vtbl(void* o) { return *reinterpret_cast<void***>(o); }
inline bool WinIgnores(void* w) {
    return (At<unsigned>(w, 0x9c) & 0x200000u) != 0 || (At<unsigned char>(w, 0xa0) & 0x08) != 0;
}
inline void NotifyPanel(void* panel) {
    if (panel) reinterpret_cast<Thiscall0_t>(Vtbl(panel)[0x1c / 4])(panel, nullptr);
}

template <int CB, int HIT, int MISS, int REL, int PNL, int RPNL>
struct MouseD {
    static WinMouse_t orig;
    static void __fastcall re(void* self, void*, int x, int y, int release) {
        if (WinIgnores(self)) return;
        if (release != 0) {
            reinterpret_cast<Thiscall2_t>(Vtbl(self)[REL / 4])(self, nullptr, x, y);
            NotifyPanel(At<void*>(self, RPNL));
            return;
        }
        *reinterpret_cast<void**>(0x0083ab2c) = self;
        Cb2_t cb = At<Cb2_t>(self, CB);
        if (cb != 0) cb(x, y);
        int a = x, b = y;
        int rect[4];
        const int hit = kHitTestRect(static_cast<char*>(self) + 0xbc, nullptr, x, y, &a, &b, rect);
        bool handled = false;
        if (hit >= 0)
            handled = reinterpret_cast<Thiscall3_t>(Vtbl(self)[HIT / 4])(self, nullptr, a, b, reinterpret_cast<int>(rect)) != 0;
        if (!handled) reinterpret_cast<Thiscall2_t>(Vtbl(self)[MISS / 4])(self, nullptr, x, y);
        NotifyPanel(At<void*>(self, PNL));
    }
};
template <int CB, int HIT, int MISS, int REL, int PNL, int RPNL>
WinMouse_t MouseD<CB, HIT, MISS, REL, PNL, RPNL>::orig;

// 0x0047bf40  mouseDispatch23c: callback +0x23c (cdecl, call eax 0x0047bf87), hit slot 0x34 (0x0047bfbf), miss slot 0x98
//   (0x0047bfcc), release slot 0xbc (0x0047bff5), panels +0x44 (0x0047bfd2) / +0x6c (0x0047bffb).
// 0x0047c010  mouseDispatch240: callback +0x240, hit 0x30 (0x0047c08f), miss 0x9c (0x0047c09c), release 0xc0 (0x0047c0c5),
//   panels +0x48 (0x0047c0a2) / +0x70 (0x0047c0cb).
// 0x0047c0e0  mouseDispatch244: callback +0x244, hit 0x3c (0x0047c15f), miss 0x74 (0x0047c16c), release 0xa4 (0x0047c192),
//   panels +0x20 (0x0047c16f) / +0x54 (0x0047c198).
// 0x0047c290  mouseDispatch24c: callback +0x24c, hit 0x48 (0x0047c30f), miss 0x7c (0x0047c31c), release 0xac (0x0047c342),
//   panels +0x28 (0x0047c31f) / +0x5c (0x0047c348).
// 0x0047c360  mouseDispatch250: callback +0x250, hit 0x38 (0x0047c3df), miss 0x80 (0x0047c3ec), release 0xb0 (0x0047c415),
//   panels +0x2c (0x0047c3f2) / +0x60 (0x0047c41b).
// 0x0047c500  mouseDispatch258: callback +0x258, hit 0x4c (0x0047c57f), miss 0x88 (0x0047c58c), release 0xb8 (0x0047c5b5),
//   panels +0x34 (0x0047c592) / +0x68 (0x0047c5bb).
typedef MouseD<0x23c, 0x34, 0x98, 0xbc, 0x44, 0x6c> M23c;
typedef MouseD<0x240, 0x30, 0x9c, 0xc0, 0x48, 0x70> M240;
typedef MouseD<0x244, 0x3c, 0x74, 0xa4, 0x20, 0x54> M244;
typedef MouseD<0x24c, 0x48, 0x7c, 0xac, 0x28, 0x5c> M24c;
typedef MouseD<0x250, 0x38, 0x80, 0xb0, 0x2c, 0x60> M250;
typedef MouseD<0x258, 0x4c, 0x88, 0xb8, 0x34, 0x68> M258;

// 0x0047c1b0  Window::click(x, y, release): the same flow with callback +0x248 (0x0047c1ec-0x0047c1f8), hit slot 0x40
// (0x0047c245), miss slot 0x78 (0x0047c252), release slot 0xa8 (0x0047c270), panels +0x24 (0x0047c255) / +0x58
// (0x0047c276), plus the global cdecl callback at 0x0083ab04: after the window callback, or at once on a release, it runs
// with (x, y) when non-null (mov eax, [0x83ab04] at 0x0047c1fd, call eax at 0x0047c207).
WinMouse_t WindowClick_orig;
void __fastcall WindowClick_re(void* self, void*, int x, int y, int release) {
    if (WinIgnores(self)) return;                                            // 0x0047c1b8-0x0047c1cf
    if (release == 0) {                                                      // 0x0047c1e2 / 0x0047c1e4
        *reinterpret_cast<void**>(0x0083ab2c) = self;                        // 0x0047c1e6
        Cb2_t cb = At<Cb2_t>(self, 0x248);
        if (cb != 0) cb(x, y);                                               // 0x0047c1ec-0x0047c1f8
    }
    Cb2_t g = *reinterpret_cast<Cb2_t*>(0x0083ab04);
    if (g != 0) g(x, y);                                                     // 0x0047c1fd-0x0047c207
    if (release != 0) {                                                      // 0x0047c20d / 0x0047c210
        reinterpret_cast<Thiscall2_t>(Vtbl(self)[0xa8 / 4])(self, nullptr, x, y);   // 0x0047c26a-0x0047c270
        NotifyPanel(At<void*>(self, 0x58));                                  // 0x0047c276
        return;
    }
    int a = x, b = y;
    int rect[4];
    const int hit = kHitTestRect(static_cast<char*>(self) + 0xbc, nullptr, x, y, &a, &b, rect);   // 0x0047c223
    bool handled = false;
    if (hit >= 0)                                                            // 0x0047c22e / 0x0047c230
        handled = reinterpret_cast<Thiscall3_t>(Vtbl(self)[0x40 / 4])(self, nullptr, a, b, reinterpret_cast<int>(rect)) != 0;   // 0x0047c245
    if (!handled) reinterpret_cast<Thiscall2_t>(Vtbl(self)[0x78 / 4])(self, nullptr, x, y);   // 0x0047c248-0x0047c252
    NotifyPanel(At<void*>(self, 0x24));                                      // 0x0047c255-0x0047c25e
}

// ===================================================================================================
// Part 5 - thin wrappers over a device object (the devices are the fixtures' own recording objects; the live display
// 0x0083ad50 is swapped for one under a frozen first thread in the Surface keys).
// ===================================================================================================

// 0x00478b30  Widget_clearArea(this, v): the device pointer at +4 (mov ecx, [ecx + 4], 0x00478b30); a null device returns
// at once (test / je 0x00478b43). Otherwise its virtual slot 0x44 (call [eax + 0x44], 0x00478b40) runs with (0, v) (push edx
// = v, push 0, 0x00478b3d-0x00478b3e); ret 4. Returns nothing the callers read (0x0047d610, 0x0047d850, 0x004808c0,
// 0x004810d0 call it as a statement).
typedef void(__fastcall* Widget1_t)(void* ecx, void* edx, int v);                 // ret 4
Widget1_t WidgetClearArea_orig;
void __fastcall WidgetClearArea_re(void* self, void*, int v) {
    void* dev = At<void*>(self, 4);                                              // 0x00478b30
    if (dev == 0) return;                                                        // 0x00478b33-0x00478b35
    reinterpret_cast<Thiscall2_t>(Vtbl(dev)[0x44 / 4])(dev, nullptr, 0, v);      // 0x00478b3b-0x00478b40
}

// 0x004845d0  Snd::queryDevice4845d0(): the device at +0x40 (mov ecx, [ecx + 0x40], 0x004845d0); null returns 0 (xor eax,
// eax, 0x004845dc); otherwise a tail jump to its virtual slot 0x5c (jmp [eax + 0x5c], 0x004845d9), whose result is returned.
typedef int(__fastcall* SndQuery_t)(void* ecx, void* edx);
SndQuery_t SndQueryDevice_orig;
int __fastcall SndQueryDevice_re(void* self, void*) {
    void* dev = At<void*>(self, 0x40);                                           // 0x004845d0
    if (dev == 0) return 0;                                                      // 0x004845d3-0x004845dc
    return reinterpret_cast<Thiscall0_t>(Vtbl(dev)[0x5c / 4])(dev, nullptr);     // 0x004845d7-0x004845d9
}

// 0x004845e0  Snd::status(): the device at +0x40; null returns 0x14 (mov eax, 0x14 at 0x004845e3, test / je 0x004845f1);
// otherwise the result of its virtual slot 0x20 (tail jump, 0x004845ee).
SndQuery_t SndStatus_orig;
int __fastcall SndStatus_re(void* self, void*) {
    void* dev = At<void*>(self, 0x40);                                           // 0x004845e0
    if (dev == 0) return 0x14;                                                   // 0x004845e3-0x004845ea
    return reinterpret_cast<Thiscall0_t>(Vtbl(dev)[0x20 / 4])(dev, nullptr);     // 0x004845ec-0x004845ee
}

// 0x004830e0  Palette::setEntries(this, table, start, count): a null table returns 3 (test ebx / jne at 0x004830f1-0x004830f7,
// mov eax, 3 at 0x004830fa). A null device at +4 (mov eax, [esi + 4], 0x00483109) is created by the object's own virtual
// slot 0 (mov eax, [esi] / call [eax], 0x00483110-0x00483114). For each of the count entries from index start
// (cmp ebp, edx / jge at 0x00483128-0x0048312a, so count <= 0 copies nothing) three bytes are copied to a 0x300-byte
// local buffer in reverse order from the four-byte entry at table + 0x28 + 4 * index: local[3 j + 0] = entry byte 2
// (0x00483139, store 0x00483143), local[3 j + 1] = byte 1 (0x00483147, 0x0048314e), local[3 j + 2] = byte 0 (0x00483152,
// 0x0048315d); j counts from 0. Then the device's virtual slot 0x14 runs with (local, start, count) (push eax = count, push
// ebp = start, push the local buffer, call [edx + 0x14], 0x00483172-0x0048317b); its result is returned.
typedef int(__fastcall* PalSet_t)(void* ecx, void* edx, const unsigned char* table, int start, int count);   // ret 0xc
PalSet_t PaletteSetEntries_orig;
int __fastcall PaletteSetEntries_re(void* self, void*, const unsigned char* table, int start, int count) {
    if (table == 0) return 3;                                                    // 0x004830f1 / 0x004830fa
    if (At<void*>(self, 4) == 0)                                                 // 0x00483109-0x0048310e
        reinterpret_cast<Thiscall0_t>(Vtbl(self)[0])(self, nullptr);             // 0x00483110-0x00483114
    unsigned char local[0x300];
    const int end = start + count;                                               // 0x00483125
    for (int k = start, j = 0; k < end; k++, j++) {                              // 0x00483128-0x0048312a / 0x0048315c
        const unsigned char* e = table + 0x28 + 4 * k;
        local[3 * j + 0] = e[2];                                                 // 0x00483139 / 0x00483143
        local[3 * j + 1] = e[1];                                                 // 0x00483147 / 0x0048314e
        local[3 * j + 2] = e[0];                                                 // 0x00483152 / 0x0048315d
    }
    void* dev = At<void*>(self, 4);                                              // 0x0048316f
    return reinterpret_cast<Thiscall3_t>(Vtbl(dev)[0x14 / 4])(dev, nullptr, reinterpret_cast<int>(local), start, count);   // 0x0048317b
}

// 0x004838b0  Surface::releaseBacking(): the handle at +4 (0x004838b6; test cmp eax, edi / je at 0x004838b9-0x004838bb) is
// released through the display g_graphicsDevice 0x0083ad50 (mov ecx, [0x83ad50] at 0x004838bd, its virtual slot 0x88 with the
// handle, 0x004838c6); then +0x10, +0xc, +0x18 and +0x1c are cleared (0x004838cc-0x004838d5) and +8 is set to -1
// (0x004838d8). Returns nothing (eax is whatever the display's slot left; the callers 0x00483800 and 0x00483850 ignore it).
inline void* GraphicsDevice() { return *reinterpret_cast<void**>(0x0083ad50); }
typedef int(__fastcall* Thiscall1_t)(void*, void*, int);
This0_t SurfaceReleaseBacking_orig;
void __fastcall SurfaceReleaseBacking_re(void* self, void*) {
    void* handle = At<void*>(self, 4);                                           // 0x004838b6
    if (handle != 0) {                                                           // 0x004838b9
        void* disp = GraphicsDevice();                                           // 0x004838bd
        reinterpret_cast<Thiscall1_t>(Vtbl(disp)[0x88 / 4])(disp, nullptr, reinterpret_cast<int>(handle));   // 0x004838c6
    }
    At<int>(self, 0x10) = 0;                                                     // 0x004838cc
    At<int>(self, 0xc) = 0;                                                      // 0x004838cf
    At<int>(self, 0x18) = 0;                                                     // 0x004838d2
    At<int>(self, 0x1c) = 0;                                                     // 0x004838d5
    At<int>(self, 8) = -1;                                                       // 0x004838d8
}

// 0x00483800  Surface::createFromId(this, id, a, b): id 0 returns 3 (test edi / jne at 0x00483808-0x0048380a, mov eax, 3 at
// 0x0048380d). Otherwise Surface::releaseBacking 0x004838b0 (call 0x00483818), then the display's virtual slot 0x74 is
// called with the ADDRESS of the handle field, this + 4 (lea eax, [esi + 4] at 0x00483821; a null this gives 0, which
// faults at the store 0x00483838 anyway), the returned handle is stored at +4 (0x00483838) and its virtual slot 8 runs
// with (id, a, b) (push ecx / push ecx / push edi, call [edx + 8], 0x0048383b-0x00483846); its result is returned.
typedef int(__fastcall* SurfCreate3_t)(void* ecx, void* edx, int id, int a, int b);                 // ret 0xc
typedef void*(__fastcall* DispCreate_t)(void* ecx, void* edx, void* slot);
SurfCreate3_t SurfaceCreateFromId_orig;
int __fastcall SurfaceCreateFromId_re(void* self, void*, int id, int a, int b) {
    if (id == 0) return 3;                                                       // 0x00483808-0x0048380d
    reinterpret_cast<This0_t>(0x004838b0)(self, nullptr);                        // 0x00483818
    void* disp = GraphicsDevice();                                               // 0x00483828
    void* handle = reinterpret_cast<DispCreate_t>(Vtbl(disp)[0x74 / 4])(disp, nullptr, static_cast<char*>(self) + 4);   // 0x00483831
    At<void*>(self, 4) = handle;                                                 // 0x00483838
    return reinterpret_cast<Thiscall3_t>(Vtbl(handle)[8 / 4])(handle, nullptr, id, a, b);               // 0x00483846
}

// 0x00483850  Surface::createSized(this, w, h, a, b): a zero w (test ebx / je 0x0048385b) or zero h (test edi / je
// 0x00483863) returns 0x10 (mov eax, 0x10, 0x0048389f). Otherwise releaseBacking (call 0x00483865), display slot 0x74 with
// &this->+4, store the handle (0x00483885) and its virtual slot 4 with (w, h, a, b) (push ecx / push ecx / push edi /
// push ebx, call [edx + 4], 0x00483888-0x00483894); its result is returned.
typedef int(__fastcall* SurfCreate4_t)(void* ecx, void* edx, int w, int h, int a, int b);           // ret 0x10
typedef int(__fastcall* Thiscall4_t)(void*, void*, int, int, int, int);
SurfCreate4_t SurfaceCreateSized_orig;
int __fastcall SurfaceCreateSized_re(void* self, void*, int w, int h, int a, int b) {
    if (w == 0 || h == 0) return 0x10;                                           // 0x00483857-0x00483863 / 0x0048389f
    reinterpret_cast<This0_t>(0x004838b0)(self, nullptr);                        // 0x00483865
    void* disp = GraphicsDevice();                                               // 0x00483875
    void* handle = reinterpret_cast<DispCreate_t>(Vtbl(disp)[0x74 / 4])(disp, nullptr, static_cast<char*>(self) + 4);   // 0x0048387e
    At<void*>(self, 4) = handle;                                                 // 0x00483885
    return reinterpret_cast<Thiscall4_t>(Vtbl(handle)[4 / 4])(handle, nullptr, w, h, a, b);             // 0x00483894
}

// ===================================================================================================
// Part 6 - the sound object wrappers (Snd). The sound function table g_soundFns at 0x0083af6c (entry 0 = create, entry 1 =
// release, filled by the loader of sound.dll's exports) and the library flag g_soundLib 0x0083afc4 are replaced by recording
// callbacks under a frozen first thread in the fixtures; the objects are the fixtures' own. Nothing reaches sound.dll.
// ===================================================================================================

typedef int(__cdecl* SndFn1_t)(int);
typedef int(__cdecl* SndFn3_t)(int, int, int);

// 0x00484110  releaseSoundObj(p): without the sound library (g_soundLib 0x0083afc4 zero, test 0x00484115, `jne` 0x00484117)
// it returns 1 (0x00484119); otherwise the cdecl function at 0x0083af70 (table entry 1) runs with p (call [0x83af70]
// 0x00484124, `add esp, 4` 0x0048412a) and its result is returned.
typedef int(__cdecl* ReleaseSound_t)(int p);
ReleaseSound_t ReleaseSoundObj_orig;
int __cdecl ReleaseSoundObj_re(int p) {
    if (*reinterpret_cast<int*>(0x0083afc4) == 0) return 1;                      // 0x00484110-0x0048411e
    return (*reinterpret_cast<SndFn1_t*>(0x0083af70))(p);                        // 0x0048411f-0x0048412d
}

// 0x004840e0  createSound4840e0(a, b, c): without the sound library it returns 1 (0x004840e9); otherwise the cdecl function at
// 0x0083af6c (table entry 0) runs with the same three arguments in the same order (push eax = c, push ecx = b, push edx = a,
// call [0x83af6c] at 0x004840fb-0x0048410e) - the call is at 0x004840fe and the stack is cleaned at 0x00484104 and its result is returned (0x00484107).
typedef int(__cdecl* CreateSound_t)(int a, int b, int c);
CreateSound_t CreateSound4840e0_orig;
int __cdecl CreateSound4840e0_re(int a, int b, int c) {
    if (*reinterpret_cast<int*>(0x0083afc4) == 0) return 1;                      // 0x004840e0-0x004840ee
    return (*reinterpret_cast<SndFn3_t*>(0x0083af6c))(a, b, c);                  // 0x004840ef-0x00484107
}

// 0x00484130  clearSoundPtrs(): zeroes the 11 dwords from 0x0083af6c (`mov ecx, 0xb`, `mov edi, 0x83af6c`, `rep stosd` with
// eax = 0, 0x00484131-0x0048413d), i.e. the function table; returns nothing (eax is 0 from `xor eax, eax`, 0x00484136).
Void0C_t ClearSoundPtrs_orig;
void __cdecl ClearSoundPtrs_re() {
    int* p = reinterpret_cast<int*>(0x0083af6c);
    for (int i = 0; i < 11; i++) p[i] = 0;                                       // 0x0048413d
}

// 0x004844e0  Snd::unlinkFromList(): a sound object whose flag bit 2 (`test byte ptr [ecx + 0x44], 2`, 0x004844e0) is clear
// returns 0 (0x004844e6). Otherwise it leaves the global doubly linked list (prev +0x48, next +0x4c; head 0x0083af68, tail
// 0x0083af64): with a previous node that node's next = this next (0x004844ef-0x004844f6), else the head = this next
// (0x004844fb-0x004844fe); with a next node that node's prev = this prev (0x0048450a-0x0048450d), else the tail = this prev
// (0x00484522-0x00484526); bit 2 is cleared in the low byte of +0x44 (`and al, 0xfd` 0x00484513 / 0x0048452e) and +0x4c, +0x48
// are zeroed (0x00484515, 0x0048451b / 0x00484530, 0x00484536). Returns 0 on every path (`xor eax, eax` 0x0048451e /
// 0x00484539).
typedef int(__fastcall* Snd0_t)(void* ecx, void* edx);
Snd0_t SndUnlinkFromList_orig;
int __fastcall SndUnlinkFromList_re(void* self, void*) {
    if ((At<unsigned char>(self, 0x44) & 2) == 0) return 0;                      // 0x004844e0 / 0x004844e4 / 0x004844e6
    void* prev = At<void*>(self, 0x48);                                          // 0x004844e9
    void* next = At<void*>(self, 0x4c);
    if (prev != 0) At<void*>(prev, 0x4c) = next;                                 // 0x004844ee / 0x004844f1 / 0x004844f6
    else *reinterpret_cast<void**>(0x0083af68) = next;                           // 0x004844fb-0x004844fe
    next = At<void*>(self, 0x4c);                                                // 0x00484503
    if (next != 0) {                                                             // 0x00484508
        At<void*>(next, 0x48) = At<void*>(self, 0x48);                           // 0x0048450a-0x0048450d
    } else {
        *reinterpret_cast<void**>(0x0083af64) = At<void*>(self, 0x48);           // 0x00484522-0x00484526
    }
    unsigned flags = At<unsigned>(self, 0x44);                                   // 0x00484510 / 0x0048452b
    flags = (flags & ~0xffu) | ((flags & 0xfdu) & 0xffu);                        // and al, 0xfd
    At<void*>(self, 0x4c) = 0;                                                   // 0x00484515 / 0x00484530
    At<unsigned>(self, 0x44) = flags;                                            // 0x00484518 / 0x00484533
    At<void*>(self, 0x48) = 0;                                                   // 0x0048451b / 0x00484536
    return 0;                                                                    // 0x0048451e / 0x00484539
}

// 0x00484550  Snd::stop484550(): the device at +0x40 (0x00484556; edi = 0 from 0x00484554): when non-null its virtual slot 0x14
// runs (call [eax + 0x14] 0x0048455f) and the result is kept in edi (0x00484562); then the object's own virtual slot 0x80
// runs (call [edx + 0x80] 0x00484568), +0x40 is cleared (0x00484571), bit 0 of the low byte of +0x44 is cleared (`and al, 0xfe`
// 0x00484578, store 0x0048457a) and edi is returned (0x0048457d).
Snd0_t SndStop_orig;
int __fastcall SndStop_re(void* self, void*) {
    int result = 0;                                                              // 0x00484554
    void* dev = At<void*>(self, 0x40);                                           // 0x00484556
    if (dev != 0)                                                                // 0x00484559 / 0x0048455b
        result = reinterpret_cast<Thiscall0_t>(Vtbl(dev)[0x14 / 4])(dev, nullptr);   // 0x0048455f
    reinterpret_cast<Thiscall0_t>(Vtbl(self)[0x80 / 4])(self, nullptr);          // 0x00484568
    unsigned flags = At<unsigned>(self, 0x44);                                   // 0x0048456e
    At<void*>(self, 0x40) = 0;                                                   // 0x00484571
    flags = (flags & ~0xffu) | ((flags & 0xfeu) & 0xffu);                        // and al, 0xfe
    At<unsigned>(self, 0x44) = flags;                                            // 0x0048457a
    return result;                                                               // 0x0048457d
}

// 0x00484750  Snd::applyField484750(a): the object's virtual slot 4 runs with a (call [eax + 4] 0x0048475c); a zero answer
// (test 0x0048475f, `jne` 0x00484761) runs virtual slot 0x28 (call [edx + 0x28] 0x00484767) and returns its result;
// a non-zero answer is returned unchanged (eax kept to 0x0048476b, `ret 4`). startSoundSlot 0x00448200 returns it (`ret`
// at 0x0044821b right after the call).
typedef int(__fastcall* Snd1_t)(void* ecx, void* edx, int a);
Snd1_t SndApplyField484750_orig;
int __fastcall SndApplyField484750_re(void* self, void*, int a) {
    int r = reinterpret_cast<Thiscall1_t>(Vtbl(self)[4 / 4])(self, nullptr, a);  // 0x0048475c
    if (r == 0)                                                                  // 0x0048475f / 0x00484761
        r = reinterpret_cast<Thiscall0_t>(Vtbl(self)[0x28 / 4])(self, nullptr);  // 0x00484767
    return r;                                                                    // 0x0048476b
}

// 0x004847b0  Snd::applyField4847b0(a): the same shape with virtual slot 0x54 (call [eax + 0x54] 0x004847bc) and the fallback
// slot 0x28 (call [edx + 0x28] 0x004847c7); `ret 4` at 0x004847cb.
Snd1_t SndApplyField4847b0_orig;
int __fastcall SndApplyField4847b0_re(void* self, void*, int a) {
    int r = reinterpret_cast<Thiscall1_t>(Vtbl(self)[0x54 / 4])(self, nullptr, a);   // 0x004847bc
    if (r == 0)                                                                  // 0x004847bf / 0x004847c1
        r = reinterpret_cast<Thiscall0_t>(Vtbl(self)[0x28 / 4])(self, nullptr);  // 0x004847c7
    return r;                                                                    // 0x004847cb
}

// 0x00484e30  Snd::applyMode484e30(a, mode): the object's virtual slot 0x88 runs with (a, mode) (push ebx = mode, push ecx = a,
// call [eax + 0x88] 0x00484e3c-0x00484e42); a mode with bit 2 (`test bl, 4` 0x00484e48, `je` 0x00484e4b) returns 0
// (0x00484e4e); otherwise virtual slot 0x8c runs (call [edx + 0x8c] 0x00484e58) and its result is returned (`ret 8`
// 0x00484e60).
typedef int(__fastcall* Snd2_t)(void* ecx, void* edx, int a, int mode);
Snd2_t SndApplyMode_orig;
int __fastcall SndApplyMode_re(void* self, void*, int a, int mode) {
    reinterpret_cast<Thiscall2_t>(Vtbl(self)[0x88 / 4])(self, nullptr, a, mode); // 0x00484e42
    if ((mode & 0xff) & 4) return 0;                                             // 0x00484e48 / 0x00484e4b
    return reinterpret_cast<Thiscall0_t>(Vtbl(self)[0x8c / 4])(self, nullptr);   // 0x00484e58
}

// 0x00448200  startSoundSlot(i): Snd::applyField484750 0x00484750 on the slot object at 0x0080d840 + 108 * i (i * 27 via two
// `lea` at 0x00448209-0x0044820c, then `lea ecx, [eax * 4 + 0x80d840]` 0x0044820f) with 0x3e8 (push 0x3e8 0x00448204, call
// 0x00448216); its result is returned (`ret` 0x0044821b).
typedef int(__cdecl* StartSoundSlot_t)(int i);
StartSoundSlot_t StartSoundSlot_orig;
int __cdecl StartSoundSlot_re(int i) {
    void* obj = reinterpret_cast<void*>(0x0080d840u + static_cast<unsigned>(i) * 27u * 4u);   // 0x00448209-0x0044820f
    return reinterpret_cast<Thiscall1_t>(0x00484750)(obj, nullptr, 0x3e8);       // 0x00448216
}

}  // namespace

// 0x004747c0  Buf_deleteDtor
SG_HOOK("golf_clean.exe", 0x004747c0, Buf_deleteDtor, BufDeleteDtor_re, BufDeleteDtor_orig);
// 0x00487240  StrList::scalarDtor
SG_HOOK("golf_clean.exe", 0x00487240, StrList_scalarDtor, StrListScalarDtor_re, StrListScalarDtor_orig);
// 0x0049d070  pairArray_deleteDtor
SG_HOOK("golf_clean.exe", 0x0049d070, pairArray_deleteDtor, PairArrayDeleteDtor_re, PairArrayDeleteDtor_orig);
// 0x004a0160  loggerDeleteDtor
SG_HOOK("golf_clean.exe", 0x004a0160, loggerDeleteDtor, LoggerDeleteDtor_re, LoggerDeleteDtor_orig);
// 0x004882d0  KeyTable::scalarDtor
SG_HOOK("golf_clean.exe", 0x004882d0, KeyTable_scalarDtor, KeyTableScalarDtor_re, KeyTableScalarDtor_orig);
// 0x00487390  NodeList::dtor
SG_HOOK("golf_clean.exe", 0x00487390, NodeList_dtor, NodeListDtor_re, NodeListDtor_orig);
// 0x004872f0  NodeList::scalarDtor
SG_HOOK("golf_clean.exe", 0x004872f0, NodeList_scalarDtor, NodeListScalarDtor_re, NodeListScalarDtor_orig);
// 0x00487a40  NodeListB::scalarDtor
SG_HOOK("golf_clean.exe", 0x00487a40, NodeListB_scalarDtor, NodeListBScalarDtor_re, NodeListBScalarDtor_orig);
// 0x00487b60  NodeListC::scalarDtor
SG_HOOK("golf_clean.exe", 0x00487b60, NodeListC_scalarDtor, NodeListCScalarDtor_re, NodeListCScalarDtor_orig);
// 0x00487ce0  NodeList::clearScalarDtor
SG_HOOK("golf_clean.exe", 0x00487ce0, NodeList_clearScalarDtor, NodeListClearScalarDtor_re, NodeListClearScalarDtor_orig);
// 0x00480390  List480::dtor
SG_HOOK("golf_clean.exe", 0x00480390, List480_dtor, List480Dtor_re, List480Dtor_orig);
// 0x00480410  BinkFrame::scalarDtor
SG_HOOK("golf_clean.exe", 0x00480410, BinkFrame_scalarDtor, BinkFrameScalarDtor_re, BinkFrameScalarDtor_orig);
// 0x00401c30  deletingDtor_401c30
SG_HOOK("golf_clean.exe", 0x00401c30, deletingDtor_401c30, DeletingDtor401c30_re, DeletingDtor401c30_orig);
// 0x004a4d30  listClearThunk
SG_HOOK("golf_clean.exe", 0x004a4d30, listClearThunk, ListClearThunk_re, ListClearThunk_orig);
// 0x004876c0  HashTable::remove
SG_HOOK("golf_clean.exe", 0x004876c0, HashTable_remove, HashTableRemove_re, HashTableRemove_orig);
// 0x004673e0  genNoise
SG_HOOK("golf_clean.exe", 0x004673e0, genNoise, GenNoise_re, GenNoise_orig);
// 0x0042f2c0  markTileKind11
SG_HOOK("golf_clean.exe", 0x0042f2c0, markTileKind11, MarkTileKind11_re, MarkTileKind11_orig);
// 0x0042f120  floodMark
SG_HOOK("golf_clean.exe", 0x0042f120, floodMark, FloodMark_re, FloodMark_orig);
// 0x004ae910  jpeg_default_colorspace
SG_HOOK("golf_clean.exe", 0x004ae910, jpeg_default_colorspace, JpegDefaultColorspace_re, JpegDefaultColorspace_orig);
// 0x004ae760  jpeg_set_defaults
SG_HOOK("golf_clean.exe", 0x004ae760, jpeg_set_defaults, JpegSetDefaults_re, JpegSetDefaults_orig);
// 0x004ae400  jpeg_write_scanlines
SG_HOOK("golf_clean.exe", 0x004ae400, jpeg_write_scanlines, JpegWriteScanlines_re, JpegWriteScanlines_orig);
// 0x004ae380  jpeg_start_compress
SG_HOOK("golf_clean.exe", 0x004ae380, jpeg_start_compress, JpegStartCompress_re, JpegStartCompress_orig);
// 0x004ae270  jpeg_finish_compress
SG_HOOK("golf_clean.exe", 0x004ae270, jpeg_finish_compress, JpegFinishCompress_re, JpegFinishCompress_orig);
// 0x0047bf40  Window::mouseDispatch23c
SG_HOOK("golf_clean.exe", 0x0047bf40, Window_mouseDispatch23c, M23c::re, M23c::orig);
// 0x0047c010  Window::mouseDispatch240
SG_HOOK("golf_clean.exe", 0x0047c010, Window_mouseDispatch240, M240::re, M240::orig);
// 0x0047c0e0  Window::mouseDispatch244
SG_HOOK("golf_clean.exe", 0x0047c0e0, Window_mouseDispatch244, M244::re, M244::orig);
// 0x0047c1b0  Window::click
SG_HOOK("golf_clean.exe", 0x0047c1b0, Window_click, WindowClick_re, WindowClick_orig);
// 0x0047c290  Window::mouseDispatch24c
SG_HOOK("golf_clean.exe", 0x0047c290, Window_mouseDispatch24c, M24c::re, M24c::orig);
// 0x0047c360  Window::mouseDispatch250
SG_HOOK("golf_clean.exe", 0x0047c360, Window_mouseDispatch250, M250::re, M250::orig);
// 0x0047c500  Window::mouseDispatch258
SG_HOOK("golf_clean.exe", 0x0047c500, Window_mouseDispatch258, M258::re, M258::orig);
// 0x00478b30  Widget_clearArea
SG_HOOK("golf_clean.exe", 0x00478b30, Widget_clearArea, WidgetClearArea_re, WidgetClearArea_orig);
// 0x004845d0  Snd::queryDevice4845d0
SG_HOOK("golf_clean.exe", 0x004845d0, Snd_queryDevice4845d0, SndQueryDevice_re, SndQueryDevice_orig);
// 0x004845e0  Snd::status
SG_HOOK("golf_clean.exe", 0x004845e0, Snd_status, SndStatus_re, SndStatus_orig);
// 0x004830e0  Palette::setEntries
SG_HOOK("golf_clean.exe", 0x004830e0, Palette_setEntries, PaletteSetEntries_re, PaletteSetEntries_orig);
// 0x004838b0  Surface::releaseBacking
SG_HOOK("golf_clean.exe", 0x004838b0, Surface_releaseBacking, SurfaceReleaseBacking_re, SurfaceReleaseBacking_orig);
// 0x00483800  Surface::createFromId
SG_HOOK("golf_clean.exe", 0x00483800, Surface_createFromId, SurfaceCreateFromId_re, SurfaceCreateFromId_orig);
// 0x00483850  Surface::createSized
SG_HOOK("golf_clean.exe", 0x00483850, Surface_createSized, SurfaceCreateSized_re, SurfaceCreateSized_orig);
// 0x00484110  releaseSoundObj
SG_HOOK("golf_clean.exe", 0x00484110, releaseSoundObj, ReleaseSoundObj_re, ReleaseSoundObj_orig);
// 0x004840e0  createSound4840e0
SG_HOOK("golf_clean.exe", 0x004840e0, createSound4840e0, CreateSound4840e0_re, CreateSound4840e0_orig);
// 0x00484130  clearSoundPtrs
SG_HOOK("golf_clean.exe", 0x00484130, clearSoundPtrs, ClearSoundPtrs_re, ClearSoundPtrs_orig);
// 0x004844e0  Snd::unlinkFromList
SG_HOOK("golf_clean.exe", 0x004844e0, Snd_unlinkFromList, SndUnlinkFromList_re, SndUnlinkFromList_orig);
// 0x00484550  Snd::stop484550
SG_HOOK("golf_clean.exe", 0x00484550, Snd_stop484550, SndStop_re, SndStop_orig);
// 0x00484750  Snd::applyField484750
SG_HOOK("golf_clean.exe", 0x00484750, Snd_applyField484750, SndApplyField484750_re, SndApplyField484750_orig);
// 0x004847b0  Snd::applyField4847b0
SG_HOOK("golf_clean.exe", 0x004847b0, Snd_applyField4847b0, SndApplyField4847b0_re, SndApplyField4847b0_orig);
// 0x00484e30  Snd::applyMode484e30
SG_HOOK("golf_clean.exe", 0x00484e30, Snd_applyMode484e30, SndApplyMode_re, SndApplyMode_orig);
// 0x00448200  startSoundSlot
SG_HOOK("golf_clean.exe", 0x00448200, startSoundSlot, StartSoundSlot_re, StartSoundSlot_orig);
