// C3 batch c3cz (round 24, 2026-10-10) of golf_clean.exe: functions earlier batches dropped because they call the
// CRT heap (malloc 0x004a504f, operator new 0x004a57f1, free 0x004a5007, operator delete 0x004a4ffc) or because
// their evidence ran through objects only a heap stand-in can supply. The fixtures (re/frida/js/fixtures.d/c3cz.js)
// replace those entry points for the length of one key with an arena allocator whose cursor and call counter are
// state, and a deallocator that only records the pointer, so both arms get the same blocks and the frees are
// compared state (the c3cn method, re/frida/js/heapstandin.js, plus scheduled allocation failures).
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). The exe
// has no relocations, so its addresses are literal constants; every callee runs through its original address:
//   Buf_alloc                      -> Buf_releaseData 0x004747e0 (C3, c3cn)
//   ChildArray::insert             -> ChildArray_grow 0x00479b40 (THIS batch)
//   ChildArray::remove             -> Vec_release 0x00479bb0 (THIS batch), the child's vtable slot 2
//   Label::setText                 -> Window::visible 0x004801f0 (C3), the label's vtable slots 0x134 / 0x13c
//   listPush                       -> listRemoveHead 0x004a4ed0 (C3, c3cn)
//   listEraseRange                 -> operatorDelete_4a4ffc 0x004a4ffc (THIS batch)
//   MsgBox::initDefaults           -> MsgBox::setLabelA 0x0048de00 / setLabelB 0x0048de90 (THIS batch)
//   Surface_free                   -> Cache483010::dtor 0x00483010 and operatorDelete_4a4ffc (THIS batch)
//   Surface_open                   -> Surface_free 0x00473ae0 (THIS batch)
//   freeFonts                      -> Surface_free (THIS batch)
//   Cache483010::dtor              -> Cache483070::reset 0x00483070 (THIS batch)
//   createCache                    -> Cache482fd0::ctor 0x00482fd0 (C2)
//   Cache482f60::scalarDtorAdj20   -> Cache482f60::dtor 0x004835f0 (C2, a c3db candidate)
//   allocPairArray                 -> Buf_ctor 0x004747a0 (C3)
//   initPairArrayGlobal            -> allocPairArray 0x0049d050 (THIS batch)
//   ListModel::clear               -> Buf_releaseData 0x004747e0 (C3, c3cn)
//   ListModel::dtor                -> ListModel::clear 0x004894b0, List402_clear 0x004026a0 (THIS batch),
//                                     Buf_dtor 0x00474810 (C3, c3cn)
//   alloc_small                    -> out_of_memory 0x004af0a0 (C3, c3cc), jpeg_get_large 0x004b0460 (C3, c3cn)
//   alloc_large                    -> the same two
//   alloc_sarray / alloc_barray    -> alloc_small 0x004aef70, alloc_large 0x004af0c0 (THIS batch)
//   free_pool                      -> jpeg_free_large 0x004b0470 (C3, c3cn), the virtual arrays' close callback
//   self_destruct_jmemmgr          -> free_pool 0x004af920 (THIS batch), jpeg_free_large, jpeg_mem_term 0x004a0890
//   jinit_c_coef_controller        -> jround_up 0x004b04d0 (C3)
//   std_huff_tables                -> add_huff_table 0x004ae8b0 (C3, c3be)
//   finish_pass_phuff              -> emit_eobrun 0x004b25d0 (C3, c3bo), flush_bits_4b2510 0x004b2510 (C3, c3aa)
// Method pointers (cinfo->err->error_exit, cinfo->mem->alloc_small / alloc_large / request_virt_barray, a list's
// vtable slot 1, an item's virtual-base deleting destructor, a display's slots +0x90 / +0x94) are read from their
// slots in both arms, so the fixture's recording stubs are what runs there.
#include "hooks.h"

#include <initializer_list>

namespace {

template <class T>
T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <class T>
T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
char* Ptr(void* p, int off) { return static_cast<char*>(p) + off; }

// ---- the CRT heap entry points (they stay original; the fixtures replace them for one key) ----------------------
typedef void*(__cdecl* Malloc_t)(unsigned);
typedef void(__cdecl* Free_t)(void*);
const Malloc_t kMalloc = reinterpret_cast<Malloc_t>(0x004a504f);        // malloc
const Malloc_t kOperatorNew = reinterpret_cast<Malloc_t>(0x004a57f1);   // operator new
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);              // free
const Free_t kOperatorDelete = reinterpret_cast<Free_t>(0x004a4ffc);    // operator delete (this batch hooks it)
typedef char*(__cdecl* Strncpy_t)(char*, const char*, unsigned);
const Strncpy_t kStrncpy = reinterpret_cast<Strncpy_t>(0x004a6c80);     // strncpy (CRT)

typedef void(__fastcall* This0_t)(void*, void*);
typedef void(__fastcall* This1_t)(void*, void*, int);
typedef void(__fastcall* This2_t)(void*, void*, int, int);
typedef int(__fastcall* ThisRet0_t)(void*, void*);

// A virtual call: [[obj] + slot].
template <class F>
F Slot(void* obj, unsigned slot) { return *reinterpret_cast<F*>(At<char*>(obj, 0) + slot); }

// The deleting destructor of an object reached through its virtual base: [obj] is a vbtable whose entry +4 is the
// offset of the virtual base, whose vtable slot 0 is called with `this` = the base and the argument 1. The sequence
// `mov eax,[obj]; mov ecx,[eax+4]; add ecx,obj; mov edx,[ecx]; push 1; call [edx]` recurs in every list clear below
// (0x004026ce-0x004026d9 and 0x004026e8-0x004026f5 in List402_clear).
void DeleteThroughVbase(void* obj) {
    char* base = Ptr(obj, At<int>(At<void*>(obj, 0), 4));
    Slot<This1_t>(base, 0)(base, 0, 1);
}

// strlen + 1, the `or ecx,-1; xor eax,eax; repne scasb; not ecx` idiom (0x00488a4c-0x00488a53 and its twins).
unsigned SizeWithNul(const char* s) {
    unsigned n = 0;
    while (s[n] != 0) n++;
    return n + 1;
}
// The copy those functions inline after storing a NUL in the new block: find the end of the destination with a
// second scasb (0x00488a90), then `rep movsd` / `rep movsb` the whole source including its NUL (0x00488a98-
// 0x00488a9f). The destination holds an empty string, so the result is the source copied to its start.
void AppendCopy(char* dst, const char* src) {
    while (*dst != 0) dst++;
    unsigned n = SizeWithNul(src);
    for (unsigned i = 0; i < n; i++) dst[i] = src[i];
}

// =====================================================================================================================
// Part 1: plain CRT heap leaves
// =====================================================================================================================

// 0x004a4ffc  operatorDelete_4a4ffc
// void __cdecl operator delete(void* p): pushes its argument again (`push dword ptr [esp+4]` at 0x004a4ffc), calls
// free 0x004a5007 (0x004a5000) and pops it (0x004a5005). No null test: a NULL reaches free.
typedef void(__cdecl* OperatorDelete_t)(void*);
OperatorDelete_t OperatorDelete_orig;
void __cdecl OperatorDelete_re(void* p) {
    kFree(p);
}

// 0x00479b40  ChildArray_grow
// void __thiscall ChildArray::grow(): the pointer array is {items +0x224, capacity +0x228, count +0x22c}. Adds 10 to
// the capacity and stores it first (0x00479b44-0x00479b4d), mallocs capacity*4 bytes (`shl eax,2` 0x00479b53, call
// 0x00479b57) WITHOUT a null test, copies the `count` items with a loop that re-reads the count each pass
// (0x00479b6d-0x00479b84; a count <= 0 skips it at 0x00479b6b), frees the old array when it is not null
// (0x00479b8e / 0x00479b91) and stores the new one (0x00479b99).
This0_t ChildArrayGrow_orig;
void __fastcall ChildArrayGrow_re(void* self, void*) {
    int cap = At<int>(self, 0x228) + 10;
    At<int>(self, 0x228) = cap;
    void** fresh = static_cast<void**>(kMalloc(static_cast<unsigned>(cap) << 2));
    for (int i = 0; i < At<int>(self, 0x22c); i++)
        fresh[i] = At<void**>(self, 0x224)[i];
    void* old = At<void*>(self, 0x224);
    if (old != 0)
        kFree(old);
    At<void**>(self, 0x224) = fresh;
}

// 0x00479bb0  Vec_release
// void __thiscall Vec::release(): shrinks the same array by 10. A capacity that is not 0 and below 10 (signed,
// `jl 0x00479c3c` at 0x00479bc0) returns at once. Otherwise the capacity drops by 10 (0x00479bc2 / 0x00479bc7); when
// it reaches 0 the array is freed when not null (0x00479bd7 / 0x00479bda) and the pointer cleared (0x00479be2).
// Any other new capacity (a 0 start gives -10) gets a fresh block of capacity*4 bytes (0x00479bee-0x00479bf3), the
// count items copied with the same re-reading loop (0x00479c09-0x00479c20), the old block freed when not null
// (0x00479c2a / 0x00479c2d) and the new one stored (0x00479c35).
This0_t VecRelease_orig;
void __fastcall VecRelease_re(void* self, void*) {
    int cap = At<int>(self, 0x228);
    if (cap != 0 && cap < 10)
        return;
    cap -= 10;
    At<int>(self, 0x228) = cap;
    if (cap == 0) {
        void* old = At<void*>(self, 0x224);
        if (old != 0)
            kFree(old);
        At<void*>(self, 0x224) = 0;
        return;
    }
    void** fresh = static_cast<void**>(kMalloc(static_cast<unsigned>(cap) << 2));
    for (int i = 0; i < At<int>(self, 0x22c); i++)
        fresh[i] = At<void**>(self, 0x224)[i];
    void* old = At<void*>(self, 0x224);
    if (old != 0)
        kFree(old);
    At<void**>(self, 0x224) = fresh;
}

