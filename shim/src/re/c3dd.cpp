// C3 batch c3dd (round 25, 2026-10-10) of golf_clean.exe: the rows c3cz dropped because they reach the heap through
// _realloc 0x004a6980 (heapAlloc 0x00474860 grows its block in 0x400-byte steps through it) and the callers that only
// allocate through heapAlloc. The fixtures (re/frida/js/fixtures.d/c3dd.js) replace malloc 0x004a504f, operator new
// 0x004a57f1, free 0x004a5007 and realloc 0x004a6980 for the length of one key with the heap stand-in
// (re/frida/js/heapstandin.js: arena allocator, recording deallocator, realloc that copies into a new arena block and
// records the old pointer), so both arms get the same blocks and every free / realloc is compared state.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). The exe
// has no relocations, so its addresses are literal constants; every callee runs through its original address:
//   heapAlloc            -> _realloc 0x004a6980, appendNewline 0x004925f0 (C3), __itoa 0x004ad425, _exit 0x004a5108,
//                           MessageBoxA through the IAT slot 0x004ba1f4 (the out-of-memory arm only)
//   strDup               -> heapAlloc 0x00474860 (THIS batch)
//   strAssign            -> strFree 0x0049d0e0 (C3, c3cn), Buf_alloc 0x00474820 (C3, c3cz), strDup 0x0049d100 (THIS batch)
//   bufferCopyTo, Surface_get -> heapAlloc, malloc 0x004a504f
//   S3List_add / add2    -> heapAlloc, malloc, the list's virtual slot 0
//   View_attach          -> heapAlloc, malloc
//   TextView::addImage, postMessageLine, listAddEntry -> S3List_add 0x00401d10 (THIS batch); listAddEntry also free
//                           0x004a5007 and malloc
// Method pointers (the list's vtable slot 0, the vbtable entry that locates a virtual base) are read from their
// slots in both arms, so the fixture's recording stubs are what runs there.
#include "hooks.h"

namespace {

template <class T>
T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <class T>
T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
char* Ptr(void* p, int off) { return static_cast<char*>(p) + off; }

typedef void*(__cdecl* Malloc_t)(unsigned);
typedef void(__cdecl* Free_t)(void*);
typedef void*(__cdecl* Realloc_t)(void*, unsigned);
const Malloc_t kMalloc = reinterpret_cast<Malloc_t>(0x004a504f);     // malloc
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);           // free
const Realloc_t kRealloc = reinterpret_cast<Realloc_t>(0x004a6980);  // _realloc

typedef void(__fastcall* This0_t)(void*, void*);
typedef void(__fastcall* This1_t)(void*, void*, int);
typedef int(__fastcall* ThisRet1_t)(void*, void*, int);
typedef char*(__fastcall* HeapAlloc_t)(void*, void*, int);
const HeapAlloc_t kHeapAlloc = reinterpret_cast<HeapAlloc_t>(0x00474860);
const ThisRet1_t kS3ListAdd = reinterpret_cast<ThisRet1_t>(0x00401d10);
const ThisRet1_t kBufAlloc = reinterpret_cast<ThisRet1_t>(0x00474820);
const This0_t kStrFree = reinterpret_cast<This0_t>(0x0049d0e0);
typedef char*(__fastcall* StrDup_t)(void*, void*, const char*);
const StrDup_t kStrDup = reinterpret_cast<StrDup_t>(0x0049d100);

typedef void(__cdecl* AppendNewline_t)(char*);
typedef char*(__cdecl* Itoa_t)(int, char*, int);
typedef void(__cdecl* Exit_t)(int);
typedef int(__stdcall* MessageBoxA_t)(void*, const char*, const char*, unsigned);
const AppendNewline_t kAppendNewline = reinterpret_cast<AppendNewline_t>(0x004925f0);
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004ad425);
const Exit_t kExit = reinterpret_cast<Exit_t>(0x004a5108);

// A virtual call: [[obj] + slot].
template <class F>
F Slot(void* obj, unsigned slot) { return *reinterpret_cast<F*>(At<char*>(obj, 0) + slot); }

unsigned SizeWithNul(const char* s) {
    unsigned n = 0;
    while (s[n] != 0) n++;
    return n + 1;
}
// The copy the callers inline after storing a NUL in the new block: find the end of the destination with a second
// scasb, then `rep movsd` / `rep movsb` the whole source including its NUL (0x004746c6-0x004746e0 in Surface_get,
// 0x0049d12a-0x0049d13c in strDup). The destination holds an empty string, so the result is the source copied to its
// start.
void AppendCopy(char* dst, const char* src) {
    while (*dst != 0) dst++;
    unsigned n = SizeWithNul(src);
    for (unsigned i = 0; i < n; i++) dst[i] = src[i];
}
// strcat as VC6 inlines it in heapAlloc's message arm (the scasb / movsd / movsb runs at 0x004748d1-0x00474908).
void Cat(char* dst, const char* src) {
    AppendCopy(dst, src);
}

// The allocator choice every record copier makes: heapAlloc 0x00474860 on the heap object when there is one, else malloc
// 0x004a504f (e.g. 0x004a0553-0x004a0563 / 0x004a056a-0x004a0574).
void* AllocFrom(void* heap, unsigned n) {
    if (heap != 0)
        return kHeapAlloc(heap, 0, static_cast<int>(n));
    return kMalloc(n);
}

// ---- the out-of-memory arms ---------------------------------------------------------------------------------------
// S3List_add, S3List_add2 and netHeapAlloc test heapAlloc's result and, on NULL, show a message box and exit:
// `push 0; push 0x4c1458; push 0x4c1434; push 0; call [0x004ba1f4]` (MessageBoxA(0, text 0x4c1434, caption 0x4c1458,
// 0)) then `push 3; call 0x004a5108` (_exit(3)), e.g. 0x00401d49-0x00401d5d. The fixtures cannot run this arm: the
// game would exit.
void NoMemory() {
    (*reinterpret_cast<MessageBoxA_t*>(0x004ba1f4))(0, reinterpret_cast<const char*>(0x004c1434),
                                                       reinterpret_cast<const char*>(0x004c1458), 0);
    kExit(3);
}

// ---- the virtual-base objects S3List_add allocates ------------------------------------------------------------------
// Each is allocated with the class-specific operator new that parks the heap in the global 0x00839650 (0x00401d41
// `mov [0x839650], edi`) and constructed inline: [obj] = the class's vbtable pointer, a base-class vptr at one offset,
// the saved heap (the global, then cleared: 0x00401dbe-0x00401dc7) behind it, and the final vptr stored in the virtual
// base at [[obj] + 4] + obj (0x00401dcd-0x00401dd2).
struct ObjSpec {
    unsigned size, vptr, baseOff, savedOff, finalVptr;
    unsigned vbOff;   // where the vbtable pointer sits (0, or 0x58 in netHeapAlloc's 0x64-byte object)
    bool voice;       // run VoiceRx::ctor 0x004852e0 on the block and store the vtable 0x004bbc3c at +0 first
};
const ObjSpec kNode = {0x1c, 0x004ba28c, 0x14, 0x18, 0x004ba288, 0, false};   // 0x00401db5-0x00401dd2; base vptr 0x004ba278
const ObjSpec kObj18 = {0x18, 0x004ba280, 0x10, 0x14, 0x004ba27c, 0, false};  // S3List_add's second object, 0x00401e1e-0x00401e3b
const ObjSpec kObj10 = {0x10, 0x004ba2b8, 0x08, 0x0c, 0x004ba2b4, 0, false};  // S3List_add2's second object, 0x0040238e-0x004023ab
// netHeapAlloc's second object: 0x64 bytes, vbptr 0x004bbcd4 at +0x58, base vptr at +0x5c, saved heap at +0x60
// (0x0049badd-0x0049baf0), VoiceRx::ctor 0x004852e0 (0x0049bbbb), the vtable 0x004bbc3c at +0 (0x0049bbc3) and the
// final vptr 0x004bbc38 at +0x58 + [vbptr + 4] (0x0049bbc0-0x0049bbcc).
const ObjSpec kObj64 = {0x64, 0x004bbcd4, 0x5c, 0x60, 0x004bbc38, 0x58, true};

