// Helpers shared by the phase 3.5 module files (events*.cpp). Not part of the public API.
#ifndef PORTSIM_SIM_EVENTS_INTERNAL_H
#define PORTSIM_SIM_EVENTS_INTERNAL_H

#include <cstdint>
#include <initializer_list>

#include "portsim/registry.h"
#include "portsim/registry_list.h"

namespace portsim {
namespace ev {

uint32_t text_va(Ctx &ctx);
// the CRT strstr over the address space (0 when absent)
uint32_t vstrstr(const Memory &m, uint32_t hay, uint32_t needle);
// calls another module's port function by its hooks.csv name (MissingResource{name} when it is not registered)
Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args);

}  // namespace ev
}  // namespace portsim

#endif
