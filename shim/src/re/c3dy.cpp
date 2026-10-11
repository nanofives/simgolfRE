// C3 batch c3dy (round 28 follow-up, simulation plan phase 0, 2026-10-11) of golf_clean.exe: the shared leaves the large
// simulation functions call: waitTicks, pumpAndRedraw, Snd::play, playSound, playSoundAt, inRange, rebuildTileCoverage,
// scanShotArea, scanNearbyGolfers, storyText. Fixtures: re/frida/js/fixtures.d/c3dy.js. Registry: re/frida/registry.d/c3dy.py.
// Purposes and per-jcc tables: log/c3/c3dy_purpose.md.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites
// the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their
// original addresses; an import is called through the IAT slot the original reads, never the shim's own import table.
#include "hooks.h"

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
template <typename T> static inline T Imp(unsigned slot) { return *reinterpret_cast<T*>(slot); }
static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
template <typename T> static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(B(p) + off); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int a);
typedef int(__cdecl* C2_t)(int a, int b);
typedef int(__cdecl* C3_t)(int a, int b, int c);
typedef int(__cdecl* C4_t)(int a, int b, int c, int d);
typedef int(__cdecl* C5_t)(int a, int b, int c, int d, int e);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);
typedef unsigned(__stdcall* TimeGetTime_t)();

// ---- the pump family's callees (original addresses)
const C0_t kPumpMessages = reinterpret_cast<C0_t>(0x0045c030);     // pumpMessages
const C0_t kFlushRedraw = reinterpret_cast<C0_t>(0x0045ae70);      // flushRedraw
const C0_t kSetDirtyFlagA = reinterpret_cast<C0_t>(0x0045ae30);    // setDirtyFlagA
const C0_t kSetDirtyFlagB = reinterpret_cast<C0_t>(0x0045ae50);    // setDirtyFlagB
const C0_t kEnterModal = reinterpret_cast<C0_t>(0x00483d30);       // enterModal483d30
const C0_t kPumpInput483c30 = reinterpret_cast<C0_t>(0x00483c30);  // pumpInput483c30
const C0_t kDrainAndRedraw = reinterpret_cast<C0_t>(0x0045af00);   // drainAndRedraw
const C0_t kWaitRedrawReady = reinterpret_cast<C0_t>(0x0045aed0);  // waitRedrawReady
// IAT slot of WINMM!timeGetTime (0x004ba21c; the callers load it with `mov ebx/edi, [0x4ba21c]` or `call [0x4ba21c]`)
const unsigned kIatTimeGetTime = 0x004ba21c;

}  // namespace

// ====================================================================================================================
// Pumps with a deadline
// ====================================================================================================================

// 0x0045bf80  waitTicks(ticks, pump): cdecl, `ret`. ticks == 0 returns 0 (test 0x0045bf87, je 0x0045bf8a -> xor eax, eax
// 0x0045bfec). Otherwise limit = (ticks * 1000) / 100 as the 32-bit signed product and the magic-number division by 100
// (lea x5 three times 0x0045bf8c..0x0045bf98, shl 3 at 0x0045bfa0, imul by 0x51eb851f at 0x0045bfa3, sar 5 at 0x0045bfa5, plus the
// sign bit at 0x0045bfaa..0x0045bfad), start = timeGetTime() (call edi 0x0045bfb1), elapsed = timeGetTime() - start (0x0045bfb5
// / 0x0045bfb7); an elapsed time that is not below limit (unsigned `jae` 0x0045bfbb) returns 0. Otherwise the loop: pumpInput483c30
// (0x0045bfc1); with pump != 0 (0x0045bfc6/0x0045bfc8) pumpMessages (0x0045bfca), then drainAndRedraw (0x0045bfcf): a zero result
// (0x0045bfd4/0x0045bfd6) or a non-zero [0x00822d68] (0x0045bfd8..0x0045bfdf) leaves through waitRedrawReady (0x0045bff0) and
// returns 1 (0x0045bff8); the loop ends with 0 once timeGetTime() - start is not below limit (unsigned `jb` 0x0045bfe7).
int __cdecl WaitTicks_re(int ticks, int pump) {
    if (ticks == 0) return 0;                                               // 0x0045bf87 / 0x0045bf8a
    // ticks * 125 * 8 as the three lea chains and shl (wraps like the 32-bit register), then the signed /100
    int prod = static_cast<int>(static_cast<unsigned>(ticks) * 1000u);      // 0x0045bf8c..0x0045bfa0
    long long wide = static_cast<long long>(prod) * 0x51eb851fLL;           // 0x0045bf9b / 0x0045bfa3
    int hi = static_cast<int>(wide >> 32);                                  // edx
    hi >>= 5;                                                               // 0x0045bfa5
    unsigned limit = static_cast<unsigned>(hi + static_cast<int>(static_cast<unsigned>(hi) >> 31));  // 0x0045bfaa / 0x0045bfad
    TimeGetTime_t tg = Imp<TimeGetTime_t>(kIatTimeGetTime);                 // 0x0045bf8f
    unsigned start = tg();                                                  // 0x0045bfb1
    if (tg() - start >= limit) return 0;                                    // 0x0045bfb5..0x0045bfbb
    do {
        kPumpInput483c30();                                                 // 0x0045bfc1
        if (pump != 0) {                                                    // 0x0045bfc6 / 0x0045bfc8
            kPumpMessages();                                                // 0x0045bfca
            if (kDrainAndRedraw() == 0 || G<int>(0x00822d68) != 0) {        // 0x0045bfcf..0x0045bfdf
                kWaitRedrawReady();                                         // 0x0045bff0
                return 1;                                                   // 0x0045bff8
            }
        }
    } while (tg() - start < limit);                                         // 0x0045bfe1..0x0045bfe7
    return 0;                                                               // 0x0045bfec
}
C2_t WaitTicks_orig;
// 0x0045bf80
SG_HOOK("golf_clean.exe", 0x0045bf80, waitTicks, WaitTicks_re, WaitTicks_orig);

