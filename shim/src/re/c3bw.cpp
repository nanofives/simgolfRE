// Batch c3bw: jgld.dll non-render.
//   Quat::ctor                0x00003050   (lead, log/c3/c3bq_notes.txt "Vetted leads")
//   Quat::setIdentity         0x00003170   (lead)
//   Matrix::dtor              0x000032f0   (lead)
//   Matrix::ctor              0x00004590   (lead)
//   Transform::ctor           0x00005450
//   Transform::dtor           0x000034e0
//   LinkedList::ctor          0x00006c80   (lead; the gate holds it, see log/c3/c3bw_notes.txt)
//   LinkedList::dtor          0x00006cf0
//   LinkedList::clear         0x00006df0
//   LinkedList::removeCurrent 0x00007100
//   inflate_trees_free        0x0009f250
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the
// form the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so
// these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook
// finds them by (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall:
// ecx = this, edx unused.
//
// Nothing live in the running jgld.dll is handed to these bodies: the quaternions, matrices,
// transforms, lists, list nodes, huft chains and the z_stream all come from the fixtures
// (re/frida/js/fixtures.d/c3bw.js).
//
// Two of these functions destroy objects through a function pointer the OBJECT carries, which is why
// they can be A/B'd at all:
//   * LinkedList::clear 0x10006df0 and LinkedList::removeCurrent 0x10007100 reach `delete node`
//     through slot 0 of the node's own vtable (0x10006e70, 0x10006ea8, 0x1000715d, 0x10007224), the
//     compiler-generated scalar deleting destructor. The fixture's nodes carry a vtable of our own
//     whose slot 0 only marks the node, so nothing is handed to operator delete and the two arms
//     cannot double-free. This is what c3bq's notes rejected these functions for.
//   * inflate_trees_free 0x1009f250 frees through the z_stream's zfree at +0x24 with the opaque at
//     +0x28 (0x1009f2bc-0x1009f2c3); the fixture's z_stream carries a marker of our own there.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so an address the code stores or calls is resolved against the running base.
unsigned char* JgldBase() {
    static unsigned char* base = 0;
    if (!base) base = reinterpret_cast<unsigned char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000u); }

inline unsigned& U32(void* p, unsigned off) {
    return *reinterpret_cast<unsigned*>(static_cast<char*>(p) + off);
}
inline unsigned U32R(const void* p, unsigned off) {
    return *reinterpret_cast<const unsigned*>(static_cast<const char*>(p) + off);
}
inline void*& P32(void* p, unsigned off) {
    return *reinterpret_cast<void**>(static_cast<char*>(p) + off);
}
inline void* P32R(const void* p, unsigned off) {
    return *reinterpret_cast<void* const*>(static_cast<const char*>(p) + off);
}

// Vector3::set(x, y, z), 0x100036e0: three integer moves into [this], [this+4], [this+8] (c3ap).
typedef void(__fastcall* Vec3Set_t)(void*, void*, float, float, float);

// ------------------------------------------------------------------------------ Quat::ctor 0x00003050
//
// Quat::Quat(): __thiscall, no stack argument (`ret` at 0x10003098), `this` spilled to [ebp-4] at
// 0x1000306a. It pushes three integer zeros (0x1000306d, 0x1000306f, 0x10003071) and calls
// Vector3::set on `this` (0x10003076, the incremental-link thunk 0x10001960 -> 0x100036e0), which
// stores those three dwords at [this], [this+4] and [this+8]; the bit pattern 0 is +0.0f. It then
// writes the dword 0x3f800000 - 1.0f as an integer immediate - at [this+0xc] (0x1000307e) and returns
// `this` in eax (0x1000307b, 0x10003085). No branch, nothing read from the object.
typedef void*(__fastcall* QuatCtor_t)(void*, void*);
QuatCtor_t QuatCtor_orig;

void* __fastcall QuatCtor_re(void* self, void*) {
    reinterpret_cast<Vec3Set_t>(JgldVA(0x100036e0))(self, 0, 0.0f, 0.0f, 0.0f);
    U32(self, 0x0c) = 0x3f800000u;
    return self;
}

// ----------------------------------------------------------------------- Quat::setIdentity 0x00003170
//
// The same body as Quat::ctor at a different address: three pushed zeros (0x1000318d, 0x1000318f,
// 0x10003191) into Vector3::set on `this` (0x10003196) and the dword 0x3f800000 at [this+0xc]
// (0x1000319e). The one difference is the epilogue: the constructor reloads `this` into eax at
// 0x10003085 after the store, this function does not (0x100031a5 pops straight into the epilogue), so
// it has no declared return value. eax does still hold `this` when it returns, because 0x1000319b
// loaded it to address the store; the registry entry therefore declares ret="void" and the A/B
// compares memory only. No branch.
typedef void(__fastcall* QuatSetIdentity_t)(void*, void*);
QuatSetIdentity_t QuatSetIdentity_orig;

