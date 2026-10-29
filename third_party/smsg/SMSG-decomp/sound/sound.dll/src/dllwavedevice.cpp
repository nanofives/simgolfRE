#include "sounddevice.h"
#include "sound.h"
#include "soundlist.h"
#include "dlist.h"
#include "sndwin.h"
#include "dsb.h"
#include "dsmgr.h"
#include "wavefile.h"
#include "wavemgr.h"
#include "cmix.h"
#include "dllwavedevice.h"
#include "voicerxmgr.h"
#include "ctimer.h"
Dll_Wave_Device *gbl_pwave_device = 0;

WAVEFORMATEX g_wfx22050Mono16 = {WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};

WAVEFORMATEX g_wfx44100Stereo16 = {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};

SNDERR __cdecl Dll_Wave_Device::create_device(Wave_Device **ppdev, unsigned long flags)
{
    if (gbl_pwave_device == 0) {
        gbl_pwave_device = new Dll_Wave_Device;
        *ppdev = gbl_pwave_device;
        return SNDERR_OK;
    }
    return SNDERR_ALREADY_OPEN;
}

SNDERR Dll_Wave_Device::start_raw_dump(char *fname)
{
    if (m_pmixer)
        return m_pmixer->open_dump_file(fname);
    return SNDERR_NO_DEVICE;
}

SNDERR Dll_Wave_Device::set_hwnd(HWND hwnd)
{
    if (!m_pdsmgr)
        return SNDERR_UNINITIALIZED;
    return m_pdsmgr->set_hwnd(hwnd);
}

SNDERR Dll_Wave_Device::stop_raw_dump()
{
    if (m_pmixer)
        return m_pmixer->close_dump_file();
    return SNDERR_NO_DEVICE;
}

SNDERR __cdecl Dll_Wave_Device::delete_device()
{
    delete gbl_pwave_device;
    gbl_pwave_device = 0;
    return SNDERR_OK;
}

Dll_Wave_Device::Dll_Wave_Device()
{
    m_pdsmgr = 0;
    m_pmix_dsb = 0;
    m_pmixer = 0;
    m_ptimer = 0;
    m_rate = 0;
    m_time_past = 0;
    m_master_volume = 0x7f;
    memset(&m_flags, 0, sizeof(m_flags));
    m_time_since_last_update = 0;
    m_saved_flags = 0;

    int mmx_support = 1;
}

Dll_Wave_Device::~Dll_Wave_Device()
{
    if (m_pdsmgr)
        release();
}