// 0x0045c0c0  pumpAndRedraw(limit): cdecl, `ret`, always returns 1 (mov eax, 1 at 0x0045c148). start = timeGetTime()
// (call ebx 0x0045c0ca); [0x00822d94] = [0x00822d68] = 0 (0x0045c0d4 / 0x0045c0d9). Loop: pumpMessages (0x0045c0de), now =
// timeGetTime() (0x0045c0e3); a non-zero [0x00822d68] (0x0045c0e7..0x0045c0ee), a non-zero flushRedraw (0x0045c0f0..0x0045c0f7)
// or a non-zero [0x0083afcc] (0x0045c0f9..0x0045c100) leaves the loop; limit == 0 repeats (0x0045c104); now - start (signed)
// below limit repeats (0x0045c106..0x0045c10a). After the loop [0x00822d94] = [0x00822b9c] and [0x00822d7c] = [0x00822b98]
// (reads 0x0045c10c / 0x0045c111, stores 0x0045c117 / 0x0045c11c); flushRedraw again (0x0045c122); a non-zero result sets
// [0x00822d94] = setDirtyFlagB() and [0x00822d7c] = setDirtyFlagA() (0x0045c12f..0x0045c13e); enterModal483d30 (0x0045c143).
int __cdecl PumpAndRedraw_re(int limit) {
    TimeGetTime_t tg = Imp<TimeGetTime_t>(kIatTimeGetTime);                 // 0x0045c0c1
    unsigned start = tg();                                                  // 0x0045c0ca
    G<int>(0x00822d94) = 0;                                                 // 0x0045c0d4
    G<int>(0x00822d68) = 0;                                                 // 0x0045c0d9
    for (;;) {
        kPumpMessages();                                                    // 0x0045c0de
        unsigned now = tg();                                                // 0x0045c0e3
        if (G<int>(0x00822d68) != 0) break;                                 // 0x0045c0e7..0x0045c0ee
        if (kFlushRedraw() != 0) break;                                     // 0x0045c0f0..0x0045c0f7
        if (G<int>(0x0083afcc) != 0) break;                                 // 0x0045c0f9..0x0045c100
        if (limit == 0) continue;                                           // 0x0045c102 / 0x0045c104
        if (static_cast<int>(now - start) < limit) continue;                // 0x0045c106..0x0045c10a
        break;
    }
    int a = G<int>(0x00822b9c);                                             // 0x0045c10c
    int b = G<int>(0x00822b98);                                             // 0x0045c111
    G<int>(0x00822d94) = a;                                                 // 0x0045c117
    G<int>(0x00822d7c) = b;                                                 // 0x0045c11c
    if (kFlushRedraw() != 0) {                                              // 0x0045c122 / 0x0045c12b / 0x0045c12d
        G<int>(0x00822d94) = kSetDirtyFlagB();                              // 0x0045c12f / 0x0045c134
        G<int>(0x00822d7c) = kSetDirtyFlagA();                              // 0x0045c139 / 0x0045c13e
    }
    kEnterModal();                                                          // 0x0045c143
    return 1;                                                               // 0x0045c148
}
C1_t PumpAndRedraw_orig;
// 0x0045c0c0
SG_HOOK("golf_clean.exe", 0x0045c0c0, pumpAndRedraw, PumpAndRedraw_re, PumpAndRedraw_orig);

// ====================================================================================================================
// Sound: Snd::play, playSound, playSoundAt
// ====================================================================================================================

namespace {
const T1_t kBucketEmpty = reinterpret_cast<T1_t>(0x00487770);   // HashTable::bucketEmpty(this = g_musicDevice, i), result in al
const T1_t kBucketSet = reinterpret_cast<T1_t>(0x004877a0);     // HashTable::bucketSet(this = g_musicDevice, i), result in al
const T0_t kSndReopen = reinterpret_cast<T0_t>(0x00484d60);     // Snd::reopen
const T0_t kSndPlay = reinterpret_cast<T0_t>(0x00484940);       // Snd::play
const T1_t kSndSetPitch = reinterpret_cast<T1_t>(0x00484f40);   // Snd::setPitch
const T1_t kSndSetVolume = reinterpret_cast<T1_t>(0x00485140);  // Snd::setVolume
const T1_t kSndSetPan = reinterpret_cast<T1_t>(0x004847f0);     // Snd::setPan
const T1_t kSndSetField34 = reinterpret_cast<T1_t>(0x004846b0); // Snd::setField34
const C5_t kPlaySound = reinterpret_cast<C5_t>(0x004481b0);     // playSound
typedef int(__cdecl* WorldToScreen_t)(int wx, int wy, int* sx, int* sy, int margin);
const WorldToScreen_t kWorldToScreen = reinterpret_cast<WorldToScreen_t>(0x0042fb90);
const C3_t kClamp = reinterpret_cast<C3_t>(0x00467130);         // clamp(v, lo, hi)
const T1_t kRandomRange = reinterpret_cast<T1_t>(0x0045c1e0);   // Random::range(this = 0x00822d9c, n)
// the virtual call every Snd/device slot below makes: [[obj] + slot](obj, args)
template <typename F> static inline F VSlot(void* obj, unsigned slot) { return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + slot); }
}  // namespace

// 0x00484940  Snd::play(this): thiscall, plain `ret`. result (edi) = 0 (xor 0x00484944). key = [this+0x3c] (0x00484946); a key
// below 0x10 (unsigned `jae` 0x0048494c) asks the music device 0x0083ad80: a true HashTable::bucketEmpty(key) (0x00484954 /
// 0x00484959 / 0x0048495b) or a true bucketSet(key) (0x00484966 / 0x0048496b / 0x0048496d) returns 0x14 (0x00484991). Then, with
// bit 0x10 of the byte [this+0x58] set (0x0048496f / 0x00484973), the object's slot +0x40 is called with [this+4]
// (0x00484975..0x0048497d). The sound object [this+0x40]: non-NULL (0x00484983 / 0x00484985) result = its slot +0x1c
// (0x00484989 / 0x0048498c). NULL: bit 0x10 clear gives 0x14 (0x0048499c / 0x004849b5); set calls Snd::reopen (0x004849a0),
// reloads [this+0x40] and, when non-NULL, takes its slot +0x1c result (0x004849a5..0x004849b1), else result stays 0. With bit
// 0x10 set (0x004849ba / 0x004849be): [this+0x68] = timeGetTime() (call [0x4ba21c] 0x004849c0 / store 0x004849ca), the
// object's slot +0x80 is called (0x004849cd) and [this+0x40] = 0 (0x004849d3). eax = result (0x004849da).
int __fastcall SndPlay_re(void* self, void*) {
    int result = 0;                                                         // 0x00484944
    unsigned key = At<unsigned>(self, 0x3c);                                // 0x00484946
    if (key < 0x10) {                                                       // 0x00484949 / 0x0048494c
        if ((kBucketEmpty(reinterpret_cast<void*>(0x0083ad80), 0, key) & 0xff) != 0) return 0x14;   // 0x00484954..0x0048495b
        if ((kBucketSet(reinterpret_cast<void*>(0x0083ad80), 0, At<unsigned>(self, 0x3c)) & 0xff) != 0) return 0x14;   // 0x00484960..0x0048496d
        if (At<unsigned char>(self, 0x58) & 0x10) {                         // 0x0048496f / 0x00484973
            VSlot<T1_t>(self, 0x40)(self, 0, At<int>(self, 4));             // 0x00484975..0x0048497d
        }
    }
    void* child = At<void*>(self, 0x40);                                    // 0x00484980
    if (child != 0) {                                                       // 0x00484983 / 0x00484985
        result = VSlot<T0_t>(child, 0x1c)(child, 0);                        // 0x00484989 / 0x0048498c
    } else if (At<unsigned char>(self, 0x58) & 0x10) {                      // 0x00484998 / 0x0048499c
        kSndReopen(self, 0);                                                // 0x004849a0
        child = At<void*>(self, 0x40);                                      // 0x004849a5
        if (child != 0) result = VSlot<T0_t>(child, 0x1c)(child, 0);        // 0x004849a8..0x004849b1
    } else {
        result = 0x14;                                                      // 0x004849b5
    }
    if (At<unsigned char>(self, 0x58) & 0x10) {                             // 0x004849ba / 0x004849be
        At<unsigned>(self, 0x68) = Imp<TimeGetTime_t>(kIatTimeGetTime)();   // 0x004849c0 / 0x004849ca
        VSlot<T0_t>(self, 0x80)(self, 0);                                   // 0x004849cd
        At<int>(self, 0x40) = 0;                                            // 0x004849d3
    }
    return result;                                                          // 0x004849da
}
T0_t SndPlay_orig;
// 0x00484940
SG_HOOK("golf_clean.exe", 0x00484940, Snd_play, SndPlay_re, SndPlay_orig);

