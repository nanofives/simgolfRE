// C3 batch c3fg (round 34, 2026-10-13/14) of sound.dll: the MMIO rows (log/c3/c3fg_candidates.tsv, log/c3/round34_brief.md),
// A/B'd over the mmio stand-in re/frida/js/mmiostandin.js (fixture files from log/c3/c3fg_gen.py).
// Method as c3fd / c3dt: fixture-owned objects only, module globals the body reads or writes swapped for the key with the
// first thread frozen, operator new / delete / malloc / free through heapstandin.js, winmm imports through the mmio stand-in.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are
// file VAs (VA = 0x10000000 + RVA), from log/c3/alt/c3fg_asm/<rva>_<name>.txt (dumped by log/c3/alt/c3fg_survey.py; thunk
// targets in log/c3/alt/c3fg_thunks.txt). sound.dll loads above its preferred base: module globals, thunks and IAT slots are
// taken relative to the loaded base. __thiscall is emulated with __fastcall (ecx = this, edx unused). Every callee runs
// through the address the original calls (its incremental-link thunk when the original calls the thunk), and every import
// through the original's IAT slot, so the stand-ins see the reimplementation's calls exactly as the original's.
#include "hooks.h"
#include <stdint.h>
#include <string.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

char* c3fg_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3fg_fn(unsigned rva) { return reinterpret_cast<F>(c3fg_sound(rva)); }
template <typename T> T& c3fg_g(unsigned rva) { return *reinterpret_cast<T*>(c3fg_sound(rva)); }
// the import whose IAT slot is at RVA `slotRva` (`call dword ptr [0x100b....]`)
template <typename F> F c3fg_imp(unsigned slotRva) { return *reinterpret_cast<F*>(c3fg_sound(slotRva)); }

typedef void*(__cdecl* OpNew_t)(unsigned);
typedef void(__cdecl* OpDelete_t)(void*);
void* c3fg_new(unsigned n) { return c3fg_fn<OpNew_t>(0x000424d7)(n); }        // operator new 0x100424d7
void c3fg_delete(void* p) { c3fg_fn<OpDelete_t>(0x0004249a)(p); }             // operator delete 0x1004249a
void c3fg_free(void* p) { c3fg_fn<OpDelete_t>(0x00042c07)(p); }               // free 0x10042c07
void* c3fg_malloc(unsigned n) { return c3fg_fn<OpNew_t>(0x00042acd)(n); }     // _malloc 0x10042acd

typedef unsigned(__fastcall* Th0_t)(void*, void*);
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);
typedef unsigned(__fastcall* Th3_t)(void*, void*, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th4_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th5_t)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th6_t)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
// virtual slot `off` of `obj` (`mov eax, [obj]; call [eax + off]`)
template <typename F> F c3fg_vfn(void* obj, int off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}
unsigned c3fg_u(const void* p) { return static_cast<unsigned>(reinterpret_cast<uintptr_t>(p)); }

// WINMM IAT slots of sound.dll (import name table, log/c3/alt/c3fg_thunks.py)
const unsigned kMmioDescend = 0x000b8624, kMmioAscend = 0x000b8628, kMmioSetBuffer = 0x000b862c,
               kMmioAdvance = 0x000b8630, kMmioSetInfo = 0x000b8634, kMmioWrite = 0x000b8658,
               kMmioStringToFourcc = 0x000b865c, kMmioOpen = 0x000b8660, kMmioRead = 0x000b8664,
               kMmioGetInfo = 0x000b8668, kMmioSeek = 0x000b866c, kMmioClose = 0x000b8670;
typedef unsigned(__stdcall* MmioClose_t)(void*, unsigned);
typedef long(__stdcall* MmioRead_t)(void*, void*, long);
typedef long(__stdcall* MmioSeek_t)(void*, long, int);
typedef void*(__stdcall* MmioOpen_t)(char*, void*, unsigned);
typedef unsigned(__stdcall* MmioInfo_t)(void*, void*, unsigned);          // GetInfo / SetInfo / Advance
typedef unsigned(__stdcall* MmioSetBuffer_t)(void*, void*, long, unsigned);
typedef unsigned(__stdcall* MmioDescend_t)(void*, void*, void*, unsigned);
typedef unsigned(__stdcall* MmioAscend_t)(void*, void*, unsigned);
typedef unsigned(__stdcall* MmioFourcc_t)(const char*, unsigned);
typedef long(__stdcall* MmioWrite_t)(void*, const void*, long);
unsigned c3fg_mmioClose(void* h, unsigned f) { return c3fg_imp<MmioClose_t>(kMmioClose)(h, f); }
long c3fg_mmioRead(void* h, void* p, long n) { return c3fg_imp<MmioRead_t>(kMmioRead)(h, p, n); }
long c3fg_mmioSeek(void* h, long o, int w) { return c3fg_imp<MmioSeek_t>(kMmioSeek)(h, o, w); }
void* c3fg_mmioOpen(char* n, void* i, unsigned f) { return c3fg_imp<MmioOpen_t>(kMmioOpen)(n, i, f); }
unsigned c3fg_mmioGetInfo(void* h, void* i, unsigned f) { return c3fg_imp<MmioInfo_t>(kMmioGetInfo)(h, i, f); }
unsigned c3fg_mmioSetInfo(void* h, void* i, unsigned f) { return c3fg_imp<MmioInfo_t>(kMmioSetInfo)(h, i, f); }
unsigned c3fg_mmioAdvance(void* h, void* i, unsigned f) { return c3fg_imp<MmioInfo_t>(kMmioAdvance)(h, i, f); }
unsigned c3fg_mmioSetBuffer(void* h, void* b, long n, unsigned f) { return c3fg_imp<MmioSetBuffer_t>(kMmioSetBuffer)(h, b, n, f); }
unsigned c3fg_mmioDescend(void* h, void* c, void* p, unsigned f) { return c3fg_imp<MmioDescend_t>(kMmioDescend)(h, c, p, f); }
unsigned c3fg_mmioAscend(void* h, void* c, unsigned f) { return c3fg_imp<MmioAscend_t>(kMmioAscend)(h, c, f); }
unsigned c3fg_fourcc(const char* s, unsigned f) { return c3fg_imp<MmioFourcc_t>(kMmioStringToFourcc)(s, f); }
long c3fg_mmioWrite(void* h, const void* p, long n) { return c3fg_imp<MmioWrite_t>(kMmioWrite)(h, p, n); }

// module globals (file VAs 0x100b....)
const unsigned kSoundTimer = 0x000b49ec;       // g_soundTimer (pointer; its first dword is the timer period in ms)
const unsigned kOpenSoundList = 0x000b49c8;    // g_openSoundList: head +0 (0x100b49c8), tail +4, cursor +8, count +0xc
const unsigned kOpenSoundTail = 0x000b49cc;    // the unlinks write the removed last node's prev here (0x100320bc)
const unsigned kOpenSoundCur = 0x000b49d0;     // written by every unlink (0x100320a8 / 0x100320b2)
const unsigned kOpenSoundCount = 0x000b49d4;   // g_openSoundCount
const unsigned kOpenMmioCount = 0x000b4a08;    // g_openMmioCount

struct ListNode { ListNode* prev; ListNode* next; void* value; };

