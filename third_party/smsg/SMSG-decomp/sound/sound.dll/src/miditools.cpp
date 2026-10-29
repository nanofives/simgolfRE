#include "MidiSequence.h"
#include "dlist.h"
#include "random.h"
#include "miditools.h"

NoteMap Dorian(D0, D0, E0, E0, F0, G0, G0, A0, A0, B0, B0, C1, DORIAN);
NoteMap Phrygian(E0, E0, F0, F0, G0, A0, A0, B0, B0, C1, C1, D1, PHRYGIAN);
NoteMap Lydian(F0, F0, G0, G0, A0, B0, B0, C1, C1, D1, D1, E1, LYDIAN);
NoteMap Mixolydian(G0, G0, A0, A0, B0, C1, C1, D1, D1, E1, E1, F1, MIXOLYDIAN);
NoteMap Aeolian(A0, A0, B0, B0, C1, D1, D1, E1, E1, F1, F1, G1, AEOLIAN);

int MidiCtrl::m_id_enumerator = 0;

MidiCtrlSocket::MidiCtrlSocket()
{
    set_size();
    m_read_write = 0x44444444;
}

MidiCtrlSocket::~MidiCtrlSocket()
{
    delete_all_children();
}

SNDERR MidiCtrlSocket::attach_ctrl(MidiCtrl *pctrl)
{
    if (!is_ctrl_ok(pctrl))
        return SNDERR_WRONG_TYPE;
    m_ctrl_list.add_to_end((MidiCtrl *)pctrl);
    return SNDERR_OK;
}

SNDERR MidiCtrlSocket::insert_after(MidiCtrl *pctrl_insert_after, MidiCtrl *pctrl)
{
    MidiCtrl *pcur_ctrl;
    if (pctrl_insert_after == 0)
        m_ctrl_list.add_to_head(pctrl);
    else {
        pcur_ctrl = m_ctrl_list.seek(pctrl_insert_after);
        if (pcur_ctrl == 0)
            return SNDERR_BAD_PARAM;
        m_ctrl_list.insert(pctrl);
    }

    return SNDERR_OK;
}

bool MidiCtrlSocket::is_ctrl_ok(MidiCtrl *psrcctrl)
{
    return 1;
}

SNDERR MidiCtrlSocket::remove(MidiCtrl *pctrl)
{
    if (m_ctrl_list.remove(pctrl) == 0)
        return SNDERR_NOT_LOADED;
    return SNDERR_OK;
}

void MidiCtrlSocket::delete_all_children()
{
    MidiCtrl *pcur_ctrl;
    while ((pcur_ctrl = m_ctrl_list.remove_head()) != 0)
        delete pcur_ctrl;
}

MidiCtrl *MidiCtrlSocket::get_ctrl()
{
    return m_ctrl_list.get_first();
}

void MidiCtrlSocket::reset_all_controls()
{
    for (MidiCtrl *pcur_ctrl = m_ctrl_list.get_first(); pcur_ctrl != 0; pcur_ctrl = m_ctrl_list.get_next())
        pcur_ctrl->reset();
    set_size();
}

void MidiCtrlSocket::update_all_controls()
{
    for (MidiCtrl *pcur_ctrl = m_ctrl_list.get_first(); pcur_ctrl != 0; pcur_ctrl = m_ctrl_list.get_next())
        pcur_ctrl->update();
}

MidiCtrl::MidiCtrl()
{
    m_id = -1;
    m_target_type = -1;
    m_target = -1;
    m_fcc = 0x6c727463;
    memset(&m_flags, 0, sizeof(m_flags));
    set_size();
    m_read_write = 0x44444444;
}

MidiCtrlTarget::MidiCtrlTarget()
{
    m_pctrl = 0;
    set_size();
    m_read_write = 0x44444444;
}

int MidiCtrlTarget::start_control(int input)
{
    int result;
    if (get_list()) {
        MidiCtrl *pnext = get_list()->get_first();
        while (pnext) {
            switch (pnext->m_type) {
            case MIDI_NOTEMAP:
                input = pnext->do_control(input);
                break;
            case MIDI_RANDOMIZER:
                result = pnext->do_control(input);
                if (result == 1)
                    return input;
                break;
            case MIDI_STEPPER:
                input = pnext->do_control(input);
                break;
            }
            pnext = get_list()->get_next();
        }
    }
    return input;
}

