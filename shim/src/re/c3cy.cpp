// C3 batch c3cy (round 24, 2026-10-10): the IMPORT STAND-IN pilot. Functions of golf_clean.exe and jgld.dll that
// earlier batches dropped only because they call a Win32 import. Their A/B runs under re/frida/js/importstandin.js,
// which for one key points the module's IAT slots at recording stubs on the A/B thread (the call sequence is
// compared state, the results are the fixture's).
//
// RULE for every body here: an import is called through the SAME IAT slot the original reads (the exe's slot
// address, or jgld's base + slot RVA), never through the shim's own import table. The shim is another module: its
// imports would reach the real API with fixture values, and the stand-in would not see the call.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py <module> 0x<addr> --list) and
// cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx
// unused). Callees run through their original addresses:
//   MappedFile::dtor / MappedFile::open (exe) -> MappedFile::close 0x00492e80 (this batch)
//   Snd::busy                               -> Snd::queryDevice4845d0 0x004845d0 (C2)
//   jgld MappedFile::dtor                   -> MappedFile::close through its thunk 0x100018bb (this batch)
//   jgld MappedFile::deletingDtor           -> MappedFile::dtor through its thunk 0x100012c1 (this batch),
//                                              operator delete 0x1007e6d0 (heap stand-in in the fixture)
//   jgld Font::release                      -> jfree through its thunk 0x100017b7 (C3, c3cn)
//
// jgld.dll addresses in comments are VAs (image base 0x10000000); the DLL is relocated at run time, so every jgld
// address is rebuilt from the running base.
#include "hooks.h"

namespace {

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
static inline unsigned& field_u(void* base, unsigned off) {
    return *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(base) + off);
}
// The import whose IAT slot is at `slot` (`call dword ptr [slot]` in the original).
template <typename T> static inline T Imp(unsigned slot) {
    return *reinterpret_cast<T*>(slot);
}

// ---- golf_clean.exe IAT slots (pefile over original\golf_clean.exe; the exe has no relocations)
typedef int(__stdcall* Handle1_t)(void* h);                 // CloseHandle, UnmapViewOfFile
typedef void*(__stdcall* CreateFileA_t)(const char*, unsigned, unsigned, void*, unsigned, unsigned, void*);
typedef void*(__stdcall* CreateFileMappingA_t)(void*, void*, unsigned, unsigned, unsigned, const char*);
typedef void*(__stdcall* MapViewOfFile_t)(void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__stdcall* GetFileSize_t)(void*, unsigned*);
typedef unsigned(__stdcall* NoArgs_t)();                    // GetLastError, timeGetTime
typedef void*(__stdcall* FindFirstFileA_t)(const char*, void*);
typedef int(__stdcall* Int1_t)(int);                        // GetSystemMetrics
typedef short(__stdcall* GetAsyncKeyState_t)(int);
typedef int(__stdcall* ClipCursor_t)(const void* rect);

const unsigned kIatUnmapViewOfFile = 0x004ba0d0;
const unsigned kIatCloseHandle = 0x004ba0cc;
const unsigned kIatCreateFileA = 0x004ba0c0;
const unsigned kIatCreateFileMappingA = 0x004ba0b8;
const unsigned kIatMapViewOfFile = 0x004ba0b4;
const unsigned kIatGetFileSize = 0x004ba0a0;
const unsigned kIatGetLastError = 0x004ba02c;
const unsigned kIatFindFirstFileA = 0x004ba090;
const unsigned kIatTimeGetTime = 0x004ba21c;
const unsigned kIatGetSystemMetrics = 0x004ba1c8;
const unsigned kIatGetAsyncKeyState = 0x004ba1f0;
const unsigned kIatClipCursor = 0x004ba1d0;

// ---- exe globals
unsigned* const g_randomSeed = reinterpret_cast<unsigned*>(0x00822d9c);      // RNG seed (Random::next)
unsigned* const g_timeSeed51b380 = reinterpret_cast<unsigned*>(0x0051b380);
void** const g_cursorClipOwner = reinterpret_cast<void**>(0x0083aa98);
int* const g_cursorWidth = reinterpret_cast<int*>(0x0083b5f8);
int* const g_cursorHeight = reinterpret_cast<int*>(0x0083b5fc);

// ---- exe callees
typedef void(__fastcall* Void0_t)(void* ecx, void* edx);
typedef int(__fastcall* Int0_t)(void* ecx, void* edx);
const Void0_t kMappedFileClose = reinterpret_cast<Void0_t>(0x00492e80);
const Int0_t kSndQueryDevice = reinterpret_cast<Int0_t>(0x004845d0);

// ---- jgld.dll
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
template <typename T> static inline T JImp(unsigned slotVa) {
    return *reinterpret_cast<T*>(JgldVA(slotVa));
}
const unsigned kJIatUnmapViewOfFile = 0x1012c430;
const unsigned kJIatCloseHandle = 0x1012c428;
const unsigned kJIatDeleteObject = 0x1012c370;
const unsigned kJIatRemoveFontResourceA = 0x1012c3c0;
typedef int(__stdcall* Ptr1_t)(void*);                      // DeleteObject
typedef int(__stdcall* Str1_t)(const char*);                // RemoveFontResourceA
typedef int(__cdecl* Jfree_t)(void*);
typedef void(__cdecl* OpDelete_t)(void*);

// =====================================================================================================================
// golf_clean.exe
// =====================================================================================================================

