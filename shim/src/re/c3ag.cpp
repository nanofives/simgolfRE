// C3 batch c3ag (2026-10-08) of golf_clean.exe: golfer/course event builders. Hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; every body cites the
// address of each global, table field and callee it uses. Callees are invoked through their original addresses, so a
// callee that another batch hooks runs its own reimplementation in both A/B arms. The shared text buffer 0x0051a068,
// the landmark bitmasks and the startMessage ticker globals are declared as A/B state regions (re/frida/registry.d/
// c3ag.py).
#include "hooks.h"
#include <string.h>

namespace {

template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }

char* const kText = reinterpret_cast<char*>(0x0051A068);  // shared scratch text buffer

// Appends the NUL-terminated string at `src` to the text buffer (the same bytes as the inline repne scasb / rep movs
// strcat the originals use at 0x0045c529, 0x0047242b and 0x00472460).
void appendText(const char* src) {
    char* dst = kText;
    while (*dst) ++dst;
    do { *dst++ = *src; } while (*src++);
}

// Returns the binary string at `addr` (the text itself is never reproduced here; the repo is public).
const char* str(unsigned addr) { return reinterpret_cast<const char*>(addr); }

typedef void(__cdecl* BuildGolferName_t)(int golfer, int withTitle);
typedef void(__cdecl* ReplaceInText_t)(const char* key, const char* repl);
typedef int(__cdecl* TileBlocked_t)(int x, int y);
typedef void(__cdecl* LandmarkName_t)(int type, int full);
typedef int(__cdecl* StartMessage_t)(int arg, int prio, int sound);
typedef int(__fastcall* RandomRange_t)(void* self, void* edx, int n);
typedef void(__cdecl* LogTick_t)(int a, int b);
typedef void(__cdecl* PlaySound_t)(int, int, int, int, int);
typedef void(__cdecl* AppendNumber_t)(int n);
typedef void(__cdecl* AppendCents_t)(int n);
typedef void(__cdecl* AppendOpinion_t)(int rating, int twist);
typedef int(__cdecl* Clamp_t)(int v, int lo, int hi);
typedef char*(__cdecl* Itoa_t)(int v, char* buf, int radix);

const BuildGolferName_t kBuildGolferName = reinterpret_cast<BuildGolferName_t>(0x004676E0);
const ReplaceInText_t kReplaceInText = reinterpret_cast<ReplaceInText_t>(0x0045B7C0);
const TileBlocked_t kTileBlocked = reinterpret_cast<TileBlocked_t>(0x0040BF60);
const LandmarkName_t kLandmarkName = reinterpret_cast<LandmarkName_t>(0x004074A0);
const StartMessage_t kStartMessage = reinterpret_cast<StartMessage_t>(0x0040CB00);
const RandomRange_t kRange = reinterpret_cast<RandomRange_t>(0x0045C1E0);  // __thiscall, ecx = the RNG object
const LogTick_t kLogTick = reinterpret_cast<LogTick_t>(0x0040C6F0);
const PlaySound_t kPlaySound = reinterpret_cast<PlaySound_t>(0x004481B0);
const AppendNumber_t kAppendNumber = reinterpret_cast<AppendNumber_t>(0x0042DC00);
const AppendCents_t kAppendCents = reinterpret_cast<AppendCents_t>(0x0042DD50);
const AppendOpinion_t kAppendOpinion = reinterpret_cast<AppendOpinion_t>(0x00469A20);
const Clamp_t kClamp = reinterpret_cast<Clamp_t>(0x00467130);
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);

char* const kNumBuf = reinterpret_cast<char*>(0x0058A528);  // the shared __itoa scratch buffer

void* const kRng = reinterpret_cast<void*>(0x00822D9C);  // Random object the two call sites load into ecx

