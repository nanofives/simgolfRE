#ifndef SOUND_H
#define SOUND_H

#include <windows.h>
#include <mmsystem.h>
#include <avifmt.h>
#include <dsound.h>

#ifdef _USRDLL
#include "dlist.h"
#endif

extern "C" char *__cdecl strcpy(char *, const char *);

extern "C" unsigned int __cdecl strlen(const char *s);
extern "C" char *__cdecl strcpy(char *dst, const char *src);
extern "C" char *__cdecl strstr(const char *s, const char *sub);

enum SNDERR {
    SNDERR_OK = 0x00,
    SNDERR_NO_DRIVER = 0x01,
    SNDERR_NOT_CREATED = 0x02,
    SNDERR_NO_DEVICE = 0x03,
    SNDERR_NO_DLL = 0x04,
    SNDERR_NO_SOUND_THREAD = 0x05,
    SNDERR_NOCANDO = 0x06,
    SNDERR_BAD_DEVICE_OPEN = 0x07,
    SNDERR_BAD_FILE_OPEN = 0x08,
    SNDERR_BAD_FILE_CLOSE = 0x09,
    SNDERR_BAD_PARAM = 0x0a,
    SNDERR_NO_SUPPORT = 0x0b,
    SNDERR_ALREADY_OPEN = 0x0c,
    SNDERR_ALREADY_ACTIVE = 0x0d,
    SNDERR_ALREADY_PLAYING = 0x0e,
    SNDERR_ALREADY_LOADED = 0x0f,
    SNDERR_ALLOCATED = 0x10,
    SNDERR_NO_MEM = 0x11,
    SNDERR_BAD_FORMAT = 0x12,
    SNDERR_UNINITIALIZED = 0x13,
    SNDERR_NOT_LOADED = 0x14,
    SNDERR_NOT_ACTIVE = 0x15,
    SNDERR_NO_CLASS = 0x16,
    SNDERR_NO_WINDOW = 0x17,
    SNDERR_WINAPI_FAILURE = 0x18,
    SNDERR_BAD_DSB_WRITE = 0x19,
    SNDERR_BAD_DSB_LOCK = 0x1a,
    SNDERR_BAD_DSB_UNLOCK = 0x1b,
    SNDERR_BAD_DSB_PLAY = 0x1c,
    SNDERR_DSB_CREATE = 0x1d,
    SNDERR_DSB_LOST = 0x1e,
    SNDERR_DSBAPI_FAILURE = 0x1f,
    SNDERR_BAD_COM_INIT = 0x20,
    SNDERR_WRONG_TYPE = 0x21,
    SNDERR_BUFFER_OVERRUN = 0x22,
    SNDERR_MMIO_FAILURE = 0x23,
    SNDERR_NOT_PRESENT = 0x24,
    SNDERR_NO_RESOURCES = 0x25,
    SNDERR_NO_DATA = 0x26,
    SNDERR_OUT_OF_SYNC = 0x27,
    SNDERR_BUSY = 0x28,
    SNDERR_END_OF_TRACK = 0x29,
    SNDERR_END_OF_FILE = 0x2a,
    SNDERR_END_OF_BUFFER = 0x2b,
    SNDERR_BAD_DECODER_INIT = 0x2c,
    SNDERR_RESET = 0x2d,
    SNDERR_MISSING_MSS_REDIST = 0x2e,
    SNDERR_UNDEFINED = 0x2f
};

struct DSDevice_tag {
    char *description;
    char *module;
    GUID *lpguid;
    void *lpcontext;
    DSDevice_tag *pnext;
};

enum REQUEST_TYPE {
    REQUEST_NULL,
    REQUEST_PLAY,
    REQUEST_STOP,
    REQUEST_UNLOAD,
    REQUEST_DELETE,
    REQUEST_RESET,
    REQUEST_FADE,
    REQUEST_SET_ACTIVE_TRACKS,
    REQUEST_PLAY_TRACKSET,
    REQUEST_STOP_TRACKSET,
    REQUEST_XPOSE_TRACKSET,
    REQUEST_SET_TSDONE_CALLBACK,
    REQUEST_SET_TRACK_LOOP_STATE,
    REQUEST_SET_LINK,
    REQUEST_SET_SOUNDLIST_RAMP,
    REQUEST_SET_SOUNDLIST_VOL,
    REQUEST_SOUNDLIST_FADE,
    REQUEST_DELETE_SOUNDLIST
};

