#ifndef MIDIMGR_H
#define MIDIMGR_H

#include "keymap.h"
#include "midifile.h"

class CProgramMgr : public ProgramCtrl {
    unsigned long m_rnd_pitch_index, m_rnd_volume_index;

    Dll_KeyMap *m_pkeymap;

public:
    CProgramMgr();
    ~CProgramMgr();

    SNDERR load(ProgramCtrl *pctrl);
    SNDERR unload();
    SNDERR stop();
    void set_ctrl(ProgramCtrl *pctrl);
    int xlat_pitch(int pitch);
    unsigned long xlat_volume(unsigned long volume);
    void set_keymap(Dll_KeyMap *pkm) { m_pkeymap = pkm; }
    Dll_KeyMap *get_keymap() { return m_pkeymap; }
};
typedef char CProgramMgr_sizeof_check[sizeof(CProgramMgr) == 0x34 ? 1 : -1];

class CMidiChannel {
    unsigned long m_cur_status;

    unsigned long m_cur_volume;

    unsigned long m_cur_pitch;

    unsigned long m_cur_patch;
    unsigned m_cur_pan;
    ChannelCtrl m_pctrl;

public:
    CMidiChannel();
    ~CMidiChannel();
    void reset_rnd_prgm_table();

    void set_cur_status(unsigned long);
    unsigned long get_cur_status();
    void set_cur_volume(unsigned long cur_volume) { m_cur_volume = cur_volume; }
    unsigned long get_cur_volume() { return m_cur_volume; }
    void set_cur_pitch(unsigned long);
    unsigned long get_cur_pitch();
    void set_cur_patch(unsigned long cur_patch) { m_cur_patch = cur_patch; }
    unsigned long get_cur_patch() { return m_cur_patch; }
    void set_ctrl(ChannelCtrl *pctrl) { m_pctrl = *pctrl; }
    ChannelCtrl *get_ctrl();
    int is_randomize_patches() { return m_pctrl.is_randomize_patches(); }

    int get_patch_lo();
    int get_patch_hi();
    unsigned long xlat_program(unsigned long program_number);
    void set_cur_pan(unsigned int pan) { m_cur_pan = pan; }
    int get_cur_pan() { return m_cur_pan; }

    void clear_on_off_table();
};
typedef char CMidiChannel_sizeof_check[sizeof(CMidiChannel) == 0x4c ? 1 : -1];

struct meter {
    unsigned long bpm;
    unsigned long type;
};

struct mmgr_flags {
    int need_timer_reset : 1;
    int attached : 1;
    int startup : 1;
    int stop_pending : 1;
    int reset_pending : 1;
    int switch_state : 1;
    int GT_req_pending : 1;
    unsigned int switch_type : 2;
    int looping : 1;
    int continuous : 1;
    int stopping : 1;
};

class CMidiMgr : public Midi, public CMidiFile {
    friend class CAmbienceMgr;
    friend class Dll_Midi_Device;

    HWND m_hwnd;

    mmgr_flags m_flags;
    static MidiMgrList m_midi_list;

    DList<DllTrackSet> m_trackset_list;

    DList<DllTrackSet> m_GT_trackset_list;
    int *m_switch_table;
    int m_nswitchers;
    int m_nswitch_threads;
    unsigned long m_switch_index;
    unsigned long m_switch_tracklo;
    unsigned long m_switch_trackhi;
    SNDERR reload();

    CMidiChannel m_channels[16];
    meter m_meter;
    unsigned long m_tempo;
    unsigned long m_time_ticks;
    double m_ms_running_time;
    double m_ms_per_tick;

    unsigned long m_volume;
    unsigned long m_last_update_time;
    unsigned long m_current_active_set;
    int m_key;
    unsigned int m_flic_frame;

