#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "sounddevice.h"
#include "dsb.h"
#include "mpeg.h"
#include "wavefile.h"
#include "dllwavedevice.h"
#include "dlist.h"
#include "cmix.h"
#include "ctimer.h"


CWaveFile::CWaveFile()
{
    m_hfile = 0;
    memset(&m_mmioinfo, 0, sizeof m_mmioinfo);
    m_phwdsb = 0;
    memset(&m_flags, 0, sizeof(m_flags));
    m_curstartpos = 0;
    m_curendpos = 0;
    m_curpos = 0;
    m_curfpos = 0;
    m_dataoffset = 0;
    m_hwnd = 0;
    m_volume = 0x10000;
    m_fp_volume = 1.0;
    m_reset_volume = 0x10000;
    m_fp_reset_volume = 1.0;
    m_left_volume = 0x10000;
    m_fp_left_volume = 1.0;
    m_reset_left_volume = 0x10000;
    m_right_volume = 0x10000;
    m_fp_right_volume = 1.0;
    m_reset_right_volume = 0x10000;
    m_fade_time = 1000;
    set_fade(m_fade_time);
    m_pan = 0;
    m_pitch = 0x10000;
    m_time = 0;
    m_pmmioinfo = 0;
    m_delay = 0;
    m_time_past = 0;
    m_loop_count = 0;
    m_loop_size = 0;
    m_loop_startpos = 0;
    memset(m_fname, 0, sizeof m_fname);
    m_msgs_pending = 0;
    m_fp_shifter = 0.0;
    m_fp_pitch = 1.0;
    m_shifter.whole = 0;
    for (int i = 0; i < 16; i++)
        instance[i].set_time_past(&m_time_past);
    m_nmpeg_frames = 0;
    m_first_mpeg_frame = 0;
    m_last_mpeg_frame = 0;
    m_ms_length = 0;
    m_ppQueuedSound = 0;
    m_ppCurSound = 0;
}

CWaveFile::CWaveFile(char *fname)
{
    m_hfile = 0;
    memset(&m_mmioinfo, 0, sizeof m_mmioinfo);
    m_phwdsb = 0;
    memset(&m_flags, 0, sizeof(m_flags));
    m_curstartpos = 0;
    m_curendpos = 0;
    m_curpos = 0;
    m_curfpos = 0;
    m_dataoffset = 0;
    m_hwnd = 0;
    m_volume = 0x10000;
    m_reset_volume = 0x10000;
    m_left_volume = 0x10000;
    m_reset_left_volume = 0x10000;
    m_right_volume = 0x10000;
    m_reset_right_volume = 0x10000;
    m_fade_time = 1000;
    set_fade(m_fade_time);
    m_pan = 0;
    m_pmmioinfo = 0;
    m_time = 0;
    m_delay = 0;
    m_time_past = 0;
    m_loop_count = 0;
    m_loop_size = 0;
    m_loop_startpos = 0;
    m_msgs_pending = 0;
    m_nmpeg_frames = 0;
    m_first_mpeg_frame = 0;
    m_last_mpeg_frame = 0;
    m_ppQueuedSound = 0;
    m_ppCurSound = 0;
    m_ms_length = 0;
    load(fname);
}

void CWaveFile::undo_load()
{
    if (m_pmmioinfo != 0) {
        mmioerr(m_pmmioinfo->wErrorRet);
        delete m_pmmioinfo;
        m_pmmioinfo = 0;
    }
    memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
    memset(&m_flags, 0, sizeof(m_flags));
    m_flags.all_in_buffer = 0;
    m_flags.loaded = 0;
    if (m_hfile != 0) {
        mmioClose(m_hfile, 0);
        gbl_created_list.remove(&m_mmioinfo);
        gbl_mmio--;
    }
    m_hfile = 0;
}

SNDERR CWaveFile::hw_load(CDSMgr *pds)
{
    void *p1;
    unsigned long s1;
    void *p2;
    unsigned long s2;
    if (pds != 0) {
        SNDERR retval = SNDERR_OK;
        if (pds->create_dsb(&m_phwdsb, &m_wfx, 0x100e0, m_data_size) == 0) {
            m_flags.hwbuffer = 1;
            if (m_phwdsb->lock(0, m_data_size, &p1, &s1, &p2, &s2) == 0) {
                if (s1 == m_data_size) {
                    if (mmioRead(m_hfile, (char *)p1, s1) != (long)s1) {
                        m_phwdsb->unlock(p1, s1, p2, s2);
                        pds->destroy_dsb(m_phwdsb);
                        return SNDERR_MMIO_FAILURE;
                    }
                    m_phwdsb->unlock(p1, s1, p2, s2);
                    m_phwdsb->set_owner(this);
                }
            }
            m_pitch = 22050;
            return SNDERR_OK;
        }
        return SNDERR_NO_MEM;
    }
    m_pitch = 22050;
    return SNDERR_OK;
}

SNDERR CWaveFile::load_3d(CDSMgr *pds)
{
    void *p1;
    unsigned long s1;
    unsigned long s2;
    void *p2;

    if (pds != 0) {
        if (!pds->is_3d())
            return SNDERR_NO_SUPPORT;
        SNDERR retval = SNDERR_OK;
        if (pds->create_dsb(&m_phwdsb, &m_wfx, 0x100f4, m_data_size) == 0) {
            m_flags.hwbuffer = 1;
            if (m_phwdsb->lock(0, m_data_size, &p1, &s1, &p2, &s2) == 0) {
                if (s1 == m_data_size) {
                    if (mmioRead(m_hfile, (char *)p1, s1) != (long)s1) {
                        m_phwdsb->unlock(p1, s1, p2, s2);
                        pds->destroy_dsb(m_phwdsb);
                        return SNDERR_MMIO_FAILURE;
                    }
                    m_phwdsb->unlock(p1, s1, p2, s2);
                    m_phwdsb->set_owner(this);
                }
            }
            m_pitch = 22050;
            return SNDERR_OK;
        }
        return SNDERR_NO_MEM;
    }
    return SNDERR_NO_DRIVER;
}

void CWaveFile::add_dup_hwdsb(CDSB *pduphwdsb)
{
    if (pduphwdsb) {
        pduphwdsb->set_owner(this);
        m_dup_hwdsb_list.add_to_end(pduphwdsb);
    }
}

void CWaveFile::remove_dup_hwdsb(CDSB *pduphwdsb)
{
    if (pduphwdsb != 0) {
        pduphwdsb->set_owner(0);
        m_dup_hwdsb_list.remove(pduphwdsb);
    }
}

SNDERR CWaveFile::mpeg_fade()
{
    if (m_first_mpeg_frame == 0)
        return (SNDERR)0;

    m_first_mpeg_frame->set_fade(m_fade_time);

    WaveBuffer *p = m_first_mpeg_frame;
    if (p->m_fp_volume != 0.0 && p->m_fp_fade_amount != 0.0) {
        p->m_flags.fading = 1;
    } else {
        p->m_flags.done = 1;
        p->m_flags.playing = 0;
        p->m_midi_note = 0;
    }
    return (SNDERR)0;
}

SNDERR CWaveFile::hwdsb_stop()
{
    if (m_phwdsb)
        m_phwdsb->stop();
    return SNDERR_OK;
}

SNDERR CWaveFile::mpeg_play()
{
#ifdef USE_MPEG
    SNDERR result = SNDERR_OK;

    m_milesMpeg.setLoopState(m_flags.looping);
    m_milesMpeg.play();
    if (!m_milesMpeg.decode_frames(10))
        return SNDERR_NOCANDO;
    if (m_nmpeg_frames == 0)
        m_nmpeg_frames = m_milesMpeg.get_frames(&m_first_mpeg_frame, &m_last_mpeg_frame);
    if (m_first_mpeg_frame) {
        m_first_mpeg_frame->m_fp_volume = m_fp_volume;
        m_first_mpeg_frame->m_fp_fade_amount = m_fp_fade_amount;
        m_first_mpeg_frame->m_fp_left_volume = m_fp_left_volume;
        m_first_mpeg_frame->m_fp_right_volume = m_fp_right_volume;
        m_first_mpeg_frame->m_left_volume = m_left_volume;
        m_first_mpeg_frame->m_right_volume = m_right_volume;
        m_first_mpeg_frame->m_pan = m_pan;
        m_first_mpeg_frame->m_fp_reset_fade_amount = m_fp_fade_amount;
        m_first_mpeg_frame->m_volume = m_volume;
    }
    if (m_phwdsb)
        m_phwdsb->play(1);
    return SNDERR_OK;
#else
    return SNDERR_NO_SUPPORT;
#endif
}

#ifdef USE_MPEG
static const WAVEFORMATEX mpeg_wfx = {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};
#endif

SNDERR CWaveFile::mpeg_load(const char *fname)
{
#ifdef USE_MPEG
    HMMIO hFile = 0;
    strcpy(m_fname, fname);
    SNDERR retVal = SNDERR_OK;
    if ((hFile = mmioOpenA(m_fname, 0, 0)) == 0) {
        mmioerr(m_mmioinfo.wErrorRet);
        memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
        return SNDERR_BAD_FILE_OPEN;
    }
    mmioClose(hFile, 0);

    if ((retVal = m_milesMpeg.init(m_fname)) != SNDERR_OK)
        return retVal;

    m_wfx = mpeg_wfx;
    if (!m_flags.shared_dsb) {
        CDSMgr *pds = gbl_pwave_device->get_pdsmgr();
        if (pds) {
            if (pds && pds->create_dsb(&m_phwdsb, &m_wfx, 0x100e0, 0x10000) == 0) {
                m_phwdsb->set_write_margin(gbl_pwave_device->get_rate(), 8);
                m_flags.hwbuffer = 1;
                m_flags.hwstreaming = 1;
            } else
                m_flags.hwstreaming = 0;
        } else
            m_flags.hwstreaming = 0;
    }
    m_flags.mpeg = 1;
    m_flags.loaded = 1;
    return SNDERR_OK;
#else
    return SNDERR_NO_SUPPORT;
#endif
}

