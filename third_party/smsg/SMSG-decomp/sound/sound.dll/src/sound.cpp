#include "sounddevice.h"
#include <string.h>
#pragma intrinsic(strcpy, strcat, strlen)
#include "MidiSequence.h"
#include "sound.h"
extern "C" {
    Sound *Sound::gbl_sound_tail = 0;
    Sound *Sound::gbl_sound_head = 0;
    static int s_spare[2] = {0};
    int g_snd_system = 0;
}

Sound::Sound()
{
    m_pprev = 0;
    m_pnext = 0;
    m_pdllsnd = 0;
    m_fname = 0;
    memset(&m_sf, 0, sizeof(m_sf));
    m_sf.loaded = 0;
    set_fade(1000);
    m_type = SOUNDTYPE_UNKNOWN;
}

Sound::~Sound()
{
    if (m_fname) {
        delete m_fname;
        m_fname = 0;
    }
    if (m_pdllsnd) {
        delete_sound(m_pdllsnd);
        m_pdllsnd = 0;
    }
    detach();
}


void Sound::set_type(SOUNDTYPE type)
{
    switch (type) {
    case SOUNDTYPE_AVI:
        m_sf.avi = 1;
        m_type = type;
        return;
    case SOUNDTYPE_MIDI:
        m_sf.midi = 1;
        m_type = type;
        return;
    case SOUNDTYPE_WAVE:
        m_sf.wave = 1;
        m_type = type;
        return;
    case SOUNDTYPE_AMBIENCE:
        m_sf.ambience = 1;
        m_sf.midi = 1;
        m_type = type;
        return;
    case SOUNDTYPE_VOICERX:
        m_sf.vrx = 1;
        m_type = type;
        return;
    case SOUNDTYPE_VOICETX:
        m_sf.vtx = 1;
        m_type = type;
        return;
    }
    m_type = type;
}


SNDERR Sound::load(const char *fname)
{
    SNDERR retval;
    char *tmpname;
    char *p = (char *)fname;

    if (!g_snd_system)
        return SNDERR_NO_DRIVER;

    if (!m_pdllsnd) {
        retval = create_sound(&m_pdllsnd, p, SOUNDTYPE_WAVE);
        if (retval != SNDERR_OK)
            return retval;
    } else if (m_pdllsnd->is_loaded()) {
        return SNDERR_ALREADY_LOADED;
    }

    retval = m_pdllsnd->load(p);
    if (retval == SNDERR_OK) {
        if (!m_sf.loaded) {
            m_sf.loaded = 1;
            attach();
            if (get_loop_state())
                m_pdllsnd->set_loop_state(1);
        }
    } else {
        m_sf.loaded = 0;
    }

    tmpname = new char[strlen(p) + 1];
    strcpy(tmpname, p);
    if (m_fname)
        delete m_fname;
    m_fname = tmpname;
    return retval;
}

SNDERR Sound::attach(Sound *psound)
{
    SNDERR retval;

    if (psound == 0)
        return SNDERR_BAD_PARAM;
    retval = SNDERR_OK;
    if (psound->m_pnext == 0 && psound->m_pprev == 0) {
        if (gbl_sound_tail == 0) {
            gbl_sound_head = psound;
            gbl_sound_tail = psound;
            psound->m_pnext = 0;
            psound->m_pprev = 0;
            psound->m_sf.attached = 1;
            psound->m_sf.directAttach = 1;
        } else {
            psound->m_pprev = gbl_sound_tail;
            gbl_sound_tail = psound;
            psound->m_pprev->m_pnext = psound;
            psound->m_pnext = 0;
            psound->m_sf.attached = 1;
            psound->m_sf.directAttach = 1;
        }
    }
    return retval;
}

SNDERR Sound::detach(Sound *psound)
{
    if (psound == 0)
        return SNDERR_BAD_PARAM;
    if (!psound->m_sf.attached)
        return SNDERR_OK;

    SNDERR retval = SNDERR_OK;

    if (psound->m_pprev != 0)
        psound->m_pprev->m_pnext = psound->m_pnext;
    else
        gbl_sound_head = psound->m_pnext;

    if (psound->m_pnext != 0)
        psound->m_pnext->m_pprev = psound->m_pprev;
    else
        gbl_sound_tail = psound->m_pprev;

    psound->m_pnext = 0;
    psound->m_sf.attached = 0;
    psound->m_sf.directAttach = 0;
    psound->m_pprev = 0;
    return retval;
}

SNDERR Sound::attach()
{
    SNDERR retval = SNDERR_OK;

    if (m_pnext == 0 && m_pprev == 0) {
        if (gbl_sound_tail == 0) {
            gbl_sound_head = this;
            gbl_sound_tail = this;
            m_pnext = 0;
            m_pprev = 0;
            m_sf.attached = 1;
        } else {
            m_pprev = gbl_sound_tail;
            gbl_sound_tail = this;
            m_pprev->m_pnext = this;
            m_pnext = 0;
            m_sf.attached = 1;
        }
    }
    return retval;
}

SNDERR Sound::detach()
{
    if (!m_sf.attached)
        return SNDERR_OK;

    SNDERR retval = SNDERR_OK;
    if (m_pprev != 0)
        m_pprev->m_pnext = m_pnext;
    else
        gbl_sound_head = m_pnext;

    if (m_pnext != 0)
        m_pnext->m_pprev = m_pprev;
    else
        gbl_sound_tail = m_pprev;

    m_pnext = 0;
    m_pprev = 0;
    m_sf.attached = 0;
    return retval;
}

SNDERR Sound::load(IAVIStream *paviaudio)
{
    SNDERR retval = SNDERR_NO_SUPPORT;
    return retval;
}

SNDERR Sound::unload()
{
    SNDERR retval = SNDERR_OK;

    if (m_pdllsnd)
        retval = m_pdllsnd->unload();

    detach();
    m_pdllsnd = 0;
    m_sf.loaded = 0;
    return retval;
}

SNDERR Sound::play()
{
    SNDERR retval = SNDERR_NOT_LOADED;
    if (m_pdllsnd)
        retval = m_pdllsnd->play();
    return retval;
}

