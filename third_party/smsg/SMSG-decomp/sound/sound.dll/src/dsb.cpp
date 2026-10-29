#include <dsound.h>
#include <math.h>
#include <string.h>
#include "sound.h"
#include "sounddevice.h"
#include "dsb.h"
#include "dsmgr.h"

int dsbindex;
int gbl_dsinit;


CDSB::CDSB()
{
    m_flags.created = 0;
    m_flags.virgin = 1;
    m_pdsmgr = 0;
    m_delay = 0;
    init();
    m_xpos = 0.0f;
    m_ypos = 0.0f;
    m_zpos = 0.0f;
    m_pds3d_buffer = 0;
    m_pds3d_listener = 0;
    m_total_ms_played = 0;
    m_write_cursor = 0;
    m_ms_write_margin = 0;
    m_start_time = 0;
    m_buffer_mask = 0;
}

CDSB::~CDSB()
{
    if (m_lpdsb == 0)
        return;
    if (m_pdsmgr == 0)
        return;

    m_lpdsb->Stop();

    if (m_pds3d_buffer != 0 || m_pds3d_listener != 0) {
        if (m_pds3d_buffer != 0) {
            if (m_preverb != 0) {
                m_preverb->Release();
                m_preverb = 0;
            }
            m_pds3d_buffer->Release();
            m_pds3d_buffer = 0;
        }
    }

    m_lpdsb->Release();

    memset(&m_dsbcaps, 0, 20);
    memset(&m_dsbcaps, 0, 20);

    m_size = 0;
    m_curplaypos = 0;
    m_lpdsb = 0;
    m_total_ms_played = 0;
    m_write_cursor = 0;
}

void CDSB::resync()
{
    m_total_ms_played = 0;
    m_write_cursor = 0;
    if (m_lpdsb != 0)
        m_lpdsb->Stop();
    if (m_lpdsb != 0) {
        m_start_time = timeGetTime();
        if (m_lpdsb->Play(0, 0, DSBPLAY_LOOPING) == 0)
            m_lpdsb->GetFormat(&m_wfx, 18, 0);
    }
}

SNDERR CDSB::dsbrestore()
{
    if (m_lpdsb)
        m_lpdsb->Restore();
    return SNDERR_OK;
}

SNDERR CDSB::set_buffer_size(unsigned long size)
{
    if (m_lpdsb != 0)
        return SNDERR_ALREADY_OPEN;
    m_dsbdesc.dwBufferBytes = size;
    return SNDERR_OK;
}

SNDERR CDSB::setsamplerate(unsigned long srate)
{
    if (m_lpdsb)
        return SNDERR_ALREADY_OPEN;
    if (srate != 44100 && srate != 22050 && srate != 11025)
        return SNDERR_BAD_PARAM;
    m_wfx.nSamplesPerSec = srate;
    m_wfx.nAvgBytesPerSec = (unsigned long)m_wfx.nBlockAlign * srate;
    return SNDERR_OK;
}

void CDSB::set_frequency(unsigned long freq)
{
    if (m_lpdsb != 0)
        m_lpdsb->SetFrequency(freq);
}

void CDSB::set_volume(long volume)
{
    m_volume = volume;
    if (m_lpdsb != 0)
        m_lpdsb->SetVolume(volume);
}

void CDSB::set_volume(unsigned long volume)
{
    double volume_scale;
    double processed_volume;
    int dB_volume;
    double master_scale;
    double scaled_scale;
    double master_volume;

    if (volume > 127)
        volume = 127;

    master_volume = m_pdsmgr ? m_pdsmgr->get_volume() : 127;

    volume_scale = volume * (1.0 / 127.0);
    master_scale = master_volume * (1.0 / 127.0);
    scaled_scale = volume_scale * master_scale;
    processed_volume = scaled_scale * 127.0;
    volume = (unsigned long)processed_volume;

    if (volume) {
        processed_volume = log(volume * (1.0 / 127.0)) * 10.0;
        dB_volume = (int)(processed_volume * 100.0);
    } else
        dB_volume = -10000;

    m_volume = dB_volume;

    if (m_lpdsb)
        m_lpdsb->SetVolume(dB_volume);
}