SNDERR CWaveFile::load(const char *fname)
{
    SNDERR retval;
    unsigned int mmresult;

    if (strstr(fname, "mp3") == 0 && strstr(fname, "MP3") == 0) {
        MMCKINFO ckparent;
        MMCKINFO ckdata;
        MMCKINFO ckfmt;
        MMCKINFO ckcue;
        MMCKINFO cksample;

        memset(&ckparent, 0, sizeof(ckparent));
        memset(&ckfmt, 0, sizeof(ckfmt));
        memset(&ckdata, 0, sizeof(ckdata));
        memset(&ckcue, 0, sizeof(ckcue));

        if (m_flags.loaded) {
            retval = unload();
            if (retval != 0)
                return retval;
        }

        m_hfile = 0;
        memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
        memset(&m_sample_chunk, 0, sizeof(m_sample_chunk));
        m_curstartpos = 0;
        m_curendpos = 0;
        m_curpos = 0;
        m_curfpos = 0;
        m_dataoffset = 0;
        m_mmioinfo.cchBuffer = 0x20000;
        m_mmioinfo.fccIOProc = FOURCC_MEM;
        strcpy(m_fname, fname);
        m_hfile = mmioOpenA(m_fname, 0, MMIO_READ);
        if (m_hfile == 0) {
            mmioerr(m_mmioinfo.wErrorRet);
            memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
            return SNDERR_BAD_FILE_OPEN;
        }

        gbl_mmio++;
        m_flags.loaded = 1;
        gbl_created_list.add_to_end(&m_mmioinfo);

        ckparent.ckid = mmioStringToFOURCCA("RIFF", 0);
        ckparent.fccType = mmioStringToFOURCCA("WAVE", 0);
        if ((mmresult = mmioDescend(m_hfile, &ckparent, 0, MMIO_FINDRIFF)) != 0) {
            undo_load();
            return SNDERR_BAD_FORMAT;
        }

        ckfmt.ckid = mmioStringToFOURCCA("fmt ", 0);
        if ((mmresult = mmioDescend(m_hfile, &ckfmt, &ckparent, MMIO_FINDCHUNK)) != 0) {
            undo_load();
            return SNDERR_BAD_FORMAT;
        }
        mmioRead(m_hfile, (char *)&m_wfx, 16);
        m_wfx.cbSize = 0;
        mmioAscend(m_hfile, &ckfmt, 0);

        cksample.ckid = mmioStringToFOURCCA("smpl", 0);
        if ((mmresult = mmioDescend(m_hfile, &cksample, &ckparent, MMIO_FINDCHUNK)) == 0) {
            mmioRead(m_hfile, (char *)&m_sample_chunk.sd, sizeof(m_sample_chunk.sd));
            if (m_sample_chunk.sd.cSampleLoops != 0) {
                m_sample_chunk.psl =
                    (SampleLoop_tag *)operator new(m_sample_chunk.sd.cSampleLoops * sizeof(SampleLoop_tag));
                mmioRead(m_hfile, (char *)m_sample_chunk.psl, m_sample_chunk.sd.cSampleLoops * sizeof(SampleLoop_tag));
                m_loop_size = stob(m_sample_chunk.psl->dwEnd - m_sample_chunk.psl->dwStart + 1);
                m_loop_startpos = stob(m_sample_chunk.psl->dwStart);
            }
            mmioAscend(m_hfile, &cksample, 0);
        } else {
            m_sample_chunk.sd.dwMIDIUnityNote = 0x3c;
        }

        ckparent.ckid = mmioStringToFOURCCA("RIFF", 0);
        mmioSeek(m_hfile, 0, 0);
        ckparent.fccType = mmioStringToFOURCCA("WAVE", 0);
        if ((mmresult = mmioDescend(m_hfile, &ckparent, 0, MMIO_FINDRIFF)) != 0) {
            undo_load();
            return SNDERR_BAD_FORMAT;
        }

        ckcue.ckid = mmioStringToFOURCCA("cue ", 0);
        if ((mmresult = mmioDescend(m_hfile, &ckcue, &ckparent, MMIO_FINDCHUNK)) == 0) {
            unsigned long ncuepoints;
            ncuepoints = 0;
            mmioRead(m_hfile, (char *)&ncuepoints, 4);
            for (unsigned long i = 0; i < ncuepoints; i++) {
                CuePoint_tag *pcue_point = (CuePoint_tag *)operator new(sizeof(CuePoint_tag));
                mmioRead(m_hfile, (char *)pcue_point, sizeof(CuePoint_tag));
                m_cue_list.add_to_end(pcue_point);
                pcue_point = 0;
            }
            mmioAscend(m_hfile, &ckcue, 0);
        }

        mmioAscend(m_hfile, &ckparent, 0);
        ckparent.ckid = mmioStringToFOURCCA("RIFF", 0);
        mmioSeek(m_hfile, 0, 0);
        ckparent.fccType = mmioStringToFOURCCA("WAVE", 0);
        if ((mmresult = mmioDescend(m_hfile, &ckparent, 0, MMIO_FINDRIFF)) != 0) {
            undo_load();
            return SNDERR_BAD_FORMAT;
        }

        ckdata.ckid = mmioStringToFOURCCA("data", 0);
        if ((mmresult = mmioDescend(m_hfile, &ckdata, &ckparent, MMIO_FINDCHUNK)) != 0) {
            undo_load();
            return SNDERR_BAD_FORMAT;
        }

        m_dataoffset = ckdata.dwDataOffset;
        m_data_size = ckdata.cksize;
        m_curendpos = ckdata.cksize / m_wfx.nBlockAlign;
        m_ms_length = m_curendpos * 1000 / m_wfx.nSamplesPerSec;
        if (m_sample_chunk.psl == 0) {
            m_loop_size = ckdata.cksize;
            m_loop_startpos = 0;
        }

        if (m_flags.try_hw) {
            SNDERR retval;
            if (gbl_pwave_device != 0)
                retval = hw_load(gbl_pwave_device->get_pdsmgr());
            if (retval == SNDERR_OK || retval == SNDERR_MMIO_FAILURE) {
                mmioClose(m_hfile, 0);
                gbl_created_list.remove(&m_mmioinfo);
                gbl_mmio--;
                m_hfile = 0;
                if (retval == SNDERR_OK)
                    m_flags.all_in_buffer = 1;
                return retval;
            }
        }

        if (m_flags.buffer3d) {
            SNDERR retval;
            if (gbl_pwave_device != 0)
                retval = load_3d(gbl_pwave_device->get_pdsmgr());
            if (retval == SNDERR_OK || retval == SNDERR_MMIO_FAILURE) {
                mmioClose(m_hfile, 0);
                gbl_created_list.remove(&m_mmioinfo);
                gbl_mmio--;
                m_hfile = 0;
                return retval;
            }
        }

        long buff_size = 0x20000;
        if (m_flags.all_in_buffer || m_data_size < 0x20000) {
            buff_size = m_data_size;
            m_flags.all_in_buffer = 1;
        }
        if (!m_flags.all_in_buffer)
            m_buffer_mask = 0x1ffff;

        mmioSetBuffer(m_hfile, 0, buff_size, 0);
        mmioGetInfo(m_hfile, &m_mmioinfo, 0);
        mmioAdvance(m_hfile, &m_mmioinfo, 0);

        if ((m_curendpos - m_curstartpos) * m_wfx.nBlockAlign <= m_mmioinfo.cchBuffer) {
            mmioAdvance(m_hfile, &m_mmioinfo, 0);
            m_flags.all_in_buffer = 1;
            m_pmmioinfo = &m_mmioinfo;
            m_mmioinfo.pchEndRead = m_mmioinfo.pchBuffer + stob(m_curendpos - m_curstartpos);
            for (int i = 0; i < 16; i++) {
                instance[i].init(m_pmmioinfo, &m_wfx);
                instance[i].set_end_position(m_curendpos);
                instance[i].set_start_position(m_curstartpos);
                instance[i].set_current_position(0);
                instance[i].set_done(true);
                set_buffer_done(i);
                if (is_asr()) {
                    instance[i].set_ploop_start(m_pmmioinfo->pchBuffer + stob(m_sample_chunk.psl->dwStart));
                    instance[i].set_ploop_end(m_pmmioinfo->pchBuffer + stob(m_sample_chunk.psl->dwEnd + 1));
                    instance[i].set_loop_size(stob(m_sample_chunk.psl->dwEnd - m_sample_chunk.psl->dwStart + 1));
                } else {
                    instance[i].set_ploop_start(m_pmmioinfo->pchBuffer);
                    instance[i].set_ploop_end(m_pmmioinfo->pchEndRead);
                    instance[i].set_loop_size(m_pmmioinfo->pchEndRead - m_pmmioinfo->pchBuffer + 1);
                }
            }
        } else {
            m_pmmioinfo = (MMIOINFO *)operator new(sizeof(MMIOINFO));
            memset(m_pmmioinfo, 0, sizeof(MMIOINFO));
            m_pmmioinfo->cchBuffer = buff_size;
            m_pmmioinfo->fccIOProc = FOURCC_MEM;
            m_pmmioinfo->adwInfo[0] = 0;
            m_pmmioinfo->pchBuffer = (char *)malloc(buff_size);
            m_hbuffer = mmioOpenA(0, m_pmmioinfo, MMIO_CREATE | MMIO_READWRITE);
            if (m_hbuffer == 0) {
                mmioerr(m_pmmioinfo->wErrorRet);
                unload();
                return SNDERR_BAD_FILE_OPEN;
            }

            gbl_mmio++;
            mmioGetInfo(m_hbuffer, m_pmmioinfo, 0);
            gbl_created_list.add_to_end(m_pmmioinfo);
            m_pwrite = m_pmmioinfo->pchBuffer;
            if (is_asr()) {
                set_loop_state(1);
            }
            advance_stream();
            mmioGetInfo(m_hfile, &m_mmioinfo, 0);
            mmioAdvance(m_hfile, &m_mmioinfo, 0);

            {
                if (m_flags.hwstreaming && !m_flags.shared_dsb) {
                    CDSMgr *pds = gbl_pwave_device->get_pdsmgr();
                    if (pds == 0) {
                        m_flags.hwstreaming = 0;
                    } else if (pds->create_dsb(&m_phwdsb, &m_wfx, 0x100e0, 0x10000) == 0) {
                        m_phwdsb->set_write_margin(gbl_pwave_device->get_rate(), 8);
                        m_flags.hwbuffer = 1;
                    } else {
                        m_flags.hwstreaming = 0;
                    }
                }
            }
        }

        m_time = 0;
        return SNDERR_OK;
    }

    retval = mpeg_load(fname);
    return retval;
}

SNDERR CWaveFile::read_from_file(char *pdest, unsigned long *nbytes)
{
    SNDERR result = SNDERR_OK;
    unsigned int bytes_left_in_buffer = m_mmioinfo.pchEndRead - m_mmioinfo.pchNext;
    if (*nbytes > bytes_left_in_buffer) {
        *nbytes = bytes_left_in_buffer;
        result = SNDERR_END_OF_BUFFER;
    }
    memcpy(pdest, m_mmioinfo.pchNext, *nbytes);
    m_mmioinfo.pchNext += *nbytes;
    mmioSetInfo(m_hfile, &m_mmioinfo, 0);
    return result;
}

SNDERR CWaveFile::advance_stream()
{
    char *pendwrite;
    char *pd1;
    char *pd2;
    char *new_end_read = 0;
    unsigned long used;
    unsigned long left_in_file;
    unsigned long d1size;
    unsigned long d2size = 0;
    SNDERR read_result;

    if (m_flags.eod)
        return SNDERR_NO_DATA;

    pendwrite = m_pmmioinfo->pchNext - 4;
    if (pendwrite < m_pmmioinfo->pchBuffer)
        pendwrite += m_pmmioinfo->cchBuffer;

    used = (unsigned)(pendwrite - m_pwrite) & m_buffer_mask;
    if (used == 0)
        used = m_pmmioinfo->cchBuffer;

    if (m_flags.looping && !is_asr() && m_ppQueuedSound == 0)
        left_in_file = m_loop_startpos + m_loop_size - m_curfpos;
    else if (m_flags.looping && is_asr() && !m_flags.release)
        left_in_file = m_loop_startpos + m_loop_size - m_curfpos;
    else
        left_in_file = m_data_size - m_curfpos;

    int enough_left_in_file = left_in_file >= used;
    if (enough_left_in_file) {
        get_write_pointers(&pd1, &d1size, &pd2, &d2size, used);
        read_result = read_from_file(pd1, &d1size);
        if (pd2 != 0 && read_result == 0) {
            read_result = read_from_file(pd2, &d2size);
            new_end_read = m_pmmioinfo->pchBuffer + d2size;
            m_curfpos += d1size + d2size;
        } else {
            new_end_read = m_pwrite + d1size;
            m_curfpos += d1size;
        }
        m_pwrite = new_end_read;
        m_pmmioinfo->pchEndRead = m_pmmioinfo->pchEndWrite;
        if (read_result != 0)
            mmioAdvance(m_hfile, &m_mmioinfo, 0);
        m_flags.advance_pending = 0;
        return SNDERR_OK;
    }

    if (m_flags.looping && !m_flags.release && m_ppQueuedSound == 0) {
        get_write_pointers(&pd1, &d1size, &pd2, &d2size, left_in_file);
        read_result = read_from_file(pd1, &d1size);
        if (pd2 != 0 && read_result == 0) {
            read_result = read_from_file(pd2, &d2size);
            new_end_read = m_pmmioinfo->pchBuffer + d2size;
            m_curfpos += d1size + d2size;
        } else {
            new_end_read = m_pwrite + d1size;
            m_curfpos += d1size;
        }
        m_pwrite = new_end_read;
        if (read_result == 0) {
            if (d1size + d2size == left_in_file) {
                mmioSeek(m_hfile, -(long)m_loop_size, 1);
                mmioGetInfo(m_hfile, &m_mmioinfo, 0);
                mmioAdvance(m_hfile, &m_mmioinfo, 0);
                m_curfpos = m_loop_startpos;
                m_flags.advance_pending = 0;
                return SNDERR_OK;
            }
        } else if (read_result == 0x2b)
            mmioAdvance(m_hfile, &m_mmioinfo, 0);
        m_flags.advance_pending = 0;
        return SNDERR_OK;
    }

    unsigned int bytes_returned = 0;
    unsigned char all_out = 0;
    get_write_pointers(&pd1, &d1size, &pd2, &d2size, left_in_file);
    read_result = read_from_file(pd1, &d1size);
    if (pd2 != 0 && read_result == 0) {
        read_result = read_from_file(pd2, &d2size);
        new_end_read = m_pmmioinfo->pchBuffer + d2size;
        m_curfpos += d1size + d2size;
    } else {
        new_end_read = m_pwrite + d1size;
        m_curfpos += d1size;
    }
    m_pwrite = new_end_read;
    if (read_result == 0x2b)
        mmioAdvance(m_hfile, &m_mmioinfo, 0);
    m_flags.advance_pending = 0;
    m_pmmioinfo->pchEndRead = new_end_read;
    if (m_curfpos - m_data_size == 0)
        m_flags.eod = 1;
    return SNDERR_OK;
}

