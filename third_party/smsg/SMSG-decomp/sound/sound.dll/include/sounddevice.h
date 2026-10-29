#ifndef SOUND_DEVICE_H
#define SOUND_DEVICE_H

#include "sound.h"

extern "C" void *__cdecl memset(void *dst, int val, unsigned int count);

class Midi_Device;

struct SD_Flags_tag {
    int active : 1;
    int attached : 1;
};

class Sound_Device {
    SD_Flags_tag m_flags;
    unsigned long m_volume;
    unsigned long m_pan;
    unsigned long m_ms_rate;

protected:
    Sound_Device();
    virtual ~Sound_Device();
    Sound_Device *m_pdll_device;
    SNDERR attach();
    SNDERR detach();
    static Sound_Device *m_devlist_head;
    static Sound_Device *m_devlist_tail;
    Sound_Device *m_pprev;
    Sound_Device *m_pnext;
    virtual SNDERR create_device(unsigned long flags) = 0;
    virtual SNDERR delete_device() = 0;

public:
    virtual void timer_unhook() = 0;
    virtual SNDERR init(HWND game_hwnd, unsigned long flags) = 0;
    virtual SNDERR release() = 0;
    virtual unsigned long get_ndevices() = 0;
    virtual SNDERR select(unsigned long id) = 0;
    virtual SNDERR get_description(unsigned long id, char *buff, unsigned long buffsize) = 0;
    virtual void set_volume(unsigned long volume) = 0;
    virtual unsigned long get_volume() = 0;
    virtual void set_pan(int pan) = 0;
    virtual SNDERR fade(unsigned long ms_fade_time) = 0;
    virtual SNDERR stop() = 0;
    virtual void update_sound() = 0;
    virtual void set_rate(unsigned long ms) = 0;
    virtual unsigned long get_rate() = 0;
    virtual void add_to_time_past(unsigned long mstime) = 0;
    virtual unsigned long get_time_past() = 0;
    virtual void suspend() = 0;
    virtual void restart() = 0;
};

struct WD_FLAGS_tag {
    int initalized : 1;
    int record : 1;
};

class CTimer;

typedef struct {
    unsigned long environment;
    float fVolume;
    float fDecayTime_sec;
    float fDamping;
} EAX_REVERB_PROPERTIES;

#ifdef _USRDLL
class CMidiMgr;
class CCriticalRequest {
public:
    CCriticalRequest();
    CCriticalRequest(REQUEST_TYPE type, Sound *psound);
    ~CCriticalRequest();
    REQUEST_TYPE m_type;
    Sound *m_psound;
    int m_data1;
    int m_data2;
};

class CCriticalRequestQueue {
public:
    CCriticalRequestQueue();
    ~CCriticalRequestQueue();
    CCriticalRequest *m_queue[0x1000];
    unsigned m_TT_index;
    unsigned m_GT_index;

    CCriticalRequest *operator[](unsigned index);
};
struct SeqNode {
    SeqNode *m_prev;
    SeqNode *m_next;
    CMidiMgr *m_payload;

    SeqNode(CMidiMgr *payload)
    {
        m_prev = 0;
        m_next = 0;
        m_payload = payload;
    }
};

struct MidiMgrList {
    SeqNode *m_head;
    SeqNode *m_tail;
    SeqNode *m_cursor;
    int m_count;

    MidiMgrList()
    {
        m_head = 0;
        m_tail = 0;
        m_cursor = 0;
        m_count = 0;
    }
    ~MidiMgrList()
    {
        CMidiMgr *p = PopFront();
        while (p)
            p = PopFront();
    }
    CMidiMgr *PopFront()
    {
        SeqNode *n = m_head;
        if (!n)
            return 0;
        SeqNode *nx = n->m_next;
        if (nx) {
            nx->m_prev = 0;
            m_head = nx;
        } else {
            m_tail = 0;
            m_head = 0;
        }
        CMidiMgr *payload = n->m_payload;
        delete n;
        m_count--;
        return payload;
    }
    CMidiMgr *First()
    {
        m_cursor = m_head;
        return m_head ? m_head->m_payload : 0;
    }
    CMidiMgr *Next()
    {
        if (!m_cursor)
            return 0;
        m_cursor = m_cursor->m_next;
        return m_cursor ? m_cursor->m_payload : 0;
    }

    void RemoveBySequence(CMidiMgr *v)
    {
        SeqNode *p = m_head;
        while (p && p->m_payload != v)
            p = p->m_next;
        if (p) {
            if (p->m_prev)
                p->m_prev->m_next = p->m_next;
            else
                m_head = p->m_next;
            SeqNode *nx = p->m_next;
            if (nx) {
                nx->m_prev = p->m_prev;
                m_cursor = nx;
            } else {
                m_tail = p->m_prev;
                m_cursor = 0;
            }
            delete p;
            m_count--;
        }
    }

