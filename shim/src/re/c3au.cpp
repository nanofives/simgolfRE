// C3 batch c3au (2026-10-08) of sound.dll: DirectSound buffer geometry, sequencer list helpers, the ADPCM bit
// (de)serialiser and two x87 leaves.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each
// body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio and re/analysis/util. sound.dll is LoadLibrary'd after the shim starts, so
// these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// Four functions forward to an original callee at its own address (bitPack/bitUnpack -> bitPermute 0x1003bf30,
// Voice_setLevel -> Voice_setPanGains 0x10037f30, codecApplyFixedCoeffs -> the IIR filter 0x10040e30); sound.dll
// is relocatable, so those addresses are resolved against the module base at the first call.
// No DirectSound, winmm or Win32 call is made, and no live sound object, device or list is touched: every object
// the A/B passes in is allocated by the fixture.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fld(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T fldc(const void* base, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + off);
}

// sound.dll has base relocations, so an original callee or constant is addressed through the loaded base.
char* c3au_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

// 0x0000f480  mapDsError (VA 0x1000f480)
// __cdecl with one stack argument ([esp+4] at 0x1000f480, `ret` at 0x1000f4a2 pops nothing): a DirectSound /
// COM result code, mapped to this module's own small error numbers. The compare chain is SIGNED
// (`jg` at 0x1000f489, 0x1000f492), and every one of these codes has bit 31 set, so the chain orders them
// 0x80004001 < 0x8007000e < 0x80070057 < 0x8878000a < 0x88780064...
// Above 0x8878000a the code is range-checked by `add eax, 0x7787ff9c` (0x1000f4bc, that is eax - 0x88780064)
// and `cmp eax, 0x46` / `ja 0x1000f4ed` (0x1000f4c1): outside 0x88780064..0x887800aa the result is 0x2f. Inside,
// a 0x47-byte index table at 0x1000f508 selects one of five arms through the jump table at 0x1000f4f4; only four
// of its entries are not the default: index 0 (0x88780064) -> 0x12 (0x1000f4db), index 0x14 (0x88780078) -> 1
// (0x1000f4d5), index 0x32 (0x88780096) -> 0x1e (0x1000f4e7) and index 0x46 (0x887800aa) -> 0x13 (0x1000f4e1).
// The four codes below the range are handled by the compare chain itself: 0x8878000a -> 0xc (0x1000f4b6),
// 0x80070057 -> 0xa (0x1000f4b0), 0x8007000e -> 0x11 (0x1000f4a3), 0x80004001 -> 0xb (0x1000f49d). Anything
// else returns 0x2f (0x1000f4ed).
typedef int(__cdecl* MapDsError_t)(int);
MapDsError_t MapDsError_orig;
int __cdecl MapDsError_re(int hr) {
    const int kAllocated = static_cast<int>(0x8878000a);
    const int kOutOfMemory = static_cast<int>(0x8007000e);
    if (hr > kAllocated) {                                  // jg at 0x1000f489 (signed)
        unsigned idx = static_cast<unsigned>(hr) - 0x88780064u;
        if (idx > 0x46u) return 0x2f;                       // ja at 0x1000f4c4
        switch (idx) {
            case 0x00: return 0x12;
            case 0x14: return 1;
            case 0x32: return 0x1e;
            case 0x46: return 0x13;
            default: return 0x2f;                           // every other index selects arm 4
        }
    }
    if (hr == kAllocated) return 0xc;                       // je at 0x1000f48b
    if (hr > kOutOfMemory)                                  // jg at 0x1000f492 (signed)
        return hr == static_cast<int>(0x80070057) ? 0xa : 0x2f;
    if (hr == kOutOfMemory) return 0x11;                    // je at 0x1000f494
    return hr == static_cast<int>(0x80004001) ? 0xb : 0x2f;
}

// 0x00035770  Channel_findStreamNode (VA 0x10035770)
// __thiscall with no stack argument (`ret` at 0x100357bb), returns an int that starts as -1
// (`or eax, 0xffffffff` at 0x10035776). The list head is [ecx+0x1348] (0x10035770) and is copied into the walk
// cursor [ecx+0x1350] (0x1003577d); each node holds its successor at +4 (0x100357a5) and its record at +8
// (0x10035785, 0x100357b2). For each record the channel's position [ecx+0x16c] (0x1003578c) is compared
// UNSIGNED with the record's key [edx+4] (`cmp esi, edi` at 0x10035795, `jb 0x100357b9` at 0x10035797): a key
// above the position ends the walk, otherwise the record's first dword becomes the result (0x10035799) and the
// cursor advances. The walk also ends on a null head (0x10035783), a null first record (0x1003578a), a null
// cursor re-read from [ecx+0x1350] (0x100357a3), a null successor (0x100357b0) or a null record (0x100357b7).
// The cursor is written on every step, so it is left pointing at the node the walk stopped on (or null).
typedef int(__fastcall* ChanFindStream_t)(void*, void*);
ChanFindStream_t ChanFindStream_orig;
int __fastcall ChanFindStream_re(void* self, void*) {
    int result = -1;
    void* node = fld<void*>(self, 0x1348);
    fld<void*>(self, 0x1350) = node;
    if (!node) return result;
    void* rec = fld<void*>(node, 8);
    if (!rec) return result;
    for (;;) {
        if (fld<unsigned>(self, 0x16c) < fldc<unsigned>(rec, 4)) break;
        result = fldc<int>(rec, 0);
        node = fld<void*>(self, 0x1350);
        if (!node) break;
        node = fld<void*>(node, 4);
        fld<void*>(self, 0x1350) = node;
        if (!node) break;
        rec = fld<void*>(node, 8);
        if (!rec) break;
    }
    return result;
}

