// C3 batch c3ar (2026-10-08) of sound.dll: MIDI sequencer / voice-table helpers.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each
// body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio and re/analysis/util. sound.dll is LoadLibrary'd after the shim starts, so
// these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// Fourteen of the fifteen functions are leaves: no callee, no import, no DirectSound/winmm call. The exception is
// Seq_rewind 0x0001e1c0, which calls Seq_rewindTracks at its original address (resolved against the module base,
// because sound.dll is relocatable).
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fld(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

// 0x0001e710  Midi_readVarLen (VA 0x1001e710)
// __cdecl with two stack arguments (`ret` at 0x1001e737 pops nothing): a byte pointer ([esp+4] at 0x1001e710) and
// an out counter ([esp+8], read as [esp+0xc] at 0x1001e715 after `push esi`). The counter is zeroed before the
// loop (0x1001e71b). Each iteration loads one byte (`mov dl, [ecx]` at 0x1001e71d), keeps its low 7 bits
// (`and edx, 0x7f` at 0x1001e71f clears every bit above bit 6, so the upper bytes of edx never matter), shifts the
// accumulator left by 7 (0x1001e722) and ors the 7 bits in (0x1001e725); the counter is incremented
// (0x1001e727-0x1001e72a) and the same byte is re-read to test its top bit (`and dl, 0x80` at 0x1001e72e,
// `jne 0x1001e734` back to the top). The accumulator is a 32-bit register, so a group count above four shifts the
// earliest groups out instead of saturating. The byte pointer is advanced after the test (`inc ecx` at 0x1001e731).
typedef unsigned(__cdecl* ReadVarLen_t)(const unsigned char*, unsigned*);
ReadVarLen_t ReadVarLen_orig;
unsigned __cdecl ReadVarLen_re(const unsigned char* p, unsigned* count) {
    unsigned acc = 0;
    *count = 0;
    unsigned char cont;
    do {
        acc = (acc << 7) | static_cast<unsigned>(*p & 0x7f);
        *count = *count + 1;
        cont = static_cast<unsigned char>(*p & 0x80);
        ++p;
    } while (cont != 0);
    return acc;
}

// 0x0001e6e0  byteSwap16 (VA 0x1001e6e0)
// __cdecl with one stack argument (`ret` at 0x1001e6fa pops nothing). `push ecx` at 0x1001e6e0 reserves a 4-byte
// scratch slot, so the argument sits at [esp+8]; `lea ecx, [esp+9]` at 0x1001e6e3 points at its byte 1. The loop
// (0x1001e6e7-0x1001e6f2) copies two bytes with the source walking down (`dec ecx` at 0x1001e6ee) and the
// destination walking up ([esp+eax+2] at 0x1001e6e9): byte 1 of the argument lands at [esp+2] and byte 0 at
// [esp+3]. `mov ax, [esp+2]` at 0x1001e6f4 reads them back as one little-endian word, so the result is the low
// 16 bits of the argument with their two bytes exchanged. eax holds the loop counter 2 when the word is written
// into ax, so the high half of eax is zero on return. Bytes 2 and 3 of the argument are not read.
typedef unsigned short(__cdecl* ByteSwap16_t)(unsigned);
ByteSwap16_t ByteSwap16_orig;
unsigned short __cdecl ByteSwap16_re(unsigned v) {
    unsigned char b0 = static_cast<unsigned char>(v);
    unsigned char b1 = static_cast<unsigned char>(v >> 8);
    return static_cast<unsigned short>(b1 | (static_cast<unsigned>(b0) << 8));
}

// 0x0001e9f0  Track_advancePos (VA 0x1001e9f0)
// __thiscall, one stack argument (`ret 4` at 0x1001ea07 and 0x1001ea1e). Reads the current position [ecx+0x40]
// (0x1001e9f0) and the limit [ecx+0x44] (0x1001e9f3), adds the argument to the position (`add eax, esi` at
// 0x1001e9fb) and compares the sum with the limit UNSIGNED (`cmp eax, edx` at 0x1001e9fd, `jbe 0x1001ea0a` at
// 0x1001e9ff): past the limit it returns 0x22 (0x1001ea01) and writes nothing. Exactly at the limit it sets bit 1
// of [ecx+0x34] (`or dword [ecx+0x34], 2` at 0x1001ea0c, skipped by `jne 0x1001ea10` at 0x1001ea0a). On both
// accepted paths the new position is stored at [ecx+0x40] (0x1001ea13) and the same delta is added to the second
// counter [ecx+0x2c] (0x1001ea16, 0x1001ea1a), and 0 is returned (0x1001ea18). The addition wraps modulo 2^32.
typedef unsigned(__fastcall* TrackAdvance_t)(void*, void*, unsigned);
TrackAdvance_t TrackAdvance_orig;
unsigned __fastcall TrackAdvance_re(void* self, void*, unsigned delta) {
    unsigned pos = fld<unsigned>(self, 0x40) + delta;
    unsigned limit = fld<unsigned>(self, 0x44);
    if (pos > limit) return 0x22;
    if (pos == limit) fld<unsigned>(self, 0x34) = fld<unsigned>(self, 0x34) | 2u;
    fld<unsigned>(self, 0x40) = pos;
    fld<unsigned>(self, 0x2c) = fld<unsigned>(self, 0x2c) + delta;
    return 0;
}

// 0x00037760  Voice_setLoopFlag (VA 0x10037760)
// __thiscall, one stack argument (`ret 4` at 0x10037790), no conditional branch. The argument is a voice index:
// `shl edx, 4` / `add edx, eax` / `shl edx, 4` (0x10037766-0x1003776b) multiply it by 0x110, and 0x238 is the
// voice array's offset inside the channel (`lea eax, [edx+ecx+0x238]` at 0x1003776e). The flag byte is read from
// [edx+ecx+0x2d4] (0x10037775), which is the same address as [eax+0x9c] because 0x238 + 0x9c = 0x2d4: bit 2 is set
// (`or dl, 4` at 0x1003777c) and the byte is stored (0x10037781), then bit 1 is cleared from that same value
// (`and cl, 0xfd` at 0x10037787) and it is stored again at the same address (0x1003778a). The second store wins,
// so the flag byte ends as (old | 4) & 0xfd. No bound is checked against the index: the offset 0x2d4 + index*0x110
// is used as given.
typedef void(__fastcall* VoiceSetLoopFlag_t)(void*, void*, unsigned);
VoiceSetLoopFlag_t VoiceSetLoopFlag_orig;
void __fastcall VoiceSetLoopFlag_re(void* self, void*, unsigned index) {
    unsigned off = index * 0x110u + 0x2d4u;
    unsigned char v = static_cast<unsigned char>(fld<unsigned char>(self, off) | 4);
    fld<unsigned char>(self, off) = v;
    fld<unsigned char>(self, off) = static_cast<unsigned char>(v & 0xfd);
}

// 0x000377f0  Channel_broadcastVoiceBit1 (VA 0x100377f0)
// __thiscall, one stack argument, no return value (eax holds a leftover pointer at 0x10037822). Bit 0 of the
// channel flag dword [ecx+0x58] (read at 0x100377f0, `test al, 1` at 0x100377f3, `je 0x100377f5`) selects between
// two unrelated bodies.
// With bit 0 set, bit 0 of the argument is moved to bit 1 (`and dl, 1` at 0x100377fc, `shl dl, 1` at 0x10037805)
// and written into the flag byte of all 0x10 voices (`mov esi, 0x10` at 0x10037807; the body at
// 0x1003780c-0x1003781f reads [eax], advances eax by 0x110, clears bit 1 (`and cl, 0xfd` at 0x10037813), ors the
// new bit in (0x10037816) and stores to [eax-0x110]), starting at [ecx+0x2d4] (`lea eax, [ecx+0x2d4]` at
// 0x100377ff). The channel flag dword itself is not changed on this path.
// With bit 0 clear, nothing in the voice array is touched: bit 0 of the argument is moved to bit 7
// (`shl edx, 7` at 0x1003782e) and merged into [ecx+0x58] after that byte's bit 7 is cleared (`and al, 0x7f` at
// 0x10037829, which masks only the low byte, so bits 8..31 of the dword survive) and stored at 0x10037833.
typedef void(__fastcall* ChanBroadcastBit1_t)(void*, void*, unsigned);
ChanBroadcastBit1_t ChanBroadcastBit1_orig;
void __fastcall ChanBroadcastBit1_re(void* self, void*, unsigned bit) {
    unsigned flags = fld<unsigned>(self, 0x58);
    if (flags & 1) {
        unsigned char set = static_cast<unsigned char>((bit & 1) << 1);
        for (int i = 0; i < 0x10; ++i) {
            unsigned off = 0x2d4u + static_cast<unsigned>(i) * 0x110u;
            fld<unsigned char>(self, off) =
                static_cast<unsigned char>((fld<unsigned char>(self, off) & 0xfd) | set);
        }
        return;
    }
    fld<unsigned>(self, 0x58) = (flags & 0xffffff7fu) | ((bit & 1) << 7);
}

// 0x000382c0  Channel_allocVoiceSlot (VA 0x100382c0)
// __thiscall, one stack argument (`ret 4` at 0x100382ef and 0x10038303): a pointer to an out counter. The search
// mask is bits 12..27 of the channel flag dword (`shr eax, 0xc` at 0x100382c7, `and eax, 0xffff` at 0x100382ca).
// The counter is zeroed first (`mov dword [edx], 0` at 0x100382cf). While the mask's bit 0 is set (`test al, 1` /
// `je 0x100382e6` at 0x100382d5, and the same test at 0x100382df with `jne 0x100382da` closing the loop) the
// counter is incremented in memory and the mask is shifted right one (0x100382da-0x100382e3), so the counter ends
// as the number of consecutive set bits starting at bit 12, at most 0x10 because the mask is 16 bits wide.
// The counter is re-read from memory (0x100382e6) and compared with 0x10 (`cmp edx, 0x10` at 0x100382e8,
// `jb 0x100382f2`): 0x10 or more returns 0 (`xor eax, eax` at 0x100382ed), otherwise the address of that voice
// record is returned, 0x238 + count*0x110 from the channel (`shl`/`add`/`shl` at 0x100382f4-0x100382f9 and
// `lea eax, [eax+ecx+0x238]` at 0x100382fc).
typedef void*(__fastcall* ChanAllocVoice_t)(void*, void*, unsigned*);
ChanAllocVoice_t ChanAllocVoice_orig;
void* __fastcall ChanAllocVoice_re(void* self, void*, unsigned* out) {
    unsigned mask = (fld<unsigned>(self, 0x58) >> 12) & 0xffffu;
    *out = 0;
    if (mask & 1) {
        do {
            *out = *out + 1;
            mask >>= 1;
        } while (mask & 1);
    }
    unsigned count = *out;
    if (count >= 0x10) return 0;
    return reinterpret_cast<char*>(self) + 0x238u + count * 0x110u;
}

// 0x0001ae00  Seq_setPlayRange (VA 0x1001ae00)
// __thiscall, two stack arguments (`ret 8` at 0x1001ae35), no return value (eax holds the low bound). The two
// arguments are read at 0x1001ae00 and 0x1001ae04 and compared UNSIGNED (`cmp eax, edx` at 0x1001ae08,
// `jbe 0x1001ae13` at 0x1001ae0b): when the first is greater the pair is exchanged through esi
// (0x1001ae0d-0x1001ae11), so the lower value always ends in eax. The low bound is written twice, to [ecx+0x208]
// (0x1001ae19) and to the play cursor [ecx+0x20c] (0x1001ae2e); the high bound goes to [ecx+0x204] (0x1001ae22);
// and bit 0 of [ecx+0x214] is set (`or esi, 1` at 0x1001ae1f, stored at 0x1001ae28) with the other 31 bits kept.
typedef void(__fastcall* SeqSetPlayRange_t)(void*, void*, unsigned, unsigned);
SeqSetPlayRange_t SeqSetPlayRange_orig;
void __fastcall SeqSetPlayRange_re(void* self, void*, unsigned a, unsigned b) {
    unsigned lo = a, hi = b;
    if (lo > hi) {
        lo = b;
        hi = a;
    }
    fld<unsigned>(self, 0x208) = lo;
    fld<unsigned>(self, 0x204) = hi;
    fld<unsigned>(self, 0x214) = fld<unsigned>(self, 0x214) | 1u;
    fld<unsigned>(self, 0x20c) = lo;
}

// 0x0001adc0  Midi_advanceMsgRing (VA 0x1001adc0)
// __thiscall with no stack argument (`ret` at 0x1001ade7) and no return value (eax holds the pre-increment index).
// The write index [ecx+0x20c] (read at 0x1001adc0) is incremented (`lea edx, [eax+1]` at 0x1001adcd) and stored
// unconditionally (0x1001add2), then compared UNSIGNED with the ring's end [ecx+0x204] (read at 0x1001adc7,
// `cmp edx, esi` at 0x1001add0, `jbe 0x1001add9` at 0x1001add9): only when the incremented index is above the end
// is it replaced with the ring's start [ecx+0x208] (0x1001addb-0x1001ade1). The increment wraps modulo 2^32, so an
// index of 0xffffffff becomes 0 and compares below any end value.
typedef void(__fastcall* MidiAdvanceRing_t)(void*, void*);
MidiAdvanceRing_t MidiAdvanceRing_orig;
void __fastcall MidiAdvanceRing_re(void* self, void*) {
    unsigned next = fld<unsigned>(self, 0x20c) + 1u;
    unsigned end = fld<unsigned>(self, 0x204);
    fld<unsigned>(self, 0x20c) = next;
    if (next > end) fld<unsigned>(self, 0x20c) = fld<unsigned>(self, 0x208);
}

// 0x0001ab60  Sound_initEntry (VA 0x1001ab60)
// __thiscall with no stack argument (`ret` at 0x1001aba2), no conditional branch, returns the object
// (`mov eax, edx` at 0x1001ab9f). `rep stosd` with a count of 0x80 from the object + 4 (`lea edi, [edx+4]` at
// 0x1001ab66, `mov ecx, 0x80` at 0x1001ab69, `xor eax, eax` at 0x1001ab6e, `rep stosd` at 0x1001ab72) zeroes bytes
// 4 .. 0x203, and [edx] is zeroed separately at 0x1001ab70. Then five more dwords are zeroed: 0x214 (through
// `lea eax, [edx+0x214]` at 0x1001ab74 and the store at 0x1001ab7b), 0x204 (0x1001ab7d), 0x208 (0x1001ab83),
// 0x20c (0x1001ab89) and 0x210 (0x1001ab8f), so bytes 0 .. 0x217 all end as zero; the dword at 0x218 is set to
// 0xfa (0x1001ab95) and nothing at or beyond 0x21c is touched.
typedef void*(__fastcall* SoundInitEntry_t)(void*, void*);
SoundInitEntry_t SoundInitEntry_orig;
void* __fastcall SoundInitEntry_re(void* self, void*) {
    unsigned* p = reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + 4);
    for (int n = 0x80; n != 0; --n) *p++ = 0;
    fld<unsigned>(self, 0) = 0;
    fld<unsigned>(self, 0x214) = 0;
    fld<unsigned>(self, 0x204) = 0;
    fld<unsigned>(self, 0x208) = 0;
    fld<unsigned>(self, 0x20c) = 0;
    fld<unsigned>(self, 0x210) = 0;
    fld<unsigned>(self, 0x218) = 0xfa;
    return self;
}

