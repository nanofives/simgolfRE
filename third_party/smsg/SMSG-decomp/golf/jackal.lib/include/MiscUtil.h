#ifndef MISCUTIL_H
#define MISCUTIL_H
#include "jgltypes.h"
void swap(int *a, int *b);

char *memrchr(char *start, char *from, char wanted);

char *findnum(char *s);

extern const char g_digitRange[];

extern "C" void memsetword(ushort *dst, ushort val, int count);

#endif
