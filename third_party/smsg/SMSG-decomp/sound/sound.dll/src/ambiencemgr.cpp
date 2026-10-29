#include "MidiSequence.h"
#include "sound.h"
#include "dlist.h"
#include "miditools.h"
#include "midifile.h"
#include "midimgr.h"

CAmbienceMgr::CAmbienceMgr()
{
    memset(m_fname, 0, sizeof m_fname);
    set_type(SOUNDTYPE_AMBIENCE);
    m_hfile = 0;
}

CAmbienceMgr::~CAmbienceMgr()
{
    m_control_load_list.delete_all();
}

SNDERR CAmbienceMgr::load(const char *fname)
{
    char *prgm_fname = 0;
    const unsigned long prgm = mmioFOURCC('p', 'r', 'g', 'm');
    const unsigned long chnl = mmioFOURCC('c', 'h', 'n', 'l');
    const unsigned long kmap = mmioFOURCC('k', 'm', 'a', 'p');
    const unsigned long trak = mmioFOURCC('t', 'r', 'a', 'k');
    const unsigned long glbl = mmioFOURCC('g', 'l', 'b', 'l');
    const unsigned long ctrl = mmioFOURCC('c', 't', 'r', 'l');

    unsigned long MThd = mmioStringToFOURCC("MThd", 0);
    ChannelCtrl cctrl;
    ProgramCtrl pctrl;
    TrackCtrl *ptrack_ctrl;
    unsigned int glblset_size = 0;
    GlblMidiSet glblset;
    DllTrackSet *pnew_trackset = 0;
    DList<TrackCtrl> track_ctrl_list;
    MMIOINFO mmioinfo;
    unsigned long cur_fcc;
    unsigned long ck_size;
    char strbuff[0x104];
    int i;
    unsigned long total_read;
    unsigned long keylo;
    unsigned long keyhi;
    KeyMap *pkm;
    KeyMapWave *pkmw;
    int kmw_size = 0;
    unsigned long km_count = 0;
    bool old_amb = false;
    unsigned int fname_size;
    long new_offset;
    unsigned long nkms;
    unsigned long release;
    DllTrackSet *pts;
    CMidiTrack *ptrack;
    unsigned int j;

    if (m_flags.GT_req_pending)
        sound_win.clean_message_queue();
    if (CMidiFile::is_loaded()) {
        SNDERR retval = unload();
        if (retval)
            return retval;
    }

    m_hfile = 0;
    strcpy(m_fname, fname);
    m_hfile = mmioOpen(m_fname, 0, 0);
    if (!m_hfile)
        return SNDERR_BAD_FILE_OPEN;
    gbl_mmio++;

    while (1) {
        mmioRead(m_hfile, (HPSTR)&cur_fcc, 4);
        if (cur_fcc == MThd) {
            new_offset = mmioSeek(m_hfile, -4, SEEK_CUR);
            if (new_offset == -1)
                return SNDERR_BAD_FILE_OPEN;
            break;
        }
        switch (cur_fcc) {
        case chnl:
            mmioRead(m_hfile, (HPSTR)&ck_size, 4);
            mmioRead(m_hfile, (HPSTR)&cctrl, ck_size);
            set_channel_ctrl(&cctrl);
            mmioGetInfo(m_hfile, &mmioinfo, 0);
            continue;

        case kmap: {
            pkm = new KeyMap;
            mmioRead(m_hfile, (HPSTR)&ck_size, 4);
            mmioRead(m_hfile, (HPSTR)&pkm->m_flags, 4);
            mmioRead(m_hfile, (HPSTR)&pkm->m_cycle_lo, 4);
            mmioRead(m_hfile, (HPSTR)&pkm->m_cycle_hi, 4);
            mmioRead(m_hfile, strbuff, 1);
            total_read = 0xd;
            i = 0;
            while (strbuff[i++]) {
                mmioRead(m_hfile, &strbuff[i], 1);
                total_read++;
            }
            pkm->set_name(strbuff);
            mmioRead(m_hfile, (HPSTR)&nkms, 4);
            if (pkm->m_flags.version) {
                mmioRead(m_hfile, (HPSTR)&kmw_size, 4);
                total_read += 4;
            }
            km_count = 0;
            pkmw = new KeyMapWave;
            while (km_count < nkms && total_read < ck_size) {
                km_count++;
                if (!pkm->m_flags.version) {
                    mmioRead(m_hfile, (HPSTR)&keylo, 4);
                    mmioRead(m_hfile, (HPSTR)&keyhi, 4);
                    total_read += 8;
                    pkmw->set_keyhi(keyhi);
                    pkmw->set_keylo(keylo);
                } else {
                    mmioRead(m_hfile, (HPSTR)pkmw, kmw_size);
                    total_read += kmw_size;
                }
                mmioRead(m_hfile, strbuff, 1);
                total_read++;
                i = 0;
                while (strbuff[i++]) {
                    mmioRead(m_hfile, &strbuff[i], 1);
                    total_read++;
                }
                if (is_local_load()) {
                    char temp_path[256];
                    if (m_fname) {
                        strcpy(temp_path, m_fname);
                        char *ptemp = strrchr(temp_path, '\\');
                        if (ptemp)
                            ptemp[1] = 0;
                        else
                            temp_path[0] = 0;
                    } else {
                        temp_path[0] = 0;
                    }
                    char *pfname = strrchr(strbuff, '\\');
                    if (pfname && strcmp(temp_path, "") != 0)
                        strcat(temp_path, pfname);
                    else
                        strcat(temp_path, strbuff);
                    strcpy(strbuff, temp_path);
                }
                pkmw->set_fname(strbuff);
                if (!isKeyMapLoaded(pkm->get_name()))
                    pkm->map_range(pkmw);
            }
            ::operator delete(pkmw);
            if (ck_size - total_read >= 4) {
                mmioRead(m_hfile, (HPSTR)&release, 4);
                pkm->m_release_time = release;
            }
            add_keymap(pkm);
            delete pkm;
            cleanOutKeymapList();
            continue;
        }

        case prgm: {
            total_read = 0;
            old_amb = false;
            mmioRead(m_hfile, (HPSTR)&ck_size, 4);
            if (ck_size == 0x1c) {
                old_amb = true;
                mmioRead(m_hfile, (HPSTR)&fname_size, 4);
                mmioRead(m_hfile, (HPSTR)&pctrl, 0x18);
                pctrl.set_pitch_map_state(false);
                pctrl.set_note_off(0);
            } else {
                mmioGetInfo(m_hfile, &mmioinfo, 0);
                mmioRead(m_hfile, (HPSTR)&pctrl, 0x1c);
            }
            if (old_amb) {
                mmioRead(m_hfile, strbuff, fname_size);
                total_read = fname_size + 0x18;
                strbuff[fname_size] = 0;
            } else {
                mmioRead(m_hfile, strbuff, 1);
                i = 0;
                while (strbuff[i++])
                    mmioRead(m_hfile, &strbuff[i], 1);
                total_read = i + 0x1c;
            }
            if (is_local_load()) {
                char temp_path[256];
                if (m_fname) {
                    strcpy(temp_path, m_fname);
                    char *ptemp = strrchr(temp_path, '\\');
                    if (ptemp)
                        ptemp[1] = 0;
                    else
                        temp_path[0] = 0;
                } else {
                    temp_path[0] = 0;
                }
                char *pfname = strrchr(strbuff, '\\');
                if (pfname && strcmp(temp_path, "") != 0)
                    strcat(temp_path, pfname);
                else
                    strcat(temp_path, strbuff);
                strcpy(strbuff, temp_path);
            }
            pctrl.set_fname(strbuff);
            if (total_read < ck_size) {
                mmioRead(m_hfile, strbuff, 1);
                i = 0;
                while (strbuff[i++])
                    mmioRead(m_hfile, &strbuff[i], 1);
                pctrl.set_keymap(strbuff);
            }
            map_patch(&pctrl, old_amb);
            set_program_ctrl(&pctrl);
            pctrl.set_fname(0);
            pctrl.set_keymap(0);
            memset(&pctrl, 0, sizeof pctrl);
            continue;
        }
        case glbl: {
            mmioRead(m_hfile, (HPSTR)&ck_size, 4);
            mmioRead(m_hfile, (HPSTR)&glblset_size, 4);
            ck_size -= 4;
            mmioRead(m_hfile, (HPSTR)&glblset, glblset_size);
            ck_size -= glblset_size;
            CMidiMgr::set_play_type(glblset.m_flags.all);
            pnew_trackset = new DllTrackSet;
            while (glblset.m_ntracksets-- != 0) {
                mmioRead(m_hfile, (HPSTR)pnew_trackset, glblset.m_trackset_size);
                ck_size -= glblset.m_trackset_size;
                add_active_trackset(pnew_trackset);
            }
            ::operator delete(pnew_trackset);
            pnew_trackset = 0;
            continue;
        }

        case trak: {
            ptrack_ctrl = new TrackCtrl;
            mmioRead(m_hfile, (HPSTR)&ck_size, 4);
            mmioRead(m_hfile, (HPSTR)ptrack_ctrl, ck_size);
            track_ctrl_list.add_to_end(ptrack_ctrl);
            continue;
        }

        case ctrl:
            read_control();
            continue;
        }
        mmioClose(m_hfile, 0);
        gbl_mmio--;
        return SNDERR_BAD_FILE_OPEN;
    }

    if (CMidiMgr::load(m_hfile, fname) != 0) {
        mmioClose(m_hfile, 0);
        gbl_mmio--;
        m_hfile = 0;
        memset(m_fname, 0, sizeof m_fname);
        return SNDERR_BAD_FORMAT;
    }
    mmioClose(m_hfile, 0);
    gbl_mmio--;
    m_hfile = 0;

    while ((ptrack_ctrl = track_ctrl_list.remove_head()) != 0) {
        set_track_ctrl(ptrack_ctrl);
        delete ptrack_ctrl;
    }
    create_control_trees();

    pts = m_trackset_list.get_first();
    ptrack = 0;
    while (pts) {
        for (j = pts->m_tracklo; j <= pts->m_trackhi; j++)
            if (get_track(&ptrack, j) == 0)
                ptrack->m_ptrackset = pts;
        pts = m_trackset_list.get_next();
    }
    return SNDERR_OK;
}