// 0x00025010  NodeList_initNode (VA 0x10025010)
// __thiscall, one stack argument (`ret 4` at 0x10025026), no conditional branch. Zeroes the two link dwords at
// offset 0 (0x10025016) and 4 (0x1002501c) and stores the argument as the node's payload at offset 8 (read at
// 0x10025012, stored at 0x10025023). Returns the node (`mov eax, ecx` at 0x10025010). The argument is not
// dereferenced.
typedef void*(__fastcall* NodeListInitNode_t)(void*, void*, unsigned);
NodeListInitNode_t NodeListInitNode_orig;
void* __fastcall NodeListInitNode_re(void* self, void*, unsigned payload) {
    fld<unsigned>(self, 0) = 0;
    fld<unsigned>(self, 4) = 0;
    fld<unsigned>(self, 8) = payload;
    return self;
}

// 0x00008810  pairStore (VA 0x10008810)
// __thiscall, two stack arguments (`ret 8` at 0x1000881f), no conditional branch. Stores the first argument
// ([esp+4], read at 0x10008816) at offset 0 (0x1000881a) and the second ([esp+8], read at 0x10008810) at offset 4
// (0x1000881c), and returns the object (`mov eax, ecx` at 0x10008814). Neither argument is dereferenced.
typedef void*(__fastcall* PairStore_t)(void*, void*, unsigned, unsigned);
PairStore_t PairStore_orig;
void* __fastcall PairStore_re(void* self, void*, unsigned a, unsigned b) {
    fld<unsigned>(self, 0) = a;
    fld<unsigned>(self, 4) = b;
    return self;
}