struct SndNode {
    SndNode *m_prev;
    SndNode *m_next;
    void *m_payload;

    SndNode(void *payload)
    {
        m_prev = 0;
        m_next = 0;
        m_payload = payload;
    }
};

struct SndNodeList {
    SndNode *m_head;
    SndNode *m_tail;
    SndNode *m_cursor;

    int m_count;

#ifdef _USRDLL
    SndNodeList()
    {
        m_head = 0;
        m_tail = 0;
        m_cursor = 0;
        m_count = 0;
    }
#else
    SndNodeList();
#endif

    void *First()
    {
        SndNode *n = m_head;
        m_cursor = n;
        if (!n)
            return 0;
        return n->m_payload;
    }

    void *Next()
    {
        SndNode *n = m_cursor;
        if (!n)
            return 0;
        n = n->m_next;
        m_cursor = n;
        return n ? n->m_payload : 0;
    }

    void Add(void *p)
    {
        SndNode *n = new SndNode(p);
        if (m_tail) {
            m_tail->m_next = n;
            n->m_prev = m_tail;
        } else {
            m_head = n;
        }
        m_tail = n;
        m_count++;
    }
    int Append(void *payload);
    int Prepend(void *payload);

#ifdef _USRDLL
    ~SndNodeList();
#else
    virtual ~SndNodeList();
#endif

    void *PopFront()
    {
        SndNode *n = m_head;
        if (!n)
            return 0;

        SndNode *nx = n->m_next;
        if (nx) {
            nx->m_prev = 0;
            m_head = nx;
        } else {
            m_tail = 0;
            m_head = 0;
        }

        void *payload = n->m_payload;
        delete n;
        m_count--;
        return payload;
    }

#ifdef _USRDLL
    void *Remove(void *payload);
#else
    void *Remove(void *payload)
    {
        SndNode *p = m_head;
        while (p && p->m_payload != payload)
            p = p->m_next;
        if (p) {
            if (p->m_prev)
                p->m_prev->m_next = p->m_next;
            else
                m_head = p->m_next;
            SndNode *nx = p->m_next;
            if (nx) {
                nx->m_prev = p->m_prev;
                m_cursor = nx;
            } else {
                m_tail = p->m_prev;
                m_cursor = 0;
            }
            void *v = p->m_payload;
            delete p;
            m_count--;
            return v;
        }
        return 0;
    }
#endif
};

#ifndef _USRDLL
inline int SoundUnrecoveredPad(int a, int b)
{
    if (!a)
        return 0;
    int c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c)
        c = a;
    c++;
    c += b;
    return c + b;
}
#endif

class Wave;

class CDSB;
class WaveBuffer;

#ifdef _USRDLL
#include "sndwin.h"
#endif

enum SOUNDID {
    WF_ID_RESET = WM_USER + 1000,
    WF_ID_UNLOAD,
    WF_ID_DELETE,
    WF_ID_ADVANCE,
    MF_ID_RESET,
    MF_ID_UNLOAD,
    WD_ID_CRIT_REQ,
    MD_ID_CRIT_REQ,
    WI_ID_RESET,
    WI_ID_OPEN,
    WI_ID_CLOSE,
    MPEG_ID_DECODE
};


struct STFlags_tag {
    unsigned char state : 1;
};

class SoundThread {
    static void fn_sound_thread(void *ch);
    friend class WaveInDeviceMgr;

    STFlags_tag m_flags;
    CRITICAL_SECTION m_cs;
    unsigned long m_id;
    void *m_handle;
    void *m_event_handle;

public:
    SoundThread();
    ~SoundThread();
    void stop();
    void start();
    SNDERR post_message(unsigned int msg, long wparam, long lparam);
    unsigned char is_running();
};

extern SoundThread gbl_sound_thread;

