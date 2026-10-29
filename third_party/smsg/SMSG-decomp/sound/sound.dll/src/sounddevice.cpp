#include "SoundStreamBase.h"
#include "sounddevice.h"
#include "sound.h"

Sound_Device *Sound_Device::m_devlist_head = 0;
Sound_Device *Sound_Device::m_devlist_tail = 0;

SNDERR Sound_Device::attach()
{
    SNDERR retval = SNDERR_OK;

    if (m_pnext == 0 && m_pprev == 0) {
        if (m_devlist_tail == 0) {
            m_devlist_head = this;
            m_devlist_tail = this;
            m_pnext = 0;
            m_pprev = 0;
        } else {
            m_pprev = m_devlist_tail;
            m_devlist_tail = this;
            m_pprev->m_pnext = this;
            m_pnext = 0;
        }
        m_flags.attached = 1;
    }
    return retval;
}

SNDERR Sound_Device::detach()
{
    if (!m_flags.attached)
        return SNDERR_OK;
    if (m_pprev)
        m_pprev->m_pnext = m_pnext;
    else
        m_devlist_head = m_pnext;
    if (m_pnext)
        m_pnext->m_pprev = m_pprev;
    else
        m_devlist_tail = m_pprev;
    m_flags.attached = 0;
    m_pnext = 0;
    m_pprev = 0;
    return SNDERR_OK;
}

Sound_Device::Sound_Device()
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_volume = 0x7f;
    m_pan = 0;
    m_pprev = 0;
    m_pnext = 0;
    m_ms_rate = 0;
    m_pdll_device = 0;
}

Sound_Device::~Sound_Device()
{
    if (m_pdll_device)
        m_pdll_device->release();
}

Wave_Device::Wave_Device()
{
    memset(&m_flags, 0, sizeof(m_flags));
}

Wave_Device::~Wave_Device()
{
}

SNDERR Wave_Device::create_device(unsigned long flags)
{
    return SNDERR_OK;
}

SNDERR Wave_Device::delete_device()
{
    return SNDERR_OK;
}

SNDERR Wave_Device::init(HWND game_hwnd, unsigned long flags)
{
    SNDERR retval = create_device(flags);
    if (retval != SNDERR_OK)
        return retval;
    retval = m_pdll_device->init(game_hwnd, flags);
    if (retval != SNDERR_OK) {
        delete_device();
        return retval;
    }
    return SNDERR_OK;
}

SNDERR Wave_Device::release()
{
    if (m_pdll_device) {
        m_pdll_device->release();
        m_pdll_device = 0;
    }
    return SNDERR_OK;
}

void Wave_Device::suspend()
{
    if (m_pdll_device)
        m_pdll_device->suspend();
}

void Wave_Device::restart()
{
    if (m_pdll_device)
        m_pdll_device->restart();
}

void Wave_Device::update_sound()
{
    if (m_pdll_device)
        m_pdll_device->update_sound();
}

unsigned long Wave_Device::get_ndevices()
{
    if (m_pdll_device)
        return m_pdll_device->get_ndevices();
    return 0;
}

SNDERR Wave_Device::set_hwnd(HWND hwnd)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_hwnd(hwnd);
    return SNDERR_UNINITIALIZED;
}

unsigned long Wave_Device::get_hw_mem_size()
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->get_hw_mem_size();
    return 0;
}

SNDERR Wave_Device::select(unsigned long id)
{
    Sound *pcursnd = 0;
    Sound *unloaded = 0;

    if (m_pdll_device == 0)
        return SNDERR_NOT_CREATED;

    for (pcursnd = Sound::get_list(); pcursnd != 0;) {
        if ((pcursnd->get_attrib() & 1) && !(pcursnd->get_attrib() & 4)) {
            Sound *temp = pcursnd->get_next();
            pcursnd->unload();
            if (unloaded == 0) {
                pcursnd->set_prev(0);
                pcursnd->set_next(0);
            } else {
                unloaded->set_next(pcursnd);
                pcursnd->set_prev(unloaded);
            }
            unloaded = pcursnd;
            pcursnd = temp;
        } else {
            pcursnd = pcursnd->get_next();
        }
    }

    m_pdll_device->select(id);

    while (unloaded) {
        pcursnd = unloaded;
        unloaded = unloaded->get_prev();
        pcursnd->set_next(0);
        pcursnd->set_prev(0);
        pcursnd->load(pcursnd->get_fname());
    }
    return SNDERR_OK;
}

