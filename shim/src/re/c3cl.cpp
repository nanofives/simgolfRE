// C3 batch c3cl (2026-10-09) of sound.dll: nine audio functions that were never offered to an earlier batch
// (log/c3/c3cl_candidates.tsv) - the sequencer track finder of the Sound/Seq vtables and its one callee, the
// open/close inner-device slots of the three device base vtables 0x1005b914 / 0x1005b9e4 / 0x1005ba5c, and the
// playback-state and flag slots (23 and 27) of the channel vtable 0x1005bb90.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside
// each body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// Nothing is allocated or freed and no sound.dll module global is read or written: every sequencer, track
// list, device, channel and inner sub-object the A/B passes in is allocated by the fixture, and every vtable
// the bodies dispatch through belongs to one of those synthetic objects.
// ONE import is reached, by one body: getPlaybackTime 0x1002baa0 calls timeGetTime through sound.dll's own
// import slot [0x100b8654] (WINMM.dll timeGetTime, read from the module's import table). The call has no side
// effect, and every vector of that key is built so that the comparison the body makes with the result is
// decided by the fixture's own fields and not by the clock (see re/frida/registry.d/c3cl.py).
// Seq_findTrackById forwards to its callee 0x1002e430 at that address, so with both hooks linked the
// "original" arm would run this file's reimplementation of the callee: that key is also run with
// SIMGOLF_HOOKS_OFF=sound.dll:2e430.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T fatc(const void* base, int off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + off);
}

// sound.dll has base relocations, so an original callee and the module's own import slot are addressed
// through the loaded base.
char* c3cl_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

// Shapes of the vtable slots these bodies dispatch through.
typedef void(__fastcall* SlotVoid0_t)(void*, void*);              // ecx = object, nothing pushed
typedef int(__fastcall* SlotInt0_t)(void*, void*);                // ecx = object, nothing pushed, result
typedef int(__fastcall* SlotInt1_t)(void*, void*, int);           // ecx = object, one pushed argument
typedef int(__fastcall* SlotInt2_t)(void*, void*, int, int);      // ecx = object, two pushed arguments
typedef void(__fastcall* SlotVoid1_t)(void*, void*, unsigned);    // ecx = object, one pushed argument

// The callee Seq_findTrackById calls, at its own address.
typedef void(__fastcall* TrackSetField8_t)(void*, void*, unsigned);   // 0x1002e430

// WINMM.dll timeGetTime, through sound.dll's own import slot (RVA 0xb8654).
typedef DWORD(__stdcall* TimeGetTime_t)();

// ---------------------------------------------------------------------------------------------------------
// sequencer: the track list at Seq+0x218 (Sound vtable 0x1005b000 slot 40, Seq vtable 0x1005b6a8 slot 40)
// ---------------------------------------------------------------------------------------------------------

// 0x0002e430  Track_setField8RaisingMax (VA 0x1002e430)
// __thiscall with one stack argument (`ret 4` at 0x1002e441); 20 bytes, no callee.
// The argument is read at 0x1002e430 and the dword at this+0xc is read at 0x1002e434. The compare at
// 0x1002e437 is followed by `jae 0x1002e43e`, an UNSIGNED test: this+0xc is only overwritten with the
// argument when it is strictly below it (0x1002e43b), so this+0xc keeps the largest value ever passed.
// this+8 is then stored unconditionally (0x1002e43e).
// No value is loaded into eax before the return, so the body has no return value: eax still holds the
// argument, which `mov eax, dword ptr [esp + 4]` at 0x1002e430 put there.
// What the two fields count is not determined by this body; its one caller is Seq_findTrackById 0x10023080,
// which calls it on the track element whose id at +4 matched, with its own second argument.
TrackSetField8_t TrackSetField8RaisingMax_orig;
void __fastcall TrackSetField8RaisingMax_re(void* self, void*, unsigned value) {
    if (fatc<unsigned>(self, 0xc) < value) {     // cmp / jae at 0x1002e437, 0x1002e439
        fat<unsigned>(self, 0xc) = value;        // 0x1002e43b
    }
    fat<unsigned>(self, 8) = value;              // 0x1002e43e
}

