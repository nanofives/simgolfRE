// C3 batch c3fa (round 33, 2026-10-13): golf_clean.exe size-only rows under 1 KB (log/c3/c3fa_candidates.tsv).
//
// Each body was written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites
// the instructions it reproduces. Registry: re/frida/registry.d/c3fa.py; fixtures: re/frida/js/fixtures.d/c3fa.js;
// Purposes and per-jcc tables: log/c3/c3fa_purpose.md; notes: log/c3/c3fa_notes.txt.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their original addresses, so a
// callee another batch hooks runs its reimplementation (the verifier re-runs the keys with those hooks off).
#include "hooks.h"
#include <string.h>

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
template <typename T> static inline T& F(void* p, int off) { return *reinterpret_cast<T*>(B(p) + off); }
static inline char* Text() { return reinterpret_cast<char*>(0x0051a068); }   // the shared text buffer

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int);
typedef int(__cdecl* C2_t)(int, int);
typedef int(__cdecl* C4_t)(int, int, int, int);
typedef int(__cdecl* C5_t)(int, int, int, int, int);
typedef int(__cdecl* C7_t)(int, int, int, int, int, int, int);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* T3_t)(void* ecx, void* edx, int a, int b, int c);
typedef int(__fastcall* T4_t)(void* ecx, void* edx, int a, int b, int c, int d);

}  // namespace

// =====================================================================================================================
// 0x004065c0  showCharacterIntro (cdecl, no arguments)
// strcpy(text, 0x004c4714) (0x004065c2..0x004065e7), strcat(text, 0x004c46bc) (0x004065e9..0x00406617); a counter starts
// at 10 (0x00406619) and gains 3 for every non-zero dword of the 22-entry table 0x004c15a0..0x004c19c0, stride 0x30
// (0x0040661e..0x0040662e); [0x004c2e08] = -1 (0x00406641); drawWindow 0x0040d320 (0xc8, 0x172, 0x80007fff, -2)
// (0x0040664b); skillPointDialog 0x0045f0f0 (0x004d6098, 0, counter, -1, 0xc8) (0x0040665f); its result is stored as a
// word at [0x004c2c9c] (0x00406667) and left in eax.
// =====================================================================================================================
C0_t ShowCharacterIntro_orig;
int __cdecl ShowCharacterIntro_re() {
    strcpy(Text(), reinterpret_cast<const char*>(0x004c4714));
    strcat(Text(), reinterpret_cast<const char*>(0x004c46bc));
    int lines = 10;
    for (unsigned p = 0x004c15a0; p < 0x004c19c0; p += 0x30)
        if (G<int>(p) != 0) lines += 3;
    G<int>(0x004c2e08) = -1;
    reinterpret_cast<C4_t>(0x0040d320)(0xc8, 0x172, static_cast<int>(0x80007fffu), -2);
    const int r = reinterpret_cast<C5_t>(0x0045f0f0)(0x004d6098, 0, lines, -1, 0xc8);
    G<short>(0x004c2c9c) = static_cast<short>(r);
    return r;
}

// =====================================================================================================================
// 0x00432560  showPreferencesMenu (cdecl, no arguments)
// strcpy(text, 0x004c7c34) (0x00432563..0x0043258b); for each of the six string pointers 0x004c2ca8..0x004c2cbc
// (0x0043258d, edx += 4, loop test 0x004325e3 / jl 0x004325eb): strcat(text, *p) then strcat(text, 0x004c7c30); then
// showPopup 0x0046d6e0 (0x190, 0xc8, [0x005a5a00], 0, 0) (0x00432600; the two zeros are eax, cleared by the last strcat's
// `xor eax, eax` 0x00432592) and its result is stored back into [0x005a5a00] (0x00432608) and returned.
// =====================================================================================================================
C0_t ShowPreferencesMenu_orig;
int __cdecl ShowPreferencesMenu_re() {
    strcpy(Text(), reinterpret_cast<const char*>(0x004c7c34));
    for (unsigned p = 0x004c2ca8; p < 0x004c2cc0; p += 4) {
        strcat(Text(), G<const char*>(p));
        strcat(Text(), reinterpret_cast<const char*>(0x004c7c30));
    }
    const int r = reinterpret_cast<C5_t>(0x0046d6e0)(0x190, 0xc8, G<int>(0x005a5a00), 0, 0);
    G<int>(0x005a5a00) = r;
    return r;
}

// =====================================================================================================================
// 0x00431ee0  saveThumbAndFull (cdecl, path)
// dot = strchr 0x004a6170 (path, '.') (0x00431eeb); if dot, *dot = 0 (0x00431ef9); strcat(path, 0x004c7838)
// (0x00431efd..0x00431f32); saveCourseJpg 0x00431d20 (path, [0x004c1570], 0, 0, 0x320, 0x258, 2) (0x00431f3c); if dot,
// *dot = 0 again (0x00431f48); strcat(path, 0x004c782c) (0x00431f4c..0x00431f81); saveCourseJpg (path, [0x004c1570], 0,
// 0, 0x320, 0x258, 10) (0x00431f8b), whose result stays in eax. Without a dot the second suffix is appended after the
// first one (nothing truncates the path).
// =====================================================================================================================
C1_t SaveThumbAndFull_orig;
int __cdecl SaveThumbAndFull_re(int pathArg) {
    char* path = reinterpret_cast<char*>(pathArg);
    char* dot = reinterpret_cast<char*>(reinterpret_cast<C2_t>(0x004a6170)(pathArg, '.'));
    if (dot) *dot = 0;
    strcat(path, reinterpret_cast<const char*>(0x004c7838));
    reinterpret_cast<C7_t>(0x00431d20)(pathArg, G<int>(0x004c1570), 0, 0, 0x320, 0x258, 2);
    if (dot) *dot = 0;
    strcat(path, reinterpret_cast<const char*>(0x004c782c));
    return reinterpret_cast<C7_t>(0x00431d20)(pathArg, G<int>(0x004c1570), 0, 0, 0x320, 0x258, 10);
}

// =====================================================================================================================
// 0x00435680  clickEmployeeView (cdecl, x, y)
// With [0x00561254] != 0 (0x00435685) it returns handleEmployeeAction 0x00436c00 (x, y) (0x00435693). Otherwise
// r = hitEmployeeSlot435570 0x00435570 (x, y) (0x004356a6). For r >= 0 (0x004356b0) with e = [0x005a9cf0 + 4r] != -1
// (0x004356b9): the record at 0x005794b8 + e * 0x100 (shl 8, 0x004356c1); when its word at +8 ([e*0x100 + 0x005794c0],
// 0x004356c4) is not -1, or [0x0053df54] > 0 (0x004356d3), [0x0053df54] = e + 1 (0x004356d8); then the camera
// [0x004c2ba0] / [0x004c2ba4] = record +0 / +4 shifted right (arithmetic) by 10 (0x004356ea..0x004356f6). Then by r:
// -3 [0x00567afc] = 4 (0x00435702); -4 [0x0059ca54] = [0x00561254] = 1 (0x0043571a / 0x0043571f); -5 [0x005aa6c8] -= 0x10
// when it is non-zero (0x00435734 / 0x00435739); -6 [0x005aa6c8] += 0x10 (0x00435747). Every path but the first returns -1.
// =====================================================================================================================
C2_t ClickEmployeeView_orig;
int __cdecl ClickEmployeeView_re(int x, int y) {
    if (G<int>(0x00561254) != 0) return reinterpret_cast<C2_t>(0x00436c00)(x, y);
    const int r = reinterpret_cast<C2_t>(0x00435570)(x, y);
    if (r >= 0) {
        const int e = G<int>(0x005a9cf0 + 4 * r);
        if (e != -1) {
            const unsigned rec = static_cast<unsigned>(e) << 8;
            if (G<int>(rec + 0x005794c0) != -1 || G<int>(0x0053df54) > 0) G<int>(0x0053df54) = e + 1;
            G<int>(0x004c2ba0) = G<int>(rec + 0x005794b8) >> 10;
            G<int>(0x004c2ba4) = G<int>(rec + 0x005794bc) >> 10;
        }
    }
    if (r == -3) {
        G<int>(0x00567afc) = 4;
    } else if (r == -4) {
        G<int>(0x0059ca54) = 1;
        G<int>(0x00561254) = 1;
    } else if (r == -5) {
        if (G<int>(0x005aa6c8) != 0) G<int>(0x005aa6c8) -= 0x10;
    } else if (r == -6) {
        G<int>(0x005aa6c8) += 0x10;
    }
    return -1;
}

