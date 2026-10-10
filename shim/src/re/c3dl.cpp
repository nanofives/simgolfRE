// C3 batch c3dl (round 26, 2026-10-10): golf_clean.exe rows earlier batches dropped for the CRT heap or a Win32 import
// that the two stand-ins together (re/frida/js/heapstandin.js, re/frida/js/importstandin.js) plus the frozen-thread
// helper (re/frida/js/freeze.js) can now run: tiny destructors and constructors, the Reader object, the critical-section
// list family, global-object teardown. Fixtures: re/frida/js/fixtures.d/c3dl.js. Registry: re/frida/registry.d/c3dl.py.
// Purposes and per-jcc tables: log/c3/c3dl_purpose.md.
//
// RULES (c3cy's, c3de's): an import is called through the SAME IAT slot the original reads, never through the shim's own
// import table; callees run through their original addresses; __thiscall is emulated with __fastcall (ecx = this, edx
// unused); every fact cites the address of the instruction it comes from (py -3.12 re/tools/asm2inline.py golf_clean.exe
// 0x<addr> --list).
//
// Hooked callees of other batches (the verifier re-runs these keys with those hooks off):
//   Surface_free 0x00473ae0 (c3cz), MappedFile::dtor 0x00492dc0 (c3cy), operatorDelete_4a4ffc 0x004a4ffc (c3cn)
#include "hooks.h"

namespace {

template <class T>
static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <class T>
static inline T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
// The import whose IAT slot is at `slot` (`call dword ptr [slot]` in the original).
template <typename T>
static inline T Imp(unsigned slot) { return *reinterpret_cast<T*>(slot); }

// ---- golf_clean.exe IAT slots (pefile over original\golf_clean.exe; the exe has no relocations)
typedef int(__stdcall* Stdcall1_t)(void* a);
const unsigned kIatCloseHandle = 0x004ba0cc;
const unsigned kIatUnmapViewOfFile = 0x004ba0d0;
const unsigned kIatLeaveCriticalSection = 0x004ba0e0;
const unsigned kIatEnterCriticalSection = 0x004ba0e8;
const unsigned kIatDestroyCursor = 0x004ba1b0;
const unsigned kIatBinkBufferClose = 0x004ba224;      // binkw32.dll _BinkBufferClose@4
const unsigned kIatBinkClose = 0x004ba260;            // binkw32.dll _BinkClose@4

// ---- exe callees (original addresses)
typedef void(__fastcall* Void0_t)(void* ecx, void* edx);
typedef void*(__fastcall* Ptr0_t)(void* ecx, void* edx);
typedef void*(__cdecl* Malloc_t)(unsigned size);
typedef void(__cdecl* Free_t)(void* p);
typedef int(__cdecl* Fclose_t)(void* fp);
const Void0_t kSurfaceFree = reinterpret_cast<Void0_t>(0x00473ae0);
const Void0_t kMappedFileDtor = reinterpret_cast<Void0_t>(0x00492dc0);
const Malloc_t kMalloc = reinterpret_cast<Malloc_t>(0x004a504f);
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);
const Free_t kOperatorDelete = reinterpret_cast<Free_t>(0x004a4ffc);
const Fclose_t kFclose = reinterpret_cast<Fclose_t>(0x004a609f);

// =====================================================================================================================
// Globals read by tiny wrappers
// =====================================================================================================================

typedef int(__cdecl* Cdecl0_t)();
// 0x004840c0  callSoundHook94(): the module handle global 0x0083afc0 (`mov eax, [0x0083afc0]` 0x004840c0) at zero returns
// 0 (`xor eax, eax; ret` 0x004840c9 / 0x004840cb); otherwise the function pointer stored in 0x0083af94 is entered with a
// plain `jmp` (0x004840cc), so its result is the result of this function and no argument was pushed.
Cdecl0_t CallSoundHook94_orig;
int __cdecl CallSoundHook94_re() {
    if (G<int>(0x0083afc0) == 0) return 0;                                   // 0x004840c5 / 0x004840c9
    return G<Cdecl0_t>(0x0083af94)();                                        // 0x004840cc
}

// =====================================================================================================================
// Small destructors and constructors
// =====================================================================================================================

// 0x00492da0  MappedFile::scalarDtor(this, flags): MappedFile::dtor 0x00492dc0 (call 0x00492da3); bit 0 of flags (`test
// byte ptr [esp + 8], 1` 0x00492da8, `je` 0x00492dad) hands the object to operator delete 0x004a4ffc (push 0x00492daf,
// call 0x00492db0); returns this (`mov eax, esi` 0x00492db8); `ret 4` (0x00492dbb).
typedef void*(__fastcall* ScalarDtor_t)(void* ecx, void* edx, unsigned flags);
ScalarDtor_t MappedFileScalarDtor_orig;
void* __fastcall MappedFileScalarDtor_re(void* self, void*, unsigned flags) {
    kMappedFileDtor(self, 0);                                                // 0x00492da3
    if (flags & 1) kOperatorDelete(self);                                    // 0x00492da8-0x00492db0
    return self;                                                             // 0x00492db8
}

// 0x004884b0  Cursor::destroy(this): a cursor handle at +4 (`mov eax, [esi + 4]` 0x004884b6, `je` 0x004884bb) goes to
// DestroyCursor [0x4ba1b0] (push 0x004884bd, call 0x004884be) and +4 is cleared (0x004884c4). An object at +8 (0x004884c7
// / 0x004884cc) gets its virtual slot 0 with the argument 1 (`mov eax, [ecx]` 0x004884ce, `push 1` 0x004884d0, call
// [eax] 0x004884d2: the scalar deleting destructor) and +8 is cleared (0x004884d4). +0xc and +0x10 are cleared
// (0x004884d7 / 0x004884da). eax at the `ret` (0x004884df) is the last call's result, or +4 / +8 read as zero.
typedef int(__fastcall* Int0_t)(void* ecx, void* edx);
typedef int(__fastcall* Slot1_t)(void* ecx, void* edx, int a);
Int0_t CursorDestroy_orig;
int __fastcall CursorDestroy_re(void* self, void*) {
    int r = 0;                                                               // edi, 0x004884b4
    void* handle = At<void*>(self, 4);                                       // 0x004884b6
    if (handle != 0) {                                                       // 0x004884b9 / 0x004884bb
        r = Imp<Stdcall1_t>(kIatDestroyCursor)(handle);                      // 0x004884be
        At<unsigned>(self, 4) = 0;                                           // 0x004884c4
    }
    void* obj = At<void*>(self, 8);                                          // 0x004884c7
    if (obj != 0) {                                                          // 0x004884ca / 0x004884cc
        void** vt = *reinterpret_cast<void***>(obj);                         // 0x004884ce
        r = reinterpret_cast<Slot1_t>(vt[0])(obj, 0, 1);                     // 0x004884d0 / 0x004884d2
        At<unsigned>(self, 8) = 0;                                           // 0x004884d4
    }
    At<unsigned>(self, 0xc) = 0;                                             // 0x004884d7
    At<unsigned>(self, 0x10) = 0;                                            // 0x004884da
    return r;                                                                // 0x004884df
}

// 0x00487060  BinkPlayer::close(this): a Bink handle at +4 (`mov eax, [esi + 4]` 0x00487066, `je` 0x0048706b) goes to
// _BinkClose [0x4ba260] (push 0x0048706d, call 0x0048706e); a buffer handle at +8 (0x00487074 / 0x00487079) goes to
// _BinkBufferClose [0x4ba224] (0x0048707c). Then +4 and +8 are cleared (0x00487082 / 0x00487085). eax at the `ret`
// (0x0048708a) is the last call's result, or +8 read as zero.
Int0_t BinkPlayerClose_orig;
int __fastcall BinkPlayerClose_re(void* self, void*) {
    int r = 0;
    if (At<void*>(self, 4) != 0)                                             // 0x00487066 / 0x0048706b
        Imp<Stdcall1_t>(kIatBinkClose)(At<void*>(self, 4));                  // 0x0048706e
    void* buf = At<void*>(self, 8);                                          // 0x00487074
    if (buf != 0)                                                            // 0x00487077 / 0x00487079
        r = Imp<Stdcall1_t>(kIatBinkBufferClose)(buf);                       // 0x0048707c
    At<unsigned>(self, 4) = 0;                                               // 0x00487082
    At<unsigned>(self, 8) = 0;                                               // 0x00487085
    return r;                                                                // 0x0048708a
}

// 0x00486c90  Timer::ctor(this): the Timer vtable 0x004baea0 into +0 (0x00486c94), +0xc .. +0x24 (seven dwords, 0x0c..0x24) = 0
// (0x00486c9a-0x00486cac), +0x28 = 5 (0x00486caf), +0x2c = 0 (0x00486cb6), +8 = 0 (0x00486cb9), +4 = 0
// (0x00486cbc). Returns this (`mov eax, ecx` 0x00486c90).
typedef void*(__fastcall* Ctor_t)(void* ecx, void* edx);
Ctor_t TimerCtor_orig;
void* __fastcall TimerCtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004baea0;                                      // 0x00486c94
    for (unsigned off = 0xc; off <= 0x24; off += 4)
        At<unsigned>(self, off) = 0;                                         // 0x00486c9a-0x00486cac
    At<unsigned>(self, 0x28) = 5;                                            // 0x00486caf
    At<unsigned>(self, 0x2c) = 0;                                            // 0x00486cb6
    At<unsigned>(self, 8) = 0;                                               // 0x00486cb9
    At<unsigned>(self, 4) = 0;                                               // 0x00486cbc
    return self;
}

// 0x00487210  StrList::ctor(this): the vtable 0x004baeac into +0 (0x00487215), +4 = 0 (`lea ecx, [eax + 4]` 0x00487212, store
// 0x0048721f), +8 = 0x7f (0x00487221), +0xc = 0 (0x00487228), +0x18 = 0 (0x0048722b), +0x1c = 0 (0x0048722e), +0x10 = 0
// (0x00487231), +0x14 = 0 (0x00487234). Returns this (`mov eax, ecx` 0x00487210).
Ctor_t StrListCtor_orig;
void* __fastcall StrListCtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004baeac;                                      // 0x00487215
    At<unsigned>(self, 4) = 0;                                               // 0x0048721f
    At<unsigned>(self, 8) = 0x7f;                                            // 0x00487221
    At<unsigned>(self, 0xc) = 0;                                             // 0x00487228
    At<unsigned>(self, 0x18) = 0;                                            // 0x0048722b
    At<unsigned>(self, 0x1c) = 0;                                            // 0x0048722e
    At<unsigned>(self, 0x10) = 0;                                            // 0x00487231
    At<unsigned>(self, 0x14) = 0;                                            // 0x00487234
    return self;
}

// =====================================================================================================================
// Reader (object 0x164 bytes: +0 vtable 0x004bb084, +4 byte, +0x154 .. +0x160 dwords; +0x158 FILE*, +0x15c / +0x160 buffers)
// =====================================================================================================================

// 0x00487f80  Reader::closeFile(this): a FILE* at +0x158 (`mov eax, [esi + 0x158]` 0x00487f83, `je` 0x00487f8b) goes to
// the CRT fclose 0x004a609f (push 0x00487f8d, call 0x00487f8e) and +0x158 is cleared (0x00487f96). eax at the `ret`
// (0x00487fa1) is fclose's result, or the zero field value.
Int0_t ReaderCloseFile_orig;
int __fastcall ReaderCloseFile_re(void* self, void*) {
    int r = 0;
    void* fp = At<void*>(self, 0x158);                                       // 0x00487f83
    if (fp != 0) {                                                           // 0x00487f89 / 0x00487f8b
        r = kFclose(fp);                                                     // 0x00487f8e
        At<unsigned>(self, 0x158) = 0;                                       // 0x00487f96
    }
    return r;                                                                // 0x00487fa1
}

// 0x00487f30  Reader::free(this): Reader::closeFile 0x00487f80 (call 0x00487f33); a block at +0x15c (0x00487f38 /
// 0x00487f3e) goes to the CRT free 0x004a5007 (call 0x00487f43) and +0x15c is cleared (0x00487f4b); the same for
// +0x160 (0x00487f55-0x00487f68). The callers (Reader::allocBuffers 0x00487ee4, Reader::scalarDtor 0x00488469) overwrite
// eax afterwards.
Void0_t ReaderFree_orig;
void __fastcall ReaderFree_re(void* self, void*) {
    reinterpret_cast<Int0_t>(0x00487f80)(self, 0);                           // 0x00487f33
    if (At<void*>(self, 0x15c) != 0) {                                       // 0x00487f38 / 0x00487f3e
        kFree(At<void*>(self, 0x15c));                                       // 0x00487f43
        At<unsigned>(self, 0x15c) = 0;                                       // 0x00487f4b
    }
    if (At<void*>(self, 0x160) != 0) {                                       // 0x00487f55 / 0x00487f5b
        kFree(At<void*>(self, 0x160));                                       // 0x00487f60
        At<unsigned>(self, 0x160) = 0;                                       // 0x00487f68
    }
}

// 0x00487ee0  Reader::allocBuffers(this, n): Reader::free 0x00487f30 (call 0x00487ee4), then malloc 0x004a504f of n bytes
// (call 0x00487eee) into +0x15c (0x00487ef6); NULL returns 4 (`mov eax, 4` 0x00487f01). A second malloc of n bytes
// (call 0x00487f0b) goes into +0x160 (0x00487f13); the result is 0 for a block and 4 for NULL (`neg eax; sbb eax, eax;
// and al, 0xfc; add eax, 4` 0x00487f19-0x00487f21).
typedef int(__fastcall* Int1_t)(void* ecx, void* edx, unsigned n);
Int1_t ReaderAllocBuffers_orig;
int __fastcall ReaderAllocBuffers_re(void* self, void*, unsigned n) {
    reinterpret_cast<Void0_t>(0x00487f30)(self, 0);                          // 0x00487ee4
    void* a = kMalloc(n);                                                    // 0x00487eee
    At<void*>(self, 0x15c) = a;                                              // 0x00487ef6
    if (a == 0) return 4;                                                    // 0x00487efc / 0x00487efe / 0x00487f01
    void* b = kMalloc(n);                                                    // 0x00487f0b
    At<void*>(self, 0x160) = b;                                              // 0x00487f13
    if (b == 0) return 4;                                                    // 0x00487f19-0x00487f21 (neg / sbb / and / add)
    return 0;                                                                // 0x00487f24
}

// 0x00487ea0  Reader::ctor(this, n): +4 (a byte) = 0 (0x00487ea5), +0x154 .. +0x160 = 0 (0x00487ea8-0x00487eba), the vtable
// 0x004bb084 into +0 (0x00487ec5), then Reader::allocBuffers 0x00487ee0 with n (push 0x00487ec4, call 0x00487ecb). Returns
// this (`mov eax, esi` 0x00487ed2); `ret 4` (0x00487ed3).
typedef void*(__fastcall* Ctor1_t)(void* ecx, void* edx, unsigned n);
Ctor1_t ReaderCtor_orig;
void* __fastcall ReaderCtor_re(void* self, void*, unsigned n) {
    At<unsigned char>(self, 4) = 0;                                          // 0x00487ea5
    At<unsigned>(self, 0x154) = 0;                                           // 0x00487ea8
    At<unsigned>(self, 0x158) = 0;                                           // 0x00487eae
    At<unsigned>(self, 0x15c) = 0;                                           // 0x00487eb4
    At<unsigned>(self, 0x160) = 0;                                           // 0x00487eba
    At<unsigned>(self, 0) = 0x004bb084;                                      // 0x00487ec5
    reinterpret_cast<Int1_t>(0x00487ee0)(self, 0, n);                        // 0x00487ecb
    return self;                                                             // 0x00487ed2
}