// 0x00023080  Seq_findTrackById (VA 0x10023080)
// __thiscall with two stack arguments (`ret 8` at 0x1002308f, 0x100230db, 0x100230ec): the track id and the
// value handed to the callee.
// The dword at this+0x224 gates the whole body: when it is zero the function returns SNDERR 0x24 without
// touching anything (`test eax, eax` / `jne 0x10023092` at 0x10023086, `mov eax, 0x24` at 0x1002308a).
// The list head at this+0x218 is read (0x10023092) and stored into the cursor this+0x220 (0x1002309a)
// BEFORE it is tested (`je 0x100230d6` at 0x100230a0), so the cursor is overwritten even with an empty list.
// A node is {+4 next, +8 element}, the layout Seq_listGetAt 0x1001d780 walks. The element of the current
// node is read at 0x100230a2 and a null element ends the walk with 0x24 (0x100230a7).
// The loop compares the element's dword at +4 with the id (`cmp dword ptr [eax + 4], edx` at 0x100230ad).
// On a miss the cursor is re-read from this+0x220 (0x100230b2), its +4 is taken as the next node
// (0x100230bc), stored back into the cursor (0x100230c1) and tested (0x100230c7), and the next element is
// read and tested (0x100230c9, 0x100230ce) before the compare is repeated (`jmp 0x100230ad` at 0x100230d0).
// On a hit the element is re-tested at 0x100230d2; it was proved non-null at 0x100230a7 or 0x100230ce one
// iteration earlier, so the fall-through to the 0x24 return at 0x100230d6 is not reachable.
// The found element is then handed to 0x1002e430 with the second argument pushed (`mov ecx, [esp+8]` /
// `push ecx` / `mov ecx, eax` at 0x100230de-0x100230e3, `call 0x1000136b` at 0x100230e5, an incremental-link
// thunk to 0x1002e430), and the function returns 0 (0x100230ea).
typedef int(__fastcall* SeqFindTrackById_t)(void*, void*, int, int);
SeqFindTrackById_t SeqFindTrackById_orig;
int __fastcall SeqFindTrackById_re(void* self, void*, int id, int value) {
    if (fatc<int>(self, 0x224) == 0) return 0x24;             // 0x10023080-0x1002308a
    void* node = fatc<void*>(self, 0x218);                    // 0x10023092
    fat<void*>(self, 0x220) = node;                           // 0x1002309a
    if (!node) return 0x24;                                   // je 0x100230d6 at 0x100230a0
    void* elem = fatc<void*>(node, 8);                        // 0x100230a2
    if (!elem) return 0x24;                                   // je 0x100230d6 at 0x100230a7
    while (fatc<int>(elem, 4) != id) {                        // 0x100230ad, je 0x100230d2 at 0x100230b0
        void* cursor = fatc<void*>(self, 0x220);              // 0x100230b2
        if (!cursor) return 0x24;                             // je 0x100230d6 at 0x100230ba
        void* next = fatc<void*>(cursor, 4);                  // 0x100230bc
        fat<void*>(self, 0x220) = next;                       // 0x100230c1
        if (!next) return 0x24;                               // je 0x100230d6 at 0x100230c7
        elem = fatc<void*>(next, 8);                          // 0x100230c9
        if (!elem) return 0x24;                               // je 0x100230d6 at 0x100230ce
    }
    // 0x100230d2: `test eax, eax` on the element, proved non-null above; the 0x24 arm is unreachable.
    reinterpret_cast<TrackSetField8_t>(c3cl_sound(0x2e430))(elem, 0, static_cast<unsigned>(value));
    return 0;                                                 // 0x100230ea
}

// ---------------------------------------------------------------------------------------------------------
// device bases: the inner device at this+0x14 (vtables 0x1005b914, 0x1005b9e4, 0x1005ba5c)
// ---------------------------------------------------------------------------------------------------------
//
// Three classes share one pair of open/close bodies around an inner device held at this+0x14. The close
// bodies call the inner's own vtable slot +0x14 with the inner in ecx and nothing pushed; the open bodies
// call this' slot +4 first, then the inner's slot +0x10 with two pushed arguments, then this' slot +8.