// 0x00038330  Channel_findVoiceByTag (VA 0x10038330)
// __thiscall, one stack argument (`ret 4` at 0x10038369 and 0x1003837f): a tag to look for in the channel's
// 0x10 voice records. The search mask is bits 12..27 of the channel flag dword (`shr eax, 0xc` at 0x10038333,
// `and eax, 0xffff` at 0x1003833c), one bit per voice, and the scan pointer starts at [ecx+0x2d4]
// (`lea esi, [ecx+0x2d4]` at 0x10038343) and advances by 0x110 (0x1003835a) for 0x10 iterations
// (`cmp edx, 0x10` / `jl 0x10038349` at 0x10038360). A voice is accepted only when its mask bit is set
// (`test al, 1` at 0x10038349), its dword at +0x48 of the scan pointer equals the argument (`cmp [esi+0x48],
// edi` at 0x1003834d) and bit 3 of the byte at the scan pointer is CLEAR (`test byte [esi], 8` at 0x10038352,
// `je 0x1003836c` jumps to the hit). The hit returns the voice record's address, 0x238 + index*0x110 from the
// channel (`shl`/`add`/`shl` at 0x1003836f-0x10038375 and `lea eax, [eax+ecx+0x238]` at 0x10038378); no hit
// returns 0 (0x10038367). Nothing is written.
typedef void*(__fastcall* ChanFindVoice_t)(void*, void*, unsigned);
ChanFindVoice_t ChanFindVoice_orig;
void* __fastcall ChanFindVoice_re(void* self, void*, unsigned tag) {
    unsigned mask = (fld<unsigned>(self, 0x58) >> 12) & 0xffffu;
    for (unsigned i = 0; i < 0x10u; ++i) {
        unsigned scan = 0x2d4u + i * 0x110u;
        if ((mask & 1) && fld<unsigned>(self, scan + 0x48) == tag && (fld<unsigned char>(self, scan) & 8) == 0)
            return reinterpret_cast<char*>(self) + 0x238u + i * 0x110u;
        mask >>= 1;
    }
    return 0;
}

// 0x0001e230  Track_resetReadState (VA 0x1001e230)
// __thiscall with no stack argument (`ret` at 0x1001e272 and 0x1001e283), no return value (eax holds the
// clamped field value on every path). The flag dword [ecx+0x4c] is first zeroed outright (0x1001e233) and then
// rewritten with bit 1 of its OLD value kept in place and every other bit clear: the old value is shifted left
// 30 and arithmetically right 31 (0x1001e23d, 0x1001e240), masked to 1 (0x1001e243) and shifted back up one
// (0x1001e249), while the re-read of the already-zeroed field (0x1001e23a) contributes nothing
// (`and edx, 0xfffffffd` at 0x1001e246, `or eax, edx` at 0x1001e24b, stored at 0x1001e24d).
// Then the read cursor [ecx+0x178] is reloaded from [ecx+0x17c] (0x1001e250, 0x1001e256) and the signed value
// at [ecx+0x180] is clamped into [-0x40, 0x3f]: below -0x40 (`cmp eax, -0x40` / `jge 0x1001e273` at 0x1001e262)
// it is replaced by 0xffffffc0 (0x1001e267) and the function returns, above 0x3f (`cmp eax, 0x3f` /
// `jle 0x1001e27d` at 0x1001e273) by 0x3f (0x1001e278); in range it is written back unchanged (0x1001e27d).
typedef void(__fastcall* TrackResetRead_t)(void*, void*);
TrackResetRead_t TrackResetRead_orig;
void __fastcall TrackResetRead_re(void* self, void*) {
    unsigned old = fld<unsigned>(self, 0x4c);
    fld<unsigned>(self, 0x4c) = 0;                       // the store at 0x1001e233, overwritten below
    fld<unsigned>(self, 0x4c) = ((old >> 1) & 1u) << 1;
    fld<unsigned>(self, 0x178) = fld<unsigned>(self, 0x17c);
    int v = fld<int>(self, 0x180);
    if (v < -0x40) {
        fld<int>(self, 0x180) = -0x40;
        return;
    }
    if (v > 0x3f) v = 0x3f;
    fld<int>(self, 0x180) = v;
}

