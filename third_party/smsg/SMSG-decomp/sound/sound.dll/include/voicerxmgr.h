#ifndef VOICERXMGR_H
#define VOICERXMGR_H

struct VRx_Flags_tag {
    int playing : 1;
    int active : 1;
    int idle : 1;
    int mixer_unload : 1;
    int positions_valid : 1;
    int stop_flag : 1;
};

class CVoiceRxMgr : public VoiceRx {
    VRx_Flags_tag m_flags;
    DList<WaveBuffer> m_buffers;
    MMIOINFO m_in_buffer;
    unsigned long m_buffer_size;
    CDSB *m_pdsb;

    WAVEFORMATEX m_wfx;

    int m_prev_play_position;
    SNDERR add_lpc10buffer(MMIOINFO *pmmioinfo);

    virtual unsigned long get_nbuffers() { return m_buffers.get_nnodes(); }

    struct lpc10_decoder_state *m_lpc10_decoder_state;
    int m_start_id;
    int m_start_buffers;
    int m_buffers_added;
    int m_buffers_played;

public:
    SNDERR enable();
    SNDERR disable();
    CVoiceRxMgr();
    virtual ~CVoiceRxMgr();
    unsigned long stob(unsigned long);
    unsigned long btos(unsigned long);
    virtual SNDERR add_buffer(MMIOINFO *pmmioinfo);
    void update_cur_buffer();
    virtual unsigned long get_buffer_size() { return m_buffer_size; }
    void set_mixer_unload() { m_flags.mixer_unload = 1; }

    virtual SNDERR release();
    virtual SNDERR unload();
    SNDERR direct_unload();

    virtual SNDERR init(unsigned long buffer_size, WAVEFORMATEX *lpwfx, unsigned long flags);
    virtual SNDERR start();
    SNDERR get_nsamples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                        unsigned long *ret_samples_read);

    virtual SNDERR stop();
    virtual int is_playing()
    {
        if (m_flags.playing && m_flags.active)
            return 1;
        return 0;
    }
    int is_active() { return m_flags.active; }
    int is_idle();
    void set_idle(int idle_state) { m_flags.idle = idle_state; }
    WAVEFORMATEX *get_format() { return &m_wfx; }
    int get_prev_play_position();
    int are_positions_valid();
    void validate_positions();
    void invalidate_positions() { m_flags.positions_valid = 0; }
    virtual SNDERR detach();
    int need_mixer_unload();
    SNDERR discard_buffer(HMMIO hfile);
    void set_active(bool state) { m_flags.active = state; }
    void set_done()
    {
        m_flags.playing = 0;
        m_flags.active = 0;
    }

    int lpc10_read_bits(char *f, int *bits, int len);
    int lpc10_write_16bit_samples(short *int16samples, float *const speech, int n);
};

#endif