void CDSB::set_volume(double volume)
{
    double processed_volume;
    int dB_volume;
    double master_scale;
    double scaled_scale;
    double master_volume;

    master_volume = m_pdsmgr ? m_pdsmgr->get_volume() : 127;

    master_scale = master_volume * (1.0 / 127.0);
    scaled_scale = volume * master_scale;
    volume = (unsigned long)(scaled_scale * 127.0);

    if (volume != 0.0) {
        processed_volume = log(volume * (1.0 / 127.0)) * 10.0;
        dB_volume = (int)(processed_volume * 100.0);
    } else
        dB_volume = -10000;

    m_volume = dB_volume;

    if (m_lpdsb)
        m_lpdsb->SetVolume(dB_volume);
}

SNDERR CDSB::setsamplesize(unsigned long samplesize)
{
    if (m_lpdsb != 0)
        return SNDERR_ALREADY_OPEN;
    if (samplesize != 16 && samplesize != 8)
        return SNDERR_BAD_PARAM;

    m_wfx.wBitsPerSample = (unsigned short)samplesize;
    m_wfx.nBlockAlign = (unsigned short)(m_wfx.nChannels * samplesize);
    m_wfx.nAvgBytesPerSec = m_wfx.nSamplesPerSec * (unsigned)m_wfx.nBlockAlign;
    return SNDERR_OK;
}

SNDERR CDSB::setnumchannels(unsigned long nchannels)
{
    if (m_lpdsb)
        return SNDERR_ALREADY_OPEN;
    if (nchannels != 0 && nchannels != 1)
        return SNDERR_BAD_PARAM;
    m_wfx.nChannels = (unsigned short)nchannels;
    m_wfx.nBlockAlign = (unsigned short)(m_wfx.wBitsPerSample * (unsigned short)nchannels);
    m_wfx.nAvgBytesPerSec = (unsigned long)m_wfx.nBlockAlign * m_wfx.nSamplesPerSec;
    return SNDERR_OK;
}

SNDERR CDSB::dserrhandler(long err)
{
    int errindex = 0;
    SNDERR retval = SNDERR_UNDEFINED;

    switch (err) {
    case DSERR_ALLOCATED:
        errindex = 0;
        retval = SNDERR_ALREADY_OPEN;
        break;
    case DSERR_INVALIDPARAM:
        errindex = 1;
        retval = SNDERR_BAD_PARAM;
        break;
    case DSERR_NOAGGREGATION:
        errindex = 2;
        retval = SNDERR_UNDEFINED;
        break;
    case DSERR_NODRIVER:
        errindex = 3;
        retval = SNDERR_NO_DRIVER;
        break;
    case DSERR_OUTOFMEMORY:
        errindex = 4;
        retval = SNDERR_NO_MEM;
        break;
    case DSERR_BADFORMAT:
        errindex = 5;
        retval = SNDERR_BAD_FORMAT;
        break;
    case DSERR_UNINITIALIZED:
        errindex = 6;
        retval = SNDERR_UNINITIALIZED;
        break;
    case DSERR_UNSUPPORTED:
        errindex = 7;
        retval = SNDERR_NO_SUPPORT;
        break;
    case DSERR_BUFFERLOST:
        errindex = 8;
        retval = SNDERR_DSB_LOST;
        break;
    default:
        errindex = 9;
        retval = SNDERR_UNDEFINED;
        break;
    }
    return retval;
}

SNDERR CDSB::init()
{
    m_flags.type = 0;
    memset(&m_dsbdesc, 0, sizeof(m_dsbdesc));
    m_dsbdesc.dwSize = sizeof(m_dsbdesc);
    m_dsbdesc.dwFlags = 0xe8;
    m_dsbdesc.dwBufferBytes = 0x10000;
    m_dsbdesc.dwReserved = 0;
    m_dsbdesc.lpwfxFormat = &m_wfx;
    memset(&m_wfx, 0, 18);
    m_wfx.wFormatTag = 1;
    m_wfx.nChannels = 2;
    m_wfx.nSamplesPerSec = 22050;
    m_wfx.nAvgBytesPerSec = 88200;
    m_wfx.nBlockAlign = 4;
    m_wfx.wBitsPerSample = 16;
    m_wfx.cbSize = 0;
    m_block_size = 0;
    m_cur_dsb_play = 0;
    m_cur_play_interval = 0;
    m_cur_play_write_interval = 0;
    m_curplaypos = 0;
    m_delay = 0;
    memset(&m_dsbcaps, 0, sizeof(m_dsbcaps));
    m_last_dsb_play = 0;
    m_last_dsb_write = 0;
    m_last_user_write = 0;
    m_last_write_limit = 0;
    m_lpdsb = 0;
    m_nsamples_per_tick = 0x295;
    set_write_margin(30, 18);
    m_pdsmgr = 0;
    m_pnext = 0;
    m_pprev = 0;
    m_pds3d_buffer = 0;
    m_pds3d_listener = 0;
    m_preverb = 0;
    m_pwf_owner = 0;
    m_volume = 0;
    m_write_offset = 0;
    return SNDERR_OK;
}

