// AudioSink (step S4.2): what sound.dll's DSOUND / WINMM / ole32 / KERNEL32 imports become in the port.
//
// The original reaches the system through its IAT (snd_import in audio.h) and through COM interfaces the system hands back
// (IDirectSound, IDirectSoundBuffer, IDirectSound3DListener ...: their methods are called through vtables, snd_vcall). In
// the replay a host serves both (the fixture's stand-ins). With no host, they arrive here as COMMANDS:
//
//   import(ctx, cmd)   an IAT call: cmd.dll / cmd.name come from the image's import directory (the user's sound.dll), the
//                      argument slots as the original pushed them; the return value is what the original receives in eax.
//   com(ctx, cmd)      a method of an interface object the sink created with audio_sink_object(): cmd.iface is the
//                      sink's own interface id, cmd.slot the vtable byte offset / 4, args[0] the object.
//
// A sink creates interface objects inside the context's address space with audio_sink_object(ctx, iface, nslots): the
// object's vtable holds addresses of a reserved range the dispatcher routes back to com(). Pointers the original passes
// (buffers, WAVEFORMATEX, out-parameters) are virtual addresses: a sink reads and writes them through ctx.mem.
//
// RecordingSink logs every command as one text line (the S4.10 command stream) and answers with a configurable value.
#ifndef PORTSIM_SIM_AUDIO_SINK_H
#define PORTSIM_SIM_AUDIO_SINK_H

#include <cstdint>
#include <string>
#include <vector>

#include "portsim/registry.h"

namespace portsim {

struct AudioCommand {
  std::string dll;   // import commands: the DLL as the import directory names it (e.g. "DSOUND.dll"); COM: empty
  std::string name;  // import commands: the import name, or "#<ordinal>"; COM: empty
  uint32_t iface = 0;  // COM: the sink's interface id
  uint32_t slot = 0;   // COM: vtable slot index
  std::vector<uint32_t> args;  // the original's argument slots (COM: args[0] = the object)
  bool want_st0 = false;
};

struct AudioReply {
  uint32_t eax = 0, edx = 0;
  double st0 = 0.0;
  bool has_st0 = false;
};

class AudioSink {
 public:
  virtual ~AudioSink() = default;
  virtual AudioReply import(Ctx &ctx, const AudioCommand &cmd) = 0;
  virtual AudioReply com(Ctx &ctx, const AudioCommand &cmd) = 0;
};

// An interface object of `nslots` methods in the context's address space; its vtable entries route to sink->com() with
// the given interface id. Returns the object's virtual address (the dword at it is the vtable address). Objects live in
// a block the audio core maps for the context (freed with audio_detach).
uint32_t audio_sink_object(Ctx &ctx, uint32_t iface, uint32_t nslots);
// P3: returns an object audio_sink_object made; the next object of the same interface and slot count reuses it (a sink
// whose objects are released and created again over a session then needs no more of the block than it holds at once)
void audio_sink_object_free(Ctx &ctx, uint32_t obj, uint32_t iface, uint32_t nslots);

// One text line per command, e.g. "import DSOUND.dll!DirectSoundCreate 0x0 0x30001000 0x0" / "com 2.12 0x...";
// every reply is `answer` (0 = S_OK / MMSYSERR_NOERROR).
class RecordingSink : public AudioSink {
 public:
  uint32_t answer = 0;
  std::vector<std::string> lines;
  AudioReply import(Ctx &ctx, const AudioCommand &cmd) override;
  AudioReply com(Ctx &ctx, const AudioCommand &cmd) override;
};

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_SINK_H
