#ifndef CMIX_H
#define CMIX_H

struct CM_Flags_tag {
    int active : 1;
    int use_mmx : 1;
};

class CWaveMixer {
    friend class Dll_Wave_Device;

    CDSB *m_pdsb;
    double *m_pmixbuff;

    CM_Flags_tag m_flags;

    unsigned long m_mixbuff_size;

    unsigned long m_nchannels;

    void fill_mixbuff(unsigned long fill_value);

    HWND m_hwnd;

    DWORD m_last_time;

    WAVEFORMATEX m_wfx;
    HMMIO m_hfile_dump;
    double *m_fpmix_buffer;
    short *m_pword_buffer;

public:
    CWaveMixer();
    ~CWaveMixer();
    SNDERR open_dump_file(char *fname);
    SNDERR close_dump_file();
    SNDERR add_wf(CWaveFile *);
    SNDERR mb_2_dsb(CDSB *pdsb, unsigned long nbytes, unsigned long position);
    SNDERR mix_2_mb(CDSB *pdsb);
    SNDERR mix_2_3d_mb(CDSB *pdsb_left, CDSB *pdsb_right);
    SNDERR mix_2_fpbuffer(double *fpbuffer1, double *fpbuffer2, unsigned long nsamples);
    SNDERR fpmb_2_dsb(CDSB *pdsb, double *pmix_buffer, DWORD nbytes, DWORD position);
    SNDERR fpmb_2_3ddsb(CDSB *pdsb_l, CDSB *pdsb_r, double *pmix_buffer, DWORD nbytes, DWORD position_l,
                        DWORD position_r);
    void fp2word(short *dst, double *src, unsigned long nwords);
    void left_fp2word(short *dst, double *src, unsigned long nwords);
    void right_fp2word(short *dst, double *src, unsigned long nwords);
    SNDERR set_mixbuff_size(unsigned long size);
    unsigned long get_mixbuff_size();
    static int get_filter_index(WAVEFORMATEX *pwfx, WAVEFORMATEX *pwfx_dsb);

    void set_hwnd(HWND hwnd) { m_hwnd = hwnd; }
    HWND__ *get_hwnd();
    WAVEFORMATEX *get_format() { return &m_wfx; }
    void set_active(int active_flag) { m_flags.active = active_flag; }

    void set_dsb(CDSB *);
    void set_nchannels(unsigned long);
    int is_active();
    SNDERR clean_play_list();
    int uses_mmx() { return m_flags.use_mmx; }
    void use_mmx(int mmx_flag) { m_flags.use_mmx = mmx_flag; }
    void set_word_buffer(short *);
};

#endif
