// C3 batch c3ah (2026-10-08) of golf_clean.exe: course / economy event builders. Hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; every body cites the
// address of each global, record field and callee it uses. Callees are invoked through their original addresses, so a
// hooked callee runs its own reimplementation in both A/B arms; the fixed global buffers they write (the shared text
// buffer 0x0051a068, the itoa scratch 0x0058a528, the message ticker globals, the RNG seed 0x00822d9c, the tile
// tables and the heightfield) are declared as A/B state regions in re/frida/registry.d/c3ah.py.
#include "hooks.h"
#include <string.h>
#include <math.h>

namespace {

template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }

char* const kText = reinterpret_cast<char*>(0x0051A068);   // shared text buffer
char* const kNumBuf = reinterpret_cast<char*>(0x0058A528);  // shared itoa scratch

typedef void(__cdecl* Void1_t)(int);
typedef void(__cdecl* Void2_t)(int, int);
typedef int(__cdecl* Int2_t)(int, int);
typedef void(__cdecl* Void4_t)(int, int, int, int);
typedef void(__cdecl* Void_t)();
typedef int(__cdecl* Int3_t)(int, int, int);
typedef void(__cdecl* PlaySound_t)(int, int, int, int, int);
typedef char*(__cdecl* Itoa_t)(int, char*, int);
typedef int(__fastcall* RandomRange_t)(void* self, void* edx, int n);

const Void1_t kAppendCourseTitle = reinterpret_cast<Void1_t>(0x0040DAA0);  // appendCourseTitle
const Int2_t kRateLot = reinterpret_cast<Int2_t>(0x0042EF40);              // rateLot
const Void4_t kPointsPopup = reinterpret_cast<Void4_t>(0x0040C890);        // pointsPopup
const Void2_t kFreeRecordAtPos = reinterpret_cast<Void2_t>(0x004011B0);    // freeRecordAtPos
const Void_t kRebuildHeightfield = reinterpret_cast<Void_t>(0x0042F7A0);   // rebuildHeightfield
const Void2_t kBuildGolferName = reinterpret_cast<Void2_t>(0x004676E0);    // buildGolferName
const Int3_t kStartMessage = reinterpret_cast<Int3_t>(0x0040CB00);         // startMessage
const PlaySound_t kPlaySound = reinterpret_cast<PlaySound_t>(0x004481B0);  // playSound
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);                 // __itoa
const RandomRange_t kRange = reinterpret_cast<RandomRange_t>(0x0045C1E0);  // Random::range, ecx = the RNG object
void* const kRng = reinterpret_cast<void*>(0x00822D9C);                    // the global Random object (its seed)

typedef int(__cdecl* Int1_t)(int);
const Int1_t kTypeBit7Clear = reinterpret_cast<Int1_t>(0x0046C940);        // typeBit7Clear
const Void2_t kLogTick = reinterpret_cast<Void2_t>(0x0040C6F0);            // logTick
typedef void(__cdecl* Void3_t)(int, int, int);
const Void3_t kPostEvent = reinterpret_cast<Void3_t>(0x0046E7B0);          // postEvent
const Int2_t kAngleFixed = reinterpret_cast<Int2_t>(0x004672D0);           // angleFixed
const Void1_t kAppendUpgradeText = reinterpret_cast<Void1_t>(0x0040E5F0);  // appendUpgradeText

// ------------------------------------------------------------------------------------- 0x004732d0 addScore

// One scoreboard row, 0x9c bytes; the table is the 10 rows at 0x00541ce0. The field offsets are fixed by the stores
// at 0x004733de (cash +0x88), 0x004733eb (fun +0x80), 0x004733f6 (skill +0x84), 0x004733fc (level word +0x96) and
// 0x00473403 (id +0x98), and by the two strcpy targets 0x00541ce0 (name) and 0x00541d20 (text, +0x40).
struct ScoreRow {
    char name[0x40];
    char text[0x40];
    int fun;      // +0x80
    int skill;    // +0x84
    int cash;     // +0x88
    char gap[0x96 - 0x8c];
    short level;  // +0x96
    int id;       // +0x98
};

