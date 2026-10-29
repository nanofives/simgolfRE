#include <windows.h>
#include <string.h>
#include <mmsystem.h>
#include "sound.h"
#include "sounddevice.h"
#include "MidiSequence.h"
#include "waveindevicemgr.h"
#include "sndwin.h"
#include "wavefile.h"
#include "wavemgr.h"
#include "midimgr.h"

extern "C" long __stdcall snd_wnd_proc(HWND hwnd, unsigned int uMsg, unsigned int wParam, long lParam);

const char *CSndWin::m_class_name = "snd_class";
const char *CSndWin::m_wnd_name = "snd_window";
HINSTANCE CSndWin::m_snd_hinst;
WNDCLASSA CSndWin::m_snd_class = {0, (WNDPROC)snd_wnd_proc, 0, 0, 0, 0, 0, 0, 0, CSndWin::get_wnd_class_name()};

CSndWin::CSndWin()
{
    m_hwnd = 0;
    m_game_proc = 0;
    memset(&m_flags, 0, sizeof m_flags);
}

CSndWin::~CSndWin()
{
    if (m_hwnd != 0 && m_flags.initialized) {
        SetWindowLongA(m_hwnd, -4, m_game_proc);
        m_game_proc = 0;
        m_hwnd = 0;
        m_flags.initialized = 0;
    }
}

SNDERR CSndWin::init(HWND game_hwnd)
{
    if (m_hwnd == 0) {
        m_game_proc = SetWindowLongA(game_hwnd, -4, (long)snd_wnd_proc);
        if (m_game_proc == 0)
            return SNDERR_WINAPI_FAILURE;
        m_hwnd = game_hwnd;
        m_flags.initialized = 1;
    }
    return SNDERR_OK;
}

SNDERR CSndWin::create()
{
    if (m_hwnd != 0)
        return SNDERR_OK;
    RegisterClassA(&m_snd_class);
    m_hwnd = CreateWindowExA(0, get_wnd_class_name(), get_wnd_name(), 0, 0, 0, 0, 0, 0, 0, m_snd_hinst, 0);
    return m_hwnd != 0 ? SNDERR_OK : SNDERR_NO_WINDOW;
}

SNDERR CSndWin::destroy()
{
    if (m_hwnd != 0 && m_flags.initialized) {
        SetWindowLongA(m_hwnd, -4, m_game_proc);
        m_game_proc = 0;
        m_hwnd = 0;
        m_flags.initialized = 0;
    }
    return SNDERR_OK;
}

SNDERR CSndWin::clean_message_queue()
{
    MSG msg;
    while (PeekMessageA(&msg, m_hwnd, 0x7e8, 0x7ef, PM_REMOVE))
        snd_wnd_proc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    return SNDERR_OK;
}

