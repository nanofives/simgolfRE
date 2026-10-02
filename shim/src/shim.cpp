// simgolf_shim -- loaded as a winmm.dll proxy next to golf_clean.exe.
//
// Purpose: make the stock (SafeDisc-unwrapped) game run on Windows 11 in a window, with no
// installer, no registry and no external tools. It is also the host for future RE patches.
//
// Fixes (each one toggleable in simgolf_shim.ini, [shim] section):
//   windowed=1   jgl(d).dll calls ChangeDisplaySettingsA(800x600x16, CDS_FULLSCREEN) and creates a
//                WS_POPUP "JackalClass" window sized to GetSystemMetrics(SM_CX/CYSCREEN).
//                We suppress the mode switch, report an 800x600 screen, give the window a frame,
//                and stop ClipCursor from trapping the mouse in the desktop's top-left corner.
//                Mouse input already works windowed: the game pairs GetCursorPos with
//                ScreenToClient (observed via re/frida/js/input_probe.js).
//   bink_dib=1   binkw32 1.x with BINKBUFFERAUTO (flags 0) picks a DirectDraw primary/overlay path
//                that access-violates inside BinkBufferOpen on Win11 (intro movie crash, intermittent).
//                Forcing BINKBUFFERDIBSECTION (type 2) is stable ("DIB Section" per
//                BinkBufferGetDescription).
//   skip_intro=0 BinkOpen("./flics/SMSG_introfinal.bik") returns NULL without opening the file; the game
//                handles that and goes straight to the main menu (~4.5 s boot instead of ~20 s).
//                Off for play, on for the test suite (SIMGOLF_SKIP_INTRO=1).
//
// Test hooks (exported, called by the Frida harness):
//   SimGolfShim_SetVirtualCursor(x, y)  -- GetCursorPos reports this client-space point instead of
//                                          the real cursor, so tests never move the user's mouse.
//   SimGolfShim_ClearVirtualCursor()
//   SimGolfShim_GetInfo()               -- returns a static string describing the active config.
//   SimGolfShim_CovPhase / CovDump      -- INT3 function coverage (coverage.cpp), only with SIMGOLF_COVERAGE.

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <share.h>
#include <shlwapi.h>
#include "../deps/minhook/MinHook.h"

void WinmmProxyResolve(HMODULE real);
void PatchesInit();
void SgHooksInstall();
void CoverageInit();