typedef void*(__fastcall* VoiceCtor_t)(void*, void*);
const VoiceCtor_t kVoiceCtor = reinterpret_cast<VoiceCtor_t>(0x004852e0);

void* Construct(void* p, const ObjSpec& s) {
    At<unsigned>(p, s.vbOff) = s.vptr;
    At<unsigned>(p, s.baseOff) = 0x004ba278;
    At<unsigned>(p, s.savedOff) = G<unsigned>(0x00839650);
    G<unsigned>(0x00839650) = 0;
    if (s.voice) {
        kVoiceCtor(p, 0);
        At<unsigned>(p, 0) = 0x004bbc3c;
    }
    char* vb = At<char*>(p, s.vbOff);
    At<unsigned>(p, s.vbOff + static_cast<unsigned>(At<int>(vb, 4))) = s.finalVptr;
    return p;
}

// The allocation sequence in front of every construction: the list's own heap [list+0x18] when set (0x00401d19 /
// 0x00401d2b), else the heap in the virtual base at [[list+4]+4] + list + 8 (0x00401d62-0x00401d73), else malloc
// (0x00401da3). A heapAlloc NULL shows the message box and exits; a malloc NULL returns NULL.
void* NewObj(void* list, const ObjSpec& s) {
    void* heap = At<void*>(list, 0x18);
    if (heap == 0) {
        char* vt = At<char*>(list, 4);
        heap = At<void*>(list, static_cast<unsigned>(At<int>(vt, 4)) + 8);
    }
    if (heap != 0) {
        void* p = kHeapAlloc(heap, 0, static_cast<int>(s.size));
        if (p == 0) {
            NoMemory();
            return 0;
        }
        G<void*>(0x00839650) = heap;
        return Construct(p, s);
    }
    void* p = kMalloc(s.size);
    if (p == 0)
        return 0;
    G<unsigned>(0x00839650) = 0;
    return Construct(p, s);
}

// =====================================================================================================================

// 0x00474860  heapAlloc
// char* __thiscall S0Heap::alloc(int n): the block is +8, the bump pointer +0xc, the capacity +0x10, the free count
// +0x14, a flag byte +4. While the free count is below n (signed, `cmp [ebp+0x14], ebx; jge` at 0x0047486c /
// 0x00474871, re-tested by `jl` at 0x004748a9) bit 0 of the flag byte returns NULL (0x00474876 / 0x0047487a); else the
// block is grown to capacity + 0x400 through _realloc 0x004a6980 (0x0047487c-0x00474886) and, when that is not NULL,
// the block, capacity and free count take the new values (0x0047489a / 0x004748a3 / 0x004748a6; the bump pointer is
// NOT moved to the new block). A NULL from _realloc builds the "out of memory" text in 0x0083c340 and exits
// (0x004748d0-0x00474a72). Finally the bump pointer is returned and advanced by n and the free count drops by n
// (0x004748ab-0x004748ba).
typedef char*(__fastcall* HeapAllocRe_t)(void*, void*, int);
HeapAllocRe_t HeapAlloc_orig;
char* __fastcall HeapAlloc_re(void* self, void*, int n) {
    while (At<int>(self, 0x14) < n) {
        if (At<unsigned char>(self, 4) & 1)
            return 0;
        void* p = kRealloc(At<void*>(self, 8), static_cast<unsigned>(At<int>(self, 0x10) + 0x400));
        if (p == 0) {
            char* g = reinterpret_cast<char*>(0x0083c340);
            char buf[80];
            g[0] = 0;
            Cat(g, reinterpret_cast<const char*>(0x004e41ec));    // 0x004748d1-0x00474908
            kAppendNewline(g);                                    // 0x0047490a
            Cat(g, reinterpret_cast<const char*>(0x004e41e0));    // 0x0047490f-0x00474941
            kItoa(At<int>(self, 0x10), buf, 10);                  // 0x00474947
            Cat(g, buf);                                          // 0x0047494c-0x0047497b
            kAppendNewline(g);                                    // 0x0047497d
            Cat(g, reinterpret_cast<const char*>(0x004e41d4));    // 0x00474982-0x004749b3
            kItoa(At<int>(self, 0x14), buf, 10);                  // 0x004749ba
            Cat(g, buf);
            kAppendNewline(g);                                    // 0x004749f0
            Cat(g, reinterpret_cast<const char*>(0x004e41c0));    // 0x004749f5-0x00474a27
            kItoa(n, buf, 10);                                    // 0x00474a2a
            Cat(g, buf);
            (*reinterpret_cast<MessageBoxA_t*>(0x004ba1f4))(0, g, reinterpret_cast<const char*>(0x004e41b0), 0);  // 0x00474a6a
            kExit(3);                                             // 0x00474a72
            return 0;
        }
        At<void*>(self, 8) = p;
        At<int>(self, 0x10) += 0x400;
        At<int>(self, 0x14) += 0x400;
    }
    char* r = At<char*>(self, 0xc);
    At<char*>(self, 0xc) = r + n;
    At<int>(self, 0x14) -= n;
    return r;
}

// 0x0049d100  strDup
// char* __thiscall strDup(const char* s): asks heapAlloc 0x00474860 (this = the heap, ecx kept across the length scan
// at 0x0049d105 / 0x0049d114) for strlen(s) + 1 bytes (0x0049d10f-0x0049d116), copies the string with its NUL into the
// returned block without testing it (0x0049d12a-0x0049d13c) and returns the block (0x0049d137). A NULL block faults.
StrDup_t StrDup_orig;
char* __fastcall StrDup_re(void* self, void*, const char* s) {
    unsigned n = SizeWithNul(s);
    char* p = kHeapAlloc(self, 0, static_cast<int>(n));
    for (unsigned i = 0; i < n; i++)
        p[i] = s[i];
    return p;
}

