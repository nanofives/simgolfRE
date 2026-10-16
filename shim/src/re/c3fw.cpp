// C3 batch c3fw (2026-10-15) of golf_clean.exe: loadGameBrowserScreen 0x0043b610, the saved-game / championship-course
// browser, showThemePackSelect 0x004725b0 (theme packs), pickAProScreen 0x0043a8c0 (pro golfers) and courseActionMenu
// 0x0046f550 (the world map): the SEH + vector-constructor screens.
// Fixtures: re/frida/js/fixtures.d/c3fw.js. Registry: re/frida/registry.d/c3fw.py. Purposes and branch coverage:
// log/c3/c3fw_purpose.md. Notes: log/c3/c3fw_notes.txt. Original callees, slots and literal globals the body uses (the
// port's data model): log/c3/c3fw_deps.tsv. Listings: log/c3/alt/c3fw_43b610.asm, c3fw_4725b0.asm, c3fw_43a8c0.asm,
// c3fw_46f550.asm.
//
// Hand-written from the disassembly; every statement cites the instructions it reproduces. __thiscall is emulated with
// __fastcall (ecx = this, edx unused). Callees run through their original addresses, so a hooked callee runs its own
// reimplementation.
//
// FRAME OBJECTS. The original keeps a 15-element Surface array (0x2c bytes each) built by the vector constructor iterator
// ??_L 0x004a55d2 at [esp+0x5c] and an Image at [esp+0x2f0] (esp = the body's esp after the four register pushes,
// 0x0043b636). The reimplementation keeps the same objects in its own frame at offsets of its own choosing; the A/B logs
// pointers into either frame by (object ordinal, offset) (re/frida/js/framemask.js), so it compares which object and
// which array element every call targets, not where either frame puts them. ??_L / ??_M and the Surface constructor and
// destructor run for real in both arms.
//
// The original's SEH frame (handler 0x004b7441) unwinds the Image and the array on an exception. No callee of this body
// throws a C++ exception, so the reimplementation calls the destructors on the normal paths only.
#include "hooks.h"

#ifdef C3FW_NEGCTL
// negative-control build only (shim/src/re/c3fw_negctl.cpp includes this file with C3FW_NEGCTL defined): word 0 is set
// by __freeze.negOn() after the fixture froze the first thread; word 2 selects the wrong detail (from the environment
// variable C3FW_NEG, read once).
extern "C" __declspec(dllexport) volatile int C3fwNegCtl[4] = {0, 0x5147454e, 0, 0};
extern "C" __declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableA(const char*, char*, unsigned long);
static int NegSel() {
    static int sel = -1;
    if (sel < 0) {
        char b[16] = {0};
        unsigned long n = GetEnvironmentVariableA("C3FW_NEG", b, sizeof(b));
        sel = 0;
        for (unsigned long i = 0; i < n && b[i] >= '0' && b[i] <= '9'; i++) sel = sel * 10 + (b[i] - '0');
    }
    return sel;
}
// WRONG(k, right, wrong): the wrong value only in the negative-control build, armed, with C3FW_NEG = k
#define WRONG(k, right, wrong) ((C3fwNegCtl[0] != 0 && NegSel() == (k)) ? (wrong) : (right))
#else
#define WRONG(k, right, wrong) (right)
#endif

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline void* P(unsigned a) { return reinterpret_cast<void*>(a); }
static inline char* S(unsigned a) { return reinterpret_cast<char*>(a); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int);
typedef int(__cdecl* C2_t)(int, int);
typedef int(__cdecl* C3_t)(int, int, int);
typedef int(__cdecl* C4_t)(int, int, int, int);
typedef int(__cdecl* C5_t)(int, int, int, int, int);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* Tn1_t)(void* ecx, void* edx, int);
typedef int(__fastcall* Tn2_t)(void* ecx, void* edx, int, int);
typedef int(__fastcall* Tn4_t)(void* ecx, void* edx, int, int, int, int);
typedef int(__fastcall* Tn5_t)(void* ecx, void* edx, int, int, int, int, int);
typedef int(__fastcall* Tn7_t)(void* ecx, void* edx, int, int, int, int, int, int, int);
typedef void(__stdcall* VecCtor_t)(void* p, unsigned size, int count, void* ctor, void* dtor);
typedef void(__stdcall* VecDtor_t)(void* p, unsigned size, int count, void* dtor);
typedef char*(__cdecl* Itoa_t)(int value, char* buf, int radix);

// ---- callees (original addresses) ----
const C1_t kSaveGame = reinterpret_cast<C1_t>(0x0040B4A0);             // saveGame (name)
const C1_t kLoadCourse = reinterpret_cast<C1_t>(0x0040B840);           // loadCourse (flag); path in 0x0051a068
const C4_t kLoadGame = reinterpret_cast<C4_t>(0x0040B9B0);             // loadGame (name, a, b, c)
const C5_t kBlitPanel = reinterpret_cast<C5_t>(0x0040CA10);            // blitPanel (x, y, w, h, 0)
const C1_t kAppendCourseTitle = reinterpret_cast<C1_t>(0x0040DAA0);    // appendCourseTitle
const C1_t kAppendNumber = reinterpret_cast<C1_t>(0x0042DC00);         // appendNumber
const C1_t kAppendCents = reinterpret_cast<C1_t>(0x0042DD50);          // appendCents
const C3_t kListMatchingFiles = reinterpret_cast<C3_t>(0x0043D2A0);    // listMatchingFiles (pattern, a, b) -> count
const C0_t kSetDirtyFlagB = reinterpret_cast<C0_t>(0x0045AE50);        // returns and clears the key word 0x00822b9c
const C0_t kFlushRedraw = reinterpret_cast<C0_t>(0x0045AE70);
const C1_t kAppendString = reinterpret_cast<C1_t>(0x0045B9F0);
const C2_t kWaitTicks = reinterpret_cast<C2_t>(0x0045BF80);
const C0_t kPumpMessages = reinterpret_cast<C0_t>(0x0045C030);
const C1_t kLoadThemes = reinterpret_cast<C1_t>(0x004658B0);
const C3_t kClamp = reinterpret_cast<C3_t>(0x00467130);                // clamp (v, lo, hi)
const C2_t kApproxDistance = reinterpret_cast<C2_t>(0x00467170);
const C5_t kShowPopup = reinterpret_cast<C5_t>(0x0046D6E0);            // showPopup (x, y, a, b, c); text in 0x0051a068
const Tn7_t kSurfaceBlit = reinterpret_cast<Tn7_t>(0x00473BF0);        // Surface_blit (src, x, y, w, h, 1, 1)
const Tn4_t kImageDraw = reinterpret_cast<Tn4_t>(0x00473E60);          // Image_draw (display, x, y, 0) on a Surface
const Tn5_t kStretchBlit = reinterpret_cast<Tn5_t>(0x00473F60);        // Surface_stretchBlit
const T0_t kImageCtor = reinterpret_cast<T0_t>(0x00474AE0);
const T0_t kImageDtor = reinterpret_cast<T0_t>(0x00474C40);
const Tn5_t kLoadImage = reinterpret_cast<Tn5_t>(0x00475840);          // loadImage (path, 0, 0, 0x100, 2)
const Tn7_t kBlitThunk = reinterpret_cast<Tn7_t>(0x00475C60);          // Surface_blitThunk
const Tn4_t kSetDrawTarget = reinterpret_cast<Tn4_t>(0x004762D0);
const Tn2_t kViewToLocal = reinterpret_cast<Tn2_t>(0x0047AB50);
const Tn5_t kFillWindowRect = reinterpret_cast<Tn5_t>(0x00480B00);     // fillWindowRect (x, y, w, h, colour)
const Tn1_t kRefreshFull = reinterpret_cast<Tn1_t>(0x00480C80);        // Window::refreshFull
const C0_t kEnterModal483d30 = reinterpret_cast<C0_t>(0x00483D30);
const C1_t kRemove = reinterpret_cast<C1_t>(0x004A64B8);               // CRT remove (path)
const VecCtor_t kVecCtor = reinterpret_cast<VecCtor_t>(0x004A55D2);    // ??_L eh vector constructor iterator
const VecDtor_t kVecDtor = reinterpret_cast<VecDtor_t>(0x004A5713);    // ??_M eh vector destructor iterator
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);             // _itoa
const C4_t kDrawText = reinterpret_cast<C4_t>(0x004049D0);             // drawText4049d0 (text, x, y, colour)
const C4_t kDrawCenteredText = reinterpret_cast<C4_t>(0x00404B70);     // drawCenteredText (text, x, y, colour)
const C4_t kDrawCenteredText2 = reinterpret_cast<C4_t>(0x00404BC0);    // drawCenteredText2 (text, x, y, colour)
void* const kSurfaceCtor = P(0x00473AB0);                              // ??_L's element constructor
void* const kSurfaceDtor = P(0x004041F0);                              // ??_L / ??_M's element destructor

// ---- globals ----
void* const kRoot = P(0x00519A60);                 // root window
const int kScreenSurface = 0x00519CD4;             // `0x519a60 ? 0x519cd4 : 0` (0x0043b905..0x0043b91c), a constant
char* const kBuf = S(0x0051A068);                  // shared text buffer
char* const kItoaBuf = S(0x005A9F6C);              // _itoa's output buffer here
char* const kNames = S(0x0080B130);                // file names from listMatchingFiles, 100 bytes each
char* const kThemeName = S(0x00567328);            // current theme name
const unsigned kDisplayPtr = 0x004C1570;           // the drawing target object passed to every draw call
const unsigned kMouseButton = 0x00822D68;
const unsigned kMouseX = 0x00822D60, kMouseY = 0x00822D64;

// string literals of the original (addresses only; their text is in the binary)
const unsigned kStrWhileBrowsing = 0x004C8998;     // saveGame name, listMatchingFiles exclusion
const unsigned kStrWhileBrowsingSve = 0x004C5314;  // loadGame name on the cancel path
const unsigned kStrImageMO = 0x004C8858, kStrImage = 0x004C897C;
const unsigned kStrTitleChamp = 0x004C8960, kStrPatternChamp = 0x004C8944;
const unsigned kStrTitleLoad = 0x004C8930, kStrPatternSaves = 0x004C891C;
const unsigned kStrEmpty = 0x004E9A84;
const unsigned kStrAutosave = 0x004C8910, kStrCloseParen = 0x004C59E0, kStrMore = 0x004C8904;
const unsigned kStrHoles = 0x004C88FC, kStrPar = 0x004C88F8, kStrYards = 0x004C88F0, kStrCash = 0x004C88E8;
const unsigned kStrFun = 0x004C88DC, kStrLength = 0x004C88CC, kStrAccuracy = 0x004C88BC, kStrImagination = 0x004C88B0;
const unsigned kStrHole = 0x004C88A8, kStrTheme = 0x004C889C, kStrDesignedBy = 0x004C888C, kStrDesigner = 0x004D6098;
const unsigned kStrRecord = 0x004C52D4, kStrBy = 0x004C8884;
const unsigned kStrDelete = 0x004C87EC, kStrCancel = 0x004C8590, kStrLoad = 0x004C87E4;
const unsigned kStrAskDelete = 0x004C87C0, kStrChampDir = 0x004C5328, kStrYesNo = 0x004C8794, kStrSavesDir = 0x004C8874;
const unsigned kStrLoading = 0x004C3FA8;

static void StrCpy(char* d, const char* s) { while ((*d++ = *s++) != 0) {} }
static void StrCat(char* d, const char* s) { while (*d) d++; StrCpy(d, s); }
static int StrCmp(const char* a, const char* b) {     // 0x0043ca0e..0x0043ca38: 0 equal, else -1 / 1 (unsigned bytes)
    for (;;) {
        unsigned char x = static_cast<unsigned char>(*a), y = static_cast<unsigned char>(*b);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
        a++; b++;
    }
}
static inline int Disp() { return G<int>(kDisplayPtr); }
static inline char* NameAt(int i) { return kNames + i * 100; }   // lea eax,[i+i*4]; lea eax,[eax+eax*4]; [eax*4+0x80b130]

// The reimplementation's frame objects (offsets chosen here; the frame mask makes them layout-independent)
const unsigned kSurfaceSize = 0x2C, kSurfaceCount = 0xF;

// Button table (0x0043b6aa..0x0043b6e6): left x / top y of the five buttons 0 load, 1 back, 2 scroll up, 3 scroll down,
// 4 delete. Array element k is button k's normal image, 5 + k its highlighted image, 10 / 14 the load / delete buttons'
// disabled images (no selection); 11..13 are constructed and never drawn.
const int kBtnX[5] = {0x258, 0x2E4, 0x30A, 0x30A, 0x29E};
const int kBtnY[5] = {0x208, 0x212, 0x064, 0x17C, 0x208};