// =====================================================================================================================
// 0x00436c00  handleEmployeeAction (cdecl, x, y)
// r = hitEmployee436b00 0x00436b00 (x, y) (0x00436c0a). -3: [0x00561254] = 0, [0x00567afc] = 4 (0x00436c17 / 0x00436c21);
// -2: [0x00561254] = 0, [0x00567afc] = 2 (0x00436c34 / 0x00436c3e). Otherwise a switch through the table 0x00436e2c for
// r in 0..6 (unsigned test `ja` 0x00436c54), sel = [0x004c7aec], list 0x005aa5b8 (walker indices), walker records of
// 0x4c bytes at 0x00585850 (index * 19 * 4):
//   0, 1 (0x00436c61): sel = -1, [0x0059ca54] = 0.
//   2 (0x00436c76): if [0x005aa558] != 0: sel = -1, [0x005aa558] -= 2.      3 (0x00436c96): sel = -1, [0x005aa558] += 2.
//   4 (0x00436cae): if [0x0059ca54] != 0 and sel != -1: [0x004c2e10] = list[sel].
//   6 (0x00436cda): same guards; g = list[sel]; text[0] = 0 (0x00436cfd); pickGolferName 0x00467600 (g) (0x00436d04);
//      textInputBox 0x0045b2c0 (0x004c837c, 0xc8, 0x1b0, 0x1f) (0x00436d1a); strcpy(0x0058587a + g * 0x4c, text)
//      (0x00436d1f..0x00436d50).
//   5 (0x00436d56): same guards; g = list[sel]; [0x00571fd4] -= 0x19 (0x00436d84); byte [rec + 0x12] = 0 (0x00436d90);
//      pointsPopup 0x0040c890 (-0x19, rec +0, rec +4, -1) (0x00436da7; the -1 is ebx pushed at 0x00436d7d);
//      [0x004c2e10] = -1 (0x00436dbe); the word at 0x0058421a + 20 * (sign-extended word [0x005a6d3c] % 100) gets -0x19
//      (0x00436dc8).
// Any other r (0x00436dd5): r < 8 (signed, 0x00436dd8) returns; else sel = r - 8 (0x00436de5); sel >= [0x005aa6c0]
// (signed, 0x00436dea) sets sel = -1; otherwise the camera [0x004c2ba0] / [0x004c2ba4] = walker list[sel]'s +0 / +4 >> 10
// (0x00436e13..0x00436e1f). Every path returns -1.
// =====================================================================================================================
C2_t HandleEmployeeAction_orig;
int __cdecl HandleEmployeeAction_re(int x, int y) {
    const int r = reinterpret_cast<C2_t>(0x00436b00)(x, y);
    if (r == -3) { G<int>(0x00561254) = 0; G<int>(0x00567afc) = 4; return -1; }
    if (r == -2) { G<int>(0x00561254) = 0; G<int>(0x00567afc) = 2; return -1; }
    int& sel = G<int>(0x004c7aec);
    if (static_cast<unsigned>(r) <= 6) {
        switch (r) {
        case 0:
        case 1:
            sel = -1;
            G<int>(0x0059ca54) = 0;
            return -1;
        case 2:
            if (G<int>(0x005aa558) == 0) return -1;
            sel = -1;
            G<int>(0x005aa558) -= 2;
            return -1;
        case 3:
            sel = -1;
            G<int>(0x005aa558) += 2;
            return -1;
        }
        if (G<int>(0x0059ca54) == 0 || sel == -1) return -1;
        const int g = G<int>(0x005aa5b8 + 4 * sel);
        if (r == 4) {
            G<int>(0x004c2e10) = g;
        } else if (r == 6) {
            Text()[0] = 0;
            reinterpret_cast<C1_t>(0x00467600)(g);
            reinterpret_cast<C4_t>(0x0045b2c0)(0x004c837c, 0xc8, 0x1b0, 0x1f);
            strcpy(reinterpret_cast<char*>(0x0058587a + g * 0x4c), Text());
        } else {   // r == 5
            const unsigned rec = 0x00585850u + static_cast<unsigned>(g) * 0x4c;
            G<int>(0x00571fd4) -= 0x19;
            G<unsigned char>(rec + 0x12) = 0;
            reinterpret_cast<C4_t>(0x0040c890)(-0x19, G<int>(rec), G<int>(rec + 4), -1);
            const int t = G<short>(0x005a6d3c);
            G<int>(0x004c2e10) = -1;
            G<short>(0x0058421a + (t % 100) * 20) += -0x19;
        }
        return -1;
    }
    if (r < 8) return -1;
    sel = r - 8;
    if (sel >= G<int>(0x005aa6c0)) { sel = -1; return -1; }
    const unsigned rec = 0x00585850u + static_cast<unsigned>(G<int>(0x005aa5b8 + 4 * sel)) * 0x4c;
    G<int>(0x004c2ba0) = G<int>(rec) >> 10;
    G<int>(0x004c2ba4) = G<int>(rec + 4) >> 10;
    return -1;
}

// =====================================================================================================================
// 0x00436060  clickGolferPanel (cdecl, x, y)
// With [0x00561254] != 0 (0x00436068) it returns handleEmployeeAction 0x00436c00 (x, y) (0x00436076). Otherwise
// s = hitGolferPanel435f00 0x00435f00 (x, y) (0x0043608c); n = [0x005685f0], f = [0x0059e7b8]; three Image_draw
// 0x00473e60 calls (this, [0x004c1570], word x, word y, 0), each followed by a re-read of f:
//   0x00583b88 at (0x004c7ac4, 0x004c7ac6) when n <= 1 (0x004360a2) or f & 0x4200000 (0x004360ae)   (0x004360cd)
//   0x00583c38 at (0x004c7ac8, 0x004c7aca) unless f & 0x2000 (0x004360dd), n > 1 (0x004360e5), [0x004c2e14] != -1
//              (0x004360ed) and !(f & 0x4200000) (0x004360f4) all hold                                   (0x00436113)
//   0x00583ce8 at (0x004c7acc, 0x004c7ace) unless f & 0x400000 (0x00436122) and !(f & 0x4200000) (0x00436129) (0x00436148)
// Then a switch on s through the table 0x004362b8 (s unsigned > 10 returns -1, 0x00436155):
//   0 (0x00436162): byte [0x00582cd0] = 0x20, word [0x00582d6e] = 0, byte [0x00582cd1] = [0x004d60b4] & 7, or 7 when that
//      dword is 0 (0x0043616e); showCharacterEditor 0x004385d0 (0, 0x98).
//   1 (0x004361b3): with n > 1, !(f & 0x4200000), [0x005a59f8] == -1: f = (f & ~0x2000) | 0x4000 stored (0x004361dd).
//   2 (0x004361e7): with f & 0x2000, n > 1, [0x004c2e14] != -1, [0x005a59f8] == -1, !(f & 0x4200000): f | 0x4000 stored.
//   3 (0x0043622d): with f & 0x400000, !(f & 0x4200000), [0x005a59f8] == -1: returns 0x4a (0x00436245).
//   4..8: [0x0058f330] = 0, -1, 1, 3, 4 (0x00436250 .. 0x00436288).   9: [0x00567afc] = 2 (0x00436298).
//   10: [0x0059ca54] = [0x00561254] = 1 (0x004362a4 / 0x004362aa).
// Every other path returns -1 (ebp, 0x004360d7).
// =====================================================================================================================
C2_t ClickGolferPanel_orig;
static void GolferPanelDraw(unsigned img, unsigned px, unsigned py) {
    reinterpret_cast<T4_t>(0x00473e60)(reinterpret_cast<void*>(img), 0, G<int>(0x004c1570), G<short>(px), G<short>(py), 0);
}
int __cdecl ClickGolferPanel_re(int x, int y) {
    if (G<int>(0x00561254) != 0) return reinterpret_cast<C2_t>(0x00436c00)(x, y);
    const unsigned s = static_cast<unsigned>(reinterpret_cast<C2_t>(0x00435f00)(x, y));
    unsigned f = G<unsigned>(0x0059e7b8);
    if (G<int>(0x005685f0) <= 1 || (f & 0x4200000u) != 0) {
        GolferPanelDraw(0x00583b88, 0x004c7ac4, 0x004c7ac6);
        f = G<unsigned>(0x0059e7b8);
    }
    if (!((f & 0x2000u) && G<int>(0x005685f0) > 1 && G<int>(0x004c2e14) != -1 && (f & 0x4200000u) == 0)) {
        GolferPanelDraw(0x00583c38, 0x004c7ac8, 0x004c7aca);
        f = G<unsigned>(0x0059e7b8);
    }
    if (!((f & 0x400000u) && (f & 0x4200000u) == 0)) {
        GolferPanelDraw(0x00583ce8, 0x004c7acc, 0x004c7ace);
        f = G<unsigned>(0x0059e7b8);
    }
    switch (s) {
    case 0: {
        const int t = G<int>(0x004d60b4);
        G<unsigned char>(0x00582cd0) = 0x20;
        G<short>(0x00582d6e) = 0;
        G<unsigned char>(0x00582cd1) = t ? static_cast<unsigned char>(t & 7) : 7;
        reinterpret_cast<C2_t>(0x004385d0)(0, 0x98);
        return -1;
    }
    case 1:
        if (G<int>(0x005685f0) > 1 && (f & 0x4200000u) == 0 && G<int>(0x005a59f8) == -1)
            G<unsigned>(0x0059e7b8) = (f & ~0x2000u) | 0x4000u;
        return -1;
    case 2:
        if ((f & 0x2000u) && G<int>(0x005685f0) > 1 && G<int>(0x004c2e14) != -1 && G<int>(0x005a59f8) == -1 &&
            (f & 0x4200000u) == 0)
            G<unsigned>(0x0059e7b8) = f | 0x4000u;
        return -1;
    case 3:
        if ((f & 0x400000u) && (f & 0x4200000u) == 0 && G<int>(0x005a59f8) == -1) return 0x4a;
        return -1;
    case 4: G<int>(0x0058f330) = 0; return -1;
    case 5: G<int>(0x0058f330) = -1; return -1;
    case 6: G<int>(0x0058f330) = 1; return -1;
    case 7: G<int>(0x0058f330) = 3; return -1;
    case 8: G<int>(0x0058f330) = 4; return -1;
    case 9: G<int>(0x00567afc) = 2; return -1;
    case 10: G<int>(0x0059ca54) = 1; G<int>(0x00561254) = 1; return -1;
    }
    return -1;
}

