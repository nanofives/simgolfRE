#ifndef DLLWAVEDEVICE_H
#define DLLWAVEDEVICE_H

struct DllWaveDeviceFlags {
    unsigned int record : 1;
    unsigned int disabled : 1;
    unsigned int mmx_support : 1;
    unsigned int eax : 1;
    unsigned int ds3d : 1;
    unsigned int debug : 1;
    unsigned int miles : 1;
};

class Dll_Wave_Device : public Wave_Device {
    friend class CWaveMixer;
    friend class CMidiMgr;

    DllWaveDeviceFlags m_flags;

    static CDSMgr *m_pdsmgr;
    static CDSB *m_pmix_dsb;
    static CDSB *m_p3d_dsb_l;
    static CDSB *m_p3d_dsb_r;
    static CWaveMixer *m_pmixer;
    CTimer *m_ptimer;
    unsigned long m_rate;
    unsigned long m_time_past;
    unsigned long m_time_since_last_update;
    unsigned long m_master_volume;
    unsigned long m_record_id;
    void *m_hmain_thread;
    unsigned int m_saved_flags;

    SndNodeList m_play_list;

    CCriticalRequestQueue m_req_queue;

    SNDERR clean_request_queue();
    SNDERR kill_request_queue();


    void do_requests();

protected:
    virtual void add_to_time_past(unsigned long ms_time);
    virtual void reset_time_past() { m_time_past = 0; }

    virtual unsigned long get_time_past() { return m_time_past; }

    static SNDERR __cdecl create_device(Wave_Device **ppdev, unsigned long flags);
    static SNDERR __cdecl delete_device();

public:
    Dll_Wave_Device();
    virtual ~Dll_Wave_Device();
    SNDERR pull_from_play_list(CWaveMgr *pwmgr);
    void add_to_play_list(CWaveMgr *pwmgr);

    virtual SNDERR start_raw_dump(char *fname);
    virtual SNDERR stop_raw_dump();
    virtual SNDERR enable();
    virtual SNDERR disable();
    virtual int is_disabled() { return m_flags.disabled; }
    bool is_debug() { return m_flags.debug; }
    void set_timer(CTimer *);
    virtual void timer_unhook() { m_ptimer = 0; }
    virtual void set_rate(unsigned long rate) { m_rate = rate; }
    virtual unsigned long get_rate() { return m_rate; }
    CTimer *get_timer();
    CWaveMixer *get_mixer() { return m_pmixer; }

    virtual SNDERR init(HWND game_hwnd, unsigned long flags);
    virtual SNDERR release();
    virtual void update_sound();
    void update_hwbuffers();
    virtual unsigned long get_ndevices();
    virtual SNDERR get_description(unsigned long id, char *buffer, unsigned long buff_size);
    virtual SNDERR select(unsigned long id);
    virtual void set_volume(unsigned long volume);
    virtual void suspend();
    virtual void restart();
    CDSMgr *get_pdsmgr() { return m_pdsmgr; }
    virtual unsigned long get_hw_mem_size();

    SNDERR queue_request(Sound *psound, REQUEST_TYPE type);
    SNDERR queue_request(CCriticalRequest *preq);

    SNDERR post_GT_request(CCriticalRequest *pcur_req);
    SNDERR post_GT_request(CWaveMgr *pwmgr, REQUEST_TYPE type);
    unsigned long weed_out_conflicting_requests(unsigned int *start_index, unsigned int *end_index);

    virtual SNDERR set_hwnd(HWND hwnd);
    virtual void *get_ds();
    virtual bool is_eax() { return m_flags.eax; }
    virtual SNDERR set_eax(unsigned long environment);
    virtual SNDERR set_eax(EAX_REVERB_PROPERTIES *peax);
    SNDERR get_eax(unsigned long *penvironment);
    SNDERR get_eax(EAX_REVERB_PROPERTIES *peax);
    SNDERR set_eax_volume(float volume);
    SNDERR get_eax_volume(float *pvolume);
    SNDERR set_eax_decay(float decay);
    SNDERR get_eax_decay(float *pdecay);
    SNDERR set_eax_damping(float damping);
    SNDERR get_eax_damping(float *pdamping);
    virtual SNDERR set_eax_mix(float mix);
    virtual bool is_3d();
    virtual SNDERR set_listener_position(float x, float y, float z);
    virtual SNDERR get_listener_position(float *x, float *y, float *z);
    virtual SNDERR set_listener_xpos(float x);
    virtual SNDERR get_listener_xpos(float *px);
    virtual SNDERR set_listener_ypos(float y);
    virtual SNDERR get_listener_ypos(float *py);
    virtual SNDERR set_listener_zpos(float z);
    virtual SNDERR get_listener_zpos(float *pz);
};
typedef char Dll_Wave_Device_sizeof_check[sizeof(Dll_Wave_Device) == 0x41e0 ? 1 : -1];

#endif
