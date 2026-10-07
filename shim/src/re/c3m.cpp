// C3 batch c3m (2026-10-07): golfer/course text builders and the new-golfer generator of golf_clean.exe, hand-written
// from the C2 transcriptions re/analysis/<subsystem>/<addr>_<name>.md and the disassembly (re/tools/asm2inline.py);
// none of the four is matched. Each body cites the address every field offset, table and constant comes from. All
// four are __cdecl. Callees (__itoa, landmarkName, postEvent, Random::range, typeBit7Clear) are reached through their
// original addresses, so a hooked callee runs its own reimplementation on both arms of the A/B.
#include <string.h>

#include "hooks.h"

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }
inline const char* str(unsigned addr) { return reinterpret_cast<const char*>(addr); }

char* const kText = reinterpret_cast<char*>(0x0051a068);  // shared text buffer (0x0051a068)

// The golfer records: 0x98 of 0x100 bytes from 0x005794b8 (updateMembership clears 0x40 dwords there, 0x00421bfd).
// Field addresses below are the record-0 addresses the instructions use, indexed with golfer << 8.
inline unsigned golfer(int g) { return static_cast<unsigned>(g) << 8; }

typedef char*(__cdecl* Itoa_t)(int value, char* buf, int radix);                 // 0x004ad425 __itoa
typedef void(__cdecl* LandmarkName_t)(int id, int longForm);                     // 0x004074a0 landmarkName
typedef void(__cdecl* PostEvent_t)(int kind, int x, int y);                      // 0x0046e7b0 postEvent
typedef int(__fastcall* Range_t)(void* self, void* edx, int n);                 // 0x0045c1e0 Random::range
typedef int(__cdecl* Golfer1_t)(int golfer);                                     // 0x0046c940 typeBit7Clear

const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004ad425);
const LandmarkName_t kLandmarkName = reinterpret_cast<LandmarkName_t>(0x004074a0);
const PostEvent_t kPostEvent = reinterpret_cast<PostEvent_t>(0x0046e7b0);
const Range_t kRange = reinterpret_cast<Range_t>(0x0045c1e0);
const Golfer1_t kTypeBit7Clear = reinterpret_cast<Golfer1_t>(0x0046c940);
void* const kRng = reinterpret_cast<void*>(0x00822d9c);  // the generator object every caller here passes in ecx

inline int range(int n) { return kRange(kRng, 0, n); }

// ------------------------------------------------------------------------------------------------ buildGolferName

