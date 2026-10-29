#include <windows.h>
#include <mmsystem.h>
#include "sound.h"
#include "sounddevice.h"
#include "dllwaveindevice.h"
#include "waveindevicemgr.h"
#include "ctimer.h"

Dll_Wave_In_Device *gbl_pwave_in_device = 0;

Dll_Wave_In_Device::Dll_Wave_In_Device()
{
    m_ptimer = 0;
    m_rate = 0;
    m_time_past = 0;
    m_master_volume = 0x7f;
    memset(&m_flags, 0, sizeof(m_flags));
    m_time_since_last_update = 0;
}

Dll_Wave_In_Device::~Dll_Wave_In_Device()
{
}

SNDERR __cdecl Dll_Wave_In_Device::create_device(Wave_In_Device **ppdev, unsigned long flags)
{
    if (gbl_pwave_in_device == 0) {
        gbl_pwave_in_device = new Dll_Wave_In_Device;
        *ppdev = gbl_pwave_in_device;
        return SNDERR_OK;
    }
    return SNDERR_ALREADY_OPEN;
}

SNDERR __cdecl Dll_Wave_In_Device::delete_device()
{
    delete gbl_pwave_in_device;
    gbl_pwave_in_device = 0;
    return SNDERR_OK;
}

SNDERR Dll_Wave_In_Device::start_record()
{
    SNDERR retval;

    if (m_pwidmgr == 0)
        return SNDERR_NO_DEVICE;
    retval = m_pwidmgr->enable();
    if (retval != SNDERR_OK)
        return retval;
    retval = m_pwidmgr->prepare_headers();
    if (retval != SNDERR_OK)
        return retval;
    retval = m_pwidmgr->add_buffers();
    if (retval != SNDERR_OK)
        return retval;
    return m_pwidmgr->start_record();
}

SNDERR Dll_Wave_In_Device::end_record()
{
    if (m_pwidmgr == 0)
        return SNDERR_NO_DEVICE;
    m_pwidmgr->stop_record();
    return m_pwidmgr->disable();
}

SNDERR Dll_Wave_In_Device::init(HWND game_hwnd, unsigned long flags)
{
    SNDERR retval = SNDERR_OK;
    unsigned long pcmbuffersize;
    unsigned long pcmnumbuffers;

    if (m_flags.initialized)
        return SNDERR_OK;

    pcmbuffersize = 0x168;
    pcmnumbuffers = 10;
    if (m_pwidmgr == 0) {
        m_pwidmgr = new WaveInDeviceMgr;
        retval = m_pwidmgr->init(pcmnumbuffers, pcmbuffersize);
        if (retval != SNDERR_OK) {
            delete m_pwidmgr;
            m_pwidmgr = 0;
            return retval;
        }
        set_game_hwnd(game_hwnd);
    }
    m_flags.initialized = 1;
    return SNDERR_OK;
}

SNDERR Dll_Wave_In_Device::set_vxw_key(signed char *key)
{
    return SNDERR_OK;
}

SNDERR Dll_Wave_In_Device::release()
{
    if (m_ptimer != 0) {
        m_ptimer->stop();
        gbl_ptimer->unregister_device(this);
    }
    if (m_pwidmgr != 0) {
        delete m_pwidmgr;
        m_pwidmgr = 0;
    }
    return SNDERR_OK;
}

void Dll_Wave_In_Device::suspend()
{
    if (m_ptimer != 0)
        gbl_ptimer->unregister_device(this);
}

void Dll_Wave_In_Device::restart()
{
    if (gbl_ptimer != 0) {
        m_ptimer = gbl_ptimer;
        gbl_ptimer->register_device(this);
        gbl_ptimer->start();
    }
}

SNDERR Dll_Wave_In_Device::select(unsigned long id)
{
    return SNDERR_OK;
}
void Dll_Wave_In_Device::update_sound()
{
    m_time_past = 0;
}
void Dll_Wave_In_Device::add_to_time_past(unsigned long ms_time)
{
    m_time_since_last_update = ms_time;
    m_time_past = m_time_past + ms_time;
}
unsigned long Dll_Wave_In_Device::get_ndevices()
{
    return 0;
}
SNDERR Dll_Wave_In_Device::get_description(unsigned long id, char *buffer, unsigned long buff_size)
{
    return SNDERR_OK;
}
SNDERR Dll_Wave_In_Device::hook_vtx(CVoiceTxMgr *pvtx)
{
    if (m_pwidmgr == 0)
        return SNDERR_UNINITIALIZED;
    return m_pwidmgr->hook_vtx(pvtx);
}
SNDERR Dll_Wave_In_Device::unhook_vtx()
{
    if (m_pwidmgr == 0)
        return SNDERR_UNINITIALIZED;
    return m_pwidmgr->unhook_vtx();
}
