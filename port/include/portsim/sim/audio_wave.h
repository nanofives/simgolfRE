// The sound.dll wave family (step S4.9, log/roadmap/S4/9_notes.md): module audio_wave, 100 of the 101 rows of S4.9 registered under their
// hooks.csv names: Wave_Device and Wave_In_Device (the DLL's exported output / capture device objects), WaveStream / StreamReader, the
// WaveInDevice capture object over the winmm waveIn imports, the mmio RIFF readers (parseRiffWave, openMmioSound, loadWaveResource,
// fillDecodeRing, flushMmioBuffer ...), the MidiReader stream readers and the wave-file writers. (loadSoundFile 0x41d0 is the open row.)
//
// It has no API of its own: the functions are reached through the audio core's dispatcher (portsim/sim/audio.h: snd_call by rva,
// snd_vcall by slot, snd_import by IAT slot) like any sound.dll code. Every mmio call goes through snd_import, so with no host
// attached the AudioSink receives it as a `WINMM!mmioXxx` command (portsim/sim/audio_sink.h) carrying the original's argument slots;
// the pointers in those slots (MMIOINFO, MMCKINFO, read / write buffers) are addresses of the context's 32-bit space. No function
// of this module depends on the x87 control word of its caller (the only floating-point work, computeCaptureDuration and the
// 16-bit sample clamps, runs at the words the C3 fixtures used and matches there).
#ifndef PORTSIM_SIM_AUDIO_WAVE_H
#define PORTSIM_SIM_AUDIO_WAVE_H

namespace portsim {}  // no declarations: see the header comment

#endif  // PORTSIM_SIM_AUDIO_WAVE_H