// 0x004676e0  buildGolferName(golfer, withTitle): appends golfer's display name to the text buffer.
// withTitle != 0 (test at 0x004676e7) first appends the string at 0x004c52b8. The golfer's kind byte is
// [0x005794d0 + golfer*0x100] (0x00467723): its top three bits (& 0xe0) select the form through the byte/dword jump
// tables 0x004678cc / 0x004678b4 (index = kind - 0x20, `ja` to the default at 0x0046773d), its low five bits (& 0x1f)
// are the sub-kind:
//   0x20 (0x004677ee): sub-kind 4 -> the string at 0x004d6098; otherwise the 0x38-byte name slot
//        0x0058dd50 + byte[0x00579573 + golfer*0x100] * 0x38 (lea/sub/lea at 0x004677fd..0x00467806).
//   0x40 (0x00467752): the string *(char**)0x004c2c10, then a numeral suffix chosen by dword[0x0053a450] & 0x7f.
//   0x60 (0x00467816): the string *(char**)0x004c2c18, then a numeral suffix chosen by the sub-kind.
//        Suffix (tables 0x004679b0 / 0x004679d4, identical): 0 appends nothing more (0x004678ae); 1..8 the strings
//        0x004e1bd0, 0x004e1bc8, 0x004e1bc4, 0x004e1bc0, 0x004e1bbc, 0x004e1bb4, 0x004e1bac, 0x004e1ba8; above 8
//        (unsigned `ja` at 0x0046778b / 0x00467847) 0x004e1ba4.
//   0x80 (0x00467850): 0x004e1b94 when bit 0 of golfer is set, else 0x004e1b80.
//   0x00, 0xa0, 0xc0, 0xe0 (default, 0x0046786a): the golfer-type name 0x004d6098 + (short)[0x0057956e +
//        golfer*0x100] * 0x230.
// The table entry for index 0xe0 (0x00467861, a read of 0x004c2bd4) would need kind 0x100, so no byte reaches it.
// Every append is the inline strcat sequence (repne scasb / rep movs into 0x0051a068).
typedef void(__cdecl* BuildGolferName_t)(int golfer, int withTitle);
BuildGolferName_t BuildGolferName_orig;
void __cdecl BuildGolferName_re(int g, int withTitle) {
    static const unsigned kSuffix[9] = {0, 0x004e1bd0, 0x004e1bc8, 0x004e1bc4, 0x004e1bc0,
                                        0x004e1bbc, 0x004e1bb4, 0x004e1bac, 0x004e1ba8};
    if (withTitle != 0) strcat(kText, str(0x004c52b8));
    const unsigned char kind = at<unsigned char>(0x005794d0 + golfer(g));
    const unsigned sub = kind & 0x1f;
    unsigned suffix;
    switch (kind & 0xe0) {
    case 0x20:
        if (sub == 4) strcat(kText, str(0x004d6098));
        else strcat(kText, str(0x0058dd50 + at<unsigned char>(0x00579573 + golfer(g)) * 0x38));
        return;
    case 0x40:
        strcat(kText, *reinterpret_cast<const char**>(0x004c2c10));
        suffix = at<unsigned>(0x0053a450) & 0x7f;
        break;
    case 0x60:
        strcat(kText, *reinterpret_cast<const char**>(0x004c2c18));
        suffix = sub;
        break;
    case 0x80:
        strcat(kText, str((g & 1) ? 0x004e1b94 : 0x004e1b80));
        return;
    default:
        strcat(kText, str(0x004d6098 + at<short>(0x0057956e + golfer(g)) * 0x230));
        return;
    }
    if (suffix == 0) return;
    strcat(kText, str(suffix <= 8 ? kSuffix[suffix] : 0x004e1ba4));
}

// ------------------------------------------------------------------------------------------------- decorationName

// Tile grids, 50 x 50, cell = x*50 + y: type bytes 0x005722e8 (signed, movsx at 0x00407719), object/variant bytes
// 0x0056988c, flag words 0x0053caf0. Placed objects 0x0058bcb8, 16 bytes each: kind word +0, x word +2, y word +4,
// landmark id dword +8. Per-type category byte 0x00578376 + type*0x30 (signed, 0x00407778).
inline int cell(int x, int y) { return x * 50 + y; }

