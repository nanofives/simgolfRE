#include <windows.h>
#include <mmsystem.h>
#include "sounddevice.h"
#include "dlist.h"
#include "waveindevicemgr.h"

WaveInDeviceMgr::WaveInDeviceMgr()
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_ndevices = waveInGetNumDevs();
    m_device_id = 0;
    waveInGetDevCapsA(WAVE_MAPPER, &m_caps, sizeof(WAVEINCAPSA));
    m_wfx.wFormatTag = WAVE_FORMAT_PCM;
    m_wfx.nChannels = 1;
    m_wfx.nSamplesPerSec = 8000;
    m_wfx.nAvgBytesPerSec = 16000;
    m_wfx.nBlockAlign = 2;
    m_wfx.wBitsPerSample = 16;
    m_wfx.cbSize = 0;
    m_handle = 0;
}

static SNDERR ReleaseCapture(WaveInDeviceMgr *m);

WaveInDeviceMgr::~WaveInDeviceMgr()
{
    ReleaseCapture(this);
}

SNDERR WaveInDeviceMgr::init(unsigned long nbuffers, DWORD buffer_size)
{
    unsigned long i;
    WIBuffer *pprev;
    WIBuffer *pbuffer;
    SNDERR retval;

    pprev = 0;
    if (m_buffers.get_first() != 0)
        return SNDERR_ALLOCATED;

    i = 0;
    if (nbuffers > 0) {
        do {
            pbuffer = new WIBuffer;
            retval = pbuffer->set_buffer(buffer_size);
            if (retval != SNDERR_OK)
                return retval;
            if (pprev != 0)
                pprev->set_next(pbuffer->get_whdr());
            pprev = pbuffer;
            m_buffers.add_to_end(pbuffer);
        } while (++i < nbuffers);
    }

    pbuffer = m_buffers.get_last();
    pprev = m_buffers.get_first();
    pbuffer->set_next(pprev->get_whdr());
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::free_buffers()
{
    unsigned int retval = 0;
    WIBuffer *pcurbuffer;

    if (m_flags.recording) {
        retval = waveInStop(m_handle);
        retval = waveInReset(m_handle);
    }

    pcurbuffer = m_buffers.remove_head();
    while (pcurbuffer != 0) {
        delete pcurbuffer;
        pcurbuffer = m_buffers.remove_head();
    }
    return SNDERR_OK;
}

static void FreeBufferQueue(DList<WIBuffer> *l)
{
    WIBuffer *p = l->remove_head();
    while (p != 0) {
        delete p;
        p = l->remove_head();
    }
}

static MMRESULT StopCapture(WaveInDeviceMgr *m)
{
    MMRESULT r = MMSYSERR_NOERROR;
    if (m->m_flags.enabled) {
        waveInStop(m->m_handle);
        r = waveInReset(m->m_handle);
        m->disable();
    }
    if (m->m_flags.recording) {
        waveInStop(m->m_handle);
        r = waveInReset(m->m_handle);
    }
    return r;
}

static SNDERR ReleaseCapture(WaveInDeviceMgr *m)
{
    MMRESULT r = StopCapture(m);
    FreeBufferQueue(&m->m_buffers);
    if (r != 0)
        return m->handle_error(r);
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::start_record()
{
    waveInStart(m_handle);
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::prepare_headers(void)
{
    WIBuffer *pbuffer;
    unsigned long mmerror;

    pbuffer = m_buffers.get_first();
    if (pbuffer == 0)
        return SNDERR_UNINITIALIZED;
    if (m_handle == 0)
        return SNDERR_UNINITIALIZED;

    while (pbuffer != 0) {
        mmerror = waveInPrepareHeader(m_handle, pbuffer->get_whdr(), sizeof(WAVEHDR));
        if (mmerror != 0) {
            switch (mmerror) {
            case 2:
            case 5:
                return SNDERR_BAD_PARAM;
            case 6:
                return SNDERR_NO_DRIVER;
            case 33:
                return SNDERR_ALREADY_PLAYING;
            case 4:
                return SNDERR_ALREADY_OPEN;
            case 7:
                return SNDERR_NO_MEM;
            case 32:
                return SNDERR_BAD_FORMAT;
            }
            return SNDERR_NO_SUPPORT;
        }
        pbuffer = m_buffers.get_next();
    }
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::unprepare_headers(void)
{
    WIBuffer *pbuffer;
    unsigned long mmerror;

    pbuffer = m_buffers.get_first();
    if (pbuffer == 0)
        return SNDERR_UNINITIALIZED;
    if (m_handle == 0)
        return SNDERR_UNINITIALIZED;

    while (pbuffer != 0) {
        mmerror = waveInUnprepareHeader(m_handle, pbuffer->get_whdr(), sizeof(WAVEHDR));
        if (mmerror != 0) {
            switch (mmerror) {
            case 2:
            case 5:
                return SNDERR_BAD_PARAM;
            case 6:
                return SNDERR_NO_DRIVER;
            case 33:
                return SNDERR_ALREADY_PLAYING;
            case 4:
                return SNDERR_ALREADY_OPEN;
            case 7:
                return SNDERR_NO_MEM;
            case 32:
                return SNDERR_BAD_FORMAT;
            }
            return SNDERR_NO_SUPPORT;
        }
        pbuffer = m_buffers.get_next();
    }
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::add_buffers(void)
{
    WIBuffer *pbuffer;
    unsigned long mmerror;

    pbuffer = m_buffers.get_first();
    if (pbuffer == 0)
        return SNDERR_UNINITIALIZED;
    if (m_handle == 0)
        return SNDERR_UNINITIALIZED;

    while (pbuffer != 0) {
        mmerror = waveInAddBuffer(m_handle, pbuffer->get_whdr(), sizeof(WAVEHDR));
        if (mmerror != 0) {
            switch (mmerror) {
            case 2:
            case 5:
                return SNDERR_BAD_PARAM;
            case 6:
                return SNDERR_NO_DRIVER;
            case 33:
                return SNDERR_ALREADY_PLAYING;
            case 4:
                return SNDERR_ALREADY_OPEN;
            case 7:
                return SNDERR_NO_MEM;
            case 32:
                return SNDERR_BAD_FORMAT;
            }
            return SNDERR_NO_SUPPORT;
        }
        pbuffer = m_buffers.get_next();
    }
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::add_buffer(WIBuffer *pbuffer)
{
    if (pbuffer == 0)
        return SNDERR_BAD_PARAM;

    if (m_handle == 0)
        return SNDERR_UNINITIALIZED;

    waveInAddBuffer(m_handle, pbuffer->get_whdr(), sizeof(WAVEHDR));
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::stop_record()
{
    unsigned long retval;

    retval = waveInStop(m_handle);
    retval = waveInReset(m_handle);
    return SNDERR_OK;
}

unsigned long WaveInDeviceMgr::get_num_devices()
{
    return waveInGetNumDevs();
}

SNDERR WaveInDeviceMgr::select(unsigned long id)
{
    if (id >= m_ndevices)
        return SNDERR_BAD_PARAM;

    waveInGetDevCapsA(id, &m_caps, sizeof(m_caps));

    if (!(m_caps.dwFormats & m_format))
        return SNDERR_BAD_FORMAT;

    m_device_id = id;
    return SNDERR_OK;
}

void __stdcall WaveInDeviceMgr::call_back(HWAVEIN hwi, WORD msg, DWORD instance, DWORD p1, DWORD p2)
{
    if (msg == WIM_DATA) {
        if (gbl_pwave_device != 0 && gbl_pwave_device->is_debug())
            PostMessageA((HWND)sound_win.get_hwnd(), WI_ID_RESET, p1, (long)instance);
        else
            gbl_sound_thread.post_message(0x7f0, (long)p1, (long)instance);
    }
}

SNDERR WaveInDeviceMgr::enable(void)
{
    unsigned long retval;

    if (m_handle != 0)
        return SNDERR_ALREADY_OPEN;

    retval =
        waveInOpen(&m_handle, (UINT)-1, &m_wfx, (DWORD)::WaveInDeviceMgr::call_back, (DWORD)this, CALLBACK_FUNCTION);
    if (retval != 0)
        return handle_error(retval);

    m_flags.enabled = 1;
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::disable(void)
{
    unsigned long retval;
    unsigned int mm_result;

    if (m_handle == 0)
        return SNDERR_UNINITIALIZED;

    EnterCriticalSection(&gbl_sound_thread.m_cs);

    m_flags.enabled = 0;
    mm_result = waveInStop(m_handle);
    mm_result = waveInReset(m_handle);

    unprepare_headers();

    retval = waveInClose(m_handle);
    if (retval != 0)
        return handle_error(retval);

    LeaveCriticalSection(&gbl_sound_thread.m_cs);
    m_handle = 0;
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::handle_error(unsigned errorval)
{
    switch (errorval) {
    case 2:
    case 5:
        return SNDERR_BAD_PARAM;
    case 6:
        return SNDERR_NO_DRIVER;
    case 33:
        return SNDERR_ALREADY_PLAYING;
    case 4:
        return SNDERR_ALREADY_OPEN;
    case 7:
        return SNDERR_NO_MEM;
    case 32:
        return SNDERR_BAD_FORMAT;
    }
    return SNDERR_NO_SUPPORT;
}

SNDERR WaveInDeviceMgr::set_format(DWORD format)
{
    m_wfx.wFormatTag = 1;
    m_wfx.nChannels = 1;
    m_wfx.nSamplesPerSec = 8000;
    m_wfx.nAvgBytesPerSec = 16000;
    m_wfx.nBlockAlign = 2;
    m_wfx.wBitsPerSample = 16;
    m_wfx.cbSize = 0;

    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::process_buffer(WIBuffer *pwib)
{
    if (pwib == 0)
        return SNDERR_BAD_PARAM;

    waveInUnprepareHeader(m_handle, pwib->get_whdr(), sizeof(WAVEHDR));

    pwib->clear_flags();

    if (m_pvtx != 0)
        m_pvtx->add_buffer(pwib->get_whdr());

    waveInPrepareHeader(m_handle, pwib->get_whdr(), sizeof(WAVEHDR));
    waveInAddBuffer(m_handle, pwib->get_whdr(), sizeof(WAVEHDR));

    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::hook_vtx(CVoiceTxMgr *pvtx)
{
    m_pvtx = pvtx;
    return SNDERR_OK;
}

SNDERR WaveInDeviceMgr::unhook_vtx()
{
    m_pvtx = 0;
    return SNDERR_OK;
}

WIBuffer::WIBuffer()
{
    m_whdr.lpData = 0;
    m_whdr.dwBufferLength = 0;
    m_whdr.dwBytesRecorded = 0;
    m_whdr.dwUser = (DWORD)this;
    m_whdr.dwFlags = 0;
    m_whdr.dwLoops = 0;
    m_whdr.lpNext = 0;
    m_whdr.reserved = 0;
}

WIBuffer::~WIBuffer()
{
    if (m_whdr.lpData)
        delete m_whdr.lpData;
    m_whdr.lpData = 0;
}

SNDERR WIBuffer::set_buffer(DWORD size)
{
    if (m_whdr.lpData)
        return SNDERR_ALLOCATED;
    if (!size)
        return SNDERR_BAD_PARAM;
    m_whdr.lpData = (char *)::operator new(size);
    m_whdr.dwBufferLength = size;
    return SNDERR_OK;
}

void WIBuffer::set_next(WAVEHDR *lpwhdr)
{
    m_whdr.lpNext = lpwhdr;
}

WAVEHDR *WIBuffer::get_whdr()
{
    return &m_whdr;
}