// =====================================================================================================================
// Critical-section list (object: +4 head, +8 tail, +0xc count, +0x10 CRITICAL_SECTION; node 0x18 bytes: +0 / +4 / +8
// caller words, +0xc data block, +0x10 its size, +0x14 next)
// =====================================================================================================================

// 0x004a4b00  listClear4a4b(this): EnterCriticalSection [0x4ba0e8] on this + 0x10 (`lea ebp, [esi + 0x10]` 0x004a4b05, call
// 0x004a4b09). While the head +4 is non-NULL (0x004a4b0f-0x004a4b16, and the loop test `cmp edi, ebx` 0x004a4b47) the head
// node's next (+0x14) is saved (0x004a4b1e), its data block +0xc, when non-NULL, goes to free 0x004a5007 (call 0x004a4b29)
// and +0xc becomes 0 (0x004a4b34), the node goes to free (0x004a4b3f) and +4 takes the saved next (0x004a4b4c). Then tail
// +8 and count +0xc are cleared (0x004a4b50 / 0x004a4b53); LeaveCriticalSection [0x4ba0e0] (call 0x004a4b56) and its
// result stays in eax.
Int0_t ListClear_orig;
int __fastcall ListClear_re(void* self, void*) {
    void* cs = static_cast<char*>(self) + 0x10;                              // 0x004a4b05
    Imp<Stdcall1_t>(kIatEnterCriticalSection)(cs);                           // 0x004a4b09
    void* node = At<void*>(self, 4);                                         // 0x004a4b0f
    if (node != 0) {                                                         // 0x004a4b14 / 0x004a4b16
        for (;;) {
            node = At<void*>(self, 4);                                       // 0x004a4b19
            void* next = At<void*>(node, 0x14);                              // 0x004a4b1e
            if (At<void*>(node, 0xc) != 0)                                   // 0x004a4b21 / 0x004a4b24
                kFree(At<void*>(node, 0xc));                                 // 0x004a4b29
            At<unsigned>(At<void*>(self, 4), 0xc) = 0;                       // 0x004a4b31 / 0x004a4b34
            if (At<void*>(self, 4) != 0)                                     // 0x004a4b37 / 0x004a4b3a
                kFree(At<void*>(self, 4));                                   // 0x004a4b3f
            At<void*>(self, 4) = next;                                       // 0x004a4b4c
            if (next == 0) break;                                            // 0x004a4b47 / 0x004a4b4c
        }
    }
    At<unsigned>(self, 8) = 0;                                               // 0x004a4b50
    At<unsigned>(self, 0xc) = 0;                                             // 0x004a4b53
    return Imp<Stdcall1_t>(kIatLeaveCriticalSection)(cs);                    // 0x004a4b56
}

// 0x004a4b60  listAppend4a4b60(this, src, size, a3, a4, a5): EnterCriticalSection on this + 0x10 (0x004a4b66 / 0x004a4b6f).
// With the head +4 NULL (0x004a4b75 / 0x004a4b7a) a node of 0x18 bytes (push 0x18 0x004a4b78) comes from malloc 0x004a504f
// (call 0x004a4b7e) into +4 (0x004a4b86); NULL leaves the critical section and returns 4 (0x004a4b8b-0x004a4b9c). With a
// head, the node comes from malloc (call 0x004a4ba1) into the old tail's next (+0x14, 0x004a4bac); NULL returns 4
// (0x004a4bb5-0x004a4bc3). The node becomes the tail +8 (0x004a4bd1), its next is 0 (0x004a4bd4), +4 = a3 (0x004a4be2), +0 =
// a4 (0x004a4bec), +8 = a5 (0x004a4bf7). A size above 0 (`test ebp, ebp; je` 0x004a4bf5 / 0x004a4bfa) takes a data block
// from malloc (call 0x004a4bfd) into +0xc (0x004a4c08); NULL returns 4 with the node linked and the count unchanged
// (0x004a4c11-0x004a4c26). A non-NULL src has size bytes copied into the block (`rep movsd / movsb` 0x004a4c3a /
// 0x004a4c41), and +0x10 = size (0x004a4c4a). The count +0xc is incremented (0x004a4c52), the critical section is left
// (call 0x004a4c55) and the result is 0.
typedef int(__fastcall* Append_t)(void* ecx, void* edx, const void* src, unsigned size, unsigned a3, unsigned a4,
                                  unsigned a5);
Append_t ListAppend_orig;
int __fastcall ListAppend_re(void* self, void*, const void* src, unsigned size, unsigned a3, unsigned a4, unsigned a5) {
    void* cs = static_cast<char*>(self) + 0x10;                              // 0x004a4b66
    Imp<Stdcall1_t>(kIatEnterCriticalSection)(cs);                           // 0x004a4b6f
    void* node;
    if (At<void*>(self, 4) == 0) {                                           // 0x004a4b75 / 0x004a4b7c
        node = kMalloc(0x18);                                                // 0x004a4b7e
        At<void*>(self, 4) = node;                                           // 0x004a4b86
        if (node == 0) {                                                     // 0x004a4b89 / 0x004a4b8b
            Imp<Stdcall1_t>(kIatLeaveCriticalSection)(cs);                   // 0x004a4b8e
            return 4;                                                        // 0x004a4b97
        }
    } else {
        node = kMalloc(0x18);                                                // 0x004a4ba1
        At<void*>(At<void*>(self, 8), 0x14) = node;                          // 0x004a4bac
        if (At<void*>(At<void*>(self, 8), 0x14) == 0) {                      // 0x004a4baf-0x004a4bb7
            Imp<Stdcall1_t>(kIatLeaveCriticalSection)(cs);                   // 0x004a4bba
            return 4;                                                        // 0x004a4bc3
        }
    }
    At<void*>(self, 8) = node;                                               // 0x004a4bd1
    At<unsigned>(node, 0x14) = 0;                                            // 0x004a4bd4
    At<unsigned>(node, 4) = a3;                                              // 0x004a4be2
    At<unsigned>(node, 0) = a4;                                              // 0x004a4bec
    At<unsigned>(node, 8) = a5;                                              // 0x004a4bf7
    if (size != 0) {                                                         // 0x004a4bf5 / 0x004a4bfa
        void* data = kMalloc(size);                                          // 0x004a4bfd
        At<void*>(node, 0xc) = data;                                         // 0x004a4c08
        if (data == 0) {                                                     // 0x004a4c11 / 0x004a4c13
            Imp<Stdcall1_t>(kIatLeaveCriticalSection)(cs);                   // 0x004a4c16
            return 4;                                                        // 0x004a4c1f
        }
        if (src != 0) {                                                      // 0x004a4c29 / 0x004a4c2d / 0x004a4c2f
            const char* s = static_cast<const char*>(src);
            char* d = static_cast<char*>(data);
            for (unsigned i = 0; i < size; i++) d[i] = s[i];                 // 0x004a4c3a-0x004a4c41
        }
        At<unsigned>(node, 0x10) = size;                                     // 0x004a4c4a
    }
    At<unsigned>(self, 0xc) = At<unsigned>(self, 0xc) + 1;                   // 0x004a4c4d-0x004a4c52
    Imp<Stdcall1_t>(kIatLeaveCriticalSection)(cs);                           // 0x004a4c55
    return 0;                                                                // 0x004a4c5e
}

// 0x004a4c70  queuePop(this, dst, outA, outB, outSize): EnterCriticalSection on this + 0x10 (0x004a4c76 / 0x004a4c7e). With
// the head +4 NULL (0x004a4c84 / 0x004a4c87 / 0x004a4c89) the section is left (call 0x004a4c8c) and the result is 0
// (0x004a4c94). Otherwise: a non-NULL outSize receives the head's +0x10 (0x004a4c9b-0x004a4ca6); a head data block +0xc
// with a non-NULL dst (0x004a4ca9-0x004a4cb6) is copied into dst (the head's +0x10 bytes, `rep movsd / movsb`
// 0x004a4cc2 / 0x004a4cc9), freed (call 0x004a4cd3) and its field cleared (0x004a4cdf); the head becomes the node's next
// +0x14 (0x004a4ce9-0x004a4cf2); a non-NULL outA receives the node's +4 (0x004a4cf7 / 0x004a4cfa) and a non-NULL outB its
// +0 (0x004a4d04 / 0x004a4d06); the result is the node's +8 (0x004a4d08); the node goes to free (call 0x004a4d10); the count
// +0xc is decremented (0x004a4d1d); the section is left (call 0x004a4d20). The tail +8 is not touched.
typedef unsigned(__fastcall* Pop_t)(void* ecx, void* edx, void* dst, unsigned* outA, unsigned* outB, unsigned* outSize);
Pop_t QueuePop_orig;
unsigned __fastcall QueuePop_re(void* self, void*, void* dst, unsigned* outA, unsigned* outB, unsigned* outSize) {
    void* cs = static_cast<char*>(self) + 0x10;                              // 0x004a4c76
    Imp<Stdcall1_t>(kIatEnterCriticalSection)(cs);                           // 0x004a4c7e
    void* node = At<void*>(self, 4);                                         // 0x004a4c84
    if (node == 0) {                                                         // 0x004a4c87 / 0x004a4c89
        Imp<Stdcall1_t>(kIatLeaveCriticalSection)(cs);                       // 0x004a4c8c
        return 0;                                                            // 0x004a4c94
    }
    if (outSize != 0) *outSize = At<unsigned>(node, 0x10);                   // 0x004a4c9b-0x004a4ca6
    void* data = At<void*>(node, 0xc);                                       // 0x004a4ca9
    if (data != 0 && dst != 0) {                                             // 0x004a4cac / 0x004a4cae / 0x004a4cb4 / 0x004a4cb6
        unsigned n = At<unsigned>(node, 0x10);                               // 0x004a4cb8
        const char* s = static_cast<const char*>(data);
        char* d = static_cast<char*>(dst);
        for (unsigned i = 0; i < n; i++) d[i] = s[i];                        // 0x004a4cc2-0x004a4cc9
        if (At<void*>(node, 0xc) != 0)                                       // 0x004a4ccb / 0x004a4cce / 0x004a4cd0
            kFree(At<void*>(node, 0xc));                                     // 0x004a4cd3
        At<unsigned>(node, 0xc) = 0;                                         // 0x004a4cdf
    }
    At<void*>(self, 4) = At<void*>(At<void*>(self, 4), 0x14);                // 0x004a4ce6-0x004a4cf2
    if (outA != 0) *outA = At<unsigned>(node, 4);                            // 0x004a4cf7 / 0x004a4cfa
    if (outB != 0) *outB = At<unsigned>(node, 0);                            // 0x004a4d04 / 0x004a4d06
    unsigned r = At<unsigned>(node, 8);                                      // 0x004a4d08
    kFree(node);                                                             // 0x004a4d10 (the test at 0x004a4d0b / 0x004a4d0d is on a pointer known non-NULL)
    At<unsigned>(self, 0xc) = At<unsigned>(self, 0xc) - 1;                   // 0x004a4d18-0x004a4d1d
    Imp<Stdcall1_t>(kIatLeaveCriticalSection)(cs);                           // 0x004a4d20
    return r;                                                                // 0x004a4d26
}

// =====================================================================================================================
// Net operations, display forwarders
// =====================================================================================================================

// 0x00497c20  netBeginOp0(this, a): the net object global 0x008400b0 at zero returns 7 (`mov eax, [0x008400b0]` 0x00497c20,
// `mov eax, 7` 0x00497c2c). Otherwise, with none of the bits 0x60000000 set in +0xe8 (`test dword ptr [esi + 0xe8],
// 0x60000000` 0x00497c35, `jne` 0x00497c3f): +0x60 = 0 (0x00497c4b), +0xe4 = a (0x00497c52), the sub-object at this + 0x64
// (`lea ecx, [esi + 0x64]` 0x00497c48) gets its virtual slot +0x90 (`mov edx, [esi + 0x64]` 0x00497c45, call [edx + 0x90]
// 0x00497c58) with no arguments, and +0xe8 |= 0x20000000 (0x00497c5e). The result is 0 (`xor eax, eax` 0x00497c68).
typedef int(__fastcall* Op_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* Slot0_t)(void* ecx, void* edx);
Op_t NetBeginOp0_orig;
int __fastcall NetBeginOp0_re(void* self, void*, int a) {
    if (G<int>(0x008400b0) == 0) return 7;                                   // 0x00497c25 / 0x00497c28 / 0x00497c2c
    if ((At<unsigned>(self, 0xe8) & 0x60000000u) == 0) {                     // 0x00497c35 / 0x00497c3f
        At<unsigned>(self, 0x60) = 0;                                        // 0x00497c4b
        At<int>(self, 0xe4) = a;                                             // 0x00497c52
        void* sub = static_cast<char*>(self) + 0x64;                         // 0x00497c48
        void** vt = *reinterpret_cast<void***>(sub);                         // 0x00497c45
        reinterpret_cast<Slot0_t>(vt[0x90 / 4])(sub, 0);                     // 0x00497c58
        At<unsigned>(self, 0xe8) |= 0x20000000u;                             // 0x00497c5e
    }
    return 0;                                                                // 0x00497c68
}

// 0x00497c70  netBeginOp1(this, a): the same as netBeginOp0 with +0x60 = 1 (0x00497c9b); the tests are at 0x00497c70-0x00497c8f,
// the slot call at 0x00497ca8, the flag at 0x00497cae and the result at 0x00497cb8.
Op_t NetBeginOp1_orig;
int __fastcall NetBeginOp1_re(void* self, void*, int a) {
    if (G<int>(0x008400b0) == 0) return 7;                                   // 0x00497c75 / 0x00497c78 / 0x00497c7c
    if ((At<unsigned>(self, 0xe8) & 0x60000000u) == 0) {                     // 0x00497c85 / 0x00497c8f
        At<unsigned>(self, 0x60) = 1;                                        // 0x00497c9b
        At<int>(self, 0xe4) = a;                                             // 0x00497ca2
        void* sub = static_cast<char*>(self) + 0x64;                         // 0x00497c98
        void** vt = *reinterpret_cast<void***>(sub);                         // 0x00497c95
        reinterpret_cast<Slot0_t>(vt[0x90 / 4])(sub, 0);                     // 0x00497ca8
        At<unsigned>(self, 0xe8) |= 0x20000000u;                             // 0x00497cae
    }
    return 0;                                                                // 0x00497cb8
}

// 0x00483ba0  screenFlip(): `mov ecx, [0x0083ad50]` (0x00483ba0), `mov eax, [ecx]` (0x00483ba6) and a tail `jmp [eax + 0x124]`
// (0x00483ba8): the display's virtual slot +0x124 with the display as this, no arguments; its result is the result.
typedef int(__cdecl* Cdecl0i_t)();
Cdecl0i_t ScreenFlip_orig;
int __cdecl ScreenFlip_re() {
    void* disp = G<void*>(0x0083ad50);                                       // 0x00483ba0
    void** vt = *reinterpret_cast<void***>(disp);                            // 0x00483ba6
    return reinterpret_cast<Slot0_t>(vt[0x124 / 4])(disp, 0);                // 0x00483ba8
}

