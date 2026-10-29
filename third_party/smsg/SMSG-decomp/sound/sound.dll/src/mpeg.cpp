#include <windows.h>
#include <mmsystem.h>
#include "sound.h"
#include "mpeg.h"

#ifdef USE_MPEG

WAVEFORMATEX default_wfx = {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};

ASI_ERROR ASI_error;

ASI_STREAM_OPEN ASI_stream_open;
ASI_STREAM_PROCESS ASI_stream_process;
ASI_STREAM_SEEK ASI_stream_seek;
ASI_STREAM_CLOSE ASI_stream_close;
ASI_STREAM_ATTRIBUTE ASI_stream_attribute;
ASI_STREAM_SET_PREFERENCE ASI_stream_set_preference;

HATTRIB INPUT_BIT_RATE;
HATTRIB INPUT_SAMPLE_RATE;
HATTRIB INPUT_BITS;
HATTRIB INPUT_CHANNELS;
HATTRIB OUTPUT_BIT_RATE;
HATTRIB OUTPUT_SAMPLE_RATE;
HATTRIB OUTPUT_BITS;
HATTRIB OUTPUT_CHANNELS;
HATTRIB POSITION;
HATTRIB PERCENT_DONE;
HATTRIB MIN_INPUT_BLOCK_SIZE;
HATTRIB RAW_RATE;
HATTRIB RAW_BITS;
HATTRIB RAW_CHANNELS;
HATTRIB REQUESTED_BIT_RATE;
HATTRIB REQUESTED_RATE;
HATTRIB REQUESTED_BITS;
HATTRIB REQUESTED_CHANS;
HATTRIB REQUESTED_LAYER;

RIB_INTERFACE_ENTRY ASICODEC[] = {
    {RIB_FUNCTION, "ASI_error", (U32)&ASI_error, RIB_NONE},
};

RIB_INTERFACE_ENTRY ASISTR[] = {
    {RIB_FUNCTION, "ASI_stream_attribute", (U32)&ASI_stream_attribute, RIB_NONE},
    {RIB_FUNCTION, "ASI_stream_open", (U32)&ASI_stream_open, RIB_NONE},
    {RIB_FUNCTION, "ASI_stream_seek", (U32)&ASI_stream_seek, RIB_NONE},
    {RIB_FUNCTION, "ASI_stream_close", (U32)&ASI_stream_close, RIB_NONE},
    {RIB_FUNCTION, "ASI_stream_process", (U32)&ASI_stream_process, RIB_NONE},
    {RIB_FUNCTION, "ASI_stream_set_preference", (U32)&ASI_stream_set_preference, RIB_NONE},

    {RIB_ATTRIBUTE, "Input bit rate", (U32)&INPUT_BIT_RATE, RIB_NONE},
    {RIB_ATTRIBUTE, "Input sample rate", (U32)&INPUT_SAMPLE_RATE, RIB_NONE},
    {RIB_ATTRIBUTE, "Input sample width", (U32)&INPUT_BITS, RIB_NONE},
    {RIB_ATTRIBUTE, "Input channels", (U32)&INPUT_CHANNELS, RIB_NONE},
    {RIB_ATTRIBUTE, "Output bit rate", (U32)&OUTPUT_BIT_RATE, RIB_NONE},
    {RIB_ATTRIBUTE, "Output sample rate", (U32)&OUTPUT_SAMPLE_RATE, RIB_NONE},
    {RIB_ATTRIBUTE, "Output sample width", (U32)&OUTPUT_BITS, RIB_NONE},
    {RIB_ATTRIBUTE, "Output channels", (U32)&OUTPUT_CHANNELS, RIB_NONE},
    {RIB_ATTRIBUTE, "Position", (U32)&POSITION, RIB_NONE},
    {RIB_ATTRIBUTE, "Percent done", (U32)&PERCENT_DONE, RIB_NONE},
    {RIB_ATTRIBUTE, "Minimum input block size", (U32)&MIN_INPUT_BLOCK_SIZE, RIB_NONE},

    {RIB_PREFERENCE, "Raw source sample rate", (U32)&RAW_RATE, RIB_NONE},
    {RIB_PREFERENCE, "Raw source sample width", (U32)&RAW_BITS, RIB_NONE},
    {RIB_PREFERENCE, "Raw source channels", (U32)&RAW_CHANNELS, RIB_NONE},
    {RIB_PREFERENCE, "Requested bit rate", (U32)&REQUESTED_BIT_RATE, RIB_NONE},
    {RIB_PREFERENCE, "Requested sample rate", (U32)&REQUESTED_RATE, RIB_NONE},
    {RIB_PREFERENCE, "Requested sample width", (U32)&REQUESTED_BITS, RIB_NONE},
    {RIB_PREFERENCE, "Requested channels", (U32)&REQUESTED_CHANS, RIB_NONE},
    {RIB_PREFERENCE, "Layer", (U32)&REQUESTED_LAYER, RIB_NONE},
};

