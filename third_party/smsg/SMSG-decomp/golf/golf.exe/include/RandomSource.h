#ifndef RANDOMSOURCE_H
#define RANDOMSOURCE_H

class RandomSource {
public:
    RandomSource() : m_seed(0) {}

    unsigned int m_seed;

    double get();

    unsigned short NextInt(int n);
};

extern RandomSource g_rng;

#endif
