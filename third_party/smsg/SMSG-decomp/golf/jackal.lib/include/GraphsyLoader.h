#ifndef GRAPHSYLOADER_H
#define GRAPHSYLOADER_H

#include "Surface.h"

extern HMODULE g_graphsyModule;

void FreeGraphsyDll();
GraphSys *LoadGraphsyDll(const char *dllName);

#endif
