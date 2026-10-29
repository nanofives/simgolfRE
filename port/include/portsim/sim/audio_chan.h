// The sound.dll channel and voice family (step S4.6, log/roadmap/S4/6_notes.md): module audio_chan, 101 functions registered under
// their hooks.csv names (Channel_*, Voice_*, Ch5*, SubChannel, ChannelTable, ChanRecord, VoiceSlot, the voice / channel
// constructors, destructors, source openers and the x87 rate and fade functions). It has no API of its own: the functions are
// reached through the audio core's dispatcher (portsim/sim/audio.h: snd_call by rva, snd_vcall by slot) like any sound.dll code.
//
// x87: every x87 instruction of these bodies is an operation on the audio core's x87 unit (audio.h "the x87 unit", since S4.2b)
// at the unit's control word, so Channel_setRate, Channel_recomputeVoiceGains, Channel_setReverseRate, Channel_stepVolumeFade and
// Voice_resetDefaults, which depend on the word their caller left (the C3 keys run them at 0x027f and 0x0f7f), give the original's
// bits on x64 and Win32 alike, and every compare leaves the condition codes the original's fcom leaves.
#ifndef PORTSIM_SIM_AUDIO_CHAN_H
#define PORTSIM_SIM_AUDIO_CHAN_H

namespace portsim {

// The functions of the module whose result depends on the caller's x87 control word.
inline constexpr const char *kAudioChanX87Functions[] = {"Channel_setRate", "Channel_recomputeVoiceGains", "Channel_setReverseRate",
                                                         "Channel_stepVolumeFade", "Voice_resetDefaults"};

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_CHAN_H
