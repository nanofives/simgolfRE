#include <dsound.h>

#include "sound.h"
#include "sounddevice.h"
#include "dlist.h"
#include "dsb.h"
#include "wavefile.h"

CDSB *CDSMgr::gbl_dsb_head = 0;
CDSB *CDSMgr::gbl_dsb_tail = 0;
HWND CDSMgr::m_game_hwnd = 0;
unsigned CDSMgr::m_device = 0;

#pragma intrinsic(strcpy, strlen)


static inline SNDERR LinkToGlobalList(CDSB *v)
{
    if (CDSMgr::gbl_dsb_tail == 0) {
        CDSMgr::gbl_dsb_head = v;
        CDSMgr::gbl_dsb_tail = v;
        v->set_prev(0);
        v->set_next(0);
    } else {
        v->set_prev(CDSMgr::gbl_dsb_tail);
        CDSMgr::gbl_dsb_tail->set_next(v);
        CDSMgr::gbl_dsb_tail = v;
        v->set_next(0);
    }
    return SNDERR_OK;
}

static inline void UnlinkFromBufferList(CDSB *v)
{
    CDSB *g = CDSMgr::gbl_dsb_head;
    while (g) {
        if (g == v)
            break;
        g = g->m_pnext;
    }
    if (g) {
        CDSB *gp = g->m_pprev;
        CDSB *gn = g->m_pnext;
        if (gp)
            gp->m_pnext = gn;
        else
            CDSMgr::gbl_dsb_head = (CDSB *)gn;
        if (gn)
            gn->m_pprev = gp;
        else
            CDSMgr::gbl_dsb_tail = (CDSB *)gp;
        g->m_pprev = 0;
        g->m_pnext = 0;
    }
}

static inline void UnreserveHwVoice(CDSMgr *ss)
{
    ss->m_num_basehwbuffers--;
    ss->m_max_duphwbuffers = ss->m_max_hwbuffers - ss->m_num_basehwbuffers;
}

static inline void ReleaseEvictedBuffer(CDSMgr *ss, CDSB *dead)
{
    CDSB *g;

    if (ss->m_flags.deviceinitflag) {
        g = CDSMgr::gbl_dsb_head;
        while (g) {
            if (g == dead)
                break;
            g = g->m_pnext;
        }
        if (g)
            ss->detach_dsb(g);
        if (dead->isdsbtype(4))
            ss->m_num_basehwbuffers--;
        if (!dead->destroy())
            delete dead;
    }
}

static inline int EvictIdleBuffer(CDSMgr *ss)
{
    Node<CDSB> *nd;
    CDSB *buf;
    CDSB *dead;

    nd = ss->m_dup_hwdsb_list.get_head();
    while (nd) {
        buf = nd->get_obj();
        if (!buf->is_playing())
            break;
        nd = nd->get_next();
    }
    if (nd == 0)
        return 0;
    buf->m_pwf_owner->remove_dup_hwdsb(buf);
    dead = ss->m_dup_hwdsb_list.remove(buf);
    if (dead == 0)
        return 0;
    ReleaseEvictedBuffer(ss, dead);
    ss->m_num_duphwbuffers--;
    return 1;
}


CDSMgr::CDSMgr()
{
    m_nreleases = 0;
    m_lpds = 0;
    m_devlist = 0;
    m_pcurdev = 0;
    memset(&m_flags, 0, sizeof(m_flags));
    m_ndsdevices = 0;
    m_volume = 0x7f;
    m_num_basehwbuffers = 0;
    m_num_duphwbuffers = 0;
    m_max_basehwbuffers = 0;
    m_max_hwbuffers = 0;
    m_nduphwbuffers_per_sound = 2;
    m_pds3d_listener = 0;
    m_peax_listener_dsb = 0;
    m_posx = 0;
    m_posy = 0;
    m_posz = 0;
    memset(&m_saved_eax, 0, sizeof(m_saved_eax));
    memset(&m_dscaps, 0, sizeof(m_dscaps));
    dsb_list = 0;
}

