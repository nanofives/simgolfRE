#ifndef TIMER_H
#define TIMER_H

#include "Surface.h"

void flush_timer();

class Time {
public:
    Time();
    virtual ~Time();
    void stop();
    void close();
    void init(void(__cdecl *fn)(int), int arg, int periodMs, int resolutionMs);
    void SetCallback(void(__cdecl *fn)(int, void *), int arg, void *ctx, int periodMs, int resolutionMs);
    int pulse();
    int start();
    void SetTargetAndStart(void(__cdecl *fn)(int), int arg, int periodMs, int resolutionMs);
    void SetCallbackAndStart(void *fn, int arg, void *ctx, int periodMs, int resolutionMs);
    void SetCallbackAndPulse(void *fn, int arg, void *ctx, int periodMs, int resolutionMs);

    void (*m_onDone)();
    unsigned long m_flags;
    unsigned long m_timerId;
    void(__cdecl *m_onTick)(int);
    void(__cdecl *m_callback)(int, void *);
    void *m_context;
    int m_arg;
    unsigned long m_periodMs;
    int m_posted;
    int m_resolutionMs;
    int m_reserved;
    static void close_class();
};

extern Time *g_activeTimer;

extern int g_timerInitCount;
int InitTimers();

#endif
