// C3 batch c3an (2026-10-08) of sound.dll: pure helpers (ring buffers, list walks, bit fields, the LCG).
// Addresses in SG_HOOK and in the `// 0x...` comments are RVAs; the instruction addresses quoted inside each body
// are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio and re/analysis/util. sound.dll is LoadLibrary'd after the shim starts, so
// these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// Every function here is a leaf: no callee, no import, no DirectSound/winmm call, no x87 arithmetic.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fld(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

// 0x0000b030  getSpanLength (VA 0x1000b030)
// Reads two dwords of the object in ecx, [ecx+0xc] at 0x1000b030 and [ecx+8] at 0x1000b033, subtracts them
// (sub eax, edx at 0x1000b036) and adds one (inc eax at 0x1000b038). `ret` at 0x1000b039 pops no stack argument:
// __thiscall with no argument. The subtraction and the increment wrap modulo 2^32, so they are written unsigned.
typedef int(__fastcall* SpanLength_t)(void*, void*);
SpanLength_t SpanLength_orig;
int __fastcall SpanLength_re(void* self, void*) {
    unsigned end = fld<unsigned>(self, 0xc);
    unsigned start = fld<unsigned>(self, 8);
    return static_cast<int>(end - start + 1u);
}

// 0x000088c0  CommandQueue::at (VA 0x100088c0)
// Loads the stack argument ([esp+4] at 0x100088c0), masks it with 0xfff (and eax, 0xfff at 0x100088c4) and returns
// the dword at this + index*4 (mov eax, [ecx+eax*4] at 0x100088c9). `ret 4` at 0x100088cc: __thiscall, one argument.
// The queue array starts at the object itself (offset 0) and holds 0x1000 dwords.
typedef unsigned(__fastcall* CmdQueueAt_t)(void*, void*, unsigned);
CmdQueueAt_t CmdQueueAt_orig;
unsigned __fastcall CmdQueueAt_re(void* self, void*, unsigned index) {
    return fld<unsigned>(self, (index & 0xfff) * 4);
}

// 0x00008860  CommandQueue::init (VA 0x10008860)
// Zeroes 0x1000 dwords from the object itself (mov ecx, 0x1000 at 0x10008863, xor eax, eax at 0x10008868,
// lea/mov edi, edx at 0x1000886a, rep stosd at 0x1000886c = bytes 0 .. 0x3fff), then zeroes the two dwords that
// follow the array: [edx+0x4004] at 0x1000886e and [edx+0x4000] at 0x10008874, in that order. Returns the object
// (mov eax, edx at 0x1000887a). `ret` at 0x1000887d: __thiscall with no argument.
typedef void*(__fastcall* CmdQueueInit_t)(void*, void*);
CmdQueueInit_t CmdQueueInit_orig;
void* __fastcall CmdQueueInit_re(void* self, void*) {
    unsigned* p = reinterpret_cast<unsigned*>(self);
    for (int n = 0x1000; n != 0; --n) *p++ = 0;
    fld<unsigned>(self, 0x4004) = 0;
    fld<unsigned>(self, 0x4000) = 0;
    return self;
}

// 0x0000c3d0  WaveCmdQueue::pushValue (VA 0x1000c3d0)
// Ring buffer of 0x1000 dwords at +0x1d8, write index at +0x41dc (read at 0x1000c3d0), read index at +0x41d8
// (read at 0x1000c3d6). The next write index is (write + 1) & 0xfff (inc eax at 0x1000c3dc, and eax, 0xfff at
// 0x1000c3dd); when it equals the read index (cmp at 0x1000c3e2, jne at 0x1000c3e4) the queue is full and the
// function returns 0x22 (mov eax, 0x22 at 0x1000c3e6) without writing anything. Otherwise the stack argument
// ([esp+4] at 0x1000c3ee) is stored at [ecx+index*4+0x1d8] (0x1000c3f2), the write index is updated (0x1000c3f9)
// and 0 is returned (xor eax, eax at 0x1000c3ff). `ret 4` at 0x1000c3eb and 0x1000c401: __thiscall, one argument.
typedef unsigned(__fastcall* WaveCmdPush_t)(void*, void*, unsigned);
WaveCmdPush_t WaveCmdPush_orig;
unsigned __fastcall WaveCmdPush_re(void* self, void*, unsigned value) {
    unsigned next = (fld<unsigned>(self, 0x41dc) + 1) & 0xfff;
    if (next == fld<unsigned>(self, 0x41d8)) return 0x22;
    fld<unsigned>(self, 0x1d8 + next * 4) = value;
    fld<unsigned>(self, 0x41dc) = next;
    return 0;
}

