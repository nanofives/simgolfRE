// C3 batch c3fq (round 35, 2026-10-14) of sound.dll: the sound leftovers of log/c3/c3fq_candidates.tsv (log/c3/round35_brief.md,
// c3fk's Path C in log/c3/c3fk_notes.txt). Method as c3fk (shim/src/re/c3fk.cpp): fixture-owned objects only, thread-filtered
// recording stubs on the incremental-link thunks of callees, operator new / delete through heapstandin.js, the WINMM mmio
// imports through mmiostandin.js, module globals swapped for the key with the first thread frozen and the multimedia timer
// quiet.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are file
// VAs (VA = 0x10000000 + RVA). Every fact comes from the disassembly (log/c3/alt/c3fq/dis/<rva>_<name>.txt, dumped by
// log/c3/alt/c3fq_dis.py), cited by instruction address. sound.dll is LoadLibrary'd after the shim starts, so thunks, import
// slots and module globals are taken relative to the loaded base. __thiscall is emulated with __fastcall (ecx = this, edx
// unused). Every callee runs through the address the original calls (its incremental-link thunk, or its import slot). The C++
// EH frames some originals build (`push -1; push <handler>; mov fs:[0], esp`) only unwind a throwing callee; nothing these
// bodies call throws in the A/B, so the reimplementations build none.
#include "hooks.h"
// NEGATIVE CONTROL (log/c3/alt/c3fq_negctl_gen.py): every body below carries one wrong detail behind NEG_ON.
extern "C" __declspec(dllexport) volatile int C3fqNegCtl[4] = {0, 0x5147454e, 0, 0};   // word 1 = magic; re/frida/js/freeze.js negOn() sets word 0
#define NEG_ON (C3fqNegCtl[0])
#include <stdint.h>
#include <string.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

char* c3fq_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3fq_fn(unsigned rva) { return reinterpret_cast<F>(c3fq_sound(rva)); }
template <typename T> T& c3fq_g(unsigned rva) { return *reinterpret_cast<T*>(c3fq_sound(rva)); }
// an import through its IAT slot (`call [slot]`), so an import stand-in on that slot serves both arms
template <typename F> F c3fq_imp(unsigned rva) { return *reinterpret_cast<F*>(c3fq_sound(rva)); }
// virtual slot `off` of `obj` (`mov eax, [obj]; call [eax + off]`)
template <typename F> F c3fq_vfn(void* obj, int off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}

typedef unsigned(__fastcall* Th0_t)(void*, void*);
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* Th1p_t)(void*, void*, void*);
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);
typedef void*(__cdecl* OpNew_t)(unsigned);
typedef void(__cdecl* OpDelete_t)(void*);

void* c3fq_new(unsigned n) { return c3fq_fn<OpNew_t>(0x000424d7)(n); }       // operator new 0x100424d7
void c3fq_delete(void* p) { c3fq_fn<OpDelete_t>(0x0004249a)(p); }             // operator delete 0x1004249a

// =====================================================================================================================
// A. ChannelTable (names from round 34, re/names/sound_16.tsv)
// =====================================================================================================================

// 0x0001b0d0  ChannelTable_releaseObject (VA 0x1001b0d0). __thiscall (obj), `ret 4` (0x1001b134); obj = [esp+8] after one
// push (0x1001b0d1). Every one of the 0x80 table slots this+4 .. this+0x200 that holds obj is set to 0 (`lea eax, [ecx+4]`,
// counter 0x80, `cmp [eax], ebx` / `jne`, store 0x1001b0d5-0x1001b0eb). Then the member array at obj+0x1440: when its first
// slot is non-zero (0x1001b0fd / 0x1001b0ff), at most three members (`cmp edi, 3` / `jge` 0x1001b101 / 0x1001b104) are each
// destroyed through their vtable slot 0 with 1 (`push 1; call [eax]` 0x1001b10c-0x1001b110, the deleting destructor) and their
// slot cleared (0x1001b112); the walk goes on while the NEXT slot is non-zero (`mov eax, [esi+4]` / `test` / `jne`
// 0x1001b118-0x1001b121). The `test ecx, ecx` at 0x1001b108 never sees 0: the slot it reads is the one just tested.
// Finally obj itself gets its vtable slot 0 with 1 (0x1001b125-0x1001b12f; obj was dereferenced at 0x1001b0ed already, so the
// NULL arm of 0x1001b127 cannot run without a fault first). Returns 0 (`xor eax, eax` 0x1001b131): ChannelTable_loadRange
// uses that eax as its loop index (0x1001b035).
typedef unsigned(__fastcall* Del_t)(void*, void*, unsigned);
Th1p_t RelObj_orig;
unsigned __fastcall RelObj_re(void* self, void*, void* obj) {
    for (int k = 0; k < 0x80; ++k)                                             // 0x1001b0d5-0x1001b0eb
        if (fat<void*>(self, 4 + 4 * k) == obj) fat<void*>(self, 4 + 4 * k) = NEG_ON ? obj : 0;
    void** m = &fat<void*>(obj, 0x1440);                                       // 0x1001b0ed / 0x1001b0f4
    if (*m) {                                                                  // 0x1001b0fd / 0x1001b0ff
        int j = 0;
        do {
            if (j >= 3) break;                                                 // 0x1001b101 / 0x1001b104
            if (void* x = *m) c3fq_vfn<Del_t>(x, 0)(x, 0, 1);                  // 0x1001b106-0x1001b110
            *m = 0;                                                            // 0x1001b112
            ++m;                                                               // 0x1001b11b
            ++j;                                                               // 0x1001b11e
        } while (*m);                                                          // 0x1001b118 / 0x1001b11f / 0x1001b121
    }
    if (obj) c3fq_vfn<Del_t>(obj, 0)(obj, 0, 1);                               // 0x1001b125-0x1001b12f
    return 0;                                                                  // 0x1001b131
}