// 0x00407700  decorationName(x, y, id): appends the scenery name of a tile to the text buffer and returns 1 on every
// path (ebx = 1 at 0x00407773, eax = 1 at 0x00407a90 / 0x00407ad7).
// The tile type: id == -1 (0x00407705) takes the signed type byte of cell (x, y); id with bit 0x100 set (`test ch,1`
// at 0x00407723) names placed object id & 0xff and takes the type byte at that object's (x, y) words (0x00407734 /
// 0x0040773b); otherwise id is the type.
// Types 0x16 and 0x15 (0x0040775d / 0x00407766) look up the object o = byte[0x0056988c + cell] (0x00407a66): object
// kind 4 calls landmarkName(dword[0x0058bcc0 + o*16], 0) (0x00407a88); kind 2 appends 0x004c4c60; else 0x004c4980 for
// type 0x16 and 0x004c4c54 for 0x15.
// Any other type switches on its category c (tables 0x00407b08 / 0x00407ae4, index c - 4, unsigned `ja` at 0x00407785
// to the default 0x004c4c6c): 7 -> 0x004c4c78; 0xc -> 0x004c4df4; 0x12 -> 0x004c4de8; 0x13 -> 0x004c4e28; 0x15/0x16 ->
// 0x004c4980; categories 4, 0xd and 0x11 test further (course type = byte 0x005a34e0, course record byte
// 0x00571ff4 + dword[0x0059bf90] * 0x2e):
//   0xd (0x00407813): type 0x10 -> 0x004c4ddc; course type 1 -> 0x004c4dcc; type 0xd -> 0x004c4ddc; type 0xf ->
//       0x004c4dc4; type 0xe -> 0x004c4ddc; any other type appends nothing (0x00407855).
//   0x11 (0x004077ae): type 0x13 -> 0x004c4e1c; flag word bit 0x20 (0x004077c6) -> 0x004c4e0c; object byte non-zero and
//       cell type 0x11 (0x004077e2 / 0x004077e4) -> 0x004c4e04; else 0x004c4df4 for course type 1, 0x004c4de8 otherwise.
//   4 (0x00407865): type 0xa -> 0x004c4df4 (course type 1) / 0x004c4ddc; type 0xc -> 0x004c4db4 (course type 1) /
//       0x004c4df4; other types except 4 -> 0x004c4da8 when the course record byte is 0xd, else 0x004c4d9c.
//       Type 4 with flag word bit 0x1000 clear (0x004078f0): 0x004c4cac when the course record byte is 0xa, else
//       0x004c4c98 (course type 1) / 0x004c4c84. With the bit set the variant v = object byte % 5 picks one of five
//       strings for course type 1 (0x00407b1c: 0x004c4d8c, 0x004c4d78, 0x004c4d64, 0x004c4d54, 0x004c4d44), 2
//       (0x00407b30: 0x004c4d34, 0x004c4d1c, 0x004c4d0c, 0x004c4cfc, 0x004c4cec), 0 or 3 (0x00407b44: 0x004c4cdc,
//       0x004c4cc8, 0x004c4d64, 0x004c4cb8, 0x004c4cec); any other course type appends nothing (0x004079b4).
typedef int(__cdecl* DecorationName_t)(int x, int y, int id);
DecorationName_t DecorationName_orig;
int __cdecl DecorationName_re(int x, int y, int id) {
    int type;
    if (id == -1) {
        type = at<signed char>(0x005722e8 + cell(x, y));
    } else if (id & 0x100) {
        const unsigned o = (id & 0xff) * 16;
        x = at<short>(0x0058bcba + o);
        y = at<short>(0x0058bcbc + o);
        type = at<signed char>(0x005722e8 + cell(x, y));
    } else {
        type = id;
    }
    const int c = cell(x, y);

    if (type == 0x16 || type == 0x15) {
        const unsigned o = at<unsigned char>(0x0056988c + c) * 16;
        const short kind = at<short>(0x0058bcb8 + o);
        if (kind == 4) {
            kLandmarkName(at<int>(0x0058bcc0 + o), 0);
            return 1;
        }
        unsigned s;
        if (kind == 2) s = 0x004c4c60;
        else s = (type == 0x16) ? 0x004c4980 : 0x004c4c54;
        strcat(kText, str(s));
        return 1;
    }

    // Read on use only, as the original does: x and y are arbitrary when id names the type directly.
    const signed char courseType = at<signed char>(0x005a34e0);
    auto courseRec = [] { return at<unsigned char>(0x00571ff4 + at<int>(0x0059bf90) * 0x2e); };
    auto flags = [c] { return at<unsigned short>(0x0053caf0 + c * 2); };
    auto objByte = [c] { return at<unsigned char>(0x0056988c + c); };
    const int category = at<signed char>(0x00578376 + type * 0x30);
    unsigned s;
    switch (category) {
    case 4:
        if (type == 4) {
            if ((flags() & 0x1000) == 0) {
                if (courseRec() == 0xa) s = 0x004c4cac;
                else s = (courseType == 1) ? 0x004c4c98 : 0x004c4c84;
            } else {
                static const unsigned kCt1[5] = {0x004c4d8c, 0x004c4d78, 0x004c4d64, 0x004c4d54, 0x004c4d44};
                static const unsigned kCt2[5] = {0x004c4d34, 0x004c4d1c, 0x004c4d0c, 0x004c4cfc, 0x004c4cec};
                static const unsigned kCt03[5] = {0x004c4cdc, 0x004c4cc8, 0x004c4d64, 0x004c4cb8, 0x004c4cec};
                const int v = objByte() % 5;
                if (courseType == 1) s = kCt1[v];
                else if (courseType == 2) s = kCt2[v];
                else if (courseType == 0 || courseType == 3) s = kCt03[v];
                else return 1;
            }
        } else if (type == 0xa) {
            s = (courseType == 1) ? 0x004c4df4 : 0x004c4ddc;
        } else if (type == 0xc) {
            s = (courseType == 1) ? 0x004c4db4 : 0x004c4df4;
        } else {
            s = (courseRec() == 0xd) ? 0x004c4da8 : 0x004c4d9c;
        }
        break;
    case 7: s = 0x004c4c78; break;
    case 0xc: s = 0x004c4df4; break;
    case 0xd:
        if (type == 0x10) s = 0x004c4ddc;
        else if (courseType == 1) s = 0x004c4dcc;
        else if (type == 0xd) s = 0x004c4ddc;
        else if (type == 0xf) s = 0x004c4dc4;
        else if (type == 0xe) s = 0x004c4ddc;
        else return 1;
        break;
    case 0x11:
        if (type == 0x13) s = 0x004c4e1c;
        else if (flags() & 0x20) s = 0x004c4e0c;
        else if (objByte() != 0 && at<unsigned char>(0x005722e8 + c) == 0x11) s = 0x004c4e04;
        else s = (courseType == 1) ? 0x004c4df4 : 0x004c4de8;
        break;
    case 0x12: s = 0x004c4de8; break;
    case 0x13: s = 0x004c4e28; break;
    case 0x15:
    case 0x16: s = 0x004c4980; break;
    default: s = 0x004c4c6c; break;
    }
    strcat(kText, str(s));
    return 1;
}