void __fastcall QuatSetIdentity_re(void* self, void*) {
    reinterpret_cast<Vec3Set_t>(JgldVA(0x100036e0))(self, 0, 0.0f, 0.0f, 0.0f);
    U32(self, 0x0c) = 0x3f800000u;
}

// ---------------------------------------------------------------------------- Matrix::ctor 0x00004590
//
// Matrix::Matrix(): __thiscall, no stack argument (`ret` at 0x100045d1). Two steps and nothing else:
// the dword at [this] is set to the Matrix vtable, VA 0x1011d024 (0x100045b0), and Matrix::loadIdentity
// is called on `this` (0x100045b9, thunk 0x100016db -> 0x100045f0), which zeroes the sixteen cells at
// [this+4] .. [this+0x40] and writes 1.0f into the four diagonal ones. `this` is returned in eax
// (0x100045be). No branch. The vtable address is a relocated constant, so it is resolved against the
// running base here.
typedef void*(__fastcall* MatrixCtor_t)(void*, void*);
MatrixCtor_t MatrixCtor_orig;

typedef void(__fastcall* MatrixLoadIdentity_t)(void*, void*);

void* __fastcall MatrixCtor_re(void* self, void*) {
    P32(self, 0) = JgldVA(0x1011d024);
    reinterpret_cast<MatrixLoadIdentity_t>(JgldVA(0x100045f0))(self, 0);
    return self;
}

// ---------------------------------------------------------------------------- Matrix::dtor 0x000032f0
//
// Matrix::~Matrix(), the virtual destructor (`??1Matrix@@UAE@XZ`, re/match/jgld_math.cpp): the same two
// steps as the constructor - the vtable at [this] (0x10003310) and Matrix::loadIdentity on `this`
// (0x10003319, the same thunk 0x100016db) - and no reload of `this` into eax afterwards, so it has no
// return value (`ret` at 0x1000332e). It calls no deallocator: the object's storage is not touched
// beyond the sixteen cells loadIdentity rewrites.
typedef void(__fastcall* MatrixDtor_t)(void*, void*);
MatrixDtor_t MatrixDtor_orig;

void __fastcall MatrixDtor_re(void* self, void*) {
    P32(self, 0) = JgldVA(0x1011d024);
    reinterpret_cast<MatrixLoadIdentity_t>(JgldVA(0x100045f0))(self, 0);
}

// ------------------------------------------------------------------------- Transform::ctor 0x00005450
//
// Transform::Transform(): __thiscall, no stack argument (`ret` at 0x1000549c). Three steps, in this
// order: Quat::ctor on the quaternion sub-object at this+4 (`add ecx,4` at 0x10005470, call 0x10005473
// through the thunk 0x100013e8 -> 0x10003050), the Transform vtable VA 0x1011d028 at [this]
// (0x1000547b), and Transform::reset on `this` (0x10005484, thunk 0x100017e9 -> 0x10005650), which
// clears the flags word at [this+0x20]. `this` is returned in eax (0x10005489). No branch.
//
// The position Vector3 at this+0x14 is NOT written by this body: the twelve bytes between the
// quaternion and the flags word keep whatever they held, which the A/B's state regions cover.
typedef void*(__fastcall* XformCtor_t)(void*, void*);
XformCtor_t XformCtor_orig;

typedef void(__fastcall* XformReset_t)(void*, void*);

void* __fastcall XformCtor_re(void* self, void*) {
    reinterpret_cast<QuatCtor_t>(JgldVA(0x10003050))(static_cast<char*>(self) + 4, 0);
    P32(self, 0) = JgldVA(0x1011d028);
    reinterpret_cast<XformReset_t>(JgldVA(0x10005650))(self, 0);
    return self;
}

// ------------------------------------------------------------------------- Transform::dtor 0x000034e0
//
// Transform::~Transform(): the vtable VA 0x1011d028 at [this] (0x10003500) and Transform::reset on
// `this` (0x10003509, the same thunk 0x100017e9), and nothing else (`ret` at 0x1000351e, no reload of
// `this` into eax). The quaternion at this+4 is not destroyed and no deallocator is called.
typedef void(__fastcall* XformDtor_t)(void*, void*);
XformDtor_t XformDtor_orig;