SNDERR Sound::play(unsigned long num)
{
    SNDERR retval = SNDERR_NOT_LOADED;
    if (m_pdllsnd)
        retval = m_pdllsnd->play(num);
    return retval;
}

int Sound::is_playing()
{
    if (m_pdllsnd)
        return m_pdllsnd->is_playing();
    return 0;
}

SNDERR Sound::stop()
{
    SNDERR retval = SNDERR_NOT_LOADED;
    if (m_pdllsnd)
        retval = m_pdllsnd->stop();
    return retval;
}

SNDERR Sound::release()
{
    SNDERR retval = SNDERR_NOT_LOADED;
    if (m_pdllsnd)
        retval = m_pdllsnd->release();
    return retval;
}

void Sound::set_volume(int vol)
{
    vol &= 0x7f;
    Patch::set_volume(vol);
    Patch::set_volume(vol);
    if (m_pdllsnd)
        m_pdllsnd->set_volume(vol);
}

void Sound::set_loop_state(int state)
{
    Patch::set_loop_state(state);
    if (m_pdllsnd)
        m_pdllsnd->set_loop_state(state);
}

void Sound::set_delay(unsigned long delay)
{
    Patch::set_delay(delay);
    if (m_pdllsnd)
        m_pdllsnd->set_delay(delay);
}
SNDERR Sound::set_fade(unsigned long ms_fade)
{
    if (ms_fade == 0)
        return SNDERR_BAD_PARAM;

    m_fade_amount = ms_fade;
    if (m_pdllsnd)
        m_pdllsnd->set_fade(ms_fade);
    return SNDERR_OK;
}

SNDERR Sound::set_fade_in(unsigned int ms_fade)
{
    if (ms_fade == 0)
        return SNDERR_BAD_PARAM;

    Patch::set_fade_in(ms_fade);
    if (m_pdllsnd)
        m_pdllsnd->set_fade_in(ms_fade);
    return SNDERR_OK;
}

SNDERR Sound::fade()
{
    if (get_fade_amount() && m_pdllsnd)
        return m_pdllsnd->fade();
    return SNDERR_UNINITIALIZED;
}

SNDERR Sound::fade(unsigned long ms_fade)
{
    SNDERR retval = set_fade(ms_fade);
    if (retval == SNDERR_OK)
        return fade();
    return retval;
}

SNDERR Sound::fade_in()
{
    if (get_fade_amount() && m_pdllsnd)
        return m_pdllsnd->fade_in();
    return SNDERR_UNINITIALIZED;
}

void Sound::ramp(int dst_vol, int src_vol, unsigned int ms_time)
{
    if (m_pdllsnd)
        m_pdllsnd->ramp(dst_vol, src_vol, ms_time);
}

SNDERR Sound::fade_in(unsigned int ms_fade)
{
    SNDERR retval = set_fade_in(ms_fade);
    if (retval == SNDERR_OK)
        return fade();
    return retval;
}

int Sound::is_looping()
{
    if (m_pdllsnd)
        return m_pdllsnd->is_looping();
    return 0;
}

unsigned long Sound::get_time()
{
    if (m_pdllsnd)
        return m_pdllsnd->get_time();
    return 0;
}

void Sound::set_pan(int pan)
{
    if (pan < -64)
        pan = -64;
    else if (pan > 63)
        pan = 63;

    Patch::set_pan(pan);
    if (m_pdllsnd)
        m_pdllsnd->set_pan(pan);
}

Wave::Wave()
{
    memset(&m_stateBits, 0, sizeof(m_stateBits));
    m_sf.wave = 1;
    set_type(SOUNDTYPE_WAVE);
    m_pitch = 0;
    m_reverb_mix = 1.0f;
    m_group = 0x10;
    m_ms_length = 0;
    m_play_time = 0;
}

Wave::~Wave()
{
    if (m_fname)
        delete m_fname;
    m_fname = 0;
    Sound::detach();
}

SNDERR Wave::play(int ibuffer)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->play(ibuffer);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave::play()
{
    SNDERR retval = SNDERR_OK;

    if (m_pdllsnd) {
        retval = m_pdllsnd->play();
    } else if (m_stateBits & 0x10) {
        Wave::load();
        if (m_pdllsnd)
            retval = m_pdllsnd->play();
    } else {
        retval = SNDERR_NOT_LOADED;
    }

    if ((m_stateBits & 0x10) != 0) {
        m_play_time = timeGetTime();
        detach();
        m_pdllsnd = 0;
    }
    return retval;
}



SNDERR Wave::play(int *pindex)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->play(pindex);
    return SNDERR_NOT_LOADED;
}

void Wave::stop_instance(int ibuffer)
{
    if (m_pdllsnd)
        ((Wave *)m_pdllsnd)->stop_instance(ibuffer);
}

void Wave::fade_instance(int ibuffer)
{
    if (m_pdllsnd)
        ((Wave *)m_pdllsnd)->fade_instance(ibuffer);
}

void Wave::init(char *fname, unsigned long flags)
{
    if ((flags & 4) && (flags & 0x80))
        return;

    char *p = fname;

    if (p) {
        if (m_fname)
            delete m_fname;
        m_fname = new char[strlen(p) + 1];
        strcpy(m_fname, p);
    }

    memset(&m_stateBits, 0, sizeof(m_stateBits));

    if (flags & 4) {
        m_stateBits |= 2;
        if (!m_pdllsnd) {
            if (create_sound(&m_pdllsnd, p, SOUNDTYPE_WAVE) == SNDERR_OK) {
                m_pdllsnd->set_attrib(get_attrib());
                attach();
            }
        }
        if (m_pdllsnd)
            m_pdllsnd->set_attrib(flags);
    }

    if (flags & 1)
        m_stateBits |= 1;
    if ((flags & 0x10) && !(flags & 4))
        m_stateBits |= 4;
    if (flags & 2)
        set_loop_state(1);
    if (flags & 0x40)
        m_stateBits |= 8;
    if (flags & 0x80)
        m_stateBits |= 0x10;
    if (flags & 0x100)
        m_stateBits |= 0x20;
    if (flags & 0x400)
        m_stateBits |= 0x40;
}