// 0x0047e5f0  ChildArray::insert
// void __thiscall ChildArray::insert(Window* child): a null child returns (0x0047e5fa). When count + 1 exceeds the
// capacity (`jle` at 0x0047e60b, signed) ChildArray_grow 0x00479b40 runs (0x0047e60f). A child whose flag word +0x9c
// has bit 0x2000000 (0x0047e614) is appended at items[count] (0x0047e620-0x0047e62c); any other child is put first:
// the items move up one slot from the top down (0x0047e64b-0x0047e65d, skipped for a count <= 0 at 0x0047e649) and
// items[0] gets the child (0x0047e665). Either way the count goes up by one (0x0047e637 / 0x0047e66e).
typedef void(__fastcall* ChildArrayInsert_t)(void*, void*, void*);
ChildArrayInsert_t ChildArrayInsert_orig;
void __fastcall ChildArrayInsert_re(void* self, void*, void* child) {
    if (child == 0)
        return;
    if (At<int>(self, 0x22c) + 1 > At<int>(self, 0x228))
        reinterpret_cast<This0_t>(0x00479b40)(self, 0);
    if (At<unsigned>(child, 0x9c) & 0x2000000) {
        At<void**>(self, 0x224)[At<int>(self, 0x22c)] = child;
        At<int>(self, 0x22c) += 1;
        return;
    }
    for (int i = At<int>(self, 0x22c); i > 0; i--)
        At<void**>(self, 0x224)[i] = At<void**>(self, 0x224)[i - 1];
    At<void**>(self, 0x224)[0] = child;
    At<int>(self, 0x22c) += 1;
}

// 0x0047e680  ChildArray::remove
// void __thiscall ChildArray::remove(Window* child): a null child returns (0x0047e689). The items are searched from
// index 0 for the child (0x0047e69e-0x0047e6a8, none for a count <= 0). When found (`jge` at 0x0047e6ac is not taken)
// the count drops by one FIRST (0x0047e6ae / 0x0047e6af), the child's vtable slot 2 runs with no argument
// (0x0047e6b5 / 0x0047e6b7), and the items above the index move down one slot up to the new count (0x0047e6c2-
// 0x0047e6d8). Found or not, when count <= capacity - 10 (signed, `jg` at 0x0047e6ec) Vec_release 0x00479bb0 runs
// (0x0047e6f0).
ChildArrayInsert_t ChildArrayRemove_orig;
void __fastcall ChildArrayRemove_re(void* self, void*, void* child) {
    if (child == 0)
        return;
    int count = At<int>(self, 0x22c);
    int i = 0;
    while (i < count && At<void**>(self, 0x224)[i] != child)
        i++;
    if (i < count) {
        At<int>(self, 0x22c) = count - 1;
        Slot<This0_t>(child, 8)(child, 0);
        while (i < At<int>(self, 0x22c)) {
            i++;
            At<void**>(self, 0x224)[i - 1] = At<void**>(self, 0x224)[i];
        }
    }
    if (!(At<int>(self, 0x22c) > At<int>(self, 0x228) - 10))
        reinterpret_cast<This0_t>(0x00479bb0)(self, 0);
}

// 0x00474820  Buf_alloc
// int __thiscall Buf::alloc(unsigned size): a buffer at +8 is released first through Buf_releaseData 0x004747e0
// (`test eax,eax; je` at 0x00474827 / 0x00474829, call 0x0047482b). The new block from malloc 0x004a504f (0x00474835)
// is stored at +8 even when it is NULL (0x0047483d); NULL returns 1 (0x00474845). Otherwise +0x14 and +0x10 get the
// size (0x0047484e / 0x00474851), +0xc the block (0x00474854), and the result is 0 (0x00474858).
typedef int(__fastcall* BufAlloc_t)(void*, void*, unsigned);
BufAlloc_t BufAlloc_orig;
int __fastcall BufAlloc_re(void* self, void*, unsigned size) {
    if (At<void*>(self, 8) != 0)
        reinterpret_cast<This0_t>(0x004747e0)(self, 0);
    void* p = kMalloc(size);
    At<void*>(self, 8) = p;
    if (p == 0)
        return 1;
    At<unsigned>(self, 0x14) = size;
    At<unsigned>(self, 0x10) = size;
    At<void*>(self, 0xc) = p;
    return 0;
}

// 0x00486250  EditBox::resizeBuffer
// void __thiscall EditBox::resizeBuffer(int n): the text buffer is +0x574, its size +0x578. The same size returns
// (`cmp ebx,[edi+0x578]; je` at 0x00486258 / 0x0048625e). Otherwise a block of n + 1 bytes from malloc 0x004a504f
// (0x00486260-0x00486265), used without a null test, gets a NUL at [n] and at [0] (0x0048626f / 0x00486273); an old
// buffer is copied in with strncpy(new, old, n) (0x00486280-0x00486283) and freed (the second null test at 0x00486293
// re-reads the same field, call 0x00486296); the new buffer and size are stored (0x0048629e / 0x004862a4).
typedef void(__fastcall* EditBoxResize_t)(void*, void*, int);
EditBoxResize_t EditBoxResize_orig;
void __fastcall EditBoxResize_re(void* self, void*, int n) {
    if (n == At<int>(self, 0x578))
        return;
    char* p = static_cast<char*>(kMalloc(static_cast<unsigned>(n + 1)));
    p[n] = 0;
    p[0] = 0;
    char* old = At<char*>(self, 0x574);
    if (old != 0) {
        kStrncpy(p, old, static_cast<unsigned>(n));
        if (At<char*>(self, 0x574) != 0)
            kFree(At<char*>(self, 0x574));
    }
    At<char*>(self, 0x574) = p;
    At<int>(self, 0x578) = n;
}

// 0x00488a20  Button::setTooltip
// int __thiscall Button::setTooltip(const char* text): an old tooltip at +0x5f8 is freed and the field cleared
// (0x00488a2d-0x00488a38). A null text returns 0 (0x00488a48 -> 0x00488aa3). Otherwise a block of strlen + 1 bytes
// (0x00488a4c-0x00488a56) is stored at +0x5f8 (0x00488a5e); NULL returns 4 (0x00488a6a); else its first byte is
// zeroed (0x00488a73), the text copied in (0x00488a76-0x00488a9f) and the result is 0 (0x00488aa3).
typedef int(__fastcall* SetText_t)(void*, void*, const char*);
SetText_t ButtonSetTooltip_orig;
int __fastcall ButtonSetTooltip_re(void* self, void*, const char* text) {
    if (At<char*>(self, 0x5f8) != 0) {
        kFree(At<char*>(self, 0x5f8));
        At<char*>(self, 0x5f8) = 0;
    }
    if (text == 0)
        return 0;
    char* p = static_cast<char*>(kMalloc(SizeWithNul(text)));
    At<char*>(self, 0x5f8) = p;
    if (p == 0)
        return 4;
    p[0] = 0;
    AppendCopy(At<char*>(self, 0x5f8), text);
    return 0;
}

// 0x004a29c0  setStringField
// int __thiscall setStringField(const char* text, int index): the field is the string pointer at this + 4*index +
// 0x2c. A null text empties an existing string in place (`mov byte ptr [ebx],0` at 0x004a29dc after the null test at
// 0x004a29d6) and returns 0. Otherwise an old string is freed (0x004a29ee-0x004a29f3; the field is NOT cleared), a
// block of strlen + 1 bytes from malloc 0x004a504f (0x004a2a07) is stored in the field (0x004a2a0f); NULL returns 4
// (0x004a2a1a); else it gets a NUL first byte (0x004a2a23), the text is copied in (0x004a2a26-0x004a2a4d) and the
// result is 0.
typedef int(__fastcall* SetStringField_t)(void*, void*, const char*, int);
SetStringField_t SetStringField_orig;
int __fastcall SetStringField_re(void* self, void*, const char* text, int index) {
    char*& field = At<char*>(self, 4 * index + 0x2c);
    if (text == 0) {
        if (field != 0)
            field[0] = 0;
        return 0;
    }
    if (field != 0)
        kFree(field);
    char* p = static_cast<char*>(kMalloc(SizeWithNul(text)));
    field = p;
    if (p == 0)
        return 4;
    p[0] = 0;
    AppendCopy(field, text);
    return 0;
}

// 0x004a4db0  listPush
// int __thiscall listPush(void** out, unsigned size, int a, int b): the list is {head +4, tail +8, count +0xc,
// limit +0x10}, a node {a +0, b +4, data +8, size +0xc, next +0x10} of 0x14 bytes. A null `out` returns 3
// (0x004a4dbd). *out = 0 (0x004a4dc8). count == limit returns 0x12 (0x004a4dd4). The node comes from malloc
// 0x004a504f (push 0x14 at 0x004a4de4): with no head it is stored as the head (0x004a4df2; NULL returns 4 at
// 0x004a4dfb), else as the old tail's +0x10 (0x004a4e0f; NULL returns 4 at 0x004a4e1e). The node becomes the tail
// (0x004a4e2f), its next is cleared (0x004a4e32), +4 = b (0x004a4e3e), +0 = a (0x004a4e48). A size of 0 skips the
// data block (0x004a4e4a). Otherwise a block of `size` bytes is stored at +8 (0x004a4e58); NULL removes the list's
// HEAD through listRemoveHead 0x004a4ed0 (0x004a4e67) and returns 4, else +0xc = size (0x004a4e77). Then the count
// goes up (0x004a4e7f), *out = the node's data pointer (0x004a4e8b) and the result is 0.
typedef int(__fastcall* ListPush_t)(void*, void*, void**, unsigned, int, int);
ListPush_t ListPush_orig;
int __fastcall ListPush_re(void* self, void*, void** out, unsigned size, int a, int b) {
    if (out == 0)
        return 3;
    *out = 0;
    if (At<int>(self, 0xc) == At<int>(self, 0x10))
        return 0x12;
    char* node;
    if (At<void*>(self, 4) == 0) {
        node = static_cast<char*>(kMalloc(0x14));
        At<char*>(self, 4) = node;
        if (node == 0)
            return 4;
    } else {
        At<char*>(At<char*>(self, 8), 0x10) = static_cast<char*>(kMalloc(0x14));
        node = At<char*>(At<char*>(self, 8), 0x10);
        if (node == 0)
            return 4;
    }
    At<char*>(self, 8) = node;
    At<void*>(node, 0x10) = 0;
    At<int>(At<char*>(self, 8), 4) = b;
    At<int>(At<char*>(self, 8), 0) = a;
    if (size != 0) {
        At<void*>(At<char*>(self, 8), 8) = kMalloc(size);
        char* tail = At<char*>(self, 8);
        if (At<void*>(tail, 8) == 0) {
            reinterpret_cast<This0_t>(0x004a4ed0)(self, 0);
            return 4;
        }
        At<unsigned>(tail, 0xc) = size;
    }
    At<int>(self, 0xc) += 1;
    *out = At<void*>(At<char*>(self, 8), 8);
    return 0;
}