// 0x000116a0  seekListToIndex (VA 0x100116a0)
// __thiscall, one stack argument (`ret 4` at 0x100116b3, 0x100116c5, 0x100116d6 and 0x100116f5): the index to
// seek to. Three guards run first: a zero count at [ecx+0x148] returns 3 (0x100116a7, 0x100116ad), an index at
// or above the count returns 0xa (`cmp edi, esi` / `jb 0x100116c8` at 0x100116ba, UNSIGNED, 0x100116bf), and a
// non-null cursor already parked at [ecx+8] returns 6 (0x100116cb, 0x100116d0).
// The walk starts at the head [ecx+4] (0x100116d9) with a counter in edx (0x100116dc) and runs at most `count`
// steps (`cmp edx, esi` / `jb 0x100116e2` at 0x100116ed). Each step compares the counter with the index
// (0x100116e2), stores the current node at [ecx+8] (0x100116e4, which happens before the exit test because the
// `mov` does not touch the flags) and leaves when they are equal (`je 0x100116f1` at 0x100116e7); otherwise the
// node's successor at +0x10 is loaded (0x100116e9). The `test esi, esi` / `jbe 0x100116f1` at 0x100116de can
// never take its jump: the count was already proved non-zero by the first guard. Returns 0 (0x100116f2).
typedef int(__fastcall* SeekList_t)(void*, void*, unsigned);
SeekList_t SeekList_orig;
int __fastcall SeekList_re(void* self, void*, unsigned index) {
    unsigned count = fld<unsigned>(self, 0x148);
    if (count == 0) return 3;
    if (index >= count) return 0xa;
    if (fld<void*>(self, 8) != 0) return 6;
    void* node = fld<void*>(self, 4);
    for (unsigned i = 0;;) {
        bool last = (i == index);
        fld<void*>(self, 8) = node;
        if (last) break;
        node = fld<void*>(node, 0x10);
        if (++i >= count) break;
    }
    return 0;
}

// 0x0002ad10  Sound_setSourceType (VA 0x1002ad10)
// __thiscall, one stack argument (`ret 4` at 0x1002ad2f and the other arms): a source type. `lea edx, [eax-1]`
// (0x1002ad14) and `cmp edx, 6` / `ja 0x1002ad77` (0x1002ad17) send everything outside 1..7 to the tail that
// only stores the argument at [ecx+0x54] (0x1002ad77). The jump table at 0x1002ad80 (0x1002ad1c) sends type 1
// to 0x1002ad41 (bit 2 of [ecx+0x44]), 2 to 0x1002ad32 (bit 3), 3 to the plain tail 0x1002ad77, 4 to 0x1002ad23
// (bit 4), 5 to 0x1002ad50 (bits 3 and 5, `or edx, 0x28`), 6 to 0x1002ad5f (`or dh, 1`, that is bit 8) and 7 to
// 0x1002ad6e (`or dl, 0x80`, bit 7), which is the only arm that falls through into the tail instead of
// returning. Every arm stores the argument at [ecx+0x54]; the flag bits are only ever set, never cleared.
typedef void(__fastcall* SoundSetSourceType_t)(void*, void*, unsigned);
SoundSetSourceType_t SoundSetSourceType_orig;
void __fastcall SoundSetSourceType_re(void* self, void*, unsigned type) {
    if (type - 1u <= 6u) {
        switch (type) {
            case 1: fld<unsigned>(self, 0x44) |= 0x04u; break;
            case 2: fld<unsigned>(self, 0x44) |= 0x08u; break;
            case 3: break;
            case 4: fld<unsigned>(self, 0x44) |= 0x10u; break;
            case 5: fld<unsigned>(self, 0x44) |= 0x28u; break;
            case 6: fld<unsigned>(self, 0x44) |= 0x100u; break;
            case 7: fld<unsigned>(self, 0x44) |= 0x80u; break;
        }
    }
    fld<unsigned>(self, 0x54) = type;
}

