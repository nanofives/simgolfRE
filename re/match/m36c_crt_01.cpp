// FLAGS sound.dll: /O1
// LANG c
// m36c (round 36): sound.dll static CRT functions written by hand from the disassembly. The static CRT in sound.dll
// is /O1 (`push 1; pop esi`, `push dword ptr [esp+4]`). Globals and callees are placeholders named by address: the
// matcher masks addresses, so which CRT name each one has is not determined here.
#include <windows.h>

extern void *g_100651b8, *g_100651bc, *g_100651c0, *g_100651c4, *g_100651c8, *g_100651cc;
void __cdecl f_1004477c(void);
void __cdecl f_10044426(void);
void __cdecl f_1004448c(void);
void __cdecl f_100443cc(void);
void __cdecl f_10044474(void);

/* 0x10042506: fills six function-pointer slots (0x1004477c goes into two of them) */
// MATCH: sound.dll 0x10042506 _m36c_fill_651b8
void __cdecl m36c_fill_651b8(void)
{
    g_100651bc = (void *)f_10044426;
    g_100651b8 = (void *)f_1004477c;
    g_100651c0 = (void *)f_1004448c;
    g_100651c4 = (void *)f_100443cc;
    g_100651c8 = (void *)f_10044474;
    g_100651cc = (void *)f_1004477c;
}

/* 0x10049604: 1 unless KERNEL32!IsBadWritePtr(p, n) is nonzero (FID: _ValidateRead / _ValidateWrite, not determined) */
// MATCH: sound.dll 0x10049604 _m36c_validate_49604
int __cdecl m36c_validate_49604(void *p, unsigned int n)
{
    int ret = 1;
    if (IsBadWritePtr(p, n))
        ret = 0;
    return ret;
}

/* 0x10047771: LeaveCriticalSection on entry n of the table at 0x10067400; this one keeps its frame pointer */
#pragma optimize("y", off)
extern LPCRITICAL_SECTION g_10067400[];
// MATCH: sound.dll 0x10047771 _m36c_leave_67400
void __cdecl m36c_leave_67400(int n)
{
    LeaveCriticalSection(g_10067400[n]);
}
#pragma optimize("", on)

/* 0x10047474: resize of a block in a small-block-heap page (paragraph map of 0xf0 entries per 0x1000-byte page, an
   8-byte per-page record at region+0x18; shrinking stores 0xf1 in the record's second word). Layout written from the
   offsets the code uses: page +0 start pointer, +4 count, +8 map; region +0x10 first page, +0x18 records. */
typedef unsigned char map_t;
typedef struct { int free_paras_in_page; int last_failed_alloc; } regmap_t;
typedef struct { map_t *pstarting_alloc_map; unsigned free_paras_at_start; map_t alloc_map[0xf0 + 1]; map_t sentinel;
                 map_t reserved[6]; unsigned char blocks[0xf0][16]; } page_t;
typedef struct region { struct region *next, *prev; regmap_t *pstart; regmap_t *puncommitted;
                        page_t *pages_begin; page_t *pages_end; regmap_t region_map[33]; } region_t;

// MATCH: sound.dll 0x10047474 _m36c_sbh_resize_47474
int __cdecl m36c_sbh_resize_47474(region_t *preg, page_t *ppage, map_t *pmap, unsigned new_para_req)
{
    map_t *pmap2;
    map_t *pmap3;
    regmap_t *pregmap;
    unsigned old_para_sz;
    unsigned free_para_count;
    int retval = 0;

    pregmap = &(preg->region_map[ppage - preg->pages_begin]);

    old_para_sz = *pmap;
    if (old_para_sz > new_para_req) {
        *pmap = (map_t)new_para_req;
        pregmap->free_paras_in_page += (int)(old_para_sz - new_para_req);
        pregmap->last_failed_alloc = 0xf1;
        retval++;
    }
    else if (old_para_sz < new_para_req) {
        /* compare operand orders as the binary has them (cmp limit, end / cmp pmap, start / cmp end, start) */
        if ((ppage->alloc_map + 0xf0) >= (pmap + new_para_req)) {
            for (pmap2 = pmap + old_para_sz, pmap3 = pmap + new_para_req;
                 (pmap2 < pmap3) && (*pmap2 == 0);
                 pmap2++) ;
            if (pmap2 == pmap3) {
                *pmap = (map_t)new_para_req;
                if ((pmap <= ppage->pstarting_alloc_map) && (pmap3 > ppage->pstarting_alloc_map)) {
                    if (pmap3 < ppage->alloc_map + 0xf0) {
                        ppage->pstarting_alloc_map = pmap3;
                        for (free_para_count = 0; *pmap3 == 0; free_para_count++, pmap3++) ;
                        ppage->free_paras_at_start = free_para_count;
                    }
                    else {
                        ppage->pstarting_alloc_map = &(ppage->alloc_map[0]);
                        ppage->free_paras_at_start = 0;
                    }
                }
                pregmap->free_paras_in_page += (int)(old_para_sz - new_para_req);
                retval++;
            }
        }
    }
    return retval;
}

