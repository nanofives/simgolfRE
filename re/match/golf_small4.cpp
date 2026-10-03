// Small golf_clean.exe functions, batch 4 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <string.h>

extern signed char g_5783a1;             // 0x5783a1 (shift)
extern int g_56c770[8], g_56c794[8], g_56a924[8];
extern int g_tick;                       // 0x834170
extern short g_568600[500];              // 0x568600
int f_491c70(int a, int b);              // 0x491c70

struct Handle4 { int m0; int m4; };      // only +4 is tested

// Sum of x/8 while x decays by x >> shift until it drops below 0x40.
// MATCH: golf_clean.exe 0x004223c0 ?decaySum@@YAHH@Z
int decaySum(int x)
{
    int r = 0;
    do {
        r += x / 8;
        x -= x >> g_5783a1;
    } while (x >= 0x40);
    return r;
}

// MATCH: golf_clean.exe 0x0040c860 ?clearMatching@@YAXHH@Z
void clearMatching(int a, int b)
{
    for (int i = 0; i < 8; i++)
        if (g_56c770[i] == a && g_56c794[i] == b)
            g_56a924[i] = 0;
}

// Stores a | b in a 500-slot ring indexed by game tick / 1024.
// MATCH: golf_clean.exe 0x0040c6f0 ?logTick@@YAXHH@Z
void logTick(int a, int b)
{
    g_568600[g_tick / 1024 % 500] = (short)(a | b);
}

// MATCH: golf_clean.exe 0x00467270 ?fold467270@@YAHHH@Z
int fold467270(int a, int b)
{
    if (a & 0x80000000) {
        b = -b;
        a &= 0x7fffffff;
    }
    if (a & 0x40000000)
        a = 0x7fffffff - a;
    return f_491c70(a, b);
}

// MATCH: golf_clean.exe 0x0044faf0 ?bucket44faf0@@YAHH@Z
int bucket44faf0(int v)
{
    if (v <= 5)
        return 0;
    if (v <= 9)
        return 1;
    if (v <= 0x11)
        return 2;
    return v <= 0x12 ? 3 : -1;
}

// Turns Caps Lock off when it is on.
// MATCH: golf_clean.exe 0x004321d0 ?capsLockOff@@YAXXZ
void capsLockOff()
{
    BYTE keys[256];
    GetKeyboardState(keys);
    if (keys[VK_CAPITAL] & 1)
        keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY, 0);
}

class ObjA { public: void f_482dd0(); char pad[0x44]; };   // array at 0x50fc20, stride 0x44
class ObjB { public: void f_481ba0(); char pad[0x84]; };   // array at 0x4f65c8, stride 0x84
extern ObjA g_objA[];
extern ObjB g_objB[];

// MATCH: golf_clean.exe 0x004041c0 ?resetPair4041c0@@YAXH@Z
void resetPair4041c0(int i)
{
    g_objA[i].f_482dd0();
    g_objB[i].f_481ba0();
}

class Text477 {
public:
    int put(const char* s);              // 0x477250
    int put(const char* s, int len);     // 0x477280
};

// MATCH: golf_clean.exe 0x00477250 ?put@Text477@@QAEHPBD@Z
int Text477::put(const char* s)
{
    if (!s)
        return 0;
    return put(s, strlen(s));
}

class Obj4837f0 { public: void f_4837f0(); };   // 0x4837f0
extern Obj4837f0 g_821020, g_821f08, g_821ec8, g_821f28, g_821ee8;

// MATCH: golf_clean.exe 0x0044ac60 ?reset44ac60@@YAXXZ
void reset44ac60()
{
    g_821020.f_4837f0();
    g_821f08.f_4837f0();
    g_821ec8.f_4837f0();
    g_821f28.f_4837f0();
    g_821ee8.f_4837f0();
}
