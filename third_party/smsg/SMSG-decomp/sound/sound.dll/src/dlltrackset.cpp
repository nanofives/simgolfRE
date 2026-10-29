#include "MidiSequence.h"

DllTrackSet::DllTrackSet()
{
    m_loop_ctrl = 0;
    m_loop_count = 0;
    m_pitch_offset = 0;
    m_pdone_callback = 0;
    m_ntracks_playing = 0;
}

unsigned long DllTrackSet::get_ntracks()
{
    return m_trackhi - m_tracklo + 1;
}