namespace {

struct Config {
    int windowed = 1;
    int binkDib = 1;
    int skipIntro = 0;
    int log = 1;
    int width = 800;
    int height = 600;
    int posX = -1;  // -1 = centre on the primary monitor
    int posY = -1;
} g_cfg;

HMODULE s_self;
char s_dir[MAX_PATH];
FILE* s_log;
HWND s_mainHwnd;
volatile LONG s_vcurOn;
volatile LONG s_vcurX, s_vcurY;
char s_info[256];

void Log(const char* fmt, ...) {
    if (!s_log) return;
    va_list ap;
    va_start(ap, fmt);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(s_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    vfprintf(s_log, fmt, ap);
    fputc('\n', s_log);
    fflush(s_log);
    va_end(ap);
}

void LoadConfig() {
    char ini[MAX_PATH];
    snprintf(ini, sizeof(ini), "%s\\simgolf_shim.ini", s_dir);
    g_cfg.windowed = GetPrivateProfileIntA("shim", "windowed", g_cfg.windowed, ini);
    g_cfg.binkDib = GetPrivateProfileIntA("shim", "bink_dib", g_cfg.binkDib, ini);
    g_cfg.log = GetPrivateProfileIntA("shim", "log", g_cfg.log, ini);
    g_cfg.skipIntro = GetPrivateProfileIntA("shim", "skip_intro", g_cfg.skipIntro, ini);
    g_cfg.posX = GetPrivateProfileIntA("shim", "x", g_cfg.posX, ini);
    g_cfg.posY = GetPrivateProfileIntA("shim", "y", g_cfg.posY, ini);
    // Environment overrides (used by the test harness): SIMGOLF_WINDOWED=0/1 etc.
    char v[32];
    if (GetEnvironmentVariableA("SIMGOLF_WINDOWED", v, sizeof(v))) g_cfg.windowed = atoi(v);
    if (GetEnvironmentVariableA("SIMGOLF_BINK_DIB", v, sizeof(v))) g_cfg.binkDib = atoi(v);
    if (GetEnvironmentVariableA("SIMGOLF_SHIM_LOG", v, sizeof(v))) g_cfg.log = atoi(v);
    if (GetEnvironmentVariableA("SIMGOLF_SKIP_INTRO", v, sizeof(v))) g_cfg.skipIntro = atoi(v);
}

const DWORD kFramedStyle = WS_VISIBLE | WS_CLIPSIBLINGS | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

// ---------------------------------------------------------------- user32 hooks
typedef LONG(WINAPI* ChangeDisplaySettingsA_t)(DEVMODEA*, DWORD);
ChangeDisplaySettingsA_t o_ChangeDisplaySettingsA;
LONG WINAPI h_ChangeDisplaySettingsA(DEVMODEA* dm, DWORD flags) {
    if (dm)
        Log("ChangeDisplaySettingsA %lux%lu %lubpp flags=0x%lx -> suppressed", dm->dmPelsWidth, dm->dmPelsHeight, dm->dmBitsPerPel, flags);
    else
        Log("ChangeDisplaySettingsA(NULL) restore -> suppressed");
    return DISP_CHANGE_SUCCESSFUL;
}

typedef int(WINAPI* GetSystemMetrics_t)(int);
GetSystemMetrics_t o_GetSystemMetrics;
int WINAPI h_GetSystemMetrics(int i) {
    if (i == SM_CXSCREEN) return g_cfg.width;
    if (i == SM_CYSCREEN) return g_cfg.height;
    return o_GetSystemMetrics(i);
}

typedef HWND(WINAPI* CreateWindowExA_t)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
CreateWindowExA_t o_CreateWindowExA;
HWND WINAPI h_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h,
                              HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    bool isMain = !IS_INTRESOURCE(cls) && cls && lstrcmpA(cls, "JackalClass") == 0;
    if (!isMain) return o_CreateWindowExA(ex, cls, title, style, x, y, w, h, parent, menu, inst, param);

    RECT rc = {0, 0, g_cfg.width, g_cfg.height};
    AdjustWindowRectEx(&rc, kFramedStyle, FALSE, 0);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int px = g_cfg.posX, py = g_cfg.posY;
    if (px < 0 || py < 0) {
        RECT wa;
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
        px = wa.left + ((wa.right - wa.left) - ww) / 2;
        py = wa.top + ((wa.bottom - wa.top) - wh) / 2;
    }
    HWND hwnd = o_CreateWindowExA(0, cls, title, kFramedStyle, px, py, ww, wh, parent, menu, inst, param);
    s_mainHwnd = hwnd;
    Log("CreateWindowExA JackalClass style 0x%08lx %dx%d -> framed %dx%d at %d,%d hwnd=%p",
        style, w, h, ww, wh, px, py, hwnd);
    return hwnd;
}

typedef BOOL(WINAPI* SetWindowPos_t)(HWND, HWND, int, int, int, int, UINT);
SetWindowPos_t o_SetWindowPos;
BOOL WINAPI h_SetWindowPos(HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags) {
    if (hwnd && hwnd == s_mainHwnd) flags |= SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER;
    return o_SetWindowPos(hwnd, after, x, y, cx, cy, flags);
}

typedef BOOL(WINAPI* ClipCursor_t)(const RECT*);
ClipCursor_t o_ClipCursor;
BOOL WINAPI h_ClipCursor(const RECT*) { return o_ClipCursor(NULL); }

// Virtual keys/buttons (test harness). The in-game UI reads mouse buttons from GetKeyboardState
// (VK_LBUTTON etc., ~13 calls/s), which posted WM_LBUTTONDOWN messages do not update; the main menu
// reads window messages. s_vkey[vk]: 0 = pass through, 1 = forced up, 2 = forced down.
volatile LONG s_vkey[256];

typedef BOOL(WINAPI* GetKeyboardState_t)(PBYTE);
GetKeyboardState_t o_GetKeyboardState;
BOOL WINAPI h_GetKeyboardState(PBYTE keys) {
    BOOL r = o_GetKeyboardState(keys);
    if (r && keys)
        for (int i = 0; i < 256; ++i)
            if (s_vkey[i]) keys[i] = (s_vkey[i] == 2) ? (BYTE)(keys[i] | 0x80) : (BYTE)(keys[i] & ~0x80);
    return r;
}
typedef SHORT(WINAPI* GetKeyState_t)(int);
GetKeyState_t o_GetKeyState, o_GetAsyncKeyState;
SHORT WINAPI h_GetKeyState(int vk) {
    SHORT r = o_GetKeyState(vk);
    LONG v = s_vkey[vk & 0xFF];
    return v ? (SHORT)(v == 2 ? (r | 0x8000) : (r & 0x7FFF)) : r;
}
SHORT WINAPI h_GetAsyncKeyState(int vk) {
    SHORT r = o_GetAsyncKeyState(vk);
    LONG v = s_vkey[vk & 0xFF];
    return v ? (SHORT)(v == 2 ? 0x8001 : 0) : r;
}

// Time warp (test harness): SIMGOLF_TIMEWARP=N makes the game's clocks run N times faster, so scenarios
// that need game days/months (career, landmarks, SGA invitations) fit in minutes. Clocks are scaled
// from the moment the shim loads, so they stay monotonic.
double s_warp = 1.0;
typedef DWORD(WINAPI* TickFn_t)(void);
TickFn_t o_timeGetTime, o_GetTickCount;
DWORD s_tgtBase, s_gtcBase;
DWORD WINAPI h_timeGetTime() { DWORD r = o_timeGetTime(); return s_tgtBase + (DWORD)((r - s_tgtBase) * s_warp); }
DWORD WINAPI h_GetTickCount() { DWORD r = o_GetTickCount(); return s_gtcBase + (DWORD)((r - s_gtcBase) * s_warp); }
typedef BOOL(WINAPI* QPC_t)(LARGE_INTEGER*);
QPC_t o_QueryPerformanceCounter;
LONGLONG s_qpcBase;
BOOL WINAPI h_QueryPerformanceCounter(LARGE_INTEGER* c) {
    BOOL r = o_QueryPerformanceCounter(c);
    if (r && c) c->QuadPart = s_qpcBase + (LONGLONG)((c->QuadPart - s_qpcBase) * s_warp);
    return r;
}

typedef BOOL(WINAPI* GetCursorPos_t)(LPPOINT);
GetCursorPos_t o_GetCursorPos;
BOOL WINAPI h_GetCursorPos(LPPOINT pt) {
    if (s_vcurOn && s_mainHwnd && pt) {
        POINT p = {s_vcurX, s_vcurY};
        ClientToScreen(s_mainHwnd, &p);
        *pt = p;
        return TRUE;
    }
    return o_GetCursorPos(pt);
}

// ---------------------------------------------------------------- binkw32 hooks
typedef void*(WINAPI* BinkBufferOpen_t)(HWND, unsigned, unsigned, unsigned);
BinkBufferOpen_t o_BinkBufferOpen;
void* WINAPI h_BinkBufferOpen(HWND hwnd, unsigned w, unsigned h, unsigned flags) {
    unsigned forced = (flags & ~0x1Fu) | 2u;  // type field = BINKBUFFERDIBSECTION
    void* r = o_BinkBufferOpen(hwnd, w, h, forced);
    Log("BinkBufferOpen %ux%u flags=0x%x -> 0x%x buf=%p", w, h, flags, forced, r);
    return r;
}

typedef void*(WINAPI* BinkOpen_t)(const char*, unsigned);
BinkOpen_t o_BinkOpen;
void* WINAPI h_BinkOpen(const char* name, unsigned flags) {
    if (name && StrStrIA(name, "introfinal")) {
        Log("BinkOpen %s -> skipped (skip_intro)", name);
        return nullptr;
    }
    return o_BinkOpen(name, flags);
}

bool Hook(void* target, void* detour, void** orig, const char* name) {
    if (!target) { Log("hook %s: target missing", name); return false; }
    MH_STATUS a = MH_CreateHook(target, detour, orig);
    MH_STATUS b = (a == MH_OK) ? MH_EnableHook(target) : a;
    Log("hook %s @%p: %s", name, target, b == MH_OK ? "ok" : MH_StatusToString(b));
    return b == MH_OK;
}

void InstallHooks() {
    MH_Initialize();
    HMODULE u32 = GetModuleHandleA("user32.dll");
    if (g_cfg.windowed) {
        Hook(GetProcAddress(u32, "ChangeDisplaySettingsA"), h_ChangeDisplaySettingsA, (void**)&o_ChangeDisplaySettingsA, "ChangeDisplaySettingsA");
        Hook(GetProcAddress(u32, "GetSystemMetrics"), h_GetSystemMetrics, (void**)&o_GetSystemMetrics, "GetSystemMetrics");
        Hook(GetProcAddress(u32, "CreateWindowExA"), h_CreateWindowExA, (void**)&o_CreateWindowExA, "CreateWindowExA");
        Hook(GetProcAddress(u32, "SetWindowPos"), h_SetWindowPos, (void**)&o_SetWindowPos, "SetWindowPos");
        Hook(GetProcAddress(u32, "ClipCursor"), h_ClipCursor, (void**)&o_ClipCursor, "ClipCursor");
    }
    Hook(GetProcAddress(u32, "GetCursorPos"), h_GetCursorPos, (void**)&o_GetCursorPos, "GetCursorPos");
    {
        char v[32];
        if (GetEnvironmentVariableA("SIMGOLF_TIMEWARP", v, sizeof(v)) && atof(v) > 1.0) {
            s_warp = atof(v);
            HMODULE wm = GetModuleHandleA("winmm.dll");  // that is us; the real one is resolved in g_winmm
            (void)wm;
            char sys[MAX_PATH];
            GetSystemDirectoryA(sys, sizeof(sys));
            lstrcatA(sys, "\\winmm.dll");
            HMODULE real = GetModuleHandleA(sys);
            HMODULE k32 = GetModuleHandleA("kernel32.dll");
            s_tgtBase = ((TickFn_t)GetProcAddress(real, "timeGetTime"))();
            s_gtcBase = GetTickCount();
            LARGE_INTEGER q; QueryPerformanceCounter(&q); s_qpcBase = q.QuadPart;
            Hook((void*)GetProcAddress(real, "timeGetTime"), h_timeGetTime, (void**)&o_timeGetTime, "timeGetTime(warp)");
            Hook((void*)GetProcAddress(k32, "GetTickCount"), h_GetTickCount, (void**)&o_GetTickCount, "GetTickCount(warp)");
            Hook((void*)GetProcAddress(k32, "QueryPerformanceCounter"), h_QueryPerformanceCounter, (void**)&o_QueryPerformanceCounter, "QueryPerformanceCounter(warp)");
            Log("time warp x%.1f", s_warp);
        }
    }
    Hook(GetProcAddress(u32, "GetKeyboardState"), h_GetKeyboardState, (void**)&o_GetKeyboardState, "GetKeyboardState");
    Hook(GetProcAddress(u32, "GetKeyState"), h_GetKeyState, (void**)&o_GetKeyState, "GetKeyState");
    Hook(GetProcAddress(u32, "GetAsyncKeyState"), h_GetAsyncKeyState, (void**)&o_GetAsyncKeyState, "GetAsyncKeyState");
    if (g_cfg.binkDib) {
        HMODULE bink = GetModuleHandleA("binkw32.dll");
        Hook(bink ? (void*)GetProcAddress(bink, "_BinkBufferOpen@16") : nullptr, h_BinkBufferOpen, (void**)&o_BinkBufferOpen, "BinkBufferOpen");
    }
    if (g_cfg.skipIntro) {
        HMODULE bink = GetModuleHandleA("binkw32.dll");
        Hook(bink ? (void*)GetProcAddress(bink, "_BinkOpen@8") : nullptr, h_BinkOpen, (void**)&o_BinkOpen, "BinkOpen");
    }
}

}  // namespace

