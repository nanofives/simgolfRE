#include <windows.h>
#include <mmsystem.h>
#include <process.h>
#include "sound.h"
#include "sounddevice.h"


SoundThread gbl_sound_thread;
bool gbl_in_sound_thread;


SoundThread::SoundThread()
{
    memset(&m_flags, 0, sizeof(m_flags));
    InitializeCriticalSection(&m_cs);
    m_id = 0;
    m_handle = 0;
    m_event_handle = CreateEventA(0, 1, 0, 0);
}

SoundThread::~SoundThread()
{
    stop();
    m_handle = 0;
    m_id = 0;
    DeleteCriticalSection(&m_cs);
    CloseHandle(m_event_handle);
}

void SoundThread::stop()
{
    if (m_flags.state) {
        m_flags.state = 0;
        EnterCriticalSection(&m_cs);
        m_flags.state = 0;
        SetEvent(m_event_handle);
        LeaveCriticalSection(&m_cs);
        Sleep(0);
        EnterCriticalSection(&m_cs);
        LeaveCriticalSection(&m_cs);
        m_handle = 0;
        m_id = 0;
    }
}

void SoundThread::start()
{
    ResetEvent(m_event_handle);
    if (!m_flags.state) {
        m_flags.state = 1;
        m_handle = (void *)_beginthread(fn_sound_thread, 0, 0);
        SetThreadPriority(m_handle, 0);
    }
}

void SoundThread::fn_sound_thread(void *ch)
{
    MSG msg;

    gbl_in_sound_thread = true;
    gbl_sound_thread.m_id = GetCurrentThreadId();

    while (
        MsgWaitForMultipleObjects(1, &gbl_sound_thread.m_event_handle, FALSE, INFINITE, QS_POSTMESSAGE | QS_SENDMESSAGE)
        != 0) {
        EnterCriticalSection(&gbl_sound_thread.m_cs);
        if (!gbl_sound_thread.m_flags.state) {
            LeaveCriticalSection(&gbl_sound_thread.m_cs);
            break;
        }
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
            thread_proc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
        LeaveCriticalSection(&gbl_sound_thread.m_cs);
    }
    gbl_in_sound_thread = false;
}

SNDERR SoundThread::post_message(unsigned int msg, long wparam, long lparam)
{
    if (!m_flags.state)
        return SNDERR_NOT_ACTIVE;
    while (!PostThreadMessageA(m_id, msg, wparam, lparam)) {
        OutputDebugStringA("PostMesage Fail!\n");
        Sleep(100);
    }
    return SNDERR_OK;
}
