#ifndef WAVEFILE_H
#define WAVEFILE_H

struct SampleLoop_tag {
    unsigned long dwIdentifier;
    unsigned long dwType;
    unsigned long dwStart;
    unsigned long dwEnd;
    unsigned long dwFraction;
    unsigned long dwPlayCount;
};

struct SampleData_tag {
    unsigned long dwManufacturer;
    unsigned long dwProduct;
    unsigned long dwSamplePeriod;
    unsigned long dwMIDIUnityNote;
    unsigned long dwMIDIPitchFraction;
    unsigned long dwSMPTEFormat;
    unsigned long dwSMPTEOffset;
    unsigned long cSampleLoops;
    unsigned long cbSamplerData;
};

struct SampleChunk_tag {
    SampleData_tag sd;
    SampleLoop_tag *psl;
    void *pssd;
};

struct CuePoint_tag {
    unsigned long dwName;
    unsigned long dwPosition;
    unsigned long fccChunk;
    unsigned long dwChunkStart;
    unsigned long dwBlockStart;
    unsigned long dwSampleOffset;
};

struct WBFlags_tag {
    unsigned char playing : 1;
    int looping : 1;
    unsigned char linked : 1;
    unsigned char release : 1;
    unsigned char done : 1;
    unsigned char fading : 1;
    unsigned char last_fade : 1;
    unsigned char fade_in : 1;
    unsigned char ramping : 1;
};

typedef union Shifter_tag {
    __int64 whole;
    struct {
        unsigned short empty;
        unsigned short fxd_pt_frac;
        unsigned long fxd_pt_int;
    };
} SHIFTER;

class WaveBuffer {
    friend class CWaveFile;
    friend unsigned long __cdecl fp_mov_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                     unsigned long flags);
    friend unsigned long __cdecl fp_add_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                     unsigned long flags);
    friend unsigned long __cdecl fp_add_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);
    friend unsigned long __cdecl fp_mov_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);
    friend unsigned long __cdecl fp_mov_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                  unsigned long flags);
    friend unsigned long __cdecl fp_add_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                  unsigned long flags);
    friend unsigned long __cdecl fp_mov_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);
    friend unsigned long __cdecl fp_add_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);

    MMIOINFO m_mmioinfo;
    int m_pan;
    unsigned long m_pitch;

    int m_volume;
    double m_fp_volume;
    double m_fp_reset_volume;
    int m_right_volume;
    double m_fp_right_volume;
    int m_left_volume;
    double m_fp_left_volume;
    unsigned long m_current_position;
    unsigned long m_start_position;
    unsigned long m_end_position;
    WBFlags_tag m_flags;
    WAVEFORMATEX m_wfx;
    WaveBuffer *m_plinked_wb;
    Shifter_tag m_shifter;
    unsigned long m_time;
    unsigned long m_delay;
    unsigned long *m_time_past;
    int m_loop_count;

    int m_cur_loop_count;

    char *m_ploop_end;
    char *m_ploop_start;
    unsigned long m_loop_size;
    CDSB *m_phwdsb;
    unsigned long m_midi_note;
    unsigned long m_velocity;
    int m_fade_amount;

    double m_fp_fade_amount;
    double m_fp_reset_fade_amount;
    double m_fp_shifter;
    double m_fp_pitch;

