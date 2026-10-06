// C3 batch c3f (2026-10-06) of golf_clean.exe: util math/string/RNG leaves and Snd484 field accessors.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the
// matched sources in re/match/*.cpp; each body cites the address of every global, field offset and callee it
// relies on. Callees are invoked through their original addresses (a hooked callee runs its own reimplementation,
// consistently for both arms of the path-1 A/B).
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused), as in golf_state.cpp.
#include <string.h>

#include "hooks.h"

namespace {

typedef int(__cdecl* Int2_t)(int, int);

// Originals of the two trig helpers, called at their VAs.
const Int2_t kFold467270 = reinterpret_cast<Int2_t>(0x00467270);  // b-scaled fold used by absDiff
const Int2_t kTrig491c70 = reinterpret_cast<Int2_t>(0x00491C70);  // b-scaled trig used by cosScaled

// ----------------------------------------------------------------------------------------------- pure leaves

// 0x00467130  clamp(v, lo, hi): raises v to lo (jge at 0x0046713a), then lowers it to hi only when v > hi AND
// hi >= lo (the `cmp ecx,edx; jl` at 0x00467146/0x00467148 skips the upper clamp when hi < lo).
typedef int(__cdecl* Clamp_t)(int, int, int);
Clamp_t Clamp_orig;
int __cdecl Clamp_re(int v, int lo, int hi) {
    if (v < lo) v = lo;
    if (v > hi && hi >= lo) v = hi;
    return v;
}

// 0x00467150  sign(v): 1 when v > 0 (0x00467158); otherwise `setge cl` on v (1 when v == 0, else 0) then `dec ecx`
// (0x00467162/0x00467165), giving 0 for v == 0 and -1 for v < 0.
typedef int(__cdecl* Int1_t)(int);
Int1_t Sign_orig;
int __cdecl Sign_re(int v) {
    if (v > 0) return 1;
    return (v >= 0) - 1;
}

// 0x004672b0  absDiff: returns fold467270(a + 0x40000000, b) (the `add ecx,0x40000000` at 0x004672b8, then the
// forwarding call at 0x004672c0; __cdecl, caller cleans). fold467270 (0x00467270) runs its original.
Int2_t AbsDiff_orig;
int __cdecl AbsDiff_re(int a, int b) {
    return kFold467270(a + 0x40000000, b);
}

// 0x00491d80  cosScaled: returns the value of 0x00491c70 applied to (a + 0x3fffffff, b) (the `add ecx,0x3fffffff`
// at 0x00491d88, forwarding call at 0x00491d90; __cdecl). 0x00491c70 runs its original.
Int2_t CosScaled_orig;
int __cdecl CosScaled_re(int a, int b) {
    return kTrig491c70(a + 0x3fffffff, b);
}

// ----------------------------------------------------------------------------------------------- RNG (thiscall)

// 0x0045c1e0  Random::range(n): advances the generator via Random::next (0x0045c1a0, ecx = this) and multiplies its
// [0,1) result by n masked to 16 bits (`and eax,0xffff` at 0x0045c1e9); the product is truncated to int by __ftol
// (0x004a6030, 0x0045c1f8) and returned in eax (callers keep only ax). __thiscall, one stack arg, so `ret 4`.
typedef double(__fastcall* RandomNext_t)(void* self, void* edx);
const RandomNext_t kRandomNext = reinterpret_cast<RandomNext_t>(0x0045C1A0);
typedef int(__fastcall* RandomRange_t)(void* self, void* edx, int n);
RandomRange_t RandomRange_orig;
int __fastcall RandomRange_re(void* self, void*, int n) {
    double d = kRandomNext(self, 0);         // advances the seed at [this]; runs its own reimpl during the A/B
    n &= 0xffff;
    return static_cast<int>(static_cast<long long>(d * n));  // __ftol: 64-bit truncation toward zero, low dword
}

// ----------------------------------------------------------------------------- Snd484 accessors (thiscall)
// Each stores/validates a field of the Snd484 object and, when the device pointer at +0x40 is non-null, notifies it
// through one virtual slot. The device slot is the object's [+0x40]; the virtual is [[+0x40] + slot]. These helpers
// are invoked as thiscall (ecx = device, one stack arg) only when a device is attached.

typedef void(__fastcall* DevNotify_t)(void* dev, void* edx, int arg);
typedef unsigned(__fastcall* DevFlags_t)(void* dev, void* edx);

inline void* sndDevice(void* self) { return *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x40); }
inline void sndNotify(void* dev, unsigned vtblOff, int arg) {
    void* vtbl = *reinterpret_cast<void**>(dev);
    DevNotify_t fn = *reinterpret_cast<DevNotify_t*>(reinterpret_cast<char*>(vtbl) + vtblOff);
    fn(dev, 0, arg);
}