void CDSB::reinit()
{
    memset(&m_dsbcaps, 0, sizeof(m_dsbcaps));
    m_size = 0;
    m_curplaypos = 0;
    m_lpdsb = 0;
    m_total_ms_played = 0;
    m_write_cursor = 0;
}

SNDERR CDSB::stop()
{
    if (m_lpdsb == 0)
        return SNDERR_NOT_CREATED;
    m_lpdsb->Stop();
    return SNDERR_OK;
}

SNDERR CDSB::reset()
{
    unsigned long play;
    unsigned long write;

    if (m_lpdsb != 0) {
        m_lpdsb->SetCurrentPosition(0);
        m_lpdsb->GetCurrentPosition(&play, &write);
    }

    m_flags.virgin = 1;
    m_last_dsb_write = 0;
    m_last_returned_dsb_write = 0;
    m_last_user_write = 0;
    m_last_write_limit = 0;
    m_curplaypos = 0;
    m_last_dsb_play = 0;
    m_cur_dsb_play = 0;
    m_cur_play_interval = 0;
    m_cur_play_write_interval = 0;
    m_write_cursor = 0;
    m_total_ms_played = 0;
    return SNDERR_OK;
}

SNDERR CDSB::quiet()
{
    void *lpvAudioPtr1;
    unsigned long dwAudioBytes1;
    void *lpvAudioPtr2;
    unsigned long dwAudioBytes2;

    if (m_lpdsb != 0) {
        if (m_lpdsb->Lock(0, m_dsbdesc.dwBufferBytes, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2, 2)
            == 0) {
            memset(lpvAudioPtr1, 0, dwAudioBytes1);
            if (dwAudioBytes2 != 0)
                memset(lpvAudioPtr2, 0, dwAudioBytes2);
            m_lpdsb->Unlock(lpvAudioPtr1, dwAudioBytes1, lpvAudioPtr2, dwAudioBytes2);
            return SNDERR_OK;
        }
        return (SNDERR)0x1a;
    }
    return SNDERR_NOT_PRESENT;
}

SNDERR CDSB::release()
{
    long dsb_return = 0;

    if (m_lpdsb != 0) {
        dsb_return = m_lpdsb->Stop();
        if (is_3d())
            release_3d();
        dsb_return = m_lpdsb->Release();
        reinit();
    }
    return SNDERR_OK;
}

SNDERR CDSB::set_format(WAVEFORMATEX *pwfx)
{
    long dserr;
    SNDERR retval;

    if (m_lpdsb != 0 && m_flags.type != 1)
        return (SNDERR)0x06;
    if (m_lpdsb == 0 && m_flags.type == 1)
        return (SNDERR)0x06;
    if (m_lpdsb != 0 && m_flags.type == 1) {
        dserr = m_lpdsb->SetFormat(pwfx);
        if (dserr != 0) {
            retval = dserrhandler(dserr);
            return retval;
        }
        m_wfx = *pwfx;
        return SNDERR_OK;
    }
    m_wfx = *pwfx;
    return SNDERR_OK;
}

