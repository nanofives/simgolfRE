// C3 batch c3fx (2026-10-16) of golf_clean.exe: showCharacterEditor 0x004385d0, the golfer / character editor (gender,
// skin, hair, outfit and body choices, the name / profession / bio texts, the 20 comment lines, the member's ten skill
// bars, load / save of .chr / .pro files).
// Fixtures: re/frida/js/fixtures.d/c3fx.js. Registry: re/frida/registry.d/c3fx.py. Purpose and branch notes:
// log/c3/c3fx_purpose.md. Notes: log/c3/c3fx_notes.txt. Original callees, slots, imports and literal globals (the port's
// data model): log/c3/c3fx_deps.tsv. Listings: log/c3/alt/c3fx_4385d0.asm (raw), c3fx_4385d0_norm.asm (every [esp+x]
// rewritten as a frame offset [F+x'], F = esp after the four register pushes, 0x00438603), c3fx_4385d0_fold.asm.
//
// Hand-written from the disassembly; every statement cites the instructions it reproduces. __thiscall is emulated with
// __fastcall (ecx = this, edx unused). Callees run through their original addresses, so a hooked callee runs its own
// reimplementation.
//
// FRAME. The original keeps one object in its frame, an Image at [F+0x3c] (Image_ctor 0x00474ae0 at 0x004386ba, its
// destructor 0x00474c40 at 0x0043a31e), under an SEH frame (handler 0x004b73ab, state word [F+0x2fc]); no callee throws a
// C++ exception, so the reimplementation calls the destructor on the normal path only. The dword [F+0x30] (the hair-style
// counter of control 10) is read before any write (0x00439b12 / 0x00439b29): it keeps the distance 0xc below the Image
// (struct CeFrame), so the value the A/B fixture's Image_ctor stand-in writes at this - 0xc is read alike in both arms.
// Locals of the original: x [F+0x24], y [F+0x1c] (View_toLocal), control [F+0x2c], member [F+0x20], done [F+0x34],
// record offset [F+0x10] = character * 0x230, golfer offset [F+0x28] = golfer * 0x100, flags byte [F+0x38].
#include "hooks.h"

#ifdef C3FX_NEGCTL
// negative-control build only (shim/src/re/c3fx_negctl.cpp includes this file with C3FX_NEGCTL defined): word 0 is set
// by __freeze.negOn() after the fixture froze the first thread; the wrong detail is chosen by the environment variable
// C3FX_NEG (read once); C3FX_NEG=99 makes the whole body return at once (the null control).
extern "C" __declspec(dllexport) volatile int C3fxNegCtl[4] = {0, 0x5147454e, 0, 0};
extern "C" __declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableA(const char*, char*, unsigned long);
static int NegSel() {
    static int sel = -1;
    if (sel < 0) {
        char b[16] = {0};
        unsigned long n = GetEnvironmentVariableA("C3FX_NEG", b, sizeof(b));
        sel = 0;
        for (unsigned long i = 0; i < n && b[i] >= '0' && b[i] <= '9'; i++) sel = sel * 10 + (b[i] - '0');
    }
    return sel;
}
#define NEG_ON(k) (C3fxNegCtl[0] != 0 && NegSel() == (k))
#define WRONG(k, right, wrong) (NEG_ON(k) ? (wrong) : (right))
#else
#define NEG_ON(k) false
#define WRONG(k, right, wrong) (right)
#endif

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline void* P(unsigned a) { return reinterpret_cast<void*>(a); }
static inline char* S(unsigned a) { return reinterpret_cast<char*>(a); }
static inline int I(const void* p) { return reinterpret_cast<int>(p); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int);
typedef int(__cdecl* C2_t)(int, int);
typedef int(__cdecl* C3_t)(int, int, int);
typedef int(__cdecl* C4_t)(int, int, int, int);
typedef int(__cdecl* C5_t)(int, int, int, int, int);
typedef int(__cdecl* C8_t)(int, int, int, int, int, int, int, int);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* Tn1_t)(void* ecx, void* edx, int);
typedef int(__fastcall* Tn2_t)(void* ecx, void* edx, int, int);
typedef int(__fastcall* Tn3_t)(void* ecx, void* edx, int, int, int);
typedef int(__fastcall* Tn4_t)(void* ecx, void* edx, int, int, int, int);
typedef int(__fastcall* Tn5_t)(void* ecx, void* edx, int, int, int, int, int);
typedef int(__fastcall* Tn6_t)(void* ecx, void* edx, int, int, int, int, int, int);
typedef int(__fastcall* Tn7_t)(void* ecx, void* edx, int, int, int, int, int, int, int);
typedef char*(__cdecl* Itoa_t)(int value, char* buf, int radix);