// 0x00475c20  DC_calla0(this, a, b, c, d): the object at +4 (`mov ecx, [ecx + 4]` 0x00475c20) NULL returns 7 (0x00475c27); a
// NULL `a` returns 0x10 (0x00475c33-0x00475c37). Otherwise the object's virtual slot +0xa0 (`mov edx, [ecx]` 0x00475c47,
// call [edx + 0xa0] 0x00475c55) runs with this = the object and the arguments (a->+4 (0x00475c44), b, c, d) (pushes
// 0x00475c49-0x00475c54, d first); its result is the result.
typedef int(__fastcall* Dc4_t)(void* ecx, void* edx, void* a, int b, int c, int d);
typedef int(__fastcall* SlotA0_t)(void* ecx, void* edx, int a4, int b, int c, int d);
Dc4_t DcCalla0_orig;
int __fastcall DcCalla0_re(void* self, void*, void* a, int b, int c, int d) {
    void* obj = At<void*>(self, 4);                                          // 0x00475c20
    if (obj == 0) return 7;                                                  // 0x00475c23 / 0x00475c25 / 0x00475c27
    if (a == 0) return 0x10;                                                 // 0x00475c2f-0x00475c37
    void** vt = *reinterpret_cast<void***>(obj);                             // 0x00475c47
    return reinterpret_cast<SlotA0_t>(vt[0xa0 / 4])(obj, 0, At<int>(a, 4), b, c, d);   // 0x00475c44-0x00475c55
}

// =====================================================================================================================
// Live-global surfaces, logger
// =====================================================================================================================

// 0x0049ead0  freeCursors(): Surface_free 0x00473ae0 on the global Surfaces 0x00840860 (`mov ecx, 0x00840860` 0x0049ead0,
// call 0x0049ead5), 0x00840890 (0x0049eada / 0x0049eadf) and, as a tail jump, 0x00840830 (0x0049eae4 / 0x0049eae9).
// Its caller (0x00485740 shutdown) ignores eax.
typedef void(__cdecl* CdeclVoid0_t)();
CdeclVoid0_t FreeCursors_orig;
void __cdecl FreeCursors_re() {
    kSurfaceFree(reinterpret_cast<void*>(0x00840860), 0);                    // 0x0049ead5
    kSurfaceFree(reinterpret_cast<void*>(0x00840890), 0);                    // 0x0049eadf
    kSurfaceFree(reinterpret_cast<void*>(0x00840830), 0);                    // 0x0049eae9
}

// 0x004a01d0  loggerSetPath(this, path): a NULL path returns 0x10 (0x004a01d7 / 0x004a01db / 0x004a01df). An existing block
// at +4 goes to free 0x004a5007 (call 0x004a01f0) and +4 is cleared (0x004a01f8). A block of strlen(path) + 1 bytes
// (`repne scasb` 0x004a0206 / 0x004a0208, push 0x004a020a) from malloc 0x004a504f (call 0x004a020b) goes to +4
// (0x004a0213); NULL returns 4 (0x004a021c). The block's first byte is 0 (0x004a0225) and the path is appended to it with
// the inlined strcat (0x004a0228-0x004a0255: find the end of the destination, copy the source with its NUL). The
// function 0x004a00f0 (path, the string 0x004c7924: call 0x004a0257) is the path opener; its result 0 returns 6
// (0x004a025f / 0x004a0262 / 0x004a0266), any other value goes to fclose 0x004a609f (call 0x004a0270) and the result is 0
// (`xor eax, eax` 0x004a0278).
typedef int(__fastcall* LoggerSetPath_t)(void* ecx, void* edx, const char* path);
typedef void*(__cdecl* Open2_t)(const char* path, const char* mode);
LoggerSetPath_t LoggerSetPath_orig;
int __fastcall LoggerSetPath_re(void* self, void*, const char* path) {
    if (path == 0) return 0x10;                                              // 0x004a01d7 / 0x004a01db
    if (At<void*>(self, 4) != 0) {                                           // 0x004a01e8 / 0x004a01eb
        kFree(At<void*>(self, 4));                                           // 0x004a01f0
        At<unsigned>(self, 4) = 0;                                           // 0x004a01f8
    }
    unsigned n = 0;
    while (path[n] != 0) n++;                                                // 0x004a0206
    char* buf = static_cast<char*>(kMalloc(n + 1));                          // 0x004a020b
    At<char*>(self, 4) = buf;                                                // 0x004a0213
    if (buf == 0) return 4;                                                  // 0x004a0216 / 0x004a0218 / 0x004a021c
    buf[0] = 0;                                                              // 0x004a0225
    char* d = buf;
    while (*d != 0) d++;                                                     // 0x004a0240 / 0x004a0244
    for (unsigned i = 0; i <= n; i++) d[i] = path[i];                        // 0x004a0248-0x004a0255 (the NUL included)
    void* fp = reinterpret_cast<Open2_t>(0x004a00f0)(path, reinterpret_cast<const char*>(0x004c7924));   // 0x004a0257
    if (fp == 0) return 6;                                                   // 0x004a025f / 0x004a0262
    kFclose(fp);                                                             // 0x004a0270
    return 0;                                                                // 0x004a0278
}

// 0x004a0180  loggerCtor(this, path): the logger vtable 0x004bc034 into +0 (0x004a0188), +4 = 0 (0x004a018e), then
// loggerSetPath 0x004a01d0 with the path (push 0x004a0187, call 0x004a0195). Returns this (`mov eax, esi` 0x004a019a).
Ctor1_t LoggerCtor_orig;
void* __fastcall LoggerCtor_re(void* self, void*, unsigned path) {
    At<unsigned>(self, 0) = 0x004bc034;                                      // 0x004a0188
    At<unsigned>(self, 4) = 0;                                               // 0x004a018e
    reinterpret_cast<LoggerSetPath_t>(0x004a01d0)(self, 0, reinterpret_cast<const char*>(path));   // 0x004a0195
    return self;                                                             // 0x004a019a
}

// =====================================================================================================================
// Objects with two embedded HotLists (+0xb4 and +0x254; 0x2b8 bytes)
// =====================================================================================================================

typedef void*(__fastcall* HlAlloc_t)(void* ecx, void* edx, int n);
typedef int(__fastcall* HlAdd_t)(void* ecx, void* edx, int a1, int a2, int a3, int a4, int a5, int a6, int a7);
typedef int(__stdcall* MessageBoxA_t)(void* hwnd, const char* text, const char* caption, unsigned type);
const HlAlloc_t kHotListAlloc = reinterpret_cast<HlAlloc_t>(0x00492920);
const HlAdd_t kHotListAdd = reinterpret_cast<HlAdd_t>(0x004929b0);
const unsigned kIatMessageBoxA = 0x004ba1f4;

// 0x00478a20  List_reset(this): HotList::alloc 0x00492920 on this + 0xb4 (`lea ecx, [esi + 0xb4]` 0x00478a25) with n = 0x50
// (`push 0x50` 0x00478a23, call 0x00478a2b); +0xb0 = 0 (0x00478a30). Then 0x28 dwords from +0x1b4 (`lea eax, [esi + 0x1b4]`
// 0x00478a3a, ecx = 0x28 0x00478a40): a non-zero one is cleared (`cmp [eax], 0; je` 0x00478a4a / 0x00478a4d, store
// 0x00478a4f) and the dword 0xa0 below it (+0x114 + 4 * k) is set to -2 (`mov edx, 0xfffffffe` 0x00478a45, store 0x00478a55).
// eax at the `ret` (0x00478a62) is the loop pointer, this + 0x254.
typedef unsigned(__fastcall* ListReset_t)(void* ecx, void* edx);
ListReset_t ListReset_orig;
unsigned __fastcall ListReset_re(void* self, void*) {
    kHotListAlloc(static_cast<char*>(self) + 0xb4, 0, 0x50);                 // 0x00478a2b
    At<unsigned>(self, 0xb0) = 0;                                            // 0x00478a30
    char* p = static_cast<char*>(self) + 0x1b4;                              // 0x00478a3a
    for (int k = 0; k < 0x28; k++) {                                         // 0x00478a40 / 0x00478a5e / 0x00478a5f
        if (At<unsigned>(p, 0) != 0)                                         // 0x00478a4a / 0x00478a4d
            At<unsigned>(p, 0) = 0;                                          // 0x00478a4f
        *reinterpret_cast<int*>(p - 0xa0) = -2;                              // 0x00478a55
        p += 4;                                                              // 0x00478a5b
    }
    return reinterpret_cast<unsigned>(p);                                    // 0x00478a62
}

// 0x00478a70  Widget_reset(this): HotList::alloc 0x00492920 on this + 0x254 (`lea ecx, [esi + 0x254]` 0x00478a75) with n = 10
// (`push 0xa` 0x00478a73, call 0x00478a7b); +0x2b4 = 0 (0x00478a80). eax at the `ret` (0x00478a8b) is alloc's result.
typedef void*(__fastcall* WidgetReset_t)(void* ecx, void* edx);
WidgetReset_t WidgetReset_orig;
void* __fastcall WidgetReset_re(void* self, void*) {
    void* r = kHotListAlloc(static_cast<char*>(self) + 0x254, 0, 10);        // 0x00478a7b
    At<unsigned>(self, 0x2b4) = 0;                                           // 0x00478a80
    return r;                                                                // 0x00478a8b
}

// 0x00478a90  addDropdownSpot(this, a1 .. a6): the counter +0x2b4 is incremented and stored (`inc edx` 0x00478a98, store
// 0x00478a9b); a result above 0x14 (signed: `cmp eax, 0x14; jle` 0x00478aa1 / 0x00478aa4) shows MessageBoxA [0x4ba1f4]
// (hwnd 0, text 0x004e428c, caption 0x004e4280, type 0: pushes 0x00478a96-0x00478ab0, call 0x00478ab2) and returns its
// result. Otherwise HotList::add 0x004929b0 runs on this + 0x254 (`add ecx, 0x254` 0x00478ad9) with (a1 .. a6, 0) (the
// pushes 0x00478ac3-0x00478ad8 reverse the argument order into the callee's) and its result is returned.
typedef int(__fastcall* DropdownSpot_t)(void* ecx, void* edx, int a1, int a2, int a3, int a4, int a5, int a6);
DropdownSpot_t AddDropdownSpot_orig;
int __fastcall AddDropdownSpot_re(void* self, void*, int a1, int a2, int a3, int a4, int a5, int a6) {
    int n = At<int>(self, 0x2b4) + 1;                                        // 0x00478a90 / 0x00478a98
    At<int>(self, 0x2b4) = n;                                                // 0x00478a9b
    if (n > 0x14)                                                            // 0x00478aa1 / 0x00478aa4
        return Imp<MessageBoxA_t>(kIatMessageBoxA)(0, reinterpret_cast<const char*>(0x004e428c),
                                                   reinterpret_cast<const char*>(0x004e4280), 0);   // 0x00478ab2
    return kHotListAdd(static_cast<char*>(self) + 0x254, 0, a1, a2, a3, a4, a5, a6, 0);   // 0x00478adf
}

// =====================================================================================================================
// Constructors of the UI object chain (Image 0x2b8 bytes, View 0x274, View4804a0 0x574+, EditBox, the global object)
// =====================================================================================================================

typedef void*(__fastcall* CtorFn_t)(void* ecx, void* edx);
const CtorFn_t kViewCtor = reinterpret_cast<CtorFn_t>(0x00479c40);
const CtorFn_t kView4804a0Ctor = reinterpret_cast<CtorFn_t>(0x004804a0);
const CtorFn_t kImageCtor = reinterpret_cast<CtorFn_t>(0x00474ae0);
const CtorFn_t kTimerCtor = reinterpret_cast<CtorFn_t>(0x00486c90);
const CtorFn_t kCtor4837f0 = reinterpret_cast<CtorFn_t>(0x004837f0);
const CtorFn_t kHotListCtor = reinterpret_cast<CtorFn_t>(0x00492850);
typedef unsigned(__fastcall* ResetFn_t)(void* ecx, void* edx);
const ResetFn_t kEditBoxReset = reinterpret_cast<ResetFn_t>(0x00485ff0);
// initWidgetTable 0x00495d30 (C3, c3g) copies 38 live globals into the object
const CtorFn_t kViewBaseCtor = reinterpret_cast<CtorFn_t>(0x00495d30);

