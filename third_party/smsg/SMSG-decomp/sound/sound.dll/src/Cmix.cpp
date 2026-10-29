#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <string.h>
#include "sound.h"
#include "sounddevice.h"
#include "dlist.h"
#include "dsb.h"
#include "wavefile.h"
#include "wavemgr.h"
#include "avimgr.h"
#include "cmix.h"
#include "voicerxmgr.h"

double g_MixAccumulator[32000];

CWaveMixer::CWaveMixer()
{
    m_pdsb = 0;
    m_pmixbuff = 0;
    m_nchannels = 2;
    m_hwnd = 0;
    m_mixbuff_size = 0x4000;
    m_pmixbuff = new double[0x4000];
    set_active(0);
    m_last_time = 0;
    m_hfile_dump = 0;
    m_pword_buffer = 0;
    memset(&m_wfx, 0, sizeof(m_wfx));
    m_wfx.wFormatTag = WAVE_FORMAT_PCM;
    m_wfx.cbSize = 0;
    m_fpmix_buffer = 0;
    m_wfx.nChannels = 2;
    m_wfx.nSamplesPerSec = 22050;
    m_wfx.nAvgBytesPerSec = 88200;
    m_wfx.nBlockAlign = 4;
    m_wfx.wBitsPerSample = 16;
}

CWaveMixer::~CWaveMixer()
{
    CWaveMgr *pcur = CWaveMgr::get_list_head();
    set_active(0);
    if (m_pmixbuff)
        delete m_pmixbuff;
    m_pmixbuff = 0;
}

SNDERR CWaveMixer::set_mixbuff_size(unsigned long size)
{
    int reset_active;

    if (size == m_mixbuff_size)
        return SNDERR_OK;

    reset_active = m_flags.active;
    if (reset_active == 1)
        set_active(0);

    if (m_pmixbuff)
        operator delete(m_pmixbuff);
    m_pmixbuff = 0;
    m_pmixbuff = (double *)operator new(size * sizeof(double));
    m_mixbuff_size = size;

    fill_mixbuff(0);

    set_active(reset_active);
    return SNDERR_OK;
}

void CWaveMixer::fill_mixbuff(unsigned long fill_value)
{
    unsigned long i;

    for (i = 0; i < m_mixbuff_size; i++)
        m_pmixbuff[i] = fill_value;
}