// 0x0049d090  strAssign
// int __thiscall strAssign(unsigned size): a non-zero flag at +0x18 releases the buffer first through strFree
// 0x0049d0e0 (0x0049d093-0x0049d09a). Buf_alloc 0x00474820 (this, size) returning non-zero returns 1 (0x0049d0ab /
// 0x0049d0af). Otherwise the constant string at 0x004e49f4 is stored through strDup 0x0049d100 (0x0049d0b8-0x0049d0bf;
// its result is dropped), +0x18 becomes 1 (0x0049d0c4) and the result is 0.
typedef int(__fastcall* StrAssign_t)(void*, void*, unsigned);
StrAssign_t StrAssign_orig;
int __fastcall StrAssign_re(void* self, void*, unsigned size) {
    if (At<int>(self, 0x18) != 0)
        kStrFree(self, 0);
    if (kBufAlloc(self, 0, static_cast<int>(size)) != 0)
        return 1;
    kStrDup(self, 0, reinterpret_cast<const char*>(0x004e49f4));
    At<int>(self, 0x18) = 1;
    return 0;
}

// 0x004a0540  bufferCopyTo
// int __thiscall record::copyTo(record* dst): the source string is +0x1c, the heap +0x18. A string present
// (0x0049054c / 0x004a054e) gets strlen + 1 bytes from heapAlloc 0x00474860 when the heap is set (0x004a0553-0x004a0563)
// else from malloc 0x004a504f (0x004a056a-0x004a0574); the block is stored in dst+4 even when NULL (0x004a057e) and
// NULL returns 4 (0x004a0585). Otherwise the block gets a NUL and a copy of the string (0x004a058e-0x004a05b9).
// dst+8 and dst+0xc take +0x20 and +0x24 on every path that gets there (0x004a05bc-0x004a05c6), result 0.
typedef int(__fastcall* BufferCopyTo_t)(void*, void*, void*);
BufferCopyTo_t BufferCopyTo_orig;
int __fastcall BufferCopyTo_re(void* self, void*, void* dst) {
    const char* src = At<const char*>(self, 0x1c);
    if (src != 0) {
        void* heap = At<void*>(self, 0x18);
        unsigned n = SizeWithNul(src);
        char* p = static_cast<char*>(AllocFrom(heap, n));
        At<char*>(dst, 4) = p;
        if (p == 0)
            return 4;
        *p = 0;
        AppendCopy(p, src);
    }
    At<int>(dst, 8) = At<int>(self, 0x20);
    At<int>(dst, 0xc) = At<int>(self, 0x24);
    return 0;
}

// 0x00474650  Surface_get
// int __thiscall record::copyTo(record* dst): dst+4 takes +0x1c (0x00474659 / 0x0047465c). A NULL string at +0x20
// (0x0047465f / 0x00474662) stores NULL in dst+8 and +0x24 in dst+0xc and returns 0 (0x00474666-0x00474673). Otherwise
// strlen + 1 bytes come from heapAlloc 0x00474860 when +0x18 is set (0x00474677-0x0047468a) else from malloc
// 0x004a504f (0x00474691-0x0047469b); the block is stored in dst+8 even when NULL (0x004746a5), NULL returns 4
// (0x004746ac), else it gets a NUL and a copy of the string (0x004746b5-0x004746e0), dst+0xc takes +0x24 (0x004746e2 /
// 0x004746e7) and the result is 0.
typedef int(__fastcall* SurfaceGet_t)(void*, void*, void*);
SurfaceGet_t SurfaceGet_orig;
int __fastcall SurfaceGet_re(void* self, void*, void* dst) {
    At<int>(dst, 4) = At<int>(self, 0x1c);
    const char* src = At<const char*>(self, 0x20);
    if (src == 0) {
        At<const char*>(dst, 8) = 0;
        At<int>(dst, 0xc) = At<int>(self, 0x24);
        return 0;
    }
    void* heap = At<void*>(self, 0x18);
    unsigned n = SizeWithNul(src);
    char* p = static_cast<char*>(AllocFrom(heap, n));
    At<char*>(dst, 8) = p;
    if (p == 0)
        return 4;
    *p = 0;
    AppendCopy(p, src);
    At<int>(dst, 0xc) = At<int>(self, 0x24);
    return 0;
}

// 0x00401d10  S3List_add / 0x00402280  S3List_add2
// int __thiscall S3List::add(int item): a circular doubly linked list of nodes {item +4, payload +8, next +0xc, prev
// +0x10}; the list holds {vbptr +4, head +8, cursor +0xc, last +0x14, count +0x10, heap +0x18}. Every node and payload
// is a virtual-base object built by NewObj above (heapAlloc, or malloc without a heap; an allocation that fails with
// a heap shows the message box and exits). The first item (count 0, `jne 0x00401f1a` at 0x00401d25) makes the head
// node, stores the item (0x00401df1-0x00401df5) and links it to itself (0x00401dfb / 0x00401e01), allocates the payload
// object into head+8 (0x00401e42-0x00401e45 or 0x00401edc-0x00401edf) and returns 4 when either is NULL (0x00401de2,
// 0x00401ee8). Later items link a new node behind the tail (0x00401fcc-0x00402017, 4 when the node is NULL: 0x00401fde)
// and give it a payload (0x0040205b / 0x004020f5; 4 when NULL: 0x004020fe). Success calls list-vtable slot 0 with the
// payload (0x00401efe / 0x00402114), moves the cursor and, with the count re-read after that call, sets last = count
// and count = count + 1 (0x00401f00-0x00401f0e / 0x00402116-0x0040211d); the result is 0. S3List_add2 differs only in
// its payload object (0x10 bytes, vptr 0x004ba2b8: 0x0040237b-0x004023ab).
int S3ListAdd(void* self, int item, const ObjSpec& second) {
    int count = At<int>(self, 0x10);
    if (count == 0) {
        void* n = NewObj(self, kNode);
        At<void*>(self, 8) = n;
        if (n == 0)
            return 4;
        At<int>(n, 4) = item;
        At<void*>(At<void*>(self, 8), 0xc) = At<void*>(self, 8);
        At<void*>(At<void*>(self, 8), 0x10) = At<void*>(self, 8);
        void* p = NewObj(self, second);
        At<void*>(At<void*>(self, 8), 8) = p;
        void* payload = At<void*>(At<void*>(self, 8), 8);
        if (payload == 0)
            return 4;
        Slot<This1_t>(self, 0)(self, 0, reinterpret_cast<int>(payload));
        At<void*>(self, 0xc) = At<void*>(self, 8);
        int c = At<int>(self, 0x10);
        At<int>(self, 0x14) = c;
        At<int>(self, 0x10) = c + 1;
        return 0;
    }
    void* n = NewObj(self, kNode);
    void* head = At<void*>(self, 8);
    char* tail = At<char*>(head, 0x10);
    At<void*>(tail, 0xc) = n;
    tail = At<char*>(At<void*>(self, 8), 0x10);
    void* nn = At<void*>(tail, 0xc);
    if (nn == 0)
        return 4;
    At<void*>(nn, 0x10) = tail;
    head = At<void*>(self, 8);
    tail = At<char*>(head, 0x10);
    nn = At<void*>(tail, 0xc);
    At<void*>(nn, 0xc) = head;
    head = At<void*>(self, 8);
    tail = At<char*>(head, 0x10);
    nn = At<void*>(tail, 0xc);
    At<void*>(head, 0x10) = nn;
    void* last = At<void*>(At<void*>(self, 8), 0x10);
    At<void*>(self, 0xc) = last;
    At<int>(last, 4) = item;
    void* p = NewObj(self, second);
    At<void*>(At<void*>(self, 0xc), 8) = p;
    void* payload = At<void*>(At<void*>(self, 0xc), 8);
    if (payload == 0)
        return 4;
    Slot<This1_t>(self, 0)(self, 0, reinterpret_cast<int>(payload));
    int c = At<int>(self, 0x10);
    At<int>(self, 0x14) = c;
    At<int>(self, 0x10) = c + 1;
    return 0;
}

