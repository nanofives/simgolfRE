#include "jgldefs.h"
#include "GraphsyLoader.h"

typedef GraphSys *(*GetGraphsyObjectPtr)();

HMODULE g_graphsyModule = 0;

void FreeGraphsyDll()
{
    if (graphsys)
        graphsys->cleanup_system();
    if (g_graphsyModule) {
        FreeLibrary(g_graphsyModule);
        g_graphsyModule = 0;
    }
    graphsys = 0;
}

GraphSys *LoadGraphsyDll(const char *dllName)
{
    if (!dllName)
        return 0;
    g_graphsyModule = LoadLibraryA(dllName);
    if (!g_graphsyModule)
        return 0;
    GetGraphsyObjectPtr get = (GetGraphsyObjectPtr)GetProcAddress(g_graphsyModule, "get_graphsy_object_ptr");
    if (!get) {
        FreeGraphsyDll();
        return 0;
    }
    GraphSys *shell = get();
    if (!shell)
        FreeGraphsyDll();
    graphsys = shell;
    return shell;
}