int Wave::is_hwbuffer()
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->is_hwbuffer();
    return 0;
}


unsigned int Wave::get_ms_length()
{
    return m_ms_length;
}

int Wave::is_playing()
{
    if (!m_pdllsnd && is_discard()) {
        if (m_play_time && m_ms_length > timeGetTime() - m_play_time)
            return 1;
    } else if (m_pdllsnd) {
        return m_pdllsnd->is_playing();
    }
    return 0;
}

SNDERR Wave::set_fname(const char *fname)
{
    if (fname) {
        if (m_fname)
            delete m_fname;
        m_fname = new char[strlen(fname) + 1];
        strcpy(m_fname, fname);
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR Wave::set_link(char *fname, unsigned long *psemaphore)
{
    if (m_pdllsnd == 0)
        return SNDERR_UNINITIALIZED;

    ((Wave *)m_pdllsnd)->set_link(fname, psemaphore);
    return SNDERR_OK;
}

SNDERR Wave::dyna_load(char *fname)
{
    SNDERR retval;

    if (!m_pdllsnd) {
        retval = create_sound(&m_pdllsnd, fname, SOUNDTYPE_WAVE);
        if (retval == SNDERR_OK) {
            m_pdllsnd->set_attrib(get_attrib());
            attach();
        }
    } else {
        retval = SNDERR_ALREADY_OPEN;
    }
    return retval;
}

SNDERR Wave::load(const char *fname)
{
    unsigned long attrib = 0;
    SNDERR rval;

    if (!m_pdllsnd) {
        rval = create_sound(&m_pdllsnd, fname, SOUNDTYPE_WAVE);
        if (rval != SNDERR_OK)
            return rval;
    }

    if (m_stateBits & 1)
        attrib = 1;
    if (is_looping())
        attrib |= 2;
    if (m_stateBits & 8)
        attrib |= 0x40;
    if (m_stateBits & 0x10)
        attrib |= 0x80;
    if (m_stateBits & 0x40)
        attrib |= 0x400;
    if (m_stateBits & 2)
        attrib |= 4;
    m_pdllsnd->set_attrib(attrib);

    if (!(m_stateBits & 2)) {
        rval = Sound::load(fname);
        if (rval != SNDERR_OK) {
            delete_sound(m_pdllsnd);
            m_pdllsnd = 0;
            return rval;
        }
        m_ms_length = ((Wave *)m_pdllsnd)->get_ms_length();
        m_pdllsnd->set_volume(get_volume());
        ((Wave *)m_pdllsnd)->set_pitch(get_pitch());
        m_pdllsnd->set_pan(get_pan());
    }
    return SNDERR_OK;
}

SNDERR Wave::load()
{
    unsigned long attrib = 0;
    SNDERR rval;
    char *fname = get_fname();
    if (fname) {
        if (!m_pdllsnd) {
            rval = create_sound(&m_pdllsnd, fname, SOUNDTYPE_WAVE);
            if (rval != SNDERR_OK)
                return rval;
        }
        if (m_stateBits & 1)
            attrib = 1;
        if (is_looping())
            attrib |= 2;
        if (m_stateBits & 4)
            attrib |= 0x11;
        if (m_stateBits & 8) {
            attrib |= 0x40;
            if (!(m_stateBits & 0x20))
                attrib |= 1;
        }
        if (m_stateBits & 0x10)
            attrib |= 0x80;
        if (m_stateBits & 0x20)
            attrib |= 0x100;
        if (m_stateBits & 0x40)
            attrib |= 0x400;
        m_pdllsnd->set_attrib(attrib);
        rval = Sound::load(fname);
        if (rval != SNDERR_OK) {
            delete_sound(m_pdllsnd);
            m_pdllsnd = 0;
            return rval;
        }
        m_ms_length = ((Wave *)m_pdllsnd)->get_ms_length();
    } else {
        rval = SNDERR_BAD_FILE_OPEN;
    }
    return rval;
}

SNDERR Wave::load(char *fname, unsigned long flags)
{
    SNDERR ret_val = SNDERR_OK;

    init(fname, flags);
    if (flags & 4)
        return ret_val;
    return load();
}

SNDERR Wave::reload()
{
    SNDERR rval = SNDERR_OK;
    unsigned long attrib = 0;
    char *fname = get_fname();

    if (fname) {
        if (!m_pdllsnd) {
            rval = create_sound(&m_pdllsnd, fname, SOUNDTYPE_WAVE);
            if (rval != SNDERR_OK)
                return rval;
        }
        if (m_stateBits & 1)
            attrib = 1;
        if (is_looping())
            attrib |= 2;
        m_pdllsnd->set_attrib(attrib);
        rval = ((Wave *)m_pdllsnd)->reload();
        if (rval == SNDERR_OK && !m_sf.loaded) {
            m_sf.loaded = 1;
            attach();
            if (get_loop_state())
                m_pdllsnd->set_loop_state(1);
        }
    } else {
        rval = SNDERR_BAD_FILE_OPEN;
    }
    return rval;
}

SNDERR Wave::unload()
{
    SNDERR retval = SNDERR_OK;

    if (m_pdllsnd)
        retval = m_pdllsnd->unload();
    m_pdllsnd = 0;
    if (!is_dynamic_buffer())
        detach();
    m_sf.loaded = 0;
    return retval;
}

SNDERR Wave::set_pitch(int pitch)
{
    if (pitch < -1200)
        pitch = -1200;
    else if (pitch > 1200)
        pitch = 1200;
    m_pitch = pitch;
    if (m_pdllsnd)
        ((Wave *)m_pdllsnd)->set_pitch(pitch);
    return SNDERR_OK;
}

SNDERR Wave::set_attrib(unsigned long attrib)
{
    if (attrib & 0x02)
        Patch::set_loop_state(1);
    if (attrib & 0x01)
        m_stateBits |= 1;
    if (attrib & 0x04)
        m_stateBits |= 2;
    if (attrib & 0x40)
        m_stateBits |= 8;
    if (attrib & 0x80)
        m_stateBits |= 0x10;
    if (!(attrib & 4) && (attrib & 0x10))
        m_stateBits |= 4;
    if (!(attrib & 4) && (attrib & 0x100))
        m_stateBits |= 0x20;
    if (attrib & 0x400)
        m_stateBits |= 0x40;
    if (m_pdllsnd)
        m_pdllsnd->set_attrib(attrib);
    return SNDERR_OK;
}

unsigned long Wave::get_attrib()
{
    unsigned long attrib = 0;
    if (m_pdllsnd)
        attrib = m_pdllsnd->get_attrib();
    if (get_loop_state())
        attrib |= 2;
    if (m_stateBits & 0x01)
        attrib |= 0x01;
    if (m_stateBits & 0x08)
        attrib |= 0x40;
    if (m_stateBits & 0x02)
        attrib |= 0x04;
    if (m_stateBits & 0x04)
        attrib |= 0x10;
    if (m_stateBits & 0x10)
        attrib |= 0x80;
    if (m_stateBits & 0x20)
        attrib |= 0x100;
    return attrib;
}

unsigned long Wave::get_time(unsigned long instance)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->get_time(instance);
    return 0;
}

unsigned long Wave::get_current_marker()
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->get_current_marker();
    return -1;
}