unsigned long __cdecl fp_mov_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);
unsigned long __cdecl fp_add_16216s2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);
unsigned long __cdecl fp_mov_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);
unsigned long __cdecl fp_add_8216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);
unsigned long __cdecl fp_mov_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);
unsigned long __cdecl fp_add_8k16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);
typedef unsigned long(__cdecl *PFNFILTER)(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
extern const PFNFILTER filter_table[];
#ifdef _USRDLL
extern const PFNFILTER mmx_filter_table[];
unsigned long __cdecl mov_8k222k16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_8k222k16216m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
#endif

unsigned long __cdecl add_16216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_16216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_828s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_828s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_828m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_828m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_8216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl add_8216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_16216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_16216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_828s2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_828s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_828m2smb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_828m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_8216s2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);
unsigned long __cdecl mov_8216m2mmb(void **ppdst, void **ppsrc, int nsamples, void *pwf, unsigned long flags);

unsigned long __cdecl fp_mov_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);
unsigned long __cdecl fp_add_16216m2smb(double *pdst, void **ppsrc, int nsamples, void *pcaller, unsigned long flags);

extern int gbl_mmio;

class Sound;

struct IDirectSoundBuffer;
struct IDirectSound3DBuffer;
struct IAVIStream;
struct IDirectSound3DListener;
class Dll_Wave_Device;

class CWaveFile;
class CDSMgr;

class CVoiceTxMgr;
class CACMgr;
class CACStream;

#ifdef _USRDLL
#include "dsb.h"
#endif


class CWaveMgr;
enum SOUNDTYPE {
    SOUNDTYPE_UNKNOWN,
    SOUNDTYPE_WAVE,
    SOUNDTYPE_MIDI,
    SOUNDTYPE_AIF,
    SOUNDTYPE_AVI,
    SOUNDTYPE_AMBIENCE,
    SOUNDTYPE_VOICERX,
    SOUNDTYPE_VOICETX,
    SOUNDTYPE_SOUNDLIST,
    MAX_SOUND_TYPES
};

enum SOUNDFUNCS {
    CREATESOUND,
    DELETESOUND,
    INITSOUNDTIMER,
    RELEASESOUND,
    CREATEWAVEDEVICE,
    DELETEWAVEDEVICE,
    CREATEMIDIDEVICE,
    DELETEMIDIDEVICE,
    CREATEWAVEINDEVICE,
    DELETEWAVEINDEVICE,
    GETSOUNDVERSION,
    MAX_SOUND_FUNCS
};

class Sound_Device;

typedef SNDERR (*PFNCREATESOUND)(Sound **, const char *, SOUNDTYPE);
typedef SNDERR (*PFNDELETESOUND)(Sound *);
typedef SNDERR (*PFNINITSOUNDTIMER)(unsigned long, unsigned long);
typedef SNDERR (*PFNRELEASESOUND)();
typedef SNDERR (*PFNCREATEWAVEDEVICE)(Sound_Device **, unsigned long);
typedef SNDERR (*PFNDELETEWAVEDEVICE)();
typedef SNDERR (*PFNCREATEMIDIDEVICE)(Sound_Device **, unsigned long);
typedef SNDERR (*PFNDELETEMIDIDEVICE)();
typedef SNDERR (*PFNCREATEWAVEINDEVICE)(Sound_Device **, unsigned long);
typedef SNDERR (*PFNDELETEWAVEINDEVICE)();
typedef unsigned long (*PFNGETSOUNDVERSION)();
typedef void (*PFNUPDATESOUND)();

extern "C" {
    extern int g_snd_system;
}

typedef struct ADSR_tag {
    unsigned long ms_attack;
    unsigned long vol_attack_begin;
    unsigned long ms_sustain;
    unsigned long vol_sustain_begin;
    unsigned long ms_decay;
    unsigned long vol_decay_begin;
    unsigned long ms_release;
    unsigned long vol_release_begin;
    unsigned long vol_release_end;
} ADSR;

class Patch {
    unsigned long m_volume;