// 0x0001d780  Seq_listGetAt (VA 0x1001d780)
// __thiscall, two stack arguments (`ret 8` at 0x1001d7df): an out slot ([esp+0xc] at 0x1001d79b) and a step
// count ([esp+0x10] at 0x1001d79f). The list head is [ecx+0x19c] (0x1001d780), copied into the cursor
// [ecx+0x1a4] (0x1001d78c); a node holds its successor at +4 (0x1001d7b3) and its payload at +8 (0x1001d794,
// 0x1001d7c0). The payload of the head is written to the out slot first (0x1001d7a5); a zero count skips the
// loop (`test edi, edi` / `jbe 0x1001d7d2` at 0x1001d7a3).
// Each step re-reads the cursor (0x1001d7a9), follows one successor link, stores it back as the cursor
// (0x1001d7b8) and writes that node's payload (or 0 when the cursor or the successor is null, 0x1001d7c5) to
// the out slot (0x1001d7c9); a null payload ends the walk early (`je 0x1001d7d2` at 0x1001d7cb), otherwise the
// loop runs until the step counter reaches the count (`cmp esi, edi` / `jb 0x1001d7a9` at 0x1001d7ce,
// UNSIGNED). The return value is derived from the final out slot with `neg`/`sbb`/`and al, 0xdc`/`add 0x24`
// (0x1001d7d5-0x1001d7dc): 0 when it is non-null, 0x24 when it is null.
typedef int(__fastcall* SeqListGetAt_t)(void*, void*, void**, unsigned);
SeqListGetAt_t SeqListGetAt_orig;
int __fastcall SeqListGetAt_re(void* self, void*, void** out, unsigned count) {
    void* node = fld<void*>(self, 0x19c);
    fld<void*>(self, 0x1a4) = node;
    *out = node ? fld<void*>(node, 8) : 0;
    if (count != 0) {
        for (unsigned i = 0;;) {
            void* payload = 0;
            node = fld<void*>(self, 0x1a4);
            if (node) {
                node = fld<void*>(node, 4);
                fld<void*>(self, 0x1a4) = node;
                if (node) payload = fld<void*>(node, 8);
            }
            *out = payload;
            if (!payload) break;
            if (++i >= count) break;
        }
    }
    return *out != 0 ? 0 : 0x24;
}

// 0x00022280  Seq_copyChannelParams (VA 0x10022280)
// __thiscall, one stack argument (`ret 4` at 0x1002228d and 0x100222f4): the source record. A null source
// returns 0xa (0x10022284, 0x10022288). The destination slot is selected by the source's own index at +0x28
// (0x10022290): `lea esi, [edx+edx*8]` and `lea edx, [edx+esi*2]` (0x10022294, 0x10022297) multiply it by 19,
// and `lea ecx, [ecx+edx*4+0x264]` (0x1002229b) turns that into this + 0x264 + index*0x4c, so the table's
// stride is 0x4c bytes. No bound is checked on the index.
// Thirteen fields are copied from the source to the slot at the same offsets: the dwords at +4, +8, +0xc,
// +0x10, +0x14 (0x100222a2-0x100222bd), the WORD at +0x18 (`mov dx, [eax+0x18]` at 0x100222c0, so +0x1a is not
// copied) and the dwords at +0x1c, +0x20, +0x24, +0x28, +0x2c, +0x30 and +0x34 (0x100222c8-0x100222ef).
// Offset 0 of the source is not read and offset 0 of the slot is not written. Returns 0 (0x100222f2).
typedef int(__fastcall* SeqCopyChanParams_t)(void*, void*, const void*);
SeqCopyChanParams_t SeqCopyChanParams_orig;
int __fastcall SeqCopyChanParams_re(void* self, void*, const void* src) {
    if (!src) return 0xa;
    unsigned index = fldc<unsigned>(src, 0x28);
    char* dst = reinterpret_cast<char*>(self) + 0x264u + index * 0x4cu;
    static const unsigned kDwords[] = {4, 8, 0xc, 0x10, 0x14, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34};
    for (unsigned i = 0; i < sizeof(kDwords) / sizeof(kDwords[0]); ++i)
        fld<unsigned>(dst, kDwords[i]) = fldc<unsigned>(src, kDwords[i]);
    fld<unsigned short>(dst, 0x18) = fldc<unsigned short>(src, 0x18);
    return 0;
}

// 0x00023c70  Seq_cmdHandler11 (VA 0x10023c70)
// __thiscall, two stack arguments (`ret 8` at 0x10023c7f, 0x10023ccb and 0x10023cd7): a key ([esp+4] at
// 0x10023c99) and a value ([esp+8] at 0x10023cce). A zero guard dword at [ecx+0x224] returns 0x24 at once
// (0x10023c70, 0x10023c7a). The list head is [ecx+0x218] (0x10023c82) and is copied into the cursor
// [ecx+0x220] (0x10023c88); a node holds its successor at +4 (0x10023cac) and its record at +8 (0x10023c92,
// 0x10023cb9). The walk compares the key with each record's dword at +4 (`cmp [eax+4], edx` at 0x10023c9d) and
// stops at the first match (`je 0x10023cc2`); a null head, a null record, a null cursor re-read from
// [ecx+0x220] or a null successor all return 0x24 (0x10023c90, 0x10023c97, 0x10023caa, 0x10023cb7, 0x10023cbe,
// 0x10023cc6). On a match the value is stored at +0x30 of the record (0x10023cd2) and 0 is returned
// (0x10023cd5). The `test eax, eax` / `jne 0x10023cce` at 0x10023cc2 re-tests the record that was just
// dereferenced, so its zero side cannot be reached from the match.
typedef int(__fastcall* SeqCmd11_t)(void*, void*, unsigned, unsigned);
SeqCmd11_t SeqCmd11_orig;
int __fastcall SeqCmd11_re(void* self, void*, unsigned key, unsigned value) {
    if (fld<unsigned>(self, 0x224) == 0) return 0x24;
    void* node = fld<void*>(self, 0x218);
    fld<void*>(self, 0x220) = node;
    if (!node) return 0x24;
    void* rec = fld<void*>(node, 8);
    if (!rec) return 0x24;
    while (fld<unsigned>(rec, 4) != key) {
        node = fld<void*>(self, 0x220);
        if (!node) return 0x24;
        node = fld<void*>(node, 4);
        fld<void*>(self, 0x220) = node;
        if (!node) return 0x24;
        rec = fld<void*>(node, 8);
        if (!rec) return 0x24;
    }
    fld<unsigned>(rec, 0x30) = value;
    return 0;
}