ThisRet1_t S3ListAdd_orig;
int __fastcall S3ListAdd_re(void* self, void*, int item) {
    return S3ListAdd(self, item, kObj18);
}

ThisRet1_t S3ListAdd2_orig;
int __fastcall S3ListAdd2_re(void* self, void*, int item) {
    return S3ListAdd(self, item, kObj10);
}

// 0x0047abe0  View_attach
// void __thiscall View::attach(View* v): returns at once when this+0x9c has 0x200000 (0x0047abea), this+0xa0 has 8
// (0x0047abf5), or v is NULL while this+0x9c has 0x1000 (0x0047ac06-0x0047ac0d), or v+0x9c has 0x1000 (`not eax; test
// ah,0x10; je`: 0x0047ac13-0x0047ac1e). Otherwise v keeps a circular list {head +0x138, cursor +0x13c, count +0x140,
// last +0x144, heap +0x148}: a node of 0x1c bytes from heapAlloc when the heap is set else malloc (0x0047ac38-0x0047ac4a /
// 0x0047acb9-0x0047accb) holds this at +4 and links into the ring (0x0047ac5b-0x0047ac6f, 0x0047acd4-0x0047ad20) and gets a
// 4-byte payload from the same allocator into +8 (0x0047ac64-0x0047ac9a / 0x0047acf7-0x0047ad4b). A NULL node or
// payload skips the count update (0x0047ac55, 0x0047aca8, 0x0047ace8, 0x0047ad59 all jump to 0x0047ad6e); otherwise
// last = count and count = count + 1 (0x0047ad5b-0x0047ad68). Then, when count - 1 >= 0 (0x0047ad6e-0x0047ad75), last is
// cleared and the cursor goes back to the head (0x0047ad77-0x0047ad87). Finally this+0x130 = v (0x0047ad8d).
typedef void(__fastcall* ViewAttach_t)(void*, void*, void*);
ViewAttach_t ViewAttach_orig;
void __fastcall ViewAttach_re(void* self, void*, void* v) {
    if (At<unsigned>(self, 0x9c) & 0x200000)
        return;
    if (At<unsigned char>(self, 0xa0) & 8)
        return;
    if ((At<unsigned>(self, 0x9c) & 0x1000) && v == 0)
        return;
    if (At<unsigned>(v, 0x9c) & 0x1000)
        return;
    int count = At<int>(v, 0x140);
    void* heap = At<void*>(v, 0x148);
    bool linked = false;
    if (count == 0) {
        void* node = AllocFrom(heap, 0x1c);
        At<void*>(v, 0x138) = node;
        if (node != 0) {
            At<void*>(node, 4) = self;
            At<void*>(At<void*>(v, 0x138), 0xc) = At<void*>(v, 0x138);
            At<void*>(At<void*>(v, 0x138), 0x10) = At<void*>(v, 0x138);
            heap = At<void*>(v, 0x148);
            void* payload = AllocFrom(heap, 4);
            At<void*>(At<void*>(v, 0x138), 8) = payload;
            if (At<void*>(At<void*>(v, 0x138), 8) != 0) {
                At<void*>(v, 0x13c) = At<void*>(v, 0x138);
                linked = true;
            }
        }
    } else {
        void* node = AllocFrom(heap, 0x1c);
        char* head = At<char*>(v, 0x138);
        char* tail = At<char*>(head, 0x10);
        At<void*>(tail, 0xc) = node;
        tail = At<char*>(At<char*>(v, 0x138), 0x10);
        char* nn = At<char*>(tail, 0xc);
        if (nn != 0) {
            At<void*>(nn, 0x10) = tail;
            head = At<char*>(v, 0x138);
            tail = At<char*>(head, 0x10);
            nn = At<char*>(tail, 0xc);
            At<void*>(nn, 0xc) = head;
            head = At<char*>(v, 0x138);
            tail = At<char*>(head, 0x10);
            nn = At<char*>(tail, 0xc);
            At<void*>(head, 0x10) = nn;
            char* last = At<char*>(At<char*>(v, 0x138), 0x10);
            At<void*>(v, 0x13c) = last;
            At<void*>(last, 4) = self;
            heap = At<void*>(v, 0x148);
            void* payload = AllocFrom(heap, 4);
            At<void*>(At<void*>(v, 0x13c), 8) = payload;
            if (At<void*>(At<void*>(v, 0x13c), 8) != 0)
                linked = true;
        }
    }
    if (linked) {
        int c = At<int>(v, 0x140);
        At<int>(v, 0x144) = c;
        At<int>(v, 0x140) = c + 1;
    }
    if (At<int>(v, 0x140) - 1 >= 0) {
        void* head = At<void*>(v, 0x138);
        At<int>(v, 0x144) = 0;
        At<void*>(v, 0x13c) = head;
    }
    At<void*>(self, 0x130) = v;
}

// 0x0048dfc0  TextView::addImage
// int __thiscall TextView::addImage(int image): NULL returns 3 (0x0048dfc7-0x0048dfcd). The list embedded at +0x13e0
// takes the image at +0x1c and zero in +0x20 / +0x24 (0x0048dfd6-0x0048dfe3) and S3List_add 0x00401d10 is called on it
// with item 0 (0x0048dfdc / 0x0048dfe6); a non-zero result returns 1 (0x0048dfeb-0x0048dfef), else the counter at +0x137c
// is incremented (0x0048dff8-0x0048e005) and the result is 0.
typedef int(__fastcall* AddImage_t)(void*, void*, int);
AddImage_t AddImage_orig;
int __fastcall AddImage_re(void* self, void*, int image) {
    if (image == 0)
        return 3;
    char* list = Ptr(self, 0x13e0);
    At<int>(list, 0x1c) = image;
    At<int>(list, 0x20) = 0;
    At<int>(list, 0x24) = 0;
    if (kS3ListAdd(list, 0, 0) != 0)
        return 1;
    At<int>(self, 0x137c) += 1;
    return 0;
}