// ----------------------------------------------------------------------------------------------- announceHoleType

// 0x00460df0  announceHoleType(hole, kind): copies the string 0x004d5538 over the text buffer (strcpy, 0x00460e15),
// appends __itoa(hole, 0x00824134, 10) (0x00460e24) and the string 0x004d5514, then switches on kind - 1 (table
// 0x004610e8, unsigned `ja` at 0x00460e89 for kind outside 1..7: nothing more):
//   1 -> 0x004d54bc; 2 -> 0x004d5450; 4 -> 0x004d532c (append only).
//   3 -> 0x004d53c0, 5 -> 0x004d52b8, 6 -> 0x004d522c, each followed, when dword[0x00822c88] >= 2 (signed `jl` at
//        0x00460f2d / 0x00460fd1 / 0x00461044), by postEvent(0 / 1 / 4, hx, hy).
//   7 -> 0x004d51a4 then postEvent(9, hx, hy) unconditionally (0x004610db).
// hx = dword[0x00575ac8 + hole*0x208] * 0x400 + 0x200, hy = dword[0x00575acc + hole*0x208] * 0x400 + 0x200 (hole *
// 0x208 as shl 6 / add / shl 3 at 0x00460f33..0x00460f3a; shl 0xa / add 0x200 at 0x00460f49..0x00460f55).
typedef void(__cdecl* AnnounceHoleType_t)(int hole, int kind);
AnnounceHoleType_t AnnounceHoleType_orig;
void __cdecl AnnounceHoleType_re(int hole, int kind) {
    static const unsigned kText1to7[7] = {0x004d54bc, 0x004d5450, 0x004d53c0, 0x004d532c,
                                          0x004d52b8, 0x004d522c, 0x004d51a4};
    strcpy(kText, str(0x004d5538));
    strcat(kText, kItoa(hole, reinterpret_cast<char*>(0x00824134), 10));
    strcat(kText, str(0x004d5514));
    const unsigned k = static_cast<unsigned>(kind) - 1;
    if (k > 6) return;
    strcat(kText, str(kText1to7[k]));
    int event;
    switch (kind) {
    case 3: event = 0; break;
    case 5: event = 1; break;
    case 6: event = 4; break;
    case 7: event = 9; break;
    default: return;  // kinds 1, 2, 4 post nothing
    }
    if (kind != 7 && at<int>(0x00822c88) < 2) return;
    const unsigned h = static_cast<unsigned>(hole) * 0x208;
    kPostEvent(event, at<int>(0x00575ac8 + h) * 0x400 + 0x200, at<int>(0x00575acc + h) * 0x400 + 0x200);
}

// ----------------------------------------------------------------------------------------------- updateMembership

