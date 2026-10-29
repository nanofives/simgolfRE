#ifndef RGB555_H
#define RGB555_H

inline int PackRgb555(unsigned char r, unsigned char g, unsigned char b)
{
    return (((0xffe00000 | r >> 3) << 5 | g >> 3) << 5) | b >> 3;
}

#endif
