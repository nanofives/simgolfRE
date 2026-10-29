#ifndef AUTOSOUNDCTOR_H
#define AUTOSOUNDCTOR_H

#include <math.h>
#include <iostream.h>
#include "WidgetBase.h"

inline int WidgetObserversPad(int a, int b)
{
    int c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
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
    if (c > b)
        c -= b;
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
    c += b;
    return c + b;
}

inline AutoSound::AutoSound(int unrecovered)
{
    ResetToDefaults();
    m_onShown = 0;
    m_onHidden = 0;
    m_onMouseMove = 0;
    m_onLeftClick = 0;
    m_onLeftDown = 0;
    m_onLeftUp = 0;
    m_onRightClick = 0;
    m_onRightDown = 0;
    m_onRightUp = 0;
    m_onCharKey = 0;
    m_onKeyDown = 0;
    m_onKeyUp = 0;
    m_onLeftDoubleClick = 0;
    m_onRightDoubleClick = 0;
    m_onHoverChanged = 0;
    m_onMouseMoveAlt = 0;
}

#endif
