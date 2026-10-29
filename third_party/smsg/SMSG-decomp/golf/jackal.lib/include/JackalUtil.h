#ifndef JACKALUTIL_H
#define JACKALUTIL_H

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "StringUtil.h"

inline int FreeIfNotNull(void *p)
{
    if (p == 0)
        return 0;
    free(p);
    return 0;
}

inline void *AllocateBytes(unsigned int n)
{
    return malloc(n);
}

inline void *ReallocateBytes(void *p, unsigned int n)
{
    return realloc(p, n);
}

inline void ClearString(char *s)
{
    *s = 0;
}

inline void AppendString(char *dst, const char *src)
{
    strcat(dst, src);
}

inline void AppendSpace(char *s)
{
    strcat(s, " ");
}

inline void AppendComma(char *s)
{
    strcat(s, ", ");
}

inline void AppendInt(char *dst, int n)
{
    char buf[80];
    itoa(n, buf, 10);
    strcat(dst, buf);
}

inline DWORD GetMilliseconds()
{
    return timeGetTime();
}

#endif
