#include "MidiSequence.h"
#include "dlist.h"
#include "ctimer.h"
#include "dllmididevice.h"
#include "midifile.h"

#pragma intrinsic(memset, strcpy)

CMidiFile::CMidiFile()
{
    memset(&m_hfile, 0, sizeof(m_hfile));
    memset(&m_mmioinfo, 0, 0x48);
    memset(&m_flags, 0, 4);
    memset(&m_header, 0, sizeof m_header);
    m_header.division = 120;

    m_curstartpos = 0;
    m_curendpos = 0;
    m_curpos = 0;
    m_curfpos = 0;
    m_hwnd = 0;
    m_volume = 0x10000;
    m_reset_volume = 0x10000;

    set_fade(1000);

    m_pan = 0;
    m_time = 0;
    m_delay = 0;
    m_time_past = 0;
    m_loop_count = 0;
    m_loop_startpos = 0;
}

void CMidiFile::undo_load()
{
    if (m_hfile != 0) {
        mmioClose(m_hfile, 0);
        --gbl_mmio;
    }
    m_hfile = 0;
    memset(&m_mmioinfo, 0, 0x48);
    memset(&m_flags, 0, 4);

    m_curstartpos = 0;
    m_curendpos = 0;
    m_curpos = 0;
    m_curfpos = 0;
    m_hwnd = 0;
    m_volume = 0x10000;
    m_reset_volume = 0x10000;

    set_fade(1000);

    m_pan = 0;
    m_time = 0;
    m_delay = 0;
    m_time_past = 0;
    m_loop_count = 0;
    m_loop_startpos = 0;
}

SNDERR CMidiFile::get_pcur_event(unsigned char **ppevent, unsigned long track)
{
    unsigned long cur_track = 0;
    CMidiTrack *pcur_track = m_track_list.get_first();

    while (cur_track < track) {
        pcur_track = m_track_list.get_next();
        if (pcur_track == 0)
            break;
        cur_track++;
    }
    if (pcur_track == 0)
        return SNDERR_NOT_PRESENT;
    *ppevent = pcur_track->m_pcur_event;
    return SNDERR_OK;
}

SNDERR CMidiFile::get_pfirst_event(unsigned char **ppevent, unsigned long track)
{
    unsigned long cur_track = 0;
    CMidiTrack *pcur_track = m_track_list.get_first();

    while (cur_track < track) {
        pcur_track = m_track_list.get_next();
        if (pcur_track == 0)
            break;
        cur_track++;
    }
    if (pcur_track == 0)
        return SNDERR_NOT_PRESENT;
    pcur_track->reset();
    *ppevent = pcur_track->m_pcur_event;
    return SNDERR_OK;
}

SNDERR CMidiFile::advance_track(unsigned long distance, unsigned long track)
{
    CMidiTrack *ptrack;
    SNDERR retval = get_track(&ptrack, track);
    if (retval != 0)
        return retval;
    if (!ptrack->at_end())
        retval = ptrack->advance(distance);
    return retval;
}

SNDERR CMidiFile::get_track(CMidiTrack **pptrack, unsigned long track)
{
    unsigned long cur_track = 0;
    *pptrack = m_track_list.get_first();

    while (cur_track < track) {
        *pptrack = m_track_list.get_next();
        if (*pptrack == 0)
            break;
        cur_track++;
    }
    if (*pptrack == 0)
        return SNDERR_NOT_PRESENT;
    return SNDERR_OK;
}

struct MidiCk_tag {
    unsigned long id;
    unsigned long length;
};