// 0x004481b0  playSound(idx, volume, pan, pitch, field34): cdecl, plain `ret`. idx == -1 (cmp 0x004481b4, je 0x004481b7) returns
// eax = idx = -1 (0x004481ff). Otherwise the Snd object is 0x0080d840 + idx * 108 (lea x3, x9, x4 + 0x80d840 at 0x004481b9..0x004481c0,
// 32-bit wrap) and, in this order: Snd::setPitch(pitch) (0x004481ce), Snd::setVolume(volume) (0x004481da), Snd::setPan(pan)
// (0x004481e6), Snd::setField34(field34) (0x004481f2), then Snd::play (0x004481f9) whose eax is the return value.
int __cdecl PlaySound_re(int idx, int volume, int pan, int pitch, int field34) {
    if (idx == -1) return -1;                                               // 0x004481b4 / 0x004481b7
    void* snd = reinterpret_cast<void*>(0x0080d840u + static_cast<unsigned>(idx) * 108u);   // 0x004481b9..0x004481c0
    kSndSetPitch(snd, 0, pitch);                                            // 0x004481ce
    kSndSetVolume(snd, 0, volume);                                          // 0x004481da
    kSndSetPan(snd, 0, pan);                                                // 0x004481e6
    kSndSetField34(snd, 0, field34);                                        // 0x004481f2
    return kSndPlay(snd, 0);                                                // 0x004481f9
}
C5_t PlaySound_orig;
// 0x004481b0
SG_HOOK("golf_clean.exe", 0x004481b0, playSound, PlaySound_re, PlaySound_orig);

// 0x0040c500  playSoundAt(sound, wx, wy, field34): cdecl, plain `ret`. sx / sy are the two words worldToScreen(wx, wy, &sx, &sy, 0)
// (0x0040c519) writes over the caller's argument slots (sx over wy, sy over wx, from the lea at 0x0040c507 / 0x0040c50d), so they
// start as wy and wx. A zero result with field34 != -1 returns 0 (0x0040c525..0x0040c52a); a zero result with field34 == -1 or a
// non-zero result with field34 == -1 (0x0040c532..0x0040c539) takes the clamp arm: sx = clamp(sx, 0, 0x31f) (0x0040c547),
// sy = clamp(sy, 0, 0x257) (0x0040c55c), f34 = 0 (xor ebx 0x0040c56e), and skips the screen test. Otherwise f34 = field34 and
// [0x005a9cbc] != 0 (0x0040c572..0x0040c579) doubles both: sx = 2 sx - 0x190, sy = 2 sy - 0x190 (0x0040c583 / 0x0040c58a); then
// sy outside 0..0x1f4 or sx outside 0..0x31f returns (0x0040c5a3..0x0040c5bf) with eax = the sx of 0x0040c57b when [0x005a9cbc]
// != 0, else 0. Then [0x005a9cd8] == 0 (0x0040c5cb / 0x0040c5d2): pitch = 0x12c - (Random::range(0x00822d9c, 0x258) & 0xffff)
// (0x0040c5d8..0x0040c5fb); [0x005a9cd8] != 0 (0x0040c65b): pitch = (byte[0x004c2fa4 + [0x005a9cdc] % 0x24] - 5) * 100
// (0x0040c66a..0x0040c683) and [0x005a9cdc] += 1 (0x0040c688). pan = sx * 127 / 800 - 0x40 (the 0x51eb851f magic division, 0x0040c5f8 /
// 0x0040c605..0x0040c60f, 0x0040c690..0x0040c6a9). volume = base + |sx - 400| / 16 (random arm 0x0040c613..0x0040c63d) or base + sy / 12
// (counter arm, 0x2aaaaaab division by 6 at 0x0040c6a1..0x0040c6b6, halved 0x0040c6bc..0x0040c6cc), base 50 when [0x004c2844] >= 4 else 10
// (setge / dec / and 0xffffffd8 / add 0x32 at 0x0040c630..0x0040c63a and 0x0040c6af..0x0040c6be). playSound(sound, volume, pan,
// pitch, f34) (0x0040c645 / 0x0040c6d0), [0x005a9cd8] = 0 (0x0040c64d / 0x0040c6d8); eax = playSound's.
int __cdecl PlaySoundAt_re(int sound, int wx, int wy, int field34) {
    int sx = wy;                                                            // 0x0040c50d (the slot holds arg 3)
    int sy = wx;                                                            // 0x0040c507 (the slot holds arg 2)
    int r = kWorldToScreen(wx, wy, &sx, &sy, 0);                            // 0x0040c519
    int f34 = 0;
    bool clampArm = false;
    if (r == 0) {                                                           // 0x0040c521 / 0x0040c523
        if (field34 != -1) return 0;                                        // 0x0040c525 / 0x0040c52a
        clampArm = true;                                                    // 0x0040c530
    } else {
        f34 = field34;                                                      // 0x0040c532
        if (f34 == -1) clampArm = true;                                     // 0x0040c536 / 0x0040c539
    }
    if (clampArm) {
        sx = kClamp(sx, 0, 0x31f);                                          // 0x0040c547
        sy = kClamp(sy, 0, 0x257);                                          // 0x0040c55c
        f34 = 0;                                                            // 0x0040c56e
    } else {
        int flag = G<int>(0x005a9cbc);                                      // 0x0040c572
        int eaxOut = flag;
        if (flag != 0) {                                                    // 0x0040c577 / 0x0040c579
            eaxOut = sx;                                                    // 0x0040c57b
            sx = sx + sx - 0x190;                                           // 0x0040c583
            sy = sy + sy - 0x190;                                           // 0x0040c58a
        }
        if (sy < 0 || sy > 0x1f4 || sx < 0 || sx >= 0x320) return eaxOut;  // 0x0040c5a3..0x0040c5bf
    }
    int volume, pan, pitch;
    const int zoom = G<int>(0x004c2844);
    int base = 10;
    if (zoom >= 4) base = 50;                                               // 0x0040c630..0x0040c63a / 0x0040c6af..0x0040c6be (setge / dec / and 0xffffffd8 / add 0x32)
    // pan = trunc((sx * 127) * 0x51eb851f >> 40) - 0x40 (the 32-bit product wraps like the register)
    int prod = static_cast<int>(static_cast<unsigned>(sx) * 127u);          // 0x0040c5f8 / 0x0040c5fd
    int hi = static_cast<int>((static_cast<long long>(prod) * 0x51eb851fLL) >> 32) >> 8;   // 0x0040c605 / 0x0040c607
    pan = hi + static_cast<int>(static_cast<unsigned>(hi) >> 31) - 0x40;    // 0x0040c60a..0x0040c60f
    if (G<int>(0x005a9cd8) == 0) {                                          // 0x0040c5cb / 0x0040c5d2
        int rnd = kRandomRange(reinterpret_cast<void*>(0x00822d9c), 0, 0x258) & 0xffff;   // 0x0040c5d8..0x0040c5ec
        pitch = 0x12c - rnd;                                                // 0x0040c5f3 / 0x0040c5fb
        int t = sx - 0x190;                                                 // 0x0040c613
        int s = t >> 31;                                                    // 0x0040c619
        int e = (t ^ s) - s;                                                // 0x0040c61a / 0x0040c61d
        e += (e >> 31) & 0xf;                                               // 0x0040c625 / 0x0040c626 / 0x0040c629
        e >>= 4;                                                            // 0x0040c62d
        volume = e + base;                                                  // 0x0040c63d
    } else {
        int c = G<int>(0x005a9cdc);                                         // 0x0040c65b
        int rem = c % 0x24;                                                 // 0x0040c669 / 0x0040c66a
        int b = *reinterpret_cast<signed char*>(0x004c2fa4 + rem);          // 0x0040c673
        pitch = (b - 5) * 100;                                              // 0x0040c67a..0x0040c683
        G<int>(0x005a9cdc) = c + 1;                                         // 0x0040c686 / 0x0040c688
        int h2 = static_cast<int>((static_cast<long long>(sy) * 0x2aaaaaabLL) >> 32);   // 0x0040c6b6
        int w = h2 >> 1;                                                    // 0x0040c6bc
        volume = w + base + static_cast<int>(static_cast<unsigned>(w) >> 31);   // 0x0040c6c1..0x0040c6cc
    }
    int rv = kPlaySound(sound, volume, pan, pitch, f34);                    // 0x0040c645 / 0x0040c6d0
    G<int>(0x005a9cd8) = 0;                                                 // 0x0040c64d / 0x0040c6d8
    return rv;
}
C4_t PlaySoundAt_orig;
// 0x0040c500
SG_HOOK("golf_clean.exe", 0x0040c500, playSoundAt, PlaySoundAt_re, PlaySoundAt_orig);

