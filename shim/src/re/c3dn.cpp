// Batch c3dn (round 27, jgld.dll): reachable C2 rows of jgld.dll that the stand-ins (import, heap, freeze) unblock.
//
//   group W - Display import wrappers   Display::showWindow / hideWindow / minimizeWindow / maximizeWindow,
//                                       getWindowRect, invalidate, setWindowPos, getCursor, restore,
//                                       setResolution, setDesktopMode, shutdown
//   group M - module globals            initFontTable, Display::shutdownFonts, Display::freePalette,
//                                       Palette::animate, trace, DllMain, onPaint, onActivate
//   group G - Surface GDI wrappers      Surface::setTextColor16 / setTextColorRGB / selectObject /
//                                       selectSystemFont / textOut, Font::textWidth
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form the debug
// build's disassembly prints. jgld.dll is relocated at run time, so every module address (vtables, globals,
// callees, import slots) is resolved against the running base. __thiscall is emulated with __fastcall (ecx = this,
// edx unused). Imports are called through jgld's own IAT slots (the import stand-in replaces those slots for one
// key), callees through the incremental-link thunks the originals call.
//
// Values a body leaves in eax: every function here returns what the original leaves in eax on each path, because
// the callers (vtable slots, window-procedure dispatch) may pass it on. Two recurring sources:
//   - an import's result when the import is the last call (__chkesp 0x1007e780 and the epilogue keep eax);
//   - 0xcccccccc, the /GZ fill (`mov eax, 0xcccccccc; rep stosd` in the prologue) when a path stores nothing in eax.
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
// Slot `off` of the vtable of `obj` (`mov edx, [obj]; call [edx + off]`).
inline void* VSlot(void* obj, unsigned off) { return F<void*>(F<void*>(obj, 0), off); }
// The import whose IAT slot is at VA `slot`.
template <class Fn> Fn Imp(unsigned slot) { return *static_cast<Fn*>(JgldVA(slot)); }

const int kFill = static_cast<int>(0xcccccccc);

// jgld.dll's IAT slots used here (re/tools: pefile over original\jgld.dll)
const unsigned kShowWindow = 0x1012c624, kGetWindowRect = 0x1012c604, kInvalidateRect = 0x1012c614,
               kSetWindowPos = 0x1012c620, kGetCursorPos = 0x1012c610, kScreenToClient = 0x1012c618,
               kChangeDisplaySettingsA = 0x1012c5f4, kEnumDisplaySettingsA = 0x1012c60c,
               kGetSystemMetrics = 0x1012c600, kDestroyWindow = 0x1012c5f0, kUnregisterClassA = 0x1012c5e0,
               kDeleteDC = 0x1012c380, kDeleteObject = 0x1012c370, kAnimatePalette = 0x1012c3b4,
               kOutputDebugStringA = 0x1012c42c, kInitializeCriticalSection = 0x1012c41c,
               kDeleteCriticalSection = 0x1012c424, kBeginPaint = 0x1012c638, kEndPaint = 0x1012c628,
               kDefWindowProcA = 0x1012c62c, kSetTextColor = 0x1012c39c, kSelectObject = 0x1012c378,
               kGetStockObject = 0x1012c3a4, kTextOutA = 0x1012c3a0, kGetTextExtentPoint32A = 0x1012c374;

// module globals
const unsigned kFontDC = 0x10128724;     // the font-measuring DC (Font::textWidth, initFontTable)
const unsigned kPalette = 0x10128738;    // the module palette handle (deleteLogPalette, Palette::animate)
const unsigned kSink = 0x1012872c;       // the window-message event sink (every on* handler)
const unsigned kCritSec = 0x101286b0;    // the module critical section DllMain creates and deletes

typedef int(__fastcall* Method0_t)(void*, void*);

// =====================================================================================================================
// group W - Display import wrappers. `this` is a Display; +0x144 is its window handle (stored by createWindow at
// 0x1006591f), +0x140 the class's instance handle, +0x148 an object released through its slot +8, +0x98 a DEVMODE.
// =====================================================================================================================

// 0x000675e0 / 0x00067640 / 0x000676a0 / 0x00067700  Display::showWindow / hideWindow / minimizeWindow /
// maximizeWindow (thiscall, no stack argument, plain `ret`): ShowWindow ([this+0x144], n) with n = 1 (`push 1`
// 0x100675ff), 0 (0x1006765f), 6 (0x100676bf), 3 (0x1006771f); ShowWindow's result is left in eax.
typedef BOOL(WINAPI* ShowWindow_t)(HWND, int);
int ShowWithCmd(void* self, int cmd) {
    return Imp<ShowWindow_t>(kShowWindow)(F<HWND>(self, 0x144), cmd);
}
Method0_t ShowWindow_orig, HideWindow_orig, MinimizeWindow_orig, MaximizeWindow_orig;
int __fastcall ShowWindow_re(void* self, void*) { return ShowWithCmd(self, 1); }
int __fastcall HideWindow_re(void* self, void*) { return ShowWithCmd(self, 0); }
int __fastcall MinimizeWindow_re(void* self, void*) { return ShowWithCmd(self, 6); }
int __fastcall MaximizeWindow_re(void* self, void*) { return ShowWithCmd(self, 3); }

// 0x00066650  Display::getWindowRect(rect) (`ret 4` at 0x1006669a): GetWindowRect ([this+0x144], rect)
// (0x1006667d); its result is left in eax.
typedef BOOL(WINAPI* GetWindowRect_t)(HWND, RECT*);
typedef int(__fastcall* Method1_t)(void*, void*, void*);
Method1_t GetWindowRect_orig;
int __fastcall GetWindowRect_re(void* self, void*, void* rect) {
    return Imp<GetWindowRect_t>(kGetWindowRect)(F<HWND>(self, 0x144), static_cast<RECT*>(rect));
}

// 0x00066710  Display::invalidate(rect) (`ret 4` at 0x1006675c): InvalidateRect ([this+0x144], rect, FALSE)
// (`push 0` 0x1006672f, call 0x1006673f); its result is left in eax.
typedef BOOL(WINAPI* InvalidateRect_t)(HWND, const RECT*, BOOL);
Method1_t Invalidate_orig;
int __fastcall Invalidate_re(void* self, void*, void* rect) {
    return Imp<InvalidateRect_t>(kInvalidateRect)(F<HWND>(self, 0x144), static_cast<RECT*>(rect), FALSE);
}

// 0x00066810  Display::setWindowPos(x, y, cx, cy) (`ret 0x10` at 0x1006686a): SetWindowPos ([this+0x144], NULL,
// x, y, cx, cy, 0) (pushes 0x1006682f..0x10066846, call 0x1006684d); its result is left in eax.
typedef BOOL(WINAPI* SetWindowPos_t)(HWND, HWND, int, int, int, int, UINT);
typedef int(__fastcall* Method4_t)(void*, void*, int, int, int, int);
Method4_t SetWindowPos_orig;
int __fastcall SetWindowPos_re(void* self, void*, int x, int y, int cx, int cy) {
    return Imp<SetWindowPos_t>(kSetWindowPos)(F<HWND>(self, 0x144), 0, x, y, cx, cy, 0);
}

// 0x00066780  Display::getCursor(px, py) (`ret 8` at 0x100667ed): GetCursorPos into a local POINT (0x100667a3),
// ScreenToClient ([this+0x144], &pt) (0x100667c0), then *px = pt.x (0x100667d3) and *py = pt.y (0x100667db). The
// last store leaves py in eax (`mov eax, [ebp+0xc]` 0x100667d5).
typedef BOOL(WINAPI* GetCursorPos_t)(POINT*);
typedef BOOL(WINAPI* ScreenToClient_t)(HWND, POINT*);
typedef int(__fastcall* Method2_t)(void*, void*, void*, void*);
Method2_t GetCursor_orig;
int __fastcall GetCursor_re(void* self, void*, void* px, void* py) {
    POINT pt;
    Imp<GetCursorPos_t>(kGetCursorPos)(&pt);
    Imp<ScreenToClient_t>(kScreenToClient)(F<HWND>(self, 0x144), &pt);
    *static_cast<int*>(px) = pt.x;
    *static_cast<int*>(py) = pt.y;
    return reinterpret_cast<int>(py);
}