// 0x00474ae0  Image_ctor(this): the vtable 0x004ba86c into +0 (0x00474b00) and +4 = 0 (0x00474b0c), HotList::ctor 0x00492850
// on this + 0xb4 (call 0x00474b0f) and this + 0x254 (call 0x00474b1e), the final vtable 0x004ba858 into +0 (0x00474b23),
// +4 = 0 (0x00474b29), the byte +0x28 = 0 (0x00474b2c), forty dwords: +0x1b4 + 4k = 0 and +0x114 + 4k = -2 (the loop
// 0x00474b3a-0x00474b4a, ecx = 0x28 at 0x00474b35), then the field resets listed below; +0x40 takes the dword at 0x004e4210
// (0x00474b7c / 0x00474b82) and +0x5c the global 0x0083ad44 (0x00474b97 / 0x00474bf4). Returns this (`mov eax, esi`
// 0x00474c03). The SEH frame (0x00474ae0-0x00474aee) only matters if a callee throws.
CtorFn_t ImageCtor_orig;
void* __fastcall ImageCtor_re(void* self, void*) {
    At<unsigned>(self, 0) = 0x004ba86c;                                      // 0x00474b00
    At<unsigned>(self, 4) = 0;                                               // 0x00474b0c
    At<unsigned>(self, 0x0) = 0x4ba86c;                     // 0x00474b00
    At<unsigned>(self, 0x4) = 0;                            // 0x00474b0c
    kHotListCtor(static_cast<char*>(self) + 0xb4, 0);       // 0x00474b0f
    kHotListCtor(static_cast<char*>(self) + 0x254, 0);      // 0x00474b1e
    At<unsigned>(self, 0x0) = 0x4ba858;                     // 0x00474b23
    At<unsigned>(self, 0x4) = 0;                            // 0x00474b29
    At<unsigned char>(self, 0x28) = 0;// 0x00474b2c
    for (int k = 0; k < 0x28; k++) {                                         // 0x00474b35 / 0x00474b49 / 0x00474b4a
        At<unsigned>(self, 0x1b4 + 4 * k) = 0;                               // 0x00474b3a
        At<unsigned>(self, 0x114 + 4 * k) = 0xfffffffeu;                     // 0x00474b40
    }
    At<unsigned>(self, 0xac) = 0;                           // 0x00474b4c
    At<unsigned>(self, 0x8) = 0;                            // 0x00474b52
    At<unsigned>(self, 0x10) = 0;                           // 0x00474b55
    At<unsigned>(self, 0xc) = 0;                            // 0x00474b58
    At<unsigned>(self, 0x18) = 0;                           // 0x00474b5b
    At<unsigned>(self, 0x14) = 0;                           // 0x00474b5e
    At<unsigned>(self, 0x1c) = 0;                           // 0x00474b61
    At<unsigned>(self, 0x20) = 0;                           // 0x00474b64
    At<unsigned>(self, 0x24) = 0;                           // 0x00474b6a
    At<unsigned>(self, 0x2c) = 0xffffffffu;                 // 0x00474b6d
    At<unsigned>(self, 0x30) = 0;                           // 0x00474b70
    At<unsigned>(self, 0x34) = 0;                           // 0x00474b73
    At<unsigned>(self, 0x38) = 0;                           // 0x00474b76
    At<unsigned>(self, 0x3c) = 0;                           // 0x00474b79
    At<unsigned>(self, 0x40) = G<unsigned>(0x004e4210);     // 0x00474b82
    At<unsigned>(self, 0x44) = 0xffffffffu;                 // 0x00474b85
    At<unsigned>(self, 0x48) = 0;                           // 0x00474b88
    At<unsigned>(self, 0x4c) = 0;                           // 0x00474b8b
    At<unsigned>(self, 0x50) = 0;                           // 0x00474b8e
    At<unsigned>(self, 0x54) = 0;                           // 0x00474b91
    At<unsigned>(self, 0x58) = 0;                           // 0x00474b94
    At<unsigned>(self, 0x7c) = 0xffffffffu;                 // 0x00474ba2
    At<unsigned>(self, 0x8c) = 2;                           // 0x00474ba5
    At<unsigned>(self, 0x9c) = 2;                           // 0x00474bab
    At<unsigned>(self, 0x70) = 0xffffffffu;                 // 0x00474bb1
    At<unsigned>(self, 0x80) = 0xffffffffu;                 // 0x00474bb4
    At<unsigned>(self, 0x90) = 2;                           // 0x00474bba
    At<unsigned>(self, 0xa0) = 2;                           // 0x00474bc0
    At<unsigned>(self, 0x74) = 0xffffffffu;                 // 0x00474bc6
    At<unsigned>(self, 0x84) = 0xffffffffu;                 // 0x00474bc9
    At<unsigned>(self, 0x94) = 2;                           // 0x00474bcf
    At<unsigned>(self, 0xa4) = 2;                           // 0x00474bd5
    At<unsigned>(self, 0x78) = 0xffffffffu;                 // 0x00474bdb
    At<unsigned>(self, 0x88) = 0xffffffffu;                 // 0x00474bde
    At<unsigned>(self, 0x98) = 2;                           // 0x00474be4
    At<unsigned>(self, 0xa8) = 2;                           // 0x00474bea
    At<unsigned>(self, 0x5c) = G<unsigned>(0x0083ad44);     // 0x00474bf4
    At<unsigned>(self, 0x6c) = 0;                           // 0x00474bf7
    At<unsigned>(self, 0x60) = 0;                           // 0x00474bfa
    At<unsigned>(self, 0x64) = 0;                           // 0x00474bfd
    At<unsigned>(self, 0x68) = 0;                           // 0x00474c00
    return self;                                                             // 0x00474c03
}

// 0x00479c40  View_ctor(this): initWidgetTable 0x00495d30 on this (call 0x00479c5e), HotList::ctor on this + 0xbc (call
// 0x00479c6f), then the field stores below: the vtable 0x004ba8b4 at +0 (0x00479c9c), +0x134 = the vtable 0x004baa10
// (0x00479c74), +0xb0 = this (0x00479ca2), constants 0 / 1 / 2, and copies of the globals 0x0083ab64 / 0x0083ab68 /
// 0x0083ab6c / 0x0083ab70 and the dwords 0x004e42c8-0x004e42e8 (0x00479d31-0x00479dc1). Returns this (`mov eax, esi`
// 0x00479ef5).
CtorFn_t ViewCtor_orig;
void* __fastcall ViewCtor_re(void* self, void*) {
    kViewBaseCtor(self, 0);                                 // 0x00479c5e
    kHotListCtor(static_cast<char*>(self) + 0xbc, 0);       // 0x00479c6f
    At<unsigned>(self, 0x134) = 0x4baa10;                   // 0x00479c74
    At<unsigned>(self, 0x138) = 0;                          // 0x00479c7e
    At<unsigned>(self, 0x13c) = 0;                          // 0x00479c84
    At<unsigned>(self, 0x140) = 0;                          // 0x00479c8a
    At<unsigned>(self, 0x144) = 0;                          // 0x00479c90
    At<unsigned>(self, 0x148) = 0;                          // 0x00479c96
    At<unsigned>(self, 0x0) = 0x4ba8b4;                     // 0x00479c9c
    At<unsigned>(self, 0xb0) = reinterpret_cast<unsigned>(self);// 0x00479ca2
    At<unsigned>(self, 0x228) = 0;                          // 0x00479ca8
    At<unsigned>(self, 0x224) = 0;                          // 0x00479cae
    At<unsigned>(self, 0x22c) = 0;                          // 0x00479cb4
    At<unsigned>(self, 0xa0) = 0;                           // 0x00479cba
    At<unsigned>(self, 0xa4) = 0;                           // 0x00479cc0
    At<unsigned>(self, 0xa8) = 0;                           // 0x00479cc6
    At<unsigned>(self, 0xac) = 0;                           // 0x00479ccc
    At<unsigned>(self, 0xb4) = 0;                           // 0x00479cd2
    At<unsigned>(self, 0xb8) = 0;                           // 0x00479cd8
    At<unsigned>(self, 0x1a4) = 0;                          // 0x00479cde
    At<unsigned>(self, 0x1a8) = 0;                          // 0x00479ce4
    At<unsigned>(self, 0x208) = 0;                          // 0x00479cea
    At<unsigned>(self, 0x20c) = 0;                          // 0x00479cf0
    At<unsigned>(self, 0x210) = 0;                          // 0x00479cf6
    At<unsigned>(self, 0x214) = 0;                          // 0x00479cfc
    At<unsigned>(self, 0x218) = 0;                          // 0x00479d02
    At<unsigned>(self, 0x204) = 0;                          // 0x00479d08
    At<unsigned>(self, 0x130) = 0;                          // 0x00479d0e
    At<unsigned>(self, 0x15c) = 0;                          // 0x00479d14
    At<unsigned>(self, 0x21c) = 0;                          // 0x00479d1a
    At<unsigned>(self, 0x19c) = 0;                          // 0x00479d25
    At<unsigned>(self, 0x1a0) = 1;                          // 0x00479d2b
    At<unsigned>(self, 0x168) = G<unsigned>(0x0083ab64);    // 0x00479d37
    At<unsigned>(self, 0x16c) = G<unsigned>(0x004e42c8);    // 0x00479d43
    At<unsigned>(self, 0x180) = G<unsigned>(0x004e42cc);    // 0x00479d4f
    At<unsigned>(self, 0x170) = G<unsigned>(0x0083ab6c);    // 0x00479d5b
    At<unsigned>(self, 0x174) = G<unsigned>(0x004e42d0);    // 0x00479d67
    At<unsigned>(self, 0x178) = G<unsigned>(0x004e42d4);    // 0x00479d73
    At<unsigned>(self, 0x17c) = G<unsigned>(0x004e42d8);    // 0x00479d7f
    At<unsigned>(self, 0x184) = G<unsigned>(0x004e42dc);    // 0x00479d8b
    At<unsigned>(self, 0x188) = G<unsigned>(0x004e42e0);    // 0x00479d97
    At<unsigned>(self, 0x18c) = G<unsigned>(0x004e42e4);    // 0x00479da3
    At<unsigned>(self, 0x190) = G<unsigned>(0x0083ab70);    // 0x00479daf
    At<unsigned>(self, 0x194) = G<unsigned>(0x004e42e8);    // 0x00479dbb
    At<unsigned>(self, 0x164) = G<unsigned>(0x0083ab68);    // 0x00479dc7
    At<unsigned>(self, 0x14c) = 0;                          // 0x00479dcd
    At<unsigned>(self, 0x198) = 0;                          // 0x00479dd3
    At<unsigned>(self, 0x150) = 0;                          // 0x00479dd9
    At<unsigned>(self, 0x154) = 0;                          // 0x00479ddf
    At<unsigned>(self, 0x158) = 0;                          // 0x00479de5
    At<unsigned>(self, 0x26c) = 0;                          // 0x00479deb
    At<unsigned>(self, 0x270) = 0;                          // 0x00479df1
    At<unsigned>(self, 0x160) = 0;                          // 0x00479df7
    At<unsigned>(self, 0x1cc) = 0;                          // 0x00479dfd
    At<unsigned>(self, 0x1d0) = 0;                          // 0x00479e03
    At<unsigned>(self, 0x1d4) = 0;                          // 0x00479e09
    At<unsigned>(self, 0x1d8) = 0;                          // 0x00479e0f
    At<unsigned>(self, 0x1ac) = 0;                          // 0x00479e15
    At<unsigned>(self, 0x1b0) = 0;                          // 0x00479e1b
    At<unsigned>(self, 0x1b4) = 0;                          // 0x00479e25
    At<unsigned>(self, 0x1b8) = 0;                          // 0x00479e2b
    At<unsigned>(self, 0x1bc) = 0;                          // 0x00479e31
    At<unsigned>(self, 0x1c0) = 0;                          // 0x00479e37
    At<unsigned>(self, 0x1c4) = 0;                          // 0x00479e3d
    At<unsigned>(self, 0x1c8) = 0;                          // 0x00479e43
    At<unsigned>(self, 0x11c) = 0;                          // 0x00479e49
    At<unsigned>(self, 0x128) = 0;                          // 0x00479e4f
    At<unsigned>(self, 0x124) = 0;                          // 0x00479e55
    At<unsigned>(self, 0x120) = 0;                          // 0x00479e5b
    At<unsigned>(self, 0x1ec) = 0;                          // 0x00479e61
    At<unsigned>(self, 0x1f0) = 0;                          // 0x00479e67
    At<unsigned>(self, 0x9c) = 0;                           // 0x00479e6d
    At<unsigned>(self, 0x1f4) = 1;                          // 0x00479e73
    At<unsigned>(self, 0x1f8) = 1;                          // 0x00479e79
    At<unsigned>(self, 0x1fc) = 1;                          // 0x00479e7f
    At<unsigned>(self, 0x200) = 1;                          // 0x00479e85
    At<unsigned>(self, 0x230) = 0;                          // 0x00479e8b
    At<unsigned>(self, 0x234) = 0;                          // 0x00479e91
    At<unsigned>(self, 0x238) = 0;                          // 0x00479e97
    At<unsigned>(self, 0x23c) = 0;                          // 0x00479e9d
    At<unsigned>(self, 0x240) = 0;                          // 0x00479ea3
    At<unsigned>(self, 0x244) = 0;                          // 0x00479ea9
    At<unsigned>(self, 0x248) = 0;                          // 0x00479eaf
    At<unsigned>(self, 0x24c) = 0;                          // 0x00479eb5
    At<unsigned>(self, 0x250) = 0;                          // 0x00479ebb
    At<unsigned>(self, 0x254) = 0;                          // 0x00479ec1
    At<unsigned>(self, 0x258) = 0;                          // 0x00479ec7
    At<unsigned>(self, 0x25c) = 0;                          // 0x00479ecd
    At<unsigned>(self, 0x260) = 0;                          // 0x00479ed3
    At<unsigned>(self, 0x264) = 0;                          // 0x00479ed9
    At<unsigned>(self, 0x268) = 0;                          // 0x00479edf
    At<unsigned>(self, 0x12c) = 0;                          // 0x00479ee5
    At<unsigned>(self, 0x220) = 0x2;                        // 0x00479eeb
    return self;                                                             // 0x00479ef5
}

// 0x004804a0  View4804a0::ctor(this): View_ctor 0x00479c40 on this (call 0x004804bf), Image_ctor 0x00474ae0 on this + 0x274
// (`lea ebx, [esi + 0x274]` 0x004804c4, call 0x004804d2), the vtables 0x004baa30 at +0 (0x004804db) and 0x004baa1c at +0x274
// (0x004804e1), zeros, and +0x56c = the global 0x0083ac18 (0x00480559 / 0x0048055f). Returns this (`mov eax, esi`
// 0x00480565).
CtorFn_t View4804a0Ctor_orig;
void* __fastcall View4804a0Ctor_re(void* self, void*) {
    kViewCtor(self, 0);                                     // 0x004804bf
    kImageCtor(static_cast<char*>(self) + 0x274, 0);        // 0x004804d2
    At<unsigned>(self, 0x0) = 0x4baa30;                     // 0x004804db
    At<unsigned>(self, 0x274) = 0x4baa1c;                   // 0x004804e1
    At<unsigned>(self, 0x570) = 0;                          // 0x004804e7
    At<unsigned>(self, 0x1a4) = 0;                          // 0x004804ed
    At<unsigned>(self, 0x1a8) = 0;                          // 0x004804f3
    At<unsigned>(self, 0x52c) = 0;                          // 0x004804f9
    At<unsigned>(self, 0x530) = 0;                          // 0x004804ff
    At<unsigned>(self, 0x534) = 0;                          // 0x00480505
    At<unsigned>(self, 0x538) = 0;                          // 0x0048050b
    At<unsigned>(self, 0x53c) = 0;                          // 0x00480511
    At<unsigned>(self, 0x540) = 0;                          // 0x00480517
    At<unsigned>(self, 0x544) = 0;                          // 0x0048051d
    At<unsigned>(self, 0x548) = 0;                          // 0x00480523
    At<unsigned>(self, 0x54c) = 0;                          // 0x00480529
    At<unsigned>(self, 0x550) = 0;                          // 0x0048052f
    At<unsigned>(self, 0x554) = 0;                          // 0x00480535
    At<unsigned>(self, 0x558) = 0;                          // 0x0048053b
    At<unsigned>(self, 0x55c) = 0;                          // 0x00480541
    At<unsigned>(self, 0x560) = 0;                          // 0x00480547
    At<unsigned>(self, 0x564) = 0;                          // 0x0048054d
    At<unsigned>(self, 0x568) = 0;                          // 0x00480553
    At<unsigned>(self, 0x56c) = G<unsigned>(0x0083ac18);    // 0x0048055f
    return self;                                                             // 0x00480565
}

// 0x00486070  EditBox::ctor(this): View4804a0::ctor 0x004804a0 on this (call 0x0048608d), Timer::ctor 0x00486c90 on this +
// 0x5ac (`lea ecx, [esi + 0x5ac]` 0x00486092, call 0x004860a0), the vtables 0x004ba4a8 at +0 (0x004860a7) and 0x004ba494 at
// +0x274 (0x004860ad), then EditBox::resetFields 0x00485ff0 (call 0x004860b7). Returns this (`mov eax, esi` 0x004860c0).
CtorFn_t EditBoxCtor_orig;
void* __fastcall EditBoxCtor_re(void* self, void*) {
    kView4804a0Ctor(self, 0);                               // 0x0048608d
    kTimerCtor(static_cast<char*>(self) + 0x5ac, 0);        // 0x004860a0
    At<unsigned>(self, 0x0) = 0x4ba4a8;                     // 0x004860a7
    At<unsigned>(self, 0x274) = 0x4ba494;                   // 0x004860ad
    kEditBoxReset(self, 0);                                 // 0x004860b7
    return self;                                                             // 0x004860c0
}