// ---- callees (original addresses) ----
const C4_t kDrawText = reinterpret_cast<C4_t>(0x004049D0);             // drawText4049d0 (text, x, y, colour)
const C4_t kDrawCenteredText = reinterpret_cast<C4_t>(0x00404B70);     // drawCenteredText (text, x, y, colour)
const C4_t kDrawCenteredText2 = reinterpret_cast<C4_t>(0x00404BC0);    // drawCenteredText2 (text, x, y, colour)
const C5_t kDrawPanelS2 = reinterpret_cast<C5_t>(0x0040CC00);          // drawPanelS2 (x, y, w, h, 1)
const C2_t kDrawPanelBackdrop = reinterpret_cast<C2_t>(0x00432620);    // drawPanelBackdrop (x, y); text in 0x0051a068
const C3_t kPromptAndSaveFile = reinterpret_cast<C3_t>(0x00437910);    // promptAndSaveFile (character, golfer | -1, 0)
const C3_t kLoadThemeFile = reinterpret_cast<C3_t>(0x00437FA0);        // loadThemeFile (name, character, golfer | -1)
const C2_t kNearestCharControl = reinterpret_cast<C2_t>(0x00438260);   // nearestCharControl (x, y) -> 0..20 | -1
const C1_t kPickCharEditor = reinterpret_cast<C1_t>(0x00438390);       // pickCharEditor (male) -> choice | -1
const C3_t kListMatchingFiles = reinterpret_cast<C3_t>(0x0043D2A0);    // listMatchingFiles (pattern, a, b) -> count
const C3_t kCellAt = reinterpret_cast<C3_t>(0x0043D6F0);               // cellAt (set, row, col)
const C5_t kPlaySound = reinterpret_cast<C5_t>(0x004481B0);            // playSound (id, volume, 0, 0, 0)
const C1_t kStartSoundSlot = reinterpret_cast<C1_t>(0x00448200);
const C4_t kDrawMessageBuffer = reinterpret_cast<C4_t>(0x0045B0D0);    // drawMessageBuffer (x, w, y, colour); text 0x0051a068
const C4_t kTextInputBox = reinterpret_cast<C4_t>(0x0045B2C0);         // textInputBox (prompt, x, y, len); result 0x0051a068
const C2_t kWaitTicks = reinterpret_cast<C2_t>(0x0045BF80);            // waitTicks (ticks, pump)
const C0_t kPumpMessages = reinterpret_cast<C0_t>(0x0045C030);
const Tn1_t kRandomRange = reinterpret_cast<Tn1_t>(0x0045C1E0);        // Random::range (this, n)
const C1_t kComposeGolferSprite = reinterpret_cast<C1_t>(0x00462020);
const C8_t kAddSprite = reinterpret_cast<C8_t>(0x004628D0);
const C1_t kThoughtFlag = reinterpret_cast<C1_t>(0x004675D0);
const C2_t kBuildGolferName = reinterpret_cast<C2_t>(0x004676E0);      // buildGolferName (golfer, 0) -> appends to 0x0051a068
const C4_t kBuildGolferComment = reinterpret_cast<C4_t>(0x00469B00);   // buildGolferComment (kind, value, 0, golfer)
const C1_t kTypeBit7Clear = reinterpret_cast<C1_t>(0x0046C940);
const C5_t kShowPopup = reinterpret_cast<C5_t>(0x0046D6E0);            // showPopup (x, y, a, b, c); text in 0x0051a068
const C3_t kScrollTextWindow = reinterpret_cast<C3_t>(0x0046DE70);     // scrollTextWindow (text, x, y) -> row | -1
const Tn7_t kDrawScaled = reinterpret_cast<Tn7_t>(0x00473CB0);         // Surface_drawScaled (target, x, y, 1, 1, 1, 0)
const Tn4_t kImageDraw = reinterpret_cast<Tn4_t>(0x00473E60);          // Image_draw (target, x, y, extra)
const T0_t kImageCtor = reinterpret_cast<T0_t>(0x00474AE0);
const T0_t kImageDtor = reinterpret_cast<T0_t>(0x00474C40);
const Tn5_t kLoadImage = reinterpret_cast<Tn5_t>(0x00475840);          // loadImage (path, 0, 0, 0x100, 2)
const Tn7_t kBlitThunk = reinterpret_cast<Tn7_t>(0x00475C60);          // Surface_blitThunk
const Tn4_t kSetDrawTarget = reinterpret_cast<Tn4_t>(0x004762D0);
const Tn3_t kPixelRed = reinterpret_cast<Tn3_t>(0x00479830);
const Tn3_t kPixelGreen = reinterpret_cast<Tn3_t>(0x004798C0);
const Tn3_t kPixelBlue = reinterpret_cast<Tn3_t>(0x00479950);
const Tn2_t kViewToLocal = reinterpret_cast<Tn2_t>(0x0047AB50);
const Tn1_t kWindowShow = reinterpret_cast<Tn1_t>(0x0047B670);
const Tn5_t kFillWindowRect = reinterpret_cast<Tn5_t>(0x00480B00);     // fillWindowRect (x, y, w, h, colour)
const Tn1_t kRefreshFull = reinterpret_cast<Tn1_t>(0x00480C80);        // Window::refreshFull
const Tn4_t kPaletteNearest = reinterpret_cast<Tn4_t>(0x00483420);     // Palette::nearest (r, g, b, 0)
const C0_t kShowScreen = reinterpret_cast<C0_t>(0x00483BB0);
const C0_t kRedrawAll = reinterpret_cast<C0_t>(0x00483C70);
const C0_t kEnterModal483d30 = reinterpret_cast<C0_t>(0x00483D30);
const C0_t kLeaveModal483d80 = reinterpret_cast<C0_t>(0x00483D80);
const T0_t kEditReleaseState = reinterpret_cast<T0_t>(0x004860D0);
const Tn6_t kEditCreate = reinterpret_cast<Tn6_t>(0x00486110);
const Tn1_t kEditSetText = reinterpret_cast<Tn1_t>(0x00486200);
const Tn1_t kEditResizeBuffer = reinterpret_cast<Tn1_t>(0x00486250);
const Tn1_t kEditSetCaretActive = reinterpret_cast<Tn1_t>(0x00486B30);
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);             // _itoa

// ---- globals ----
void* const kRoot = P(0x00519A60);                 // root window
const int kScreenSurface = 0x00519CD4;             // `0x519a60 ? 0x519cd4 : 0` (0x004386f4..0x0043870b), a constant
char* const kBuf = S(0x0051A068);                  // shared text buffer
char* const kItoaBuf = S(0x005A9F6C);              // _itoa's output buffer here
char* const kNames = S(0x0080B130);                // file names from listMatchingFiles, 100 bytes each
const unsigned kThemeName = 0x00567328;            // current theme name
const unsigned kDisplayPtr = 0x004C1570;           // the drawing target object passed to every draw call
const unsigned kMouseButton = 0x00822D68, kKeyWord = 0x00822B9C;
const unsigned kChar = 0x004D6088;                 // character records, 0x230 bytes each
const unsigned kBackup = 0x004E1618;               // the 0x230-byte copy taken at entry
const unsigned kGolfer = 0x005794C0;               // golfer records, 0x100 bytes each
const unsigned kPreview = 0x00582DC0;              // golfer 0x99 (= kGolfer + 0x99 * 0x100), the editor's preview golfer
const unsigned kTypes = 0x005849E0;                // per-character type records, 0x2c bytes each
const unsigned kComments = 0x00543D10;             // 20 comment lines of 0x32 bytes per character (0x4e2 each)
const unsigned kEditBox = 0x005A9F78;              // the global EditBox of the bio entry

static char* Rec(int c) { return S(kChar + c * 0x230); }
static unsigned char& RB(char* r, int off) { return *reinterpret_cast<unsigned char*>(r + off); }
static unsigned& RD(char* r, int off) { return *reinterpret_cast<unsigned*>(r + off); }

static void StrCpy(char* d, const char* s) { while ((*d++ = *s++) != 0) {} }
static void StrCat(char* d, const char* s) { while (*d) d++; StrCpy(d, s); }
static void MemCpy(void* d, const void* s, int n) {
    for (int i = 0; i < n; i++) static_cast<char*>(d)[i] = static_cast<const char*>(s)[i];
}
static int Abs(int v) { return v < 0 ? -v : v; }              // cdq / xor / sub (0x004387ac..0x004387af)
static int Target() { return G<int>(kDisplayPtr); }

// the frame: [F+0x30] keeps its distance 0xc below the Image at [F+0x3c] (see the header)
struct CeFrame {
    int style;                      // [F+0x30], read before any write
    int pad[2];                     // [F+0x34] / [F+0x38] (kept apart as locals below)
    unsigned char img[0x2B8];       // [F+0x3c] Image
};

// the hover tips 0x0043963e..0x00439747 (table 0x0043a368, control - 8)
const unsigned kTip[12] = {0x004C8604, 0x004C85F4, 0x004C85E8, 0x004C85DC, 0x004C85D0, 0x004C85C4,
                           0x004C85B8, 0x004C85AC, 0x004C85A0, 0x004C8598, 0x004C8590, 0x004C8588};

}  // namespace