// The rectangles cut out of the two loaded images (0x0043b730..0x0043b8e4): {element, x, y, w, h}
struct Cut { int elem, x, y, w, h; };
const Cut kCutsMO[7] = {
    {5, 0x258, 0x208, 0x46, 0x46},     // 0x0043b74d
    {6, 0x2E4, 0x212, 0x3C, 0x3C},     // 0x0043b773
    {7, 0x30A, 0x064, 0x16, 0x28},     // 0x0043b792
    {8, 0x30A, 0x17C, 0x16, 0x28},     // 0x0043b7b4
    {9, 0x29E, 0x208, 0x46, 0x46},     // 0x0043b7d6
    {10, 0x258, 0x1B3, 0x46, 0x46},    // 0x0043b7fc
    {14, 0x29E, 0x1B3, 0x46, 0x46},    // 0x0043b822
};
const Cut kCuts[5] = {
    {0, 0x258, 0x208, 0x46, 0x46},     // 0x0043b85b
    {1, 0x2E4, 0x212, 0x3C, 0x3C},     // 0x0043b881
    {2, 0x30A, 0x064, 0x16, 0x28},     // 0x0043b8a0
    {3, 0x30A, 0x17C, 0x16, 0x28},     // 0x0043b8c2
    {4, 0x29E, 0x208, 0x46, 0x46},     // 0x0043b8e4
};

}  // namespace

