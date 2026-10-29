#ifndef MIDIFILE_H
#define MIDIFILE_H

#include "miditools.h"

struct MThd_tag {
    unsigned long length;
    unsigned long format;
    unsigned long ntrks;
    unsigned long division;
};

struct MFFlags_tag {
    int loaded : 1;
    int looping : 1;
    int fading : 1;
    int done : 1;
    int eod : 1;
    int active : 1;
    int release : 1;
    int reset_pending : 1;
    int unload_pending : 1;
    int timer_unload : 1;
    int prepared : 1;
};

class CMidiFile {
    friend class CMidiMgr;

    void mmioerr(unsigned long err);

    HMMIO m_hfile;
    MMIOINFO m_mmioinfo;

    MFFlags_tag m_flags;

    char m_fname[0x100];

    MThd_tag m_header;

    unsigned long m_curstartpos;
    unsigned long m_curendpos;
    unsigned long m_curpos;
    unsigned long m_curfpos;
    HWND m_hwnd;

    unsigned long m_fade_amount;
    unsigned long m_volume;
    unsigned long m_reset_volume;
    int m_pan;
    unsigned long m_time;
    unsigned long m_delay;
    unsigned long m_time_past;
    int m_loop_count;
    int m_cur_loop_count;
    unsigned long m_loop_startpos;

    DList<CMidiTrack> m_track_list;
    unsigned long m_control_track;

public:
    SNDERR set_track(unsigned char *, unsigned long);
    CMidiFile();
    ~CMidiFile();
    SNDERR load(const char *fname);
    SNDERR load(HMMIO hfile);
    void undo_load();
    SNDERR reload();

    SNDERR unload();
    void set_timer_unload();
    void clear_timer_unload();
    int need_timer_unload() { return m_flags.timer_unload; }
    void set_fade(unsigned long ms);
    void set_fade_flag(int state) { m_flags.fading = state; }
    SNDERR fade();
    SNDERR fade(unsigned long ms);
    int is_fading() { return m_flags.fading; }
    void set_unload_pending() { m_flags.unload_pending = 1; }
    int is_unload_pending() { return m_flags.unload_pending; }
    void set_hwnd(HWND hwnd) { m_hwnd = hwnd; }

    HWND get_hwnd() { return m_hwnd; }
    void setstartpos(unsigned long);
    void setendpos(unsigned long);
    unsigned long get_current_position();
    void set_current_position(unsigned long);
    int is_loaded() { return m_flags.loaded; }
    void set_active(int val) { m_flags.active = val; }
    void set_done(int flag) { m_flags.done = flag; }
    void set_prepared(int flag) { m_flags.prepared = flag; }
    int is_active() { return m_flags.active; }
    int is_done() { return m_flags.done; }
    int is_playing() { return m_flags.active && !m_flags.done; }
    SNDERR reset();
    void reset_flags();
    SNDERR post_reset();
    void set_volume(int);
    void set_loop_state(int state);
    void set_pan(int pan);
    int get_loop_state() { return m_flags.looping; }

    SNDERR play();
    SNDERR play(unsigned long);
    unsigned long be2le(unsigned long be);
    static unsigned short be2le(unsigned short be);
    static unsigned long varlen2dw(unsigned char *string, unsigned long *bytes_used);
    SNDERR get_pfirst_event(unsigned char **ppevent, unsigned long track);
    SNDERR get_pcur_event(unsigned char **ppevent, unsigned long track);
    SNDERR get_track(CMidiTrack **pptrack, unsigned long track);
    SNDERR advance_track(unsigned long distance, unsigned long track);

    unsigned long get_division() { return m_header.division; }

    unsigned long get_ntracks() { return m_header.ntrks; }
    SNDERR set_control_track(unsigned long);
    void set_all_tracks_inactive();
};


class CMidiEvent {
    union {
        struct {
            unsigned char m_status;
            unsigned char m_data[3];
        };
        unsigned long m_packed;
    };

public:
    unsigned char get_status() { return m_status; }
    void set_status(unsigned char);
    void set_packed(unsigned long packed) { m_packed = packed; }
    unsigned long get_packed() { return m_packed; }
    void set_d1(unsigned char);
    void set_d2(unsigned char d2) { m_data[1] = d2; }
};

union MTrk_flags {
    unsigned int all;
    struct {
        int initialized : 1;
        int at_end : 1;
        int own_data : 1;
        unsigned char mute : 1;
        unsigned char switcher : 1;
        unsigned char active_switcher : 1;
        unsigned char active : 1;
        unsigned char loop : 1;
        unsigned char sync_start : 1;
        unsigned char sync_running : 1;
        unsigned char sync_beat : 1;
        unsigned char sync_measure : 1;
        unsigned char no_sync : 1;
    };
};

class CMidiTrack : public MidiCtrlTarget {
    friend class CMidiFile;
    friend class CMidiMgr;

    unsigned char *m_pcur_event;
    unsigned char m_cur_status;
    MTrk_flags m_flags;
    unsigned char *m_pdata;
    unsigned long m_cur_offset;
    unsigned long m_length;
    unsigned long m_cur_channel;
    int m_ticks;
    unsigned long m_tick_offset;
    unsigned long m_total_ticks;

public:
    CWaveMgr *m_on_off_table[128];
    CMidiEvent m_hw_on_off_table[128];
    CMidiTrack();
    ~CMidiTrack();

    DllTrackSet *m_ptrackset;
    int m_pitch_shift;
    int m_velocity_offset;
    void set_pdata(unsigned char *pdata);



    unsigned char *get_pdata() { return m_pdata; }
    void set_pcur_event(unsigned char *);
    unsigned char *get_pcur_event() { return m_pcur_event; }
    void set_cur_channel(unsigned long cur_channel) { m_cur_channel = cur_channel; }
    unsigned long get_cur_channel() { return m_cur_channel; }
    void set_cur_status(unsigned char cur_status) { m_cur_status = cur_status; }
    unsigned char get_cur_status() { return m_cur_status; }
    void reset();

    void set_length(unsigned long length) { m_length = length; }
    SNDERR advance(unsigned long distance);
    SNDERR seek(unsigned long seek_ticks, unsigned long *overshot_ticks);
    SNDERR seek_to_prior(unsigned long seek_ticks, unsigned long *prior_ticks);

    int at_end() { return m_flags.at_end; }
    void set_done() { m_flags.at_end = 1; }
    void add_ticks(unsigned long ticks) { m_ticks += ticks; }
    void set_own_data(int flag) { m_flags.own_data = flag; }
    int is_own_data() { return m_flags.own_data; }
    int get_ticks() { return m_ticks + m_tick_offset; }
    void mute() { m_flags.mute = 1; }
    void unmute() { m_flags.mute = 0; }
    bool is_mute() { return m_flags.mute; }
    bool is_switcher() { return m_flags.switcher; }
    void set_switcher() { m_flags.switcher = 1; }
    void set_tick_offset(unsigned long offset);
    bool is_active_switcher() { return m_flags.active_switcher; }
    void set_active_switcher(bool state) { m_flags.active_switcher = state; }
    unsigned long get_total_ticks() { return m_total_ticks; }
    unsigned long calc_total_ticks();

    void set_loop_state(bool state) { m_flags.loop = state; }
    unsigned long control_change(unsigned char *, unsigned long);
    bool is_looping() { return m_flags.loop; }

    void hw_notes_off();

    virtual int start_control(int input);
};


#endif
