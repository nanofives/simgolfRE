#ifndef RANDOM_H
#define RANDOM_H

class Random {
    unsigned randomSeed;

public:
    Random() { randomSeed = 0; }
    void reseed(unsigned int newSeed) { randomSeed = newSeed; }

    unsigned int getSeed();
    unsigned short get(unsigned short num);
    float get();
};

#endif