SNDERR CDSB::create(IDirectSound *lpds, unsigned long ms_write_margin)
{
    SNDERR retval;
    HRESULT dserr;

    if (ms_write_margin == 0)
        return SNDERR_BAD_PARAM;

    if (m_flags.type == 1)
        m_dsbdesc.lpwfxFormat = 0;
    else if (m_dsbdesc.lpwfxFormat == 0)
        m_dsbdesc.lpwfxFormat = &m_wfx;
    else
        memcpy(&m_wfx, m_dsbdesc.lpwfxFormat, 18);

    if (m_lpdsb != 0)
        return SNDERR_ALREADY_OPEN;

    dserr = lpds->CreateSoundBuffer(&m_dsbdesc, &m_lpdsb, 0);
    if (dserr != 0) {
        retval = dserrhandler(dserr);
        return retval;
    }

    m_ms_write_margin = ms_write_margin;
    m_dsbcaps.dwSize = 20;
    if (m_dsbdesc.dwBufferBytes != 0)
        m_buffer_mask = ~m_dsbdesc.dwBufferBytes % m_dsbdesc.dwBufferBytes;
    dserr = m_lpdsb->GetCaps(&m_dsbcaps);
    if (dserr != DS_OK)
        retval = dserrhandler(dserr);

    if (m_flags.type == 1) {
        m_lpdsb->SetFormat(&m_wfx);
        m_lpdsb->Play(0, 0, 1);
    }

    if (m_dsbdesc.dwFlags & 0x10) {
        init_3d();
        init_reverb();
    }

    m_lpdsb->GetFormat(&m_wfx, 18, 0);
    return SNDERR_OK;
}

bool CDSB::is_3d()
{
    if (m_pds3d_buffer || m_pds3d_listener)
        return 1;
    return 0;
}

SNDERR CDSB::init_3d()
{
    long hr;

    if (m_lpdsb == 0)
        return SNDERR_NOT_LOADED;

    if (m_flags.type)
        hr = m_lpdsb->QueryInterface(IID_IDirectSound3DListener, (void **)&m_pds3d_listener);
    else
        hr = m_lpdsb->QueryInterface(IID_IDirectSound3DBuffer, (void **)&m_pds3d_buffer);

    if (hr >= 0)
        return SNDERR_OK;
    m_pds3d_listener = 0;
    m_pds3d_buffer = 0;
    return SNDERR_NO_SUPPORT;
}

SNDERR CDSB::init_reverb()
{
    long hr;

    if (m_pds3d_listener != 0)
        hr = m_pds3d_listener->QueryInterface(IID_IKsPropertySet, (void **)&m_preverb);
    else if (m_pds3d_buffer != 0)
        hr = m_pds3d_buffer->QueryInterface(IID_IKsPropertySet, (void **)&m_preverb);
    else
        hr = 0;

    if (hr >= 0)
        return SNDERR_OK;
    m_preverb = 0;
    return SNDERR_NO_SUPPORT;
}

SNDERR CDSB::release_3d()
{
    if (m_pds3d_buffer == 0)
        return (SNDERR)2;

    if (m_preverb != 0) {
        m_preverb->Release();
        m_preverb = 0;
    }
    m_pds3d_buffer->Release();
    m_pds3d_buffer = 0;
    return SNDERR_OK;
}

SNDERR CDSB::release_reverb()
{
    if (m_preverb == 0)
        return SNDERR_NOT_CREATED;
    m_preverb->Release();
    m_preverb = 0;
    return SNDERR_OK;
}

SNDERR CDSB::destroy()
{
    if (m_pdsmgr == 0)
        return SNDERR_NO_DRIVER;
    if (m_lpdsb == 0)
        return SNDERR_OK;

    m_lpdsb->Stop();

    if (m_pds3d_buffer != 0 || m_pds3d_listener != 0) {
        if (m_pds3d_buffer != 0) {
            if (m_preverb != 0) {
                m_preverb->Release();
                m_preverb = 0;
            }
            m_pds3d_buffer->Release();
            m_pds3d_buffer = 0;
        }
    }

    m_lpdsb->Release();

    memset(&m_dsbcaps, 0, 20);
    memset(&m_dsbcaps, 0, 20);

    m_size = 0;
    m_curplaypos = 0;
    m_lpdsb = 0;
    m_total_ms_played = 0;
    m_write_cursor = 0;
    return SNDERR_OK;
}

SNDERR CDSB::get_cur_dsbwrite(unsigned long *dsbwrite)
{
    if (!m_lpdsb)
        return SNDERR_NOT_LOADED;
    m_lpdsb->GetCurrentPosition(&m_curplaypos, dsbwrite);
    return SNDERR_OK;
}

