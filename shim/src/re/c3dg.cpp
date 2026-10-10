// Batch c3dg (round 25, jgld.dll): rows c3dc dropped for Win32 imports, plus the MappedFile writers c3cy left.
// Every import runs under the import stand-in (re/frida/js/importstandin.js) for the length of a key, so the
// call sequence (which import, in which order, with which arguments) is compared state and the results are the
// fixture's.
//
//   group M - MappedFile writers     openWrite 0x00002a20   create 0x00002bd0   closeTruncate 0x00002ef0
//   group P - the module palette     realizePalette 0x000699c0   onPaletteChanged 0x00069a70
//                                    deleteLogPalette 0x0006ad60   createPalette 0x0006aa70
//                                    Display::createPalette 0x00067e80
//   group F - the font DC            initFonts 0x00068f30   Display::initFonts 0x00067e00
//   group B - GDI blits              Surface::stretchTo 0x00008f70   Display::present 0x00066470
//   group K - onKey 0x00069c70 (the event sink 0x1012872c, the display 0x10128420, PeekMessageA)
//   group E - Display::enumModes 0x00065a90 (EnumDisplaySettingsA into the two mode arrays)
//   group S - Sprite::ctorN 0x00014cd0, Sprite::dtor 0x00014de0, Factory::createSprite 0x00067a90,
//             Factory::createPalette 0x00067910 (tracked list, critical sections, operator new)
//
// RULE for every body here (c3cy's): an import is called through jgld's OWN import slot (base + slot RVA), never
// through the shim's import table: the shim is another module, so the stand-in would not see the call and the
// real API would get fixture values. Slots (pefile over original\jgld.dll, log/c3/c3dg_jgld_iat.txt).
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form the debug
// build's disassembly prints. jgld.dll is relocated at run time, so every module address (vtables, globals,
// callees, import slots) is resolved against the running base. __thiscall is emulated with __fastcall (ecx = this,
// edx unused). Callees run through their original addresses (their incremental-link thunks, as the originals call
// them), so a callee hooked by another batch runs that batch's body.
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
template <class T> T& G(unsigned va) { return *static_cast<T*>(JgldVA(va)); }
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }
// The import whose slot is at VA `slotVa` (`call dword ptr [slotVa]` in the original).
template <class T> T Imp(unsigned slotVa) { return *static_cast<T*>(JgldVA(slotVa)); }
// Slot `off` of the vtable of `obj` (`mov edx, [obj]; call [edx + off]`).
template <class T> T Virt(void* obj, unsigned off) { return F<T>(F<void*>(obj, 0), off); }

// ---- jgld import slots (VAs)
const unsigned kIatCloseHandle = 0x1012c428, kIatUnmapViewOfFile = 0x1012c430, kIatCreateFileA = 0x1012c434;
const unsigned kIatSetFilePointer = 0x1012c438, kIatSetEndOfFile = 0x1012c43c, kIatMapViewOfFile = 0x1012c440;
const unsigned kIatGetLastError = 0x1012c444, kIatCreateFileMappingA = 0x1012c448, kIatGetFileSize = 0x1012c4c0;
const unsigned kIatGetDC = 0x1012c5ec, kIatReleaseDC = 0x1012c630, kIatSelectPalette = 0x1012c3b8;
const unsigned kIatRealizePalette = 0x1012c36c, kIatDeleteObject = 0x1012c370, kIatCreatePalette = 0x1012c3b0;
const unsigned kIatCreateCompatibleDC = 0x1012c388, kIatStretchBlt = 0x1012c3bc, kIatBitBlt = 0x1012c3a8;
const unsigned kIatPeekMessageA = 0x1012c63c, kIatEnumDisplaySettingsA = 0x1012c60c;
const unsigned kIatInitializeCriticalSection = 0x1012c41c, kIatDeleteCriticalSection = 0x1012c424;

typedef int(__stdcall* Api1_t)(void*);                                         // CloseHandle, DeleteObject...
typedef void*(__stdcall* CreateFileA_t)(const char*, unsigned, unsigned, void*, unsigned, unsigned, void*);
typedef void*(__stdcall* CreateFileMappingA_t)(void*, void*, unsigned, unsigned, unsigned, const char*);
typedef void*(__stdcall* MapViewOfFile_t)(void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__stdcall* GetFileSize_t)(void*, unsigned*);
typedef unsigned(__stdcall* SetFilePointer_t)(void*, int, int*, unsigned);
typedef unsigned(__stdcall* NoArgs_t)();
typedef void*(__stdcall* GetDC_t)(void*);
typedef int(__stdcall* ReleaseDC_t)(void*, void*);
typedef void*(__stdcall* SelectPalette_t)(void*, void*, int);
typedef unsigned(__stdcall* RealizePalette_t)(void*);
typedef void*(__stdcall* CreatePalette_t)(void*);
typedef void*(__stdcall* CreateCompatibleDC_t)(void*);
typedef int(__stdcall* StretchBlt_t)(void*, int, int, int, int, void*, int, int, int, int, unsigned);
typedef int(__stdcall* BitBlt_t)(void*, int, int, int, int, void*, int, int, unsigned);
typedef int(__stdcall* PeekMessageA_t)(void*, void*, unsigned, unsigned, unsigned);
typedef int(__stdcall* EnumDisplaySettingsA_t)(const char*, unsigned, void*);
typedef void(__stdcall* CritSec_t)(void*);

// ---- jgld callees, by the incremental-link thunk each original calls (thunk -> body)
const unsigned kThunkMappedFileClose = 0x100018bb;    // -> MappedFile::close 0x10002e10
const unsigned kMemset = 0x1007e7c0;                  // CRT memset (called directly)
const unsigned kThunkRealizePalette = 0x10001028;     // -> realizePalette 0x100699c0
const unsigned kThunkJalloc = 0x100017c6;             // -> jalloc 0x10014a40
const unsigned kThunkJfree = 0x100017b7;              // -> jfree 0x10014a90
const unsigned kThunkCreatePalette = 0x100019d3;      // -> createPalette 0x1006aa70
const unsigned kThunkDeleteLogPalette = 0x1000129e;   // -> deleteLogPalette 0x1006ad60
const unsigned kThunkInitFonts = 0x10001659;          // -> initFonts 0x10068f30
const unsigned kThunkInitFontTable = 0x10001217;      // -> initFontTable 0x10068fa0
const unsigned kThunkRectHeight = 0x1000173a;         // -> rectHeight 0x10009160
const unsigned kThunkRectWidth = 0x10001794;          // -> rectWidth 0x10009120
const unsigned kThunkIntersect = 0x10001087;          // -> intersect 0x10008590
const unsigned kThunkRec14Reset = 0x1000116d;         // -> Array_Rec14::reset 0x10066ac0
const unsigned kThunkRec94Reset = 0x1000160e;         // -> Array_Rec94::reset 0x10066b90
const unsigned kThunkRec14Add = 0x1000119f;           // -> Array_Rec14::add 0x100681a0
const unsigned kThunkRec94Add = 0x10001375;           // -> Array_Rec94::add 0x10068400
const unsigned kThunkSpriteBaseCtor = 0x100017c1;     // -> SpriteBase::ctor 0x10014c70
const unsigned kThunkSpriteRelease = 0x100015c3;      // -> Sprite::release 0x10014eb0
const unsigned kThunkSpriteBaseDtor = 0x100017df;     // -> SpriteBase::~SpriteBase 0x100168e0
const unsigned kThunkPaletteCtor = 0x1000191a;        // -> Palette::ctor 0x1006a150
const unsigned kThunkSpriteCtorN = 0x100018de;        // -> Sprite::ctorN 0x10014cd0
const unsigned kOperatorNew = 0x1007f0f0;             // operator new (called directly)

typedef void(__fastcall* Method_t)(void*, void*);
typedef int(__fastcall* IntMethod_t)(void*, void*);
typedef void*(__fastcall* PtrMethod_t)(void*, void*);
typedef void(__fastcall* MethodU_t)(void*, void*, unsigned);
typedef void*(__fastcall* CtorP_t)(void*, void*, void*);
typedef void*(__cdecl* Memset_t)(void*, int, unsigned);
typedef void*(__cdecl* Alloc_t)(unsigned);
typedef void(__cdecl* Free_t)(void*);
typedef int(__cdecl* Cdecl0_t)();
typedef int(__cdecl* Cdecl1_t)(void*);
typedef int(__cdecl* Cdecl2_t)(void*, void*);
typedef int(__cdecl* RectFn_t)(const void*);
typedef int(__cdecl* Intersect_t)(void*, const void*, const void*);

void CloseMf(void* self) { reinterpret_cast<Method_t>(JgldVA(kThunkMappedFileClose))(self, 0); }

