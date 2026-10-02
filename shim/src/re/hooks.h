// Registry for reverse-engineered reimplementations (gta-reversed style).
//
//   // 0x0045baf0
//   static int __stdcall WinMain_re(HINSTANCE, HINSTANCE, LPSTR, int);
//   static decltype(&WinMain_re) WinMain_orig;
//   SG_HOOK("golf_clean.exe", 0x0045baf0, WinMain, WinMain_re, WinMain_orig);
//
// Rules (enforced by scripts/re_classify.py for C3):
//   * one SG_HOOK per RVA, and the RVA also appears as a `// 0x00xxxxxx` comment above the body;
//   * every hook is runtime-toggleable: [hooks] 0045baf0=0 in simgolf_shim.ini, or
//     SIMGOLF_HOOKS_OFF=0045baf0,00412345 / SIMGOLF_HOOKS_OFF=all in the environment;
//   * the trampoline to the original stays reachable (SimGolfShim_FindHook), which is what the
//     Frida A/B diff (re/frida/diff_hook.py) calls for the "original" arm.
//
// The address is a VA for golf_clean.exe (image base 0x00400000, no relocations) and an RVA for DLLs.
#pragma once
#include <windows.h>

struct SgHook {
    const char* module;
    DWORD addr;
    const char* name;
    void* detour;
    void** original;
    bool installed;
    SgHook* next;
    SgHook(const char* m, DWORD a, const char* n, void* d, void** o);
};

#define SG_HOOK(module, addr, name, detour, original) \
    static SgHook s_sghook_##name(module, addr, #name, (void*)(detour), (void**)&(original))

void SgHooksInstall();  // called by the shim after its compat hooks