// 0x0001aef0  ChannelTable_loadRange (VA 0x1001aef0). __thiscall (rec), `ret 4` (0x1001b061); rec = [esp+0x2c] after the EH
// frame, `sub esp, 0xc` and four pushes (0x1001af0c). rec[1] (`mov eax, [edi+4]` 0x1001af1b, kept in the argument slot
// 0x1001af20) is the first table index, rec[0] (`mov ecx, [edi]` 0x1001af1e, local 0x1001af24) the last; both are read once,
// here. rec[2] is the object count and rec+0xc the load argument.
// - operator new(0x1450) (0x1001af28); a non-NULL block is built by MultiSound_ctorWithType through the thunk 0x100026ad with
//   the module global [0x100b49c0] (0x1001af3c-0x1001af47) and its eax is the object (0x1001af4c); a NULL block leaves the
//   object NULL (`xor ebx, ebx` 0x1001af34), which faults at the next instruction, so that arm is not part of the behaviour.
// - obj+0x1424 |= 1 (0x1001af4e-0x1001af5b); r = obj's vtable slot +0x10 with rec+0xc (0x1001af54, 0x1001af6d). A non-zero r
//   deletes obj through its vtable slot 0 with 1 (`test ebx, ebx` 0x1001af76 is never 0 here, 0x1001af7a-0x1001af80) and
//   returns r (0x1001af82).
// - Otherwise obj's slot +4 gets this+0x218 (0x1001af89-0x1001af94). When rec[2] > 1 (unsigned, `jbe` 0x1001afa1) members
//   1 .. rec[2]-1 go to obj+0x1440, +0x1444 ... (0x1001afa3, `add esi, 4` 0x1001b003): each a new 0x1450 block built the same
//   way (the global re-read, 0x1001afc6) or NULL (0x1001afd6), stored (0x1001afdb), loaded through slot +0x10 with rec+0xc
//   (its result dropped, 0x1001afea), then the stored member re-read (0x1001aff1) gets slot +4 with this+0x218 re-read
//   (0x1001aff3-0x1001affc); the count is re-read for the `jb` test (0x1001afff-0x1001b008). A NULL member faults at
//   0x1001afdd, as above.
// - obj+0x1438 = rec[2] re-read (0x1001b00e / 0x1001b019). When first <= last (unsigned, `ja` 0x1001b023): from i = first
//   to last (`jbe` 0x1001b038), a non-zero slot this+4+4i (0x1001b025-0x1001b02b) goes to ChannelTable_releaseObject through
//   the thunk 0x10001e01 (0x1001b02d-0x1001b030), and the index continues from that call's eax plus one (`inc eax` 0x1001b035
//   follows the call with eax as the counter: releaseObject returns 0, so the scan restarts at index 1); then every slot
//   first .. last is set to obj (`rep stosd`, count last - first + 1, 0x1001b03e-0x1001b04b; the repeated `ja` 0x1001b03c tests
//   the same values as 0x1001b023 and never jumps). Returns 0 (0x1001b04d).
typedef void*(__fastcall* CtorT_t)(void*, void*, unsigned);
Th1p_t LoadRange_orig;
unsigned __fastcall LoadRange_re(void* self, void*, void* recp) {
    unsigned* rec = static_cast<unsigned*>(recp);
    const unsigned first = rec[1];                                             // 0x1001af1b / 0x1001af20
    const unsigned last = rec[0];                                              // 0x1001af1e / 0x1001af24
    void* blk = c3fq_new(0x1450);                                              // 0x1001af12 / 0x1001af28
    void* obj = 0;                                                             // 0x1001af34
    if (blk) obj = c3fq_fn<CtorT_t>(0x000026ad)(blk, 0, c3fq_g<unsigned>(0x000b49c0));   // 0x1001af3c-0x1001af4c
    fat<unsigned>(obj, 0x1424) |= 1;                                           // 0x1001af4e-0x1001af5b
    unsigned r = c3fq_vfn<Th1p_t>(obj, 0x10)(obj, 0, rec + 3);                 // 0x1001af54 / 0x1001af6d
    if (r) {                                                                   // 0x1001af72 / 0x1001af74
        if (obj) c3fq_vfn<Del_t>(obj, 0)(obj, 0, 1);                           // 0x1001af76-0x1001af80
        return r;                                                              // 0x1001af82
    }
    c3fq_vfn<Th1_t>(obj, 4)(obj, 0, fat<unsigned>(self, 0x218));              // 0x1001af89-0x1001af94
    if (rec[2] > 1) {                                                          // 0x1001af97-0x1001afa1
        void** slot = &fat<void*>(obj, 0x1440);                                // 0x1001afa3
        unsigned n = 1;                                                        // 0x1001af9a
        do {
            void* b = c3fq_new(0x1450);                                        // 0x1001afa9 / 0x1001afae
            void* m = 0;                                                       // 0x1001afd6
            if (b) m = c3fq_fn<CtorT_t>(0x000026ad)(b, 0, c3fq_g<unsigned>(0x000b49c0));   // 0x1001afc4-0x1001afcf
            *slot = m;                                                         // 0x1001afdb
            c3fq_vfn<Th1p_t>(m, 0x10)(m, 0, rec + 3);                          // 0x1001afd8-0x1001afea
            void* again = *slot;                                               // 0x1001aff1
            c3fq_vfn<Th1_t>(again, 4)(again, 0, fat<unsigned>(self, 0x218));   // 0x1001afed-0x1001affc
            ++n;                                                               // 0x1001b002
            ++slot;                                                            // 0x1001b003
        } while (n < rec[2]);                                                  // 0x1001afff / 0x1001b006 / 0x1001b008
    }
    fat<unsigned>(obj, 0x1438) = rec[2] + (NEG_ON ? 1u : 0u);                   // 0x1001b00e / 0x1001b019
    if (first > last) return 0;                                                // 0x1001b01f / 0x1001b023
    unsigned i = first;                                                        // 0x1001b021
    do {
        void* o = fat<void*>(self, 4 + 4 * i);                                 // 0x1001b025
        if (o) i = c3fq_fn<Th1p_t>(0x00001e01)(self, 0, o);                    // 0x1001b029-0x1001b030 (eax is the index)
        ++i;                                                                   // 0x1001b035
    } while (i <= last);                                                       // 0x1001b036 / 0x1001b038
    for (unsigned k = first; k <= last; ++k) fat<void*>(self, 4 + 4 * k) = obj;   // 0x1001b03e-0x1001b04b
    return 0;                                                                  // 0x1001b04d
}