// =====================================================================================================================
// 0x004a08a0  panelDispatch (thiscall, kind, a, b, c, d; ret 0x14)
// v = kind & 0x1f is stored at this+0x1f4 (0x004a08a7); v - 1 above 15 (unsigned, 0x004a08b1) returns 3 (0x004a0970).
// Otherwise the byte table 0x004a0990 picks a case of the jump table 0x004a0978: v 1 optionDialogBuild 0x0049ed20 on
// this+0x60c, v 2 ListBox::build 0x0048a5d0 on this+0x5d4, v 4 panelCreate4a2400 0x004a2400 on this+0x770, v 8
// panelBuild4a3880 0x004a3880 on this+0x6e8, v 16 panelBuildRow 0x0049d6d0 on this+0x5f4, every other v returns 3. Each
// call passes (a, b, c, d) (0x004a08c6..0x004a08d9) and its result is returned.
// =====================================================================================================================
typedef int(__fastcall* T5_t)(void* ecx, void* edx, int k, int a, int b, int c, int d);
T5_t PanelDispatch5_orig;
int __fastcall PanelDispatch_re(void* self, void*, int kind, int a, int b, int c, int d) {
    const int v = kind & 0x1f;
    F<int>(self, 0x1f4) = v;
    unsigned fn, off;
    switch (v) {
    case 1: fn = 0x0049ed20; off = 0x60c; break;
    case 2: fn = 0x0048a5d0; off = 0x5d4; break;
    case 4: fn = 0x004a2400; off = 0x770; break;
    case 8: fn = 0x004a3880; off = 0x6e8; break;
    case 16: fn = 0x0049d6d0; off = 0x5f4; break;
    default: return 3;
    }
    return reinterpret_cast<T4_t>(fn)(B(self) + off, 0, a, b, c, d);
}

// =====================================================================================================================
// 0x00482570  decodeImageChunks (thiscall, chunk, image, dst; ret 0xc)
// t = word [chunk+4] (0x0048257a). Leaf chunks call a decoder (this, chunk, image, dst ? dst : image + 8) and return its
// result: t 7 decodeChunk7WordDelta 0x004827d0, 12 the `xor eax, eax; ret 0xc` stub 0x00487cb0, 13 Surface::clear
// 0x00482940 (0x004825f1), 15 Surface::blitRLE 0x00482990 (0x004826aa), 16 Surface::copyRaw 0x00482a80 (0x0048268a);
// t 4 and 11 call Palette::decodeChunk 0x004826f0 (this, chunk) (0x004825a2; table 0x004826cc). t 0xf1fa (0x0048261d) is a
// frame: for each of word [chunk+6] sub-chunks starting at chunk+0x10, each advancing by its own dword size (0x00482673):
// when image+0x34 is non-zero, image+0x3c is 0 and dst is 0 (0x00482637..0x00482648), Surface_copyFrom 0x004744c0 on
// image+8 from the record image+0x34 + 0x2c * [image+0x40] (0x0048264a..0x0048265a); otherwise the sub-chunk is decoded by
// a recursive call (0x0048266c). A frame returns 0, as does every other type (0x00482683).
// =====================================================================================================================
T3_t DecodeImageChunks_orig;
int __fastcall DecodeImageChunks_re(void* self, void*, int chunkArg, int imageArg, int dst) {
    char* chunk = reinterpret_cast<char*>(chunkArg);
    char* image = reinterpret_cast<char*>(imageArg);
    const unsigned t = F<unsigned short>(chunk, 4);
    const int d = dst ? dst : imageArg + 8;
    switch (t) {
    case 4:
    case 11: return reinterpret_cast<T1_t>(0x004826f0)(self, 0, chunkArg);
    case 7: return reinterpret_cast<T3_t>(0x004827d0)(self, 0, chunkArg, imageArg, d);
    case 12: return reinterpret_cast<T3_t>(0x00487cb0)(self, 0, chunkArg, imageArg, d);
    case 13: return reinterpret_cast<T3_t>(0x00482940)(self, 0, chunkArg, imageArg, d);
    case 15: return reinterpret_cast<T3_t>(0x00482990)(self, 0, chunkArg, imageArg, d);
    case 16: return reinterpret_cast<T3_t>(0x00482a80)(self, 0, chunkArg, imageArg, d);
    case 0xf1fa: {
        char* sub = chunk + 0x10;
        for (unsigned i = 0; i < F<unsigned short>(chunk, 6); ++i) {
            const int recs = F<int>(image, 0x34);
            if (recs != 0 && F<int>(image, 0x3c) == 0 && dst == 0)
                reinterpret_cast<T1_t>(0x004744c0)(image + 8, 0, recs + F<int>(image, 0x40) * 0x2c);
            else
                reinterpret_cast<T3_t>(0x00482570)(self, 0, reinterpret_cast<int>(sub), imageArg, dst);
            sub += F<int>(sub, 0);
        }
        return 0;
    }
    }
    return 0;
}