SNDERR CDSB::get_cur_dsbplay(unsigned long *dsbplay)
{
    unsigned long write_stub;
    if (!m_lpdsb)
        return SNDERR_NOT_LOADED;
    m_lpdsb->GetCurrentPosition(dsbplay, &write_stub);
    return SNDERR_OK;
}

SNDERR CDSB::lock(unsigned long pos, unsigned long size, void **pp1, unsigned long *s1, void **pp2, unsigned long *s2)
{
    HRESULT errval = DS_OK;
    errval = m_lpdsb->Lock(pos, size, pp1, s1, pp2, s2, 0);
    if (errval != DS_OK) {
        if (dserrhandler(errval) == SNDERR_DSB_LOST) {
            dsbrestore();
            errval = m_lpdsb->Lock(pos, size, pp1, s1, pp2, s2, 0);
            if (errval != DS_OK) {
                if (dserrhandler(errval) == SNDERR_DSB_LOST)
                    return SNDERR_DSB_LOST;
                else
                    return SNDERR_BAD_DSB_WRITE;
            }
        }
        return SNDERR_BAD_DSB_WRITE;
    }
    return SNDERR_OK;
}

SNDERR CDSB::unlock(void *p1, unsigned long s1, void *p2, unsigned long s2)
{
    release_pointer(p1, s1, p2, s2);
    return SNDERR_OK;
}

SNDERR CDSB::get_pointer(void **pp1, unsigned long *s1, void **pp2, unsigned long *s2)
{
    long errval;
    unsigned long curdsbwrite = 0, curplay = 0, curwritelimit = 0, curuserwrite = 0, bytes_to_write = 0;
    int reset = 0;


    if (m_lpdsb == 0)
        return SNDERR_NOT_LOADED;

    errval = m_lpdsb->GetCurrentPosition(&curplay, &curdsbwrite);
    if (errval != DS_OK)
        dserrhandler(errval);
    if (curdsbwrite == m_last_dsb_write)
        return SNDERR_NOT_ACTIVE;

    curwritelimit = m_write_margin + curdsbwrite;
    if (curwritelimit >= m_dsbdesc.dwBufferBytes) {
        curwritelimit -= m_dsbdesc.dwBufferBytes;
        if (m_last_write_limit > curdsbwrite || m_last_write_limit < curwritelimit) {
            curuserwrite = m_last_write_limit;
            bytes_to_write = diff(curuserwrite, curwritelimit);
        } else {
            curuserwrite = m_block_size + curdsbwrite;
            if (curuserwrite >= m_dsbdesc.dwBufferBytes)
                curuserwrite -= m_dsbdesc.dwBufferBytes;
            bytes_to_write = diff(curuserwrite, curwritelimit);
        }
    } else {
        if (m_last_write_limit > curdsbwrite && m_last_write_limit < curwritelimit) {
            curuserwrite = m_last_write_limit;
            bytes_to_write = diff(curuserwrite, curwritelimit);
        } else {
            curuserwrite = m_block_size + curdsbwrite;
            bytes_to_write = curwritelimit - curuserwrite;
        }
    }

    errval = m_lpdsb->Lock(curuserwrite, bytes_to_write, pp1, s1, pp2, s2, 0);
    if (errval != DS_OK) {
        if (dserrhandler(errval) == SNDERR_DSB_LOST) {
            dsbrestore();
            errval = m_lpdsb->Lock(curuserwrite, bytes_to_write, pp1, s1, pp2, s2, 0);
            if (errval != DS_OK && dserrhandler(errval) == SNDERR_DSB_LOST)
                return SNDERR_DSB_LOST;
        }
        return SNDERR_BAD_DSB_WRITE;
    }
    m_last_write_limit = curwritelimit;
    m_last_dsb_write = curdsbwrite;
    return SNDERR_OK;
}

SNDERR CDSB::get_write_info(unsigned long *nbytes_to_write, unsigned long *position, unsigned long ms_time_past)
{
    if (m_pdsmgr != 0) {
        if (m_pdsmgr->is_emulated())
            return get_emulated_write_info(nbytes_to_write, position, ms_time_past);
        else
            return get_real_write_info(nbytes_to_write, position);
    }
    return SNDERR_NO_DRIVER;
}