// 0x00029320  VoiceBase_closeInner (VA 0x10029320)
// __thiscall with no stack argument (`ret` at 0x10029339). The inner pointer at this+0x14 is read at
// 0x10029323 and, when it is non-null (`je 0x10029336` at 0x10029328), its vtable is read at 0x1002932a and
// slot +0x14 is called with the inner still in ecx and nothing pushed (0x1002932c). this+0x14 is cleared
// AFTER that call (0x1002932f). Returns 0 (`xor eax, eax` at 0x10029336).
typedef int(__fastcall* CloseInner_t)(void*, void*);
CloseInner_t VoiceBaseCloseInner_orig;
int __fastcall VoiceBaseCloseInner_re(void* self, void*) {
    void* inner = fatc<void*>(self, 0x14);                        // 0x10029323
    if (inner) {                                                  // je 0x10029336 at 0x10029328
        void** ivt = *reinterpret_cast<void***>(inner);            // 0x1002932a
        reinterpret_cast<SlotVoid0_t>(ivt[0x14 / 4])(inner, 0);    // call at 0x1002932c
        fat<void*>(self, 0x14) = 0;                                // 0x1002932f
    }
    return 0;                                                      // 0x10029336
}

// 0x0002a000  MidiDeviceBase_closeInner (VA 0x1002a000)
// Byte-for-byte the same body as VoiceBase_closeInner 0x10029320 at its own address: the inner pointer is
// read at 0x1002a003, tested at 0x1002a008 (`je 0x1002a016`), its vtable slot +0x14 is called at 0x1002a00c
// and this+0x14 is cleared at 0x1002a00f. Returns 0 (0x1002a016).
CloseInner_t MidiDeviceBaseCloseInner_orig;
int __fastcall MidiDeviceBaseCloseInner_re(void* self, void*) {
    void* inner = fatc<void*>(self, 0x14);                        // 0x1002a003
    if (inner) {                                                  // je 0x1002a016 at 0x1002a008
        void** ivt = *reinterpret_cast<void***>(inner);            // 0x1002a00a
        reinterpret_cast<SlotVoid0_t>(ivt[0x14 / 4])(inner, 0);    // call at 0x1002a00c
        fat<void*>(self, 0x14) = 0;                                // 0x1002a00f
    }
    return 0;                                                      // 0x1002a016
}

// 0x0002a470  WaveInDeviceBase_closeInner (VA 0x1002a470)
// The same shape with one extra call: after the inner's slot +0x14 (0x1002a47c) the body re-reads THIS
// object's vtable (0x1002a47f, after the inner call, so a slot the inner call swapped would be the one
// taken) and calls its slot +8 with this in ecx and nothing pushed (0x1002a481, 0x1002a483). this+0x14 is
// cleared last (0x1002a486). Returns 0 (0x1002a48d).
CloseInner_t WaveInDeviceBaseCloseInner_orig;
int __fastcall WaveInDeviceBaseCloseInner_re(void* self, void*) {
    void* inner = fatc<void*>(self, 0x14);                        // 0x1002a473
    if (inner) {                                                  // je 0x1002a48d at 0x1002a478
        void** ivt = *reinterpret_cast<void***>(inner);            // 0x1002a47a
        reinterpret_cast<SlotVoid0_t>(ivt[0x14 / 4])(inner, 0);    // call at 0x1002a47c
        void** vt = *reinterpret_cast<void***>(self);              // 0x1002a47f
        reinterpret_cast<SlotVoid0_t>(vt[8 / 4])(self, 0);         // call at 0x1002a483
        fat<void*>(self, 0x14) = 0;                                // 0x1002a486
    }
    return 0;                                                      // 0x1002a48d
}