// 0x00037e10  MmioBuffer_init (VA 0x10037e10)
// __thiscall, two stack arguments (`ret 8` at 0x10037e62), no return value (eax holds this + 0xa0 at the end,
// 0x10037e40). The first argument is an optional 0x48-byte block: when it is non-null (`cmp esi, edx` /
// `je 0x10037e28` at 0x10037e19, edx being the zero set at 0x10037e15) 0x12 dwords are copied from it to the
// object's offset 0 (`mov ecx, 0x12` at 0x10037e1e, `rep movsd` at 0x10037e25); when it is null bytes 0..0x47
// are left as they were. Then the three dwords at +0x88, +0x8c and +0x90 are zeroed (0x10037e2c-0x10037e38).
// The second argument is dereferenced unconditionally: its four dwords at +0, +4, +8 and +0xc and the WORD at
// +0x10 are copied to the object's +0xa0 .. +0xb1 (0x10037e3e-0x10037e5e), that is 0x12 bytes, with +0xb2 and
// beyond untouched.
typedef void(__fastcall* MmioBufferInit_t)(void*, void*, const void*, const void*);
MmioBufferInit_t MmioBufferInit_orig;
void __fastcall MmioBufferInit_re(void* self, void*, const void* block, const void* fmt) {
    if (block) {
        for (unsigned i = 0; i < 0x12u; ++i) fld<unsigned>(self, i * 4) = fldc<unsigned>(block, i * 4);
    }
    fld<unsigned>(self, 0x88) = 0;
    fld<unsigned>(self, 0x8c) = 0;
    fld<unsigned>(self, 0x90) = 0;
    for (unsigned i = 0; i < 4u; ++i) fld<unsigned>(self, 0xa0 + i * 4) = fldc<unsigned>(fmt, i * 4);
    fld<unsigned short>(self, 0xb0) = fldc<unsigned short>(fmt, 0x10);
}

// 0x00010eb0  DsBuffer::setRegionMs (VA 0x10010eb0)
// __thiscall, two stack arguments (`ret 8` at 0x10010ee8 and 0x10010eff): a duration in milliseconds
// ([esp+4] at 0x10010ebb) and a region count ([esp+0x10] after the two pushes, that is the second argument, at
// 0x10010ed8). The byte rate at [ecx+0xac] (0x10010eb0) is multiplied by the milliseconds (`imul` at
// 0x10010ebb, 32-bit truncating) and divided by 1000 by the usual unsigned magic sequence (`mov eax,
// 0x10624dd3` at 0x10010eb6, `mul edx` at 0x10010ec0, `shr edx, 6` at 0x10010ecd). That quotient is multiplied
// by the 16-bit block alignment at [ecx+0xb4] (zero-extended at 0x10010ec5, `imul esi, edx` at 0x10010ed0) to
// give one region's size, and by the region count (0x10010ed8) to give the total.
// A total above the buffer size [ecx+0x78] (read at 0x10010ed5, `cmp eax, edi` / `jbe 0x10010eeb` at
// 0x10010edd, UNSIGNED) returns 0xa and writes nothing (0x10010ee2). Otherwise the total goes to [ecx+0x38]
// (0x10010eeb), half of (total - region size) to [ecx+0x3c] (`sub eax, esi` / `shr eax, 1` at 0x10010eee,
// stored at 0x10010ef5), the region size to [ecx+0x40] (0x10010ef2) and the millisecond quotient to [ecx+0x34]
// (0x10010ef9), and 0 is returned (0x10010efc).
typedef int(__fastcall* DsBufSetRegionMs_t)(void*, void*, unsigned, unsigned);
DsBufSetRegionMs_t DsBufSetRegionMs_orig;
int __fastcall DsBufSetRegionMs_re(void* self, void*, unsigned ms, unsigned regions) {
    unsigned frames = (fld<unsigned>(self, 0xac) * ms) / 1000u;     // the 0x10624dd3 / shr 6 magic
    unsigned region = static_cast<unsigned>(fld<unsigned short>(self, 0xb4)) * frames;
    unsigned total = region * regions;
    if (total > fld<unsigned>(self, 0x78)) return 0xa;
    fld<unsigned>(self, 0x38) = total;
    fld<unsigned>(self, 0x40) = region;
    fld<unsigned>(self, 0x3c) = (total - region) >> 1;
    fld<unsigned>(self, 0x34) = frames;
    return 0;
}