    void AppendObject1(CMidiMgr *seq)
    {
        SeqNode *n = new SeqNode(seq);

        if (m_tail) {
            m_tail->m_next = n;
            n->m_prev = m_tail;
            m_tail = n;
        } else {
            m_head = n;
            m_tail = n;
        }
        m_count++;
    }
};
#endif

class Midi_Device : public Sound_Device {
    WD_FLAGS_tag m_flags;

protected:
    virtual SNDERR create_device(unsigned long flags);
    virtual SNDERR delete_device();

public:
    Midi_Device();
    virtual ~Midi_Device();

    virtual SNDERR init(HWND game_hwnd, unsigned long flags);
    virtual SNDERR release();
    virtual unsigned long get_ndevices();
    virtual SNDERR select(unsigned long id);
    virtual SNDERR get_description(unsigned long id, char *desc_string, unsigned long buffsize);
    virtual void set_volume(unsigned long volume);
    virtual unsigned long get_volume();
    virtual void set_pan(int pan);
    virtual SNDERR fade(unsigned long ms_fade_time);
    virtual SNDERR stop();
    virtual void update_sound();
#ifdef _USRDLL
    virtual void timer_unhook() {}
#else
    virtual void timer_unhook();
#endif
    virtual void set_rate(unsigned long ms);
    virtual unsigned long get_rate();
#ifdef _USRDLL
    virtual unsigned long get_time_past() { return 0; }
#else
    virtual unsigned long get_time_past();
#endif
#ifdef _USRDLL
    virtual void add_to_time_past(unsigned long mstime) {}
#else
    virtual void add_to_time_past(unsigned long mstime);
#endif
#ifdef _USRDLL
    virtual void reset_time_past() {}
#else
    virtual void reset_time_past();
#endif
    virtual void suspend();
    virtual void restart();
    virtual SNDERR enable();
    virtual SNDERR disable();
    virtual int is_disabled();
};

class Dll_Midi_Device;
#ifdef _USRDLL
#include "dllmididevice.h"
#endif

extern Dll_Midi_Device *gbl_pmidi_device;

class Dll_Wave_In_Device;

class CWaveMixer;



extern double g_MixAccumulator[];

extern WAVEFORMATEX g_wfx22050Mono16;

#ifdef _USRDLL
#include "dsmgr.h"
#include "wavemgr.h"
#include "soundlist.h"
#endif



#ifndef _USRDLL
inline SndNodeList::~SndNodeList()
{
    void *p = PopFront();
    while (p)
        p = PopFront();
}

inline SndNodeList::SndNodeList()
{
    m_head = 0;
    m_tail = 0;
    m_cursor = 0;
    m_count = 0;
}

inline void *SoundUnrecoveredAlloc()
{
    return new SndNode(0);
}
#endif

struct WaveControlGroupFlags_tag {
    unsigned char status;
    unsigned char muted;
};

class WaveControlGroup {
public:
    WaveControlGroupFlags_tag m_flags;
    unsigned m_volume;
    bool isMute();
    void mute() { m_flags.muted = 1; }
    void unMute();
    WaveControlGroup();
    SndNodeList m_wave_list;
};
#ifdef _USRDLL
typedef char WaveControlGroup_sizeof_check[sizeof(WaveControlGroup) == 0x18 ? 1 : -1];
#else
typedef char WaveControlGroup_sizeof_check[sizeof(WaveControlGroup) == 0x1c ? 1 : -1];
#endif

class Wave_Device : public Sound_Device {
    WD_FLAGS_tag m_flags;

protected:
    virtual SNDERR create_device(unsigned long flags);
    virtual SNDERR delete_device();

public:
    WaveControlGroup m_group_list[16];

    Wave_Device();
    virtual ~Wave_Device();

    virtual SNDERR init(HWND game_hwnd, unsigned long flags);
    virtual SNDERR release();
    virtual SNDERR start_raw_dump(char *fname);
    virtual SNDERR stop_raw_dump();
    virtual unsigned long get_ndevices();
    virtual SNDERR select(unsigned long id);
    virtual SNDERR get_description(unsigned long id, char *buff, unsigned long buffsize);
    virtual void set_volume(unsigned long volume);
    virtual unsigned long get_volume();
    virtual void set_pan(int pan);
    virtual SNDERR fade(unsigned long ms_fade_time);
    virtual SNDERR stop();
    virtual void update_sound();
#ifdef _USRDLL
    virtual void timer_unhook() {}
#else
    virtual void timer_unhook();
#endif
    virtual void set_rate(unsigned long ms);
    virtual unsigned long get_rate();
    virtual unsigned long get_hw_mem_size();
#ifdef _USRDLL
    virtual unsigned long get_time_past() { return 0; }
#else
    virtual unsigned long get_time_past();
#endif
#ifdef _USRDLL
    virtual void add_to_time_past(unsigned long mstime) {}
#else
    virtual void add_to_time_past(unsigned long mstime);
#endif
#ifdef _USRDLL
    virtual void reset_time_past() {}
#else
    virtual void reset_time_past();
#endif
    virtual void suspend();
    virtual void restart();
    virtual SNDERR enable();
    virtual SNDERR disable();
    virtual int is_disabled();
    virtual SNDERR set_hwnd(HWND hwnd);
    virtual void *get_ds();
    SNDERR set_group_volume(unsigned group, unsigned volume);
    unsigned get_group_volume(unsigned group);
    SNDERR muteGroup(unsigned group);
    SNDERR unMuteGroup(unsigned group);
    SNDERR disable_group(unsigned group);
    SNDERR enable_group(unsigned group);
    SNDERR add_to_group(unsigned group, Sound *pSound);
    SNDERR pull_from_group(Sound *pSound);
    bool is_group_disabled(unsigned group);
    bool isGroupMute(unsigned group);