// 0x0000fec0  DsBuffer::hasAuxInterfaces (VA 0x1000fec0)
// __thiscall with no stack argument (`ret` at 0x1000fed0 and 0x1000fed3), read-only. Returns 1 in al
// (`mov al, 1` at 0x1000fed1) when either of the two pointer fields [ecx+0x64] (read at 0x1000fec0,
// `jne 0x1000fec5`) or [ecx+0x68] (read at 0x1000fec7, `jne 0x1000fecc`) is non-zero, and 0 otherwise
// (`xor al, al` at 0x1000fece). Only al is defined on return; neither field is dereferenced.
typedef unsigned char(__fastcall* DsBufHasAux_t)(void*, void*);
DsBufHasAux_t DsBufHasAux_orig;
unsigned char __fastcall DsBufHasAux_re(void* self, void*) {
    if (fld<unsigned>(self, 0x64) != 0) return 1;
    if (fld<unsigned>(self, 0x68) != 0) return 1;
    return 0;
}

// 0x000381f0  Voice_noteOff (VA 0x100381f0)
// __thiscall with no stack argument (`ret` at 0x1003821a and 0x10038241) and no return value. Two doubles of the
// voice record are compared against the constant at 0x1005b498, which is 0.0 in the image
// (8 bytes 00 00 00 00 00 00 00 00 at RVA 0x5b498): [ecx+0x58] (`fld qword` at 0x100381f0, `fcomp qword` at
// 0x100381f3) and [ecx+0xf0] (0x10038200, 0x10038206). Each test is `fnstsw ax` + `test ah, 0x40` +
// `jne` (0x100381f9-0x100381fe and 0x1003820c-0x10038211); ah bit 6 is the x87 C3 flag, which fcomp sets both
// when the operands are equal AND when the comparison is unordered (either operand a NaN), so both of those take
// the jump to 0x1003821b.
// Only when both doubles are non-zero and ordered does the function set bit 3 of the byte at [ecx+0x9c]
// (`or byte [ecx+0x9c], 8` at 0x10038213) and nothing else. Otherwise (0x1003821b onwards) the dword at
// [ecx+0xe4] is zeroed (0x10038221), bit 2 of [ecx+0x9c] is set (`or al, 4` at 0x1003822b, stored at 0x1003822d)
// and bit 0 of [ecx+0x94] is cleared (`and al, 0xfe` at 0x10038239, stored at 0x1003823b).
// The comparison is reproduced on the bit pattern rather than with a C++ `==` so that a NaN takes the same side
// as the original regardless of how the compiler lowers a floating compare.
typedef void(__fastcall* VoiceNoteOff_t)(void*, void*);
VoiceNoteOff_t VoiceNoteOff_orig;
bool c3ar_zeroOrNan(const void* p) {
    unsigned lo = reinterpret_cast<const unsigned*>(p)[0];
    unsigned hi = reinterpret_cast<const unsigned*>(p)[1];
    if ((hi & 0x7ff00000u) == 0x7ff00000u) return ((hi & 0x000fffffu) | lo) != 0;  // NaN: unordered -> C3 set
    return ((hi & 0x7fffffffu) | lo) == 0;                                         // +0.0 or -0.0 -> C3 set
}
void __fastcall VoiceNoteOff_re(void* self, void*) {
    if (!c3ar_zeroOrNan(reinterpret_cast<char*>(self) + 0x58) &&
        !c3ar_zeroOrNan(reinterpret_cast<char*>(self) + 0xf0)) {
        fld<unsigned char>(self, 0x9c) = static_cast<unsigned char>(fld<unsigned char>(self, 0x9c) | 8);
        return;
    }
    fld<unsigned>(self, 0xe4) = 0;
    fld<unsigned char>(self, 0x9c) = static_cast<unsigned char>(fld<unsigned char>(self, 0x9c) | 4);
    fld<unsigned char>(self, 0x94) = static_cast<unsigned char>(fld<unsigned char>(self, 0x94) & 0xfe);
}

