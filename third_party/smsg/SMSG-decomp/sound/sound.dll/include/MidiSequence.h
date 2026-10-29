#ifndef MIDISEQUENCE_H
#define MIDISEQUENCE_H

#include <windows.h>
#include <mmsystem.h>

#include "sounddevice.h"

#include "random.h"

#include "keymap.h"

union TS_FLAGS_tag {
    unsigned int all;
    struct {
        unsigned char sync_start : 1;
        unsigned char sync_running : 1;
        unsigned char sync_beat : 1;
        unsigned char sync_measure : 1;
        unsigned char no_sync : 1;
    };
};

class TrackSet {
public:
    TS_FLAGS_tag m_flags;
    TrackSet();
    unsigned m_id;
    unsigned m_tracklo;
    unsigned m_trackhi;
    unsigned int m_nswitch_threads;
    unsigned m_loops;
    unsigned int m_linked_set;
    unsigned m_tempo;
    int m_sync_track;

    void set_lo(unsigned lo);
    void set_hi(unsigned hi);

public:
    void set_sync_type(unsigned int);
};
typedef char TrackSet_sizeof_check[sizeof(TrackSet) == 0x24 ? 1 : -1];

union GMS_flags_tag {
    unsigned int all;
    struct {
        int looping : 1;
        int continuous : 1;
        int stopping : 1;
    };
};

class GlblMidiSet {
public:
    GlblMidiSet();
    unsigned m_trackset_size;
    unsigned m_ntracksets;
    GMS_flags_tag m_flags;
};

class DllTrackSet : public TrackSet {
public:
    DllTrackSet();
    unsigned long get_ntracks();
    unsigned long m_loop_ctrl;
    unsigned long m_loop_count;
    int m_pitch_offset;
    void(__cdecl *m_pdone_callback)(void *);
    unsigned int m_ntracks_playing;
};

typedef char DllTrackSet_sizeof_check[sizeof(DllTrackSet) == 0x38 ? 1 : -1];


#include "miditools.h"
#include "midifile.h"
#include "midimgr.h"

#endif
