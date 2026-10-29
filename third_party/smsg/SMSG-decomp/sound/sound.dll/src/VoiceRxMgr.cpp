#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include "sound.h"
#include "sounddevice.h"
#include "dlist.h"
#include "cmix.h"
#include "dllwavedevice.h"
#include "voicerxmgr.h"

WAVEFORMATEX tstfmt = {WAVE_FORMAT_PCM, 1, 8000, 16000, 2, 16, 0};

CVoiceRxMgr::CVoiceRxMgr()
{
    m_pdsb = 0;
    m_buffer_size = 0;
    memset(&m_flags, 0, sizeof m_flags);
    memset(&m_in_buffer, 0, sizeof m_in_buffer);
    set_type(SOUNDTYPE_VOICERX);
    m_lpc10_decoder_state = 0;
    m_buffers_played = 0;
    m_start_id = 0;
    m_buffers_added = 0;
}

CVoiceRxMgr::~CVoiceRxMgr()
{
    HMMIO hfile;
    MMIOINFO *pmmioinfo;
    WaveBuffer *pwb;

    pwb = m_buffers.remove_head();
    while (pwb) {
        pmmioinfo = pwb->get_mmioinfo();
        hfile = pmmioinfo->hmmio;
        if (hfile) {
            mmioClose(hfile, 0);
            gbl_created_list.remove(pwb->get_mmioinfo());
        }
        gbl_mmio--;
        delete pwb;
        pwb = m_buffers.remove_head();
    }
}