HWND Wave::get_game_hwnd()
{
    if (m_pdllsnd)
        return m_pdllsnd->get_game_hwnd();
    return 0;
}

int Wave::get_ndevices()
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->get_ndevices();
    return 0;
}

SNDERR Wave::get_device_description(char *desc, int device_id, int buffsize)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->get_device_description(desc, device_id, buffsize);

    if (buffsize)
        *desc = 0;
    return SNDERR_NO_DRIVER;
}

SNDERR Wave::select_device(int id)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->select_device(id);
    return SNDERR_NO_DRIVER;
}

SNDERR Wave::set_reverb_mix(float mix)
{
    m_reverb_mix = mix;
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->set_reverb_mix(mix);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave::set_bufflimit(unsigned long buffsize)
{
    SNDERR retval = SNDERR_NO_SUPPORT;
    return retval;
}
SNDERR Wave::set_asdr()
{
    SNDERR retval = SNDERR_NO_SUPPORT;
    return retval;
}
SNDERR Wave::set_attack(unsigned long mstime, unsigned long level1, unsigned long level2)
{
    SNDERR retval = SNDERR_NO_SUPPORT;
    return retval;
}
SNDERR Wave::set_sustain(unsigned long mstime, unsigned long level1, unsigned long level2)
{
    SNDERR retval = SNDERR_NO_SUPPORT;
    return retval;
}
SNDERR Wave::set_decay(unsigned long mstime, unsigned long level1, unsigned long level2)
{
    SNDERR retval = SNDERR_NO_SUPPORT;
    return retval;
}
SNDERR Wave::set_release(unsigned long mstime, unsigned long level1, unsigned long level2)
{
    SNDERR retval = SNDERR_NO_SUPPORT;
    return retval;
}



void Wave::set_volume(int volume)
{
    Patch::set_volume(volume);
}

SNDERR Wave::set_position3d(float x, float y, float z)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->set_position3d(x, y, z);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave::set_xpos(float x)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->set_xpos(x);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave::set_ypos(float y)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->set_ypos(y);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave::set_zpos(float z)
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->set_zpos(z);
    return SNDERR_NOT_LOADED;
}

bool Wave::is_3d()
{
    if (m_pdllsnd)
        return ((Wave *)m_pdllsnd)->is_3d();
    return 0;
}

MpegStream::MpegStream()
{
    for (int i = 0; i < 40; i++)
        m_mpeg_list[i] = 0;
    m_link_semaphore = 0;
}

MpegStream::~MpegStream()
{
    for (int i = 0; i < 40; i++) {
        if (m_mpeg_list[i]) {
            delete m_mpeg_list[i];
            m_mpeg_list[i] = 0;
        }
    }
}

SNDERR MpegStream::set_next(unsigned int play_index)
{
    m_link_semaphore = 0;
    if (m_pdllsnd)
        ((Wave *)m_pdllsnd)->set_link(m_mpeg_list[play_index], &m_link_semaphore);
    return SNDERR_OK;
}

SNDERR MpegStream::start(unsigned int play_index)
{
    init(m_mpeg_list[play_index], 0x102);
    SNDERR retval = load();
    if (retval != SNDERR_OK) {
        if (retval != SNDERR_ALREADY_LOADED)
            return retval;
        if (m_pdllsnd)
            m_pdllsnd->unload();
        m_pdllsnd = 0;
        if (!(m_stateBits & 2))
            detach();
        m_sf.loaded = 0;
        init(m_mpeg_list[play_index], 0x102);
        retval = load();
        if (retval != SNDERR_OK)
            return retval;
    }

    retval = Wave::play();
    if (retval != SNDERR_OK)
        return retval;
    m_link_semaphore = 1;
    return SNDERR_OK;
}

SNDERR MpegStream::add_to_playlist(char *fname)
{
    for (int i = 0; i < 40; i++) {
        if (m_mpeg_list[i] == 0) {
            m_mpeg_list[i] = new char[strlen(fname) + 1];
            strcpy(m_mpeg_list[i], fname);
            return SNDERR_OK;
        }
    }
    return SNDERR_END_OF_BUFFER;
}

SNDERR MpegStream::clear_playlist()
{
    for (int i = 0; i < 40; i++) {
        if (m_mpeg_list[i]) {
            delete m_mpeg_list[i];
            m_mpeg_list[i] = 0;
        }
    }
    return SNDERR_OK;
}

SoundList::SoundList()
{
    for (int i = 0; i < 40; i++)
        m_soundList[i] = 0;
    set_type(SOUNDTYPE_SOUNDLIST);
}