// 0x00421bc0  updateMembership(membersOnly): generates a new golfer in the next record slot and returns the slot, or
// -1 with the decline message in the text buffer.
// Slot s = counter % 0x98 (signed idiv, counter = dword 0x0059ae7c, which is incremented, 0x00421bd8). The record
// (0x40 dwords at 0x005794b8 + s*0x100) is zeroed (0x00421bfd); position dwords +0 / +4 = dword[0x00578150] / [0x00578154]
// * 0x400 + 0x600; byte +0x1a (0x005794d2) = byte[0x00575cb9]; word 0x00579570 = Random::range(0x7fff) (0x00421c34).
// Category pick over the 8 counts of row s & 3 of the dword table 0x0059dea0 (8 per row): a category c is eligible when
// bit 0x40 of dword 0x0059e7b8 is set, or c >= 3 and c != 4 (0x00421c56..0x00421c60). The first pass finds the minimum
// eligible count m (start 9999 = 0x270f) and counts the ties n: a count below m resets n to 1, an equal count
// increments n (0x00421c73 / 0x00421c7e); n starts as membersOnly (ebx = [esp+0x24], 0x00421c3f). The second pass
// draws r = Random::range(n) & 0xffff and walks the eligible categories again: each count equal to m decrements r, and
// when r goes negative (`jns` at 0x00421cc2) the category is stored in byte 0x005794d1 and r becomes 99. Then
// dword[0x0059dea0 + (byte 0x005794d1 + (signed byte 0x00579572) * 8) * 4] is incremented (0x00421cef).
// Type draw, up to 999 attempts (counter [esp+0x18], 0x00421e74): type t = Random::range(0x4b) + 1 into word 0x0057956e
// (0x00421d04). Category byte 0x005794d1 = 6 when typeBit7Clear(s) is 0; otherwise from type flags byte
// f = [0x004d60a8 + t*0x230]: 1, or 3 when f & 0x11, then | 4 when f & 0xc, and a plain 1 becomes 3 for an odd slot
// and 5 for an even one (neg/sbb/and 0xfe/add 4/or 1 at 0x00421d55..0x00421d62). A non-zero dword
// [0x004d60b4 + t*0x230] overrides it with its low 3 bits (0x00421d8e).
// The type is refused when another golfer j (j != s, 0x98 records, type word 0x0057956e + j*0x100, status byte
// 0x005794d9 + j*0x100) has status 0xff and t % 19 == its type % 19 or the same type (0x00421dde / 0x00421dfb), or has
// a positive status (signed `jle` at 0x00421e07) and the same type; also when byte [0x00584a09 + t*0x2c] is 0xff
// (0x00421e48), and when membersOnly != 0 and byte [0x005849e2 + t*0x2c] is 0 (0x00421e69).
// Accepted (0x00421ed2): word 0x00579570 = (Random::range(0x80) << 8) + (signed byte)[0x004d60a9 + t*0x230]; byte
// 0x00579572 = t % 3; byte 0x005794d9 = 0xff; byte 0x005794d5 = 0xb; dword 0x00579578 = dword[0x00834170]; word
// 0x0057955c = Random::range(3) + 3 (ecx = 3 from the idiv is the pushed argument, 0x00421f1f), = 4 when dword
// 0x00822c88 is 0, = dword[0x00543cc4] + 4 when that dword is non-zero; dword 0x005794c8 |= 0x10000 when dword
// 0x00543ccc is non-zero. Returns s.
// 999 refusals (0x00421e7d): strcpy of 0x004c6cac into the text buffer, counter = (counter - 1) % 0x98 (signed idiv
// of the already-incremented counter, 0x00421eb6..0x00421ec5), returns -1. (The `jl` at 0x00421e8c is always taken:
// the attempt counter is at most 998 there.)
typedef int(__cdecl* UpdateMembership_t)(int membersOnly);
UpdateMembership_t UpdateMembership_orig;
int __cdecl UpdateMembership_re(int membersOnly) {
    const int counter = at<int>(0x0059ae7c);
    const int s = counter % 0x98;
    at<int>(0x0059ae7c) = counter + 1;
    const unsigned r = golfer(s);
    memset(reinterpret_cast<void*>(0x005794b8 + r), 0, 0x100);
    at<int>(0x005794b8 + r) = at<int>(0x00578150) * 0x400 + 0x600;
    at<int>(0x005794bc + r) = at<int>(0x00578154) * 0x400 + 0x600;
    at<unsigned char>(0x005794d2 + r) = at<unsigned char>(0x00575cb9);
    at<short>(0x00579570 + r) = static_cast<short>(range(0x7fff));

    const bool all = (at<unsigned>(0x0059e7b8) & 0x40) != 0;
    int* const counts = reinterpret_cast<int*>(0x0059dea0) + (s & 3) * 8;
    int m = 9999, n = membersOnly;
    for (int c = 0; c < 8; c++) {
        if (!all && (c < 3 || c == 4)) continue;
        if (counts[c] < m) { m = counts[c]; n = 1; }
        else if (counts[c] == m) n++;
    }
    int pick = range(n) & 0xffff;
    for (int c = 0; c < 8; c++) {
        if (!all && (c < 3 || c == 4)) continue;
        if (counts[c] != m) continue;
        if (--pick < 0) {
            at<unsigned char>(0x005794d1 + r) = static_cast<unsigned char>(c);
            pick = 99;
        }
    }
    reinterpret_cast<int*>(0x0059dea0)[at<unsigned char>(0x005794d1 + r) + at<signed char>(0x00579572 + r) * 8]++;

    for (int attempt = 0; attempt < 999; attempt++) {
        at<short>(0x0057956e + r) = static_cast<short>(range(0x4b) + 1);
        const int t = at<short>(0x0057956e + r);
        const unsigned tr = static_cast<unsigned>(t) * 0x230;
        unsigned char cat;
        if (kTypeBit7Clear(s) == 0) {
            cat = 6;
        } else {
            const unsigned char f = at<unsigned char>(0x004d60a8 + tr);
            cat = 1;
            if (f & 0x11) cat = 3;
            if (f & 0xc) cat |= 4;
            if (cat == 1) cat = (s & 1) ? 3 : 5;
        }
        at<unsigned char>(0x005794d1 + r) = cat;
        const unsigned override_ = at<unsigned>(0x004d60b4 + tr);
        if (override_ != 0) at<unsigned char>(0x005794d1 + r) = static_cast<unsigned char>(override_ & 7);

        bool ok = true;
        for (int j = 0; j < 0x98; j++) {
            if (j == s) continue;
            const signed char status = at<signed char>(0x005794d9 + golfer(j));
            const short other = at<short>(0x0057956e + golfer(j));
            if (status == -1) {
                if (other % 19 == t % 19) ok = false;
                if (other == t) ok = false;
            } else if (status > 0 && other == t) {
                ok = false;
            }
        }
        const unsigned tt = static_cast<unsigned>(t) * 0x2c;
        if (at<unsigned char>(0x00584a09 + tt) == 0xff) ok = false;
        if (membersOnly != 0 && at<unsigned char>(0x005849e2 + tt) == 0) continue;
        if (!ok) continue;

        at<short>(0x00579570 + r) =
            static_cast<short>((range(0x80) << 8) + at<signed char>(0x004d60a9 + tr));
        at<signed char>(0x00579572 + r) = static_cast<signed char>(t % 3);
        at<unsigned char>(0x005794d9 + r) = 0xff;
        at<unsigned char>(0x005794d5 + r) = 0xb;
        at<int>(0x00579578 + r) = at<int>(0x00834170);
        at<short>(0x0057955c + r) = static_cast<short>(range(3) + 3);
        if (at<int>(0x00822c88) == 0) at<short>(0x0057955c + r) = 4;
        if (at<int>(0x00543cc4) != 0) at<short>(0x0057955c + r) = static_cast<short>(at<int>(0x00543cc4) + 4);
        if (at<int>(0x00543ccc) != 0) at<unsigned>(0x005794c8 + r) |= 0x10000;
        return s;
    }
    strcpy(kText, str(0x004c6cac));
    at<int>(0x0059ae7c) = (at<int>(0x0059ae7c) - 1) % 0x98;
    return -1;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004676e0, buildGolferName, BuildGolferName_re, BuildGolferName_orig);
SG_HOOK("golf_clean.exe", 0x00407700, decorationName, DecorationName_re, DecorationName_orig);
SG_HOOK("golf_clean.exe", 0x00460df0, announceHoleType, AnnounceHoleType_re, AnnounceHoleType_orig);
SG_HOOK("golf_clean.exe", 0x00421bc0, updateMembership, UpdateMembership_re, UpdateMembership_orig);