SNDERR CWaveMixer::mix_2_mb(CDSB *pdsb)
{
    unsigned long filter_index = 0;
    unsigned long sounds_processed = 0;

    if (pdsb == 0)
        return SNDERR_BAD_PARAM;

    DWORD cur_time = timeGetTime();
    if (m_last_time == 0)
        m_last_time = cur_time;

    unsigned long nbytes_to_write;
    unsigned long position;
    unsigned long samples_read;
    unsigned long nsamples;
    SNDERR retval = pdsb->get_write_info(&nbytes_to_write, &position, cur_time - m_last_time);
    if (retval == 0) {
        nsamples = pdsb->bytes_to_samples(nbytes_to_write);
        if (nsamples == 0)
            return SNDERR_OK;

        unsigned long time_past = cur_time - m_last_time;
        m_last_time = cur_time;
        unsigned long cur_delay = 0;
        m_fpmix_buffer = 0;
        int first_time = 1;
        if (gbl_pwave_device != 0) {
            void *pdsb1;
            unsigned long dsb_size1;
            void *pdsb2;
            unsigned long dsb_size2;

            {
                Sound *pcursnd = (Sound *)gbl_pwave_device->m_play_list.First();
                while (pcursnd != 0) {
                    switch (pcursnd->get_type()) {
                    case SOUNDTYPE_WAVE: {
                        CWaveMgr *pcur_wave_sound = static_cast<CWaveMgr *>(pcursnd);
                        if (pcur_wave_sound->is_loaded()) {
                            unsigned long filter_index;
                            if (pcur_wave_sound->is_mpeg() == 0) {
                                if (pcur_wave_sound->is_playing() && pcur_wave_sound->is_hwbuffer() == 0) {
                                    pcur_wave_sound->set_time_past(time_past);
                                    filter_index = pcur_wave_sound->get_format()->wBitsPerSample == 8
                                                       ? (m_wfx.wBitsPerSample != 16 ? 4 : 8)
                                                       : 0;
                                    if (pcur_wave_sound->get_format()->nChannels == 1)
                                        filter_index += 2;
                                    if (m_wfx.nChannels == 1)
                                        filter_index += 1;
                                    if (first_time) {
                                        filter_index += 12;
                                        first_time = 0;
                                    }
                                    pcur_wave_sound->get_nsamples(g_MixAccumulator, nsamples, filter_index,
                                                                  &samples_read);
                                    sounds_processed++;
                                }
                                if (pcur_wave_sound->is_mpeg() == 0) {
                                    if (pcur_wave_sound->is_playing() && pcur_wave_sound->is_hwbuffer())
                                        pcur_wave_sound->UpdateCompletionState();
                                    if (pcur_wave_sound->is_mpeg() == 0) {
                                        if (pcur_wave_sound->is_playing() && pcur_wave_sound->is_hwbuffer()
                                            && pcur_wave_sound->is_static() == 0) {
                                            pcur_wave_sound->stream_to_hwbuffer(time_past, 0, &samples_read);
                                            pcur_wave_sound->set_time_past(time_past);
                                        }
                                        if (pcur_wave_sound->is_mpeg() == 0)
                                            break;
                                    }
                                }
                            }
                            if (pcur_wave_sound->is_hwbuffer()) {
                                pcur_wave_sound->get_mpeg_samples_to_dsb(time_past, 0, &samples_read);
                                pcur_wave_sound->set_time_past(time_past);
                            }
                            if (pcur_wave_sound->is_mpeg() != 0 && pcur_wave_sound->phwdsb() != 0
                                && pcur_wave_sound->phwdsb()->is_playing() == 0)
                                pcur_wave_sound->set_done();
                        }
                        break;
                    }

                    case SOUNDTYPE_AVI: {
                        CAviMgr *pcur_avi_sound = (CAviMgr *)pcursnd;
                        if (pcursnd->is_playing()) {
                            if (!pcur_avi_sound->are_positions_valid()) {
                                pcur_avi_sound->set_start_position(position);
                                pcur_avi_sound->set_prev_play_position(-(int)pdsb->get_cur_play_write_interval());
                                pcur_avi_sound->validate_positions();
                            } else {
                                pcur_avi_sound->set_prev_play_position(pcur_avi_sound->get_prev_play_position()
                                                                       + pdsb->get_play_interval());
                            }
                            pcur_avi_sound->update_cur_frame();
                            unsigned long filter_index = pcur_avi_sound->get_format()->wBitsPerSample == 8
                                                             ? (m_wfx.wBitsPerSample != 16 ? 4 : 8)
                                                             : 0;
                            if (pcur_avi_sound->get_format()->nChannels == 1)
                                filter_index += 2;
                            if (m_wfx.nChannels == 1)
                                filter_index += 1;
                            if (first_time) {
                                filter_index += 12;
                                first_time = 0;
                            }
                            pcur_avi_sound->get_nsamples((int *)m_pmixbuff, nsamples, filter_index, &samples_read);
                        }
                        break;
                    }

                    case SOUNDTYPE_VOICERX: {
                        CVoiceRxMgr *pcur_vrx_sound = (CVoiceRxMgr *)pcursnd;
                        if (pcursnd->is_playing()) {
                            unsigned long filter_index = pcur_vrx_sound->get_format()->wBitsPerSample == 8
                                                             ? (m_wfx.wBitsPerSample != 16 ? 4 : 8)
                                                             : 0;
                            if (pcur_vrx_sound->get_format()->nChannels == 1)
                                filter_index += 2;
                            if (m_wfx.nChannels == 1)
                                filter_index += 1;
                            if (first_time) {
                                filter_index += 12;
                                first_time = 0;
                            }
                            pcur_vrx_sound->get_nsamples(g_MixAccumulator, nsamples, filter_index, &samples_read);
                        }
                        break;
                    }
                    }
                    pcursnd = (Sound *)gbl_pwave_device->m_play_list.Next();
                }
            }

            if (first_time) {
                pdsb->lock(position, nbytes_to_write, &pdsb1, &dsb_size1, &pdsb2, &dsb_size2);
                memset(pdsb1, 0, dsb_size1);
                if (pdsb2 != 0)
                    memset(pdsb2, 0, dsb_size2);
                if (m_hfile_dump != 0) {
                    mmioWrite(m_hfile_dump, (const char *)pdsb1, dsb_size1);
                    if (pdsb2 != 0)
                        mmioWrite(m_hfile_dump, (const char *)pdsb2, dsb_size2);
                }
                pdsb->unlock(pdsb1, dsb_size1, pdsb2, dsb_size2);
                clean_play_list();
                return SNDERR_OK;
            }
            fpmb_2_dsb(pdsb, g_MixAccumulator, nbytes_to_write, position);
            clean_play_list();
            return SNDERR_OK;
        }
        return SNDERR_NO_DEVICE;
    }
    return retval;
}

