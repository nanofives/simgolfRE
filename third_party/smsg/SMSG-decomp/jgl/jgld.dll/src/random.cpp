#include "random.h"
extern "C" {
PRNG internal_random;
}
double PRNG::get()
{
    m_state = m_state * 1103515245 + 12345;
    return ((m_state >> 16) & 0x7fff) / 32768.0;
}
unsigned short PRNG::get(int limit)
{
    return (unsigned short)((limit & 0xffff) * get());
}