// =====================================================================================================================
// B. rewindChannel
// =====================================================================================================================

// 0x000339c0  rewindChannel (VA 0x100339c0). __thiscall, no stack argument, plain `ret` (0x10033b30, 0x100339dc).
// - A NULL mmio handle at +0 (0x100339c9-0x100339cd) with bit 2 of +0x5c clear (`test [esi+0x5c], bl`, bl = 4, 0x100339cf /
//   0x100339d2) returns 0x14 (0x100339d6), nothing written.
// - +0x58 &= 0xfffffedf (bits 5 and 8 cleared, 0x100339dd-0x100339e5); +0x16c = +0x164 (0x100339ea / 0x100339f3).
// - Bit 0 of the new +0x58 set (`test dl, 1` 0x100339f0 / 0x100339f9): +0x28 = +0x24 and +0x2c = +0x24 + +0x178
//   (0x100339fb-0x10033a09). Else bit 2 of +0x5c clear (0x10033a0e / 0x10033a11): mmioSeek(handle, word +0x18c * +0x164 +
//   +0x174, SEEK_SET) (0x10033a13-0x10033a2b), mmioGetInfo(handle, this+0xc, 0) (0x10033a31-0x10033a39), mmioAdvance(handle
//   re-read, this+0xc, MMIO_READ) (0x10033a3f-0x10033a4a); then on the stream object at +8: +0x20 = +0x18 (0x10033a4c-0x10033a52)
//   and +0x1c = that +0x20 (0x10033a55-0x10033a5b); +0x170 = 0 (0x10033a61); +0x1378 = the stream's +0x18 (0x10033a5e-0x10033a6c);
//   fillDecodeRing through the thunk 0x10002022 (0x10033a72); mmioAdvance(handle re-read, this+0xc, MMIO_READ) again
//   (0x10033a77-0x10033a7c). Every import result is dropped.
// - Then, on every path that did not return 0x14: +0x5c &= ~3 (0x10033a7f / 0x10033a91 / 0x10033a9f); +0x1d8 = +0x208
//   (0x10033a82 / 0x10033a93); +0x1e0 = +0x210 and +0x1e4 = +0x214 (0x10033a8b, 0x10033aa2-0x10033ab4); +0x228 = 0 (0x10033abd);
//   +0x58 = (+0x58 as read at 0x10033a88) & 0xf0000f67 (0x10033aa8 / 0x10033ac3); Channel_recomputeVoiceGains(this, +0x1c0)
//   through the thunk 0x10001efb (0x10033a99, 0x10033aba-0x10033ac6); Channel_setPanGains(this, +0x224) through 0x10001a7d
//   (0x10033acb-0x10033ad4).
// - Bit 0 of +0x58 re-read after those calls (0x10033ad9 / 0x10033add): for the 16 voice records at this+0x250 + 0x110k
//   (eax = this+0x254 + 0x110k, counter 0x10, 0x10033adf-0x10033b29): byte +0x84 &= 0xf1 (0x10033af1), +4 = +0 (0x10033aee /
//   0x10033af7), +0xd8 = +0xe0 (0x10033af9 / 0x10033aff), +0x70 = 0, +0x74 = 0, +0xa8 = 0 (0x10033b0b-0x10033b11),
//   +0xdc = +0xe4 (0x10033b05 / 0x10033b17), byte +0x84 &= 0xfd (0x10033b1d). Returns 0 (0x10033b2d).
typedef long(__stdcall* MmioSeek_t)(void*, long, int);
typedef unsigned(__stdcall* MmioInfo_t)(void*, void*, unsigned);
Th0_t Rewind_orig;
unsigned __fastcall Rewind_re(void* self, void*) {
    if (!fat<void*>(self, 0) && !(fat<unsigned char>(self, 0x5c) & 4)) return 0x14;   // 0x100339c9-0x100339d6
    unsigned f58 = fat<unsigned>(self, 0x58) & 0xfffffedf;                     // 0x100339dd-0x100339e0
    fat<unsigned>(self, 0x58) = f58;                                           // 0x100339e5
    fat<unsigned>(self, 0x16c) = fat<unsigned>(self, 0x164);                   // 0x100339ea / 0x100339f3
    if (f58 & 1) {                                                             // 0x100339f0 / 0x100339f9
        unsigned a = fat<unsigned>(self, 0x24);                                // 0x100339fb
        fat<unsigned>(self, 0x28) = a;                                         // 0x10033a06
        fat<unsigned>(self, 0x2c) = fat<unsigned>(self, 0x178) + a + (NEG_ON ? 1u : 0u);   // 0x100339fe-0x10033a09
    } else if (!(fat<unsigned char>(self, 0x5c) & 4)) {                        // 0x10033a0e / 0x10033a11
        long pos = static_cast<long>(fat<unsigned short>(self, 0x18c) * fat<unsigned>(self, 0x164) + fat<unsigned>(self, 0x174));
        c3fq_imp<MmioSeek_t>(0x000b866c)(fat<void*>(self, 0), pos, 0);         // 0x10033a13-0x10033a2b
        void* info = reinterpret_cast<char*>(self) + 0xc;                      // 0x10033a33
        c3fq_imp<MmioInfo_t>(0x000b8668)(fat<void*>(self, 0), info, 0);        // 0x10033a31-0x10033a39 (mmioGetInfo)
        c3fq_imp<MmioInfo_t>(0x000b8630)(fat<void*>(self, 0), info, 0);        // 0x10033a3f-0x10033a4a (mmioAdvance)
        void* st = fat<void*>(self, 8);                                        // 0x10033a4c
        fat<unsigned>(st, 0x20) = fat<unsigned>(st, 0x18);                     // 0x10033a4f / 0x10033a52
        st = fat<void*>(self, 8);                                              // 0x10033a55
        fat<unsigned>(st, 0x1c) = fat<unsigned>(st, 0x20);                     // 0x10033a58 / 0x10033a5b
        st = fat<void*>(self, 8);                                              // 0x10033a5e
        fat<unsigned>(self, 0x170) = 0;                                        // 0x10033a61
        fat<unsigned>(self, 0x1378) = fat<unsigned>(st, 0x18);                 // 0x10033a69 / 0x10033a6c
        c3fq_fn<Th0_t>(0x00002022)(self, 0);                                   // 0x10033a72 (fillDecodeRing)
        c3fq_imp<MmioInfo_t>(0x000b8630)(fat<void*>(self, 0), info, 0);        // 0x10033a77-0x10033a7c (mmioAdvance)
    }
    const unsigned f5c = fat<unsigned>(self, 0x5c);                            // 0x10033a7f
    const unsigned f208 = fat<unsigned>(self, 0x208);                          // 0x10033a82
    const unsigned f58b = fat<unsigned>(self, 0x58);                           // 0x10033a88
    const unsigned f214 = fat<unsigned>(self, 0x214);                          // 0x10033a8b
    fat<unsigned>(self, 0x1d8) = f208;                                         // 0x10033a93
    const unsigned f1c0 = fat<unsigned>(self, 0x1c0);                          // 0x10033a99
    fat<unsigned>(self, 0x5c) = f5c & ~3u;                                     // 0x10033a91 / 0x10033a9f
    fat<unsigned>(self, 0x1e0) = fat<unsigned>(self, 0x210);                   // 0x10033aa2 / 0x10033aae
    fat<unsigned>(self, 0x1e4) = f214;                                         // 0x10033ab4
    fat<unsigned>(self, 0x228) = 0;                                            // 0x10033abd
    fat<unsigned>(self, 0x58) = f58b & 0xf0000f67;                             // 0x10033aa8 / 0x10033ac3
    c3fq_fn<Th1_t>(0x00001efb)(self, 0, f1c0);                                 // 0x10033ac6 (Channel_recomputeVoiceGains)
    c3fq_fn<Th1_t>(0x00001a7d)(self, 0, fat<unsigned>(self, 0x224));           // 0x10033acb-0x10033ad4 (Channel_setPanGains)
    if (fat<unsigned char>(self, 0x58) & 1) {                                  // 0x10033ad9 / 0x10033add
        char* v = reinterpret_cast<char*>(self) + 0x254;                       // 0x10033adf
        for (int k = 0; k < 0x10; ++k, v += 0x110) {                           // 0x10033ae5 / 0x10033b23-0x10033b29
            unsigned prev = fat<unsigned>(v, -4);                              // 0x10033aee
            fat<unsigned char>(v, 0x80) &= 0xf1;                               // 0x10033af1
            fat<unsigned>(v, 0) = prev;                                        // 0x10033af7
            fat<unsigned>(v, 0xd4) = fat<unsigned>(v, 0xdc);                   // 0x10033af9 / 0x10033aff
            unsigned e0 = fat<unsigned>(v, 0xe0);                              // 0x10033b05
            fat<unsigned>(v, 0x6c) = 0;                                        // 0x10033b0b
            fat<unsigned>(v, 0x70) = 0;                                        // 0x10033b0e
            fat<unsigned>(v, 0xa4) = 0;                                        // 0x10033b11
            fat<unsigned>(v, 0xd8) = e0;                                       // 0x10033b17
            fat<unsigned char>(v, 0x80) &= 0xfd;                               // 0x10033b1d
        }
    }
    return 0;                                                                  // 0x10033b2d
}