// 0x00029f90  MidiDeviceBase_openInner (VA 0x10029f90)
// __thiscall with two stack arguments (`ret 8` at 0x10029fc8 and 0x10029fcf). The second argument is loaded
// into edi at 0x10029f94.
// This object's own vtable slot +4 is called FIRST with the second argument pushed (`mov eax, [esi]` /
// `push edi` / `call dword ptr [eax + 4]` at 0x10029f98-0x10029f9b); a non-zero result is returned straight
// away and nothing else happens (`jne 0x10029fcd` at 0x10029fa0).
// Otherwise the inner device at this+0x14 (0x10029fa2) is opened through its vtable slot +0x10 with both
// arguments pushed, the second first so that the first argument is the one nearest the top of the stack
// (`push edi` / `push eax` at 0x10029fa9, 0x10029faa, `call dword ptr [edx + 0x10]` at 0x10029fad).
// A zero result from the inner is the success path: the body returns 0 (`je 0x10029fcb` at 0x10029fb4,
// `xor eax, eax`). A NON-ZERO result is the failure path: this' own vtable is re-read (0x10029fb6), its slot
// +8 is called with this in ecx and nothing pushed (0x10029fba), this+0x14 is CLEARED (0x10029fbf, after the
// call) and the inner's non-zero result is returned (0x10029fbd).
typedef int(__fastcall* OpenInner_t)(void*, void*, int, int);
OpenInner_t MidiDeviceBaseOpenInner_orig;
int __fastcall MidiDeviceBaseOpenInner_re(void* self, void*, int a, int b) {
    void** vt = *reinterpret_cast<void***>(self);                 // 0x10029f98
    int rc = reinterpret_cast<SlotInt1_t>(vt[4 / 4])(self, 0, b);  // call at 0x10029f9b
    if (rc) return rc;                                             // jne 0x10029fcd at 0x10029fa0
    void* inner = fatc<void*>(self, 0x14);                         // 0x10029fa2
    void** ivt = *reinterpret_cast<void***>(inner);                // 0x10029fab
    int rc2 = reinterpret_cast<SlotInt2_t>(ivt[0x10 / 4])(inner, 0, a, b);   // call at 0x10029fad
    if (!rc2) return 0;                                            // je 0x10029fcb at 0x10029fb4
    void** vt2 = *reinterpret_cast<void***>(self);                 // 0x10029fb6
    reinterpret_cast<SlotVoid0_t>(vt2[8 / 4])(self, 0);            // call at 0x10029fba
    fat<void*>(self, 0x14) = 0;                                    // 0x10029fbf
    return rc2;                                                    // 0x10029fbd
}

// 0x0002a420  WaveInDeviceBase_openInner (VA 0x1002a420)
// The same sequence at its own address - this' slot +4 with the second argument (0x1002a42b), a non-zero
// result returned straight away (`jne 0x1002a456` at 0x1002a430), the inner at this+0x14 (0x1002a432) opened
// through its slot +0x10 with both arguments (0x1002a43d), a zero result returning 0 (`je 0x1002a454` at
// 0x1002a444) and a non-zero result calling this' slot +8 (0x1002a44a) and returning the code (0x1002a44d) -
// WITHOUT the clear of this+0x14 that MidiDeviceBase_openInner 0x10029f90 makes at 0x10029fbf: there is no
// store into the object anywhere in this body.
OpenInner_t WaveInDeviceBaseOpenInner_orig;
int __fastcall WaveInDeviceBaseOpenInner_re(void* self, void*, int a, int b) {
    void** vt = *reinterpret_cast<void***>(self);                 // 0x1002a428
    int rc = reinterpret_cast<SlotInt1_t>(vt[4 / 4])(self, 0, b);  // call at 0x1002a42b
    if (rc) return rc;                                             // jne 0x1002a456 at 0x1002a430
    void* inner = fatc<void*>(self, 0x14);                         // 0x1002a432
    void** ivt = *reinterpret_cast<void***>(inner);                // 0x1002a43b
    int rc2 = reinterpret_cast<SlotInt2_t>(ivt[0x10 / 4])(inner, 0, a, b);   // call at 0x1002a43d
    if (!rc2) return 0;                                            // je 0x1002a454 at 0x1002a444
    void** vt2 = *reinterpret_cast<void***>(self);                 // 0x1002a446
    reinterpret_cast<SlotVoid0_t>(vt2[8 / 4])(self, 0);            // call at 0x1002a44a
    return rc2;                                                    // 0x1002a44d
}

// ---------------------------------------------------------------------------------------------------------
// channel: slots 23 and 27 of the vtable 0x1005bb90, over the device at this+0x40
// ---------------------------------------------------------------------------------------------------------

