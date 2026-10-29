#ifndef WAVEMGR_H
#define WAVEMGR_H

struct wmgr_flags {
    int amb_sound : 1;
    unsigned char GT_req_pending : 1;
};

class CWaveMgr : public Wave, public CWaveFile {
    friend class Dll_Wave_Device;
    HWND m_hwnd;

    wmgr_flags m_flags;
    static const unsigned long pitch_table[25];
    static const unsigned long ds_pitch_table[25];
    static const double fp_pitch_table[25];

    virtual SNDERR reload();

    unsigned long m_msgs_pending;

    char *m_plinked_wave;
    unsigned long *m_plinked_semaphore;

public:
    int m_nvoices;
    int m_midi_note;
    CWaveMgr *m_voices[3];
    CWaveMgr();
    CWaveMgr(HWND hwnd);
    virtual ~CWaveMgr();
    virtual SNDERR load(const char *fname);
    virtual SNDERR unload();
    virtual SNDERR play();
    virtual SNDERR play(int ibuffer);
    virtual SNDERR play(int *pinstance);
    virtual SNDERR fade();
    virtual SNDERR fade_in();

    virtual void ramp(int dest_vol, int src_vol, unsigned int ms_time);
    virtual SNDERR stop();

    virtual SNDERR release();
    SNDERR direct_unload();
    SNDERR post_delete();
    virtual void set_volume(int vol);
    virtual void set_pan(int pan);
    virtual void set_loop_state(int state) { CWaveFile::set_loop_state(state); }
    virtual void set_delay(unsigned long delay);
    void set_GT_request_pending(bool state) { m_flags.GT_req_pending = state; }
    bool is_GT_request_pending() { return m_flags.GT_req_pending; }
    virtual SNDERR set_fade(unsigned long ms_fade)
    {
        CWaveFile::set_fade(ms_fade);
        return SNDERR_OK;
    }

    virtual SNDERR set_fade_in(unsigned int ms_fade)
    {
        CWaveFile::set_fade_in(ms_fade);
        return SNDERR_OK;
    }
    unsigned long get_delay();
    virtual unsigned long get_time() { return CWaveFile::get_time(); }
    virtual unsigned long get_time(unsigned long instance);
    virtual unsigned long get_current_marker() { return CWaveFile::get_current_marker(); }
    virtual int is_looping() { return CWaveFile::get_loop_state(); }
    virtual SNDERR set_pitch(int pitch);
    virtual SNDERR set_attrib(unsigned long attrib);
    virtual unsigned long get_attrib();
    virtual HWND get_game_hwnd() { return CDSMgr::get_game_hwnd(); }
    unsigned long get_npending_msgs();
    void dec_msgs_pending();
    SNDERR reset();


    virtual int is_playing() { return CWaveFile::is_playing(); }
    virtual int is_hwbuffer() { return CWaveFile::is_hwbuffer(); }
    static CWaveMgr *get_list_head() { return (CWaveMgr *)gbl_sound_head; }
    static CWaveMgr *get_list_tail();
    CWaveMgr *get_next_playing();
    CWaveMgr *get_prev_playing();
    void set_hwnd(HWND__ *);
    HWND get_hwnd() { return m_hwnd; }
    virtual SNDERR detach();

    static void free_all();
    virtual int is_loaded() { return CWaveFile::is_loaded(); }
    int is_amb_sound() { return m_flags.amb_sound; }
    void set_amb_sound() { m_flags.amb_sound = 1; }
    void set_wndproc_delete();
    virtual unsigned int get_ms_length();
    virtual SNDERR set_position3d(float x, float y, float z);

    virtual bool is_3d() { return CWaveFile::is_3d(); }
    virtual SNDERR set_xpos(float x);
    virtual SNDERR set_ypos(float y);
    virtual SNDERR set_zpos(float z);
    virtual SNDERR set_reverb_mix(float mix);
    virtual SNDERR set_link(char *fname, unsigned long *psemaphore);

    char *get_linked_wave();
    void TT_clear_linked_wave();
    unsigned long *TT_get_plinked_semaphore();
};


#ifdef USE_MPEG
typedef char assert_sizeof_CWaveMgr[(sizeof(CWaveMgr) == 0x1498) ? 1 : -1];
#else
typedef char assert_sizeof_CWaveMgr[(sizeof(CWaveMgr) == 0x1450) ? 1 : -1];
#endif

#endif