// =====================================================================================================================
// C. The buffer queue: Wave_Device::appendBuffer, Wave_Device::freeBufferList
// =====================================================================================================================
// Queue nodes are 0xc-byte records {prev +0, next +4, payload +8} (built at 0x10006936-0x1000693b); the object keeps the head
// at +0x5c, the tail at +0x60, a cursor at +0x64 and the node count at +0x68.
typedef unsigned(__stdcall* MmioClose_t)(void*, unsigned);
typedef void(__fastcall* Void0_t)(void*, void*);

// unlink the head node n (`mov ecx, [eax+4]`; a non-NULL next gets prev = 0 and becomes the head, else tail and head = 0),
// as at 0x1000651f-0x10006530, 0x1000657a-0x1000658b, 0x1000664f-0x10006660, 0x10006719-0x1000672a
void c3fq_unlinkHead(void* self, void* n) {
    if (void* nx = fat<void*>(n, 4)) {
        fat<void*>(nx, 0) = 0;
        fat<void*>(self, 0x5c) = nx;
    } else {
        fat<void*>(self, 0x60) = 0;
        fat<void*>(self, 0x5c) = 0;
    }
}

// 0x00006850  Wave_Device::appendBuffer (VA 0x10006850). __thiscall (buf), `ret 4` (0x1000688c, 0x100068d4, 0x100069e6); buf
// = [esp+0x18] after the EH frame and two pushes (0x10006867). Reached through slot 36 (+0x90) of the vtable 0x1005b1c0
// (dword 0x1005b250, stored by WaveStream::ctor 0x10006020).
// - buf other than this+0x6c returns 0xa (0x1000686f-0x10006878).
// - d = buf+0x20 - buf+0x18 (0x1000688f-0x10006895, kept in the argument slot 0x10006897). d = 0: mmioClose(+0xb0, 0)
//   (0x1000689d-0x100068a7), the module counter [0x100b4a08] minus 1 (0x100068ad-0x100068b3), +0xb0 = 0 (0x100068b8), returns
//   0x26 (0x100068c0).
// - Otherwise a 0x110-byte voice: operator new (0x100068dd), Voice_resetDefaults through the thunk 0x10001e56 on a non-NULL
//   block (0x100068f1-0x100068fa; NULL gives voice 0, 0x100068fe, which faults at 0x1000696c), then MmioBuffer_init(voice,
//   buf, this+0xd8) through 0x100017a8 (0x10006900-0x10006912).
// - +0x64 = the tail (0x10006917 / 0x1000691c); the tail's payload, or 0 for no tail, is kept (0x1000691f-0x10006926).
// - A 0xc-byte node (operator new 0x1000692a) gets {0, 0, voice} (0x10006936-0x1000693b), NULL left as NULL (0x10006940).
//   With a tail (re-read 0x10006942) the tail's next = node and node's prev = the tail re-read (0x10006949-0x1000694f; a NULL
//   node faults there); without one the head = node (0x10006953). Tail = node (0x10006959), +0x68 plus 1 (0x10006956-0x10006961),
//   +0xb0 = 0 (0x10006966).
// - voice+0x90 = d / word voice+0xac (`div` unsigned, 0x10006964-0x10006975, stored 0x10006987); voice+0xd4 = voice+0x20
//   (0x10006977 / 0x1000697c); voice+0xd8 = voice+0x18 (0x1000698d / 0x10006990).
// - The old tail's payload, when non-zero (0x1000697a / 0x10006996): +0x98 |= 1 (0x10006998-0x100069a0), byte +0x9c |= 1
//   (0x100069a6-0x100069ae), +0xb4 = voice (0x100069b4), +0xcc = 0 (0x100069ba).
// - +0x58 with bit 2 set (0x100069c4 / 0x100069c6) and bit 0 clear (0x100069c8 / 0x100069ca): bit 2 cleared, bits 0 and 1 set
//   (0x100069cc-0x100069d0). Returns 0 (0x100069d9).
typedef void*(__fastcall* Ctor0_t)(void*, void*);
typedef unsigned(__fastcall* Th2pp_t)(void*, void*, void*, void*);
Th1p_t AppendBuf_orig;
unsigned __fastcall AppendBuf_re(void* self, void*, void* buf) {
    if (buf != reinterpret_cast<char*>(self) + 0x6c) return 0xa;               // 0x1000686f-0x10006878
    const unsigned d = fat<unsigned>(buf, 0x20) - fat<unsigned>(buf, 0x18);    // 0x1000688f-0x10006897
    if (!d) {
        c3fq_imp<MmioClose_t>(0x000b8670)(fat<void*>(self, 0xb0), 0);          // 0x1000689d-0x100068a7
        --c3fq_g<int>(0x000b4a08);                                             // 0x100068ad-0x100068b3
        fat<void*>(self, 0xb0) = 0;                                            // 0x100068b8
        return 0x26;                                                           // 0x100068c0
    }
    void* p = c3fq_new(0x110);                                                 // 0x100068d8 / 0x100068dd
    void* v = p ? c3fq_fn<Ctor0_t>(0x00001e56)(p, 0) : 0;                      // 0x100068f1-0x100068fe
    c3fq_fn<Th2pp_t>(0x000017a8)(v, 0, buf, reinterpret_cast<char*>(self) + 0xd8);   // 0x10006900-0x10006912
    void* tail = fat<void*>(self, 0x60);                                       // 0x10006917
    fat<void*>(self, 0x64) = tail;                                             // 0x1000691c
    void* prevVoice = tail ? fat<void*>(tail, 8) : 0;                          // 0x1000691f-0x10006926
    void** node = static_cast<void**>(c3fq_new(0xc));                          // 0x10006928 / 0x1000692a
    if (node) { node[0] = 0; node[1] = 0; node[2] = v; }                       // 0x10006936-0x1000693b
    if (fat<void*>(self, 0x60)) {                                              // 0x10006942-0x10006947
        fat<void*>(fat<void*>(self, 0x60), 4) = node;                          // 0x10006949
        node[0] = fat<void*>(self, 0x60);                                      // 0x1000694c / 0x1000694f
    } else {
        fat<void*>(self, 0x5c) = node;                                         // 0x10006953
    }
    fat<void*>(self, 0x60) = node;                                             // 0x10006959
    ++fat<unsigned>(self, 0x68);                                               // 0x10006956 / 0x10006960 / 0x10006961
    fat<void*>(self, 0xb0) = 0;                                                // 0x10006966
    const unsigned q = d / fat<unsigned short>(v, 0xac);                       // 0x1000696c-0x10006975
    fat<unsigned>(v, 0xd4) = fat<unsigned>(v, 0x20);                           // 0x10006977 / 0x1000697c
    fat<unsigned>(v, 0x90) = q + (NEG_ON ? 1u : 0u);                            // 0x10006987
    fat<unsigned>(v, 0xd8) = fat<unsigned>(v, 0x18);                           // 0x1000698d / 0x10006990
    if (prevVoice) {                                                           // 0x1000697a / 0x10006996
        fat<unsigned>(prevVoice, 0x98) |= 1;                                   // 0x10006998-0x100069a0
        fat<unsigned char>(prevVoice, 0x9c) |= 1;                              // 0x100069a6-0x100069ae
        fat<void*>(prevVoice, 0xb4) = v;                                       // 0x100069b4
        fat<unsigned>(prevVoice, 0xcc) = 0;                                    // 0x100069ba
    }
    const unsigned f = fat<unsigned>(self, 0x58);                              // 0x100069c0
    if ((f & 4) && !(f & 1)) fat<unsigned>(self, 0x58) = (f & ~4u) | 3;        // 0x100069c4-0x100069d0
    return 0;                                                                  // 0x100069d9
}

