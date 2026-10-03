// golf_clean.exe 0x004889f0 / 0x00490cf0 (release). WIP 80%: the original stores the handle first
// (mov [0x83b60c], eax) then a and b; VC6 here stores a first. Tried: struct of the three globals,
// `!= 0` test, early return. Not in the 100% suite.
// FLAGS golf_clean.exe: /O2
struct Handle4 { int m0; int m4; };
extern void* g_83b60c; extern int g_83b610, g_83b614;
extern void* g_83b9c4; extern int g_83b9c8, g_83b9cc;

// MATCH: golf_clean.exe 0x004889f0 ?select83b60c@@YAHPAUHandle4@@HH@Z
int select83b60c(Handle4* h, int a, int b)
{
    if (!h)
        return 3;
    if (h->m4) {
        g_83b60c = h;
        g_83b610 = a;
        g_83b614 = b;
    }
    return 0;
}

// MATCH: golf_clean.exe 0x00490cf0 ?select83b9c4@@YAHPAUHandle4@@HH@Z
int select83b9c4(Handle4* h, int a, int b)
{
    if (!h)
        return 3;
    if (h->m4) {
        g_83b9c4 = h;
        g_83b9c8 = a;
        g_83b9cc = b;
    }
    return 0;
}