// 0x00492e80  MappedFile::close(this): release whatever the mapped-file object still holds, view first. A non-null
// view at +4 (read 0x00492e84, tested 0x00492e89) goes to UnmapViewOfFile [0x4ba0d0] (0x00492e8c) and +4 is cleared
// (0x00492e92). A non-null mapping handle at +0xc (0x00492e99 / 0x00492ea4) goes to CloseHandle [0x4ba0cc]
// (0x00492ea7, the slot loaded into edi at 0x00492e9c) and +0xc is cleared (0x00492ea9). A file handle at +8 that is
// not INVALID_HANDLE_VALUE (-1, `cmp eax, -1` 0x00492eb3) goes to the same CloseHandle (0x00492eb9) and +8 becomes -1
// (0x00492ebb). No return value is set.
typedef void(__fastcall* MfClose_t)(void* ecx, void* edx);
MfClose_t MfClose_orig;
void __fastcall MfClose_re(void* self, void*) {
    if (field_ptr(self, 4) != 0) {                                               // 0x00492e84 / 0x00492e89
        Imp<Handle1_t>(kIatUnmapViewOfFile)(field_ptr(self, 4));                 // 0x00492e8c
        field_i(self, 4) = 0;                                                    // 0x00492e92
    }
    Handle1_t closeHandle = Imp<Handle1_t>(kIatCloseHandle);                     // 0x00492e9c
    if (field_ptr(self, 0xc) != 0) {                                             // 0x00492e99 / 0x00492ea4
        closeHandle(field_ptr(self, 0xc));                                       // 0x00492ea7
        field_i(self, 0xc) = 0;                                                  // 0x00492ea9
    }
    if (field_i(self, 8) != -1) {                                                // 0x00492eb0 / 0x00492eb3
        closeHandle(field_ptr(self, 8));                                         // 0x00492eb9
        field_i(self, 8) = -1;                                                   // 0x00492ebb
    }
}

// 0x00492dc0  MappedFile::dtor(this): store the class vtable 0x004bba78 at +0 (0x00492dc0) and tail-jump into
// MappedFile::close 0x00492e80 (0x00492dc6).
typedef void(__fastcall* MfDtor_t)(void* ecx, void* edx);
MfDtor_t MfDtor_orig;
void __fastcall MfDtor_re(void* self, void*) {
    field_u(self, 0) = 0x004bba78;                                               // 0x00492dc0
    kMappedFileClose(self, 0);                                                   // 0x00492dc6
}

// 0x00492dd0  MappedFile::open(this, name, sequential): map a file read-only and return the view. The second
// argument picks the CreateFileA flags: `neg / sbb / and 0xf8000000 / add 0x10000080` (0x00492dd5-0x00492de2) gives
// 0x08000080 (FILE_FLAG_SEQUENTIAL_SCAN | NORMAL) when it is non-zero and 0x10000080 (FILE_FLAG_RANDOM_ACCESS |
// NORMAL) when it is 0. MappedFile::close 0x00492e80 runs first (0x00492de8). CreateFileA(name, 0x80000000
// GENERIC_READ, 1 FILE_SHARE_READ, NULL, 3 OPEN_EXISTING, flags, NULL) (pushes 0x00492df1-0x00492dff, call
// 0x00492e00) is stored at +8 (0x00492e09); -1 calls GetLastError [0x4ba02c] and ignores its result (0x00492e0e) and
// returns 0 (0x00492e15). CreateFileMappingA(file, NULL, 2 PAGE_READONLY, 0, 0, NULL) (0x00492e1b-0x00492e26) is
// stored at +0xc (0x00492e2e); NULL closes again and returns 0 (0x00492e33-0x00492e3b). MapViewOfFile(mapping,
// 4 FILE_MAP_READ, 0, 0, 0) (0x00492e41-0x00492e4a) is stored at +4 (0x00492e52); NULL closes and returns 0
// (0x00492e57-0x00492e5f). Otherwise GetFileSize(file, NULL) (0x00492e65-0x00492e6b) is stored at +0x10
// (0x00492e71) and the view at +4 is returned (0x00492e74). `ret 8` (0x00492e79).
typedef void*(__fastcall* MfOpen_t)(void* ecx, void* edx, const char* name, int sequential);
MfOpen_t MfOpen_orig;
void* __fastcall MfOpen_re(void* self, void*, const char* name, int sequential) {
    unsigned flags = sequential != 0 ? 0x08000080u : 0x10000080u;                // 0x00492dd5-0x00492de2
    kMappedFileClose(self, 0);                                                   // 0x00492de8
    void* file = Imp<CreateFileA_t>(kIatCreateFileA)(name, 0x80000000u, 1, 0, 3, flags, 0);   // 0x00492e00
    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 8) = file;         // 0x00492e09
    if (file == reinterpret_cast<void*>(-1)) {                                   // 0x00492e06 / 0x00492e0c
        Imp<NoArgs_t>(kIatGetLastError)();                                       // 0x00492e0e
        return 0;                                                                // 0x00492e15
    }
    void* mapping = Imp<CreateFileMappingA_t>(kIatCreateFileMappingA)(file, 0, 2, 0, 0, 0);    // 0x00492e26
    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0xc) = mapping;    // 0x00492e2e
    if (mapping == 0) {                                                          // 0x00492e2c / 0x00492e31
        kMappedFileClose(self, 0);                                               // 0x00492e35
        return 0;                                                                // 0x00492e3b
    }
    void* view = Imp<MapViewOfFile_t>(kIatMapViewOfFile)(mapping, 4, 0, 0, 0);   // 0x00492e4a
    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 4) = view;         // 0x00492e52
    if (view == 0) {                                                             // 0x00492e50 / 0x00492e55
        kMappedFileClose(self, 0);                                               // 0x00492e59
        return 0;                                                                // 0x00492e5f
    }
    field_u(self, 0x10) = Imp<GetFileSize_t>(kIatGetFileSize)(field_ptr(self, 8), 0);   // 0x00492e6b / 0x00492e71
    return field_ptr(self, 4);                                                   // 0x00492e74
}

// 0x00479b20  releaseCursorClip(this): when this window is the cursor-clip owner g_cursorClipOwner 0x0083aa98
// (`cmp [0x83aa98], ecx` 0x00479b20, `jne` 0x00479b26), ClipCursor(NULL) [0x4ba1d0] (0x00479b28 / 0x00479b2a) frees
// the cursor and the owner is cleared (0x00479b30). Any other window changes nothing.
typedef void(__fastcall* ReleaseClip_t)(void* ecx, void* edx);
ReleaseClip_t ReleaseClip_orig;
void __fastcall ReleaseClip_re(void* self, void*) {
    if (*g_cursorClipOwner == self) {                                            // 0x00479b20 / 0x00479b26
        Imp<ClipCursor_t>(kIatClipCursor)(0);                                    // 0x00479b2a
        *g_cursorClipOwner = 0;                                                  // 0x00479b30
    }
}