SoundList::~SoundList()
{
    for (int i = 0; i < 40; i++) {
        if (m_soundList[i]) {
            delete m_soundList[i];
            m_soundList[i] = 0;
        }
    }
}

SNDERR SoundList::attach()
{
    return Sound::attach();
}

SNDERR SoundList::detach()
{
    return Sound::detach();
}

SNDERR SoundList::stop()
{
    return SNDERR_UNINITIALIZED;
}

void SoundList::scaleVolumeToGroup(int volume)
{
}

SNDERR SoundList::add(char *fname)
{
    for (int i = 0; i < 40; i++) {
        if (m_soundList[i] == 0) {
            m_soundList[i] = new char[strlen(fname) + 1];
            strcpy(m_soundList[i], fname);
            return SNDERR_OK;
        }
    }
    return SNDERR_END_OF_BUFFER;
}

SNDERR SoundList::setListName(char *pname)
{
    if (pname == 0)
        return SNDERR_BAD_PARAM;

    m_fname = new char[strlen(pname) + 1];
    strcpy(m_fname, pname);
    return SNDERR_OK;
}

SNDERR SoundList::clear()
{
    stop();
    for (int i = 0; i < 40; i++) {
        if (m_soundList[i]) {
            delete m_soundList[i];
            m_soundList[i] = 0;
        }
    }
    return SNDERR_OK;
}


KeyMapWave::KeyMapWave()
{
    memset(m_fname, 0, sizeof m_fname);
    m_keylo = 0;
    m_keyhi = 0;
    m_nvoices = 1;
}

KeyMap::KeyMap()
{
    memset(&m_flags, 0, sizeof m_flags);
    m_flags.version = 1;
    m_cycle_hi = 0;
    m_cycle_lo = 0;
    m_name = 0;
    m_nwaves = 0;
    m_release_time = 250;
    memset(m_kmw_table, 0, sizeof m_kmw_table);
}

KeyMap::~KeyMap()
{
    for (int i = 0; i < 128; i++) {
        if (m_kmw_table[i] != 0) {
            i = m_kmw_table[i]->get_keyhi();
            unmap_range(m_kmw_table[i]);
        }
    }
    if (m_name != 0) {
        delete m_name;
        m_name = 0;
    }
}

unsigned long KeyMap::unmap_range(KeyMapWave *pkmw)
{
    unsigned long next_unique = 0;
    unsigned long i = pkmw->get_keylo();
    next_unique = pkmw->get_keyhi() + 1;
    delete m_kmw_table[i];
    for (; i < next_unique; i++)
        m_kmw_table[i] = 0;
    return next_unique;
}

void KeyMap::map_range(KeyMapWave *pkmw)
{
    unsigned long i;
    unsigned long keylo = pkmw->get_keylo();
    unsigned long keyhi = pkmw->get_keyhi();
    KeyMapWave *newkmw = new KeyMapWave;
    newkmw->set_fname(pkmw->get_fname());
    newkmw->set_keylo(keylo);
    newkmw->set_keyhi(keyhi);
    newkmw->set_nvoices(pkmw->get_nvoices());
    for (i = keylo; i <= keyhi; i++) {
        if (m_kmw_table[i] != 0)
            i = unmap_range(m_kmw_table[i]);
    }
    for (i = keylo; i <= keyhi; i++)
        m_kmw_table[i] = newkmw;
}

void KeyMap::set_name(const char *name)
{
    if (m_name != 0)
        ::operator delete(m_name);
    m_name = (char *)::operator new(strlen(name) + 1);
    strcpy(m_name, name);
}

void KeyMap::set_base_path(const char *p)
{
    char buff[256];
    char newfname[256];
    strcpy(buff, p);
    int plen = strlen(buff);
    if (buff[plen - 1] != '\\') {
        buff[plen] = '\\';
        buff[plen + 1] = 0;
    }
    for (int i = 0; i < 128; i++) {
        KeyMapWave *pkmw = m_kmw_table[i];
        if (pkmw == 0)
            continue;
        const char *oldfname = pkmw->get_fname();
        if (oldfname == 0)
            continue;
        strcpy(newfname, buff);
        const char *tmpname = strrchr(oldfname, '\\');
        if (tmpname != 0)
            tmpname++;
        else
            tmpname = oldfname;
        strcat(newfname, tmpname);
        pkmw->set_fname(newfname);
    }
}

const char *KeyMap::get_name()
{
    return m_name;
}

Midi::Midi()
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_flags.local_waves = 1;
    m_frame_delay = 0;
    m_sf.midi = 1;
}

Midi::~Midi()
{
    if (m_fname)
        delete m_fname;
    m_fname = 0;
    if (m_pdllsnd)
        delete_sound(m_pdllsnd);
    m_pdllsnd = 0;
    m_frame_delay = 0;
}

SNDERR Midi::play()
{
    if (m_pdllsnd)
        return m_pdllsnd->play();
    return SNDERR_NOT_LOADED;
}

void Midi::set_volume(int volume)
{
    Patch::set_volume(volume);
}

void Midi::set_frame(unsigned int frame)
{
    if (m_pdllsnd)
        ((Midi *)m_pdllsnd)->set_frame(frame);
}

void Midi::set_frame_delay(unsigned int msdelay)
{
    if (m_pdllsnd)
        ((Midi *)m_pdllsnd)->m_frame_delay = msdelay;
    m_frame_delay = msdelay;
}

unsigned Midi::get_frame()
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_frame();
    return 0;
}

void Midi::switch_state(bool state)
{
    if (m_pdllsnd)
        ((Midi *)m_pdllsnd)->switch_state(state);
}

SNDERR Midi::set_switch_type(unsigned long switch_type)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_switch_type(switch_type);
    return SNDERR_UNINITIALIZED;
}

SNDERR Midi::add_switch_range(unsigned long tracklo, unsigned long trackhi)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->add_switch_range(tracklo, trackhi);
    return SNDERR_UNINITIALIZED;
}

SNDERR Midi::set_nswitch_threads(unsigned long nswitch_threads)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_nswitch_threads(nswitch_threads);
    return SNDERR_UNINITIALIZED;
}