void __fastcall XformDtor_re(void* self, void*) {
    P32(self, 0) = JgldVA(0x1011d028);
    reinterpret_cast<XformReset_t>(JgldVA(0x10005650))(self, 0);
}

// ---------------------------------------------------------------------- the LinkedList class
//
// Field offsets, all read off the four bodies below: vptr +0, m_data +4 (set from a node's data at
// 0x100071d6), m_flag +8 (a byte, set from a node's flag at 0x100071e5), m_count +0xc (0x10006cbd,
// 0x10006eca, 0x10007245), m_head +0x10 (0x10006ca9, 0x10006e10, 0x10007120), m_cur +0x14 (0x10006cb3,
// 0x10006ede, 0x100071c7) and m_ordered +0x18 (0x10006ec0).
// ListNode: vptr +0, prev +4 (0x10007137), next +8 (0x10006e21), data +0xc (0x100071d3) and flag as a
// byte at +0x10 (0x100071e2).
//
// `delete node` is slot 0 of the node's own vtable called with the argument 1 (the scalar deleting
// destructor): `push 1` / `mov edx,[node]` / `call [edx]` at 0x10006e66-0x10006e70, 0x10006e9e-
// 0x10006ea8, 0x10007153-0x1000715d and 0x1000721a-0x10007224. The fixture's nodes carry a vtable of
// ours, so this call marks the node instead of freeing it.
typedef void*(__fastcall* NodeDelete_t)(void*, void*, int);

void DeleteNode(void* node) {
    void* vtbl = P32R(node, 0);
    reinterpret_cast<NodeDelete_t>(*static_cast<void**>(vtbl))(node, 0, 1);
}

// ------------------------------------------------------------------------ LinkedList::ctor 0x00006c80
//
// LinkedList::LinkedList(): __thiscall, no stack argument (`ret` at 0x10006ccd), no callee at all.
// Four stores: the LinkedList vtable VA 0x1011d05c at [this] (0x10006ca0), then m_head = 0
// (0x10006ca9), m_cur = 0 (0x10006cb3) and m_count = 0 (0x10006cbd), in that order. m_data, m_flag and
// m_ordered are left untouched, which the state region covers. `this` is returned in eax (0x10006cc4).
// No branch.
typedef void*(__fastcall* ListCtor_t)(void*, void*);
ListCtor_t ListCtor_orig;

void* __fastcall ListCtor_re(void* self, void*) {
    P32(self, 0) = JgldVA(0x1011d05c);
    U32(self, 0x10) = 0;
    U32(self, 0x14) = 0;
    U32(self, 0x0c) = 0;
    return self;
}

// ------------------------------------------------------------------------ LinkedList::dtor 0x00006cf0
//
// LinkedList::~LinkedList(): the LinkedList vtable VA 0x1011d05c at [this] (0x10006d10) and then
// LinkedList::clear on `this` (0x10006d19, the incremental-link thunk 0x10001500 -> 0x10006df0). No
// other store, no return value (`ret` at 0x10006d2e), no branch of its own.
typedef void(__fastcall* ListDtor_t)(void*, void*);
ListDtor_t ListDtor_orig;

typedef void(__fastcall* ListClear_t)(void*, void*);

void __fastcall ListDtor_re(void* self, void*) {
    P32(self, 0) = JgldVA(0x1011d05c);
    reinterpret_cast<ListClear_t>(JgldVA(0x10006df0))(self, 0);
}

// ----------------------------------------------------------------------- LinkedList::clear 0x00006df0
//
// __thiscall, no stack argument (`ret` at 0x10006ef5). m_head == 0 returns at once (0x10006e10 /
// `jne` 0x10006e14, the `jmp` at 0x10006e16 going to the epilogue).
//
// Otherwise it holds `first` = m_head->next in [ebp-0xc] (0x10006e21) and walks `n` = [ebp-8]:
//   n = first->next                                        0x10006e2a (entry) and 0x10006e35 (re-read)
//   while (n != m_head) {                                  cmp at 0x10006e41 / je 0x10006e44
//       first->next = n->next;                             0x10006e4c / 0x10006e4f
//       delete n;                                          guard je 0x10006e62, call 0x10006e70
//       n = first->next;                                   0x10006e32-0x10006e38
//   }
//   delete m_head;                                         guard je 0x10006e9a, call 0x10006ea8
//   m_ordered = 0; m_count = 0; m_head = 0; m_cur = 0;     0x10006ec0, 0x10006eca, 0x10006ed4, 0x10006ede
//
// Two consequences the loop shape forces, both reproduced here and measured by the vectors:
//   * the node at m_head->next is never passed to the deleting destructor unless it IS m_head (a
//     one-node list, where m_head->next points back at m_head). For any list of two or more nodes that
//     node survives the call: the loop starts at its successor and ends when it reaches m_head again.
//   * the two `delete` null guards (0x10006e62 and 0x10006e9a) can only take their null side if a node
//     pointer is null, and a null `n` faults at 0x10006e4c before the guard is reached, while a null
//     m_head has already returned at 0x10006e14. Neither null side is reachable; no vector takes them.
ListClear_t ListClear_orig;