// 0x0044ac00  listEraseRange
// node** __thiscall listEraseRange(node** out, node* first, node* last): a std::list erase(first, last). Each node
// {next +0, prev +4} from first up to (not including) last is unlinked (prev->next = next at 0x0044ac1b, next->prev =
// prev at 0x0044ac22), handed to operator delete 0x004a4ffc (0x0044ac25) and the list's size at +8 drops by one
// (0x0044ac30 / 0x0044ac33). *out = last (0x0044ac3d) and `out` is returned (0x0044ac38).
typedef void**(__fastcall* ListEraseRange_t)(void*, void*, void**, void*, void*);
ListEraseRange_t ListEraseRange_orig;
void** __fastcall ListEraseRange_re(void* self, void*, void** out, void* first, void* last) {
    void* node = first;
    while (node != last) {
        void* cur = node;
        node = At<void*>(cur, 0);
        At<void*>(At<void*>(cur, 4), 0) = At<void*>(cur, 0);
        At<void*>(At<void*>(cur, 0), 4) = At<void*>(cur, 4);
        kOperatorDelete(cur);
        At<int>(self, 8) -= 1;
    }
    *out = node;
    return out;
}

// 0x00449210  allocTileBuffer
// void* allocTileBuffer(): the static construction of a std::list held in globals: the allocator byte 0x00820f18
// gets the HIGH byte of ecx (`push ecx` at 0x00449210, `mov al,[esp+3]` at 0x00449211, store 0x00449217), a 0xc-byte
// head node comes from operator new 0x004a57f1 (push 0xc at 0x00449215, call 0x0044921c) and is linked to itself
// (next +0 and prev +4 at 0x00449221 / 0x00449223) without a null test, 0x00820f1c = the head (0x00449226) and the
// size 0x00820f20 = 0 (0x0044922b). eax is left holding the head.
ThisRet0_t AllocTileBuffer_orig;
int __fastcall AllocTileBuffer_re(void* ecxValue, void*) {
    G<unsigned char>(0x00820f18) = static_cast<unsigned char>(reinterpret_cast<unsigned>(ecxValue) >> 24);
    void** head = static_cast<void**>(kOperatorNew(0xc));
    head[0] = head;
    head[1] = head;
    G<void**>(0x00820f1c) = head;
    G<int>(0x00820f20) = 0;
    return reinterpret_cast<int>(head);
}

// 0x0047ada0  View_unlinkFromParent
// void __thiscall View::unlinkFromParent(): the parent is +0x130; its attachment ring is {head +0x138, cursor +0x13c,
// count +0x140, last index +0x144, keep-blocks flag +0x148}, a node {view +4, data +8, next +0xc, prev +0x10}.
// No parent or no head returns (0x0047ada9 / 0x0047adb7). Starting at the CURSOR, up to `count` nodes are visited,
// advancing the cursor each time (0x0047adce-0x0047ade5); a count <= 0 visits none (0x0047adc8). The node whose +4 is
// this view is unlinked (next->prev = prev at 0x0047adf5, prev->next = next at 0x0047ae04), the head moves on when it
// was the head (0x0047ae0d-0x0047ae18), the cursor moves to the next node (0x0047ae29), and unless the parent's
// +0x148 is set (0x0047ae2f) the data block is freed when not null (0x0047ae39), +8 cleared (0x0047ae43) and the node
// freed (0x0047ae4d); the count drops (0x0047ae55). Found or not, a count of 0 clears the head (0x0047ae66) and the
// last index becomes count - 1 (0x0047ae6c / 0x0047ae6d).
This0_t ViewUnlink_orig;
void __fastcall ViewUnlink_re(void* self, void*) {
    char* parent = At<char*>(self, 0x130);
    if (parent == 0 || At<void*>(parent, 0x138) == 0)
        return;
    int n = At<int>(parent, 0x140);
    int i = 0;
    bool found = false;
    while (i < n) {
        if (At<void*>(At<char*>(parent, 0x13c), 4) == self) {
            found = true;
            break;
        }
        At<char*>(parent, 0x13c) = At<char*>(At<char*>(parent, 0x13c), 0xc);
        i++;
    }
    if (found) {
        char* cur = At<char*>(parent, 0x13c);
        At<char*>(At<char*>(cur, 0xc), 0x10) = At<char*>(cur, 0x10);
        cur = At<char*>(parent, 0x13c);
        At<char*>(At<char*>(cur, 0x10), 0xc) = At<char*>(cur, 0xc);
        char* node = At<char*>(parent, 0x13c);
        if (node == At<char*>(parent, 0x138))
            At<char*>(parent, 0x138) = At<char*>(node, 0xc);
        At<char*>(parent, 0x13c) = At<char*>(node, 0xc);
        if (At<int>(parent, 0x148) == 0) {
            if (At<void*>(node, 8) != 0)
                kFree(At<void*>(node, 8));
            At<void*>(node, 8) = 0;
            if (node != 0)
                kFree(node);
        }
        At<int>(parent, 0x140) -= 1;
    }
    int count = At<int>(parent, 0x140);
    if (count == 0)
        At<int>(parent, 0x138) = 0;
    At<int>(parent, 0x144) = count - 1;
}

// 0x0047b8f0  Label::setText
// void __thiscall Label::setText(const char* text): an old text at +0x14c is freed and the field cleared
// (0x0047b8fd-0x0047b908). A text gets a copy: a block of strlen + 1 bytes (0x0047b926) stored at +0x14c (0x0047b92e);
// NULL returns at once (0x0047b936), else NUL first byte (0x0047b938) and the copy (0x0047b93b-0x0047b964). Then,
// with or without a text, when Window::visible 0x004801f0 says so (0x0047b968 / 0x0047b96f) the label's vtable slot
// 0x134 runs with 0 (0x0047b973-0x0047b977) and slot 0x13c with (0, 0) (0x0047b97f-0x0047b985).
typedef void(__fastcall* LabelSetText_t)(void*, void*, const char*);
LabelSetText_t LabelSetText_orig;
void __fastcall LabelSetText_re(void* self, void*, const char* text) {
    if (At<char*>(self, 0x14c) != 0) {
        kFree(At<char*>(self, 0x14c));
        At<char*>(self, 0x14c) = 0;
    }
    if (text != 0) {
        char* p = static_cast<char*>(kMalloc(SizeWithNul(text)));
        At<char*>(self, 0x14c) = p;
        if (p == 0)
            return;
        p[0] = 0;
        AppendCopy(At<char*>(self, 0x14c), text);
    }
    if (reinterpret_cast<ThisRet0_t>(0x004801f0)(self, 0) == 0)
        return;
    Slot<This1_t>(self, 0x134)(self, 0, 0);
    Slot<This2_t>(self, 0x13c)(self, 0, 0, 0);
}

// 0x0048de00  MsgBox::setLabelA
// int __cdecl MsgBox::setLabelA(const char* text): the label is the global string 0x0083b994. A null text returns 3
// (0x0048de0b). An old label is freed and the global cleared (0x0048de12-0x0048de24); a block of strlen + 1 bytes
// (0x0048de3a) is stored in it (0x0048de42); NULL returns 5 (0x0048de4c), else a NUL first byte (0x0048de53) and the
// copy (0x0048de56-0x0048de7f); the result is the 0 left in eax by the copy's `xor eax,eax` (0x0048de5b).
// 0x0048de90  MsgBox::setLabelB is the same body over 0x0083b998, returning 4 instead of 5 for NULL (0x0048dedc).
typedef int(__cdecl* SetLabel_t)(const char*);
int SetGlobalLabel(unsigned global, const char* text, int oom) {
    if (text == 0)
        return 3;
    if (G<char*>(global) != 0) {
        kFree(G<char*>(global));
        G<char*>(global) = 0;
    }
    char* p = static_cast<char*>(kMalloc(SizeWithNul(text)));
    G<char*>(global) = p;
    if (p == 0)
        return oom;
    p[0] = 0;
    AppendCopy(G<char*>(global), text);
    return 0;
}
SetLabel_t SetLabelA_orig;
int __cdecl SetLabelA_re(const char* text) { return SetGlobalLabel(0x0083b994, text, 5); }
SetLabel_t SetLabelB_orig;
int __cdecl SetLabelB_re(const char* text) { return SetGlobalLabel(0x0083b998, text, 4); }

// 0x00490bf0  MsgBox::initDefaults
// int __cdecl MsgBox::initDefaults(): copies the dword 0x0083ad44 into 0x0083b9b4 and 0x0083b9c4 (0x00490bf0-
// 0x00490bff), sets label B from the string at 0x004c8590 (0x00490bf5 / 0x00490c04) and label A from 0x004e3db0
// (0x00490c09 / 0x00490c0e) through setLabelB 0x0048de90 and setLabelA 0x0048de00, ignoring their results, clears
// 0x0083b99c and 0x0083b9a0 (0x00490c18 / 0x00490c1d) and returns 0 (0x00490c13).
typedef int(__cdecl* Cdecl0_t)();
Cdecl0_t MsgBoxInit_orig;
int __cdecl MsgBoxInit_re() {
    unsigned v = G<unsigned>(0x0083ad44);
    G<unsigned>(0x0083b9b4) = v;
    G<unsigned>(0x0083b9c4) = v;
    reinterpret_cast<SetLabel_t>(0x0048de90)(reinterpret_cast<const char*>(0x004c8590));
    reinterpret_cast<SetLabel_t>(0x0048de00)(reinterpret_cast<const char*>(0x004e3db0));
    G<unsigned>(0x0083b99c) = 0;
    G<unsigned>(0x0083b9a0) = 0;
    return 0;
}

// 0x00492660  HotList::freeEntryTip
// void __thiscall HotList::freeEntryTip(): only while the global 0x0083d348 is 0 (0x00492660 / 0x0049266a; HotList::grow
// 0x00492690 sets it to 1 around its array delete, 0x00492766 / 0x00492781) the entry's tip text at +0x1c is freed
// when not null (0x00492671 / 0x00492674) and the field cleared (0x0049267c).
This0_t FreeEntryTip_orig;
void __fastcall FreeEntryTip_re(void* self, void*) {
    if (G<int>(0x0083d348) != 0)
        return;
    if (At<void*>(self, 0x1c) != 0) {
        kFree(At<void*>(self, 0x1c));
        At<void*>(self, 0x1c) = 0;
    }
}

// 0x0049d460  freeFileWinBuf
// void __cdecl freeFileWinBuf(): frees the global block 0x00840820 when it is not null (0x0049d465 / 0x0049d46a) and
// clears it (0x0049d472).
// 0x00481b20  freeResampleTable is the same body over 0x0083ac10 (0x00481b25 / 0x00481b2a / 0x00481b32).
typedef void(__cdecl* CdeclVoid0_t)();
void FreeGlobalBlock(unsigned global) {
    if (G<void*>(global) != 0) {
        kFree(G<void*>(global));
        G<void*>(global) = 0;
    }
}
CdeclVoid0_t FreeFileWinBuf_orig;
void __cdecl FreeFileWinBuf_re() { FreeGlobalBlock(0x00840820); }
CdeclVoid0_t FreeResampleTable_orig;
void __cdecl FreeResampleTable_re() { FreeGlobalBlock(0x0083ac10); }