// ====================================================================================================================
// Simulation leaves: inRange, rebuildTileCoverage, scanShotArea, scanNearbyGolfers, storyText
// ====================================================================================================================

namespace {
const C2_t kDistance = reinterpret_cast<C2_t>(0x0040acd0);          // distance(dx, dy)
const C3_t kHoleQuadrants = reinterpret_cast<C3_t>(0x00407000);     // holeQuadrants(x, y, n)
const C4_t kProbe40c4b0 = reinterpret_cast<C4_t>(0x0040c4b0);       // f40c4b0(x, y, a, b)
const C2_t kTileType40bfa0 = reinterpret_cast<C2_t>(0x0040bfa0);    // f40bfa0(x, y)
const C4_t kNarrateShot = reinterpret_cast<C4_t>(0x00407e00);       // narrateShot(golfer, a, b, flag)
const C3_t kEmitGolferThought = reinterpret_cast<C3_t>(0x00467a00); // emitGolferThought(golfer, a, b)
const C2_t kFloodMark = reinterpret_cast<C2_t>(0x0042f120);         // floodMark(row, col)
const C4_t kComputeStoryReply = reinterpret_cast<C4_t>(0x004669f0); // computeStoryReply(a, b, c, d)
static inline int RandRange(int n) { return kRandomRange(reinterpret_cast<void*>(0x00822d9c), 0, n); }   // Random::range
}  // namespace

