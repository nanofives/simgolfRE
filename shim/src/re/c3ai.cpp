// C3 batch c3ai (2026-10-08) of golf_clean.exe: text builders. Hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; every body cites the
// address of each global, table and callee it uses, and is not a copy of a matched source. Both functions are
// __cdecl. Callees are invoked through their original addresses, so a hooked callee runs its own reimplementation in
// both arms of the path-1 A/B (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF for the hooked callees:
// findByteInRange 0x004935f0 (c3w), comboFindItem 0x004940e0 (c3y), findMarkupTokenIndex 0x004a4ad0 (c3s),
// appendCourseTitle 0x0040daa0 and startMessage 0x0040cb00 (c3g)).
#include "hooks.h"
#include <string.h>

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }
inline const char* str(unsigned addr) { return reinterpret_cast<const char*>(addr); }

// CRT and game callees, reached through their original addresses.
typedef char*(__cdecl* Strstr_t)(const char*, const char*);
typedef int(__cdecl* Strncmp_t)(const char*, const char*, unsigned);
typedef char*(__cdecl* Itoa_t)(int, char*, int);
typedef char*(__cdecl* FindDigit_t)(const char*);
typedef const char*(__fastcall* ComboFind_t)(void* self, void* edx, int key);   // __thiscall, ecx = this
typedef int(__fastcall* FindToken_t)(void* self, void* edx, int key);           // __thiscall, ecx = this
typedef void(__cdecl* AppendCourseTitle_t)(int);
typedef int(__cdecl* StartMessage_t)(unsigned, int, int);
typedef int(__cdecl* SaveGame_t)(const char*);

const Strstr_t kStrstr = reinterpret_cast<Strstr_t>(0x004A64F0);                 // _strstr
const Strncmp_t kStrncmp = reinterpret_cast<Strncmp_t>(0x004A6AD0);              // _strncmp
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);                       // __itoa
const FindDigit_t kFindDigit = reinterpret_cast<FindDigit_t>(0x004935F0);        // findByteInRange ('0'..'9')
const ComboFind_t kComboFindItem = reinterpret_cast<ComboFind_t>(0x004940E0);    // comboFindItem
const FindToken_t kFindToken = reinterpret_cast<FindToken_t>(0x004A4AD0);        // findMarkupTokenIndex
const AppendCourseTitle_t kAppendCourseTitle = reinterpret_cast<AppendCourseTitle_t>(0x0040DAA0);
const StartMessage_t kStartMessage = reinterpret_cast<StartMessage_t>(0x0040CB00);
const SaveGame_t kSaveGame = reinterpret_cast<SaveGame_t>(0x0040B4A0);          // saveGame (pages 11 and 29 only)

// Markup tokens, referenced by address only (the repo is public; the bytes are in the anchored binary).
const char* const kDollar = str(0x004C6158);   // the 1-char marker scanned for at 0x00494333
const char* const kGt = str(0x004E4710);       // ">"  (0x004945c2, 0x00494909)
const char* const kLt = str(0x004E4704);       // "<"  (0x0049483c)
const char* const kEq = str(0x004E4700);       // "="  (0x0049489e)
const char* const kHex = str(0x004E4714);      // "$HEX"      (0x004944ce)
const char* const kNum = str(0x004E471C);      // "$NUM"      (0x00494420)
const char* const kNumber = str(0x004E4724);   // "$NUMBER"    (0x0049436c)
const char* const kDropDown = str(0x004E4248); // "$DROPDOWN"  (0x00494944)
const char* const kDropLink = str(0x004E423C); // "$DROPLINK"  (0x00494958)
const char* const kLinkLt = str(0x004E4254);   // "$LINK<"     (0x00494798)
const char* const kLink = str(0x004E4708);     // "$LINK"      (0x004947df)

// Markup data tables, all in .data (0x0083....):
//   0x0083de68  int[10]            number slots      ($NUMBER/$NUM/$HEX, 0x004943d1)
//   0x0083dda8  int[]              the same array addressed by raw character code ($<#c>, 0x0049463d:
//                                  0x0083dda8 + '0'*4 == 0x0083de68, so '0'..'9' select number slots 0..9)
//   0x0083e8b8  int[10]            gender per text slot  (0x00494666)
//   0x0083d3c8  int[10]            plural per text slot  (0x0049466d)
//   0x0083e8e0  char[10][0x100]    text slots            (0x00494701, 0x00494b27)
//   0x0083f368  struct[10] (0x108) link slots: name at +0, id at +0x104 (0x00494878, 0x004948ce)
//   0x0083fdb8  {void* base; int key}[10]  dropdown fall-back lists (0x00494a17/0x00494a1e)
//   0x0083fe08  void*[10] (stride 8)       dropdown combo objects   (0x004949ff)
//   0x0083fe5c  int                        elision pass enable      (0x00494b9d)
const unsigned kNumberSlots = 0x0083DE68;
const unsigned kCharNumbers = 0x0083DDA8;
const unsigned kGenderSlots = 0x0083E8B8;
const unsigned kPluralSlots = 0x0083D3C8;
const unsigned kTextSlots = 0x0083E8E0;
const unsigned kLinkSlots = 0x0083F368;
const unsigned kDropLists = 0x0083FDB8;
const unsigned kDropCombos = 0x0083FE08;