// =====================================================================================================================
// Part 2: Surface and the colour caches behind it (the live display 0x0083ad50 is swapped by the fixture)
// =====================================================================================================================

// 0x00473ab0  Surface_ctor
// Surface* __thiscall Surface::Surface(): +0 = the vtable 0x004ba2d8 (0x00473ab7), and the dwords +4, +8, +0xc,
// +0x10 .. +0x24 = 0 (0x00473ab4-0x00473ad2); returns this (0x00473ab0).
ThisRet0_t SurfaceCtor_orig;
int __fastcall SurfaceCtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004ba2d8;
    for (unsigned off = 4; off <= 0x24; off += 4)
        At<unsigned>(self, off) = 0;
    return reinterpret_cast<int>(self);
}

// 0x00473ae0  Surface_free
// void __thiscall Surface::free(): nothing happens without a display (0x0083ad50 == 0, 0x00473aee). With one, a
// handle at +4 is released through the display's vtable slot +0x94 (0x00473af5-0x00473afa); the +4 field itself is
// left as it is. A block at +8 is freed and the field cleared (0x00473b05-0x00473b10). A cache at +0xc runs
// Cache483010::dtor 0x00483010 (0x00473b1d), goes to operator delete 0x004a4ffc (0x00473b23) and the field is cleared
// (0x00473b2b). Then +0x10 .. +0x24 are cleared (0x00473b2e-0x00473b3d).
This0_t SurfaceFree_orig;
void __fastcall SurfaceFree_re(void* self, void*) {
    void* disp = G<void*>(0x0083ad50);
    if (disp == 0)
        return;
    int handle = At<int>(self, 4);
    if (handle != 0)
        Slot<This1_t>(disp, 0x94)(disp, 0, handle);
    if (At<void*>(self, 8) != 0) {
        kFree(At<void*>(self, 8));
        At<void*>(self, 8) = 0;
    }
    void* cache = At<void*>(self, 0xc);
    if (cache != 0) {
        reinterpret_cast<This0_t>(0x00483010)(cache, 0);
        kOperatorDelete(cache);
        At<void*>(self, 0xc) = 0;
    }
    for (unsigned off = 0x10; off <= 0x24; off += 4)
        At<unsigned>(self, off) = 0;
}

// 0x00473b50  Surface_open
// int __thiscall Surface::open(const char* name, int a, int b): a null name returns 0x10 (0x00473b5f). Otherwise
// Surface_free 0x00473ae0 runs (0x00473b6a), a block of strlen + 1 bytes (0x00473b7b) is stored at +8 (0x00473b83);
// NULL returns 4 (0x00473b8c); else NUL first byte (0x00473b95), the name copied in (0x00473b98-0x00473bbe), +0x20
// and +0x24 = 0 (the copy's zero eax, 0x00473bc4 / 0x00473bc7), +0x10 = +0x18 = a and +0x14 = +0x1c = b
// (0x00473bcf-0x00473bd8), and the result is 0.
typedef int(__fastcall* SurfaceOpen_t)(void*, void*, const char*, int, int);
SurfaceOpen_t SurfaceOpen_orig;
int __fastcall SurfaceOpen_re(void* self, void*, const char* name, int a, int b) {
    if (name == 0)
        return 0x10;
    reinterpret_cast<This0_t>(0x00473ae0)(self, 0);
    char* p = static_cast<char*>(kMalloc(SizeWithNul(name)));
    At<char*>(self, 8) = p;
    if (p == 0)
        return 4;
    p[0] = 0;
    AppendCopy(At<char*>(self, 8), name);
    At<int>(self, 0x20) = 0;
    At<int>(self, 0x24) = 0;
    At<int>(self, 0x10) = a;
    At<int>(self, 0x14) = b;
    At<int>(self, 0x18) = a;
    At<int>(self, 0x1c) = b;
    return 0;
}

// 0x00483070  Cache483070::reset
// void __thiscall Cache::reset(): a handle at +4 is released through the display's slot +0x90 and cleared, only when
// both the handle and the display 0x0083ad50 are non-zero (0x0048307a-0x00483091). Then five entries of 0x10 bytes
// from +8 {+0 index, +4 object, +8 byte, +9 byte, +0xc block}: the index = -1 (0x0048309c), an object is destroyed
// through its vtable slot 0 with 1 (a scalar deleting destructor, 0x004830a9-0x004830ad) and the field cleared
// (0x004830af), a block is freed (0x004830b9) and cleared (0x004830c1), and both bytes are cleared (0x004830c4 /
// 0x004830c7).
This0_t CacheReset_orig;
void __fastcall CacheReset_re(void* self, void*) {
    int handle = At<int>(self, 4);
    if (handle != 0) {
        void* disp = G<void*>(0x0083ad50);
        if (disp != 0) {
            Slot<This1_t>(disp, 0x90)(disp, 0, handle);
            At<int>(self, 4) = 0;
        }
    }
    for (int k = 0; k < 5; k++) {
        char* e = Ptr(self, 8 + 0x10 * k);
        At<int>(e, 0) = -1;
        void* obj = At<void*>(e, 4);
        if (obj != 0) {
            Slot<This1_t>(obj, 0)(obj, 0, 1);
            At<void*>(e, 4) = 0;
        }
        if (At<void*>(e, 0xc) != 0) {
            kFree(At<void*>(e, 0xc));
            At<void*>(e, 0xc) = 0;
        }
        At<unsigned char>(e, 8) = 0;
        At<unsigned char>(e, 9) = 0;
    }
}

// 0x00483010  Cache483010::dtor
// void __thiscall Cache::~Cache(): stores the vtable 0x004babbc at +0 (0x00483010) and tail-jumps to
// Cache483070::reset 0x00483070 (0x00483016), which never writes +0.
This0_t CacheDtor_orig;
void __fastcall CacheDtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004babbc;
    reinterpret_cast<This0_t>(0x00483070)(self, 0);
}

// 0x0049ff30  freeFonts
// void __cdecl freeFonts(): Surface_free 0x00473ae0 on the global Surface 0x008408c8 (0x0049ff30 / 0x0049ff35) and,
// as a tail jump, on 0x008408f8 (0x0049ff3a / 0x0049ff3f).
CdeclVoid0_t FreeFonts_orig;
void __cdecl FreeFonts_re() {
    reinterpret_cast<This0_t>(0x00473ae0)(reinterpret_cast<void*>(0x008408c8), 0);
    reinterpret_cast<This0_t>(0x00473ae0)(reinterpret_cast<void*>(0x008408f8), 0);
}

// 0x0049c830  createCache
// int __thiscall createCache(Holder* out): slot 0 of the vtable 0x004babac. A 0x58-byte block from operator new
// 0x004a57f1 (push 0x58 at 0x0049c849) is constructed by Cache482fd0::ctor 0x00482fd0 (0x0049c865), which returns it;
// a NULL block skips the constructor (0x0049c861 / 0x0049c86c). out->+4 gets the pointer either way (0x0049c874);
// NULL returns 4 (0x0049c879), else this->+0x1c = the cache (0x0049c894) and the result is 0.
typedef int(__fastcall* CreateCache_t)(void*, void*, void*);
CreateCache_t CreateCache_orig;
int __fastcall CreateCache_re(void* self, void*, void* out) {
    void* cache = kOperatorNew(0x58);
    if (cache != 0)
        cache = reinterpret_cast<void*(__fastcall*)(void*, void*)>(0x00482fd0)(cache, 0);
    At<void*>(out, 4) = cache;
    if (cache == 0)
        return 4;
    At<void*>(self, 0x1c) = cache;
    return 0;
}

// 0x00482f60  Cache482f60::ctor
// Cache482f60* __thiscall Cache482f60::Cache482f60(int mostDerived): a class with a virtual base at the offset its
// vbtable (+4) gives. When mostDerived is non-zero (0x00482f6f) it installs the vbtable 0x004babb4 at +4
// (0x00482f71), the base's vtable 0x004ba278 at +0x20 (0x00482f78), saves the global 0x00839650 at +0x24 and clears
// the global (0x00482f7f-0x00482f88). Then +0 = 0x004ba2ac (0x00482f91) and the base's vtable slot at this + 4 +
// [vbtable+4] = 0x004ba2a8 (0x00482f9a), +8 .. +0x18 = 0 (0x00482fa2-0x00482fae), +0 = 0x004babac (0x00482fb4) and the
// base's slot again = 0x004baba8 (0x00482fbd). Returns this. The vbtable is re-read from +4 for both base stores.
typedef void*(__fastcall* Ctor1_t)(void*, void*, int);
Ctor1_t Cache482f60Ctor_orig;
void* __fastcall Cache482f60Ctor_re(void* self, void*, int mostDerived) {
    if (mostDerived != 0) {
        At<unsigned>(self, 4) = 0x004babb4;
        At<unsigned>(self, 0x20) = 0x004ba278;
        At<unsigned>(self, 0x24) = G<unsigned>(0x00839650);
        G<unsigned>(0x00839650) = 0;
    }
    At<unsigned>(self, 0) = 0x004ba2ac;
    At<unsigned>(self, 4 + At<int>(At<void*>(self, 4), 4)) = 0x004ba2a8;
    for (unsigned off = 8; off <= 0x18; off += 4)
        At<unsigned>(self, off) = 0;
    At<unsigned>(self, 0) = 0x004babac;
    At<unsigned>(self, 4 + At<int>(At<void*>(self, 4), 4)) = 0x004baba8;
    return self;
}

// 0x00483750  Cache482f60::scalarDtorAdj20
// void* __thiscall scalarDtorAdj20(unsigned flags): `this` is the virtual base at +0x20 of the full object; slot 0 of
// the base vtable 0x004baba8. The full object is this - 0x20 (0x00483752). Cache482f60::dtor 0x004835f0 runs on the
// base (0x0048375a); then the base's vtable = 0x004ba278 (0x00483763), the global 0x00839650 = the dword at base+4
// (0x00483769 / 0x0048376e). With flags bit 0 (0x0048376c / 0x00483774) and a non-null full object (0x00483778), the
// full object is freed (0x0048377f) only when that dword is 0 (0x0048377c), and the global is cleared (0x00483787).
// Returns the full object (0x00483791).
typedef void*(__fastcall* ScalarDtor_t)(void*, void*, unsigned);
ScalarDtor_t CacheScalarDtorAdj20_orig;
void* __fastcall CacheScalarDtorAdj20_re(void* self, void*, unsigned flags) {
    char* full = Ptr(self, -0x20);
    reinterpret_cast<This0_t>(0x004835f0)(self, 0);
    At<unsigned>(self, 0) = 0x004ba278;
    unsigned saved = At<unsigned>(self, 4);
    G<unsigned>(0x00839650) = saved;
    if ((flags & 1) != 0 && full != 0) {
        if (saved == 0)
            kFree(full);
        G<unsigned>(0x00839650) = 0;
    }
    return full;
}

