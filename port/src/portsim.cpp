// portsim library (phase 2.2 skeleton). The simulation core arrives in phase 3; for now the library only exposes the
// GameState size so the build, the include paths and the C ABI layer have something to link.
#include "gamestate.h"

extern "C" unsigned portsim_gamestate_size(void) { return static_cast<unsigned>(sizeof(portsim::GameState)); }
