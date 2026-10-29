// S10.12: sound.dll 0x10048268, the static CRT's exponent-field helper (names row sound_8 doubleExponentField).
// FLAGS sound.dll: /O1
// LANG c
// Original (bytes read by match.py --show): mov eax,[esp+8]; and eax,0x7ff00000; cmp eax,0x7ff00000; je L; ret;
// L: mov eax,[esp+8]; ret. The m36c version (wip/m36c_48268.cpp, 42.9%) tests `!=` and loads the mask into ecx.
// MATCH: sound.dll 0x10048268 _expfield
unsigned int __cdecl _expfield(unsigned int lo, unsigned int hi)
{
    unsigned int x = hi & 0x7ff00000;
    if (x == 0x7ff00000)
        return hi;
    return x;
}