CDSMgr::~CDSMgr()
{
    DSDevice_tag *pcur = m_devlist;
    while (pcur) {
        if (pcur->description) {
            delete pcur->description;
            pcur->description = 0;
        }
        if (pcur->module) {
            delete pcur->module;
            pcur->module = 0;
        }
        if (pcur->lpguid) {
            delete pcur->lpguid;
            pcur->lpguid = 0;
        }
        DSDevice_tag *ptmp = pcur;
        pcur = pcur->pnext;
        delete ptmp;
    }
    m_devlist = 0;
    m_pcurdev = 0;
    destroy();
    CoUninitialize();
}

SNDERR CDSMgr::init(unsigned int flags, unsigned long ms_rate)
{
    if (m_lpds != 0 || m_devlist != 0 || m_pcurdev != 0)
        return SNDERR_ALREADY_OPEN;
    if (ms_rate == 0)
        return SNDERR_BAD_PARAM;
    m_ms_rate = ms_rate;
    if (flags & 0x20) {
        m_flags.is_3d = 1;
        if (flags & 0x40)
            m_flags.eax = 1;
    }
    int dsretval = DirectSoundEnumerateA((LPDSENUMCALLBACKA)ds_enum_callback, this);
    if (dsretval != 0) {
        CDSB::dserrhandler(dsretval);
        return SNDERR_BAD_PARAM;
    }
    SNDERR retval = set_pcur(CDSMgr::m_device);
    if (retval == SNDERR_OK)
        m_flags.enuminitflag = 1;
    return retval;
}

SNDERR CDSMgr::init(unsigned long device_id, unsigned int flags, unsigned long ms_rate)
{
    init(flags, ms_rate);
    if (device_id >= m_ndsdevices)
        device_id = 0;
    SNDERR retval = set_pcur(device_id);
    if (retval == SNDERR_OK)
        m_flags.enuminitflag = 1;
    return retval;
}

SNDERR CDSMgr::set_pcur(unsigned long devnum)
{
    DSDevice_tag *pcur;
    unsigned long i;

    if (!m_ndsdevices)
        return SNDERR_NO_DEVICE;
    if (devnum >= m_ndsdevices)
        return SNDERR_BAD_PARAM;
    if (m_pcurdev)
        return SNDERR_NOCANDO;
    pcur = m_devlist;
    for (i = 0; i < m_ndsdevices; i++) {
        m_pcurdev = pcur;
        if (i == devnum)
            break;
        pcur = pcur->pnext;
    }
    return SNDERR_OK;
}

void CDSMgr::suspend()
{
}
void CDSMgr::restart()
{
}

unsigned long CDSMgr::get_hw_mem_size()
{
    if (m_lpds) {
        m_lpds->GetCaps(&m_dscaps);
        return m_dscaps.dwFreeHwMemBytes;
    }
    return 0;
}

SNDERR CDSMgr::get_description(unsigned long id, char *buff, unsigned long buff_size)
{
    unsigned long i;
    DSDevice_tag *curdev;

    if (id >= m_ndsdevices)
        return SNDERR_BAD_PARAM;
    curdev = m_devlist;
    if (curdev == 0)
        return SNDERR_NO_DEVICE;
    i = 0;
    while (i < id && curdev != 0) {
        curdev = curdev->pnext;
        i++;
    }
    if (curdev == 0)
        return SNDERR_NO_DEVICE;
    strncpy(buff, curdev->description, buff_size);
    return SNDERR_OK;
}