SNDERR CWaveMixer::mix_2_3d_mb(CDSB *pdsb_left, CDSB *pdsb_right)
{
    unsigned long filter_index = 0;
    unsigned long sounds_processed = 0;

    if (pdsb_left == 0 || pdsb_right == 0)
        return SNDERR_BAD_PARAM;

    DWORD cur_time = timeGetTime();
    if (m_last_time == 0)
        m_last_time = cur_time;

    unsigned long avi_position = 0;

    unsigned long nbytes_to_write_left;
    unsigned long position_left;
    unsigned long position_right;
    unsigned long samples_read;

    SNDERR retval = pdsb_left->get_write_info(&nbytes_to_write_left, &position_left, cur_time - m_last_time);
    if (retval != 0) {
        if (retval != 0x2d)
            return retval;
        pdsb_right->m_flags.virgin = 1;
    }
    retval = pdsb_right->get_write_info(nbytes_to_write_left, &position_right);
    if (retval != 0)
        return retval;

    unsigned long nsamples = pdsb_left->bytes_to_samples(nbytes_to_write_left);

    DWORD time_past = cur_time - m_last_time;
    m_last_time = cur_time;
    unsigned long cur_delay = 0;
    m_fpmix_buffer = 0;
    int first_time = 1;

    if (gbl_pwave_device != 0) {
        void *pdsb1_left;
        unsigned long dsb_size1_left;
        void *pdsb2_left;
        unsigned long dsb_size2_left;
        void *pdsb1_right;
        unsigned long dsb_size1_right;
        void *pdsb2_right;
        unsigned long dsb_size2_right;

        {
            Sound *pcursnd = (Sound *)gbl_pwave_device->m_play_list.First();
            while (pcursnd != 0) {
                switch (pcursnd->get_type()) {
                case SOUNDTYPE_WAVE: {
                    CWaveMgr *pcur_wave_sound = (CWaveMgr *)pcursnd;
                    if (pcur_wave_sound->is_loaded()) {
                        if (pcur_wave_sound->is_hwbuffer() == 0 && pcur_wave_sound->is_playing()) {
                            pcur_wave_sound->set_time_past(time_past);
                            unsigned long filter_index = pcur_wave_sound->get_format()->wBitsPerSample == 8
                                                             ? (m_wfx.wBitsPerSample != 0x10 ? 4 : 8)
                                                             : 0;
                            if (pcur_wave_sound->get_format()->nChannels == 1)
                                filter_index += 2;
                            if (m_wfx.nChannels == 1)
                                filter_index += 1;
                            if (first_time) {
                                filter_index += 12;
                                first_time = 0;
                            }
                            pcur_wave_sound->get_nsamples(g_MixAccumulator, nsamples, filter_index, &samples_read);
                            sounds_processed++;
                        }
                        if (pcur_wave_sound->is_hwstreaming())
                            pcur_wave_sound->get_mpeg_samples_to_dsb(nsamples, 0, &samples_read);
                        if (pcur_wave_sound->is_hwbuffer() && pcur_wave_sound->phwdsb() != 0
                            && pcur_wave_sound->phwdsb()->is_playing() == 0)
                            pcur_wave_sound->set_done();
                    }
                } break;

                case SOUNDTYPE_AVI:
                    if (pcursnd->is_playing()) {
                        CAviMgr *pcur_avi_sound = (CAviMgr *)pcursnd;
                        if (!pcur_avi_sound->are_positions_valid()) {
                            pcur_avi_sound->set_start_position(avi_position);
                            pcur_avi_sound->set_prev_play_position(-(int)pdsb_left->get_cur_play_write_interval());
                            pcur_avi_sound->validate_positions();
                        } else {
                            pcur_avi_sound->set_prev_play_position(pcur_avi_sound->get_prev_play_position()
                                                                   + pdsb_left->get_play_interval());
                        }
                        pcur_avi_sound->update_cur_frame();
                        unsigned long filter_index = pcur_avi_sound->get_format()->wBitsPerSample == 8
                                                         ? (m_wfx.wBitsPerSample != 0x10 ? 4 : 8)
                                                         : 0;
                        if (pcur_avi_sound->get_format()->nChannels == 1)
                            filter_index += 2;
                        if (m_wfx.nChannels == 1)
                            filter_index += 1;
                        if (first_time) {
                            filter_index += 12;
                            first_time = 0;
                        }
                        pcur_avi_sound->get_nsamples((int *)m_pmixbuff, nsamples, filter_index, &samples_read);
                    }
                    break;

                case SOUNDTYPE_VOICERX:
                    if (pcursnd->is_playing()) {
                        CVoiceRxMgr *pcur_vrx_sound = (CVoiceRxMgr *)pcursnd;
                        unsigned long filter_index = pcur_vrx_sound->get_format()->wBitsPerSample == 8
                                                         ? (m_wfx.wBitsPerSample != 0x10 ? 4 : 8)
                                                         : 0;
                        if (pcur_vrx_sound->get_format()->nChannels == 1)
                            filter_index += 2;
                        if (m_wfx.nChannels == 1)
                            filter_index += 1;
                        if (first_time) {
                            filter_index += 12;
                            first_time = 0;
                        }
                        pcur_vrx_sound->get_nsamples(g_MixAccumulator, nsamples, filter_index, &samples_read);
                    }
                    break;
                }

                pcursnd = (Sound *)gbl_pwave_device->m_play_list.Next();
            }
        }

        if (first_time) {
            pdsb_left->lock(position_left, nbytes_to_write_left, &pdsb1_left, &dsb_size1_left, &pdsb2_left,
                            &dsb_size2_left);
            pdsb_right->lock(position_right, nbytes_to_write_left, &pdsb1_right, &dsb_size1_right, &pdsb2_right,
                             &dsb_size2_right);
            memset(pdsb1_left, 0, dsb_size1_left);
            memset(pdsb1_right, 0, dsb_size1_right);
            if (pdsb2_left != 0)
                memset(pdsb2_left, 0, dsb_size2_left);
            if (pdsb2_right != 0)
                memset(pdsb2_right, 0, dsb_size2_right);
            if (m_hfile_dump != 0) {
                mmioWrite(m_hfile_dump, (const char *)pdsb1_left, dsb_size1_left);
                if (pdsb2_left != 0)
                    mmioWrite(m_hfile_dump, (const char *)pdsb2_left, dsb_size2_left);
            }
            pdsb_left->unlock(pdsb1_left, dsb_size1_left, pdsb2_left, dsb_size2_left);
            pdsb_right->unlock(pdsb1_right, dsb_size1_right, pdsb2_right, dsb_size2_right);
            clean_play_list();
            return SNDERR_OK;
        }
        fpmb_2_3ddsb(pdsb_left, pdsb_right, g_MixAccumulator, nbytes_to_write_left, position_left, position_right);
        clean_play_list();
        return SNDERR_OK;
    }
    return SNDERR_NO_DEVICE;
}