    int m_pan;
    ADSR m_adsr;
    int m_loop_state;
    unsigned long m_delay;

protected:
    int m_fade_amount;

public:
    Patch();
    virtual ~Patch();
    void set_volume(int vol) { m_volume = vol & 0x7f; }
    void set_pan(int pan) { m_pan = pan; }
    void set_delay(unsigned long delay) { m_delay = delay; }
    int get_pan() { return m_pan; }
    int get_volume() { return m_volume; }
    unsigned long get_delay();
    void set_loop_state(int state) { m_loop_state = state; }
    virtual SNDERR set_fade(unsigned long fade_amount)
    {
        if (fade_amount == 0)
            return SNDERR_BAD_PARAM;

        m_fade_amount = fade_amount;
        return SNDERR_OK;
    }
    virtual SNDERR set_fade_in(int fade_amount)
    {
        if (fade_amount == 0)
            return SNDERR_BAD_PARAM;

        m_fade_amount = fade_amount;
        return SNDERR_OK;
    }
    unsigned long get_fade_amount() { return m_fade_amount; }
    int get_loop_state() { return m_loop_state; }
};

typedef struct SoundFlags_tag {
    int loaded : 1;
    int attached : 1;
    int wave : 1;
    int midi : 1;
    int avi : 1;
    int ambience : 1;
    int redbook : 1;
    int vtx : 1;
    int vrx : 1;
    int directAttach : 1;
} SOUNDFLAGS;

class Sound : public Patch {
    friend class Wave_Device;

public:
    unsigned int m_group;

    Sound();
    Sound(char *fname);
    virtual ~Sound();
    virtual SNDERR load(const char *fname);
    virtual SNDERR load(IAVIStream *paviaudio);
    virtual SNDERR unload();
    virtual SNDERR play();
    virtual SNDERR play(unsigned long num);
    virtual SNDERR stop();
    virtual SNDERR fade();
    virtual SNDERR fade(unsigned long ms_fade);
    virtual SNDERR fade_in();
    virtual SNDERR fade_in(unsigned int ms_fade);
    virtual void ramp(int dst_vol, int src_vol, unsigned int ms_time);
    virtual SNDERR release();
    virtual HWND get_game_hwnd() = 0;
    virtual void set_volume(int vol);
    virtual void set_pan(int pan);
    virtual void set_loop_state(int state);
    virtual void set_delay(unsigned long delay);
    virtual void set_type(SOUNDTYPE type);
    virtual SNDERR set_fade(unsigned long ms_fade);
    virtual SNDERR set_fade_in(unsigned int ms_fade);
    virtual int is_looping();
    virtual int is_playing();
    virtual int is_loaded();
    int is_attached() { return m_sf.attached; }
    static SNDERR unloadall();
    static SNDERR fadeall();
    static Sound *get_list() { return gbl_sound_head; }
    virtual Sound *get_next();
    virtual Sound *get_prev();
    virtual SNDERR set_attrib(unsigned long attrib) = 0;
    virtual unsigned long get_attrib() = 0;

    virtual unsigned long get_time();
    virtual SOUNDTYPE get_type();
    int is_wave() { return m_sf.wave; }
    int is_avi();
    int is_ambience() { return m_sf.ambience; }
    int is_vtx();
    int is_vrx() { return m_sf.vrx; }
    char *get_fname() { return m_fname; }
    static SNDERR create_sound(Sound **ppsnd, const char *fspec, SOUNDTYPE sound_type);
    static SNDERR delete_sound(Sound *psnd);
    static SNDERR attach(Sound *psound);

protected:
    virtual SNDERR attach();

public:
    static SNDERR detach(Sound *psound);
    virtual SNDERR detach();
    Sound *m_pdllsnd;
    SOUNDFLAGS m_sf;

protected:
    static Sound *gbl_sound_head;
    static Sound *gbl_sound_tail;
    Sound *m_pprev;
    Sound *m_pnext;

public:
    char *m_fname;

protected:
    void set_prev(Sound *psnd) { m_pprev = psnd; }
    void set_next(Sound *psnd) { m_pnext = psnd; }

private:
    SOUNDTYPE m_type;
};

class CDSMgr;

