#ifndef DSB_H
#define DSB_H

struct DSB_FLAGS_tag {
    unsigned int created : 1;
    unsigned int type : 1;
    unsigned char virgin : 1;
};

class CDSB {
    friend void UnlinkFromBufferList(CDSB *v);
    friend void ReleaseEvictedBuffer(CDSMgr *ss, CDSB *dead);
    friend int EvictIdleBuffer(CDSMgr *ss);
    friend class CDSMgr;
    friend class CWaveMixer;

    SNDERR dsbrestore();
    unsigned long diff(unsigned long pos1, unsigned long pos2)
    {
        return pos2 < pos1 ? pos2 + m_dsbdesc.dwBufferBytes - pos1 : pos2 - pos1;
    }

    DSB_FLAGS_tag m_flags;

    unsigned long m_size;
    unsigned long m_curplaypos;
    unsigned long m_last_dsb_play;
    unsigned long m_cur_dsb_play;
    unsigned long m_cur_play_interval;
    unsigned long m_cur_play_write_interval;

    CWaveFile *m_pwf_owner;

    unsigned long m_last_dsb_write;
    unsigned long m_last_returned_dsb_write;
    unsigned long m_last_user_write;
    unsigned long m_last_write_limit;
    unsigned long m_nsamples_per_tick;

    unsigned long m_write_margin;

    unsigned long m_write_offset;
    unsigned long m_block_size;
    int m_delay;
    unsigned long m_write_cursor;
    unsigned long m_total_ms_played;
    unsigned long m_ms_write_margin;
    unsigned long m_start_time;
    unsigned long m_buffer_mask;

    CDSMgr *m_pdsmgr;
    IDirectSoundBuffer *m_lpdsb;
    IDirectSound3DBuffer *m_pds3d_buffer;
    IDirectSound3DListener *m_pds3d_listener;

    IKsPropertySet *m_preverb;

    DSBUFFERDESC m_dsbdesc;
    DSBCAPS m_dsbcaps;
    WAVEFORMATEX m_wfx;
    CDSB *m_pprev;
    CDSB *m_pnext;

private:
    long m_volume;
    float m_xpos;
    float m_ypos;
    float m_zpos;

    SNDERR init_3d();
    SNDERR init_reverb();

    SNDERR release_3d();

    SNDERR release_reverb();

public:
    CDSB();
    ~CDSB();
    SNDERR init();
    void reinit();
    SNDERR create(IDirectSound *lpds, unsigned long ms_write_margin);

    int get_delay() { return m_delay; }
    void set_delay(int delay) { m_delay = -delay; }
    void adjust_delay(int time_past) { m_delay += time_past; }
    SNDERR destroy();
    SNDERR play(unsigned long flags);
    SNDERR mpeg_play();
    static SNDERR dserrhandler(long err);
    int isdsbtype(unsigned long type) { return m_dsbcaps.dwFlags | type; }
    void set_owner(CWaveFile *pwf_owner) { m_pwf_owner = pwf_owner; }
    int is_playing();
    void set_dsmgr(CDSMgr *pdsmgr) { m_pdsmgr = pdsmgr; }
    void set_next(CDSB *pnext) { m_pnext = pnext; }
    void set_prev(CDSB *pprev) { m_pprev = pprev; }
    CDSB *get_next() { return m_pnext; }
    CDSB *get_prev() { return m_pprev; }
    IDirectSound3DBuffer *get_3d_buffer() { return m_pds3d_buffer; }
    IDirectSound3DListener *get_3d_listener() { return m_pds3d_listener; }
    IKsPropertySet *get_ds_property_set();
    tWAVEFORMATEX *get_format();
    int get_nchannels();
    int get_nbits();
    unsigned long get_nwrite_margin_samples() { return m_wfx.nBlockAlign ? m_write_margin / m_wfx.nBlockAlign : 0; }
    unsigned long bytes_to_samples(unsigned long nbytes) { return m_wfx.nBlockAlign ? nbytes / m_wfx.nBlockAlign : 0; }
    unsigned long samples_to_bytes(unsigned long nsamples) { return nsamples * m_wfx.nBlockAlign; }
    SNDERR setsamplerate(unsigned long srate);
    SNDERR setsamplesize(unsigned long samplesize);
    SNDERR setnumchannels(unsigned long nchannels);

    SNDERR set_type(unsigned long type);
    SNDERR set_buffer_size(unsigned long size);
    SNDERR set_format(WAVEFORMATEX *pwfx);
    void set_last_play_position(unsigned long);
    unsigned long get_last_play_position();
    unsigned long get_play_interval() { return m_cur_play_interval; }
    unsigned long get_cur_play_write_interval() { return m_cur_play_write_interval; }
    SNDERR stop();
    SNDERR release();
    void set_samples_per_tick(unsigned long ms);
    SNDERR set_write_margin(unsigned long ms, unsigned long nblocks);
    SNDERR get_pointer(void **pp1, unsigned long *s1, void **pp2, unsigned long *s2);
    SNDERR release_pointer(void *p1, unsigned long s1, void *p2, unsigned long s2);
    SNDERR lock(unsigned long pos, unsigned long size, void **pp1, unsigned long *s1, void **pp2, unsigned long *s2);
    SNDERR unlock(void *p1, unsigned long s1, void *p2, unsigned long s2);

    SNDERR quiet();
    void set_dsb_desc(DSBUFFERDESC *lpdsbdesc) { m_dsbdesc = *lpdsbdesc; }
    void set_dsb_flags(unsigned long flags) { m_dsbdesc.dwFlags = flags; }
    SNDERR get_cur_dsbwrite(unsigned long *dsbwrite);
    SNDERR get_cur_dsbplay(unsigned long *dsbplay);
    void set_volume(unsigned long volume);
    void set_volume(double volume);
    void set_volume(long volume);

    long get_volume() { return m_volume; }
    void set_frequency(unsigned long freq);
    SNDERR get_write_info(unsigned long *nbytes_to_write, unsigned long *position, unsigned long ms_time_past);
    SNDERR get_write_info(unsigned long nbytes, unsigned long *position);
    SNDERR get_real_write_info(unsigned long *nbytes_to_write, unsigned long *position);
    SNDERR get_emulated_write_info(unsigned long *nbytes_to_write, unsigned long *position);
    SNDERR get_emulated_write_info(unsigned long *nbytes_to_write, unsigned long *position, unsigned long ms_time_past);
    SNDERR sync_emulated();
    bool is_3d();
    SNDERR set_position3d(float x, float y, float z);
    SNDERR set_xpos(float x);
    SNDERR set_ypos(float y);
    SNDERR set_zpos(float z);
    float xpos() { return m_xpos; }
    float ypos() { return m_ypos; }
    float zpos() { return m_zpos; }
    SNDERR reset();
    SNDERR set_reverb_mix(float mix);
    void resync();
};

#endif