// 0x004732d0  addScore(): inserts the finished course into the 10-row scoreboard at 0x00541ce0 and returns its row,
// or -1 when it does not place. __cdecl, no arguments, returns int.
typedef int(__cdecl* AddScore_t)();
AddScore_t AddScore_orig;
int __cdecl AddScore_re() {
    ScoreRow* const rows = reinterpret_cast<ScoreRow*>(0x00541CE0);
    const int id = ref<int>(0x00822C78);  // read at 0x0047333d / 0x00473373 / 0x004733f1

    // score = (cash/10 + skill rating + fun rating) * (level + 1)   (0x004732d0..0x004732fc)
    const int score = (ref<int>(0x00571FD4) / 10 + ref<int>(0x00541CD8) + ref<int>(0x0059AE78)) *
                      (ref<int>(0x00822C88) + 1);
    ref<int>(0x008392A8) = -1;  // 0x00473303

    // Find the first row this score beats (0x00473314..0x00473357). A row carrying the current course id before
    // that point refuses the insert (0x00473348); so does running past row 9 (0x00473351, limit 0x0054237c).
    int i = 0;
    for (;;) {
        const int rowScore = (rows[i].cash / 10 + rows[i].fun + rows[i].skill) * (rows[i].level + 1);
        if (score >= rowScore) break;                 // 0x0047333b
        if (rows[i].id == id) return -1;              // 0x00473348
        if (++i == 10) return -1;                     // 0x00473357 falls through to 0x0047335b (or eax, -1)
    }

    // The lowest row at or below i that already holds this course id is overwritten instead of row 9
    // (0x0047336c..0x00473388; the scan walks the id field backwards from row 9).
    int slot = 9;
    for (int j = 9; j >= i; --j)
        if (rows[j].id == id) slot = j;

    // Shift rows [i, slot) down one place (0x0047339f..0x004733b1, 0x27 dwords = one 0x9c row per step).
    for (int j = slot; j > i; --j)
        rows[j] = rows[j - 1];

    kText[0] = 0;                                     // 0x004733d7
    rows[i].cash = ref<int>(0x00571FD4);              // 0x004733de
    rows[i].fun = ref<int>(0x0059AE78);               // 0x004733eb
    rows[i].skill = ref<int>(0x00541CD8);             // 0x004733f6
    rows[i].level = ref<short>(0x00822C88);           // 0x004733fc (the low word of the level dword)
    rows[i].id = id;                                  // 0x00473403
    strcpy(rows[i].name, reinterpret_cast<const char*>(0x004D6098));  // 0x00473414..0x0047342a
    kAppendCourseTitle(0);                            // 0x0047342c, argument pushed at 0x004733d5
    ref<int>(0x008392A8) = i;                         // 0x00473450
    strcpy(rows[i].text, kText);                      // 0x0047343e..0x00473462
    return i;
}

// ------------------------------------------------------------------- 0x0040e400 clearObjectFootprint