class VoiceRx : public Sound {
public:
    VoiceRx();
    virtual ~VoiceRx();
    virtual SNDERR init(unsigned long packet_size, WAVEFORMATEX *lpwfx, unsigned long flags);
    virtual SNDERR release();
    virtual SNDERR unload();
    virtual SNDERR start();
    virtual SNDERR stop();
    virtual SNDERR add_buffer(MMIOINFO *pmmioinfo);
    virtual unsigned long get_buffer_size();
#ifdef _USRDLL
    virtual HWND get_game_hwnd() { return 0; }
    virtual SNDERR set_attrib(unsigned long attrib) { return SNDERR_NO_SUPPORT; }
    virtual unsigned long get_attrib() { return 0; }
    virtual unsigned long get_nbuffers()
    {
        if (m_pdllsnd)
            return ((VoiceRx *)m_pdllsnd)->get_nbuffers();
        return 0;
    }
#else
    virtual HWND get_game_hwnd();
    virtual SNDERR set_attrib(unsigned long attrib);
    virtual unsigned long get_attrib();
    virtual unsigned long get_nbuffers();
#endif
};

typedef struct VTXFlags_tag {
    int recording : 1;
} VTXFLAGS;

class VoiceTx : public Sound {
    VTXFLAGS m_flags;

public:
    VoiceTx();
    virtual ~VoiceTx();
    virtual SNDERR init(unsigned long buffer_size, WAVEFORMATEX *lpwfx, unsigned long flags);
    virtual SNDERR release();
    virtual SNDERR unload();
    virtual MMIOINFO *get_next_buffer();
    virtual SNDERR return_buffer(MMIOINFO *pbuffer);
    virtual SNDERR start();
    virtual SNDERR stop();
    virtual unsigned long get_nbuffers();
    int is_recording();

#ifdef _USRDLL
    virtual HWND get_game_hwnd() { return 0; }
    virtual SNDERR set_attrib(unsigned long attrib) { return SNDERR_NO_SUPPORT; }
    virtual unsigned long get_attrib() { return 0; }
#else
    virtual HWND get_game_hwnd();
    virtual SNDERR set_attrib(unsigned long attrib);
    virtual unsigned long get_attrib();
#endif
};

#ifndef _USRDLL
struct HmmioQueue {
    SndNode *m_head;
    SndNode *m_tail;
    SndNode *m_cursor;
    union {
        int m_count;
        unsigned m_countU;
    };

    HmmioQueue()
    {
        m_head = 0;
        m_tail = 0;
        m_cursor = 0;
        m_count = 0;
    }
    ~HmmioQueue()
    {
        void *p = PopFront();
        while (p)
            p = PopFront();
    }
    void *PopFront()
    {
        SndNode *n = m_head;
        if (!n)
            return 0;

        SndNode *nx = n->m_next;
        if (nx) {
            nx->m_prev = 0;
            m_head = nx;
        } else {
            m_tail = 0;
            m_head = 0;
        }

        void *payload = n->m_payload;
        delete n;
        m_count--;
        return payload;
    }
    void Append(void *p);
};
#endif

struct CVTXM_tag {
    int use_voxware : 1;
    int use_acm : 1;
    int codec_initialized : 1;
};

class CVoiceTxMgr : public VoiceTx {
#ifdef _USRDLL
    DList<HMMIO> m_buffers;
#else
    HmmioQueue m_buffers;
#endif
    unsigned long m_buffer_size;
    MMIOINFO *m_pbuffer_out;
    CACMgr *m_pacm;
    CACStream *m_pacstream;
    CVTXM_tag m_flags;
    int m_buffers_processed;
    int m_buffers_taken;
    int m_buffers_returned;
    int m_start_id;
    struct lpc10_encoder_state *m_lpc10_state;
    void s2f_and_scale(float *fdst, short *isrc, int nsamples);
    void lpc_write_bits(char *dstbuff, int *bits, int len);

public:
    virtual SNDERR start();
    CVoiceTxMgr();
    virtual ~CVoiceTxMgr();
    virtual SNDERR release();
    virtual SNDERR unload();
    virtual SNDERR stop();
    virtual SNDERR init(unsigned long buffer_size, WAVEFORMATEX *lpwfx, unsigned long flags);
    SNDERR add_buffer(WAVEHDR *pwhdr);
    virtual MMIOINFO *get_next_buffer();
    virtual SNDERR return_buffer(MMIOINFO *pmmioinfo);
    virtual unsigned long get_nbuffers();
};