void CWaveFile::get_write_pointers(char **pp1, unsigned long *size1, char **pp2, unsigned long *size2,
                                   unsigned long size)
{
    if (m_pwrite + size <= m_pmmioinfo->pchEndWrite) {
        *size1 = size;
        *pp1 = m_pwrite;
        *size2 = 0;
        *pp2 = 0;
    } else {
        *size1 = m_pmmioinfo->pchEndWrite - m_pwrite;
        *size2 = size - *size1;
        *pp1 = m_pwrite;
        *pp2 = m_pmmioinfo->pchBuffer;
    }
}

SNDERR CWaveFile::reload()
{
    m_flags.loaded = 0;
    return load(m_fname);
}

SNDERR CWaveFile::reset()
{
    unsigned long byte_startpos;

    if (m_hfile == 0 && !m_flags.mpeg)
        return SNDERR_NOT_LOADED;
#ifdef USE_MPEG
    if (m_flags.mpeg) {
        m_milesMpeg.reset();
        clear_mpeg_frames();
    }
#endif
    m_flags.reset_pending = 0;
    m_flags.eod = 0;
    m_curpos = m_curstartpos;
    byte_startpos = stob(m_curstartpos) + m_dataoffset;

    if (m_flags.all_in_buffer) {
        m_mmioinfo.pchNext = m_mmioinfo.pchBuffer;
        m_mmioinfo.pchEndRead = m_mmioinfo.pchNext + m_data_size;
    } else if (!m_flags.mpeg) {
        mmioSeek(m_hfile, byte_startpos, 0);
        mmioGetInfo(m_hfile, &m_mmioinfo, 0);
        mmioAdvance(m_hfile, &m_mmioinfo, 0);
        m_pmmioinfo->pchNext = m_pmmioinfo->pchEndRead = m_pmmioinfo->pchBuffer;
        m_curfpos = 0;
        m_pwrite = m_pmmioinfo->pchBuffer;
        advance_stream();
        mmioAdvance(m_hfile, &m_mmioinfo, 0);
    }

    m_time = 0;
    m_flags.done = 0;
    m_flags.release = 0;
    m_flags.fading = 0;
    m_flags.last_fade = 0;
    m_flags.last_loop = 0;
    m_flags.buffer_bits = 0;
    m_volume = m_reset_volume;
    m_fp_volume = m_fp_reset_volume;
    set_fade(m_fade_time);
    set_pan(m_pan);

    if (m_flags.all_in_buffer) {
        for (int i = 0; i < 16; i++) {
            instance[i].reset();
            instance[i].set_release(0);
        }
    }
    return SNDERR_OK;
}

void CWaveFile::reset_flags()
{
    int big_buffer = m_flags.big_buffer;
    WFFlags_tag *st = &m_flags;
    int try_hw = st->try_hw;
    int looping = st->looping;
    int all_in_buffer = st->all_in_buffer;

    memset(st, 0, sizeof(*st));

    st->all_in_buffer = all_in_buffer;
    st->looping = looping;
    st->try_hw = try_hw;
    st->big_buffer = big_buffer;

    m_volume = m_reset_volume;
    m_fp_volume = m_fp_reset_volume;

    set_pan(m_pan);
}

CWaveFile::~CWaveFile()
{
    unsigned int result;
#ifdef USE_MPEG
    WaveBuffer *pframe;
#endif
    if (m_ppCurSound)
        m_ppCurSound = 0;
    if (m_flags.loaded && m_hfile) {
        if ((result = mmioClose(m_hfile, 0)) != 0)
            mmioerr(result);
        gbl_created_list.remove(&m_mmioinfo);
        gbl_mmio--;
        m_flags.loaded = 0;
    }
    if (m_pmmioinfo && m_pmmioinfo != &m_mmioinfo) {
        char *pbuffer = m_pmmioinfo->pchBuffer;
        mmioClose(m_pmmioinfo->hmmio, 0);
        delete m_pmmioinfo;
        free(pbuffer);
        m_pmmioinfo = 0;
    }
#ifdef USE_MPEG
    if (is_mpeg()) {
        m_milesMpeg.release();
        while ((pframe = m_first_mpeg_frame) != 0) {
            m_first_mpeg_frame = pframe->get_plinked_wb();
            char *pbuff = pframe->get_mmioinfo()->pchBuffer;
            mmioClose(pframe->get_mmioinfo()->hmmio, 0);
            gbl_created_list.remove(pframe->get_mmioinfo());
            gbl_mmio--;
            free(pbuff);
            delete pframe;
            m_nmpeg_frames--;
        }
    }
#endif
}

SNDERR CWaveFile::mpeg_unload()
{
#ifdef USE_MPEG
    WaveBuffer *pframe;
    CDSMgr *pds;
    if (m_phwdsb)
        m_phwdsb->stop();
    pds = gbl_pwave_device->get_pdsmgr();
    if (pds && m_phwdsb)
        pds->destroy_dsb(m_phwdsb);
    m_phwdsb = 0;
    while ((pframe = m_first_mpeg_frame) != 0) {
        m_first_mpeg_frame = pframe->get_plinked_wb();
        char *pbuff = pframe->get_mmioinfo()->pchBuffer;
        mmioClose(pframe->get_mmioinfo()->hmmio, 0);
        gbl_created_list.remove(pframe->get_mmioinfo());
        gbl_mmio--;
        free(pbuff);
        delete pframe;
        m_nmpeg_frames--;
    }
    m_first_mpeg_frame = 0;
    m_last_mpeg_frame = 0;
    m_flags.loaded = 0;
    m_flags.done = 0;
    m_nmpeg_frames = 0;
    m_milesMpeg.release();
    return SNDERR_OK;
#else
    return SNDERR_NO_SUPPORT;
#endif
}

SNDERR CWaveFile::clear_mpeg_frames()
{
    WaveBuffer *pframe = m_first_mpeg_frame;
    if (pframe != 0) {
        do {
            m_first_mpeg_frame = pframe->get_plinked_wb();
            char *pbuff = pframe->m_mmioinfo.pchBuffer;
            mmioClose(pframe->m_mmioinfo.hmmio, 0);
            gbl_created_list.remove(&pframe->m_mmioinfo);
            gbl_mmio--;
            free(pbuff);
            if (pframe != 0)
                ::operator delete(pframe);
            m_nmpeg_frames--;
            pframe = m_first_mpeg_frame;
        } while (pframe != 0);
    }
    return SNDERR_OK;
}

SNDERR CWaveFile::unload()
{
    SNDERR retval = SNDERR_OK;
    if (m_flags.loaded) {
        if (m_flags.mpeg)
            return mpeg_unload();
        if (m_phwdsb != 0) {
            CDSMgr *pds = gbl_pwave_device->get_pdsmgr();
            CDSB *pdsb;
            while ((pdsb = m_dup_hwdsb_list.remove_head()) != 0)
                pds->destroy_dsb(pdsb);
            pds->destroy_dsb(m_phwdsb);
            m_phwdsb = 0;
            m_flags.loaded = 0;
            return retval;
        }
        mmioClose(m_hfile, 0);
        gbl_created_list.remove(&m_mmioinfo);
        gbl_mmio--;
        memset(&m_mmioinfo, 0, sizeof(m_mmioinfo));
        if (m_flags.all_in_buffer == 0) {
            char *pbuffer = m_pmmioinfo->pchBuffer;
            mmioClose(m_hbuffer, 0);
            gbl_created_list.remove(&m_mmioinfo);
            gbl_mmio--;
            free(pbuffer);
            ::operator delete(m_pmmioinfo);
            m_pmmioinfo = 0;
        }
        if (m_sample_chunk.psl != 0) {
            ::operator delete(m_sample_chunk.psl);
            m_sample_chunk.psl = 0;
        }
        CuePoint_tag *pcue_point = m_cue_list.remove_head();
        while (pcue_point != 0) {
            ::operator delete(pcue_point);
            pcue_point = m_cue_list.remove_head();
        }
        reset_flags();
    }
    m_hfile = 0;
    m_hbuffer = 0;
    m_curendpos = 0;
    m_dataoffset = 0;
    m_curfpos = 0;
    for (int i = 0; i < 16; i++)
        memset(&instance[i], 0, sizeof(WaveBuffer));
    m_pmmioinfo = 0;
    return retval;
}

void CWaveFile::mmioerr(unsigned long err)
{
}

SNDERR CWaveFile::play()
{
    SNDERR retval = SNDERR_OK;
    CDSMgr *pdsmgr = gbl_pwave_device->get_pdsmgr();
    if (pdsmgr == 0)
        return SNDERR_NO_DEVICE;
    if (m_flags.mpeg)
        return mpeg_play();
    if (is_hwstreaming()) {
        if (m_phwdsb != 0)
            m_phwdsb->play(1);
    }
    if (m_flags.all_in_buffer) {
        unsigned long mask = 1;
        int i = 0;
        if (m_flags.hwbuffer) {
            CDSB *pplay_dsb = m_phwdsb;
            if (pplay_dsb->is_playing() || m_delay != 0) {
                Node<CDSB> *pnode = m_dup_hwdsb_list.get_head();
                while (pnode != 0) {
                    pplay_dsb = pnode->get_obj();
                    if (!pplay_dsb->is_playing())
                        break;
                    pnode = pnode->get_next();
                }
                if (pnode == 0) {
                    retval = pdsmgr->duplicate_dsb(this, m_phwdsb, &pplay_dsb);
                    if (retval == 0x25 || retval == 0x1d) {
                        if (pdsmgr->steal_duplicate_hwdsb(this, &pplay_dsb) != 0)
                            return SNDERR_NOCANDO;
                    } else if (retval != 0) {
                        return SNDERR_NOCANDO;
                    }
                }
                pplay_dsb->set_volume(m_phwdsb->get_volume());
            }
            pplay_dsb->set_frequency(m_pitch);
            if (m_flags.buffer3d)
                pplay_dsb->set_position3d(m_phwdsb->xpos(), m_phwdsb->ypos(), m_phwdsb->zpos());
            if (m_delay == 0)
                pplay_dsb->play(0);
            else
                pplay_dsb->set_delay(m_delay);
        } else {
            for (; i < 16; i++) {
                if ((mask & m_flags.buffer_bits) == 0)
                    break;
                mask <<= 1;
            }
            if (i == 16)
                return SNDERR_NOCANDO;
            instance[i].reset();
            instance[i].set_left_volume(m_fp_left_volume);
            instance[i].set_right_volume(m_fp_right_volume);
            instance[i].set_pitch(m_fp_pitch);
            instance[i].set_volume(m_fp_volume);
            instance[i].set_delay(get_delay());
            instance[i].set_time(0);
            instance[i].set_time_past(&m_time_past);
            instance[i].set_done(false);
            if (is_asr())
                instance[i].set_loop_state(1);
            m_flags.buffer_bits |= mask;
        }
    }
    m_flags.done = 0;
    return SNDERR_OK;
}