extern "C" long __cdecl thread_proc(HWND hwnd, unsigned int uMsg, unsigned int wParam, long lParam)
{
    CWaveFile *pwf;
    CWaveMgr *pwmgr;
    WaveInDeviceMgr *pwid;
    CMidiMgr *pmmgr;
#ifdef USE_MPEG
    MilesMpeg *pmMpeg;
#endif
    CCriticalRequest *pcur_req;
    int i;

    pmmgr = 0;
#ifdef USE_MPEG
    pmMpeg = 0;
#endif
    i = 0;
    switch (uMsg) {

    case WF_ID_DELETE:
        if (sound_win.is_wo_ignored())
            return 0;
        pwmgr = (CWaveMgr *)lParam;
        Sound::delete_sound(pwmgr);
        return 0;

    case WF_ID_ADVANCE:
        if (sound_win.is_wo_ignored())
            return 0;
        pwf = (CWaveFile *)wParam;
        pwf->dec_msgs_pending();
        if (pwf->is_loaded() == 0) {
            pwf->set_advance_pending(0);
            return 0;
        }
        pwf->advance_file();
        pwf->set_advance_pending(0);
        return 0;

    case WI_ID_RESET:
        if (lParam == 0)
            break;
        pwid = (WaveInDeviceMgr *)lParam;
        if (pwid->is_enabled())
            pwid->process_buffer((WIBuffer *)wParam);
        return 0;

    case WD_ID_CRIT_REQ:
        if (sound_win.is_wo_ignored())
            return 0;
        pcur_req = (CCriticalRequest *)lParam;
        pwmgr = (CWaveMgr *)pcur_req->m_psound;
        switch (pcur_req->m_type) {
        case REQUEST_UNLOAD:
            if (pwmgr->is_loaded())
                pwmgr->direct_unload();
            pwmgr->set_GT_request_pending(false);
            if (!pwmgr->is_dynamic_buffer())
                if (pwmgr)
                    if (pwmgr)
                        delete pwmgr;
            delete pcur_req;
            break;
        case REQUEST_DELETE:
            if (pwmgr)
                delete pwmgr;
            delete pcur_req;
            break;
        case REQUEST_RESET:
            pwmgr->reset();
            pwmgr->set_GT_request_pending(false);
            delete pcur_req;
            break;
        }
        return 0;

    case MD_ID_CRIT_REQ:
        if (sound_win.is_mo_ignored())
            return 0;
        pcur_req = (CCriticalRequest *)lParam;
        pmmgr = (CMidiMgr *)pcur_req->m_psound;
        switch (pcur_req->m_type) {
        case REQUEST_UNLOAD:
            pmmgr->set_GT_request_pending(false);
            delete pcur_req;
            if (pmmgr)
                delete pmmgr;
            break;
        case REQUEST_RESET:
            pmmgr->reset();
            pmmgr->set_GT_request_pending(false);
            delete pcur_req;
            break;
        case REQUEST_DELETE:
            if (pmmgr)
                delete pmmgr;
            delete pcur_req;
            break;
        }
        return 0;

    case MF_ID_RESET:
        pmmgr = (CMidiMgr *)lParam;
        pmmgr->reset();
        return 0;

#ifdef USE_MPEG
    case MPEG_ID_DECODE:
        pmMpeg = (MilesMpeg *)lParam;
        pmMpeg->decode_frames(wParam);
        return 0;
#endif

    case MM_WIM_OPEN:
        i = 0;
        break;
    case MM_WIM_CLOSE:
        i = 1;
        break;
    case MM_WIM_DATA:
        i = 2;
        break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

extern "C" long __stdcall snd_wnd_proc(HWND hwnd, unsigned int uMsg, unsigned int wParam, long lParam)
{
    CWaveFile *pwf;
    CWaveMgr *pwmgr;
    WaveInDeviceMgr *pwid;
    CMidiMgr *pmmgr;
#ifdef USE_MPEG
    MilesMpeg *pmMpeg;
#endif
    CCriticalRequest *pcur_req;
    int i;

    pmmgr = 0;
#ifdef USE_MPEG
    pmMpeg = 0;
#endif
    i = 0;
    switch (uMsg) {

    case WF_ID_DELETE:
        if (sound_win.is_wo_ignored())
            return 0;
        pwmgr = (CWaveMgr *)lParam;
        Sound::delete_sound(pwmgr);
        return 0;

    case WF_ID_ADVANCE:
        if (sound_win.is_wo_ignored())
            return 0;
        pwf = (CWaveFile *)wParam;
        pwf->dec_msgs_pending();
        if (pwf->is_loaded() == 0) {
            pwf->set_advance_pending(0);
            return 0;
        }
        pwf->advance_file();
        pwf->set_advance_pending(0);
        return 0;

    case WI_ID_RESET:
        if (lParam == 0)
            break;
        pwid = (WaveInDeviceMgr *)lParam;
        if (pwid->is_enabled())
            pwid->process_buffer((WIBuffer *)wParam);
        return 0;

    case WD_ID_CRIT_REQ:
        if (sound_win.is_wo_ignored())
            return 0;
        pcur_req = (CCriticalRequest *)lParam;
        pwmgr = (CWaveMgr *)pcur_req->m_psound;
        switch (pcur_req->m_type) {
        case REQUEST_UNLOAD:
            if (pwmgr->is_loaded())
                pwmgr->direct_unload();
            pwmgr->set_GT_request_pending(false);
            if (!pwmgr->is_dynamic_buffer())
                if (pwmgr)
                    if (pwmgr)
                        delete pwmgr;
            delete pcur_req;
            break;
        case REQUEST_DELETE:
            if (pwmgr)
                delete pwmgr;
            delete pcur_req;
            break;
        case REQUEST_RESET:
            pwmgr->reset();
            pwmgr->set_GT_request_pending(false);
            delete pcur_req;
            break;
        }
        return 0;

    case MD_ID_CRIT_REQ:
        if (sound_win.is_mo_ignored())
            return 0;
        pcur_req = (CCriticalRequest *)lParam;
        pmmgr = (CMidiMgr *)pcur_req->m_psound;
        switch (pcur_req->m_type) {
        case REQUEST_UNLOAD:
            pmmgr->set_GT_request_pending(false);
            delete pcur_req;
            if (pmmgr)
                delete pmmgr;
            break;
        case REQUEST_RESET:
            pmmgr->reset();
            pmmgr->set_GT_request_pending(false);
            delete pcur_req;
            break;
        case REQUEST_DELETE:
            if (pmmgr)
                delete pmmgr;
            delete pcur_req;
            break;
        }
        return 0;

    case MF_ID_RESET:
        pmmgr = (CMidiMgr *)lParam;
        pmmgr->reset();
        return 0;

#ifdef USE_MPEG
    case MPEG_ID_DECODE:
        pmMpeg = (MilesMpeg *)lParam;
        pmMpeg->decode_frames(wParam);
        return 0;
#endif

    case MM_WIM_OPEN:
        i = 0;
        break;
    case MM_WIM_CLOSE:
        i = 1;
        break;
    case MM_WIM_DATA:
        i = 2;
        break;
    }

    return CallWindowProcA((WNDPROC)sound_win.m_game_proc, hwnd, uMsg, wParam, lParam);
}
