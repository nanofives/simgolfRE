#include <string.h>

#pragma intrinsic(strcmp)

#include "MidiSequence.h"
#include "sound.h"
#include "miditools.h"
#include "random.h"
#include "wavefile.h"
#include "dlist.h"
#include "keymap.h"
#include "midifile.h"
#include "midimgr.h"

MidiMgrList CMidiMgr::m_midi_list;
DList<Dll_KeyMap> CMidiMgr::m_keymap_list;



CMidiMgr::CMidiMgr()
{
    memset(&m_flags, 0, sizeof m_flags);
    m_flags.looping = 1;
    m_meter.bpm = 4;
    m_meter.type = 4;
    m_hwnd = 0;
    m_ms_per_tick = 0.0;
    m_time_ticks = 0;
    m_ms_running_time = 0.0;
    m_volume = 0x7f;
    memset(m_patch_table, 0, sizeof m_patch_table);
    set_type(SOUNDTYPE_MIDI);
    m_switch_table = 0;
    m_nswitchers = 0;
    m_nswitch_threads = 0;
    m_switch_tracklo = 0;
    m_switch_trackhi = 0;
    m_current_active_set = -1;
    m_key = 0;
    set_tempo(120);
    m_flic_frame = 0;
}

CMidiMgr::CMidiMgr(HWND hwnd)
{
    memset(&m_flags, 0, sizeof m_flags);
    m_hwnd = hwnd;
    CMidiFile::set_hwnd(hwnd);
    set_type(SOUNDTYPE_MIDI);
    m_meter.bpm = 4;
    m_meter.type = 4;
    m_ms_per_tick = 0.0;
    set_tempo(120);
    m_key = 0;
    m_flic_frame = 0;
    m_volume = 0x7f;
}

SNDERR CMidiMgr::update_tracks(unsigned long ms_time_past)
{
    SNDERR retval = SNDERR_OK;
    unsigned long cur_track = 0;
    unsigned long cur_channel = 0;
    unsigned long bytes_used = 0;
    int processed_tracks = 0;
    double ms_event = 0;
    unsigned char breset = 0;
    double new_time = 0.0;
    unsigned char skip_loop = 0;
    double ticks;
    if (is_sync_to_flic()) {
        new_time = (double)get_frame() * (double)get_frame_delay();
        if (new_time < m_ms_running_time)
            skip_loop = 1;
        ms_time_past = (unsigned long)(new_time - m_ms_running_time);
    }
    if (m_ms_per_tick == 0.0)
        return (SNDERR)0x13;
    if (m_flags.startup) {
        ticks = 0;
        m_flags.startup = 0;
    } else {
        ticks = (unsigned long)(ms_time_past / m_ms_per_tick + 0.5);
    }
    if (is_sync_to_flic())
        m_ms_running_time = new_time;
    else
        m_ms_running_time = m_ms_running_time + ms_time_past;
    m_time_ticks = (long)(m_ms_running_time / m_ms_per_tick + 0.5);
    CMidiTrack *ptrack;
    while (1) {
        if (skip_loop)
            break;
        retval = get_track(&ptrack, (cur_track++));
        if (retval == 0x24)
            break;
        if (ptrack->is_switcher() && !ptrack->is_active_switcher())
            continue;
        if (!ptrack->m_flags.active)
            continue;
        int i = 0;
        while (!ptrack->at_end()) {
            if (i++ >= 10)
                break;
            if (CMidiFile::is_done())
                break;
            processed_tracks = 1;
            unsigned char *pcur_event = ptrack->get_pcur_event();
            if (pcur_event != 0) {
                int event_ticks = varlen2dw(pcur_event, &bytes_used);
                ms_event = (event_ticks + ptrack->get_ticks()) * m_ms_per_tick;
                if (!(ms_event < m_ms_running_time))
                    break;
                ptrack->advance(bytes_used);
                bytes_used = 0;
                if (process_event(ptrack, &bytes_used) == SNDERR_END_OF_TRACK)
                    break;
                ptrack->advance(bytes_used);
                ptrack->add_ticks(event_ticks);
            }
        }
    }
    if (processed_tracks == 0 && !CMidiFile::is_fading() && !CMidiFile::is_done() && m_flags.continuous == 0) {
        if (m_flags.looping) {
            reset();
            prepare_channels();
            if (m_flags.switch_state && m_nswitchers != 0) {
                for (int i = 0; i < m_nswitch_threads; i++) {
                    CMidiTrack *pswitcher;
                    if (get_track(&pswitcher, m_switch_table[i]) == 0 && pswitcher != 0)
                        pswitcher->set_active_switcher(true);
                }
            }
            return SNDERR_OK;
        }
        if (m_flags.stopping) {
            stop();
            m_midi_list.RemoveBySequence(this);
            m_flags.attached = 0;
            prepare_channels();
            reset();
            return SNDERR_OK;
        }
    } else if (CMidiFile::is_done()) {
        if (CMidiFile::need_timer_unload()) {
            stop();
            reset();
            m_midi_list.RemoveBySequence(this);
            m_flags.attached = 0;
            unload();
            return SNDERR_OK;
        }
        reset();
        m_midi_list.RemoveBySequence(this);
        m_flags.attached = 0;
        m_flags.stop_pending = 0;
        prepare_channels();
    }
    return SNDERR_OK;
}

CMidiMgr::~CMidiMgr()
{
    if (CMidiFile::is_loaded())
        CMidiFile::unload();

    for (int i = 0; i < 127; i++) {
        if (m_patch_table[i] != 0) {
            delete m_patch_table[i];
            m_patch_table[i] = 0;
        }
    }

    DllTrackSet *pts = m_trackset_list.remove_head();
    while (pts != 0) {
        delete pts;
        pts = m_trackset_list.remove_head();
    }
}

SNDERR CMidiMgr::reload()
{
    return CMidiFile::reload();
}

SNDERR CMidiMgr::load(const char *fname)
{
    CMidiTrack *ptrack;
    if (m_flags.GT_req_pending)
        sound_win.clean_message_queue();
    SNDERR retval = CMidiFile::load(fname);
    get_track(&ptrack, 1);
    if (retval == SNDERR_OK)
        prepare_channels();
    return retval;
}

SNDERR CMidiMgr::load(HMMIO hfile, const char *fname)
{
    CMidiTrack *ptrack;
    SNDERR retval = CMidiFile::load(hfile);
    get_track(&ptrack, 1);
    if (retval == SNDERR_OK)
        prepare_channels();
    if (m_trackset_list.get_nnodes() == 0) {
        CMidiTrack *ptrack;
        for (unsigned i = 0; i < get_ntracks(); i++) {
            if (get_track(&ptrack, i) == 0)
                ptrack->m_flags.active = 1;
        }
    }
    return retval;
}

SNDERR CMidiMgr::unload()
{
    SNDERR retval;
    if (gbl_pmidi_device != 0)
        retval = gbl_pmidi_device->queue_request(this, REQUEST_UNLOAD, 0, 0);
    else
        retval = SNDERR_UNINITIALIZED;
    return retval;
}

SNDERR CMidiMgr::direct_unload()
{
    SNDERR retval = SNDERR_OK;
    if (CMidiFile::is_loaded()) {
        retval = CMidiFile::unload();
        m_sf.loaded = 0;
    }
    return retval;
}

void CMidiMgr::cleanOutKeymapList()
{
    Dll_KeyMap *pcurKeyMap = m_keymap_list.get_first();
    while (pcurKeyMap != 0) {
        if (pcurKeyMap->get_ref() <= 0 && !pcurKeyMap->activeWaves()) {
            m_keymap_list.remove_cur();
            delete pcurKeyMap;
            pcurKeyMap = m_keymap_list.get_pcur();
        } else {
            pcurKeyMap = m_keymap_list.get_next();
        }
    }
}