// The unlink sequence shared by Sound_resetBuffers (0x10032074-0x100320d1), closeMmioSound (0x10035ce5-0x10035d42) and
// parseRiffWave (0x10032b7a-0x10032bd8): the first node of g_openSoundList whose value is `value` is taken out (prev->next
// or the head updated, next->prev updated and the cursor 0x100b49d0 set to next; with no next, the cursor = 0 and the tail
// 0x100b49cc = the node's prev), freed with operator delete, and g_openSoundCount lowered. Returns the node (NULL when none
// matched) and, through *count, the lowered count (eax of the originals after the decrement).
ListNode* c3fg_unlinkOpenSound(void* value, unsigned* count) {
    ListNode* n = c3fg_g<ListNode*>(kOpenSoundList);
    while (n && n->value != value) n = n->next;
    if (!n) return 0;
    if (n->prev) n->prev->next = n->next;
    else c3fg_g<ListNode*>(kOpenSoundList) = n->next;
    if (n->next) {
        n->next->prev = n->prev;
        c3fg_g<ListNode*>(kOpenSoundCur) = n->next;
    } else {
        c3fg_g<ListNode*>(kOpenSoundCur) = 0;
        c3fg_g<ListNode*>(kOpenSoundTail) = n->prev;
    }
    c3fg_delete(n);
    *count = --c3fg_g<unsigned>(kOpenSoundCount);
    return n;
}

// 0x0001d380  MidiReader_init (VA 0x1001d380). __thiscall, no stack argument (plain `ret` 0x1001d459), returns this
// (`mov eax, esi` 0x1001d454), no callee. Zeroes +0x19c..+0x1a8 (0x1001d38c-0x1001d39e), +0 (0x1001d3a9), the 0x12 dwords
// +4..+0x4b (`rep stosd` 0x1001d3b1), +0x4c (0x1001d3bd), +0x150..+0x15b (0x1001d3bf-0x1001d3c4; the zero written to +0x15c
// at 0x1001d3c7 is overwritten with 0x78 at 0x1001d3ca), +0x160..+0x170 (0x1001d3d4-0x1001d3ec); +0x178 = +0x17c = 0x10000
// (0x1001d3f2 / 0x1001d3f8). +0x174 comes from g_soundTimer 0x100b49ec (0x1001d3fe): left alone when it is NULL
// (0x1001d405); 0x10000 when the period [timer] is >= 1000 (`jae` 0x1001d40f, 0x1001d42a); else 0x10000 / (1000 / period),
// two unsigned divisions (0x1001d418 / 0x1001d420; a period of 0 faults at 0x1001d418). Then +0x180..+0x190 and +0x198 = 0
// (0x1001d430-0x1001d44e). +0x194 and +0x50..+0x14f are not written.
typedef void*(__fastcall* MidiInit_t)(void*, void*);
MidiInit_t MidiInit_orig;
void* __fastcall MidiInit_re(void* self, void*) {
    for (int off = 0x19c; off <= 0x1a8; off += 4) fat<unsigned>(self, off) = 0;    // 0x1001d38c-0x1001d39e
    fat<unsigned>(self, 0) = 0;                                                     // 0x1001d3a9
    for (int off = 4; off < 0x4c; off += 4) fat<unsigned>(self, off) = 0;           // 0x1001d3b1
    fat<unsigned>(self, 0x4c) = 0;                                                  // 0x1001d3bd
    fat<unsigned>(self, 0x150) = 0;                                                 // 0x1001d3bf
    fat<unsigned>(self, 0x154) = 0;                                                 // 0x1001d3c1
    fat<unsigned>(self, 0x158) = 0;                                                 // 0x1001d3c4
    fat<unsigned>(self, 0x15c) = 0x78;                                              // 0x1001d3ca
    for (int off = 0x160; off <= 0x170; off += 4) fat<unsigned>(self, off) = 0;    // 0x1001d3d4-0x1001d3ec
    fat<unsigned>(self, 0x178) = 0x10000;                                           // 0x1001d3f2
    fat<unsigned>(self, 0x17c) = 0x10000;                                           // 0x1001d3f8
    const unsigned* timer = c3fg_g<const unsigned*>(kSoundTimer);                   // 0x1001d3fe
    if (timer) {                                                                    // 0x1001d405
        unsigned period = timer[0];                                                 // 0x1001d407
        if (period >= 1000) fat<unsigned>(self, 0x174) = 0x10000;                   // 0x1001d40f / 0x1001d42a
        else fat<unsigned>(self, 0x174) = 0x10000u / (1000u / period);              // 0x1001d418 / 0x1001d420
    }
    for (int off = 0x180; off <= 0x190; off += 4) fat<unsigned>(self, off) = 0;    // 0x1001d430-0x1001d448
    fat<unsigned>(self, 0x198) = 0;                                                 // 0x1001d44e
    return self;                                                                    // 0x1001d454
}

// 0x00032020  Sound_resetBuffers (VA 0x10032020). __thiscall, no stack argument (plain `ret` 0x100320e5). The decode buffer
// +8, when set, is freed with operator delete and cleared (0x1003202a-0x10032035). The 0x12-dword MMIOINFO at +0xc is zeroed
// (`rep stosd` 0x10032048), +0x58 and +0x5c are zeroed (0x10032051 / 0x10032053; the `and 0xfffffffc` on +0x58 at 0x10032058
// leaves 0). When the HMMIO +0 is set (0x10032061): mmioClose(h, 0) (0x10032065), the node of g_openSoundList whose value is
// the MMIOINFO's address this+0xc (`cmp [eax+8], ebx`, 0x10032074) is unlinked and freed (c3fg_unlinkOpenSound), and
// g_openMmioCount 0x100b4a08 is lowered whether or not a node matched (0x100320d6). +0 = 0 on every path (0x100320dc).
// eax at the `ret`: the lowered g_openSoundCount when a node was unlinked (0x100320d0), else 0 (the NULL handle, the NULL
// list head or the walk's last NULL next).
typedef unsigned(__fastcall* ResetBufs_t)(void*, void*);
ResetBufs_t ResetBufs_orig;
unsigned __fastcall ResetBufs_re(void* self, void*) {
    if (fat<void*>(self, 8)) {                                                      // 0x1003202a
        c3fg_delete(fat<void*>(self, 8));                                           // 0x1003202d
        fat<void*>(self, 8) = 0;                                                    // 0x10032035
    }
    memset(&fat<char>(self, 0xc), 0, 0x12 * 4);                                     // 0x10032048
    fat<unsigned>(self, 0x58) = 0;                                                  // 0x10032051 / 0x1003205b
    fat<unsigned>(self, 0x5c) = 0;                                                  // 0x10032053
    unsigned eax = 0;
    void* h = fat<void*>(self, 0);                                                  // 0x1003205d
    if (h) {                                                                        // 0x10032061
        c3fg_mmioClose(h, 0);                                                       // 0x10032065
        unsigned count = 0;
        if (c3fg_unlinkOpenSound(&fat<char>(self, 0xc), &count)) eax = count;       // 0x1003206b-0x100320d1
        --c3fg_g<unsigned>(kOpenMmioCount);                                         // 0x100320d6
    }
    fat<void*>(self, 0) = 0;                                                        // 0x100320dc
    return eax;
}

// The close path parseRiffWave takes when a decode stream answers 0 or 0x23 (0x10032b66-0x10032bec, 0x10032c3a-0x10032cbc):
// mmioClose(+0, 0), the g_openSoundList node of this+0xc unlinked and freed, g_openMmioCount lowered, +0 = 0.
void c3fg_closeOpenSound(void* self) {
    c3fg_mmioClose(fat<void*>(self, 0), 0);                                         // 0x10032b6b / 0x10032c3f
    unsigned count = 0;
    c3fg_unlinkOpenSound(&fat<char>(self, 0xc), &count);                            // 0x10032b71-0x10032bd8
    --c3fg_g<unsigned>(kOpenMmioCount);                                             // 0x10032bdd-0x10032be6
    fat<void*>(self, 0) = 0;                                                        // 0x10032bec / 0x10032cbc
}

