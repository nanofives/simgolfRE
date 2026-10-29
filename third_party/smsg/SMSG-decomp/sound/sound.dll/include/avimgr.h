#ifndef AVIMGR_H
#define AVIMGR_H

struct AviFlags {
    int playing : 1;
    int active : 1;
    int idle : 1;
    int mixer_unload : 1;
    int positions_valid : 1;
};

class CAviMgr : public AviSound {
    AviFlags m_flags;

    DList<WaveBuffer> m_frames;

    MMIOINFO m_out_buffer;

    unsigned long m_frame_size;
    unsigned long m_frame_buffer_size;
    unsigned long m_frame_rate;
    unsigned long m_rem_frame_time;

    CDSB *m_pdsb;

    int m_cur_frame;
    int m_start_frame;

    int m_start_position;

    int m_prev_play_position;

    WAVEFORMATEX m_wfx;

public:
    CAviMgr();
    virtual ~CAviMgr();
    unsigned long stob(unsigned long);
    unsigned long btos(unsigned long bytes) { return bytes / m_wfx.nBlockAlign; }
    virtual MMIOINFO *get_frame(unsigned long size);
    virtual SNDERR return_frame(MMIOINFO *pmmioinfo);
    void update_cur_frame();

    virtual int get_cur_frame() { return m_start_frame + m_cur_frame; }
    virtual unsigned long get_frame_size() { return m_frame_size; }
    void set_mixer_unload() { m_flags.mixer_unload = 1; }

    virtual SNDERR release();
    virtual SNDERR init(AVIStreamHeader *paudheader, AVIStreamHeader *pvidheader, WAVEFORMATEX *lpwfx,
                        unsigned long flags);
    virtual SNDERR start(unsigned long start_frame);
    SNDERR start();

    SNDERR get_nsamples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                        unsigned long *ret_samples_read);

    virtual SNDERR stop();
    virtual int is_playing() { return m_flags.playing && m_flags.active; }
    int is_active();
    int is_idle() { return m_flags.idle; }
    void set_idle(int idle_state) { m_flags.idle = idle_state; }
    WAVEFORMATEX *get_format() { return &m_wfx; }
    void set_start_position(int position) { m_start_position = position; }
    void set_prev_play_position(int position) { m_prev_play_position = position; }
    int get_start_position();
    int get_prev_play_position() { return m_prev_play_position; }
    int are_positions_valid() { return m_flags.positions_valid; }
    void validate_positions() { m_flags.positions_valid = 1; }
    void invalidate_positions() { m_flags.positions_valid = 0; }
    virtual SNDERR detach();
    int need_mixer_unload() { return m_flags.mixer_unload; }
    SNDERR discard_frame(HMMIO hfile);
};

#endif
