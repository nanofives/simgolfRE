#include "sounddevice.h"
#include "wavefile.h"
#include "wavemgr.h"
#include "ctimer.h"

#pragma intrinsic(strcpy, strlen)

CWaveMgr::CWaveMgr()
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_hwnd = 0;
    m_msgs_pending = 0;
    m_nvoices = 1;
    m_midi_note = 0;
    memset(m_voices, 0, sizeof m_voices);
    m_plinked_wave = 0;
    m_plinked_semaphore = 0;
    set_type(SOUNDTYPE_WAVE);
}

CWaveMgr::CWaveMgr(HWND hwnd)
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_hwnd = hwnd;
    m_nvoices = 1;
    m_plinked_wave = 0;
    m_plinked_semaphore = 0;
    CWaveFile::set_hwnd(hwnd);
    memset(m_voices, 0, sizeof m_voices);
    set_type(SOUNDTYPE_WAVE);
}

CWaveMgr::~CWaveMgr()
{
    int i = 0;

    if (is_GT_request_pending()) {
        int bad;
        bad = 1;
    }
    if (m_nvoices > 1) {
        for (i = 1; i < m_nvoices; i++) {
            if (m_voices[i - 1]) {
                if (m_voices[i - 1]->is_GT_request_pending()) {
                    int bad;
                    bad = 1;
                }
                delete m_voices[i - 1];
                m_voices[i - 1] = 0;
            }
        }
    }
    if (CWaveFile::is_loaded())
        CWaveFile::unload();
    if (m_plinked_wave) {
        delete m_plinked_wave;
        m_plinked_wave = 0;
    }
}

SNDERR CWaveMgr::reload()
{
    return (SNDERR)CWaveFile::reload();
}

SNDERR CWaveMgr::load(const char *fname)
{
    SNDERR retval = (SNDERR)CWaveFile::load(fname);
    if (retval == SNDERR_OK && m_nvoices > 1 && m_nvoices < 3) {
        for (int i = 1; i < m_nvoices; i++) {
            CWaveMgr *pnew_voice = new CWaveMgr();
            retval = pnew_voice->load(fname);
            if (retval != SNDERR_OK)
                break;
            m_voices[i - 1] = pnew_voice;
        }
    }
    return retval;
}

SNDERR CWaveMgr::direct_unload()
{
    SNDERR retval;

    if (is_playing()) {
        int bad;
        bad = 1;
    }
    if (m_nvoices > 1) {
        for (int i = 1; i < m_nvoices; i++) {
            if (m_voices[i - 1]) {
                if (m_voices[i - 1]->is_GT_request_pending()) {
                    int bad;
                    bad = 1;
                }
                m_voices[i - 1]->direct_unload();
                delete m_voices[i - 1];
                m_voices[i - 1] = 0;
            }
        }
        m_nvoices = 1;
    }
    if (CWaveFile::is_loaded()) {
        retval = CWaveFile::unload();
        m_sf.loaded = 0;
    } else {
        retval = SNDERR_NOT_LOADED;
    }
    return retval;
}

SNDERR CWaveMgr::unload()
{
    SNDERR retval = SNDERR_OK;

    if (!is_loaded())
        return retval;

    if (gbl_pwave_device != 0)
        retval = gbl_pwave_device->queue_request(this, REQUEST_UNLOAD);
    else
        retval = SNDERR_UNINITIALIZED;
    return retval;
}

SNDERR CWaveMgr::reset()
{
    set_volume(Wave::get_volume());
    CWaveFile::reset();
    return SNDERR_OK;
}

SNDERR CWaveMgr::fade()
{
    SNDERR return_val = CWaveFile::fade();
    if (is_mpeg())
        gbl_pwave_device->queue_request(this, REQUEST_FADE);
    return return_val;
}

SNDERR CWaveMgr::fade_in()
{
    SNDERR return_val = CWaveFile::fade_in();
    return return_val;
}

void CWaveMgr::free_all()
{
    CWaveMgr *pwmgr;
    while ((pwmgr = (CWaveMgr *)Sound::get_list()) != 0) {
        pwmgr->stop();
        pwmgr->unload();
    }
}