// 0x00066970  Display::restore (thiscall, no stack argument, plain `ret`): ChangeDisplaySettingsA (this + 0x98, 4)
// (0x1006699a); a non-zero result (anything but DISP_CHANGE_SUCCESSFUL) returns 2 (0x100669ab), zero returns 0
// (0x100669b2).
typedef LONG(WINAPI* ChangeDisplaySettingsA_t)(void*, DWORD);
Method0_t Restore_orig;
int __fastcall Restore_re(void* self, void*) {
    const LONG r = Imp<ChangeDisplaySettingsA_t>(kChangeDisplaySettingsA)(static_cast<char*>(self) + 0x98, 4);
    return r != 0 ? 2 : 0;
}

// 0x00066890  Display::setResolution(w, h) (`ret 8` at 0x10066938): EnumDisplaySettingsA (NULL,
// ENUM_CURRENT_SETTINGS = -1, this + 0x98) (0x100668c2; its result is not tested), the 0x94-byte DEVMODE at
// this + 0x98 copied into a local by memcpy 0x1007f3a0 (0x100668e5), then in the copy dmPelsWidth (+0x6c) = w
// (0x100668f0), dmPelsHeight (+0x70) = h (0x100668f6), dmFields (+0x28) = 0x1c0000 (DM_BITSPERPEL | DM_PELSWIDTH |
// DM_PELSHEIGHT, 0x100668f9), and ChangeDisplaySettingsA (&copy, 4) (0x1006690b): non-zero -> 2, zero -> 0.
typedef BOOL(WINAPI* EnumDisplaySettingsA_t)(const char*, DWORD, void*);
typedef int(__fastcall* MethodII_t)(void*, void*, int, int);
MethodII_t SetResolution_orig;
int __fastcall SetResolution_re(void* self, void*, int w, int h) {
    char* const dm = static_cast<char*>(self) + 0x98;
    Imp<EnumDisplaySettingsA_t>(kEnumDisplaySettingsA)(0, static_cast<DWORD>(-1), dm);
    unsigned char copy[0x94];
    memcpy(copy, dm, sizeof copy);
    F<int>(copy, 0x6c) = w;
    F<int>(copy, 0x70) = h;
    F<int>(copy, 0x28) = 0x1c0000;
    const LONG r = Imp<ChangeDisplaySettingsA_t>(kChangeDisplaySettingsA)(copy, 4);
    return r != 0 ? 2 : 0;
}

// 0x00066200  Display::setDesktopMode(flag) (`ret 4` at 0x10066275): GetSystemMetrics (SM_CXSCREEN = 0)
// (0x10066221) and (SM_CYSCREEN = 1) (0x10066235), then this's own vtable slot +0x58 (Display slot 22,
// setMode) with (cx, cy, flag) (0x1006625b); that call's result is left in eax.
typedef int(WINAPI* GetSystemMetrics_t)(int);
typedef int(__fastcall* SetMode_t)(void*, void*, int, int, int);
typedef int(__fastcall* MethodI_t)(void*, void*, int);
MethodI_t SetDesktopMode_orig;
int __fastcall SetDesktopMode_re(void* self, void*, int flag) {
    const int cx = Imp<GetSystemMetrics_t>(kGetSystemMetrics)(0);
    const int cy = Imp<GetSystemMetrics_t>(kGetSystemMetrics)(1);
    return reinterpret_cast<SetMode_t>(VSlot(self, 0x58))(self, 0, cx, cy, flag);
}

// 0x000656c0  Display::shutdown (thiscall, no stack argument, plain `ret`):
//   ChangeDisplaySettingsA (this + 4, 0) (0x100656e8; result not tested);
//   [this+0x144] != 0 (je 0x100656ff): DestroyWindow ([this+0x144]) (0x1006570d), [this+0x144] = 0 (0x1006571d);
//   [this+0x148] != 0 (je 0x10065731): the object's vtable slot +8 with ecx = the object and no stack argument
//     (0x10065749), [this+0x148] = 0 (0x10065756);
//   UnregisterClassA (class name 0x1011d978, [this+0x140]) (0x10065771), [this+0x140] = 0 (0x10065781).
// The last store keeps UnregisterClassA's result in eax (the stores go through eax = this only at 0x1006577e:
// `mov eax, [ebp-4]` then `mov [eax+0x140], 0`), so eax = this on return.
typedef BOOL(WINAPI* DestroyWindow_t)(HWND);
typedef BOOL(WINAPI* UnregisterClassA_t)(const char*, HINSTANCE);
typedef int(__fastcall* Release_t)(void*, void*);
Method0_t Shutdown_orig;
int __fastcall Shutdown_re(void* self, void*) {
    Imp<ChangeDisplaySettingsA_t>(kChangeDisplaySettingsA)(static_cast<char*>(self) + 4, 0);
    if (F<HWND>(self, 0x144)) {
        Imp<DestroyWindow_t>(kDestroyWindow)(F<HWND>(self, 0x144));
        F<HWND>(self, 0x144) = 0;
    }
    if (void* const obj = F<void*>(self, 0x148)) {
        reinterpret_cast<Release_t>(VSlot(obj, 8))(obj, 0);
        F<void*>(self, 0x148) = 0;
    }
    Imp<UnregisterClassA_t>(kUnregisterClassA)(static_cast<const char*>(JgldVA(0x1011d978)),
                                               F<HINSTANCE>(self, 0x140));
    F<HINSTANCE>(self, 0x140) = 0;
    return reinterpret_cast<int>(self);
}

// =====================================================================================================================
// group M - functions over module globals
// =====================================================================================================================

// 0x00068fa0  initFontTable (cdecl, no argument, plain `ret`): when the font DC 0x10128724 is non-zero (cmp/je
// 0x10068fb8 / 0x10068fbf), DeleteDC (it) (0x10068fc9) and the global = 0 (0x10068fd6). Despite its name it tears
// the font DC down. eax: DeleteDC's result after the delete; the /GZ fill 0xcccccccc when the global is 0 (nothing
// after the prologue's `rep stosd` writes eax on that path).
typedef BOOL(WINAPI* DeleteDC_t)(HDC);
typedef int(__cdecl* Void0_t)();
Void0_t InitFontTable_orig;
int __cdecl InitFontTable_re() {
    if (G<HDC>(kFontDC) == 0)
        return kFill;
    const int r = Imp<DeleteDC_t>(kDeleteDC)(G<HDC>(kFontDC));
    G<HDC>(kFontDC) = 0;
    return r;
}

// 0x00067e40  Display::shutdownFonts (thiscall, no stack argument, plain `ret`): calls initFontTable 0x10068fa0
// through the thunk 0x10001217 (0x10067e5d); `this` is stored (0x10067e5a) and never read. eax is the callee's.
Method0_t ShutdownFonts_orig;
int __fastcall ShutdownFonts_re(void*, void*) {
    return reinterpret_cast<Void0_t>(JgldVA(0x10001217))();
}

// 0x00067ed0  Display::freePalette (thiscall, no stack argument, plain `ret`): calls deleteLogPalette 0x1006ad60
// through the thunk 0x1000129e (0x10067eed); `this` is never read. eax is the callee's.
Method0_t FreePalette_orig;
int __fastcall FreePalette_re(void*, void*) {
    return reinterpret_cast<Void0_t>(JgldVA(0x1000129e))();
}

// 0x0006a860  Palette::animate (thiscall, no stack argument, plain `ret`): when the module palette 0x10128738 is
// non-zero (cmp 0x1006a87d, jne 0x1006a884), AnimatePalette (it, 10, 0xec, this + 0x34) (pushes 0x1006a890..
// 0x1006a898, call 0x1006a89f): entries 10..245 from the Palette's PALETTEENTRY array at +0x34. eax:
// AnimatePalette's result; 0xcccccccc (the /GZ fill) when the palette is 0.
typedef BOOL(WINAPI* AnimatePalette_t)(HPALETTE, UINT, UINT, const void*);
Method0_t Animate_orig;
int __fastcall Animate_re(void* self, void*) {
    if (G<HPALETTE>(kPalette) == 0)
        return kFill;
    return Imp<AnimatePalette_t>(kAnimatePalette)(G<HPALETTE>(kPalette), 10, 0xec, static_cast<char*>(self) + 0x34);
}