    SNDERR set_active_tracks();

public:
    virtual SNDERR set_active_tracks(unsigned long lo, unsigned long hi);
    virtual SNDERR set_active_tracks(unsigned int setid);

private:
    SNDERR TT_play_trackset(unsigned int setid);
    SNDERR TT_play_trackset(unsigned lo, unsigned hi, unsigned sync);
    SNDERR TT_play_trackset(TrackSet *pcur_set);

    SNDERR TT_stop_trackset(unsigned int setid);
    SNDERR TT_stop_trackset(unsigned lo, unsigned hi);
    SNDERR TT_xpose_trackset(unsigned setid, int pitch);
    SNDERR TT_xpose_trackset(unsigned lo, unsigned hi, int pitch);

    SNDERR TT_set_trackset_done_callback(unsigned int setid, void(__cdecl *pcallback)(void *));

    SNDERR TT_set_track_loop_state(unsigned int track, bool state);

public:
    CProgramMgr *m_patch_table[128];
    static DList<Dll_KeyMap> m_keymap_list;

    virtual SNDERR set_track(unsigned char *pdata, unsigned long tracknum);
    SNDERR fade_track(unsigned long tracknum);
    virtual SNDERR mute_track(unsigned long tracknum);
    virtual SNDERR unmute_track(unsigned long tracknum);
    virtual SNDERR add_active_trackset(TrackSet *pts);
    SNDERR add_active_trackset(DllTrackSet *set);

    virtual TrackSet *get_trackset(unsigned int set_id);
    virtual SNDERR xpose_trackset(unsigned int setid, int pitch);
    virtual bool is_trackset_playing(unsigned int setid);
    virtual SNDERR set_active_range_lo(unsigned int setid, unsigned int lo);
    virtual SNDERR set_active_range_hi(unsigned int setid, unsigned int hi);
    virtual SNDERR remove_active_trackset(unsigned int id);
    SNDERR post_reset();

    virtual SNDERR reset();
    CMidiMgr();
    CMidiMgr(HWND hwnd);
    virtual ~CMidiMgr();
    virtual SNDERR load(const char *fname);
    SNDERR load(HMMIO hfile, const char *fname);
    virtual SNDERR unload();
    SNDERR direct_unload();

    virtual SNDERR play();
    virtual SNDERR play(unsigned long patch_num);
    virtual SNDERR fade();
    virtual SNDERR stop();
    void direct_stop(bool stopSyncAlso);
    SNDERR post_unload();

    virtual SNDERR set_tempo(unsigned long tempo);
    void set_ms_per_tick(unsigned long);
    virtual void set_volume(int vol);
    virtual void set_pan(int pan);
    virtual void set_loop_state(int state) { CMidiFile::set_loop_state(state); }
    virtual SNDERR set_fade(unsigned long ms_fade)
    {
        CMidiFile::set_fade(ms_fade);
        return SNDERR_OK;
    }
    virtual unsigned long get_time();
    virtual int is_looping() { return CMidiFile::get_loop_state(); }
    int is_attached();
    virtual int is_playing() { return CMidiFile::is_playing() || m_flags.stop_pending != 0; }
    static CMidiMgr *get_list_head();
    static CMidiMgr *get_list_tail();
    CMidiMgr *get_next_playing();
    CMidiMgr *get_prev_playing();
    void set_hwnd(HWND__ *);
    HWND get_hwnd() { return m_hwnd; }
    virtual SNDERR attach()
    {
        m_midi_list.AppendObject1(this);
        m_flags.attached = 1;
        return SNDERR_OK;
    }
    virtual SNDERR detach()
    {
        m_midi_list.RemoveBySequence(this);
        return SNDERR_OK;
    }
    static void free_all();

    void prepare_channels();
    SNDERR prepare_track(unsigned long tracknum);