// ---- module globals (VAs)
const unsigned kPalette = 0x10128738;        // the module's HPALETTE (createPalette 0x1006ac42)
const unsigned kPalClientA = 0x1012873c;     // createPalette's first argument (0x1006aca6)
const unsigned kPalClientB = 0x10128740;     // createPalette's second argument (0x1006acae)
const unsigned kFontDC = 0x10128724;         // the font memory DC (initFonts 0x10068f5e)
const unsigned kEventSink = 0x1012872c;      // the Lib / event sink onKey forwards to (0x10069c92)
const unsigned kDisplay = 0x10128420;        // the module Display (onKey 0x10069cbc)
const unsigned kModes14 = 0x101286c8;        // Array_Rec14 of display modes (enumModes 0x10065ac6)
const unsigned kModes94 = 0x101286f8;        // Array_Rec94 of DEVMODEs (enumModes 0x10065ad0)

// =====================================================================================================================
// group M - MappedFile writers. Object layout (MappedFile::close 0x10002e10, c3cy): +0 vtable, +4 view,
// +8 file handle (-1 = none), +0xc mapping handle, +0x10 size.
// =====================================================================================================================

// 0x00002a20  MappedFile::openWrite(this, name, sequential): open an existing file for read/write and map all of
// it. The flags word starts at FILE_ATTRIBUTE_NORMAL 0x80 (0x10002a3d) and gains FILE_FLAG_SEQUENTIAL_SCAN
// 0x08000000 when `sequential` is non-zero (`cmp [ebp+0xc], 0` 0x10002a44, `or eax, 0x8000000` 0x10002a4d) or
// FILE_FLAG_RANDOM_ACCESS 0x10000000 otherwise (0x10002a5a). Whatever the object held is closed first
// (MappedFile::close through 0x100018bb, 0x10002a66). CreateFileA (name, GENERIC_READ|GENERIC_WRITE 0xc0000000,
// share 0, no security, OPEN_EXISTING 3, flags, no template) 0x10002a88 goes to +8; INVALID_HANDLE_VALUE (`cmp
// [eax+8], -1` 0x10002a9e) calls GetLastError 0x10002aa6 and returns 0 whatever it says (both arms of the
// `cmp [ebp-0x10], 2` at 0x10002ab6 are `xor eax, eax`). CreateFileMappingA (file, NULL, PAGE_READWRITE 4, 0, 0,
// NULL) 0x10002add goes to +0xc; NULL (0x10002af3) closes and returns 0. MapViewOfFile (mapping,
// FILE_MAP_ALL_ACCESS 0xf001f, 0, 0, 0) 0x10002b19 goes to +4; NULL (0x10002b2f) closes and returns 0. Otherwise
// GetFileSize (file, NULL) 0x10002b4c goes to +0x10 and the view is returned (0x10002b62); `ret 8`.
typedef void*(__fastcall* MfOpenWrite_t)(void*, void*, const char*, int);
MfOpenWrite_t MfOpenWrite_orig;
void* __fastcall MfOpenWrite_re(void* self, void*, const char* name, int sequential) {
    unsigned flags = 0x80;                                                       // 0x10002a3d
    if (sequential != 0)                                                         // 0x10002a44 / 0x10002a48
        flags |= 0x08000000;                                                     // 0x10002a4d
    else
        flags |= 0x10000000;                                                     // 0x10002a5a
    CloseMf(self);                                                               // 0x10002a66
    F<void*>(self, 8) = Imp<CreateFileA_t>(kIatCreateFileA)(name, 0xc0000000, 0, 0, 3, flags, 0);   // 0x10002a88
    if (F<int>(self, 8) == -1) {                                                 // 0x10002a9e / 0x10002aa2
        Imp<NoArgs_t>(kIatGetLastError)();                                       // 0x10002aa6 (result unused)
        return 0;                                                                // 0x10002abc / 0x10002ac3
    }
    F<void*>(self, 0xc) =
        Imp<CreateFileMappingA_t>(kIatCreateFileMappingA)(F<void*>(self, 8), 0, 4, 0, 0, 0);       // 0x10002add
    if (F<void*>(self, 0xc) == 0) {                                              // 0x10002af3 / 0x10002af7
        CloseMf(self);                                                           // 0x10002afc
        return 0;
    }
    F<void*>(self, 4) = Imp<MapViewOfFile_t>(kIatMapViewOfFile)(F<void*>(self, 0xc), 0xf001f, 0, 0, 0); // 0x10002b19
    if (F<void*>(self, 4) == 0) {                                                // 0x10002b2f / 0x10002b33
        CloseMf(self);                                                           // 0x10002b38
        return 0;
    }
    F<unsigned>(self, 0x10) = Imp<GetFileSize_t>(kIatGetFileSize)(F<void*>(self, 8), 0);          // 0x10002b4c
    return F<void*>(self, 4);                                                    // 0x10002b62
}

// 0x00002bd0  MappedFile::create(this, name, size, sequential): create (or truncate) a file of `size` bytes,
// map it and zero the view. Flags as openWrite from `sequential` at [ebp+0x10] (0x10002bf4..0x10002c0a); close
// first (0x10002c16); +0x10 = size (0x10002c21) before anything can fail. CreateFileA (name, 0xc0000000, 0, NULL,
// CREATE_ALWAYS 2, flags, NULL) 0x10002c3b goes to +8; -1 (0x10002c51) calls GetLastError 0x10002c59 and returns 0
// (both arms of `cmp [ebp-0xc], 2` 0x10002c69 return 0). SetFilePointer (file, size, NULL, FILE_BEGIN 0)
// 0x10002c8e: -1 (`cmp eax, -1` 0x10002c9b) closes and returns 0. SetEndOfFile (file) 0x10002cb8, result unused.
// SetFilePointer (file, 0, NULL, FILE_BEGIN) 0x10002cd4: -1 (0x10002ce1) closes and returns 0. CreateFileMappingA
// (file, NULL, PAGE_READWRITE, 0, 0, NULL) 0x10002d08 -> +0xc, NULL (0x10002d1e) closes and returns 0;
// MapViewOfFile (mapping, 0xf001f, 0, 0, 0) 0x10002d44 -> +4, NULL (0x10002d5a) closes and returns 0. Otherwise
// memset 0x1007e7c0 (view, 0, size) 0x10002d79 and the view is returned (0x10002d84); `ret 0xc`.
typedef void*(__fastcall* MfCreate_t)(void*, void*, const char*, unsigned, int);
MfCreate_t MfCreate_orig;
void* __fastcall MfCreate_re(void* self, void*, const char* name, unsigned size, int sequential) {
    unsigned flags = 0x80;                                                       // 0x10002bed
    if (sequential != 0)                                                         // 0x10002bf4 / 0x10002bf8
        flags |= 0x08000000;                                                     // 0x10002bfd
    else
        flags |= 0x10000000;                                                     // 0x10002c0a
    CloseMf(self);                                                               // 0x10002c16
    F<unsigned>(self, 0x10) = size;                                              // 0x10002c21
    F<void*>(self, 8) = Imp<CreateFileA_t>(kIatCreateFileA)(name, 0xc0000000, 0, 0, 2, flags, 0);   // 0x10002c3b
    if (F<int>(self, 8) == -1) {                                                 // 0x10002c51 / 0x10002c55
        Imp<NoArgs_t>(kIatGetLastError)();                                       // 0x10002c59
        return 0;                                                                // 0x10002c6f / 0x10002c76
    }
    SetFilePointer_t setFilePointer = Imp<SetFilePointer_t>(kIatSetFilePointer);
    if (setFilePointer(F<void*>(self, 8), static_cast<int>(size), 0, 0) == 0xffffffffu) {   // 0x10002c8e / 0x10002c9b
        CloseMf(self);                                                           // 0x10002ca3
        return 0;
    }
    Imp<Api1_t>(kIatSetEndOfFile)(F<void*>(self, 8));                            // 0x10002cb8
    if (setFilePointer(F<void*>(self, 8), 0, 0, 0) == 0xffffffffu) {             // 0x10002cd4 / 0x10002ce1
        CloseMf(self);                                                           // 0x10002ce9
        return 0;
    }
    F<void*>(self, 0xc) =
        Imp<CreateFileMappingA_t>(kIatCreateFileMappingA)(F<void*>(self, 8), 0, 4, 0, 0, 0);       // 0x10002d08
    if (F<void*>(self, 0xc) == 0) {                                              // 0x10002d1e / 0x10002d22
        CloseMf(self);                                                           // 0x10002d27
        return 0;
    }
    F<void*>(self, 4) = Imp<MapViewOfFile_t>(kIatMapViewOfFile)(F<void*>(self, 0xc), 0xf001f, 0, 0, 0); // 0x10002d44
    if (F<void*>(self, 4) == 0) {                                                // 0x10002d5a / 0x10002d5e
        CloseMf(self);                                                           // 0x10002d63
        return 0;
    }
    reinterpret_cast<Memset_t>(JgldVA(kMemset))(F<void*>(self, 4), 0, size);     // 0x10002d79
    return F<void*>(self, 4);                                                    // 0x10002d84
}