// 0x00014550  trace(png_ptr, message) (cdecl, plain `ret` at 0x1001458b): OutputDebugStringA (message)
// (`mov eax, [ebp+0xc]` 0x1001456a, call 0x1001456e); the first argument is not read. eax is the import's.
// loadPng 0x100145a0 passes it (thunk 0x1000117c) as both the error and the warning function of
// png_create_read_struct 0x1006bac0 (`push 0x1000117c` at 0x100145ee and 0x100145f3).
typedef int(WINAPI* OutputDebugStringA_t)(const char*);
typedef int(__cdecl* Trace_t)(void*, const char*);
Trace_t Trace_orig;
int __cdecl Trace_re(void*, const char* msg) {
    return Imp<OutputDebugStringA_t>(kOutputDebugStringA)(msg);
}

// 0x00065300  DllMain(hinst, reason, reserved) (stdcall, `ret 0xc` at 0x1006536b): reason 0 (DLL_PROCESS_DETACH,
// je 0x10065322) -> DeleteCriticalSection (0x101286b0) (0x10065349); reason 1 (DLL_PROCESS_ATTACH, je 0x10065328)
// -> InitializeCriticalSection (0x101286b0) (0x10065333); any other reason calls nothing. Always returns 1
// (0x10065356).
typedef void(WINAPI* CritSec_t)(void*);
typedef BOOL(WINAPI* DllMain_t)(HINSTANCE, DWORD, void*);
DllMain_t DllMain_orig;
BOOL WINAPI DllMain_re(HINSTANCE, DWORD reason, void*) {
    if (reason == 0)
        Imp<CritSec_t>(kDeleteCriticalSection)(JgldVA(kCritSec));
    else if (reason == 1)
        Imp<CritSec_t>(kInitializeCriticalSection)(JgldVA(kCritSec));
    return 1;
}

// 0x00069ac0  onPaint(hwnd) (cdecl, plain `ret` at 0x10069b60; windowProc's WM_PAINT handler): BeginPaint (hwnd,
// &ps) (0x10069ae8). A NULL DC (jne 0x10069afc not taken) jumps to the end with eax = 0. Otherwise ps.rcPaint
// (ps + 8 .. + 0x14) is copied into a local RECT (0x10069b00..0x10069b15), the event sink 0x1012872c's vtable
// slot +4 is called with ecx = the sink and the RECT's address (0x10069b2c), and EndPaint (hwnd, &ps) (0x10069b40)
// leaves its result in eax.
typedef HDC(WINAPI* BeginPaint_t)(HWND, PAINTSTRUCT*);
typedef BOOL(WINAPI* EndPaint_t)(HWND, const PAINTSTRUCT*);
typedef int(__fastcall* SinkRect_t)(void*, void*, RECT*);
typedef int(__cdecl* OnPaint_t)(HWND);
OnPaint_t OnPaint_orig;
int __cdecl OnPaint_re(HWND hwnd) {
    PAINTSTRUCT ps;
    const HDC dc = Imp<BeginPaint_t>(kBeginPaint)(hwnd, &ps);
    if (dc == 0)
        return 0;
    RECT r = ps.rcPaint;
    void* const sink = G<void*>(kSink);
    reinterpret_cast<SinkRect_t>(VSlot(sink, 4))(sink, 0, &r);
    return Imp<EndPaint_t>(kEndPaint)(hwnd, &ps);
}

// 0x00069b90  onActivate(hwnd, active, lparam, minimized) (cdecl, plain `ret` at 0x10069c3a; windowProc's
// WM_ACTIVATE handler with the wParam halves split): InvalidateRect (hwnd, NULL, FALSE) (0x10069bb2); when active
// != 0 (je 0x10069bc3) and minimized == 0 (jne 0x10069bc9), realizePalette 0x100699c0 (hwnd) through the thunk
// 0x10001028 (0x10069bcf, `add esp, 4`); the event sink's slot +0x2c with (active, minimized) (0x10069bef); then
// DefWindowProcA (hwnd, WM_ACTIVATE = 6, (active & 0xffff) | (minimized << 16), lparam) (0x10069c1d), whose
// result is left in eax.
typedef LRESULT(WINAPI* DefWindowProcA_t)(HWND, UINT, WPARAM, LPARAM);
typedef int(__cdecl* RealizePalette_t)(HWND);
typedef int(__fastcall* Sink2_t)(void*, void*, int, int);
typedef int(__cdecl* OnActivate_t)(HWND, int, int, int);
OnActivate_t OnActivate_orig;
int __cdecl OnActivate_re(HWND hwnd, int active, int lparam, int minimized) {
    Imp<InvalidateRect_t>(kInvalidateRect)(hwnd, 0, FALSE);
    if (active != 0 && minimized == 0)
        reinterpret_cast<RealizePalette_t>(JgldVA(0x10001028))(hwnd);
    void* const sink = G<void*>(kSink);
    reinterpret_cast<Sink2_t>(VSlot(sink, 0x2c))(sink, 0, active, minimized);
    const WPARAM wp = (static_cast<unsigned>(active) & 0xffff) | ((static_cast<unsigned>(minimized) & 0xffff) << 16);
    return static_cast<int>(Imp<DefWindowProcA_t>(kDefWindowProcA)(hwnd, 6, wp, lparam));
}

// =====================================================================================================================
// group G - Surface GDI wrappers (jgld's Surface vtable 0x1011d0b0). Slot +0x28 is Surface::acquireDC 0x10008640
// (copies the memory DC +0x4bc into +0x4b8, counts +0x4c4 up, returns the DC), slot +0x2c Surface::releaseDC
// 0x100086b0 (counts down by its argument). Each wrapper takes the DC from slot +0x28, makes one GDI call on it,
// releases with 1 and returns 0 (`xor eax, eax`).
// =====================================================================================================================
typedef HDC(__fastcall* AcquireDC_t)(void*, void*);
typedef int(__fastcall* ReleaseDC_t)(void*, void*, int);
inline HDC SurfDC(void* s) { return reinterpret_cast<AcquireDC_t>(VSlot(s, 0x28))(s, 0); }
inline void SurfRelease(void* s) { reinterpret_cast<ReleaseDC_t>(VSlot(s, 0x2c))(s, 0, 1); }
typedef COLORREF(WINAPI* SetTextColor_t)(HDC, COLORREF);
typedef HGDIOBJ(WINAPI* SelectObject_t)(HDC, HGDIOBJ);
typedef HGDIOBJ(WINAPI* GetStockObject_t)(int);
typedef BOOL(WINAPI* TextOutA_t)(HDC, int, int, const char*, int);

// 0x00008d00  Surface::setTextColor16(colour) (`ret 4` at 0x10008d7d): SetTextColor (dc, (colour & 0xffff) |
// 0x10ff0000) (0x10008d37 / 0x10008d3c, call 0x10008d48): the 0x10ff high word marks a DIBINDEX-style 16-bit value.
MethodI_t SetTextColor16_orig;
int __fastcall SetTextColor16_re(void* self, void*, int colour) {
    const HDC dc = SurfDC(self);
    Imp<SetTextColor_t>(kSetTextColor)(dc, (static_cast<unsigned>(colour) & 0xffff) | 0x10ff0000);
    SurfRelease(self);
    return 0;
}

// 0x00008c40  Surface::setTextColorRGB(r, g, b) (`ret 0xc` at 0x10008cd4): SetTextColor (dc, (r & 0xff) |
// (g & 0xff) << 8 | (b & 0xff) << 16) (0x10008c74..0x10008c96, call 0x10008c9f).
typedef int(__fastcall* Method3_t)(void*, void*, int, int, int);
Method3_t SetTextColorRGB_orig;
int __fastcall SetTextColorRGB_re(void* self, void*, int r, int g, int b) {
    const HDC dc = SurfDC(self);
    const unsigned c = (static_cast<unsigned>(r) & 0xff) | ((static_cast<unsigned>(g) & 0xff) << 8) |
                       ((static_cast<unsigned>(b) & 0xff) << 16);
    Imp<SetTextColor_t>(kSetTextColor)(dc, c);
    SurfRelease(self);
    return 0;
}

// 0x00008ae0  Surface::selectObject(pfont) (`ret 4` at 0x10008b5e): the argument points at a Font pointer
// (`mov ecx, [eax]` 0x10008b17); SelectObject (dc, [font+0x18]) (0x10008b21, call 0x10008b29), +0x18 being the
// Font's HFONT.
MethodI_t SelectObject_orig;
int __fastcall SelectObject_re(void* self, void*, int pfont) {
    const HDC dc = SurfDC(self);
    void* const font = *reinterpret_cast<void**>(pfont);
    Imp<SelectObject_t>(kSelectObject)(dc, F<HGDIOBJ>(font, 0x18));
    SurfRelease(self);
    return 0;
}

