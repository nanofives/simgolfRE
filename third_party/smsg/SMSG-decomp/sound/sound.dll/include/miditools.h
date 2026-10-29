#ifndef MIDITOOLS_H
#define MIDITOOLS_H

#include "dlist.h"
#include "random.h"
#include "sound.h"

union PCFlags_tag {
    unsigned long all;
    struct {
        int randomize_pitch : 1;
        int randomize_vol : 1;
        int static_mem : 1;
        int note_off : 1;
        int ignore_pitch : 1;
    };
};

class ProgramCtrl {
    unsigned long m_patch_num;
    PCFlags_tag m_flags;
    long m_pitch_hi, m_pitch_lo;
    unsigned long m_vol_hi, m_vol_lo;

    unsigned long m_release_time;

    char *m_fname;
    char *m_szpath;
    char *m_keymap;

public:
    ProgramCtrl();
    ~ProgramCtrl();
    void set_patch_num(unsigned long patch_num) { m_patch_num = patch_num; }
    unsigned long get_patch_num() { return m_patch_num; }
    void set_randomize_pitch(int brp) { m_flags.randomize_pitch = brp; }
    void set_pitch_hi(unsigned long pitch_hi) { m_pitch_hi = pitch_hi; }
    unsigned long get_pitch_hi() { return m_pitch_hi; }
    void set_pitch_lo(unsigned long pitch_lo) { m_pitch_lo = pitch_lo; }
    unsigned long get_pitch_lo() { return m_pitch_lo; }
    void set_randomize_vol(int brp) { m_flags.randomize_vol = brp; }
    void set_vol_hi(unsigned long vol_hi) { m_vol_hi = vol_hi; }
    unsigned long get_vol_hi() { return m_vol_hi; }
    void set_vol_lo(unsigned long vol_lo) { m_vol_lo = vol_lo; }
    unsigned long get_vol_lo() { return m_vol_lo; }
    void set_release_time(unsigned long);
    unsigned long get_release_time();
    const char *get_fname() { return m_fname; }
    void set_fname(const char *fname);
    void set_base_filepath(const char *szpath);
    int is_randomize_pitch() { return m_flags.randomize_pitch; }
    int is_randomize_vol() { return m_flags.randomize_vol; }
    int note_off() { return m_flags.note_off; }
    void set_note_off(int note_off) { m_flags.note_off = note_off; }
    int get_new_pitch();
    void set_static(int);
    int is_static();
    void set_keymap(const char *keymap);

    char *get_keymap() { return m_keymap; }
    bool is_pitch_mapped() { return !m_flags.ignore_pitch; }
    void set_pitch_map_state(bool state) { m_flags.ignore_pitch = !state; }
};

enum MIDI_CONTROL {
    PROGRAM_CONTROL,
    TRACK_CONTROL,
    CHANNEL_CONTROL,
    MIDI_NOTEMAP,
    MIDI_STEPPER,
    MIDI_RANDOMIZER,
    MIDI_CHOOSER,
    MIDI_METHOD,
    MIDI_CTRL_DISK
};

struct MidiCtrlFlags {
    unsigned char can_be_target;
    unsigned char state;
};

class MidiCtrl {
public:
    unsigned long m_fcc;
    int m_total_class_size;
    int m_size;
    int m_read_write;
    int m_id;
    MidiCtrlFlags m_flags;
    MIDI_CONTROL m_type;
    int m_target_type;
    int m_target;
    static int m_id_enumerator;
    MidiCtrl();
    virtual ~MidiCtrl() {}
    virtual void set_type(MIDI_CONTROL) = 0;
    virtual void set_total_class_size() = 0;
    virtual void refresh() { m_flags.state = 1; }
    virtual int do_control(int input) { return input; }
    virtual void reset() {}
    virtual void update() {}
    void set_size() { m_size = (char *)&m_target - (char *)&m_size; }
    int size();
    int *disk_read() { return &m_read_write; }
    unsigned long *disk_write();
    int write_size();
    MIDI_CONTROL type() { return m_type; }
    int id();
    int make_id();
    bool can_be_target() { return m_flags.can_be_target; }
};

class MidiCtrlDiskRead : public MidiCtrl {
public:
    MidiCtrlDiskRead();
    virtual ~MidiCtrlDiskRead();

private:
    virtual void set_type(MIDI_CONTROL m_type) {}
    virtual void set_total_class_size() { m_total_class_size = sizeof(MidiCtrlDiskRead); }
};
typedef char MidiCtrlDiskRead_sizeof_check[sizeof(MidiCtrlDiskRead) == 0x28 ? 1 : -1];