// 0x00028560  randRange (VA 0x10028560)
// Linear-congruential step on the dword at the object itself: state = state * 0x19660d + 0x3c6ef35f
// (imul at 0x10028562, add at 0x10028568), stored back at 0x1002856d. The result is the high half of the new state
// (shr eax, 0x10 at 0x10028573) multiplied by the low 16 bits of the argument (and ecx, 0xffff at 0x10028576,
// imul eax, ecx at 0x1002857c, a 32-bit multiply that keeps the low dword) and shifted right 16 (0x1002857f).
// `ret 4` at 0x10028582: __thiscall, one argument.
typedef unsigned(__fastcall* RandRange_t)(void*, void*, unsigned);
RandRange_t RandRange_orig;
unsigned __fastcall RandRange_re(void* self, void*, unsigned n) {
    unsigned state = fld<unsigned>(self, 0) * 0x19660du + 0x3c6ef35fu;
    fld<unsigned>(self, 0) = state;
    return ((state >> 16) * (n & 0xffffu)) >> 16;
}

// 0x0001bb80  Seq_firstTrackData (VA 0x1001bb80)
// Reads the list head at [ecx+0xc] (0x1001bb80) and stores it in the cursor field [ecx+0x14] (0x1001bb85), which
// happens on both paths because the store precedes the branch (test at 0x1001bb83, je at 0x1001bb88). With a null
// head it returns 0 (xor eax, eax at 0x1001bb8e); otherwise it returns the node's payload pointer [node+8]
// (0x1001bb8a). `ret` at 0x1001bb8d and 0x1001bb90: __thiscall with no argument.
typedef unsigned(__fastcall* SeqFirstTrackData_t)(void*, void*);
SeqFirstTrackData_t SeqFirstTrackData_orig;
unsigned __fastcall SeqFirstTrackData_re(void* self, void*) {
    void* node = fld<void*>(self, 0xc);
    fld<void*>(self, 0x14) = node;
    if (node == 0) return 0;
    return fld<unsigned>(node, 8);
}

// 0x0000f0c0  DsBuffer::setBufferSize (VA 0x1000f0c0)
// Guard first: when the dword at [ecx+0x60] is non-zero (read at 0x1000f0c0, test at 0x1000f0c3, je at 0x1000f0c5)
// the function returns 0xc (mov eax, 0xc at 0x1000f0c7) and writes nothing. Otherwise the stack argument
// ([esp+4] at 0x1000f0cf) is stored at [ecx+0x78] (0x1000f0d3) and 0 is returned (xor eax, eax at 0x1000f0d6).
// `ret 4` at 0x1000f0cc and 0x1000f0d8: __thiscall, one argument.
typedef unsigned(__fastcall* DsBufSetSize_t)(void*, void*, unsigned);
DsBufSetSize_t DsBufSetSize_orig;
unsigned __fastcall DsBufSetSize_re(void* self, void*, unsigned size) {
    if (fld<unsigned>(self, 0x60) != 0) return 0xc;
    fld<unsigned>(self, 0x78) = size;
    return 0;
}