// 0x00008b90  Surface::selectSystemFont (thiscall, no stack argument, plain `ret` at 0x10008c11): SelectObject
// (dc, GetStockObject (SYSTEM_FONT = 0xd)) (0x10008bc6 / 0x10008bdc).
Method0_t SelectSystemFont_orig;
int __fastcall SelectSystemFont_re(void* self, void*) {
    const HDC dc = SurfDC(self);
    Imp<SelectObject_t>(kSelectObject)(dc, Imp<GetStockObject_t>(kGetStockObject)(0xd));
    SurfRelease(self);
    return 0;
}

// 0x00008da0  Surface::textOut(x, y, text, length) (`ret 0x10` at 0x10008e1f): TextOutA (dc, x, y, text, length)
// (pushes 0x10008dd6..0x10008de5, call 0x10008dea).
Method4_t TextOut_orig;
int __fastcall TextOut_re(void* self, void*, int x, int y, int text, int len) {
    const HDC dc = SurfDC(self);
    Imp<TextOutA_t>(kTextOutA)(dc, x, y, reinterpret_cast<const char*>(text), len);
    SurfRelease(self);
    return 0;
}

// 0x00068e60  Font::textWidth(text, length) (`ret 8` at 0x10068f00): a NULL text returns 0 (jne 0x10068e81 not
// taken, `xor eax, eax` 0x10068e83). Otherwise, on the module's font DC 0x10128724: SelectObject (dc,
// [this+0x18]) (0x10068e97), GetTextExtentPoint32A (dc, text, length, &size) (0x10068eb8), SelectObject (dc,
// GetStockObject (SYSTEM_FONT = 0xd)) (0x10068ec9 / 0x10068ee0), and size.cx is returned (0x10068eed).
typedef BOOL(WINAPI* GetTextExtentPoint32A_t)(HDC, const char*, int, SIZE*);
MethodII_t TextWidth_orig;
int __fastcall TextWidth_re(void* self, void*, int text, int len) {
    if (text == 0)
        return 0;
    SIZE size;
    Imp<SelectObject_t>(kSelectObject)(G<HDC>(kFontDC), F<HGDIOBJ>(self, 0x18));
    Imp<GetTextExtentPoint32A_t>(kGetTextExtentPoint32A)(G<HDC>(kFontDC), reinterpret_cast<const char*>(text), len,
                                                         &size);
    Imp<SelectObject_t>(kSelectObject)(G<HDC>(kFontDC), Imp<GetStockObject_t>(kGetStockObject)(0xd));
    return size.cx;
}

// =====================================================================================================================
// group C - constructors. Tracked::ctor 0x10006ab0 (through the thunk 0x100013e3) stores the Tracked vtable
// 0x1011d058 and registers the object in the module's tracked list 0x10128428 (LinkedList::add, which allocates a
// node with operator new 0x1007f0f0); each derived constructor then overwrites +0 with its own vtable.
// =====================================================================================================================
typedef void*(__fastcall* Ctor_t)(void*, void*);
typedef void*(__fastcall* Ctor1_t)(void*, void*, void*);
inline void* TrackedCtor(void* self) { return reinterpret_cast<Ctor_t>(JgldVA(0x100013e3))(self, 0); }

// 0x00009690  SurfaceBase::ctor (thiscall, no stack argument, plain `ret` at 0x100096d1): Tracked::ctor
// (0x100096b0), the SurfaceBase vtable 0x1011d220 into this[0] (0x100096b8), returns this (0x100096be).
Ctor_t SurfaceBaseCtor_orig;
void* __fastcall SurfaceBaseCtor_re(void* self, void*) {
    TrackedCtor(self);
    F<void*>(self, 0) = JgldVA(0x1011d220);
    return self;
}

// 0x00014c70  SpriteBase::ctor: the same shape (Tracked::ctor 0x10014c90), vtable 0x1011d444 (0x10014c98).
Ctor_t SpriteBaseCtor_orig;
void* __fastcall SpriteBaseCtor_re(void* self, void*) {
    TrackedCtor(self);
    F<void*>(self, 0) = JgldVA(0x1011d444);
    return self;
}

// 0x0006a260  PaletteBase::ctor: the same shape (Tracked::ctor 0x1006a280), vtable 0x1011db3c (0x1006a288).
Ctor_t PaletteBaseCtor_orig;
void* __fastcall PaletteBaseCtor_re(void* self, void*) {
    TrackedCtor(self);
    F<void*>(self, 0) = JgldVA(0x1011db3c);
    return self;
}

// 0x00069220  FontBase::FontBase (thiscall, no stack argument, plain `ret` at 0x1006924f): the FontBase vtable
// 0x1011da90 into this[0] (0x10069240), returns this (0x10069246). FontBase is not a Tracked class.
Ctor_t FontBaseCtor_orig;
void* __fastcall FontBaseCtor_re(void* self, void*) {
    F<void*>(self, 0) = JgldVA(0x1011da90);
    return self;
}

// 0x0006a150  Palette::ctor(owner) (`ret 4` at 0x1006a1c1): PaletteBase::ctor through the thunk 0x10001555
// (0x1006a170), the Palette vtable 0x1011db10 (0x1006a178), +0x80c = owner (0x1006a184), +8 = 0 (0x1006a18d),
// +0x810 = 0 (0x1006a197), +0x814 = 0 (0x1006a1a4); returns this (0x1006a1ae).
Ctor1_t PaletteCtor_orig;
void* __fastcall PaletteCtor_re(void* self, void*, void* owner) {
    reinterpret_cast<Ctor_t>(JgldVA(0x10001555))(self, 0);
    F<void*>(self, 0) = JgldVA(0x1011db10);
    F<void*>(self, 0x80c) = owner;
    F<int>(self, 8) = 0;
    F<int>(self, 0x810) = 0;
    F<int>(self, 0x814) = 0;
    return self;
}

// 0x00068660  Font::Font(owner) (`ret 4` at 0x1006871e; the SEH frame 0x10068663..0x10068671 only brackets the
// member construction): FontBase::FontBase on this through the thunk 0x10001898 (0x10068695), Tracked::ctor on
// this + 8 (0x100686a7), the Font vtable 0x1011da6c into this[0] (0x100686af), the vtable 0x1011da68 into this[2]
// (0x100686b8, over Tracked's), +4 = owner (0x100686c5), then 0 into +0x10, +0x14, +0x18, +0x1c and +0x20
// (0x100686cb..0x100686f3); returns this (0x10068701).
Ctor1_t FontCtor_orig;
void* __fastcall FontCtor_re(void* self, void*, void* owner) {
    reinterpret_cast<Ctor_t>(JgldVA(0x10001898))(self, 0);
    TrackedCtor(static_cast<char*>(self) + 8);
    F<void*>(self, 0) = JgldVA(0x1011da6c);
    F<void*>(self, 8) = JgldVA(0x1011da68);
    F<void*>(self, 4) = owner;
    for (unsigned off = 0x10; off <= 0x20; off += 4)
        F<int>(self, off) = 0;
    return self;
}

// 0x00067fa0 / 0x000680a0 / 0x00068300  Array_e0::ctor / Array_Rec14::ctor / Array_Rec94::ctor (thiscall, no
// stack argument, plain `ret`): the class vtable 0x1011da58 / 0x1011da5c / 0x1011da60 into this[0] (0x10067fc0 /
// 0x100680c0 / 0x10068320), +4 (the element block) = 0 (0x10067fc9 / 0x100680c9 / 0x10068329), then the class's
// clearRet through the thunk 0x100013cf / 0x100019ce / 0x1000191f (0x10067fd3 / 0x100680d3 / 0x10068333; with +4 = 0
// the clear frees nothing and resets +8 / +0xc / +0x10); returns this.
Ctor_t ArrayE0Ctor_orig, ArrayRec14Ctor_orig, ArrayRec94Ctor_orig;
void* ArrayCtor(void* self, unsigned vt, unsigned clearThunk) {
    F<void*>(self, 0) = JgldVA(vt);
    F<int>(self, 4) = 0;
    reinterpret_cast<Method0_t>(JgldVA(clearThunk))(self, 0);
    return self;
}
void* __fastcall ArrayE0Ctor_re(void* self, void*) { return ArrayCtor(self, 0x1011da58, 0x100013cf); }
void* __fastcall ArrayRec14Ctor_re(void* self, void*) { return ArrayCtor(self, 0x1011da5c, 0x100019ce); }
void* __fastcall ArrayRec94Ctor_re(void* self, void*) { return ArrayCtor(self, 0x1011da60, 0x1000191f); }