// 0x00002ef0  MappedFile::closeTruncate(this, end): close the mapping and cut the file at `end`, a pointer into
// the view. An `end` below the view (unsigned `cmp ecx, [eax+4]` / `jae` 0x10002f13 / 0x10002f16) only closes
// (0x10002f1b). Otherwise the length is end - view (0x10002f2b); a non-NULL view is unmapped (UnmapViewOfFile
// 0x10002f43, +4 = 0 0x10002f53); a non-NULL mapping is closed (CloseHandle 0x10002f6c, +0xc = 0 0x10002f7c);
// SetFilePointer (file, length, NULL, FILE_BEGIN) 0x10002f94 returning -1 (0x10002fa1) closes (0x10002fa9) and
// stops; otherwise SetEndOfFile (file) 0x10002fb9, and a file handle that is not ZERO (`cmp [edx+8], 0`
// 0x10002fc9; the -1 "no file" value used everywhere else passes this test) is closed (CloseHandle 0x10002fd8)
// and +8 becomes 0 (0x10002fe8), not -1. `ret 4`; no return value is set.
typedef void(__fastcall* MfCloseTruncate_t)(void*, void*, char*);
MfCloseTruncate_t MfCloseTruncate_orig;
void __fastcall MfCloseTruncate_re(void* self, void*, char* end) {
    if (reinterpret_cast<unsigned>(end) < F<unsigned>(self, 4)) {                // 0x10002f13 / 0x10002f16
        CloseMf(self);                                                           // 0x10002f1b
        return;
    }
    const unsigned length = reinterpret_cast<unsigned>(end) - F<unsigned>(self, 4);   // 0x10002f2b
    if (F<void*>(self, 4) != 0) {                                                // 0x10002f34 / 0x10002f38
        Imp<Api1_t>(kIatUnmapViewOfFile)(F<void*>(self, 4));                     // 0x10002f43
        F<void*>(self, 4) = 0;                                                   // 0x10002f53
    }
    if (F<void*>(self, 0xc) != 0) {                                              // 0x10002f5d / 0x10002f61
        Imp<Api1_t>(kIatCloseHandle)(F<void*>(self, 0xc));                       // 0x10002f6c
        F<void*>(self, 0xc) = 0;                                                 // 0x10002f7c
    }
    if (Imp<SetFilePointer_t>(kIatSetFilePointer)(F<void*>(self, 8), static_cast<int>(length), 0, 0) ==
        0xffffffffu) {                                                           // 0x10002f94 / 0x10002fa1
        CloseMf(self);                                                           // 0x10002fa9
        return;
    }
    Imp<Api1_t>(kIatSetEndOfFile)(F<void*>(self, 8));                            // 0x10002fb9
    if (F<void*>(self, 8) != 0) {                                                // 0x10002fc9 / 0x10002fcd
        Imp<Api1_t>(kIatCloseHandle)(F<void*>(self, 8));                         // 0x10002fd8
        F<void*>(self, 8) = 0;                                                   // 0x10002fe8
    }
}

// =====================================================================================================================
// group P - the module palette
// =====================================================================================================================

// 0x000699c0  realizePalette(hwnd): GetDC (hwnd) 0x100699de; SelectPalette (dc, the module palette at VA
// 0x10128738, bForceBackground 0) 0x100699fd; RealizePalette (dc) 0x10069a10; ReleaseDC (hwnd, dc) 0x10069a27;
// returns 1 (0x10069a34). No result is tested. cdecl.
typedef int(__cdecl* RealizePal_t)(void*);
RealizePal_t RealizePal_orig;
int __cdecl RealizePal_re(void* hwnd) {
    void* dc = Imp<GetDC_t>(kIatGetDC)(hwnd);                                    // 0x100699de
    Imp<SelectPalette_t>(kIatSelectPalette)(dc, G<void*>(kPalette), 0);          // 0x100699f2 / 0x100699fd
    Imp<RealizePalette_t>(kIatRealizePalette)(dc);                               // 0x10069a10
    Imp<ReleaseDC_t>(kIatReleaseDC)(hwnd, dc);                                   // 0x10069a27
    return 1;                                                                    // 0x10069a34
}

// 0x00069a70  onPaletteChanged(hwnd, changer): the WM_PALETTECHANGED handler (windowProc 0x100692b0 passes hwnd
// and wParam). When the window that changed the palette is another one (`cmp eax, [ebp+0xc]` / `je` 0x10069a8b /
// 0x10069a8e), realizePalette 0x100699c0 through its thunk 0x10001028 (0x10069a94) for hwnd. cdecl; no return
// value is set (windowProc returns 0 after it).
typedef void(__cdecl* OnPalChanged_t)(void*, void*);
OnPalChanged_t OnPalChanged_orig;
void __cdecl OnPalChanged_re(void* hwnd, void* changer) {
    if (hwnd != changer)                                                         // 0x10069a8b / 0x10069a8e
        reinterpret_cast<Cdecl1_t>(JgldVA(kThunkRealizePalette))(hwnd);          // 0x10069a94
}

// 0x0006ad60  deleteLogPalette(): a non-NULL module palette (`cmp [0x10128738], 0` 0x1006ad78) goes to
// DeleteObject 0x1006ad89 and the global is cleared (0x1006ad96). cdecl, no arguments.
typedef void(__cdecl* DeleteLogPal_t)();
DeleteLogPal_t DeleteLogPal_orig;
void __cdecl DeleteLogPal_re() {
    if (G<void*>(kPalette) != 0) {                                               // 0x1006ad78 / 0x1006ad7f
        Imp<Api1_t>(kIatDeleteObject)(G<void*>(kPalette));                       // 0x1006ad89
        G<void*>(kPalette) = 0;                                                  // 0x1006ad96
    }
}

// 0x0006aa70  createPalette(a, b): build a 256-entry LOGPALETTE and make it the module palette.
//   First deleteLogPalette 0x1006ad60 (thunk 0x1000129e, 0x1006aa88) releases the previous module palette and
//   clears VA 0x10128738, whatever happens next.
//   jalloc 0x10014a40 (through 0x100017c6) of 0x404 bytes (0x1006aa8d); NULL (0x1006aa9d) returns 4.
//   palVersion 0x300 (0x1006aab0), palNumEntries 0x100 (0x1006aab8).
//   Entries 0..9 (loop 0x1006aad0, i < 0xa) and 246..255 (+0x3dc = 4 + 246*4) take the twenty system colours
//   from the 4-byte quads at 0x1011dab0 and 0x1011dad8 with the byte order swapped (peRed = quad byte 2,
//   peGreen = byte 1, peBlue = byte 0: 0x1006aae3 / 0x1006aaf7 / 0x1006ab0b and 0x1006ab2a / 0x1006ab41 /
//   0x1006ab58) and flags 0 (0x1006ab1c / 0x1006ab6c).
//   Entries 10..245 (loop 0x1006ab8b, i < 0xf6) only get flags 5 = PC_RESERVED | PC_NOCOLLAPSE (0x1006ab9a);
//   their colour bytes stay whatever jalloc returned.
//   Then entries 8, 9 and 246, 247 (loop 0x1006abb3, i < 2) are overwritten as PC_EXPLICIT (flags 2) entries
//   whose low byte is a hardware index: 8 + i (0x1006abc5) and 0xf5 + i (0x1006abf9), other bytes 0.
//   CreatePalette 0x1006ac35 goes to VA 0x10128738 (0x1006ac42). NULL (0x1006ac47): jfree (through 0x100017b7,
//   0x1006ac54), return 1. Otherwise jfree (0x1006ac6e), then slot 0 of a's vtable (0x1006ac87) and of b's
//   (0x1006ac9a), a goes to VA 0x1012873c (0x1006aca6), b to VA 0x10128740 (0x1006acae), return 0. cdecl.
typedef int(__cdecl* CreatePal_t)(void*, void*);
CreatePal_t CreatePal_orig;
int __cdecl CreatePal_re(void* a, void* b) {
    reinterpret_cast<Cdecl0_t>(JgldVA(kThunkDeleteLogPalette))();               // 0x1006aa88
    unsigned char* lp = static_cast<unsigned char*>(
        reinterpret_cast<Alloc_t>(JgldVA(kThunkJalloc))(0x404));                // 0x1006aa8d / 0x1006aa92
    if (lp == 0)                                                                 // 0x1006aa9d / 0x1006aaa1
        return 4;                                                                // 0x1006aaa3
    F<unsigned short>(lp, 0) = 0x300;                                            // 0x1006aab0
    F<unsigned short>(lp, 2) = 0x100;                                            // 0x1006aab8
    const unsigned char* low = static_cast<const unsigned char*>(JgldVA(0x1011dab0));
    const unsigned char* high = static_cast<const unsigned char*>(JgldVA(0x1011dad8));
    for (int i = 0; i < 0xa; ++i) {                                              // 0x1006aad0 / 0x1006aad4
        lp[4 + i * 4 + 0] = low[i * 4 + 2];                                      // 0x1006aae3 / 0x1006aaea
        lp[4 + i * 4 + 1] = low[i * 4 + 1];                                      // 0x1006aaf7 / 0x1006aafe
        lp[4 + i * 4 + 2] = low[i * 4 + 0];                                      // 0x1006ab0b / 0x1006ab12
        lp[4 + i * 4 + 3] = 0;                                                   // 0x1006ab1c
        lp[0x3dc + i * 4 + 0] = high[i * 4 + 2];                                 // 0x1006ab2a / 0x1006ab31
        lp[0x3dc + i * 4 + 1] = high[i * 4 + 1];                                 // 0x1006ab41 / 0x1006ab48
        lp[0x3dc + i * 4 + 2] = high[i * 4 + 0];                                 // 0x1006ab58 / 0x1006ab5f
        lp[0x3dc + i * 4 + 3] = 0;                                               // 0x1006ab6c
    }
    for (int i = 0xa; i < 0xf6; ++i)                                             // 0x1006ab8b / 0x1006ab92
        lp[4 + i * 4 + 3] = 5;                                                   // 0x1006ab9a
    for (int i = 0; i < 2; ++i) {                                                // 0x1006abb3 / 0x1006abb7
        lp[0x24 + i * 4 + 0] = static_cast<unsigned char>(i + 8);                // 0x1006abc5
        lp[0x24 + i * 4 + 1] = 0;                                                // 0x1006abcf
        lp[0x24 + i * 4 + 2] = 0;                                                // 0x1006abda
        lp[0x24 + i * 4 + 3] = 2;                                                // 0x1006abe5
        lp[0x3dc + i * 4 + 0] = static_cast<unsigned char>(i + 0xf5);            // 0x1006abf9
        lp[0x3dc + i * 4 + 1] = 0;                                               // 0x1006ac06
        lp[0x3dc + i * 4 + 2] = 0;                                               // 0x1006ac14
        lp[0x3dc + i * 4 + 3] = 2;                                               // 0x1006ac22
    }
    G<void*>(kPalette) = Imp<CreatePalette_t>(kIatCreatePalette)(lp);            // 0x1006ac35 / 0x1006ac42
    Free_t jfree = reinterpret_cast<Free_t>(JgldVA(kThunkJfree));
    if (G<void*>(kPalette) == 0) {                                               // 0x1006ac47 / 0x1006ac4e
        jfree(lp);                                                               // 0x1006ac54
        return 1;                                                                // 0x1006ac63
    }
    jfree(lp);                                                                   // 0x1006ac6e
    Virt<Method_t>(a, 0)(a, 0);                                                  // 0x1006ac87
    Virt<Method_t>(b, 0)(b, 0);                                                  // 0x1006ac9a
    G<void*>(kPalClientA) = a;                                                   // 0x1006aca6
    G<void*>(kPalClientB) = b;                                                   // 0x1006acae
    return 0;                                                                    // 0x1006acb4
}