// 0x00484f40  Snd::setPitch(p): clamps p to [-0x4b0, 0x4b0] (0x00484f44..0x00484f59), stores it at [this+0x5c]
// (0x00484f5e), and when the device [this+0x40] is set calls its virtual +0x9c with the clamped value
// (0x00484f6b); returns 0 (`xor eax,eax` at 0x00484f71). __thiscall, one stack arg (`ret 4`).
typedef int(__fastcall* Snd1_t)(void* self, void* edx, int arg);
Snd1_t SetPitch_orig;
int __fastcall SetPitch_re(void* self, void*, int p) {
    if (p < -0x4b0) p = -0x4b0;
    else if (p > 0x4b0) p = 0x4b0;
    *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x5c) = p;
    void* dev = sndDevice(self);
    if (dev) sndNotify(dev, 0x9c, p);
    return 0;
}

// 0x004847f0  Snd::setPan(p): clamps p to [-0x40, 0x3f] (0x004847f4..0x00484805), stores it at [this+8]
// (0x0048480a), notifies the device virtual +0x44 when [this+0x40] is set (0x00484817), and returns the clamped
// value in eax (left from the clamp; `ret 4`).
Snd1_t SetPan_orig;
int __fastcall SetPan_re(void* self, void*, int p) {
    if (p < -0x40) p = -0x40;
    else if (p > 0x3f) p = 0x3f;
    *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 8) = p;
    void* dev = sndDevice(self);
    if (dev) sndNotify(dev, 0x44, p);
    return p;
}

// 0x004846d0  Snd::setField38(v): returns 10 when v == 0 (0x004846d8); otherwise stores v at [this+0x38]
// (0x004846e0), notifies the device virtual +4 when [this+0x40] is set (0x004846ed), and returns 0 (`xor eax,eax`
// at 0x004846f0). __thiscall, one stack arg (`ret 4`).
Snd1_t SetField38_orig;
int __fastcall SetField38_re(void* self, void*, int v) {
    if (v == 0) return 10;
    *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x38) = v;
    void* dev = sndDevice(self);
    if (dev) sndNotify(dev, 4, v);
    return 0;
}

// 0x00484260  Snd::setMode(m): ORs a bit into the flags word [this+0x44] by mode (jump table 0x004842d0) and stores
// the mode at [this+0x54]; mode 4 -> 0x10, 2 -> 8, 1 -> 4, 5 -> 0x28, 6 -> 0x100, 7 -> 0x80 (then falls into the
// default store), mode 3 / out-of-range only store the mode (default target 0x004842c7). __thiscall, one arg (`ret 4`).
typedef void(__fastcall* SetMode_t)(void* self, void* edx, int m);
SetMode_t SetMode_orig;
void __fastcall SetMode_re(void* self, void*, int m) {
    unsigned* f44 = reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + 0x44);
    switch (m) {
        case 4: *f44 |= 0x10; break;
        case 2: *f44 |= 8; break;
        case 1: *f44 |= 4; break;
        case 5: *f44 |= 0x28; break;
        case 6: *f44 |= 0x100; break;
        case 7: *f44 |= 0x80; break;
        default: break;
    }
    *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x54) = m;
}

// 0x00484ff0  Snd::flags(): composes a status word. Starts from the device virtual +0x70 when [this+0x40] is set
// (0x00484ffe), else 0; ORs 2 when [this+0x30] is non-null (0x00485008). Then from the low byte at [this+0x58]:
// bit 0 -> 1, bit 3 -> 0x40, bit 1 -> 4, bit 2 -> 0x10, bit 4 -> 0x80, bit 5 -> 0x100 (`or ah,1` at 0x00485036).
// Returns the word in eax. __thiscall, no stack args (`ret`).
typedef int(__fastcall* SndFlags_t)(void* self, void* edx);
SndFlags_t Flags_orig;
int __fastcall Flags_re(void* self, void*) {
    unsigned r = 0;
    void* dev = sndDevice(self);
    if (dev) {
        void* vtbl = *reinterpret_cast<void**>(dev);
        DevFlags_t fn = *reinterpret_cast<DevFlags_t*>(reinterpret_cast<char*>(vtbl) + 0x70);
        r = fn(dev, 0);
    }
    if (*reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x30)) r |= 2;
    unsigned char c = *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(self) + 0x58);
    if (c & 1) r |= 1;
    if (c & 8) r |= 0x40;
    if (c & 2) r |= 4;
    if (c & 4) r |= 0x10;
    if (c & 0x10) r |= 0x80;
    if (c & 0x20) r |= 0x100;
    return static_cast<int>(r);
}

// ----------------------------------------------------------------------------- MappedFile::ctor (thiscall)