// The six elision words the final pass searches for, 0x004e4688 + i*0x14, i < 6 (0x004944bb4..0x00494be8).
const unsigned kElisionWords = 0x004E4688;

// The original builds its output with inline strlen/rep movs; these two helpers write exactly the same bytes.
inline char* appendStr(char* dst, const char* src) {       // inline strcat: copies strlen(src)+1 bytes
    strcat(dst, src);
    return dst;
}
inline char* endOf(char* p) { return p + strlen(p); }

// 0x004942f0  expandTextMarkup(const char* in, char* out) -----------------------------------------------------
// Expands the "$" markup of `in` into `out`. Returns 3 when either pointer is NULL (0x0049430d / 0x0049431c),
// 0xe when a token carries a slot digit above 9 (0x00494390, 0x00494444, 0x004944f2, 0x00494803, 0x0049497c) or
// when an unknown "$" is not followed by a digit (0x00494ae6 / 0x00494af1), and 0 otherwise (0x00494c48).
// Each round finds the next marker with _strstr (0x00494333) and dispatches on the byte after it
// (movsx / cmp 0x2a / jump table 0x00494c64 at 0x00494363).
int __cdecl ExpandTextMarkup_re(const char* in, char* out);
typedef int(__cdecl* Expand_t)(const char*, char*);
Expand_t ExpandTextMarkup_orig;

bool isElisionVowel(char c) {                              // 0x00494bf7..0x00494c2d
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y' || c == 'h' || c == 'H' ||
           c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y';
}