// 0x00067e80  Display::createPalette(this, a, b): slot 48 (+0xc0) of the Display vtable 0x1011d640. Calls
// createPalette 0x1006aa70 through its thunk 0x100019d3 with (a, b) (0x10067ea5) and returns its result, which
// is still in eax at the `ret 8` (0x10067ebd). `this` is not read.
typedef int(__fastcall* DispCreatePal_t)(void*, void*, void*, void*);
DispCreatePal_t DispCreatePal_orig;
int __fastcall DispCreatePal_re(void*, void*, void* a, void* b) {
    return reinterpret_cast<Cdecl2_t>(JgldVA(kThunkCreatePalette))(a, b);        // 0x10067ea5
}

// =====================================================================================================================
// group F - the font DC
// =====================================================================================================================

// 0x00068f30  initFonts(): initFontTable 0x10068fa0 through its thunk 0x10001217 (0x10068f48; it deletes a
// previous font DC at VA 0x10128724 with DeleteDC and clears it), then CreateCompatibleDC (NULL) 0x10068f51 goes
// to VA 0x10128724 (0x10068f5e); NULL there (0x10068f63) returns 2 (0x10068f6c), otherwise 0 (0x10068f73). cdecl.
typedef int(__cdecl* InitFonts_t)();
InitFonts_t InitFonts_orig;
int __cdecl InitFonts_re() {
    reinterpret_cast<Cdecl0_t>(JgldVA(kThunkInitFontTable))();                   // 0x10068f48
    G<void*>(kFontDC) = Imp<CreateCompatibleDC_t>(kIatCreateCompatibleDC)(0);    // 0x10068f51 / 0x10068f5e
    if (G<void*>(kFontDC) == 0)                                                  // 0x10068f63 / 0x10068f6a
        return 2;                                                                // 0x10068f6c
    return 0;                                                                    // 0x10068f73
}

// 0x00067e00  Display::initFonts(this): slot 46 (+0xb8) of the Display vtable 0x1011d640. Calls initFonts
// 0x10068f30 through its thunk 0x10001659 (0x10067e1d) and returns with its result still in eax (no instruction
// between the call and the `ret` at 0x10067e32 writes eax). `this` is not read.
typedef int(__fastcall* DispInitFonts_t)(void*, void*);
DispInitFonts_t DispInitFonts_orig;
int __fastcall DispInitFonts_re(void*, void*) {
    return reinterpret_cast<Cdecl0_t>(JgldVA(kThunkInitFonts))();                // 0x10067e1d
}

// =====================================================================================================================
// group B - GDI blits. Surface slots used: +0x28 acquire a DC (returns it), +0x2c release it (one argument, 1
// here), +0xd4 the surface's bounds RECT, +0xec select a palette.
// =====================================================================================================================

// 0x00008f70  Surface::stretchTo(this, dst, srcRect, dstRect): slot 16 (+0x40) of the Surface vtable 0x1011d0b0.
// Any NULL argument (0x10008f93 / 0x10008f99 / 0x10008f9f) returns 0x10. The destination DC comes from dst's
// slot +0x28 (0x10008fb9); NULL (0x10008fc6) returns 7. The source DC from this's slot +0x28 (0x10008fe0); NULL
// (0x10008fed) releases dst's DC (slot +0x2c with 1, 0x10008fff) and returns 7. Otherwise StretchBlt 0x10009070
// (dstDC, dstRect.left, dstRect.top, width(dstRect), height(dstRect), srcDC, srcRect.left, srcRect.top,
// width(srcRect), height(srcRect), SRCCOPY 0xcc0020), with rectHeight 0x10009160 (thunk 0x1000173a) of each rect
// evaluated before its rectWidth 0x10009120 (thunk 0x10001794): srcRect's pair at 0x1000901e / 0x1000902b,
// dstRect's at 0x10009049 / 0x10009056. Then dst's slot +0x2c (1) 0x10009089 and this's 0x1000909f; return 0
// (0x100090a9); `ret 0xc`.
typedef int(__fastcall* StretchTo_t)(void*, void*, void*, const int*, const int*);
StretchTo_t StretchTo_orig;
int __fastcall StretchTo_re(void* self, void*, void* dst, const int* srcRect, const int* dstRect) {
    if (dst == 0 || srcRect == 0 || dstRect == 0)                                // 0x10008f93 / 0x10008f99 / 0x10008f9f
        return 0x10;                                                             // 0x10008fa5
    void* dstDC = Virt<PtrMethod_t>(dst, 0x28)(dst, 0);                          // 0x10008fb9
    if (dstDC == 0)                                                              // 0x10008fc6 / 0x10008fca
        return 7;                                                                // 0x10008fcc
    void* srcDC = Virt<PtrMethod_t>(self, 0x28)(self, 0);                        // 0x10008fe0
    if (srcDC == 0) {                                                            // 0x10008fed / 0x10008ff1
        Virt<MethodU_t>(dst, 0x2c)(dst, 0, 1);                                   // 0x10008fff
        return 7;                                                                // 0x10009009
    }
    RectFn_t rectHeight = reinterpret_cast<RectFn_t>(JgldVA(kThunkRectHeight));
    RectFn_t rectWidth = reinterpret_cast<RectFn_t>(JgldVA(kThunkRectWidth));
    const int sh = rectHeight(srcRect);                                          // 0x1000901e
    const int sw = rectWidth(srcRect);                                           // 0x1000902b
    const int dh = rectHeight(dstRect);                                          // 0x10009049
    const int dw = rectWidth(dstRect);                                           // 0x10009056
    Imp<StretchBlt_t>(kIatStretchBlt)(dstDC, dstRect[0], dstRect[1], dw, dh,
                                      srcDC, srcRect[0], srcRect[1], sw, sh, 0xcc0020);   // 0x10009070
    Virt<MethodU_t>(dst, 0x2c)(dst, 0, 1);                                       // 0x10009089
    Virt<MethodU_t>(self, 0x2c)(self, 0, 1);                                     // 0x1000909f
    return 0;                                                                    // 0x100090a9
}