// 0x004a0600  postMessageLine
// int __thiscall postMessageLine(int a, int item, int c, int d): +0x1c = a, +0x20 = c, +0x24 = d (0x004a0611 / 0x004a0618 /
// 0x004a061e), then S3List_add 0x00401d10 appends `item` (0x004a061b / 0x004a0621); a non-zero result returns at once
// (0x004a062e). With count = +0x10, the loop 0x004a0648-0x004a0726 runs for k = count - 1 down to 1 (0x004a0634-0x004a0640,
// 0x004a071e / 0x004a0726): node A is the k-th node from the head (the cursor +0xc is reset to the head, 0x004a065d, and
// walked k times through next +0xc, 0x004a0688-0x004a0694), node B is the node at index (k + 1) mod count (idiv at
// 0x004a069e, walked the same way: 0x004a06d7-0x004a06e3), the index +0x14 is set to the walked counts (0x004a069a,
// 0x004a06e8), and the items at +4 are exchanged by three xors (0x004a06eb-0x004a070a) and the payloads at +8 by a
// plain swap (0x004a0707-0x004a0713). The cursor and index end on the last B. The three branches that walk backwards
// (negative k, 0x004a0662-0x004a0684; negative remainder, 0x004a06b6-0x004a06d3) and the two `jg` bail-outs
// (0x004a0652, 0x004a06aa) are dead: k runs from count - 1 down to 1, so k >= 1 and k <= count - 1, and (k + 1) mod
// count of two positive numbers is not negative and not above count - 1. The result is 0.
typedef int(__fastcall* PostMessageLine_t)(void*, void*, int, int, int, int);
PostMessageLine_t PostMessageLine_orig;
int __fastcall PostMessageLine_re(void* self, void*, int a, int item, int c, int d) {
    At<int>(self, 0x1c) = a;
    At<int>(self, 0x20) = c;
    At<int>(self, 0x24) = d;
    int r = kS3ListAdd(self, 0, item);
    if (r != 0)
        return r;
    int k = At<int>(self, 0x10) - 1;
    if (k <= 0)
        return 0;
    int x = k + 1;
    for (;;) {
        int count = At<int>(self, 0x10);
        At<void*>(self, 0xc) = At<void*>(self, 8);
        for (int i = k; i > 0; i--)
            At<void*>(self, 0xc) = At<void*>(At<void*>(self, 0xc), 0xc);
        At<int>(self, 0x14) = k;
        int rem = x % count;
        char* a_node = At<char*>(self, 0xc);
        At<void*>(self, 0xc) = At<void*>(self, 8);
        for (int i = rem; i > 0; i--)
            At<void*>(self, 0xc) = At<void*>(At<void*>(self, 0xc), 0xc);
        char* b_node = At<char*>(self, 0xc);
        At<int>(self, 0x14) = rem;
        At<unsigned>(a_node, 4) ^= At<unsigned>(b_node, 4);
        At<unsigned>(b_node, 4) ^= At<unsigned>(a_node, 4);
        At<unsigned>(a_node, 4) ^= At<unsigned>(b_node, 4);
        unsigned t = At<unsigned>(a_node, 8);
        At<unsigned>(a_node, 8) = At<unsigned>(b_node, 8);
        At<unsigned>(b_node, 8) = t;
        k--;
        x--;
        if (k <= 0)
            break;
    }
    return 0;
}

// 0x004a28a0  listAddEntry
// int __thiscall listAddEntry(int a, const char* s, int b): the object's virtual base sits at the offset in
// [[this] + 8] (0x004a28a6-0x004a28a9) and its slot index is the dword at base + 0xd0 (0x004a28ac); 10 returns 1
// (0x004a28b3-0x004a28bb). A string present (0x004a28c8) replaces the owned copy at this + index*4 + 0x2c: an old one is
// freed through free 0x004a5007 (0x004a28cc-0x004a28d5), strlen + 1 bytes come from malloc 0x004a504f (0x004a28e4-0x004a28e9)
// and are stored even when NULL (0x004a28f1); NULL returns 4 (0x004a28fc), else the block gets a NUL and a copy
// (0x004a2905-0x004a292f). The base + 0xdc / 0xe0 take a and 0 (0x004a293e / 0x004a2945) and the list at base + 0xc0
// (its +0x24 cleared, 0x004a2953) gets the slot index as an item from S3List_add 0x00401d10 (0x004a2956); a non-zero
// result returns 4 (0x004a2962). Otherwise this + index*4 + 0x54 takes b (0x004a296f / 0x004a2970), result 0.
typedef int(__fastcall* ListAddEntry_t)(void*, void*, int, const char*, int);
ListAddEntry_t ListAddEntry_orig;
int __fastcall ListAddEntry_re(void* self, void*, int a, const char* s, int b) {
    int off = At<int>(At<void*>(self, 0), 8);
    int idx = At<int>(self, static_cast<unsigned>(off) + 0xd0);
    if (idx == 10)
        return 1;
    if (s != 0) {
        char*& slot = At<char*>(self, static_cast<unsigned>(idx) * 4 + 0x2c);
        if (slot != 0)
            kFree(slot);
        slot = static_cast<char*>(kMalloc(SizeWithNul(s)));
        if (slot == 0)
            return 4;
        *slot = 0;
        AppendCopy(slot, s);
    }
    off = At<int>(At<void*>(self, 0), 8);
    At<int>(self, static_cast<unsigned>(off) + 0xdc) = a;
    At<int>(self, static_cast<unsigned>(off) + 0xe0) = 0;
    char* list = Ptr(self, off + 0xc0);
    At<int>(list, 0x24) = 0;
    if (kS3ListAdd(list, 0, idx) != 0)
        return 4;
    At<int>(self, static_cast<unsigned>(idx) * 4 + 0x54) = b;
    return 0;
}


// =====================================================================================================================
// Part 2: destructors and constructors over the heap (the rows c3cz listed as doable, not attempted)
// =====================================================================================================================

typedef void(__cdecl* OperatorDelete_t)(void*);
const OperatorDelete_t kOperatorDelete = reinterpret_cast<OperatorDelete_t>(0x004a4ffc);   // operator delete (C3, c3cz)
const This0_t kCacheDtor = reinterpret_cast<This0_t>(0x00483010);                          // Cache483010::dtor (C3, c3cz)
const This0_t kSurfaceFree = reinterpret_cast<This0_t>(0x00473ae0);                        // Surface_free (C3, c3cz)
const This0_t kFlicReset = reinterpret_cast<This0_t>(0x00481ba0);                          // Flic::reset (THIS batch)
const This0_t kStreamDtor = reinterpret_cast<This0_t>(0x00482dd0);                         // Stream482dd0::dtor (THIS batch)
typedef int(__fastcall* ThisRet0_t)(void*, void*);
const ThisRet0_t kSurfaceCtor = reinterpret_cast<ThisRet0_t>(0x00473ab0);                  // Surface_ctor (C3, c3cz)
typedef void(__stdcall* VecCtorIter_t)(void*, unsigned, int, void*, void*);
const VecCtorIter_t kVecCtorIter = reinterpret_cast<VecCtorIter_t>(0x004a55d2);            // eh vector constructor iterator

// 0x00482dd0  Stream482dd0::dtor
// void __thiscall Stream::~Stream(): +4, +0x38 and +0x3c are cleared and +0x40 becomes -1 (0x00482dd9 / 0x00482dde /
// 0x00482de1 / 0x00482de4). A stream object at +0x34 (0x00482dd6 / 0x00482deb test) is destroyed through its vtable slot
// 0 with the argument 3 (0x00482ded-0x00482df1) and the field cleared (0x00482df3). Surface_free 0x00473ae0 then runs on
// the Surface at +8 (0x00482df6 / 0x00482df9).
This0_t StreamDtor_orig;
void __fastcall StreamDtor_re(void* self, void*) {
    void* stream = At<void*>(self, 0x34);
    At<int>(self, 4) = 0;
    At<int>(self, 0x38) = 0;
    At<int>(self, 0x3c) = 0;
    At<int>(self, 0x40) = -1;
    if (stream != 0) {
        Slot<This1_t>(stream, 0)(stream, 0, 3);
        At<void*>(self, 0x34) = 0;
    }
    kSurfaceFree(Ptr(self, 8), 0);
}