struct MmCk { unsigned ckid, cksize, fccType, dwDataOffset, dwFlags; };             // MMCKINFO
typedef char*(__cdecl* Strstr_t)(const char*, const char*);
// the DLL's own four-character strings (0x10063eb8..0x10063ee0) and strstr arguments (0x10063ee8 / 0x10063eec)
const char* c3fg_str(unsigned rva) { return c3fg_sound(rva); }
const unsigned kStrData = 0x00063eb8, kStrCue = 0x00063ec0, kStrSmpl = 0x00063ec8, kStrFmt = 0x00063ed0,
               kStrWave = 0x00063ed8, kStrRiff = 0x00063ee0, kStrMP3 = 0x00063ee8, kStrMp3 = 0x00063eec;
const unsigned kWaveDevice = 0x000b4a20;       // the Wave_Device create_device stores (tested at 0x10032b3b / 0x10032c0f)
const unsigned kDsDevice = 0x000b49f0;         // the DirectSound device passed to the decode streams and createBufferFmt

// 0x000326a0  parseRiffWave (VA 0x100326a0). __thiscall, one stack argument: the file name (`ret 4`; read as [esp+0x78] at
// 0x100326a6 after three pushes). Returns 0xb for a name containing `mp3` or `MP3` (strstr 0x10042690 with 0x10063eec /
// 0x10063ee8, 0x100326b3 / 0x100326c9, return 0x1003300e). With +0x58 bit 1 set, Channel_releaseBuffers (thunk 0x10001d11,
// 0x10032738) runs first and a non-zero result is returned (0x1003273f -> 0x10033013). Then +0 = 0, the MMIOINFO +0xc and
// the 0xb dwords +0x194..+0x1bf are zeroed (0x10032759 / 0x10032766), +0x164..+0x174 = 0 (0x1003276d-0x10032785), the
// MMIOINFO's cchBuffer +0x20 = 0x20000 and fccIOProc +0x10 = 'MEM ' (0x1003278b / 0x10032792), the name is copied to +0x64
// (0x1003279a-0x100327b2) and opened with mmioOpenA(+0x64, NULL, 0) into +0 (0x100327b4). A NULL handle zeroes the MMIOINFO
// again and returns 8 (0x100327cc / 0x100327d1). Otherwise g_openMmioCount 0x100b4a08 is raised, +0x58 |= 2 and the
// MMIOINFO's address is appended to g_openSoundList by listAppendNodeB (thunk 0x10001bb8, ecx 0x100b49c8, 0x100327fc).
// The chunks, each four-character code from mmioStringToFOURCCA(<DLL string>, 0): RIFF / WAVE (MMIO_FINDRIFF 0x20, no
// parent, 0x10032834), `fmt ` (MMIO_FINDCHUNK 0x10 under it, 0x1003286d); either missing calls Sound_resetBuffers (thunk
// 0x10001aaf) and returns 0x12. 0x10 bytes of `fmt ` are read to +0x180 (0x1003289c), the word +0x190 = 0 (0x100328a9),
// ascend (0x100328b2). `smpl` (0x100328d5): found, 0x24 bytes are read to +0x194 (0x100328e8); with a non-zero loop count
// +0x1b0, count * 0x18 bytes are allocated to +0x1b8 (0x100328fb) and read (0x1003291b), +0x1368 = (end - start + 1) *
// blockAlign and +0x136c = blockAlign * start from the first loop record (+8 start, +0xc end; blockAlign = the word +0x18c,
// 0x10032923-0x10032940); ascend (0x10032951). Missing: +0x1a0 = 0x3c (0x10032959). Then RIFF again after mmioSeek(0, 0)
// (0x10032978 / 0x10032998), `cue ` (0x100329d1): found, a dword count is read into the argument's stack slot (zeroed first,
// 0x100329d7 / 0x100329e6) and `count` 0x18-byte records are allocated, read and appended to the list +0x1348 by
// listAppendNodeA (thunk 0x100012bc), then ascend (0x10032a39). The RIFF chunk is ascended (0x10032a4b), RIFF found a third
// time after mmioSeek(0, 0) (0x10032a63 / 0x10032a81) and `data` (0x10032ab9); missing -> 0x12. +0x174 = the data offset,
// +0x178 = its size, +0x168 = size / blockAlign, +0x139c = +0x168 * 1000 / nSamplesPerSec +0x184 (unsigned 32-bit, 0x10032adf-
// 0x10032b12); without a loop table, +0x1368 = size and +0x136c = 0 (0x10032b22 / 0x10032b28).
// Decode streams: +0x58 bit 29 (0x10032b2e) asks beginDecodeStreamA (thunk 0x10002509) on [0x100b49f0] when the wave device
// [0x100b4a20] is set, else the value in the argument's slot is the answer (this+0xc, or the cue count when `cue ` was read:
// 0x10032b55); an answer of 0 or 0x23 closes the file (c3fg_closeOpenSound), sets +0x58 bit 0 for 0, and returns it
// (0x10032bf9). +0x5c bit 5 (0x10032c05) does the same through beginDecodeStreamB (thunk 0x10001b7c), without the bit-0 store.
// Buffer: 0x20000 bytes, or the data size with +0x58 bit 0 set when bit 0 was already set or the size is below 0x20000
// (0x10032cd3-0x10032ce7); with bit 0 still clear, +0x1374 = 0x1ffff (0x10032cf0). mmioSetBuffer(+0, NULL, size, 0),
// mmioGetInfo(+0, +0xc, 0), mmioAdvance(+0, +0xc, 0) (0x10032d03-0x10032d26). When (+0x168 - +0x164) * blockAlign fits the
// MMIOINFO's cchBuffer +0x20 (`ja` 0x10032d45, unsigned), the sound plays from that buffer: mmioAdvance once more, +0x58 |= 1,
// +8 = this+0xc, the MMIOINFO's pchEndRead +0x2c = pchBuffer +0x24 + that byte count, and 16 voice records (+0x2c4 - 0x8c,
// stride 0x110, 0x10032d8e-0x10032e7b) each get a copy of the MMIOINFO, the 0x12-byte format +0x180, the frame range
// (+0x164 / +0x168), bit 2 set and bit 1 cleared in their byte +0x10, and the play window (+0x4c start, +0x48 end, +0x50
// length) from the first loop record or the whole buffer; +0x228 = 0, return 0. Otherwise (streaming, 0x10032e97): a 0x48-byte
// MMIOINFO is allocated to +8, zeroed, given cchBuffer = the buffer size, fccIOProc 'MEM ', adwInfo[0] = 0 and a _malloc'd
// buffer (0x10032ec7), and opened with mmioOpenA(NULL, it, MMIO_CREATE | MMIO_READWRITE 0x1002) into +4 (0x10032ee0); NULL ->
// Channel_releaseBuffers and 8. Else g_openMmioCount is raised, mmioGetInfo(+4, +8, 0), listAppendNodeB(+8), +0x1378 = its
// buffer; with a loop table +0x58 |= 4 and +0x2d0 |= 1 (the 16-iteration loop at 0x10032f56 ORs the same dword);
// fillDecodeRing (thunk 0x10002022), mmioGetInfo(+0, +0xc, 0), mmioAdvance(+0, +0xc, 0) (0x10032f78 / 0x10032f81). With +0x5c bit
// 6 set and bit 7 clear (0x10032f8c / 0x10032f90): no DirectSound device [0x100b49f0] clears bit 6 (0x10032ff3); else
// DirectSoundDevice::createBufferFmt (thunk 0x100011e5) on it with (+0x54, +0x180, 0x100e0, 0x10000) (0x10032fb1); non-zero
// clears bit 6 (0x10032ff0); zero calls slot +0x40 of [0x100b4a20] with 8 and DsBuffer::setRegionMs (thunk 0x1000182f) on
// [+0x54] with its result, and sets +0x58 bit 28 (0x10032fd3). Every streaming exit after the open stores +0x228 = 0, returns 0.
typedef unsigned(__fastcall* Parse_t)(void*, void*, const char*);
Parse_t Parse_orig;
unsigned __fastcall Parse_re(void* self, void*, const char* name) {
    Strstr_t strstr_ = c3fg_fn<Strstr_t>(0x00042690);
    if (strstr_(name, c3fg_str(kStrMp3))) return 0xb;                               // 0x100326b3 / 0x1003300e
    if (strstr_(name, c3fg_str(kStrMP3))) return 0xb;                               // 0x100326c9
    MmCk riff, data, fmt, cue, smpl;
    memset(&riff, 0, sizeof riff); memset(&data, 0, sizeof data); memset(&fmt, 0, sizeof fmt);
    memset(&cue, 0, sizeof cue); memset(&smpl, 0, sizeof smpl);
    if (fat<unsigned char>(self, 0x58) & 2) {                                       // 0x1003272a
        unsigned r = c3fg_fn<Th0_t>(0x00001d11)(self, 0);                           // 0x10032738
        if (r) return r;                                                            // 0x1003273f
    }
    fat<void*>(self, 0) = 0;                                                        // 0x1003274f
    memset(&fat<char>(self, 0xc), 0, 0x48);                                         // 0x10032759
    memset(&fat<char>(self, 0x194), 0, 0x2c);                                       // 0x10032766
    for (int off = 0x164; off <= 0x174; off += 4) fat<unsigned>(self, off) = 0;    // 0x1003276d-0x10032785
    fat<unsigned>(self, 0x20) = 0x20000;                                            // 0x1003278b
    fat<unsigned>(self, 0x10) = 0x204d454d;                                         // 0x10032792 'MEM '
    strcpy(&fat<char>(self, 0x64), name);                                           // 0x1003279a-0x100327b2
    void* h = c3fg_mmioOpen(&fat<char>(self, 0x64), 0, 0);                          // 0x100327b4
    fat<void*>(self, 0) = h;                                                        // 0x100327bc
    if (!h) {                                                                       // 0x100327bf
        memset(&fat<char>(self, 0xc), 0, 0x48);                                     // 0x100327cc
        return 8;                                                                   // 0x100327d1
    }
    ++c3fg_g<unsigned>(kOpenMmioCount);                                             // 0x100327dd-0x100327e8
    fat<unsigned>(self, 0x58) |= 2;                                                 // 0x100327f1
    c3fg_fn<Th1_t>(0x00001bb8)(c3fg_sound(kOpenSoundList), 0, c3fg_u(&fat<char>(self, 0xc)));   // 0x100327fc
    riff.ckid = c3fg_fourcc(c3fg_str(kStrRiff), 0);                                 // 0x1003280e
    riff.fccType = c3fg_fourcc(c3fg_str(kStrWave), 0);                              // 0x1003281b
    if (c3fg_mmioDescend(fat<void*>(self, 0), &riff, 0, 0x20)) {                    // 0x10032834
        c3fg_fn<Th0_t>(0x00001aaf)(self, 0);                                        // 0x1003283c
        return 0x12;
    }
    fmt.ckid = c3fg_fourcc(c3fg_str(kStrFmt), 0);                                   // 0x10032857
    if (c3fg_mmioDescend(fat<void*>(self, 0), &fmt, &riff, 0x10)) {                 // 0x1003286d
        c3fg_fn<Th0_t>(0x00001aaf)(self, 0);                                        // 0x10032875
        return 0x12;
    }
    c3fg_mmioRead(fat<void*>(self, 0), &fat<char>(self, 0x180), 0x10);              // 0x1003289c
    fat<unsigned short>(self, 0x190) = 0;                                           // 0x100328a9
    c3fg_mmioAscend(fat<void*>(self, 0), &fmt, 0);                                  // 0x100328b2
    smpl.ckid = c3fg_fourcc(c3fg_str(kStrSmpl), 0);                                 // 0x100328bf
    if (c3fg_mmioDescend(fat<void*>(self, 0), &smpl, &riff, 0x10) == 0) {          // 0x100328d5 / 0x100328d9
        c3fg_mmioRead(fat<void*>(self, 0), &fat<char>(self, 0x194), 0x24);          // 0x100328e8
        if (fat<unsigned>(self, 0x1b0)) {                                           // 0x100328f2
            void* t = c3fg_new(fat<unsigned>(self, 0x1b0) * 0x18);                  // 0x100328fb
            fat<void*>(self, 0x1b8) = t;                                            // 0x10032909
            c3fg_mmioRead(fat<void*>(self, 0), t, fat<unsigned>(self, 0x1b0) * 0x18);   // 0x1003291b
            const unsigned* lp = fat<const unsigned*>(self, 0x1b8);
            unsigned ba = fat<unsigned short>(self, 0x18c);                         // 0x10032925
            fat<unsigned>(self, 0x1368) = (lp[3] - lp[2] + 1) * ba;                 // 0x1003292c-0x10032936
            fat<unsigned>(self, 0x136c) = ba * lp[2];                               // 0x1003293c / 0x10032940
        }
        c3fg_mmioAscend(fat<void*>(self, 0), &smpl, 0);                             // 0x10032951
    } else {
        fat<unsigned>(self, 0x1a0) = 0x3c;                                          // 0x10032959
    }
    riff.ckid = c3fg_fourcc(c3fg_str(kStrRiff), 0);                                 // 0x1003296a
    c3fg_mmioSeek(fat<void*>(self, 0), 0, 0);                                       // 0x10032978
    riff.fccType = c3fg_fourcc(c3fg_str(kStrWave), 0);                              // 0x10032985
    if (c3fg_mmioDescend(fat<void*>(self, 0), &riff, 0, 0x20)) {                    // 0x10032998
        c3fg_fn<Th0_t>(0x00001aaf)(self, 0);                                        // 0x100329a0
        return 0x12;
    }
    cue.ckid = c3fg_fourcc(c3fg_str(kStrCue), 0);                                   // 0x100329bb
    unsigned slot = c3fg_u(&fat<char>(self, 0xc));                                  // the argument's slot, 0x10032752
    if (c3fg_mmioDescend(fat<void*>(self, 0), &cue, &riff, 0x10) == 0) {           // 0x100329d1 / 0x100329d5
        slot = 0;                                                                   // 0x100329d7
        c3fg_mmioRead(fat<void*>(self, 0), &slot, 4);                               // 0x100329e6
        unsigned i = 0;                                                             // 0x100329ec
        if (slot) {                                                                 // 0x100329f6 (jbe)
            do {
                void* rec = c3fg_new(0x18);                                         // 0x100329fa
                c3fg_mmioRead(fat<void*>(self, 0), rec, 0x18);                      // 0x10032a0b
                c3fg_fn<Th1_t>(0x000012bc)(&fat<char>(self, 0x1348), 0, c3fg_u(rec));   // 0x10032a18
            } while (++i < slot);                                                   // 0x10032a25-0x10032a2c (jb)
        }
        c3fg_mmioAscend(fat<void*>(self, 0), &cue, 0);                              // 0x10032a39
    }
    c3fg_mmioAscend(fat<void*>(self, 0), &riff, 0);                                 // 0x10032a4b
    riff.ckid = c3fg_fourcc(c3fg_str(kStrRiff), 0);                                 // 0x10032a57
    c3fg_mmioSeek(fat<void*>(self, 0), 0, 0);                                       // 0x10032a63
    riff.fccType = c3fg_fourcc(c3fg_str(kStrWave), 0);                              // 0x10032a6f
    if (c3fg_mmioDescend(fat<void*>(self, 0), &riff, 0, 0x20)) {                    // 0x10032a81
        c3fg_fn<Th0_t>(0x00001aaf)(self, 0);                                        // 0x10032a89
        return 0x12;
    }
    data.ckid = c3fg_fourcc(c3fg_str(kStrData), 0);                                 // 0x10032aa3
    if (c3fg_mmioDescend(fat<void*>(self, 0), &data, &riff, 0x10)) {                // 0x10032ab9
        c3fg_fn<Th0_t>(0x00001aaf)(self, 0);                                        // 0x10032ac1
        return 0x12;
    }
    fat<unsigned>(self, 0x174) = data.dwDataOffset;                                 // 0x10032adf
    unsigned cks = data.cksize;                                                     // 0x10032ad9
    fat<unsigned>(self, 0x178) = cks;                                               // 0x10032af0
    unsigned frames = cks / fat<unsigned short>(self, 0x18c);                       // 0x10032af6
    fat<unsigned>(self, 0x168) = frames;                                            // 0x10032afa
    fat<unsigned>(self, 0x139c) = (frames * 1000u) / fat<unsigned>(self, 0x184);    // 0x10032b00-0x10032b12
    if (!fat<void*>(self, 0x1b8)) {                                                 // 0x10032b20
        fat<unsigned>(self, 0x1368) = cks;                                          // 0x10032b22
        fat<unsigned>(self, 0x136c) = 0;                                            // 0x10032b28
    }
    if (fat<unsigned>(self, 0x58) & 0x20000000) {                                   // 0x10032b2e
        unsigned r = c3fg_g<void*>(kWaveDevice)                                     // 0x10032b3b
            ? c3fg_fn<Th1_t>(0x00002509)(self, 0, c3fg_g<unsigned>(kDsDevice))      // 0x10032b4c
            : slot;                                                                 // 0x10032b55
        if (r == 0 || r == 0x23) {                                                  // 0x10032b5b / 0x10032b60
            c3fg_closeOpenSound(self);
            if (r == 0) fat<unsigned>(self, 0x58) |= 1;                             // 0x10032bef-0x10032bf6
            return r;                                                               // 0x10032bf9
        }
    }
    if (fat<unsigned char>(self, 0x5c) & 0x20) {                                    // 0x10032c05
        unsigned r = c3fg_g<void*>(kWaveDevice)                                     // 0x10032c0f
            ? c3fg_fn<Th1_t>(0x00001b7c)(self, 0, c3fg_g<unsigned>(kDsDevice))      // 0x10032c20
            : slot;                                                                 // 0x10032c29
        if (r == 0 || r == 0x23) {                                                  // 0x10032c2f / 0x10032c34
            c3fg_closeOpenSound(self);
            return r;                                                               // 0x10032cbf
        }
    }
    unsigned fl = fat<unsigned>(self, 0x58);                                        // 0x10032ccb
    unsigned bsz = 0x20000;                                                         // 0x10032cce
    if ((fl & 1) || fat<unsigned>(self, 0x178) < 0x20000) {                         // 0x10032cd5 / 0x10032cdd
        bsz = fat<unsigned>(self, 0x178);                                           // 0x10032cdf
        fat<unsigned>(self, 0x58) = fl | 1;                                         // 0x10032ce7
    }
    if (!(fat<unsigned char>(self, 0x58) & 1)) fat<unsigned>(self, 0x1374) = 0x1ffff;   // 0x10032cee / 0x10032cf0
    c3fg_mmioSetBuffer(fat<void*>(self, 0), 0, bsz, 0);                             // 0x10032d03
    c3fg_mmioGetInfo(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);                // 0x10032d13
    c3fg_mmioAdvance(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);                // 0x10032d26
    unsigned ba = fat<unsigned short>(self, 0x18c);
    if ((fat<unsigned>(self, 0x168) - fat<unsigned>(self, 0x164)) * ba <= fat<unsigned>(self, 0x20)) {   // 0x10032d45 (ja)
        c3fg_mmioAdvance(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);            // 0x10032d53
        fat<unsigned>(self, 0x58) |= 1;                                             // 0x10032d5e / 0x10032d6a
        fat<void*>(self, 8) = &fat<char>(self, 0xc);                                // 0x10032d73
        fat<unsigned>(self, 0x2c) = (fat<unsigned>(self, 0x168) - fat<unsigned>(self, 0x164)) * fat<unsigned short>(self, 0x18c)
                                    + fat<unsigned>(self, 0x24);                    // 0x10032d6d-0x10032d86
        char* rec = &fat<char>(self, 0x2c4);                                        // 0x10032d80
        for (int k = 0; k < 16; k++, rec += 0x110) {                                // 0x10032d89 / 0x10032e75-0x10032e7b
            char* mi = fat<char*>(self, 8);                                         // 0x10032d8e
            if (mi) memcpy(rec - 0x8c, mi, 0x48);                                   // 0x10032d91-0x10032da0
            fat<unsigned>(rec, -4) = 0;                                             // 0x10032daa
            fat<unsigned>(rec, 0) = 0;                                              // 0x10032dad
            fat<unsigned>(rec, 4) = 0;                                              // 0x10032daf
            memcpy(rec + 0x14, &fat<char>(self, 0x180), 0x10);                      // 0x10032db2-0x10032dc9
            fat<unsigned short>(rec, 0x24) = fat<unsigned short>(self, 0x190);      // 0x10032dcc / 0x10032dd0
            fat<unsigned>(rec, 4) = fat<unsigned>(self, 0x168);                     // 0x10032dda
            fat<unsigned>(rec, 0) = fat<unsigned>(self, 0x164);                     // 0x10032de3
            fat<unsigned>(rec, -4) = 0;                                             // 0x10032de5
            rec[0x10] = static_cast<char>((rec[0x10] & 0xfd) | 4);                  // 0x10032de8-0x10032df1
            const unsigned* lp = fat<const unsigned*>(self, 0x1b8);                 // 0x10032df4
            unsigned b = fat<unsigned short>(self, 0x18c);
            unsigned len;
            if (lp) {                                                               // 0x10032dfc
                fat<unsigned>(rec, 0x4c) = lp[2] * b + fat<unsigned>(fat<char*>(self, 8), 0x18);         // 0x10032dfe-0x10032e17
                fat<unsigned>(rec, 0x48) = (lp[3] + 1) * b + fat<unsigned>(fat<char*>(self, 8), 0x18);   // 0x10032e1a-0x10032e34
                len = (lp[3] - lp[2] + 1) * b;                                      // 0x10032e37-0x10032e4f
            } else {
                char* m = fat<char*>(self, 8);
                fat<unsigned>(rec, 0x4c) = fat<unsigned>(m, 0x18);                  // 0x10032e5a
                fat<unsigned>(rec, 0x48) = fat<unsigned>(m, 0x20);                  // 0x10032e63
                len = fat<unsigned>(m, 0x20) - fat<unsigned>(m, 0x18) + 1;          // 0x10032e69-0x10032e71
            }
            fat<unsigned>(rec, 0x50) = len;                                         // 0x10032e72
        }
        fat<unsigned>(self, 0x228) = 0;                                             // 0x10032e82
        return 0;
    }
    char* mi = static_cast<char*>(c3fg_new(0x48));                                  // 0x10032e99
    fat<void*>(self, 8) = mi;                                                       // 0x10032ea7
    memset(mi, 0, 0x48);                                                            // 0x10032eaa
    fat<unsigned>(fat<char*>(self, 8), 0x14) = bsz;                                 // 0x10032eb0
    fat<unsigned>(fat<char*>(self, 8), 4) = 0x204d454d;                             // 0x10032eb6 'MEM '
    fat<unsigned>(fat<char*>(self, 8), 0x30) = 0;                                   // 0x10032ec0
    void* buf = c3fg_malloc(bsz);                                                   // 0x10032ec7
    fat<void*>(fat<char*>(self, 8), 0x18) = buf;                                    // 0x10032ed2
    void* mh = c3fg_mmioOpen(0, fat<char*>(self, 8), 0x1002);                       // 0x10032ee0
    fat<void*>(self, 4) = mh;                                                       // 0x10032ee8
    if (!mh) {                                                                      // 0x10032eeb
        c3fg_fn<Th0_t>(0x00001d11)(self, 0);                                        // 0x10032eef
        return 8;                                                                   // 0x10032ef7
    }
    ++c3fg_g<unsigned>(kOpenMmioCount);                                             // 0x10032f03-0x10032f11
    c3fg_mmioGetInfo(fat<void*>(self, 4), fat<char*>(self, 8), 0);                  // 0x10032f1e
    c3fg_fn<Th1_t>(0x00001bb8)(c3fg_sound(kOpenSoundList), 0, c3fg_u(fat<char*>(self, 8)));   // 0x10032f29
    fat<unsigned>(self, 0x1378) = fat<unsigned>(fat<char*>(self, 8), 0x18);         // 0x10032f39 / 0x10032f3c
    if (fat<void*>(self, 0x1b8)) {                                                  // 0x10032f42
        fat<unsigned>(self, 0x58) |= 4;                                             // 0x10032f4c
        fat<unsigned>(self, 0x2d0) |= 1;                                            // 0x10032f56-0x10032f65
    }
    c3fg_fn<Th0_t>(0x00002022)(self, 0);                                            // 0x10032f69
    c3fg_mmioGetInfo(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);                // 0x10032f78
    c3fg_mmioAdvance(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);                // 0x10032f81
    unsigned f5c = fat<unsigned>(self, 0x5c);                                       // 0x10032f87
    if ((f5c & 0x40) && !(f5c & 0x80)) {                                            // 0x10032f8c / 0x10032f90
        void* dev = c3fg_g<void*>(kDsDevice);                                       // 0x10032f92
        if (dev) {                                                                  // 0x10032f9a
            if (c3fg_fn<Th4_t>(0x000011e5)(dev, 0, c3fg_u(&fat<char>(self, 0x54)), c3fg_u(&fat<char>(self, 0x180)),
                                           0x100e0, 0x10000) == 0) {                // 0x10032fb1 / 0x10032fb8
                void* wd = c3fg_g<void*>(kWaveDevice);                              // 0x10032fba
                unsigned v = c3fg_vfn<Th1_t>(wd, 0x40)(wd, 0, 8);                   // 0x10032fc4
                c3fg_fn<Th1_t>(0x0000182f)(fat<void*>(self, 0x54), 0, v);           // 0x10032fca
                fat<unsigned>(self, 0x58) |= 0x10000000;                            // 0x10032fd3
                fat<unsigned>(self, 0x228) = 0;                                     // 0x10032fd8
                return 0;
            }
            f5c = fat<unsigned>(self, 0x5c);                                        // 0x10032ff0
        }
        fat<unsigned>(self, 0x5c) = f5c & ~0x40u;                                   // 0x10032ff3 / 0x10032ff5
    }
    fat<unsigned>(self, 0x228) = 0;                                                 // 0x10032ff9
    return 0;
}