// 0x0002baa0  getPlaybackTime (VA 0x1002baa0)
// __thiscall with no stack argument (`ret` at 0x1002bad3, 0x1002bade, 0x1002bae2).
// The device pointer at this+0x40 is read at 0x1002baa3. When it is NON-NULL the whole body is that device's
// own vtable slot +0x5c, called with the device in ecx and nothing pushed, and its result is returned
// (`jne 0x1002bad8` at 0x1002baa8, `mov edx, [ecx]` / `call dword ptr [edx + 0x5c]` at 0x1002bad8,
// 0x1002bada).
// With no device the dword at this+0x58 decides: it is shifted right by 4 and bit 0 of the result is tested
// (`shr eax, 4` / `test al, 1` at 0x1002baad, 0x1002bab0), i.e. bit 4 of this+0x58. When that bit is clear
// the body falls into `test ecx, ecx` at 0x1002bad4, which re-tests the device pointer already known to be
// zero, so `je 0x1002badf` is always taken and the result is 0; the `jne` side of 0x1002bad6 is not
// reachable.
// With the bit set the dword at this+0x68 is read (0x1002bab4) and a zero there returns 0 (`je 0x1002badf`
// at 0x1002bab9). Otherwise WINMM's timeGetTime is called through sound.dll's own import slot [0x100b8654]
// (0x1002babb), this+0x68 is re-read (0x1002bac1) and subtracted from it (0x1002bac7), and the difference is
// compared with the dword at this+0x64 (0x1002bac4, `cmp ecx, eax` / `jbe 0x1002badf` at 0x1002bac9,
// 0x1002bacb - UNSIGNED): the body returns 1 only while this+0x64 is strictly above the elapsed count
// (0x1002bacd), and 0 once it is not.
// Nothing is written by any path.
typedef int(__fastcall* GetPlaybackTime_t)(void*, void*);
GetPlaybackTime_t GetPlaybackTime_orig;
int __fastcall GetPlaybackTime_re(void* self, void*) {
    void* device = fatc<void*>(self, 0x40);                        // 0x1002baa3
    if (device) {                                                  // jne 0x1002bad8 at 0x1002baa8
        void** dvt = *reinterpret_cast<void***>(device);           // 0x1002bad8
        return reinterpret_cast<SlotInt0_t>(dvt[0x5c / 4])(device, 0);   // call at 0x1002bada
    }
    unsigned state = fatc<unsigned>(self, 0x58);                   // 0x1002baaa
    if (((state >> 4) & 1) == 0) return 0;                         // 0x1002baad-0x1002bab2, then 0x1002bad4
    if (fatc<unsigned>(self, 0x68) == 0) return 0;                 // 0x1002bab4, je 0x1002badf at 0x1002bab9
    TimeGetTime_t timeGetTime =
        *reinterpret_cast<TimeGetTime_t*>(c3cl_sound(0xb8654));    // the import slot called at 0x1002babb
    unsigned now = timeGetTime();
    unsigned elapsed = now - fatc<unsigned>(self, 0x68);           // re-read at 0x1002bac1, sub at 0x1002bac7
    if (fatc<unsigned>(self, 0x64) <= elapsed) return 0;           // jbe 0x1002badf at 0x1002bacb
    return 1;                                                      // 0x1002bacd
}