NoteMap::NoteMap()
{
    for (int i = 0; i < 128; i++)
        m_map[i] = i;
    set_type(MIDI_NOTEMAP);
    m_flags.can_be_target = 1;
    m_tonal_center = 0;
    set_total_class_size();
    m_read_write = 0x44444444;
    set_size();
}

NoteMap::NoteMap(MIDI_NOTE n0, MIDI_NOTE n1, MIDI_NOTE n2, MIDI_NOTE n3, MIDI_NOTE n4, MIDI_NOTE n5, MIDI_NOTE n6,
                 MIDI_NOTE n7, MIDI_NOTE n8, MIDI_NOTE n9, MIDI_NOTE n10, MIDI_NOTE n11, NOTEMAP_TYPE sub_type)
{
    m_map[0] = n0;
    m_map[1] = n1;
    m_map[2] = n2;
    m_map[3] = n3;
    m_map[4] = n4;
    m_map[5] = n5;
    m_map[6] = n6;
    m_map[7] = n7;
    m_map[8] = n8;
    m_map[9] = n9;
    m_map[10] = n10;
    m_map[11] = n11;
    dup_octave(0);
    set_type(MIDI_NOTEMAP);
    m_tonal_center = 0;
    set_total_class_size();
    set_sub_type(sub_type);
    m_read_write = 0x44444444;
    set_size();
}

NoteMap &NoteMap::operator=(NoteMap &nm)
{
    memcpy(m_map, nm.m_map, 128 * sizeof(unsigned));
    return nm;
}

SNDERR NoteMap::dup_octave(unsigned octave)
{
    int octave_offset = octave;
    if (octave > 9)
        return SNDERR_BAD_PARAM;
    const int start_note = octave * 12;
    const int end_note = start_note + 11;
    unsigned cur_octave = 0;
    int j, i;
    for (i = 0, j = 0; i < 128; i++) {
        if (i == start_note) {
            i = end_note + 1;
            octave_offset--;
            cur_octave++;
        }
        m_map[i] = (unsigned char)(m_map[start_note + j] - (unsigned char)((char)octave_offset * 12)) & 0x7f;
        if (j == 11) {
            j = 0;
            octave_offset--;
        } else {
            j++;
        }
    }
    return SNDERR_OK;
}

SNDERR NoteMap::map_note(MIDI_NOTE dst, MIDI_NOTE src)
{
    if (dst < 128 && src < 128) {
        m_map[dst] = (unsigned char)src;
        return SNDERR_OK;
    }
    return SNDERR_BAD_PARAM;
}

void NoteMap::reset()
{
    reset_all_controls();
}

void NoteMap::update()
{
    update_all_controls();
}

int NoteMap::do_control(int input)
{
    return start_control(input);
}

int NoteMap::start_control(int input)
{
    if (get_list()) {
        MidiCtrl *pnext = get_list()->get_first();
        while (pnext) {
            switch (pnext->m_type) {
            case MIDI_NOTEMAP:
                input = pnext->do_control(input);
                break;
            case MIDI_RANDOMIZER:
                if (!pnext->m_flags.state)
                    return input;
                break;
            case MIDI_STEPPER:
                input = pnext->do_control(input);
                break;
            }
            pnext = get_list()->get_next();
        }
    }
    return m_map[input];
}

void NoteMap::construct(int *const pscale)
{
    unsigned int j = 0;
    for (unsigned i = m_tonal_center; i < m_tonal_center + 12; i++, j++)
        m_map[i % 12] = (unsigned char)((m_tonal_center + pscale[j]) % 12);
    dup_octave(0);
}

Stepper::Stepper()
{
    m_add = 1;
    m_cur_add = 0;
    set_type(MIDI_STEPPER);
    m_read_write = 0x44444444;
}

int Stepper::do_control(int input)
{
    return m_cur_add + input;
}

void Stepper::update()
{
    m_cur_add += m_add;
}

Randomizer::Randomizer()
{
    m_on_off_table = 0;
    set_range(4);
    set_type(MIDI_RANDOMIZER);
    set_total_class_size();
    set_size();
    m_read_write = 0x44444444;
}

Randomizer::~Randomizer()
{
}

void Randomizer::set_range(unsigned range)
{
    m_range = range;
    if (m_on_off_table != 0) {
        delete[] m_on_off_table;
        m_on_off_table = 0;
    }
    m_on_off_table = new unsigned char[range];
    reset();
    m_cur_index = 0;
}

