// NEGATIVE CONTROL for batch c3dh (round 25). NEVER list this file in shim/re_batches.txt: every body here is DELIBERATELY WRONG,
// and only in one heap, import or COM call, so that a diff_hook run proves the heap stand-in's free log / arena cursor, the import
// stand-in's call log and the COM stubs' records are really compared state. None of the defects changes a return value (except
// where noted), so each key must read RED on state alone.
//
// Built INSTEAD of c3dh.cpp through log/c3/c3dh_negctl_batches.txt; driven by log/c3/alt/c3dh_negctl.py (mode "wrong"), whose CSVs go
// to log/c3/alt, never to log/diff.
//
//   0x00008920  SoundTimer::unregisterClient      MISSING CALL: the unlinked node is never handed to operator delete 0x1004249a
//   0x000088e0  SoundTimer::registerClient        MISSING CALL: LeaveCriticalSection is never called
//   0x00009040  SoundTimer::linkClient            WRONG ARGUMENT: operator new(0x18) instead of new(0xc)
//   0x000287c0  SoundWindowObj_unsubclass         WRONG ARGUMENT: SetWindowLongA index -16 instead of -4
//   0x0002aa90  postSoundThreadMessage            WRONG ARGUMENT: Sleep(0x65) instead of Sleep(0x64)
//   0x00010db0  DsBuffer::play                    WRONG ARGUMENT: Play gets flags 0 instead of the stack argument
//   0x00010be0  DsBuffer::getTimedPosition        MISSING CALL: the reset branch never calls Stop (+0x48)
//   0x00008ce0  SoundTimer::start                 WRONG ARGUMENT: timeSetEvent gets fuEvent 0 instead of [+0x1c]
//   0x00008d70  SoundTimer::killEvent             MISSING CALL: timeEndPeriod is never called
//   0x000086f0  openMmioFile                      WRONG FIELD: the MMIOINFO's cchBuffer is 0x2000
//   0x00039140  WaveInDevice_close                MISSING CALL: waveInReset is never called
//   0x00006330  closeMmioNodeList                 MISSING CALL: mmioClose is never called
//   0x00031490  freeMmioHandleList                MISSING CALL: the element is never handed to operator delete
//   0x00006420  Wave_Device::openMemoryMmio       WRONG ARGUMENT: mmioGetInfo gets this+0x68 instead of this+0x6c
//   0x0002a8a0  stopSoundThreadFull               WRONG ARGUMENT: Sleep(1) instead of Sleep(0)
//   0x000135a0  Ds3DListener::setPosition         WRONG ARGUMENT: the COM call's last argument is 1 instead of 0
//   0x00021240  Sound_findByName                  MISSING WRITE: the walk cursor 0x100b4ff0 is not advanced
//   0x0001ef10  Midi_Device::flushMessages        MISSING WRITE: byte 2 of the entry is not cleared before the send
//   0x00023ea0  Voice_requestCmd12                WRONG ARGUMENT: the queued command is 0xd instead of 0xc
//   0x0003aa90  codecAllocContextA                WRONG ARGUMENT: malloc(0x2540) instead of 0x2544
//   0x00030fb0  Channel_setFormat                 EXTRA CALL: a second codecAllocContextB (a second allocation)
//   0x0004588f  _errno                            WRONG OFFSET: the __doserrno slot (+0xc) instead of +8
//   0x00023c20  Seq_cmdHandler12                  WRONG BIT: the flag goes to bit 3 instead of bit 4
//   0x000470ac  sbhRegionLocate                   WRONG CONSTANT: page + 4 instead of page + 8
//   0x00038710  WaveInDevice_dtor                 WRONG ARGUMENT: the element destructor gets flags 0 (the element is not released)
//   0x0001ced0  Sound_setName                     MISSING CALL: the old name is not handed to operator delete
//   0x0002d100  allocVoiceSlot                    MISSING CALL: the replaced slots are not handed to operator delete
//   0x0002ed50  buildVoiceRxName                  WRONG ARGUMENT: operator new(0x20) instead of new(0xa)
//   0x0000dbe0  postNotification                 WRONG ARGUMENT: PostMessageA gets message 0x7ef instead of 0x7ee
//   0x00020b60  postMidiNotification              WRONG FIELD: PostMessageA gets the window at +0x214 instead of +0x210
//   0x00038fd0  onWaveInCallbackMessage           WRONG ORDER: wParam and lParam swapped
//   0x00008bf0  SoundTimer::stopKeepLock          MISSING CALL: the drained client's virtual slot +0xc is not called
#include "hooks.h"

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
char* snd(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F fn(unsigned rva) { return reinterpret_cast<F>(snd(rva)); }
template <typename F> F imp(unsigned slotRva) { return *reinterpret_cast<F*>(snd(slotRva)); }

typedef unsigned(__stdcall* CsFn_t)(void*);
typedef void*(__cdecl* OpNew_t)(unsigned);
typedef void(__cdecl* OpDelete_t)(void*);
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);
typedef void(__fastcall* Th0v_t)(void*, void*);
typedef unsigned(__stdcall* Imp1_t)(unsigned);
typedef unsigned(__stdcall* Imp3_t)(void*, int, void*);
typedef unsigned(__stdcall* Imp4_t)(unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__stdcall* Imp5_t)(unsigned, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__stdcall* Imp0_t)();
typedef unsigned(__stdcall* MmClose_t)(unsigned, unsigned);
typedef unsigned(__stdcall* MmOpen_t)(const char*, void*, unsigned);
typedef unsigned(__stdcall* MmGetInfo_t)(unsigned, void*, unsigned);
typedef unsigned(__stdcall* Com1_t)(void*);
typedef unsigned(__stdcall* Com4_t)(void*, unsigned, unsigned, unsigned);
typedef unsigned(__stdcall* Com5_t)(void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__cdecl* Cdecl1_t)(unsigned);

void* c3_new(unsigned n) { return fn<OpNew_t>(0x000424d7)(n); }
void c3_delete(void* p) { fn<OpDelete_t>(0x0004249a)(p); }
void* cs() { return snd(0x000b4990); }
static inline void* slot(void* obj, unsigned off) { return fat<void*>(fat<void*>(obj, 0), off); }
static inline unsigned& gTick() { return *reinterpret_cast<unsigned*>(snd(0x000b49a8)); }
static inline unsigned& gMmio() { return *reinterpret_cast<unsigned*>(snd(0x000b4a08)); }

