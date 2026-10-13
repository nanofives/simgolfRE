// C3 batch c3er (round 31, 2026-10-13) of golf_clean.exe: the largest end-of-career screens.
// Fixtures: re/frida/js/fixtures.d/c3er.js. Registry: re/frida/registry.d/c3er.py.
// Purposes and per-jcc tables: log/c3/c3er_purpose.md. Notes and drops: log/c3/c3er_notes.txt.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and
// cites the instruction each fact comes from. __thiscall is emulated with __fastcall (ecx = this, edx unused); callees
// run through their original addresses, so a hooked callee uses its own reimplementation during the A/B.
#include "hooks.h"

#include <string.h>

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline char* B(void* p) { return reinterpret_cast<char*>(p); }

typedef int(__cdecl* C0_t)();
typedef void(__cdecl* V1_t)(int);
typedef int(__cdecl* C4_t)(const char*, int, int, int);
typedef char*(__cdecl* Itoa_t)(int value, char* buf, int radix);
typedef void*(__fastcall* T0p_t)(void* ecx, void* edx);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* T1t_t)(void* ecx, void* edx, const char* a);
typedef int(__fastcall* T4_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d);
typedef int(__fastcall* T5_t)(void* ecx, void* edx, int a, int b, int c, int d, int e);
typedef int(__fastcall* T5p_t)(void* ecx, void* edx, void* a, int b, int c, int d, int e);
typedef int(__fastcall* T5l_t)(void* ecx, void* edx, const char* a, void* b, int c, int d, int e);

// Callees of drawScoreSummary, by the address of the call that reaches each one.
const C0_t kAddScore = reinterpret_cast<C0_t>(0x004732d0);          // 0x0047348f
const T0p_t kImageCtor = reinterpret_cast<T0p_t>(0x00474ae0);       // 0x00473498
const T5l_t kLoadImage = reinterpret_cast<T5l_t>(0x00475840);       // 0x004734bf, 0x00473501
const T5p_t kFillRegion = reinterpret_cast<T5p_t>(0x00475c90);      // 0x004734e5, 0x00473595  Surface_fillRegion
const T4_t kSetDrawTarget = reinterpret_cast<T4_t>(0x004762d0);     // 0x004735c0, 0x004736ac, 0x00473844, 0x0047390d
const T1t_t kTextPut = reinterpret_cast<T1t_t>(0x00477250);         // 0x004735d9
const T5_t kFillRect = reinterpret_cast<T5_t>(0x00478b80);          // 0x0047360a, 0x00473628  Widget_fillRect
const C4_t kDrawCenteredText = reinterpret_cast<C4_t>(0x00404b70);  // eleven calls, 0x00473638 .. 0x004739f8
const V1_t kAppendCents = reinterpret_cast<V1_t>(0x0042dd50);       // 0x0047377c
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004ad425);          // 0x004736f9, 0x004737cd, 0x0047388e, 0x0047394b
const T1_t kRefreshFull = reinterpret_cast<T1_t>(0x00480c80);       // 0x00473a19  Window::refreshFull
const V1_t kPumpAndRedraw = reinterpret_cast<V1_t>(0x0045c0c0);     // 0x00473a20
const T0p_t kImageDtor = reinterpret_cast<T0p_t>(0x00474c40);       // 0x00473a37

// The scoreboard row written by addScore 0x004732d0 (shim/src/re/c3ah.cpp): name +0, text +0x40, fun +0x80,
// skill +0x84, cash +0x88, level +0x96 (word), id +0x98; stride 0x9c (0x0047350c..0x0047351b computes 0x9c * i:
// lea ebp,[eax+eax*4]; shl ebp,3; sub ebp,eax; shl ebp,2), table base 0x00541ce0 (0x00473522).
struct ScoreRow {
    char name[0x40];
    char text[0x40];
    int fun;    // +0x80, read at 0x004737c6 as [ecx + 0x541d60]
    int skill;  // +0x84, read at 0x00473775 as [ecx + 0x541d64]
    int cash;   // +0x88, read at 0x004736e1 as [eax + 0x541d68]
    char gap[0x96 - 0x8c];
    short level;  // +0x96, read sign-extended at 0x00473942 as [eax + 0x541d76]
    int id;       // +0x98
};

char* const kText = reinterpret_cast<char*>(0x0051a068);    // the shared text buffer (0x004736cb, 0x00473873, ...)
char* const kNumBuf = reinterpret_cast<char*>(0x008392ac);  // this function's itoa scratch (0x004736d0, 0x004737a8, ...)

// 0x004734c4 mov eax, 0x00519a60 / 0x004734ce neg eax / 0x004734d0 sbb eax, eax / 0x004734d7 and eax, 0x00519cd4
// (and the same sequence on ecx at 0x0047353b / 0x00473554 / 0x00473557 / 0x0047355f): the immediate 0x00519a60 is
// not zero, so both fill calls get the constant 0x00519cd4.
void* const kFillSource = reinterpret_cast<void*>(0x00519cd4);

}  // namespace