// The decode-stream start shared by beginDecodeStreamA 0x32130 and beginDecodeStreamB 0x32290 from the createBufferFmt call on:
// DirectSoundDevice::createBufferFmt (thunk 0x100011e5) on the device with (+0x54, the format +0x180, codec, size +0x178);
// non-zero -> 0x11. +0x58 |= 0x10000000. DsBuffer::restore (thunk 0x1000213f) on the buffer [+0x54] with (0, size, &ptr1,
// &bytes1, &ptr2, &bytes2) (A: 0x10032171-0x10032195); a non-zero result or bytes1 != size ends with +0x220 = 0x5622 and 0
// (A: 0x1003219c / 0x100321aa). mmioRead(+0, ptr1, bytes1) (A: 0x100321b5); a short read unlocks (DsBuffer::unlock, thunk
// 0x10001ec9, with (ptr1, bytes1, ptr2, bytes2)), removes the buffer from the device (DirectSoundDevice::removeBuffer, thunk
// 0x10001677) and returns 0x23 without the +0x220 store; a full read unlocks the same way, stores this at the buffer's +0x20
// (A: 0x1003220b), then +0x220 = 0x5622 and 0.
unsigned c3fg_beginDecode(void* self, void* dev, unsigned codec) {
    if (c3fg_fn<Th4_t>(0x000011e5)(dev, 0, c3fg_u(&fat<char>(self, 0x54)), c3fg_u(&fat<char>(self, 0x180)), codec,
                                   fat<unsigned>(self, 0x178)))                     // A 0x1003215d / B 0x100322d1
        return 0x11;                                                                // A 0x10032223 / B 0x10032397
    fat<unsigned>(self, 0x58) |= 0x10000000;                                        // A 0x10032171 / B 0x100322e5
    void* ptr1 = 0; void* ptr2 = 0; unsigned bytes1 = 0, bytes2 = 0;                // [arg slot], [esp+0xc], [esp+0x14], [esp+0x10]
    if (c3fg_fn<Th6_t>(0x0000213f)(fat<void*>(self, 0x54), 0, 0, fat<unsigned>(self, 0x178), c3fg_u(&ptr1), c3fg_u(&bytes1),
                                   c3fg_u(&ptr2), c3fg_u(&bytes2)) == 0             // A 0x10032195 / B 0x10032309
        && bytes1 == fat<unsigned>(self, 0x178)) {                                  // A 0x100321aa / B 0x1003231e
        long n = c3fg_mmioRead(fat<void*>(self, 0), ptr1, static_cast<long>(bytes1));   // A 0x100321b5 / B 0x10032329
        if (static_cast<unsigned>(n) != bytes1) {                                   // A 0x100321c1 / B 0x10032335
            c3fg_fn<Th4_t>(0x00001ec9)(fat<void*>(self, 0x54), 0, c3fg_u(ptr1), bytes1, c3fg_u(ptr2), bytes2);   // A 0x100321d5
            c3fg_fn<Th1_t>(0x00001677)(dev, 0, c3fg_u(fat<void*>(self, 0x54)));     // A 0x100321df / B 0x10032353
            return 0x23;
        }
        c3fg_fn<Th4_t>(0x00001ec9)(fat<void*>(self, 0x54), 0, c3fg_u(ptr1), bytes1, c3fg_u(ptr2), bytes2);       // A 0x10032204
        fat<void*>(fat<void*>(self, 0x54), 0x20) = self;                            // A 0x1003220b / B 0x1003237f
    }
    fat<unsigned>(self, 0x220) = 0x5622;                                            // A 0x1003220e / B 0x10032382
    return 0;
}