SNDERR CDSB::get_real_write_info(unsigned long *nbytes_to_write, unsigned long *position)
{
    long errval;
    SNDERR retval;
    unsigned long curdsbwrite = 0, curplay = 0, curwritelimit = 0, curuserwrite = 0, bytes_to_write = 0;
    int reset = 0;
    unsigned long play_diff;

    retval = SNDERR_OK;

    if (m_lpdsb == 0)
        return SNDERR_NOT_LOADED;

    errval = m_lpdsb->GetCurrentPosition(&curplay, &curdsbwrite);
    if (errval != DS_OK)
        dserrhandler(errval);
    play_diff = diff(m_last_dsb_play, curplay);
    m_cur_play_interval = bytes_to_samples(play_diff);

    if (curdsbwrite == m_last_dsb_write)
        return SNDERR_NOT_ACTIVE;

    curwritelimit = m_write_margin + curdsbwrite;
    m_last_dsb_play = curplay;

    if (curwritelimit >= m_dsbdesc.dwBufferBytes) {
        curwritelimit -= m_dsbdesc.dwBufferBytes;
        if (m_last_write_limit > curdsbwrite || m_last_write_limit < curwritelimit) {
            curuserwrite = m_last_write_limit;
            bytes_to_write = diff(curuserwrite, curwritelimit);
        } else {
            curuserwrite = m_block_size + curdsbwrite;
            if (curuserwrite >= m_dsbdesc.dwBufferBytes)
                curuserwrite -= m_dsbdesc.dwBufferBytes;
            bytes_to_write = diff(curuserwrite, curwritelimit);
            retval = SNDERR_RESET;
        }
    } else {
        if (m_last_write_limit > curdsbwrite && m_last_write_limit < curwritelimit) {
            curuserwrite = m_last_write_limit;
            bytes_to_write = diff(curuserwrite, curwritelimit);
        } else {
            curuserwrite = m_block_size + curdsbwrite;
            if (curuserwrite >= m_dsbdesc.dwBufferBytes)
                curuserwrite -= m_dsbdesc.dwBufferBytes;
            bytes_to_write = diff(curuserwrite, curwritelimit);
            retval = SNDERR_RESET;
        }
    }

    m_last_write_limit = curwritelimit;
    m_last_dsb_write = curdsbwrite;
    *nbytes_to_write = bytes_to_write;
    *position = curuserwrite;
    m_cur_play_write_interval = bytes_to_samples(diff(curplay, curuserwrite));
    return retval;
}

SNDERR CDSB::get_write_info(unsigned long nbytes, unsigned long *position)
{
    unsigned long cur_dsb_write = 0, cur_write_limit = 0, cur_user_write = 0, cur_dsb_play = 0;
    long errval;

    errval = m_lpdsb->GetCurrentPosition(&cur_dsb_play, &cur_dsb_write);
    if (errval != DS_OK) {
        dserrhandler(errval);
        return SNDERR_NOCANDO;
    }

    if (m_flags.virgin) {
        m_flags.virgin = 0;
        m_last_returned_dsb_write = cur_dsb_write;
        m_last_dsb_write = cur_dsb_write;
        cur_user_write = m_block_size + cur_dsb_write;
        if (cur_user_write >= m_dsbdesc.dwBufferBytes)
            cur_user_write -= m_dsbdesc.dwBufferBytes;
    } else {
        cur_user_write = m_last_write_limit;
    }

    cur_write_limit = nbytes + cur_user_write;
    if (cur_write_limit >= m_dsbdesc.dwBufferBytes)
        cur_write_limit -= m_dsbdesc.dwBufferBytes;
    m_last_dsb_write = cur_dsb_write;
    m_last_write_limit = cur_write_limit;
    *position = cur_user_write;
    return SNDERR_OK;
}