// =====================================================================================================================
// 0x004855b0  engineInit (cdecl, module, hwnd, flags)
// [0x0083afd0] = flags (0x004855bc); [0x0083ad50] = loadGraphsyObject 0x004a00a0 (module) (0x004855c2 / 0x004855ca); a NULL
// object returns 0x19 (0x004855d5). Then, one flag bit at a time, each step returning its callee's non-zero result:
//   0x8000 (0x004855e0): r = display slot +4 (0x0083a4f8, hwnd, 0x0083a7b8) (0x004855f4); r != 0: engineShutdown 0x00485740
//          (0x004855fd), return r (this step falls through on 0)
//   0x1    resetViewState47cfd0 0x0047cfd0 (hwnd, 1) (0x00485610)
//   0x2    resetScreenState 0x00483320 (0) (0x00485627), [0x0083ad0c] = 0x0083acb0 (0x00485634), Cache483030::attach
//          0x00483030 and Cache483060::detach 0x00483060 on 0x0083acb0 (0x0048563e / 0x00485648); no result test
//   0x4    jpeg_mem_init 0x004b04b0 (0x00485652)          0x8    loadFonts 0x0049fe50 (0x00485664)
//   0x10   jpeg_mem_init 0x004b04b0 again (0x00485676)    0x20   loadCursors 0x0049e9d0 (0x00485688)
//   0x80   setPrimarySurface 0x00483ac0 (0x0083ad20) (0x0048569f); on 0: setButtonDefaults 0x004889f0 (0x0083ad20, 0, 0)
//          (0x004856b6), [0x0083ab68] = [0x0083ad44] (0x004856c3)
//   0x100  MsgBox::initDefaults 0x00490bf0 (0x004856cd)   0x200  loadFileWinResource 0x0049d3b0 (0x004856db)
//   0x400  loadPcxSurface 0x0049d1b0 (0x004856e9)        0x4000 initSinTable 0x00491c10 (0x004856f7), result ignored
//   0x800  cacheCursorMetrics 0x004884e0 (0x00485701)
//   0x2000 strBufReset 0x0049d090 on 0x008406e8 (0x8000) (0x00485719): non-zero returns 1 (0x00485724)
//   0x1000 Timer::lockInc 0x00486fe0 (0x00485730)
// and 0 at the end (0x00485739).
// =====================================================================================================================
typedef int(__cdecl* C3_t)(int, int, int);
C3_t EngineInit_orig;
int __cdecl EngineInit_re(int module, int hwnd, int flags) {
    G<int>(0x0083afd0) = flags;
    const int disp = reinterpret_cast<C1_t>(0x004a00a0)(module);
    G<int>(0x0083ad50) = disp;
    if (disp == 0) return 0x19;
    int r;
    if (flags & 0x8000) {
        void* d = reinterpret_cast<void*>(disp);
        r = (*reinterpret_cast<T3_t*>(*reinterpret_cast<char**>(d) + 4))(d, 0, 0x0083a4f8, hwnd, 0x0083a7b8);
        if (r != 0) { reinterpret_cast<C0_t>(0x00485740)(); return r; }
    }
    if ((flags & 1) && (r = reinterpret_cast<C2_t>(0x0047cfd0)(hwnd, 1)) != 0) return r;
    if (flags & 2) {
        reinterpret_cast<C1_t>(0x00483320)(0);
        G<int>(0x0083ad0c) = 0x0083acb0;
        typedef int(__fastcall * T0_t)(void*, void*);
        reinterpret_cast<T0_t>(0x00483030)(reinterpret_cast<void*>(0x0083acb0), 0);
        reinterpret_cast<T0_t>(0x00483060)(reinterpret_cast<void*>(0x0083acb0), 0);
    }
    if ((flags & 4) && (r = reinterpret_cast<C0_t>(0x004b04b0)()) != 0) return r;
    if ((flags & 8) && (r = reinterpret_cast<C0_t>(0x0049fe50)()) != 0) return r;
    if ((flags & 0x10) && (r = reinterpret_cast<C0_t>(0x004b04b0)()) != 0) return r;
    if ((flags & 0x20) && (r = reinterpret_cast<C0_t>(0x0049e9d0)()) != 0) return r;
    if (flags & 0x80) {
        if ((r = reinterpret_cast<C1_t>(0x00483ac0)(0x0083ad20)) != 0) return r;
        reinterpret_cast<C3_t>(0x004889f0)(0x0083ad20, 0, 0);
        G<int>(0x0083ab68) = G<int>(0x0083ad44);
    }
    if ((flags & 0x100) && (r = reinterpret_cast<C0_t>(0x00490bf0)()) != 0) return r;
    if ((flags & 0x200) && (r = reinterpret_cast<C0_t>(0x0049d3b0)()) != 0) return r;
    if ((flags & 0x400) && (r = reinterpret_cast<C0_t>(0x0049d1b0)()) != 0) return r;
    if (flags & 0x4000) reinterpret_cast<C0_t>(0x00491c10)();
    if ((flags & 0x800) && (r = reinterpret_cast<C0_t>(0x004884e0)()) != 0) return r;
    if ((flags & 0x2000) && reinterpret_cast<T1_t>(0x0049d090)(reinterpret_cast<void*>(0x008406e8), 0, 0x8000) != 0)
        return 1;
    if ((flags & 0x1000) && (r = reinterpret_cast<C0_t>(0x00486fe0)()) != 0) return r;
    return 0;
}

// =====================================================================================================================
// 0x00496030  buildLabeledControl (thiscall, no arguments; returns this)
// View4804a0::ctor 0x004804a0 on this (0x0049604e); ctorObj4a13f0 0x004a13f0 on this+0x60c and this+0xcdc (0x0049605f /
// 0x0049606f); vtables [this] = 0x004bbac8, [this+0x274] = 0x004bbab4 (0x00496074 / 0x0049607a); the style fields are
// copied from the globals at 0x0083ff1c.. and 0x004e4764.. (0x00496084..0x00496132), twelve dwords from 0x0083ff24 (four
// groups of three, 0x00496161..0x0049618f) to +0x5dc / +0x5e8 / +0x5f4 / +0x600, +0x5d4 / +0x5d8 from 0x0083ff58 /
// 0x0083ff5c, and +0x13ac / +0x13b0 = 0 (0x004961ac / 0x004961b2).
// =====================================================================================================================
typedef void*(__fastcall* P0_t)(void* ecx, void* edx);
P0_t BuildLabeledControl_orig;
void* __fastcall BuildLabeledControl_re(void* self, void*) {
    reinterpret_cast<P0_t>(0x004804a0)(self, 0);
    reinterpret_cast<P0_t>(0x004a13f0)(B(self) + 0x60c, 0);
    reinterpret_cast<P0_t>(0x004a13f0)(B(self) + 0xcdc, 0);
    F<unsigned>(self, 0) = 0x004bbac8u;
    F<unsigned>(self, 0x274) = 0x004bbab4u;
    F<int>(self, 0x574) = G<int>(0x0083ff1c);
    F<int>(self, 0x57c) = G<int>(0x004e4770);
    F<int>(self, 0x588) = 0;
    F<int>(self, 0x580) = G<int>(0x0083ff20);
    F<int>(self, 0x584) = G<int>(0x004e4774);
    F<int>(self, 0x58c) = G<int>(0x0083ff20);
    F<int>(self, 0x590) = G<int>(0x004e476c);
    F<int>(self, 0x59c) = -1;
    F<int>(self, 0x598) = 0;
    F<int>(self, 0x594) = G<int>(0x004e4768);
    F<int>(self, 0x5a0) = G<int>(0x004e4764);
    F<int>(self, 0x5a4) = 0;
    F<int>(self, 0x5ac) = 0;
    F<int>(self, 0x5b0) = 0;
    F<int>(self, 0x5b4) = 0;
    F<int>(self, 0x5b8) = 0;
    F<int>(self, 0x5a8) = G<int>(0x004e4778);
    F<int>(self, 0x5bc) = G<int>(0x004e477c);
    F<int>(self, 0x5c4) = G<int>(0x004e4780);
    F<int>(self, 0x5c8) = G<int>(0x004e4784);
    F<int>(self, 0x5cc) = G<int>(0x004e4788);
    F<int>(self, 0x5d0) = G<int>(0x004e478c);
    for (int i = 0; i < 3; ++i) {
        F<int>(self, 0x5dc + 4 * i) = G<int>(0x0083ff24 + 4 * i);
        F<int>(self, 0x5e8 + 4 * i) = G<int>(0x0083ff30 + 4 * i);
        F<int>(self, 0x5f4 + 4 * i) = G<int>(0x0083ff3c + 4 * i);
        F<int>(self, 0x600 + 4 * i) = G<int>(0x0083ff48 + 4 * i);
    }
    F<int>(self, 0x5d4) = G<int>(0x0083ff58);
    F<int>(self, 0x5d8) = G<int>(0x0083ff5c);
    F<int>(self, 0x13ac) = 0;
    F<int>(self, 0x13b0) = 0;
    return self;
}

