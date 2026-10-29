#ifndef STRINGS_H
#define STRINGS_H

#include "Heap.h"

struct Strings : Heap {
    Strings();
    int init(unsigned int bytes);
    int shutdown();
    char *put(const char *s);
    char *get(char *s);

    int m_open;
};

extern Strings strings;

#endif