// 0x0040e400  clearObjectFootprint(index): removes the placed object `index` (record 0x10 bytes at 0x0058bcb8) and
// resets the tiles it covered. __cdecl, void.
typedef void(__cdecl* Clear_t)(int);
Clear_t ClearObjectFootprint_orig;
void __cdecl ClearObjectFootprint_re(int index) {
    const unsigned rec = 0x0058BCB8 + index * 0x10;          // 0x0040e40b (shl edi, 4)
    const int type = ref<short>(rec);                        // 0x0040e40e
    const int y = ref<short>(rec + 4);                       // 0x0040e415
    const int x = ref<short>(rec + 2);                       // 0x0040e41c

    if (type == 5) {                                         // 0x0040e423: a lot is refunded
        const int rating = kRateLot(x, y);                   // 0x0040e432
        const int cost = ref<int>(rec + 8);                  // 0x0040e437
        // rating/2 (0x0040e43f cdq/sub/sar) + cost/50 (0x0040e444 magic 0x51eb851f, 0x0040e455 sar 4 + sign fix)
        const int refund = cost / 50 + rating / 2;
        ref<int>(0x00571FD4) -= refund;                      // cash, 0x0040e467
        kPointsPopup(-refund, x * 0x400 + 0x200, y * 0x400 + 0x200, -1);  // 0x0040e488
        // Revenue row of the current date: 0x00584212 + (date % 100) * 0x14 (0x0040e48d..0x0040e4a2).
        ref<short>(0x00584212 + (ref<short>(0x005A6D3C) % 100) * 0x14) -= static_cast<short>(refund);
        if (cost != 0) kFreeRecordAtPos(x, y);               // 0x0040e4b2 / 0x0040e4c4
    }

    ref<unsigned short>(rec) = 0xffff;                       // 0x0040e4cf: the slot is free
    if (type == 0xf) ref<int>(0x0059E7B8) |= 0x800;          // 0x0040e4cc / 0x0040e4df

    // Footprint side: the signed byte at 0x004c26c0 + type*0x14 (0x0040e4f5), widened for types above 5 other than
    // 7 by the dword at 0x005a8c38 + type*4 minus one (0x0040e50f..0x0040e529).
    int side = ref<signed char>(0x004C26C0 + type * 0x14);
    const int cell0 = y + x * 50;                            // 0x0040e4eb..0x0040e4ff
    ref<unsigned short>(0x0053CAF0 + cell0 * 2) &= 0xfde0;   // 0x0040e505
    if (type >= 6 && type != 7)                              // 0x0040e512 / 0x0040e517 (the `type != 5` test at
        side += ref<int>(0x005A8C38 + type * 4) - 1;         // 0x0040e519 can no longer be true here)

    if (side > 0) {                                          // 0x0040e531
        // The replacement tile type is loop-invariant: 0x11 for type 0xc on course kinds 0 and 2 (0x0040e548..
        // 0x0040e55a), else 4 plus one when the current course index 0x0059bf90 is at least 0xc (0x0040e561).
        unsigned char fill;
        const unsigned char kind = ref<unsigned char>(0x005A34E0);
        if (type == 0xc && (kind == 0 || kind == 2)) fill = 0x11;
        else fill = static_cast<unsigned char>((ref<int>(0x0059BF90) >= 0xc) + 4);

        int rowBase = x * 50;                                // 0x0040e533
        for (int r = 0; r < side; ++r) {                     // outer 0x0040e599
            for (int c = 0; c < side; ++c) {                 // inner 0x0040e58b
                const int cell = rowBase + y + c;            // 0x0040e571
                ref<unsigned short>(0x0053CAF0 + cell * 2) &= 0xfbdf;  // 0x0040e574 (mask 0xfffffbdf)
                ref<unsigned char>(0x005722E8 + cell) = fill;          // 0x0040e57e
                ref<unsigned char>(0x0056988C + cell) = 0;             // 0x0040e584
            }
            rowBase += 50;                                   // 0x0040e591
        }
    }

    kRebuildHeightfield();                                   // 0x0040e59b
}

// --------------------------------------------------------------------- 0x00406670 membershipReport

// Source strings, referenced by address only (never reproduced; the repo is public).
const char* const kApplies = reinterpret_cast<const char*>(0x004C48E8);
const char* const kUpgrade = reinterpret_cast<const char*>(0x004C4924);
const char* const kTier3Word = reinterpret_cast<const char*>(0x004C48F8);
const char* const kTier4Word = reinterpret_cast<const char*>(0x004C4908);
const char* const kTier5Word = reinterpret_cast<const char*>(0x004C4914);
const char* const kAtYourClub = reinterpret_cast<const char*>(0x004C48CC);
const char* const kBigBucks = reinterpret_cast<const char*>(0x004C4884);
const char* const kFeeLead = reinterpret_cast<const char*>(0x004C483C);
const char* const kFeeTail = reinterpret_cast<const char*>(0x004C481C);
const char* const kJoinA = reinterpret_cast<const char*>(0x004C4818);
const char* const kJoinB = reinterpret_cast<const char*>(0x004C4810);
const char* const kJoinMid = reinterpret_cast<const char*>(0x004C47F0);
const char* const kJoinC = reinterpret_cast<const char*>(0x004C47E4);
const char* const kJoinD = reinterpret_cast<const char*>(0x004C47D8);
const char* const kJoinTail = reinterpret_cast<const char*>(0x004C47C0);
const char* const kCurrentHdr = reinterpret_cast<const char*>(0x004C47A4);
const char* const kResigned = reinterpret_cast<const char*>(0x004C4798);
const char* const kBasicWord = reinterpret_cast<const char*>(0x004C4790);
const char* const kMembersPl = reinterpret_cast<const char*>(0x004C4784);
const char* const kMembersSg = reinterpret_cast<const char*>(0x004C4780);
const char* const kCountTail = reinterpret_cast<const char*>(0x004C4778);

