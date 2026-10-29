#ifndef HEAPMESSAGE_H
#define HEAPMESSAGE_H

#include "JackalUtil.h"

extern "C" char msg[0x1000];

inline void ClearHeapMessage()
{
    ClearString(msg);
}

inline void AppendHeapMessage(const char *s)
{
    AppendString(msg, s);
}

inline void AppendHeapMessageInt(int n)
{
    AppendInt(msg, n);
}

inline void AppendHeapMessageComma()
{
    AppendComma(msg);
}

inline void AppendHeapMessageSpace()
{
    AppendSpace(msg);
}

#endif
