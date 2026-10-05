// Terrain.dll functions, batch 2 (debug build): spline helpers, image byte swap, type tables, GL shutdown.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>

float powf2(float b, float e);                   // 0x10005840 (terrain_small1.cpp)

// n! (1 for 0).
// MATCH: Terrain.dll 0x100058a0 ?factorial@@YAHH@Z
int factorial(int n)
{
    int r;
    if (n == 0)
        return 1;
    r = n;
    for (int i = n; i > 2; i--)
        r *= i - 1;
    return r;
}

class Terrain {
public:
    float bernstein(float t, int i);
    void closeSystem();
    void reloadTextures();                       // 0x100380a0 (terrain_textures.cpp)
    int idClass(int id);
    int flagCode(int a, int b, int c);
    HGLRC m_rc;                                  // +0x00
};

// Degree-2 Bernstein basis value B(i, 2)(t), for the Bezier spline code.
// MATCH: Terrain.dll 0x10005750 ?bernstein@Terrain@@QAEMMH@Z
float Terrain::bernstein(float t, int i)
{
    float c;
    int n2 = factorial(2);
    float f2 = (float)n2;
    int d = factorial(i) * factorial(2 - i);
    c = f2 / d;
    float a = powf2(t, (float)i) * c;
    int e = 2 - i;
    return powf2(1.0f - t, (float)e) * a;
}

// MATCH: Terrain.dll 0x10009ba0 ?closeSystem@Terrain@@QAEXXZ
void Terrain::closeSystem()
{
    if (!m_rc)
        return;
    if (!wglMakeCurrent(NULL, NULL))
        MessageBox(NULL, "Release of RC failed!", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
    if (!wglDeleteContext(m_rc))
        MessageBox(NULL, "Deletion of RC failed!", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
    m_rc = NULL;
    reloadTextures();
}

struct ImageInfo { int bpp; int width; int height; };
// Swaps the first and third byte of every 3-byte pixel (BGR <-> RGB).
// MATCH: Terrain.dll 0x10001b60 ?swapRedBlue@@YAXPAEPAUImageInfo@@@Z
void swapRedBlue(unsigned char* p, ImageInfo* info)
{
    unsigned char* q;
    int n = 0;
    unsigned char t;
    for (n = info->width * info->height, q = p; n > 0; n--, q += 3) {
        t = q[0];
        q[0] = q[2];
        q[2] = t;
    }
}

class Tile {
public:
    char isOpenType();
    char  m_pad0[0x24];
    int   m_type;                                // +0x24
};
// Tile types 0, 1, 2, 3, 7, 8, 9.
// MATCH: Terrain.dll 0x100155b0 ?isOpenType@Tile@@QAEDXZ
char Tile::isOpenType()
{
    int r;
    if (!(m_type == 2 || m_type == 7 || m_type == 1 || m_type == 0 || m_type == 9 || m_type == 8 || m_type == 3))
        r = 0;
    else
        r = 1;
    return r;
}

// Class 0..8 of an object/texture id.
// MATCH: Terrain.dll 0x10013dd0 ?idClass@Terrain@@QAEHH@Z
int Terrain::idClass(int id)
{
    switch (id) {
    case 0: case 1: case 2: case 10: case 0xc: case 0x14: case 0x15:
        return 0;
    case 3: case 0x2b:
        return 2;
    case 4: case 0xe: case 0x18: case 0x22:
        return 4;
    case 5: case 7: case 0x19:
        return 3;
    case 6: case 0x10:
        return 1;
    case 0x28: case 0x29: case 0x2a: case 0x3d:
        return 8;
    case 0x32: case 0x34: case 0x46:
        return 7;
    case 0x3c:
        return 5;
    case 0x1e:
        return 6;
    default:
        return 0;
    }
}

// Code of three 0/1/2 flags: a in {4, 40}, b in {1, 10}, c in {2, 20}.
// MATCH: Terrain.dll 0x10013f00 ?flagCode@Terrain@@QAEHHHH@Z
int Terrain::flagCode(int a, int b, int c)
{
    int r = 0;
    if (a == 1)
        r += 4;
    else if (a == 2)
        r += 0x28;
    if (b == 1)
        r += 1;
    else if (b == 2)
        r += 10;
    if (c == 1)
        r += 2;
    else if (c == 2)
        r += 0x14;
    return r;
}