// 0x0049d050  allocPairArray
// void* __thiscall allocPairArray(): Buf_ctor 0x004747a0 (0x0049d053), then +0 = the vtable 0x004bbe6c (0x0049d058)
// over the one Buf_ctor stored, +0x18 = 0 (0x0049d05e); returns this (0x0049d065).
ThisRet0_t AllocPairArray_orig;
int __fastcall AllocPairArray_re(void* self, void*) {
    reinterpret_cast<This0_t>(0x004747a0)(self, 0);
    At<unsigned>(self, 0) = 0x004bbe6c;
    At<unsigned>(self, 0x18) = 0;
    return reinterpret_cast<int>(self);
}

// 0x0049d010  initPairArrayGlobal
// void* __cdecl initPairArrayGlobal(): `mov ecx, 0x008406e8; jmp 0x0049d050`: allocPairArray on the global object
// 0x008406e8, whose result (the object) it returns.
Cdecl0_t InitPairArrayGlobal_orig;
int __cdecl InitPairArrayGlobal_re() {
    return reinterpret_cast<ThisRet0_t>(0x0049d050)(reinterpret_cast<void*>(0x008406e8), 0);
}

// =====================================================================================================================
// Part 3: lists whose items and nodes are destroyed through their virtual bases
// =====================================================================================================================

// The loop the three list clears share. The list is {vtable +0, current +8, next +0xc, count +0x10, +0x14}, a node
// {vbtable +0, data +8, next +0xc}. `count` times: next = current->+0xc (0x004026bb / 0x004026be), the list's vtable
// slot 1 runs with the node's data (0x004026c1-0x004026c7), non-null data is destroyed through its virtual base
// (0x004026ca-0x004026d9), current->+8 = 0 (0x004026db / 0x004026de), a non-null current node is destroyed the same
// way (0x004026e1-0x004026f5), and current = next (0x004026fe). The count is re-read every pass (0x004026f7).
void ClearNodes(char* list) {
    for (int i = 0; i < At<int>(list, 0x10); i++) {
        char* cur = At<char*>(list, 8);
        At<char*>(list, 0xc) = At<char*>(cur, 0xc);
        void* data = At<void*>(cur, 8);
        Slot<This1_t>(list, 4)(list, 0, reinterpret_cast<int>(data));
        if (data != 0)
            DeleteThroughVbase(data);
        At<void*>(At<char*>(list, 8), 8) = 0;
        if (At<char*>(list, 8) != 0)
            DeleteThroughVbase(At<char*>(list, 8));
        At<char*>(list, 8) = At<char*>(list, 0xc);
    }
}

// 0x004026a0  List402_clear
// void __thiscall List402::clear(): an empty current (+8 == 0) returns at once (0x004026a9). Otherwise the shared node
// loop runs (skipped for a count <= 0 at 0x004026b3) and +8, +0x14 and +0x10 are cleared (0x00402706-0x0040270c).
This0_t List402Clear_orig;
void __fastcall List402Clear_re(void* self, void*) {
    char* list = static_cast<char*>(self);
    if (At<void*>(list, 8) == 0)
        return;
    ClearNodes(list);
    At<int>(list, 8) = 0;
    At<int>(list, 0x14) = 0;
    At<int>(list, 0x10) = 0;
}

// 0x004894b0  ListModel::clear
// void __thiscall ListModel::clear(): the node list at +0xc0 is cleared like List402_clear (0x004894be-0x00489532;
// empty at 0x004894c6, count <= 0 at 0x004894d1) and its +0x14 is cleared again either way (0x00489538); the Buf at +4
// is released through Buf_releaseData 0x004747e0 (0x0048953b); then the model's fields are reset: zeros to +0x1c,
// +0x28, +0x2c, +0xf0, +0x3c, +0x40, +0x58 .. +0x6c, -1 to +0xf4, +0x4c and +0x20, and copies of the globals listed
// in kCopies below (0x00489540-0x0048969f).
struct FieldCopy { unsigned off, global; };
const FieldCopy kCopies[] = {
    {0x24, 0x004e44cc}, {0x30, 0x004e4488}, {0x34, 0x0083b61c}, {0x44, 0x0083b624}, {0x48, 0x004e448c},
    {0x38, 0x0083b620}, {0x50, 0x004e4490}, {0x54, 0x004e4494}, {0x70, 0x004e4498}, {0xb0, 0x0083b634},
    {0xb4, 0x0083b638}, {0xb8, 0x0083b63c}, {0xbc, 0x0083b640}, {0x74, 0x0083b628}, {0x80, 0x004e449c},
    {0x8c, 0x004e44a8}, {0x98, 0x004e44b4}, {0xa4, 0x004e44c0}, {0x78, 0x0083b62c}, {0x84, 0x004e44a0},
    {0x90, 0x004e44ac}, {0x9c, 0x004e44b8}, {0xa8, 0x004e44c4}, {0x7c, 0x0083b630}, {0x88, 0x004e44a4},
    {0x94, 0x004e44b0}, {0xa0, 0x004e44bc}, {0xac, 0x004e44c8},
};
This0_t ListModelClear_orig;
void __fastcall ListModelClear_re(void* self, void*) {
    char* list = Ptr(self, 0xc0);
    if (At<void*>(list, 8) != 0) {
        ClearNodes(list);
        At<int>(list, 8) = 0;
        At<int>(list, 0x14) = 0;
        At<int>(list, 0x10) = 0;
    }
    At<int>(list, 0x14) = 0;
    reinterpret_cast<This0_t>(0x004747e0)(Ptr(self, 4), 0);
    for (unsigned off : {0x1cu, 0x28u, 0x2cu, 0xf0u, 0x3cu, 0x40u, 0x58u, 0x5cu, 0x60u, 0x64u, 0x68u, 0x6cu})
        At<int>(self, off) = 0;
    for (unsigned off : {0xf4u, 0x4cu, 0x20u})
        At<int>(self, off) = -1;
    for (const FieldCopy& c : kCopies)
        At<unsigned>(self, c.off) = G<unsigned>(c.global);
}

// 0x00489370  ListModel::dtor
// void __thiscall ListModel::~ListModel(): +0 = the vtable 0x004bb21c (0x00489391), ListModel::clear 0x004894b0
// (0x0048939f). The node list at +0xc0 gets the vtable 0x004bb22c (0x004893b1) and its virtual base (at +0xc4 +
// [[+0xc4]+4]) 0x004bb228 (0x004893bd); List402_clear 0x004026a0 runs on it (0x004893cc) and its +0x14 is cleared
// (0x004893d3). It then gets 0x004ba270 and 0x004ba26c (0x004893dd / 0x004893e9) and, when +8 is still non-zero
// (0x004893f6; List402_clear always leaves it 0), the node loop runs again through the new vtable (0x00489404-
// 0x0048945f) and +8 / +0x14 / +0x10 are cleared; +0x14 is cleared either way (0x0048946d). The dword at +0xec goes to
// the global 0x00839650 and +0xe8 = 0x004ba278 (0x00489470-0x00489479). Buf_dtor 0x00474810 runs on +4 (0x0048948a).
This0_t ListModelDtor_orig;
void __fastcall ListModelDtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004bb21c;
    reinterpret_cast<This0_t>(0x004894b0)(self, 0);
    char* list = Ptr(self, 0xc0);
    At<unsigned>(list, 0) = 0x004bb22c;
    At<unsigned>(Ptr(self, 0xc4), At<int>(At<void*>(self, 0xc4), 4)) = 0x004bb228;
    reinterpret_cast<This0_t>(0x004026a0)(list, 0);
    At<int>(list, 0x14) = 0;
    At<unsigned>(list, 0) = 0x004ba270;
    At<unsigned>(Ptr(self, 0xc4), At<int>(At<void*>(self, 0xc4), 4)) = 0x004ba26c;
    if (At<void*>(list, 8) != 0) {
        ClearNodes(list);
        At<int>(list, 8) = 0;
        At<int>(list, 0x14) = 0;
        At<int>(list, 0x10) = 0;
    }
    At<int>(list, 0x14) = 0;
    unsigned saved = At<unsigned>(self, 0xec);
    At<unsigned>(self, 0xe8) = 0x004ba278;
    G<unsigned>(0x00839650) = saved;
    reinterpret_cast<This0_t>(0x00474810)(Ptr(self, 4), 0);
}

// 0x004a33e0  panelSubDtor4a33e0
// void __thiscall panelSubDtor4a33e0(): `this` is +0x28 of a node list. The list gets the vtable 0x004ba2c4
// (0x004a33e7) and its virtual base (this - 0x24 + [[this-0x24]+4]) 0x004ba2c0 (0x004a33f3). An empty list (+8 == 0)
// only clears +0x14 (0x004a3400 -> 0x004a346d). Otherwise the node loop runs through the new vtable (slot 1 is
// 0x00402190, a bare `ret 4`; 0x004a340d-0x004a345a, none for a count <= 0 at 0x004a340a) and +0x14, +8, +0x10,
// +0x14 are cleared (0x004a345d-0x004a3466).
This0_t PanelSubDtor_orig;
void __fastcall PanelSubDtor_re(void* self, void*) {
    char* list = Ptr(self, -0x28);
    At<unsigned>(list, 0) = 0x004ba2c4;
    At<unsigned>(Ptr(self, -0x24), At<int>(At<void*>(self, -0x24), 4)) = 0x004ba2c0;
    if (At<void*>(list, 8) == 0) {
        At<int>(list, 0x14) = 0;
        return;
    }
    ClearNodes(list);
    At<int>(list, 0x14) = 0;
    At<int>(list, 8) = 0;
    At<int>(list, 0x10) = 0;
    At<int>(list, 0x14) = 0;
}