// 0x00024c50  Voice_setPlayModeBits (VA 0x10024c50)
// Only the low byte of the stack argument is read (mov al, [esp+4] at 0x10024c50). The three tests are an ordered
// chain, each returning: test al, 1 / je at 0x10024c54-0x10024c56, test al, 2 / je at 0x10024c6d-0x10024c6f,
// test al, 4 / je at 0x10024c86-0x10024c88. Each arm rewrites bits 9..11 of the dword at [ecx+0x214]: bit 1 keeps
// bit 9 (and ah, 0xf3 at 0x10024c5e clears bits 10 and 11, or ah, 2 at 0x10024c61 sets bit 9), bit 2 keeps bit 10
// (and dh, 0xf5 at 0x10024c77, or dh, 4 at 0x10024c7a), bit 4 keeps bit 11 (and ah, 0xf9 at 0x10024c90,
// or ah, 8 at 0x10024c93). With none of the three bits set nothing is written. `ret 4` at 0x10024c6a, 0x10024c83
// and 0x10024c9c: __thiscall, one argument.
typedef void(__fastcall* VoiceSetPlayMode_t)(void*, void*, unsigned);
VoiceSetPlayMode_t VoiceSetPlayMode_orig;
void __fastcall VoiceSetPlayMode_re(void* self, void*, unsigned mode) {
    unsigned char m = static_cast<unsigned char>(mode);
    if (m & 1) {
        fld<unsigned>(self, 0x214) = (fld<unsigned>(self, 0x214) & 0xfffff3ffu) | 0x200u;
        return;
    }
    if (m & 2) {
        fld<unsigned>(self, 0x214) = (fld<unsigned>(self, 0x214) & 0xfffff5ffu) | 0x400u;
        return;
    }
    if (m & 4) {
        fld<unsigned>(self, 0x214) = (fld<unsigned>(self, 0x214) & 0xfffff9ffu) | 0x800u;
    }
}

// 0x0002ceb0  VoiceSlot_init (VA 0x1002ceb0)
// Zeroes 0x41 dwords starting at +0xc (mov ecx, 0x41 at 0x1002ceb3, lea edi, [edx+0xc] at 0x1002ceba,
// rep stosd at 0x1002cebd = bytes 0xc .. 0x10f), then writes the first three dwords: [edx+4] = 0 at 0x1002cebf,
// [edx] = 0 at 0x1002cec2 and [edx+8] = 1 at 0x1002cec4. Returns the object (mov eax, edx at 0x1002cecb).
// `ret` at 0x1002cece: __thiscall with no argument.
typedef void*(__fastcall* VoiceSlotInit_t)(void*, void*);
VoiceSlotInit_t VoiceSlotInit_orig;
void* __fastcall VoiceSlotInit_re(void* self, void*) {
    unsigned* p = reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + 0xc);
    for (int n = 0x41; n != 0; --n) *p++ = 0;
    fld<unsigned>(self, 4) = 0;
    fld<unsigned>(self, 0) = 0;
    fld<unsigned>(self, 8) = 1;
    return self;
}

// 0x00037720  Channel_setStreamSource (VA 0x10037720)
// Stores the two stack arguments ([esp+4] at 0x10037720, [esp+8] at 0x10037724) at [ecx+0x13a0] (0x10037728) and
// [ecx+0x13a4] (0x10037733), and clears bit 2 of the dword at [ecx+0x58] (read at 0x1003772e, and al, 0xfb at
// 0x10037731, stored back at 0x10037739). Neither argument is dereferenced. Always returns 0 (xor eax, eax at
// 0x1003773c). `ret 8` at 0x1003773e: __thiscall, two arguments.
typedef unsigned(__fastcall* ChannelSetStreamSource_t)(void*, void*, unsigned, unsigned);
ChannelSetStreamSource_t ChannelSetStreamSource_orig;
unsigned __fastcall ChannelSetStreamSource_re(void* self, void*, unsigned source, unsigned backref) {
    fld<unsigned>(self, 0x13a0) = source;
    fld<unsigned>(self, 0x13a4) = backref;
    fld<unsigned>(self, 0x58) = fld<unsigned>(self, 0x58) & 0xfffffffbu;
    return 0;
}

