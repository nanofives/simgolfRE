#ifndef LINKLISTENTRY_H
#define LINKLISTENTRY_H

#include "HeapNew.h"

struct LinkListEntry : virtual HeapNew {
    int key;
    void *elem;
    LinkListEntry *next;
    LinkListEntry *prev;
};

#endif