SNDERR CWaveMixer::mix_2_fpbuffer(double *fpbuffer1, double *fpbuffer2, unsigned long nsamples)
{
    __asm {
        push    ecx
        push    esi
        push    edi
        fninit
        mov     ecx, nsamples
        shl     ecx, 1
        mov     edi, fpbuffer1
        mov     esi, fpbuffer2
    lp0:
        fld     qword ptr [edi]
        fadd    qword ptr [esi]
        fstp    qword ptr [edi]
        add     esi, 8
        add     edi, 8
        dec     ecx
        jnz     lp0
        pop     edi
        pop     esi
        pop     ecx
    }
    return SNDERR_OK;
}

SNDERR CWaveMixer::mb_2_dsb(CDSB *pdsb, unsigned long nbytes, unsigned long position)
{
    return SNDERR_OK;
}

void CWaveMixer::left_fp2word(short *dst, double *src, unsigned long nwords)
{
    int cursample;

    __asm {
        push    esi
        push    edi
        push    ecx
        fninit
        mov     edi, dst
        mov     esi, src
        mov     ecx, nwords
    lp1:
        fld     qword ptr [esi]
        fistp   dword ptr cursample
        cmp     cursample, 32767
        jg      hi1
        cmp     cursample, -32768
        jl      lo1
        mov     eax, cursample
    st1:
        mov     word ptr [edi], ax
        add     esi, 16
        add     edi, 2
        dec     ecx
        jnz     lp1
        jmp     dn1
    hi1:
        mov     ax, 32767
        jmp     st1
    lo1:
        mov     ax, 0x8000
        jmp     st1
    dn1:
        pop     ecx
        pop     edi
        pop     esi
    }

    if (m_hfile_dump != 0) mmioWrite(m_hfile_dump, (const char *)dst, nwords * 2);
}