union CCFlags_tag {
    unsigned long all;
    struct {
        int randomize_patches : 1;
    };
};

class ChannelCtrl : public MidiCtrl {
    unsigned long m_channel_num;

    CCFlags_tag m_flags;
    unsigned long m_patch_hi;
    unsigned long m_patch_lo;

public:
    ChannelCtrl();
    virtual ~ChannelCtrl();
    void set_channel_num(unsigned long);
    unsigned long get_channel_num() { return m_channel_num; }
    void set_randomize_patches(int);
    int is_randomize_patches() { return m_flags.randomize_patches; }
    void set_patch_hi(unsigned long);
    unsigned long get_patch_hi();
    void set_patch_lo(unsigned long);
    unsigned long get_patch_lo();
    unsigned long get_flags();
    virtual void set_type(MIDI_CONTROL type) { m_type = type; }
    virtual void set_total_class_size() { m_total_class_size = sizeof(ChannelCtrl); }
};
typedef char ChannelCtrl_sizeof_check[sizeof(ChannelCtrl) == 0x38 ? 1 : -1];

union TRACKCTRLFlags_tag {
    unsigned int all;
    struct {
        unsigned char active : 1;
        unsigned char looping : 1;
    };
};

class TrackCtrl : public MidiCtrl {
public:
    unsigned int m_trk_num;
    TRACKCTRLFlags_tag m_flags;
    int m_velocity_offset;
    TrackCtrl();
    virtual void set_type(MIDI_CONTROL type) { m_type = type; }
    virtual void set_total_class_size() { m_total_class_size = sizeof(TrackCtrl); }
};
typedef char TrackCtrl_sizeof_check[sizeof(TrackCtrl) == 0x34 ? 1 : -1];

class MidiCtrlSocket {
    int m_size;

    int m_read_write;

    virtual bool is_ctrl_ok(MidiCtrl *psrcctrl);

public:
    DList<MidiCtrl> m_ctrl_list;

    MidiCtrlSocket();
    ~MidiCtrlSocket();
    MidiCtrl *get_ctrl();
    SNDERR attach_ctrl(MidiCtrl *pctrl);
    SNDERR init_socket_list(MidiCtrl *);
    void delete_all_children();
    SNDERR insert_after(MidiCtrl *pctrl_insert_after, MidiCtrl *pctrl);

    SNDERR remove(MidiCtrl *pctrl);
    DList<MidiCtrl> *get_list() { return &m_ctrl_list; }

    void reset_all_controls();
    void update_all_controls();
    void set_size() { m_size = 0x18; }
    int size();
    int *disk_read() { return &m_read_write; }
    int *disk_write();
    int write_size();
};

class MidiCtrlTarget : public MidiCtrlSocket {
    int m_size;

    int m_read_write;

    int m_id;
    MidiCtrl *m_pctrl;

public:
    static int m_id_enumerator;
    MidiCtrlTarget();
    virtual int start_control(int input);
    void set_size() { m_size = (char *)&m_pctrl - (char *)&m_size; }
    int size();
    int *disk_read() { return &m_read_write; }

    int *disk_write();
    int write_size();
    int make_id();
    int set_id(int);
    int id();
};

enum MIDI_NOTE {
    C0,
    CS0,
    D0,
    DS0,
    E0,
    F0,
    FS0,
    G0,
    GS0,
    A0,
    AS0,
    B0,
    C1,
    CS1,
    D1,
    DS1,
    E1,
    F1,
    FS1,
    G1,
    GS1,
    A1,
    AS1,
    B1,
    C2,
    CS2,
    D2,
    DS2,
    E2,
    F2,
    FS2,
    G2,
    GS2,
    A2,
    AS2,
    B2,
    C3,
    CS3,
    D3,
    DS3,
    E3,
    F3,
    FS3,
    G3,
    GS3,
    A3,
    AS3,
    B3,
    C4,
    CS4,
    D4,
    DS4,
    E4,
    F4,
    FS4,
    G4,
    GS4,
    A4,
    AS4,
    B4,
    C5,
    CS5,
    D5,
    DS5,
    E5,
    F5,
    FS5,
    G5,
    GS5,
    A5,
    AS5,
    B5,
    C6,
    CS6,
    D6,
    DS6,
    E6,
    F6,
    FS6,
    G6,
    GS6,
    A6,
    AS6,
    B6,
    C7,
    CS7,
    D7,
    DS7,
    E7,
    F7,
    FS7,
    G7,
    GS7,
    A7,
    AS7,
    B7,
    C8,
    CS8,
    D8,
    DS8,
    E8,
    F8,
    FS8,
    G8,
    GS8,
    A8,
    AS8,
    B8,
    C9,
    CS9,
    D9,
    DS9,
    E9,
    F9,
    FS9,
    G9,
    GS9,
    A9,
    AS9,
    B9,
    C10,
    CS10,
    D10,
    DS10,
    E10,
    F10,
    FS10,
    G10,
    NMIDI_NOTES
};

