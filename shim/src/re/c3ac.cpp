// C3 batch c3ac (2026-10-08) of golf_clean.exe: large gameplay functions (candidates over 2 KB). Hand-written from the
// disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; every body
// cites the address of each global, per-hole field and callee it uses. All are __cdecl. Callees are invoked through
// their original addresses (so a hooked callee still runs its own reimplementation and the path-1 A/B drives both arms;
// the shared text buffer, the event records, the RNG seed and the ticker globals are declared as A/B state regions).
#include "hooks.h"
#include <string.h>

namespace {

template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// A hole record is 0x208 bytes; the per-hole tables of holeMagazineEvent are addressed as fieldBase + hole*0x208
// (the asm forms hole*0x208 as ((hole<<6)+hole)<<3 at 0x0042df36; the decompiler's "hole*0x82" is the same offset in
// dword units). hole runs 1..0x12.
template <typename T> T& fld(unsigned base, int hole) { return ref<T>(base + hole * 0x208); }

// Appends the NUL-terminated string at `src` to the text buffer 0x0051a068 (same bytes as the inline strlen / rep movs
// the original uses; the terminator is copied too).
void appendText(const char* src) {
    char* dst = reinterpret_cast<char*>(0x0051A068);
    while (*dst) ++dst;
    do { *dst++ = *src; } while (*src++);
}

typedef void(__cdecl* AnnounceType_t)(int, int);
typedef int(__cdecl* StartMessage_t)(int, int, int);
typedef void(__cdecl* PlaySound_t)(int, int, int, int, int);
typedef int(__cdecl* AngleFixed_t)(int, int);
typedef void(__cdecl* AppendHoleName_t)(int);
typedef void(__cdecl* LogTick_t)(int, int);
typedef void(__cdecl* PostEvent_t)(int, int, int);
typedef char*(__cdecl* Itoa_t)(int, char*, int);

const AnnounceType_t kAnnounceType = reinterpret_cast<AnnounceType_t>(0x00460DF0);   // announceHoleType
const StartMessage_t kStartMessage = reinterpret_cast<StartMessage_t>(0x0040CB00);   // startMessage
const PlaySound_t kPlaySound = reinterpret_cast<PlaySound_t>(0x004481B0);            // playSound
const AngleFixed_t kAngleFixed = reinterpret_cast<AngleFixed_t>(0x004672D0);         // angleFixed
const AppendHoleName_t kAppendHoleName = reinterpret_cast<AppendHoleName_t>(0x00407280);  // appendHoleName
const LogTick_t kLogTick = reinterpret_cast<LogTick_t>(0x0040C6F0);                  // logTick
const PostEvent_t kPostEvent = reinterpret_cast<PostEvent_t>(0x0046E7B0);            // postEvent
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);                           // __itoa

// Magazine-message source strings, referenced by address only (never reproduced; the repo is public):
const char* const kHoleHash = reinterpret_cast<const char*>(0x004C591C);  // "Hole #"
const char* const kKnownAs = reinterpret_cast<const char*>(0x004C7788);   // " (henceforth known as '"
const char* const kBest100 = reinterpret_cast<const char*>(0x004C7754);   // "'), has been rated as one of the best 100 holes "
const char* const kByEnquirer = reinterpret_cast<const char*>(0x004C7728);// "in the country by Golf Enquirer magazine!"
const char* const kFeesSuffix = reinterpret_cast<const char*>(0x004C7708);// " Increase greens fees by ...100."
const char* const kTop18 = reinterpret_cast<const char*>(0x004C76DC);     // " has been rated as one of the Top 18 holes "
const char* const kByGreat = reinterpret_cast<const char*>(0x004C76AC);   // "in the country by Great Golf Holes magazine!"