// 0x00066470  Display::present(this, rect): slot 41 (+0xa4) of the Display vtable 0x1011d640. The back surface is
// [[this+0x148]+4] (0x10066490 / 0x10066496). Its DC comes from its slot +0x28 (0x100664a6); its slot +0xec gets
// the +4 field of the palette client at VA 0x1012873c (0x100664b5 / 0x100664c6). A NULL rect (`cmp [ebp+8], 0`
// 0x100664d3) blits the whole surface: BitBlt 0x10066538 (this+0x138, 0, 0, width, height, surfaceDC, 0, 0,
// SRCCOPY) where height = rectHeight of the surface's slot +0xd4 RECT (0x100664f2 / 0x10066500) is evaluated
// before width = rectWidth of a second +0xd4 call (0x10066513 / 0x10066521). Otherwise intersect 0x10008590 (thunk
// 0x10001087; out = a local RECT, rect, the +0xd4 RECT fetched at 0x10066551) 0x10066567; non-zero (`test eax,
// eax` 0x1006656f) blits the intersection: BitBlt 0x100665b2 (this+0x138, l.left, l.top, width(l), height(l),
// surfaceDC, l.left, l.top, SRCCOPY), height (0x1006658a) before width (0x10066597). Both paths end with the
// surface's slot +0x2c (1) 0x100665cb; return 0 (0x100665d5); `ret 4`.
typedef int(__fastcall* Present_t)(void*, void*, const int*);
Present_t Present_orig;
int __fastcall Present_re(void* self, void*, const int* rect) {
    void* surface = F<void*>(F<void*>(self, 0x148), 4);                          // 0x10066490 / 0x10066496
    void* dc = Virt<PtrMethod_t>(surface, 0x28)(surface, 0);                     // 0x100664a6
    Virt<MethodU_t>(surface, 0xec)(surface, 0, F<unsigned>(G<void*>(kPalClientA), 4));   // 0x100664b5 / 0x100664c6
    RectFn_t rectHeight = reinterpret_cast<RectFn_t>(JgldVA(kThunkRectHeight));
    RectFn_t rectWidth = reinterpret_cast<RectFn_t>(JgldVA(kThunkRectWidth));
    if (rect == 0) {                                                             // 0x100664d3 / 0x100664d7
        const int h = rectHeight(Virt<PtrMethod_t>(surface, 0xd4)(surface, 0));  // 0x100664f2 / 0x10066500
        const int w = rectWidth(Virt<PtrMethod_t>(surface, 0xd4)(surface, 0));   // 0x10066513 / 0x10066521
        Imp<BitBlt_t>(kIatBitBlt)(F<void*>(self, 0x138), 0, 0, w, h, dc, 0, 0, 0xcc0020);   // 0x10066538
    } else {
        int local[4];
        void* bounds = Virt<PtrMethod_t>(surface, 0xd4)(surface, 0);             // 0x10066551
        if (reinterpret_cast<Intersect_t>(JgldVA(kThunkIntersect))(local, rect, bounds) != 0) {   // 0x10066567 / 0x1006656f
            const int h = rectHeight(local);                                     // 0x1006658a
            const int w = rectWidth(local);                                      // 0x10066597
            Imp<BitBlt_t>(kIatBitBlt)(F<void*>(self, 0x138), local[0], local[1], w, h,
                                      dc, local[0], local[1], 0xcc0020);         // 0x100665b2
        }
    }
    Virt<MethodU_t>(surface, 0x2c)(surface, 0, 1);                               // 0x100665cb
    return 0;                                                                    // 0x100665d5
}

// =====================================================================================================================
// group K - onKey
// =====================================================================================================================

// 0x00069c70  onKey(hwnd, key, down): the WM_KEYDOWN/KEYUP/SYSKEYDOWN/SYSKEYUP handler (windowProc passes five
// arguments; the body reads three). Slot +0xc of the event sink at VA 0x1012872c gets (key, down) (0x10069ca0).
// On a key-up (`cmp [ebp+0x10], 0` / `je` 0x10069caa / 0x10069cae) nothing else happens. On a key-down, a key in
// 0x60..0x6f (VK_NUMPAD0..VK_DIVIDE; unsigned `jb` 0x10069cb4 below 0x60, `jbe` 0x10069cba up to 0x6f) drains
// the WM_CHAR messages the key produced: PeekMessageA (&msg, hwnd, 0x102, 0x102, PM_REMOVE 1) 0x10069cf7 until it
// returns 0 (0x10069d04); any other key pumps the display at VA 0x10128420: its slot +0x138 (0x10069ccc) until it
// returns 0 (0x10069cd9). cdecl; no return value is set.
typedef void(__cdecl* OnKey_t)(void*, unsigned, int);
OnKey_t OnKey_orig;
void __cdecl OnKey_re(void* hwnd, unsigned key, int down) {
    void* sink = G<void*>(kEventSink);
    reinterpret_cast<void(__fastcall*)(void*, void*, unsigned, int)>(F<void*>(F<void*>(sink, 0), 0xc))(
        sink, 0, key, down);                                                     // 0x10069c92 / 0x10069ca0
    if (down == 0)                                                               // 0x10069caa / 0x10069cae
        return;
    if (key < 0x60 || key > 0x6f) {                                              // 0x10069cb4 / 0x10069cba
        for (;;) {
            void* display = G<void*>(kDisplay);                                  // 0x10069cbc
            if (Virt<IntMethod_t>(display, 0x138)(display, 0) == 0)              // 0x10069ccc / 0x10069cd9
                break;
        }
        return;
    }
    unsigned char msg[0x1c];                                                     // [ebp-0x1c]
    while (Imp<PeekMessageA_t>(kIatPeekMessageA)(msg, hwnd, 0x102, 0x102, 1) != 0)   // 0x10069cf7 / 0x10069d04
        ;
}

// =====================================================================================================================
// group E - Display::enumModes
// =====================================================================================================================

// 0x00065a90  Display::enumModes(this): slot 26 (+0x68) of the Display vtable 0x1011d640. A DEVMODEA local at
// [ebp-0x98] gets dmSize 0x94 (word at +0x24, 0x10065ab3), dmFields 0 (+0x28, 0x10065ab9) and dmDriverExtra 0
// (+0x26, 0x10065ac0); nothing else of it is initialised. The module's two mode arrays are emptied:
// Array_Rec14::reset on VA 0x101286c8 (thunk 0x1000116d, 0x10065acb) and Array_Rec94::reset on VA 0x101286f8
// (thunk 0x1000160e, 0x10065ad5). Then EnumDisplaySettingsA (NULL, i, &devmode) for i = 0, 1, ... (the counter
// is pushed before its increment, 0x10065aff / 0x10065b0b; call 0x10065b11) until it returns 0 (0x10065b1e). For
// each mode a 5-dword record {dmPelsWidth +0x6c, dmPelsHeight +0x70, dmPelsWidth again, dmBitsPerPel +0x68,
// dmDisplayFrequency +0x78} (0x10065b22..0x10065b49) is passed BY VALUE to Array_Rec14::add on 0x101286c8 (thunk
// 0x1000119f, 0x10065b66) and the whole 0x94-byte DEVMODE by value to Array_Rec94::add on 0x101286f8 (thunk
// 0x10001375, 0x10065b85). Returns VA 0x101286c8 (0x10065b8f).
struct Rec14 { unsigned v[5]; };
struct Rec94 { unsigned char b[0x94]; };
typedef void(__fastcall* Rec14Add_t)(void*, void*, Rec14);
typedef void(__fastcall* Rec94Add_t)(void*, void*, Rec94);
typedef void*(__fastcall* EnumModes_t)(void*, void*);
EnumModes_t EnumModes_orig;
void* __fastcall EnumModes_re(void*, void*) {
    Rec94 dm;                                                                    // [ebp-0x98]
    F<unsigned short>(&dm, 0x24) = 0x94;                                         // 0x10065ab3
    F<unsigned>(&dm, 0x28) = 0;                                                  // 0x10065ab9
    F<unsigned short>(&dm, 0x26) = 0;                                            // 0x10065ac0
    reinterpret_cast<Method_t>(JgldVA(kThunkRec14Reset))(JgldVA(kModes14), 0);   // 0x10065acb
    reinterpret_cast<Method_t>(JgldVA(kThunkRec94Reset))(JgldVA(kModes94), 0);   // 0x10065ad5
    for (unsigned i = 0;; ++i) {                                                 // 0x10065ada / 0x10065b0b
        if (Imp<EnumDisplaySettingsA_t>(kIatEnumDisplaySettingsA)(0, i, &dm) == 0)   // 0x10065b11 / 0x10065b1e
            break;
        Rec14 r;
        r.v[3] = F<unsigned>(&dm, 0x68);                                         // 0x10065b22 / 0x10065b25
        r.v[0] = F<unsigned>(&dm, 0x6c);                                         // 0x10065b2b / 0x10065b2e
        r.v[1] = F<unsigned>(&dm, 0x70);                                         // 0x10065b34 / 0x10065b37
        r.v[2] = F<unsigned>(&dm, 0x6c);                                         // 0x10065b3d / 0x10065b40
        r.v[4] = F<unsigned>(&dm, 0x78);                                         // 0x10065b46 / 0x10065b49
        reinterpret_cast<Rec14Add_t>(JgldVA(kThunkRec14Add))(JgldVA(kModes14), 0, r);   // 0x10065b66
        reinterpret_cast<Rec94Add_t>(JgldVA(kThunkRec94Add))(JgldVA(kModes94), 0, dm);  // 0x10065b85
    }
    return JgldVA(kModes14);                                                     // 0x10065b8f
}

// =====================================================================================================================
// group S - Sprite construction and destruction, the two factory slots
// =====================================================================================================================