class AviSound : public Sound {
public:
    AviSound();
    virtual ~AviSound();
    virtual SNDERR init(AVIStreamHeader *paudheader, AVIStreamHeader *pvidheader, WAVEFORMATEX *lpwfx,
                        unsigned long flags);
    virtual SNDERR release();
#ifdef _USRDLL
    virtual SNDERR unload() { return SNDERR_OK; }
#else
    virtual SNDERR unload();
#endif
    virtual SNDERR start(unsigned long start_frame);
    virtual SNDERR stop();
    virtual MMIOINFO *get_frame(unsigned long size);
    virtual SNDERR return_frame(MMIOINFO *pmmioinfo);
    virtual int get_cur_frame();
    virtual unsigned long get_frame_size();
    virtual unsigned long get_time();
#ifdef _USRDLL
    virtual HWND get_game_hwnd() { return 0; }
    virtual SNDERR set_attrib(unsigned long attrib) { return SNDERR_NO_SUPPORT; }
    virtual unsigned long get_attrib() { return 0; }
#else
    virtual HWND get_game_hwnd();
    virtual SNDERR set_attrib(unsigned long attrib);
    virtual unsigned long get_attrib();
#endif
};


class DllTrackSet;
class TrackSet;
class MidiCtrl;

class Wave : public Sound {
public:
    union {
        unsigned long m_stateBits;
        unsigned char m_stateBitsB;
        struct {
            unsigned int static_buffer : 1;
            unsigned int dynamic_buffer : 1;
            unsigned int try_hw : 1;
            unsigned int buffer3d : 1;
            unsigned int discard : 1;
            unsigned int try_hwstream : 1;
        };
    };

private:
    int m_pitch;
    float m_reverb_mix;
    unsigned m_ms_length;
    unsigned m_play_time;
    SNDERR dyna_load(char *fname);

    virtual SNDERR reload();

public:
    Wave();
    virtual ~Wave();
    virtual void init(char *fname, unsigned long flags);
    virtual SNDERR load(const char *fname);
    virtual SNDERR load(char *fname, unsigned long flags);
    virtual SNDERR load();
    virtual SNDERR unload();
    virtual SNDERR play();
    virtual SNDERR play(int ibuffer);
    virtual SNDERR play(int *pindex);
    virtual SNDERR set_pitch(int pitch);
    virtual void set_volume(int volume);
    virtual SNDERR set_attrib(unsigned long attrib);
    virtual unsigned long get_attrib();
    virtual HWND get_game_hwnd();
    int get_pitch() { return m_pitch; }
    virtual SNDERR set_bufflimit(unsigned long buffsize);
    virtual SNDERR set_asdr();
    virtual SNDERR set_attack(unsigned long mstime, unsigned long level1, unsigned long level2);
    virtual SNDERR set_sustain(unsigned long mstime, unsigned long level1, unsigned long level2);
    virtual SNDERR set_decay(unsigned long mstime, unsigned long level1, unsigned long level2);
    virtual SNDERR set_release(unsigned long mstime, unsigned long level1, unsigned long level2);
    virtual unsigned long get_time(unsigned long instance);
    virtual unsigned long get_current_marker();
    virtual int get_ndevices();
    virtual SNDERR get_device_description(char *desc, int device_id, int buffsize);
    virtual unsigned int get_ms_length();
    virtual int is_playing();
    SNDERR select_device(int id);
    virtual int is_hwbuffer();
    virtual void stop_instance(int ibuffer);
    virtual void fade_instance(int ibuffer);
    virtual SNDERR set_position3d(float x, float y, float z);
    virtual SNDERR set_xpos(float x);
    virtual SNDERR set_ypos(float y);
    virtual SNDERR set_zpos(float z);
    virtual bool is_3d();
    virtual SNDERR set_reverb_mix(float mix);
    int is_dynamic_buffer() { return dynamic_buffer; }
    bool is_discard() { return discard; }
#ifdef _USRDLL
    virtual int is_loaded()
    {
        if (m_pdllsnd)
            return m_pdllsnd->is_loaded();
        return 0;
    }
#else
    virtual int is_loaded();
#endif
    SNDERR set_fname(const char *fname);
    virtual SNDERR set_link(char *fname, unsigned long *psemaphore);
};


