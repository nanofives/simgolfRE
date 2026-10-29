#ifndef DSMGR_H
#define DSMGR_H

struct DSMgrflags_tag {
    unsigned char enuminitflag : 1;
    unsigned char deviceinitflag : 1;
    unsigned char ESS_device : 1;
    unsigned char is_3d : 1;
    unsigned char eax : 1;
    unsigned char a3d : 1;
};

class CDSMgr {
    friend SNDERR LinkToGlobalList(CDSB *v);
    friend void UnlinkFromBufferList(CDSB *v);
    friend void UnreserveHwVoice(CDSMgr *ss);
    friend void ReleaseEvictedBuffer(CDSMgr *ss, CDSB *dead);
    friend int EvictIdleBuffer(CDSMgr *ss);

    IDirectSound *m_lpds;
    DSDevice_tag *m_devlist;

    DSDevice_tag *m_pcurdev;

    DSCAPS m_dscaps;

    DSMgrflags_tag m_flags;

    CDSB m_primary_dsb;
    CDSB *dsb_list;

    unsigned long m_ndsdevices;
    unsigned long m_max_hwbuffers;
    unsigned long m_max_duphwbuffers;

    unsigned long m_max_basehwbuffers;

    unsigned long m_num_basehwbuffers;
    unsigned long m_num_duphwbuffers;

    unsigned long m_nduphwbuffers_per_sound;

    unsigned long m_ms_rate;

    DList<CDSB> m_dup_hwdsb_list;

    DList<CWaveFile> m_pwf_hwbuffers;
    static CDSB *gbl_dsb_head;
    static CDSB *gbl_dsb_tail;
    static HWND m_game_hwnd;

    unsigned long m_volume;

    int m_nreleases;

    unsigned long m_dsclflags;
    static unsigned int m_device;

    IDirectSound3DListener *m_pds3d_listener;
    CDSB *m_peax_listener_dsb;
    EAX_REVERB_PROPERTIES m_saved_eax;

    float m_posx;
    float m_posy;
    float m_posz;

public:
    CDSMgr();
    ~CDSMgr();
    static int __stdcall ds_enum_callback(GUID *lpGuid, char *lpstrDescription, char *lpstrModule, void *lpContext);
    CDSB *get_primary_dsb();
    void set_volume(unsigned long volume);
    unsigned long get_volume() { return m_volume; }

    SNDERR create(HWND hwnd, unsigned long dsclflags);

    SNDERR create_dsb(CDSB **ppdsb);
    SNDERR create_dsb(CDSB **ppdsb, DSBUFFERDESC *lpdsbdesc);
    SNDERR create_dsb(CDSB **ppdsb, unsigned long flags);
    SNDERR create_dsb(CDSB **ppdsb, unsigned long flags, unsigned long size);
    SNDERR create_dsb(CDSB **ppdsb, WAVEFORMATEX *pwfx, unsigned long flags, unsigned long size);
    SNDERR duplicate_dsb(CWaveFile *powner, CDSB *pdsb, CDSB **ppnew_dsb);
    SNDERR steal_duplicate_hwdsb(CWaveFile *newowner, CDSB **ppdsb);
    Node<CDSB> *get_first_duphwdsb() { return m_dup_hwdsb_list.get_head(); }
    SNDERR free_a_duphwdsb();
    SNDERR destroy_dsb(CDSB *pdsb);
    SNDERR destroy_all_dsbs();
    inline SNDERR detach_dsb(CDSB *pdsb);
    SNDERR attach_dsb(CDSB *pdsb);
    void destroy();

    void disable();

    SNDERR init(unsigned int flags, unsigned long ms_rate);
    SNDERR init(unsigned long device_id, unsigned int flags, unsigned long ms_rate);
    SNDERR ds_enum();
    SNDERR get_description(unsigned long id, char *buff, unsigned long buff_size);
    unsigned long get_ndevices() { return m_ndsdevices; }
    unsigned long get_hw_mem_size();
    SNDERR set_pcur(unsigned long devnum);
    SNDERR list_devices();
    int is_emulated() { return m_dscaps.dwFlags & DSCAPS_EMULDRIVER; }
    bool is_3d() { return m_flags.is_3d; }
    bool is_eax() { return m_flags.eax; }
    void suspend();
    void restart();
    static HWND get_game_hwnd() { return m_game_hwnd; }
    SNDERR set_hwnd(HWND hwnd);
    IDirectSound *pds() { return m_lpds; }
    unsigned int ndup_hw_buffers();
    unsigned int nbase_hw_buffers();
    SNDERR init_A3D();
    SNDERR uninit_A3D();
    unsigned char is_a3d();
    SNDERR get_eax(EAX_REVERB_PROPERTIES *peax);
    SNDERR set_eax(EAX_REVERB_PROPERTIES *peax);
    SNDERR set_listener_position(float x, float y, float z);

    SNDERR set_listener_position() { return set_listener_position(m_posx, m_posy, m_posz); }
};

typedef char CDSMgr_sizeof_check[sizeof(CDSMgr) == 0x1b8 ? 1 : -1];

#endif