// 0x00485ff0  EditBox::resetFields(this): +0x574, +0x580, +0x57c, +0x578, +0x5a8, +0x5a4 = 0 (0x00485ff2-0x00486010), then
// copies of the dwords 0x004e43f0 / 0x004e440c / 0x004e4408 / 0x004e43f8 and the globals 0x0083afd8 / 0x0083afdc and the
// dword 0x004e43f4 into +0x584 / +0x588 / +0x58c / +0x59c / +0x590 / +0x594 / +0x598 (0x00486016-0x00486060). eax at the
// `ret` (0x00486066) is the last value loaded, the dword 0x004e43f4.
ResetFn_t EditBoxResetFields_orig;
unsigned __fastcall EditBoxResetFields_re(void* self, void*) {
    At<unsigned>(self, 0x574) = 0;                          // 0x00485ff2
    At<unsigned>(self, 0x580) = 0;                          // 0x00485ff8
    At<unsigned>(self, 0x57c) = 0;                          // 0x00485ffe
    At<unsigned>(self, 0x578) = 0;                          // 0x00486004
    At<unsigned>(self, 0x5a8) = 0;                          // 0x0048600a
    At<unsigned>(self, 0x5a4) = 0;                          // 0x00486010
    At<unsigned>(self, 0x584) = G<unsigned>(0x004e43f0);    // 0x0048601b
    At<unsigned>(self, 0x588) = G<unsigned>(0x004e440c);    // 0x00486027
    At<unsigned>(self, 0x58c) = G<unsigned>(0x004e4408);    // 0x00486032
    At<unsigned>(self, 0x59c) = G<unsigned>(0x004e43f8);    // 0x0048603e
    At<unsigned>(self, 0x590) = G<unsigned>(0x0083afd8);    // 0x00486049
    At<unsigned>(self, 0x594) = G<unsigned>(0x0083afdc);    // 0x00486055
    At<unsigned>(self, 0x598) = G<unsigned>(0x004e43f4);    // 0x00486060
    return G<unsigned>(0x004e43f4);                                          // 0x00486066
}

// 0x0049c020  ctorGlobalObj49c020(this): View4804a0::ctor 0x004804a0 on this (call 0x0049c03d), the constructor 0x004837f0 on
// this + 0x574 (`lea ecx, [esi + 0x574]` 0x0049c042, call 0x0049c050), Timer::ctor on this + 0x598 (call 0x0049c060), the
// vtables 0x004bbcf4 at +0 (0x0049c069) and 0x004bbce0 at +0x274 (0x0049c06f), +0x594 = 0 (0x0049c079). Returns this
// (`mov eax, esi` 0x0049c083).
CtorFn_t CtorGlobalObj49c020_orig;
void* __fastcall CtorGlobalObj49c020_re(void* self, void*) {
    kView4804a0Ctor(self, 0);                               // 0x0049c03d
    kCtor4837f0(static_cast<char*>(self) + 0x574, 0);       // 0x0049c050
    kTimerCtor(static_cast<char*>(self) + 0x598, 0);        // 0x0049c060
    At<unsigned>(self, 0x0) = 0x4bbcf4;                     // 0x0049c069
    At<unsigned>(self, 0x274) = 0x4bbce0;                   // 0x0049c06f
    At<unsigned>(self, 0x594) = 0x0;                        // 0x0049c079
    return self;                                                             // 0x0049c083
}

// =====================================================================================================================
// Image teardown and the display-backed Image::create
// =====================================================================================================================

const Ptr0_t kImageReleaseAll = reinterpret_cast<Ptr0_t>(0x00474cb0);
typedef int(__fastcall* DtorFn_t)(void* ecx, void* edx);
const DtorFn_t kHotListDtor = reinterpret_cast<DtorFn_t>(0x004928d0);
const DtorFn_t kImageDtor = reinterpret_cast<DtorFn_t>(0x00474c40);

// Image_dtor 0x00474c40 is owned by batch c3di (hooked there); resetViewState47cfd0 below calls it through its address.

// 0x0047cfd0  resetViewState47cfd0(): an Image of 0x2b8 bytes on the stack (`sub esp, 0x2b8` 0x0047cfd0) is built with
// Image_ctor 0x00474ae0 (call 0x0047cfda), the globals 0x0083a4e8 / 0x0083a4ec / 0x0083a4f0 / 0x0083a4f4 are cleared
// (0x0047cfe1-0x0047cff0), the empty function 0x004a0890 runs (call 0x0047cff5) and the Image is destroyed with Image_dtor
// 0x00474c40 (call 0x0047cffe). The result is 0 (`xor eax, eax` 0x0047d003).
typedef int(__cdecl* ResetViewState_t)();
ResetViewState_t ResetViewState_orig;
int __cdecl ResetViewState_re() {
    alignas(4) char image[0x2b8];                                            // 0x0047cfd0
    kImageCtor(image, 0);                                                    // 0x0047cfda
    G<unsigned>(0x0083a4e8) = 0;                                             // 0x0047cfe1
    G<unsigned>(0x0083a4ec) = 0;                                             // 0x0047cfe6
    G<unsigned>(0x0083a4f0) = 0;                                             // 0x0047cfeb
    G<unsigned>(0x0083a4f4) = 0;                                             // 0x0047cff0
    reinterpret_cast<void(__cdecl*)()>(0x004a0890)();                        // 0x0047cff5
    kImageDtor(image, 0);                                                    // 0x0047cffe
    return 0;                                                                // 0x0047d003
}

// 0x00474dd0  Surface_createFull(this, a1, a2, a3, a4, a5, a6): Image_releaseAll 0x00474cb0 on this (call 0x00474dd4). The
// size field +8 takes a4 when it is non-zero (0x00474df1), else stays when it is non-zero, else becomes 1 (0x00474de8). The
// display's virtual slot +0x7c (`mov ecx, [0x0083ad50]` 0x00474df4, `mov eax, [ecx]` 0x00474dff, call [eax + 0x7c]
// 0x00474e01) runs with the Image and +8 as arguments (pushes 0x00474dfd / 0x00474dfe) and its result goes to +4
// (0x00474e07). That object's virtual slot +4 (`mov edx, [eax]` 0x00474e0f, call [edx + 4] 0x00474e1e) runs with this = the
// object and (a1, a2, a3, +8) (pushes 0x00474e0a-0x00474e1b). A non-zero result is returned after Image_releaseAll again
// (call 0x00474e29, `mov eax, edi` 0x00474e2e). A zero result runs List_reset 0x00478a20 (call 0x00474e35) and Widget_reset
// 0x00478a70 (call 0x00474e3c), then setDrawTarget 0x004762d0 on this with ([0x0083ad44], 0, 0, 0) (pushes 0x00474e47-
// 0x00474e4d, call 0x00474e50), stores a5 into +0x20 (0x00474e5a) and returns 0 (0x00474e5d). a6 (the sixth stack word,
// popped by `ret 0x18` 0x00474e60) is not read.
typedef int(__fastcall* CreateFull_t)(void* ecx, void* edx, int a1, int a2, int a3, int a4, int a5, int a6);
typedef void*(__fastcall* DispSlot7c_t)(void* ecx, void* edx, void* image, int size);
typedef int(__fastcall* ObjSlot1_t)(void* ecx, void* edx, int a1, int a2, int a3, int size);
typedef int(__fastcall* SetDrawTarget_t)(void* ecx, void* edx, int a, int b, int c, int d);
CreateFull_t SurfaceCreateFull_orig;
int __fastcall SurfaceCreateFull_re(void* self, void*, int a1, int a2, int a3, int a4, int a5, int) {
    kImageReleaseAll(self, 0);                                               // 0x00474dd4
    if (a4 != 0) At<int>(self, 8) = a4;                                      // 0x00474ddd / 0x00474ddf / 0x00474df1
    else if (At<int>(self, 8) == 0) At<int>(self, 8) = 1;                    // 0x00474de1 / 0x00474de6 / 0x00474de8
    void* disp = G<void*>(0x0083ad50);                                       // 0x00474df4
    void** dvt = *reinterpret_cast<void***>(disp);                           // 0x00474dff
    void* obj = reinterpret_cast<DispSlot7c_t>(dvt[0x7c / 4])(disp, 0, self, At<int>(self, 8));   // 0x00474e01
    At<void*>(self, 4) = obj;                                                // 0x00474e07
    void** ovt = *reinterpret_cast<void***>(obj);                            // 0x00474e0f
    int r = reinterpret_cast<ObjSlot1_t>(ovt[1])(obj, 0, a1, a2, a3, At<int>(self, 8));   // 0x00474e1e
    if (r != 0) {                                                            // 0x00474e25 / 0x00474e27
        kImageReleaseAll(self, 0);                                           // 0x00474e29
        return r;                                                            // 0x00474e2e
    }
    reinterpret_cast<Ptr0_t>(0x00478a20)(self, 0);                           // 0x00474e35
    reinterpret_cast<Ptr0_t>(0x00478a70)(self, 0);                           // 0x00474e3c
    reinterpret_cast<SetDrawTarget_t>(0x004762d0)(self, 0, G<int>(0x0083ad44), 0, 0, 0);   // 0x00474e50
    At<int>(self, 0x20) = a5;                                                // 0x00474e5a
    return 0;                                                                // 0x00474e5d
}

// =====================================================================================================================
// Sound library loader (the 11-entry function table at 0x0083af6c, the module handle 0x0083afc0, the flag 0x0083afc4)
// and the Bink frame step
// =====================================================================================================================

typedef void*(__stdcall* LoadLibraryA_t)(const char* name);
typedef void*(__stdcall* GetProcAddress_t)(void* module, unsigned ordinal);
typedef int(__stdcall* Stdcall2_t)(void* a, void* b);
typedef int(__stdcall* Stdcall3_t)(void* a, void* b, void* c);
typedef int(__stdcall* Stdcall7_t)(void* a, void* b, void* c, void* d, void* e, void* f, void* g);
typedef int(__cdecl* ShowMessageBox_t)(const char* text, const char* caption);
const unsigned kIatGetProcAddress = 0x004ba0a4;
const unsigned kIatLoadLibraryA = 0x004ba0a8;
const unsigned kIatFreeLibrary = 0x004ba0ac;
const unsigned kIatBinkDoFrame = 0x004ba228;           // binkw32.dll _BinkDoFrame@4
const unsigned kIatBinkBufferLock = 0x004ba22c;        // _BinkBufferLock@4
const unsigned kIatBinkCopyToBuffer = 0x004ba230;      // _BinkCopyToBuffer@28
const unsigned kIatBinkBufferUnlock = 0x004ba234;      // _BinkBufferUnlock@4
const unsigned kIatBinkGetRects = 0x004ba238;          // _BinkGetRects@8
const unsigned kIatBinkBufferBlit = 0x004ba23c;        // _BinkBufferBlit@12
const unsigned kIatBinkNextFrame = 0x004ba240;         // _BinkNextFrame@4
const Cdecl0_t kCallSoundHook94 = reinterpret_cast<Cdecl0_t>(0x004840c0);
const Cdecl0_t kFreeSoundLib = reinterpret_cast<Cdecl0_t>(0x00484060);
const Cdecl0_t kClearSoundPtrs = reinterpret_cast<Cdecl0_t>(0x00484130);

// clearSoundPtrs 0x00484130 is hooked by batch c3dm (C3 since round 26 part 1); freeSoundLib calls it through kClearSoundPtrs.

// 0x00484060  freeSoundLib(): the flag 0x0083afc4 = 0 (0x00484065); a loaded module handle in 0x0083afc0 (`mov eax,
// [0x0083afc0]` 0x00484060, `test eax, eax; je` 0x0048406f / 0x00484071) goes to FreeLibrary [0x4ba0ac] (push 0x00484073, call
// 0x00484074) and the handle is cleared (0x0048407a). clearSoundPtrs 0x00484130 runs (call 0x00484084). The result is 0
// (`xor eax, eax` 0x00484089).
Cdecl0_t FreeSoundLib_orig;
int __cdecl FreeSoundLib_re() {
    int h = G<int>(0x0083afc0);                                              // 0x00484060
    G<unsigned>(0x0083afc4) = 0;                                             // 0x00484065
    if (h != 0) {                                                            // 0x0048406f / 0x00484071
        Imp<Stdcall1_t>(kIatFreeLibrary)(reinterpret_cast<void*>(h));        // 0x00484074
        G<unsigned>(0x0083afc0) = 0;                                         // 0x0048407a
    }
    kClearSoundPtrs();                                                       // 0x00484084
    return 0;                                                                // 0x00484089
}

// 0x00484090  callSoundHook74(a, b): the module handle 0x0083afc0 at zero returns 4 (0x00484099). Otherwise the function
// pointer in 0x0083af74 (table entry 1) is called with (a, b) as pushed (pushes 0x004840a7 / 0x004840a8, call [0x83af74]
// 0x004840a9, caller-cleaned `add esp, 8` 0x004840af) and its result is returned.
typedef int(__cdecl* Hook2_t)(int a, int b);
Hook2_t CallSoundHook74_orig;
int __cdecl CallSoundHook74_re(int a, int b) {
    if (G<int>(0x0083afc0) == 0) return 4;                                   // 0x00484095 / 0x00484097 / 0x00484099
    return G<Hook2_t>(0x0083af74)(a, b);                                     // 0x004840a9
}

// 0x00483fc0  soundVersionCheck(): a loaded handle (0x0083afc0 non-zero, `test eax, eax; jne` 0x00483fc8 / 0x00483fca) returns
// 0 (0x0048403f-0x00484043). Otherwise LoadLibraryA [0x4ba0a8] of the string 0x004e43bc (push 0x00483fcc, call 0x00483fd1)
// is stored in 0x0083afc0 (0x00483fd9); NULL returns 1 (0x00483fe2). With a module, GetProcAddress (the function pointer
// read from [0x4ba0a4] into ebx, 0x00483fe9, called 0x00484008) fills the table 0x0083af6c .. 0x0083af98 (esi, 11 entries,
// `cmp esi, 0x83af98; jl` 0x00484014 / 0x0048401a) with the ordinals 1 .. 11 (`lea ecx, [edi + 1]; and ecx, 0xffff`
// 0x00483ffd / 0x00484000); a NULL entry (stored first, 0x0048400c; `je` 0x0048400e) runs freeSoundLib 0x00484060 (call
// 0x00484045), clears the flag 0x0083afc4 (0x0048404c) and returns 1 (0x00484056). A full table calls callSoundHook94
// 0x004840c0 (call 0x0048401c): a result whose bits 8-15 are not 0x0c (`and eax, 0xff00; cmp eax, 0xc00` 0x00484021 /
// 0x00484026) shows showMessageBox 0x00485590 (text 0x004babd0, caption 0x004bac50: pushes 0x0048402d / 0x00484032, call
// 0x00484037); the result is 0 either way (0x00484041).
Cdecl0_t SoundVersionCheck_orig;
int __cdecl SoundVersionCheck_re() {
    if (G<int>(0x0083afc0) != 0) return 0;                                   // 0x00483fc0 / 0x00483fc8
    void* h = Imp<LoadLibraryA_t>(kIatLoadLibraryA)(reinterpret_cast<const char*>(0x004e43bc));   // 0x00483fd1
    G<void*>(0x0083afc0) = h;                                                // 0x00483fd9
    if (h == 0) return 1;                                                    // 0x00483fde
    GetProcAddress_t getProc = Imp<GetProcAddress_t>(kIatGetProcAddress);    // 0x00483fe9
    unsigned i = 0;
    unsigned slot = 0x0083af6c;                                              // 0x00483ff1
    for (;;) {
        void* fn = getProc(G<void*>(0x0083afc0), (i + 1) & 0xffff);          // 0x00484008
        G<void*>(slot) = fn;                                                 // 0x0048400c
        if (fn == 0) {                                                       // 0x0048400a / 0x0048400e
            kFreeSoundLib();                                                 // 0x00484045
            G<unsigned>(0x0083afc4) = 0;                                     // 0x0048404c
            return 1;                                                        // 0x00484056
        }
        slot += 4;                                                           // 0x00484010
        i++;                                                                 // 0x00484013
        if (!(slot < 0x0083af98)) break;                                     // 0x00484014 / 0x0048401a
    }
    int v = kCallSoundHook94();                                              // 0x0048401c
    if ((v & 0xff00) != 0xc00)                                               // 0x00484021 / 0x00484026 / 0x0048402b
        reinterpret_cast<ShowMessageBox_t>(0x00485590)(reinterpret_cast<const char*>(0x004babd0),
                                                       reinterpret_cast<const char*>(0x004bac50));   // 0x00484037
    return 0;                                                                // 0x00484041
}

