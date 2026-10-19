// Registry lookup over the modules of port/src/sim/modules.inc (used by the C ABI layer).
#ifndef PORTSIM_REGISTRY_LIST_H
#define PORTSIM_REGISTRY_LIST_H

#include "portsim/registry.h"

namespace portsim {
int registry_count();
const FuncDef *registry_at(int i, const char **module);
int registry_find(const char *name);  // index or -1
}  // namespace portsim

#endif  // PORTSIM_REGISTRY_LIST_H
