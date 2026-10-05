// Function-entry coverage with one-shot INT3 breakpoints (the Mesos / bncov technique).
//
// Enabled only when SIMGOLF_COVERAGE=<entries file> is set (test harness). The file has one entry per
// line: "<module>\t<hex address>" (VA for golf_clean.exe, RVA for DLLs). For each entry we save the
// first byte and write 0xCC. A vectored exception handler catches the breakpoint, puts the byte back,
// records the hit in the current phase's bit and resumes at the same address. One byte per function
// means no patch ever overlaps the next function (the Frida Interceptor census hung the game on tiny
// functions), and a function costs nothing after its first hit.
//
// Modules not mapped at init (jgld.dll, sound.dll are LoadLibrary'd) stay pending and are adopted at the next
// SimGolfShim_CovPhase call, so functions that run before their module's first phase switch are not seen.
// Phases: SimGolfShim_CovPhase(n) re-arms every entry and switches the bit being recorded, so the
// result says which functions ran in EACH phase (boot, menu, sandbox...), not only first hits.
// SimGolfShim_CovDump(path) writes "module\taddr\tmask" for every entry that ran in any phase.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>

void ShimLog(const char* fmt, ...);

namespace {

struct Entry {
    DWORD addr;      // absolute address in this process
    DWORD fileAddr;  // address as written in the entries file
    BYTE orig;
    volatile LONG armed;
    volatile LONG mask;
};

// One table per module, sorted by addr and never resized once published: the VEH reads them without a lock,
// so a module that is loaded later (jgld.dll, sound.dll are LoadLibrary'd) gets its own new table.
struct Module {
    std::string name;
    std::vector<Entry> entries;
};
const int kMaxModules = 16;
Module* s_mods[kMaxModules];
volatile LONG s_nmods;
std::vector<std::pair<std::string, DWORD>> s_pending;  // entries of modules not mapped yet
volatile LONG s_phase;
CRITICAL_SECTION s_lock;
bool s_on;

Entry* Find(DWORD a) {
    LONG n = s_nmods;
    for (LONG i = 0; i < n; ++i) {
        auto& v = s_mods[i]->entries;
        auto it = std::lower_bound(v.begin(), v.end(), a, [](const Entry& e, DWORD x) { return e.addr < x; });
        if (it != v.end() && it->addr == a) return &*it;
    }
    return nullptr;
}

LONG CALLBACK Veh(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    DWORD a = (DWORD)(ULONG_PTR)ep->ExceptionRecord->ExceptionAddress;
    Entry* e = Find(a);
    if (!e) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&e->armed, 0)) {
        *(volatile BYTE*)a = e->orig;
        FlushInstructionCache(GetCurrentProcess(), (void*)a, 1);
    }
    InterlockedOr(&e->mask, 1 << (s_phase & 31));
    ep->ContextRecord->Eip = a;
    return EXCEPTION_CONTINUE_EXECUTION;
}

int ArmAll() {
    int n = 0;
    for (LONG i = 0; i < s_nmods; ++i) {
        for (auto& e : s_mods[i]->entries) {
            if (e.armed) { ++n; continue; }
            if (*(BYTE*)e.addr == 0xCC) continue;  // someone else's breakpoint / padding
            e.orig = *(BYTE*)e.addr;
            InterlockedExchange(&e.armed, 1);
            *(volatile BYTE*)e.addr = 0xCC;
            ++n;
        }
    }
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    return n;
}

void MakeWritable(HMODULE m) {
    auto* dos = (IMAGE_DOS_HEADER*)m;
    auto* nt = (IMAGE_NT_HEADERS*)((BYTE*)m + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        DWORD old;
        VirtualProtect((BYTE*)m + sec->VirtualAddress, sec->Misc.VirtualSize, PAGE_EXECUTE_READWRITE, &old);
    }
}

// Publishes a table for every pending module that is mapped now (called with s_lock held or before the VEH
// exists). The table is complete and sorted before s_nmods makes it visible; its INT3s are written later by
// ArmAll, so the VEH always finds the entry of a breakpoint it meets.
int AdoptLoaded() {
    int adopted = 0;
    std::vector<std::pair<std::string, DWORD>> still;
    std::vector<std::string> done;
    for (auto& pe : s_pending) {
        if (std::find(done.begin(), done.end(), pe.first) != done.end()) continue;
        HMODULE m = GetModuleHandleA(pe.first.c_str());
        if (!m || s_nmods >= kMaxModules) continue;
        auto* mod = new Module{pe.first, {}};
        bool isExe = (m == GetModuleHandleA(nullptr));
        for (auto& q : s_pending) {
            if (q.first != pe.first) continue;
            Entry e{};
            e.addr = isExe ? q.second : (DWORD)(ULONG_PTR)m + q.second;
            e.fileAddr = q.second;
            mod->entries.push_back(e);
        }
        std::sort(mod->entries.begin(), mod->entries.end(), [](const Entry& a, const Entry& b) { return a.addr < b.addr; });
        mod->entries.erase(std::unique(mod->entries.begin(), mod->entries.end(),
                                       [](const Entry& a, const Entry& b) { return a.addr == b.addr; }), mod->entries.end());
        MakeWritable(m);
        s_mods[s_nmods] = mod;
        InterlockedIncrement(&s_nmods);
        adopted += (int)mod->entries.size();
        done.push_back(pe.first);
    }
    for (auto& pe : s_pending)
        if (std::find(done.begin(), done.end(), pe.first) == done.end()) still.push_back(pe);
    s_pending.swap(still);
    return adopted;
}

}  // namespace

void CoverageInit() {
    char path[MAX_PATH];
    if (!GetEnvironmentVariableA("SIMGOLF_COVERAGE", path, sizeof(path))) return;
    FILE* f = fopen(path, "r");
    if (!f) { ShimLog("coverage: cannot open %s", path); return; }
    InitializeCriticalSection(&s_lock);
    char line[512], mod[256];
    unsigned long addr;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255[^\t]\t%lx", mod, &addr) != 2) continue;
        s_pending.emplace_back(mod, (DWORD)addr);
    }
    fclose(f);
    AdoptLoaded();
    AddVectoredExceptionHandler(1, Veh);
    s_on = true;
    int armed = ArmAll();
    ShimLog("coverage: %d entries armed from %s (%d pending: module not loaded at init), phase 0", armed, path,
            (int)s_pending.size());
}

extern "C" int __stdcall SimGolfShim_CovPhase(int phase) {
    if (!s_on) return -1;
    EnterCriticalSection(&s_lock);
    int adopted = AdoptLoaded();     // DLLs LoadLibrary'd since the last phase (jgld.dll, sound.dll)
    InterlockedExchange(&s_phase, phase);
    int n = ArmAll();
    LeaveCriticalSection(&s_lock);
    ShimLog("coverage: phase %d, %d entries re-armed (%d newly loaded, %d still pending)", phase, n, adopted,
            (int)s_pending.size());
    return n;
}

extern "C" int __stdcall SimGolfShim_CovDump(const char* path) {
    if (!s_on) return -1;
    FILE* f = fopen(path, "w");
    if (!f) return -2;
    int n = 0;
    fprintf(f, "module\taddr\tmask\n");
    for (LONG i = 0; i < s_nmods; ++i) {
        for (auto& e : s_mods[i]->entries) {
            if (!e.mask) continue;
            fprintf(f, "%s\t%08lx\t%08lx\n", s_mods[i]->name.c_str(), e.fileAddr, (unsigned long)e.mask);
            ++n;
        }
    }
    fclose(f);
    return n;
}
