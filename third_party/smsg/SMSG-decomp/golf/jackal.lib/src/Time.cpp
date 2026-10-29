#include "jgldefs.h"
#include <windows.h>
#include <mmsystem.h>

#include "Timer.h"

int g_timerInitCount = 0;
Time *g_activeTimer = 0;

Time::Time()
{
    m_timerId = 0;
    m_onTick = 0;
    m_callback = 0;
    m_context = 0;
    m_arg = 0;
    m_periodMs = 0;
    m_posted = 0;
    m_resolutionMs = 5;
    m_reserved = 0;
    m_flags = 0;
    m_onDone = 0;
}

Time::~Time()
{
    close();
}

void Time::init(void(__cdecl *fn)(int), int arg, int periodMs, int resolutionMs)
{
    close();
    m_onTick = fn;
    m_callback = 0;
    m_context = 0;
    m_arg = arg;
    m_periodMs = periodMs;
    m_resolutionMs = resolutionMs;
}

void Time::SetCallback(void(__cdecl *fn)(int, void *), int arg, void *ctx, int periodMs, int resolutionMs)
{
    close();
    m_onTick = 0;
    m_callback = fn;
    m_context = ctx;
    m_arg = arg;
    m_periodMs = periodMs;
    m_resolutionMs = resolutionMs;
}

void Time::SetTargetAndStart(void(__cdecl *fn)(int), int arg, int periodMs, int resolutionMs)
{
    init(fn, arg, periodMs, resolutionMs);
    start();
}

void Time::SetCallbackAndStart(void *fn, int arg, void *ctx, int periodMs, int resolutionMs)
{
    SetCallback((void(__cdecl *)(int, void *))fn, arg, ctx, periodMs, resolutionMs);
    start();
}

void Time::SetCallbackAndPulse(void *fn, int arg, void *ctx, int periodMs, int resolutionMs)
{
    SetCallback((void(__cdecl *)(int, void *))fn, arg, ctx, periodMs, resolutionMs);
    pulse();
}

void __stdcall TimerProc(void *hwnd, unsigned msg, unsigned idEvent, unsigned time);
void __stdcall TimeProc(unsigned uID, unsigned uMsg, unsigned long dwUser, unsigned long dw1, unsigned long dw2);

int Time::start()
{
    if (m_onTick == 0 && m_callback == 0)
        return 7;

    m_flags &= ~1;

    if (m_periodMs < 0x32) {
        m_timerId = timeSetEvent(m_periodMs, m_resolutionMs, (LPTIMECALLBACK)TimeProc, (unsigned long)this, 1);
        return m_timerId ? 0 : 2;
    }

    m_timerId = SetTimer((HWND)graphsys->get_window_handle(), (unsigned)this, m_periodMs, (TIMERPROC)TimerProc);
    return m_timerId ? 0 : 2;
}

int Time::pulse()
{
    if (m_onTick == 0 && m_callback == 0)
        return 7;

    if (m_periodMs < 0x32) {
        m_timerId = timeSetEvent(m_periodMs, m_resolutionMs, (LPTIMECALLBACK)TimeProc, (unsigned long)this, 0);
        return m_timerId ? 0 : 2;
    }

    m_flags = (m_flags & ~2) | 1;
    m_timerId = SetTimer((HWND)graphsys->get_window_handle(), (unsigned)this, m_periodMs, (TIMERPROC)TimerProc);
    return m_timerId ? 0 : 2;
}

void Time::stop()
{
    int rearm = ~m_flags & 1;
    int id = m_timerId;

    if (id) {
        if (m_periodMs < 0x32)
            timeKillEvent(id);
        else if (graphsys)
            KillTimer((HWND)graphsys->get_window_handle(), id);
        m_timerId = 0;
    }

    if (rearm)
        flush_timer();
}

void Time::close()
{
    stop();
    m_onTick = 0;
    m_flags = 0;
    m_callback = 0;
    m_context = 0;
    m_arg = 0;
    m_periodMs = 0;
    m_posted = 0;
    m_resolutionMs = 5;
    m_reserved = 0;
}

void __stdcall TimerProc(void *hwnd, unsigned msg, unsigned idEvent, unsigned time)
{
    Time *c = (Time *)idEvent;

    if (c == 0)
        return;
    if (g_activeTimer != 0 && c != g_activeTimer)
        return;
    if (c->m_posted != 0)
        return;

    PostMessageA((HWND)graphsys->get_window_handle(), 0x401, (unsigned)c, 0);
    c->m_posted = 1;
}

void __stdcall TimeProc(unsigned uID, unsigned uMsg, unsigned long dwUser, unsigned long dw1, unsigned long dw2)
{
    Time *p = (Time *)dwUser;

    if (p == 0)
        return;

    if (p->m_onDone) {
        p->m_onDone();
        return;
    }

    if (g_activeTimer && p != g_activeTimer)
        return;

    if (p->m_posted)
        return;

    PostMessageA((HWND)graphsys->get_window_handle(), 0x401, (unsigned)p, 0);
    p->m_posted = 1;
}

int InitTimers()
{
    g_timerInitCount++;
    return 0;
}

void Time::close_class()
{
    --g_timerInitCount;
}

unsigned BeginTimerPeriod(unsigned resolutionMs)
{
    TIMECAPS caps;

    if (timeGetDevCaps(&caps, sizeof(caps)) != TIMERR_NOERROR)
        return 0;
    if (resolutionMs < caps.wPeriodMin)
        resolutionMs = caps.wPeriodMin;
    if (resolutionMs > caps.wPeriodMax)
        resolutionMs = caps.wPeriodMax;
    if (timeBeginPeriod(resolutionMs) != TIMERR_NOERROR)
        return 0;
    return resolutionMs;
}

void EndTimerPeriod(unsigned periodMs)
{
    if (periodMs != 0)
        timeEndPeriod(periodMs);
}

int g_timerSpare[2] = {0};