// 0x000685c0  Array_Rec14::clearRet (thiscall, no stack argument, plain `ret` at 0x100685f7): Array_Rec14::clear
// 0x10068100 through the thunk 0x1000194c (0x100685e0), then returns 0 (`xor eax, eax` 0x100685e5).
Method0_t ArrayRec14ClearRet_orig;
int __fastcall ArrayRec14ClearRet_re(void* self, void*) {
    reinterpret_cast<Method0_t>(JgldVA(0x1000194c))(self, 0);
    return 0;
}

// 0x000069b0  constructTrackedList (cdecl, no argument, plain `ret` at 0x100069e2): LinkedList::ctor 0x10006c80
// through the thunk 0x100013a7 on the module's tracked list 0x10128428 (`mov ecx, 0x10128428` 0x100069c8); eax is
// the callee's (it returns its `this`).
Void0_t ConstructTrackedList_orig;
int __cdecl ConstructTrackedList_re() {
    return reinterpret_cast<Method0_t>(JgldVA(0x100013a7))(JgldVA(0x10128428), 0);
}

// 0x00007970  Surface::ctorOwner(owner) (`ret 4` at 0x10007b69; the SEH frame only brackets the members):
// SurfaceBase::ctor through the thunk 0x10001492 (0x100079a5), initDepthInfo 0x100092e0 on this + 0x24 through
// 0x100014dd (0x100079b7), the Surface vtable 0x1011d0b0 (0x100079bf), +0x80 = owner (0x100079cb), zeroes +8,
// +0x38, +0x3c, +0x40, the byte +0x74, +0x7c and the eight handle / DC words +0x4ac..+0x4c8 (0x100079d4..
// 0x10007a68), the BITMAPINFOHEADER-shaped words +0x84 = 0x28, +0x88 = 0, +0x8c = 0, word +0x90 = 1, word +0x92 = 8,
// +0x94..+0xa0 = 0, +0xa4 = 0x100, +0xa8 = 0 (0x10007a75..0x10007af5), memset (this + 0x54, 0, 0x10) through
// 0x1007e7c0 (0x10007b0a), InitializeCriticalSection (this + 0xc) (0x10007b1b), and +0x4d0 = makeColor
// 0x1000a660 (0xff, 0, 0xff) through 0x10001582 (0x10007b34 / 0x10007b3f); returns this (0x10007b4c).
typedef void(__cdecl* Memset_t)(void*, int, unsigned);
typedef int(__cdecl* MakeColor_t)(int, int, int);
Ctor1_t SurfaceCtorOwner_orig;
void* __fastcall SurfaceCtorOwner_re(void* self, void*, void* owner) {
    reinterpret_cast<Ctor_t>(JgldVA(0x10001492))(self, 0);
    reinterpret_cast<Method0_t>(JgldVA(0x100014dd))(static_cast<char*>(self) + 0x24, 0);
    F<void*>(self, 0) = JgldVA(0x1011d0b0);
    F<void*>(self, 0x80) = owner;
    F<int>(self, 8) = 0;
    F<int>(self, 0x38) = 0;
    F<int>(self, 0x3c) = 0;
    F<int>(self, 0x40) = 0;
    F<unsigned char>(self, 0x74) = 0;
    F<int>(self, 0x7c) = 0;
    for (unsigned off = 0x4ac; off <= 0x4c8; off += 4)
        F<int>(self, off) = 0;
    F<int>(self, 0x84) = 0x28;
    F<int>(self, 0x88) = 0;
    F<int>(self, 0x8c) = 0;
    F<short>(self, 0x90) = 1;
    F<short>(self, 0x92) = 8;
    F<int>(self, 0x94) = 0;
    F<int>(self, 0x98) = 0;
    F<int>(self, 0x9c) = 0;
    F<int>(self, 0xa0) = 0;
    F<int>(self, 0xa4) = 0x100;
    F<int>(self, 0xa8) = 0;
    reinterpret_cast<Memset_t>(JgldVA(0x1007e7c0))(static_cast<char*>(self) + 0x54, 0, 0x10);
    Imp<CritSec_t>(kInitializeCriticalSection)(static_cast<char*>(self) + 0xc);
    F<int>(self, 0x4d0) = reinterpret_cast<MakeColor_t>(JgldVA(0x10001582))(0xff, 0, 0xff);
    return self;
}

// =====================================================================================================================
// group H - destructors and factories
// =====================================================================================================================
typedef BOOL(WINAPI* DeleteObject_t)(HGDIOBJ);
typedef void(__fastcall* Dtor_t)(void*, void*);
typedef void(__cdecl* Delete_t)(void*);
typedef void*(__cdecl* New_t)(unsigned);

// 0x00007cc0  Surface::release (thiscall, no stack argument, plain `ret` at 0x10007dea; returns nothing):
//   [+0x4bc] (the memory DC) != 0 (je 0x10007ce7): +0x4c4 = 0, +0x4c8 = 0 (0x10007cf0 / 0x10007cfd); when [+0x4b0]
//     (the bitmap the DC held before) != 0 (je 0x10007d11): SelectObject (dc, it) (0x10007d29), +0x4b0 = 0; then
//     DeleteDC (dc) (0x10007d4f), +0x4b8 = 0, +0x4bc = 0 (0x10007d5f / 0x10007d6c);
//   [+0x4b4] != 0 (je 0x10007d80): DeleteObject (it) (0x10007d8e), +0x4b4 = 0;
//   [+0x4ac] != 0 (je 0x10007db2): DeleteObject (it) (0x10007dc0), +0x4ac = 0.
Dtor_t SurfaceRelease_orig;
void __fastcall SurfaceRelease_re(void* self, void*) {
    if (F<HDC>(self, 0x4bc)) {
        F<int>(self, 0x4c4) = 0;
        F<int>(self, 0x4c8) = 0;
        if (F<HGDIOBJ>(self, 0x4b0)) {
            Imp<SelectObject_t>(kSelectObject)(F<HDC>(self, 0x4bc), F<HGDIOBJ>(self, 0x4b0));
            F<HGDIOBJ>(self, 0x4b0) = 0;
        }
        Imp<DeleteDC_t>(kDeleteDC)(F<HDC>(self, 0x4bc));
        F<HDC>(self, 0x4b8) = 0;
        F<HDC>(self, 0x4bc) = 0;
    }
    if (F<HGDIOBJ>(self, 0x4b4)) {
        Imp<DeleteObject_t>(kDeleteObject)(F<HGDIOBJ>(self, 0x4b4));
        F<HGDIOBJ>(self, 0x4b4) = 0;
    }
    if (F<HGDIOBJ>(self, 0x4ac)) {
        Imp<DeleteObject_t>(kDeleteObject)(F<HGDIOBJ>(self, 0x4ac));
        F<HGDIOBJ>(self, 0x4ac) = 0;
    }
}

// 0x00007bf0  Surface::dtor (thiscall, no stack argument, plain `ret` at 0x10007c95; the SEH frame brackets the
// body): the Surface vtable 0x1011d0b0 into this[0] (0x10007c25); when +0x80 (the owner) is non-zero (je
// 0x10007c3c), the owner's +4 = 0 (0x10007c47); DeleteCriticalSection (this + 0xc) (0x10007c57); Surface::release
// through the thunk 0x100010aa (0x10007c67); SurfaceBase::dtor 0x100096f0 through 0x1000149c (0x10007c76).
Dtor_t SurfaceDtor_orig;
void __fastcall SurfaceDtor_re(void* self, void*) {
    F<void*>(self, 0) = JgldVA(0x1011d0b0);
    if (void* const owner = F<void*>(self, 0x80))
        F<int>(owner, 4) = 0;
    Imp<CritSec_t>(kDeleteCriticalSection)(static_cast<char*>(self) + 0xc);
    reinterpret_cast<Dtor_t>(JgldVA(0x100010aa))(self, 0);
    reinterpret_cast<Dtor_t>(JgldVA(0x1000149c))(self, 0);
}