// Token strings replaced in the text buffer, referenced by address only (never reproduced; the repo is public):
const char* const kTokenMyName = reinterpret_cast<const char*>(0x004D395C);   // pushed at 0x0045c52d
const char* const kTokenPartner = reinterpret_cast<const char*>(0x004D3954);  // pushed at 0x0045c541
// Landmark-notice fragments, likewise by address only:
const char* const kNoticeHead = reinterpret_cast<const char*>(0x004E409C);    // loaded at 0x004723f4
const char* const kNoticeTail = reinterpret_cast<const char*>(0x004E4070);    // loaded at 0x00472439

// 0x0045c460  substituteStoryNames(g): rewrites the two name tokens of the text that is already in the shared buffer
// 0x0051a068. The buffer is first copied into a 1024-byte stack save (frame 0x480 at 0x0045c460; the save slot is
// esp+0x80, the two 64-byte name slots are esp+0x40 and esp+0x00). The buffer is then emptied (0x0045c49c) and
// buildGolferName 0x004676e0 is called with (g, 0) at 0x0045c4a3, so the buffer holds g's name, which is copied into
// the first name slot; the buffer is emptied again (0x0045c4d4) and buildGolferName is called with (g ^ 1, 0) at
// 0x0045c4db (the xor is at 0x0045c4bc), giving the second name. The saved text is copied back over the buffer
// (0x0045c529) and replaceInText 0x0045b7c0 is called twice: the token at 0x004d395c is replaced by g's name
// (0x0045c537) and the token at 0x004d3954 by (g ^ 1)'s name (0x0045c546). The function has no conditional branch
// (only the repne scasb / rep movs string loops); __cdecl, void, one argument at [esp+0x490] (0x0045c480).
typedef void(__cdecl* Substitute_t)(int);
Substitute_t SubstituteStoryNames_orig;
void __cdecl SubstituteStoryNames_re(int g) {
    char save[1024];
    char nameSelf[64];
    char namePartner[64];

    strcpy(save, kText);
    kText[0] = 0;
    kBuildGolferName(g, 0);
    strcpy(nameSelf, kText);

    kText[0] = 0;
    kBuildGolferName(g ^ 1, 0);
    strcpy(namePartner, kText);

    strcpy(kText, save);
    kReplaceInText(kTokenMyName, nameSelf);
    kReplaceInText(kTokenPartner, namePartner);
}