SNDERR CWaveMgr::play()
{
    SNDERR retval = SNDERR_OK;

    if (is_active() && is_static() == 0)
        return SNDERR_ALREADY_PLAYING;

    if (gbl_pwave_device != 0 && gbl_pwave_device->is_disabled() == 0) {
        if (is_GT_request_pending())
            return SNDERR_BUSY;

        if (CWaveFile::is_loaded() == 0) {
            reset_flags();
            if (is_dynamic_buffer()) {
                retval = load(Sound::get_fname());
                if (retval != SNDERR_OK)
                    return SNDERR_BAD_FILE_OPEN;
            } else {
                return SNDERR_NOT_LOADED;
            }
        }

        if (is_static()) {
            if (is_hwbuffer() == 0)
                CWaveFile::play();
        }

        if (is_active())
            return SNDERR_OK;

        if (is_mpeg() || is_hwstreaming()) {
            set_hwvolume(Wave::get_volume());
            CWaveFile::play();
        }

        if (is_static() && is_hwbuffer() != 0 && is_dynamic_buffer())
            gbl_pwave_device->queue_request(this, REQUEST_PLAY);

        if (is_hwbuffer() == 0 || is_discard() || is_hwstreaming())
            gbl_pwave_device->queue_request(this, REQUEST_PLAY);

        return SNDERR_OK;
    }
    return SNDERR_NO_DRIVER;
}

SNDERR CWaveMgr::play(int ibuffer)
{
    if (is_static() == 0)
        return SNDERR_NO_SUPPORT;
    if (CWaveFile::is_loaded() == 0) {
        reset_flags();
        if (is_dynamic_buffer()) {
            if (load(Sound::get_fname()) != 0)
                return SNDERR_BAD_FILE_OPEN;
        } else {
            return SNDERR_NOT_LOADED;
        }
    }
    if (gbl_ptimer != 0)
        gbl_ptimer->start();
    CWaveFile::play(ibuffer);
    if (is_active() == 0) {
        set_active(1);
        attach();
    }
    return SNDERR_OK;
}

SNDERR CWaveMgr::play(int *pinstance)
{
    SNDERR retval = SNDERR_OK;

    *pinstance = -1;

    if (is_static() == 0)
        return SNDERR_WRONG_TYPE;

    if (CWaveFile::is_loaded() == 0) {
        reset_flags();
        if (is_dynamic_buffer()) {
            retval = load(Sound::get_fname());
            if (retval != SNDERR_OK)
                return SNDERR_BAD_FILE_OPEN;
        } else {
            return SNDERR_NOT_LOADED;
        }
    }

    retval = CWaveFile::play(pinstance);
    if (retval == SNDERR_OK) {
        if (is_active())
            return SNDERR_OK;
        gbl_pwave_device->queue_request(this, REQUEST_PLAY);
    }

    if (is_active())
        return SNDERR_OK;

    if (is_hwbuffer() == 0 || is_discard() || is_hwstreaming())
        gbl_pwave_device->queue_request(this, REQUEST_PLAY);

    return retval;
}

SNDERR CWaveMgr::stop()
{
    if (!CWaveFile::is_loaded())
        return SNDERR_NOT_LOADED;

    if (gbl_pwave_device != 0)
        gbl_pwave_device->queue_request(this, REQUEST_STOP);

    return SNDERR_OK;
}

SNDERR CWaveMgr::release()
{
    if (CWaveFile::is_loaded() == 0)
        return SNDERR_NOT_LOADED;
    CWaveFile::set_release(1);
    return SNDERR_OK;
}

SNDERR CWaveMgr::detach()
{
    Sound::detach();
    return SNDERR_OK;
}

void CWaveMgr::set_volume(int vol)
{
    double dBvol = 0;

    if (vol > 0x7f)
        vol = 0x7f;
    else if (vol < 0)
        vol = 0;

    Wave::set_volume(vol);

    if (is_hwbuffer())
        set_hwvolume(vol);

    if (vol != 0)
        ++vol;
    CWaveFile::set_volume(vol << 9);
}

void CWaveMgr::ramp(int dest_vol, int src_vol, unsigned int ms_time)
{
    if (dest_vol != 0 && dest_vol != -1) {
        if (dest_vol > 0x7f)
            dest_vol = 0x7f;
        else if (dest_vol < 0)
            dest_vol = 0;
        dest_vol = (dest_vol + 1) << 9;
    }
    if (src_vol != 0 && src_vol != -1) {
        if (src_vol > 0x7f)
            src_vol = 0x7f;
        else if (src_vol < 0)
            src_vol = 0;
        src_vol = (src_vol + 1) << 9;
    }
    CWaveFile::ramp(dest_vol, src_vol, ms_time);
}

void CWaveMgr::set_delay(unsigned long delay)
{
    CWaveFile::set_delay(delay);
}

void CWaveMgr::set_pan(int pan)
{
    Sound::set_pan(pan);
    CWaveFile::set_pan(pan);
}

const unsigned long CWaveMgr::pitch_table[25] = {32768, 34716, 36781,  38968,  41285,  43740,  46341, 49097, 52016,
                                                 55109, 58386, 61858,  65536,  69433,  73562,  77936, 82570, 87480,
                                                 92682, 98193, 104032, 110218, 116772, 123715, 131072};
const unsigned long CWaveMgr::ds_pitch_table[25] = {11025, 11680, 12375, 13111, 13890, 14716, 15591, 16518, 17501,
                                                    18541, 19644, 20812, 22050, 23361, 24750, 26222, 27781, 29433,
                                                    31183, 33037, 35002, 37083, 39288, 41624, 44100};