S32 AILCALLBACK MilesMpeg::Stream_CB(U32 user, void *dest, S32 bytesrequested, S32 offset)
{
    MilesMpeg *pMssMpeg = (MilesMpeg *)user;

    long curNBytes = bytesrequested;
    unsigned char *pchDest = (unsigned char *)dest;
    if (offset != -1)
        pMssMpeg->m_sourcePosition = offset;

    if (pMssMpeg->m_sourcePosition + bytesrequested > pMssMpeg->m_sourceLength)
        bytesrequested = pMssMpeg->m_sourceLength - pMssMpeg->m_sourcePosition;

    memcpy(pchDest, pMssMpeg->m_pSource + pMssMpeg->m_sourcePosition, curNBytes);

    pMssMpeg->m_sourcePosition += bytesrequested;

    return bytesrequested;
}

MilesMpeg::MilesMpeg()
{
    m_pFirstFrame = NULL;
    m_pLastFrame = NULL;
    m_nDecodedFrames = 0;
    m_pSource = NULL;
    m_pSource = NULL;
    memset(&m_flags, 0, sizeof(m_flags));
    InitializeCriticalSection(&m_cs);
}

MilesMpeg::~MilesMpeg()
{
    m_pFirstFrame = NULL;
    m_pLastFrame = NULL;
    m_nDecodedFrames = 0;
    DeleteCriticalSection(&m_cs);
}

SNDERR MilesMpeg::init(char *fname)
{
    long req_layer;
    long source_rate;
    long source_bits;
    long source_channels;

    if (m_flags.initialized)
        return SNDERR_ALREADY_LOADED;

    m_pSource = (unsigned char *)AIL_file_read(fname, 0);
    m_sourceLength = AIL_file_size(fname);
    m_sourcePosition = 0;

    m_hASI = RIB_find_file_provider("ASI codec", "Input file types", fname);
    if (m_hASI == 0)
        return SNDERR_MISSING_MSS_REDIST;

    RIB_request(m_hASI, "ASI codec", ASICODEC);
    RIB_request(m_hASI, "ASI stream", ASISTR);

    m_hStream = ASI_stream_open((U32)this, Stream_CB, m_sourceLength);

    req_layer = 3;
    source_rate = 44100;
    source_bits = 16;
    source_channels = 2;

    ASI_stream_set_preference(m_hStream, REQUESTED_LAYER, &req_layer);
    ASI_stream_set_preference(m_hStream, RAW_RATE, &source_rate);
    ASI_stream_set_preference(m_hStream, RAW_BITS, &source_bits);
    ASI_stream_set_preference(m_hStream, RAW_CHANNELS, &source_channels);
    m_flags.initialized = 1;
    return SNDERR_OK;
}

SNDERR MilesMpeg::release()
{
    reset();
    if (m_pSource) {
        AIL_mem_free_lock(m_pSource);
        m_pSource = NULL;
    }
    if (m_hStream) {
        ASI_stream_close(m_hStream);
        m_hStream = 0;
    }
    m_flags.initialized = 0;
    return SNDERR_OK;
}