SNDERR CDSB::get_emulated_write_info(unsigned long *nbytes_to_write, unsigned long *position)
{
    long errval;
    unsigned long curdsbwrite = 0, curplay = 0, curwritelimit = 0, curuserwrite = 0, bytes_to_write = 0;
    int reset = 0;
    int nbytes_off = 0;
    static int sample_flop = 0x370;

    if (m_lpdsb == 0)
        return SNDERR_NOT_LOADED;

    sample_flop = (sample_flop == 0x370) ? 0x374 : 0x370;

    if (m_flags.virgin) {
        if (sync_emulated() != SNDERR_OK)
            return SNDERR_OUT_OF_SYNC;
        errval = m_lpdsb->GetCurrentPosition(&curplay, &curdsbwrite);
        m_last_returned_dsb_write = curdsbwrite;
        m_last_dsb_write = curdsbwrite;
        m_flags.virgin = 0;
    } else {
        IDirectSoundBuffer *pBuf = m_lpdsb;
        errval = pBuf->GetCurrentPosition(&curplay, &curdsbwrite);
    }
    if (errval != DS_OK)
        dserrhandler(errval);

    if (curdsbwrite != m_last_returned_dsb_write)
        m_last_returned_dsb_write = curdsbwrite;

    curdsbwrite = (m_last_dsb_write + sample_flop) & 0xffff;
    m_last_dsb_write = curdsbwrite;
    curwritelimit = m_write_margin + curdsbwrite;
    if (curwritelimit >= m_dsbdesc.dwBufferBytes) {
        curwritelimit -= m_dsbdesc.dwBufferBytes;
        curuserwrite = m_last_write_limit;
        bytes_to_write = diff(curuserwrite, curwritelimit);
    } else {
        curuserwrite = m_last_write_limit;
        bytes_to_write = diff(curuserwrite, curwritelimit);
    }

    m_last_write_limit = curwritelimit;
    m_last_dsb_write = curdsbwrite;
    *nbytes_to_write = bytes_to_write;
    *position = curuserwrite;
    m_cur_play_write_interval = bytes_to_samples(diff(curplay, curuserwrite));
    return SNDERR_OK;
}

SNDERR CDSB::sync_emulated()
{
    long errval;
    unsigned long dsbwrite1 = 0, dsbwrite2 = 0, curplay = 0;
    unsigned long timeout = 0;

    errval = m_lpdsb->GetCurrentPosition(&curplay, &dsbwrite1);
    if (errval != DS_OK)
        dserrhandler(errval);
    dsbwrite2 = dsbwrite1;
    while (1) {
        if (timeout++ >= 0x10000)
            break;
        m_lpdsb->GetCurrentPosition(&curplay, &dsbwrite2);
        if (dsbwrite2 != dsbwrite1)
            break;
    }
    return (timeout < 0x10000) ? SNDERR_OK : SNDERR_OUT_OF_SYNC;
}

SNDERR CDSB::get_emulated_write_info(unsigned long *nbytes_to_write, unsigned long *position,
                                     unsigned long ms_time_past)
{
    unsigned long total_ms_written = 0;
    long samples_to_write = 0;
    unsigned char out_of_sync;

    if (m_total_ms_played == 0) {
        total_ms_written = timeGetTime() - m_start_time;
        m_total_ms_played = total_ms_written;
        total_ms_written = total_ms_written + m_ms_write_margin * 2;
        m_write_cursor = total_ms_written * m_wfx.nSamplesPerSec / 1000;
        samples_to_write = m_wfx.nSamplesPerSec * ms_time_past / 1000;
    } else {
        total_ms_written = m_write_cursor * 1000 / 22050;
        m_total_ms_played += ms_time_past;
        if (m_total_ms_played > total_ms_written)
            out_of_sync = 1;
        samples_to_write =
            (m_ms_write_margin * 10 - total_ms_written + m_total_ms_played) * m_wfx.nSamplesPerSec / 1000;
        if (samples_to_write < 0)
            samples_to_write = 0;
    }

    *nbytes_to_write = samples_to_bytes(samples_to_write);
    *position = samples_to_bytes(m_write_cursor) & m_buffer_mask;
    m_write_cursor += samples_to_write;

    if (*nbytes_to_write > m_dsbdesc.dwBufferBytes) {
        m_total_ms_played = 0;
        m_write_cursor = 0;
        if (m_lpdsb != 0)
            m_lpdsb->Stop();
        if (m_lpdsb != 0) {
            m_start_time = timeGetTime();
            if (m_lpdsb->Play(0, 0, 1) == 0)
                m_lpdsb->GetFormat(&m_wfx, 18, 0);
        }
        *nbytes_to_write = 0;
        return SNDERR_OUT_OF_SYNC;
    }
    return SNDERR_OK;
}