int __stdcall CDSMgr::ds_enum_callback(GUID *lpGuid, char *lpstrDescription, char *lpstrModule, void *lpContext)
{
    static int device = 0;
    DSDevice_tag *pnew = new DSDevice_tag;
    ((CDSMgr *)lpContext)->m_ndsdevices++;
    DSDevice_tag *pcur = ((CDSMgr *)lpContext)->m_devlist;
    if (pcur == 0) {
        ((CDSMgr *)lpContext)->m_devlist = pnew;
    } else {
        while (pcur->pnext != 0)
            pcur = pcur->pnext;
        pcur->pnext = pnew;
    }
    pnew->description = new char[strlen(lpstrDescription) + 1];
    strcpy(pnew->description, lpstrDescription);
    pnew->module = new char[strlen(lpstrModule) + 1];
    strcpy(pnew->module, lpstrModule);
    if (lpGuid != 0) {
        pnew->lpguid = new GUID;
        *pnew->lpguid = *lpGuid;
    } else {
        pnew->lpguid = 0;
    }
    pnew->lpcontext = lpContext;
    pnew->pnext = 0;
    device++;
    return TRUE;
}

void CDSMgr::destroy()
{
    CDSB *curpdsb;

    curpdsb = CDSMgr::gbl_dsb_head;
    while (curpdsb) {
        detach_dsb(curpdsb);
        curpdsb->destroy();
        delete curpdsb;
        curpdsb = CDSMgr::gbl_dsb_head;
    }

    if (m_flags.deviceinitflag)
        m_primary_dsb.destroy();

    if (m_peax_listener_dsb) {
        m_peax_listener_dsb->destroy();
        delete m_peax_listener_dsb;
        m_peax_listener_dsb = 0;
    }

    if (m_lpds) {
        m_lpds->Release();
        m_lpds = 0;
    }
    m_pcurdev = 0;
    ++m_nreleases;
}

void CDSMgr::disable()
{
    CDSB *curpdsb = CDSMgr::gbl_dsb_head;
    while (curpdsb != 0) {
        if (curpdsb->m_pwf_owner != 0) {
            m_pwf_hwbuffers.add_to_end(curpdsb->m_pwf_owner);
            curpdsb->m_pwf_owner->unload();
        } else {
            detach_dsb(curpdsb);
            curpdsb->release();
            delete curpdsb;
        }
        curpdsb = CDSMgr::gbl_dsb_head;
    }

    curpdsb = m_dup_hwdsb_list.remove_head();
    while (curpdsb)
        curpdsb = m_dup_hwdsb_list.remove_head();

    if (m_flags.deviceinitflag)
        m_primary_dsb.destroy();

    if (m_peax_listener_dsb) {
        m_peax_listener_dsb->destroy();
        delete m_peax_listener_dsb;
        m_peax_listener_dsb = 0;
    }

    if (m_lpds)
        m_lpds->Release();
    m_lpds = 0;
    ++m_nreleases;
}