// 0x000064e0  Wave_Device::freeBufferList (VA 0x100064e0). __thiscall (dst, count, fmt, outN), `ret 0x10` (0x100065f4,
// 0x10006797); after `sub esp, 0xc` the arguments are [esp+0x10..0x1c] (fmt read at 0x100064e3). Callers 0x10007010
// (writeWaveFile), 0x100075e0.
// - fn = the converter table entry 0x1005b4a4[fmt] (0x100064eb, read before any test); pad = fmt >= 0xc (unsigned `jb`
//   0x100064f4 / 0x10006500, flag set 0x10006502).
// - +0x58 bit 3 set (0x1000650a / 0x1000650e): the queue is drained: the head node (0x10006514 / 0x10006519) is unlinked, its
//   payload kept (0x10006533), the node freed (operator delete 0x10006537) and +0x68 lowered (0x1000653c-0x10006545); while the
//   payload is non-zero (0x10006543 / 0x10006548, 0x1000659e / 0x100065a3): a non-zero mmio handle at payload+0x44 is closed
//   (mmioClose(h, 0) 0x1000654a-0x10006553) and [0x100b4a08] lowered (0x10006559); the payload goes to VoiceSlot_dtorEmpty
//   through the thunk 0x10001b45 and operator delete (0x10006563-0x1000656b; the `cmp edi, ebx` 0x1000655f is never 0 here);
//   then the next head the same way (0x10006573-0x100065a0), stopping at an empty queue (0x10006578). Then vtable slot +0x80
//   of this (0x100065a5-0x100065a9), whether the queue was empty or not.
// - The cursor +0x64 = the head (0x100065af-0x100065b4). No head or a NULL head payload (0x100065b7, 0x100065be): *outN = 0
//   (0x100065c0 / 0x100065c4); with pad, count * 8 bytes at dst are zeroed (0x100065c6-0x100065e7); returns 0x26 (0x100065eb).
// - Otherwise, voice = that payload: d = dst and pos = voice+0x1c are locals (0x100065f7-0x10006614, pos in fmt's argument
//   slot), and got = fn(&d, &pos, count, voice, 0) (cdecl, `add esp, 0x14`, 0x10006618 / 0x10006622); *outN = got
//   (0x1000661c / 0x10006629).
// - k = voice+0xcc (0x1000662b); for k > 0 (`jbe` 0x10006633) rounds: the cursor, when non-zero, moves to its next
//   (0x1000663b-0x10006645); a non-zero head is unlinked, freed, +0x68 lowered (0x10006648-0x10006670); voice+0x44 is closed
//   (no zero test, 0x10006673-0x10006678) and [0x100b4a08] lowered (0x1000667e-0x10006687); a non-zero voice goes to
//   VoiceSlot_dtorEmpty and operator delete (0x10006685-0x1000669c); cursor = head (0x1000669f / 0x100066a4) and voice = the
//   head's payload or 0 (0x100066a7-0x100066ae); `dec ebp` / `jne` 0x100066b0 / 0x100066b1. A NULL voice in a later round faults
//   at 0x10006673.
// - A non-zero voice (0x100066bb): voice+0x1c = pos (0x100066c3 / 0x100066c7); pos >= voice+0x20 (unsigned, `jb` 0x100066ce /
//   0x100066d1) with bit 0 of byte voice+0x9c clear (0x100066d7 / 0x100066de) finishes it: +0xcc += +0xc8 + 1 and +0xc8 = 0
//   (0x100066e4-0x10006703), +0x58 bit 4 cleared and bit 2 set (0x100066fd-0x1000670c), vtable slot +0x20 of this (0x1000670f),
//   the head (non-zero) unlinked, freed, +0x68 lowered (0x10006712-0x1000673a), voice+0x44 closed and the counter lowered
//   (0x1000673d-0x10006751), VoiceSlot_dtorEmpty and operator delete on the voice (0x10006757-0x1000675d).
// - got other than count (0x10006769 / 0x1000676b) with pad (0x1000676d / 0x10006771): (count - got) * 8 bytes are zeroed at d
//   as fn left it (0x10006773-0x1000678c). Returns 0 (0x10006791).
typedef unsigned(__cdecl* Conv_t)(void**, unsigned*, unsigned, void*, unsigned);
typedef unsigned(__fastcall* Pull_t)(void*, void*, void*, unsigned, unsigned, unsigned*);
Pull_t Pull_orig;