// =====================================================================================================================
// Part 4: IJG libjpeg 6a's memory manager (jmemmgr.c) and three jinit_* modules, as linked into the exe
// =====================================================================================================================
// my_memory_mgr (cinfo->mem, cinfo+4): pub methods +0 .. +0x2c (alloc_small +0, alloc_large +4, request_virt_barray
// +0x14), small_list[2] +0x30, large_list[2] +0x38, virt_sarray_list +0x40, virt_barray_list +0x44,
// total_space_allocated +0x48, last_rowsperchunk +0x4c. A pool header is {next +0, bytes_used +4, bytes_left +8},
// 0x10 bytes. cinfo->err (cinfo+0) holds error_exit at +0, msg_code +0x14 and msg_parm.i[0] +0x18.
typedef void(__cdecl* ErrorExit_t)(void*);
typedef void(__cdecl* OutOfMemory_t)(void*, int);
typedef void*(__cdecl* GetLarge_t)(void*, unsigned);
typedef void(__cdecl* FreeLarge_t)(void*, void*, unsigned);
const OutOfMemory_t kOutOfMemory = reinterpret_cast<OutOfMemory_t>(0x004af0a0);
const GetLarge_t kGetLarge = reinterpret_cast<GetLarge_t>(0x004b0460);
const FreeLarge_t kFreeLarge = reinterpret_cast<FreeLarge_t>(0x004b0470);

void ErrExit1(void* cinfo, int code, int parm) {
    char* err = At<char*>(cinfo, 0);
    At<int>(err, 0x14) = code;
    At<int>(err, 0x18) = parm;
    At<ErrorExit_t>(At<char*>(cinfo, 0), 0)(cinfo);
}
void ErrExit(void* cinfo, int code) {
    At<int>(At<char*>(cinfo, 0), 0x14) = code;
    At<ErrorExit_t>(At<char*>(cinfo, 0), 0)(cinfo);
}

// 0x004aef70  alloc_small
// void* alloc_small(cinfo, int pool_id, size_t size): a size over 0xffe0 calls out_of_memory 0x004af0a0 with 1
// (0x004aef83-0x004aef8e); the size is rounded up to a multiple of 8 (0x004aef96-0x004aefa6). A pool id outside 0..1
// (signed, 0x004aefb0 / 0x004aefb5) is ERREXIT1(0xc, id) (0x004aefb7-0x004aefc8) and the id is used as it is. The
// pool chain small_list[id] is searched for the first header with bytes_left >= size (unsigned `jae` at 0x004aefde),
// remembering the last header passed (0x004aefe0). Found: the object is carved from it (0x004af084). Not found: the
// minimum request is size + 0x10 (0x004aeff2) and the slop first_pool_slop[id] (table 0x004bd09c) for an empty chain
// or extra_pool_slop[id] (0x004bd0a4) after the last header (0x004aeff5-0x004af002), capped at 0xfff0 - minimum
// (0x004af009-0x004af014). jpeg_get_large 0x004b0460 is asked for minimum + slop (0x004af01b); NULL halves the slop
// (0x004af027), calls out_of_memory with 2 once it is below 50 (0x004af029-0x004af031) and asks again (0x004af03e).
// total_space_allocated += minimum + slop (0x004af04e-0x004af05c), the new header gets next 0, used 0, left = rounded
// size + slop (0x004af061-0x004af06e) and is linked after the last header or as the chain's first (0x004af071-
// 0x004af07d). Carving: the object is header + 0x10 + bytes_used (0x004af087), bytes_used += size, bytes_left -= size
// (0x004af08b-0x004af097).
typedef void*(__cdecl* AllocSmall_t)(void*, int, unsigned);
AllocSmall_t AllocSmall_orig;
void* __cdecl AllocSmall_re(void* cinfo, int pool_id, unsigned size) {
    char* mem = At<char*>(cinfo, 4);
    if (size > 0xffe0)
        kOutOfMemory(cinfo, 1);
    if (size & 7)
        size += 8 - (size & 7);
    if (pool_id < 0 || pool_id >= 2)
        ErrExit1(cinfo, 0xc, pool_id);
    char* prev = 0;
    char* hdr = At<char*>(mem, 0x30 + 4 * pool_id);
    while (hdr != 0) {
        if (At<unsigned>(hdr, 8) >= size)
            break;
        prev = hdr;
        hdr = At<char*>(hdr, 0);
    }
    if (hdr == 0) {
        unsigned minRequest = size + 0x10;
        unsigned slop = prev == 0 ? G<unsigned>(0x004bd09c + 4 * pool_id) : G<unsigned>(0x004bd0a4 + 4 * pool_id);
        if (slop > 0xfff0 - minRequest)
            slop = 0xfff0 - minRequest;
        for (;;) {
            hdr = static_cast<char*>(kGetLarge(cinfo, minRequest + slop));
            if (hdr != 0)
                break;
            slop >>= 1;
            if (slop < 50)
                kOutOfMemory(cinfo, 2);
        }
        At<unsigned>(mem, 0x48) += minRequest + slop;
        At<unsigned>(hdr, 0) = 0;
        At<unsigned>(hdr, 4) = 0;
        At<unsigned>(hdr, 8) = slop + size;
        if (prev == 0)
            At<char*>(mem, 0x30 + 4 * pool_id) = hdr;
        else
            At<char*>(prev, 0) = hdr;
    }
    char* data = hdr + At<unsigned>(hdr, 4) + 0x10;
    At<unsigned>(hdr, 4) += size;
    At<unsigned>(hdr, 8) -= size;
    return data;
}

// 0x004af0c0  alloc_large
// void* alloc_large(cinfo, int pool_id, size_t size): out_of_memory 0x004af0a0 with 3 for a size over 0xffe0
// (0x004af0cf-0x004af0da), rounding to 8 (0x004af0e2-0x004af0f0), ERREXIT1(0xc, id) for an id outside 0..1
// (0x004af0f6-0x004af113). jpeg_get_large 0x004b0460 is asked for size + 0x10 (0x004af118-0x004af11d); NULL calls
// out_of_memory with 4 (0x004af12b-0x004af12e) and the code goes on with the NULL header (a returning error_exit would
// fault there). total_space_allocated += size + 0x10 (0x004af136-0x004af13e); the header gets next = large_list[id],
// used = size, left = 0 (0x004af145-0x004af14e) and becomes large_list[id] (0x004af155); the object is header + 0x10.
AllocSmall_t AllocLarge_orig;
void* __cdecl AllocLarge_re(void* cinfo, int pool_id, unsigned size) {
    char* mem = At<char*>(cinfo, 4);
    if (size > 0xffe0)
        kOutOfMemory(cinfo, 3);
    if (size & 7)
        size += 8 - (size & 7);
    if (pool_id < 0 || pool_id >= 2)
        ErrExit1(cinfo, 0xc, pool_id);
    char* hdr = static_cast<char*>(kGetLarge(cinfo, size + 0x10));
    if (hdr == 0)
        kOutOfMemory(cinfo, 4);
    At<unsigned>(mem, 0x48) += size + 0x10;
    char* next = At<char*>(mem, 0x38 + 4 * pool_id);
    At<unsigned>(hdr, 4) = size;
    At<char*>(hdr, 0) = next;
    At<unsigned>(hdr, 8) = 0;
    At<char*>(mem, 0x38 + 4 * pool_id) = hdr;
    return hdr + 0x10;
}

// 0x004af170  alloc_sarray
// JSAMPARRAY alloc_sarray(cinfo, int pool_id, JDIMENSION samplesperrow, JDIMENSION numrows): rows per chunk =
// 0xffe0 / samplesperrow (unsigned `div` at 0x004af17c); a quotient <= 0 is ERREXIT(0x45) (0x004af18a-0x004af19a).
// It is capped at numrows (signed, 0x004af1a3 / 0x004af1a5) and stored in mem->last_rowsperchunk (0x004af1b8). The row
// pointer array comes from alloc_small 0x004aef70 (numrows * 4 bytes, 0x004af1b1-0x004af1be). Then while the rows done
// are below numrows (unsigned, 0x004af1ca / 0x004af20b) the chunk is capped at the rows left (unsigned, 0x004af1d4),
// alloc_large 0x004af0c0 gives chunk * samplesperrow bytes (0x004af1e0-0x004af1ea) and each row of the chunk gets
// its pointer, samplesperrow bytes apart (0x004af1f6-0x004af209). Returns the row array.
// 0x004af220  alloc_barray is the same over blocks: 0xffe0 / (blocksperrow << 7) (0x004af225-0x004af22f) and chunks
// of chunk * blocksperrow << 7 bytes (0x004af28d-0x004af29c), rows (blocksperrow << 7) bytes apart (0x004af2be).
typedef void*(__cdecl* AllocArray_t)(void*, int, unsigned, unsigned);
void* AllocRows(void* cinfo, int pool_id, unsigned rowBytes, unsigned numrows) {
    char* mem = At<char*>(cinfo, 4);
    unsigned per = 0xffe0u / rowBytes;
    if (static_cast<int>(per) <= 0)
        ErrExit(cinfo, 0x45);
    if (!(static_cast<int>(per) < static_cast<int>(numrows)))
        per = numrows;
    At<unsigned>(mem, 0x4c) = per;
    char** result = static_cast<char**>(reinterpret_cast<AllocSmall_t>(0x004aef70)(cinfo, pool_id, numrows * 4));
    unsigned row = 0;
    while (row < numrows) {
        if (!(per < numrows - row))
            per = numrows - row;
        char* work = static_cast<char*>(reinterpret_cast<AllocSmall_t>(0x004af0c0)(cinfo, pool_id, per * rowBytes));
        for (unsigned i = per; i != 0; i--) {
            result[row++] = work;
            work += rowBytes;
        }
    }
    return result;
}
AllocArray_t AllocSarray_orig;
void* __cdecl AllocSarray_re(void* cinfo, int pool_id, unsigned samplesperrow, unsigned numrows) {
    return AllocRows(cinfo, pool_id, samplesperrow, numrows);
}
AllocArray_t AllocBarray_orig;
void* __cdecl AllocBarray_re(void* cinfo, int pool_id, unsigned blocksperrow, unsigned numrows) {
    return AllocRows(cinfo, pool_id, blocksperrow << 7, numrows);
}