SNDERR CDSMgr::create(HWND hwnd, unsigned long dsclflags)
{
    WAVEFORMATEX ideal_3d_primary_format;
    WAVEFORMATEX ideal_primary_format;
    long dserr;
    SNDERR retval;
    unsigned long primary_flags;
    CWaveFile *pwf;

    if (hwnd == 0)
        return SNDERR_BAD_PARAM;

    if (m_lpds != 0)
        return SNDERR_ALREADY_OPEN;

    if (m_flags.enuminitflag) {
        if (dsclflags == 0)
            dsclflags = DSSCL_PRIORITY;

        if (m_pcurdev == 0)
            return SNDERR_UNINITIALIZED;

        dserr = CoInitialize(0);
        if (dserr != S_OK && dserr != S_FALSE) {
            m_lpds = 0;
            return SNDERR_BAD_COM_INIT;
        }

        --m_nreleases;
        dserr = CoCreateInstance(CLSID_DirectSound, 0, CLSCTX_INPROC_SERVER, IID_IDirectSound, (void **)&m_lpds);
        if (dserr != 0 || m_lpds == 0 || (dserr = m_lpds->Initialize(m_pcurdev->lpguid)) == 0)
            dserr = m_lpds->SetCooperativeLevel(hwnd, dsclflags);
        if (dserr != 0) {
            CDSB::dserrhandler(dserr);
            destroy();
            return SNDERR_NO_DEVICE;
        }

        m_game_hwnd = hwnd;
        m_dscaps.dwSize = sizeof(m_dscaps);
        dserr = m_lpds->GetCaps(&m_dscaps);
        if (dserr != 0)
            CDSB::dserrhandler(dserr);

        if ((m_dscaps.dwFlags & DSCAPS_EMULDRIVER) == 0) {
            m_num_basehwbuffers = 0;
            m_max_hwbuffers = m_dscaps.dwFreeHwMixingStaticBuffers;
            m_max_basehwbuffers = m_dscaps.dwFreeHwMixingStaticBuffers;
            m_max_duphwbuffers = m_dscaps.dwFreeHwMixingStaticBuffers;
            m_num_duphwbuffers = 0;
            if (m_dscaps.dwMaxHw3DStreamingBuffers < 2 && m_flags.is_3d) {
                m_flags.is_3d = 0;
                m_flags.eax = 0;
            }
        } else {
            m_flags.is_3d = 0;
            m_flags.eax = 0;
        }

        primary_flags = DSBCAPS_PRIMARYBUFFER;

        ideal_primary_format.wFormatTag = WAVE_FORMAT_PCM;
        ideal_primary_format.nChannels = 2;
        ideal_primary_format.nSamplesPerSec = 44100;
        ideal_primary_format.nAvgBytesPerSec = 176400;
        ideal_primary_format.nBlockAlign = 4;
        ideal_primary_format.wBitsPerSample = 16;
        ideal_primary_format.cbSize = 0;

        ideal_3d_primary_format.wFormatTag = WAVE_FORMAT_PCM;
        ideal_3d_primary_format.nChannels = 1;
        ideal_3d_primary_format.nSamplesPerSec = 22050;
        ideal_3d_primary_format.nAvgBytesPerSec = 44100;
        ideal_3d_primary_format.nBlockAlign = 2;
        ideal_3d_primary_format.wBitsPerSample = 16;
        ideal_3d_primary_format.cbSize = 0;

        if (m_flags.is_3d) {
            m_primary_dsb.set_format(&ideal_3d_primary_format);
            primary_flags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D;
        } else {
            m_primary_dsb.set_format(&ideal_primary_format);
        }

        retval = m_primary_dsb.set_type(1);
        if (retval != 0) {
            destroy();
            return retval;
        }

        m_primary_dsb.set_dsb_flags(primary_flags);
        retval = m_primary_dsb.create(m_lpds, m_ms_rate);
        if (retval != 0) {
            destroy();
            return retval;
        }

        m_primary_dsb.m_pdsmgr = this;
        m_flags.deviceinitflag = 1;

        pwf = m_pwf_hwbuffers.remove_head();
        while (pwf != 0) {
            if (m_max_hwbuffers != 0)
                pwf->load(pwf->get_fname());
            pwf = m_pwf_hwbuffers.remove_head();
        }

        m_dsclflags = dsclflags;
        return SNDERR_OK;
    }

    return SNDERR_NO_DEVICE;
}

SNDERR CDSMgr::get_eax(EAX_REVERB_PROPERTIES *peax)
{
    unsigned long eax_size = 0;
    if (!m_flags.eax)
        return SNDERR_NO_SUPPORT;
    return SNDERR_NO_SUPPORT;
}

SNDERR CDSMgr::set_eax(EAX_REVERB_PROPERTIES *peax)
{
    return SNDERR_OK;
}

SNDERR CDSMgr::init_A3D()
{
    GUID *lpguid = 0;
    long hr = 0;
    if (m_pcurdev)
        lpguid = m_pcurdev->lpguid;
    return SNDERR_OK;
}
SNDERR CDSMgr::uninit_A3D()
{
    return SNDERR_OK;
}

SNDERR CDSMgr::set_hwnd(HWND hwnd)
{
    if (!m_lpds)
        return SNDERR_UNINITIALIZED;

    long dserr = m_lpds->SetCooperativeLevel(hwnd, m_dsclflags);
    if (dserr) {
        CDSB::dserrhandler(dserr);
        destroy();
        return SNDERR_NO_DEVICE;
    }
    m_game_hwnd = hwnd;
    return SNDERR_OK;
}

