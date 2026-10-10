// NEGATIVE CONTROL for batch c3cy's import stand-in (re/frida/js/importstandin.js). NEVER list this file in
// shim/re_batches.txt: every body here is DELIBERATELY WRONG, and only in one import call, so that a diff_hook run
// proves the stand-in's call log is really compared state and not decoration. None of the defects changes a return
// value or a field the registry compares, so each key must read RED on the import log alone.
//
// Built instead of c3cy.cpp through log/c3/c3cy_negctl_batches.txt; driven by log/c3/alt/c3cy_negctl.py, whose CSVs
// go to log/c3/alt, never to log/diff.
//
//   0x00492e80  MappedFile::close (exe)  MISSING CALL: the mapping handle at +0xc is cleared but never handed to
//                                        CloseHandle [0x4ba0cc] (the original calls it at 0x00492ea7)
//   0x00492dd0  MappedFile::open (exe)   WRONG ARGUMENT: CreateFileA gets share mode 3 instead of 1 (FILE_SHARE_READ
//                                        pushed at 0x00492df8); the stand-in's results do not depend on it
//   0x00479b20  releaseCursorClip        EXTRA CALL: ClipCursor(NULL) also when this window is not the owner
//   0x00068da0  Font::release (jgld)     WRONG ARGUMENT: RemoveFontResourceA gets the name + 1 (0x10068df8 passes
//                                        the dword at +0x20 itself)
//   0x00486e40  Timer::startRepeat       WRONG ARGUMENT: timeSetEvent gets fuEvent 1 instead of 0 (`push 0`
//                                        0x00486e63); the stand-in's result does not depend on it
#include "hooks.h"

namespace {

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> static inline T Imp(unsigned slot) { return *reinterpret_cast<T*>(slot); }

typedef int(__stdcall* Handle1_t)(void* h);
typedef void*(__stdcall* CreateFileA_t)(const char*, unsigned, unsigned, void*, unsigned, unsigned, void*);
typedef void*(__stdcall* CreateFileMappingA_t)(void*, void*, unsigned, unsigned, unsigned, const char*);
typedef void*(__stdcall* MapViewOfFile_t)(void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__stdcall* GetFileSize_t)(void*, unsigned*);
typedef unsigned(__stdcall* NoArgs_t)();
typedef int(__stdcall* ClipCursor_t)(const void* rect);
typedef int(__stdcall* Ptr1_t)(void*);
typedef int(__stdcall* Str1_t)(const char*);
typedef int(__cdecl* Jfree_t)(void*);
typedef void(__fastcall* Void0_t)(void* ecx, void* edx);

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }

// 0x00492e80  MappedFile::close (WRONG: no CloseHandle for the mapping)
typedef void(__fastcall* MfClose_t)(void* ecx, void* edx);
MfClose_t MfClose_orig;
void __fastcall MfClose_re(void* self, void*) {
    if (field_ptr(self, 4) != 0) {
        Imp<Handle1_t>(0x004ba0d0)(field_ptr(self, 4));
        field_i(self, 4) = 0;
    }
    if (field_ptr(self, 0xc) != 0) field_i(self, 0xc) = 0;               // the CloseHandle is left out
    if (field_i(self, 8) != -1) {
        Imp<Handle1_t>(0x004ba0cc)(field_ptr(self, 8));
        field_i(self, 8) = -1;
    }
}

// 0x00492dd0  MappedFile::open (WRONG: share mode 3)
typedef void*(__fastcall* MfOpen_t)(void* ecx, void* edx, const char* name, int sequential);
MfOpen_t MfOpen_orig;
void* __fastcall MfOpen_re(void* self, void*, const char* name, int sequential) {
    unsigned flags = sequential != 0 ? 0x08000080u : 0x10000080u;
    reinterpret_cast<Void0_t>(0x00492e80)(self, 0);
    void* file = Imp<CreateFileA_t>(0x004ba0c0)(name, 0x80000000u, 3, 0, 3, flags, 0);   // 3, not 1
    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 8) = file;
    if (file == reinterpret_cast<void*>(-1)) {
        Imp<NoArgs_t>(0x004ba02c)();
        return 0;
    }
    void* mapping = Imp<CreateFileMappingA_t>(0x004ba0b8)(file, 0, 2, 0, 0, 0);
    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0xc) = mapping;
    if (mapping == 0) { reinterpret_cast<Void0_t>(0x00492e80)(self, 0); return 0; }
    void* view = Imp<MapViewOfFile_t>(0x004ba0b4)(mapping, 4, 0, 0, 0);
    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 4) = view;
    if (view == 0) { reinterpret_cast<Void0_t>(0x00492e80)(self, 0); return 0; }
    field_i(self, 0x10) = static_cast<int>(Imp<GetFileSize_t>(0x004ba0a0)(field_ptr(self, 8), 0));
    return field_ptr(self, 4);
}