// 0x0003e380  floatCopySign (VA 0x1003e380)
// __cdecl with two stack arguments ([ebp+8] at 0x1003e398 and [ebp+0xc] at 0x1003e3c4, `ret` at 0x1003e3f5
// pops nothing), both pointers to a float; returns a double in st(0) (`fld qword [ebp-0x14]` at 0x1003e3ec).
// A /Od /GZ debug body, so every intermediate is spilled and the rounding points are visible.
// The first float is compared with the dword at 0x1005ec00, which is 0.0f in the image (bytes 00 00 00 00 at
// RVA 0x5ec00): `fld dword [eax]` at 0x1003e39b, `fcomp dword` at 0x1003e39d, `fnstsw ax` + `test ah, 1` +
// `jne 0x1003e3b4` at 0x1003e3a3-0x1003e3a8. ah bit 0 is the x87 C0 flag, which fcomp sets when st(0) is below
// the operand AND when the comparison is unordered (either operand a NaN), so a negative value and a NaN both
// take the jump and are negated into the float slot [ebp-0xc] (`fchs` at 0x1003e3b9, `fstp dword` at
// 0x1003e3bb); otherwise the argument's four bytes are copied into that slot unchanged (0x1003e3ad), which
// keeps -0.0 as -0.0 because it compares equal to 0.0f and C0 stays clear.
// That float is widened to the double slot [ebp-8] (`fld dword` / `fstp qword` at 0x1003e3be, exact). The
// second float is tested the same way against 0.0f (0x1003e3c7-0x1003e3d4): below zero or unordered, the
// double is negated into [ebp-0x14] (`fchs` at 0x1003e3e7), otherwise it is copied there as two dwords
// (0x1003e3d6-0x1003e3df). Every step is a sign flip or an exact widening, so no rounding mode can change the
// result.
typedef double(__cdecl* FloatCopySign_t)(const float*, const float*);
FloatCopySign_t FloatCopySign_orig;
double __cdecl FloatCopySign_re(const float* mag, const float* sign) {
    float a = *mag;
    float m = !(a >= 0.0f) ? -a : a;             // C0 set = below zero or unordered
    double r = static_cast<double>(m);
    float s = *sign;
    if (!(s >= 0.0f)) r = -r;
    return r;
}

// 0x0003c180  bitPack (VA 0x1003c180)
// __cdecl with six stack arguments ([ebp+8] .. [ebp+0x1c], `ret` at 0x1003c1ca pops nothing), a /Od /GZ debug
// wrapper: it pushes its six arguments in reverse (0x1003c198-0x1003c1af) behind a leading 0 (`push 0` at
// 0x1003c1b0) and calls bitPermute at 0x1003bf30 (0x1003c1b2), then returns bitPermute's value in eax, which
// is always 0 (`xor eax, eax` at 0x1003c04b and 0x1003c169). The leading 0 is bitPermute's mode selector: 0
// serialises, 1 deserialises (`cmp [ebp-0x44], 1` / `je 0x1003bf6e` at 0x1003bf66). bitPack's sixth argument
// is the one only the serialising mode uses (`[ebp+0x20] + 0x2540` at 0x1003bf76).
// The reimplementation forwards to the original bitPermute: the hook under test is this wrapper's argument
// shuffle, not the 578-byte body it calls.
typedef int(__cdecl* BitPermute_t)(int, const int*, int*, int*, int*, int*, void*);
typedef int(__cdecl* BitPack_t)(const int*, int*, int*, int*, int*, void*);
BitPack_t BitPack_orig;
int __cdecl BitPack_re(const int* count, int* a, int* b, int* values, int* bits, void* ctx) {
    BitPermute_t permute = reinterpret_cast<BitPermute_t>(c3au_sound(0x0003bf30));
    return permute(0, count, a, b, values, bits, ctx);
}

// 0x0003c1d0  bitUnpack (VA 0x1003c1d0)
// __cdecl with five stack arguments ([ebp+8] .. [ebp+0x18], `ret` at 0x1003c218), the same /Od /GZ wrapper
// shape as bitPack: a trailing 0 is pushed first (0x1003c1e8), then the five arguments in reverse
// (0x1003c1ea-0x1003c1fd), then the mode 1 (`push 1` at 0x1003c1fe), and bitPermute 0x1003bf30 is called
// (0x1003c200). So the serialising mode's extra argument is passed as NULL, which mode 1 never reads: the
// slot [ebp-4] that holds it in mode 0 is only written at 0x1003bf7b, on the other side of the mode test.
// Returns bitPermute's 0.
typedef int(__cdecl* BitUnpack_t)(const int*, int*, int*, int*, int*);
BitUnpack_t BitUnpack_orig;
int __cdecl BitUnpack_re(const int* count, int* a, int* b, int* values, int* bits) {
    BitPermute_t permute = reinterpret_cast<BitPermute_t>(c3au_sound(0x0003bf30));
    return permute(1, count, a, b, values, bits, 0);
}

