// C3 batch c3aj (2026-10-08) of golf_clean.exe: the end-of-hole narrator matchUpdate 0x00427380 (5,723 bytes).
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x00427380 --list) and the C2
// transcription re/analysis/sim/00427380_matchUpdate.md; every statement cites the address of the global, record
// field or callee it comes from. __cdecl, one argument, void. Callees are invoked through their original addresses
// (so a hooked callee still runs its own reimplementation and the path-1 A/B drives both arms); the text buffer, the
// message/event/popup globals, the RNG seed, the golfer records, the hole table, the type table and the per-week
// money tables are declared as A/B state regions in re/frida/registry.d/c3aj.py.
#include "hooks.h"

#include <string.h>

namespace {

template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// A golfer record is 0x100 bytes; every field of golfer `i` is at <fieldBase> + i*0x100 (ebp = index << 8 at
// 0x0042738b). A hole record is 0x208 bytes; the asm forms hole*0x208 as ((hole<<6)+hole)*8 (0x0042748b..0x00427492).
// 0x00575cb8 is 0x00575ab0 + 0x208, so `0x00575cb8 + hole*0x208` is the par byte of hole + 1 and
// `0x00575cb8 + (hole - 1)*0x208` is the par byte of hole: the two addresses this function reads are one table.
// A zero par byte ends the course, so `the byte at 0x00575cb8 + hole*0x208 is 0` means "hole was the last one".
inline unsigned gr(int golfer) { return static_cast<unsigned>(golfer) * 0x100; }
inline unsigned hr(int hole) { return static_cast<unsigned>(hole) * 0x208; }

char* const kText = reinterpret_cast<char*>(0x0051A068);  // shared text buffer
char* const kNumBuf = reinterpret_cast<char*>(0x0058A528);  // itoa scratch

// The inline strlen / rep movs append the original compiles for every string concatenation (the terminator is
// copied too); `setText` is the same copy with 0x0051a068 as the destination (no scan, 0x004287f8).
void appendText(const char* src) {
    char* dst = kText;
    while (*dst) ++dst;
    do { *dst++ = *src; } while (*src++);
}
inline void appendAt(unsigned strAddr) { appendText(reinterpret_cast<const char*>(strAddr)); }
inline void setTextAt(unsigned strAddr) { strcpy(kText, reinterpret_cast<const char*>(strAddr)); }

typedef int(__cdecl* Trend_t)(int, int);                      // 0x004060a0 computeRatingTrend
typedef int(__cdecl* Clamp_t)(int, int, int);                 // 0x00467130 clamp
typedef int(__cdecl* Sign_t)(int);                            // 0x00467150 sign
typedef void(__cdecl* SoundAt_t)(int, int, int, int);         // 0x0040c500 playSoundAt
typedef void(__cdecl* PlaySound_t)(int, int, int, int, int);  // 0x004481b0 playSound
typedef void(__cdecl* Popup_t)(int, int, int, int);           // 0x0040c890 pointsPopup
typedef int(__cdecl* StartMessage_t)(int, int, int);          // 0x0040cb00 startMessage
typedef void(__cdecl* LogTick_t)(int, int);                   // 0x0040c6f0 logTick
typedef void(__cdecl* PostEvent_t)(int, int, int);            // 0x0046e7b0 postEvent
typedef void(__cdecl* Name_t)(int, int);                      // 0x004676e0 buildGolferName
typedef void(__cdecl* Thought_t)(int, int, int);              // 0x00467a00 emitGolferThought
typedef void(__cdecl* Patron_t)(int);                         // 0x004266b0 patronEvent
typedef void(__cdecl* AppendNumber_t)(int);                   // 0x0042dc00 appendNumber
typedef int(__cdecl* AppendString_t)(int);                    // 0x0045b9f0 appendString
typedef int(__cdecl* AppendBuf_t)(int);                       // 0x0045b8b0 appendToBuffer45b8b0
typedef void(__cdecl* Opinion_t)(int, int);                   // 0x00469a20 appendOpinion
typedef char*(__cdecl* Itoa_t)(int, char*, int);              // 0x004ad425 __itoa

const Trend_t kTrend = reinterpret_cast<Trend_t>(0x004060A0);
const Clamp_t kClamp = reinterpret_cast<Clamp_t>(0x00467130);
const Sign_t kSign = reinterpret_cast<Sign_t>(0x00467150);
const SoundAt_t kPlaySoundAt = reinterpret_cast<SoundAt_t>(0x0040C500);
const PlaySound_t kPlaySound = reinterpret_cast<PlaySound_t>(0x004481B0);
const Popup_t kPointsPopup = reinterpret_cast<Popup_t>(0x0040C890);
const StartMessage_t kStartMessage = reinterpret_cast<StartMessage_t>(0x0040CB00);
const LogTick_t kLogTick = reinterpret_cast<LogTick_t>(0x0040C6F0);
const PostEvent_t kPostEvent = reinterpret_cast<PostEvent_t>(0x0046E7B0);
const Name_t kBuildGolferName = reinterpret_cast<Name_t>(0x004676E0);
const Thought_t kEmitGolferThought = reinterpret_cast<Thought_t>(0x00467A00);
const Patron_t kPatronEvent = reinterpret_cast<Patron_t>(0x004266B0);
const AppendNumber_t kAppendNumber = reinterpret_cast<AppendNumber_t>(0x0042DC00);
const AppendString_t kAppendString = reinterpret_cast<AppendString_t>(0x0045B9F0);
const AppendBuf_t kAppendToBuffer = reinterpret_cast<AppendBuf_t>(0x0045B8B0);
const Opinion_t kAppendOpinion = reinterpret_cast<Opinion_t>(0x00469A20);
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);