// ---- unregisterClient without operator delete
typedef unsigned(__fastcall* UnregClient_t)(void*, void*, void*);
UnregClient_t UnregClient_orig;
unsigned __fastcall UnregClient_re(void* self, void*, void* client) {
    imp<CsFn_t>(0x000b83f4)(cs());
    void* node = fat<void*>(self, 0x4c);
    while (node && fat<void*>(node, 8) != client) node = fat<void*>(node, 4);
    bool found = false;
    if (node) {
        void* prev = fat<void*>(node, 0);
        if (prev) fat<void*>(prev, 4) = fat<void*>(node, 4);
        else fat<void*>(self, 0x4c) = fat<void*>(node, 4);
        void* next = fat<void*>(node, 4);
        if (next) {
            fat<void*>(next, 0) = fat<void*>(node, 0);
            fat<void*>(self, 0x54) = next;
        } else {
            void* p = fat<void*>(node, 0);
            fat<unsigned>(self, 0x54) = 0;
            fat<void*>(self, 0x50) = p;
        }
        found = fat<void*>(node, 8) != 0;
        // DEFECT: c3_delete(node) is missing
        fat<unsigned>(self, 0x58) -= 1;
    }
    imp<CsFn_t>(0x000b84e0)(cs());
    void* vtbl = fat<void*>(client, 0);
    reinterpret_cast<Th0v_t>(fat<void*>(vtbl, 0xc))(client, 0);
    return found ? 0u : 0x14u;
}

// ---- linkClient / registerClient
typedef unsigned(__fastcall* LinkClient_t)(void*, void*, unsigned);
LinkClient_t LinkClient_orig;
unsigned __fastcall LinkClient_re(void* self, void*, unsigned client) {
    char* node = static_cast<char*>(c3_new(0x18));                             // DEFECT: 0x18 instead of 0xc
    if (node) { fat<unsigned>(node, 0) = 0; fat<unsigned>(node, 4) = 0; fat<unsigned>(node, 8) = client; }
    void* front = fat<void*>(self, 0);
    if (front) {
        fat<void*>(front, 0) = node;
        fat<void*>(node, 4) = fat<void*>(self, 0);
        fat<void*>(self, 0) = node;
    } else {
        fat<void*>(self, 0) = node;
        fat<void*>(self, 4) = node;
    }
    unsigned n = fat<unsigned>(self, 0xc) + 1;
    fat<unsigned>(self, 0xc) = n;
    return n;
}
typedef unsigned(__fastcall* RegClient_t)(void*, void*, unsigned);
RegClient_t RegClient_orig;
unsigned __fastcall RegClient_re(void* self, void*, unsigned client) {
    imp<CsFn_t>(0x000b83f4)(cs());
    fn<Th1_t>(0x00009040)(static_cast<char*>(self) + 0x4c, 0, client);
    return 0;                                                                  // DEFECT: LeaveCriticalSection is never called
}

// ---- SoundWindowObj_unsubclass with the wrong GWL index
typedef unsigned(__fastcall* Unsub_t)(void*, void*);
Unsub_t Unsub_orig;
unsigned __fastcall Unsub_re(void* self, void*) {
    void* hwnd = fat<void*>(self, 8);
    if (hwnd != 0 && (fat<unsigned char>(self, 4) & 4) != 0) {
        imp<Imp3_t>(0x000b85a4)(hwnd, -16, fat<void*>(self, 0xc));            // DEFECT: -16 instead of -4
        unsigned char flags = fat<unsigned char>(self, 4);
        fat<unsigned>(self, 0xc) = 0;
        flags = static_cast<unsigned char>(flags & 0xfb);
        fat<unsigned>(self, 8) = 0;
        fat<unsigned char>(self, 4) = flags;
    }
    return 0;
}

// ---- postSoundThreadMessage with Sleep(0x65)
typedef unsigned(__fastcall* PostThr_t)(void*, void*, unsigned, unsigned, unsigned);
PostThr_t PostThr_orig;
unsigned __fastcall PostThr_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3) {
    if ((fat<unsigned char>(self, 0) & 1) == 0) return 0x15;
    Imp4_t post = imp<Imp4_t>(0x000b85b0);
    if (post(fat<unsigned>(self, 0x1c), a1, a2, a3) == 0) {
        do {
            imp<Imp1_t>(0x000b83e8)(reinterpret_cast<unsigned>(snd(0x00063e68)));
            imp<Imp1_t>(0x000b840c)(0x65);                                     // DEFECT: 0x65 instead of 0x64
        } while (post(fat<unsigned>(self, 0x1c), a1, a2, a3) == 0);
    }
    return 0;
}

// ---- DsBuffer::play with Play(…, 0)
typedef unsigned(__fastcall* Play_t)(void*, void*, unsigned);
Play_t Play_orig;
unsigned __fastcall Play_re(void* self, void*, unsigned flags) {
    (void)flags;
    void* com = fat<void*>(self, 0x60);
    if (com == 0) return 0x14;
    fat<unsigned>(self, 0x54) = imp<Imp0_t>(0x000b8654)();
    com = fat<void*>(self, 0x60);
    if (reinterpret_cast<Com4_t>(slot(com, 0x30))(com, 0, 0, 0) != 0) return 0x1c;   // DEFECT: flags 0
    com = fat<void*>(self, 0x60);
    reinterpret_cast<Com4_t>(slot(com, 0x14))(com, reinterpret_cast<unsigned>(static_cast<char*>(self) + 0xa8), 0x12, 0);
    return 0;
}