SNDERR CWaveFile::play(int *pinstance)
{
    *pinstance = -1;
    if (!m_flags.all_in_buffer || m_flags.hwbuffer)
        return SNDERR_WRONG_TYPE;

    unsigned mask = 1;
    int i;
    for (i = 0; i < 16; i++) {
        if (!(m_flags.buffer_bits & mask))
            break;
        mask <<= 1;
    }
    if (i == 16)
        return SNDERR_NOCANDO;

    instance[i].reset();
    instance[i].set_left_volume(m_fp_left_volume);
    instance[i].set_right_volume(m_fp_right_volume);
    instance[i].set_pitch(m_fp_pitch);
    instance[i].set_volume(m_fp_volume);
    instance[i].set_delay(get_delay());
    instance[i].set_time(0);
    instance[i].set_time_past(&m_time_past);
    instance[i].set_done(false);
    instance[i].m_flags.playing = 1;

    m_flags.buffer_bits |= mask;
    m_flags.done = 0;
    *pinstance = i;
    return SNDERR_OK;
}

SNDERR CWaveFile::play(int ibuffer)
{
    if (!m_flags.all_in_buffer)
        return SNDERR_NO_SUPPORT;
    if (ibuffer > 16)
        return SNDERR_BAD_PARAM;

    unsigned long mask = 1;
    mask <<= ibuffer;

    instance[ibuffer].reset();
    instance[ibuffer].set_pitch(m_fp_pitch);
    instance[ibuffer].set_delay(get_delay());
    instance[ibuffer].set_time(0);
    instance[ibuffer].set_time_past(&m_time_past);
    instance[ibuffer].set_done(false);
    instance[ibuffer].set_fade(m_fade_time);

    m_flags.buffer_bits |= mask;
    m_flags.done = 0;
    instance[ibuffer].m_flags.playing = 1;
    return SNDERR_OK;
}

SNDERR CWaveFile::fade()
{
    if (m_flags.active == 0 || m_flags.done != 0)
        return SNDERR_NOT_ACTIVE;
    if (m_flags.all_in_buffer == 0 || m_flags.hwbuffer != 0) {
        m_flags.fading = 1;
        return SNDERR_OK;
    }
    unsigned long mask = 1;
    int i;
    for (i = 0; i < 16; i++) {
        if (mask != 0 && m_flags.buffer_bits != 0) {
            instance[i].fade();
        }
        mask <<= 1;
    }
    return SNDERR_OK;
}

SNDERR CWaveFile::fade_in()
{
    if (m_flags.active && !m_flags.done) {
        if (m_flags.all_in_buffer) {
            unsigned long mask = 1;
            int i;
            for (i = 0; i < 16; i++) {
                if (mask != 0 && m_flags.buffer_bits != 0)
                    instance[i].fade_in();
                mask <<= 1;
            }
            return SNDERR_OK;
        }
        m_fp_reset_volume = m_fp_volume;
        m_fp_volume = 0;
        m_flags.fading = 1;
        m_flags.fade_in = 1;
        return SNDERR_OK;
    }
    return SNDERR_NOT_ACTIVE;
}

void CWaveFile::ramp(int dst_vol, int src_vol, unsigned int ms_time)
{
    if (src_vol == -1)
        src_vol = m_volume;
    if (dst_vol == -1)
        dst_vol = m_volume;
    m_fp_dst_volume = dst_vol * 1.52587890625e-05;
    double fp_src_volume = src_vol ? src_vol * 1.52587890625e-05 : 0.0;
    set_volume(fp_src_volume);
    if (gbl_ptimer != 0) {
        unsigned rate = gbl_ptimer->get_rate();
        if (ms_time > rate) {
            unsigned number_of_intervals = ms_time / rate;
            m_fade_amount = (unsigned)(src_vol - dst_vol) / number_of_intervals;
            m_fp_fade_amount = (fp_src_volume - m_fp_dst_volume) / (double)number_of_intervals;
        } else {
            m_fade_amount = src_vol - dst_vol;
            m_fp_fade_amount = (double)(src_vol - dst_vol);
        }
        m_flags.ramping = 1;
    }
}

SNDERR CWaveFile::fade(unsigned long ms)
{
    set_fade(ms);
    m_flags.fading = 1;
    return SNDERR_OK;
}

SNDERR CWaveFile::fade_in(unsigned long ms)
{
    set_fade_in(ms);
    m_fp_reset_volume = m_fp_volume;
    m_fp_volume = 0;
    m_flags.fading = 1;
    m_flags.fade_in = 1;
    return SNDERR_OK;
}

unsigned long CWaveFile::operator++(int)
{
    if (m_flags.loaded == 0)
        return 0;

    m_pmmioinfo->pchNext += m_wfx.nBlockAlign;
    if (m_pmmioinfo->pchNext >= m_pmmioinfo->pchEndRead) {
        m_pmmioinfo->pchNext = m_pmmioinfo->pchEndRead;
        return 0;
    }

    m_curpos++;
    if (m_flags.done != 0)
        return m_curpos;
    if (m_curpos > m_curendpos) {
        if (m_flags.looping != 0) {
            m_curpos = m_curstartpos;
            return m_curstartpos;
        }
        m_curpos = m_curendpos;
        m_flags.done = 1;
    }
    return m_curpos;
}

unsigned long CWaveFile::get_nsamples_left_in_buffer()
{
    if (m_flags.looping)
        return btos(m_pmmioinfo->pchEndRead - m_pmmioinfo->pchNext);

    unsigned long bytes_left_in_file = stob(m_curendpos - m_curpos);
    unsigned long bytes_left_in_buffer = m_pmmioinfo->pchEndRead - m_pmmioinfo->pchNext;

    if (bytes_left_in_file < bytes_left_in_buffer)
        return m_curendpos - m_curpos;
    return bytes_left_in_buffer / m_wfx.nBlockAlign;
}

SNDERR CWaveFile::advance()
{
    static int test_count;
    if (!m_flags.loaded)
        return SNDERR_NOT_LOADED;
    if (m_flags.all_in_buffer)
        return SNDERR_OK;
    if (m_flags.eod)
        return SNDERR_OK;
    advance_stream();
    return SNDERR_OK;
}

SNDERR CWaveFile::advance_file()
{
    if (m_flags.loaded && !m_flags.all_in_buffer && !m_flags.eod)
        advance_stream();
    return SNDERR_OK;
}

unsigned long CWaveFile::read_data(char *buff, unsigned long request)
{
    MMIOINFO finfo;
    mmioGetInfo(m_hfile, &finfo, 0);
    unsigned long in_fbuffer = finfo.pchEndRead - finfo.pchNext;
    if (in_fbuffer == 0)
        return 0;
    if (request > in_fbuffer)
        request = in_fbuffer;
    memmove(buff, finfo.pchNext, request);
    finfo.pchNext += request;
    mmioSetInfo(m_hfile, &finfo, 0);
    return request;
}

BOOL CWaveFile::need_to_advance()
{
    unsigned long used = 0;
    if (!m_flags.loaded)
        return FALSE;
    if (m_flags.all_in_buffer)
        return FALSE;
    if (m_flags.eod)
        return FALSE;
    if (m_flags.advance_pending)
        return FALSE;
    used = (m_pmmioinfo->pchNext - m_pwrite) & m_buffer_mask;
    if (used > (unsigned)m_pmmioinfo->cchBuffer >> 2) {
        m_flags.advance_pending = 1;
        return TRUE;
    }
    return FALSE;
}

SNDERR CWaveFile::seek_to_curstartpos()
{
    m_flags.loaded = 0;
    long offset = m_curstartpos * m_wfx.nBlockAlign + m_dataoffset;
    mmioSeek(m_hfile, offset, 0);
    mmioAdvance(m_hfile, &m_mmioinfo, 0);
    return SNDERR_OK;
}

void CWaveFile::set_hwvolume(int vol)
{
    int i;
    if (is_hwbuffer() && m_phwdsb != 0) {
        m_phwdsb->set_volume((unsigned long)vol);
        for (i = 0; i < 0x10; i++)
            if (instance[i].m_phwdsb != 0)
                instance[i].m_phwdsb->set_volume((unsigned long)vol);
    }
}

void CWaveFile::set_volume(int vol)
{
    int bad;
    if (vol < 0)
        vol = 0;
    else if (vol > 0x10000)
        vol = 0x10000;
    m_reset_volume = vol;
    m_fp_reset_volume = vol * (1.0 / 65536.0);
    if (m_fp_reset_volume == 0.0)
        bad = 1;
    if (!m_flags.fading) {
        m_fp_volume = m_fp_reset_volume;
        m_volume = vol;
    }
    set_pan(m_pan);
    if (!m_flags.hwbuffer && m_flags.all_in_buffer) {
        for (int i = 0; i < 16; i++) {
            if (!instance[i].is_fading()) {
                instance[i].set_volume(m_fp_volume);
                instance[i].set_left_volume(m_fp_left_volume);
                instance[i].set_right_volume(m_fp_right_volume);
                instance[i].set_fade(m_fade_time);
            }
        }
    }
}

void CWaveFile::set_pan(int pan)
{
    if (pan < -64)
        pan = -64;
    else if (pan > 63)
        pan = 63;

    int curvol = m_volume;
    m_pan = pan;
    int hit_zero;

    if (pan == 0) {
        m_right_volume = curvol;
        m_left_volume = curvol;
        m_fp_right_volume = m_fp_volume;
        if (m_fp_right_volume == 0.0)
            hit_zero = 1;
        m_fp_left_volume = m_fp_volume;
    } else {
        int scale;
        if (pan < 0)
            scale = 64;
        else
            scale = 63;
        int scaled_pan = curvol * pan / scale;
        int new_right = curvol + scaled_pan;
        int new_left = curvol - scaled_pan;
        if (new_right > 0x10000)
            new_right = 0x10000;
        if (new_left > 0x10000)
            new_left = 0x10000;
        m_right_volume = new_right;
        m_left_volume = new_left;
        m_fp_right_volume = new_right * (1.0 / 65536.0);
        m_fp_left_volume = new_left * (1.0 / 65536.0);
    }
}

void CWaveFile::set_fade(unsigned long ms)
{
    if (m_flags.all_in_buffer) {
        for (unsigned long i = 0; i < 16; i++) {
            instance[i].set_fade(ms);
        }
    }
    if (gbl_ptimer != 0) {
        unsigned long rate = gbl_ptimer->get_rate();
        if (ms > rate) {
            unsigned long number_of_decrements = ms / rate;
            m_fade_time = ms;
            m_fade_amount = m_reset_volume / number_of_decrements;
            m_fp_fade_amount = m_fp_volume / (double)number_of_decrements;
        } else {
            m_fade_amount = m_reset_volume;
            m_fp_fade_amount = m_fp_volume;
            m_fade_time = ms;
        }
    }
}

