#include <string.h>

#pragma intrinsic(strcpy, strcat, strcmp, strlen)

#include "MidiSequence.h"
#include "sound.h"
#include "wavemgr.h"

Dll_KeyMap::Dll_KeyMap()
{
    int i;

    m_pszdesc = 0;
    for (i = 0; i < 128; i++)
        m_wave_table[i] = 0;

    memset(&m_flags, 0, sizeof(m_flags));
    m_keycycle_hi = 0;
    m_keycycle_lo = 0;
    m_curkey = 0;
    m_refNum = 0;
    m_release_time = 0xfa;
}

Dll_KeyMap::~Dll_KeyMap()
{
    for (int i = 0; i < 0x80; ++i) {
        CWaveMgr *pcurwmgr = m_wave_table[i];
        if (pcurwmgr != 0) {
            do {
                m_wave_table[i] = 0;
                ++i;
            } while (pcurwmgr == m_wave_table[i]);
            --i;
            delete pcurwmgr;
        }
    }
    if (m_pszdesc != 0)
        ::operator delete(m_pszdesc);
    m_pszdesc = 0;
}

SNDERR Dll_KeyMap::stop_all()
{
    for (int i = 0; i < 0x80; i++) {
        if (m_wave_table[i]) {
            if (m_wave_table[i]->m_nvoices) {
                for (int j = 0; j < m_wave_table[i]->m_nvoices - 1; j++) {
                    if (gbl_pwave_device->pull_from_play_list(m_wave_table[i]->m_voices[j]) == SNDERR_OK) {
                        if (m_wave_table[i]->m_voices[j]->is_static())
                            m_wave_table[i]->m_voices[j]->reset();
                        else
                            gbl_pwave_device->post_GT_request(m_wave_table[i]->m_voices[j], REQUEST_RESET);
                    }
                }
            }
            if (gbl_pwave_device->pull_from_play_list(m_wave_table[i]) == SNDERR_OK) {
                if (m_wave_table[i]->is_static())
                    m_wave_table[i]->reset();
                else
                    gbl_pwave_device->post_GT_request(m_wave_table[i], REQUEST_RESET);
            }
        }
    }
    return SNDERR_OK;
}

bool Dll_KeyMap::activeWaves()
{
    for (int i = 0; i < 128; ++i) {
        if (m_wave_table[i] != 0) {
            if (m_wave_table[i]->m_nvoices != 0) {
                for (int j = 0; j < m_wave_table[i]->m_nvoices - 1; ++j) {
                    if (m_wave_table[i]->m_voices[j] != 0 && m_wave_table[i]->m_voices[j]->is_playing())
                        return true;
                }
            }
            if (m_wave_table[i]->is_playing())
                return true;
        }
    }
    return false;
}

unsigned long Dll_KeyMap::get_next_key()
{
    unsigned long curkey = m_curkey++;
    if (m_curkey > m_keycycle_hi)
        m_curkey = m_keycycle_lo;
    return curkey;
}

void Dll_KeyMap::set_cycle(unsigned long keylo, unsigned long keyhi)
{
    if (keylo > keyhi) {
        unsigned long tmp = keyhi;
        keyhi = keylo;
        keylo = tmp;
    }

    m_keycycle_lo = keylo;
    m_flags.cycle = 1;
    m_keycycle_hi = keyhi;
    m_curkey = keylo;
}

void Dll_KeyMap::set_release_time(unsigned long release_time)
{
    CWaveMgr *prev_wmgr = 0;
    for (unsigned long i = 0; i < 128; i++) {
        if (m_wave_table[i] != 0 && m_wave_table[i] != prev_wmgr) {
            m_wave_table[i]->set_fade(release_time);
            for (int j = 0; j < m_wave_table[i]->m_nvoices - 1; j++)
                m_wave_table[i]->m_voices[j]->set_fade(release_time);
            prev_wmgr = m_wave_table[i];
        }
    }
    m_release_time = release_time;
}

SNDERR Dll_KeyMap::map_range(KeyMapWave *pkmw)
{
    unsigned int i;
    unsigned long keylo = pkmw->get_keylo();
    unsigned long keyhi = pkmw->get_keyhi();

    CWaveMgr *pwmgr = new CWaveMgr(sound_win.get_hwnd());
    pwmgr->set_amb_sound();
    SNDERR retval = pwmgr->load(pkmw->get_fname());
    if (retval != 0) {
        delete pwmgr;
        return retval;
    }
    pwmgr->set_fade(m_release_time);

    for (i = 1; i < pkmw->get_nvoices(); i++) {
        pwmgr->m_voices[i - 1] = new CWaveMgr(sound_win.get_hwnd());
        pwmgr->m_voices[i - 1]->load(pkmw->get_fname());
        pwmgr->m_voices[i - 1]->set_fade(m_release_time);
    }
    pwmgr->m_nvoices = pkmw->get_nvoices();

    for (i = keylo; i <= keyhi; i++) {
        if (m_wave_table[i] != 0)
            i = unmap_range(m_wave_table[i]);
    }
    for (i = keylo; i <= keyhi; i++)
        m_wave_table[i] = pwmgr;
    return SNDERR_OK;
}

SNDERR Dll_KeyMap::unmap_range(CWaveMgr *pwmgr)
{
    int i;
    for (i = 0; i < 0x80; i++)
        if (m_wave_table[i] == pwmgr)
            m_wave_table[i] = 0;

    for (i = 0; pwmgr->m_voices[i] != 0 && i < 3; i++) {
        delete pwmgr->m_voices[i];
        pwmgr->m_voices[i] = 0;
    }
    delete pwmgr;
    return SNDERR_OK;
}

SNDERR Dll_KeyMap::set_desc(const char *pdesc)
{
    if (pdesc == 0)
        return SNDERR_BAD_PARAM;
    if (m_pszdesc != 0)
        ::operator delete(m_pszdesc);
    m_pszdesc = (char *)::operator new(strlen(pdesc) + 1);
    strcpy(m_pszdesc, pdesc);
    return SNDERR_OK;
}

void Dll_KeyMap::set_base_path(const char *p)
{
    CWaveMgr *last_pw;
    int i;
    char newfname[256];
    char buff[256];
    int plen;

    strcpy(buff, p);
    last_pw = 0;

    plen = strlen(buff);
    if (buff[plen - 1] != '\\') {
        buff[plen] = '\\';
        buff[plen + 1] = '\0';
    }

    for (i = 0; i < 128; i++) {
        CWaveMgr *pw = m_wave_table[i];
        const char *oldfname;
        const char *tmpname;

        if (pw == 0)
            continue;
        if (pw == last_pw)
            continue;
        if (pw->is_loaded() != 0)
            continue;
        if (strstr(p, "C:\\") != 0)
            continue;

        oldfname = pw->CWaveFile::get_fname();
        last_pw = pw;
        if (oldfname == 0)
            continue;

        strcpy(newfname, buff);
        tmpname = strrchr(oldfname, '\\');
        if (tmpname != 0)
            tmpname = tmpname + 1;
        else
            tmpname = oldfname;
        strcat(newfname, tmpname);

        if (pw->is_loaded() != 0) {
            if (strcmp(oldfname, newfname) == 0)
                continue;
            pw->unload();
        }
        pw->load(newfname);
    }
}