// 0x00406670  membershipReport(golfer): builds the membership-application / current-membership report text into
// 0x0051a068 and hands it to the message ticker. __cdecl, void, one argument.
typedef void(__cdecl* Member_t)(unsigned);
Member_t MembershipReport_orig;
void __cdecl MembershipReport_re(unsigned golfer) {
    kText[0] = 0;                                            // 0x00406673
    const unsigned grec = golfer * 0x100;                    // 0x00406682 (shl eax, 8)
    ref<int>(0x005794C8 + grec) &= 0x7fffffff;               // 0x00406687..0x00406696

    // The golfer's membership-type slot: the word at 0x0057956e + golfer*0x100 indexes the 0x2c-byte type table
    // 0x005849e0; its tier byte (+2) is bumped by one (0x0040669c..0x004066b2) and the low 3 bits select the text.
    const int slot = ref<short>(0x0057956E + grec);
    unsigned char& tierByte = ref<unsigned char>(0x005849E2 + slot * 0x2c);
    tierByte = static_cast<unsigned char>(tierByte + 1);
    const int tier = tierByte & 7;                           // 0x004066bb

    kBuildGolferName(static_cast<int>(golfer), 0);           // 0x004066c0: writes the name into kText

    const char* lead;
    if (tier < 3) {                                          // 0x004066cb
        lead = kApplies;
    } else {
        strcat(kText, kUpgrade);                             // 0x004066cd block
        if (tier == 3) strcat(kText, kTier3Word);            // 0x004066ff
        else if (tier == 4) strcat(kText, kTier4Word);       // 0x00406702
        else if (tier == 5) strcat(kText, kTier5Word);       // 0x00406705
        lead = ref<const char*>(0x004C2A88 + tier * 4);      // 0x00406742: tier-name pointer table
    }
    strcat(kText, lead);                                     // 0x00406750 block
    strcat(kText, kAtYourClub);                              // 0x00406778 block

    if (tier == 3) {                                         // 0x004067a6
        strcat(kText, ref<const char*>(0x004C2A94));         // 0x004067a8 (the tier-3 entry of the same table)
        strcat(kText, kBigBucks);                            // 0x004067d3 -> the shared strcat at 0x00406889
    } else if (tier > 3) {                                   // 0x004067dd
        strcat(kText, ref<const char*>(0x004C2A88 + tier * 4));  // 0x004067e3
        strcat(kText, kFeeLead);                             // 0x0040681c
        // The surcharge digit is 2 for tier 4 and 5 otherwise (0x00406845: neg/sbb/and 3/add 2 over tier - 4).
        strcat(kText, kItoa(tier != 4 ? 5 : 2, kNumBuf, 10));  // 0x00406852
        strcat(kText, kFeeTail);                             // 0x00406884
    }

    // Pick a free membership slot: Random::range(0x4b) + 1 until the tier byte of that slot is 0, at most 1000
    // tries (0x004068b3..0x004068e3). A full table leaves the join sentence out.
    int tries = 0;
    int pick = -1;
    for (;;) {
        const int cand = (kRange(kRng, 0, 0x4b) & 0xffff) + 1;   // 0x004068ba..0x004068c8
        if (ref<unsigned char>(0x005849E2 + cand * 0x2c) == 0) { pick = cand; break; }  // 0x004068da
        if (++tries >= 1000) break;                          // 0x004068e3
    }
    if (pick >= 0) {
        // (the `tries >= 1000` retest at 0x004068f0 can never fire: the loop above leaves tries <= 999 here)
        ref<unsigned char>(0x005849E2 + pick * 0x2c) = 1;    // 0x004068ff
        strcat(kText, kTypeBit7Clear(static_cast<int>(golfer)) != 0 ? kJoinA : kJoinB);  // 0x00406907/0x00406916
        strcat(kText, kJoinMid);                             // 0x00406946
        strcat(kText, kTypeBit7Clear(static_cast<int>(golfer)) != 0 ? kJoinC : kJoinD);  // 0x00406971/0x00406980
        strcat(kText, reinterpret_cast<const char*>(0x004D6098 + pick * 0x230));  // 0x004069aa..0x004069c1
        strcat(kText, kJoinTail);                            // 0x004069ec
    }

    strcat(kText, kCurrentHdr);                              // 0x00406a17

    // Per-tier census of the membership table: 76 entries of 0x2c from 0x005849e0 (the walk runs over the word at
    // +0x2a from 0x00584a0a to 0x0058571a, 0x00406a66..0x00406a93). `present` counts entries whose word is set,
    // `count` those whose tier byte (+2) matches, plus the banned ones (+0x29 == -1) when the tier is -1.
    int unused = 0x64;                                       // [esp+0x14] at 0x00406a21: accumulated, never read
    for (int t = 5; t >= -1; --t) {
        if (t == 0) continue;                                // 0x00406a53
        int present = 0, count = 0;
        for (unsigned p = 0x00584A0A; p < 0x0058571A; p += 0x2c) {
            if (ref<short>(p) != 0) {                        // 0x00406a6c
                ++present;
                if ((ref<unsigned char>(p - 0x28) & 7) == static_cast<unsigned>(t)) ++count;  // 0x00406a7c
            }
            if (ref<signed char>(p - 1) == -1 && t == -1) ++count;  // 0x00406a83 / 0x00406a88
        }
        if (count == 0) continue;                            // 0x00406a9b
        if (t == -1) {
            strcat(kText, kResigned);                        // 0x00406aa6
        } else {
            strcat(kText, ref<const char*>(0x004C2A88 + t * 4));  // 0x00406aad
            if (t == 2) {                                    // 0x00406adf
                strcat(kText, kBasicWord);                   // 0x00406ae1
                strcat(kText, kMembersPl);                   // 0x00406b0c
            } else if (t < 2) {
                strcat(kText, kMembersSg);                   // 0x00406b1c
            } else {
                strcat(kText, kMembersPl);                   // 0x00406b15
            }
        }
        strcat(kText, kItoa(count, kNumBuf, 10));            // 0x00406b51
        strcat(kText, kCountTail);                           // 0x00406b83
        unused += (-100 * count) / present;                  // 0x00406b9f..0x00406bc2 (present >= 1 whenever
    }                                                        //   count came from the word-set arm)
    (void)unused;

    if ((ref<unsigned char>(0x005A5A00) & 8) != 0) {         // 0x00406bd8
        if (ref<int>(0x004C2E0C) == static_cast<int>(golfer | 0x100)) {  // 0x00406bee
            ref<int>(0x005A9CCC) = 3;                        // 0x00406bf0
            ref<unsigned char>(0x00569498) = 0;              // 0x00406bfa
        }
        if (kStartMessage(static_cast<int>(0x80004010), 0, static_cast<int>(golfer)) != 0)  // 0x00406c09
            kPlaySound(0x38, 0x46, 0, 0, 0);                 // 0x00406c1f
    }
}