public:
    WaveBuffer();
    ~WaveBuffer();
    void init(MMIOINFO *pmmioinfo, WAVEFORMATEX *pwfx);

    WaveBuffer *get_plinked_wb() { return m_plinked_wb; }
    void set_plinked_wb(WaveBuffer *plinked_wb) { m_plinked_wb = plinked_wb; }
    void reset();
    void set_start_position(unsigned long start_position) { m_start_position = start_position; }
    void set_end_position(unsigned long end_position) { m_end_position = end_position; }
    void set_current_position(unsigned long current_position) { m_current_position = current_position; }
    void set_time(unsigned long time) { m_time = time; }
    void set_time_past(unsigned long *ptime_past) { m_time_past = ptime_past; }
    unsigned long get_start_position();
    unsigned long get_end_position();
    unsigned long get_current_position() { return m_current_position; }
    unsigned long get_time() { return m_time; }
    void set_pnext(char *pnext) { m_mmioinfo.pchNext = pnext; }
    char *get_pnext() { return m_mmioinfo.pchNext; }
    char *get_pstart() { return m_mmioinfo.pchBuffer; }
    char *get_pend() { return m_mmioinfo.pchEndRead; }
    char *get_ploop_start() { return m_ploop_start; }
    char *get_ploop_end() { return m_ploop_end; }
    unsigned long get_loop_size();
    int is_end_of_data(unsigned long);
    void set_pan(int pan);
    void set_pitch(double pitch) { m_fp_pitch = pitch; }
    void set_volume(double volume) { m_fp_volume = volume; }
    void set_volume(unsigned long volume);

    void set_left_volume(double volume) { m_fp_left_volume = volume; }
    void set_right_volume(double volume) { m_fp_right_volume = volume; }
    void fade();
    void fade_in();
    void set_fade_amount(int);
    int get_left_volume() { return m_left_volume; }
    int get_right_volume() { return m_right_volume; }
    int get_pan() { return m_pan; }
    void set_delay(unsigned long delay) { m_delay = delay; }
    unsigned long get_delay() { return m_delay; }
    unsigned long get_pitch() { return m_pitch; }
    int get_volume() { return m_volume; }
    unsigned long get_midi_note() { return m_midi_note; }
    void set_midi_note(unsigned long midi_note) { m_midi_note = midi_note; }
    unsigned long get_velocity() { return m_velocity; }
    void set_velocity(unsigned long velocity) { m_velocity = velocity; }
    void set_loop_state(int state) { m_flags.looping = state; }
    int is_looping() { return m_flags.looping; }
    int is_linked() { return m_flags.linked; }
    int is_fading() { return m_flags.fading; }
    void set_fade(unsigned long ms);

    void set_fade_in(unsigned long ms);
    unsigned long get_fade_amount() { return m_fade_amount; }
    double get_fp_fade_amount() { return m_fp_fade_amount; }
    void set_link_state(bool state) { m_flags.linked = state; }
    void set_loop_count(int count) { m_loop_count = count; }
    void set_ploop_start(char *pstart) { m_ploop_start = pstart; }
    void set_release(bool release) { m_flags.release = release; }
    void set_ploop_end(char *pend) { m_ploop_end = pend; }
    void set_loop_size(unsigned long size) { m_loop_size = size; }
    int get_loop_count() { return m_loop_count; }
    unsigned long get_nsamples_left_in_buffer();
    unsigned long btos(unsigned long bytes) { return bytes / m_wfx.nBlockAlign; }
    unsigned long stob(unsigned long);
    void set_done(bool state) { m_flags.done = state; }
    int is_done() { return m_flags.done; }
    int is_released() { return m_flags.release; }
    SHIFTER *get_shifter() { return &m_shifter; }
    MMIOINFO *get_mmioinfo() { return &m_mmioinfo; }
};

struct WFFlags_tag {
    int all_in_buffer : 1;
    int loaded : 1;
    int looping : 1;
    int fading : 1;
    int done : 1;
    int eod : 1;
    int active : 1;
    int release : 1;
    int reset_pending : 1;
    int unload_pending : 1;
    int advance_pending : 1;
    int mixer_unload : 1;
    unsigned int buffer_bits : 16;
    int hwbuffer : 1;
    int try_hw : 1;
    int wndproc_delete : 1;
    int big_buffer : 1;
    int last_loop : 1;
    int last_fade : 1;
    int mpeg : 1;
    int fade_in : 1;
    int ramping : 1;
    unsigned int buffer3d : 1;
    unsigned int hwstreaming : 1;
    unsigned int shared_dsb : 1;
    int queueReady : 1;
    int attrib_400 : 1;
    int release_cur_sound : 1;
};