// ====================================================================================================================
// 0x00473470  drawScoreSummary
// ====================================================================================================================
// __cdecl, no arguments, `ret` at 0x00473a54 (callee pops nothing). The SEH frame 0x00473470..0x0047347e and the
// unwind state slots ([esp+0x2f8] = 0 at 0x004734b4, [esp+0x2e4] = -1 at 0x00473a2c) only matter if a callee throws:
// they are not reproduced.
typedef void(__cdecl* DrawScoreSummary_t)();
DrawScoreSummary_t DrawScoreSummary_orig;

void __cdecl DrawScoreSummary_re() {
    // The Image lives in the frame at [esp+0x24] (0x00473494); the frame is 0x2cc bytes (0x00473485), so the object
    // is at most 0x2a8 bytes. 0x2c0 is used here for the same reason c3ei's loaders use 0x2c0.
    alignas(16) unsigned char img[0x2c0];
    void* const image = img;

    kAddScore();                            // 0x0047348f
    kImageCtor(image, 0);                   // 0x00473494 / 0x00473498

    // loadImage(this, name, 0x005199c8, 0, 0x100, 0) (pushes 0x0047349d..0x004734ab, call 0x004734bf)
    kLoadImage(image, 0, reinterpret_cast<const char*>(0x004e418c), reinterpret_cast<void*>(0x005199c8), 0, 0x100, 0);
    // Surface_fillRegion(this, 0x00519cd4, 0, 0, 0x320, 0x258) (pushes 0x004734c9..0x004734e0, call 0x004734e5)
    kFillRegion(image, 0, kFillSource, 0, 0, 0x320, 0x258);
    // the second image into the same object (pushes 0x004734ea..0x004734f8, call 0x00473501)
    kLoadImage(image, 0, reinterpret_cast<const char*>(0x004e4170), reinterpret_cast<void*>(0x005199c8), 0, 0x100, 0);

    for (int i = 0; i < 10; ++i) {  // 0x00473506 (i = 0), 0x00473a04 / 0x00473a05 / 0x00473a0c (i + 1 < 10)
        // cell = the dword table at 0x004e3e00 (0x0047350f); the row (0x00473522 / 0x00473528)
        const int cell = G<int>(0x004e3e00 + 4 * i);
        ScoreRow* const row = reinterpret_cast<ScoreRow*>(0x00541ce0 + 0x9c * i);
        if (row->name[0] == 0) continue;  // 0x0047352e / 0x00473530

        const int band = cell / 5;   // 0x00473536..0x00473549 (magic 0x66666667, sar edx, 1), kept in edi
        const int col = cell % 5;    // 0x0047354b..0x0047355d (idiv by 5), remainder in edx
        const int cellX = col * 160; // 0x00473567 / 0x0047356c (lea edx+edx*4; shl 5)

        // Surface_fillRegion(this, 0x00519cd4, cellX, band ? 0x130 + 0x28 : 0x28, 0xa0, band ? 0x130 - 0x30 : 0x130)
        // (0x0047356f..0x0047358e compute the two selected constants, pushes 0x00473580..0x00473590, call 0x00473595)
        kFillRegion(image, 0, kFillSource, cellX, band != 0 ? 0x158 : 0x28, 0xa0, band != 0 ? 0x100 : 0x130);

        const int cx = cellX + 0x50;                    // 0x004735a2
        const int nameY = band != 0 ? 0x23f : 0x142;    // 0x004735a7..0x004735ba (0x142 + 0xfd, or 0x142)
        // setDrawTarget([0x004c1570], 0x0051b360, 0, 0, 0) (pushes 0x004735a5..0x004735b5, call 0x004735c0)
        kSetDrawTarget(G<void*>(0x004c1570), 0, 0x0051b360, 0, 0, 0);

        const int selected = G<int>(0x008392a8);  // written by addScore; read at 0x004735c9 and 0x00473651
        if (i == selected) {                      // 0x004735ce / 0x004735d0
            // Text_put([0x004c1570], row->name) returns the run width (0x004735d9)
            const int w = kTextPut(G<void*>(0x004c1570), 0, row->name);
            const int half = (w + 4) / 2;  // 0x004735de..0x004735ec (add 4; cdq; sub eax,edx; sar eax,1)
            // Widget_fillRect(0x00519cd4, cx - half, nameY + 13, cx + half, nameY + 13, 0x80007ff0)
            kFillRect(kFillSource, 0, cx - half, nameY + 0xd, cx + half, nameY + 0xd, 0x80007ff0);  // 0x0047360a
            // the same span one row higher in a second colour (0x00473617..0x00473622, call 0x00473628)
            kFillRect(kFillSource, 0, cx - half, nameY + 0xc, cx + half, nameY + 0xc, 0x80004206);
        }

        kDrawCenteredText(row->name, cx, nameY - 1, 0x80000000);  // 0x0047362d..0x00473638
        kDrawCenteredText(row->name, cx, nameY + 1, 0x80007ff0);  // 0x0047363d..0x00473648
        // 0x0047364d..0x00473662: (i != selected) adds 0xffffc207 to 0x80007fff, giving 0x80004206
        kDrawCenteredText(row->name, cx, nameY, i != selected ? 0x80004206 : 0x80007fff);  // call 0x0047366b

        // the detail column's first baseline (0x00473670..0x00473683: 0xc2 + 0x119 when band != 0, else 0xc2)
        int y = band != 0 ? 0x1db : 0xc2;
        if (band == 0) {
            y += 2 * (5 * i - 0xf);  // 0x0047368d / 0x00473691
        } else {
            // setDrawTarget([0x004c1570], 0x00519fd8, 0, 0, 0) (pushes 0x0047369d..0x004736a3, call 0x004736ac)
            y += 4 * i - 0x20;  // 0x004736a8
            kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00519fd8, 0, 0, 0);
        }

        // "Cash: " + cash / 10 (strcpy 0x004736b1..0x004736df, cash at 0x004736e1, /10 at 0x004736e7..0x004736f6,
        // itoa 0x004736f9, strcat 0x00473700..0x00473732, draw 0x00473734)
        strcpy(kText, reinterpret_cast<const char*>(0x004e4168));
        strcat(kText, kItoa(row->cash / 10, kNumBuf, 10));
        kDrawCenteredText(kText, cx, y, 0x80004206);

        // the line step (0x00473739..0x0047374b: 0xc - 2 when band != 0, else 0xc)
        const int step = band != 0 ? 0xa : 0xc;
        y -= step;  // 0x00473752

        // "Skill: " + appendCents(skill) (strcpy 0x0047373d..0x0047376f, skill at 0x00473775, call 0x0047377c,
        // draw 0x0047378d)
        strcpy(kText, reinterpret_cast<const char*>(0x004e4160));
        kAppendCents(row->skill);
        kDrawCenteredText(kText, cx, y, 0x80004206);

        y -= step;  // 0x00473792

        // "Fun: " + fun (strcpy 0x00473796..0x004737c0, fun at 0x004737c6, itoa 0x004737cd, strcat and draw 0x00473808)
        strcpy(kText, reinterpret_cast<const char*>(0x004e4158));
        strcat(kText, kItoa(row->fun, kNumBuf, 10));
        kDrawCenteredText(kText, cx, y, 0x80004206);

        y -= step + (band != 0 ? 2 : 8);  // 0x0047381a..0x0047382c (8 - 6 when band != 0, else 8)
        // setDrawTarget([0x004c1570], band ? 0x0051b360 : 0x00519928, 0, 0, 0) (0x0047382e..0x00473839, call 0x00473844)
        kSetDrawTarget(G<void*>(0x004c1570), 0, band != 0 ? 0x0051b360 : 0x00519928, 0, 0, 0);

        // the total: (cash / 10 + skill + fun) * (level + 1) (0x00473849..0x0047388a), written over an emptied buffer
        // (0x00473873) and drawn three times (0x004738cc, 0x004738e0, 0x004738f1)
        kText[0] = 0;
        strcat(kText, kItoa((row->cash / 10 + row->skill + row->fun) * (row->level + 1), kNumBuf, 10));
        kDrawCenteredText(kText, cx, y - 1, 0x80000000);
        kDrawCenteredText(kText, cx, y + 1, 0x80007ff0);
        kDrawCenteredText(kText, cx, y, 0x80005288);

        y -= 0xd;  // 0x004738ff
        kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00519fd8, 0, 0, 0);  // 0x004738f6..0x00473908, call 0x0047390d

        // "Total Score (x" + (level + 1) + ")" (strcpy 0x00473912..0x0047393c, level at 0x00473942, itoa 0x0047394b,
        // the ")" at 0x004c59e0 strcat'ed at 0x00473986..0x004739af, draw 0x004739b1)
        strcpy(kText, reinterpret_cast<const char*>(0x004e4148));
        strcat(kText, kItoa(row->level + 1, kNumBuf, 10));
        strcat(kText, reinterpret_cast<const char*>(0x004c59e0));
        kDrawCenteredText(kText, cx, y, 0x80004206);

        y -= 0xc;  // 0x004739bd
        if (i == 0) {  // 0x004739c0 / 0x004739c2
            // the fixed address 0x00541d20 is row 0's text field (0x004739c4), reached only on this pass
            y += -2;  // 0x004739ce
            strcpy(kText, reinterpret_cast<const char*>(0x00541d20));
            kDrawCenteredText(kText, cx, y, 0x80004206);  // 0x004739d7..0x004739f8
        }
    }

    kRefreshFull(reinterpret_cast<void*>(0x00519a60), 0, 0);  // 0x00473a12 / 0x00473a14 / 0x00473a19
    kPumpAndRedraw(0);                                        // 0x00473a1e / 0x00473a20
    kImageDtor(image, 0);                                     // 0x00473a28 / 0x00473a37
}