// ---- DsBuffer::getTimedPosition without the Stop call in the reset branch
typedef unsigned(__fastcall* TimedPos_t)(void*, void*, unsigned*, unsigned*, unsigned);
TimedPos_t TimedPos_orig;
static inline unsigned div1000(unsigned x) {
    return static_cast<unsigned>((static_cast<unsigned long long>(0x10624dd3u) * x) >> 32) >> 6;
}
unsigned __fastcall TimedPos_re(void* self, void*, unsigned* out1, unsigned* out2, unsigned a3) {
    unsigned edx;
    unsigned rate = fat<unsigned>(self, 0xac);
    if (fat<unsigned>(self, 0x4c) == 0) {
        unsigned now = imp<Imp0_t>(0x000b8654)();
        unsigned elapsed = now - fat<unsigned>(self, 0x54);
        rate = fat<unsigned>(self, 0xac);
        fat<unsigned>(self, 0x4c) = elapsed;
        unsigned x = (elapsed + fat<unsigned>(self, 0x50) * 2) * rate;
        unsigned y = rate * a3;
        fat<unsigned>(self, 0x48) = div1000(x);
        edx = div1000(y);
    } else {
        unsigned t = fat<unsigned>(self, 0x48) * 5 * 5 * 5 * 8;
        unsigned hi = static_cast<unsigned>((static_cast<unsigned long long>(0xbe37c63bu) * t) >> 32);
        unsigned q = hi >> 14;
        unsigned cur = a3 + fat<unsigned>(self, 0x4c);
        fat<unsigned>(self, 0x4c) = cur;
        unsigned lead = fat<unsigned>(self, 0x50) * 10;
        unsigned z = (lead - q + cur) * fat<unsigned>(self, 0xac);
        edx = div1000(z);
    }
    unsigned align = fat<unsigned short>(self, 0xb4);
    *out1 = align * edx;
    unsigned align2 = fat<unsigned short>(self, 0xb4);
    *out2 = (align2 * fat<unsigned>(self, 0x48)) & fat<unsigned>(self, 0x58);
    unsigned len = fat<unsigned>(self, 0x78);
    fat<unsigned>(self, 0x48) = fat<unsigned>(self, 0x48) + edx;
    if (*out1 <= len) return 0;
    void* com = fat<void*>(self, 0x60);
    fat<unsigned>(self, 0x4c) = 0;
    fat<unsigned>(self, 0x48) = 0;
    (void)com;                                                                 // DEFECT: the Stop call (slot +0x48) is missing
    if (fat<void*>(self, 0x60) != 0) {
        fat<unsigned>(self, 0x54) = imp<Imp0_t>(0x000b8654)();
        com = fat<void*>(self, 0x60);
        if (reinterpret_cast<Com4_t>(slot(com, 0x30))(com, 0, 0, 1) == 0) {
            com = fat<void*>(self, 0x60);
            reinterpret_cast<Com4_t>(slot(com, 0x14))(com, reinterpret_cast<unsigned>(static_cast<char*>(self) + 0xa8), 0x12, 0);
        }
    }
    *out1 = 0;
    return 0x27;
}

// ---- SoundTimer::start with fuEvent 0 / killEvent without timeEndPeriod
typedef unsigned(__fastcall* TimerStart_t)(void*, void*);
TimerStart_t TimerStart_orig;
unsigned __fastcall TimerStart_re(void* self, void*) {
    if ((fat<unsigned char>(self, 0x3c) & 4) != 0) return 0xd;
    gTick() = 0;
    if (imp<Imp1_t>(0x000b8640)(fat<unsigned>(self, 0x18)) == 0x61) return 0xb;
    unsigned id = imp<Imp5_t>(0x000b8644)(fat<unsigned>(self, 0), fat<unsigned>(self, 0x18),
        reinterpret_cast<unsigned>(snd(0x0000128a)), reinterpret_cast<unsigned>(self), 0);   // DEFECT: fuEvent 0
    fat<unsigned>(self, 0x40) = id;
    if (id == 0x61) return 0xb;
    unsigned f = fat<unsigned>(self, 0x3c);
    f = (f & ~0xffu) | static_cast<unsigned char>(f | 7);
    fat<unsigned>(self, 0x3c) = f;
    return 0;
}
typedef unsigned(__fastcall* KillEvent_t)(void*, void*);
KillEvent_t KillEvent_orig;
unsigned __fastcall KillEvent_re(void* self, void*) {
    if ((fat<unsigned char>(self, 0x3c) & 1) == 0) return 0x15;
    gTick() = gTick() + 1;
    fat<unsigned>(self, 0x3c) = fat<unsigned>(self, 0x3c) & 0xfffffffbu;
    imp<CsFn_t>(0x000b83f4)(cs());
    imp<CsFn_t>(0x000b84e0)(cs());
    fat<unsigned>(self, 8) = 0;
    fat<unsigned>(self, 4) = 0;
    imp<Imp1_t>(0x000b8648)(fat<unsigned>(self, 0x40));
    // DEFECT: timeEndPeriod is never called
    unsigned f = fat<unsigned>(self, 0x3c);
    f = (f & ~0xffu) | static_cast<unsigned char>(f & 0xfe);
    fat<unsigned>(self, 0x3c) = f;
    return 0;
}

// ---- openMmioFile with cchBuffer 0x2000
typedef unsigned(__fastcall* OpenMmioFile_t)(void*, void*, const char*);
OpenMmioFile_t OpenMmioFile_orig;
unsigned __fastcall OpenMmioFile_re(void* self, void*, const char* name) {
    unsigned info[0x12];
    for (int i = 0; i < 0x12; i++) info[i] = 0;
    info[1] = 0x204d454d;
    info[5] = 0x2000;                                                          // DEFECT: 0x2000 instead of 0x1000
    unsigned h = imp<MmOpen_t>(0x000b8660)(name, info, 0x11001);
    fat<unsigned>(self, 0x30) = h;
    if (h == 0) return 8;
    gMmio() = gMmio() + 1;
    return 0;
}

// ---- Wave_Device::openMemoryMmio with mmioGetInfo into this+0x68
typedef unsigned(__fastcall* OpenMemMmio_t)(void*, void*, unsigned);
OpenMemMmio_t OpenMemMmio_orig;
unsigned __fastcall OpenMemMmio_re(void* self, void*, unsigned size) {
    if (fat<unsigned>(self, 0xb0) != 0) return 0;
    unsigned info[0x12];
    for (int i = 0; i < 0x12; i++) info[i] = 0;
    info[1] = 0x204d454d;
    unsigned len = size;
    if (len == 0) len = fat<unsigned>(self, 0xb8);
    info[5] = len;
    info[12] = 0x64;
    unsigned h = imp<MmOpen_t>(0x000b8660)(0, info, 0x1000);
    if (h == 0) return 0;
    gMmio() = gMmio() + 1;
    char* dst = static_cast<char*>(self) + 0x6c;
    imp<MmGetInfo_t>(0x000b8668)(h, static_cast<char*>(self) + 0x68, 0);       // DEFECT: this+0x68
    return reinterpret_cast<unsigned>(dst);
}