// ====================================================================================================================
// loadGameBrowserScreen 0x0043b610 (__cdecl (int resume, int championship), `ret` at 0x0043cb2f)
// ====================================================================================================================
// 0x0043b610
int __cdecl LoadGameBrowserScreen_re(int resume, int championship) {
    alignas(8) char surfaces[kSurfaceSize * kSurfaceCount];   // the original's [esp+0x5c]
    alignas(8) char image[0x2B8];                             // [esp+0x2f0]
    char themeCopy[0x100];                                    // [esp+0x5a8]
    char text[0x3E8];                                         // [esp+0x628]
    void* const img = image;
    void* surf[kSurfaceCount];
    for (unsigned k = 0; k < kSurfaceCount; k++) surf[k] = surfaces + k * kSurfaceSize;

    if (resume == 0 && championship == 0)                                            // 0x0043b634..0x0043b640
        kSaveGame(static_cast<int>(kStrWhileBrowsing));                              // 0x0043b647
    const int savedType = static_cast<signed char>(G<unsigned char>(0x005A34E0));    // 0x0043b64f / [esp+0x2c]
    const int savedFlags = G<int>(0x0059E7B8);                                       // 0x0043b656 / [esp+0x30]
    StrCpy(themeCopy, kThemeName);                                                   // 0x0043b664..0x0043b697
    kVecCtor(surfaces, kSurfaceSize, kSurfaceCount, kSurfaceCtor, kSurfaceDtor);     // 0x0043b681..0x0043b6ea
    int sel = -1;                                                                    // [esp+0x14] (0x0043b6fd)
    int scroll = 0;                                                                  // [esp+0x10] (0x0043b705)
    kImageCtor(img, 0);                                                              // 0x0043b709
    kLoadImage(img, 0, static_cast<int>(kStrImageMO), 0, 0, 0x100, 2);              // 0x0043b70e..0x0043b72b
    for (unsigned c = 0; c < 7; c++) {
        const Cut& k = kCutsMO[c];
        kSurfaceBlit(surf[k.elem], 0, reinterpret_cast<int>(img), k.x, k.y, k.w, k.h, 1, 1);
    }
    kLoadImage(img, 0, static_cast<int>(kStrImage), 0, 0, 0x100, 2);                // 0x0043b827..0x0043b83c
    for (unsigned c = 0; c < 5; c++) {
        const Cut& k = kCuts[c];
        kSurfaceBlit(surf[k.elem], 0, reinterpret_cast<int>(img), k.x, k.y, k.w, k.h, 1, 1);
    }
    G<int>(kMouseButton) = 0;                                                        // 0x0043b8e9

    int n = 0;                       // [esp+0x1c]: the file count
    int maxScroll = 0;               // [esp+0x28]
    int pick = -1;                   // ebx on the load path (0x0043c8eb)
    bool load = false;
    for (;;) {
        // ---------------------------------------------------------------- redraw (0x0043b8ef, every jump back lands here)
        kSetDrawTarget(P(Disp()), 0, 0x00519A40, 0, 0, 0);                           // 0x0043b8ef..0x0043b900
        kBlitThunk(img, 0, kScreenSurface, 0, 0, 0, 0, 0x320, 0x258);                // 0x0043b905..0x0043b92d
        if (sel == -1) {                                                             // 0x0043b932
            kImageDraw(surf[10], 0, Disp(), 0x258, 0x208, 0);                        // 0x0043b953
            kImageDraw(surf[14], 0, Disp(), 0x29E, 0x208, 0);                        // 0x0043b971
        }
        if (championship != 0) {                                                     // 0x0043b976..0x0043b98b
            kDrawCenteredText(static_cast<int>(kStrTitleChamp), 0x1F8, 0x2A, static_cast<int>(0x80000000));   // 0x0043b992
            StrCpy(kBuf, S(kStrPatternChamp));                                       // 0x0043b997..0x0043b9c9
            n = kListMatchingFiles(reinterpret_cast<int>(kBuf), static_cast<int>(kStrEmpty),
                                   static_cast<int>(kStrEmpty));                     // 0x0043b9cb
        } else {
            kDrawCenteredText(static_cast<int>(kStrTitleLoad), 0x1F8, 0x2A, static_cast<int>(0x80000000));   // 0x0043b9da
            StrCpy(kBuf, S(kStrPatternSaves));                                       // 0x0043b9df..0x0043ba1b
            n = kListMatchingFiles(reinterpret_cast<int>(kBuf), static_cast<int>(kStrEmpty),
                                   static_cast<int>(kStrWhileBrowsing));             // 0x0043ba1f
        }
        kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                           // 0x0043ba27..0x0043ba3e
        maxScroll = n - 0x10;                                                        // 0x0043ba47 / [esp+0x28]
        if (scroll > maxScroll) scroll = maxScroll;                                  // 0x0043ba4a..0x0043ba59
        if (scroll < 0) scroll = 0;                                                  // 0x0043ba5d..0x0043ba63
        int y = 0x74;                                                                // ebx (0x0043ba4c)
        if (n != 0) {                                                                // 0x0043ba67: the scroll bar
            const int pos = scroll * 0xE4 / n;                                       // 0x0043ba6b..0x0043ba7f (idiv)
            const int vis = kClamp(n - scroll, 0, 0x10);                             // 0x0043ba8f
            const int len = kClamp(vis * 0xE4 / n, 0, 0xE4 - pos);                   // 0x0043ba94..0x0043baa7
            kFillWindowRect(kRoot, 0, 0x30D, pos + 0x93, 6, len, static_cast<int>(0x80007FFF));   // 0x0043baaf..0x0043bac8
        }
        // 0x0043bacd..0x0043bc65: up to 16 names from the scroll position
        char* name = NameAt(scroll);
        for (int i = scroll; i < scroll + 0x10; i++) {
            if (i >= n) break;                                                       // 0x0043baf5
            StrCpy(kBuf, name);                                                      // 0x0043bafd..0x0043bb22
            if (name[0] == 0x26) {                                                   // 0x0043bb20: an autosave name
                StrCpy(kBuf, S(kStrAutosave));                                       // 0x0043bb2a..0x0043bb5c
                StrCpy(text, kBuf);                                                  // 0x0043bb5e..0x0043bb7d
                kLoadGame(reinterpret_cast<int>(name), 0, 1, 0);                     // 0x0043bb7f
                StrCat(text, kBuf);                                                  // 0x0043bb84..0x0043bbb6
                StrCpy(kBuf, text);                                                  // 0x0043bbb8..0x0043bbdd
                StrCat(kBuf, S(kStrCloseParen));                                     // 0x0043bbdf..0x0043bc08
            }
            if (sel == i)                                                            // 0x0043bc0a..0x0043bc14
                kFillWindowRect(kRoot, 0, 0x136, y - 1, 0x1C8, 0xF, static_cast<int>(0x80007B20));   // 0x0043bc30
            kDrawText(reinterpret_cast<int>(kBuf), 0x140, y, static_cast<int>(0x80000000));   // 0x0043bc45
            y += 0x10;                                                               // 0x0043bc55
            name += 100;                                                             // 0x0043bc5c
        }
        if (n > scroll + 0x10)                                                       // 0x0043bc6b..0x0043bc78
            kDrawText(static_cast<int>(kStrMore), 0x140, y, static_cast<int>(0x80000000));   // 0x0043bc8a

        if (sel != -1) {                                                             // 0x0043bc92: the selection's details
            const int hdr = static_cast<int>(0x80004210), val = static_cast<int>(0x80000000);
            kDrawCenteredText(static_cast<int>(kStrHoles), 0x42, 0x7C, hdr);         // 0x0043bcab
            kDrawCenteredText(static_cast<int>(kStrPar), 0x7D, 0x7C, hdr);           // 0x0043bcbe
            kDrawCenteredText(static_cast<int>(kStrYards), 0xB8, 0x7C, hdr);         // 0x0043bcd4
            kBuf[0] = 0;                                                             // 0x0043bce8
            StrCat(kBuf, kItoa(G<int>(0x005685F0) - 1, kItoaBuf, 10));               // 0x0043bcd9..0x0043bd2d
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0x42, 0x92, val);         // 0x0043bd2f
            kBuf[0] = 0;                                                             // 0x0043bd3c
            StrCat(kBuf, kItoa(G<int>(0x0059AAFC), kItoaBuf, 10));                   // 0x0043bd34..0x0043bd89
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0x7D, 0x92, val);         // 0x0043bd8b
            kBuf[0] = 0;                                                             // 0x0043bd9d
            StrCat(kBuf, kItoa(G<int>(0x0058D36C), kItoaBuf, 10));                   // 0x0043bd90..0x0043bde5
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0xB8, 0x92, val);         // 0x0043bde7
            const int lbl = static_cast<int>(0x80002108);
            kDrawCenteredText(static_cast<int>(kStrCash), 0x4B, 0xA9, lbl);          // 0x0043bdfd
            kBuf[0] = 0;                                                             // 0x0043be0a
            kAppendNumber(G<int>(0x00571FD4) * 100);                                 // 0x0043be02..0x0043be1b
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0xA1, 0xA9, val);         // 0x0043be34
            kDrawText(static_cast<int>(kStrFun), 0x30, 0xC0, lbl);                   // 0x0043be4a
            kDrawText(static_cast<int>(kStrLength), 0x30, 0xD2, lbl);                // 0x0043be60
            kDrawText(static_cast<int>(kStrAccuracy), 0x30, 0xE4, lbl);              // 0x0043be76
            kDrawText(static_cast<int>(kStrImagination), 0x30, 0xF6, lbl);           // 0x0043be8f
            kBuf[0] = 0;                                                             // 0x0043bea2
            StrCat(kBuf, kItoa(G<int>(0x0059AE78), kItoaBuf, 10));                   // 0x0043be94..0x0043beea
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0xB8, 0xC0, static_cast<int>(0x80001284));   // 0x0043beec
            const int skill = static_cast<int>(0x80000210);
            kBuf[0] = 0;                                                             // 0x0043bef6
            kAppendCents(G<int>(0x005A882C));                                        // 0x0043befe
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0xB8, 0xD2, skill);       // 0x0043bf17
            kBuf[0] = 0;                                                             // 0x0043bf25
            kAppendCents(G<int>(0x0056949C));                                        // 0x0043bf2d
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0xB8, 0xE4, skill);       // 0x0043bf46
            kBuf[0] = 0;                                                             // 0x0043bf51
            kAppendCents(G<int>(0x005A636C));                                        // 0x0043bf59
            kDrawCenteredText(reinterpret_cast<int>(kBuf), 0xB8, 0xF6, skill);       // 0x0043bf72
            kDrawCenteredText(static_cast<int>(kStrHole), 0x42, 0x10E, hdr);         // 0x0043bf88
            kDrawCenteredText(static_cast<int>(kStrPar), 0x7D, 0x10E, hdr);          // 0x0043bf9e
            kDrawCenteredText(static_cast<int>(kStrYards), 0xB8, 0x10E, hdr);        // 0x0043bfba
            // 0x0043bfbf..0x0043c102: one row per hole 1 .. [0x5685f0] - 1 (the bound re-read every pass); the hole
            // record at 0x575cbc + 0x208 * (hole - 1): par = the signed byte at -4, yards = the signed word at +0
            int row = 0x11E;                                                         // ebx (0x0043bfd4)
            const char* hole = S(0x00575CBC);                                        // [esp+0x18] (0x0043bfd9)
            for (int h = 1; h < G<int>(0x005685F0); h++) {                           // 0x0043bfc4..0x0043bfce, 0x0043c0fc
                kBuf[0] = 0;                                                         // 0x0043bfe9
                StrCat(kBuf, kItoa(h, kItoaBuf, 10));                                // 0x0043bff0..0x0043c02a
                kDrawCenteredText(reinterpret_cast<int>(kBuf), 0x42, row, val);      // 0x0043c02c
                kBuf[0] = 0;                                                         // 0x0043c03c
                StrCat(kBuf, kItoa(static_cast<signed char>(hole[WRONG(8, -4, -3)]), kItoaBuf, 10));   // 0x0043c031..0x0043c082
                kDrawCenteredText(reinterpret_cast<int>(kBuf), 0x7D, row, val);      // 0x0043c084
                kBuf[0] = 0;                                                         // 0x0043c094
                StrCat(kBuf, kItoa(*reinterpret_cast<const short*>(hole), kItoaBuf, 10));   // 0x0043c089..0x0043c0df
                kDrawCenteredText(reinterpret_cast<int>(kBuf), 0xB8, row, val);      // 0x0043c0e1
                hole += 0x208;                                                       // 0x0043c0f3
                row += 0x11;                                                         // 0x0043c0f9
            }
            const int info = static_cast<int>(0x80007FFF);
            kBuf[0] = 0;                                                             // 0x0043c10a
            kAppendCourseTitle(0);                                                   // 0x0043c108..0x0043c111
            StrCat(kBuf, S(kStrTheme));                                              // 0x0043c116..0x0043c155
            StrCat(kBuf, kThemeName);                                                // 0x0043c157..0x0043c180
            StrCat(kBuf, S(kStrCloseParen));                                         // 0x0043c182..0x0043c1ab
            kDrawText(reinterpret_cast<int>(kBuf), 0x184, 0x1BA, info);              // 0x0043c1ad
            StrCpy(kBuf, S(kStrDesignedBy));                                         // 0x0043c1b2..0x0043c1d7
            StrCat(kBuf, S(kStrDesigner));                                           // 0x0043c1d9..0x0043c211
            kDrawText(reinterpret_cast<int>(kBuf), 0x184, 0x1CA, info);              // 0x0043c218
            const int record = G<int>(0x0056A524);                                   // 0x0043c21d
            if (record != 0) {                                                       // 0x0043c226
                StrCpy(kBuf, S(kStrRecord));                                         // 0x0043c22e..0x0043c259
                StrCat(kBuf, kItoa(record, kItoaBuf, 10));                           // 0x0043c25b..0x0043c28a
                StrCat(kBuf, S(kStrBy));                                             // 0x0043c28c..0x0043c2b5
                kAppendString(0x14);                                                 // 0x0043c267 / 0x0043c2b7
                kDrawText(reinterpret_cast<int>(kBuf), 0x184, 0x1DA, info);          // 0x0043c2d0
            }
            // 0x0043c2d8..0x0043c31f: the course's picture. c = the signed byte +0 of course record [0x59bf90]
            // (0x571ff4, stride 0x2e); u = the signed byte at 0x4c1ea8 + 0x82 * c; the Surface 0x541710 + 0x2c * u is
            // drawn from 0x5791f8 + 0x2c * u at (0x54, 0x20)
            const int ci = G<int>(0x0059BF90);
            const int c = static_cast<signed char>(G<unsigned char>(0x00571FF4 + ci * 0x2E));
            const int u = static_cast<signed char>(G<unsigned char>(0x004C1EA8 + c * 0x82));
            kStretchBlit(P(0x00541710 + u * 0x2C), 0, 0x005791F8 + u * 0x2C, Disp(), 0x54, 0x20, 0);   // 0x0043c31f
        }
        kRefreshFull(kRoot, 0, 0);                                                   // 0x0043c324..0x0043c32b

        // ---------------------------------------------------------------- the input loop (0x0043c330..0x0043c5d9)
        int hot = -1;                                                                // ebp (0x0043c334)
        int dwell = 0;                                                               // ebx (0x0043c337)
        int action;                                                                  // the switch index
        for (;;) {
            kWaitTicks(2, 0);                                                        // 0x0043c339..0x0043c33d
            kPumpMessages();                                                         // 0x0043c345
            int a, b;                                                                // [esp+0x24] / [esp+0x20]
            kViewToLocal(kRoot, 0, reinterpret_cast<int>(&a), reinterpret_cast<int>(&b));   // 0x0043c34a..0x0043c359
            const int prev = hot;                                                    // esi (0x0043c373)
            hot = -1;                                                                // 0x0043c376
            if (kApproxDistance(a - 0x279, b - 0x22B) < 0x1A) hot = 0;               // 0x0043c35e..0x0043c386
            if (kApproxDistance(a - 0x301, b - 0x22C) < 0x18) hot = 1;               // 0x0043c388..0x0043c3aa
            if (kApproxDistance(a - 0x310, b - 0x78) < 0x14) hot = 2;                // 0x0043c3af..0x0043c3ce
            if (kApproxDistance(a - 0x310, b - 0x190) < 0x14) hot = 3;               // 0x0043c3d3..0x0043c3f6
            if (kApproxDistance(a - 0x2C0, b - 0x22B) < 0x17) hot = 4;               // 0x0043c3fb..0x0043c41d
            if (hot != prev) {                                                       // 0x0043c422
                if (prev != -1)                                                      // 0x0043c42a
                    kImageDraw(surf[prev], 0, Disp(), kBtnX[prev], kBtnY[prev], 0);  // 0x0043c42f..0x0043c44c
                if (hot != -1)                                                       // 0x0043c451
                    kImageDraw(surf[WRONG(1, 5 + hot, hot)], 0, Disp(), kBtnX[hot], kBtnY[hot], 0); // 0x0043c456..0x0043c477
                if (sel == -1) {                                                     // 0x0043c47c
                    kImageDraw(surf[10], 0, Disp(), 0x258, 0x208, 0);                // 0x0043c49a
                    kImageDraw(surf[14], 0, Disp(), 0x29E, 0x208, 0);                // 0x0043c4b9
                }
                kRefreshFull(kRoot, 0, 0);                                           // 0x0043c4c5
                dwell = 0;                                                           // 0x0043c4ca
            } else {
                dwell++;                                                             // 0x0043c4d1
                if (dwell > 0x1E && hot != -1) {                                     // 0x0043c4d2..0x0043c4de: the tip
                    kBuf[0] = 0;                                                     // 0x0043c4e6
                    switch (hot) {                                                   // 0x0043c4ed..0x0043c4f8
                    case 0: StrCpy(kBuf, S(sel == -1 ? kStrEmpty : kStrLoad)); break;   // 0x0043c523..0x0043c550
                    case 1: StrCpy(kBuf, S(kStrCancel)); break;                      // 0x0043c501..0x0043c550
                    case 4: StrCpy(kBuf, S(kStrDelete)); break;                      // 0x0043c4fa / 0x0043c532..0x0043c550
                    default: break;                                                  // 2, 3: no text
                    }
                    if (dwell == 0x1F)                                               // 0x0043c552
                        kBlitPanel(kBtnX[hot] + 6, kBtnY[hot] + 0xE, 0x30, 0x10, 0); // 0x0043c557..0x0043c56d
                    kDrawCenteredText2(reinterpret_cast<int>(kBuf), kBtnX[hot] + 0x1E, kBtnY[hot] + 0x10,
                                       static_cast<int>(0x80007FFF));                // 0x0043c575..0x0043c58f
                    kRefreshFull(kRoot, 0, 0);                                       // 0x0043c597..0x0043c59e
                }
            }
            // 0x0043c5a7..0x0043c5de: a button press acts on the hot button (none: the list click below); otherwise the
            // loop goes on until flushRedraw reports a key; Esc is the back button, any other key takes the list click
            // at the last pointer position
            if (G<int>(kMouseButton) != 0) {                                         // 0x0043c5a7..0x0043c5ae
                action = hot;
                if (hot == -1) action = -2;                                          // 0x0043c5db..0x0043c5de
                break;
            }
            if (kFlushRedraw() == 0) continue;                                       // 0x0043c5b0..0x0043c5b7
            if (G<int>(kMouseButton) != 0) {                                         // 0x0043c5bd..0x0043c5c4
                action = hot;
                if (hot == -1) action = -2;
                break;
            }
            if (kSetDirtyFlagB() == 0x1B) { hot = 1; action = 1; break; }            // 0x0043c5c6..0x0043c5d9
            action = -2;                                                             // 0x0043c5ce -> 0x0043c7ab
            break;
        }

        if (action != -2) {                                                          // 0x0043c5e4..0x0043c5f7
            G<int>(kMouseButton) = 0;                                                // 0x0043c5e7
            if (static_cast<unsigned>(action) > 4) continue;                         // 0x0043c5f1 (ja: redraw)
            if (action == 0) { pick = sel; load = true; break; }                     // 0x0043c8eb
            if (action == 1) { load = false; break; }                                // 0x0043caa2
            if (action == 2) { scroll = kClamp(scroll - 4, 0, maxScroll); continue; }   // 0x0043c5fe..0x0043c619
            if (action == 3) { scroll = kClamp(scroll + WRONG(5, 4, 3), 0, maxScroll); continue; }   // 0x0043c61e..0x0043c639
            // 4: delete the selection after a confirmation (0x0043c63e..0x0043c7a6)
            StrCpy(kBuf, S(kStrAskDelete));                                          // 0x0043c63e..0x0043c66a
            if (championship != 0) StrCat(kBuf, S(kStrChampDir));                    // 0x0043c66c..0x0043c699
            char* victim = NameAt(sel);                                              // 0x0043c69b..0x0043c6a8 (ebx)
            StrCat(kBuf, victim);                                                    // 0x0043c6b1..0x0043c6db
            StrCat(kBuf, S(kStrYesNo));                                              // 0x0043c6dd..0x0043c70b
            if (kShowPopup(0x190, 0x64, 1, 1, 0) != 0) continue;                     // 0x0043c6b3..0x0043c717
            StrCpy(kBuf, S(WRONG(3, championship != 0, false) ? kStrChampDir : kStrSavesDir));        // 0x0043c71d..0x0043c76f
            StrCat(kBuf, victim);                                                    // 0x0043c771..0x0043c79c
            kRemove(reinterpret_cast<int>(kBuf));                                    // 0x0043c79e
            continue;
        }

        // 0x0043c7ab..0x0043c8e6: a press (or a key) at the pointer position over the list
        if (G<int>(kMouseX) <= 0x140) continue;                                      // 0x0043c7ab..0x0043c7b5
        const int my = G<int>(kMouseY);
        if (my < 0x74) continue;                                                     // 0x0043c7bb..0x0043c7c3
        sel = (my - 0x74) / 16 + scroll;                                             // 0x0043c7c9..0x0043c7e0 (rounds to 0)
        if (sel >= scroll + 0x10) {                                                  // 0x0043c7db..0x0043c7e4
            scroll = kClamp(scroll + 4, 0, maxScroll);                               // 0x0043c7e6..0x0043c7f9
            continue;
        }
        if (sel < n) {                                                               // 0x0043c802
            if (championship != 0) {                                                 // 0x0043c80c..0x0043c815
                StrCpy(kBuf, S(kStrChampDir));                                       // 0x0043c817..0x0043c83f
                StrCat(kBuf, NameAt(sel));                                           // 0x0043c839..0x0043c871
                kLoadCourse(1);                                                      // 0x0043c821 / 0x0043c873
            } else {
                kLoadGame(reinterpret_cast<int>(NameAt(WRONG(2, sel, sel + 1))), 0, 0, 0);              // 0x0043c880..0x0043c894
            }
            continue;
        }
        sel = -1;                                                                    // 0x0043c8bb
        kImageDraw(surf[10], 0, Disp(), 0x258, 0x208, 0);                            // 0x0043c8a1..0x0043c8c3
        kImageDraw(surf[14], 0, Disp(), 0x29E, 0x208, 0);                            // 0x0043c8c8..0x0043c8e1
    }

    int result = 0;
    if (load) {
        // 0x0043c8ef..0x0043c8ff: restore the course type and flags saved at entry
        G<unsigned char>(0x005A34E0) = static_cast<unsigned char>(savedType);
        G<int>(0x0059E7B8) = savedFlags;
        if (pick >= 0 && pick < n) {                                                 // 0x0043c8fd..0x0043c90f
            kSetDrawTarget(P(Disp()), 0, 0x00519928, 0, 0, 0);                       // 0x0043c915..0x0043c926
            kDrawText(static_cast<int>(kStrLoading), 0x226, 0x1C8, static_cast<int>(0x80007B20));   // 0x0043c93f
            kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                       // 0x0043c944..0x0043c958
            kRefreshFull(kRoot, 0, 0);                                               // 0x0043c95d..0x0043c964
            if (championship != 0) {                                                 // 0x0043c969..0x0043c972
                StrCpy(kBuf, S(kStrChampDir));                                       // 0x0043c974..0x0043c99a
                StrCat(kBuf, NameAt(pick));                                          // 0x0043c99c..0x0043c9cd
                kLoadCourse(0);                                                      // 0x0043c9a1 / 0x0043c9cf
            } else {
                kLoadGame(reinterpret_cast<int>(NameAt(pick)), resume == 0 ? 1 : 0, 0, 0);   // 0x0043c9d9..0x0043c9fa
            }
            if (WRONG(4, StrCmp(kThemeName, themeCopy) != 0, false))                                  // 0x0043ca02..0x0043ca3d
                kLoadThemes(1);                                                      // 0x0043ca3f..0x0043ca41
            kEnterModal483d30();                                                     // 0x0043ca49
            kImageDtor(img, 0);                                                      // 0x0043ca5d
            kVecDtor(surfaces, kSurfaceSize, kSurfaceCount, kSurfaceDtor);           // 0x0043ca62..0x0043ca7b
            return 1;                                                                // 0x0043ca80
        }
        if (resume == 0 && championship == 0)                                        // 0x0043ca8a..0x0043caa0
            kLoadGame(static_cast<int>(kStrWhileBrowsingSve), 1, 0, 0);              // 0x0043cacb..0x0043cad6
    } else {
        // 0x0043caa2..0x0043cad6: back
        G<int>(0x0059E7B8) = WRONG(6, savedFlags, G<int>(0x0059E7B8));
        G<unsigned char>(0x005A34E0) = static_cast<unsigned char>(savedType);
        if (resume == 0 && championship == 0)
            kLoadGame(static_cast<int>(kStrWhileBrowsingSve), 1, 0, 0);
    }
    kEnterModal483d30();                                                             // 0x0043cade
    kImageDtor(img, 0);                                                              // 0x0043caf2
    kVecDtor(surfaces, kSurfaceSize, WRONG(7, kSurfaceCount, kSurfaceCount - 1), kSurfaceDtor);   // 0x0043caf7..0x0043cb10
    return result;                                                                   // 0x0043cb15
}
C2_t LoadGameBrowserScreen_orig;
// 0x0043b610
SG_HOOK("golf_clean.exe", 0x0043b610, loadGameBrowserScreen, LoadGameBrowserScreen_re, LoadGameBrowserScreen_orig);