// 0x0042dea0  holeMagazineEvent(): once-per-tick pass over the 18 holes of the current course. For each hole with a
// non-zero par byte (0x00575ab0 + hole*0x208, 0x0042df40) it: accumulates the course totals g_par 0x0059aafc,
// g_yards 0x0058d36c and (when the dword 0x00575ad0 != 0) g_fun_rating 0x0059ae78 (0x0042dfc7); builds the per-hole
// shot histogram at 0x00575ada (9 rows x 8 columns of signed shorts, 0x0042dff0) into column sums col40/col20;
// flags the hole too-hard (bit 0x4) or too-easy (bit 0x8) from the histogram total vs weighted sum against the
// difficulty dword 0x00822c88 (0x0042e085); sets the notable-skill flags 0x100/0x200/0x400 and the bitmask `notable`
// from the length/accuracy/imagination column deltas (columns 6/5/3 vs 7, 0x0042e0f2); posts the hole-type notice
// (announceHoleType 0x00460df0 + startMessage 0x0040cb00, 0x0042e2ec) once per notice bit (mask 0x005685f8); derives
// the per-hole counter 0x00575cac from neighbour similarity and the two tee/pin angles (angleFixed 0x004672d0,
// 0x0042e478); and fires the two magazine events (0x0042e59b / 0x0042e649) when the hole rating 0x00575ca4 and this
// hole's skill delta clear their thresholds and no blocking flag is set, each building its message into 0x0051a068
// and calling startMessage, logTick 0x0040c6f0, playSound 0x004481b0 and postEvent 0x0046e7b0. Writes the course
// skill rating g_skill_rating 0x00541cd8 at the end (0x0042e794). The member-count scratch 0x0056d1b0 is (members in
// the type table 0x005849e0, type&7 > 2 and not banned) minus placed objects of type 5 (0x0058bcb8). __cdecl, void.
typedef void(__cdecl* Hole_t)();
Hole_t HoleMagazineEvent_orig;
void __cdecl HoleMagazineEvent_re() {
    // Member-count scratch 0x0056d1b0 (0x0042deaf..0x0042def8).
    int members = 0;
    for (unsigned p = 0x005849E2; p < 0x005856F2; p += 0x2c)                       // type table 0x005849e0, stride 0x2c
        if (ref<unsigned char>(p + 0x27) != 0xff && (ref<unsigned char>(p) & 7) > 2) ++members;
    for (unsigned q = 0x0058BCB8; q < 0x0058CCB8; q += 0x10)                        // placed objects, stride 0x10
        if (ref<short>(q) == 5) --members;
    ref<int>(0x0056D1B0) = members;

    ref<int>(0x0059AE78) = 0;  // g_fun_rating
    ref<int>(0x0059AAFC) = 0;  // g_par
    ref<int>(0x0058D36C) = 0;  // g_yards
    ref<int>(0x005A636C) = 0;  // g_imagination_x100
    ref<int>(0x0056949C) = 0;  // g_accuracy_skill_x100
    ref<int>(0x005A882C) = 0;  // g_length_skill_x100

    int skillSum = 0;          // local_60: running course skill sum
    int prevNotable = -1;      // local_4c: previous hole's notable bitmask

    int hole = 1;
    for (;;) {
        const signed char par = fld<signed char>(0x00575AB0, hole);  // 0x0042df40
        if (par != 0) {
            const int prevSkillSum = skillSum;                 // iVar10 at loop top
            const int prevFun = ref<int>(0x0059AE78);          // iVar4 at loop top

            ref<int>(0x0058D36C) += fld<short>(0x00575AB4, hole);   // g_yards += yards word
            ref<int>(0x0059AAFC) += par;                            // g_par += par

            int col20[8], col40[8];
            for (int c = 0; c < 8; ++c) { col20[c] = static_cast<int>(par) << 3; col40[c] = 8; }  // 0x0042df6e..

            if (fld<int>(0x00575AD0, hole) != 0)                    // 0x0042dfc1
                ref<int>(0x0059AE78) += (fld<short>(0x00575C08, hole) * 100) /
                                        (fld<int>(0x00575AD4, hole) / 2 + 4 + fld<int>(0x00575AD0, hole));

            int histTotal = 0, histWeighted = 0;                   // iVar11, iVar12
            short* rowPtr = reinterpret_cast<short*>(0x00575ADA + hole * 0x208);  // local_68 (histogram base)
            for (int row = 1; row < 10; ++row) {                   // local_5c 1..9
                short* pc = rowPtr;
                for (int c = 0; c < 8; ++c) {                      // 8 columns, step 0xb shorts
                    const int v = *pc;                             // signed short
                    histTotal += v;
                    col40[c] += v;
                    col20[c] += v * row;
                    histWeighted += v * row;
                    pc += 0xb;
                }
                ++rowPtr;
            }

            unsigned f0 = fld<unsigned>(0x00575CB0, hole);         // uVar13
            fld<unsigned>(0x00575CB0, hole) = f0 & 0xFFFFFFF3;     // clear too-hard/too-easy (0x0042e03b)
            if (histTotal >= 10) {                                 // 0x0042e057 (9 < iVar11)
                const int d = -ref<int>(0x00822C88);
                if (((d + 6) * histTotal) / 3 + par * histTotal < histWeighted)   // too hard -> bit 0x4
                    fld<unsigned>(0x00575CB0, hole) = (f0 & 0xFFFFFFF3) | 4;
                if (histWeighted < par * histTotal - ((d + 3) * histTotal) / 6)   // too easy -> bit 0x8
                    fld<unsigned>(0x00575CB0, hole) |= 8;
            }

            unsigned f1 = fld<unsigned>(0x00575CB0, hole);         // uVar13 (re-read, 0x0042e15a)
            int notable = 0;                                       // local_68
            int minDelta = 100;                                    // local_5c
            int minSkillBit = 0;                                   // local_58
            fld<unsigned>(0x00575CB0, hole) = f1 & 0xFFFFF8FF;     // clear notable bits 0x700
            const int notableThresh = (ref<int>(0x00822C88) != 0) ? 0x32 : 0x19;
            if (col40[7] != 0) {
                if (col40[6] != 0) {                               // length (column 6)
                    const int delta = (col20[6] * 100) / col40[6] - (col20[7] * 100) / col40[7];
                    ref<int>(0x005A882C) += delta;                 // g_length
                    skillSum += delta;
                    notable = (delta >= notableThresh) ? 1 : 0;    // 0x0042e0f2
                    if (delta > 0x31) fld<unsigned>(0x00575CB0, hole) = (f1 & 0xFFFFF8FF) | 0x100;
                    if (delta < 100) { minSkillBit = 1; minDelta = delta; }
                }
                if (col40[5] != 0) {                               // accuracy (column 5)
                    const int delta = (col20[5] * 100) / col40[5] - (col20[7] * 100) / col40[7];
                    skillSum += delta;
                    ref<int>(0x0056949C) += delta;                 // g_accuracy
                    if (delta >= notableThresh) notable |= 2;
                    if (delta > 0x31) fld<unsigned>(0x00575CB0, hole) |= 0x200;
                    if (delta < minDelta) { minSkillBit = 2; minDelta = delta; }
                }
                if (col40[3] != 0) {                               // imagination (column 3)
                    const int delta = (col20[3] * 100) / col40[3] - (col20[7] * 100) / col40[7];
                    ref<int>(0x005A636C) += delta;                 // g_imagination
                    skillSum += delta;
                    if (delta >= notableThresh) notable |= 4;
                    if (delta > 0x31) fld<unsigned>(0x00575CB0, hole) |= 0x400;
                    if (delta < minDelta) { minSkillBit = 4; minDelta = delta; }
                }
            }
            if (minDelta < ((ref<int>(0x00822C88) != 0) ? 0x64 : 0x32))  // 0x0042e26c
                notable &= ~minSkillBit;

            // Hole-type notice (announceHoleType + startMessage), once per notice bit (mask 0x005685f8). The forced
            // branch (flag bit 1 of 0x0056c7b4) re-processes hole 1 with notable = 4 (0x0042e2a9..0x0042e2d6).
            bool fire;
            if ((ref<unsigned char>(0x0056C7B4) & 2) == 0) {
                fire = (notable != 0);
            } else {
                ref<unsigned>(0x0056C7B4) &= 0xFFFFFFFD;
                hole = 1;
                notable = 4;
                fire = true;
            }
            if (fire) {
                const unsigned bit = 1u << notable;                // 1 << local_68
                if ((bit & ref<unsigned>(0x005685F8)) == 0) {
                    kAnnounceType(hole, notable);
                    if (kStartMessage(0x80001284, 0, -4) != 0) {
                        ref<unsigned>(0x005685F8) |= bit;
                        kPlaySound(0x2a, 100, 0, 0, 0);
                    }
                }
            }

            fld<unsigned>(0x00575CB0, hole) &= 0xFFFFFFEF;         // clear bit 0x10 (0x0042e2fb)
            fld<int>(0x00575CAC, hole) = 0;
            if (hole > 1) {
                if (notable == prevNotable && fld<int>(0x00575AD0, hole) > 7 && notable != 7)
                    fld<int>(0x00575CAC, hole) = 1;
                if (((fld<unsigned>(0x00575AA8, hole) ^ fld<unsigned>(0x00575CB0, hole)) & 0x60) == 0)
                    fld<int>(0x00575CAC, hole) += 1;
                if (fld<short>(0x00575BE2, hole) == 0 && fld<short>(0x00575BE4, hole) == 0)
                    fld<int>(0x00575CAC, hole) += 1;
                if (fld<signed char>(0x00575AB0, hole) == fld<signed char>(0x005758A8, hole))
                    fld<int>(0x00575CAC, hole) += 1;
                const int a1 = kAngleFixed(fld<int>(0x005758C0, hole) - fld<int>(0x005758B0, hole),
                                           fld<int>(0x005758C4, hole) - fld<int>(0x005758B4, hole));
                const int a2 = kAngleFixed(fld<int>(0x00575AC8, hole) - fld<int>(0x00575AB8, hole),
                                           fld<int>(0x00575ACC, hole) - fld<int>(0x00575ABC, hole));
                const int diff = a2 - a1;                          // 0x0042e4a0
                const int s = diff >> 0x1f;
                if ((((diff >> 0x18) ^ s) - s) < 0x28)
                    fld<int>(0x00575CAC, hole) += 1;
                if (fld<int>(0x00575CAC, hole) != 0 && ref<int>(0x00822C88) < 2)
                    fld<int>(0x00575CAC, hole) -= 1;
            }

            prevNotable = notable;                                 // local_4c = local_68
            int skillDelta = skillSum - prevSkillSum;              // iVar10
            const int funDelta = ((ref<int>(0x0059AE78) - prevFun) * 6) /
                                 ((ref<int>(0x00822C88) != 0) + 2); // iVar4
            if (ref<int>(0x00822C88) < 2 && skillDelta < funDelta)
                skillDelta = funDelta;

            // Magazine event 1 (best-100): rating 0x00575ca4 > 200, this hole's skill delta > 200, flags & 0xd clear.
            if (fld<int>(0x00575CA4, hole) > 200 && skillDelta > 200 &&
                (fld<unsigned char>(0x00575CB0, hole) & 0xd) == 0) {
                strcpy(reinterpret_cast<char*>(0x0051A068), kHoleHash);
                kItoa(hole, reinterpret_cast<char*>(0x0058A528), 10);
                appendText(reinterpret_cast<char*>(0x0058A528));
                appendText(kKnownAs);
                fld<unsigned>(0x00575CB0, hole) |= 0x80;           // 0x0042e5da (appendHoleName reads bit 0x80)
                kAppendHoleName(hole);
                appendText(kBest100);
                appendText(kByEnquirer);
                appendText(kFeesSuffix);
                if (kStartMessage(0x80004010, 0, -21) == 0) {
                    fld<unsigned>(0x00575CB0, hole) &= 0xFFFFFF7F; // clear bit 0x80
                } else {
                    fld<unsigned>(0x00575CB0, hole) |= 1;
                    ref<int>(0x004C2850) += 1;
                    kLogTick(0x80, static_cast<unsigned short>(hole));
                    kPlaySound(0x2e, 100, 0, 0, 0);
                    kPostEvent(7, fld<int>(0x00575AC8, hole) * 0x400 + 0x200,
                               fld<int>(0x00575ACC, hole) * 0x400 + 0x200);
                }
            }

            // Magazine event 2 (Top-18): rating > 400, skill delta > 300, flags & 0xe clear.
            if (fld<int>(0x00575CA4, hole) > 400 && skillDelta > 300 &&
                (fld<unsigned char>(0x00575CB0, hole) & 0xe) == 0) {
                *reinterpret_cast<char*>(0x0051A068) = 0;
                kAppendHoleName(hole);
                appendText(kTop18);
                appendText(kByGreat);
                appendText(kFeesSuffix);
                if (kStartMessage(0x80005288, 0, -21) != 0) {
                    ref<int>(0x004C2850) += 1;
                    fld<unsigned>(0x00575CB0, hole) |= 2;
                    kLogTick(0xa0, static_cast<unsigned short>(hole));
                    kPlaySound(0x2f, 100, 0, 0, 0);
                    kPostEvent(10, fld<int>(0x00575AC8, hole) * 0x400 + 0x200,
                               fld<int>(0x00575ACC, hole) * 0x400 + 0x200);
                }
            }
        }

        hole = hole + 1;
        if (hole > 0x12) {
            ref<int>(0x00541CD8) = skillSum;                       // g_skill_rating_x100
            return;
        }
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0042DEA0, holeMagazineEvent, HoleMagazineEvent_re, HoleMagazineEvent_orig);