// 0x00007900 / 0x0000a4e0 / 0x00014c00 / 0x00016930  Surface / SurfaceBase / Sprite / SpriteBase ::deletingDtor
// (flags) (VC6 `??_G`, `ret 4`): the class destructor through the thunk 0x1000125d (Surface::dtor 0x10007bf0) /
// 0x1000149c (SurfaceBase::dtor 0x100096f0) / 0x10001735 (Sprite::dtor 0x10014de0) / 0x100017df
// (SpriteBase::~SpriteBase 0x100168e0) (0x10007920 / 0x1000a500 / 0x10014c20 / 0x10016950); when flags bit 0 is set
// (`and eax, 1; test; je` 0x10007928..0x1000792d and the same at +0x28 in the others) operator delete 0x1007e6d0
// (this) (0x10007933 ...); returns this.
typedef void*(__fastcall* DelDtor_t)(void*, void*, unsigned);
void* DeletingDtor(void* self, unsigned flags, unsigned dtorThunk) {
    reinterpret_cast<Dtor_t>(JgldVA(dtorThunk))(self, 0);
    if (flags & 1)
        reinterpret_cast<Delete_t>(JgldVA(0x1007e6d0))(self);
    return self;
}
DelDtor_t SurfaceDelDtor_orig, SurfaceBaseDelDtor_orig, SpriteDelDtor_orig, SpriteBaseDelDtor_orig;
void* __fastcall SurfaceDelDtor_re(void* self, void*, unsigned f) { return DeletingDtor(self, f, 0x1000125d); }
void* __fastcall SurfaceBaseDelDtor_re(void* self, void*, unsigned f) { return DeletingDtor(self, f, 0x1000149c); }
void* __fastcall SpriteDelDtor_re(void* self, void*, unsigned f) { return DeletingDtor(self, f, 0x10001735); }
void* __fastcall SpriteBaseDelDtor_re(void* self, void*, unsigned f) { return DeletingDtor(self, f, 0x100017df); }

// 0x00067850  Factory::createFont(owner) (`ret 4` at 0x100678de): operator new 0x1007f0f0 (0x24) (0x10067884); a
// non-NULL block (je 0x1006789a) is constructed by Font::Font (owner) through the thunk 0x1000101e (0x100678a3) and
// returned, NULL returns 0 (0x100678ad). `this` (the factory) is never read.
typedef void*(__fastcall* Factory1_t)(void*, void*, void*);
Factory1_t CreateFont_orig;
void* __fastcall CreateFont_re(void*, void*, void* owner) {
    void* const p = reinterpret_cast<New_t>(JgldVA(0x1007f0f0))(0x24);
    if (p == 0)
        return 0;
    return reinterpret_cast<Ctor1_t>(JgldVA(0x1000101e))(p, 0, owner);
}

// 0x000679d0  Factory::createSurface(owner, unused) (`ret 8` at 0x10067a61): operator new (0x4d4) (0x10067a07); a
// non-NULL block (je 0x10067a1d) is constructed by Surface::ctorOwner (owner) through the thunk 0x100017d0
// (0x10067a26), NULL returns 0 (0x10067a30). The second argument and `this` are never read.
typedef void*(__fastcall* Factory2_t)(void*, void*, void*, int);
Factory2_t CreateSurface_orig;
void* __fastcall CreateSurface_re(void*, void*, void* owner, int) {
    void* const p = reinterpret_cast<New_t>(JgldVA(0x1007f0f0))(0x4d4);
    if (p == 0)
        return 0;
    return reinterpret_cast<Ctor1_t>(JgldVA(0x100017d0))(p, 0, owner);
}

// =====================================================================================================================
// group P - libpng 1.0.5 helpers (direct calls, no thunks: libpng is compiled into jgld.dll)
// =====================================================================================================================
typedef void(__cdecl* PngFree_t)(void*, void*);
typedef int(__cdecl* PngMsg_t)(void*, const char*);

// 0x00079300  png_default_warning(png_ptr, message) (cdecl, plain `ret` at 0x1007933e): fprintf 0x10083010 (stderr
// 0x101233f0, "libpng warning: %s\n" 0x1011e0e0, message) (0x10079326); png_ptr is not read. eax is fprintf's.
typedef int(__cdecl* Fprintf_t)(void*, const char*, ...);
PngMsg_t PngDefaultWarning_orig;
int __cdecl PngDefaultWarning_re(void*, const char* msg) {
    return reinterpret_cast<Fprintf_t>(JgldVA(0x10083010))(JgldVA(0x101233f0),
                                                           static_cast<const char*>(JgldVA(0x1011e0e0)), msg);
}

// 0x00079040  png_warning(png_ptr, message) (cdecl, plain `ret` at 0x1007909d): when the warning function at
// png_ptr + 0x44 is non-NULL (cmp / je 0x1007905b / 0x1007905f) it is called cdecl with (png_ptr, message)
// (0x1007906e, `add esp, 8`); otherwise png_default_warning 0x10079300 (png_ptr, message) (0x10079085). eax is the
// called function's.
PngMsg_t PngWarning_orig;
int __cdecl PngWarning_re(void* png, const char* msg) {
    if (PngMsg_t fn = F<PngMsg_t>(png, 0x44))
        return fn(png, msg);
    return reinterpret_cast<PngMsg_t>(JgldVA(0x10079300))(png, msg);
}

// 0x00079250  png_chunk_warning(png_ptr, message) (cdecl, plain `ret` at 0x100792a5): png_format_buffer 0x10079100
// (png_ptr, buffer, message) into a local buffer at ebp-0x50 (0x1007927a), then png_warning 0x10079040 (png_ptr,
// buffer) (0x1007928a); eax is png_warning's. The buffer here is larger than the original's 0x50 bytes; the
// messages the fixture passes keep png_format_buffer's output (the chunk name, up to four "[XX]" escapes, ": "
// and at most 64 message bytes) well inside both.
typedef void(__cdecl* PngFormat_t)(void*, char*, const char*);
PngMsg_t PngChunkWarning_orig;
int __cdecl PngChunkWarning_re(void* png, const char* msg) {
    char buf[0x80];
    reinterpret_cast<PngFormat_t>(JgldVA(0x10079100))(png, buf, msg);
    return reinterpret_cast<PngMsg_t>(JgldVA(0x10079040))(png, buf);
}

// 0x00078980  png_info_destroy(png_ptr, info_ptr) (cdecl, plain `ret` at 0x10078a9a), png_free 0x10078ed0 on each
// block the info struct owns:
//   text array +0x38 non-NULL (je 0x1007899f): png_free (key, the dword at +4 of each 16-byte entry) for i in
//     [0, num_text +0x30) (signed `cmp; jge` 0x100789b9 / 0x100789bc; 0x100789ca), then png_free (the array)
//     (0x100789e8);
//   png_free (+0xa0) (0x100789fe) and png_free (+0xac) (0x10078a14) unconditionally (pCAL purpose and units);
//   pCAL parameter array +0xb0 non-NULL (je 0x10078a26): png_free (params[i]) for i in [0, the byte +0xb5) (signed
//     compare 0x10078a45 / 0x10078a48), then png_free (the array) (0x10078a76);
//   png_info_init 0x10078940 (info_ptr) (0x10078a82): memset (info, 0, 0xb8).
typedef void(__cdecl* PngInfoInit_t)(void*);
typedef void(__cdecl* PngInfoDestroy_t)(void*, void*);
PngInfoDestroy_t PngInfoDestroy_orig;
void __cdecl PngInfoDestroy_re(void* png, void* info) {
    const PngFree_t pngFree = reinterpret_cast<PngFree_t>(JgldVA(0x10078ed0));
    if (char* const text = F<char*>(info, 0x38)) {
        for (int i = 0; i < F<int>(info, 0x30); i++)
            pngFree(png, F<void*>(text, i * 16 + 4));
        pngFree(png, text);
    }
    pngFree(png, F<void*>(info, 0xa0));
    pngFree(png, F<void*>(info, 0xac));
    if (void** const params = F<void**>(info, 0xb0)) {
        for (int i = 0; i < static_cast<int>(F<unsigned char>(info, 0xb5)); i++)
            pngFree(png, params[i]);
        pngFree(png, params);
    }
    reinterpret_cast<PngInfoInit_t>(JgldVA(0x10078940))(info);
}