// ====================================================================================================================
// showThemePackSelect 0x004725b0 (__cdecl, no arguments, `ret` at 0x004732ca); listing log/c3/alt/c3fw_4725b0.asm
// ====================================================================================================================
// Frame of the original (esp after the four register pushes, 0x004725ce): the column words at [esp+0x28] (five written,
// 0x004725cf..0x004725eb; the sixth, [esp+0x32], is read at 0x00472e59 for column 5 and never written), the button
// tables at [esp+0x34] (y) and [esp+0x48] (x), the probe path at [esp+0x58] (0x8c bytes), one flag byte per theme at
// [esp+0xe4], Surface arrays B (4) at [esp+0x148] and A (8) at [esp+0x1f8], the Image at [esp+0x358] and a
// WIN32_FIND_DATAA at [esp+0x610]. The reimplementation keeps the column words and the path at the same distance
// (path - 0x26 = the unwritten sixth word), so a value the A/B fixture leaves in that word before the draw loop reads
// it is seen alike by both arms; nothing else here depends on the frame layout.
namespace {

typedef int(__stdcall* VecDtorI_t)(void* p, unsigned size, int count, void* dtor);   // ??_M; eax passed through
typedef unsigned(__stdcall* FindFirst_t)(const char* path, void* data);
const VecDtorI_t kVecDtorI = reinterpret_cast<VecDtorI_t>(0x004A5713);
const C0_t kListThemes = reinterpret_cast<C0_t>(0x004724C0);
const Tn7_t kDrawScaled = reinterpret_cast<Tn7_t>(0x00473CB0);       // Surface_drawScaled (display, x, y, 1, 1, 1, 0)
static inline unsigned FindFirstFileSlot(const char* path, void* data) {   // `mov ebx, [0x4ba090]; call ebx` (0x0047260b)
    return G<FindFirst_t>(0x004BA090)(path, data);
}

char* const kThemeNames = S(0x0056E928);       // theme names, 0x32 bytes each (listThemes)
const unsigned kThemeStride = 0x32;

// the six probe suffixes and the flag each sets (0x004726b7 stores 1, the others OR: 0x00472754, 0x004727f1,
// 0x0047288e, 0x0047292b, 0x004729c8)
const unsigned kProbeSuffix[6] = {0x004E18E4, 0x004C856C, 0x004E18AC, 0x004E1868, 0x004E4140, 0x004E4130};
const unsigned char kProbeBit[6] = {1, 2, 4, 8, 0x20, 0x40};
// button tables (0x004729ff..0x00472a2f): x [esp+0x48], y [esp+0x34] of buttons 0 .. 3
const int kTpX[4] = {0x245, 0x2D6, 0x301, 0x301};
const int kTpY[4] = {0x208, 0x215, 0x051, 0x1D7};
// the rectangles cut from the two sheets (0x00472a96..0x00472c4a): {element, x, y, w, h}
struct TpCut { int elem, x, y, w, h; };
const TpCut kTpCutsA[8] = {
    {4, 0x001, 0x01, 0x57, 0x1F},   // 0x00472ab1
    {5, 0x059, 0x01, 0x57, 0x1F},   // 0x00472ad1
    {6, 0x0B1, 0x01, 0x57, 0x1F},   // 0x00472af4
    {7, 0x001, 0x21, 0xDD, 0x1F},   // 0x00472b17
    {0, 0x109, 0x01, 0x47, 0x50},   // 0x00472b3a
    {1, 0x151, 0x01, 0x30, 0x31},   // 0x00472b5d
    {2, 0x182, 0x01, 0x13, 0x23},   // 0x00472b80
    {3, 0x196, 0x01, 0x13, 0x23},   // 0x00472ba3
};
const TpCut kTpCutsB[4] = {
    {0, 0x245, 0x208, 0x47, 0x50},  // 0x00472be3
    {1, 0x2D6, 0x215, 0x30, 0x31},  // 0x00472c09
    {2, 0x301, 0x051, 0x13, 0x23},  // 0x00472c28
    {3, 0x301, 0x1D7, 0x13, 0x23},  // 0x00472c4a
};

// the column words and the probe path at the original's relative layout ([esp+0x28] / [esp+0x32] / [esp+0x58])
struct TpCols {
    short colX[5];             // +0x00 (written)
    volatile short col5;       // +0x0a (never written: read for column 5)
    char gap[0x24];            // +0x0c
    char path[0x8C];           // +0x30
};

}  // namespace

// 0x004725b0
int __cdecl ShowThemePackSelect_re() {
    TpCols f;
    f.colX[0] = 0x15A; f.colX[1] = 0x1B4; f.colX[2] = 0x20E; f.colX[3] = 0x268; f.colX[4] = 0x2C2;   // 0x004725cf..0x004725eb
    alignas(8) char findData[0x140];                         // [esp+0x610]
    unsigned char flags[0x64];                               // [esp+0xe4]
    alignas(8) char arrB[4 * 0x2C];                          // [esp+0x148]
    alignas(8) char arrA[8 * 0x2C];                          // [esp+0x1f8]
    alignas(8) char image[0x2B8];                            // [esp+0x358]
    void* const img = image;

    const int count = kListThemes();                                                 // 0x004725f2 / [esp+0x18]
    // 0x00472605..0x004729de: six FindFirstFileA probes per theme (<0x004c84e8><name><suffix>), one flag bit each; the
    // handles are never closed
    for (int i = 0; i < count; i++) {
        const char* nm = kThemeNames + kThemeStride * i;                             // ebp (0x00472611, += 0x32)
        flags[i] = 0;                                                                // 0x00472626
        for (int s = 0; s < 6; s++) {
            StrCpy(f.path, S(0x004C84E8));
            StrCat(f.path, nm);
            StrCat(f.path, S(kProbeSuffix[s]));
            if (FindFirstFileSlot(f.path, findData) != 0xFFFFFFFFu) {
                if (s == 0) flags[i] = 1;                                            // 0x004726b7: a store, not an OR
                else flags[i] |= kProbeBit[s];
            }
        }
    }

    kVecCtor(arrA, 0x2C, 8, kSurfaceCtor, kSurfaceDtor);                             // 0x004729e4..0x00472a37
    kVecCtor(arrB, 0x2C, 4, kSurfaceCtor, kSurfaceDtor);                             // 0x00472a3c..0x00472a5b
    kImageCtor(img, 0);                                                              // 0x00472a6f
    kLoadImage(img, 0, 0x004E4110, 0, 0, 0x100, 2);                                  // 0x00472a74..0x00472a91
    for (unsigned c = 0; c < 8; c++) {
        const TpCut& k = kTpCutsA[c];
        kSurfaceBlit(arrA + 0x2C * k.elem, 0, reinterpret_cast<int>(img), k.x, k.y, k.w, k.h, 1, 1);
    }
    kLoadImage(img, 0, 0x004E40F4, 0, 0, 0x100, 2);                                  // 0x00472ba8..0x00472bbd
    for (unsigned c = 0; c < 4; c++) {
        const TpCut& k = kTpCutsB[c];
        kSurfaceBlit(arrB + 0x2C * k.elem, 0, reinterpret_cast<int>(img), k.x, k.y, k.w, k.h, 1, 1);
    }
    kEnterModal483d30();                                                             // 0x00472c4f
    G<int>(kMouseButton) = 0;                                                        // 0x00472c54

    int selected = -1;                       // [esp+0x24]: the current theme's name pointer, -1 none
    int exitKind = 3;                        // 1: 0x00473197, 2: 0x0047320f, 3: 0x0047325f
    for (;;) {
        // ---------------------------------------------------------------- redraw (0x00472c5a)
        kSetDrawTarget(P(Disp()), 0, 0x0051A028, 0, 0, 0);                           // 0x00472c5a..0x00472c6b
        kBlitThunk(img, 0, kScreenSurface, 0, 0, 0, 0, 0x320, 0x258);                // 0x00472c70..0x00472c98
        kDrawText(0x004E40E0, 0x4E, 0x22, static_cast<int>(0x80000000));             // 0x00472cab
        kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                           // 0x00472cc4
        kDrawCenteredText(0x004E40D8, 0x15A, 0x3E, static_cast<int>(0x80000000));    // 0x00472cda
        kDrawCenteredText(0x004E40CC, 0x1B4, 0x25, static_cast<int>(0x80000000));    // 0x00472cf0
        kDrawCenteredText(0x004E40C0, 0x20E, 0x3E, static_cast<int>(0x80000000));    // 0x00472d06
        kDrawCenteredText(0x004E40B4, 0x268, 0x25, static_cast<int>(0x80000000));    // 0x00472d1c
        kDrawCenteredText(0x004E40AC, 0x2C2, 0x3E, static_cast<int>(0x80000000));    // 0x00472d35
        kSetDrawTarget(P(Disp()), 0, 0x0051A028, 0, 0, 0);                           // 0x00472d4e
        selected = -1;                                                               // 0x00472d57
        int rowY = 0x5C;                                                             // ebp (0x00472d6f)
        for (int i = 0; i < count; i++) {                                            // 0x00472d5f / 0x00472ed4
            const char* nm = kThemeNames + kThemeStride * i;                         // [esp+0x20]
            StrCpy(kBuf, nm);                                                        // 0x00472d7c..0x00472da3
            const bool cur = StrCmp(kBuf, kThemeName) == 0;                          // 0x00472da5..0x00472dd9
            if (cur) {
                kImageDraw(arrA + 0x2C * WRONG(13, 7, 6), 0, Disp(), 0x48, rowY, 0); // 0x00472ddb..0x00472dec
                kDrawText(reinterpret_cast<int>(kBuf), 0x49, rowY + 0xB, static_cast<int>(0x80000000));   // 0x00472e01
                selected = reinterpret_cast<int>(nm);                                // 0x00472e12
            } else {
                kDrawText(reinterpret_cast<int>(kBuf), 0x49, rowY + 0xB, static_cast<int>(0x80004210));   // 0x00472e28
            }
            const int fl = static_cast<signed char>(flags[i]);                       // 0x00472e3c (movsx)
            for (int b = 0; b < 6; b++) {                                            // 0x00472e36..0x00472eb5
                if (fl & (1 << b)) {                                                 // 0x00472e48..0x00472e57
                    const int col = b < 5 ? f.colX[b] : f.col5;                      // movsx [esi] (0x00472e59)
                    kDrawScaled(arrA + 0x2C * (cur ? 5 : 6), 0, Disp(), col - 0x2B, rowY, 1, 1, 1, 0);   // 0x00472e59..0x00472ea9
                } else if (cur) {                                                    // 0x00472e88
                    const int col = b < 5 ? f.colX[b] : f.col5;                      // 0x00472e8c
                    kDrawScaled(arrA + 0x2C * 4, 0, Disp(), col - 0x2B, rowY, 1, 1, 1, 0);   // 0x00472e8c..0x00472ea9
                }
            }
            rowY += 0x20;                                                            // 0x00472ec3
        }
        kRefreshFull(kRoot, 0, 0);                                                   // 0x00472eda..0x00472ee1

        // ---------------------------------------------------------------- input loop (0x00472ee6..0x00473136)
        int hot = -1;                                                                // ebp (0x00472ee6)
        int dwell = 0;                                                               // ebx (0x00472ee9)
        bool redraw = false;
        for (;;) {
            kWaitTicks(2, 0);                                                        // 0x00472eeb..0x00472eef
            kPumpMessages();                                                         // 0x00472ef7
            int a, b;                                                                // [esp+0x14] / [esp+0x1c]
            kViewToLocal(kRoot, 0, reinterpret_cast<int>(&a), reinterpret_cast<int>(&b));   // 0x00472efc..0x00472f0b
            const int prev = hot;                                                    // esi (0x00472f24)
            hot = -1;                                                                // 0x00472f27
            if (kApproxDistance(a - 0x267, b - 0x22B) < 0x1A) hot = 0;               // 0x00472f10..0x00472f37
            if (kApproxDistance(a - 0x2ED, b - 0x22C) < 0x18) hot = 1;               // 0x00472f39..0x00472f5b
            if (kApproxDistance(a - 0x30A, b - 0x63) < 0x14) hot = 2;                // 0x00472f60..0x00472f80
            if (kApproxDistance(a - 0x30A, b - 0x1E7) < 0x14) hot = 3;               // 0x00472f85..0x00472fa7
            if (hot != prev) {                                                       // 0x00472fac
                if (prev != -1)                                                      // 0x00472fb0
                    kImageDraw(arrB + 0x2C * prev, 0, Disp(), kTpX[prev], kTpY[prev], 0);   // 0x00472fb5..0x00472fd5
                if (hot != -1)                                                       // 0x00472fda
                    kImageDraw(WRONG(11, arrA, arrB) + 0x2C * hot, 0, Disp(), kTpX[hot], kTpY[hot], 0);   // 0x00472fdf..0x00473000
                kRefreshFull(kRoot, 0, 0);                                           // 0x00473005..0x0047300c
                dwell = 0;                                                           // 0x00473011
            } else {
                dwell++;                                                             // 0x00473018
                if (dwell > 0x1E && hot != -1) {                                     // 0x00473019..0x00473025: the tip
                    kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);               // 0x0047302b..0x0047303c
                    kBuf[0] = 0;                                                     // 0x00473043
                    if (hot == 0)                                                    // 0x0047304d -> 0x00473074
                        StrCpy(kBuf, S(selected == -1 ? kStrEmpty : kStrLoad));
                    else if (hot == 1)                                               // 0x0047304f..0x00473050
                        StrCpy(kBuf, S(kStrCancel));                                 // 0x00473052..0x004730a5
                    if (dwell == 0x1F)                                               // 0x004730a7
                        kBlitPanel(kTpX[hot], kTpY[hot] + 0xE, 0x30, 0x10, 0);       // 0x004730ac..0x004730bf
                    kDrawCenteredText2(reinterpret_cast<int>(kBuf), kTpX[hot] + 0x18, kTpY[hot] + 0x10,
                                       static_cast<int>(0x80007FFF));                // 0x004730c7..0x004730e1
                    kRefreshFull(kRoot, 0, 0);                                       // 0x004730e9..0x004730f0
                }
            }
            // 0x004730f5..0x00473136
            int btn = G<int>(kMouseButton);
            if (btn == 0) {
                if (kFlushRedraw() == 0) continue;                                   // 0x004730fe..0x00473105
                btn = G<int>(kMouseButton);                                          // 0x0047310b
                if (btn == 0) hot = 1;                                               // 0x00473114: a key is back
            }
            if (hot == -1) {                                                         // 0x0047311b..0x0047311e
                if (btn == 0) { exitKind = 3; break; }                               // 0x0047313b (btn is never 0 here)
                // 0x00473143..0x00473192: a press on a theme row: idx = (y - 0x5b) / 32, rounded toward zero
                const int idx = (G<int>(kMouseY) - 0x5B) / WRONG(12, 32, 31);
                if (idx < 0 || idx >= count) { exitKind = 3; break; }                // 0x00473155 / 0x0047315f
                StrCpy(kThemeName, kThemeNames + kThemeStride * idx);                // 0x00473165..0x00473190
                redraw = true;
                break;
            }
            G<int>(kMouseButton) = 0;                                                // 0x00473123
            if (hot == 0) { exitKind = 2; break; }                                   // 0x0047312d -> 0x0047320f
            if (hot == 1) { exitKind = 1; break; }                                   // 0x00473134 -> 0x00473197
            redraw = true;                                                           // 0x00473136: 2, 3 redraw
            break;
        }
        if (!redraw) break;
    }
    if (exitKind == 1 && !WRONG(14, false, true))
        StrCpy(kThemeName, S(0x004C6C10));                                           // 0x00473197..0x004731c2
    kImageDtor(img, 0);                                                              // 0x004731cb / 0x0047321e / 0x0047326e
    kVecDtorI(arrB, 0x2C, 4, kSurfaceDtor);                                          // 0x004731e9 / 0x0047323c / 0x0047328c
    return kVecDtorI(arrA, 0x2C, 8, kSurfaceDtor);                                   // 0x004732ad: its eax is returned
}
C0_t ShowThemePackSelect_orig;
// 0x004725b0
SG_HOOK("golf_clean.exe", 0x004725b0, showThemePackSelect, ShowThemePackSelect_re, ShowThemePackSelect_orig);