// 0x000285a0  nextRandomFloat (VA 0x100285a0)
// __thiscall with no stack argument (`ret` at 0x100285c9), returns a float in st(0). The dword at the object
// itself is a linear-congruential state: state = state * 0x19660d + 0x3c6ef35f (`imul` at 0x100285a3, `add` at
// 0x100285a9) stored back at 0x100285ae. The low 23 bits of the new state become the mantissa of a float with the
// exponent of 1.0 (`and eax, 0x7fffff` at 0x100285b0, `or eax, 0x3f800000` at 0x100285b5), which is written to the
// scratch slot reserved by `push ecx` (0x100285ba), loaded as a single (`fld dword [esp]` at 0x100285be) and
// reduced by the constant at 0x1005b8a8 (`fsub dword` at 0x100285c2), which is 1.0f in the image (bytes
// 00 00 80 3f at RVA 0x5b8a8). The intermediate is in [1.0, 2.0) and the subtrahend is 1.0, so the difference is
// exact at any x87 precision setting and the result is mantissa/2^23 in [0, 1).
typedef float(__fastcall* NextRandomFloat_t)(void*, void*);
NextRandomFloat_t NextRandomFloat_orig;
float __fastcall NextRandomFloat_re(void* self, void*) {
    unsigned state = fld<unsigned>(self, 0) * 0x19660du + 0x3c6ef35fu;
    fld<unsigned>(self, 0) = state;
    unsigned bits = (state & 0x7fffffu) | 0x3f800000u;
    float f;
    *reinterpret_cast<unsigned*>(&f) = bits;
    return f - 1.0f;
}