// 0x004722c0  LandmarkAvailableNotice(g, who): marks one landmark type as unlocked and appends its notice to the text
// buffer. The search starts at golfer g's world position, dwords 0x005794b8 + g*0x100 and 0x005794bc + g*0x100
// (0x004722ca/0x004722d0), each arithmetic-shifted right by 10 (0x004722d6/0x004722d9) to a tile (x, y). The tile
// index is x*50 + y (0x004722de..0x004722e4); the tile type byte is 0x005722e8 + index (0x004722e7). A tile is
// accepted when the byte at 0x00578376 + type*0x30 equals 4 (0x004722f6, jne at 0x004722fc), the type is not 0x16
// (0x004722fe, je at 0x00472301) and the word at 0x0053caf0 + index*2 has no bit of 0x320 set (0x00472303..0x00472314).
// Otherwise x and y each move by Random::range(3) - 1 (0x0045c1e0 with ecx = 0x00822d9c at 0x0047231d and 0x00472332,
// each result masked to 16 bits) and the search restarts, unless tileBlocked 0x0040bf60 returns non-zero for the new
// tile (0x00472342, jne at 0x0047234c), in which case the function returns having written nothing but the RNG seed.
// The landmark id comes from the first byte of row `who` of the 50-byte-row table 0x0053a454 (0x0047235e): the byte
// minus 0x41 indexes the switch at 0x0047236c (`ja` to the default at 0x004723c3 above 0x17) through the byte table
// 0x004724a8 and the dword table 0x0047247c. The mapping those two tables encode is A -> 0x16a, C -> 0x168,
// F -> 0x170, G -> 0x16f, H -> 0x16d, L -> 0x16c, M -> 0x16b, P -> 0x169, R -> 0x170, S -> 0x173, X -> 0x171, and
// every other byte -> Random::range(10) masked to 16 bits plus 0x168 (0x004723c3..0x004723d4). Bit (id - 0x168) is
// then set in both 0x00822c70 (0x00472400) and 0x00543cfc (0x0047240b), the string at 0x004e409c is appended to the
// buffer (0x00472411), landmarkName 0x004074a0 is called with (id - 0x168, 1) at 0x00472434, the string at 0x004e4070
// is appended (0x00472445) and startMessage 0x0040cb00 is called with (0x80000210, 0, -1) at 0x0047246e. __cdecl,
// void, two arguments at [esp+4] (0x004722c0) and [esp+0x14] (0x00472354).
typedef void(__cdecl* Landmark_t)(int, int);
Landmark_t LandmarkAvailableNotice_orig;
void __cdecl LandmarkAvailableNotice_re(int g, int who) {
    int x = ref<int>(0x005794B8 + g * 0x100) >> 10;
    int y = ref<int>(0x005794BC + g * 0x100) >> 10;

    for (;;) {
        const int index = x * 50 + y;
        const signed char type = ref<signed char>(0x005722E8 + index);
        if (ref<unsigned char>(0x00578376 + type * 0x30) == 4 && type != 0x16 &&
            (ref<unsigned short>(0x0053CAF0 + index * 2) & 0x320) == 0)
            break;
        x += (kRange(kRng, 0, 3) & 0xFFFF) - 1;
        y += (kRange(kRng, 0, 3) & 0xFFFF) - 1;
        if (kTileBlocked(x, y) != 0) return;
    }

    int id;
    switch (ref<signed char>(0x0053A454 + who * 50)) {
    case 'A': id = 0x16a; break;
    case 'C': id = 0x168; break;
    case 'F': id = 0x170; break;
    case 'G': id = 0x16f; break;
    case 'H': id = 0x16d; break;
    case 'L': id = 0x16c; break;
    case 'M': id = 0x16b; break;
    case 'P': id = 0x169; break;
    case 'R': id = 0x170; break;
    case 'S': id = 0x173; break;
    case 'X': id = 0x171; break;
    default:  id = (kRange(kRng, 0, 10) & 0xFFFF) + 0x168; break;
    }

    const unsigned bit = 1u << (id - 0x168);
    ref<unsigned>(0x00822C70) |= bit;
    ref<unsigned>(0x00543CFC) |= bit;
    appendText(kNoticeHead);
    kLandmarkName(id - 0x168, 1);
    appendText(kNoticeTail);
    kStartMessage(0x80000210, 0, -1);
}

// Shared by the three patron blocks: the refusal message that names the partner who quit (0x00426893,
// 0x00426ba4 and 0x00427022 differ only in the two strings they pass). Every address cited is from the first
// copy (0x00426893); the other two are the same instructions at 0x00426bcf / 0x0042704d (the mood test),
// 0x00426c0f / 0x0042708d (buildGolferName) and 0x00426c4e / 0x004270cc (appendOpinion).
void patronRefusal(int g, unsigned rec, unsigned head, unsigned tail) {
    appendText(str(head));
    if (ref<short>(0x0057955C + rec) > 2) {                         // 0x004268c8 (signed word compare)
        appendText(str(0x004C70C4));
        kBuildGolferName(ref<short>(0x0057955A + rec), 0);           // partner index, 0x004268ef
        appendText(str(0x004C70AC));
    }
    kAppendOpinion(ref<short>(0x0057955C + rec), 0);                 // 0x0042693d
    appendText(str(tail));
    kStartMessage(0x80006000, 1, g);                                 // 0x00426977
}