// 0x0002c040  Channel_applyFlags (VA 0x1002c040)
// __thiscall with one stack argument (`ret 4` at 0x1002c0a3), a flag word read into eax at 0x1002c040 and
// never modified afterwards.
// edx is set to 1 at 0x1002c044 and is what bit 1 of the argument stores into this+0x30 (`test al, 2` /
// `je 0x1002c050` at 0x1002c049, `mov dword ptr [ecx + 0x30], edx` at 0x1002c04d): the stored value is the
// literal 1, not the argument.
// Seven bits of the argument are then folded into the dword at this+0x58, each with its own
// read-modify-write `or`, so they accumulate:
//   bit 0  (`test dl, al` at 0x1002c050, dl still 1)  -> bit 0    (0x1002c054)
//   bit 2  (`and edx, 4` at 0x1002c059)               -> bit 1    (0x1002c05e)
//   bit 6  (`test al, 0x40` at 0x1002c062)            -> bit 3    (0x1002c066)
//   bit 7  (`test al, 0x80` at 0x1002c06a)            -> bit 4    (0x1002c06e)
//   bit 4  only when bit 2 is CLEAR (`test edx, edx` / `jne 0x1002c07e` at 0x1002c072, `test al, 0x10` at
//          0x1002c076)                                -> bit 2    (0x1002c07a)
//   bit 8  only when bit 2 is CLEAR (0x1002c07e, `test ah, 1` at 0x1002c082)  -> bit 5  (0x1002c087)
//   bit 10 (`test ah, 4` at 0x1002c08b)               -> bit 6    (0x1002c090)
// Finally the device at this+0x40 is read (0x1002c094) and, when it is non-null (`je 0x1002c0a1` at
// 0x1002c099), the UNCHANGED argument is forwarded to that device's own vtable slot +0x6c with the device in
// ecx (`mov edx, [ecx]` / `push eax` / `call dword ptr [edx + 0x6c]` at 0x1002c09b-0x1002c09e).
// Returns 0 (`xor eax, eax` at 0x1002c0a1).
typedef int(__fastcall* ChannelApplyFlags_t)(void*, void*, unsigned);
ChannelApplyFlags_t ChannelApplyFlags_orig;
int __fastcall ChannelApplyFlags_re(void* self, void*, unsigned flags) {
    if (flags & 0x2) fat<int>(self, 0x30) = 1;                     // 0x1002c049, 0x1002c04d
    if (flags & 0x1) fat<unsigned>(self, 0x58) |= 1;               // 0x1002c050, 0x1002c054
    unsigned bit2 = flags & 4;                                     // 0x1002c059
    if (bit2) fat<unsigned>(self, 0x58) |= 2;                      // 0x1002c05e
    if (flags & 0x40) fat<unsigned>(self, 0x58) |= 8;              // 0x1002c066
    if (flags & 0x80) fat<unsigned>(self, 0x58) |= 0x10;           // 0x1002c06e
    if (!bit2 && (flags & 0x10)) fat<unsigned>(self, 0x58) |= 4;   // 0x1002c072-0x1002c07a
    if (!bit2 && (flags & 0x100)) fat<unsigned>(self, 0x58) |= 0x20;   // 0x1002c07e-0x1002c087
    if (flags & 0x400) fat<unsigned>(self, 0x58) |= 0x40;          // 0x1002c08b, 0x1002c090
    void* device = fatc<void*>(self, 0x40);                        // 0x1002c094
    if (device) {                                                  // je 0x1002c0a1 at 0x1002c099
        void** dvt = *reinterpret_cast<void***>(device);           // 0x1002c09b
        reinterpret_cast<SlotVoid1_t>(dvt[0x6c / 4])(device, 0, flags);   // call at 0x1002c09e
    }
    return 0;                                                      // 0x1002c0a1
}

SG_HOOK("sound.dll", 0x0002E430, sound_Track_setField8RaisingMax, TrackSetField8RaisingMax_re,
        TrackSetField8RaisingMax_orig);
SG_HOOK("sound.dll", 0x00023080, sound_Seq_findTrackById, SeqFindTrackById_re, SeqFindTrackById_orig);
SG_HOOK("sound.dll", 0x00029320, sound_VoiceBase_closeInner, VoiceBaseCloseInner_re,
        VoiceBaseCloseInner_orig);
SG_HOOK("sound.dll", 0x0002A000, sound_MidiDeviceBase_closeInner, MidiDeviceBaseCloseInner_re,
        MidiDeviceBaseCloseInner_orig);
SG_HOOK("sound.dll", 0x0002A470, sound_WaveInDeviceBase_closeInner, WaveInDeviceBaseCloseInner_re,
        WaveInDeviceBaseCloseInner_orig);
SG_HOOK("sound.dll", 0x00029F90, sound_MidiDeviceBase_openInner, MidiDeviceBaseOpenInner_re,
        MidiDeviceBaseOpenInner_orig);
SG_HOOK("sound.dll", 0x0002A420, sound_WaveInDeviceBase_openInner, WaveInDeviceBaseOpenInner_re,
        WaveInDeviceBaseOpenInner_orig);
SG_HOOK("sound.dll", 0x0002BAA0, sound_getPlaybackTime, GetPlaybackTime_re, GetPlaybackTime_orig);
SG_HOOK("sound.dll", 0x0002C040, sound_Channel_applyFlags, ChannelApplyFlags_re, ChannelApplyFlags_orig);

}  // namespace