// 0x00479b20  releaseCursorClip (WRONG: ClipCursor(NULL) for every window)
typedef void(__fastcall* ReleaseClip_t)(void* ecx, void* edx);
ReleaseClip_t ReleaseClip_orig;
void __fastcall ReleaseClip_re(void* self, void*) {
    void** owner = reinterpret_cast<void**>(0x0083aa98);
    Imp<ClipCursor_t>(0x004ba1d0)(0);                                      // called whatever the owner
    if (*owner == self) *owner = 0;
}

// 0x00068da0  Font::release (WRONG: RemoveFontResourceA(name + 1))
typedef void(__fastcall* FontRelease_t)(void* ecx, void* edx);
FontRelease_t FontRelease_orig;
void __fastcall FontRelease_re(void* self, void*) {
    if (field_ptr(self, 0x18) != 0) {
        (*reinterpret_cast<Ptr1_t*>(JgldVA(0x1012c370)))(field_ptr(self, 0x18));
        field_i(self, 0x18) = 0;
    }
    if (field_ptr(self, 0x20) != 0) {
        (*reinterpret_cast<Str1_t*>(JgldVA(0x1012c3c0)))(static_cast<const char*>(field_ptr(self, 0x20)) + 1);
        reinterpret_cast<Jfree_t>(JgldVA(0x100017b7))(field_ptr(self, 0x20));
        field_i(self, 0x20) = 0;
    }
}

// 0x00486e40  Timer::startRepeat (WRONG: timeSetEvent(..., 1))
typedef int(__fastcall* TimerStart_t)(void* ecx, void* edx);
typedef unsigned(__stdcall* TimeSetEvent_t)(unsigned, unsigned, void*, void*, unsigned);
typedef unsigned(__stdcall* SetTimer_t)(void*, void*, unsigned, void*);
typedef void*(__fastcall* DisplayHwnd_t)(void* ecx, void* edx);
TimerStart_t TimerStartRepeat_orig;
int __fastcall TimerStartRepeat_re(void* self, void*) {
    unsigned* f = static_cast<unsigned*>(self);
    if (f[0x10 / 4] == 0 && f[0x14 / 4] == 0) return 7;
    unsigned id;
    if (f[0x20 / 4] < 0x32) {
        id = Imp<TimeSetEvent_t>(0x004ba218)(f[0x20 / 4], f[0x28 / 4], reinterpret_cast<void*>(0x00486f90), self, 1);
    } else {
        f[8 / 4] = (f[8 / 4] & ~2u) | 1u;
        void* d = *reinterpret_cast<void**>(0x0083ad50);
        void* hwnd = reinterpret_cast<DisplayHwnd_t>((*reinterpret_cast<void***>(d))[0x20 / 4])(d, 0);
        id = Imp<SetTimer_t>(0x004ba1b8)(hwnd, self, f[0x20 / 4], reinterpret_cast<void*>(0x00486f40));
    }
    f[0xc / 4] = id;
    return id != 0 ? 0 : 2;
}

}  // namespace

// 0x00492e80  MappedFile::close (negative control)
SG_HOOK("golf_clean.exe", 0x00492e80, MappedFile_close, MfClose_re, MfClose_orig);
// 0x00492dd0  MappedFile::open (negative control)
SG_HOOK("golf_clean.exe", 0x00492dd0, MappedFile_open, MfOpen_re, MfOpen_orig);
// 0x00479b20  releaseCursorClip (negative control)
SG_HOOK("golf_clean.exe", 0x00479b20, releaseCursorClip, ReleaseClip_re, ReleaseClip_orig);
// 0x00068da0  Font::release (negative control)
SG_HOOK("jgld.dll", 0x00068da0, jgld_Font_release, FontRelease_re, FontRelease_orig);
// 0x00486e40  Timer::startRepeat (negative control)
SG_HOOK("golf_clean.exe", 0x00486e40, Timer_startRepeat, TimerStartRepeat_re, TimerStartRepeat_orig);