// 0x00481ba0  Flic::reset
// void __thiscall Flic::reset(): +0x74 is cleared (0x00481ba9). An object at +0x54 is destroyed through its vtable slot 0
// with the argument 1 (0x00481bb0-0x00481bb4) and +0x54 and +0x44 are cleared (0x00481bb6 / 0x00481bb9; the block at +0x44
// is NOT freed on this path). Objects at +0x6c and +0x70 go through vtable slot 3 (+0xc) with the argument 1
// (0x00481bc7 / 0x00481bd8) and are cleared. The caches at +0x64 and +0x68 run Cache483010::dtor 0x00483010 and operator
// delete 0x004a4ffc and are cleared (0x00481be8-0x00481bf6, 0x00481c02-0x00481c10). Objects at +0x5c and +0x60 go
// through vtable slot 0 with the argument 1 (0x00481c1e / 0x00481c2e) and are cleared. The cache at +0x78 is released
// like +0x64 (0x00481c3c-0x00481c4a). The blocks at +0x44, +0x48 and +0x4c (read AFTER the clears above) are deleted
// with operator delete and cleared (0x00481c4d-0x00481c84); +0x50, +0x58 and +0x7c are cleared (0x00481c87-0x00481c8d).
This0_t FlicReset_orig;
void __fastcall FlicReset_re(void* self, void*) {
    At<int>(self, 0x74) = 0;
    void* c = At<void*>(self, 0x54);
    if (c != 0) {
        Slot<This1_t>(c, 0)(c, 0, 1);
        At<int>(self, 0x54) = 0;
        At<int>(self, 0x44) = 0;
    }
    c = At<void*>(self, 0x6c);
    if (c != 0) {
        Slot<This1_t>(c, 0xc)(c, 0, 1);
        At<int>(self, 0x6c) = 0;
    }
    c = At<void*>(self, 0x70);
    if (c != 0) {
        Slot<This1_t>(c, 0xc)(c, 0, 1);
        At<int>(self, 0x70) = 0;
    }
    c = At<void*>(self, 0x64);
    if (c != 0) {
        kCacheDtor(c, 0);
        kOperatorDelete(c);
        At<int>(self, 0x64) = 0;
    }
    c = At<void*>(self, 0x68);
    if (c != 0) {
        kCacheDtor(c, 0);
        kOperatorDelete(c);
        At<int>(self, 0x68) = 0;
    }
    c = At<void*>(self, 0x5c);
    if (c != 0) {
        Slot<This1_t>(c, 0)(c, 0, 1);
        At<int>(self, 0x5c) = 0;
    }
    c = At<void*>(self, 0x60);
    if (c != 0) {
        Slot<This1_t>(c, 0)(c, 0, 1);
        At<int>(self, 0x60) = 0;
    }
    c = At<void*>(self, 0x78);
    if (c != 0) {
        kCacheDtor(c, 0);
        kOperatorDelete(c);
        At<int>(self, 0x78) = 0;
    }
    c = At<void*>(self, 0x44);
    if (c != 0) {
        kOperatorDelete(c);
        At<int>(self, 0x44) = 0;
    }
    c = At<void*>(self, 0x48);
    if (c != 0) {
        kOperatorDelete(c);
        At<int>(self, 0x48) = 0;
    }
    c = At<void*>(self, 0x4c);
    if (c != 0) {
        kOperatorDelete(c);
    }
    At<int>(self, 0x4c) = 0;
    At<int>(self, 0x50) = 0;
    At<int>(self, 0x58) = 0;
    At<int>(self, 0x7c) = 0;
}

// 0x00404280  R2C404280_dtor
// void __thiscall R2C::~R2C(): the vtable becomes 0x004ba2e0 (0x0040429d) and Flic::reset 0x00481ba0 runs (0x004042ab);
// the vtable becomes 0x004ba2dc and Stream482dd0::dtor 0x00482dd0 runs (0x004042b0-0x004042c0); the Surface sub-object
// at +8 gets its vtable 0x004ba2d8 (0x004042d0) and Surface_free 0x00473ae0 runs on it (0x004042d6). The exception
// registration the original builds around these calls (fs:[0], 0x00404282-0x0040428e) only matters when a callee throws.
This0_t R2CDtor_orig;
void __fastcall R2CDtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004ba2e0;
    kFlicReset(self, 0);
    At<unsigned>(self, 0) = 0x004ba2dc;
    kStreamDtor(self, 0);
    At<unsigned>(self, 8) = 0x004ba2d8;
    kSurfaceFree(Ptr(self, 8), 0);
}

// 0x004042f0  scalarDtor_404280
// R2C* __thiscall R2C::scalar_deleting_destructor(unsigned char flags): R2C404280_dtor 0x00404280 runs on this
// (0x004042f3); bit 0 of the byte argument (`test byte ptr [esp+8], 1`, 0x004042f8) passes this to operator delete
// 0x004a4ffc (0x004042ff-0x00404300); the result is this (0x00404308).
typedef void*(__fastcall* ScalarDtor_t)(void*, void*, int);
ScalarDtor_t ScalarDtor_orig;
void* __fastcall ScalarDtor_re(void* self, void*, int flags) {
    reinterpret_cast<This0_t>(0x00404280)(self, 0);
    if (flags & 1)
        kOperatorDelete(self);
    return self;
}