#include "mpeg.h"

class CWaveMgr;
class XAudioPlayer;

class CWaveFile {
    friend class CDSMgr;
    friend class CMidiMgr;
    friend class Dll_Wave_Device;
    friend class CWaveMixer;
    friend class CWaveMgr;
    friend unsigned long __cdecl fp_mov_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                     unsigned long flags);
    friend unsigned long __cdecl fp_add_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                     unsigned long flags);
    friend unsigned long __cdecl fp_add_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);
    friend unsigned long __cdecl fp_mov_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);
    friend unsigned long __cdecl fp_mov_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                  unsigned long flags);
    friend unsigned long __cdecl fp_add_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                  unsigned long flags);
    friend unsigned long __cdecl fp_mov_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);
    friend unsigned long __cdecl fp_add_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller,
                                                   unsigned long flags);

    void mmioerr(unsigned long err);
    SNDERR advance();

    SNDERR advance_stream();
    BOOL need_to_advance();

    void undo_load();

    int is_end_of_data(unsigned long);

    HMMIO m_hfile;
    HMMIO m_hbuffer;

    MMIOINFO *m_pmmioinfo;

    MMIOINFO m_mmioinfo;

    CDSB *m_phwdsb;
    union {
        WFFlags_tag m_flags;
        unsigned long m_flagsWord;
    };

    unsigned long m_bufflimit;

    char m_fname[0x100];

    unsigned long m_curstartpos;
    unsigned long m_curendpos;

    unsigned long m_curpos;

    unsigned long m_curfpos;
    unsigned long m_dataoffset;
    unsigned long m_data_size;
    HWND m_hwnd;
    WAVEFORMATEX m_wfx;

    SampleChunk_tag m_sample_chunk;

    unsigned long m_fade_time;
    int m_fade_amount;

    double m_fp_fade_amount;

    double m_fp_dst_volume;
    unsigned long m_volume;
    double m_fp_volume;
    unsigned long m_left_volume;

    double m_fp_left_volume;

    unsigned long m_right_volume;

    double m_fp_right_volume;

    int m_reset_volume;
    double m_fp_reset_volume;
    unsigned long m_reset_right_volume;
    unsigned long m_reset_left_volume;

    unsigned long m_pitch;
    int m_pan;
    unsigned long m_time;
    unsigned long m_delay;

    unsigned long m_time_past;

    WaveBuffer instance[16];

    DList<CDSB> m_dup_hwdsb_list;
    DList<CuePoint_tag> m_cue_list;

    Shifter_tag m_shifter;

    int m_loop_count;
    int m_cur_loop_count;
    unsigned long m_loop_size;
    unsigned long m_loop_startpos;

    unsigned long m_msgs_pending;

    unsigned long m_buffer_mask;

    char *m_pwrite;
    double m_fp_shifter;
    double m_fp_pitch;

    WaveBuffer *m_first_mpeg_frame;
    WaveBuffer *m_last_mpeg_frame;
    unsigned int m_nmpeg_frames;

    unsigned m_ms_length;

#ifdef USE_MPEG
    MilesMpeg m_milesMpeg;
#endif

    unsigned long read_data(char *buff, unsigned long request);
    void get_write_pointers(char **pp1, unsigned long *size1, char **pp2, unsigned long *size2, unsigned long size);

    SNDERR read_from_file(char *pdest, unsigned long *nbytes);

    SNDERR load_3d(CDSMgr *pds);

    CWaveMgr **m_ppQueuedSound;

    CWaveMgr **m_ppCurSound;