// Narration strings, referenced by address only (never reproduced; the repo is public).
const unsigned kFirstFee = 0x004C7444;   // " has just paid you your first greens fee of "
const unsigned kFeeTip1 = 0x004C7408;    // " simoleans!  Golfers pay a fee at the end of each hole - "
const unsigned kFeeTip2 = 0x004C73CC;    // "happy golfers pay higher fees, ..."
const unsigned kFeeTip3 = 0x004C7374;    // " Greens fees are your main source of revenue, ..."
const unsigned kFeeTip4 = 0x004C7334;    // " Refer to your financial report ..."
const unsigned kWinsHole = 0x004C7324;   // " wins hole #"
const unsigned kByScore = 0x004C7314;    // " by a score of "
const unsigned kTo = 0x004C730C;         // " to "
const unsigned kAndWins = 0x004C7300;    // (winner's purse lead-in)
const unsigned kAndLoses = 0x004C72F8;   // (loser's purse lead-in)
const unsigned kMoneyTail = 0x004C4944;  // (currency tail)
const unsigned kEndOfMatch = 0x004C72DC; // " At the end of the match, "
const unsigned kPays = 0x004C72D0;       // (course pays lead-in)
const unsigned kCollects = 0x004C72A8;   // (course collects lead-in)
const unsigned kMoneyTail2 = 0x004C72C8; // (second currency tail)
const unsigned kScoreTied = 0x004C72B4;  // "the score is tied."
const unsigned kTiedAt = 0x004C7298;     // " are tied at "
const unsigned kEvenPar = 0x004C728C;    // "even par."
const unsigned kUnderPar = 0x004C7280;   // " under par."
const unsigned kOverPar = 0x004C7274;    // " over par."
const unsigned kLeads = 0x004C726C;      // " leads "
const unsigned kTrails = 0x004C7260;     // " trails "
const unsigned kBy = 0x004C52CC;         // " by "
const unsigned kShots = 0x004C7258;      // " shots."
const unsigned kShot = 0x004C7250;       // " shot."
const unsigned kAnd = 0x004C6AF4;        // " and "
const unsigned kNewRecord = 0x004C7228;  // " has just set a new course record of "
const unsigned kStrokesFor = 0x004C7218; // " strokes for "
const unsigned kHoles = 0x004C720C;      // " holes! "
const unsigned kCommissioner = 0x004C7088;  // "County commissioner "
const unsigned kCeo = 0x004C71B0;           // "Corporate CEO "
const unsigned kLastHole = 0x004C71C0;      // " is playing the last hole on your course. ..."
const unsigned kQuoteTail = 0x004C4E54;     // (closing quote)