const double CWaveMgr::fp_pitch_table[25] = {0.5,
                                             0.5297315471796,
                                             0.5612310241547,
                                             0.5946035575014,
                                             0.6299605249474,
                                             0.667419927085,
                                             0.7071067811865,
                                             0.7491535384383,
                                             0.7937005259841,
                                             0.8408964152537,
                                             0.8908987181403,
                                             0.9438743126817,
                                             1.0,
                                             1.059463094359,
                                             1.122462048309,
                                             1.189207115003,
                                             1.259921049895,
                                             1.33483985417,
                                             1.414213562373,
                                             1.498307076877,
                                             1.587401051968,
                                             1.681792830507,
                                             1.781797436281,
                                             1.887748625363,
                                             2.0};

SNDERR CWaveMgr::set_pitch(int pitch)
{
    int index;
    int next_index;
    double base_fp_pitch;
    double inbetween_fp_pitch;
    int base_pitch;
    int frac;
    unsigned long new_pitch;
    double new_fp_pitch;
    int inbetween_pitch;

    if (pitch < -1200 || pitch > 1200)
        pitch = pitch % 1200;
    Wave::set_pitch(pitch);

    __asm {
        push eax
        push ecx
        push edx
        mov  eax, pitch
        cdq
        mov  ecx, 100
        idiv ecx
        cmp  edx, 0
        jge  done
        neg  edx
done: mov index, eax
        mov  frac, edx
        pop  edx
        pop  ecx
        pop  eax
    }

    index += 12;
    base_fp_pitch = 0.0;
    if (is_hwbuffer())
        base_pitch = ds_pitch_table[index];
    else {
        base_pitch = pitch_table[index];
        base_fp_pitch = fp_pitch_table[index];
    }

    next_index = (pitch < 0) ? index - 1 : index + 1;
    inbetween_fp_pitch = (fp_pitch_table[next_index] - fp_pitch_table[index]) * frac * 0.01;
    new_fp_pitch = base_fp_pitch + inbetween_fp_pitch;
    inbetween_pitch = pitch_table[next_index] - pitch_table[index];
    new_pitch = base_pitch - (int)((float)inbetween_pitch * frac * -0.01f);

    CWaveFile::set_pitch(new_pitch);
    CWaveFile::set_pitch(new_fp_pitch);
    return SNDERR_OK;
}

SNDERR CWaveMgr::set_attrib(unsigned long attrib)
{
    Wave::set_attrib(attrib);
    if (attrib & 0x001)
        set_static();
    if (attrib & 0x010)
        set_try_hwbuffer();
    if (attrib & 0x020)
        set_big_buffer();
    if (attrib & 0x040)
        set_3d();
    if (attrib & 0x100)
        set_try_hwstreaming();
    if (attrib & 0x200)
        set_shared_dsb();
    if (attrib & 0x002)
        set_loop_state(1);
    if (attrib & 0x400)
        CWaveFile::m_flags.attrib_400 = 1;
    return SNDERR_OK;
}

SNDERR CWaveMgr::set_link(char *fname, unsigned long *psemaphore)
{
    if (!gbl_pwave_device)
        return SNDERR_NO_DRIVER;
    if (!fname)
        return SNDERR_BAD_PARAM;
    char *pfname = new char[strlen(fname) + 1];
    strcpy(pfname, fname);
    CCriticalRequest *preq = new CCriticalRequest;
    preq->m_data1 = (int)pfname;
    preq->m_data2 = (int)psemaphore;
    preq->m_type = REQUEST_SET_LINK;
    preq->m_psound = this;
    gbl_pwave_device->queue_request(preq);
    return SNDERR_OK;
}

unsigned long CWaveMgr::get_attrib()
{
    unsigned long attrib = Wave::get_attrib();
    if (is_asr())
        attrib |= 8;
    return attrib;
}

unsigned long CWaveMgr::get_time(unsigned long instance)
{
    return CWaveFile::get_time(instance);
}

SNDERR CWaveMgr::set_position3d(float x, float y, float z)
{
    return CWaveFile::set_position3d(x, y, z);
}

SNDERR CWaveMgr::set_xpos(float x)
{
    return CWaveFile::set_xpos(x);
}

SNDERR CWaveMgr::set_reverb_mix(float mix)
{
    Wave::set_reverb_mix(mix);
    return CWaveFile::set_reverb_mix(mix);
}

SNDERR CWaveMgr::set_ypos(float y)
{
    return CWaveFile::set_ypos(y);
}
SNDERR CWaveMgr::set_zpos(float z)
{
    return CWaveFile::set_zpos(z);
}

unsigned int CWaveMgr::get_ms_length()
{
    return CWaveFile::get_ms_length();
}