/* 0x10045bc8: heap selection: 1 on an NT platform with major version >= 5; else the value after ',' in an
   environment variable (strings at 0x1005ef10 / 0x1005eef8, quoted as Ghidra decodes them; the matcher masks both),
   if it is 1, 2 or 3 (tested in the order 2, 3, 1); else 3 when byte 0 of 0x10045b9b's output is >= 6, else 2.
   Callees by address: 0x1004a9d0 (3 args), 0x10042690 strstr, 0x10045680 strchr, 0x1004a795 (3 args), 0x10045b9b. */
typedef union { DWORD dw; struct { BYTE bverMajor; BYTE bverMinor; WORD wReserved; } bver; } LinkerVersion;
int __cdecl f_1004a9d0(const char *, const char *, unsigned);
char * __cdecl f_10042690(const char *, const char *);
char * __cdecl f_10045680(const char *, int);
long __cdecl f_1004a795(const char *, char **, int);
void __cdecl f_10045b9b(LinkerVersion *);

// MATCH: sound.dll 0x10045bc8 _m36c_heap_select_45bc8
int __cdecl m36c_heap_select_45bc8(void)
{
    char *cp, *env_heap_type;
    int heap_choice;
    LinkerVersion lv;
    OSVERSIONINFOA osvi;
    char lpszExeName[260];
    char env_heap_select_string[0x1090];

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    if ( GetVersionExA(&osvi) ) {
        if ( (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osvi.dwMajorVersion >= 5) )
            return 1;
    }

    if ( GetEnvironmentVariableA("__MSVCRT_HEAP_SELECT", env_heap_select_string, sizeof(env_heap_select_string)) )
    {
        for ( cp = env_heap_select_string ; *cp ; cp++ )
            if ('a' <= *cp && *cp <= 'z')
                *cp += 'A' - 'a';
        if ( !f_1004a9d0("__GLOBAL_HEAP_SELECTED", env_heap_select_string, sizeof("__GLOBAL_HEAP_SELECTED") - 1) )
            env_heap_type = env_heap_select_string;
        else
        {
            GetModuleFileNameA(NULL, lpszExeName, sizeof(lpszExeName));
            for ( cp = lpszExeName ; *cp ; cp++ )
                if ('a' <= *cp && *cp <= 'z')
                    *cp += 'A' - 'a';
            env_heap_type = f_10042690(env_heap_select_string, lpszExeName);
        }
        if ( env_heap_type )
        {
            if ( env_heap_type = f_10045680(env_heap_type, ',') )
            {
                for ( cp = ++env_heap_type ; *cp ; )
                    if ( *cp == ';' )
                        *cp = 0;
                    else
                        cp++;

                heap_choice = (int)f_1004a795(env_heap_type, NULL, 10);
                if ( (heap_choice == 2) || (heap_choice == 3) || (heap_choice == 1) )
                    return heap_choice;
            }
        }
    }

    f_10045b9b(&lv);
    if ( lv.bver.bverMajor >= 6 )
        return 3;
    return 2;
}

/* 0x10051319: a transition date (record {year, yearday, ms} at 0x10068900 for kind 1, else at 0x10068910) from the
   day-in-month or day-of-year form; month tables 0x1006891c / 0x10068950 picked by (year & 3) == 0; 0x10068874 is
   added in seconds to the second record. The binary tests `monthdow > dayofweek` for the `week * 7` arm and stores
   the yearday before the year in the first record. */
typedef struct { int yr; int yd; long ms; } transitiondate;
extern transitiondate g_10068900, g_10068910;
extern int g_1006891c[], g_10068950[];
extern long g_10068874;

// MATCH: sound.dll 0x10051319 _m36c_transition_51319
void __cdecl m36c_transition_51319(int trantype, int datetype, int year, int month, int week, int dayofweek,
                                   int date, int hour, int min, int sec, int msec)
{
    int yearday;
    int monthdow;

    if ( datetype == 1 ) {
        yearday = 1 + (((year & 3) == 0) ? g_1006891c[month - 1] : g_10068950[month - 1]);
        monthdow = (yearday + ((year - 70) * 365) + ((year - 1) >> 2) - 17 + 4) % 7;
        if ( monthdow <= dayofweek )
            yearday += (dayofweek - monthdow) + (week - 1) * 7;
        else
            yearday += (dayofweek - monthdow) + week * 7;
        if ( (week == 5) &&
             (yearday > (((year & 3) == 0) ? g_1006891c[month] : g_10068950[month])) )
        {
            yearday -= 7;
        }
    }
    else {
        yearday = ((year & 3) == 0) ? g_1006891c[month - 1] : g_10068950[month - 1];
        yearday += date;
    }

    if ( trantype == 1 ) {
        g_10068900.yd = yearday;
        g_10068900.yr = year;
        g_10068900.ms = (long)msec + (1000L * (sec + 60L * (min + 60L * hour)));
    }
    else {
        g_10068910.yd = yearday;
        g_10068910.ms = (long)msec + (1000L * (sec + 60L * (min + 60L * hour)));
        if ( (g_10068910.ms += (g_10068874 * 1000L)) < 0 ) {
            g_10068910.ms += 24 * 60 * 60 * 1000L;
            g_10068910.yd--;
        }
        else if ( g_10068910.ms >= 24 * 60 * 60 * 1000L ) {
            g_10068910.ms -= 24 * 60 * 60 * 1000L;
            g_10068910.yd++;
        }
        g_10068910.yr = year;
    }
}