// ---- WaveInDevice_close without waveInReset
typedef unsigned(__fastcall* WaveInClose_t)(void*, void*);
WaveInClose_t WaveInClose_orig;
static unsigned waveInErr(unsigned r) {
    static const unsigned char idx[0x20] = {0, 6, 1, 0, 2, 3, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 5};
    static const unsigned val[7] = {0xa, 0xc, 1, 0x11, 0x12, 0xe, 0xb};
    unsigned e = r - 2;
    if (e > 0x1f) return 0xb;
    return val[idx[e]];
}
typedef unsigned(__stdcall* WaveInUnprep_t)(unsigned, void*, unsigned);
unsigned __fastcall WaveInClose_re(void* self, void*) {
    if (fat<unsigned>(self, 0x44) == 0) return 0x13;
    void* c = snd(0x000b503c);
    imp<CsFn_t>(0x000b83f4)(c);
    unsigned f = fat<unsigned>(self, 4);
    f = (f & ~0xffu) | static_cast<unsigned char>(f & 0xfd);
    fat<unsigned>(self, 4) = f;
    imp<Imp1_t>(0x000b8614)(fat<unsigned>(self, 0x44));
    // DEFECT: waveInReset is never called
    char* node = fat<char*>(self, 0x5c);
    fat<void*>(self, 0x64) = node;
    if (node) {
        void* hdr = fat<void*>(node, 8);
        if (hdr && fat<unsigned>(self, 0x44) != 0) {
            for (;;) {
                if (imp<WaveInUnprep_t>(0x000b8608)(fat<unsigned>(self, 0x44), hdr, 0x20) != 0) break;
                char* cur = fat<char*>(self, 0x64);
                if (!cur) break;
                cur = fat<char*>(cur, 4);
                fat<void*>(self, 0x64) = cur;
                if (!cur) break;
                hdr = fat<void*>(cur, 8);
                if (!hdr) break;
            }
        }
    }
    unsigned r = imp<Imp1_t>(0x000b85fc)(fat<unsigned>(self, 0x44));
    if (r != 0) return waveInErr(r);
    imp<CsFn_t>(0x000b84e0)(c);
    fat<unsigned>(self, 0x44) = 0;
    return 0;
}

// ---- closeMmioNodeList without mmioClose / freeMmioHandleList without the element delete
static void* popFront(void* self, unsigned frontOff, unsigned backOff, unsigned countOff, char* node) {
    char* next = fat<char*>(node, 4);
    if (next) { fat<unsigned>(next, 0) = 0; fat<void*>(self, frontOff) = next; }
    else { fat<unsigned>(self, backOff) = 0; fat<unsigned>(self, frontOff) = 0; }
    void* elem = fat<void*>(node, 8);
    c3_delete(node);
    fat<unsigned>(self, countOff) = fat<unsigned>(self, countOff) - 1;
    return elem;
}
typedef unsigned(__fastcall* CloseMmioList_t)(void*, void*);
CloseMmioList_t CloseMmioList_orig;
unsigned __fastcall CloseMmioList_re(void* self, void*) {
    reinterpret_cast<Th0v_t>(slot(self, 0x20))(self, 0);
    if ((fat<unsigned char>(self, 0x44) & 2) != 0) {
        fat<unsigned>(self, 0x58) = fat<unsigned>(self, 0x58) | 8;
        return 0;
    }
    char* node = fat<char*>(self, 0x5c);
    if (node) {
        void* elem = popFront(self, 0x5c, 0x60, 0x68, node);
        while (elem) {
            unsigned h = fat<unsigned>(elem, 0x44);
            if (h) {
                // DEFECT: mmioClose is never called
                gMmio() = gMmio() - 1;
            }
            fn<Th0v_t>(0x00037df0)(elem, 0);
            c3_delete(elem);
            node = fat<char*>(self, 0x5c);
            if (!node) break;
            elem = popFront(self, 0x5c, 0x60, 0x68, node);
        }
    }
    return 0;
}
typedef unsigned(__fastcall* FreeMmioList_t)(void*, void*);
FreeMmioList_t FreeMmioList_orig;
unsigned __fastcall FreeMmioList_re(void* self, void*) {
    char* node = fat<char*>(self, 0x5c);
    if (node) {
        void* elem = popFront(self, 0x5c, 0x60, 0x68, node);
        while (elem) {
            imp<MmClose_t>(0x000b8670)(fat<unsigned>(elem, 0), 0);
            gMmio() = gMmio() - 1;
            // DEFECT: the element is never handed to operator delete
            node = fat<char*>(self, 0x5c);
            if (!node) break;
            elem = popFront(self, 0x5c, 0x60, 0x68, node);
        }
    }
    fat<unsigned>(self, 0x74) = 0;
    return 0;
}

// ---- stopSoundThreadFull with Sleep(1)
typedef void(__fastcall* StopThr_t)(void*, void*);
StopThr_t StopThr_orig;
void __fastcall StopThr_re(void* self, void*) {
    unsigned char al = fat<unsigned char>(self, 0);
    if ((al & 1) == 0) return;
    void* c = static_cast<char*>(self) + 4;
    al = static_cast<unsigned char>(al & 0xfe);
    fat<unsigned char>(self, 0) = al;
    imp<CsFn_t>(0x000b83f4)(c);
    unsigned char dl = fat<unsigned char>(self, 0);
    dl = static_cast<unsigned char>(dl & 0xfe);
    fat<unsigned char>(self, 0) = dl;
    imp<Imp1_t>(0x000b83fc)(fat<unsigned>(self, 0x24));
    imp<CsFn_t>(0x000b84e0)(c);
    imp<Imp1_t>(0x000b840c)(1);                                                // DEFECT: Sleep(1)
    imp<CsFn_t>(0x000b83f4)(c);
    imp<CsFn_t>(0x000b84e0)(c);
    fat<unsigned>(self, 0x20) = 0;
    fat<unsigned>(self, 0x1c) = 0;
}