SNDERR Dll_Wave_Device::init(HWND game_hwnd, unsigned long flags)
{
    SNDERR retval;

    if (!sound_win.initialized() && (flags & 0x80)) {
        sound_win.init(game_hwnd);
        m_flags.debug = 1;
    }
    m_saved_flags = flags;

    if (!m_pdsmgr) {
        m_pdsmgr = new CDSMgr;
        retval = m_pdsmgr->init(flags, 15);
        if (retval != SNDERR_OK) {
            delete m_pdsmgr;
            m_pdsmgr = 0;
            return SNDERR_NO_DEVICE;
        }
        retval = m_pdsmgr->create(game_hwnd, 2);
        if (retval != SNDERR_OK) {
            delete m_pdsmgr;
            m_pdsmgr = 0;
            return SNDERR_NO_DEVICE;
        }
    }

    if (!m_pmixer) {
        m_pmixer = new CWaveMixer;
        m_pmixer->set_hwnd(sound_win.get_hwnd());
        if (m_flags.mmx_support)
            m_pmixer->use_mmx(1);
    }

    unsigned long mix_buffer_size = 0x10000;
    if (!m_pmix_dsb && !m_pdsmgr->is_eax()) {
        unsigned long flags = 0x10088;
        if (m_pdsmgr->create_dsb(&m_pmix_dsb, flags, mix_buffer_size) == 0) {
            set_rate(30);
            m_pmix_dsb->set_write_margin(get_rate(), 14);
            m_pmix_dsb->quiet();
            m_pmix_dsb->play(1);
            m_pmixer->set_mixbuff_size(m_pmix_dsb->get_nwrite_margin_samples() * 2);
        } else {
            m_pdsmgr->destroy();
            m_pmix_dsb = 0;
            return SNDERR_DSB_LOST;
        }
    }

    if (m_pdsmgr->is_3d())
        m_pdsmgr->set_listener_position(0.0f, 0.0f, -0.1f);

    if (m_pdsmgr->is_eax()) {
        unsigned long flags = 0x10094;
        if (m_pdsmgr->create_dsb(&m_p3d_dsb_l, &g_wfx22050Mono16, flags, mix_buffer_size) == 0) {
            if (m_pdsmgr->create_dsb(&m_p3d_dsb_r, &g_wfx22050Mono16, flags, mix_buffer_size) == 0) {
                set_rate(15);
                m_p3d_dsb_r->set_write_margin(get_rate(), 14);
                m_p3d_dsb_l->set_write_margin(get_rate(), 14);
                if (m_p3d_dsb_l->get_3d_buffer() != 0)
                    m_p3d_dsb_l->get_3d_buffer()->SetPosition(-0.5f, 0.0f, -0.5f, 0);
                if (m_p3d_dsb_r->get_3d_buffer() != 0)
                    m_p3d_dsb_r->get_3d_buffer()->SetPosition(0.5f, 0.0f, -0.5f, 0);
                m_p3d_dsb_l->quiet();
                m_p3d_dsb_r->quiet();
                m_p3d_dsb_l->play(1);
                m_p3d_dsb_r->play(1);
                m_pmixer->set_mixbuff_size(m_p3d_dsb_l->get_nwrite_margin_samples());
                m_flags.eax = 1;
            } else {
                m_pdsmgr->destroy_dsb(m_p3d_dsb_l);
                m_pdsmgr->destroy();
                m_p3d_dsb_r = 0;
                m_p3d_dsb_l = 0;
                return SNDERR_DSB_LOST;
            }
        } else {
            m_pdsmgr->destroy();
            m_p3d_dsb_l = 0;
            return SNDERR_DSB_LOST;
        }
    }

    if (!m_flags.debug)
        gbl_sound_thread.start();

    if (gbl_ptimer) {
        m_ptimer = gbl_ptimer;
        gbl_ptimer->register_device(this);
        gbl_ptimer->start();
    }
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::release()
{
    if (m_ptimer) {
        m_ptimer->stop();
        gbl_ptimer->unregister_device(this);
    }

    if (m_flags.debug)
        sound_win.set_wo_processing(false);
    else
        gbl_sound_thread.stop();

    kill_request_queue();

    if (m_pdsmgr) {
        if (m_pmix_dsb) {
            m_pdsmgr->destroy_dsb(m_pmix_dsb);
            m_pmix_dsb = 0;
        }
        if (m_pdsmgr->is_3d()) {
            m_pdsmgr->destroy_dsb(m_p3d_dsb_l);
            m_pdsmgr->destroy_dsb(m_p3d_dsb_r);
        }
        delete m_pdsmgr;
        m_pdsmgr = 0;
    }

    if (m_pmixer) {
        delete m_pmixer;
        m_pmixer = 0;
    }

    if (m_flags.debug)
        sound_win.destroy();

    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::enable()
{
    SNDERR retval;

    if (!m_flags.disabled)
        return SNDERR_OK;

    if (m_pdsmgr == 0 || m_pmixer == 0 || m_pmix_dsb != 0)
        return SNDERR_NOCANDO;

    m_pdsmgr->init(m_saved_flags, 15);

    retval = m_pdsmgr->create(CDSMgr::get_game_hwnd(), 2);
    if (retval != SNDERR_OK) {
        delete m_pdsmgr;
        m_pdsmgr = 0;
        return SNDERR_NO_DEVICE;
    }

    unsigned long mix_buffer_size = 0x10000;
    if (m_pmix_dsb == 0 && !m_pdsmgr->is_eax()) {
        unsigned long flags = 0x10088;
        if (m_pdsmgr->create_dsb(&m_pmix_dsb, flags, mix_buffer_size) == 0) {
            set_rate(15);
            m_pmix_dsb->set_write_margin(get_rate(), 14);
        } else {
            m_pdsmgr->destroy();
            m_pmix_dsb = 0;
            return SNDERR_DSB_LOST;
        }
    }

    if (m_pdsmgr->is_3d())
        m_pdsmgr->set_listener_position();

    if (m_pdsmgr->is_eax()) {
        unsigned long flags = 0x10094;
        if (m_pdsmgr->create_dsb(&m_p3d_dsb_l, &g_wfx22050Mono16, flags, mix_buffer_size) == 0) {
            if (m_pdsmgr->create_dsb(&m_p3d_dsb_r, &g_wfx22050Mono16, flags, mix_buffer_size) == 0) {
                set_rate(15);
                m_p3d_dsb_r->set_write_margin(get_rate(), 14);
                m_p3d_dsb_l->set_write_margin(get_rate(), 14);

                if (m_p3d_dsb_l->get_3d_buffer() != 0)
                    m_p3d_dsb_l->get_3d_buffer()->SetPosition(-0.5f, 0.0f, -0.5f, 0);
                if (m_p3d_dsb_r->get_3d_buffer() != 0)
                    m_p3d_dsb_r->get_3d_buffer()->SetPosition(0.5f, 0.0f, -0.5f, 0);

                m_pmixer->set_mixbuff_size(m_p3d_dsb_l->get_nwrite_margin_samples());
            } else {
                m_pdsmgr->destroy_dsb(m_p3d_dsb_l);
                m_pdsmgr->destroy();
                m_p3d_dsb_r = 0;
                m_p3d_dsb_l = 0;
                return SNDERR_DSB_LOST;
            }
        } else {
            m_pdsmgr->destroy();
            m_p3d_dsb_l = 0;
            return SNDERR_DSB_LOST;
        }
        m_flags.eax = 1;
    }

    restart();
    m_flags.disabled = 0;

    if (gbl_pmidi_device != 0 && gbl_pmidi_device->is_disabled() != 0)
        gbl_pmidi_device->enable();

    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::disable()
{
    if (m_flags.disabled)
        return SNDERR_OK;

    if (gbl_pmidi_device)
        gbl_pmidi_device->disable();

    suspend();

    gbl_ptimer->stop();
    m_pmixer->clean_play_list();

    m_time_past = 0;
    m_time_since_last_update = 0;

    if (m_pdsmgr) {
        if (m_pmix_dsb) {
            m_pdsmgr->destroy_dsb(m_pmix_dsb);
            m_pmix_dsb = 0;
        }
        if (m_p3d_dsb_l) {
            m_pdsmgr->destroy_dsb(m_p3d_dsb_l);
            m_p3d_dsb_l = 0;
        }
        if (m_p3d_dsb_r) {
            m_pdsmgr->destroy_dsb(m_p3d_dsb_r);
            m_p3d_dsb_r = 0;
        }
        m_pdsmgr->disable();
    }

    m_flags.disabled = 1;
    return SNDERR_OK;
}

void Dll_Wave_Device::suspend()
{
    gbl_ptimer->stop();
    if (m_ptimer)
        gbl_ptimer->unregister_device(this);

    if (m_pmix_dsb)
        m_pmix_dsb->stop();

    if (m_pdsmgr->is_eax()) {
        if (m_p3d_dsb_l)
            m_p3d_dsb_l->stop();
        if (m_p3d_dsb_r)
            m_p3d_dsb_r->stop();
    }

    if (m_pmixer)
        m_pmixer->m_last_time = 0;

    m_time_past = 0;
    m_time_since_last_update = 0;
}

void Dll_Wave_Device::restart()
{
    if (m_pmix_dsb)
        m_pmix_dsb->resync();

    if (m_pdsmgr->is_eax()) {
        if (m_p3d_dsb_l)
            m_p3d_dsb_l->play(1);
        if (m_p3d_dsb_r)
            m_p3d_dsb_r->play(1);
    }

    if (gbl_ptimer) {
        m_ptimer = gbl_ptimer;
        gbl_ptimer->register_device(this);
        gbl_ptimer->start();
    }
}

SNDERR Dll_Wave_Device::select(unsigned long id)
{
    if (m_pdsmgr == 0)
        return SNDERR_NO_DEVICE;

    if (id >= m_pdsmgr->get_ndevices())
        return SNDERR_BAD_PARAM;

    if (m_ptimer != 0) {
        gbl_ptimer->unregister_device(this);
        gbl_ptimer->stop();
    }

    if (m_pmix_dsb != 0) {
        m_pdsmgr->destroy_dsb(m_pmix_dsb);
        m_pmix_dsb = 0;
    }

    m_pdsmgr->destroy_all_dsbs();
    m_pdsmgr->destroy();

    if (m_pdsmgr->set_pcur(id) != 0)
        return SNDERR_NOCANDO;

    SNDERR retval = m_pdsmgr->create(CDSMgr::get_game_hwnd(), 0);
    if (retval != SNDERR_OK)
        return retval;

    unsigned long mix_buffer_size = 0x10000;
    if (m_pdsmgr->is_emulated())
        mix_buffer_size = 0x20000;

    if (m_pdsmgr->create_dsb(&m_pmix_dsb, 0x10088, mix_buffer_size) == 0) {
        set_rate(15);
        m_pmix_dsb->set_write_margin(get_rate(), 14);
        m_pmix_dsb->quiet();
        m_pmix_dsb->play(1);

        m_pmixer->set_mixbuff_size(m_pmix_dsb->get_nwrite_margin_samples() * 2);

        m_ptimer = gbl_ptimer;
        gbl_ptimer->register_device(this);
        gbl_ptimer->start();
    } else {
        m_pdsmgr->destroy();
        m_pmix_dsb = 0;
        return SNDERR_DSB_LOST;
    }

    return SNDERR_OK;
}

void Dll_Wave_Device::update_sound()
{
    do_requests();

    update_hwbuffers();

    m_time_past = 0;

    if (m_pmixer) {
        if (!m_pdsmgr->is_eax())
            m_pmixer->mix_2_mb(m_pmix_dsb);
        else
            m_pmixer->mix_2_3d_mb(m_p3d_dsb_l, m_p3d_dsb_r);
    }
}

SNDERR Dll_Wave_Device::queue_request(Sound *psound, REQUEST_TYPE type)
{
    unsigned new_index = (m_req_queue.m_GT_index + 1) & 0xfff;
    if (new_index == m_req_queue.m_TT_index)
        return SNDERR_BUFFER_OVERRUN;
    m_req_queue.m_queue[new_index] = new CCriticalRequest;
    m_req_queue.m_queue[new_index]->m_type = type;
    m_req_queue.m_queue[new_index]->m_psound = psound;
    m_req_queue.m_GT_index = new_index;
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::queue_request(CCriticalRequest *preq)
{
    unsigned new_index = (m_req_queue.m_GT_index + 1) & 0xfff;
    if (new_index == m_req_queue.m_TT_index)
        return SNDERR_BUFFER_OVERRUN;
    m_req_queue.m_queue[new_index] = preq;
    m_req_queue.m_GT_index = new_index;
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::clean_request_queue()
{
    CWaveMgr *pwmgr;
    unsigned TT_index = m_req_queue.m_TT_index;
    unsigned GT_index = m_req_queue.m_GT_index;
    if (TT_index == GT_index)
        return SNDERR_OK;

    unsigned nreqs = (unsigned)(GT_index - TT_index) & 0xfff;
    if (nreqs)
        weed_out_conflicting_requests(&TT_index, &GT_index);

    TT_index = (TT_index + 1) & 0xfff;

    for (unsigned i = 0; i < nreqs; i++) {
        CCriticalRequest *pcur_req = m_req_queue[TT_index];
        if (pcur_req) {
            switch (pcur_req->m_type) {
            case REQUEST_PLAY:
                delete pcur_req;
                break;

            case REQUEST_STOP: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr->is_wave()) {
                    m_play_list.Remove(pwmgr);
                    pwmgr->ResetAllSlots();
                    if (pwmgr->is_dynamic_buffer())
                        pwmgr->direct_unload();
                    else
                        pwmgr->reset();
                } else if (pwmgr->is_vrx()) {
                    m_play_list.Remove(pwmgr);
                    ((CWaveMgr *)pcur_req->m_psound)->m_stateBits &= ~3;
                }
            }
                delete pcur_req;
                break;

            case REQUEST_UNLOAD: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr->is_wave()) {
                    m_play_list.Remove(pwmgr);
                    pwmgr->ResetAllSlots();
                    for (int i = 1; i < pwmgr->m_nvoices; i++) {
                        CWaveMgr *pvoice = pwmgr->m_voices[i];
                        if (pvoice) {
                            m_play_list.Remove(pvoice);
                            pvoice->ResetAllSlots();
                            pvoice->direct_unload();
                        }
                        pwmgr->direct_unload();
                    }
                } else if (pwmgr->is_vrx()) {
                    CVoiceRxMgr *pvrxmgr = (CVoiceRxMgr *)pwmgr;
                    m_play_list.Remove(pwmgr);
                    pwmgr->m_stateBits &= ~3;
                    pvrxmgr->direct_unload();
                }
                delete pcur_req;
            } break;

            case REQUEST_DELETE: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr->is_wave()) {
                    if (!pwmgr)
                        break;
                    if (pwmgr->m_flagsWord & 0x40) {
                        m_play_list.Remove(pwmgr);
                        pwmgr->ResetAllSlots();
                    }
                    for (int i = 1; i < pwmgr->m_nvoices; i++) {
                        CWaveMgr *pvoice = pwmgr->m_voices[i];
                        if (pvoice) {
                            m_play_list.Remove(pvoice);
                            pvoice->ResetAllSlots();
                            delete pvoice;
                        }
                    }
                    delete pwmgr;
                } else if (pwmgr->is_vrx()) {
                    if (pwmgr->is_dynamic_buffer())
                        m_play_list.Remove(pwmgr);
                    CWaveMgr *t = (CWaveMgr *)pcur_req->m_psound;
                    if (t)
                        delete t;
                }
                delete pcur_req;
            } break;
            }
            m_req_queue.m_queue[TT_index] = 0;
        }
        TT_index = (TT_index + 1) & 0xfff;
    }

    m_req_queue.m_TT_index = GT_index;
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::kill_request_queue()
{
    CWaveMgr *pwmgr = 0, *pvoice = 0;
    CVoiceRxMgr *pvrxmgr = 0;
    unsigned TT_index = m_req_queue.m_TT_index;
    unsigned GT_index = m_req_queue.m_GT_index;
    unsigned char active = 0;
    if (TT_index == GT_index)
        return SNDERR_OK;

    unsigned nreqs = (unsigned)(GT_index - TT_index) & 0xfff;

    TT_index = (TT_index + 1) & 0xfff;

    for (unsigned i = 0; i < nreqs; i++) {
        CCriticalRequest *pcur_req = m_req_queue[TT_index];
        if (pcur_req) {
            switch (pcur_req->m_type) {
            case REQUEST_PLAY:
                delete pcur_req;
                break;

            case REQUEST_STOP:
                delete pcur_req;
                break;

            case REQUEST_UNLOAD:
                delete pcur_req;
                break;

            case REQUEST_DELETE:
                delete (CWaveMgr *)pcur_req->m_psound;
                pcur_req->m_psound = 0;
                delete pcur_req;
                break;

            default:
                break;
            }
            m_req_queue.m_queue[TT_index] = 0;
        }
        TT_index = (TT_index + 1) & 0xfff;
    }

    m_req_queue.m_TT_index = GT_index;
    return SNDERR_OK;
}

