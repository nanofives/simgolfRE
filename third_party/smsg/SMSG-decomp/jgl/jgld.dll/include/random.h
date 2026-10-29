#ifndef JGLPRNG_H
#define JGLPRNG_H
class PRNG {
public:
    PRNG()
    {
        m_state = 0;
    }
    void Seed(unsigned int s)
    {
        m_state = s;
    }
    unsigned short get(int limit);
    double         get();
    unsigned int   m_state;
};
extern "C" PRNG internal_random;
#endif