SNDERR CDSB::release_pointer(void *p1, unsigned long s1, void *p2, unsigned long s2)
{
    long errval;

    if (!m_lpdsb)
        return SNDERR_NOT_LOADED;
    errval = m_lpdsb->Unlock(p1, s1, p2, s2);
    if (errval) {
        dserrhandler(errval);
        return SNDERR_BAD_DSB_UNLOCK;
    }
    return SNDERR_OK;
}

SNDERR CDSB::play(unsigned long flags)
{
    long errval;

    if (m_lpdsb == 0)
        return SNDERR_NOT_LOADED;
    m_start_time = timeGetTime();
    errval = m_lpdsb->Play(0, 0, flags);
    if (errval != 0) {
        dserrhandler(errval);
        return SNDERR_BAD_DSB_PLAY;
    }
    m_lpdsb->GetFormat(&m_wfx, 18, 0);
    return SNDERR_OK;
}

int CDSB::is_playing()
{
    if (m_lpdsb == 0)
        return 0;

    unsigned long status;
    m_lpdsb->GetStatus(&status);

    if (m_delay != 0)
        return 1;
    return status & 1;
}

void CDSB::set_samples_per_tick(unsigned long ms)
{
    m_nsamples_per_tick = m_wfx.nSamplesPerSec * ms / 1000;
}

SNDERR CDSB::set_write_margin(unsigned long ms, unsigned long nblocks)
{
    unsigned long temp_write_margin = 0, temp_block_size = 0;
    unsigned long temp_samples_per_tick;

    temp_samples_per_tick = (m_wfx.nSamplesPerSec * ms) / 1000;
    temp_block_size = m_wfx.nBlockAlign * temp_samples_per_tick;
    if ((temp_write_margin = temp_block_size * nblocks) > m_dsbdesc.dwBufferBytes)
        return SNDERR_BAD_PARAM;
    m_block_size = temp_block_size;
    m_write_margin = temp_write_margin;
    m_nsamples_per_tick = temp_samples_per_tick;
    m_write_offset = (m_write_margin - m_block_size) / 2;
    return SNDERR_OK;
}

SNDERR CDSB::set_type(unsigned long type)
{
    if (m_lpdsb != 0)
        return SNDERR_NOCANDO;

    switch (type) {
    case 1:
        memset(&m_dsbdesc, 0, sizeof(m_dsbdesc));
        m_dsbdesc.dwSize = 0x24;
        m_dsbdesc.dwFlags = 1;
        break;
    case 0:
        m_dsbdesc.lpwfxFormat = &m_wfx;
        m_dsbdesc.dwSize = 0x24;
        m_dsbdesc.dwFlags = 0xe8;
        m_dsbdesc.dwBufferBytes = 0x10000;
        m_dsbdesc.dwReserved = 0;
        break;
    default:
        return SNDERR_BAD_PARAM;
    }
    m_flags.type = type;
    return SNDERR_OK;
}

SNDERR CDSB::set_position3d(float x, float y, float z)
{
    if (m_pds3d_buffer == 0)
        return SNDERR_NO_SUPPORT;
    m_xpos = x;
    m_ypos = y;
    m_zpos = z;
    long DS_return = m_pds3d_buffer->SetPosition(m_xpos, m_ypos, m_zpos, 0);
    if (DS_return != DS_OK)
        return SNDERR_DSBAPI_FAILURE;
    return SNDERR_OK;
}

SNDERR CDSB::set_xpos(float x)
{
    if (m_pds3d_buffer == 0)
        return SNDERR_NO_SUPPORT;
    m_xpos = x;
    m_pds3d_buffer->SetPosition(m_xpos, m_ypos, m_zpos, 0);
    return SNDERR_OK;
}

SNDERR CDSB::set_ypos(float y)
{
    if (m_pds3d_buffer == 0)
        return SNDERR_NO_SUPPORT;
    m_ypos = y;
    m_pds3d_buffer->SetPosition(m_xpos, m_ypos, m_zpos, 0);
    return SNDERR_OK;
}

SNDERR CDSB::set_zpos(float z)
{
    if (m_pds3d_buffer == 0)
        return SNDERR_NO_SUPPORT;
    m_zpos = z;
    m_pds3d_buffer->SetPosition(m_xpos, m_ypos, m_zpos, 0);
    return SNDERR_OK;
}

SNDERR CDSB::set_reverb_mix(float mix)
{
    return SNDERR_NO_SUPPORT;
}