// 0x00492d80  MappedFile::ctor(): installs vtable 0x004bba78 at [this] (0x00492d84), then m_4 = 0 (0x00492d8a),
// m_8 = -1 (0x00492d8d) and m_c = 0 (0x00492d94); returns this in eax (`mov eax,ecx` at 0x00492d80). __thiscall,
// no stack args (`ret`).
typedef void*(__fastcall* Ctor_t)(void* self, void* edx);
Ctor_t MappedFileCtor_orig;
void* __fastcall MappedFileCtor_re(void* self, void*) {
    int* p = reinterpret_cast<int*>(self);
    p[0] = 0x004bba78;
    p[1] = 0;
    p[2] = -1;
    p[3] = 0;
    return self;
}

// ------------------------------------------------------------------------------------- buffer writers (cdecl)

// 0x0045b880  clearBuffers(): zeroes the 0x1002-byte buffer at 0x0056fcb0 (0x400 dwords + 1 word, 0x0045b88d/
// 0x0045b88f) and fills the 0x100-byte table at 0x0059d81c with 0xff (0x40 dwords of 0xffffffff, 0x0045b89e).
typedef void(__cdecl* V0_t)();
V0_t ClearBuffers_orig;
void __cdecl ClearBuffers_re() {
    memset(reinterpret_cast<void*>(0x0056fcb0), 0, 0x1002);
    memset(reinterpret_cast<void*>(0x0059d81c), 0xff, 0x100);
}

// 0x0045b8b0  appendToBuffer45b8b0(id): manages the text store g_56fcb0[0x1002] (0x0056fcb0) with a 128-entry offset
// table g_59d81c (0x0059d81c, shorts; -1 means empty) and length table g_5a46b8 (0x005a46b8, shorts). When id != -1
// and its offset is live it removes that entry: the trailing bytes are packed down over it (memcpy of
// 0x1002 - len - off bytes), the offset is cleared and every later offset is shifted left by len. It then finds the
// highest end (max of offset+length over live entries) and, for a new entry (id == -1), the first empty slot with
// index > 0x20; an explicit id reuses that slot. If the current text g_51a068 (0x0051a068) plus its NUL fits below
// the end, it is copied in, the slot's offset/length are recorded and the slot index is returned; otherwise -1.
typedef int(__cdecl* Int1v_t)(int);
Int1v_t AppendToBuffer_orig;
int __cdecl AppendToBuffer_re(int id) {
    char* buf = reinterpret_cast<char*>(0x0056fcb0);
    short* off = reinterpret_cast<short*>(0x0059d81c);
    short* len = reinterpret_cast<short*>(0x005a46b8);
    const char* text = reinterpret_cast<const char*>(0x0051a068);

    if (id != -1 && off[id] != -1) {
        int o = off[id];
        int n = static_cast<int>(strlen(buf + o)) + 1;
        memcpy(buf + o, buf + o + n, 0x1002 - n - o);
        off[id] = -1;
        for (int i = 0; i < 128; i++)
            if (off[i] > o) off[i] = static_cast<short>(off[i] - n);
    }

    int end = 0, slot = -1;
    for (int i = 0; i < 128; i++) {
        if (off[i] != -1) {
            if (len[i] + off[i] > end) end = len[i] + off[i];
        }
        if (off[i] == -1 && slot == -1 && i > 0x20) slot = i;
    }
    if (id != -1) slot = id;

    int n = static_cast<int>(strlen(text)) + 1;
    if (n + end < 0x1002) {
        strcpy(buf + end, text);
        off[slot] = static_cast<short>(end);
        len[slot] = static_cast<short>(n);
        return slot;
    }
    return -1;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00467130, clamp, Clamp_re, Clamp_orig);
SG_HOOK("golf_clean.exe", 0x00467150, sign, Sign_re, Sign_orig);
SG_HOOK("golf_clean.exe", 0x004672b0, absDiff, AbsDiff_re, AbsDiff_orig);
SG_HOOK("golf_clean.exe", 0x00491d80, cosScaled, CosScaled_re, CosScaled_orig);
SG_HOOK("golf_clean.exe", 0x0045c1e0, Random_range, RandomRange_re, RandomRange_orig);
SG_HOOK("golf_clean.exe", 0x00484f40, Snd_setPitch, SetPitch_re, SetPitch_orig);
SG_HOOK("golf_clean.exe", 0x004847f0, Snd_setPan, SetPan_re, SetPan_orig);
SG_HOOK("golf_clean.exe", 0x004846d0, Snd_setField38, SetField38_re, SetField38_orig);
SG_HOOK("golf_clean.exe", 0x00484260, Snd_setMode, SetMode_re, SetMode_orig);
SG_HOOK("golf_clean.exe", 0x00484ff0, Snd_flags, Flags_re, Flags_orig);
SG_HOOK("golf_clean.exe", 0x00492d80, MappedFile_ctor, MappedFileCtor_re, MappedFileCtor_orig);
SG_HOOK("golf_clean.exe", 0x0045b880, clearBuffers, ClearBuffers_re, ClearBuffers_orig);
SG_HOOK("golf_clean.exe", 0x0045b8b0, appendToBuffer45b8b0, AppendToBuffer_re, AppendToBuffer_orig);