void Dll_Wave_Device::do_requests()
{
    CWaveMgr *pwmgr = 0, *pvoice = 0;
    dllSoundList *psoundlist = 0;
    CVoiceRxMgr *pvrxmgr = 0;
    CCriticalRequest *pcur_req = 0, *pcur_req2 = 0, *pcur_req3 = 0;
    unsigned int TT_index = m_req_queue.m_TT_index;
    unsigned int GT_index = m_req_queue.m_GT_index;
    unsigned char active = 0;
    if (TT_index == GT_index)
        return;

    unsigned int nreqs = (GT_index - TT_index) & 0xfff;
    TT_index = (TT_index + 1) & 0xfff;

    for (unsigned int i = 0; i < nreqs; i++) {
        pcur_req = m_req_queue[TT_index];
        if (pcur_req) {
            switch (pcur_req->m_type) {
            case REQUEST_PLAY: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr->is_wave()) {
                    if (!pwmgr->is_active() && !pwmgr->is_GT_request_pending()) {
                        m_play_list.Add(pwmgr);
                        ((CWaveMgr *)pcur_req->m_psound)->set_active(1);
                        pwmgr = (CWaveMgr *)pcur_req->m_psound;
                        if (pwmgr->is_dynamic_buffer() && pwmgr->is_hwbuffer()) {
                            pwmgr = (CWaveMgr *)pcur_req->m_psound;
                            if (pwmgr->is_static())
                                pwmgr->CWaveFile::play();
                        }
                        int bad = 0;
                    }
                } else if (pwmgr->is_vrx()) {
                    if (!((CVoiceRxMgr *)pwmgr)->is_active()) {
                        m_play_list.Add(pwmgr);
                        ((CVoiceRxMgr *)pcur_req->m_psound)->set_active(1);
                    }
                }
                delete pcur_req;
            } break;

            case REQUEST_STOP: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr->is_wave()) {
                    m_play_list.Remove(pwmgr);
                    pwmgr->set_done();
                    pwmgr->set_active(0);
                    if (pwmgr->is_dynamic_buffer()) {
                        pcur_req->m_type = REQUEST_UNLOAD;
                        pwmgr = (CWaveMgr *)pcur_req->m_psound;
                        if (!pwmgr)
                            break;
                        pwmgr->m_flags.GT_req_pending = 1;
                        if (is_debug())
                            PostMessageA(pwmgr->m_hwnd, WD_ID_CRIT_REQ, 0, (long)pcur_req);
                        else
                            gbl_sound_thread.post_message(0x7ee, 0, (long)pcur_req);
                        break;
                    } else {
                        if (pwmgr->phwdsb()) {
                            pwmgr->hwdsb_stop();
                            pwmgr->phwdsb()->reset();
#ifdef USE_MPEG
                            if (pwmgr->is_mpeg()) {
                                pwmgr->mpeg_stop();
                                pwmgr->phwdsb()->quiet();
                            }
#endif
                        }
                        pcur_req->m_type = REQUEST_RESET;
                        pwmgr = (CWaveMgr *)pcur_req->m_psound;
                        if (!pwmgr)
                            break;
                        pwmgr->m_flags.GT_req_pending = 1;
                        if (is_debug())
                            PostMessageA(pwmgr->m_hwnd, WD_ID_CRIT_REQ, 0, (long)pcur_req);
                        else
                            gbl_sound_thread.post_message(0x7ee, 0, (long)pcur_req);
                        break;
                    }
                }
                if (!pwmgr->is_vrx())
                    break;
                m_play_list.Remove(pwmgr);
                ((CVoiceRxMgr *)pcur_req->m_psound)->set_done();
                delete pcur_req;
            } break;

            case REQUEST_UNLOAD: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr->is_wave()) {
                    m_play_list.Remove(pwmgr);
                    pwmgr->set_done();
                    pwmgr->set_active(0);
                    for (int i = 1; i < pwmgr->m_nvoices; i++) {
                        pvoice = pwmgr->m_voices[i];
                        if (pvoice) {
                            m_play_list.Remove(pvoice);
                            pvoice->set_done();
                            pvoice->set_active(0);
                        }
                    }
                    pwmgr = (CWaveMgr *)pcur_req->m_psound;
                    if (!pwmgr)
                        break;
                    pwmgr->m_flags.GT_req_pending = 1;
                    if (is_debug())
                        PostMessageA(pwmgr->m_hwnd, WD_ID_CRIT_REQ, 0, (long)pcur_req);
                    else
                        gbl_sound_thread.post_message(0x7ee, 0, (long)pcur_req);
                    break;
                }
                if (!pwmgr->is_vrx())
                    break;
                pvrxmgr = (CVoiceRxMgr *)pwmgr;
                m_play_list.Remove(pwmgr);
                pvrxmgr->set_done();
                pvrxmgr->set_active(0);
                pvrxmgr->direct_unload();
            } break;

            case REQUEST_DELETE: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr->is_wave()) {
                    if (!pwmgr)
                        break;
                    if (pwmgr->is_active()) {
                        m_play_list.Remove(pwmgr);
                        pwmgr->set_done();
                        pwmgr->set_active(0);
                    }
                    pwmgr = (CWaveMgr *)pcur_req->m_psound;
                    if (!pwmgr)
                        break;
                    pwmgr->m_flags.GT_req_pending = 1;
                    if (is_debug())
                        PostMessageA(pwmgr->m_hwnd, WD_ID_CRIT_REQ, 0, (long)pcur_req);
                    else
                        gbl_sound_thread.post_message(0x7ee, 0, (long)pcur_req);
                    break;
                }
                if (!pwmgr->is_vrx())
                    break;
                if (pwmgr->is_dynamic_buffer())
                    m_play_list.Remove(pwmgr);
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr)
                    delete pwmgr;
                delete pcur_req;
            } break;

            case REQUEST_FADE: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr && pwmgr->is_mpeg())
                    pwmgr->mpeg_fade();
                delete pcur_req;
            } break;

            case REQUEST_SET_LINK: {
                pwmgr = (CWaveMgr *)pcur_req->m_psound;
                if (pwmgr) {
                    pwmgr->m_plinked_wave = (char *)pcur_req->m_data1;
                    pwmgr->m_plinked_semaphore = (unsigned long *)pcur_req->m_data2;
                }
                delete pcur_req;
            } break;

            case REQUEST_SET_SOUNDLIST_RAMP: {
                psoundlist = (dllSoundList *)pcur_req->m_psound;
                if (psoundlist) {
                    pwmgr = psoundlist->getCurSound();
                    if (pwmgr) {
                        if (!pcur_req->m_data1)
                            pwmgr->ramp(0, -1, pcur_req->m_data2);
                        else
                            pwmgr->ramp(-1, 0, pcur_req->m_data2);
                    }
                }
                delete pcur_req;
            } break;

            case REQUEST_SET_SOUNDLIST_VOL: {
                psoundlist = (dllSoundList *)pcur_req->m_psound;
                if (psoundlist) {
                    pwmgr = psoundlist->getCurSound();
                    if (pwmgr)
                        pwmgr->set_volume(pcur_req->m_data1);
                }
                delete pcur_req;
            } break;

            case REQUEST_SOUNDLIST_FADE: {
                psoundlist = (dllSoundList *)pcur_req->m_psound;
                if (psoundlist) {
                    pwmgr = psoundlist->getCurSound();
                    if (pwmgr)
                        pwmgr->CWaveFile::fade(0x7d0);
                }
                delete pcur_req;
            } break;

            case REQUEST_DELETE_SOUNDLIST: {
                psoundlist = (dllSoundList *)pcur_req->m_psound;
                if (!psoundlist) {
                    delete pcur_req;
                    break;
                }
                pwmgr = psoundlist->getCurSound();
                if (pwmgr) {
                    if (pwmgr->is_active()) {
                        m_play_list.Remove(pwmgr);
                        pwmgr->set_done();
                        pwmgr->set_active(0);
                    }
                    pcur_req2 = new CCriticalRequest;
                    pcur_req2->m_type = REQUEST_DELETE;
                    pcur_req2->m_psound = pwmgr;
                    pwmgr->m_flags.GT_req_pending = 1;
                    unsigned char o = (unsigned char)(*(unsigned int *)&m_flags >> 5);
                    if (o & 1) {
                        HWND h = pwmgr->m_hwnd;
                        PostMessageA(h, WD_ID_CRIT_REQ, 0, (long)pcur_req2);
                    } else
                        gbl_sound_thread.post_message(0x7ee, 0, (long)pcur_req2);
                }
                pwmgr = psoundlist->getQueuedSound();
                if (pwmgr) {
                    pcur_req3 = new CCriticalRequest;
                    pcur_req3->m_type = REQUEST_DELETE;
                    pcur_req3->m_psound = pwmgr;
                    pwmgr->m_flags.GT_req_pending = 1;
                    if (is_debug())
                        PostMessageA(pwmgr->m_hwnd, WD_ID_CRIT_REQ, 0, (long)pcur_req3);
                    else
                        gbl_sound_thread.post_message(0x7ee, 0, (long)pcur_req3);
                }
                delete psoundlist;
                delete pcur_req;
            } break;

            default:
                break;
            }

            m_req_queue.m_queue[TT_index] = 0;
            TT_index = (TT_index + 1) & 0xfff;
        }
    }

    m_req_queue.m_TT_index = GT_index;
}