SNDERR Midi::map_patch(char *fname, unsigned long number, unsigned long midi_channel)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->map_patch(fname, number, midi_channel);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::map_patch(char *fname, unsigned long number)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->map_patch(fname, number);
    return SNDERR_NOT_LOADED;
}

void Midi::set_base_path(const char *p)
{
    if (m_pdllsnd)
        ((Midi *)m_pdllsnd)->set_base_path(p);
}

SNDERR Midi::clear_patch(unsigned long number)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->clear_patch(number);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::reset()
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->reset();
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::set_track(unsigned char *pdata, unsigned long tracknum)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_track(pdata, tracknum);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::mute_track(unsigned long tracknum)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->mute_track(tracknum);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::unmute_track(unsigned long tracknum)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->unmute_track(tracknum);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::set_active_tracks(unsigned long lo, unsigned long hi)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_active_tracks(lo, hi);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::set_active_tracks(unsigned int index)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_active_tracks(index);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::play_trackset(unsigned int setid)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->play_trackset(setid);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::xpose_trackset(unsigned int setid, int pitch)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->xpose_trackset(setid, pitch);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::stop_trackset(unsigned int setid)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->stop_trackset(setid);
    return SNDERR_NOT_LOADED;
}

bool Midi::is_trackset_playing(unsigned int setid)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->is_trackset_playing(setid);
    return 0;
}

SNDERR Midi::add_active_trackset(TrackSet *pts)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->add_active_trackset(pts);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::set_active_range_lo(unsigned int setid, unsigned int lo)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_active_range_lo(setid, lo);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::set_active_range_hi(unsigned int setid, unsigned int hi)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_active_range_hi(setid, hi);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::remove_active_trackset(unsigned int setid)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->remove_active_trackset(setid);
    return SNDERR_NOT_LOADED;
}

TrackSet *Midi::get_trackset(unsigned int set_id)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_trackset(set_id);
    return 0;
}

unsigned long Midi::get_ntracks()
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_ntracks();
    return 0;
}

SNDERR Midi::load_patch(unsigned int patch_num)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->load_patch(patch_num);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::unload_patch(unsigned int patch_num)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->unload_patch(patch_num);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::set_patch(unsigned int midi_channel, unsigned int patch_num)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_patch(midi_channel, patch_num);
    return SNDERR_NOT_LOADED;
}

void Midi::init(const char *fname, unsigned long flags)
{
    if (fname != 0) {
        if (m_fname != 0)
            ::operator delete(m_fname);
        m_fname = 0;
        m_fname = (char *)::operator new(strlen(fname) + 1);
        strcpy(m_fname, fname);
    }
    memset(&m_flags, 0, sizeof(m_flags));
    if (flags & 1)
        m_flags.sync_to_flic = 1;
}

SNDERR Midi::set_load_type(bool blocal)
{
    m_flags.local_waves = blocal;
    if (m_pdllsnd)
        ((Midi *)m_pdllsnd)->set_load_type(blocal);
    return SNDERR_OK;
}

SNDERR Midi::set_fname(const char *fname)
{
    if (fname != 0) {
        if (m_fname != 0)
            ::operator delete(m_fname);
        m_fname = 0;
        m_fname = (char *)::operator new(strlen(fname) + 1);
        strcpy(m_fname, fname);
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR Midi::set_tempo(unsigned long tempo)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_tempo(tempo);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::load(const char *fname)
{
    SNDERR rval;
    unsigned long attrib = 0;

    if (m_pdllsnd == 0)
        rval = create_sound(&m_pdllsnd, fname, SOUNDTYPE_MIDI);
    if (rval != SNDERR_OK)
        return rval;

    rval = Sound::load(fname);
    if (rval != SNDERR_OK)
        return rval;

    if (m_flags.sync_to_flic) {
        ((Midi *)m_pdllsnd)->m_flags.sync_to_flic = 1;
        if (m_frame_delay != 0)
            ((Midi *)m_pdllsnd)->m_frame_delay = m_frame_delay;
    }
    return SNDERR_OK;
}

SNDERR Midi::load(const char *fname, unsigned int flags)
{
    init(fname, flags);
    return load();
}

SNDERR Midi::load()
{
    SNDERR rval = SNDERR_OK;
    unsigned long attrib = 0;
    char *fname = get_fname();
    if (fname) {
        if (m_pdllsnd == 0) {
            rval = create_sound(&m_pdllsnd, fname, SOUNDTYPE_MIDI);
            if (rval != SNDERR_OK)
                return rval;
        }

        rval = Sound::load(fname);
        if (rval == SNDERR_OK && m_flags.sync_to_flic) {
            ((Midi *)m_pdllsnd)->m_flags.sync_to_flic = 1;
            if (m_frame_delay != 0)
                ((Midi *)m_pdllsnd)->m_frame_delay = m_frame_delay;
        }
        return rval;
    }
    return SNDERR_BAD_FILE_OPEN;
}

SNDERR Midi::unload()
{
    SNDERR retval = SNDERR_OK;
    if (m_pdllsnd) {
        if (!is_playing()) {
            m_pdllsnd->unload();
            delete_sound(m_pdllsnd);
        } else {
            retval = m_pdllsnd->unload();
        }
        m_pdllsnd = 0;
    }
    detach();
    m_sf.loaded = 0;
    return retval;
}

unsigned long Midi::get_time(unsigned long instance)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_time(instance);
    return 0;
}

unsigned long Midi::get_control_track()
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_control_track();
    return 0;
}

unsigned long Midi::get_total_track_ticks(unsigned long track)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_total_track_ticks(track);
    return 0;
}

SNDERR Midi::set_control(void *pcontrol, int target_type, int target)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_control(pcontrol, target_type, target);
    return SNDERR_NOT_LOADED;
}

void *Midi::get_control_target(int target_type, int target)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_control_target(target_type, target);
    return 0;
}

SNDERR Midi::set_track_loop_state(unsigned int track, bool state)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_track_loop_state(track, state);
    return SNDERR_NOT_LOADED;
}