// 0x00474cb0  Image_releaseAll
// void __thiscall Image::releaseAll(): a handle at +4 is released through the display's vtable slot +0x8c
// (`mov ecx,[0x83ad50]; mov edx,[ecx]; call [edx+0x8c]`, 0x00474cbf-0x00474cc8). With the global 0x00839ab0 clear
// (0x00474cce), a cache at +0xac runs Cache483010::dtor 0x00483010, operator delete 0x004a4ffc and is cleared
// (0x00474cd6-0x00474cf0). The forty pointers from +0x1b4 are freed with free 0x004a5007 when set and cleared, and the
// forty dwords from +0x114 become -2 (0x00474cf6-0x00474d20). The remaining fields are reset (0x00474d22-0x00474dc6):
// +0x10, +0xc, +0x1c, +0x18, +0x14, +0x24, +0x30, +0x34, +0x38, +0x3c, +0x48, +0x4c, +0x50, +0x6c, +0x60, +0x64, +0x68,
// +0x20 and the byte +0x28 to 0; +0x2c, +0x44, +0x7c, +0x70, +0x80, +0x74, +0x84, +0x78, +0x88 to -1; +0x8c, +0x9c, +0x90,
// +0xa0, +0x94, +0xa4, +0x98, +0xa8 to 2; +0x40 from the dword at 0x004e4210 and +0x5c from the global 0x0083ad44.
This0_t ImageReleaseAll_orig;
void __fastcall ImageReleaseAll_re(void* self, void*) {
    int handle = At<int>(self, 4);
    if (handle != 0) {
        void* disp = G<void*>(0x0083ad50);
        Slot<This1_t>(disp, 0x8c)(disp, 0, handle);
    }
    if (G<int>(0x00839ab0) == 0) {
        void* cache = At<void*>(self, 0xac);
        if (cache != 0) {
            kCacheDtor(cache, 0);
            kOperatorDelete(cache);
            At<int>(self, 0xac) = 0;
        }
    }
    for (unsigned k = 0; k < 0x28; k++) {
        void* p = At<void*>(self, 0x1b4 + 4 * k);
        if (p != 0) {
            kFree(p);
            At<void*>(self, 0x1b4 + 4 * k) = 0;
        }
        At<int>(self, 0x114 + 4 * k) = -2;
    }
    const int zero[] = {0x10, 0xc, 0x1c, 0x18, 0x14, 0x24, 0x30, 0x34, 0x38, 0x3c, 0x48, 0x4c, 0x50, 0x6c, 0x60, 0x64, 0x68, 0x20};
    for (unsigned i = 0; i < sizeof zero / sizeof zero[0]; i++)
        At<int>(self, zero[i]) = 0;
    const int minus1[] = {0x2c, 0x44, 0x7c, 0x70, 0x80, 0x74, 0x84, 0x78, 0x88};
    for (unsigned i = 0; i < sizeof minus1 / sizeof minus1[0]; i++)
        At<int>(self, minus1[i]) = -1;
    const int two[] = {0x8c, 0x9c, 0x90, 0xa0, 0x94, 0xa4, 0x98, 0xa8};
    for (unsigned i = 0; i < sizeof two / sizeof two[0]; i++)
        At<int>(self, two[i]) = 2;
    At<int>(self, 0x40) = G<int>(0x004e4210);
    At<int>(self, 0x5c) = G<int>(0x0083ad44);
    At<unsigned char>(self, 0x28) = 0;
}

// ---- the static Surface-array constructors ---------------------------------------------------------------------------
// Each constructs the Surfaces of a global object in place: Surface_ctor 0x00473ab0 on single members at the listed
// offsets and the eh vector constructor iterator 0x004a55d2 (first, element size 0x2c, count, ctor 0x00473ab0, dtor
// 0x004041f0) on runs. The result is this. The exception registration each builds (fs:[0]) is not reproduced.
void CtorSingles(void* self, const unsigned* offs, unsigned n) {
    for (unsigned i = 0; i < n; i++)
        kSurfaceCtor(Ptr(self, offs[i]), 0);
}
void CtorRun(void* self, unsigned off, int count) {
    kVecCtorIter(Ptr(self, off), 0x2c, count, reinterpret_cast<void*>(0x00473ab0), reinterpret_cast<void*>(0x004041f0));
}

// 0x0044ae60  gSurfaceCtor_44ae60: Surfaces at +0 and +0x2c (0x0044ae7d / 0x0044ae8d).
ThisRet0_t GSurface44ae60_orig;
int __fastcall GSurface44ae60_re(void* self, void*) {
    static const unsigned offs[] = {0, 0x2c};
    CtorSingles(self, offs, 2);
    return reinterpret_cast<int>(self);
}
// 0x0044b060  gSurfaceCtor_44b060: Surfaces at +0, +0x2c, +0x58, +0x84, +0xb0, +0xdc (0x0044b07d-0x0044b0ba).
ThisRet0_t GSurface44b060_orig;
int __fastcall GSurface44b060_re(void* self, void*) {
    static const unsigned offs[] = {0, 0x2c, 0x58, 0x84, 0xb0, 0xdc};
    CtorSingles(self, offs, 6);
    return reinterpret_cast<int>(self);
}
// 0x0044b2c0  gSurfaceCtor_44b2c0: Surfaces at +0 .. +0x134 step 0x2c (eight, 0x0044b2dd-0x0044b34a), a run of 8 at +0x160
// (0x0044b35b-0x0044b369) and a run of 0x24 at +0x2c0 (0x0044b37a-0x0044b388).
ThisRet0_t GSurface44b2c0_orig;
int __fastcall GSurface44b2c0_re(void* self, void*) {
    static const unsigned offs[] = {0, 0x2c, 0x58, 0x84, 0xb0, 0xdc, 0x108, 0x134};
    CtorSingles(self, offs, 8);
    CtorRun(self, 0x160, 8);
    CtorRun(self, 0x2c0, 0x24);
    return reinterpret_cast<int>(self);
}
// 0x0044b450  gSurfaceCtor_44b450: Surfaces at +0 and +0x2c, a run of 0x12 at +0x58 (0x0044b46d-0x0044b499).
ThisRet0_t GSurface44b450_orig;
int __fastcall GSurface44b450_re(void* self, void*) {
    static const unsigned offs[] = {0, 0x2c};
    CtorSingles(self, offs, 2);
    CtorRun(self, 0x58, 0x12);
    return reinterpret_cast<int>(self);
}
// 0x0044b5a0  gSurfaceCtor_44b5a0: a Surface at +0, runs of 4 at +0x2c and 0xc at +0xdc, a Surface at +0x2ec, a run of
// 0x15 at +0x318 (0x0044b5bd-0x0044b62a).
ThisRet0_t GSurface44b5a0_orig;
int __fastcall GSurface44b5a0_re(void* self, void*) {
    kSurfaceCtor(self, 0);
    CtorRun(self, 0x2c, 4);
    CtorRun(self, 0xdc, 0xc);
    kSurfaceCtor(Ptr(self, 0x2ec), 0);
    CtorRun(self, 0x318, 0x15);
    return reinterpret_cast<int>(self);
}
// 0x0044b6f0  gSurfaceCtor_44b6f0: a Surface at +0 and a run of 4 at +0x2c (0x0044b70d-0x0044b72c).
ThisRet0_t GSurface44b6f0_orig;
int __fastcall GSurface44b6f0_re(void* self, void*) {
    kSurfaceCtor(self, 0);
    CtorRun(self, 0x2c, 4);
    return reinterpret_cast<int>(self);
}
// 0x0044b8d0  gSurfaceCtor_44b8d0: twelve Surfaces from +0 step 0x2c (0x0044b8ed-0x0044b99a).
ThisRet0_t GSurface44b8d0_orig;
int __fastcall GSurface44b8d0_re(void* self, void*) {
    for (unsigned k = 0; k < 12; k++)
        kSurfaceCtor(Ptr(self, 0x2c * k), 0);
    return reinterpret_cast<int>(self);
}
// 0x00474a90  gSurfaceCtor_839a68: `mov ecx, 0x839a68; jmp 0x473ab0` (0x00474a90 / 0x00474a95): Surface_ctor on the global
// Surface at 0x00839a68.
typedef int(__cdecl* Cdecl0_t)();
Cdecl0_t GSurface839a68_orig;
int __cdecl GSurface839a68_re() {
    return kSurfaceCtor(reinterpret_cast<void*>(0x00839a68), 0);
}