enum NOTEMAP_TYPE {
    DORIAN,
    PHRYGIAN,
    LYDIAN,
    MIXOLYDIAN,
    AEOLIAN,
    USER,
    NTYPES
};

class NoteMap : public MidiCtrlTarget, public MidiCtrl {
    int m_size;

    int m_read_write;

    unsigned char m_map[128];
    NOTEMAP_TYPE m_sub_type;
    unsigned m_tonal_center;

public:
    NoteMap();

    NoteMap(MIDI_NOTE n0, MIDI_NOTE n1, MIDI_NOTE n2, MIDI_NOTE n3, MIDI_NOTE n4, MIDI_NOTE n5, MIDI_NOTE n6,
            MIDI_NOTE n7, MIDI_NOTE n8, MIDI_NOTE n9, MIDI_NOTE n10, MIDI_NOTE n11, NOTEMAP_TYPE sub_type);
    SNDERR dup_octave(unsigned octave);
    SNDERR map_note(MIDI_NOTE dst, MIDI_NOTE src);
    unsigned char *get_map();
    virtual void set_type(MIDI_CONTROL type) { m_type = type; }
    void set_sub_type(NOTEMAP_TYPE type) { m_sub_type = type; }
    NOTEMAP_TYPE sub_type();
    NoteMap &operator=(NoteMap &nm);
    virtual int do_control(int input);
    virtual int start_control(int input);
    void construct(int *const pscale);

    virtual void reset();
    virtual void update();

    void set_tonal_center(unsigned int);
    virtual void set_total_class_size() { m_total_class_size = sizeof(NoteMap); }
    void set_size() { m_size = (char *)&m_tonal_center - (char *)&m_size; }
    int size();
    int *disk_read() { return &m_read_write; }

    int *disk_write();
    int write_size();
};
typedef char NoteMap_sizeof_check[sizeof(NoteMap) == 0xe4 ? 1 : -1];

class Stepper : public MidiCtrl {
    int m_size;
    int m_read_write;
    int m_add;
    int m_cur_add;

public:
    Stepper();
    void set_add(int);
    virtual int do_control(int input);
    virtual void set_type(MIDI_CONTROL type) { m_type = type; }
    virtual void set_total_class_size() { m_total_class_size = sizeof(Stepper); }
    void set_size();
    int size();
    int *disk_read();
    int *disk_write();
    int write_size();
    virtual void update();
};
typedef char Stepper_sizeof_check[sizeof(Stepper) == 0x38 ? 1 : -1];

class Randomizer : public MidiCtrl {
    Random m_random;
    unsigned char *m_on_off_table;
    unsigned m_cur_index;
    int m_size;
    int m_read_write;
    unsigned m_range;

public:
    Randomizer();
    virtual ~Randomizer();
    void set_range(unsigned range);

    virtual void set_type(MIDI_CONTROL type) { m_type = type; }
    virtual int do_control(int input);
    virtual void reset();
    virtual void update();

    virtual void set_total_class_size() { m_total_class_size = sizeof(Randomizer); }
    void set_size() { m_size = (char *)&m_range - (char *)&m_size; }

public:
    int size();
    int *disk_read();
    int *disk_write();
    int write_size();
};
typedef char Randomizer_sizeof_check[sizeof(Randomizer) == 0x40 ? 1 : -1];

struct Chooser_Flags_tag {
    bool randomize;
    bool cycle;
};

class Chooser : public MidiCtrlTarget, public MidiCtrl {
    int m_size;
    Chooser_Flags_tag m_flags;
    int m_read_write;

public:
    Chooser();
    MidiCtrl *do_control();

    virtual void set_type(MIDI_CONTROL type) { m_type = type; }
    void set_size() { m_size = (char *)&m_read_write - (char *)&m_size; }
    int size();
    int *disk_read();
    int *disk_write();
    int write_size();
    virtual void set_total_class_size() { m_total_class_size = sizeof(Chooser); }
};
typedef char Chooser_sizeof_check[sizeof(Chooser) == 0x60 ? 1 : -1];

#endif