// 0x00427380  matchUpdate(golfer): closes out the hole `golfer` has just finished. Sections, in order:
// (1) the shot histogram and the per-type stroke totals (only when the kind byte 0x005794d0 is 0, 0x00427390);
// (2) the greens fee 0x004c2850 built from the golfer's satisfaction word 0x0057955c, the course mode 0x00543cf4,
//     the hole's magazine flags 0x00575cb0 and the fee bonus 0x00543cd8, plus the type surcharge from the type
//     table 0x005849e0 (0x004274d1); (3) the first-fee tutorial message and the fee payment into the cash dword
//     0x00571fd4, the hole takings 0x00575ca4 and the weekly money table 0x00584210; (4) the hole-score array
//     0x005794db, the type scorecard 0x005849f6, the 0x0058585862 record sweep and the date fields; (5) the
//     two-golfer match narration (hole winner, end-of-match purse, standings) into the text buffer 0x0051a068;
//     (6) the course-record tables 0x0056a524 and the per-type bests 0x005849e0/0x005849e1; (7) the next hole,
//     patronEvent 0x004266b0, the last-hole VIP line and the satisfaction decay.
typedef void(__cdecl* MatchUpdate_t)(int);
MatchUpdate_t MatchUpdate_orig;
void __cdecl MatchUpdate_re(int golfer) {
    const unsigned g = gr(golfer);  // 0x0042738b  ebp = golfer << 8

    // ---- (1) histogram and per-type stroke totals, kind byte 0x005794d0 == 0 (0x00427390) ----------------------
    if (ref<unsigned char>(0x005794D0 + g) == 0) {
        const unsigned char kindFlags = ref<unsigned char>(0x005794D1 + g);   // 0x0042739e
        const int sel = (~kindFlags) & 7;                                     // 0x004273ab  not al; and eax, 7
        kTrend(ref<signed char>(0x005794D9 + g), sel);                        // 0x004273b4, result unused
        const int shots = kClamp(ref<signed char>(0x005794DA + g), 0, 9);     // 0x004273c5
        const int column = (kindFlags & 0xf) * 0xb;                           // 0x004273d6  lea/lea = *11
        const int hole = ref<signed char>(0x005794D9 + g);                    // 0x004273de
        ref<short>(0x00575AD8 + (shots + column + hole * 0x104) * 2) += 1;    // 0x004273ef
        kTrend(ref<signed char>(0x005794D9 + g), sel);                        // 0x004273ff, result unused
        const int band = ref<signed char>(0x00579572 + g) + (kindFlags & 7) * 4;   // 0x0042740d..0x00427417
        const unsigned slot = static_cast<unsigned>(ref<signed char>(0x005794D9 + g) + band * 0x2e) * 4;  // 0x00427429
        ref<int>(0x0056AE90 + slot) += ref<signed char>(0x005794DA + g);      // 0x0042743e
        ref<int>(0x0056AEDC + slot) += 1;                                     // 0x0042744b
    }

    // ---- (2) the greens fee for this hole, in 0x004c2850 -------------------------------------------------------
    ref<signed char>(0x005794DB + g + ref<signed char>(0x005794D9 + g)) =
        ref<signed char>(0x005794DA + g);                                     // 0x0042745e  score of this hole
    int fee = ref<short>(0x0057955C + g);                                     // 0x0042746a  satisfaction word
    ref<int>(0x004C2850) = fee;                                               // 0x00427474
    if (ref<int>(0x00543CF4) == 2) { fee += fee; ref<int>(0x004C2850) = fee; }          // 0x0042747a
    const unsigned holeFlags = ref<unsigned>(0x00575CB0 + hr(ref<signed char>(0x005794D9 + g)));  // 0x00427492
    if (holeFlags & 1) { fee += 2; ref<int>(0x004C2850) = fee; }              // 0x00427499  best-100 hole
    if (holeFlags & 2) { fee += 2; ref<int>(0x004C2850) = fee; }              // 0x004274a6  top-18 hole
    if (ref<int>(0x00543CD8) != 0) { fee += ref<int>(0x00543CD8); ref<int>(0x004C2850) = fee; }   // 0x004274ba
    const int type = ref<short>(0x0057956E + g);                              // 0x004274c4  golfer type index
    const unsigned rank = ref<unsigned char>(0x005849E2 + type * 0x2c) & 7;   // 0x004274d1  type table, stride 0x2c
    if (static_cast<int>(rank) > 3) {                                         // 0x004274dc  jle
        fee += (rank != 4) ? 5 : 2;                                           // 0x004274de  sbb/and 3/add 2
        ref<int>(0x004C2850) = fee;
        kPlaySoundAt(0x19, ref<int>(0x005794B8 + g), ref<int>(0x005794BC + g), 0);   // 0x00427504
        fee = ref<int>(0x004C2850);                                           // 0x00427509  reloaded
    }

    // ---- (3) the fee payment, with the first-fee tutorial (0x00427512) -----------------------------------------
    if ((ref<unsigned>(0x0059E7B8) & 0x200000) == 0) {                        // 0x0042751c  g_flags
        if (ref<short>(0x005A6D3C) == 0 && ref<short>(0x00584210) == 0) {     // 0x0042752a, 0x00427538
            ref<int>(0x004C2E0C) = golfer | 0x100;                            // 0x00427546
            kText[0] = 0;                                                     // 0x0042754b
            kBuildGolferName(golfer, 0);                                      // 0x00427552
            appendAt(kFirstFee);
            appendText(kItoa(fee * 100, kNumBuf, 10));                        // 0x0042759a
            appendAt(kFeeTip1);
            appendAt(kFeeTip2);
            ref<int>(0x005A9CCC) = 3;                                         // 0x0042764a
            appendAt(kFeeTip3);
            appendAt(kFeeTip4);
            kStartMessage(1, 1, golfer);                                      // 0x00427684
            fee = ref<int>(0x004C2850);                                       // 0x00427689  reloaded
        }
        ref<int>(0x00571FD4) += fee;                                          // 0x004276ba  cash
        ref<int>(0x00575CA4 + hr(ref<signed char>(0x005794D9 + g))) += fee;   // 0x004276c0  hole takings
        ref<short>(0x00584210 + (ref<short>(0x005A6D3C) % 100) * 0x14) +=
            static_cast<short>(fee);                                          // 0x004276da  weekly income
        kPointsPopup(fee, ref<int>(0x005794B8 + g), ref<int>(0x005794BC + g), -1);   // 0x004276eb
    }

    // ---- (4) the thought, the scorecard and the date fields (0x004276f3) ---------------------------------------
    if (ref<unsigned char>(0x0057953C + g) == 0)                              // 0x004276fb
        kEmitGolferThought(golfer, 0x13, ref<short>(0x0057955C + g));         // 0x00427708
    ref<signed char>(0x005849F6 + ref<short>(0x0057956E + g) * 0x2c + ref<signed char>(0x005794D9 + g)) =
        ref<signed char>(0x005794DA + g);                                     // 0x0042772a  per-type scorecard
    for (unsigned p = 0x00585862; p < 0x00586B62; p += 0x4c) {                // 0x00427736..0x00427751
        const unsigned char v = ref<unsigned char>(p);
        if (v != 0 && ref<signed char>(p + 1) == golfer) ref<unsigned char>(p) = v | 2;
    }
    const int date = ref<int>(0x00834170);                                    // 0x00427753  tick/date dword
    ref<int>(0x0057958C + g) = 0;                                             // 0x00427759
    ref<signed char>(0x005794DA + g) = 0;                                     // 0x00427763
    if (date > ref<int>(0x00579578 + g))                                      // 0x00427772  jle
        ref<int>(0x00575C9C + hr(ref<signed char>(0x005794D9 + g))) +=
            (date - ref<int>(0x00579578 + g)) / 2;                            // 0x00427792
    const unsigned flags = ref<unsigned>(0x0059E7B8);                         // 0x00427794
    ref<int>(0x00579578 + g) = date;                                          // 0x00427799
    ref<unsigned>(0x005794C8 + g) &= 0xfbdfbbff;                              // 0x004277ab
    ref<unsigned char>(0x005794D4 + g) = 0;                                   // 0x004277b1

    // ---- (5) the two-golfer match narration (0x004277bd) -------------------------------------------------------
    if ((flags & 0x200000) == 0 && (ref<unsigned char>(0x005794D1 + g) & 0xf0) != 0) {   // 0x004277bd, 0x004277cd
        const int partner = ref<short>(0x0057955A + g);                       // 0x004277d3
        const unsigned pg = gr(partner);
        if ((ref<unsigned char>(0x005794D1 + pg) & 0xf0) != 0 &&              // 0x004277e5
            (ref<signed char>(0x005794D9 + pg) > ref<signed char>(0x005794D9 + g) ||
             ref<signed char>(0x005794D9 + pg) == 0)) {                       // 0x004277f9 jg, 0x004277fd jne
            const int holes = ref<signed char>(0x005794D9 + g);               // 0x00427803
            int sumMine = 0, sumPartner = 0, sumPar = 0;                      // 0x0042780f..0x0042781b
            for (int h = 1; h <= holes; ++h) {                                // 0x0042781f jl, 0x00427869 jle
                sumMine += ref<signed char>(0x005794DB + g + h);              // 0x00427833
                sumPartner += ref<signed char>(0x005794DB + pg + h);          // 0x0042784b
                sumPar += ref<signed char>(0x00575CB8 + (h - 1) * 0x208);     // 0x00427851
            }
            const int even = golfer & 0xfffe;                                 // 0x00427879
            const int odd = golfer | 1;                                       // 0x00427882
            kText[0] = 0;                                                     // 0x0042788c
            const unsigned evenScoreAddr = 0x005794DB + gr(even) + holes;     // 0x00427893
            const unsigned oddScoreAddr = 0x005794DB + gr(odd) + holes;       // 0x004278a8

            if (ref<signed char>(oddScoreAddr) < ref<signed char>(evenScoreAddr)) {   // 0x004278af jge
                const int purse = (ref<int>(0x0059B730) * 2000) / 100;        // 0x004278b7..0x004278de
                kBuildGolferName(odd, 0);                                     // 0x004278e4
                appendAt(kWinsHole);
                appendText(kItoa(holes, kNumBuf, 10));                        // 0x00427922
                appendAt(kByScore);
                appendText(kItoa(ref<signed char>(oddScoreAddr), kNumBuf, 10));   // 0x00427990
                appendAt(kTo);
                appendText(kItoa(ref<signed char>(evenScoreAddr), kNumBuf, 10));  // 0x004279f9
                appendAt(kAndWins);
                kAppendNumber(purse * 100);                                   // 0x00427a61
                appendAt(kMoneyTail);
                ref<short>(0x0058421E + (ref<short>(0x005A6D3C) % 100) * 0x14) +=
                    static_cast<short>(purse);                                // 0x00427ab2
                kPointsPopup(purse, ref<int>(0x005794B8 + gr(odd)), ref<int>(0x005794BC + gr(odd)), -1);  // 0x00427ac2
                ref<int>(0x00571FD4) += purse;                                // 0x00427ad7
                kStartMessage(0x80002108, 0, ref<int>(0x005A59F8));           // 0x00427ae4
                kPlaySound(0x23, 100, 0, 0, 0);                               // 0x00427af3
            }
            if (ref<signed char>(evenScoreAddr) < ref<signed char>(oddScoreAddr)) {   // 0x00427b0e jle
                const int purse = (ref<int>(0x0059B730) * -2000) / 100;       // 0x00427b14..0x00427b42
                kBuildGolferName(even, 0);                                    // 0x00427b48
                appendAt(kWinsHole);
                appendText(kItoa(holes, kNumBuf, 10));                        // 0x00427b86
                appendAt(kByScore);
                appendText(kItoa(ref<signed char>(evenScoreAddr), kNumBuf, 10));  // 0x00427bef
                appendAt(kTo);
                appendText(kItoa(ref<signed char>(oddScoreAddr), kNumBuf, 10));   // 0x00427c5d
                appendAt(kAndLoses);
                kAppendNumber(((purse ^ (purse >> 31)) - (purse >> 31)) * 100);   // 0x00427cb6  abs
                appendAt(kMoneyTail);
                ref<short>(0x0058421E + (ref<short>(0x005A6D3C) % 100) * 0x14) +=
                    static_cast<short>(purse);                                // 0x00427d1b
                kPointsPopup(purse, ref<int>(0x005794B8 + gr(odd)), ref<int>(0x005794BC + gr(odd)), -1);  // 0x00427d2b
                ref<int>(0x00571FD4) += purse;                                // 0x00427d40
                kStartMessage(0x80002108, 0, ref<int>(0x005A59F8));           // 0x00427d4d
                kPlaySound(0x24, 100, 0, 0, 0);                               // 0x00427d5c
            }

            // End of the match: the next hole's 0x00575cb8 byte is 0 (0x00427d72).
            if (ref<signed char>(0x00575CB8 + hr(ref<signed char>(0x005794D9 + g))) == 0) {   // 0x00427d7b jne
                appendAt(kEndOfMatch);
                const int award = (ref<int>(0x0059B730) * 2000) / 100;        // 0x00427da5..0x00427dce
                if ((ref<unsigned char>(0x005794D1 + gr(odd)) & 0xf0) != 0) { // 0x00427ddc je
                    const short awardW = static_cast<short>(award);
                    // The golfer won the match: the course pays the purse (0x00427de2..0x00427e0e).
                    if ((golfer == even && sumMine < sumPartner) || (golfer == odd && sumPartner < sumMine)) {
                        ref<int>(0x00571FD4) -= award;                        // 0x00427e26
                        kPointsPopup(-award, ref<int>(0x005794B8 + g), ref<int>(0x005794BC + g), -1);  // 0x00427e39
                        appendAt(kPays);
                        kAppendNumber(award * 100);                           // 0x00427e79
                        appendAt(kMoneyTail2);
                        ref<short>(0x0058421E + (ref<short>(0x005A6D3C) % 100) * 0x14) -= awardW;   // 0x00427ecb
                        kPlaySound(0x24, 100, 0, 0, 0);                       // 0x00427ed3
                        ref<int>(0x0059B730) -= 1;                            // 0x00427ee1
                    }
                    if (sumMine == sumPartner) {                              // 0x00427ef4 jne
                        ref<int>(0x0059B730) -= 1;                            // 0x00427f02
                        appendAt(kScoreTied);
                    }
                    // The golfer lost the match: the course collects (0x00427f34..0x00427f5a).
                    if ((golfer == even && sumPartner < sumMine) || (golfer == odd && sumMine < sumPartner)) {
                        ref<int>(0x00571FD4) += award;                        // 0x00427f72
                        kPointsPopup(award, ref<int>(0x005794B8 + g), ref<int>(0x005794BC + g), -1);   // 0x00427f83
                        ref<short>(0x0058421E + (ref<short>(0x005A6D3C) % 100) * 0x14) += awardW;   // 0x00427fa2
                        appendAt(kCollects);
                        kAppendNumber(award * 100);                           // 0x00427fdd
                        appendAt(kMoneyTail2);
                        kPlaySound(0x23, 100, 0, 0, 0);                       // 0x00428016
                        ref<int>(0x005787CC) += 1;                            // 0x00428027
                        kPostEvent(5, ref<int>(0x005794B8 + g), ref<int>(0x005794BC + g));   // 0x00428036
                        const unsigned char tag = (golfer == even)            // 0x00428046 jne
                                                      ? ref<unsigned char>(0x00579573 + g)
                                                      : ref<unsigned char>(0x00579573 + gr(ref<short>(0x0057955A + g)));
                        kLogTick(0x60, tag);                                  // 0x0042806a
                    }
                }
                // Standings line (0x00428072).
                ref<int>(0x005A59F8) = -1;                                    // 0x0042807c
                kBuildGolferName(ref<short>(0x0057955A + g), 0);              // 0x00428086
                unsigned tail;
                if (sumMine == sumPartner) {                                  // 0x00428098 jne
                    appendAt(kAnd);
                    kBuildGolferName(golfer, 0);                              // 0x004280d1
                    appendAt(kTiedAt);
                    if (sumMine == sumPar) {                                  // 0x00428110 jne
                        tail = kEvenPar;
                    } else {
                        const int d = sumMine - sumPar;
                        appendText(kItoa((d ^ (d >> 31)) - (d >> 31), kNumBuf, 10));   // 0x00428131
                        tail = (sumMine < sumPar) ? kUnderPar : kOverPar;     // 0x00428169 jge
                    }
                } else {
                    unsigned lead;
                    if (sumPartner < sumMine) {                               // 0x00428185 jle
                        if ((ref<unsigned char>(0x005794D1 + gr(odd)) & 0xf0) != 0 &&        // 0x00428189
                            (ref<unsigned char>(0x005794D1 + gr(odd ^ 1)) & 0xf0) != 0)      // 0x0042819e
                            kPlaySound((golfer == odd) + 0x2f, 100, 0, 0, 0); // 0x004281b7  sete
                        lead = kLeads;
                    } else {
                        if ((ref<unsigned char>(0x005794D1 + gr(odd)) & 0xf0) != 0 &&        // 0x004281c8
                            (ref<unsigned char>(0x005794D1 + gr(odd ^ 1)) & 0xf0) != 0)      // 0x004281dd
                            kPlaySound((golfer != odd) + 0x2f, 100, 0, 0, 0); // 0x004281f6  setne
                        lead = kTrails;
                    }
                    appendAt(lead);
                    kBuildGolferName(golfer, 0);                              // 0x00428231
                    appendAt(kBy);
                    const int d = sumPartner - sumMine;                       // 0x00428261
                    const int ad = (d ^ (d >> 31)) - (d >> 31);               // 0x0042826f  cdq/xor/sub
                    appendText(kItoa(ad, kNumBuf, 10));                       // 0x0042827c
                    tail = (ad > 1) ? kShots : kShot;                         // 0x004282b6 jg
                }
                appendAt(tail);
                kStartMessage(0x80002108, 1, ref<int>(0x005A59F8));           // 0x004282f2
            }
        }
    }

    // ---- (6) the course-record tables, at the end of the round (0x004282fa) ------------------------------------
    if ((ref<unsigned>(0x0059E7B8) & 0x200000) == 0) {                        // 0x00428304 jne
        const signed char myHole = ref<signed char>(0x005794D9 + g);          // 0x0042830a
        if (myHole == 0x12 || ref<signed char>(0x00575CB8 + hr(myHole)) == 0) {   // 0x00428312 je, 0x00428327 jne
            int total = 0;        // ebx: strokes over the holes played
            int grossPlus5 = 0;   // [esp+0x20]: same, with 5 per unplayed hole
            int overPar = 0;      // [esp+0x10]: (score - par) sum, +1 per unplayed hole
            int typeSum = 0;      // [esp+0x14]: sum of the per-hole type bits & 3
            int parSum = 0;       // [esp+0x1c]
            int allPlayed = 1;    // [esp+0x28]
            for (int h = 1; h <= 18; ++h) {                                   // 0x00428354..0x004283e6
                if (h <= myHole) {                                            // 0x00428358 jg
                    const int sc = ref<signed char>(0x005794DB + g + h);      // 0x0042835a
                    const int par = ref<signed char>(0x00575CB8 + (h - 1) * 0x208);   // 0x00428368
                    total += sc;
                    grossPlus5 += sc;
                    parSum += par;
                    overPar += sc - par;
                    typeSum += ref<unsigned char>(0x005849E3 + ref<short>(0x0057956E + g) * 0x2c + h) & 3;  // 0x00428394
                } else {
                    grossPlus5 += 5;                                          // 0x004283b2
                    overPar += 1;                                             // 0x004283b5
                }
                if (h < ref<int>(0x005685F0) && ref<signed char>(0x005794DB + g + h) == 0)   // 0x004283c4, 0x004283cf
                    allPlayed = 0;                                            // 0x004283d1
            }
            kText[0] = 0;                                                     // 0x004283f0
            if (allPlayed != 0) {                                             // 0x004283f9 je
                int i = 0;
                for (;;) {                                                    // 0x00428406
                    const int v = ref<int>(0x0056A524 + i * 4);
                    if (v == 0) {                                             // 0x0042840a je
                        ref<int>(0x0056A524 + i * 4) = total;                 // 0x00428426
                        kText[0] = 0;                                         // 0x0042842d
                        break;
                    }
                    if (total < v) {                                          // 0x0042840e jl
                        if (i <= 8) {                                         // 0x0042843a jg
                            int s = 0x1d;                                     // 0x0042843c
                            unsigned q = 0x0056A544;                          // 0x00428441
                            for (;;) {                                        // 0x00428449
                                const int a = ref<int>(q);
                                if (a != 0) {                                 // 0x00428451 je
                                    kText[0] = 0;                             // 0x00428455
                                    ref<int>(q + 4) = a;                      // 0x0042845c
                                    kAppendString(s - 1);                     // 0x00428463
                                    kAppendToBuffer(s);                       // 0x00428469
                                }
                                q -= 4;                                       // 0x00428475
                                --s;                                          // 0x00428478
                                if (s - 0x15 < i) break;                      // 0x00428482 jge
                            }
                        }
                        ref<int>(0x0056A524 + i * 4) = total;                 // 0x0042848a
                        kText[0] = 0;                                         // 0x00428491
                        break;
                    }
                    ++i;                                                      // 0x00428413
                    if (0x0056A524 + i * 4 >= 0x0056A54C) goto afterRecords;  // 0x00428419 jl
                }
                kBuildGolferName(golfer, 0);                                  // 0x00428499
                kAppendToBuffer(i + 0x14);                                    // 0x004284a2
            }
          afterRecords:
            // New course record message (0x004284aa).
            if (ref<int>(0x005685F0) > 2 && ref<unsigned char>(0x005794DC + g) != 0 &&        // 0x004284b1, 0x004284bf
                ref<int>(0x004C2E0C) == -1 && allPlayed != 0 &&                               // 0x004284cc, 0x004284d8
                (ref<int>(0x0056A524) == 0 || total < ref<int>(0x0056A524)) &&                // 0x004284e5, 0x004284e9
                total <= parSum) {                                                            // 0x004284f3 jg
                ref<int>(0x004C2E0C) = golfer | 0x100;                        // 0x00428505
                kBuildGolferName(golfer, 0);                                  // 0x0042850b
                kAppendToBuffer(0x14);                                        // 0x00428512
                appendAt(kNewRecord);
                appendText(kItoa(total, kNumBuf, 10));                        // 0x0042854c
                appendAt(kStrokesFor);
                appendText(kItoa(ref<int>(0x005685F0) - 1, kNumBuf, 10));     // 0x004285b4
                appendAt(kHoles);
                typeSum += 1;                                                 // 0x00428613
            }
            // Per-type bests and the "come back" flag (0x0042861a).
            const int difficulty = ref<int>(0x00822C88);                      // 0x0042861a
            int score = typeSum;                                              // edi
            if (ref<short>(0x0057955C + g) >= difficulty * 2 + 6) ++score;    // 0x00428631 jl
            const unsigned tb = static_cast<unsigned>(ref<short>(0x0057956E + g)) * 0x2c;   // 0x00428634..0x00428645
            const unsigned char best = ref<unsigned char>(0x005849E0 + tb);   // 0x00428648
            if (grossPlus5 < static_cast<int>(best) || best == 0)             // 0x00428658 jl, 0x0042865c jne
                ref<unsigned char>(0x005849E0 + tb) = static_cast<unsigned char>(grossPlus5);   // 0x0042865e
            const signed char avg = ref<signed char>(0x005849E1 + tb);        // 0x00428664
            ref<signed char>(0x005849E1 + tb) = (avg == 0)                    // 0x0042866c jne
                                                    ? static_cast<signed char>(overPar)
                                                    : static_cast<signed char>((avg + overPar) / 2);   // 0x00428680
            if ((ref<unsigned char>(0x005849E2 + tb + ref<int>(0x005685F0)) & 3) != 0 ||      // 0x00428696 jne
                difficulty == 0 || ref<short>(0x005A6D3C) == 0 ||                             // 0x0042869a, 0x004286a4
                (ref<int>(0x00571FD4) < 200 && ref<int>(0x0056D1B0) <= 0))                    // 0x004286b0, 0x004286b9
                ++score;                                                                      // 0x004286bb
            if (ref<int>(0x00543CC4) != 0) score += ref<int>(0x00543CC4);     // 0x004286c3
            if (ref<unsigned char>(0x005794D0 + g) == 0) {                    // 0x004286cf jne
                const unsigned char tflags = ref<unsigned char>(0x005849E2 + tb);   // 0x004286d1
                if (score >= (1 << (tflags & 7)) && (tflags & 7) < 5) {       // 0x004286e5 jl, 0x004286eb jge
                    ref<unsigned>(0x005794C8 + g) |= 0x80000000;              // 0x004286f8
                    ref<short>(0x00579556 + g) = static_cast<short>(0xfff8);  // 0x004286fe
                }
            }
        }
    }

    // ---- (7) advance to the next hole (0x00428707) -------------------------------------------------------------
    const signed char nextHole = ref<signed char>(0x005794D9 + g) + 1;        // 0x00428713
    ref<signed char>(0x005794D9 + g) = nextHole;                              // 0x00428717
    if ((ref<unsigned>(0x0059E7B8) & 0x4200000) != 0) {                       // 0x00428720 test, 0x00428728 je
        if (ref<signed char>(0x00575AB0 + hr(nextHole)) == 0)                 // 0x00428738 jne
            ref<signed char>(0x005794D9 + g) = 1;                             // 0x0042873a  wrap to hole 1
        ref<short>(0x00579576 + g) += static_cast<short>(ref<int>(0x005685F0) * 2);   // 0x0042874a
        if (ref<signed char>(0x005794DB + g + ref<signed char>(0x005794D9 + g)) != 0)  // 0x00428761 je
            kPatronEvent(golfer);                                             // 0x00428768
    } else {
        if (ref<signed char>(0x00575AB0 + hr(nextHole)) == 0) {               // 0x00428780 je
            kPatronEvent(golfer);                                             // 0x004289cb
            return;
        }
        const signed char partnerHole = ref<signed char>(0x005794D9 + gr(ref<short>(0x0057955A + g)));   // 0x00428790
        if (partnerHole == 0 || partnerHole == 0x13) {                        // 0x00428798 je, 0x004287a0 je
            kPatronEvent(golfer);                                             // 0x004289cb
            return;
        }
    }

    // The VIP's last-hole line (0x004287a6).
    if (ref<signed char>(0x00575CB8 + hr(ref<signed char>(0x005794D9 + g))) == 0) {   // 0x004287bd jne
        const unsigned kind = ref<unsigned char>(0x005794D0 + g) & 0xe0;      // 0x004287c9
        if (kind == 0x40 || kind == 0x60) {                                   // 0x004287d1 je, 0x004287d6 jne
            setTextAt(kind == 0x40 ? kCommissioner : kCeo);                   // 0x004287dc / 0x004287e3
            kBuildGolferName(golfer, 0);                                      // 0x0042880e
            appendAt(kLastHole);
            kAppendOpinion(ref<short>(0x0057955C + g), 0);                    // 0x0042884c
            appendAt(kQuoteTail);
            kStartMessage(0x80000210, 1, golfer);                             // 0x0042888a
        }
    }

    // ---- the rolling satisfaction average and the end-of-hole decay (0x00428892) -------------------------------
    ref<short>(0x0057956C + g) = static_cast<short>(ref<short>(0x0057956C + g) * 7 / 8) +
                                 ref<short>(0x0057955C + g);                  // 0x00428899..0x004288b2
    if ((ref<unsigned>(0x005794C8 + g) & 0x100000) != 0)                      // 0x004288c4 je
        ref<unsigned char>(0x005794EF + g + ref<signed char>(0x005794D9 + g)) =
            ref<unsigned char>(0x0057956C + g);                               // 0x004288d3
    else
        ref<short>(0x0057956C + g) = ref<short>(0x0057955C + g);              // 0x004288e3
    ref<unsigned char>(0x00579514 + g + ref<signed char>(0x005794D9 + g)) =
        ref<unsigned char>(0x0057955C + g);                                   // 0x004288f7
    ref<signed char>(0x005794EE + g) -= static_cast<signed char>(kSign(ref<signed char>(0x005794EE + g)));  // 0x00428906

    if ((ref<unsigned char>(0x005794D0 + g) & 0xe0) == 0x40) {                // 0x00428928 je
        const int v = (ref<signed char>(0x005794D9 + g) + 6) * ref<short>(0x0057955C + g) *
                      ref<int>(0x00822C88);                                   // 0x0042899c..0x004289a3
        ref<short>(0x0057955C + g) = static_cast<short>(ref<short>(0x0057955C + g) - v / 0xa0);   // 0x004289b8
        return;
    }
    const int par = ref<short>(0x00584A0A + static_cast<unsigned>(ref<short>(0x0057956E + g)) * 0x2c);   // 0x00428942
    const int n = (par + ref<signed char>(0x005794D9 + g) + 6) *
                  (ref<short>(0x0057955C + g) + ref<int>(0x00822C88) - 1) *
                  (ref<int>(0x00822C88) + 1);                                 // 0x0042894a..0x00428963
    ref<short>(0x0057955C + g) = static_cast<short>(
        ref<short>(0x0057955C + g) - n / ((ref<int>(0x00543CD4) * 5 + 0xf) * 8));   // 0x00428966..0x00428979
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00427380, matchUpdate, MatchUpdate_re, MatchUpdate_orig);
