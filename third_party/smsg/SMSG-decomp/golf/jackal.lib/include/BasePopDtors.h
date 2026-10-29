

#ifndef BASEPOPDTORS_H
#define BASEPOPDTORS_H

#include "BasePop.h"
#include <math.h>
#include <iostream.h>
#include <fstream.h>
#include <strstrea.h>

inline unsigned BasePopPad(unsigned a, unsigned b)
{
    unsigned c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    return c + b;
}


#undef IMAGEBUTTON_DTOR_DEFERRED
#include "ImageButton.h"


inline void BasePopUnrecoveredLocal()
{
    Dialogs list;
}


Dialogs::~Dialogs()
{
    close();
}

#undef BASEPOP_DTOR_DEFERRED
#include "BasePop.h"

#endif
