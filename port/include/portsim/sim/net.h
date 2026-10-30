// The Jackal network layer as SPECIFICATION (roadmap S9.3, decision G14): golf_clean.exe links the layer but never
// activates it (re/analysis/net/NET_MODEL.md section 0), so nothing in tick calls these functions. They are ported over
// the session object in the 32-bit space, with every call the original makes through a vtable slot, an import or an
// unported peer going to a NetHost at the IDirectPlay2A slot level (NET_MODEL.md section 2).
#ifndef PORTSIM_SIM_NET_H
#define PORTSIM_SIM_NET_H

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "portsim/registry.h"

namespace portsim {
namespace net {

// Every outward call. Names follow the callee: Stream* are slots of the stream source at session+0x64, Sess* slots of the
// session object's own vtable, Dp* slots of IDirectPlay2A, Obj* slots of a list payload, Imp* imports of the exe.
enum class Call : uint32_t {
  StreamStop = 0x14, StreamFlush = 0x20, StreamNext = 0x88, StreamRelease = 0x8c, StreamStart = 0x90, StreamPending = 0x94,
  SessAddPeer = 0x1008, SessSetName = 0x1010, SessOnHostLost = 0x1014, SessOnHostMoved = 0x1018, SessPeerRemoved = 0x100c,
  DpClose = 0x2010, DpRelease = 0x2008, DpGetPlayerName = 0x2054,
  NetService = 0x3000, NetSendMessages = 0x3001, ListAddVoiceRx = 0x3002, VoiceRxStart = 0x3003,
  ListSlot4 = 0x3004, ListItemDelete = 0x3005, ListNodeDelete = 0x3006, ListClear = 0x3008,
  ImpTimeGetTime = 0x4000, ImpEnterCs = 0x4001, ImpLeaveCs = 0x4002, ImpDeleteCs = 0x4003, ImpSetEvent = 0x4004,
  ImpSleep = 0x4005, ImpCloseHandle = 0x4006, ImpFree = 0x4007,
  HudText = 0x5000, DialogRun = 0x5001,
};

// One entry of the receive queue (session+0x148): `user` is queuePop's second out word (0 = a DirectPlay system message).
struct Pop {
  uint32_t user = 0;
  std::vector<uint8_t> bytes;
};

// The host answers each call; a recording host keeps (call, args, payload) and answers from a script.
class NetHost {
 public:
  virtual ~NetHost() {}
  virtual uint32_t call(Call c, std::initializer_list<uint32_t> args, const std::vector<uint8_t> *payload = nullptr) = 0;
  virtual bool queue_pop(uint32_t queue_va, Pop &out) = 0;   // false = the queue is empty (queuePop returned 0)
};

struct Logged {
  Call c;
  std::vector<uint32_t> args;
  std::vector<uint8_t> payload;
};
using Script = uint32_t (*)(Call c, const std::vector<uint32_t> &args, void *user);

class RecordingNetHost : public NetHost {
 public:
  std::vector<Logged> log;
  Script script = nullptr;
  void *user = nullptr;
  std::vector<Pop> inbox;   // queued entries, popped front first
  uint32_t call(Call c, std::initializer_list<uint32_t> args, const std::vector<uint8_t> *payload) override;
  bool queue_pop(uint32_t queue_va, Pop &out) override;
};

// The context's host (a RecordingNetHost by default); the pointer must outlive the context's calls.
RecordingNetHost &default_host();
void set_host(Ctx &ctx, NetHost *host);

}  // namespace net
}  // namespace portsim

#endif  // PORTSIM_SIM_NET_H