// 0x00032130  beginDecodeStreamA (VA 0x10032130). __thiscall, one stack argument, the DirectSound device (`ret 4`; [esp+0x14]
// at 0x10032134 after one push). A NULL device stores +0x220 = 0x5622 and returns 0 (0x1003213e -> 0x1003220e); otherwise
// c3fg_beginDecode with the codec id 0x100e0 (0x10032154).
typedef unsigned(__fastcall* Decode_t)(void*, void*, void*);
Decode_t DecA_orig;
unsigned __fastcall DecA_re(void* self, void*, void* dev) {
    if (!dev) {                                                                     // 0x1003213e
        fat<unsigned>(self, 0x220) = 0x5622;                                        // 0x1003220e
        return 0;
    }
    return c3fg_beginDecode(self, dev, 0x100e0);                                    // 0x10032154
}

// 0x00032290  beginDecodeStreamB (VA 0x10032290). __thiscall, one stack argument, the device (`ret 4`). A NULL device returns
// 1 (0x1003229e -> 0x100323a7); a device whose byte +0x6c has bit 3 clear returns 0xb (0x100322a4 / 0x100322ac); otherwise
// c3fg_beginDecode with the codec id 0x100f4 (0x100322c8).
Decode_t DecB_orig;
unsigned __fastcall DecB_re(void* self, void*, void* dev) {
    if (!dev) return 1;                                                             // 0x1003229e / 0x100323a7
    if (!(fat<unsigned char>(dev, 0x6c) & 8)) return 0xb;                           // 0x100322a4 / 0x100322ac
    return c3fg_beginDecode(self, dev, 0x100f4);                                    // 0x100322c8
}