// 0x0006d150  png_destroy_read_struct(png_ptr_ptr, info_ptr_ptr, end_info_ptr_ptr) (cdecl, plain `ret` at
// 0x1006d242): each pointer-to-pointer is read only when non-NULL (je 0x1006d181 / 0x1006d18f / 0x1006d19d; the
// locals start at 0). png_read_destroy 0x1006d250 (png, info, end) (0x1006d1b3). Info non-NULL (je 0x1006d1bf):
// png_free (png, info->text +0x38) (0x1006d1cc), png_destroy_struct 0x10078e20 (info) (0x1006d1d8),
// *info_ptr_ptr = NULL (0x1006d1e3). End info the same (je 0x1006d1ed; 0x1006d1fa, 0x1006d206, 0x1006d211). Png
// non-NULL (je 0x1006d21b): png_destroy_struct (png) (0x1006d221), *png_ptr_ptr = NULL (0x1006d22c). Note that a
// non-NULL info / end pointer is written through its pointer-to-pointer, which was tested non-NULL to get it.
typedef void(__cdecl* PngReadDestroy_t)(void*, void*, void*);
typedef void(__cdecl* PngDestroyStruct_t)(void*);
typedef void(__cdecl* PngDestroyRead_t)(void**, void**, void**);
PngDestroyRead_t PngDestroyRead_orig;
void __cdecl PngDestroyRead_re(void** pp, void** pi, void** pe) {
    void* const png = pp ? *pp : 0;
    void* const info = pi ? *pi : 0;
    void* const end = pe ? *pe : 0;
    reinterpret_cast<PngReadDestroy_t>(JgldVA(0x1006d250))(png, info, end);
    const PngFree_t pngFree = reinterpret_cast<PngFree_t>(JgldVA(0x10078ed0));
    const PngDestroyStruct_t destroy = reinterpret_cast<PngDestroyStruct_t>(JgldVA(0x10078e20));
    if (info) {
        pngFree(png, F<void*>(info, 0x38));
        destroy(info);
        *pi = 0;
    }
    if (end) {
        pngFree(png, F<void*>(end, 0x38));
        destroy(end);
        *pe = 0;
    }
    if (png) {
        destroy(png);
        *pp = 0;
    }
}

// =====================================================================================================================
// group D - Surface depth dispatchers and setPalette (jgld's Surface vtable 0x1011d0b0; slot +0xe4 returns a
// pointer to {bits per pixel, format})
// =====================================================================================================================
typedef int*(__fastcall* FormatSlot_t)(void*, void*);
inline int SurfDepth(void* s) { return *reinterpret_cast<FormatSlot_t>(VSlot(s, 0xe4))(s, 0); }

// 0x00009770  Surface::fillRectDispatch(rect, colour) (`ret 8` at 0x100097eb; Surface vtable slot +0x44): depth 8
// (je 0x100097ad) -> fillRectC8 0x10011dd0 (rect, colour) through the thunk 0x100011f4 (0x100097c2); depth 0x10
// (je 0x100097b3) -> fillRect16 0x1000dcf0 (rect, colour) through 0x100013de (0x100097d4); any other depth draws
// nothing. Always returns 0 (`xor eax, eax` 0x100097d9).
typedef int(__fastcall* RectColour_t)(void*, void*, void*, int);
typedef int(__fastcall* MethodPI_t)(void*, void*, void*, int);
MethodPI_t FillRectDispatch_orig;
int __fastcall FillRectDispatch_re(void* self, void*, void* rect, int colour) {
    const int depth = SurfDepth(self);
    if (depth == 8)
        reinterpret_cast<RectColour_t>(JgldVA(0x100011f4))(self, 0, rect, colour);
    else if (depth == 0x10)
        reinterpret_cast<RectColour_t>(JgldVA(0x100013de))(self, 0, rect, colour);
    return 0;
}

// 0x000095a0  Surface::drawDashedLine(x1, y1, x2, y2, c1, c2, len1, len2, phase) (`ret 0x24` at 0x10009653;
// Surface vtable slot): depth 8 (je 0x100095dd) -> dashedLine8 0x10010870 through the thunk 0x10001901
// (0x1000960e); depth 0x10 (je 0x100095e3) -> dashedLine16 0x1000c140 through 0x10001258 (0x1000963c); both with
// the nine arguments in order; any other depth draws nothing. Always returns 0 (0x10009641).
typedef void(__fastcall* Dashed_t)(void*, void*, int, int, int, int, int, int, int, int, int);
typedef int(__fastcall* Method9_t)(void*, void*, int, int, int, int, int, int, int, int, int);
Method9_t DrawDashedLine_orig;
int __fastcall DrawDashedLine_re(void* self, void*, int x1, int y1, int x2, int y2, int c1, int c2, int len1,
                                 int len2, int phase) {
    const int depth = SurfDepth(self);
    if (depth == 8)
        reinterpret_cast<Dashed_t>(JgldVA(0x10001901))(self, 0, x1, y1, x2, y2, c1, c2, len1, len2, phase);
    else if (depth == 0x10)
        reinterpret_cast<Dashed_t>(JgldVA(0x10001258))(self, 0, x1, y1, x2, y2, c1, c2, len1, len2, phase);
    return 0;
}

// 0x000089d0  Surface::setPalette(palette) (`ret 4` at 0x10008aa2): a NULL palette returns 0 (jne 0x100089fd not
// taken; eax still holds the argument). Otherwise +0x7c = palette (0x10008a0a); Palette::getField810 0x1000aef0
// (through 0x100012cb, 0x10008a10) is compared with +0x78 (0x10008a18): equal returns that value (je 0x10008a1b).
// Different: +0x78 = getField810 again (0x10008a20 / 0x10008a28), Palette::toRGBQuads 0x1006ae50 into a 0x400-byte
// local (through 0x100013c0, 0x10008a35), the DC from slot +0x28 (0x10008a44): NULL returns 0 (je 0x10008a50);
// else SetDIBColorTable ([this+0x4b8], 0, 0x100, quads) (0x10008a6c) and slot +0x2c with 1 (0x10008a85), whose eax
// is returned (Surface::releaseDC leaves `this` there).
typedef BOOL(WINAPI* SetDIBColorTable_t)(HDC, UINT, UINT, const void*);
typedef void(__fastcall* ToQuads_t)(void*, void*, void*);
const unsigned kSetDIBColorTable = 0x1012c394;
Method1_t SetPalette_orig;
int __fastcall SetPalette_re(void* self, void*, void* pal) {
    if (pal == 0)
        return 0;
    F<void*>(self, 0x7c) = pal;
    const Method0_t field810 = reinterpret_cast<Method0_t>(JgldVA(0x100012cb));
    const int f = field810(pal, 0);
    if (F<int>(self, 0x78) == f)
        return f;
    F<int>(self, 0x78) = field810(pal, 0);
    unsigned char quads[0x400];
    reinterpret_cast<ToQuads_t>(JgldVA(0x100013c0))(pal, 0, quads);
    if (SurfDC(self) == 0)
        return 0;
    Imp<SetDIBColorTable_t>(kSetDIBColorTable)(F<HDC>(self, 0x4b8), 0, 0x100, quads);
    return reinterpret_cast<ReleaseDC_t>(VSlot(self, 0x2c))(self, 0, 1);
}

}  // namespace