// 0x0001e1c0  Seq_rewind (VA 0x1001e1c0)
// __thiscall with no stack argument (`ret` at 0x1001e20a), always returns 0 (`xor eax, eax` at 0x1001e207).
// Walks the track list whose head is at [ecx+0x19c] (read at 0x1001e1c3) keeping the walk position in the cursor
// field [ecx+0x1a4] (written at 0x1001e1cb and 0x1001e1f0); each node holds its successor at +4 (0x1001e1eb) and
// its track record at +8 (0x1001e1d3, 0x1001e1f8). For every track record it calls Seq_rewindTracks
// (VA 0x1001e8e0, through the incremental-link thunk at 0x1001e1dc) with the record in ecx. The walk stops on a
// null head (`je 0x1001e1d1`), a null track record on the first node (`je 0x1001e1d8`), a null cursor re-read from
// [ecx+0x1a4] after the call (`je 0x1001e1e9`), a null successor (`je 0x1001e1f6`) or a null track record inside
// the loop (`jne 0x1001e1fd` closes it). Afterwards bits 2, 3, 4 and 7 of the low byte of [ecx+0x4c] are cleared
// (`and al, 0x63` at 0x1001e202, stored at 0x1001e204), which leaves bits 8..31 of the dword unchanged.
// The re-read of the cursor inside the loop means a track record that overlaps the sequence at [ecx+0x1a4] ends
// the walk, which is what the aliased fixture variant exercises.
typedef unsigned(__fastcall* SeqRewind_t)(void*, void*);
SeqRewind_t SeqRewind_orig;
typedef void(__fastcall* SeqRewindTracks_t)(void*, void*);
SeqRewindTracks_t c3ar_rewindTracks;
unsigned __fastcall SeqRewind_re(void* self, void*) {
    if (!c3ar_rewindTracks) {  // sound.dll is relocatable: resolve the callee's RVA against its real base
        HMODULE m = GetModuleHandleA("sound.dll");
        if (!m) return 0;
        c3ar_rewindTracks = reinterpret_cast<SeqRewindTracks_t>(reinterpret_cast<char*>(m) + 0x0001e8e0);
    }
    void* node = fld<void*>(self, 0x19c);
    fld<void*>(self, 0x1a4) = node;
    if (node != 0) {
        void* track = fld<void*>(node, 8);
        if (track != 0) {
            for (;;) {
                c3ar_rewindTracks(track, 0);
                node = fld<void*>(self, 0x1a4);
                if (node == 0) break;
                node = fld<void*>(node, 4);
                fld<void*>(self, 0x1a4) = node;
                if (node == 0) break;
                track = fld<void*>(node, 8);
                if (track == 0) break;
            }
        }
    }
    fld<unsigned>(self, 0x4c) = fld<unsigned>(self, 0x4c) & 0xffffff63u;
    return 0;
}

}  // namespace