// =====================================================================================================================
// 0x00480b30  Window::createBackingSurface (thiscall, w, h, keep, a3; ret 0x10)
// Virtual slot +0x54 (&w, &h, &keep) (0x00480b4a) may rewrite the three arguments. keep != 0 (0x00480b58): w and h are
// raised to at least 1 (0x00480b5a..0x00480b6a). keep == 0: Window::adjustSizeOuter 0x0047ca10 (&lw, &lh) with both
// locals 1 (0x00480b7c / 0x00480b80 / 0x00480b84), w and h raised to lw / lh (0x00480b91 / 0x00480ba1), then
// Window::innerSize 0x0047cb10 (&w, &h) (0x00480bbd). [0x00839ab0] = 1 around Surface_createFull 0x00474dd0 on this+0x274
// (w, h, a3, 0, bit 31 of [this+0x9c] as 8, 0) (0x00480bc6 / 0x00480bee / 0x00480bf5); its non-zero result is returned
// (0x00480bff), otherwise Window::setSize 0x0047b4e0 (w, h, 1, 0)'s (0x00480c10).
// =====================================================================================================================
typedef int(__fastcall* T2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* T6_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f);
T4_t CreateBackingSurface_orig;
int __fastcall CreateBackingSurface_re(void* self, void*, int w, int h, int keep, int a3) {
    (*reinterpret_cast<T3_t*>(*reinterpret_cast<char**>(self) + 0x54))(self, 0, reinterpret_cast<int>(&w),
                                                                         reinterpret_cast<int>(&h), reinterpret_cast<int>(&keep));
    if (keep != 0) {
        if (w <= 1) w = 1;
        if (h <= 1) h = 1;
    } else {
        int lw = 1, lh = 1;
        reinterpret_cast<T2_t>(0x0047ca10)(self, 0, reinterpret_cast<int>(&lw), reinterpret_cast<int>(&lh));
        if (lw > w) w = lw;
        if (lh > h) h = lh;
    }
    if (keep == 0) reinterpret_cast<T2_t>(0x0047cb10)(self, 0, reinterpret_cast<int>(&w), reinterpret_cast<int>(&h));
    G<int>(0x00839ab0) = 1;
    const int flag = static_cast<int>((F<unsigned>(self, 0x9c) >> 28) & 8);
    const int r = reinterpret_cast<T6_t>(0x00474dd0)(B(self) + 0x274, 0, w, h, a3, 0, flag, 0);
    G<int>(0x00839ab0) = 0;
    if (r != 0) return r;
    return reinterpret_cast<T4_t>(0x0047b4e0)(self, 0, w, h, 1, 0);
}

// =====================================================================================================================
// 0x00431d20  saveCourseJpg (cdecl, path, surf, x, y, w, h, scale)
// Returns at once (eax as noted) when path, surf, w, h or scale is 0 (0x00431d2a..0x00431d5a), when x, y, w, h or scale
// is negative (0x00431d66..0x00431d8a) or scale > 20 (0x00431d93). The inner object o = [surf+4] gives width (+0xd8),
// height (+0xdc), pitch (+0xe0, stored at [0x0056125c]) and a format record (+0xe4), each 0 without o (0x00431d99..
// 0x00431df4); the format must be {0x10, 0} (0x00431df6 / 0x00431e04, a NULL record faults there) and x < width, y < height
// (0x00431e10 / 0x00431e1c); w and h are clipped to the surface (0x00431e27 / 0x00431e36). f = 0x004a614d (path,
// 0x004c7828) (0x00431e48); none returns 0. [0x004c3074] = scale (0x00431e5a); [0x00543cf0] = o's pixels (+0x10) +
// 2 * (pitch * y + x) (0x00431e70..0x00431e88); [0x0056a510] = malloc 0x004a504f (3 * w / scale) (0x00431e92);
// CreateJPG 0x004ae020 (f, w / scale, h / scale, 0x00431be0) (0x00431eb0); free 0x004a5007 of the row buffer when it is
// non-NULL (0x00431ec2); fclose 0x004a609f (f) (0x00431ecb), whose result stays in eax.
// =====================================================================================================================
C7_t SaveCourseJpg_orig;
typedef int(__fastcall* V_t)(void* ecx, void* edx);
static inline int VCall0(void* o, int slot) { return (*reinterpret_cast<V_t*>(*reinterpret_cast<char**>(o) + slot))(o, 0); }
int __cdecl SaveCourseJpg_re(int path, int surf, int x, int y, int w, int h, int scale) {
    if (path == 0) return path;
    if (surf == 0 || w == 0) return path;
    if (h == 0) return h;
    if (scale == 0 || x < 0 || y < 0 || w < 0 || h < 0 || scale < 0 || scale > 0x14) return h;
    void* o = F<void*>(reinterpret_cast<void*>(surf), 4);
    const int width = o ? VCall0(o, 0xd8) : 0;
    o = F<void*>(reinterpret_cast<void*>(surf), 4);
    const int height = o ? VCall0(o, 0xdc) : 0;
    o = F<void*>(reinterpret_cast<void*>(surf), 4);
    G<int>(0x0056125c) = o ? VCall0(o, 0xe0) : 0;
    o = F<void*>(reinterpret_cast<void*>(surf), 4);
    int* fmt = reinterpret_cast<int*>(o ? VCall0(o, 0xe4) : 0);
    if (fmt[0] != 0x10 || fmt[1] != 0) return reinterpret_cast<int>(fmt);
    if (x >= width) return x;
    if (y >= height) return x;
    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;
    const int f = reinterpret_cast<C2_t>(0x004a614d)(path, 0x004c7828);
    if (f == 0) return 0;
    G<int>(0x004c3074) = scale;
    o = F<void*>(reinterpret_cast<void*>(surf), 4);
    const int pixels = o ? VCall0(o, 0x10) : 0;
    G<int>(0x00543cf0) = pixels + (G<int>(0x0056125c) * y + x) * 2;
    G<int>(0x0056a510) = reinterpret_cast<C1_t>(0x004a504f)((w * 3) / scale);
    reinterpret_cast<C4_t>(0x004ae020)(f, w / scale, h / scale, 0x00431be0);
    if (G<int>(0x0056a510) != 0) reinterpret_cast<C1_t>(0x004a5007)(G<int>(0x0056a510));
    return reinterpret_cast<C1_t>(0x004a609f)(f);
}

// =====================================================================================================================
// 0x0049c940  movieAdvanceFrame (thiscall, no arguments)
// No mmio handle [this+0xbc] returns 7 (0x0049c954). logLine 0x004a0320 (0x004e4924, 0x004e9a84, timeGetTime(), 0, 0)
// (0x0049c973; the two zeros are pushed before the argument-free timeGetTime and stay as logLine's last arguments). With
// a current chunk ([this+0xd4] != 0, 0x0049c989) mmioAscend (h, this+0xd4, 0) (0x0049c995) must return 0, else the result
// is 1. Then [this+0xd4] = 0 (0x0049c9a3) and a loop: mmioDescend (h, this+0xd4, this+0xc0, 0) (0x0049c9c3), non-zero
// returns 1; a chunk id equal to [this+0x1a4] (0x0049c9dc) ends the frame: [this+0x940]++ (0x0049cab5), logLine
// (0x004e48dc, 0x004e9a84, timeGetTime(), 0, 0) (0x0049cac8), result 0. An id equal to [this+0x1a8] (0x0049c9e8) logs
// (0x004e4900, 0x004e4910, timeGetTime(), size, 0) (0x0049ca04) and asks the sink at this+4 for a buffer (slot +0x8c,
// size) (0x0049ca1b): NULL logs (0x004e48f4, 0x004e9a84, 0, 0, 0) (0x0049ca72); otherwise mmioRead (h, [buf+0x18], size)
// (0x0049ca3f) must return size (0x0049ca4b, else 1), [buf+0x20] = [buf+0x18] + size (0x0049ca55) and slot +0x90 (buf)
// (0x0049ca5a). After any chunk but the end one: an id 'LIST' (0x5453494c, 0x0049ca7a) descends again at once; any other
// is left with mmioAscend (h, this+0xd4, 0) (0x0049ca91), 0 continuing the loop and non-zero returning 1.
// =====================================================================================================================
P0_t MovieAdvanceFrame_orig;
typedef int(__stdcall* Asc_t)(int, int, int);
typedef int(__stdcall* Desc_t)(int, int, int, int);
typedef int(__stdcall* Read_t)(int, int, int);
typedef int(__stdcall* Time_t)();
typedef int(__cdecl* Log_t)(int, int, int, int, int);
void* __fastcall MovieAdvanceFrame_re(void* self, void*) {
    const Asc_t ascend = *reinterpret_cast<Asc_t*>(0x004ba210);
    const Desc_t descend = *reinterpret_cast<Desc_t*>(0x004ba208);
    const Read_t read = *reinterpret_cast<Read_t*>(0x004ba20c);
    const Log_t log = reinterpret_cast<Log_t>(0x004a0320);
    if (F<int>(self, 0xbc) == 0) return reinterpret_cast<void*>(7);
    log(0x004e4924, 0x004e9a84, (*reinterpret_cast<Time_t*>(0x004ba21c))(), 0, 0);
    const int ck = reinterpret_cast<int>(B(self) + 0xd4);
    if (F<int>(self, 0xd4) != 0 && ascend(F<int>(self, 0xbc), ck, 0) != 0) return reinterpret_cast<void*>(1);
    F<int>(self, 0xd4) = 0;
    for (;;) {
        if (descend(F<int>(self, 0xbc), ck, reinterpret_cast<int>(B(self) + 0xc0), 0) != 0) return reinterpret_cast<void*>(1);
        const int id = F<int>(self, 0xd4);
        if (id == F<int>(self, 0x1a4)) break;
        if (id == F<int>(self, 0x1a8)) {
            log(0x004e4900, 0x004e4910, (*reinterpret_cast<Time_t*>(0x004ba21c))(), F<int>(self, 0xd8), 0);
            void* sink = B(self) + 4;
            typedef char*(__fastcall * Get_t)(void*, void*, int);
            char* buf = (*reinterpret_cast<Get_t*>(*reinterpret_cast<char**>(sink) + 0x8c))(sink, 0, F<int>(self, 0xd8));
            if (buf != 0) {
                const int size = F<int>(self, 0xd8);
                if (size != read(F<int>(self, 0xbc), F<int>(buf, 0x18), size)) return reinterpret_cast<void*>(1);
                F<int>(buf, 0x20) = F<int>(buf, 0x18) + size;
                (*reinterpret_cast<T1_t*>(*reinterpret_cast<char**>(sink) + 0x90))(sink, 0, reinterpret_cast<int>(buf));
            } else {
                log(0x004e48f4, 0x004e9a84, 0, 0, 0);
            }
        }
        if (F<int>(self, 0xd4) == 0x5453494c) continue;
        if (ascend(F<int>(self, 0xbc), ck, 0) != 0) return reinterpret_cast<void*>(1);
    }
    F<int>(self, 0x940) += 1;
    log(0x004e48dc, 0x004e9a84, (*reinterpret_cast<Time_t*>(0x004ba21c))(), 0, 0);
    return 0;
}

