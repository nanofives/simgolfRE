#ifndef CTIMER_H
#define CTIMER_H

struct CTimer_Flags_tag {
    int loaded : 1;
    int internal : 1;
    int active : 1;
};

class CTimer {
    unsigned long m_rate;
    unsigned long m_ms_time;
    unsigned long m_ms_time_past;
    int m_samples;
    int m_samples_per_tick;
    int m_avg_samples_per_tick;
    int m_resolution;
    int m_type;
    unsigned long m_dwUser;
    void (*m_pcallback)(void);
    WAVEFORMATEX m_wfx;
    CTimer_Flags_tag m_flags;
    unsigned m_id;
    TIMECAPS m_caps;
    DList<Sound_Device> m_slist;

public:
    int get_samples_per_tick(WAVEFORMATEX *pwfx);
    int get_samples_per_tick();
    SNDERR set_wave_format(WAVEFORMATEX *pwfx);
    CTimer();
    ~CTimer();
    SNDERR set_rate(unsigned long rate);
    int get_rate() { return m_rate; }
    void set_callback(void(__cdecl *p_callback)()) { m_pcallback = p_callback; }
    SNDERR start();
    SNDERR stop();
    void release();

    void register_device(Sound_Device *device);
    SNDERR unregister_device(Sound_Device *device);
    int is_active() { return m_flags.active; }
    static void CALLBACK timer_proc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);
};

extern CTimer *gbl_ptimer;
extern unsigned long gbl_flags;
extern "C" void update_sound(void);

#endif
