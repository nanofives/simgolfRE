// sound.dll's mixers and channel renderers (step S4.4, log/roadmap/S4/4_notes.md): module audio_mixer, 38 functions registered
// under their hooks.csv names and reached through the audio core's dispatcher like any sound.dll code.
//
// The virtual x87 unit these bodies compute on (control word, TOP, C0..C3, tags, eight 80-bit registers, per context) was
// S4.4's and is the audio core's since S4.2b: portsim/sim/audio.h, namespace portsim::x87 ("the x87 unit"). So is sound.dll's
// __ftol (snd_crt::ftol_, registered "audio_core.crt.ftol", called with snd_ftol) and the stack the renderers keep their
// by-address locals on (SndFrame, "the stack").
#ifndef PORTSIM_SIM_AUDIO_MIXER_H
#define PORTSIM_SIM_AUDIO_MIXER_H

#include <cstdint>

#include "portsim/sim/audio.h"

namespace portsim {

// sound.dll's __ftol (rva 0x42550): S4.4's name for snd_crt::ftol_.
constexpr uint32_t kSndFtolRva = snd_crt::ftol_;

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_MIXER_H