// 0x000675e0  Display::showWindow
SG_HOOK("jgld.dll", 0x000675E0, Display_showWindow, ShowWindow_re, ShowWindow_orig);
// 0x00067640  Display::hideWindow
SG_HOOK("jgld.dll", 0x00067640, Display_hideWindow, HideWindow_re, HideWindow_orig);
// 0x000676a0  Display::minimizeWindow
SG_HOOK("jgld.dll", 0x000676A0, Display_minimizeWindow, MinimizeWindow_re, MinimizeWindow_orig);
// 0x00067700  Display::maximizeWindow
SG_HOOK("jgld.dll", 0x00067700, Display_maximizeWindow, MaximizeWindow_re, MaximizeWindow_orig);
// 0x00066650  Display::getWindowRect
SG_HOOK("jgld.dll", 0x00066650, Display_getWindowRect, GetWindowRect_re, GetWindowRect_orig);
// 0x00066710  Display::invalidate
SG_HOOK("jgld.dll", 0x00066710, Display_invalidate, Invalidate_re, Invalidate_orig);
// 0x00066810  Display::setWindowPos
SG_HOOK("jgld.dll", 0x00066810, Display_setWindowPos, SetWindowPos_re, SetWindowPos_orig);
// 0x00066780  Display::getCursor
SG_HOOK("jgld.dll", 0x00066780, Display_getCursor, GetCursor_re, GetCursor_orig);
// 0x00066970  Display::restore
SG_HOOK("jgld.dll", 0x00066970, Display_restore, Restore_re, Restore_orig);
// 0x00066890  Display::setResolution
SG_HOOK("jgld.dll", 0x00066890, Display_setResolution, SetResolution_re, SetResolution_orig);
// 0x00066200  Display::setDesktopMode
SG_HOOK("jgld.dll", 0x00066200, Display_setDesktopMode, SetDesktopMode_re, SetDesktopMode_orig);
// 0x000656c0  Display::shutdown
SG_HOOK("jgld.dll", 0x000656C0, Display_shutdown, Shutdown_re, Shutdown_orig);
// 0x00068fa0  initFontTable
SG_HOOK("jgld.dll", 0x00068FA0, initFontTable, InitFontTable_re, InitFontTable_orig);
// 0x00067e40  Display::shutdownFonts
SG_HOOK("jgld.dll", 0x00067E40, Display_shutdownFonts, ShutdownFonts_re, ShutdownFonts_orig);
// 0x00067ed0  Display::freePalette
SG_HOOK("jgld.dll", 0x00067ED0, Display_freePalette, FreePalette_re, FreePalette_orig);
// 0x0006a860  Palette::animate
SG_HOOK("jgld.dll", 0x0006A860, Palette_animate, Animate_re, Animate_orig);
// 0x00014550  trace
SG_HOOK("jgld.dll", 0x00014550, trace, Trace_re, Trace_orig);
// 0x00065300  DllMain
SG_HOOK("jgld.dll", 0x00065300, DllMain, DllMain_re, DllMain_orig);
// 0x00069ac0  onPaint
SG_HOOK("jgld.dll", 0x00069AC0, onPaint, OnPaint_re, OnPaint_orig);
// 0x00069b90  onActivate
SG_HOOK("jgld.dll", 0x00069B90, onActivate, OnActivate_re, OnActivate_orig);
// 0x00008d00  Surface::setTextColor16
SG_HOOK("jgld.dll", 0x00008D00, Surface_setTextColor16, SetTextColor16_re, SetTextColor16_orig);
// 0x00008c40  Surface::setTextColorRGB
SG_HOOK("jgld.dll", 0x00008C40, Surface_setTextColorRGB, SetTextColorRGB_re, SetTextColorRGB_orig);
// 0x00008ae0  Surface::selectObject
SG_HOOK("jgld.dll", 0x00008AE0, Surface_selectObject, SelectObject_re, SelectObject_orig);
// 0x00008b90  Surface::selectSystemFont
SG_HOOK("jgld.dll", 0x00008B90, Surface_selectSystemFont, SelectSystemFont_re, SelectSystemFont_orig);
// 0x00008da0  Surface::textOut
SG_HOOK("jgld.dll", 0x00008DA0, Surface_textOut, TextOut_re, TextOut_orig);
// 0x00068e60  Font::textWidth
SG_HOOK("jgld.dll", 0x00068E60, Font_textWidth, TextWidth_re, TextWidth_orig);
// 0x00009690  SurfaceBase::ctor
SG_HOOK("jgld.dll", 0x00009690, SurfaceBase_ctor, SurfaceBaseCtor_re, SurfaceBaseCtor_orig);
// 0x00014c70  SpriteBase::ctor
SG_HOOK("jgld.dll", 0x00014C70, SpriteBase_ctor, SpriteBaseCtor_re, SpriteBaseCtor_orig);
// 0x0006a260  PaletteBase::ctor
SG_HOOK("jgld.dll", 0x0006A260, PaletteBase_ctor, PaletteBaseCtor_re, PaletteBaseCtor_orig);
// 0x00069220  FontBase::FontBase
SG_HOOK("jgld.dll", 0x00069220, FontBase_FontBase, FontBaseCtor_re, FontBaseCtor_orig);
// 0x0006a150  Palette::ctor
SG_HOOK("jgld.dll", 0x0006A150, Palette_ctor, PaletteCtor_re, PaletteCtor_orig);
// 0x00068660  Font::Font
SG_HOOK("jgld.dll", 0x00068660, Font_Font, FontCtor_re, FontCtor_orig);
// 0x00067fa0  Array_e0::ctor
SG_HOOK("jgld.dll", 0x00067FA0, Array_e0_ctor, ArrayE0Ctor_re, ArrayE0Ctor_orig);
// 0x000680a0  Array_Rec14::ctor
SG_HOOK("jgld.dll", 0x000680A0, Array_Rec14_ctor, ArrayRec14Ctor_re, ArrayRec14Ctor_orig);
// 0x00068300  Array_Rec94::ctor
SG_HOOK("jgld.dll", 0x00068300, Array_Rec94_ctor, ArrayRec94Ctor_re, ArrayRec94Ctor_orig);
// 0x000685c0  Array_Rec14::clearRet
SG_HOOK("jgld.dll", 0x000685C0, Array_Rec14_clearRet, ArrayRec14ClearRet_re, ArrayRec14ClearRet_orig);
// 0x000069b0  constructTrackedList
SG_HOOK("jgld.dll", 0x000069B0, constructTrackedList, ConstructTrackedList_re, ConstructTrackedList_orig);
// 0x00007970  Surface::ctorOwner
SG_HOOK("jgld.dll", 0x00007970, Surface_ctorOwner, SurfaceCtorOwner_re, SurfaceCtorOwner_orig);
// 0x00007cc0  Surface::release
SG_HOOK("jgld.dll", 0x00007CC0, Surface_release, SurfaceRelease_re, SurfaceRelease_orig);
// 0x00007bf0  Surface::dtor
SG_HOOK("jgld.dll", 0x00007BF0, Surface_dtor, SurfaceDtor_re, SurfaceDtor_orig);
// 0x00007900  Surface::deletingDtor
SG_HOOK("jgld.dll", 0x00007900, Surface_deletingDtor, SurfaceDelDtor_re, SurfaceDelDtor_orig);
// 0x0000a4e0  SurfaceBase::deletingDtor
SG_HOOK("jgld.dll", 0x0000A4E0, SurfaceBase_deletingDtor, SurfaceBaseDelDtor_re, SurfaceBaseDelDtor_orig);
// 0x00014c00  Sprite::deletingDtor
SG_HOOK("jgld.dll", 0x00014C00, Sprite_deletingDtor, SpriteDelDtor_re, SpriteDelDtor_orig);
// 0x00016930  SpriteBase::deletingDtor
SG_HOOK("jgld.dll", 0x00016930, SpriteBase_deletingDtor, SpriteBaseDelDtor_re, SpriteBaseDelDtor_orig);
// 0x00067850  Factory::createFont
SG_HOOK("jgld.dll", 0x00067850, Factory_createFont, CreateFont_re, CreateFont_orig);
// 0x000679d0  Factory::createSurface
SG_HOOK("jgld.dll", 0x000679D0, Factory_createSurface, CreateSurface_re, CreateSurface_orig);
// 0x00079300  png_default_warning
SG_HOOK("jgld.dll", 0x00079300, png_default_warning, PngDefaultWarning_re, PngDefaultWarning_orig);
// 0x00079040  png_warning
SG_HOOK("jgld.dll", 0x00079040, png_warning, PngWarning_re, PngWarning_orig);
// 0x00079250  png_chunk_warning
SG_HOOK("jgld.dll", 0x00079250, png_chunk_warning, PngChunkWarning_re, PngChunkWarning_orig);
// 0x00078980  png_info_destroy
SG_HOOK("jgld.dll", 0x00078980, png_info_destroy, PngInfoDestroy_re, PngInfoDestroy_orig);
// 0x0006d150  png_destroy_read_struct
SG_HOOK("jgld.dll", 0x0006D150, png_destroy_read_struct, PngDestroyRead_re, PngDestroyRead_orig);
// 0x00009770  Surface::fillRectDispatch
SG_HOOK("jgld.dll", 0x00009770, Surface_fillRectDispatch, FillRectDispatch_re, FillRectDispatch_orig);
// 0x000095a0  Surface::drawDashedLine
SG_HOOK("jgld.dll", 0x000095A0, Surface_drawDashedLine, DrawDashedLine_re, DrawDashedLine_orig);
// 0x000089d0  Surface::setPalette
SG_HOOK("jgld.dll", 0x000089D0, Surface_setPalette, SetPalette_re, SetPalette_orig);