int __cdecl ExpandTextMarkup_re(const char* in, char* out) {
    char one[2];                                           // the 1-char scratch string at [esp+0x12]
    one[0] = 0;
    one[1] = 0;                                            // 0x00494303 / 0x00494308
    if (in == 0 || out == 0) return 3;                     // 0x00494300 / 0x0049431a
    char* const outStart = out;                            // saved at [esp+0x1c], 0x00494322
    // The gender slot [esp+0x18] is only written by the $<...> forms that name one; the '#' form reads it back
    // without writing it (0x00494651), so in the original it keeps the value of a previous round (and is read
    // uninitialized on the first). Fixtures never take that path (see log/c3/c3ai_purpose.md).
    int gender = 0;

    for (;;) {
        const char* m = kStrstr(in, kDollar);              // 0x00494333
        if (m == 0) break;                                 // 0x0049433f
        const char* const afterDollar = m + 1;             // 0x00494345
        const unsigned prefix = static_cast<unsigned>(m - in);

        switch (*afterDollar) {
        case 'N': {                                        // 0x0049436a
            int slot;
            unsigned skip;
            if (kStrncmp(m, kNumber, 7) == 0) {            // 0x00494372
                slot = static_cast<signed char>(m[7]) - 0x30;   // 0x00494382
                if (slot > 9) return 0xe;                  // 0x00494390
                skip = 8;                                  // 0x004943bc
            } else if (kStrncmp(m, kNum, 4) == 0) {        // 0x00494426
                slot = static_cast<signed char>(m[4]) - 0x30;   // 0x00494436
                if (slot > 9) return 0xe;                  // 0x00494444
                skip = 5;                                  // 0x0049446d
            } else {
                goto unknownToken;                         // 0x00494430
            }
            memcpy(out, in, prefix);                       // 0x00494464
            out += prefix;
            *out = 0;                                      // 0x004943c5 / 0x00494477
            in = m + skip;
            char tmp[80];                                  // local_a0 / local_140
            kItoa(at<int>(kNumberSlots + slot * 4), tmp, 10);   // 0x004943d9 / 0x0049448a
            appendStr(out, tmp);
            out = endOf(out);                              // 0x00494417
            break;
        }

        case 'H': {                                        // 0x004944cc
            if (kStrncmp(m, kHex, 4) != 0) goto unknownToken;   // 0x004944d4 / 0x004944de
            const int slot = static_cast<signed char>(m[4]) - 0x30;   // 0x004944e4
            if (slot > 9) return 0xe;                      // 0x004944f2
            memcpy(out, in, prefix);                       // 0x00494512
            out += prefix;
            *out = 0;
            in = m + 5;                                    // 0x0049451b
            char tmp[80];                                  // local_f0
            kItoa(at<int>(kNumberSlots + slot * 4), tmp, 10);   // 0x00494538 (the same table as $NUM, base 10)
            appendStr(out, tmp);
            out = endOf(out);                              // 0x00494575
            break;
        }

        case '$': {                                        // 0x0049457a: "$$" emits one marker
            memcpy(out, in, prefix + 1);                   // 0x00494593, count = (m - in) + 1
            out += prefix + 1;
            *out = 0;                                      // 0x0049459f
            in = m + 2;                                    // 0x0049459a / 0x004945a3
            break;
        }

        case 'L': {                                        // 0x00494796
            if (kStrncmp(m, kLinkLt, 6) == 0) {            // 0x0049479e: "$LINK<" passes the marker through
                memcpy(out, in, prefix + 1);               // 0x004947c1
                out += prefix + 1;
                *out = 0;                                  // 0x004947ca (al == 0 there)
                in = afterDollar;                          // 0x004947cd
                break;
            }
            if (kStrncmp(m, kLink, 5) != 0) goto unknownToken;    // 0x004947e5 / 0x004947ef
            const int slot = static_cast<signed char>(m[5]) - 0x30;    // 0x004947f5
            if (slot >= 10) return 0xe;                    // 0x00494803
            memcpy(out, in, prefix + 5);                   // 0x00494829, count = (m - in) + 5: "$LINK" is kept
            char* const w = out + prefix + 5;              // 0x0049481e
            *w = 0;                                        // 0x00494841
            in = m + 6;                                    // 0x00494816 / 0x0049482d
            const unsigned rec = kLinkSlots + slot * 0x108;      // 0x0049486b: slot*33*8
            appendStr(w, kLt);                             // 0x0049485c
            appendStr(w, str(rec));                        // 0x00494895: the link name at +0
            appendStr(w, kEq);                             // 0x004948bd
            char tmp[80];                                  // local_50
            kItoa(at<int>(rec + 0x104), tmp, 10);          // 0x004948ce / 0x004948d5: the link id at +0x104
            appendStr(w, tmp);                             // 0x00494900
            appendStr(w, kGt);                             // 0x00494928
            out = endOf(w);                                // 0x0049493b
            break;
        }

        case 'D': {                                        // 0x00494942
            if (kStrncmp(m, kDropDown, 9) != 0 && kStrncmp(m, kDropLink, 9) != 0)
                goto unknownToken;                         // 0x0049494a / 0x0049495e / 0x00494968
            const int slot = static_cast<signed char>(m[9]) - 0x30;    // 0x0049496e
            if (slot >= 10) return 0xe;                    // 0x0049497c
            memcpy(out, in, prefix);                       // 0x0049499a
            char* const w = out + prefix;                  // 0x00494991
            *w = 0;                                        // 0x004949b5
            one[0] = '[';                                  // 0x0049499e
            appendStr(w, one);                             // 0x004949ca
            memcpy(w + 1, m, 10);                          // 0x004949d6..0x004949f3 (4 + 4 + 2 bytes of the token)
            char* const q = w + 11;                        // 0x004949da
            *q = 0;                                        // 0x004949fb
            in = m + 10;                                   // 0x004949dd / 0x004949e5
            const char* item;
            void* const combo = at<void*>(kDropCombos + slot * 8);     // 0x004949ff
            if (combo != 0) {                              // 0x00494a08
                item = kComboFindItem(combo, 0, at<int>(reinterpret_cast<unsigned>(combo) + 0x1c));  // 0x00494a0e
            } else {
                void* const base = at<void*>(kDropLists + slot * 8);   // 0x00494a1e
                const int key = at<int>(kDropLists + slot * 8 + 4);    // 0x00494a17
                const int idx = kFindToken(base, 0, key);  // 0x00494a2d
                item = (idx < 0x100)                       // 0x00494a32 (signed)
                           ? at<const char*>(reinterpret_cast<unsigned>(base) + idx * 0x18 + 0x578)  // 0x00494a3e
                           : 0;                            // 0x00494a47
            }
            one[0] = '^';                                  // 0x00494a54
            appendStr(q, item);                            // the resolved item text
            appendStr(q, one);                             // "^"
            one[0] = ']';
            appendStr(q, one);                             // "]"
            out = endOf(q);                                // 0x00494ad2
            break;
        }

        case '<': {                                        // 0x004945af: "$<...>" gender / plural selection
            memcpy(out, in, prefix);                       // 0x004945ca
            char* w = out + prefix;                        // 0x004945be
            *w = 0;                                        // 0x004945d4
            out = w;
            const char* const gt = kStrstr(m, kGt);        // 0x004945d8
            if (gt == 0) {                                 // 0x004945e2
                in = afterDollar;                          // 0x004945e4
                break;
            }
            const char* p = m + 2;                         // 0x004945f4
            const char* const limit = gt + 1;              // 0x004945f7: also the next input position
            in = limit;                                    // 0x004945fa
            while (*p == ' ') ++p;                         // 0x00494601
            const char c = *p;                             // 0x0049460e
            int explicitIdx = -1;                          // eax, 0x00494610
            int plural = 0;                                // esi, 0x00494613
            if (c == 'M' || c == 'm') gender = 0;          // 0x00494615 / 0x0049461a
            else if (c == 'F' || c == 'f') gender = 1;     // 0x0049461f / 0x00494624
            else if (c == 'N' || c == 'n') gender = 2;     // 0x00494629 / 0x0049462e
            else if (c == '#') {                           // 0x00494633: plural from a number slot
                const int code = static_cast<signed char>(p[1]);       // 0x00494638
                ++p;                                       // 0x0049463c
                plural = (at<int>(kCharNumbers + code * 4) != 1) ? 1 : 0;   // 0x0049463d / 0x00494649
                explicitIdx = code - 0x30;                 // 0x00494644
            } else {                                       // 0x00494657: a text-slot digit
                explicitIdx = static_cast<signed char>(c) - 0x30;
                if (explicitIdx >= 10) continue;           // 0x00494660 (signed) -> next round
                gender = at<int>(kGenderSlots + explicitIdx * 4);       // 0x00494666
                plural = at<int>(kPluralSlots + explicitIdx * 4);       // 0x0049466d
            }
            ++p;                                           // 0x00494695
            if (explicitIdx < 0) {                         // 0x00494698
                const char d = *p;                         // 0x0049469a
                if (d >= '0' && d <= '9') {                // 0x0049469c / 0x004946a0
                    plural = d - 0x31;                     // 0x004946a4: '1' is plural 0
                    ++p;                                   // 0x004946aa
                }
            }
            while (*p == ' ') ++p;                         // 0x004946ab
            if (*p == ':') ++p;                            // 0x004946b8
            const char* list = p;                          // ebx: the alternatives string
            int colons = 0;                                // edi, 0x004946be
            bool haveList = false;
            if (p < limit) {                               // 0x004946c0 (unsigned)
                for (const char* q = p; q < limit; ++q)    // 0x004946c6..0x004946cf
                    if (*q == ':') ++colons;
                haveList = (colons != 0);                  // 0x004946d3
            }
            if (!haveList) {                               // 0x004946d5: take the alternatives from a text slot
                const char* f = kFindDigit(p);             // 0x004946d6
                if (f != 0) {                              // 0x004946e0
                    f = kFindDigit(p);                     // 0x004946e7 (called a second time)
                    const int slot = static_cast<signed char>(*f) - 0x30;   // 0x004946ec
                    if (slot >= 10) continue;              // 0x004946f8 -> next round
                    list = str(kTextSlots + slot * 0x100); // 0x004946fe / 0x00494701
                    if (*list != 0) {                      // 0x0049470b
                        for (const char* q = list; *q; ++q)     // 0x0049470f..0x0049471a
                            if (*q == ':') ++colons;
                        haveList = true;
                    }
                }
            }
            if (haveList) {                                // 0x00494720: which alternative to take
                int pick;
                if (colons == 1) pick = (plural != 0) ? 1 : 0;              // 0x00494723
                else if (colons == 2) pick = gender;                        // 0x00494733
                else if (colons == 3) pick = ((gender != 0) ? 1 : 0) + ((plural != 0) ? 2 : 0);   // 0x0049473c
                else if (colons == 5) pick = ((plural != 0) ? 3 : 0) + gender;                    // 0x00494755
                else pick = colons;                                         // edi unchanged
                while (pick != 0) {                        // 0x00494762..0x00494771
                    const char sep = *list++;
                    if (sep == ':') --pick;
                }
            }
            for (char t = *list; t != 0 && t != ':' && t != '>'; t = *list)  // 0x00494773..0x0049478b
                { *w++ = t; ++list; }
            *w = 0;                                        // 0x0049478d
            out = w;
            break;
        }

        default:
        unknownToken: {                                    // 0x00494ad7: "$<digit>" through the text slots
            const char* const d = kFindDigit(m);           // 0x00494ad8
            if (d == 0) return 0xe;                        // 0x00494ae6
            const int slot = static_cast<signed char>(*d) - 0x30;      // 0x00494ae8
            if (slot > 9) return 0xe;                      // 0x00494af1
            memcpy(out, in, prefix);                       // 0x00494b07
            out += prefix;
            *out = 0;                                      // 0x00494b19
            in = d + 1;                                    // 0x00494b18 / 0x00494b20
            appendStr(out, str(kTextSlots + slot * 0x100));            // 0x00494b47
            out = endOf(out);                              // 0x00494b5a
            break;
        }
        }
    }

    appendStr(out, in);                                    // 0x00494b94: the rest of the input

    if (at<int>(0x0083FE5C) == 1) {                        // 0x00494b9d: elision pass over the finished output
        char* cur = outStart;                              // 0x00494baa / 0x00494bae
        for (;;) {
            char* best = 0;                                // ebx
            int off = 0;                                   // [esp+0x1c]
            for (int i = 0; i < 6; ++i) {                  // 0x00494bb4 .. 0x00494be8 (0x004e4688, stride 0x14)
                char* const f = kStrstr(cur, str(kElisionWords + i * 0x14));
                if (f != 0 && (best == 0 || f < best)) {   // 0x00494bc5 / 0x00494bc9 / 0x00494bcd
                    best = f;
                    off = (i < 3) ? 2 : 1;                 // 0x00494bd3: the 4-char forms elide at +2, the 3-char at +1
                }
            }
            if (best == 0) break;                          // 0x00494bec
            char* e = best + off;                          // 0x00494bf2
            if (isElisionVowel(e[2])) {                    // 0x00494bf4
                *e = '\'';                                 // 0x00494c31
                char t;
                do { t = e[2]; ++e; *e = t; } while (t != 0);   // 0x00494c34..0x00494c3c
            }
            cur = best + 1;                                // 0x00494c3e
        }
    }
    return 0;                                              // 0x00494c48
}