bool Midi::get_track_loop_state(unsigned int track)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_track_loop_state(track);
    return 0;
}

SNDERR Midi::get_track_velocity_offset(unsigned int track, int *pvelocity_offset)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_track_velocity_offset(track, pvelocity_offset);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::set_track_velocity_offset(unsigned int track, int velocity_offset)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->set_track_velocity_offset(track, velocity_offset);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi::add_keymap(KeyMap *pkm)
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->add_keymap(pkm);
    return SNDERR_NOT_LOADED;
}

void Midi::set_play_type(unsigned int play_type)
{
    if (m_pdllsnd)
        ((Midi *)m_pdllsnd)->set_play_type(play_type);
}

int Midi::get_key()
{
    if (m_pdllsnd)
        return ((Midi *)m_pdllsnd)->get_key();
    return 0;
}

int Midi::get_tonal_center()
{
    int key = get_key();
    key = (signed char)(key >> 8);

    switch (key) {
    case -1:
        return 5;
    case -2:
        return 10;
    case -3:
        return 3;
    case -4:
        return 8;
    case -7:
    case 5:
        return 11;
    case 1:
        return 7;
    case 2:
        return 2;
    case 3:
        return 9;
    case 4:
        return 4;
    case -6:
    case 6:
        return 6;
    case -5:
    case 7:
        return 1;
    default:
        return 0;
    }
}

GlblMidiSet::GlblMidiSet()
{
    m_ntracksets = 0;
    m_trackset_size = sizeof(TrackSet);
    m_flags.looping = 1;
}

TrackSet::TrackSet()
{
    m_id = 0;
    m_tracklo = 0;
    m_trackhi = 0;
    m_nswitch_threads = 0;
    m_flags.all = 0;
    m_loops = 0;
    m_linked_set = 0;
    m_tempo = 0;
    m_sync_track = -1;
}

void TrackSet::set_lo(unsigned lo)
{
    if (m_trackhi < lo)
        m_trackhi = lo;
    m_tracklo = lo;
}

void TrackSet::set_hi(unsigned hi)
{
    if (m_tracklo > hi)
        m_tracklo = hi;
    m_trackhi = hi;
}

SNDERR Sound::unloadall()
{
    SNDERR retval;

    while (gbl_sound_head)
        gbl_sound_head->unload();
    retval = SNDERR_NO_SUPPORT;
    return retval;
}

Patch::Patch()
{
    m_volume = 0x7f;
    m_pan = 0;
    memset(&m_adsr, 0, sizeof(m_adsr));
    m_loop_state = 0;
}

Patch::~Patch()
{
}

Ambience::Ambience()
{
    set_type(SOUNDTYPE_AMBIENCE);
}

SNDERR Ambience::load(const char *fname, unsigned int flags)
{
    SNDERR err = SNDERR_OK;

    if (flags & 1)
        set_sync_to_flic();
    return load(fname);
}

SNDERR Ambience::load(const char *fname)
{
    SNDERR rval = SNDERR_OK;
    unsigned char alt_path = 0;
    unsigned long attrib = 0;
    char p[0x100];

    strcpy(p, fname);
    char *new_path = p;

    if (m_pdllsnd != 0)
        unload();
    if (m_pdllsnd == 0) {
        rval = create_sound(&m_pdllsnd, new_path, SOUNDTYPE_AMBIENCE);
        if (rval != SNDERR_OK)
            return rval;
    }

    ((Midi *)m_pdllsnd)->set_load_type(is_local_load());

    rval = Sound::load(new_path);
    if (rval != SNDERR_OK)
        return rval;

    if (is_sync_to_flic()) {
        ((Midi *)m_pdllsnd)->set_sync_to_flic();
        if (get_frame_delay())
            ((Midi *)m_pdllsnd)->set_frame_delay(get_frame_delay());
    }

    if (m_pdllsnd != 0) {
        m_pdllsnd->set_volume(get_volume());
        m_pdllsnd->set_pan(get_pan());
    }
    return SNDERR_OK;
}

SNDERR Ambience::unload()
{
    if (m_pdllsnd != 0)
        m_pdllsnd->unload();
    detach();
    m_pdllsnd = 0;
    m_sf.loaded = 0;
    return SNDERR_OK;
}

AviSound::AviSound()
{
    set_type(SOUNDTYPE_AVI);
}

AviSound::~AviSound()
{
    if (m_pdllsnd) {
        delete m_pdllsnd;
        m_pdllsnd = 0;
    }
}



SNDERR AviSound::init(AVIStreamHeader *paudheader, AVIStreamHeader *pvidheader, WAVEFORMATEX *lpwfx,
                      unsigned long flags)
{
    SNDERR retval = SNDERR_OK;

    if (m_pdllsnd == 0) {
        retval = create_sound(&m_pdllsnd, 0, SOUNDTYPE_AVI);
        if (retval != SNDERR_OK)
            return retval;
    }
    if (m_pdllsnd == 0)
        return retval;

    retval = ((AviSound *)m_pdllsnd)->init(paudheader, pvidheader, lpwfx, flags);
    if (retval != SNDERR_OK) {
        delete_sound(m_pdllsnd);
        m_pdllsnd = 0;
    }
    attach();
    return retval;
}

SNDERR AviSound::release()
{
    SNDERR retval = SNDERR_OK;

    if (m_pdllsnd) {
        if (!m_pdllsnd->is_playing())
            delete_sound(m_pdllsnd);
        else
            retval = m_pdllsnd->release();
        m_pdllsnd = 0;
    }
    detach();
    return SNDERR_OK;
}

SNDERR AviSound::start(unsigned long start_frame)
{
    if (m_pdllsnd)
        ((AviSound *)m_pdllsnd)->start(start_frame);
    return SNDERR_OK;
}

SNDERR AviSound::stop()
{
    if (m_pdllsnd)
        m_pdllsnd->stop();
    return SNDERR_OK;
}

MMIOINFO *AviSound::get_frame(unsigned long size)
{
    if (m_pdllsnd)
        return ((AviSound *)m_pdllsnd)->get_frame(size);
    return 0;
}