void CAmbienceMgr::read_control()
{
    MidiCtrlDiskRead mcdr;
    NoteMap *pnotemap;
    MidiCtrlTarget *pmidi_target;
    MidiCtrlSocket *pmidi_socket;
    MidiCtrl *pctrl;
    int total_bytes;
    int size;
    int mcdr_size;

    mmioRead(m_hfile, (HPSTR)&total_bytes, 4);
    mmioRead(m_hfile, (HPSTR)&size, 4);
    mcdr_size = sizeof(MidiCtrlDiskRead);
    mmioRead(m_hfile, (HPSTR)mcdr.disk_read(), size);
    switch (mcdr.type()) {
    case MIDI_NOTEMAP:
        pnotemap = new NoteMap;
        pctrl = pnotemap;
        pmidi_target = pnotemap;
        pmidi_socket = pmidi_target;
        mmioRead(m_hfile, (HPSTR)&size, 4);
        mmioRead(m_hfile, (HPSTR)pmidi_target->disk_read(), size);
        mmioRead(m_hfile, (HPSTR)&size, 4);
        mmioRead(m_hfile, (HPSTR)pmidi_socket->disk_read(), size);
        mmioRead(m_hfile, (HPSTR)&size, 4);
        mmioRead(m_hfile, (HPSTR)pnotemap->disk_read(), size);
        *pctrl = mcdr;
        break;
    default:
        return;
    }
    m_control_load_list.add_to_end(pctrl);
}