class SoundList : public Sound {
    char *m_soundList[40];

public:
    SoundList();
    virtual ~SoundList();
    virtual SNDERR queueSound(unsigned idx) = 0;
    virtual SNDERR play() = 0;
    virtual SNDERR stop();
    virtual bool isAlreadyQueued() = 0;
    virtual int is_playing() = 0;
    virtual void ramp(int dst_vol, int src_vol, unsigned int ms_time) = 0;
    virtual SNDERR fade() = 0;
    virtual void set_volume(int vol) = 0;
    void scaleVolumeToGroup(int volume);
    virtual void set_scaled_volume(int v) = 0;

    SNDERR add(char *fname);
    SNDERR clear();

    virtual SNDERR attach();
    virtual SNDERR detach();
    char *getSoundName(unsigned int iPlay) { return iPlay < 40 ? m_soundList[iPlay] : 0; }
    SNDERR setListName(char *pname);
};

class KeyMapWave {
    unsigned long m_keyhi;
    unsigned long m_keylo;
    unsigned long m_nvoices;
    char m_fname[0x104];

public:
    KeyMapWave();

    SNDERR set_keyhi(unsigned long keyhi)
    {
        if (m_keylo > keyhi)
            m_keylo = keyhi;
        m_keyhi = keyhi;
        return SNDERR_OK;
    }
    SNDERR set_keylo(unsigned long keylo)
    {
        if (m_keyhi < keylo)
            m_keyhi = keylo;
        m_keylo = keylo;
        return SNDERR_OK;
    }
    unsigned long get_keyhi() { return m_keyhi; }
    unsigned long get_keylo() { return m_keylo; }
    unsigned long get_nvoices() { return m_nvoices; }
    void set_nvoices(unsigned int nvoices) { m_nvoices = nvoices; }
    SNDERR set_fname(const char *fname)
    {
        strcpy(m_fname, fname);
        return SNDERR_OK;
    }
    const char *get_fname() { return m_fname; }
};

typedef struct KMFLAGS_tag {
    int cycle : 1;
    unsigned int version : 2;
    int ignore_velocity : 1;
} KMFLAGS;

class KeyMap {
    KeyMapWave *m_kmw_table[128];
    char *m_name;

public:
    KMFLAGS m_flags;
    unsigned long m_cycle_hi;
    unsigned long m_cycle_lo;
    unsigned long m_nwaves;
    unsigned long m_release_time;

    KeyMap();
    virtual ~KeyMap();
    KeyMapWave *get_key_wave(unsigned long key) { return m_kmw_table[key]; }
    void map_range(KeyMapWave *pkmw);
    unsigned long unmap_range(KeyMapWave *pkmw);
    void set_name(const char *name);
    const char *get_name();
    void set_flags(KMFLAGS flags);
    void set_base_path(const char *p);
};

class CMidiTrack;
typedef struct Midi_Flags_tag {
    unsigned char local_waves : 1;
    unsigned char sync_to_flic : 1;
} MIDIFLAGS;