void CWaveFile::set_fade_in(unsigned long ms)
{
    if (m_flags.all_in_buffer) {
        for (unsigned long i = 0; i < 16; i++) {
            instance[i].set_fade_in(ms);
        }
    }

    if (gbl_ptimer != 0) {
        unsigned long rate = gbl_ptimer->get_rate();
        if (ms > rate) {
            int number_of_increments = ms / rate;
            m_fade_amount = -(m_reset_volume / number_of_increments);
            m_fp_fade_amount = -(m_fp_volume / (double)number_of_increments);
        } else {
            m_fade_amount = -m_reset_volume;
            m_fp_fade_amount = -m_fp_volume;
        }
        m_fade_time = ms;
    }
}

void CWaveFile::set_pitch(unsigned long pitch)
{
    m_pitch = pitch;
}

unsigned long CWaveFile::get_current_marker()
{
    unsigned long return_val = -1;
    CuePoint_tag *pcue_point = m_cue_list.get_first();
    while (pcue_point != 0 && m_curpos >= pcue_point->dwPosition) {
        return_val = pcue_point->dwName;
        pcue_point = m_cue_list.get_next();
    }
    return return_val;
}

SNDERR CWaveFile::get_cursample(unsigned long *pdst)
{
    unsigned char *pdata;
    unsigned long nchannels;
    unsigned long wBitsPerSample;

    if (m_flags.loaded == 0)
        return SNDERR_NOT_LOADED;

    nchannels = m_wfx.nChannels;
    wBitsPerSample = m_wfx.wBitsPerSample;
    pdata = (unsigned char *)m_pmmioinfo->pchNext;

    __asm {
        push    ebx
        push    ecx
        xor     eax, eax
        mov     ebx, nchannels
        movzx   ecx, byte ptr wBitsPerSample
        cmp     ecx, 8
        je      do8bit
        cmp     ecx, 1
        je      do16bitmono
        mov     eax, pdata
        jmp     gcsdone
    do16bitmono:
        movzx   eax, word ptr pdata
        shl     eax, 16
        mov     ax, word ptr pdata
        jmp     gcsdone
    do8bit:
        cmp     ecx, 1
        je      do8bitmono
        movzx   eax, byte ptr pdata[1]
        shl     eax, 8
        sub     eax, 8000h
        shl     eax, 16
        movzx   ax, byte ptr pdata
        shl     ax, 8
        sub     ax, 8000h
        jmp     gcsdone
    do8bitmono:
        movzx   eax, byte ptr pdata
        shl     eax, 8
        sub     eax, 8000h
        mov     ebx, eax
        shl     eax, 16
        mov     ax, bx
    gcsdone:
        mov     pdst, eax
        pop     ecx
        pop     ebx
    }

    return SNDERR_OK;
}

unsigned long CWaveFile::get_cursample_channel(unsigned long channel)
{
    unsigned char *pdata;
    unsigned long retval = 0;
    unsigned long nchannels;
    unsigned long wBitsPerSample;

    if (m_flags.loaded == 0)
        return SNDERR_NOT_LOADED;

    nchannels = m_wfx.nChannels;
    wBitsPerSample = m_wfx.wBitsPerSample;
    pdata = (unsigned char *)m_pmmioinfo->pchNext;

    __asm {
        push    ebx
        mov     ebx, pdata
        cmp     wBitsPerSample, 8
        je      gcc_do8bit
        cmp     nchannels, 1
        je      gcc_do16bitleft
        cmp     channel, 1
        je      gcc_do16bitleft
        movzx   eax, word ptr [ebx+2]
        jmp     gcc_done
    gcc_do16bitleft:
        movzx   eax, word ptr [ebx]
        jmp     gcc_done
    gcc_do8bit:
        cmp     nchannels, 1
        je      gcc_do8bitleft
        cmp     channel, 1
        je      gcc_do8bitleft
        movzx   eax, byte ptr [ebx+1]
        shl     eax, 8
        sub     eax, 8000h
        jmp     gcc_done
    gcc_do8bitleft:
        movzx   eax, byte ptr [ebx]
        shl     eax, 4
    gcc_done:
        mov     retval, eax
        pop     ebx
    }

    return retval;
}

SNDERR CWaveFile::get_nsamples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                               unsigned long *ret_samples_read)
{
    int i = 0;
    int active_instances = 0;
    unsigned long mask = 1;
    int do_a_move = 0;
    unsigned long most_read = 0;
    unsigned long debug_time = 0;
    unsigned char wb_done;
    int big_fuck_up;

    if (filter_index > 0x18)
        return SNDERR_BAD_PARAM;

    if (m_flags.fading || m_flags.ramping) {
        if (m_fade_amount < 0) {
            m_volume -= m_fade_amount;
            if (m_volume > 0x10000)
                m_volume = 0x10000;
        } else if (m_fade_amount < (int)m_volume) {
            m_volume -= m_fade_amount;
        } else {
            m_volume = 0;
        }
    }

    i = 0;
    if (!m_flags.all_in_buffer && !m_flags.mpeg && !m_flags.hwstreaming) {
        get_stream_samples(pdst, nsamples, filter_index, ret_samples_read);
    } else if (m_flags.hwstreaming) {
        stream_to_hwbuffer(nsamples, filter_index, ret_samples_read);
    } else if (m_flags.buffer_bits) {
        if (filter_index >= 0xc)
            do_a_move = 1;
        while (i < 16) {
            wb_done = 0;
            if (m_flags.buffer_bits & mask) {
                active_instances++;
                debug_time = timeGetTime();
                get_buffer_samples(&instance[i], pdst, nsamples, filter_index, ret_samples_read);
                if (*ret_samples_read != nsamples || instance[i].is_done()) {
                    m_flags.buffer_bits &= ~mask;
                    active_instances--;
                }
                if (!instance[i].m_flags.playing && (m_flags.buffer_bits & mask))
                    big_fuck_up = 1;
                if (*ret_samples_read > most_read)
                    most_read = *ret_samples_read;
                if (do_a_move) {
                    do_a_move = 0;
                    filter_index -= 0xc;
                }
            }
            mask <<= 1;
            i++;
        }
        *ret_samples_read = most_read;
        if (!m_flags.buffer_bits)
            m_flags.done = 1;
    } else if (m_flags.mpeg) {
        get_mpeg_samples(pdst, nsamples, filter_index, ret_samples_read);
    }
    return SNDERR_OK;
}

SNDERR CWaveFile::get_mpeg_samples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                                   unsigned long *ret_samples_read)
{
    unsigned long total_samples_processed = 0;
    bool moving = false;
    if (filter_index >= 12)
        moving = true;
    if (!is_hwstreaming())
        return SNDERR_NO_SUPPORT;
    SNDERR ret = get_mpeg_samples_to_dsb(nsamples, filter_index, ret_samples_read);
    if (m_flags.hwbuffer && moving)
        memset(pdst, 0, nsamples * 16);
    return ret;
}

SNDERR CWaveFile::setNextMpegFrame()
{
    WaveBuffer *next_frame = m_first_mpeg_frame->get_plinked_wb();
    char *pbuff = m_first_mpeg_frame->get_mmioinfo()->pchBuffer;

    mmioClose(m_first_mpeg_frame->get_mmioinfo()->hmmio, 0);
    free(pbuff);
    gbl_mmio--;
    gbl_created_list.remove(&m_first_mpeg_frame->m_mmioinfo);

    if (next_frame != 0) {
        if (is_fading()) {
            next_frame->m_flags.last_fade = m_first_mpeg_frame->m_flags.last_fade;
            next_frame->m_fp_reset_fade_amount = next_frame->m_fp_fade_amount = m_first_mpeg_frame->m_fp_fade_amount;
            next_frame->m_fp_reset_volume = next_frame->m_fp_volume = m_first_mpeg_frame->m_fp_volume;
            next_frame->set_pan(m_pan);
            if (next_frame->m_fp_volume != 0.0 && next_frame->m_fp_fade_amount != 0.0) {
                next_frame->m_flags.fading = 1;
            } else {
                next_frame->m_flags.done = 1;
                next_frame->m_flags.playing = 0;
                next_frame->m_midi_note = 0;
            }
        }
        if (is_fading() == 0) {
            next_frame->m_fp_volume = m_fp_volume;
            next_frame->m_fp_fade_amount = m_fp_fade_amount;
            next_frame->m_fp_left_volume = m_fp_left_volume;
            next_frame->m_fp_right_volume = m_fp_right_volume;
            next_frame->m_left_volume = m_left_volume;
            next_frame->m_right_volume = m_right_volume;
            next_frame->m_pan = m_pan;
            next_frame->m_fp_reset_fade_amount = m_fp_fade_amount;
            next_frame->m_volume = m_volume;
        }
    }

    if (m_first_mpeg_frame != 0)
        ::operator delete(m_first_mpeg_frame);
    m_first_mpeg_frame = next_frame;
    m_nmpeg_frames--;
    return SNDERR_OK;
}