SNDERR CMidiFile::load(const char *fname)
{
    unsigned long MThd = mmioStringToFOURCCA("MThd", 0);
    unsigned long MTrk = mmioStringToFOURCCA("MTrk", 0);
    MidiCk_tag track_hdr;
    SNDERR retval;
    unsigned long i;
    CMidiTrack *pnew_track;
    CMidiTrack *pprev_track;
    unsigned long cur_id;

    if (m_flags.loaded) {
        retval = unload();
        if (retval != SNDERR_OK)
            return retval;
    }

    m_hfile = 0;
    memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
    m_curstartpos = 0;
    m_curendpos = 0;
    m_curpos = 0;
    m_curfpos = 0;
    strcpy(m_fname, fname);
    m_hfile = mmioOpenA(m_fname, 0, MMIO_READ);
    if (m_hfile == 0) {
        memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
        return SNDERR_BAD_FILE_OPEN;
    }

    mmioRead(m_hfile, (HPSTR)&cur_id, 4);
    if (cur_id != MThd) {
        undo_load();
        return SNDERR_BAD_FORMAT;
    }

    gbl_mmio = gbl_mmio + 1;

    mmioRead(m_hfile, (HPSTR)&m_header.length, 4);
    m_header.length = be2le(m_header.length);
    m_flags.loaded = 1;

    mmioRead(m_hfile, (HPSTR)&m_header.format, 2);
    m_header.format = be2le((unsigned short)m_header.format);

    mmioRead(m_hfile, (HPSTR)&m_header.ntrks, 2);
    m_header.ntrks = be2le((unsigned short)m_header.ntrks);

    mmioRead(m_hfile, (HPSTR)&m_header.division, 2);
    m_header.division = be2le((unsigned short)m_header.division);

    pnew_track = 0;
    pprev_track = 0;
    m_control_track = 0;

    for (i = 0; i < m_header.ntrks; i++) {
        pnew_track = new CMidiTrack;

        mmioRead(m_hfile, (HPSTR)&track_hdr, 8);
        track_hdr.length = be2le(track_hdr.length);
        pnew_track->set_pdata((unsigned char *)::operator new(track_hdr.length));
        pnew_track->set_own_data(1);
        pnew_track->m_cur_offset = 0;
        pnew_track->set_length(track_hdr.length);
        mmioRead(m_hfile, (HPSTR)pnew_track->get_pdata(), track_hdr.length);

        m_track_list.add_to_end(pnew_track);
        pnew_track->reset();
        pnew_track->calc_total_ticks();

        if (pprev_track != 0 && pnew_track->get_total_ticks() > pprev_track->get_total_ticks())
            m_control_track = i;
        pprev_track = pnew_track;
    }

    mmioClose(m_hfile, 0);
    gbl_mmio = gbl_mmio - 1;
    m_hfile = 0;
    return SNDERR_OK;
}

SNDERR CMidiFile::load(HMMIO hfile)
{
    unsigned long MThd = mmioStringToFOURCCA("MThd", 0);
    unsigned long MTrk = mmioStringToFOURCCA("MTrk", 0);
    MidiCk_tag track_hdr;
    SNDERR retval;
    unsigned long i;
    long ret;
    CMidiTrack *pnew_track;
    CMidiTrack *pprev_track;
    unsigned char *pdata;
    unsigned long cur_id;

    if (m_flags.loaded) {
        retval = unload();
        if (retval != SNDERR_OK)
            return retval;
    }

    m_hfile = hfile;
    memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
    m_curstartpos = 0;
    m_curendpos = 0;
    m_curpos = 0;
    m_curfpos = 0;
    ret = mmioGetInfo(m_hfile, &m_mmioinfo, 0);

    ret = mmioRead(m_hfile, (HPSTR)&cur_id, 4);
    if (cur_id != MThd) {
        undo_load();
        return SNDERR_BAD_FORMAT;
    }

    mmioRead(m_hfile, (HPSTR)&m_header.length, 4);
    m_header.length = be2le(m_header.length);
    m_flags.loaded = 1;

    mmioRead(m_hfile, (HPSTR)&m_header.format, 2);
    m_header.format = be2le((unsigned short)m_header.format);

    mmioRead(m_hfile, (HPSTR)&m_header.ntrks, 2);
    m_header.ntrks = be2le((unsigned short)m_header.ntrks);

    mmioRead(m_hfile, (HPSTR)&m_header.division, 2);
    m_header.division = be2le((unsigned short)m_header.division);

    pnew_track = 0;
    pprev_track = 0;
    m_control_track = 0;

    for (i = 0; i < m_header.ntrks; i++) {
        pnew_track = new CMidiTrack;

        mmioRead(m_hfile, (HPSTR)&track_hdr, 8);
        track_hdr.length = be2le(track_hdr.length);
        pdata = (unsigned char *)::operator new(track_hdr.length);
        pnew_track->set_own_data(1);
        pnew_track->set_length(track_hdr.length);
        mmioRead(m_hfile, (HPSTR)pdata, track_hdr.length);
        pnew_track->set_pdata(pdata);
        pnew_track->m_cur_offset = 0;

        m_track_list.add_to_end(pnew_track);
        pnew_track->reset();

        if (pprev_track == 0 || pnew_track->get_total_ticks() > pprev_track->get_total_ticks()) {
            pprev_track = pnew_track;
            m_control_track = i;
        }
    }

    m_hfile = 0;
    return SNDERR_OK;
}

