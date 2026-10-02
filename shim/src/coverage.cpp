// Function-entry coverage with one-shot INT3 breakpoints (the Mesos / bncov technique).
//
// Enabled only when SIMGOLF_COVERAGE=<entries file> is set (test harness). The file has one entry per
// line: "<module>\t<hex address>" (VA for golf_clean.exe, RVA for DLLs). For each entry we save the
// first byte and write 0xCC. A vectored exception handler catches the breakpoint, puts the byte back,
// records the hit in the current phase's bit and resumes at the same address. One byte per function
// means no patch ever overlaps the next function (the Frida Interceptor census hung the game on tiny
// functions), and a function costs nothing after its first hit.
//
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
    int module;      // index into s_modules
    BYTE orig;
    volatile LONG armed;
    volatile LONG mask;
};

std::vector<std::string> s_modules;
std::vector<Entry> s_entries;  // sorted by addr
volatile LONG s_phase;
CRITICAL_SECTION s_lock;
bool s_on;

Entry* Find(DWORD a) {
    auto it = std::lower_bound(s_entries.begin(), s_entries.end(), a, [](const Entry& e, DWORD v) { return e.addr < v; });
    return (it != s_entries.end() && it->addr == a) ? &*it : nullptr;
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
    for (auto& e : s_entries) {
        if (e.armed) { ++n; continue; }
        if (*(BYTE*)e.addr == 0xCC) continue;  // someone else's breakpoint / padding
        e.orig = *(BYTE*)e.addr;
        InterlockedExchange(&e.armed, 1);
        *(volatile BYTE*)e.addr = 0xCC;
        ++n;
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

}  // namespace

void CoverageInit() {
    char path[MAX_PATH];
    if (!GetEnvironmentVariableA("SIMGOLF_COVERAGE", path, sizeof(path))) return;
    FILE* f = fopen(path, "r");
    if (!f) { ShimLog("coverage: cannot open %s", path); return; }
    InitializeCriticalSection(&s_lock);
    char line[512], mod[256];
    unsigned long addr;
    int skipped = 0;
    std::vector<int> writable;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255[^\t]\t%lx", mod, &addr) != 2) continue;
        HMODULE m = GetModuleHandleA(mod);
        if (!m) { ++skipped; continue; }  // DLL not mapped at init (jgld.dll is LoadLibrary'd later)
        auto it = std::find(s_modules.begin(), s_modules.end(), std::string(mod));
        int mi = (int)(it - s_modules.begin());
        if (it == s_modules.end()) { s_modules.push_back(mod); MakeWritable(m); }
        bool isExe = (m == GetModuleHandleA(nullptr));
        Entry e{};
        e.addr = isExe ? (DWORD)addr : (DWORD)(ULONG_PTR)m + (DWORD)addr;
        e.fileAddr = (DWORD)addr;
        e.module = mi;
        s_entries.push_back(e);
    }
    fclose(f);
    std::sort(s_entries.begin(), s_entries.end(), [](const Entry& a, const Entry& b) { return a.addr < b.addr; });
    s_entries.erase(std::unique(s_entries.begin(), s_entries.end(), [](const Entry& a, const Entry& b) { return a.addr == b.addr; }), s_entries.end());
    AddVectoredExceptionHandler(1, Veh);
    s_on = true;
    int armed = ArmAll();
    ShimLog("coverage: %d entries armed from %s (%d skipped: module not loaded at init), phase 0", armed, path, skipped);
}

extern "C" int __stdcall SimGolfShim_CovPhase(int phase) {
    if (!s_on) return -1;
    EnterCriticalSection(&s_lock);
    InterlockedExchange(&s_phase, phase);
    int n = ArmAll();
    LeaveCriticalSection(&s_lock);
    ShimLog("coverage: phase %d, %d entries re-armed", phase, n);
    return n;
}

extern "C" int __stdcall SimGolfShim_CovDump(const char* path) {
    if (!s_on) return -1;
    FILE* f = fopen(path, "w");
    if (!f) return -2;
    int n = 0;
    fprintf(f, "module\taddr\tmask\n");
    for (auto& e : s_entries) {
        if (!e.mask) continue;
        fprintf(f, "%s\t%08lx\t%08lx\n", s_modules[e.module].c_str(), e.fileAddr, (unsigned long)e.mask);
        ++n;
    }
    fclose(f);
    return n;
}