SNDERR CDSMgr::create_dsb(CDSB **ppdsb)
{
    SNDERR retval;

    if (!m_flags.deviceinitflag)
        return SNDERR_NO_DEVICE;

    *ppdsb = new CDSB;
    retval = (*ppdsb)->create(m_lpds, m_ms_rate);
    if (retval == 0) {
        retval = LinkToGlobalList(*ppdsb);
    } else {
        delete *ppdsb;
        *ppdsb = 0;
    }
    return retval;
}

SNDERR CDSMgr::duplicate_dsb(CWaveFile *powner, CDSB *pdsb, CDSB **ppnew_dsb)
{
    SNDERR retval;
    long ds_return = 0;

    if (!m_flags.deviceinitflag || m_lpds == 0)
        return SNDERR_NO_DEVICE;
    if (m_num_duphwbuffers == m_max_duphwbuffers)
        return SNDERR_NO_RESOURCES;

    *ppnew_dsb = new CDSB;
    (*ppnew_dsb)->set_dsmgr(this);
    ds_return = m_lpds->DuplicateSoundBuffer(pdsb->m_lpdsb, &(*ppnew_dsb)->m_lpdsb);
    if (ds_return == 0) {
        LinkToGlobalList(*ppnew_dsb);
        m_dup_hwdsb_list.add_to_end(*ppnew_dsb);

        (*ppnew_dsb)->set_owner(powner);
        powner->add_dup_hwdsb(*ppnew_dsb);

        if (pdsb->is_3d()) {
            (*ppnew_dsb)->init_3d();
            (*ppnew_dsb)->init_reverb();
        }

        m_num_duphwbuffers++;
        retval = SNDERR_OK;
    } else {
        CDSB::dserrhandler(ds_return);
        delete *ppnew_dsb;
        *ppnew_dsb = 0;
        retval = SNDERR_DSB_CREATE;
    }
    return retval;
}

SNDERR CDSMgr::free_a_duphwdsb()
{
    CDSB *pcurdsb;
    Node<CDSB> *pcurnode;
    CWaveFile *pwf;

    for (pcurnode = m_dup_hwdsb_list.get_head(); pcurnode; pcurnode = pcurnode->get_next()) {
        pcurdsb = pcurnode->get_obj();
        if (!pcurdsb->is_playing()) {
            if (pcurnode == 0)
                return SNDERR_NOCANDO;

            pwf = pcurdsb->m_pwf_owner;
            pwf->remove_dup_hwdsb(pcurdsb);

            pcurdsb = m_dup_hwdsb_list.remove(pcurdsb);
            if (pcurdsb == 0)
                return SNDERR_NOT_PRESENT;

            if (m_flags.deviceinitflag) {
                UnlinkFromBufferList(pcurdsb);
                if (pcurdsb->isdsbtype(4))
                    m_num_basehwbuffers--;
                if (pcurdsb->destroy() == 0)
                    delete pcurdsb;
            }

            m_num_duphwbuffers--;
            return SNDERR_OK;
        }
    }
    return SNDERR_NOCANDO;
}

SNDERR CDSMgr::steal_duplicate_hwdsb(CWaveFile *newowner, CDSB **ppdsb)
{
    CDSB *pcurdsb;
    Node<CDSB> *pcurnode;
    SNDERR retval;
    CWaveFile *pwf;

    if (newowner == 0)
        return SNDERR_BAD_PARAM;

    for (pcurnode = m_dup_hwdsb_list.get_head(); pcurnode; pcurnode = pcurnode->get_next()) {
        pcurdsb = pcurnode->get_obj();
        if (!pcurdsb->is_playing()) {
            if (pcurnode == 0)
                return SNDERR_NOT_PRESENT;

            pwf = pcurdsb->m_pwf_owner;
            pwf->remove_dup_hwdsb(pcurdsb);

            pcurdsb = m_dup_hwdsb_list.remove(pcurdsb);
            if (pcurdsb == 0)
                return SNDERR_NOT_PRESENT;

            if (m_flags.deviceinitflag) {
                UnlinkFromBufferList(pcurdsb);
                if (pcurdsb->isdsbtype(4))
                    m_num_basehwbuffers--;
                if (pcurdsb->destroy() == 0)
                    delete pcurdsb;
            }

            m_num_duphwbuffers--;
            retval = duplicate_dsb(newowner, newowner->m_phwdsb, ppdsb);
            if (retval)
                *ppdsb = 0;
            else
                (*ppdsb)->m_pwf_owner = newowner;
            return retval;
        }
    }
    return SNDERR_NOT_PRESENT;
}

