#include "sounddevice.h"
#include "MidiSequence.h"
#include "dlist.h"
#include "sndwin.h"
#include "dllmididevice.h"
#include "dllwavedevice.h"
#include "midifile.h"
#include "midimgr.h"
#include "ctimer.h"

Dll_Midi_Device *gbl_pmidi_device = 0;

SNDERR __cdecl Dll_Midi_Device::create_device(Midi_Device **ppdev, unsigned long flags)
{
    if (!gbl_pmidi_device) {
        gbl_pmidi_device = new Dll_Midi_Device;
        *ppdev = gbl_pmidi_device;
        return SNDERR_OK;
    }
    return SNDERR_ALREADY_OPEN;
}

SNDERR __cdecl Dll_Midi_Device::delete_device()
{
    delete gbl_pmidi_device;
    gbl_pmidi_device = 0;
    return SNDERR_OK;
}

Dll_Midi_Device::Dll_Midi_Device()
{
    memset(&m_flags, 0, sizeof m_flags);
    m_hwnd = 0;
    m_master_volume = 0x7f;
    m_ptimer = 0;
    m_rate = 0xf;
    m_time_past = 0;
    m_hmidi_out = 0;
}

Dll_Midi_Device::~Dll_Midi_Device()
{
}

SNDERR Dll_Midi_Device::init(HWND game_hwnd, unsigned long flags)
{
    SNDERR retval = SNDERR_OK;
    if (flags & 2) {
        retval = init_win_device();
        if (retval != SNDERR_OK)
            return retval;
    }
    if (gbl_ptimer) {
        m_ptimer = gbl_ptimer;
        gbl_ptimer->register_device(this);
    }
    return SNDERR_OK;
}

SNDERR Dll_Midi_Device::init_win_device()
{
    unsigned int result = midiOutOpen(&m_hmidi_out, MIDI_MAPPER, 0, 0, 0);
    if (result)
        return SNDERR_BAD_DEVICE_OPEN;
    return SNDERR_OK;
}

SNDERR Dll_Midi_Device::release()
{
    if (m_ptimer) {
        m_ptimer->stop();
        gbl_ptimer->unregister_device(this);
    }

    CMidiMgr *pmidi = CMidiMgr::m_midi_list.First();
    clean_request_queue();
    while (pmidi) {
        CMidiMgr *ptmp_midi = pmidi;
        pmidi->set_active(0);
        CMidiMgr::m_midi_list.RemoveBySequence(pmidi);
        pmidi = CMidiMgr::m_midi_list.Next();
        delete ptmp_midi;
    }

    if (m_hmidi_out)
        midiOutClose(m_hmidi_out);

    Dll_KeyMap *pcurKeymap = CMidiMgr::m_keymap_list.remove_head();
    while (pcurKeymap) {
        delete pcurKeymap;
        pcurKeymap = CMidiMgr::m_keymap_list.remove_head();
    }
    return SNDERR_OK;
}

void Dll_Midi_Device::update_sound()
{
    Dll_KeyMap *pcurKeymap = 0;

    do_requests();
    for (CMidiMgr *pmidi = m_play_list.First(); pmidi; pmidi = m_play_list.Next())
        if (pmidi->is_active())
            pmidi->update_tracks(m_time_past);
    m_time_past = 0;
}

unsigned long Dll_Midi_Device::get_ndevices()
{
    return SNDERR_UNDEFINED;
}

SNDERR Dll_Midi_Device::get_description(unsigned long id, char *desc_buff, unsigned long buffsize)
{
    return SNDERR_UNDEFINED;
}

SNDERR Dll_Midi_Device::select(unsigned long id)
{
    return SNDERR_UNDEFINED;
}

void Dll_Midi_Device::set_volume(unsigned long volume)
{
}

void Dll_Midi_Device::suspend()
{
    if (m_ptimer != 0)
        gbl_ptimer->unregister_device(this);
}

void Dll_Midi_Device::restart()
{
    if (gbl_ptimer == 0)
        return;
    m_ptimer = gbl_ptimer;
    gbl_ptimer->register_device(this);
    gbl_ptimer->start();
}

SNDERR Dll_Midi_Device::disable()
{
    if (!m_flags.disabled) {
        suspend();
        sound_win.clean_message_queue();
        sound_win.set_mo_processing(false);
        m_flags.disabled = 1;
    }
    return SNDERR_OK;
}

