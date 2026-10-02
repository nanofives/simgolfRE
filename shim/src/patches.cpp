// Signature-checked byte patches, applied in memory by the shim (golf_clean.exe on disk stays the
// pristine diffing reference -- never patch it in place).
//
// One patch per INI file in the patch directory (default <repo>\patches, override with
// SIMGOLF_PATCH_DIR or [shim] patch_dir=). Format:
//
//   [patch]
//   desc=What it changes and why (cite RVAs / analysis note)
//   module=golf_clean.exe        ; any module; DLLs loaded later (jgld.dll) are patched on load
//   rva=0xd3910                  ; offset from the module base
//   expect=53 69 64              ; bytes that must be there first (the signature)
//   write=53 49 44               ; replacement, same length as expect
//   enabled=0                    ; default state
//
// [patches] <name>=0/1 in simgolf_shim.ini overrides `enabled` (name = file name without .ini).
// SIMGOLF_PATCHES=name1,name2 force-enables a list (test harness).
// Every decision is logged: "patch <name>: applied | SKIP mismatch | SKIP disabled | pending <module>".

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include <vector>
#include <string>
#include "../deps/minhook/MinHook.h"

void ShimLog(const char* fmt, ...);
const char* ShimDir();

namespace {

struct Patch {
    std::string name, module, desc;
    DWORD rva = 0;
    std::vector<BYTE> expect, write;
    bool enabled = false;
    bool done = false;
};

std::vector<Patch> s_patches;
CRITICAL_SECTION s_lock;

bool ParseHex(const char* s, std::vector<BYTE>& out) {
    out.clear();
    while (*s) {
        while (*s == ' ' || *s == ',') ++s;
        if (!*s) break;
        if (!isxdigit((unsigned char)s[0]) || !isxdigit((unsigned char)s[1])) return false;
        char b[3] = {s[0], s[1], 0};
        out.push_back((BYTE)strtoul(b, nullptr, 16));
        s += 2;
    }
    return !out.empty();
}

bool ForcedOn(const std::string& name) {
    char v[1024];
    if (!GetEnvironmentVariableA("SIMGOLF_PATCHES", v, sizeof(v))) return false;
    for (char* tok = strtok(v, ","); tok; tok = strtok(nullptr, ","))
        if (_stricmp(tok, name.c_str()) == 0) return true;
    return false;
}

void TryApply(Patch& p) {
    if (p.done || !p.enabled) return;
    HMODULE m = GetModuleHandleA(p.module.c_str());
    if (!m) return;  // not loaded yet; LoadLibrary hook retries
    p.done = true;
    BYTE* at = (BYTE*)m + p.rva;
    if (memcmp(at, p.expect.data(), p.expect.size()) != 0) {
        ShimLog("patch %s: SKIP mismatch at %s+0x%lx (binary does not match the anchor)", p.name.c_str(), p.module.c_str(), p.rva);
        return;
    }
    DWORD old;
    VirtualProtect(at, p.write.size(), PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, p.write.data(), p.write.size());
    VirtualProtect(at, p.write.size(), old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, p.write.size());
    ShimLog("patch %s: applied %u bytes at %s+0x%lx (%s)", p.name.c_str(), (unsigned)p.write.size(), p.module.c_str(), p.rva, p.desc.c_str());
}

void ApplyPending() {
    EnterCriticalSection(&s_lock);
    for (auto& p : s_patches) TryApply(p);
    LeaveCriticalSection(&s_lock);
}

typedef HMODULE(WINAPI* LoadLibraryA_t)(LPCSTR);
LoadLibraryA_t o_LoadLibraryA;
HMODULE WINAPI h_LoadLibraryA(LPCSTR name) {
    HMODULE r = o_LoadLibraryA(name);
    if (r) ApplyPending();
    return r;
}

}  // namespace

void PatchesInit() {
    InitializeCriticalSection(&s_lock);
    char dir[MAX_PATH], v[MAX_PATH];
    char ini[MAX_PATH];
    snprintf(ini, sizeof(ini), "%s\\simgolf_shim.ini", ShimDir());
    if (GetEnvironmentVariableA("SIMGOLF_PATCH_DIR", v, sizeof(v)))
        lstrcpynA(dir, v, MAX_PATH);
    else {
        GetPrivateProfileStringA("shim", "patch_dir", "..\\patches", v, sizeof(v), ini);
        if (PathIsRelativeA(v)) {
            char tmp[MAX_PATH];
            snprintf(tmp, sizeof(tmp), "%s\\%s", ShimDir(), v);
            PathCanonicalizeA(dir, tmp);
        } else {
            lstrcpynA(dir, v, MAX_PATH);
        }
    }

    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.ini", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        ShimLog("patches: none in %s", dir);
        return;
    }
    do {
        char path[MAX_PATH], buf[4096];
        snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
        Patch p;
        p.name = fd.cFileName;
        p.name.resize(p.name.size() - 4);
        GetPrivateProfileStringA("patch", "module", "", buf, sizeof(buf), path); p.module = buf;
        GetPrivateProfileStringA("patch", "desc", "", buf, sizeof(buf), path); p.desc = buf;
        GetPrivateProfileStringA("patch", "rva", "", buf, sizeof(buf), path); p.rva = strtoul(buf, nullptr, 0);
        GetPrivateProfileStringA("patch", "expect", "", buf, sizeof(buf), path);
        bool okE = ParseHex(buf, p.expect);
        GetPrivateProfileStringA("patch", "write", "", buf, sizeof(buf), path);
        bool okW = ParseHex(buf, p.write);
        if (p.module.empty() || !p.rva || !okE || !okW || p.expect.size() != p.write.size()) {
            ShimLog("patch %s: INVALID file %s", p.name.c_str(), path);
            continue;
        }
        p.enabled = GetPrivateProfileIntA("patch", "enabled", 0, path) != 0;
        p.enabled = GetPrivateProfileIntA("patches", p.name.c_str(), p.enabled, ini) != 0;
        if (ForcedOn(p.name)) p.enabled = true;
        if (!p.enabled) ShimLog("patch %s: SKIP disabled", p.name.c_str());
        s_patches.push_back(p);
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    ApplyPending();
    for (auto& p : s_patches)
        if (p.enabled && !p.done) ShimLog("patch %s: pending %s", p.name.c_str(), p.module.c_str());

    void* ll = (void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (MH_CreateHook(ll, (void*)h_LoadLibraryA, (void**)&o_LoadLibraryA) == MH_OK && MH_EnableHook(ll) == MH_OK)
        ShimLog("hook LoadLibraryA @%p: ok", ll);
}
