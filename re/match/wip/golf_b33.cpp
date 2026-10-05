// golf_clean.exe 0x00491c70 (release). WIP 32%, not in the 100% suite: the original keeps len in edx and reuses
// esi for the mirror bit then the first table value; ours keeps len in edi. Tried: masking a in place, the
// len < 0xffffff middle case order (from the original's compare order).
// FLAGS golf_clean.exe: /O2

extern int g_sineTab[];                  // 0x83b9f4 (quarter-wave table, 0xffff = 1)
#define SINE(a, i, mirror) ((mirror) \
    ? ((g_sineTab[(i) + 1] - g_sineTab[i]) * (int)((a) & 0x3fffff) >> 22) - g_sineTab[i] + 0xffff \
    : ((g_sineTab[(i) + 1] - g_sineTab[i]) * (int)((a) & 0x3fffff) >> 22) + g_sineTab[i])
// len * sin-like(a) for a 32-bit angle: bit 31 negates, bit 30 mirrors the quarter, bits 22..29 index the table,
// the low 22 bits interpolate; the product is scaled in three ways to stay in 32 bits.
// MATCH: golf_clean.exe 0x00491c70 ?scaleBySine@@YAHIH@Z
int scaleBySine(unsigned int a, int len)
{
    if (a & 0x80000000)
        len = -len;
    unsigned int mirror = a & 0x40000000;
    a &= 0x3fffffff;
    int i = (int)a >> 22;
    if (len < 0xffff)
        return SINE(a, i, mirror) * len >> 16;
    if (len < 0xffffff)
        return SINE(a, i, mirror) * (len >> 8) >> 8;
    return SINE(a, i, mirror) * (len >> 16);
}