// 0x004884e0  cacheCursorMetrics(): GetSystemMetrics [0x4ba1c8] (slot loaded once into esi at 0x004884e1) for
// 0xd SM_CXCURSOR (0x004884e7 / 0x004884e9) into g_cursorWidth 0x0083b5f8 (0x004884ed) and for 0xe SM_CYCURSOR
// (0x004884eb / 0x004884f2) into g_cursorHeight 0x0083b5fc (0x004884f4); returns 0 (0x004884f9).
typedef int(__cdecl* CacheMetrics_t)();
CacheMetrics_t CacheMetrics_orig;
int __cdecl CacheMetrics_re() {
    Int1_t gsm = Imp<Int1_t>(kIatGetSystemMetrics);                              // 0x004884e1
    *g_cursorWidth = gsm(0xd);                                                   // 0x004884e9 / 0x004884ed
    *g_cursorHeight = gsm(0xe);                                                  // 0x004884f2 / 0x004884f4
    return 0;                                                                    // 0x004884f9
}

// 0x0047abc0  isPrimaryMouseDown(): GetAsyncKeyState(1 VK_LBUTTON) [0x4ba1f0] (0x0047abc1 / 0x0047abc3) is
// sign-extended from 16 bits and shifted right arithmetically by 15 (`movsx esi, ax` 0x0047abc9, `sar esi, 0xf`
// 0x0047abce): -1 when bit 15 (key down) is set, else 0. That is XORed with GetSystemMetrics(0x17 SM_SWAPBUTTON)
// [0x4ba1c8] (0x0047abcc / 0x0047abd1 / 0x0047abd7) and returned.
typedef int(__cdecl* PrimaryDown_t)();
PrimaryDown_t PrimaryDown_orig;
int __cdecl PrimaryDown_re() {
    int down = static_cast<int>(Imp<GetAsyncKeyState_t>(kIatGetAsyncKeyState)(1)) >> 15;   // 0x0047abc3-0x0047abce
    return down ^ Imp<Int1_t>(kIatGetSystemMetrics)(0x17);                       // 0x0047abd1 / 0x0047abd7
}

// 0x00467110  seedFromTime(): t = timeGetTime() [0x4ba21c] (0x00467110), stored at 0x0051b380 (0x00467119) and,
// times 37 (`lea ecx, [eax+eax*8]` 0x00467116, `lea edx, [eax+ecx*4]` 0x0046711e), as the RNG seed 0x00822d9c
// (0x00467121). eax still holds t at the `ret` (0x00467127).
typedef unsigned(__cdecl* SeedFromTime_t)();
SeedFromTime_t SeedFromTime_orig;
unsigned __cdecl SeedFromTime_re() {
    unsigned t = Imp<NoArgs_t>(kIatTimeGetTime)();                               // 0x00467110
    *g_timeSeed51b380 = t;                                                       // 0x00467119
    *g_randomSeed = t + (t + t * 8) * 4;                                         // 0x00467116 / 0x0046711e / 0x00467121
    return t;                                                                    // 0x00467127
}

// 0x004378a0  fileExists(name): FindFirstFileA(name, &findData) [0x4ba090] with a 0x140-byte WIN32_FIND_DATAA on the
// stack (`sub esp, 0x140` 0x004378a4, `lea eax, [esp]` 0x004378aa, call 0x004378b0); returns 1 when the handle is not
// INVALID_HANDLE_VALUE (`cmp eax, -1` / `setne dl` 0x004378b8 / 0x004378bb), else 0. The handle is never passed to
// FindClose: every successful call leaks one find handle.
typedef int(__cdecl* FileExists_t)(const char* name);
FileExists_t FileExists_orig;
int __cdecl FileExists_re(const char* name) {
    unsigned char findData[0x140];                                               // 0x004378a4
    void* h = Imp<FindFirstFileA_t>(kIatFindFirstFileA)(name, findData);         // 0x004378b0
    return h != reinterpret_cast<void*>(-1) ? 1 : 0;                             // 0x004378b8 / 0x004378bb
}

// 0x00484b30  Snd::busy(this): with a device object at +0x40 (read 0x00484b33, `jne` 0x00484b38) or bit 4 of the
// flags at +0x58 clear (`shr eax, 4; test al, 1` 0x00484b3d / 0x00484b40, `je` 0x00484b42), the answer comes from
// Snd::queryDevice4845d0 0x004845d0 (0x00484b68 / 0x00484b6a), which asks the device's vtable slot +0x5c and returns
// 0 without a device. Otherwise a zero start time at +0x68 (0x00484b44 / 0x00484b49) returns 0 (0x00484b64), and a
// set one compares the elapsed time timeGetTime() [0x4ba21c] - [+0x68] (0x00484b4b / 0x00484b57) with the duration
// at +0x64 unsigned: duration > elapsed returns 1 (`cmp ecx, eax; jbe` 0x00484b59 / 0x00484b5b, 0x00484b5d), else 0.
typedef int(__fastcall* SndBusy_t)(void* ecx, void* edx);
SndBusy_t SndBusy_orig;
int __fastcall SndBusy_re(void* self, void*) {
    if (field_ptr(self, 0x40) != 0 || ((field_u(self, 0x58) >> 4) & 1) == 0)    // 0x00484b33-0x00484b42
        return kSndQueryDevice(self, 0);                                         // 0x00484b6a
    if (field_u(self, 0x68) == 0) return 0;                                      // 0x00484b44 / 0x00484b49
    unsigned elapsed = Imp<NoArgs_t>(kIatTimeGetTime)() - field_u(self, 0x68);   // 0x00484b4b / 0x00484b57
    return field_u(self, 0x64) > elapsed ? 1 : 0;                                // 0x00484b59 / 0x00484b5b
}

// =====================================================================================================================
// jgld.dll (debug build; RVAs in the SG_HOOK lines, VAs in the comments)
// =====================================================================================================================