int Randomizer::do_control(int input)
{
    if (m_cur_index >= m_range)
        reset();
    m_flags.state = m_on_off_table[m_cur_index];
    m_cur_index = m_cur_index + 1;
    return m_flags.state;
}

void Randomizer::update()
{
    if (m_cur_index >= m_range)
        reset();
    m_flags.state = m_on_off_table[m_cur_index++];
}

void Randomizer::reset()
{
    memset(m_on_off_table, 0, m_range);
    unsigned true_index = m_random.get((unsigned short)m_range);
    m_on_off_table[true_index] = 1;
    m_cur_index = 0;
}

Chooser::Chooser()
{
    set_type(MIDI_CHOOSER);
    set_total_class_size();
    set_size();
    set_total_class_size();
    m_read_write = 0x44444444;
}

MidiCtrl *Chooser::do_control()
{
    return 0;
}

ChannelCtrl::ChannelCtrl()
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_channel_num = 0;
    m_patch_hi = 0;
    m_patch_lo = 0;
    set_type(CHANNEL_CONTROL);
    set_total_class_size();
}

ChannelCtrl::~ChannelCtrl()
{
}

TrackCtrl::TrackCtrl()
{
    m_flags.all = 0;
    m_velocity_offset = 0;
    set_type(TRACK_CONTROL);
    set_total_class_size();
}

int MidiCtrlTarget::m_id_enumerator = -1;

ProgramCtrl::ProgramCtrl()
{
    memset(&m_flags, 0, sizeof(m_flags));
    m_patch_num = 0;
    m_pitch_hi = 0;
    m_pitch_lo = 0;
    m_vol_hi = 0;
    m_vol_lo = 0;
    m_fname = 0;
    m_szpath = 0;
    m_keymap = 0;
    m_release_time = 250;
}

ProgramCtrl::~ProgramCtrl()
{
    if (m_fname)
        ::operator delete(m_fname);
    if (m_keymap)
        ::operator delete(m_keymap);
}

void ProgramCtrl::set_base_filepath(const char *szpath)
{
    int i = 0;
    char buff[256];

    if (m_szpath != 0)
        delete[] m_szpath;
    m_szpath = 0;

    int plen = strlen(szpath);
    strcpy(buff, szpath);
    if (buff[plen - 1] != '\\') {
        buff[plen] = '\\';
        buff[plen + 1] = 0;
        plen++;
    }
    plen++;
    m_szpath = new char[plen];
    strcpy(m_szpath, buff);

    if (m_fname != 0) {
        i = strlen(m_fname);
        char *pcurptr = m_fname + i;
        while (pcurptr != m_fname) {
            pcurptr--;
            if (*pcurptr == '\\') {
                ++pcurptr;
                break;
            }
        }
        strcat(buff, pcurptr);
        set_fname(buff);
    }
}

void ProgramCtrl::set_fname(const char *fname)
{
    int i = 0;
    char buff[256];

    if (m_fname != 0)
        delete[] m_fname;
    m_fname = 0;
    if (fname == 0) {
        m_fname = 0;
    } else {
        if (m_szpath != 0) {
            const char *mark;
            strcpy(buff, m_szpath);
            mark = strrchr(fname, '\\');
            if (mark == 0)
                mark = fname;
            else
                mark = mark + 1;
            strcat(buff, mark);
        } else {
            strcpy(buff, fname);
        }
        m_fname = new char[strlen(buff) + 1];
        strcpy(m_fname, buff);
    }
}

int ProgramCtrl::get_new_pitch()
{
    if (!m_flags.randomize_pitch)
        return 0;
    int interval;
    if (m_pitch_hi > m_pitch_lo)
        interval = m_pitch_hi - m_pitch_lo + 1;
    else
        interval = m_pitch_lo - m_pitch_hi + 1;
    int pitch_offset = timeGetTime() % interval;
    int new_pitch = m_pitch_lo + pitch_offset;
    return new_pitch;
}

void ProgramCtrl::set_keymap(const char *keymap)
{
    if (m_keymap != 0)
        ::operator delete(m_keymap);
    if (keymap != 0) {
        m_keymap = (char *)::operator new(strlen(keymap) + 1);
        strcpy(m_keymap, keymap);
    } else {
        m_keymap = 0;
    }
}