int __cdecl ShowCharacterEditor_re(int character, int golfer) {
#ifdef C3FX_NEGCTL
    if (NEG_ON(99)) return 0;                                                       // null control
#endif
    CeFrame f;
    char* const R1 = Rec(character);                                                // [F+0x10] = character * 0x230 (0x0043863e)
    char* const Gr = S(kGolfer + golfer * 0x100);                                   // [F+0x28] = golfer << 8 (0x00438620)
    const int member = (RB(Gr, 0x10) & 0xE0) == 0x20;                               // 0x004385fc..0x00438611 [F+0x20]
    int done = 0;                                                                   // 0x0043861c [F+0x34]
    G<int>(0x005AA55C) = 0;                                                         // 0x0043862b
    MemCpy(P(kBackup), R1, 0x230);                                                  // 0x00438642..0x00438648 (rep movsd, 0x8c)
    kStartSoundSlot(0x2D);                                                          // 0x0043860c / 0x0043864a
    kPlaySound((RB(R1, 0x21) & 0x80) ? 0x7D : 0x80, 0x64, 0, 0, 0);                // 0x0043864f..0x0043866d
    G<short>(kPreview + 0xAE) = static_cast<short>(character);                      // 0x00438682 (0x582e6e)
    const int r = kRandomRange(P(0x00822D9C), 0, 3);                                // 0x0043867d..0x0043868a
    G<unsigned char>(kPreview + 0xB2) = static_cast<unsigned char>(r);              // 0x00438697 (0x582e72)
    G<short>(kPreview + 0xB0) = static_cast<short>(static_cast<signed char>(RB(R1, 0x21)));   // 0x0043868f / 0x0043869c
    G<unsigned char>(kPreview + 0x10) = 0;                                          // 0x004386a3 (0x582dd0)
    G<unsigned char>(kPreview + 0x11) = RB(Gr, 0x11);                               // 0x004386aa / 0x004386b4 (0x582dd1)
    kImageCtor(f.img, 0);                                                           // 0x004386b0..0x004386ba
    kLoadImage(f.img, 0, I(S((RB(R1, 0x21) & 0x80) ? 0x004C8700 : 0x004C86E4)), 0, 0, 0x100, 2);   // 0x004386bf..0x004386e9
    G<int>(kMouseButton) = 0;                                                       // 0x004386ee

    for (;;) {
        // ---- redraw (0x004386f4)
        kBlitThunk(f.img, 0, kScreenSurface, 0, 0, 0, 0, 0x320, 0x258);             // 0x004386f4..0x00438719
        int x = 0, y = 0;
        kViewToLocal(kRoot, 0, I(&x), I(&y));                                      // 0x0043871e..0x0043872d (x [F+0x24], y [F+0x1c])
        int ctl = kNearestCharControl(x, y);                                        // 0x00438732..0x00438747
        if (ctl == 0xF && (RB(R1, 0x21) & 0x80) == 0) ctl = -1;                    // 0x00438744..0x0043875d
        int hot = -1;                                                               // 0x00438761 (ebp)
        if (ctl == -1 || ctl == 0x14) {                                             // 0x00438764..0x0043876c
            if (Abs(x - 0x9A) < 0x50 && Abs(y - 0x40) < 0x28) {                     // 0x004387a3..0x004387c5
                const int q = (y - 0x18) / WRONG(9, 0x14, 0x15);                    // 0x004387c7..0x004387d9 (/20, truncation)
                if (static_cast<unsigned>(q) <= 4) hot = q;                         // 0x004387db..0x00438800, table 0x0043a338
            }
        } else {
            kDrawScaled(S(0x0059DF20 + ctl * 0x58), 0, Target(), G<short>(0x004C7B90 + ctl * 4),
                        G<short>(0x004C7B92 + ctl * 4), 1, 1, 1, 0);                // 0x0043876e..0x0043879c
        }

        // ---- the figure (0x00438805)
        {
            char* const R = Rec(G<short>(kPreview + 0xAE));                         // 0x00438805..0x00438818
            const int hs = (RB(R, 0x23) >> 4) & 3;                                  // 0x0043881b / 0x0043882e..0x00438831
            const int body = static_cast<signed char>(RB(R, 0x22));                 // 0x00438827
            if (RB(R, 0x2C) & 8)                                                    // 0x00438821 / 0x00438834
                kImageDraw(P(0x005787A0), 0, Target(), 0x160, 0x74, 0);             // 0x00438838..0x0043884d
            kComposeGolferSprite(0x99);                                             // 0x00438852..0x00438857
            const int sheet = (RB(R, 0x2C) & 8) ? hs + 4 : hs;                      // 0x0043885c..0x0043886c
            const int t0 = Target();                                                // 0x0043886e
            const int a = sheet + kTypeBit7Clear(0x99) * 8;                         // 0x00438880..0x0043888a
            kImageDraw(S(0x005439E0 + a * 0x2C), 0, t0, 0x163, 0x74, 0x0081CA10);   // 0x00438890..0x0043889d
            const int t1 = Target();                                                // 0x004388a2
            if (body < 0x14) {                                                      // 0x004388a8 / 0x004388b0
                const int g = kTypeBit7Clear(0x99);                                 // 0x004388b2
                const int i = (body + g * 0x14) * 3 + kThoughtFlag(0x99);           // 0x004388b7..0x004388cd
                kDrawScaled(S(0x0059E7C0 + i * 0x2C), 0, t1, 0x150, 0x14, 1, 1, 1, 0);   // 0x004388cf..0x00438927
            } else {
                const int g = kTypeBit7Clear(0x99);                                 // 0x004388ed
                const int i = (body + g * 0x48) * 3 + kThoughtFlag(0x99);           // 0x004388f2..0x00438908
                kDrawScaled(S(0x005628E8 + i * 0x2C), 0, t1, 0x13C, 0x14, 1, 1, 1, 0);   // 0x0043890a..0x00438927
            }
        }
        {
            void* const disp = G<void*>(0x0083AD50);                                // 0x0043892c..0x00438934
            if (disp != 0) {
                typedef int(__fastcall * Slot3_t)(void*, void*, int, int, int);
                (*reinterpret_cast<Slot3_t*>(*static_cast<unsigned*>(disp) + 0x9C))(disp, 0, 1, 1, 1);   // 0x00438936..0x0043893e
            }
        }
        kImageDraw(P(0x0059E600), 0, Target(), 0x12B, 0xDC, 0);                     // 0x00438944..0x0043895c
        int set;                                                                    // esi (0x00438961..0x00438a3d)
        if (kTypeBit7Clear(0x99) == 0) {                                            // 0x00438961..0x00438970
            char* const R = Rec(G<short>(kPreview + 0xAE));                         // 0x00438972..0x0043898a
            set = 7;                                                                // 0x00438979
            const unsigned char b21 = RB(R, 0x21);                                  // 0x0043898d
            if (b21 & 1) set = 6;                                                   // 0x00438993..0x00438998
            if (RB(R, 0x20) & 8) set = 8;                                           // 0x0043899d..0x004389a6
            if (b21 & 4) set = 5;                                                   // 0x004389ab..0x004389b0
            if (RD(R, 0x2C) != 0) set = ((RB(R, 0x23) >> 4) & 3) + 5;               // 0x004389b5..0x004389d2
        } else {
            const unsigned char fl = G<unsigned char>(kPreview + 0x11);             // 0x004389d6
            if (fl & 4) set = ((~fl & 0xFF) & 2 | 4) >> 1;                          // 0x004389db..0x004389ed
            else set = 1;                                                           // 0x004389f1
            char* const R = Rec(G<short>(kPreview + 0xAE));                         // 0x004389f6..0x00438a09
            if (RD(R, 0x2C) != 0) set = (RB(R, 0x23) >> 4) & 3;                     // 0x00438a0c..0x00438a22
            if (G<unsigned char>(kPreview + 0x10) == 0x20)                          // 0x00438a24
                set = static_cast<signed char>(G<unsigned char>(0x0058DD70 + G<unsigned char>(kPreview + 0xB3) * 0x38));   // 0x00438a2d..0x00438a3d
        }
        int cell = kCellAt(set, 5, 0);                                              // 0x00438a45..0x00438a4a
        G<int>(0x005AA6C4) = cell;                                                  // 0x00438a6a
        kAddSprite(0x13B, 0xEF, 0xEF, 0x1E, 0x28, cell, 4, 0x199);                  // 0x00438a4f..0x00438a6f
        cell = kCellAt(set, 5, 4);                                                  // 0x00438a74..0x00438a79
        G<int>(0x005AA6C4) = cell;                                                  // 0x00438a99
        kAddSprite(0x14F, 0xEF, 0xEF, 0x1E, 0x28, cell, 4, 0x199);                  // 0x00438a7e..0x00438a9e

        // ---- names (0x00438aa3)
        kSetDrawTarget(P(Target()), 0, 0x00519928, 0, 0, 0);                        // 0x00438aa3..0x00438ab7
        kBuf[0] = 0;                                                                // 0x00438ac3
        kBuildGolferName(0x99, 0);                                                  // 0x00438abc..0x00438aca
        if (RB(Rec(G<short>(kPreview + 0xAE)), 0x2C) & 8) StrCat(kBuf, S(0x004C86DC));   // 0x00438acf..0x00438b1c
        kDrawCenteredText2(I(kBuf), 0x9A, 0x18, hot != 0 ? static_cast<int>(0x80000848) : static_cast<int>(0x80007B20));   // 0x00438b1e..0x00438b3b
        StrCpy(kBuf, S(0x004C59E4));                                                // 0x00438b40..0x00438b63
        StrCat(kBuf, Rec(G<short>(kPreview + 0xAE)));                               // 0x00438b65..0x00438ba7 (record + 0)
        StrCat(kBuf, S(0x004C59E0));                                                // 0x00438ba9..0x00438bf0
        kDrawCenteredText2(I(kBuf), 0x9A, 0x30, hot != 1 ? static_cast<int>(0x80000848)
                                                          : static_cast<int>(WRONG(10, 0x80007FFF, 0x80007FFE)));   // 0x00438bcf..0x00438bf2
        kSetDrawTarget(P(Target()), 0, 0x0051B360, 0, 0, 0);                        // 0x00438bf7..0x00438c0b
        {
            const unsigned char st = G<unsigned char>(kPreview + 0xB0);             // 0x00438c10 (dl, read once)
            kBuf[0] = 0;                                                            // 0x00438c16
            if (st & 8) StrCpy(kBuf, S(0x004C86D4));                                // 0x00438c1d..0x00438c45
            if (st & 0x10) StrCpy(kBuf, S(0x004C86CC));                             // 0x00438c47..0x00438c6f
            if (st & 0x20) StrCpy(kBuf, S(0x004C86C0));                             // 0x00438c71..0x00438c99
            if (st & 0x40) StrCpy(kBuf, S(0x004C86B8));                             // 0x00438c9b..0x00438cc3
        }
        if (hot == 2) kDrawCenteredText2(I(kBuf), 0x9A, 0x45, static_cast<int>(0x80007FFF));   // 0x00438cc5..0x00438cdb
        else kDrawCenteredText(I(kBuf), 0x9A, 0x45, static_cast<int>(0x80000848));  // 0x00438ce2..0x00438cf3
        {
            const unsigned char st = G<unsigned char>(kPreview + 0xB0);             // 0x00438cf8
            kBuf[0] = 0;                                                            // 0x00438d04
            if (st & 1) StrCpy(kBuf, S(0x004C86B0));                                // 0x00438d01..0x00438d30
            if (st & 2) StrCpy(kBuf, S(0x004C86A4));                                // 0x00438d32..0x00438d5a
            if (st & 4) StrCpy(kBuf, S(0x004C869C));                                // 0x00438d5c..0x00438d84
        }
        if (hot == 3) kDrawCenteredText2(I(kBuf), 0x9A, 0x5A, static_cast<int>(0x80007FFF));   // 0x00438d86..0x00438d9c
        else kDrawCenteredText(I(kBuf), 0x9A, 0x5A, static_cast<int>(0x80000848));  // 0x00438da3..0x00438db4

        // ---- the five flag choices of record + 0x20 (controls 3..7) and the three preview flags (controls 0..2)
        kSetDrawTarget(P(Target()), 0, 0x0051B360, 0, 0, 0);                        // 0x00438db9..0x00438dcd
        {
            const int flags = static_cast<signed char>(RB(Rec(G<short>(kPreview + 0xAE)), 0x20));   // 0x00438dd2..0x00438e01 [F+0x38]
            for (int k = 0; k < 5; k++) {                                           // 0x00438e05..0x00438ea8 (0x59e054 .. 0x59e20c)
                const int bit = (1 << k) & flags;                                   // 0x00438e09..0x00438e12
                if (bit)
                    kDrawScaled(S(0x0059E054 + k * 0x58), 0, Target(), G<short>(0x004C7B9C + k * 4),
                                G<short>(0x004C7B9E + k * 4), 1, 1, 1, 0);          // 0x00438e16..0x00438e3b
                StrCpy(kBuf, G<char*>(0x004C2864 + k * 4));                         // 0x00438e40..0x00438e6f
                kDrawCenteredText(I(kBuf), 0x5D, 0x7A + k * 0x1C, bit ? static_cast<int>(0x80000848)
                                                                      : static_cast<int>(0x80004210));   // 0x00438e65..0x00438e84
            }
        }
        kSetDrawTarget(P(Target()), 0, 0x00519928, 0, 0, 0);                        // 0x00438eae..0x00438ebf
        for (int k = 0; k < 3; k++) {                                               // 0x00438ed6..0x00438f91 (0x59df4c .. 0x59e054)
            const unsigned char b = static_cast<unsigned char>(1 << k);             // 0x00438edb..0x00438ee2
            if (b & G<unsigned char>(kPreview + 0x11))                              // 0x00438ed6 / 0x00438ee4
                kDrawScaled(S(0x0059DF4C + k * 0x58), 0, Target(), G<short>(0x004C7B90 + k * 4),
                            G<short>(0x004C7B92 + k * 4), 1, 1, 1, 0);              // 0x00438ee8..0x00438f0d
            const unsigned char fl = G<unsigned char>(kPreview + 0x11);             // 0x00438f1e
            StrCpy(kBuf, G<char*>(0x004C2858 + k * 4));                             // 0x00438f12..0x00438f3f
            kBuf[0] = static_cast<char>(kBuf[0] + 0xE0);                            // 0x00438f41..0x00438f50
            kDrawCenteredText(I(kBuf), 0xD4, 0x8E + k * 0x23, (b & fl) ? static_cast<int>(0x80000848)
                                                                       : static_cast<int>(0x80004210));   // 0x00438f2a..0x00438f6d
        }

        if (member) {                                                               // 0x00438f97..0x00438f9d
            // ---- the member's ten skills (0x00438fa3)
            kDrawCenteredText(0x004C8688, 0x249, 0x1E, static_cast<int>(0x80000000));   // 0x00438fa3..0x00438fb4
            kSetDrawTarget(P(Target()), 0, 0x0051B360, 0, 0, 0);                    // 0x00438fb9..0x00438fcd
            int yy = 0x3D;                                                          // 0x00438fdd (ebp)
            for (int k = 0; k < 10; k++, yy += 0x10) {                              // 0x00438fe6..0x0043913b (0x4c2c3c .. 0x4c2c64)
                const unsigned skill = character != 0 ? RB(Gr, 0xE8 + k)             // 0x00438fe6..0x00438ff5 (0x5795a8)
                                                      : G<unsigned char>(0x005A5A04 + k);   // 0x00438ffe
                const int bar = yy - 2;                                             // 0x00439004
                kFillWindowRect(kRoot, 0, 0x1F2, bar - 1, 0x2A, 0xE, static_cast<int>(0x80000000));   // 0x00439007..0x00439024
                kFillWindowRect(kRoot, 0, 0x1F3, bar, 0x28, 0xC,
                                WRONG(5, skill, !skill) ? static_cast<int>(0x800023E8) : static_cast<int>(0x80007D08));   // 0x00439029..0x0043904b
                if (skill) {                                                        // 0x00439050
                    StrCpy(kBuf, S(0x004C5340));                                    // 0x00439058..0x00439087
                    StrCat(kBuf, kItoa(static_cast<int>(skill) * 10, kItoaBuf, 10));   // 0x00439062..0x004390c7
                    StrCat(kBuf, S(0x004C60C4));                                    // 0x004390c9..0x004390f2
                    kDrawText(I(kBuf), 0x1F5, yy, static_cast<int>(0x80000000));    // 0x00439096..0x004390f4
                }
                kDrawText(G<int>(0x004C2C3C + k * 4), 0x222, yy, skill ? static_cast<int>(0x80000000)
                                                                       : static_cast<int>(0x80004210));   // 0x00439100..0x0043911a
            }
        } else {
            // ---- the character's type card (0x00439146)
            char* const T = S(kTypes + character * 0x2C);                           // 0x00439146..0x00439153
            int kind = RB(T, 2) & 7;                                                // 0x00439156 / 0x00439162
            if (RB(T, 0x29) == 0xFF) kind = -1;                                     // 0x0043915c..0x0043916b
            else if (kind > 3)                                                      // 0x00439170
                kFillWindowRect(kRoot, 0, 0x1F4, 0x1C, 0xA0, 0x11, static_cast<int>(0x80000210));   // 0x00439175..0x0043918d
            switch (kind + 1) {                                                     // 0x00439192..0x0043919e, table 0x0043a34c
            case 0: kDrawCenteredText(0x004C867C, 0x244, 0x1E, static_cast<int>(0x80006000)); break;   // 0x004391a5
            case 2: kDrawCenteredText(0x004C3AC0, 0x244, 0x1E, static_cast<int>(0x80006318)); break;   // 0x004391bd
            case 3: kDrawCenteredText(0x004C8674, 0x244, 0x1E, static_cast<int>(0x80004210)); break;   // 0x004391d5
            case 4: kDrawCenteredText(0x004C8664, 0x244, 0x1E, static_cast<int>(0x80002108)); break;   // 0x004391ed
            case 5: kDrawCenteredText(0x004C8658, 0x244, 0x1E, static_cast<int>(0x80007FF0)); break;   // 0x00439205
            case 6: kDrawCenteredText2(0x004C8648, 0x244, 0x1E, static_cast<int>(0x80007FFF)); break;  // 0x0043921d
            default: break;                                                         // case 1 (0x00439236) and ja
            }
            kSetDrawTarget(P(Target()), 0, 0x0051B360, 0, 0, 0);                    // 0x00439236..0x00439247
            StrCpy(kBuf, S(0x004C863C));                                            // 0x0043924c..0x00439277
            StrCat(kBuf, RB(T, 0) ? kItoa(RB(T, 0), kItoaBuf, 10) : S(0x004C52BC));   // 0x0043926c..0x004392cb
            kDrawCenteredText(I(kBuf), 0x244, 0x32, static_cast<int>(0x80002108));  // 0x004392a4..0x004392d2
            StrCpy(kBuf, S(0x004C8630));                                            // 0x004392d7..0x00439305
            {
                const signed char v = static_cast<signed char>(RB(T, 1));           // 0x004392fa
                StrCat(kBuf, v > 0 ? kItoa(v, kItoaBuf, 10) : S(0x004C52BC));       // 0x00439303..0x00439357 (jle)
            }
            kDrawCenteredText(I(kBuf), 0x244, 0x40, static_cast<int>(0x80002108));  // 0x00439330..0x0043935e
            StrCpy(kBuf, S(0x004C8620));                                            // 0x00439363..0x00439393
            {
                const short v = *reinterpret_cast<short*>(T + 0x2A);                // 0x00439386
                StrCat(kBuf, v != 0 ? kItoa(v, kItoaBuf, 10) : S(0x004C8618));      // 0x00439390..0x004393e5
            }
            kDrawCenteredText(I(kBuf), 0x244, 0x4E, static_cast<int>(0x80002108));  // 0x004393be..0x004393ec
            kDrawCenteredText(0x004C8614, 0x244, 0x62, ctl != 0x14 ? static_cast<int>(0x8000211F)
                                                                   : static_cast<int>(0x80007B20));   // 0x004393f1..0x00439411
            StrCpy(kBuf, R1 + 0x30);                                                // 0x00439416..0x0043944c
            kDrawMessageBuffer(0x1E, 0x1E4, 0x72, static_cast<int>(0x80000018));    // 0x0043941c..0x0043944e
        }

        // ---- the 20 comment lines (0x00439456)
        kSetDrawTarget(P(Target()), 0, 0x0051B360, 0, 0, 0);                        // 0x00439456..0x00439467
        {
            int yy = 0x10F;                                                         // 0x00439473 (ebp)
            const signed char* kinds = reinterpret_cast<const signed char*>(0x004C2D10);   // 0x00439478 (ebx)
            const unsigned* labels = reinterpret_cast<const unsigned*>(0x004C2CC0);  // 0x0043947d [F+0x18]
            char* line = S(kComments + character * 0x4E2);                          // 0x00439485..0x00439498 [F+0x14]
            for (;;) {
                int kind = *kinds;                                                  // 0x0043949c
                G<short>(kPreview + 0xA6) = 0;                                      // 0x004394ad (0x582e66)
                kBuf[0] = 0;                                                        // 0x004394bc
                kBuildGolferComment(kind, G<int>(0x00838DA8 + kind * 4) & 0x3FFF, 0, 0x99);   // 0x0043949f..0x004394c5
                if (G<unsigned>(0x0058B198) == 0x80006318u) G<unsigned>(0x0058B198) = 0x80006318u;   // 0x004394ca..0x004394dc
                StrCpy(kBuf, reinterpret_cast<const char*>(*labels));               // 0x004394e1..0x00439505
                StrCat(kBuf, S(0x004C5DB4));                                        // 0x00439507..0x00439539
                kDrawText(I(kBuf), 0x34, yy, G<int>(0x0058B198));                   // 0x0043952b..0x00439540
                unsigned colour = 0x80000848u;                                      // 0x00439557
                G<unsigned>(0x0058B198) = colour;                                   // 0x00439565
                StrCpy(kBuf, line);                                                 // 0x00439545..0x00439574
                if (kBuf[0] == 0) {                                                 // 0x00439576..0x0043957e
                    kind = *kinds;                                                  // 0x00439580
                    kBuildGolferComment(kind, G<int>(0x00838DA8 + kind * 4) & 0x3FFF, 0, 0x99);   // 0x00439583..0x00439599
                    colour = 0x80004210u;                                           // 0x0043959e
                    G<unsigned>(0x0058B198) = colour;                               // 0x004395a6
                }
                kDrawText(I(kBuf), 0x19C, yy, static_cast<int>(colour));            // 0x004395ab..0x004395b7
                yy += 0x10;                                                         // 0x004395ca
                line += 0x32;                                                       // 0x004395cd
                labels++;                                                           // 0x004395d0
                const signed char next = kinds[1];                                  // 0x004395c4
                kinds++;                                                            // 0x004395d3
                if (next == -1) break;                                              // 0x004395d4..0x004395de (cmp al, 0xff)
            }
        }

        // ---- the hover tip (0x004395e4)
        {
            const int last = G<int>(0x005A9F54);                                    // 0x004395e8
            G<short>(kPreview + 0xAE) = static_cast<short>(character);              // 0x004395ee / 0x004395f8
            if (ctl == last && ctl != -1) {                                         // 0x004395f6..0x00439608
                const int n = G<int>(0x005AA554) + 1;                               // 0x0043960e..0x00439614
                G<int>(0x005AA554) = n;                                             // 0x00439618
                if (n > WRONG(7, 10, 9)) {                                          // 0x00439615 / 0x0043961e
                    kBuf[0] = 0;                                                    // 0x00439627
                    const unsigned t = static_cast<unsigned>(ctl - 8);              // 0x00439624
                    if (t <= 0xC) {                                                 // 0x0043962e..0x00439637, table 0x0043a368
                        bool draw = true;
                        if (t < 12) StrCpy(kBuf, S(kTip[t]));                       // 0x0043963e..0x00439747, 0x00439756..0x00439774
                        else if (!member) StrCpy(kBuf, S(0x004C857C));              // 0x00439749..0x00439774
                        else draw = false;                                          // 0x0043974f
                        if (draw && kBuf[0] != 0) kDrawPanelBackdrop(x, y);         // 0x00439776..0x00439789
                    }
                }
            } else {
                G<int>(0x005AA554) = 0;                                             // 0x00439793
                G<int>(0x005A9F54) = ctl;                                           // 0x0043979d
            }
        }
        kRefreshFull(kRoot, 0, 0);                                                  // 0x004397a2..0x004397a9
        kWaitTicks(5, 0);                                                           // 0x004397ae..0x004397b2
        kPumpMessages();                                                            // 0x004397b7
        kWaitTicks(5, 0);                                                           // 0x004397bc..0x004397c0
        int btn = G<int>(kMouseButton);                                             // 0x004397c5 (edi)
        if (btn == 0) continue;                                                     // 0x004397ce..0x004397d0

        // ---- a press (0x004397d6)
        if (static_cast<unsigned>(ctl) <= 0x14) {                                   // 0x004397da..0x004397e3, table 0x0043a39c
            switch (ctl) {
            case 0: G<unsigned char>(kPreview + 0x11) ^= 1; break;                  // 0x004397ea
            case 1: G<unsigned char>(kPreview + 0x11) ^= 2; break;                  // 0x004397f6
            case 2: G<unsigned char>(kPreview + 0x11) ^= 4; break;                  // 0x00439802
            case 3: RB(R1, 0x20) ^= 1; break;                                       // 0x0043980e
            case 4: RB(R1, 0x20) ^= 2; break;                                       // 0x0043981e
            case 5: RB(R1, 0x20) ^= 4; break;                                       // 0x0043982e
            case 6: RB(R1, 0x20) ^= 8; break;                                       // 0x0043983e
            case 7: RB(R1, 0x20) ^= 0x10; break;                                    // 0x0043984e
            case 8: {                                                               // 0x0043985e: load a character
                StrCpy(kBuf, S(0x004C84E8));                                        // 0x0043985e..0x00439883
                StrCat(kBuf, S(kThemeName));                                        // 0x00439885..0x004398b2
                StrCat(kBuf, S(member ? 0x004C8574 : 0x004C856C));                  // 0x004398a9..0x004398f7
                int n = kListMatchingFiles(I(kBuf), 0x004E9A84, 0x004E9A84);        // 0x004398cd..0x004398fe
                if (n == 0) break;                                                  // 0x00439903..0x00439905
                StrCpy(kBuf, S(0x004C855C));                                        // 0x0043990b..0x00439930
                if (n > 0) {                                                        // 0x0043992e..0x00439932
                    char* name = kNames;                                            // 0x00439934
                    do {
                        StrCat(kBuf, name);                                         // 0x00439939..0x00439964
                        name += 0x64;                                               // 0x00439940
                        StrCat(kBuf, S(0x004C7C30));                                // 0x00439966..0x00439990
                    } while (--n != 0);                                             // 0x0043998f..0x00439992
                }
                const int pick = kScrollTextWindow(I(kBuf), 0x64, 0x14);            // 0x00439994..0x0043999d
                if (pick == -1) break;                                              // 0x004399a5..0x004399a8
                kLoadThemeFile(I(kNames + pick * 100), character, member ? golfer : -1);   // 0x004399ae..0x004399d9
                break;
            }
            case 9: {                                                               // 0x004399e6: save the character
                if (kPromptAndSaveFile(character, member ? golfer : -1, 0) == 0) break;   // 0x004399e6..0x00439a0f
                StrCpy(kBuf, S(0x004C8540));                                        // 0x00439a15..0x00439a38
                StrCat(kBuf, S(kThemeName));                                        // 0x00439a3a..0x00439a63
                StrCat(kBuf, S(0x004C84E4));                                        // 0x00439a65..0x00439a92
                StrCat(kBuf, R1 + 0x10);                                            // 0x00439a8b..0x00439ac0
                StrCat(kBuf, S(member ? 0x004C8530 : 0x004C8538));                  // 0x00439ac2..0x00439b01
                kShowPopup(0x12C, 0x64, 0, 1, 0);                                   // 0x00439adb..0x00439b03
                break;
            }
            case 10: {                                                              // 0x00439b0d: hair style, from the frame counter
                if (btn == 1) f.style = (f.style + WRONG(8, 1, 2)) % 4;             // 0x00439b12..0x00439b23 (C remainder)
                else f.style = (f.style + 3) % 4;                                   // 0x00439b29..0x00439b3d
                RB(R1, 0x23) = static_cast<unsigned char>((static_cast<unsigned char>(f.style) << 4) | (RB(R1, 0x23) & 0xF));   // 0x00439b41..0x00439b57
                RD(R1, 0x2C) |= 0x80;                                               // 0x00439b5d..0x00439b66
                break;
            }
            case 11: RD(R1, 0x2C) = (RD(R1, 0x2C) ^ 8) | 0x80; break;               // 0x00439b71..0x00439b81
            case 12: {                                                              // 0x00439b8c: body / outfit preset
                const int c = kPickCharEditor(kTypeBit7Clear(0x99));                // 0x00439b8c..0x00439b9c
                if (c == -1) break;                                                 // 0x00439ba1..0x00439ba4
                RB(R1, 0x22) = static_cast<unsigned char>(c);                       // 0x00439bb3
                if (c >= 0x13) break;                                               // 0x00439bae / 0x00439bb9 (signed)
                int idx = c;                                                        // 0x00439bb1
                if (kTypeBit7Clear(0x99) == 0) idx = c + 0x14;                      // 0x00439bbf..0x00439bd0
                RB(R1, 0x27) = G<unsigned char>(0x004D55E8 + idx * 0x44);           // 0x00439bd3..0x00439be9
                RB(R1, 0x28) = G<unsigned char>(0x004D55E8 + idx * 0x44 + WRONG(11, 3, 2));   // 0x00439be3 / 0x00439bef
                break;
            }
            case 13: {                                                              // 0x00439bfa: record + 0x24, 0..9
                const unsigned char b = RB(R1, 0x24);
                const int v = btn == 1 ? static_cast<signed char>(b + WRONG(2, 1, 2))   // 0x00439c03..0x00439c0b
                                       : static_cast<signed char>(b - 1) + 10;      // 0x00439c10..0x00439c1b
                RB(R1, 0x24) = static_cast<unsigned char>(v % 10);                  // 0x00439c1e..0x00439c26 (idiv)
                RD(R1, 0x2C) |= 0x80;                                               // 0x00439c2c..0x00439c34
                break;
            }
            case 14: {                                                              // 0x00439c3f: record + 0x25, 0..9
                const unsigned char b = RB(R1, 0x25);
                const int v = btn == 1 ? static_cast<signed char>(b + 1) : static_cast<signed char>(b - 1) + 10;   // 0x00439c48..0x00439c60
                RB(R1, 0x25) = static_cast<unsigned char>(v % 10);                  // 0x00439c63..0x00439c6b
                RD(R1, 0x2C) |= 0x80;                                               // 0x00439c71..0x00439c79
                break;
            }
            case 15: {                                                              // 0x00439c84: record + 0x28, 0..4
                const unsigned char b = RB(R1, 0x28);
                const int v = btn == 1 ? static_cast<signed char>(b + 1) : static_cast<signed char>(b - 1) + 5;    // 0x00439c8d..0x00439ca5
                RB(R1, 0x28) = static_cast<unsigned char>(v % 5);                   // 0x00439ca8..0x00439cb0
                RD(R1, 0x2C) |= 0x80;                                               // 0x00439cb6..0x00439cbe
                break;
            }
            case 16: {                                                              // 0x00439cc9: record + 0x27, 0..3
                const unsigned char b = RB(R1, 0x27);                               // 0x00439cd0
                const int v = btn == 1 ? static_cast<signed char>(b + 1) : static_cast<signed char>(b - 1) + 4;    // 0x00439cd8..0x00439cec
                RB(R1, 0x27) = static_cast<unsigned char>(v % 4);                   // 0x00439cdd..0x00439cfc (and / jns: C remainder)
                RD(R1, 0x2C) |= 0x80;                                               // 0x00439d02..0x00439d0b
                break;
            }
            case 17: {                                                              // 0x00439d16: gender
                RB(R1, 0x21) ^= 0x80;                                               // 0x00439d23..0x00439d2e
                RD(R1, 0x2C) |= 0x80;                                               // 0x00439d34..0x00439d3d
                const bool female = (RB(R1, 0x21) & 0x80) != 0;                     // 0x00439d43..0x00439d4c
                kLoadImage(f.img, 0, I(S(WRONG(1, female, !female) ? 0x004C8700 : 0x004C86E4)), 0, 0, 0x100, 2);   // 0x00439d4e..0x00439d6a
                break;
            }
            case 18:                                                                // 0x00439d74: cancel restores the entry copy
                MemCpy(R1, P(kBackup), 0x230);                                      // 0x00439d74..0x00439d88 (the editor stays open)
                if (NEG_ON(4)) done = 1;
                break;
            case 19:                                                                // 0x00439d8f: exit
                done = 1;                                                           // 0x00439d9d
                if (!NEG_ON(3)) RB(Gr, 0x11) = G<unsigned char>(kPreview + 0x11);   // 0x00439d8f..0x00439da5
                RD(R1, 0x2C) = (G<unsigned char>(kPreview + 0x11) & 7) | (RD(R1, 0x2C) & 0xFFFFFFF8u);   // 0x00439dab..0x00439dbf
                break;
            case 20: {                                                              // 0x00439dca: the bio entry
                if (member != 0) break;                                             // 0x00439dca..0x00439dd2
                kDrawPanelS2(0x1D8, 0x68, 0x13C, 0xD8, 1);                          // 0x00439dd8..0x00439deb
                kPixelRed(P(kScreenSurface), 0, 0x258, 0x78, 0);                    // 0x00439df3..0x00439e00
                kPixelGreen(P(kScreenSurface), 0, 0x258, 0x78, 0);                  // 0x00439e05..0x00439e12
                kPixelBlue(P(kScreenSurface), 0, 0x258, 0x78, 0);                   // 0x00439e17..0x00439e24
                const int col = kPaletteNearest(G<void*>(0x0083AD0C), 0, 0xFF, 0xFF, 0xFF, 0);   // 0x00439e29..0x00439e3f
                kEditCreate(P(kEditBox), 0, 0x1E0, 0x70, 0x12C, 0xC8, I(kRoot), 0); // 0x00439e44..0x00439e62
                kEditSetCaretActive(P(kEditBox), 0, 1);                             // 0x00439e67..0x00439e6e
                kEditResizeBuffer(P(kEditBox), 0, 0x1F4);                           // 0x00439e73..0x00439e7d
                char* const bio = R1 + 0x30;                                        // 0x00439e82..0x00439e8b (ebx)
                kEditSetText(P(kEditBox), 0, I(bio));                               // 0x00439e91..0x00439e92
                G<int>(kEditBox + 0x584) = col;                                     // 0x00439e9d (0x5aa4fc)
                G<int>(kEditBox + 0x590) = 1;                                       // 0x00439ea3 (0x5aa508)
                G<int>(kEditBox + 0x594) = 2;                                       // 0x00439ead (0x5aa50c)
                G<int>(kEditBox + 0x598) = 0;                                       // 0x00439eb7 (0x5aa510)
                G<int>(kEditBox + 0x59C) = 4;                                       // 0x00439ebd (0x5aa514)
                kWindowShow(P(kEditBox), 0, 0);                                     // 0x00439e97..0x00439ec7
                G<int>(kMouseButton) = 0;                                           // 0x00439ecc
                G<int>(kKeyWord) = 0;                                               // 0x00439ed2
                do {
                    kShowScreen();                                                  // 0x00439edd
                    kRedrawAll();                                                   // 0x00439ee2
                    kLeaveModal483d80();                                            // 0x00439ee7
                    kPumpMessages();                                                // 0x00439eec
                    kRefreshFull(kRoot, 0, 0);                                      // 0x00439ef1..0x00439ef7
                } while (G<int>(kMouseButton) == 0 && G<int>(kKeyWord) != 0xD);     // 0x00439efc..0x00439f0a
                bio[0] = 0;                                                         // 0x00439f0c..0x00439f10 (ebx is never 0)
                StrCat(bio, G<char*>(kEditBox + 0x574));                            // 0x00439f13..0x00439f3c (0x5aa4ec)
                kEditReleaseState(P(kEditBox), 0);                                  // 0x00439f3e..0x00439f43
                break;
            }
            }
        } else if (ctl == -1) {                                                     // 0x00439f4d..0x00439f50
            int yy = y;                                                             // 0x00439f5a (esi)
            if (Abs(x - 0x9A) < 0x50 && Abs(yy - 0x40) < 0x28) {                    // 0x00439f56..0x00439f7c
                const int q = (yy - 0x18) / 0x14;                                   // 0x00439f7e..0x00439f90
                switch (static_cast<unsigned>(q) <= 3 ? q : -1) {                   // 0x00439f92..0x00439f97, table 0x0043a3f0
                case 0:                                                             // 0x00439f9e: the name (record + 0x10)
                    kBuf[0] = 0;                                                    // 0x00439fac
                    kTextInputBox(0x004C8524, 0xC8, 0x20, 0x10);                    // 0x00439f9e..0x00439fb3
                    if (kBuf[0] != 0) StrCpy(R1 + 0x10, kBuf);                      // 0x00439fb8..0x00439fee
                    yy = y; btn = G<int>(kMouseButton);                             // 0x00439ff0 / 0x00439ff4
                    break;
                case 1:                                                             // 0x0043a0ba: the profession (record + 0)
                    kBuf[0] = 0;                                                    // 0x0043a0c8
                    kTextInputBox(0x004C8508, 0xC8, 0x20, 0x10);                    // 0x0043a0ba..0x0043a0cf
                    if (kBuf[0] != 0) StrCpy(R1, kBuf);                             // 0x0043a0d4..0x0043a109 -> 0x00439feb
                    yy = y; btn = G<int>(kMouseButton);                             // 0x00439ff0 / 0x00439ff4
                    break;
                case 2: {                                                           // 0x0043a10e: record + 0x21 bits 3..6
                    const unsigned char b = RB(R1, 0x21);                           // 0x0043a112
                    int m = b & 0x78;                                               // 0x0043a118..0x0043a11a
                    if (btn == 1) { m = (m & 0x3C) << 1; if (m == 0) m = 8; }       // 0x0043a11d..0x0043a129
                    else { m = (m >> 1) & 0x78; if (m == 0) m = 0x40; }             // 0x0043a130..0x0043a137
                    m |= b & 0x87;                                                  // 0x0043a13c..0x0043a142
                    RB(R1, 0x21) = static_cast<unsigned char>(m);                   // 0x0043a14e
                    G<unsigned short>(kPreview + 0xB0) = static_cast<unsigned short>((G<unsigned short>(kPreview + 0xB0) & 0xFF87) | m);   // 0x0043a144..0x0043a157
                    break;
                }
                case 3: {                                                           // 0x0043a163: record + 0x21 bits 0..2
                    const unsigned char b = RB(R1, 0x21);                           // 0x0043a167
                    int m = b & 7;                                                  // 0x0043a16d..0x0043a16f
                    if (btn == 1) { m = (m & 3) << 1; if (m == 0) m = btn; }        // 0x0043a172..0x0043a17e (edi = 1)
                    else { m = (m >> 1) & 7; if (m == 0) m = 4; }                   // 0x0043a182..0x0043a189
                    m |= b & 0xF8;                                                  // 0x0043a18e..0x0043a194
                    RB(R1, 0x21) = static_cast<unsigned char>(m);                   // 0x0043a1a0
                    G<unsigned short>(kPreview + 0xB0) = static_cast<unsigned short>((G<unsigned short>(kPreview + 0xB0) & 0xFFF8) | m);   // 0x0043a196..0x0043a1a9
                    break;
                }
                default: break;                                                     // 0x00439f95 (ja)
                }
            }
            // the comment lines (0x00439ffa)
            if (Abs(x - 0x242) < 0xB2 && yy >= 0x10E) {                             // 0x00439ffa..0x0043a01a
                const int row = (yy - 0x10E) / 0x10 + WRONG(6, 0, 1);               // 0x0043a027..0x0043a039 (truncation)
                char* const line = S(kComments + (character * 25 + row) * 0x32);    // 0x0043a035..0x0043a05b
                StrCpy(kBuf, line);                                                 // 0x0043a05b..0x0043a07f
                kTextInputBox(0x004C8518, 0x12C, (row + 0xF) * 0x10, 0x30);         // 0x0043a031..0x0043a081
                StrCpy(line, kBuf);                                                 // 0x0043a086..0x0043a0a9
                yy = y; btn = G<int>(kMouseButton);                                 // 0x0043a0ab / 0x0043a0af
                                                                                    // 0x0043a0b5 -> 0x0043a1bc
            }
            // the member's skill bars (0x0043a1bc)
            if (member && character != 0 && Abs(x - 0x208) < 0x19 && yy >= 0x3E && yy < 0xDE) {   // 0x0043a1bc..0x0043a1e6
                const int row = (yy - 0x40) / 0x10;                                 // 0x0043a1e8..0x0043a1f5 (truncation)
                unsigned char& s = RB(Gr, 0xE8 + row);                              // 0x0043a1fb (0x5795a8)
                if (btn == 1) {                                                     // 0x0043a1f8 / 0x0043a202
                    if (s < 0xA) s = static_cast<unsigned char>(s + 1);             // 0x0043a204..0x0043a20b (jae)
                    else kPlaySound(0x18, 0x64, 0, 0, 0);                           // 0x0043a223..0x0043a22d
                } else {
                    if (s > 0) s = static_cast<unsigned char>(s - 1);               // 0x0043a214..0x0043a21a (jbe)
                    else kPlaySound(0x18, 0x64, 0, 0, 0);                           // 0x0043a223..0x0043a22d
                }
            }
        }

        // ---- wait for the release (0x0043a235)
        do kPumpMessages(); while (G<int>(kMouseButton) != 0);                      // 0x0043a235..0x0043a241
        kWaitTicks(5, 0);                                                           // 0x0043a243..0x0043a247
        do kPumpMessages(); while (G<int>(kMouseButton) != 0);                      // 0x0043a24f..0x0043a25b
        if (done) break;                                                            // 0x0043a25d..0x0043a263
    }

    // ---- exit (0x0043a269)
    if ((RB(Gr, 0x10) & 0xE0) == 0x20 && *reinterpret_cast<short*>(Gr + 0xAE) != 0) {   // 0x0043a270..0x0043a287
        // 0x0043a289..0x0043a294: the byte at +0xb3 is zero-extended before `cmp ecx, -1`, so this test never fails
        char* const T2 = S(0x0058DD70 + RB(Gr, 0xB3) * 0x38);                       // 0x0043a296..0x0043a2a3
        T2[2] = static_cast<char>(RB(R1, 0x23) & 0xF);                              // 0x0043a2a6..0x0043a2af
        T2[1] = static_cast<char>(RB(R1, 0x27));                                    // 0x0043a2b5..0x0043a2bb
        T2[3] = static_cast<char>(RB(R1, WRONG(12, 0x24, 0x25)));                   // 0x0043a2c1..0x0043a2c7
        T2[4] = static_cast<char>(RB(R1, 0x25));                                    // 0x0043a2cd..0x0043a2d3
    }
    kStartSoundSlot((RB(R1, 0x21) & 0x80) ? 0x7D : 0x80);                           // 0x0043a2df..0x0043a2f3
    kPlaySound(0x2D, 0x64, 0, 0, 0);                                                // 0x0043a2f8..0x0043a302
    kEnterModal483d30();                                                            // 0x0043a30a
    return kImageDtor(f.img, 0);                                                    // 0x0043a30f..0x0043a31e (its eax is returned)
}
C2_t ShowCharacterEditor_orig;
// 0x004385d0
SG_HOOK("golf_clean.exe", 0x004385d0, showCharacterEditor, ShowCharacterEditor_re, ShowCharacterEditor_orig);
