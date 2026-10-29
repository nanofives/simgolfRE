#include "sound.h"
#include "sounddevice.h"
#include "soundlist.h"
#include "wavefile.h"

dllSoundList::dllSoundList()
{
    m_pCurSound = 0;
    m_pQueuedSound = 0;
    m_curPlaying = 0;
    m_curQueued = 0;
    m_curVolume = 0x7f;
    set_type(SOUNDTYPE_SOUNDLIST);
}

SNDERR dllSoundList::queueSound(unsigned iPlay)
{
    if (m_pQueuedSound)
        return SNDERR_ALREADY_LOADED;
    if (iPlay >= 40)
        return SNDERR_BAD_PARAM;
    if (!getSoundName(iPlay))
        return SNDERR_NOT_PRESENT;
    unsigned long flags = 0x102;
    if (m_pCurSound)
        flags = 0x302;
    CWaveMgr *pTemp = new CWaveMgr;
    pTemp->set_attrib(flags);
    SNDERR retVal = pTemp->load(getSoundName(iPlay));
    if (retVal) {
        delete pTemp;
    } else {
        if (pTemp->is_mpeg())
            pTemp->mpeg_play();
        m_pQueuedSound = pTemp;
        if (m_pCurSound)
            m_pCurSound->queueSound(&m_pQueuedSound, &m_pCurSound);
    }
    return retVal;
}

SNDERR dllSoundList::play()
{
    if (m_pQueuedSound == 0 && m_pCurSound == 0)
        return SNDERR_NOT_LOADED;
    if (is_playing())
        return SNDERR_OK;
    if (m_pCurSound != 0 && m_pCurSound->is_playing() == 0) {
        m_pCurSound->set_loop_state(1);
        m_pCurSound->set_volume(m_curVolume);
        return m_pCurSound->play();
    }
    m_pCurSound = m_pQueuedSound;
    m_pQueuedSound = 0;
    m_pCurSound->set_loop_state(1);
    m_pCurSound->set_volume(m_curVolume);
    return m_pCurSound->play();
}

SNDERR dllSoundList::stop()
{
    if (m_pCurSound)
        m_pCurSound->stop();
    return SNDERR_NO_SUPPORT;
}

void dllSoundList::ramp(int dst_vol, int src_vol, unsigned int ms_time)
{
    if (gbl_pwave_device) {
        CCriticalRequest *pcReq = new CCriticalRequest(REQUEST_SET_SOUNDLIST_RAMP, this);
        pcReq->m_data1 = dst_vol;
        pcReq->m_data2 = ms_time;
        gbl_pwave_device->queue_request(pcReq);
    }
}

SNDERR dllSoundList::fade()
{
    if (!gbl_pwave_device)
        return SNDERR_NO_DRIVER;
    if (m_pCurSound)
        m_pCurSound->setppCurSound(&m_pCurSound);
    CCriticalRequest *pcReq = new CCriticalRequest(REQUEST_SOUNDLIST_FADE, this);
    gbl_pwave_device->queue_request(pcReq);
    return SNDERR_OK;
}

void dllSoundList::set_volume(int vol)
{
    Patch::set_volume(vol);
    if (gbl_pwave_device) {
        CCriticalRequest *pcReq = new CCriticalRequest(REQUEST_SET_SOUNDLIST_VOL, this);
        pcReq->m_data1 = vol;
        gbl_pwave_device->queue_request(pcReq);
    }
}

void dllSoundList::set_scaled_volume(int vol)
{
    m_curVolume = vol;
    if (gbl_pwave_device) {
        CCriticalRequest *pcReq = new CCriticalRequest(REQUEST_SET_SOUNDLIST_VOL, this);
        pcReq->m_data1 = vol;
        gbl_pwave_device->queue_request(pcReq);
    }
}