// =====================================================================================================================
// 0x0040bbf0  deserializeCourse (cdecl, mode)
// [0x0053e638] = mode (0x0040bbfb), then saveFieldIo 0x0040af70 (address, size) over 57 save fields in a fixed order
// (0x0040bc00 .. 0x0040bef1); the last call's result stays in eax.
// =====================================================================================================================
C1_t DeserializeCourse_orig;
static const unsigned kCourseFields[][2] = {
    {0x00822c78, 4}, {0x005685f0, 4}, {0x0058d36c, 4}, {0x0059aafc, 4}, {0x005a34e0, 1}, {0x0059ae78, 4},
    {0x00541cd8, 4}, {0x005a882c, 4}, {0x0056949c, 4}, {0x005a636c, 4}, {0x005a9cac, 4}, {0x005a9cb0, 4},
    {0x005787cc, 4}, {0x00561258, 4}, {0x00571fd4, 4}, {0x0056a524, 0x28}, {0x0059bf90, 4}, {0x00575ab0, 0x28a0},
    {0x005722e8, 0x9c4}, {0x0056988c, 0x9c4}, {0x0053caf0, 0x1388}, {0x0053bbac, 0x9c4}, {0x005a4998, 0xa29},
    {0x00838c1c, 0x169}, {0x0058bcb8, 0x1000}, {0x004c284c, 4}, {0x00561250, 4}, {0x00834170, 4}, {0x005a6364, 4},
    {0x0053a450, 4}, {0x00822c88, 4}, {0x0059b730, 4}, {0x0059ae7c, 4}, {0x0059dea0, 0x80}, {0x0059c08c, 4},
    {0x00822c70, 4}, {0x005685f8, 4}, {0x005a5a24, 4}, {0x00543cfc, 4}, {0x00572cac, 4}, {0x0059aaf8, 4},
    {0x0059ae80, 0x1c8}, {0x0056d1b8, 0x1770}, {0x004c2c94, 0x12}, {0x005689e8, 0x320}, {0x005419d0, 0x40},
    {0x00572cb0, 0xa00}, {0x004d6088, 0xa640}, {0x00542414, 0x9c4}, {0x0059e7b8, 4}, {0x00567328, 0x64},
    {0x00571d38, 4}, {0x0056fcb0, 0x1002}, {0x0059d81c, 0x100}, {0x005a46b8, 0x100}, {0x00585850, 0x1300},
    {0x00571fd8, 0x30e},
};
int __cdecl DeserializeCourse_re(int mode) {
    G<int>(0x0053e638) = mode;
    int r = 0;
    for (unsigned i = 0; i < sizeof(kCourseFields) / sizeof(kCourseFields[0]); ++i)
        r = reinterpret_cast<C2_t>(0x0040af70)(static_cast<int>(kCourseFields[i][0]), static_cast<int>(kCourseFields[i][1]));
    return r;
}

// =====================================================================================================================
// 0x0044cce0  resetAllPanels (cdecl, no arguments)
// Surface::releaseBacking 0x004838b0 on 0x00821ee8, 0x00821f28, 0x00821020 (0x0044cce6..0x0044ccfa), then Surface_free
// 0x00473ae0 on the panel surfaces at fixed addresses in this order (0x0044cd04 .. 0x0044cfe8): single surfaces, the
// 0x2c-stride runs 0x00820f40..0x00820fc4 (0x0044cd0e..0x0044cd1e) and 0x00821fa0..0x008222b8 (0x0044ce72..0x0044ce82),
// the five rows 0x00821468 + 0x2c * i (0x0044cd63..0x0044cda1: for the first three, below 0x008214ec, twelve surfaces
// at +0x160 + 0x84 * k then the row itself; for all five, +0x84), and the 21 rows 0x008224f4 + 0x2c * i
// (0x0044cee3..0x0044cf19: +0x2ec always, the row itself below 0x008225a4, +0xb0 below 0x00822704). The last
// Surface_free result stays in eax.
// =====================================================================================================================
C0_t ResetAllPanels_orig;
typedef int(__fastcall* T0r_t)(void*, void*);
static int SurfFree(unsigned p) { return reinterpret_cast<T0r_t>(0x00473ae0)(reinterpret_cast<void*>(p), 0); }
static int SurfFreeList(const unsigned* p, unsigned n) {
    int r = 0;
    for (unsigned i = 0; i < n; ++i) r = SurfFree(p[i]);
    return r;
}
int __cdecl ResetAllPanels_re() {
    static const unsigned a[] = {0x008210c8, 0x008210f4, 0x00821308, 0x00821360, 0x008213b8, 0x00821410};
    static const unsigned b[] = {0x00821334, 0x0082138c, 0x008213e4, 0x0082143c, 0x00821c60, 0x00821c8c, 0x00821200,
                                 0x00821258, 0x008212b0, 0x0082122c, 0x00821284, 0x008212dc, 0x008223c0, 0x00822418,
                                 0x00822470, 0x008223ec, 0x00822444, 0x0082249c, 0x00821f48, 0x00821f74};
    static const unsigned c[] = {0x008222b8, 0x00822310, 0x00822368, 0x008222e4, 0x0082233c, 0x00822394, 0x00821bf8,
                                 0x00821c24, 0x008227b4};
    static const unsigned d[] = {0x008227b4, 0x008224c8, 0x00821070, 0x0082109c, 0x00821120, 0x0082114c, 0x00821178,
                                 0x008211a4, 0x008211d0, 0x00821cb8, 0x00821d10, 0x00821d68, 0x00821dc0, 0x00821e18,
                                 0x00821e70, 0x00821ce4, 0x00821d3c, 0x00821d94, 0x00821dec, 0x00821e44, 0x00821e9c};
    typedef int(__fastcall * Rel_t)(void*, void*);
    reinterpret_cast<Rel_t>(0x004838b0)(reinterpret_cast<void*>(0x00821ee8), 0);
    reinterpret_cast<Rel_t>(0x004838b0)(reinterpret_cast<void*>(0x00821f28), 0);
    reinterpret_cast<Rel_t>(0x004838b0)(reinterpret_cast<void*>(0x00821020), 0);
    SurfFree(0x00821040);
    for (unsigned p = 0x00820f40; p < 0x00820fc4; p += 0x2c) SurfFree(p);
    SurfFreeList(a, sizeof(a) / sizeof(a[0]));
    for (unsigned row = 0x00821468; row < 0x00821544; row += 0x2c) {
        if (row < 0x008214ec) {
            for (unsigned k = 0; k < 12; ++k) SurfFree(row + 0x160 + 0x84 * k);
            SurfFree(row);
        }
        SurfFree(row + 0x84);
    }
    SurfFreeList(b, sizeof(b) / sizeof(b[0]));
    for (unsigned p = 0x00821fa0; p < 0x008222b8; p += 0x2c) SurfFree(p);
    SurfFreeList(c, sizeof(c) / sizeof(c[0]));
    for (unsigned row = 0x008224f4; row < 0x00822890; row += 0x2c) {
        SurfFree(row + 0x2ec);
        if (row < 0x008225a4) SurfFree(row);
        if (row < 0x00822704) SurfFree(row + 0xb0);
    }
    return SurfFreeList(d, sizeof(d) / sizeof(d[0]));
}

