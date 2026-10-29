#include "jgldefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Log.h"
#include "EngineInit.h"

FILE *env_open(const char *name, const char *mode)
{
    char *path = filefind_get(name);
    if (path == 0)
        path = (char *)name;
    return fopen(path, mode);
}

void close_cursor_class()
{
}
