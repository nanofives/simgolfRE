// The Jackal network layer as specification (see portsim/sim/net.h). Roadmap S9.3: written from the C3/C4 notes
// (re/analysis/net/<addr>_*.md), the hook sources as reading reference and NET_MODEL.md; never called from tick.
#include "portsim/sim/net.h"

#include <initializer_list>
#include <map>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"

namespace portsim {
namespace net {

uint32_t RecordingNetHost::call(Call c, std::initializer_list<uint32_t> args, const std::vector<uint8_t> *payload) {
  Logged l;
  l.c = c;
  l.args.assign(args.begin(), args.end());
  if (payload) l.payload = *payload;
  log.push_back(l);
  return script ? script(c, log.back().args, user) : 0;
}

namespace {
RecordingNetHost g_default;
NetHost *g_host = &g_default;

// Negative controls: mode 0 is the correct body (one per function, see test_net.py).
int g_nc_name_of = 0, g_nc_begin0 = 0, g_nc_begin1 = 0, g_nc_flush = 0, g_nc_pump = 0, g_nc_send_list = 0;
int g_nc_queue = 0, g_nc_svc_ctl = 0, g_nc_handle = 0, g_nc_poll = 0, g_nc_shutdown = 0, g_nc_list_remove = 0;
int g_nc_add_voice = 0;
// Defect switch (model_defects: netHandleControl never clears slot 15 after the move down): 1 = the original.
int g_defect_slot15 = 1;

constexpr uint32_t kPeers = 0x170, kPeerSize = 0x58, kPeerCount = 16;

struct S {
  const Memory &m;
  uint32_t s;
  uint32_t &w(uint32_t off) const { return m.ref<uint32_t>(s + off); }
  uint32_t peer(uint32_t k) const { return s + kPeers + kPeerSize * k; }
  uint32_t &pid(uint32_t k) const { return m.ref<uint32_t>(peer(k)); }
};
uint32_t &iface(Ctx &c) { return c.gs().g_netInterface; }
Call stream(uint32_t off) { return static_cast<Call>(off); }

void copy_name(const Memory &m, uint32_t dst, uint32_t src) {
  uint32_t n = 0;
  while (m.load<uint8_t>(src + n)) ++n;
  for (uint32_t i = 0; i <= n; ++i) m.store<uint8_t>(dst + i, m.load<uint8_t>(src + i));
}
}  // namespace

void set_host(Ctx &, NetHost *host) { g_host = host ? host : &g_default; }
RecordingNetHost &default_host() { return g_default; }

#include "net_basic.inc"
#include "net_queue.inc"
#include "net_poll.inc"
#include "net_control.inc"
#include "net_handle.inc"
#include "net_dialog.inc"
#include "net_testhost.inc"

}  // namespace net

// kModule_net (the FuncDef table and its registry entry) is defined in net_registry.inc.
#include "net_registry.inc"

}  // namespace portsim