// 0x00487180  BinkPlayer::frame(this): _BinkDoFrame [0x4ba228] on the Bink handle +4 (push 0x00487186, call 0x00487187), then
// _BinkBufferLock [0x4ba22c] on the buffer +8 (0x00487190 / 0x00487191); a non-zero result (`test eax, eax; je` 0x00487197 /
// 0x00487199) runs _BinkCopyToBuffer [0x4ba230] with (bink, buf+0x14, buf+0x18, buf+4, 0, 0, buf+0x10) (pushes 0x0048719e-
// 0x004871b5, call 0x004871b6) and _BinkBufferUnlock [0x4ba234] (0x004871bc-0x004871c0). _BinkGetRects [0x4ba238] gets
// (bink, buf+0x10) (0x004871c6-0x004871d1) and _BinkBufferBlit [0x4ba23c] (buf, bink+0x34, rects) (0x004871d7-0x004871e3).
// Bink +0xc against +8 (`cmp edx, eax; jne` 0x004871f2 / 0x004871f4): equal returns false (`xor al, al` 0x004871f6); else
// _BinkNextFrame [0x4ba240] on the Bink handle (call 0x004871fb) and true (`mov al, 1` 0x00487201). Only al is defined.
typedef unsigned char(__fastcall* BinkFrame_t)(void* ecx, void* edx);
BinkFrame_t BinkPlayerFrame_orig;
unsigned char __fastcall BinkPlayerFrame_re(void* self, void*) {
    Imp<Stdcall1_t>(kIatBinkDoFrame)(At<void*>(self, 4));                    // 0x00487187
    if (Imp<Stdcall1_t>(kIatBinkBufferLock)(At<void*>(self, 8)) != 0) {      // 0x00487191 / 0x00487197
        void* buf = At<void*>(self, 8);                                      // 0x0048719b
        Imp<Stdcall7_t>(kIatBinkCopyToBuffer)(At<void*>(self, 4), At<void*>(buf, 0x14), At<void*>(buf, 0x18),
                                              At<void*>(buf, 4), 0, 0, At<void*>(buf, 0x10));   // 0x004871b6
        Imp<Stdcall1_t>(kIatBinkBufferUnlock)(At<void*>(self, 8));           // 0x004871c0
    }
    void* buf = At<void*>(self, 8);                                          // 0x004871c6
    void* bink = At<void*>(self, 4);                                         // 0x004871c9
    int rects = Imp<Stdcall2_t>(kIatBinkGetRects)(bink, At<void*>(buf, 0x10));   // 0x004871d1
    Imp<Stdcall3_t>(kIatBinkBufferBlit)(At<void*>(self, 8), static_cast<char*>(bink) + 0x34,
                                        reinterpret_cast<void*>(rects));     // 0x004871e3
    bink = At<void*>(self, 4);                                               // 0x004871e9
    if (At<int>(bink, 0xc) == At<int>(bink, 8)) return 0;                    // 0x004871f2 / 0x004871f4
    Imp<Stdcall1_t>(kIatBinkNextFrame)(bink);                                // 0x004871fb
    return 1;                                                                // 0x00487201
}

// 0x004823c0  setNewHeap(size, heap): a heap object (`mov esi, [esp + 0xc]` 0x004823c1, `test esi, esi; je` 0x004823c5 /
// 0x004823c7) gets heapAlloc 0x00474860 (this = heap, the size pushed 0x004823cf, call 0x004823d0); a result of 0 shows
// MessageBoxA [0x4ba1f4] (text 0x004c1434, caption 0x004c1458: pushes 0x004823e1-0x004823ed, call 0x004823ef) and exit(3)
// (0x004a5108, call 0x004823f7). Otherwise the global 0x00839650 = the heap (0x004823d9) and the block is returned. Without a
// heap malloc 0x004a504f gets the size (call 0x00482401); NULL returns 0 (0x0048240d), else 0x00839650 = 0 (0x0048240f) and
// the block is returned.
typedef void*(__cdecl* SetNewHeap_t)(unsigned size, void* heap);
typedef void*(__fastcall* HeapAlloc_t)(void* ecx, void* edx, unsigned size);
typedef void(__cdecl* Exit_t)(int code);
SetNewHeap_t SetNewHeap_orig;
void* __cdecl SetNewHeap_re(unsigned size, void* heap) {
    if (heap != 0) {                                                         // 0x004823c1 / 0x004823c5 / 0x004823c7
        void* r = reinterpret_cast<HeapAlloc_t>(0x00474860)(heap, 0, size);  // 0x004823d0
        if (r == 0) {                                                        // 0x004823d5 / 0x004823d7
            Imp<MessageBoxA_t>(kIatMessageBoxA)(0, reinterpret_cast<const char*>(0x004c1434),
                                                reinterpret_cast<const char*>(0x004c1458), 0);   // 0x004823ef
            reinterpret_cast<Exit_t>(0x004a5108)(3);                         // 0x004823f7
        }
        G<void*>(0x00839650) = heap;                                         // 0x004823d9
        return r;                                                            // 0x004823e0
    }
    void* p = kMalloc(size);                                                 // 0x00482401
    if (p == 0) return 0;                                                    // 0x00482409 / 0x0048240b / 0x0048240d
    G<unsigned>(0x00839650) = 0;                                             // 0x0048240f
    return p;                                                                // 0x0048241a
}

// =====================================================================================================================
// Tooltip timers: HotList::showTip / tipTimerCallback and Button::onTimer / onTipTimer. Tooltip::show 0x00480220 draws
// through the live display and is replaced by the fixture's recording stub on the A/B thread (call arguments and the rect
// it receives are compared); View::toParent 0x0047b170, Window::visible 0x004801f0 and the Timer family run as the
// originals.
// =====================================================================================================================

typedef int(__fastcall* TooltipShow_t)(void* ecx, void* edx, const char* text, int* rect);
typedef void(__fastcall* ToParent_t)(void* ecx, void* edx, int* x, int* y);
typedef int(__fastcall* Visible_t)(void* ecx, void* edx);
typedef int(__fastcall* TimerStop_t)(void* ecx, void* edx);
typedef int(__fastcall* TimerInitStartMm_t)(void* ecx, void* edx, int cb, int a2, int a3, int a4, int a5);
typedef int(__fastcall* Slot78_t)(void* ecx, void* edx, int a, int b);
const TooltipShow_t kTooltipShow = reinterpret_cast<TooltipShow_t>(0x00480220);
const ToParent_t kToParent = reinterpret_cast<ToParent_t>(0x0047b170);
const Visible_t kWindowVisible = reinterpret_cast<Visible_t>(0x004801f0);
const TimerStop_t kTimerStop = reinterpret_cast<TimerStop_t>(0x00486ec0);
const TimerInitStartMm_t kTimerInitStartMmPeriodic = reinterpret_cast<TimerInitStartMm_t>(0x00486d60);

// 0x00492bd0  HotList::showTip(this, index): Timer::stop 0x00486ec0 on this + 0x20 (call 0x00492bda). With no owner window
// at +0xc (`mov ecx, [esi + 0xc]; test ecx, ecx; je` 0x00492bdf / 0x00492be2 / 0x00492be4) the result is Timer::stop's.
// The entry index * 0x20 (`shl eax, 5` 0x00492bf1) into the array +0x50 (0x00492bee) gives the entry; a zero text +0x1c
// (0x00492bf7 / 0x00492bfb / 0x00492bfd) returns 0 (the text just loaded). Otherwise the entry's rect +4 .. +0x10 is copied
// to a local rect (0x00492c03-0x00492c23) and the point (0, 0) (0x00492c2c / 0x00492c34) is converted by View::toParent
// 0x0047b170 on the owner (call 0x00492c3c); the rect is moved by that point (0x00492c41-0x00492c72). The tooltip globals
// are set: 0x0083aac8 from this + 0x10, or from 0x0083ad44 when that is zero (0x00492c69-0x00492c86), 0x0083aad8 from this +
// 0x18 (0x00492c8b / 0x00492c92) and 0x004e42c4 from this + 0x1c (0x00492c98 / 0x00492c9b). Tooltip::show 0x00480220 runs on
// the owner with (the entry's text, the rect) (pushes 0x00492ca3 / 0x00492ca7, call 0x00492ca8) and its result is returned.
typedef int(__fastcall* ShowTip_t)(void* ecx, void* edx, int index);
ShowTip_t HotListShowTip_orig;
int __fastcall HotListShowTip_re(void* self, void*, int index) {
    int r = kTimerStop(static_cast<char*>(self) + 0x20, 0);                  // 0x00492bda
    void* owner = At<void*>(self, 0xc);                                      // 0x00492bdf
    if (owner == 0) return r;                                                // 0x00492be4
    char* entry = static_cast<char*>(At<void*>(self, 0x50)) + 32 * index;    // 0x00492bee-0x00492bf4
    char* text = At<char*>(entry, 0x1c);                                     // 0x00492bf7
    if (text == 0) return 0;                                                 // 0x00492bfd
    int rect[4] = {At<int>(entry, 4), At<int>(entry, 8), At<int>(entry, 0xc), At<int>(entry, 0x10)};   // 0x00492c06-0x00492c23
    int x = 0, y = 0;                                                        // 0x00492c2c / 0x00492c34
    kToParent(owner, 0, &x, &y);                                             // 0x00492c3c
    rect[0] += x; rect[2] += x;                                              // 0x00492c4d / 0x00492c4f
    rect[1] += y; rect[3] += y;                                              // 0x00492c65 / 0x00492c67
    int v = At<int>(self, 0x10);                                             // 0x00492c69
    if (v == 0) G<int>(0x0083aac8) = G<int>(0x0083ad44);                     // 0x00492c70 / 0x00492c76 / 0x00492c78-0x00492c7e
    else G<int>(0x0083aac8) = v;                                             // 0x00492c86
    G<int>(0x0083aad8) = At<int>(self, 0x18);                                // 0x00492c92
    G<int>(0x004e42c4) = At<int>(self, 0x1c);                                // 0x00492c9b
    return kTooltipShow(owner, 0, text, rect);                               // 0x00492ca8
}

// 0x00492bb0  HotList::tipTimerCallback(index, hotlist): a non-NULL second argument (`mov ecx, [esp + 8]; test ecx, ecx; je`
// 0x00492bb0 / 0x00492bb4 / 0x00492bb6) runs HotList::showTip 0x00492bd0 on it with the first argument (push 0x00492bbc, call
// 0x00492bbd); the plain `ret` (0x00492bc2) leaves cleanup to the caller. eax on the NULL path is the caller's; Timer_fire
// 0x00495cb0 returns the callback's eax to the timer machinery, which ignores it, so the registry declares the function void.
typedef void(__cdecl* TipCallback_t)(int index, void* hotlist);
TipCallback_t HotListTipTimerCallback_orig;
void __cdecl HotListTipTimerCallback_re(int index, void* hotlist) {
    if (hotlist != 0)                                                        // 0x00492bb4 / 0x00492bb6
        reinterpret_cast<ShowTip_t>(0x00492bd0)(hotlist, 0, index);          // 0x00492bbd
}

// 0x00488fd0  Button::onTimer(this, arg): arg 0 (`cmp eax, edi; jne` 0x00488fdd / 0x00488fdf): Timer::stop on this + 0x57c
// (call 0x00488feb); Window::visible 0x004801f0 (call 0x00488ff2) zero returns 0 (0x00488ff9); a zero text pointer +0x5f8
// (0x00488fff) returns visible's result. Otherwise the rect +0x1ac .. +0x1b8 (0x0048900b-0x00489031) is moved so that its
// top-left becomes View::toParent 0x0047b170's conversion of (0, 0) (call 0x00489040): left' = x, top' = y, right' = right +
// (x - left), bottom' = bottom + (y - top) (0x00489045-0x0048907b); Tooltip::show runs on this with (text +0x5f8, rect)
// (call 0x00489087) and its result is returned. arg 1 (`cmp eax, 1` 0x00489094): the interval is +0x5b0 when above 0 (signed
// `jg` 0x004890a1), else +0x5ac (0x004890a3); Timer::initStartMmPeriodic 0x00486d60 on this + 0x5b4 with (the callback
// 0x00488fb0, 2, this, the interval, 5) (pushes 0x004890a9-0x004890af, call 0x004890ba) and its result is returned. Any
// other arg calls the object's virtual slot +0x78 with (0, 0) (`mov edx, [esi]` 0x004890c7, call [edx + 0x78] 0x004890cd) and
// returns its result.
typedef int(__fastcall* ButtonOnTimer_t)(void* ecx, void* edx, int arg);
ButtonOnTimer_t ButtonOnTimer_orig;
int __fastcall ButtonOnTimer_re(void* self, void*, int arg) {
    if (arg == 0) {                                                          // 0x00488fdf
        kTimerStop(static_cast<char*>(self) + 0x57c, 0);                     // 0x00488feb
        int v = kWindowVisible(self, 0);                                     // 0x00488ff2
        if (v == 0) return 0;                                                // 0x00488ff9 / 0x00488ffb
        if (At<void*>(self, 0x5f8) == 0) return v;                           // 0x00488fff / 0x00489005
        int r0 = At<int>(self, 0x1ac), r1 = At<int>(self, 0x1b0), r2 = At<int>(self, 0x1b4), r3 = At<int>(self, 0x1b8);   // 0x00489019-0x00489031
        int a = 0, b = 0;                                                    // 0x00489011 / 0x00489015
        kToParent(self, 0, &a, &b);                                          // 0x00489040
        int dx = a - r0, dy = b - r1;                                        // 0x00489055 / 0x00489057
        int rect[4] = {r0 + dx, r1 + dy, r2 + dx, r3 + dy};                  // 0x00489059-0x0048906f
        return kTooltipShow(self, 0, At<const char*>(self, 0x5f8), rect);    // 0x00489087
    }
    if (arg == 1) {                                                          // 0x00489097
        int interval = At<int>(self, 0x5b0);                                 // 0x00489099
        if (!(interval > 0)) interval = At<int>(self, 0x5ac);                // 0x0048909f / 0x004890a1 / 0x004890a3
        return kTimerInitStartMmPeriodic(static_cast<char*>(self) + 0x5b4, 0, 0x00488fb0, 2, reinterpret_cast<int>(self),
                                         interval, 5);                       // 0x004890ba
    }
    void** vt = *reinterpret_cast<void***>(self);                            // 0x004890c7
    return reinterpret_cast<Slot78_t>(vt[0x78 / 4])(self, 0, 0, 0);          // 0x004890cd
}