// ---- Ds3DListener::setPosition with apply = 1
typedef unsigned(__fastcall* ListenerPos_t)(void*, void*, unsigned, unsigned, unsigned);
ListenerPos_t ListenerPos_orig;
unsigned __fastcall ListenerPos_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3) {
    void* com = fat<void*>(self, 0xd8);
    if (com != 0) {
        unsigned r = reinterpret_cast<Com5_t>(slot(com, 0x38))(com, a1, a2, a3, 1);   // DEFECT: 1 instead of 0
        if (r != 0) return fn<Cdecl1_t>(0x0000f480)(r);
    }
    fat<unsigned>(self, 0x1ac) = a1;
    fat<unsigned>(self, 0x1b0) = a2;
    fat<unsigned>(self, 0x1b4) = a3;
    return 0;
}


// ---- batch c3dh, second wave
typedef unsigned(__stdcall* MidiShort_t)(unsigned, unsigned);
typedef unsigned(__fastcall* Th0u_t)(void*, void*);
typedef unsigned(__cdecl* Malloc_t)(unsigned);
typedef unsigned(__cdecl* Cdecl1p_t)(unsigned);
typedef unsigned(__cdecl* Cdecl0_t)();
typedef unsigned(__fastcall* Th5_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* ListGetAt_t)(void*, void*, void*, unsigned);

// Sound_findByName without the cursor write at the end of each step
typedef unsigned char(__stdcall* FindByName_t)(const char*);
FindByName_t FindByName_orig;
unsigned char __stdcall FindByName_re(const char* name) {
    char* node = *reinterpret_cast<char**>(snd(0x000b4fe8));
    *reinterpret_cast<char**>(snd(0x000b4ff0)) = node;
    while (node != 0) {
        char* sound = fat<char*>(node, 8);
        if (sound == 0) break;
        const char* a = fat<const char*>(sound, 0);
        const char* b = name;
        bool equal;
        for (;;) {
            unsigned char x = static_cast<unsigned char>(a[0]);
            unsigned char y = static_cast<unsigned char>(b[0]);
            if (x != y) { equal = false; break; }
            if (x == 0) { equal = true; break; }
            x = static_cast<unsigned char>(a[1]);
            y = static_cast<unsigned char>(b[1]);
            if (x != y) { equal = false; break; }
            a += 2; b += 2;
            if (x == 0) { equal = true; break; }
        }
        if (equal) return 1;
        node = fat<char*>(node, 4);
        // DEFECT: the cursor 0x100b4ff0 is not updated for the next node
    }
    return 0;
}

// flushMessages without clearing byte 2 of the entry before the send
typedef void(__fastcall* FlushMsg_t)(void*, void*);
FlushMsg_t FlushMsg_orig;
void __fastcall FlushMsg_re(void* self, void*) {
    char* dev = *reinterpret_cast<char**>(snd(0x000b4a1c));
    if (fat<unsigned>(dev, 0x3c) == 0) return;
    MidiShort_t shortMsg = imp<MidiShort_t>(0x000b8638);
    char* p = static_cast<char*>(self) + 0x258;
    for (int i = 0; i < 0x80; i++, p += 4) {
        if (*reinterpret_cast<unsigned char*>(p) == 0) continue;
        // DEFECT: p[2] = 0 is missing
        char* d2 = *reinterpret_cast<char**>(snd(0x000b4a1c));
        unsigned msg = *reinterpret_cast<unsigned*>(p);
        shortMsg(fat<unsigned>(d2, 0x3c), msg);
        *reinterpret_cast<unsigned*>(p) = 0;
    }
}

// Voice_requestCmd12 pushing command 0xd
typedef unsigned(__fastcall* VoiceReq12_t)(void*, void*, unsigned, unsigned);
VoiceReq12_t VoiceReq12_orig;
unsigned __fastcall VoiceReq12_re(void* self, void*, unsigned a1, unsigned a2) {
    unsigned limit = reinterpret_cast<Th0u_t>(slot(self, 0xb4))(self, 0);
    if (a1 > limit) return 0xa;
    void* dev = *reinterpret_cast<void**>(snd(0x000b4a1c));
    if (dev == 0) return 0x13;
    return fn<Th5_t>(0x0000a240)(dev, 0, reinterpret_cast<unsigned>(self), 0xd, a1, a2 & 0xff);   // DEFECT: 0xd
}

// codecAllocContextA asking malloc for 0x2540 bytes
typedef unsigned(__cdecl* AllocCtx_t)();
AllocCtx_t AllocCtxA_orig;
unsigned __cdecl AllocCtxA_re() {
    unsigned p = fn<Malloc_t>(0x00042acd)(0x2540);                             // DEFECT: 0x2540
    if (p != 0) fn<Cdecl1p_t>(0x0003aae0)(p);
    return p;
}

// Channel_setFormat allocating the codec context twice
typedef unsigned(__fastcall* SetFormat_t)(void*, void*, unsigned, unsigned, unsigned);
SetFormat_t SetFormat_orig;
unsigned __fastcall SetFormat_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3) {
    (void)a3;
    if (a1 == 0) return 0xa;
    char* dst = static_cast<char*>(self) + 0xbc;
    const char* src = a2 != 0 ? reinterpret_cast<const char*>(static_cast<uintptr_t>(a2)) : snd(0x00063ea0);
    for (int i = 0; i < 4; i++) fat<unsigned>(dst, 4 * i) = *reinterpret_cast<const unsigned*>(src + 4 * i);
    fat<unsigned short>(dst, 0x10) = *reinterpret_cast<const unsigned short*>(src + 0x10);
    fat<unsigned>(self, 0xb4) = a1;
    fn<Cdecl0_t>(0x0003aea0)();                                                // DEFECT: a second, unused allocation
    fat<unsigned>(self, 0xd4) = fn<Cdecl0_t>(0x0003aea0)();
    return 0;
}

