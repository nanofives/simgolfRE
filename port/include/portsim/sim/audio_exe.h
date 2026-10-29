// The exe's sound layer (roadmap step S4.10; module audio_exe, port/src/sim/audio_exe.cpp; write-up
// re/analysis/data/audio_stream.md): golf_clean.exe's side of the sound system on the port's sound.dll engine. The Snd
// objects of g_sounds (300 records of 0x6c bytes) and their four class tables, the sound requests the game makes (playSound,
// startSoundSlot, Snd::status), the start-up and shut-down chain (soundVersionCheck, initSound, loadSoundTable,
// shutdownSoundSystem), the device holders g_sfxDevice / g_musicDevice / g_voiceDevice (resourceLoad, openHook*, closeHook*,
// releaseWithHook*, the group table of g_musicDevice) and VoiceRx.
//
// ADDRESSES. The exe is not relocatable: its image base is the GameState's base minus .data's rva (exe_base), so in the
// segments and the oracle (GameState at the original .data address) it is the original's. Exe code is cited by function
// name; exe code addresses are image-relative (rvas, exe_rva below); data addresses are GameState members.
//
// CALLS. Every call the original makes to code that is not inline in it goes through exe_call (an exe function by rva),
// exe_vcall (a virtual slot), exe_call_va (a pointer read from memory) or exe_import (an IAT slot):
//   * an exe address a replay host claims (a fixture's Interceptor.replace stand-in) goes to the host;
//   * an exe function this module (or another) registers under its hooks.csv name runs in the port (exe_functions());
//   * an address inside sound.dll, a sink interface object or any other address goes to the audio core's dispatcher
//     (snd_call_va: sound.dll's port functions by rva, the host, or the AudioSink);
//   * an unregistered exe function goes to the replay host (counted as an exe FALLBACK, development only) or, with no host,
//     stops the call with MissingResource{name}.
// Imports: with a replay host the IAT slot's value is called (the fixture's stand-ins); without one (production) the
// loader imports are served here (LoadLibraryA maps the user's sound.dll from the game files with audio_attach,
// GetProcAddress resolves an ordinal through its export table, FreeLibrary keeps the mapping), timeGetTime is the context's
// InputSource clock at the read's site (else the AudioSink's answer), and every other import goes to the AudioSink as a
// command named by the exe's import directory.
//
// HEAP. The exe's operator new / operator delete go to a claim or the replay host when there is one (the replay's heap
// stand-ins); with none they are this module's arena in the context's address space (exe_new / exe_delete; U-1495).
//
// THE COMMAND STREAM (gate b). With an AudioSink attached (audio_set_sink) every call another module makes into this
// module is a REQUEST: before it the sink receives import(dll "golf_clean.exe", name = the hooks.csv name, the argument
// slots), preceded by a frame marker import(dll "portsim", name "frame", {frame-head clock}) whenever the frame changed
// since the previous request. The device-less path (g_soundReady 0, every Snd's +0x40 NULL: all 205 segment start images)
// makes no other command.
#ifndef PORTSIM_SIM_AUDIO_EXE_H
#define PORTSIM_SIM_AUDIO_EXE_H

#include <cstdint>
#include <initializer_list>
#include <string>

#include "portsim/registry.h"
#include "portsim/sim/audio.h"

namespace portsim {

// golf_clean.exe's layout (the anchored build; audio_exe.cpp checks the user's file against it when it maps the image)
constexpr uint32_t kExeDataRva = 0xc1000u;    // .data: the GameState
constexpr uint32_t kExeRdataEnd = 0xc1000u;   // headers, .text and .rdata end where .data starts
inline uint32_t exe_base(const Ctx &ctx) { return ctx.mem.data_base() - kExeDataRva; }

// Image-relative addresses of the exe code this module calls or dispatches to, and of its IAT slots.
namespace exe_rva {
// the CRT (static, in the exe)
constexpr uint32_t operator_new = 0xa57f1, operator_delete = 0xa4ffc, ftol = 0xa6030, eh_vector_dtor = 0xa5713;
// IAT slots
constexpr uint32_t iat_LoadLibraryA = 0xba0a8, iat_GetProcAddress = 0xba0a4, iat_FreeLibrary = 0xba0ac,
                   iat_timeGetTime = 0xba21c, iat_MessageBoxA = 0xba1f4;
// the function loadSoundTable whose instructions hold the sound table (push mode; push path; mov ecx, slot; call)
constexpr uint32_t loadSoundTable = 0x48220;
constexpr uint32_t Snd_applyMode = 0x84e30;
}  // namespace exe_rva

// The site keys of the exe's timeGetTime reads (InputSource::clock: the call's return address as an offset from mainLoop's
// frame-head return address; tick.cpp's convention): Snd::play+0x86 and Snd::busy+0x21.
constexpr int32_t kSiteSndPlayClock = 0x74d97;
constexpr int32_t kSiteSndBusyClock = 0x74f22;

// One exe function the module can dispatch to: rva and hooks.csv name.
struct ExeFn {
  uint32_t rva;
  const char *name;
};
const ExeFn *exe_functions(size_t *count);
const char *exe_function_name(uint32_t rva);  // nullptr when the module does not know the rva

// ---- calls (see above) ------------------------------------------------------------------------------------------------
Ret exe_call(Ctx &ctx, uint32_t rva, SndAbi abi, std::initializer_list<uint32_t> args);
Ret exe_call_va(Ctx &ctx, uint32_t va, SndAbi abi, const uint32_t *args, int nargs, bool want_st0 = false);
Ret exe_call_va(Ctx &ctx, uint32_t va, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0 = false);
// [[obj] + slot_off], thiscall with obj first
Ret exe_vcall(Ctx &ctx, uint32_t obj, uint32_t slot_off, std::initializer_list<uint32_t> args);
Ret exe_import(Ctx &ctx, uint32_t iat_rva, SndAbi abi, std::initializer_list<uint32_t> args, int32_t clock_site = 0);
uint32_t exe_new(Ctx &ctx, uint32_t n);
void exe_delete(Ctx &ctx, uint32_t p);

// ---- the module's per-context state ------------------------------------------------------------------------------------
// Replay mode (the oracle and the bundle replay, configuration entry "audio_exe.setup"): a host serves claims, imports and
// exe fallbacks. Without it (production, the segments) imports and the heap are served as described above.
void audio_exe_set_replay(Ctx &ctx, bool replay);
bool audio_exe_replay(Ctx &ctx);
void audio_exe_claim(Ctx &ctx, uint32_t va);
void audio_exe_reset(Ctx &ctx);  // drops the context's exe-layer state (claims, counters, stream position, replay mode)
void audio_exe_release(Ctx &ctx);  // audio_exe_reset plus the context's DeviceSink (audio_release, sg_state_free)
struct AudioExeCounters {
  uint32_t requests = 0;     // calls from other modules (stream requests)
  uint32_t dispatched = 0;   // exe functions run through exe_call / exe_vcall
  uint32_t fallbacks = 0;    // unregistered exe functions handed to the host
  uint32_t heap_allocs = 0;  // exe_new served by the module's arena
  std::string fallback_names;  // space-separated
};
const AudioExeCounters &audio_exe_counters(Ctx &ctx);

// Map the user's golf_clean.exe headers, .text and .rdata read-only at exe_base (from the context's GameFiles
// "golf_clean.exe", checked against the anchored layout) unless the address is already mapped. Called before the first
// class-table read; MissingResource when the file is absent or another build.
void exe_map_image(Ctx &ctx);

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_EXE_H