void c3fq_dropVoice(void* v) {                                                 // 0x1000668f-0x1000669c, 0x10006757-0x1000675d
    c3fq_fn<Void0_t>(0x00001b45)(v, 0);
    c3fq_delete(v);
}
void c3fq_popHead(void* self) {                                                // 0x10006648-0x10006670, 0x10006712-0x1000673a
    void* h = fat<void*>(self, 0x5c);
    if (!h) return;
    c3fq_unlinkHead(self, h);
    c3fq_delete(h);
    --fat<unsigned>(self, 0x68);
}

unsigned __fastcall Pull_re(void* self, void*, void* dst, unsigned count, unsigned fmt, unsigned* outN) {
    Conv_t fn = c3fq_g<Conv_t>(0x0005b4a4 + 4 * fmt);                          // 0x100064eb
    const bool pad = fmt >= 0xc;                                               // 0x100064f4-0x10006502
    if (fat<unsigned char>(self, 0x58) & 8) {                                  // 0x1000650a / 0x1000650e
        if (void* n = fat<void*>(self, 0x5c)) {                                // 0x10006514-0x10006519
            c3fq_unlinkHead(self, n);                                          // 0x1000651f-0x10006530
            void* v = fat<void*>(n, 8);                                        // 0x10006533
            c3fq_delete(n);                                                    // 0x10006537
            --fat<unsigned>(self, 0x68);                                       // 0x1000653c-0x10006545
            while (v) {                                                        // 0x10006548 / 0x100065a3
                if (void* h = fat<void*>(v, 0x44)) {                           // 0x1000654a-0x1000654f
                    c3fq_imp<MmioClose_t>(0x000b8670)(h, 0);                   // 0x10006551-0x10006553
                    --c3fq_g<int>(0x000b4a08);                                 // 0x10006559
                }
                c3fq_dropVoice(v);                                             // 0x10006563-0x1000656b
                n = fat<void*>(self, 0x5c);                                    // 0x10006573
                if (!n) break;                                                 // 0x10006576 / 0x10006578
                c3fq_unlinkHead(self, n);                                      // 0x1000657a-0x1000658b
                v = fat<void*>(n, 8);                                          // 0x1000658e
                c3fq_delete(n);                                                // 0x10006592
                --fat<unsigned>(self, 0x68);                                   // 0x10006597-0x100065a0
            }
        }
        c3fq_vfn<Void0_t>(self, 0x80)(self, 0);                                // 0x100065a5-0x100065a9
    }
    void* n = fat<void*>(self, 0x5c);                                          // 0x100065af
    fat<void*>(self, 0x64) = n;                                                // 0x100065b4
    void* v = n ? fat<void*>(n, 8) : 0;                                        // 0x100065b7-0x100065be
    if (!v) {
        *outN = 0;                                                             // 0x100065c0 / 0x100065c4
        if (pad) memset(dst, 0, count << 3);                                   // 0x100065c6-0x100065e7
        return 0x26;                                                           // 0x100065eb
    }
    void* d = dst;                                                             // 0x100065f7 / 0x10006601
    unsigned pos = fat<unsigned>(v, 0x1c);                                     // 0x10006605 / 0x10006614
    const unsigned got = fn(&d, &pos, count, v, 0);                            // 0x10006608-0x10006618
    *outN = got + (NEG_ON ? 1u : 0u);                                          // 0x1000661c / 0x10006629
    unsigned k = fat<unsigned>(v, 0xcc);                                       // 0x1000662b
    if (k) {                                                                   // 0x10006631 / 0x10006633
        do {
            if (void* c = fat<void*>(self, 0x64)) fat<void*>(self, 0x64) = fat<void*>(c, 4);   // 0x1000663b-0x10006645
            c3fq_popHead(self);                                                // 0x10006648-0x10006670
            c3fq_imp<MmioClose_t>(0x000b8670)(fat<void*>(v, 0x44), 0);         // 0x10006673-0x10006678
            --c3fq_g<int>(0x000b4a08);                                         // 0x1000667e-0x10006687
            if (v) c3fq_dropVoice(v);                                          // 0x10006685-0x1000669c
            n = fat<void*>(self, 0x5c);                                        // 0x1000669f
            fat<void*>(self, 0x64) = n;                                        // 0x100066a4
            v = n ? fat<void*>(n, 8) : 0;                                      // 0x100066a7-0x100066ae
        } while (--k);                                                         // 0x100066b0 / 0x100066b1
    }
    if (v) {                                                                   // 0x100066bb
        fat<unsigned>(v, 0x1c) = pos;                                          // 0x100066c3 / 0x100066c7
        if (pos >= fat<unsigned>(v, 0x20) && !(fat<unsigned char>(v, 0x9c) & 1)) {   // 0x100066ca-0x100066de
            const unsigned c8 = fat<unsigned>(self, 0xc8) + 1;                 // 0x100066e4 / 0x100066f0
            fat<unsigned>(self, 0xcc) = fat<unsigned>(self, 0xcc) + c8;        // 0x100066ea / 0x100066f3 / 0x100066f7
            fat<unsigned>(self, 0xc8) = 0;                                     // 0x10006703
            fat<unsigned>(self, 0x58) = (fat<unsigned>(self, 0x58) & ~0x10u) | 4;   // 0x100066fd-0x1000670c
            c3fq_vfn<Void0_t>(self, 0x20)(self, 0);                            // 0x1000670f
            c3fq_popHead(self);                                                // 0x10006712-0x1000673a
            c3fq_imp<MmioClose_t>(0x000b8670)(fat<void*>(v, 0x44), 0);         // 0x1000673d-0x10006742
            --c3fq_g<int>(0x000b4a08);                                         // 0x10006748-0x10006751
            c3fq_dropVoice(v);                                                 // 0x10006757-0x1000675d
        }
    }
    if (got != count && pad) memset(d, 0, (count - got) << 3);                 // 0x10006769-0x1000678c
    return 0;                                                                  // 0x10006791
}

