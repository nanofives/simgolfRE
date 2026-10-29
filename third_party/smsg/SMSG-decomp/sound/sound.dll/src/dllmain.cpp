#include "sound.h"
#include "sounddevice.h"
#include "MidiSequence.h"
#include "soundlist.h"
#include "dlist.h"
#include "sndwin.h"
#include "wavefile.h"
#include "ctimer.h"
#include "keymap.h"
#include "midimgr.h"
#include "avimgr.h"

unsigned long gbl_flags = 0;

static unsigned s_reserved[4] = {0};

CTimer *gbl_ptimer = 0;

CDSMgr *Dll_Wave_Device::m_pdsmgr = 0;
CWaveMixer *Dll_Wave_Device::m_pmixer = 0;
CDSB *Dll_Wave_Device::m_pmix_dsb = 0;
CDSB *Dll_Wave_Device::m_p3d_dsb_l = 0;
CDSB *Dll_Wave_Device::m_p3d_dsb_r = 0;
WaveInDeviceMgr *Dll_Wave_In_Device::m_pwidmgr = 0;
int gbl_mmio = 0;

DList<MMIOINFO> gbl_created_list;

CSndWin sound_win;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInst, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        CSndWin::set_snd_hinst(hInst);
        break;
    }
    return TRUE;
}

SNDERR __cdecl init_sound(HWND hwnd, unsigned long flags)
{
    SNDERR retval;
    gbl_flags = flags;
    if ((flags & 1) == 0) {
        gbl_ptimer = new CTimer;
        gbl_ptimer->set_callback(&update_sound);
    }
    retval = sound_win.init(hwnd);
    return retval;
}

SNDERR __cdecl init_sound_timer(unsigned long ms_resolution, unsigned long flags)
{
    gbl_flags = flags;
    if ((flags & 1) == 0) {
        gbl_ptimer = new CTimer;
        gbl_ptimer->set_callback(&update_sound);
    }
    return SNDERR_OK;
}

unsigned long __cdecl get_sound_version(void)
{
    unsigned long version_with_header = 0x05000001;
    version_with_header |= 0x0C00;
    return version_with_header;
}

SNDERR __cdecl release_sound(void)
{
    if (gbl_ptimer) {
        gbl_ptimer->release();

        delete gbl_ptimer;

        gbl_ptimer = 0;
    }
    return SNDERR_OK;
}

SOUNDTYPE __cdecl get_sound_type(const char *fspec);

SNDERR Sound::create_sound(Sound **ppsnd, const char *fspec, SOUNDTYPE sound_type)
{
    if (sound_type == SOUNDTYPE_UNKNOWN)
        sound_type = get_sound_type(fspec);

    switch (sound_type) {

    case SOUNDTYPE_WAVE:
        *ppsnd = new CWaveMgr(sound_win.get_hwnd());
        ((CWaveMgr *)*ppsnd)->set_fname(fspec);
        break;

    case SOUNDTYPE_MIDI:
        if (gbl_pmidi_device == 0)
            return SNDERR_NO_DEVICE;
        *ppsnd = new CMidiMgr;
        ((CMidiMgr *)*ppsnd)->set_fname(fspec);
        break;

    case SOUNDTYPE_AVI:
        *ppsnd = new CAviMgr;
        break;

    case SOUNDTYPE_AMBIENCE:
        if (gbl_pmidi_device == 0)
            return SNDERR_NO_DEVICE;
        *ppsnd = new CAmbienceMgr;
        break;

    case SOUNDTYPE_VOICERX:
        *ppsnd = new CVoiceRxMgr;
        break;

    case SOUNDTYPE_VOICETX:
        *ppsnd = new CVoiceTxMgr;
        break;

    case SOUNDTYPE_SOUNDLIST:
        *ppsnd = new dllSoundList;
        break;

    default:
        return SNDERR_NO_SUPPORT;
    }

    return SNDERR_OK;
}

SNDERR Sound::delete_sound(Sound *psnd)
{
    Dll_KeyMap *pcurKeyMap;
    int i;
    CCriticalRequest *pcReq;
    dllSoundList *psoundlist;
    CWaveMgr *pwmgr;

    if (!psnd)
        return SNDERR_OK;

    if (psnd->is_wave()) {
        if (gbl_pwave_device && gbl_ptimer && gbl_ptimer->is_active())
            gbl_pwave_device->queue_request(psnd, REQUEST_DELETE);
        else
            delete psnd;
    } else if (psnd->is_ambience()) {
        if (gbl_pmidi_device && gbl_ptimer && gbl_ptimer->is_active()) {
            for (i = 0; i < 128; i++) {
                if (((CMidiMgr *)psnd)->m_patch_table[i]) {
                    pcurKeyMap = ((CMidiMgr *)psnd)->m_patch_table[i]->get_keymap();
                    if (pcurKeyMap)
                        pcurKeyMap->dec_ref();
                }
            }
            gbl_pmidi_device->queue_request((CMidiMgr *)psnd, REQUEST_DELETE, 0, 0);
        } else {
            delete psnd;
        }
    } else if (psnd->get_type() & 8) {
        if (gbl_pwave_device && gbl_ptimer && gbl_ptimer->is_active()) {
            pcReq = new CCriticalRequest(REQUEST_DELETE_SOUNDLIST, psnd);
            gbl_pwave_device->queue_request(pcReq);
        } else {
            psoundlist = (dllSoundList *)psnd;
            pwmgr = psoundlist->getCurSound();
            if (pwmgr && pwmgr->is_active() && gbl_pwave_device) {
                gbl_pwave_device->pull_from_play_list(pwmgr);
                pwmgr->set_done();
                pwmgr->set_active(0);
                delete pwmgr;
            }
            delete psnd;
        }
    }
    return SNDERR_OK;
}


SOUNDTYPE __cdecl get_sound_type(const char *fspec)
{
    if (fspec == 0)
        return SOUNDTYPE_WAVE;
    if (strstr(fspec, "wav"))
        return SOUNDTYPE_WAVE;
    if (strstr(fspec, "mid"))
        return SOUNDTYPE_MIDI;
    if (strstr(fspec, "aif"))
        return SOUNDTYPE_AIF;
    return strstr(fspec, "rmi") ? SOUNDTYPE_MIDI : SOUNDTYPE_UNKNOWN;
}

extern "C" void update_sound(void)
{
}