// 0x00473470  drawScoreSummary
SG_HOOK("golf_clean.exe", 0x00473470, drawScoreSummary, DrawScoreSummary_re, DrawScoreSummary_orig);

// ====================================================================================================================
// 0x0044cff0  CareerRetirementFlow
// ====================================================================================================================
namespace {

typedef void(__cdecl* V0_t)();
typedef int(__cdecl* C3i_t)(int, int, int);
typedef int(__cdecl* C4i_t)(int, int, int, int);
typedef int(__cdecl* C5i_t)(int, int, int, int, int);
typedef void(__cdecl* V2_t)(int, int);
typedef int(__fastcall* T4i_t)(void* ecx, void* edx, int a, int b, int c, int d);
typedef int(__fastcall* T5s_t)(void* ecx, void* edx, void* a, const char* b, int c, int d, int e);
typedef int(__fastcall* T5x_t)(void* ecx, void* edx, void* other, int surf, int x, int y, int flag);
typedef int(__fastcall* T4p_t)(void* ecx, void* edx, void* a, int b, int c, void* d);
typedef int(__fastcall* T6c_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f);
typedef int(__fastcall* T7b_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g);
typedef void(__fastcall* Tv_t)(void* ecx, void* edx);
typedef void(__fastcall* T2o_t)(void* ecx, void* edx, int* x, int* y);

const Tv_t kSurfaceCtor = reinterpret_cast<Tv_t>(0x00473ab0);         // six calls 0x0044d02d..0x0044d08b
const Tv_t kSurfaceFree = reinterpret_cast<Tv_t>(0x00473ae0);         // twelve calls 0x0044e529..0x0044e5fa
const T7b_t kBlitTakeDirty = reinterpret_cast<T7b_t>(0x00473c60);     // six calls 0x0044d0f6..0x0044d1d9
const T4p_t kImageDraw = reinterpret_cast<T4p_t>(0x00473e60);         // 0x0044e405, 0x0044e4de
const T5x_t kStretchBlit = reinterpret_cast<T5x_t>(0x00473f60);       // 0x0044d252, 0x0044dfca, ... 0x0044e3c2
const T6c_t kDrawSprite = reinterpret_cast<T6c_t>(0x00474440);        // 0x0044d226
const Tv_t kImageReleaseAll = reinterpret_cast<Tv_t>(0x00474cb0);     // 0x0044d1e5
const T6c_t kSurfaceCreateFull = reinterpret_cast<T6c_t>(0x00474dd0); // 0x0044d0b1
const T4i_t kSetQuad = reinterpret_cast<T4i_t>(0x00476310);           // eleven calls 0x0044d298..0x0044e3d7
const T5s_t kDrawStringAt = reinterpret_cast<T5s_t>(0x00476700);      // fifteen calls 0x0044d334..0x0044e387
const T5s_t kDrawStringAt2 = reinterpret_cast<T5s_t>(0x00476750);     // eight calls 0x0044d4ba..0x0044dacb
const T2o_t kViewToLocal = reinterpret_cast<T2o_t>(0x0047ab50);       // 0x0044e44b
const C5i_t kPlaySound = reinterpret_cast<C5i_t>(0x004481b0);         // 0x0044d1f4
const V1_t kStartSoundSlot = reinterpret_cast<V1_t>(0x00448200);      // 0x0044e51a
const C0_t kFlushRedraw = reinterpret_cast<C0_t>(0x0045ae70);         // 0x0044e4ef
const V2_t kWaitTicks = reinterpret_cast<V2_t>(0x0045bf80);           // 0x0044e41a, 0x0044e42f
const V0_t kPumpMessages = reinterpret_cast<V0_t>(0x0045c030);        // 0x0044e437
const V0_t kPumpLight = reinterpret_cast<V0_t>(0x0045c150);           // 0x0044e504
const C4i_t kLoadStoryStrings = reinterpret_cast<C4i_t>(0x00466b70);  // 0x0044df31
const C3i_t kClamp = reinterpret_cast<C3i_t>(0x00467130);             // 0x0044db27
const V2_t kLandmarkName = reinterpret_cast<V2_t>(0x004074a0);        // 0x0044df6a
const V1_t kAppendNumber = reinterpret_cast<V1_t>(0x0042dc00);        // 0x0044d48a, 0x0044d4d8
const V0_t kEnterModal = reinterpret_cast<V0_t>(0x00483d30);          // 0x0044e422
const C0_t kDrawScoreSummary = reinterpret_cast<C0_t>(0x00473470);    // 0x0044e513

static inline const char* S(unsigned a) { return reinterpret_cast<const char*>(a); }
static inline int L(const char* s) { return static_cast<int>(strlen(s)); }

// The two window globals this screen draws through: the widget object 0x00519cd4 (every `mov ecx, 0x519cd4`) and the
// window 0x00519a60. Every `mov eax, 0x519a60; neg eax; sbb eax, eax; and eax, 0x519cd4` pair (0x0044d205, 0x0044d22b,
// 0x0044dfaa, 0x0044e3dc, 0x0044e492, 0x0044e4bd, ...) tests a non-zero immediate, so the surface argument it computes
// is the constant 0x00519cd4 on every pass.
void* const kWidget = reinterpret_cast<void*>(0x00519cd4);
void* const kWindow = reinterpret_cast<void*>(0x00519a60);
void* const kFontA = reinterpret_cast<void*>(0x00821020);    // 0x0044d32a
void* const kFontB = reinterpret_cast<void*>(0x00821ee8);    // every other drawStringAt / drawStringAt2
char* const kNumBuf2 = reinterpret_cast<char*>(0x00821c50);  // this function's itoa scratch (0x0044d2af, ...)

// One statistics row: the label, the "increased / not changed / decreased" verdict, last year's value ending at
// x = 0x259 and this year's ending at x = 0x1f4 in a verdict colour. Inlined four times (cash 0x0044d3cb, fun
// 0x0044d565, skill 0x0044d75c, membership 0x0044d8ed); `kind` is the number format each uses:
//   0 appendNumber(v * 100)  (0x0044d480..0x0044d489: lea eax,[eax+eax*4] twice then shl 2)
//   1 __itoa(v, 0x00821c50, 10)  (0x0044d625)      2 appendCents(v)  (0x0044d815)
void StatRow(const short* arr, int lastIdx, int thisIdx, int y, const char* label, int kind) {
    const int last = arr[lastIdx];   // mov ax, word ptr [ebp*2 + <base>]  (0x0044d3db)
    const int now = arr[thisIdx];    // mov cx, word ptr [ebx*2 + <base>]  (0x0044d3f8)
    strcpy(kText, label);
    // 0x0044d400 / 0x0044d403 / 0x0044d411: a signed 16-bit compare of last year against this year
    strcat(kText, S(last < now ? 0x004d29bc : (last == now ? 0x004d29b0 : 0x004d29a4)));
    kDrawStringAt(kWidget, 0, kFontB, kText, 0x12d, y, L(kText));        // 0x0044d46c
    kText[0] = 0;                                                        // 0x0044d479
    if (kind == 0) kAppendNumber(last * 100);
    else if (kind == 1) strcat(kText, kItoa(last, kNumBuf2, 10));
    else kAppendCents(last);
    kDrawStringAt2(kWidget, 0, kFontB, kText, 0x259, y, L(kText));       // 0x0044d4ba
    kText[0] = 0;                                                        // 0x0044d4c7
    if (kind == 0) kAppendNumber(now * 100);
    else if (kind == 1) strcat(kText, kItoa(now, kNumBuf2, 10));
    else kAppendCents(now);
    // 0x0044d4f3 / 0x0044d4fc..0x0044d509: the 16-bit difference picks 0x80007d08 or 0x80000000
    const int colour = last < now ? 0x80001284 : (last != now ? 0x80007d08 : 0x80000000);
    kSetQuad(kWidget, 0, colour, -1, 2, 2);                              // 0x0044d51a
    kDrawStringAt2(kWidget, 0, kFontB, kText, 0x1f4, y, L(kText));       // 0x0044d54b
    kSetQuad(kWidget, 0, 0x80000000, -1, 2, 2);                          // 0x0044d560
}

}  // namespace