// 0x004070b0  inRange(type, dist, row, col): cdecl, plain `ret`, returns 1 when lo < dist < hi (signed; `jge` 0x00407261 / `jle`
// 0x00407265 give 0), else 0. Initial window (hi = eax, lo = ecx): type 0x16 or 0x15 (0x004070b5..0x004070bd) reads the low byte
// of the tile word at 0x0053caf0 + 2 * (col + 50 * row) (0x004070bf..0x004070d9), masks it with 0x1f and gives hi = 0xc8 when
// (value - 1) > 3 signed (dec / cmp 3 / jg 0x004070e7) else 0, lo = 0; any other type gives hi = lo = type (0x004070ed..0x004070f1).
// The signed byte [0x005a34e0] (0x004070f5) above 3 unsigned (ja 0x004070ff) keeps that window; 0..3 jump through the table
// 0x00407270 = {0x0040710c, 0x0040715a, 0x004071a6, 0x0040722b}. Arms (r = Random::range(0x00822d9c, n) & 0xffff, 0x00407121 etc.):
//   A 0x004071c9: hi = r(100) + 0x190, lo = 0x32      B 0x0040712e/0x00407255: hi = r(100) + 0x64, lo = 0x14
//   C 0x00407143: hi = r(100) + 0x64, lo = 0x32       D 0x00407192: hi = r(100) + 0x12c, lo = 0x64
//   E 0x00407206: type 0x10: hi = r(200) + 0x190, lo = 0x4b      F 0x0040723f: hi = r(100) + 0x190, lo = 0x14
// mode 0 (0x0040710c): 0xd A, 0xe B, 0xf C, 0x10 E. mode 1 (0x0040715a): 0xd A, 0xe B, 0xf D, 0x10 E. mode 2 (0x004071a6):
// 0xd B, 0xe A, 0xf C, 0x10 E. mode 3 (0x0040722b): 0xd..0x10 F. Types outside 0xd..0x10 keep the initial window.
int __cdecl InRange_re(int type, int dist, int row, int col) {
    int hi, lo;
    if (type == 0x16 || type == 0x15) {                                     // 0x004070b5 / 0x004070b8 / 0x004070ba / 0x004070bd
        int idx = col + row * 50;                                           // 0x004070ca..0x004070d5
        int v = G<unsigned char>(0x0053caf0 + static_cast<unsigned>(idx) * 2u) & 0x1f;   // 0x004070d9 / 0x004070e0
        hi = (v - 1 > 3) ? 0xc8 : 0;                                        // 0x004070e3..0x004070eb
        lo = 0;                                                             // 0x004070c8
    } else {
        hi = type;                                                          // 0x004070ed
        lo = type;                                                          // 0x004070f1
    }
    const int mode = G<signed char>(0x005a34e0);                            // 0x004070f5
    if (static_cast<unsigned>(mode) <= 3u) {                                // 0x004070fc / 0x004070ff
        int arm = 0;                                                        // 0 none, 1 A .. 6 F
        switch (mode) {
            case 0: arm = (type == 0xd) ? 1 : (type == 0xe) ? 2 : (type == 0xf) ? 3 : (type == 0x10) ? 5 : 0; break;   // 0x0040710c..0x00407209
            case 1: arm = (type == 0xd) ? 1 : (type == 0xe) ? 2 : (type == 0xf) ? 4 : (type == 0x10) ? 5 : 0; break;   // 0x0040715a..0x0040717d
            case 2: arm = (type == 0xd) ? 2 : (type == 0xe) ? 1 : (type == 0xf) ? 3 : (type == 0x10) ? 5 : 0; break;   // 0x004071a6..0x004071e9
            default: arm = (type >= 0xd && type <= 0x10) ? 6 : 0; break;    // 0x0040722b..0x0040723a
        }
        switch (arm) {
            case 1: hi = (RandRange(0x64) & 0xffff) + 0x190; lo = 0x32; break;   // 0x004071c9..0x004071df
            case 2: hi = (RandRange(0x64) & 0xffff) + 0x64; lo = 0x14; break;    // 0x0040712e / 0x00407255
            case 3: hi = (RandRange(0x64) & 0xffff) + 0x64; lo = 0x32; break;    // 0x00407143..0x00407152
            case 4: hi = (RandRange(0x64) & 0xffff) + 0x12c; lo = 0x64; break;   // 0x00407186..0x0040719c
            case 5: hi = (RandRange(0xc8) & 0xffff) + 0x190; lo = 0x4b; break;   // 0x0040720b..0x00407224
            case 6: hi = (RandRange(0x64) & 0xffff) + 0x190; lo = 0x14; break;   // 0x0040723f..0x00407250 / 0x00407255
            default: break;
        }
    }
    return (dist < hi && dist > lo) ? 1 : 0;                                // 0x0040725a..0x0040726d
}
C4_t InRange_orig;
// 0x004070b0
SG_HOOK("golf_clean.exe", 0x004070b0, inRange, InRange_re, InRange_orig);

// 0x0042f340  rebuildTileCoverage(): cdecl, no arguments, no result any caller reads. x = the short [0x0058bcbc], y = the short
// [0x0058bcba] (record 0's words +4 / +2; 0x0042f345 / 0x0042f34e). Bit 0x40 of the low byte of each of the 2500 words from
// 0x0053caf0 is cleared (and byte [..], 0xbf, 0x0042f363, 50 words per pass until 0x0053de78). w = the signed byte [0x004c27ec]:
// for rows y .. y + w - 1 and columns x .. x + w - 1 (0x0042f37b..0x0042f3e8, nothing when w <= 0) a tile word with a bit of
// 0x420 set (0x0042f3a6..0x0042f3b1) calls floodMark(row, col) (0x0042f3b3..0x0042f3b5). Then the 256 sixteen-byte records from
// 0x0058bcb8 (esi = record + 7, 0x0042f3ea..0x0042f49c): type t = the short at +0; t < 6 changes nothing (0x0042f3f3 / 0x0042f3f7);
// t >= 0x11 sets bit 0x40 of the byte at +7 (0x0042f3fd / 0x0042f401 / 0x0042f490); 6 <= t <= 0x10 clears it (0x0042f407), takes n =
// the signed byte [0x004c26c0 + 20 t], for t != 7 (0x0042f41e) replaces the dword [0x005a8c38 + 4 t] by clamp(it, [rec+8] + 1, 99)
// (0x0042f423..0x0042f43a) and n = that + n - 1 (0x0042f441); then for dr, dc in 0 .. n - 1 (0x0042f445..0x0042f479) a set bit 0x40 in
// the low byte of the tile word at 0x0053caf0 + 2 * ((dc + short rec+4) + 50 * (dr + short rec+2)) sets bit 0x40 of the record's
// byte +7 (0x0042f44d..0x0042f46e). Finally [0x00822c88] == 0 sets it, and so does a non-zero value with bit 0x1000000 of
// [0x0059e7b8] (0x0042f47b..0x0042f490).
void __cdecl RebuildTileCoverage_re() {
    const int x = G<short>(0x0058bcbc);                                     // 0x0042f345
    const int y = G<short>(0x0058bcba);                                     // 0x0042f34e
    for (unsigned a = 0x0053caf0; a < 0x0053de78; a += 2) G<unsigned char>(a) &= 0xbf;   // 0x0042f35e..0x0042f371
    const int w = G<signed char>(0x004c27ec);                               // 0x0042f373
    for (int row = y; row < y + w; row++) {                                 // 0x0042f37e..0x0042f3e8
        unsigned p = 0x0053caf0u + 2u * static_cast<unsigned>(x + 50 * y) + 100u * static_cast<unsigned>(row - y);   // 0x0042f385..0x0042f396 / 0x0042f3d9
        for (int col = x; col < x + w; col++, p += 2) {                     // 0x0042f39a..0x0042f3ce
            if ((G<unsigned short>(p) & 0x420) != 0) kFloodMark(row, col);  // 0x0042f3a6..0x0042f3b5
        }
    }
    for (unsigned rec = 0x0058bcb8; rec < 0x0058ccb8; rec += 0x10) {        // 0x0042f3ea / 0x0042f493 / 0x0042f496
        unsigned char& flag = G<unsigned char>(rec + 7);
        const int t = G<short>(rec);                                        // 0x0042f3ef
        if (t < 6) continue;                                                // 0x0042f3f3 / 0x0042f3f7
        if (t < 0x11) {                                                     // 0x0042f3fd / 0x0042f401
            flag &= 0xbf;                                                   // 0x0042f407
            int n = G<signed char>(0x004c26c0u + static_cast<unsigned>(t) * 20u);   // 0x0042f414
            if (t != 7) {                                                   // 0x0042f41e / 0x0042f421 (the jle 0x0042f41c on t > 5 cannot be taken: t >= 6 here)
                int lo = G<int>(rec + 8) + 1;                               // 0x0042f423 / 0x0042f42d
                int v = kClamp(G<int>(0x005a8c38u + static_cast<unsigned>(t) * 4u), lo, 0x63);   // 0x0042f426..0x0042f432
                G<int>(0x005a8c38u + static_cast<unsigned>(t) * 4u) = v;    // 0x0042f43a
                n = v + n - 1;                                              // 0x0042f441
            }
            for (int dr = 0; dr < n; dr++) {                                // 0x0042f445..0x0042f479
                for (int dc = 0; dc < n; dc++) {
                    int idx = (G<short>(rec + 4) + dc) + 50 * (G<short>(rec + 2) + dr);   // 0x0042f44d..0x0042f461
                    if (G<unsigned char>(0x0053caf0u + static_cast<unsigned>(idx) * 2u) & 0x40) flag |= 0x40;   // 0x0042f464 / 0x0042f46e
                }
            }
            if (G<int>(0x00822c88) == 0 || (G<unsigned>(0x0059e7b8) & 0x1000000) != 0) flag |= 0x40;   // 0x0042f47b..0x0042f490
            continue;
        }
        flag |= 0x40;                                                       // 0x0042f490
    }
}
C0_t RebuildTileCoverage_orig;
// 0x0042f340
SG_HOOK("golf_clean.exe", 0x0042f340, rebuildTileCoverage, RebuildTileCoverage_re, RebuildTileCoverage_orig);