// 0x0045fd80  buildScenarioIntro() --------------------------------------------------------------------------
// Builds the scenario narration into the shared text buffer 0x0051a068 and hands it to startMessage 0x0040cb00.
// Runs only on two values of the game clock 0x00834170: at 0x20 it writes one of four back-stories picked by the
// course-type byte 0x005a34e0 (0x0045fda0, jump table 0x004604dc), unless flag bit 0x01000000 of 0x0059e7b8 is set
// (0x0045fd90), and at 0x1400 it writes a second, fixed message (0x004603e5). Three of the four stories call
// appendCourseTitle 0x0040daa0 with -1 and two of them splice in the cash 0x00571fd4 as (cash*100)/1000
// (0x0045ff25..0x0045ff42, 0x004600ea..0x00460107) through the itoa scratch 0x00824134.
typedef void(__cdecl* Intro_t)();
Intro_t BuildScenarioIntro_orig;
void __cdecl BuildScenarioIntro_re() {
    char* const buf = reinterpret_cast<char*>(0x0051A068);
    if (at<int>(0x00834170) == 0x20) {                     // 0x0045fd86
        if ((at<unsigned>(0x0059E7B8) & 0x01000000) != 0) return;      // 0x0045fd90 / 0x0045fd9a
        const int kind = at<signed char>(0x005A34E0);      // 0x0045fda0 (movsx; cmp 3 / ja at 0x0045fdaa)
        const char* tail = 0;
        char* const scratch = reinterpret_cast<char*>(0x00824134);   // the itoa scratch (0x0045fe9a / 0x0046008a)
        switch (kind) {
        case 0:                                            // 0x0045fdb7
            strcpy(buf, str(0x004D44D4));
            kAppendCourseTitle(-1);                        // 0x0045fdde
            appendStr(buf, str(0x004D44A4));
            appendStr(buf, str(0x004D4470));
            kAppendCourseTitle(-1);                        // 0x0045fe3d
            appendStr(buf, str(0x004D4430));
            appendStr(buf, str(0x004D43F4));
            appendStr(buf, str(0x004D43D4));
            appendStr(buf, str(0x004D43C0));
            appendStr(buf, str(0x004D43B4));
            kItoa((at<int>(0x00571FD4) * 100) / 1000, scratch, 10);    // 0x0045ff25..0x0045ff42
            appendStr(buf, scratch);
            appendStr(buf, str(0x004D4394));
            kAppendCourseTitle(-1);                        // 0x0045ff9e
            tail = str(0x004D4364);                        // 0x0045ffa6
            break;
        case 1:                                            // 0x0045ffb0
            strcpy(buf, str(0x004D4354));
            kAppendCourseTitle(-1);                        // 0x0045ffd7
            appendStr(buf, str(0x004D4324));
            appendStr(buf, str(0x004D42D8));
            appendStr(buf, str(0x004D42BC));
            appendStr(buf, str(0x004D4288));
            appendStr(buf, str(0x004D4240));
            appendStr(buf, str(0x004D4200));
            kItoa((at<int>(0x00571FD4) * 100) / 1000, scratch, 10);    // 0x004600ea..0x00460107
            appendStr(buf, scratch);
            appendStr(buf, str(0x004D41E8));
            kAppendCourseTitle(-1);                        // 0x00460163
            tail = str(0x004D41C4);                        // 0x0046016b
            break;
        case 2:                                            // 0x0046028e
            strcpy(buf, str(0x004D44D4));
            kAppendCourseTitle(-1);                        // 0x004602b5
            appendStr(buf, str(0x004D3FC0));
            appendStr(buf, str(0x004D3F7C));
            appendStr(buf, str(0x004D3F10));
            appendStr(buf, str(0x004D3EBC));
            appendStr(buf, str(0x004D3E7C));
            kAppendCourseTitle(-1);                        // 0x00460395
            tail = str(0x004D3E5C);                        // 0x0046039d
            break;
        case 3:                                            // 0x00460175
            strcpy(buf, str(0x004D44D4));
            kAppendCourseTitle(-1);                        // 0x0046019c
            appendStr(buf, str(0x004D418C));
            appendStr(buf, str(0x004D4138));
            appendStr(buf, str(0x004D40F0));
            appendStr(buf, str(0x004D4094));
            appendStr(buf, str(0x004D4030));
            kAppendCourseTitle(-1);                        // 0x0046027c
            tail = str(0x004D3FFC);                        // 0x00460284
            break;
        default:                                           // 0x0045fdaa: no text is built
            break;
        }
        if (tail != 0) appendStr(buf, tail);               // 0x004603a2
        kStartMessage(0x8000211F, 1, -4);                  // 0x004603ca..0x004603d3
        at<int>(0x005A7144) = 0x80;                        // 0x004603db
    }
    if (at<int>(0x00834170) == 0x1400) {                   // 0x004603e5
        strcpy(buf, str(0x004D3DFC));                      // 0x00460411
        appendStr(buf, str(0x004D3DA0));
        appendStr(buf, str(0x004D3D58));
        appendStr(buf, str(0x004D3D20));
        appendStr(buf, str(0x004D3CB8));
        kStartMessage(0x80002190, 0, -4);                  // 0x004604c6..0x004604d0
    }
}