void CWaveMixer::right_fp2word(short *dst, double *src, unsigned long nwords)
{
    int cursample;

    __asm {
        push    esi
        push    edi
        push    ecx
        fninit
        mov     edi, dst
        mov     esi, src
        mov     ecx, nwords
        add     esi, 8
    lp1:
        fld     qword ptr [esi]
        fistp   dword ptr cursample
        cmp     cursample, 32767
        jg      hi1
        cmp     cursample, -32768
        jl      lo1
        mov     eax, cursample
    st1:
        mov     word ptr [edi], ax
        add     esi, 16
        add     edi, 2
        dec     ecx
        jnz     lp1
        jmp     dn1
    hi1:
        mov     ax, 32767
        jmp     st1
    lo1:
        mov     ax, 0x8000
        jmp     st1
    dn1:
        pop     ecx
        pop     edi
        pop     esi
    }

    if (m_hfile_dump != 0) mmioWrite(m_hfile_dump, (const char *)dst, nwords * 2);
}

void CWaveMixer::fp2word(short *dst, double *src, unsigned long nwords)
{
    int cursample;

    __asm {
        push    esi
        push    edi
        push    ecx
        fninit
        mov     edi, dst
        mov     esi, src
        mov     ecx, nwords
    lp1:
        fld     qword ptr [esi]
        fistp   dword ptr cursample
        cmp     cursample, 32767
        jg      hi1
        cmp     cursample, -32768
        jl      lo1
        mov     eax, cursample
    st1:
        mov     word ptr [edi], ax
        add     esi, 8
        add     edi, 2
        dec     ecx
        jnz     lp1
        jmp     dn1
    hi1:
        mov     ax, 32767
        jmp     st1
    lo1:
        mov     ax, 0x8000
        jmp     st1
    dn1:
        pop     ecx
        pop     edi
        pop     esi
    }

    if (m_hfile_dump != 0) mmioWrite(m_hfile_dump, (const char *)dst, nwords * 2);
}