// 0x00409a90  scanNearbyGolfers(g): cdecl, plain `ret`. Record base s = g << 8 (0x00409a9a); fields x = [s+0x57958c], y = [s+0x579590].
// For each of the 256 entries (stride 0x24 from 0x005736b0; ebx = entry + 4, 0x00409a9d / 0x00409b3f / 0x00409b42):
// an entry whose dword +8 is not -1 (0x00409aa2) draws Random::range(0x00822d9c, ([0x00822c88] << 10) / 4 + 0x600) (0x00409aa8..0x00409ac4)
// and calls distance(entry.x - x, entry.y - y) (0x00409acf..0x00409ae9); the comparison only feeds a register nothing reads. A non-zero
// bit 0 of [0x0056c7b4] (0x00409af6) is cleared (0x00409aff..0x00409b14) and emitGolferThought(g, 0x18, 0x14) is called (0x00409b19). Every
// entry calls holeQuadrants(x, y, 6) (0x00409b21..0x00409b31). Afterwards the first entry whose dword +8 is -1 (0x00409b4e..0x00409b66)
// receives {+0 x, +4 y, +8 x, +0xc y, +0x10 [s+0x579594], +0x14 [s+0x579598], +0x18 [s+0x57959c] / 2, +0x1c [s+0x5795a0] / 3,
// +0x20 [0x00834170]} (0x00409b6b..0x00409bdb). eax at the ret is the entry count scanned (256) when none was free (0x00409b5d), else
// the sign bit of the high word of [s+0x5795a0] * 0x55555556 (shr 31 at 0x00409bcf).
int __cdecl ScanNearbyGolfers_re(int g) {
    const unsigned s = static_cast<unsigned>(g) << 8;                       // 0x00409a9a
    const unsigned base = 0x005736b0;
    for (unsigned e = base; e < 0x00575ab0; e += 0x24) {                    // 0x00409a9d / 0x00409b3f / 0x00409b42 (ebx = e + 4)
        if (G<int>(e + 8) != -1) {                                          // 0x00409aa2
            int arg = (G<int>(0x00822c88) << 10);                           // 0x00409aa8 / 0x00409ab2
            arg += (arg >> 31) & 3;                                         // 0x00409ab5 / 0x00409ab6 / 0x00409ab9
            arg = (arg >> 2) + 0x600;                                       // 0x00409abb / 0x00409abe
            int r = RandRange(arg) & 0xffff;                                // 0x00409ac4 / 0x00409ad2 / 0x00409ae3
            int dx = G<int>(e) - G<int>(s + 0x0057958c);                    // 0x00409acf / 0x00409adf
            int dy = G<int>(e + 4) - G<int>(s + 0x00579590);                // 0x00409ad5 / 0x00409ad7
            (void)(kDistance(dx, dy) < r);                                  // 0x00409ae9..0x00409af3
        }
        if (G<unsigned char>(0x0056c7b4) & 1) {                             // 0x00409af6
            G<unsigned>(0x0056c7b4) &= ~1u;                                 // 0x00409aff..0x00409b14
            kEmitGolferThought(g, 0x18, 0x14);                              // 0x00409b19
        }
        kHoleQuadrants(G<int>(s + 0x0057958c), G<int>(s + 0x00579590), 6);  // 0x00409b21..0x00409b31
    }
    int i = 0;
    for (unsigned e = 0x005736b8; G<int>(e) != -1; e += 0x24) {             // 0x00409b50..0x00409b64
        i++;
        if (e + 0x24 >= 0x005736b8u + 0x2400u) return i;                    // 0x00409b5e: the loop ends at 256 entries
    }
    const unsigned o = static_cast<unsigned>(i) * 0x24u;                    // 0x00409b6b / 0x00409b74
    G<int>(0x005736b0 + o) = G<int>(s + 0x0057958c);                        // 0x00409b7e
    G<int>(0x005736b8 + o) = G<int>(s + 0x0057958c);                        // 0x00409b84
    G<int>(0x005736b4 + o) = G<int>(s + 0x00579590);                        // 0x00409b90
    G<int>(0x005736bc + o) = G<int>(s + 0x00579590);                        // 0x00409b96
    G<int>(0x005736c0 + o) = G<int>(s + 0x00579594);                        // 0x00409ba2
    G<int>(0x005736c4 + o) = G<int>(s + 0x00579598);                        // 0x00409ba8
    int h = G<int>(s + 0x0057959c);
    G<int>(0x005736c8 + o) = (h - (h >> 31)) >> 1;                          // 0x00409bb4..0x00409bbf
    int hi = static_cast<int>((static_cast<long long>(G<int>(s + 0x005795a0)) * 0x55555556LL) >> 32);   // 0x00409bc5 / 0x00409bca
    int sign = static_cast<int>(static_cast<unsigned>(hi) >> 31);           // 0x00409bcf
    G<int>(0x005736cc + o) = hi + sign;                                     // 0x00409bd2 / 0x00409bd5
    G<int>(0x005736d0 + o) = G<int>(0x00834170);                            // 0x00409bdb / 0x00409be1
    return sign;                                                            // 0x00409bcc / 0x00409bcf
}
C1_t ScanNearbyGolfers_orig;
// 0x00409a90
SG_HOOK("golf_clean.exe", 0x00409a90, scanNearbyGolfers, ScanNearbyGolfers_re, ScanNearbyGolfers_orig);