SNDERR Dll_Wave_Device::post_GT_request(CCriticalRequest *pcur_req)
{
    CWaveMgr *pcur_wave = (CWaveMgr *)pcur_req->m_psound;
    if (pcur_wave == 0)
        return SNDERR_UNINITIALIZED;
    pcur_wave->m_flags.GT_req_pending = 1;
    if (is_debug())
        PostMessageA(pcur_wave->m_hwnd, WD_ID_CRIT_REQ, 0, (LPARAM)pcur_req);
    else
        gbl_sound_thread.post_message(0x7ee, 0, (long)pcur_req);
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::post_GT_request(CWaveMgr *pwmgr, REQUEST_TYPE type)
{
    if (pwmgr == 0)
        return SNDERR_BAD_PARAM;
    CCriticalRequest *pcreq = new CCriticalRequest;
    pcreq->m_psound = pwmgr;
    pcreq->m_type = type;
    pwmgr->m_flags.GT_req_pending = 1;
    if (is_debug())
        PostMessageA(pwmgr->m_hwnd, WD_ID_CRIT_REQ, 0, (LPARAM)pcreq);
    else
        gbl_sound_thread.post_message(0x7ee, 0, (long)pcreq);
    return SNDERR_OK;
}

unsigned long Dll_Wave_Device::weed_out_conflicting_requests(unsigned int *start_index, unsigned int *end_index)
{
    unsigned long index1 = *start_index;
    unsigned long index2 = (index1 + 1) & 0xfff;
    CWaveMgr *pcur_wave = 0;
    while (index1 != *end_index) {
        if (index2 == *end_index)
            break;
        while (m_req_queue[index1] != 0) {
            if (index2 == *end_index)
                break;
            if (m_req_queue[index2] != 0) {
                if (m_req_queue[index1]->m_psound == m_req_queue[index2]->m_psound) {
                    if (m_req_queue[index1]->m_type == m_req_queue[index2]->m_type) {
                        delete m_req_queue[index2];
                        m_req_queue.m_queue[index2] = 0;
                    } else if (m_req_queue[index1]->m_type == REQUEST_PLAY
                               && m_req_queue[index2]->m_type == REQUEST_STOP) {
                        pcur_wave = (CWaveMgr *)m_req_queue[index1]->m_psound;
                        if (pcur_wave != 0 && pcur_wave->is_active() != 0) {
                            delete m_req_queue[index1];
                            m_req_queue.m_queue[index1] = 0;
                        } else {
                            delete m_req_queue[index1];
                            m_req_queue.m_queue[index1] = 0;
                            delete m_req_queue[index2];
                            m_req_queue.m_queue[index2] = 0;
                        }
                    } else if (m_req_queue[index1]->m_type == REQUEST_STOP
                               && m_req_queue[index2]->m_type == REQUEST_PLAY) {
                        pcur_wave = (CWaveMgr *)m_req_queue[index1]->m_psound;
                        if (pcur_wave != 0 && pcur_wave->is_active() == 0) {
                            delete m_req_queue[index1];
                            m_req_queue.m_queue[index1] = 0;
                        } else {
                            delete m_req_queue[index1];
                            m_req_queue.m_queue[index1] = 0;
                            delete m_req_queue[index2];
                            m_req_queue.m_queue[index2] = 0;
                        }
                    }
                }
            }
            index2 = (index2 + 1) & 0xfff;
        }
        index1 = (index1 + 1) & 0xfff;
        index2 = (index1 + 1) & 0xfff;
    }
    return 0;
}

SNDERR Dll_Wave_Device::pull_from_play_list(CWaveMgr *pwmgr)
{
    if (m_play_list.Remove(pwmgr) != 0) {
        pwmgr->set_active(0);
        return SNDERR_OK;
    }
    return SNDERR_NOT_PRESENT;
}

void Dll_Wave_Device::add_to_play_list(CWaveMgr *pwmgr)
{
    m_play_list.Append(pwmgr);
    pwmgr->set_active(1);
}

void Dll_Wave_Device::update_hwbuffers()
{
    if (m_pdsmgr) {
        Node<CDSB> *pnode = m_pdsmgr->get_first_duphwdsb();
        while (pnode) {
            CDSB *pdsb = pnode->get_obj();
            if (pdsb->get_delay() != 0) {
                pdsb->adjust_delay(m_time_since_last_update);
                if (pdsb->get_delay() >= 0) {
                    pdsb->set_delay(0);
                    pdsb->play(0);
                }
            }
            pnode = pnode->get_next();
        }
    }
}

void Dll_Wave_Device::add_to_time_past(unsigned long ms_time)
{
    m_time_since_last_update = ms_time;
    m_time_past = m_time_past + ms_time;
}

unsigned long Dll_Wave_Device::get_ndevices()
{
    if (m_pdsmgr != 0)
        return m_pdsmgr->get_ndevices();
    return 0;
}

unsigned long Dll_Wave_Device::get_hw_mem_size()
{
    if (m_pdsmgr)
        return m_pdsmgr->get_hw_mem_size();
    return 0;
}

SNDERR Dll_Wave_Device::get_description(unsigned long id, char *buffer, unsigned long buff_size)
{
    if (m_pdsmgr != 0)
        return m_pdsmgr->get_description(id, buffer, buff_size);
    *buffer = 0;
    return SNDERR_NO_DEVICE;
}

void Dll_Wave_Device::set_volume(unsigned long volume)
{
    if (m_pdsmgr)
        m_pdsmgr->set_volume(volume);
}

void *Dll_Wave_Device::get_ds()
{
    if (m_pdsmgr != 0)
        return m_pdsmgr->pds();
    return 0;
}

SNDERR Dll_Wave_Device::set_eax(unsigned long environment)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::set_eax(EAX_REVERB_PROPERTIES *peax)
{
    if (m_pdsmgr && m_pdsmgr->is_eax())
        return m_pdsmgr->set_eax(peax);
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::set_eax_mix(float mix)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::get_eax(unsigned long *penvironment)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::get_eax(EAX_REVERB_PROPERTIES *peax)
{
    if (m_pdsmgr && m_pdsmgr->is_eax())
        return m_pdsmgr->get_eax(peax);
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::set_eax_volume(float volume)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::get_eax_volume(float *pvolume)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::set_eax_decay(float decay)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::get_eax_decay(float *pdecay)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::set_eax_damping(float damping)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::get_eax_damping(float *pdamping)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_Device::set_listener_position(float x, float y, float z)
{
    if (m_pdsmgr != 0)
        return m_pdsmgr->set_listener_position(x, y, z);
    return SNDERR_NO_DEVICE;
}

SNDERR Dll_Wave_Device::get_listener_position(float *x, float *y, float *z)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::set_listener_xpos(float x)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::get_listener_xpos(float *px)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::set_listener_ypos(float y)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::get_listener_ypos(float *py)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::set_listener_zpos(float z)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR Dll_Wave_Device::get_listener_zpos(float *pz)
{
    return SNDERR_NO_SUPPORT;
}

bool Dll_Wave_Device::is_3d()
{
    if (m_pdsmgr)
        return m_pdsmgr->is_3d();
    return 0;
}

#include "SndNodeList.inl"