// 0x004af920  free_pool
// void free_pool(cinfo, int pool_id): ERREXIT1(0xc, id) for an id outside 0..1 (0x004af931-0x004af94e), the id used
// as it is afterwards. For pool 1 (JPOOL_IMAGE, 0x004af953) every virtual sample array (mem->virt_sarray_list +0x40,
// linked through +0x2c) and then every virtual block array (+0x44) whose b_s_open flag +0x28 is set gets the flag
// cleared and its close_backing_store (+0x38) called with (cinfo, &its backing store info +0x30) (0x004af95f-
// 0x004af96c, 0x004af983-0x004af990); both list heads are cleared (0x004af97c / 0x004af9a1). Then the large pool chain
// large_list[id] (+0x38) is detached (0x004af9a8) and each header handed to jpeg_free_large 0x004b0470 with its
// space = bytes_left + bytes_used + 0x10 (0x004af9b0-0x004af9bf), total_space_allocated dropping by the same amount
// (0x004af9ca); then small_list[id] (+0x30) the same way (0x004af9df-0x004afa07; jpeg_free_small is the same address).
typedef void(__cdecl* FreePool_t)(void*, int);
typedef void(__cdecl* CloseBs_t)(void*, void*);
void FreeChain(void* cinfo, char* mem, unsigned slot) {
    char* hdr = At<char*>(mem, slot);
    At<char*>(mem, slot) = 0;
    while (hdr != 0) {
        char* next = At<char*>(hdr, 0);
        unsigned space = At<unsigned>(hdr, 8) + At<unsigned>(hdr, 4) + 0x10;
        kFreeLarge(cinfo, hdr, space);
        At<unsigned>(mem, 0x48) -= space;
        hdr = next;
    }
}
void CloseVirtList(void* cinfo, char* arr) {
    for (; arr != 0; arr = At<char*>(arr, 0x2c)) {
        if (At<int>(arr, 0x28) != 0) {
            At<int>(arr, 0x28) = 0;
            At<CloseBs_t>(arr, 0x38)(cinfo, Ptr(arr, 0x30));
        }
    }
}
FreePool_t FreePool_orig;
void __cdecl FreePool_re(void* cinfo, int pool_id) {
    char* mem = At<char*>(cinfo, 4);
    if (pool_id < 0 || pool_id >= 2)
        ErrExit1(cinfo, 0xc, pool_id);
    if (pool_id == 1) {
        CloseVirtList(cinfo, At<char*>(mem, 0x40));
        At<char*>(mem, 0x40) = 0;
        CloseVirtList(cinfo, At<char*>(mem, 0x44));
        At<char*>(mem, 0x44) = 0;
    }
    FreeChain(cinfo, mem, 0x38 + 4 * pool_id);
    FreeChain(cinfo, mem, 0x30 + 4 * pool_id);
}

// 0x004afa20  self_destruct_jmemmgr
// void self_destruct(cinfo): free_pool 0x004af920 for pool 1 then pool 0 (`dec esi; jns` at 0x004afa35 / 0x004afa36),
// jpeg_free_small (0x004b0470) of the manager itself with size 0x50 (0x004afa38-0x004afa3f), cinfo->mem = 0
// (0x004afa45), jpeg_mem_term 0x004a0890 (a bare `ret`, 0x004afa4c). Installed as mem->self_destruct by
// jinit_memory_mgr.
typedef void(__cdecl* JCommon_t)(void*);
JCommon_t SelfDestruct_orig;
void __cdecl SelfDestruct_re(void* cinfo) {
    for (int pool = 1; pool >= 0; pool--)
        reinterpret_cast<FreePool_t>(0x004af920)(cinfo, pool);
    kFreeLarge(cinfo, At<void*>(cinfo, 4), 0x50);
    At<void*>(cinfo, 4) = 0;
    reinterpret_cast<JCommon_t>(0x004a0890)(cinfo);
}

// 0x004afaf0  jinit_marker_writer
// void jinit_marker_writer(cinfo): a 0x18-byte object from (*cinfo->mem->alloc_small)(cinfo, 1, 0x18) (0x004afaf5-
// 0x004afafd) becomes cinfo->marker (+0x154, 0x004afaff) and gets its six methods: write_file_header 0x004afb60,
// write_frame_header 0x004afc40, write_scan_header 0x004afdc0, write_file_trailer 0x004b0060, write_tables_only
// 0x004b0300, write_marker_header 0x004b0320 (0x004afb05-0x004afb49), each store re-reading cinfo->marker.
typedef void*(__cdecl* MemAlloc_t)(void*, int, unsigned);
JCommon_t JinitMarkerWriter_orig;
void __cdecl JinitMarkerWriter_re(void* cinfo) {
    void* m = At<MemAlloc_t>(At<char*>(cinfo, 4), 0)(cinfo, 1, 0x18);
    At<void*>(cinfo, 0x154) = m;
    const unsigned methods[6] = {0x004afb60, 0x004afc40, 0x004afdc0, 0x004b0060, 0x004b0300, 0x004b0320};
    for (int i = 0; i < 6; i++)
        At<unsigned>(At<void*>(cinfo, 0x154), 4 * i) = methods[i];
}

// 0x004b06f0  jinit_c_coef_controller
// void jinit_c_coef_controller(cinfo, boolean need_full_buffer): a 0x68-byte controller from (*mem->alloc_small)(cinfo,
// 1, 0x68) (0x004b06f7-0x004b06ff) becomes cinfo->coef (+0x150, 0x004b070c) with start_pass 0x004b07b0 (0x004b0714).
// need_full_buffer (0x004b071a): for each of num_components (+0x34; none for <= 0 at 0x004b0728) components of the
// comp_info array (+0x3c, 0x54 bytes each) whole_image[ci] (controller +0x40 + 4ci) = (*mem->request_virt_barray)
// (cinfo, 1, FALSE, jround_up(width_in_blocks +0x1c, h_samp_factor +8), jround_up(height_in_blocks +0x20,
// v_samp_factor +0xc), v_samp_factor) (0x004b0731-0x004b075e; jround_up 0x004b04d0, the height one computed first).
// Otherwise one 0x500-byte buffer from (*mem->alloc_large)(cinfo, 1, 0x500) (0x004b077e-0x004b0789) is split into the
// ten MCU_buffer pointers (+0x18, 0x80 bytes apart, 0x004b0792-0x004b07a2) and whole_image[0] = NULL (0x004b07a4).
typedef void(__cdecl* JinitBool_t)(void*, int);
typedef int(__cdecl* JroundUp_t)(int, int);
typedef void*(__cdecl* RequestVirt_t)(void*, int, int, int, int, int);
JinitBool_t JinitCoef_orig;
void __cdecl JinitCoef_re(void* cinfo, int needFull) {
    char* mem = At<char*>(cinfo, 4);
    char* coef = static_cast<char*>(At<MemAlloc_t>(mem, 0)(cinfo, 1, 0x68));
    At<char*>(cinfo, 0x150) = coef;
    At<unsigned>(coef, 0) = 0x004b07b0;
    if (needFull != 0) {
        char* comp = At<char*>(cinfo, 0x3c);
        for (int ci = 0; ci < At<int>(cinfo, 0x34); ci++, comp += 0x54) {
            const JroundUp_t roundUp = reinterpret_cast<JroundUp_t>(0x004b04d0);
            int rows = roundUp(At<int>(comp, 0x20), At<int>(comp, 0xc));
            int cols = roundUp(At<int>(comp, 0x1c), At<int>(comp, 8));
            At<void*>(coef, 0x40 + 4 * ci) =
                At<RequestVirt_t>(At<char*>(cinfo, 4), 0x14)(cinfo, 1, 0, cols, rows, At<int>(comp, 0xc));
        }
        return;
    }
    char* buf = static_cast<char*>(At<MemAlloc_t>(At<char*>(cinfo, 4), 4)(cinfo, 1, 0x500));
    for (int i = 0; i < 10; i++)
        At<char*>(coef, 0x18 + 4 * i) = buf + 0x80 * i;
    At<void*>(coef, 0x40) = 0;
}

// 0x004b12b0  jinit_huff_encoder
// void jinit_huff_encoder(cinfo): a 0x6c-byte encoder from (*mem->alloc_small)(cinfo, 1, 0x6c) (0x004b12b5-
// 0x004b12bd) becomes cinfo->entropy (+0x164, 0x004b12c2) with start_pass 0x004b12f0 (0x004b12c8); for i = 0..3 the
// four tables dc_derived_tbls +0x2c, ac_derived_tbls +0x3c, dc_count_ptrs +0x4c and ac_count_ptrs +0x5c get NULL
// (0x004b12d9-0x004b12e8).
JCommon_t JinitHuff_orig;
void __cdecl JinitHuff_re(void* cinfo) {
    char* e = static_cast<char*>(At<MemAlloc_t>(At<char*>(cinfo, 4), 0)(cinfo, 1, 0x6c));
    At<char*>(cinfo, 0x164) = e;
    At<unsigned>(e, 0) = 0x004b12f0;
    for (int i = 0; i < 4; i++) {
        At<void*>(e, 0x3c + 4 * i) = 0;
        At<void*>(e, 0x2c + 4 * i) = 0;
        At<void*>(e, 0x5c + 4 * i) = 0;
        At<void*>(e, 0x4c + 4 * i) = 0;
    }
}

// 0x004ae850  std_huff_tables
// void std_huff_tables(cinfo): add_huff_table 0x004ae8b0 four times, with the standard tables in the exe: DC luminance
// (&dc_huff_tbl_ptrs[0] cinfo+0x50, bits 0x004bcd08, val 0x004bcd1c; 0x004ae855-0x004ae864), AC luminance (+0x60,
// 0x004bcd48, 0x004bcd5c; 0x004ae869-0x004ae878), DC chrominance (+0x54, 0x004bcd28, 0x004bcd3c; 0x004ae87d-0x004ae88c)
// and AC chrominance (+0x64, 0x004bce00, 0x004bce14; 0x004ae891-0x004ae8a0).
typedef void(__cdecl* AddHuffTable_t)(void*, void*, const void*, const void*);
JCommon_t StdHuffTables_orig;
void __cdecl StdHuffTables_re(void* cinfo) {
    const AddHuffTable_t add = reinterpret_cast<AddHuffTable_t>(0x004ae8b0);
    add(cinfo, Ptr(cinfo, 0x50), reinterpret_cast<void*>(0x004bcd08), reinterpret_cast<void*>(0x004bcd1c));
    add(cinfo, Ptr(cinfo, 0x60), reinterpret_cast<void*>(0x004bcd48), reinterpret_cast<void*>(0x004bcd5c));
    add(cinfo, Ptr(cinfo, 0x54), reinterpret_cast<void*>(0x004bcd28), reinterpret_cast<void*>(0x004bcd3c));
    add(cinfo, Ptr(cinfo, 0x64), reinterpret_cast<void*>(0x004bce00), reinterpret_cast<void*>(0x004bce14));
}