// 0x00038400  Channel_clearBackref (VA 0x10038400)
// Reads the back pointer at [ecx+0x13a4] (0x10038400); when it is null (test at 0x10038406, je at 0x10038408)
// nothing happens. Otherwise it zeroes the dword it points at (mov [eax], 0 at 0x1003840a) and then zeroes the
// field itself (0x10038410). `ret` at 0x1003841a: __thiscall with no argument.
typedef void(__fastcall* ChannelClearBackref_t)(void*, void*);
ChannelClearBackref_t ChannelClearBackref_orig;
void __fastcall ChannelClearBackref_re(void* self, void*) {
    unsigned* back = fld<unsigned*>(self, 0x13a4);
    if (back != 0) {
        *back = 0;
        fld<unsigned*>(self, 0x13a4) = 0;
    }
}

// 0x0001f150  Seq_clearTrackFlag8All (VA 0x1001f150)
// Walks the node list whose head is at [ecx+0x19c] (read at 0x1001f150) and keeps the walk position in the cursor
// field [ecx+0x1a4] (written at 0x1001f159 and 0x1001f181). Each node holds its successor at +4 (0x1001f17c) and a
// track record at +8 (0x1001f161, 0x1001f189); the body clears bit 3 of the track's flag byte at +0x38
// (mov dl, 0xf7 at 0x1001f168, read/and/store at 0x1001f16a-0x1001f16f). The walk ends on a null head
// (je at 0x1001f15f), a null track record (je at 0x1001f166 before the loop, jne at 0x1001f18e inside it), a null
// cursor re-read from [ecx+0x1a4] (je at 0x1001f17a) or a null successor (je at 0x1001f187).
// `ret` at 0x1001f191: __thiscall with no argument.
typedef void(__fastcall* SeqClearTrackFlag8All_t)(void*, void*);
SeqClearTrackFlag8All_t SeqClearTrackFlag8All_orig;
void __fastcall SeqClearTrackFlag8All_re(void* self, void*) {
    void* node = fld<void*>(self, 0x19c);
    fld<void*>(self, 0x1a4) = node;
    if (node == 0) return;
    void* track = fld<void*>(node, 8);
    if (track == 0) return;
    for (;;) {
        fld<unsigned char>(track, 0x38) = static_cast<unsigned char>(fld<unsigned char>(track, 0x38) & 0xf7);
        node = fld<void*>(self, 0x1a4);
        if (node == 0) return;
        node = fld<void*>(node, 4);
        fld<void*>(self, 0x1a4) = node;
        if (node == 0) return;
        track = fld<void*>(node, 8);
        if (track == 0) return;
    }
}

}  // namespace

SG_HOOK("sound.dll", 0x0000b030, sound_getSpanLength, SpanLength_re, SpanLength_orig);
SG_HOOK("sound.dll", 0x000088c0, sound_CommandQueue_at, CmdQueueAt_re, CmdQueueAt_orig);
SG_HOOK("sound.dll", 0x00008860, sound_CommandQueue_init, CmdQueueInit_re, CmdQueueInit_orig);
SG_HOOK("sound.dll", 0x0000c3d0, sound_WaveCmdQueue_pushValue, WaveCmdPush_re, WaveCmdPush_orig);
SG_HOOK("sound.dll", 0x00028560, sound_randRange, RandRange_re, RandRange_orig);
SG_HOOK("sound.dll", 0x0001bb80, sound_Seq_firstTrackData, SeqFirstTrackData_re, SeqFirstTrackData_orig);
SG_HOOK("sound.dll", 0x0000f0c0, sound_DsBuffer_setBufferSize, DsBufSetSize_re, DsBufSetSize_orig);
SG_HOOK("sound.dll", 0x00024c50, sound_Voice_setPlayModeBits, VoiceSetPlayMode_re, VoiceSetPlayMode_orig);
SG_HOOK("sound.dll", 0x0002ceb0, sound_VoiceSlot_init, VoiceSlotInit_re, VoiceSlotInit_orig);
SG_HOOK("sound.dll", 0x00037720, sound_Channel_setStreamSource, ChannelSetStreamSource_re, ChannelSetStreamSource_orig);
SG_HOOK("sound.dll", 0x00038400, sound_Channel_clearBackref, ChannelClearBackref_re, ChannelClearBackref_orig);
SG_HOOK("sound.dll", 0x0001f150, sound_Seq_clearTrackFlag8All, SeqClearTrackFlag8All_re, SeqClearTrackFlag8All_orig);