SNDERR AviSound::return_frame(MMIOINFO *pmmioinfo)
{
    if (m_pdllsnd)
        return ((AviSound *)m_pdllsnd)->return_frame(pmmioinfo);
    return SNDERR_UNINITIALIZED;
}

int AviSound::get_cur_frame()
{
    if (m_pdllsnd)
        return ((AviSound *)m_pdllsnd)->get_cur_frame();
    return -1;
}

unsigned long AviSound::get_frame_size()
{
    if (m_pdllsnd)
        return ((AviSound *)m_pdllsnd)->get_frame_size();
    return 0;
}

unsigned long AviSound::get_time()
{
    return 0;
}

VoiceRx::VoiceRx()
{
    char fname[9] = "Voice Rx";
    set_type(SOUNDTYPE_VOICERX);
    m_fname = new char[10];
    strcpy(m_fname, fname);
}

VoiceRx::~VoiceRx()
{
    if (m_pdllsnd) {
        delete m_pdllsnd;
        m_pdllsnd = 0;
    }
    if (m_fname) {
        delete m_fname;
        m_fname = 0;
    }
}

SNDERR VoiceRx::init(unsigned long packet_size, WAVEFORMATEX *lpwfx, unsigned long flags)
{
    SNDERR retval = SNDERR_OK;

    if (m_pdllsnd == 0) {
        retval = create_sound(&m_pdllsnd, 0, SOUNDTYPE_VOICERX);
        if (retval != SNDERR_OK)
            return retval;
    }
    if (m_pdllsnd == 0)
        return retval;

    retval = ((VoiceRx *)m_pdllsnd)->init(packet_size, lpwfx, flags);
    if (retval != SNDERR_OK) {
        delete_sound(m_pdllsnd);
        m_pdllsnd = 0;
    }
    attach();
    return retval;
}

SNDERR VoiceRx::release()
{
    SNDERR retval = SNDERR_OK;

    if (m_pdllsnd) {
        if (!m_pdllsnd->is_playing())
            delete_sound(m_pdllsnd);
        else
            retval = m_pdllsnd->release();
        m_pdllsnd = 0;
    }
    detach();
    return SNDERR_OK;
}

SNDERR VoiceRx::start()
{
    if (m_pdllsnd)
        ((VoiceRx *)m_pdllsnd)->start();
    return SNDERR_OK;
}

SNDERR VoiceRx::stop()
{
    if (m_pdllsnd)
        m_pdllsnd->stop();
    return SNDERR_OK;
}

SNDERR VoiceRx::unload()
{
    if (m_pdllsnd)
        return m_pdllsnd->unload();
    return SNDERR_UNINITIALIZED;
}

SNDERR VoiceRx::add_buffer(MMIOINFO *pmmioinfo)
{
    if (m_pdllsnd)
        return ((VoiceRx *)m_pdllsnd)->add_buffer(pmmioinfo);
    return SNDERR_UNINITIALIZED;
}

unsigned long VoiceRx::get_buffer_size()
{
    if (m_pdllsnd)
        return ((VoiceRx *)m_pdllsnd)->get_buffer_size();
    return 0;
}

VoiceTx::VoiceTx()
{
    memset(&m_flags, 0, sizeof m_flags);
    char fname[] = "Voice Tx";
    m_fname = new char[10];
    strcpy(m_fname, fname);
    set_type(SOUNDTYPE_VOICETX);
}


VoiceTx::~VoiceTx()
{
    if (m_pdllsnd)
        delete_sound(m_pdllsnd);
    m_pdllsnd = 0;
    if (m_fname) {
        delete m_fname;
        m_fname = 0;
    }
}

SNDERR VoiceTx::release()
{
    if (m_pdllsnd)
        m_pdllsnd->release();
    return SNDERR_OK;
}

SNDERR VoiceTx::start()
{
    if (!m_flags.recording) {
        if (m_pdllsnd) {
            SNDERR result = ((VoiceTx *)m_pdllsnd)->start();
            if (result)
                return result;
        }
        m_flags.recording = 1;
    }
    return SNDERR_OK;
}

SNDERR VoiceTx::stop()
{
    SNDERR result = SNDERR_OK;

    if (m_flags.recording && m_pdllsnd)
        return m_pdllsnd->stop();
    return SNDERR_OK;
}

SNDERR VoiceTx::unload()
{
    stop();
    release();
    delete_sound(m_pdllsnd);
    m_pdllsnd = 0;
    return SNDERR_OK;
}

SNDERR VoiceTx::init(unsigned long buffer_size, WAVEFORMATEX *lpwfx, unsigned long flags)
{
    SNDERR retval = SNDERR_OK;

    unload();

    if (m_pdllsnd == 0) {
        retval = create_sound(&m_pdllsnd, 0, SOUNDTYPE_VOICETX);
        if (retval != SNDERR_OK)
            return retval;
    }
    if (m_pdllsnd == 0)
        return retval;

    retval = ((CVoiceTxMgr *)m_pdllsnd)->init(buffer_size, lpwfx, flags);
    if (retval != SNDERR_OK && retval != SNDERR_ALLOCATED) {
        delete_sound(m_pdllsnd);
        m_pdllsnd = 0;
    }
    if (m_pdllsnd != 0)
        attach();
    return retval;
}

unsigned long VoiceTx::get_nbuffers()
{
    if (m_pdllsnd)
        return ((VoiceTx *)m_pdllsnd)->get_nbuffers();
    return 0;
}

MMIOINFO *VoiceTx::get_next_buffer()
{
    if (m_pdllsnd)
        return ((VoiceTx *)m_pdllsnd)->get_next_buffer();
    return 0;
}

SNDERR VoiceTx::return_buffer(MMIOINFO *pbuffer)
{
    if (m_pdllsnd)
        return ((VoiceTx *)m_pdllsnd)->return_buffer(pbuffer);
    return SNDERR_UNINITIALIZED;
}

void do_message(const char *msg, const char *caption)
{
    MessageBoxA(0, msg, caption, 0);
}