// 0x00002e10  MappedFile::close(this): jgld's copy of the exe's close. A non-null view at +4 (`cmp [eax+4], 0`
// 0x10002e30) goes to UnmapViewOfFile [0x1012c430] (0x10002e3f) and +4 is cleared (0x10002e4f); a non-null mapping
// at +0xc (0x10002e59) goes to CloseHandle [0x1012c428] (0x10002e68) and +0xc is cleared (0x10002e78); a file handle
// at +8 other than -1 (0x10002e82) goes to CloseHandle (0x10002e91) and +8 becomes -1 (0x10002ea1). Each import call
// is bracketed by the debug build's esp check (__chkesp 0x1007e780 at 0x10002e47 / 0x10002e70 / 0x10002e99).
typedef void(__fastcall* JMfClose_t)(void* ecx, void* edx);
JMfClose_t JMfClose_orig;
void __fastcall JMfClose_re(void* self, void*) {
    if (field_ptr(self, 4) != 0) {                                               // 0x10002e30 / 0x10002e34
        JImp<Handle1_t>(kJIatUnmapViewOfFile)(field_ptr(self, 4));               // 0x10002e3f
        field_i(self, 4) = 0;                                                    // 0x10002e4f
    }
    if (field_ptr(self, 0xc) != 0) {                                             // 0x10002e59 / 0x10002e5d
        JImp<Handle1_t>(kJIatCloseHandle)(field_ptr(self, 0xc));                 // 0x10002e68
        field_i(self, 0xc) = 0;                                                  // 0x10002e78
    }
    if (field_i(self, 8) != -1) {                                                // 0x10002e82 / 0x10002e86
        JImp<Handle1_t>(kJIatCloseHandle)(field_ptr(self, 8));                   // 0x10002e91
        field_i(self, 8) = -1;                                                   // 0x10002ea1
    }
}

// 0x00002820  MappedFile::dtor(this): store the MappedFile vtable 0x1011d01c at +0 (0x10002840) and call
// MappedFile::close 0x10002e10 through its incremental-link thunk 0x100018bb (0x10002849).
typedef void(__fastcall* JMfDtor_t)(void* ecx, void* edx);
JMfDtor_t JMfDtor_orig;
void __fastcall JMfDtor_re(void* self, void*) {
    *reinterpret_cast<void**>(self) = JgldVA(0x1011d01c);                        // 0x10002840
    reinterpret_cast<Void0_t>(JgldVA(0x100018bb))(self, 0);                      // 0x10002849
}

// 0x000026c0  MappedFile::deletingDtor(this, flags): MappedFile::dtor 0x10002820 through its thunk 0x100012c1
// (0x100026e0); when bit 0 of flags is set (`and eax, 1` 0x100026e8, `je` 0x100026ed) operator delete 0x1007e6d0
// releases the object (0x100026f3); returns this (0x100026fb), `ret 4` (0x1000270e).
typedef void*(__fastcall* JMfDelDtor_t)(void* ecx, void* edx, unsigned flags);
JMfDelDtor_t JMfDelDtor_orig;
void* __fastcall JMfDelDtor_re(void* self, void*, unsigned flags) {
    reinterpret_cast<Void0_t>(JgldVA(0x100012c1))(self, 0);                      // 0x100026e0
    if (flags & 1)                                                               // 0x100026e8 / 0x100026ed
        reinterpret_cast<OpDelete_t>(JgldVA(0x1007e6d0))(self);                  // 0x100026f3
    return self;                                                                 // 0x100026fb
}

// 0x00068da0  Font::release(this): a GDI font handle at +0x18 (`cmp [eax+0x18], 0` 0x10068dc0) goes to DeleteObject
// [0x1012c370] (0x10068dcf) and +0x18 is cleared (0x10068ddf). A font-file name at +0x20 (0x10068de9) is removed
// with RemoveFontResourceA [0x1012c3c0] (0x10068df8), the string is handed to jfree 0x10014a90 through its thunk
// 0x100017b7 (0x10068e0c) and +0x20 is cleared (0x10068e17).
typedef void(__fastcall* FontRelease_t)(void* ecx, void* edx);
FontRelease_t FontRelease_orig;
void __fastcall FontRelease_re(void* self, void*) {
    if (field_ptr(self, 0x18) != 0) {                                            // 0x10068dc0 / 0x10068dc4
        JImp<Ptr1_t>(kJIatDeleteObject)(field_ptr(self, 0x18));                  // 0x10068dcf
        field_i(self, 0x18) = 0;                                                 // 0x10068ddf
    }
    if (field_ptr(self, 0x20) != 0) {                                            // 0x10068de9 / 0x10068ded
        JImp<Str1_t>(kJIatRemoveFontResourceA)(static_cast<const char*>(field_ptr(self, 0x20)));   // 0x10068df8
        reinterpret_cast<Jfree_t>(JgldVA(0x100017b7))(field_ptr(self, 0x20));    // 0x10068e0c
        field_i(self, 0x20) = 0;                                                 // 0x10068e17
    }
}

// =====================================================================================================================
// golf_clean.exe Timer family. Timer layout (Timer::ctor 0x00486c90): +0 vtable 0x004baea0, +4 callback, +8 flags,
// +0xc timer id, +0x10 / +0x14 the two targets (startOnce / startRepeat return 7 when both are 0), +0x18, +0x1c,
// +0x20 interval (ms), +0x24 posted flag, +0x28 resolution (5 after reset), +0x2c.
// Intervals below 0x32 use the multimedia timer (timeSetEvent / timeKillEvent), the others a window timer on the
// display's window (SetTimer / KillTimer with the HWND from the display 0x0083ad50's vtable slot +0x20).
// =====================================================================================================================