SNDERR Wave_Device::get_description(unsigned long id, char *buff, unsigned long buffsize)
{
    if (m_pdll_device)
        m_pdll_device->get_description(id, buff, buffsize);
    return SNDERR_OK;
}

void Wave_Device::set_rate(unsigned long ms)
{
    if (m_pdll_device)
        m_pdll_device->set_rate(ms);
}

unsigned long Wave_Device::get_rate()
{
    if (m_pdll_device)
        return m_pdll_device->get_rate();
    return 0;
}

void Wave_Device::set_volume(unsigned long volume)
{
    if (m_pdll_device)
        m_pdll_device->set_volume(volume);
}

unsigned long Wave_Device::get_volume()
{
    return 0;
}

void Wave_Device::set_pan(int pan)
{
}

void *Wave_Device::get_ds()
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->get_ds();
    return 0;
}

SNDERR Wave_Device::fade(unsigned long ms_fade_time)
{
    return SNDERR_OK;
}

SNDERR Wave_Device::stop()
{
    return SNDERR_OK;
}

SNDERR Wave_Device::enable()
{
    if (m_pdll_device)
        ((Wave_Device *)m_pdll_device)->enable();
    return SNDERR_OK;
}

SNDERR Wave_Device::disable()
{
    if (m_pdll_device)
        ((Wave_Device *)m_pdll_device)->disable();
    return SNDERR_OK;
}

int Wave_Device::is_disabled()
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->is_disabled();
    return 1;
}

SNDERR Wave_Device::start_raw_dump(char *fname)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->start_raw_dump(fname);
    return SNDERR_NO_DEVICE;
}

SNDERR Wave_Device::stop_raw_dump()
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->stop_raw_dump();
    return SNDERR_NO_DEVICE;
}

