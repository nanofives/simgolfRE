#include "sounddevice.h"
#include "dlist.h"

INT32 lpc10_bits[LPC10_BITS_IN_COMPRESSED_FRAME];
real lpc10_encode_buffer[LPC10_SAMPLES_PER_FRAME] = {0};

CVoiceTxMgr::CVoiceTxMgr()
{
    m_pbuffer_out = 0;
    memset(&m_flags, 0, sizeof m_flags);
    m_pacm = 0;
    m_lpc10_state = 0;
    m_buffers_taken = 0;
    m_buffers_returned = 0;
    m_buffers_processed = 0;
    m_start_id = 0;
}

CVoiceTxMgr::~CVoiceTxMgr()
{
    HMMIO *phmmio = m_buffers.remove_head();
    while (phmmio) {
        mmioClose(*phmmio, 0);
        gbl_mmio--;
        delete phmmio;
        phmmio = m_buffers.remove_head();
    }
    if (gbl_pwave_in_device)
        gbl_pwave_in_device->unhook_vtx();
}

SNDERR CVoiceTxMgr::init(unsigned long buffer_size, WAVEFORMATEX *lpwfx, unsigned long flags)
{
    SNDERR retval = SNDERR_OK;
    m_buffer_size = buffer_size;
    if (!gbl_pwave_in_device)
        return SNDERR_UNINITIALIZED;
    retval = gbl_pwave_in_device->hook_vtx(this);
    return retval;
}

SNDERR CVoiceTxMgr::release()
{
    HMMIO *phmmio = m_buffers.remove_head();
    while (phmmio) {
        mmioClose(*phmmio, 0);
        gbl_mmio--;
        delete phmmio;
        phmmio = m_buffers.remove_head();
    }
    m_pacm = 0;
    return SNDERR_OK;
}

SNDERR CVoiceTxMgr::unload()
{
    MMIOINFO mmioinfo;
    HMMIO *phmmio = m_buffers.remove_head();
    while (phmmio) {
        mmioGetInfo(*phmmio, &mmioinfo, 0);
        mmioClose(*phmmio, 0);
        gbl_mmio--;
        delete phmmio;
        phmmio = m_buffers.remove_head();
    }
    if (gbl_pwave_in_device)
        gbl_pwave_in_device->unhook_vtx();
    m_flags.codec_initialized = 0;
    return SNDERR_OK;
}

void CVoiceTxMgr::s2f_and_scale(float *fdst, short *isrc, int nsamples)
{
    for (int i = 0; i < nsamples; i++)
        fdst[i] = isrc[i] * (1.0f / 32768.0f);
}

SNDERR CVoiceTxMgr::add_buffer(WAVEHDR *pwhdr)
{
    SNDERR result = SNDERR_OK;
    if (!pwhdr)
        return SNDERR_BAD_PARAM;
    if (m_buffers.get_nnodes() > 100)
        return SNDERR_ALLOCATED;

    MMIOINFO mmioinfo;
    int i;
    memset(&mmioinfo, 0, sizeof(mmioinfo));
    unsigned long dst_size = LPC10_BITS_IN_COMPRESSED_FRAME;
    mmioinfo.fccIOProc = FOURCC_MEM;
    mmioinfo.cchBuffer = dst_size;
    mmioinfo.adwInfo[0] = 0;
    mmioinfo.pchBuffer = 0;

    HMMIO *phfile = new HMMIO;
    *phfile = mmioOpenA(0, &mmioinfo, MMIO_ALLOCBUF | MMIO_CREATE | MMIO_READWRITE);
    if (!*phfile)
        return SNDERR_NOCANDO;
    ++gbl_mmio;
    mmioGetInfo(*phfile, &mmioinfo, 0);

    s2f_and_scale(lpc10_encode_buffer, (short *)pwhdr->lpData, LPC10_SAMPLES_PER_FRAME);

    for (i = 0; i < 1; i++)
        lpc10_encode(&lpc10_encode_buffer[i * LPC10_SAMPLES_PER_FRAME], &lpc10_bits[i * LPC10_BITS_IN_COMPRESSED_FRAME],
                     m_lpc10_state);

    lpc_write_bits(mmioinfo.pchBuffer, lpc10_bits, LPC10_BITS_IN_COMPRESSED_FRAME);

    m_buffers_processed++;
    mmioinfo.pchEndRead = mmioinfo.pchBuffer + 7;
    mmioSetInfo(*phfile, &mmioinfo, 0);
    m_buffers.add_to_end(phfile);
    return result;
}

unsigned long CVoiceTxMgr::get_nbuffers()
{
    return m_buffers.get_nnodes();
}

MMIOINFO *CVoiceTxMgr::get_next_buffer()
{
    if (m_pbuffer_out != 0)
        return 0;

    HMMIO *phmmio = m_buffers.remove_head();
    if (phmmio == 0)
        return 0;

    MMIOINFO *pmmioinfo = new MMIOINFO;
    memset(pmmioinfo, 0, sizeof(MMIOINFO));
    mmioGetInfo(*phmmio, pmmioinfo, 0);
    delete phmmio;
    m_pbuffer_out = pmmioinfo;
    m_buffers_taken++;
    return pmmioinfo;
}

SNDERR CVoiceTxMgr::return_buffer(MMIOINFO *pmmioinfo)
{
    if (pmmioinfo != m_pbuffer_out)
        return SNDERR_OUT_OF_SYNC;

    gbl_mmio--;
    mmioClose(pmmioinfo->hmmio, 0);
    m_buffers_returned++;
    delete m_pbuffer_out;
    m_pbuffer_out = 0;
    return SNDERR_OK;
}

SNDERR CVoiceTxMgr::start()
{
    SNDERR retval = SNDERR_OK;
    m_buffers_processed = 0;
    m_buffers_taken = 0;
    m_start_id++;
    m_buffers_returned = 0;
    m_lpc10_state = create_lpc10_encoder_state();
    gbl_pwave_in_device->hook_vtx(this);
    return retval;
}

SNDERR CVoiceTxMgr::stop()
{
    return unload();
}

void CVoiceTxMgr::lpc_write_bits(char *dstbuff, int *bits, int len)
{
    unsigned char mask = 0x80;
    unsigned char data = 0;
    int bit_write = 0;
    for (int i = 0; i < len; i++) {
        if (bits[i])
            data |= mask;
        mask >>= 1;
        if (mask == 0 || i == len - 1) {
            dstbuff[bit_write++] = data;
            data = 0;
            mask = 0x80;
        }
    }
}