typedef unsigned(__stdcall* TimeSetEvent_t)(unsigned delay, unsigned res, void* proc, void* user, unsigned flags);
typedef unsigned(__stdcall* TimeKillEvent_t)(unsigned id);
typedef unsigned(__stdcall* SetTimer_t)(void* hwnd, void* id, unsigned elapse, void* proc);
typedef int(__stdcall* KillTimer_t)(void* hwnd, unsigned id);
typedef int(__stdcall* PostMessageA_t)(void* hwnd, unsigned msg, void* wparam, void* lparam);
const unsigned kIatTimeSetEvent = 0x004ba218;
const unsigned kIatTimeKillEvent = 0x004ba214;
const unsigned kIatSetTimer = 0x004ba1b8;
const unsigned kIatKillTimer = 0x004ba1b4;
const unsigned kIatPostMessageA = 0x004ba1e4;
void** const g_display = reinterpret_cast<void**>(0x0083ad50);
void** const g_timerOwner83aff0 = reinterpret_cast<void**>(0x0083aff0);
typedef void*(__fastcall* DisplayHwnd_t)(void* ecx, void* edx);              // display vtable +0x20
static inline void* DisplayHwnd(void* display) {
    return reinterpret_cast<DisplayHwnd_t>(field_ptr(field_ptr(display, 0), 0x20))(display, 0);
}
void* const kTimerProcMm = reinterpret_cast<void*>(0x00486f90);              // Timer::callback
void* const kTimerProcWnd = reinterpret_cast<void*>(0x00486f40);
typedef void(__cdecl* PumpInputDisplay_t)();
const PumpInputDisplay_t kPumpInputDisplay = reinterpret_cast<PumpInputDisplay_t>(0x00483da0);   // C2
const Void0_t kTimerStop = reinterpret_cast<Void0_t>(0x00486ec0);
const Void0_t kTimerReset = reinterpret_cast<Void0_t>(0x00486f10);
const Int0_t kTimerStartOnce = reinterpret_cast<Int0_t>(0x00486dc0);
const Int0_t kTimerStartRepeat = reinterpret_cast<Int0_t>(0x00486e40);
typedef void(__fastcall* TimerInit5_t)(void* ecx, void* edx, int a1, int a2, int a3, int a4, int a5);
const TimerInit5_t kTimerInit = reinterpret_cast<TimerInit5_t>(0x00486d20);

// `neg eax; sbb eax, eax; and al, 0xfe; add eax, 2`: 0 for a non-zero timer id, 2 for 0.
static inline int StartResult(unsigned id) { return id != 0 ? 0 : 2; }

// 0x00486ec0  Timer::stop(this): bit 0 of the flags at +8 is sampled first (`not esi; and esi, 1` 0x00486ec4 -
// 0x00486ecc). A non-zero timer id at +0xc (0x00486ec7 / 0x00486ed1) is killed: an interval at +0x20 below 0x32
// (`cmp [edi+0x20], 0x32; jae` 0x00486ed3 / 0x00486ed7, unsigned) calls timeKillEvent(id) [0x4ba214] (0x00486eda);
// otherwise, when the display 0x0083ad50 is not NULL (0x00486ee8 / 0x00486eea), KillTimer(display->slot+0x20(), id)
// [0x4ba1b4] (0x00486eef / 0x00486ef3). Either way the id becomes 0 (0x00486ef9), also when there was no display.
// When bit 0 was clear the function ends in a tail jump to pumpInputDisplay483da0 0x00483da0 (0x00486f04 / 0x00486f06).
// Return value (verifier fix, 2026-10-10): the original leaves a defined eax on every path, and Timer_fire 0x00495cb0
// returns it (`call 0x486ec0; pop esi; ret 4` at 0x00495cd2): eax is the id (0x00486ec7) unless an import call replaces
// it with timeKillEvent's or KillTimer's result; the tail jump to pumpInputDisplay483da0 then leaves it unchanged when the
// display 0x0083ad50 is NULL and replaces it with display slot +0x150's result otherwise (0x00483dbb).
typedef unsigned(__fastcall* TimerStop_t)(void* ecx, void* edx);
TimerStop_t TimerStop_orig;
typedef unsigned(__cdecl* PumpInputDisplayR_t)();
unsigned __fastcall TimerStop_re(void* self, void*) {
    bool bit0Clear = (field_u(self, 8) & 1) == 0;                                // 0x00486ec4-0x00486ecc
    unsigned id = field_u(self, 0xc);                                            // 0x00486ec7
    unsigned eax = id;
    if (id != 0) {                                                               // 0x00486ecf / 0x00486ed1
        if (field_u(self, 0x20) < 0x32) {                                        // 0x00486ed3 / 0x00486ed7
            eax = Imp<TimeKillEvent_t>(kIatTimeKillEvent)(id);                   // 0x00486eda
        } else if (*g_display != 0) {                                            // 0x00486ee2-0x00486eea
            void* hwnd = DisplayHwnd(*g_display);                                // 0x00486eef
            eax = Imp<KillTimer_t>(kIatKillTimer)(hwnd, id);                     // 0x00486ef3
        }
        field_u(self, 0xc) = 0;                                                  // 0x00486ef9
    }
    if (bit0Clear) {                                                             // 0x00486f04 / 0x00486f06
        bool disp = *g_display != 0;                                             // 0x00483daf / 0x00483db5
        unsigned r = reinterpret_cast<PumpInputDisplayR_t>(kPumpInputDisplay)();
        if (disp) eax = r;                                                       // 0x00483dbb
    }
    return eax;
}

// 0x00486f10  Timer::reset(this): Timer::stop 0x00486ec0 (0x00486f13), then the resolution +0x28 = 5 (0x00486f1a)
// and +0x10, +8, +0x14, +0x18, +0x1c, +0x20, +0x24, +0x2c = 0 (0x00486f21-0x00486f36); returns 0 (0x00486f18).
typedef int(__fastcall* TimerReset_t)(void* ecx, void* edx);
TimerReset_t TimerReset_orig;
int __fastcall TimerReset_re(void* self, void*) {
    kTimerStop(self, 0);                                                         // 0x00486f13
    field_u(self, 0x28) = 5;                                                     // 0x00486f1a
    field_u(self, 0x10) = 0;                                                     // 0x00486f21
    field_u(self, 8) = 0;                                                        // 0x00486f24
    field_u(self, 0x14) = 0;                                                     // 0x00486f27
    field_u(self, 0x18) = 0;                                                     // 0x00486f2a
    field_u(self, 0x1c) = 0;                                                     // 0x00486f2d
    field_u(self, 0x20) = 0;                                                     // 0x00486f30
    field_u(self, 0x24) = 0;                                                     // 0x00486f33
    field_u(self, 0x2c) = 0;                                                     // 0x00486f36
    return 0;                                                                    // 0x00486f18
}