public:
    XAudioPlayer *m_pxaplayer;

    CWaveFile();
    CWaveFile(char *fname);
    ~CWaveFile();
    SNDERR load(const char *fname);
    SNDERR reload();
    SNDERR hw_load(CDSMgr *pds);

    SNDERR mpeg_load(const char *fname);
    SNDERR mpeg_unload();

    SNDERR clear_mpeg_frames();
    SNDERR unload();
    SNDERR advance_file();

    void add_dup_hwdsb(CDSB *pduphwdsb);
    void remove_dup_hwdsb(CDSB *pduphwdsb);
    SNDERR hwdsb_stop();
    void set_mixer_unload();
    void set_try_hwbuffer() { m_flags.try_hw = 1; }
    void set_try_hwstreaming() { m_flags.hwstreaming = 1; }
    void set_shared_dsb() { m_flags.shared_dsb = 1; }
    bool is_shared_dsb() { return m_flags.shared_dsb != 0; }
    void set_big_buffer() { m_flags.big_buffer = 1; }
    void set_fade(unsigned long ms);
    void set_fade_in(unsigned long ms);
    void ramp(int dst_vol, int src_vol, unsigned int ms_time);
    SNDERR fade_in();
    SNDERR fade_in(unsigned long ms);
    SNDERR fade();
    SNDERR fade(unsigned long ms);
    int is_fading() { return m_flags.fading; }
    void stop_fade() { m_flags.fading = 0; }
    int need_mixer_unload();
    void set_unload_pending();
    void clear_unload_pending();
    int is_unload_pending();
    void set_hwnd(HWND hwnd) { m_hwnd = hwnd; }
    HWND__ *get_hwnd();
    char *get_fname() { return m_fname; }
    void setstartpos(unsigned long);
    void setendpos(unsigned long);
    unsigned long get_current_position() { return m_curpos; }
    unsigned long get_current_marker();
    void set_current_position(unsigned long cur_position) { m_curpos = cur_position; }
    int is_loaded() { return m_flags.loaded; }
    SNDERR get_cursample(unsigned long *pdst);
    unsigned long get_cursample_channel(unsigned long channel);
    SNDERR get_nsamples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                        unsigned long *ret_samples_read);
    SNDERR get_stream_samples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                              unsigned long *ret_samples_read);
    SNDERR stream_to_hwbuffer(unsigned long timePast, unsigned long filter_index, unsigned long *ret_samples_read);
    SNDERR get_mpeg_samples_to_dsb(unsigned long time_past, unsigned long filter_index,
                                   unsigned long *ret_samples_read);
    SNDERR get_mpeg_samples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                            unsigned long *ret_samples_read);
    SNDERR get_buffer_samples(WaveBuffer *pwb, void *pdst, unsigned long nsamples, unsigned long filter_index,
                              unsigned long *ret_samples_read);
    WaveBuffer *get_free_instance();
    WaveBuffer *get_free_instance(unsigned long *i);
    WAVEFORMATEX *get_format() { return &m_wfx; }
    unsigned long operator++(int);
    unsigned long get_nsamples_left_in_buffer();

    int get_nbytes_left_in_stream();
    SNDERR seek_to_curstartpos();
    void set_active(int val) { m_flags.active = val; }
    void set_done()
    {
        m_flags.done = 1;
        for (int i = 0; i < 16; i++)
            set_buffer_done(i);
    }
#ifdef USE_MPEG
    void mpeg_stop() { m_milesMpeg.stop(); }