void __fastcall ListClear_re(void* self, void*) {
    void* head = P32R(self, 0x10);
    if (head == 0) return;                                  // 0x10006e14

    void* first = P32R(head, 8);                            // 0x10006e21
    void* n = P32R(first, 8);                               // 0x10006e2a
    while (n != head) {                                     // 0x10006e41 / 0x10006e44
        P32(first, 8) = P32R(n, 8);                         // 0x10006e4f
        DeleteNode(n);                                      // 0x10006e70
        n = P32R(first, 8);                                 // 0x10006e35
    }
    DeleteNode(head);                                       // 0x10006ea8

    U32(self, 0x18) = 0;                                    // m_ordered, 0x10006ec0
    U32(self, 0x0c) = 0;                                    // m_count,   0x10006eca
    U32(self, 0x10) = 0;                                    // m_head,    0x10006ed4
    U32(self, 0x14) = 0;                                    // m_cur,     0x10006ede
}

// --------------------------------------------------------------- LinkedList::removeCurrent 0x00007100
//
// __thiscall, no stack argument (`ret` at 0x10007258). Three arms:
//   m_head == 0                      0x10007120 / `jne` 0x10007124: returns at once (jmp 0x10007126).
//   m_head == m_head->prev           0x10007137 / `jne` 0x1000713a: the one-node list. It deletes
//                                    m_head (guard je 0x1000714f, call 0x1000715d) and writes
//                                    m_head = 0 (0x10007175), m_cur = 0 (0x1000717f), m_count = 0
//                                    (0x10007189). m_data and m_flag keep their old values.
//   otherwise                        from 0x10007195:
//       if (m_head == m_cur) m_head = m_head->prev;        cmp 0x1000719e / jne 0x100071a1, store 0x100071af
//       n = m_cur;                                         0x100071b8
//       m_cur = m_cur->prev;                               0x100071c7
//       m_data = m_cur->data;                              0x100071d6
//       m_flag = m_cur->flag;   (one byte)                 0x100071e5
//       n->next->prev = n->prev;                           0x100071f4
//       n->prev->next = n->next;                           0x10007203
//       delete n;                                          guard je 0x10007216, call 0x10007224
//       m_count = m_count - 1;                             0x1000723f / 0x10007245
//
// The two `delete` null guards (0x1000714f, 0x10007216) are unreachable for the same reason as in
// clear: the first runs only after m_head was found non-null at 0x10007124, and the second only after
// [m_cur+4] was dereferenced at 0x100071c4, which faults for a null m_cur.
typedef void(__fastcall* ListRemoveCurrent_t)(void*, void*);
ListRemoveCurrent_t ListRemoveCurrent_orig;

void __fastcall ListRemoveCurrent_re(void* self, void*) {
    void* head = P32R(self, 0x10);
    if (head == 0) return;                                  // 0x10007124

    if (head == P32R(head, 4)) {                            // 0x1000713a
        DeleteNode(head);                                   // 0x1000715d
        U32(self, 0x10) = 0;                                // 0x10007175
        U32(self, 0x14) = 0;                                // 0x1000717f
        U32(self, 0x0c) = 0;                                // 0x10007189
        return;
    }

    if (P32R(self, 0x10) == P32R(self, 0x14))               // 0x100071a1
        P32(self, 0x10) = P32R(P32R(self, 0x10), 4);        // 0x100071af

    void* n = P32R(self, 0x14);                             // 0x100071b8
    P32(self, 0x14) = P32R(P32R(self, 0x14), 4);            // 0x100071c7
    P32(self, 4) = P32R(P32R(self, 0x14), 0x0c);            // 0x100071d6
    *(static_cast<unsigned char*>(self) + 8) =
        *(static_cast<const unsigned char*>(P32R(self, 0x14)) + 0x10);   // 0x100071e5

    P32(P32R(n, 8), 4) = P32R(n, 4);                        // 0x100071f4
    P32(P32R(n, 4), 8) = P32R(n, 8);                        // 0x10007203
    DeleteNode(n);                                          // 0x10007224
    U32(self, 0x0c) = U32R(self, 0x0c) - 1;                 // 0x10007245
}