// 0x00014cd0  Sprite::ctorN(this, owner): SpriteBase::ctor 0x10014c70 (thunk 0x100017c1, 0x10014cf0; it runs
// Tracked::ctor, which adds `this` to the tracked-object list 0x10128428, and writes the SpriteBase vtable), then
// the Sprite vtable 0x1011d380 at +0 (0x10014cf8), owner at +0xc (0x10014d04), zero at +8, +0x10, +0x14, +0x18,
// +0x1c, +0x20, +0x24 (0x10014d0a..0x10014d46), 0xff at +0x28 (0x10014d50), zero at +0x2c, +0x30, +0x34
// (0x10014d5a..0x10014d6e), InitializeCriticalSection (this+0x38) 0x10014d7e; returns this (0x10014d8b), `ret 4`.
typedef void*(__fastcall* SpriteCtorN_t)(void*, void*, void*);
SpriteCtorN_t SpriteCtorN_orig;
void* __fastcall SpriteCtorN_re(void* self, void*, void* owner) {
    reinterpret_cast<Method_t>(JgldVA(kThunkSpriteBaseCtor))(self, 0);          // 0x10014cf0
    F<void*>(self, 0) = JgldVA(0x1011d380);                                      // 0x10014cf8
    F<void*>(self, 0xc) = owner;                                                 // 0x10014d04
    F<unsigned>(self, 8) = 0;                                                    // 0x10014d0a
    F<unsigned>(self, 0x10) = 0;                                                 // 0x10014d14
    F<unsigned>(self, 0x14) = 0;                                                 // 0x10014d1e
    F<unsigned>(self, 0x18) = 0;                                                 // 0x10014d28
    F<unsigned>(self, 0x1c) = 0;                                                 // 0x10014d32
    F<unsigned>(self, 0x20) = 0;                                                 // 0x10014d3c
    F<unsigned>(self, 0x24) = 0;                                                 // 0x10014d46
    F<unsigned>(self, 0x28) = 0xff;                                              // 0x10014d50
    F<unsigned>(self, 0x2c) = 0;                                                 // 0x10014d5a
    F<unsigned>(self, 0x30) = 0;                                                 // 0x10014d64
    F<unsigned>(self, 0x34) = 0;                                                 // 0x10014d6e
    Imp<CritSec_t>(kIatInitializeCriticalSection)(static_cast<char*>(self) + 0x38);   // 0x10014d7e
    return self;                                                                 // 0x10014d8b
}

// 0x00014de0  Sprite::dtor(this): the Sprite vtable 0x1011d380 at +0 (0x10014e15); when the owner slot pointer at
// +0xc is non-NULL (0x10014e25 / 0x10014e29) the dword it points to is cleared (0x10014e31); DeleteCriticalSection
// (this+0x38) 0x10014e40; Sprite::release 0x10014eb0 (thunk 0x100015c3, 0x10014e50; frees the payloads and lowers
// the sprite memory counters); SpriteBase::~SpriteBase 0x100168e0 (thunk 0x100017df, 0x10014e5f; unregisters
// `this` from the tracked list). The SEH frame (0x10014de3..0x10014e67) only records the unwind state.
typedef void(__fastcall* SpriteDtor_t)(void*, void*);
SpriteDtor_t SpriteDtor_orig;
void __fastcall SpriteDtor_re(void* self, void*) {
    F<void*>(self, 0) = JgldVA(0x1011d380);                                      // 0x10014e15
    if (F<unsigned*>(self, 0xc) != 0)                                            // 0x10014e25 / 0x10014e29
        *F<unsigned*>(self, 0xc) = 0;                                            // 0x10014e31
    Imp<CritSec_t>(kIatDeleteCriticalSection)(static_cast<char*>(self) + 0x38);  // 0x10014e40
    reinterpret_cast<Method_t>(JgldVA(kThunkSpriteRelease))(self, 0);           // 0x10014e50
    reinterpret_cast<Method_t>(JgldVA(kThunkSpriteBaseDtor))(self, 0);          // 0x10014e5f
}

// 0x00067a90  Factory::createSprite(this, owner, unused): slot 32 (+0x80) of the Display vtable 0x1011d640.
// operator new 0x1007f0f0 (0x50 bytes, 0x10067ac2 / 0x10067ac4); a non-NULL block (0x10067ad6 / 0x10067ada) is
// built by Sprite::ctorN 0x10014cd0 (thunk 0x100018de, 0x10067ae3) with `owner` and that result is returned,
// otherwise 0 (0x10067aed). The second argument is popped (`ret 8` 0x10067b1e) and never read.
typedef void*(__fastcall* CreateSprite_t)(void*, void*, void*, unsigned);
CreateSprite_t CreateSprite_orig;
void* __fastcall CreateSprite_re(void*, void*, void* owner, unsigned) {
    void* p = reinterpret_cast<Alloc_t>(JgldVA(kOperatorNew))(0x50);             // 0x10067ac4
    if (p == 0)                                                                  // 0x10067ad6 / 0x10067ada
        return 0;                                                                // 0x10067aed
    return reinterpret_cast<CtorP_t>(JgldVA(kThunkSpriteCtorN))(p, 0, owner);    // 0x10067ae3
}

// 0x00067910  Factory::createPalette(this, owner): slot 30 (+0x78) of the Display vtable 0x1011d640. operator new
// 0x1007f0f0 (0x820 bytes, 0x10067942 / 0x10067947); a non-NULL block (0x10067959 / 0x1006795d) is built by
// Palette::ctor 0x1006a150 (thunk 0x1000191a, 0x10067966) with `owner` and that result is returned, otherwise 0
// (0x10067970); `ret 4`.
typedef void*(__fastcall* CreatePalObj_t)(void*, void*, void*);
CreatePalObj_t CreatePalObj_orig;
void* __fastcall CreatePalObj_re(void*, void*, void* owner) {
    void* p = reinterpret_cast<Alloc_t>(JgldVA(kOperatorNew))(0x820);            // 0x10067947
    if (p == 0)                                                                  // 0x10067959 / 0x1006795d
        return 0;                                                                // 0x10067970
    return reinterpret_cast<CtorP_t>(JgldVA(kThunkPaletteCtor))(p, 0, owner);    // 0x10067966
}


// =====================================================================================================================
// group C - Sprite::create and its two depth bodies. Sprite fields written (re/match/jgld_sprite.cpp layout):
// +0x14 payload, +0x18 flags (bit 0 = row-span form), +0x1c row-span table {BYTE start; BYTE len; WORD off;} per
// row, +0x20 bits per pixel, +0x24 payload size, +0x28 key colour (read), +0x2c / +0x30 width, +0x34 height.
// Module counters: VA 0x101284c4 (non-zero enables the span form), 0x101284c8 (pixels allocated), 0x101284cc
// (bytes the span form saved).
// =====================================================================================================================

const unsigned kSpanEnable = 0x101284c4, kSprPixels = 0x101284c8, kSprSaved = 0x101284cc;
const unsigned kMemcpy = 0x1007f3a0;          // CRT memcpy (called directly)
const unsigned kExpand = 0x10080610;          // CRT _expand (called directly)
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);
typedef void*(__cdecl* Expand_t)(void*, unsigned);

struct RowSpan { unsigned char start; unsigned char len; unsigned short off; };