// 0x00486dc0  Timer::startOnce(this): with both targets +0x10 and +0x14 zero (0x00486dc3-0x00486dcf) returns 7
// (0x00486dd1). Otherwise bit 0 of the flags +8 is cleared (0x00486dde / 0x00486de4). An interval +0x20 below 0x32
// (0x00486de1 / 0x00486de7) calls timeSetEvent(interval, resolution +0x28, Timer::callback 0x00486f90, this, 1)
// [0x4ba218] (pushes 0x00486dec-0x00486df5, call 0x00486df6); 1 is TIME_PERIODIC in mmsystem.h. Otherwise
// SetTimer(display->slot+0x20(), this, interval, 0x00486f40) [0x4ba1b8] (0x00486e0a-0x00486e1d), the display read
// from 0x0083ad50 without a NULL test. The id is stored at +0xc (0x00486dfc / 0x00486e23) and the result is 0 for a
// non-zero id and 2 for 0 (0x00486e00-0x00486e06 / 0x00486e27-0x00486e2d).
typedef int(__fastcall* TimerStart_t)(void* ecx, void* edx);
TimerStart_t TimerStartOnce_orig;
int __fastcall TimerStartOnce_re(void* self, void*) {
    if (field_u(self, 0x10) == 0 && field_u(self, 0x14) == 0) return 7;          // 0x00486dc3-0x00486dd1
    field_u(self, 8) &= ~1u;                                                     // 0x00486dde / 0x00486de4
    unsigned interval = field_u(self, 0x20);
    unsigned id;
    if (interval < 0x32) {                                                       // 0x00486de1 / 0x00486de7
        id = Imp<TimeSetEvent_t>(kIatTimeSetEvent)(interval, field_u(self, 0x28), kTimerProcMm, self, 1);   // 0x00486df6
    } else {
        void* hwnd = DisplayHwnd(*g_display);                                    // 0x00486e0a-0x00486e19
        id = Imp<SetTimer_t>(kIatSetTimer)(hwnd, self, interval, kTimerProcWnd);  // 0x00486e1d
    }
    field_u(self, 0xc) = id;                                                     // 0x00486dfc / 0x00486e23
    return StartResult(id);                                                      // 0x00486e00-0x00486e06
}

// 0x00486e40  Timer::startRepeat(this): the same 7 for two zero targets (0x00486e43-0x00486e51). An interval below
// 0x32 (0x00486e5b / 0x00486e5e) calls timeSetEvent(interval, resolution, 0x00486f90, this, 0) [0x4ba218]
// (0x00486e60-0x00486e6d; 0 is TIME_ONESHOT) without touching the flags; otherwise the flags +8 get bit 1 cleared and
// bit 0 set (0x00486e81-0x00486e91) before SetTimer(display->slot+0x20(), this, interval, 0x00486f40) [0x4ba1b8]
// (0x00486e94-0x00486ea0). Id to +0xc (0x00486e73 / 0x00486ea6), result 0 / 2 as in startOnce.
TimerStart_t TimerStartRepeat_orig;
int __fastcall TimerStartRepeat_re(void* self, void*) {
    if (field_u(self, 0x10) == 0 && field_u(self, 0x14) == 0) return 7;          // 0x00486e43-0x00486e51
    unsigned interval = field_u(self, 0x20);
    unsigned id;
    if (interval < 0x32) {                                                       // 0x00486e5b / 0x00486e5e
        id = Imp<TimeSetEvent_t>(kIatTimeSetEvent)(interval, field_u(self, 0x28), kTimerProcMm, self, 0);   // 0x00486e6d
    } else {
        field_u(self, 8) = (field_u(self, 8) & ~2u) | 1u;                        // 0x00486e81-0x00486e91
        void* hwnd = DisplayHwnd(*g_display);                                    // 0x00486e94-0x00486e9c
        id = Imp<SetTimer_t>(kIatSetTimer)(hwnd, self, interval, kTimerProcWnd);  // 0x00486ea0
    }
    field_u(self, 0xc) = id;                                                     // 0x00486e73 / 0x00486ea6
    return StartResult(id);                                                      // 0x00486e77-0x00486e7d
}

// 0x00486d20  Timer::init(this, a1, a2, a3, a4, a5): Timer::reset 0x00486f10 (0x00486d23), then +0x14 = a1
// (0x00486d34), +0x18 = a3 (0x00486d3b), +0x10 = 0 (0x00486d42), +0x1c = a2 (0x00486d49), +0x20 = a4 (0x00486d4c),
// +0x28 = a5 (0x00486d4f); `ret 0x14` (0x00486d53).
TimerInit5_t TimerInit_orig;
void __fastcall TimerInit_re(void* self, void*, int a1, int a2, int a3, int a4, int a5) {
    kTimerReset(self, 0);                                                        // 0x00486d23
    field_i(self, 0x14) = a1;                                                    // 0x00486d34
    field_i(self, 0x18) = a3;                                                    // 0x00486d3b
    field_i(self, 0x10) = 0;                                                     // 0x00486d42
    field_i(self, 0x1c) = a2;                                                    // 0x00486d49
    field_i(self, 0x20) = a4;                                                    // 0x00486d4c
    field_i(self, 0x28) = a5;                                                    // 0x00486d4f
}

// 0x00486cf0  Timer::initA(this, a1, a2, a3, a4): Timer::reset (0x00486cf3), then +0x10 = a1 (0x00486d04), +0x1c = a2
// (0x00486d09), +0x14 = +0x18 = 0 (0x00486d0c / 0x00486d0f), +0x20 = a3 (0x00486d16), +0x28 = a4 (0x00486d19);
// `ret 0x10` (0x00486d1d).
typedef void(__fastcall* TimerInit4_t)(void* ecx, void* edx, int a1, int a2, int a3, int a4);
TimerInit4_t TimerInitA_orig;
void __fastcall TimerInitA_re(void* self, void*, int a1, int a2, int a3, int a4) {
    kTimerReset(self, 0);                                                        // 0x00486cf3
    field_i(self, 0x10) = a1;                                                    // 0x00486d04
    field_i(self, 0x1c) = a2;                                                    // 0x00486d09
    field_i(self, 0x14) = 0;                                                     // 0x00486d0c
    field_i(self, 0x18) = 0;                                                     // 0x00486d0f
    field_i(self, 0x20) = a3;                                                    // 0x00486d16
    field_i(self, 0x28) = a4;                                                    // 0x00486d19
}