// ---------------------------------------------------------------------- inflate_trees_free 0x0009f250
//
// __cdecl, two stack arguments (`ret` at 0x1009f2ea pops nothing): the table pointer t at [ebp+8] and
// the z_stream z at [ebp+0xc]. Returns 0 (`xor eax,eax` at 0x1009f2d8). zlib 1.0.2's
// inflate_trees_free (re/match/vendor/zlib-1.0.2/inftrees.c), whose huft entries are eight bytes and
// whose chain link lives in the dummy entry just below the table, i.e. at t-4.
//
// First loop (0x1009f275-0x1009f299), the list reversal: p = 0 (0x1009f268), q = t (0x1009f272); while
// q != 0 (`je` 0x1009f279) it reads r = [q-4] (0x1009f27e), writes [q-4] = p (0x1009f28a), then p = q
// (0x1009f290) and q = r (0x1009f296).
//
// Second loop (0x1009f29b-0x1009f2d6), the frees: while p != 0 (`je` 0x1009f29f) it steps p back by
// eight bytes to the block's own base (`sub ecx,8` at 0x1009f2a4), reads the next block from [p+4]
// (0x1009f2ad), calls the z_stream's zfree at [z+0x24] with the opaque at [z+0x28] and p
// (0x1009f2b9-0x1009f2c3, a __cdecl call: the caller pops the eight bytes at 0x1009f2c6) and moves on
// (0x1009f2d3).
//
// The deallocator is entirely the caller's: the fixture's z_stream carries a marker of ours at +0x24,
// so both arms "free" the same blocks and neither releases memory.
typedef int(__cdecl* InflateTreesFree_t)(void*, void*);
InflateTreesFree_t InflateTreesFree_orig;

typedef void(__cdecl* ZFree_t)(void*, void*);

int __cdecl InflateTreesFree_re(void* t, void* z) {
    char* p = 0;
    char* q = static_cast<char*>(t);
    while (q != 0) {                                        // 0x1009f279
        char* r = *reinterpret_cast<char**>(q - 4);         // 0x1009f27e
        *reinterpret_cast<char**>(q - 4) = p;               // 0x1009f28a
        p = q;                                              // 0x1009f290
        q = r;                                              // 0x1009f296
    }
    while (p != 0) {                                        // 0x1009f29f
        p -= 8;                                             // 0x1009f2a4
        char* next = *reinterpret_cast<char**>(p + 4);      // 0x1009f2ad
        reinterpret_cast<ZFree_t>(P32R(z, 0x24))(P32R(z, 0x28), p);   // 0x1009f2c3
        p = next;                                           // 0x1009f2d3
    }
    return 0;                                               // 0x1009f2d8
}

}  // namespace

// 0x00003050  Quat::ctor
SG_HOOK("jgld.dll", 0x00003050, Quat_ctor, QuatCtor_re, QuatCtor_orig);
// 0x00003170  Quat::setIdentity
SG_HOOK("jgld.dll", 0x00003170, Quat_setIdentity, QuatSetIdentity_re, QuatSetIdentity_orig);
// 0x00004590  Matrix::ctor
SG_HOOK("jgld.dll", 0x00004590, Matrix_ctor, MatrixCtor_re, MatrixCtor_orig);
// 0x000032f0  Matrix::dtor
SG_HOOK("jgld.dll", 0x000032f0, Matrix_dtor, MatrixDtor_re, MatrixDtor_orig);
// 0x00005450  Transform::ctor
SG_HOOK("jgld.dll", 0x00005450, Transform_ctor, XformCtor_re, XformCtor_orig);
// 0x000034e0  Transform::dtor
SG_HOOK("jgld.dll", 0x000034e0, Transform_dtor, XformDtor_re, XformDtor_orig);
// 0x00006c80  LinkedList::ctor
SG_HOOK("jgld.dll", 0x00006c80, LinkedList_ctor, ListCtor_re, ListCtor_orig);
// 0x00006cf0  LinkedList::dtor
SG_HOOK("jgld.dll", 0x00006cf0, LinkedList_dtor, ListDtor_re, ListDtor_orig);
// 0x00006df0  LinkedList::clear
SG_HOOK("jgld.dll", 0x00006df0, LinkedList_clear, ListClear_re, ListClear_orig);
// 0x00007100  LinkedList::removeCurrent
SG_HOOK("jgld.dll", 0x00007100, LinkedList_removeCurrent, ListRemoveCurrent_re, ListRemoveCurrent_orig);
// 0x0009f250  inflate_trees_free
SG_HOOK("jgld.dll", 0x0009f250, inflate_trees_free, InflateTreesFree_re, InflateTreesFree_orig);
