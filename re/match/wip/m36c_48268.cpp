// m36c WIP (round 36): best 42.9% for sound.dll 0x10048268, from scratch crt1.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O1
// LANG c
#include <windows.h>

extern void *pf_cfltcvt, *pf_cropzeros, *pf_fassign, *pf_forcdecpt, *pf_positive, *pf_cfltcvt2;
void __cdecl x_cfltcvt(void);
void __cdecl x_cropzeros(void);
void __cdecl x_fassign(void);
void __cdecl x_forcdecpt(void);
void __cdecl x_positive(void);

/* 0x10042506 */
// MATCH: sound.dll 0x10042506 _cfltcvt_init
void __cdecl _cfltcvt_init(void)
{
    pf_cropzeros = (void *)x_cropzeros;
    pf_cfltcvt = (void *)x_cfltcvt;
    pf_fassign = (void *)x_fassign;
    pf_forcdecpt = (void *)x_forcdecpt;
    pf_positive = (void *)x_positive;
    pf_cfltcvt2 = (void *)x_cfltcvt;
}

/* 0x10048268 */
// MATCH: sound.dll 0x10048268 _expfield
unsigned int __cdecl _expfield(unsigned int lo, unsigned int hi)
{
    if ((hi & 0x7ff00000) != 0x7ff00000)
        return hi & 0x7ff00000;
    return hi;
}

/* 0x10049604 */
// MATCH: sound.dll 0x10049604 _ValidateWrite
int __cdecl _ValidateWrite(void *p, unsigned int n)
{
    int ret = 1;
    if (IsBadWritePtr(p, n))
        ret = 0;
    return ret;
}

#pragma optimize("y", off)
extern LPCRITICAL_SECTION x_locktable[];
/* 0x10047771 */
// MATCH: sound.dll 0x10047771 _x_unlock
void __cdecl _x_unlock(int locknum)
{
    LeaveCriticalSection(x_locktable[locknum]);
}
#pragma optimize("", on)