// 0x004604f0  showTutorialText() ----------------------------------------------------------------------------
// Shows one page of the tutorial. The page number is the dword 0x00567a18; the switch at 0x00460515 (jump table
// 0x00460d70, 29 entries selected by page-1, `ja` at 0x0046050f) picks the page, and every page that exists
// builds its text into 0x0051a068, hands it to startMessage 0x0040cb00 with (0x80002190, 1, -1) and advances
// 0x00567a18 by one (0x00460b9a / 0x00460c21 / 0x00460ce3). 16 of the pages first point the camera
// 0x004c2ba0/0x004c2ba4 at the feature they talk about, computed from the hole corner tiles 0x00575cc0/0x00575cc4
// (+0x10/+0x14 at 0x00575cd0/0x00575cd4), the second hole's 0x00575ec8/0x00575ecc (+0x10/0x14 at 0x00575ed8/
// 0x00575edc), the third's 0x005760d0/0x005760d4 (+0x10/+0x14 at 0x005760e0/0x005760e4) or the placed-object
// words 0x0058bcba/0x0058bcbc and 0x0058bd0a/0x0058bd0c; eight of them also set the view mode 0x004c2844.
// Pages 11 and 29 share the block at 0x00460cea, which saves the game (saveGame 0x0040b4a0) before its message,
// then clears bit 0x8000 of 0x0059e7b8 (0x00460d58) and resets 0x00567a18 to 0.
typedef void(__cdecl* Tutorial_t)();
Tutorial_t ShowTutorialText_orig;
void __cdecl ShowTutorialText_re() {
    char* const buf = reinterpret_cast<char*>(0x0051A068);
    int& page = at<int>(0x00567A18);
    int& camX = at<int>(0x004C2BA0);
    int& camY = at<int>(0x004C2BA4);
    at<int>(0x00543D04) = -1;                              // 0x004604fb
    at<int>(0x005685F4) = 0;                               // 0x00460505
    if (static_cast<unsigned>(page - 1) > 0x1c) return;    // 0x004604f6..0x0046050f

    switch (page - 1) {                                    // 0x00460515, table 0x00460d70
    case 0:                                                // 0x0046051c
        at<int>(0x004C2844) = 2;                           // 0x00460526
        strcpy(buf, str(0x004D5140));
        appendStr(buf, str(0x004D50F4));
        break;
    case 1:                                                // 0x00460555
        camY = at<int>(0x00575CC4) - 8;                    // 0x00460560 / 0x00460566
        camX = at<int>(0x00575CC0) + 8;                    // 0x00460563 / 0x0046056c
        at<int>(0x004C2844) = 4;                           // 0x0046057b
        strcpy(buf, str(0x004D50A0));
        appendStr(buf, str(0x004D504C));
        appendStr(buf, str(0x004D5028));
        appendStr(buf, str(0x004D4FE4));
        break;
    case 2:                                                // 0x00460600
        camX = (at<int>(0x00575CC0) + at<int>(0x00575CD0)) / 2 + 5;    // 0x0046060b..0x00460623
        camY = (at<int>(0x00575CC4) + at<int>(0x00575CD4)) / 2 - 5;    // 0x00460628..0x00460637
        strcpy(buf, str(0x004D4FB8));
        appendStr(buf, str(0x004D4F60));
        appendStr(buf, str(0x004D4F30));
        appendStr(buf, str(0x004D4F0C));
        break;
    case 3:                                                // 0x004606b9
        camX = (at<int>(0x00575ED8) + at<int>(0x00575EC8)) / 2 + 8;    // 0x004606c4..0x004606dc
        camY = (at<int>(0x00575EDC) + at<int>(0x00575ECC)) / 2 - 4;    // 0x004606e1..0x004606f0
        strcpy(buf, str(0x004D4EA8));
        appendStr(buf, str(0x004D4E38));
        break;
    case 4:                                                // 0x0046071c
        camX = (at<int>(0x00575EC8) + at<int>(0x00575ED8)) / 2 + 4;    // 0x00460727..0x0046073f
        camY = (at<int>(0x00575ECC) + at<int>(0x00575EDC)) / 2 - 4;    // 0x00460744..0x00460753
        strcpy(buf, str(0x004D4DCC));
        appendStr(buf, str(0x004D4D74));
        break;
    case 5:                                                // 0x0046077f
        camX = (at<int>(0x005760D0) + at<int>(0x005760E0)) / 2 + 4;    // 0x0046078a..0x004607a2
        camY = (at<int>(0x005760D4) + at<int>(0x005760E4)) / 2 - 4;    // 0x004607a7..0x004607b6
        strcpy(buf, str(0x004D4D28));
        appendStr(buf, str(0x004D4CD4));
        appendStr(buf, str(0x004D4CB8));
        break;
    case 6:                                                // 0x0046080d: no camera move
        strcpy(buf, str(0x004D4C88));
        appendStr(buf, str(0x004D4C40));
        break;
    case 7:                                                // 0x0046083c
        camX = at<short>(0x0058BD0A) + 4;                  // 0x0046083c / 0x0046084a / 0x00460850
        camY = at<short>(0x0058BD0C) - 4;                  // 0x00460843 / 0x0046084d / 0x00460855
        strcpy(buf, str(0x004D4BD4));
        appendStr(buf, str(0x004D4B9C));
        break;
    case 8:                                                // 0x0046088a: no camera move
        strcpy(buf, str(0x004D4B44));
        appendStr(buf, str(0x004D4B18));
        break;
    case 9:                                                // 0x004608b9
        camX = at<short>(0x0058BCBA) + 4;                  // 0x004608b9 / 0x004608c7 / 0x004608cd
        camY = at<short>(0x0058BCBC) - 4;                  // 0x004608c0 / 0x004608ca / 0x004608d2
        at<int>(0x004C2844) = 2;                           // 0x004608e2
        strcpy(buf, str(0x004D4ACC));
        appendStr(buf, str(0x004D4A48));
        break;
    case 20:                                               // 0x00460911
        camX = at<int>(0x00575EC8) + 8;                    // 0x0046091c / 0x00460922
        camY = at<int>(0x00575ECC) - 8;                    // 0x0046091f / 0x00460927
        at<int>(0x004C2844) = 2;                           // 0x00460937
        strcpy(buf, str(0x004D49D0));
        appendStr(buf, str(0x004D4990));
        break;
    case 21:                                               // 0x00460966
        camY = at<int>(0x00575ECC) - 8;                    // 0x00460971 / 0x00460977
        camX = at<int>(0x00575EC8) + 8;                    // 0x00460974 / 0x0046097d
        at<int>(0x004C2844) = 2;                           // 0x0046098c
        strcpy(buf, str(0x004D4930));
        appendStr(buf, str(0x004D48C8));
        appendStr(buf, str(0x004D48A8));
        appendStr(buf, str(0x004D4FE4));
        break;
    case 22:                                               // 0x00460a11
        camX = at<int>(0x00575CC0) + 5;                    // 0x00460a1c / 0x00460a22
        camY = at<int>(0x00575CC4) - 10;                   // 0x00460a1f / 0x00460a27
        at<int>(0x004C2844) = 4;                           // 0x00460a37
        strcpy(buf, str(0x004D4858));
        appendStr(buf, str(0x004D4804));
        appendStr(buf, str(0x004D47E0));
        break;
    case 23:                                               // 0x00460a91
        camX = at<int>(0x00575EC8) + 5;                    // 0x00460a9c / 0x00460aa2
        camY = at<int>(0x00575ECC) - 5;                    // 0x00460a9f / 0x00460aa7
        strcpy(buf, str(0x004D477C));
        appendStr(buf, str(0x004D4744));
        break;
    case 24:                                               // 0x00460adc: one string, no append
        camX = at<int>(0x00575EC8) + 4;                    // 0x00460ae7 / 0x00460aed
        camY = at<int>(0x00575ECC) - 4;                    // 0x00460aea / 0x00460af2
        strcpy(buf, str(0x004D4704));                      // 0x00460af8, copied at 0x00460cbf
        break;
    case 25:                                               // 0x00460b16
        camX = at<int>(0x005760D0) + 6;                    // 0x00460b21 / 0x00460b27
        camY = at<int>(0x005760D4) - 6;                    // 0x00460b24 / 0x00460b2c
        strcpy(buf, str(0x004D467C));
        appendStr(buf, str(0x004D4630));
        break;
    case 26:                                               // 0x00460ba1
        camX = at<int>(0x005760E0) + 1;                    // 0x00460bac / 0x00460bae
        camY = at<int>(0x005760E4) - 1;                    // 0x00460bad / 0x00460bb3
        strcpy(buf, str(0x004D4620));
        appendStr(buf, str(0x004D45C4));
        break;
    case 27:                                               // 0x00460c28
        camX = at<int>(0x00575EC8) + 8;                    // 0x00460c33 / 0x00460c39
        camY = at<int>(0x00575ECC) - 8;                    // 0x00460c36 / 0x00460c3e
        at<int>(0x004C2844) = 2;                           // 0x00460c4e
        strcpy(buf, str(0x004D4588));
        appendStr(buf, str(0x004D4530));
        appendStr(buf, str(0x004D4500));
        break;
    case 10:                                               // 0x00460cea, shared with case 28: saves first
    case 28:
        kSaveGame(str(0x004D44FC));                        // 0x00460cef
        camX = at<short>(0x0058BCBA) + 4;                  // 0x00460cf4 / 0x00460d02 / 0x00460d08
        camY = at<short>(0x0058BCBC) - 4;                  // 0x00460cfb / 0x00460d05 / 0x00460d0d
        at<int>(0x004C2844) = 4;                           // 0x00460d3c
        strcpy(buf, str(0x004D44E0));                      // 0x00460d38
        kStartMessage(0x80002190, 1, -1);                  // 0x00460d4b
        {
            const unsigned f = at<unsigned>(0x0059E7B8);   // 0x00460d50
            page = 0;                                      // 0x00460d5b
            at<unsigned>(0x0059E7B8) = f & ~0x8000u;       // 0x00460d58 (and ah, 0x7f) / 0x00460d65
        }
        return;
    default:                                               // pages 12..20: the table sends them to 0x00460d6a
        return;
    }
    kStartMessage(0x80002190, 1, -1);                      // 0x00460cc6..0x00460cd4
    page = page + 1;                                       // 0x00460cd9 / 0x00460ce3 (re-read after the call)
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004942F0, expandTextMarkup, ExpandTextMarkup_re, ExpandTextMarkup_orig);
SG_HOOK("golf_clean.exe", 0x0045FD80, buildScenarioIntro, BuildScenarioIntro_re, BuildScenarioIntro_orig);
SG_HOOK("golf_clean.exe", 0x004604F0, showTutorialText, ShowTutorialText_re, ShowTutorialText_orig);