// 0x00409620  scanShotArea(g): cdecl, plain `ret`, no result any caller reads. [0x004c2844] < 4 returns (0x0040962f). z = the
// short [g * 904 + 0x0059fc60] (0x00409645..0x00409653); z == ([0x004c2e0c] & 0xff) returns (0x0040965b / 0x00409661). s = z << 8.
// (byte [s+0x5794d0] & 0xe0) == 0x20 returns unless z == [0x005a59f8] (0x0040966c..0x0040967e). With P = {[s+0x57957c], [s+0x579580]}
// and Q = {[s+0x57958c], [s+0x579590]}: D = (25 * distance(P0 - Q0, P1 - Q1) + ((.. >> 31) & 0x3ff)) >> 10 (0x00409696..0x004096bd);
// R = f40c4b0(Q0, Q1, [c * 520 + 0x00575ac8], [c * 520 + 0x00575acc]) with c = the signed byte [s+0x5794d9] (0x004096c4..0x004096ea);
// T = f40bfa0(P0, P1) (0x00409703); S = the sum of the ten bytes [s+0x5795a8..] when z == [0x005a59f8] else 0 (0x00409716..0x00409727);
// U = f40bfa0(Q0, Q1) (0x0040973d); attr(i) = the signed byte [48 i + 0x00578372]. attr(U) > 0 (0x00409759): z != [0x005a59f8]
// returns; else with n = 250 * (attr(T) + 4) / ([0x00822c88] + 2) (0x0040976c..0x00409791) a draw Random::range(n) & 0xffff not
// below attr(U) * S returns (0x00409799..0x004097ab), else narrateShot(g, R, D, 1) (0x004097c2). attr(U) <= 0: v = 0x1e / 0x19 / 0x14 /
// 0xf for the signed byte b = [s+0x5794d3] below 5 / below 10 / 10 / above 10 (0x004097d2..0x004097f1); k = R (the local at esp+0x10):
// k == 0 halves v (0x004097fc..0x00409803); k > 0x19 with b > 0 and attr(f40bfa0(Q0, Q1)) <= 0 sets k = 0x19 and v = 200 / b
// (0x00409805..0x00409848). z == [0x005a59f8]: attr(T) > 0 adds v * clamp(attr(T), 0, 3) * 0xd5555555 >> 32 (+ sign) to v
// (0x0040985e..0x00409891); a draw ((Random::range(S) & 0xffff) + 4 [0x00822c88] + 5) * v, divided by 32 truncating, replaces v and a
// non-zero holeQuadrants([s+0x5794b8], [s+0x5794bc], 4) halves it (0x00409893..0x004098ec). D > (k + 1) * v calls narrateShot(g, R,
// D, 0) and clears bit 0x10 of [0x0056c7b4] (0x0040991a..0x00409936); otherwise the same when bit 0x10 of [0x0056c7b4] is set and
// ([0x005a59f8] == z or [0x005a59f8] == -1) (0x004098fe..0x00409918).
void __cdecl ScanShotArea_re(int g) {
    if (G<int>(0x004c2844) < 4) return;                                     // 0x0040962f
    const int e = G<int>(0x004c2e0c) & 0xff;                                // 0x00409639 / 0x0040963f
    const int z = G<short>(0x0059fc60u + static_cast<unsigned>(g) * 904u);  // 0x00409645..0x00409653
    if (z == e) return;                                                     // 0x0040965b / 0x00409661
    const unsigned s = static_cast<unsigned>(z) << 8;                       // 0x00409669
    if ((G<unsigned char>(s + 0x005794d0) & 0xe0) == 0x20 && z != G<int>(0x005a59f8)) return;   // 0x0040966c..0x0040967e
    const int p0 = G<int>(s + 0x0057957c), p1 = G<int>(s + 0x00579580);
    const int q0 = G<int>(s + 0x0057958c), q1 = G<int>(s + 0x00579590);
    const int d = kDistance(p0 - q0, p1 - q1);                              // 0x00409696..0x004096a2
    int v25 = d * 25;                                                       // 0x004096a7 / 0x004096b1
    const int D = (v25 + ((v25 >> 31) & 0x3ff)) >> 10;                      // 0x004096b4..0x004096bd
    const int c = G<signed char>(s + 0x005794d9);                           // 0x004096aa
    const unsigned co = static_cast<unsigned>(c) * 520u;                    // 0x004096c4..0x004096cb
    const int R = kProbe40c4b0(q0, q1, G<int>(co + 0x00575ac8), G<int>(co + 0x00575acc));   // 0x004096d4..0x004096ea
    int k = R;                                                              // 0x004096ff (esp+0x10)
    const int T = kTileType40bfa0(p0, p1);                                  // 0x00409703
    int S = 0;                                                              // 0x00409716
    if (z == G<int>(0x005a59f8)) {                                          // 0x0040970a / 0x00409718
        for (int i = 0; i < 10; i++) S += G<unsigned char>(s + i + 0x005795a8);   // 0x0040971c..0x0040972d
    }
    const int U = kTileType40bfa0(G<int>(s + 0x0057958c), G<int>(s + 0x00579590));   // 0x0040973d
    const signed char attrU = G<signed char>(static_cast<unsigned>(U) * 48u + 0x00578372);   // 0x0040974b
    if (attrU > 0) {                                                        // 0x00409757 / 0x00409759
        if (z != G<int>(0x005a59f8)) return;                                // 0x00409764 / 0x00409766
        int num = (static_cast<int>(G<signed char>(static_cast<unsigned>(T) * 48u + 0x00578372)) + 4) * 250;   // 0x0040977b..0x0040978e
        int n = num / (G<int>(0x00822c88) + 2);                             // 0x00409778 / 0x00409790 / 0x00409791
        int r = RandRange(n) & 0xffff;                                      // 0x00409799 / 0x004097a4
        if (r >= static_cast<int>(attrU) * S) return;                       // 0x0040979e..0x004097ab
        kNarrateShot(g, R, D, 1);                                           // 0x004097c2
        return;
    }
    const int b = G<signed char>(s + 0x005794d3);                           // 0x004097d2
    int v = 0x14;                                                           // 0x004097d8
    if (b < 5) v = 0x19;                                                    // 0x004097dd / 0x004097df / 0x004097e1
    if (b < 10) v += 5;                                                     // 0x004097e6 / 0x004097e8 / 0x004097ea
    if (b > 10) v -= 5;                                                     // 0x004097ef / 0x004097f1
    if (k == 0) {                                                           // 0x004097f8 / 0x004097fa
        v = (v - (v >> 31)) >> 1;                                           // 0x004097fc..0x00409803
    } else if (k > 0x19 && b > 0) {                                         // 0x00409805 / 0x00409808 / 0x0040980a / 0x0040980c
        int W = kTileType40bfa0(G<int>(s + 0x0057958c), G<int>(s + 0x00579590));   // 0x0040981c
        if (!(G<signed char>(static_cast<unsigned>(W) * 48u + 0x00578372) > 0)) {   // 0x0040982a / 0x00409830 / 0x00409832
            k = 0x19;                                                       // 0x00409840
            v = 200 / G<signed char>(s + 0x005794d3);                       // 0x00409834..0x00409849
        }
    }
    if (z == G<int>(0x005a59f8)) {                                          // 0x0040984d..0x00409858
        signed char at = G<signed char>(static_cast<unsigned>(T) * 48u + 0x00578372);   // 0x0040985e..0x0040986a
        if (at > 0) {                                                       // 0x0040986c
            int cl = kClamp(at, 0, 3);                                      // 0x00409871..0x00409876
            int hi = static_cast<int>((static_cast<long long>(static_cast<int>(0xd5555555)) * static_cast<int>(static_cast<unsigned>(cl) * static_cast<unsigned>(v))) >> 32);   // 0x00409882..0x00409885
            hi += static_cast<int>(static_cast<unsigned>(hi) >> 31);        // 0x00409887..0x0040988f
            v += hi;                                                        // 0x00409891
        }
    }
    if (z == G<int>(0x005a59f8)) {                                          // 0x00409893..0x0040989f
        int r = RandRange(S) & 0xffff;                                      // 0x004098a1..0x004098b2
        int x = (r + G<int>(0x00822c88) * 4 + 5) * v;                       // 0x004098bf / 0x004098c3
        x += (x >> 31) & 0x1f;                                              // 0x004098c6..0x004098ca
        v = x >> 5;                                                         // 0x004098cc / 0x004098cf
        if (kHoleQuadrants(G<int>(s + 0x005794b8), G<int>(s + 0x005794bc), 4) != 0) v = (v - (v >> 31)) >> 1;   // 0x004098d9..0x004098ec
    }
    if (D > static_cast<int>(static_cast<unsigned>(k + 1) * static_cast<unsigned>(v)) || ((G<unsigned char>(0x0056c7b4) & 0x10) != 0 && (z == G<int>(0x005a59f8) || G<int>(0x005a59f8) == -1))) {   // 0x004098ee..0x00409918
        kNarrateShot(g, R, D, 0);                                           // 0x00409927
        G<unsigned>(0x0056c7b4) &= 0xffffffefu;                             // 0x0040992c..0x00409936
    }
}
C1_t ScanShotArea_orig;
// 0x00409620
SG_HOOK("golf_clean.exe", 0x00409620, scanShotArea, ScanShotArea_re, ScanShotArea_orig);