SNDERR CMidiFile::reload()
{
    m_flags.loaded = 0;
    return load(m_fname);
}

SNDERR CMidiFile::post_reset()
{
    if (m_flags.reset_pending)
        return SNDERR_OK;
    m_flags.reset_pending = 1;

    if (gbl_pwave_device != 0 && gbl_pwave_device->is_debug())
        PostMessageA(get_hwnd(), MF_ID_RESET, 0, (long)this);
    else
        gbl_sound_thread.post_message(0x7ec, 0, (long)this);

    return SNDERR_OK;
}

SNDERR CMidiFile::reset()
{

    CMidiTrack *pcur_track = m_track_list.get_first();
    while (pcur_track != 0) {
        pcur_track->reset();
        pcur_track = m_track_list.get_next();
    }
    m_flags.reset_pending = 0;
    m_flags.eod = 0;
    m_flags.fading = 0;
    m_flags.done = 0;
    return SNDERR_OK;
}

void CMidiFile::reset_flags()
{
    int looping = m_flags.looping;
    memset(&m_flags, 0, sizeof(m_flags));
    m_flags.looping = looping;

    m_volume = m_reset_volume;
    set_pan(m_pan);
}

CMidiFile::~CMidiFile()
{
    if (m_flags.loaded)
        unload();
}

SNDERR CMidiFile::unload()
{
    unsigned int result;
    SNDERR retval = SNDERR_OK;
    CMidiTrack *ptrk;
    unsigned char *pdata;

    if (m_flags.loaded && m_hfile != 0) {
        result = mmioClose(m_hfile, 0);
        if (result != 0)
            mmioerr(result);
        --gbl_mmio;
    }

    ptrk = m_track_list.remove_head();
    while (ptrk != 0) {
        pdata = ptrk->get_pdata();
        if (pdata != 0 && !ptrk->is_own_data())
            ptrk->set_pdata(0);
        delete ptrk;
        ptrk = m_track_list.remove_head();
    }

    m_hfile = 0;
    m_curendpos = 0;
    m_curfpos = 0;
    m_flags.loaded = 0;
    return retval;
}

void CMidiFile::mmioerr(unsigned long err)
{
}

SNDERR CMidiFile::play()
{
    m_flags.done = 0;
    return SNDERR_OK;
}

SNDERR CMidiFile::fade()
{
    m_flags.fading = 1;
    return SNDERR_OK;
}

SNDERR CMidiFile::fade(unsigned long ms)
{
    set_fade(ms);
    m_flags.fading = 1;
    return SNDERR_OK;
}

void CMidiFile::set_pan(int pan)
{
    if (pan < -0x40)
        pan = -0x40;
    if (pan > 0x3f)
        pan = 0x3f;
    m_pan = pan;
}

void CMidiFile::set_fade(unsigned long ms)
{
    unsigned long rate;
    unsigned long number_of_decrements;

    if (gbl_ptimer != 0) {
        rate = gbl_ptimer->get_rate();
        if (ms > rate) {
            number_of_decrements = ms / rate;
            m_fade_amount = m_reset_volume / number_of_decrements;
        } else
            m_fade_amount = m_reset_volume;
    }
}

void CMidiFile::set_loop_state(int state)
{
    m_flags.looping = state;
}

unsigned long CMidiFile::be2le(unsigned long be)
{
    union {
        unsigned long d;
        unsigned char b[4];
    };
    unsigned char *pos = (unsigned char *)&be;
    for (int i = 0; i < 4; i++)
        b[i] = pos[3 - i];
    return d;
}

unsigned short CMidiFile::be2le(unsigned short be)
{
    union {
        unsigned short w;
        unsigned char b[2];
    };
    unsigned char *pos = (unsigned char *)&be;
    for (int i = 0; i < 2; i++)
        b[i] = pos[1 - i];
    return w;
}

union dwstr {
    unsigned long dw;
    unsigned char str[4];
};

unsigned long CMidiFile::varlen2dw(unsigned char *string, unsigned long *bytes_used)
{
    int i = 0;
    int j = 3;
    unsigned long bedw = 0;
    unsigned long shift_amount = 7;
    dwstr k;

    k.dw = 0;
    *bytes_used = 0;
    do {
        k.dw = k.dw << shift_amount;
        k.dw = k.dw | (string[i] & 0x7f);
        *bytes_used = *bytes_used + 1;
    } while ((string[i++] & 0x80) != 0);
    return k.dw;
}

