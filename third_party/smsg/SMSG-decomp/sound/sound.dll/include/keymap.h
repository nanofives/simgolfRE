#ifndef KEYMAP_H
#define KEYMAP_H

#include "sound.h"

class Dll_KeyMap {
    char *m_pszdesc;
    CWaveMgr *m_wave_table[128];

    unsigned long m_keycycle_hi;
    unsigned long m_keycycle_lo;
    unsigned long m_curkey;

    unsigned long m_refNum;

public:
    KMFLAGS_tag m_flags;
    unsigned long m_release_time;

    Dll_KeyMap();
    ~Dll_KeyMap();
    unsigned long get_next_key();
    SNDERR set_desc(const char *pdesc);
    const char *get_desc() { return m_pszdesc; }
    SNDERR map_range(KeyMapWave *pkmw);
    SNDERR unmap_range(CWaveMgr *pwmgr);
    CWaveMgr *get_key_wave(unsigned long key) { return m_wave_table[key & 0x7f]; }
    void set_cycle(unsigned long keylo, unsigned long keyhi);
    int is_cycling() { return m_flags.cycle; }
    void reset();
    void set_base_path(const char *p);
    void set_release_time(unsigned long release_time);
    SNDERR stop_all();

    int inc_ref() { return ++m_refNum; }
    int get_ref() { return m_refNum; }
    int dec_ref() { return --m_refNum; }
    bool activeWaves();
};

#endif