SNDERR CDSMgr::create_dsb(CDSB **ppdsb, DSBUFFERDESC *lpdsbdesc)
{
    SNDERR retval;

    if (!m_flags.deviceinitflag)
        return SNDERR_NO_DEVICE;
    if (lpdsbdesc->dwFlags & 4) {
        if (m_num_basehwbuffers >= m_max_basehwbuffers)
            return SNDERR_NO_RESOURCES;
        m_num_basehwbuffers++;
        m_max_duphwbuffers = m_max_hwbuffers - m_num_basehwbuffers;
        if (m_num_duphwbuffers > m_max_duphwbuffers) {
            if (!EvictIdleBuffer(this)) {
                UnreserveHwVoice(this);
                return SNDERR_NO_RESOURCES;
            }
        }
    }
    *ppdsb = new CDSB;
    (*ppdsb)->m_dsbdesc = *lpdsbdesc;
    (*ppdsb)->m_pdsmgr = this;
    retval = (*ppdsb)->create(m_lpds, m_ms_rate);
    if (retval == 0) {
        retval = LinkToGlobalList(*ppdsb);
    } else {
        if (lpdsbdesc->dwFlags & 4)
            UnreserveHwVoice(this);
        delete *ppdsb;
        *ppdsb = 0;
    }
    return retval;
}

SNDERR CDSMgr::create_dsb(CDSB **ppdsb, unsigned long flags)
{
    SNDERR retval;

    if (!m_flags.deviceinitflag)
        return SNDERR_NO_DEVICE;

    *ppdsb = new CDSB;
    (*ppdsb)->set_dsb_flags(flags);
    (*ppdsb)->set_dsmgr(this);
    retval = (*ppdsb)->create(m_lpds, m_ms_rate);
    if (retval == 0) {
        retval = LinkToGlobalList(*ppdsb);
    } else {
        delete *ppdsb;
        *ppdsb = 0;
    }
    return retval;
}

SNDERR CDSMgr::create_dsb(CDSB **ppdsb, unsigned long flags, unsigned long size)
{
    SNDERR retval;
    unsigned used;
    unsigned max;

    if (!m_flags.deviceinitflag)
        return SNDERR_NO_DEVICE;
    if (flags & 4) {
        used = m_num_basehwbuffers;
        max = m_max_basehwbuffers;
        if (used >= max)
            return SNDERR_NO_RESOURCES;
        used++;
        m_num_basehwbuffers = used;
        max = m_max_hwbuffers - used;
        m_max_duphwbuffers = max;
        if (m_num_duphwbuffers > max) {
            if (!EvictIdleBuffer(this)) {
                UnreserveHwVoice(this);
                return SNDERR_NO_RESOURCES;
            }
        }
    }
    *ppdsb = new CDSB;
    (*ppdsb)->m_dsbdesc.dwFlags = flags;
    (*ppdsb)->set_buffer_size(size);
    (*ppdsb)->m_pdsmgr = this;
    retval = (*ppdsb)->create(m_lpds, m_ms_rate);
    if (retval == 0) {
        retval = LinkToGlobalList(*ppdsb);
    } else {
        if (flags & 4)
            UnreserveHwVoice(this);
        delete *ppdsb;
        *ppdsb = 0;
    }
    return retval;
}

