#ifndef WAVEINDEVICEMGR_H
#define WAVEINDEVICEMGR_H

class WIBuffer {
    WAVEHDR m_whdr;

public:
    WIBuffer();
    ~WIBuffer();
    SNDERR set_buffer(DWORD size);
    void set_next(WAVEHDR *lpwhdr);
    WAVEHDR *get_whdr();
    void clear_flags() { m_whdr.dwFlags = 0; }

public:
    void clear_recorded();
    void clear_reserved();
};

struct WID_Flags_tag {
    int recording : 1;
    int enabled : 1;
};

class WaveInDeviceMgr {
    friend MMRESULT StopCapture(WaveInDeviceMgr *m);
    friend SNDERR ReleaseCapture(WaveInDeviceMgr *m);

    WID_Flags_tag m_flags;

    WAVEINCAPSA m_caps;

    unsigned long m_ndevices;

    DWORD m_format;
    unsigned long m_device_id;
    HWAVEIN m_handle;
    WAVEFORMATEX m_wfx;
    DList<WIBuffer> m_buffers;
    CVoiceTxMgr *m_pvtx;

public:
    SNDERR process_buffer(WIBuffer *pwib);
    SNDERR enable();
    SNDERR disable();
    int is_enabled() { return m_flags.enabled; }
    static void __stdcall call_back(HWAVEIN hwi, WORD msg, DWORD instance, DWORD p1, DWORD p2);

    SNDERR init(unsigned long nbuffers, DWORD buffer_size);
    unsigned long get_num_devices();
    WaveInDeviceMgr();

    virtual ~WaveInDeviceMgr();

    SNDERR select(unsigned long id);
    SNDERR handle_error(unsigned errorval);

    SNDERR set_format(DWORD format);

    SNDERR free_buffers();
    SNDERR start_record();
    SNDERR stop_record();
    SNDERR prepare_headers();
    SNDERR unprepare_headers();
    SNDERR add_buffers();
    SNDERR add_buffer(WIBuffer *pbuffer);
    SNDERR hook_vtx(CVoiceTxMgr *pvtx);
    SNDERR unhook_vtx();
};

#endif