// 0x004266b0  patronEvent(g): builds and posts the message for the patron golfer `g` leaving the course, and
// applies its effect. The golfer record stride is 0x100 (shl ebp, 8 at 0x004266b8); the kind byte is
// 0x005794d0 + g*0x100 and its top three bits (& 0xe0, 0x004266c3) select one of three blocks, each of which
// first writes the patron's title and name into the shared buffer 0x0051a068 (strcpy of a fixed string plus
// buildGolferName 0x004676e0) and then branches on whether the golfer's current hole 0x005794d9 + g*0x100 is
// unbuilt, i.e. the par byte 0x00575ab0 + hole*0x208 is 0 (0x00426716, 0x004269d8, 0x00426cdd), and on the
// golfer's mood word 0x0057955c + g*0x100 being above 2 (0x0042672d, 0x004269f5, 0x00426cf7):
//   kind 0x60 (0x004266c5) clears bit 7 of 0x00572cac (0x00426701). With an unbuilt hole and mood > 2 it
//     invests when the hole index is above twice 0x00572cac (0x0042673e): logTick(0x100, kind & 0x1f)
//     (0x0042679d; eax is zeroed at 0x0042678d so only the five low bits reach it), an amount of
//     ((mood > 4) + 1) * 5000 / 100 (0x004267b1..0x004267d5) appended ten-fold by appendNumber 0x0042dc00
//     (0x00426809), startMessage(0x80001204, 1, g) (0x0042684c), the amount added to 0x00571fd4 (0x00426867)
//     and to the word 0x0058421e + (0x005a6d3c % 100) * 0x14 (0x0042685c..0x00426877), then playSound 0x19
//     (0x00426882). When the hole index is not above twice 0x00572cac it posts the short refusal
//     (startMessage(0x80006000, 1, g) at 0x00426775) and decrements 0x00572cac (0x00426783).
//   kind 0x40 (0x00426987) clears bit 7 of 0x0053a450 (0x004269df). With an unbuilt hole and mood > 2 it
//     appends a figure of (0x00822c88 + 2) * (0x0053a450 + 3) * 50, through __itoa 0x004ad425 into 0x0058a528
//     when 0x00822c88 <= 1 and through appendCents 0x0042dd50 otherwise (0x00426a5b), sets 0x00569498 to 0
//     (0x00426b6b) and 0x005a9ccc to 3 (0x00426b72), calls startMessage(0x80001204, 1, g) (0x00426b7c) and
//     playSound 0x2f (0x00426b8b), and sets bit 0 of 0x00567a1c (0x00426b9a).
//   kind 0x80 (0x00426c98). With an unbuilt hole and mood > 2 it picks a landmark index: starting from the mood
//     it draws Random::range(counter) (0x0045c1e0 with ecx = 0x00822d9c at 0x00426d0a), clamps the low 16 bits
//     to 0..0xf (0x00467130 at 0x00426d15) and stops as soon as that bit is clear in 0x00543cfc (0x00426d2f),
//     incrementing the counter up to 0x18 (0x00426d35). It then builds the message with landmarkName 0x004074a0
//     twice (0x00426d67 with 1, 0x00426e4b with 0), one of four effect sentences chosen by index & 3 through the
//     jump table 0x00427370 (0x00426da6), a price of (index * 5 + 5) * 200 through appendNumber (0x00426e8b),
//     and a figure of (0x0059aaf8 + 1)^2 * 25 after incrementing 0x0059aaf8 (0x00426ecf), again through __itoa
//     when 0x00822c88 <= 1 and appendCents otherwise (0x00426ed5). It sets 0x00569498 to 0 (0x00426fa4), calls
//     startMessage(0x80001204, 1, g) (0x00426fd2), playSound 0x2f (0x00426fe1) and logTick(0x160, index)
//     (0x00426fec), and sets bit `index` in 0x00543cfc and 0x00822c70 (0x00427011, 0x00427017).
// Each block's other case posts the refusal that names the partner who quit when the mood is above 2
// (0x00426893, 0x00426ba4, 0x00427022). A fourth block at 0x00427124 is unreachable: 0x00427114 masks eax with
// 0xe0, so the `cmp eax, 0x100` at 0x00427119 never holds and the `jne` at 0x0042711e always skips it. The tail
// (0x00427345) zeroes the dword 0x0057958c + g*0x100, sets the hole byte 0x005794d9 + g*0x100 to 0x13, and sets
// 0x005a59f8 to -1 when bit 0x200 of the flags 0x005794c8 + g*0x100 is set (`test ah, 2` at 0x0042735f).
// __cdecl, void, one argument at [esp+8] (0x004266b1).
typedef void(__cdecl* Patron_t)(int);
Patron_t PatronEvent_orig;
void __cdecl PatronEvent_re(int g) {
    const unsigned rec = static_cast<unsigned>(g) * 0x100;
    const unsigned char kind = ref<unsigned char>(0x005794D0 + rec);

    if ((kind & 0xe0) == 0x60) {                                     // Corporate CEO block
        strcpy(kText, str(0x004C71B0));
        kBuildGolferName(g, 0);
        ref<unsigned>(0x00572CAC) &= 0xFFFFFF7F;                     // `and al, 0x7f` at 0x00426701
        const int hole = ref<signed char>(0x005794D9 + rec);         // 0x00426708
        if (ref<signed char>(0x00575AB0 + hole * 0x208) != 0 || ref<short>(0x0057955C + rec) <= 2) {
            patronRefusal(g, rec, 0x004C70D0, 0x004C70A0);
        } else if (hole > ref<int>(0x00572CAC) * 2) {                // 0x0042673e (signed)
            kLogTick(0x100, kind & 0x1f);                            // 0x0042679d
            const int amount = ((ref<short>(0x0057955C + rec) > 4) + 1) * 5000 / 100;
            appendText(str(0x004C7124));
            kAppendNumber(amount * 100);                             // 0x00426809
            appendText(str(0x004C7100));
            ref<unsigned char>(0x00569498) = 0;                      // 0x0042681e (al is 0 after the strlen)
            kStartMessage(0x80001204, 1, g);                         // 0x0042684c
            ref<int>(0x00571FD4) += amount;                          // 0x00426867
            const int slot = ref<short>(0x005A6D3C) % 100;           // idiv 100 at 0x00426870
            ref<short>(0x0058421E + slot * 0x14) += static_cast<short>(amount);   // 0x00426877
            kPlaySound(0x19, 100, 0, 0, 0);                          // 0x00426882
        } else {
            appendText(str(0x004C715C));
            kStartMessage(0x80006000, 1, g);                         // 0x00426775
            ref<int>(0x00572CAC) -= 1;                               // 0x00426783
        }
    }

    if ((kind & 0xe0) == 0x40) {                                     // County commissioner block
        strcpy(kText, str(0x004C7088));
        kBuildGolferName(g, 0);
        const int hole = ref<signed char>(0x005794D9 + rec);         // 0x004269bb
        ref<unsigned>(0x0053A450) &= 0xFFFFFF7F;                     // 0x004269df
        if (ref<signed char>(0x00575AB0 + hole * 0x208) != 0 || ref<short>(0x0057955C + rec) <= 2) {
            patronRefusal(g, rec, 0x004C6FC8, 0x004C6FB8);
        } else {
            appendText(str(0x004C7058));
            appendText(str(0x004C7038));
            if (ref<int>(0x00822C88) > 1) {                          // 0x00426a5b
                appendText(str(0x004C700C));
                kAppendCents((ref<int>(0x00822C88) + 2) * (ref<int>(0x0053A450) + 3) * 50);   // 0x00426b30
            } else {
                appendText(str(0x004C7024));
                kItoa((ref<int>(0x00822C88) + 2) * (ref<int>(0x0053A450) + 3) * 50, kNumBuf, 10);  // 0x00426ab2
                appendText(kNumBuf);
            }
            appendText(str(0x004C7000));
            ref<unsigned char>(0x00569498) = 0;                      // 0x00426b6b
            ref<int>(0x005A9CCC) = 3;                                // 0x00426b72
            kStartMessage(0x80001204, 1, g);                         // 0x00426b7c
            kPlaySound(0x2f, 100, 0, 0, 0);                          // 0x00426b8b
            ref<unsigned>(0x00567A1C) |= 1;                          // 0x00426b9a
        }
    }

    if ((kind & 0xe0) == 0x80) {                                     // Wealthy Heiress block
        strcpy(kText, str(0x004C6FA4));
        kBuildGolferName(g, 0);
        const int hole = ref<signed char>(0x005794D9 + rec);         // 0x00426ccc
        if (ref<signed char>(0x00575AB0 + hole * 0x208) != 0 || ref<short>(0x0057955C + rec) <= 2) {
            patronRefusal(g, rec, 0x004C6DC0, 0x004C6D90);
        } else {
            int counter = ref<short>(0x0057955C + rec);              // 0x00426cec
            int lm;
            for (;;) {
                lm = kClamp(kRange(kRng, 0, counter) & 0xFFFF, 0, 0xf);       // 0x00426d0a, 0x00426d15
                if (((1u << (lm & 0x1f)) & ref<unsigned>(0x00543CFC)) == 0) break;   // 0x00426d2f
                ++counter;
                if (counter > 0x18) break;                           // 0x00426d35
            }
            appendText(str(0x004C6F88));
            kLandmarkName(lm, 1);                                    // 0x00426d67
            appendText(str(0x004C6F54));
            switch (lm & 3) {                                        // jump table 0x00427370
            case 0: appendText(str(0x004C6F30)); break;
            case 1: appendText(str(0x004C6F14)); break;
            case 2: appendText(str(0x004D6098)); appendText(str(0x004C6EF4)); break;
            case 3: appendText(str(0x004C6ECC)); break;
            }
            appendText(str(0x004C6E78));
            kLandmarkName(lm, 0);                                    // 0x00426e4b
            appendText(str(0x004C6E54));
            kAppendNumber((lm * 5 + 5) * 200);                       // 0x00426e8b
            appendText(str(0x004C6E38));
            const int diff = ref<int>(0x00822C88);                   // 0x00426eb9
            ref<int>(0x0059AAF8) += 1;                               // 0x00426ecf
            if (diff > 1) {                                          // 0x00426ed5
                appendText(str(0x004C6E08));
                const int n = ref<int>(0x0059AAF8) + 1;              // 0x00426f6d
                kAppendCents(n * n * 25);                            // 0x00426f8c
            } else {
                appendText(str(0x004C6E20));
                const int n = ref<int>(0x0059AAF8) + 1;              // 0x00426efc
                kItoa(n * n * 25, kNumBuf, 10);                      // 0x00426f1b
                appendText(kNumBuf);
            }
            appendText(str(0x004C6DF8));
            ref<unsigned char>(0x00569498) = 0;                      // 0x00426fa4
            kStartMessage(0x80001204, 1, g);                         // 0x00426fd2
            kPlaySound(0x2f, 100, 0, 0, 0);                          // 0x00426fe1
            kLogTick(0x160, lm);                                     // 0x00426fec
            const unsigned bit = 1u << (lm & 0x1f);                  // 0x00427002
            ref<unsigned>(0x00543CFC) |= bit;                        // 0x00427011
            ref<unsigned>(0x00822C70) |= bit;                        // 0x00427017
        }
    }

    ref<int>(0x0057958C + rec) = 0;                                  // 0x00427345
    ref<unsigned char>(0x005794D9 + rec) = 0x13;                     // 0x0042734f
    if (ref<unsigned>(0x005794C8 + rec) & 0x200) ref<int>(0x005A59F8) = -1;   // 0x0042735f, 0x00427365
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0045C460, substituteStoryNames, SubstituteStoryNames_re, SubstituteStoryNames_orig);
SG_HOOK("golf_clean.exe", 0x004266B0, patronEvent, PatronEvent_re, PatronEvent_orig);
SG_HOOK("golf_clean.exe", 0x004722C0, LandmarkAvailableNotice, LandmarkAvailableNotice_re, LandmarkAvailableNotice_orig);
