#include <windows.h>
#include <mmsystem.h>
#include "sound.h"
#include "dlist.h"
#include "wavefile.h"
#include "avimgr.h"

CAviMgr::CAviMgr()
{
    m_cur_frame = 0;
    memset(&m_flags, 0, sizeof(m_flags));
    m_frame_buffer_size = 0;
    m_frame_rate = 0;
    m_frame_size = 0;
    memset(&m_out_buffer, 0, sizeof(m_out_buffer));
    m_pdsb = 0;
    m_rem_frame_time = 0;
    set_type(SOUNDTYPE_AVI);
    m_prev_play_position = 0;
    m_rem_frame_time = 0;
    m_start_frame = 0;
    m_start_position = 0;
    memset(&m_wfx, 0, sizeof(m_wfx));
}

CAviMgr::~CAviMgr()
{
    HMMIO hfile;
    MMIOINFO *pmmioinfo;
    WaveBuffer *pwb;

    pwb = m_frames.remove_head();
    while (pwb) {
        pmmioinfo = pwb->get_mmioinfo();
        hfile = pmmioinfo->hmmio;
        if (hfile) {
            mmioClose(hfile, 0);
            gbl_mmio--;
        }
        delete pwb;
        pwb = m_frames.remove_head();
    }
}

SNDERR CAviMgr::detach()
{
    AviSound::detach();
    return SNDERR_OK;
}

SNDERR CAviMgr::release()
{
    HMMIO hfile;
    MMIOINFO *pmmioinfo;
    WaveBuffer *pwb;

    stop();

    if (is_attached()) {
        set_mixer_unload();
        return SNDERR_OK;
    }

    pwb = m_frames.remove_head();
    while (pwb) {
        pmmioinfo = pwb->get_mmioinfo();
        hfile = pmmioinfo->hmmio;
        if (hfile) {
            mmioClose(hfile, 0);
            gbl_mmio--;
        }
        delete pwb;
        pwb = m_frames.remove_head();
    }
    return SNDERR_OK;
}

MMIOINFO *CAviMgr::get_frame(unsigned long size)
{
    MMIOINFO mmioinfo;
    HMMIO hfile;

    if (m_out_buffer.hmmio != 0)
        return 0;

    memset(&mmioinfo, 0, sizeof(mmioinfo));
    mmioinfo.fccIOProc = mmioFOURCC('M', 'E', 'M', ' ');
    mmioinfo.cchBuffer = (size != 0) ? size : m_frame_buffer_size;
    mmioinfo.adwInfo[0] = 100;

    hfile = mmioOpenA(0, &mmioinfo, MMIO_CREATE);
    if (hfile == 0)
        return 0;

    gbl_mmio = gbl_mmio + 1;
    mmioGetInfo(hfile, &m_out_buffer, 0);
    return &m_out_buffer;
}

SNDERR CAviMgr::get_nsamples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                             unsigned long *ret_samples_read)
{
    HMMIO hfile;
    int moving;
    PFNFILTER pfilter;
    unsigned long loop_count;
    unsigned long nsamples_processed;
    unsigned long i;
    WaveBuffer *curpwb;
    unsigned long samples_read;
    char *pdst_temp;
    char *psrc_temp;
    MMIOINFO *pmmioinfo;
    WaveBuffer *nextpwb;

    samples_read = 0;
    nsamples_processed = 0;
    moving = 0;

    pfilter = filter_table[filter_index];
    if (filter_index >= 0xc)
        moving = 1;

    if (m_flags.mixer_unload) {
        curpwb = m_frames.remove_head();
        while (curpwb) {
            pmmioinfo = curpwb->get_mmioinfo();
            hfile = pmmioinfo->hmmio;
            if (hfile) {
                mmioClose(hfile, 0);
                gbl_mmio--;
            }
            delete curpwb;
            curpwb = m_frames.remove_head();
        }
        detach();
    }

    curpwb = m_frames.get_first();
    if (curpwb == 0) {
        *ret_samples_read = 0;
        if (moving)
            memset(pdst, 0, nsamples * 8);
        return SNDERR_NO_DATA;
    }

    pdst_temp = (char *)pdst;
    psrc_temp = curpwb->get_pnext();
    nsamples_processed = pfilter((void **)&pdst_temp, (void **)&psrc_temp, nsamples, curpwb, 0);

    *ret_samples_read = nsamples_processed;

    loop_count = curpwb->get_loop_count();
    for (i = 0; i < loop_count; i++) {
        nextpwb = m_frames.get_next();
        m_frames.remove_head();
        mmioClose(curpwb->get_mmioinfo()->hmmio, 0);
        --gbl_mmio;
        delete curpwb;
        curpwb = m_frames.get_first();
    }

    if (curpwb != 0) {
        curpwb->set_pnext(psrc_temp);
        if (curpwb->get_pnext() >= curpwb->get_pend()) {
            if (curpwb->is_linked() == 0) {
                m_start_frame += m_cur_frame + 1;
                m_cur_frame = 0;
                invalidate_positions();
                set_idle(1);
                stop();
                m_frames.remove_head();
                mmioClose(curpwb->get_mmioinfo()->hmmio, 0);
                --gbl_mmio;
                delete curpwb;
            }
        }
    }

    if (nsamples_processed != nsamples && moving)
        memset(pdst_temp, 0, (nsamples - nsamples_processed) * 8);
    return SNDERR_OK;
}