SNDERR CWaveFile::get_mpeg_samples_to_dsb(unsigned long time_past, unsigned long filter_index,
                                          unsigned long *ret_samples_read)
{
#ifdef USE_MPEG
    unsigned long total_samples_processed = 0, position = 0, dsb_size1 = 0, dsb_size2 = 0;
    SNDERR retval = SNDERR_OK;
    unsigned char moving = 1;
    char *psrc_temp;
    void *pdsb1 = 0, *pdsb2 = 0;
    unsigned char bPassToQueuedSound = 0;

    if (is_fading() || m_flags.ramping) {
        if (m_flags.last_fade) {
            m_flags.done = 1;
            if (m_ppCurSound)
                *m_ppCurSound = 0;
        } else if (m_flags.fade_in) {
            if (m_fp_volume >= m_fp_reset_volume) {
                m_flags.fading = 0;
                m_flags.fade_in = 0;
            }
        } else if (m_flags.ramping) {
            if (m_fp_fade_amount > 0.0) {
                if (m_fp_volume > m_fp_dst_volume) {
                    if (m_fp_volume < m_fp_fade_amount)
                        m_fp_volume = m_fp_dst_volume;
                    else
                        m_fp_volume -= m_fp_fade_amount;
                    m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
                } else
                    m_flags.ramping = 0;
            } else {
                if (m_fp_volume < m_fp_dst_volume) {
                    if (m_fp_dst_volume - m_fp_volume > fabs(m_fp_fade_amount))
                        m_fp_volume -= m_fp_fade_amount;
                    else
                        m_fp_volume = m_fp_dst_volume;
                    m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
                } else
                    m_flags.ramping = 0;
            }
        } else {
            if (m_fp_volume == 0.0) {
                m_flags.done = 1;
                if (m_ppCurSound)
                    *m_ppCurSound = 0;
            }
            if (m_fp_volume <= m_fp_fade_amount) {
                m_flags.last_fade = 1;
                m_fp_fade_amount = m_fp_volume;
            }
            m_fp_volume -= m_fp_fade_amount;
            m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
        }
    }

    if (m_first_mpeg_frame && !m_flags.done) {
        unsigned long samples_read = 0;
        unsigned long total_nbytes_to_write = 0;
        unsigned long nsamples = 0;
        if ((retval = m_phwdsb->get_write_info(&total_nbytes_to_write, &position, time_past)) != SNDERR_OK)
            return retval;
        nsamples = m_phwdsb->bytes_to_samples(total_nbytes_to_write);

        while (nsamples) {
            unsigned int read_index = 0;
            int nbytes_to_write;
            assert(m_first_mpeg_frame);

            if (m_first_mpeg_frame->get_pnext() >= m_first_mpeg_frame->get_pend())
                setNextMpegFrame();

            if (nsamples > m_first_mpeg_frame->get_nsamples_left_in_buffer())
                samples_read = m_first_mpeg_frame->get_nsamples_left_in_buffer();
            else
                samples_read = nsamples;

            psrc_temp = m_first_mpeg_frame->get_pnext();
            nbytes_to_write = stob(samples_read);
            m_phwdsb->lock(position, nbytes_to_write, &pdsb1, &dsb_size1, &pdsb2, &dsb_size2);
            position += dsb_size1;
            memcpy(pdsb1, psrc_temp, dsb_size1);
            if (pdsb2) {
                memcpy(pdsb2, psrc_temp + dsb_size1, dsb_size2);
                position = dsb_size2;
            }
            m_phwdsb->unlock(pdsb1, dsb_size1, pdsb2, dsb_size2);

            nsamples -= samples_read;
            total_samples_processed += samples_read;
            m_first_mpeg_frame->set_pnext(psrc_temp + nbytes_to_write);

            if (nsamples) {
                setNextMpegFrame();
                if (m_first_mpeg_frame == 0)
                    m_last_mpeg_frame = 0;
            }

            if (m_nmpeg_frames == 0) {
                if (nsamples) {
                    nbytes_to_write = stob(nsamples);
                    m_phwdsb->lock(position, nbytes_to_write, &pdsb1, &dsb_size1, &pdsb2, &dsb_size2);
                    position += dsb_size1;

                    if (!m_milesMpeg.isEndOfStream()) {
                        WaveBuffer *first_frame = 0, *last_frame = 0;
                        m_nmpeg_frames += m_milesMpeg.get_frames(&first_frame, &last_frame);

                        if (last_frame) {
                            if (m_last_mpeg_frame)
                                m_last_mpeg_frame->set_plinked_wb(first_frame);
                            m_last_mpeg_frame = last_frame;
                            if (m_first_mpeg_frame == 0)
                                m_first_mpeg_frame = first_frame;
                        } else {
                            memset(pdsb1, 0, dsb_size1);
                            if (pdsb2) {
                                memset(pdsb2, 0, dsb_size2);
                                position = dsb_size2;
                            }
                            *ret_samples_read = nsamples;
                            nsamples = 0;
                        }
                    } else if (m_milesMpeg.isEndOfStream()) {
                        WaveBuffer *first_frame = 0, *last_frame = 0;
                        m_nmpeg_frames += m_milesMpeg.get_frames(&first_frame, &last_frame);
                        if (last_frame) {
                            if (m_last_mpeg_frame)
                                m_last_mpeg_frame->set_plinked_wb(first_frame);
                            m_last_mpeg_frame = last_frame;
                            if (m_first_mpeg_frame == 0)
                                m_first_mpeg_frame = first_frame;
                        } else if (m_ppQueuedSound) {
                            if ((*m_ppQueuedSound)->is_mpeg())
                                psrc_temp = ((CWaveFile *)(*m_ppQueuedSound))->m_first_mpeg_frame->get_pnext();
                            else
                                psrc_temp = (*m_ppQueuedSound)->get_pnext();

                            memcpy(pdsb1, psrc_temp, dsb_size1);
                            psrc_temp += dsb_size1;
                            if (pdsb2) {
                                memcpy(pdsb2, psrc_temp, dsb_size2);
                                psrc_temp += dsb_size2;
                            }
                            if ((*m_ppQueuedSound)->is_mpeg()) {
                                assert(psrc_temp < ((CWaveFile *)(*m_ppQueuedSound))->m_first_mpeg_frame->get_pend());
                                ((CWaveFile *)(*m_ppQueuedSound))->m_first_mpeg_frame->set_pnext(psrc_temp);
                            } else
                                ((CWaveFile *)(*m_ppQueuedSound))->m_pmmioinfo->pchNext += dsb_size1 + dsb_size2;
                            nsamples = 0;
                            bPassToQueuedSound = 1;
                        } else {
                            memset(pdsb1, 0, dsb_size1);
                            if (pdsb2) {
                                memset(pdsb2, 0, dsb_size2);
                                position = dsb_size2;
                            }
                            *ret_samples_read = nsamples;
                            nsamples = 0;
                        }
                    }
                    m_phwdsb->unlock(pdsb1, dsb_size1, pdsb2, dsb_size2);
                }
            }
        }
    }

    if (bPassToQueuedSound) {
        gbl_pwave_device->add_to_play_list(*m_ppQueuedSound);
        ((CWaveFile *)(*m_ppQueuedSound))->m_phwdsb = m_phwdsb;
        ((CWaveFile *)(*m_ppQueuedSound))->m_flags.hwbuffer = 1;
        m_phwdsb = 0;
        *m_ppCurSound = *m_ppQueuedSound;
        *m_ppQueuedSound = 0;
        set_done();
        m_ppQueuedSound = 0;
        m_ppCurSound = 0;
        return SNDERR_OK;
    }

    if (m_nmpeg_frames < 10 && !m_flags.done) {
        WaveBuffer *first_frame = 0, *last_frame = 0;
        m_nmpeg_frames += m_milesMpeg.get_frames(&first_frame, &last_frame);
        if (last_frame) {
            if (m_last_mpeg_frame)
                m_last_mpeg_frame->set_plinked_wb(first_frame);
            m_last_mpeg_frame = last_frame;
            if (m_first_mpeg_frame == 0)
                m_first_mpeg_frame = first_frame;
        }

        if (!m_milesMpeg.isEndOfStream()) {
            if (gbl_pwave_device && gbl_pwave_device->is_debug())
                PostMessageA(m_hwnd, MPEG_ID_DECODE, 6, (long)&m_milesMpeg);
            else
                gbl_sound_thread.post_message(MPEG_ID_DECODE, 6, (long)&m_milesMpeg);
        }
    }

    if ((m_nmpeg_frames == 0 && m_milesMpeg.isEndOfStream()) || m_flags.done) {
        m_flags.done = 1;
        m_phwdsb->stop();
    }
    *ret_samples_read = total_samples_processed;
    return SNDERR_OK;
#else
    return SNDERR_NO_SUPPORT;
#endif
}

int CWaveFile::UpdateCompletionState()
{
    CDSB *s = m_phwdsb;
    if (s != 0 && s->is_playing() == 0) {
        m_flags.done = 1;
    } else if (m_flags.fading) {
        return UpdateFade();
    }
    return 0;
}

int CWaveFile::UpdateFade()
{
    if (!m_flags.fading && !m_flags.ramping)
        return 0;

    if (m_flags.last_fade) {
        m_flags.done = 1;
        m_phwdsb->stop();
        if (m_ppCurSound == 0)
            return 0;
        m_flags.shared_dsb = 1;
        m_flags.release_cur_sound = 1;
        return 0;
    }

    if (m_flags.fade_in) {
        if (m_fp_volume >= m_fp_reset_volume) {
            m_flags.fading = 0;
            m_flags.fade_in = 0;
        }
        return 0;
    }

    if (m_flags.ramping) {
        if (m_fp_fade_amount > 0.0) {
            if (m_fp_volume > m_fp_dst_volume) {
                if (m_fp_volume < m_fp_fade_amount)
                    m_fp_volume = m_fp_dst_volume;
                else
                    m_fp_volume -= m_fp_fade_amount;
                m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
                return 0;
            }
        } else {
            if (m_fp_volume < m_fp_dst_volume) {
                if (m_fp_dst_volume - m_fp_volume > fabs(m_fp_fade_amount))
                    m_fp_volume -= m_fp_fade_amount;
                else
                    m_fp_volume = m_fp_dst_volume;
                m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
                return 0;
            }
        }
        m_fp_volume = m_fp_dst_volume;
        m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
        m_flags.ramping = 0;
        return 0;
    }

    if (m_fp_volume == 0.0) {
        m_flags.done = 1;
        m_phwdsb->stop();
        if (m_ppCurSound == 0)
            return 0;
        m_flags.shared_dsb = 1;
        m_flags.release_cur_sound = 1;
        return 0;
    }
    if (m_fp_volume <= m_fp_fade_amount) {
        m_flags.last_fade = 1;
        m_fp_fade_amount = m_fp_volume;
    }
    m_fp_volume -= m_fp_fade_amount;
    m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
    return 0;
}

SNDERR CWaveFile::get_stream_samples(void *pdst, unsigned long nsamples, unsigned long filter_index,
                                     unsigned long *ret_samples_read)
{
    if (nsamples > 0x7d0)
        return SNDERR_BUFFER_OVERRUN;

    unsigned long samples_read = 0;
    unsigned long samples_to_read = 0;
    int moving = 0;
    CWaveMixer *pmixer = gbl_pwave_device->get_mixer();
    PFNFILTER pfilter;
    if (pmixer && pmixer->uses_mmx())
        pfilter = mmx_filter_table[filter_index];
    else
        pfilter = filter_table[filter_index];
    if (filter_index >= 0xc)
        moving = 1;

    m_time += m_time_past;
    if (m_delay != 0 && m_delay > m_time) {
        *ret_samples_read = nsamples;
        if (moving)
            memset(pdst, 0, nsamples * 16);
        return SNDERR_OK;
    }

    char *pdst_temp = (char *)pdst;
    char *psrc_temp = m_pmmioinfo->pchNext;
    if (moving) {
        if (m_wfx.nChannels == 2)
            *ret_samples_read = fp_mov_16216s2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
        else if (m_wfx.wBitsPerSample == 8)
            *ret_samples_read = fp_mov_8216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
        else
            *ret_samples_read = fp_mov_16216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
    } else {
        if (m_wfx.nChannels == 2)
            *ret_samples_read = fp_add_16216s2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
        else if (m_wfx.wBitsPerSample == 8)
            *ret_samples_read = fp_add_8216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
        else
            *ret_samples_read = fp_add_16216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
    }

    if (is_fading() || m_flags.ramping) {
        if (m_flags.last_fade) {
            m_flags.done = 1;
        } else {
            set_volume(m_fp_volume - m_fp_fade_amount);
            if (m_flags.fade_in) {
                if (m_fp_volume >= m_fp_reset_volume) {
                    m_flags.fading = 0;
                    m_flags.fade_in = 0;
                }
            } else if (m_flags.ramping) {
                if (m_fp_fade_amount > 0.0) {
                    if (m_fp_volume <= m_fp_dst_volume)
                        m_flags.ramping = 0;
                } else {
                    if (m_fp_volume >= m_fp_dst_volume)
                        m_flags.ramping = 0;
                }
            } else {
                if (m_fp_volume == 0.0)
                    m_flags.done = 1;
                if (m_fp_volume <= m_fp_fade_amount) {
                    m_fp_fade_amount = m_fp_volume;
                    m_flags.last_fade = 1;
                }
            }
            set_pan(m_pan);
        }
    }

    if (*ret_samples_read > nsamples)
        return SNDERR_BAD_PARAM;

    m_pmmioinfo->pchNext = psrc_temp;

    if (*ret_samples_read != nsamples) {
        if (moving) {
            unsigned long unfilled = nsamples - *ret_samples_read;
            memset(pdst_temp + *ret_samples_read * 16, 0, unfilled * 16);
        }
        m_flags.done = 1;
        return SNDERR_OK;
    }
    *ret_samples_read = nsamples;

    if (need_to_advance()) {
        m_msgs_pending++;
        if (gbl_pwave_device != 0 && gbl_pwave_device->is_debug()) {
            PostMessageA(m_hwnd, WF_ID_ADVANCE, (WPARAM)this, 0);
            return SNDERR_OK;
        }
        gbl_sound_thread.post_message(0x7eb, (long)this, 0);
    }
    return SNDERR_OK;
}

static inline CWaveFile *VoiceOf(CWaveMgr *s)
{
    return s;
}

