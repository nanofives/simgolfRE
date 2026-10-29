#include "sound.h"
#include "sounddevice.h"
#include <stdio.h>
#include "dlist.h"
#include "ctimer.h"

CRITICAL_SECTION gbl_timer_cs;
static int timer_guard = 0;

enum {
    kSampleRate = 44100,
    kChannels = 2,
    kBitsPerSample = 16,
    kPeriodMs = 30
};

void CTimer::register_device(Sound_Device *device)
{
    EnterCriticalSection(&gbl_timer_cs);
    m_slist.add_to_head(device);
    LeaveCriticalSection(&gbl_timer_cs);
}

SNDERR CTimer::unregister_device(Sound_Device *device)
{
    Sound_Device *ret_device;

    EnterCriticalSection(&gbl_timer_cs);
    ret_device = m_slist.remove(device);
    LeaveCriticalSection(&gbl_timer_cs);
    device->timer_unhook();
    return ret_device ? SNDERR_OK : SNDERR_NOT_LOADED;
}

CTimer::CTimer()
{
    memset(&m_flags, 0, sizeof(m_flags));
    timeGetDevCaps(&m_caps, sizeof(m_caps));
    m_resolution = m_caps.wPeriodMin;
    m_ms_time = 0;
    m_ms_time_past = 0;
    m_samples = 0;
    m_avg_samples_per_tick = 0;
    m_type = 1;
    m_pcallback = 0;
    m_id = 0;
    m_dwUser = 0;
    m_wfx.wFormatTag = 1;
    m_wfx.cbSize = 0;
    m_rate = kPeriodMs;
    m_wfx.nChannels = kChannels;
    m_wfx.nSamplesPerSec = kSampleRate;
    m_wfx.nAvgBytesPerSec = kSampleRate * kChannels * kBitsPerSample / 8;
    m_wfx.nBlockAlign = kChannels * kBitsPerSample / 8;
    m_wfx.wBitsPerSample = kBitsPerSample;
    m_samples_per_tick = kSampleRate * kPeriodMs / 1000;
    memset(&gbl_timer_cs, 0, sizeof(gbl_timer_cs));
    InitializeCriticalSection(&gbl_timer_cs);
}

CTimer::~CTimer()
{
    timer_guard++;

    if (m_flags.loaded && m_flags.internal) {
        timer_guard++;
        m_flags.active = 0;
        EnterCriticalSection(&gbl_timer_cs);
        LeaveCriticalSection(&gbl_timer_cs);
        m_ms_time_past = 0;
        m_ms_time = 0;
        timeKillEvent(m_id);
        timeEndPeriod(m_resolution);
        m_flags.loaded = 0;
    }

    DeleteCriticalSection(&gbl_timer_cs);
    memset(&gbl_timer_cs, 0, sizeof(gbl_timer_cs));
}

void CTimer::release()
{
    timer_guard++;
    EnterCriticalSection(&gbl_timer_cs);
    LeaveCriticalSection(&gbl_timer_cs);

    if (m_flags.loaded) {
        timer_guard++;
        m_flags.active = 0;
        EnterCriticalSection(&gbl_timer_cs);
        LeaveCriticalSection(&gbl_timer_cs);
        m_ms_time_past = 0;
        m_ms_time = 0;
        timeKillEvent(m_id);
        timeEndPeriod(m_resolution);
        m_flags.loaded = 0;
    }

    Sound_Device *psd;
    while ((psd = m_slist.remove_head()) != 0)
        psd->timer_unhook();
}

SNDERR CTimer::start()
{
    if (m_flags.active)
        return SNDERR_ALREADY_ACTIVE;

    timer_guard = 0;
    if (timeBeginPeriod(m_resolution) == 0x61)
        return SNDERR_NO_SUPPORT;

    m_id = timeSetEvent(m_rate, m_resolution, timer_proc, (DWORD)this, m_type);
    if (m_id == 0x61)
        return SNDERR_NO_SUPPORT;

    m_flags.loaded = 1;
    m_flags.internal = 1;
    m_flags.active = 1;
    return SNDERR_OK;
}

SNDERR CTimer::stop()
{
    unsigned int retval;

    if (!m_flags.loaded)
        return SNDERR_NOT_ACTIVE;

    timer_guard++;
    m_flags.active = 0;
    EnterCriticalSection(&gbl_timer_cs);
    LeaveCriticalSection(&gbl_timer_cs);

    m_ms_time_past = 0;
    m_ms_time = 0;
    retval = timeKillEvent(m_id);
    retval = timeEndPeriod(m_resolution);
    m_flags.loaded = 0;
    return SNDERR_OK;
}

int CTimer::get_samples_per_tick(WAVEFORMATEX *pwfx)
{
    if (pwfx == 0)
        return 0;
    return pwfx->nSamplesPerSec * m_rate / 1000;
}

SNDERR CTimer::set_rate(unsigned long rate)
{
    if (m_flags.active)
        return SNDERR_ALREADY_ACTIVE;
    if (rate <= 0)
        return SNDERR_BAD_PARAM;
    if (rate < m_caps.wPeriodMin)
        return SNDERR_NO_SUPPORT;
    m_rate = rate;
    return SNDERR_OK;
}

SNDERR CTimer::set_wave_format(WAVEFORMATEX *pwfx)
{
    if (pwfx == 0)
        return SNDERR_BAD_PARAM;
    m_wfx = *pwfx;
    return SNDERR_OK;
}

void CALLBACK CTimer::timer_proc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
    char buf[256];
    CTimer *ptimer = (CTimer *)dwUser;

    if (timer_guard != 0)
        return;
    if (ptimer != 0 && !ptimer->m_flags.active)
        return;

    timer_guard = 1;
    EnterCriticalSection(&gbl_timer_cs);
    if (ptimer != 0 && !ptimer->m_flags.active) {
        timer_guard++;
        LeaveCriticalSection(&gbl_timer_cs);
        return;
    }

    DWORD ms_time = timeGetTime();
    if (ptimer->m_ms_time != 0) {
        if (ms_time > ptimer->m_ms_time)
            ptimer->m_ms_time_past = ms_time - ptimer->m_ms_time;
        else
            ptimer->m_ms_time_past = ptimer->m_ms_time - ms_time;
        if (ptimer->m_ms_time_past < ptimer->m_rate - 7) {
            timer_guard--;
            LeaveCriticalSection(&gbl_timer_cs);
            return;
        }
    } else {
        ptimer->m_ms_time_past = 0;
    }

    if (ptimer->m_ms_time_past > ptimer->m_rate + 20) {
        sprintf(buf, "timer Late!!! by %d ms\n", ptimer->m_ms_time_past);
        OutputDebugStringA(buf);
    }
    ptimer->m_ms_time = ms_time;

    Sound_Device *pcur_device = ptimer->m_slist.get_first();
    DWORD debug_time = 0;
    while (pcur_device != 0) {
        pcur_device->add_to_time_past(ptimer->m_ms_time_past);
        pcur_device->update_sound();
        pcur_device = ptimer->m_slist.get_next();
    }

    timer_guard--;
    LeaveCriticalSection(&gbl_timer_cs);
}