#endif
    int is_eod() { return m_flags.eod; }
    int is_active() { return m_flags.active; }
    int is_playing() { return m_flags.active && !m_flags.done; }
    int is_static() { return m_flags.all_in_buffer; }
    int is_asr() { return m_sample_chunk.psl != 0; }
    int is_hwbuffer() { return m_flags.hwbuffer; }
    bool is_hwstreaming() { return m_flags.hwstreaming; }
    int is_mpeg() { return m_flags.mpeg; }
    unsigned long get_bps();
    unsigned long get_num_channels();
    unsigned long stob(unsigned long nsamples) { return nsamples * m_wfx.nBlockAlign; }
    unsigned long btos(unsigned long bytes) { return bytes / m_wfx.nBlockAlign; }
    SNDERR reset();

    void reset_flags();
    void set_volume(int vol);
    void set_volume(double volume) { m_fp_volume = volume; }

    void set_hwvolume(int vol);
    void set_pan(int pan);
    void set_pitch(unsigned long pitch);

    void set_pitch(double pitch) { m_fp_pitch = pitch; }
    void set_time(unsigned long);
    void set_time_past(unsigned long time_past) { m_time_past = time_past; }
    void set_delay(unsigned long delay) { m_delay = delay; }
    void set_release(bool release);
    unsigned long get_delay() { return m_delay; }
    unsigned long get_time() { return m_time; }

    unsigned long get_time(unsigned long instance_index);
    unsigned long get_pitch() { return m_pitch; }
    unsigned long get_left_volume() { return m_left_volume; }

    double get_fp_left_volume();
    unsigned long get_right_volume() { return m_right_volume; }
    double get_fp_right_volume();
    CDSB *phwdsb() { return m_phwdsb; }
    unsigned long get_volume();
    double get_fp_volume();
    int get_loop_state() { return m_flags.looping; }
    void set_loop_state(int state);
    void set_loop_count(int count) { m_loop_count = count; }
    int get_loop_count() { return m_loop_count; }
    void set_buffer_done(int buffer_index);
    void set_buffer_inactive(int buffer_index);
    int get_buffer_state(unsigned long buffer_index);
    SNDERR set_static();


    SNDERR play();
    SNDERR play(int ibuffer);
    SNDERR play(int *pinstance);
    SNDERR mpeg_play();

    SNDERR mpeg_fade();

    SNDERR setNextMpegFrame();
    SHIFTER *get_shifter() { return &m_shifter; }
    char *get_pnext() { return m_pmmioinfo->pchNext; }
    char *get_pstart() { return m_pmmioinfo->pchBuffer; }
    char *get_pend()
    {
        if (m_pmmioinfo->pchNext > m_pmmioinfo->pchEndRead)
            return m_pmmioinfo->pchEndWrite;
        return m_pmmioinfo->pchEndRead;
    }
    char *get_pend_write() { return m_pmmioinfo->pchEndWrite; }
    void dec_msgs_pending() { m_msgs_pending--; }
    void set_advance_pending(int flag) { m_flags.advance_pending = flag; }
    unsigned long get_nmsgs_pending();
    unsigned long midi_unity_note() { return m_sample_chunk.sd.dwMIDIUnityNote; }
    WaveBuffer *find_midi_instance(unsigned long midi_note);

    int is_instance_playing(int mask) { return mask & m_flags.buffer_bits; }
    WaveBuffer *get_instance(int instance_index) { return instance_index < 16 ? &instance[instance_index] : 0; }
    SNDERR queueSound(CWaveMgr **ppQueueSound, CWaveMgr **ppCurSound);
    void setppCurSound(CWaveMgr **ppCurSound) { m_ppCurSound = ppCurSound; }
    bool is_3d() { return m_flags.buffer3d && m_flags.loaded; }
    SNDERR set_position3d(float x, float y, float z);
    void set_3d() { m_flags.buffer3d = 1; }

    SNDERR set_xpos(float x);
    SNDERR set_ypos(float y);
    SNDERR set_zpos(float z);
    SNDERR set_reverb_mix(float mix);
    double *get_fp_shifter() { return &m_fp_shifter; }
    double *get_fp_pitch() { return &m_fp_pitch; }
    void decode_frame(unsigned int);
    unsigned int get_ms_length() { return m_ms_length; }
    void ResetAllSlots()
    {
        m_flagsWord |= 0x10;
        for (int ch = 0; ch < 0x10; ch++)
            set_buffer_done(ch);
        m_flagsWord &= ~0x40;
    }
    int UpdateCompletionState();

    void ClearAndReleaseStopNotifyFlag();
    int UpdateFade();
};

extern DList<MMIOINFO> gbl_created_list;

#endif