// 0x00005770  processWaveChunks (VA 0x10005770). __thiscall, no stack argument (plain `ret` 0x10005833). Walks the chunk list
// whose first node is +0xa4c (nodes {prev +0, next +4, record +8}) with the cursor +0xa54 (0x1000577e), stopping at a NULL
// node or record (0x10005784 / 0x1000578f / 0x10005815-0x10005829). A running object pointer (ebx) starts as this: the
// `push ecx` at 0x10005770 makes the stack slot read at 0x10005795 hold ecx. Per record: type +0x20 == 0 (0x1000579e) asks the
// object's virtual +0x118 for (0, record +0x24) (0x100057fd) and that result becomes the object; type 3 (0x100057a0) moves the
// cursor to the previous node and back to its next one (0x100057a5-0x100057cf; with no previous node the cursor becomes NULL)
// and looks at the previous node's record: its byte +0x18 zero skips (0x100057da); its +0x1c == 3 makes the object that record
// - 0x2c (0x100057e6); a NULL object skips (0x100057eb); other types skip (0x100057a3). The record is handed to
// Ctrl_appendIfAccepted (thunk 0x10001654) on the object (0x10005808), even when the virtual answered NULL. eax at the `ret` is
// the last cursor value read (NULL, or the head node when its record is NULL).
typedef unsigned(__fastcall* ProcChunks_t)(void*, void*);
ProcChunks_t ProcChunks_orig;
unsigned __fastcall ProcChunks_re(void* self, void*) {
    ListNode* cur = fat<ListNode*>(self, 0xa4c);                                    // 0x10005776
    fat<ListNode*>(self, 0xa54) = cur;                                              // 0x1000577e
    if (!cur) return 0;                                                             // 0x10005784
    char* rec = static_cast<char*>(cur->value);                                     // 0x1000578a
    if (!rec) return c3fg_u(cur);                                                   // 0x1000578f
    void* obj = self;                                                               // 0x10005795 (the slot pushed at 0x10005770)
    for (;;) {
        unsigned t = fat<unsigned>(rec, 0x20);                                      // 0x10005799
        bool append = false;
        if (t == 0) {                                                               // 0x1000579e
            obj = reinterpret_cast<void*>(c3fg_vfn<Th2_t>(self, 0x118)(self, 0, 0, fat<unsigned>(rec, 0x24)));   // 0x100057fd
            append = true;
        } else if (t == 3) {                                                        // 0x100057a3
            char* prevRec = 0;
            ListNode* c = fat<ListNode*>(self, 0xa54);                              // 0x100057a5
            if (c) {
                c = c->prev;                                                        // 0x100057af
                fat<ListNode*>(self, 0xa54) = c;                                    // 0x100057b3
                if (c) prevRec = static_cast<char*>(c->value);                      // 0x100057bb
            }
            c = fat<ListNode*>(self, 0xa54);                                        // 0x100057c2
            if (c) fat<ListNode*>(self, 0xa54) = c->next;                           // 0x100057cc / 0x100057cf
            if (fat<unsigned char>(prevRec, 0x18)) {                                // 0x100057d5 / 0x100057da
                if (fat<unsigned>(prevRec, 0x1c) == 3)                              // 0x100057dc
                    obj = prevRec ? prevRec - 0x2c : 0;                             // 0x100057e2-0x100057ef
                if (obj) append = true;                                             // 0x100057e9 / 0x100057eb
            }
        }
        if (append) c3fg_fn<Th1_t>(0x00001654)(obj, 0, c3fg_u(rec));               // 0x10005808
        ListNode* c = fat<ListNode*>(self, 0xa54);                                  // 0x1000580d
        if (!c) return 0;                                                           // 0x10005815
        c = c->next;                                                                // 0x10005817
        fat<ListNode*>(self, 0xa54) = c;                                            // 0x1000581c
        if (!c) return 0;                                                           // 0x10005822
        rec = static_cast<char*>(c->value);                                         // 0x10005824
        if (!rec) return c3fg_u(c);                                                 // 0x10005829
    }
}