SNDERR CWaveFile::stream_to_hwbuffer(unsigned long timePast, unsigned long filter_index,
                                     unsigned long *ret_samples_read)
{
    char *psrc_temp = 0;
    SNDERR retval = SNDERR_OK;
    unsigned long samples_read = 0;
    unsigned long samples_to_read = 0;
    int moving = 0;
    unsigned char bHandToQueued = 0;
    char *pCurEndRead = 0;
    unsigned long total_nbytes_to_write = 0;
    char *curWrite;
    unsigned long time_past = 0;
    unsigned long position = 0;
    unsigned long dsb_size1 = 0;
    unsigned long dsb_size2 = 0;
    void *pdsb1 = 0;
    void *pdsb2 = 0;
    unsigned long nsamples;
    unsigned long cur_nbytes_to_write;
    unsigned long cur_nbytes_to_read;

    if (!m_phwdsb)
        return SNDERR_UNINITIALIZED;

    psrc_temp = m_pmmioinfo->pchNext;
    pCurEndRead = get_pend();

    retval = m_phwdsb->get_write_info(&total_nbytes_to_write, &position, time_past);
    if (retval != 0 && retval != 0x2d)
        return retval;

    nsamples = m_phwdsb->bytes_to_samples(total_nbytes_to_write);

    m_phwdsb->lock(position, total_nbytes_to_write, &pdsb1, &dsb_size1, &pdsb2, &dsb_size2);
    curWrite = (char *)pdsb1;
    cur_nbytes_to_write = dsb_size1;

    while (total_nbytes_to_write != 0) {
        if (total_nbytes_to_write + psrc_temp > pCurEndRead) {
            cur_nbytes_to_read = pCurEndRead - psrc_temp;
            if ((!m_flags.looping && m_flags.eod) || (m_ppQueuedSound != 0 && m_flags.eod)) {
                if (psrc_temp > m_pmmioinfo->pchEndRead)
                    m_flags.last_loop = 1;
                else {
                    m_flags.done = 1;
                }
            }
        } else
            cur_nbytes_to_read = total_nbytes_to_write;
        if (cur_nbytes_to_read == 0)
            continue;

        for (;;) {
            if (cur_nbytes_to_write == 0 && dsb_size2 != 0) {
                cur_nbytes_to_write = dsb_size2;
                curWrite = (char *)pdsb2;
            }
            if (cur_nbytes_to_read <= cur_nbytes_to_write) {
                if (cur_nbytes_to_read < cur_nbytes_to_write && m_flags.done && m_ppQueuedSound != 0 && m_flags.eod) {
                    memcpy(curWrite, psrc_temp, cur_nbytes_to_read);
                    total_nbytes_to_write -= cur_nbytes_to_read;
                    cur_nbytes_to_write -= cur_nbytes_to_read;
                    curWrite += cur_nbytes_to_read;

                    if ((*m_ppQueuedSound)->is_mpeg()) {
                        psrc_temp = VoiceOf(*m_ppQueuedSound)->m_first_mpeg_frame->get_pnext();
                        pCurEndRead = VoiceOf(*m_ppQueuedSound)->m_first_mpeg_frame->get_pend();
                        if (psrc_temp + cur_nbytes_to_write > pCurEndRead)
                            cur_nbytes_to_read = VoiceOf(*m_ppQueuedSound)->m_first_mpeg_frame->get_pend() - psrc_temp;
                        else
                            cur_nbytes_to_read = cur_nbytes_to_write;
                    } else {
                        psrc_temp = (*m_ppQueuedSound)->get_pnext();
                        pCurEndRead = (*m_ppQueuedSound)->get_pend();
                        cur_nbytes_to_read = cur_nbytes_to_write;
                    }

                    memcpy(curWrite, psrc_temp, cur_nbytes_to_read);
                    curWrite += cur_nbytes_to_read;
                    total_nbytes_to_write -= cur_nbytes_to_read;
                    cur_nbytes_to_write -= cur_nbytes_to_read;
                    psrc_temp += cur_nbytes_to_write;

                    if ((*m_ppQueuedSound)->is_mpeg()) {
                        if (psrc_temp >= pCurEndRead) {
                            VoiceOf(*m_ppQueuedSound)->setNextMpegFrame();
                            psrc_temp = VoiceOf(*m_ppQueuedSound)->m_first_mpeg_frame->get_pnext();
                            pCurEndRead = VoiceOf(*m_ppQueuedSound)->m_first_mpeg_frame->get_pend();
                        } else {
                            VoiceOf(*m_ppQueuedSound)->m_first_mpeg_frame->set_pnext(psrc_temp);
                        }
                        bHandToQueued = 1;
                    } else {
                        VoiceOf(*m_ppQueuedSound)->m_pmmioinfo->pchNext += cur_nbytes_to_read;
                        bHandToQueued = 1;
                    }
                } else {
                    memcpy(curWrite, psrc_temp, cur_nbytes_to_read);
                    total_nbytes_to_write -= cur_nbytes_to_read;
                    cur_nbytes_to_write -= cur_nbytes_to_read;
                    psrc_temp += cur_nbytes_to_read;
                    curWrite += cur_nbytes_to_read;
                    if (m_flags.last_loop) {
                        psrc_temp = get_pstart();
                        m_flags.last_loop = 0;
                    } else if (psrc_temp == m_pmmioinfo->pchEndRead && !m_flags.done) {
                        psrc_temp = m_pmmioinfo->pchBuffer;
                    } else if (m_flags.done && cur_nbytes_to_write != 0) {
                        memset(curWrite, 0, cur_nbytes_to_write);
                        total_nbytes_to_write -= cur_nbytes_to_write;
                        cur_nbytes_to_write = 0;
                    }
                }
                break;
            }
            memcpy(curWrite, psrc_temp, cur_nbytes_to_write);
            total_nbytes_to_write -= cur_nbytes_to_write;
            cur_nbytes_to_read -= cur_nbytes_to_write;
            psrc_temp += cur_nbytes_to_write;
            cur_nbytes_to_write = 0;
            if (cur_nbytes_to_read == 0)
                break;
        }
    }

    m_phwdsb->unlock(pdsb1, dsb_size1, pdsb2, dsb_size2);

    {
        if (is_fading() || m_flags.ramping) {
            if (m_flags.last_fade) {
                m_flags.done = 1;
                if (m_ppCurSound != 0)
                    *m_ppCurSound = 0;
            } else if (m_flags.fade_in) {
                if (m_fp_volume >= m_fp_reset_volume) {
                    m_flags.fading = 0;
                    m_flags.fade_in = 0;
                }
            } else {
                if (m_flags.ramping) {
                    if (m_fp_fade_amount > 0.0) {
                        if (m_fp_volume > m_fp_dst_volume) {
                            if (m_fp_volume < m_fp_fade_amount)
                                m_fp_volume = m_fp_dst_volume;
                            else
                                m_fp_volume = m_fp_volume - m_fp_fade_amount;
                            m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
                        } else
                            m_flags.ramping = 0;
                    } else {
                        if (m_fp_volume < m_fp_dst_volume) {
                            if (m_fp_dst_volume - m_fp_volume > fabs(m_fp_fade_amount))
                                m_fp_volume = m_fp_volume - m_fp_fade_amount;
                            else
                                m_fp_volume = m_fp_dst_volume;
                            m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
                        } else
                            m_flags.ramping = 0;
                    }
                } else {
                    if (m_fp_volume == 0.0) {
                        m_flags.done = 1;
                        if (m_ppCurSound != 0)
                            *m_ppCurSound = 0;
                    }
                    if (m_fp_volume <= m_fp_fade_amount) {
                        m_flags.last_fade = 1;
                        m_fp_fade_amount = m_fp_volume;
                    }
                    m_fp_volume = m_fp_volume - m_fp_fade_amount;
                    m_phwdsb->set_volume((unsigned long)(m_fp_volume * 127.0));
                }
            }
        }
    }

    if (bHandToQueued) {
        VoiceOf(*m_ppQueuedSound)->m_fp_volume = m_fp_volume;
        VoiceOf(*m_ppQueuedSound)->m_fp_fade_amount = m_fp_fade_amount;
        VoiceOf(*m_ppQueuedSound)->m_fp_dst_volume = m_fp_dst_volume;
        VoiceOf(*m_ppQueuedSound)->m_volume = m_volume;
        VoiceOf(*m_ppQueuedSound)->m_reset_volume = m_reset_volume;
        VoiceOf(*m_ppQueuedSound)->m_right_volume = m_right_volume;
        VoiceOf(*m_ppQueuedSound)->m_left_volume = m_left_volume;
        VoiceOf(*m_ppQueuedSound)->m_reset_right_volume = m_reset_right_volume;
        VoiceOf(*m_ppQueuedSound)->m_reset_left_volume = m_reset_left_volume;
        if (!(VoiceOf(*m_ppQueuedSound)->is_mpeg())) {
            if (VoiceOf(*m_ppQueuedSound)->need_to_advance()) {
                ((CWaveFile *)m_ppQueuedSound)->m_flags.advance_pending = 1;
                ((CWaveFile *)m_ppQueuedSound)->CWaveFile::m_msgs_pending++;
                gbl_sound_thread.post_message(0x7eb, (long)*m_ppQueuedSound, 0);
            }
        }
        gbl_pwave_device->add_to_play_list(*m_ppQueuedSound);
        VoiceOf(*m_ppQueuedSound)->m_phwdsb = m_phwdsb;
        VoiceOf(*m_ppQueuedSound)->m_flags.hwbuffer = 1;
        m_phwdsb = 0;
        *m_ppCurSound = *m_ppQueuedSound;
        *m_ppQueuedSound = 0;
        set_shared_dsb();
        set_done();
        m_ppQueuedSound = 0;
        m_ppCurSound = 0;
        return SNDERR_NO_SUPPORT;
    }

    m_pmmioinfo->pchNext = psrc_temp;
    *ret_samples_read = nsamples;
    if (need_to_advance()) {
        m_msgs_pending++;
        gbl_sound_thread.post_message(0x7eb, (long)this, 0);
    }
    return SNDERR_NO_SUPPORT;
}

SNDERR CWaveFile::get_buffer_samples(WaveBuffer *pwb, void *pdst, unsigned long nsamples, unsigned long filter_index,
                                     unsigned long *ret_samples_read)
{
    unsigned long samples_read = 0;
    unsigned long nsamples_processed = 0;
    int idst = 0;
    int isrc = 0;
    int moving = 0;
    unsigned long debug_time = 0;
    CWaveMixer *pmixer = gbl_pwave_device->get_mixer();
    PFNFILTER pfilter;
    if (pmixer && pmixer->uses_mmx())
        pfilter = mmx_filter_table[filter_index];
    else
        pfilter = filter_table[filter_index];
    char *psrc_temp = pwb->get_pnext();
    if (filter_index >= 0xc)
        moving = 1;

    if (!pwb->is_done()) {
        pwb->set_time(pwb->get_time() + m_time_past);
        if (pwb->get_delay() > (unsigned)pwb->get_time()) {
            *ret_samples_read = nsamples;
            if (moving)
                memset(pdst, 0, nsamples * 16);
            return SNDERR_OK;
        }
        if (moving) {
            if (m_wfx.nChannels == 2)
                *ret_samples_read = fp_mov_16216s2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
            else if (m_wfx.wBitsPerSample == 8)
                *ret_samples_read = fp_mov_8216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
            else
                nsamples_processed = fp_mov_16216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, pwb, 0);
        } else {
            if (m_wfx.nChannels == 2)
                *ret_samples_read = fp_add_16216s2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
            else if (m_wfx.wBitsPerSample == 8)
                *ret_samples_read = fp_mov_8216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, this, 1);
            else
                nsamples_processed = fp_add_16216m2smb((double *)pdst, (void **)&psrc_temp, nsamples, pwb, 0);
        }
        *ret_samples_read = nsamples_processed;
        pwb->set_pnext(psrc_temp);
    }

    if (pwb->is_fading()) {
        if (pwb->m_flags.last_fade) {
            pwb->m_flags.fading = 0;
            pwb->m_flags.last_fade = 0;
            pwb->reset();
            pwb->set_done(1);
            pwb->set_volume(m_fp_volume);
            pwb->set_pan(m_pan);
            pwb->set_fade(m_fade_time);
            pwb->m_midi_note = 0;
            pwb->m_flags.playing = 0;
        } else {
            pwb->set_volume(pwb->m_fp_volume - pwb->m_fp_fade_amount);
            if (pwb->m_flags.fade_in) {
                if (pwb->m_fp_volume >= pwb->m_fp_reset_volume) {
                    pwb->m_flags.fading = 0;
                    pwb->m_flags.fade_in = 0;
                }
            } else if (pwb->m_fp_volume <= 0.0) {
                pwb->m_flags.fading = 0;
                pwb->m_flags.last_fade = 0;
                pwb->reset();
                pwb->set_done(1);
                pwb->set_volume(m_fp_volume);
                pwb->set_fade(m_fade_time);
                pwb->m_midi_note = 0;
                pwb->m_flags.fading = 0;
            } else {
                if (pwb->m_fp_volume < pwb->m_fp_fade_amount) {
                    pwb->m_fp_fade_amount = pwb->m_fp_volume;
                    pwb->m_flags.last_fade = 1;
                }
            }
            pwb->set_pan(m_pan);
        }
    }

    if (nsamples_processed != nsamples) {
        if (moving && !m_flags.mpeg)
            memset((char *)pdst + nsamples_processed * 16, 0, (nsamples - nsamples_processed) * 16);
        pwb->reset();
        pwb->set_done(1);
    }
    return SNDERR_OK;
}