// _errno returning the __doserrno slot
typedef unsigned(__cdecl* Errno_t)();
Errno_t Errno_orig;
unsigned __cdecl Errno_re() { return fn<Cdecl0_t>(0x00044171)() + 0xc; }       // DEFECT: +0xc

// Seq_cmdHandler12 storing the flag in bit 3
typedef unsigned(__fastcall* SeqCmd12_t)(void*, void*, unsigned, unsigned);
SeqCmd12_t SeqCmd12_orig;
unsigned __fastcall SeqCmd12_re(void* self, void*, unsigned index, unsigned flag) {
    void* slotValue = reinterpret_cast<void*>(static_cast<uintptr_t>(index));
    unsigned r = fn<ListGetAt_t>(0x0001d780)(static_cast<char*>(self) + 0x60, 0, &slotValue, index);
    if (r != 0) return r;
    unsigned char cl = static_cast<unsigned char>(flag & 1);
    cl = static_cast<unsigned char>(cl << 3);                                  // DEFECT: << 3
    unsigned char dl = fat<unsigned char>(slotValue, 0x38);
    dl = static_cast<unsigned char>(dl & 0xef);
    fat<unsigned char>(slotValue, 0x38) = static_cast<unsigned char>(cl | dl);
    return r;
}

// sbhRegionLocate returning page + 4 instead of page + 8
typedef unsigned(__cdecl* SbhLocate_t)(unsigned, unsigned*, unsigned*);
SbhLocate_t SbhLocate_orig;
unsigned __cdecl SbhLocate_re(unsigned p, unsigned* outRegion, unsigned* outPage) {
    char* head = snd(0x000653d8);
    char* r = head;
    for (;;) {
        if (p > fat<unsigned>(r, 0x10) && p < fat<unsigned>(r, 0x14)) break;
        r = fat<char*>(r, 0);
        if (r == head) return 0;
    }
    if ((p & 0xf) != 0) return 0;
    if ((p & 0xfff) < 0x100) return 0;
    *outRegion = reinterpret_cast<unsigned>(r);
    unsigned page = p & 0xfffff000u;
    unsigned off = p - page;
    *outPage = page;
    int d = static_cast<int>(off - 0x100) >> 4;
    return static_cast<unsigned>(d) + page + 4;                                // DEFECT: + 4
}


// WaveInDevice_dtor calling the element destructor with flags 0 (the element itself is not released)
typedef void(__fastcall* WaveInDtor_t)(void*, void*);
WaveInDtor_t WaveInDtor_orig;
static void* popNode(char* list, char* node) {
    char* next = fat<char*>(node, 4);
    if (next) { fat<unsigned>(next, 0) = 0; fat<void*>(list, 0) = next; }
    else { fat<unsigned>(list, 4) = 0; fat<unsigned>(list, 0) = 0; }
    void* elem = fat<void*>(node, 8);
    c3_delete(node);
    fat<unsigned>(list, 0xc) = fat<unsigned>(list, 0xc) - 1;
    return elem;
}
void __fastcall WaveInDtor_re(void* self, void*) {
    fat<void*>(self, 0) = snd(0x0005c604);
    unsigned char al = fat<unsigned char>(self, 4);
    Imp1_t stop = imp<Imp1_t>(0x000b8614);
    Imp1_t reset = imp<Imp1_t>(0x000b8618);
    if ((al & 2) != 0) {
        stop(fat<unsigned>(self, 0x44));
        reset(fat<unsigned>(self, 0x44));
        fn<Th0u_t>(0x00039140)(self, 0);
    }
    if ((fat<unsigned char>(self, 4) & 1) != 0) {
        stop(fat<unsigned>(self, 0x44));
        reset(fat<unsigned>(self, 0x44));
    }
    char* list = static_cast<char*>(self) + 0x5c;
    char* node = fat<char*>(list, 0);
    if (node) {
        void* elem = popNode(list, node);
        while (elem) {
            fn<Th1_t>(0x00038ae0)(elem, 0, 0);                                 // DEFECT: flags 0 instead of 1
            elem = reinterpret_cast<void*>(static_cast<uintptr_t>(fn<Th0u_t>(0x00039620)(list, 0)));
        }
    }
    node = fat<char*>(list, 0);
    if (node) {
        void* elem = popNode(list, node);
        while (elem) {
            node = fat<char*>(list, 0);
            if (!node) break;
            elem = popNode(list, node);
        }
    }
}


// Sound_setName without releasing the old name
typedef void(__fastcall* SetName_t)(void*, void*, const char*);
SetName_t SetName_orig;
void __fastcall SetName_re(void* self, void*, const char* name) {
    char buf[0x100];
    // DEFECT: the old name at +0x1c is not handed to operator delete
    fat<void*>(self, 0x1c) = 0;
    if (name == 0) return;
    const char* prefix = fat<const char*>(self, 0x20);
    if (prefix != 0) {
        unsigned n = 0;
        while (prefix[n]) { buf[n] = prefix[n]; n++; }
        buf[n] = 0;
        const char* slash = strrchr(name, '\\');
        const char* src = slash != 0 ? slash + 1 : name;
        unsigned m = 0;
        while (src[m]) { buf[n + m] = src[m]; m++; }
        buf[n + m] = 0;
    } else {
        unsigned n = 0;
        while (name[n]) { buf[n] = name[n]; n++; }
        buf[n] = 0;
    }
    unsigned len = 0;
    while (buf[len]) len++;
    char* p = static_cast<char*>(c3_new(len + 1));
    fat<void*>(self, 0x1c) = p;
    for (unsigned i = 0; i <= len; i++) p[i] = buf[i];
}

