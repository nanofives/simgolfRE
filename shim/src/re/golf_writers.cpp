// Writer leaves of golf_clean.exe (message/popup rings, golfer and tick tables, text buffer),
// A/B'd with diff_hook state regions. Reimplemented from re/analysis/<subsystem>/<addr>_<name>.md.
#include <string.h>

#include "hooks.h"

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }
char* const kText = reinterpret_cast<char*>(0x0051a068);  // shared text buffer

typedef void(__cdecl* V2_t)(int, int);
typedef void(__cdecl* V1_t)(int);
typedef void(__cdecl* V3_t)(int, int, int);
typedef void(__cdecl* V4_t)(int, int, int, int);
typedef void(__cdecl* V0_t)();

// 0x0040c720  queueMessage(a, b, c): slot = the index at 0x0053bba8; a, b, c to the dword arrays 0x0056c770,
// 0x0056c794, 0x0056a794; 0x30 to 0x0056a924; strcpy of the text buffer 0x0051a068 into the 64-byte slot at
// 0x0056c570 + slot*64; index = (slot + 1) % 8 (signed remainder, 0x0040c781..0x0040c78f).
V3_t QueueMessage_orig;
void __cdecl QueueMessage_re(int a, int b, int c) {
    const int slot = at<int>(0x0053bba8);
    at<int>(0x0056c770 + slot * 4) = a;
    at<int>(0x0056c794 + slot * 4) = b;
    at<int>(0x0056a794 + slot * 4) = c;
    at<int>(0x0056a924 + slot * 4) = 0x30;
    strcpy(reinterpret_cast<char*>(0x0056c570 + slot * 64), kText);
    at<int>(0x0053bba8) = (slot + 1) % 8;
}

// 0x0040c890  pointsPopup(pts, x, y, who): nothing when pts == 0 or bit 0x1000000 of 0x0059e7b8 is set; when
// who != -1 subtracts pts from the dword at 0x00575ca0 + who*0x208; slot = 0x0059abb0: x, y, pts, 0x18 to
// 0x00542fd8, 0x00542ff8, 0x00542dd8, 0x00542f00; slot = (slot + 1) % 8 (signed).
V4_t PointsPopup_orig;
void __cdecl PointsPopup_re(int pts, int x, int y, int who) {
    if (!pts || (at<int>(0x0059e7b8) & 0x1000000)) return;
    if (who != -1) at<int>(0x00575ca0 + who * 0x208) -= pts;
    const int slot = at<int>(0x0059abb0);
    at<int>(0x00542fd8 + slot * 4) = x;
    at<int>(0x00542ff8 + slot * 4) = y;
    at<int>(0x00542dd8 + slot * 4) = pts;
    at<int>(0x00542f00 + slot * 4) = 0x18;
    at<int>(0x0059abb0) = (slot + 1) % 8;
}

// 0x0040c6f0  logTick(a, b): stores (short)(a | b) at 0x00568600 + ((tick / 1024) % 500) * 2, tick = the dword at
// 0x00834170 (signed division and remainder, 0x0040c6ff..0x0040c712).
V2_t LogTick_orig;
void __cdecl LogTick_re(int a, int b) {
    const int tick = at<int>(0x00834170);
    at<short>(0x00568600 + (tick / 1024 % 500) * 2) = static_cast<short>(a | b);
}

// 0x0040c860  clearMatching(a, b): for the 8 slots of the message ring, zeroes 0x0056a924[i] when 0x0056c770[i] == a
// and 0x0056c794[i] == b.
V2_t ClearMatching_orig;
void __cdecl ClearMatching_re(int a, int b) {
    for (int i = 0; i < 8; i++)
        if (at<int>(0x0056c770 + i * 4) == a && at<int>(0x0056c794 + i * 4) == b) at<int>(0x0056a924 + i * 4) = 0;
}

// 0x00426670  resetGolfer(g): zeroes the bytes at 0x005794da, 0x005794d5, 0x005794d9 + g*0x100; then for the 100
// 8-byte entries at 0x005689e8 whose word +4 equals the golfer's type word (0x0057956e + g*0x100), writes 0xffff to
// the entry's word +0.
V1_t ResetGolfer_orig;
void __cdecl ResetGolfer_re(int g) {
    const unsigned rec = g * 0x100;
    at<char>(0x005794da + rec) = 0;
    at<char>(0x005794d5 + rec) = 0;
    at<char>(0x005794d9 + rec) = 0;
    const short type = at<short>(0x0057956e + rec);
    for (unsigned q = 0x005689e8; q < 0x00568d08; q += 8)
        if (at<short>(q + 4) == type) at<unsigned short>(q) = 0xffff;
}

// 0x00469a20  appendOpinion(rating, twist): strcat of one string onto the text buffer 0x0051a068: rating 0..5 ->
// 0x004e2a74, 0x004e2a54, 0x004e2a34, 0x004e2a14, 0x004e29f0, 0x004e29d4 (jump table 0x00469ad4); 6 -> 0x004e29b4
// when twist <= 1 else 0x004e2990; 7 -> 0x004e2974 when twist < 1 else 0x004e2958; other ratings in 0..0x7f ->
// 0x004e2930 when twist < 1 else 0x004e2908; outside 0..0x7f -> 0x004e28e4.
V2_t AppendOpinion_orig;
void __cdecl AppendOpinion_re(int rating, int twist) {
    static const unsigned kFixed[6] = {0x004e2a74, 0x004e2a54, 0x004e2a34, 0x004e2a14, 0x004e29f0, 0x004e29d4};
    unsigned s;
    if (static_cast<unsigned>(rating) <= 5) s = kFixed[rating];
    else if (rating == 6) s = twist <= 1 ? 0x004e29b4 : 0x004e2990;
    else if (rating == 7) s = twist < 1 ? 0x004e2974 : 0x004e2958;
    else if (rating < 0 || rating > 0x7f) s = 0x004e28e4;
    else s = twist < 1 ? 0x004e2930 : 0x004e2908;
    strcat(kText, reinterpret_cast<const char*>(s));
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0040c720, queueMessage, QueueMessage_re, QueueMessage_orig);
SG_HOOK("golf_clean.exe", 0x0040c890, pointsPopup, PointsPopup_re, PointsPopup_orig);
SG_HOOK("golf_clean.exe", 0x0040c6f0, logTick, LogTick_re, LogTick_orig);
SG_HOOK("golf_clean.exe", 0x0040c860, clearMatching, ClearMatching_re, ClearMatching_orig);
SG_HOOK("golf_clean.exe", 0x00426670, resetGolfer, ResetGolfer_re, ResetGolfer_orig);
SG_HOOK("golf_clean.exe", 0x00469a20, appendOpinion, AppendOpinion_re, AppendOpinion_orig);