// =====================================================================================================================
// D. create_sound (export)
// =====================================================================================================================
// 0x000093e0  create_sound (VA 0x100093e0). __cdecl(out, name, type); the EH frame makes out = [esp+0x18], name =
// [esp+0x18] after `push esi` (name = esi 0x100093fa), type = eax (0x100093ee). ret (caller cleans). Picks a type:
// type != 0 is used directly (0x100093fe / 0x10009400); else a NULL name is type 1 (0x10009402-0x10009406); else strstr
// (0x10042690) of name against the module strings "wav" 0x10063dc0 -> 1 (0x1000941f), "mid" 0x10063dbc -> 2 (0x10009438),
// "aif" 0x10063db8 -> 3 (0x10009451), else "rmi" 0x10063db4 found -> 2 / not found -> 0 (0x10009458-0x1000946a). t - 1
// (0x1000946d) indexes the 8-entry jump table 0x100096e0 after `cmp 7; ja` to the default 0xb (0x1000946e-0x10009477).
// Each case allocates (operator new) and builds one Sound class, stores it at *out and returns 0; a NULL allocation leaves
// *out = 0 (the heap stand-in never returns NULL, so those arms do not run). Cases idx1 and idx4 first return 3 when the
// gate global [0x100b4a1c] is 0 (0x100094d5 / 0x10009583). Index 2 is the default (0xb). The seven constructors
// (MultiSound_ctorWithType 0x10039790, Sound5b6a8_ctor 0x1001f4a0, WaveStream::ctor 0x10006020, Sound::ctor 0x10003f30,
// Sound5c450_ctor 0x10030080, StreamReader_ctor 0x10031200, Ch5c3a4_ctor 0x1002f740) and the name setters (Object_setName
// 0x1002bb10, Sound_assignName 0x1002ddf0) are all C3; the fixture stubs them (recording, returning `this`), so this body's
// evidence is the dispatch (which type, which size, which constructor and name call), not the constructors' interiors.
// Object_setName / Sound_assignName take only `name`: the -1 stored at [esp+0x10] before each call is the EH try-level, not
// a pushed argument (0x100094b2 / 0x10009522).
typedef void*(__fastcall* SetName_t)(void*, void*, const char*);
typedef int(__cdecl* Strstr_t)(const char*, const char*);
typedef int(__cdecl* CreateSound_t)(void**, const char*, unsigned);
CreateSound_t CreateSound_orig;