// 0x00488e90  Button::onTipTimer(this, a, b): a non-zero text pointer +0x5f8 (`mov eax, [ecx + 0x5f8]; test eax, eax; je`
// 0x00488e90 / 0x00488e96 / 0x00488e98) runs Button::onTimer 0x00488fd0 with 0 (push 0x00488e9a, call 0x00488e9c) and its
// result is returned; otherwise eax is the zero just loaded. `ret 8` (0x00488ea1) drops the two arguments, which are not read.
typedef int(__fastcall* OnTipTimer_t)(void* ecx, void* edx, int a, int b);
OnTipTimer_t ButtonOnTipTimer_orig;
int __fastcall ButtonOnTipTimer_re(void* self, void*, int, int) {
    if (At<void*>(self, 0x5f8) == 0) return 0;                               // 0x00488e96 / 0x00488e98
    return reinterpret_cast<ButtonOnTimer_t>(0x00488fd0)(self, 0, 0);        // 0x00488e9c
}

// =====================================================================================================================
// Button press and mouse-move handlers (object 0x640 bytes: +0x130 the parent window, +0x278 the rect provider, +0x5ac the
// auto-repeat interval, +0x5b4 its Timer, +0x5e8 the button id, +0x614 the pressed state, +0x61c the press time, +0x62c a
// callback). Virtual slots of the parent (+0xc8 / +0xcc / +0xd0), of the objects at parent +0x74 / +0x78 / +0x7c (slot +0x1c)
// and of the button itself (+0x78 / +0x120) land in the fixture's recording machine stubs; timeGetTime, GetAsyncKeyState and
// GetSystemMetrics are the import stand-in's; Tooltip::hide and the Timer family run as the originals.
// =====================================================================================================================

typedef int(__fastcall* Slot0r_t)(void* ecx, void* edx);
typedef int(__fastcall* Slot1r_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* Slot2r_t)(void* ecx, void* edx, int a, int b);
typedef int(__cdecl* InRect_t)(int x, int y, const int* rect);
typedef unsigned(__stdcall* TimeGetTime_t)();
typedef int(__cdecl* Cb1_t)(int a);
const unsigned kIatTimeGetTime = 0x004ba21c;
const TimerInitStartMm_t kTimerInitStartMmOneShot = reinterpret_cast<TimerInitStartMm_t>(0x00486d90);
const InRect_t kInRect = reinterpret_cast<InRect_t>(0x00492610);
const Slot0r_t kIsPrimaryMouseDown = reinterpret_cast<Slot0r_t>(0x0047abc0);
template <class F>
static inline F VSlot(void* obj, unsigned off) { return reinterpret_cast<F>((*reinterpret_cast<void***>(obj))[off / 4]); }

// 0x00488ab0  Button::beginPress(this, a1, a2) (neither argument is read; `ret 8` 0x00488bed): the capture global 0x0083ab30 at
// zero takes this and 0x0083ab34 = 0 (0x00488ab0-0x00488ac2); Tooltip::hide 0x00480360 runs (call 0x00488acc). Pressed state
// +0x614 zero (0x00488ad1 / 0x00488ad9): +0x614 = 1 (0x00488ae1); the object at parent+0x7c, when set, gets its virtual slot
// +0x1c (0x00488aeb-0x00488af4); the parent's slot +0xd0 gets the id +0x5e8 (0x00488b0e); the button's own slot +0x120 runs
// (0x00488b18); the interval +0x5ac of -1 returns -1 (0x00488b24 / 0x00488b27), else Timer::initStartMmOneShot 0x00486d90
// on this + 0x5b4 with (0x00488fb0, 1, this, interval, 5) (pushes 0x00488b2d-0x00488b33, call 0x00488b3e) and its result is
// returned. Pressed state set: timeGetTime [0x4ba21c] goes to +0x61c (call 0x00488b47, store 0x00488b53) and the parent to
// 0x0083ab2c (0x00488b59); when +0x624 is zero (0x00488b5f / 0x00488b67) the object at parent+0x74, when set, gets slot +0x1c
// (0x00488b6f-0x00488b78) and the parent's slot +0xc8 gets the id (the two copies of this call, 0x00488b94 when +0x620 is
// non-zero and 0x00488bab when it is zero, are the same call); a non-zero result with an interval other than -1 starts the
// timer the same way (0x00488b9e-0x00488bd1). The callback +0x62c (0x00488bd6), when set, runs with the id (cdecl, 0x00488be7)
// and its result is returned; with none the result is 0 (the zero just loaded, 0x00488bdc).
typedef int(__fastcall* BeginPress_t)(void* ecx, void* edx, int a1, int a2);
BeginPress_t ButtonBeginPress_orig;
int __fastcall ButtonBeginPress_re(void* self, void*, int, int) {
    if (G<int>(0x0083ab30) == 0) {                                           // 0x00488ab0 / 0x00488ab6 / 0x00488aba
        G<void*>(0x0083ab30) = self;                                         // 0x00488abc
        G<int>(0x0083ab34) = 0;                                              // 0x00488ac2
    }
    reinterpret_cast<Cdecl0_t>(0x00480360)();                                // 0x00488acc
    if (At<int>(self, 0x614) == 0) {                                         // 0x00488ad1 / 0x00488ad7 / 0x00488ad9
        void* parent = At<void*>(self, 0x130);                               // 0x00488adb
        At<int>(self, 0x614) = 1;                                            // 0x00488ae1
        void* o = At<void*>(parent, 0x7c);                                   // 0x00488aeb
        if (o != 0) VSlot<Slot0r_t>(o, 0x1c)(o, 0);                          // 0x00488aee / 0x00488af0 / 0x00488af4
        void* p = At<void*>(self, 0x130);                                    // 0x00488afd
        VSlot<Slot1r_t>(p, 0xd0)(p, 0, At<int>(self, 0x5e8));                // 0x00488b0e
        VSlot<Slot0r_t>(self, 0x120)(self, 0);                               // 0x00488b18
        int iv = At<int>(self, 0x5ac);                                       // 0x00488b1e
        if (iv == -1) return iv;                                             // 0x00488b24 / 0x00488b27
        return kTimerInitStartMmOneShot(static_cast<char*>(self) + 0x5b4, 0, 0x00488fb0, 1, reinterpret_cast<int>(self), iv, 5);   // 0x00488b3e
    }
    At<unsigned>(self, 0x61c) = Imp<TimeGetTime_t>(kIatTimeGetTime)();       // 0x00488b47 / 0x00488b53
    G<void*>(0x0083ab2c) = At<void*>(self, 0x130);                           // 0x00488b59
    if (At<int>(self, 0x624) == 0) {                                         // 0x00488b5f / 0x00488b65 / 0x00488b67
        void* par = At<void*>(self, 0x130);                                  // 0x00488b69
        void* o74 = At<void*>(par, 0x74);                                    // 0x00488b6f
        if (o74 != 0) VSlot<Slot0r_t>(o74, 0x1c)(o74, 0);                    // 0x00488b72 / 0x00488b74 / 0x00488b78
        void* p2 = At<void*>(self, 0x130);                                   // 0x00488b81
        int r;
        if (At<int>(self, 0x620) != 0)                                       // 0x00488b87 / 0x00488b92
            r = VSlot<Slot1r_t>(p2, 0xc8)(p2, 0, At<int>(self, 0x5e8));      // 0x00488b94
        else
            r = VSlot<Slot1r_t>(p2, 0xc8)(p2, 0, At<int>(self, 0x5e8));      // 0x00488bab
        if (r != 0) {                                                        // 0x00488b9a / 0x00488bb1
            int iv = At<int>(self, 0x5ac);                                   // 0x00488b9e / 0x00488bb5
            if (iv != -1)                                                    // 0x00488ba4 / 0x00488bbb
                kTimerInitStartMmOneShot(static_cast<char*>(self) + 0x5b4, 0, 0x00488fb0, 1, reinterpret_cast<int>(self), iv, 5);   // 0x00488bd1
        }
    }
    Cb1_t cb = At<Cb1_t>(self, 0x62c);                                       // 0x00488bd6
    if (cb == 0) return 0;                                                   // 0x00488bdc
    return cb(At<int>(self, 0x5e8));                                         // 0x00488be7
}

// 0x00488cf0  Button::onMouseMove(this, x, y): the rect provider +0x278 (when set, 0x00488cf8 / 0x00488d00) writes the
// button's rect through its virtual slot +0xd0 (0x00488d09); with none the local rect stays uninitialised (stack words), so no
// vector runs that case. The rect is moved to the origin: (0, 0, right - left, bottom - top) (0x00488d1d-0x00488d3e). The
// active button is 0x0083ab40, or 0x0083ab44 when that is zero (0x00488d2f-0x00488d48); another button returns that global
// (0x00488d4d / 0x00488d4f, eax unchanged). The down flag 0x0083ab48 zero returns 0 (0x00488d55-0x00488d5c). InRect 0x00492610
// (x, y, rect) (call 0x00488d71): outside with the pressed state +0x614 equal to 1 (0x00488d7d) leaves the button: Timer::stop on
// this + 0x5b4 (call 0x00488d8c), +0x614 = 0 (0x00488d97), slot +0x1c of the object at parent+0x78 when set (0x00488da1-
// 0x00488daa), the parent's slot +0xcc with the id (0x00488dc4) and the button's slot +0x120 (0x00488dce), whose result is
// returned. Otherwise InRect is asked again (call 0x00488de4): outside returns 0 (0x00488dee). Inside and pressed (+0x614
// non-zero, 0x00488df4-0x00488dfc): isPrimaryMouseDown 0x0047abc0 (call 0x00488e00) zero returns 0, else the button's slot
// +0x78 runs with (x, y) (call 0x00488e0f) and its result is returned. Inside and not pressed: an interval +0x5ac other than -1
// starts Timer::initStartMmPeriodic 0x00486d60 on this + 0x5b4 with (0x00488fb0, 1, this, interval, 5) (0x00488e1b-0x00488e37),
// +0x614 = 1 (0x00488e42), slot +0x1c of the object at parent+0x7c when set (0x00488e4c-0x00488e55), the parent's slot +0xd0
// with the id (0x00488e6f) and the button's slot +0x120 (0x00488e79), whose result is returned.
typedef int(__fastcall* ButtonOnMouseMove_t)(void* ecx, void* edx, int x, int y);
ButtonOnMouseMove_t ButtonOnMouseMove_orig;
int __fastcall ButtonOnMouseMove_re(void* self, void*, int x, int y) {
    int rect[4];
    void* q = At<void*>(self, 0x278);                                        // 0x00488cf8
    if (q != 0) VSlot<Slot1r_t>(q, 0xd0)(q, 0, reinterpret_cast<int>(rect)); // 0x00488d09
    rect[2] = rect[2] - rect[0];                                             // 0x00488d36
    rect[3] = rect[3] - rect[1];                                             // 0x00488d2d
    rect[0] = 0;                                                             // 0x00488d34
    rect[1] = 0;                                                             // 0x00488d1f
    int active = G<int>(0x0083ab40);                                         // 0x00488d2f
    if (active == 0) active = G<int>(0x0083ab44);                            // 0x00488d38 / 0x00488d46 / 0x00488d48
    if (active != reinterpret_cast<int>(self)) return active;                // 0x00488d4d / 0x00488d4f
    int down = G<int>(0x0083ab48);                                           // 0x00488d55
    if (down == 0) return 0;                                                 // 0x00488d5a / 0x00488d5c
    int inside = kInRect(x, y, rect);                                        // 0x00488d71
    if (inside == 0 && At<int>(self, 0x614) == 1) {                          // 0x00488d7b / 0x00488d7d / 0x00488d84
        kTimerStop(static_cast<char*>(self) + 0x5b4, 0);                     // 0x00488d8c
        void* parent = At<void*>(self, 0x130);                               // 0x00488d91
        At<int>(self, 0x614) = 0;                                            // 0x00488d97
        void* o = At<void*>(parent, 0x78);                                   // 0x00488da1
        if (o != 0) VSlot<Slot0r_t>(o, 0x1c)(o, 0);                          // 0x00488da4 / 0x00488da6 / 0x00488daa
        void* p = At<void*>(self, 0x130);                                    // 0x00488db3
        VSlot<Slot1r_t>(p, 0xcc)(p, 0, At<int>(self, 0x5e8));                // 0x00488dc4
        return VSlot<Slot0r_t>(self, 0x120)(self, 0);                        // 0x00488dce
    }
    inside = kInRect(x, y, rect);                                            // 0x00488de4
    if (inside == 0) return 0;                                               // 0x00488dec / 0x00488dee
    if (At<int>(self, 0x614) != 0) {                                         // 0x00488df4 / 0x00488dfa / 0x00488dfc
        if (kIsPrimaryMouseDown(self, 0) == 0) return 0;                     // 0x00488e00 / 0x00488e05 / 0x00488e07
        return VSlot<Slot2r_t>(self, 0x78)(self, 0, x, y);                   // 0x00488e0f
    }
    int iv = At<int>(self, 0x5ac);                                           // 0x00488e1b
    if (iv != -1)                                                            // 0x00488e21 / 0x00488e24
        kTimerInitStartMmPeriodic(static_cast<char*>(self) + 0x5b4, 0, 0x00488fb0, 1, reinterpret_cast<int>(self), iv, 5);   // 0x00488e37
    void* parent = At<void*>(self, 0x130);                                   // 0x00488e3c
    At<int>(self, 0x614) = 1;                                                // 0x00488e42
    void* o = At<void*>(parent, 0x7c);                                       // 0x00488e4c
    if (o != 0) VSlot<Slot0r_t>(o, 0x1c)(o, 0);                              // 0x00488e4f / 0x00488e51 / 0x00488e55
    void* p = At<void*>(self, 0x130);                                        // 0x00488e5e
    VSlot<Slot1r_t>(p, 0xd0)(p, 0, At<int>(self, 0x5e8));                    // 0x00488e6f
    return VSlot<Slot0r_t>(self, 0x120)(self, 0);                            // 0x00488e79
}

// =====================================================================================================================
// Tooltip::show / Tooltip::hide. Their drawing callees drawTooltip 0x0047cdb0 and redrawDrawList 0x0047f8e0 are replaced by
// the fixture's recording stubs; the text measure (Stream4838f0::puts 0x004838f0 on the font object), strchr 0x004a6170
// and Tooltip::hide itself run as the originals.
// =====================================================================================================================

typedef int(__cdecl* DrawTooltip_t)(int* rect);
typedef void(__cdecl* RedrawDrawList_t)(int* rect, int a);
typedef char*(__cdecl* Strchr_t)(const char* s, int c);
typedef int(__fastcall* FontMeasure_t)(void* ecx, void* edx, const char* text);
typedef int(__fastcall* DispWidth_t)(void* ecx, void* edx);
const DrawTooltip_t kDrawTooltip = reinterpret_cast<DrawTooltip_t>(0x0047cdb0);
const RedrawDrawList_t kRedrawDrawList = reinterpret_cast<RedrawDrawList_t>(0x0047f8e0);
const Strchr_t kStrchr = reinterpret_cast<Strchr_t>(0x004a6170);
const FontMeasure_t kFontMeasure = reinterpret_cast<FontMeasure_t>(0x004838f0);