// 0x004b3380  finish_pass_phuff
// void finish_pass_phuff(cinfo): the progressive Huffman encoder is cinfo->entropy (+0x164). Its next_output_byte
// (+0x10) and free_in_buffer (+0x14) are loaded from the destination manager cinfo->dest (+0x14) {next_output_byte +0,
// free_in_buffer +4} (0x004b3386-0x004b339b), emit_eobrun 0x004b25d0 (0x004b339e) and flush_bits_4b2510 0x004b2510
// (0x004b33a4) run on the encoder, and the two fields go back to the destination (0x004b33a9-0x004b33bb). Installed
// as entropy->finish_pass by start_pass_phuff (`mov [ebx+8], 0x4b3380` at 0x004b1ff9).
typedef void(__cdecl* Entropy_t)(void*);
JCommon_t FinishPassPhuff_orig;
void __cdecl FinishPassPhuff_re(void* cinfo) {
    char* e = At<char*>(cinfo, 0x164);
    At<unsigned>(e, 0x10) = At<unsigned>(At<char*>(cinfo, 0x14), 0);
    At<unsigned>(e, 0x14) = At<unsigned>(At<char*>(cinfo, 0x14), 4);
    reinterpret_cast<Entropy_t>(0x004b25d0)(e);
    reinterpret_cast<Entropy_t>(0x004b2510)(e);
    At<unsigned>(At<char*>(cinfo, 0x14), 0) = At<unsigned>(e, 0x10);
    At<unsigned>(At<char*>(cinfo, 0x14), 4) = At<unsigned>(e, 0x14);
}

// =====================================================================================================================
// Part 5: the timer lock counter
// =====================================================================================================================

// 0x00486fe0  Timer::lockInc
// int __cdecl Timer::lockInc(): increments the global 0x0083afec (load, inc, store at 0x00486fe0-0x00486fe6) and
// returns 0 (0x00486feb).
Cdecl0_t TimerLockInc_orig;
int __cdecl TimerLockInc_re() {
    G<int>(0x0083afec) = G<int>(0x0083afec) + 1;
    return 0;
}

// 0x00486ff0  Timer::lockDec
// void __cdecl Timer::lockDec(): `dec dword ptr [0x0083afec]` (0x00486ff0); eax is not written.
CdeclVoid0_t TimerLockDec_orig;
void __cdecl TimerLockDec_re() {
    G<int>(0x0083afec) -= 1;
}

}  // namespace

// ---- registrations ---------------------------------------------------------------------------------------------
// 0x004a4ffc  operatorDelete_4a4ffc
SG_HOOK("golf_clean.exe", 0x004a4ffc, operatorDelete_4a4ffc, OperatorDelete_re, OperatorDelete_orig);
// 0x00479b40  ChildArray_grow
SG_HOOK("golf_clean.exe", 0x00479b40, ChildArray_grow, ChildArrayGrow_re, ChildArrayGrow_orig);
// 0x00479bb0  Vec_release
SG_HOOK("golf_clean.exe", 0x00479bb0, Vec_release, VecRelease_re, VecRelease_orig);
// 0x0047e5f0  ChildArray::insert
SG_HOOK("golf_clean.exe", 0x0047e5f0, ChildArray_insert, ChildArrayInsert_re, ChildArrayInsert_orig);
// 0x0047e680  ChildArray::remove
SG_HOOK("golf_clean.exe", 0x0047e680, ChildArray_remove, ChildArrayRemove_re, ChildArrayRemove_orig);
// 0x00474820  Buf_alloc
SG_HOOK("golf_clean.exe", 0x00474820, Buf_alloc, BufAlloc_re, BufAlloc_orig);
// 0x00486250  EditBox::resizeBuffer
SG_HOOK("golf_clean.exe", 0x00486250, EditBox_resizeBuffer, EditBoxResize_re, EditBoxResize_orig);
// 0x00488a20  Button::setTooltip
SG_HOOK("golf_clean.exe", 0x00488a20, Button_setTooltip, ButtonSetTooltip_re, ButtonSetTooltip_orig);
// 0x004a29c0  setStringField
SG_HOOK("golf_clean.exe", 0x004a29c0, setStringField, SetStringField_re, SetStringField_orig);
// 0x004a4db0  listPush
SG_HOOK("golf_clean.exe", 0x004a4db0, listPush, ListPush_re, ListPush_orig);
// 0x0044ac00  listEraseRange
SG_HOOK("golf_clean.exe", 0x0044ac00, listEraseRange, ListEraseRange_re, ListEraseRange_orig);
// 0x00449210  allocTileBuffer
SG_HOOK("golf_clean.exe", 0x00449210, allocTileBuffer, AllocTileBuffer_re, AllocTileBuffer_orig);
// 0x0047ada0  View_unlinkFromParent
SG_HOOK("golf_clean.exe", 0x0047ada0, View_unlinkFromParent, ViewUnlink_re, ViewUnlink_orig);
// 0x0047b8f0  Label::setText
SG_HOOK("golf_clean.exe", 0x0047b8f0, Label_setText, LabelSetText_re, LabelSetText_orig);
// 0x0048de00  MsgBox::setLabelA
SG_HOOK("golf_clean.exe", 0x0048de00, MsgBox_setLabelA, SetLabelA_re, SetLabelA_orig);
// 0x0048de90  MsgBox::setLabelB
SG_HOOK("golf_clean.exe", 0x0048de90, MsgBox_setLabelB, SetLabelB_re, SetLabelB_orig);
// 0x00490bf0  MsgBox::initDefaults
SG_HOOK("golf_clean.exe", 0x00490bf0, MsgBox_initDefaults, MsgBoxInit_re, MsgBoxInit_orig);
// 0x00492660  HotList::freeEntryTip
SG_HOOK("golf_clean.exe", 0x00492660, HotList_freeEntryTip, FreeEntryTip_re, FreeEntryTip_orig);
// 0x0049d460  freeFileWinBuf
SG_HOOK("golf_clean.exe", 0x0049d460, freeFileWinBuf, FreeFileWinBuf_re, FreeFileWinBuf_orig);
// 0x00481b20  freeResampleTable
SG_HOOK("golf_clean.exe", 0x00481b20, freeResampleTable, FreeResampleTable_re, FreeResampleTable_orig);
// 0x00473ab0  Surface_ctor
SG_HOOK("golf_clean.exe", 0x00473ab0, Surface_ctor, SurfaceCtor_re, SurfaceCtor_orig);
// 0x00473ae0  Surface_free
SG_HOOK("golf_clean.exe", 0x00473ae0, Surface_free, SurfaceFree_re, SurfaceFree_orig);
// 0x00473b50  Surface_open
SG_HOOK("golf_clean.exe", 0x00473b50, Surface_open, SurfaceOpen_re, SurfaceOpen_orig);
// 0x00483070  Cache483070::reset
SG_HOOK("golf_clean.exe", 0x00483070, Cache483070_reset, CacheReset_re, CacheReset_orig);
// 0x00483010  Cache483010::dtor
SG_HOOK("golf_clean.exe", 0x00483010, Cache483010_dtor, CacheDtor_re, CacheDtor_orig);
// 0x0049ff30  freeFonts
SG_HOOK("golf_clean.exe", 0x0049ff30, freeFonts, FreeFonts_re, FreeFonts_orig);
// 0x0049c830  createCache
SG_HOOK("golf_clean.exe", 0x0049c830, createCache, CreateCache_re, CreateCache_orig);
// 0x00482f60  Cache482f60::ctor
SG_HOOK("golf_clean.exe", 0x00482f60, Cache482f60_ctor, Cache482f60Ctor_re, Cache482f60Ctor_orig);
// 0x00483750  Cache482f60::scalarDtorAdj20
SG_HOOK("golf_clean.exe", 0x00483750, Cache482f60_scalarDtorAdj20, CacheScalarDtorAdj20_re, CacheScalarDtorAdj20_orig);
// 0x0049d050  allocPairArray
SG_HOOK("golf_clean.exe", 0x0049d050, allocPairArray, AllocPairArray_re, AllocPairArray_orig);
// 0x0049d010  initPairArrayGlobal
SG_HOOK("golf_clean.exe", 0x0049d010, initPairArrayGlobal, InitPairArrayGlobal_re, InitPairArrayGlobal_orig);
// 0x004026a0  List402_clear
SG_HOOK("golf_clean.exe", 0x004026a0, List402_clear, List402Clear_re, List402Clear_orig);
// 0x004894b0  ListModel::clear
SG_HOOK("golf_clean.exe", 0x004894b0, ListModel_clear, ListModelClear_re, ListModelClear_orig);
// 0x00489370  ListModel::dtor
SG_HOOK("golf_clean.exe", 0x00489370, ListModel_dtor, ListModelDtor_re, ListModelDtor_orig);
// 0x004a33e0  panelSubDtor4a33e0
SG_HOOK("golf_clean.exe", 0x004a33e0, panelSubDtor4a33e0, PanelSubDtor_re, PanelSubDtor_orig);
// 0x004aef70  alloc_small
SG_HOOK("golf_clean.exe", 0x004aef70, alloc_small, AllocSmall_re, AllocSmall_orig);
// 0x004af0c0  alloc_large
SG_HOOK("golf_clean.exe", 0x004af0c0, alloc_large, AllocLarge_re, AllocLarge_orig);
// 0x004af170  alloc_sarray
SG_HOOK("golf_clean.exe", 0x004af170, alloc_sarray, AllocSarray_re, AllocSarray_orig);
// 0x004af220  alloc_barray
SG_HOOK("golf_clean.exe", 0x004af220, alloc_barray, AllocBarray_re, AllocBarray_orig);
// 0x004af920  free_pool
SG_HOOK("golf_clean.exe", 0x004af920, free_pool, FreePool_re, FreePool_orig);
// 0x004afa20  self_destruct_jmemmgr
SG_HOOK("golf_clean.exe", 0x004afa20, self_destruct_jmemmgr, SelfDestruct_re, SelfDestruct_orig);
// 0x004afaf0  jinit_marker_writer
SG_HOOK("golf_clean.exe", 0x004afaf0, jinit_marker_writer, JinitMarkerWriter_re, JinitMarkerWriter_orig);
// 0x004b06f0  jinit_c_coef_controller
SG_HOOK("golf_clean.exe", 0x004b06f0, jinit_c_coef_controller, JinitCoef_re, JinitCoef_orig);
// 0x004b12b0  jinit_huff_encoder
SG_HOOK("golf_clean.exe", 0x004b12b0, jinit_huff_encoder, JinitHuff_re, JinitHuff_orig);
// 0x004ae850  std_huff_tables
SG_HOOK("golf_clean.exe", 0x004ae850, std_huff_tables, StdHuffTables_re, StdHuffTables_orig);
// 0x004b3380  finish_pass_phuff
SG_HOOK("golf_clean.exe", 0x004b3380, finish_pass_phuff, FinishPassPhuff_re, FinishPassPhuff_orig);
// 0x00486fe0  Timer::lockInc
SG_HOOK("golf_clean.exe", 0x00486fe0, Timer_lockInc, TimerLockInc_re, TimerLockInc_orig);
// 0x00486ff0  Timer::lockDec
SG_HOOK("golf_clean.exe", 0x00486ff0, Timer_lockDec, TimerLockDec_re, TimerLockDec_orig);