// 0x00486d60  Timer::initStartOnce(this, a1..a5): Timer::init 0x00486d20 with the same five arguments (re-pushed
// 0x00486d60-0x00486d7b, call 0x00486d7e), then Timer::startOnce 0x00486dc0 (0x00486d85), whose result is returned;
// `ret 0x14` (0x00486d8b).
typedef int(__fastcall* TimerInitStart_t)(void* ecx, void* edx, int a1, int a2, int a3, int a4, int a5);
TimerInitStart_t TimerInitStartOnce_orig;
int __fastcall TimerInitStartOnce_re(void* self, void*, int a1, int a2, int a3, int a4, int a5) {
    kTimerInit(self, 0, a1, a2, a3, a4, a5);                                     // 0x00486d7e
    return kTimerStartOnce(self, 0);                                             // 0x00486d85
}

// 0x00486d90  Timer::initStartRepeat(this, a1..a5): Timer::init (0x00486dae) then Timer::startRepeat 0x00486e40
// (0x00486db5), whose result is returned; `ret 0x14` (0x00486dbb).
TimerInitStart_t TimerInitStartRepeat_orig;
int __fastcall TimerInitStartRepeat_re(void* self, void*, int a1, int a2, int a3, int a4, int a5) {
    kTimerInit(self, 0, a1, a2, a3, a4, a5);                                     // 0x00486dae
    return kTimerStartRepeat(self, 0);                                           // 0x00486db5
}

// 0x00486ce0  Timer::dtor(this): the Timer vtable 0x004baea0 into +0 (0x00486ce0) and a tail jump into Timer::reset
// 0x00486f10 (0x00486ce6), whose eax (0) is what the caller sees.
typedef int(__fastcall* TimerDtor_t)(void* ecx, void* edx);
TimerDtor_t TimerDtor_orig;
int __fastcall TimerDtor_re(void* self, void*) {
    field_u(self, 0) = 0x004baea0;                                               // 0x00486ce0
    return reinterpret_cast<Int0_t>(0x00486f10)(self, 0);                        // 0x00486ce6
}

// 0x00486f90  Timer::callback(id, msg, user, dw1, dw2): the multimedia-timer procedure startOnce / startRepeat pass to
// timeSetEvent (pushed at 0x00486def / 0x00486e66). The Timer is the user dword (0x00486f91); NULL does nothing
// (0x00486f95 / 0x00486f97). A callback at Timer+4 (0x00486f99 / 0x00486f9e) is called with no arguments
// (0x00486fa0) and nothing else happens. Otherwise, when the global 0x0083aff0 is set and is another Timer
// (0x00486fa6-0x00486fb1), nothing; when the posted flag +0x24 is already set (0x00486fb3 / 0x00486fb8), nothing;
// else PostMessageA(display->slot+0x20(), 0x401, timer, 0) [0x4ba1e4] (0x00486fba-0x00486fce) and +0x24 = 1
// (0x00486fd4). `ret 0x14` (0x00486fdc).
typedef void(__stdcall* TimerCallback_t)(unsigned id, unsigned msg, void* user, unsigned dw1, unsigned dw2);
typedef void(__cdecl* TimerFn_t)();
TimerCallback_t TimerCallback_orig;
void __stdcall TimerCallback_re(unsigned, unsigned, void* timer, unsigned, unsigned) {
    if (timer == 0) return;                                                      // 0x00486f95 / 0x00486f97
    if (field_ptr(timer, 4) != 0) {                                              // 0x00486f99 / 0x00486f9e
        reinterpret_cast<TimerFn_t>(field_ptr(timer, 4))();                      // 0x00486fa0
        return;
    }
    if (*g_timerOwner83aff0 != 0 && *g_timerOwner83aff0 != timer) return;        // 0x00486fa6-0x00486fb1
    if (field_u(timer, 0x24) != 0) return;                                       // 0x00486fb3 / 0x00486fb8
    void* hwnd = DisplayHwnd(*g_display);                                        // 0x00486fba-0x00486fca
    Imp<PostMessageA_t>(kIatPostMessageA)(hwnd, 0x401, timer, 0);                // 0x00486fce
    field_u(timer, 0x24) = 1;                                                    // 0x00486fd4
}

// =====================================================================================================================
// jgld.dll Font destructor chain
// =====================================================================================================================

// 0x00068760  Font::~Font(this): the Font vtable 0x1011da6c into +0 (0x10068795) and the Tracked sub-object's vtable
// 0x1011da68 into +8 (0x1006879e); when +4 holds a pointer (0x100687af / 0x100687b3) the dword it points at is cleared
// (0x100687bb). Then Font::release 0x10068da0 (thunk 0x10001014, call 0x100687c4), Tracked::dtor 0x10006b80 on the
// Tracked sub-object at this+8 (this+8 for a non-NULL this, NULL otherwise: 0x100687cd-0x100687e5; thunk 0x1000195b,
// call 0x100687e8), and FontBase::~FontBase 0x10069010 (thunk 0x1000108c, call 0x100687f7). The SEH frame
// (0x10068763-0x10068771, states 1 / 0 / -1 at 0x100687a5 / 0x100687c9 / 0x100687ed) only matters when one of the
// callees throws.
typedef void(__fastcall* FontDtor_t)(void* ecx, void* edx);
FontDtor_t FontDtor_orig;
void __fastcall FontDtor_re(void* self, void*) {
    *reinterpret_cast<void**>(self) = JgldVA(0x1011da6c);                        // 0x10068795
    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 8) = JgldVA(0x1011da68);   // 0x1006879e
    if (field_ptr(self, 4) != 0)                                                 // 0x100687af / 0x100687b3
        *static_cast<int*>(field_ptr(self, 4)) = 0;                              // 0x100687bb
    reinterpret_cast<Void0_t>(JgldVA(0x10001014))(self, 0);                      // 0x100687c4
    void* tracked = self != 0 ? reinterpret_cast<char*>(self) + 8 : 0;           // 0x100687cd-0x100687e5
    reinterpret_cast<Void0_t>(JgldVA(0x1000195b))(tracked, 0);                   // 0x100687e8
    reinterpret_cast<Void0_t>(JgldVA(0x1000108c))(self, 0);                      // 0x100687f7
}

