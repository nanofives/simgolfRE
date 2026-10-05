// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll random generator (debug build): the ANSI C LCG (seed * 1103515245 + 12345, bits 16..30) in a class with a
// global instance at 0x1012844c; _$E2 / _$E1 are its compiler-generated initializers (no destructor, no atexit).
class Random {
public:
    Random();
    double next();
    int range(int n);
    unsigned int seed;
};
Random g_random;                 // 0x1012844c
// MATCH: jgld.dll 0x10007620 ??0Random@@QAE@XZ
Random::Random()
{
    seed = 0;
}
// MATCH: jgld.dll 0x10007530 ?next@Random@@QAENXZ
double Random::next()
{
    seed = seed * 0x41c64e6d + 0x3039;
    return ((seed >> 16) & 0x7fff) / 32768.0;
}
// MATCH: jgld.dll 0x100075b0 ?range@Random@@QAEHH@Z
int Random::range(int n)
{
    int m = n & 0xffff;
    double d = m;
    return (int)(next() * d);
}
// MATCH: jgld.dll 0x100074b0 _$E2
// MATCH: jgld.dll 0x100074f0 _$E1