SNDERR CVoiceRxMgr::detach()
{
    Sound::detach();
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::release()
{
    HMMIO hfile;
    MMIOINFO *pmmioinfo;
    WaveBuffer *pwb;

    if (is_attached()) {
        stop();
        set_mixer_unload();
        return SNDERR_OK;
    }

    pwb = m_buffers.remove_head();
    while (pwb) {
        pmmioinfo = pwb->get_mmioinfo();
        hfile = pmmioinfo->hmmio;
        if (hfile) {
            mmioClose(hfile, 0);
            gbl_created_list.remove(pwb->get_mmioinfo());
        }
        gbl_mmio--;
        delete pwb;
        pwb = m_buffers.remove_head();
    }
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::direct_unload()
{
    HMMIO hfile;
    MMIOINFO *pmmioinfo;
    WaveBuffer *pwb;

    pwb = m_buffers.remove_head();
    while (pwb) {
        pmmioinfo = pwb->get_mmioinfo();
        hfile = pmmioinfo->hmmio;
        if (hfile) {
            mmioClose(hfile, 0);
            gbl_created_list.remove(pwb->get_mmioinfo());
        }
        gbl_mmio--;
        delete pwb;
        pwb = m_buffers.remove_head();
    }
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::unload()
{
    if (gbl_pwave_device)
        gbl_pwave_device->queue_request(this, REQUEST_UNLOAD);
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::add_buffer(MMIOINFO *pmmioinfo)
{
    return add_lpc10buffer(pmmioinfo);
}

int CVoiceRxMgr::lpc10_write_16bit_samples(short *int16samples, float *const speech, int n)
{
    float real_sample;
    int i;
    for (i = 0; i < n; i++) {
        real_sample = speech[i] * 32768.0f;
        if (real_sample < -32768.0)
            int16samples[i] = -32768;
        else if (real_sample > 32767.0)
            int16samples[i] = 32767;
        else
            int16samples[i] = (short)real_sample;
    }
    return i;
}

int CVoiceRxMgr::lpc10_read_bits(char *f, int *bits, int len)
{
    int i;
    int c;
    int read_index;

    read_index = 0;
    for (i = 0; i < len; i++) {
        if (i % 8 == 0)
            c = f[read_index++];
        if (c & (0x80 >> (i & 7)))
            bits[i] = 1;
        else
            bits[i] = 0;
    }
    return len;
}

SNDERR CVoiceRxMgr::add_lpc10buffer(MMIOINFO *pmmioinfo)
{
    MMIOINFO mmioinfo;
    real frame_buffer[180];
    HMMIO hfile;
    WaveBuffer *pwb;
    INT32 bit_buffer[54];
    WaveBuffer *plast_pwb;
    unsigned long dst_size;

    if (!pmmioinfo)
        return SNDERR_BAD_PARAM;
    if (!m_lpc10_decoder_state)
        return SNDERR_UNINITIALIZED;

    dst_size = 0x168;
    memset(&mmioinfo, 0, sizeof mmioinfo);
    mmioinfo.fccIOProc = FOURCC_MEM;
    mmioinfo.cchBuffer = dst_size;
    mmioinfo.adwInfo[0] = 100;

    hfile = mmioOpenA(0, &mmioinfo, MMIO_CREATE);
    if (!hfile)
        return SNDERR_BAD_FILE_OPEN;
    ++gbl_mmio;
    mmioGetInfo(hfile, &mmioinfo, 0);

    lpc10_read_bits(pmmioinfo->pchBuffer, bit_buffer, 54);

    lpc10_decode(bit_buffer, frame_buffer, m_lpc10_decoder_state);

    lpc10_write_16bit_samples((short *)mmioinfo.pchBuffer, frame_buffer, 180);

    mmioinfo.pchEndRead = mmioinfo.pchBuffer + 0x168;
    mmioinfo.pchNext = mmioinfo.pchBuffer;
    mmioSetInfo(hfile, &mmioinfo, 0);

    pwb = new WaveBuffer;
    pwb->init(&mmioinfo, &m_wfx);

    plast_pwb = m_buffers.get_last();

    m_buffers.add_to_end(pwb);

    pwb->set_end_position(pwb->btos(pmmioinfo->pchEndRead - pmmioinfo->pchBuffer));
    pwb->set_ploop_end(pwb->get_pend());
    pwb->set_ploop_start(pwb->get_pstart());

    if (plast_pwb) {
        plast_pwb->set_loop_state(1);
        plast_pwb->set_link_state(1);
        plast_pwb->set_plinked_wb(pwb);
        plast_pwb->set_loop_count(0);
    }

    m_buffers_added++;
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::get_nsamples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                                 unsigned long *ret_samples_read)
{
    HMMIO hfile;
    int moving;
    CWaveMixer *pmixer;
    PFNFILTER pfilter;
    unsigned long loop_count;
    unsigned long nsamples_processed;
    unsigned long i;
    WaveBuffer *oldpwb;
    WaveBuffer *curpwb;
    unsigned long samples_read;
    char *pdst_temp;
    char *psrc_temp;
    MMIOINFO *pmmioinfo;

    samples_read = 0;
    nsamples_processed = 0;
    moving = 0;

    pmixer = gbl_pwave_device->get_mixer();
    if (!pmixer)
        return SNDERR_UNINITIALIZED;
    if (pmixer && pmixer->uses_mmx())
        pfilter = mmx_filter_table[filter_index];
    else
        pfilter = filter_table[filter_index];
    if (filter_index >= 0xc)
        moving = 1;
    if (moving)
        pfilter = mov_8k222k16216m2smb;
    else
        pfilter = add_8k222k16216m2smb;

    if (m_flags.mixer_unload) {
        curpwb = m_buffers.remove_head();
        while (curpwb) {
            pmmioinfo = curpwb->get_mmioinfo();
            hfile = pmmioinfo->hmmio;
            if (hfile) {
                mmioClose(hfile, 0);
                gbl_created_list.remove(curpwb->get_mmioinfo());
            }
            gbl_mmio--;
            delete curpwb;
            curpwb = m_buffers.remove_head();
        }
        detach();
    }

    curpwb = m_buffers.get_first();
    if (curpwb == 0) {
        *ret_samples_read = 0;
        if (moving)
            memset(pdst, 0, nsamples * 8);
        if (m_flags.stop_flag)
            m_flags.playing = 0;
        return SNDERR_NO_DATA;
    }

    pdst_temp = (char *)pdst;
    psrc_temp = curpwb->get_pnext();
    if (moving)
        nsamples_processed = fp_mov_8k16216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, curpwb, 0);
    else
        nsamples_processed = fp_add_8k16216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, curpwb, 0);
    *ret_samples_read = nsamples_processed;

    loop_count = curpwb->get_loop_count();
    for (i = 0; i < loop_count; i++) {
        oldpwb = m_buffers.remove_head();
        if (oldpwb) {
            mmioClose(oldpwb->get_mmioinfo()->hmmio, 0);
            gbl_mmio--;
            delete oldpwb;
        }
    }
    if (loop_count != 0)
        curpwb = m_buffers.get_first();
    if (curpwb != 0) {
        curpwb->set_pnext(psrc_temp);
        if (curpwb->get_pnext() >= curpwb->get_pend() && curpwb->is_linked() == 0) {
            invalidate_positions();
            set_idle(1);
            stop();
            m_buffers.remove_head();
            mmioClose(curpwb->get_mmioinfo()->hmmio, 0);
            gbl_mmio--;
            delete curpwb;
            curpwb = 0;
        }
    } else {
        if (m_flags.stop_flag)
            m_flags.playing = 0;
    }

    if (nsamples_processed != nsamples && moving)
        memset((double *)pdst + nsamples_processed, 0, (nsamples - nsamples_processed) * 16);
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::init(unsigned long buffer_size, WAVEFORMATEX *lpwfx, unsigned long flags)
{
    if (!buffer_size)
        return SNDERR_BAD_PARAM;

    if (lpwfx)
        m_wfx = *lpwfx;
    else
        m_wfx = tstfmt;
    m_buffer_size = buffer_size;
    m_lpc10_decoder_state = create_lpc10_decoder_state();
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::start()
{
    if (m_flags.playing)
        return SNDERR_ALREADY_ACTIVE;
    if (!gbl_pwave_device || gbl_pwave_device->is_disabled())
        return SNDERR_NO_DRIVER;
    m_flags.playing = 1;
    m_flags.idle = 0;
    m_flags.stop_flag = 0;
    gbl_pwave_device->queue_request(this, REQUEST_PLAY);
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::stop()
{
    if (gbl_pwave_device)
        gbl_pwave_device->queue_request(this, REQUEST_STOP);
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::discard_buffer(HMMIO hfile)
{
    mmioClose(hfile, 0);
    gbl_mmio--;
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::enable()
{
    SNDERR result = SNDERR_OK;
    return SNDERR_OK;
}

SNDERR CVoiceRxMgr::disable()
{
    SNDERR result = SNDERR_OK;
    return SNDERR_OK;
}
