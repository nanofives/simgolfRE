#ifndef DEGREES_H
#define DEGREES_H

const float DEGREES_TO_RADIANS = 0.017453292f;

inline float DegreesToRadians(float degrees)
{
    return degrees * DEGREES_TO_RADIANS;
}

#endif
