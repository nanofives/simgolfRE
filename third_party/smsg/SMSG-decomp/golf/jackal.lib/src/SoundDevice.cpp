#include "SoundDll.h"
#include "WaveSoundDevice.h"
#include "SoundObject1.h"
#include "WaveInSoundDevice.h"

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
    if (m_pdll_device == 0) {
        if (gbl_fntable[CREATEWAVEDEVICE] != 0)
            return ((PFNCREATEWAVEDEVICE)gbl_fntable[CREATEWAVEDEVICE])(&m_pdll_device, flags);
        return SNDERR_NOT_LOADED;
    }
    return SNDERR_ALREADY_OPEN;
}

SNDERR Wave_Device::delete_device()
{
    if (m_pdll_device != 0) {
        if (gbl_fntable[DELETEWAVEDEVICE] != 0) {
            ((PFNDELETEWAVEDEVICE)gbl_fntable[DELETEWAVEDEVICE])();
            m_pdll_device = 0;
            return SNDERR_OK;
        }
    }
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_Device::release()
{
    if (m_pdll_device) {
        m_pdll_device->release();
        Wave_Device::delete_device();
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

void *Wave_Device::get_ds()
{
    if (m_pdll_device)
        return ((Wave_Device *)m_pdll_device)->get_ds();
    return 0;
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

SNDERR Wave_Device::muteGroup(unsigned group)
{
    if (group > 15)
        return SNDERR_BAD_PARAM;
    SndNodeList *list = &m_group_list[group].m_wave_list;
    SndNode *n = list->m_head;
    list->m_cursor = n;
    if (n != 0) {
        Sound *psound = (Sound *)n->m_payload;
        while (psound != 0) {
            psound->stop();
            psound = (Sound *)list->Next();
        }
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

inline void Wave_Device::timer_unhook()
{
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

unsigned long Wave_Device::get_volume()
{
    return 0;
}

void Wave_Device::set_pan(int pan)
{
}

SNDERR Wave_Device::fade(unsigned long ms_fade_time)
{
    return SNDERR_OK;
}

SNDERR Wave_Device::stop()
{
    return SNDERR_OK;
}

inline void Wave_Device::add_to_time_past(unsigned long mstime)
{
}

inline unsigned long Wave_Device::get_time_past()
{
    return 0;
}

inline void Wave_Device::reset_time_past()
{
}

SNDERR Midi_Device::create_device(unsigned long flags)
{
    if (m_pdll_device == 0) {
        if (gbl_fntable[CREATEMIDIDEVICE] != 0)
            return ((PFNCREATEMIDIDEVICE)gbl_fntable[CREATEMIDIDEVICE])(&m_pdll_device, flags);
        return SNDERR_NOT_LOADED;
    }
    return SNDERR_ALREADY_OPEN;
}

SNDERR Midi_Device::delete_device()
{
    if (m_pdll_device != 0) {
        if (gbl_fntable[DELETEMIDIDEVICE] != 0) {
            ((PFNDELETEMIDIDEVICE)gbl_fntable[DELETEMIDIDEVICE])();
            m_pdll_device = 0;
            return SNDERR_OK;
        }
    }
    return SNDERR_NOT_LOADED;
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
        Midi_Device::delete_device();
        m_pdll_device = 0;
    }
    return SNDERR_OK;
}

void Midi_Device::set_volume(unsigned long volume)
{
}

SNDERR Midi_Device::select(unsigned long id)
{
    return SNDERR_OK;
}

SNDERR Midi_Device::enable()
{
    if (m_pdll_device)
        ((Midi_Device *)m_pdll_device)->enable();
    return SNDERR_OK;
}

int Midi_Device::is_disabled()
{
    if (m_pdll_device)
        return ((Midi_Device *)m_pdll_device)->is_disabled();
    return SNDERR_NO_DRIVER;
}

inline void Midi_Device::timer_unhook()
{
}

unsigned long Midi_Device::get_ndevices()
{
    return SNDERR_OK;
}

SNDERR Midi_Device::get_description(unsigned long id, char *desc_string, unsigned long buffsize)
{
    return SNDERR_OK;
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

inline void Midi_Device::add_to_time_past(unsigned long mstime)
{
}

inline unsigned long Midi_Device::get_time_past()
{
    return 0;
}

void Midi_Device::suspend()
{
}

void Midi_Device::restart()
{
}

inline void Midi_Device::reset_time_past()
{
}

SNDERR Midi_Device::disable()
{
    if (m_pdll_device)
        ((Midi_Device *)m_pdll_device)->disable();
    return SNDERR_OK;
}

Wave_In_Device::Wave_In_Device()
{
}

Wave_In_Device::~Wave_In_Device()
{
}

SNDERR Wave_In_Device::create_device(unsigned long flags)
{
    if (m_pdll_device == 0) {
        if (gbl_fntable[CREATEWAVEINDEVICE] != 0)
            return ((PFNCREATEWAVEINDEVICE)gbl_fntable[CREATEWAVEINDEVICE])(&m_pdll_device, flags);
        return SNDERR_NOT_LOADED;
    }
    return SNDERR_ALREADY_OPEN;
}

SNDERR Wave_In_Device::delete_device()
{
    if (m_pdll_device != 0) {
        if (gbl_fntable[DELETEWAVEINDEVICE] != 0) {
            ((PFNDELETEWAVEINDEVICE)gbl_fntable[DELETEWAVEINDEVICE])();
            m_pdll_device = 0;
            return SNDERR_OK;
        }
    }
    return SNDERR_NOT_LOADED;
}

SNDERR Wave_In_Device::init(HWND game_hwnd, unsigned long flags)
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

SNDERR Wave_In_Device::get_description(unsigned long id, char *desc_string, unsigned long buffsize)
{
    return SNDERR_OK;
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

int SndNodeList::Append(void *payload)
{
    SndNode *n = new SndNode(payload);
    if (m_tail) {
        m_tail->m_next = n;
        n->m_prev = m_tail;
        m_tail = n;
    } else {
        m_head = n;
        m_tail = n;
    }
    return ++m_count;
}

inline void Wave_In_Device::timer_unhook()
{
}

unsigned long Wave_In_Device::get_ndevices()
{
    return 0;
}

SNDERR Wave_In_Device::select(unsigned long id)
{
    return SNDERR_OK;
}

inline void Wave_In_Device::set_volume(unsigned long)
{
}

inline unsigned long Wave_In_Device::get_volume()
{
    return 0;
}

inline void Wave_In_Device::set_pan(int)
{
}

inline SNDERR Wave_In_Device::fade(unsigned long)
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

inline void Wave_In_Device::add_to_time_past(unsigned long mstime)
{
}

inline unsigned long Wave_In_Device::get_time_past()
{
    return 0;
}

void Wave_In_Device::suspend()
{
}

void Wave_In_Device::restart()
{
}

SNDERR Wave_In_Device::get_caps(unsigned long id)
{
    return SNDERR_OK;
}

inline void Wave_In_Device::reset_time_past()
{
}