typedef void(__cdecl* CareerRetirementFlow_t)();
CareerRetirementFlow_t CareerRetirementFlow_orig;

void __cdecl CareerRetirementFlow_re() {
    // The frame is 0x3e0 bytes (0x0044d005): six 0x2c-byte Surfaces at +0x30, +0x5c, +0x88, +0xb4, +0xe0 and +0x10c,
    // the Image at +0x138. The EH state bytes ([esp+0x3f8], [esp+0x410]) are not reproduced.
    alignas(16) unsigned char imgBuf[0x2c0];
    alignas(4) unsigned char topBuf[0x2c], topABuf[0x2c], botBuf[0x2c], botABuf[0x2c], lineBuf[0x2c], lineABuf[0x2c];
    void* const image = imgBuf;
    void* const top = topBuf;        // frame +0x88: the 0xb0-tall capture of the first image
    void* const topA = topABuf;      // frame +0xe0: the same region of the second image
    void* const bottom = botBuf;     // frame +0xb4
    void* const bottomA = botABuf;   // frame +0x10c
    void* const line = lineBuf;      // frame +0x30: the 0xf-tall line strip
    void* const lineA = lineABuf;    // frame +0x5c

    kImageCtor(image, 0);            // 0x0044d016
    kSurfaceCtor(top, 0);            // 0x0044d02d
    kSurfaceCtor(topA, 0);           // 0x0044d041
    kSurfaceCtor(bottom, 0);         // 0x0044d055
    kSurfaceCtor(bottomA, 0);        // 0x0044d069
    kSurfaceCtor(line, 0);           // 0x0044d07a
    kSurfaceCtor(lineA, 0);          // 0x0044d08b

    kSurfaceCreateFull(image, 0, 0x320, 0x258, 0, 1, 0, 0);                        // 0x0044d0b1
    kLoadImage(image, 0, S(0x004d2a3c), 0, 0, 0x100, 1);                           // 0x0044d0cd
    const int img = reinterpret_cast<int>(image);
    kBlitTakeDirty(top, 0, img, 0xbb, 0x28, 0x1ad, 0xb0, 1, 0);                    // 0x0044d0f6
    kBlitTakeDirty(bottom, 0, img, 0xbb, 0x176, 0x1ad, 0x37, 1, 0);                // 0x0044d11f
    kBlitTakeDirty(line, 0, img, 0xbb, 0x123, 0x1ad, 0xf, 1, 0);                   // 0x0044d145
    kLoadImage(image, 0, S(0x004d2a10), 0, 0, 0x100, 1);                           // 0x0044d161
    kBlitTakeDirty(topA, 0, img, 0xbb, 0x28, 0x1ad, 0xb0, 1, 0);                   // 0x0044d18a
    kBlitTakeDirty(bottomA, 0, img, 0xbb, 0x176, 0x1ad, 0x37, 1, 0);               // 0x0044d1b3
    kBlitTakeDirty(lineA, 0, img, 0xbb, 0x123, 0x1ad, 0xf, 1, 0);                  // 0x0044d1d9
    kImageReleaseAll(image, 0);                                                    // 0x0044d1e5
    kPlaySound(0x7f, 100, 0, 0, 0);                                                // 0x0044d1f4

    const int buttonQuad = G<int>(0x00519d40);                                     // 0x0044d1f9, kept in the frame
    kDrawSprite(reinterpret_cast<void*>(0x00821040), 0, 0x00519cd4, 0, 0, 0x3e800000, 0, 1);  // 0x0044d226
    kStretchBlit(top, 0, topA, 0x00519cd4, 0xbb, 0x28, 0);                               // 0x0044d252

    // The month index: the game date 0x00834170 divided by 1024 (0x0044d261..0x0044d26c), then this year's slot
    // month % 500 (0x0044d272) and last year's (month - 8) % 500 (0x0044d282): a year is eight of these steps.
    const int date = G<int>(0x00834170);
    const int month = date / 1024;
    const int thisIdx = month % 500;
    const int lastIdx = (month - 8) % 500;

    kSetQuad(kWidget, 0, 0x80000000, -1, 2, 2);                                    // 0x0044d298

    // "END of  YEAR:  " plus the year word 0x005a6d3c + 2000 (0x0044d29d..0x0044d2d6)
    strcpy(kText, S(0x004d2a00));
    strcat(kText, kItoa(G<short>(0x005a6d3c) + 0x7d0, kNumBuf2, 10));
    kDrawStringAt(kWidget, 0, kFontA, kText, 0x196, 0x37, L(kText));               // 0x0044d334
    kDrawStringAt(kWidget, 0, kFontB, S(0x004d29f4), 0x1d1, 0x5d, L(S(0x004d29f4)));  // 0x0044d365
    kDrawStringAt(kWidget, 0, kFontB, S(0x004d29e8), 0x236, 0x5d, L(S(0x004d29e8)));  // 0x0044d392
    kDrawStringAt(kWidget, 0, kFontB, S(0x004d29dc), 0x194, 0xc8, L(S(0x004d29dc)));  // 0x0044d3c6

    StatRow(reinterpret_cast<const short*>(0x0051b388), lastIdx, thisIdx, 0x73, S(0x004d29c8), 0);  // 0x0044d3cb
    StatRow(reinterpret_cast<const short*>(0x0053e63c), lastIdx, thisIdx, 0x87, S(0x004d298c), 1);  // 0x0044d565
    StatRow(reinterpret_cast<const short*>(0x005409ac), lastIdx, thisIdx, 0x9b, S(0x004d2974), 2);  // 0x0044d75c
    // The membership row first copies the live count 0x0058b388 into this year's slot (0x0044d904 / 0x0044d913).
    reinterpret_cast<short*>(0x00520640)[thisIdx] = G<short>(0x0058b388);
    StatRow(reinterpret_cast<const short*>(0x00520640), lastIdx, thisIdx, 0xaf, S(0x004d295c), 1);  // 0x0044d8ed

    // The highlights list: one line per month from last year's slot to this year's, from the word table 0x00568600.
    int y = *reinterpret_cast<const int*>(static_cast<char*>(top) + 0x1c) + 0x28;     // 0x0044dad0 / 0x0044dadd
    const int step = *reinterpret_cast<const int*>(static_cast<char*>(line) + 0x1c);  // [esp+0x4c], 0x0044e002
    for (int m = lastIdx; m <= thisIdx; ++m) {                                   // 0x0044daf7 / 0x0044e016
        const short ev = reinterpret_cast<const short*>(0x00568600)[m];            // 0x0044daff / 0x0044db0e
        if (ev == 0) continue;                                                     // 0x0044db12
        // the month name of this step (clamp(m - lastIdx, 0, 7) at 0x0044db27, pointer table 0x004c2908) and ": "
        strcpy(kText, G<const char*>(0x004c2908 + 4 * kClamp(m - lastIdx, 0, 7)));
        strcat(kText, S(0x004d2958));
        // 0x0044db7c..0x0044db8a: the event word sign-extended; the kind is its low byte masked with 0xe0, with the
        // bytes above it kept (`and al, 0xe0` touches AL only, so 0x120 stays 0x120); the subject is its low 5 bits.
        const int kind = (static_cast<int>(ev) & ~0xff) | (ev & 0xe0);
        const int n = ev & 0x1f;
        const char* tail = 0;   // the string the common tail 0x0044df79 appends
        if (kind > 0xc0) {                                                         // 0x0044db94
            switch (kind) {                                                        // tables 0x0044e6e4 / 0x0044e6cc
                case 0xe0:                                                         // 0x0044de2e
                    strcat(kText, S(0x004d6098));
                    strcat(kText, S(0x004d28f8));
                    strcat(kText, kItoa(n, kNumBuf2, 10));
                    tail = S(0x004d28e8);
                    break;
                case 0x100:                                                        // 0x0044deca
                    strcat(kText, G<const char*>(0x004c2c18));
                    tail = S(0x004d28d4);
                    break;
                case 0x120:                                                        // 0x0044deff
                    strcat(kText, S(0x004d28c4));
                    kLoadStoryStrings(n, -1, 0, 0);                                // 0x0044df31
                    break;
                case 0x140:                                                        // 0x0044df74
                    tail = S(0x004d2894);
                    break;
                case 0x160:                                                        // 0x0044df3b
                    strcat(kText, S(0x004d28b0));
                    kLandmarkName(n, 0);                                           // 0x0044df6a
                    break;
                default:                                                           // 0x0044de19
                    break;
            }
        } else if (kind == 0xc0) {                                                 // 0x0044db9a / 0x0044ddd2
            strcat(kText, S(0x0055d738 + 0x25 * n));
            tail = S(0x004d2904);
        } else {
            switch (kind) {                                                        // tables 0x0044e648 / 0x0044e630
                case 0x20:                                                         // 0x0044dbbd
                    strcat(kText, S(0x004c4978));
                    strcat(kText, kItoa(n, kNumBuf2, 10));
                    strcat(kText, S(0x004d294c));
                    // the hole record 0x00575ab0 + 0x208 * n: a zero first byte drops the whole line
                    if (G<char>(0x00575ab0 + 0x208 * n) == 0) {                    // 0x0044dc56 / 0x0044dc5f
                        kText[0] = 0;                                              // 0x0044dc65
                        continue;                                                  // 0x0044dc6c
                    }
                    break;
                case 0x40:                                                         // 0x0044dc71
                    strcat(kText, S(0x004c26b0 + 0x14 * n));
                    tail = S(0x004d2944);
                    break;
                case 0x60:                                                         // 0x0044dd90
                    strcat(kText, S(0x004d2914));
                    tail = S(0x0058dd50 + 0x38 * n);
                    break;
                case 0x80:                                                         // 0x0044dcae
                    strcat(kText, S(0x004c4978));
                    strcat(kText, kItoa(n, kNumBuf2, 10));
                    tail = S(0x004d2934);
                    break;
                case 0xa0:                                                         // 0x0044dd1f
                    strcat(kText, S(0x004c4978));
                    strcat(kText, kItoa(n, kNumBuf2, 10));
                    tail = S(0x004d2924);
                    break;
                default:                                                           // 0x0044dba8
                    break;
            }
        }
        if (tail) strcat(kText, tail);                                             // 0x0044df79
        if (kText[0] == 0) continue;                                               // 0x0044dfa1 / 0x0044dfa8
        kStretchBlit(line, 0, lineA, 0x00519cd4, 0xbb, y, 0);                             // 0x0044dfca
        kDrawStringAt(kWidget, 0, kFontB, kText, 0x194, y + 2, L(kText));          // 0x0044dffd
        y += step;                                                                 // 0x0044e002
    }

    // The board's debt warnings: only while the money 0x00571fd4 is negative and bit 0x01000000 of the flag word
    // 0x0059e7b8 is clear (0x0044e026..0x0044e03d). The counter 0x005a6374 picks the wording and is raised.
    if (G<int>(0x00571fd4) < 0 && (G<int>(0x0059e7b8) & 0x01000000) == 0) {
        kSetQuad(kWidget, 0, 0x80007d08, -1, 2, 2);                                // 0x0044e053
        const int warn = G<int>(0x005a6374);                                       // 0x0044e058
        G<int>(0x005a6374) = warn + 1;                                             // 0x0044e064
        // Each arm draws two lines through the line strip (0x0044e28c / 0x0044e30e, 0x0044e187 / 0x0044e209,
        // 0x0044e07e / 0x0044e100); a counter above 2 draws none (0x0044e078).
        static const unsigned kWarn[3][2] = {{0x004d2788, 0x004d2758},
                                             {0x004d27f0, 0x004d27c4},
                                             {0x004d285c, 0x004d2824}};
        if (warn >= 0 && warn <= 2) {                                              // 0x0044e06a / 0x0044e071 / 0x0044e078
            for (int k = 0; k < 2; ++k) {
                strcpy(kText, S(kWarn[warn][k]));
                kStretchBlit(line, 0, lineA, 0x00519cd4, 0xbb, y, 0);
                kDrawStringAt(kWidget, 0, kFontB, kText, 0x194, y + 2, L(kText));
                y += step;
            }
        }
    } else {
        G<int>(0x005a6374) = 0;                                                    // 0x0044e392
    }

    kStretchBlit(bottom, 0, bottomA, 0x00519cd4, 0xbb, y, 0);                               // 0x0044e3c2
    kSetQuad(kWidget, 0, buttonQuad, -1, 2, 2);                                    // 0x0044e3d7
    y += 8;                                                                        // 0x0044e3e1
    kImageDraw(reinterpret_cast<void*>(0x00820f40), 0, kWidget, 0x226, y, 0);      // 0x0044e405
    bool running = true;                                                           // 0x0044e3fb
    bool hovered = false;                                                          // 0x0044e400
    kRefreshFull(kWindow, 0, 0);                                                   // 0x0044e411
    kWaitTicks(0x3c, 0);                                                           // 0x0044e41a
    kEnterModal();                                                                 // 0x0044e422
    for (;;) {
        kWaitTicks(1, 0);                                                          // 0x0044e42f
        kPumpMessages();                                                           // 0x0044e437
        int px = 0, py = 0;
        kViewToLocal(kWindow, 0, &px, &py);                                        // 0x0044e44b
        kRefreshFull(kWindow, 0, 0);                                               // 0x0044e457
        if (G<int>(0x00822d68) == 1) running = false;                              // 0x0044e45c / 0x0044e464
        // the button rect: x in [0x226, 0x252), y in [button, button + 0x2c)  (0x0044e46d..0x0044e488)
        const bool inside = px >= 0x226 && px < 0x252 && py >= y && py < y + 0x2c;
        if (inside) {
            if (!hovered) {                                                        // 0x0044e48e / 0x0044e490
                hovered = true;                                                    // 0x0044e4a8
                kImageDraw(reinterpret_cast<void*>(0x00820f6c), 0, kWidget, 0x226, y, 0);  // 0x0044e4de
                kRefreshFull(kWindow, 0, 0);                                       // 0x0044e4ea
            }
        } else if (hovered) {                                                      // 0x0044e4b9 / 0x0044e4bb
            hovered = false;                                                       // 0x0044e4d3
            kImageDraw(reinterpret_cast<void*>(0x00820f40), 0, kWidget, 0x226, y, 0);
            kRefreshFull(kWindow, 0, 0);
        }
        if (kFlushRedraw() != 0) break;                                            // 0x0044e4ef / 0x0044e4f6
        if (!running) break;                                                       // 0x0044e4fc / 0x0044e4fe
    }
    kPumpLight();                                                                  // 0x0044e504
    if (kAddScore() != -1) kDrawScoreSummary();                                    // 0x0044e509..0x0044e513
    kStartSoundSlot(0x7f);                                                         // 0x0044e51a

    kSurfaceFree(top, 0);                                                          // 0x0044e529
    kSurfaceFree(bottom, 0);                                                       // 0x0044e535
    kSurfaceFree(line, 0);                                                         // 0x0044e53e
    kSurfaceFree(topA, 0);                                                         // 0x0044e54a
    kSurfaceFree(bottomA, 0);                                                      // 0x0044e556
    kSurfaceFree(lineA, 0);                                                        // 0x0044e55f
    // the six destructors of the unwind chain: the base vtable 0x004ba2d8 into +0, then the same body again
    void* const order[6] = {lineA, line, bottomA, bottom, topA, top};              // 0x0044e569..0x0044e5fa
    for (int k = 0; k < 6; ++k) {
        *reinterpret_cast<unsigned*>(order[k]) = 0x004ba2d8;
        kSurfaceFree(order[k], 0);
    }
    kImageDtor(image, 0);                                                          // 0x0044e611
}

// 0x0044cff0  CareerRetirementFlow
SG_HOOK("golf_clean.exe", 0x0044cff0, CareerRetirementFlow, CareerRetirementFlow_re, CareerRetirementFlow_orig);