SG_HOOK("sound.dll", 0x0001e710, sound_Midi_readVarLen, ReadVarLen_re, ReadVarLen_orig);
SG_HOOK("sound.dll", 0x0001e6e0, sound_byteSwap16, ByteSwap16_re, ByteSwap16_orig);
SG_HOOK("sound.dll", 0x0001e9f0, sound_Track_advancePos, TrackAdvance_re, TrackAdvance_orig);
SG_HOOK("sound.dll", 0x00037760, sound_Voice_setLoopFlag, VoiceSetLoopFlag_re, VoiceSetLoopFlag_orig);
SG_HOOK("sound.dll", 0x000377f0, sound_Channel_broadcastVoiceBit1, ChanBroadcastBit1_re, ChanBroadcastBit1_orig);
SG_HOOK("sound.dll", 0x000382c0, sound_Channel_allocVoiceSlot, ChanAllocVoice_re, ChanAllocVoice_orig);
SG_HOOK("sound.dll", 0x0001ae00, sound_Seq_setPlayRange, SeqSetPlayRange_re, SeqSetPlayRange_orig);
SG_HOOK("sound.dll", 0x0001adc0, sound_Midi_advanceMsgRing, MidiAdvanceRing_re, MidiAdvanceRing_orig);
SG_HOOK("sound.dll", 0x0001ab60, sound_Sound_initEntry, SoundInitEntry_re, SoundInitEntry_orig);
SG_HOOK("sound.dll", 0x00025010, sound_NodeList_initNode, NodeListInitNode_re, NodeListInitNode_orig);
SG_HOOK("sound.dll", 0x00008810, sound_pairStore, PairStore_re, PairStore_orig);
SG_HOOK("sound.dll", 0x0000fec0, sound_DsBuffer_hasAuxInterfaces, DsBufHasAux_re, DsBufHasAux_orig);
SG_HOOK("sound.dll", 0x000381f0, sound_Voice_noteOff, VoiceNoteOff_re, VoiceNoteOff_orig);
SG_HOOK("sound.dll", 0x000285a0, sound_nextRandomFloat, NextRandomFloat_re, NextRandomFloat_orig);
SG_HOOK("sound.dll", 0x0001e1c0, sound_Seq_rewind, SeqRewind_re, SeqRewind_orig);
