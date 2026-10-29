#include "random.h"

Random internal_random;

unsigned short Random::get(unsigned short num)
{
    unsigned int temp;

    temp = randomSeed = randomSeed * 0x19660d + 0x3c6ef35f;
    temp = temp >> 16;
    temp = temp * num;
    temp = temp >> 16;
    return (unsigned short)temp;
}

float Random::get()
{
    unsigned int temp;

    randomSeed = randomSeed * 0x19660d + 0x3c6ef35f;
    temp = (randomSeed & 0x7fffff) | 0x3f800000;

    return *(float *)&temp - 1.0f;
}