// 0x0001b690  Sprite::create8(this, bits, w, h, bpp, e): `ret 0x14`; bpp and e are not read.
//   Slot +8 of this's vtable (Sprite::release 0x10014eb0 on the Sprite vtable) empties the sprite (0x1001b6b7).
//   +0x30 = w (0x1001b6c7), +0x34 = h (0x1001b6d0), +0x20 = 8 (0x1001b6d6), +0x24 = w * h (0x1001b6e7).
//   jalloc (thunk 0x100017c6) of +0x24 bytes (0x1001b6f1) -> +0x14; NULL (0x1001b702) returns 4.
//   VA 0x101284c8 += w * h (0x1001b719..0x1001b721). bits NULL (0x1001b727) goes to the end: +0x2c = w, return 0.
//   Span form when VA 0x101284c4 != 0 (0x1001b731), w > 4 unsigned (`jbe` 0x1001b742), w < 0x100 signed (`jge`
//   0x1001b74f) and w * h < 0x10000 signed (`jge` 0x1001b762); otherwise the flat form.
//   Span form: budget = w * h - h * 4 (0x1001b768..0x1001b777); jalloc (h * 4) -> +0x1c (0x1001b781); NULL
//   (0x1001b792) calls slot +8 again (0x1001b7a2) and returns 4. For each row y (signed `jge` 0x1001b7dd):
//     start = first x (signed x < w, 0x1001b7fe) whose byte differs from +0x28 (0x1001b810), stored as a byte
//     (0x1001b825); then x runs down from w - 1 while x > start (signed `jle` 0x1001b84d) and the byte equals the
//     key (0x1001b85c); len = x + 1 - start, stored as a byte (0x1001b884).
//     A non-zero len (0x1001b897): when budget < len (unsigned `jae` 0x1001b8b1) the table goes to jfree (thunk
//     0x100017b7, 0x1001b8ba), +0x1c = 0 (0x1001b8c5) and the flat form runs instead (0x1001b8cc -> 0x1001b9eb ->
//     0x1001b9b3). Otherwise off = dst - payload (0x1001b8e3), memcpy 0x1007f3a0 (dst, row + start, len)
//     (0x1001b910), budget -= len (0x1001b92c), dst += len (0x1001b943).
//     The row pointer advances by w (0x1001b949).
//   After the rows: +0x18 |= 1 (0x1001b95a), +0x24 = w * h - budget (0x1001b96f), _expand 0x10080610 (payload,
//   +0x24) (0x1001b980) -> +0x14 (0x1001b98b), VA 0x101284cc += w * h - +0x24 - h * 4 (0x1001b98e..0x1001b9ab).
//   Flat form: +0x18 &= ~1 (0x1001b9b9), memcpy (payload, bits, w * h) (0x1001b9d4).
//   End: +0x2c = w (0x1001b9e2), return 0 (0x1001b9e5).
typedef int(__fastcall* Create8_t)(void*, void*, const unsigned char*, unsigned, int, int, int);
Create8_t Create8_orig;
int __fastcall Create8_re(void* self, void*, const unsigned char* bits, unsigned w, int h, int, int) {
    Virt<Method_t>(self, 8)(self, 0);                                            // 0x1001b6b7
    F<unsigned>(self, 0x30) = w;                                                 // 0x1001b6c7
    F<int>(self, 0x34) = h;                                                      // 0x1001b6d0
    F<int>(self, 0x20) = 8;                                                      // 0x1001b6d6
    F<unsigned>(self, 0x24) = w * h;                                             // 0x1001b6e7
    Alloc_t jalloc = reinterpret_cast<Alloc_t>(JgldVA(kThunkJalloc));
    F<unsigned char*>(self, 0x14) = static_cast<unsigned char*>(jalloc(F<unsigned>(self, 0x24)));   // 0x1001b6f1
    if (F<unsigned char*>(self, 0x14) == 0)                                      // 0x1001b702 / 0x1001b706
        return 4;                                                                // 0x1001b708
    G<unsigned>(kSprPixels) += w * h;                                            // 0x1001b719 / 0x1001b721
    if (bits != 0) {                                                             // 0x1001b727 / 0x1001b72b
        bool flat = true;
        if (G<int>(kSpanEnable) != 0 && w > 4 && static_cast<int>(w) < 0x100 &&
            static_cast<int>(w * h) < 0x10000) {                                 // 0x1001b731..0x1001b762
            flat = false;
            unsigned budget = w * h - h * 4;                                     // 0x1001b768..0x1001b777
            F<RowSpan*>(self, 0x1c) = static_cast<RowSpan*>(jalloc(h * 4));      // 0x1001b781
            if (F<RowSpan*>(self, 0x1c) == 0) {                                  // 0x1001b792 / 0x1001b796
                Virt<Method_t>(self, 8)(self, 0);                                // 0x1001b7a2
                return 4;                                                        // 0x1001b7ac
            }
            unsigned char* dst = F<unsigned char*>(self, 0x14);                  // 0x1001b7b9
            const unsigned char* row = bits;                                     // 0x1001b7c2
            for (int y = 0; y < h; ++y) {                                        // 0x1001b7da / 0x1001b7dd
                RowSpan* sp = F<RowSpan*>(self, 0x1c) + y;
                int x = 0;
                while (x < static_cast<int>(w) && row[x] == F<unsigned>(self, 0x28))   // 0x1001b7fe / 0x1001b810
                    ++x;
                sp->start = static_cast<unsigned char>(x);                       // 0x1001b825
                x = static_cast<int>(w) - 1;                                     // 0x1001b82b
                while (x > sp->start && row[x] == F<unsigned>(self, 0x28))       // 0x1001b84d / 0x1001b85c
                    --x;
                sp->len = static_cast<unsigned char>(x + 1 - sp->start);         // 0x1001b879 / 0x1001b884
                if (sp->len != 0) {                                              // 0x1001b897 / 0x1001b899
                    if (budget < sp->len) {                                      // 0x1001b8ae / 0x1001b8b1
                        reinterpret_cast<Free_t>(JgldVA(kThunkJfree))(F<void*>(self, 0x1c));   // 0x1001b8ba
                        F<void*>(self, 0x1c) = 0;                                // 0x1001b8c5
                        flat = true;                                             // 0x1001b8cc -> 0x1001b9b3
                        break;
                    }
                    sp->off = static_cast<unsigned short>(dst - F<unsigned char*>(self, 0x14));   // 0x1001b8e3
                    reinterpret_cast<Memcpy_t>(JgldVA(kMemcpy))(dst, row + sp->start, sp->len);   // 0x1001b910
                    budget -= sp->len;                                           // 0x1001b92c
                    dst += sp->len;                                              // 0x1001b943
                }
                row += w;                                                        // 0x1001b949
            }
            if (!flat) {
                F<unsigned>(self, 0x18) |= 1;                                    // 0x1001b95a
                F<unsigned>(self, 0x24) = w * h - budget;                        // 0x1001b96f
                F<void*>(self, 0x14) = reinterpret_cast<Expand_t>(JgldVA(kExpand))(
                    F<void*>(self, 0x14), F<unsigned>(self, 0x24));              // 0x1001b980 / 0x1001b98b
                G<unsigned>(kSprSaved) += w * h - F<unsigned>(self, 0x24) - h * 4;   // 0x1001b98e..0x1001b9ab
            }
        }
        if (flat) {
            F<unsigned>(self, 0x18) &= ~1u;                                      // 0x1001b9b9
            reinterpret_cast<Memcpy_t>(JgldVA(kMemcpy))(F<void*>(self, 0x14), bits, w * h);   // 0x1001b9d4
        }
    }
    F<unsigned>(self, 0x2c) = w;                                                 // 0x1001b9e2
    return 0;                                                                    // 0x1001b9e5
}

// 0x0001a250  Sprite::create16(this, bits, w, h, bpp, e): create8's body for 16-bit pixels, with these
// differences (each read from the instructions):
//   +0x20 = 0x10 (0x1001a296); +0x24 = w * h * 2 (`shl edx, 1` 0x1001a2a4) is what jalloc gets (0x1001a2b3), but
//   VA 0x101284c8 still grows by w * h (0x1001a2d4..0x1001a2e3) and the span budget is still w * h - h * 4 in the
//   same units as a row's pixel count (0x1001a32a..0x1001a339).
//   The scans compare 16-bit pixels (`mov cx, word ptr [eax+edx*2]` 0x1001a3cd, 0x1001a41b) with +0x28.
//   off = (dst - payload) >> 1 (`sar edx, 1` 0x1001a4a0); memcpy (dst, row + start * 2, len) copies len BYTES
//   (0x1001a4b0..0x1001a4d9; the count pushed is the byte len, not len * 2); dst advances by len * 2 (0x1001a50a)
//   and the row by w * 2 (0x1001a516).
//   _expand gets +0x24 = w * h - budget (0x1001a53c / 0x1001a54d), a size in the budget's units; VA 0x101284cc +=
//   w * h - +0x24 - h * 4 (0x1001a55b..0x1001a578).
//   The flat form copies w * h BYTES (0x1001a58e..0x1001a5a1), half of the w * h * 2 payload.
typedef int(__fastcall* Create16_t)(void*, void*, const unsigned short*, unsigned, int, int, int);
Create16_t Create16_orig;
int __fastcall Create16_re(void* self, void*, const unsigned short* bits, unsigned w, int h, int, int) {
    Virt<Method_t>(self, 8)(self, 0);                                            // 0x1001a277
    F<unsigned>(self, 0x30) = w;                                                 // 0x1001a287
    F<int>(self, 0x34) = h;                                                      // 0x1001a290
    F<int>(self, 0x20) = 0x10;                                                   // 0x1001a296
    F<unsigned>(self, 0x24) = (w * h) << 1;                                      // 0x1001a2a0..0x1001a2a9
    Alloc_t jalloc = reinterpret_cast<Alloc_t>(JgldVA(kThunkJalloc));
    F<unsigned char*>(self, 0x14) = static_cast<unsigned char*>(jalloc(F<unsigned>(self, 0x24)));   // 0x1001a2b3
    if (F<unsigned char*>(self, 0x14) == 0)                                      // 0x1001a2c4 / 0x1001a2c8
        return 4;                                                                // 0x1001a2ca
    G<unsigned>(kSprPixels) += w * h;                                            // 0x1001a2db / 0x1001a2e3
    if (bits != 0) {                                                             // 0x1001a2e9 / 0x1001a2ed
        bool flat = true;
        if (G<int>(kSpanEnable) != 0 && w > 4 && static_cast<int>(w) < 0x100 &&
            static_cast<int>(w * h) < 0x10000) {                                 // 0x1001a2f3..0x1001a324
            flat = false;
            unsigned budget = w * h - h * 4;                                     // 0x1001a32a..0x1001a339
            F<RowSpan*>(self, 0x1c) = static_cast<RowSpan*>(jalloc(h * 4));      // 0x1001a343
            if (F<RowSpan*>(self, 0x1c) == 0) {                                  // 0x1001a354 / 0x1001a358
                Virt<Method_t>(self, 8)(self, 0);                                // 0x1001a364
                return 4;                                                        // 0x1001a36e
            }
            unsigned char* dst = F<unsigned char*>(self, 0x14);                  // 0x1001a37b
            const unsigned short* row = bits;                                    // 0x1001a384
            for (int y = 0; y < h; ++y) {                                        // 0x1001a39c / 0x1001a39f
                RowSpan* sp = F<RowSpan*>(self, 0x1c) + y;
                int x = 0;
                while (x < static_cast<int>(w) && row[x] == F<unsigned>(self, 0x28))   // 0x1001a3c0 / 0x1001a3d4
                    ++x;
                sp->start = static_cast<unsigned char>(x);                       // 0x1001a3e9
                x = static_cast<int>(w) - 1;                                     // 0x1001a3ec
                while (x > sp->start && row[x] == F<unsigned>(self, 0x28))       // 0x1001a411 / 0x1001a422
                    --x;
                sp->len = static_cast<unsigned char>(x + 1 - sp->start);         // 0x1001a43f / 0x1001a44a
                if (sp->len != 0) {                                              // 0x1001a45d / 0x1001a45f
                    if (budget < sp->len) {                                      // 0x1001a474 / 0x1001a477
                        reinterpret_cast<Free_t>(JgldVA(kThunkJfree))(F<void*>(self, 0x1c));   // 0x1001a480
                        F<void*>(self, 0x1c) = 0;                                // 0x1001a48b
                        flat = true;                                             // 0x1001a492 -> 0x1001a580
                        break;
                    }
                    sp->off = static_cast<unsigned short>(
                        (dst - F<unsigned char*>(self, 0x14)) >> 1);             // 0x1001a49d..0x1001a4ab
                    reinterpret_cast<Memcpy_t>(JgldVA(kMemcpy))(dst, row + sp->start, sp->len);   // 0x1001a4d9
                    budget -= sp->len;                                           // 0x1001a4f3
                    dst += sp->len * 2;                                          // 0x1001a50a
                }
                row += w;                                                        // 0x1001a516 (w * 2 bytes)
            }
            if (!flat) {
                F<unsigned>(self, 0x18) |= 1;                                    // 0x1001a527
                F<unsigned>(self, 0x24) = w * h - budget;                        // 0x1001a53c
                F<void*>(self, 0x14) = reinterpret_cast<Expand_t>(JgldVA(kExpand))(
                    F<void*>(self, 0x14), F<unsigned>(self, 0x24));              // 0x1001a54d / 0x1001a558
                G<unsigned>(kSprSaved) += w * h - F<unsigned>(self, 0x24) - h * 4;   // 0x1001a55b..0x1001a578
            }
        }
        if (flat) {
            F<unsigned>(self, 0x18) &= ~1u;                                      // 0x1001a586
            reinterpret_cast<Memcpy_t>(JgldVA(kMemcpy))(F<void*>(self, 0x14), bits, w * h);   // 0x1001a5a1
        }
    }
    F<unsigned>(self, 0x2c) = w;                                                 // 0x1001a5af
    return 0;                                                                    // 0x1001a5b2
}

