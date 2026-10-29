#ifndef SOUNDLIST_H
#define SOUNDLIST_H

class dllSoundList : public SoundList {
    CWaveMgr *m_pCurSound;
    CWaveMgr *m_pQueuedSound;

    unsigned int m_curPlaying;
    unsigned int m_curQueued;
    int m_curVolume;

public:
    dllSoundList();
    virtual SNDERR queueSound(unsigned iPlay);
    virtual SNDERR play();
    virtual SNDERR stop();
    virtual HWND get_game_hwnd() { return CDSMgr::get_game_hwnd(); }
    virtual SNDERR set_attrib(unsigned long attrib) { return SNDERR_NO_SUPPORT; }
    virtual unsigned long get_attrib() { return 0; }
    virtual bool isAlreadyQueued()
    {
        if (m_curQueued)
            return true;
        return false;
    }
    virtual int is_playing()
    {
        if (m_pCurSound)
            return m_pCurSound->is_playing();
        return 0;
    }
    virtual void ramp(int dst_vol, int src_vol, unsigned int ms_time);
    virtual void set_volume(int vol);
    virtual void set_scaled_volume(int vol);
    CWaveMgr *getCurSound() { return m_pCurSound; }
    CWaveMgr *getQueuedSound() { return m_pQueuedSound; }

    virtual SNDERR fade();
};

#endif