// ====================================================================================================================
// pickAProScreen 0x0043a8c0 (__cdecl, no arguments, `ret` at 0x0043b5ed); listing log/c3/alt/c3fw_43a8c0.asm
// ====================================================================================================================
// The same frame and buttons as loadGameBrowserScreen: a 15-element Surface array at [esp+0x54] (??_L 0x0043a98a), an
// Image at [esp+0x2e8], the x / y tables at [esp+0x2c] / [esp+0x40], a copy of the theme name at [esp+0x5a0]; selection
// [esp+0x14], scroll [esp+0x10], count [esp+0x24], scroll limit [esp+0x28].
namespace {

const C4_t kDrawTextSurface = reinterpret_cast<C4_t>(0x00404AD0);        // drawTextSurface (text, x, y, colour)
const C4_t kBuildGolferComment = reinterpret_cast<C4_t>(0x00469B00);     // buildGolferComment (0x3e, 0, 0, 0)
const C3_t kLoadThemeFile = reinterpret_cast<C3_t>(0x00437FA0);          // loadThemeFile (name, 0, 1)

const unsigned kStrStdTheme = 0x004C3C5C;      // the theme name set at entry
const unsigned kStrPickTitle = 0x004C8830, kStrPickPattern = 0x004C8814, kStrPickSheet = 0x004C883C;
const unsigned kStrSkillsSuffix = 0x004C8808, kStrQuote = 0x004C4E54, kStrSignature = 0x004C87F4;
const unsigned kStrPlus = 0x004C5340, kStrPercent = 0x004C60C4;
const unsigned kSkillNames = 0x004C2C3C;       // 10 string pointers (0x0043aeb6 .. 0x004c2c64)
const unsigned kProSkills = 0x005A5A04;        // signed skill bytes, one per row (0x0043af19)

}  // namespace

