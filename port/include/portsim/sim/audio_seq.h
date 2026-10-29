// The sound.dll sequencer and MIDI family (step S4.5, log/roadmap/S4/5_notes.md): module audio_seq, 80 functions registered under
// their hooks.csv names (Seq_*, Midi_*, Track_*, Midi_Device and MidiDeviceBase). It has no API of its own: the functions are
// reached through the audio core's dispatcher (portsim/sim/audio.h: snd_call by rva, snd_vcall by slot) like any sound.dll code,
// so a caller in another module needs only this header's name table (kAudioSeqX87Functions) to know which function needs the
// Win32 judge.
//
// x87: Seq_setTempo depends on the x87 control word of its caller (the C3 keys run it at 0x027f and 0x0f7f). On Win32 its body
// runs the original's x87 instructions under the control word of the call; on x64 it computes with doubles and equals the
// original at 0x027f only (port/CORE.md section 7). Its FuncDef.fpcw is 0x027f.
#ifndef PORTSIM_SIM_AUDIO_SEQ_H
#define PORTSIM_SIM_AUDIO_SEQ_H

namespace portsim {

// The functions of the module whose result depends on the caller's x87 control word.
inline constexpr const char *kAudioSeqX87Functions[] = {"Seq_setTempo"};

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_SEQ_H
