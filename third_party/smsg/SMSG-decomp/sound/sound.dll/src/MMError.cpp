#include "MmResultTranslator.h"

CMMError::CMMError()
{
}

CMMError::~CMMError()
{
}

SNDERR CMMError::handle_error(MMRESULT mmerror)
{
    switch (mmerror) {
    case MMSYSERR_NOERROR:
        return SNDERR_OK;
    case MMSYSERR_NOTENABLED:
    case MMSYSERR_NODRIVER:
        return SNDERR_NO_DRIVER;
    case MMSYSERR_NOMEM:
        return SNDERR_NO_MEM;
    case MMSYSERR_NOTSUPPORTED:
        return SNDERR_NO_SUPPORT;
    case MMSYSERR_ALLOCATED:
    case MMSYSERR_HANDLEBUSY:
        return SNDERR_ALLOCATED;
    case MMSYSERR_BADDEVICEID:
    case MMSYSERR_INVALHANDLE:
    case MMSYSERR_INVALFLAG:
    case MMSYSERR_INVALPARAM:
    case MMSYSERR_INVALIDALIAS:
        return SNDERR_BAD_PARAM;
    case 0x200:
        return SNDERR_NOCANDO;
    case MMSYSERR_ERROR:
    case MMSYSERR_BADERRNUM:
    case MMSYSERR_BADDB:
    case MMSYSERR_KEYNOTFOUND:
    case MMSYSERR_READERROR:
    case MMSYSERR_WRITEERROR:
    case MMSYSERR_DELETEERROR:
    case MMSYSERR_VALNOTFOUND:
    case MMSYSERR_NODRIVERCB:
        return SNDERR_UNDEFINED;
    }
    return SNDERR_UNDEFINED;
}