// 0x000399a0  loadMultiChannel (VA 0x100399a0). __thiscall, one stack argument, the file name (`ret 4`; [esp+0x18] at 0x100399b8
// after the three SEH pushes and `sub esp, 8`), virtual slot 4 of the MultiSound vtable 0x1005c7e8 (slot 0x1005c7f8 holds the
// thunk 0x100024a0 -> 0x100399a0). parseRiffWave (thunk 0x10001e51) parses the file into the channel +0x70 (0x100399ca); its
// non-zero result is returned (0x100399d3). With +0x1438 equal to 2 (signed: `jle` 1 at 0x100399e2, `jge` 3 at 0x100399eb) the
// sub-channels 1 .. +0x1438 - 1 are built into the slots +0x1440, +0x1444 ... (0x100399fe, `add ebx, 4` 0x10039aaf), +0x1438
// re-read every round (0x10039ab2): operator new 0x1450 (0x10039a09), SubChannel_ctor (thunk 0x10001465), the channel
// constructor on +0x70 (thunk 0x10001005), the MultiSound vtable at +0, +0x1424 / +0x1428 / +0x1420 / +0x142c / +0x143c /
// +0x1440..+0x1448 / +0x1430 / +0x1434 = 0 and +0x1438 = 1 (0x10039a3b-0x10039a81), Sound_setSourceType(1) (thunk
// 0x100018a2); then the sub-channel's own slot 4 with the name (0x10039aa1); a non-zero answer is returned before the slot is
// written (0x10039aa6), else the slot gets it (0x10039aac). Returns the last result (0). A failed operator new leaves the
// object NULL and faults at the slot-4 load (0x10039a94); the EH frame only unwinds a throwing constructor.
typedef unsigned(__fastcall* LoadMulti_t)(void*, void*, const char*);
LoadMulti_t LoadMulti_orig;
unsigned __fastcall LoadMulti_re(void* self, void*, const char* name) {
    unsigned r = c3fg_fn<Th1_t>(0x00001e51)(&fat<char>(self, 0x70), 0, c3fg_u(name));   // 0x100399ca
    if (r) return r;                                                                // 0x100399d3
    int n = fat<int>(self, 0x1438);                                                 // 0x100399d9
    if (n <= 1 || n >= 3) return 0;                                                 // 0x100399e2 / 0x100399eb
    void** slot = &fat<void*>(self, 0x1440);                                        // 0x100399fe
    for (int i = 1; i < fat<int>(self, 0x1438); i++, slot++) {                      // 0x10039aae-0x10039ab8
        char* p = static_cast<char*>(c3fg_new(0x1450));                             // 0x10039a09
        if (p) {                                                                    // 0x10039a1d
            c3fg_fn<Th0_t>(0x00001465)(p, 0);                                       // 0x10039a21
            c3fg_fn<Th0_t>(0x00001005)(p + 0x70, 0);                                // 0x10039a2e
            fat<void*>(p, 0) = c3fg_sound(0x0005c7e8);                              // 0x10039a3b
            fat<unsigned>(p, 0x1424) = 0;                                           // 0x10039a43
            fat<unsigned>(p, 0x1428) = 0;                                           // 0x10039a4c
            fat<unsigned>(p, 0x1420) = 0;                                           // 0x10039a55
            fat<unsigned>(p, 0x142c) = 0;                                           // 0x10039a5b
            fat<unsigned>(p, 0x1438) = 1;                                           // 0x10039a61
            fat<unsigned>(p, 0x143c) = 0;                                           // 0x10039a6b
            fat<unsigned>(p, 0x1440) = 0;                                           // 0x10039a71
            fat<unsigned>(p, 0x1444) = 0;                                           // 0x10039a73
            fat<unsigned>(p, 0x1448) = 0;                                           // 0x10039a76
            fat<unsigned>(p, 0x1430) = 0;                                           // 0x10039a7b
            fat<unsigned>(p, 0x1434) = 0;                                           // 0x10039a81
            c3fg_fn<Th1_t>(0x000018a2)(p, 0, 1);                                    // 0x10039a87
        }
        r = c3fg_vfn<Th1_t>(p, 0x10)(p, 0, c3fg_u(name));                           // 0x10039aa1
        if (r) return r;                                                            // 0x10039aa6
        *slot = p;                                                                  // 0x10039aac
    }
    return r;
}