// 0x004852e0  VoiceRx::ctor
// VoiceRx* __thiscall VoiceRx::VoiceRx(): the base constructor 0x00484150 runs on this (0x004852fa-0x00485301), the vtable
// becomes 0x004badf8 (0x00485306), the nine bytes at 0x004e43c8 / 0x004e43cc / 0x004e43d0 (two dwords and a byte: an
// 8-character name and its NUL) are copied to the stack (0x0048530c-0x00485331), 0x00484260 gets this and 6 (0x00485321 /
// 0x00485335), operator new 0x004a57f1 returns a 10-byte block (0x0048533a / 0x0048533c) stored at +0x50 without a null
// test (0x0048534f) and the name is copied into it with its NUL (0x00485352-0x0048536a). The result is this (0x00485365).
typedef void*(__fastcall* VoiceRxCtor_t)(void*, void*);
VoiceRxCtor_t VoiceRxCtor_orig;
void* __fastcall VoiceRxCtor_re(void* self, void*) {
    reinterpret_cast<This0_t>(0x00484150)(self, 0);
    At<unsigned>(self, 0) = 0x004badf8;
    char name[9];
    *reinterpret_cast<unsigned*>(name) = G<unsigned>(0x004e43c8);
    *reinterpret_cast<unsigned*>(name + 4) = G<unsigned>(0x004e43cc);
    name[8] = G<char>(0x004e43d0);
    reinterpret_cast<This1_t>(0x00484260)(self, 0, 6);
    char* p = static_cast<char*>(reinterpret_cast<Malloc_t>(0x004a57f1)(10));
    At<char*>(self, 0x50) = p;
    unsigned n = SizeWithNul(name);
    for (unsigned i = 0; i < n; i++)
        p[i] = name[i];
    return self;
}

// 0x0049b970  netHeapAlloc
// int __thiscall NetList::add(int item): S3List_add's body (the same ring, the same allocation sequence, the same
// virtual call through slot 0 and the same count / last update, 0x0049b990-0x0049be9b) with a different payload: a
// 0x64-byte object (kObj64 above) built by the allocation sequence of 0x0049bab4-0x0049bb7c / 0x0049bd3e-0x0049be14. The
// frame also registers an exception handler (fs:[0], 0x0049b972-0x0049b97e) that only matters when a callee throws.
ThisRet1_t NetHeapAlloc_orig;
int __fastcall NetHeapAlloc_re(void* self, void*, int item) {
    return S3ListAdd(self, item, kObj64);
}

}  // namespace

// 0x00474860  heapAlloc
SG_HOOK("golf_clean.exe", 0x00474860, heapAlloc, HeapAlloc_re, HeapAlloc_orig);
// 0x0049d100  strDup
SG_HOOK("golf_clean.exe", 0x0049d100, strDup, StrDup_re, StrDup_orig);
// 0x0049d090  strAssign
SG_HOOK("golf_clean.exe", 0x0049d090, strAssign, StrAssign_re, StrAssign_orig);
// 0x004a0540  bufferCopyTo
SG_HOOK("golf_clean.exe", 0x004a0540, bufferCopyTo, BufferCopyTo_re, BufferCopyTo_orig);
// 0x00474650  Surface_get
SG_HOOK("golf_clean.exe", 0x00474650, Surface_get, SurfaceGet_re, SurfaceGet_orig);
// 0x00401d10  S3List_add
SG_HOOK("golf_clean.exe", 0x00401d10, S3List_add, S3ListAdd_re, S3ListAdd_orig);
// 0x00402280  S3List_add2
SG_HOOK("golf_clean.exe", 0x00402280, S3List_add2, S3ListAdd2_re, S3ListAdd2_orig);
// 0x0047abe0  View_attach
SG_HOOK("golf_clean.exe", 0x0047abe0, View_attach, ViewAttach_re, ViewAttach_orig);
// 0x0048dfc0  TextView::addImage
SG_HOOK("golf_clean.exe", 0x0048dfc0, TextView_addImage, AddImage_re, AddImage_orig);
// 0x004a0600  postMessageLine
SG_HOOK("golf_clean.exe", 0x004a0600, postMessageLine, PostMessageLine_re, PostMessageLine_orig);
// 0x004a28a0  listAddEntry
SG_HOOK("golf_clean.exe", 0x004a28a0, listAddEntry, ListAddEntry_re, ListAddEntry_orig);
// 0x00482dd0  Stream482dd0::dtor
SG_HOOK("golf_clean.exe", 0x00482dd0, Stream482dd0_dtor, StreamDtor_re, StreamDtor_orig);
// 0x00481ba0  Flic::reset
SG_HOOK("golf_clean.exe", 0x00481ba0, Flic_reset, FlicReset_re, FlicReset_orig);
// 0x00404280  R2C404280_dtor
SG_HOOK("golf_clean.exe", 0x00404280, R2C404280_dtor, R2CDtor_re, R2CDtor_orig);
// 0x004042f0  scalarDtor_404280
SG_HOOK("golf_clean.exe", 0x004042f0, scalarDtor_404280, ScalarDtor_re, ScalarDtor_orig);
// 0x00474cb0  Image_releaseAll
SG_HOOK("golf_clean.exe", 0x00474cb0, Image_releaseAll, ImageReleaseAll_re, ImageReleaseAll_orig);
// 0x0044ae60  gSurfaceCtor_44ae60
SG_HOOK("golf_clean.exe", 0x0044ae60, gSurfaceCtor_44ae60, GSurface44ae60_re, GSurface44ae60_orig);
// 0x0044b060  gSurfaceCtor_44b060
SG_HOOK("golf_clean.exe", 0x0044b060, gSurfaceCtor_44b060, GSurface44b060_re, GSurface44b060_orig);
// 0x0044b2c0  gSurfaceCtor_44b2c0
SG_HOOK("golf_clean.exe", 0x0044b2c0, gSurfaceCtor_44b2c0, GSurface44b2c0_re, GSurface44b2c0_orig);
// 0x0044b450  gSurfaceCtor_44b450
SG_HOOK("golf_clean.exe", 0x0044b450, gSurfaceCtor_44b450, GSurface44b450_re, GSurface44b450_orig);
// 0x0044b5a0  gSurfaceCtor_44b5a0
SG_HOOK("golf_clean.exe", 0x0044b5a0, gSurfaceCtor_44b5a0, GSurface44b5a0_re, GSurface44b5a0_orig);
// 0x0044b6f0  gSurfaceCtor_44b6f0
SG_HOOK("golf_clean.exe", 0x0044b6f0, gSurfaceCtor_44b6f0, GSurface44b6f0_re, GSurface44b6f0_orig);
// 0x0044b8d0  gSurfaceCtor_44b8d0
SG_HOOK("golf_clean.exe", 0x0044b8d0, gSurfaceCtor_44b8d0, GSurface44b8d0_re, GSurface44b8d0_orig);
// 0x00474a90  gSurfaceCtor_839a68
SG_HOOK("golf_clean.exe", 0x00474a90, gSurfaceCtor_839a68, GSurface839a68_re, GSurface839a68_orig);
// 0x004852e0  VoiceRx::ctor
SG_HOOK("golf_clean.exe", 0x004852e0, VoiceRx_ctor, VoiceRxCtor_re, VoiceRxCtor_orig);
// 0x0049b970  netHeapAlloc
SG_HOOK("golf_clean.exe", 0x0049b970, netHeapAlloc, NetHeapAlloc_re, NetHeapAlloc_orig);