// ---------------------------------------------------------------------- 0x0040e720 announceBuilding

// 0x0040e720  announceBuilding(): closes the hole currently being built (its index is the dword 0x005685f0),
// derives its length and par, builds the "hole is now open" notice and, when a new building has become
// available, its description; then renumbers the hole counter. __cdecl, no arguments, void.
typedef void(__cdecl* Void0_t)();
Void0_t AnnounceBuilding_orig;
void __cdecl AnnounceBuilding_re() {
    const int hole = ref<int>(0x005685F0);     // 0x0040e720
    const unsigned rec = hole * 0x208;         // 0x0040e72b (shl 6 / add / shl 3)

    // Refused when the hole has no tee (0x00575ab8) or no pin (0x00575ac8), or - with bit 0x40 of the global
    // flags 0x0059e7b8 set - no third point 0x00575ac0 (0x0040e739..0x0040e760).
    if (ref<int>(0x00575AB8 + rec) == 0 || ref<int>(0x00575AC8 + rec) == 0 ||
        ((ref<unsigned char>(0x0059E7B8) & 0x40) != 0 && ref<int>(0x00575AC0 + rec) == 0)) {
        kPlaySound(0x18, 100, 0, 0, 0);        // 0x0040f14a
        return;
    }

    for (int i = 0; i < 10; ++i) ref<int>(0x0056A524 + i * 4) = 0;   // 0x0040e777 (rep stosd, 10 dwords)
    kLogTick(0x20, hole);                                            // 0x0040e77b

    char shape[16];
    strcpy(shape, reinterpret_cast<const char*>(0x004C5934));        // default shape word, 0x0040e780
    if (ref<int>(0x00834170) != 0) kPlaySound(0x2a, 100, 0, 0, 0);   // 0x0040e7b0 / 0x0040e7bc

    // Length: when the yards word 0x00575ab4 is still 0 it is 25 * the tee-to-pin distance, computed as
    // sqrt(625 * (dx*dx + dy*dy)) with fild/fsqrt and truncated by __ftol 0x004a6030 (0x0040e7de..0x0040e833);
    // only the low word is kept. Above 250 a quarter of the excess is added (0x0040e83c..0x0040e84f).
    if (ref<short>(0x00575AB4 + rec) == 0) {                         // 0x0040e7d4
        int dx = ref<int>(0x00575AB8 + rec) - ref<int>(0x00575AC8 + rec);
        if (dx < 0) dx = -dx;                                        // 0x0040e7ec (cdq / xor / sub)
        int dy = ref<int>(0x00575ABC + rec) - ref<int>(0x00575ACC + rec);
        if (dy < 0) dy = -dy;                                        // 0x0040e801
        const int sq = (dy * dy + dx * dx) * 625;                    // 0x0040e808..0x0040e819 (four lea x5)
        const short len = static_cast<short>(static_cast<int>(static_cast<long long>(sqrt(static_cast<double>(sq)))));
        ref<short>(0x00575AB4 + rec) = len;                          // 0x0040e833
        if (len > 250)                                               // 0x0040e82e (signed word compare)
            ref<short>(0x00575AB4 + rec) = static_cast<short>((static_cast<int>(len) - 250) / 4 + len);
    }

    int yards = ref<short>(0x00575AB4 + rec);                        // 0x0040e85c (sign-extended)
    if ((ref<unsigned char>(0x00575CB0 + rec) & 0x60) != 0 && yards > 250) yards += 25;   // 0x0040e863/0x0040e86d
    if (yards > 300) yards -= 25 * ref<int>(0x005A8C60);             // 0x0040e878..0x0040e88c

    ref<unsigned char>(0x00575AB0 + rec) = 3;                        // 0x0040e892 (par, default 3)
    if (yards <= 0x32) ref<unsigned char>(0x00575AB0 + rec) = 2;     // 0x0040e899
    if (yards >= 0xfa) ref<unsigned char>(0x00575AB0 + rec) = 4;     // 0x0040e8a8
    if (yards >= 0x1db) ref<unsigned char>(0x00575AB0 + rec) = 5;    // 0x0040e8b7
    if (yards > 0x271) ref<unsigned char>(0x00575AB0 + rec) = 6;     // 0x0040e8c6

    if (ref<int>(0x00822C88) < 2) {                                  // 0x0040e8d6 (difficulty gate)
        const int px = ref<int>(0x00575AC8 + rec) * 0x400 + 0x200;
        const int py = ref<int>(0x00575ACC + rec) * 0x400 + 0x200;
        if (ref<unsigned char>(0x00575CB0 + rec) & 0x40) kPostEvent(0, px, py);   // 0x0040e8e3 / 0x0040e906
        if (ref<unsigned char>(0x00575CB0 + rec) & 0x20) kPostEvent(1, px, py);   // 0x0040e925 / 0x0040e949
        if (ref<signed char>(0x00575AB0 + rec) == 5) kPostEvent(4, px, py);       // 0x0040e968 / 0x0040e98c
    }

    // Shape word: "short" below the par band's lower bound, "long" above its upper bound (0x0040e9a4..0x0040ea9b).
    const signed char par = ref<signed char>(0x00575AB0 + rec);
    if (par == 3) {                                                                   // 0x0040e9ae
        if (yards < 0x7d) strcpy(shape, reinterpret_cast<const char*>(0x004C592C));   // 0x0040ea41
        if (yards > 0xc8) strcpy(shape, reinterpret_cast<const char*>(0x004C5924));   // 0x0040ea75
    } else if (par == 4) {                                                            // 0x0040e9b5
        if (yards < 0x15e) strcpy(shape, reinterpret_cast<const char*>(0x004C592C));  // 0x0040ea06
        if (yards > 0x1a9) strcpy(shape, reinterpret_cast<const char*>(0x004C5924));  // 0x0040ea3a
    } else if (par == 5) {                                                            // 0x0040e9b8
        if (yards < 0x1f4) strcpy(shape, reinterpret_cast<const char*>(0x004C592C));  // 0x0040e9c4
        if (yards > 0x23f) strcpy(shape, reinterpret_cast<const char*>(0x004C5924));  // 0x0040e9f8
    }

    // Reset the hole's statistics: four dwords and two words (0x0040eaa5..0x0040eac4), the 10x8 shot grid at
    // 0x00575ad8 (outer step 2, inner step 0x16, 0x0040ead0..0x0040eae6) and the 0x20 dwords at 0x00575b88.
    ref<int>(0x00575CA4 + rec) = 0;
    ref<int>(0x00575AD4 + rec) = 0;
    ref<int>(0x00575AD0 + rec) = 0;
    ref<int>(0x00575C9C + rec) = 0;
    ref<short>(0x00575C0A + rec) = 0;
    ref<short>(0x00575C08 + rec) = 0;
    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 8; ++c) ref<short>(0x00575AD8 + rec + r * 2 + c * 0x16) = 0;
    for (int i = 0; i < 0x20; ++i) ref<int>(0x00575B88 + rec + i * 4) = 0;

    // Orientation bucket: angleFixed(pin - third point) folded into 0..7 (0x0040eb01..0x0040eb3b).
    const int ang = kAngleFixed(ref<int>(0x00575AC8 + rec) - ref<int>(0x00575AC0 + rec),
                                ref<int>(0x00575ACC + rec) - ref<int>(0x00575AC4 + rec));
    ref<unsigned char>(0x00575AB2 + rec) = static_cast<unsigned char>(((((ang >> 0x1c) & 0xf) + 1) >> 1) & 7);

    // The tee tile: flag bit 0x4000 of the word at 0x0053caf0 and the byte at 0x00578804 (0x0040eb41..0x0040eb61).
    const int cell = ref<int>(0x00575ABC + rec) + ref<int>(0x00575AB8 + rec) * 50;
    ref<unsigned char>(0x0053CAF1 + cell * 2) |= 0x40;
    ref<unsigned char>(0x00578804 + cell) = 0;

    strcpy(kText, reinterpret_cast<const char*>(0x004C591C));                 // 0x0040eb79 (strcpy, not strcat)
    strcat(kText, kItoa(hole, kNumBuf, 10));                                  // 0x0040eb8b
    strcat(kText, reinterpret_cast<const char*>(0x004C5914));                 // 0x0040ebba
    strcat(kText, shape);                                                     // 0x0040ebec
    strcat(kText, kItoa(ref<short>(0x00575AB4 + rec), kNumBuf, 10));          // 0x0040ec22 / 0x0040ec2b
    strcat(kText, reinterpret_cast<const char*>(0x004C590C));                 // 0x0040ec5d
    const unsigned flags = ref<unsigned>(0x00575CB0 + rec);                   // 0x0040ec97
    if (flags & 0x1000) strcat(kText, reinterpret_cast<const char*>(0x004C5904));  // 0x0040eca3
    if (flags & 0x2000) strcat(kText, reinterpret_cast<const char*>(0x004C58F8));  // 0x0040ecd8
    strcat(kText, reinterpret_cast<const char*>(0x004C58F0));                 // 0x0040ed07
    strcat(kText, kItoa(ref<signed char>(0x00575AB0 + rec), kNumBuf, 10));    // 0x0040ed34 / 0x0040ed43
    const unsigned flags2 = ref<unsigned>(0x00575CB0 + rec);                  // 0x0040ed82 (re-read)
    if (flags2 & 0x20) strcat(kText, reinterpret_cast<const char*>(0x004C58D4));   // 0x0040ed8c
    if (flags2 & 0x40) strcat(kText, reinterpret_cast<const char*>(0x004C58B8));   // 0x0040edbe
    strcat(kText, reinterpret_cast<const char*>(0x004C58A0));                 // 0x0040eded

    int msgArg = -1;                                                          // 0x0040ee1a
    if (hole == 6) {                                                          // 0x0040ee20
        kAppendUpgradeText(0);                                                // 0x0040effa
    } else if (hole != 10) {                                                  // 0x0040ee29
        const int bi = ref<int>(0x005A6364);                                  // 0x0040ee2f (next building index)
        if (hole > bi - 5 && bi < 0xf) {                                      // 0x0040ee3a / 0x0040ee43
            strcat(kText, reinterpret_cast<const char*>((hole & 1) ? 0x004C5870 : 0x004C583C));  // 0x0040ee51
            strcat(kText, reinterpret_cast<const char*>(0x004C5824));                            // 0x0040ee83
            strcat(kText, reinterpret_cast<const char*>(0x004C26B0 + bi * 0x14));                // 0x0040eeae
            strcat(kText, reinterpret_cast<const char*>(0x004C581C));                            // 0x0040eedf
            strcat(kText, reinterpret_cast<const char*>(0x004C26B0 + bi * 0x14));                // 0x0040ef0a
            // The per-building blurb: a jump table at 0x0040f158 over the index 3..14 (0x0040ef36..0x0040ef44);
            // index 4 lands on the shared tail, and an index outside 3..14 skips the blurb (0x0040ef3e).
            const int bi2 = ref<int>(0x005A6364);
            if (static_cast<unsigned>(bi2 - 3) <= 0xb) {
                unsigned s = 0;
                switch (bi2) {
                    case 3:  s = 0x004C57AC; break;
                    case 5:  strcat(kText, reinterpret_cast<const char*>(0x004C5750));           // 0x0040ef52
                             s = 0x004C5720; break;
                    case 6:  s = 0x004C5694; break;
                    case 7:  s = 0x004C56DC; break;
                    case 8:  s = 0x004C5618; break;
                    case 9:  s = 0x004C55DC; break;
                    case 10: s = 0x004C5654; break;
                    case 11: s = 0x004C54F4; break;
                    case 12: s = 0x004C5558; break;
                    case 13: s = 0x004C5594; break;
                    case 14: s = 0x004C551C; break;
                    default: break;                                            // case 4
                }
                if (s != 0) strcat(kText, reinterpret_cast<const char*>(s));    // 0x0040efc3
            }
            msgArg = -bi2;                                                      // 0x0040efeb
            ref<int>(0x005A6364) = bi2 + 1;                                     // 0x0040eff0
        }
    }
    if (hole == 10) kAppendUpgradeText(1);                                      // 0x0040f00b / 0x0040f00f
    if (hole == 0x12) kAppendUpgradeText(2);                                    // 0x0040f020 / 0x0040f024

    if (ref<int>(0x00834170) != 0)                                              // 0x0040f037
        kStartMessage(static_cast<int>(0x80004210), 1, msgArg);                 // 0x0040f043

    if (hole == 1) {                                                            // 0x0040f054
        // Two course-centre bytes, each (the hole-19 slot + 2 * hole 1's tee) / 3 (0x0040f056..0x0040f095).
        ref<signed char>(0x00585860) =
            static_cast<signed char>((ref<int>(0x00578150) + ref<int>(0x00575CC0) * 2) / 3);
        ref<signed char>(0x00585861) =
            static_cast<signed char>((ref<int>(0x00578154) + ref<int>(0x00575CC4) * 2) / 3);
    } else {
        if (hole == 9)                                                          // 0x0040f0a0
            kPostEvent(6, ref<int>(0x00576D10) * 0x400 + 0x200, ref<int>(0x00576D14) * 0x400 + 0x200);
        if (hole == 0x12)                                                       // 0x0040f0d5
            kPostEvent(0xe, ref<int>(0x00577F58) * 0x400 + 0x200, ref<int>(0x00577F5C) * 0x400 + 0x200);
    }

    // Renumber: the counter becomes the first hole whose par byte is 0, plus one (0x0040f0ff..0x0040f12c).
    ref<int>(0x005685F0) = 1;
    if (ref<signed char>(0x00575CB8) != 0) {
        int n = 1;
        while (ref<signed char>(0x00575CB8 + n * 0x208) != 0) ++n;
        ref<int>(0x005685F0) = n + 1;
    }
    ref<int>(0x004C2848) = -1;                                                  // 0x0040f132
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0040E720, announceBuilding, AnnounceBuilding_re, AnnounceBuilding_orig);
SG_HOOK("golf_clean.exe", 0x004732D0, addScore, AddScore_re, AddScore_orig);
SG_HOOK("golf_clean.exe", 0x0040E400, clearObjectFootprint, ClearObjectFootprint_re, ClearObjectFootprint_orig);
SG_HOOK("golf_clean.exe", 0x00406670, membershipReport, MembershipReport_re, MembershipReport_orig);
