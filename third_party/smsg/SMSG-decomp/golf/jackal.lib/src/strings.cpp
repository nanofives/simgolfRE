#include "jgldefs.h"
#include "Strings.h"
#include "crt.h"

Strings strings;

Strings::Strings()
{
    m_open = 0;
}

int Strings::init(unsigned int bytes)
{
    if (m_open)
        shutdown();
    if (Heap::init(bytes))
        return 1;
    put("-Nil-");
    m_open = 1;
    return 0;
}

int Strings::shutdown()
{
    Heap::shutdown();
    m_open = 0;
    return 0;
}

char *Strings::put(const char *s)
{
    char *p = (char *)Alloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

char *Strings::get(char *s)
{
    if (s <= (char *)m_block)
        return (char *)m_block;
    if (s > (char *)m_block + m_capacity)
        return 0;
    return s;
}