SNDERR CAviMgr::return_frame(MMIOINFO *pmmioinfo)
{
    if (pmmioinfo != &m_out_buffer)
        return SNDERR_BAD_PARAM;

    unsigned long nbytes_filled = pmmioinfo->pchEndRead - pmmioinfo->pchBuffer;
    if (nbytes_filled == 0) {
        mmioClose(m_out_buffer.hmmio, 0);
        gbl_mmio--;
        m_out_buffer.hmmio = 0;
        return SNDERR_NO_DATA;
    }

    WaveBuffer *pwb = new WaveBuffer;
    pwb->init(pmmioinfo, &m_wfx);

    WaveBuffer *plast_pwb = m_frames.get_last();

    m_frames.add_to_end(pwb);

    m_out_buffer.hmmio = 0;
    pwb->set_end_position(pwb->btos(nbytes_filled));
    pwb->set_ploop_end(pwb->get_pend());
    pwb->set_ploop_start(pwb->get_pstart());

    if (plast_pwb) {
        plast_pwb->set_loop_state(1);
        plast_pwb->set_link_state(1);
        plast_pwb->set_plinked_wb(pwb);
        plast_pwb->set_loop_count(0);
    }

    if (is_idle() && !m_flags.playing) {
        m_flags.playing = 1;
        m_flags.active = 1;
        m_flags.idle = 0;
    }

    return SNDERR_OK;
}

SNDERR CAviMgr::init(AVIStreamHeader *paudheader, AVIStreamHeader *pvidheader, WAVEFORMATEX *lpwfx, unsigned long flags)
{
    if (paudheader == 0 || lpwfx == 0 || pvidheader == 0)
        return SNDERR_BAD_PARAM;

    m_wfx = *lpwfx;
    m_frame_rate = pvidheader->dwRate / pvidheader->dwScale;
    m_frame_size = paudheader->dwRate / m_frame_rate;
    m_frame_buffer_size = paudheader->dwSuggestedBufferSize;
    return SNDERR_OK;
}

SNDERR CAviMgr::start(unsigned long start_frame)
{
    if (m_flags.playing)
        return SNDERR_ALREADY_ACTIVE;

    m_start_frame = start_frame;
    m_flags.playing = 1;
    m_flags.active = 1;
    m_flags.idle = 0;
    m_cur_frame = 0;

    attach();
    return SNDERR_OK;
}

SNDERR CAviMgr::start()
{
    if (m_flags.playing)
        return SNDERR_ALREADY_ACTIVE;
    m_flags.playing = 1;
    m_flags.active = 1;
    m_flags.idle = 0;
    return SNDERR_OK;
}

SNDERR CAviMgr::stop()
{
    m_flags.playing = 0;
    m_flags.positions_valid = 0;
    return SNDERR_OK;
}

SNDERR CAviMgr::discard_frame(HMMIO hfile)
{
    mmioClose(hfile, 0);
    gbl_mmio--;
    return SNDERR_OK;
}

void CAviMgr::update_cur_frame()
{
    if (m_frame_size == 0)
        return;
    if (m_prev_play_position == 0)
        return;

    double rem_frame_time = 0;
    int frame_size_samples = btos(m_frame_size);

    m_cur_frame = m_prev_play_position / frame_size_samples;

    rem_frame_time = m_prev_play_position % frame_size_samples;
    rem_frame_time = rem_frame_time / m_wfx.nSamplesPerSec * 1000.0;
    m_rem_frame_time = (int)rem_frame_time;
}