SNDERR CWaveFile::queueSound(CWaveMgr **ppQueueSound, CWaveMgr **ppCurSound)
{
    assert(m_ppQueuedSound == NULL);

    m_ppQueuedSound = ppQueueSound;
    m_ppCurSound = ppCurSound;
    m_flags.looping = 0;
#ifdef USE_MPEG
    if (m_flags.mpeg)
        m_milesMpeg.setLoopState(0);
#endif
    return SNDERR_OK;
}

void CWaveFile::set_buffer_done(int buffer_index)
{
    instance[buffer_index].set_done(true);
    instance[buffer_index].set_release(false);
}

void CWaveFile::set_buffer_inactive(int buffer_index)
{
    unsigned long mask = 1;
    mask <<= buffer_index;
    m_flags.buffer_bits &= ~mask;
}

inline int CWaveFile::get_buffer_state(unsigned long buffer_index)
{
    unsigned long mask = 1;
    mask <<= buffer_index;
    return m_flags.buffer_bits & mask;
}

void CWaveFile::set_release(bool release)
{
    if (m_flags.all_in_buffer) {
        for (int i = 0; i < 16; i++)
            instance[i].m_flags.release = release;
    } else {
        m_flags.release = release;
    }
}

unsigned long CWaveFile::get_time(unsigned long instance_index)
{
    if (get_buffer_state(instance_index))
        return instance[instance_index].m_time;
    return 0;
}

void CWaveFile::set_loop_state(int state)
{
    m_flags.looping = state;
    for (int i = 0; i < 16; i++)
        instance[0].set_loop_state(state);
}

SNDERR CWaveFile::set_static()
{
    int timeout = 0x10000;

    if (!m_flags.loaded) {
        m_flags.all_in_buffer = 1;
        return SNDERR_OK;
    }

    if (is_playing())
        set_done();

    while (1) {
        if (!m_flags.active)
            break;
        if (m_flags.done)
            break;
        if (--timeout == 0)
            break;
        Sleep(1);
    }

    if (is_playing())
        return SNDERR_NOCANDO;

    mmioGetInfo(m_hfile, &m_mmioinfo, 0);
    mmioSetBuffer(m_hfile, 0, m_data_size, 0);
    mmioGetInfo(m_hfile, &m_mmioinfo, 0);
    mmioSeek(m_hfile, m_dataoffset, 0);
    mmioGetInfo(m_hfile, &m_mmioinfo, 0);
    mmioAdvance(m_hfile, &m_mmioinfo, 0);

    if (m_pmmioinfo != 0 && m_pmmioinfo != &m_mmioinfo) {
        ::operator delete(m_pmmioinfo);
        m_pmmioinfo = &m_mmioinfo;
    }

    m_flags.all_in_buffer = 1;
    for (int i = 0; i < 16; i++) {
        instance[i].init(m_pmmioinfo, &m_wfx);
        instance[i].set_end_position(m_curendpos);
        instance[i].set_start_position(m_curstartpos);
        instance[i].set_current_position(0);
        instance[i].set_done(true);
        set_buffer_done(i);
    }
    return SNDERR_OK;
}

SNDERR CWaveFile::set_position3d(float x, float y, float z)
{
    if (m_flags.buffer3d && m_phwdsb)
        return m_phwdsb->set_position3d(x, y, z);
    return SNDERR_NO_SUPPORT;
}

SNDERR CWaveFile::set_xpos(float x)
{
    if (m_flags.buffer3d && m_phwdsb)
        return m_phwdsb->set_xpos(x);
    return SNDERR_NO_SUPPORT;
}

SNDERR CWaveFile::set_ypos(float y)
{
    if (m_flags.buffer3d && m_phwdsb)
        return m_phwdsb->set_ypos(y);
    return SNDERR_NO_SUPPORT;
}

SNDERR CWaveFile::set_zpos(float z)
{
    if (m_flags.buffer3d && m_phwdsb)
        return m_phwdsb->set_zpos(z);
    return SNDERR_NO_SUPPORT;
}

SNDERR CWaveFile::set_reverb_mix(float mix)
{
    if (m_flags.buffer3d && m_phwdsb)
        return m_phwdsb->set_reverb_mix(mix);
    return SNDERR_NO_SUPPORT;
}

WaveBuffer::WaveBuffer()
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_volume = 0x10000;
    m_fp_volume = 1.0;
    m_fp_reset_volume = 1.0;
    m_left_volume = 0x10000;
    m_fp_left_volume = 1.0;
    m_right_volume = 0x10000;
    m_fp_right_volume = 1.0;
    m_current_position = 0;
    m_start_position = 0;
    m_end_position = 0;
    m_shifter.whole = 0;
    m_pitch = 0x10000;
    m_time = 0;
    m_delay = 0;
    set_fade(1000);
    m_time_past = 0;
    m_loop_count = 0;
    m_phwdsb = 0;
    m_plinked_wb = 0;
    m_fp_shifter = 0.0;
    m_fp_pitch = 1.0;
    m_fade_amount = 0;
    m_ploop_end = 0;
    m_ploop_start = 0;
    m_loop_size = 0;
    m_midi_note = 0;
    m_pan = 0;
    m_velocity = 0x80;
}

WaveBuffer::~WaveBuffer()
{
}

void WaveBuffer::init(MMIOINFO *pmmioinfo, WAVEFORMATEX *pwfx)
{
    if (pmmioinfo)
        m_mmioinfo = *pmmioinfo;

    m_current_position = 0;
    m_start_position = 0;
    m_end_position = 0;

    m_wfx = *pwfx;
}

void WaveBuffer::reset()
{
    m_mmioinfo.pchNext = m_mmioinfo.pchBuffer;
    m_current_position = 0;
    m_start_position = 0;
    m_time = 0;
    m_flags.release = 0;
    m_flags.done = 0;
    m_flags.fading = 0;
    int bad;
    m_fp_fade_amount = m_fp_reset_fade_amount;
    if (m_fp_fade_amount == 0.0)
        bad = 1;
}

unsigned long WaveBuffer::get_nsamples_left_in_buffer()
{
    return btos(m_mmioinfo.pchEndRead - m_mmioinfo.pchNext);
}

void WaveBuffer::set_pan(int pan)
{
    if (pan < -64)
        pan = -64;
    else if (pan > 63)
        pan = 63;

    int curvol = m_volume;
    m_pan = pan;

    if (pan == 0) {
        m_right_volume = curvol;
        m_left_volume = curvol;
        m_fp_right_volume = m_fp_volume;
        m_fp_left_volume = m_fp_volume;
        return;
    }

    int scale = 63 + (pan < 0);

    double fp_scaled_pan = pan * m_fp_volume;
    fp_scaled_pan /= scale;

    int scaled_pan = curvol * pan / scale;

    double new_fp_right = m_fp_volume + fp_scaled_pan;
    double new_fp_left = m_fp_volume - fp_scaled_pan;
    int new_right = curvol + scaled_pan;
    int new_left = curvol - scaled_pan;

    if (new_right > 0x10000)
        new_right = 0x10000;
    if (new_fp_right > 1.0)
        new_fp_right = 1.0;
    if (new_left > 0x10000)
        new_left = 0x10000;
    if (new_fp_left > 1.0)
        new_fp_left = 1.0;

    m_right_volume = new_right;
    m_left_volume = new_left;
    m_fp_right_volume = new_fp_right;
    m_fp_left_volume = new_fp_left;
}

void WaveBuffer::set_fade(unsigned long ms)
{
    if (gbl_pwave_device != 0) {
        double rate = gbl_pwave_device->get_rate();
        if (ms > rate) {
            double number_of_decrements = ms / rate;
            m_fade_amount = (int)(get_volume() / number_of_decrements);
            m_fp_reset_fade_amount = m_fp_fade_amount = m_fp_volume / number_of_decrements;
        } else {
            m_fade_amount = get_volume();
            m_fp_reset_fade_amount = m_fp_fade_amount = m_fp_volume;
        }
    }
}

void WaveBuffer::set_fade_in(unsigned long ms)
{
    if (gbl_pwave_device != 0) {
        double rate = gbl_pwave_device->get_rate();
        if (ms > rate) {
            double number_of_increments = ms / rate;
            m_fade_amount = -(int)(m_volume / number_of_increments);
            m_fp_reset_fade_amount = m_fp_fade_amount = -(m_fp_volume / number_of_increments);
            m_fp_reset_volume = m_fp_volume;
        } else {
            m_fade_amount = -m_volume;
            m_fp_reset_fade_amount = m_fp_fade_amount = -m_fp_volume;
            m_fp_reset_volume = m_fp_volume;
        }
    }
}

void WaveBuffer::fade()
{
    if (m_fp_volume != 0.0 && m_fp_fade_amount != 0.0) {
        m_flags.fading = 1;
        return;
    }
    m_midi_note = 0;
    m_flags.done = 1;
    m_flags.playing = 0;
}

void WaveBuffer::fade_in()
{
    if (m_fp_fade_amount < 0.0) {
        if (!m_flags.fading && !m_flags.fade_in) {
            m_flags.fading = 1;
            m_flags.fade_in = 1;
            m_fp_reset_volume = m_fp_volume;
            m_fp_volume = 0;
        }
    }
}

WaveBuffer *CWaveFile::get_free_instance(unsigned long *i)
{
    unsigned long mask = m_flags.buffer_bits;
    *i = 0;
    while ((mask & 1) != 0) {
        *i = *i + 1;
        mask >>= 1;
    }
    if (*i >= 0x10)
        return 0;
    return &instance[*i];
}

WaveBuffer *CWaveFile::find_midi_instance(unsigned long midi_note)
{
    unsigned long mask = m_flags.buffer_bits;
    for (int i = 0; i < 16; mask >>= 1, i++)
        if ((mask & 1) && instance[i].m_midi_note == midi_note && !instance[i].m_flags.fading)
            return &instance[i];
    return 0;
}

void WaveBuffer::set_volume(unsigned long volume)
{
    m_fp_volume = volume * (1.0 / 127.0);
    if (volume != 0)
        volume++;
    m_volume = volume << 9;
    set_pan(get_pan());
}

void CWaveFile::ClearAndReleaseStopNotifyFlag()
{
    if (m_ppCurSound) {
        *m_ppCurSound = 0;
        m_ppCurSound = 0;
    }
}