// allocVoiceSlot without releasing the replaced slots
typedef unsigned(__fastcall* AllocSlot_t)(void*, void*, void*);
AllocSlot_t AllocSlot_orig;
unsigned __fastcall AllocSlot_re(void* table, void*, void* desc) {
    unsigned hi = fat<unsigned>(desc, 0);
    unsigned lo = fat<unsigned>(desc, 4);
    char* slot = static_cast<char*>(c3_new(0x110));
    if (slot) {
        for (unsigned k = 0xc; k < 0x110; k += 4) fat<unsigned>(slot, k) = 0;
        fat<unsigned>(slot, 4) = 0;
        fat<unsigned>(slot, 0) = 0;
        fat<unsigned>(slot, 8) = 1;
    }
    const char* nm = static_cast<const char*>(desc) + 0xc;
    unsigned n = 0;
    while (nm[n]) n++;
    for (unsigned i = 0; i <= n; i++) slot[0xc + i] = nm[i];
    fat<unsigned>(slot, 4) = lo;
    if (lo > hi) fat<unsigned>(slot, 4) = hi;
    fat<unsigned>(slot, 0) = hi;
    unsigned value = fat<unsigned>(desc, 8);
    fat<unsigned>(slot, 8) = value;
    if (lo > hi) return value;
    unsigned i = lo;
    do {
        char* old = fat<char*>(table, 4 * i + 4);
        if (old != 0) {
            unsigned oldLo = fat<unsigned>(old, 4);
            unsigned oldHi1 = fat<unsigned>(old, 0) + 1;
            // DEFECT: c3_delete(table[oldLo]) is missing
            if (oldLo < oldHi1)
                for (unsigned k = 0; k < oldHi1 - oldLo; k++) fat<unsigned>(table, 4 * (oldLo + k) + 4) = 0;
            i = oldHi1;
        }
        i = i + 1;
    } while (i <= hi);
    for (unsigned k = lo; k <= hi; k++) fat<void*>(table, 4 * k + 4) = slot;
    return reinterpret_cast<unsigned>(slot);
}

// buildVoiceRxName asking operator new for 0x20 bytes
typedef void*(__fastcall* BuildRx_t)(void*, void*);
BuildRx_t BuildRx_orig;
void* __fastcall BuildRx_re(void* self, void*) {
    fat<unsigned>(self, 4) = 0x7f;
    fat<unsigned>(self, 8) = 0;
    for (unsigned k = 0xc; k < 0x30; k += 4) fat<unsigned>(self, k) = 0;
    fat<unsigned>(self, 0x30) = 0;
    fat<void*>(self, 0) = snd(0x0005bae0);
    fat<unsigned>(self, 0x48) = 0;
    fat<unsigned>(self, 0x4c) = 0;
    fat<unsigned>(self, 0x40) = 0;
    fat<unsigned>(self, 0x50) = 0;
    fat<unsigned>(self, 0x44) = 0;
    fat<unsigned>(self, 0x38) = 0x3e8;
    fat<unsigned>(self, 0x54) = 0;
    fat<void*>(self, 0) = snd(0x0005c234);
    char buf[9];
    for (int k = 0; k < 9; k++) buf[k] = snd(0x00063e80)[k];
    fn<Th1_t>(0x0002ad10)(self, 0, 6);
    char* p = static_cast<char*>(c3_new(0x20));                                // DEFECT: 0x20 instead of 0xa
    fat<void*>(self, 0x50) = p;
    unsigned n = 0;
    while (buf[n]) n++;
    for (unsigned i = 0; i <= n; i++) p[i] = buf[i];
    return self;
}


// ---- notification posts
typedef unsigned(__stdcall* PostMsg_t)(unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th3p_t)(void*, void*, unsigned, unsigned, unsigned);
typedef void*(__fastcall* Th0p_t)(void*, void*);

// postNotification posting message 0x7ef
typedef unsigned(__fastcall* PostNote_t)(void*, void*, void*, unsigned);
PostNote_t PostNote_orig;
unsigned __fastcall PostNote_re(void* self, void*, void* rec, unsigned value) {
    if (rec == 0) return 0xa;
    char* node = static_cast<char*>(c3_new(0x10));
    if (node) node = static_cast<char*>(fn<Th0p_t>(0x000087f0)(node, 0));
    fat<void*>(node, 4) = rec;
    fat<unsigned>(node, 0) = value;
    fat<unsigned char>(rec, 0x1428) = static_cast<unsigned char>(fat<unsigned char>(rec, 0x1428) | 1);
    if (((fat<unsigned>(self, 0x1a4) >> 5) & 1) != 0) {
        imp<PostMsg_t>(0x000b85a0)(fat<unsigned>(rec, 0x1420), 0x7ef, 0, reinterpret_cast<unsigned>(node));   // DEFECT: 0x7ef
    } else {
        fn<Th3p_t>(0x0002aa90)(snd(0x000b5038), 0, 0x7ee, 0, reinterpret_cast<unsigned>(node));
    }
    return 0;
}

// postMidiNotification posting to the window at +0x214 instead of +0x210
typedef unsigned(__fastcall* PostMidi_t)(void*, void*);
PostMidi_t PostMidi_orig;
unsigned __fastcall PostMidi_re(void* self, void*) {
    if ((fat<unsigned>(self, 0xac) & 0x100) != 0) return 0;
    fat<unsigned>(self, 0x214) = fat<unsigned>(self, 0x214) | 0x10;
    char* eng = *reinterpret_cast<char**>(snd(0x000b4a20));
    if (eng != 0 && ((fat<unsigned>(eng, 0x1a4) >> 5) & 1) != 0) {
        imp<PostMsg_t>(0x000b85a0)(fat<unsigned>(self, 0x214), 0x7ec, 0, reinterpret_cast<unsigned>(self));   // DEFECT: +0x214
        return 0;
    }
    fn<Th3p_t>(0x0002aa90)(snd(0x000b5038), 0, 0x7ec, 0, reinterpret_cast<unsigned>(self));
    return 0;
}

// onWaveInCallbackMessage with wParam and lParam swapped
typedef void(__stdcall* WaveInCb_t)(unsigned, unsigned, unsigned, unsigned, unsigned);
WaveInCb_t WaveInCb_orig;
void __stdcall WaveInCb_re(unsigned a1, unsigned msg, unsigned inst, unsigned p1, unsigned p2) {
    (void)a1; (void)p2;
    if ((msg & 0xffff) != 0x3c0) return;
    char* eng = *reinterpret_cast<char**>(snd(0x000b4a20));
    if (eng != 0 && ((fat<unsigned>(eng, 0x1a4) >> 5) & 1) != 0) {
        imp<PostMsg_t>(0x000b85a0)(*reinterpret_cast<unsigned*>(snd(0x000b49c0)), 0x7f0, inst, p1);   // DEFECT: swapped
        return;
    }
    fn<Th3p_t>(0x0002aa90)(snd(0x000b5038), 0, 0x7f0, p1, inst);
}


