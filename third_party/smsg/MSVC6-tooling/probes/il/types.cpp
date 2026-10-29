


enum Colour { RED, GREEN };

void types(void)
{
    char c = 1;
    unsigned char uc = 2;
    short s = 3;
    unsigned short us = 4;
    long l = 5;
    unsigned u = 6;
    float f = 7;
    double d = 8;
    long double ld = 9;
    Colour e = GREEN;
    volatile int v = 10;
    const int k = 11;
    int *p = 0;
    bool b = true;
    v = c + uc + s + us + l + u + (int)f + (int)d + (int)ld + e + k + (p != 0) + b;
}