// =====================================================================================================================
// 0x0040aa80  hireEmployee (cdecl, no arguments)
// k = showHireEmployeeDialog 0x00459400 () (0x0040aa82), m = enterModal483d30 0x00483d30 () (0x0040aa89); k outside 0..11
// (0x0040aa90 / 0x0040aa99) returns m. With bucketValue 0x0044faf0 ([0x005685f0] - 1) < 1 (0x0040aab1) and k % 3 == 2
// (0x0040aac0): playSound 0x004481b0 (0x18, 0x64, 0, 0, 0) (0x0040aacc), strcpy(text, 0x004c5228), startMessage
// 0x0040cb00 (0x80006000, 1, -1) (0x0040aaff), whose result is returned. Otherwise canAfford 0x00406c30 (0) == 0
// (0x0040ab1f) returns 0; else g = spawnWalker 0x00402970 (-2 - k / 3) (0x0040ab67; [0x00571fd4] and the day word at
// 0x0058421a are rewritten with their own values, 0x0040ab2f / 0x0040ab4e) and the walker record r = 0x00585850 + 0x4c g:
// with byte [0x00575cb8] != 0 (0x0040ab77), c = (byte [0x00575cb9] + 2) & 7 and r+0x10 / r+0x11 = byte [0x004c2878 + 4c]
// + byte [0x00575cc0] / byte [0x004c2898 + 4c] + byte [0x00575cc4] (0x0040ab97..0x0040abb2); else each is +3 or -3 (by
// whether Random::range 0x0045c1e0 (2) on 0x00822d9c answers non-zero in its low word) plus byte [0x00578150] /
// [0x00578154] (0x0040abca..0x0040ac03). k % 3 == 2 sets bit 3 of r+0x12 (0x0040ac18). d = (((angleFixed 0x004672d0
// (dx, dy) >> 28) & 15) + 1 >> 1) & 7 for dx = (sbyte r+0x10 << 10) - r+0 + 0x200, dy likewise from r+0x11 / r+4
// (0x0040ac1f..0x0040ac61), stored at r+0x16 (0x0040ac66); r+0 / r+4 gain (dword [0x004c2878 + 4d] << 10) + 0x200 and
// (dword [0x004c2898 + 4d] << 10) + 0x200 (0x0040ac84 / 0x0040acae); the word r+0x22 = [0x00834170] / 1024 (rounded
// toward zero, 0x0040acb4..0x0040acc0), which is also the result.
// =====================================================================================================================
C0_t HireEmployee_orig;
int __cdecl HireEmployee_re() {
    const int k = reinterpret_cast<C0_t>(0x00459400)();
    const int m = reinterpret_cast<C0_t>(0x00483d30)();
    if (k < 0 || k >= 12) return m;
    const int b = reinterpret_cast<C1_t>(0x0044faf0)(G<int>(0x005685f0) - 1);
    if (b < 1 && k % 3 == 2) {
        reinterpret_cast<C5_t>(0x004481b0)(0x18, 0x64, 0, 0, 0);
        strcpy(Text(), reinterpret_cast<const char*>(0x004c5228));
        return reinterpret_cast<C3_t>(0x0040cb00)(static_cast<int>(0x80006000u), 1, -1);
    }
    if (reinterpret_cast<C1_t>(0x00406c30)(0) == 0) return 0;
    const int g = reinterpret_cast<C1_t>(0x00402970)(-2 - k / 3);
    const unsigned r = 0x00585850u + static_cast<unsigned>(g) * 0x4c;
    if (G<unsigned char>(0x00575cb8) != 0) {
        const unsigned c = (G<unsigned char>(0x00575cb9) + 2u) & 7u;
        G<unsigned char>(r + 0x10) = static_cast<unsigned char>(G<unsigned char>(0x004c2878 + 4 * c) + G<unsigned char>(0x00575cc0));
        G<unsigned char>(r + 0x11) = static_cast<unsigned char>(G<unsigned char>(0x004c2898 + 4 * c) + G<unsigned char>(0x00575cc4));
    } else {
        void* rng = reinterpret_cast<void*>(0x00822d9c);
        const int r1 = reinterpret_cast<T1_t>(0x0045c1e0)(rng, 0, 2);
        G<unsigned char>(r + 0x10) = static_cast<unsigned char>(((r1 & 0xffff) ? 3 : -3) + G<unsigned char>(0x00578150));
        const int r2 = reinterpret_cast<T1_t>(0x0045c1e0)(rng, 0, 2);
        G<unsigned char>(r + 0x11) = static_cast<unsigned char>(((r2 & 0xffff) ? 3 : -3) + G<unsigned char>(0x00578154));
    }
    if (k % 3 == 2) G<unsigned char>(r + 0x12) |= 8;
    const int dy = (G<signed char>(r + 0x11) << 10) - G<int>(r + 4) + 0x200;
    const int dx = (G<signed char>(r + 0x10) << 10) - G<int>(r) + 0x200;
    const int a = reinterpret_cast<C2_t>(0x004672d0)(dx, dy);
    const int d = ((((a >> 28) & 0xf) + 1) >> 1) & 7;
    G<signed char>(r + 0x16) = static_cast<signed char>(d);
    G<int>(r) += (G<int>(0x004c2878 + 4 * d) << 10) + 0x200;
    G<int>(r + 4) += (G<int>(0x004c2898 + 4 * d) << 10) + 0x200;
    const int date = G<int>(0x00834170);
    const int day = (date + ((date >> 31) & 0x3ff)) >> 10;
    G<short>(r + 0x22) = static_cast<short>(day);
    return day;
}

// =====================================================================================================================
// 0x00481ca0  Flic::buildFrameTable (thiscall, flags; ret 4)
// No loader object o = [this+0x54] returns 7 (0x00481cb2). [this+0x7c] = GetFileSize ([o+8], 0) (0x00481cc0); [this+0x58]
// = the header h = [o+4] (0x00481ccf); word h+4 must be 0xaf12 (0x00481cd2, else 0x16) and word h+0xc 8 (0x00481ce6, else
// 0x17). A header whose dword h+0x1a is not 0xf1f1f2f2 (0x00481cf9) is copied into p = operator new 0x004a57f1 (size)
// (0x00481d8d) and converted: p+0x58 = 0x1c, word p+0x62 = word p+6, word p+0x60 = 1, p+0x5c = 0, words p+0x64 / p+0x66 =
// 0, word p+0x68 / p+0x6a = word p+8 / p+0xa, and p+0x1a = 0xf1f1f2f2 (0x00481ddf..0x00481e29). An already converted one
// is copied the same way only with flags & 4 (0x00481d06); otherwise [this+0x44] = [this+0x58] = h (0x00481d75 /
// 0x00481d81). A copy deletes o through its slot 0 (1) (0x00481d54 / 0x00481dd3) and clears [this+0x54]; a failed
// operator new runs Flic::reset 0x00481ba0 and returns 4 (0x00481d1f / 0x00481d9e). The result is 0 when [this+0x44] is
// non-NULL and 7 otherwise (`neg; sbb; and al, 0xf9; add 7`, 0x00481d5f..0x00481d6a).
// =====================================================================================================================
T1_t FlicBuildFrameTable_orig;
int __fastcall FlicBuildFrameTable_re(void* self, void*, int flags) {
    char* o = F<char*>(self, 0x54);
    if (o == 0) return 7;
    typedef unsigned(__stdcall * Size_t)(int, int);
    const unsigned size = (*reinterpret_cast<Size_t*>(0x004ba0a0))(F<int>(o, 8), 0);
    F<unsigned>(self, 0x7c) = size;
    char* h = F<char*>(o, 4);
    F<char*>(self, 0x58) = h;
    if (F<unsigned short>(h, 4) != 0xaf12) return 0x16;
    if (F<unsigned short>(h, 0xc) != 8) return 0x17;
    const bool convert = F<unsigned>(h, 0x1a) != 0xf1f1f2f2u;
    if (!convert && !(flags & 4)) {
        F<char*>(self, 0x44) = h;
        F<char*>(self, 0x58) = h;
        return h ? 0 : 7;
    }
    char* p = reinterpret_cast<char*>(reinterpret_cast<C1_t>(0x004a57f1)(static_cast<int>(size)));
    F<char*>(self, 0x44) = p;
    if (p == 0) {
        reinterpret_cast<T0r_t>(0x00481ba0)(self, 0);
        return 4;
    }
    memcpy(p, F<char*>(F<char*>(self, 0x54), 4), F<unsigned>(self, 0x7c));
    char* lo = F<char*>(self, 0x54);
    if (lo) (*reinterpret_cast<T1_t*>(*reinterpret_cast<char**>(lo)))(lo, 0, 1);
    F<char*>(self, 0x54) = 0;
    F<char*>(self, 0x58) = F<char*>(self, 0x44);
    if (convert) {
        char* q = F<char*>(self, 0x58);
        F<int>(q, 0x58) = 0x1c;
        F<unsigned short>(q, 0x62) = F<unsigned short>(q, 6);
        F<unsigned short>(q, 0x60) = 1;
        F<int>(q, 0x5c) = 0;
        F<unsigned short>(q, 0x64) = 0;
        F<unsigned short>(q, 0x66) = 0;
        F<unsigned short>(q, 0x68) = F<unsigned short>(q, 8);
        F<unsigned short>(q, 0x6a) = F<unsigned short>(q, 0xa);
        F<unsigned>(q, 0x1a) = 0xf1f1f2f2u;
    }
    return F<char*>(self, 0x44) ? 0 : 7;
}