// 0x00480360  Tooltip::hide(): the visible-tip global 0x0083aac4 at zero returns 0 (`mov ecx, [0x0083aac4]; xor eax, eax;
// cmp ecx, eax; je` 0x00480360-0x0048036a). Otherwise 0x0083aac0 and 0x0083aac4 are cleared (0x00480372 / 0x00480377),
// redrawDrawList 0x0047f8e0 runs with (0x0083a2c8, 0) (pushes 0x0048036c / 0x0048036d, call 0x0048037c) and drawTooltip
// 0x0047cdb0 with 0x0083a2c8 (push 0x00480381, call 0x00480386); its result is returned (`add esp, 0xc; ret` 0x0048038b).
Cdecl0_t TooltipHide_orig;
int __cdecl TooltipHide_re() {
    if (G<int>(0x0083aac4) == 0) return 0;                                   // 0x00480366 / 0x00480368 / 0x0048036a
    G<int>(0x0083aac0) = 0;                                                  // 0x00480372
    G<int>(0x0083aac4) = 0;                                                  // 0x00480377
    kRedrawDrawList(reinterpret_cast<int*>(0x0083a2c8), 0);                  // 0x0048037c
    return kDrawTooltip(reinterpret_cast<int*>(0x0083a2c8));                 // 0x00480386
}

// 0x00480220  Tooltip::show(this, text, rect): a NULL text (`test edi, edi; je` 0x00480229 / 0x0048022b) or NULL rect
// (0x00480235 / 0x00480237) returns at once. Tooltip::hide 0x00480360 runs (call 0x0048023f); 0x0083aac0 = this (0x00480246).
// Each line of the text (separated by byte 0x5e: strchr 0x004a6170, call 0x00480251) is measured with the font object
// 0x0083aac8 through 0x004838f0: the width of the sample string 0x004c3f70 (call 0x0048026d) is the padding, the line's
// width plus twice the padding (`lea eax, [eax + ecx*2]` 0x00480288) raises the maximum (`jle` 0x0048028d); the separator
// is temporarily zeroed (0x0048025f) and restored (0x00480295); lines are counted in ebx (0x00480298). The line height is
// font+0x10 + font+8 (or font+0xc when font+8 is negative: signed `jl` 0x004802aa, 0x004802b3) times the line count
// (`imul eax, ebx` 0x004802b6). The top is rect.top - height, or rect.bottom when that is negative (`jns` 0x004802c4,
// 0x004802c6). The left is (rect.right - width + rect.left) halved toward zero (`cdq; sub eax, edx; sar esi, 1`
// 0x004802d2-0x004802d7), 0 when negative (0x004802db); otherwise, when left + width exceeds the display's virtual slot
// +0xa8 (0 for a NULL display: 0x004802df-0x004802fe), left = that width - width (0x00480302-0x00480314). The rect
// 0x0083a2c8 = (left, top, left + width, top + height) (0x0048031a-0x00480335), 0x0083aac4 = the text (0x0048033b) and
// drawTooltip 0x0047cdb0 runs on the rect (call 0x00480341); its result is the result. `ret 8` (0x0048034e).
typedef int(__fastcall* TooltipShowT)(void* ecx, void* edx, const char* text, int* rect);
TooltipShowT TooltipShow_orig;
int __fastcall TooltipShow_re(void* self, void*, const char* text, int* rect) {
    if (text == 0) return 0;                                                 // 0x0048022b
    if (rect == 0) return 0;                                                 // 0x00480237
    reinterpret_cast<Cdecl0_t>(0x00480360)();                                // 0x0048023f
    int maxw = 0, lines = 0;                                                 // 0x00480244 / 0x0048024c
    G<void*>(0x0083aac0) = self;                                             // 0x00480246
    const char* line = text;
    for (;;) {
        char* sep = kStrchr(line, 0x5e);                                     // 0x00480251
        if (sep != 0) *sep = 0;                                              // 0x0048025d / 0x0048025f
        int pad = kFontMeasure(G<void*>(0x0083aac8), 0, reinterpret_cast<const char*>(0x004c3f70));   // 0x0048026d
        int w = kFontMeasure(G<void*>(0x0083aac8), 0, line);                 // 0x0048027f
        w = w + pad * 2;                                                     // 0x00480288
        if (w > maxw) maxw = w;                                              // 0x0048028b / 0x0048028d
        if (sep != 0) *sep = 0x5e;                                           // 0x00480291 / 0x00480293 / 0x00480295
        lines++;                                                             // 0x00480298
        line = sep + 1;                                                      // 0x00480299
        if (sep == 0) break;                                                 // 0x0048029c / 0x0048029e
    }
    int* font = G<int*>(0x0083aac8);                                         // 0x004802a0
    int lh;
    if (font[2] < 0) lh = font[3];                                           // 0x004802a5 / 0x004802aa / 0x004802b3
    else lh = font[4] + font[2];                                             // 0x004802ac / 0x004802af
    int total = lh * lines;                                                  // 0x004802b6
    int top = rect[1] - total;                                               // 0x004802bf / 0x004802c2
    if (top < 0) top = rect[3];                                              // 0x004802c4 / 0x004802c6
    int v = rect[2] - maxw + rect[0];                                        // 0x004802c9-0x004802d0
    if (v < 0) v = v + 1;                                                    // 0x004802d2 / 0x004802d3 (cdq; sub eax, edx)
    int left = v >> 1;                                                       // 0x004802d5 / 0x004802d7 (sar)
    if (left < 0) {                                                          // 0x004802d9
        left = 0;                                                            // 0x004802db
    } else {
        void* disp = G<void*>(0x0083ad50);                                   // 0x004802df
        int sw;
        if (disp == 0) sw = 0;                                               // 0x004802e5 / 0x004802e7 / 0x004802e9
        else sw = reinterpret_cast<DispWidth_t>((*reinterpret_cast<void***>(disp))[0xa8 / 4])(disp, 0);   // 0x004802ef
        disp = G<void*>(0x0083ad50);                                         // 0x004802f5
        if (left + maxw > sw) {                                              // 0x004802fb / 0x004802fe / 0x00480300
            if (disp == 0) sw = 0;                                           // 0x00480302 / 0x00480304 / 0x00480306
            else sw = reinterpret_cast<DispWidth_t>((*reinterpret_cast<void***>(disp))[0xa8 / 4])(disp, 0);   // 0x0048030c
            left = sw - maxw;                                                // 0x00480312 / 0x00480314
        }
    }
    G<int>(0x0083a2c8) = left;                                               // 0x0048031a
    G<int>(0x0083a2cc) = top;                                                // 0x00480320
    G<int>(0x0083a2d0) = left + maxw;                                        // 0x0048032f
    G<int>(0x0083a2d4) = top + total;                                        // 0x00480335
    G<const char*>(0x0083aac4) = text;                                       // 0x0048033b
    return kDrawTooltip(reinterpret_cast<int*>(0x0083a2c8));                 // 0x00480341
}

}  // namespace

// 0x00488ab0  Button::beginPress
SG_HOOK("golf_clean.exe", 0x00488ab0, Button_beginPress, ButtonBeginPress_re, ButtonBeginPress_orig);
// 0x00488cf0  Button::onMouseMove
SG_HOOK("golf_clean.exe", 0x00488cf0, Button_onMouseMove, ButtonOnMouseMove_re, ButtonOnMouseMove_orig);
// 0x00480360  Tooltip::hide
SG_HOOK("golf_clean.exe", 0x00480360, Tooltip_hide, TooltipHide_re, TooltipHide_orig);
// 0x00480220  Tooltip::show
SG_HOOK("golf_clean.exe", 0x00480220, Tooltip_show, TooltipShow_re, TooltipShow_orig);
// 0x00492bd0  HotList::showTip
SG_HOOK("golf_clean.exe", 0x00492bd0, HotList_showTip, HotListShowTip_re, HotListShowTip_orig);
// 0x00492bb0  HotList::tipTimerCallback
SG_HOOK("golf_clean.exe", 0x00492bb0, HotList_tipTimerCallback, HotListTipTimerCallback_re, HotListTipTimerCallback_orig);
// 0x00488fd0  Button::onTimer
SG_HOOK("golf_clean.exe", 0x00488fd0, Button_onTimer, ButtonOnTimer_re, ButtonOnTimer_orig);
// 0x00488e90  Button::onTipTimer
SG_HOOK("golf_clean.exe", 0x00488e90, Button_onTipTimer, ButtonOnTipTimer_re, ButtonOnTipTimer_orig);

// 0x00484060  freeSoundLib
SG_HOOK("golf_clean.exe", 0x00484060, freeSoundLib, FreeSoundLib_re, FreeSoundLib_orig);
// 0x00484090  callSoundHook74
SG_HOOK("golf_clean.exe", 0x00484090, callSoundHook74, CallSoundHook74_re, CallSoundHook74_orig);
// 0x00483fc0  soundVersionCheck
SG_HOOK("golf_clean.exe", 0x00483fc0, soundVersionCheck, SoundVersionCheck_re, SoundVersionCheck_orig);
// 0x00487180  BinkPlayer::frame
SG_HOOK("golf_clean.exe", 0x00487180, BinkPlayer_frame, BinkPlayerFrame_re, BinkPlayerFrame_orig);
// 0x004823c0  setNewHeap
SG_HOOK("golf_clean.exe", 0x004823c0, setNewHeap, SetNewHeap_re, SetNewHeap_orig);

// 0x0047cfd0  resetViewState47cfd0
SG_HOOK("golf_clean.exe", 0x0047cfd0, resetViewState47cfd0, ResetViewState_re, ResetViewState_orig);
// 0x00474dd0  Surface_createFull
SG_HOOK("golf_clean.exe", 0x00474dd0, Surface_createFull, SurfaceCreateFull_re, SurfaceCreateFull_orig);

// 0x00474ae0  Image_ctor
SG_HOOK("golf_clean.exe", 0x00474ae0, Image_ctor, ImageCtor_re, ImageCtor_orig);
// 0x00479c40  View_ctor
SG_HOOK("golf_clean.exe", 0x00479c40, View_ctor, ViewCtor_re, ViewCtor_orig);
// 0x004804a0  View4804a0::ctor
SG_HOOK("golf_clean.exe", 0x004804a0, View4804a0_ctor, View4804a0Ctor_re, View4804a0Ctor_orig);
// 0x00486070  EditBox::ctor
SG_HOOK("golf_clean.exe", 0x00486070, EditBox_ctor, EditBoxCtor_re, EditBoxCtor_orig);
// 0x00485ff0  EditBox::resetFields
SG_HOOK("golf_clean.exe", 0x00485ff0, EditBox_resetFields, EditBoxResetFields_re, EditBoxResetFields_orig);
// 0x0049c020  ctorGlobalObj49c020
SG_HOOK("golf_clean.exe", 0x0049c020, ctorGlobalObj49c020, CtorGlobalObj49c020_re, CtorGlobalObj49c020_orig);
// 0x00478a20  List_reset
SG_HOOK("golf_clean.exe", 0x00478a20, List_reset, ListReset_re, ListReset_orig);
// 0x00478a70  Widget_reset
SG_HOOK("golf_clean.exe", 0x00478a70, Widget_reset, WidgetReset_re, WidgetReset_orig);
// 0x00478a90  addDropdownSpot
SG_HOOK("golf_clean.exe", 0x00478a90, addDropdownSpot, AddDropdownSpot_re, AddDropdownSpot_orig);

// 0x00497c20  netBeginOp0
SG_HOOK("golf_clean.exe", 0x00497c20, netBeginOp0, NetBeginOp0_re, NetBeginOp0_orig);
// 0x00497c70  netBeginOp1
SG_HOOK("golf_clean.exe", 0x00497c70, netBeginOp1, NetBeginOp1_re, NetBeginOp1_orig);
// 0x00483ba0  screenFlip
SG_HOOK("golf_clean.exe", 0x00483ba0, screenFlip, ScreenFlip_re, ScreenFlip_orig);
// 0x00475c20  DC_calla0
SG_HOOK("golf_clean.exe", 0x00475c20, DC_calla0, DcCalla0_re, DcCalla0_orig);
// 0x0049ead0  freeCursors
SG_HOOK("golf_clean.exe", 0x0049ead0, freeCursors, FreeCursors_re, FreeCursors_orig);
// 0x004a01d0  loggerSetPath
SG_HOOK("golf_clean.exe", 0x004a01d0, loggerSetPath, LoggerSetPath_re, LoggerSetPath_orig);
// 0x004a0180  loggerCtor
SG_HOOK("golf_clean.exe", 0x004a0180, loggerCtor, LoggerCtor_re, LoggerCtor_orig);

// 0x004840c0  callSoundHook94
SG_HOOK("golf_clean.exe", 0x004840c0, callSoundHook94, CallSoundHook94_re, CallSoundHook94_orig);
// 0x00492da0  MappedFile::scalarDtor
SG_HOOK("golf_clean.exe", 0x00492da0, MappedFile_scalarDtor, MappedFileScalarDtor_re, MappedFileScalarDtor_orig);
// 0x004884b0  Cursor::destroy
SG_HOOK("golf_clean.exe", 0x004884b0, Cursor_destroy, CursorDestroy_re, CursorDestroy_orig);
// 0x00487060  BinkPlayer::close
SG_HOOK("golf_clean.exe", 0x00487060, BinkPlayer_close, BinkPlayerClose_re, BinkPlayerClose_orig);
// 0x00486c90  Timer::ctor
SG_HOOK("golf_clean.exe", 0x00486c90, Timer_ctor, TimerCtor_re, TimerCtor_orig);
// 0x00487210  StrList::ctor
SG_HOOK("golf_clean.exe", 0x00487210, StrList_ctor, StrListCtor_re, StrListCtor_orig);
// 0x00487f80  Reader::closeFile
SG_HOOK("golf_clean.exe", 0x00487f80, Reader_closeFile, ReaderCloseFile_re, ReaderCloseFile_orig);
// 0x00487f30  Reader::free
SG_HOOK("golf_clean.exe", 0x00487f30, Reader_free, ReaderFree_re, ReaderFree_orig);
// 0x00487ee0  Reader::allocBuffers
SG_HOOK("golf_clean.exe", 0x00487ee0, Reader_allocBuffers, ReaderAllocBuffers_re, ReaderAllocBuffers_orig);
// 0x00487ea0  Reader::ctor
SG_HOOK("golf_clean.exe", 0x00487ea0, Reader_ctor, ReaderCtor_re, ReaderCtor_orig);
// 0x004a4b00  listClear4a4b
SG_HOOK("golf_clean.exe", 0x004a4b00, listClear4a4b, ListClear_re, ListClear_orig);
// 0x004a4b60  listAppend4a4b60
SG_HOOK("golf_clean.exe", 0x004a4b60, listAppend4a4b60, ListAppend_re, ListAppend_orig);
// 0x004a4c70  queuePop
SG_HOOK("golf_clean.exe", 0x004a4c70, queuePop, QueuePop_re, QueuePop_orig);
