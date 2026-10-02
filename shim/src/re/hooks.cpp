#include "hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../deps/minhook/MinHook.h"

void ShimLog(const char* fmt, ...);
const char* ShimDir();

static SgHook* s_head;

SgHook::SgHook(const char* m, DWORD a, const char* n, void* d, void** o)
    : module(m), addr(a), name(n), detour(d), original(o), installed(false), next(s_head) {
    s_head = this;
}

static bool Disabled(const SgHook& h) {
    char key[16], v[2048];
    snprintf(key, sizeof(key), "%08lx", h.addr);
    if (GetEnvironmentVariableA("SIMGOLF_HOOKS_OFF", v, sizeof(v))) {
        if (_stricmp(v, "all") == 0) return true;
        for (char* t = strtok(v, ","); t; t = strtok(nullptr, ","))
            if (strtoul(t, nullptr, 16) == h.addr) return true;
    }
    char ini[MAX_PATH];
    snprintf(ini, sizeof(ini), "%s\\simgolf_shim.ini", ShimDir());
    return GetPrivateProfileIntA("hooks", key, 1, ini) == 0;
}

void SgHooksInstall() {
    int n = 0, on = 0;
    for (SgHook* h = s_head; h; h = h->next) {
        ++n;
        if (Disabled(*h)) {
            ShimLog("re-hook %s %s+%08lx: OFF", h->name, h->module, h->addr);
            continue;
        }
        HMODULE m = GetModuleHandleA(h->module);
        if (!m) {
            ShimLog("re-hook %s: module %s not loaded at init (deferred hooks not supported yet)", h->name, h->module);
            continue;
        }
        BYTE* target = (_stricmp(h->module, "golf_clean.exe") == 0) ? (BYTE*)(ULONG_PTR)h->addr : (BYTE*)m + h->addr;
        MH_STATUS s = MH_CreateHook(target, h->detour, h->original);
        if (s == MH_OK) s = MH_EnableHook(target);
        h->installed = (s == MH_OK);
        on += h->installed;
        ShimLog("re-hook %s %s+%08lx: %s", h->name, h->module, h->addr, h->installed ? "ON" : MH_StatusToString(s));
    }
    ShimLog("re-hooks: %d registered, %d installed", n, on);
}

// Frida A/B support: returns 1 and fills detour/original (trampoline) if a hook is registered at addr.
extern "C" int __stdcall SimGolfShim_FindHook(DWORD addr, void** detour, void** original, int* installed) {
    for (SgHook* h = s_head; h; h = h->next) {
        if (h->addr != addr) continue;
        if (detour) *detour = h->detour;
        if (original) *original = h->installed ? *h->original : nullptr;
        if (installed) *installed = h->installed;
        return 1;
    }
    return 0;
}