    SNDERR prepare_track(CMidiTrack *ptrack);
    SNDERR process_event(CMidiTrack *cur_track, unsigned long *bytes_used);
    unsigned long control_change(unsigned char *pdata, unsigned long cur_channel, CMidiTrack *ptrack);
    unsigned long program_change(unsigned char *pdata, unsigned long cur_channel);
    unsigned long note_on(unsigned char *pdata, unsigned long cur_channel, CMidiTrack *ptrack);
    unsigned long note_off(unsigned char *pdata, unsigned long cur_channel, CMidiTrack *ptrack);
    unsigned long meta_event(unsigned char *pdata, CMidiTrack *cur_track);
    unsigned long pitch_wheel(unsigned char *pdata, unsigned long cur_channel);
    SNDERR update_tracks(unsigned long ms_time_past);
    void set_meter(meter *);
    virtual SNDERR map_patch(char *fname, unsigned long number, unsigned long midi_channel);
    virtual SNDERR map_patch(char *fname, unsigned long patch_num);
    SNDERR map_patch(ProgramCtrl *pctrl, bool blegacy);

    virtual SNDERR clear_patch(unsigned long number);
    SNDERR set_program_ctrl(ProgramCtrl *pctrl);
    SNDERR set_channel_ctrl(ChannelCtrl *pctrl);
    virtual SNDERR add_keymap(KeyMap *pkm);
    virtual void set_base_path(const char *p);
    virtual unsigned long get_ntracks() { return CMidiFile::get_ntracks(); }
    void switch_track();

    virtual SNDERR set_track_loop_state(unsigned int track, bool state);
    virtual SNDERR get_track_velocity_offset(unsigned int track, int *pvelocity_offset);
    virtual SNDERR set_track_velocity_offset(unsigned int track, int velocity_offset);
    virtual bool get_track_loop_state(unsigned int track);
    SNDERR set_track_ctrl(TrackCtrl *pctrl);
    virtual SNDERR set_switch_type(unsigned long switch_type);
    virtual SNDERR set_nswitch_threads(unsigned long nswitch_threads);
    virtual SNDERR add_switch_range(unsigned long tracklo, unsigned long trackhi);
    void reset_switch_table(int *table);

    virtual void switch_state(bool state) { m_flags.switch_state = state; }
    SNDERR set_control_track(unsigned long track);
    virtual unsigned long get_control_track() { return m_control_track; }
    virtual unsigned long get_total_track_ticks(unsigned long track);
    void set_GT_request_pending(bool state) { m_flags.GT_req_pending = state; }
    SNDERR seek_to_prior(CMidiTrack *ptrack, unsigned long seek_ticks);
    void set_tick_offset(CMidiTrack *ptrack, unsigned long offset, unsigned long flags, int sync_track);
    virtual SNDERR stop_trackset(unsigned int setid);
    virtual SNDERR play_trackset(unsigned int setid);
    virtual void set_play_type(unsigned int play_type);
    virtual SNDERR load_patch(unsigned int patch_num);
    virtual SNDERR unload_patch(unsigned int patch_num);
    virtual SNDERR set_patch(unsigned int midi_channel, unsigned int patch_num);
    unsigned long pack_midi_msg(unsigned int status, unsigned int channel, unsigned int d1, unsigned int d2)
    {
        return status | channel | (d1 << 8) | (d2 << 16);
    }
    virtual SNDERR set_control(void *pcontrol, int target_type, int target);
    virtual void *get_control_target(int target_type, int target);
    virtual int get_key() { return m_key; }
    virtual void set_frame(unsigned int frame)
    {
        if (is_sync_to_flic())
            m_flic_frame = frame;
    }
    virtual unsigned get_frame() { return m_flic_frame; }

    bool isKeyMapLoaded(const char *name);
    static void cleanOutKeymapList();
};

class CAmbienceMgr : public CMidiMgr {
private:
    char m_fname[0x100];
    void strip_path(char *temp_path);
    void make_path_local(char *buff, char *file_name);

    HMMIO m_hfile;
    DList<MidiCtrl> m_control_load_list;

    void read_control();
    void create_control_trees();

public:
    virtual SNDERR load(const char *fname);
    CAmbienceMgr();
    virtual ~CAmbienceMgr();
};

#endif