void ShimLog(const char* fmt, ...) {
    if (!s_log) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Log("%s", buf);
}
const char* ShimDir() { return s_dir; }

extern "C" void __stdcall SimGolfShim_SetVirtualCursor(int x, int y) {
    InterlockedExchange(&s_vcurX, x);
    InterlockedExchange(&s_vcurY, y);
    InterlockedExchange(&s_vcurOn, 1);
}
extern "C" void __stdcall SimGolfShim_ClearVirtualCursor() { InterlockedExchange(&s_vcurOn, 0); }
// state: 0 = release control to the real keyboard/mouse, 1 = forced up, 2 = forced down.
extern "C" void __stdcall SimGolfShim_SetVirtualKey(int vk, int state) { InterlockedExchange(&s_vkey[vk & 0xFF], state); }
extern "C" const char* __stdcall SimGolfShim_GetInfo() { return s_info; }

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    s_self = inst;
    DisableThreadLibraryCalls(inst);

    // Real winmm first: the proxy stubs must be live before anyone calls through them.
    char sys[MAX_PATH];
    GetSystemDirectoryA(sys, sizeof(sys));  // SysWOW64 for a 32-bit process
    lstrcatA(sys, "\\winmm.dll");
    WinmmProxyResolve(LoadLibraryA(sys));

    GetModuleFileNameA(NULL, s_dir, sizeof(s_dir));
    char* slash = strrchr(s_dir, '\\');
    if (slash) *slash = 0;
    LoadConfig();
    {
        // SIMGOLF_VCURSOR=x,y: virtual cursor live from the first frame, so the user's real mouse can never
        // set a hover state the harness did not choose (2026-10-02: a real cursor resting on "Start New Game"
        // turned the harness's "Sandbox Mode" click into a Start New Game click).
        char v[64];
        int x, y;
        if (GetEnvironmentVariableA("SIMGOLF_VCURSOR", v, sizeof(v)) && sscanf(v, "%d,%d", &x, &y) == 2) {
            s_vcurX = x;
            s_vcurY = y;
            s_vcurOn = 1;
            s_vkey[VK_LBUTTON] = s_vkey[VK_RBUTTON] = s_vkey[VK_MBUTTON] = 1;  // real buttons never leak in
        }
    }
    if (g_cfg.log) {
        char p[MAX_PATH];
        snprintf(p, sizeof(p), "%s\\simgolf_shim.log", s_dir);
        s_log = _fsopen(p, "w", _SH_DENYNO);
    }
    snprintf(s_info, sizeof(s_info), "simgolf_shim windowed=%d bink_dib=%d skip_intro=%d %dx%d", g_cfg.windowed, g_cfg.binkDib, g_cfg.skipIntro, g_cfg.width, g_cfg.height);
    Log("%s (pid %lu)", s_info, GetCurrentProcessId());
    InstallHooks();
    PatchesInit();
    SgHooksInstall();
    CoverageInit();  // last: arms INT3s over the final bytes (after MinHook JMPs)
    return TRUE;
}