// SoundTimer::stopKeepLock without the client's virtual slot call while draining the list
typedef void(__fastcall* StopKeep_t)(void*, void*);
StopKeep_t StopKeep_orig;
void __fastcall StopKeep_re(void* self, void*) {
    gTick() = gTick() + 1;
    imp<CsFn_t>(0x000b83f4)(cs());
    imp<CsFn_t>(0x000b84e0)(cs());
    if ((fat<unsigned char>(self, 0x3c) & 1) != 0) {
        gTick() = gTick() + 1;
        fat<unsigned>(self, 0x3c) = fat<unsigned>(self, 0x3c) & 0xfffffffbu;
        imp<CsFn_t>(0x000b83f4)(cs());
        imp<CsFn_t>(0x000b84e0)(cs());
        fat<unsigned>(self, 8) = 0;
        fat<unsigned>(self, 4) = 0;
        imp<Imp1_t>(0x000b8648)(fat<unsigned>(self, 0x40));
        imp<Imp1_t>(0x000b864c)(fat<unsigned>(self, 0x18));
        unsigned f = fat<unsigned>(self, 0x3c);
        f = (f & ~0xffu) | static_cast<unsigned char>(f & 0xfe);
        fat<unsigned>(self, 0x3c) = f;
    }
    for (;;) {
        char* node = fat<char*>(self, 0x4c);
        if (node == 0) break;
        char* next = fat<char*>(node, 4);
        if (next != 0) { fat<unsigned>(next, 0) = 0; fat<void*>(self, 0x4c) = next; }
        else { fat<unsigned>(self, 0x50) = 0; fat<unsigned>(self, 0x4c) = 0; }
        void* client = fat<void*>(node, 8);
        c3_delete(node);
        fat<unsigned>(self, 0x58) = fat<unsigned>(self, 0x58) - 1;
        if (client == 0) break;
        // DEFECT: the client's virtual slot +0xc is not called
    }
}

}  // namespace

SG_HOOK("sound.dll", 0x00008920, SoundTimer_unregisterClient, UnregClient_re, UnregClient_orig);
SG_HOOK("sound.dll", 0x000088e0, SoundTimer_registerClient, RegClient_re, RegClient_orig);
SG_HOOK("sound.dll", 0x00009040, SoundTimer_linkClient, LinkClient_re, LinkClient_orig);
SG_HOOK("sound.dll", 0x000287c0, SoundWindowObj_unsubclass, Unsub_re, Unsub_orig);
SG_HOOK("sound.dll", 0x0002aa90, postSoundThreadMessage, PostThr_re, PostThr_orig);
SG_HOOK("sound.dll", 0x00010db0, DsBuffer_play, Play_re, Play_orig);
SG_HOOK("sound.dll", 0x00010be0, DsBuffer_getTimedPosition, TimedPos_re, TimedPos_orig);
SG_HOOK("sound.dll", 0x00008ce0, SoundTimer_start, TimerStart_re, TimerStart_orig);
SG_HOOK("sound.dll", 0x00008d70, SoundTimer_killEvent, KillEvent_re, KillEvent_orig);
SG_HOOK("sound.dll", 0x000086f0, openMmioFile, OpenMmioFile_re, OpenMmioFile_orig);
SG_HOOK("sound.dll", 0x00039140, WaveInDevice_close, WaveInClose_re, WaveInClose_orig);
SG_HOOK("sound.dll", 0x00006330, closeMmioNodeList, CloseMmioList_re, CloseMmioList_orig);
SG_HOOK("sound.dll", 0x00031490, freeMmioHandleList, FreeMmioList_re, FreeMmioList_orig);
SG_HOOK("sound.dll", 0x00006420, Wave_Device_openMemoryMmio, OpenMemMmio_re, OpenMemMmio_orig);
SG_HOOK("sound.dll", 0x0002a8a0, stopSoundThreadFull, StopThr_re, StopThr_orig);
SG_HOOK("sound.dll", 0x000135a0, Ds3DListener_setPosition, ListenerPos_re, ListenerPos_orig);
SG_HOOK("sound.dll", 0x00021240, Sound_findByName, FindByName_re, FindByName_orig);
SG_HOOK("sound.dll", 0x0001ef10, Midi_Device_flushMessages, FlushMsg_re, FlushMsg_orig);
SG_HOOK("sound.dll", 0x00023ea0, Voice_requestCmd12, VoiceReq12_re, VoiceReq12_orig);
SG_HOOK("sound.dll", 0x0003aa90, codecAllocContextA, AllocCtxA_re, AllocCtxA_orig);
SG_HOOK("sound.dll", 0x00030fb0, Channel_setFormat, SetFormat_re, SetFormat_orig);
SG_HOOK("sound.dll", 0x0004588f, _errno, Errno_re, Errno_orig);
SG_HOOK("sound.dll", 0x00023c20, Seq_cmdHandler12, SeqCmd12_re, SeqCmd12_orig);
SG_HOOK("sound.dll", 0x000470ac, sbhRegionLocate, SbhLocate_re, SbhLocate_orig);
SG_HOOK("sound.dll", 0x00038710, WaveInDevice_dtor, WaveInDtor_re, WaveInDtor_orig);
SG_HOOK("sound.dll", 0x0001ced0, Sound_setName, SetName_re, SetName_orig);
SG_HOOK("sound.dll", 0x0002d100, allocVoiceSlot, AllocSlot_re, AllocSlot_orig);
SG_HOOK("sound.dll", 0x0002ed50, buildVoiceRxName, BuildRx_re, BuildRx_orig);
SG_HOOK("sound.dll", 0x0000dbe0, postNotification, PostNote_re, PostNote_orig);
SG_HOOK("sound.dll", 0x00020b60, postMidiNotification, PostMidi_re, PostMidi_orig);
SG_HOOK("sound.dll", 0x00038fd0, onWaveInCallbackMessage, WaveInCb_re, WaveInCb_orig);
SG_HOOK("sound.dll", 0x00008bf0, SoundTimer_stopKeepLock, StopKeep_re, StopKeep_orig);
