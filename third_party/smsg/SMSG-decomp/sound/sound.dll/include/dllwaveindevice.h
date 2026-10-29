#ifndef DLLWAVEINDEVICE_H
#define DLLWAVEINDEVICE_H

struct DllWaveInDeviceFlags {
    int record : 1;
    int initialized;
    int use_voxware;
};

class Dll_Wave_In_Device : public Wave_In_Device {
    DllWaveInDeviceFlags m_flags;
    static WaveInDeviceMgr *m_pwidmgr;
    CTimer *m_ptimer;
    unsigned long m_rate;
    unsigned long m_time_past;
    unsigned long m_time_since_last_update;
    unsigned long m_master_volume;
    unsigned long m_record_id;
    HWND m_game_hwnd;

protected:
    virtual void add_to_time_past(unsigned long ms_time);
    virtual unsigned long get_time_past() { return m_time_past; }

    virtual void reset_time_past() { m_time_past = 0; }

    static SNDERR __cdecl create_device(Wave_In_Device **ppdev, unsigned long flags);
    static SNDERR __cdecl delete_device();

public:
    Dll_Wave_In_Device();
    virtual ~Dll_Wave_In_Device();
    SNDERR start_raw_dump(char *);
    SNDERR stop_raw_dump();
    int is_recording();
    virtual SNDERR start_record();
    virtual SNDERR end_record();
    SNDERR stop_record();
    SNDERR hook_vtx(CVoiceTxMgr *pvtx);
    SNDERR unhook_vtx();

    void set_timer(CTimer *);
    virtual void timer_unhook() { m_ptimer = 0; }
    virtual void set_rate(unsigned long rate) { m_rate = rate; }
    virtual unsigned long get_rate() { return m_rate; }

    CTimer *get_timer();
    virtual SNDERR init(HWND game_hwnd, unsigned long flags);
    HWND__ *get_hwnd();
    virtual SNDERR release();
    virtual void update_sound();
    virtual unsigned long get_ndevices();
    virtual SNDERR get_description(unsigned long id, char *buffer, unsigned long buff_size);
    virtual SNDERR select(unsigned long id);
    virtual void suspend();
    virtual void restart();

    virtual void set_volume(unsigned long) {}
    virtual unsigned long get_volume() { return 0; }
    virtual void set_pan(int) {}
    virtual SNDERR fade(unsigned long) { return SNDERR_OK; }
    void set_game_hwnd(HWND game_hwnd) { m_game_hwnd = game_hwnd; }
    HWND__ *get_game_hwnd();
    virtual SNDERR set_vxw_key(signed char *key);
};

#endif