// 0x0043a8c0
int __cdecl PickAProScreen_re() {
    alignas(8) char surfaces[kSurfaceSize * kSurfaceCount];   // [esp+0x54]
    alignas(8) char image[0x2B8];                             // [esp+0x2e8]
    char themeCopy[0x100];                                    // [esp+0x5a0]
    void* const img = image;
    void* surf[kSurfaceCount];
    for (unsigned k = 0; k < kSurfaceCount; k++) surf[k] = surfaces + k * kSurfaceSize;

    StrCpy(themeCopy, kThemeName);                                                   // 0x0043a8df..0x0043a914
    StrCpy(kThemeName, S(kStrStdTheme));                                             // 0x0043a916..0x0043a944
    kVecCtor(surfaces, kSurfaceSize, kSurfaceCount, kSurfaceCtor, kSurfaceDtor);     // 0x0043a8f6..0x0043a98a
    int sel = -1;                                                                    // [esp+0x14] (0x0043a99f)
    int scroll = 0;                                                                  // [esp+0x10] (0x0043a9a7)
    kImageCtor(img, 0);                                                              // 0x0043a9ab
    kLoadImage(img, 0, static_cast<int>(kStrImageMO), 0, 0, 0x100, 2);              // 0x0043a9b0..0x0043a9cd
    for (unsigned c = 0; c < 7; c++) {                                               // 0x0043a9ef..0x0043aac4
        const Cut& k = kCutsMO[c];
        kSurfaceBlit(surf[k.elem], 0, reinterpret_cast<int>(img), k.x, k.y, k.w, k.h, 1, 1);
    }
    kLoadImage(img, 0, static_cast<int>(kStrPickSheet), 0, 0, 0x100, 2);            // 0x0043aac9..0x0043aade
    for (unsigned c = 0; c < 5; c++) {                                               // 0x0043aafd..0x0043ab86
        const Cut& k = kCuts[c];
        kSurfaceBlit(surf[k.elem], 0, reinterpret_cast<int>(img), k.x, k.y, k.w, k.h, 1, 1);
    }
    G<int>(kMouseButton) = 0;                                                        // 0x0043ab8b

    int n = 0, maxScroll = 0;
    int result = 0;
    for (;;) {
        // ---------------------------------------------------------------- redraw (0x0043ab91)
        kSetDrawTarget(P(Disp()), 0, 0x00519A40, 0, 0, 0);                           // 0x0043ab91..0x0043aba2
        kBlitThunk(img, 0, kScreenSurface, 0, 0, 0, 0, 0x320, 0x258);                // 0x0043aba7..0x0043abcf
        if (sel == -1) {                                                             // 0x0043abd4
            kImageDraw(surf[10], 0, Disp(), 0x258, 0x208, 0);                        // 0x0043abf5
            kImageDraw(surf[14], 0, Disp(), 0x29E, 0x208, 0);                        // 0x0043ac14
        }
        kDrawCenteredText(static_cast<int>(kStrPickTitle), 0x1F8, 0x2A, static_cast<int>(0x80000000));   // 0x0043ac2a
        StrCpy(kBuf, S(kStrPickPattern));                                            // 0x0043ac2f..0x0043ac61
        n = kListMatchingFiles(reinterpret_cast<int>(kBuf), static_cast<int>(kStrEmpty),
                               static_cast<int>(kStrEmpty));                         // 0x0043ac63
        kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                           // 0x0043ac68..0x0043ac82
        maxScroll = n - 0x10;                                                        // 0x0043ac8b / [esp+0x28]
        if (scroll > maxScroll) scroll = maxScroll;                                  // 0x0043ac8e..0x0043ac9b
        if (scroll < 0) scroll = 0;                                                  // 0x0043ac9f..0x0043aca7
        int y = 0x74;                                                                // ebp (0x0043ac90)
        const char* name = NameAt(scroll);                                           // [esp+0x18]
        for (int i = scroll; i < scroll + 0x10; i++) {                               // 0x0043acaf..0x0043ad64
            if (i >= n) break;                                                       // 0x0043acd1
            StrCpy(kBuf, name);                                                      // 0x0043acdb..0x0043acfd
            if (sel == i)                                                            // 0x0043ad03
                kFillWindowRect(kRoot, 0, 0x136, y - 1, 0x1C8, 0xF, static_cast<int>(0x80001284));   // 0x0043ad21
            // 0x0043ad26..0x0043ad32: the selected row in 0x80007fff, the others in 0x80000000
            kDrawText(reinterpret_cast<int>(kBuf), 0x140, y,
                      static_cast<int>(sel == i ? 0x80007FFFu : 0x80000000u));       // 0x0043ad44
            name += 100;                                                             // 0x0043ad50
            y += 0x10;                                                               // 0x0043ad53
        }
        if (sel != -1) {                                                             // 0x0043ad6a: the pro's card
            // 0x0043ad75..0x0043adb0: portrait = the Surface 0x562914 + 0x84 * (c + 72 * t), c = signed byte
            // 0x004d60aa, t = 1 when bit 7 of 0x004d60a9 is clear
            const int t = (G<unsigned char>(0x004D60A9) & 0x80) ? 0 : 1;
            const int c = static_cast<signed char>(G<unsigned char>(0x004D60AA));
            const int idx = c + t * 72;
            kDrawScaled(P(0x00562914 + idx * 0x84), 0, Disp(), 0x45, 0x3D, 1, 1, 1, 0);   // 0x0043adb0
            StrCpy(kBuf, S(kStrDesigner));                                           // 0x0043adb5..0x0043ade9
            StrCat(kBuf, S(kStrSkillsSuffix));                                       // 0x0043adeb..0x0043ae14
            kDrawText(reinterpret_cast<int>(kBuf), 0x38, 0xFF, static_cast<int>(0x80000000));   // 0x0043ae16
            StrCpy(kBuf, S(kStrQuote));                                              // 0x0043ae1b..0x0043ae4a
            G<short>(0x0057956E) = 0;                                                // 0x0043ae43
            kBuildGolferComment(0x3E, 0, 0, 0);                                      // 0x0043ae4c
            StrCat(kBuf, S(kStrQuote));                                              // 0x0043ae51..0x0043ae88
            kDrawTextSurface(reinterpret_cast<int>(kBuf), 0x24, 0x232, static_cast<int>(0x80007FFF));   // 0x0043ae8f
            kDrawText(static_cast<int>(kStrSignature), 0x38, 0x220, static_cast<int>(0x80000000));     // 0x0043aea5
            // 0x0043aeaa..0x0043afb1: ten skill rows from y 0x117 every 0x18: the name from the table 0x004c2c3c and
            // "+" (skill byte * 10) "%"
            int row = 0x117;
            for (int i = 0; i < 10; i++) {
                StrCpy(kBuf, *reinterpret_cast<const char* const*>(kSkillNames + 4 * i));   // 0x0043aebb..0x0043aee9
                kDrawText(reinterpret_cast<int>(kBuf), 0x5E, row, static_cast<int>(0x80000210));   // 0x0043aeeb
                StrCpy(kBuf, S(kStrPlus));                                           // 0x0043aef0..0x0043af25
                const int v = static_cast<signed char>(G<unsigned char>(kProSkills + i)) * WRONG(18, 10, 9);
                StrCat(kBuf, kItoa(v, kItoaBuf, 10));                                // 0x0043af27..0x0043af67
                StrCat(kBuf, S(kStrPercent));                                        // 0x0043af69..0x0043af92
                kDrawCenteredText(reinterpret_cast<int>(kBuf), 0x3B, row, static_cast<int>(0x80000210));   // 0x0043af94
                row += 0x18;                                                         // 0x0043afa4
            }
        }
        kRefreshFull(kRoot, 0, 0);                                                   // 0x0043afb7..0x0043afbe

        // ---------------------------------------------------------------- input loop (0x0043afc3..0x0043b246)
        int hot = -1;                                                                // ebx (0x0043afc7)
        int dwell = 0;                                                               // ebp (0x0043afca)
        int action;                                                                  // -2: the list press 0x0043b3b1
        for (;;) {
            kWaitTicks(2, 0);                                                        // 0x0043afcc..0x0043afd0
            kPumpMessages();                                                         // 0x0043afd8
            int a, b;                                                                // [esp+0x20] / [esp+0x1c]
            kViewToLocal(kRoot, 0, reinterpret_cast<int>(&a), reinterpret_cast<int>(&b));   // 0x0043afdd..0x0043afec
            const int prev = hot;                                                    // esi (0x0043b005)
            hot = -1;                                                                // 0x0043b008
            if (kApproxDistance(a - 0x279, b - 0x22B) < 0x1A) hot = 0;               // 0x0043aff1..0x0043b018
            if (kApproxDistance(a - 0x301, b - 0x22C) < 0x18) hot = 1;               // 0x0043b01a..0x0043b03d
            if (kApproxDistance(a - 0x310, b - 0x78) < 0x14) hot = 2;                // 0x0043b042..0x0043b062
            if (kApproxDistance(a - 0x310, b - 0x190) < 0x14) hot = 3;               // 0x0043b067..0x0043b089
            if (kApproxDistance(a - 0x2C0, b - 0x22B) < 0x17) hot = 4;               // 0x0043b08e..0x0043b0b1
            if (hot != prev) {                                                       // 0x0043b0b6
                if (prev != -1)                                                      // 0x0043b0be
                    kImageDraw(surf[prev], 0, Disp(), kBtnX[prev], kBtnY[prev], 0);  // 0x0043b0c3..0x0043b0e0
                if (hot != -1)                                                       // 0x0043b0e5
                    kImageDraw(surf[WRONG(15, 5 + hot, hot)], 0, Disp(), kBtnX[hot], kBtnY[hot], 0);   // 0x0043b0ea..0x0043b10a
                if (sel == -1) {                                                     // 0x0043b10f
                    kImageDraw(surf[10], 0, Disp(), 0x258, 0x208, 0);                // 0x0043b12e
                    kImageDraw(surf[14], 0, Disp(), 0x29E, 0x208, 0);                // 0x0043b14d
                }
                kRefreshFull(kRoot, 0, 0);                                           // 0x0043b152..0x0043b159
                dwell = 0;                                                           // 0x0043b15e
            } else {
                dwell++;                                                             // 0x0043b165
                if (dwell > 0x1E && hot != -1) {                                     // 0x0043b166..0x0043b172: the tip
                    kBuf[0] = 0;                                                     // 0x0043b17a
                    switch (hot) {                                                   // 0x0043b181..0x0043b18c
                    case 0: StrCpy(kBuf, S(sel == -1 ? kStrEmpty : kStrLoad)); break;   // 0x0043b1b7..0x0043b1e4
                    case 1: StrCpy(kBuf, S(kStrCancel)); break;                      // 0x0043b195..0x0043b1e4
                    case 4: StrCpy(kBuf, S(kStrDelete)); break;                      // 0x0043b18e / 0x0043b1c6..0x0043b1e4
                    default: break;
                    }
                    kDrawCenteredText2(reinterpret_cast<int>(kBuf), kBtnX[hot] + 0x1E, kBtnY[hot] + 0x10,
                                       static_cast<int>(0x80007FFF));                // 0x0043b1e6..0x0043b200
                    kRefreshFull(kRoot, 0, 0);                                       // 0x0043b208..0x0043b20f
                }
            }
            // 0x0043b218..0x0043b24b: a press acts on the hot button (none: the list press); otherwise the loop goes
            // on until flushRedraw reports a key: Esc is back, any other key acts like a press
            if (G<int>(kMouseButton) == 0) {
                if (kFlushRedraw() == 0) continue;                                   // 0x0043b221..0x0043b228
                if (G<int>(kMouseButton) == 0 && kSetDirtyFlagB() == 0x1B) {          // 0x0043b22e..0x0043b23f
                    hot = 1; action = 1; break;                                      // 0x0043b241
                }
            }
            action = hot == -1 ? -2 : hot;                                           // 0x0043b248
            break;
        }

        if (action != -2) {                                                          // 0x0043b251..0x0043b264
            G<int>(kMouseButton) = 0;                                                // 0x0043b254
            if (static_cast<unsigned>(action) > 4) continue;                         // 0x0043b25e (ja: redraw)
            if (action == 0) {                                                       // 0x0043b456: load
                if (sel < 0 || sel >= n) { result = 0; break; }                      // 0x0043b45a..0x0043b466 -> 0x0043b542
                kSetDrawTarget(P(Disp()), 0, 0x00519928, 0, 0, 0);                   // 0x0043b46c..0x0043b47d
                kDrawText(static_cast<int>(kStrLoading), 0x184, 0x1D8, static_cast<int>(0x80007FFF));   // 0x0043b496
                kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                   // 0x0043b49b..0x0043b4af
                kRefreshFull(kRoot, 0, 0);                                           // 0x0043b4b4..0x0043b4bb
                kLoadThemeFile(reinterpret_cast<int>(NameAt(sel)), 0, 1);            // 0x0043b4c0..0x0043b4d2
                result = 1;
                break;
            }
            if (action == 1) {                                                       // 0x0043b59c: back, the theme name
                kEnterModal483d30();                                                 // left as set at entry
                kImageDtor(img, 0);                                                  // 0x0043b5b0
                kVecDtor(surfaces, kSurfaceSize, kSurfaceCount, kSurfaceDtor);       // 0x0043b5b5..0x0043b5ce
                return 0;                                                            // 0x0043b5d3
            }
            if (action == 2) { scroll = kClamp(scroll - 4, 0, maxScroll); continue; }   // 0x0043b26b..0x0043b286
            if (action == 3) { scroll = kClamp(scroll + WRONG(16, 4, 5), 0, maxScroll); continue; }   // 0x0043b28b..0x0043b2a6
            // 4: delete (0x0043b2ab..0x0043b3ac), always from the championship directory
            StrCpy(kBuf, S(kStrAskDelete));                                          // 0x0043b2ab..0x0043b2d8
            char* victim = NameAt(sel);                                              // 0x0043b2bb..0x0043b2da (ebx)
            StrCat(kBuf, victim);                                                    // 0x0043b2e4..0x0043b30e
            StrCat(kBuf, S(kStrYesNo));                                              // 0x0043b310..0x0043b340
            if (kShowPopup(0x190, 0x64, 1, 1, 0) != 0) continue;                     // 0x0043b2ee..0x0043b34c
            StrCpy(kBuf, S(kStrChampDir));                                           // 0x0043b352..0x0043b37a
            StrCat(kBuf, victim);                                                    // 0x0043b37c..0x0043b3a2
            kRemove(reinterpret_cast<int>(kBuf));                                    // 0x0043b3a4
            continue;
        }
        // 0x0043b3b1..0x0043b451: a press at the last press position
        if (G<int>(kMouseX) <= 0x140) continue;                                      // 0x0043b3b1..0x0043b3bb
        const int my = G<int>(kMouseY);
        if (my < 0x74) continue;                                                     // 0x0043b3c1..0x0043b3c9
        sel = (my - 0x74) / 16 + scroll;                                             // 0x0043b3cf..0x0043b3e7 (no row bound)
        if (sel < n) {                                                               // 0x0043b3e5..0x0043b3eb
            kLoadThemeFile(reinterpret_cast<int>(NameAt(WRONG(17, sel, sel + 1))), 0, 1);   // 0x0043b3ed..0x0043b3ff
            continue;
        }
        sel = -1;                                                                    // 0x0043b426
        kImageDraw(surf[10], 0, Disp(), 0x258, 0x208, 0);                            // 0x0043b40c..0x0043b42e
        kImageDraw(surf[14], 0, Disp(), 0x29E, 0x208, 0);                            // 0x0043b433..0x0043b44c
    }
    // 0x0043b4da / 0x0043b542: restore the theme name saved at entry
    kEnterModal483d30();
    StrCpy(kThemeName, themeCopy);                                                   // 0x0043b4df..0x0043b50c / 0x0043b547..0x0043b573
    kImageDtor(img, 0);                                                              // 0x0043b515 / 0x0043b57c
    kVecDtor(surfaces, kSurfaceSize, kSurfaceCount, kSurfaceDtor);                   // 0x0043b51a..0x0043b533 / 0x0043b581..0x0043b5ce
    return result;                                                                   // 0x0043b538 / 0x0043b5d3
}
C0_t PickAProScreen_orig;
// 0x0043a8c0
SG_HOOK("golf_clean.exe", 0x0043a8c0, pickAProScreen, PickAProScreen_re, PickAProScreen_orig);

// ====================================================================================================================
// courseActionMenu 0x0046f550 (__cdecl, one stack argument it never reads, `ret` at 0x004709ba); reached through the
// thunk 0x00470a00 (`jmp`), called at 0x00407d46 and 0x0040fb9f. Listing log/c3/alt/c3fw_46f550.asm (frame offsets
// normalised in c3fw_46f550_norm.asm, string idioms folded in c3fw_46f550_fold.asm). Its switch-case block
// 0x004702bb..0x004703a2 lies outside both ranges Ghidra gives the function; the jump table 0x004709dc points into it.
// ====================================================================================================================
// The world map: the 16 course sites, a nearest-site hover, four buttons and the purchase of a site. F = esp after
// the four register pushes (0x0046f56f). Frame: hot button / draw counter [F+0x10], nearest site [F+0x14] (-1), the
// pointer [F+0x20] (x) / [F+0x18] (y), the record offset [F+0x1c], a blink counter [F+0x24], a dwell counter [F+0x28],
// [F+0x2c] = nearest % 16, the previous hot button [F+0x30], the site -> course-record table int[16] at [F+0x34], the
// Surface array (8) at [F+0x74], a Cache482fd0 at [F+0x1d4] and the Image at [F+0x22c]. [F+0x24], [F+0x28] and
// [F+0x30] are never initialised (first written at 0x004703da, 0x004703a2 / 0x004702a1, 0x004703aa); the table entry
// of a site no course record names stays unwritten. The reimplementation keeps those slots and the table at the same
// distance from the Surface array (struct CamFrame), so values the A/B fixture leaves there are seen alike.
namespace {

typedef int(__fastcall* Rnd_t)(void* ecx, void* edx, int);
const T0_t kCacheCtor = reinterpret_cast<T0_t>(0x00482FD0);              // Cache482fd0::ctor
const T0_t kCacheDtor = reinterpret_cast<T0_t>(0x00483010);              // Cache483010::dtor (no stack argument)
const Tn5_t kFillRegion = reinterpret_cast<Tn5_t>(0x00475C90);           // Surface_fillRegion (`ret 0x14`)
const C1_t kTierPrice = reinterpret_cast<C1_t>(0x0046F1D0);
const C0_t kSeedCourseTable = reinterpret_cast<C0_t>(0x0046F2B0);
const C5_t kDrawPanelS2 = reinterpret_cast<C5_t>(0x0040CC00);
const C5_t kDrawFrameR3 = reinterpret_cast<C5_t>(0x0040CEF0);
const C4_t kDrawWindow = reinterpret_cast<C4_t>(0x0040D320);
const C3_t kStartMessage = reinterpret_cast<C3_t>(0x0040CB00);
const C1_t kAppendDate = reinterpret_cast<C1_t>(0x0040D7B0);
const C4_t kTextInputBox = reinterpret_cast<C4_t>(0x0045B2C0);
const C1_t kPumpAndRedraw = reinterpret_cast<C1_t>(0x0045C0C0);
const C2_t kLoadGameBrowser = reinterpret_cast<C2_t>(0x0043B610);
const C2_t kDrawPanelBackdrop = reinterpret_cast<C2_t>(0x00432620);      // (x, y), text in 0x0051a068
const C5_t kPlaySound = reinterpret_cast<C5_t>(0x004481B0);
const Rnd_t kRandomRange = reinterpret_cast<Rnd_t>(0x0045C1E0);         // Random::range (this 0x00822d9c, n)

char* const kItoaBuf2 = S(0x008392AC);           // the _itoa buffer here
const unsigned kCourseRecs = 0x00571FF4;         // the 16 records (0x2e) from their site byte: +0 site, +1, +4, +6, +7
const unsigned kCash = 0x00571FD4, kDate = 0x00834170, kFlags = 0x0059E7B8;

// site k's definition (0x82 bytes from 0x004c1e90): name +0, x +0x3a, y +0x3c, bytes +0x3e, +0x3f, +0x40, text +0x41;
// the body walks it through ebx = 0x004c1ed1 + 0x82 * k (0x0046f9bd, 0x00470144)
static inline const char* SiteBase(int k) { return reinterpret_cast<const char*>(0x004C1ED1 + 0x82 * k); }
static inline int SiteX(int k) { return *reinterpret_cast<const short*>(0x004E3DC0 + 4 * k); }   // [ebp + 0x4e3dfc]
static inline int SiteY(int k) { return *reinterpret_cast<const short*>(0x004E3DC2 + 4 * k); }   // [ebp + 0x4e3dfe]
static inline unsigned char RecByte(int c, int off) { return G<unsigned char>(kCourseRecs + 0x2E * c + off); }

struct CamFrame {
    volatile int blink;        // F+0x24
    volatile int dwell;        // F+0x28
    int mod16;                 // F+0x2c
    volatile int prevHot;      // F+0x30
    int site[16];              // F+0x34
    char arr[8 * 0x2C];        // F+0x74
};

}  // namespace