SNDERR CWaveMixer::fpmb_2_dsb(CDSB *pdsb, double *pmix_buffer, DWORD nbytes, DWORD position)
{
    void *pdsb1, *pdsb2;
    DWORD dsb_size1, dsb_size2;
    int mixsize1;
    short *wpsrc, *wpdest;
    int i = 0;

    if (pdsb->lock(position, nbytes, &pdsb1, &dsb_size1, &pdsb2, &dsb_size2) != 0)
        return SNDERR_BAD_DSB_LOCK;

    mixsize1 = pdsb->bytes_to_samples(dsb_size1);
    mixsize1 <<= 1;

    fp2word((short *)pdsb1, pmix_buffer, mixsize1);

    if (m_pword_buffer != 0) {
        wpsrc = m_pword_buffer;
        wpdest = (short *)pdsb1;
        for (i = 0; i < mixsize1; i++)
            wpdest[i] += wpsrc[i];
    }

    if (pdsb2 != 0) {
        int mixsize2;

        mixsize2 = pdsb->bytes_to_samples(dsb_size2);
        mixsize2 <<= 1;
        fp2word((short *)pdsb2, pmix_buffer + mixsize1, mixsize2);

        if (m_pword_buffer != 0) {
            wpsrc = m_pword_buffer + i;
            wpdest = (short *)pdsb2;
            for (i = 0; i < mixsize2; i++)
                wpdest[i] += wpsrc[i];
        }
    }

    pdsb->unlock(pdsb1, dsb_size1, pdsb2, dsb_size2);

    if (m_pword_buffer != 0) {
        delete m_pword_buffer;
        m_pword_buffer = 0;
    }
    return SNDERR_OK;
}

SNDERR CWaveMixer::fpmb_2_3ddsb(CDSB *pdsb_l, CDSB *pdsb_r, double *pmix_buffer, DWORD nbytes, DWORD position_l,
                                DWORD position_r)
{
    void *pdsb1_l;
    DWORD dsb_size1_l;
    void *pdsb2_l;
    DWORD dsb_size2_l;
    void *pdsb1_r;
    DWORD dsb_size1_r;
    void *pdsb2_r;
    DWORD dsb_size2_r;
    int mixsize1_l, mixsize1_r;
    int i = 0;

    if (pdsb_l->lock(position_l, nbytes, &pdsb1_l, &dsb_size1_l, &pdsb2_l, &dsb_size2_l) != 0)
        return SNDERR_BAD_DSB_LOCK;

    if (pdsb_r->lock(position_r, nbytes, &pdsb1_r, &dsb_size1_r, &pdsb2_r, &dsb_size2_r) != 0) {
        pdsb_l->unlock(pdsb1_l, dsb_size1_l, pdsb2_l, dsb_size2_l);
        return SNDERR_BAD_DSB_LOCK;
    }

    mixsize1_l = pdsb_l->bytes_to_samples(dsb_size1_l);

    left_fp2word((short *)pdsb1_l, pmix_buffer, mixsize1_l);

    mixsize1_r = pdsb_r->bytes_to_samples(dsb_size1_r);

    right_fp2word((short *)pdsb1_r, pmix_buffer, mixsize1_r);

    if (pdsb2_l != 0) {
        int mixsize2_l;

        mixsize2_l = pdsb_l->bytes_to_samples(dsb_size2_l);

        left_fp2word((short *)pdsb2_l, pmix_buffer + mixsize1_l * 2, mixsize2_l);
    }

    if (pdsb2_r != 0) {
        int mixsize2_r;

        mixsize2_r = pdsb_r->bytes_to_samples(dsb_size2_r);

        right_fp2word((short *)pdsb2_r, pmix_buffer + mixsize1_r * 2, mixsize2_r);
    }

    pdsb_l->unlock(pdsb1_l, dsb_size1_l, pdsb2_l, dsb_size2_l);
    pdsb_r->unlock(pdsb1_r, dsb_size1_r, pdsb2_r, dsb_size2_r);

    if (m_pword_buffer != 0) {
        delete m_pword_buffer;
        m_pword_buffer = 0;
    }
    return SNDERR_OK;
}