    virtual bool is_eax();
    virtual SNDERR set_eax(EAX_REVERB_PROPERTIES *peax);
    virtual SNDERR set_eax(unsigned long environment);
    virtual SNDERR set_eax_mix(float mix);
    virtual bool is_3d();
    virtual SNDERR set_listener_position(float x, float y, float z);
    virtual SNDERR get_listener_position(float *px, float *py, float *pz);
    virtual SNDERR set_listener_xpos(float x);
    virtual SNDERR get_listener_xpos(float *px);
    virtual SNDERR set_listener_ypos(float y);
    virtual SNDERR get_listener_ypos(float *py);
    virtual SNDERR set_listener_zpos(float z);
    virtual SNDERR get_listener_zpos(float *pz);
};
#ifdef _USRDLL
typedef char Wave_Device_sizeof_check[sizeof(Wave_Device) == 0x1a4 ? 1 : -1];
#else
typedef char Wave_Device_sizeof_check[sizeof(Wave_Device) == 0x1e4 ? 1 : -1];
#endif

class Dll_Wave_Device;
#ifdef _USRDLL
#include "dllwavedevice.h"
#endif



extern "C" {
#include "lpc10.h"
}

extern real lpc10_encode_buffer[LPC10_SAMPLES_PER_FRAME];
extern INT32 lpc10_bits[LPC10_BITS_IN_COMPRESSED_FRAME];

class WaveInDeviceMgr;

extern CRITICAL_SECTION gbl_timer_cs;

extern Dll_Wave_Device *gbl_pwave_device;

extern Dll_Wave_In_Device *gbl_pwave_in_device;



extern "C" long __cdecl thread_proc(HWND hwnd, unsigned int msg, unsigned int wParam, long lParam);

class Wave_In_Device : public Sound_Device {
    WD_FLAGS_tag m_flags;

protected:
    virtual SNDERR create_device(unsigned long flags);
    virtual SNDERR delete_device();

public:
    Wave_In_Device();
    virtual ~Wave_In_Device();

    virtual SNDERR init(HWND game_hwnd, unsigned long flags);
    virtual SNDERR release();
    virtual unsigned long get_ndevices();
    virtual SNDERR select(unsigned long id);
    virtual SNDERR get_description(unsigned long id, char *desc_string, unsigned long buffsize);
    virtual SNDERR get_caps(unsigned long id);
    virtual SNDERR stop();
    virtual void update_sound();
#ifdef _USRDLL
    virtual void timer_unhook() {}
#else
    virtual void timer_unhook();
#endif
    virtual void set_rate(unsigned long ms);
    virtual unsigned long get_rate();
#ifdef _USRDLL
    virtual unsigned long get_time_past() { return 0; }
#else
    virtual unsigned long get_time_past();
#endif
#ifdef _USRDLL
    virtual void add_to_time_past(unsigned long mstime) {}
#else
    virtual void add_to_time_past(unsigned long mstime);
#endif
#ifdef _USRDLL
    virtual void reset_time_past() {}
#else
    virtual void reset_time_past();
#endif
    virtual void suspend();
    virtual void restart();
    virtual SNDERR start_record();
    virtual SNDERR end_record();
#ifdef _USRDLL
    virtual void set_volume(unsigned long) {}
#else
    virtual void set_volume(unsigned long);
#endif
#ifdef _USRDLL
    virtual unsigned long get_volume() { return 0; }
#else
    virtual unsigned long get_volume();
#endif
#ifdef _USRDLL
    virtual void set_pan(int) {}
#else
    virtual void set_pan(int);
#endif
#ifdef _USRDLL
    virtual SNDERR fade(unsigned long) { return SNDERR_OK; }
#else
    virtual SNDERR fade(unsigned long);
#endif
    virtual SNDERR set_codec(unsigned long codec);
    virtual SNDERR set_vxw_key(signed char *key);
};

#ifdef _USRDLL
#include "dllwaveindevice.h"
#endif

#endif