const char* c3fq_str(unsigned rva) { return c3fq_sound(rva); }

int __cdecl CreateSound_re(void** out, const char* name, unsigned type) {
    unsigned t;
    if (type) t = type;                                                        // 0x100093fe / 0x10009400
    else if (!name) t = 1;                                                      // 0x10009402-0x10009406
    else {
        Strstr_t ss = c3fq_fn<Strstr_t>(0x00042690);                           // 0x10042690 strstr
        if (ss(name, c3fq_str(0x00063dc0))) t = 1;                             // 0x1000940d-0x1000941f "wav"
        else if (ss(name, c3fq_str(0x00063dbc))) t = 2;                        // 0x10009426-0x10009438 "mid"
        else if (ss(name, c3fq_str(0x00063db8))) t = 3;                        // 0x1000943f-0x10009451 "aif"
        else t = ss(name, c3fq_str(0x00063db4)) ? 2 : 0;                       // 0x10009458-0x1000946a "rmi"
    }
    unsigned idx = t - 1;                                                       // 0x1000946d
    if (idx > (NEG_ON ? 6u : 7u)) return 0xb;                                   // 0x1000946e-0x10009471 default
    switch (idx) {
    case 0: {                                                                  // 0x1000947e MultiSound, 0x1450
        void* o = c3fq_new(0x1450);                                            // 0x1000947e-0x10009483
        if (o) c3fq_fn<CtorT_t>(0x000026ad)(o, 0, c3fq_g<unsigned>(0x000b49c0));   // 0x1000949b-0x100094a4
        *out = o;                                                              // 0x100094ba
        c3fq_fn<SetName_t>(0x000021b7)(o, 0, name);                // 0x100094be Object_setName
        return 0;                                                              // 0x100094c3
    }
    case 1:                                                                     // 0x100094d5 Sound5b6a8, 0x948
        if (!c3fq_g<unsigned>(0x000b4a1c)) return 3;                           // 0x100094d5-0x100094de
        {
            void* o = c3fq_new(0x948);                                         // 0x100094f3-0x100094f8
            if (o) c3fq_fn<Ctor0_t>(0x00002513)(o, 0);                        // 0x10009510-0x10009512 Sound5b6a8_ctor
            *out = o;                                                          // 0x1000952a
            c3fq_fn<SetName_t>(0x0000105f)(o, 0, name);           // 0x1000952c Sound_assignName
            return 0;                                                          // 0x10009531
        }
    case 2: return 0xb;                                                        // 0x100096c9 (jump-table default slot)
    case 3: {                                                                  // 0x10009543 WaveStream, 0xec
        void* o = c3fq_new(0xec);                                             // 0x10009543-0x10009548
        if (o) c3fq_fn<Ctor0_t>(0x0000138e)(o, 0);                           // 0x10009564-0x10009566 WaveStream::ctor
        *out = o;                                                             // 0x1000956f
        return 0;                                                             // 0x10009571
    }
    case 4:                                                                    // 0x10009583 Sound, 0xa60
        if (!c3fq_g<unsigned>(0x000b4a1c)) return 3;                          // 0x10009583-0x1000958c
        {
            void* o = c3fq_new(0xa60);                                        // 0x100095a1-0x100095a6
            if (o) c3fq_fn<Ctor0_t>(0x00001163)(o, 0);                       // 0x100095c4 Sound::ctor
            *out = o;                                                         // 0x100095cd
            return 0;                                                         // 0x100095cf
        }
    case 5: {                                                                  // 0x100095e1 Sound5c450, 0xe8
        void* o = c3fq_new(0xe8);                                            // 0x100095e1-0x100095e6
        if (o) c3fq_fn<Ctor0_t>(0x000018c5)(o, 0);                          // 0x10009604 Sound5c450_ctor
        *out = o;                                                            // 0x1000960d
        return 0;                                                            // 0x1000960f
    }
    case 6: {                                                                  // 0x10009621 StreamReader, 0x94
        void* o = c3fq_new(0x94);                                            // 0x10009621-0x10009626
        if (o) c3fq_fn<Ctor0_t>(0x0000140b)(o, 0);                          // 0x10009640 StreamReader_ctor
        *out = o;                                                            // 0x10009649
        return 0;                                                            // 0x1000964b
    }
    case 7: {                                                                  // 0x10009675 Ch5c3a4, 0x10c
        void* o = c3fq_new(0x10c);                                           // 0x10009675-0x1000967a
        if (o) c3fq_fn<Ctor0_t>(0x0000237e)(o, 0);                          // 0x10009694 Ch5c3a4_ctor
        *out = o;                                                            // 0x1000969d
        return 0;                                                            // 0x1000969f
    }
    }
    return 0xb;                                                                // unreachable (idx 0..7 all handled)
}

}  // namespace

// 0x000093e0  create_sound
SG_HOOK("sound.dll", 0x000093e0, create_sound, CreateSound_re, CreateSound_orig);
// 0x000064e0  Wave_Device::freeBufferList
SG_HOOK("sound.dll", 0x000064e0, Wave_Device_freeBufferList, Pull_re, Pull_orig);
// 0x00006850  Wave_Device::appendBuffer
SG_HOOK("sound.dll", 0x00006850, Wave_Device_appendBuffer, AppendBuf_re, AppendBuf_orig);
// 0x0001b0d0  ChannelTable_releaseObject
SG_HOOK("sound.dll", 0x0001b0d0, ChannelTable_releaseObject, RelObj_re, RelObj_orig);
// 0x0001aef0  ChannelTable_loadRange
SG_HOOK("sound.dll", 0x0001aef0, ChannelTable_loadRange, LoadRange_re, LoadRange_orig);
// 0x000339c0  rewindChannel
SG_HOOK("sound.dll", 0x000339c0, rewindChannel, Rewind_re, Rewind_orig);