CMidiTrack::CMidiTrack()
{
    memset(&m_flags, 0, sizeof(m_flags));
    memset(m_on_off_table, 0, sizeof(m_on_off_table));
    m_flags.active = 0;
    m_pcur_event = 0;
    m_pdata = 0;
    m_ticks = 0;
    m_cur_channel = 0x10;
    m_cur_status = 0;
    m_tick_offset = 0;
    m_total_ticks = 0;
    m_pitch_shift = 0;
    m_velocity_offset = 0;
    m_ptrackset = 0;
}

CMidiTrack::~CMidiTrack()
{
    if (m_pdata != 0 && m_flags.own_data)
        ::operator delete(m_pdata);
    m_pdata = 0;
}

int CMidiTrack::start_control(int input)
{
    if (get_list()) {
        MidiCtrl *pnext = get_list()->get_first();
        while (pnext) {
            switch (pnext->m_type) {
            case MIDI_NOTEMAP:
                input = pnext->do_control(input);
                break;
            case MIDI_RANDOMIZER:
                return input;
            case MIDI_STEPPER:
                input = pnext->do_control(input);
                break;
            }
            pnext = get_list()->get_next();
        }
    }
    return input;
}

void CMidiTrack::reset()
{
    m_pcur_event = m_pdata;
    m_cur_offset = 0;
    m_tick_offset = 0;
    m_flags.at_end = 0;
    m_flags.active_switcher = 0;
    m_ticks = 0;
    m_cur_channel = 0x10;
    m_cur_status = 0;
    update_all_controls();

    for (MidiCtrl *pcur_ctrl = m_ctrl_list.get_first(); pcur_ctrl != 0; pcur_ctrl = m_ctrl_list.get_next()) {
        if (pcur_ctrl->m_type == MIDI_RANDOMIZER) {
            pcur_ctrl->do_control(0);
            if (pcur_ctrl->m_flags.state == 1)
                mute();
            else
                unmute();
        }
    }
}

void CMidiTrack::set_pdata(unsigned char *pdata)
{
    if (m_pdata != 0 && m_flags.own_data) {
        delete m_pdata;
        m_pdata = 0;
    }
    m_pdata = pdata;
    m_pcur_event = pdata;
    if (pdata != 0)
        calc_total_ticks();
    m_cur_offset = 0;
}

SNDERR CMidiTrack::advance(unsigned long distance)
{
    if (m_cur_offset + distance > m_length)
        return SNDERR_BUFFER_OVERRUN;
    if (m_cur_offset + distance == m_length)
        m_flags.at_end = 1;
    m_pcur_event += distance;
    m_cur_offset += distance;
    return SNDERR_OK;
}

void CMidiTrack::set_tick_offset(unsigned long offset)
{
    unsigned long total_ticks = get_total_ticks();
    unsigned long rem = 0;
    unsigned long overshot;

    if (total_ticks == 0)
        return;
    if (offset > total_ticks) {
        rem = offset % total_ticks;
        m_tick_offset = offset - rem;
        seek_to_prior(rem, &overshot);
    } else {
        m_tick_offset = 0;
        seek_to_prior(offset, &overshot);
    }
}

SNDERR CMidiTrack::seek(unsigned long seek_ticks, unsigned long *overshot_ticks)
{
    unsigned long ticks = 0;
    unsigned long cur_status = 0;
    int done = 0;
    unsigned long bytes_used = 0;
    unsigned long varlength = 0;
    unsigned char *midi_stream = m_pdata;
    unsigned char *pcur_event = m_pdata;

    while (!done && ticks < seek_ticks) {
        ticks += CMidiFile::varlen2dw(midi_stream, &bytes_used);
        pcur_event = midi_stream;
        midi_stream += bytes_used;

        if (*midi_stream & 0x80) {
            cur_status = *midi_stream;
            if ((cur_status & 0xf0) != 0xf0)
                cur_status &= 0xf0;
            midi_stream++;
        }

        switch (cur_status) {
        case 0xff:
            if (*midi_stream == 0x2f) {
                m_pcur_event = pcur_event;
                *overshot_ticks = 0;
                done = 1;
                m_ticks = ticks;
                return SNDERR_END_OF_TRACK;
            }
            midi_stream++;
            varlength = CMidiFile::varlen2dw(midi_stream, &bytes_used);
            midi_stream += bytes_used;
            midi_stream += varlength;
            break;
        case 0xc0:
        case 0xe0:
            midi_stream += 1;
            break;
        case 0x80:
        case 0x90:
        case 0xb0:
            midi_stream += 2;
            break;
        default:
            return SNDERR_BAD_FORMAT;
        }
    }

    *overshot_ticks = ticks - seek_ticks;
    m_pcur_event = midi_stream;
    m_ticks = ticks;
    return SNDERR_OK;
}

