// Step S4.8 (log/roadmap/S4/8_notes.md): sound.dll's DirectSound wrappers (module audio_ds) and its devices, Ctrl objects,
// command queues, sound timer clients, sound thread and window and the GetProcAddress exports (module audio_dev).
//
// Both modules are written against portsim/sim/audio.h. Since S4.2b the locals the original passes by address (S4.8's
// SndLocals) are audio.h's SndFrame on the core's stack, and the caller's x87 control word (S4.8's snd_fpcw) is the core's
// x87 unit's word (x87::unit(ctx).cw). What is left here:
#ifndef PORTSIM_SIM_AUDIO_DEV_H
#define PORTSIM_SIM_AUDIO_DEV_H

#include <cstdint>

namespace portsim {

// The DirectSound / COM result -> sound.dll error code map that mapDsError (rva 0xf480) and the inline copies in
// DsBuffer::setPrimaryFormat and DsBuffer::create compute (see audio_ds.cpp, mapDsError).
uint32_t snd_ds_error(uint32_t hr);

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_DEV_H