SNDERR Wave_Device::add_to_group(unsigned group, Sound *pSound)
{
    if (group <= 15 && pSound != 0) {
        m_group_list[group].m_wave_list.Append(pSound);
        pSound->m_group = group;
        if (pSound->get_type() == SOUNDTYPE_SOUNDLIST)
            static_cast<SoundList *>(pSound)->scaleVolumeToGroup(pSound->get_volume());
        else
            pSound->set_volume(pSound->get_volume());
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

SNDERR Wave_Device::pull_from_group(Sound *pSound)
{
    if (pSound == 0 || pSound->m_group >= 16)
        return SNDERR_BAD_PARAM;
    m_group_list[pSound->m_group].m_wave_list.Remove(pSound);
    pSound->m_group = 16;
    return SNDERR_OK;
}

SNDERR Wave_Device::set_group_volume(unsigned group, unsigned volume)
{
    if (group > 15 || volume > 0x7f)
        return SNDERR_BAD_PARAM;
    m_group_list[group].m_volume = volume;
    Sound *psound = (Sound *)m_group_list[group].m_wave_list.First();
    while (psound != 0) {
        if (psound->get_type() == SOUNDTYPE_SOUNDLIST)
            static_cast<SoundList *>(psound)->scaleVolumeToGroup(psound->get_volume());
        else
            psound->set_volume(psound->get_volume());
        psound = (Sound *)m_group_list[group].m_wave_list.Next();
    }
    return SNDERR_OK;
}

unsigned Wave_Device::get_group_volume(unsigned group)
{
    if (group > 15)
        return 0;
    return m_group_list[group].m_volume;
}

SNDERR Wave_Device::enable_group(unsigned group)
{
    if (group > 15)
        return SNDERR_BAD_PARAM;
    if (m_group_list[group].m_flags.status)
        return SNDERR_OK;
    Sound *psound = (Sound *)m_group_list[group].m_wave_list.First();
    while (psound != 0) {
        psound->load(psound->get_fname());
        psound = (Sound *)m_group_list[group].m_wave_list.Next();
    }
    m_group_list[group].m_flags.status = 1;
    return SNDERR_OK;
}

SNDERR Wave_Device::muteGroup(unsigned group)
{
    if (group > 15)
        return SNDERR_BAD_PARAM;
    Sound *psound = (Sound *)m_group_list[group].m_wave_list.First();
    while (psound != 0) {
        psound->stop();
        psound = (Sound *)m_group_list[group].m_wave_list.Next();
    }
    m_group_list[group].mute();
    return SNDERR_OK;
}

SNDERR Wave_Device::unMuteGroup(unsigned group)
{
    if (group > 15)
        return SNDERR_BAD_PARAM;
    m_group_list[group].m_flags.muted = 0;
    return SNDERR_OK;
}

SNDERR Wave_Device::disable_group(unsigned group)
{
    if (group > 15)
        return SNDERR_BAD_PARAM;
    if (!m_group_list[group].m_flags.status)
        return SNDERR_OK;
    Sound *psound = (Sound *)m_group_list[group].m_wave_list.First();
    while (psound != 0) {
        if (psound->get_type() == SOUNDTYPE_SOUNDLIST)
            psound->stop();
        else
            psound->unload();
        psound = (Sound *)m_group_list[group].m_wave_list.Next();
    }
    m_group_list[group].m_flags.status = 0;
    return SNDERR_OK;
}

bool Wave_Device::is_group_disabled(unsigned group)
{
    if (group > 15)
        return true;
    if (m_group_list[group].m_flags.status)
        return false;
    return true;
}

bool Wave_Device::isGroupMute(unsigned group)
{
    if (group > 15)
        return true;
    if (m_group_list[group].m_flags.muted)
        return true;
    return false;
}

inline WaveControlGroup::WaveControlGroup()
{
    m_volume = 0x7f;
    m_flags.status = 1;
}

bool Wave_Device::is_eax()
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->is_eax();
    return 0;
}

SNDERR Wave_Device::set_eax(EAX_REVERB_PROPERTIES *peax)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_eax(peax);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::set_eax(unsigned long environment)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_eax(environment);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::set_eax_mix(float mix)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_eax_mix(mix);
    return SNDERR_NOT_LOADED;
}

bool Wave_Device::is_3d()
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->is_3d();
    return 0;
}

SNDERR Wave_Device::set_listener_position(float x, float y, float z)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_listener_position(x, y, z);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::get_listener_position(float *px, float *py, float *pz)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->get_listener_position(px, py, pz);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::set_listener_xpos(float x)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_listener_xpos(x);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::get_listener_xpos(float *px)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->get_listener_xpos(px);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::set_listener_ypos(float y)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_listener_ypos(y);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::get_listener_ypos(float *py)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->get_listener_ypos(py);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::set_listener_zpos(float z)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->set_listener_zpos(z);
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::get_listener_zpos(float *pz)
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->get_listener_zpos(pz);
    return SNDERR_NOT_LOADED;
}

SNDERR Midi_Device::create_device(unsigned long flags)
{
    return SNDERR_OK;
}

SNDERR Midi_Device::delete_device()
{
    return SNDERR_OK;
}

Midi_Device::Midi_Device()
{
}

Midi_Device::~Midi_Device()
{
}

SNDERR Midi_Device::init(HWND game_hwnd, unsigned long flags)
{
    SNDERR retval = create_device(flags);
    if (retval)
        return retval;

    retval = m_pdll_device->init(game_hwnd, flags);
    if (retval) {
        delete_device();
        m_pdll_device = 0;
        return retval;
    }
    return SNDERR_OK;
}

SNDERR Midi_Device::release()
{
    if (m_pdll_device) {
        m_pdll_device->release();
        m_pdll_device = 0;
    }
    return SNDERR_OK;
}