class Midi : public Sound {
    MIDIFLAGS m_flags;
    unsigned int m_frame_delay;

public:
    virtual SNDERR set_tempo(unsigned long tempo);
    virtual SNDERR mute_track(unsigned long tracknum);
    virtual SNDERR unmute_track(unsigned long tracknum);
    virtual SNDERR set_active_tracks(unsigned long lo, unsigned long hi);
    virtual SNDERR set_active_tracks(unsigned int index);
    virtual SNDERR add_active_trackset(TrackSet *pts);
    virtual TrackSet *get_trackset(unsigned int set_id);
    virtual SNDERR set_active_range_lo(unsigned int setid, unsigned int lo);
    virtual SNDERR set_active_range_hi(unsigned int setid, unsigned int hi);
    virtual SNDERR remove_active_trackset(unsigned int setid);
    virtual SNDERR set_track_loop_state(unsigned int track, bool state);
    virtual bool get_track_loop_state(unsigned int track);
    virtual unsigned long get_ntracks();
    virtual SNDERR reset();
    Midi();
    virtual ~Midi();
    virtual void init(const char *fname, unsigned long flags);
    virtual SNDERR load(const char *fname);
    virtual SNDERR load(const char *fname, unsigned int flags);
    virtual SNDERR load();
    virtual SNDERR unload();
    virtual SNDERR play();
    virtual unsigned long get_time(unsigned long instance);
#ifdef _USRDLL
    virtual HWND get_game_hwnd() { return 0; }
    virtual SNDERR set_attrib(unsigned long flags) { return SNDERR_NO_SUPPORT; }
    virtual unsigned long get_attrib() { return 0; }
#else
    virtual HWND get_game_hwnd();
    virtual SNDERR set_attrib(unsigned long flags);
    virtual unsigned long get_attrib();
#endif
    virtual SNDERR map_patch(char *fname, unsigned long number, unsigned long midi_channel);
    virtual SNDERR map_patch(char *fname, unsigned long number);
    virtual SNDERR clear_patch(unsigned long number);
    virtual void set_volume(int volume);
    virtual SNDERR add_keymap(KeyMap *pkm);
    SNDERR set_fname(const char *fname);
    virtual void switch_state(bool state);
    virtual SNDERR set_switch_type(unsigned long switch_type);
    virtual SNDERR set_nswitch_threads(unsigned long nswitch_threads);
    virtual SNDERR add_switch_range(unsigned long tracklo, unsigned long trackhi);
    virtual SNDERR play_trackset(unsigned int setid);
    virtual SNDERR stop_trackset(unsigned int setid);
    virtual SNDERR xpose_trackset(unsigned int setid, int pitch);
    virtual bool is_trackset_playing(unsigned int setid);
    virtual void set_play_type(unsigned int play_type);
    virtual SNDERR load_patch(unsigned int patch_num);
    virtual SNDERR unload_patch(unsigned int patch_num);
    virtual SNDERR set_patch(unsigned int midi_channel, unsigned int patch_num);
    SNDERR set_load_type(bool blocal);
    bool is_local_load() { return m_flags.local_waves; }
    virtual void set_base_path(const char *p);
    virtual SNDERR set_track(unsigned char *pdata, unsigned long tracknum);
    virtual SNDERR set_control(void *pcontrol, int target_type, int target);
    virtual void *get_control_target(int target_type, int target);
    virtual unsigned long get_control_track();
    virtual unsigned long get_total_track_ticks(unsigned long track);
    virtual SNDERR set_track_velocity_offset(unsigned int track, int velocity_offset);
    virtual SNDERR get_track_velocity_offset(unsigned int track, int *pvelocity_offset);
    virtual int get_key();
    int get_tonal_center();
    virtual void set_frame(unsigned int frame);
    void set_sync_to_flic() { m_flags.sync_to_flic = 1; }
    virtual unsigned get_frame();
    bool is_sync_to_flic() { return m_flags.sync_to_flic; }
    void set_frame_delay(unsigned int msdelay);
    unsigned int get_frame_delay() { return m_frame_delay; }
};

#ifdef _USRDLL
#include "wavefile.h"
#endif

#ifdef _USRDLL
#include "voicerxmgr.h"
#endif

class Ambience : public Midi {
public:
    Ambience();
    virtual SNDERR load(const char *fname);
    virtual SNDERR load(const char *fname, unsigned int flags);
    virtual SNDERR unload();
};

class MpegStream : public Wave {
    char *m_mpeg_list[40];
    unsigned long m_link_semaphore;

public:
    MpegStream();
    virtual ~MpegStream();
    SNDERR set_next(unsigned int play_index);
    SNDERR start(unsigned int play_index);
    SNDERR add_to_playlist(char *fname);
    unsigned long getLinkedSemaphore();
    SNDERR clear_playlist();
};


enum SoundUnrecovered {
    SOUND_UNRECOVERED_1,
    SOUND_UNRECOVERED_2,
    SOUND_UNRECOVERED_3,
    SOUND_UNRECOVERED_4,
    SOUND_UNRECOVERED_5
};

#endif