SNDERR Dll_Midi_Device::enable()
{
    if (m_flags.disabled) {
        sound_win.set_mo_processing(true);
        restart();
        m_flags.disabled = 0;
    }
    return SNDERR_OK;
}

SNDERR Dll_Midi_Device::queue_request(CMidiMgr *pmmgr, REQUEST_TYPE type, int data1, int data2)
{
    unsigned new_index = (m_req_queue.m_GT_index + 1) & 0xfff;
    if (new_index == m_req_queue.m_TT_index)
        return SNDERR_BUFFER_OVERRUN;
    m_req_queue.m_queue[new_index] = new CCriticalRequest;
    m_req_queue.m_queue[new_index]->m_type = type;
    m_req_queue.m_queue[new_index]->m_psound = pmmgr;
    m_req_queue.m_queue[new_index]->m_data1 = data1;
    m_req_queue.m_queue[new_index]->m_data2 = data2;
    m_req_queue.m_GT_index = new_index;
    return SNDERR_OK;
}

SNDERR Dll_Midi_Device::clean_request_queue()
{
    unsigned TT_index = m_req_queue.m_TT_index;
    unsigned GT_index = m_req_queue.m_GT_index;
    unsigned char active = 0;
    if (TT_index == GT_index)
        return SNDERR_OK;

    unsigned nreqs = (unsigned)(GT_index - TT_index) & 0xfff;
    if (nreqs)
        weed_out_conflicting_requests(&TT_index, &GT_index);

    TT_index = (TT_index + 1) & 0xfff;

    for (unsigned i = 0; i < nreqs; i++) {
        CCriticalRequest *pcur_req = m_req_queue.m_queue[TT_index];
        if (pcur_req) {
            CMidiMgr *pmmgr;
            switch (pcur_req->m_type) {
            case REQUEST_PLAY:
                delete pcur_req;
                break;

            case REQUEST_STOP:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                pmmgr->direct_stop(0);
                pmmgr->set_active(0);
                pmmgr->reset();
                delete pcur_req;
                break;

            case REQUEST_UNLOAD:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                m_play_list.RemoveBySequence(pmmgr);
                pmmgr->direct_stop(0);
                pmmgr->set_active(0);
                pmmgr->direct_unload();
                delete pcur_req;
                break;

            case REQUEST_DELETE:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                if (pmmgr->is_active()) {
                    m_play_list.RemoveBySequence(pmmgr);
                    pmmgr->set_active(0);
                }
                delete pmmgr;
                delete pcur_req;
                break;

            case REQUEST_SET_ACTIVE_TRACKS:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->set_active_tracks();
                delete pcur_req;
                break;

            case REQUEST_PLAY_TRACKSET:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->TT_stop_trackset(pcur_req->m_data1);
                pmmgr->TT_play_trackset(pcur_req->m_data1);
                delete pcur_req;
                break;

            case REQUEST_STOP_TRACKSET:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->TT_stop_trackset(pcur_req->m_data1);
                delete pcur_req;
                break;

            case REQUEST_XPOSE_TRACKSET:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->TT_xpose_trackset(pcur_req->m_data1, pcur_req->m_data2);
                break;

            case REQUEST_SET_TSDONE_CALLBACK:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->TT_set_trackset_done_callback(pcur_req->m_data1, (void (*)(void *))pcur_req->m_data2);
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

void Dll_Midi_Device::do_requests()
{
    unsigned TT_index = m_req_queue.m_TT_index;
    unsigned GT_index = m_req_queue.m_GT_index;
    if (TT_index == GT_index)
        return;

    unsigned nreqs = (unsigned)(GT_index - TT_index) & 0xfff;
    if (nreqs)
        weed_out_conflicting_requests(&TT_index, &GT_index);

    TT_index = (TT_index + 1) & 0xfff;

    for (unsigned i = 0; i < nreqs; i++) {
        CCriticalRequest *pcur_req = m_req_queue.m_queue[TT_index];
        if (pcur_req) {
            CMidiMgr *pmmgr;
            switch (pcur_req->m_type) {
            case REQUEST_PLAY:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (pmmgr && !pmmgr->is_active()) {
                    ((CMidiMgr *)pcur_req->m_psound)->set_active(1);
                    m_play_list.AppendObject1(pmmgr);
                }
                delete pcur_req;
                break;

            case REQUEST_STOP:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                m_play_list.RemoveBySequence(pmmgr);
                pmmgr->direct_stop(0);
                pmmgr->set_active(0);
                pmmgr->post_reset();
                delete pcur_req;
                break;

            case REQUEST_UNLOAD:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                m_play_list.RemoveBySequence(pmmgr);
                pmmgr->direct_stop(0);
                pmmgr->set_active(0);
                post_GT_request(pcur_req);
                break;

            case REQUEST_DELETE:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                m_play_list.RemoveBySequence(pmmgr);
                pmmgr->direct_stop(0);
                pmmgr->set_active(0);
                post_GT_request(pcur_req);
                break;

            case REQUEST_SET_ACTIVE_TRACKS:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->set_active_tracks();
                delete pcur_req;
                break;

            case REQUEST_PLAY_TRACKSET:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->TT_stop_trackset(pcur_req->m_data1);
                pmmgr->TT_play_trackset(pcur_req->m_data1);
                delete pcur_req;
                break;

            case REQUEST_STOP_TRACKSET:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->TT_stop_trackset(pcur_req->m_data1);
                delete pcur_req;
                break;

            case REQUEST_XPOSE_TRACKSET:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (pmmgr)
                    pmmgr->TT_xpose_trackset(pcur_req->m_data1, pcur_req->m_data2);
                break;

            case REQUEST_SET_TSDONE_CALLBACK:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (!pmmgr)
                    break;
                pmmgr->TT_set_trackset_done_callback(pcur_req->m_data1, (void (*)(void *))pcur_req->m_data2);
            case REQUEST_SET_TRACK_LOOP_STATE:
                pmmgr = (CMidiMgr *)pcur_req->m_psound;
                if (pmmgr)
                    pmmgr->TT_set_track_loop_state(pcur_req->m_data1, pcur_req->m_data2 != 0);
                break;
            }
            m_req_queue.m_queue[TT_index] = 0;
        }
        TT_index = (TT_index + 1) & 0xfff;
    }

    m_req_queue.m_TT_index = GT_index;
}

SNDERR Dll_Midi_Device::post_GT_request(CCriticalRequest *pcur_req)
{
    CMidiMgr *pcur_midi = (CMidiMgr *)pcur_req->m_psound;
    if (!pcur_midi)
        return SNDERR_UNINITIALIZED;
    pcur_midi->m_flags.GT_req_pending = 1;
    if (gbl_pwave_device && gbl_pwave_device->is_debug())
        PostMessageA(pcur_midi->get_hwnd(), MD_ID_CRIT_REQ, 0, (long)pcur_req);
    else
        gbl_sound_thread.post_message(MD_ID_CRIT_REQ, 0, (long)pcur_req);
    return SNDERR_OK;
}

unsigned long Dll_Midi_Device::weed_out_conflicting_requests(unsigned int *start_index, unsigned int *end_index)
{
    unsigned long index1 = *start_index;
    unsigned long index2 = (index1 + 1) & 0xfff;
    CMidiMgr *pcur_midi;
    while (index1 != *end_index) {
        if (index2 == *end_index)
            break;
        while (m_req_queue[index1] != 0) {
            if (index2 == *end_index)
                break;
            if (m_req_queue[index2] != 0) {
                if (m_req_queue[index1]->m_psound == m_req_queue[index2]->m_psound) {
                    if (m_req_queue[index1]->m_type == m_req_queue[index2]->m_type
                        && m_req_queue[index1]->m_data1 == m_req_queue[index2]->m_data1
                        && m_req_queue[index1]->m_data2 == m_req_queue[index2]->m_data2) {
                        delete m_req_queue[index2];
                        m_req_queue.m_queue[index2] = 0;
                    } else if (m_req_queue[index1]->m_type == REQUEST_PLAY
                               && m_req_queue[index2]->m_type == REQUEST_STOP) {
                        pcur_midi = (CMidiMgr *)m_req_queue[index1]->m_psound;
                        if (pcur_midi != 0 && pcur_midi->is_active()) {
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
                        pcur_midi = (CMidiMgr *)m_req_queue[index1]->m_psound;
                        if (pcur_midi != 0 && !pcur_midi->is_active()) {
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