// 0x0046f550
int __cdecl CourseActionMenu_re(int) {
    CamFrame f;
    alignas(8) char cache[0x58];          // F+0x1d4
    alignas(8) char image[0x2B8];         // F+0x22c
    void* const img = image;
    char* const arr = f.arr;
    kVecCtor(arr, 0x2C, 8, kSurfaceCtor, kSurfaceDtor);                              // 0x0046f56f..0x0046f58d
    int nearest = -1;                                                                // [F+0x14] (0x0046f585)
    kCacheCtor(cache, 0);                                                            // 0x0046f5a4
    kImageCtor(img, 0);                                                              // 0x0046f5b8
    kLoadImage(img, 0, 0x004E4054, 0, 0, 0x100, 2);                                  // 0x0046f5bd..0x0046f5dc
    static const struct { int elem, x, y, w, h; } kCutsA[7] = {
        {0, 0x0FA, 0x198, 0x4C, 0x72}, {1, 0x2D4, 0x198, 0x4C, 0x72}, {2, 0x226, 0x1CA, 0x4C, 0x40},
        {3, 0x190, 0x1CA, 0x4C, 0x40}, {4, 0x226, 0x198, 0x4C, 0x32}, {5, 0x190, 0x198, 0x4C, 0x32},
        {7, 0x2DC, 0x214, 0x44, 0x44}};                                              // 0x0046f602..0x0046f6e6
    for (int c = 0; c < 7; c++)
        kSurfaceBlit(arr + 0x2C * kCutsA[c].elem, 0, reinterpret_cast<int>(img), kCutsA[c].x, kCutsA[c].y,
                     kCutsA[c].w, kCutsA[c].h, 1, 1);
    kLoadImage(img, 0, 0x004E403C, reinterpret_cast<int>(cache), 0, 0x100, 0);      // 0x0046f6eb..0x0046f708
    kSurfaceBlit(arr + 0x2C * 6, 0, reinterpret_cast<int>(img), 0x2DC, 0x214, 0x44, 0x44, 1, 1);   // 0x0046f72e

    int hot;                                                                         // [F+0x10]
    int result;
    for (;;) {
        // ---------------------------------------------------------------- one frame (0x0046f733)
        for (int c = 0; c < 16; c++) {                                               // 0x0046f733..0x0046f74a
            const int site = static_cast<signed char>(RecByte(c, 0));
            *reinterpret_cast<int*>(reinterpret_cast<char*>(f.site) + 4 * site) = c;   // no bound on the site byte
        }
        kFillRegion(img, 0, kScreenSurface, 0, 0, 0x320, 0x258);                     // 0x0046f74c..0x0046f770
        if (G<int>(kDate) == 0) {                                                    // 0x0046f775..0x0046f77e
            kImageDraw(arr, 0, Disp(), 0x2D4, 0x198, 0);                             // 0x0046f780..0x0046f7d0
        } else {
            kImageDraw(arr + 0x2C * 2, 0, Disp(), 0x2D4, 0x1CA, 0);                  // 0x0046f799..0x0046f7b1
            kImageDraw(arr + 0x2C * 4, 0, Disp(), 0x2D4, 0x198, 0);                  // 0x0046f7b6..0x0046f7d0
        }
        kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                           // 0x0046f7e6
        kDrawCenteredText(0x004E4024, 0x7E, 0x13, static_cast<int>(0x80000000));     // 0x0046f7f9
        kDrawCenteredText(0x004E4010, 0x7E, 0x21, static_cast<int>(0x80000000));     // 0x0046f80c
        kSetDrawTarget(P(Disp()), 0, 0x00519928, 0, 0, 0);                           // 0x0046f825
        StrCpy(kBuf, S(0x004C61E0));                                                 // 0x0046f82a..0x0046f848
        kAppendNumber(G<int>(kCash) * 100);                                          // 0x0046f84a..0x0046f85e
        if (G<int>(kFlags) & 0x1000000) {                                            // 0x0046f863..0x0046f870
            StrCpy(kBuf, S(0x004E4004));                                             // 0x0046f872..0x0046f895
            StrCat(kBuf, S(0x004C61E0));                                             // 0x0046f897..0x0046f8c0
        }
        kDrawCenteredText(reinterpret_cast<int>(kBuf), 0x2E4, 0x12, static_cast<int>(0x80000000));   // 0x0046f8d3
        kSetDrawTarget(P(Disp()), 0, 0x00519FD8, 0, 0, 0);                           // 0x0046f8ec
        kStretchBlit(P(0x0053C85C), 0, 0x005A4424, Disp(), 0x18, 0x226, 0);          // 0x0046f90a
        kDrawText(0x004E3FF8, 0x2C, 0x230, static_cast<int>(0x80000000));            // 0x0046f920
        kStretchBlit(P(0x0053C830), 0, 0x005A43F8, Disp(), 0x7C, 0x226, 0);          // 0x0046f942
        kDrawText(0x004E3FE4, 0x90, 0x230, static_cast<int>(0x80000000));            // 0x0046f95b
        kStretchBlit(P(0x0053C888), 0, 0x005A4450, Disp(), 0xF4, 0x226, 0);          // 0x0046f980
        kDrawText(0x004E3FD0, 0x108, 0x230, static_cast<int>(0x80000000));           // 0x0046f999
        f.mod16 = nearest % 16;                                                      // 0x0046f99e..0x0046f9b9 (signed)
        for (int k = 15; k >= 0; k--) {                                              // [F+0x10] 0xf .. 0 (0x0046f9aa, 0x0047014a)
            const char* d = SiteBase(k);                                             // ebx
            const int x = SiteX(k), y = SiteY(k);
            const int kind = static_cast<signed char>(d[-3]);
            kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                       // 0x0046f9c4..0x0046f9d5
            const int c = f.site[k];                                                 // [F+ebp+0x70]
            StrCpy(kBuf, d - 0x41);                                                  // 0x0046f9da..0x0046f9f6
            int st = kTierPrice(c) <= G<int>(kCash) ? 1 : 0;                         // 0x0046f9f8..0x0046fa16
            const int rec = c * 0x2E;                                                // [F+0x1c] (0x0046fa29)
            if (G<unsigned char>(kCourseRecs + 6 + rec) != 0xFF) st = 0;             // 0x0046fa2d..0x0046fa36
            if (k == nearest) {                                                      // 0x0046fa38..0x0046fa42
                if (st == 0)                                                         // 0x0046fa44
                    kImageDraw(P(0x0053C5C8 + kind * 0xB0), 0, Disp(), x, y, 0);     // 0x0046fa48..0x0046fa73
                else
                    st = 2;                                                          // 0x0046fa7a
            }
            kImageDraw(P(0x0053C570 + (st + kind * 4) * 0x2C), 0, Disp(), x, y, 0);  // 0x0046fa7f..0x0046faac
            kDrawCenteredText(reinterpret_cast<int>(kBuf), x + 0x60, y + 4, static_cast<int>(0x80000000));   // 0x0046fad1
            kSetDrawTarget(P(Disp()), 0, 0x00519FD8, 0, 0, 0);                       // 0x0046faea
            StrCpy(kBuf, d);                                                         // 0x0046faef..0x0046fb0f
            kDrawCenteredText(reinterpret_cast<int>(kBuf), x + 0x68, y + 0x10, static_cast<int>(0x80000000));   // 0x0046fb31
            const int lbl = static_cast<int>(0x80002108);
            if (k == nearest && G<unsigned char>(kCourseRecs + 6 + rec) == 0xFF) {   // 0x0046fb36..0x0046fb54
                StrCpy(kBuf, S(0x004E3FC8));                                         // 0x0046fb5a..0x0046fb84
                StrCat(kBuf, kItoa(G<unsigned char>(kCourseRecs + 1 + rec), kItoaBuf2, 10));   // 0x0046fb86..0x0046fbc1
                StrCat(kBuf, S(0x004E3FBC));                                         // 0x0046fbc3..0x0046fbf3
                switch (static_cast<signed char>(d[-1])) {                           // 0x0046fbe7..0x0046fc36
                case 0: StrCat(kBuf, S(0x004E3FA4)); break;
                case 1: StrCat(kBuf, S(0x004E3FAC)); break;
                case 2: StrCat(kBuf, S(0x004E3FB4)); break;
                default: break;
                }
                kDrawCenteredText(reinterpret_cast<int>(kBuf), x + 0x70, y + 0x1E, lbl);   // 0x0046fc58
                kBuf[0] = 0;                                                         // 0x0046fc67
                switch (static_cast<signed char>(d[-2])) {                           // 0x0046fc5d..0x0046fcaf
                case 0: StrCat(kBuf, S(0x004E3F80)); break;
                case 1: StrCat(kBuf, S(0x004E3F8C)); break;
                case 2: StrCat(kBuf, S(0x004E3F98)); break;
                default: break;
                }
                switch (kind) {                                                      // jump table 0x004709bc
                case 0: StrCat(kBuf, S(0x004E3F68)); break;                          // 0x0046fcc8
                case 1: StrCat(kBuf, S(0x004C9758)); break;                          // 0x0046fcd6
                case 2: StrCat(kBuf, S(0x004E3F74)); break;                          // 0x0046fcc1
                case 3: StrCat(kBuf, S(0x004E3F60)); break;                          // 0x0046fccf
                default: break;                                                      // 0x0046fcb8 (ja)
                }
                StrCat(kBuf, S(0x004E3F54));                                         // 0x0046fd03..0x0046fd2e
                kDrawCenteredText(reinterpret_cast<int>(kBuf), x + 0x6A, y + 0x28, lbl);   // 0x0046fd50
                kBuf[0] = 0;                                                         // 0x0046fd5f
                StrCat(kBuf, S(0x004E3F48));                                         // 0x0046fd55..0x0046fd8f
                kAppendNumber(kTierPrice(c) * 100);                                  // 0x0046fd82..0x0046fda0
                StrCat(kBuf, S(0x004C4944));                                         // 0x0046fda5..0x0046fddd
                if (!(G<int>(kFlags) & 0x1000000))                                   // 0x0046fdce..0x0046fddf
                    kDrawCenteredText(reinterpret_cast<int>(kBuf), x + 0x68, y + 0x32, lbl);   // 0x0046fe01
                kBuf[0] = 0;                                                         // 0x0046fe09
            } else {
                kBuf[0] = 0;                                                         // 0x0046fe28
                StrCat(kBuf, kItoa(G<unsigned char>(kCourseRecs + 1 + rec), kItoaBuf2, 10));   // 0x0046fe15..0x0046fe5d
                StrCat(kBuf, S(0x004E3F3C));                                         // 0x0046fe5f..0x0046fe8d
                kAppendNumber(kTierPrice(c) * 100);                                  // 0x0046fe83..0x0046fe9e
                const unsigned char y6 = G<unsigned char>(kCourseRecs + 6 + rec);    // 0x0046feaa
                if (y6 != 0xFF) {                                                    // 0x0046feb0..0x0046feb2
                    kBuf[0] = 0;                                                     // 0x0046fec9
                    StrCat(kBuf, kItoa(static_cast<signed char>(y6) + 0x7D1, kItoaBuf2, 10));   // 0x0046feb8..0x0046ff00
                    StrCat(kBuf, S(0x004E3F38));                                     // 0x0046ff02..0x0046ff37
                    const unsigned char y7 = G<unsigned char>(kCourseRecs + 7 + rec);   // 0x0046ff26..0x0046ff35
                    StrCat(kBuf, y7 == 0xFF ? S(0x004E3F30)
                                            : kItoa(static_cast<signed char>(y7) + 0x7D1, kItoaBuf2, 10));   // 0x0046ff39..0x0046ff87
                    if (k == nearest)                                                // 0x0046ff7e..0x0046ff8d
                        kDrawCenteredText(0x004E3F1C, x + 0x70, y + 0x2D, lbl);      // 0x0046ffaf
                }
                if (!(G<int>(kFlags) & 0x1000000))                                   // 0x0046ffb7..0x0046ffc1
                    kDrawCenteredText(reinterpret_cast<int>(kBuf), x + 0x70, y + 0x1E, lbl);   // 0x0046ffe3
            }
            // 0x0046ffeb..0x0047013b: the plate state (2: record byte +6 not 0xff, 1: price <= cash, 0: not, 3: the
            // nearest site mod 16 while bit 3 of the frame counter is set) and its placement by the definition's x / y
            int ms = kTierPrice(c) <= G<int>(kCash) ? 1 : 0;                         // 0x0046ffeb..0x00470004
            if (G<unsigned char>(kCourseRecs + 6 + rec) != 0xFF) ms = 2;             // 0x00470009..0x00470016
            if (f.mod16 == k && (f.blink & WRONG(21, 8, 4))) ms = 3;                 // 0x0047001b..0x0047002e
            const int dx = *reinterpret_cast<const short*>(d - 7), dy = *reinterpret_cast<const short*>(d - 5);
            int v;
            if (0xC8 - dx > 0) {                                                     // 0x00470033..0x0047004b
                if (0x12C - dy > 0) {                                                // 0x0047004d
                    v = 4;
                    kStretchBlit(P(0x0053C830 + ms * 0x2C), 0, 0x005A43F8 + ms * 0x2C, Disp(), dx - 0xC, dy - 0xE, 0);
                } else {
                    v = 5;
                    kStretchBlit(P(0x0053C8E0 + ms * 0x2C), 0, 0x005A44A8 + ms * 0x2C, Disp(), dx - 0xF, dy, 0);
                }
            } else if (0x12C - dy > 0) {                                             // 0x004700a8
                v = 6;
                kStretchBlit(P(0x0053C990 + ms * 0x2C), 0, 0x005A4558 + ms * 0x2C, Disp(), dx, dy - 0xE, 0);
            } else {
                v = 7;
                kStretchBlit(P(0x0053CA40 + ms * 0x2C), 0, 0x005A4608 + ms * 0x2C, Disp(), dx, dy, 0);   // 0x004700fb
            }
            const int e = ms + v * 4;                                                // 0x00470107
            kStretchBlit(P(0x0053C570 + e * 0x2C), 0, 0x005A4138 + e * 0x2C, Disp(), x + 0xA0, y + 0xA, 0);   // 0x0047013b
        }
        hot = -1;                                                                    // 0x00470185..0x0047018a
        int a, b;                                                                    // [F+0x20] / [F+0x18]
        kViewToLocal(kRoot, 0, reinterpret_cast<int>(&a), reinterpret_cast<int>(&b));   // 0x0047015e..0x0047016d
        if (kApproxDistance(a - 0x300, b - 0x22D) < 0x19) hot = 3;                   // 0x00470172..0x004701a0
        if (kApproxDistance(a - 0x2F8, b - 0x1DC) < 0x19) hot = G<int>(kDate) != 0 ? 1 : 0;   // 0x004701a4..0x004701d3
        bool tested = false;
        if (G<int>(kDate) != 0 && kApproxDistance(a - 0x306, b - 0x1A9) < 0x14) {    // 0x004701d7..0x00470202
            hot = 2;
            tested = true;                                                           // jmp 0x00470216
        }
        bool skip = false;                                                           // -1: straight to 0x004703ae
        if (!tested && hot == -1) skip = true;                                       // 0x00470208..0x0047020b
        if (!skip) {
            if (static_cast<unsigned>(hot) <= 3) {                                   // 0x00470211..0x00470216
                static const int kHx[4] = {0x2D4, 0x2D4, 0x2D4, 0x2DC};
                static const int kHy[4] = {0x198, 0x1CA, 0x198, 0x214};
                kImageDraw(arr + 0x2C * (2 * hot + 1), 0, Disp(), kHx[hot], kHy[hot], 0);   // table 0x004709cc
            }
            if (f.prevHot != hot) {                                                  // 0x0047028f..0x00470293
                f.dwell = 0;                                                         // 0x004703a2
            } else {
                f.dwell = f.dwell + 1;                                               // 0x00470299..0x004702a1
                if (f.dwell > 0x14 && static_cast<unsigned>(hot) <= 3) {             // 0x004702a5..0x004702ae: the tip
                    static const unsigned kTipText[4] = {0x004E3F10, 0x004E3F04, 0x004E3EF8, 0x004C8590};
                    static const int kTipX[4] = {0x2F8, 0x2F8, 0x306, 0x300};
                    static const int kTipY[4] = {0x1DC, 0x1DC, 0x1A9, 0x22D};
                    StrCpy(kBuf, S(kTipText[hot]));                                  // table 0x004709dc
                    kDrawPanelBackdrop(kTipX[hot], kTipY[hot]);
                }
            }
            f.prevHot = hot;                                                         // 0x004703aa
        }
        kRefreshFull(kRoot, 0, 0);                                                   // 0x004703ae..0x004703b5
        kWaitTicks(5, 0);                                                            // 0x004703ba..0x004703be
        f.blink = f.blink + 1;                                                       // 0x004703c3..0x004703da
        kViewToLocal(kRoot, 0, reinterpret_cast<int>(&a), reinterpret_cast<int>(&b));   // 0x004703ca..0x004703de
        // 0x004703e3..0x00470461: the nearest site within 0x1e of the pointer: each site is tested against its map
        // spot +0x19 and against its definition's x / y
        int best = 0x1E;                                                             // edi
        nearest = -1;                                                                // 0x004703e8
        for (int k = 0; k < 16; k++) {
            const int d1 = kApproxDistance(a - SiteX(k) - WRONG(22, 0x19, 0x18), b - SiteY(k) - 0x19);   // 0x004703fc..0x00470417
            if (d1 < best) { best = d1; nearest = k; }
            const char* d = SiteBase(k);
            const int d2 = kApproxDistance(a - *reinterpret_cast<const short*>(d - 7),
                                           b - *reinterpret_cast<const short*>(d - 5));   // 0x00470429..0x0047043f
            if (d2 < best) { best = d2; nearest = k; }
        }
        kPumpMessages();                                                             // 0x00470463
        const int btn = G<int>(kMouseButton);                                        // 0x00470468
        if (btn == 0) continue;                                                      // 0x00470470
        if (hot == 0) { kSeedCourseTable(); continue; }                              // 0x0047047e
        if (hot == 1) {                                                              // 0x00470491..0x00470605: save
            kDrawPanelS2(0x78, 0x50, 0x210, 0x50, 0);                                // 0x0047049e
            kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                       // 0x004704b7
            kDrawCenteredText2(0x004C3F74, 0x180, 0x58, static_cast<int>(0x80007FFF));   // 0x004704cd
            kBuf[0] = 0;                                                             // 0x004704d4
            kAppendCourseTitle(-1);                                                  // 0x004704db
            StrCat(kBuf, S(0x004C3F70));                                             // 0x004704e0..0x0047051c
            // 0x0047050d..0x0047053b: (date % 1024 (signed) * 30) / 1024 + 1
            const int day = (G<int>(kDate) % 1024) * 30 / 1024 + 1;
            StrCat(kBuf, kItoa(day, kItoaBuf2, 10));                                 // 0x0047053d..0x0047056a
            StrCat(kBuf, S(0x004C3F70));                                             // 0x0047056c..0x0047059b
            kAppendDate(G<int>(kDate));                                              // 0x00470590..0x0047059d
            kTextInputBox(0x004C3F5C, 0x80, 0x66, 0x28);                             // 0x004705a2..0x004705b0
            if (kBuf[0] == 0) continue;                                              // 0x004705b5..0x004705bf
            kSaveGame(reinterpret_cast<int>(kBuf));                                  // 0x004705c5..0x004705ca
            StrCpy(kBuf, S(0x004C3F00));                                             // 0x004705cf..0x004705fb
            kStartMessage(static_cast<int>(0x80001284), 1, -4);                      // 0x004705d9..0x004705fd
            continue;
        }
        if (hot == 2) {                                                              // 0x0047060a..0x0047062f: load
            if (kLoadGameBrowser(G<int>(kDate) == 0 ? 1 : 0, 0) != 0) { result = -1; break; }
            continue;
        }
        if (hot == 3) { result = -1; break; }                                        // 0x00470634..0x00470637: back
        // 0x0047063d..0x00470875: a left press on the nearest site whose record byte +6 is 0xff buys it
        if (btn != 1) continue;                                                      // 0x0047063d
        if (nearest == -1) continue;                                                 // 0x00470646..0x0047064d
        const int c = f.site[WRONG(19, nearest, (nearest + 1) & 15)];                // 0x00470653..0x00470657
        if (G<unsigned char>(kCourseRecs + 6 + c * 0x2E) != 0xFF) continue;          // 0x0047065b..0x0047066b
        StrCpy(kBuf, S(0x004E3EDC));                                                 // 0x00470671..0x00470696
        StrCat(kBuf, S(0x004E3EBC));                                                 // 0x00470698..0x004706c9
        const int cheat = G<int>(0x005A6364) == 0x11 ? 1 : 0;                         // ebp (0x004706bc..0x004706cd)
        if (G<int>(kCash) < kTierPrice(c) && cheat == 0) {                           // 0x004706d2..0x004706e7
            kDrawWindow(0xC8, 0xC8, static_cast<int>(0x80006000), -2);               // 0x004706e9..0x004706fa
            kRefreshFull(kRoot, 0, cheat);                                           // 0x00470702..0x00470708
            kPumpAndRedraw(cheat);                                                   // 0x0047070d
            continue;
        }
        kSetDrawTarget(P(Disp()), 0, 0x0051B360, 0, 0, 0);                           // 0x0047071b..0x0047072c
        kDrawFrameR3(0xCC, 0x114, 0xC0, 0x30, 1);                                    // 0x00470731..0x00470744
        StrCpy(kBuf, S(0x004E3EAC));                                                 // 0x0047074c..0x00470780
        StrCat(kBuf, S(0x00571FD8 + c * 0x2E));                                      // 0x00470753..0x004707b8: the record's name
        StrCat(kBuf, S(0x004C4E60));                                                 // 0x004707ba..0x004707e3
        kDrawCenteredText2(reinterpret_cast<int>(kBuf), 0x12C, 0x11D, static_cast<int>(0x80007FFF));   // 0x004707e5
        StrCpy(kBuf, S(0x004E3E90));                                                 // 0x004707ea..0x00470821
        kDrawCenteredText2(reinterpret_cast<int>(kBuf), 0x12C, 0x12C, static_cast<int>(0x80007FFF));   // 0x00470823
        kRefreshFull(kRoot, 0, 0);                                                   // 0x0047082b..0x00470832
        G<int>(kCash) = G<int>(kCash) - kTierPrice(c);                               // 0x00470837..0x00470857
        G<int>(0x00543CFC) = G<int>(0x00543CFC) | (1 << ((c + WRONG(20, 0, 1)) & 31));                   // 0x0047084b..0x00470869 (shl cl)
        switch (static_cast<signed char>(RecByte(c, 4))) {                           // 0x00470850..0x00470875, table 0x004709ec
        case 0: kPlaySound(0x33, 0x64, 0, 0, 0); break;                              // 0x00470939
        case 1: kPlaySound((kRandomRange(P(0x00822D9C), 0, 3) & 0xFFFF) + 0x78, 0x64, 0, 0, 0); break;   // 0x0047091a
        case 2: kPlaySound((kRandomRange(P(0x00822D9C), 0, 3) & 0xFFFF) + 0x6E, 0x64, 0, 0, 0); break;   // 0x004708fb
        case 3: kPlaySound((kRandomRange(P(0x00822D9C), 0, 3) & 0xFFFF) + 0x73, 0x64, 0, 0, 0); break;   // 0x0047087c
        default: break;                                                              // 0x0047086f (ja)
        }
        result = c;                                                                  // 0x004709a0 (ebx)
        break;
    }
    // 0x0047089e / 0x0047094b
    kEnterModal483d30();
    G<int>(kMouseButton) = 0;                                                        // 0x004708aa / 0x00470957
    kImageDtor(img, 0);                                                              // 0x004708bc / 0x00470969
    kCacheDtor(cache, 0);                                                            // 0x004708d0 / 0x0047097d
    kVecDtor(arr, 0x2C, 8, kSurfaceDtor);                                            // 0x004708d5..0x004708ee / 0x00470982..0x0047099b
    return result;                                                                   // 0x004708f3 (-1) / 0x004709a0
}
C1_t CourseActionMenu_orig;
// 0x0046f550
SG_HOOK("golf_clean.exe", 0x0046f550, courseActionMenu, CourseActionMenu_re, CourseActionMenu_orig);