// 0x00014ff0  Sprite::create(this, bits, w, h, bpp, e, key): slot 1 (+4) of the Sprite vtable 0x1011d380. bpp 8
// (`cmp [ebp-0xc], 8` 0x10015013) calls create8 0x1001b690 (thunk 0x10001825, 0x10015038) and bpp 0x10
// (0x10015019) create16 0x1001a250 (thunk 0x100015be, 0x10015059), each with (bits, w, h, bpp, e). Any other bpp
// calls neither, and the result local [ebp-8] keeps the prologue's /GZ fill 0xcccccccc. A non-zero result
// (0x10015061 / 0x10015065) is returned, so another bpp returns 0xcccccccc; zero calls slot +0x10 of this's
// vtable (Sprite::setField10 0x10017eb0 on the Sprite vtable) with `key` (0x1001507a) and returns 0
// (0x10015084). `ret 0x18`.
const unsigned kThunkCreate8 = 0x10001825, kThunkCreate16 = 0x100015be;
typedef int(__fastcall* CreateDepth_t)(void*, void*, const void*, unsigned, int, int, int);
typedef int(__fastcall* SpriteCreate_t)(void*, void*, const void*, unsigned, int, int, int, int);
SpriteCreate_t SpriteCreate_orig;
int __fastcall SpriteCreate_re(void* self, void*, const void* bits, unsigned w, int h, int bpp, int e, int key) {
    int r = static_cast<int>(0xcccccccc);                                        // [ebp-8], /GZ fill
    if (bpp == 8)                                                                // 0x10015013 / 0x10015017
        r = reinterpret_cast<CreateDepth_t>(JgldVA(kThunkCreate8))(self, 0, bits, w, h, bpp, e);    // 0x10015038
    else if (bpp == 0x10)                                                        // 0x10015019 / 0x1001501d
        r = reinterpret_cast<CreateDepth_t>(JgldVA(kThunkCreate16))(self, 0, bits, w, h, bpp, e);   // 0x10015059
    if (r != 0)                                                                  // 0x10015061 / 0x10015065
        return r;                                                                // 0x10015067
    Virt<void(__fastcall*)(void*, void*, int)>(self, 0x10)(self, 0, key);        // 0x1001507a
    return 0;                                                                    // 0x10015084
}

}  // namespace

// 0x00002a20  MappedFile::openWrite
SG_HOOK("jgld.dll", 0x00002a20, jgld_MappedFile_openWrite, MfOpenWrite_re, MfOpenWrite_orig);
// 0x00002bd0  MappedFile::create
SG_HOOK("jgld.dll", 0x00002bd0, jgld_MappedFile_create, MfCreate_re, MfCreate_orig);
// 0x00002ef0  MappedFile::closeTruncate
SG_HOOK("jgld.dll", 0x00002ef0, jgld_MappedFile_closeTruncate, MfCloseTruncate_re, MfCloseTruncate_orig);
// 0x000699c0  realizePalette
SG_HOOK("jgld.dll", 0x000699c0, jgld_realizePalette, RealizePal_re, RealizePal_orig);
// 0x00069a70  onPaletteChanged
SG_HOOK("jgld.dll", 0x00069a70, jgld_onPaletteChanged, OnPalChanged_re, OnPalChanged_orig);
// 0x0006ad60  deleteLogPalette
SG_HOOK("jgld.dll", 0x0006ad60, jgld_deleteLogPalette, DeleteLogPal_re, DeleteLogPal_orig);
// 0x0006aa70  createPalette
SG_HOOK("jgld.dll", 0x0006aa70, jgld_createPalette, CreatePal_re, CreatePal_orig);
// 0x00067e80  Display::createPalette
SG_HOOK("jgld.dll", 0x00067e80, jgld_Display_createPalette, DispCreatePal_re, DispCreatePal_orig);
// 0x00068f30  initFonts
SG_HOOK("jgld.dll", 0x00068f30, jgld_initFonts, InitFonts_re, InitFonts_orig);
// 0x00067e00  Display::initFonts
SG_HOOK("jgld.dll", 0x00067e00, jgld_Display_initFonts, DispInitFonts_re, DispInitFonts_orig);
// 0x00008f70  Surface::stretchTo
SG_HOOK("jgld.dll", 0x00008f70, jgld_Surface_stretchTo, StretchTo_re, StretchTo_orig);
// 0x00066470  Display::present
SG_HOOK("jgld.dll", 0x00066470, jgld_Display_present, Present_re, Present_orig);
// 0x00069c70  onKey
SG_HOOK("jgld.dll", 0x00069c70, jgld_onKey, OnKey_re, OnKey_orig);
// 0x00065a90  Display::enumModes
SG_HOOK("jgld.dll", 0x00065a90, jgld_Display_enumModes, EnumModes_re, EnumModes_orig);
// 0x00014cd0  Sprite::ctorN
SG_HOOK("jgld.dll", 0x00014cd0, jgld_Sprite_ctorN, SpriteCtorN_re, SpriteCtorN_orig);
// 0x00014de0  Sprite::dtor
SG_HOOK("jgld.dll", 0x00014de0, jgld_Sprite_dtor, SpriteDtor_re, SpriteDtor_orig);
// 0x00067a90  Factory::createSprite
SG_HOOK("jgld.dll", 0x00067a90, jgld_Factory_createSprite, CreateSprite_re, CreateSprite_orig);
// 0x00067910  Factory::createPalette
SG_HOOK("jgld.dll", 0x00067910, jgld_Factory_createPalette, CreatePalObj_re, CreatePalObj_orig);
// 0x0001b690  Sprite::create8
SG_HOOK("jgld.dll", 0x0001b690, jgld_Sprite_create8, Create8_re, Create8_orig);
// 0x0001a250  Sprite::create16
SG_HOOK("jgld.dll", 0x0001a250, jgld_Sprite_create16, Create16_re, Create16_orig);
// 0x00014ff0  Sprite::create
SG_HOOK("jgld.dll", 0x00014ff0, jgld_Sprite_create, SpriteCreate_re, SpriteCreate_orig);