SNDERR CMidiTrack::seek_to_prior(unsigned long seek_ticks, unsigned long *prior_ticks)
{
    unsigned long ticks = 0;
    unsigned long cur_status = 0;
    int done = 0;
    unsigned long bytes_used = 0;
    unsigned long varlength = 0;
    unsigned char *midi_stream = m_pdata;
    unsigned char *pcur_event = m_pdata;
    unsigned char got_one = 0;
    unsigned long event_ticks = 0;

    while (!done) {
        event_ticks = CMidiFile::varlen2dw(midi_stream, &bytes_used);
        if (ticks + event_ticks > seek_ticks) {
            if (!got_one) {
                return SNDERR_END_OF_TRACK;
            }
            *prior_ticks = ticks;
            break;
        }
        pcur_event = midi_stream;
        got_one = 1;
        ticks += event_ticks;
        midi_stream += bytes_used;

        if (*midi_stream & 0x80) {
            int hi;
            cur_status = *midi_stream;
            hi = *midi_stream & 0xf0;
            m_cur_status = cur_status;
            if (hi != 0xf0) {
                m_cur_channel = cur_status & 0xf;
                cur_status = hi;
                midi_stream++;
            } else {
                midi_stream++;
            }
        }

        switch (cur_status) {
        case 0xff:
            if (*midi_stream == 0x2f) {
                m_pcur_event = pcur_event;
                *prior_ticks = 0;
                done = 1;
                m_ticks = ticks;
                return SNDERR_END_OF_TRACK;
            }
            midi_stream++;
            varlength = CMidiFile::varlen2dw(midi_stream, &bytes_used);
            midi_stream += bytes_used;
            midi_stream += varlength;
            break;
        case 0xc0:
        case 0xe0:
            midi_stream += 1;
            break;
        case 0x80:
        case 0x90:
        case 0xb0:
            midi_stream += 2;
            break;
        default:
            return SNDERR_BAD_FORMAT;
        }
    }

    m_pcur_event = midi_stream;
    m_ticks = ticks;
    *prior_ticks = ticks;
    return SNDERR_OK;
}

void CMidiTrack::hw_notes_off()
{
    if (gbl_pmidi_device->hmidi_out() == 0)
        return;

    for (int i = 0; i < 128; i++) {
        if (m_hw_on_off_table[i].get_status() != 0) {
            m_hw_on_off_table[i].set_d2(0);
            midiOutShortMsg(gbl_pmidi_device->hmidi_out(), m_hw_on_off_table[i].get_packed());
            m_hw_on_off_table[i].set_packed(0);
        }
    }
}

unsigned long CMidiTrack::calc_total_ticks()
{
    unsigned long ticks = 0;
    unsigned long cur_status = 0;
    int done = 0;
    unsigned long bytes_used = 0;
    unsigned long varlength = 0;
    unsigned char *midi_stream = m_pdata;

    while (!done) {
        ticks += CMidiFile::varlen2dw(midi_stream, &bytes_used);
        midi_stream += bytes_used;

        if (*midi_stream & 0x80) {
            cur_status = *midi_stream;
            if ((cur_status & 0xf0) != 0xf0)
                cur_status &= 0xf0;
            midi_stream++;
        }

        switch (cur_status) {
        case 0xff:
            if (*midi_stream == 0x2f)
                done = 1;
            midi_stream++;
            varlength = CMidiFile::varlen2dw(midi_stream, &bytes_used);
            midi_stream += bytes_used;
            midi_stream += varlength;
            break;
        case 0xc0:
        case 0xe0:
            midi_stream += 1;
            break;
        case 0x80:
        case 0x90:
        case 0xb0:
            midi_stream += 2;
            break;
        default:
            return 0;
        }
    }

    m_total_ticks = ticks;
    return ticks;
}

void CMidiFile::set_all_tracks_inactive()
{

    CMidiTrack *ptrack = m_track_list.get_first();
    while (ptrack != 0) {
        ptrack->m_flags.active = 0;
        ptrack = m_track_list.get_next();
    }
}
