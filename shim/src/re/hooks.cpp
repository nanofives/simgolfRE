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
        // a token is an address, or module:address (DLL RVAs collide across jgld.dll, sound.dll and Terrain.dll)
        for (char* t = strtok(v, ","); t; t = strtok(nullptr, ",")) {
            char* colon = strchr(t, ':');
            if (colon) {
                *colon = 0;
                if (_stricmp(t, h.module) == 0 && strtoul(colon + 1, nullptr, 16) == h.addr) return true;
            } else if (strtoul(t, nullptr, 16) == h.addr) {
                return true;
            }
        }
    }
    char ini[MAX_PATH];
    snprintf(ini, sizeof(ini), "%s\\simgolf_shim.ini", ShimDir());
    return GetPrivateProfileIntA("hooks", key, 1, ini) == 0;
}

// Installs one hook if its module is mapped; returns false (and leaves it pending) when it is not yet.
static bool InstallOne(SgHook* h) {
    HMODULE m = GetModuleHandleA(h->module);
    if (!m) return false;
    BYTE* target = (_stricmp(h->module, "golf_clean.exe") == 0) ? (BYTE*)(ULONG_PTR)h->addr : (BYTE*)m + h->addr;
    MH_STATUS s = MH_CreateHook(target, h->detour, h->original);
    if (s == MH_OK) s = MH_EnableHook(target);
    h->installed = (s == MH_OK);
    ShimLog("re-hook %s %s+%08lx: %s", h->name, h->module, h->addr, h->installed ? "ON" : MH_StatusToString(s));
    return true;
}

static CRITICAL_SECTION s_lock;
static bool s_pending;   // some enabled hook waits for its module (jgld.dll and sound.dll are LoadLibrary'd later)

void SgHooksInstall() {
    InitializeCriticalSection(&s_lock);
    int n = 0, on = 0, wait = 0;
    for (SgHook* h = s_head; h; h = h->next) {
        ++n;
        h->tried = false;
        if (Disabled(*h)) {
            ShimLog("re-hook %s %s+%08lx: OFF", h->name, h->module, h->addr);
            h->tried = true;
            continue;
        }
        if (InstallOne(h)) { h->tried = true; on += h->installed; }
        else ++wait;
    }
    s_pending = wait > 0;
    ShimLog("re-hooks: %d registered, %d installed, %d waiting for their module", n, on, wait);
}

// Called after every successful LoadLibraryA (patches.cpp): installs the hooks whose module just appeared.
void SgHooksInstallPending() {
    if (!s_pending) return;
    EnterCriticalSection(&s_lock);
    int wait = 0;
    for (SgHook* h = s_head; h; h = h->next) {
        if (h->tried) continue;
        if (InstallOne(h)) h->tried = true;
        else ++wait;
    }
    s_pending = wait > 0;
    LeaveCriticalSection(&s_lock);
}

// Module-aware lookup for DLL hooks (their RVAs collide across modules): module is "jgld.dll", "Terrain.dll", ...
extern "C" int __stdcall SimGolfShim_FindHookM(const char* module, DWORD addr, void** detour, void** original, int* installed) {
    for (SgHook* h = s_head; h; h = h->next) {
        if (h->addr != addr || _stricmp(h->module, module) != 0) continue;
        if (detour) *detour = h->detour;
        if (original) *original = h->installed ? *h->original : nullptr;
        if (installed) *installed = h->installed;
        return 1;
    }
    return 0;
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