// 0x000691b0  Font::deletingDtor(this, flags): Font::~Font 0x10068760 (thunk 0x1000180c, call 0x100691d0); flags bit 0
// (`and eax, 1` 0x100691d8, `je` 0x100691dd) hands the object to operator delete 0x1007e6d0 (0x100691e3); returns
// this (0x100691eb); `ret 4` (0x100691fe).
typedef void*(__fastcall* FontDelDtor_t)(void* ecx, void* edx, unsigned flags);
FontDelDtor_t FontDelDtor_orig;
void* __fastcall FontDelDtor_re(void* self, void*, unsigned flags) {
    reinterpret_cast<Void0_t>(JgldVA(0x1000180c))(self, 0);                      // 0x100691d0
    if (flags & 1)                                                               // 0x100691d8 / 0x100691dd
        reinterpret_cast<OpDelete_t>(JgldVA(0x1007e6d0))(self);                  // 0x100691e3
    return self;                                                                 // 0x100691eb
}

// 0x00069260  Font::deletingDtor_adj8(tracked, flags): slot 0 of the Tracked-side vtable 0x1011da68 that ~Font and
// Font::Font install at +8. It moves `this` back from the Tracked sub-object to the Font (`sub ecx, 8` 0x10069260) and
// jumps into Font::deletingDtor 0x100691b0 through its thunk 0x10001753 (0x10069263), flags left on the stack.
FontDelDtor_t FontDelDtorAdj8_orig;
void* __fastcall FontDelDtorAdj8_re(void* tracked, void*, unsigned flags) {
    void* font = reinterpret_cast<char*>(tracked) - 8;                           // 0x10069260
    return reinterpret_cast<FontDelDtor_t>(JgldVA(0x10001753))(font, 0, flags);  // 0x10069263
}

}  // namespace

// 0x00486ec0  Timer::stop
SG_HOOK("golf_clean.exe", 0x00486ec0, Timer_stop, TimerStop_re, TimerStop_orig);
// 0x00486f10  Timer::reset
SG_HOOK("golf_clean.exe", 0x00486f10, Timer_reset, TimerReset_re, TimerReset_orig);
// 0x00486dc0  Timer::startOnce
SG_HOOK("golf_clean.exe", 0x00486dc0, Timer_startOnce, TimerStartOnce_re, TimerStartOnce_orig);
// 0x00486e40  Timer::startRepeat
SG_HOOK("golf_clean.exe", 0x00486e40, Timer_startRepeat, TimerStartRepeat_re, TimerStartRepeat_orig);
// 0x00486d20  Timer::init
SG_HOOK("golf_clean.exe", 0x00486d20, Timer_init, TimerInit_re, TimerInit_orig);
// 0x00486cf0  Timer::initA
SG_HOOK("golf_clean.exe", 0x00486cf0, Timer_initA, TimerInitA_re, TimerInitA_orig);
// 0x00486d60  Timer::initStartOnce
SG_HOOK("golf_clean.exe", 0x00486d60, Timer_initStartOnce, TimerInitStartOnce_re, TimerInitStartOnce_orig);
// 0x00486d90  Timer::initStartRepeat
SG_HOOK("golf_clean.exe", 0x00486d90, Timer_initStartRepeat, TimerInitStartRepeat_re, TimerInitStartRepeat_orig);
// 0x00486ce0  Timer::dtor
SG_HOOK("golf_clean.exe", 0x00486ce0, Timer_dtor, TimerDtor_re, TimerDtor_orig);
// 0x00486f90  Timer::callback
SG_HOOK("golf_clean.exe", 0x00486f90, Timer_callback, TimerCallback_re, TimerCallback_orig);
// 0x00068760  Font::~Font (jgld.dll)
SG_HOOK("jgld.dll", 0x00068760, jgld_Font_dtor, FontDtor_re, FontDtor_orig);
// 0x000691b0  Font::deletingDtor (jgld.dll)
SG_HOOK("jgld.dll", 0x000691b0, jgld_Font_deletingDtor, FontDelDtor_re, FontDelDtor_orig);
// 0x00069260  Font::deletingDtor_adj8 (jgld.dll)
SG_HOOK("jgld.dll", 0x00069260, jgld_Font_deletingDtor_adj8, FontDelDtorAdj8_re, FontDelDtorAdj8_orig);

// 0x00492e80  MappedFile::close
SG_HOOK("golf_clean.exe", 0x00492e80, MappedFile_close, MfClose_re, MfClose_orig);
// 0x00492dc0  MappedFile::dtor
SG_HOOK("golf_clean.exe", 0x00492dc0, MappedFile_dtor, MfDtor_re, MfDtor_orig);
// 0x00492dd0  MappedFile::open
SG_HOOK("golf_clean.exe", 0x00492dd0, MappedFile_open, MfOpen_re, MfOpen_orig);
// 0x00479b20  releaseCursorClip
SG_HOOK("golf_clean.exe", 0x00479b20, releaseCursorClip, ReleaseClip_re, ReleaseClip_orig);
// 0x004884e0  cacheCursorMetrics
SG_HOOK("golf_clean.exe", 0x004884e0, cacheCursorMetrics, CacheMetrics_re, CacheMetrics_orig);
// 0x0047abc0  isPrimaryMouseDown
SG_HOOK("golf_clean.exe", 0x0047abc0, isPrimaryMouseDown, PrimaryDown_re, PrimaryDown_orig);
// 0x00467110  seedFromTime
SG_HOOK("golf_clean.exe", 0x00467110, seedFromTime, SeedFromTime_re, SeedFromTime_orig);
// 0x004378a0  fileExists
SG_HOOK("golf_clean.exe", 0x004378a0, fileExists, FileExists_re, FileExists_orig);
// 0x00484b30  Snd::busy
SG_HOOK("golf_clean.exe", 0x00484b30, Snd_busy, SndBusy_re, SndBusy_orig);
// 0x00002e10  MappedFile::close (jgld.dll)
SG_HOOK("jgld.dll", 0x00002e10, jgld_MappedFile_close, JMfClose_re, JMfClose_orig);
// 0x00002820  MappedFile::dtor (jgld.dll)
SG_HOOK("jgld.dll", 0x00002820, jgld_MappedFile_dtor, JMfDtor_re, JMfDtor_orig);
// 0x000026c0  MappedFile::deletingDtor (jgld.dll)
SG_HOOK("jgld.dll", 0x000026c0, jgld_MappedFile_deletingDtor, JMfDelDtor_re, JMfDelDtor_orig);
// 0x00068da0  Font::release (jgld.dll)
SG_HOOK("jgld.dll", 0x00068da0, jgld_Font_release, FontRelease_re, FontRelease_orig);