// 0x000383b0  Voice_setLevel (VA 0x100383b0)
// __thiscall, one stack argument (`ret 4` at 0x100383e7), no return value (eax is whatever the callee left).
// The argument is written into an 8-byte stack temporary with a zero high dword (`mov [esp+4], 0` at
// 0x100383b7, `mov [esp], eax` at 0x100383bf) and loaded with `fild qword` (0x100383c3), so it is converted as
// an UNSIGNED 32-bit value; the product with the double at 0x1005b488, which is 0.007874015748031496 in the
// image (bytes 08 04 02 81 40 20 80 3f at RVA 0x5b488, that is 1/127 rounded to double), is stored as a double
// at [ecx+0x58] (`fmul qword` at 0x100383c9, `fstp qword` at 0x100383cf). The thread's x87 precision control is
// 53 bits (control word 0x027f, measured in the running game), so that multiply rounds once, directly to
// double, and an SSE2 double multiply gives the same bits.
// Then [ecx+0x50] gets (argument + (argument != 0)) << 9: `test eax, eax` at 0x100383c7 with `je 0x100383d5`
// skipping the `inc eax` at 0x100383d4, and `shl eax, 9` at 0x100383d5, stored at 0x100383d8 (the shift drops
// the top 9 bits). Finally the current pan at [ecx+0x48] (0x100383db) is passed to Voice_setPanGains
// (VA 0x10037f30, through the incremental-link thunk at 0x1000231a) with the voice still in ecx, which
// recomputes the gain pairs at +0x68, +0x70, +0x74, +0x78, +0x80 and +0x84 from the double just written.
// The reimplementation calls that original callee at its own address.
typedef void(__fastcall* VoiceSetPanGains_t)(void*, void*, int);
typedef void(__fastcall* VoiceSetLevel_t)(void*, void*, unsigned);
VoiceSetLevel_t VoiceSetLevel_orig;
void __fastcall VoiceSetLevel_re(void* self, void*, unsigned level) {
    const double scale = *reinterpret_cast<const double*>(c3au_sound(0x0005b488));
    fld<double>(self, 0x58) = static_cast<double>(level) * scale;
    fld<unsigned>(self, 0x50) = (level + (level != 0 ? 1u : 0u)) << 9;
    VoiceSetPanGains_t gains = reinterpret_cast<VoiceSetPanGains_t>(c3au_sound(0x00037f30));
    gains(self, 0, fld<int>(self, 0x48));
}

// 0x00006bd0  computeCaptureDuration (VA 0x10006bd0)
// __thiscall with no stack argument (`ret` at 0x10006c40), no return value (eax is the last value computed).
// Two guards leave the object untouched: a zero at [esi+0xb4] (0x10006bd6, `je 0x10006c3c` at 0x10006bde) and a
// zero at [esi+0xd4] (0x10006be0, `je 0x10006c3c` at 0x10006be8).
// [esi+0xb4] is divided UNSIGNED by the 16-bit field at [esi+0xe4] (zero-extended at 0x10006bed, `xor edx,edx`
// + `div edi` at 0x10006bf4), giving a per-unit size. [esi+0xd4] is then divided SIGNED by it twice
// (`cdq` + `idiv edi` at 0x10006c04 and 0x10006c0f): the first quotient is stored at [esi+0xc8] (0x10006c07)
// and the second division is run again only for its remainder (0x10006c18).
// The remainder is converted with `fild dword` (0x10006c1c, signed 32-bit) and divided by [esi+0xdc] loaded
// with `fild qword` from a temporary whose high dword was zeroed at 0x10006bf8 (0x10006c20, 0x10006c24), that
// is as an UNSIGNED 32-bit value; `fdivp st(1)` at 0x10006c28 leaves remainder/value. That is multiplied by the
// double at 0x1005b280, which is 1000.0 in the image (bytes 00 00 00 00 00 40 8f 40 at RVA 0x5b280), at
// 0x10006c2a, and converted to int by the CRT's __ftol at 0x10042550 (0x10006c30), which truncates toward zero
// and keeps the low dword of a 64-bit fistp; the result is stored at [esi+0xc0] (0x10006c35).
// Both the divide and the multiply round once at the thread's 53-bit precision control, so double arithmetic
// in the same order reproduces them.
typedef void(__fastcall* ComputeCaptureDuration_t)(void*, void*);
ComputeCaptureDuration_t ComputeCaptureDuration_orig;
void __fastcall ComputeCaptureDuration_re(void* self, void*) {
    unsigned total = fld<unsigned>(self, 0xb4);
    if (total == 0) return;
    int span = fld<int>(self, 0xd4);
    if (span == 0) return;
    unsigned unitSize = total / static_cast<unsigned>(fld<unsigned short>(self, 0xe4));
    fld<int>(self, 0xc8) = span / static_cast<int>(unitSize);
    int rem = span % static_cast<int>(unitSize);
    const double thousand = *reinterpret_cast<const double*>(c3au_sound(0x0005b280));
    double v = static_cast<double>(rem) / static_cast<double>(fld<unsigned>(self, 0xdc)) * thousand;
    fld<int>(self, 0xc0) = static_cast<int>(static_cast<long long>(v));   // __ftol: 64-bit truncation, low dword
}