SNDERR CDSMgr::create_dsb(CDSB **ppdsb, WAVEFORMATEX *pwfx, unsigned long flags, unsigned long size)
{
    SNDERR retval;
    unsigned used;
    unsigned max;

    if (!m_flags.deviceinitflag)
        return SNDERR_NO_DEVICE;
    if (pwfx == 0)
        return SNDERR_BAD_PARAM;
    if (flags & 4) {
        used = m_num_basehwbuffers;
        max = m_max_basehwbuffers;
        if (used >= max)
            return SNDERR_NO_RESOURCES;
        used++;
        m_num_basehwbuffers = used;
        max = m_max_hwbuffers - used;
        m_max_duphwbuffers = max;
        if (m_num_duphwbuffers > max) {
            if (!EvictIdleBuffer(this)) {
                UnreserveHwVoice(this);
                return SNDERR_NO_RESOURCES;
            }
        }
    }
    *ppdsb = new CDSB;
    (*ppdsb)->m_dsbdesc.dwFlags = flags;
    (*ppdsb)->set_buffer_size(size);
    (*ppdsb)->set_format(pwfx);
    (*ppdsb)->m_pdsmgr = this;
    retval = (*ppdsb)->create(m_lpds, m_ms_rate);
    if (retval == 0) {
        LinkToGlobalList(*ppdsb);
    } else {
        if (flags & 4)
            UnreserveHwVoice(this);
        delete *ppdsb;
        *ppdsb = 0;
    }
    return retval;
}

SNDERR CDSMgr::destroy_dsb(CDSB *pdsb)
{
    if (!m_flags.deviceinitflag)
        return SNDERR_NO_DEVICE;
    if (pdsb == 0)
        return SNDERR_BAD_PARAM;

    CDSB *curdsb = CDSMgr::gbl_dsb_head;
    while (curdsb != 0 && curdsb != pdsb)
        curdsb = curdsb->get_next();
    detach_dsb(curdsb);

    if (pdsb->isdsbtype(4))
        --m_num_basehwbuffers;

    if (pdsb->destroy() == 0)
        delete pdsb;
    return SNDERR_OK;
}

SNDERR CDSMgr::destroy_all_dsbs()
{
    if (!m_flags.deviceinitflag)
        return SNDERR_NO_DEVICE;

    CDSB *curdsb = CDSMgr::gbl_dsb_head;
    while (curdsb != 0) {
        detach_dsb(curdsb);

        curdsb->destroy();
        if (curdsb != 0)
            delete curdsb;
        curdsb = CDSMgr::gbl_dsb_head;
    }
    return SNDERR_OK;
}

SNDERR CDSMgr::attach_dsb(CDSB *pdsb)
{
    SNDERR retval = LinkToGlobalList(pdsb);
    return retval;
}

SNDERR CDSMgr::detach_dsb(CDSB *pdsb)
{
    CDSB *pprev;
    CDSB *pnext;
    if (pdsb == 0)
        return SNDERR_BAD_PARAM;
    pprev = pdsb->get_prev();
    pnext = pdsb->get_next();
    if (pprev)
        pprev->set_next(pnext);
    else
        CDSMgr::gbl_dsb_head = pnext;
    if (pnext)
        pnext->set_prev(pprev);
    else
        CDSMgr::gbl_dsb_tail = pprev;
    pdsb->set_prev(0);
    pdsb->set_next(0);
    return SNDERR_OK;
}

void CDSMgr::set_volume(unsigned long volume)
{
    CDSB *pcur = CDSMgr::gbl_dsb_head;
    m_volume = volume;
    for (; pcur; pcur = pcur->get_next())
        pcur->set_volume(volume);
}

SNDERR CDSMgr::set_listener_position(float x, float y, float z)
{
    long ds_return;

    if (m_primary_dsb.get_3d_listener()) {
        ds_return = m_primary_dsb.get_3d_listener()->SetPosition(x, y, z, 0);
        if (ds_return)
            return (SNDERR)CDSB::dserrhandler(ds_return);
    }
    m_posx = x;
    m_posy = y;
    m_posz = z;
    return SNDERR_OK;
}
