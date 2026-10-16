// C3 batch c3fy (2026-10-16): mainLoop 0x0040f5c0 of golf_clean.exe, the game's session and frame loop.
// Spec (phase list, globals, jump tables): re/analysis/sim/mainLoop_structure.md. Dependencies: log/c3/c3fy_deps.tsv.
// Fixtures: re/frida/js/fixtures.d/c3fy.js (none; path-1 N/A). Registry: re/frida/registry.d/c3fy.py (empty). Notes:
// log/c3/c3fy_notes.txt. Negctl control: shim/src/re/c3fy_negctl.cpp.
//
// EXTENT. ONE contiguous body 0x0040f5c0..0x00421615 (73,813 bytes, 21,541 instructions, 27 switch tables at
// 0x00421618..0x00421b23, one `ret` at 0x0040fb04), measured by log/c3/alt/c3fy_cfg.py from the entry with every
// switch table followed. re/functions_ghidra.tsv recorded only 8,035 bytes in two ranges (Ghidra stopped at the switch
// `jmp [ecx*4 + 0x421634]` at 0x004115d5), so ~65 KB of the loop lay outside every function in the master.
//
// METHOD. Hand-written from the disassembly (log/c3/alt/c3fy_norm.asm: every [esp+X] rewritten as a frame offset
// [F+X], F = esp after the prologue's `push edi` at 0x0040f5e1). The body is the original's phase order (the dominator
// chain of the frame loop, log/c3/alt/c3fy_spine.py): each phase is a function here, called in the original's order.
// Callees run through their original addresses (a hooked callee runs its own reimplementation). __thiscall is emulated
// with __fastcall. The verifier wrote Setup / Menu / Session / FramePreamble / MainLoopBody; the per-frame phases P1..H2
// were transcribed range by range (log/c3/c3fy_brief.md), each cross-checked by compile and, for several, a Unicorn or
// scenario A/B.
//
// FRAME. The entry is naked and builds the original's SEH frame at the same address: the SEH record (state -1, handler
// 0x004b7372, previous fs:[0]), __alloca_probe 0x004a6070 for 0x1238 bytes, then ebx / esi / edi / ebp (the last is the
// phantom slot the original pushes once at 0x0040f982). The body keeps every frame slot it shares with the original at
// the original's offset from F, so the slots the original reads before writing, the pointers it hands to callees and the
// SEH unwind all line up (log/c3/alt/c3fy_uninit.py, c3fy_frame_slots.txt). F = entry_esp - 0x1250.
//
// FIXES (c3fy2, 2026-10-16; U-0008 resolved). Eight transcription defects, localised by the scenario A/B's opt-in
// per-poll trace and write watch (re/frida/scenario_ab.py --trace / --watch) and swept by three audits of the listing
// (log/c3/c3fy2_hazards.py, c3fy2_setter_check.py, c3fy2_strcheck.py): a jcc / setcc that read a register reloaded
// after the flag-setting compare (0x004139f5 and 0x00413a60, the U-0008 downhill / uphill flags; 0x00414a91; 0x004156b7),
// a jne whose flags come from its only predecessor's compare (0x00414e91), eax not loaded before the switch it feeds
// (0x0041e38f), and two strcpy idioms written as strcat (0x0041cabc, 0x0041ec50). Notes: log/c3/c3fy2_notes.txt.
#include "hooks.h"

namespace {

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef unsigned int u32;

template <typename T> static inline T& G(u32 addr) { return *reinterpret_cast<T*>(addr); }
#define G32(a) G<int>(a)
#define GU32(a) G<u32>(a)
#define G16(a) G<short>(a)
#define GU16(a) G<u16>(a)
#define G8(a) G<s8>(a)
#define GU8(a) G<u8>(a)

// frame slots at the original's offsets from F (F = esp after `push edi` at 0x0040f5e1)
#define FD(o) (*reinterpret_cast<int*>(F + (o)))
#define FU(o) (*reinterpret_cast<u32*>(F + (o)))
#define FW(o) (*reinterpret_cast<short*>(F + (o)))
#define FUW(o) (*reinterpret_cast<u16*>(F + (o)))
#define FB(o) (*reinterpret_cast<s8*>(F + (o)))
#define FUB(o) (*reinterpret_cast<u8*>(F + (o)))
#define FA(o) (reinterpret_cast<int>(F + (o)))

// callee at its original address: cdecl, and thiscall emulated with fastcall (ecx = self, edx unused).
// Pass every argument as int (or u32): a char / short template argument would push garbage upper bytes.
template <typename... A> static inline int CC(u32 addr, A... a) {
    return reinterpret_cast<int(__cdecl*)(A...)>(addr)(a...);
}
template <typename... A> static inline int TC(u32 addr, int self, A... a) {
    return reinterpret_cast<int(__fastcall*)(int, int, A...)>(addr)(self, 0, a...);
}

const u32 kBuf = 0x0051A068;   // the shared text buffer
static inline int Rnd(int n) { return TC(0x0045C1E0, 0x00822D9C, n); }            // Random::range(n) on 0x00822d9c
static inline int Itoa(int v, u32 buf, int radix) { return CC(0x004AD425, v, (int)buf, radix); }   // __itoa
static inline int Root() { return G32(0x004C1570); }                                 // the draw-target object

// VC6's inline strcpy / strcat idioms (repne scasb + rep movsd / movsb), expanded in place by the original
static inline void StrCpy(u32 dst, u32 src) {
    char* d = reinterpret_cast<char*>(dst);
    const char* s = reinterpret_cast<const char*>(src);
    while ((*d++ = *s++) != 0) {
    }
}
static inline void StrCat(u32 dst, u32 src) {
    char* d = reinterpret_cast<char*>(dst);
    while (*d) d++;
    StrCpy(reinterpret_cast<u32>(d), src);
}

// phase exits (where control goes after a phase that can leave the frame's straight order)
enum PhExit { kNext = 0, kFrame = 1, kRetile = 2, kMenu = 3 };
// kNext   = fall through to the next phase in the original's order
// kFrame  = jump to the frame head 0x0040fc29 (timeGetTime, next frame)
// kRetile = jump to 0x0040fc24 (rebuildTileCoverage, then the frame head)
// kMenu   = jump to the title menu 0x00420e35

// Negctl control (dormant unless built with -DC3FY_NEG, i.e. through shim/src/re/c3fy_negctl.cpp, which is never listed
// in shim/re_batches.txt). The wrong body is chosen by c3fyNegMode(): the env SIMGOLF_C3FY_NEG when it is set (the
// scenario A/B passes it to the ON arm with --env), else the exported switch c3fyNegCtl[0] (c3fy_negctl.cpp starts it
// at 1). No fixed-address page (the switch is an exported BSS word).
//   1  one extra Random::range draw per frame, before the preamble: shifts the RNG seed 0x00822d9c, so every scenario
//      reads RED from its first in-game checkpoint.
//   2  (c3fy2) the U-0008 defect put back in P3: the downhill test at 0x00413a60 compares the hole index [0x005685f0]
//      with the tee height - 1 instead of the green height. A wrong body in the middle of the frame loop that only a
//      scenario with a hole under construction reaches (course_season); sandbox_basic never runs that block.
#if defined(C3FY_NEG)
extern "C" __declspec(dllexport) volatile int c3fyNegCtl[4];
static int c3fyNegMode() {
    static int envMode = -2;                        // -2 not read yet, -1 env not set
    if (envMode == -2) {
        char buf[16];
        DWORD n = GetEnvironmentVariableA("SIMGOLF_C3FY_NEG", buf, sizeof buf);
        int v = -1;
        if (n > 0 && n < sizeof buf) {
            v = 0;
            for (DWORD i = 0; i < n && buf[i] >= '0' && buf[i] <= '9'; i++) v = v * 10 + (buf[i] - '0');
        }
        envMode = v;
    }
    return envMode >= 0 ? envMode : c3fyNegCtl[0];
}
#endif

}  // namespace
namespace {

typedef void(__cdecl* MainLoop_t)();
void __stdcall MainLoopBody(u8* F);

// 0x0040f5c0  mainLoop: the naked entry rebuilds the original's frame (prologue 0x0040f5c0..0x0040f5e1 plus the phantom
// `push ebp` the original makes once at 0x0040f982), hands F to the body, then unwinds exactly as the original epilogue
// 0x0040faec..0x0040fb04. F = esp after `push ebp`, +4 = the base the body uses (= entry_esp - 0x1250).
__declspec(naked) void __cdecl MainLoop_re() {
    __asm {
        mov eax, dword ptr fs:[0]
        push -1
        push 0x004B7372
        push eax
        mov eax, 0x1238
        mov dword ptr fs:[0], esp
        mov ecx, 0x004A6070
        call ecx
        push ebx
        push esi
        push edi
        push ebp
        lea eax, [esp + 4]
        push eax
        call MainLoopBody
        mov ecx, dword ptr [esp + 0x1248]
        pop ebp
        pop edi
        pop esi
        pop ebx
        mov dword ptr fs:[0], ecx
        add esp, 0x1244
        ret
    }
}

}  // namespace

MainLoop_t MainLoop_orig;
SG_HOOK("golf_clean.exe", 0x0040f5c0, mainLoop, MainLoop_re, MainLoop_orig);

// c3fy verifier-owned parts of mainLoop: Setup 0x0040f5e2..0x0040f9f7, Session 0x0040f9fc..0x0040fc24,
// the frame preamble 0x0040fc24..0x00410604, and the title menu 0x00420e35..0x00421610. Compile-checked alone with
// log/c3/alt/c3fy_check.bat V (the Ph_* phase functions are declared extern here; MainLoopBody is assembled last in
// shim/src/re/c3fy.cpp). Every address cited is from log/c3/alt/c3fy_norm.asm.
namespace {

// phase entry points written by the part files P1..H2 (merged into c3fy.cpp)
void Ph_00410604(u8* F);
void Ph_0041234e(u8* F);
void Ph_0041353d(u8* F);
void Ph_00415847(u8* F);
void Ph_004175f2(u8* F);
int Ph_00418bc7(u8* F);
void Ph_00419d06(u8* F, int eax_in);
int Ph_0041c0c5(u8* F, int* h2in);
int Ph_0041ecbb(u8* F, const int* h2in);

// ---- callee shorthands used across the verifier parts ----
static inline void PlaySound(int a, int b, int c, int d, int e) { CC(0x004481B0, a, b, c, d, e); }
static inline void StartSoundSlot(int s) { CC(0x00448200, s); }
static inline int SurfaceCreate(int self, int w, int h, int a, int b, int c, int d) {
    return TC(0x00474DD0, self, w, h, a, b, c, d);
}
static inline void SetDrawTarget(int self, int a, int b, int c, int d) { TC(0x004762D0, self, a, b, c, d); }
static inline void Blit3(int self, int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    TC(0x00475D00, self, a, b, c, d, e, f, g, h, i);
}
static inline void ImageDraw(int self, int a, int b, int c, int d) { TC(0x00473E60, self, a, b, c, d); }
static inline void DrawCentered(int a, int b, int c, int d) { CC(0x00404B70, a, b, c, d); }

// ====================================================================================================================
// Setup 0x0040f5e2..0x0040f9f7: one-time boot. Builds the three menu Surfaces, the title-screen button geometry, the
// title art, the random seed, the top scores and the default pro, the info-screen and interface art, the ticker tapes
// and the autosave probe, then jumps to the title menu.
// ====================================================================================================================
void Setup(u8* F) {
    PlaySound(0x80, 0x64, 0, 0, 0);                                                     // 0x0040f5ef
    for (int i = 0; i < 0x114; i++) G32(0x00578350 + 4 * i) = G32(0x004C1A40 + 4 * i);  // 0x0040f60e memcpy tile names
    SurfaceCreate(0x00568D10, 0x320, 0x258, 0, 1, 0, 0);                                 // 0x0040f61f
    SurfaceCreate(0x00587AE8, 0x320, 0x258, 0, 1, 0, 0);                                 // 0x0040f63b
    SurfaceCreate(0x005A7148, 0x8C, 0x1A4, 8, 1, 0, 0);                                  // 0x0040f657
    TC(0x00482FD0, FA(0x2B0));                                                           // 0x0040f663 Cache482fd0::ctor
    FD(0x124C) = 0;                                                                      // 0x0040f672 SEH scope state
    // title button geometry (0x0040f67d..0x0040f6fb, 0x0040f705..0x0040f771): two word tables the menu draw reads
    const short gy[7] = {0x2A, 0x1A0, 0x1FE, 0x11F, 0xD, (short)0x2D1, 0xA8};            // y at F+0x150+4i
    const short gx[7] = {0x15, 0x1F, 0x167, 0x1DA, 0x17E, 0x210, 0xC6};                 // x at F+0x152+4i
    const short ty[7] = {0xB6, 0x23D, 0x277, 0x19F, 0x8D, (short)0x2D1, 0};             // text y at F+0xf4+4i
    const short tx[7] = {0x42, 0x58, 0x199, 0x20D, 0x1AF, 0x210, 0};                    // text x at F+0xf6+4i
    for (int i = 0; i < 7; i++) {
        FW(0x150 + 4 * i) = gy[i];
        FW(0x152 + 4 * i) = gx[i];
        FW(0xF4 + 4 * i) = ty[i];
        FW(0xF6 + 4 * i) = tx[i];
    }
    FD(0x1F4) = 0x004C6C98;                                                              // 0x0040f779 "Continue Saved Game"
    FD(0x1F8) = 0x004C6C88;                                                              // 0x0040f784 "Start New Game"
    FD(0x1FC) = 0x004C6C78;                                                              // 0x0040f78f "Select A Theme"
    FD(0x200) = 0x004C6C64;                                                              // 0x0040f79a "Play a Championship"
    FD(0x204) = 0x004C6C54;                                                              // 0x0040f7b6 "Sandbox Mode" (raw [esp+0x214] at depth +0x10 = F+0x204)
    SetDrawTarget(Root(), 0x0051A028, 0, 0, 0);                                          // 0x0040f7c1
    TC(0x00475840, 0x00568D10, 0x004C6C3C, 0x005199C8, 0, 0x100, 0);                     // 0x0040f7de loadImage TitleBase.pcx
    Blit3(0x00568D10, Root(), 0, 0, 0x320, 0x258, 0, 0, G32(0x00822C8C), G32(0x00822C90)); // 0x0040f80e
    for (int esi = 0; esi < 7; esi++) {                                                  // 0x0040f813..0x0040f880
        const int hi = (esi == 6) ? 1 : 0;                                              // 0x0040f834 sete al
        const int k = hi + 2 * esi;                                                     // 0x0040f838
        // c3fyc fix: argument order from the pushes. 0x0040f815 eax = [F+esi*4+0x152], 0x0040f81d ecx = [F+esi*4+0x150];
        // push 0 / push eax (0x0040f827) / push ecx (0x0040f833) / push edx = Root (0x0040f837): Image_draw(Root, [+0x150],
        // [+0x152], 0) (the menu's 0x00420f0c has the same order). The old line passed +0x152 first.
        ImageDraw(0x0053B7E0 + 0x2C * k, Root(), FW(0x150 + 4 * esi), FW(0x152 + 4 * esi), 0);  // 0x0040f848
        if (esi < 5)                                                                     // 0x0040f84d..0x0040f850
            // c3fyc fix: 0x0040f852 eax = [+0xf6] + 8 (0x0040f869), 0x0040f85a ecx = [+0xf4], edx = [+0x1f4]; pushes color,
            // eax, ecx, edx (0x0040f86c..0x0040f873): drawCentered([+0x1f4], [+0xf4], [+0xf6] + 8, color). The old line
            // swapped +0xf4 and +0xf6.
            DrawCentered(FD(0x1F4 + 4 * esi), FW(0xF4 + 4 * esi), FW(0xF6 + 4 * esi) + 8, 0x80006318);  // 0x0040f874
    }
    SetDrawTarget(Root(), 0x0051A028, 0, 0, 0);                                          // 0x0040f893
    G32(0x005A5A00) = -1;                                                                // 0x0040f8a5
    G32(0x00822C88) = -1;                                                                // 0x0040f8ab session mode = none
    G32(0x00822C78) = Rnd(0xFFFF) & 0xFFFF;                                              // 0x0040f8b1 title ticker seed
    CC(0x0040AD60);                                                                      // 0x0040f8c0 loadTopScores
    if (CC(0x004378A0, 0x004C6C28))                                                      // 0x0040f8ca fileExists Themes\Default.pro
        CC(0x00437FA0, 0x004C6C1C, 0, 0);                                                // 0x0040f8df loadThemeFile Default.pro
    G8(0x005A34E0) = 0;                                                                  // 0x0040f8f0 course type = 0
    StrCpy(0x00567328, 0x004C6C10);                                                      // 0x0040f910 theme name = "Standard"
    CC(0x0044BDE0);                                                                      // 0x0040f912 loadInfoScreenArt
    CC(0x00406250, 0);                                                                   // 0x0040f919 showLoadingScreen(0)
    TC(0x00480C80, 0x00519A60, 0);                                                       // 0x0040f928 Window::refreshFull
    CC(0x00442180, 1);                                                                   // 0x0040f92f loadInterfaceArt(1)
    G32(0x0058DD34) = -1;                                                                // 0x0040f939
    G32(0x0058DD38) = -1;                                                                // 0x0040f941
    G32(0x0058DD3C) = -1;                                                                // 0x0040f947
    G32(0x0058DD40) = -1;                                                                // 0x0040f94d
    G32(0x0058DD44) = -1;                                                                // 0x0040f953
    G16(0x0058DD48) = -1;                                                                // 0x0040f959
    G8(0x0058DD4A) = -1;                                                                 // 0x0040f960
    for (int i = 0; i < 0x10; i++)                                                       // 0x0040f966..0x0040f977
        GU8(0x0058DD34 + G8(0x004C2D28 + i)) = (u8)i;                                    // scatter 0..15 by the order table
    StrCpy(0x005694A8, 0x004C6C00);                                                      // 0x0040f983 "Ticker Tape #1"
    StrCpy(0x00569528, 0x004C6BF0);                                                      // 0x0040f9a7 "Ticker Tape #2"
    StrCpy(0x005695A8, 0x004E9A84);                                                      // 0x0040f9c9 (empty default string)
    reinterpret_cast<int(WINAPI*)(int, int)>(GU32(0x004BA090))(0x004C6BD4, FA(0x1104));  // 0x0040f9f1 FindFirstFileA &AutoSave1
    // 0x0040f9f7 jmp 0x00420e35 -> the caller goes to Menu
}

}  // namespace
// c3fy part MV: the TITLE MENU of mainLoop, 0x00420e35..0x00421610.
// Entry: int Menu(u8* F). Returns the session selection ebx that the original
// carries to its `jmp 0x0040f9fc` at 0x00421610. The menu can loop back to its
// head 0x00420e35 (0x00421402 after a theme-pack preview, 0x00421508 when the
// difficulty screen is cancelled) and re-test ebx at 0x00421443 (0x004215f2).
//
// No incoming register matters: ebp is zeroed at 0x00420e3b and ebx at 0x00420ed8.
// (verified: the first use of every register in this range is a full write.)
// The only x87 in mainLoop is in P5's range, not here.
//
// Every `movsx word ptr [F+..]` (a signed short slot) is cast (int)FW(..) before
// it becomes a CC/TC argument, so MSVC pushes the sign-extended 32-bit value the
// original's `movsx; push` produces (never a short arg with garbage upper bytes).

namespace {

// Surface_blit3 full-screen copy, identical args at 0x00420ed3 / 0x004210dd /
// 0x004212ab / 0x004213e9 / 0x004215d9: blit(this, Root, 0,0, 0x320,0x258, 0,0,
// g_screenWidth [0x822c8c], g_screenHeight [0x822c90]). (thiscall, this + 9 args)
static void MV_blitFull(int self) {
    TC(0x475D00, self, Root(), 0, 0, 0x320, 0x258, 0, 0,
       G32(0x822C8C), G32(0x822C90));
}

int Menu(u8* F) {
    int ebx;  // the running selection; persists block E -> F -> H -> remap -> exit

menu_top:  // 0x00420e35
    // --- reset state + draw the title background (0x00420e35..0x00420ed3) ---
    // setDrawTarget(Root, 0x51a028, 0, 0, 0)                         0x00420e45
    TC(0x4762D0, Root(), 0x51A028, 0, 0, 0);
    GU32(0x834170) = 0;              // g_date = 0                     0x00420e4a
    GU32(0x59E7B8) = 0x40000000;     //                               0x00420e50
    CC(0x4315E0);                    // buildGolferBioText()           0x00420e5a (no pushed args)
    CC(0x401750);                    // initRecords()                  0x00420e5f
    for (int i = 0; i < 8; i++) GU32(0x5A5A04 + 4 * i) = 0;  // 0x00420e71 rep stosd
    for (int i = 0; i < 8; i++) GU32(0x542F00 + 4 * i) = 0;  // 0x00420e7d rep stosd
    // setDrawTarget(Root, 0x51a028, 0, 0, 0)                         0x00420e8c
    //   (the push ebp at 0x00420e70 was hoisted above the two rep stosd)
    TC(0x4762D0, Root(), 0x51A028, 0, 0, 0);
    // loadImage(this=0x568d10, 0x4c6c3c, 0x5199c8, 0, 0x100, 0)      0x00420ea7
    TC(0x475840, 0x568D10, 0x4C6C3C, 0x5199C8, 0, 0x100, 0);
    MV_blitFull(0x568D10);                                         // 0x00420ed3

    // --- draw the 7 menu items (0x00420ed8..0x00420ff1) ---
    for (ebx = 0; ebx < 7; ebx++) {                                // 0x00420ed8 / 0x00420fee
        // this = 44 * ((ebx==6?1:0) + 2*ebx) + 0x53b7e0            0x00420ef4..0x00420f05
        int al = (ebx == 6) ? 1 : 0;
        int t = al + ebx * 2;
        int self = t * 44 + 0x53B7E0;
        // Image_draw(self, Root, item.x [F+0x150+ebx*4], item.y [F+0x152+ebx*4], 0)  0x00420f0c
        TC(0x473E60, self, Root(), (int)FW(0x150 + ebx * 4), (int)FW(0x152 + ebx * 4), 0);
        if (ebx < 5) {                                             // 0x00420f14 jge 0x00420f40
            // drawCenteredText(F+0x1f4+ebx*4, F+0xf4+ebx*4, F+0xf6+ebx*4 + 8, 0x80006318)  0x00420f38
            CC(0x404B70, FD(0x1F4 + ebx * 4), (int)FW(0xF4 + ebx * 4),
               (int)FW(0xF6 + ebx * 4) + 8, (int)0x80006318);
        }
        if (ebx == 1) {                                            // 0x00420f43 jne 0x00420fed
            StrCpy(kBuf, 0x4C59E4);     // 0x00420f73
            StrCat(kBuf, 0x567328);     // 0x00420f9e
            StrCat(kBuf, 0x4C59E0);     // 0x00420fd5
            // drawCenteredText(kBuf, F+0xf8, F+0xfa + 0x18, 0x80006318)  0x00420fe5
            CC(0x404B70, kBuf, (int)FW(0xF8), (int)FW(0xFA) + 0x18, (int)0x80006318);
        }
    }

    CC(0x45C150);                    // pumpLight()                    0x00420ff7 (no pushed args)
    FD(0x2C) = 0;                    //                               0x00421006
    StrCpy(kBuf, 0x4C59C4);          //                               0x00421023

    // --- hover / click loop (0x00421025..0x0042127a) ---
hover_loop:  // 0x00421025
    CC(0x45BF80, 1, 0);              // waitTicks(1, 0)                0x00421029
    CC(0x483BD0);                    // pumpInput483bd0()              0x00421031 (no pushed args)
    CC(0x45C030);                    // pumpMessages()                 0x00421036 (no pushed args)
    // View_toLocal(0x519a60, &F[0x1e4], &F[0x1d8])                   0x00421050
    TC(0x47AB50, 0x519A60, FA(0x1E4), FA(0x1D8));

    // nearest menu hotspot over the 6 hotspots (0x00421055..0x004210b0)
    ebx = (FD(0x2C) != 0) ? -1 : 1;  //                               0x00421055..0x0042106d
    {
        int bestDist = 0xA0;         // ebp
        for (int esi = 0; esi < 6; esi++) {                        // 0x0042106f / 0x004210ad
            // distance(F+0x1e4 - hotspot.y [F+0xf4+esi*4],
            //          (F+0x1d8 - hotspot.x [F+0xf6+esi*4]) * 4 - 0x18)   0x00421091
            int d = CC(0x40ACD0,
                       FD(0x1E4) - (int)FW(0xF4 + esi * 4),
                       (FD(0x1D8) - (int)FW(0xF6 + esi * 4)) * 4 - 0x18);
            if (d < bestDist) {      //                               0x0042109b jge 0x004210a9
                bestDist = d;
                ebx = esi;
                FD(0x2C) = 1;        //                               0x004210a1
            }
        }
    }

    // redraw the frame with the hovered item highlighted (0x004210b2..0x00421258)
    MV_blitFull(0x568D10);                                         // 0x004210dd
    // Image_draw(this=0x53ba1c, Root, F+0x168, F+0x16a, 0)          0x00421101
    TC(0x473E60, 0x53BA1C, Root(), (int)FW(0x168), (int)FW(0x16A), 0);
    for (int ebp = 0; ebp < 6; ebp++) {                            // 0x00421106 / 0x00421255
        // al = (ebp==6) ? 1 : (ebx==ebp ? 1 : 0); the ebp==6 arm (0x0042110d
        // mov eax,1) is dead: the loop re-enters at 0x00421114 and ebp is 0..5.
        int al = (ebx == ebp) ? 1 : 0;                             // 0x00421116/0x00421118
        int t = al + ebp * 2;
        int self = t * 44 + 0x53B7E0;                              // 0x00421138..0x0042113f
        // Image_draw(self, Root, F+0x150+ebp*4, F+0x152+ebp*4, 0)  0x00421146
        TC(0x473E60, self, Root(), (int)FW(0x150 + ebp * 4), (int)FW(0x152 + ebp * 4), 0);
        if (ebp < 5) {                                             // 0x0042114e jge 0x00421190
            int color, ty;
            if (ebp == ebx) {                                      // 0x00421152 jne 0x00421166
                color = (int)0x80002108;
                ty = (int)FW(0xF6 + ebp * 4) + 6;                  // 0x00421161
            } else {
                color = (int)0x80004210;
                ty = (int)FW(0xF6 + ebp * 4) + 8;                  // 0x00421173
            }
            // drawCenteredText(F+0x1f4+ebp*4, F+0xf4+ebp*4, ty, color)  0x00421188
            CC(0x404B70, FD(0x1F4 + ebp * 4), (int)FW(0xF4 + ebp * 4), ty, color);
        }
        if (ebp == 1) {                                            // 0x00421193 jne 0x00421254
            StrCpy(kBuf, 0x4C59E4);     // 0x004211bc
            StrCat(kBuf, 0x567328);     // 0x004211e7
            StrCat(kBuf, 0x4C59E0);     // 0x00421219
            // color = (ebx==1) ? 0x80002108 : 0x80004210          0x0042120f..0x0042122a
            int color = (ebx == 1) ? (int)0x80002108 : (int)0x80004210;
            // ty = F+0xfa + 0x18 + (ebx!=1 ? 2 : 0)                0x0042122f..0x00421243
            int ty = (int)FW(0xFA) + 0x18 + ((ebx != 1) ? 2 : 0);
            // drawCenteredText(kBuf, F+0xf8, ty, color)           0x0042124c
            CC(0x404B70, kBuf, (int)FW(0xF8), ty, color);
        }
    }

    // Window::refreshFull(0x519a60, 0)                             0x00421265
    TC(0x480C80, 0x519A60, 0);
    if (G32(0x822D68) == 0) goto hover_loop;   // no mouse click     0x00421271
    if (ebx == -1) goto hover_loop;            // no item under cursor 0x0042127a

    // --- a click landed on item ebx: redraw items 0..4 un-highlit (0x00421280..0x0042130e) ---
    MV_blitFull(0x568D10);                                         // 0x004212ab
    for (int esi = 0, edi = 0x53B7E0; edi < 0x53B998; esi += 4, edi += 0x58) {  // 0x004212b7/0x0042130e
        // Image_draw(this=edi, Root, F+0x150+esi, F+0x152+esi, 0)  0x004212d3
        TC(0x473E60, edi, Root(), (int)FW(0x150 + esi), (int)FW(0x152 + esi), 0);
        // drawCenteredText(F+0x1f4+esi, F+0xf4+esi, F+0xf6+esi + 8, 0x80006318)  0x004212fa
        CC(0x404B70, FD(0x1F4 + esi), (int)FW(0xF4 + esi), (int)FW(0xF6 + esi) + 8,
           (int)0x80006318);
    }
    if (ebx >= 0 && ebx != 3 && ebx != 1) {    // 0x00421310/0x00421317/0x0042131c
        // Image_draw(this=0x53ba1c, Root, F+0x168, F+0x16a, 0)     0x0042133d
        TC(0x473E60, 0x53BA1C, Root(), (int)FW(0x168), (int)FW(0x16A), 0);
    }

    // --- remap the hotspot index to a session code via the jump table 0x00421b04 ---
    // eax = ebx + 2; if ((u32)eax > 7) no remap; else switch.      0x00421342..0x0042134a
    {
        u32 jx = (u32)(ebx + 2);
        if (jx <= 7) {
            switch (jx) {
                case 0: ebx = 0; break;          // 0x00421351
                case 1: break;                   // 0x00421376 (no remap)
                case 2: ebx = 1; break;          // 0x00421355
                case 3: ebx = 0; break;          // 0x00421351
                case 4: ebx = 4; break;          // 0x0042135c
                case 5: ebx = 2; break;          // 0x00421363
                case 6: ebx = 3; break;          // 0x0042136a
                case 7: ebx = -2; break;         // 0x00421371
            }
        }
        // in practice ebx is 0..5 here (ebx==-1 was filtered), jx is 2..7.
    }

    // 0x00421376
    CC(0x45C150);                    // pumpLight()                    0x00421376 (no pushed args)
    if (ebx == 4) {                  //                               0x0042137b jne 0x00421407
        // theme-pack select + a preview Image in F+0xe4c (0x00421384..0x004213fd)
        CC(0x4725B0);                // showThemePackSelect()          0x00421384 (no pushed args)
        TC(0x474AE0, FA(0xE4C));     // Image_ctor(&F[0xe4c])          0x00421390
        FB(0x124C) = 1;              //                               0x004213af
        // loadImage(this=&F[0xe4c], 0x4c59b8, 0x5199c8, 0, 0x100, 0) 0x004213b7
        TC(0x475840, FA(0xE4C), 0x4C59B8, 0x5199C8, 0, 0x100, 0);
        MV_blitFull(FA(0xE4C));                                    // 0x004213e9
        FB(0x124C) = 0;              //                               0x004213f5
        TC(0x474C40, FA(0xE4C));     // Image_dtor(&F[0xe4c])          0x004213fd
        goto menu_top;               //                               0x00421402 jmp 0x00420e35
    }

    if (ebx == 3) {                  //                               0x00421407 jne 0x00421443
        // championship flags (0x0042140c..0x00421439)
        int eax = G32(0x59E7B8);             // 0x0042140c
        GU32(0x571FD4) = 0x186A0;            // g_cash_div100 = 100000  0x00421411
        eax |= 0x1000000;                    // 0x0042141b
        GU32(0x5A6364) = 0x11;               // 0x00421420
        GU32(0x59E7B8) = eax;                // 0x0042142a
        GU32(0x543CFC) = 0x3FFF;             // 0x0042142f
        GU32(0x822C88) = 1;                  // 0x00421439
    }

L421443:  // 0x00421443 (also the theme-preview return at 0x004215f2)
    if (ebx == 0 || ebx == -1 || ebx == 2) {   // 0x00421443/0x00421447/0x0042144a
        // difficulty prompt text + selectDifficultyScreen (0x00421455..0x00421522)
        StrCpy(kBuf, 0x4C5998);     // 0x00421478
        StrCat(kBuf, 0x4C598C);     // 0x004214a3
        StrCat(kBuf, 0x4C597C);     // 0x004214ce
        StrCat(kBuf, 0x4C596C);     // 0x004214f9
        int eax = CC(0x43A400);     // selectDifficultyScreen()        0x004214fb (no pushed args)
        GU32(0x822C88) = eax;       //                                 0x00421503
        if (eax == -1) goto menu_top;  //                             0x00421508 jmp 0x00420e35
        GU32(0x822C78) = Rnd(0xFFFF) & 0xFFFF;  // Random::range(0xffff)  0x00421518/0x00421522
    }

    // 0x00421527
    // loadImage(this=0x568d10, 0x4c5950, 0, 0, 0x100, 2)            0x0042153c
    TC(0x475840, 0x568D10, 0x4C5950, 0, 0, 0x100, 2);
    if (ebx == 2) {                  //                               0x00421541 jne 0x0042155e
        int eax = G32(0x59E7B8);             // 0x00421546
        GU8(0x5A34E0) = 0xFF;                // 0x0042154b
        eax |= 0x4000000;                    // 0x00421552
        GU32(0x59E7B8) = eax;                // 0x00421557
        // falls to 0x00421567                                        0x0042155c jmp
    } else if (ebx == 1) {           //                               0x0042155e cmp; 0x00421561 je
        goto L4215f7;                //                               0x004215f7
    }

    // 0x00421567
    if (G32(0x822C88) == 5) {        //                               0x00421567 jne 0x00421601
        // another theme-pack preview, Image in F+0xb94 (0x00421574..0x004215ed)
        CC(0x4725B0);                // showThemePackSelect()          0x00421574 (no pushed args)
        TC(0x474AE0, FA(0xB94));     // Image_ctor(&F[0xb94])          0x00421580
        FB(0x124C) = 2;              //                               0x0042159f
        // loadImage(this=&F[0xb94], 0x4c59b8, 0x5199c8, 0, 0x100, 0) 0x004215a7
        TC(0x475840, FA(0xB94), 0x4C59B8, 0x5199C8, 0, 0x100, 0);
        MV_blitFull(FA(0xB94));                                    // 0x004215d9
        FB(0x124C) = 0;              //                               0x004215e5
        TC(0x474C40, FA(0xB94));     // Image_dtor(&F[0xb94])          0x004215ed
        goto L421443;                //                               0x004215f2 jmp 0x00421443
    }
    goto L421601;                    //                               0x0042156e jne 0x00421601

L4215f7:  // 0x004215f7
    GU32(0x822C88) = 6;              //                               0x004215f7 (falls to 0x00421601)

L421601:  // 0x00421601
    CC(0x406250, -1);                // showLoadingScreen(-1)          0x00421603
    CC(0x46F2B0);                    // seedCourseTable()              0x0042160b
    return ebx;                      // jmp 0x0040f9fc with ebx        0x00421610
}

}  // namespace
// c3fy part SV: mainLoop 0x0040f5c0 — the session entry 0x0040f9fc..0x0040fc24 (Session) and the frame preamble
// 0x0040fc24..0x00410604 (FramePreamble, falls into P1 at 0x00410604).
// Hand transcription of log/c3/alt/c3fy_norm.asm (and c3fy_fold.asm for the inline strcpy/strcat idioms, every folded
// line re-checked against c3fy_norm.asm). Every [F+X] of the original stays a frame access at the same offset/width;
// every callee is called at its original address (CC cdecl, TC thiscall) in the original's order.
//
// Callee argument counts corrected from the disassembly / existing C4 reimplementations (c3fy_sigs.tsv is wrong for
// these):  pumpLight 0x45c150 = 0 args (ends `ret`, reads nothing); showCreditsScreen 0x44b9c0 = 0 args (c3en.cpp
// ShowCreditsScreen_re is __cdecl()); resetSpriteQueue 0x4627d0 = 0 args, cdecl (c3fu.cpp, reads no register first);
// customizeCharacter 0x40f190 = 1 arg cdecl (caller `add esp,4`); 0x467130 = 3 args cdecl (reads [esp+4/8/c]);
// 0x4845e0 = thiscall, no stack args.
namespace {

enum SessAct { sQuit, sMenu, sFrame };   // Session's three outcomes (the verifier maps them to the frame control flow)

// timeGetTime through the IAT slot the original calls (never the import directly; the scenario harness redirects it)
static inline u32 SV_timeGetTime() { return reinterpret_cast<DWORD(WINAPI*)()>(GU32(0x004BA21C))(); }

// ====================================================================================================================
// Session 0x0040f9fc..0x0040fc24 : per-session setup after a title-menu selection (sel = ebx from the menu).
// Returns sQuit (original returns from mainLoop), sMenu (jmp 0x00420e35) or sFrame (reaches 0x0040fc24 -> FramePreamble).
// ====================================================================================================================
int Session(u8* F, int sel) {
    int mode = G32(0x00822C88);                     // 0x0040f9fc
    CC(0x004658B0, (mode != 6) ? 1 : 0);            // 0x0040fa0e loadThemes(mode!=6?1:0)  (setne dl)
    CC(0x00448200, 0x80);                           // 0x0040fa18 startSoundSlot(0x80)

    if (sel == -2) {                                // 0x0040fa20 cmp ebx,-2 : the QUIT path 0x0040fa29..0x0040fac8
        CC(0x00448200, 0x2D);                       // 0x0040fa2b startSoundSlot(0x2d)
        CC(0x0045BF80, 0x3C, 0);                    // 0x0040fa34 waitTicks(0x3c, 0)
        StrCpy(kBuf, 0x004C52AC);                   // 0x0040fa3e..0x0040fa6b copy top-scores file name into kBuf
        int fd = CC(0x004A5D48, (int)kBuf, 0x8301, 0x80);   // 0x0040fa6d __open(name, 0x8301, 0x80)
        G32(0x00568D08) = fd;                        // 0x0040fa7d
        CC(0x004A5B58, fd, 0x00541CE0, 0x618);      // 0x0040fa82 __write(fd, 0x541ce0, 0x618) (the 0x618-byte top10 block)
        CC(0x004A5A78, G32(0x00568D08));            // 0x0040fa8e __close(fd)
        CC(0x00437910, 0, sel, 0);                   // 0x0040fa98 promptAndSaveFile(0, sel, 0)
        CC(0x0043CCE0, 0x004C6BB4, 1);              // 0x0040faa4 playAudioFile(SMSG_closingfinal.bik, 1)
        CC(0x0045C150);                             // 0x0040faa9 pumpLight()
        CC(0x004481B0, 0x81, 0x64, 0, 0, 0);        // 0x0040fabb playSound(0x81,0x64,0,0,0)
        CC(0x0044B9C0);                             // 0x0040fac3 showCreditsScreen()
        CC(0x00448200, 0x81);                       // 0x0040facd startSoundSlot(0x81)
        FD(0x124C) = -1;                             // 0x0040fadc SEH try-level before the dtor
        TC(0x00483010, FA(0x2B0));                  // 0x0040fae7 Cache483010::dtor(F+0x2b0) (lea ecx,[F+0x2b0])
        return sQuit;                               // 0x0040faec.. the mainLoop epilogue (verifier-owned)
    }

    int flags = G32(0x0059E7B8);                    // 0x0040fb05
    G32(0x00567AFC) = 5;                             // 0x0040fb0a
    G32(0x00822D8C) = -1;                            // 0x0040fb19
    if (flags & 0x4000000) {                        // 0x0040fb14 test eax,0x4000000; 0x0040fb23 je (NOT sandbox)
        if (CC(0x0046DDD0) == 0) {                  // 0x0040fb25 startNewCourse(); 0x0040fb2c jne skips this
            G32(0x0059E7B8) &= 0xFBFFFFFF;          // 0x0040fb2e clear the sandbox bit
            return sMenu;                           // 0x0040fb38 jmp 0x00420e35
        }
        // startNewCourse != 0 -> fall to the loadWorldAssets block (0x0040fb63)
    } else if (G32(0x00822C88) == 6) {              // 0x0040fb3d cmp [0x822c88],esi(6)
        if (CC(0x0043B610, 1, 0) == 0)              // 0x0040fb49 loadGameBrowserScreen(1,0); 0x0040fb53 je
            return sMenu;                           // jmp 0x00420e35
        CC(0x004658B0, 1);                          // 0x0040fb5b loadThemes(1)
        // fall to the loadWorldAssets block (0x0040fb63)
    } else {                                        // 0x0040fb77 the normal new-course path
        G32(0x00543CF4) = 0;                        // 0x0040fb89
        G32(0x0059BF90) = 0;                        // 0x0040fb94
        G32(0x00571FD4) = (flags & 0x1000000) ? 0x2710 : 0x3E8;  // 0x0040fb77..0x0040fb9a (neg/sbb/and 0x2328 + 0x3e8)
        int r = CC(0x00470A00, 1);                  // 0x0040fb9f courseActionMenu(1) (thunk -> 0x0046f550)
        G32(0x0059BF90) = r;                        // 0x0040fbaa
        if (r == -1)                                // 0x0040fbaf je 0x00420e35
            return sMenu;
        GU8(0x005A34E0) = GU8(0x00571FF8 + r * 46); // 0x0040fbb5..0x0040fbc4 byte[0x571ff8 + result*46] (edx=r*23, *2)
        CC(0x0043DBE0);                             // 0x0040fbc9 loadWorldAssets()
        CC(0x00470A60, G32(0x0059BF90));            // 0x0040fbd5 generateRandomCourse(result)
        CC(0x0042F7A0);                             // 0x0040fbdd rebuildHeightfield()
        int f2 = G32(0x0059E7B8);                   // 0x0040fbe2
        G32(0x005A6364) = 6;                        // 0x0040fbe7
        if (f2 & 0x1000000) {                       // 0x0040fbf1 test; 0x0040fbf6 je 0x0040fc16
            G32(0x00571FD4) = 0x2710;               // 0x0040fbf8
            G32(0x005A6364) = 0x11;                 // 0x0040fc02
            G32(0x00543CFC) = 0x3FFF;               // 0x0040fc0c
        }
        GU16(0x0058421E) = GU16(0x00571FD4);        // 0x0040fc16..0x0040fc1d (mov dx,[0x571fd4]; mov [0x58421e],dx)
        return sFrame;                              // 0x0040fc24 falls into FramePreamble
    }
    // loadWorldAssets block 0x0040fb63 (reached by the sandbox-nonzero and mode==6 fall-throughs)
    CC(0x0043DBE0);                                 // 0x0040fb63 loadWorldAssets()
    G32(0x00542DF8) = 1;                            // 0x0040fb68
    return sFrame;                                  // 0x0040fb72 jmp 0x0040fc24
}

// ====================================================================================================================
// FramePreamble helpers
// ====================================================================================================================

// 0x0040fd1c..0x0040fe4b : scan the 0x98 (152) golfer records in a rotating order from [0x5a5a24], count active ones
// (ebx), maybe clear the "drag" bit of [0x59e7b8], and when the gates pass flag a golfer pair and start their story.
// Returns ebx (the count read at 0x0040fe4e).
static int SV_golferScan(u8* F) {
    s8 dl_b = G8(0x00575CB8);                       // 0x0040fd1c
    int sel567 = G32(0x00567AFC);                   // 0x0040fd22
    int count = 0;                                  // 0x0040fd27 ebx
    FD(0x28) = (dl_b != 0) ? 1 : 0;                 // 0x0040fd29..0x0040fd36 (setne cl)
    FD(0xC) = 0;                                    // 0x0040fd3a
    int edi = -1;                                   // 0x0040fd30
    bool setPath = false;
    if (sel567 == 1 && G32(0x0055E928) != 0) {      // 0x0040fd33 cmp eax,1; 0x0040fd40 cmp [0x55e928],0
        int f2 = G32(0x0059E7B8);                   // 0x0040fd48
        if (!(f2 & 0x8000000)) {                    // 0x0040fd4d test eax,0x8000000; 0x0040fd52 jne
            G32(0x004C2854) = -1;                   // 0x0040fd56
            G32(0x0059E7B8) = f2 | 2;               // 0x0040fd54 or al,2; 0x0040fd5c store
            G32(0x004C2848) = -1;                   // 0x0040fd61
            setPath = true;                         // 0x0040fd67 jmp 0x0040fd75
        }
    }
    if (!setPath)                                   // 0x0040fd69
        G32(0x0059E7B8) &= 0xFFFFFFFD;              // 0x0040fd6e and al,0xfd (clear bit 1); 0x0040fd70 store

    // 0x0040fd75 scan loop (ebp = 0x98 down to 1; esi decremented; rem = esi mod 0x98 is the golfer index)
    int esi = G32(0x005A5A24) + 0x98;               // 0x0040fd75/0x0040fd80
    for (int ebp = 0x98; ebp != 0; --ebp, --esi) {  // 0x0040fd86..0x0040fde2
        int rem = esi % 0x98;                       // 0x0040fd86..0x0040fd90 signed idiv by 0x98, remainder
        int recbase = rem << 8;                     // 0x0040fd92 golfer record = 0x5794d0 + rem*0x100
        u8 clb = GU8(recbase + 0x005794D9);         // 0x0040fd95 mov cl, byte[rem<<8 + 0x5794d9]
        if (clb == 0xFF) {                          // 0x0040fd9b cmp cl,0xff; 0x0040fd9e jne
            count++;                                // 0x0040fda0
            FD(0x18) = count;                       // 0x0040fda1
            int tmp = (rem ^ 1) << 8;               // 0x0040fda5/0x0040fda7/0x0040fdaa
            if (GU8(tmp + 0x005794D9) == 0xFF)      // 0x0040fdad cmp byte[(rem^1)<<8 + 0x5794d9], cl(0xff)
                edi = rem;                          // 0x0040fdb5
            count = FD(0x18);                       // 0x0040fdb7
        }
        if (clb == 1) {                             // 0x0040fdbb cmp cl,1; 0x0040fdbe jne
            FD(0xC) = FD(0xC) + 1;                  // 0x0040fdc0/0x0040fdc4/0x0040fdc7
            // 0x0040fdc5 cmp cl,cl is always equal, so the jne at 0x0040fdcb is never taken: always continue
            u8 clb2 = GU8(recbase + 0x005794DA);    // 0x0040fdcd mov cl, byte[rem<<8 + 0x5794da]
            if (clb2 == 0) {                        // 0x0040fdd3 test cl,cl; 0x0040fdd5 jne
                FD(0x28) = 0;                       // 0x0040fdd7
                count++;                            // 0x0040fddf
            }
        }
    }

    // 0x0040fde4 : examine the partner slot edi^1 and, if the gates pass, pair edi with edi^1 and start their story
    int partner = edi ^ 1;                          // 0x0040fde4/0x0040fde6
    int precbase = partner << 8;                    // 0x0040fde9/0x0040fdeb (ecx = precbase + 0x5794d9)
    s8 dl9 = G8(precbase + 0x005794D9);             // 0x0040fdf4 mov dl, byte[(edi^1)<<8 + 0x5794d9]
    int ebpv;                                       // 0x0040fdf4..0x0040fe07
    if ((u8)dl9 == 0xFF)                            // 0x0040fdfa cmp dl,0xff; 0x0040fdfd je
        ebpv = FD(0x28);                            // 0x0040fe03
    else
        ebpv = 0;                                   // 0x0040fdff
    int esiv;                                       // 0x0040fe07..0x0040fe1b
    if (G32(0x0059E7B8) & 0x100000)                 // 0x0040fe07 test [0x59e7b8],0x100000; 0x0040fe11 jne
        esiv = FD(0xC);                             // 0x0040fe17
    else
        esiv = 0;                                   // 0x0040fe13
    if (!(GU8(0x0059E7BA) & 0x20)                   // 0x0040fe1b test byte[0x59e7ba],0x20; 0x0040fe22 jne -> skip
        && esiv == 0                                // 0x0040fe24 test esi; 0x0040fe26 jne -> skip
        && ebpv != 0                                // 0x0040fe28 test ebp; 0x0040fe2a je -> skip
        && edi != -1                                // 0x0040fe2c cmp edi,-1; 0x0040fe2f je -> skip
        && (s8)dl9 <= 0) {                          // 0x0040fe31 test dl,dl; 0x0040fe33 jg -> skip (signed)
        G8((edi << 8) + 0x005794D9) = 1;            // 0x0040fe3c mov byte[edi<<8 + 0x5794d9],1
        G8(precbase + 0x005794D9) = 1;              // 0x0040fe43 mov byte[ecx],1
        CC(0x0045DE80, edi, partner);               // 0x0040fe46 pairGolfersWithStory(edi, edi^1)
    }
    return count;                                   // ebx at 0x0040fe4e
}

// 0x0040fe4e..0x00410394 : advance the game date / membership and, when the gates pass, pick a magazine issue, build
// its headline text in kBuf and post it as a message. Every "jmp 0x00410399" in the original becomes `return` here;
// the caller (FramePreamble) continues at 0x00410399. `count` is ebx from SV_golferScan.
static void SV_dateStory(u8* F, int count) {
    int eax;                                        // 0x0040fe4e..0x0040fe6a : "should the membership tick happen"
    if ((GU8(0x00834170) & 0x7F) && count >= 2)     // 0x0040fe4e test byte[date],0x7f; 0x0040fe57 cmp ebx,2 (signed)
        eax = 0;                                    // 0x0040fe5c
    else
        eax = 1;                                    // 0x0040fe60
    if (count >= 8)                                 // 0x0040fe65 cmp ebx,8; 0x0040fe68 jl
        eax = 0;                                    // 0x0040fe6a
    if (G32(0x0059E7B8) & 0x200000) return;         // 0x0040fe6c test [0x59e7b8],0x200000; 0x0040fe76 jne 0x00410399
    if (eax == 0) return;                           // 0x0040fe7c test eax; 0x0040fe7e je 0x00410399

    int um = CC(0x00421BC0, 1);                     // 0x0040fe86 updateMembership(1)
    if (um == -1) return;                           // 0x0040fe8e cmp eax,-1; 0x0040fe91 je 0x00410399
    int esi5 = um << 8;                             // 0x0040fe97/0x0040fe9b golfer[um] record stride 0x100
    G32(0x005A5A24) = um;                           // 0x0040fea3
    GU16(esi5 + 0x0057955A) = (u16)um;              // 0x0040fea8 mov word[um<<8 + 0x57955a], ax
    int rnd6 = Rnd(6);                              // 0x0040feaf Random::range(6)
    int ecxv = -6 - rnd6;                           // 0x0040feb4/0x0040feb9 (0xfffffffa - rnd6)
    int e14 = G32(0x004C2E14);                      // 0x0040febb
    GU16(esi5 + 0x00579556) = (u16)ecxv;            // 0x0040fec0 mov word[um<<8 + 0x579556], cx
    int ecx0 = (e14 == -1) ? 1 : 0;                 // 0x0040fec9 cmp [0x4c2e14],-1; 0x0040fed1 sete cl
    for (u32 p = 0x005794D0; p < 0x00582CD0; p += 0x100) {   // 0x0040fed4..0x0040fef1 (golfer records)
        if (G8(p + 9) != 0 && (GU8(p) & 0xE0) == 0x20)       // 0x0040fed4/0x0040fed7/0x0040fedb/0x0040fedd/0x0040fee0
            ecx0 = 0;                               // 0x0040fee5 xor ecx,ecx
    }
    int ebxv;                                       // ebx = 0x2000 (the date-cap constant) on both branches below
    if (GU8(0x0056C7B4) & 0x40) {                   // 0x0040fef3 test byte[0x56c7b4],0x40; 0x0040fefa je 0x0040ff27
        int date = G32(0x00834170);                 // 0x0040fefc
        G8(esi5 + 0x005794D1) = 7;                  // 0x0040ff08 mov byte[um<<8 + 0x5794d1], 7
        G32(0x00541CD8) = 0x1F4;                    // 0x0040ff0f
        if (date <= 0x2000)                         // 0x0040ff19 jg 0x0040ff34 (signed)
            G32(0x00834170) = 0x2001;               // 0x0040ff1b
        ebxv = 0x2000;                              // 0x0040ff01 mov ebx,0x2000
    } else {
        if (ecx0 == 0) return;                      // 0x0040ff27 test ecx; 0x0040ff29 je 0x00410399
        ebxv = 0x2000;                              // 0x0040ff2f mov ebx,0x2000
    }
    // 0x0040ff34
    if (G8(esi5 + 0x005794D1) != 7) return;         // 0x0040ff34 cmp byte,7; 0x0040ff3b jne 0x00410399
    if (G32(0x00834170) <= ebxv) return;            // 0x0040ff41 cmp [date],ebx; 0x0040ff47 jle 0x00410399 (signed)
    int v59 = G32(0x0059B730);                      // 0x0040ff4d
    if (GU8((v59 * 65) * 8 + 0x005760C8) == 0)      // 0x0040ff4d..0x0040ff60 byte[v59*65*8 + 0x5760c8]
        return;                                     // 0x0040ff62 je 0x00410399

    // magazine pick loop 0x0040ff68..0x0040ffd3 : retry a random issue until its weighted score sits in the window
    int attempts = 0;                               // 0x0040ff68 xor edi,edi
    for (;;) {
        int r = Rnd(0x64);                          // 0x0040ff71 Random::range(100)
        int rv = r & 0xFFFF;                        // 0x0040ff76/0x0040ff7b
        int acc = -0x14;                            // 0x0040ff81 esi = -20
        FD(0x18) = rv;                              // 0x0040ff86
        int base = (rv * 7) * 8;                    // 0x0040ff8a/0x0040ff91/0x0040ff93 (rv*56 record stride)
        for (int k = 0; k < 0xC; k++)               // 0x0040ff96..0x0040ffa5 sum 12 weight bytes
            acc += GU8(base + k + 0x0058DD75);
        u8 kind = GU8(base + 0x0058DD70);           // 0x0040ffa7 mov cl, byte[base + 0x58dd70]
        attempts++;                                 // 0x0040ffad inc edi
        if (kind == 0xFF) continue;                 // 0x0040ffae je 0x0040ff6a (empty slot, retry)
        int ebp2 = G32(0x0059B730);                 // 0x0040ffb3
        int ecx5 = ebp2 * 5;                        // 0x0040ffbf lea ecx,[ebp+ebp*4]
        int ediq = attempts / 4;                    // 0x0040ffb9..0x0040ffc5 edi/4 (signed)
        if (acc > ediq + ecx5) continue;            // 0x0040ffc8/0x0040ffcb cmp esi,edx; jg 0x0040ff6a
        if (acc < ecx5 - ediq) continue;            // 0x0040ffcf/0x0040ffd1 cmp esi,ecx; jl 0x0040ff6a
        break;                                      // falls to 0x0040ffd5
    }
    // 0x0040ffd5
    if (G32(0x00834170) <= ebxv) return;            // 0x0040ffd5 cmp [date],ebx(0x2000); jle 0x00410399 (signed)
    int base2 = (FD(0x18) * 7) * 8;                 // 0x0040ffe1..0x0040ffee the chosen issue's record offset (rv*56)
    if (GU8(base2 + 0x0058DD70) == 0xFF) return;    // 0x0040fff1 cmp byte,0xff; 0x0040fff8 je 0x00410399
    if (G32(0x004C2E14) != -1) return;              // 0x0040fffe cmp [0x4c2e14],-1; 0x00410005 jne 0x00410399

    // 0x0041000b..0x00410347 build the headline text in kBuf (0x51a068)
    int mag = G32(0x0059B730);                      // ebp, used by the first __itoa value
    StrCpy(kBuf, 0x004C6BA4);                       // 0x0041000b
    StrCat(kBuf, (u32)(base2 + 0x0058DD50));        // 0x00410039 the issue's name string
    StrCat(kBuf, 0x004C6B68);                       // 0x00410065
    int v1 = (((mag * 5 + 5) * 5) * 5) << 4;        // 0x00410089/0x00410092/0x00410097/0x0041009a
    StrCat(kBuf, (u32)Itoa(v1, 0x0058A528, 0xA));   // 0x0041009e __itoa(v1, 0x58a528, 10)
    StrCat(kBuf, 0x004C6B58);                       // 0x004100cd
    int v2 = (((G32(0x0059B730) * 5 + 5) * 5) * 5) << 5;   // 0x004100fd/0x00410106/0x0041010b/0x0041010e
    StrCat(kBuf, (u32)Itoa(v2, 0x0058A528, 0xA));   // 0x00410112 __itoa(v2, 0x58a528, 10)
    StrCat(kBuf, 0x004C6B44);                       // 0x0041014a
    FD(0xC) = 0;                                     // 0x00410177
    int mx = 0;                                     // edx = running max of 12 bytes at base2+0x58dd75
    for (int k = 0; k < 0xC; k++) {                 // 0x00410182..0x00410192
        int v = GU8(base2 + 0x0058DD75 + k);        // 0x00410184 (zero-extended)
        if (v > mx) { FD(0xC) = v; mx = v; }        // 0x00410186/0x0041018a/0x0041018e
    }
    if (mx != 0) {                                  // 0x00410194 test edx; je 0x004102be
        u8 flag70 = GU8(base2 + 0x0058DD70);        // 0x0041019c
        u32 src = (flag70 & 4) ? 0x004C6B2C : 0x004C6B18;   // 0x004101a2/0x004101a7/0x004101ab
        StrCat(kBuf, src);                          // 0x004101b0
        StrCat(kBuf, (u32)Itoa(mx, 0x0058A528, 0xA));       // 0x004101e0 __itoa(mx, 0x58a528, 10) (edx = mx)
        StrCat(kBuf, 0x004C6AFC);                   // 0x00410212
        int eax_flag = 1;                           // 0x00410238 mov eax,1
        u32 edx_tab = 0x004C2C3C;                   // 0x00410240
        int ebp_ptr = base2 + 0x0058DD75;           // ebp pointer over the 12 weight bytes
        do {                                        // 0x00410247 loop
            int esi_mx = FD(0xC);                   // 0x00410247
            u8 cl_b = GU8(ebp_ptr);                 // 0x0041024d mov cl, byte[ebp]
            if ((int)cl_b == esi_mx) {              // 0x00410250 cmp ecx,esi; jne 0x004102b2
                u32 src2 = (eax_flag != 0) ? 0x004C3F70 : 0x004C6AF4;   // 0x00410254/0x00410256/0x0041025b/0x0041025d
                eax_flag = 0;                       // 0x00410265 xor eax,eax (inside the first strcat)
                StrCat(kBuf, src2);                 // 0x00410262
                StrCat(kBuf, GU32(edx_tab));        // 0x0041028a strcat(kBuf, dword[edx])
            }
            edx_tab += 4;                           // 0x004102b2
            ebp_ptr += 1;                           // 0x004102b5
        } while (edx_tab < 0x004C2C6C);             // 0x004102b6 cmp; jl 0x00410247
    }
    // 0x004102be
    StrCat(kBuf, 0x004C6AD0);                       // 0x004102be
    StrCat(kBuf, 0x004D6098);                       // 0x004102f3
    StrCat(kBuf, 0x004C6AA0);                       // 0x0041031e
    int sm = CC(0x0040CB00, 0x80002108, 0, -0x14);  // 0x00410349 startMessage(0x80002108, 0, -0x14)
    if (sm == 0) return;                            // 0x00410351 test eax; je 0x00410399
    G32(0x0059E7B8) |= 0x2000;                      // 0x00410355 or ch,0x20; 0x00410368 store (before playSound)
    CC(0x004481B0, 0x26, 0x64, 0, 0, 0);            // 0x0041036e playSound(0x26,0x64,0,0,0)
    G32(0x004C2E14) = FD(0x18);                     // 0x0041037d store the chosen issue index
    G32(0x0056C7B4) &= 0xFFFFFFBF;                  // 0x00410377/0x0041038a/0x0041038e clear bit 0x40
    G32(0x0059B730) = G32(0x0059B730) + 1;          // 0x00410382/0x0041038d/0x00410394 ++issue counter
    // return -> 0x00410399
}

// ====================================================================================================================
// FramePreamble 0x0040fc24..0x00410604 : start of every frame. If retile != 0, rebuildTileCoverage first (0x0040fc24),
// then stamp the frame time, rebuild the per-type amenity clamps, run the golfer/story/membership/magazine pass,
// fill the label-pointer table, refresh the terrain render, detect a camera move and set up the tile stride.
// Falls through to P1 (Ph_00410604); no register is live there beyond the frame slots P1 documents.
// ====================================================================================================================
void FramePreamble(u8* F, int retile) {
    if (retile)
        CC(0x0042F340);                             // 0x0040fc24 rebuildTileCoverage() (only on the 0x0040fc24 entry)
    FU(0x1EC) = SV_timeGetTime();                   // 0x0040fc29/0x0040fc2f timeGetTime -> [F+0x1ec]

    // 0x0040fc36..0x0040fc64 : clear the two 0x14-dword arrays, reset [0x5a6370], re-cap [0x571fd4] in sandbox
    for (int i = 0; i < 0x14; i++) G32(0x005A8C38 + 4 * i) = 0;   // 0x0040fc36..0x0040fc42
    for (int i = 0; i < 0x14; i++) G32(0x00543CA0 + 4 * i) = 0;   // 0x0040fc44..0x0040fc4e
    int fl1 = G32(0x0059E7B8);                      // 0x0040fc50
    G32(0x005A6370) = 0;                            // 0x0040fc5c (ebx = 0)
    if (fl1 & 0x1000000)                            // 0x0040fc57 test; 0x0040fc62 je 0x0040fc6e
        G32(0x00571FD4) = 0x2710;                   // 0x0040fc64

    // 0x0040fc6e..0x0040fce7 : for each placed object (16-byte records) of type >= 6 (and != 5), clamp its two
    // per-type accumulators [0x5a8c38 + type*4] and (if byte+7 & 0x40) [0x543ca0 + type*4] to <= 0x63 and OR type into
    // the mask [0x5a6370].  0x467130 = clamp(value, count+1, 0x63) (cdecl, 3 args).
    for (u32 rec = 0x0058BCB8; rec < 0x0058CCB8; rec += 0x10) {   // 0x0040fc6e..0x0040fce7
        short ty = G16(rec);                        // 0x0040fc73 (signed word)
        if (ty >= 6 && ty != 5) {                   // 0x0040fc76 jl; 0x0040fc7c je (ty>=6 makes !=5 always true)
            G32(0x005A8C38 + ty * 4) =              // 0x0040fc9f store
                CC(0x00467130, G32(0x005A8C38 + ty * 4), G32(rec + 8) + 1, 0x63);   // 0x0040fc82..0x0040fc9a
            if (GU8(rec + 7) & 0x40) {              // 0x0040fca5 mov al,byte[rec+7]; 0x0040fcab test; 0x0040fcad je
                G32(0x00543CA0 + ty * 4) =          // 0x0040fcc5 store
                    CC(0x00467130, G32(0x00543CA0 + ty * 4), G32(rec + 8) + 1, 0x63);   // 0x0040fcaf..0x0040fcc0
                G32(0x005A6370) |= (1 << (ty & 0x1F));   // 0x0040fccc..0x0040fcd8 (shl masks cl to 5 bits)
            }
        }
    }

    // 0x0040fce9..0x0040fd19 : reset a couple of globals and, under bit 0x4000 of [0x59e7b8], customizeCharacter
    if (G32(0x005A8C54) != 0) {                     // 0x0040fce9 cmp; 0x0040fcef je 0x0040fcfd
        G32(0x00543CBC) = 0;                        // 0x0040fcf1
        G32(0x005A8C54) = 0;                        // 0x0040fcf7
    }
    int fl2 = G32(0x0059E7B8);                      // 0x0040fcfd
    G32(0x005A8C78) = 0;                            // 0x0040fd02
    if (fl2 & 0x4000)                               // 0x0040fd08 test ah,0x40 (bit 0x4000); 0x0040fd0b je 0x0040fd1c
        CC(0x0040F190, (fl2 >> 13) & 1);            // 0x0040fd14 customizeCharacter((flags>>13)&1)

    int count = SV_golferScan(F);                   // 0x0040fd1c..0x0040fe4b
    SV_dateStory(F, count);                         // 0x0040fe4e..0x00410394 (every early exit lands here, at 0x00410399)

    // 0x00410399..0x004104b6 : fill the 24-entry label-pointer table in the frame, and (unless bit 1 of [0x5619a0])
    // clear the 0x4e2-dword array at 0x55eb40
    u8 lab = GU8(0x005619A0);                       // 0x00410399
    FD(0x238) = 0x004C6A98;                         // 0x0041039e
    FD(0x23C) = 0x004C6A90;                         // 0x004103ab
    FD(0x240) = 0x004C6A84;                         // 0x004103b6
    FD(0x244) = 0x004C6A7C;                         // 0x004103c1
    FD(0x248) = 0x004C6A74;                         // 0x004103cc
    FD(0x24C) = 0x004C6A6C;                         // 0x004103d7
    FD(0x250) = 0x004C6A64;                         // 0x004103e2
    FD(0x254) = 0x004C6A5C;                         // 0x004103ed
    FD(0x258) = 0x004C6A54;                         // 0x004103f8
    FD(0x25C) = 0x004C6A4C;                         // 0x00410403
    FD(0x260) = 0x004C6A44;                         // 0x0041040e
    FD(0x264) = 0x004C6A38;                         // 0x00410419
    FD(0x268) = 0x004C6A30;                         // 0x00410424
    FD(0x26C) = 0x004C6A28;                         // 0x0041042f
    FD(0x270) = 0x004C6A20;                         // 0x0041043a
    FD(0x274) = 0x004C6A18;                         // 0x00410445
    FD(0x218) = 0x004C6A10;                         // 0x00410450
    FD(0x21C) = 0x004C6A08;                         // 0x0041045b
    FD(0x220) = 0x004C6A00;                         // 0x00410466
    FD(0x224) = 0x004C69F8;                         // 0x00410471
    FD(0x228) = 0x004C69F0;                         // 0x0041047c
    FD(0x22C) = 0x004C69E8;                         // 0x00410487
    FD(0x230) = 0x004C69E0;                         // 0x00410492
    FD(0x234) = 0x004C69D8;                         // 0x0041049d
    if (!(lab & 2))                                 // 0x004103a9 test al,2; 0x004104a8 jne 0x004104b8
        for (int i = 0; i < 0x4E2; i++) G32(0x0055EB40 + 4 * i) = 0;   // 0x004104aa..0x004104b6

    // 0x004104b8..0x0041054b : refresh the terrain render; when the date is set and [0x542df8] != 0 also re-apply the
    // course type, refresh again and play the "course changed" sound
    CC(0x004498A0, G32(0x004C2BA0), G32(0x004C2BA4), G32(0x004C2844), G32(0x005685F4));   // 0x004104d3 updateTerrainRender
    G32(0x004C2E04) = -1;                           // 0x004104e2
    bool doCourse = true;                           // 0x004104e0/0x004104ec/0x004104f5
    if (G32(0x00834170) != 0 && G32(0x00542DF8) == 0)   // 0x004104d8 test; 0x004104ee test; 0x004104f5 je 0x0041054e
        doCourse = false;
    if (doCourse) {
        TC(0x004845E0, 0x0080EB3C);                 // 0x004104fc (ecx = 0x80eb3c)
        CC(0x00449400, (int)G8(0x005A34E0));        // 0x00410509 applyCourseType(movsx byte[0x5a34e0])
        CC(0x00449520, (int)G8(0x005A34E0));        // 0x0041050e refreshTerrain (reuses the same pushed arg)
        CC(0x004498A0, G32(0x004C2BA0), G32(0x004C2BA4), G32(0x004C2844), G32(0x005685F4));   // 0x0041052d updateTerrainRender
        G32(0x00542DF8) = 0;                        // 0x0041053c (before the sound)
        CC(0x004481B0, 0x2D, 0x64, 0, 0, 0);        // 0x00410546 playSound(0x2d,0x64,0,0,0)
    }

    // 0x0041054e..0x004105cf : if the camera / selection changed, clear the coverage array and the 0x98 selection
    // slots; then store the current camera as "last" and reset the sprite queue
    int camX = G32(0x004C2BA0);                     // 0x0041054e (edx, reused at the stores below)
    bool unchanged =
        camX == G32(0x00543D04) &&                  // 0x00410554/0x00410559
        G32(0x004C2BA4) == G32(0x00543D00) &&       // 0x0041055d/0x00410563/0x0041056a
        G32(0x00567B00) == G32(0x004C2844) &&       // 0x0041056c/0x00410571/0x00410579
        G32(0x005A59FC) == G32(0x005685F4);         // 0x0041057b/0x00410581/0x00410588
    if (!unchanged) {                               // 0x0041058a
        for (int i = 0; i < 0x4E2; i++) G32(0x0055EB40 + 4 * i) = 0;    // 0x0041058a..0x00410596
        for (int i = 0; i < 0x98; i++) G32(0x00567B34 + 4 * i) = -1;    // 0x00410598..0x004105a5
    }
    // 0x004105a7
    int curZ = G32(0x004C2844);                     // 0x004105a7
    int curW = G32(0x005685F4);                     // 0x004105ac
    G32(0x00543D04) = camX;                         // 0x004105b2
    G32(0x00543D00) = G32(0x004C2BA4);              // 0x004105b8/0x004105be
    G32(0x00567B00) = curZ;                         // 0x004105c4
    G32(0x005A59FC) = curW;                         // 0x004105c9
    CC(0x004627D0);                                 // 0x004105cf resetSpriteQueue()

    // 0x004105d4..0x00410600 : tile stride setup for the frame; falls into P1 at 0x00410604
    int z = G32(0x004C2844);                        // 0x004105d4
    G32(0x0058B384) = z;                            // 0x004105d9
    G32(0x004C2840) = z * 8;                        // 0x004105de/0x004105eb (lea edx,[z*8])
    int e = (z * 5) << 2;                           // 0x004105e5/0x004105e8 (z*20)
    e = e / 4;                                      // 0x004105f1..0x004105f7 signed /4 -> z*5
    FD(0x40) = e;                                   // 0x004105fc
    FD(0x30) = 0;                                   // 0x00410600 (ebx = 0)
    // 0x00410604 falls through to Ph_00410604 (P1)
}

}  // namespace
// c3fy part P1: mainLoop 0x0040f5c0, range 0x00410604..0x0041234e (the first per-tile loop over the 50 x 50 grid).
// Hand transcription of log/c3/alt/c3fy_norm.asm. Every [F+X] access of the original is a frame access at the same
// offset and width. Registers are C locals named after the register they hold in the original.
// Entry: ebx = 0 (0x004105fa) is the outer counter, [F+0x30] = 0 (0x00410600), [F+0x40] written at 0x004105fc.
// Exit: falls through to 0x0041234e with no register live.

namespace {

// The two 8-dword tables indexed by a direction (0..7): 0x004c2878 holds the offset the code adds to ebp (the first
// tileToScreen argument), 0x004c2898 the offset it adds to ebx (the second one). A neighbour's tile index is
// t98(d) + (t78(d) + ebp) * 50 + ebx (lea chains such as 0x00410a5e..0x00410a77).
static inline int P1_t78(int d) { return G32(0x004C2878 + 4 * d); }
static inline int P1_t98(int d) { return G32(0x004C2898 + 4 * d); }
static inline int P1_nb(int d, int ebp, int ebx) { return P1_t98(d) + (P1_t78(d) + ebp) * 50 + ebx; }

// ---- 0x0041064e..0x004107be: grid overlay (flag 2 of 0x0059e7b8, 0x00542f20 != 3): two edges of the cell as
// rectangles on the Widget 0x00519cd4, and with flag 0x20 the cell's 0x00543018 byte as a number.
static void P1_grid(u8* F, int ebp, int ebx) {
    if ((GU8(0x0059E7B8) & 2) == 0) return;                                        // 0x00410655
    if (G32(0x00542F20) == 3) return;                                              // 0x00410662
    int esi = ebx << 10;                                                           // 0x0041067d
    int edi = ebp << 10;                                                           // 0x00410680
    if (CC(0x0042FB90, edi, esi, FA(0x13c), FA(0x11c), 0) == 0) return;            // 0x00410686 worldToScreen, 0x00410690
    CC(0x0042FB90, (ebp << 10) + 0x3ff, esi, FA(0x144), FA(0x134), 0);             // 0x004106b5 worldToScreen
    TC(0x00478B80, 0x00519CD4, FD(0x13c), FD(0x11c), FD(0x144), FD(0x134), (int)0x80000000);   // 0x004106e7 Widget_fillRect
    CC(0x0042FB90, edi, (ebx << 10) + 0x3ff, FA(0x144), FA(0x134), 0);             // 0x0041070a worldToScreen
    TC(0x00478B80, 0x00519CD4, FD(0x13c), FD(0x11c), FD(0x144), FD(0x134), (int)0x80000000);   // 0x0041073c Widget_fillRect
    if ((GU8(0x0059E7B8) & 0x20) == 0) return;                                     // 0x00410748
    int v = G8(0x00543018 + FD(0x14));                                             // 0x0041074e
    if (v == 0) return;                                                            // 0x00410756
    GU8(kBuf) = 0;                                                                 // 0x00410763
    int r = Itoa(v, 0x0058A528, 10);                                               // 0x0041076a __itoa
    StrCat(kBuf, (u32)r);                                                          // 0x0041076f..0x004107a6
    CC(0x00404AD0, (int)kBuf, FD(0x13c) + 0x10, FD(0x11c) - 3, (int)0x80007D08);   // 0x004107b9 drawTextSurface
}

// ---- 0x004109fb..0x00410bdb: type-0x11 tile, two side sprites (records at 0x00587e7c and 0x00587f00), each drawn
// when a neighbour corner height (cornerHeights + clamp 0..10) is above the tile corner returned at 0x00410a0f.
static void P1_t11_4109fb(u8* F, int ebp, int ebx) {
    int esi = 1;                                                                   // 0x00410a04
    int edi = CC(0x0040BFE0, ebp, ebx, (G32(0x005685F4) + 1) & 7, 0);             // 0x00410a0f cornerHeights
    int ecx = G32(0x005685F4);                                                     // 0x00410a14
    int eax = (ecx + 2) & 7;
    ecx = (ecx - 1) & 7;
    int r = CC(0x0040BFE0, P1_t78(eax) + ebp, P1_t98(eax) + ebx, ecx, 0);         // 0x00410a44 cornerHeights
    eax = CC(0x00467130, r, 0, 0xa);                                               // 0x00410a4d clamp
    ecx = G32(0x005685F4) & 7;                                                     // 0x00410a52
    if (FD(0x90) & G8(0x005619A0 + P1_nb(ecx, ebp, ebx))) esi = 2;                 // 0x00410a88
    if (FD(0x84) & G8(0x005619A0 + FD(0x14))) esi = 4;                             // 0x00410aa1
    if (eax > edi) {                                                               // 0x00410aaa
        int edx = FD(0x44) - FD(0x40);
        ecx = G8(0x005A34E0) * 8 - edi + eax;
        int rec = (ecx * 7 + esi) * 11;                                            // 0x00410ac8..0x00410ad6
        int d = FW(0x186 + 2 * esi) - FW(0x196 + 2 * esi);                         // 0x00410ae0, 0x00410aeb
        CC(0x004628D0, FD(0x50), edx, edx, d, 0x28, 0x00587E7C + rec * 4, 4, -1);   // 0x00410afd addSprite
    }
    // 0x00410b05
    ecx = (G32(0x005685F4) + 3) & 7;
    if (FD(0xb4) & G8(0x005619A0 + FD(0x14))) return;                              // 0x00410b24 -> 0x00410bde
    if ((FD(0x84) & G8(0x005619A0 + P1_nb(ecx, ebp, ebx))) == 0) return;           // 0x00410b54
    ecx = G32(0x005685F4);                                                         // 0x00410b5a
    eax = (ecx + 2) & 7;
    ecx = (ecx - 3) & 7;
    r = CC(0x0040BFE0, P1_t78(eax) + ebp, P1_t98(eax) + ebx, ecx, 0);             // 0x00410b87 cornerHeights
    eax = CC(0x00467130, r, 0, 0xa);                                               // 0x00410b90 clamp
    if (eax <= edi) return;                                                        // 0x00410b9a
    ecx = G8(0x005A34E0) * 8 - edi + eax;
    CC(0x004628D0, G32(0x004C2840) + FD(0x50), FD(0x44), FD(0x44), 0x13, 0x28, 0x00587F00 + ecx * 77 * 4, 4, -1);   // 0x00410bd6 addSprite
}

// ---- 0x00410bde..0x00410dd1: type-0x11 tile, side sprites from the records at 0x00587e7c and 0x00587f84.
static void P1_t11_410bde(u8* F, int ebp, int ebx) {
    int eax = G8(0x005619A0 + FD(0x14));                                           // 0x00410be9
    if ((FD(0x84) & eax) == 0) return;                                             // 0x00410bf2 -> 0x00410dd4
    if (FD(0x90) & eax) return;                                                    // 0x00410c01 -> 0x00410dd4
    int esi = 0;                                                                   // 0x00410c0c
    int edi = CC(0x0040BFE0, ebp, ebx, (G32(0x005685F4) + 1) & 7, 0);             // 0x00410c16 cornerHeights
    int ecx = G32(0x005685F4);                                                     // 0x00410c1b
    eax = ecx & 7;
    ecx = (ecx - 3) & 7;
    int r = CC(0x0040BFE0, P1_t78(eax) + ebp, P1_t98(eax) + ebx, ecx, 0);         // 0x00410c4a cornerHeights
    eax = CC(0x00467130, r, 0, 0xa);                                               // 0x00410c53 clamp
    ecx = (G32(0x005685F4) - 2) & 7;                                               // 0x00410c58
    if (FD(0x84) & G8(0x005619A0 + P1_nb(ecx, ebp, ebx))) esi = 5;                 // 0x00410c91
    if (FD(0x90) & G8(0x005619A0 + FD(0x14))) eax = 0;                             // 0x00410cac
    if (esi != 0 && eax > edi) {                                                   // 0x00410cb2, 0x00410cb6
        ecx = G8(0x005A34E0) * 8 - edi + eax;
        int rec = (ecx * 7 + esi) * 11;                                            // 0x00410cca..0x00410cd8
        int d = FW(0x186 + 2 * esi) - FW(0x196 + 2 * esi);                         // 0x00410cdb, 0x00410cea
        CC(0x004628D0, FD(0x50) - G32(0x004C2840), FD(0x44), FD(0x44), d, 0x28, 0x00587E7C + rec * 4, 4, -1);   // 0x00410d0b addSprite
    }
    // 0x00410d13
    eax = (2 - G32(0x005685F4)) & 7;
    if (FD(0x84) & G8(0x005619A0 + P1_nb(eax, ebp, ebx))) return;                  // 0x00410d4d -> 0x00410dd4
    ecx = G32(0x005685F4);                                                         // 0x00410d53
    eax = ecx & 7;
    ecx = (ecx + 3) & 7;
    r = CC(0x0040BFE0, P1_t78(eax) + ebp, P1_t98(eax) + ebx, ecx, 0);             // 0x00410d7f cornerHeights
    eax = CC(0x00467130, r, 0, 0xa);                                               // 0x00410d88 clamp
    if (eax <= edi) return;                                                        // 0x00410d92
    ecx = G8(0x005A34E0) * 8 - edi + eax;
    CC(0x004628D0, FD(0x50), FD(0x44) - FD(0x40), FD(0x44), 0xd, 0x28, 0x00587F84 + ecx * 77 * 4, 4, -1);   // 0x00410dcc addSprite
}

// ---- 0x00410dd4..0x00410ea1: type-0x11 tile, side sprite from the records at 0x00587c14 (clamped height > 3).
static void P1_t11_410dd4(u8* F, int ebp, int ebx) {
    int ecx = (G32(0x005685F4) - 1) & 7;                                           // 0x00410ddd
    int eax = G8(0x005619A0 + FD(0x14));                                           // 0x00410de0
    if ((eax & FD(0xcc)) == 0) return;                                             // 0x00410df3 -> 0x00410ea4
    if ((G8(0x005619A0 + P1_nb(ecx, ebp, ebx)) & FD(0xb4)) == 0) return;           // 0x00410e23
    ecx = G32(0x005685F4);                                                         // 0x00410e25
    eax = (ecx - 2) & 7;
    ecx = (ecx + 1) & 7;
    int r = CC(0x0040BFE0, P1_t78(eax) + ebp, P1_t98(eax) + ebx, ecx, 0);         // 0x00410e50 cornerHeights
    eax = CC(0x00467130, r, 0, 0xa);                                               // 0x00410e59 clamp
    if (eax <= 3) return;                                                          // 0x00410e64
    eax = eax + G8(0x005A34E0) * 8;                                                // 0x00410e75
    CC(0x004628D0, FD(0x50) - G32(0x004C2840), FD(0x44), FD(0x44), 6, 0x28, 0x00587C14 + eax * 77 * 4, 4, -1);   // 0x00410e9c addSprite
}

// ---- 0x00410ea4..0x00410ff5: type-0x11 tile with a type-0x11 neighbour flagged in [F+0xb4] / [F+0xcc]: cell
// 0x237, variant 3 / 2, index (date + 7 * ebp) mod [0x0053fcc4], drawn at -/+ half the 0x004c2840 width. Returns the two table
// pointers (esi, edi) set at 0x00410f56 / 0x00410f5d, which 0x0041102f / 0x00411040 read.
static void P1_t11_410ea4(u8* F, int ebp, int ebx, u32* pEsi, u32* pEdi) {
    int eax = G32(0x005685F4) & 7;                                                 // 0x00410ea9
    int idx = P1_nb(eax, ebp, ebx);                                                // 0x00410ec5
    if ((FD(0xb4) & G8(0x005619A0 + idx)) != 0 && G8(0x005722E8 + idx) == 0x11) {   // 0x00410ed8, 0x00410ee1
        int c = CC(0x0043D6F0, 0x237, (G32(0x00834170) + ebp * 7) % G32(0x0053FCC4), 3);   // 0x00410f03 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x00410f0f
        CC(0x004628D0, FD(0x50) - G32(0x004C2840) / 2, FD(0x40) / 3 + FD(0x44), FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x00410f42 addSprite
    }
    // 0x00410f4a
    eax = (G32(0x005685F4) + 2) & 7;
    u32 esi = 0x004C2878 + eax * 4;                                                // 0x00410f56
    u32 edi = 0x004C2898 + eax * 4;                                                // 0x00410f5d
    idx = ebx + (G32(esi) + ebp) * 50 + G32(edi);                                  // 0x00410f64..0x00410f73
    if ((FD(0xcc) & G8(0x005619A0 + idx)) != 0 && G8(0x005722E8 + idx) == 0x11) {   // 0x00410f86, 0x00410f8f
        int c = CC(0x0043D6F0, 0x237, (G32(0x00834170) + ebp * 7) % G32(0x0053FCC4), 2);   // 0x00410fb1 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x00410fbd
        CC(0x004628D0, G32(0x004C2840) / 2 + FD(0x50), FD(0x40) / 3 + FD(0x44), FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x00410ff0 addSprite
    }
    *pEsi = esi;
    *pEdi = edi;
}

// ---- 0x00410ff8..0x0041127f: [F+0x90] side, type-0x11 neighbour: the height step between two corners (cornerHeights
// difference, scaled by 0x004c2e00 * zoom / 8) picks cells 0x237 + 0x23b (step <= 3) or 0x238 + 0x23c (step > 3),
// variant 0, drawn at + half the 0x004c2840 width.
static void P1_t11_410ff8(u8* F, int ebp, int ebx, u32 esi, u32 edi) {
    if ((FD(0x90) & G8(0x005619A0 + FD(0x14))) == 0) return;                       // 0x0041100c -> 0x00411282
    int eax = CC(0x0040BFE0, ebp, ebx, (G32(0x005685F4) + 1) & 7, 0);             // 0x00411021 cornerHeights
    int edx = (G32(0x005685F4) - 1) & 7;                                           // 0x00411026
    int ecx = G32(esi);                                                            // 0x0041102f
    FD(0x18) = eax;                                                                // 0x0041103b
    int r = CC(0x0040BFE0, ecx + ebp, ebx + G32(edi), edx, 0);                    // 0x0041104a cornerHeights
    CC(0x00467130, r, 0, 0xa);                                                     // 0x00411053 clamp: its eax is never read (0x00411061 overwrites it)
    edx = G32(0x005685F4);                                                         // 0x00411058
    eax = (edx + 2) & 7;
    ecx = P1_t78(eax) + ebp;                                                       // 0x00411075
    eax = P1_t98(eax);                                                             // 0x0041106e
    if (G8(0x005722E8 + eax + ecx * 50 + ebx) != 0x11) return;                     // 0x00411088 -> 0x00411282
    edx = (G32(0x005685F4) - 3) & 7;                                               // 0x0041108e
    eax = CC(0x0040BFE0, ecx, eax + ebx, edx, 0);                                 // 0x004110a1 cornerHeights
    eax -= FD(0x18);                                                               // 0x004110ad
    if (eax <= 3) {                                                                // 0x004110b2 jg
        eax = (eax - 1) * G32(0x004C2E00) * G32(0x004C2844);                       // 0x004110b8..0x004110c7
        int edi2 = ebp * 7;
        int esi2 = eax / 8;                                                        // 0x004110ce..0x004110e8
        int c = CC(0x0043D6F0, 0x237, (G32(0x00834170) + edi2) % G32(0x0053FCC4), 0);   // 0x004110f1 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x004110f9
        if (esi2 > 0) {                                                            // 0x00411100
            CC(0x004628D0, G32(0x004C2840) / 2 + FD(0x50), FD(0x44) - FD(0x40) / 2 - esi2, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x00411132 addSprite
        }
        c = CC(0x0043D6F0, 0x23b, (G32(0x00834170) + edi2) % G32(0x0053FCD4), 0);  // 0x00411152 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x00411160
        int dv = (esi2 != 0 ? 1 : 0) + 2;                                          // 0x0041116c setne, 0x00411170
        CC(0x004628D0, G32(0x004C2840) / 2 + FD(0x50), FD(0x44) - FD(0x40) / dv, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x00411192 addSprite
    } else {
        // 0x0041119f
        eax = (eax + eax - 0xd) * G32(0x004C2E00) * G32(0x004C2844);
        int esi2 = ebp * 7;
        int edi2 = eax / 8;                                                        // 0x004111b8..0x004111d4
        int c = CC(0x0043D6F0, 0x238, (G32(0x00834170) + esi2) % G32(0x0053FCC8), 0);   // 0x004111dd cellAt
        G32(0x005A9CC8) = c;                                                       // 0x004111ed
        CC(0x004628D0, G32(0x004C2840) / 2 + FD(0x50), FD(0x44) - FD(0x40) / 2 - edi2, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x00411217 addSprite
        c = CC(0x0043D6F0, 0x23c, (G32(0x00834170) + esi2) % G32(0x0053FCD8), 0);  // 0x00411232 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x0041123e
        CC(0x004628D0, G32(0x004C2840) / 2 + FD(0x50), FD(0x44) - FD(0x40) / 3, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x0041127a addSprite
    }
}

// ---- 0x00411282..0x004114e2: the same for the [F+0x84] side, variant 1, drawn at - half the 0x004c2840 width.
static void P1_t11_411282(u8* F, int ebp, int ebx) {
    if ((FD(0x84) & G8(0x005619A0 + FD(0x14))) == 0) return;                       // 0x00411296 -> 0x004114e5
    int eax = G32(0x005685F4) & 7;                                                 // 0x004112a1
    u32 esi = 0x004C2878 + eax * 4;                                                // 0x004112a4
    u32 edi = 0x004C2898 + eax * 4;                                                // 0x004112ab
    if (G8(0x005722E8 + ebx + (G32(esi) + ebp) * 50 + G32(edi)) != 0x11) return;   // 0x004112c9 -> 0x004114e5
    eax = CC(0x0040BFE0, ebp, ebx, (G32(0x005685F4) + 1) & 7, 0);                 // 0x004112de cornerHeights
    int edx = (G32(0x005685F4) + 3) & 7;                                           // 0x004112e3
    int ecx = G32(esi);                                                            // 0x004112e9
    FD(0x18) = eax;                                                                // 0x004112f3
    eax = CC(0x0040BFE0, ecx + ebp, ebx + G32(edi), edx, 0);                      // 0x00411302 cornerHeights
    eax -= FD(0x18);                                                               // 0x0041130e
    if (eax <= 3) {                                                                // 0x00411313 jg
        eax = (eax - 1) * G32(0x004C2E00) * G32(0x004C2844);                       // 0x00411319..0x00411328
        int edi2 = ebp * 7;
        int esi2 = eax / 8;                                                        // 0x0041132f..0x0041134b
        int c = CC(0x0043D6F0, 0x237, (G32(0x00834170) + edi2) % G32(0x0053FCC4), 1);   // 0x00411354 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x0041135c
        if (esi2 > 0) {                                                            // 0x00411363
            CC(0x004628D0, FD(0x50) - G32(0x004C2840) / 2, FD(0x44) - FD(0x40) / 2 - esi2, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x00411395 addSprite
        }
        c = CC(0x0043D6F0, 0x23b, (G32(0x00834170) + edi2) % G32(0x0053FCD4), 1);  // 0x004113b5 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x004113c3
        int dv = (esi2 != 0 ? 1 : 0) + 2;                                          // 0x004113cf setne, 0x004113d3
        CC(0x004628D0, FD(0x50) - G32(0x004C2840) / 2, FD(0x44) - FD(0x40) / dv, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x004113f5 addSprite
    } else {
        // 0x00411402
        eax = (eax + eax - 0xd) * G32(0x004C2E00) * G32(0x004C2844);
        int esi2 = ebp * 7;
        int edi2 = eax / 8;                                                        // 0x0041141b..0x00411435
        int c = CC(0x0043D6F0, 0x238, (G32(0x00834170) + esi2) % G32(0x0053FCC8), 1);   // 0x0041143e cellAt
        G32(0x005A9CC8) = c;                                                       // 0x0041144e
        CC(0x004628D0, FD(0x50) - G32(0x004C2840) / 2, FD(0x44) - FD(0x40) / 2 - edi2, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x00411478 addSprite
        c = CC(0x0043D6F0, 0x23c, (G32(0x00834170) + esi2) % G32(0x0053FCD8), 1);  // 0x00411495 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x004114a1
        CC(0x004628D0, FD(0x50) - G32(0x004C2840) / 2, FD(0x44) - FD(0x40) / 3, FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);   // 0x004114dd addSprite
    }
}

// ---- 0x00410881..0x004114e2: what a type-0x11 tile draws before 0x004114e5. The rotation switch fills the four
// side masks [F+0x84] [F+0x90] [F+0xb4] [F+0xcc] for rotations 0/2/4/6 only (an odd rotation keeps the values the
// slots already hold); then two word tables at F+0x188 / F+0x198 and the six side-sprite phases. esi = [F+0x14].
static void P1_type11(u8* F, int ebp, int ebx, int esi) {
    switch ((u32)G32(0x005685F4)) {                                                // 0x00410889 ja, 0x0041088f table 0x00421618
    case 0:                                                                        // 0x00410896
        FD(0x84) = 1; FD(0x90) = 4; FD(0xb4) = 0x10; FD(0xcc) = 0x40;
        break;
    case 2:                                                                        // 0x004108c7
        FD(0x84) = 4; FD(0x90) = 0x10; FD(0xb4) = 0x40; FD(0xcc) = 1;
        break;
    case 4:                                                                        // 0x004108f5
        FD(0x84) = 0x10; FD(0x90) = 0x40; FD(0xb4) = 1; FD(0xcc) = 4;
        break;
    case 6:                                                                        // 0x00410923
        FD(0x84) = 0x40; FD(0x90) = 1; FD(0xb4) = 4; FD(0xcc) = 0x10;
        break;
    default:                                                                       // 0x0041094f: cases 1, 3, 5 and > 6
        break;
    }
    // 0x0041094f
    int edx = G8(0x005619A0 + esi);
    int eax = FD(0x90);                                                            // 0x00410956
    FW(0x198) = 1; FW(0x19a) = 0x13; FW(0x19c) = 0x2c; FW(0x19e) = 0x52;           // 0x0041095d..0x004109a5
    FW(0x1a0) = 0x71; FW(0x1a2) = 0x8e; FW(0x1a4) = 0xa1; FW(0x1a6) = 0xae;
    FW(0x188) = 6; FW(0x18a) = 0x20; FW(0x18c) = 0x3f; FW(0x18e) = 0x61;           // 0x004109af..0x004109eb
    FW(0x190) = 0x81; FW(0x192) = 0x9b; FW(0x194) = 0xa7;
    if (eax & edx) P1_t11_4109fb(F, ebp, ebx);                                          // 0x00410967 test, 0x004109f5
    P1_t11_410bde(F, ebp, ebx);                                                         // 0x00410bde
    P1_t11_410dd4(F, ebp, ebx);                                                         // 0x00410dd4
    u32 pEsi, pEdi;
    P1_t11_410ea4(F, ebp, ebx, &pEsi, &pEdi);                                          // 0x00410ea4
    P1_t11_410ff8(F, ebp, ebx, pEsi, pEdi);                                            // 0x00410ff8
    P1_t11_411282(F, ebp, ebx);                                                        // 0x00411282
}

// ---- 0x00411574..0x0041182f: type-0x11 tile: a pixel (when Random::range(3) != 0 and zoom >= 4) in one of three
// colours chosen from date and position, a cell 0x233..0x235 when the tile's -1 / -0x32 / +0x32 neighbours are type 0x11 and its +1 neighbour is not,
// and a cell 0x22e..0x232 at the next grid corner when four tiles are type 0x11 and two corner heights match.
// edi = [F+0x14] (0x004114e5).
static void P1_type11Deco(u8* F, int ebp, int ebx, int edi) {
    int eax = Rnd(3);                                                              // 0x0041157b Random::range
    if ((short)eax != 0 && G32(0x004C2844) >= 4) {                                 // 0x00411583 (test ax, ax), 0x0041158c
        int esi = G32(0x00834170);                                                 // 0x0041158e
        int ecx = esi / 2 - ebx * 9 - ebp * 11;                                    // 0x00411594..0x004115b9
        int edx = (esi + ebx * 16 + ebx) % G32(0x004C2840);                        // 0x004115ae..0x004115bc
        ecx &= 0x1f;                                                               // 0x004115c6
        edx += FD(0x28);                                                           // 0x004115c2, 0x004115c9
        if (ecx < 5) {                                                             // 0x004115ce jge; 0x004115d3 ja is the same bound
            int col;
            switch (ecx) {                                                         // 0x004115d5 table 0x00421634
            case 0: case 4: col = (int)0x80004210; break;                          // 0x004115dc
            case 1: case 3: col = (int)0x80006318; break;                          // 0x004115e3
            default: col = (int)0x80007FFF; break;                                 // 0x004115ef (case 2)
            }
            TC(0x00478EA0, 0x00519CD4, edx, FD(0x44), col);                        // 0x004115ff Widget_writePixel
        }
    }
    // 0x00411604
    if (G8(0x005722E8 + edi) != 0x11) return;                                      // 0x0041160b -> 0x00411832
    if (G8(0x005722E7 + edi) == 0x11 && GU8(0x0056988B + edi) != 0                 // 0x00411618, 0x00411626 (jbe)
        && G8(0x005722E9 + edi) != 0x11 && G8(0x0057231A + edi) == 0x11            // 0x00411633, 0x00411640
        && G8(0x005722B6 + edi) == 0x11 && (ebx + ebp) % 3 != 0) {                 // 0x0041164d, 0x00411660
        int ecx = (ebp + ebx * 2) % 3 + 0x233;                                     // 0x00411662..0x00411670
        u8 al = GU8(0x005A34E0);                                                   // 0x00411669
        int k;
        if (al == 2 || al == 3) k = G32(0x005685F4) / 2 + 1;                       // 0x00411678, 0x0041167c -> 0x0041168b
        else k = G32(0x005685F4) / 2 - 1;                                          // 0x0041167e
        int c = CC(0x0043D6F0, ecx, (G32(0x00834170) + ebp * 7) % G32(0x0053F3E8 + ecx * 4), k & 3);   // 0x004116b5 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x004116c1
        CC(0x004628D0, FD(0x50), FD(0x44), FD(0x44), 0x3c, 0x3c, c, 4, 0xb2);      // 0x004116d9 addSprite
    }
    // 0x004116e1
    if (G8(0x005722E8 + edi) != 0x11) return;                                      // 0x004116e8
    eax = FD(0x14);                                                                // 0x004116ee
    if (G8(0x0057231A + eax) != 0x11) return;                                      // 0x004116f9
    if (G8(0x005722E9 + eax) != 0x11) return;                                      // 0x00411708
    if (G8(0x0057231B + eax) != 0x11) return;                                      // 0x00411717
    int esi = ebp + 1;                                                             // 0x00411721
    edi = ebx + 1;                                                                 // 0x00411726
    FD(0xc) = esi;                                                                 // 0x00411729
    int h = CC(0x0040BFE0, ebp, ebx, 3, 0);                                        // 0x0041172d cornerHeights
    FD(0x18) = h;                                                                  // 0x0041173a
    eax = CC(0x0040BFE0, esi, edi, 7, 0);                                          // 0x0041173e cornerHeights
    if (FD(0x18) != eax) return;                                                   // 0x0041174c
    // 0x00411752
    esi = -1;
    u8 cl = GU8(0x0056988C + FD(0x14));                                            // 0x00411759
    if (cl <= 1) {                                                                 // 0x00411762 ja
        if ((((ebp - ebx) * 3) & 7) == 0) esi = (ebx + ebp) % 3 + 0x22e;           // 0x0041176d (test al, 7), 0x0041177c
        // 0x00411782: cmp cl, 1 / ja is never taken here (cl <= 1 tested at 0x0041175f)
        if ((ebx * ebx + ebp * 3) % 13 == 0) esi = (ebx + ebp) % 3 + 0x230;        // 0x0041179c, 0x004117ab
    }
    if (esi == -1) return;                                                         // 0x004117b4
    if (CC(0x0042FB90, FD(0xc) << 10, edi << 10, FA(0x208), FA(0x1dc), 0) == 0) return;   // 0x004117d4 worldToScreen, 0x004117de
    int ecx = (G32(0x005685F4) / 2 - ebx + ebp + 1) & 3;                           // 0x004117e0..0x004117fd
    int c = CC(0x0043D6F0, esi, G32(0x00834170) % G32(0x0053F3E8 + esi * 4), ecx); // 0x00411803 cellAt
    G32(0x005A9CC8) = c;                                                           // 0x00411816
    CC(0x004628D0, FD(0x208), FD(0x1dc), FD(0x1dc), 0x3c, 0x3c, c, 4, 0xb2);       // 0x0041182a addSprite
}

// ---- 0x00411832..0x00411912: two neighbour scans whose register results are never read: the first loop's esi / edi
// are overwritten at 0x004118ca / 0x004118dc, the second loop's esi is dead after 0x00411901 (checked with
// c3fy_firstuse.py). Their frame writes stay: [F+0xc] = 0, [F+0x10] = 9, [F+0x18] = 0 at the end. Then a tile whose
// type is not 1 loses flag 0x80 of its 0x0053caf0 word. Returns eax and dl as they are at 0x00411914.
static void P1_scan(u8* F, int ebp, int ebx, int* pEax, u8* pDl) {
    int edi = 0;                                                                   // 0x00411832
    int eax = 1;                                                                   // 0x00411834
    FD(0xc) = edi;                                                                 // 0x00411839
    FD(0x10) = eax;                                                                // 0x0041183d
    int esi;
    do {                                                                           // 0x00411841
        int edx = G32(0x005685F4) + eax - 1;                                       // 0x00411851
        esi = 0;                                                                   // 0x00411847
        FD(0x18) = 3;                                                              // 0x00411849
        do {                                                                       // 0x00411855
            FD(0xc) = FD(0xc) >> 1;                                                // 0x00411855..0x0041185b
            eax = edx & 7;
            esi >>= 1;                                                             // 0x00411864
            int t = G8(0x005722E8 + P1_nb(eax, ebp, ebx));                         // 0x00411884
            edi |= (int)(1u << (t & 31));                                          // 0x0041188c shl eax, cl; 0x00411894
            if (G8(0x00578376 + t * 48) != FD(0x20)) esi |= 4;                     // 0x0041189d, 0x004118a1
            edx++;                                                                 // 0x004118aa
            FD(0x18) = FD(0x18) - 1;                                               // 0x004118ab
        } while (FD(0x18) != 0);                                                   // 0x004118b0
        eax = FD(0x10) + 2;                                                        // 0x004118b2
        FD(0x10) = eax;                                                            // 0x004118bc
    } while (eax < 8);                                                             // 0x004118c0
    // 0x004118c6
    u8 dl = GU8(0x005722E8 + FD(0x14));                                            // 0x004118ce
    esi = FD(0xc);                                                                 // 0x004118ca
    int ecx = 0;                                                                   // 0x004118d4
    do {                                                                           // 0x004118d6
        eax = G32(0x004C2878 + ecx) + ebp;
        edi = G32(0x004C2898 + ecx);
        esi >>= 1;                                                                 // 0x004118e4
        eax = edi + eax * 50;                                                      // 0x004118e6..0x004118ec
        if (GU8(0x005722E8 + eax + ebx) == dl) esi |= 8;                           // 0x004118f6
        ecx += 8;
    } while (ecx < 0x20);                                                          // 0x00411901
    if (dl != 1) {                                                                 // 0x00411906
        eax = FD(0x14);                                                            // 0x00411908
        GU8(0x0053CAF0 + eax * 2) &= 0x7f;                                         // 0x0041190c
    }
    (void)esi;
    (void)edi;
    *pEax = eax;
    *pDl = dl;
}

// ---- 0x00411cf9..0x00411e14: flag-0x20 tile: cell 0x1ff for each of the four even directions whose neighbour
// has flag 0x20 and is not type 0x11, then the cell from the word table F+0x1b8 (variant [F+0x10c + mask]) and, for
// word 0x1fe, cell 0x204 with index date mod [0x0053fbf8].
static void P1_flag20Rest(u8* F, int ebp, int ebx) {
    int esi = 0;                                                                   // 0x00411cf9
    u32 edi = 0x004C2898;                                                          // 0x00411cfb
    do {                                                                           // 0x00411d00
        int eax = (G32(0x005685F4) + esi * 2) & 7;
        int idx = P1_nb(eax, ebp, ebx);                                            // 0x00411d24
        if ((GU8(0x0053CAF0 + idx * 2) & 0x20) != 0 && G8(0x005722E8 + idx) != 0x11) {   // 0x00411d2f, 0x00411d38
            int c = CC(0x0043D6F0, 0x1ff, 0, (-1 - esi) & 3);                      // 0x00411d4a cellAt
            int ecx = (G32(edi) - 1) * FD(0x40);                                   // 0x00411d4f..0x00411d58
            G32(0x005A9CC8) = c;                                                   // 0x00411d5f
            CC(0x00462A30, FD(0x50), FD(0x44), ecx + FD(0x44), 0x3c, 0x3c, c, 4, 0x4e);   // 0x00411d72 enqueueSprite
        }
        edi += 8;                                                                  // 0x00411d7a
        esi++;
    } while ((int)edi < 0x004C28B8);                                               // 0x00411d84
    int eax = FD(0x18);                                                            // 0x00411d8a
    if (eax == 0) return;                                                          // 0x00411d90 -> 0x00411e17
    int s = FB(0x10c + eax);                                                       // 0x00411d96
    int e = FD(0xc);                                                               // 0x00411d9e
    int c = CC(0x0043D6F0, (int)(short)e, 0, s);                                   // 0x00411da9 cellAt
    G32(0x005A9CC8) = c;                                                           // 0x00411db6
    CC(0x00462A30, FD(0x50), FD(0x44), FD(0x44) - FD(0x40), 0x3c, 0x3c, c, 4, 0x4e);   // 0x00411dcf enqueueSprite
    if ((short)e != 0x1fe) return;                                                 // 0x00411ddc
    c = CC(0x0043D6F0, 0x204, G32(0x00834170) % G32(0x0053FBF8), s);               // 0x00411df1 cellAt
    G32(0x005A9CC8) = c;                                                           // 0x00411dfa
    CC(0x004628D0, FD(0x50), FD(0x44), FD(0x44), 0x3c, 0x3c, c, 4, 0x4e);          // 0x00411e0f addSprite
}

// ---- 0x00411959..0x00411e14: tile with g_tileFlags bit 0x20, [0x00578372 + 48 * type] > 0, type not 0x15 / 0x16. Builds the
// neighbour mask [F+0x20] (flag 0x20 of the four even neighbours), the 16-word cell table F+0x1b8 and the 16-byte
// variant table F+0x10c; for a tile whose 0x00578376 attribute is 0x11 it draws the cells (0x226 + low 5 flag bits,
// 0x1ff, 0x205 or the table's cell).
static void P1_flag20(u8* F, int ebp, int ebx) {
    int edi = G32(0x005685F4);                                                     // 0x00411959
    int esi = 0;
    int edx = 6;                                                                   // 0x00411961
    do {                                                                           // 0x00411966
        int eax = (edx + edi) & 7;
        esi <<= 1;                                                                 // 0x0041196c
        int idx = P1_nb(eax, ebp, ebx);                                            // 0x00411987
        if (GU8(0x0053CAF0 + idx * 2) & 0x20) esi |= 1;                            // 0x00411991
        edx -= 2;
    } while (edx >= 0);                                                            // 0x00411999 jns
    FW(0x1be) = 0x200; FW(0x1c4) = 0x200; FW(0x1ca) = 0x200; FW(0x1d0) = 0x200;   // 0x004119a5..0x004119bd
    edx = FD(0x10);                                                                // 0x004119c5
    FW(0x1ba) = 0x1fe; FW(0x1bc) = 0x1fe; FW(0x1c0) = 0x1fe;                       // 0x004119c9..0x004119f1
    FW(0x1c2) = 0x1fe; FW(0x1c8) = 0x1fe; FW(0x1cc) = 0x1fe;
    u8 al = GU8(0x00578376 + edx);                                                 // 0x004119f9
    FD(0x20) = esi;                                                                // 0x00411a06
    FW(0x1b8) = 0;                                                                 // 0x00411a0a
    FW(0x1c6) = 0x201; FW(0x1ce) = 0x201; FW(0x1d2) = 0x201; FW(0x1d4) = 0x201;   // 0x00411a14..0x00411a2c
    FW(0x1d6) = 0x202;                                                             // 0x00411a34
    FB(0x10c) = 0; FB(0x10d) = 1; FB(0x10e) = 0; FB(0x10f) = 2;                    // 0x00411a3e..0x00411ab6
    FB(0x110) = 1; FB(0x111) = 1; FB(0x112) = 1; FB(0x113) = 2;
    FB(0x114) = 0; FB(0x115) = 3; FB(0x116) = 0; FB(0x117) = 3;
    FB(0x118) = 0; FB(0x119) = 0; FB(0x11a) = 1; FB(0x11b) = 0;
    if (al != 0x11) return;                                                        // 0x00411a04 cmp, 0x00411abe -> 0x00411e17
    // 0x00411ac4
    edx = G32(0x005685F4);
    esi = 0;
    edi = 0;
    FD(0x10) = 1;                                                                  // 0x00411ace
    FD(0x18) = 4;                                                                  // 0x00411ad6
    do {                                                                           // 0x00411ade
        int eax = edx & 7;
        esi >>= 1;                                                                 // 0x00411ae3
        int ecx = P1_t78(eax) + ebp;
        eax = P1_t98(eax) + ebx;
        edi >>= 1;                                                                 // 0x00411af7
        eax = eax + ecx * 50;                                                      // 0x00411af9..0x00411aff
        if (GU8(0x0053CAF0 + eax * 2) & 0x20) {                                    // 0x00411b0a
            int t = G8(0x005722E8 + eax);                                          // 0x00411b0c
            esi |= 8;                                                              // 0x00411b13
            if (G8(0x00578376 + t * 48) == 0x11) FD(0x10) = 0;                     // 0x00411b23, 0x00411b25
            else edi |= 8;                                                         // 0x00411b2f
        }
        edx += 2;                                                                  // 0x00411b36
        FD(0x18) = FD(0x18) - 1;                                                   // 0x00411b39
    } while (FD(0x18) != 0);                                                       // 0x00411b3e
    int f10 = FD(0x10);                                                            // 0x00411b40
    FD(0x18) = esi;                                                                // 0x00411b44
    if (f10 != 0) {                                                                // 0x00411b4a
        int eax = FD(0xc);                                                         // 0x00411b50
        if ((eax & 0x100) != 0) {                                                  // 0x00411b5f (test cx, cx)
            int sel = (FD(0x20) & 5) != 0 ? 1 : 0;                                 // 0x00411b68..0x00411b74 (and dl, 5 / neg / sbb / neg)
            int c = CC(0x0043D6F0, (eax & 0x1f) + 0x226, 0, sel);                  // 0x00411b7a cellAt
            G32(0x005A9CC8) = c;                                                   // 0x00411b8e
            CC(0x00462A30, FD(0x50), FD(0x44), FD(0x44) - FD(0x40), 0x28, 0x28, c, 4, 0xa9);   // 0x00411ba3 enqueueSprite
            return;                                                                // 0x00411ba8 -> 0x00411e14
        }
        // 0x00411bad
        int e5 = FD(0x20) & 5;
        int c = CC(0x0043D6F0, 0x1ff, 0, e5 != 0 ? 1 : 0);                         // 0x00411bb9 setne, 0x00411bc4 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x00411bc9
        int z = G32(0x004C2840);                                                   // 0x00411bce
        int ecx = (e5 != 0) ? -(z / 2) : z / 2;                                    // 0x00411bd9 -> 0x00411bdb / 0x00411be5
        int hs = FD(0x40) / 2;                                                     // 0x00411beb..0x00411c00 (esi)
        int y = FD(0x44) - hs;                                                     // 0x00411c05
        CC(0x00462A30, ecx + FD(0x50), y, y, 0x3c, 0x3c, G32(0x005A9CC8), 4, 0x4e);   // 0x00411c14 enqueueSprite
        c = CC(0x0043D6F0, 0x1ff, 0, (e5 != 0 ? 1 : 0) + 2);                       // 0x00411c1d setne, 0x00411c2b cellAt
        z = G32(0x004C2840);                                                       // 0x00411c32
        G32(0x005A9CC8) = c;                                                       // 0x00411c3a
        int ax = (e5 != 0) ? -(z / 2) : z / 2;                                     // 0x00411c43 -> 0x00411c45 / 0x00411c4d
        CC(0x00462A30, FD(0x50) - ax, hs + FD(0x44), FD(0x44) - FD(0x40), 0x3c, 0x3c, c, 4, 0x4e);   // 0x00411c71 enqueueSprite
        return;                                                                    // 0x00411c79 -> 0x00411e17
    }
    // 0x00411c7e: si = word [F+0x1b8 + 2 * esi]; the upper half of esi is the mask's (0 here: esi <= 15)
    esi = (int)(((u32)esi & 0xFFFF0000u) | FUW(0x1b8 + esi * 2));
    int eax = (short)esi;                                                          // 0x00411c86
    FD(0xc) = esi;                                                                 // 0x00411c8e
    if (eax == 0x1fe) {                                                            // 0x00411c92
        int k;
        switch ((u32)(edi - 1)) {                                                  // 0x00411c94 dec, 0x00411c98 ja, 0x00411c9a table 0x00421648
        case 0: k = 3; break;                                                      // 0x00411ca1
        case 1: k = 2; break;                                                      // 0x00411ca8
        case 3: k = 1; break;                                                      // 0x00411caf
        case 7: k = 0; break;                                                      // 0x00411cb6
        default:                                                                   // 0x00411cf9: cases 2, 4, 5, 6 and > 7
            P1_flag20Rest(F, ebp, ebx);
            return;
        }
        // 0x00411cb8
        int c = CC(0x0043D6F0, 0x205, 0, k);                                       // 0x00411cc0 cellAt
        G32(0x005A9CC8) = c;                                                       // 0x00411ccd
        CC(0x00462A30, FD(0x50), FD(0x44), FD(0x44) - FD(0x40), 0x3c, 0x3c, c, 4, 0x4e);   // 0x00411ce6 enqueueSprite
        return;                                                                    // 0x00411ceb -> 0x00411e14
    }
    if (eax == -1) return;                                                         // 0x00411cf0, 0x00411cf3 -> 0x00411e17
    P1_flag20Rest(F, ebp, ebx);                                                    // 0x00411cf9
}

// ---- 0x00411e17..0x00411f14: tile with flag 0x200: for each even direction canStep allows, an enqueued cell
// 0x208 + (0x0056988c byte mod 7), offset by the direction tables times the zoom; with no allowed direction the
// tile's flag 0x200 is cleared.
static void P1_flag200(u8* F, int ebp, int ebx) {
    if ((GU16(0x0053CAF0 + FD(0x14) * 2) & 0x200) == 0) return;                    // 0x00411e2c -> 0x00411f1c
    int esi = 0;                                                                   // 0x00411e32
    int edi = 0;                                                                   // 0x00411e34
    FD(0xc) = edi;                                                                 // 0x00411e36
    do {                                                                           // 0x00411e3a
        if (CC(0x00407400, ebp, ebx, edi) != 0) {                                  // 0x00411e3d canStep, 0x00411e47
            esi = (FD(0xc) - G32(0x005685F4) + 3) & 7;                             // 0x00411e4d..0x00411e6f
            int rem = (int)GU8(0x0056988C + FD(0x14)) % 7;                         // 0x00411e61..0x00411e6d (edi)
            int edx = (-2 - esi / 2) & 3;                                          // 0x00411e72..0x00411e88
            int c = CC(0x0043D6F0, rem + 0x208, 0, edx);                           // 0x00411e8f cellAt
            int ecx = G32(0x004C2898 + esi * 4) * G32(0x004C2844);                 // 0x00411e94, 0x00411ea1
            G32(0x005A9CC8) = c;                                                   // 0x00411eab
            int y0 = FD(0x44);                                                     // 0x00411eb1
            int y3 = y0 - (ecx * 6) / 2;                                           // 0x00411eb5..0x00411ec3
            int y2 = y0 - (ecx * 5) / 2;                                           // 0x00411ec7..0x00411ed4
            int x = FD(0x50) - G32(0x004C2878 + esi * 4) * G32(0x004C2844) * 4;    // 0x00411ecb, 0x00411ed6..0x00411ee7
            CC(0x00462A30, x, y2, y3, 0x28, 0x28, c, 4, rem + 0xaa);               // 0x00411eeb enqueueSprite
            edi = FD(0xc);                                                         // 0x00411ef0
            esi = 1;                                                               // 0x00411ef7
        }
        edi += 2;                                                                  // 0x00411efc
        FD(0xc) = edi;                                                             // 0x00411f02
    } while (edi < 8);                                                             // 0x00411f06
    if (esi == 0) GU8(0x0053CAF1 + FD(0x14) * 2) &= 0xfd;                          // 0x00411f0e, 0x00411f14
}

// ---- 0x00411f3a..0x004120ab: tile with flag 0x80: with mode [0x00567afc] == 3 and the selected record
// [0x005a59f8] (stride 0x100) whose 0x005794d9 byte equals the tile's low 5 flag bits, Sprite_draw of 0x005423e8
// (only while flag 4 of the date's low byte or flag 4 of 0x0059e7b8 is set); then an enqueued cell 0x185 / 0x189 + [0x005a34e0].
// edi = [F+0x14], eax = [F+0xc] = flag word & 0x1f.
static void P1_flag80(u8* F, int edi, int eax) {
    if (G32(0x00567AFC) == 3) {                                                    // 0x00411f41
        int ecx = G32(0x005A59F8);                                                 // 0x00411f43
        if (ecx != -1 && G8(0x005794D9 + (ecx << 8)) == eax                        // 0x00411f4c, 0x00411f5a
            && ((GU8(0x00834170) & 4) != 0 || (GU8(0x0059E7B8) & 4) != 0)) {       // 0x00411f63, 0x00411f6c
            TC(0x004741B0, 0x005423E8, Root(), G32(0x004C2840) + FD(0x28) - 0x14, FD(0x2c) + FD(0x40) - 0xa,
               G32(0x00824148), 0);                                                // 0x00411f9e Sprite_draw
        }
    }
    // 0x00411fa3
    u16 cx = GU16(0x0053CAF0 + edi * 2);
    eax = 0;                                                                       // 0x00411fab
    int esi = G8(0x005A34E0);                                                      // 0x00411fad
    int ebx = esi + 0x189;                                                         // 0x00411fbc
    if ((cx & 0x4800) == 0x4000) {                                                 // 0x00411fc2, 0x00411fc7
        int edx = FD(0x14);                                                        // 0x00411fc9
        edi = GU8(0x00578804 + edx);                                               // 0x00411fcf, 0x00411fd5
        ebx = esi + 0x185;                                                         // 0x00411fd7
        esi = G32(0x0053F3E8 + ebx * 4);                                           // 0x00411fdd
        if (edi >= esi) {                                                          // 0x00411fe6 jl
            cx = (u16)(cx & 0xBFFF);                                               // 0x00411fec and ch, 0xbf
            edi = esi - 1;                                                         // 0x00411fef
            GU16(0x0053CAF0 + edx * 2) = cx;                                       // 0x00411ff2
        }
    } else {
        // 0x00411ffc
        int k = FD(0xc);
        esi = ((k << 6) + k) << 3;                                                 // 0x00412002..0x0041200b (k * 0x208)
        int ecx = G32(0x00575AD4 + esi) / 2 + G32(0x00575AD0 + esi) + 4;           // 0x0041200e..0x0041201f
        int q = ((int)G16(0x00575C08 + esi) * 100) / ecx;                          // 0x00412023..0x00412036
        edi = G32(0x00834170) % G32(0x0053F3E8 + ebx * 4);                         // 0x00412038..0x0041204a
        eax = CC(0x00467130, q / 20, 0, 3);                                        // 0x00412045..0x00412056 (/ 20), 0x00412059 clamp
        if (GU8(0x00575AB0 + esi) == 0) eax = 1;                                   // 0x00412067, 0x0041206b
    }
    // 0x00412070
    int c = CC(0x0043D6F0, ebx, edi, eax);                                         // 0x00412073 cellAt
    int y = FD(0x2c) + FD(0x40);                                                   // 0x00412078..0x00412085
    G32(0x005A9CC8) = c;                                                           // 0x0041208c
    CC(0x00462A30, G32(0x004C2840) + FD(0x28), y, y, 0x28, 0x28, c, 4, 0x63);      // 0x0041209e enqueueSprite
}

// ---- 0x004120ae..0x0041232b: type-0 tile with low flag bits k != 0 (record k * 0x208 of g_holes 0x00575ab0): with a
// non-zero 0x00575ab0 byte, recomputes the tile's 0x0056988c byte from the rotation and the record's 0x00575ab1 /
// 0x00575cb0 fields, draws two cells 0x18d..0x18f mirrored about the tile, and with record flags 1 / 2 an
// enqueued sprite from the records at 0x0058a538; with a zero byte, clears the tile's 0x0056988c byte.
// eax = [F+0xc], edi = [F+0x14].
static void P1_hole(u8* F, int eax, int edi) {
    if (GU8(0x005722E8 + edi) != 0) return;                                        // 0x004120b6 -> 0x00412332
    if (eax == 0) return;                                                          // 0x004120be
    int esi = ((eax << 6) + eax) << 3;                                             // 0x004120c4..0x004120cb (eax * 0x208)
    FD(0x18) = esi;                                                                // 0x004120ce
    u8 dl = GU8(0x00575AB0 + esi);                                                 // 0x004120d2
    if (dl == 0) {                                                                 // 0x004120da
        GU8(0x0056988C + edi) = 0;                                                 // 0x0041232b
        return;
    }
    int ecx = G8(0x00575AB1 + esi);                                                // 0x004120e0
    FD(0x2c) = (int)GU8(0x0056988C + edi);                                         // 0x004120eb, 0x004120f1
    u8 al = (u8)((u8)(GU8(0x005685F4) - (u8)ecx - 2) & 7);                         // 0x004120f5..0x004120fe
    al = (u8)(al * 3);                                                             // 0x00412100 imul bl (bl = 3 at 0x004120e9)
    int ebx = G32(0x00575CB0 + esi);                                               // 0x00412102
    al = (u8)(al + 1);                                                             // 0x00412108
    GU8(0x0056988C + edi) = al;                                                    // 0x0041210d
    if (ebx & 0x20) {                                                              // 0x0041210a, 0x00412113
        al = (u8)(al + 1);
        GU8(0x0056988C + edi) = al;                                                // 0x00412117
    }
    if (ebx & 0x40) GU8(0x0056988C + edi) = (u8)(GU8(0x0056988C + edi) + 2);       // 0x00412120, 0x00412122
    if ((int)GU8(0x0056988C + edi) != FD(0x2c)) GU8(0x005619A0) &= 0xfd;           // 0x00412137, 0x00412139 (absolute 0x005619a0, no tile index)
    // 0x00412140
    ebx = ((ecx - G32(0x005685F4)) + 1) & 7;                                       // 0x00412140..0x00412150
    int a = CC(0x00467130, (int)(s8)dl - 3, 0, 2);                                 // 0x00412159 clamp
    int edi2 = G32(0x0053FA1C + a * 4) - 1;                                        // 0x00412162, 0x00412179
    a += 0x18d;                                                                    // 0x00412169
    u16 si = GU16(0x0053CAF0 + FD(0x14) * 2);                                      // 0x00412171
    int ecx2 = edi2;                                                               // 0x0041217c
    if ((si & 0x4800) == 0x4000) {                                                 // 0x0041217e, 0x00412189
        ecx2 = GU8(0x00578804 + FD(0x14));                                         // 0x00412191
        if (ecx2 >= G32(0x0053F3E8 + a * 4)) {                                     // 0x004121a0 jl
            si = (u16)(si & 0xBFFF);                                               // 0x004121a6
            ecx2 = edi2;                                                           // 0x004121ac
            GU16(0x0053CAF0 + FD(0x14) * 2) = si;                                  // 0x004121ae
        }
    }
    // 0x004121b6
    int c = CC(0x0043D6F0, a, ecx2, 0);                                            // 0x004121ba cellAt
    u32 pB = 0x004C2898 + ebx * 4;                                                 // 0x004121c2 (esi)
    G32(0x005A9CC8) = c;                                                           // 0x004121d3
    u32 pA = 0x004C2878 + ebx * 4;                                                 // 0x004121d8 (ebx)
    int x2, y2, c2;
    if (ebx & 1) {                                                                 // 0x004121c9 test bl, 1; 0x004121e1
        int dy = G32(pB) * G32(0x0058B384);                                        // 0x004121e3..0x004121ea
        int dx = G32(0x0058B384) * G32(pA);                                        // 0x004121ed
        CC(0x004628D0, FD(0x50) + dx * 4, FD(0x44) + dy * 2, FD(0x44), 0x28, 0x28, c, 4, 0x5e);   // 0x00412201 addSprite
        dy = G32(pB) * G32(0x0058B384);                                            // 0x00412206..0x00412213
        dx = G32(0x0058B384) * G32(pA);                                            // 0x00412216
        c2 = G32(0x005A9CC8);                                                      // 0x0041220d
        y2 = FD(0x44) - dy * 2;                                                    // 0x00412226..0x00412229
        x2 = FD(0x50) - dx * 4;                                                    // 0x0041222c..0x00412236
    } else {
        // 0x0041223b
        int dy = G32(pB) * G32(0x0058B384);                                        // 0x0041223b..0x00412243
        int dx = G32(0x0058B384) * G32(pA);                                        // 0x00412246
        int yy = FD(0x44) + dy * 2 + dy;                                           // 0x00412249..0x00412251
        int xx = FD(0x50) + dx * 4 + dx;                                           // 0x00412254..0x0041225d
        CC(0x004628D0, xx, yy, FD(0x44), 0x28, 0x28, c, 4, 0x5e);                  // 0x00412260 addSprite
        c2 = G32(0x005A9CC8);                                                      // 0x00412265
        int z = G32(0x0058B384);                                                   // 0x0041226a
        dy = G32(pB) * z;                                                          // 0x00412275, 0x00412277
        dx = z * G32(pA);                                                          // 0x0041227a
        y2 = FD(0x44) - dy * 3;                                                    // 0x0041227d..0x00412289
        x2 = FD(0x50) - dx * 5;                                                    // 0x0041228c..0x00412296
    }
    CC(0x004628D0, x2, y2, FD(0x44), 0x28, 0x28, c2, 4, 0x5e);                     // 0x00412299 addSprite (both arms)
    // 0x0041229e
    esi = FD(0x18);
    int edx = FD(0xc);                                                             // 0x004122a2
    int fl = G32(0x00575CB0 + esi);                                                // 0x004122a9
    ecx = edx - 1;                                                                 // 0x004122af
    if (fl & 1) ecx = edx + 0x11;                                                  // 0x004122b4, 0x004122b6
    if (fl & 2) ecx = edx + 0x23;                                                  // 0x004122bb, 0x004122bd
    if ((fl & 3) == 0) return;                                                     // 0x004122c2 -> 0x00412332
    int bit = ((int)GU8(0x00575AB1 + esi) - G32(0x005685F4)) & 4;                  // 0x004122c4..0x004122d2 (low byte of the difference)
    int z = G32(0x0058B384);                                                       // 0x004122d4
    int yy;
    if (bit != 0) yy = FD(0x44) - z * 3;                                           // 0x004122d9, 0x004122db..0x004122e2
    else yy = z + FD(0x44) + z * 2;                                                // 0x0041231f..0x00412326
    // 0x004122e4
    CC(0x00462A30, FD(0x50), yy, yy, 9, (ecx / 18) * 3 + 0x10, 0x0058A538 + ecx * 11 * 4, 4, -1);   // 0x00412315 enqueueSprite
}

// ---- 0x00410606..0x0041232b: one tile (ebp, ebx) of the per-tile loop; returning = 0x00412332.
static void P1_tile(u8* F, int ebp, int ebx) {
    FD(0x14) = ebx + ebp * 50;                                                     // 0x00410606..0x0041061c
    if (CC(0x0042F940, ebp, ebx, FA(0x50), FA(0x44)) == 0) return;                 // 0x00410620 tileToScreen, 0x0041062a
    FD(0x28) = FD(0x50) - G32(0x004C2840);                                         // 0x00410630..0x00410640
    FD(0x2c) = FD(0x44) - FD(0x40);                                                // 0x00410644..0x0041064a
    P1_grid(F, ebp, ebx);                                                          // 0x0041064e
    // 0x004107c1: tile type 0x14 draws nothing
    int esi = FD(0x14);
    int t = G8(0x005722E8 + esi);                                                  // 0x004107c5
    if (t == 0x14) return;                                                         // 0x004107cd
    FD(0x20) = G8(0x00578376 + t * 0x30);                                          // 0x004107d3..0x004107f4
    TC(0x004762D0, Root(), 0x00519FD8, 0, 0, 0);                                   // 0x004107f8 setDrawTarget
    if ((GU8(0x0059E7B8) & 0x20) != 0) {                                           // 0x00410804
        int v = G8(0x005619A0 + esi);                                              // 0x00410806
        if (v != 0) {                                                              // 0x0041080e
            GU8(kBuf) = 0;                                                         // 0x0041081b
            int r = Itoa(v, 0x0058A528, 10);                                       // 0x00410822 __itoa
            StrCat(kBuf, (u32)r);                                                  // 0x00410827..0x0041085b
            CC(0x00404B70, (int)kBuf, FD(0x50), FD(0x44) - 4, (int)0x80007FFF);    // 0x00410868 drawCenteredText
            esi = FD(0x14);                                                        // 0x0041086d
        }
    }
    if (G8(0x005722E8 + esi) == 0x11) P1_type11(F, ebp, ebx, esi);                 // 0x00410874, 0x0041087b
    // 0x004114e5: type 0xb on course type 3 without flag 0x100: enqueued cell 0x12a
    int edi = FD(0x14);
    if (G8(0x005722E8 + edi) == 0xb && G8(0x005A34E0) == 3                         // 0x004114f0, 0x004114f9
        && (GU16(0x0053CAF0 + edi * 2) & 0x100) == 0) {                            // 0x004114fb..0x0041150c
        int c = CC(0x0043D6F0, 0x12a, 0, (G32(0x005685F4) / 2 - ebx * 2 + ebp) & 3);   // 0x0041152a cellAt
        G32(0x005A9CC8) = c;                                                       // 0x00411545
        int k = ((ebx + ebp) % 3 != 0) ? 3 : 4;                                    // 0x0041153f idiv, 0x0041154b..0x0041154f
        CC(0x00462A30, FD(0x50), FD(0x44), FD(0x44), 0x28, 0x26, c, k, 0xa3);      // 0x0041155f enqueueSprite
    }
    if (G8(0x005722E8 + edi) == 0x11) P1_type11Deco(F, ebp, ebx, edi);             // 0x00411567, 0x0041156e
    // 0x00411832
    int eax;
    u8 dl;
    P1_scan(F, ebp, ebx, &eax, &dl);
    // 0x00411914: [F+0xc] = flag word (upper half of eax kept: mov ax, word)
    eax = (int)(((u32)eax & 0xFFFF0000u) | GU16(0x0053CAF0 + FD(0x14) * 2));      // 0x00411914, 0x00411918
    FD(0xc) = eax;                                                                 // 0x00411922
    if ((eax & 0x20) != 0) {                                                       // 0x00411920 test al, 0x20; 0x00411926
        int a48 = (int)(s8)dl * 48;                                                // 0x0041192c..0x00411932
        FD(0x10) = a48;                                                            // 0x00411935
        if (G8(0x00578372 + a48) > 0 && dl != 0x16 && dl != 0x15) {                // 0x00411941 jle, 0x0041194a, 0x00411953
            P1_flag20(F, ebp, ebx);                                                // 0x00411959
            P1_flag200(F, ebp, ebx);                                               // 0x00411e17
        }
    }
    // 0x00411f1c
    edi = FD(0x14);
    u16 cx = GU16(0x0053CAF0 + edi * 2);                                           // 0x00411f20
    eax = cx & 0x1f;                                                               // 0x00411f28, 0x00411f2a
    FD(0xc) = eax;                                                                 // 0x00411f30
    if ((cx & 0x80) != 0) {                                                        // 0x00411f2d test cl, 0x80; 0x00411f34
        P1_flag80(F, edi, eax);                                                    // 0x00411f3a
        eax = FD(0xc);                                                             // 0x004120a3
        edi = FD(0x14);                                                            // 0x004120a7
    }
    P1_hole(F, eax, edi);                                                          // 0x004120ae
}

// ---- 0x00410604..0x0041234e: the per-tile loop. Outer counter ebx (kept in [F+0x30]), inner counter ebp, both
// 0..49; the tile body can clobber ebx (0x00411fbc onward), so it is reloaded from [F+0x30] after each tile.
void Ph_00410604(u8* F) {
    int ebx = 0;                                                                   // 0x004105fa (entry)
    do {
        int ebp = 0;                                                               // 0x00410604
        do {
            P1_tile(F, ebp, ebx);                                                  // 0x00410606
            ebx = FD(0x30);                                                        // 0x00412332
            ebp++;                                                                 // 0x00412336
        } while (ebp < 0x32);                                                      // 0x0041233a
        ebx++;                                                                     // 0x00412340
        FD(0x30) = ebx;                                                            // 0x00412344
    } while (ebx < 0x32);                                                          // 0x00412348
}

}  // namespace
// c3fy part P2: mainLoop 0x0040f5c0, range 0x0041234e..0x0041353d (the second per-tile pass).
// Hand transcription of log/c3/alt/c3fy_norm.asm. Every [F+X] access of the original is a frame access at the same
// offset and width; every callee is called at its original address (CC cdecl, TC thiscall).
// Loop: b = [F+0x30] (esi) outer 0..49, a = [F+0x20] (ebp) inner 0..49, tile index idx = a * 50 + b ([F+0x14]).
namespace {

// cellAt 0x0043d6f0 (cdecl, 3 arguments) followed by the store of its result into [0x005a9cc8], the order every
// cellAt site of this range has (no call between the two)
static inline int P2_cell(int t, int f, int r) {
    int c = CC(0x0043D6F0, t, f, r);
    G32(0x005A9CC8) = c;
    return c;
}
// enqueueSprite 0x00462a30 (cdecl, 8 arguments) with the arguments ([F+0x3c], [F+0x38], [F+0x38], w, h, cell, 4, k)
static inline void P2_enq(u8* F, int w, int h, int cell, int k) {
    CC(0x00462A30, FD(0x3c), FD(0x38), FD(0x38), w, h, cell, 4, k);
}
// addSprite 0x004628d0 (cdecl, 8 arguments) with the same argument shape; its return value is never read here
static inline void P2_add(u8* F, int w, int h, int cell, int k) {
    CC(0x004628D0, FD(0x3c), FD(0x38), FD(0x38), w, h, cell, 4, k);
}

// 0x004123bd..0x0041246b: when bit 0x20 of the byte [0x0059e7b8] is set, three filled rectangles on the window object
// 0x00519a60 (fillWindowRect 0x00480b00, thiscall, 5 arguments) whose heights come from the per-tile byte tables
// 0x0053ea24, 0x005a6378 (x2) and 0x0056c7e4 (x2), width [0x004c2840] / 4, at the tile's screen point ([F+0x3c], [F+0x38]).
static void P2_bars(u8* F, int idx) {
    if (!(GU8(0x0059E7B8) & 0x20)) return;   // 0x004123bd / 0x004123c4
    {
        int q = G32(0x004C2840) / 4;          // 0x004123ca..0x004123e7 (cdq; and edx, 3; add; sar 2)
        int h = GU8(0x0053EA24 + idx);        // 0x004123d1
        TC(0x00480B00, 0x00519A60, q + FD(0x3c), FD(0x38) - h, q, h, (int)0x80000210);   // 0x004123fa fillWindowRect
    }
    {
        int q = G32(0x004C2840) / 4;          // 0x004123ff..0x00412424
        int h2 = GU8(0x005A6378 + idx) << 1;  // 0x00412406 / 0x0041241b
        TC(0x00480B00, 0x00519A60, FD(0x3c) - q, FD(0x38) - h2, q, h2, (int)0x800023E8);   // 0x00412431 fillWindowRect
    }
    {
        int q = G32(0x004C2840) / 4;          // 0x00412436..0x00412454
        int h2 = GU8(0x0056C7E4 + idx) << 1;  // 0x0041243d / 0x00412452
        TC(0x00480B00, 0x00519A60, FD(0x3c), FD(0x38) - h2, q, h2, (int)0x80007D08);   // 0x00412466 fillWindowRect
    }
}

// 0x0041246b..0x0041264f: clears the text buffer; when bit 0x2000 of the tile word is set, selects the draw target
// (0x0051b360 when [0x004c2844] >= 4, else 0x00519fd8), builds a label in the text buffer from the tile type byte
// [0x005722e8 + idx] (switch table 0x00421668 on type - 12) and draws it centred (drawCenteredText2 0x00404bc0) at
// ([F+0x3c], [F+0x38] - 3) on one tile of every 8 in a date-dependent pattern.
// Returns the value ecx holds at 0x0041264f (its upper 16 bits reach [F+0xc] at 0x00412659; see the notes).
static u32 P2_label(u8* F, int a, int idx) {
    u16 w = GU16(0x0053CAF0 + 2 * idx);       // 0x0041246b
    GU8(kBuf) = 0;                             // 0x00412473
    u32 ecx = w & 0x2000;                      // 0x0041247a
    if (ecx == 0) return ecx;                  // 0x00412480 / 0x00412483 -> 0x0041264f
    u32 tgt = 0x0051B360;                      // 0x00412491
    if (G32(0x004C2844) < 4) tgt = 0x00519FD8; // 0x0041248e / 0x00412496 / 0x00412498
    int root = Root();                         // 0x0041249d
    TC(0x004762D0, root, (int)tgt, 0, 0, 0);   // 0x004124aa setDrawTarget
    ecx = (u32)root;                           // the original setDrawTarget (0x004762d0..0x00476301) never writes ecx
    int sel = G8(0x005722E8 + idx) - 0xc;      // 0x004124af / 0x004124b6
    u32 cat = 0;                               // edi at 0x004125ca, the strcat source
    bool doCat = true;
    switch ((u32)sel) {                        // 0x004124b9 ja 0x004125f2; 0x004124c2 jmp [table 0x00421668]
    case 5:                                    // type 17 -> 0x004124c9
        StrCpy(kBuf, 0x004C69D0);              // 0x004124c9..0x004124f7
        cat = FU(0x238 + 4 * ((FD(0x30) + a) % 16));   // 0x004124d9 / 0x004124e6 / 0x004124f2..0x00412500
        break;                                 // 0x00412507 -> 0x004125ca
    case 6:                                    // type 18 -> 0x0041250c
        StrCpy(kBuf, FU(0x238 + 4 * ((FD(0x30) + a) % 16)));   // 0x0041250c..0x00412543
        cat = 0x004C69C4;                      // 0x00412545
        break;                                 // 0x0041254a -> 0x004125ca
    case 0:                                    // type 12 -> 0x0041254c
        StrCpy(kBuf, FU(0x218 + 4 * ((FD(0x30) + a) % 8)));    // 0x0041254c..0x00412583
        cat = 0x004C69BC;                      // 0x00412585
        break;                                 // 0x0041258a -> 0x004125ca
    case 1:
    case 2:
    case 3:                                    // types 13..15 -> 0x0041258c
        StrCpy(kBuf, FU(0x218 + 4 * ((FD(0x30) + a) % 8)));    // 0x0041258c..0x004125c3
        cat = 0x004C69B4;                      // 0x004125c5, falls into 0x004125ca
        break;
    default:                                   // type 16 (entry 4) and type - 12 > 6 unsigned -> 0x004125f2
        doCat = false;
        break;
    }
    if (doCat) {
        StrCat(kBuf, cat);                     // 0x004125ca..0x004125f0
        ecx = 0;                               // rep movsb at 0x004125f0 ends with ecx = 0
    }
    if (GU8(kBuf) == 0) return ecx;            // 0x004125f2 / 0x004125f9 -> 0x0041264f
    int b = FD(0x30);                          // 0x004125fb
    ecx = (u32)(b + a);                        // 0x00412608
    int d = (G32(0x00834170) >> 5) + (b + a) + 2 * b;   // 0x004125ff..0x0041260e
    if (d & 7) return ecx;                     // 0x00412610 test dl, 7 / 0x00412613
    if (GU8(0x005A5A00) & 1) return ecx;       // 0x00412615 / 0x0041261c
    if (G32(0x004C2844) <= 2) return ecx;      // 0x0041261e / 0x00412625
    if (G32(0x00567AFC) == 3) return ecx;      // 0x00412627 / 0x0041262e
    CC(0x00404BC0, (int)kBuf, FD(0x3c), FD(0x38) - 3, (int)0x80007FFF);   // 0x00412647 drawCenteredText2
    return 0;   // ecx as drawCenteredText2 leaves it: not determined (notes, UNCERTAIN 1)
}

// 0x0041264f..0x004129a4: [F+0xc] = ecx with the tile word in its low half. When bit 0x1000 of the tile word is set:
// a 4-bit mask of the four neighbours (offset tables 0x004c2878 / 0x004c2898) that also have bit 0x1000, the same
// [0x0056988c] / 5 and the same [0x00543018] byte, rotated right [0x005685f4] / 2 times within 4 bits; it selects a
// sprite type (word table 0x004c2f28) and rotation (0x004c2f2a), the type is moved by 12 or 6 for [0x0056988c] >= 10 or
// >= 5, the frame is the type's last frame (count table 0x0053f3e8) or [F+0x10]; cellAt + addSprite at the tile's point.
// For every fifth tile ((b + 2a) % 5 == 0) whose mask has bits 0 and 3, whose (a-1, b-1) tile has bit 0x1000 and whose
// heightBlend at (a-1, b-1) equals the one at (a, b): a second sprite (type 0x1b4 + (a & 1)) enqueued at one of four
// offsets selected by [0x005685f4] (switch table 0x00421684). Every path through the bit-0x1000 block then clears bit
// 0x100 of the tile word (0x0041299c).
static void P2_paths(u8* F, int idx, u32 ecx) {
    u16 w = GU16(0x0053CAF0 + 2 * idx);       // 0x0041264f (mov cx, word)
    ecx = (ecx & 0xFFFF0000u) | w;
    FU(0xc) = ecx;                             // 0x00412659
    if (!(w & 0x1000)) return;                 // 0x0041265d..0x00412665 -> 0x004129a4
    int ti = FD(0x14);                         // edx 0x0041266b
    int h = GU8(0x0056988C + ti);              // ebx 0x0041266f / 0x0041267c
    u32 c800 = ecx & 0x800;                    // 0x00412676
    int mask = 0;                              // ebp 0x00412682
    FU(0x18) = c800;                           // 0x00412686
    FD(0x2c) = h % 5 + 0x2d;                   // 0x0041268a..0x00412693
    if ((u16)c800 != 0) FD(0x2c) = 0xbb;       // 0x00412690 / 0x00412697 / 0x00412699
    for (int i = 0; i < 0x20; i += 8) {        // 0x004126a1 edi = 0; 0x00412721..0x00412727
        int ni = (G32(0x004C2878 + i) + FD(0x20)) * 50 + G32(0x004C2898 + i) + FD(0x30);   // 0x004126a3..0x004126c4
        mask >>= 1;                            // 0x004126b5
        if ((GU16(0x0053CAF0 + 2 * ni) & 0x1000) != 0x1000) continue;   // 0x004126c7..0x004126da
        int nq = GU8(0x0056988C + ni) / 5;     // 0x004126dc..0x004126f9
        if (h / 5 != nq) continue;             // 0x004126fb..0x00412708
        int tj = FD(0x14);                     // 0x0041270a
        if (GU8(0x00543018 + tj) == GU8(0x00543018 + ni)) mask |= 8;   // 0x0041270e..0x0041271e
    }
    int r = G32(0x005685F4) / 2;               // 0x0041272d..0x00412735
    if (r > 0) {                               // 0x00412737 / 0x00412739
        do {
            int lo = (mask & 1) << 3;          // 0x0041273b..0x00412740
            mask >>= 1;                        // 0x00412743
            mask |= lo;                        // 0x00412745
        } while (--r != 0);                    // 0x00412747 / 0x00412748
    }
    int t = G16(0x004C2F28 + 4 * mask);        // ecx 0x0041274a
    int rot = G16(0x004C2F2A + 4 * mask);      // eax 0x00412752
    if (t == 0x1a3) rot ^= 2;                  // 0x0041275a / 0x00412762
    if (t == 0x1a5) rot = (rot + 1) & 3;       // 0x00412765 / 0x0041276d / 0x0041276e
    if (rot == -1) rot = (FD(0x20) - 2 * FD(0x30)) & 3;   // 0x00412771..0x00412785
    if (h >= 0xa) t += 0xc;                    // 0x00412788 / 0x0041278d
    else if (h >= 5) t += 6;                   // 0x00412792 / 0x00412797
    int last = G32(0x0053F3E8 + 4 * t) - 1;    // edi 0x0041279a / 0x004127a5
    int f10 = FD(0x10);                        // 0x004127a1
    int frame = last;                          // esi 0x004127a9
    if (f10 != -1 && FW(0x18) == 0) {          // 0x004127a6 / 0x004127ab / 0x004127ad / 0x004127b3
        int tj = FD(0x14);                     // ebx 0x004127b5
        frame = f10;                           // 0x004127b9
        if ((int)GU8(0x00578804 + tj) >= last) {   // 0x004127bb..0x004127c5
            GU16(0x0053CAF0 + 2 * tj) = (u16)(FU(0xc) & 0xFFFFBFFFu);   // 0x004127c7..0x004127d0
            GU8(0x00578804 + tj) = 0xff;       // 0x004127d8
        }
    }
    int cell = CC(0x0043D6F0, t, frame, rot);  // 0x004127e2 cellAt
    int k2c = FD(0x2c);                        // 0x004127e7
    G32(0x005A9CC8) = cell;                    // 0x004127f2
    CC(0x004628D0, FD(0x3c), FD(0x38), FD(0x38), 0x2a, 0x2a, cell, 4, k2c);   // 0x00412803 addSprite
    int bb = FD(0x30);                         // ebx 0x00412808
    int aa = FD(0x20);                         // esi 0x0041280c
    int tk = FD(0x14);                         // ebp at 0x0041299c (read at 0x00412834 or 0x00412998)
    if ((bb + aa * 2) % 5 == 0 && (u8)(mask & 9) == 9) {   // 0x00412818..0x0041282e
        if ((GU16(0x0053CA8A + 2 * tk) & 0x1000) == 0x1000) {   // 0x00412834..0x00412849 (tile idx - 51)
            int hb1 = CC(0x0040C170, aa - 1, bb - 1, 0);   // 0x00412859 heightBlend
            int hb2 = CC(0x0040C170, aa, bb, 0);           // 0x00412864 heightBlend
            if (hb2 == hb1) {                  // 0x0041286c / 0x0041286e
                int odd = aa & 1;              // 0x00412874 / 0x00412876
                int t2 = odd + 0x1b4;          // ecx 0x00412879
                int k = (odd != 0 ? 0x26 : 0) + 0x96;   // esi 0x0041287f..0x0041288b
                int f = FD(0x10);              // 0x0041288d
                int fr2, rot2;
                int cnt = 0;
                if (f != -1 && f < (cnt = G32(0x0053F3E8 + 4 * t2)) &&
                    (GU16(0x0053CAF0 + 2 * tk) & 0x800) == 0) {   // 0x00412891..0x004128b1
                    rot2 = (G32(0x005685F4) / 2 + bb) & 3;          // 0x004128b3..0x004128bf
                    fr2 = FD(0x10) % cnt;                           // 0x004128c3..0x004128c8
                } else {                                            // 0x004128cc
                    rot2 = (G32(0x005685F4) / 2 + bb) & 3;          // 0x004128cc..0x004128df
                    fr2 = G32(0x0053F3E8 + 4 * t2) - 1;             // 0x004128d4 / 0x004128e2
                }
                int c2 = CC(0x0043D6F0, t2, fr2, rot2);   // 0x004128e6 cellAt
                u32 sw = GU32(0x005685F4);     // 0x004128eb
                G32(0x005A9CC8) = c2;          // 0x004128f7
                switch (sw) {                  // 0x004128f4 ja 0x0041299c; 0x00412902 jmp [table 0x00421684]
                case 0:                        // -> 0x00412909
                    CC(0x00462A30, FD(0x3c) - G32(0x004C2840), FD(0x38), FD(0x38), 0x50, 0x4c, c2, 4, k);   // 0x00412924 enqueueSprite
                    break;
                case 2:                        // -> 0x0041292e
                    CC(0x00462A30, FD(0x3c), FD(0x40) + FD(0x38), FD(0x38), 0x50, 0x4c, c2, 4, k);   // 0x00412947 enqueueSprite
                    break;
                case 4:                        // -> 0x00412951
                    CC(0x00462A30, G32(0x004C2840) + FD(0x3c), FD(0x38), FD(0x38), 0x50, 0x4c, c2, 4, k);   // 0x0041296b enqueueSprite
                    break;
                case 6:                        // -> 0x00412975
                    CC(0x00462A30, FD(0x3c), FD(0x38) - FD(0x40), FD(0x38), 0x50, 0x4c, c2, 4, k);   // 0x0041298e enqueueSprite
                    break;
                default:                       // 1, 3, 5 and > 6 -> 0x0041299c
                    break;
                }
            }
        }
    }
    GU8(0x0053CAF1 + 2 * tk) &= 0xfe;          // 0x0041299c (clears bit 0x100 of the tile word)
}

// 0x004129a4..0x0041311c: when bit 0x100 of the tile word is set: draw target 0x00519fd8, a scenery label
// (sceneryLabel 0x00407b60 into the text buffer, drawn centred at ([F+0x3c], [F+0x38] + 5)) under the same three global
// gates as the type label, then one or two sprites chosen by the class byte [0x00578376 + 48 * type] (switch on
// class - 4 through the byte table 0x004216c0 and the jump table 0x004216a0), with rotation
// ([0x005685f4] / 2 - 2b + a) & 3 unless the case computes its own.
static void P2_scenery(u8* F) {
    int ti = FD(0x14);                         // edi 0x004129a4
    int aa = FD(0x20);                         // ebx 0x004129a8
    if (!(GU16(0x0053CAF0 + 2 * ti) & 0x100)) return;   // 0x004129ac..0x004129bc -> 0x0041311c
    TC(0x004762D0, Root(), 0x00519FD8, 0, 0, 0);   // 0x004129d3 setDrawTarget
    if (!(GU8(0x005A5A00) & 1) && G32(0x004C2844) > 2 && G32(0x00567AFC) != 3) {   // 0x004129d8..0x004129f1
        int b0 = FD(0x30);                     // 0x004129f3
        GU8(kBuf) = 0;                         // 0x004129fb
        if (CC(0x00407B60, aa, b0, 1) != 0)    // 0x00412a02 sceneryLabel / 0x00412a0c
            CC(0x00404BC0, (int)kBuf, FD(0x3c), FD(0x38) + 5, (int)0x80007FFF);   // 0x00412a25 drawCenteredText2
    }
    int twob = FD(0x30) * 2;                   // ebp 0x00412a32 / 0x00412a37
    s8 ty = G8(0x005722E8 + ti);               // cl 0x00412a3a
    u8 ty8 = (u8)ty;
    int rot = ((G32(0x005685F4) / 2) - twob + aa) & 3;   // esi 0x00412a2d..0x00412a5d
    int cls = G8(0x00578376 + 48 * (int)ty) - 4;        // 0x00412a44..0x00412a5a
    int c;
    // 0x00412a60 ja 0x004130bd; 0x00412a6b byte table 0x004216c0[cls] -> 0x00412a71 jmp [0x004216a0 + 4 * that]
    switch ((u32)cls) {
    case 14:
    case 15:                                   // classes 18, 19: byte 5 -> 0x00412a78
        if (GU8(0x005A34E0) == 1) break;       // 0x00412a78 / 0x00412a7f
        c = P2_cell(0x12c, 0, rot);            // 0x00412a8d cellAt / 0x00412a92
        P2_enq(F, 0x28, 0x28, c, 0xa5);        // 0x00412aae enqueueSprite (tail 0x00412a97..0x00412ab3)
        break;
    case 13: {                                 // class 17: byte 4 -> 0x00412abb
        int tj = FD(0x14);                     // 0x00412abb
        if (GU8(0x0053CAF0 + 2 * tj) & 0x20) break;   // 0x00412abf / 0x00412ac7
        if (ty8 == 0x12) break;                // 0x00412acd / 0x00412ad0
        u8 hh = GU8(0x0056988C + tj);          // dl 0x00412ad6
        if (hh != 0 && ty8 == 0x11) {          // 0x00412ad8 / 0x00412ade / 0x00412ae0 / 0x00412ae3
            int b0 = FD(0x30);                 // esi 0x00412ae5
            int v = (G32(0x00834170) + b0 * 7) + aa * 3;   // eax 0x00412ae9..0x00412afd
            int m = v % 64;                    // ecx 0x00412aff..0x00412b0d
            int lim = G32(0x0053FA64);         // 0x00412b0e
            if (m >= lim) break;               // 0x00412b14 / 0x00412b16 -> 0x0041311c
            int r2 = ((v >> 6) + b0 + aa) & 3; // 0x00412b1c..0x00412b23
            int cl = CC(0x00467130, m, 0, lim - 1);   // 0x00412b2c clamp
            c = P2_cell(0x19f, cl, r2);        // 0x00412b3a cellAt / 0x00412b46
            P2_enq(F, 0x3c, 0x3c, c, 0x5a);    // 0x00412aae enqueueSprite (tail 0x00412aa3)
            break;
        }
        if (GU8(0x005A34E0) == 1) {            // 0x00412b52 / 0x00412b59
            c = P2_cell(0x129, 0, rot);        // 0x00412b63 cellAt / 0x00412b68
            P2_enq(F, 0x28, 0x28, c, 0xa2);    // 0x00412ed3 enqueueSprite (tail 0x00412ec1)
        } else {
            int fr = G32(0x00834170) % G32(0x0053FA68);   // 0x00412b77..0x00412b7f idiv
            c = P2_cell(0x1a0, fr, 0);         // 0x00412b8b cellAt / 0x00412b98
            P2_enq(F, 0x28, 0x28, c, 0x1d);    // 0x00412ba9 enqueueSprite
        }
        break;
    }
    case 9: {                                  // class 13: byte 3 -> 0x00412bb6
        c = P2_cell(0xfc, 0, rot);             // 0x00412bbe cellAt / 0x00412bc7
        P2_add(F, 0x28, 0x28, c, 0x59);        // 0x00412bdc addSprite
        int tj = FD(0x14);                     // edi 0x00412be1
        u8 t2 = GU8(0x005722E8 + tj);          // al 0x00412be8
        if (t2 == 0x10) {                      // 0x00412bee / 0x00412bf0
            int hh = GU8(0x0056988C + tj);     // ecx 0x00412bf8 / 0x00412bfa
            GU8(0x0053CAF1 + 2 * tj) &= 0xf7;  // 0x00412c00 (clears bit 0x800 of the tile word)
            int m = hh % 8;                    // 0x00412c08..0x00412c15
            int st = m + 0x12e;                // edi 0x00412c16
            int k = m + 0xb3;                  // esi 0x00412c1c
            if (m == 0) {                      // 0x00412c22 (flags of 0x00412c0a and / 0x00412c15 inc)
                st = 0xf9;                     // 0x00412c32
                int sh = CC(0x0042DBA0, aa << 8, FD(0x30) << 8);   // 0x00412c37 sampleHeight
                k = ((sh / 32) & 3) + 0x36;    // 0x00412c3c..0x00412c4d
            }
            int c2;
            if (FD(0x10) != -1) {              // 0x00412c50 / 0x00412c54 / 0x00412c57
                int f = FD(0x10);
                c2 = CC(0x0043D6F0, st, f, (aa - twob) & 3);   // 0x00412c59..0x00412c63 cellAt
                int lastf = G32(0x0053F3E8 + 4 * st) - 1;      // 0x00412c68 / 0x00412c76
                int f2 = FD(0x10);             // 0x00412c6f
                G32(0x005A9CC8) = c2;          // 0x00412c79
                if (f2 == lastf) GU8(0x0053CAF1 + 2 * FD(0x14)) &= 0xbf;   // 0x00412c77 / 0x00412c7e / 0x00412c84
                if (FD(0x10) == 0) {           // 0x00412c8c..0x00412c92
                    CC(0x004481B0, 0x93, 0x64, 0, 0, 0);   // 0x00412ca1 playSound
                    c2 = G32(0x005A9CC8);      // 0x00412ca6
                }
                FD(0x10) = -2;                 // 0x00412cae
            } else {                           // 0x00412cb8
                int lastf = G32(0x0053F3E8 + 4 * st) - 1;      // 0x00412cb8 / 0x00412cc4
                c2 = CC(0x0043D6F0, st, lastf, (aa - twob) & 3);   // 0x00412cbf..0x00412cc8 cellAt
                G32(0x005A9CC8) = c2;          // 0x00412cd0
            }
            if (st == 0xf9)                    // 0x00412cd6 / 0x00412cdf
                CC(0x00462A30, FD(0x3c), FD(0x38), FD(0x38), 0x8c, 0x8c, c2, 4, k);   // 0x00412cf6 enqueueSprite
            else
                CC(0x00462A30, FD(0x3c), FD(0x38), FD(0x38), 0x64, 0x64, c2, 4, k);   // 0x00412d12 enqueueSprite
            break;
        }
        if (GU8(0x005A34E0) == 1) {            // 0x00412d1f / 0x00412d26
            c = P2_cell(0xfa, G32(0x0053F7D0) - 1, rot);   // 0x00412d35 cellAt / 0x00412d3a
            P2_enq(F, 0x3c, 0x3c, c, 0x57);    // 0x00412e32 enqueueSprite (tail 0x00412e20)
            break;
        }
        if (t2 == 0xd) {                       // 0x00412d46 / 0x00412d48
            c = P2_cell(0xfd, G32(0x0053F7DC) - 1, rot);   // 0x00412d58 cellAt / 0x00412d61
            P2_enq(F, 0x64, 0x64, c, 0x5f);    // 0x00412d76 enqueueSprite
        }
        if (GU8(0x005722E8 + tj) == 0xf) {     // 0x00412d7e / 0x00412d85
            c = P2_cell(0x173, 0, rot);        // 0x00412d8f cellAt / 0x00412d9c
            P2_enq(F, 0x8c, 0x8c, c, 0x6b);    // 0x00412db3 enqueueSprite
        }
        if (GU8(0x005722E8 + tj) == 0xe) {     // 0x00412dbb / 0x00412dc2
            c = P2_cell(0x12f, G32(0x0053F8A4) - 1, rot);  // 0x00412dd6 cellAt / 0x00412de5
            P2_enq(F, 0x64, 0x64, c, 0xb4);    // 0x00412aae enqueueSprite (tail 0x00412aa3)
        }
        break;
    }
    case 0:                                    // class 4: byte 0 -> 0x00412df1
        if (ty8 == 4) {                        // 0x00412df1 / 0x00412df4 -> 0x00412fa4
            int cr = G32(0x0059BF90);          // 0x00412fa4
            if (GU8(0x00571FF4 + 46 * cr) == 0xa) {   // 0x00412fa9..0x00412fb9
                c = P2_cell(0x100, 0, rot);    // 0x00412fc3 cellAt / 0x00412fd2
                P2_add(F, 0x3c, 0x3c, c, 0x81);   // 0x00413025 addSprite (tail 0x0041301a)
            } else if (GU8(0x005A34E0) == 1) { // 0x00412fdb / 0x00412fe2
                int q = (FD(0x30) + aa) & 3;   // edi 0x00412fe4..0x00412fee
                c = P2_cell((q != 0 ? -1 : 0) + 0x128, 0, rot);   // 0x00412ff1..0x00412ffe cellAt / 0x00413007
                P2_add(F, 0x28, 0x28, c, (q != 0 ? -1 : 0) + 0xa1);   // 0x00413003 / 0x0041300c / 0x00413025 addSprite
            } else {                           // 0x00413032
                c = P2_cell(0xfc, 0, rot);     // 0x0041303a cellAt / 0x00413043
                P2_add(F, 0x28, 0x28, c, 0x59);   // 0x00413058 addSprite
                c = P2_cell(0xfe, 0, rot);     // 0x00413065 cellAt / 0x00413072
                P2_enq(F, 0x3c, 0x3c, c, 0x55);   // 0x00413114 enqueueSprite (tail; add esp, 0x58 pops both)
            }
        } else if (ty8 == 0xa) {               // 0x00412dfa / 0x00412dfd
            if (GU8(0x005A34E0) == 1) {        // 0x00412e03 / 0x00412e09 / 0x00412e0d
                c = P2_cell(0x102, 0, rot);    // 0x00412e14 cellAt / 0x00412e19
                P2_enq(F, 0x3c, 0x3c, c, 0x5c);   // 0x00412e32 enqueueSprite (tail 0x00412e20)
            } else {                           // 0x00412e3f
                c = P2_cell(0xfc, 0, rot);     // 0x00412e44 cellAt / 0x00412e51
                P2_add(F, 0x28, 0x28, c, 0x59);   // 0x00412e62 addSprite
                c = P2_cell(0xfd, 0, rot);     // 0x00412e6f cellAt / 0x00412e78
                P2_enq(F, 0x64, 0x64, c, 0x5f);   // 0x00413114 enqueueSprite (tail; add esp, 0x58)
            }
        } else if (ty8 == 0xc) {               // 0x00412e98 / 0x00412e9b
            u8 g = GU8(0x005A34E0);            // al 0x00412ea1
            if (g == 1) {                      // 0x00412ea6 / 0x00412ea8
                c = P2_cell(0x12b, 0, rot);    // 0x00412eb2 cellAt / 0x00412eb7
                P2_enq(F, 0x28, 0x28, c, 0xa4);   // 0x00412ed3 enqueueSprite
            } else if (g == 3) {               // 0x00412ee0 / 0x00412ee5
                c = P2_cell(0x12e, 0, rot);    // 0x00412eec cellAt / 0x00412ef1
                P2_enq(F, 0x28, 0x28, c, 0xa8);   // 0x00412f0d enqueueSprite
            } else {                           // 0x00412f1a
                c = P2_cell(0x16d, 0, rot);    // 0x00412f1f cellAt / 0x00412f2e
                P2_enq(F, 0x8c, 0x8c, c, 0x69);   // 0x00412aae enqueueSprite (tail 0x00412aa3)
            }
        } else {                               // 0x00412f3d
            int cr = G32(0x0059BF90);          // 0x00412f3d
            if (GU8(0x00571FF4 + 46 * cr) == 0xd) {   // 0x00412f45..0x00412f55
                c = P2_cell(0xff, 0, rot);     // 0x00412f8c cellAt / 0x00412f98
                P2_enq(F, 0x3c, 0x3c, c, 0x56);   // 0x00412aae enqueueSprite (tail 0x00412aa3)
            } else {
                c = P2_cell(0x101, 0, rot);    // 0x00412f5c cellAt / 0x00412f69
                P2_enq(F, 0x3c, 0x3c, c, 0x5b);   // 0x00412f7a enqueueSprite
            }
        }
        break;
    case 3:                                    // class 7: byte 1 -> 0x0041308b
        c = P2_cell(0xfb, 0, rot);             // 0x00413093 cellAt / 0x00413098
        P2_enq(F, 0x28, 0x28, c, 0x58);        // 0x00412f0d enqueueSprite (tail 0x00412efb)
        break;
    case 8:                                    // class 12: byte 2 -> 0x004130a4
        c = P2_cell(0x16d, 0, rot);            // 0x004130ac cellAt / 0x004130b1
        P2_enq(F, 0x28, 0x28, c, 0x69);        // 0x00412aae enqueueSprite (tail 0x00412a9c)
        break;
    case 17:
    case 18:                                   // classes 21, 22: byte 6 -> 0x0041311c (nothing)
        break;
    default:                                   // byte 7 and class - 4 > 0x12 unsigned -> 0x004130bd
        c = P2_cell(0xfc, 0, rot);             // 0x004130c5 cellAt / 0x004130d2
        P2_add(F, 0x28, 0x28, c, 0x59);        // 0x004130e3 addSprite
        c = P2_cell(0x12d, 0, rot);            // 0x004130f0 cellAt / 0x00413100
        P2_enq(F, 0x28, 0x28, c, 0xa7);        // 0x00413114 enqueueSprite (add esp, 0x58)
        break;
    }
}

// 0x00413279..0x0041342c: the object kind n == 2 body (after 0x004131ca). nearestHole is called and its result
// discarded; [F+0x18] = ([0x0056988c] % 4) + 0x2e; a 4-bit neighbour mask of tiles with bit 0x1000 and the same
// [0x00543018] byte, rotated like the first one; bit 0x100 of the tile word cleared, the word kept in [F+0x2c];
// then either a typed sprite (bit 4 of [0x0059e7b8]) or the fixed sprite 0x00568258 + 44 * ((mask - 1) & 15)
// at the screen point ([F+0xe0], [F+0xe4]). ebx is the register the caller's 0x004134ce reads.
static void P2_obj2(u8* F, int& ebx) {
    int bb = FD(0x30);                         // 0x00413279
    int aa = FD(0x20);                         // 0x0041327d
    int m = 0;                                 // esi 0x00413283
    CC(0x00407340, aa, bb);                    // 0x00413285 nearestHole; its result % 4 (0x0041328d..0x00413298) is overwritten at 0x0041329d
    int ti = FD(0x14);                         // edi 0x00413299
    FD(0x18) = (int)GU8(0x0056988C + ti) % 4 + 0x2e;   // 0x0041329d..0x004132b6
    for (int i = 0; i < 0x20; i += 8) {        // 0x004132b4 ecx = 0; 0x00413307..0x0041330d
        int ni = (G32(0x004C2878 + i) + FD(0x20)) * 50 + G32(0x004C2898 + i) + FD(0x30);   // 0x004132ba..0x004132db
        m >>= 1;                               // 0x004132c6
        if ((GU16(0x0053CAF0 + 2 * ni) & 0x1000) != 0x1000) continue;   // 0x004132dd..0x004132f0 -> 0x00413307
        if (GU8(0x00543018 + ti) == GU8(0x00543018 + ni)) m |= 8;     // 0x004132f2..0x00413300
        ti = FD(0x14);                         // 0x00413303
    }
    GU8(0x0053CAF1 + 2 * ti) &= 0xfe;          // 0x0041330f
    FUW(0x2c) = GU16(0x0053CAF0 + 2 * ti);     // 0x00413317 / 0x0041331f
    int r = G32(0x005685F4) / 2;               // 0x00413324..0x0041332c
    if (r > 0) {                               // 0x0041332e / 0x00413330
        do {
            int lo = (m & 1) << 3;             // 0x00413332..0x00413337
            m >>= 1;                           // 0x0041333a
            m |= lo;                           // 0x0041333c
        } while (--r != 0);                    // 0x0041333e / 0x0041333f
    }
    if (GU8(0x0059E7B8) & 4) {                 // 0x00413341 / 0x00413348
        int t = G16(0x004C2F28 + 4 * m);       // edi 0x0041334e
        int rot = G16(0x004C2F2A + 4 * m);     // eax 0x00413356
        if (t == 0x1a5) rot = (rot + 1) & 3;   // 0x0041335e..0x00413367
        if (rot == -1) rot = (FD(0x20) - 2 * FD(0x30)) & 3;   // 0x0041336a..0x0041337e
        int last = G32(0x0053F3E8 + 4 * t) - 1;   // esi 0x00413381 / 0x0041338c
        int f10 = FD(0x10);                    // edx 0x00413388
        int fr = last;                         // ecx 0x00413390
        if (f10 != -1) {                       // 0x0041338d / 0x00413392
            int tj = FD(0x14);                 // ebx 0x00413394
            fr = f10;                          // 0x00413398
            if ((int)GU8(0x00578804 + tj) >= last) {   // 0x0041339a..0x004133a4
                GU16(0x0053CAF0 + 2 * tj) = (u16)(FU(0x2c) & 0xFFFFBFFFu);   // 0x004133a6..0x004133af
                GU8(0x00578804 + tj) = 0xff;   // 0x004133b7
            }
            ebx = FD(0xc);                     // 0x004133be
        }
        int cell = CC(0x0043D6F0, t, fr, rot); // 0x004133c5 cellAt
        int k18 = FD(0x18);                    // 0x004133ca
        G32(0x005A9CC8) = cell;                // 0x004133d8
        CC(0x004628D0, FD(0xe0), FD(0xe4), FD(0xe4), 0x2a, 0x2a, cell, 4, k18);   // 0x004133ec addSprite
    } else {                                   // 0x004133f6
        int j = (m - 1) & 0xf;                 // 0x004133f6 / 0x004133fb
        CC(0x004628D0, FD(0xe0), FD(0xe4), 0, 0x20, 0xe, (int)(0x00568258 + 44 * j), 4, 0x31);   // 0x00413424 addSprite
    }
}

// 0x0041311c..0x004134fc: placed objects. n = (low byte of the tile word) & 0x1f; for tile type 0x15 or 0x16 with
// n != 0: screen point of the object from worldToScreen 0x0042fb90 into [F+0xe0] / [F+0xe4] (offset from the record
// byte [0x004c26c0 + 20 * (n - 1)]), draw target 0x0051b360, then buildSceneSprites 0x00463180 or, for n - 1 == 2,
// the P2_obj2 body; then (objectAt flag 0x40 clear, bit 1 of [0x00834170] set, n - 1 >= 6) the text buffer gets the
// prefix 0x004c69a8 / 0x004c69a0 selected by [0x005a8c34 + 4 * n] and the record name 0x004c26b0 + 20 * (n - 1).
static void P2_objects(u8* F) {
    int ti = FD(0x14);                         // eax 0x0041311c
    int n = GU8(0x0053CAF0 + 2 * ti) & 0x1f;   // ebp 0x00413120 / 0x0041312d / 0x00413132
    u8 ty = GU8(0x005722E8 + ti);              // al 0x00413127
    if (ty != 0x15 && ty != 0x16) return;      // 0x00413130..0x00413138 -> 0x004134fc
    if (n == 0) return;                        // 0x0041313e / 0x00413140 -> 0x004134fc
    int k = G32(0x005A8C34 + 4 * n);           // esi 0x00413146
    n -= 1;                                    // 0x0041314d
    int x, y;
    if (n == 7) k = 1;                         // 0x0041314e / 0x00413151 / 0x00413157
    int ebx = n * 20;                          // 0x0041315c / 0x00413167 (and 0x0041321b / 0x00413226)
    int dz = G8(0x004C26C0 + ebx);             // 0x0041316d / 0x00413232
    if (n == 7 || n < 6 || n == 5) {           // 0x00413209 / 0x0041320c (0x00413215 cannot be taken: n < 6 jumped first)
        FD(0xc) = ebx;                         // 0x00413178
        int off = (dz * 1024 - 0x400) / 2;     // 0x0041317c..0x0041318e
        y = (FD(0x30) << 10) + off + 0x200;    // 0x00413174 / 0x00413190 / 0x00413194
        x = (FD(0x20) << 10) + off + 0x200;    // 0x0041319b..0x004131a3
    } else {                                   // 0x0041321b
        FD(0xc) = ebx;                         // 0x0041323f
        int off = (int)((u32)(k + dz - 2) << 10) / 2;   // 0x0041323b..0x0041324d (and 0x0041325a..0x00413268)
        y = off + (FD(0x30) << 10) + 0x200;    // 0x00413249 / 0x0041324f / 0x00413252
        x = off + (FD(0x20) << 10) + 0x200;    // 0x0041325e / 0x0041326a / 0x0041326d
    }
    CC(0x0042FB90, x, y, FA(0xe0), FA(0xe4), 0);   // 0x004131ab worldToScreen (0x004131aa push x)
    int root = Root();                         // 0x004131b0
    k -= 1;                                    // 0x004131b9
    TC(0x004762D0, root, 0x0051B360, 0, 0, 0); // 0x004131c5 setDrawTarget
    if (n == 2) {                              // 0x004131ca / 0x004131cd -> 0x00413279
        P2_obj2(F, ebx);
    } else {                                   // 0x004131d3
        int hb = GU8(0x0056988C + FD(0x14));   // 0x004131d3..0x004131dd
        CC(0x00463180, n, k, FD(0xe0), FD(0xe4), FD(0x3c), FD(0x38), hb);   // 0x004131fc buildSceneSprites
    }
    // 0x0041342c
    int o = CC(0x0040DF80, FD(0x20), FD(0x30));   // 0x00413436 objectAt
    if (GU8((u32)(o << 4) + 0x0058BCBF) & 0x40) return;   // 0x0041343b..0x00413448
    if (!(GU8(0x00834170) & 2)) return;        // 0x0041344e / 0x00413455
    if (n < 6) return;                         // 0x0041345b / 0x0041345e
    if (n == 5) return;                        // 0x00413464 / 0x00413467 (cannot be taken after 0x0041345e)
    int sel = G32(0x005A8C38 + 4 * n);         // ebp 0x0041346d
    GU8(kBuf) = 0;                             // 0x00413474
    if (sel == 2) StrCpy(kBuf, 0x004C69A8);    // 0x0041347b / 0x0041347e; 0x00413480..0x004134a3
    if (sel == 3) StrCpy(kBuf, 0x004C69A0);    // 0x004134a5 / 0x004134a8; 0x004134aa..0x004134cc
    StrCat(kBuf, (u32)(ebx + 0x004C26B0));     // 0x004134ce..0x004134fa
}

// 0x0041235a..0x00413521: one tile (a, b).
static void P2_tile(u8* F, int& a, int& b) {
    if (CC(0x0040BF60, a, b) != 0) return;     // 0x0041235c tileBlocked / 0x00412366 -> 0x00413521
    FD(0x10) = -1;                             // 0x00412370
    int idx = b + a * 50;                      // ebx 0x0041236c / 0x00412378 / 0x0041237b
    FD(0x14) = idx;                            // 0x0041237e
    if (GU16(0x0053CAF0 + 2 * idx) & 0x4000)   // 0x00412382..0x00412393
        FD(0x10) = GU8(0x00578804 + idx);      // 0x00412395..0x0041239d
    if (CC(0x0042F940, a, b, FA(0x3c), FA(0x38)) == 0) return;   // 0x004123ad tileToScreen / 0x004123b7 -> 0x00413521
    P2_bars(F, idx);                           // 0x004123bd..0x0041246b
    u32 ecx = P2_label(F, a, idx);             // 0x0041246b..0x0041264f
    P2_paths(F, idx, ecx);                     // 0x0041264f..0x004129a4
    P2_scenery(F);                             // 0x004129a4..0x0041311c
    P2_objects(F);                             // 0x0041311c..0x004134fc
    // 0x004134fc: step the screen point back by one tile
    int g = G32(0x004C2840);                   // 0x004134fc
    int x3c = FD(0x3c);                        // 0x00413501
    a = FD(0x20);                              // ebp 0x00413505
    b = FD(0x30);                              // esi 0x00413509
    FD(0x3c) = x3c - g;                        // 0x0041350d / 0x00413513
    FD(0x38) = FD(0x38) - FD(0x40);            // 0x0041350f..0x0041351d
}

// Entry: mainLoop 0x0041234e..0x0041353d. No register is live at entry or exit.
void Ph_0041234e(u8* F) {
    int b = 0;                                 // esi 0x0041234e
    FD(0x30) = b;                              // 0x00412350
    do {
        int a = 0;                             // ebp 0x00412354
        FD(0x20) = a;                          // 0x00412356
        do {
            P2_tile(F, a, b);                  // 0x0041235a..0x0041351d
            ++a;                               // 0x00413521
            FD(0x20) = a;                      // 0x00413525
        } while (a < 0x32);                    // 0x00413522 / 0x00413529 -> 0x0041235a
        ++b;                                   // 0x0041352f
        FD(0x30) = b;                          // 0x00413533
    } while (b < 0x32);                        // 0x00413530 / 0x00413537 -> 0x00412354
}

}  // namespace
// c3fy phase P3: mainLoop 0x0040f5c0, range 0x0041353d..0x00415847 (2464 instructions).
// Transcribed from log/c3/alt/c3fy_norm.asm (the F-relative normalized listing = the truth).
// Entry state: no register live at 0x0041353d (brief). Falls through to 0x00415847 (P4); P4 writes
// esi there and ebx is dead (first use writes bl), so nothing is handed over -> void return.
//
// Machine model: the eight GP registers are C ints; the caller's stack scratch below F is a local
// array `stk` indexed by `sp` (grows downward: `push x` -> stk[--sp]=x). A cdecl callee reads its
// args as stk[sp+0..n-1] (stk[sp] = top = call-order arg0) and the caller cleans with `add esp,k`
// (sp += k/4); a thiscall callee pops its own stack args (sp += n-1). Modeling the stack at runtime
// (not at transcribe time) keeps argument resolution correct across every branch, including the
// sprite-id diamonds and the shared `row` argument of cellAt. Branches are `goto La_<addr>`.
// Frame slots stay at their F offsets via FD/FU/FW/FB. Globals via G32/GU32/G16/GU16/G8/GU8.
namespace {

void Ph_0041353d(u8* F) {
    int eax = 0, ebx = 0, ecx = 0, edx = 0, esi = 0, edi = 0, ebp = 0;
    int stk[400];
    int sp = 256;
    (void)eax; (void)ebx; (void)ecx; (void)edx; (void)esi; (void)edi; (void)ebp; (void)stk; (void)sp;

ebx = G32(0x5685f0); // 0x41353d  mov ebx, dword ptr [0x5685f0]
// 0x413543  cmp ebx, 0x12
if ((int)(ebx) > (int)(0x12)) goto La_413d5c; // 0x413546  jg 0x413d5c
eax = ebx; // 0x41354c  mov eax, ebx
eax = (eax) << (6); // 0x41354e  shl eax, 6
eax = (eax) + (ebx); // 0x413551  add eax, ebx
eax = (eax) << (3); // 0x413553  shl eax, 3
ecx = G32(eax + 0x575ab8); // 0x413556  mov ecx, dword ptr [eax + 0x575ab8]
// 0x41355c  test ecx, ecx
if (((ecx)) == 0) goto La_413d5c; // 0x41355e  je 0x413d5c
ecx = G32(eax + 0x575ac8); // 0x413564  mov ecx, dword ptr [eax + 0x575ac8]
// 0x41356a  test ecx, ecx
if (((ecx)) == 0) goto La_413d5c; // 0x41356c  je 0x413d5c
esi = (esi) ^ (esi); // 0x413572  xor esi, esi
edx = 2; // 0x413574  mov edx, 2
FD(0xc) = esi; // 0x413579  mov dword ptr [F+0xc], esi
FD(0x28) = edx; // 0x41357d  mov dword ptr [F+0x28], edx
goto La_41358b; // 0x413581  jmp 0x41358b
La_413583:;
esi = FD(0xc); // 0x413583  mov esi, dword ptr [F+0xc]
edx = FD(0x28); // 0x413587  mov edx, dword ptr [F+0x28]
La_41358b:;
ebp = (ebp) ^ (ebp); // 0x41358b  xor ebp, ebp
ecx = 0x40; // 0x41358d  mov ecx, 0x40
eax = (eax) ^ (eax); // 0x413592  xor eax, eax
edi = 0x582eb8; // 0x413594  mov edi, 0x582eb8
// 0x413599  cmp edx, 3
FD(0x10) = ebp; // 0x41359c  mov dword ptr [F+0x10], ebp
G32(0x541a10) = ebp; // 0x4135a0  mov dword ptr [0x541a10], ebp
{ for (int _i = 0; _i < 0x40; _i++) G32(edi + _i * 4) = eax; edi += 0x40 * 4; ecx = 0; } // 0x4135a6  rep stosd (memset 0x582eb8, 0x100 bytes = 0)
if ((u32)(edx) > (u32)(3)) goto La_4135ff; // 0x4135a8  ja 0x4135ff
// 0x4135aa  jmp dword ptr [edx*4 + 0x4216d4]  (switch table 0x4216d4, edx=0..3 after ja default above)
switch (edx) { case 0: goto La_4135b1; case 1: goto La_4135c5; case 2: goto La_4135d9; case 3: goto La_4135ed; }
La_4135b1:;
G8(0x582ed1) = 6; // 0x4135b1  mov byte ptr [0x582ed1], 6
FD(0xe8) = 0x80007ff0; // 0x4135b8  mov dword ptr [F+0xe8], 0x80007ff0
goto La_4135ff; // 0x4135c3  jmp 0x4135ff
La_4135c5:;
G8(0x582ed1) = 5; // 0x4135c5  mov byte ptr [0x582ed1], 5
FD(0xe8) = 0x800023e8; // 0x4135cc  mov dword ptr [F+0xe8], 0x800023e8
goto La_4135ff; // 0x4135d7  jmp 0x4135ff
La_4135d9:;
G8(0x582ed1) = 3; // 0x4135d9  mov byte ptr [0x582ed1], 3
FD(0xe8) = 0x8000211f; // 0x4135e0  mov dword ptr [F+0xe8], 0x8000211f
goto La_4135ff; // 0x4135eb  jmp 0x4135ff
La_4135ed:;
G8(0x582ed1) = 7; // 0x4135ed  mov byte ptr [0x582ed1], 7
FD(0xe8) = 0x80007fff; // 0x4135f4  mov dword ptr [F+0xe8], 0x80007fff
La_4135ff:;
// 0x4135ff  cmp edx, ebp
G8(0x582f72) = 1; // 0x413601  mov byte ptr [0x582f72], 1
eax = ebx; // 0x413608  mov eax, ebx
if ((edx) == (ebp)) goto La_41361c; // 0x41360a  je 0x41361c
eax = (eax) << (6); // 0x41360c  shl eax, 6
eax = (eax) + (ebx); // 0x41360f  add eax, ebx
eax = (eax) << (3); // 0x413611  shl eax, 3
ecx = G32(eax + 0x575ab8); // 0x413614  mov ecx, dword ptr [eax + 0x575ab8]
goto La_41362a; // 0x41361a  jmp 0x41362a
La_41361c:;
eax = (eax) << (6); // 0x41361c  shl eax, 6
eax = (eax) + (ebx); // 0x41361f  add eax, ebx
eax = (eax) << (3); // 0x413621  shl eax, 3
ecx = G32(eax + 0x575ac0); // 0x413624  mov ecx, dword ptr [eax + 0x575ac0]
La_41362a:;
ecx = (ecx) << (0xa); // 0x41362a  shl ecx, 0xa
ecx = (ecx) + (0x200); // 0x41362d  add ecx, 0x200
// 0x413633  cmp edx, ebp
G32(0x582f8c) = ecx; // 0x413635  mov dword ptr [0x582f8c], ecx
G32(0x582eb8) = ecx; // 0x41363b  mov dword ptr [0x582eb8], ecx
G32(0x582f7c) = ecx; // 0x413641  mov dword ptr [0x582f7c], ecx
if ((edx) == (ebp)) goto La_413651; // 0x413647  je 0x413651
eax = G32(eax + 0x575abc); // 0x413649  mov eax, dword ptr [eax + 0x575abc]
goto La_413657; // 0x41364f  jmp 0x413657
La_413651:;
eax = G32(eax + 0x575ac4); // 0x413651  mov eax, dword ptr [eax + 0x575ac4]
La_413657:;
eax = (eax) << (0xa); // 0x413657  shl eax, 0xa
eax = (eax) + (0x200); // 0x41365a  add eax, 0x200
G8(0x582ed9) = (ebx & 0xff); // 0x41365f  mov byte ptr [0x582ed9], bl
G32(0x582f90) = eax; // 0x413665  mov dword ptr [0x582f90], eax
G32(0x582ebc) = eax; // 0x41366a  mov dword ptr [0x582ebc], eax
G32(0x582f80) = eax; // 0x41366f  mov dword ptr [0x582f80], eax
ebp = (int)(esi*4 + 0x542e24); // 0x413674  lea ebp, [esi*4 + 0x542e24]
edi = (int)(esi*4 + 0x542dfc); // 0x41367b  lea edi, [esi*4 + 0x542dfc]
La_413682:;
// 0x413682  test dword ptr [0x59e7b8], 0x40000
if (((((G32(0x59e7b8)) & (0x40000)))) != 0) goto La_4136ef; // 0x41368c  jne 0x4136ef
stk[--sp] = (int)(0); // 0x41368e  push 0
stk[--sp] = (int)(0); // 0x413690  push 0
stk[--sp] = (int)(-1); // 0x413692  push -1
stk[--sp] = (int)(0); // 0x413694  push 0
stk[--sp] = (int)(0x9a); // 0x413696  push 0x9a
G32(0x5a9cc4) = 2; // 0x41369b  mov dword ptr [0x5a9cc4], 2
eax = CC(0x424120, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // prepareShot  0x4136a5
eax = G32(0x5a8828); // 0x4136aa  mov eax, dword ptr [0x5a8828]
ecx = G32(0x5a8830); // 0x4136af  mov ecx, dword ptr [0x5a8830]
ebx = FD(0xc); // 0x4136b5  mov ebx, dword ptr [F+0xc]
sp += 5; // 0x4136b9  add esp, 0x14  (caller stack cleanup)
eax = ((int)(eax)) >> (0xa); // 0x4136bc  sar eax, 0xa
ecx = ((int)(ecx)) >> (0xa); // 0x4136bf  sar ecx, 0xa
ebx = (ebx) + 1; // 0x4136c2  inc ebx
G32(edi) = eax; // 0x4136c3  mov dword ptr [edi], eax
G32(ebp) = ecx; // 0x4136c5  mov dword ptr [ebp], ecx
FD(0xc) = ebx; // 0x4136c8  mov dword ptr [F+0xc], ebx
ebx = G32(0x5685f0); // 0x4136cc  mov ebx, dword ptr [0x5685f0]
edi = (edi) + (4); // 0x4136d2  add edi, 4
G32(0x582f84) = eax; // 0x4136d5  mov dword ptr [0x582f84], eax
G32(0x582f88) = ecx; // 0x4136da  mov dword ptr [0x582f88], ecx
ebp = (ebp) + (4); // 0x4136e0  add ebp, 4
G32(0x5a9cc4) = 0; // 0x4136e3  mov dword ptr [0x5a9cc4], 0
goto La_41370e; // 0x4136ed  jmp 0x41370e
La_4136ef:;
edx = FD(0xc); // 0x4136ef  mov edx, dword ptr [F+0xc]
eax = G32(edi); // 0x4136f3  mov eax, dword ptr [edi]
ecx = G32(ebp); // 0x4136f5  mov ecx, dword ptr [ebp]
edx = (edx) + 1; // 0x4136f8  inc edx
edi = (edi) + (4); // 0x4136f9  add edi, 4
G32(0x582f84) = eax; // 0x4136fc  mov dword ptr [0x582f84], eax
G32(0x582f88) = ecx; // 0x413701  mov dword ptr [0x582f88], ecx
FD(0xc) = edx; // 0x413707  mov dword ptr [F+0xc], edx
ebp = (ebp) + (4); // 0x41370b  add ebp, 4
La_41370e:;
edx = (edx & ~0xffu) | ((GU8(0x582eda)) & 0xff); // 0x41370e  mov dl, byte ptr [0x582eda]
esi = FD(0x28); // 0x413714  mov esi, dword ptr [F+0x28]
// 0x413718  test dl, dl
if (((((edx & 0xff))&0xff)) != 0) goto La_413725; // 0x41371a  jne 0x413725
// 0x41371c  cmp esi, 2
if ((esi) == (2)) goto La_4138e2; // 0x41371f  je 0x4138e2
La_413725:;
edx = (int)(eax + eax*4); // 0x413725  lea edx, [eax + eax*4]
edx = (int)(edx + edx*4); // 0x413728  lea edx, [edx + edx*4]
edx = (int)(G8(ecx + edx*2 + 0x5722e8)); // 0x41372b  movsx edx, byte ptr [ecx + edx*2 + 0x5722e8]
edx = (int)(edx + edx*2); // 0x413733  lea edx, [edx + edx*2]
edx = (edx) << (4); // 0x413736  shl edx, 4
// 0x413739  cmp byte ptr [edx + 0x578372], 1
if ((int)(s8)(GU8(edx + 0x578372)) > (int)(s8)(1)) goto La_4138f4; // 0x413740  jg 0x4138f4
stk[--sp] = (int)(ecx); // 0x413746  push ecx
ecx = G32(0x582f7c); // 0x413747  mov ecx, dword ptr [0x582f7c]
stk[--sp] = (int)(eax); // 0x41374d  push eax
eax = G32(0x582f80); // 0x41374e  mov eax, dword ptr [0x582f80]
stk[--sp] = (int)(eax); // 0x413753  push eax
stk[--sp] = (int)(ecx); // 0x413754  push ecx
eax = CC(0x40c4b0, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3]); // tileDistance  0x413755
ecx = FD(0x10); // 0x41375a  mov ecx, dword ptr [F+0x10]
edx = FA(0x174); // 0x41375e  lea edx, [F+0x174]
ecx = (ecx) + (eax); // 0x413765  add ecx, eax
stk[--sp] = (int)(0); // 0x413767  push 0
FD(0x10) = ecx; // 0x413769  mov dword ptr [F+0x10], ecx
ecx = G32(0x582f80); // 0x41376d  mov ecx, dword ptr [0x582f80]
eax = FA(0x128); // 0x413773  lea eax, [F+0x128]
stk[--sp] = (int)(edx); // 0x41377a  push edx
edx = G32(0x582f7c); // 0x41377b  mov edx, dword ptr [0x582f7c]
stk[--sp] = (int)(eax); // 0x413781  push eax
stk[--sp] = (int)(ecx); // 0x413782  push ecx
stk[--sp] = (int)(edx); // 0x413783  push edx
eax = CC(0x42fb90, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // worldToScreen  0x413784
sp += 9; // 0x413789  add esp, 0x24  (caller stack cleanup)
// 0x41378c  test eax, eax
if (((eax)) == 0) goto La_413837; // 0x41378e  je 0x413837
edx = G32(0x582f88); // 0x413794  mov edx, dword ptr [0x582f88]
eax = FA(0x170); // 0x41379a  lea eax, [F+0x170]
ecx = FA(0x14c); // 0x4137a1  lea ecx, [F+0x14c]
stk[--sp] = (int)(eax); // 0x4137a8  push eax
eax = G32(0x582f84); // 0x4137a9  mov eax, dword ptr [0x582f84]
stk[--sp] = (int)(ecx); // 0x4137ae  push ecx
stk[--sp] = (int)(edx); // 0x4137af  push edx
stk[--sp] = (int)(eax); // 0x4137b0  push eax
eax = CC(0x42f940, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3]); // tileToScreen  0x4137b1
sp += 4; // 0x4137b6  add esp, 0x10  (caller stack cleanup)
// 0x4137b9  test eax, eax
if (((eax)) == 0) goto La_413837; // 0x4137bb  je 0x413837
ecx = FD(0x128); // 0x4137bd  mov ecx, dword ptr [F+0x128]
edx = FD(0x14c); // 0x4137c4  mov edx, dword ptr [F+0x14c]
eax = (int)(esi + esi*2 + -3); // 0x4137cb  lea eax, [esi + esi*2 - 3]
stk[--sp] = (int)(7); // 0x4137cf  push 7
ecx = (ecx) + (eax); // 0x4137d1  add ecx, eax
edx = (edx) + (eax); // 0x4137d3  add edx, eax
eax = FD(0x170); // 0x4137d5  mov eax, dword ptr [F+0x170]
stk[--sp] = (int)(2); // 0x4137dc  push 2
eax = (eax) + 1; // 0x4137de  inc eax
stk[--sp] = (int)(0x80000000); // 0x4137df  push 0x80000000
stk[--sp] = (int)(eax); // 0x4137e4  push eax
FD(0x14c) = edx; // 0x4137e5  mov dword ptr [F+0x14c], edx
stk[--sp] = (int)(edx); // 0x4137ec  push edx
edx = FD(0x174); // 0x4137ed  mov edx, dword ptr [F+0x174]
edx = (edx) + 1; // 0x4137f4  inc edx
FD(0x128) = ecx; // 0x4137f5  mov dword ptr [F+0x128], ecx
stk[--sp] = (int)(edx); // 0x4137fc  push edx
stk[--sp] = (int)(ecx); // 0x4137fd  push ecx
eax = CC(0x4493d0, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6]); // drawMapLine  0x4137fe
eax = FD(0xe8); // 0x413803  mov eax, dword ptr [F+0xe8]
ecx = FD(0x170); // 0x41380a  mov ecx, dword ptr [F+0x170]
edx = FD(0x14c); // 0x413811  mov edx, dword ptr [F+0x14c]
stk[--sp] = (int)(9); // 0x413818  push 9
stk[--sp] = (int)(2); // 0x41381a  push 2
stk[--sp] = (int)(eax); // 0x41381c  push eax
eax = FD(0x174); // 0x41381d  mov eax, dword ptr [F+0x174]
stk[--sp] = (int)(ecx); // 0x413824  push ecx
ecx = FD(0x128); // 0x413825  mov ecx, dword ptr [F+0x128]
stk[--sp] = (int)(edx); // 0x41382c  push edx
stk[--sp] = (int)(eax); // 0x41382d  push eax
stk[--sp] = (int)(ecx); // 0x41382e  push ecx
eax = CC(0x4493d0, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6]); // drawMapLine  0x41382f
sp += 14; // 0x413834  add esp, 0x38  (caller stack cleanup)
La_413837:;
eax = G32(0x582f84); // 0x413837  mov eax, dword ptr [0x582f84]
ecx = eax; // 0x41383c  mov ecx, eax
ecx = (ecx) << (0xa); // 0x41383e  shl ecx, 0xa
ecx = (ecx) + (0x200); // 0x413841  add ecx, 0x200
G32(0x582f8c) = ecx; // 0x413847  mov dword ptr [0x582f8c], ecx
G32(0x582eb8) = ecx; // 0x41384d  mov dword ptr [0x582eb8], ecx
G32(0x582f7c) = ecx; // 0x413853  mov dword ptr [0x582f7c], ecx
ecx = G32(0x582f88); // 0x413859  mov ecx, dword ptr [0x582f88]
edx = ecx; // 0x41385f  mov edx, ecx
edx = (edx) << (0xa); // 0x413861  shl edx, 0xa
edx = (edx) + (0x200); // 0x413864  add edx, 0x200
G32(0x582f90) = edx; // 0x41386a  mov dword ptr [0x582f90], edx
G32(0x582ebc) = edx; // 0x413870  mov dword ptr [0x582ebc], edx
G32(0x582f80) = edx; // 0x413876  mov dword ptr [0x582f80], edx
edx = G32(0x541a10); // 0x41387c  mov edx, dword ptr [0x541a10]
// 0x413882  test edx, edx
if (((edx)) != 0) goto La_4138a6; // 0x413884  jne 0x4138a6
edx = (int)(eax + eax*4); // 0x413886  lea edx, [eax + eax*4]
edx = (int)(edx + edx*4); // 0x413889  lea edx, [edx + edx*4]
// 0x41388c  cmp byte ptr [ecx + edx*2 + 0x5722e8], 1
if (((GU8(ecx + edx*2 + 0x5722e8))&0xff) != ((1)&0xff)) goto La_4138a6; // 0x413894  jne 0x4138a6
edx = (int)(G8(0x582eda)); // 0x413896  movsx edx, byte ptr [0x582eda]
edx = (edx) + (3); // 0x41389d  add edx, 3
G32(0x541a10) = edx; // 0x4138a0  mov dword ptr [0x541a10], edx
La_4138a6:;
edx = (edx & ~0xffu) | ((GU8(0x582eda)) & 0xff); // 0x4138a6  mov dl, byte ptr [0x582eda]
ebx = G32(0x5685f0); // 0x4138ac  mov ebx, dword ptr [0x5685f0]
edx = (edx & ~0xffu) | ((((edx & 0xff)) + 1) & 0xff); // 0x4138b2  inc dl
// 0x4138b4  cmp dl, 5
G8(0x582eda) = (edx & 0xff); // 0x4138b7  mov byte ptr [0x582eda], dl
if ((int)(s8)((edx & 0xff)) >= (int)(s8)(5)) goto La_4138f0; // 0x4138bd  jge 0x4138f0
esi = ebx; // 0x4138bf  mov esi, ebx
esi = (esi) << (6); // 0x4138c1  shl esi, 6
esi = (esi) + (ebx); // 0x4138c4  add esi, ebx
esi = (esi) << (3); // 0x4138c6  shl esi, 3
// 0x4138c9  cmp eax, dword ptr [esi + 0x575ac8]
if ((eax) != (G32(esi + 0x575ac8))) goto La_413682; // 0x4138cf  jne 0x413682
// 0x4138d5  cmp ecx, dword ptr [esi + 0x575acc]
if ((ecx) == (G32(esi + 0x575acc))) goto La_4138f0; // 0x4138db  je 0x4138f0
goto La_413682; // 0x4138dd  jmp 0x413682
La_4138e2:;
FD(0x16c) = eax; // 0x4138e2  mov dword ptr [F+0x16c], eax
FD(0x184) = ecx; // 0x4138e9  mov dword ptr [F+0x184], ecx
La_4138f0:;
esi = FD(0x28); // 0x4138f0  mov esi, dword ptr [F+0x28]
La_4138f4:;
esi = (esi) + 1; // 0x4138f4  inc esi
// 0x4138f5  cmp esi, 4
FD(0x28) = esi; // 0x4138f8  mov dword ptr [F+0x28], esi
if ((int)(esi) < (int)(4)) goto La_413583; // 0x4138fc  jl 0x413583
esi = G32(0x59e7b8); // 0x413902  mov esi, dword ptr [0x59e7b8]
esi = (esi) | (0x40000); // 0x413908  or esi, 0x40000
G32(0x59e7b8) = esi; // 0x41390e  mov dword ptr [0x59e7b8], esi
esi = ebx; // 0x413914  mov esi, ebx
esi = (esi) << (6); // 0x413916  shl esi, 6
esi = (esi) + (ebx); // 0x413919  add esi, ebx
esi = (esi) << (3); // 0x41391b  shl esi, 3
G16(esi + 0x575ab4) = 0; // 0x41391e  mov word ptr [esi + 0x575ab4], 0
edx = G32(esi + 0x575ac8); // 0x413927  mov edx, dword ptr [esi + 0x575ac8]
// 0x41392d  cmp eax, edx
if ((eax) != (edx)) goto La_41395b; // 0x41392f  jne 0x41395b
// 0x413931  cmp ecx, dword ptr [esi + 0x575acc]
if ((ecx) != (G32(esi + 0x575acc))) goto La_41395b; // 0x413937  jne 0x41395b
eax = FD(0x10); // 0x413939  mov eax, dword ptr [F+0x10]
eax = (int)(eax + eax*4); // 0x41393d  lea eax, [eax + eax*4]
ecx = (int)(eax + eax*4); // 0x413940  lea ecx, [eax + eax*4]
eax = 0x51eb851f; // 0x413943  mov eax, 0x51eb851f
{ long long _p = (long long)(int)eax * (long long)(int)(ecx); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x413948  imul ecx
edx = ((int)(edx)) >> (3); // 0x41394a  sar edx, 3
eax = edx; // 0x41394d  mov eax, edx
eax = ((u32)(eax)) >> (0x1f); // 0x41394f  shr eax, 0x1f
edx = (edx) + (eax); // 0x413952  add edx, eax
G16(esi + 0x575ab4) = (edx & 0xffff); // 0x413954  mov word ptr [esi + 0x575ab4], dx
La_41395b:;
ebp = FD(0x184); // 0x41395b  mov ebp, dword ptr [F+0x184]
esi = ebx; // 0x413962  mov esi, ebx
esi = (esi) << (6); // 0x413964  shl esi, 6
esi = (esi) + (ebx); // 0x413967  add esi, ebx
ebx = FD(0x16c); // 0x413969  mov ebx, dword ptr [F+0x16c]
esi = (esi) << (3); // 0x413970  shl esi, 3
ecx = ebp; // 0x413973  mov ecx, ebp
edx = ebx; // 0x413975  mov edx, ebx
ecx = (ecx) - (G32(esi + 0x575abc)); // 0x413977  sub ecx, dword ptr [esi + 0x575abc]
stk[--sp] = (int)(ecx); // 0x41397d  push ecx
ecx = G32(esi + 0x575ab8); // 0x41397e  mov ecx, dword ptr [esi + 0x575ab8]
edx = (edx) - (ecx); // 0x413984  sub edx, ecx
stk[--sp] = (int)(edx); // 0x413986  push edx
eax = CC(0x4672d0, stk[sp+0], stk[sp+1]); // angleFixed  0x413987
eax = ((int)(eax)) >> (0x1c); // 0x41398c  sar eax, 0x1c
eax = (eax) & (0xf); // 0x41398f  and eax, 0xf
stk[--sp] = (int)(0); // 0x413992  push 0
eax = (eax) + 1; // 0x413994  inc eax
eax = ((int)(eax)) >> (1); // 0x413995  sar eax, 1
eax = (eax & ~0xffu) | ((((eax & 0xff)) & (7)) & 0xff); // 0x413997  and al, 7
G8(esi + 0x575ab1) = (eax & 0xff); // 0x413999  mov byte ptr [esi + 0x575ab1], al
edi = G32(esi + 0x575cb0); // 0x41399f  mov edi, dword ptr [esi + 0x575cb0]
eax = G32(0x5685f0); // 0x4139a5  mov eax, dword ptr [0x5685f0]
edi = (edi) & (0xffffcfff); // 0x4139aa  and edi, 0xffffcfff
G32(esi + 0x575cb0) = edi; // 0x4139b0  mov dword ptr [esi + 0x575cb0], edi
esi = eax; // 0x4139b6  mov esi, eax
esi = (esi) << (6); // 0x4139b8  shl esi, 6
esi = (esi) + (eax); // 0x4139bb  add esi, eax
esi = (esi) << (3); // 0x4139bd  shl esi, 3
eax = G32(esi + 0x575abc); // 0x4139c0  mov eax, dword ptr [esi + 0x575abc]
ecx = G32(esi + 0x575ab8); // 0x4139c6  mov ecx, dword ptr [esi + 0x575ab8]
stk[--sp] = (int)(eax); // 0x4139cc  push eax
stk[--sp] = (int)(ecx); // 0x4139cd  push ecx
eax = CC(0x40c170, stk[sp+0], stk[sp+1], stk[sp+2]); // heightBlend  0x4139ce
edx = G32(esi + 0x575acc); // 0x4139d3  mov edx, dword ptr [esi + 0x575acc]
edi = eax; // 0x4139d9  mov edi, eax
eax = G32(esi + 0x575ac8); // 0x4139db  mov eax, dword ptr [esi + 0x575ac8]
stk[--sp] = (int)(0); // 0x4139e1  push 0
stk[--sp] = (int)(edx); // 0x4139e3  push edx
stk[--sp] = (int)(eax); // 0x4139e4  push eax
edi = (edi) + 1; // 0x4139e5  inc edi
eax = CC(0x40c170, stk[sp+0], stk[sp+1], stk[sp+2]); // heightBlend  0x4139e6
sp += 8; // 0x4139eb  add esp, 0x20  (caller stack cleanup)
{ int cmpA = eax; // 0x4139ee  cmp eax, edi: the jle at 0x4139f5 reads the flags of THIS compare (green height vs tee+1)
eax = G32(0x5685f0); // 0x4139f0  mov eax, dword ptr [0x5685f0] (moves never touch the flags)
if ((int)(cmpA) <= (int)(edi)) goto La_413a11; } // 0x4139f5  jle 0x413a11  (c3fy2 fix: was eax, the hole index)
ecx = eax; // 0x4139f7  mov ecx, eax
ecx = (ecx) << (6); // 0x4139f9  shl ecx, 6
ecx = (ecx) + (eax); // 0x4139fc  add ecx, eax
edx = G32(ecx*8 + 0x575cb0); // 0x4139fe  mov edx, dword ptr [ecx*8 + 0x575cb0]
ecx = (int)(ecx*8 + 0x575cb0); // 0x413a05  lea ecx, [ecx*8 + 0x575cb0]
edx = (edx & ~0xff00u) | ((((((edx>>8)&0xff)) | (0x10)) & 0xff)<<8); // 0x413a0c  or dh, 0x10
G32(ecx) = edx; // 0x413a0f  mov dword ptr [ecx], edx
La_413a11:;
esi = eax; // 0x413a11  mov esi, eax
edx = eax; // 0x413a13  mov edx, eax
esi = (esi) << (6); // 0x413a15  shl esi, 6
esi = (esi) + (eax); // 0x413a18  add esi, eax
stk[--sp] = (int)(0); // 0x413a1a  push 0
edx = (edx) << (6); // 0x413a1c  shl edx, 6
esi = (esi) << (3); // 0x413a1f  shl esi, 3
edx = (edx) + (eax); // 0x413a22  add edx, eax
eax = G32(esi + 0x575abc); // 0x413a24  mov eax, dword ptr [esi + 0x575abc]
ecx = G32(esi + 0x575ab8); // 0x413a2a  mov ecx, dword ptr [esi + 0x575ab8]
edx = (edx) << (3); // 0x413a30  shl edx, 3
stk[--sp] = (int)(eax); // 0x413a33  push eax
stk[--sp] = (int)(ecx); // 0x413a34  push ecx
FD(0x2c) = edx; // 0x413a35  mov dword ptr [F+0x2c], edx
eax = CC(0x40c170, stk[sp+0], stk[sp+1], stk[sp+2]); // heightBlend  0x413a39
edx = G32(esi + 0x575acc); // 0x413a3e  mov edx, dword ptr [esi + 0x575acc]
edi = eax; // 0x413a44  mov edi, eax
eax = G32(esi + 0x575ac8); // 0x413a46  mov eax, dword ptr [esi + 0x575ac8]
stk[--sp] = (int)(0); // 0x413a4c  push 0
stk[--sp] = (int)(edx); // 0x413a4e  push edx
stk[--sp] = (int)(eax); // 0x413a4f  push eax
edi = (edi) - 1; // 0x413a50  dec edi
eax = CC(0x40c170, stk[sp+0], stk[sp+1], stk[sp+2]); // heightBlend  0x413a51
sp += 6; // 0x413a56  add esp, 0x18  (caller stack cleanup)
{ int cmpA = eax; // 0x413a59  cmp eax, edi: the jge at 0x413a60 reads the flags of THIS compare (green height vs tee-1)
eax = G32(0x5685f0); // 0x413a5b  mov eax, dword ptr [0x5685f0] (moves never touch the flags)
#if defined(C3FY_NEG)
if (c3fyNegMode() == 2) cmpA = eax;   // negctl mode 2: the U-0008 defect (compares the hole index)
#endif
if ((int)(cmpA) >= (int)(edi)) goto La_413a7c; } // 0x413a60  jge 0x413a7c  (c3fy2 fix U-0008: was eax, the hole index)
ecx = eax; // 0x413a62  mov ecx, eax
ecx = (ecx) << (6); // 0x413a64  shl ecx, 6
ecx = (ecx) + (eax); // 0x413a67  add ecx, eax
edx = G32(ecx*8 + 0x575cb0); // 0x413a69  mov edx, dword ptr [ecx*8 + 0x575cb0]
ecx = (int)(ecx*8 + 0x575cb0); // 0x413a70  lea ecx, [ecx*8 + 0x575cb0]
edx = (edx & ~0xff00u) | ((((((edx>>8)&0xff)) | (0x20)) & 0xff)<<8); // 0x413a77  or dh, 0x20
G32(ecx) = edx; // 0x413a7a  mov dword ptr [ecx], edx
La_413a7c:;
// 0x413a7c  cmp dword ptr [F+0x10], 0xfa
if ((int)(FD(0x10)) >= (int)(0xfa)) goto La_413aaa; // 0x413a84  jge 0x413aaa
ecx = eax; // 0x413a86  mov ecx, eax
ecx = (ecx) << (6); // 0x413a88  shl ecx, 6
ecx = (ecx) + (eax); // 0x413a8b  add ecx, eax
ecx = (ecx) << (3); // 0x413a8d  shl ecx, 3
ebx = G32(ecx + 0x575ab8); // 0x413a90  mov ebx, dword ptr [ecx + 0x575ab8]
ebp = G32(ecx + 0x575abc); // 0x413a96  mov ebp, dword ptr [ecx + 0x575abc]
FD(0x16c) = ebx; // 0x413a9c  mov dword ptr [F+0x16c], ebx
FD(0x184) = ebp; // 0x413aa3  mov dword ptr [F+0x184], ebp
La_413aaa:;
edx = eax; // 0x413aaa  mov edx, eax
esi = eax; // 0x413aac  mov esi, eax
edx = (edx) << (6); // 0x413aae  shl edx, 6
esi = (esi) << (6); // 0x413ab1  shl esi, 6
edx = (edx) + (eax); // 0x413ab4  add edx, eax
esi = (esi) + (eax); // 0x413ab6  add esi, eax
edx = (edx) << (3); // 0x413ab8  shl edx, 3
esi = (esi) << (3); // 0x413abb  shl esi, 3
FD(0x2c) = edx; // 0x413abe  mov dword ptr [F+0x2c], edx
eax = G32(esi + 0x575acc); // 0x413ac2  mov eax, dword ptr [esi + 0x575acc]
edx = G32(esi + 0x575abc); // 0x413ac8  mov edx, dword ptr [esi + 0x575abc]
ecx = G32(esi + 0x575ac8); // 0x413ace  mov ecx, dword ptr [esi + 0x575ac8]
eax = (eax) - (edx); // 0x413ad4  sub eax, edx
stk[--sp] = (int)(eax); // 0x413ad6  push eax
eax = G32(esi + 0x575ab8); // 0x413ad7  mov eax, dword ptr [esi + 0x575ab8]
ecx = (ecx) - (eax); // 0x413add  sub ecx, eax
stk[--sp] = (int)(ecx); // 0x413adf  push ecx
eax = CC(0x4672d0, stk[sp+0], stk[sp+1]); // angleFixed  0x413ae0
edx = G32(esi + 0x575acc); // 0x413ae5  mov edx, dword ptr [esi + 0x575acc]
edi = eax; // 0x413aeb  mov edi, eax
eax = G32(esi + 0x575ac8); // 0x413aed  mov eax, dword ptr [esi + 0x575ac8]
edx = (edx) - (ebp); // 0x413af3  sub edx, ebp
eax = (eax) - (ebx); // 0x413af5  sub eax, ebx
stk[--sp] = (int)(edx); // 0x413af7  push edx
stk[--sp] = (int)(eax); // 0x413af8  push eax
eax = CC(0x4672d0, stk[sp+0], stk[sp+1]); // angleFixed  0x413af9
ecx = G32(0x5685f0); // 0x413afe  mov ecx, dword ptr [0x5685f0]
edi = (edi) - (eax); // 0x413b04  sub edi, eax
eax = ecx; // 0x413b06  mov eax, ecx
sp += 4; // 0x413b08  add esp, 0x10  (caller stack cleanup)
eax = (eax) << (6); // 0x413b0b  shl eax, 6
eax = (eax) + (ecx); // 0x413b0e  add eax, ecx
eax = (eax) << (3); // 0x413b10  shl eax, 3
// 0x413b13  cmp dword ptr [eax + 0x575ac8], ebx
if ((G32(eax + 0x575ac8)) != (ebx)) goto La_413b25; // 0x413b19  jne 0x413b25
// 0x413b1b  cmp dword ptr [eax + 0x575acc], ebp
if ((G32(eax + 0x575acc)) != (ebp)) goto La_413b25; // 0x413b21  jne 0x413b25
edi = (edi) ^ (edi); // 0x413b23  xor edi, edi
La_413b25:;
esi = G32(eax + 0x575cb0); // 0x413b25  mov esi, dword ptr [eax + 0x575cb0]
esi = (esi) & (0xffffff9f); // 0x413b2b  and esi, 0xffffff9f
// 0x413b2e  cmp edi, 0x71c71c6
G32(eax + 0x575cb0) = esi; // 0x413b34  mov dword ptr [eax + 0x575cb0], esi
ecx = esi; // 0x413b3a  mov ecx, esi
if ((int)(edi) <= (int)(0x71c71c6)) goto La_413b47; // 0x413b3c  jle 0x413b47
ecx = (ecx) | (0x20); // 0x413b3e  or ecx, 0x20
G32(eax + 0x575cb0) = ecx; // 0x413b41  mov dword ptr [eax + 0x575cb0], ecx
La_413b47:;
// 0x413b47  cmp edi, 0xf8e38e3a
if ((int)(edi) >= (int)(0xf8e38e3a)) goto La_413b56; // 0x413b4d  jge 0x413b56
G32(eax + 0x575cb0) = (G32(eax + 0x575cb0)) | (0x40); // 0x413b4f  or dword ptr [eax + 0x575cb0], 0x40
La_413b56:;
ecx = G32(eax + 0x575acc); // 0x413b56  mov ecx, dword ptr [eax + 0x575acc]
esi = G32(eax + 0x575ac8); // 0x413b5c  mov esi, dword ptr [eax + 0x575ac8]
ebp = (ebp) - (ecx); // 0x413b62  sub ebp, ecx
ebx = (ebx) - (esi); // 0x413b64  sub ebx, esi
stk[--sp] = (int)(ebp); // 0x413b66  push ebp
stk[--sp] = (int)(ebx); // 0x413b67  push ebx
eax = CC(0x4672d0, stk[sp+0], stk[sp+1]); // angleFixed  0x413b68
FD(0x30) = eax; // 0x413b6d  mov dword ptr [F+0x30], eax
eax = G32(0x5685f0); // 0x413b71  mov eax, dword ptr [0x5685f0]
sp += 2; // 0x413b76  add esp, 8  (caller stack cleanup)
FD(0x20) = 0xaaaaa9a; // 0x413b79  mov dword ptr [F+0x20], 0xaaaaa9a
ecx = (int)(eax + eax*2); // 0x413b81  lea ecx, [eax + eax*2]
eax = (eax) | (0xffffffff); // 0x413b84  or eax, 0xffffffff
edi = (int)(ecx*8 + 0x59ae80); // 0x413b87  lea edi, [ecx*8 + 0x59ae80]
ecx = 6; // 0x413b8e  mov ecx, 6
{ for (int _i = 0; _i < 6; _i++) G32(edi + _i * 4) = eax; edi += 6 * 4; ecx = 0; } // 0x413b93  rep stosd (6 dwords of eax=-1 at [ecx*8+0x59ae80])
eax = 0x19; // 0x413b95  mov eax, 0x19
FD(0x18) = eax; // 0x413b9a  mov dword ptr [F+0x18], eax
La_413b9e:;
ecx = eax; // 0x413b9e  mov ecx, eax
eax = FD(0x20); // 0x413ba0  mov eax, dword ptr [F+0x20]
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (1)) & 0xff); // 0x413ba4  and cl, 1
FD(0xc) = 0; // 0x413ba7  mov dword ptr [F+0xc], 0
ecx = (ecx & ~0xffu) | ((-((ecx & 0xff))) & 0xff); // 0x413baf  neg cl
edx = eax >> 31; // 0x413bb1  cdq
ecx = -(int)((((ecx & 0xff)) != 0)); // 0x413bb2  sbb ecx, ecx
edx = (edx) & (7); // 0x413bb4  and edx, 7
ecx = (ecx) & (2); // 0x413bb7  and ecx, 2
eax = (eax) + (edx); // 0x413bba  add eax, edx
ecx = (ecx) - 1; // 0x413bbc  dec ecx
FD(0x28) = 0x19000; // 0x413bbd  mov dword ptr [F+0x28], 0x19000
eax = ((int)(eax)) >> (3); // 0x413bc5  sar eax, 3
ecx = (ecx) * (eax); // 0x413bc8  imul ecx, eax
eax = FD(0x30); // 0x413bcb  mov eax, dword ptr [F+0x30]
ecx = (ecx) + (eax); // 0x413bcf  add ecx, eax
eax = 0x96; // 0x413bd1  mov eax, 0x96
edi = ecx; // 0x413bd6  mov edi, ecx
FD(0x2c) = eax; // 0x413bd8  mov dword ptr [F+0x2c], eax
FD(0x14) = edi; // 0x413bdc  mov dword ptr [F+0x14], edi
goto La_413be6; // 0x413be0  jmp 0x413be6
La_413be2:;
edi = FD(0x14); // 0x413be2  mov edi, dword ptr [F+0x14]
La_413be6:;
// 0x413be6  cmp eax, dword ptr [F+0x10]
if ((int)(eax) >= (int)(FD(0x10))) goto La_413d10; // 0x413bea  jge 0x413d10
eax = 0x51eb851f; // 0x413bf0  mov eax, 0x51eb851f
{ long long _p = (long long)(int)eax * (long long)(int)(FD(0x28)); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x413bf5  imul dword ptr [F+0x28]
edx = ((int)(edx)) >> (3); // 0x413bf9  sar edx, 3
eax = edx; // 0x413bfc  mov eax, edx
eax = ((u32)(eax)) >> (0x1f); // 0x413bfe  shr eax, 0x1f
edx = (edx) + (eax); // 0x413c01  add edx, eax
esi = edx; // 0x413c03  mov esi, edx
stk[--sp] = (int)(esi); // 0x413c05  push esi
stk[--sp] = (int)(edi); // 0x413c06  push edi
eax = CC(0x467270, stk[sp+0], stk[sp+1]); // foldRange  0x413c07
ecx = G32(0x5685f0); // 0x413c0c  mov ecx, dword ptr [0x5685f0]
stk[--sp] = (int)(esi); // 0x413c12  push esi
edx = ecx; // 0x413c13  mov edx, ecx
stk[--sp] = (int)(edi); // 0x413c15  push edi
edx = (edx) << (6); // 0x413c16  shl edx, 6
edx = (edx) + (ecx); // 0x413c19  add edx, ecx
ecx = G32(edx*8 + 0x575ac8); // 0x413c1b  mov ecx, dword ptr [edx*8 + 0x575ac8]
ecx = (ecx) << (0xa); // 0x413c22  shl ecx, 0xa
ebx = (int)(eax + ecx + 0x200); // 0x413c25  lea ebx, [eax + ecx + 0x200]
eax = CC(0x4672b0, stk[sp+0], stk[sp+1]); // absDiff  0x413c2c
edi = G32(0x5685f0); // 0x413c31  mov edi, dword ptr [0x5685f0]
sp += 4; // 0x413c37  add esp, 0x10  (caller stack cleanup)
edx = edi; // 0x413c3a  mov edx, edi
edx = (edx) << (6); // 0x413c3c  shl edx, 6
edx = (edx) + (edi); // 0x413c3f  add edx, edi
ecx = G32(edx*8 + 0x575acc); // 0x413c41  mov ecx, dword ptr [edx*8 + 0x575acc]
ecx = (ecx) << (0xa); // 0x413c48  shl ecx, 0xa
ecx = (ecx) - (eax); // 0x413c4b  sub ecx, eax
eax = ebx; // 0x413c4d  mov eax, ebx
eax = ((int)(eax)) >> (0xa); // 0x413c4f  sar eax, 0xa
ecx = (ecx) + (0x200); // 0x413c52  add ecx, 0x200
esi = (int)(eax + eax*4); // 0x413c58  lea esi, [eax + eax*4]
edx = ecx; // 0x413c5b  mov edx, ecx
edx = ((int)(edx)) >> (0xa); // 0x413c5d  sar edx, 0xa
esi = (int)(esi + esi*4); // 0x413c60  lea esi, [esi + esi*4]
// 0x413c63  cmp byte ptr [edx + esi*2 + 0x5722e8], 2
if (((GU8(edx + esi*2 + 0x5722e8))&0xff) != ((2)&0xff)) goto La_413d10; // 0x413c6b  jne 0x413d10
esi = ebx; // 0x413c71  mov esi, ebx
esi = (esi) & (0x800003ff); // 0x413c73  and esi, 0x800003ff
if (((int)(esi)) >= 0) goto La_413c83; // 0x413c79  jns 0x413c83
esi = (esi) - 1; // 0x413c7b  dec esi
esi = (esi) | (0xfffffc00); // 0x413c7c  or esi, 0xfffffc00
esi = (esi) + 1; // 0x413c82  inc esi
La_413c83:;
ebp = ecx; // 0x413c83  mov ebp, ecx
esi = ((int)(esi)) >> (4); // 0x413c85  sar esi, 4
ebp = (ebp) & (0x800003ff); // 0x413c88  and ebp, 0x800003ff
if (((int)(ebp)) >= 0) goto La_413c98; // 0x413c8e  jns 0x413c98
ebp = (ebp) - 1; // 0x413c90  dec ebp
ebp = (ebp) | (0xfffffc00); // 0x413c91  or ebp, 0xfffffc00
ebp = (ebp) + 1; // 0x413c97  inc ebp
La_413c98:;
ebp = ((int)(ebp)) >> (4); // 0x413c98  sar ebp, 4
// 0x413c9b  cmp esi, 0x18
FD(0x34) = ebp; // 0x413c9e  mov dword ptr [F+0x34], ebp
if ((int)(esi) >= (int)(0x18)) goto La_413cb9; // 0x413ca2  jge 0x413cb9
ebp = (int)(eax + eax*4); // 0x413ca4  lea ebp, [eax + eax*4]
ebp = (int)(ebp + ebp*4); // 0x413ca7  lea ebp, [ebp + ebp*4]
// 0x413cab  cmp byte ptr [edx + ebp*2 + 0x5722b6], 2
if (((GU8(edx + ebp*2 + 0x5722b6))&0xff) != ((2)&0xff)) goto La_413d10; // 0x413cb3  jne 0x413d10
ebp = FD(0x34); // 0x413cb5  mov ebp, dword ptr [F+0x34]
La_413cb9:;
// 0x413cb9  cmp esi, 0x30
if ((int)(esi) < (int)(0x30)) goto La_413cce; // 0x413cbc  jl 0x413cce
esi = (int)(eax + eax*4); // 0x413cbe  lea esi, [eax + eax*4]
esi = (int)(esi + esi*4); // 0x413cc1  lea esi, [esi + esi*4]
// 0x413cc4  cmp byte ptr [edx + esi*2 + 0x57231a], 2
if (((GU8(edx + esi*2 + 0x57231a))&0xff) != ((2)&0xff)) goto La_413d10; // 0x413ccc  jne 0x413d10
La_413cce:;
// 0x413cce  cmp ebp, 0x18
if ((int)(ebp) >= (int)(0x18)) goto La_413ce3; // 0x413cd1  jge 0x413ce3
esi = (int)(eax + eax*4); // 0x413cd3  lea esi, [eax + eax*4]
esi = (int)(esi + esi*4); // 0x413cd6  lea esi, [esi + esi*4]
// 0x413cd9  cmp byte ptr [edx + esi*2 + 0x5722e7], 2
if (((GU8(edx + esi*2 + 0x5722e7))&0xff) != ((2)&0xff)) goto La_413d10; // 0x413ce1  jne 0x413d10
La_413ce3:;
// 0x413ce3  cmp ebp, 0x30
if ((int)(ebp) < (int)(0x30)) goto La_413cf8; // 0x413ce6  jl 0x413cf8
eax = (int)(eax + eax*4); // 0x413ce8  lea eax, [eax + eax*4]
eax = (int)(eax + eax*4); // 0x413ceb  lea eax, [eax + eax*4]
// 0x413cee  cmp byte ptr [edx + eax*2 + 0x5722e9], 2
if (((GU8(edx + eax*2 + 0x5722e9))&0xff) != ((2)&0xff)) goto La_413d10; // 0x413cf6  jne 0x413d10
La_413cf8:;
edx = FD(0xc); // 0x413cf8  mov edx, dword ptr [F+0xc]
eax = (int)(edx + edi*2); // 0x413cfc  lea eax, [edx + edi*2]
edi = (edi) + (eax); // 0x413cff  add edi, eax
edi = (edi) << (3); // 0x413d01  shl edi, 3
G32(edi + 0x59ae80) = ebx; // 0x413d04  mov dword ptr [edi + 0x59ae80], ebx
G32(edi + 0x59ae84) = ecx; // 0x413d0a  mov dword ptr [edi + 0x59ae84], ecx
La_413d10:;
eax = FD(0x2c); // 0x413d10  mov eax, dword ptr [F+0x2c]
edi = FD(0xc); // 0x413d14  mov edi, dword ptr [F+0xc]
edx = FD(0x28); // 0x413d18  mov edx, dword ptr [F+0x28]
eax = (eax) + (0x32); // 0x413d1c  add eax, 0x32
edi = (edi) + 1; // 0x413d1f  inc edi
edx = (edx) + (0xc800); // 0x413d20  add edx, 0xc800
// 0x413d26  cmp eax, 0x12c
FD(0xc) = edi; // 0x413d2b  mov dword ptr [F+0xc], edi
FD(0x2c) = eax; // 0x413d2f  mov dword ptr [F+0x2c], eax
FD(0x28) = edx; // 0x413d33  mov dword ptr [F+0x28], edx
if ((int)(eax) < (int)(0x12c)) goto La_413be2; // 0x413d37  jl 0x413be2
eax = FD(0x18); // 0x413d3d  mov eax, dword ptr [F+0x18]
edx = FD(0x20); // 0x413d41  mov edx, dword ptr [F+0x20]
eax = (eax) - 1; // 0x413d45  dec eax
edx = (edx) - (0xaaaaaaa); // 0x413d46  sub edx, 0xaaaaaaa
// 0x413d4c  test eax, eax
FD(0x18) = eax; // 0x413d4e  mov dword ptr [F+0x18], eax
FD(0x20) = edx; // 0x413d52  mov dword ptr [F+0x20], edx
if (((int)(eax)) > 0) goto La_413b9e; // 0x413d56  jg 0x413b9e
La_413d5c:;
eax = CC(0x462800); // snapshotTables  0x413d5c
eax = CC(0x430360); // drawWorldObjects  0x413d61
esi = 0x59ae98; // 0x413d66  mov esi, 0x59ae98
La_413d6b:;
edi = 3; // 0x413d6b  mov edi, 3
La_413d70:;
eax = G32(esi); // 0x413d70  mov eax, dword ptr [esi]
// 0x413d72  test eax, eax
if (((int)(eax)) <= 0) goto La_413d95; // 0x413d74  jle 0x413d95
ecx = FA(0x278); // 0x413d76  lea ecx, [F+0x278]
stk[--sp] = (int)(0); // 0x413d7d  push 0
stk[--sp] = (int)(ecx); // 0x413d7f  push ecx
ecx = G32(esi + 4); // 0x413d80  mov ecx, dword ptr [esi + 4]
edx = FA(0x27c); // 0x413d83  lea edx, [F+0x27c]
stk[--sp] = (int)(edx); // 0x413d8a  push edx
stk[--sp] = (int)(ecx); // 0x413d8b  push ecx
stk[--sp] = (int)(eax); // 0x413d8c  push eax
eax = CC(0x42fb90, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // worldToScreen  0x413d8d
sp += 5; // 0x413d92  add esp, 0x14  (caller stack cleanup)
La_413d95:;
esi = (esi) + (8); // 0x413d95  add esi, 8
edi = (edi) - 1; // 0x413d98  dec edi
if (((edi)) != 0) goto La_413d70; // 0x413d99  jne 0x413d70
// 0x413d9b  cmp esi, 0x59b048
if ((int)(esi) < (int)(0x59b048)) goto La_413d6b; // 0x413da1  jl 0x413d6b
esi = (esi) ^ (esi); // 0x413da3  xor esi, esi
La_413da5:;
edi = (edi) ^ (edi); // 0x413da5  xor edi, edi
La_413da7:;
eax = (int)(edi + edi*4); // 0x413da7  lea eax, [edi + edi*4]
ebx = (ebx) | (0xffffffff); // 0x413daa  or ebx, 0xffffffff
FD(0x30) = ebx; // 0x413dad  mov dword ptr [F+0x30], ebx
edx = (int)(eax + eax*4); // 0x413db1  lea edx, [eax + eax*4]
ebp = (int)(esi + edx*2); // 0x413db4  lea ebp, [esi + edx*2]
FD(0x14) = ebp; // 0x413db7  mov dword ptr [F+0x14], ebp
eax = (eax & ~0xffffu) | ((GU16(ebp*2 + 0x53caf0)) & 0xffff); // 0x413dbb  mov ax, word ptr [ebp*2 + 0x53caf0]
ecx = eax; // 0x413dc3  mov ecx, eax
ecx = (ecx) & (0x4000); // 0x413dc5  and ecx, 0x4000
// 0x413dcb  test cx, cx
if (((((ecx & 0xffff))&0xffff)) == 0) goto La_413e0a; // 0x413dce  je 0x413e0a
ebx = (ebx) ^ (ebx); // 0x413dd0  xor ebx, ebx
eax = (eax) & (0x800); // 0x413dd2  and eax, 0x800
ebx = (ebx & ~0xffu) | ((GU8(ebp + 0x578804)) & 0xff); // 0x413dd7  mov bl, byte ptr [ebp + 0x578804]
// 0x413ddd  test ax, ax
FD(0x30) = ebx; // 0x413de0  mov dword ptr [F+0x30], ebx
if (((((eax & 0xffff))&0xffff)) == 0) goto La_413df7; // 0x413de4  je 0x413df7
stk[--sp] = (int)(8); // 0x413de6  push 8
ecx = 0x822d9c; // 0x413de8  mov ecx, 0x822d9c
eax = Rnd(stk[sp]); sp += 1; // 0x413ded  call 0x45c1e0
// 0x413df2  test ax, ax
if (((((eax & 0xffff))&0xffff)) != 0) goto La_413dfd; // 0x413df5  jne 0x413dfd
La_413df7:;
G8(ebp + 0x578804) = (GU8(ebp + 0x578804)) + 1; // 0x413df7  inc byte ptr [ebp + 0x578804]
La_413dfd:;
// 0x413dfd  cmp ebx, 0x18
if ((int)(ebx) <= (int)(0x18)) goto La_413e0a; // 0x413e00  jle 0x413e0a
G8(ebp*2 + 0x53caf1) = (GU8(ebp*2 + 0x53caf1)) & (0xbf); // 0x413e02  and byte ptr [ebp*2 + 0x53caf1], 0xbf
La_413e0a:;
edx = FA(0x6c); // 0x413e0a  lea edx, [F+0x6c]
eax = FA(0x7c); // 0x413e0e  lea eax, [F+0x7c]
stk[--sp] = (int)(edx); // 0x413e15  push edx
stk[--sp] = (int)(eax); // 0x413e16  push eax
stk[--sp] = (int)(esi); // 0x413e17  push esi
stk[--sp] = (int)(edi); // 0x413e18  push edi
eax = CC(0x42f940, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3]); // tileToScreen  0x413e19
sp += 4; // 0x413e1e  add esp, 0x10  (caller stack cleanup)
// 0x413e21  test eax, eax
if (((eax)) != 0) goto La_413e56; // 0x413e23  jne 0x413e56
// 0x413e25  test ebx, ebx
if (((int)(ebx)) <= 0) goto La_414931; // 0x413e27  jle 0x414931
eax = (eax & ~0xffffu) | ((GU16(ebp*2 + 0x53caf0)) & 0xffff); // 0x413e2d  mov ax, word ptr [ebp*2 + 0x53caf0]
ecx = eax; // 0x413e35  mov ecx, eax
ecx = (ecx) & (0x800); // 0x413e37  and ecx, 0x800
// 0x413e3d  test cx, cx
if (((((ecx & 0xffff))&0xffff)) != 0) goto La_414931; // 0x413e40  jne 0x414931
eax = (eax & ~0xff00u) | ((((((eax>>8)&0xff)) & (0xbf)) & 0xff)<<8); // 0x413e46  and ah, 0xbf
G16(ebp*2 + 0x53caf0) = (eax & 0xffff); // 0x413e49  mov word ptr [ebp*2 + 0x53caf0], ax
goto La_414931; // 0x413e51  jmp 0x414931
La_413e56:;
eax = FD(0x6c); // 0x413e56  mov eax, dword ptr [F+0x6c]
edx = FD(0x40); // 0x413e5a  mov edx, dword ptr [F+0x40]
eax = (eax) - (edx); // 0x413e5e  sub eax, edx
FD(0xc) = eax; // 0x413e60  mov dword ptr [F+0xc], eax
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5722e8)) & 0xff); // 0x413e64  mov al, byte ptr [ebp + 0x5722e8]
// 0x413e6a  cmp al, 0x14
if ((((eax & 0xff))&0xff) == ((0x14)&0xff)) goto La_414931; // 0x413e6c  je 0x414931
ebx = (ebx & ~0xffu) | ((GU8(ebp*2 + 0x53caf0)) & 0xff); // 0x413e72  mov bl, byte ptr [ebp*2 + 0x53caf0]
ebx = (ebx) & (0x1f); // 0x413e79  and ebx, 0x1f
// 0x413e7c  cmp al, 0x15
if ((((eax & 0xff))&0xff) == ((0x15)&0xff)) goto La_413e88; // 0x413e7e  je 0x413e88
// 0x413e80  cmp al, 0x16
if ((((eax & 0xff))&0xff) != ((0x16)&0xff)) goto La_413f46; // 0x413e82  jne 0x413f46
La_413e88:;
// 0x413e88  test ebx, ebx
if (((ebx)) == 0) goto La_413f46; // 0x413e8a  je 0x413f46
ebx = (ebx) - 1; // 0x413e90  dec ebx
// 0x413e91  cmp ebx, 6
if ((int)(ebx) < (int)(6)) goto La_413ebb; // 0x413e94  jl 0x413ebb
// 0x413e96  cmp ebx, 7
if ((ebx) == (7)) goto La_413ebb; // 0x413e99  je 0x413ebb
// 0x413e9b  cmp ebx, 5
if ((ebx) == (5)) goto La_413ebb; // 0x413e9e  je 0x413ebb
ecx = G32(ebx*4 + 0x5a8c38); // 0x413ea0  mov ecx, dword ptr [ebx*4 + 0x5a8c38]
edx = (int)(ebx + ebx*4); // 0x413ea7  lea edx, [ebx + ebx*4]
eax = (int)(G8(edx*4 + 0x4c26c0)); // 0x413eaa  movsx eax, byte ptr [edx*4 + 0x4c26c0]
eax = (int)(eax + ecx + -2); // 0x413eb2  lea eax, [eax + ecx - 2]
eax = (eax) << (0xa); // 0x413eb6  shl eax, 0xa
goto La_413ece; // 0x413eb9  jmp 0x413ece
La_413ebb:;
ecx = (int)(ebx + ebx*4); // 0x413ebb  lea ecx, [ebx + ebx*4]
eax = (int)(G8(ecx*4 + 0x4c26c0)); // 0x413ebe  movsx eax, byte ptr [ecx*4 + 0x4c26c0]
eax = (eax) << (0xa); // 0x413ec6  shl eax, 0xa
eax = (eax) - (0x400); // 0x413ec9  sub eax, 0x400
La_413ece:;
edx = eax >> 31; // 0x413ece  cdq
eax = (eax) - (edx); // 0x413ecf  sub eax, edx
edx = FA(0x1f0); // 0x413ed1  lea edx, [F+0x1f0]
stk[--sp] = (int)(0); // 0x413ed8  push 0
stk[--sp] = (int)(edx); // 0x413eda  push edx
edx = esi; // 0x413edb  mov edx, esi
ecx = FA(0x210); // 0x413edd  lea ecx, [F+0x210]
eax = ((int)(eax)) >> (1); // 0x413ee4  sar eax, 1
edx = (edx) << (0xa); // 0x413ee6  shl edx, 0xa
stk[--sp] = (int)(ecx); // 0x413ee9  push ecx
ecx = (int)(edx + eax + 0x200); // 0x413eea  lea ecx, [edx + eax + 0x200]
edx = edi; // 0x413ef1  mov edx, edi
edx = (edx) << (0xa); // 0x413ef3  shl edx, 0xa
stk[--sp] = (int)(ecx); // 0x413ef6  push ecx
eax = (int)(edx + eax + 0x200); // 0x413ef7  lea eax, [edx + eax + 0x200]
stk[--sp] = (int)(eax); // 0x413efe  push eax
eax = CC(0x42fb90, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // worldToScreen  0x413eff
ecx = (ecx & ~0xffffu) | ((GU16(ebp*2 + 0x53caf0)) & 0xffff); // 0x413f04  mov cx, word ptr [ebp*2 + 0x53caf0]
sp += 5; // 0x413f0c  add esp, 0x14  (caller stack cleanup)
ecx = (ecx) & (0x4000); // 0x413f0f  and ecx, 0x4000
// 0x413f15  test cx, cx
if (((((ecx & 0xffff))&0xffff)) != 0) goto La_413f46; // 0x413f18  jne 0x413f46
// 0x413f1a  cmp ebx, 5
if ((ebx) != (5)) goto La_413f46; // 0x413f1d  jne 0x413f46
stk[--sp] = (int)(esi); // 0x413f1f  push esi
stk[--sp] = (int)(edi); // 0x413f20  push edi
eax = CC(0x40df80, stk[sp+0], stk[sp+1]); // objectAt  0x413f21
edx = eax; // 0x413f26  mov edx, eax
sp += 2; // 0x413f28  add esp, 8  (caller stack cleanup)
edx = (edx) << (4); // 0x413f2b  shl edx, 4
ecx = G32(edx + 0x58bcc0); // 0x413f2e  mov ecx, dword ptr [edx + 0x58bcc0]
// 0x413f34  test ecx, ecx
if (((ecx)) == 0) goto La_413f46; // 0x413f36  je 0x413f46
stk[--sp] = (int)(esi); // 0x413f38  push esi
eax = (eax) & (1); // 0x413f39  and eax, 1
stk[--sp] = (int)(edi); // 0x413f3c  push edi
stk[--sp] = (int)(eax); // 0x413f3d  push eax
eax = CC(0x4012d0, stk[sp+0], stk[sp+1], stk[sp+2]); // drawRecords  0x413f3e
sp += 3; // 0x413f43  add esp, 0xc  (caller stack cleanup)
La_413f46:;
eax = (eax & ~0xffffu) | ((GU16(ebp*2 + 0x53caf0)) & 0xffff); // 0x413f46  mov ax, word ptr [ebp*2 + 0x53caf0]
ecx = eax; // 0x413f4e  mov ecx, eax
ecx = (ecx) & (0x800); // 0x413f50  and ecx, 0x800
// 0x413f56  test cx, cx
if (((((ecx & 0xffff))&0xffff)) == 0) goto La_414056; // 0x413f59  je 0x414056
// 0x413f5f  test dword ptr [0x59e7b8], 0x4000000
if (((((G32(0x59e7b8)) & (0x4000000)))) == 0) goto La_413f76; // 0x413f69  je 0x413f76
eax = (eax & ~0xff00u) | ((((((eax>>8)&0xff)) & (0xf7)) & 0xff)<<8); // 0x413f6b  and ah, 0xf7
G16(ebp*2 + 0x53caf0) = (eax & 0xffff); // 0x413f6e  mov word ptr [ebp*2 + 0x53caf0], ax
La_413f76:;
edx = (edx & ~0xffffu) | ((GU16(ebp*2 + 0x53caf0)) & 0xffff); // 0x413f76  mov dx, word ptr [ebp*2 + 0x53caf0]
eax = 0x3e7; // 0x413f7e  mov eax, 0x3e7
edx = (edx) & (0x4000); // 0x413f83  and edx, 0x4000
// 0x413f89  test dx, dx
if (((((edx & 0xffff))&0xffff)) == 0) goto La_413f9f; // 0x413f8c  je 0x413f9f
eax = FD(0x30); // 0x413f8e  mov eax, dword ptr [F+0x30]
stk[--sp] = (int)(0x63); // 0x413f92  push 0x63
stk[--sp] = (int)(0); // 0x413f94  push 0
stk[--sp] = (int)(eax); // 0x413f96  push eax
eax = CC(0x467130, stk[sp+0], stk[sp+1], stk[sp+2]); // clamp  0x413f97
sp += 3; // 0x413f9c  add esp, 0xc  (caller stack cleanup)
La_413f9f:;
ecx = (int)(edi + esi*2); // 0x413f9f  lea ecx, [edi + esi*2]
ecx = (ecx) & (0x80000003); // 0x413fa2  and ecx, 0x80000003
if (((int)(ecx)) >= 0) goto La_413faf; // 0x413fa8  jns 0x413faf
ecx = (ecx) - 1; // 0x413faa  dec ecx
ecx = (ecx) | (0xfffffffc); // 0x413fab  or ecx, 0xfffffffc
ecx = (ecx) + 1; // 0x413fae  inc ecx
La_413faf:;
edx = (int)(G8(ebp + 0x5722e8)); // 0x413faf  movsx edx, byte ptr [ebp + 0x5722e8]
stk[--sp] = (int)(0); // 0x413fb6  push 0
stk[--sp] = (int)(eax); // 0x413fb8  push eax
edx = (int)(edx + edx*2); // 0x413fb9  lea edx, [edx + edx*2]
stk[--sp] = (int)(ecx); // 0x413fbc  push ecx
edx = (edx) << (4); // 0x413fbd  shl edx, 4
eax = esi; // 0x413fc0  mov eax, esi
ebx = (ebx & ~0xffu) | ((GU8(edx + 0x578372)) & 0xff); // 0x413fc2  mov bl, byte ptr [edx + 0x578372]
// 0x413fc8  test bl, bl
if (((int)(s8)((ebx & 0xff))) > 0) goto La_413ffc; // 0x413fca  jg 0x413ffc
ecx = G32(0x53f9f8); // 0x413fcc  mov ecx, dword ptr [0x53f9f8]
eax = (eax) & (2); // 0x413fd2  and eax, 2
edx = edi; // 0x413fd5  mov edx, edi
ecx = (ecx) - (eax); // 0x413fd7  sub ecx, eax
edx = (edx) & (1); // 0x413fd9  and edx, 1
ecx = (ecx) - (edx); // 0x413fdc  sub ecx, edx
ecx = (ecx) - 1; // 0x413fde  dec ecx
stk[--sp] = (int)(ecx); // 0x413fdf  push ecx
eax = CC(0x467130, stk[sp+0], stk[sp+1], stk[sp+2]); // clamp  0x413fe0
sp += 3; // 0x413fe5  add esp, 0xc  (caller stack cleanup)
stk[--sp] = (int)(eax); // 0x413fe8  push eax
stk[--sp] = (int)(0x184); // 0x413fe9  push 0x184
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x413fee
G32(0x5a9cc8) = eax; // 0x413ff3  mov dword ptr [0x5a9cc8], eax
stk[--sp] = (int)(0x4c); // 0x413ff8  push 0x4c
goto La_41402a; // 0x413ffa  jmp 0x41402a
La_413ffc:;
ecx = G32(0x53fa28); // 0x413ffc  mov ecx, dword ptr [0x53fa28]
eax = (eax) & (2); // 0x414002  and eax, 2
edx = edi; // 0x414005  mov edx, edi
ecx = (ecx) - (eax); // 0x414007  sub ecx, eax
edx = (edx) & (1); // 0x414009  and edx, 1
ecx = (ecx) - (edx); // 0x41400c  sub ecx, edx
ecx = (ecx) - 1; // 0x41400e  dec ecx
stk[--sp] = (int)(ecx); // 0x41400f  push ecx
eax = CC(0x467130, stk[sp+0], stk[sp+1], stk[sp+2]); // clamp  0x414010
sp += 3; // 0x414015  add esp, 0xc  (caller stack cleanup)
stk[--sp] = (int)(eax); // 0x414018  push eax
stk[--sp] = (int)(0x190); // 0x414019  push 0x190
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x41401e
G32(0x5a9cc8) = eax; // 0x414023  mov dword ptr [0x5a9cc8], eax
stk[--sp] = (int)(0x4d); // 0x414028  push 0x4d
La_41402a:;
ecx = FD(0x6c); // 0x41402a  mov ecx, dword ptr [F+0x6c]
edx = FD(0x7c); // 0x414031  mov edx, dword ptr [F+0x7c]
stk[--sp] = (int)(4); // 0x414038  push 4
stk[--sp] = (int)(eax); // 0x41403a  push eax
eax = FD(0xc); // 0x41403b  mov eax, dword ptr [F+0xc]
stk[--sp] = (int)(0x28); // 0x41403f  push 0x28
stk[--sp] = (int)(0x1e); // 0x414041  push 0x1e
stk[--sp] = (int)(eax); // 0x414043  push eax
stk[--sp] = (int)(ecx); // 0x414044  push ecx
stk[--sp] = (int)(edx); // 0x414045  push edx
eax = CC(0x4628d0, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // addSprite  0x414046
sp += 11; // 0x41404b  add esp, 0x2c  (caller stack cleanup)
FD(0x30) = 0xffffffff; // 0x41404e  mov dword ptr [F+0x30], 0xffffffff
La_414056:;
ecx = (ecx & ~0xffu) | ((GU8(ebp + 0x5722e8)) & 0xff); // 0x414056  mov cl, byte ptr [ebp + 0x5722e8]
eax = (int)((s8)(ecx & 0xff)); // 0x41405c  movsx eax, cl
FD(0x34) = eax; // 0x41405f  mov dword ptr [F+0x34], eax
eax = (int)(eax + eax*2); // 0x414063  lea eax, [eax + eax*2]
eax = (eax) << (4); // 0x414066  shl eax, 4
FD(0x10) = eax; // 0x414069  mov dword ptr [F+0x10], eax
// 0x41406d  cmp byte ptr [eax + 0x578376], 0xd
if (((GU8(eax + 0x578376))&0xff) != ((0xd)&0xff)) goto La_414931; // 0x414074  jne 0x414931
// 0x41407a  cmp dword ptr [0x4c2844], 1
if ((int)(G32(0x4c2844)) <= (int)(1)) goto La_414931; // 0x414081  jle 0x414931
eax = G32(0x59e7b8); // 0x414087  mov eax, dword ptr [0x59e7b8]
// 0x41408c  test ah, 4
if ((((((((eax>>8)&0xff)) & (4)))&0xff)) != 0) goto La_414931; // 0x41408f  jne 0x414931
eax = (eax & ~0xffffu) | ((GU16(ebp*2 + 0x53caf0)) & 0xffff); // 0x414095  mov ax, word ptr [ebp*2 + 0x53caf0]
eax = (eax) & (0x100); // 0x41409d  and eax, 0x100
// 0x4140a2  test ax, ax
if (((((eax & 0xffff))&0xffff)) == 0) goto La_4140b0; // 0x4140a5  je 0x4140b0
// 0x4140a7  cmp cl, 0x10
if ((((ecx & 0xff))&0xff) != ((0x10)&0xff)) goto La_414931; // 0x4140aa  jne 0x414931
La_4140b0:;
eax = (eax) ^ (eax); // 0x4140b0  xor eax, eax
ebp = 1; // 0x4140b2  mov ebp, 1
FD(0x18) = eax; // 0x4140b7  mov dword ptr [F+0x18], eax
FD(0x2c) = eax; // 0x4140bb  mov dword ptr [F+0x2c], eax
FD(0x28) = eax; // 0x4140bf  mov dword ptr [F+0x28], eax
FD(0x20) = ebp; // 0x4140c3  mov dword ptr [F+0x20], ebp
La_4140c7:;
ecx = G32(0x5685f4); // 0x4140c7  mov ecx, dword ptr [0x5685f4]
ebx = FD(0x18); // 0x4140cd  mov ebx, dword ptr [F+0x18]
edx = (edx) | (0xffffffff); // 0x4140d1  or edx, 0xffffffff
ebp = (int)(ecx + ebp + -1); // 0x4140d4  lea ebp, [ecx + ebp - 1]
La_4140d8:;
eax = ebp; // 0x4140d8  mov eax, ebp
eax = (eax) & (7); // 0x4140da  and eax, 7
ecx = G32(eax*4 + 0x4c2878); // 0x4140dd  mov ecx, dword ptr [eax*4 + 0x4c2878]
eax = G32(eax*4 + 0x4c2898); // 0x4140e4  mov eax, dword ptr [eax*4 + 0x4c2898]
ecx = (ecx) + (edi); // 0x4140eb  add ecx, edi
ecx = (int)(ecx + ecx*4); // 0x4140ed  lea ecx, [ecx + ecx*4]
ecx = (int)(ecx + ecx*4); // 0x4140f0  lea ecx, [ecx + ecx*4]
ecx = (int)(eax + ecx*2); // 0x4140f3  lea ecx, [eax + ecx*2]
eax = 1; // 0x4140f6  mov eax, 1
ecx = (int)(G8(ecx + esi + 0x5722e8)); // 0x4140fb  movsx ecx, byte ptr [ecx + esi + 0x5722e8]
eax = (eax) << ((ecx & 0xff)); // 0x414103  shl eax, cl
ebx = (ebx) | (eax); // 0x414105  or ebx, eax
// 0x414107  test edx, edx
if (((int)(edx)) < 0) goto La_414115; // 0x414109  jl 0x414115
// 0x41410b  cmp ecx, dword ptr [F+0x34]
if ((ecx) != (FD(0x34))) goto La_414115; // 0x41410f  jne 0x414115
FD(0x2c) = (FD(0x2c)) + 1; // 0x414111  inc dword ptr [F+0x2c]
La_414115:;
edx = (edx) + 1; // 0x414115  inc edx
ebp = (ebp) + 1; // 0x414116  inc ebp
// 0x414117  cmp edx, 1
if ((int)(edx) <= (int)(1)) goto La_4140d8; // 0x41411a  jle 0x4140d8
ebp = FD(0x20); // 0x41411c  mov ebp, dword ptr [F+0x20]
FD(0x18) = ebx; // 0x414120  mov dword ptr [F+0x18], ebx
ebp = (ebp) + (2); // 0x414124  add ebp, 2
// 0x414127  cmp ebp, 8
FD(0x20) = ebp; // 0x41412a  mov dword ptr [F+0x20], ebp
if ((int)(ebp) < (int)(8)) goto La_4140c7; // 0x41412e  jl 0x4140c7
ecx = FD(0x10); // 0x414130  mov ecx, dword ptr [F+0x10]
edx = (edx) ^ (edx); // 0x414134  xor edx, edx
ebx = (int)(G8(ecx + 0x578377)); // 0x414136  movsx ebx, byte ptr [ecx + 0x578377]
FD(0x10) = ebx; // 0x41413d  mov dword ptr [F+0x10], ebx
La_414141:;
eax = G32(edx*4 + 0x4c2878); // 0x414141  mov eax, dword ptr [edx*4 + 0x4c2878]
ecx = G32(edx*4 + 0x4c2898); // 0x414148  mov ecx, dword ptr [edx*4 + 0x4c2898]
eax = (eax) + (edi); // 0x41414f  add eax, edi
eax = (int)(eax + eax*4); // 0x414151  lea eax, [eax + eax*4]
eax = (int)(eax + eax*4); // 0x414154  lea eax, [eax + eax*4]
eax = (int)(ecx + eax*2); // 0x414157  lea eax, [ecx + eax*2]
ebp = (int)(G8(eax + esi + 0x5722e8)); // 0x41415a  movsx ebp, byte ptr [eax + esi + 0x5722e8]
eax = edx; // 0x414162  mov eax, edx
eax = (eax & ~0xffu) | ((((eax & 0xff)) | (4)) & 0xff); // 0x414164  or al, 4
eax = (eax) << (2); // 0x414166  shl eax, 2
ecx = G32(eax + 0x4c2878); // 0x414169  mov ecx, dword ptr [eax + 0x4c2878]
eax = G32(eax + 0x4c2898); // 0x41416f  mov eax, dword ptr [eax + 0x4c2898]
ecx = (ecx) + (edi); // 0x414175  add ecx, edi
ecx = (int)(ecx + ecx*4); // 0x414177  lea ecx, [ecx + ecx*4]
ecx = (int)(ecx + ecx*4); // 0x41417a  lea ecx, [ecx + ecx*4]
ecx = (int)(eax + ecx*2); // 0x41417d  lea ecx, [eax + ecx*2]
eax = (int)(ebp + ebp*2); // 0x414180  lea eax, [ebp + ebp*2]
eax = (eax) << (4); // 0x414184  shl eax, 4
ecx = (int)(G8(ecx + esi + 0x5722e8)); // 0x414187  movsx ecx, byte ptr [ecx + esi + 0x5722e8]
eax = (int)(G8(eax + 0x578377)); // 0x41418f  movsx eax, byte ptr [eax + 0x578377]
// 0x414196  cmp eax, ebx
if ((eax) != (ebx)) goto La_4141bb; // 0x414198  jne 0x4141bb
ebp = FD(0x10); // 0x41419a  mov ebp, dword ptr [F+0x10]
ebx = (int)(ecx + ecx*2); // 0x41419e  lea ebx, [ecx + ecx*2]
ebx = (ebx) << (4); // 0x4141a1  shl ebx, 4
ebx = (int)(G8(ebx + 0x578377)); // 0x4141a4  movsx ebx, byte ptr [ebx + 0x578377]
// 0x4141ab  cmp ebx, ebp
if ((ebx) != (ebp)) goto La_4141b3; // 0x4141ad  jne 0x4141b3
FD(0x28) = (FD(0x28)) + 1; // 0x4141af  inc dword ptr [F+0x28]
La_4141b3:;
ebx = FD(0x10); // 0x4141b3  mov ebx, dword ptr [F+0x10]
// 0x4141b7  cmp eax, ebx
if ((eax) == (ebx)) goto La_4141d0; // 0x4141b9  je 0x4141d0
La_4141bb:;
ecx = (int)(ecx + ecx*2); // 0x4141bb  lea ecx, [ecx + ecx*2]
ecx = (ecx) << (4); // 0x4141be  shl ecx, 4
eax = (int)(G8(ecx + 0x578377)); // 0x4141c1  movsx eax, byte ptr [ecx + 0x578377]
// 0x4141c8  cmp eax, ebx
if ((eax) == (ebx)) goto La_4141d0; // 0x4141ca  je 0x4141d0
FD(0x28) = (FD(0x28)) + 1; // 0x4141cc  inc dword ptr [F+0x28]
La_4141d0:;
edx = (edx) + 1; // 0x4141d0  inc edx
// 0x4141d1  cmp edx, 4
if ((int)(edx) < (int)(4)) goto La_414141; // 0x4141d4  jl 0x414141
edx = FD(0x14); // 0x4141da  mov edx, dword ptr [F+0x14]
eax = (eax & ~0xffu) | ((GU8(edx + 0x5722e8)) & 0xff); // 0x4141de  mov al, byte ptr [edx + 0x5722e8]
// 0x4141e4  cmp al, 0x10
if ((((eax & 0xff))&0xff) == ((0x10)&0xff)) goto La_414931; // 0x4141e6  je 0x414931
// 0x4141ec  cmp al, 0xd
if ((((eax & 0xff))&0xff) != ((0xd)&0xff)) goto La_414359; // 0x4141ee  jne 0x414359
ebx = FD(0x28); // 0x4141f4  mov ebx, dword ptr [F+0x28]
ecx = 0xfffffff8; // 0x4141f8  mov ecx, 0xfffffff8
eax = 0x19d; // 0x4141fd  mov eax, 0x19d
// 0x414202  cmp ebx, 4
FD(0x20) = ecx; // 0x414205  mov dword ptr [F+0x20], ecx
FD(0x10) = eax; // 0x414209  mov dword ptr [F+0x10], eax
if ((u32)(ebx) > (u32)(4)) goto La_41423e; // 0x41420d  ja 0x41423e
// 0x41420f  jmp dword ptr [ebx*4 + 0x4216e4]  (switch table 0x4216e4, ebx=0..4 after ja default above)
switch (ebx) { case 0: goto La_414216; case 1: goto La_414220; case 2: goto La_414228; case 3: goto La_414232; case 4: goto La_41423a; }
La_414216:;
FD(0x10) = 0x19b; // 0x414216  mov dword ptr [F+0x10], 0x19b
goto La_41423e; // 0x41421e  jmp 0x41423e
La_414220:;
FD(0x20) = 0xfffffff9; // 0x414220  mov dword ptr [F+0x20], 0xfffffff9
La_414228:;
FD(0x10) = 0x19c; // 0x414228  mov dword ptr [F+0x10], 0x19c
goto La_41423e; // 0x414230  jmp 0x41423e
La_414232:;
FD(0x20) = 0xfffffff9; // 0x414232  mov dword ptr [F+0x20], 0xfffffff9
La_41423a:;
FD(0x10) = eax; // 0x41423a  mov dword ptr [F+0x10], eax
La_41423e:;
eax = (eax & ~0xffu) | ((GU8(edx + 0x56988c)) & 0xff); // 0x41423e  mov al, byte ptr [edx + 0x56988c]
// 0x414244  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_414263; // 0x414246  je 0x414263
eax = (eax) & (0xff); // 0x414248  and eax, 0xff
ebx = 3; // 0x41424d  mov ebx, 3
edx = eax >> 31; // 0x414252  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ebx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414253  idiv ebx
FD(0x20) = ecx; // 0x414255  mov dword ptr [F+0x20], ecx
edx = (edx) + (0x19b); // 0x414259  add edx, 0x19b
FD(0x10) = edx; // 0x41425f  mov dword ptr [F+0x10], edx
La_414263:;
ecx = esi; // 0x414263  mov ecx, esi
edx = edi; // 0x414265  mov edx, edi
ecx = (ecx) << (8); // 0x414267  shl ecx, 8
edx = (edx) << (8); // 0x41426a  shl edx, 8
stk[--sp] = (int)(ecx); // 0x41426d  push ecx
stk[--sp] = (int)(edx); // 0x41426e  push edx
eax = CC(0x42dba0, stk[sp+0], stk[sp+1]); // sampleHeight  0x41426f
edx = eax >> 31; // 0x414274  cdq
edx = (edx) & (0x1f); // 0x414275  and edx, 0x1f
ebp = edi; // 0x414278  mov ebp, edi
eax = (eax) + (edx); // 0x41427a  add eax, edx
sp += 2; // 0x41427c  add esp, 8  (caller stack cleanup)
ebx = eax; // 0x41427f  mov ebx, eax
eax = (int)(esi + esi); // 0x414281  lea eax, [esi + esi]
ebx = ((int)(ebx)) >> (5); // 0x414284  sar ebx, 5
ebx = (ebx) & (3); // 0x414287  and ebx, 3
ebp = (ebp) - (eax); // 0x41428a  sub ebp, eax
eax = FD(0x30); // 0x41428c  mov eax, dword ptr [F+0x30]
ebx = (ebx) + (0x25); // 0x414290  add ebx, 0x25
ebp = (ebp) & (3); // 0x414293  and ebp, 3
// 0x414296  cmp eax, -1
stk[--sp] = (int)(ebp); // 0x414299  push ebp
if ((eax) == (-1)) goto La_41439c; // 0x41429a  je 0x41439c
edx = FD(0x10); // 0x4142a0  mov edx, dword ptr [F+0x10]
ecx = eax; // 0x4142a4  mov ecx, eax
stk[--sp] = (int)(ecx); // 0x4142a6  push ecx
stk[--sp] = (int)(edx); // 0x4142a7  push edx
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x4142a8
G32(0x5a9cc8) = eax; // 0x4142ad  mov dword ptr [0x5a9cc8], eax
eax = FD(0x10); // 0x4142b2  mov eax, dword ptr [F+0x10]
sp += 3; // 0x4142b6  add esp, 0xc  (caller stack cleanup)
ecx = G32(eax*4 + 0x53f3e8); // 0x4142b9  mov ecx, dword ptr [eax*4 + 0x53f3e8]
eax = FD(0x30); // 0x4142c0  mov eax, dword ptr [F+0x30]
ecx = (ecx) - 1; // 0x4142c4  dec ecx
// 0x4142c5  cmp eax, ecx
if ((eax) != (ecx)) goto La_4142d5; // 0x4142c7  jne 0x4142d5
eax = FD(0x14); // 0x4142c9  mov eax, dword ptr [F+0x14]
G8(eax*2 + 0x53caf1) = (GU8(eax*2 + 0x53caf1)) & (0xbf); // 0x4142cd  and byte ptr [eax*2 + 0x53caf1], 0xbf
La_4142d5:;
eax = FD(0x30); // 0x4142d5  mov eax, dword ptr [F+0x30]
// 0x4142d9  test eax, eax
if (((eax)) != 0) goto La_4142f2; // 0x4142db  jne 0x4142f2
stk[--sp] = (int)(0); // 0x4142dd  push 0
stk[--sp] = (int)(0); // 0x4142df  push 0
stk[--sp] = (int)(0); // 0x4142e1  push 0
stk[--sp] = (int)(0x64); // 0x4142e3  push 0x64
stk[--sp] = (int)(0x93); // 0x4142e5  push 0x93
eax = CC(0x4481b0, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // playSound  0x4142ea
sp += 5; // 0x4142ef  add esp, 0x14  (caller stack cleanup)
La_4142f2:;
FD(0x30) = 0xfffffffe; // 0x4142f2  mov dword ptr [F+0x30], 0xfffffffe
FD(0x20) = 0xfffffff8; // 0x4142fa  mov dword ptr [F+0x20], 0xfffffff8
La_414302:;
// 0x414302  cmp ebx, -1
if ((ebx) == (-1)) goto La_414359; // 0x414305  je 0x414359
eax = FD(0x20); // 0x414307  mov eax, dword ptr [F+0x20]
ecx = G32(0x5a9cc8); // 0x41430b  mov ecx, dword ptr [0x5a9cc8]
edx = FD(0x40); // 0x414311  mov edx, dword ptr [F+0x40]
stk[--sp] = (int)(ebx); // 0x414315  push ebx
stk[--sp] = (int)(eax); // 0x414316  push eax
eax = FD(0xc); // 0x414317  mov eax, dword ptr [F+0xc]
stk[--sp] = (int)(ecx); // 0x41431b  push ecx
ecx = FD(0x6c); // 0x41431c  mov ecx, dword ptr [F+0x6c]
stk[--sp] = (int)(0x8c); // 0x414320  push 0x8c
eax = (eax) + (edx); // 0x414325  add eax, edx
stk[--sp] = (int)(0x8c); // 0x414327  push 0x8c
stk[--sp] = (int)(eax); // 0x41432c  push eax
edx = (int)(edi + esi*2); // 0x41432d  lea edx, [edi + esi*2]
eax = esi; // 0x414330  mov eax, esi
eax = (eax) + (edx); // 0x414332  add eax, edx
stk[--sp] = (int)(ecx); // 0x414334  push ecx
edx = eax >> 31; // 0x414335  cdq
ecx = 5; // 0x414336  mov ecx, 5
ebx = FD(0x7c); // 0x41433b  mov ebx, dword ptr [F+0x7c]
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414342  idiv ecx
edx = (edx) - (2); // 0x414344  sub edx, 2
edx = (edx) * (G32(0x4c2844)); // 0x414347  imul edx, dword ptr [0x4c2844]
edx = (edx) + (ebx); // 0x41434e  add edx, ebx
stk[--sp] = (int)(edx); // 0x414350  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x414351
sp += 8; // 0x414356  add esp, 0x20  (caller stack cleanup)
La_414359:;
edx = FD(0x14); // 0x414359  mov edx, dword ptr [F+0x14]
// 0x41435d  cmp byte ptr [edx + 0x5722e8], 0xe
if (((GU8(edx + 0x5722e8))&0xff) != ((0xe)&0xff)) goto La_4146e1; // 0x414364  jne 0x4146e1
ecx = FD(0x28); // 0x41436a  mov ecx, dword ptr [F+0x28]
eax = 0x194; // 0x41436e  mov eax, 0x194
// 0x414373  cmp ecx, 4
FD(0x20) = 0xfffffff8; // 0x414376  mov dword ptr [F+0x20], 0xfffffff8
FD(0x10) = eax; // 0x41437e  mov dword ptr [F+0x10], eax
if ((u32)(ecx) > (u32)(4)) goto La_41444a; // 0x414382  ja 0x41444a
// 0x414388  jmp dword ptr [ecx*4 + 0x4216f8]  (switch table 0x4216f8, ecx=0..4 after ja default above)
switch (ecx) { case 0: goto La_41438f; case 1: goto La_414434; case 2: goto La_41442c; case 3: goto La_41443e; case 4: goto La_414446; }
La_41438f:;
FD(0x10) = 0x192; // 0x41438f  mov dword ptr [F+0x10], 0x192
goto La_41444a; // 0x414397  jmp 0x41444a
La_41439c:;
eax = FD(0x10); // 0x41439c  mov eax, dword ptr [F+0x10]
edx = G32(eax*4 + 0x53f3e8); // 0x4143a0  mov edx, dword ptr [eax*4 + 0x53f3e8]
edx = (edx) - 1; // 0x4143a7  dec edx
stk[--sp] = (int)(edx); // 0x4143a8  push edx
stk[--sp] = (int)(eax); // 0x4143a9  push eax
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x4143aa
G32(0x5a9cc8) = eax; // 0x4143af  mov dword ptr [0x5a9cc8], eax
eax = FD(0x18); // 0x4143b4  mov eax, dword ptr [F+0x18]
sp += 3; // 0x4143b8  add esp, 0xc  (caller stack cleanup)
// 0x4143bb  test ah, 0x20
if ((((((((eax>>8)&0xff)) & (0x20)))&0xff)) != 0) goto La_414302; // 0x4143be  jne 0x414302
eax = (eax & ~0xffu) | ((GU8(0x5a34e0)) & 0xff); // 0x4143c4  mov al, byte ptr [0x5a34e0]
// 0x4143c9  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_4143d5; // 0x4143cb  je 0x4143d5
// 0x4143cd  cmp al, 3
if ((((eax & 0xff))&0xff) != ((3)&0xff)) goto La_414302; // 0x4143cf  jne 0x414302
La_4143d5:;
eax = G32(0x53f8ac); // 0x4143d5  mov eax, dword ptr [0x53f8ac]
stk[--sp] = (int)(ebp); // 0x4143da  push ebp
eax = (eax) - 1; // 0x4143db  dec eax
stk[--sp] = (int)(eax); // 0x4143dc  push eax
stk[--sp] = (int)(0x131); // 0x4143dd  push 0x131
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x4143e2
ecx = G32(0x4c2840); // 0x4143e7  mov ecx, dword ptr [0x4c2840]
stk[--sp] = (int)(0xa6); // 0x4143ed  push 0xa6
stk[--sp] = (int)(4); // 0x4143f2  push 4
G32(0x5a9cc8) = eax; // 0x4143f4  mov dword ptr [0x5a9cc8], eax
stk[--sp] = (int)(eax); // 0x4143f9  push eax
eax = 0x2aaaaaab; // 0x4143fa  mov eax, 0x2aaaaaab
{ long long _p = (long long)(int)eax * (long long)(int)(ecx); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x4143ff  imul ecx
eax = FD(0x6c); // 0x414401  mov eax, dword ptr [F+0x6c]
ecx = edx; // 0x414408  mov ecx, edx
ecx = ((u32)(ecx)) >> (0x1f); // 0x41440a  shr ecx, 0x1f
edx = (edx) + (ecx); // 0x41440d  add edx, ecx
stk[--sp] = (int)(0x64); // 0x41440f  push 0x64
edx = (edx) + (eax); // 0x414411  add edx, eax
stk[--sp] = (int)(0x64); // 0x414413  push 0x64
stk[--sp] = (int)(edx); // 0x414415  push edx
edx = FD(0x7c); // 0x414416  mov edx, dword ptr [F+0x7c]
stk[--sp] = (int)(eax); // 0x41441d  push eax
stk[--sp] = (int)(edx); // 0x41441e  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x41441f
sp += 11; // 0x414424  add esp, 0x2c  (caller stack cleanup)
goto La_414359; // 0x414427  jmp 0x414359
La_41442c:;
FD(0x20) = 0xfffffff9; // 0x41442c  mov dword ptr [F+0x20], 0xfffffff9
La_414434:;
FD(0x10) = 0x193; // 0x414434  mov dword ptr [F+0x10], 0x193
goto La_41444a; // 0x41443c  jmp 0x41444a
La_41443e:;
FD(0x20) = 0xfffffff9; // 0x41443e  mov dword ptr [F+0x20], 0xfffffff9
La_414446:;
FD(0x10) = eax; // 0x414446  mov dword ptr [F+0x10], eax
La_41444a:;
ebx = esi; // 0x41444a  mov ebx, esi
eax = edi; // 0x41444c  mov eax, edi
ebx = (ebx) << (8); // 0x41444e  shl ebx, 8
eax = (eax) << (8); // 0x414451  shl eax, 8
stk[--sp] = (int)(ebx); // 0x414454  push ebx
stk[--sp] = (int)(eax); // 0x414455  push eax
ebp = 0x29; // 0x414456  mov ebp, 0x29
FD(0x34) = eax; // 0x41445b  mov dword ptr [F+0x34], eax
eax = CC(0x42dba0, stk[sp+0], stk[sp+1]); // sampleHeight  0x41445f
ecx = eax; // 0x414464  mov ecx, eax
eax = 0x4ec4ec4f; // 0x414466  mov eax, 0x4ec4ec4f
{ long long _p = (long long)(int)eax * (long long)(int)(ecx); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x41446b  imul ecx
edx = ((int)(edx)) >> (2); // 0x41446d  sar edx, 2
eax = edx; // 0x414470  mov eax, edx
sp += 2; // 0x414472  add esp, 8  (caller stack cleanup)
eax = ((u32)(eax)) >> (0x1f); // 0x414475  shr eax, 0x1f
edx = (edx) + (eax); // 0x414478  add edx, eax
// 0x41447a  test dl, 4
if (((((((edx & 0xff)) & (4)))&0xff)) == 0) goto La_4144a5; // 0x41447d  je 0x4144a5
edx = FD(0x10); // 0x41447f  mov edx, dword ptr [F+0x10]
eax = (eax & ~0xffu) | ((GU8(0x5a34e0)) & 0xff); // 0x414483  mov al, byte ptr [0x5a34e0]
edx = (edx) + (6); // 0x414488  add edx, 6
// 0x41448b  cmp al, 2
FD(0x10) = edx; // 0x41448d  mov dword ptr [F+0x10], edx
if ((((eax & 0xff))&0xff) != ((2)&0xff)) goto La_41449a; // 0x414491  jne 0x41449a
ebp = 0x2a; // 0x414493  mov ebp, 0x2a
goto La_4144d1; // 0x414498  jmp 0x4144d1
La_41449a:;
// 0x41449a  cmp al, 1
if ((((eax & 0xff))&0xff) != ((1)&0xff)) goto La_4144aa; // 0x41449c  jne 0x4144aa
ebp = 0x2a; // 0x41449e  mov ebp, 0x2a
goto La_4144d1; // 0x4144a3  jmp 0x4144d1
La_4144a5:;
eax = (eax & ~0xffu) | ((GU8(0x5a34e0)) & 0xff); // 0x4144a5  mov al, byte ptr [0x5a34e0]
La_4144aa:;
// 0x4144aa  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_4144b2; // 0x4144ac  je 0x4144b2
// 0x4144ae  cmp al, 3
if ((((eax & 0xff))&0xff) != ((3)&0xff)) goto La_4144d1; // 0x4144b0  jne 0x4144d1
La_4144b2:;
ecx = FD(0x34); // 0x4144b2  mov ecx, dword ptr [F+0x34]
stk[--sp] = (int)(ebx); // 0x4144b6  push ebx
stk[--sp] = (int)(ecx); // 0x4144b7  push ecx
eax = CC(0x42dba0, stk[sp+0], stk[sp+1]); // sampleHeight  0x4144b8
edx = eax >> 31; // 0x4144bd  cdq
edx = (edx) & (0x1f); // 0x4144be  and edx, 0x1f
sp += 2; // 0x4144c1  add esp, 8  (caller stack cleanup)
eax = (eax) + (edx); // 0x4144c4  add eax, edx
ebp = eax; // 0x4144c6  mov ebp, eax
ebp = ((int)(ebp)) >> (5); // 0x4144c8  sar ebp, 5
ebp = (ebp) & (3); // 0x4144cb  and ebp, 3
ebp = (ebp) + (0x29); // 0x4144ce  add ebp, 0x29
La_4144d1:;
edx = FD(0x14); // 0x4144d1  mov edx, dword ptr [F+0x14]
ebx = (ebx & ~0xffu) | ((GU8(edx + 0x56988c)) & 0xff); // 0x4144d5  mov bl, byte ptr [edx + 0x56988c]
// 0x4144db  test bl, bl
if (((((ebx & 0xff))&0xff)) == 0) goto La_41453d; // 0x4144dd  je 0x41453d
ebx = (ebx) & (0xff); // 0x4144df  and ebx, 0xff
ecx = 3; // 0x4144e5  mov ecx, 3
eax = ebx; // 0x4144ea  mov eax, ebx
edx = eax >> 31; // 0x4144ec  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x4144ed  idiv ecx
eax = (eax & ~0xffu) | ((GU8(0x5a34e0)) & 0xff); // 0x4144ef  mov al, byte ptr [0x5a34e0]
ecx = edx; // 0x4144f4  mov ecx, edx
ecx = (ecx) + (0x192); // 0x4144f6  add ecx, 0x192
// 0x4144fc  cmp al, 2
FD(0x10) = ecx; // 0x4144fe  mov dword ptr [F+0x10], ecx
if ((((eax & 0xff))&0xff) == ((2)&0xff)) goto La_414508; // 0x414502  je 0x414508
// 0x414504  cmp al, 1
if ((((eax & 0xff))&0xff) != ((1)&0xff)) goto La_41450d; // 0x414506  jne 0x41450d
La_414508:;
ebp = 0x29; // 0x414508  mov ebp, 0x29
La_41450d:;
eax = 0x55555556; // 0x41450d  mov eax, 0x55555556
{ long long _p = (long long)(int)eax * (long long)(int)(ebx); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x414512  imul ebx
eax = edx; // 0x414514  mov eax, edx
eax = ((u32)(eax)) >> (0x1f); // 0x414516  shr eax, 0x1f
edx = (edx) + (eax); // 0x414519  add edx, eax
// 0x41451b  test dl, 1
if (((((((edx & 0xff)) & (1)))&0xff)) == 0) goto La_414535; // 0x41451e  je 0x414535
eax = (eax & ~0xffu) | ((GU8(0x5a34e0)) & 0xff); // 0x414520  mov al, byte ptr [0x5a34e0]
ecx = (ecx) + (6); // 0x414525  add ecx, 6
// 0x414528  cmp al, 2
FD(0x10) = ecx; // 0x41452a  mov dword ptr [F+0x10], ecx
if ((((eax & 0xff))&0xff) == ((2)&0xff)) goto La_414534; // 0x41452e  je 0x414534
// 0x414530  cmp al, 1
if ((((eax & 0xff))&0xff) != ((1)&0xff)) goto La_414535; // 0x414532  jne 0x414535
La_414534:;
ebp = (ebp) + 1; // 0x414534  inc ebp
La_414535:;
FD(0x20) = 0xfffffff8; // 0x414535  mov dword ptr [F+0x20], 0xfffffff8
La_41453d:;
ecx = (int)(esi + esi); // 0x41453d  lea ecx, [esi + esi]
eax = edi; // 0x414540  mov eax, edi
eax = (eax) - (ecx); // 0x414542  sub eax, ecx
ecx = FD(0x30); // 0x414544  mov ecx, dword ptr [F+0x30]
eax = (eax) & (3); // 0x414548  and eax, 3
// 0x41454b  cmp ecx, -1
stk[--sp] = (int)(eax); // 0x41454e  push eax
if ((ecx) == (-1)) goto La_4145af; // 0x41454f  je 0x4145af
edx = FD(0x10); // 0x414551  mov edx, dword ptr [F+0x10]
ebx = ecx; // 0x414555  mov ebx, ecx
stk[--sp] = (int)(ebx); // 0x414557  push ebx
stk[--sp] = (int)(edx); // 0x414558  push edx
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x414559
ecx = FD(0x10); // 0x41455e  mov ecx, dword ptr [F+0x10]
sp += 3; // 0x414562  add esp, 0xc  (caller stack cleanup)
G32(0x5a9cc8) = eax; // 0x414565  mov dword ptr [0x5a9cc8], eax
edx = G32(ecx*4 + 0x53f3e8); // 0x41456a  mov edx, dword ptr [ecx*4 + 0x53f3e8]
edx = (edx) - 1; // 0x414571  dec edx
// 0x414572  cmp ebx, edx
if ((ebx) != (edx)) goto La_414582; // 0x414574  jne 0x414582
ecx = FD(0x14); // 0x414576  mov ecx, dword ptr [F+0x14]
G8(ecx*2 + 0x53caf1) = (GU8(ecx*2 + 0x53caf1)) & (0xbf); // 0x41457a  and byte ptr [ecx*2 + 0x53caf1], 0xbf
La_414582:;
// 0x414582  test ebx, ebx
if (((ebx)) != 0) goto La_41459d; // 0x414584  jne 0x41459d
stk[--sp] = (int)(ebx); // 0x414586  push ebx
stk[--sp] = (int)(ebx); // 0x414587  push ebx
stk[--sp] = (int)(ebx); // 0x414588  push ebx
stk[--sp] = (int)(0x32); // 0x414589  push 0x32
stk[--sp] = (int)(0x91); // 0x41458b  push 0x91
eax = CC(0x4481b0, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // playSound  0x414590
eax = G32(0x5a9cc8); // 0x414595  mov eax, dword ptr [0x5a9cc8]
sp += 5; // 0x41459a  add esp, 0x14  (caller stack cleanup)
La_41459d:;
FD(0x30) = 0xfffffffe; // 0x41459d  mov dword ptr [F+0x30], 0xfffffffe
FD(0x20) = 0xfffffff8; // 0x4145a5  mov dword ptr [F+0x20], 0xfffffff8
goto La_4145ca; // 0x4145ad  jmp 0x4145ca
La_4145af:;
eax = FD(0x10); // 0x4145af  mov eax, dword ptr [F+0x10]
ecx = G32(eax*4 + 0x53f3e8); // 0x4145b3  mov ecx, dword ptr [eax*4 + 0x53f3e8]
ecx = (ecx) - 1; // 0x4145ba  dec ecx
stk[--sp] = (int)(ecx); // 0x4145bb  push ecx
stk[--sp] = (int)(eax); // 0x4145bc  push eax
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x4145bd
sp += 3; // 0x4145c2  add esp, 0xc  (caller stack cleanup)
G32(0x5a9cc8) = eax; // 0x4145c5  mov dword ptr [0x5a9cc8], eax
La_4145ca:;
// 0x4145ca  cmp dword ptr [F+0x10], 0x192
if ((FD(0x10)) != (0x192)) goto La_414695; // 0x4145d2  jne 0x414695
edx = FD(0xc); // 0x4145d8  mov edx, dword ptr [F+0xc]
ecx = FD(0x40); // 0x4145dc  mov ecx, dword ptr [F+0x40]
stk[--sp] = (int)(ebp); // 0x4145e0  push ebp
ebx = (int)(edx + ecx); // 0x4145e1  lea ebx, [edx + ecx]
ecx = FD(0x20); // 0x4145e4  mov ecx, dword ptr [F+0x20]
edx = esi; // 0x4145e8  mov edx, esi
edx = ((u32)(edx)) >> (1); // 0x4145ea  shr edx, 1
edx = (edx) & (1); // 0x4145ec  and edx, 1
edx = (edx) + (ecx); // 0x4145ef  add edx, ecx
ecx = edi; // 0x4145f1  mov ecx, edi
stk[--sp] = (int)(edx); // 0x4145f3  push edx
stk[--sp] = (int)(eax); // 0x4145f4  push eax
eax = FD(0x6c); // 0x4145f5  mov eax, dword ptr [F+0x6c]
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (1)) & 0xff); // 0x4145f9  and cl, 1
stk[--sp] = (int)(0x8c); // 0x4145fc  push 0x8c
stk[--sp] = (int)(0x8c); // 0x414601  push 0x8c
ecx = (ecx & ~0xffu) | ((-((ecx & 0xff))) & 0xff); // 0x414606  neg cl
stk[--sp] = (int)(ebx); // 0x414608  push ebx
stk[--sp] = (int)(eax); // 0x414609  push eax
eax = G32(0x4c2840); // 0x41460a  mov eax, dword ptr [0x4c2840]
ecx = -(int)((((ecx & 0xff)) != 0)); // 0x41460f  sbb ecx, ecx
ecx = (ecx) & (0xfffffffe); // 0x414611  and ecx, 0xfffffffe
edx = eax >> 31; // 0x414614  cdq
ecx = (ecx) + (5); // 0x414615  add ecx, 5
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414618  idiv ecx
eax = (eax) + (FD(0x7c)); // 0x41461a  add eax, dword ptr [F+0x7c]
stk[--sp] = (int)(eax); // 0x414621  push eax
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x414622
eax = FD(0x20); // 0x414627  mov eax, dword ptr [F+0x20]
edx = esi; // 0x41462b  mov edx, esi
edx = ~(edx); // 0x41462d  not edx
edx = ((u32)(edx)) >> (1); // 0x41462f  shr edx, 1
ecx = edi; // 0x414631  mov ecx, edi
edx = (edx) & (1); // 0x414633  and edx, 1
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (2)) & 0xff); // 0x414636  and cl, 2
edx = (edx) + (eax); // 0x414639  add edx, eax
eax = G32(0x5a9cc8); // 0x41463b  mov eax, dword ptr [0x5a9cc8]
stk[--sp] = (int)(ebp); // 0x414640  push ebp
ebp = G32(0x4c2840); // 0x414641  mov ebp, dword ptr [0x4c2840]
stk[--sp] = (int)(edx); // 0x414647  push edx
ecx = (ecx & ~0xffu) | ((-((ecx & 0xff))) & 0xff); // 0x414648  neg cl
stk[--sp] = (int)(eax); // 0x41464a  push eax
eax = ebp; // 0x41464b  mov eax, ebp
ecx = -(int)((((ecx & 0xff)) != 0)); // 0x41464d  sbb ecx, ecx
stk[--sp] = (int)(0x8c); // 0x41464f  push 0x8c
ecx = (ecx) & (0xa); // 0x414654  and ecx, 0xa
stk[--sp] = (int)(0x8c); // 0x414657  push 0x8c
edx = eax >> 31; // 0x41465c  cdq
ecx = (ecx) + (-6); // 0x41465d  add ecx, -6
stk[--sp] = (int)(ebx); // 0x414660  push ebx
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414661  idiv ecx
edx = FD(0x6c); // 0x414663  mov edx, dword ptr [F+0x6c]
ecx = esi; // 0x41466a  mov ecx, esi
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (1)) & 0xff); // 0x41466c  and cl, 1
edx = (edx) - (eax); // 0x41466f  sub edx, eax
eax = ebp; // 0x414671  mov eax, ebp
ecx = (ecx & ~0xffu) | ((-((ecx & 0xff))) & 0xff); // 0x414673  neg cl
ecx = -(int)((((ecx & 0xff)) != 0)); // 0x414675  sbb ecx, ecx
stk[--sp] = (int)(edx); // 0x414677  push edx
ecx = (ecx) & (0xfffffffe); // 0x414678  and ecx, 0xfffffffe
edx = eax >> 31; // 0x41467b  cdq
ecx = (ecx) + (5); // 0x41467c  add ecx, 5
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x41467f  idiv ecx
edx = FD(0x7c); // 0x414681  mov edx, dword ptr [F+0x7c]
edx = (edx) - (eax); // 0x414688  sub edx, eax
stk[--sp] = (int)(edx); // 0x41468a  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x41468b
sp += 16; // 0x414690  add esp, 0x40  (caller stack cleanup)
goto La_4146e1; // 0x414693  jmp 0x4146e1
La_414695:;
ecx = FD(0x20); // 0x414695  mov ecx, dword ptr [F+0x20]
edx = FD(0xc); // 0x414699  mov edx, dword ptr [F+0xc]
stk[--sp] = (int)(ebp); // 0x41469d  push ebp
stk[--sp] = (int)(ecx); // 0x41469e  push ecx
stk[--sp] = (int)(eax); // 0x41469f  push eax
eax = FD(0x40); // 0x4146a0  mov eax, dword ptr [F+0x40]
ecx = FD(0x6c); // 0x4146a4  mov ecx, dword ptr [F+0x6c]
stk[--sp] = (int)(0x8c); // 0x4146a8  push 0x8c
edx = (edx) + (eax); // 0x4146ad  add edx, eax
stk[--sp] = (int)(0x8c); // 0x4146af  push 0x8c
stk[--sp] = (int)(edx); // 0x4146b4  push edx
edx = (int)(edi + esi*2); // 0x4146b5  lea edx, [edi + esi*2]
eax = esi; // 0x4146b8  mov eax, esi
stk[--sp] = (int)(ecx); // 0x4146ba  push ecx
eax = (eax) + (edx); // 0x4146bb  add eax, edx
ecx = 5; // 0x4146bd  mov ecx, 5
edx = eax >> 31; // 0x4146c2  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x4146c3  idiv ecx
ebx = FD(0x7c); // 0x4146c5  mov ebx, dword ptr [F+0x7c]
edx = (edx) - (2); // 0x4146cc  sub edx, 2
edx = (edx) * (G32(0x4c2844)); // 0x4146cf  imul edx, dword ptr [0x4c2844]
edx = (edx) + (ebx); // 0x4146d6  add edx, ebx
stk[--sp] = (int)(edx); // 0x4146d8  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x4146d9
sp += 8; // 0x4146de  add esp, 0x20  (caller stack cleanup)
La_4146e1:;
edx = FD(0x14); // 0x4146e1  mov edx, dword ptr [F+0x14]
// 0x4146e5  cmp byte ptr [edx + 0x5722e8], 0xf
if (((GU8(edx + 0x5722e8))&0xff) != ((0xf)&0xff)) goto La_41491d; // 0x4146ec  jne 0x41491d
eax = FD(0x28); // 0x4146f2  mov eax, dword ptr [F+0x28]
ebp = 0x197; // 0x4146f6  mov ebp, 0x197
// 0x4146fb  cmp eax, 4
ebx = 0xfffffff8; // 0x4146fe  mov ebx, 0xfffffff8
FD(0x10) = ebp; // 0x414703  mov dword ptr [F+0x10], ebp
if ((u32)(eax) > (u32)(4)) goto La_414731; // 0x414707  ja 0x414731
// 0x414709  jmp dword ptr [eax*4 + 0x42170c]  (switch table 0x42170c, eax=0..4 after ja default above)
switch (eax) { case 0: goto La_414710; case 1: goto La_41471c; case 2: goto La_414717; case 3: goto La_414723; case 4: goto La_414728; }
La_414710:;
ebp = 0x195; // 0x414710  mov ebp, 0x195
goto La_41472d; // 0x414715  jmp 0x41472d
La_414717:;
ebx = 0xfffffff7; // 0x414717  mov ebx, 0xfffffff7
La_41471c:;
ebp = 0x196; // 0x41471c  mov ebp, 0x196
goto La_41472d; // 0x414721  jmp 0x41472d
La_414723:;
ebx = 0xfffffff9; // 0x414723  mov ebx, 0xfffffff9
La_414728:;
ebp = 0x197; // 0x414728  mov ebp, 0x197
La_41472d:;
FD(0x10) = ebp; // 0x41472d  mov dword ptr [F+0x10], ebp
La_414731:;
eax = FD(0x14); // 0x414731  mov eax, dword ptr [F+0x14]
eax = (eax & ~0xffu) | ((GU8(eax + 0x56988c)) & 0xff); // 0x414735  mov al, byte ptr [eax + 0x56988c]
// 0x41473b  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_41475d; // 0x41473d  je 0x41475d
eax = (eax) & (0xff); // 0x41473f  and eax, 0xff
ecx = 3; // 0x414744  mov ecx, 3
edx = eax >> 31; // 0x414749  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x41474a  idiv ecx
ebx = 0xfffffff8; // 0x41474c  mov ebx, 0xfffffff8
ebp = edx; // 0x414751  mov ebp, edx
ebp = (ebp) + (0x195); // 0x414753  add ebp, 0x195
FD(0x10) = ebp; // 0x414759  mov dword ptr [F+0x10], ebp
La_41475d:;
ecx = FD(0x30); // 0x41475d  mov ecx, dword ptr [F+0x30]
edx = (int)(esi + esi); // 0x414761  lea edx, [esi + esi]
eax = edi; // 0x414764  mov eax, edi
eax = (eax) - (edx); // 0x414766  sub eax, edx
eax = (eax) & (3); // 0x414768  and eax, 3
// 0x41476b  cmp ecx, -1
stk[--sp] = (int)(eax); // 0x41476e  push eax
if ((ecx) == (-1)) goto La_4147bf; // 0x41476f  je 0x4147bf
ebx = ecx; // 0x414771  mov ebx, ecx
stk[--sp] = (int)(ebx); // 0x414773  push ebx
stk[--sp] = (int)(ebp); // 0x414774  push ebp
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x414775
G32(0x5a9cc8) = eax; // 0x41477a  mov dword ptr [0x5a9cc8], eax
eax = G32(ebp*4 + 0x53f3e8); // 0x41477f  mov eax, dword ptr [ebp*4 + 0x53f3e8]
sp += 3; // 0x414786  add esp, 0xc  (caller stack cleanup)
eax = (eax) - 1; // 0x414789  dec eax
// 0x41478a  cmp ebx, eax
if ((ebx) != (eax)) goto La_41479a; // 0x41478c  jne 0x41479a
eax = FD(0x14); // 0x41478e  mov eax, dword ptr [F+0x14]
G8(eax*2 + 0x53caf1) = (GU8(eax*2 + 0x53caf1)) & (0xbf); // 0x414792  and byte ptr [eax*2 + 0x53caf1], 0xbf
La_41479a:;
// 0x41479a  test ebx, ebx
if (((ebx)) != 0) goto La_4147b0; // 0x41479c  jne 0x4147b0
stk[--sp] = (int)(ebx); // 0x41479e  push ebx
stk[--sp] = (int)(ebx); // 0x41479f  push ebx
stk[--sp] = (int)(ebx); // 0x4147a0  push ebx
stk[--sp] = (int)(0x32); // 0x4147a1  push 0x32
stk[--sp] = (int)(0x92); // 0x4147a3  push 0x92
eax = CC(0x4481b0, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // playSound  0x4147a8
sp += 5; // 0x4147ad  add esp, 0x14  (caller stack cleanup)
La_4147b0:;
FD(0x30) = 0xfffffffe; // 0x4147b0  mov dword ptr [F+0x30], 0xfffffffe
ebx = 0xfffffff8; // 0x4147b8  mov ebx, 0xfffffff8
goto La_4147d6; // 0x4147bd  jmp 0x4147d6
La_4147bf:;
ecx = G32(ebp*4 + 0x53f3e8); // 0x4147bf  mov ecx, dword ptr [ebp*4 + 0x53f3e8]
ecx = (ecx) - 1; // 0x4147c6  dec ecx
stk[--sp] = (int)(ecx); // 0x4147c7  push ecx
stk[--sp] = (int)(ebp); // 0x4147c8  push ebp
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x4147c9
sp += 3; // 0x4147ce  add esp, 0xc  (caller stack cleanup)
G32(0x5a9cc8) = eax; // 0x4147d1  mov dword ptr [0x5a9cc8], eax
La_4147d6:;
edx = esi; // 0x4147d6  mov edx, esi
eax = edi; // 0x4147d8  mov eax, edi
edx = (edx) << (7); // 0x4147da  shl edx, 7
eax = (eax) << (7); // 0x4147dd  shl eax, 7
stk[--sp] = (int)(edx); // 0x4147e0  push edx
stk[--sp] = (int)(eax); // 0x4147e1  push eax
eax = CC(0x42dba0, stk[sp+0], stk[sp+1]); // sampleHeight  0x4147e2
edx = eax >> 31; // 0x4147e7  cdq
edx = (edx) & (0x1f); // 0x4147e8  and edx, 0x1f
sp += 2; // 0x4147eb  add esp, 8  (caller stack cleanup)
eax = (eax) + (edx); // 0x4147ee  add eax, edx
ebp = eax; // 0x4147f0  mov ebp, eax
eax = FD(0x10); // 0x4147f2  mov eax, dword ptr [F+0x10]
ebp = ((int)(ebp)) >> (5); // 0x4147f6  sar ebp, 5
ebp = (ebp) & (3); // 0x4147f9  and ebp, 3
ebp = (ebp) + (0x32); // 0x4147fc  add ebp, 0x32
// 0x4147ff  cmp eax, 0x195
if ((eax) != (0x195)) goto La_4148d0; // 0x414804  jne 0x4148d0
ecx = FD(0xc); // 0x41480a  mov ecx, dword ptr [F+0xc]
edx = FD(0x40); // 0x41480e  mov edx, dword ptr [F+0x40]
stk[--sp] = (int)(ebp); // 0x414812  push ebp
eax = (int)(ecx + edx); // 0x414813  lea eax, [ecx + edx]
edx = G32(0x5a9cc8); // 0x414816  mov edx, dword ptr [0x5a9cc8]
ecx = esi; // 0x41481c  mov ecx, esi
FD(0x34) = eax; // 0x41481e  mov dword ptr [F+0x34], eax
ecx = ((u32)(ecx)) >> (1); // 0x414822  shr ecx, 1
ecx = (ecx) & (1); // 0x414824  and ecx, 1
ecx = (ecx) + (ebx); // 0x414827  add ecx, ebx
stk[--sp] = (int)(ecx); // 0x414829  push ecx
stk[--sp] = (int)(edx); // 0x41482a  push edx
ecx = edi; // 0x41482b  mov ecx, edi
stk[--sp] = (int)(0x8c); // 0x41482d  push 0x8c
stk[--sp] = (int)(0x8c); // 0x414832  push 0x8c
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (1)) & 0xff); // 0x414837  and cl, 1
stk[--sp] = (int)(eax); // 0x41483a  push eax
eax = FD(0x6c); // 0x41483b  mov eax, dword ptr [F+0x6c]
ecx = (ecx & ~0xffu) | ((-((ecx & 0xff))) & 0xff); // 0x414842  neg cl
stk[--sp] = (int)(eax); // 0x414844  push eax
eax = G32(0x4c2840); // 0x414845  mov eax, dword ptr [0x4c2840]
ecx = -(int)((((ecx & 0xff)) != 0)); // 0x41484a  sbb ecx, ecx
ecx = (ecx) & (0xfffffffe); // 0x41484c  and ecx, 0xfffffffe
edx = eax >> 31; // 0x41484f  cdq
ecx = (ecx) + (5); // 0x414850  add ecx, 5
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414853  idiv ecx
eax = (eax) + (FD(0x7c)); // 0x414855  add eax, dword ptr [F+0x7c]
stk[--sp] = (int)(eax); // 0x41485c  push eax
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x41485d
edx = esi; // 0x414862  mov edx, esi
eax = G32(0x5a9cc8); // 0x414864  mov eax, dword ptr [0x5a9cc8]
edx = ~(edx); // 0x414869  not edx
edx = ((u32)(edx)) >> (1); // 0x41486b  shr edx, 1
ecx = FD(0x34); // 0x41486d  mov ecx, dword ptr [F+0x34]
edx = (edx) & (1); // 0x414871  and edx, 1
edx = (edx) + (ebx); // 0x414874  add edx, ebx
stk[--sp] = (int)(ebp); // 0x414876  push ebp
stk[--sp] = (int)(edx); // 0x414877  push edx
stk[--sp] = (int)(eax); // 0x414878  push eax
stk[--sp] = (int)(0x8c); // 0x414879  push 0x8c
ebp = G32(0x4c2840); // 0x41487e  mov ebp, dword ptr [0x4c2840]
stk[--sp] = (int)(0x8c); // 0x414884  push 0x8c
stk[--sp] = (int)(ecx); // 0x414889  push ecx
ecx = edi; // 0x41488a  mov ecx, edi
eax = ebp; // 0x41488c  mov eax, ebp
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (2)) & 0xff); // 0x41488e  and cl, 2
ecx = (ecx & ~0xffu) | ((-((ecx & 0xff))) & 0xff); // 0x414891  neg cl
ecx = -(int)((((ecx & 0xff)) != 0)); // 0x414893  sbb ecx, ecx
ecx = (ecx) & (0xa); // 0x414895  and ecx, 0xa
edx = eax >> 31; // 0x414898  cdq
ecx = (ecx) + (-6); // 0x414899  add ecx, -6
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x41489c  idiv ecx
edx = FD(0x6c); // 0x41489e  mov edx, dword ptr [F+0x6c]
ecx = esi; // 0x4148a5  mov ecx, esi
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (1)) & 0xff); // 0x4148a7  and cl, 1
edx = (edx) - (eax); // 0x4148aa  sub edx, eax
eax = ebp; // 0x4148ac  mov eax, ebp
ecx = (ecx & ~0xffu) | ((-((ecx & 0xff))) & 0xff); // 0x4148ae  neg cl
ecx = -(int)((((ecx & 0xff)) != 0)); // 0x4148b0  sbb ecx, ecx
stk[--sp] = (int)(edx); // 0x4148b2  push edx
ecx = (ecx) & (0xfffffffe); // 0x4148b3  and ecx, 0xfffffffe
edx = eax >> 31; // 0x4148b6  cdq
ecx = (ecx) + (5); // 0x4148b7  add ecx, 5
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x4148ba  idiv ecx
edx = FD(0x7c); // 0x4148bc  mov edx, dword ptr [F+0x7c]
edx = (edx) - (eax); // 0x4148c3  sub edx, eax
stk[--sp] = (int)(edx); // 0x4148c5  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x4148c6
sp += 16; // 0x4148cb  add esp, 0x40  (caller stack cleanup)
goto La_41491d; // 0x4148ce  jmp 0x41491d
La_4148d0:;
eax = G32(0x5a9cc8); // 0x4148d0  mov eax, dword ptr [0x5a9cc8]
ecx = FD(0xc); // 0x4148d5  mov ecx, dword ptr [F+0xc]
edx = FD(0x40); // 0x4148d9  mov edx, dword ptr [F+0x40]
stk[--sp] = (int)(ebp); // 0x4148dd  push ebp
stk[--sp] = (int)(ebx); // 0x4148de  push ebx
stk[--sp] = (int)(eax); // 0x4148df  push eax
eax = FD(0x6c); // 0x4148e0  mov eax, dword ptr [F+0x6c]
stk[--sp] = (int)(0x8c); // 0x4148e4  push 0x8c
ecx = (ecx) + (edx); // 0x4148e9  add ecx, edx
stk[--sp] = (int)(0x8c); // 0x4148eb  push 0x8c
stk[--sp] = (int)(ecx); // 0x4148f0  push ecx
stk[--sp] = (int)(eax); // 0x4148f1  push eax
ecx = (int)(edi + esi*2); // 0x4148f2  lea ecx, [edi + esi*2]
eax = esi; // 0x4148f5  mov eax, esi
eax = (eax) + (ecx); // 0x4148f7  add eax, ecx
ecx = 5; // 0x4148f9  mov ecx, 5
edx = eax >> 31; // 0x4148fe  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x4148ff  idiv ecx
ebx = FD(0x7c); // 0x414901  mov ebx, dword ptr [F+0x7c]
edx = (edx) - (2); // 0x414908  sub edx, 2
edx = (edx) * (G32(0x4c2844)); // 0x41490b  imul edx, dword ptr [0x4c2844]
edx = (edx) + (ebx); // 0x414912  add edx, ebx
stk[--sp] = (int)(edx); // 0x414914  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x414915
sp += 8; // 0x41491a  add esp, 0x20  (caller stack cleanup)
La_41491d:;
eax = FD(0x30); // 0x41491d  mov eax, dword ptr [F+0x30]
// 0x414921  test eax, eax
if (((int)(eax)) < 0) goto La_414931; // 0x414923  jl 0x414931
eax = FD(0x14); // 0x414925  mov eax, dword ptr [F+0x14]
G8(eax*2 + 0x53caf1) = (GU8(eax*2 + 0x53caf1)) & (0xbf); // 0x414929  and byte ptr [eax*2 + 0x53caf1], 0xbf
La_414931:;
edi = (edi) + 1; // 0x414931  inc edi
// 0x414932  cmp edi, 0x32
if ((int)(edi) < (int)(0x32)) goto La_413da7; // 0x414935  jl 0x413da7
esi = (esi) + 1; // 0x41493b  inc esi
// 0x41493c  cmp esi, 0x32
if ((int)(esi) < (int)(0x32)) goto La_413da5; // 0x41493f  jl 0x413da5
ecx = G32(0x4c1570); // 0x414945  mov ecx, dword ptr [0x4c1570]
esi = (esi) ^ (esi); // 0x41494b  xor esi, esi
stk[--sp] = (int)(esi); // 0x41494d  push esi
stk[--sp] = (int)(esi); // 0x41494e  push esi
stk[--sp] = (int)(esi); // 0x41494f  push esi
stk[--sp] = (int)(0x51b360); // 0x414950  push 0x51b360
eax = TC(0x4762d0, ecx, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3]); sp += 4; // setDrawTarget  0x414955
G32(0x58bcb0) = esi; // 0x41495a  mov dword ptr [0x58bcb0], esi
FD(0x14) = esi; // 0x414960  mov dword ptr [F+0x14], esi
La_414964:;
ebp = esi; // 0x414964  mov ebp, esi
ebp = (ebp) << (8); // 0x414966  shl ebp, 8
G32(ebp + 0x5794c0) = 0xffffffff; // 0x414969  mov dword ptr [ebp + 0x5794c0], 0xffffffff
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d9)) & 0xff); // 0x414973  mov al, byte ptr [ebp + 0x5794d9]
// 0x414979  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_415836; // 0x41497b  je 0x415836
// 0x414981  cmp al, 0xff
if ((((eax & 0xff))&0xff) == ((0xff)&0xff)) goto La_415836; // 0x414983  je 0x415836
stk[--sp] = (int)(esi); // 0x414989  push esi
eax = CC(0x46c940, stk[sp+0]); // typeBit7Clear  0x41498a
sp += 1; // 0x41498f  add esp, 4  (caller stack cleanup)
// 0x414992  test eax, eax
if (((eax)) != 0) goto La_4149f8; // 0x414994  jne 0x4149f8
ecx = (int)(G16(ebp + 0x57956e)); // 0x414996  movsx ecx, word ptr [ebp + 0x57956e]
ebx = 7; // 0x41499d  mov ebx, 7
eax = (int)(ecx*8); // 0x4149a2  lea eax, [ecx*8]
eax = (eax) - (ecx); // 0x4149a9  sub eax, ecx
eax = (int)(eax + eax*4); // 0x4149ab  lea eax, [eax + eax*4]
eax = (eax) << (4); // 0x4149ae  shl eax, 4
ecx = (ecx & ~0xffu) | ((GU8(eax + 0x4d60a9)) & 0xff); // 0x4149b1  mov cl, byte ptr [eax + 0x4d60a9]
// 0x4149b7  test cl, 1
if (((((((ecx & 0xff)) & (1)))&0xff)) == 0) goto La_4149c1; // 0x4149ba  je 0x4149c1
ebx = 6; // 0x4149bc  mov ebx, 6
La_4149c1:;
// 0x4149c1  test byte ptr [eax + 0x4d60a8], 8
if ((((((GU8(eax + 0x4d60a8)) & (8)))&0xff)) == 0) goto La_4149cf; // 0x4149c8  je 0x4149cf
ebx = 8; // 0x4149ca  mov ebx, 8
La_4149cf:;
// 0x4149cf  test cl, 4
if (((((((ecx & 0xff)) & (4)))&0xff)) == 0) goto La_4149d9; // 0x4149d2  je 0x4149d9
ebx = 5; // 0x4149d4  mov ebx, 5
La_4149d9:;
ecx = G32(eax + 0x4d60b4); // 0x4149d9  mov ecx, dword ptr [eax + 0x4d60b4]
// 0x4149df  test ecx, ecx
if (((ecx)) == 0) goto La_414a69; // 0x4149e1  je 0x414a69
ebx = (ebx & ~0xffu) | ((GU8(eax + 0x4d60ab)) & 0xff); // 0x4149e7  mov bl, byte ptr [eax + 0x4d60ab]
ebx = ((int)(ebx)) >> (4); // 0x4149ed  sar ebx, 4
ebx = (ebx) & (3); // 0x4149f0  and ebx, 3
ebx = (ebx) + (5); // 0x4149f3  add ebx, 5
goto La_414a69; // 0x4149f6  jmp 0x414a69
La_4149f8:;
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d1)) & 0xff); // 0x4149f8  mov al, byte ptr [ebp + 0x5794d1]
// 0x4149fe  test al, 4
if (((((((eax & 0xff)) & (4)))&0xff)) == 0) goto La_414a14; // 0x414a00  je 0x414a14
eax = (eax & ~0xffu) | ((~((eax & 0xff))) & 0xff); // 0x414a02  not al
eax = (eax) & (0xff); // 0x414a04  and eax, 0xff
eax = (eax) & (2); // 0x414a09  and eax, 2
eax = (eax & ~0xffu) | ((((eax & 0xff)) | (4)) & 0xff); // 0x414a0c  or al, 4
eax = ((u32)(eax)) >> (1); // 0x414a0e  shr eax, 1
ebx = eax; // 0x414a10  mov ebx, eax
goto La_414a19; // 0x414a12  jmp 0x414a19
La_414a14:;
ebx = 1; // 0x414a14  mov ebx, 1
La_414a19:;
ecx = (int)(G16(ebp + 0x57956e)); // 0x414a19  movsx ecx, word ptr [ebp + 0x57956e]
eax = (int)(ecx*8); // 0x414a20  lea eax, [ecx*8]
eax = (eax) - (ecx); // 0x414a27  sub eax, ecx
eax = (int)(eax + eax*4); // 0x414a29  lea eax, [eax + eax*4]
eax = (eax) << (4); // 0x414a2c  shl eax, 4
ecx = G32(eax + 0x4d60b4); // 0x414a2f  mov ecx, dword ptr [eax + 0x4d60b4]
// 0x414a35  test ecx, ecx
if (((ecx)) == 0) goto La_414a45; // 0x414a37  je 0x414a45
ebx = (ebx & ~0xffu) | ((GU8(eax + 0x4d60ab)) & 0xff); // 0x414a39  mov bl, byte ptr [eax + 0x4d60ab]
ebx = ((int)(ebx)) >> (4); // 0x414a3f  sar ebx, 4
ebx = (ebx) & (3); // 0x414a42  and ebx, 3
La_414a45:;
// 0x414a45  cmp byte ptr [ebp + 0x5794d0], 0x20
if (((GU8(ebp + 0x5794d0))&0xff) != ((0x20)&0xff)) goto La_414a69; // 0x414a4c  jne 0x414a69
ecx = (ecx) ^ (ecx); // 0x414a4e  xor ecx, ecx
ecx = (ecx & ~0xffu) | ((GU8(ebp + 0x579573)) & 0xff); // 0x414a50  mov cl, byte ptr [ebp + 0x579573]
edx = (int)(ecx*8); // 0x414a56  lea edx, [ecx*8]
edx = (edx) - (ecx); // 0x414a5d  sub edx, ecx
ebx = (ebx & ~0xffu) | ((GU8(edx*8 + 0x58dd70)) & 0xff); // 0x414a5f  mov bl, byte ptr [edx*8 + 0x58dd70]
ebx = (ebx) & (3); // 0x414a66  and ebx, 3
La_414a69:;
ecx = (ecx & ~0xffu) | ((GU8(eax + 0x4d60ab)) & 0xff); // 0x414a69  mov cl, byte ptr [eax + 0x4d60ab]
stk[--sp] = (int)(esi); // 0x414a6f  push esi
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) & (0xf)) & 0xff); // 0x414a70  and cl, 0xf
G8(eax + 0x4d60ab) = (ecx & 0xff); // 0x414a73  mov byte ptr [eax + 0x4d60ab], cl
eax = CC(0x46c940, stk[sp+0]); // typeBit7Clear  0x414a79
ecx = (int)(G16(ebp + 0x57956e)); // 0x414a7e  movsx ecx, word ptr [ebp + 0x57956e]
sp += 1; // 0x414a85  add esp, 4  (caller stack cleanup)
{ int testA = eax; // 0x414a88  test eax, eax: the jne at 0x414a91 reads typeBit7Clear's result, not the lea
eax = (int)(ecx*8); // 0x414a8a  lea eax, [ecx*8] (lea never touches the flags)
if (((testA)) != 0) goto La_414aab; } // 0x414a91  jne 0x414aab  (c3fy2 fix: was the lea result)
eax = (eax) - (ecx); // 0x414a93  sub eax, ecx
ecx = (ecx & ~0xffu) | (((ebx & 0xff)) & 0xff); // 0x414a95  mov cl, bl
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) - (5)) & 0xff); // 0x414a97  sub cl, 5
eax = (int)(eax + eax*4); // 0x414a9a  lea eax, [eax + eax*4]
eax = (eax) << (4); // 0x414a9d  shl eax, 4
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) << (4)) & 0xff); // 0x414aa0  shl cl, 4
eax = (int)(eax + 0x4d60ab); // 0x414aa3  lea eax, [eax + 0x4d60ab]
goto La_414abe; // 0x414aa9  jmp 0x414abe
La_414aab:;
eax = (eax) - (ecx); // 0x414aab  sub eax, ecx
ecx = (ecx & ~0xffu) | (((ebx & 0xff)) & 0xff); // 0x414aad  mov cl, bl
edx = (int)(eax + eax*4); // 0x414aaf  lea edx, [eax + eax*4]
edx = (edx) << (4); // 0x414ab2  shl edx, 4
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) << (4)) & 0xff); // 0x414ab5  shl cl, 4
eax = (int)(edx + 0x4d60ab); // 0x414ab8  lea eax, [edx + 0x4d60ab]
La_414abe:;
edx = (edx & ~0xffu) | ((GU8(eax)) & 0xff); // 0x414abe  mov dl, byte ptr [eax]
edx = (edx & ~0xffu) | ((((edx & 0xff)) | ((ecx & 0xff))) & 0xff); // 0x414ac0  or dl, cl
ecx = (ecx & ~0xffu) | ((GU8(ebp + 0x5794d5)) & 0xff); // 0x414ac2  mov cl, byte ptr [ebp + 0x5794d5]
G8(eax) = (edx & 0xff); // 0x414ac8  mov byte ptr [eax], dl
edx = (edx & ~0xffu) | ((GU8(ebp + 0x5794d2)) & 0xff); // 0x414aca  mov dl, byte ptr [ebp + 0x5794d2]
eax = G32(0x5685f4); // 0x414ad0  mov eax, dword ptr [0x5685f4]
eax = (eax) - (edx); // 0x414ad5  sub eax, edx
eax = (eax) - (2); // 0x414ad7  sub eax, 2
eax = (eax) & (7); // 0x414ada  and eax, 7
FD(0xc) = eax; // 0x414add  mov dword ptr [F+0xc], eax
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414ae1  mov al, byte ptr [ebp + 0x5794d6]
edx = eax; // 0x414ae7  mov edx, eax
edx = (edx) & (0xff); // 0x414ae9  and edx, 0xff
// 0x414aef  cmp cl, 7
FD(0x34) = edx; // 0x414af2  mov dword ptr [F+0x34], edx
if ((int)(s8)((ecx & 0xff)) >= (int)(s8)(7)) goto La_414e0e; // 0x414af6  jge 0x414e0e
eax = FD(0x14); // 0x414afc  mov eax, dword ptr [F+0x14]
FD(0x4c) = 0x28; // 0x414b00  mov dword ptr [F+0x4c], 0x28
stk[--sp] = (int)(eax); // 0x414b08  push eax
FD(0x48) = edx; // 0x414b09  mov dword ptr [F+0x48], edx
eax = CC(0x46c940, stk[sp+0]); // typeBit7Clear  0x414b0d
sp += 1; // 0x414b12  add esp, 4  (caller stack cleanup)
// 0x414b15  test eax, eax
if (((eax)) == 0) goto La_414b39; // 0x414b17  je 0x414b39
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d1)) & 0xff); // 0x414b19  mov al, byte ptr [ebp + 0x5794d1]
// 0x414b1f  test al, 2
if (((((((eax & 0xff)) & (2)))&0xff)) != 0) goto La_414b2d; // 0x414b21  jne 0x414b2d
FD(0x4c) = 0x32; // 0x414b23  mov dword ptr [F+0x4c], 0x32
goto La_414b39; // 0x414b2b  jmp 0x414b39
La_414b2d:;
// 0x414b2d  test al, 4
if (((((((eax & 0xff)) & (4)))&0xff)) != 0) goto La_414b39; // 0x414b2f  jne 0x414b39
FD(0x4c) = 0x37; // 0x414b31  mov dword ptr [F+0x4c], 0x37
La_414b39:;
ecx = (ecx & ~0xffffu) | ((GU16(ebp + 0x579564)) & 0xffff); // 0x414b39  mov cx, word ptr [ebp + 0x579564]
edx = (int)(G8(ebp + 0x5794d4)); // 0x414b40  movsx edx, byte ptr [ebp + 0x5794d4]
ecx = (ecx & ~0xffffu) | ((((ecx & 0xffff)) - (3)) & 0xffff); // 0x414b47  sub cx, 3
ecx = (ecx & ~0xffffu) | ((-((ecx & 0xffff))) & 0xffff); // 0x414b4b  neg cx
ecx = -(int)((((ecx & 0xffff)) != 0)); // 0x414b4e  sbb ecx, ecx
ecx = (ecx) & (0xfffffffe); // 0x414b50  and ecx, 0xfffffffe
ecx = (ecx) + (8); // 0x414b53  add ecx, 8
// 0x414b56  cmp edx, ecx
if ((int)(edx) <= (int)(ecx)) goto La_414b62; // 0x414b58  jle 0x414b62
FD(0x4c) = 0x50; // 0x414b5a  mov dword ptr [F+0x4c], 0x50
La_414b62:;
// 0x414b62  cmp byte ptr [ebp + 0x5794d8], 2
if ((int)(s8)(GU8(ebp + 0x5794d8)) > (int)(s8)(2)) goto La_414b72; // 0x414b69  jg 0x414b72
G8(ebp + 0x5794d6) = 0; // 0x414b6b  mov byte ptr [ebp + 0x5794d6], 0
La_414b72:;
// 0x414b72  cmp byte ptr [ebp + 0x5794d4], 0xd
if (((GU8(ebp + 0x5794d4))&0xff) != ((0xd)&0xff)) goto La_414b83; // 0x414b79  jne 0x414b83
FD(0x4c) = 0x5a; // 0x414b7b  mov dword ptr [F+0x4c], 0x5a
La_414b83:;
// 0x414b83  test byte ptr [0x59e7b8], 4
if ((((((GU8(0x59e7b8)) & (4)))&0xff)) != 0) goto La_414ca2; // 0x414b8a  jne 0x414ca2
// 0x414b90  cmp byte ptr [ebp + 0x5794d5], 6
if (((GU8(ebp + 0x5794d5))&0xff) == ((6)&0xff)) goto La_414caf; // 0x414b97  je 0x414caf
eax = G32(ebp + 0x5794c8); // 0x414b9d  mov eax, dword ptr [ebp + 0x5794c8]
// 0x414ba3  test eax, 0x8000000
if (((((eax) & (0x8000000)))) == 0) goto La_414be0; // 0x414ba8  je 0x414be0
ecx = (ecx & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414baa  mov cl, byte ptr [ebp + 0x5794d6]
// 0x414bb0  test cl, cl
if (((((ecx & 0xff))&0xff)) == 0) goto La_414bbe; // 0x414bb2  jbe 0x414bbe
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) - 1) & 0xff); // 0x414bb4  dec cl
G8(ebp + 0x5794d6) = (ecx & 0xff); // 0x414bb6  mov byte ptr [ebp + 0x5794d6], cl
goto La_414bc9; // 0x414bbc  jmp 0x414bc9
La_414bbe:;
eax = (eax) & (0xf7ffffff); // 0x414bbe  and eax, 0xf7ffffff
G32(ebp + 0x5794c8) = eax; // 0x414bc3  mov dword ptr [ebp + 0x5794c8], eax
La_414bc9:;
eax = (eax) ^ (eax); // 0x414bc9  xor eax, eax
stk[--sp] = (int)(0x63); // 0x414bcb  push 0x63
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414bcd  mov al, byte ptr [ebp + 0x5794d6]
stk[--sp] = (int)(0); // 0x414bd3  push 0
stk[--sp] = (int)(eax); // 0x414bd5  push eax
eax = CC(0x467130, stk[sp+0], stk[sp+1], stk[sp+2]); // clamp  0x414bd6
sp += 3; // 0x414bdb  add esp, 0xc  (caller stack cleanup)
goto La_414c3a; // 0x414bde  jmp 0x414c3a
La_414be0:;
// 0x414be0  cmp byte ptr [ebp + 0x5794d6], 3
if (((GU8(ebp + 0x5794d6))&0xff) != ((3)&0xff)) goto La_414c25; // 0x414be7  jne 0x414c25
stk[--sp] = (int)(2); // 0x414be9  push 2
ecx = 0x822d9c; // 0x414beb  mov ecx, 0x822d9c
eax = Rnd(stk[sp]); sp += 1; // 0x414bf0  call 0x45c1e0
// 0x414bf5  test ax, ax
if (((((eax & 0xffff))&0xffff)) == 0) goto La_414c25; // 0x414bf8  je 0x414c25
// 0x414bfa  cmp byte ptr [ebp + 0x5794d4], 3
if ((int)(s8)(GU8(ebp + 0x5794d4)) > (int)(s8)(3)) goto La_414c25; // 0x414c01  jg 0x414c25
eax = G32(ebp + 0x5794c8); // 0x414c03  mov eax, dword ptr [ebp + 0x5794c8]
// 0x414c09  test eax, 0x400000
if (((((eax) & (0x400000)))) != 0) goto La_414c25; // 0x414c0e  jne 0x414c25
eax = (eax) | (0x8000000); // 0x414c10  or eax, 0x8000000
G32(ebp + 0x5794c8) = eax; // 0x414c15  mov dword ptr [ebp + 0x5794c8], eax
eax = (eax) ^ (eax); // 0x414c1b  xor eax, eax
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414c1d  mov al, byte ptr [ebp + 0x5794d6]
goto La_414c3a; // 0x414c23  jmp 0x414c3a
La_414c25:;
edx = (edx & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414c25  mov dl, byte ptr [ebp + 0x5794d6]
edx = (edx & ~0xffu) | ((((edx & 0xff)) + 1) & 0xff); // 0x414c2b  inc dl
eax = (eax & ~0xffu) | (((edx & 0xff)) & 0xff); // 0x414c2d  mov al, dl
G8(ebp + 0x5794d6) = (edx & 0xff); // 0x414c2f  mov byte ptr [ebp + 0x5794d6], dl
eax = (eax) & (0xff); // 0x414c35  and eax, 0xff
La_414c3a:;
ecx = FD(0x4c); // 0x414c3a  mov ecx, dword ptr [F+0x4c]
FD(0x48) = eax; // 0x414c3e  mov dword ptr [F+0x48], eax
ecx = (ecx) + (ebx); // 0x414c42  add ecx, ebx
ecx = G32(ecx*4 + 0x53f3e8); // 0x414c44  mov ecx, dword ptr [ecx*4 + 0x53f3e8]
// 0x414c4b  cmp eax, ecx
if ((int)(eax) < (int)(ecx)) goto La_414c5c; // 0x414c4d  jl 0x414c5c
eax = (int)(ecx + -1); // 0x414c4f  lea eax, [ecx - 1]
FD(0x48) = eax; // 0x414c52  mov dword ptr [F+0x48], eax
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x414c56  mov byte ptr [ebp + 0x5794d6], al
La_414c5c:;
esi = FD(0x4c); // 0x414c5c  mov esi, dword ptr [F+0x4c]
eax = (eax) ^ (eax); // 0x414c60  xor eax, eax
// 0x414c62  cmp esi, 0x5a
eax = (eax & ~0xffu) | ((((esi) != (0x5a)) ? 1 : 0) & 0xff); // 0x414c65  setne al
eax = (eax) + (5); // 0x414c68  add eax, 5
eax = (eax) * (ecx); // 0x414c6b  imul eax, ecx
edx = eax >> 31; // 0x414c6e  cdq
edx = (edx) & (7); // 0x414c6f  and edx, 7
edx = (edx) + (eax); // 0x414c72  add edx, eax
eax = FD(0x48); // 0x414c74  mov eax, dword ptr [F+0x48]
edx = ((int)(edx)) >> (3); // 0x414c78  sar edx, 3
// 0x414c7b  cmp eax, edx
if ((int)(eax) >= (int)(edx)) goto La_414c8e; // 0x414c7d  jge 0x414c8e
// 0x414c7f  cmp byte ptr [ebp + 0x5794d5], 4
if (((GU8(ebp + 0x5794d5))&0xff) != ((4)&0xff)) goto La_414c8e; // 0x414c86  jne 0x414c8e
G8(ebp + 0x5794d8) = (GU8(ebp + 0x5794d8)) - 1; // 0x414c88  dec byte ptr [ebp + 0x5794d8]
La_414c8e:;
ecx = (ecx) - 1; // 0x414c8e  dec ecx
// 0x414c8f  cmp eax, ecx
if ((int)(eax) >= (int)(ecx)) goto La_414ca2; // 0x414c91  jge 0x414ca2
// 0x414c93  cmp byte ptr [ebp + 0x5794d5], 5
if (((GU8(ebp + 0x5794d5))&0xff) != ((5)&0xff)) goto La_414ca2; // 0x414c9a  jne 0x414ca2
G8(ebp + 0x5794d8) = (GU8(ebp + 0x5794d8)) - 1; // 0x414c9c  dec byte ptr [ebp + 0x5794d8]
La_414ca2:;
// 0x414ca2  cmp byte ptr [ebp + 0x5794d5], 6
if (((GU8(ebp + 0x5794d5))&0xff) != ((6)&0xff)) goto La_414df0; // 0x414ca9  jne 0x414df0
La_414caf:;
eax = G32(ebp + 0x579590); // 0x414caf  mov eax, dword ptr [ebp + 0x579590]
ecx = G32(ebp + 0x5794bc); // 0x414cb5  mov ecx, dword ptr [ebp + 0x5794bc]
esi = G32(ebp + 0x5794b8); // 0x414cbb  mov esi, dword ptr [ebp + 0x5794b8]
eax = (eax) - (ecx); // 0x414cc1  sub eax, ecx
ecx = G32(ebp + 0x57958c); // 0x414cc3  mov ecx, dword ptr [ebp + 0x57958c]
stk[--sp] = (int)(eax); // 0x414cc9  push eax
ecx = (ecx) - (esi); // 0x414cca  sub ecx, esi
stk[--sp] = (int)(ecx); // 0x414ccc  push ecx
eax = CC(0x40acd0, stk[sp+0], stk[sp+1]); // distance  0x414ccd
edx = G32(ebp + 0x5794c8); // 0x414cd2  mov edx, dword ptr [ebp + 0x5794c8]
sp += 2; // 0x414cd8  add esp, 8  (caller stack cleanup)
edx = (edx & ~0xffu) | ((((edx & 0xff)) & (0x60)) & 0xff); // 0x414cdb  and dl, 0x60
edx = (edx & ~0xffu) | ((-((edx & 0xff))) & 0xff); // 0x414cde  neg dl
edx = -(int)((((edx & 0xff)) != 0)); // 0x414ce0  sbb edx, edx
edx = (edx) & (0xfffffffe); // 0x414ce2  and edx, 0xfffffffe
edx = (edx) + (5); // 0x414ce5  add edx, 5
edx = (edx) << (0xa); // 0x414ce8  shl edx, 0xa
// 0x414ceb  cmp eax, edx
if ((int)(eax) <= (int)(edx)) goto La_414dde; // 0x414ced  jle 0x414dde
edx = (edx & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414cf3  mov dl, byte ptr [ebp + 0x5794d6]
FD(0x4c) = 0xaa; // 0x414cf9  mov dword ptr [F+0x4c], 0xaa
edx = (edx & ~0xffu) | ((((edx & 0xff)) + 1) & 0xff); // 0x414d01  inc dl
eax = (eax & ~0xffu) | (((edx & 0xff)) & 0xff); // 0x414d03  mov al, dl
G8(ebp + 0x5794d6) = (edx & 0xff); // 0x414d05  mov byte ptr [ebp + 0x5794d6], dl
eax = (eax) & (0xff); // 0x414d0b  and eax, 0xff
ecx = eax; // 0x414d10  mov ecx, eax
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d2)) & 0xff); // 0x414d12  mov al, byte ptr [ebp + 0x5794d2]
edx = (edx & ~0xffu) | (((eax & 0xff)) & 0xff); // 0x414d18  mov dl, al
eax = (eax & ~0xffu) | ((((eax & 0xff)) & (1)) & 0xff); // 0x414d1a  and al, 1
edx = (edx) & (7); // 0x414d1c  and edx, 7
FD(0x48) = ecx; // 0x414d1f  mov dword ptr [F+0x48], ecx
eax = (eax & ~0xffu) | ((-((eax & 0xff))) & 0xff); // 0x414d23  neg al
edx = G32(edx*4 + 0x4c2878); // 0x414d25  mov edx, dword ptr [edx*4 + 0x4c2878]
eax = -(int)((((eax & 0xff)) != 0)); // 0x414d2c  sbb eax, eax
eax = (eax & ~0xffu) | ((((eax & 0xff)) & (0xfe)) & 0xff); // 0x414d2e  and al, 0xfe
eax = (eax) + (5); // 0x414d30  add eax, 5
eax = (eax) << (6); // 0x414d33  shl eax, 6
edx = (edx) * (eax); // 0x414d36  imul edx, eax
eax = G32(ebp + 0x57957c); // 0x414d39  mov eax, dword ptr [ebp + 0x57957c]
eax = (eax) - (edx); // 0x414d3f  sub eax, edx
G32(ebp + 0x5794b8) = eax; // 0x414d41  mov dword ptr [ebp + 0x5794b8], eax
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d2)) & 0xff); // 0x414d47  mov al, byte ptr [ebp + 0x5794d2]
edx = (edx & ~0xffu) | (((eax & 0xff)) & 0xff); // 0x414d4d  mov dl, al
eax = (eax & ~0xffu) | ((((eax & 0xff)) & (1)) & 0xff); // 0x414d4f  and al, 1
edx = (edx) & (7); // 0x414d51  and edx, 7
eax = (eax & ~0xffu) | ((-((eax & 0xff))) & 0xff); // 0x414d54  neg al
edx = G32(edx*4 + 0x4c2898); // 0x414d56  mov edx, dword ptr [edx*4 + 0x4c2898]
eax = -(int)((((eax & 0xff)) != 0)); // 0x414d5d  sbb eax, eax
eax = (eax & ~0xffu) | ((((eax & 0xff)) & (0xfe)) & 0xff); // 0x414d5f  and al, 0xfe
eax = (eax) + (5); // 0x414d61  add eax, 5
eax = (eax) << (6); // 0x414d64  shl eax, 6
edx = (edx) * (eax); // 0x414d67  imul edx, eax
eax = G32(ebp + 0x579580); // 0x414d6a  mov eax, dword ptr [ebp + 0x579580]
eax = (eax) - (edx); // 0x414d70  sub eax, edx
G32(ebp + 0x5794bc) = eax; // 0x414d72  mov dword ptr [ebp + 0x5794bc], eax
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794c8)) & 0xff); // 0x414d78  mov al, byte ptr [ebp + 0x5794c8]
// 0x414d7e  test al, 0x20
if (((((((eax & 0xff)) & (0x20)))&0xff)) == 0) goto La_414da2; // 0x414d80  je 0x414da2
// 0x414d82  cmp ecx, 0x14
FD(0x4c) = 0xa0; // 0x414d85  mov dword ptr [F+0x4c], 0xa0
if ((ecx) != (0x14)) goto La_414da2; // 0x414d8d  jne 0x414da2
FD(0x48) = 6; // 0x414d8f  mov dword ptr [F+0x48], 6
G8(ebp + 0x5794d6) = 6; // 0x414d97  mov byte ptr [ebp + 0x5794d6], 6
ecx = FD(0x48); // 0x414d9e  mov ecx, dword ptr [F+0x48]
La_414da2:;
// 0x414da2  test byte ptr [ebp + 0x5794c8], 0x40
if ((((((GU8(ebp + 0x5794c8)) & (0x40)))&0xff)) == 0) goto La_414dcb; // 0x414da9  je 0x414dcb
// 0x414dab  cmp ecx, 0x14
FD(0x4c) = 0x96; // 0x414dae  mov dword ptr [F+0x4c], 0x96
if ((ecx) != (0x14)) goto La_414dcb; // 0x414db6  jne 0x414dcb
FD(0x48) = 6; // 0x414db8  mov dword ptr [F+0x48], 6
G8(ebp + 0x5794d6) = 6; // 0x414dc0  mov byte ptr [ebp + 0x5794d6], 6
ecx = FD(0x48); // 0x414dc7  mov ecx, dword ptr [F+0x48]
La_414dcb:;
edx = FD(0x4c); // 0x414dcb  mov edx, dword ptr [F+0x4c]
edx = (edx) + (ebx); // 0x414dcf  add edx, ebx
eax = G32(edx*4 + 0x53f3e8); // 0x414dd1  mov eax, dword ptr [edx*4 + 0x53f3e8]
// 0x414dd8  cmp ecx, eax
if ((int)(ecx) < (int)(eax)) goto La_414df0; // 0x414dda  jl 0x414df0
goto La_414deb; // 0x414ddc  jmp 0x414deb
La_414dde:;
eax = FD(0x4c); // 0x414dde  mov eax, dword ptr [F+0x4c]
eax = (eax) + (ebx); // 0x414de2  add eax, ebx
eax = G32(eax*4 + 0x53f3e8); // 0x414de4  mov eax, dword ptr [eax*4 + 0x53f3e8]
La_414deb:;
eax = (eax) - 1; // 0x414deb  dec eax
FD(0x48) = eax; // 0x414dec  mov dword ptr [F+0x48], eax
La_414df0:;
// 0x414df0  cmp dword ptr [F+0x4c], 0x5a
if ((FD(0x4c)) == (0x5a)) goto La_41530d; // 0x414df5  je 0x41530d
ecx = FD(0xc); // 0x414dfb  mov ecx, dword ptr [F+0xc]
ecx = (ecx) + (2); // 0x414dff  add ecx, 2
ecx = (ecx) & (7); // 0x414e02  and ecx, 7
FD(0xc) = ecx; // 0x414e05  mov dword ptr [F+0xc], ecx
goto La_41530d; // 0x414e09  jmp 0x41530d
La_414e0e:;
// 0x414e0e  cmp cl, 0xb
if ((int)(s8)((ecx & 0xff)) >= (int)(s8)(0xb)) goto La_414e91; // 0x414e11  jge 0x414e91
// 0x414e13  cmp word ptr [ebp + 0x579562], 0xa0
FD(0x4c) = 0; // 0x414e1c  mov dword ptr [F+0x4c], 0
if ((int)(short)(GU16(ebp + 0x579562)) <= (int)(short)(0xa0)) goto La_414e2e; // 0x414e24  jle 0x414e2e
FD(0x4c) = 0xd2; // 0x414e26  mov dword ptr [F+0x4c], 0xd2
La_414e2e:;
ecx = G32(ebp + 0x5794c8); // 0x414e2e  mov ecx, dword ptr [ebp + 0x5794c8]
// 0x414e34  test ch, 0x80
if ((((((((ecx>>8)&0xff)) & (0x80)))&0xff)) == 0) goto La_414e41; // 0x414e37  je 0x414e41
FD(0x4c) = 0xe6; // 0x414e39  mov dword ptr [F+0x4c], 0xe6
La_414e41:;
edx = FD(0x4c); // 0x414e41  mov edx, dword ptr [F+0x4c]
ecx = (ecx & ~0xffu) | (((eax & 0xff)) & 0xff); // 0x414e45  mov cl, al
ecx = (ecx & ~0xffu) | ((((ecx & 0xff)) + 1) & 0xff); // 0x414e47  inc cl
edx = (edx) + (ebx); // 0x414e49  add edx, ebx
eax = ecx; // 0x414e4b  mov eax, ecx
G8(ebp + 0x5794d6) = (ecx & 0xff); // 0x414e4d  mov byte ptr [ebp + 0x5794d6], cl
esi = G32(edx*4 + 0x53f3e8); // 0x414e53  mov esi, dword ptr [edx*4 + 0x53f3e8]
eax = (eax) & (0xff); // 0x414e5a  and eax, 0xff
edx = eax >> 31; // 0x414e5f  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(esi); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414e60  idiv esi
// 0x414e62  test dword ptr [ebp + 0x5794c8], 0x80000
FD(0x48) = edx; // 0x414e6c  mov dword ptr [F+0x48], edx
if (((((G32(ebp + 0x5794c8)) & (0x80000)))) != 0) goto La_41530d; // 0x414e70  jne 0x41530d
eax = (eax & ~0xffu) | (((ecx & 0xff)) & 0xff); // 0x414e76  mov al, cl
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x414e78  inc al
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x414e7a  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x414e80  and eax, 0xff
edx = eax >> 31; // 0x414e85  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(esi); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414e86  idiv esi
FD(0x48) = edx; // 0x414e88  mov dword ptr [F+0x48], edx
goto La_41530d; // 0x414e8c  jmp 0x41530d
La_414e91:;
// 0x414e91 is reached only from the jge at 0x414e11, so its jne reads the flags of `cmp cl, 0xb` at 0x414e0e (ecx is
// unchanged on that edge): taken when cl != 0xb, i.e. cl > 0xb (c3fy2 fix: was `eax != 0`)
if (((ecx & 0xff)) != (0xb)) goto La_414f43; // 0x414e91  jne 0x414f43
edx = (edx & ~0xffu) | ((GU8(ebp + 0x5794d4)) & 0xff); // 0x414e97  mov dl, byte ptr [ebp + 0x5794d4]
ecx = 0xa; // 0x414e9d  mov ecx, 0xa
// 0x414ea2  cmp dl, 0xd
FD(0x4c) = ecx; // 0x414ea5  mov dword ptr [F+0x4c], ecx
if ((((edx & 0xff))&0xff) != ((0xd)&0xff)) goto La_414eb7; // 0x414ea9  jne 0x414eb7
FD(0x4c) = 0x64; // 0x414eab  mov dword ptr [F+0x4c], 0x64
ecx = FD(0x4c); // 0x414eb3  mov ecx, dword ptr [F+0x4c]
La_414eb7:;
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x414eb7  inc al
esi = (int)(ecx + ebx); // 0x414eb9  lea esi, [ecx + ebx]
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x414ebc  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x414ec2  and eax, 0xff
edx = eax >> 31; // 0x414ec7  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(G32(esi*4 + 0x53f3e8)); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414ec8  idiv dword ptr [esi*4 + 0x53f3e8]
// 0x414ecf  cmp ecx, 0xa
FD(0x48) = edx; // 0x414ed2  mov dword ptr [F+0x48], edx
if ((ecx) != (0xa)) goto La_414f23; // 0x414ed6  jne 0x414f23
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794da)) & 0xff); // 0x414ed8  mov al, byte ptr [ebp + 0x5794da]
// 0x414ede  test al, al
if (((((eax & 0xff))&0xff)) != 0) goto La_414f23; // 0x414ee0  jne 0x414f23
edx = FD(0x14); // 0x414ee2  mov edx, dword ptr [F+0x14]
ecx = G32(0x834170); // 0x414ee6  mov ecx, dword ptr [0x834170]
ecx = (ecx) + (edx); // 0x414eec  add ecx, edx
eax = (int)(edx + edx*8); // 0x414eee  lea eax, [edx + edx*8]
eax = (int)(ecx + eax*4); // 0x414ef1  lea eax, [ecx + eax*4]
ecx = G32(ebx*4 + 0x53f438); // 0x414ef4  mov ecx, dword ptr [ebx*4 + 0x53f438]
eax = (eax) & (0x7f); // 0x414efb  and eax, 0x7f
// 0x414efe  cmp eax, ecx
if ((int)(eax) >= (int)(ecx)) goto La_414f27; // 0x414f00  jge 0x414f27
// 0x414f02  cmp word ptr [ebp + 0x579556], 0
FD(0x48) = eax; // 0x414f0a  mov dword ptr [F+0x48], eax
FD(0x4c) = 0x14; // 0x414f0e  mov dword ptr [F+0x4c], 0x14
if (((GU16(ebp + 0x579556))&0xffff) != ((0)&0xffff)) goto La_414f27; // 0x414f16  jne 0x414f27
G16(ebp + 0x579556) = 0xfffe; // 0x414f18  mov word ptr [ebp + 0x579556], 0xfffe
goto La_414f27; // 0x414f21  jmp 0x414f27
La_414f23:;
edx = FD(0x14); // 0x414f23  mov edx, dword ptr [F+0x14]
La_414f27:;
// 0x414f27  cmp byte ptr [ebp + 0x5794d4], 0xd
if (((GU8(ebp + 0x5794d4))&0xff) != ((0xd)&0xff)) goto La_414f47; // 0x414f2e  jne 0x414f47
eax = FD(0x48); // 0x414f30  mov eax, dword ptr [F+0x48]
// 0x414f34  test eax, eax
if (((eax)) != 0) goto La_414f47; // 0x414f36  jne 0x414f47
G16(ebp + 0x579556) = 0; // 0x414f38  mov word ptr [ebp + 0x579556], 0
goto La_414f47; // 0x414f41  jmp 0x414f47
La_414f43:;
edx = FD(0x14); // 0x414f43  mov edx, dword ptr [F+0x14]
La_414f47:;
// 0x414f47  cmp byte ptr [ebp + 0x5794d5], 0xc
if (((GU8(ebp + 0x5794d5))&0xff) != ((0xc)&0xff)) goto La_41501d; // 0x414f4e  jne 0x41501d
ecx = G32(ebp + 0x5794c8); // 0x414f54  mov ecx, dword ptr [ebp + 0x5794c8]
ecx = (ecx) & (0x20000); // 0x414f5a  and ecx, 0x20000
ecx = -(ecx); // 0x414f60  neg ecx
ecx = -(int)(((ecx) != 0)); // 0x414f62  sbb ecx, ecx
ecx = (ecx) & (0x96); // 0x414f64  and ecx, 0x96
ecx = (ecx) + (0x1e); // 0x414f6a  add ecx, 0x1e
edx = (edx) & (1); // 0x414f6d  and edx, 1
FD(0x4c) = ecx; // 0x414f70  mov dword ptr [F+0x4c], ecx
FD(0x18) = edx; // 0x414f74  mov dword ptr [F+0x18], edx
if (((edx)) == 0) goto La_414fba; // 0x414f78  je 0x414fba
// 0x414f7a  test byte ptr [0x834170], 1
if ((((((GU8(0x834170)) & (1)))&0xff)) != 0) goto La_414fba; // 0x414f81  jne 0x414fba
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414f83  mov al, byte ptr [ebp + 0x5794d6]
// 0x414f89  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_414fba; // 0x414f8b  je 0x414fba
// 0x414f8d  test byte ptr [0x59e7b8], 8
if ((((((GU8(0x59e7b8)) & (8)))&0xff)) != 0) goto La_414fba; // 0x414f94  jne 0x414fba
edx = (int)(ecx + ebx); // 0x414f96  lea edx, [ecx + ebx]
esi = eax; // 0x414f99  mov esi, eax
esi = (esi) & (0xff); // 0x414f9b  and esi, 0xff
edi = G32(edx*4 + 0x53f3e8); // 0x414fa1  mov edi, dword ptr [edx*4 + 0x53f3e8]
eax = edi; // 0x414fa8  mov eax, edi
edx = eax >> 31; // 0x414faa  cdq
eax = (eax) - (edx); // 0x414fab  sub eax, edx
eax = ((int)(eax)) >> (1); // 0x414fad  sar eax, 1
// 0x414faf  cmp esi, eax
if ((int)(esi) > (int)(eax)) goto La_414fba; // 0x414fb1  jg 0x414fba
eax = esi; // 0x414fb3  mov eax, esi
edx = eax >> 31; // 0x414fb5  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(edi); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414fb6  idiv edi
goto La_414fd7; // 0x414fb8  jmp 0x414fd7
La_414fba:;
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x414fba  mov al, byte ptr [ebp + 0x5794d6]
ecx = (ecx) + (ebx); // 0x414fc0  add ecx, ebx
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x414fc2  inc al
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x414fc4  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x414fca  and eax, 0xff
edx = eax >> 31; // 0x414fcf  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(G32(ecx*4 + 0x53f3e8)); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x414fd0  idiv dword ptr [ecx*4 + 0x53f3e8]
La_414fd7:;
eax = edx; // 0x414fd7  mov eax, edx
FD(0x48) = edx; // 0x414fd9  mov dword ptr [F+0x48], edx
// 0x414fdd  test eax, eax
if (((eax)) != 0) goto La_41501d; // 0x414fdf  jne 0x41501d
eax = G32(ebp + 0x5794c8); // 0x414fe1  mov eax, dword ptr [ebp + 0x5794c8]
// 0x414fe7  test eax, 0x8000000
if (((((eax) & (0x8000000)))) != 0) goto La_415014; // 0x414fec  jne 0x415014
ecx = FD(0x18); // 0x414fee  mov ecx, dword ptr [F+0x18]
// 0x414ff2  test ecx, ecx
if (((ecx)) != 0) goto La_415014; // 0x414ff4  jne 0x415014
eax = (eax) | (0x8000000); // 0x414ff6  or eax, 0x8000000
G32(ebp + 0x5794c8) = eax; // 0x414ffb  mov dword ptr [ebp + 0x5794c8], eax
G8(ebp + 0x5794d6) = 0; // 0x415001  mov byte ptr [ebp + 0x5794d6], 0
G32(ebp + 0x5794c8) = (G32(ebp + 0x5794c8)) ^ (0x20000); // 0x415008  xor dword ptr [ebp + 0x5794c8], 0x20000
goto La_41501d; // 0x415012  jmp 0x41501d
La_415014:;
G16(ebp + 0x579556) = 0; // 0x415014  mov word ptr [ebp + 0x579556], 0
La_41501d:;
// 0x41501d  cmp byte ptr [ebp + 0x5794d5], 0xd
if (((GU8(ebp + 0x5794d5))&0xff) != ((0xd)&0xff)) goto La_4150f7; // 0x415024  jne 0x4150f7
ecx = G32(ebp + 0x5794c8); // 0x41502a  mov ecx, dword ptr [ebp + 0x5794c8]
eax = FD(0x14); // 0x415030  mov eax, dword ptr [F+0x14]
ecx = (ecx) & (0x20000); // 0x415034  and ecx, 0x20000
ecx = -(ecx); // 0x41503a  neg ecx
ecx = -(int)(((ecx) != 0)); // 0x41503c  sbb ecx, ecx
ecx = (ecx) & (0xa); // 0x41503e  and ecx, 0xa
ecx = (ecx) + (0xbe); // 0x415041  add ecx, 0xbe
eax = (eax) & (1); // 0x415047  and eax, 1
FD(0x4c) = ecx; // 0x41504a  mov dword ptr [F+0x4c], ecx
FD(0x18) = eax; // 0x41504e  mov dword ptr [F+0x18], eax
if (((eax)) == 0) goto La_415094; // 0x415052  je 0x415094
// 0x415054  test byte ptr [0x834170], 1
if ((((((GU8(0x834170)) & (1)))&0xff)) != 0) goto La_415094; // 0x41505b  jne 0x415094
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x41505d  mov al, byte ptr [ebp + 0x5794d6]
// 0x415063  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_415094; // 0x415065  je 0x415094
// 0x415067  test byte ptr [0x59e7b8], 8
if ((((((GU8(0x59e7b8)) & (8)))&0xff)) != 0) goto La_415094; // 0x41506e  jne 0x415094
edx = (int)(ecx + ebx); // 0x415070  lea edx, [ecx + ebx]
esi = eax; // 0x415073  mov esi, eax
esi = (esi) & (0xff); // 0x415075  and esi, 0xff
edi = G32(edx*4 + 0x53f3e8); // 0x41507b  mov edi, dword ptr [edx*4 + 0x53f3e8]
eax = edi; // 0x415082  mov eax, edi
edx = eax >> 31; // 0x415084  cdq
eax = (eax) - (edx); // 0x415085  sub eax, edx
eax = ((int)(eax)) >> (1); // 0x415087  sar eax, 1
// 0x415089  cmp esi, eax
if ((int)(esi) > (int)(eax)) goto La_415094; // 0x41508b  jg 0x415094
eax = esi; // 0x41508d  mov eax, esi
edx = eax >> 31; // 0x41508f  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(edi); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x415090  idiv edi
goto La_4150b1; // 0x415092  jmp 0x4150b1
La_415094:;
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x415094  mov al, byte ptr [ebp + 0x5794d6]
ecx = (ecx) + (ebx); // 0x41509a  add ecx, ebx
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x41509c  inc al
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x41509e  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x4150a4  and eax, 0xff
edx = eax >> 31; // 0x4150a9  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(G32(ecx*4 + 0x53f3e8)); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x4150aa  idiv dword ptr [ecx*4 + 0x53f3e8]
La_4150b1:;
eax = edx; // 0x4150b1  mov eax, edx
FD(0x48) = edx; // 0x4150b3  mov dword ptr [F+0x48], edx
// 0x4150b7  test eax, eax
if (((eax)) != 0) goto La_4150f7; // 0x4150b9  jne 0x4150f7
eax = G32(ebp + 0x5794c8); // 0x4150bb  mov eax, dword ptr [ebp + 0x5794c8]
// 0x4150c1  test eax, 0x8000000
if (((((eax) & (0x8000000)))) != 0) goto La_4150ee; // 0x4150c6  jne 0x4150ee
ecx = FD(0x18); // 0x4150c8  mov ecx, dword ptr [F+0x18]
// 0x4150cc  test ecx, ecx
if (((ecx)) != 0) goto La_4150ee; // 0x4150ce  jne 0x4150ee
eax = (eax) | (0x8000000); // 0x4150d0  or eax, 0x8000000
G32(ebp + 0x5794c8) = eax; // 0x4150d5  mov dword ptr [ebp + 0x5794c8], eax
G8(ebp + 0x5794d6) = 0; // 0x4150db  mov byte ptr [ebp + 0x5794d6], 0
G32(ebp + 0x5794c8) = (G32(ebp + 0x5794c8)) ^ (0x20000); // 0x4150e2  xor dword ptr [ebp + 0x5794c8], 0x20000
goto La_4150f7; // 0x4150ec  jmp 0x4150f7
La_4150ee:;
G16(ebp + 0x579556) = 0; // 0x4150ee  mov word ptr [ebp + 0x579556], 0
La_4150f7:;
// 0x4150f7  cmp byte ptr [ebp + 0x5794d5], 0xe
if (((GU8(ebp + 0x5794d5))&0xff) != ((0xe)&0xff)) goto La_41515e; // 0x4150fe  jne 0x41515e
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x415100  mov al, byte ptr [ebp + 0x5794d6]
ecx = G32(ebx*4 + 0x53f5a0); // 0x415106  mov ecx, dword ptr [ebx*4 + 0x53f5a0]
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x41510d  inc al
FD(0x4c) = 0x6e; // 0x41510f  mov dword ptr [F+0x4c], 0x6e
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x415117  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x41511d  and eax, 0xff
edx = eax >> 31; // 0x415122  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x415123  idiv ecx
eax = ecx; // 0x415125  mov eax, ecx
esi = edx; // 0x415127  mov esi, edx
edx = eax >> 31; // 0x415129  cdq
eax = (eax) - (edx); // 0x41512a  sub eax, edx
FD(0x48) = esi; // 0x41512c  mov dword ptr [F+0x48], esi
eax = ((int)(eax)) >> (1); // 0x415130  sar eax, 1
// 0x415132  cmp esi, eax
if ((esi) != (eax)) goto La_41514c; // 0x415134  jne 0x41514c
stk[--sp] = (int)(4); // 0x415136  push 4
ecx = 0x822d9c; // 0x415138  mov ecx, 0x822d9c
eax = Rnd(stk[sp]); sp += 1; // 0x41513d  call 0x45c1e0
ecx = (ecx & ~0xffu) | ((0xfe) & 0xff); // 0x415142  mov cl, 0xfe
{ int _p = (int)(s8)(eax & 0xff) * (int)((s8)(ecx & 0xff)); eax = (eax & ~0xffffu) | (_p & 0xffff); } // 0x415144  imul cl
G8(ebp + 0x5794d6) = (GU8(ebp + 0x5794d6)) + ((eax & 0xff)); // 0x415146  add byte ptr [ebp + 0x5794d6], al
La_41514c:;
// 0x41514c  test esi, esi
if (((esi)) != 0) goto La_41515e; // 0x41514e  jne 0x41515e
G8(ebp + 0x5794d5) = 0xb; // 0x415150  mov byte ptr [ebp + 0x5794d5], 0xb
G16(ebp + 0x579556) = (esi & 0xffff); // 0x415157  mov word ptr [ebp + 0x579556], si
La_41515e:;
// 0x41515e  cmp byte ptr [ebp + 0x5794d5], 0x12
if (((GU8(ebp + 0x5794d5))&0xff) != ((0x12)&0xff)) goto La_415213; // 0x415165  jne 0x415213
eax = FD(0xc); // 0x41516b  mov eax, dword ptr [F+0xc]
esi = G32(ebx*4 + 0x53f4d8); // 0x41516f  mov esi, dword ptr [ebx*4 + 0x53f4d8]
edx = eax >> 31; // 0x415176  cdq
eax = (eax) - (edx); // 0x415177  sub eax, edx
FD(0x4c) = 0x46; // 0x415179  mov dword ptr [F+0x4c], 0x46
eax = ((int)(eax)) >> (1); // 0x415181  sar eax, 1
FD(0xc) = eax; // 0x415183  mov dword ptr [F+0xc], eax
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x415187  mov al, byte ptr [ebp + 0x5794d6]
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x41518d  inc al
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x41518f  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x415195  and eax, 0xff
edx = eax >> 31; // 0x41519a  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(G32(ebx*4 + 0x53f500)); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x41519b  idiv dword ptr [ebx*4 + 0x53f500]
eax = (int)(G16(ebp + 0x579556)); // 0x4151a2  movsx eax, word ptr [ebp + 0x579556]
FD(0x48) = edx; // 0x4151a9  mov dword ptr [F+0x48], edx
edx = 0xc; // 0x4151ad  mov edx, 0xc
edx = (edx) - (esi); // 0x4151b2  sub edx, esi
// 0x4151b4  cmp eax, edx
if ((int)(eax) <= (int)(edx)) goto La_415213; // 0x4151b6  jle 0x415213
G8(ebp + 0x5794d7) = 0x3c; // 0x4151b8  mov byte ptr [ebp + 0x5794d7], 0x3c
eax = 0xb; // 0x4151bf  mov eax, 0xb
ecx = (int)(G16(ebp + 0x579556)); // 0x4151c4  movsx ecx, word ptr [ebp + 0x579556]
eax = (eax) - (ecx); // 0x4151cb  sub eax, ecx
FD(0x4c) = 0x3c; // 0x4151cd  mov dword ptr [F+0x4c], 0x3c
// 0x4151d5  cmp eax, 0xc
FD(0x48) = eax; // 0x4151d8  mov dword ptr [F+0x48], eax
if ((int)(eax) >= (int)(0xc)) goto La_415213; // 0x4151dc  jge 0x415213
G8(ebp + 0x5794d5) = 0xb; // 0x4151de  mov byte ptr [ebp + 0x5794d5], 0xb
edx = G32(ebp + 0x5794b8); // 0x4151e5  mov edx, dword ptr [ebp + 0x5794b8]
edx = (edx) & (0xfffffc00); // 0x4151eb  and edx, 0xfffffc00
edx = (edx) + (0x200); // 0x4151f1  add edx, 0x200
G32(ebp + 0x5794b8) = edx; // 0x4151f7  mov dword ptr [ebp + 0x5794b8], edx
eax = G32(ebp + 0x5794bc); // 0x4151fd  mov eax, dword ptr [ebp + 0x5794bc]
eax = (eax) & (0xfffffc00); // 0x415203  and eax, 0xfffffc00
eax = (eax) + (0x200); // 0x415208  add eax, 0x200
G32(ebp + 0x5794bc) = eax; // 0x41520d  mov dword ptr [ebp + 0x5794bc], eax
La_415213:;
// 0x415213  cmp byte ptr [ebp + 0x5794d5], 0x11
if (((GU8(ebp + 0x5794d5))&0xff) != ((0x11)&0xff)) goto La_41526b; // 0x41521a  jne 0x41526b
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x41521c  mov al, byte ptr [ebp + 0x5794d6]
ecx = G32(ebx*4 + 0x53f4d8); // 0x415222  mov ecx, dword ptr [ebx*4 + 0x53f4d8]
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x415229  inc al
FD(0x4c) = 0x3c; // 0x41522b  mov dword ptr [F+0x4c], 0x3c
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x415233  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x415239  and eax, 0xff
edx = eax >> 31; // 0x41523e  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x41523f  idiv ecx
eax = FD(0xc); // 0x415241  mov eax, dword ptr [F+0xc]
esi = edx; // 0x415245  mov esi, edx
edx = eax >> 31; // 0x415247  cdq
eax = (eax) - (edx); // 0x415248  sub eax, edx
FD(0x48) = esi; // 0x41524a  mov dword ptr [F+0x48], esi
eax = ((int)(eax)) >> (1); // 0x41524e  sar eax, 1
// 0x415250  test esi, esi
FD(0xc) = eax; // 0x415252  mov dword ptr [F+0xc], eax
if (((esi)) != 0) goto La_41526b; // 0x415256  jne 0x41526b
ecx = (ecx) - 1; // 0x415258  dec ecx
G8(ebp + 0x5794d5) = 0x12; // 0x415259  mov byte ptr [ebp + 0x5794d5], 0x12
FD(0x48) = ecx; // 0x415260  mov dword ptr [F+0x48], ecx
G8(ebp + 0x5794d6) = 0; // 0x415264  mov byte ptr [ebp + 0x5794d6], 0
La_41526b:;
// 0x41526b  cmp byte ptr [ebp + 0x5794d5], 0x13
if (((GU8(ebp + 0x5794d5))&0xff) != ((0x13)&0xff)) goto La_41529d; // 0x415272  jne 0x41529d
edx = (edx & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x415274  mov dl, byte ptr [ebp + 0x5794d6]
FD(0x4c) = 0xdc; // 0x41527a  mov dword ptr [F+0x4c], 0xdc
edx = (edx & ~0xffu) | ((((edx & 0xff)) + 1) & 0xff); // 0x415282  inc dl
eax = (eax & ~0xffu) | (((edx & 0xff)) & 0xff); // 0x415284  mov al, dl
G8(ebp + 0x5794d6) = (edx & 0xff); // 0x415286  mov byte ptr [ebp + 0x5794d6], dl
eax = (eax) & (0xff); // 0x41528c  and eax, 0xff
edx = eax >> 31; // 0x415291  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(G32(ebx*4 + 0x53f758)); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x415292  idiv dword ptr [ebx*4 + 0x53f758]
FD(0x48) = edx; // 0x415299  mov dword ptr [F+0x48], edx
La_41529d:;
// 0x41529d  cmp byte ptr [ebp + 0x5794d5], 0x10
if (((GU8(ebp + 0x5794d5))&0xff) != ((0x10)&0xff)) goto La_41530d; // 0x4152a4  jne 0x41530d
ecx = (ecx & ~0xffu) | ((GU8(ebp + 0x5794d4)) & 0xff); // 0x4152a6  mov cl, byte ptr [ebp + 0x5794d4]
eax = (eax) ^ (eax); // 0x4152ac  xor eax, eax
// 0x4152ae  cmp cl, 6
eax = (eax & ~0xffu) | ((((int)(s8)((ecx & 0xff)) <= (int)(s8)(6)) ? 1 : 0) & 0xff); // 0x4152b1  setle al
eax = (eax) - 1; // 0x4152b4  dec eax
eax = (eax) & (0xa); // 0x4152b5  and eax, 0xa
eax = (eax) + (0x78); // 0x4152b8  add eax, 0x78
// 0x4152bb  cmp cl, 0xd
FD(0x4c) = eax; // 0x4152be  mov dword ptr [F+0x4c], eax
if ((((ecx & 0xff))&0xff) != ((0xd)&0xff)) goto La_4152d0; // 0x4152c2  jne 0x4152d0
FD(0x4c) = 0x8c; // 0x4152c4  mov dword ptr [F+0x4c], 0x8c
eax = FD(0x4c); // 0x4152cc  mov eax, dword ptr [F+0x4c]
La_4152d0:;
eax = (eax) + (ebx); // 0x4152d0  add eax, ebx
ecx = G32(eax*4 + 0x53f3e8); // 0x4152d2  mov ecx, dword ptr [eax*4 + 0x53f3e8]
// 0x4152d9  test ecx, ecx
if (((ecx)) == 0) goto La_4152fe; // 0x4152db  je 0x4152fe
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d6)) & 0xff); // 0x4152dd  mov al, byte ptr [ebp + 0x5794d6]
eax = (eax & ~0xffu) | ((((eax & 0xff)) + 1) & 0xff); // 0x4152e3  inc al
G8(ebp + 0x5794d6) = (eax & 0xff); // 0x4152e5  mov byte ptr [ebp + 0x5794d6], al
eax = (eax) & (0xff); // 0x4152eb  and eax, 0xff
edx = eax >> 31; // 0x4152f0  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(ecx); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x4152f1  idiv ecx
ecx = (ecx) - 1; // 0x4152f3  dec ecx
// 0x4152f4  cmp edx, ecx
FD(0x48) = edx; // 0x4152f6  mov dword ptr [F+0x48], edx
if ((edx) != (ecx)) goto La_41530d; // 0x4152fa  jne 0x41530d
goto La_415306; // 0x4152fc  jmp 0x415306
La_4152fe:;
FD(0x4c) = 0xa; // 0x4152fe  mov dword ptr [F+0x4c], 0xa
La_415306:;
G8(ebp + 0x5794d8) = 2; // 0x415306  mov byte ptr [ebp + 0x5794d8], 2
La_41530d:;
// 0x41530d  test byte ptr [0x59e7b8], 4
if ((((((GU8(0x59e7b8)) & (4)))&0xff)) == 0) goto La_415320; // 0x415314  je 0x415320
ecx = (ecx & ~0xffu) | ((FUB(0x34)) & 0xff); // 0x415316  mov cl, byte ptr [F+0x34]
G8(ebp + 0x5794d6) = (ecx & 0xff); // 0x41531a  mov byte ptr [ebp + 0x5794d6], cl
La_415320:;
eax = FD(0x4c); // 0x415320  mov eax, dword ptr [F+0x4c]
edx = (edx) ^ (edx); // 0x415324  xor edx, edx
edx = (edx & ~0xffu) | ((GU8(ebp + 0x5794d7)) & 0xff); // 0x415326  mov dl, byte ptr [ebp + 0x5794d7]
// 0x41532c  cmp eax, edx
if ((eax) == (edx)) goto La_41535d; // 0x41532e  je 0x41535d
G8(ebp + 0x5794d6) = 0; // 0x415330  mov byte ptr [ebp + 0x5794d6], 0
G8(ebp + 0x5794d7) = (eax & 0xff); // 0x415337  mov byte ptr [ebp + 0x5794d7], al
eax = (eax & ~0xffu) | ((GU8(ebp + 0x5794d5)) & 0xff); // 0x41533d  mov al, byte ptr [ebp + 0x5794d5]
FD(0x48) = 0; // 0x415343  mov dword ptr [F+0x48], 0
// 0x41534b  cmp al, 0xc
if ((((eax & 0xff))&0xff) == ((0xc)&0xff)) goto La_41535d; // 0x41534d  je 0x41535d
// 0x41534f  cmp al, 0xd
if ((((eax & 0xff))&0xff) == ((0xd)&0xff)) goto La_41535d; // 0x415351  je 0x41535d
G32(ebp + 0x5794c8) = (G32(ebp + 0x5794c8)) & (0xf7ffffff); // 0x415353  and dword ptr [ebp + 0x5794c8], 0xf7ffffff
La_41535d:;
edx = G32(ebp + 0x5794bc); // 0x41535d  mov edx, dword ptr [ebp + 0x5794bc]
eax = FA(0x70); // 0x415363  lea eax, [F+0x70]
stk[--sp] = (int)(0); // 0x415367  push 0
ecx = FA(0x88); // 0x415369  lea ecx, [F+0x88]
stk[--sp] = (int)(eax); // 0x415370  push eax
eax = G32(ebp + 0x5794b8); // 0x415371  mov eax, dword ptr [ebp + 0x5794b8]
stk[--sp] = (int)(ecx); // 0x415377  push ecx
stk[--sp] = (int)(edx); // 0x415378  push edx
stk[--sp] = (int)(eax); // 0x415379  push eax
eax = CC(0x42fb90, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // worldToScreen  0x41537a
sp += 5; // 0x41537f  add esp, 0x14  (caller stack cleanup)
// 0x415382  test eax, eax
if (((eax)) == 0) goto La_41557f; // 0x415384  je 0x41557f
ecx = FD(0x88); // 0x41538a  mov ecx, dword ptr [F+0x88]
edx = FD(0x70); // 0x415391  mov edx, dword ptr [F+0x70]
G32(ebp + 0x5794c0) = ecx; // 0x415395  mov dword ptr [ebp + 0x5794c0], ecx
G32(ebp + 0x5794c4) = edx; // 0x41539b  mov dword ptr [ebp + 0x5794c4], edx
eax = (eax & ~0xffu) | ((GU8(ebp + 0x57953c)) & 0xff); // 0x4153a1  mov al, byte ptr [ebp + 0x57953c]
// 0x4153a7  test al, al
if (((((eax & 0xff))&0xff)) == 0) goto La_4154f5; // 0x4153a9  je 0x4154f5
// 0x4153af  cmp al, 7
if ((u32)(((eax & 0xff))&0xff) > (u32)((7)&0xff)) goto La_4154f5; // 0x4153b1  ja 0x4154f5
// 0x4153b7  cmp byte ptr [ebp + 0x57953d], 0x32
if (((GU8(ebp + 0x57953d))&0xff) != ((0x32)&0xff)) goto La_4154f5; // 0x4153be  jne 0x4154f5
// 0x4153c4  cmp word ptr [ebp + 0x57956a], 1
if (((GU16(ebp + 0x57956a))&0xffff) != ((1)&0xffff)) goto La_4154f5; // 0x4153cc  jne 0x4154f5
// 0x4153d2  test dword ptr [ebp + 0x5794c8], 0x100000
if (((((G32(ebp + 0x5794c8)) & (0x100000)))) != 0) goto La_4154f5; // 0x4153dc  jne 0x4154f5
eax = G32(0x571d38); // 0x4153e2  mov eax, dword ptr [0x571d38]
// 0x4153e7  test eax, 0x40000
if (((((eax) & (0x40000)))) != 0) goto La_4154f5; // 0x4153ec  jne 0x4154f5
esi = eax; // 0x4153f2  mov esi, eax
eax = FD(0x14); // 0x4153f4  mov eax, dword ptr [F+0x14]
esi = (esi) | (0x40000); // 0x4153f8  or esi, 0x40000
stk[--sp] = (int)(0); // 0x4153fe  push 0
stk[--sp] = (int)(eax); // 0x415400  push eax
G32(0x571d38) = esi; // 0x415401  mov dword ptr [0x571d38], esi
G8(0x51a068) = 0; // 0x415407  mov byte ptr [0x51a068], 0
eax = CC(0x4676e0, stk[sp+0], stk[sp+1]); // buildGolferName  0x41540e
edi = 0x4c6af4; // 0x415413  mov edi, 0x4c6af4
ecx = (ecx) | (0xffffffff); // 0x415418  or ecx, 0xffffffff
eax = (eax) ^ (eax); // 0x41541b  xor eax, eax
StrCat(0x51A068, 0x4C6AF4); // 0x41541d..0x415446  VC6 inline strcat(kBuf, &DAT_4c6af4) (scasb+movsd+movsb)
ecx = ~(ecx); // 0x41541f  not ecx
edi = (edi) - (ecx); // 0x415421  sub edi, ecx
stk[--sp] = (int)(eax); // 0x415423  push eax
esi = edi; // 0x415424  mov esi, edi
edx = ecx; // 0x415426  mov edx, ecx
edi = 0x51a068; // 0x415428  mov edi, 0x51a068
ecx = (ecx) | (0xffffffff); // 0x41542d  or ecx, 0xffffffff
// 0x415430  repne scasb al, byte ptr es:[edi]  (folded into the StrCat above)
ecx = edx; // 0x415432  mov ecx, edx
edi = (edi) - 1; // 0x415434  dec edi
ecx = ((u32)(ecx)) >> (2); // 0x415435  shr ecx, 2
// 0x415438  rep movsd dword ptr es:[edi], dword ptr [esi]  (folded into the StrCat above)
eax = FD(0x14); // 0x41543a  mov eax, dword ptr [F+0x14]
ecx = edx; // 0x41543e  mov ecx, edx
ecx = (ecx) & (3); // 0x415440  and ecx, 3
eax = (eax) ^ (1); // 0x415443  xor eax, 1
// 0x415446  rep movsb byte ptr es:[edi], byte ptr [esi]  (folded into the StrCat above)
stk[--sp] = (int)(eax); // 0x415448  push eax
eax = CC(0x4676e0, stk[sp+0], stk[sp+1]); // buildGolferName  0x415449
edi = 0x4c6980; // 0x41544e  mov edi, 0x4c6980
ecx = (ecx) | (0xffffffff); // 0x415453  or ecx, 0xffffffff
eax = (eax) ^ (eax); // 0x415456  xor eax, eax
StrCat(0x51A068, 0x4C6980); // 0x415458..0x415484  strcat(kBuf, &DAT_4c6980)
ecx = ~(ecx); // 0x41545a  not ecx
edi = (edi) - (ecx); // 0x41545c  sub edi, ecx
stk[--sp] = (int)(eax); // 0x41545e  push eax
esi = edi; // 0x41545f  mov esi, edi
edx = ecx; // 0x415461  mov edx, ecx
edi = 0x51a068; // 0x415463  mov edi, 0x51a068
ecx = (ecx) | (0xffffffff); // 0x415468  or ecx, 0xffffffff
// 0x41546b  repne scasb al, byte ptr es:[edi]  (folded into the StrCat above)
ecx = edx; // 0x41546d  mov ecx, edx
edi = (edi) - 1; // 0x41546f  dec edi
ecx = ((u32)(ecx)) >> (2); // 0x415470  shr ecx, 2
// 0x415473  rep movsd dword ptr es:[edi], dword ptr [esi]  (folded into the StrCat above)
stk[--sp] = (int)(eax); // 0x415475  push eax
ecx = edx; // 0x415476  mov ecx, edx
eax = (int)(G16(ebp + 0x579568)); // 0x415478  movsx eax, word ptr [ebp + 0x579568]
ecx = (ecx) & (3); // 0x41547f  and ecx, 3
stk[--sp] = (int)(-1); // 0x415482  push -1
// 0x415484  rep movsb byte ptr es:[edi], byte ptr [esi]  (folded into the StrCat above)
stk[--sp] = (int)(eax); // 0x415486  push eax
eax = CC(0x466b70, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3]); // loadStoryStrings  0x415487
edi = 0x4c691c; // 0x41548c  mov edi, 0x4c691c
ecx = (ecx) | (0xffffffff); // 0x415491  or ecx, 0xffffffff
eax = (eax) ^ (eax); // 0x415494  xor eax, eax
StrCat(0x51A068, 0x4C691C); // 0x415496..0x4154b7  strcat(kBuf, &DAT_4c691c)
ecx = ~(ecx); // 0x415498  not ecx
edi = (edi) - (ecx); // 0x41549a  sub edi, ecx
esi = edi; // 0x41549c  mov esi, edi
edx = ecx; // 0x41549e  mov edx, ecx
edi = 0x51a068; // 0x4154a0  mov edi, 0x51a068
ecx = (ecx) | (0xffffffff); // 0x4154a5  or ecx, 0xffffffff
// 0x4154a8  repne scasb al, byte ptr es:[edi]  (folded into the StrCat above)
ecx = edx; // 0x4154aa  mov ecx, edx
edi = (edi) - 1; // 0x4154ac  dec edi
ecx = ((u32)(ecx)) >> (2); // 0x4154ad  shr ecx, 2
// 0x4154b0  rep movsd dword ptr es:[edi], dword ptr [esi]  (folded into the StrCat above)
ecx = edx; // 0x4154b2  mov ecx, edx
ecx = (ecx) & (3); // 0x4154b4  and ecx, 3
// 0x4154b7  rep movsb byte ptr es:[edi], byte ptr [esi]  (folded into the StrCat above)
edi = 0x4c68e0; // 0x4154b9  mov edi, 0x4c68e0
ecx = (ecx) | (0xffffffff); // 0x4154be  or ecx, 0xffffffff
StrCat(0x51A068, 0x4C68E0); // 0x4154c1..0x4154eb  strcat(kBuf, &DAT_4c68e0)
ecx = ~(ecx); // 0x4154c3  not ecx
edi = (edi) - (ecx); // 0x4154c5  sub edi, ecx
esi = edi; // 0x4154c7  mov esi, edi
edx = ecx; // 0x4154c9  mov edx, ecx
edi = 0x51a068; // 0x4154cb  mov edi, 0x51a068
ecx = (ecx) | (0xffffffff); // 0x4154d0  or ecx, 0xffffffff
stk[--sp] = (int)(-2); // 0x4154d3  push -2
// 0x4154d5  repne scasb al, byte ptr es:[edi]  (folded into the StrCat above)
ecx = edx; // 0x4154d7  mov ecx, edx
edi = (edi) - 1; // 0x4154d9  dec edi
ecx = ((u32)(ecx)) >> (2); // 0x4154da  shr ecx, 2
// 0x4154dd  rep movsd dword ptr es:[edi], dword ptr [esi]  (folded into the StrCat above)
ecx = edx; // 0x4154df  mov ecx, edx
stk[--sp] = (int)(1); // 0x4154e1  push 1
ecx = (ecx) & (3); // 0x4154e3  and ecx, 3
stk[--sp] = (int)(0x80000018); // 0x4154e6  push 0x80000018
// 0x4154eb  rep movsb byte ptr es:[edi], byte ptr [esi]  (folded into the StrCat above)
eax = CC(0x40cb00, stk[sp+0], stk[sp+1], stk[sp+2]); // startMessage  0x4154ed
sp += 11; // 0x4154f2  add esp, 0x2c  (caller stack cleanup)
La_4154f5:;
eax = FD(0x48); // 0x4154f5  mov eax, dword ptr [F+0x48]
// 0x4154f9  test eax, eax
if (((int)(eax)) >= 0) goto La_415505; // 0x4154fb  jge 0x415505
FD(0x48) = 0; // 0x4154fd  mov dword ptr [F+0x48], 0
La_415505:;
eax = FD(0xc); // 0x415505  mov eax, dword ptr [F+0xc]
ecx = FD(0x4c); // 0x415509  mov ecx, dword ptr [F+0x4c]
stk[--sp] = (int)(eax); // 0x41550d  push eax
eax = FD(0x48); // 0x41550e  mov eax, dword ptr [F+0x48]
ecx = (ecx) + (ebx); // 0x415512  add ecx, ebx
esi = 4; // 0x415514  mov esi, 4
edx = eax >> 31; // 0x415519  cdq
{ long long _n = ((long long)edx<<32) | (u32)eax; int _d=(G32(ecx*4 + 0x53f3e8)); eax=(int)(_n/_d); edx=(int)(_n%_d);} // 0x41551a  idiv dword ptr [ecx*4 + 0x53f3e8]
FD(0x4c) = ecx; // 0x415521  mov dword ptr [F+0x4c], ecx
stk[--sp] = (int)(edx); // 0x415525  push edx
stk[--sp] = (int)(ecx); // 0x415526  push ecx
eax = CC(0x43d6f0, stk[sp+0], stk[sp+1], stk[sp+2]); // cellAt  0x415527
edx = (int)(G16(ebp + 0x57956e)); // 0x41552c  movsx edx, word ptr [ebp + 0x57956e]
sp += 3; // 0x415533  add esp, 0xc  (caller stack cleanup)
G32(0x5a9cc8) = eax; // 0x415536  mov dword ptr [0x5a9cc8], eax
ecx = (int)(edx*8); // 0x41553b  lea ecx, [edx*8]
ecx = (ecx) - (edx); // 0x415542  sub ecx, edx
ecx = (int)(ecx + ecx*4); // 0x415544  lea ecx, [ecx + ecx*4]
ecx = (ecx) << (4); // 0x415547  shl ecx, 4
// 0x41554a  test byte ptr [ecx + 0x4d60b4], 8
if ((((((GU8(ecx + 0x4d60b4)) & (8)))&0xff)) == 0) goto La_415558; // 0x415551  je 0x415558
esi = 3; // 0x415553  mov esi, 3
La_415558:;
edx = FD(0x14); // 0x415558  mov edx, dword ptr [F+0x14]
edx = (edx & ~0xff00u) | ((((((edx>>8)&0xff)) | (1)) & 0xff)<<8); // 0x41555c  or dh, 1
stk[--sp] = (int)(edx); // 0x41555f  push edx
stk[--sp] = (int)(esi); // 0x415560  push esi
stk[--sp] = (int)(eax); // 0x415561  push eax
eax = FD(0x70); // 0x415562  mov eax, dword ptr [F+0x70]
stk[--sp] = (int)(0x28); // 0x415569  push 0x28
stk[--sp] = (int)(0x1e); // 0x41556b  push 0x1e
stk[--sp] = (int)(eax); // 0x41556d  push eax
stk[--sp] = (int)(eax); // 0x41556e  push eax
eax = FD(0x88); // 0x41556f  mov eax, dword ptr [F+0x88]
stk[--sp] = (int)(eax); // 0x415576  push eax
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x415577
sp += 8; // 0x41557c  add esp, 0x20  (caller stack cleanup)
La_41557f:;
eax = G32(ebp + 0x57958c); // 0x41557f  mov eax, dword ptr [ebp + 0x57958c]
// 0x415585  test eax, eax
if (((eax)) == 0) goto La_415823; // 0x415587  je 0x415823
// 0x41558d  test byte ptr [ebp + 0x5794c8], 1
if ((((((GU8(ebp + 0x5794c8)) & (1)))&0xff)) != 0) goto La_415823; // 0x415594  jne 0x415823
ecx = FA(0x70); // 0x41559a  lea ecx, [F+0x70]
stk[--sp] = (int)(0); // 0x41559e  push 0
stk[--sp] = (int)(ecx); // 0x4155a0  push ecx
ecx = G32(ebp + 0x579590); // 0x4155a1  mov ecx, dword ptr [ebp + 0x579590]
edx = FA(0x88); // 0x4155a7  lea edx, [F+0x88]
stk[--sp] = (int)(edx); // 0x4155ae  push edx
stk[--sp] = (int)(ecx); // 0x4155af  push ecx
stk[--sp] = (int)(eax); // 0x4155b0  push eax
eax = CC(0x42fb90, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // worldToScreen  0x4155b1
sp += 5; // 0x4155b6  add esp, 0x14  (caller stack cleanup)
// 0x4155b9  test eax, eax
if (((eax)) == 0) goto La_415823; // 0x4155bb  je 0x415823
// 0x4155c1  test byte ptr [0x59e7b8], 0x20
if ((((((GU8(0x59e7b8)) & (0x20)))&0xff)) == 0) goto La_4155e1; // 0x4155c8  je 0x4155e1
edx = G32(0x4c1570); // 0x4155ca  mov edx, dword ptr [0x4c1570]
stk[--sp] = (int)(0); // 0x4155d0  push 0
stk[--sp] = (int)(0x64); // 0x4155d2  push 0x64
stk[--sp] = (int)(0x64); // 0x4155d4  push 0x64
stk[--sp] = (int)(edx); // 0x4155d6  push edx
ecx = 0x58b650; // 0x4155d7  mov ecx, 0x58b650
eax = TC(0x473e60, ecx, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3]); sp += 4; // Image_draw  0x4155dc
La_4155e1:;
// 0x4155e1  test dword ptr [ebp + 0x5794c8], 0x40000
if (((((G32(ebp + 0x5794c8)) & (0x40000)))) == 0) goto La_4155fd; // 0x4155eb  je 0x4155fd
// 0x4155ed  cmp dword ptr [ebp + 0x57959c], 0x400
if ((int)(G32(ebp + 0x57959c)) > (int)(0x400)) goto La_4156a5; // 0x4155f7  jg 0x4156a5
La_4155fd:;
eax = G32(ebp + 0x579594); // 0x4155fd  mov eax, dword ptr [ebp + 0x579594]
// 0x415603  test eax, eax
if (((eax)) != 0) goto La_4156a5; // 0x415605  jne 0x4156a5
eax = G32(ebp + 0x579590); // 0x41560b  mov eax, dword ptr [ebp + 0x579590]
ecx = G32(ebp + 0x57958c); // 0x415611  mov ecx, dword ptr [ebp + 0x57958c]
stk[--sp] = (int)(eax); // 0x415617  push eax
stk[--sp] = (int)(ecx); // 0x415618  push ecx
eax = CC(0x40bfa0, stk[sp+0], stk[sp+1]); // typeAtPos  0x415619
edx = (int)(eax + eax*2); // 0x41561e  lea edx, [eax + eax*2]
sp += 2; // 0x415621  add esp, 8  (caller stack cleanup)
edx = (edx) << (4); // 0x415624  shl edx, 4
eax = (eax & ~0xffu) | ((GU8(edx + 0x578372)) & 0xff); // 0x415627  mov al, byte ptr [edx + 0x578372]
// 0x41562d  test al, al
if (((int)(s8)((eax & 0xff))) <= 0) goto La_4156a5; // 0x41562f  jle 0x4156a5
ecx = FD(0x88); // 0x415631  mov ecx, dword ptr [F+0x88]
eax = FD(0x70); // 0x415638  mov eax, dword ptr [F+0x70]
stk[--sp] = (int)(0x80007fff); // 0x41563c  push 0x80007fff
stk[--sp] = (int)(eax); // 0x415641  push eax
edx = (int)(ecx + 1); // 0x415642  lea edx, [ecx + 1]
stk[--sp] = (int)(edx); // 0x415645  push edx
stk[--sp] = (int)(eax); // 0x415646  push eax
stk[--sp] = (int)(ecx); // 0x415647  push ecx
ecx = 0x519cd4; // 0x415648  mov ecx, 0x519cd4
eax = TC(0x478b80, ecx, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); sp += 5; // Widget_fillRect  0x41564d
esi = G32(0x4c2e00); // 0x415652  mov esi, dword ptr [0x4c2e00]
ecx = FD(0x70); // 0x415658  mov ecx, dword ptr [F+0x70]
La_41565c:;
edi = FD(0x14); // 0x41565c  mov edi, dword ptr [F+0x14]
La_415660:;
esi = (esi) * (G32(0x4c2844)); // 0x415660  imul esi, dword ptr [0x4c2844]
esi = (esi) * (G32(ebp + 0x579594)); // 0x415667  imul esi, dword ptr [ebp + 0x579594]
edx = FD(0x88); // 0x41566e  mov edx, dword ptr [F+0x88]
eax = 0x66666667; // 0x415675  mov eax, 0x66666667
G32(edi*4 + 0x567b34) = edx; // 0x41567a  mov dword ptr [edi*4 + 0x567b34], edx
{ long long _p = (long long)(int)eax * (long long)(int)(esi); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x415681  imul esi
edx = ((int)(edx)) >> (1); // 0x415683  sar edx, 1
esi = edx; // 0x415685  mov esi, edx
eax = edx; // 0x415687  mov eax, edx
esi = ((u32)(esi)) >> (0x1f); // 0x415689  shr esi, 0x1f
eax = (eax) + (esi); // 0x41568c  add eax, esi
edx = eax >> 31; // 0x41568e  cdq
edx = (edx) & (0xf); // 0x41568f  and edx, 0xf
eax = (eax) + (edx); // 0x415692  add eax, edx
eax = ((int)(eax)) >> (4); // 0x415694  sar eax, 4
ecx = (ecx) - (eax); // 0x415697  sub ecx, eax
G32(edi*4 + 0x567d94) = ecx; // 0x415699  mov dword ptr [edi*4 + 0x567d94], ecx
goto La_415832; // 0x4156a0  jmp 0x415832
La_4156a5:;
edx = G32(0x4c2844); // 0x4156a5  mov edx, dword ptr [0x4c2844]
eax = (eax) ^ (eax); // 0x4156ab  xor eax, eax
{ int cmpA = edx; // 0x4156ad  cmp edx, 4: edx = [0x4c2844] (0x4156a5); the setle at 0x4156b7 reads THIS compare
edx = FD(0x88); // 0x4156b0  mov edx, dword ptr [F+0x88] (moves never touch the flags)
eax = (eax & ~0xffu) | ((((int)(cmpA) <= (int)(4)) ? 1 : 0) & 0xff); } // 0x4156b7  setle al  (c3fy2 fix: was F+0x88)
eax = (eax) - 1; // 0x4156ba  dec eax
stk[--sp] = (int)(-1); // 0x4156bb  push -1
eax = (eax & ~0xffu) | ((((eax & 0xff)) & (0xfe)) & 0xff); // 0x4156bd  and al, 0xfe
eax = (eax) + (4); // 0x4156bf  add eax, 4
stk[--sp] = (int)(eax); // 0x4156c2  push eax
eax = FD(0x70); // 0x4156c3  mov eax, dword ptr [F+0x70]
stk[--sp] = (int)(0x5791c8); // 0x4156c7  push 0x5791c8
stk[--sp] = (int)(1); // 0x4156cc  push 1
ecx = (int)(eax + -1); // 0x4156ce  lea ecx, [eax - 1]
stk[--sp] = (int)(1); // 0x4156d1  push 1
eax = (eax) + 1; // 0x4156d3  inc eax
stk[--sp] = (int)(ecx); // 0x4156d4  push ecx
stk[--sp] = (int)(eax); // 0x4156d5  push eax
stk[--sp] = (int)(edx); // 0x4156d6  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x4156d7
eax = G32(ebp + 0x579594); // 0x4156dc  mov eax, dword ptr [ebp + 0x579594]
sp += 8; // 0x4156e2  add esp, 0x20  (caller stack cleanup)
// 0x4156e5  test eax, eax
if (((eax)) == 0) goto La_41570e; // 0x4156e7  je 0x41570e
edx = G32(ebp + 0x579590); // 0x4156e9  mov edx, dword ptr [ebp + 0x579590]
eax = FA(0x70); // 0x4156ef  lea eax, [F+0x70]
stk[--sp] = (int)(-1); // 0x4156f3  push -1
ecx = FA(0x88); // 0x4156f5  lea ecx, [F+0x88]
stk[--sp] = (int)(eax); // 0x4156fc  push eax
eax = G32(ebp + 0x57958c); // 0x4156fd  mov eax, dword ptr [ebp + 0x57958c]
stk[--sp] = (int)(ecx); // 0x415703  push ecx
stk[--sp] = (int)(edx); // 0x415704  push edx
stk[--sp] = (int)(eax); // 0x415705  push eax
eax = CC(0x42fb90, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4]); // worldToScreen  0x415706
sp += 5; // 0x41570b  add esp, 0x14  (caller stack cleanup)
La_41570e:;
ecx = G32(0x5a9cbc); // 0x41570e  mov ecx, dword ptr [0x5a9cbc]
stk[--sp] = (int)(-1); // 0x415714  push -1
ecx = -(ecx); // 0x415716  neg ecx
ecx = -(int)(((ecx) != 0)); // 0x415718  sbb ecx, ecx
eax = 0x66666667; // 0x41571a  mov eax, 0x66666667
ecx = (ecx) & (0xfffffffe); // 0x41571f  and ecx, 0xfffffffe
esi = FD(0x70); // 0x415722  mov esi, dword ptr [F+0x70]
ecx = (ecx) + (4); // 0x415726  add ecx, 4
stk[--sp] = (int)(ecx); // 0x415729  push ecx
ecx = G32(0x4c2e00); // 0x41572a  mov ecx, dword ptr [0x4c2e00]
ecx = (ecx) * (G32(0x4c2844)); // 0x415730  imul ecx, dword ptr [0x4c2844]
ecx = (ecx) * (G32(ebp + 0x579594)); // 0x415737  imul ecx, dword ptr [ebp + 0x579594]
{ long long _p = (long long)(int)eax * (long long)(int)(ecx); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x41573e  imul ecx
edx = ((int)(edx)) >> (1); // 0x415740  sar edx, 1
ecx = edx; // 0x415742  mov ecx, edx
eax = edx; // 0x415744  mov eax, edx
ecx = ((u32)(ecx)) >> (0x1f); // 0x415746  shr ecx, 0x1f
eax = (eax) + (ecx); // 0x415749  add eax, ecx
stk[--sp] = (int)(0x58b650); // 0x41574b  push 0x58b650
edx = eax >> 31; // 0x415750  cdq
edx = (edx) & (0xf); // 0x415751  and edx, 0xf
stk[--sp] = (int)(1); // 0x415754  push 1
eax = (eax) + (edx); // 0x415756  add eax, edx
edx = FD(0x88); // 0x415758  mov edx, dword ptr [F+0x88]
stk[--sp] = (int)(1); // 0x41575f  push 1
stk[--sp] = (int)(esi); // 0x415761  push esi
eax = ((int)(eax)) >> (4); // 0x415762  sar eax, 4
esi = (esi) - (eax); // 0x415765  sub esi, eax
stk[--sp] = (int)(esi); // 0x415767  push esi
stk[--sp] = (int)(edx); // 0x415768  push edx
eax = CC(0x462a30, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6], stk[sp+7]); // enqueueSprite  0x415769
esi = G32(0x4c2e00); // 0x41576e  mov esi, dword ptr [0x4c2e00]
eax = 0x66666667; // 0x415774  mov eax, 0x66666667
ecx = esi; // 0x415779  mov ecx, esi
edi = G32(0x58bcb0); // 0x41577b  mov edi, dword ptr [0x58bcb0]
ecx = (ecx) * (G32(0x4c2844)); // 0x415781  imul ecx, dword ptr [0x4c2844]
ecx = (ecx) * (G32(ebp + 0x579594)); // 0x415788  imul ecx, dword ptr [ebp + 0x579594]
{ long long _p = (long long)(int)eax * (long long)(int)(ecx); eax=(int)_p; edx=(int)((unsigned long long)_p>>32);} // 0x41578f  imul ecx
edx = ((int)(edx)) >> (1); // 0x415791  sar edx, 1
ecx = edx; // 0x415793  mov ecx, edx
eax = edx; // 0x415795  mov eax, edx
ecx = ((u32)(ecx)) >> (0x1f); // 0x415797  shr ecx, 0x1f
eax = (eax) + (ecx); // 0x41579a  add eax, ecx
ecx = FD(0x70); // 0x41579c  mov ecx, dword ptr [F+0x70]
edx = eax >> 31; // 0x4157a3  cdq
ebx = FD(0x88); // 0x4157a4  mov ebx, dword ptr [F+0x88]
edx = (edx) & (0xf); // 0x4157ab  and edx, 0xf
edx = (edx) + (eax); // 0x4157ae  add edx, eax
eax = ecx; // 0x4157b0  mov eax, ecx
edx = ((int)(edx)) >> (4); // 0x4157b2  sar edx, 4
eax = (eax) - (edx); // 0x4157b5  sub eax, edx
edx = G32(ebp + 0x5794c8); // 0x4157b7  mov edx, dword ptr [ebp + 0x5794c8]
sp += 8; // 0x4157bd  add esp, 0x20  (caller stack cleanup)
G32(edi*4 + 0x5a47e8) = ebx; // 0x4157c0  mov dword ptr [edi*4 + 0x5a47e8], ebx
G32(edi*4 + 0x5a4868) = eax; // 0x4157c7  mov dword ptr [edi*4 + 0x5a4868], eax
edi = (edi) + 1; // 0x4157ce  inc edi
// 0x4157cf  test edx, 0x400000
G32(0x58bcb0) = edi; // 0x4157d5  mov dword ptr [0x58bcb0], edi
if (((((edx) & (0x400000)))) == 0) goto La_41565c; // 0x4157db  je 0x41565c
edi = FD(0x14); // 0x4157e1  mov edi, dword ptr [F+0x14]
edx = G32(edi*4 + 0x567b34); // 0x4157e5  mov edx, dword ptr [edi*4 + 0x567b34]
// 0x4157ec  cmp edx, -1
if ((edx) == (-1)) goto La_415660; // 0x4157ef  je 0x415660
ecx = G32(edi*4 + 0x567d94); // 0x4157f5  mov ecx, dword ptr [edi*4 + 0x567d94]
stk[--sp] = (int)(7); // 0x4157fc  push 7
stk[--sp] = (int)(2); // 0x4157fe  push 2
stk[--sp] = (int)(0x80007f9c); // 0x415800  push 0x80007f9c
stk[--sp] = (int)(ecx); // 0x415805  push ecx
stk[--sp] = (int)(edx); // 0x415806  push edx
stk[--sp] = (int)(eax); // 0x415807  push eax
stk[--sp] = (int)(ebx); // 0x415808  push ebx
eax = CC(0x406220, stk[sp+0], stk[sp+1], stk[sp+2], stk[sp+3], stk[sp+4], stk[sp+5], stk[sp+6]); // drawMinimapSegment  0x415809
esi = G32(0x4c2e00); // 0x41580e  mov esi, dword ptr [0x4c2e00]
ecx = FD(0x70); // 0x415814  mov ecx, dword ptr [F+0x70]
sp += 7; // 0x41581b  add esp, 0x1c  (caller stack cleanup)
goto La_415660; // 0x41581e  jmp 0x415660
La_415823:;
eax = FD(0x14); // 0x415823  mov eax, dword ptr [F+0x14]
G32(eax*4 + 0x567b34) = 0xffffffff; // 0x415827  mov dword ptr [eax*4 + 0x567b34], 0xffffffff
La_415832:;
esi = FD(0x14); // 0x415832  mov esi, dword ptr [F+0x14]
La_415836:;
esi = (esi) + 1; // 0x415836  inc esi
// 0x415837  cmp esi, 0x98
FD(0x14) = esi; // 0x41583d  mov dword ptr [F+0x14], esi
if ((int)(esi) < (int)(0x98)) goto La_414964; // 0x415841  jl 0x414964

}

}  // namespace
// c3fy part P4: mainLoop 0x0040f5c0, range 0x00415847..0x004175f2 (falls through to 0x004175f2).
// Hand transcription of log/c3/alt/c3fy_norm.asm. F = esp after `push edi` at 0x0040f5e1; every [F+X] of the
// original is FD/FU/FW/FB(X) here at the same offset and width. Registers are C locals named after them.
// Entry: no register is live at 0x00415847. Exit: eax / ebx / edi / esi are dead at 0x004175f2.

namespace {

// ---------------------------------------------------------------------------------------------------------------
// 0x00415847..0x0041596e: the 256 records of 0x24 bytes at 0x005736b8 (loop pointer esi, end 0x00575ab8).
// A record whose dword +0 is not -1 and whose age (date [0x00834170] - dword +0x18) is below 0x40 draws two
// rectangles at the projection of (+0, +4); every record whose +0 is not -1 draws one more at the projection of
// (-8, -4).
static void P4_records5736b8(u8* F) {
    u32 esi = 0x005736B8;                                                     // 0x00415847
    for (;;) {
        int eax = G32(esi);                                                   // 0x0041584c
        if (eax != -1) {                                                      // 0x00415851
            int ecx = G32(0x00834170) - G32(esi + 0x18);                      // 0x00415857..0x00415860
            if (ecx < 0x40) {                                                 // 0x00415865 jge 0x00415915
                if (CC(0x0042FB90, eax, G32(esi + 4), FA(0xdc), FA(0xec), 0) != 0) {   // worldToScreen 0x00415882, 0x0041588c
                    TC(0x00480B00, 0x00519A60, FD(0xdc), FD(0xec), 2, 1, (int)0x80000000);   // fillWindowRect 0x004158b0
                    int a;
                    if (G32(esi + 8) != 0)                                    // 0x004158ba
                        a = (Rnd(3) & 0xffff) + 1;                            // Random::range 0x004158c3, 0x004158c8..0x004158cd
                    else
                        a = 2;                                                // 0x004158d0
                    int c = G32(esi + 8);                                     // 0x004158d5
                    int d = (c != 0 ? 1 : 0) + 1;                             // 0x004158d8..0x004158df
                    int t = (int)((u32)c * (u32)G32(0x004C2844));             // 0x004158f0 imul
                    int y = FD(0xec) - t / 16;                                // 0x004158e9 / 0x004158f7..0x00415907 (cdq, and 0xf, sar 4)
                    TC(0x00480B00, 0x00519A60, FD(0xdc), y, a, d, (int)0x80000100);   // fillWindowRect 0x00415910
                }
            }
            // 0x00415915
            if (CC(0x0042FB90, G32(esi - 8), G32(esi - 4), FA(0xdc), FA(0xec), 0) != 0)   // worldToScreen 0x0041592f, 0x00415939
                TC(0x00480B00, 0x00519A60, FD(0xdc) - 1, FD(0xec), 3, 1, (int)0x80002108);   // fillWindowRect 0x0041595a
        }
        esi += 0x24;                                                          // 0x0041595f
        if ((int)esi >= 0x00575AB8) break;                                    // 0x00415968 jl 0x0041584c
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x0041596e..0x004159f7: drawFlyingBall, then the counter [0x004c2e28]: while it is not -1 and below
// [0x0053f7c8] it queues cell (0xf8, counter, 0) as a sprite at the projection of (0x005787fc, 0x00578800)
// (stored to 0x005a9cc8) and steps; at the limit it becomes -1.
static void P4_flyingBall(u8* F) {
    CC(0x004038F0);                                                           // drawFlyingBall 0x0041596e (pushes nothing)
    int esi = G32(0x004C2E28);                                                // 0x00415973
    if (esi == -1) return;                                                    // 0x0041597c je 0x004159f7
    if (esi < G32(0x0053F7C8)) {                                              // 0x00415984 jge 0x004159ed
        if (CC(0x0042FB90, G32(0x005787FC), G32(0x00578800), FA(0x20c), FA(0x1e0), 0) != 0) {   // worldToScreen 0x004159a5, 0x004159af
            int cell = CC(0x0043D6F0, 0xf8, esi, 0);                          // cellAt 0x004159b9
            G32(0x005A9CC8) = cell;                                           // 0x004159c9
            CC(0x00462A30, FD(0x20c), FD(0x1e0), FD(0x1e0), 0x28, 0x28, cell, 4, 0x1d);   // enqueueSprite 0x004159dd
        }
        G32(0x004C2E28) += 1;                                                 // 0x004159e5
    } else {
        G32(0x004C2E28) = -1;                                                 // 0x004159ed
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x004159f7..0x00415a44: with byte [0x00569498] == 1 and golfer g = [0x004c2e08] != -1 whose byte +0x19
// (0x005794d9 + 0x100 g) is set, Sprite 0x005423e8 is drawn at (x - 0x14, y - 0xa) with frame [0x00824148].
static void P4_selectedSprite(u8* F) {
    (void)F;
    if (GU8(0x00569498) != 1) return;                                         // 0x004159fe
    int eax = G32(0x004C2E08);                                                // 0x00415a00
    if (eax == -1) return;                                                    // 0x00415a08
    u32 g = (u32)eax << 8;                                                    // 0x00415a0a
    if (GU8(g + 0x005794D9) == 0) return;                                     // 0x00415a0d / 0x00415a15
    int edx = G32(0x00824148);                                                // 0x00415a17
    int ecx = G32(g + 0x005794C4) - 0xa;                                      // 0x00415a1d / 0x00415a31
    int x = G32(g + 0x005794C0) - 0x14;                                       // 0x00415a26 / 0x00415a34
    TC(0x004741B0, 0x005423E8, Root(), x, ecx, edx, 0);                       // Sprite_draw 0x00415a3f
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00415a44..0x00415a77: golfer h = [0x0053df54] (> 0) with byte +0x19 (0x005793d9 + 0x100 h) set gets a
// circle of radius 0xf at its record's (+0, +4) (0x005793b8 / 0x005793bc + 0x100 h).
static void P4_markerCircle(u8* F) {
    (void)F;
    int eax = G32(0x0053DF54);                                                // 0x00415a44
    if (eax <= 0) return;                                                     // 0x00415a4b jle
    u32 g = (u32)eax << 8;                                                    // 0x00415a4d
    if (GU8(g + 0x005793D9) == 0) return;                                     // 0x00415a50 / 0x00415a58
    CC(0x00407C60, G32(g + 0x005793B8), G32(g + 0x005793BC), 0xf, (int)0x800023E8);   // drawCircle 0x00415a6f
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00415a77..0x00415b6d: tickAnimals; with [0x005a59f8] == -1 and bit 0x200000 of [0x0059e7b8] clear, six
// fields of golfer record 0x99 are set; in mode [0x00567afc] == 3 the golfer [0x005a59f8] with x > 0 gets the
// image 0x0059b050 + 44 * clamp(byte +0x2e + 2, 0, 4), drawn when that byte is below 3 or when the date has
// bit 4 (|byte| == 3) or bit 2 (otherwise) set.
static void P4_animalsAndImage(u8* F) {
    (void)F;
    CC(0x00401C00);                                                           // tickAnimals 0x00415a77
    int esi = G32(0x005A59F8);                                                // 0x00415a7c
    if (esi == -1 && (G32(0x0059E7B8) & 0x200000) == 0) {                     // 0x00415a85 / 0x00415a91
        short ax = (short)G8(0x004D60A9);                                     // 0x00415a93 movsx ax
        G16(0x00582E6E) = 0;                                                  // 0x00415a9b
        G16(0x00582E70) = ax;                                                 // 0x00415aa4
        GU8(0x00582DD1) = 7;                                                  // 0x00415aaa
        GU8(0x00582E72) = 2;                                                  // 0x00415ab1
        GU8(0x00582DD0) = 0x24;                                               // 0x00415ab8
        GU8(0x00582DD5) = 7;                                                  // 0x00415abf
    }
    if (G32(0x00567AFC) != 3) return;                                         // 0x00415ac6 / 0x00415acd
    u32 g = (u32)esi << 8;                                                    // 0x00415ad3 / 0x00415ad5 (and 0x00415ae6)
    if (G32(g + 0x005794C0) <= 0) return;                                     // 0x00415ad8 / 0x00415ae0 jle
    int c = CC(0x00467130, (int)G8(g + 0x005794EE) + 2, 0, 4);                // clamp 0x00415af8
    s8 bl = G8(g + 0x005794EE);                                               // 0x00415afd
    bool draw;
    if (bl < 3) {                                                             // 0x00415b0a setl, 0x00415b15 jne 0x00415b35
        draw = true;
    } else {
        int a = (int)bl;                                                      // 0x00415b17
        if (a < 0) a = -a;                                                    // 0x00415b1a..0x00415b1d (cdq, xor, sub)
        int mask = (a - 3 != 0) ? 2 : 4;                                      // 0x00415b25..0x00415b2e (neg, sbb, and al 0xfe, add 4)
        draw = (mask & G32(0x00834170)) != 0;                                 // 0x00415b1f / 0x00415b31 / 0x00415b33
    }
    if (!draw) return;
    int y = G32(g + 0x005794C4) - G32(0x004C2844) * 10;                       // 0x00415b35..0x00415b47
    int x = G32(g + 0x005794C0) + 2;                                          // 0x00415b49 / 0x00415b4f
    TC(0x00473E60, (int)(0x0059B050u + 44u * (u32)c), Root(), x, y, 0);       // Image_draw 0x00415b68 (this 0x00415b5a..0x00415b61)
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00415b6d..0x00415c3a: renderSpriteQueue, flushCourseInfo, then the selection resets: [0x004c2848] and
// [0x004c2854] to -1 by mode [0x00567afc] and the flags [0x0055e924] / [0x0055e928]; when either selection was
// set, bit 0x08000000 of [0x0059e7b8] is cleared and the zoom [0x004c2844] becomes 4; from hole 0x13 on a
// selection of 0 is cleared. Then the font target by zoom (>= 4: 0x0051b360) and the label loop counters.
static void P4_queueAndSelection(u8* F) {
    int eax, ecx;
    const int esi = 0;                                                        // 0x00415b7c
    CC(0x00463100);                                                           // renderSpriteQueue 0x00415b6d
    CC(0x00432170);                                                           // flushCourseInfo 0x00415b72
    eax = G32(0x00567AFC);                                                    // 0x00415b77
    if (eax == esi && G32(0x0055E924) == esi) {                               // 0x00415b80 / 0x00415b88
        ecx = G32(0x004C2848);                                                // 0x00415bb8
        goto L_415bbe;
    }
    ecx = -1;                                                                 // 0x00415b8a
    G32(0x004C2848) = ecx;                                                    // 0x00415b8f
    if (eax == esi) {                                                         // 0x00415b95
        if (G32(0x0055E924) != esi) goto L_415d97;                            // 0x00415b9d
        goto L_415bbe;                                                        // 0x00415ba3
    }
    if (eax == 1 && G32(0x0055E928) == esi) goto L_415d97;                    // 0x00415ba8 / 0x00415bb0
    goto L_415bbe;                                                            // 0x00415ba8 / 0x00415bb6
L_415d97:
    eax = G32(0x004C2854);                                                    // 0x00415d97
    if (eax != -1) goto L_415bcb;                                             // 0x00415d9f
    goto L_415bf0;                                                            // 0x00415da5
L_415bbe:
    eax = -1;                                                                 // 0x00415bbe
    G32(0x004C2854) = eax;                                                    // 0x00415bc4
    if (ecx == -1) goto L_415bf0;                                             // 0x00415bc9
L_415bcb:
    G32(0x0059E7B8) = G32(0x0059E7B8) & 0xF7FFFFFF;                           // 0x00415bcb..0x00415bda
    if (eax != -1 || ecx != eax)                                              // 0x00415be0 / 0x00415be4
        G32(0x004C2844) = 4;                                                  // 0x00415be6
L_415bf0:
    if (G32(0x005685F0) >= 0x13 && ecx == esi)                                // 0x00415bf7 / 0x00415bfb
        G32(0x004C2848) = -1;                                                 // 0x00415bfd
    {
        u32 tgt = (G32(0x004C2844) >= 4) ? 0x0051B360u : 0x00519FD8u;         // 0x00415c07..0x00415c16
        TC(0x004762D0, Root(), (int)tgt, esi, esi, esi);                      // setDrawTarget 0x00415c25
    }
    FD(0x18) = esi;                                                           // 0x00415c2a
    FD(0x28) = esi;                                                           // 0x00415c2e
    FD(0xc) = 0x0058BCB8;                                                     // 0x00415c32
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00415c3a..0x00415ec2: labels of the 256 placed objects (0x10 bytes each, [F+0xc] from 0x0058bcb8 to
// 0x0058ccb8; [F+0x18] counts them, [F+0x28] steps by 0xd). An object of kind k >= 5 (word +0) is projected at
// ((x << 10) + o, (y << 10) + o), o = ((byte [0x004c26c0 + 20 k] << 10) - 0x400) / 2 + 0x200. Kind 5 with a
// positive dword +8 (n) writes the name at 0x0055d713 + 37 n plus the string 0x004c68dc and, at zoom 4 with
// bits 0x30 of ([F+0x28] + date) clear, draws it and the string 0x004c68d4 below. Other kinds write
// 0x004c69a8 / 0x004c69a0 (state [0x005a8c38 + 4 k] == 2 / 3, kinds 6 and 8 up) and the name at
// 0x004c26b0 + 20 k, drawn for kinds >= 6 whose byte +7 lacks bit 0x40 (colour 0x80007d08 when bit 3 of
// ([F+0x18] + date) is set, else 0x80006000).
static void P4_objectLabel(u8* F) {
    u32 edx = FU(0xc);                                                        // 0x00415c3a
    short ax = G16(edx);                                                      // 0x00415c3e
    if (ax < 5) return;                                                       // 0x00415c41 / 0x00415c45 jl 0x00415e98
    int ebp = ax;                                                             // 0x00415c4b movsx
    int ebx = ebp * 20;                                                       // 0x00415c57 / 0x00415c5c
    u32 ecx = FU(0xc);                                                        // 0x00415c5f
    int eax = ((int)G8((u32)ebx + 0x004C26C0) << 10) - 0x400;                 // 0x00415c63..0x00415c6d
    eax = eax / 2;                                                            // 0x00415c72 / 0x00415c73 / 0x00415c85 (cdq, sub, sar 1)
    int y = ((int)G16(ecx + 4) << 10) + eax + 0x200;                          // 0x00415c7d / 0x00415c87 / 0x00415c8d
    int x = ((int)G16(ecx + 2) << 10) + eax + 0x200;                          // 0x00415c81 / 0x00415c8a / 0x00415c95
    if (CC(0x0042FB90, x, y, FA(0x148), FA(0x124), 0) == 0) return;           // worldToScreen 0x00415c9d, 0x00415ca7
    int n = 0;
    if (ebp == 5 && (n = G32(FU(0xc) + 8)) > 0) {                             // 0x00415cb0 / 0x00415cb6..0x00415cbf
        StrCpy(kBuf, (u32)(n * 37 + 0x0055D713));                             // 0x00415cc5..0x00415ced
        StrCat(kBuf, 0x004C68DC);                                             // 0x00415cef..0x00415d20
        if (G32(0x004C2844) != 4) return;                                     // 0x00415d13 / 0x00415d22
        if (((FD(0x28) + G32(0x00834170)) & 0x30) != 0) return;               // 0x00415d28..0x00415d36
        CC(0x00404BC0, (int)kBuf, FD(0x148), FD(0x124) - 0xc, (int)0x80007E9F);   // drawCenteredText2 0x00415d59
        int z = G32(0x004C2844);                                              // 0x00415d5e
        int d = 4 - z;                                                        // 0x00415d6b / 0x00415d75
        CC(0x00404BC0, 0x004C68D4, FD(0x148), FD(0x124) + d * 2 + d, (int)0x80007E9F);   // drawCenteredText2 0x00415d8a
        return;                                                               // 0x00415d92
    }
    // 0x00415daa
    GU8(0x0051A068) = 0;                                                      // 0x00415dad
    if (ebp >= 6 && ebp != 7 && ebp != 5) {                                   // 0x00415db4 / 0x00415db9 / 0x00415dbe (ebp != 5 always true here)
        int e = G32((u32)ebp * 4 + 0x005A8C38);                               // 0x00415dc0
        if (e == 2) StrCpy(kBuf, 0x004C69A8);                                 // 0x00415dca, 0x00415dcc..0x00415def
        if (e == 3) StrCpy(kBuf, 0x004C69A0);                                 // 0x00415df4, 0x00415df6..0x00415e19
    }
    StrCat(kBuf, (u32)(ebx + 0x004C26B0));                                    // 0x00415e1b..0x00415e4a
    if (ebp < 6) return;                                                      // 0x00415e47 / 0x00415e4c
    if ((GU8(FU(0xc) + 7) & 0x40) != 0) return;                               // 0x00415e4e..0x00415e56
    int dl = (FD(0x18) + G32(0x00834170)) & 8;                                // 0x00415e58..0x00415e6b
    int color = (int)((dl != 0 ? 0x1D08u : 0u) + 0x80006000u);                // 0x00415e75..0x00415e82 (neg dl, sbb, and, add)
    CC(0x00404BC0, (int)kBuf, FD(0x148), FD(0x124) - 0x10, color);            // drawCenteredText2 0x00415e90
}

static void P4_objectLabels(u8* F) {
    for (;;) {
        P4_objectLabel(F);                                                    // 0x00415c3a..0x00415e95
        int eax = FD(0xc);                                                    // 0x00415e98
        int edi = FD(0x18);                                                   // 0x00415e9c
        int esi = FD(0x28);                                                   // 0x00415ea0
        eax += 0x10;                                                          // 0x00415ea4
        edi += 1;                                                             // 0x00415ea7
        esi += 0xd;                                                           // 0x00415ea8
        FD(0x18) = edi;                                                       // 0x00415eb0
        FD(0x28) = esi;                                                       // 0x00415eb4
        FD(0xc) = eax;                                                        // 0x00415eb8
        if (eax >= 0x0058CCB8) break;                                         // 0x00415eab / 0x00415ebc jl 0x00415c3a
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00415ec2..0x00416305: with bit 1 of [0x0059e7b8] set and mode m = [0x00542f20] != 3, the cells around the
// cursor cell (cx, cy) = ([0x00543d08], [0x00543d0c]): rows edi = cx - 1 .. cx + (m == 1 ? 2 : 1), columns
// ebx = cy - (m == 1) .. cy + 2. Each projected cell gets its height (heightBlend - 3) as text (0x004e9a84 for 0,
// else 0x004c5340 + __itoa), the font 0x0051b360 at (cx, cy + 1), 0x00519fd8 elsewhere, and 0x00519928 in mode 1
// inside the 2x2 block at (cx, cy); modes 0 / 2 mark (cx, cy + 1) with a 5x3 rectangle, mode 2 the other cells
// with a 3x2 one; mode 1 draws the edge lines of (cx, cy + 1) and (cx + 1, cy).
// Frame: [F+0x28] row, [F+0xc] (row << 10) - 1, [F+0x18] row << 10, [F+0x10] column - 1, [F+0x34] column << 10,
// [F+0x8c] / [F+0x94] screen point, [F+0xa4] / [F+0x9c] second point.
static void P4_holeGrid(u8* F) {
    if ((GU8(0x0059E7B8) & 2) == 0) return;                                   // 0x00415ec2 / 0x00415ec9
    int ecx = G32(0x00542F20);                                                // 0x00415ecf
    if (ecx == 3) return;                                                     // 0x00415ed5 / 0x00415ed8
    int ebp = G32(0x00543D08);                                                // 0x00415ede
    int edx = ((ecx - 1) != 0 ? -1 : 0) + 2 + ebp;                            // 0x00415ee4..0x00415ef5 (dec, neg, sbb, add 2)
    int eax = ebp - 1;                                                        // 0x00415eeb
    FD(0x28) = eax;                                                           // 0x00415ef1
    if (eax > edx) return;                                                    // 0x00415ef7 / 0x00415ef9 jg 0x00416305
    FD(0xc) = (eax << 10) - 1;                                                // 0x00415eff..0x00415f03
    for (;;) {
        int esi = G32(0x00543D0C);                                            // 0x00415f07
        int edi = FD(0x28);                                                   // 0x00415f0f
        int ebx = esi - (ecx == 1 ? 1 : 0);                                   // 0x00415f13..0x00415f1e (sete)
        esi += 2;                                                             // 0x00415f1b
        if (ebx <= esi) {                                                     // 0x00415f20 / 0x00415f22 jg 0x004162de
            FD(0x18) = edi << 10;                                             // 0x00415f28 / 0x00415f2c / 0x00415f32
            FD(0x10) = ebx - 1;                                               // 0x00415f2f / 0x00415f39
            ebp = (ebx << 10) - 1;                                            // 0x00415f2a / 0x00415f36 / 0x00415f3d
            for (;;) {
                int c18 = FD(0x18);                                           // 0x00415f3e
                esi = ebx << 10;                                              // 0x00415f42 / 0x00415f55
                FD(0x34) = esi;                                               // 0x00415f5b
                if (CC(0x0042FB90, c18, esi, FA(0x8c), FA(0x94), 0) != 0) {   // worldToScreen 0x00415f5f, 0x00415f69
                    u32 tgt = 0x00519FD8;                                     // 0x00415f87
                    if (edi == G32(0x00543D08) && ebx == G32(0x00543D0C) + 1) // 0x00415f6f / 0x00415f75 / 0x00415f77..0x00415f85
                        tgt = 0x0051B360;                                     // 0x00415f7d
                    TC(0x004762D0, Root(), (int)tgt, 0, 0, 0);                // setDrawTarget 0x00415f99
                    if (G32(0x00542F20) == 1) {                               // 0x00415f9e / 0x00415fa5
                        int a = G32(0x00543D08);                              // 0x00415fa7
                        if (edi == a || edi == a + 1) {                       // 0x00415fae / 0x00415fb0..0x00415fb3
                            a = G32(0x00543D0C);                              // 0x00415fb5
                            if (ebx == a || ebx == a + 1)                     // 0x00415fbc / 0x00415fbe..0x00415fc1
                                TC(0x004762D0, Root(), 0x00519928, 0, 0, 0);  // setDrawTarget 0x00415fd4
                        }
                    }
                    int h = CC(0x0040C170, edi, FD(0x10), 0);                 // heightBlend 0x00415fe1 (three pushes)
                    h -= 3;                                                   // 0x00415feb
                    StrCpy(kBuf, h == 0 ? 0x004E9A84u : 0x004C5340u);         // 0x00415fee / 0x00415ff3 / 0x00415ff5, 0x00415ffa..0x00416020
                    int r = Itoa(h, 0x0058A528, 10);                          // __itoa 0x00416022
                    StrCat(kBuf, (u32)r);                                     // 0x00416027..0x0041605e
                    CC(0x00404BC0, (int)kBuf, FD(0x8c), FD(0x94) - 0xf, (int)0x80007FFF);   // drawCenteredText2 0x0041606e
                    edi = FD(0x28);                                           // 0x00416073
                    esi = FD(0x34);                                           // 0x00416077
                }
                // 0x0041607e
                int m = G32(0x00542F20);                                      // 0x0041607e
                if (m == 0 || m == 2) {                                       // 0x00416085 / 0x00416087 / 0x0041608a
                    if (edi == G32(0x00543D08) && ebx == G32(0x00543D0C) + 1) {   // 0x0041608c / 0x00416092 / 0x00416094..0x0041609d
                        TC(0x00480B00, 0x00519A60, FD(0x8c) - 2, FD(0x94) - 1, 5, 3, (int)0x80006090);   // fillWindowRect 0x004160e2 (via 0x004160bc)
                    } else if (m == 2) {                                      // 0x004160be / 0x004160c1
                        TC(0x00480B00, 0x00519A60, FD(0x8c) - 1, FD(0x94) - 1, 3, m, (int)0x80006090);   // fillWindowRect 0x004160e2
                    }
                }
                // 0x004160e7
                if (G32(0x00542F20) == 1) {                                   // 0x004160e7 / 0x004160ee
                    if (edi == G32(0x00543D08) && ebx == G32(0x00543D0C) + 1) {   // 0x004160f4 / 0x004160fa / 0x00416100..0x00416109
                        CC(0x0042FB90, FD(0x18), ebp, FA(0x8c), FA(0x94), 0); // worldToScreen 0x00416127 (eax not read)
                        if (CC(0x0042FB90, FD(0x18), ebp - 0x3ff, FA(0xa4), FA(0x9c), 0) != 0)   // worldToScreen 0x0041614a, 0x00416154
                            CC(0x004493D0, FD(0xa4), FD(0x9c), FD(0x8c), FD(0x94), (int)0x80006090, 3, 7);   // drawMapLine 0x0041617f
                        if (CC(0x0042FB90, FD(0xc) + 0x400, ebp, FA(0xa4), FA(0x9c), 0) != 0)   // worldToScreen 0x004161a4, 0x004161ae
                            CC(0x004493D0, FD(0xa4), FD(0x9c), FD(0x8c), FD(0x94), (int)0x80006090, 3, 7);   // drawMapLine 0x004161d9
                    }
                    // 0x004161e1
                    if (edi == G32(0x00543D08) + 1 && ebx == G32(0x00543D0C)) {   // 0x004161e1..0x004161f6
                        if (CC(0x0042FB90, FD(0xc), ebp + 0x400, FA(0xa4), FA(0x9c), 0) != 0)   // worldToScreen 0x0041621a, 0x00416224
                            CC(0x004493D0, FD(0xa4), FD(0x9c), FD(0x8c), FD(0x94), (int)0x80006090, 3, 7);   // drawMapLine 0x0041624f
                        if (CC(0x0042FB90, FD(0xc) - 0x3ff, esi, FA(0xa4), FA(0x9c), 0) != 0)   // worldToScreen 0x00416275, 0x0041627f
                            CC(0x004493D0, FD(0xa4), FD(0x9c), FD(0x8c), FD(0x94), (int)0x80006090, 3, 7);   // drawMapLine 0x004162aa
                    }
                }
                // 0x004162b2
                int lim = G32(0x00543D0C) + 2;                                // 0x004162b2 / 0x004162bc
                esi = FD(0x10) + 1;                                           // 0x004162b7 / 0x004162bf
                ebx += 1;                                                     // 0x004162bb
                ebp += 0x400;                                                 // 0x004162c0
                FD(0x10) = esi;                                               // 0x004162c8
                if (ebx > lim) break;                                         // 0x004162c6 / 0x004162cc jle 0x00415f3e
            }
            ebp = G32(0x00543D08);                                            // 0x004162d2
            ecx = G32(0x00542F20);                                            // 0x004162d8
        }
        // 0x004162de
        int b = FD(0xc) + 0x400;                                              // 0x004162de / 0x004162e5
        edi += 1;                                                             // 0x004162e4
        FD(0x28) = edi;                                                       // 0x004162ec
        edx = ((ecx - 1) != 0 ? -1 : 0) + 2 + ebp;                            // 0x004162e2 / 0x004162eb..0x004162fb
        FD(0xc) = b;                                                          // 0x004162f4
        if (edi > edx) break;                                                 // 0x004162fd / 0x004162ff jle 0x00415f07
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00416305..0x00416838: the ball label. With [0x00822c88] == 0, date below 0x2000 and walker 0 (0x00585850)
// active (byte +0x12), in state 0xfa (+0x13) and with byte +0x15 clear, the walker's projection gets a text built
// from the hole records and the date flags, drawn on a panel when the date has bit 3 / 4 set (first arm) or when
// it lacks bit 5, is at most 0x2000, [0x00822c88] == 0 and [0x005a59f8] == -1 (second arm).
static void P4_ballLabelDraw(u8* F) {
    if (GU8(0x0051A068) == 0) return;                                         // 0x00416760 / 0x00416767
    int z = G32(0x004C2844);                                                  // 0x0041676d
    int esi = z * 10;                                                         // 0x00416772 / 0x00416775
    u32 tgt = (z > 4) ? 0x0051B360u : 0x00519FD8u;                            // 0x00416777..0x00416781
    TC(0x004762D0, Root(), (int)tgt, 0, 0, 0);                                // setDrawTarget 0x00416793
    int w = TC(0x00477250, Root(), (int)kBuf);                                // Text_put 0x004167a3
    int ecx = FD(0x130) - esi;                                                // 0x004167a8 / 0x004167b3
    int h = (w + 8) / 2;                                                      // 0x004167af / 0x004167b2 / 0x004167b5 / 0x004167b7
    int edi = ecx + 5;                                                        // 0x004167b9
    ecx = FD(0x140);                                                          // 0x004167bc
    CC(0x004493D0, ecx - h, edi, h + ecx, edi, (int)0x80000000, 0xa, 5);      // drawMapLine 0x004167d5
    int y = FD(0x130);                                                        // 0x004167da
    int s = G32(0x00822C74);                                                  // 0x004167e1
    int x = FD(0x140) + 4;                                                    // 0x004167e7 / 0x004167f3
    y = y - esi + 0xa;                                                        // 0x004167f1 / 0x004167fc
    TC(0x00474030, 0x005A4100, Root(), x, y, s, 1, 1, 1, 0);                  // Surface_drawScaled2 0x0041680f
    CC(0x00404B70, (int)kBuf, FD(0x140), FD(0x130) - esi, (int)0x80007FFF);   // drawCenteredText 0x00416830
}

static void P4_ballLabel(u8* F) {
    if (G32(0x00822C88) != 0) return;                                         // 0x00416305 / 0x0041630c
    if (G32(0x00834170) >= 0x2000) return;                                    // 0x00416312 / 0x0041631c
    if (GU8(0x00585862) == 0) return;                                         // 0x00416322 / 0x00416329
    if (GU8(0x00585863) != 0xfa) return;                                      // 0x0041632f / 0x00416336
    if (GU8(0x00585865) != 0) return;                                         // 0x0041633c / 0x00416343
    if (CC(0x0042FB90, G32(0x00585850), G32(0x00585854), FA(0x140), FA(0x130), 0) == 0) return;   // worldToScreen 0x00416368, 0x00416372
    u8 al = GU8(0x00575CB8);                                                  // 0x00416378
    GU8(0x0051A068) = 0;                                                      // 0x0041637d
    if (al == 0) {                                                            // 0x00416384 / 0x00416386 jne 0x004164aa
        StrCpy(kBuf, 0x004C68B8);                                             // 0x0041638c..0x004163b7
        int e = G32(0x00575CD0);                                              // 0x004163ac (edx)
        if (e == 0) StrCpy(kBuf, 0x004C6898);                                 // 0x004163b5 / 0x004163b9, 0x004163bb..0x004163dc
        int bp = G32(0x00575CC0);                                             // 0x004163de
        if (bp == 0) StrCpy(kBuf, 0x004C6878);                                // 0x004163e4 / 0x004163e6, 0x004163e8..0x0041640b
        u8 bl = GU8(0x00834170);                                              // 0x0041640d
        if ((bl & 0x20) == 0) {                                               // 0x00416413 / 0x00416416
            StrCpy(kBuf, 0x004C6854);                                         // 0x00416418..0x0041643d
            if (e != 0 && bp != 0)                                            // 0x0041643b / 0x0041643f / 0x00416441 / 0x00416443
                StrCpy(kBuf, 0x004C6828);                                     // 0x00416445..0x00416465, shared tail 0x00416490..0x00416493
        } else {
            if (G32(0x00567AFC) != 0)                                         // 0x00416467 / 0x0041646e
                StrCpy(kBuf, 0x004C6800);                                     // 0x00416470..0x00416493
        }
        if ((bl & 0x18) != 0) {                                               // 0x00416495 / 0x00416498 jne 0x00416760
            P4_ballLabelDraw(F);
            return;
        }
        GU8(0x0051A068) = 0;                                                  // 0x0041649e
        return;                                                               // 0x004164a5 jmp 0x00416838
    }
    // 0x004164aa
    if (GU8(0x00575EC0) == 0) {                                               // 0x004164aa / 0x004164b1
        StrCpy(kBuf, 0x004C67D8);                                             // 0x004164b3..0x004164dd
        if (GU8(0x005795D9) == 1 && GU8(0x005795DA) == 0)                     // 0x004164c3 / 0x004164db / 0x004164df, 0x004164e1..0x004164e8
            StrCpy(kBuf, 0x004C67C0);                                         // 0x004164ea..0x0041650d
    }
    if (GU8(0x005760C8) != 0) {                                               // 0x0041650f / 0x00416516
        int hole = CC(0x00407340, G32(0x00585850) >> 10, G32(0x00585854) >> 10);   // nearestHole 0x00416530 (sar 0x00416528 / 0x0041652b)
        u32 rec = (u32)hole * 0x208u;                                         // 0x00416535..0x00416543
        int a = G32(rec + 0x00575AD4) / 2;                                    // 0x0041654b..0x0041655a (cdq, sub, sar 1)
        int div = a + G32(rec + 0x00575AD0) + 4;                              // 0x00416554 / 0x0041655c
        int q = ((int)G16(rec + 0x00575C08) * 100) / div;                     // 0x00416560..0x00416574 (idiv, before the copy)
        StrCpy(kBuf, 0x004C67AC);                                             // 0x00416537 / 0x00416578..0x00416594
        int r = Itoa(q, 0x0058A528, 10);                                      // __itoa 0x00416596
        StrCat(kBuf, (u32)r);                                                 // 0x0041659b..0x004165c6
        StrCat(kBuf, 0x004C679C);                                             // 0x004165c8..0x004165f8
        if (G16(0x004C2C9C) == 0) {                                           // 0x004165f1 (ax = 0 from 0x004165a0) / 0x004165fa
            StrCpy(kBuf, 0x004C677C);                                         // 0x004165fc..0x0041661a, shared tail 0x00416648..0x0041664b
        } else if ((G32(0x0059E7B8) & 0x400000) != 0) {                       // 0x0041661c / 0x00416626
            StrCpy(kBuf, 0x004C675C);                                         // 0x00416628..0x0041664b
        }
    }
    // 0x0041664d
    int edx = G32(0x00834170);                                                // 0x0041664d
    if ((edx & 0x40) != 0) {                                                  // 0x00416653 / 0x00416656
        int m = G32(0x00567AFC);                                              // 0x0041665c
        if (m == 0) {                                                         // 0x00416661 / 0x00416664
            u32 s;
            if (G32(0x0055E924) != 0)                                         // 0x004166f4 / 0x004166fb
                s = (edx & 0x80) != 0 ? 0x004C664Cu : 0x004C6624u;            // 0x004166fd / 0x00416700 / 0x00416702 / 0x00416709
            else
                s = 0x004C6600;                                               // 0x00416710
            StrCpy(kBuf, s);                                                  // 0x00416715..0x00416733
        } else if (m == 1) {                                                  // 0x0041666a / 0x0041666b
            u32 s;
            if (G32(0x0055E928) != 0) s = 0x004C66D8;                         // 0x004166cd / 0x004166d4 / 0x004166d6
            else if (G32(0x00822C88) != 0) s = 0x004C66B0;                    // 0x004166dd / 0x004166e4 / 0x004166e6
            else s = 0x004C6680;                                              // 0x004166ed
            StrCpy(kBuf, s);                                                  // 0x00416715..0x00416733
        } else if (m == 2) {                                                  // 0x0041666d / 0x0041666e
            if (GU8(0x005858AE) == 0) {                                       // 0x00416674 / 0x0041667b
                StrCpy(kBuf, 0x004C6720);                                     // 0x0041667d, 0x00416715..0x00416733
            } else {
                GU8(0x0051A068) = 0;                                          // 0x00416689
                CC(0x00467600, 1);                                            // pickGolferName 0x00416690
                StrCat(kBuf, 0x004C6708);                                     // 0x00416695..0x004166c9
                edx = G32(0x00834170);                                        // 0x004166c0
            }
        }
    }
    // 0x00416735
    if ((edx & 0x20) == 0 && edx <= 0x2000 && G32(0x00822C88) == 0 && G32(0x005A59F8) == -1) {   // 0x00416738 / 0x00416740 / 0x00416749 / 0x00416752
        P4_ballLabelDraw(F);                                                  // 0x00416760
        return;
    }
    GU8(0x0051A068) = 0;                                                      // 0x00416754
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00416838..0x00416e48: the 152 golfer records (esi from 0x005794c0 by 0x100 to 0x00582cc0; [F+0x14] is the
// index). An active golfer (byte +0x19) on screen (projection of -8 / -4 into [F+0x78] / [F+0x74]) gets, at zoom
// 4 with no selection and in states 0xc / 0xd, a minimap inset; with comment byte +0x7c in 1..7 at zoom > 2, the
// comment text (buildGolferComment), framed by a thought bubble for comment id 0x32 or a panel when bit 4 of
// [0x005a5a00] is set; comment 5 with flags 0xc000 at a hole quadrant emits thought 0x2c.
// Frame: [F+0xc] text colour, [F+0x18] text width + 8, [F+0x2c] / [F+0x34] / [F+0x28] / [F+0x10] spills.
static void P4_golferOne(u8* F, u32 esi) {
    if (GU8(esi + 0x19) == 0) return;                                         // 0x00416845 / 0x0041684a
    if (CC(0x0042FB90, G32(esi - 8), G32(esi - 4), FA(0x78), FA(0x74), 0) == 0) return;   // worldToScreen 0x00416867, 0x00416871
    int edx = G32(0x004C2844);                                                // 0x00416877
    int ebx = edx * 10;                                                       // 0x00416882 / 0x00416885
    if (G32(0x005A9CBC) == 0 && G32(0x004C2E08) != FD(0x14) && GU8(0x00569498) == 0 &&
        G32(0x004C2848) == -1 && G32(0x004C2854) == -1 && edx == 4) {         // 0x00416889 / 0x0041689b / 0x004168a8 / 0x004168b9 / 0x004168c5 / 0x004168ce
        u8 al = GU8(esi + 0x15);                                              // 0x004168d4
        bool go;
        if (al == 0xc && (GU8(esi + 0x7d) != 1 || G32(0x00834170) < 0x2000))  // 0x004168d9 / 0x004168df / 0x004168eb
            go = true;
        else
            go = (al == 0xd);                                                 // 0x004168ed / 0x004168ef
        if (go && G32(0x00567AFC) != 3) {                                     // 0x004168f5 / 0x004168fc
            int eax = G32(esi);                                               // 0x00416902
            int ecx = 0;
            if (eax > 0x50 && (ecx = G32(esi + 4)) > 0x50 && eax < 0x2d0 && ecx < 0x208 &&   // 0x00416907 / 0x00416913 / 0x0041691e / 0x0041692a
                (GU8(esi + 0x10) != 0 || esi == 0x005794C0)) {                // 0x00416935 / 0x0041693d
                FD(0x2c) = CC(0x00467130, eax, 0x28, 0x2f8);                  // clamp 0x0041694b, 0x0041695e
                int e2 = G32(esi + 4) - 0xa;                                  // 0x00416950 / 0x00416958
                FD(0x34) = CC(0x00467130, e2, 0x28, 0x230);                   // clamp 0x00416962, 0x00416967
                int edi = CC(0x00467130, G32(esi), 0x50, 0x2d0);              // clamp 0x00416975
                int e3 = G32(esi + 4) - 0xa;                                  // 0x0041697a / 0x00416982
                int ebp = CC(0x00467130, e3, 0x50, 0x208);                    // clamp 0x0041698a
                // 0x004169b5..0x004169c1: neg / sbb / and on the constant 0x00519a60, so the source is 0x00519cd4
                TC(0x00475D00, 0x00519CD4, 0x00519CD4, FD(0x2c) - 0x28, FD(0x34) - 0x28, 0x50, 0x46,
                   edi - 0x50, ebp - 0x50, 0xa0, 0x8c);                       // Surface_blit3 0x004169ce
                CC(0x00406220, edi - 0x4f, ebp + 0x3c, edi + 0x51, ebp + 0x3c, (int)0x80000000, 2, 7);   // drawMinimapSegment 0x004169e9
                CC(0x00406220, edi + 0x50, ebp - 0x4f, edi + 0x50, ebp + 0x3c, (int)0x80000000, 2, 7);   // drawMinimapSegment 0x00416a04
                edx = G32(0x004C2844);                                        // 0x00416a09
                ebx += 0x10;                                                  // 0x00416a12
            }
        }
    }
    // 0x00416a15
    u8 c7c = GU8(esi + 0x7c);                                                 // 0x00416a15
    if (c7c == 0) return;                                                     // 0x00416a1a
    if (c7c > 7) return;                                                      // 0x00416a20 / 0x00416a22 ja
    if (edx <= 2) return;                                                     // 0x00416a28 / 0x00416a2b jle
    u8 a7d = GU8(esi + 0x7d);                                                 // 0x00416a31
    GU8(0x0051A068) = 0;                                                      // 0x00416a34
    int idx;
    if ((a7d & 0x80) != 0)                                                    // 0x00416a3b / 0x00416a3d
        idx = G16(esi + 0x9a);                                                // 0x00416a3f movsx
    else
        idx = FD(0x14);                                                       // 0x00416a48
    int w94 = GU16(esi + 0x94) & 0x3fff;                                      // 0x00416a4c / 0x00416a56
    int b7e = GU8(esi + 0x7e);                                                // 0x00416a54 / 0x00416a5c
    CC(0x00469B00, (int)(a7d & 0x7f), w94, b7e, idx);                         // buildGolferComment 0x00416a65
    u8 al = GU8(esi + 0x7c);                                                  // 0x00416a6a
    u8 cl = GU8(esi + 0x7d);                                                  // 0x00416a6d
    int ebp = (int)(((al < 5) ? 0xFFFFE319u : 0u) + 0x80007FFFu);             // 0x00416a73..0x00416a7d (cmp, sbb, and, add)
    FD(0xc) = ebp;                                                            // 0x00416a86
    if ((cl & 0x80) == 0 && cl == (u8)(GU8(esi + 0x68) & 0x7f)) {             // 0x00416a83 / 0x00416a8a / 0x00416a8c..0x00416a94
        u16 cx = (u16)(GU16(esi + 0x80) & 0xc000);                            // 0x00416a96 / 0x00416a9d
        if (cx == 0x4000) {                                                   // 0x00416aa2 / 0x00416aa7
            ebp = (int)(((al < 5) ? 0xFFFFDF18u : 0u) + 0x800043F0u);         // 0x00416aa9..0x00416ab3
            FD(0xc) = ebp;                                                    // 0x00416ab9
        }
        if (cx == 0xc000) {                                                   // 0x00416abd / 0x00416ac2
            ebp = (int)(((al < 5) ? 0xFFFFE2F8u : 0u) + 0x80007D08u);         // 0x00416ac4..0x00416ace
            FD(0xc) = ebp;                                                    // 0x00416ad4
        }
    }
    {
        u32 tgt = (G32(0x004C2844) > 4) ? 0x0051B360u : 0x00519FD8u;          // 0x00416ad8..0x00416ae7
        TC(0x004762D0, Root(), (int)tgt, 0, 0, 0);                            // setDrawTarget 0x00416af9
    }
    int w = TC(0x00477250, Root(), (int)kBuf);                                // Text_put 0x00416b09
    u32 edi = (u32)((int)G16(esi + 0x9a) << 8);                               // 0x00416b0e / 0x00416b15
    int ecx = w + 8;                                                          // 0x00416b18 / 0x00416b20
    FD(0x18) = ecx;                                                           // 0x00416b25
    if (GU8(edi + 0x0057953C) != 0) {                                         // 0x00416b1a / 0x00416b23 / 0x00416b29
        int d = G32(esi + 4) - G32(edi + 0x005794C4);                         // 0x00416b2b..0x00416b34
        if (d < 0) d = (int)(0u - (u32)d);                                    // 0x00416b36..0x00416b39 (cdq, xor, sub)
        if (d <= 0xc) {                                                       // 0x00416b3b / 0x00416b3e jg 0x00416b5e
            int a = G32(esi + 4);                                             // 0x00416b40
            int b = G32(edi + 0x005794C4);                                    // 0x00416b43
            if (a > b) ebx -= 4;                                              // 0x00416b4b jg 0x00416b5b
            else if (a != b) ebx += 4;                                        // 0x00416b4d jne 0x00416b56
            else if ((FUB(0x14) & 1) != 0) ebx -= 4;                          // 0x00416b4f / 0x00416b54 jne 0x00416b5b
            else ebx += 4;                                                    // 0x00416b56
        }
    }
    // 0x00416b5e
    int color;
    if ((GU8(esi + 0x7d) & 0x7f) == 0x32) {                                   // 0x00416b5e..0x00416b65 jne 0x00416d43
        int eax = G32(edi + 0x005794C4) - G32(esi + 4);                       // 0x00416b6b / 0x00416b71 / 0x00416b77
        int c8 = G32(esi + 8) & 0x100000;                                     // 0x00416b74 / 0x00416b7a
        eax = eax / 2;                                                        // 0x00416b79 / 0x00416b80 / 0x00416b84 (cdq, sub, sar 1)
        int e = (c8 != 0 ? 0xe : 0) + -7;                                     // 0x00416b82..0x00416b8d (neg, sbb, and 0xe, add -7)
        e -= eax;                                                             // 0x00416b90
        ebx = e + G32(0x004C2844) * 10;                                       // 0x00416b92..0x00416b9a
        int t = G16(esi + 0xae);                                              // 0x00416b9d movsx
        FD(0x2c) = (int)G8((u32)t * 0x230u + 0x004D60AA);                     // 0x00416ba4..0x00416bbc
        int edi2;
        if (c8 != 0) {                                                        // 0x00416bb3 / 0x00416bc0 je 0x00416c7b
            int f18 = FD(0x18);                                               // 0x00416bc6
            int c = FD(0x78);                                                 // 0x00416bca
            int bp = FD(0x74);                                                // 0x00416bce
            c -= 0xc;                                                         // 0x00416bd2
            edi2 = f18 / 2;                                                   // 0x00416bd5..0x00416be0 (cdq, sub, sar 1)
            FD(0x78) = c;                                                     // 0x00416bd8
            bp = bp - ebx - 0x2a;                                             // 0x00416bde / 0x00416be8
            c = c - edi2 - 0x38;                                              // 0x00416be4 / 0x00416bed
            TC(0x00473CB0, 0x005A58C4, Root(), c, bp, 1, 1, 2, 0);            // Surface_drawScaled 0x00416c00
            int x = FD(0x74);                                                 // 0x00416c05
            FD(0x10) = Root();                                                // 0x00416c09 / 0x00416c11
            FD(0x34) = x - ebx - 0x2a;                                        // 0x00416c0f / 0x00416c15 / 0x00416c18
            FD(0x28) = FD(0x78) - edi2 - 0x38;                                // 0x00416c1c..0x00416c25
            int r1 = CC(0x0046C940, FD(0x14));                                // typeBit7Clear 0x00416c2e
            int k = FD(0x2c) + r1 * 72;                                       // 0x00416c33 / 0x00416c37 / 0x00416c3e
            int r2 = CC(0x004675D0, FD(0x14));                                // thoughtFlag 0x00416c46 (argument read 0x00416c3a)
            k = k * 3 + r2;                                                   // 0x00416c42 / 0x00416c4e
            TC(0x00473CB0, (int)(0x005628E8u + 44u * (u32)k), FD(0x10), FD(0x28), FD(0x34), 1, 1, 2, 0);   // Surface_drawScaled 0x00416d14 (via 0x00416c76)
        } else {
            // 0x00416c7b
            int f18 = FD(0x18);                                               // 0x00416c7b
            int c = FD(0x74);                                                 // 0x00416c7f
            int bp = FD(0x78);                                                // 0x00416c83
            c -= ebx;                                                         // 0x00416c87
            edi2 = f18 / 2;                                                   // 0x00416c89..0x00416c93 (cdq, sub, sar 1)
            bp += 0xc;                                                        // 0x00416c90
            c -= 0x2a;                                                        // 0x00416c97
            FD(0x78) = bp;                                                    // 0x00416c9c
            bp = edi2 + bp - 0xc;                                             // 0x00416ca5
            TC(0x00473CB0, 0x005A58C4, Root(), bp, c, 1, 1, 2, 0);            // Surface_drawScaled 0x00416cb7
            int y = FD(0x74);                                                 // 0x00416cbc
            int x = FD(0x78);                                                 // 0x00416cc0
            y = y - ebx - 0x2a;                                               // 0x00416cc6 / 0x00416ccc
            int root = Root();                                                // 0x00416cd2
            x = edi2 + x + 0x18;                                              // 0x00416cd8
            int r1 = CC(0x0046C940, FD(0x14));                                // typeBit7Clear 0x00416ce3 (argument read 0x00416cdc)
            int k = FD(0x2c) + r1 * 72;                                       // 0x00416ce8 / 0x00416cec / 0x00416cf3
            int r2 = CC(0x004675D0, FD(0x14));                                // thoughtFlag 0x00416cfb (argument read 0x00416cef)
            k = k * 3 + r2;                                                   // 0x00416cf7 / 0x00416d03
            TC(0x00473CB0, (int)(0x005628E8u + 44u * (u32)k), root, x, y, -1, 1, 2, 0);   // Surface_drawScaled 0x00416d14
        }
        // 0x00416d19
        TC(0x00480B00, 0x00519A60, FD(0x78) - edi2, FD(0x74) - ebx, FD(0x18), 0xb, (int)0x80002008);   // fillWindowRect 0x00416d38
        color = FD(0xc);                                                      // 0x00416d3d
    } else {
        // 0x00416d43
        color = ebp;
        if ((GU8(0x005A5A00) & 0x10) != 0) {                                  // 0x00416d43 / 0x00416d4a
            int e = FD(0x74) - ebx + 5;                                       // 0x00416d4c / 0x00416d52 / 0x00416d5b
            int h = ecx / 2;                                                  // 0x00416d5e / 0x00416d68..0x00416d6b
            int c = FD(0x78);                                                 // 0x00416d60
            CC(0x004493D0, c - h, e, h + c, e, (int)0x80000000, 0xa, 5);      // drawMapLine 0x00416d75
            int y = FD(0x74);                                                 // 0x00416d7a
            int s = G32(0x00822C74);                                          // 0x00416d81
            int x = FD(0x78) + 4;                                             // 0x00416d87 / 0x00416d93
            y = y - ebx + 0xa;                                                // 0x00416d91 / 0x00416d9c
            TC(0x00474030, 0x005A4100, Root(), x, y, s, 1, 1, 1, 0);          // Surface_drawScaled2 0x00416daf
        } else {
            GU8(0x0051A068) = 0;                                              // 0x00416db6
        }
    }
    // 0x00416dbd
    CC(0x00404B70, (int)kBuf, FD(0x78), FD(0x74) - ebx, color);               // drawCenteredText 0x00416dcf
    {
        int c74 = FD(0x74);                                                   // 0x00416dd4
        u8 a = GU8(esi + 0x7c);                                               // 0x00416ddb
        FD(0x74) = c74 + 8;                                                   // 0x00416de1 / 0x00416de6
        if (a != 5) return;                                                   // 0x00416de4 / 0x00416dea
    }
    if ((GU16(esi + 0x80) & 0xc000) != 0xc000) return;                        // 0x00416dec..0x00416dfd
    if (CC(0x00407000, G32(esi - 8), G32(esi - 4), 1) == 0) return;           // holeQuadrants 0x00416e09, 0x00416e13
    CC(0x00467A00, FD(0x14), 0x2c, G32(0x005694A0) | 0x100);                  // emitGolferThought 0x00416e25 (or ah, 1 at 0x00416e1e)
}

static void P4_golfers(u8* F) {
    FD(0x14) = 0;                                                             // 0x00416838
    u32 esi = 0x005794C0;                                                     // 0x00416840
    for (;;) {
        P4_golferOne(F, esi);                                                 // 0x00416845..0x00416e2a
        int edx = FD(0x14);                                                   // 0x00416e2d
        esi += 0x100;                                                         // 0x00416e31
        edx += 1;                                                             // 0x00416e37
        FD(0x14) = edx;                                                       // 0x00416e3e
        if ((int)esi >= 0x00582CC0) break;                                    // 0x00416e38 / 0x00416e42 jl 0x00416845
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00416e48..0x00417277: the 64 walker records (0x4c bytes; ebx = record + 0x15 from 0x00585865 to 0x00586b65,
// ebp = 0x23 * index). A walker with flags byte +0x12 (ebx - 3) and comment byte +0x15 in 1..7 on screen at zoom
// > 2 gets a text chosen by its kind byte +0x14 (ebx - 1) through the switch 0x00421740 / 0x00421720 (kind 35:
// by ((date + ebp) / 100) % 4 through 0x0042176c), six bubble pieces of Surface 0x005787d0 and the text drawn
// above it; [F+0xa0] / [F+0x80] hold the projection, [F+0x80] steps by 8 after the text.
static void P4_walkerOne(u8* F, u32 ebx, int ebp) {
    if (GU8(ebx - 3) == 0) return;                                            // 0x00416e4f / 0x00416e54
    if (GU8(ebx) == 0) return;                                                // 0x00416e5a / 0x00416e5d
    if (CC(0x0042FB90, G32(ebx - 0x15), G32(ebx - 0x11), FA(0xa0), FA(0x80), 0) == 0) return;   // worldToScreen 0x00416e7d, 0x00416e87
    s8 al = G8(ebx);                                                          // 0x00416e8d
    if (al == 0) return;                                                      // 0x00416e91
    if (al > 7) return;                                                       // 0x00416e97 / 0x00416e99 jg
    if (G32(0x004C2844) <= 2) return;                                         // 0x00416e9f / 0x00416ea6 jle
    int k = (int)G8(ebx - 1) + 7;                                             // 0x00416eac / 0x00416eb0
    if ((u32)k <= 0x2a) {                                                     // 0x00416eb3 / 0x00416eb6 ja 0x00417068
        switch (GU8(0x00421740u + (u32)k)) {                                  // 0x00416ebe index byte, 0x00416ec4 jmp [0x00421720 + 4 i]
        case 0:                                                               // 0x00416fe0 (kind -7)
            if (CC(0x0046C940, (int)G16(ebx + 0xb)) != 0)                     // typeBit7Clear 0x00416fe5, 0x00416fef
                StrCpy(kBuf, 0x004C652C);                                     // 0x00416ff1, 0x00417048..0x00417066
            else
                StrCpy(kBuf, 0x004C6510);                                     // 0x00416ff8..0x00417018, shared tail 0x00417063
            break;
        case 1:                                                               // 0x00417021 (kind -6)
            StrCpy(kBuf, 0x004C64EC);                                         // 0x00417021..0x00417041, shared tail 0x00417063
            break;
        case 2:                                                               // 0x00417043 (kind -5)
            StrCpy(kBuf, 0x004C64D8);                                         // 0x00417043..0x00417066
            break;
        case 3:                                                               // 0x0041701a (kind -4)
            StrCpy(kBuf, 0x004C64FC);                                         // 0x0041701a, 0x00417048..0x00417066
            break;
        case 4:                                                               // 0x00416fa0 (kind -3)
            if (CC(0x0046C940, (int)G16(ebx + 0xb)) != 0)                     // typeBit7Clear 0x00416fa5, 0x00416faf
                StrCpy(kBuf, 0x004C6564);                                     // 0x00416fb1, 0x00417048..0x00417066
            else
                StrCpy(kBuf, 0x004C6548);                                     // 0x00416fbb..0x00416fdb, shared tail 0x00417063
            break;
        case 5:                                                               // 0x00416f60 (kind -2)
            if (CC(0x0046C940, (int)G16(ebx + 0xb)) != 0)                     // typeBit7Clear 0x00416f65, 0x00416f6f
                StrCpy(kBuf, 0x004C6594);                                     // 0x00416f71, 0x00417048..0x00417066
            else
                StrCpy(kBuf, 0x004C657C);                                     // 0x00416f7b..0x00416f9b, shared tail 0x00417063
            break;
        case 6: {                                                             // 0x00416ecb (kind 35)
            int q = (G32(0x00834170) + ebp) / 100;                            // 0x00416ecb..0x00416ee3 (imul 0x51eb851f, sar 5, sign add)
            int sub = q % 4;                                                  // 0x00416ee5..0x00416ef1 (and 0x80000003, jns, dec, or -4, inc)
            if ((u32)sub <= 3) {                                              // 0x00416ef2 / 0x00416ef5 ja 0x00417068
                switch (sub) {                                                // 0x00416efb jmp [0x0042176c + 4 sub]
                case 0: StrCpy(kBuf, 0x004C65F0); break;                      // 0x00416f02, 0x00417048..0x00417066
                case 1: StrCpy(kBuf, 0x004C65D0); break;                      // 0x00416f0c..0x00416f2c, shared tail 0x00417063
                case 2: StrCpy(kBuf, 0x004C65BC); break;                      // 0x00416f31, 0x00417048..0x00417066
                case 3: StrCpy(kBuf, 0x004C65A8); break;                      // 0x00416f3b..0x00416f5b, shared tail 0x00417063
                }
            }
            break;
        }
        default:                                                              // index byte 7 -> 0x00417068 (kinds -1..34)
            break;
        }
    }
    // 0x00417068
    {
        u32 tgt = (G32(0x004C2844) > 4) ? 0x0051B360u : 0x00519FD8u;          // 0x00417068..0x00417077
        TC(0x004762D0, Root(), (int)tgt, 0, 0, 0);                            // setDrawTarget 0x00417089
    }
    TC(0x00474030, 0x005787D0, Root(), FD(0xa0) - 0x20, FD(0x80) - G32(0x004C2844) * 10, G32(0x00822C74), 1, 1, 1, 0);   // Surface_drawScaled2 0x004170c9
    TC(0x00474030, 0x005787D0, Root(), FD(0xa0) - 0x40, FD(0x80) - G32(0x004C2844) * 10, G32(0x00822C74), 1, 1, 1, 0);   // Surface_drawScaled2 0x00417107
    TC(0x00474030, 0x005787D0, Root(), FD(0xa0), FD(0x80) - G32(0x004C2844) * 10, G32(0x00822C74), 1, 1, 1, 0);          // Surface_drawScaled2 0x00417143
    TC(0x00474030, 0x005787D0, Root(), FD(0xa0) - 0x60, FD(0x80) - G32(0x004C2844) * 10, G32(0x00822C74), 1, 1, 1, 0);   // Surface_drawScaled2 0x00417181
    TC(0x00474030, 0x005787D0, Root(), FD(0xa0) + 0x20, FD(0x80) - G32(0x004C2844) * 10, G32(0x00822C74), 1, 1, 1, 0);   // Surface_drawScaled2 0x004171bf
    TC(0x00474030, 0x005787D0, Root(), FD(0xa0) + 0x40, FD(0x80) - G32(0x004C2844) * 10, G32(0x00822C74), 1, 1, 1, 0);   // Surface_drawScaled2 0x004171fd
    u32 g = FU(0x14) << 8;                                                    // 0x00417202 / 0x00417206
    u8 a = GU8(g + 0x0057953C);                                               // 0x00417209
    u8 c = GU8(ebx - 3);                                                      // 0x0041720f
    int color = (int)(((a < 5) ? 0xFFFFE319u : 0u) + 0x80007FFFu);            // 0x00417212..0x0041721b (cmp, sbb, and, add)
    if ((c & 0x10) != 0) color = (int)0x80007D08;                             // 0x00417220 / 0x00417223 / 0x00417225
    CC(0x00404B70, (int)kBuf, FD(0xa0), FD(0x80) - G32(0x004C2844) * 10, color);   // drawCenteredText 0x0041724c
    FD(0x80) = FD(0x80) + 8;                                                  // 0x00417251 / 0x0041725b / 0x0041725e
}

static void P4_walkers(u8* F) {
    int ebp = 0;                                                              // 0x00416e48
    u32 ebx = 0x00585865;                                                     // 0x00416e4a
    for (;;) {
        P4_walkerOne(F, ebx, ebp);                                            // 0x00416e4f..0x0041725e
        ebx += 0x4c;                                                          // 0x00417265
        ebp += 0x23;                                                          // 0x00417268
        if ((int)ebx >= 0x00586B65) break;                                    // 0x0041726b / 0x00417271 jl 0x00416e4f
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00417277..0x004175f2: the hole HUD. For hole h = [0x005685f0] below 0x13 whose record (0x00575ab8 + 0x208 h)
// has a non-zero dword +0 and point B (+0x10 / +0x14, B.x non-zero) on screen, the distance word at record - 4
// (0x00575ab4 + 0x208 h) is computed once (while 0) as __ftol(sqrt(625 * (dx * dx + dy * dy))) between point A
// (+0 / +4) and point B, plus a quarter of its excess over 0xc8; the par p (2..6) comes from that word (+0x19 when
// byte +0x1f8 has bits 0x60, minus 25 * [0x005a8c60] above 0x12c); three lines (0x004c4978 + h, the word +
// 0x004c501c, 0x004c64d0 + p) are drawn at B's screen point minus [F+0x40]; the string 0x004c64b8 goes at A's
// screen point when the date has bit 2 or 3 set.
static int P4_sqrtFtol(u8* F) {
    int p = FA(0x34);
    int r;
    __asm {
        mov eax, p
        fild dword ptr [eax]
        fsqrt
        mov ecx, 0x004A6030
        call ecx
        mov r, eax
    }
    return r;                                                                 // 0x0041732f fild, 0x00417333 fsqrt, 0x00417335 __ftol
}

static void P4_holeHud(u8* F) {
    int ecx = G32(0x005685F0);                                                // 0x00417277
    if (ecx >= 0x13) return;                                                  // 0x0041727d / 0x00417280 jge 0x004175f2
    u32 rec = (u32)ecx * 0x208u;                                              // 0x00417286..0x0041728d
    if (G32(rec + 0x00575AB8) == 0) return;                                   // 0x00417290 / 0x00417298
    int c = G32(rec + 0x00575AC8);                                            // 0x0041729e
    if (c == 0) return;                                                       // 0x004172a4 / 0x004172a6
    int e = G32(rec + 0x00575ACC);                                            // 0x004172ac
    if (CC(0x0042F940, c, e, FA(0xd8), FA(0xd0)) == 0) return;                // tileToScreen 0x004172c4, 0x004172ce
    u32 esi = (u32)G32(0x005685F0) * 0x208u;                                  // 0x004172d4..0x004172e0
    if (G16(esi + 0x00575AB4) == 0) {                                         // 0x004172e3 / 0x004172eb jne 0x00417365
        int eax = G32(esi + 0x00575AB8);                                      // 0x004172ed
        int ebx = G32(esi + 0x00575AC8);                                      // 0x004172f3
        int ebp = G32(esi + 0x00575ACC);                                      // 0x004172f9
        int dx = eax - ebx;                                                   // 0x004172ff
        if (dx < 0) dx = (int)(0u - (u32)dx);                                 // 0x00417301..0x0041730e (cdq, xor, sub)
        int dy = G32(esi + 0x00575ABC) - ebp;                                 // 0x00417304 / 0x0041730c
        if (dy < 0) dy = (int)(0u - (u32)dy);                                 // 0x00417310..0x00417313
        u32 s = (u32)dy * (u32)dy + (u32)dx * (u32)dx;                        // 0x00417317..0x0041731d
        s *= 625u;                                                            // 0x0041731f..0x00417328 (four lea x5)
        FD(0x34) = (int)s;                                                    // 0x0041732b
        int r = P4_sqrtFtol(F);                                               // 0x0041732f..0x00417335
        u16 cx = (u16)r;                                                      // 0x0041733a
        G16(esi + 0x00575AB4) = (short)cx;                                    // 0x00417342
        if ((short)cx > 0xc8) {                                               // 0x0041733d / 0x00417349 jle 0x00417365
            int t = ((int)(short)cx - 0xc8) / 4;                              // 0x0041734b..0x00417359 (cdq, and 3, add, sar 2)
            // 0x0041735c add eax, ecx: only the low word is stored, so the upper half of ecx (|dx| >> 16) drops out
            G16(esi + 0x00575AB4) = (short)(u16)((u32)t + (u32)cx);           // 0x0041735e
        }
    }
    // 0x00417365
    u8 al = GU8(esi + 0x00575CB0);                                            // 0x00417365
    int v = G16(esi + 0x00575AB4);                                            // 0x0041736b movsx
    if ((al & 0x60) != 0) v += 0x19;                                          // 0x00417372 / 0x00417374 / 0x00417376
    if (v > 0x12c) {                                                          // 0x00417379 / 0x0041737f jle
        int d = G32(0x005A8C60);                                              // 0x00417381
        int m5 = (int)((0u - (u32)d) * 4u - (u32)d);                          // 0x00417387..0x0041738e (neg, shl 2, sub)
        v = (int)((u32)v + (u32)m5);                                          // 0x00417390
        v = (int)((u32)v + (u32)m5 * 4u);                                     // 0x00417392 lea ecx, [ecx + eax*4]
    }
    int ebp = 3;                                                              // 0x00417398
    if (v <= 0x32) ebp = 2;                                                   // 0x00417395 / 0x0041739d jg / 0x0041739f
    if (v >= 0xfa) ebp = 4;                                                   // 0x004173a4 / 0x004173aa jl / 0x004173ac
    if (v >= 0x1db) ebp = 5;                                                  // 0x004173b1 / 0x004173b7 jl / 0x004173b9
    if (v > 0x271) ebp = 6;                                                   // 0x004173be / 0x004173c4 jle / 0x004173c6
    TC(0x004762D0, Root(), 0x0051B360, 0, 0, 0);                              // setDrawTarget 0x004173dc
    StrCpy(kBuf, 0x004C4978);                                                 // 0x004173e1..0x00417411
    int r = Itoa(G32(0x005685F0), 0x0058A528, 10);                            // __itoa 0x00417413 (hole read 0x004173f3)
    int ebx = FD(0x40);                                                       // 0x0041741f
    StrCat(kBuf, (u32)r);                                                     // 0x00417418..0x00417452
    CC(0x00404BC0, (int)kBuf, FD(0xd8), FD(0xd0) - ebx - 0x1a, (int)0x80007FFF);   // drawCenteredText2 0x00417465
    int h = G32(0x005685F0);                                                  // 0x0041746a
    GU8(0x0051A068) = 0;                                                      // 0x0041747d
    r = Itoa((int)G16((u32)h * 0x208u + 0x00575AB4), 0x0058A528, 10);         // 0x00417471..0x00417484, __itoa 0x0041748d
    StrCat(kBuf, (u32)r);                                                     // 0x00417492..0x004174ba
    StrCat(kBuf, 0x004C501C);                                                 // 0x004174bc..0x004174f3
    CC(0x00404BC0, (int)kBuf, FD(0xd8), FD(0xd0) - ebx - 0xd, (int)0x80007FFF);    // drawCenteredText2 0x00417506
    StrCpy(kBuf, 0x004C64D0);                                                 // 0x0041750b..0x00417536
    r = Itoa(ebp, 0x0058A528, 10);                                            // __itoa 0x00417538
    StrCat(kBuf, (u32)r);                                                     // 0x0041753d..0x00417576
    CC(0x00404BC0, (int)kBuf, FD(0xd8), FD(0xd0) - ebx, (int)0x80007FFF);          // drawCenteredText2 0x00417586
    u32 rec2 = (u32)G32(0x005685F0) * 0x208u;                                 // 0x0041758b..0x004175a7
    if (CC(0x0042F940, G32(rec2 + 0x00575AB8), G32(rec2 + 0x00575ABC), FA(0xd8), FA(0xd0)) == 0) return;   // tileToScreen 0x004175b9, 0x004175c3
    if ((GU8(0x00834170) & 0xc) == 0) return;                                 // 0x004175c5 / 0x004175cc
    CC(0x00404BC0, 0x004C64B8, FD(0xd8), FD(0xd0) - ebx, (int)0x80007FFF);         // drawCenteredText2 0x004175ea
}

// ---------------------------------------------------------------------------------------------------------------
// entry: 0x00415847..0x004175f2, the phases in the original's order; falls through to 0x004175f2.
void Ph_00415847(u8* F) {
    P4_records5736b8(F);        // 0x00415847..0x0041596e
    P4_flyingBall(F);           // 0x0041596e..0x004159f7
    P4_selectedSprite(F);       // 0x004159f7..0x00415a44
    P4_markerCircle(F);         // 0x00415a44..0x00415a77
    P4_animalsAndImage(F);      // 0x00415a77..0x00415b6d
    P4_queueAndSelection(F);    // 0x00415b6d..0x00415c3a
    P4_objectLabels(F);         // 0x00415c3a..0x00415ec2
    P4_holeGrid(F);             // 0x00415ec2..0x00416305
    P4_ballLabel(F);            // 0x00416305..0x00416838
    P4_golfers(F);              // 0x00416838..0x00416e48
    P4_walkers(F);              // 0x00416e48..0x00417277
    P4_holeHud(F);              // 0x00417277..0x004175f2
}

}  // namespace
// c3fy part P5: mainLoop 0x0040f5c0, range 0x004175f2..0x00418bc7 (1,707 instructions, 86 call sites), hand-written
// from log/c3/alt/c3fy_norm.asm. Every [F+X] access of the original stays a frame access at the same offset and width.
// Entry: no register is live at 0x004175f2. Exit: falls through to 0x00418bc7 with no register live.
// Phases: 0x004175f2..0x004185b3 the simulation tick (P5_sim), 0x004185bb..0x00418b05 the SimFoto frame and its
// snapshot (P5_simfoto), 0x00418b05..0x00418bc7 the zoomed copy and the marker pass (P5_zoom).
namespace {

// ====================================================================================================================
// 0x00417662..0x00417818: the random tile event. Picks a random tile (row Rnd(50), column Rnd(50)) whose flag word has
// neither 0x400 nor 0x800, looks at its four neighbours (table offsets 0, 8, 0x10, 0x18 of 0x004c2878 / 0x004c2898),
// and when every free neighbour pattern passes, marks the tile (flag bits 0x4800, byte 0x00578804 + tile = 1) and plays
// sound 0x22 there.
// ====================================================================================================================
static void P5_tileEvent(u8* F) {
    const int edi = Rnd(0x32) & 0xffff;                    // 0x00417669, mov di, ax / and edi, 0xffff (0x00417678)
    const int ebx = Rnd(0x32) & 0xffff;                    // 0x0041767e, mov bx, ax / and ebx, 0xffff (0x00417689)
    const int idx = ebx + edi * 50;                        // 0x00417686..0x00417692
    FD(0x28) = idx;                                        // 0x00417695
    if ((GU16(0x0053CAF0 + idx * 2) & 0xc00) != 0) return; // 0x00417699..0x004176a9 -> 0x00417818
    FD(0x10) = 0;                                          // 0x004176b1
    FD(0xc) = 0;                                           // 0x004176b5
    FD(0x18) = 0;                                          // 0x004176b9
    int off;
    do {
        off = FD(0x18);                                    // 0x004176bd
        const int ebp = G32(0x004C2898 + off) + ebx;       // 0x004176c1 / 0x004176cd
        const int esi = G32(0x004C2878 + off) + edi;       // 0x004176c7 / 0x004176cf
        if (CC(0x0040BF60, esi, ebp) == 0) {               // 0x004176d3 tileBlocked(esi, ebp); 0x004176dd
            const int n = ebp + esi * 50;                  // 0x004176df..0x004176e5
            if ((GU16(0x0053CAF0 + n * 2) & 0x800) != 0) { // 0x004176e9..0x004176fa
                const int edx = FD(0x10);                  // 0x004176fc
                const int ecx = FD(0xc);                   // 0x00417700
                FD(0x10) = edx | 1;                        // 0x00417704 / 0x00417708
                FD(0xc) = ecx + 1;                         // 0x00417707 / 0x0041770c
            }
            const int t = G8(0x005722E8 + n);              // 0x00417710 movsx
            if (G8(0x00578372 + t * 48) <= 0)              // 0x00417717..0x00417725 (signed byte, jg skips)
                FD(0x10) = FD(0x10) | 2;                   // 0x00417727..0x0041772d
        }
        off = FD(0x18) + 8;                                // 0x00417731 / 0x00417735
        FD(0x18) = off;                                    // 0x0041773b
    } while (off < 0x20);                                  // 0x00417738 / 0x0041773f
    if (FD(0x10) != 3 || FD(0xc) > 3) FD(0x10) = 0;        // 0x00417745..0x00417758 (eax = 3 at 0x00417749)
    if ((GU16(0x0053CAF0 + FD(0x28) * 2) & 0x1000) != 0) { // 0x00417760..0x00417775
        const int r = Rnd(3) & 0xffff;                     // 0x0041777d (push eax, eax = 3 from 0x00417749); 0x00417786
        FD(0x10) = (r <= FD(0xc)) ? 1 : 0;                 // 0x00417782..0x00417792 setle (signed)
    }
    if (FD(0x10) == 0) return;                             // 0x00417796..0x0041779c -> 0x00417818
    const int g = FD(0x14) << 8;                           // 0x0041779e / 0x004177a4
    if (CC(0x00407000, G32(0x0057958C + g), G32(0x00579590 + g), 2) != 0) return;   // 0x004177b5 holeQuadrants; 0x004177bf
    const int t2 = FD(0x28);                               // 0x004177c1
    const s8 cl = G8(0x005722E8 + t2);                     // 0x004177c5
    if (cl != 2) {                                         // 0x004177c8 / 0x004177ce
        const u8 a = GU8(0x00578376 + cl * 48);            // 0x004177d0..0x004177d9 (movsx index)
        if (a != 4 && a != 0xd) return;                    // 0x004177df..0x004177e7 -> 0x00417818
    }
    GU8(0x0053CAF1 + t2 * 2) |= 0x48;                      // 0x004177e9
    GU8(0x00578804 + t2) = 1;                              // 0x00417809
    CC(0x0040C500, 0x22, (edi << 10) + 0x200, (ebx << 10) + 0x200, 0);   // 0x00417810 playSoundAt
}

// ====================================================================================================================
// 0x00417818..0x00417c3e: every 0x400 / ([0x00822c88] + 2) date ticks. Charges every open hole record one unit
// (dword +0x1f8 of each 0x208 record at 0x00575ab0 counts up, cash 0x00571fd4 and the year word 0x0058421c + 20 *
// (year % 100) count down), lets walkers with a negative mode pay their table price (pointsPopup at the nearest hole),
// records this month's skill / fun / cash / member-count words in the four 500-entry word logs, and visits one slice of
// the 256 placed objects (stride [0x00822c88] + 2) re-rating every lot (object type 5), where a new home buyer is drawn.
// Returns edi as the original holds it at 0x00417c44 (the date [0x00834170]).
// ====================================================================================================================
static int P5_dateTick(u8* F) {
    const int edx = G32(0x00822C88);                       // 0x00417818
    int edi = G32(0x00834170);                             // 0x00417823
    const int ebp = edx + 2;                               // 0x00417829
    const int ecx = 0x400 / ebp;                           // 0x0041781e..0x0041782d
    FD(0x2c) = ebp;                                        // 0x0041782f
    FD(0x18) = ecx;                                        // 0x0041783a
    if (edi % ecx != 0) return edi;                        // 0x00417835..0x00417840 -> 0x00417c44

    // 0x00417846..0x00417890: the 19 hole records 0x00575ab0 + 0x208 k (k = 0..18), ecx = record + 0x1f8
    const short si = G16(0x005A6D3C);                      // 0x00417846
    for (u32 p = 0x00575CA8; p < 0x00578340; p += 0x208) { // 0x0041784d, 0x00417884..0x00417890
        if (GU8(p - 0x1f8) != 0) {                         // 0x00417852..0x0041785a
            const int bx = G32(0x00571FD4) - 1;            // 0x0041785c / 0x00417867
            G32(p) = G32(p) + 1;                           // 0x00417862 / 0x00417868 / 0x00417869
            const int y = (int)si % 100;                   // 0x00417864..0x00417871
            G32(0x00571FD4) = bx;                          // 0x00417873
            G16(0x0058421C + y * 20) -= 1;                 // 0x00417879 / 0x0041787c
        }
    }

    // 0x00417892..0x0041796d: the 64 walker records of 0x4c bytes at 0x00585850 (esi = record + 0x13)
    for (u32 esi = 0x00585863; esi < 0x00586B63; esi += 0x4c) {   // 0x00417892, 0x00417964..0x0041796d
        if (GU8(esi - 1) == 0) continue;                   // 0x0041789c..0x004178a1
        const s8 m = G8(esi);                              // 0x004178a7
        if (m >= 0) continue;                              // 0x004178a9 / 0x004178ab (signed)
        if (m == (s8)0xfa) continue;                       // 0x004178b1 / 0x004178b3
        const int n = 4 - G32(0x00822C88);                 // 0x004178b9..0x004178c3
        const int b = CC(0x0044FAF0, G32(0x005685F0) - 1); // 0x004178c5..0x004178cc bucketValue
        FD(0x34) = b;                                      // 0x004178db
        const int r = Rnd(n) & 0xffff;                     // 0x004178e0 / 0x004178e9
        if (r > FD(0x34)) continue;                        // 0x004178e5..0x004178f0 (signed)
        if ((G32(0x0059E7B8) & 0x4000000) != 0) continue;  // 0x004178f2 / 0x004178f8 (ebx = 0x4000000 from 0x00417897)
        const int k = (GU8(esi - 1) & 8) ? 3 : 4;          // 0x004178fa..0x0041790e: and dl, 8 / neg / sbb / add 4
        const int j = -(int)G8(esi) * 2 - k;               // 0x00417902, 0x0041790c, 0x00417911, 0x00417913
        const int pts = G8(0x004C2E2C + j);                // 0x00417915 movsx
        G32(0x00571FD4) -= pts;                            // 0x0041791c
        const int y = (int)G16(0x005A6D3C) % 100;          // 0x00417922..0x0041792a (ecx = 0x64 at 0x004178fd)
        const int wy = G32(esi - 0xf) << 10;               // 0x0041792f / 0x00417932
        G16(0x0058421A + y * 20) -= (short)pts;            // 0x0041792c / 0x00417935
        const int wx = G32(esi - 0x13) << 10;              // 0x0041793d / 0x00417940
        const int h = CC(0x00407340, wx, wy);              // 0x00417945 nearestHole
        CC(0x0040C890, -pts, G32(esi - 0x13), G32(esi - 0xf), h);   // 0x00417958 pointsPopup
        G16(esi + 0x11) += (short)pts;                     // 0x00417960
    }

    // 0x00417973..0x00417a02: this month's slot in the word logs
    edi = G32(0x00834170);                                 // 0x00417973
    const int slot = (edi / 1024) % 500;                   // 0x0041797e..0x0041798d
    const short sk = G16(0x00541CD8);                      // 0x0041798f
    const short cash = G16(0x00571FD4);                    // 0x00417995 (low word of the cash dword)
    G16(0x005409AC + slot * 2) = sk;                       // 0x004179a0
    G16(0x0053E63C + slot * 2) = G16(0x0059AE78);          // 0x004179a7 / 0x004179ad
    G16(0x0051B388 + slot * 2) = cash;                     // 0x004179b4
    int cnt = 0;                                           // 0x0041799e
    G32(0x0058B388) = cnt;                                 // 0x004179bb
    for (u32 p = 0x005849E2; p < 0x005856F2; p += 0x2c) {  // 0x004179c1, 0x004179d7..0x004179df (76 type records)
        if (GU8(p + 0x27) != 0xff && (GU8(p) & 7) > 1) cnt++;   // 0x004179c6..0x004179d6
    }
    G16(0x00520640 + slot * 2) = (short)cnt;               // 0x004179e3
    const int q = edi / FD(0x18);                          // 0x004179e1..0x004179eb
    G32(0x0058B388) = cnt;                                 // 0x004179ef
    const int rem = q % ebp;                               // 0x004179f5 / 0x004179f6
    FD(0x18) = rem;                                        // 0x004179fe
    if (rem >= 0x100) return edi;                          // 0x004179f8 / 0x00417a02 -> 0x00417c44

    // 0x00417a08..0x00417c38: placed objects rem, rem + stride, ... (records of 16 bytes at 0x0058bcb8, ebx = +0xc)
    const int stride = FD(0x2c) << 4;                      // 0x00417a08 / 0x00417a0e
    u32 ebx = (u32)(rem << 4) + 0x0058BCC4;                // 0x00417a0c / 0x00417a11 / 0x00417a18
    FD(0xc) = stride;                                      // 0x00417a14
    int v;
    do {
        if (G16(ebx - 0xc) == 5) {                         // 0x00417a1e / 0x00417a23
            const int y2 = G16(ebx - 8);                   // 0x00417a29 movsx
            const int x2 = G16(ebx - 0xa);                 // 0x00417a2d movsx
            int esi = G32(ebx);                            // 0x00417a31
            const int rate = CC(0x0042EF40, x2, y2);       // 0x00417a35 rateLot
            const u8 fl = GU8(0x0056C7B4);                 // 0x00417a4f
            esi -= esi / 12;                               // 0x00417a3c..0x00417a54 (0x2aaaaaab, sar 1)
            G32(ebx) = rate + esi;                         // 0x00417a56 / 0x00417a5a
            if (fl & 0x80) {                               // 0x00417a58 / 0x00417a5c
                const u32 w = GU32(0x0056C7B4);            // 0x00417a5e
                G32(ebx) = 0x9c4;                          // 0x00417a63
                GU32(0x0056C7B4) = w & 0xffffff7f;         // 0x00417a69 and al, 0x7f / 0x00417a6b
            }
            int who;
            do {
                who = Rnd(0x64) & 0xffff;                  // 0x00417a77, mov bp, ax / and ebp, 0xffff
            } while (GU8(0x0055D758 + who * 37) == 0xff);  // 0x00417a85..0x00417a91
            if (G32(ebx - 4) == 0) {                       // 0x00417a93..0x00417a98
                const int nb = G32(0x004C284C);            // 0x00417a9e
                const int lim = (nb * 300 + 0x190) * (G32(0x00822C88) + 2);   // 0x00417aa3..0x00417abc
                if (G32(ebx) > lim) {                      // 0x00417abf / 0x00417ac1 (signed)
                    const int rec = who * 37;              // 0x00417ac7 / 0x00417ad3
                    FD(0x34) = rec;                        // 0x00417ad7
                    const int kind = G8(0x0055D758 + rec); // 0x00417adb movsx
                    StrCpy(kBuf, 0x004C64A8);              // 0x00417ae4..0x00417aff
                    StrCat(kBuf, GU32(0x004C2BA8 + kind * 4));    // 0x00417b01..0x00417b2c
                    StrCat(kBuf, 0x004C3F70);              // 0x00417b2e..0x00417b5b
                    StrCat(kBuf, (u32)(FD(0x34) + 0x0055D738));   // 0x00417b54..0x00417b89
                    StrCat(kBuf, 0x004C6444);              // 0x00417b8b..0x00417bb7
                    if (CC(0x0040CB00, (int)0x80004010, 0, -3) != 0) {   // 0x00417bbe startMessage (eax = 0 pushed at 0x00417bb6); 0x00417bc8
                        G32(ebx - 4) = who + 1;            // 0x00417bca / 0x00417bd3
                        CC(0x0040C6F0, 0xc0, who);         // 0x00417bd6 logTick
                        CC(0x004481B0, 0x33, 0x64, 0, 0, 0);   // 0x00417be5 playSound
                        G32(0x004C284C) = G32(0x004C284C) + 1;  // 0x00417bea / 0x00417bf3 / 0x00417bfb
                        const int r6 = Rnd(6) & 0xffff;    // 0x00417c01 / 0x00417c0f
                        CC(0x004011E0, r6, who, (int)G16(ebx - 0xa), (int)G16(ebx - 8));   // 0x00417c17 spawnObject
                    }
                }
            }
        }
        v = FD(0x18) + FD(0x2c);                           // 0x00417c1f..0x00417c2b
        ebx += FD(0xc);                                    // 0x00417c27 / 0x00417c2d
        FD(0x18) = v;                                      // 0x00417c34
    } while (v < 0x100);                                   // 0x00417c2f / 0x00417c38
    return G32(0x00834170);                                // 0x00417c3e
}

// ====================================================================================================================
// 0x00417c44..0x004182ee: the 1024-tick (monthly) work. Autosave 1 at date % 1024 == 0x200; at date % 1024 == 0:
// autosave 2, the word logs again, the path warning for unconnected placed objects, the 1/8 decay of the hole
// statistics, interest on a negative cash balance, the empty-course hint, the SGA tournament offer (date % 8192 ==
// 0x1000) and the year end (date % 8192 == 0: the 1/8 decay of three 50x50 byte grids, a random column of the 9x6
// dword table, showEndOfYearScreen). edi = the date as held at 0x00417c44.
// ====================================================================================================================
static void P5_tournament(u8* F) {
    // 0x00417fc5..0x00418226: the Image at F+0x36c lives between the two state-byte stores
    TC(0x00474AE0, FA(0x36c));                             // 0x00417fcc Image_ctor
    FUB(0x124c) = 3;                                       // 0x00417fea
    TC(0x00474DD0, FA(0x36c), 0x320, 0x258, 0, 1, 0, 0);   // 0x00417ff2 Surface_createFull
    TC(0x00475C60, Root(), FA(0x36c), 0, 0, 0, 0, 0x320, 0x258);   // 0x00418017 Surface_blitThunk (this = [0x004c1570])
    CC(0x0044FB30, 2);                                     // 0x0041801e showCourseReport(2)
    if (G32(0x00567B04) != 0) {                            // 0x00418023..0x0041802d
        TC(0x00475C60, FA(0x36c), Root(), 0, 0, 0, 0, 0x320, 0x258);   // 0x00418033..0x00418053 Surface_blitThunk
        StrCpy(kBuf, 0x004C62CC);                          // 0x00418058..0x00418087
        StrCat(kBuf, (u32)(0x00571FD8 + G32(0x0059BF90) * 46));   // 0x0041807f, 0x00418089..0x004180be
        StrCat(kBuf, 0x004C627C);                          // 0x004180c0..0x004180ef
        StrCat(kBuf, (u32)Itoa(G32(0x00567B04), 0x0058A528, 10));   // 0x004180e4, 0x004180f1 __itoa (pushes 0x00418062 / 0x0041806a); 0x004180f6..0x00418122
        StrCat(kBuf, 0x004C6250);                          // 0x00418124..0x00418157
        GU8(0x00569498) = 0;                               // 0x0041814f (al = 0 from 0x004180fb), inside the copy above
        StrCat(kBuf, 0x004D6098);                          // 0x00418159..0x00418182
        StrCat(kBuf, 0x004C6230);                          // 0x00418184..0x004181ad
        CC(0x0040CB00, (int)0x80001204, 1, -0x14);         // 0x004181af startMessage (pushes 0x0041811b / 0x00418120 / 0x00418132)
        CC(0x004481B0, 0x2f, 0x64, 0, 0, 0);               // 0x004181be playSound; 0x004181c8 pops both calls and __itoa
        const u32 fl = GU32(0x0059E7B8) | 0x400000;        // 0x004181c3 / 0x004181cb
        G32(0x00567B04) = 0;                               // 0x004181d0
        GU32(0x0059E7B8) = fl;                             // 0x004181da
    } else {                                               // 0x004181df jmp skips this arm
        const u32 fl = GU32(0x0059E7B8);                   // 0x004181e1
        const int root = Root();                           // 0x004181e7
        GU32(0x0059E7B8) = fl & 0xffbfffff;                // 0x004181fc / 0x0041820c
        TC(0x00475C60, FA(0x36c), root, 0, 0, 0, 0, 0x320, 0x258);   // 0x00418212 Surface_blitThunk
    }
    FUB(0x124c) = 0;                                       // 0x0041821e
    TC(0x00474C40, FA(0x36c));                             // 0x00418226 Image_dtor
}

static void P5_monthly(u8* F, int edi) {
    if (edi % 1024 == 0x200) CC(0x0040B4A0, 0x004C6438);  // 0x00417c44..0x00417c5a; 0x00417c61 saveGame
    if (G32(0x00834170) % 1024 != 0) return;              // 0x00417c69..0x00417c7c -> 0x004182f3
    CC(0x0040B4A0, 0x004C642C);                            // 0x00417c87 saveGame

    // 0x00417c8c..0x00417cd5: the word logs again (store order of the original)
    const int slot = (G32(0x00834170) / 1024) % 500;      // 0x00417c8c..0x00417cab
    const short sk = G16(0x00541CD8);                      // 0x00417cad
    const short cash = G16(0x00571FD4);                    // 0x00417cb3
    G16(0x005409AC + slot * 2) = sk;                       // 0x00417cc1
    const short fun = G16(0x0059AE78);                     // 0x00417cc8
    G16(0x0051B388 + slot * 2) = cash;                     // 0x00417cce
    G16(0x0053E63C + slot * 2) = fun;                      // 0x00417cd5

    // 0x00417cdc..0x00417dea: placed objects 1..255 of type 6..0xe without flag 0x40 in byte +7 (ebp = 0x4000000 from 0x00417cba)
    for (u32 p = 0x0058BCC8; p < 0x0058CCB8; p += 0x10) {  // 0x00417ca2, 0x00417de1..0x00417dea
        const short t = G16(p);                            // 0x00417cdc
        if (t < 6) continue;                               // 0x00417cdf / 0x00417ce3 (signed)
        // 0x00417ce9 `cmp dx, 5; je` can never be taken once t >= 6
        if (GU8(p + 7) & 0x40) continue;                   // 0x00417cf3 / 0x00417cf7
        if (t >= 0xf) continue;                            // 0x00417cfd / 0x00417d01 (signed)
        if (G32(0x00822C88) == 0) continue;                // 0x00417d07..0x00417d0e
        if ((G32(0x0059E7B8) & 0x4000000) != 0) continue;  // 0x00417d14 / 0x00417d1a
        StrCpy(kBuf, 0x004C641C);                          // 0x00417d20..0x00417d4f
        StrCat(kBuf, (u32)(0x004C26B0 + (int)t * 20));     // 0x00417d49..0x00417d81
        StrCat(kBuf, 0x004C63DC);                          // 0x00417d83..0x00417dac
        StrCat(kBuf, 0x004C63C8);                          // 0x00417dae..0x00417dd7
        CC(0x0040CB00, (int)0x80006000, 1, -1);            // 0x00417dd9 startMessage (pushes 0x00417d2a / 0x00417d32 / 0x00417d40)
    }

    // 0x00417df0..0x00417ec6: hole records 1..18 (0x00575cb8 + 0x208 k, c = record + 0x24) lose 1/8 of each statistic
    for (u32 c = 0x00575CDC; c < 0x0057816C; c += 0x208) { // 0x00417df0, 0x00417eba..0x00417ec6
        int d = G32(c - 4);                                // 0x00417df5
        G32(c - 4) = d - d / 8;                            // 0x00417dfe..0x00417e10
        d = G32(c);                                        // 0x00417e13
        G32(c) = d - d / 8;                                // 0x00417e15..0x00417e22
        short s = G16(c + 0x134);                          // 0x00417e24
        G16(c + 0x134) = (short)(s - s / 8);               // 0x00417e2b..0x00417e39
        d = G32(c + 0x1c8);                                // 0x00417e40
        G32(c + 0x1c8) = d - d / 8;                        // 0x00417e46..0x00417e53
        s = G16(c + 0x136);                                // 0x00417e59
        G16(c + 0x136) = (short)(s - s / 8);               // 0x00417e60..0x00417e6e
        u32 w = c + 0xb4;                                  // 0x00417df8
        for (int k = 0x40; k != 0; k--) {                  // 0x00417e00, 0x00417e89 / 0x00417e8e
            s = G16(w);                                    // 0x00417e75
            w += 2;                                        // 0x00417e78
            G16(w - 2) = (short)(s - s / 8);               // 0x00417e7b..0x00417e8a
        }
        u32 w2 = c + 0x138;                                // 0x00417e92
        for (int k = 0; k < 8; k++, w2 += 2) {             // 0x00417e90, 0x00417eb1..0x00417eb8
            if (k == 5) continue;                          // 0x00417e98 / 0x00417e9b
            s = G16(w2);                                   // 0x00417e9d
            G16(w2) = (short)(s - s / 8);                  // 0x00417ea0..0x00417eae
        }
    }

    // 0x00417ecc..0x00417f03: a negative cash balance grows by 1/50 (also added to the year word)
    const int bal = G32(0x00571FD4);                       // 0x00417ecc
    if (bal < 0) {                                         // 0x00417ed2 / 0x00417ed4
        const int y = (int)G16(0x005A6D3C) % 100;          // 0x00417ed6..0x00417ee3
        const int q = bal / 50;                            // 0x00417ee5..0x00417ef7 (0x51eb851f, sar 4)
        G16(0x0058421C + y * 20) += (short)q;              // 0x00417eea / 0x00417ef9
        G32(0x00571FD4) = bal + q;                         // 0x00417f01 / 0x00417f03
    }

    // 0x00417f09..0x00417f87: the build hint while mode [0x00822c88] is 0 and hole 1's dwords +0x8 or +0x18 are 0
    if (G32(0x00822C88) == 0 && (G32(0x00575CC0) == 0 || G32(0x00575CD0) == 0)) {   // 0x00417f10, 0x00417f19, 0x00417f22
        StrCpy(kBuf, 0x004C6378);                          // 0x00417f24..0x00417f55
        G32(0x00567AFC) = 0;                               // 0x00417f4d (eax = 0 from 0x00417f2c), inside the copy above
        StrCat(kBuf, 0x004C6328);                          // 0x00417f57..0x00417f80
        CC(0x0040CB00, (int)0x80006000, 1, -1);            // 0x00417f82 startMessage (pushes 0x00417f2e / 0x00417f36 / 0x00417f41)
    }

    // 0x00417f8a..0x00418226: the SGA tournament offer
    if (G32(0x00834170) % 8192 == 0x1000 && G32(0x005A59F8) == -1 && (G32(0x0059E7B8) & 0x1200000) == 0)   // 0x00417f8a..0x00417fbf
        P5_tournament(F);

    // 0x0041822b..0x004182ee: the year end
    if (G32(0x00834170) % 8192 != 0) return;              // 0x0041822b..0x00418241 -> 0x004182f3
    for (u32 row = 0x005A6378; row < 0x005A6D3C; row += 0x32) {   // 0x00418247, 0x0041829b..0x004182a4
        const int off = (int)(row - 0x005A6378);           // 0x0041824c / 0x00418250
        for (int i = 0; i < 0x32; i++) {                   // 0x0041824e, 0x00418295..0x00418299
            u8 b = GU8(0x005A6378 + i + off);              // 0x00418256
            GU8(0x005A6378 + i + off) = (u8)(b - (b >> 3)); // 0x0041825d..0x00418264
            b = GU8(0x0056C7E4 + i + off);                 // 0x0041826b
            GU8(0x0056C7E4 + i + off) = (u8)(b - (b >> 3)); // 0x00418272..0x00418279
            b = GU8(0x0053EA24 + i + off);                 // 0x00418280
            GU8(0x0053EA24 + i + off) = (u8)(b - (b >> 3)); // 0x00418287..0x0041828e
        }
    }
    int e = 0;                                             // 0x004182a6
    u32 s = 0x0054230C;                                    // 0x004182a8
    do {
        const int r = Rnd(4) & 0xffff;                     // 0x004182b4 / 0x004182b9
        s += 0x18;                                         // 0x004182be
        const int i = r + e;                               // 0x004182c1
        e += 6;                                            // 0x004182c3
        G32(0x005422F8 + i * 4) = G32(0x005422F8 + i * 4) + 1;   // 0x004182c6..0x004182d5
        G32(s - 0x18) = G32(s - 0x18) + 1;                 // 0x004182d7 / 0x004182da / 0x004182e1
    } while (s < 0x005423E4);                              // 0x004182db / 0x004182e4
    if ((G32(0x0059E7B8) & 0x4000000) == 0)                // 0x004182e6 / 0x004182ec (ebp = 0x4000000 from 0x00417cba)
        CC(0x0044CFF0);                                    // 0x004182ee showEndOfYearScreen (no argument pushed)
}

// ====================================================================================================================
// 0x004182f3..0x004185b8: one frame in 100 plays one of seven ambient sounds (switch table 0x0042177c) at a random
// volume / pan / pitch.
// ====================================================================================================================
static void P5_ambient() {
    if ((Rnd(0x64) & 0xffff) != 0) return;                 // 0x004182fa, 0x004182ff test ax, ax / 0x00418302 -> 0x004185bb
    const u32 r = (u32)(Rnd(6) & 0xffff);                  // 0x0041830f / 0x00418314
    if (r > 6) return;                                     // 0x00418319 / 0x0041831c ja -> 0x004185bb
    int id = 0;
    switch (r) {                                           // 0x00418322 jmp [eax*4 + 0x0042177c]
        case 0: id = 0x1d; break;                          // 0x00418329 (0x00418380)
        case 1: id = 0x1e; break;                          // 0x00418387 (0x004183de)
        case 2: id = 0x1f; break;                          // 0x004183e5 (0x0041843c)
        case 3: id = 0x20; break;                          // 0x00418443 (0x0041849a)
        case 4: id = 0x21; break;                          // 0x004184a1 (0x004184f8)
        case 5: id = 0x28; break;                          // 0x004184ff (0x00418556)
        case 6: id = 0x22; break;                          // 0x0041855a (0x004185b1, falls into 0x004185b3)
    }
    // the seven case blocks are identical but for the sound id: Rnd(0x80), Rnd(0x28), push 0, Rnd(0x190), then the
    // shared call at 0x004185b3 (addresses below from case 0; the others are the same at +0x5e / +0x5b strides)
    const int a = Rnd(0x80) & 0xffff;                      // 0x00418333, mov si, ax / and esi, 0xffff (0x00418338 / 0x00418342)
    const int b = Rnd(0x28) & 0xffff;                      // 0x00418348, mov di, ax / and edi, 0xffff (0x0041834d / 0x0041835c)
    const int c = Rnd(0x190) & 0xffff;                     // 0x00418362 / 0x00418367 (push 0 at 0x00418350 before it)
    CC(0x004481B0, id, b + 0x3c, 0x40 - a, 0xc8 - c, 0);   // 0x004185b3 playSound (0 pushed before Rnd(0x190)); 0x004185b8
}

static void P5_sim(u8* F) {
    const u8 al = GU8(0x0059E7B8);                         // 0x004175f2
    G32(0x0059B738) = 0;                                   // 0x004175f7
    if (al & 4) return;                                    // 0x00417601 / 0x00417603 -> 0x004185bb
    CC(0x004289E0);                                        // 0x00417609 updateGolfers
    CC(0x00402A40);                                        // 0x0041760e updateWalkers
    CC(0x00409BF0);                                        // 0x00417613 updateRollingStats
    if (GU8(0x00569498) == 0 && G32(0x0053DF54) == 0)      // 0x00417618..0x00417628
        CC(0x0046E810);                                    // 0x0041762a showBulletinBoard
    int date = G32(0x00834170);                            // 0x0041762f
    const u8 cl = GU8(0x0059E7B8);                         // 0x00417634
    date++;                                                // 0x0041763a
    G32(0x00834170) = date;                                // 0x0041763e
    if (cl & 8) {                                          // 0x0041763b / 0x00417643
        date += date & 1;                                  // 0x00417645..0x0041764a
        G32(0x00834170) = date;                            // 0x0041764c
    }
    if ((date & 3) <= G32(0x00822C88))                     // 0x00417651..0x0041765c jg -> 0x00417818 (signed)
        P5_tileEvent(F);                                   // 0x00417662..0x00417815
    const int edi = P5_dateTick(F);                        // 0x00417818..0x00417c3e
    P5_monthly(F, edi);                                    // 0x00417c44..0x004182ee
    P5_ambient();                                          // 0x004182f3..0x004185b8
}

// ====================================================================================================================
// 0x004185bb..0x00418b02: the SimFoto frame. When [0x004c2e0c] names a golfer g (low byte) whose dword 0x005794c0 +
// 0x100 g is not -1, the box around g and its partner g ^ 1 (dwords +0 / +4 at 0x005794c0, widened to 0xa0 x 0x78
// with bit 0x100 of [0x004c2e0c], else 0xf0 x 0xc8, then clamped) gets a 16-pixel border (four fillWindowRect on
// 0x00519a60) and a 1-pixel frame (four Widget_fillRect on 0x00519cd4), the label at 0x004c6220 for a golfer with
// word +0xb2 == 4 and byte +0x85 == 0x32, the caption at 0x004c4e58, and the date month'day'year. Without bit 0x100 the
// frame is saved by saveCourseJpg as snapshots\<golfer name>-<date>.jpg (the strings 0x004c4e48, 0x004c52bc, 0x004c6218).
// ====================================================================================================================
static void P5_simfoto(u8* F) {
    const int edx = G32(0x004C2E0C);                       // 0x004185bb
    if (edx == -1) return;                                 // 0x004185c1 / 0x004185c4 -> 0x00418b05
    const int gi = edx & 0xff;                             // 0x004185ca / 0x004185cc
    FD(0x10) = gi;                                         // 0x004185d4
    if (G32(0x005794C0 + (gi << 8)) == -1) return;         // 0x004185d2..0x004185e2 -> 0x00418b05
    const int pa = (gi ^ 1) << 8;                          // 0x004185e8..0x004185ed
    const int ga = gi << 8;                                // 0x004185f0
    const int px = G32(0x005794C0 + pa);                   // 0x004185f3 (edi)
    FD(0x2c) = ga;                                         // 0x004185f9
    int esi = G32(0x005794C0 + ga);                        // 0x004185fd
    FD(0xc) = px;                                          // 0x00418603
    if (esi <= px) FD(0xc) = esi;                          // 0x00418607 / 0x00418609 jg / 0x0041860b
    if (esi < px) esi = px;                                // 0x0041860f / 0x00418611 jge / 0x00418613
    const int gy = G32(0x005794C4 + ga);                   // 0x00418615 (ecx)
    const int py = G32(0x005794C4 + pa);                   // 0x0041861b (eax)
    int ebx = (gy > py) ? py : gy;                         // 0x00418621..0x00418627
    int edi = (gy < py) ? py : gy;                         // 0x00418629..0x0041862f
    const int w = esi - FD(0xc);                           // 0x00418631..0x00418637
    const int h = edi - ebx;                               // 0x00418639 / 0x0041863b
    int lim;
    if (edx & 0x100) {                                     // 0x0041863d test dh, 1 / 0x00418640
        if (w < 0xa0) {                                    // 0x00418642 / 0x00418648
            const int d = (0xa0 - w) / 2;                  // 0x0041864a..0x00418658
            FD(0xc) = FD(0xc) - d;                         // 0x00418654 / 0x0041865a / 0x0041865e
            esi += d;                                      // 0x0041865c
        }
        lim = 0x78;                                        // 0x00418662 / 0x00418667
    } else {
        if (w < 0xf0) {                                    // 0x0041866e / 0x00418674
            const int d = (0xf0 - w) / 2;                  // 0x00418676..0x00418684
            FD(0xc) = FD(0xc) - d;                         // 0x00418680 / 0x00418686 / 0x0041868a
            esi += d;                                      // 0x00418688
        }
        lim = 0xc8;                                        // 0x0041868e / 0x00418696
    }
    if (h < lim) {                                         // 0x00418665 / 0x00418694 jge -> 0x004186a6
        const int d = (lim - h) / 2;                       // 0x0041869b..0x004186a0
        ebx -= d;                                          // 0x004186a2
        edi += d;                                          // 0x004186a4
    }
    const int x0 = CC(0x00467130, FD(0xc) - 0x10, 0x10, 0x2bc);   // 0x004186a6..0x004186b5 clamp -> ebp
    esi += 0x10;                                           // 0x004186bf
    const int x1 = CC(0x00467130, esi, 0x64, 0x310);       // 0x004186c7 clamp -> esi
    FD(0xc) = x1;                                          // 0x004186d9
    const int y0 = CC(0x00467130, ebx - 0x10, 0x10, 0x1f4);   // 0x004186cc..0x004186dd clamp -> ebx
    const int y1 = CC(0x00467130, edi + 0x10, 0x64, 0x248);   // 0x004186e2..0x004186ef clamp -> edi
    const int ww = x1 - x0;                                // 0x004186f6 / 0x004186fb
    FD(0x30) = ww;                                         // 0x004186fd
    const int wb = ww + 0x20;                              // 0x00418701
    FD(0x18) = y1;                                         // 0x00418719
    FD(0x34) = wb;                                         // 0x0041871d
    TC(0x00480B00, 0x00519A60, x0 - 0x10, y0 - 0x10, wb, 0x10, (int)0x80007fff);   // 0x00418721 fillWindowRect
    const int hh = y1 - y0;                                // 0x00418726 / 0x0041872d
    FD(0x20) = hh;                                         // 0x00418734
    const int hb = hh + 0x20;                              // 0x00418738
    FD(0x28) = hb;                                         // 0x0041873b
    TC(0x00480B00, 0x00519A60, x0 - 0x10, y0 - 0x10, 0x10, hb, (int)0x80007fff);   // 0x0041874a fillWindowRect
    TC(0x00480B00, 0x00519A60, x0 - 0x10, y1, FD(0x34), 0x10, (int)0x80007fff);    // 0x0041874f..0x00418765 fillWindowRect
    TC(0x00480B00, 0x00519A60, x1, y0 - 0x10, 0x10, FD(0x28), (int)0x80007fff);    // 0x0041876a..0x00418780 fillWindowRect
    TC(0x00478B80, 0x00519CD4, x0, y0, x1, y0, (int)0x80000000);   // 0x00418793 Widget_fillRect
    TC(0x00478B80, 0x00519CD4, x0, y0, x0, y1, (int)0x80000000);   // 0x004187a6 Widget_fillRect
    TC(0x00478B80, 0x00519CD4, x0, y1, x1, y1, (int)0x80000000);   // 0x004187b9 Widget_fillRect
    TC(0x00478B80, 0x00519CD4, x1, y0, x1, y1, (int)0x80000000);   // 0x004187cc Widget_fillRect
    const int g2 = FD(0x2c);                               // 0x004187d1
    if (G16(0x0057956A + g2) == 4 && GU8(0x0057953D + g2) == 0x32) {   // 0x004187d5..0x004187e8
        TC(0x004762D0, Root(), 0x00519928, 0, 0, 0);       // 0x004187fb setDrawTarget
        CC(0x00404BC0, 0x004C6220, (x1 + x0) / 2, y0 + 4, (int)0x80007ff0);   // 0x00418800..0x00418817 drawCenteredText2; 0x0041881c
    }
    TC(0x004762D0, Root(), 0x0051B360, 0, 0, 0);           // 0x00418830 setDrawTarget
    CC(0x00404B70, 0x004C4E58, (FD(0xc) + x0) / 2, FD(0x18) + 4, (int)0x80006318);   // 0x00418835..0x00418854 drawCenteredText (popped at 0x004189d8)

    // 0x00418859..0x004189ce: the date month'day'year into the text buffer
    const int dm = G32(0x00834170) % 8192;                 // 0x00418859..0x0041886b
    FD(0x34) = dm;                                         // 0x0041886c
    GU8(kBuf) = 0;                                         // 0x00418880
    StrCat(kBuf, (u32)Itoa(dm / 1024 + 3, 0x0058A528, 10));   // 0x00418872..0x0041888e __itoa; 0x00418893..0x004188c2
    StrCat(kBuf, 0x004C4E54);                              // 0x004188c4..0x004188f6
    StrCat(kBuf, (u32)Itoa((FD(0x34) % 1024) * 30 / 1024 + 1, 0x0058A528, 10));   // 0x004188e8..0x00418917 __itoa; 0x0041891c..0x0041894b
    StrCat(kBuf, 0x004C4E54);                              // 0x0041894d..0x00418983
    StrCat(kBuf, (u32)Itoa((int)G16(0x005A6D3C) + 0x7d1, 0x0058A528, 10));   // 0x00418971..0x00418985 __itoa; 0x0041898a..0x004189be
    CC(0x00404AD0, (int)kBuf, FD(0xc) - 0x50, FD(0x18) - 0x10, (int)0x80007ff0);   // 0x004189b2..0x004189ce drawTextSurface; 0x004189d8

    // 0x004189d3..0x00418b02: the snapshot file
    if (G32(0x004C2E0C) & 0x100) return;                   // 0x004189d3 / 0x004189db test ah, 1 / 0x004189de -> 0x00418b05
    StrCpy((u32)FA(0x2a0), kBuf);                          // 0x004189e4..0x00418a0e
    StrCpy(kBuf, 0x004C4E48);                              // 0x00418a10..0x00418a36
    CC(0x004676E0, FD(0x10), 0);                           // 0x00418a38 buildGolferName (pushes 0x00418a0d / 0x00418a2b; popped at 0x00418b02)
    StrCat(kBuf, 0x004C52BC);                              // 0x00418a3d..0x00418a68
    StrCat(kBuf, (u32)FA(0x2a0));                          // 0x00418a6a..0x00418a95
    StrCat(kBuf, 0x004C6218);                              // 0x00418a97..0x00418afb
    CC(0x00431D20, (int)kBuf, Root(), (x0 & 0xffe) - 2, (y0 & 0xffe) - 2,
       (FD(0x30) & 0xffe) + 6, (FD(0x20) & 0xffe) + 6, 1); // 0x00418ab9..0x00418afd saveCourseJpg; 0x00418b02
}

// ====================================================================================================================
// 0x00418b05..0x00418bc5: while [0x005a9cbc] is non-zero, the centre 400x300 of the widget surface 0x00519cd4 is
// blitted over its whole 800x600 (Surface_blit3), then for each of the [0x0058bcb0] points (0x005a47e8 / 0x005a4868)
// mapped to 2x - 400, 2y - 300 inside the screen whose pixel reads 0x7fff, the Image 0x00567b08 is drawn at x - 2, y - 1.
// ====================================================================================================================
static void P5_zoom() {
    if (G32(0x005A9CBC) == 0) return;                      // 0x00418b05..0x00418b0c -> 0x00418bc7
    // 0x00418b12..0x00418b33: `neg / sbb / and 0x519cd4` of the constant 0x519a60 (non-zero) is the constant 0x519cd4
    TC(0x00475D00, 0x00519CD4, 0x00519CD4, 0xc8, 0x96, 0x190, 0x12c, 0, 0, 0x320, 0x258);   // 0x00418b48 Surface_blit3
    int ebx = 0;                                           // 0x00418b52
    if (G32(0x0058BCB0) <= 0) return;                      // 0x00418b4d / 0x00418b54 / 0x00418b56 -> 0x00418bc7
    do {
        const int sx = G32(0x005A47E8 + ebx * 4) * 2 - 0x190;   // 0x00418b58 / 0x00418b66
        const int sy = G32(0x005A4868 + ebx * 4) * 2 - 0x12c;   // 0x00418b5f / 0x00418b6d
        if (sx >= 0 && sx < 0x320 && sy >= 0 && sy < 0x258) {   // 0x00418b74..0x00418b8a (signed)
            if (TC(0x00478DF0, 0x00519CD4, sx, sy) == 0x7fff)   // 0x00418b93 Widget_readPixel / 0x00418b98 / 0x00418b9d
                TC(0x00473CB0, 0x00567B08, Root(), sx - 2, sy - 1, 1, 1, 1, 0);   // 0x00418b9f..0x00418bb8 Surface_drawScaled
        }
        ebx++;                                             // 0x00418bc2
    } while (ebx < G32(0x0058BCB0));                       // 0x00418bbd / 0x00418bc3 / 0x00418bc5
}

// ---- entry: 0x004175f2..0x00418bc7, falls through to 0x00418bc7 with no register live ----
void Ph_004175f2(u8* F) {
    P5_sim(F);                                             // 0x004175f2..0x004185b8
    P5_simfoto(F);                                         // 0x004185bb..0x00418b02
    P5_zoom();                                             // 0x00418b05..0x00418bc5
}

}  // namespace
// c3fy part P6: mainLoop 0x0040f5c0, range 0x00418bc7..0x00419d06 (1,350 instructions, 85 call sites).
// Hand-written from log/c3/alt/c3fy_norm.asm. Every [F+X] of the original is a frame access at the same offset and
// width; registers are C locals. Entry: no register live. Exit: returns eax as it is at 0x00419d06 (= [0x004c2854],
// read at 0x00419a39, 0x00419c12 or 0x00419cfe by path). At 0x00419d06 the original also holds ebp = 0 (0x004194bb),
// ebx = -1 (0x0041950b) and edi = 0x1e0 (0x00419c1b) on every path (notes: INTERFACE).
//
// Phases in order:
//   P6_Census     0x00418bc7..0x00418cd7  course census counters, the current course record's summary bytes
//   P6_Hud        0x00418cd7..0x004194b5  golfer list column, title / difficulty / date lines, the counts row
//   P6_StatusBar  0x004194b5..0x0041977a  cash, fun / skill ratings, the waiting-list text, cursor to view coords
//   P6_Panels     0x0041977a..0x0041993e  radial menu, the panel of the current mode [0x00567afc], hole / golfer popups
//   P6_PauseLabel 0x0041993e..0x004199b4  pause / speed label text
//   P6_Cursor     0x004199b4..0x00419c17  label draw, input pump, cursor tile, measured distance
//   P6_ToolLabel  0x00419c17..0x00419d06  build-tool label at the cursor tile

namespace {

// [[0x0083ad50] + 0x9c] (thiscall, 3 arguments, pops 12), called only while [0x0083ad50] != 0 (je 0x00418cc7 / 0x0041976a; 0x00418cbf..0x00418cd1,
// 0x00419762..0x00419774). The vtable is read at the time of the call.
static void P6_DisplaySlot9c() {
    int d = G32(0x0083AD50);
    if (d != 0) {
        u32 fn = GU32(GU32(static_cast<u32>(d)) + 0x9c);
        TC(fn, d, 1, 1, 1);
    }
}

// showInfoCursorPanel 0x00433190 (cdecl, 2 arguments, `add esp, 8` at 0x004197ef). re/c3_hold.tsv records that it
// returns with the caller's esi changed; mainLoop never reads esi after 0x004197e5 before writing it (notes), so the
// original's esi is dead there. The call saves and restores ebx / esi / edi / ebp around the callee so that a changed
// callee-saved register cannot reach this body's own register variables.
__declspec(noinline) static int P6_CallSavingRegs2(u32 addr, int a, int b) {
    int r;
    __asm {
        push ebx
        push esi
        push edi
        push ebp
        mov eax, b
        push eax
        mov eax, a
        push eax
        mov eax, addr
        call eax
        add esp, 8
        pop ebp
        pop edi
        pop esi
        pop ebx
        mov r, eax
    }
    return r;
}

// ------------------------------------------------------------------------------------------------------------------
// 0x00418bc7..0x00418cd7  census: [0x005a9cb4] = cash [0x00571fd4] / 1000; [0x005a9cac] = number of set bits 0 / 1 in
// the first dword of the 18 records at 0x00575eb8 (stride 0x208); [0x005a9cb0] = number of the 256 placed-object
// records 0x0058bcb8 (stride 0x10) whose word +0 is 5 and whose dword +8 is nonzero. Then bytes +8..+0x11 of the course
// record [0x0059bf90] (0x00571ff4, stride 0x2e) get those two counts, 0, the low bytes of [0x005787cc] / [0x00561258],
// and cash / 256 and cash % 256. Then setDrawTarget 0x0051b360 and the display slot +0x9c.
static void P6_Census(u8* F) {
    (void)F;
    int cash = G32(0x00571FD4);                 // 0x00418bc7 esi
    int edi = 0;                                // 0x00418bd9
    int ebx = 0;                                // 0x00418bde
    G32(0x005A9CAC) = ebx;                      // 0x00418be2
    G32(0x005A9CB8) = edi;                      // 0x00418be8
    G32(0x005A9CB0) = edi;                      // 0x00418bee
    G32(0x005A9CB4) = cash / 1000;              // 0x00418bcd..0x00418bf4 (imul 0x10624dd3, sar 6, sign fix)
    for (u32 p = 0x00575EB8; p < 0x00578348; p += 0x208) {   // 0x00418bfa..0x00418c17
        u32 c = GU32(p);                        // 0x00418bff
        if (c & 1) ebx++;                       // 0x00418c01 / 0x00418c04 / 0x00418c06
        if (c & 2) ebx++;                       // 0x00418c07 / 0x00418c0a / 0x00418c0c
    }
    G32(0x005A9CAC) = ebx;                      // 0x00418c19
    for (u32 p = 0x0058BCC0; p < 0x0058CCC0; p += 0x10) {    // 0x00418c1f..0x00418c41
        if (GU16(p - 8) == 5 && G32(p) != edi) // 0x00418c29 / 0x00418c2d / 0x00418c2f / 0x00418c31
            G32(0x005A9CB0) = G32(0x005A9CB0) + 1;   // 0x00418c33 inc dword
    }
    u32 rec = static_cast<u32>(G32(0x0059BF90)) * 46u;   // 0x00418c43..0x00418c5b
    u8 dl = GU8(0x005A9CB0);                    // 0x00418c48
    u8 al = GU8(0x005787CC);                    // 0x00418c56
    GU8(rec + 0x00571FFC) = static_cast<u8>(ebx);   // 0x00418c5d
    GU8(rec + 0x00571FFD) = dl;                 // 0x00418c63
    u8 dl2 = GU8(0x00561258);                   // 0x00418c69
    GU8(rec + 0x00571FFE) = 0;                  // 0x00418c6f
    GU8(rec + 0x00571FFF) = al;                 // 0x00418c76
    GU8(rec + 0x00572000) = dl2;                // 0x00418c7e
    GU8(rec + 0x00572004) = static_cast<u8>(cash / 256);   // 0x00418c84..0x00418c90 (cdq, and 0xff, sar 8)
    GU8(rec + 0x00572005) = static_cast<u8>(cash % 256);   // 0x00418c96..0x00418ca8 (and 0x800000ff, jns 0x00418c9d sign fix)
    TC(0x004762D0, Root(), 0x0051B360, edi, edi, edi);    // 0x00418cba setDrawTarget
    P6_DisplaySlot9c();                         // 0x00418cbf..0x00418cd1
}

// icon + count text: Image_draw of the icon at (ix, y), kBuf = 0x004c61e4 (one letter) + __itoa([cnt]) with [cnt]
// read after Image_draw as the original does, drawText4049d0 of kBuf at (tx, y). The original repeats this sequence
// five times; each caller cites its call sites.
static void P6_IconCount(u32 img, int ix, int y, u32 cnt, int tx) {
    TC(0x00473E60, static_cast<int>(img), Root(), ix, y, 0);   // Image_draw
    StrCpy(kBuf, 0x004C61E4);
    int r = Itoa(G32(cnt), 0x0058A528, 10);     // __itoa
    StrCat(kBuf, static_cast<u32>(r));
    CC(0x004049D0, static_cast<int>(kBuf), tx, y, 0x80007FFFu);   // drawText4049d0
}

// ------------------------------------------------------------------------------------------------------------------
// 0x00418cd7..0x004194b5  F+0xb0 = 0, F+0x14 = -1, setDrawTarget 0x0051b360. Unless bit 0x200000 of [0x0059e7b8]
// (0x00418cf9): setDrawTarget 0x00519fd8; the 0x98 dwords at 0x00567ff4 = -1; then for i = 0..0x97, golfer
// g = ([0x005a5a24] - i + 0x98) % 0x98 (record 0x005794c0 + 0x100 g) with byte +0x19 > 0 (and byte +0x19 > 9 while
// [0x005685f0] > 10 and the high byte of GetAsyncKeyState(0x10) is nonzero), one entry at x = ebp (0xcc, step 0x10,
// entries stop once ebp > 0x310): the image 0x0059b778 + 0x2c clamp(word +0x9c + 2, 1, 10) at (ebp, 0x23f), a 0xe x 9
// box at (ebp + 1, 0x24f) coloured by byte +0x10, the number byte +0x19 centred at (ebp + 8, 0x24f) in a colour chosen
// from words +0x9e / +0xa0 / +0xa2 and bit 0x20000000 of dword +8; g is appended to the table at 0x00567ff4 (cursor
// F+0x2c, index slot F+0x18, g in F+0x34). Then the title strip, the course image, the course title, the difficulty
// label (erased at 0x00419021 before it is drawn, notes), the date, and the counts row (one icon per unit, or the icon
// and its count as text when the four counts sum to 20 or more), then the [0x0056d1b0] row.
static void P6_Hud(u8* F) {
    int root = Root();                          // 0x00418cd7
    FD(0xb0) = 0;                               // 0x00418ce5
    FD(0x14) = -1;                              // 0x00418cec
    TC(0x004762D0, root, 0x0051B360, 0, 0, 0);  // 0x00418cf4 setDrawTarget
    if (GU32(0x0059E7B8) & 0x200000) return;    // 0x00418cf9 / 0x00418d03 -> 0x004194b5

    root = Root();                              // 0x00418d09
    int ebp = 0xcc;                             // 0x00418d1a
    TC(0x004762D0, root, 0x00519FD8, 0, 0, 0);  // 0x00418d1f setDrawTarget
    FD(0x2c) = 0x00567FF4;                      // 0x00418d31
    for (u32 k = 0; k < 0x98; k++) G32(0x00567FF4 + 4 * k) = -1;   // 0x00418d24..0x00418d39 rep stosd
    int edi = 0;                                // 0x00418d3b
    FD(0x18) = edi;                             // 0x00418d3d
    do {
        int g = (G32(0x005A5A24) - edi + 0x98) % 0x98;   // 0x00418d41..0x00418d53 (idiv: signed remainder)
        FD(0x34) = g;                           // 0x00418d57
        u32 s = static_cast<u32>(g) << 8;       // 0x00418d5b
        s8 lev = G8(s + 0x005794D9);            // 0x00418d5e
        if (lev <= 0) goto next;                // 0x00418d64 / 0x00418d66 je / 0x00418d6c jle -> 0x00418ed8
        if (G32(0x005685F0) > 10) {             // 0x00418d72 / 0x00418d79
            SHORT ks = reinterpret_cast<SHORT(WINAPI*)(int)>(GU32(0x004BA1F0))(0x10);   // 0x00418d7d GetAsyncKeyState
            u8 hi = static_cast<u8>(static_cast<u16>(ks) >> 8);   // 0x00418d83 xor edx, edx / 0x00418d85 mov dl, ah
            if (hi != 0) {                      // 0x00418d87 / 0x00418d89
                if (G8(s + 0x005794D9) <= 9) goto next;   // 0x00418d8b / 0x00418d92 jle -> 0x00418ed8
            }
        }
        {
            int w = G16(s + 0x0057955C);        // 0x00418d98 movsx word
            int rootD = Root();                 // 0x00418d9f edi
            int c = CC(0x00467130, w + 2, 1, 0xa);   // 0x00418dad clamp
            u32 img = 0x0059B778 + static_cast<u32>(c) * 44u;   // 0x00418db5..0x00418dc9
            TC(0x00473CB0, img, rootD, ebp, 0x23f, 1, 1, 1, 0);   // 0x00418dd1 Surface_drawScaled
            u8 b8 = GU8(s + 0x005794D0);        // 0x00418dd6
            int ebx = ebp + 8;                  // 0x00418ddc
            u32 boxc = (b8 != 0 ? 0xFFFFE320u : 0u) + 0x80007FFFu;   // 0x00418ddf..0x00418deb neg / sbb / and / add
            TC(0x00480B00, 0x00519A60, ebx - 7, 0x24f, 0xe, 9, boxc);   // 0x00418e00 fillWindowRect
            FU(0xc) = 0x80000000u;              // 0x00418e0e
            if (G16(s + 0x00579562) > 0xa0) FU(0xc) = 0x80004210u;    // 0x00418e05 / 0x00418e16 / 0x00418e18
            if (G16(s + 0x00579560) > 0x10) FU(0xc) = 0x80000018u;    // 0x00418e25 / 0x00418e2c / 0x00418e2e
            if (G16(s + 0x0057955E) > 0x10) FU(0xc) = 0x80006000u;    // 0x00418e36 / 0x00418e3d / 0x00418e3f
            if (GU32(s + 0x005794C8) & 0x20000000) FU(0xc) = 0x80007D08u;   // 0x00418e47 / 0x00418e51 / 0x00418e53
            int lv = G8(s + 0x005794D9);        // 0x00418e5b movsx byte
            GU8(kBuf) = 0;                      // 0x00418e6a
            int r = Itoa(lv, 0x0058A528, 10);   // 0x00418e71 __itoa
            StrCat(kBuf, static_cast<u32>(r));  // 0x00418e76..0x00418ea9
            CC(0x00404B70, static_cast<int>(kBuf), ebx, 0x24f, FD(0xc));   // 0x00418eb0 drawCenteredText
            int p = FD(0x2c);                   // 0x00418eb5
            G32(static_cast<u32>(p)) = FD(0x34);   // 0x00418eb9 / 0x00418ec3
            ebp += 0x10;                        // 0x00418ebd
            p += 4;                             // 0x00418ec5
            FD(0x2c) = p;                       // 0x00418ece
            if (ebp > 0x310) break;             // 0x00418ec8 / 0x00418ed2 jg -> 0x00418ee9
            edi = FD(0x18);                     // 0x00418ed4
        }
    next:
        edi++;                                  // 0x00418ed8
        FD(0x18) = edi;                         // 0x00418edf
    } while (edi < 0x98);                       // 0x00418ed9 / 0x00418ee3

    // 0x00418ee9: title strip, frame and the course-type image
    {
        int v = G32(0x00822C74);                // 0x00418ee9
        TC(0x00474030, 0x00520590, Root(), 0x30, 5, v, 1, 1, 1, 0);   // 0x00418f07 Surface_drawScaled2
        TC(0x00473F60, 0x00542F28, 0x0059AB00, Root(), 0x30, 5, 0);   // 0x00418f23 Surface_stretchBlit
        u32 crec = static_cast<u32>(G32(0x0059BF90)) * 46u;   // 0x00418f28..0x00418f39
        int t = G8(crec + 0x00571FF4);          // 0x00418f3b movsx byte (course record +0)
        int u = G8(static_cast<u32>(t) * 130u + 0x004C1EA8);   // 0x00418f43..0x00418f4a movsx byte
        int rootS = Root();                     // 0x00418f52
        u32 off = static_cast<u32>(u) * 44u;    // 0x00418f58..0x00418f5f
        TC(0x00473F60, 0x00541710 + off, static_cast<int>(0x005791F8 + off), rootS, 3, 3, 0);   // 0x00418f6f Surface_stretchBlit
    }
    GU8(kBuf) = 0;                              // 0x00418f76
    CC(0x0040DAA0, 0);                          // 0x00418f7d appendCourseTitle
    CC(0x00404B70, static_cast<int>(kBuf), 0x90, 0xd, 0x80007FFFu);   // 0x00418f93 drawCenteredText

    // 0x00418f98: switch [0x00822c88] through the table 0x00421798 (4 cases; above 3 -> 0x0041901c)
    switch (GU32(0x00822C88)) {                 // 0x00418fa0 cmp 3 / ja (unsigned) / 0x00418fa5 jmp [eax*4 + 0x421798]
    case 0: StrCpy(kBuf, 0x004C6210); break;    // 0x00418fac..0x00418fcc + tail 0x00419017..0x0041901a
    case 1: StrCpy(kBuf, 0x004C6204); break;    // 0x00418fce / 0x00418fd3 jmp -> 0x00418ffc..0x0041901a
    case 2: StrCpy(kBuf, 0x004C61F8); break;    // 0x00418fd5..0x00418ff5 + tail 0x00419017..0x0041901a
    case 3: StrCpy(kBuf, 0x004C61E8); break;    // 0x00418ff7 -> 0x00418ffc..0x0041901a
    default: break;                             // 0x00418fa3 -> 0x0041901c
    }
    {
        int d = G32(0x00834170);                // 0x0041901c g_date
        GU8(kBuf) = 0;                          // 0x00419021 (erases the label copied above)
        CC(0x0040D7B0, d);                      // 0x00419029 appendDate
        TC(0x004762D0, Root(), 0x00519FD8, 0, 0, 0);   // 0x00419042 setDrawTarget
        CC(0x00404B70, static_cast<int>(kBuf), 0x90, 0x1b, 0x80007FFFu);   // 0x00419058 drawCenteredText
    }

    // 0x0041905d: the counts row
    int n = G32(0x00561258) + G32(0x005787CC);  // 0x0041905d..0x0041906c
    n += G32(0x005A9CAC);                       // 0x0041906f / 0x0041907b
    n += G32(0x005A9CB0);                       // 0x00419075 / 0x0041907d
    if (n < 0x14) {                             // 0x0041907f / 0x00419082 jge -> 0x0041916d
        int step = CC(0x00467130, 0xa0 / (n + 1), 2, 0xc);   // 0x00419088..0x00419098 clamp
        int x = 0x94 - (step * n) / 2;          // 0x0041909f..0x004190ac
        for (int i = 0; i < G32(0x005A9CAC); i++) {   // 0x004190ae..0x004190dc (jle 0x004190ba, jl 0x004190dc)
            TC(0x00473E60, 0x0059B28C, Root(), x, 0x26, 0);   // 0x004190cd Image_draw
            x += step;                          // 0x004190d7
        }
        for (int i = 0; i < G32(0x005A9CB0); i++) {   // 0x004190de..0x00419108 (jle 0x004190e7, jl 0x00419108)
            TC(0x00473E60, 0x0059B33C, Root(), x, 0x26, 0);   // 0x004190f9 Image_draw
            x += step;                          // 0x00419103
        }
        for (int i = 0; i < G32(0x005787CC); i++) {   // 0x0041910a..0x00419135 (jle 0x00419113, jl 0x00419135)
            TC(0x00473E60, 0x0059B5FC, Root(), x, 0x26, 0);   // 0x00419126 Image_draw
            x += step;                          // 0x00419130
        }
        for (int i = 0; i < G32(0x00561258); i++) {   // 0x00419137..0x00419166 (jle 0x00419140, jl 0x00419166, jmp 0x00419168: exits -> 0x004193b0)
            TC(0x00473E60, 0x0059B578, Root(), x, 0x26, 0);   // 0x00419157 Image_draw
            x += step;                          // 0x00419161
        }
    } else {
        P6_IconCount(0x0059B28C, 0x50, 0x26, 0x005A9CAC, 0x5e);   // 0x0041917e / 0x004191b5 / 0x004191f2
        P6_IconCount(0x0059B33C, 0x74, 0x26, 0x005A9CB0, 0x82);   // 0x0041920b / 0x00419242 / 0x00419282
        P6_IconCount(0x0059B5FC, 0x98, 0x26, 0x005787CC, 0xa6);   // 0x0041929e / 0x004192d5 / 0x00419315
        P6_IconCount(0x0059B578, 0xbc, 0x26, 0x00561258, 0xca);   // 0x00419331 / 0x00419368 / 0x004193a8
    }

    // 0x004193b0: the [0x0056d1b0] row
    int m = G32(0x0056D1B0);                    // 0x004193b0
    if (m <= 0) return;                         // 0x004193b6 / 0x004193b8 -> 0x004194b5
    int step = CC(0x00467130, 0x96 / (m + 1), 2, 0xc);   // 0x004193be..0x004193ce clamp
    int x = 0x94 - (step * m) / 2;              // 0x004193d5..0x004193e2
    int m2 = G32(0x0056D1B0);                   // 0x004193e4
    if (m2 < 0xa) {                             // 0x004193ec / 0x004193ef jge -> 0x00419422
        for (int i = 0; i < m2;) {              // 0x004193f1..0x0041941b (jle 0x004193f5, jl 0x0041941b; exit -> 0x004194b5)
            TC(0x00473E60, 0x0059B3C0, Root(), x, 0x32, 0);   // 0x0041940c Image_draw
            m2 = G32(0x0056D1B0);               // 0x00419411
            x += step;                          // 0x00419416
            i++;                                // 0x00419418
        }
        return;                                 // 0x0041941d -> 0x004194b5
    }
    P6_IconCount(0x0059B3C0, 0x88, 0x32, 0x0056D1B0, 0x96);   // 0x00419436 / 0x0041946d / 0x004194ad
}

// ------------------------------------------------------------------------------------------------------------------
// 0x004194b5..0x0041977a  setDrawTarget 0x0051b360; three framed boxes at (0x289, 0xd), (0x2a5, 0x37), (0x2bb, 0x63)
// (drawScaledSprite421b90 then Surface_stretchBlit each); [0x0056a520] = 0; kBuf = 0x004c61e0 + appendNumber(cash * 100)
// (+ 0x004c61dc when bit 0x20000000 of [0x0059e7b8]), drawn at (0x2c4, 0x23) in 0x800023e8 when cash >= 0, else in
// 0x80007d08 only while bit 2 of [0x00834170] is set; nothing of it when bit 0x1000000 of [0x0059e7b8]. Then
// __itoa([0x0059ae78]) at (0x2d2, 0x4e), holeMagazineEvent, appendCents([0x00541cd8]) at (0x2dc, 0x79); kBuf is left
// holding 0x004c61ac + appendNumber([0x0056d1b0]) (not drawn in this range). View_toLocal into F+0xb8 / F+0xb0,
// capsLockOff, display slot +0x9c.
static void P6_StatusBar(u8* F) {
    TC(0x004762D0, Root(), 0x0051B360, 0, 0, 0);   // 0x004194c5 setDrawTarget (ebp = 0 at 0x004194bb)
    {
        int v = G32(0x00822C74);                // 0x004194ca
        TC(0x00421B90, 0x005205BC, Root(), 0x289, 0xd, v, 1, 1, 0);   // 0x004194e8 drawScaledSprite421b90
        TC(0x00473F60, 0x00542F54, 0x0059AB2C, Root(), 0x289, 0xd, 0);   // 0x00419506 Surface_stretchBlit
    }
    G32(0x0056A520) = 0;                        // 0x0041951d (inside the copy, after the source scan)
    StrCpy(kBuf, 0x004C61E0);                   // 0x0041950b..0x0041953e
    CC(0x0042DC00, static_cast<int>(static_cast<u32>(G32(0x00571FD4)) * 100u));   // 0x00419533..0x00419547 appendNumber
    u32 fl = GU32(0x0059E7B8);                  // 0x0041954c
    if (!(fl & 0x1000000)) {                    // 0x00419554 / 0x00419559 -> 0x004195be
        if (fl & 0x20000000) StrCat(kBuf, 0x004C61DC);   // 0x0041955b / 0x00419560; 0x00419562..0x0041958b
        if (G32(0x00571FD4) >= 0) {             // 0x0041958d / 0x00419593 jl -> 0x0041959c
            CC(0x00404BC0, static_cast<int>(kBuf), 0x2c4, 0x23, 0x800023E8u);   // 0x00419595 / jmp 0x0041959a / 0x004195b6 drawCenteredText2
        } else if (GU8(0x00834170) & 4) {       // 0x0041959c / 0x004195a3
            CC(0x00404BC0, static_cast<int>(kBuf), 0x2c4, 0x23, 0x80007D08u);   // 0x004195a5 / 0x004195b6 drawCenteredText2
        }
    }
    {
        int v = G32(0x00822C74);                // 0x004195be
        TC(0x00421B90, 0x005205E8, Root(), 0x2a5, 0x37, v, 1, 1, 0);   // 0x004195dc drawScaledSprite421b90
        TC(0x00473F60, 0x00542F80, 0x0059AB58, Root(), 0x2a5, 0x37, 0);   // 0x004195fa Surface_stretchBlit
    }
    StrCpy(kBuf, 0x004C61CC);                   // 0x004195ff..0x00419628
    {
        int v = G32(0x0059AE78);                // 0x0041962a
        GU8(kBuf) = 0;                          // 0x00419630 (erases the label just copied)
        int r = Itoa(v, 0x0058A528, 10);        // 0x00419638 __itoa
        StrCat(kBuf, static_cast<u32>(r));      // 0x0041963d..0x00419674
        CC(0x00404BC0, static_cast<int>(kBuf), 0x2d2, 0x4e, 0x80007FF0u);   // 0x00419676 drawCenteredText2
    }
    CC(0x0042DEA0);                             // 0x0041967e holeMagazineEvent
    {
        int v = G32(0x00822C74);                // 0x00419683
        TC(0x00421B90, 0x00520614, Root(), 0x2bb, 0x63, v, 1, 1, 0);   // 0x004196a1 drawScaledSprite421b90
        TC(0x00473F60, 0x00542FAC, 0x0059AB84, Root(), 0x2bb, 0x63, 0);   // 0x004196bf Surface_stretchBlit
    }
    StrCpy(kBuf, 0x004C61BC);                   // 0x004196c4..0x004196e6
    {
        int v = G32(0x00541CD8);                // 0x004196e8
        GU8(kBuf) = 0;                          // 0x004196ee (erases the label just copied)
        CC(0x0042DD50, v);                      // 0x004196f6 appendCents
        CC(0x00404BC0, static_cast<int>(kBuf), 0x2dc, 0x79, 0x800003FFu);   // 0x0041970c drawCenteredText2
    }
    StrCpy(kBuf, 0x004C61AC);                   // 0x00419711..0x00419739
    CC(0x0042DC00, G32(0x0056D1B0));            // 0x00419720 / 0x0041973b appendNumber
    TC(0x0047AB50, 0x00519A60, FA(0xb8), FA(0xb0));   // 0x00419758 View_toLocal
    CC(0x004321D0);                             // 0x0041975d capsLockOff
    P6_DisplaySlot9c();                         // 0x00419762..0x00419774
}

// ------------------------------------------------------------------------------------------------------------------
// 0x0041977a..0x0041993e  [0x005a34e8] = [0x004c2844], [0x004c2844] = 4 for the panels (restored after each); bit 0x800
// of [0x0059e7b8] forces mode [0x00567afc] = 1, [0x0055e928] = 0, [0x004c2854] = 0xf. showMainRadialMenu at the view
// point (F+0xb8, F+0xb0), then the panel of mode [0x00567afc]: 0 showInfoCursorPanel, 1 drawBuildingPanel,
// 2 drawEmployeeViewPanel, 3 / 4 drawGolferActionPanel; any other mode moves [0x004c2e20] into F+0x14 (and sets it
// to -1) when x >= 0x294 or [0x004c2e20] != -1, and [0x004c2e0c] == -1; else F+0x14 = -1. Mode 0 with [0x004c2854] == 5
// and [0x0056d1b0] <= 0 sets [0x004c2854] = -1. Then the popup request [0x0053df54]: < 0 showHoleStatsScreen(-v);
// > 0 golfer g = v - 1: byte +0x19 (0x005794d9 + 0x100 g) == -1 hireEmployee and [0x0053df54] = 0, byte 0 or 0x13 drawGolferInfoPanel(g, 1),
// else drawGolferInfoPanel(g, 0).
static void P6_Panels(u8* F) {
    u32 flags = GU32(0x0059E7B8);               // 0x0041977a
    int zoom = G32(0x004C2844);                 // 0x0041977f
    G32(0x005A34E8) = zoom;                     // 0x00419788
    G32(0x004C2844) = 4;                        // 0x0041978e
    if (flags & 0x800) {                        // 0x00419785 test ah, 8 / 0x00419798
        G32(0x00567AFC) = 1;                    // 0x0041979a
        G32(0x0055E928) = 0;                    // 0x004197a4
        G32(0x004C2854) = 0xf;                  // 0x004197aa
    }
    CC(0x00432BA0, FD(0xb8), FD(0xb0));         // 0x004197c4 showMainRadialMenu
    int mode = G32(0x00567AFC);                 // 0x004197c9
    if (mode == 0) {                            // 0x004197d1 / 0x004197d3
        P6_CallSavingRegs2(0x00433190, FD(0xb8), FD(0xb0));   // 0x004197e5 showInfoCursorPanel
        G32(0x004C2844) = G32(0x005A34E8);      // 0x004197ea / 0x004197f2
        goto L4198c2;                           // 0x004197f7
    }
    if (mode == 1) {                            // 0x004197fc / 0x004197ff
        CC(0x00434350, FD(0xb8), FD(0xb0));     // 0x00419811 drawBuildingPanel
        G32(0x004C2844) = G32(0x005A34E8);      // 0x00419816 / 0x0041981e
        goto L4198c2;                           // 0x00419823
    }
    if (mode == 2) {                            // 0x00419828 / 0x0041982b
        CC(0x00435760, FD(0xb8), FD(0xb0));     // 0x0041983d drawEmployeeViewPanel
        G32(0x004C2844) = G32(0x005A34E8);      // 0x00419842 / 0x0041984a
        goto L4198c2;                           // 0x0041984f
    }
    if (mode == 3 || mode == 4) {               // 0x00419851 / 0x00419854 / 0x00419856 / 0x00419859 -> 0x0041989e
        CC(0x004362F0, FD(0xb8), FD(0xb0));     // 0x004198ae drawGolferActionPanel
        G32(0x004C2844) = G32(0x005A34E8);      // 0x004198b3 / 0x004198bc
        goto L4198c2;                           // falls into 0x004198c2
    }
    G32(0x004C2844) = G32(0x005A34E8);          // 0x0041985b / 0x00419861
    {
        int x = FD(0xb8);                       // 0x00419867
        if ((x < 0x294 && G32(0x004C2E20) == -1) || G32(0x004C2E0C) != -1) {   // 0x0041986e / 0x00419874 jge, 0x00419876 / 0x0041987c je, 0x0041987e / 0x00419884 je
            FD(0x14) = -1;                      // 0x00419886
        } else {
            int v = G32(0x004C2E20);            // 0x0041988c
            G32(0x004C2E20) = -1;               // 0x00419892
            FD(0x14) = v;                       // 0x00419898
        }
    }
    goto L4198c7;                               // 0x0041988a / 0x0041989c (eax still the mode read at 0x004197c9)
L4198c2:
    mode = G32(0x00567AFC);                     // 0x004198c2
L4198c7:
    if (mode == 0 && G32(0x004C2854) == 5 && G32(0x0056D1B0) <= 0)   // 0x004198c7 / 0x004198c9, 0x004198cb / 0x004198d2, 0x004198d4 / 0x004198da jg
        G32(0x004C2854) = -1;                   // 0x004198dc

    int h = G32(0x0053DF54);                    // 0x004198e2
    if (h == 0) return;                         // 0x004198e7 / 0x004198e9 -> 0x0041993e
    if (h < 0) {                                // 0x004198eb jge
        CC(0x00453330, static_cast<int>(0u - static_cast<u32>(h)));   // 0x004198ed / 0x004198f0 showHoleStatsScreen
        return;                                 // 0x004198f8
    }
    int g = h - 1;                              // 0x004198fa
    u8 t = GU8((static_cast<u32>(g) << 8) + 0x005794D9);   // 0x004198fd..0x00419902
    if (t == 0xff) {                            // 0x00419908 / 0x0041990a -> 0x00419933
        CC(0x0040AA80);                         // 0x00419933 hireEmployee
        G32(0x0053DF54) = 0;                    // 0x00419938
    } else if (t == 0 || t == 0x13) {           // 0x0041990c / 0x0041990e, 0x00419910 / 0x00419912 -> 0x00419922
        CC(0x0045C560, g, 1);                   // 0x00419929 drawGolferInfoPanel; jmp 0x00419931 -> 0x0041993e
    } else {
        CC(0x0045C560, g, 0);                   // 0x00419918 drawGolferInfoPanel; jmp 0x00419920 -> 0x0041993e
    }
}

// ------------------------------------------------------------------------------------------------------------------
// 0x0041993e..0x004199b4  kBuf = "" + 0x004c61a4 when bit 2 of byte [0x0059e7b8] and [0x004c2e0c] == -1, + 0x004c619c
// when [0x0059b04c] != 0.
static void P6_PauseLabel(u8* F) {
    (void)F;
    u8 fl = GU8(0x0059E7B8);                    // 0x0041993e
    GU8(kBuf) = 0;                              // 0x00419943
    if ((fl & 4) && G32(0x004C2E0C) == -1)      // 0x0041994a / 0x0041994c / 0x0041994e / 0x00419954
        StrCat(kBuf, 0x004C61A4);               // 0x00419956..0x0041997f
    if (G32(0x0059B04C) != 0)                   // 0x00419981 / 0x00419987
        StrCat(kBuf, 0x004C619C);               // 0x00419989..0x004199b2
}

// ------------------------------------------------------------------------------------------------------------------
// 0x004199b4..0x00419c17  setDrawTarget 0x00519948, kBuf centred at (0x190, 0xa); pumpInput483bd0; View_toLocal into
// F+0x68 / F+0x58; [0x005a9cc0] = [0x005a9cbc]; screenToTile(F+0x68, F+0x58) into 0x00543d08 / 0x00543d0c. When
// F+0x58 < 0x1f4, ([0x004c2848] == -1 and [0x004c2854] == -1, or mode [0x00567afc] 2 or 5) and [0x00822d8c] != -1:
// q = distance(F+0x68 - [0x00822d8c], (F+0x58 - [0x00822d90]) * 32 / 20) * 25 / (11 * [0x004c2844]); when q > 25 four
// drawMinimapSegment lines between the cursor and ([0x00822d8c], [0x00822d90]) and q + 0x004c6194 centred between
// them. Returns [0x004c2854] (read at 0x00419a39 on the paths that leave for 0x00419c17, else at 0x00419c12).
static int P6_Cursor(u8* F) {
    TC(0x004762D0, Root(), 0x00519948, 0, 0, 0);   // 0x004199c2 setDrawTarget
    CC(0x00404BC0, static_cast<int>(kBuf), 0x190, 0xa, 0x80007FFFu);   // 0x004199d8 drawCenteredText2
    CC(0x00483BD0);                             // 0x004199e0 pumpInput483bd0
    TC(0x0047AB50, 0x00519A60, FA(0x68), FA(0x58));   // 0x004199f4 View_toLocal
    {
        int vy = FD(0x58);                      // 0x004199f9
        int vx = FD(0x68);                      // 0x004199fd
        int z = G32(0x005A9CBC);                // 0x00419a01
        G32(0x005A9CC0) = z;                    // 0x00419a14
        CC(0x00430020, vx, vy, 0x00543D08, 0x00543D0C, 0);   // 0x00419a1a screenToTile
    }
    int vy = FD(0x58);                          // 0x00419a1f
    if (vy >= 0x1f4) return G32(0x004C2854);    // 0x00419a26 / 0x00419a2c -> 0x00419c12
    int a = G32(0x004C2848);                    // 0x00419a32
    int eax = G32(0x004C2854);                  // 0x00419a39
    bool go = (a == -1 && eax == -1);           // 0x00419a37 / 0x00419a3e / 0x00419a40 / 0x00419a42
    if (!go) {
        int md = G32(0x00567AFC);               // 0x00419a44
        go = (md == 2 || md == 5);              // 0x00419a4a / 0x00419a4d / 0x00419a4f / 0x00419a52
    }
    if (!go) return eax;                        // 0x00419a52 -> 0x00419c17
    int ox = G32(0x00822D8C);                   // 0x00419a58
    if (ox == -1) return eax;                   // 0x00419a5e / 0x00419a60 -> 0x00419c17
    int oy = G32(0x00822D90);                   // 0x00419a66
    int dy = static_cast<int>(static_cast<u32>(vy - oy) << 5) / 20;   // 0x00419a71..0x00419a86 (0x66666667, sar 3)
    int dx = FD(0x68) - ox;                     // 0x00419a78 / 0x00419a81
    int dist = CC(0x0040ACD0, dx, dy);          // 0x00419a8a distance
    int zoom = G32(0x004C2844);                 // 0x00419a8f
    int q = (dist * 25) / (zoom * 11);          // 0x00419a95..0x00419aa5 idiv
    if (q <= 0x19) return G32(0x004C2854);      // 0x00419aa9 / 0x00419aac -> 0x00419c12

    CC(0x00406220, FD(0x68) + 1, FD(0x58) + 1, G32(0x00822D8C) + 1, G32(0x00822D90) + 1,
       0x80000000u, 2, 7);                      // 0x00419ab2..0x00419ad6 drawMinimapSegment
    CC(0x00406220, FD(0x68), FD(0x58) + 1, G32(0x00822D8C), G32(0x00822D90) + 1,
       0x80000000u, 2, 7);                      // 0x00419adb..0x00419b00 drawMinimapSegment
    CC(0x00406220, FD(0x68), FD(0x58), G32(0x00822D8C), G32(0x00822D90),
       0x80007FF0u, 2, 7);                      // 0x00419b05..0x00419b2c drawMinimapSegment
    CC(0x00406220, FD(0x68) + 1, FD(0x58), G32(0x00822D8C) + 1, G32(0x00822D90),
       0x80007FF0u, 2, 7);                      // 0x00419b31..0x00419b56 drawMinimapSegment
    TC(0x004762D0, Root(), 0x00519928, 0, 0, 0);   // 0x00419b6c setDrawTarget
    GU8(kBuf) = 0;                              // 0x00419b79
    int r = Itoa(q, 0x0058A528, 10);            // 0x00419b80 __itoa
    StrCat(kBuf, static_cast<u32>(r));          // 0x00419b85..0x00419bb0
    StrCat(kBuf, 0x004C6194);                   // 0x00419bb2..0x00419bdd
    int ty = (FD(0x58) + G32(0x00822D90)) / 2 - 4;   // 0x00419bd4 / 0x00419bdf..0x00419bf2
    int tx = (FD(0x68) + G32(0x00822D8C)) / 2;  // 0x00419bea / 0x00419bf6..0x00419c02
    CC(0x00404BC0, static_cast<int>(kBuf), tx, ty, 0x80007FFFu);   // 0x00419c0a drawCenteredText2
    return G32(0x004C2854);                     // 0x00419c12
}

// ------------------------------------------------------------------------------------------------------------------
// 0x00419c17..0x00419d06  when F+0x58 < 0x1e0 and bit 0x8000000 of [0x0059e7b8]: setDrawTarget 0x0051b360,
// k = buildToolLabel(tile 0x00543d08 / 0x00543d0c), tileToScreen of that tile into F+0x120 / F+0x138, the cursor sprite
// 0x0056a760 (k != 0) or 0x0058b358 (k == 0) at (F+0x120 - 0x10, F+0x138 - 0x17), and kBuf centred at
// (F+0x120, F+0x138 - 0xc) in 0x800023e8 (k != 0) or 0x80007fff. Returns eax_in, or [0x004c2854] read at 0x00419cfe
// when the label ran.
static int P6_ToolLabel(u8* F, int eax) {
    if (FD(0x58) >= 0x1e0) return eax;          // 0x00419c17 / 0x00419c20 / 0x00419c22 -> 0x00419d06
    if (!(GU32(0x0059E7B8) & 0x8000000)) return eax;   // 0x00419c28 / 0x00419c32 -> 0x00419d06
    TC(0x004762D0, Root(), 0x0051B360, 0, 0, 0);   // 0x00419c46 setDrawTarget
    int k = CC(0x0040A160, G32(0x00543D08), G32(0x00543D0C));   // 0x00419c4b..0x00419c59 buildToolLabel
    {
        int ty = G32(0x00543D0C);               // 0x00419c5e
        int tx = G32(0x00543D08);               // 0x00419c75
        CC(0x0042F940, tx, ty, FA(0x120), FA(0x138));   // 0x00419c7d tileToScreen
    }
    int sy = FD(0x138);                         // 0x00419c82
    u32 spr = (k != 0) ? 0x0056A760u : 0x0058B358u;   // 0x00419c8c / 0x00419c93 je -> 0x00419cb1; 0x00419caf jmp -> 0x00419ccb
    int sx = FD(0x120);                         // 0x00419c95 / 0x00419cb1
    int rootT = Root();                         // 0x00419c9c / 0x00419cb8
    TC(0x00421B60, spr, rootT, sx - 0x10, sy - 0x17, 1, 1, 0);   // 0x00419ccb drawScaledSprite421b60
    int ly = FD(0x138) - 0xc;                   // 0x00419cd0 / 0x00419ce2
    int lx = FD(0x120);                         // 0x00419cd7
    u32 col = (k != 0 ? 0xFFFFA3E9u : 0u) + 0x80007FFFu;   // 0x00419cde..0x00419ceb neg / sbb / and / add
    CC(0x00404BC0, static_cast<int>(kBuf), lx, ly, col);   // 0x00419cf9 drawCenteredText2
    return G32(0x004C2854);                     // 0x00419cfe
}

// ------------------------------------------------------------------------------------------------------------------
// entry: 0x00418bc7..0x00419d06. Returns eax at 0x00419d06 (= [0x004c2854]).
int Ph_00418bc7(u8* F) {
    P6_Census(F);       // 0x00418bc7..0x00418cd7
    P6_Hud(F);          // 0x00418cd7..0x004194b5
    P6_StatusBar(F);    // 0x004194b5..0x0041977a
    P6_Panels(F);       // 0x0041977a..0x0041993e
    P6_PauseLabel(F);   // 0x0041993e..0x004199b4
    int eax = P6_Cursor(F);         // 0x004199b4..0x00419c17
    return P6_ToolLabel(F, eax);    // 0x00419c17..0x00419d06
}

}  // namespace
// c3fy P7: mainLoop 0x0040f5c0 range 0x00419d06..0x0041c0c5 (falls through to 0x0041c0c5),
// plus the endless loop 0x00420d83..0x00420e0a (reached from 0x0041b209 when [0x4c2848]==0x280).
// Entry: eax_in = [0x004c2854] (current tool), ebp = 0, edi = 0x1e0, ebx = -1 (ebx never read before write here).
// Draws the on-course tool preview: placement box + landmark icon/name, the per-tool minimap markers (switch
// 0x4217a8 on [0x4c2854]), the course-rating / building panels, and the two-corner drag preview (hole/path/sand
// drag) with its flag label. Every exit returns (all 8 jumps leave to 0x0041c0c5). The helper prefix is P7_.
// Prelude (FD/FU/FW/FB/G32/CC/TC/Rnd/Itoa/Root/StrCpy/StrCat/kBuf) is included ahead of this file.

namespace {

// 0x00420d83  endless loop reached only when [0x4c2848]==0x280: place a sprite at every steppable cell forever.
static void P7_endless(u8* F) {
    // 0x00420d83 esi = [0x5a34f0] % 8 (signed, toward zero), computed once.
    int esi = G32(0x5a34f0) % 8;
    for (;;) {
        // 0x00420d96 canStep([0x543d08], [0x543d0c], 0)
        int eax = CC(0x407400, G32(0x543d08), G32(0x543d0c), 0);  // 0x00420da5
        if (eax == 0) continue;                                   // 0x00420daf je 0x420d96
        // 0x00420db1 ebp = [0x5685f4]; eax = ((3 - ebp) & 7) / 2 (signed); edx = -2 - eax; edx &= 3
        int ebp = G32(0x5685f4);                                  // 0x00420db1
        int a = ((3 - ebp) & 7) / 2;                              // 0x00420db7..0x00420dc4 (and 7; cdq;sub;sar 1 = /2)
        int edx = (-2 - a) & 3;                                   // 0x00420dc4..0x00420dd3
        // 0x00420dda cellAt(esi + 0x208, 0, edx)
        int spr = CC(0x43d6f0, esi + 0x208, 0, edx);              // 0x00420dda
        G32(0x5a9cc8) = spr;                                      // 0x00420dec
        int ecx = (esi + 0x8c) | 0x200;                           // 0x00420ddf lea; 0x00420de9 or ch,2
        // 0x00420e00 addSprite([F+0x68], [F+0x58], [F+0x58], 0x28, 0x28, spr, 4, ecx)
        // c3fyc fix: the first argument is edx = [F+0x68] (0x00420de5 mov edx, [F+0x68]; 0x00420dff push edx), not [F+0x58]
        CC(0x4628d0, FD(0x68), FD(0x58), FD(0x58), 0x28, 0x28, spr, 4, ecx);  // 0x00420e00
        // 0x00420e08 jmp 0x420d96
    }
}

// VC6 /O2 signed divide idiom with magic 0x51eb851f and sar edx,3 (used ~7x in the swing-arc box).
static int P7_m51(int x) {
    long long p = (long long)(int)0x51eb851f * x;
    int hi = (int)((unsigned long long)p >> 32);
    int q = hi >> 3;
    return q + (int)((unsigned)q >> 31);
}

// Tile/word flag test: cell word at ([0x543d0c] + 50*[0x543d08]) in table 0x53caf0, bit 0x4000 set.
// (callers pass the two coords already in locals.)

void Ph_00419d06(u8* F, int eax_in) {
    int eax = eax_in, ebx = -1, ecx = 0, edx = 0, esi = 0, edi = 0x1e0, ebp = 0;
    int spr = 0, idx = 0;

    // 0x00419d06  which drag/placement mode is active ([0x567afc]: 1 = placement, 0 = idle preview).
    ecx = G32(0x567afc);                        // 0x00419d06
    if (ecx != 1) goto L419d1e;                 // 0x00419d0f
    if (G32(0x55e928) == ebp) goto L419d34;     // 0x00419d11/0x00419d17 (ebp=0)
    return;                                     // 0x00419d19 jmp 0x41c0c5
L419d1e:
    if (ecx != ebp) goto L41b20f;               // 0x00419d1e/0x00419d20
    ecx = G32(0x55e924);                        // 0x00419d26
    if (ecx == 0) goto L41a990;                 // 0x00419d2e

L419d34:
    // placement box: only while the cursor is inside the course (y < 0x1e0) and the cell is not flag 0x4000.
    if (FD(0x58) >= edi) goto L41a97e;          // 0x00419d34/0x00419d38 (signed)
    edx = G32(0x543d08);                        // 0x00419d3e
    ecx = G32(0x543d0c);                        // 0x00419d4a
    esi = ecx + 50 * edx;                       // 0x00419d44..0x00419d50 (lea chain: ecx + 50*edx)
    if ((GU16(esi * 2 + 0x53caf0) & 0x4000) != 0) goto L41a97e;  // 0x00419d53..0x00419d64
    if (eax == -1) goto L41a97e;                // 0x00419d6a/0x00419d6d
    if (eax == 4) {                             // 0x00419d73
        if (G32(0x56a514) == -1) goto L41a97e;  // 0x00419d78/0x00419d7f
    }
L419d85:
    ebx = G32(eax * 4 + 0x5a8c38);              // 0x00419d85
    esi = (int)G8((eax + eax * 4) * 4 + 0x4c26c0) + ebx;  // 0x00419d8c/0x00419d90/0x00419d98 (footprint)
    // 0x00419d9d evalPlacementArea([0x543d08], [0x543d0c], esi, eax) -> rated height, kept in [F+0x28]
    eax = CC(0x40db90, edx, ecx, esi, eax);     // 0x00419d9d  (edx=[0x543d08], ecx=[0x543d0c])
    ecx = G32(0x4c2854);                        // 0x00419da2
    FD(0x28) = eax;                             // 0x00419dad
    if (ecx != 0) goto L419dc1;                 // 0x00419dab/0x00419db1
    if (eax == -1) goto L419dc1;                // 0x00419db3/0x00419db6
    eax = eax / 2;                              // 0x00419db8..0x00419dbb (cdq;sub;sar 1 = /2 signed)
    FD(0x28) = eax;                             // 0x00419dbd
L419dc1:
    // esi still holds the footprint from 0x00419d98 (callee-saved across 0x40db90); ecx still = [0x4c2854].
    ebp = FD(0x28);                             // 0x00419dc1
    ebx = (((ebp + 1) != 0) ? 0x2f7 : 0) + 0x80007d08;      // 0x00419dcb..0x00419de4 edge colour 1
    FD(0xc) = ecx + 0x90;                       // 0x00419def/0x00419df5 ([F+0xc] = [0x4c2854]+0x90)
    ebp = (((ebp + 1) != 0) ? (int)0xfffffa00 : 0) + 0x800; // 0x00419dea..0x00419e0c edge colour 2 / icon tint
    // --- placement rectangle: four edges, worldToScreen each corner then drawMinimapSegment ---
    // 0x00419e12 worldToScreen([0x543d08]<<10, [0x543d0c]<<10, &F[0x60], &F[0x64], 0)
    CC(0x42fb90, G32(0x543d08) << 10, G32(0x543d0c) << 10, FA(0x60), FA(0x64), 0);     // 0x00419e12
    // 0x00419e40 worldToScreen(((esi+[0x543d08])<<10)-1, [0x543d0c]<<10, &F[0xc8], &F[0xc4], 0)
    CC(0x42fb90, ((esi + G32(0x543d08)) << 10) - 1, G32(0x543d0c) << 10, FA(0xc8), FA(0xc4), 0);  // 0x00419e40
    // 0x00419e6a drawMinimapSegment(F[0x60], F[0x64], F[0xc8], F[0xc4], ebx, 1, 7)
    CC(0x406220, FD(0x60), FD(0x64), FD(0xc8), FD(0xc4), ebx, 1, 7);                   // 0x00419e6a
    // 0x00419e98 worldToScreen(((esi+[0x543d08])<<10)-1, (esi+[0x543d0c])<<10, &F[0x60], &F[0x64], 0)
    CC(0x42fb90, ((esi + G32(0x543d08)) << 10) - 1, (esi + G32(0x543d0c)) << 10, FA(0x60), FA(0x64), 0);  // 0x00419e98
    // 0x00419ebf drawMinimapSegment(F[0x60], F[0x64], F[0xc8], F[0xc4], ebx, 1, 7)
    CC(0x406220, FD(0x60), FD(0x64), FD(0xc8), FD(0xc4), ebx, 1, 7);                   // 0x00419ebf
    // 0x00419eed worldToScreen([0x543d08]<<10, ((esi+[0x543d0c])<<10)-1, &F[0xc8], &F[0xc4], 0)
    CC(0x42fb90, G32(0x543d08) << 10, ((esi + G32(0x543d0c)) << 10) - 1, FA(0xc8), FA(0xc4), 0);  // 0x00419eed
    // 0x00419f17 drawMinimapSegment(F[0x60], F[0x64], F[0xc8], F[0xc4], ebx, 1, 7)
    CC(0x406220, FD(0x60), FD(0x64), FD(0xc8), FD(0xc4), ebx, 1, 7);                   // 0x00419f17
    // 0x00419f42 worldToScreen([0x543d08]<<10, [0x543d0c]<<10, &F[0x60], &F[0x64], 0)
    CC(0x42fb90, G32(0x543d08) << 10, G32(0x543d0c) << 10, FA(0x60), FA(0x64), 0);     // 0x00419f42
    // 0x00419f6c drawMinimapSegment(F[0x60], F[0x64], F[0xc8], F[0xc4], ebx, 1, 7)
    CC(0x406220, FD(0x60), FD(0x64), FD(0xc8), FD(0xc4), ebx, 1, 7);                   // 0x00419f6c
    {
        int cen = (esi << 10) / 2;              // 0x00419f71..0x00419f94 ((esi<<10)/2 signed)
        // 0x00419fa2 worldToScreen(([0x543d08]<<10)+cen, ([0x543d0c]<<10)+cen, &F[0x60], &F[0x64], 0)
        CC(0x42fb90, (G32(0x543d08) << 10) + cen, (G32(0x543d0c) << 10) + cen, FA(0x60), FA(0x64), 0);  // 0x00419fa2
    }
    // 0x00419fbb setDrawTarget(Root(), 0x51b360, 0, 0, 0)
    TC(0x4762d0, Root(), 0x51b360, 0, 0, 0);    // 0x00419fbb
    // build tool name into kBuf
    edx = G32(0x4c2854);                        // 0x00419fc0
    GU8(0x51a068) = 0;                          // 0x00419fc6
    StrCpy(kBuf, 0x4c26b0 + 20 * edx);          // 0x00419fcd..0x00419ff8 strcpy(kBuf, 0x4c26b0 + 20*tool)
    if (edx != 4) goto L41a0e3;                 // 0x00419ff5/0x00419ffa

    // [0x4c2854]==4: Place-Landmark preview (icon + capitalised landmark name + range circle).
    ecx = G32(0x56a514);                        // 0x0041a000
    GU8(0x51a068) = 0;                          // 0x0041a009
    CC(0x4074a0, ecx, 0);                       // 0x0041a010 landmarkName([0x56a514], 0) -> kBuf
    if ((s8)GU8(0x51a068) >= 0x61) {            // 0x0041a015/0x0041a01d (signed char)
        GU8(0x51a068) = (u8)(GU8(0x51a068) + 0xe0);  // 0x0041a021/0x0041a023 uppercase first letter
    }
    ecx = G32(0x56a514);                        // 0x0041a028
    if (ecx == 3) {                             // 0x0041a02e/0x0041a031
        idx = (G32(0x5685f4) + G32(0x5a34f0)) & 3;             // 0x0041a054..0x0041a061
        eax = CC(0x43d6f0, 0x16b, 0, idx);                     // 0x0041a06c cellAt(0x16b, 0, idx)
    } else {
        idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;       // 0x0041a033..0x0041a045
        eax = CC(0x43d6f0, ecx + 0x168, 0, idx);               // 0x0041a06c cellAt([0x56a514]+0x168, 0, idx)
    }
    spr = eax;                                  // cellAt result
    G32(0x5a9cc8) = spr;                        // 0x0041a083
    ecx = (G32(0x56a514) + 0x64) | ebp;         // 0x0041a071/0x0041a07a/0x0041a081 (ebp second colour)
    // 0x0041a09d addSprite(F[0x60], F[0x64], F[0x64], 0x8c, 0x8c, spr, 4, ecx)
    CC(0x4628d0, FD(0x60), FD(0x64), FD(0x64), 0x8c, 0x8c, spr, 4, ecx);               // 0x0041a09d
    {
        int r = (25 * G32(0x56a514) + 0xc8) / 4;   // 0x0041a0a2..0x0041a0c8 (lea 5x,5x; /4 signed)
        // 0x0041a0db drawCircle(([0x543d08]<<10)+0x200, ([0x543d0c]<<10)+0x200, r, 0x80007fff)
        CC(0x407c60, (G32(0x543d08) << 10) + 0x200, (G32(0x543d0c) << 10) + 0x200, r, 0x80007fff);  // 0x0041a0db
    }

L41a0e3:
    eax = G32(0x4c2854);                        // 0x0041a0e3
    esi = G32(eax * 4 + 0x5a8c38);              // 0x0041a0e8 (per-tool colour base)
    {
        unsigned sw = (unsigned)(eax - 1);      // 0x0041a0ef dec
        if (sw > 0x12) goto L41a594;            // 0x0041a0f0/0x0041a0f3 (unsigned ja)
        switch (sw) {                           // 0x0041a0f9 jmp [sw*4 + 0x4217a8]
            case 0:  goto L41a454;
            case 5:  goto L41a36a;
            case 6:  goto L41a165;
            case 7:  goto L41a1a5;
            case 8:  goto L41a1e7;
            case 9:  goto L41a3b5;
            case 10: goto L41a229;
            case 11: goto L41a308;
            case 12: goto L41a26b;
            case 13: goto L41a2a6;
            case 14: goto L41a400;
            case 15: goto L41a100;
            case 18: goto L41a52e;
            default: goto L41a594;              // cases 1,2,3,4,16,17 and the ja path
        }
    }
    // ==== switch cases (0x4217a8) : per-tool minimap marker sprites ====

L41a100:    // tool 16: generic marker, colour from [F+0xc]
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a100..0x0041a112
    {
        int v = G32(0x56a514);
        eax = CC(0x43d6f0, v + 0x12f, G32(v * 4 + 0x53f8a4) - 1, idx);  // 0x0041a12a cellAt
    }
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a13f
    ecx = (G32(0x56a514) + 0xb4) | ebp;                // 0x0041a12f/0x0041a139/0x0041a144
    CC(0x4628d0, FD(0x60), FD(0x64), FD(0x64), 0x64, 0x64, spr, 4, ecx);  // 0x0041a158
    goto L41a594;                                      // 0x0041a160

L41a165:    // tool 7
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a165..0x0041a177
    eax = CC(0x43d6f0, 0x1ec, 0, idx);                 // 0x0041a182
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a190
    ecx = FD(0xc);                                     // 0x0041a187
    if (esi != 0) goto L41a341;                        // 0x0041a18e/0x0041a195
    ecx = 0x10;                                        // 0x0041a19b
    goto L41a341;                                      // 0x0041a1a0

L41a1a5:    // tool 8
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a1a5..0x0041a1bd
    eax = CC(0x43d6f0, esi + 0x1e0, 0, idx);           // 0x0041a1c4
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a1d2
    ecx = FD(0xc);                                     // 0x0041a1c9
    if (esi != 0) goto L41a341;                        // 0x0041a1d0/0x0041a1d7
    ecx = 0xe;                                         // 0x0041a1dd
    goto L41a341;                                      // 0x0041a1e2

L41a1e7:    // tool 9
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a1e7..0x0041a1ff
    eax = CC(0x43d6f0, esi + 0x1d4, 0, idx);           // 0x0041a206
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a214
    ecx = FD(0xc);                                     // 0x0041a20b
    if (esi != 0) goto L41a341;                        // 0x0041a212/0x0041a219
    ecx = 0x1c;                                        // 0x0041a21f
    goto L41a341;                                      // 0x0041a224

L41a229:    // tool 11
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a229..0x0041a241
    eax = CC(0x43d6f0, esi + 0x1c2, 0, idx);           // 0x0041a248
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a256
    ecx = FD(0xc);                                     // 0x0041a24d
    if (esi != 0) goto L41a341;                        // 0x0041a254/0x0041a25b
    ecx = 0x17;                                        // 0x0041a261
    goto L41a341;                                      // 0x0041a266

L41a26b:    // tool 13
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a26b..0x0041a283
    eax = CC(0x43d6f0, esi + 0x1e6, 0, idx);           // 0x0041a28a
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a298
    ecx = FD(0xc);                                     // 0x0041a28f
    if (esi != 0) goto L41a2df;                        // 0x0041a296/0x0041a29d
    ecx = 0x11;                                        // 0x0041a29f
    goto L41a2df;                                      // 0x0041a2a4

L41a2a6:    // tool 14
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a2a6..0x0041a2be
    eax = CC(0x43d6f0, esi + 0x1da, 0, idx);           // 0x0041a2c5
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a2d3
    ecx = FD(0xc);                                     // 0x0041a2ce
    if (esi != 0) goto L41a2df;                        // 0x0041a2d1/0x0041a2d8
    ecx = 0x12;                                        // 0x0041a2da
L41a2df:
    ebp = ebp | ecx;                                   // 0x0041a2df
    CC(0x4628d0, FD(0x60), FD(0x64), 0, 0xf0, 0xf0, spr, 4, ebp);  // 0x0041a2fb
    goto L41a594;                                      // 0x0041a303

L41a308:    // tool 12
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a308..0x0041a320
    eax = CC(0x43d6f0, esi + 0x1ce, 0, idx);           // 0x0041a327
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a335
    ecx = FD(0xc);                                     // 0x0041a32c
    if (esi != 0) goto L41a341;                        // 0x0041a333/0x0041a33a
    ecx = 0x1b;                                        // 0x0041a33c
L41a341:
    ebp = ebp | ecx;                                   // 0x0041a341
    CC(0x4628d0, FD(0x60), FD(0x64), 0, 0x8c, 0x8c, spr, 4, ebp);  // 0x0041a35d
    goto L41a594;                                      // 0x0041a365

L41a36a:    // tool 6
    esi += 0x17c;                                      // 0x0041a373
    eax = CC(0x43d6f0, esi, (int)G8(0x5a34e0), 0);     // 0x0041a37b cellAt(colourBase+0x17c, (s8)[0x5a34e0], 0)
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a38e
    ebp = ebp | 0xb1;                                  // 0x0041a384
    CC(0x4628d0, FD(0x60), FD(0x64), 0, 0x8c, 0x8c, spr, 4, ebp);  // 0x0041a3a8
    goto L41a594;                                      // 0x0041a3b0

L41a3b5:    // tool 10
    esi += 0x17e;                                      // 0x0041a3be
    eax = CC(0x43d6f0, esi, (int)G8(0x5a34e0), 0);     // 0x0041a3c6
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a3d9
    ebp = ebp | 0xb1;                                  // 0x0041a3cf
    CC(0x4628d0, FD(0x60), FD(0x64), 0, 0x8c, 0x8c, spr, 4, ebp);  // 0x0041a3f3
    goto L41a594;                                      // 0x0041a3fb

L41a400:    // tool 15
    idx = ((G32(0x5685f4) / 2) + G32(0x5a34f0)) & 3;   // 0x0041a400..0x0041a412
    eax = CC(0x43d6f0, 0x1f6, 0, idx);                 // 0x0041a41d
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a432
    ebp = ebp | 0x19;                                  // 0x0041a426
    CC(0x4628d0, FD(0x60), FD(0x64), 0, 0x8c, 0x8c, spr, 4, ebp);  // 0x0041a447
    goto L41a594;                                      // 0x0041a44f

L41a454:    // tool 1 (hole tee->green direction arrows): walk the 4 compass steps that canStep allows
    edi = G32(0x56a514) % 5;                           // 0x0041a454..0x0041a45f (idiv 5, remainder)
    esi = -1;                                          // 0x0041a461
    ebp = 0;                                           // 0x0041a464
    FD(0x18) = 0;                                      // 0x0041a466
L41a46c:
    eax = CC(0x407400, G32(0x543d08), G32(0x543d0c), 0);  // 0x0041a47a canStep
    if (eax == 0) goto L41a517;                        // 0x0041a482/0x0041a484
    esi = (FD(0x18) - G32(0x5685f4) + 3) & 7;          // 0x0041a48a..0x0041a49d direction 0..7
    {
        int dcell = (-2 - (esi / 2)) & 3;              // 0x0041a495/0x0041a4a0..0x0041a4af
        eax = CC(0x43d6f0, edi + 0x208, 0, dcell);     // 0x0041a4b6 cellAt
    }
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a4ca
    ebp = G32(0x4c2844);                               // 0x0041a4bb tile pixel pitch
    ecx = (edi + 0xaa) | 0x200;                        // 0x0041a4c1/0x0041a4c7
    {
        int ydelta = (5 * (G32(esi * 4 + 0x4c2898) * ebp)) / 2;   // 0x0041a4d3..0x0041a4f8
        int xdelta = (G32(esi * 4 + 0x4c2878) * ebp) * 4;         // 0x0041a4ee/0x0041a505
        CC(0x4628d0, FD(0x60) - xdelta, FD(0x64) - ydelta, FD(0x64), 0x28, 0x28, spr, 4, ecx);  // 0x0041a50b
    }
    ebp = FD(0x18);                                    // 0x0041a510
L41a517:
    ebp += 2;                                          // 0x0041a517
    FD(0x18) = ebp;                                    // 0x0041a51d
    if (ebp < 8) goto L41a46c;                         // 0x0041a51a/0x0041a521 (signed)
    if (esi != -1) goto L41a594;                       // 0x0041a527/0x0041a52a
    goto L41a58f;                                      // 0x0041a52c

L41a52e:    // tool 19
    {
        int e = G32(0x56a514) % 8;                     // 0x0041a52e..0x0041a53e (signed mod 8)
        eax = CC(0x43d6f0, e + 0x226, 0, 0);           // 0x0041a549 cellAt
    }
    spr = eax; G32(0x5a9cc8) = spr;                    // 0x0041a559
    CC(0x4628d0, FD(0x60), FD(0x64), FD(0x64), 0x28, 0x28, spr, 4, 0x2a9);  // 0x0041a56d
    if (GU8(G32(0x543d0c) + 50 * G32(0x543d08) + 0x5722e8) == 0x11) goto L41a594;  // 0x0041a585/0x0041a58d
L41a58f:
    ebx = 0x80007d08;                                  // 0x0041a58f
    // fall to L41a594

L41a594:    // common tail: tool name text + (tool 5) rating panel / (else) value text
    if (G32(0x4c2854) > 6) goto L41a5b8;               // 0x0041a594/0x0041a59b (signed jg)
    CC(0x404bc0, 0x51a068, FD(0x60), FD(0x64) - 8, ebx);   // 0x0041a5b0 drawCenteredText2(kBuf, x, y-8, colour)
L41a5b8:
    eax = FD(0x28);                                    // 0x0041a5c0
    if (G32(0x4c2854) != 5) goto L41a8ee;              // 0x0041a5b8/0x0041a5bd/0x0041a5c4
    if (eax == -1) goto L41a97e;                       // 0x0041a5ca/0x0041a5cd
    // --- tool 5: lot/course value panel ---
    CC(0x40cef0, 0x2a4, 0x17c, 0x78, 0x70, 1);         // 0x0041a5e3 drawFrameR3
    StrCpy(kBuf, 0x4c6188);                            // 0x0041a61b
    Itoa((CC(0x42ef40, G32(0x543d08), G32(0x543d0c)) / 4) * 100, 0x58a528, 0xa);  // 0x0041a625/0x0041a640 rateLot/4*100
    StrCat(kBuf, 0x58a528);                            // 0x0041a681
    CC(0x404bc0, 0x51a068, 0x2e0, 0x195, 0x800023e8);  // 0x0041a683
    StrCpy(kBuf, 0x4c6178);                            // 0x0041a6b9
    Itoa(FD(0x28) * 100, 0x58a528, 0xa);               // 0x0041a6c2
    StrCat(kBuf, 0x58a528);                            // 0x0041a703
    CC(0x404bc0, 0x51a068, 0x2e0, 0x1a4, 0x80007d08);  // 0x0041a705
    StrCpy(kBuf, 0x4c6168);                            // 0x0041a73e
    Itoa((int)G16(0x4c2726) * 100, 0x58a528, 0xa);     // 0x0041a747 ((s16)[0x4c2726]*100)
    StrCat(kBuf, 0x58a528);                            // 0x0041a786
    CC(0x404bc0, 0x51a068, 0x2e0, 0x1b3, 0x80007d08);  // 0x0041a78d
    TC(0x478b80, 0x519cd4, 0x2b0, 0x1c2, 0x310, 0x1c2, 0x80006318);  // 0x0041a7b3 Widget_fillRect
    {
        int rl = CC(0x42ef40, G32(0x543d08), G32(0x543d0c)) / 4;       // 0x0041a7c5 rateLot/4
        int val = (rl - (int)G16(0x4c2726) - FD(0x28)) * 100;          // 0x0041a7ca..0x0041a7f5
        StrCpy(kBuf, 0x4c615c);                        // 0x0041a812
        Itoa(val, 0x58a528, 0xa);                      // 0x0041a814
        StrCat(kBuf, 0x58a528);                        // 0x0041a855
        CC(0x404bc0, 0x51a068, 0x2e0, 0x1c7, 0x800023e8);  // 0x0041a857
        StrCpy(kBuf, 0x4c6158);                        // 0x0041a85c..0x0041a887
        Itoa(val, 0x58a528, 0xa);                      // 0x0041a889
        StrCat(kBuf, 0x58a528);                        // 0x0041a8c3
        {
            int col = (val <= 0) ? 0x80007d08 : 0x800023e8;  // 0x0041a8bd..0x0041a8d1
            CC(0x404bc0, 0x51a068, FD(0x60), FD(0x64) + 8, col);  // 0x0041a8e1
        }
    }
    goto L41a97e;                                      // 0x0041a8e9

L41a8ee:    // tool != 5 : "+$<value>" if positive
    if (FD(0x28) <= 0) goto L41a97e;                   // 0x0041a8ee/0x0041a8f0 (signed jle)
    StrCpy(kBuf, 0x4c52bc);                            // 0x0041a8f6..0x0041a927
    Itoa(FD(0x28) * 100, 0x58a528, 0xa);               // 0x0041a930
    StrCat(kBuf, 0x58a528);                            // 0x0041a969
    CC(0x404bc0, 0x51a068, FD(0x60), FD(0x64) + 8, 0x80007d08);  // 0x0041a976
    // fall to L41a97e

L41a97e:
    if (G32(0x567afc) != 0) goto L41b20f;              // 0x0041a97e/0x0041a985
    edi = 0x1e0;                                       // 0x0041a98b
L41a990:    // hovered-cell tooltip (idle preview)
    if (FD(0x58) >= edi) return;                        // 0x0041a990/0x0041a994 jge 0x41c0c5
    if (G32(0x55e924) != 0) return;                     // 0x0041a99a/0x0041a9a1
    if (G32(0x4c2848) == -1) return;                    // 0x0041a9a7/0x0041a9ae
    if ((GU16((G32(0x543d0c) + 50 * G32(0x543d08)) * 2 + 0x53caf0) & 0x4000) != 0) return;  // 0x0041a9b4..0x0041a9d9
    CC(0x42fb90, G32(0x543d08) << 10, G32(0x543d0c) << 10, FA(0x28c), FA(0x284), 0);  // 0x0041a9f9 worldToScreen
    CC(0x42f940, G32(0x543d08), G32(0x543d0c), FA(0x68), FA(0x58));   // 0x0041aa18 tileToScreen
    TC(0x4762d0, Root(), 0x51b360, 0, 0, 0);           // 0x0041aa31 setDrawTarget
    eax = G32(0x4c2848);                               // 0x0041aa36
    if (eax >= 0x80) goto L41b1d6;                      // 0x0041aa3b/0x0041aa40 (signed jge)
    if (eax != -2) goto L41aa8f;                        // 0x0041aa46/0x0041aa49
    {
        int lbl = CC(0x40a160, G32(0x543d08), G32(0x543d0c));  // 0x0041aa59 buildToolLabel -> kBuf
        int col = ((lbl != 0) ? (int)0xffffa3e9 : 0) + 0x80007fff;  // 0x0041aa5e..0x0041aa6e
        CC(0x404bc0, 0x51a068, FD(0x68) + 4, FD(0x58) - 0xc, col);  // 0x0041aa82 drawCenteredText2
    }
    goto L41b20f;                                      // 0x0041aa8a

L41aa8f:    // object/amenity placement preview sprites
    if (eax == 7) goto L41ab87;                         // 0x0041aa8f/0x0041aa92
    edi = FD(0x40);                                     // 0x0041aa98
    for (esi = 0; esi < 0x10; esi++) {                  // 0x0041aa9c..0x0041ab09
        edx = (int)G8(esi + 0x4c2d28);                  // 0x0041aa9e
        bool draw = (edx == eax);                       // 0x0041aaa5/0x0041aaa7
        if (!draw && GU8((eax + eax * 2) * 0x10 + 0x578376) == 0xd && esi == 7) draw = true;  // 0x0041aaa9..0x0041aabb
        if (draw) {
            int self = 0x59ca58 + 44 * (esi + 20 * (int)G8(0x5a34e0));  // 0x0041aae1..0x0041aaf4
            TC(0x474440, self, Root(), FD(0x68) - G32(0x4c2840), FD(0x58) - edi, 0x3f000000, 0, 1);  // 0x0041aafb
            eax = G32(0x4c2848);                        // 0x0041ab00
        }
    }
    if (eax != 1) goto L41ac73;                         // 0x0041ab0b/0x0041ab0e
    {
        int a = G32(0x5685f0);                          // 0x0041ab14
        if (G32(65 * a * 8 + 0x575ac8) != 0) goto L41ac73;  // 0x0041ab19..0x0041ab29
        if (a >= 0x13) goto L41ac73;                    // 0x0041ab2f/0x0041ab32
    }
    {
        int b = (int)G8(0x5a34e0);                      // 0x0041ab38
        int rem = G32(0x834170) % G32(b * 4 + 0x53fa0c);  // 0x0041ab3f..0x0041ab47
        eax = CC(0x43d6f0, b + 0x189, rem, 1);          // 0x0041ab56 cellAt
    }
    spr = eax; G32(0x5a9cc8) = spr;                     // 0x0041ab66
    CC(0x4628d0, FD(0x68) - 4, FD(0x58), FD(0x58), 0x28, 0x28, spr, 4, 0x263);  // 0x0041ab7a
    goto L41ac73;                                       // 0x0041ab82

L41ab87:    // tool/object 7: hole flag + pin preview (4-direction nib byte table at F[0xf0..0xf3])
    {
        int d = G32(0x5a34f0) - G32(0x5685f4) / 2 + 4;  // 0x0041ab87..0x0041aba9
        FB(0xf0) = 0; FB(0xf1) = 3; FB(0xf2) = 1; FB(0xf3) = 2;  // 0x0041ab95/0x0041aba1/0x0041abac/0x0041abb6
        int e = d % 4;                                  // 0x0041abbe..0x0041abc9 (signed %4)
        int self;
        if (e != 0) {                                   // 0x0041abca..0x0041abe2 (edx=e%4==e; jne)
            int cmod = ((int)FB(0xf0 + e)) % 4;         // 0x0041ac30 (index e may be negative: FINDING); 0x0041ac44..0x0041ac53
            self = 0x59cd18 + 44 * (cmod + 20 * (int)G8(0x5a34e0));  // 0x0041ac54..0x0041ac67
        } else {
            self = 0x59cab0 + 880 * (int)G8(0x5a34e0);  // 0x0041abf9..0x0041ac16
        }
        TC(0x474440, self, Root(), FD(0x68) - G32(0x4c2840), FD(0x58) - FD(0x40), 0x3f000000, 0, 1);  // 0x0041ac6e
    }
    // fall to L41ac73

L41ac73:    // object info panel (name + optional par/direction label)
    TC(0x4762d0, Root(), 0x519fd8, 0, 0, 0);           // 0x0041ac84 setDrawTarget
    {
        int obj = G32(0x4c2848);                        // 0x0041ac89
        edx = 48 * obj;                                 // 0x0041ac8f..0x0041ac97 (obj*3 <<4)
        StrCpy(kBuf, 0x578350 + edx);                   // 0x0041ac9a..0x0041acb9 object name
        GU8(0x51a068) = (u8)(GU8(0x51a068) + 0xe0);     // 0x0041acbb..0x0041acc9 uppercase first letter
        if (obj == 1 && (GU8(0x5a34f0) & 1) != 0) {     // 0x0041acc4..0x0041acd5 (test [0x5a34f0], bl=1)
            StrCpy(kBuf, 0x4c6148);                     // 0x0041acd7..0x0041acfa
        }
        if (GU8(edx + 0x578376) != 0xd) goto L41aea8;   // 0x0041acfc/0x0041ad03
    }
    {
        int tcell = (int)G8(G32(0x543d0c) + 50 * G32(0x543d08) + 0x5722e8);  // 0x0041ad09..0x0041ad1b
        if (GU8(48 * tcell + 0x578376) == 0xd) goto L41aea8;  // 0x0041ad23..0x0041ad30
    }
    ebx = G32(0x4c2848);                                // 0x0041ad51/0x0041ad9? (ebx = object id)
    if (ebx == 0xd) {                                   // 0x0041ad36/0x0041ad39
        int h = CC(0x42dba0, G32(0x543d08) << 8, G32(0x543d0c) << 8);  // 0x0041ad48 sampleHeight
        esi = ((h / 32) & 3) + 0x25;                    // 0x0041ad4d..0x0041ad6f
        FD(0xac) = esi;                                 // 0x0041ad72
        edi = 0x19b;                                    // 0x0041ad43
    } else {
        esi = FD(0xac);                                 // 0x0041ad7b (read-before-write possible)
        edi = FD(0x180);                                // 0x0041ad82 (read-before-write: FINDING)
    }
L41ad89:
    if (ebx == 0xf) {                                   // 0x0041ad89/0x0041ad8c
        int h = CC(0x42dba0, G32(0x543d08) << 7, G32(0x543d0c) << 7);  // 0x0041ad9b sampleHeight
        esi = ((h / 32) & 3) + 0x32;                    // 0x0041ada0..0x0041adbd
        FD(0xac) = esi;                                 // 0x0041adc0
        edi = 0x195;                                    // 0x0041ad96
        ebx = G32(0x4c2848);                            // 0x0041ada4
    }
L41adc7:
    if (ebx == 0xe) {                                   // 0x0041adc7/0x0041adca
        esi = 0x29;                                     // 0x0041add1
        FD(0xac) = 0x29;                                // 0x0041ade8
        edi = 0x192;                                    // 0x0041adde
        if (((G32(0x5a34f0) / 3) & 1) != 0) {           // 0x0041adcc..0x0041adf2 (imul-magic /3; test dl,1)
            edi = 0x198;                                // 0x0041adf9
            s8 t = G8(0x5a34e0);                        // 0x0041adf4
            if (t == 2 || t == 1 || t == 3) {           // 0x0041adfe..0x0041ae08
                esi = 0x2a; FD(0xac) = 0x2a;            // 0x0041ae0a/0x0041ae0f
            }
        }
    }
L41ae16:
    edi = edi + (G32(0x5a34f0) % 3);                    // 0x0041ae16..0x0041ae23 (idiv 3, add remainder)
    FD(0x180) = edi;                                    // 0x0041ae28
    if (ebx == 0x10) {                                  // 0x0041ae25/0x0041ae2f
        edi = 0xf9; esi = 0x36;                         // 0x0041ae31/0x0041ae36
        FD(0x180) = 0xf9; FD(0xac) = 0x36;              // 0x0041ae3b/0x0041ae42
    }
    {
        int ix = ((G32(0x5685f4) / 2) - 2 * G32(0x543d0c) + G32(0x543d08)) & 3;  // 0x0041ae49..0x0041ae67
        eax = CC(0x43d6f0, edi, G32(edi * 4 + 0x53f3e8) - 1, ix);  // 0x0041ae6e cellAt
    }
    spr = eax; G32(0x5a9cc8) = spr;                     // 0x0041ae79
    esi = esi | 0x200;                                  // 0x0041ae73
    CC(0x4628d0, FD(0x68), FD(0x58), FD(0x58), 0x8c, 0x8c, spr, 4, esi);  // 0x0041ae9a
    ebx = G32(0x4c2848);                                // 0x0041ae9f
L41aea8:
    if (ebx == 1 && (GU8(0x5a34f0) & 1) != 0) {         // 0x0041aea8..0x0041aeb3
        CC(0x404bc0, 0x51a068, FD(0x58) - 0x10, FD(0x68) + 4, 0x80007fff);  // 0x0041aecf
        ebx = G32(0x4c2848);                            // 0x0041aed4
    }
L41aedd:
    FD(0xc0) = -1;                                      // 0x0041aee0
    if (ebx != 1) goto L41b062;                         // 0x0041aedd/0x0041aeeb
    {
        int o = 520 * G32(0x5685f0);                    // 0x0041aef1..0x0041aefe
        if (G32(o + 0x575ac8) != 0) goto L41b20f;       // 0x0041af01/0x0041af09
        int base = G32(o + 0x575ab8);                   // 0x0041af0f
        if (base == 0) goto L41b20f;                    // 0x0041af15/0x0041af17
        if (G32(0x5685f0) >= 0x13) goto L41b20f;        // 0x0041af1d/0x0041af20
        eax = CC(0x42f940, base, G32(o + 0x575abc), FA(0xc0), FA(0xd4));  // 0x0041af3e tileToScreen
    }
    if (eax == 0) goto L41b05c;                         // 0x0041af46/0x0041af48
    CC(0x406220, FD(0xc0), FD(0xd4), FD(0x68), FD(0x58), 0x80007fff, 2, 7);  // 0x0041af71
    {
        int o = 520 * G32(0x5685f0);                    // 0x0041af76..0x0041af83
        int dx = 25 * (G32(0x543d08) - G32(o + 0x575ab8));  // 0x0041afa0..0x0041afae
        int dy = 25 * (G32(0x543d0c) - G32(o + 0x575abc));  // 0x0041af83..0x0041af9e
        int dist = CC(0x40acd0, dx, dy);                // 0x0041afb0 distance
        GU8(0x51a068) = 0;                              // 0x0041afbd
        Itoa(dist, 0x58a528, 0xa);                      // 0x0041afc4
        StrCat(kBuf, 0x58a528);                         // 0x0041aff6
        StrCat(kBuf, 0x4c501c);                         // 0x0041b028
        CC(0x404b70, 0x51a068, (FD(0xc0) + FD(0x68)) / 2, (FD(0x58) + FD(0xd4)) / 2, 0x80007fff);  // 0x0041b054
    }
L41b05c:
    ebx = G32(0x4c2848);                                // 0x0041b05c
L41b062:
    if (ebx != 0) goto L41b20f;                         // 0x0041b062/0x0041b064
    {
        int o = 520 * G32(0x5685f0);                    // 0x0041b06a..0x0041b077
        if (G32(o + 0x575ab8) != 0) goto L41b20f;       // 0x0041b07a/0x0041b082
        int base = G32(o + 0x575ac8);                   // 0x0041b088
        if (base == 0) goto L41b20f;                    // 0x0041b08e/0x0041b090
        if (G32(0x5685f0) >= 0x13) goto L41b20f;        // 0x0041b096/0x0041b099
        eax = CC(0x42f940, base, G32(o + 0x575acc), FA(0xc0), FA(0xd4));  // 0x0041b0b7 tileToScreen
    }
    if (eax == 0) goto L41b20f;                         // 0x0041b0bf/0x0041b0c1
    CC(0x406220, FD(0xc0), FD(0xd4), FD(0x68), FD(0x58), 0x80007fff, 2, 7);  // 0x0041b0ea
    {
        int o = 520 * G32(0x5685f0);                    // 0x0041b0ef..0x0041b102
        int dx = 25 * (G32(0x543d08) - G32(o + 0x575ac8));  // 0x0041b11f..0x0041b128
        int dy = 25 * (G32(0x543d0c) - G32(o + 0x575acc));  // 0x0041b102..0x0041b116
        int dist = CC(0x40acd0, dx, dy);                // 0x0041b129 distance
        GU8(0x51a068) = 0;                              // 0x0041b136 (bl=0)
        Itoa(dist, 0x58a528, 0xa);                      // 0x0041b13c
        StrCat(kBuf, 0x58a528);                         // 0x0041b16e
        StrCat(kBuf, 0x4c501c);                         // 0x0041b1a0
        CC(0x404b70, 0x51a068, (FD(0xc0) + FD(0x68)) / 2, (FD(0x58) + FD(0xd4)) / 2, 0x80007fff);  // 0x0041b1cc
    }
    goto L41b20f;                                       // 0x0041b1d4

L41b1d6:    // [0x4c2848] >= 0x80
    if (eax != 0xa0) goto L41b204;                      // 0x0041b1d6/0x0041b1db
    CC(0x404bc0, 0x4c6140, FD(0x58) - 8, FD(0x68) + 4, 0x80007fff);  // 0x0041b1f7
    eax = G32(0x4c2848);                                // 0x0041b1fc
L41b204:
    if (eax == 0x280) P7_endless(F);                    // 0x0041b204/0x0041b209 (never returns)
    // fall to L41b20f

L41b20f:    // ==== drag preview (course-edit / hole / path drag, [0x567afc]==3) ====
    if (G32(0x567afc) != 3) return;                     // 0x0041b20f/0x0041b216
    if (FD(0x58) >= 0x1e0) return;                      // 0x0041b21c/0x0041b224 (signed)
    G32(0x4c2848) = -1;                                 // 0x0041b231
    G32(0x4c2854) = -1;                                 // 0x0041b236
    G32(0x4c2e10) = -1;                                 // 0x0041b23b
    G32(0x53df54) = 0;                                  // 0x0041b24d (edi=0)
    TC(0x47ab50, 0x519a60, FA(0x68), FA(0x58));         // 0x0041b253 View_toLocal
    CC(0x42fb90, (G32(0x543d08) << 10) + 0x200, (G32(0x543d0c) << 10) + 0x200, FA(0x5c), FA(0x54), 0);  // 0x0041b283 worldToScreen
    ebp = CC(0x40acd0, FD(0x68) - FD(0x5c), FD(0x58) - FD(0x54));   // 0x0041b2a1 distance (best so far)
    ecx = G32(0x543d08);                                // 0x0041b2ad
    edx = G32(0x543d0c);                                // 0x0041b2b3
    ebx = ecx;                                          // 0x0041b2bf base x
    FD(0x34) = edx;                                     // 0x0041b2d3 base y
    {
        int rec = (G32(0x5a59f8) << 8) + 0x5794c8;      // 0x0041b2a8..0x0041b2d7
        G32(rec) &= 0xefffffff;                         // clear bit 28
    }
    edi = 0;                                            // 0x0041b245
L41b2d9:                                                 // outer loop over edi (0..1)
    esi = 0;                                            // 0x0041b2d9
L41b2db:                                                 // inner loop over esi (0..1)
    CC(0x42fb90, (ecx + edi) << 10, (edx + esi) << 10, FA(0x5c), FA(0x54), 0);  // 0x0041b2f3 worldToScreen
    eax = CC(0x40acd0, FD(0x58) - FD(0x54), FD(0x68) - FD(0x5c));   // 0x0041b311 distance
    if (eax >= ebp) goto L41b34c;                       // 0x0041b316/0x0041b31b (signed)
    ebp = eax;                                          // 0x0041b31d
    {
        int nx = edi + ebx;                             // 0x0041b323
        int ny = esi + FD(0x34);                        // 0x0041b326
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b329
        GU8(rec + 0x5794cb) = (u8)(GU8(rec + 0x5794cb) | 0x10);  // 0x0041b331 moved flag
        G32(0x543d08) = nx; ecx = nx;                   // 0x0041b338
        G32(0x543d0c) = ny; edx = ny;                   // 0x0041b344
    }
    goto L41b358;                                       // 0x0041b34a
L41b34c:
    ecx = G32(0x543d08);                                // 0x0041b34c
    edx = G32(0x543d0c);                                // 0x0041b352
L41b358:
    esi++;                                              // 0x0041b358
    if (esi < 2) goto L41b2db;                          // 0x0041b359/0x0041b35c
    edi++;                                              // 0x0041b362
    if (edi < 2) goto L41b2d9;                          // 0x0041b363/0x0041b366
    // anchor screen point for the drag start
    {
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b36c
        edi = ecx;                                      // 0x0041b371
        ebx = edx;                                      // 0x0041b382/0x0041b38c
        edi <<= 10; ebx <<= 10;                         // 0x0041b384/0x0041b387 / 0x0041b38e/0x0041b391
        if ((G32(rec + 0x5794c8) & 0x10000000) == 0) {  // 0x0041b376/0x0041b380
            edi += 0x200; ebx += 0x200;                 // 0x0041b394/0x0041b39a
        }
        G32(rec + 0x57957c) = ecx;                      // 0x0041b3a0
        G32(rec + 0x579580) = edx;                      // 0x0041b3ad
    }
    esi = CC(0x422530, G32(0x5a59f8));                  // 0x0041b3b3 rateTile (esi = drag length / quality)
    {
        int mode = G32(0x58f330);                       // 0x0041b3ba
        if (mode == 3) {                                // 0x0041b3c2/0x0041b3c5
            int e13 = 13 * esi;                         // 0x0041b3c7..0x0041b3cf
            long long prod = (long long)(int)0x38e38e39 * e13;  // 0x0041b3d2 imul
            int hi = (int)((unsigned long long)prod >> 32);
            int q = (hi >> 2);                          // 0x0041b3d4 sar edx,2
            q = q + (int)((unsigned)q >> 31);           // 0x0041b3d9/0x0041b3dc
            esi = q;                                     // 0x0041b3de
            goto Lrate_done;                            // 0x0041b3e0
        } else if (mode == 4) {                         // 0x0041b3e2/0x0041b3e5
            esi = esi / 2;                              // 0x0041b3e7..0x0041b3ec (FINDING: halved)
            goto L41b44a;                               // 0x0041b3f0
        } else if (mode == 0) {                         // 0x0041b3f2/0x0041b3f4
            goto L41b44a;
        }
    }
Lrate_done:                                              // 0x0041b3f6
    {
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b3f6
        int cx = G32(rec + 0x57958c) >> 10;             // 0x0041b3fe/0x0041b40a (signed >>)
        int cy = G32(rec + 0x579590) >> 10;             // 0x0041b404/0x0041b40d
        int t = (int)G8(cy + 50 * cx + 0x5722e8);       // 0x0041b410..0x0041b416
        if (G8(48 * t + 0x578372) > 0) {                // 0x0041b41e..0x0041b42c (test cl,cl; jle, signed)
            G32(0x58f330) = 0;                          // 0x0041b438
            CC(0x4481b0, 0x18, 0x64, 0, 0, 0);          // 0x0041b442 playSound
        }
    }
L41b44a:
    {
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b44a
        FD(0x5c) = G32(rec + 0x5794c0);                 // 0x0041b454/0x0041b460
        FD(0x54) = G32(rec + 0x5794c4);                 // 0x0041b45a/0x0041b464
        CC(0x42fb90, G32(rec + 0x57958c), G32(rec + 0x579590), FA(0x5c), FA(0x54), 0);  // 0x0041b480 worldToScreen
    }
    {
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b485
        int d2 = CC(0x40acd0, G32(rec + 0x57958c) - edi, G32(rec + 0x579590) - ebx);  // 0x0041b49f distance
        FD(0x34) = (25 * d2) / 1024;                    // 0x0041b4a4..0x0041b4bc (signed /1024)
    }
    if (G32(0x5a34e4) != 0) G32(0x5a34e4) -= 1;         // 0x0041b4bf..0x0041b4c8
    if (FD(0x5c) == -1) {                               // 0x0041b4ce/0x0041b4d3
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b4d5
        int a = (int)G8(rec + 0x5794d9);                // 0x0041b4de
        int o = 520 * a;                                // 0x0041b4e5..0x0041b4f2
        int hx = G32(rec + 0x5794b8) >> 10;             // 0x0041b4ec/0x0041b4f5
        G32(0x4c2ba0) = (G32(o + 0x575ac8) + 2 * hx) / 3;   // 0x0041b4f8..0x0041b515 (0x55555556 magic /3)
        int hy = G32(rec + 0x5794bc) >> 10;             // 0x0041b51b/0x0041b521
        G32(0x4c2ba4) = (G32(o + 0x575acc) + 2 * hy) / 3;   // 0x0041b50f..0x0041b539
    }
    ebp = CC(0x467130, FD(0x34), 0, esi);               // 0x0041b543 clamp([F+0x34], 0, esi)
    {
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b54a
        edi = edi - G32(rec + 0x57958c);                // 0x0041b559/0x0041b55f
        ebx = ebx - G32(rec + 0x579590);                // 0x0041b552
        int ang = CC(0x4672d0, edi, ebx);               // 0x0041b562 angleFixed
        FD(0x34) = ang;                                 // 0x0041b569
        GU8(rec + 0x5794d2) = (u8)(((((ang >> 28) & 0xf) + 1) >> 1) & 7);  // 0x0041b571..0x0041b588 compass dir
    }
    edi = G32(0x58f330);                                // 0x0041b574
    {
        int ae = (edi < 0) ? -edi : edi;                // 0x0041b58e..0x0041b593 abs
        FD(0x30) = (ae <= 1) ? (2 * edi) : 0;           // 0x0041b595..0x0041b5a7
    }
    {
        int seg = (edi == 4) ? 0 : ((edi == 3) ? 3 : 2);  // 0x0041b5be..0x0041b5ce
        FD(0x28) = ((edi != 3) ? 4 : 0) + 0x10;         // 0x0041b5ab..0x0041b5ba
        int pitch = G32(0x4c2844);                      // 0x0041b5d0
        ebx = 2;                                        // 0x0041b5d6
        FD(0x40) = pitch * seg;                         // 0x0041b5dd/0x0041b5e5
        if (G32(0x5a9cbc) != 0) ebx = 3;                // 0x0041b5e0..0x0041b5ed
        if (pitch < 4) ebx = 1;                         // 0x0041b5f2/0x0041b5f5/0x0041b5f7
    }
    {
        int rec = (G32(0x5a59f8) << 8);                 // ecx still = rec (0x0041b567/0x0041b580)
        int flag = (GU8(rec + 0x5794da) != 0) ? 1 : 0;  // 0x0041b5fc..0x0041b606
        int power = CC(0x467130, (60 * (esi - ebp)) / (3 * esi), flag, 0xb);  // 0x0041b60c..0x0041b620 clamp (FINDING: /0 if esi==0)
        GU8(rec + 0x5794d4) = (u8)power;                // 0x0041b637
        StrCpy(kBuf, 0x4c6138);                         // 0x0041b62f..0x0041b659
        CC(0x40a9a0, power);                            // 0x0041b65b appendClubName -> kBuf
    }
    // === flag / club label text ===
    TC(0x4762d0, Root(), 0x51b360, 0, 0, 0);            // 0x0041b674 setDrawTarget
    CC(0x404b70, 0x51a068, 0x196, 0x22c, 0x80000018);   // 0x0041b68d drawCenteredText (club name)
    StrCpy(kBuf, 0x4c612c);                             // 0x0041b692..0x0041b6c1
    {
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b6a4
        int ci = CC(0x467130, (int)G8(rec + 0x5794ee), -4, 4) + 4;  // 0x0041b6c3/0x0041b6cb clamp; +4
        u32 suf;
        if ((unsigned)ci > 8) goto L41b740;             // 0x0041b6d6/0x0041b6d9 (unsigned ja)
        switch (ci) {                                   // 0x0041b6db jmp [ci*4 + 0x4217f4]
            case 0:  suf = 0x4c60e8; break;
            case 1:  suf = 0x4c60f0; break;
            case 2:  suf = 0x4c60f4; break;
            case 3:  suf = 0x4c60fc; break;
            case 4:  suf = 0x4c6124; break;
            case 5:  suf = 0x4c6118; break;
            case 6:  suf = 0x4c6110; break;
            case 7:  case 8: suf = 0x4c6104; break;
            default: suf = 0x4c60e8; break;
        }
        StrCat(kBuf, suf);                              // 0x0041b718..0x0041b73e
    }
L41b740:
    CC(0x404b70, 0x51a068, 0x196, 0x21e, 0x80000018);   // 0x0041b754 drawCenteredText (distance line)
    StrCpy(kBuf, 0x4c60e0);                             // 0x0041b759..0x0041b784
    {
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b779
        int ty = CC(0x40bfa0, G32(rec + 0x57958c), G32(rec + 0x579590));  // 0x0041b794 typeAtPos
        StrCat(kBuf, 0x578350 + 48 * ty);               // 0x0041b799..0x0041b7da terrain type name
    }
    CC(0x404b70, 0x51a068, 0x196, 0x248, 0x80000018);   // 0x0041b7e1 drawCenteredText (lie line)
    StrCpy(kBuf, 0x4c60d4);                             // 0x0041b7e6..0x0041b814
    {
        int v = 25 * ebp;                               // 0x0041b80d..0x0041b819
        long long prod = (long long)(int)0x51eb851f * v;   // 0x0041b81e imul
        int hi = (int)((unsigned long long)prod >> 32);
        int q = hi >> 3;                                // 0x0041b820 sar edx,3
        q = q + (int)((unsigned)q >> 31);               // 0x0041b823/0x0041b828
        Itoa(q, 0x58a528, 0xa);                         // 0x0041b82b
    }
    StrCat(kBuf, 0x58a528);                             // 0x0041b867
    StrCat(kBuf, 0x4c60cc);                             // 0x0041b897
    CC(0x404b70, 0x51a068, 0x196, 0x23a, 0x80000018);   // 0x0041b899 drawCenteredText (% line)
    TC(0x4762d0, Root(), 0x519fd8, 0, 0, 0);            // 0x0041b8b2 setDrawTarget
    FD(0x2c) = 0x1ee;                                   // 0x0041b8b7
    FD(0x10) = 0x21c;                                   // 0x0041b8bf
    FD(0xc) = 0;                                        // 0x0041b8c7
    FD(0x20) = 0x4c2c3c;                                // 0x0041b8cf
L41b8d7:    // per-club-stat row loop (10 rows); [F+0xc]=index, [F+0x20]=label table ptr
    {
        int bit = FD(0xc);                              // 0x0041b8d7 (ecx)
        int rec = (G32(0x5a59f8) << 8);                 // 0x0041b8e3
        if (GU8(bit + 0x5a5a04) != 0)                   // 0x0041b8db/0x0041b8e1
            GU16(rec + 0x5794ce) = (u16)(GU16(rec + 0x5794ce) | (1 << bit));   // 0x0041b8ea..0x0041b8fa
        else
            GU16(rec + 0x5794ce) = (u16)(GU16(rec + 0x5794ce) & ~(1 << bit));  // 0x0041b8ff..0x0041b911
        FD(0x18) = (1 << bit);                          // 0x0041b914/0x0041b917/0x0041b920
        if (((int)G16(rec + 0x5794ce) & (1 << bit)) == 0) goto L41baf4;  // 0x0041b91e/0x0041b924
    }
    {
        int rec = (G32(0x5a59f8) << 8);
        StrCpy(kBuf, GU32(FD(0x20)));                   // 0x0041b937..0x0041b955 strcpy(kBuf, *[F+0x20])
        StrCat(kBuf, 0x4c60c8);                         // 0x0041b957..0x0041b98b
        Itoa(10 * (int)G8(FD(0xc) + 0x5a5a04), 0x58a528, 0xa);  // 0x0041b97b..0x0041b993
        StrCat(kBuf, 0x58a528);                         // 0x0041b9c3
        StrCat(kBuf, 0x4c60c4);                         // 0x0041b9f2
        int bit = FD(0x18);                             // 0x0041b9e9
        esi = 0;                                        // 0x0041b9f4
        if (bit == 1) {                                 // table idx0 -> 0x0041baa8
            esi = 1;
        } else if (bit == 2 || bit == 4) {              // 0x0041ba11
            if ((int)G8(rec + 0x5794d4) <= 3 && GU8(rec + 0x5794da) == 0) esi = 1;  // 0x0041ba19..0x0041ba2a
        } else if (bit == 8) {                          // 0x0041ba30
            esi = ((int)G8(rec + 0x5794d4) > 3) ? 1 : 0;   // 0x0041ba3a/0x0041ba41
        } else if (bit == 0x20) {                       // 0x0041ba48
            if (G32(0x58f330) == 1) esi = 1;            // 0x0041ba48/0x0041ba4f
        } else if (bit == 0x40) {                       // 0x0041ba9f
            if (G32(0x58f330) == -1) esi = 1;           // 0x0041ba9f/0x0041baa6
        } else if (bit == 0x80) {                       // 0x0041ba94
            if (G32(0x58f330) == 3) esi = 1;            // 0x0041ba94/0x0041ba9b
        } else if (bit == 0x100) {                      // 0x0041ba64
            int ty = CC(0x40bfa0, G32(rec + 0x57958c), G32(rec + 0x579590));  // 0x0041ba7a typeAtPos
            if (G8(48 * ty + 0x578372) > 0) esi = 1;    // 0x0041ba85..0x0041ba92
        }
        // bit == 0x10 or 0x200: esi stays 0
        int col = ((esi != 0) ? 0x3def : 0) + 0x80004210;  // 0x0041baad..0x0041babb
        CC(0x4049d0, 0x51a068, FD(0x2c), FD(0x10), col);   // 0x0041bacd drawText4049d0
        int y = FD(0x10) + 0xc;                         // 0x0041bad2
        FD(0x10) = y;                                   // 0x0041bade
        if (y >= 0x258) {                               // 0x0041bad8 (signed)
            FD(0x2c) = 0x281;                           // 0x0041bae4
            FD(0x10) = 0x21c;                           // 0x0041baec
        }
    }
L41baf4:
    FD(0x20) = FD(0x20) + 4;                            // 0x0041baf4/0x0041bb09
    FD(0xc) = FD(0xc) + 1;                              // 0x0041bafc/0x0041bb05
    if (FD(0x20) < 0x4c2c64) goto L41b8d7;              // 0x0041bb00/0x0041bb0d (signed)

    // === swing-arc box: 6 line segments around the drag, with foldRange/absDiff fixed-point ===
    { int x0 = FD(0x5c), y0 = FD(0x54); CC(0x40bf00, x0, y0); FD(0x20) = x0; FD(0x10) = y0; }  // 0x0041bb13..0x0041bb25 setLinePoint
    edi = FD(0x34);                                     // 0x0041bb3d (edi = ang, constant through the box)
    esi = FD(0x30) * 0x0aaaaaaa;                        // 0x0041bb2e/0x0041bb33
    // -- segment A --
    {
        int rec = (G32(0x5a59f8) << 8);
        int m = P7_m51(((5 * ebp) / FD(0x28)) << 10);   // 0x0041bb2a..0x0041bb55
        int p = edi + esi;                              // 0x0041bb57
        FD(0x2c) = m; FD(0x34) = p;                     // 0x0041bb5c/0x0041bb60
        int fr = CC(0x467270, p, m);                    // 0x0041bb64 foldRange
        FD(0xc) = fr + G32(rec + 0x57958c);             // 0x0041bb69..0x0041bb7e
        int ad = CC(0x4672b0, FD(0x34), FD(0x2c));      // 0x0041bb88 absDiff(p,m)
        CC(0x42fb90, FD(0xc), G32(rec + 0x579590) - ad, FA(0x5c), FA(0x54), 0);  // 0x0041bbb3 worldToScreen
        CC(0x406220, FD(0x20), FD(0x10) + 1, FD(0x5c), FD(0x54) + 1, 0x80000000, ebx, 7);  // 0x0041bbdc
    }
    // -- line A --
    {
        int d = FD(0x40) * ebp;                         // 0x0041bbe1/0x0041bbef
        FD(0x20) = FD(0x5c);                            // 0x0041bbf2
        FD(0x18) = d;                                   // 0x0041bbfb
        FD(0x10) = FD(0x54);                            // 0x0041bc22
        int delta = (d / 3) / 8;                        // 0x0041bbf6..0x0041bc2f (0x55555556 /3 then sar3 /8)
        FD(0x40) = delta;                               // 0x0041bc32
        int ny = FD(0x54) - delta;                      // 0x0041bc36
        CC(0x40bf20, FD(0x5c), ny, 0x80007fff, ebx, 7); // 0x0041bc47 lineTo
        FD(0x54) = ny;                                  // 0x0041bc43
    }
    // -- segment B --
    {
        int rec = (G32(0x5a59f8) << 8);
        int m = P7_m51(((9 * ebp) / FD(0x28)) << 10);   // 0x0041bc4c..0x0041bc6d
        int t = (int)((u32)FD(0x30) << 29) / 4 + edi;   // 0x0041bc61..0x0041bc82
        FD(0x34) = m; FD(0x2c) = t;                     // 0x0041bc7b/0x0041bc85
        int fr = CC(0x467270, t, m);                    // 0x0041bc89 foldRange
        FD(0xc) = fr + G32(rec + 0x57958c);             // 0x0041bc8e..0x0041bc9d
        int ad = CC(0x4672b0, FD(0x2c), FD(0x34));      // 0x0041bcad absDiff(t,m)
        CC(0x42fb90, FD(0xc), G32(rec + 0x579590) - ad, FA(0x5c), FA(0x54), 0);  // 0x0041bcdb
        CC(0x406220, FD(0x20), FD(0x10) + 1, FD(0x5c), FD(0x54) + 1, 0x80000000, ebx, 7);  // 0x0041bd04
    }
    // -- line B --
    {
        FD(0x20) = FD(0x5c);                            // 0x0041bd1b
        FD(0x10) = FD(0x54);                            // 0x0041bd22
        int delta = (FD(0x18) / 2) / 8;                 // 0x0041bd17..0x0041bd39
        FD(0x18) = delta;                               // 0x0041bd3c
        int ny = FD(0x54) - delta;                      // 0x0041bd40
        CC(0x40bf20, FD(0x5c), ny, 0x80007fff, ebx, 7); // 0x0041bd4c lineTo
        FD(0x54) = ny;                                  // 0x0041bd48
    }
    // -- segment C --
    {
        int rec = (G32(0x5a59f8) << 8);
        int m = P7_m51(((12 * ebp) / FD(0x28)) << 10);  // 0x0041bd51..0x0041bd73
        int t = esi / 2 + edi;                          // 0x0041bd6e..0x0041bd7d
        FD(0x34) = m; FD(0x2c) = t;                     // 0x0041bd7f/0x0041bd84
        int fr = CC(0x467270, t, m);                    // 0x0041bd88 foldRange
        FD(0xc) = fr + G32(rec + 0x57958c);             // 0x0041bd8d..0x0041bd9c
        int ad = CC(0x4672b0, FD(0x2c), FD(0x34));      // 0x0041bdac absDiff(t,m)
        CC(0x42fb90, FD(0xc), G32(rec + 0x579590) - ad, FA(0x5c), FA(0x54), 0);  // 0x0041bdda
        CC(0x406220, FD(0x20), FD(0x10) + 1, FD(0x5c), FD(0x54) + 1, 0x80000000, ebx, 7);  // 0x0041be03
    }
    // -- line C --
    {
        FD(0x20) = FD(0x5c);                            // 0x0041be19
        FD(0x10) = FD(0x54);                            // 0x0041be1d
        int ny = FD(0x54) - FD(0x18);                   // 0x0041be21/0x0041be27 (delta = [F+0x18])
        CC(0x40bf20, FD(0x5c), ny, 0x80007fff, ebx, 7); // 0x0041be35 lineTo
        FD(0x54) = ny;                                  // 0x0041be31
    }
    // -- segment D --
    {
        int rec = (G32(0x5a59f8) << 8);
        int m = P7_m51(((15 * ebp) / FD(0x28)) << 10);  // 0x0041be3a..0x0041be5a
        int t = esi / 3 + edi;                          // 0x0041be5c..0x0041be71 (0x55555556 /3)
        FD(0x34) = m; FD(0x2c) = t;                     // 0x0041be6d/0x0041be74
        int fr = CC(0x467270, t, m);                    // 0x0041be78 foldRange
        FD(0xc) = fr + G32(rec + 0x57958c);             // 0x0041be7d..0x0041be8c
        int ad = CC(0x4672b0, FD(0x2c), FD(0x34));      // 0x0041be9c absDiff(t,m)
        CC(0x42fb90, FD(0xc), G32(rec + 0x579590) - ad, FA(0x5c), FA(0x54), 0);  // 0x0041bec7
        CC(0x406220, FD(0x20), FD(0x10) + 1, FD(0x5c), FD(0x54) + 1, 0x80000000, ebx, 7);  // 0x0041bef0
    }
    // -- line D --
    {
        FD(0x10) = FD(0x54);                            // 0x0041bf0d
        FD(0x20) = FD(0x5c);                            // 0x0041bf16
        int ny = FD(0x54) - FD(0x40);                   // 0x0041bf11 (delta = [F+0x40])
        CC(0x40bf20, FD(0x5c), ny, 0x80007fff, ebx, 7); // 0x0041bf25 lineTo
        FD(0x54) = ny;                                  // 0x0041bf1a
    }
    // -- segment E -- (x arg comes through esi here)
    {
        int rec = (G32(0x5a59f8) << 8);
        int m = P7_m51(((16 * ebp) / FD(0x28)) << 10);  // 0x0041bf2a..0x0041bf48
        int t = esi / 4 + edi;                          // 0x0041bf4a..0x0041bf5c
        FD(0x34) = m; FD(0x2c) = t;                     // 0x0041bf55/0x0041bf5f
        int fr = CC(0x467270, t, m);                    // 0x0041bf63 foldRange
        esi = fr + G32(rec + 0x57958c);                 // 0x0041bf68..0x0041bf7d (into esi, not [F+0xc])
        int ad = CC(0x4672b0, FD(0x2c), FD(0x34));      // 0x0041bf85 absDiff(t,m)
        CC(0x42fb90, esi, G32(rec + 0x579590) - ad, FA(0x5c), FA(0x54), 0);  // 0x0041bfac
        CC(0x406220, FD(0x20), FD(0x10) + 1, FD(0x5c), FD(0x54) + 1, 0x80000000, ebx, 7);  // 0x0041bfd5
    }
    // -- line E -- (no delta, no [F+0x54]/[F+0x20] update; esi carries x for the final segment)
    esi = FD(0x5c);                                     // 0x0041bfe1/0x0041bfeb
    FD(0x10) = FD(0x54);                                // 0x0041bfed
    CC(0x40bf20, FD(0x5c), FD(0x54), 0x80007fff, ebx, 7);  // 0x0041bffb lineTo
    if (G32(0x58f330) == 3) return;                     // 0x0041c000/0x0041c00b je 0x41c0c5
    // -- segment F -- (ebp reassigned; drawMinimapSegment x0 = esi)
    {
        int rec = (G32(0x5a59f8) << 8);
        int m = P7_m51(((20 * ebp) / FD(0x28)) << 10);  // 0x0041c011..0x0041c031
        FD(0x34) = m;                                   // 0x0041c035
        int fr = CC(0x467270, edi, m);                  // 0x0041c039 foldRange(ang, m)
        ebp = fr + G32(rec + 0x57958c);                 // 0x0041c044..0x0041c04f
        int ad = CC(0x4672b0, edi, FD(0x34));           // 0x0041c057 absDiff(ang, m)
        CC(0x42fb90, ebp, G32(rec + 0x579590) - ad, FA(0x5c), FA(0x54), 0);  // 0x0041c07b
        CC(0x406220, esi, FD(0x10) + 1, FD(0x5c), FD(0x54) + 1, 0x80000000, ebx, 7);  // 0x0041c09d
    }
    // -- line F --
    CC(0x40bf20, FD(0x5c), FD(0x54), 0x80007fff, ebx, 7);  // 0x0041c0bd lineTo
    // 0x0041c0c2 falls through to 0x0041c0c5 (ebx dead there)
    return;
}  // Ph_00419d06

}  // namespace

// c3fy part H1: mainLoop key loop 0x0041c0c5..0x0041ecbb (+ quit dialog 0x00420ac6..0x00420e35 fragments).
// Transcribed from log/c3/alt/c3fy_norm.asm. Entry Ph_0041c0c5(F, h2in).
// Returns kFrame (jump 0x0040fc29), kMenu (0x00420e35), or kNext (fall into 0x0041ecbb).
// On kNext, h2in[0..5] = eax, ebp, ebx, ecx, edx, esi as at 0x0041ecbb.
// This file is #included after c3fy_prelude.h (namespace {...} open).

namespace {

int Ph_0041c0c5(u8* F, int* h2in) {
    int eax, ebx = 0, ecx, edx = 0, esi = 0, edi, ebp = 0;

    // ---- 0x0041c0c5  per-frame input fetch ----
    edi = 0;                                  // 0x0041c0c5 xor edi,edi
    G32(0x005A9CC0) = 0;                      // 0x0041c0c7
    CC(0x0045C030);                           // 0x0041c0cd pumpMessages
    esi = CC(0x0045AE50);                     // 0x0041c0d2 setDirtyFlagB -> esi (the key)
    FD(0x10) = esi;                           // 0x0041c0d9
    eax = CC(0x0045AE30);                     // 0x0041c0dd setDirtyFlagA
    FD(0x28) = eax;                           // 0x0041c0e4 (second key/flags)
    if (esi == edi) goto a41c150;             // 0x0041c0e8 cmp esi,edi; je
    if (!(G32(0x0059E7B8) & 0x8000)) goto a41c150;  // 0x0041c0ea test ah,0x80; je
    if (esi != 0x1b) goto a41c147;            // 0x0041c0f4 cmp esi,0x1b; jne
    // Esc with the 0x8000 flag set: cancel a drag / reset view (0x0041c0f9..0x0041c145)
    CC(0x0040B9B0, (int)0x4C60B0, 1, edi, edi);    // 0x0041c102 loadGame(0x4c60b0,1,0,0)
    eax = (int)G16(0x0058BCBA) + 4;           // 0x0041c107 movsx eax,[0x58bcba]; add eax,4
    G32(0x004C2BA0) = eax;                    // 0x0041c114
    G32(0x00567A18) = edi;                    // 0x0041c119
    eax = (int)G16(0x0058BCBC) - 4;           // 0x0041c11f movsx; sub eax,4
    G32(0x004C2844) = 4;                      // 0x0041c129
    G32(0x004C2BA4) = eax;                    // 0x0041c133
    eax = G32(0x0059E7B8);                    // 0x0041c138
    eax &= 0xFFFF7FFF;                        // 0x0041c13d and ah,0x7f (clear bit 0x8000)
    G32(0x0059E7B8) = eax;                    // 0x0041c140
    goto a41c14c;                             // 0x0041c145
a41c147:
    CC(0x004604F0);                           // 0x0041c147 showTutorialText
a41c14c:
    FD(0x10) = edi;                           // 0x0041c14c  [F+0x10] = 0 (consume key)
    esi = edi;                                // (esi tracks [F+0x10] value; keep in sync)

a41c150:
    // ---- 0x0041c150  date rollover check ----
    eax = G32(0x0056A518);                    // 0x0041c150
    ecx = eax + 0x10;                         // 0x0041c155 lea ecx,[eax+0x10]
    eax = G32(0x00834170);                    // 0x0041c158
    if (eax != ecx) goto a41c17b;             // 0x0041c15d cmp eax,ecx; jne
    edx = G32(0x00543CF8);                    // 0x0041c161
    ecx = G32(0x0056C7B4);                    // 0x0041c167
    ecx |= edx;                               // 0x0041c16d or ecx,edx
    G32(0x00543CF8) = 0;                      // 0x0041c16f  (edi==0)
    G32(0x0056C7B4) = ecx;                    // 0x0041c175
a41c17b:
    if (eax != 0x2200) goto a41c32a;          // 0x0041c17b cmp eax,0x2200; jne
    if (G32(0x00822C88) != 0) goto a41c32a;   // 0x0041c186 mov eax,[0x822c88]; test; jne
    // string compare 0x4c60a4 vs 0x4d6098 (0x0041c193..0x0041c1cc): equal -> continue
    {
        const char* a = reinterpret_cast<const char*>(0x004C60A4);
        const char* b = reinterpret_cast<const char*>(0x004D6098);
        int cmp;
        while (*a == *b) {                    // 0x0041c19d loop
            if (*a == 0) { cmp = 0; break; }  // 0x0041c1a9 je (equal end)
            a++; b++;
        }
        if (*a != *b) cmp = (*(const u8*)a < *(const u8*)b) ? -1 : 1;  // 0x0041c1c5 sbb/sbb
        if (cmp != 0) goto a41c32a;           // 0x0041c1cc test eax; jne
    }
    if (G8(0x0059E7B8) & 4) goto a41c32a;     // 0x0041c1d2 test byte [0x59e7b8],4; jne
    // build championship-end popup text (0x0041c1df..0x0041c2e2)
    StrCpy(kBuf, 0x004C6064);                 // 0x0041c1df..0x0041c204
    StrCat(kBuf, 0x004D6098);                 // 0x0041c209..0x0041c22f
    StrCat(kBuf, 0x004C6014);                 // 0x0041c234..0x0041c25a
    StrCat(kBuf, 0x004C5FDC);                 // 0x0041c25c..0x0041c285
    StrCat(kBuf, 0x004D6098);                 // 0x0041c287..0x0041c2b1
    StrCat(kBuf, 0x004C5FD8);                 // 0x0041c2b3..0x0041c2e2
    eax = CC(0x0046D6E0, 0x190, 0x64, 1, 1, 0);    // 0x0041c2e9 showPopup; eax=0 pushed (0x41c2b0)
    if (eax != 0) goto a41c32a;               // 0x0041c2f1 test eax; jne
    G16(0x00582D6E) = (short)eax;             // 0x0041c2f5 mov [0x582d6e],ax (ax=0)
    eax = G32(0x004D60B4);                    // 0x0041c2fb
    G8(0x00582CD0) = 0x20;                    // 0x0041c302
    if (eax == 0) goto a41c314;               // 0x0041c300 test eax; je
    G8(0x00582CD1) = (s8)(eax & 7);           // 0x0041c30b and al,7; mov [0x582cd1],al
    goto a41c31b;                             // 0x0041c312
a41c314:
    G8(0x00582CD1) = 7;                       // 0x0041c314
a41c31b:
    CC(0x004385D0, 0, 0x98);                  // 0x0041c322 showCharacterEditor(0,0x98)

a41c32a:
    // ---- 0x0041c32a  golfer-distance / landmark scan ----
    if (!(G8(0x0056C7B4) & 0xC)) goto a41c546; // 0x0041c32a test byte [0x56c7b4],0xc; je
    edi = -1;                                 // 0x0041c337 or edi,0xffffffff
    ebx = 0xFFFF;                             // 0x0041c33a
    ebp = 0;                                  // 0x0041c33f xor ebp,ebp
    esi = 0x005794C0;                         // 0x0041c341 mov esi,0x5794c0
a41c346:
    {
        s8 al = G8(esi + 0x19);               // 0x0041c346 mov al,[esi+0x19]
        if (al <= 0) goto a41c374;            // 0x0041c349 test al,al; jle
    }
    eax = G32(esi);                           // 0x0041c34d mov eax,[esi]
    if (eax == -1) goto a41c374;              // 0x0041c34f cmp eax,-1; je
    ecx = G32(esi + 4);                       // 0x0041c354 mov ecx,[esi+4]
    eax += (int)0xFFFFFEA2;                   // 0x0041c357 add eax,0xfffffea2
    ecx -= 0x12C;                             // 0x0041c35c sub ecx,0x12c
    eax = CC(0x0040ACD0, eax, ecx);           // 0x0041c364 distance(eax,ecx); last push=eax
    if (eax >= ebx) goto a41c374;             // 0x0041c36c cmp eax,ebx; jge (signed)
    ebx = eax;                                // 0x0041c370
    edi = ebp;                                // 0x0041c372 mov edi,ebp (best index)
a41c374:
    esi += 0x100;                             // 0x0041c374
    ebp++;                                     // 0x0041c37a inc ebp
    if (esi < 0x00582CC0) goto a41c346;       // 0x0041c37b cmp esi,0x582cc0; jl (signed)
    if (edi == -1) goto a41c3ac;              // 0x0041c383 cmp edi,-1; je
    if (!(G8(0x0056C7B4) & 8)) goto a41c3ac;  // 0x0041c388 test byte [0x56c7b4],8; je
    edx = edi;                                // 0x0041c391
    edx <<= 8;                                // 0x0041c395 shl edx,8
    G16(edx + 0x0057955C) = (short)0xFFFF;    // 0x0041c39b mov word [edx+0x57955c],0xffff
    CC(0x00467A00, edi, 8, 0x14);             // 0x0041c3a4 emitGolferThought(edi,8,0x14); last push=edi
a41c3ac:
    ecx = G32(0x0056C7B4);                    // 0x0041c3ac
    ecx &= 0xFFFFFFF7;                         // 0x0041c3b2 and ecx,~8
    G32(0x0056C7B4) = ecx;                     // 0x0041c3b8
    if (edi == -1) goto a41c53a;              // 0x0041c3b5 cmp edi,-1; je
    if (!((s8)(ecx & 0xFF) & 4)) goto a41c53a; // 0x0041c3c4 mov al,cl; test al,4; je
    esi = edi;                                // 0x0041c3ce
    ebx = 0x100000;                           // 0x0041c3d0
    esi <<= 8;                                // 0x0041c3d5 shl esi,8
    eax = G32(esi + 0x005794C8);              // 0x0041c3d8
    eax &= ebx;                               // 0x0041c3de and eax,ebx
    if (eax != 0) goto a41c3f8;               // 0x0041c3e0 jne
    ecx = (int)G16(esi + 0x0057955A);         // 0x0041c3e2 movsx ecx,word
    ecx <<= 8;                                // 0x0041c3e9
    if (!(G32(ecx + 0x005794C8) & ebx)) goto a41c53a;  // 0x0041c3ec test; je
a41c3f8:
    if (eax == 0) goto a41c496;               // 0x0041c3f8 test eax; je
    // partner path (0x0041c400..0x0041c494)
    edx = (int)G16(esi + 0x0057955A);         // 0x0041c400 movsx edx,word
    edx <<= 8;                                // 0x0041c407
    eax = (int)G16(edx + 0x0057956A);         // 0x0041c40f movsx eax,word
    eax <<= 3;                                // 0x0041c416
    eax = Rnd(eax);                           // 0x0041c41a Random::range(eax)
    ecx = (int)G16(esi + 0x0057955A);         // 0x0041c41f
    edx = (int)G8(esi + 0x005794D9);          // 0x0041c426 movsx edx,byte
    ecx <<= 8;                                // 0x0041c42d
    G8(ecx + edx + 0x005794EF) = (s8)eax;     // 0x0041c430
    eax = (int)G16(esi + 0x0057955A);         // 0x0041c437
    eax <<= 8;                                // 0x0041c43e
    if (G8(eax + 0x00579541) & 0x80) goto a41c4f4;  // 0x0041c441 test; jne
    eax = Rnd(0x20);                          // 0x0041c44e..0x0041c455
    ecx = (int)G16(esi + 0x0057955A);         // 0x0041c45a
    edx = (int)G8(esi + 0x005794D9);          // 0x0041c461
    ecx <<= 8;                                // 0x0041c468
    G8(ecx + edx + 0x005794EF) = (s8)eax;     // 0x0041c46b
    eax = (int)G16(esi + 0x0057955A);         // 0x0041c472
    eax <<= 8;                                // 0x0041c479
    if (!(G8(eax + 0x00579541) & 0x40)) goto a41c4f4;  // 0x0041c47c test; je
    ecx = (int)G8(esi + 0x005794D9);          // 0x0041c485
    G8(ecx + eax + 0x005794EF) = 0x63;        // 0x0041c48c
    goto a41c4f4;                             // 0x0041c494
a41c496:
    eax = (int)G16(esi + 0x0057956A);         // 0x0041c496 movsx eax,word
    eax <<= 3;                                // 0x0041c49d
    eax = Rnd(eax);                           // 0x0041c4a1..0x0041c4a6
    edx = (int)G8(esi + 0x005794D9);          // 0x0041c4ab
    G8(edx + esi + 0x005794EF) = (s8)eax;     // 0x0041c4b2
    if (G8(esi + 0x00579541) & 0x80) goto a41c4f4;  // 0x0041c4b9 test; jne
    eax = Rnd(0x20);                          // 0x0041c4c2..0x0041c4c9
    ecx = (int)G8(esi + 0x005794D9);          // 0x0041c4ce
    G8(ecx + esi + 0x005794EF) = (s8)eax;     // 0x0041c4d5
    if (!(G8(esi + 0x00579541) & 0x40)) goto a41c4f4;  // 0x0041c4dc test; je
    edx = (int)G8(esi + 0x005794D9);          // 0x0041c4e5
    G8(edx + esi + 0x005794EF) = 0x63;        // 0x0041c4ec
a41c4f4:
    if (!(G32(esi + 0x005794C8) & ebx)) edi ^= 1;  // 0x0041c4f4 test; jne skip; 0x0041c4fc xor edi,1
    CC(0x00466370, edi, 1);                   // 0x0041c502 advanceStory(edi,1); last push=edi
    edi = G32(esi + 0x005794C8);              // 0x0041c507
    edi |= 0x200000;                          // 0x0041c510
    G32(esi + 0x005794C8) = edi;              // 0x0041c516
    eax = (int)G16(esi + 0x0057955A);         // 0x0041c51c
    eax <<= 8;                                // 0x0041c523
    ecx = G32(eax + 0x005794C8);              // 0x0041c526
    eax = eax + 0x005794C8;                   // 0x0041c52c lea eax,[eax+0x5794c8]
    ecx |= 0x200000;                          // 0x0041c532
    G32(eax) = ecx;                           // 0x0041c538
a41c53a:
    eax = G32(0x0056C7B4);                    // 0x0041c53a
    eax &= 0xFFFFFFFB;                        // 0x0041c53f and al,0xfb
    G32(0x0056C7B4) = eax;                    // 0x0041c541

a41c546:
    edx = G32(0x00567AFC);                    // 0x0041c546 mov edx,[0x567afc]
    ebp = 0;                                  // 0x0041c54c xor ebp,ebp
a41c54e:
    ebx = 1;                                  // 0x0041c54e mov ebx,1
a41c553:
    // ---- 0x0041c553  key dispatch loop head ----
    esi = FD(0x10);                           // 0x0041c553 mov esi,[F+0x10]
    if (esi == ebp) goto a41c587;             // 0x0041c557 cmp esi,ebp; je
    {
        s8 al = G8(0x00569498);               // 0x0041c55b mov al,[0x569498]
        G32(0x00543D04) = -1;                 // 0x0041c560
        if (al != 0) {                        // 0x0041c56a test al,al; je
            if (esi == 0x20) {                // 0x0041c56e cmp esi,0x20; jne
                esi = 0;                      // 0x0041c573 xor esi,esi
                FD(0x10) = esi;               // 0x0041c575
                goto a41c580;                 // 0x0041c579
            }
        }
    }
    if (esi == 0x50) goto a41c587;            // 0x0041c57b cmp esi,0x50; je
a41c580:
    G8(0x00569498) = 0;                       // 0x0041c580 mov byte [0x569498],0
a41c587:
    if (!(G32(0x0059E7B8) & 0x2000000)) goto a41c59e;  // 0x0041c587 test; je
    CC(0x00409CF0);                           // 0x0041c593 stepCourseEditQueue
    edx = G32(0x00567AFC);                    // 0x0041c598
a41c59e:
    eax = esi - 9;                            // 0x0041c59e lea eax,[esi-9]
    if ((u32)eax > 0x71) goto a41de11;        // 0x0041c5a1 cmp eax,0x71; ja
    ecx = GU8(0x0042191C + eax);              // 0x0041c5ac mov cl,[eax+0x42191c]
    switch (ecx) {                            // 0x0041c5b2 jmp [ecx*4+0x421838]
        case 0:  goto a41c9ea; case 1:  goto a41c9c8; case 2:  goto a41c9c4;
        case 3:  goto a41cfe0; case 4:  goto a41dd2d; case 5:  goto a41dd3b;
        case 6:  goto a41dd42; case 7:  goto a41dd49; case 8:  goto a41dd57;
        case 9:  goto a41dd65; case 10: goto a41dd5e; case 11: goto a41d03b;
        case 12: goto a41d997; case 13: goto a41d078; case 14: goto a41d05e;
        case 15: goto a41c954; case 16: goto a41dcf1; case 17: goto a41dcf5;
        case 18: goto a41dcfc; case 19: goto a41dd03; case 20: goto a41dd0a;
        case 21: goto a41dd11; case 22: goto a41dd18; case 23: goto a41dd1f;
        case 24: goto a41dd26; case 25: goto a41db46; case 26: goto a41d92f;
        case 27: goto a41dd34; case 28: goto a41cc8a; case 29: goto a41ca98;
        case 30: goto a41cd12; case 31: goto a41cc70; case 32: goto a41cc7f;
        case 33: goto a41d020; case 34: goto a41d016; case 35: goto a41cc66;
        case 36: goto a41d02c; case 37: goto a41ddec; case 38: goto a41ca20;
        case 39: goto a41dd50; case 40: goto a41c997; case 41: goto a41c6f3;
        case 42: goto a41cfb2; case 43: goto a41c5b9; case 44: goto a41c613;
        case 45: goto a41c6d9; case 46: goto a41c66d; case 47: goto a41c6fe;
        case 48: goto a41c980; case 49: goto a41c5e9; case 50: goto a41c641;
        case 51: goto a41c685; case 52: goto a41c6bd; case 53: goto a41c6a1;
        case 54: goto a41c8e7; case 55: goto a41c92d; default: goto a41de11;
    }

    // ======= key switch cases (original order) =======
a41c5b9:  // key 0x66
    if (edx != 3) goto a41c5cd;               // 0x0041c5b9 cmp edx,3; jne
    G32(0x0058F330) = -1;                     // 0x0041c5be
    goto a41de11;                             // 0x0041c5c8
a41c5cd:
    G32(0x0055E924) = ebp;                    // 0x0041c5d0
    G32(0x00567AFC) = ebp;                    // 0x0041c5d6
    CC(0x0040D890, 2, ebx);                   // 0x0041c5dc  (push ebx; push 2)
    goto a41de11;                             // 0x0041c5e4
a41c5e9:  // key 0x72
    {
        u8 dl = GU8(0x005A34E0);              // 0x0041c5e9 mov dl,[0x5a34e0]
        dl = (u8)(dl - 1);                    // 0x0041c5f0 dec dl
        G32(0x0055E924) = ebp;                // 0x0041c5f2
        edx = (dl != 0) ? 4 : 5;              // 0x0041c5f8 neg dl; sbb edx,edx; add edx,5
        G32(0x00567AFC) = ebp;                // 0x0041c5fc
        CC(0x0040D890, edx, ebx);             // 0x0041c606 (push ebx; push edx)
    }
    goto a41de11;                             // 0x0041c60e
a41c613:  // key 0x67
    edx = G32(0x004C2848);                    // 0x0041c613
    eax = (edx != ebx) ? 1 : 0;               // 0x0041c61b cmp edx,ebx; setne al
    G32(0x0055E924) = ebp;                    // 0x0041c622
    G32(0x00567AFC) = ebp;                    // 0x0041c628
    CC(0x0040D890, eax, ebx);                 // 0x0041c62e (push ebx; push eax)
    G32(0x005A34F0) = ebp;                    // 0x0041c636
    goto a41de11;                             // 0x0041c63c
a41c641:  // key 0x73
    if (edx != 3) goto a41c651;               // 0x0041c641 cmp edx,3; jne
    G32(0x0058F330) = ebp;                    // 0x0041c646
    goto a41de11;                             // 0x0041c64c
a41c651:
    G32(0x0055E924) = ebp;                    // 0x0041c654
    G32(0x00567AFC) = ebp;                    // 0x0041c65a
    CC(0x0040D890, 7, ebx);                   // 0x0041c660 (push ebx; push 7)
    goto a41de11;                             // 0x0041c668
a41c66d:  // key 0x6c
    if (edx != 3) goto a41de11;               // 0x0041c66d cmp edx,3; jne
    G32(0x0058F330) = 4;                      // 0x0041c676
    goto a41de11;                             // 0x0041c680
a41c685:  // key 0x74
    G32(0x0055E924) = ebp;                    // 0x0041c688
    G32(0x00567AFC) = ebp;                    // 0x0041c68e
    CC(0x0040D890, 0xD, ebx);                 // 0x0041c694 (push ebx; push 0xd)
    goto a41de11;                             // 0x0041c69c
a41c6a1:  // key 0x77
    G32(0x0055E924) = ebp;                    // 0x0041c6a4
    G32(0x00567AFC) = ebp;                    // 0x0041c6aa
    CC(0x0040D890, 0x11, ebx);                // 0x0041c6b0 (push ebx; push 0x11)
    goto a41de11;                             // 0x0041c6b8
a41c6bd:  // key 0x76
    G32(0x0055E924) = ebp;                    // 0x0041c6c0
    G32(0x00567AFC) = ebp;                    // 0x0041c6c6
    CC(0x0040D890, 0xA, ebx);                 // 0x0041c6cc (push ebx; push 0xa)
    goto a41de11;                             // 0x0041c6d4
a41c6d9:  // key 0x68
    if (edx != 3) goto a41c6e9;               // 0x0041c6d9 cmp edx,3; jne
    G32(0x0058F330) = edx;                    // 0x0041c6de  (=3)
    goto a41de11;                             // 0x0041c6e4
a41c6e9:
    CC(0x0040E720);                           // 0x0041c6e9 announceBuilding
    goto a41de11;                             // 0x0041c6ee
a41c6f3:  // key 0x64
    G32(0x0058F330) = ebx;                    // 0x0041c6f3  (=1)
    goto a41de11;                             // 0x0041c6f9
a41c6fe:  // key 0x6e
    if (G32(0x0059E7B8) & 0x200000) goto a41c84a;  // 0x0041c6fe test; jne
    if (G32(0x005A59F8) <= ebp) goto a41de11; // 0x0041c70e cmp [0x5a59f8],ebp; jle (signed)
    StrCpy(kBuf, 0x004C5FA8);                 // 0x0041c71a..0x0041c74a
    StrCat(kBuf, 0x004C5F84);                 // 0x0041c74c..0x0041c775
    eax = CC(0x0046D6E0, 0x190, 0x8C, ebx, ebx, ebp);  // 0x0041c777 showPopup; pushes ebp,ebx,ebx,0x8c,0x190
    if (eax != 0) goto a41de11;               // 0x0041c77f test eax; jne
    ebx = 0;                                  // 0x0041c787 xor ebx,ebx
    ebp = 0x005794D1;                         // 0x0041c789 mov ebp,0x5794d1
a41c78e:  // per-golfer loop
    eax = G32(ebp - 9);                       // 0x0041c78e mov eax,[ebp-9]
    if (!((eax >> 8) & 2)) goto a41c82b;      // 0x0041c791 test ah,2; je
    eax &= ~0x0200;                           // 0x0041c79a and ah,0xfd
    G32(ebp - 9) = eax;                       // 0x0041c79f
    {
        u8 b = (u8)(GU8(ebp) & 7);            // 0x0041c7a2 al=[ebp]; and al,7
        G8(ebp) = (s8)b;                      // 0x0041c7ac mov [ebp],al
        int r = Rnd(3);                       // 0x0041c7af Random::range(3)
        u32 e = (u32)r & 0xFFFF;              // 0x0041c7b4 mov cx,ax; and ecx,0xffff
        u8 al = (u8)(0xFF << (e & 0xFF));     // 0x0041c7b7 or al,0xff; shl al,cl
        u8 cl = GU8(ebp);                     // 0x0041c7c6 mov cl,[ebp]
        cl = (u8)(cl + al);                   // 0x0041c7cb add cl,al
        G8(ebp) = (s8)cl;                     // 0x0041c7cf mov [ebp],cl
    }
    StrCpy(kBuf, 0x004C5F74);                 // 0x0041c7bf..0x0041c7f4 (src 0x4c5f74)
    CC(0x0040CB00, 0x80000000, ebp, -1);      // 0x0041c7f6 startMessage; pushes -1,ebp(eax=0 slot),0x80000000
    eax = G32(0x00567AFC);                    // 0x0041c7fb
    if (eax == 3) G32(0x00567AFC) = 1;        // 0x0041c803 cmp eax,3; jne skip; 0x0041c808
    CC(0x004266B0, ebx);                      // 0x0041c813 patronEvent(ebx)
    CC(0x00426670, ebx);                      // 0x0041c819 resetGolfer(ebx)
    G32(0x005A59F8) = -1;                     // 0x0041c821
a41c82b:
    ebp += 0x100;                             // 0x0041c82b
    ebx++;                                     // 0x0041c831 inc ebx
    if (ebp < 0x00582CD1) goto a41c78e;       // 0x0041c832 cmp ebp,0x582cd1; jl (signed)
    ebp = 0;                                  // 0x0041c83e xor ebp,ebp
    ebx = 1;                                  // 0x0041c840 mov ebx,1
    goto a41de11;                             // 0x0041c845
a41c84a:
    StrCpy(kBuf, 0x004C5F40);                 // 0x0041c84a..0x0041c877
    StrCat(kBuf, 0x004C5F84);                 // 0x0041c879..0x0041c8a7
    eax = CC(0x0046D6E0, 0x190, 0x8C, ebx, ebx, ebp);  // 0x0041c8a9 showPopup
    if (eax != 0) goto a41de11;               // 0x0041c8b1 test eax; jne
    G8(0x00569498) = (s8)eax;                 // 0x0041c8b9 mov [0x569498],al (0)
    CC(0x0045C150);                           // 0x0041c8be pumpLight
    G32(0x0059E7B8) &= 0xFF9FFFFF;            // 0x0041c8c3
    CC(0x0046D0C0);                           // 0x0041c8cd resetGolferState
    G32(0x00567B04) = ebp;                    // 0x0041c8d2
    G32(0x005A59F8) = -1;                     // 0x0041c8d8
    goto a41de11;                             // 0x0041c8e2
a41c8e7:  // key 0x78
    if (G32(0x005A9CBC) == ebp) goto a41c900; // 0x0041c8e7 cmp [0x5a9cbc],ebp; je
    G32(0x005A9CBC) = ebp;                     // 0x0041c8ef
    G32(0x00567B00) = ebx;                     // 0x0041c8f5
    goto a41de11;                             // 0x0041c8fb
a41c900:
    eax = G32(0x004C2844);                    // 0x0041c900
    if (eax > ebx) {                          // 0x0041c905 cmp eax,ebx; jle skip
        eax = eax / 2;                        // 0x0041c909 cdq; sub eax,edx; sar eax,1 (signed /2)
        G32(0x004C2844) = eax;                // 0x0041c90e
    }                                          // 0x0041c913
    eax <<= 3;                                // 0x0041c913 shl eax,3
    G32(0x004C2840) = eax;                    // 0x0041c916
    eax = -1;                                 // 0x0041c91b or eax,0xffffffff
    G32(0x004C2848) = eax;                    // 0x0041c91e
    G32(0x004C2854) = eax;                    // 0x0041c923
    goto a41de11;                             // 0x0041c928
a41c92d:  // key 0x7a
    eax = G32(0x004C2844);                    // 0x0041c92d
    if (eax >= 4) goto a41de11;               // 0x0041c932 cmp eax,4; jge (signed)
    eax += eax;                               // 0x0041c93b
    G32(0x004C2844) = eax;                    // 0x0041c93d
    ecx = eax * 8;                            // 0x0041c942 lea ecx,[eax*8]
    G32(0x004C2840) = ecx;                    // 0x0041c949
    goto a41de11;                             // 0x0041c94f
a41c954:  // key 0x30
    if (G32(0x005685F4) != ebp) goto a41c975; // 0x0041c954 cmp [0x5685f4],ebp; jne
    edx = (int)G16(0x0058BCBA);               // 0x0041c95c movsx
    eax = (int)G16(0x0058BCBC);               // 0x0041c963 movsx
    G32(0x004C2BA0) = edx;                     // 0x0041c96a
    G32(0x004C2BA4) = eax;                     // 0x0041c970
a41c975:
    G32(0x005685F4) = ebp;                     // 0x0041c975
    goto a41de11;                             // 0x0041c97b
a41c980:  // key 0x70
    G32(0x00567AFC) = ebp;                     // 0x0041c980
    G32(0x0055E924) = ebx;                     // 0x0041c986
    G32(0x004C2854) = ebp;                     // 0x0041c98c
    goto a41de11;                             // 0x0041c992
a41c997:  // key 0x62
    eax = Rnd(5);                              // 0x0041c997..0x0041c9b0 (push 5 before stores)
    G32(0x00567AFC) = ebp;                     // 0x0041c99e
    G32(0x0055E924) = ebx;                     // 0x0041c9a4
    G32(0x004C2854) = ebx;                     // 0x0041c9aa
    G32(0x0056A514) = (eax & 0xFFFF);          // 0x0041c9b5 and eax,0xffff; mov [0x56a514],eax
    goto a41de11;                             // 0x0041c9bf
a41c9c4:  // key 0x1b (Esc): clears [F+0x10], falls into 0x41c9c8
    FD(0x10) = ebp;                            // 0x0041c9c4
a41c9c8:  // key 0xd (Enter)
    eax = -1;                                  // 0x0041c9c8 or eax,0xffffffff
    G32(0x0053DF54) = ebp;                     // 0x0041c9cb
    G32(0x004C2854) = eax;                     // 0x0041c9d1
    G32(0x004C2848) = eax;                     // 0x0041c9d6
    G32(0x0059E7B8) &= 0xF7FFFFFD;             // 0x0041c9db
    goto a41de11;                             // 0x0041c9e5
a41c9ea:  // key 0x9 (Tab)
    ecx = G32(0x005A34F0);                     // 0x0041c9ea
    {
        s8 al = G8(0x0059E7B8);                // 0x0041c9f0 mov al,[0x59e7b8]
        ecx++;                                 // 0x0041c9f5 inc ecx
        G32(0x005A34F0) = ecx;                 // 0x0041c9f8
        if (!(al & 2)) goto a41de11;           // 0x0041c9f6 test al,2; je
    }
    ecx = G32(0x00542F20) + 1;                 // 0x0041ca04 lea eax,[ecx+1]
    edx = ecx % 3;                             // 0x0041ca0a mov ecx,3; cdq; idiv ecx -> edx=rem
    G32(0x00542F20) = edx;                     // 0x0041ca15
    goto a41de11;                             // 0x0041ca1b
a41ca20:  // key 0x58: reset tile/level tables, rebuild
    if (!(G32(0x0059E7B8) & 0x1000000)) goto a41de11;  // 0x0041ca20 test; je
    {
        for (int i = 0; i < 0x271; i++)                // 0x0041ca30 rep stosd ecx=0x271
            G32(0x005722E8 + 4 * i) = 0x04040404;
    }
    edx = 0x0053CAF0;                                  // 0x0041ca3f mov edx,0x53caf0
    esi = 0x005A4998;                                  // 0x0041ca44 mov esi,0x5a4998
a41ca4b:
    {
        for (int i = 0; i < 0xC; i++)                  // 0x0041ca4b rep stosd ecx=0xc
            G32(esi + 4 * i) = 0x04040404;
        G16(esi + 0x30) = (short)0x0404;               // 0x0041ca59 stosw (edi at esi+0x30)
        for (int k = 0; k < 0x32; k++) {               // 0x0041ca5b..0x0041ca69 loop eax=0x32
            G16(edx) = (short)(G16(edx) & 0x480);      // 0x0041ca60 and word [edx],0x480
            edx += 2;                                  // 0x0041ca65 add edx,2
        }
    }
    esi += 0x33;                                       // 0x0041ca6b add esi,0x33
    if (esi < 0x005A538E) goto a41ca4b;                // 0x0041ca6e cmp; jl (signed)
    CC(0x004315E0);                                    // 0x0041ca76 buildGolferBioText
    CC(0x0042F7A0);                                    // 0x0041ca7b rebuildHeightfield
    eax = G32(0x0059E7B8);                             // 0x0041ca80
    G32(0x00542DF8) = ebx;                             // 0x0041ca85 (=1)
    eax |= 0x0800;                                     // 0x0041ca8b or ah,8
    G32(0x0059E7B8) = eax;                             // 0x0041ca8e
    goto a41de11;                                      // 0x0041ca93
a41ca98:  // key 0x43
    if (G32(0x0059E7B8) & 0x200000) goto a41cc20;      // 0x0041ca98 test; jne
    G8(kBuf) = 0;                                      // 0x0041caaa mov byte[kBuf],0 (push -1 at 0x41caa8)
    CC(0x0040DAA0, -1);                                // 0x0041cab1 appendCourseTitle(-1)
    CC(0x0045B8B0, ebp);                               // 0x0041cab7 appendToBuffer45b8b0(0) (push ebp)
    StrCpy(kBuf, 0x004C5328);                          // 0x0041cabc..0x0041cae0 (push ebp at 0x41cac6): strcpy, the
                                                       // destination is not scanned ("Themes\Championship\"; c3fy2 fix: was
                                                       // StrCat, so fileExists missed the .cse and the save ran unasked)
    CC(0x0040DAA0, ebp);                               // 0x0041cae2 appendCourseTitle(0) (top push=ebp)
    StrCat(kBuf, 0x004C5F38);                          // 0x0041cae7..0x0041cb17 (push kBuf at 0x41caf1)
    eax = CC(0x004378A0, (int)kBuf);                   // 0x0041cb19 fileExists(kBuf) (top push=kBuf)
    if (eax == 0) goto a41cbcc;                        // 0x0041cb21 test eax; je
    StrCat(kBuf, 0x004C3F0C);                          // 0x0041cb29..0x0041cb5c (push ebp,ebx interleaved)
    eax = CC(0x0046D6E0, 0x190, 0x8C, ebx, ebx, ebp);  // 0x0041cb63 showPopup
    if (eax != 0) goto a41de11;                        // 0x0041cb6b test eax; jne
    StrCpy(kBuf, 0x004C5328);                          // 0x0041cb73..0x0041cb95 (push ebp at 0x41cb81)
    CC(0x0040DAA0, ebp);                               // 0x0041cb97 appendCourseTitle(0) (top push=ebp)
    StrCat(kBuf, 0x004C5F38);                          // 0x0041cb9c..0x0041cbca
a41cbcc:
    CC(0x0040B680);                                    // 0x0041cbcc
    G8(kBuf) = 0;                                      // 0x0041cbd2 (push ebx at 0x41cbd1)
    CC(0x0040DAA0, ebx);                               // 0x0041cbd9 appendCourseTitle(1) (top push=ebx)
    StrCat(kBuf, 0x004C5F18);                          // 0x0041cbde..0x0041cc11 (push -1,ebp,0x80006000)
    CC(0x0040CB00, 0x80006000, ebp, -1);               // 0x0041cc13 startMessage
    goto a41de11;                                      // 0x0041cc1b
a41cc20:  // SimFoto branch of key 0x43
    StrCpy(kBuf, 0x004C5EF0);                          // 0x0041cc20..0x0041cc4b (push -1,ebp,0x80006000)
    CC(0x0040CB00, 0x80006000, ebp, -1);               // 0x0041cc4d startMessage
    CC(0x004481B0, 0x18, 0x64, ebp, ebp, ebp);         // 0x0041cc59 playSound
    goto a41de11;                                      // 0x0041cc61
a41cc66:  // key 0x53
    CC(0x00405B10);                                    // 0x0041cc66 saveGameDialog
    goto a41de11;                                      // 0x0041cc6b
a41cc70:  // key 0x4c
    CC(0x0043B610, ebp, ebp);                          // 0x0041cc72 loadGameBrowserScreen(0,0)
    goto a41de11;                                      // 0x0041cc7a
a41cc7f:  // key 0x4e
    G32(0x005A5A00) ^= ebx;                            // 0x0041cc7f xor [0x5a5a00],ebx
    goto a41de11;                                      // 0x0041cc85
a41cc8a:  // key 0x42: golfer snapshot image
    TC(0x00474AE0, FA(0x8DC));                         // 0x0041cc91 Image_ctor(&img8dc)
    TC(0x00421B30, FA(0x8DC), 0x320, 0x258, ebx, ebp, ebp);  // 0x0041ccb2 (ecx passthrough; reset image)
    FUB(0x124C) = 4;                                   // 0x0041ccaa mov byte[F+0x124c],4
    TC(0x00475840, FA(0x8DC), 0x004C5EE4, ebp, ebp, 0x100, 2);  // 0x0041cccc loadImage
    TC(0x00473AE0, 0x0058B650);                        // 0x0041ccd6 Surface_free(0x58b650)
    TC(0x00473BF0, 0x0058B650, FA(0x8DC), 0xC0, ebp, 2, 2, ebx, ebx);  // 0x0041ccf4 Surface_blit
    FUB(0x124C) = 0;                                   // 0x0041cd00 mov byte[F+0x124c],0
    TC(0x00474C40, FA(0x8DC));                         // 0x0041cd08 Image_dtor(&img8dc)
    goto a41de11;                                      // 0x0041cd0d
a41cd12:  // key 0x4a: course report screen
    if (G32(0x0059E7B8) & 0x4200000) goto a41de11;     // 0x0041cd12 test; jne
    G32(0x00567B04) = ebp;                             // 0x0041cd24 (push 2 at 0x41cd22)
    CC(0x0044FB30, 2);                                 // 0x0041cd2a showCourseReport(2)
    eax = G32(0x00567B04);                             // 0x0041cd2f
    if (eax == ebp) goto a41cf97;                      // 0x0041cd37 cmp eax,ebp; je
    StrCpy(kBuf, 0x004C5EC8);                           // 0x0041cd3f..0x0041cd76 (push 0xa,0x58a528)
    eax = Itoa((int)G16(0x005A6D3C) + 0x7D1, 0x0058A528, 0xA);  // 0x0041cd78 __itoa
    StrCat(kBuf, (u32)eax);                             // 0x0041cd7f..0x0041cda5 (edi=eax)
    StrCat(kBuf, 0x004C3F70);                           // 0x0041cda7..0x0041cdd5
    StrCat(kBuf, 0x00571FD8 + G32(0x0059BF90) * 46);    // 0x0041cdcb..0x0041ce0c (eax*46 + 0x571fd8)
    StrCat(kBuf, 0x004C5E90);                           // 0x0041ce0e..0x0041ce43 (push 0xa at 0x41ce1c)
    eax = Itoa(G32(0x00567B04), 0x0058A528, 0xA);       // 0x0041ce46 __itoa
    StrCat(kBuf, (u32)eax);                             // 0x0041ce4b..0x0041ce7b (push ebp,ebx)
    StrCat(kBuf, 0x004C5E48);                           // 0x0041ce7d..0x0041ceab (push ebx,0x168,0x190)
    eax = CC(0x0046D6E0, 0x190, 0x168, ebx, ebx, ebp); // 0x0041cead showPopup
    if (eax != 0) goto a41cf8c;                         // 0x0041ceb7 test eax; jne
    {
        u16 ax = GU16(0x004C2C9C);                      // 0x0041cebd mov ax,word[0x4c2c9c]
        for (ecx = 0; ecx < 0xC; ecx++) {               // 0x0041cec5..0x41ceda loop
            u8 dl = GU8(ecx + 0x005A5A04);              // 0x0041cec5 mov dl,[ecx+0x5a5a04]
            if (dl != 0) {                              // 0x0041cecb test dl,dl; je
                int t = ebx << ecx;                     // 0x0041cecf mov edx,ebx; shl edx,cl
                ax = (u16)(ax | (u16)t);                // 0x0041ced3 or ax,dx
            }
        }
        G16(0x004C2C9C) = (short)ax;                    // 0x0041cedf mov [0x4c2c9c],ax
        if (ax != 0) goto a41ceec;                      // 0x0041cedc cmp ax,bp; jne
    }
    CC(0x004065C0);                                     // 0x0041cee7 showCharacterIntro
a41ceec:
    CC(0x0046C970);                                     // 0x0041ceec loadGolfPro
    {
        int a = (G32(0x00575CC4) << 0xA) + 0x200;       // 0x0041cef1..0x0041cf07
        int c = (G32(0x00575CC0) << 0xA) + 0x200;
        CC(0x0046E7B0, 3, c, a);                         // 0x0041cf11 postEvent(3,c,a) (push a,c,3)
    }
    eax = G32(0x00567B04);                              // 0x0041cf16
    if (eax >= 0x1F4) {                                 // 0x0041cf1e cmp eax,0x1f4; jl skip
        int d = (G32(0x00575CC4) << 0xA) + 0x200;       // 0x0041cf25..0x0041cf3c
        int a = (G32(0x00575CC0) << 0xA) + 0x200;
        CC(0x0046E7B0, 8, a, d);                         // 0x0041cf45 postEvent(8,a,d) (push d,a,8)
    }
    if (G32(0x00567B04) < 0x3E8) goto a41de11;          // 0x0041cf4d cmp; jl (signed)
    {
        int c = (G32(0x00575CC4) << 0xA) + 0x200;       // 0x0041cf5d..0x0041cf7b
        int d = (G32(0x00575CC0) << 0xA) + 0x200;
        CC(0x0046E7B0, 0xD, d, c);                       // 0x0041cf7f postEvent(0xd,d,c) (push c,d,0xd)
    }
    goto a41de11;                                       // 0x0041cf87
a41cf8c:
    G32(0x00567B04) = ebp;                              // 0x0041cf8c
    goto a41de11;                                       // 0x0041cf92
a41cf97:
    CC(0x004481B0, 0x18, 0x64, ebp, ebp, ebp);          // 0x0041cf9e playSound
    G8(0x00569498) = 9;                                 // 0x0041cfa6
    goto a41de11;                                       // 0x0041cfad
a41cfb2:  // key 0x65
    if (edx != ebx) goto a41cfcf;                       // 0x0041cfb2 cmp edx,ebx; jne
    if (G32(0x0055E928) != ebx) goto a41cfcf;           // 0x0041cfb6 cmp [0x55e928],ebx; jne
    G32(0x00567AFC) = ebx;                              // 0x0041cfbe
    G32(0x0055E928) = ebp;                              // 0x0041cfc4
    goto a41de11;                                       // 0x0041cfca
a41cfcf:
    G32(0x00567AFC) = ebx;                              // 0x0041cfcf
    G32(0x0055E928) = ebx;                              // 0x0041cfd5
    goto a41de11;                                       // 0x0041cfdb
a41cfe0:  // key 0x20 (space)
    eax = -1;                                           // 0x0041cfe0 or eax,0xffffffff
    G32(0x0053DF54) = ebp;                              // 0x0041cfe3
    G32(0x004C2848) = eax;                              // 0x0041cfe9
    G32(0x004C2854) = eax;                              // 0x0041cfee
    G32(0x004C2E10) = eax;                              // 0x0041cff3
    eax = G32(0x0059E7B8);                              // 0x0041cff8
    eax &= 0xF7FFFFFD;                                  // 0x0041cffd
    G32(0x00567AFC) = 5;                                // 0x0041d002
    G32(0x0059E7B8) = eax;                              // 0x0041d00c
    goto a41de11;                                       // 0x0041d011
a41d016:  // key 0x52
    CC(0x00456BE0);                                     // 0x0041d016 showRoutingMapScreen
    goto a41de11;                                       // 0x0041d01b
a41d020:  // key 0x50
    G32(0x0059E7B8) ^= 4;                               // 0x0041d020 xor [0x59e7b8],4
    goto a41de11;                                       // 0x0041d027
a41d02c:  // key 0x54
    G32(0x0059E7B8) ^= 0x400;                           // 0x0041d02c xor [0x59e7b8],0x400
    goto a41de11;                                       // 0x0041d036
a41d03b:  // key 0x2b
    ecx = G32(0x00571FD4) + 0x64;                       // 0x0041d03b..0x0041d046
    eax = G32(0x0059E7B8) | 0x20000000;                 // 0x0041d041..0x0041d049
    G32(0x00571FD4) = ecx;                              // 0x0041d04e
    G32(0x0059E7B8) = eax;                              // 0x0041d054
    goto a41de11;                                       // 0x0041d059
a41d05e:  // key 0x2f: compute nearest hole, then fall into the routing render
    eax = G32(0x00543D0C);                             // 0x0041d05e
    ecx = G32(0x00543D08);                             // 0x0041d063
    eax = CC(0x00407340, ecx, eax);                    // 0x0041d06b nearestHole (push eax; push ecx)
    G32(0x0056C790) = eax;                             // 0x0041d073
a41d078:  // key 0x2e: routing / hole render into image [F+0x624]
    TC(0x00474AE0, FA(0x624));                          // 0x0041d07f Image_ctor
    TC(0x00421B30, FA(0x624), 0x320, 0x258, ebx, ebp, ebp);  // 0x0041d0a0 reset image (ecx passthrough)
    FUB(0x124C) = 5;                                    // 0x0041d098 mov byte[F+0x124c],5
    TC(0x00475D00, Root(), FA(0x624), ebp, ebp, 0x320, 0x258, ebp, ebp, 0x320, 0x258);  // 0x0041d0cb Surface_blit3
    TC(0x004762D0, Root(), 0x00519928, ebp, ebp, ebp); // 0x0041d0de setDrawTarget
    ecx = G32(0x0056C790);                              // 0x0041d0e3
    FD(0x1A8) = 0;                                      // 0x0041d0eb (eax=0)
    FD(0x18) = ecx;                                     // 0x0041d0f2
    FD(0x1AC) = 0;                                      // 0x0041d0f6
    FD(0x0C) = ebp;                                     // 0x0041d0fd (=0)
    FD(0x1B0) = 0;                                      // 0x0041d101
    FD(0x1B4) = 0;                                      // 0x0041d108
    FD(0x2C) = ecx * 0x208;                             // 0x0041d10f eax=ecx; shl6; add ecx; shl3
a41d11d:
    eax = FD(0x0C);                                     // 0x0041d11d
    if (eax != 0) goto a41d189;                         // 0x0041d121 test eax; jne
    {
        int c = (G32(0x00543D0C) << 0xA) + 0x200;       // 0x0041d125..0x0041d13c
        int d = (G32(0x00543D08) << 0xA) + 0x200;       // 0x0041d131..0x0041d144
        CC(0x00407C60, d, c, 0xA, 0x80000000);          // 0x0041d14c drawCircle (push 0x80000000,0xa,c,d)
    }
    CC(0x0040CEF0, 0x11A, 0x1A, 0x100, 0x94, 1);        // 0x0041d164 drawFrameR3
    TC(0x00478B80, 0x00519CD4, 0x11A, 0x40, 0x21A, 0x40, 0x80000210);  // 0x0041d184 Widget_fillRect
a41d189:
    ebp = FD(0x0C);                                     // 0x0041d189
    ebx = ebp & 3;                                      // 0x0041d192 mov ebx,ebp; and ebx,3
    {
        for (int i = 0; i < 0x40; i++) G32(0x00582DB8 + 4 * i) = 0;  // 0x0041d199 rep stosd 0x40, edi=0x582db8
    }
    if ((u32)ebx > 3) goto a41d21c;                     // 0x0041d19e cmp ebx,3; ja (never)
    switch (ebx) {                                      // 0x0041d1a5 jmp [ebx*4+0x421990]
        case 0: G8(0x00582DD1) = 6; edx = CC(0x0040D760, 6); break;  // 0x0041d1ac
        case 1: G8(0x00582DD1) = 5; edx = CC(0x0040D760, 5); break;  // 0x0041d1c8
        case 2: G8(0x00582DD1) = 3; edx = CC(0x0040D760, 3); break;  // 0x0041d1e4
        case 3: G8(0x00582DD1) = 7; edx = CC(0x0040D760, 7); break;  // 0x0041d200
        default: break;
    }
    FD(0xBC) = edx;                                     // 0x0041d1bf/.. mov [F+0xbc],edx
    goto a41d223;                                       // 0x0041d1c6
a41d21c:
    edx = FD(0xBC);                                     // 0x0041d21c (read-before-write possible)
a41d223:
    FD(0x290 + 4 * ebx) = edx;                          // 0x0041d226 mov [F+ebx*4+0x290],edx
    if ((u32)ebx > 3) goto a41d270;                     // 0x0041d223 cmp ebx,3; ja
    {
        u32 src;                                        // 0x0041d22f jmp [ebx*4+0x4219a0]
        switch (ebx) {
            case 0:  src = 0x004C5E38; break;           // 0x0041d236
            case 1:  src = 0x004C5E24; break;           // 0x0041d23d
            case 2:  src = 0x004C5E0C; break;           // 0x0041d244
            default: src = 0x004C5DF4; break;           // 0x0041d24b
        }
        StrCpy(kBuf, src);                              // 0x0041d250..0x0041d26e
    }
a41d270:
    {
        int color = edx;                                // edx = [F+ebx*4+0x290] color (push at 0x41d273)
        CC(0x004049D0, (int)kBuf, 0x128, 0x90 - ebx * 20, color);  // 0x0041d289 drawText4049d0
    }
    ecx = G32(0x00543D08);                              // 0x0041d28e
    esi = G32(0x00543D0C);                              // 0x0041d294
    {
        int ax0 = (ecx << 0xA) + 0x200;                 // 0x0041d29a..0x0041d2a2
        G32(0x00582E84) = -1;                           // 0x0041d2a7
        G32(0x00582E8C) = ax0;                          // 0x0041d2b1
        G32(0x00582DB8) = ax0;                          // 0x0041d2b6
        G32(0x00582E7C) = ax0;                          // 0x0041d2bb
        int ay = (esi << 0xA) + 0x200;                  // 0x0041d2c0..0x0041d2c5
        G32(0x00582E90) = ay;                           // 0x0041d2ca
        G32(0x00582DBC) = ay;                           // 0x0041d2cf
        G32(0x00582E80) = ay;                           // 0x0041d2d4
    }
    if (G32(0x00822C88) != 0) {                         // 0x0041d2d9 test; je 0x41d2f2
        eax = (ebp / 4) & 3;                            // 0x0041d2e2 cdq;and edx,3;add;sar 2;and 3
    } else {
        eax = 1;                                        // 0x0041d2f2
    }
    G8(0x00582E72) = (s8)eax;                           // 0x0041d2f7
    G8(0x00582DD9) = (s8)GU8(FA(0x18));                 // 0x0041d2fc mov al,byte[F+0x18]; mov [0x582dd9],al
    {
        int c25 = ecx * 25;                             // 0x0041d305 lea eax,[ecx+ecx*4]; lea ecx,[eax+eax*4]
        G32(0x00582DC8) = 0;                            // 0x0041d308
        eax = G32(0x00543CB8);                          // 0x0041d315
        u8 dl = (GU8(esi + c25 * 2 + 0x005722E8) != 0) ? 1 : 0;  // 0x0041d31a mov dl,[esi+ecx*2+0x5722e8]; setne
        s8 al = G8(0x00582DD1);                         // 0x0041d328 mov al,[0x582dd1]
        G8(0x00582DDA) = (s8)dl;                        // 0x0041d32d mov [0x582dda],dl
        if (eax != 0 && (al & 4)) G32(0x00582DC8) = 0x10;  // 0x0041d326..0x0041d339
        ecx = G32(0x00543CC8);                          // 0x0041d343
        if (ecx != 0 && (al & 1)) G32(0x00582DC8) |= 4; // 0x0041d349..0x0041d351
        ecx = G32(0x00543CC0);                          // 0x0041d358
        if (ecx != 0 && (al & 2)) {                     // 0x0041d360..0x0041d364
            eax = G32(0x00582DC8); eax |= 8; G32(0x00582DC8) = eax;  // 0x0041d366
        }
    }
    edi = FD(0x2C);                                     // 0x0041d372
    esi = G32(edi + 0x00575AD4);                        // 0x0041d37c mov esi,[edi+0x575ad4]
    CC(0x00424120, 0x99, 0, -1, 0, 0);                  // 0x0041d389 prepareShot (push 0,0,-1,0,0x99)
    ebp = FD(0x0C);                                     // 0x0041d38e
    ebp = ebp * 0x388;                                  // 0x0041d397 imul ebp,ebp,0x388
    G32(edi + 0x00575AD4) = esi;                        // 0x0041d39d
    G16(ebp + 0x0059FC60) = (short)0xFFFF;              // 0x0041d3a3
    CC(0x004099F0, 0x99);                               // 0x0041d3ac addGolferPair (push 0x99)
    {
        u16 ax = GU16(0x00582E64);                      // 0x0041d3b1 mov ax,word[0x582e64]
        G32(0x005A9CC4) = 0;                            // 0x0041d3bc (edx=0)
        if (ax == 1) edx = 1;                           // 0x0041d3bc cmp ax,1; jne; mov edx,1
        else if (ax == 0xFFFF) edx = -1;                // 0x0041d3cf cmp ax,0xffff; jne; or edx,-1
        else edx = 0;                                   // (edx stays 0)
    }
    {
        int ax1, cx1;
        if (G32(0x00582DC8) & 0x10000000) {             // 0x0041d3d8 test; je 0x41d3f7
            ax1 = G32(0x00582E84) << 0xA;               // 0x0041d3e2..0x0041d3ef
            cx1 = G32(0x00582E88) << 0xA;               // 0x0041d3f2
        } else {
            ax1 = (G32(0x00582E84) << 0xA) + 0x200;     // 0x0041d3f7..0x0041d403
            cx1 = (G32(0x00582E88) << 0xA) + 0x200;     // 0x0041d400..0x0041d408
        }
        CC(0x004226A0, 0x99, ax1, cx1, edx);            // 0x0041d416 trialShot (push edx,cx1,ax1,0x99)
    }
    esi = FD(0x0C);                                     // 0x0041d41b
    edx = FD(0xBC);                                     // 0x0041d41f
    // c3fya fix: 0x0041d42d `imul esi, esi, 0xe2` keeps the product in esi, and the point loop below indexes
    // [(esi + edi) * 4 + 0x59fce8 / 0x59fc68] with it (0x0041d580). The old code multiplied only inside this expression,
    // so the loop read hole k's trajectory points at index k + edi instead of k * 0xe2 + edi (the '.' / '/' routing render).
    esi *= 0xE2;                                        // 0x0041d42d
    {
        int idx = ((int)G16(ebp + 0x0059FC64) - 1) + esi;          // 0x0041d426..0x0041d435
        idx <<= 2;                                      // 0x0041d43a shl eax,2
        ecx = G32(idx + 0x0059FC68);                    // 0x0041d43e
        eax = G32(idx + 0x0059FCE8);                    // 0x0041d444
        FD(0xA8) = ecx;                                 // 0x0041d44c
        FD(0x98) = eax;                                 // 0x0041d453
        CC(0x00407C60, ecx, eax, ebx + 5, edx);         // 0x0041d45a drawCircle (push edx,ebx+5,eax,ecx)
    }
    {
        eax = FD(0x2C);                                 // 0x0041d45f
        int c = G32(eax + 0x00575ACC);                  // 0x0041d463
        int d = G32(eax + 0x00575AC8);                  // 0x0041d469
        eax = CC(0x0040C4B0, FD(0xA8), FD(0x98), d, c); // 0x0041d481 tileDistance (push c,d,[F+0x98],[F+0xa8])
    }
    ecx = FD(0x1A8 + 4 * ebx) + eax;                    // 0x0041d486..0x0041d494
    FD(0x1A8 + 4 * ebx) = ecx;                          // 0x0041d49f
    eax = CC(0x0040BFA0, FD(0xA8), FD(0x98));           // 0x0041d4a6 typeAtPos (push [F+0x98],[F+0xa8])
    {
        int off = (eax * 3) << 4;                       // 0x0041d4ab lea ecx,[eax+eax*2]; shl ecx,4
        edi = (int)G8(off + 0x00578372);                // 0x0041d4ba movsx edi,byte
        int v = FD(0x1A8 + 4 * ebx) + (edi * 5) * 2;    // 0x0041d4c8..0x0041d4d3
        FD(0x1A8 + 4 * ebx) = v;                         // 0x0041d4dd
        eax = CC(0x0042FB90, FD(0xA8), FD(0x98), FA(0xA8), FA(0x98), 0);  // 0x0041d4ee worldToScreen
    }
    if (eax == 0) goto a41d56b;                         // 0x0041d4f6 test eax; je
    if (edi > 0) goto a41d547;                          // 0x0041d4fa test edi; jg (signed)
    TC(0x00473E60, 0x005791C8, Root(), FD(0xA8), FD(0x98), 0);   // 0x0041d51c Image_draw
    TC(0x00473E60, 0x0058B650, Root(), FD(0xA8), FD(0x98) - 1, 0);  // 0x0041d540 Image_draw
    goto a41d56b;                                       // 0x0041d545
a41d547:
    TC(0x00478B80, 0x00519CD4, FD(0xA8), FD(0x98), FD(0xA8) + 1, FD(0x98), 0x80007FFF);  // 0x0041d561 Widget_fillRect
a41d56b:
    edi = 0;                                            // 0x0041d56b xor edi,edi
    G32(0x005A9CC4) = 0;                                // 0x0041d574
    if ((short)GU16(ebp + 0x0059FC64) <= 0) goto a41d608;  // 0x0041d56d cmp word[ebp+0x59fc64],di; jle
a41d580:
    {
        int idx = (esi + edi) << 2;                     // 0x0041d580 lea eax,[esi+edi]; shl eax,2
        ecx = G32(idx + 0x0059FCE8);                    // 0x0041d597
        edx = G32(idx + 0x0059FC68);                    // 0x0041d59e
        eax = CC(0x0042FB90, edx, ecx, FA(0x178), FA(0x17C), 0);  // 0x0041d5a6 worldToScreen (push 0,FA(0x17c),FA(0x178),ecx,edx)
    }
    if (eax == 0) goto a41d5f8;                         // 0x0041d5ae test; je
    eax = FD(0x17C) - ebx;                              // 0x0041d5b2 mov eax,[F+0x17c]; sub eax,ebx
    FD(0x17C) = eax;                                    // 0x0041d5bd
    if (edi != 0) goto a41d5da;                         // 0x0041d5bb test edi; jne
    eax += 1;                                           // 0x0041d5c6 inc eax
    CC(0x0040BF00, FD(0x178), eax);                     // 0x0041d5d0 setLinePoint (push eax; push [F+0x178])
    goto a41d5f8;                                       // 0x0041d5d8
a41d5da:
    eax += 1;                                           // 0x0041d5ec inc eax
    CC(0x0040BF20, FD(0x178), eax, FD(0xBC), 1, 7);     // 0x0041d5f0 lineTo (push 7,1,[F+0xbc],eax,[F+0x178])
a41d5f8:
    eax = (int)G16(ebp + 0x0059FC64);                   // 0x0041d5f8 movsx
    edi++;                                              // 0x0041d5ff
    if (edi < eax) goto a41d580;                         // 0x0041d600 cmp; jl (signed)
a41d608:
    esi = FD(0x0C);                                     // 0x0041d608
    G16(ebp + 0x0059FC60) = (short)0x98;                // 0x0041d60c mov word[ebp+0x59fc60],0x98
    if (esi != 0xB) goto a41d893;                        // 0x0041d615 cmp esi,0xb; jne
    // ---- 0x0041d61e  (esi==0xb) routing summary text ----
    StrCpy(kBuf, 0x004C5DE4);                           // 0x0041d61e..0x0041d652 (push 0x800003ff,0x22,0x19a,kBuf)
    CC(0x00404BC0, (int)kBuf, 0x19A, 0x22, 0x800003FF); // 0x0041d654 drawCenteredText2
    StrCpy(kBuf, 0x004C5DDC);                           // 0x0041d659..0x0041d67e
    StrCat(kBuf, 0x004C5DD0);                           // 0x0041d680..0x0041d6a9
    StrCpy(kBuf, 0x004C5DC8);                           // 0x0041d6ab..0x0041d6ce
    StrCat(kBuf, 0x004C5DB8);                           // 0x0041d6d0..0x0041d705 (push 0xa,0x58a528,eax)
    eax = Itoa(FD(0x18), 0x0058A528, 0xA);              // 0x0041d707 __itoa([F+0x18])
    StrCat(kBuf, (u32)eax);                             // 0x0041d70c..0x0041d73a (push eax x3)
    StrCat(kBuf, 0x004C5DB4);                           // 0x0041d73c..0x0041d76a (push 0x51b360)
    TC(0x004762D0, Root(), 0x0051B360, 0, 0, 0);        // 0x0041d772 setDrawTarget
    CC(0x00404BC0, (int)kBuf, 0x19A, 0x32, 0x800003FF); // 0x0041d788 drawCenteredText2
    ebx = FD(0x1B4);                                    // 0x0041d78d
    ebp = 0;                                            // 0x0041d794 xor ebp,ebp
a41d799:
    {
        edx = FD(0x1A8 + 4 * ebp);                      // 0x0041d799
        u32 src = 0x004C5340;                           // 0x0041d7a0
        if (edx >= ebx) src = 0x004E9A84;               // 0x0041d7a5 cmp edx,ebx; jl keep; else
        StrCpy(kBuf, src);                              // 0x0041d7ae..0x0041d7d8 (push 0xa,0x58a528)
        int val = (ebx - edx) / 3;                      // 0x0041d7da..0x0041d7e5 (imul 0x55555556, signed /3)
        eax = Itoa(val, 0x0058A528, 0xA);               // 0x0041d7e8 __itoa
    }
    StrCat(kBuf, (u32)eax);                             // 0x0041d7ed..0x0041d815
    StrCat(kBuf, 0x004C6194);                           // 0x0041d817..0x0041d84d (push [F+ebp*4+0x290])
    CC(0x00404BC0, (int)kBuf, 0x1EA, 0x93 - (ebp & 3) * 20, FD(0x290 + 4 * ebp));  // 0x0041d867 drawCenteredText2
    ebp++;                                              // 0x0041d86f
    if (ebp < 3) goto a41d799;                          // 0x0041d870 cmp ebp,3; jl
    TC(0x004762D0, Root(), 0x00519928, 0, 0, 0);        // 0x0041d88a setDrawTarget
    esi = FD(0x0C);                                     // 0x0041d88f
a41d893:
    TC(0x00480C80, 0x00519A60, 0);                      // 0x0041d89a Window::refreshFull (push 0)
    // c3fya fix: the test at 0x0041d89f / 0x0041d8a2 (`cmp esi, 0xb; jne 0x41d8eb`) comes BEFORE pumpAndRedraw and the
    // blit back: both run only after the last hole (esi == 0xb). The old code ran them for each of the 12 holes, so the
    // routing render ('.' / '/') pumped input 11 more times and blitted the saved screen back over each hole's drawing.
    if (esi != 0xB) goto a41d8eb;                        // 0x0041d89f cmp esi,0xb; 0x0041d8a2 jne
    CC(0x0045C0C0, 0);                                  // 0x0041d8a6 pumpAndRedraw (push 0)
    TC(0x00475D00, FA(0x624), Root(), 0, 0, 0x320, 0x258, 0, 0, 0x320, 0x258);  // 0x0041d8d7 Surface_blit3
    CC(0x00483D30);                                     // 0x0041d8dc enterModal483d30
    G32(0x00822D68) = 0;                                // 0x0041d8e1
a41d8eb:
    esi++;                                              // 0x0041d8eb
    FD(0x0C) = esi;                                     // 0x0041d8ef
    if (esi < 0xC) goto a41d11d;                         // 0x0041d8ec/0x0041d8f3 cmp esi,0xc; jl
    {
        int p = 0x0059FC60;                             // 0x0041d8f9
        do {                                            // 0x0041d8fe
            G16(p) = (short)0xFFFF;
            p += 0x388;                                 // 0x0041d903
        } while (p < 0x005A34E0);                       // 0x0041d908 cmp; jl (signed)
    }
    FUB(0x124C) = 0;                                    // 0x0041d916 mov byte[F+0x124c],0
    TC(0x00474C40, FA(0x624));                          // 0x0041d91e Image_dtor
    ebp = 0;                                            // 0x0041d923 xor ebp,ebp
    ebx = 1;                                            // 0x0041d925 mov ebx,1
    goto a41de11;                                       // 0x0041d92a
a41d92f:  // key 0x3f
    eax = G32(0x0053DF54);                              // 0x0041d92f
    if (eax <= ebp) goto a41d981;                       // 0x0041d934 cmp eax,ebp; jle (signed)
    {
        int idx = eax << 8;                             // 0x0041d93b shl eax,8
        int d = (int)G16(idx + 0x0057946E);             // 0x0041d93f movsx
        CC(0x004385D0, d, eax - 1);                     // 0x0041d947 showCharacterEditor (push eax-1; push d)
    }
    eax = G32(0x0053DF54);                              // 0x0041d94c
    {
        int idx = eax << 8;                             // 0x0041d954 shl eax,8
        int d = (int)G16(idx + 0x0057946E);             // 0x0041d957 movsx
        int c = (d * 7 * 5) << 4;                       // 0x0041d95e lea[edx*8];sub;lea[*5];shl4 = d*560
        G16(idx + 0x00579470) = (short)(s8)G8(c + 0x004D60A9);  // 0x0041d96d movsx dx,byte; store word
    }
    goto a41de11;                                       // 0x0041d97c
a41d981:
    G8(0x00569498) = 1;                                 // 0x0041d981
    G32(0x005A7144) = 0x80;                             // 0x0041d988
    goto a41de11;                                       // 0x0041d992
a41d997:  // key 0x2d: lower terrain cell
    CC(0x004481B0, 0xC8, 0x64, ebp, ebp, ebp);          // 0x0041d9a1 playSound
    {
        int t = G32(0x0059BF90) * 23;                   // 0x0041d9a6 ecx = eax*23
        u8 b = GU8(t * 2 + 0x00571FF6);                 // 0x0041d9bb mov dl,byte[ecx*2+0x571ff6]
        edx = (b == 2) ? 0xD : 0xA;                     // 0x0041d9c2..0x0041d9cc (sub2;neg;sbb;and;add)
    }
    eax = G32(0x00542F20);                              // 0x0041d9b6 (loaded before)
    if (eax == ebx) goto a41db00;                       // 0x0041d9cf cmp eax,ebx; je
    {
        ecx = G32(0x00543D08);                          // 0x0041d9d7
        int cell = ecx * 17;                            // 0x0041d9de..0x0041d9e5
        int row = G32(0x00543D0C);                      // 0x0041d9e7
        int p = cell + (row + cell * 2) + 0x005A4998;   // 0x0041d9ed..0x0041d9f0
        u8 cl = (u8)(GU8(p) - 1);                        // 0x0041d9f7 dec cl
        G8(p) = (s8)cl;                                 // 0x0041d9fb
        eax = CC(0x00467130, cl & 0xFF, 3, edx);        // 0x0041da05 clamp (push edx,3,eax)
    }
    edx = G32(0x00543D08);                              // 0x0041da0a
    edi = G32(0x00543D0C);                              // 0x0041da10
    {
        int c = edx * 17;                               // 0x0041da16..0x0041da1e
        G8(c + (edi + c * 2) + 0x005A4998) = (s8)eax;    // 0x0041da23
    }
    eax = G32(0x00542F20);                              // 0x0041da2a
    if (eax != ebx) goto a41dadd;                       // 0x0041da31 cmp eax,ebx; jne
    ebp = 0;                                            // 0x0041da37 xor ebp,ebp
    goto a41da39_lower;
a41db00:
    edx = G32(0x00543D08);                              // 0x0041db00
    edi = G32(0x00543D0C);                              // 0x0041db06
a41da39_lower:
    {
        esi = edx + 1;                                  // 0x0041da39 lea esi,[edx+1]
        FD(0x0C) = ebp;                                 // 0x0041da3c
        if (edx > esi) goto a41dadd;                    // 0x0041da40 cmp; jg (never)
        int cell = edx * 17;                            // 0x0041da48..0x0041da4f
        ebx = (esi - edx) + 1;                          // 0x0041da51 sub ebx,edx; inc ebx (=2)
        ebp = cell * 3 + 0x005A4998;                    // 0x0041da54 lea ebp,[eax+eax*2+0x5a4998]
        do {                                            // 0x0041da5b outer (find max)
            eax = edi - 1;                              // 0x0041da5b lea eax,[edi-1]
            if (eax <= edi) {                           // 0x0041da5e cmp; jg skip
                do {
                    ecx = GU8(eax + ebp);               // 0x0041da64 movzx cl
                    if (ecx > FD(0x0C)) FD(0x0C) = ecx; // 0x0041da67 cmp;jle;mov
                    eax++;                              // 0x0041da71
                } while (eax <= edi);                   // 0x0041da72 cmp; jle
            }
            ebp += 0x33;                                // 0x0041da76
            ebx--;                                      // 0x0041da79
        } while (ebx != 0);                             // 0x0041da7a jne
        if (edx > esi) goto a41dadd;                    // 0x0041da7c cmp; jg
        int c2 = edx * 17;                              // 0x0041da80
        FD(0x18) = (esi - edx) + 1;                     // 0x0041da82 sub esi,edx; inc esi
        ebx = edi - 1;                                  // 0x0041da8a lea ebx,[edi-1]
        FD(0x2C) = c2 * 3;                              // 0x0041da91 lea eax,[eax+eax*2]
        do {                                            // 0x0041da98 outer2 (decrement max cells)
            if (ebx <= edi) {                           // 0x0041da98 cmp; jg skip
                esi = (edi - ebx) + 1;                  // 0x0041da9c sub esi,ebx; inc esi
                ecx = ebx + FD(0x2C) + 0x005A4998;      // 0x0041da9e lea ecx
                do {                                    // 0x0041daa8 inner2
                    u8 al = GU8(ecx);                   // 0x0041daa8 mov al,[ecx]
                    if ((al & 0xFF) == FD(0x0C) && FD(0x0C) > 3) {  // 0x0041daac..0x0041dabd
                        al = (u8)(al - 1);              // 0x0041dabf dec al
                        G8(ecx) = (s8)al;               // 0x0041dac1
                    }
                    ecx++;                              // 0x0041dac3
                    esi--;                              // 0x0041dac4
                } while (esi != 0);                     // 0x0041dac5 jne
            }
            FD(0x2C) += 0x33;                           // 0x0041dac7..0x0041dad3
            FD(0x18) -= 1;                              // 0x0041dacb mov ecx,[F+0x18]; 0x0041dad2 dec ecx; 0x0041dad7 store
            // c3fya fix: the row counter lives in ecx / [F+0x18] only; ebx keeps the start column edi - 1 (0x0041da8a)
            // for every row (0x0041da98 cmp ebx, edi). The old `ebx = FD(0x18)` made rows 2.. start at column 1, so the
            // brush-1 lower ('-' / right click with [0x542f20] == 1) lowered up to edi cells of the level table 0x5a4998.
        } while (FD(0x18) != 0);                        // 0x0041dadb jne 0x41da98 (flags of dec ecx)
    }
a41dadd:
    if (G32(0x00542F20) == 2) {                         // 0x0041dadd cmp; jne 0x41dafc
        CC(0x00406F90, edx, edi);                       // 0x0041dae8 (push edi; push edx)
        edx = G32(0x00543D08);                          // 0x0041daed
        edi = G32(0x00543D0C);                          // 0x0041daf3
    }
    esi = 0;                                            // 0x0041dafc xor esi,esi
    goto a41db17;                                       // 0x0041dafe
a41db11:
    edi = G32(0x00543D0C);                              // 0x0041db11
a41db17:
    do {                                                // 0x0041db17 interp loop
        eax = G32(esi + 0x004C2898) + edi;              // 0x0041db17..0x0041db23
        ecx = G32(esi + 0x004C2878) + edx;              // 0x0041db1d..0x0041db27
        CC(0x0040C170, ecx, eax, 0);                    // 0x0041db2b heightBlend (push 0,eax,ecx)
        edx = G32(0x00543D08);                          // 0x0041db30
        esi += 8;                                       // 0x0041db36
        if (esi < 0x20) goto a41db11;                   // 0x0041db3c cmp; jl
    } while (0);
    goto a41dcd0;                                       // 0x0041db41
a41db46:  // key 0x3d: raise terrain cell
    CC(0x004481B0, 0xC7, 0x64, ebp, ebp, ebp);          // 0x0041db50 playSound
    eax = G32(0x00542F20);                              // 0x0041db55
    if (eax == ebx) goto a41dbc9;                       // 0x0041db5d cmp eax,ebx; je
    {
        ecx = G32(0x00543D08);                          // 0x0041db61
        int cell = ecx * 17;                            // 0x0041db69..0x0041db70
        int row = G32(0x00543D0C);                      // 0x0041db72
        int p = cell + (row + cell * 2) + 0x005A4998;   // 0x0041db78..0x0041db84
        u8 cl = (u8)(GU8(p) + 1);                        // 0x0041db7b inc cl
        G8(p) = (s8)cl;                                 // 0x0041db8b
        eax = CC(0x00467130, cl & 0xFF, 3, 0xD);        // 0x0041db95 clamp (push 0xd,3,eax)
    }
    edx = G32(0x00543D08);                              // 0x0041db9a
    edi = G32(0x00543D0C);                              // 0x0041dba0
    {
        int c = edx * 17;                               // 0x0041dba6..0x0041dbae
        G8(c + (edi + c * 2) + 0x005A4998) = (s8)eax;    // 0x0041dbb3
    }
    if (G32(0x00542F20) != ebx) goto a41dc7d;           // 0x0041dbba cmp; jne
    goto a41dbd5;                                       // 0x0041dbc7
a41dbc9:
    edx = G32(0x00543D08);                              // 0x0041dbc9
    edi = G32(0x00543D0C);                              // 0x0041dbcf
a41dbd5:
    {
        esi = edx + 1;                                  // 0x0041dbd5 lea esi,[edx+1]
        FD(0x0C) = 0x63;                                // 0x0041dbd8
        if (edx > esi) goto a41dc7d;                    // 0x0041dbe0 cmp; jg
        int cell = edx * 17;                            // 0x0041dbe8..0x0041dbef
        ebx = (esi - edx) + 1;                          // 0x0041dbf1 (=2)
        ebp = cell * 3 + 0x005A4998;                    // 0x0041dbf4
        do {                                            // 0x0041dbfb outer (find min)
            eax = edi - 1;                              // 0x0041dbfb
            if (eax <= edi) {                           // 0x0041dbfe cmp; jg
                do {
                    ecx = GU8(eax + ebp);               // 0x0041dc04 movzx cl
                    if (ecx < FD(0x0C)) FD(0x0C) = ecx; // 0x0041dc07 cmp;jge;mov
                    eax++;                              // 0x0041dc11
                } while (eax <= edi);                   // 0x0041dc12 cmp; jle
            }
            ebp += 0x33;                                // 0x0041dc16
            ebx--;                                      // 0x0041dc19
        } while (ebx != 0);                             // 0x0041dc1a jne
        if (edx > esi) goto a41dc7d;                    // 0x0041dc1c cmp; jg
        int c2 = edx * 17;                              // 0x0041dc20
        FD(0x18) = (esi - edx) + 1;                     // 0x0041dc22
        ebx = edi - 1;                                  // 0x0041dc2a
        FD(0x2C) = c2 * 3;                              // 0x0041dc31
        do {                                            // 0x0041dc38 outer2 (increment min cells)
            if (ebx <= edi) {                           // 0x0041dc38 cmp; jg
                esi = (edi - ebx) + 1;                  // 0x0041dc3c
                ecx = ebx + FD(0x2C) + 0x005A4998;      // 0x0041dc3e
                do {                                    // 0x0041dc48
                    u8 al = GU8(ecx);                   // 0x0041dc48
                    if ((al & 0xFF) == FD(0x0C) && FD(0x0C) < 0xD) {  // 0x0041dc4c..0x0041dc5d
                        al = (u8)(al + 1);              // 0x0041dc5f inc al
                        G8(ecx) = (s8)al;               // 0x0041dc61
                    }
                    ecx++;                              // 0x0041dc63
                    esi--;                              // 0x0041dc64
                } while (esi != 0);                     // 0x0041dc65 jne
            }
            FD(0x2C) += 0x33;                           // 0x0041dc67..0x0041dc73
            FD(0x18) -= 1;                              // 0x0041dc6b mov ecx,[F+0x18]; 0x0041dc72 dec ecx; 0x0041dc77 store
            // c3fya fix: as in the lower loop (0x0041dadb): ebx stays the start column edi - 1 (0x0041dc2a); only
            // ecx / [F+0x18] counts the rows.
        } while (FD(0x18) != 0);                        // 0x0041dc7b jne 0x41dc38 (flags of dec ecx)
    }
a41dc7d:
    if (G32(0x00542F20) == 2) {                         // 0x0041dc7d cmp; jne
        CC(0x00406F20, edx, edi);                       // 0x0041dc88 (push edi; push edx)
        edx = G32(0x00543D08);                          // 0x0041dc8d
        edi = G32(0x00543D0C);                          // 0x0041dc93
    }
    esi = 0;                                            // 0x0041dc9c xor esi,esi
    goto a41dca6;                                       // 0x0041dc9e
a41dca0:
    edi = G32(0x00543D0C);                              // 0x0041dca0
a41dca6:
    do {                                                // 0x0041dca6 interp loop
        eax = G32(esi + 0x004C2898) + edi;              // 0x0041dca6..0x0041dcb2
        ecx = G32(esi + 0x004C2878) + edx;              // 0x0041dcac..0x0041dcb6
        CC(0x0040C170, ecx, eax, 0);                    // 0x0041dcba heightBlend (push 0,eax,ecx)
        edx = G32(0x00543D08);                          // 0x0041dcbf
        esi += 8;                                       // 0x0041dcc5
        if (esi < 0x20) goto a41dca0;                   // 0x0041dcce cmp; jl
    } while (0);
a41dcd0:
    eax = G32(0x00543D0C);                              // 0x0041dcd0
    G32(0x004C2E04) = edx;                              // 0x0041dcd5
    G32(0x005A47E4) = eax;                              // 0x0041dcdb
    CC(0x0042F7A0);                                     // 0x0041dce0 rebuildHeightfield
    ebp = 0;                                            // 0x0041dce5 xor ebp,ebp
    ebx = 1;                                            // 0x0041dce7 mov ebx,1
    goto a41de11;                                       // 0x0041dcec
a41dcf1: esi = ebx;   goto a41dd6a;                     // 0x0041dcf1 key 0x31
a41dcf5: esi = 2;     goto a41dd6a;                     // 0x0041dcf5 key 0x32
a41dcfc: esi = 3;     goto a41dd6a;                     // 0x0041dcfc key 0x33
a41dd03: esi = 4;     goto a41dd6a;                     // 0x0041dd03 key 0x34
a41dd0a: esi = 5;     goto a41dd6a;                     // 0x0041dd0a key 0x35
a41dd11: esi = 6;     goto a41dd6a;                     // 0x0041dd11 key 0x36
a41dd18: esi = 7;     goto a41dd6a;                     // 0x0041dd18 key 0x37
a41dd1f: esi = 8;     goto a41dd6a;                     // 0x0041dd1f key 0x38
a41dd26: esi = 9;     goto a41dd6a;                     // 0x0041dd26 key 0x39
a41dd2d: esi = 0xA;   goto a41dd6a;                     // 0x0041dd2d key 0x21
a41dd34: esi = 0xB;   goto a41dd6a;                     // 0x0041dd34 key 0x40
a41dd3b: esi = 0xC;   goto a41dd6a;                     // 0x0041dd3b key 0x23
a41dd42: esi = 0xD;   goto a41dd6a;                     // 0x0041dd42 key 0x24
a41dd49: esi = 0xE;   goto a41dd6a;                     // 0x0041dd49 key 0x25
a41dd50: esi = 0xF;   goto a41dd6a;                     // 0x0041dd50 key 0x5e
a41dd57: esi = 0x10;  goto a41dd6a;                     // 0x0041dd57 key 0x26
a41dd5e: esi = 0x11;  goto a41dd6a;                     // 0x0041dd5e key 0x2a
a41dd65: esi = 0x12;                                    // 0x0041dd65 key 0x28
a41dd6a:  // select hole esi, center camera
    edi = esi * 0x208;                                  // 0x0041dd6a shl6;add;shl3
    if (G8(edi + 0x00575AB0) == 0) goto a41de11;        // 0x0041dd74 mov al,byte; test; je
    eax = G32(edi + 0x00575ACC);                        // 0x0041dd82
    G32(0x0053DF54) = ebp;                              // 0x0041dda0
    eax = CC(0x0042F940, G32(edi + 0x00575AC8), eax, FA(0x280), FA(0x288));  // 0x0041dda6 tileToScreen
    if (eax != 0) {                                     // 0x0041ddae test; je
        esi = -esi;                                     // 0x0041ddb2 neg esi
        G32(0x0053DF54) = esi;                          // 0x0041ddb4
    }
    {
        int ac8 = G32(edi + 0x00575AC8);                // 0x0041ddba..0x0041ddc0
        int acc = G32(edi + 0x00575ACC);                // 0x0041ddc8
        G32(0x004C2BA0) = (G32(edi + 0x00575AB8) + ac8) / 2;   // 0x0041ddba..0x0041ddd3 (cdq;sub;sar1)
        G32(0x004C2BA4) = (G32(edi + 0x00575ABC) + acc) / 2;   // 0x0041ddd8..0x0041dde5
    }
    goto a41de11;                                       // 0x0041ddea
a41ddec:  // key 0x57
    eax = G32(0x0059E7B8);                              // 0x0041ddec
    if (eax & 0x4000000) {                              // 0x0041ddf2 test; jne 0x41de03
        CC(0x004481B0, 0x18, 0x64, ebp, ebp, ebp);      // 0x0041de09 playSound (push ebp,ebp,ebp,0x64,0x18)
    } else {
        CC(0x00407D30, ebp);                            // 0x0041ddf9 (push ebp)
    }
    // falls into a41de11
a41de11:
    // ---- 0x0041de11  second (report/view) key switch on [F+0x28] ----
    edx = FD(0x28);                                    // 0x0041de11
    eax = edx - 0x21;                                  // 0x0041de15
    if ((u32)eax > 0x58) goto a41e070;                 // 0x0041de18 cmp eax,0x58; ja
    ecx = GU8(eax + 0x004219F4);                       // 0x0041de23
    switch (ecx) {                                     // 0x0041de29 jmp [ecx*4+0x4219b0]
        case 0:  goto a41e042; case 1:  goto a41e054; case 2:  goto a41df11;
        case 3:  goto a41deff; case 4:  goto a41df03; case 5:  goto a41df0a;
        case 6:  goto a41de48; case 7:  goto a41de61; case 8:  goto a41de79;
        case 9:  goto a41de92; case 10: goto a41de30; case 11: goto a41de3a;
        case 12: goto a41deab; case 13: goto a41dec3; case 14: goto a41dedc;
        case 15: goto a41def5; default: goto a41e070;
    }
a41de30:  // 0x74 routing map screen
    CC(0x00456BE0);                                    // 0x0041de30 showRoutingMapScreen
    goto a41e070;                                      // 0x0041de35
a41de3a:  // 0x75
    CC(0x00407D30, ebp);                               // 0x0041de3b (push ebp)
    goto a41e070;                                      // 0x0041de43
a41de48:  // 0x70 report-mode toggle to 2
    G8(0x00569498) = (s8)((GU8(0x00569498) == 2) ? 0 : 2);  // 0x0041de48..0x0041de56
    goto a41e070;                                      // 0x0041de5c
a41de61:  // 0x71 toggle 3
    G8(0x00569498) = (s8)((GU8(0x00569498) == 3) ? 0 : 3);  // 0x0041de61..0x0041de6f
    goto a41e070;                                      // 0x0041de74
a41de79:  // 0x72 toggle 4
    G8(0x00569498) = (s8)((GU8(0x00569498) == 4) ? 0 : 4);  // 0x0041de79..0x0041de87
    goto a41e070;                                      // 0x0041de8d
a41de92:  // 0x73 toggle 6
    G8(0x00569498) = (s8)((GU8(0x00569498) == 6) ? 0 : 6);  // 0x0041de92..0x0041dea0
    goto a41e070;                                      // 0x0041dea6
a41deab:  // 0x76 toggle 9
    G8(0x00569498) = (s8)((GU8(0x00569498) == 9) ? 0 : 9);  // 0x0041deab..0x0041deb9
    goto a41e070;                                      // 0x0041debe
a41dec3:  // 0x77 toggle 5
    G8(0x00569498) = (s8)((GU8(0x00569498) == 5) ? 0 : 5);  // 0x0041dec3..0x0041ded1
    goto a41e070;                                      // 0x0041ded7
a41dedc:  // 0x78 toggle 0xa
    G8(0x00569498) = (s8)((GU8(0x00569498) == 0xA) ? 0 : 0xA);  // 0x0041dedc..0x0041deea
    goto a41e070;                                      // 0x0041def0
a41def5:  // 0x79
    CC(0x0046F180);                                    // 0x0041def5
    goto a41e070;                                      // 0x0041defa
a41deff: eax = 0; goto a41df16;                        // 0x0041deff key 0x26
a41df03: eax = 2; goto a41df16;                        // 0x0041df03 key 0x27
a41df0a: eax = 4; goto a41df16;                        // 0x0041df0a key 0x28
a41df11: eax = 6;                                      // 0x0041df11 key 0x25
a41df16:
    eax = (eax + G32(0x005685F4)) & 7;                 // 0x0041df16..0x0041df1e
    if ((u32)eax > 6) goto a41e070;                    // 0x0041df21 cmp eax,6; ja
    switch (eax) {                                     // 0x0041df2a jmp [eax*4+0x421a50]
        case 0: goto a41df31; case 2: goto a41df77;
        case 4: goto a41dfbc; case 6: goto a41dfff;
        default: goto a41e070;                         // cases 1,3,5
    }
a41df31:  // rotate to dir 0 -> step camera +x, -y
    G32(0x004C2BA0) = CC(0x00467130, G32(0x004C2BA0) + 4, ebp, 0x31);  // 0x0041df31..0x0041df51
    G32(0x004C2BA4) = CC(0x00467130, G32(0x004C2BA4) - 4, ebp, 0x31);  // 0x0041df56..0x0041df65
    goto a41e070;                                      // 0x0041df72
a41df77:  // dir 2 -> +x, +y
    G32(0x004C2BA0) = CC(0x00467130, G32(0x004C2BA0) + 4, ebp, 0x31);  // 0x0041df77..0x0041df97
    G32(0x004C2BA4) = CC(0x00467130, G32(0x004C2BA4) + 4, ebp, 0x31);  // 0x0041df9c..0x0041dfaa
    goto a41e070;                                      // 0x0041dfb7
a41dfbc:  // dir 4 -> -x, +y
    G32(0x004C2BA0) = CC(0x00467130, G32(0x004C2BA0) - 4, ebp, 0x31);  // 0x0041dfbc..0x0041dfdc
    G32(0x004C2BA4) = CC(0x00467130, G32(0x004C2BA4) + 4, ebp, 0x31);  // 0x0041dfe1..0x0041dfef
    goto a41e070;                                      // 0x0041dffd
a41dfff:  // dir 6 -> -x, -y
    G32(0x004C2BA0) = CC(0x00467130, G32(0x004C2BA0) - 4, ebp, 0x31);  // 0x0041dfff..0x0041e01f
    G32(0x004C2BA4) = CC(0x00467130, G32(0x004C2BA4) - 4, ebp, 0x31);  // 0x0041e024..0x0041e033
    goto a41e070;                                      // 0x0041e040
a41e042:  // key 0x21: rotate +2
    G32(0x005685F4) = (G32(0x005685F4) + 2) & 7;       // 0x0041e042..0x0041e04d
    goto a41e066;                                      // 0x0041e052
a41e054:  // key 0x24: rotate -2
    G32(0x005685F4) = (G32(0x005685F4) - 2) & 7;       // 0x0041e054..0x0041e060
a41e066:
    G32(0x00543D04) = -1;                              // 0x0041e066
a41e070:
    // ---- 0x0041e070  per-frame overlays and report screens ----
    CC(0x0045FD80);                                    // 0x0041e070 buildScenarioIntro
    CC(0x0040C7A0);                                    // 0x0041e075 drawLabels
    CC(0x0040C910);                                    // 0x0041e07a drawMoneyLabels
    if (GU8(0x00569498) == 1 || G32(0x005A9CCC) != ebp)  // 0x0041e07f cmp byte,1; je; cmp [0x5a9ccc],ebp; je
        CC(0x0040D6A0);                                // 0x0041e090 tickMessage
    if (GU8(0x00569498) == 2) CC(0x0044FB30, ebp);     // 0x0041e095 showCourseReport(0) (push ebp)
    if (GU8(0x00569498) == 3) CC(0x004546B0);          // 0x0041e0b0 showPlayerCommentsReport
    if (GU8(0x00569498) == 7) CC(0x00459850);          // 0x0041e0be showGolferPairSelect
    if (GU8(0x00569498) == 4) CC(0x00455ED0);          // 0x0041e0cc showHistographScreen
    if (GU8(0x00569498) == 5) CC(0x0044E770);          // 0x0041e0da showShortcutsScreen
    if (GU8(0x00569498) == 6) CC(0x0044F6B0);          // 0x0041e0e8 showFinancialReport
    if (GU8(0x00569498) == 8) CC(0x0045F870);          // 0x0041e0f6 showCourseRanking
    if (GU8(0x00569498) == 9) CC(0x0044FB30, ebx);     // 0x0041e104 showCourseReport(1) (push ebx)
    if (GU8(0x00569498) == 0xA) CC(0x00454C50);        // 0x0041e116 showMembershipRoster
    eax = G32(0x005A47E0);                             // 0x0041e11b
    esi = -2;                                          // 0x0041e120 mov esi,0xfffffffe
    // c3fyc fix: 0x0041e129 `call 0x46d200` pushes nothing (depth +4 before and after); buildTournamentPrep reads no
    // argument (its frame offsets stay below its 0x64-byte frame), so the old extra argument 0 was unobservable.
    if (eax == esi) CC(0x0046D200);                    // 0x0041e125 / 0x0041e127 jne / 0x0041e129 buildTournamentPrep()
    eax = G32(0x005A47E0);                             // 0x0041e12e
    edi = -1;                                          // 0x0041e133 or edi,0xffffffff
    if (eax == edi) {                                  // 0x0041e136 cmp eax,edi; jne
        G32(0x005A47E0) = esi;                          // 0x0041e13a (=-2)
        G32(0x0053DF54) = ebp;                          // 0x0041e140
    }
    if ((G32(0x0059E7B8) & 0x200000) && G32(0x0053DF54) == ebp)  // 0x0041e146..0x0041e158
        CC(0x0045A090);                                // 0x0041e15a showTournamentResults
    TC(0x0047AB50, 0x00519A60, FA(0x214), FA(0x1E8)); // 0x0041e174 View_toLocal (push FA(0x1e8); push FA(0x214))
    edx = FD(0x214);                                   // 0x0041e179
    eax = 0; ecx = 0;                                  // 0x0041e180/0x0041e182
    if (edx <= ebp) { eax = edi; ecx = edi; }          // 0x0041e184 cmp edx,ebp; jg skip (edi=-1)
    esi = FD(0x1E8);                                   // 0x0041e18c
    if (esi <= ebp) { eax++; ecx--; }                  // 0x0041e193 cmp esi,ebp; jg skip
    if (edx >= 0x31F) { eax++; ecx++; }                // 0x0041e199 cmp edx,0x31f; jl skip
    if (esi >= 0x257) { eax--; ecx++; }                // 0x0041e1a3 cmp esi,0x257; jl skip
    edx = G32(0x005685F4);                             // 0x0041e1ad
    if ((u32)edx > 6) goto a41e22b;                    // 0x0041e1b3 cmp edx,6; ja
    switch (edx) {                                     // 0x0041e1b8 jmp [edx*4+0x421a6c]
        case 0:  // 0x0041e1bf
            G32(0x004C2BA0) = G32(0x004C2BA0) + eax;
            G32(0x004C2BA4) = G32(0x004C2BA4) + ecx;
            goto a41e22b;
        case 2:  // 0x0041e1db
            G32(0x004C2BA0) = G32(0x004C2BA0) - ecx;
            ecx = G32(0x004C2BA4) + eax;
            goto a41e225;
        case 4:  // 0x0041e1f3
            G32(0x004C2BA0) = G32(0x004C2BA0) - eax;
            G32(0x004C2BA4) = G32(0x004C2BA4) - ecx;
            goto a41e22b;
        case 6:  // 0x0041e20f
            G32(0x004C2BA0) = G32(0x004C2BA0) + ecx;
            ecx = G32(0x004C2BA4) - eax;
            goto a41e225;
        default: goto a41e22b;                         // cases 1,3,5
    }
a41e225:
    G32(0x004C2BA4) = ecx;                             // 0x0041e225
a41e22b:
    G32(0x004C2BA0) = CC(0x00467130, G32(0x004C2BA0), ebp, 0x31);  // 0x0041e22b..0x0041e235 clamp
    G32(0x004C2BA4) = CC(0x00467130, G32(0x004C2BA4), ebp, 0x31);  // 0x0041e23a..0x0041e249 clamp
    eax = G32(0x004C2E0C);                             // 0x0041e253
    if (eax == edi) goto a41e290;                      // 0x0041e25b cmp eax,edi; je
    CC(0x004481B0, 0x95, 0x64, ebp, ebp, ebp);         // 0x0041e269 playSound
    TC(0x00480C80, 0x00519A60, ebp);                   // 0x0041e277 refreshFull (push ebp)
    G32(0x004C2E0C) = edi;                             // 0x0041e27d (=-1)
    CC(0x0045C0C0, ebp);                               // 0x0041e283 pumpAndRedraw (push ebp)
    goto a41e36c;                                      // 0x0041e28b
a41e290:
    CC(0x004A0890);                                    // 0x0041e290 emptyFunc_4a0890
    TC(0x00480C80, 0x00519A60, ebp);                   // 0x0041e29b refreshFull (push ebp)
    if (!(G8(0x00567A1C) & 1)) goto a41e366;           // 0x0041e2a0 test byte,1; je
    CC(0x004481B0, 0x38, 0x64, ebp, ebp, ebp);         // 0x0041e2b4 playSound
    CC(0x0045BF80, 0x5A, ebp);                         // 0x0041e2bc waitTicks (push ebp; push 0x5a)
    CC(0x00483D30);                                    // 0x0041e2c1 enterModal483d30
    StrCpy(kBuf, 0x004C5D74);                          // 0x0041e2c6..0x0041e2f3 (push ebp,ebx,ebx,0xa0)
    StrCat(kBuf, 0x004C5D48);                          // 0x0041e2f5..0x0041e323 (push 0x1c2)
    eax = CC(0x0046D6E0, 0x1C2, 0xA0, ebx, ebx, ebp);  // 0x0041e325 showPopup
    if (eax != 0) goto a41e363;                        // 0x0041e32d test eax; jne
    eax = CC(0x004587A0);                              // 0x0041e331 showBuyLandScreen
    if (eax != 0) G32(0x0053A450)++;                   // 0x0041e336 test; je; inc [0x53a450]
    CC(0x00449470);                                    // 0x0041e340 syncTiles
    CC(0x0042F2C0);                                    // 0x0041e345 markTileKind11
    CC(0x0042F340);                                    // 0x0041e34a rebuildTileCoverage
    G32(0x004C2E04) = 0x19;                            // 0x0041e34f..0x0041e354
    G32(0x005A47E4) = 0x19;                            // 0x0041e359
    CC(0x0042F7A0);                                    // 0x0041e35e rebuildHeightfield
a41e363:
    edi = -1;                                          // 0x0041e363 or edi,0xffffffff
a41e366:
    G32(0x00567A1C) = ebp;                             // 0x0041e366
    // falls into a41e36c
a41e36c:
    // ---- 0x0041e36c  pointer / click handling, frame wait ----
    eax = G32(0x00822D68);                             // 0x0041e36c
    if (eax == ebp) goto a41e381;                      // 0x0041e371 cmp eax,ebp; je
    if (G32(0x004C2E10) != edi) goto a41e50c;          // 0x0041e375 cmp [0x4c2e10],edi; jne
a41e381:
    if (eax != ebx) goto a41e39f;                      // 0x0041e381 cmp eax,ebx; jne
    edi = G32(0x0053DF54);                             // 0x0041e385
    if (edi <= ebp) goto a41e3a7;                      // 0x0041e38b cmp edi,ebp; jle (signed)
    eax = G32(0x00824130);                             // 0x0041e38f mov eax,[0x824130]: a41e6f9 switches on eax + 1
    if (eax != -1) goto a41e6f9;                       // 0x0041e394 cmp eax,-1; 0x0041e397 jne  (c3fy2 fix: eax was
                                                       // left as [0x822d68], so the switch took case 2, select golfer)
    goto a41e3a7;                                      // 0x0041e39d
a41e39f:
    if (eax == ebp) goto a41e631;                      // 0x0041e39f cmp eax,ebp; je
a41e3a7:
    eax = G32(0x005A9CBC);                             // 0x0041e3a7
    esi = -1;                                          // 0x0041e3ac or esi,0xffffffff
    G32(0x00543D04) = esi;                             // 0x0041e3b1
    if (eax == ebp) goto a41e3c7;                      // 0x0041e3af cmp eax,ebp; je
    G32(0x004C2844) = G32(0x004C2844) * 2;             // 0x0041e3b9 lea ecx,[eax+eax]
a41e3c7:
    ecx = G32(0x00822D64);                             // 0x0041e3c7
    edx = G32(0x00822D60);                             // 0x0041e3d7
    CC(0x00430020, edx, ecx, FA(0x1C), FA(0x24), ebp); // 0x0041e3e0 screenToTile (push ebp,FA(0x24),FA(0x1c),ecx,edx)
    eax = G32(0x005A9CBC);                             // 0x0041e3e5
    if (eax == ebp) goto a41e400;                      // 0x0041e3ed cmp eax,ebp; je
    G32(0x004C2844) = G32(0x004C2844) / 2;             // 0x0041e3f1 cdq;sub;sar1 (signed /2)
a41e400:
    ecx = G32(0x00822D64);                             // 0x0041e400
    if (ecx <= 0x1B8) goto a41e4b8;                    // 0x0041e406 cmp ecx,0x1b8; jle (signed)
    eax = G32(0x00822D60);                             // 0x0041e412
    if (eax >= 0xC8) goto a41e459;                      // 0x0041e417 cmp eax,0xc8; jge (signed)
    eax = CC(0x00432720, eax, ecx);                    // 0x0041e41f showSystemMenu (push ecx; push eax)
    FD(0x10) = eax;                                    // 0x0041e42a
    if (eax == ebp) return kFrame;                     // 0x0041e428 cmp eax,ebp; je 0x40fc29
    ebp = 0;                                           // 0x0041e434 xor ebp,ebp
    G32(0x00822D68) = ebp;                             // 0x0041e438
    if (eax >= ebp) goto a41c546;                      // 0x0041e436 cmp eax,ebp; jge (signed)
    edx = G32(0x00567AFC);                             // 0x0041e444
    FD(0x10) = ebp;                                    // 0x0041e44a
    FD(0x28) = -eax;                                   // 0x0041e44e neg eax; mov [F+0x28],eax
    goto a41c54e;                                      // 0x0041e454
a41e459:
    if (ecx <= 0x1E0) goto a41e4b8;                    // 0x0041e459 cmp ecx,0x1e0; jle (signed)
    edx = G32(0x00567AFC);                             // 0x0041e461
    if (edx == ebp) goto a41eab0;                      // 0x0041e467 cmp edx,ebp; je
    if (edx == ebx) goto a41ec8b;                      // 0x0041e46f cmp edx,ebx; je
    if (edx == 2) goto a41eca7;                        // 0x0041e477 cmp edx,2; je
    if (edx == 3) goto a41e48a;                        // 0x0041e480 cmp edx,3; je
    if (edx != 4) goto a41e4be;                        // 0x0041e485 cmp edx,4; jne
a41e48a:
    eax = CC(0x00436060, eax, ecx);                    // 0x0041e48c clickGolferPanel (push ecx; push eax)
    FD(0x10) = eax;                                    // 0x0041e496
    G32(0x00822D68) = ebp;                             // 0x0041e49a
    if (eax <= ebp) return kFrame;                     // 0x0041e4a0 cmp eax,ebp; jle 0x40fc29
    edx = G32(0x00567AFC);                             // 0x0041e4a6
    ebx = 1;                                           // 0x0041e4ac
    ebp = 0;                                           // 0x0041e4b1
    goto a41c553;                                      // 0x0041e4b3
a41e4b8:
    edx = G32(0x00567AFC);                             // 0x0041e4b8
a41e4be:
    eax = G32(0x0059E7B8);                             // 0x0041e4be
    if (!(eax & 2)) {                                  // 0x0041e4c3 test al,2; je 0x41ecbb (kNext)
        h2in[0] = eax;                                 // eax = [0x59e7b8]
        h2in[1] = ebp;                                 // ebp = 0
        h2in[2] = ebx;                                 // ebx = 1
        h2in[3] = ecx;                                 // ecx = [0x822d64] (0x41e400)
        h2in[4] = edx;                                 // edx = [0x567afc]
        h2in[5] = esi;                                 // esi = -1 (0x41e3ac)
        return kNext;
    }
    eax = G32(0x00822D68);                             // 0x0041e4cb
    if (eax == ebx) {                                  // 0x0041e4d0 cmp eax,ebx; jne
        FD(0x10) = 0x3D;                               // 0x0041e4d4
        goto a41e4eb;                                  // 0x0041e4dc
    }
    if (eax == 2) FD(0x10) = 0x2D;                     // 0x0041e4de cmp eax,2; jne; mov [F+0x10],0x2d
a41e4eb:
    eax = G32(0x00542F20);                             // 0x0041e4eb
    G32(0x00822D68) = ebp;                             // 0x0041e4f0
    if (eax != 3) goto a41c553;                        // 0x0041e4f6 cmp eax,3; jne
    FD(0x10) = 0x2F;                                   // 0x0041e4ff
    goto a41c553;                                      // 0x0041e507
a41e50c:
    if (eax != ebx) goto a41e624;                      // 0x0041e50c cmp eax,ebx; jne
    eax = CC(0x0040BF60, G32(0x00543D08), G32(0x00543D0C));  // 0x0041e521 tileBlocked (push [0x543d0c]; push [0x543d08])
    if (eax == 0) goto a41e541;                        // 0x0041e529 test eax; je
    CC(0x004481B0, 0x18, 0x64, ebp, ebp, ebp);         // 0x0041e534 playSound
    goto a41e62a;                                      // 0x0041e53c
a41e541:
    eax = G32(0x004C2E10);                             // 0x0041e541
    if (eax >= 0x80) goto a41e57a;                     // 0x0041e546 cmp eax,0x80; jge (signed)
    {
        s8 cl = G8(0x00543D08);                        // 0x0041e54d mov cl,byte[0x543d08]
        int a2 = (eax + eax * 8) * 2;                  // 0x0041e553 lea edx,[eax+eax*8]; lea eax,[eax+edx*2]
        s8 dl = G8(0x00543D0C);                        // 0x0041e559 mov dl,byte[0x543d0c]
        a2 <<= 2;                                      // 0x0041e55f shl eax,2
        G8(a2 + 0x00585860) = cl;                      // 0x0041e562
        G8(a2 + 0x00585861) = dl;                      // 0x0041e568
        G16(a2 + 0x0058586A) = (short)ebp;             // 0x0041e56e mov word,bp
    }
    goto a41e624;                                      // 0x0041e575
a41e57a:
    esi = eax - 0x80;                                  // 0x0041e57a lea esi,[eax-0x80]
    eax = G32(0x00543D08);                             // 0x0041e57d
    esi <<= 8;                                         // 0x0041e587 shl esi,8
    G32(esi + 0x005794B8) = (eax << 0xA) + 0x200;      // 0x0041e582..0x0041e590
    ecx = G32(0x00543D0C);                             // 0x0041e596
    G32(esi + 0x005794BC) = (ecx << 0xA) + 0x200;      // 0x0041e59c..0x0041e5a9
    G16(esi + 0x00579556) = (short)ebp;                // 0x0041e5af mov word,bp
    eax = CC(0x0040DF80, eax, ecx);                    // 0x0041e5b6 objectAt (push ecx; push [0x543d08])
    if (eax != 0) goto a41e624;                        // 0x0041e5be test; jne
    if (G32(0x0059E7B8) & 0x4200000) goto a41e609;     // 0x0041e5c2 test; jne
    if (G8(esi + 0x005794D0) != 0) goto a41e609;       // 0x0041e5ce mov al,byte; test; jne
    {
        int e = (int)G16(esi + 0x0057956E);            // 0x0041e5d8 movsx
        s8 d = G8(esi + 0x005794D9);                   // 0x0041e5df movsx edx,byte
        int a2 = ((e + e * 4) * 2 + e) * 0;            // placeholder (recomputed below)
        a2 = (e + (e * 5) * 2) << 2;                   // 0x0041e5e6 lea ecx,[eax+eax*4]; lea eax,[eax+ecx*2]; shl eax,2
        G8(d + a2 + 0x005849E3) = (s8)(G8(d + a2 + 0x005849E3) | 4);  // 0x0041e5f6 or dl,4; store
        G8(a2 + 0x00584A09) = (s8)0xFF;                // 0x0041e602
    }
a41e609:
    G8(esi + 0x005794D9) = 0x13;                       // 0x0041e609
    G32(esi + 0x0057958C) = ebp;                       // 0x0041e610
    G8(esi + 0x005794DA) = 0;                          // 0x0041e616
    G16(esi + 0x0057955C) = (short)ebp;                // 0x0041e61d mov word,bp
a41e624:
    G32(0x004C2E10) = edi;                             // 0x0041e624
a41e62a:
    eax = 0;                                           // 0x0041e62a xor eax,eax
    G32(0x00822D68) = eax;                             // 0x0041e62c
a41e631:
    if (G32(0x0059B04C) != ebp) goto a41e6b8;          // 0x0041e631 cmp [0x59b04c],ebp; jne
    {
        DWORD (WINAPI* tgt)() = reinterpret_cast<DWORD(WINAPI*)()>(GU32(0x004BA21C));  // 0x0041e639 edi=[0x4ba21c]
        eax = (int)tgt();                              // 0x0041e63f call edi (timeGetTime)
        esi = FD(0x1EC);                               // 0x0041e641
        G32(0x005A8C88) = eax - esi;                   // 0x0041e648 sub eax,esi; mov [0x5a8c88],eax
        {
            s8 al = G8(0x005619A0);                    // 0x0041e64f
            if (al & 8) G8(0x005619A0) = (s8)(al & 0xF7);  // 0x0041e654 test al,8; je; and al,0xf7
        }
        int c = (int)tgt();                            // 0x0041e65f call edi
        int t = esi + ((((G32(0x0059E7B8) & 0x20000) != 0 ? 0x4B : 0) + 0xAF) / 2);  // 0x0041e663..0x0041e67e
        if ((u32)c < (u32)t) {                         // 0x0041e680 cmp ecx,eax; jae done
            do {
                CC(0x0045BF80, ebx, ebp);              // 0x0041e686 waitTicks (push ebp; push ebx)
                c = (int)tgt();                        // 0x0041e68e call edi
                t = esi + ((((G32(0x0059E7B8) & 0x20000) != 0 ? 0x4B : 0) + 0xAF) / 2);  // 0x0041e692..0x0041e6ad
            } while ((u32)c < (u32)t);                 // 0x0041e6af cmp ecx,eax; jb loop
        }
        eax = G32(0x00822D68);                         // 0x0041e6b3
    }
a41e6b8:
    ecx = G32(0x0059E7B8);                             // 0x0041e6b8
    edx = ecx & 0x4200000;                             // 0x0041e6be
    if (edx == 0x4000000) return kMenu;                // 0x0041e6c6 cmp edx,0x4000000; je 0x420e35
    if (eax != ebp) goto a420ac6;                     // 0x0041e6d2 cmp eax,ebp; jne 0x420ac6
    if (G32(0x0059B734) != ebp) goto a420ac6;         // 0x0041e6da cmp [0x59b734],ebp; jne 0x420ac6
    // c3fyc fix: eax = [0x5a6374] here (0x0041e6e6 mov eax), and 0x00420acb..0x00420add (cmp eax, 3) and 0x00420b45
    // (cmp eax, 3) read it; the old code compared the global without assigning eax, so a420acb saw the eax of
    // 0x0041e6b3 ([0x822d68], equal to ebp here) and took the quit-text arm 0x00420b4e instead of the 0x00420ae2 prompt
    // / 0x00420e0f exit whenever [0x5a6374] >= 3 (fix 6's class, found by re/tools/c3fy_audit_live.py).
    eax = G32(0x005A6374);                             // 0x0041e6e6 mov eax,[0x5a6374]
    if (eax < 3) return kFrame;                        // 0x0041e6eb cmp eax,3; 0x0041e6ee jl 0x40fc29
    goto a420acb;                                    // 0x0041e6f4 jmp 0x420acb
a41e6f9:
    // ---- 0x0041e6f9  pending golfer action ([0x824130]) ----
    edi -= 1;                                          // 0x0041e6f9 dec edi (edi=[0x53df54]-1)
    eax += 1;                                          // 0x0041e6fa inc eax (eax=[0x824130]+1)
    if ((u32)eax > 6) goto a41eaa5;                    // 0x0041e6fb cmp eax,6; ja
    switch (eax) {                                     // 0x0041e704 jmp [eax*4+0x421a88]
        case 0: goto a41e792; case 1: goto a41ea4f; case 2: goto a41ea32;
        case 3: goto a41e722; case 4: goto a41e70b; case 5: goto a41ea95;
        case 6: goto a41e81c; default: goto a41eaa5;
    }
a41e70b:  // idx4
    G32(0x00822D68) = ebp;                             // 0x0041e710
    G32(0x00824144) ^= ebx;                            // 0x0041e70b..0x0041e718
    return kFrame;                                     // 0x0041e71d
a41e722:  // idx3
    eax = edi;                                         // 0x0041e722
    G32(0x004C2E0C) = edi;                             // 0x0041e724
    eax <<= 8;                                         // 0x0041e72a
    G32(0x0053DF54) = ebp;                             // 0x0041e72d
    if (G32(eax + 0x005794C0) == -1) {                 // 0x0041e733 cmp; jne 0x41e75a
        G32(0x004C2BA0) = G32(eax + 0x005794B8) >> 0xA;  // 0x0041e73c..0x0041e74e (sar signed)
        G32(0x004C2BA4) = G32(eax + 0x005794BC) >> 0xA;  // 0x0041e742..0x0041e754
    }
    if (G8(eax + 0x0057953C) == 0) G8(eax + 0x0057953C) = 1;  // 0x0041e75a..0x0041e769
    edi ^= ebx;                                        // 0x0041e76b
    edi <<= 8;                                         // 0x0041e76d
    {
        s8 al = G8(edi + 0x0057953C);                  // 0x0041e770
        edi = edi + 0x0057953C;                        // 0x0041e776 lea edi,[edi+0x57953c]
        if (al != 0) goto a41eaa5;                     // 0x0041e77c test al; jne
    }
    G8(edi) = 1;                                       // 0x0041e784
    G32(0x00822D68) = ebp;                             // 0x0041e787
    return kFrame;                                     // 0x0041e78d
a41e792:  // idx0
    esi = edi << 8;                                    // 0x0041e792 mov esi,edi; shl esi,8
    G16(esi + 0x00579556) = (short)0xFF9D;             // 0x0041e79e
    G8(esi + 0x005794D5) = 0xC;                        // 0x0041e7a7
    {
        int r = Rnd(2);                                // 0x0041e7ae Random::range(2)
        eax = G32(esi + 0x005794C8);                   // 0x0041e7b6
        if ((r & 0xFFFF) == 0) eax &= 0xFFFDFFFF;      // 0x0041e7b3 test ax,ax; je; and
        else eax |= 0x20000;                           // 0x0041e7be or eax,0x20000
        G32(esi + 0x005794C8) = eax;                   // 0x0041e7cf
    }
    CC(0x00467A00, edi, 0x1D, 0x14);                   // 0x0041e7d5 emitGolferThought (push 0x14,0x1d,edi)
    esi = 0;                                           // 0x0041e7dd xor esi,esi
    for (;;) {                                         // 0x0041e7df loop
        eax = Rnd(0x98) & 0xFFFF;                       // 0x0041e7df..0x0041e7ee
        if (eax == edi) continue;                      // 0x0041e7f3 cmp eax,edi; je
        eax <<= 8;                                     // 0x0041e7f7
        if (G8(eax + 0x005794D9) <= 0) continue;       // 0x0041e7fa mov cl,byte; test; jle (signed)
        G16(eax + 0x0057955C) = (short)(G16(eax + 0x0057955C) - 1);  // 0x0041e804 dec word
        esi++;                                         // 0x0041e80b
        if (esi >= 2) break;                           // 0x0041e80c cmp esi,2; jl loop
    }
    G32(0x00822D68) = ebp;                             // 0x0041e811
    return kFrame;                                     // 0x0041e817
a41e81c:  // idx6: story reply
    esi = edi << 8;                                    // 0x0041e81c mov esi,edi; shl esi,8
    ebx = 0x100000;                                    // 0x0041e81e
    eax = G32(esi + 0x005794C8) & ebx;                 // 0x0041e826..0x0041e82c
    if (eax == 0) {                                    // 0x0041e82e jne 0x41e846
        ecx = (int)G16(esi + 0x0057955A) << 8;         // 0x0041e830..0x0041e837
        if (!(G32(ecx + 0x005794C8) & ebx)) goto a41eaa5;  // 0x0041e83a test; je
    }
    {
        int w = (int)G16(esi + 0x0057956A);            // 0x0041e848 movsx eax,word
        if (eax == ebp) goto a41e92a;                  // 0x0041e846 cmp eax,ebp; je (eax=the &0x100000 result)
        // partner path 0x0041e855
        eax = Rnd(w << 3);                             // 0x0041e855 shl eax,3; Rnd
        {
            int d = (int)G16(esi + 0x0057955A) << 8;   // 0x0041e863
            s8 c = G8(esi + 0x005794D9);               // 0x0041e86a
            G8(d + c + 0x005794EF) = (s8)eax;          // 0x0041e874
        }
        int d2 = (int)G16(esi + 0x0057955A) << 8;      // 0x0041e87b
        if (G8(d2 + 0x00579541) & 0x80) goto a41e9b9;  // 0x0041e885 test; jne
        eax = Rnd(0x20);                               // 0x0041e892 Rnd(0x20)
        {
            int c = (int)G16(esi + 0x0057955A) << 8;   // 0x0041e89e
            s8 dd = G8(esi + 0x005794D9);              // 0x0041e8a5
            G8(c + dd + 0x005794EF) = (s8)eax;         // 0x0041e8af
        }
        int e = (int)G16(esi + 0x0057955A) << 8;       // 0x0041e8b6
        if (!(G8(e + 0x00579541) & 0x40)) goto a41e9b9; // 0x0041e8c0 test; je
        eax = Rnd(4);                                  // 0x0041e8cd Rnd(4)
        ebp = eax & 0xFFFF;                            // 0x0041e8d9 mov bp,ax; and ebp,0xffff
        eax = CC(0x0045C420, edi ^ 1);                 // 0x0041e8e8 thoughtBalance(edi^1) (push edi^1)
        edx = (int)G16(esi + 0x0057956A) + ebp;        // 0x0041e8ed..0x0041e8f4
        edx += G32(0x00822C88);                        // 0x0041e8f6..0x0041e8ff
        if (eax + 4 < edx) goto a41e9b7;               // 0x0041e8fc add eax,4; cmp eax,edx; jl (signed)
        {
            int c = (int)G16(esi + 0x0057955A) << 8;   // 0x0041e90c
            s8 cc = G8(esi + 0x005794D9);              // 0x0041e913
            G8(c + cc + 0x005794EF) = 0x63;            // 0x0041e91d
        }
        goto a41e9b7;                                  // 0x0041e925
    }
a41e92a:  // self path
    eax = Rnd((int)G16(esi + 0x0057956A) << 3);        // 0x0041e92a shl eax,3; Rnd
    {
        s8 d = G8(esi + 0x005794D9);                   // 0x0041e938
        G8(d + esi + 0x005794EF) = (s8)eax;            // 0x0041e93f
    }
    if (G8(esi + 0x00579541) & 0x80) goto a41e9b9;     // 0x0041e946 test; jne
    eax = Rnd(0x20);                                   // 0x0041e94f Rnd(0x20)
    {
        s8 c = G8(esi + 0x005794D9);                   // 0x0041e95b
        G8(c + esi + 0x005794EF) = (s8)eax;            // 0x0041e962
    }
    if (!(G8(esi + 0x00579541) & 0x40)) goto a41e9b9;  // 0x0041e969 test; je
    eax = Rnd(4);                                      // 0x0041e972 Rnd(4)
    ebp = eax & 0xFFFF;                                // 0x0041e97e mov bp,ax; and ebp,0xffff
    eax = CC(0x0045C420, edi);                         // 0x0041e988 thoughtBalance(edi) (push edi)
    {
        int edx2 = (int)G16(esi + 0x0057956A) + ebp;   // 0x0041e98d..0x0041e994
        edx2 += G32(0x00822C88);                       // 0x0041e996..0x0041e99f
        if (eax + 4 < edx2) goto a41e9b7;              // 0x0041e99c add eax,4; cmp; jl (signed)
    }
    {
        s8 c = G8(esi + 0x005794D9);                   // 0x0041e9a8
        G8(c + esi + 0x005794EF) = 0x63;               // 0x0041e9af
    }
a41e9b7:
    ebp = 0;                                           // 0x0041e9b7 xor ebp,ebp
a41e9b9:
    if (!(G32(esi + 0x005794C8) & ebx)) edi ^= 1;      // 0x0041e9b9 test; jne; 0x0041e9c1 xor edi,1
    eax = CC(0x00466370, edi, 1);                      // 0x0041e9c7 advanceStory(edi,1) (push 1; push edi)
    if (eax != 0) goto a41e9f1;                        // 0x0041e9cf test; jne
    G16(esi + 0x0057955C) = (short)(G16(esi + 0x0057955C) - 1);  // 0x0041e9d3 dec word
    {
        int c = (int)G16(esi + 0x0057955A) << 8;       // 0x0041e9da
        G16(c + 0x0057955C) = (short)(G16(c + 0x0057955C) - 1);  // 0x0041e9e4 dec word
    }
a41e9f1:
    edi = G32(esi + 0x005794C8) | 0x200000;            // 0x0041e9f1..0x0041e9fd
    G32(0x0053DF54) = ebp;                             // 0x0041e9f7
    G32(0x00822D68) = ebp;                             // 0x0041ea03
    G32(esi + 0x005794C8) = edi;                       // 0x0041ea09
    {
        int d = (int)G16(esi + 0x0057955A) << 8;       // 0x0041ea0f
        int c = G32(d + 0x005794C8) | 0x200000;        // 0x0041ea19..0x0041ea25
        G32(d + 0x005794C8) = c;                       // 0x0041ea2b
    }
    return kFrame;                                     // 0x0041ea2d
a41ea32:  // idx2
    edi += 0x80;                                       // 0x0041ea32
    G32(0x0053DF54) = ebp;                             // 0x0041ea38
    G32(0x004C2E10) = edi;                             // 0x0041ea3e
    G32(0x00822D68) = ebp;                             // 0x0041ea44
    return kFrame;                                     // 0x0041ea4a
a41ea4f:  // idx1
    esi = edi << 8;                                    // 0x0041ea4f mov esi,edi; shl esi,8
    CC(0x004385D0, (int)G16(esi + 0x0057956E), edi);   // 0x0041ea5d showCharacterEditor (push edi; push movsx word)
    {
        int c = (int)G16(esi + 0x0057956E);            // 0x0041ea62
        G32(0x00822D68) = ebp;                         // 0x0041ea6c
        int off = ((c * 8 - c) * 5) << 4;              // 0x0041ea72 lea[ecx*8];sub;lea[*5];shl4 = c*560
        G16(esi + 0x00579570) = (short)(s8)G8(off + 0x004D60A9);  // 0x0041ea81 movsx dx,byte; store word
    }
    return kFrame;                                     // 0x0041ea90
a41ea95:  // idx5
    G32(0x0053DF54) = ebp;                             // 0x0041ea95
    G32(0x00824130) = -1;                              // 0x0041ea9b
a41eaa5:
    G32(0x00822D68) = ebp;                             // 0x0041eaa5
    return kFrame;                                     // 0x0041eaab
a41eab0:  // click at normal speed: slot panel
    eax = CC(0x00433040, eax, ecx);                    // 0x0041eab2 clickSlotPanel (push ecx; push eax)
    G32(0x0056A514) = eax;                             // 0x0041eabf
    eax = Rnd(3) & 0xFFFF;                             // 0x0041eac4 Random::range(3)
    G32(0x005A34F0) = eax;                             // 0x0041ead0
    eax = G32(0x004C2848);                             // 0x0041ead5
    if (eax == ebx) { G32(0x005A34F0) = ebp; return kFrame; }  // 0x0041eada cmp eax,ebx; jne; else mov,jmp
    if (eax != ebp) return kFrame;                     // 0x0041eae9 cmp eax,ebp; jne 0x40fc29
    {
        int h = G32(0x005685F0) * 65;                  // 0x0041eaf1 shl6;add
        if (G32(h * 8 + 0x00575AB8) == 0) return kFrame;  // 0x0041eafe test; je 0x40fc29
    }
    StrCpy(kBuf, 0x004C5D20);                           // 0x0041eb0d..0x0041eb38 (push 0xa,0x58a528,edx)
    eax = Itoa(G32(0x005685F0), 0x0058A528, 0xA);       // 0x0041eb3a __itoa
    StrCat(kBuf, (u32)eax);                             // 0x0041eb3f..0x0041eb77
    {
        int h = G32(0x005685F0) * 65;                  // 0x0041eb67..0x0041eb77
        u32 src = (G32(h * 8 + 0x00575AC8) != 0) ? 0x004C5CD4 : 0x004C5CA4;  // 0x0041eb79..0x0041eb89
        StrCat(kBuf, src);                             // 0x0041eb8e..0x0041ebbc (push 0xa,0x58a528,edx)
    }
    eax = Itoa(G32(0x005685F0), 0x0058A528, 0xA);       // 0x0041ebe9 __itoa
    StrCat(kBuf, (u32)eax);                             // 0x0041ebee..0x0041ec19
    StrCat(kBuf, 0x004C5C48);                           // 0x0041ec1b..0x0041ec4c
    if (G32(0x005685F0) >= 0x13) StrCpy(kBuf, 0x004C5C20);  // 0x0041ec3f..0x0041ec73: strcpy (the destination is not
                                                       // scanned at 0x0041ec64), "You cannot build more than 18 holes."
                                                       // replaces the message (c3fy2 fix: was StrCat)
    CC(0x0040CB00, 0x80006000, 1, -1);                 // 0x0041ec7e startMessage (push -1,1,0x80006000)
    return kFrame;                                     // 0x0041ec86
a41ec8b:  // edx==1: pick building tool
    eax = CC(0x00434140, eax, ecx);                    // 0x0041ec8d pickBuilding (push ecx; push eax)
    G32(0x0056A514) = eax;                             // 0x0041ec95
    CC(0x0045BF80, 0xA, ebp);                          // 0x0041ec9a waitTicks (push ebp; push 0xa)
    return kFrame;                                     // 0x0041eca2
a41eca7:  // edx==2: click employee view
    eax = CC(0x00435680, eax, ecx);                    // 0x0041eca9 clickEmployeeView (push ecx; push eax)
    G32(0x0056A514) = eax;                             // 0x0041ecb1
    return kFrame;                                     // 0x0041ecb6

    // ======= quit / save-on-exit dialog (0x00420ac6) =======
a420ac6:
    eax = G32(0x005A6374);                             // 0x00420ac6
a420acb:
    G32(0x0059B734) = 0;                               // 0x00420ad1
    if (ecx & 0x1000000) goto a420b45;                 // 0x00420acb test ecx,0x1000000; jne
    if (eax < 3) goto a420b4e;                          // 0x00420add cmp eax,3; jl (signed)
    StrCpy(kBuf, 0x004C5AD0);                           // 0x00420ae2..0x00420b15 (push 0,1,1,0x96,0xfa)
    // c3fyc fix: the 5th argument is the immediate 0 (0x00420aec push 0), not the local ebp
    eax = CC(0x0046D6E0, 0xFA, 0x96, 1, 1, 0);          // 0x00420b17 showPopup
    if (eax != 0) goto a420e0f;                         // 0x00420b1f test; jne
    G32(0x005A6374) = 0;                                // 0x00420b2c
    G32(0x0059E7B8) |= 0x1000000;                       // 0x00420b27..0x00420b3b
    return kFrame;                                      // 0x00420b40
a420b45:
    if (eax >= 3) goto a420e0f;                         // 0x00420b45 cmp eax,3; jge 0x420e15 (== a420e0f)
a420b4e:
    {
        u32 edi_s;                                      // 0x00420b4e
        if (ecx & 0x4000000) {                          // 0x00420b4e test ecx,0x4000000; je 0x420bb0
            StrCpy(kBuf, 0x004C5AB0);                   // 0x00420b56..0x00420b79
            StrCat(kBuf, 0x004D6098);                   // 0x00420b7b..0x00420ba4
            edi_s = 0x004C5AA0;                         // 0x00420ba6
            goto a420ca7_do;
        }
        StrCpy(kBuf, 0x004C5A94);                       // 0x00420bb0..0x00420bdb
        edi_s = (G16(0x005A6D3C) < 0xA) ? 0x004C5A8C : 0x004C5A84;  // 0x00420bd3..0x00420be4
        StrCat(kBuf, edi_s);                            // 0x00420be9 (edi)
        StrCat(kBuf, 0x004C5A78);                       // 0x00420c11..0x00420c3a
        StrCat(kBuf, 0x004D6098);                       // 0x00420c3c..0x00420c60
        edi_s = (G8(0x004D60A9) & 0x80) ? 0x004C5A6C : 0x004C5A60;  // 0x00420c60..0x00420c75
        StrCat(kBuf, edi_s);                            // 0x00420c7a (edi)
        edi_s = 0x004C5A50;                             // 0x00420ca2
a420ca7_do:
        StrCat(kBuf, edi_s);                            // 0x00420ca7 (edi; push eax,1,1,0x64,0x190)
        StrCat(kBuf, 0x004C5A2C);                       // 0x00420cd7..0x00420d04
        StrCat(kBuf, 0x004C5A08);                       // 0x00420d06..0x00420d2f
        StrCat(kBuf, 0x004C59F4);                       // 0x00420d31..0x00420d5a
        // c3fyc fix: the 5th argument is eax = 0 (0x00420caa xor eax, eax; 0x00420cb2 push eax) on every path into
        // 0x00420ca7; the old line passed the local ebp, equal to 0 only if every path here zeroed it.
        eax = CC(0x0046D6E0, 0x190, 0x64, 1, 1, 0);     // 0x00420d5c showPopup
    }
    if (eax == 1) goto a420e0a;                         // 0x00420d64 cmp eax,1; je
    if (eax == 0) return kFrame;                        // 0x00420d6d test; je 0x40fc29
    if (eax != -1) goto a420e0f;                        // 0x00420d75 cmp eax,-1; jne
    return kFrame;                                      // 0x00420d7e jmp 0x40fc29
a420e0a:
    CC(0x00405B10);                                     // 0x00420e0a saveGameDialog
a420e0f:
    G32(0x0059E7B8) &= 0xFBFFFFFF;                      // 0x00420e0f..0x00420e20
    CC(0x0040B4A0, 0x004C59E8);                         // 0x00420e26 saveGame (push 0x4c59e8)
    CC(0x00448200, 0x2D);                               // 0x00420e2d startSoundSlot (push 0x2d)
    return kMenu;                                       // falls into 0x00420e35

}  // Ph_0041c0c5
}  // namespace
// c3fy part H2: mainLoop 0x0040f5c0, the dominator subtree of 0x0041ecbb (0x0041ecbb..0x00420ac1, 2,187
// instructions): the mouse / tool action part of the frame. Plus, as separate entries, the blocks 0x00420ac6..0x00420d7e
// and 0x00420e0a..0x00420e32 (the end-of-round prompts and the quit save), which the brief lists under H2 but which are
// entered only from H1's block 0x0041e6d2 (0x0041e6d4 / 0x0041e6e0 -> 0x00420ac6, 0x0041e6f4 -> 0x00420acb): see the notes.
// Transcribed from log/c3/alt/c3fy_norm.asm. Every [F+X] access is kept at its offset and width.
namespace {

// tile grids are 50 x 50, cell index x * 50 + y (lea / lea / lea [y + t*2] sequences)
static inline u32 H2_cell(int x, int y) { return (u32)x * 50u + (u32)y; }
#define H2_TW(i) GU16(0x0053CAF0u + 2u * (u32)(i))   // tile flag word
#define H2_TWL(i) GU8(0x0053CAF0u + 2u * (u32)(i))   // its low byte
#define H2_TWH(i) GU8(0x0053CAF1u + 2u * (u32)(i))   // its high byte
#define H2_TT(i) G8(0x005722E8u + (u32)(i))          // tile type (signed byte)
// tile coordinate -> world coordinate (shl 0xa; add 0x200)
static inline int H2_W(int v) { return (int)(((u32)v << 10) + 0x200u); }
// (short)[0x005a6d3c] % 100 (movsx; cdq; idiv 100): the row of the 20-byte records at 0x00584210
static inline u32 H2_row(int rem) { return (u32)rem * 20u; }
static inline int H2_Rem100() { return (int)G16(0x005A6D3C) % 100; }
// the 0x208-byte hole records from 0x00575ab0
static inline u32 H2_hole(int h) { return (u32)h * 0x208u; }
static inline int H2_KeyHi(u32 slot, int vk) {   // GetAsyncKeyState through the IAT value read earlier; `mov cl, ah`
    SHORT s = reinterpret_cast<SHORT(WINAPI*)(int)>(slot)(vk);
    return (int)(u8)((u16)s >> 8);
}
static inline int H2_Snd18() { return CC(0x004481B0, 0x18, 0x64, 0, 0, 0); }   // playSound(0x18, 100, 0, 0, 0)

// ---------------------------------------------------------------------------------------------------------------------
// 0x00420a9f: rebuild after an edit: markTileKind11, rebuildTileCoverage, [0x4c2e04] / [0x5a47e4] = x / y,
// rebuildHeightfield, then the frame head.
static int H2_Rebuild(u8* F) {
    CC(0x0042F2C0);                                   // 0x00420a9f markTileKind11
    CC(0x0042F340);                                   // 0x00420aa4 rebuildTileCoverage
    G32(0x004C2E04) = FD(0x1c);                       // 0x00420ab1
    G32(0x005A47E4) = FD(0x24);                       // 0x00420ab7
    CC(0x0042F7A0);                                   // 0x00420abc rebuildHeightfield
    return kFrame;                                    // 0x00420ac1
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0041ed24..0x0041f164: a click with no build tool active ([0x822d68] == 1). Shift up: a hole marker (flag 0x80)
// announces its building; a hole tee tile with record bits 3 offers the rename-hole box. Speed 5 with the cursor in the
// golfer strip moves the camera to that golfer. Otherwise the nearest golfer / walker within 40 px is selected
// (Ctrl skips the golfer scan), and a click on an object tile with no object record opens the pair select (mode 1)
// or the rename-course box (mode 2).
static int H2_Select(u8* F, int ebp) {
    u32 key = GU32(0x004BA1F0);                       // 0x0041ed24 esi = GetAsyncKeyState slot
    G8(0x00569498) = 0;                               // 0x0041ed2c
    G32(0x0053DF54) = ebp;                            // 0x0041ed33
    if (H2_KeyHi(key, 0x10) == 0) {                   // 0x0041ed39 / 0x0041ed41
        int ecx;
        int eax = CC(0x0040BF60, FD(0x1c), FD(0x24)); // 0x0041ed51 tileBlocked
        ecx = FD(0x1c);                               // 0x0041ed56
        if (eax == 0) {                               // 0x0041ed5f
            u32 i = H2_cell(ecx, FD(0x24));
            u16 w = H2_TW(i);                         // 0x0041ed6e
            if (w & 0x80) {                           // 0x0041ed76
                int h = w & 0x1f;                     // 0x0041ed7a
                G32(0x0053DF54) = -h;                 // 0x0041ed86
                if (GU8(0x00575AB0u + H2_hole(h)) != 0) return kFrame;   // 0x0041ed95 / 0x0041ed98
                CC(0x0040E720);                       // 0x0041ed9e announceBuilding
                G32(0x0053DF54) = 0;                  // 0x0041eda3
                return kFrame;                        // 0x0041edad
            }
        }
        eax = CC(0x0040BF60, ecx, FD(0x24));          // 0x0041edb8 tileBlocked
        if (eax == 0) {                               // 0x0041edc2
            u32 i = H2_cell(FD(0x1c), FD(0x24));
            if (H2_TT(i) == 0) {                      // 0x0041edd9 / 0x0041ede1
                int h = H2_TWL(i) & 0x1f;             // 0x0041ede7 / 0x0041edee
                ebp = h;                              // 0x0041edf1
                if (GU8(0x00575CB0u + (u32)h * 0x208u) & 3) {   // 0x0041edfa / 0x0041ee02
                    // rename hole: text = 0x4c5c10 + itoa(h) + 0x4c5c0c, copied to F+0x308 as the prompt
                    GU8(kBuf) = 0;                                    // 0x0041ee12
                    StrCpy(kBuf, 0x004C5C10);                         // 0x0041ee19..0x0041ee3a
                    u32 s = (u32)Itoa(ebp, 0x0058A528, 10);           // 0x0041ee3c
                    StrCat(kBuf, s);                                  // 0x0041ee43..0x0041ee6a
                    StrCat(kBuf, 0x004C5C0C);                         // 0x0041ee6c..0x0041ee9c
                    StrCpy((u32)FA(0x308), kBuf);                     // 0x0041ee9e..0x0041eebc
                    GU8(kBuf) = 0;                                    // 0x0041eebe
                    CC(0x00407280, ebp);                              // 0x0041eec5 appendHoleName(h) (push at 0x0041ee48)
                    CC(0x0045B2C0, FA(0x308), 0xc8, 0x20, 0x20);      // 0x0041eedb textInputBox
                    if (GU8(kBuf) == 0) return kFrame;                // 0x0041eee0 / 0x0041eeea
                    CC(0x0045B8B0, ebp);                              // 0x0041eef1 appendToBuffer45b8b0(h)
                    return kFrame;                                    // 0x0041eef9
                }
            }
        }
    }
    // 0x0041eefe
    if (G32(0x00567AFC) == 5) {
        int mx = G32(0x00822D60);                     // 0x0041ef07
        if (mx >= 0xcc && G32(0x00822D64) > 0x23f) {  // 0x0041ef11 / 0x0041ef1d
            int g = G32(0x00567FF4u + 4u * (u32)((mx - 0xcc) / 16));   // 0x0041ef1f..0x0041ef2d
            if (g < -1) return kFrame;                // 0x0041ef37
            if (g >= 0x98) return kFrame;             // 0x0041ef42
            G32(0x0053DF54) = g + 1;                  // 0x0041ef4e
            u32 r = (u32)g << 8;                      // 0x0041ef4b
            G32(0x004C2BA0) = G32(0x005794B8u + r) >> 10;   // 0x0041ef54 / 0x0041ef66
            G32(0x004C2BA4) = G32(0x005794BCu + r) >> 10;   // 0x0041ef5a / 0x0041ef6c
            return kFrame;                            // 0x0041ef72
        }
    }
    int edi;
    if (H2_KeyHi(key, 0x11) == 0) {                   // 0x0041ef79 / 0x0041ef81
        edi = 0x28;                                   // 0x0041ef87
        FD(0x12c) = edi;                              // 0x0041ef8e
        int k = 0;                                    // 0x0041ef8c
        for (u32 p = 0x005794C0; p < 0x00582CC0; p += 0x100, k++) {   // 0x0041ef95 / 0x0041efdb..0x0041efe8
            if (GU8(p + 0x19) == 0) continue;         // 0x0041ef9a / 0x0041ef9f
            int gx = G32(p);                          // 0x0041efa1
            if (gx == -1) continue;                   // 0x0041efa6
            int d = CC(0x0040ACD0, G32(0x00822D60) - gx, G32(0x00822D64) - G32(p + 4));   // 0x0041efbd distance
            if (d < edi) {                            // 0x0041efc7
                edi = d;                              // 0x0041efcc
                G32(0x0053DF54) = k + 1;              // 0x0041efce
                G8(0x00569498) = 0;                   // 0x0041efd4
            }
        }
        FD(0x12c) = edi;                              // 0x0041efea
    } else {
        edi = FD(0x12c);                              // 0x0041f087 (can be read before written: c3fy_uninit.txt)
    }
    // 0x0041eff1: walkers (0x4c records from 0x00585858)
    {
        int k = 0;                                    // 0x0041eff3
        for (u32 p = 0x00585863; p < 0x00586B63; p += 0x4c, k++) {   // 0x0041effa..0x0041f047
            if (GU8(p - 1) == 0) continue;            // 0x0041effa / 0x0041efff
            if (G8(p) >= 0) continue;                 // 0x0041f001 / 0x0041f004
            int wx = G32(p - 0xb);                    // 0x0041f006
            if (wx == -1) continue;                   // 0x0041f00c
            int d = CC(0x0040ACD0, G32(0x00822D60) - wx, G32(0x00822D64) - G32(p - 7));   // 0x0041f023 distance
            if (d < edi) {                            // 0x0041f02d
                edi = d;                              // 0x0041f02f
                G32(0x0053DF54) = 0;                  // 0x0041f031 (ebx = 0 from 0x0041eff1)
                G32(0x004C2E10) = k;                  // 0x0041f037
            }
        }
    }
    FD(0x12c) = edi;                                  // 0x0041f04c
    if (edi < 0x28) return kFrame;                    // 0x0041f049 / 0x0041f053
    int sel = G32(0x0053DF54);                        // 0x0041f059
    if (sel > 0 && GU8(0x005793D9u + ((u32)sel << 8)) == 0xff) {   // 0x0041f060 / 0x0041f065
        // 0x0041f06e loads [0x822d68] into eax; showGolferPairSelect takes no argument (c3et.cpp)
        CC(0x00459850);                               // 0x0041f073 showGolferPairSelect
        G32(0x0053DF54) = 0;                          // 0x0041f078
        return kFrame;                                // 0x0041f082
    }
    // 0x0041f093
    int x = FD(0x1c), y = FD(0x24);
    if ((H2_TW(H2_cell(x, y)) & 0x400) == 0) return kFrame;   // 0x0041f0a4..0x0041f0b5
    if (CC(0x0040DF80, x, y) != 0) return kFrame;     // 0x0041f0bd objectAt / 0x0041f0c7
    if (G32(0x00822D68) == 1) {                       // 0x0041f0cd
        if (G32(0x0059E7B8) & 0x200000) return kFrame;   // 0x0041f0d6 / 0x0041f0e0
        if (G32(0x005685F0) <= 1) return kFrame;      // 0x0041f0e6 / 0x0041f0ed
        CC(0x00459850);                               // 0x0041f0f3 showGolferPairSelect
    }
    if (G32(0x00822D68) != 2) return kFrame;          // 0x0041f0f8 / 0x0041f0ff
    GU8(kBuf) = 0;                                    // 0x0041f107
    CC(0x0040DAA0, -1);                               // 0x0041f10e appendCourseTitle(-1)
    CC(0x0045B2C0, 0x004C5BF8, 0xc8, 0x20, 0x20);     // 0x0041f121 textInputBox
    if (GU8(kBuf) != 0) {                             // 0x0041f126 / 0x0041f130
        if (CC(0x00405AC0, (int)kBuf) != 0) {         // 0x0041f137 sanitizeFileName / 0x0041f141 (full eax tested)
            CC(0x0045B8B0, 0);                        // 0x0041f145 appendToBuffer45b8b0(0)
            return kFrame;                            // 0x0041f14d
        }
    }
    H2_Snd18();                                       // 0x0041f15c
    return kFrame;                                    // 0x0041f164
}

static inline int H2_neg(int v) { return (int)(0u - (u32)v); }

// ---------------------------------------------------------------------------------------------------------------------
// 0x0041f35e..0x0041fc79: place the selected object tool t = [0x4c2854] (t != -1) on tile (x, y) = ([F+0x1c], [F+0x24])
// in mode [0x822d68] == 1. Cost = evalPlacementArea (halved for t == 0) + word 0x4c26c2[t * 20] * (count 0x5a8c38[t] + 2)
// / 2; t == 4 costs (5 c + 25) * 200 / 100 with c = [0x56a514], free when bit c of [0x822c70] is set. canAfford gates
// all but t == 15; t == 5 needs rateLot >= 50. Per-tool switch 0x00421aa4 (index bytes 0x00421ac0). Then the shared
// build sound, the cash / ledger updates and the points popup. Ends at 0x0041fc7c (no register live there).
static void H2_Build(u8* F, int eax) {
    const int ebp = -1;                               // 0x0041f353
    int ebx, esi, edi, ecx, rec, x, y;
    u32 i;
    if (eax == 4 && G32(0x0056A514) == ebp) {         // 0x0041f35e / 0x0041f363
        G32(0x004C2854) = ebp;                        // 0x0041f36b
        return;                                       // 0x0041f371 -> 0x0041fc7c
    }
    if (G32(0x00822D68) != 1) return;                 // 0x0041f376 / 0x0041f37d
    ebx = G32(0x005A8C38u + 4u * (u32)eax);           // 0x0041f383
    esi = CC(0x0040DB90, FD(0x1c), FD(0x24), (int)G8(0x004C26C0u + 20u * (u32)eax) + ebx, eax);   // 0x0041f3a3
    edi = G32(0x004C2854);                            // 0x0041f3a8
    if (edi == 0 && esi != ebp) esi = esi / 2;        // 0x0041f3b1..0x0041f3c0
    {
        int c = G32(0x005A8C38u + 4u * (u32)edi) + 2;                 // 0x0041f3c2 / 0x0041f3cc
        int w = G16(0x004C26C2u + 20u * (u32)edi);                    // 0x0041f3cf
        int prod = (int)((u32)w * (u32)c);                            // 0x0041f3d7
        ecx = G32(0x0056A514);                                        // 0x0041f3dd
        ebx = prod / 2 + esi;                                         // 0x0041f3da..0x0041f3e7
    }
    if (edi == 4) {                                   // 0x0041f3e9
        ebx = (int)(((u32)ecx * 5u + 0x19u) * 200u) / 100;            // 0x0041f3ee..0x0041f40a
        if ((1u << ((u32)ecx & 31u)) & GU32(0x00822C70)) ebx = 0;     // 0x0041f40c..0x0041f41e
    }
    if (esi == ebp) goto L_fc6a;                      // 0x0041f420 / 0x0041f422
    if (edi != 0xf) {                                 // 0x0041f428
        if (CC(0x00406C30, ebx) == 0) goto L_fc6a;    // 0x0041f42e canAfford / 0x0041f438
        edi = G32(0x004C2854);                        // 0x0041f43e
        ecx = G32(0x0056A514);                        // 0x0041f444
    }
    if (edi == 5) {                                   // 0x0041f44a
        if (CC(0x0042EF40, FD(0x1c), FD(0x24)) < 0x32) {   // 0x0041f45d rateLot / 0x0041f465
            StrCpy(kBuf, 0x004C5BBC);                 // 0x0041f46a..0x0041f498
            G8(0x00569498) = 0;                       // 0x0041f490
            StrCat(kBuf, 0x004C5B68);                 // 0x0041f49a..0x0041f4c1
            CC(0x0040CB00, (int)0x80004010, 0, ebp);  // 0x0041f4c3 startMessage
            return;                                   // 0x0041f4cb -> 0x0041fc7c
        }
        edi = G32(0x004C2854);                        // 0x0041f4d0
        ecx = G32(0x0056A514);                        // 0x0041f4d6
    }
    // 0x0041f4dc: switch through the index bytes 0x00421ac0 into the table 0x00421aa4 (case mapping in the notes)
    switch ((u32)edi > 0x13u ? 6 : (int)GU8(0x00421AC0u + (u32)edi)) {   // 0x0041f4df / 0x0041f4e7 / 0x0041f4ed
    case 4: {   // 0x0041f4f4 (t == 16)
        x = FD(0x1c);
        esi = FD(0x24);
        i = H2_cell(x, esi);
        u16 w = H2_TW(i);                             // 0x0041f505
        H2_TT(i) = 0x10;                              // 0x0041f50d
        G8(0x00578804u + i) = 0;                      // 0x0041f517
        GU8(0x0056988Cu + i) = (u8)(ecx + 1);         // 0x0041f521 / 0x0041f523
        H2_TW(i) = (u16)((w & 0xe7ffu) | 0x4100u);    // 0x0041f514 / 0x0041f51e / 0x0041f52c
        ebx = G16(0x004C26C2u + 20u * (u32)edi);      // 0x0041f534
        CC(0x0040A130, x, esi, 0x90, ebx);            // 0x0041f544 setGrids
        goto L_fab3;                                  // 0x0041f54c
    }
    case 0: {   // 0x0041f551 (t == 0)
        esi = FD(0x1c);
        y = FD(0x24);
        i = H2_cell(esi, y);
        u16 w = H2_TW(i);                             // 0x0041f562
        if (w & 0x20) goto L_f888;                    // 0x0041f56a / 0x0041f56d
        w = (u16)(w | 0x20);                          // 0x0041f573
        int t11 = H2_TT(i) == 0x11;                   // 0x0041f576
        H2_TW(i) = w;                                 // 0x0041f57d
        if (t11) {                                    // 0x0041f585
            w = (u16)(w & 0xfeffu);                   // 0x0041f587
            H2_TW(i) = w;                             // 0x0041f58a
        }
        G32(0x004C2E04) = esi;                        // 0x0041f592
        G32(0x005A47E4) = y;                          // 0x0041f598
        CC(0x0042F7A0);                               // 0x0041f59e rebuildHeightfield
        int yy = FD(0x24), xx = FD(0x1c);             // 0x0041f5a3 / 0x0041f5a7
        for (u32 o = 0; o < 0x20u; o += 8) {          // 0x0041f5ab..0x0041f5fb (neighbours 0, 2, 4, 6)
            u32 j = (u32)(G32(0x004C2878u + o) + xx) * 50u + (u32)(G32(0x004C2898u + o) + yy);
            s8 t = H2_TT(j);                          // 0x0041f5cb
            if (t == 0x15 || t == 0x16) {             // 0x0041f5d1 / 0x0041f5d5
                u32 r = (u32)GU8(0x0056988Cu + j) << 4;   // 0x0041f5db / 0x0041f5e1
                if (G16(0x0058BCB8u + r) >= 6) H2_TW(j) = (u16)(H2_TW(j) | 0x20);   // 0x0041f5e4 / 0x0041f5ee
            }
        }
        CC(0x0040A130, xx, yy, 0x80, ebx);            // 0x0041f605 setGrids
        CC(0x0040C500, 0x108, H2_W(FD(0x1c)), H2_W(FD(0x24)), 0);   // 0x0041f62d playSoundAt
        goto L_fab3;                                  // 0x0041f635
    }
    case 5: {   // 0x0041f63a (t == 19)
        x = FD(0x1c);
        esi = FD(0x24);
        i = H2_cell(x, esi);
        if (H2_TT(i) != 0x11) {                       // 0x0041f64b / 0x0041f652
            H2_Snd18();                               // 0x0041f6d0
            ebx = 0;                                  // 0x0041f6d8
            goto L_fab3;                              // 0x0041f6da
        }
        int r = ecx % 8;                              // 0x0041f654..0x0041f660
        u16 w = H2_TW(i);                             // 0x0041f661
        H2_TW(i) = (u16)((u32)r | ((u32)w & 0xe7f8u) | 0x120u);   // 0x0041f669..0x0041f677
        ebx = G16(0x004C26C2u + 20u * (u32)edi);      // 0x0041f682
        CC(0x0040A130, x, esi, 0x93, ebx);            // 0x0041f692 setGrids
        CC(0x0040C500, 0x107, H2_W(FD(0x1c)), H2_W(FD(0x24)), 0);   // 0x0041f6b9 playSoundAt
        goto L_fab3;                                  // 0x0041f6c1
    }
    case 1: {   // 0x0041f6df (t == 1)
        esi = FD(0x1c);
        i = H2_cell(esi, FD(0x24));
        if (H2_TW(i) & 0x200) goto L_f888;            // 0x0041f6f0..0x0041f701
        int any = 0;                                  // 0x0041f707
        for (int d = 0; d < 8; d += 2) {              // 0x0041f709..0x0041f72f
            if (CC(0x00407400, FD(0x1c), FD(0x24), d) != 0) any = 1;   // 0x0041f718 canStep / 0x0041f724
        }
        if (any) {                                    // 0x0041f731
            esi = FD(0x1c);
            int yy = FD(0x24);
            i = H2_cell(esi, yy);
            int c = G32(0x0056A514);                  // 0x0041f753
            H2_TW(i) = (u16)((H2_TW(i) & 0xefffu) | 0x220u);   // 0x0041f758..0x0041f769
            GU8(0x0056988Cu + i) = (u8)(c % 5);       // 0x0041f771 / 0x0041f774
            CC(0x0040A130, esi, yy, 0x81, ebx);       // 0x0041f77a setGrids
            CC(0x0040C500, 0x106, H2_W(FD(0x1c)), H2_W(FD(0x24)), 0);   // 0x0041f7a1 playSoundAt
            goto L_fab3;                              // 0x0041f7a9
        }
        StrCpy(kBuf, 0x004C5B4C);                     // 0x0041f7ae..0x0041f7d6
        CC(0x004481B0, 0x18, 0x64, 0, 0, 0);          // 0x0041f7da playSound
        CC(0x0040CB00, (int)0x80006000, 1, -1);       // 0x0041f7e8 startMessage
        ebx = 0;                                      // 0x0041f7f0
        goto L_fab3;                                  // 0x0041f7f2
    }
    case 2: {   // 0x0041f7f7 (t == 2)
        esi = FD(0x1c);
        i = H2_cell(esi, FD(0x24));
        u16 w = H2_TW(i);                             // 0x0041f808
        if (w & 0x1000) {                             // 0x0041f810..0x0041f81b
            GU8(0x0056988Cu + i) = (u8)ecx;           // 0x0041f882
            goto L_f888;
        }
        H2_TW(i) = (u16)(w | 0x5000u);                // 0x0041f823 / 0x0041f82d
        G8(0x00578804u + i) = 0;                      // 0x0041f835
        CC(0x004481B0, 0x94, 0x64, 0, 0, 0);          // 0x0041f83c playSound
        esi = FD(0x1c);
        int yy = FD(0x24);
        u8 c = GU8(0x0056A514);                       // 0x0041f849
        i = H2_cell(esi, yy);
        H2_TWH(i) = (u8)(H2_TWH(i) & 0xf4);           // 0x0041f860
        GU8(0x0056988Cu + i) = c;                     // 0x0041f868
        H2_TT(i) = 4;                                 // 0x0041f86e
        CC(0x0040A130, esi, yy, 0x82, ebx);           // 0x0041f875 setGrids
        goto L_fab3;                                  // 0x0041f87d
    }
    case 3: {   // 0x0041f88f (t == 15)
        int r = CC(0x0040E000, FD(0x1c), FD(0x24), 0xf, 0x60);   // 0x0041f89d stampCourseTiles
        u32 a = ((u32)r << 4) + 0x0058BCBFu;          // 0x0041f8a2 / 0x0041f8ae
        GU8(a) = (u8)(GU8(a) | 0x40);                 // 0x0041f8a8..0x0041f8b7
        G32(0x0059E7B8) = (int)(GU32(0x0059E7B8) & ~0x800u);   // 0x0041f8b9..0x0041f8c1
        CC(0x0042F340);                               // 0x0041f8c6 rebuildTileCoverage
        int yy = FD(0x24);                            // 0x0041f8cb
        esi = FD(0x1c);                               // 0x0041f8cf
        for (u32 p = 0x005794B8; p < 0x00582CB8; p += 0x100) {   // 0x0041f8d3..0x0041f903
            if (GU8(p + 0x21) != 0xff) continue;      // 0x0041f8d8 / 0x0041f8dc
            G32(p) = (int)(((u32)esi << 10) + 0x600u);         // 0x0041f8de..0x0041f8e9
            G32(p + 4) = (int)(((u32)yy << 10) + 0x600u);      // 0x0041f8eb..0x0041f8f6
        }
        edi = G32(0x004C2854);                        // 0x0041f905
        G32(0x00578150) = esi;                        // 0x0041f90b
        G32(0x00578154) = yy;                         // 0x0041f911
        goto L_fabd;                                  // 0x0041f917
    }
    default: {  // 0x0041f91c (t 3..14, 17, 18 and above 0x13)
        if (edi >= 6 && edi != 5 && G32(0x005A8C38u + 4u * (u32)edi) != 0) {   // 0x0041f91c..0x0041f92f
            int k = 0;                                // 0x0041f931
            for (u32 p = 0x0058BCB8; p < 0x0058CCB8; p += 0x10, k++) {   // 0x0041f933..0x0041f958
                if ((int)G16(p) == edi) {             // 0x0041f938 / 0x0041f93b
                    CC(0x0040E400, k);                // 0x0041f940 clearObjectFootprint
                    edi = G32(0x004C2854);            // 0x0041f945
                }
            }
        }
        rec = CC(0x0040E000, FD(0x1c), FD(0x24), edi, 0);   // 0x0041f967 stampCourseTiles (ebp = result)
        CC(0x00409CB0, FD(0x1c), FD(0x24), (int)(GU32(0x004C2854) | 0x100u));   // 0x0041f976..0x0041f981 pushTripleEntry
        edi = G32(0x004C2854);                        // 0x0041f986
        if (edi == 2) {                               // 0x0041f98f
            i = H2_cell(FD(0x1c), FD(0x24));
            H2_TWH(i) = (u8)(H2_TWH(i) | 0x40);       // 0x0041f9b2
            G8(0x00578804u + i) = 0;                  // 0x0041f9ba
            CC(0x004481B0, 0x94, 0x64, 0, 0, 0);      // 0x0041f9c1 playSound
            ecx = G32(0x0056A514);                    // 0x0041f9c6
            edi = G32(0x004C2854);                    // 0x0041f9cc
            G32(0x0058BCC0u + ((u32)rec << 4)) = ecx + 0x40;   // 0x0041f9da / 0x0041f9dd
        } else {
            ecx = G32(0x0056A514);                    // 0x0041f9e5
        }
        if (edi == 4) {                               // 0x0041f9eb
            esi = FD(0x1c);                           // 0x0041f9f0
            G32(0x0058BCC0u + ((u32)rec << 4)) = ecx; // 0x0041f9fa
            i = H2_cell(esi, FD(0x24));
            H2_TWH(i) = (u8)(H2_TWH(i) & 0xe7);       // 0x0041fa0a
            ebx = (int)(((u32)ecx * 5u + 0x19u) * 200u) / 100;   // 0x0041fa12..0x0041fa2e
            u32 bit = 1u << ((u32)ecx & 31u);         // 0x0041fa30 / 0x0041fa35
            u32 m = GU32(0x00822C70);                 // 0x0041fa37
            if (bit & m) {                            // 0x0041fa3f / 0x0041fa41
                ebx = 0;                              // 0x0041fa49
                G32(0x00822C70) = (int)(m & ~bit);    // 0x0041fa47 / 0x0041fa4b / 0x0041fa4d
            }
            goto L_faea;                              // 0x0041fa53
        }
        if (edi < 6) goto L_fab9;                     // 0x0041fa58 / 0x0041fa5b
        if (edi == 5) goto L_fab9;                    // 0x0041fa5d / 0x0041fa60
        {
            int v = G32(0x005A8C38u + 4u * (u32)edi); // 0x0041fa62
            G32(0x0058BCC0u + ((u32)rec << 4)) = v;   // 0x0041fa6e
            if (v == 0) CC(0x0040C6F0, 0x40, edi);    // 0x0041fa74 / 0x0041fa79 logTick
            CC(0x004481B0, 0xff, H2_W(FD(0x1c)), H2_W(FD(0x24)), 0, 0);   // 0x0041faa6 playSound
            CC(0x0042F340);                           // 0x0041faae rebuildTileCoverage
        }
        goto L_fab3;
    }
    }
L_f888:
    ebx = 0;                                          // 0x0041f888
    goto L_fabd;                                      // 0x0041f88a
L_fab3:
    edi = G32(0x004C2854);                            // 0x0041fab3
L_fab9:
    esi = FD(0x1c);                                   // 0x0041fab9
L_fabd:
    if (edi == 8 || edi == 7 || edi == 0xc || edi == 9 || edi == 0xb || edi == 0xd || edi == 0xe || edi == 4 ||
        edi == 0xf) goto L_faea;                      // 0x0041fabd..0x0041fae8
    goto L_fb4f;
L_faea:
    // 0x0041faea: the build sound (esi = [F+0x1c] on every path here)
    i = H2_cell(esi, FD(0x24));
    H2_TWH(i) = (u8)(H2_TWH(i) | 0x40);               // 0x0041fb03
    G8(0x00578804u + i) = 0;                          // 0x0041fb0b
    {
        int r = Rnd(0x12c);                           // 0x0041fb12 Random::range(0x12c)
        CC(0x004481B0, 0x90, 0x64, 0, H2_neg((int)((u32)r & 0xffffu)), 0);   // 0x0041fb17..0x0041fb28 playSound
    }
    esi = FD(0x1c);                                   // 0x0041fb2d
    edi = G32(0x004C2854);                            // 0x0041fb31
    i = H2_cell(esi, FD(0x24));
    H2_TWH(i) = (u8)(H2_TWH(i) & 0xe7);               // 0x0041fb47
L_fb4f:
    G32(0x00578338) = G32(0x00578338) + ebx;          // 0x0041fb4f..0x0041fb63
    {
        int rem = H2_Rem100();                        // 0x0041fb5a / 0x0041fb69 / 0x0041fb6a
        u32 a = 0x00584218u + H2_row(rem);
        GU16(a) = (u16)(GU16(a) - (u16)ebx);          // 0x0041fb6f
    }
    if (edi == 5) {                                   // 0x0041fb77
        int r1 = CC(0x0042EF40, esi, FD(0x24));       // 0x0041fb82 rateLot
        ebx += H2_neg(r1 / 4);                        // 0x0041fb87..0x0041fb97
        int rem = H2_Rem100();                        // 0x0041fb99..0x0041fba1
        int r2 = CC(0x0042EF40, FD(0x1c), FD(0x24));  // 0x0041fbb0 rateLot
        edi = G32(0x004C2854);                        // 0x0041fbb6
        u32 a = 0x00584212u + H2_row(rem);
        GU16(a) = (u16)(GU16(a) + (u16)(r2 / 4));     // 0x0041fbb5..0x0041fbc7
        esi = FD(0x1c);                               // 0x0041fbcf
    }
    G32(0x00571FD4) = G32(0x00571FD4) - ebx;          // 0x0041fbd3..0x0041fbde
    if (edi <= 4) {                                   // 0x0041fbdb / 0x0041fbe4
        int h = CC(0x00407340, esi, FD(0x24));        // 0x0041fbec nearestHole
        CC(0x0040C890, H2_neg(ebx), H2_W(FD(0x1c)), H2_W(FD(0x24)), h);   // 0x0041fc10 pointsPopup
    } else {
        CC(0x0040C890, H2_neg(ebx), H2_W(esi), H2_W(FD(0x24)), -1);       // 0x0041fc36 pointsPopup
    }
    {
        int t = G32(0x004C2854);                      // 0x0041fc3e
        if (t >= 6 && t != 0x10) {                    // 0x0041fc43 / 0x0041fc48
            G32(0x004C2854) = -1;                     // 0x0041fc4d
        } else if (t == 4) {                          // 0x0041fc59
            G32(0x004C2854) = -1;                     // 0x0041fc5e
        }
    }
    return;                                           // -> 0x0041fc7c
L_fc6a:
    H2_Snd18();                                       // 0x0041fc74
    return;                                           // -> 0x0041fc7c
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0041fc7c..0x0041fec2: in mode [0x822d68] == 2, a click on a tile of type 0x15 / 0x16 or with flag bits 0x600
// offers to remove the object there (objectAt; not for record 0 or -1, nor a type-4 object with value <= 0x10): the text
// 0x4c5188 + name (0x4c26b0 + type * 20) + 0x4c516c and showPopup(400, 100, 1, 1, 0); answer 0 removes it: type 5
// refunds value / 50 + rateLot / 2 (charged as a negative popup), types below 6 refund their table price (type 4:
// (value + 5) * 10), then clearObjectFootprint. Always ends with rebuildTileCoverage, rebuildHeightfield, frame head.
static int H2_Remove(u8* F) {
    if (G32(0x00822D68) == 2) {                       // 0x0041fc7c / 0x0041fc83
        int x = FD(0x1c), y = FD(0x24);
        u32 i = H2_cell(x, y);
        s8 t = H2_TT(i);                              // 0x0041fc9a
        if (t == 0x15 || t == 0x16 || (H2_TW(i) & 0x600) != 0) {   // 0x0041fca0..0x0041fcb8
            int ebp = CC(0x0040DF80, x, y);           // 0x0041fcc0 objectAt
            u32 ebx = (u32)ebp << 4;                  // 0x0041fcca / 0x0041fccc
            short dx = G16(0x0058BCB8u + ebx);        // 0x0041fccf
            if (dx == 4 && G32(0x0058BCC0u + ebx) <= 0x10) goto L_feb8;   // 0x0041fcd6..0x0041fce3
            if (ebp == -1) goto L_feb8;               // 0x0041fce9
            if (ebp == 0) goto L_feb8;                // 0x0041fcf2
            StrCpy(kBuf, 0x004C5188);                 // 0x0041fcfa..0x0041fd20
            StrCat(kBuf, 0x004C26B0u + 20u * (u32)(int)dx);   // 0x0041fd1a / 0x0041fd22..0x0041fd5e
            StrCat(kBuf, 0x004C516C);                 // 0x0041fd60..0x0041fd89
            if (CC(0x0046D6E0, 0x190, 0x64, 1, 1, 0) != 0) goto L_feb8;   // 0x0041fd8b showPopup / 0x0041fd95
            if (ebp == 0) goto L_feb8;                // 0x0041fd9b (ebp != 0 here: tested at 0x0041fcf2)
            if (G16(0x0058BCB8u + ebx) == 5) {        // 0x0041fda3
                int r = CC(0x0042EF40, FD(0x1c), FD(0x24));   // 0x0041fdbb rateLot
                int v = G32(0x0058BCC0u + ebx);       // 0x0041fdc0
                int esi = v / 50 + r / 2;             // 0x0041fdc8..0x0041fdea
                G32(0x00571FD4) = G32(0x00571FD4) - esi;          // 0x0041fdd4 / 0x0041fdf0 / 0x0041fdf5
                CC(0x0040C890, H2_neg(esi), H2_W(FD(0x1c)), H2_W(FD(0x24)), -1);   // 0x0041fe10 pointsPopup
                int rem = H2_Rem100();                // 0x0041fe15..0x0041fe25
                u32 a = 0x00584212u + H2_row(rem);
                GU16(a) = (u16)(GU16(a) - (u16)esi);  // 0x0041fe2a
            }
            short a = G16(0x0058BCB8u + ebx);         // 0x0041fe32
            if (a < 6) {                              // 0x0041fe39 / 0x0041fe3d
                int esi = G16(0x004C26C2u + 20u * (u32)(int)a);   // 0x0041fe3f..0x0041fe49
                if (a == 4) {                         // 0x0041fe42 / 0x0041fe51
                    int b = G32(0x0058BCC0u + ebx) + 5;           // 0x0041fe53 / 0x0041fe59
                    esi = (int)((u32)b * 10u);                    // 0x0041fe5c / 0x0041fe5f
                }
                CC(0x0040C890, esi, H2_W(FD(0x1c)), H2_W(FD(0x24)), -1);   // 0x0041fe7f pointsPopup
                G32(0x00571FD4) = G32(0x00571FD4) + esi;          // 0x0041fe84 / 0x0041fe96 / 0x0041fe9b
                int rem = H2_Rem100();                // 0x0041fe8f / 0x0041fea1 / 0x0041fea2
                u32 b2 = 0x00584218u + H2_row(rem);
                GU16(b2) = (u16)(GU16(b2) - (u16)esi);            // 0x0041fea7
            }
            CC(0x0040E400, ebp);                      // 0x0041feb0 clearObjectFootprint
        }
    }
L_feb8:
    CC(0x0042F340);                                   // 0x0041feb8 rebuildTileCoverage
    CC(0x0042F7A0);                                   // 0x0041febd rebuildHeightfield
    return kFrame;                                    // 0x0041fec2
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0042097e..0x00420a97: terrain tool t = [0x4c2848] >= 0x80 in mode 1. t == 0xa0 sets flag 0x20 of the tile and
// setGrids(x, y, 0xa0, c) with c = 15 on a type-0x11 tile, else 2; t == 0x280 on a tile with flag 0x20 rewrites the word
// and setGrids(x, y, 0x280, 2), without flag 0x20 the refusal sound. A non-zero c is charged to the cash, to the
// current hole's spend dword (0x575ca0 + hole * 0x208) and to the ledger word 0x584216 of the current row.
static int H2_TerrainSpecial(u8* F, int eax) {
    if (G32(0x00822D68) != 1) return H2_Rebuild(F);   // 0x0042097e / 0x00420985
    int esi = 0;                                      // 0x0042098b
    if (eax == 0xa0) {                                // 0x0042098d / 0x00420992
        int x = FD(0x1c), y = FD(0x24);
        u32 i = H2_cell(x, y);
        H2_TWL(i) = (u8)(H2_TWL(i) | 0x20);           // 0x004209a5
        esi = (H2_TT(i) != 0x11) ? 2 : 0xf;           // 0x004209ad..0x004209be (sub / neg / sbb / and / add)
        CC(0x0040A130, x, y, 0xa0, esi);              // 0x004209c8 setGrids
        eax = G32(0x004C2848);                        // 0x004209cd
    }
    if (eax == 0x280) {                               // 0x004209d5 / 0x004209da
        int x = FD(0x1c), y = FD(0x24);
        u32 i = H2_cell(x, y);
        u16 w = H2_TW(i);                             // 0x004209ed
        if (w & 0x20) {                               // 0x004209f5 / 0x004209f8
            esi = 2;                                  // 0x004209fa
            H2_TW(i) = w;                             // 0x004209ff (same value written back)
            CC(0x0040A130, x, y, 0x280, esi);         // 0x00420a0f setGrids
            goto L_a2f;                               // 0x00420a17
        }
        H2_Snd18();                                   // 0x00420a23
    }
    if (esi == 0) return H2_Rebuild(F);               // 0x00420a2b / 0x00420a2d
L_a2f: {
        int h = G32(0x005685F0);                      // 0x00420a2f
        G32(0x00571FD4) = G32(0x00571FD4) - esi;      // 0x00420a34 / 0x00420a43 / 0x00420a45
        u32 ha = 0x00575CA0u + H2_hole(h);            // 0x00420a3a..0x00420a52
        G32(ha) = G32(ha) + esi;                      // 0x00420a4b / 0x00420a59 / 0x00420a5d
        CC(0x0040C890, H2_neg(esi), H2_W(FD(0x1c)), H2_W(FD(0x24)), -1);   // 0x00420a7d pointsPopup
        int rem = H2_Rem100();                        // 0x00420a82..0x00420a92
        u32 a = 0x00584216u + H2_row(rem);
        GU16(a) = (u16)(GU16(a) - (u16)esi);          // 0x00420a97
    }
    return H2_Rebuild(F);                             // -> 0x00420a9f
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0041fec7..0x00420979: no object tool ([0x4c2854] == -1). Speed 2: frame head. Terrain tool t = [0x4c2848]: -1 goes
// to 0x0040fc24 (rebuildTileCoverage + frame head), >= 0x80 to H2_TerrainSpecial. Otherwise (mode 1 only) paint terrain
// type t on (x, y): [F+0x18] / [F+0xc] = type bits of the unblocked neighbours (all 8 / the even ones), [F+0x2c] = the
// price (signed bytes +4 of the old type's 0x30-byte record and +3 of t's, from 0x578370), [F+0x10] = 1 cleared by:
// a blocked tile, more than 18 holes with t == 0 (or t == 1 with no green neighbour), canAfford, flag 0x8000, a declined
// demolish prompt; [F+0x20] = old type, [F+0x28] = the tile's hole number (flags & 0x1f). Tee (t 0) and green (t 1)
// update the hole records (0x575ab8 tee, 0x575ac0 second tee, 0x575ac8 green) and the "already built" message. Then
// the tile words, placeHabitat, the sound per type kind (switch 0x00421ad4), setGrids, the charge, the type's byte
// 0x578375 into 0x53bbac, and the removal of a tee / green the new type replaced.
static int H2_Terrain(u8* F) {
    int eax, ebx, ecx, esi, ebp, edi, eb;
    u32 i;
    if (G32(0x00567AFC) == 2) return kFrame;          // 0x0041fec7 / 0x0041fece
    eax = G32(0x004C2848);                            // 0x0041fed4
    if (eax == -1) return kRetile;                    // 0x0041fed9 / 0x0041fedc
    if (eax >= 0x80) return H2_TerrainSpecial(F, eax);   // 0x0041fee2 / 0x0041fee7
    ebp = 1;                                          // 0x0041fef2
    if (G32(0x00822D68) != ebp) return H2_Rebuild(F); // 0x0041feed / 0x0041fef9
    FD(0x18) = 0;                                     // 0x0041ff09 (ebx = 0)
    FD(0xc) = 0;                                      // 0x0041ff0d
    for (int k = 0; k < 8; k++) {                     // 0x0041ff11..0x0041ff81 (ebx = k)
        int ddy = G32(0x004C2898u + 4u * (u32)k);     // 0x0041ff11
        int ddx = G32(0x004C2878u + 4u * (u32)k);     // 0x0041ff18
        if (CC(0x0040BF60, FD(0x1c) + ddx, FD(0x24) + ddy) != 0) continue;   // 0x0041ff25 tileBlocked / 0x0041ff2f
        u32 j = (u32)(FD(0x1c) + G32(0x004C2878u + 4u * (u32)k)) * 50u + (u32)FD(0x24) +
                (u32)G32(0x004C2898u + 4u * (u32)k);  // 0x0041ff31..0x0041ff51
        int t = H2_TT(j);                             // 0x0041ff58
        u32 bit = (u32)ebp << ((u32)t & 31u);         // 0x0041ff60 / 0x0041ff62
        FD(0x18) = (int)(FU(0x18) | bit);             // 0x0041ff44 / 0x0041ff64 / 0x0041ff69
        if ((k & 1) == 0) FD(0xc) = (int)(FU(0xc) | bit);   // 0x0041ff66 / 0x0041ff6d / 0x0041ff6f
    }
    {
        int x = FD(0x1c), y = FD(0x24);
        ebx = ebp;                                    // 0x0041ff86
        FD(0x10) = ebx;                               // 0x0041ff8d
        int told = H2_TT(H2_cell(x, y));              // 0x0041ff91
        edi = (int)G8(0x00578374u + (u32)told * 0x30u) +
              (int)G8(0x00578373u + (u32)G32(0x004C2848) * 0x30u);   // 0x0041ff99..0x0041ffb8
        FD(0x2c) = edi;                               // 0x0041ffba
        if (CC(0x0040BF60, x, y) != 0) {              // 0x0041ffbe tileBlocked (pushed at 0x0041ff88) / 0x0041ffc8
            H2_Snd18();                               // 0x0041ffd4
            ebx = 0;                                  // 0x0041ffdc
            FD(0x10) = ebx;                           // 0x0041ffde
        } else {
            int t2 = H2_TT(H2_cell(FD(0x1c), FD(0x24)));   // 0x0041fff2
            if (G8(0x00578374u + (u32)t2 * 0x30u) >= 0x32) H2_Snd18();   // 0x00420000 / 0x00420007 / 0x00420013
        }
    }
    if (G32(0x005685F0) >= 0x13 && G32(0x00822D68) == ebp) {   // 0x0042001b / 0x00420024
        int t = G32(0x004C2848);                      // 0x0042002c
        if (t == 0 || (t == ebp && (FUB(0xc) & 2) == 0)) {   // 0x00420031..0x0042003e
            ebx = 0;                                  // 0x00420040
            FD(0x10) = ebx;                           // 0x00420042
        }
    }
    if (CC(0x00406C30, edi) == 0) {                   // 0x00420047 canAfford / 0x00420051
        ebx = 0;                                      // 0x00420053
        FD(0x10) = ebx;                               // 0x00420055
    }
    ecx = FD(0x1c);                                   // 0x00420059
    esi = FD(0x24);                                   // 0x0042005d
    {
        i = H2_cell(ecx, esi);
        u16 w = H2_TW(i);                             // 0x0042006a
        if (w & 0x8000) {                             // 0x00420072
            ebx = 0;                                  // 0x00420077
            FD(0x10) = ebx;                           // 0x00420079
        }
        FD(0x20) = H2_TT(i);                          // 0x0042007d / 0x00420086
        if (ebx != 0 && (w & 0x400) != 0) {           // 0x00420084 / 0x0042008a / 0x00420090..0x00420099
            int o = CC(0x0040DF80, ecx, esi);         // 0x004200a1 objectAt (ebp = result)
            u32 r = (u32)o << 4;                      // 0x004200ab
            short c = G16(0x0058BCB8u + r);           // 0x004200ae
            int ed = c;                               // 0x004200b9
            if (c == 4 && G32(0x0058BCC0u + r) <= 0x10) {   // 0x004200b5..0x004200c5
                ebx = 0;                              // 0x004200c7
                o = -1;                               // 0x004200c9
                FD(0x10) = ebx;                       // 0x004200cc
            }
            u32 e20 = (u32)ed * 20u;                  // 0x004200d0 / 0x004200d3
            int declined = 0;
            if (G8(0x004C26C0u + e20) > 1 && o != -1) {   // 0x004200d6 / 0x004200dd / 0x004200e3
                StrCpy(kBuf, 0x004C5188);             // 0x004200ec..0x00420112
                StrCat(kBuf, 0x004C26B0u + e20);      // 0x00420114..0x00420144
                StrCat(kBuf, 0x004C516C);             // 0x00420146..0x00420174
                if (CC(0x0046D6E0, 0x190, 0x64, 1, 1, 0) != 0) {   // 0x00420176 showPopup / 0x00420180
                    FD(0x10) = 0;                     // 0x00420182
                    declined = 1;                     // 0x0042018a -> 0x00420199
                }
            }
            if (!declined && ebx != 0) CC(0x0040E400, o);   // 0x0042018c..0x00420191 clearObjectFootprint
            esi = FD(0x24);                           // 0x00420199
            ecx = FD(0x1c);                           // 0x0042019d
        }
    }
    // 0x004201a1
    eb = G32(0x0059E7B8);                             // 0x004201a4
    {
        u16 w = H2_TW(H2_cell(ecx, esi));             // 0x004201b0
        FD(0x28) = w & 0x1f;                          // 0x004201b8..0x004201c3
        if (eb & 0x200000) {                          // 0x004201bd / 0x004201c7
            int t = FD(0x20);                         // 0x004201c9
            if (t == 0 || (t == 1 && (w & 0x80) != 0)) {   // 0x004201cd..0x004201d9
                FD(0x10) = 0;                         // 0x004201e5
                H2_Snd18();                           // 0x004201ed
                eb = G32(0x0059E7B8);                 // 0x004201f2
                esi = FD(0x24);                       // 0x004201f8
                ecx = FD(0x1c);                       // 0x004201fc
            }
        }
    }
    if (G32(0x00822D68) == 1) {                       // 0x00420203 / 0x0042020a
        if (FD(0x10) == 0) return H2_Rebuild(F);      // 0x00420210 / 0x00420216
        int t = G32(0x004C2848);                      // 0x0042021c
        if (t == 0) {                                 // 0x00420221 / 0x00420224
            // 0x00420311: tee
            ebp = G32(0x005685F0);
            u32 e18 = FU(0x18) & 0xfffffffeu;         // 0x00420317 / 0x0042031d
            u32 hr = H2_hole(ebp);                    // 0x0042031b..0x00420325
            if (G32(0x00575AB8u + hr) == 0) {         // 0x00420328 / 0x00420330 -> 0x004204db
                if (e18 & 1) return H2_Rebuild(F);    // 0x004204db / 0x004204de (bit 0 cleared at 0x0042031d)
                int t20 = FD(0x20);                   // 0x004204e4
                if (t20 == 0) {                       // 0x004204ea
                    FD(0x10) = t20;                   // 0x004204ec
                } else if (t20 == 1) {                // 0x004204f2
                    u16 w = H2_TW(H2_cell(ecx, esi)); // 0x00420500
                    if (w & 0x80) {                   // 0x00420508
                        u32 a = H2_hole(w & 0x1f);    // 0x0042050d..0x00420517
                        GU8(0x00575AB0u + a) = 0;     // 0x0042051a
                        G32(0x00575AC8u + a) = 0;     // 0x00420521
                    }
                }
                G32(0x00575AB8u + hr) = ecx;          // 0x0042052e
                G32(0x00575ABCu + hr) = esi;          // 0x0042053a
                H2_TW(H2_cell(ecx, esi)) = (u16)ebp;  // 0x00420543
                if ((eb & 0x40) == 0) {               // 0x00420534 / 0x0042054b
                    G32(0x00575AC0u + hr) = ecx;      // 0x0042054d
                    G32(0x00575AC4u + hr) = esi;      // 0x00420553
                }
                goto L_561;                           // 0x00420559
            }
            if (G32(0x00575AC0u + hr) == 0 && (eb & 0x40) != 0) {   // 0x00420336..0x00420343
                if ((e18 & 1) == 0 && ecx != 0) {     // 0x00420345 (never taken) / 0x0042034a
                    G32(0x00575AC0u + hr) = ecx;      // 0x0042034e
                    G32(0x00575AC4u + hr) = esi;      // 0x00420357
                    i = H2_cell(ecx, esi);
                    H2_TW(i) = (u16)ebp;              // 0x00420363
                    GU8(0x0056988Cu + i) = 0;         // 0x0042036b
                } else {
                    FD(0x10) = 0;                     // 0x00420379
                }
            } else if ((e18 & 1) == 0) {              // 0x00420374 (always 0)
                FD(0x10) = 0;                         // 0x00420379
            }
            // 0x00420381: "already built a tee" message
            StrCpy(kBuf, 0x004C5D20);                 // 0x00420381..0x004203ac
            StrCat(kBuf, (u32)Itoa(ebp, 0x0058A528, 10));   // 0x004203ae / 0x004203b3..0x004203eb
            {
                int hc = G32(0x005685F0);             // 0x004203db
                u32 m = (G32(0x00575AC8u + H2_hole(hc)) != 0) ? 0x004C5CD4u : 0x004C5CA4u;   // 0x004203ed..0x004203fd
                StrCat(kBuf, m);                      // 0x00420402..0x00420430
                StrCat(kBuf, 0x004C5C7C);             // 0x00420432..0x0042045b
                StrCat(kBuf, (u32)Itoa(hc, 0x0058A528, 10));   // 0x0042045d / 0x00420462..0x00420493
                StrCat(kBuf, 0x004C5C48);             // 0x00420495..0x004204be
            }
            CC(0x0040CB00, (int)0x80006000, 1, -1);   // 0x004204c0 startMessage
            ebp = G32(0x005685F0);                    // 0x004204c5
            esi = FD(0x24);                           // 0x004204cb
            ecx = FD(0x1c);                           // 0x004204cf
            goto L_561;                               // 0x004204d6
        }
        ebp = G32(0x005685F0);                        // 0x0042022a
        if (t - 1 != 0) goto L_561;                   // 0x00420230 / 0x00420231
        {
            // 0x00420237: green
            u32 hr = H2_hole(ebp);                    // 0x00420237..0x0042023e
            if (G32(0x00575AC8u + hr) != 0 || ecx == 0 ||
                ((FUB(0xc) & 2) != 0 && G32(0x00575AB8u + hr) == 0)) {   // 0x00420241..0x00420266
                // 0x004202f0
                if (FD(0x20) != 1) {                  // 0x004202f0 / 0x004202f5
                    i = H2_cell(ecx, esi);
                    H2_TWL(i) = (u8)(H2_TWL(i) & 0xe0);   // 0x00420304
                }
                goto L_561;                           // 0x0042030c
            }
            int t20 = FD(0x20);                       // 0x0042026c
            if (t20 == 0) {                           // 0x00420270 / 0x00420272
                u32 a = H2_hole(H2_TWL(H2_cell(ecx, esi)) & 0x1f);   // 0x0042027d..0x0042028e
                GU8(0x00575AB0u + a) = 0;             // 0x00420291
                G32(0x00575AB8u + a) = 0;             // 0x00420298
            } else if (t20 == 1) {                    // 0x004202a4
                if (H2_TWL(H2_cell(ecx, esi)) & 0x80) return H2_Rebuild(F);   // 0x004202b2 / 0x004202ba
            }
            if (ebp >= 0x13) goto L_561;              // 0x004202c0 / 0x004202c3
            G32(0x00575AC8u + hr) = ecx;              // 0x004202cc
            G32(0x00575ACCu + hr) = esi;              // 0x004202d2
            H2_TW(H2_cell(ecx, esi)) = (u16)((u32)ebp | 0x80u);   // 0x004202d8 / 0x004202dd / 0x004202e3
            goto L_561;                               // 0x004202eb
        }
    }
    ebp = G32(0x005685F0);                            // 0x0042055b
L_561:
    // 0x00420561
    if (FD(0x10) == 0) return H2_Rebuild(F);          // 0x00420561..0x00420567
    {
        u32 c = H2_cell(ecx, esi);                    // edi, 0x0042056d..0x00420573
        int mode = G32(0x00822D68);                   // 0x00420576
        H2_TWH(c) = (u8)(H2_TWH(c) & 0xef);           // 0x0042057b
        u16 bx = H2_TW(c);                            // 0x00420583
        if (mode == 1 && (bx & 0x200) == 0) {         // 0x0042058b..0x0042059b
            int r = G32(0x005A34F0) % 12;             // 0x0042059d..0x004205a8
            ebp = G32(0x005685F0);                    // 0x004205aa
            GU8(0x0056988Cu + c) = (u8)((u32)r + 0xcu);   // 0x004205b0 / 0x004205b3
        }
        int t48 = G32(0x004C2848);                    // 0x004205b9
        mode = G32(0x00822D68);                       // 0x004205c1
        if (t48 == 1 && mode == 1) {                  // 0x004205be..0x004205cb
            GU8(0x0056988Cu + c) = (((u8)G32(0x005A34F0) & (u8)mode) != 0) ? 0xff : 0;   // 0x004205cd..0x004205df
        }
        if (G32(0x004C2848) == FD(0x20) && mode != 2) return H2_Rebuild(F);   // 0x004205e5..0x004205f4
        if (mode != 1) {                              // 0x004205fa / 0x004205fd
            G32(0x004C2854) = -1;                     // 0x004207ff / 0x00420802
            G32(0x004C2848) = -1;                     // 0x00420807
            goto L_814;                               // 0x0042080c
        }
        int tool = G32(0x004C2848);                   // 0x00420603
        H2_TW(c) = (u16)((bx & 0xd6ffu) | 0x4000u);   // 0x00420608 / 0x0042060c / 0x00420611
        G8(0x00578804u + c) = 0;                      // 0x00420619
        CC(0x00405E30, ecx, esi, tool);               // 0x00420620 placeHabitat
        ecx = FD(0x1c);                               // 0x00420625
        esi = FD(0x24);                               // 0x00420629
        c = H2_cell(ecx, esi);
        if ((H2_TW(c) & 0x200) == 0) {                // 0x00420639..0x00420649
            int r = G32(0x005A34F0) % 12;             // 0x0042064b..0x00420656
            GU8(0x0056988Cu + c) = (u8)((u32)r + 0xcu);   // 0x00420658 / 0x0042065a
        }
        int t = G32(0x004C2848);                      // 0x00420660 (edi)
        int kind = G8(0x00578376u + (u32)t * 0x30u);  // 0x0042066c
        if ((u32)kind <= 0x11u) {                     // 0x00420673 / 0x00420676
            int snd = 0;
            int sel = GU8(0x00421AF0u + (u32)kind);   // 0x0042067e; table 0x00421ad4 at 0x00420684
            switch (sel) {
            case 0: snd = 0x105; break;               // 0x00420735 (kinds 0, 1)
            case 1: G32(0x005A9CD8) = 1; snd = 0x101; break;   // 0x004206d4 (kind 2): store at 0x004206ea
            case 2: snd = (t != 0xc) ? 0x100 : 0x102; break;   // 0x004206ab (kind 4): sub / neg / sbb / and / add
            case 3: snd = 0x104; break;               // 0x0042068b (kinds 7, 8)
            case 4: snd = 0x102; break;               // 0x00420718 (kind 0xc)
            case 5: snd = 0x103; break;               // 0x004206fb (kind 0x11)
            default: snd = -1; break;                 // 0x00420760
            }
            if (snd != -1) {
                CC(0x0040C500, snd, H2_W(ecx), H2_W(esi), 0);   // 0x00420750 playSoundAt
                esi = FD(0x24);                       // 0x00420755
                ecx = FD(0x1c);                       // 0x00420759
            }
        }
        // 0x00420760
        int t2 = G32(0x004C2848);                     // 0x00420760
        if (t2 == 0) GU8(0x0056988Cu + H2_cell(ecx, esi)) = 0;   // 0x00420766..0x00420770
        int lv = G32(0x00822C88);                     // 0x00420777
        if (lv == 0) {                                // 0x0042077c / 0x0042077e
            if (t2 == 0x10) {                         // 0x00420780 / 0x00420783
                u32 c2 = H2_cell(ecx, esi);
                H2_TWH(c2) = (u8)(H2_TWH(c2) | 1);    // 0x0042078e
            }
        } else if (lv > 3) {                          // 0x004207c7 / 0x004207ca
            if (G8(0x00578376u + (u32)t2 * 0x30u) == 0xd) G32(0x004C2848) = 4;   // 0x004207d2 / 0x004207db
            int t20 = FD(0x20);                       // 0x004207e5
            if (G8(0x00578376u + (u32)t20 * 0x30u) == 0xd) G32(0x004C2848) = t20;   // 0x004207ef / 0x004207f8
        }
        int a = FD(0x20);                             // 0x00420796 / 0x004207e5
        if (a != 1 && a != 0) {                       // 0x0042079a..0x004207a1
            CC(0x0040A130, ecx, esi, FD(0x20), FD(0x2c));   // 0x004207af setGrids
            ebp = G32(0x005685F0);                    // 0x004207b4
            esi = FD(0x24);                           // 0x004207ba
            ecx = FD(0x1c);                           // 0x004207be
        } else {
            ebp = G32(0x005685F0);                    // 0x0042080e
        }
    }
L_814:
    // 0x00420814: charge [F+0x2c] to the cash, the hole's spend dword and the ledger word 0x584216
    edi = FD(0x2c);                                   // 0x00420814
    G32(0x00571FD4) = G32(0x00571FD4) - edi;          // 0x00420818..0x00420821
    {
        u32 ha = 0x00575CA0u + H2_hole(ebp);          // 0x00420826..0x00420846
        G32(ha) = G32(ha) + edi;                      // 0x00420839 / 0x00420853 / 0x00420856
    }
    CC(0x0040C890, H2_neg(edi), H2_W(ecx), H2_W(esi), -1);   // 0x00420858 pointsPopup
    {
        int rem = H2_Rem100();                        // 0x0042085d..0x0042086e
        u32 a = 0x00584216u + H2_row(rem);
        GU16(a) = (u16)(GU16(a) - (u16)edi);          // 0x00420876
    }
    {
        int x = FD(0x1c);                             // 0x0042086a
        int y = FD(0x24);                             // 0x0042087e
        u32 c = H2_cell(x, y);
        int tt = H2_TT(c);                            // 0x0042088b
        GU8(0x0053BBACu + c) = GU8(0x00578375u + (u32)tt * 0x30u);   // 0x00420898 / 0x0042089e
        int t20 = FD(0x20);                           // 0x004208a4
        if (tt == t20) return H2_Rebuild(F);          // 0x004208a8 / 0x004208aa
        if (t20 == 0) {                               // 0x004208b2 / 0x004208b4 -> 0x00420907
            int h = FD(0x28);                         // 0x00420907
            if (h == 0) return H2_Rebuild(F);         // 0x0042090b / 0x0042090d
            if (G32(0x005685F0) > h) G32(0x005685F0) = h;   // 0x00420913..0x0042091b
            u32 a = H2_hole(h);                       // 0x00420921..0x00420928
            int clr = 1;
            if (x == G32(0x00575AC0u + a) && y == G32(0x00575AC4u + a)) {   // 0x0042092b..0x00420939
                u8 f = GU8(0x0059E7B8);               // 0x0042093b
                G32(0x00575AC0u + a) = 0;             // 0x00420941
                if (f & 0x40) clr = 0;                // 0x00420947 / 0x0042094a
            }
            if (clr) G32(0x00575AB8u + a) = 0;        // 0x0042094c
            GU8(0x00575AB0u + a) = 0;                 // 0x00420952
            if (G32(0x004C2848) == 1 && (H2_TWL(c) & 0x80) != 0) return H2_Rebuild(F);   // 0x00420959..0x0042096b
            H2_TWL(c) = (u8)(H2_TWL(c) & 0xe0);       // 0x00420971
            return H2_Rebuild(F);                     // 0x00420979
        }
        if (t20 - 1 != 0) return H2_Rebuild(F);       // 0x004208b6 / 0x004208b7
        int h = FD(0x28);                             // 0x004208bd
        if (h == 0) return H2_Rebuild(F);             // 0x004208c1 / 0x004208c3
        if (G32(0x005685F0) > h) G32(0x005685F0) = h; // 0x004208c9..0x004208d1
        u32 a = H2_hole(h);                           // 0x004208d7..0x004208de
        GU8(0x00575AB0u + a) = 0;                     // 0x004208e1
        G32(0x00575AC8u + a) = 0;                     // 0x004208e8
        if (G32(0x004C2848) == 0) return H2_Rebuild(F);   // 0x004208ee / 0x004208f4
        H2_TWL(c) = (u8)(H2_TWL(c) & 0xe0);           // 0x004208fa
        return H2_Rebuild(F);                         // 0x00420902
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// ENTRY 0x0041ecbb. in[0..5] = eax, ebp, ebx, ecx, edx, esi at 0x0041ecbb (verified with c3fy_reach.py 0x40fc29
// 0x41ecbb): eax = [0x59e7b8] (0x0041e4be), ebp = 0, ebx = 1, ecx = [0x822d64] (0x0041e400), edx = [0x567afc]
// (0x0041e461 / 0x0041e4b8), esi = -1 (0x0041e3ac). edi is not live. Returns kFrame (0x0040fc29) or kRetile
// (0x0040fc24); this subtree has no edge to 0x00420e35.
// 0x0041ecbb..0x0041f34e: with flag 0x8000000 in mode 1 the demolish tool acts on ([0x543d08], [0x543d0c]); with no
// tool active a mode-1 click goes to H2_Select; mode 2 (the demolish tool) either demolishes the marked tile (second
// click on it) or marks the tile (clearMatching of the old mark, buildToolLabel, queueMessage); speed 3 in mode 1 with
// [0x5a34e4] <= 0 and the cursor above y 0x1e0 switches to speed 4 and copies the selected golfer's +0xc4/+0xc8 into
// +0xcc/+0xd0. A blocked tile or a click near the top message (0x569498 set, cursor x within 200 of 400) ends the frame.
int Ph_0041ecbb(u8* F, const int* in) {
    int eax = in[0], ebp = in[1], ebx = in[2], ecx = in[3], edx = in[4], esi = in[5];
    int f = eax & 0x8000000;                          // 0x0041ecbb
    eax = G32(0x00822D68);                            // 0x0041ecc0
    if (f != 0 && eax == ebx) {                       // 0x0041ecc5 / 0x0041ecc9
        CC(0x0040A4E0, G32(0x00543D08), G32(0x00543D0C));   // 0x0041eccb..0x0041ecd9 demolishObject
        return H2_Rebuild(F);                         // 0x0041ece1 -> 0x00420a9f
    }
    // 0x0041ece6
    if ((edx == 4 || edx == 5 || (G32(0x004C2848) == esi && G32(0x004C2854) == esi) || edx == 2) &&
        FD(0x14) == esi && edx != 3 && eax == ebx)    // 0x0041ece6..0x0041ed1e
        return H2_Select(F, ebp);                     // 0x0041ed24
    // 0x0041f169
    if (eax == 2) {                                   // 0x0041f16c
        if (G32(0x004C2854) != esi || GU8(0x00569498) != 0 || G32(0x0053DF54) != ebp ||
            G32(0x004C2E10) != esi) {                 // 0x0041f172..0x0041f191
            eax = 0;                                  // 0x0041f193
            G32(0x00822D68) = eax;                    // 0x0041f195
        }
        G32(0x004C2E10) = esi;                        // 0x0041f19d
        G32(0x004C2854) = esi;                        // 0x0041f1a3
        G32(0x0053DF54) = ebp;                        // 0x0041f1a9
        G8(0x00569498) = 0;                           // 0x0041f1af
        if (eax == 2) {                               // 0x0041f19a / 0x0041f1b6
            int c = G32(0x004C2E24);                  // 0x0041f1bc
            int s = FD(0x1c);                         // 0x0041f1c2
            int a = G32(0x005A6368);                  // 0x0041f1c6
            if (c == s) {                             // 0x0041f1cb / 0x0041f1cd
                int y = FD(0x24);                     // 0x0041f1cf
                if (a == y) {                         // 0x0041f1d3 / 0x0041f1d5
                    CC(0x0040A4E0, s, y);             // 0x0041f1d9 demolishObject
                    CC(0x0040C860, H2_W(FD(0x1c)), H2_W(FD(0x24)));   // 0x0041f1f9 clearMatching
                    G32(0x004C2E24) = -1;             // 0x0041f201
                    return kFrame;                    // 0x0041f20b
                }
            }
            CC(0x0040C860, H2_W(c), H2_W(a));         // 0x0041f210..0x0041f223 clearMatching
            TC(0x004762D0, Root(), 0x0051B360, ebp, ebp, ebp);   // 0x0041f228..0x0041f239 setDrawTarget
            CC(0x0040A160, FD(0x1c), FD(0x24));       // 0x0041f248 buildToolLabel
            CC(0x0040C720, H2_W(FD(0x1c)), H2_W(FD(0x24)), (int)0x800023E8);   // 0x0041f26e queueMessage
            int nx = FD(0x1c), ny = FD(0x24);         // 0x0041f273 / 0x0041f277
            edx = G32(0x00567AFC);                    // 0x0041f27b
            G32(0x004C2E24) = nx;                     // 0x0041f284
            eax = G32(0x00822D68);                    // 0x0041f289
            G32(0x005A6368) = ny;                     // 0x0041f28e
            ecx = G32(0x00822D64);                    // 0x0041f294
            esi = -1;                                 // 0x0041f29a
        }
    }
    // 0x0041f29d
    if (eax == ebx && edx != ebp && edx == 3 && G32(0x005A34E4) <= ebp && ecx < 0x1e0) {   // 0x0041f29d..0x0041f2b8
        int g = G32(0x005A59F8);                      // 0x0041f2ba
        G32(0x00567AFC) = 4;                          // 0x0041f2bf
        u32 r = (u32)g << 8;                          // 0x0041f2c9
        G32(0x004C2848) = esi;                        // 0x0041f2cc
        G32(0x004C2854) = esi;                        // 0x0041f2d2
        G32(0x00579584u + r) = G32(0x0057957Cu + r);  // 0x0041f2d8 / 0x0041f2de
        G32(0x00579588u + r) = G32(0x00579580u + r);  // 0x0041f2e4 / 0x0041f2ea
        return kFrame;                                // 0x0041f2f0
    }
    // 0x0041f2f5
    if (CC(0x0040BF60, FD(0x1c), FD(0x24)) != 0) return kFrame;   // 0x0041f2ff tileBlocked / 0x0041f309
    if (GU8(0x00569498) != 0) {                       // 0x0041f30f / 0x0041f316
        if (G32(0x00822D64) < G32(0x00587EA0) + 8) {  // 0x0041f318..0x0041f328
            int d = G32(0x00822D60) - 0x190;          // 0x0041f32a / 0x0041f330
            u32 s = (u32)(d >> 31);                   // 0x0041f336 cdq
            int ad = (int)(((u32)d ^ s) - s);         // 0x0041f337 / 0x0041f339
            if (ad < 0xc8) {                          // 0x0041f33b / 0x0041f340
                G8(0x00569498) = 0;                   // 0x0041f342
                return kFrame;                        // 0x0041f349
            }
        }
    }
    // 0x0041f34e
    int t = G32(0x004C2854);                          // 0x0041f34e
    if (t == -1) return H2_Terrain(F);                // 0x0041f353 / 0x0041f356 / 0x0041f358
    H2_Build(F, t);                                   // 0x0041f35e..0x0041fc79
    return H2_Remove(F);                              // 0x0041fc7c..0x0041fec2
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x00420e0f..0x00420e32 (from 0x00420e0f the flags word is re-read; 0x00420e15 is also entered with the incoming
// ecx from 0x00420b48): clear flag 0x4000000, saveGame(0x4c59e8), startSoundSlot(0x2d), then the title menu.
static int H2_QuitSave(int ecx) {
    ecx = (int)((u32)ecx & 0xfbffffffu);              // 0x00420e15
    G32(0x0059E7B8) = ecx;                            // 0x00420e20
    CC(0x0040B4A0, 0x004C59E8);                       // 0x00420e1b / 0x00420e26 saveGame
    CC(0x00448200, 0x2d);                             // 0x00420e2b / 0x00420e2d startSoundSlot (add esp, 8 at 0x00420e32)
    return kMenu;                                     // -> 0x00420e35
}

// ENTRY 0x00420acb (reached from H1: 0x0041e6f4 jmp, and through H2_Tail_00420ac6). eax = [0x5a6374] (read at
// 0x0041e6e6 or 0x00420ac6), ecx = [0x59e7b8] (read at 0x0041e6b8). Returns kFrame or kMenu. 0x00420acb..0x00420d7e:
// with [0x5a6374] >= 3 and flag 0x1000000 clear, popup 0x4c5ad0 (showPopup(250, 150, 1, 1, 0)); answer 0 sets flag
// 0x1000000 and clears [0x5a6374], any other answer goes to the quit save. Flag 0x1000000 set and [0x5a6374] >= 3: the
// quit save. Otherwise the end-of-game text (0x4000000 set: 0x4c5ab0 + name 0x4d6098 + 0x4c5aa0; clear: 0x4c5a94,
// 0x4c5a8c / 0x4c5a84 by the word [0x5a6d3c] < 10, 0x4c5a78, the name, 0x4c5a6c / 0x4c5a60 by bit 0x80 of [0x4d60a9],
// 0x4c5a50) + 0x4c5a2c + 0x4c5a08 + 0x4c59f4 and showPopup(400, 100, 1, 1, 0): 1 saveGameDialog then the quit save,
// 0 and -1 the frame head, any other the quit save. 0x00420d83..0x00420e0a is P7's.
static int H2_Tail_00420acb(int eax, int ecx) {
    int fl = ecx & 0x1000000;                         // 0x00420acb
    G32(0x0059B734) = 0;                              // 0x00420ad1
    if (fl != 0) {                                    // 0x00420adb
        if (eax >= 3) return H2_QuitSave(ecx);        // 0x00420b45 / 0x00420b48 -> 0x00420e15 (ecx not re-read)
    } else if (eax >= 3) {                            // 0x00420add / 0x00420ae0
        StrCpy(kBuf, 0x004C5AD0);                     // 0x00420ae2..0x00420b15
        if (CC(0x0046D6E0, 0xfa, 0x96, 1, 1, 0) != 0) // 0x00420b17 showPopup / 0x00420b21
            return H2_QuitSave(G32(0x0059E7B8));      // 0x00420e0f
        int g = G32(0x0059E7B8);                      // 0x00420b27
        G32(0x005A6374) = 0;                          // 0x00420b2c
        G32(0x0059E7B8) = (int)((u32)g | 0x1000000u); // 0x00420b36 / 0x00420b3b
        return kFrame;                                // 0x00420b40
    }
    // 0x00420b4e
    u32 edi;
    if (ecx & 0x4000000) {                            // 0x00420b4e / 0x00420b54
        StrCpy(kBuf, 0x004C5AB0);                     // 0x00420b56..0x00420b79
        StrCat(kBuf, 0x004D6098);                     // 0x00420b7b..0x00420ba4
        edi = 0x004C5AA0;                             // 0x00420ba6
    } else {
        StrCpy(kBuf, 0x004C5A94);                     // 0x00420bb0..0x00420bdb
        edi = (G16(0x005A6D3C) < 0xa) ? 0x004C5A8Cu : 0x004C5A84u;   // 0x00420bd3 / 0x00420bdd..0x00420be4
        StrCat(kBuf, edi);                            // 0x00420be9..0x00420c0f
        StrCat(kBuf, 0x004C5A78);                     // 0x00420c11..0x00420c3a
        StrCat(kBuf, 0x004D6098);                     // 0x00420c3c..0x00420c6a
        edi = (GU8(0x004D60A9) & 0x80) ? 0x004C5A6Cu : 0x004C5A60u;  // 0x00420c60 / 0x00420c6c..0x00420c75
        StrCat(kBuf, edi);                            // 0x00420c7a..0x00420ca0
        edi = 0x004C5A50;                             // 0x00420ca2
    }
    StrCat(kBuf, edi);                                // 0x00420ca7..0x00420cd2
    StrCat(kBuf, 0x004C5A2C);                         // 0x00420cd4..0x00420d04
    StrCat(kBuf, 0x004C5A08);                         // 0x00420d06..0x00420d2f
    StrCat(kBuf, 0x004C59F4);                         // 0x00420d31..0x00420d5a
    int r = CC(0x0046D6E0, 0x190, 0x64, 1, 1, 0);     // 0x00420d5c showPopup
    if (r == 1) {                                     // 0x00420d64 / 0x00420d67
        CC(0x00405B10);                               // 0x00420e0a saveGameDialog
        return H2_QuitSave(G32(0x0059E7B8));          // 0x00420e0f
    }
    if (r == 0) return kFrame;                        // 0x00420d6d / 0x00420d6f
    if (r != -1) return H2_QuitSave(G32(0x0059E7B8)); // 0x00420d75 / 0x00420d78 -> 0x00420e0f
    return kFrame;                                    // 0x00420d7e
}

// ENTRY 0x00420ac6 (reached from H1: 0x0041e6d4 / 0x0041e6e0 jne). ecx = [0x59e7b8] (read at 0x0041e6b8).
static int H2_Tail_00420ac6(int ecx) {
    return H2_Tail_00420acb(G32(0x005A6374), ecx);    // 0x00420ac6
}

}  // namespace
// ====================================================================================================================
// MainLoopBody: the top-level state machine (re/analysis/sim/mainLoop_structure.md). Setup once, then the menu /
// session / frame loop. Entered from the naked thunk with F = the original's frame base.
// ====================================================================================================================
namespace {

// Negctl control: c3fyNegMode() at the top of this file (modes 1 and 2), source shim/src/re/c3fy_negctl.cpp.

void __stdcall MainLoopBody(u8* F) {
    Setup(F);                                       // 0x0040f5e2..0x0040f9f7 -> the menu
    for (;;) {
        int sel = Menu(F);                          // 0x00420e35..0x00421610 (carries ebx to 0x0040f9fc)
        int act = Session(F, sel);                  // 0x0040f9fc..0x0040fc24
        if (act == sQuit) return;                   // the ebx == -2 quit path ran; mainLoop returns (0x0040fb04)
        if (act == sMenu) continue;                 // a load failed: jmp 0x00420e35
        // sFrame: enter the frame, first via 0x0040fc24 (rebuildTileCoverage)
        int retile = 1;
        for (;;) {
#if defined(C3FY_NEG)
            if (c3fyNegMode() == 1) Rnd(1);         // negctl mode 1: extra RNG draw, shifts the seed (RED control)
#endif
            FramePreamble(F, retile);               // 0x0040fc24 / 0x0040fc29 .. 0x00410604
            Ph_00410604(F);                         // P1  0x00410604..0x0041234e
            Ph_0041234e(F);                         // P2  0x0041234e..0x0041353d
            Ph_0041353d(F);                         // P3  0x0041353d..0x00415847
            Ph_00415847(F);                         // P4  0x00415847..0x004175f2
            Ph_004175f2(F);                         // P5  0x004175f2..0x00418bc7
            int r6 = Ph_00418bc7(F);                // P6  0x00418bc7..0x00419d06 (eax = [0x004c2854])
            Ph_00419d06(F, r6);                     // P7  0x00419d06..0x0041c0c5
            int h2in[6];
            int d = Ph_0041c0c5(F, h2in);           // H1  0x0041c0c5..0x0041ecbb
            if (d == kNext) d = Ph_0041ecbb(F, h2in);  // H2  0x0041ecbb..0x00420ac1
            if (d == kMenu) break;                  // jmp 0x00420e35 (back to the title menu)
            retile = (d == kRetile) ? 1 : 0;        // kRetile -> 0x0040fc24, kFrame -> 0x0040fc29
        }
    }
}

}  // namespace
