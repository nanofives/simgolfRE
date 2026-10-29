#ifndef DLLMIDIDEVICE_H
#define DLLMIDIDEVICE_H


struct DllMidiDeviceFlags {
    int disabled : 1;
};

class Dll_Midi_Device : public Midi_Device {
    DllMidiDeviceFlags m_flags;
    HWND m_hwnd;
    CTimer *m_ptimer;
    unsigned long m_rate;
    unsigned long m_time_past;
    unsigned long m_master_volume;

    HMIDIOUT m_hmidi_out;

    MidiMgrList m_play_list;
    CCriticalRequestQueue m_req_queue;
    void do_requests();

    SNDERR clean_request_queue();

    unsigned long weed_out_conflicting_requests(unsigned int *start_index, unsigned int *end_index);

    SNDERR init_win_device();

protected:
    virtual void add_to_time_past(unsigned long ms_time) { m_time_past += ms_time; }
    virtual unsigned long get_time_past() { return m_time_past; }

    virtual void reset_time_past() { m_time_past = 0; }

    static SNDERR __cdecl create_device(Midi_Device **ppdev, unsigned long flags);
    static SNDERR __cdecl delete_device();

public:
    Dll_Midi_Device();
    virtual ~Dll_Midi_Device();
    SNDERR queue_request(CMidiMgr *pmmgr, REQUEST_TYPE type, int data1, int data2);
    SNDERR post_GT_request(CCriticalRequest *pcur_req);

    void set_timer(CTimer *);
    virtual void timer_unhook() { m_ptimer = 0; }
    virtual void set_rate(unsigned long rate) { m_rate = rate; }
    virtual unsigned long get_rate() { return m_rate; }

    CTimer *get_timer();
    virtual SNDERR init(HWND game_hwnd, unsigned long flags);
    virtual SNDERR release();
    virtual void update_sound();
    virtual unsigned long get_ndevices();
    virtual SNDERR get_description(unsigned long id, char *desc_buff, unsigned long buffsize);
    virtual SNDERR select(unsigned long id);
    virtual void set_volume(unsigned long volume);
    virtual void suspend();
    virtual void restart();

    virtual SNDERR enable();
    virtual SNDERR disable();
    virtual int is_disabled() { return m_flags.disabled; }
    HWND__ *get_hwnd();
    HMIDIOUT hmidi_out() { return m_hmidi_out; }
};

#endif