SNDERR CWaveMixer::clean_play_list()
{
    CWaveMgr *pcur_wave_sound = 0;
    CAviMgr *pcur_avi_sound = 0;
    CVoiceRxMgr *pcur_vrx_sound = 0;
    unsigned long cur_time = timeGetTime();
    int first_time = 1;

    SndNodeList *play_list = &gbl_pwave_device->m_play_list;

    Sound *pcursnd = (Sound *)play_list->First();

    while (pcursnd) {
        switch (pcursnd->get_type()) {
        case SOUNDTYPE_WAVE:
            pcur_wave_sound = (CWaveMgr *)pcursnd;
            if (pcur_wave_sound->is_playing() == 0) {
                pcursnd = (Sound *)play_list->Next();
                play_list->Remove(pcur_wave_sound);

                if (pcur_wave_sound->CWaveFile::m_flags.release_cur_sound)
                    pcur_wave_sound->ClearAndReleaseStopNotifyFlag();

                pcur_wave_sound->set_active(0);

                if (pcur_wave_sound->is_dynamic_buffer()
                    || (pcur_wave_sound->is_discard() && !pcur_wave_sound->is_amb_sound())) {
                    gbl_pwave_device->post_GT_request(pcur_wave_sound, REQUEST_UNLOAD);
                } else if (pcur_wave_sound->is_static() == 0 && pcur_wave_sound->is_shared_dsb() == 0) {
                    gbl_pwave_device->post_GT_request(pcur_wave_sound, REQUEST_RESET);
                } else if (pcur_wave_sound->is_static() == 0 && pcur_wave_sound->is_shared_dsb() != 0) {
                    delete pcur_wave_sound;
                } else {
                    pcur_wave_sound->reset();
                }
            } else {
                pcursnd = (Sound *)play_list->Next();
            }
            break;

        case SOUNDTYPE_AVI:
            pcur_avi_sound = (CAviMgr *)pcursnd;
            if (pcur_avi_sound->is_playing() == 0) {
                pcursnd = pcursnd->get_next();
                if (pcur_avi_sound->need_mixer_unload()) {
                    pcur_avi_sound->detach();
                    pcur_avi_sound->release();
                }
            } else {
                pcursnd = pcursnd->get_next();
            }
            break;

        case SOUNDTYPE_VOICERX:
            pcur_vrx_sound = (CVoiceRxMgr *)pcursnd;
            if (pcur_vrx_sound->is_playing() == 0) {
                pcursnd = pcursnd->get_next();
                pcur_vrx_sound->detach();
            } else {
                pcursnd = pcursnd->get_next();
            }
            break;

        default:
            pcursnd = pcursnd->get_next();
            break;
        }
    }

    return SNDERR_OK;
}

SNDERR CWaveMixer::open_dump_file(char *fname)
{
    MMIOINFO mmioinfo;

    memset(&mmioinfo, 0, sizeof(mmioinfo));
    mmioinfo.cchBuffer = 0x1000;
    mmioinfo.fccIOProc = 0x204d454d;

    m_hfile_dump = mmioOpenA(fname, &mmioinfo, MMIO_ALLOCBUF | MMIO_CREATE | MMIO_WRITE);
    if (m_hfile_dump == 0)
        return SNDERR_BAD_FILE_OPEN;

    gbl_mmio++;
    return SNDERR_OK;
}

SNDERR CWaveMixer::close_dump_file()
{
    if (m_hfile_dump)
        mmioClose(m_hfile_dump, 0);
    gbl_mmio--;
    return SNDERR_OK;
}

int CWaveMixer::get_filter_index(WAVEFORMATEX *pwfx, WAVEFORMATEX *pwfx_dsb)
{
    int index;
    if (pwfx->wBitsPerSample == 8)
        index = pwfx_dsb->wBitsPerSample == 16 ? 8 : 4;
    else
        index = 0;
    if (pwfx->nChannels == 1)
        index += 2;
    if (pwfx_dsb->nChannels == 1)
        index++;
    return index;
}

#include "SndNodeList.inl"