// 0x0003d040  codecApplyFixedCoeffs (VA 0x1003d040)
// __cdecl with three stack arguments ([ebp+8], [ebp+0xc], [ebp+0x10]; `ret` at 0x1003d095 pops nothing), a
// /Od /GZ debug wrapper that always returns 0 (`xor eax, eax` at 0x1003d083). A non-null first argument is
// decremented by 4 (`sub eax, 4` at 0x1003d061, guarded by `cmp [ebp+8], 0` / `je 0x1003d067` at 0x1003d058)
// and 4 is added back when it is pushed (`add eax, 4` at 0x1003d077), so a non-null pointer is passed through
// unchanged and a null one is passed as 4.
// The call at 0x1003d07b is the IIR filter at 0x10040e30, with four arguments pushed in reverse
// (0x1003d06a-0x1003d07a): that pointer, the address 0x10064a18 (whose first dword is 1 and is read as the
// filter's first sample index at 0x10040e87), this function's second argument (the filter reads the last index
// through it at 0x10040e7f) and its third (the filter's four-float state, read at 0x10040e5a-0x10040e74 and
// written back at 0x10040f2c-0x10040f4c). The filter subtracts 4 from the sample pointer itself (0x10040e51)
// and indexes it from 1 (0x10040ea7, 0x10040f24), so the samples written are the caller's [0 .. last-1].
// The reimplementation forwards to that original filter.
typedef int(__cdecl* CodecFilter_t)(void*, const int*, const int*, float*);
typedef int(__cdecl* CodecApplyFixed_t)(float*, const int*, float*);
CodecApplyFixed_t CodecApplyFixed_orig;
int __cdecl CodecApplyFixed_re(float* samples, const int* last, float* state) {
    char* p = samples ? reinterpret_cast<char*>(samples) - 4 : 0;
    CodecFilter_t filter = reinterpret_cast<CodecFilter_t>(c3au_sound(0x00040e30));
    filter(p + 4, reinterpret_cast<const int*>(c3au_sound(0x00064a18)), last, state);
    return 0;
}

}  // namespace

SG_HOOK("sound.dll", 0x0000f480, sound_mapDsError, MapDsError_re, MapDsError_orig);
SG_HOOK("sound.dll", 0x00035770, sound_Channel_findStreamNode, ChanFindStream_re, ChanFindStream_orig);
SG_HOOK("sound.dll", 0x00038330, sound_Channel_findVoiceByTag, ChanFindVoice_re, ChanFindVoice_orig);
SG_HOOK("sound.dll", 0x0001e230, sound_Track_resetReadState, TrackResetRead_re, TrackResetRead_orig);
SG_HOOK("sound.dll", 0x000116a0, sound_seekListToIndex, SeekList_re, SeekList_orig);
SG_HOOK("sound.dll", 0x0002ad10, sound_Sound_setSourceType, SoundSetSourceType_re, SoundSetSourceType_orig);
SG_HOOK("sound.dll", 0x0001d780, sound_Seq_listGetAt, SeqListGetAt_re, SeqListGetAt_orig);
SG_HOOK("sound.dll", 0x00022280, sound_Seq_copyChannelParams, SeqCopyChanParams_re, SeqCopyChanParams_orig);
SG_HOOK("sound.dll", 0x00023c70, sound_Seq_cmdHandler11, SeqCmd11_re, SeqCmd11_orig);
SG_HOOK("sound.dll", 0x00037e10, sound_MmioBuffer_init, MmioBufferInit_re, MmioBufferInit_orig);
SG_HOOK("sound.dll", 0x00010eb0, sound_DsBuffer_setRegionMs, DsBufSetRegionMs_re, DsBufSetRegionMs_orig);
SG_HOOK("sound.dll", 0x0003e380, sound_floatCopySign, FloatCopySign_re, FloatCopySign_orig);
SG_HOOK("sound.dll", 0x0003c180, sound_bitPack, BitPack_re, BitPack_orig);
SG_HOOK("sound.dll", 0x0003c1d0, sound_bitUnpack, BitUnpack_re, BitUnpack_orig);
SG_HOOK("sound.dll", 0x000383b0, sound_Voice_setLevel, VoiceSetLevel_re, VoiceSetLevel_orig);
SG_HOOK("sound.dll", 0x00006bd0, sound_computeCaptureDuration, ComputeCaptureDuration_re, ComputeCaptureDuration_orig);
SG_HOOK("sound.dll", 0x0003d040, sound_codecApplyFixedCoeffs, CodecApplyFixed_re, CodecApplyFixed_orig);