SNDERR CMidiMgr::prepare_track(unsigned long tracknum)
{
    if (tracknum >= get_ntracks())
        return SNDERR_BAD_PARAM;
    CMidiTrack *ptrack;
    SNDERR retval = get_track(&ptrack, tracknum);
    if (retval == SNDERR_NOT_PRESENT)
        return retval;
    if (ptrack->at_end())
        return SNDERR_END_OF_TRACK;
    memset(ptrack->m_on_off_table, 0, 0x80 * 4);
    unsigned long bytes_used;
    unsigned char status;
    unsigned char *pcur_event = ptrack->get_pcur_event();
    unsigned long time = varlen2dw(pcur_event, &bytes_used);
    while (time == 0) {
        if ((pcur_event[1] & 0xf0) == 0x90)
            return SNDERR_OK;
        if ((pcur_event[1] & 0xf0) == 0xc0)
            return SNDERR_OK;
        ptrack->advance(bytes_used);
        bytes_used = 0;
        status = *ptrack->get_pcur_event();
        if (process_event(ptrack, &bytes_used) == SNDERR_END_OF_TRACK)
            return SNDERR_END_OF_TRACK;
        ptrack->advance(bytes_used);
        pcur_event = ptrack->get_pcur_event();
        time = varlen2dw(pcur_event, &bytes_used);
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::prepare_track(CMidiTrack *ptrack)
{
    if (ptrack == 0)
        return SNDERR_BAD_PARAM;
    if (ptrack->at_end())
        return SNDERR_END_OF_TRACK;
    unsigned long bytes_used;
    unsigned char status;
    unsigned char *pcur_event = ptrack->m_pcur_event;
    unsigned long time = varlen2dw(pcur_event, &bytes_used);
    while (time == 0) {
        if ((pcur_event[1] & 0xf0) == 0x90)
            return SNDERR_OK;
        if ((pcur_event[1] & 0xf0) == 0xc0)
            return SNDERR_OK;
        ptrack->advance(bytes_used);
        bytes_used = 0;
        status = *ptrack->get_pcur_event();
        SNDERR retval = process_event(ptrack, &bytes_used);
        if (retval == SNDERR_END_OF_TRACK)
            return retval;
        ptrack->advance(bytes_used);
        pcur_event = ptrack->m_pcur_event;
        time = varlen2dw(pcur_event, &bytes_used);
    }
    return SNDERR_OK;
}

void CMidiMgr::prepare_channels()
{
    SNDERR retval = SNDERR_OK;
    for (unsigned long i = 0; i < get_ntracks(); i++)
        prepare_track(i);
    set_prepared(1);
    m_time_ticks = 0;
    m_ms_running_time = 0.0;
}

SNDERR CMidiMgr::process_event(CMidiTrack *cur_track, unsigned long *bytes_used)
{
    unsigned long cur_status;
    unsigned char *event;
    unsigned long cur_channel;
    if (cur_track == 0)
        return SNDERR_BAD_PARAM;
    event = (unsigned char *)cur_track->get_pcur_event();
    if (*event & 0x80) {
        cur_status = *event;
        if ((cur_status & 0xf0) != 0xf0) {
            cur_channel = cur_status & 0x0f;
            cur_track->set_cur_status((unsigned char)cur_status);
            cur_status = cur_status & 0xf0;
            cur_track->set_cur_channel(cur_channel);
        }
        *bytes_used = 1;
        event++;
    } else {
        cur_channel = cur_track->get_cur_channel();
        cur_status = cur_track->get_cur_status() & 0xf0;
    }
    switch (cur_status) {
    case 0xff:
        *bytes_used += meta_event(event, cur_track);
        if (*event == 0x2f)
            return SNDERR_END_OF_TRACK;
        break;
    case 0x90:
        if (event[1] != 0) {
            *bytes_used += note_on(event, cur_channel, cur_track);
            return SNDERR_OK;
        }
    case 0x80:
        *bytes_used += note_off(event, cur_channel, cur_track);
        return SNDERR_OK;
    case 0xc0:
        m_channels[cur_channel].set_cur_patch(*event);
    case 0xe0:
        (*bytes_used)++;
        return SNDERR_OK;
    case 0xb0:
        *bytes_used += control_change(event, cur_channel, cur_track);
        break;
    default:
        return SNDERR_NO_SUPPORT;
    }
    return SNDERR_OK;
}

void CMidiMgr::free_all()
{
    CMidiMgr *pmmgr;
    while ((pmmgr = (CMidiMgr *)Sound::get_list()) != 0) {
        pmmgr->stop();
        pmmgr->unload();
    }
}

SNDERR CMidiMgr::post_unload()
{
    if (!CMidiFile::is_unload_pending()) {
        set_unload_pending();
        if (gbl_pwave_device != 0 && gbl_pwave_device->is_debug())
            PostMessageA(get_hwnd(), 0x7e9, 0, (long)this);
        else
            gbl_sound_thread.post_message(0x7e9, 0, (long)this);
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::post_reset()
{
    if (!CMidiFile::is_unload_pending()) {
        m_flags.reset_pending = 1;
        if (gbl_pwave_device != 0 && gbl_pwave_device->is_debug())
            PostMessageA(m_hwnd, 0x7ec, 0, (long)this);
        else
            gbl_sound_thread.post_message(0x7ec, 0, (long)this);
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::play()
{
    SNDERR retval = SNDERR_OK;
    if (!CMidiFile::is_loaded())
        reset_flags();
    if (is_playing() == 0 && !CMidiFile::is_fading() && !CMidiFile::is_active()) {
        m_flic_frame = 0;
        CMidiFile::play();
        if (m_nswitchers != 0) {
            CMidiTrack *pswitcher;
            for (int i = 0; i < m_nswitch_threads; i++) {
                get_track(&pswitcher, m_switch_table[i]);
                if (pswitcher != 0)
                    pswitcher->set_active_switcher(true);
            }
        }
        set_loop_state(1);
        m_flags.startup = 1;
        prepare_channels();
        m_last_update_time = timeGetTime();
        if (gbl_pmidi_device->queue_request(this, REQUEST_PLAY, 0, 0) == SNDERR_OK)
            set_done(0);
        return SNDERR_OK;
    }
    return SNDERR_ALREADY_PLAYING;
}

SNDERR CMidiMgr::play(unsigned long patch_num)
{
    SNDERR retval = SNDERR_OK;
    if (patch_num > 0x7f)
        return SNDERR_BAD_PARAM;
    return SNDERR_NOT_LOADED;
}

void CMidiMgr::direct_stop(bool stopSyncAlso)
{
    if (gbl_pmidi_device->hmidi_out() != 0) {
        CMidiTrack *ptrack = m_track_list.get_first();
        while (ptrack) {
            ptrack->hw_notes_off();
            ptrack = m_track_list.get_next();
        }
    }

    if (!is_sync_to_flic() || (char)stopSyncAlso != 0) {
        for (int i = 0; i < 0x7f; i++) {
            if (m_patch_table[i] != 0 && gbl_pwave_device != 0 && m_patch_table[i]->get_keymap() != 0)
                m_patch_table[i]->get_keymap()->stop_all();
        }
    }

    set_fade_flag(0);
    m_flic_frame = 0;
    m_flags.stop_pending = 1;

    set_loop_state(0);
    set_done(1);
}

SNDERR CMidiMgr::stop()
{
    if (!CMidiFile::is_loaded())
        return SNDERR_NOT_LOADED;
    if (gbl_pmidi_device != 0)
        gbl_pmidi_device->queue_request(this, REQUEST_STOP, 0, 0);
    return SNDERR_OK;
}

SNDERR CMidiMgr::fade()
{
    if (!CMidiFile::is_fading() && is_playing() != 0) {
        set_fade_flag(1);
        set_done(1);
        if (gbl_pmidi_device != 0)
            gbl_pmidi_device->queue_request(this, REQUEST_STOP, 0, 0);
    }
    return SNDERR_OK;
}

void CMidiMgr::set_volume(int vol)
{
    double dBvol = 0;
    if (vol > 0x7f)
        vol = 0x7f;
    else if (vol < 0)
        vol = 0;
    Sound::set_volume(vol);
    m_volume = vol;
}

SNDERR CMidiMgr::map_patch(char *fname, unsigned long number, unsigned long midi_channel)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR CMidiMgr::map_patch(char *fname, unsigned long patch_num)
{
    return SNDERR_NO_SUPPORT;
}

SNDERR CMidiMgr::map_patch(ProgramCtrl *pctrl, bool blegacy)
{
    unsigned long patch_num = pctrl->get_patch_num();
    const char *fname = pctrl->get_fname();
    m_hwnd = (HWND)sound_win.get_hwnd();
    if (patch_num <= 0x7f && fname != 0) {
        if (m_patch_table[patch_num] != 0) {
            delete m_patch_table[patch_num];
            m_patch_table[patch_num] = 0;
        }

        CProgramMgr *pcur_prgm = new CProgramMgr;

        if (pctrl->get_keymap() != 0) {
            pcur_prgm->ProgramCtrl::set_keymap(pctrl->get_keymap());
            Dll_KeyMap *pkm = m_keymap_list.get_first();
            while (pkm != 0) {
                if (strcmp(pkm->get_desc(), pctrl->get_keymap()) == 0) {
                    pcur_prgm->set_keymap(pkm);
                    pkm->inc_ref();
                    break;
                }
                pkm = m_keymap_list.get_next();
            }
            pcur_prgm->set_ctrl(pctrl);
            m_patch_table[patch_num] = pcur_prgm;
            return SNDERR_OK;
        }

        pcur_prgm->set_fname(fname);
        pcur_prgm->set_ctrl(pctrl);
        if (blegacy != 0) {
            SNDERR retval = pcur_prgm->load(pctrl);
            if (retval != 0) {
                delete pcur_prgm;
                m_patch_table[patch_num] = 0;
                return retval;
            }
        }
        m_patch_table[patch_num] = pcur_prgm;
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

bool CMidiMgr::isKeyMapLoaded(const char *name)
{
    Dll_KeyMap *pkm = m_keymap_list.get_first();
    while (pkm != 0) {
        if (strcmp(pkm->get_desc(), name) == 0)
            return true;
        pkm = m_keymap_list.get_next();
    }
    return false;
}

SNDERR CMidiMgr::clear_patch(unsigned long number)
{
    if (is_playing())
        return SNDERR_BUSY;
    delete m_patch_table[number];
    m_patch_table[number] = 0;
    return SNDERR_OK;
}

void CMidiMgr::set_pan(int pan)
{
    Sound::set_pan(pan);
}

unsigned long CMidiMgr::get_time()
{
    return m_time_ticks;
}

unsigned long CMidiMgr::get_total_track_ticks(unsigned long track)
{
    CMidiTrack *ptrack;
    unsigned long ticks;
    if (get_track(&ptrack, track) == 0)
        ticks = ptrack->m_total_ticks;
    else
        ticks = 0;
    return ticks;
}

unsigned long CMidiMgr::control_change(unsigned char *pdata, unsigned long cur_channel, CMidiTrack *ptrack)
{
    CWaveMgr *pwmgr = 0;
    Dll_KeyMap *pkm = 0;
    int pan = 0;
    switch (pdata[0]) {
    case 7:
        m_channels[cur_channel].set_cur_volume(pdata[1]);
        if (!ptrack->is_mute() && ptrack->get_cur_channel() == cur_channel) {
            for (int i = 0x7f; i >= 0; i--) {
                pwmgr = ptrack->m_on_off_table[i];
                if (pwmgr == 0)
                    continue;
                if (pwmgr->is_static()) {
                    int mask = 1;
                    WaveBuffer *pwb = 0;
                    for (int i = 0; i < 16; i++) {
                        if (pwmgr->is_instance_playing(mask))
                            pwb = pwmgr->get_instance(i);
                        if (pwb != 0 && !pwb->is_fading()) {
                            unsigned long cur_channel_volume = pdata[1];
                            unsigned long velocity = pwb->get_velocity();
                            cur_channel_volume = velocity * cur_channel_volume;
                            cur_channel_volume >>= 7;
                            cur_channel_volume *= m_volume;
                            cur_channel_volume >>= 7;
                            pwb->set_volume(cur_channel_volume);
                        }
                        mask <<= 1;
                    }
                } else {
                    pwmgr->set_volume(pdata[1]);
                }
            }
        }
        break;
    case 0xa:
        m_channels[cur_channel].set_cur_pan(pdata[1]);
        pan = pdata[1];
        break;
    }
    return 2;
}

unsigned long CMidiMgr::program_change(unsigned char *pdata, unsigned long cur_channel)
{
    unsigned long program_number = pdata[0];
    m_channels[cur_channel].set_cur_patch(program_number);
    return 1;
}

static Random g_rand;

unsigned long CMidiMgr::note_on(unsigned char *pdata, unsigned long cur_channel, CMidiTrack *ptrack)
{
    CMidiChannel *pchannel = &m_channels[cur_channel];
    unsigned long cur_patch = pchannel->get_cur_patch();
    unsigned long cur_channel_volume = pchannel->get_cur_volume();
    if (ptrack->is_mute())
        cur_channel_volume = 0;
    double cur_fp_vol = 1.0;
    WaveBuffer *pwb = 0;
    unsigned char do_velocity = 1;
    MIDI_NOTE note = (MIDI_NOTE)pdata[0];
    if (ptrack->get_ctrl() != 0)
        note = (MIDI_NOTE)ptrack->start_control(note);
    if (gbl_pmidi_device->hmidi_out() != 0) {
        if (ptrack->is_mute())
            return 2;
        midiOutShortMsg(gbl_pmidi_device->hmidi_out(), pack_midi_msg(0x90, cur_channel, note, pdata[1]));
        ptrack->m_hw_on_off_table[note].set_packed(pack_midi_msg(0x90, cur_channel, note, pdata[1]));
        return 2;
    }
    CProgramMgr *pcur_prgm = m_patch_table[cur_patch];
    if (pcur_prgm == 0 || ptrack->m_on_off_table[pdata[0]] != 0)
        return 2;
    Dll_KeyMap *pkm = pcur_prgm->get_keymap();
    if (pkm == 0)
        return 2;
    int i = pdata[0];
    if (pkm->is_cycling())
        i = pkm->get_next_key();
    CWaveMgr *pwmgr = pkm->get_key_wave(i);
    if (pkm->m_flags.ignore_velocity)
        do_velocity = 0;
    if (pwmgr == 0 || pwmgr->is_loaded() == 0)
        return 2;
    if (pwmgr->is_playing() != 0 && !pwmgr->is_static()) {
        if (pwmgr->m_nvoices > 1) {
            unsigned nvoices = pwmgr->m_nvoices;
            unsigned i;
            for (i = 0; i < nvoices - 1; i++) {
                if (pwmgr->m_voices[i]->is_playing() == 0) {
                    pwmgr = pwmgr->m_voices[i];
                    break;
                }
            }
            if (i == nvoices)
                return 2;
        }
    }
    if (pcur_prgm->note_off())
        ptrack->m_on_off_table[pdata[0]] = pwmgr;
    pwmgr->m_midi_note = pdata[0] + 1;
    int rand_offset = 0;
    if (pcur_prgm->is_randomize_pitch())
        rand_offset = g_rand.get(pcur_prgm->get_pitch_hi() - pcur_prgm->get_pitch_lo()) + pcur_prgm->get_pitch_lo();
    if (pcur_prgm->is_pitch_mapped()) {
        int diff = pdata[0] - pwmgr->midi_unity_note();
        pwmgr->set_pitch(ptrack->m_pitch_shift + diff * 100 + rand_offset);
    } else {
        pwmgr->set_pitch(rand_offset);
    }
    unsigned long instance;
    if (pwmgr->is_static()) {
        pwb = pwmgr->get_free_instance(&instance);
        if (pwb == 0)
            return 2;
        pwb->set_midi_note(pdata[0]);
    }
    if (pwmgr->is_asr()) {
        pwmgr->set_loop_state(1);
        if (pwb != 0)
            pwb->set_loop_state(1);
    }
    if (pwmgr->is_fading())
        pwmgr->stop_fade();
    if (do_velocity) {
        int velocity = (signed char)pdata[1] + ptrack->m_velocity_offset;
        cur_channel_volume = velocity * cur_channel_volume / 127;
        cur_channel_volume = cur_channel_volume * m_volume / 127;
        if (pwb != 0)
            pwb->set_velocity(velocity);
    }
    if (pcur_prgm->is_randomize_vol()) {
        rand_offset = g_rand.get(pcur_prgm->get_vol_hi() - pcur_prgm->get_vol_lo()) + pcur_prgm->get_vol_lo();
        rand_offset *= m_volume;
        rand_offset /= 127;
        cur_channel_volume += rand_offset;
    }
    if (pwb != 0) {
        pwb->set_pan(pchannel->get_cur_pan() - 0x3f);
        pwb->set_volume(cur_channel_volume);
    } else {
        pwmgr->CWaveFile::m_pan = pchannel->get_cur_pan() - 0x3f;
        pwmgr->set_volume(cur_channel_volume);
    }
    if (pwmgr->is_static())
        pwmgr->CWaveFile::play(instance);
    if (!pwmgr->is_active()) {
        pwmgr->set_active(1);
        gbl_pwave_device->m_play_list.Append(pwmgr);
    }
    return 2;
}

unsigned long CMidiMgr::note_off(unsigned char *pdata, unsigned long cur_channel, CMidiTrack *ptrack)
{
    unsigned long cur_patch = m_channels[cur_channel].get_cur_patch();
    int i = pdata[0];
    MIDI_NOTE note = (MIDI_NOTE)pdata[0];
    if (ptrack->get_ctrl() != 0)
        note = (MIDI_NOTE)ptrack->start_control(note);
    if (gbl_pmidi_device->hmidi_out() != 0) {
        midiOutShortMsg(gbl_pmidi_device->hmidi_out(), pack_midi_msg(0x80, cur_channel, note, pdata[1]));
        ptrack->m_hw_on_off_table[note].set_packed(0);
        return 2;
    }
    CProgramMgr *pcur_prgm = m_patch_table[cur_patch];
    CWaveMgr *pwm = ptrack->m_on_off_table[i];
    if (pwm == 0 || !pcur_prgm->note_off())
        return 2;
    ptrack->m_on_off_table[i] = 0;
    pwm->m_midi_note = 0;
    if (pwm->is_static()) {
        WaveBuffer *pwb = pwm->find_midi_instance(pdata[0]);
        if (pwb)
            pwb->fade();
        else {
            int big_fuck_up;
            big_fuck_up = 1;
        }
    } else {
        Dll_KeyMap *pkm = pcur_prgm->get_keymap();
        if (pkm)
            pwm->CWaveFile::fade(pkm->m_release_time);
        else
            pwm->fade();
    }
    return 2;
}

unsigned long CMidiMgr::meta_event(unsigned char *pdata, CMidiTrack *cur_track)
{
    unsigned long bytes_used;
    unsigned long type = pdata[0];
    unsigned long length = varlen2dw(pdata + 1, &bytes_used);
    unsigned long shift_amout = 0;
    unsigned long new_byte = 0;
    unsigned char *ptempo = bytes_used + pdata + 1;
    unsigned long tempo = 0;
    int i, j;
    float ftempo;
    unsigned char nominator, denominator, midi_clocks, n32nds_per_quarter;
    DllTrackSet *pts;

    switch (type) {
    case 0x51:
        for (i = 2, j = 0; i >= 0; i--, j += 8) {
            new_byte = ptempo[i];
            new_byte <<= j;
            tempo |= new_byte;
        }
        ftempo = (float)tempo;
        ftempo = 60000000.0f / ftempo;
        set_tempo((unsigned long)ftempo);
        break;

    case 0x58:
        denominator = ptempo[1];
        nominator = ptempo[0];
        midi_clocks = ptempo[2];
        n32nds_per_quarter = ptempo[3];
        goto done;

    case 0x59:
        m_key = be2le(*(unsigned short *)(pdata + 2));
        break;

    case 0x2f:
        if (cur_track->is_looping()) {
            cur_track->reset();
            prepare_track(cur_track);
            cur_track->m_tick_offset = m_time_ticks;
            pts = (DllTrackSet *)get_trackset(m_current_active_set);
            if (pts == 0 || pts->m_loops == 0)
                break;
            pts->m_loop_ctrl++;
            if (pts->m_loop_ctrl < pts->get_ntracks())
                break;
            pts->m_loop_ctrl = 0;
            pts->m_loop_count++;
            if (pts->m_loop_count < pts->m_loops)
                break;
            pts->m_loop_count = 0;
            m_current_active_set = pts->m_linked_set;
            set_active_tracks();
        } else {
            cur_track->set_done();
            if (cur_track->m_ptrackset != 0)
                cur_track->m_ptrackset->m_ntracks_playing--;
            if (cur_track->is_switcher()) {
                if (cur_track->is_active_switcher())
                    switch_track();
                cur_track->reset();
            }
        }
        break;
    }

done:
    return bytes_used + length + 1;
}

unsigned long CMidiMgr::pitch_wheel(unsigned char *pdata, unsigned long cur_channel)
{
    return 1;
}

inline SNDERR CMidiMgr::set_tempo(unsigned long tempo)
{
    float ftempo = (float)tempo;
    float fticks_per_minute = ftempo * get_division();
    float fticks_per_second = fticks_per_minute / 60.0f;
    float fms_per_tick = 1000.0f / fticks_per_second;
    m_ms_per_tick = fms_per_tick;
    float runningtime_scaled = m_tempo / (float)tempo;
    m_tempo = tempo;
    float runningtime = (float)m_ms_running_time * runningtime_scaled;
    m_ms_running_time = (unsigned long)runningtime;
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_program_ctrl(ProgramCtrl *pctrl)
{
    if (pctrl == 0)
        return SNDERR_BAD_PARAM;

    CProgramMgr *pprgm = m_patch_table[pctrl->get_patch_num()];
    if (pprgm != 0) {
        pprgm->set_ctrl(pctrl);
        if (pctrl->get_keymap() == 0)
            return SNDERR_OK;
        pprgm->ProgramCtrl::set_keymap(pctrl->get_keymap());

        if (pprgm->get_keymap() != 0 && strcmp(pprgm->get_keymap()->get_desc(), pctrl->get_keymap()) == 0)
            return SNDERR_OK;

        Dll_KeyMap *pkm = m_keymap_list.get_first();
        while (pkm != 0) {
            if (strcmp(pkm->get_desc(), pctrl->get_keymap()) == 0) {
                if (pprgm->get_keymap() != 0) {
                    if (pprgm->get_keymap()->dec_ref() == 0) {
                        m_keymap_list.remove(pprgm->get_keymap());
                        delete pprgm->get_keymap();
                    }
                    pprgm->set_keymap(0);
                }
                pprgm->set_keymap(pkm);
                pkm->inc_ref();
                return SNDERR_OK;
            }
            pkm = m_keymap_list.get_next();
        }
        return SNDERR_OK;
    }

    map_patch(pctrl, 0);
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_control_track(unsigned long track)
{
    if (track > get_ntracks() - 1)
        return SNDERR_BAD_PARAM;
    m_control_track = track;
    return SNDERR_OK;
}

SNDERR CMidiMgr::reset()
{
    CMidiFile::reset();
    m_flags.stop_pending = 0;
    m_flags.reset_pending = 0;
    m_flic_frame = 0;
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_channel_ctrl(ChannelCtrl *pctrl)
{
    if (pctrl == 0)
        return SNDERR_BAD_PARAM;
    CMidiChannel *pchannel = &m_channels[pctrl->get_channel_num()];
    pchannel->set_ctrl(pctrl);
    return SNDERR_OK;
}

SNDERR CMidiMgr::add_keymap(KeyMap *pkm)
{
    if (pkm == 0)
        return SNDERR_BAD_PARAM;
    Dll_KeyMap *pnewkm;
    Dll_KeyMap *pdll_km = m_keymap_list.get_first();
    while (pdll_km != 0) {
        if (strcmp(pdll_km->get_desc(), pkm->get_name()) == 0)
            break;
        pdll_km = m_keymap_list.get_next();
    }
    if (pdll_km != 0) {
        pnewkm = pdll_km;
    } else {
        pnewkm = new Dll_KeyMap;
        pnewkm->set_desc(pkm->get_name());
        {
            int i = 0;
            do {
                KeyMapWave *pkmw = pkm->get_key_wave(i);
                if (pkmw != 0) {
                    unsigned long keylo = pkmw->get_keylo();
                    unsigned long keyhi = pkmw->get_keyhi();
                    pnewkm->map_range(pkmw);
                    i = keyhi + 1;
                } else {
                    i++;
                }
            } while (i < 0x80);
        }

        pnewkm->set_release_time(pkm->m_release_time);
        if (pkm->m_flags.cycle)
            pnewkm->set_cycle(pkm->m_cycle_lo, pkm->m_cycle_hi);
        if (pkm->m_flags.ignore_velocity)
            pnewkm->m_flags.ignore_velocity = 1;
        m_keymap_list.add_to_end(pnewkm);
    }
    for (int i = 0; i < 0x80; i++) {
        CProgramMgr *pcurprgm = m_patch_table[i];
        if (pcurprgm != 0 && pcurprgm->ProgramCtrl::get_keymap() != 0) {
            if (strcmp(pnewkm->get_desc(), pcurprgm->ProgramCtrl::get_keymap()) == 0) {
                if (pcurprgm->get_keymap() != 0
                    && strcmp(pcurprgm->get_keymap()->get_desc(), pnewkm->get_desc()) != 0) {
                    if (pcurprgm->get_keymap()->dec_ref() == 0) {
                        m_keymap_list.remove(pcurprgm->get_keymap());
                        delete pcurprgm->get_keymap();
                    }
                    pcurprgm->set_keymap(0);
                } else if (pcurprgm->get_keymap() == 0) {
                    pcurprgm->set_keymap(pnewkm);
                    pnewkm->inc_ref();
                }
            }
        }
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_switch_type(unsigned long switch_type)
{
    if (switch_type > 2)
        return SNDERR_BAD_PARAM;
    m_flags.switch_type = switch_type;
    reset_switch_table(m_switch_table);
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_nswitch_threads(unsigned long nswitch_threads)
{
    m_nswitch_threads = nswitch_threads;
    return SNDERR_OK;
}

SNDERR CMidiMgr::add_switch_range(unsigned long tracklo, unsigned long trackhi)
{
    CMidiTrack *ptrack;
    if (is_playing())
        return SNDERR_BUSY;
    if (trackhi < tracklo)
        return SNDERR_BAD_PARAM;
    if (trackhi >= get_ntracks())
        return SNDERR_BAD_PARAM;
    m_switch_tracklo = tracklo;
    m_switch_trackhi = trackhi;
    m_nswitchers = trackhi - tracklo + 1;
    if (m_switch_table)
        ::operator delete(m_switch_table);
    m_switch_table = (int *)::operator new(m_nswitchers * 4);
    reset_switch_table(m_switch_table);
    for (unsigned long i = tracklo; i <= trackhi; i++) {
        if (get_track(&ptrack, i) == 0)
            ptrack->set_switcher();
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::remove_active_trackset(unsigned int id)
{
    DllTrackSet *cur_trackset = m_trackset_list.get_first();
    while (cur_trackset != 0) {
        if (cur_trackset->m_id == id)
            break;
        cur_trackset = m_trackset_list.get_next();
    }
    if (cur_trackset != 0) {
        m_trackset_list.remove(cur_trackset);
        return SNDERR_OK;
    }
    return SNDERR_NOT_PRESENT;
}

SNDERR CMidiMgr::add_active_trackset(TrackSet *pts)
{
    if (pts == 0)
        return SNDERR_BAD_PARAM;

    DllTrackSet *new_trackset = new DllTrackSet;
    static_cast<TrackSet &>(*new_trackset) = *pts;

    CMidiTrack *ptrack = 0;
    for (unsigned i = new_trackset->m_tracklo; i <= new_trackset->m_trackhi; i++) {
        get_track(&ptrack, i);
        if (ptrack != 0)
            ptrack->m_ptrackset = new_trackset;
    }

    if (m_trackset_list.get_nnodes() == 0) {
        set_all_tracks_inactive();
        m_trackset_list.add_to_end(new_trackset);
        m_GT_trackset_list.add_to_end(new_trackset);
        return SNDERR_OK;
    }

    TrackSet *cur_set = m_trackset_list.get_first();
    m_GT_trackset_list.get_first();
    while (cur_set != 0) {
        if (new_trackset->m_id <= cur_set->m_id)
            break;
        cur_set = m_trackset_list.get_next();
        m_GT_trackset_list.get_next();
    }
    if (cur_set != 0) {
        m_trackset_list.get_prev();
        m_trackset_list.insert(new_trackset);
        m_GT_trackset_list.get_prev();
        m_GT_trackset_list.insert(new_trackset);
        return SNDERR_OK;
    }
    m_trackset_list.add_to_end(new_trackset);
    m_GT_trackset_list.add_to_end(new_trackset);
    return SNDERR_OK;
}

SNDERR CMidiMgr::add_active_trackset(DllTrackSet *set)
{
    DllTrackSet *pnew_trackset = new DllTrackSet;
    *pnew_trackset = *set;

    if (m_trackset_list.get_nnodes() == 0) {
        m_trackset_list.add_to_end(pnew_trackset);
        m_GT_trackset_list.add_to_end(pnew_trackset);
    } else {
        TrackSet *cur_set = m_trackset_list.get_first();
        m_GT_trackset_list.get_first();
        while (cur_set != 0) {
            if (pnew_trackset->m_id <= cur_set->m_id)
                break;
            cur_set = m_trackset_list.get_next();
            m_GT_trackset_list.get_next();
        }
        if (cur_set != 0) {
            m_trackset_list.get_prev();
            m_GT_trackset_list.get_prev();
            m_trackset_list.insert(pnew_trackset);
            m_GT_trackset_list.insert(pnew_trackset);
        } else {
            m_trackset_list.add_to_end(pnew_trackset);
            m_GT_trackset_list.add_to_end(pnew_trackset);
        }
    }
    return SNDERR_OK;
}

TrackSet *CMidiMgr::get_trackset(unsigned int set_id)
{
    if (m_trackset_list.get_nnodes() == 0)
        return 0;
    TrackSet *cur_set = m_trackset_list.get_first();
    while (cur_set != 0) {
        if (cur_set->m_id == set_id)
            break;
        cur_set = m_trackset_list.get_next();
    }
    return cur_set;
}

SNDERR CMidiMgr::set_active_range_lo(unsigned int setid, unsigned int lo)
{
    if (m_trackset_list.get_nnodes() == 0)
        return SNDERR_NOT_PRESENT;

    TrackSet *cur_set = m_trackset_list.get_first();
    while (cur_set != 0) {
        if (cur_set->m_id == setid)
            break;
        cur_set = m_trackset_list.get_next();
    }
    if (cur_set == 0)
        return SNDERR_NOT_PRESENT;
    cur_set->set_lo(lo);
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_active_range_hi(unsigned int setid, unsigned int hi)
{
    if (m_trackset_list.get_nnodes() == 0)
        return SNDERR_NOT_PRESENT;

    TrackSet *cur_set = m_trackset_list.get_first();
    while (cur_set != 0) {
        if (cur_set->m_id == setid)
            break;
        cur_set = m_trackset_list.get_next();
    }
    if (cur_set == 0)
        return SNDERR_NOT_PRESENT;
    cur_set->set_hi(hi);
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_active_tracks(unsigned int setid)
{
    if (m_current_active_set != setid) {
        m_current_active_set = setid;
        if (gbl_pmidi_device != 0)
            gbl_pmidi_device->queue_request(this, REQUEST_SET_ACTIVE_TRACKS, 0, 0);
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_active_tracks()
{
    if (m_trackset_list.get_nnodes() == 0)
        return SNDERR_NOT_PRESENT;

    TrackSet *pcur_set = m_trackset_list.get_first();
    while (pcur_set != 0) {
        if (pcur_set->m_id == m_current_active_set)
            break;
        pcur_set = m_trackset_list.get_next();
    }
    if (pcur_set == 0)
        return SNDERR_NOT_PRESENT;

    if (pcur_set->m_tempo != 0)
        set_tempo(pcur_set->m_tempo);
    return set_active_tracks(pcur_set->m_tracklo, pcur_set->m_trackhi);
}

SNDERR CMidiMgr::set_active_tracks(unsigned long lo, unsigned long hi)
{
    CMidiTrack *ptrack;

    if (hi >= lo) {
        if (hi < get_ntracks()) {

            if (get_track(&ptrack, 0) != 0)
                ptrack->m_flags.active = 1;

            if (is_playing() == 0) {
                for (unsigned long i = 1; i <= get_ntracks(); i++) {
                    if (get_track(&ptrack, i) == 0) {
                        if (i >= lo && i <= hi)
                            ptrack->m_flags.active = 1;
                        else
                            ptrack->m_flags.active = 0;
                    }
                }
            } else {
                for (unsigned long i = 1; i <= get_ntracks(); i++) {
                    if (get_track(&ptrack, i) == 0) {
                        if (i >= lo && i <= hi) {
                            ptrack->m_flags.active = 1;
                            set_tick_offset(ptrack, m_time_ticks, 0x20, 0);
                        } else {
                            ptrack->m_flags.active = 0;
                            fade_track(i);
                        }
                    }
                }
            }
            return SNDERR_OK;
        }
    }
    return SNDERR_BAD_PARAM;
}

SNDERR CMidiMgr::play_trackset(unsigned int setid)
{
    if (m_current_active_set != setid) {
        if (gbl_pmidi_device != 0)
            gbl_pmidi_device->queue_request(this, REQUEST_PLAY_TRACKSET, setid, 0);
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::xpose_trackset(unsigned int setid, int pitch)
{
    if (gbl_pmidi_device != 0)
        gbl_pmidi_device->queue_request(this, REQUEST_XPOSE_TRACKSET, setid, pitch);
    return SNDERR_OK;
}

bool CMidiMgr::is_trackset_playing(unsigned int setid)
{
    if (m_GT_trackset_list.get_nnodes() == 0)
        return 0;
    DllTrackSet *pcur_set = m_GT_trackset_list.get_first();
    while (pcur_set != 0) {
        if (pcur_set->m_id == setid)
            break;
        pcur_set = m_GT_trackset_list.get_next();
    }
    if (pcur_set == 0)
        return 0;
    return pcur_set->m_ntracks_playing ? true : false;
}

SNDERR CMidiMgr::TT_play_trackset(unsigned int setid)
{
    if (m_trackset_list.get_nnodes() == 0)
        return SNDERR_NOT_PRESENT;
    if (m_current_active_set == setid)
        return SNDERR_OK;
    TrackSet *pcur_set = m_trackset_list.get_first();
    while (pcur_set != 0) {
        if (pcur_set->m_id == setid)
            break;
        pcur_set = m_trackset_list.get_next();
    }
    if (pcur_set == 0)
        return SNDERR_NOT_PRESENT;
    if (pcur_set->m_tempo != 0)
        set_tempo(pcur_set->m_tempo);
    return TT_play_trackset(pcur_set);
}

SNDERR CMidiMgr::TT_play_trackset(TrackSet *pcur_set)
{
    unsigned hi = pcur_set->m_trackhi;
    unsigned lo = pcur_set->m_tracklo;
    if (hi >= lo && hi < get_ntracks()) {
        CMidiTrack *ptrack;
        if (get_track(&ptrack, 0) != 0)
            ptrack->m_flags.active = 1;
        if (is_playing() == 0) {
            for (unsigned long i = lo; i <= hi; i++) {
                if (get_track(&ptrack, i) == 0)
                    ptrack->m_flags.active = 1;
            }
        } else {
            for (unsigned long i = lo; i <= hi; i++) {
                if (get_track(&ptrack, i) == 0 && ptrack != 0) {
                    ptrack->m_flags.active = 1;
                    set_tick_offset(ptrack, m_time_ticks, pcur_set->m_flags.all, pcur_set->m_sync_track);
                    if (ptrack->m_ptrackset != 0)
                        ptrack->m_ptrackset->m_ntracks_playing++;
                }
            }
        }
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR CMidiMgr::TT_play_trackset(unsigned lo, unsigned hi, unsigned sync)
{
    if (hi >= lo && hi < get_ntracks()) {
        CMidiTrack *ptrack;
        if (get_track(&ptrack, 0) != 0)
            ptrack->m_flags.active = 1;
        if (is_playing() == 0) {
            for (unsigned long i = lo; i <= hi; ++i)
                if (get_track(&ptrack, i) == 0)
                    ptrack->m_flags.active = 1;
        } else {
            for (unsigned long i = lo; i <= hi; ++i) {
                if (get_track(&ptrack, i) == 0 && ptrack != 0) {
                    ptrack->m_flags.active = 1;
                    set_tick_offset(ptrack, m_time_ticks, sync, 0);
                    if (ptrack->m_ptrackset)
                        ++ptrack->m_ptrackset->m_ntracks_playing;
                }
            }
        }
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR CMidiMgr::stop_trackset(unsigned int setid)
{
    if (gbl_pmidi_device != 0)
        gbl_pmidi_device->queue_request(this, REQUEST_STOP_TRACKSET, setid, 0);
    return SNDERR_OK;
}

SNDERR CMidiMgr::TT_stop_trackset(unsigned int setid)
{
    DllTrackSet *pcur_set;

    if (m_trackset_list.get_nnodes() == 0)
        return SNDERR_NOT_PRESENT;

    for (pcur_set = m_trackset_list.get_first(); pcur_set; pcur_set = m_trackset_list.get_next())
        if (pcur_set->m_id == setid)
            break;
    if (pcur_set == 0)
        return SNDERR_NOT_PRESENT;

    pcur_set->m_ntracks_playing = 0;
    return TT_stop_trackset(pcur_set->m_tracklo, pcur_set->m_trackhi);
}

SNDERR CMidiMgr::TT_stop_trackset(unsigned lo, unsigned hi)
{
    if (hi >= lo && hi < get_ntracks()) {
        CMidiTrack *ptrack;
        if (get_track(&ptrack, 0) != 0)
            ptrack->m_flags.active = 1;
        if (is_playing() == 0) {
            for (unsigned long i = lo; i <= hi; i++)
                if (get_track(&ptrack, i) == 0)
                    ptrack->m_flags.active = 0;
        } else {
            for (unsigned long i = lo; i <= hi; i++) {
                if (get_track(&ptrack, i) == 0 && ptrack->m_flags.active) {
                    ptrack->m_flags.active = 0;
                    fade_track(i);
                    ptrack->reset();
                }
            }
        }
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR CMidiMgr::TT_xpose_trackset(unsigned setid, int pitch)
{
    if (m_trackset_list.get_nnodes() == 0)
        return (SNDERR)0x24;
    TrackSet *pcur_set = m_trackset_list.get_first();
    while (pcur_set != 0) {
        if (pcur_set->m_id == setid)
            break;
        pcur_set = m_trackset_list.get_next();
    }
    if (pcur_set == 0)
        return (SNDERR)0x24;
    return TT_xpose_trackset(pcur_set->m_tracklo, pcur_set->m_trackhi, pitch);
}

SNDERR CMidiMgr::TT_xpose_trackset(unsigned lo, unsigned hi, int pitch)
{
    if (hi >= lo && hi < get_ntracks()) {
        CMidiTrack *ptrack;
        if (get_track(&ptrack, 0) != 0)
            ptrack->m_flags.active = 1;
        for (unsigned long i = lo; i <= hi; i++) {
            if (get_track(&ptrack, i) == 0)
                ptrack->m_pitch_shift = pitch;
        }
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR CMidiMgr::TT_set_track_loop_state(unsigned int track, bool state)
{
    CMidiTrack *ptrack;
    SNDERR ec = get_track(&ptrack, track);
    if (ec == SNDERR_OK)
        ptrack->set_loop_state(state);
    return ec;
}

SNDERR CMidiMgr::TT_set_trackset_done_callback(unsigned int setid, void(__cdecl *pcallback)(void *))
{
    DllTrackSet *pcur_set;

    if (m_trackset_list.get_nnodes() == 0)
        return SNDERR_NOT_PRESENT;

    pcur_set = m_trackset_list.get_first();
    while (pcur_set) {
        if (pcur_set->m_id == setid)
            break;
        pcur_set = m_trackset_list.get_next();
    }

    if (pcur_set == 0)
        return SNDERR_NOT_PRESENT;

    pcur_set->m_pdone_callback = pcallback;
    return SNDERR_OK;
}

void CMidiMgr::reset_switch_table(int *table)
{
    if (m_flags.switch_type != 2) {
        g_rand.reseed(timeGetTime());
        for (int i = 0; i < m_nswitchers; i++)
            table[i] = (unsigned)(long)g_rand.get() % (m_switch_trackhi - m_switch_tracklo + 1) + m_switch_tracklo;
    } else {
        unsigned long cur_track = m_switch_tracklo;
        for (int i = 0; i < m_nswitchers; i++) {
            table[i] = cur_track;
            if (++cur_track > m_switch_trackhi)
                cur_track = m_switch_tracklo;
        }
    }
    m_switch_index = 0;
}

SNDERR CMidiMgr::set_track_ctrl(TrackCtrl *pctrl)
{
    if (pctrl != 0 && (unsigned)pctrl->m_trk_num < get_ntracks()) {
        CMidiTrack *ptrack;
        get_track(&ptrack, pctrl->m_trk_num);
        ptrack->set_loop_state(pctrl->m_flags.looping);
        ptrack->m_velocity_offset = pctrl->m_velocity_offset;
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR CMidiMgr::set_track_loop_state(unsigned int track, bool state)
{
    SNDERR retval = SNDERR_OK;
    if (track > get_ntracks())
        return SNDERR_BAD_PARAM;
    if (gbl_pmidi_device != 0)
        retval = gbl_pmidi_device->queue_request(this, REQUEST_SET_TRACK_LOOP_STATE, track, state);
    else
        retval = SNDERR_UNINITIALIZED;
    return retval;
}

bool CMidiMgr::get_track_loop_state(unsigned int track)
{
    CMidiTrack *ptrack;
    if (track > get_ntracks())
        return 0;
    get_track(&ptrack, track);
    return ptrack->is_looping();
}

SNDERR CMidiMgr::get_track_velocity_offset(unsigned int track, int *pvelocity_offset)
{
    CMidiTrack *ptrack;
    if (track > get_ntracks())
        return SNDERR_BAD_PARAM;
    get_track(&ptrack, track);
    *pvelocity_offset = ptrack->m_velocity_offset;
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_track_velocity_offset(unsigned int track, int velocity_offset)
{
    CMidiTrack *ptrack;
    if (track > get_ntracks())
        return SNDERR_BAD_PARAM;
    get_track(&ptrack, track);
    ptrack->m_velocity_offset = velocity_offset;
    return SNDERR_OK;
}

SNDERR CMidiMgr::set_control(void *pcontrol, int target_type, int target)
{
    MidiCtrl *pnew_ctrl = 0;
    CMidiTrack *ptrack;
    SNDERR retval = SNDERR_BAD_PARAM;
    if (pcontrol == 0)
        return retval;
    if (target_type == 2)
        return set_program_ctrl((ProgramCtrl *)pcontrol);
    switch (((MidiCtrl *)pcontrol)->m_type) {
    case PROGRAM_CONTROL:
        return set_program_ctrl((ProgramCtrl *)pcontrol);
    case TRACK_CONTROL:
        return set_track_ctrl((TrackCtrl *)pcontrol);
    case CHANNEL_CONTROL:
        return set_channel_ctrl((ChannelCtrl *)pcontrol);
    case MIDI_NOTEMAP:
    case MIDI_RANDOMIZER: {
        if (target_type == 0 && target >= 0) {
            retval = get_track(&ptrack, target);
            if (retval == 0)
                retval = ptrack->attach_ctrl((MidiCtrl *)pcontrol);
        }
        break;
    }
    default:
        break;
    }
    return retval;
}

void *CMidiMgr::get_control_target(int target_type, int target)
{
    MidiCtrl *pnew_ctrl = 0;
    CMidiTrack *ptrack;
    if (target_type == 0 && target >= 0 && get_track(&ptrack, target) == 0)
        return ptrack;
    return 0;
}

CProgramMgr::CProgramMgr()
{
    m_rnd_pitch_index = 0;
    m_rnd_volume_index = 0;
    m_pkeymap = 0;
}

inline CProgramMgr::~CProgramMgr()
{
    set_keymap(0);
}

SNDERR CProgramMgr::load(ProgramCtrl *pctrl)
{
    SNDERR retval = SNDERR_OK;
    if (pctrl == 0)
        return SNDERR_BAD_PARAM;
    set_patch_num(pctrl->get_patch_num());
    set_pitch_hi(pctrl->get_pitch_hi());
    set_pitch_lo(pctrl->get_pitch_lo());
    set_vol_hi(pctrl->get_vol_hi());
    set_vol_lo(pctrl->get_vol_lo());
    set_randomize_pitch(pctrl->is_randomize_pitch());
    set_randomize_vol(pctrl->is_randomize_vol());
    set_fname(pctrl->get_fname());
    return SNDERR_NO_SUPPORT;
}

SNDERR CProgramMgr::unload()
{
    return SNDERR_OK;
}

SNDERR CProgramMgr::stop()
{
    if (gbl_pwave_device == 0)
        return SNDERR_NO_DEVICE;
    if (m_pkeymap != 0)
        m_pkeymap->stop_all();
    return SNDERR_OK;
}

void CProgramMgr::set_ctrl(ProgramCtrl *pctrl)
{
    if (pctrl == 0)
        return;
    set_patch_num(pctrl->get_patch_num());
    if (pctrl->is_randomize_pitch()) {
        set_randomize_pitch(1);
        set_pitch_hi(pctrl->get_pitch_hi());
        set_pitch_lo(pctrl->get_pitch_lo());
    }
    if (pctrl->is_randomize_vol()) {
        set_randomize_vol(1);
        set_vol_hi(pctrl->get_vol_hi());
        set_vol_lo(pctrl->get_vol_lo());
    }
    if (pctrl->note_off())
        set_note_off(1);
    else
        set_note_off(0);
    if (!pctrl->is_pitch_mapped())
        set_pitch_map_state(false);
    else
        set_pitch_map_state(true);
    if (pctrl->get_fname())
        set_fname(pctrl->get_fname());
}

int CProgramMgr::xlat_pitch(int pitch)
{
    return pitch;
}

unsigned long CProgramMgr::xlat_volume(unsigned long volume)
{
    return volume;
}

CMidiChannel::CMidiChannel()
{
    m_cur_volume = 0x7f;
    m_cur_status = 0;
    m_cur_pitch = 0;
    m_cur_patch = 0;
    m_cur_pan = 0x40;
}

CMidiChannel::~CMidiChannel()
{
}

void CMidiChannel::reset_rnd_prgm_table()
{
}

unsigned long CMidiChannel::xlat_program(unsigned long program_number)
{
    unsigned long new_prgm_num = 0;
    return program_number;
}

SNDERR CMidiMgr::set_track(unsigned char *pdata, unsigned long tracknum)
{
    CMidiTrack *ptrack;
    CMidiTrack *pcontrol_track;
    SNDERR result = SNDERR_OK;
    result = get_track(&pcontrol_track, m_control_track);
    if (result != 0)
        pcontrol_track = 0;
    result = get_track(&ptrack, tracknum);
    if (result != 0)
        return result;
    ptrack->set_pdata(pdata);
    ptrack->set_own_data(0);
    if (pcontrol_track) {
        if (pcontrol_track->get_total_ticks() < ptrack->get_total_ticks())
            m_control_track = tracknum;
    } else
        m_control_track = tracknum;
    prepare_track(tracknum);
    return result;
}

SNDERR CMidiMgr::unmute_track(unsigned long tracknum)
{
    CWaveMgr *pwmgr;
    CMidiTrack *ptrack;
    SNDERR retval;
    int i;
    retval = get_track(&ptrack, tracknum);
    if (retval != 0)
        return retval;
    pwmgr = 0;
    ptrack->unmute();
    for (i = 127; i >= 0; i--) {
        pwmgr = ptrack->m_on_off_table[i];
        if (pwmgr != 0)
            pwmgr->ramp(0x7f, -1, 0x19);
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::mute_track(unsigned long tracknum)
{
    CMidiTrack *ptrack;
    SNDERR retval = get_track(&ptrack, tracknum);
    if (retval != 0)
        return retval;
    ptrack->mute();
    for (int i = 0x7f; i >= 0; --i) {
        CWaveMgr *pwmgr = ptrack->m_on_off_table[i];
        if (pwmgr != 0)
            pwmgr->ramp(0, 0x7f, 0x19);
    }
    return SNDERR_OK;
}

SNDERR CMidiMgr::fade_track(unsigned long tracknum)
{
    CMidiTrack *ptrack;
    SNDERR retval = get_track(&ptrack, tracknum);
    if (retval != 0)
        return retval;
    if (gbl_pmidi_device->hmidi_out() != 0) {
        for (int i = 0; i < 128; i++) {
            if (ptrack->m_hw_on_off_table[i].get_status() != 0) {
                ptrack->m_hw_on_off_table[i].set_d2(0);
                midiOutShortMsg(gbl_pmidi_device->hmidi_out(), ptrack->m_hw_on_off_table[i].get_packed());
                ptrack->m_hw_on_off_table[i].set_packed(0);
            }
        }
        return SNDERR_OK;
    }
    for (int i = 0; i < 128; i++) {
        CWaveMgr *pwmgr = ptrack->m_on_off_table[i];
        if (pwmgr != 0) {
            ptrack->m_on_off_table[i] = 0;
            pwmgr->fade();
            if (pwmgr->m_nvoices > 1) {
                for (int j = 0; j < pwmgr->m_nvoices - 1; j++)
                    pwmgr->m_voices[j]->fade();
            }
        }
    }
    return SNDERR_OK;
}

void CMidiMgr::switch_track()
{
    static int cur_switch_index = 0;
    if (m_nswitchers == 0)
        return;
    CMidiTrack *ptrack;
    ++cur_switch_index;
    if (cur_switch_index >= m_nswitchers)
        cur_switch_index = 0;
    unsigned char switched = 0;
    int cur_switch_track = m_switch_table[cur_switch_index];
    if (get_track(&ptrack, cur_switch_track) == 0) {
        int i = 0;
        while (i++ < m_nswitchers) {
            if (!ptrack->is_active_switcher()) {
                ptrack->set_active_switcher(true);
                switched = 1;
                break;
            }
            ++cur_switch_index;
            if (cur_switch_index >= m_nswitchers)
                cur_switch_index = 0;
            cur_switch_track = m_switch_table[cur_switch_index];
            if (get_track(&ptrack, cur_switch_track) != 0) {
                switched = 1;
                break;
            }
        }
    }
    set_tick_offset(ptrack, m_time_ticks, 0x20, 0);
}

void CMidiMgr::set_base_path(const char *p)
{
    int i;
    CProgramMgr *pprgm;
    for (i = 0; i < 0x80; i++)
        pprgm = m_patch_table[i];
    for (Dll_KeyMap *pkm = m_keymap_list.get_first(); pkm != 0; pkm = m_keymap_list.get_next())
        pkm->set_base_path(p);
}

SNDERR CMidiMgr::seek_to_prior(CMidiTrack *ptrack, unsigned long seek_ticks)
{
    unsigned long ticks = 0;
    unsigned long bytes_used = 0;
    unsigned char *midi_stream = (unsigned char *)ptrack->m_pdata;
    unsigned long offset = 0;
    unsigned long cur_status = 0;
    unsigned char got_one = 0;
    unsigned char *pcur_event;
    unsigned long event_ticks;
    unsigned long varlength;
    int done = 0;

    while (!done) {
        event_ticks = varlen2dw(midi_stream, &bytes_used);
        if (ticks + event_ticks > seek_ticks) {
            if (!got_one)
                return SNDERR_END_OF_TRACK;
            done = 1;
            continue;
        }
        ticks += event_ticks;
        pcur_event = midi_stream;
        midi_stream += bytes_used;
        offset += bytes_used;
        got_one = 1;

        if (*midi_stream & 0x80) {
            cur_status = *midi_stream;
            ptrack->m_cur_status = (unsigned char)cur_status;
            if ((cur_status & 0xf0) != 0xf0) {
                ptrack->m_cur_channel = cur_status & 0x0f;
                cur_status = cur_status & 0xf0;
            }
            midi_stream++;
            offset++;
        }

        switch (cur_status) {
        case 0xff:
            if (*midi_stream == 0x2f) {
                ptrack->m_pcur_event = pcur_event;
                ptrack->m_ticks = ticks;
                return SNDERR_END_OF_TRACK;
            }
            midi_stream++;
            offset++;
            varlength = varlen2dw(midi_stream, &bytes_used);
            midi_stream += varlength + bytes_used;
            offset += varlength + bytes_used;
            continue;

        case 0xc0:
        case 0xe0:
            midi_stream++;
            offset++;
            continue;

        case 0xb0:
            if (midi_stream[0] == 7)
                m_channels[ptrack->m_cur_channel].set_cur_volume(midi_stream[1]);
        case 0x80:
        case 0x90:
            midi_stream += 2;
            offset += 2;
            continue;

        default:
            return SNDERR_BAD_FORMAT;
        }
    }

    ptrack->m_cur_offset = offset;
    ptrack->m_pcur_event = midi_stream;
    ptrack->m_ticks = ticks;
    return SNDERR_OK;
}

void CMidiMgr::set_tick_offset(CMidiTrack *ptrack, unsigned long offset, unsigned long flags, int sync_track)
{
    unsigned long total_ticks = ptrack->m_total_ticks;

    if (sync_track > 0) {
        CMidiTrack *psync_track;
        if (get_track(&psync_track, sync_track) == 0)
            total_ticks = psync_track->m_total_ticks;
    }

    int rem = 0;
    int div = 0;
    int togo = 0;
    if (total_ticks != 0) {
        if (flags & 1) {
            ptrack->m_tick_offset = total_ticks - offset % total_ticks + offset;
        } else if (flags & 4) {
            div = offset / get_division() + 1;
            ptrack->m_tick_offset = div * get_division();
        } else if (flags & 0x10) {
            ptrack->m_tick_offset = offset;
        } else {
            ptrack->m_tick_offset = total_ticks * div;
        }

        if (flags & 0x20)
            seek_to_prior(ptrack, rem);
    }
}

void CMidiMgr::set_play_type(unsigned int play_type)
{
    if (play_type & 1) {
        m_flags.looping = 1;
        m_flags.continuous = 0;
        m_flags.stopping = 0;
        return;
    }
    if (play_type & 2) {
        m_flags.continuous = 1;
        m_flags.looping = 0;
        m_flags.stopping = 0;
        return;
    }
    if (play_type & 4) {
        m_flags.stopping = 1;
        m_flags.looping = 0;
        m_flags.continuous = 0;
        return;
    }
}

SNDERR CMidiMgr::load_patch(unsigned int patch_num)
{
    return SNDERR_UNDEFINED;
}

SNDERR CMidiMgr::unload_patch(unsigned int patch_num)
{
    return SNDERR_UNDEFINED;
}

SNDERR CMidiMgr::set_patch(unsigned int midi_channel, unsigned int patch_num)
{
    if (midi_channel > 0xf || patch_num > 0x7f)
        return SNDERR_BAD_PARAM;
    m_channels[midi_channel].set_cur_patch(patch_num);
    return SNDERR_OK;
}

#include "SndNodeList.inl"