// 0x00005440  Wave_Device::readFmtChunk (VA 0x10005440). __thiscall, no stack argument (plain `ret` 0x100055f7). A CtrlObject is
// built on the stack (CtrlObject::ctor, thunk 0x10001ffa, 0x10005461; it writes +4 'ctrl', +0xc 0x18, +0x10 0x44444444, +0x14
// / +0x20 / +0x24 = -1, the word +0x18 = 0) and its vtable set to 0x1005b178 (0x10005466). Through mmioRead on the HMMIO +0xa48
// (0x10005474): a dword tag, a dword size n, then n bytes into the CtrlObject's +0x10 (0x10005488-0x100054ab). The frame holds
// 0x18 bytes there: the CtrlObject's +0x28 is the SEH record's link that 0x100055e6 / 0x100055ed reload into fs:[0], so an n
// above 0x18 replaces the thread's exception chain head with file bytes at the return. When the dword at +0x1c (the read's +0xc) is 3 (0x100054ad): operator new 0xe4
// (0x100054be), Ctrl5b5c0_ctor (thunk 0x100025b3), then three (size, bytes) pairs read into the new object's +0x20, +8 and +0x58
// (0x100054fa-0x10005550); its embedded record +0x2c gets the CtrlObject's dwords +4..+0x24 (the word at +0x18) at the same
// offsets (0x10005552-0x10005592): +4 'ctrl', +8 a stack dword nothing writes (the constructor skips it, the read starts at
// +0x10), +0xc 0x18, then the bytes read (or the constructor's values where the read was short); a 0xc-byte node {0, 0, record}
// (operator new 0x10005595) is appended to the list +0xa4c (head) / +0xa50 (tail)
// and +0xa58 counts it (0x100055b6-0x100055e0). eax at the `ret`: the new count, or the third mmioRead's result when the type
// is not 3. A failed operator new faults (NULL + 0x20 handed to mmioRead, or the node store at 0x100055c9).
typedef unsigned(__fastcall* ReadFmt_t)(void*, void*);
ReadFmt_t ReadFmt_orig;
unsigned __fastcall ReadFmt_re(void* self, void*) {
    unsigned char ctrl[0x30];                                                       // [esp+0x18]: the CtrlObject, its +0x10 the read
    c3fg_fn<Th0_t>(0x00001ffa)(ctrl, 0);                                            // 0x10005461
    fat<void*>(ctrl, 0) = c3fg_sound(0x0005b178);                                   // 0x10005466
    unsigned tag = 0, n = 0;                                                        // [esp+0x10], [esp+0xc]
    c3fg_mmioRead(fat<void*>(self, 0xa48), &tag, 4);                                // 0x10005488
    c3fg_mmioRead(fat<void*>(self, 0xa48), &n, 4);                                  // 0x10005498
    long r = c3fg_mmioRead(fat<void*>(self, 0xa48), ctrl + 0x10, static_cast<long>(n));   // 0x100054ab
    if (fat<unsigned>(ctrl, 0x1c) != 3) return static_cast<unsigned>(r);           // 0x100054ad / 0x100054b2
    char* obj = static_cast<char*>(c3fg_new(0xe4));                                 // 0x100054be
    if (obj) obj = reinterpret_cast<char*>(c3fg_fn<Th0_t>(0x000025b3)(obj, 0));    // 0x100054d5
    char* rec = obj ? obj + 0x2c : 0;                                               // 0x100054e9
    c3fg_mmioRead(fat<void*>(self, 0xa48), &tag, 4);                                // 0x100054fa
    c3fg_mmioRead(fat<void*>(self, 0xa48), obj + 0x20, static_cast<long>(tag));     // 0x1000550c
    c3fg_mmioRead(fat<void*>(self, 0xa48), &tag, 4);                                // 0x1000551c
    c3fg_mmioRead(fat<void*>(self, 0xa48), obj + 8, static_cast<long>(tag));        // 0x1000552e
    c3fg_mmioRead(fat<void*>(self, 0xa48), &tag, 4);                                // 0x1000553e
    c3fg_mmioRead(fat<void*>(self, 0xa48), obj + 0x58, static_cast<long>(tag));     // 0x10005550
    // rec+N = the CtrlObject's +N: [esp+0x20] is read before the `push 0xc` at 0x10005556, the rest after it
    fat<unsigned>(rec, 4) = fat<unsigned>(ctrl, 4);                                 // 0x10005552 / 0x10005558
    fat<unsigned>(rec, 8) = fat<unsigned>(ctrl, 8);                                 // 0x1000555b / 0x1000555f (not written by the ctor)
    fat<unsigned>(rec, 0xc) = fat<unsigned>(ctrl, 0xc);                             // 0x10005562 / 0x10005566
    fat<unsigned>(rec, 0x10) = fat<unsigned>(ctrl, 0x10);                           // 0x10005569 / 0x1000556d
    fat<unsigned>(rec, 0x14) = fat<unsigned>(ctrl, 0x14);                           // 0x10005570 / 0x10005574
    fat<unsigned short>(rec, 0x18) = fat<unsigned short>(ctrl, 0x18);               // 0x10005577 / 0x1000557c
    fat<unsigned>(rec, 0x1c) = fat<unsigned>(ctrl, 0x1c);                           // 0x10005580 / 0x10005584
    fat<unsigned>(rec, 0x20) = fat<unsigned>(ctrl, 0x20);                           // 0x10005587 / 0x1000558b
    fat<unsigned>(rec, 0x24) = fat<unsigned>(ctrl, 0x24);                           // 0x1000558e / 0x10005592
    ListNode* node = static_cast<ListNode*>(c3fg_new(0xc));                         // 0x10005595
    if (node) { node->prev = 0; node->next = 0; node->value = rec; }                // 0x100055a2-0x100055af
    ListNode* tail = fat<ListNode*>(self, 0xa50);                                   // 0x100055b6
    if (tail) {                                                                     // 0x100055be
        tail->next = node;                                                          // 0x100055c0
        node->prev = fat<ListNode*>(self, 0xa50);                                   // 0x100055c9
    } else {
        fat<ListNode*>(self, 0xa4c) = node;                                         // 0x100055cd
    }
    fat<ListNode*>(self, 0xa50) = node;                                             // 0x100055d3
    return ++fat<unsigned>(self, 0xa58);                                            // 0x100055d9-0x100055e0
}

}  // namespace

SG_HOOK("sound.dll", 0x00032130, beginDecodeStreamA, DecA_re, DecA_orig);
SG_HOOK("sound.dll", 0x00032290, beginDecodeStreamB, DecB_re, DecB_orig);
SG_HOOK("sound.dll", 0x00005770, processWaveChunks, ProcChunks_re, ProcChunks_orig);
SG_HOOK("sound.dll", 0x000399a0, loadMultiChannel, LoadMulti_re, LoadMulti_orig);
SG_HOOK("sound.dll", 0x00005440, Wave_Device_readFmtChunk, ReadFmt_re, ReadFmt_orig);
SG_HOOK("sound.dll", 0x000326a0, parseRiffWave, Parse_re, Parse_orig);
SG_HOOK("sound.dll", 0x0001d380, MidiReader_init, MidiInit_re, MidiInit_orig);
SG_HOOK("sound.dll", 0x00032020, Sound_resetBuffers, ResetBufs_re, ResetBufs_orig);
