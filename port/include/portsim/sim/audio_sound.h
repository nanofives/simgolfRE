// The sound.dll sound-object family (step S4.7, log/roadmap/S4/7_notes.md): module audio_sound, 66 functions registered under their
// hooks.csv names (Sound, Sound5b6a8, Sound5c450, MultiSound, SoundGroup, SoundWindowObj, SoundTimer, the sound registry,
// create_sound, Engine_playSound). It has no API of its own: the functions are reached through the audio core's dispatcher
// (portsim/sim/audio.h: snd_call by rva, snd_vcall by slot) like any sound.dll code.
//
// x87: Sound5b6a8_ctor seeds its tempo fields with x87 arithmetic that depends on the control word of its caller (the C3 key runs
// it at 0x027f). On Win32 its body runs the original's x87 instructions under the control word of the call; on x64 it computes with
// doubles and equals the original at 0x027f only (port/CORE.md section 7). Its FuncDef.fpcw is 0x027f.
#ifndef PORTSIM_SIM_AUDIO_SOUND_H
#define PORTSIM_SIM_AUDIO_SOUND_H

namespace portsim {

// The functions of the module whose result depends on the caller's x87 control word.
inline constexpr const char *kAudioSoundX87Functions[] = {"Sound5b6a8_ctor"};

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_SOUND_H