#include "SoundObjectBase.inl"

inline MidiCtrlDiskRead::~MidiCtrlDiskRead()
{
}
inline MidiCtrlDiskRead::MidiCtrlDiskRead()
{
}

void CAmbienceMgr::create_control_trees()
{
    MidiCtrlTarget *pcur_target;
    for (MidiCtrl *pcur_control = m_control_load_list.get_first(); pcur_control != 0;
         pcur_control = m_control_load_list.get_next()) {
        switch (pcur_control->m_target_type) {
        case 0:
            pcur_target = (MidiCtrlTarget *)get_control_target(0, pcur_control->m_target);
            break;
        case 3: {
            MidiCtrl *pprev_control = m_control_load_list.get_prev();
            m_control_load_list.get_next();
            if (pprev_control->can_be_target() == 0)
                continue;
            if (pprev_control->m_type == MIDI_NOTEMAP)
                pcur_target = static_cast<NoteMap *>(pprev_control);
            if (pcur_target == 0)
                continue;
            break;
        }
        default:
            continue;
        }
        pcur_target->attach_ctrl(pcur_control);
    }
}

void CAmbienceMgr::strip_path(char *temp_path)
{
    char *ptemp = 0;
    if (!m_fname) {
        *temp_path = 0;
    } else {
        strcpy(temp_path, m_fname);
        ptemp = strrchr(temp_path, '\\');
        if (ptemp) {
            ptemp++;
            *ptemp = 0;
        } else {
            *temp_path = 0;
        }
    }
}

void CAmbienceMgr::make_path_local(char *buff, char *file_name)
{
    char temp_path[256];
    char *pfname;

    strip_path(temp_path);

    pfname = strrchr(file_name, '\\');
    if (pfname && strcmp(temp_path, "") != 0)
        strcat(temp_path, pfname);
    else
        strcat(temp_path, file_name);
    strcpy(buff, temp_path);
}