// 0x004668f0  storyText(a, i, g): cdecl, plain `ret`; returns the first character (sign-extended) of the text buffer 0x0051a068.
// Record s = g << 8; f = [s+0x5794c8] & 0x100000 (0x004668fc / 0x00466902). f != 0: key = byte [s + i + 0x579502] (0x00466910); f == 0:
// key = byte [(short [s+0x57955a]) * 256 + i + 0x579502] (0x00466919..0x00466929). key == 0 returns 0 (0x00466934 / 0x00466936 /
// 0x0046693a). Else computeStoryReply(a, key, i, f ? g : g ^ 1) (0x00466942..0x0046694d) fills the buffer; len = strlen(buffer)
// (repne scasb, 0x00466952..0x00466969). f != 0 (0x0046696a): every '\n' within len becomes NUL (0x00466977..0x0046698a). f == 0: the
// text after the first '\n' within len moves to the buffer start including its NUL (0x0046699f..0x004669e1); with no '\n' the buffer
// becomes empty (0x004669ac). The return is the signed byte at 0x0051a068 (0x0046698c / 0x004669b2 / 0x004669e3).
int __cdecl StoryText_re(int a, int i, int g) {
    const unsigned s = static_cast<unsigned>(g) << 8;                       // 0x004668f6 / 0x004668f8
    const unsigned f = G<unsigned>(s + 0x005794c8) & 0x100000;              // 0x004668fc / 0x00466902
    unsigned key;
    if (f != 0) {
        key = G<unsigned char>(s + static_cast<unsigned>(i) + 0x00579502);  // 0x00466910
    } else {
        int rec = static_cast<int>(G<short>(s + 0x0057955a)) << 8;          // 0x00466919 / 0x00466924
        key = G<unsigned char>(static_cast<unsigned>(rec) + static_cast<unsigned>(i) + 0x00579502);   // 0x00466929
    }
    if (key == 0) return 0;                                                 // 0x00466934 / 0x00466936
    int gg = g;
    if (f == 0) gg ^= 1;                                                    // 0x0046693e / 0x00466940 / 0x00466942
    kComputeStoryReply(a, static_cast<int>(key), i, gg);                    // 0x0046694d
    char* buf = reinterpret_cast<char*>(0x0051a068);
    int len = 0;
    while (buf[len] != 0) len++;                                            // 0x0046695f
    if (f != 0) {                                                           // 0x0046696a
        for (int k = 0; k < len; k++) if (buf[k] == '\n') buf[k] = 0;       // 0x00466977..0x0046698a
        return buf[0];                                                      // 0x0046698c
    }
    int k = 0;
    while (k < len && buf[k] != '\n') k++;                                  // 0x0046699f..0x004669aa
    if (k >= len) {                                                         // 0x004669aa (no newline within len)
        buf[0] = 0;                                                         // 0x004669ac
        return buf[0];                                                      // 0x004669b2
    }
    int tail = 0;
    while (buf[k + 1 + tail] != 0) tail++;                                  // 0x004669c8
    tail++;                                                                 // not ecx (includes the NUL)
    const char* src = buf + k + 1;                                          // 0x004669bd / 0x004669cc
    for (int m = 0; m < tail; m++) buf[m] = src[m];                         // 0x004669da / 0x004669e1 (dest below src: forward copy)
    return buf[0];                                                          // 0x004669e3
}
C3_t StoryText_orig;
// 0x004668f0
SG_HOOK("golf_clean.exe", 0x004668f0, storyText, StoryText_re, StoryText_orig);
