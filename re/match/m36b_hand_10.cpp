// FLAGS sound.dll: /O2 /GX
// sound.dll hand matches (round 36, batch m36b): the device base classes (vtables 0x1005b8b0 base, 0x1005b914
// VoiceBase, 0x1005b9e4 MidiDeviceBase, 0x1005ba5c WaveInDeviceBase), the sound thread (state object 0x100b5038:
// flags byte, CRITICAL_SECTION +4, thread id +0x1c, event +0x24, running flag +0x28) and two small helpers.
#include <windows.h>
#include <string.h>

struct Random { unsigned int seed; float nextFloat(); };

// MATCH: sound.dll 0x100285a0 ?nextFloat@Random@@QAEMXZ
float Random::nextFloat()
{
    seed = seed * 0x19660d + 0x3c6ef35f;
    unsigned int bits = (seed & 0x7fffff) | 0x3f800000;
    return *(float *)&bits - 1.0f;
}

LRESULT __stdcall soundWindowSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT soundWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

struct SoundWindowObj {
    int m0, m4;
    HWND hwnd;                  // +8
    int pumpWaveMessages();
};

// MATCH: sound.dll 0x10028810 ?pumpWaveMessages@SoundWindowObj@@QAEHXZ
int SoundWindowObj::pumpWaveMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, hwnd, 0x7e8, 0x7ef, PM_REMOVE))
        soundWindowSubclassProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    return 0;
}

struct Releasable { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3();
                    virtual int open(int a, int b); virtual void release(); };

// vtable 0x1005b8b0
struct DeviceBase {
    virtual void v0();
    virtual int check(int b);           // +4
    virtual void close();               // +8
    int m4;
    int m8;                             // 0x7f
    int mc, m10;
    Releasable *impl;                   // +14
    int m18, m1c;
    DeviceBase()
    {
        memset(&m4, 0, 4);
        m8 = 0x7f;
        mc = 0;
        m18 = 0;
        m1c = 0;
        m10 = 0;
        impl = 0;
    }
    ~DeviceBase()
    {
        if (impl)
            impl->release();
    }
};

struct VoiceSlot { char b[0x18]; VoiceSlot(); ~VoiceSlot(); };

struct VoiceBase : DeviceBase {
    int m20;
    VoiceSlot slots[16];                // +24
    VoiceBase();
    ~VoiceBase();
    int openInner(int a, int b);
};

// MATCH: sound.dll 0x10029020 ??0VoiceBase@@QAE@XZ
VoiceBase::VoiceBase()
{
    memset(&m20, 0, 4);
}

// MATCH: sound.dll 0x10029200 ??1VoiceBase@@QAE@XZ
VoiceBase::~VoiceBase()
{
}

// MATCH: sound.dll 0x100292d0 ?openInner@VoiceBase@@QAEHHH@Z
int VoiceBase::openInner(int a, int b)
{
    int r = check(b);
    if (r == 0) {
        r = impl->open(a, b);
        if (r != 0) {
            close();
            return r;
        }
        return 0;
    }
    return r;
}

struct MidiDeviceBase : DeviceBase {
    MidiDeviceBase();
    int openInner(int a, int b);
};

// MATCH: sound.dll 0x10029e70 ??0MidiDeviceBase@@QAE@XZ
MidiDeviceBase::MidiDeviceBase()
{
}

// MATCH: sound.dll 0x10029f90 ?openInner@MidiDeviceBase@@QAEHHH@Z
int MidiDeviceBase::openInner(int a, int b)
{
    int r = check(b);
    if (r == 0) {
        r = impl->open(a, b);
        if (r != 0) {
            close();
            impl = 0;
            return r;
        }
        return 0;
    }
    return r;
}

struct WaveInDeviceBase : DeviceBase {
    WaveInDeviceBase();
    int openInner(int a, int b);
};

// MATCH: sound.dll 0x1002a240 ??0WaveInDeviceBase@@QAE@XZ
WaveInDeviceBase::WaveInDeviceBase()
{
}

// MATCH: sound.dll 0x1002a420 ?openInner@WaveInDeviceBase@@QAEHHH@Z
int WaveInDeviceBase::openInner(int a, int b)
{
    int r = check(b);
    if (r == 0) {
        r = impl->open(a, b);
        if (r != 0) {
            close();
            return r;
        }
        return 0;
    }
    return r;
}

struct SoundThreadFlags { unsigned char running : 1; unsigned char rest : 7; };

struct SoundThread {
    SoundThreadFlags flags;             // +0
    CRITICAL_SECTION cs;                // +4
    DWORD threadId;                     // +1c
    int m20;                            // +20
    HANDLE event;                       // +24
    void stopFull();
    int post(UINT msg, WPARAM wp, LPARAM lp);
};

// MATCH: sound.dll 0x1002a8a0 ?stopFull@SoundThread@@QAEXXZ
void SoundThread::stopFull()
{
    if (flags.running) {
        flags.running = 0;
        EnterCriticalSection(&cs);
        flags.running = 0;
        SetEvent(event);
        LeaveCriticalSection(&cs);
        Sleep(0);
        EnterCriticalSection(&cs);
        LeaveCriticalSection(&cs);
        m20 = 0;
        threadId = 0;
    }
}

// MATCH: sound.dll 0x1002aa90 ?post@SoundThread@@QAEHIIJ@Z
int SoundThread::post(UINT msg, WPARAM wp, LPARAM lp)
{
    if (!flags.running)
        return 0x15;
    while (!PostThreadMessageA(threadId, msg, wp, lp)) {
        OutputDebugStringA("PostThreadMessage failed\n");
        Sleep(100);
    }
    return 0;
}

extern SoundThread g_soundThreadState;      // 0x100b5038
extern CRITICAL_SECTION g_soundThreadCrit;  // 0x100b503c
extern DWORD g_soundThreadId;               // 0x100b5054
extern HANDLE g_soundThreadEvent;           // 0x100b505c
extern char g_soundThreadRunning;           // 0x100b5060

// MATCH: sound.dll 0x1002a970 ?soundThreadPump@@YAXXZ
void soundThreadPump()
{
    MSG msg;
    g_soundThreadRunning = 1;
    g_soundThreadId = GetCurrentThreadId();
    while (MsgWaitForMultipleObjects(1, &g_soundThreadEvent, FALSE, INFINITE, 0x48) != 0) {
        EnterCriticalSection(&g_soundThreadCrit);
        if (!g_soundThreadState.flags.running) {
            LeaveCriticalSection(&g_soundThreadCrit);
            g_soundThreadRunning = 0;
            return;
        }
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
            soundWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
        LeaveCriticalSection(&g_soundThreadCrit);
    }
    g_soundThreadRunning = 0;
}