unsigned long Midi_Device::get_ndevices()
{
    return SNDERR_OK;
}

SNDERR Midi_Device::select(unsigned long id)
{
    return SNDERR_OK;
}

SNDERR Midi_Device::get_description(unsigned long id, char *desc_string, unsigned long buffsize)
{
    return SNDERR_OK;
}

void Midi_Device::set_volume(unsigned long volume)
{
}

unsigned long Midi_Device::get_volume()
{
    return 0;
}

void Midi_Device::set_pan(int pan)
{
}

SNDERR Midi_Device::fade(unsigned long ms_fade_time)
{
    return SNDERR_OK;
}

SNDERR Midi_Device::stop()
{
    return SNDERR_OK;
}

void Midi_Device::update_sound()
{
}

void Midi_Device::set_rate(unsigned long ms)
{
}

unsigned long Midi_Device::get_rate()
{
    return 0;
}

void Midi_Device::suspend()
{
}

void Midi_Device::restart()
{
}

SNDERR Midi_Device::enable()
{
    if (m_pdll_device)
        ((Midi_Device *)m_pdll_device)->enable();
    return SNDERR_OK;
}

SNDERR Midi_Device::disable()
{
    if (m_pdll_device)
        ((Midi_Device *)m_pdll_device)->disable();
    return SNDERR_OK;
}

int Midi_Device::is_disabled()
{
    if (m_pdll_device)
        return ((Midi_Device *)m_pdll_device)->is_disabled();
    return SNDERR_NO_DRIVER;
}

Wave_In_Device::Wave_In_Device()
{
}

Wave_In_Device::~Wave_In_Device()
{
}

SNDERR Wave_In_Device::create_device(unsigned long flags)
{
    return SNDERR_OK;
}

SNDERR Wave_In_Device::delete_device()
{
    return SNDERR_OK;
}

SNDERR Wave_In_Device::init(HWND game_hwnd, unsigned long flags)
{
    SNDERR retval = create_device(flags);
    if (retval)
        return retval;

    retval = m_pdll_device->init(game_hwnd, flags);
    if (retval) {
        delete_device();
        return retval;
    }
    return SNDERR_OK;
}

SNDERR Wave_In_Device::release()
{
    if (m_pdll_device) {
        m_pdll_device->release();
        delete_device();
        m_pdll_device = 0;
    }
    return SNDERR_OK;
}

SNDERR Wave_In_Device::set_codec(unsigned long codec)
{
    if (m_pdll_device)
        return ((Wave_In_Device *)m_pdll_device)->set_codec(codec);
    return SNDERR_UNINITIALIZED;
}

SNDERR Wave_In_Device::set_vxw_key(signed char *key)
{
    if (m_pdll_device)
        return ((Wave_In_Device *)m_pdll_device)->set_vxw_key(key);
    return SNDERR_UNINITIALIZED;
}

unsigned long Wave_In_Device::get_ndevices()
{
    return 0;
}

SNDERR Wave_In_Device::select(unsigned long id)
{
    return SNDERR_OK;
}

SNDERR Wave_In_Device::get_description(unsigned long id, char *desc_string, unsigned long buffsize)
{
    return SNDERR_OK;
}

SNDERR Wave_In_Device::get_caps(unsigned long id)
{
    return SNDERR_OK;
}

SNDERR Wave_In_Device::stop()
{
    return SNDERR_OK;
}

void Wave_In_Device::update_sound()
{
}

void Wave_In_Device::set_rate(unsigned long ms)
{
}

unsigned long Wave_In_Device::get_rate()
{
    return 0;
}

void Wave_In_Device::suspend()
{
}

void Wave_In_Device::restart()
{
}

SNDERR Wave_In_Device::start_record()
{
    if (m_pdll_device)
        ((Wave_In_Device *)m_pdll_device)->start_record();
    return SNDERR_OK;
}

SNDERR Wave_In_Device::end_record()
{
    if (m_pdll_device)
        ((Wave_In_Device *)m_pdll_device)->end_record();
    return SNDERR_OK;
}

#include "SndNodeList.inl"