// =====================================================================================================================
// 0x00405b10  saveGameDialog (cdecl, no arguments)
// drawPanelS2 0x0040cc00 (0x36, 0x50, 0x274, 0x50, 0) (0x00405b22); setDrawTarget 0x004762d0 on [0x004c1570] (0x0051b360,
// 0, 0, 0) (0x00405b3b); drawCenteredText2 0x00404bc0 (0x004c3f74, 0x180, 0x58, 0x80007fff) (0x00405b51); the default
// name: text[0] = 0 (0x00405b58), appendCourseTitle 0x0040daa0 (-1) (0x00405b5f), strcat 0x004c3f70, the day
// (date % 1024) * 30 / 1024 + 1 of [0x00834170] (both divisions toward zero, 0x00405b91..0x00405bbf) through __itoa
// 0x004ad425 (day, 0x0058a528, 10) (0x00405bc1), strcat 0x004c3f70, appendDate 0x0040d7b0 ([0x00834170]) (0x00405c21);
// textInputBox 0x0045b2c0 (0x004c3f5c, 0x40, 0x66, 0x30) (0x00405c31). An empty answer (0x00405c40) or a 0 from
// sanitizeFileName 0x00405ac0 (text) (0x00405c55) shows the cancel message: strcpy(text, 0x004c3eec), startMessage
// 0x0040cb00 (0x80001284, 1, -4) (0x00405e14). Otherwise the name is kept in a local, text = 0x004c3f4c + name + 0x004c3f44
// (0x00405c5b..0x00405cfe) and fileExists 0x004378a0 (text) (0x00405d00); when it exists, text = name + 0x004c3f0c and
// showPopup 0x0046d6e0 (0x190, 0x8c, 1, 1, 0) (0x00405d6c): a non-zero answer is returned at once (0x00405d76). Then
// saveGame 0x0040b4a0 (text = name) (0x00405da5), strcpy(text, 0x004c3f00), startMessage (0x80001284, 1, -4)
// (0x00405dd8). The result is the last callee's eax.
// =====================================================================================================================
C0_t SaveGameDialog_orig;
int __cdecl SaveGameDialog_re() {
    reinterpret_cast<C5_t>(0x0040cc00)(0x36, 0x50, 0x274, 0x50, 0);
    reinterpret_cast<T4_t>(0x004762d0)(G<void*>(0x004c1570), 0, 0x0051b360, 0, 0, 0);
    reinterpret_cast<C4_t>(0x00404bc0)(0x004c3f74, 0x180, 0x58, static_cast<int>(0x80007fffu));
    Text()[0] = 0;
    reinterpret_cast<C1_t>(0x0040daa0)(-1);
    strcat(Text(), reinterpret_cast<const char*>(0x004c3f70));
    const int day = (G<int>(0x00834170) % 1024) * 30 / 1024 + 1;
    strcat(Text(), reinterpret_cast<const char*>(reinterpret_cast<C3_t>(0x004ad425)(day, 0x0058a528, 10)));
    strcat(Text(), reinterpret_cast<const char*>(0x004c3f70));
    reinterpret_cast<C1_t>(0x0040d7b0)(G<int>(0x00834170));
    reinterpret_cast<C4_t>(0x0045b2c0)(0x004c3f5c, 0x40, 0x66, 0x30);
    if (Text()[0] == 0 || reinterpret_cast<C1_t>(0x00405ac0)(0x0051a068) == 0) {
        strcpy(Text(), reinterpret_cast<const char*>(0x004c3eec));
        return reinterpret_cast<C3_t>(0x0040cb00)(static_cast<int>(0x80001284u), 1, -4);
    }
    char name[0x64];
    strcpy(name, Text());
    strcpy(Text(), reinterpret_cast<const char*>(0x004c3f4c));
    strcat(Text(), name);
    strcat(Text(), reinterpret_cast<const char*>(0x004c3f44));
    if (reinterpret_cast<C1_t>(0x004378a0)(0x0051a068) != 0) {
        strcpy(Text(), name);
        strcat(Text(), reinterpret_cast<const char*>(0x004c3f0c));
        const int r = reinterpret_cast<C5_t>(0x0046d6e0)(0x190, 0x8c, 1, 1, 0);
        if (r != 0) return r;
    }
    strcpy(Text(), name);
    reinterpret_cast<C1_t>(0x0040b4a0)(0x0051a068);
    strcpy(Text(), reinterpret_cast<const char*>(0x004c3f00));
    return reinterpret_cast<C3_t>(0x0040cb00)(static_cast<int>(0x80001284u), 1, -4);
}

SG_HOOK("golf_clean.exe", 0x00405b10, saveGameDialog, SaveGameDialog_re, SaveGameDialog_orig);
SG_HOOK("golf_clean.exe", 0x0040aa80, hireEmployee, HireEmployee_re, HireEmployee_orig);
SG_HOOK("golf_clean.exe", 0x00481ca0, Flic_buildFrameTable, FlicBuildFrameTable_re, FlicBuildFrameTable_orig);
SG_HOOK("golf_clean.exe", 0x0040bbf0, deserializeCourse, DeserializeCourse_re, DeserializeCourse_orig);
SG_HOOK("golf_clean.exe", 0x0044cce0, resetAllPanels, ResetAllPanels_re, ResetAllPanels_orig);
SG_HOOK("golf_clean.exe", 0x004855b0, engineInit, EngineInit_re, EngineInit_orig);
SG_HOOK("golf_clean.exe", 0x00496030, buildLabeledControl, BuildLabeledControl_re, BuildLabeledControl_orig);
SG_HOOK("golf_clean.exe", 0x00480b30, Window_createBackingSurface, CreateBackingSurface_re, CreateBackingSurface_orig);
SG_HOOK("golf_clean.exe", 0x00431d20, saveCourseJpg, SaveCourseJpg_re, SaveCourseJpg_orig);
SG_HOOK("golf_clean.exe", 0x0049c940, movieAdvanceFrame, MovieAdvanceFrame_re, MovieAdvanceFrame_orig);
SG_HOOK("golf_clean.exe", 0x004065c0, showCharacterIntro, ShowCharacterIntro_re, ShowCharacterIntro_orig);
SG_HOOK("golf_clean.exe", 0x00432560, showPreferencesMenu, ShowPreferencesMenu_re, ShowPreferencesMenu_orig);
SG_HOOK("golf_clean.exe", 0x00431ee0, saveThumbAndFull, SaveThumbAndFull_re, SaveThumbAndFull_orig);
SG_HOOK("golf_clean.exe", 0x00435680, clickEmployeeView, ClickEmployeeView_re, ClickEmployeeView_orig);
SG_HOOK("golf_clean.exe", 0x00436c00, handleEmployeeAction, HandleEmployeeAction_re, HandleEmployeeAction_orig);
SG_HOOK("golf_clean.exe", 0x00436060, clickGolferPanel, ClickGolferPanel_re, ClickGolferPanel_orig);
SG_HOOK("golf_clean.exe", 0x004a08a0, panelDispatch, PanelDispatch_re, PanelDispatch5_orig);
SG_HOOK("golf_clean.exe", 0x00482570, decodeImageChunks, DecodeImageChunks_re, DecodeImageChunks_orig);