WaveBuffer *MilesMpeg::getNewBuffer(unsigned int size)
{
    MMIOINFO *pmmioinfo = new MMIOINFO;
    memset(pmmioinfo, 0, sizeof(MMIOINFO));
    pmmioinfo->fccIOProc = FOURCC_MEM;
    pmmioinfo->cchBuffer = size;
    pmmioinfo->adwInfo[0] = 0;
    pmmioinfo->pchBuffer = (char *)malloc(size);
    HMMIO hfile = 0;
    if ((hfile = mmioOpen(NULL, pmmioinfo, MMIO_READWRITE | MMIO_CREATE)) == 0) {
        delete pmmioinfo;
        return NULL;
    }

    gbl_mmio++;
    mmioGetInfo(hfile, pmmioinfo, 0);

    WaveBuffer *pwb = new WaveBuffer;
    pmmioinfo->pchEndRead = pmmioinfo->pchBuffer + size;
    pwb->init(pmmioinfo, &default_wfx);

    delete pmmioinfo;
    return pwb;
}

unsigned int MilesMpeg::get_frames(WaveBuffer **first_frame, WaveBuffer **last_frame)
{
    EnterCriticalSection(&m_cs);
    unsigned int nframes = m_nDecodedFrames;
    *first_frame = m_pFirstFrame;
    *last_frame = m_pLastFrame;
    m_pLastFrame = NULL;
    m_pFirstFrame = NULL;
    m_nDecodedFrames = 0;
    LeaveCriticalSection(&m_cs);
    return nframes;
}

void MilesMpeg::clear_frames()
{
    WaveBuffer *pwb = NULL;
    EnterCriticalSection(&m_cs);
    char *pbuff = NULL;
    while (m_nDecodedFrames) {
        pwb = m_pFirstFrame;
        m_pFirstFrame = pwb->get_plinked_wb();
        m_nDecodedFrames--;
        pbuff = pwb->get_mmioinfo()->pchBuffer;
        mmioClose(pwb->get_mmioinfo()->hmmio, 0);

        free(pbuff);
        gbl_mmio--;

        delete pwb;
    }
    LeaveCriticalSection(&m_cs);
}

unsigned int MilesMpeg::decode_frames(unsigned int nFrames)
{
    WaveBuffer *pwb = NULL;
    unsigned char *buffer = (unsigned char *)AIL_mem_alloc_lock(0x8000);
    long len = 0;
    if (m_flags.stopped || m_flags.end_of_stream)
        return 0;

    for (unsigned int i = 0; i < nFrames; i++) {
        if ((len = ASI_stream_process(m_hStream, buffer, 0x8000)) != 0) {
            if (len < 0x8000) {
                if (m_flags.looping)
                    ASI_stream_seek(m_hStream, 0);
                else
                    m_flags.end_of_stream = 1;
            }

            pwb = getNewBuffer(len);
            memcpy(pwb->get_mmioinfo()->pchBuffer, buffer, len);
            EnterCriticalSection(&m_cs);
            if (m_nDecodedFrames) {
                m_pLastFrame->set_plinked_wb(pwb);
                m_pLastFrame = pwb;
            } else {
                m_pLastFrame = pwb;
                m_pFirstFrame = pwb;
            }
            m_nDecodedFrames++;
            LeaveCriticalSection(&m_cs);
        }
    }
    AIL_mem_free_lock(buffer);
    return i;
}

SNDERR MilesMpeg::reset()
{
    m_flags.stopped = 1;
    clear_frames();
    m_sourcePosition = 0;
    m_flags.end_of_stream = 0;
    return SNDERR_OK;
}

SNDERR MilesMpeg::stop()
{
    reset();
    return SNDERR_OK;
}

SNDERR MilesMpeg::play()
{
    if (!m_flags.initialized)
        return SNDERR_UNINITIALIZED;

    if (m_sourcePosition >= m_sourceLength)
        m_sourcePosition = 0;

    m_flags.stopped = 0;
    return SNDERR_OK;
}

#endif
